#include "Generation/WorldImport.h"

#include "Generation/WorldEdits.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace WorldImport
{
	static FString Root()
	{
		return FPaths::ProjectSavedDir() / TEXT("WorldForge/imported");
	}

	TArray<FEntry> List()
	{
		TArray<FEntry> Out;
		TArray<FString> Dirs;
		IFileManager::Get().FindFiles(Dirs, *(Root() / TEXT("*")), false, true);
		Dirs.Sort();
		for (const FString& D : Dirs)
		{
			const FString Folder = FPaths::ConvertRelativePathToFull(Root() / D);
			FString Text;
			TSharedPtr<FJsonObject> J;
			if (!FFileHelper::LoadFileToString(Text, *(Folder / TEXT("map.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid())
			{
				continue;
			}
			FEntry& E = Out.AddDefaulted_GetRef();
			E.Folder = Folder;
			if (!J->TryGetStringField(TEXT("title"), E.Title)) E.Title = D;
		}
		return Out;
	}

	TSharedPtr<FWorldMap> Load(const FString& Folder, TSharedPtr<FWorldEdits>& OutEdits, FString& OutNote)
	{
		FString Text;
		TSharedPtr<FJsonObject> J;
		if (!FFileHelper::LoadFileToString(Text, *(Folder / TEXT("map.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("FORGE import: sem %s/map.json"), *Folder);
			return nullptr;
		}
		TSharedPtr<FWorldMap> M = MakeShared<FWorldMap>();
		M->N = int32(J->GetNumberField(TEXT("n")));
		M->Cell = float(J->GetNumberField(TEXT("cell")));
		M->WaterLevel = float(J->GetNumberField(TEXT("water_level")));
		const int32 NN = M->N * M->N;
		TArray<uint8> HeightBytes, LayerBytes;
		if (M->N < 16 || !FFileHelper::LoadFileToArray(HeightBytes, *(Folder / TEXT("height.bin"))) || HeightBytes.Num() != NN * 4
			|| !FFileHelper::LoadFileToArray(LayerBytes, *(Folder / TEXT("layers.bin"))) || LayerBytes.Num() != NN * 4)
		{
			UE_LOG(LogTemp, Warning, TEXT("FORGE import: relevo ou camadas com tamanho errado em %s"), *Folder);
			return nullptr;
		}
		M->Height.SetNumUninitialized(NN);
		FMemory::Memcpy(M->Height.GetData(), HeightBytes.GetData(), NN * 4);
		M->Layers.SetNumUninitialized(NN);
		for (int32 I = 0; I < NN; ++I)
		{
			const uint8* B = &LayerBytes[I * 4];
			M->Layers[I] = FColor(B[0], B[1], B[2], B[3]);
		}

		// os controles: o bioma e o tamanho de lá; a semente marca o mapa (ruídos do acabamento)
		M->Settings.Biome = EWorldBiome(FMath::Clamp(int32(J->GetNumberField(TEXT("biome"))), 0, 2));
		M->Settings.MapSize = float(M->N - 1) * M->Cell;
		M->Settings.Seed = int32(FCrc::StrCrc32(*FPaths::GetCleanFilename(Folder)) & 0x7FFFFF) + 1000000;
		M->Settings.Frame = 0;
		M->Frame = EWorldFrame::Range;

		// o que o gerador faria e o importado não tem: sem erosão, leito ou bacia
		M->Flow.Init(0.0f, NN);
		M->Bed.Init(0.0f, NN);
		M->Basin.Init(0, NN);
		M->Territory.Init(-1, NN);
		// passabilidade: penhasco acima de 44° (a regra do pathfinder do Zero Hour), água abaixo da cota
		M->Pass.Init(EWorldPass::Open, NN);
		const float MaxRise = FMath::Tan(FMath::DegreesToRadians(44.0f)) * M->Cell;
		int32 Open = 0;
		for (int32 Jj = 0; Jj < M->N; ++Jj)
		{
			for (int32 Ii = 0; Ii < M->N; ++Ii)
			{
				const int32 V = M->Index(Ii, Jj);
				const float H = M->Height[V];
				float Rise = 0.0f;
				if (Ii + 1 < M->N) Rise = FMath::Max(Rise, FMath::Abs(M->Height[M->Index(Ii + 1, Jj)] - H));
				if (Jj + 1 < M->N) Rise = FMath::Max(Rise, FMath::Abs(M->Height[M->Index(Ii, Jj + 1)] - H));
				M->Pass[V] = H < M->WaterLevel - 0.5f ? EWorldPass::Water : (Rise > MaxRise ? EWorldPass::Cliff : EWorldPass::Open);
				Open += M->Pass[V] == EWorldPass::Open ? 1 : 0;
			}
		}
		M->OpenFraction = float(Open) / float(NN);
		M->bConnected = true;

		for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("starts")))
		{
			const TArray<TSharedPtr<FJsonValue>>& E = V->AsArray();
			FWorldStart& S = M->Starts.AddDefaulted_GetRef();
			S.Player = int32(E[0]->AsNumber());
			S.Pos = FVector2f(float(E[1]->AsNumber()), float(E[2]->AsNumber()));
			S.Z = M->HeightAt(S.Pos);
		}
		M->Starts.Sort([](const FWorldStart& A, const FWorldStart& B) { return A.Player < B.Player; });
		M->Settings.Players = FMath::Max(2, M->Starts.Num());
		for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("resources")))
		{
			const TArray<TSharedPtr<FJsonValue>>& E = V->AsArray();
			FWorldResource& R = M->Resources.AddDefaulted_GetRef();
			R.Pos = FVector2f(float(E[0]->AsNumber()), float(E[1]->AsNumber()));
			R.Z = M->HeightAt(R.Pos);
			R.Radius = float(E[2]->AsNumber());
			R.Amount = float(E[3]->AsNumber());
		}
		for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("props")))
		{
			const TArray<TSharedPtr<FJsonValue>>& E = V->AsArray();
			FWorldProp& P = M->Props.AddDefaulted_GetRef();
			P.Type = E[0]->AsString() == TEXT("rock") ? EWorldPropType::Rock : EWorldPropType::Tree;
			P.Pos = FVector2f(float(E[1]->AsNumber()), float(E[2]->AsNumber()));
			P.Z = M->HeightAt(P.Pos);
			P.Scale = float(E[3]->AsNumber());
			P.Yaw = float(E[4]->AsNumber());
			P.Variant = uint8(FMath::Clamp(int32(E[5]->AsNumber()), 0, 255));
		}

		OutEdits = MakeShared<FWorldEdits>();
		FString EditsText;
		if (FFileHelper::LoadFileToString(EditsText, *(Folder / TEXT("edits.json"))))
		{
			OutEdits->FromJson(EditsText);
		}

		// o que ficou de fora, em palavras
		TArray<FString> Parts;
		const TSharedPtr<FJsonObject>* Dropped;
		if (J->TryGetObjectField(TEXT("dropped"), Dropped))
		{
			int32 Total = 0;
			for (const TPair<FString, TSharedPtr<FJsonValue>>& K : (*Dropped)->Values) Total += int32(K.Value->AsNumber());
			if (Total) Parts.Add(FString::Printf(TEXT("%d peças sem suporte (prédios, torres)"), Total));
		}
		FString WaterNote;
		if (J->TryGetStringField(TEXT("water_note"), WaterNote) && !WaterNote.IsEmpty())
		{
			Parts.Add(TEXT("a água (a nossa é uma cota só)"));
		}
		OutNote = Parts.Num() ? TEXT("ficou de fora: ") + FString::Join(Parts, TEXT(", ")) : FString();
		UE_LOG(LogTemp, Display, TEXT("FORGE import %s: %d×%d, %d bases, %d campos, %d peças, %d estradas; %s"),
			*Folder, M->N, M->N, M->Starts.Num(), M->Resources.Num(), M->Props.Num(), OutEdits->Roads.Num(), *OutNote);
		return M;
	}
}
