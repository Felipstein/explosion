#include "ForgeSliceGameMode.h"

#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "LookDirector.h"
#include "Materials/MaterialParameterCollection.h"
#include "LookRecipe.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "ForgeCameraPawn.h"
#include "ForgePlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "RHI.h"
#include "RHIGlobals.h"
#include "RHIStats.h"
#include "ShaderCompiler.h"
#include "UObject/UObjectIterator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "LandscapeProxy.h"
#include "UnrealClient.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	// vistas da prova: as distâncias de câmera de RTS e duas de perto
	const FForgeSliceView DefaultViews[] = {
		{ TEXT("rts_high"), FVector2f(0, 0), 0.0f, 55.0f, 170.0f },
		{ TEXT("rts_mid"), FVector2f(10, -15), 20.0f, 50.0f, 85.0f },
		{ TEXT("rts_close"), FVector2f(-20, 10), 35.0f, 45.0f, 38.0f },
		{ TEXT("ground"), FVector2f(15, 25), 110.0f, 16.0f, 22.0f },
		{ TEXT("against_sun"), FVector2f(-10, -30), 250.0f, 28.0f, 55.0f },
	};
	constexpr int32 MotionFrames = 120;   // 4 s a 30 qps
}

AForgeSliceGameMode::AForgeSliceGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AForgeCameraPawn::StaticClass();
	PlayerControllerClass = AForgePlayerController::StaticClass();
}

void AForgeSliceGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (AForgeCameraPawn* Pawn = PC->GetPawn<AForgeCameraPawn>())
		{
			Pawn->MaxDistance = 400.0f;
			Pawn->SetView(FVector2f(0, 0), 0.0f, 50.0f, 110.0f);
		}
	}
	// veio do botão "Ver em qualidade real": no máximo (Ultra = épico), seja qual
	// for o preset do gerador; o preset dele volta junto no "voltar"
	const int32 PrevQuality = UGameplayStatics::GetIntOption(OptionsString, TEXT("q"), -1);
	if (UGameplayStatics::HasOption(OptionsString, TEXT("real")) && GEngine)
	{
		GEngine->Exec(GetWorld(), TEXT("Scalability 3"));
		UE_LOG(LogTemp, Display, TEXT("FORGE real quality map: Ultra (gerador estava em %d)"), PrevQuality);
	}
	ResumeOptions = PrevQuality >= 0 ? FString::Printf(TEXT("resume?q=%d"), PrevQuality) : FString(TEXT("resume"));
	if (FParse::Value(FCommandLine::Get(), TEXT("ForgeCapture="), CaptureFolder))
	{
		IFileManager::Get().MakeDirectory(*CaptureFolder, true);
	}
	else if (GEngine && GEngine->GameViewport)
	{
		// volta pro gerador; "resume" traz os controles do último "Ver em qualidade real"
		TWeakObjectPtr<AForgeSliceGameMode> Weak(this);
		BackButton = SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.Padding(FMargin(14.0f))
			[
				SNew(SButton)
				.ButtonColorAndOpacity(FLinearColor(0.10f, 0.11f, 0.12f, 0.85f))
				.OnClicked_Lambda([Weak]()
				{
					if (Weak.IsValid())
					{
						UGameplayStatics::OpenLevel(Weak.Get(), FName(TEXT("/Game/Maps/Forge")), true, Weak->ResumeOptions);
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("WorldForge", "BackToForge", "← Voltar ao gerador     F2: teste de qualidade · F3: grupos de triângulos"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
				]
			];
		// 1ª abertura de um mapa novo: os materiais compilam e a Unreal mostra o
		// quadriculado padrão até terminar — avisa em vez de parecer quebrado
		ShaderNote = SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			.Padding(FMargin(0.0f, 18.0f))
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
				.ShadowOffset(FVector2D(1.0, 1.0))
				.Visibility_Lambda([]() { return GShaderCompilingManager && GShaderCompilingManager->IsCompiling() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				.Text_Lambda([]()
				{
					const int32 Left = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
					return FText::Format(NSLOCTEXT("WorldForge", "Compiling", "preparando os materiais (1ª vez): faltam {0}"), FText::AsNumber(Left));
				})
			];
		GEngine->GameViewport->AddViewportWidgetContent(ShaderNote.ToSharedRef(), 9);
		GEngine->GameViewport->AddViewportWidgetContent(BackButton.ToSharedRef(), 10);
	}
	// -ForgeViews=nome:x,y,yaw,pitch,dist;... troca as vistas (testes de cena);
	// -ForgeNoMotion pula a medição e o passeio
	Views.Append(DefaultViews, UE_ARRAY_COUNT(DefaultViews));
	FString Custom;
	if (FParse::Value(FCommandLine::Get(), TEXT("ForgeViews="), Custom, false))
	{
		TArray<FString> Items;
		Custom.ParseIntoArray(Items, TEXT(";"));
		TArray<FForgeSliceView> Parsed;
		for (const FString& Item : Items)
		{
			FString Name, Numbers;
			TArray<FString> N;
			if (Item.Split(TEXT(":"), &Name, &Numbers) && Numbers.ParseIntoArray(N, TEXT(",")) == 5)
			{
				Parsed.Add({ Name, FVector2f(FCString::Atof(*N[0]), FCString::Atof(*N[1])), FCString::Atof(*N[2]), FCString::Atof(*N[3]), FCString::Atof(*N[4]) });
			}
		}
		if (Parsed.Num())
		{
			Views = Parsed;
		}
	}
	bNoMotion = FParse::Param(FCommandLine::Get(), TEXT("ForgeNoMotion"));
	ApplyLook();
	SpawnVillage();
	// o A/B de um parâmetro do material do chão na mesma montagem (13/10, o AerBlur do teste do Look):
	// -ForgeLandscapeParam=Nome=valor, no Landscape inteiro
	FString LParam;
	if (FParse::Value(FCommandLine::Get(), TEXT("ForgeLandscapeParam="), LParam, false))
	{
		FString PName, PValue;
		if (LParam.Split(TEXT("="), &PName, &PValue))
		{
			for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
			{
				It->SetLandscapeMaterialScalarParameterValue(FName(*PName), FCString::Atof(*PValue));
			}
			UE_LOG(LogTemp, Display, TEXT("FORGE parâmetro do chão: %s = %s"), *PName, *PValue);
		}
	}
}

void AForgeSliceGameMode::SpawnVillage()
{
	// a vila do mapa, do mesmo village.json que o jogo lê (Saved/WorldForge/exports/<mapa>): no jogo quem
	// desenha é o jogo; aqui a "qualidade real" mostra o resultado final inteiro (o Felipe, 13/10: "gera
	// as imagens da vila na qualidade real pra gente ver com o look real do jogo"). Casas, muros e
	// miudezas com o giro livre e a laje, postes e os fios em catenária
	UWorld* World = GetWorld();
	const FString MapName = UWorld::RemovePIEPrefix(World->GetMapName());
	const FString Path = FPaths::ProjectSavedDir() / TEXT("WorldForge/exports") / MapName / TEXT("village.json");
	FString Text;
	TSharedPtr<FJsonObject> J;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid())
	{
		return;
	}
	// a etiqueta da montagem (village_hash): o JSON de outra montagem desenharia a vila no lugar errado
	const FString Hash = J->GetStringField(TEXT("hash"));
	bool bMarker = false, bMatch = false;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		for (const FName& Tag : It->Tags)
		{
			const FString S = Tag.ToString();
			if (S.StartsWith(TEXT("village_hash:"))) { bMarker = true; bMatch |= S.RightChop(13) == Hash; }
		}
	}
	if (bMarker && !bMatch)
	{
		UE_LOG(LogTemp, Warning, TEXT("FORGE vila: o %s é de outra montagem (hash %s): não desenho"), *Path, *Hash);
		return;
	}
	AActor* Holder = World->SpawnActor<AActor>();
	USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("VillageRoot"));
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();
	TMap<FString, UHierarchicalInstancedStaticMeshComponent*> Comps;
	auto Make = [&](UStaticMesh* Mesh, bool bShadow) -> UHierarchicalInstancedStaticMeshComponent*
	{
		UHierarchicalInstancedStaticMeshComponent* C = NewObject<UHierarchicalInstancedStaticMeshComponent>(Holder);
		C->SetStaticMesh(Mesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(bShadow);
		C->SetupAttachment(Root);
		C->RegisterComponent();
		return C;
	};
	auto CompOf = [&](const FString& Mesh) -> UHierarchicalInstancedStaticMeshComponent*
	{
		if (UHierarchicalInstancedStaticMeshComponent** F = Comps.Find(Mesh)) return *F;
		UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Village/%s/StaticMeshes/%s.%s"), *Mesh, *Mesh, *Mesh), nullptr, LOAD_NoWarn | LOAD_Quiet);
		UHierarchicalInstancedStaticMeshComponent* C = SM ? Make(SM, true) : nullptr;
		Comps.Add(Mesh, C);
		return C;
	};
	int32 Drawn = 0, Missing = 0;
	const TArray<TSharedPtr<FJsonValue>>* Pieces = nullptr;
	if (J->TryGetArrayField(TEXT("pieces"), Pieces))
	{
		for (const TSharedPtr<FJsonValue>& V : *Pieces)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			double Yaw, Z;
			if (!O->TryGetNumberField(TEXT("yaw_deg"), Yaw)) Yaw = 90.0 * O->GetNumberField(TEXT("rot"));
			if (!O->TryGetNumberField(TEXT("z"), Z)) Z = O->GetNumberField(TEXT("floor_cm")) / 100.0;
			const FVector Loc(O->GetNumberField(TEXT("x")) * 100.0, O->GetNumberField(TEXT("y")) * 100.0, Z * 100.0);
			if (UHierarchicalInstancedStaticMeshComponent* C = CompOf(O->GetStringField(TEXT("mesh"))))
			{
				C->AddInstance(FTransform(FRotator(0.0, Yaw, 0.0), Loc), true);
				++Drawn;
			}
			else
			{
				++Missing;
			}
		}
	}
	// os fios: o isolador (local da malha do poste → mundo) e a parábola da flecha, em 10 pedaços de 4,5 cm,
	// escuros e sem sombra (a 80 m o pixel tem ~4 cm: o fio de 2 cm de verdade pisca)
	TArray<FVector> PoleP;
	TArray<double> PoleYaw;
	TArray<TArray<FVector>> PoleSockets;
	const TArray<TSharedPtr<FJsonValue>>* Poles = nullptr;
	if (J->TryGetArrayField(TEXT("poles"), Poles))
	{
		for (const TSharedPtr<FJsonValue>& V : *Poles)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			PoleP.Add(FVector(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), O->GetNumberField(TEXT("z"))));
			PoleYaw.Add(O->GetNumberField(TEXT("yaw_deg")));
			TArray<FVector>& S = PoleSockets.AddDefaulted_GetRef();
			const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
			if (O->TryGetArrayField(TEXT("sockets_local"), L))
			{
				for (const TSharedPtr<FJsonValue>& Sv : *L)
				{
					const TArray<TSharedPtr<FJsonValue>>& A = Sv->AsArray();
					if (A.Num() == 3) S.Add(FVector(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber()));
				}
			}
		}
	}
	auto Socket = [&](int32 Pole, int32 I) -> FVector
	{
		if (!PoleP.IsValidIndex(Pole) || !PoleSockets[Pole].IsValidIndex(I)) return FVector::ZeroVector;
		return PoleP[Pole] + FRotator(0.0, PoleYaw[Pole], 0.0).RotateVector(PoleSockets[Pole][I]);
	};
	TArray<FTransform> Segs;
	const TArray<TSharedPtr<FJsonValue>>* Wires = nullptr;
	if (J->TryGetArrayField(TEXT("wires"), Wires))
	{
		for (const TSharedPtr<FJsonValue>& V : *Wires)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			const TArray<TSharedPtr<FJsonValue>>& Aa = O->GetArrayField(TEXT("a"));
			const FVector A = Socket(int32(Aa[0]->AsNumber()), int32(Aa[1]->AsNumber()));
			FVector B;
			const TArray<TSharedPtr<FJsonValue>>* Bb = nullptr;
			if (O->TryGetArrayField(TEXT("b"), Bb) && Bb->Num() == 2) B = Socket(int32((*Bb)[0]->AsNumber()), int32((*Bb)[1]->AsNumber()));
			else if (O->TryGetArrayField(TEXT("b_wall"), Bb) && Bb->Num() == 3) B = FVector((*Bb)[0]->AsNumber(), (*Bb)[1]->AsNumber(), (*Bb)[2]->AsNumber());
			else continue;
			const double Sag = O->GetNumberField(TEXT("sag_m"));
			FVector Prev = A;
			for (int32 K = 1; K <= 10; ++K)
			{
				const double T = K / 10.0;
				const FVector Q = FMath::Lerp(A, B, T) - FVector(0.0, 0.0, 4.0 * Sag * T * (1.0 - T));
				const FVector D = Q - Prev;
				Segs.Add(FTransform(D.Rotation(), 50.0 * (Prev + Q), FVector(D.Size(), 0.045, 0.045)));
				Prev = Q;
			}
		}
	}
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube && Segs.Num())
	{
		UHierarchicalInstancedStaticMeshComponent* C = Make(Cube, false);
		if (UMaterialInterface* Dark = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Structures/Materials/MI_CivChar.MI_CivChar"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			C->SetMaterial(0, Dark);
		}
		C->AddInstances(Segs, false, true, false);
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE vila na qualidade real: %d peças (%d sem malha), %d postes, %d pedaços de fio (%s, hash %s)"),
		Drawn, Missing, PoleP.Num(), Segs.Num(), *MapName, *Hash);
}

void AForgeSliceGameMode::ApplyLook()
{
	// o vento do mapa (as etiquetas wind_yaw: e wind_speed: que o build_slice.py grava) na coleção do vento do
	// jogo (/Game/Materials/MPC_Wind, a cópia do arquivo dela: Wind = (x, y pra onde sopra, m/s, rajada))
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		float Yaw = 1e9f, Speed = 5.0f;
		for (const FName& Tag : It->Tags)
		{
			const FString T = Tag.ToString();
			if (T.StartsWith(TEXT("wind_yaw:"))) Yaw = FCString::Atof(*T.Mid(9));
			if (T.StartsWith(TEXT("wind_speed:"))) Speed = FCString::Atof(*T.Mid(11));
		}
		if (Yaw > 1e8f) continue;
		if (UMaterialParameterCollection* Wind = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Materials/MPC_Wind.MPC_Wind"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			const float R = FMath::DegreesToRadians(Yaw);
			UKismetMaterialLibrary::SetVectorParameterValue(this, Wind, TEXT("Wind"), FLinearColor(FMath::Cos(R), FMath::Sin(R), Speed, 0.3f));
			UE_LOG(LogTemp, Display, TEXT("FORGE vento do mapa: %.1f°, %.1f m/s"), Yaw, Speed);
		}
	}
	// a versão final usa a luz do jogo: a receita do Look do ambiente (Plugins/RTSLook, o ALookDirector
	// do jogo). O painel de montar continua com a luz leve dele (regra do Felipe, pelo Look, 12/10).
	// -ForgeLook=<receita> troca, -ForgeLook=none desliga (A/B)
	FString Recipe;
	if (!FParse::Value(FCommandLine::Get(), TEXT("ForgeLook="), Recipe))
	{
		// o bioma: o mapa de exemplo diz no nome; a prévia do "Ver em qualidade real", no JSON que o app gravou
		const FString MapName = GetWorld()->GetOutermost()->GetName();
		int32 Biome = MapName.Contains(TEXT("Desert")) ? 1 : (MapName.Contains(TEXT("Snow")) ? 2 : (MapName.Contains(TEXT("Forest")) ? 0 : -1));
		FString Text;
		if (Biome < 0 && FFileHelper::LoadFileToString(Text, *(FPaths::ProjectSavedDir() / TEXT("WorldForge/world_settings.json"))))
		{
			TSharedPtr<FJsonObject> J;
			if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) && J.IsValid()
				&& J->GetStringField(TEXT("map")) == MapName && J->HasField(TEXT("biome")))
			{
				Biome = int32(J->GetNumberField(TEXT("biome")));
			}
		}
		// só o deserto tem receita por enquanto. A bancada do Look (desert_golden_hour), não a
		// v2_aprovada: a areia nova só aparece com ela (a v2 tem o sol a 30° e a exposição da areia
		// antiga, e a nova estourava: meio-tom 0,48–0,52 contra 0,30–0,40 das favoritas; regra do
		// Look, 13/10)
		Recipe = Biome == 1 ? TEXT("desert_golden_hour") : TEXT("none");
	}
	if (Recipe == TEXT("none"))
	{
		UE_LOG(LogTemp, Display, TEXT("FORGE look: sem receita (a luz do mapa)"));
		return;
	}
	const FString Path = FString::Printf(TEXT("/RTSLook/Recipes/DA_Look_%s.DA_Look_%s"), *Recipe, *Recipe);
	ULookRecipe* R = LoadObject<ULookRecipe>(nullptr, *Path);
	if (!R)
	{
		UE_LOG(LogTemp, Warning, TEXT("FORGE look: receita %s não achada"), *Path);
		return;
	}
	FActorSpawnParameters P;
	P.Name = TEXT("LookDirector");
	P.bDeferConstruction = true;
	ALookDirector* D = GetWorld()->SpawnActor<ALookDirector>(ALookDirector::StaticClass(), FTransform::Identity, P);
	if (D)
	{
		D->Recipe = R;
		D->FinishSpawning(FTransform::Identity);
		UE_LOG(LogTemp, Display, TEXT("FORGE look: receita %s"), *Recipe);
	}
	// o que a câmera do jogo põe por cima (ARTSCameraPawn::SetFogExposureFix, sempre na partida):
	// a exposição local sem a luminância borrada (0 contra o 0,6 da Unreal). Sem isso a versão
	// final saía mais clara e lavada que o jogo no mesmo mapa e na mesma receita (12/10).
	// -ForgeLEBlend=0.6 volta o padrão (A/B)
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (AForgeCameraPawn* Pawn = PC->GetPawn<AForgeCameraPawn>())
		{
			float Blend = 0.0f;
			FParse::Value(FCommandLine::Get(), TEXT("ForgeLEBlend="), Blend);
			FPostProcessSettings& PP = Pawn->Camera->PostProcessSettings;
			PP.bOverride_LocalExposureBlurredLuminanceBlend = true;
			PP.LocalExposureBlurredLuminanceBlend = Blend;
			Pawn->Camera->PostProcessBlendWeight = 1.0f;
		}
	}
}

void AForgeSliceGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	for (const TSharedPtr<SWidget>& W : { BackButton, ShaderNote })
	{
		if (W.IsValid() && GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(W.ToSharedRef());
		}
	}
	BackButton.Reset();
	ShaderNote.Reset();
	Super::EndPlay(Reason);
}

void AForgeSliceGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (CaptureFolder.IsEmpty())
	{
		return;
	}
	FrameSum += DeltaSeconds;
	++FrameCount;
	// shader ainda compilando (1ª abertura): a Unreal mostra o material
	// padrão quadriculado — foto só depois que terminar
	if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
	{
		Wait = FMath::Max(Wait, 1.0);
		if (FrameCount % 120 == 0)
		{
			UE_LOG(LogTemp, Display, TEXT("FORGE waiting shaders: %d"), GShaderCompilingManager->GetNumRemainingJobs());
		}
		return;
	}
	Wait -= DeltaSeconds;
	if (Wait <= 0.0)
	{
		CaptureStep();
	}
}

void AForgeSliceGameMode::CaptureStep()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AForgeCameraPawn* Pawn = PC ? PC->GetPawn<AForgeCameraPawn>() : nullptr;
	if (!Pawn)
	{
		return;
	}
	Pawn->EdgePan = false;
	Pawn->bLocked = true;
	// -ForgeFadeAt=x,y: o "cursor" da árvore transparente nesse ponto (m), pras fotos
	FString FadeAt;
	if (FParse::Value(FCommandLine::Get(), TEXT("ForgeFadeAt="), FadeAt, false))
	{
		FString X, Y;
		if (FadeAt == TEXT("focus")) Pawn->bFadeAtFocus = true;   // no ponto focado (o passeio)
		else if (FadeAt == TEXT("center")) Pawn->bFadeAtCenter = true;   // raio do pixel do meio, como o mouse
		else if (FadeAt.Split(TEXT(","), &X, &Y)) Pawn->FadePointOverride = FVector2f(FCString::Atof(*X), FCString::Atof(*Y));
		static bool bLogged = false;
		if (!bLogged)
		{
			bLogged = true;
			for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
			{
				if (It->GetWorld() == GetWorld() && It->GetStaticMesh() && It->GetStaticMesh()->GetName().StartsWith(TEXT("SM_")))
				{
					UE_LOG(LogTemp, Display, TEXT("FORGE tree fade material: %s -> %s"), *It->GetStaticMesh()->GetName(), It->GetMaterial(0) ? *It->GetMaterial(0)->GetPathName() : TEXT("none"));
					break;
				}
			}
		}
	}   // o mouse de quem estiver no PC não mexe nas fotos
	const int32 NumViews = Views.Num();
	const int32 View = Step / 2;
	if (View < NumViews)
	{
		// duas fases por vista: põe a câmera e espera assentar; depois fotografa
		if (Step % 2 == 0)
		{
			const FForgeSliceView& V = Views[View];
			Pawn->SetView(V.Focus, V.Yaw, V.Pitch, V.Distance);
			// -ForgeDebugView: o modo teste (F2) no ponto da 1ª vista, e as fotos com a interface;
			// -ForgeDebugToggle=1,3: liga/desliga o F2 no começo dessas vistas (teste do liga-desliga)
			FString Toggles;
			const bool bToggleHere = (View == 0 && FParse::Param(FCommandLine::Get(), TEXT("ForgeDebugView")))
				|| (FParse::Value(FCommandLine::Get(), TEXT("ForgeDebugToggle="), Toggles, false)
					&& (TEXT(",") + Toggles + TEXT(",")).Contains(FString::Printf(TEXT(",%d,"), View)));
			if (bToggleHere)
			{
				if (AForgePlayerController* FPC = Cast<AForgePlayerController>(PC))
				{
					FPC->ToggleDebugView();
				}
			}
			Wait = 1.5;
			// -ForgeShotWait=s: mais tempo antes da foto (as texturas de perto terminam de carregar)
			FParse::Value(FCommandLine::Get(), TEXT("ForgeShotWait="), Wait);
		}
		else
		{
			const FString File = FPaths::Combine(CaptureFolder, FString::Printf(TEXT("%d_%s.png"), View, *Views[View].Name));
			const bool bUI = FParse::Param(FCommandLine::Get(), TEXT("ForgeDebugView")) || FParse::Param(FCommandLine::Get(), TEXT("ForgeShotUI"));
			// janela minimizada (a tela vira 8×8): a Unreal quebra ao copiar a foto (D3D12Texture.cpp)
			FVector2D ShotSize(0, 0);
			if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(ShotSize);
			if (ShotSize.X >= 64 && ShotSize.Y >= 64)
				FScreenshotRequest::RequestScreenshot(File, bUI, false);
			// -ForgeDumpMemory: o que ocupa a memória de vídeo, no log (na 1ª foto)
			FString DumpAt;
			if (FParse::Value(FCommandLine::Get(), TEXT("ForgeDumpMemory="), DumpAt) && (TEXT(",") + DumpAt + TEXT(",")).Contains(FString::Printf(TEXT(",%d,"), View)) && GEngine)
			{
				GEngine->Exec(GetWorld(), TEXT("rhi.DumpMemory"));
				GEngine->Exec(GetWorld(), TEXT("rhi.DumpResourceMemory 60"));
				GEngine->Exec(GetWorld(), TEXT("r.LumenScene.DumpStats 1"));   // fichas (cards) da iluminação global
			}
			FTextureMemoryStats Mem;
			RHIGetTextureMemoryStats(Mem);
			UE_LOG(LogTemp, Display, TEXT("FORGE shot %s · VRAM %.0f/%.0f MB, texturas %.0f MB (pool %.0f MB), estourado %.0f MB · %.1f ms"), *File,
				(Mem.StreamingMemorySize + Mem.NonStreamingMemorySize) / 1048576.0, Mem.TotalGraphicsMemory / 1048576.0,
				Mem.StreamingMemorySize / 1048576.0, Mem.TexturePoolSize / 1048576.0, GDemotedLocalMemorySize / 1048576.0,
				1000.0 * FrameSum / FMath::Max(FrameCount, 1));
			Wait = 0.3;
		}
		++Step;
		return;
	}
	// medição limpa: 4 s parado na vista média, sem foto nenhuma
	if (MotionFrame < 0 && !bMeasured)
	{
		if (MeasureStart < 0.0)
		{
			const FForgeSliceView& V = Views[FMath::Min(1, Views.Num() - 1)];
			Pawn->SetView(V.Focus, V.Yaw, V.Pitch, V.Distance);
			MeasureStart = GetWorld()->GetRealTimeSeconds();
			FrameSum = 0.0;
			FrameCount = 0;
			Wait = 4.0;
			// -ForgeProfile (com -csvGpuStats): tempos de GPU por passe nos mesmos 4 s
			// (Saved/Profiling/CSV; Tools/gpu_profile.py resume)
			if (FParse::Param(FCommandLine::Get(), TEXT("ForgeProfile")))
			{
				PC->ConsoleCommand(TEXT("csvprofile start"));
			}
			return;
		}
		const double Mean = FrameSum / FMath::Max(FrameCount, 1);
		UE_LOG(LogTemp, Display, TEXT("FORGE mean frame %.1f ms (%.0f fps) over %d frames (vista média, sem fotos)"), 1000.0 * Mean, 1.0 / FMath::Max(Mean, 1e-6), FrameCount);
		bMeasured = true;
		if (FParse::Param(FCommandLine::Get(), TEXT("ForgeProfile")))
		{
			PC->ConsoleCommand(TEXT("csvprofile stop"));
			Wait = 3.0;   // o CSV é gravado em segundo plano: fechar já deixava o arquivo vazio
			return;
		}
	}
	if (bNoMotion)
	{
		MotionFrame = MotionFrames;   // só as fotos e a medição
	}
	// passeio: a câmera de RTS anda 45 m em linha, um passo fixo por quadro
	// (o movimento não depende da taxa de quadros); uma foto por quadro
	if (MotionFrame < 0)
	{
		// põe a câmera no começo do passeio e espera: o salto da vista anterior
		// borrava o 1º quadro (desfoque de movimento)
		// (-ForgeMotion=x0,y0,x1,y1,yaw,pitch,dist troca o passeio: 14/10, o tremor da borda da laje em movimento)
		FString Mo;
		if (FParse::Value(FCommandLine::Get(), TEXT("ForgeMotion="), Mo, false))
		{
			TArray<FString> Parts;
			Mo.ParseIntoArray(Parts, TEXT(","));
			if (Parts.Num() == 7)
			{
				MotionA = FVector2f(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]));
				MotionB = FVector2f(FCString::Atof(*Parts[2]), FCString::Atof(*Parts[3]));
				MotionView = FVector(FCString::Atof(*Parts[4]), FCString::Atof(*Parts[5]), FCString::Atof(*Parts[6]));
			}
		}
		MotionFrame = 0;
		Pawn->SetView(MotionA, MotionView.X, MotionView.Y, MotionView.Z);
		Wait = 1.0;
		return;
	}
	if (MotionFrame < MotionFrames)
	{
		const float T = float(MotionFrame) / (MotionFrames - 1);
		Pawn->SetView(MotionA + (MotionB - MotionA) * T, MotionView.X, MotionView.Y, MotionView.Z);
		const FString File = FPaths::Combine(CaptureFolder, FString::Printf(TEXT("motion_%03d.png"), MotionFrame));
		FScreenshotRequest::RequestScreenshot(File, false, false);
		++MotionFrame;
		Wait = 0.0;
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE capture done"));
	CaptureFolder.Empty();
	if (FParse::Param(FCommandLine::Get(), TEXT("ForgeBack")))
	{
		// teste do botão "voltar ao gerador": o mesmo caminho do clique
		UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Maps/Forge")), true, ResumeOptions);
		return;
	}
	FGenericPlatformMisc::RequestExit(false);
}
