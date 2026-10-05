#include "WorldForgeExport.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace WorldForgeExport
{
	static bool WritePng(const FString& File, const void* Data, int64 Bytes, int32 W, int32 H, ERGBFormat Format, int32 Bits)
	{
		IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Png = Module.CreateImageWrapper(EImageFormat::PNG);
		if (!Png.IsValid() || !Png->SetRaw(Data, Bytes, W, H, Format, Bits))
		{
			return false;
		}
		return FFileHelper::SaveArrayToFile(Png->GetCompressed(), *File);
	}

	static TSharedRef<FJsonObject> Vec(FVector2f P, float Z)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("x"), P.X);
		O->SetNumberField(TEXT("y"), P.Y);
		O->SetNumberField(TEXT("z"), Z);
		return O;
	}

	bool Save(const FWorldMap& M, const TArray<FColor>& Minimap, const FString& Folder)
	{
		if (M.N < 2 || !IFileManager::Get().MakeDirectory(*Folder, true))
		{
			return false;
		}
		float MinH = TNumericLimits<float>::Max(), MaxH = TNumericLimits<float>::Lowest();
		for (float H : M.Height)
		{
			MinH = FMath::Min(MinH, H);
			MaxH = FMath::Max(MaxH, H);
		}
		TArray<uint16> H16;
		H16.SetNumUninitialized(M.Height.Num());
		const float Range = FMath::Max(MaxH - MinH, 1e-3f);
		for (int32 K = 0; K < M.Height.Num(); ++K)
		{
			H16[K] = uint16(FMath::Clamp(FMath::RoundToInt((M.Height[K] - MinH) / Range * 65535.0f), 0, 65535));
		}
		bool bOk = WritePng(FPaths::Combine(Folder, TEXT("height16.png")), H16.GetData(), H16.Num() * 2, M.N, M.N, ERGBFormat::Gray, 16);
		bOk &= WritePng(FPaths::Combine(Folder, TEXT("layers.png")), M.Layers.GetData(), M.Layers.Num() * 4, M.N, M.N, ERGBFormat::BGRA, 8);
		const int32 S = FMath::RoundToInt(FMath::Sqrt(float(Minimap.Num())));
		if (S > 0)
		{
			bOk &= WritePng(FPaths::Combine(Folder, TEXT("minimap.png")), Minimap.GetData(), Minimap.Num() * 4, S, S, ERGBFormat::BGRA, 8);
		}

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> Set = MakeShared<FJsonObject>();
		const FWorldGenSettings& St = M.Settings;
		Set->SetNumberField(TEXT("seed"), St.Seed);
		Set->SetNumberField(TEXT("mapSize"), St.MapSize);
		Set->SetNumberField(TEXT("players"), St.Players);
		Set->SetNumberField(TEXT("biome"), int32(St.Biome));
		Set->SetNumberField(TEXT("relief"), St.Relief);
		Set->SetNumberField(TEXT("plateaus"), St.Plateaus);
		Set->SetNumberField(TEXT("water"), St.Water);
		Set->SetNumberField(TEXT("erosion"), St.Erosion);
		Set->SetNumberField(TEXT("forests"), St.Forests);
		Set->SetNumberField(TEXT("resources"), St.Resources);
		Root->SetObjectField(TEXT("settings"), Set);
		Root->SetNumberField(TEXT("grid"), M.N);
		Root->SetNumberField(TEXT("cellMeters"), M.Cell);
		Root->SetNumberField(TEXT("heightMinMeters"), MinH);
		Root->SetNumberField(TEXT("heightMaxMeters"), MaxH);
		Root->SetNumberField(TEXT("waterLevelMeters"), M.WaterLevel);
		Root->SetBoolField(TEXT("connected"), M.bConnected);
		Root->SetNumberField(TEXT("ramps"), M.RampsCarved);
		Root->SetNumberField(TEXT("openFraction"), M.OpenFraction);
		Root->SetNumberField(TEXT("rushMeters"), M.RushMeters);
		Root->SetNumberField(TEXT("rushStraightMeters"), M.RushStraight);
		TArray<TSharedPtr<FJsonValue>> Starts, Res, Times;
		for (const FWorldStart& S0 : M.Starts)
		{
			TSharedRef<FJsonObject> O = Vec(S0.Pos, S0.Z);
			O->SetNumberField(TEXT("player"), S0.Player);
			Starts.Add(MakeShared<FJsonValueObject>(O));
		}
		for (const FWorldResource& R : M.Resources)
		{
			TSharedRef<FJsonObject> O = Vec(R.Pos, R.Z);
			O->SetNumberField(TEXT("radius"), R.Radius);
			O->SetNumberField(TEXT("amount"), R.Amount);
			O->SetNumberField(TEXT("owner"), R.Owner);
			Res.Add(MakeShared<FJsonValueObject>(O));
		}
		for (const TPair<FString, double>& T : M.Timings)
		{
			TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetNumberField(T.Key, T.Value);
			Times.Add(MakeShared<FJsonValueObject>(O));
		}
		Root->SetArrayField(TEXT("starts"), Starts);
		Root->SetArrayField(TEXT("resources"), Res);
		Root->SetArrayField(TEXT("timingsMs"), Times);
		int32 Trees = 0, Rocks = 0;
		for (const FWorldProp& P : M.Props) (P.Type == EWorldPropType::Tree ? Trees : Rocks)++;
		Root->SetNumberField(TEXT("trees"), Trees);
		Root->SetNumberField(TEXT("rocks"), Rocks);
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Root, Writer);
		bOk &= FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Folder, TEXT("map.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		return bOk;
	}
}
