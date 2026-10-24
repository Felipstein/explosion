#include "RTSPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/ICursor.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RTSConvert.h"
#include "RTSGameData.h"
#include "RTSMapBaker.h"
#include "RTSHUD.h"
#include "RTSMatchSubsystem.h"

// A parte da economia e da construção no controle do jogador (HUD provisório, até o HUD
// de verdade sair da arte: docs/HUD.md). Os gestos seguem o ZH:
//  - com o Dozer selecionado, o botão de um prédio entra no modo de posicionar: o fantasma
//    segue o mouse, o clique esquerdo marca a obra, o direito ou o Esc desistem, R gira
//    (no ZH se gira arrastando; aqui é provisório) e Shift segue posicionando;
//  - com prédio selecionado, o clique no chão marca o ponto de encontro;
//  - caminhão + clique no depósito = coletar ali; Dozer + clique na obra parada = retomar.

bool ARTSPlayerController::GetCursorGround(FVector& Out) const
{
	float X = 0.0f;
	float Y = 0.0f;
	return ReadMouse(X, Y) && GroundUnderCursor(FVector2D(X, Y), Out);
}

uint32 ARTSPlayerController::SelectedStructure() const
{
	URTSMatchSubsystem* M = Match();
	if (!M)
	{
		return 0;
	}
	for (const uint32 Id : Selected)
	{
		FRTSUnitState U;
		if (M->GetUnit(Id, U) && U.bStructure && U.Owner == M->LocalPlayer())
		{
			return Id;
		}
	}
	return 0;
}

void ARTSPlayerController::SelectedWith(bool bConstructors, TArray<uint32>& Out) const
{
	Out.Reset();
	URTSMatchSubsystem* M = Match();
	if (!M)
	{
		return;
	}
	for (const uint32 Id : Selected)
	{
		FRTSUnitState U;
		FRTSDefInfo D;
		if (M->GetUnit(Id, U) && U.Owner == M->LocalPlayer() && M->GetDef(U.Def, D) && (bConstructors ? D.Constructs.Num() > 0 : D.SupplyBoxes > 0))
		{
			Out.Add(Id);
		}
	}
}

void ARTSPlayerController::BeginPlacing(int32 Def)
{
	PlacingDef = Def;
	bAttackMoveArmed = false;
	// a frente do prédio pra quem joga (pra baixo da tela), como a vista 3/4 do ZH
	PlacingDir = PlayerCameraManager ? RTS::DirFacingViewer(PlayerCameraManager->GetCameraRotation().Yaw) : 0;
}

bool ARTSPlayerController::TickPlacement(const FVector2D& Mouse, bool bHasMouse)
{
	if (PlacingDef < 0)
	{
		return false;
	}
	URTSMatchSubsystem* M = Match();
	TArray<uint32> Dozers;
	SelectedWith(true, Dozers);
	if (!M || Dozers.IsEmpty())
	{
		PlacingDef = -1;   // o Dozer morreu ou saiu da seleção
		return false;
	}
	// o botão direito é do controle da câmera: arrastar rola, o clique simples desiste (ao soltar,
	// sem tirar a seleção; RTSPlayerController.cpp). Desistir ao apertar matava o arraste (10/10)
	if (WasInputKeyJustPressed(EKeys::R))
	{
		PlacingDir = (PlacingDir + 1) & 3;
	}
	const ARTSHUD* HUD = GetRTSHUD();
	if ((HUD && !HUD->ButtonAt(Mouse).IsNone()) || bOverMinimap)
	{
		return false;   // os botões do painel e o minimapa seguem valendo
	}
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && bHasMouse)
	{
		FVector Ground;
		if (GroundUnderCursor(Mouse, Ground))
		{
			FVector2D Center;
			FVector2D Half;
			if (M->CanPlace(uint16(PlacingDef), PlacingDir, FVector2D(Ground.X, Ground.Y), Center, Half) == ERTSPlace::Ok)
			{
				M->OrderBuild(Dozers, uint16(PlacingDef), PlacingDir, Center);
				AddMarker(FVector(Center.X, Center.Y, Ground.Z), false);
				++Orders;
				if (!IsInputKeyDown(EKeys::LeftShift) && !IsInputKeyDown(EKeys::RightShift))
				{
					PlacingDef = -1;
				}
			}
		}
	}
	return true;   // posicionando: o clique esquerdo é do fantasma (nem caixa nem ordem)
}

void ARTSPlayerController::UpdateGhost()
{
	URTSMatchSubsystem* M = Match();
	const FRTSBakedMap* Map = M ? M->BakedMap() : nullptr;
	FVector Ground;
	if (!Map || PlacingDef < 0 || !GetCursorGround(Ground))
	{
		if (Ghost)
		{
			Ghost->SetActorHiddenInGame(true);
		}
		return;
	}
	if (!Ghost)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Ghost = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		if (!Ghost)
		{
			return;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Ghost);
		Ghost->SetRootComponent(Root);
		Root->RegisterComponent();
		GhostMesh = NewObject<UStaticMeshComponent>(Ghost);
		GhostMesh->SetupAttachment(Root);
		GhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GhostMesh->SetCastShadow(false);
		GhostMesh->SetCanEverAffectNavigation(false);
		GhostMesh->bDisallowNanite = true;   // transparente: o Nanite só desenha opaco e recortado
		GhostMesh->RegisterComponent();
	}
	if (GhostDef != PlacingDef)
	{
		GhostDef = PlacingDef;
		const FRTSUnitVisual* Visual = M->VisualOf(uint16(PlacingDef));
		UStaticMesh* Mesh = nullptr;
		if (Visual && Visual->HullMeshes.Num())
		{
			const FString& Path = Visual->HullMeshes[0];
			Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *Path, *FPaths::GetBaseFilename(Path)), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		GhostMesh->SetStaticMesh(Mesh);
		UMaterialInterface* GhostMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Structures/Materials/M_StructureGhost.M_StructureGhost"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		for (int32 Slot = 0; Mesh && GhostMaterial && Slot < GhostMesh->GetNumMaterials(); ++Slot)
		{
			GhostMesh->SetMaterial(Slot, GhostMaterial);
		}
	}
	FVector2D Center;
	FVector2D Half;
	const bool bOk = M->CanPlace(uint16(PlacingDef), PlacingDir, FVector2D(Ground.X, Ground.Y), Center, Half) == ERTSPlace::Ok;
	GhostMesh->SetCustomPrimitiveDataVector4(0, bOk ? FVector4(0.25f, 1.0f, 0.45f, 1.0f) : FVector4(1.0f, 0.16f, 0.10f, 1.0f));
	Ghost->SetActorLocationAndRotation(FVector(Center.X, Center.Y, Map->GroundZCm(Center)), FRotator(0.0f, float(PlacingDir & 3) * 90.0f, 0.0f));
	Ghost->SetActorHiddenInGame(false);
}

int32 ARTSPlayerController::DockAt(const FVector& Ground) const
{
	URTSMatchSubsystem* M = Match();
	if (!M)
	{
		return -1;
	}
	for (int32 K = 0; K < M->DockCount(); ++K)
	{
		FRTSDockState D;
		// a pilha pequena é fácil de errar de cima: um pouco de folga no clique
		const float Slack = 200.0f;
		if (M->GetDock(K, D) && D.Boxes > 0 && FMath::Abs(Ground.X - D.PosCm.X) <= D.HalfCm.X + Slack && FMath::Abs(Ground.Y - D.PosCm.Y) <= D.HalfCm.Y + Slack)
		{
			return K;
		}
	}
	return -1;
}

uint32 ARTSPlayerController::StructureAt(const FVector& Ground) const
{
	URTSMatchSubsystem* M = Match();
	if (!M)
	{
		return 0;
	}
	TArray<FRTSUnitState> Units;
	M->GetUnits(Units);
	for (const FRTSUnitState& U : Units)
	{
		FRTSDefInfo D;
		if (!U.bStructure || !M->IsUnitShown(U.Id) || !M->GetDef(U.Def, D))
		{
			continue;
		}
		const bool bTurned = (U.Dir & 1) != 0;
		const double HX = (bTurned ? D.SizeM.Y : D.SizeM.X) * 50.0;
		const double HY = (bTurned ? D.SizeM.X : D.SizeM.Y) * 50.0;
		if (FMath::Abs(Ground.X - U.PosCm.X) <= HX && FMath::Abs(Ground.Y - U.PosCm.Y) <= HY)
		{
			return U.Id;
		}
	}
	return 0;
}

bool ARTSPlayerController::HandleEconomyClick(const FVector2D& Mouse, uint32 Hit)
{
	URTSMatchSubsystem* M = Match();
	if (!M || Selected.IsEmpty())
	{
		return false;
	}
	FRTSUnitState HitUnit;
	const bool bHit = Hit != 0 && M->GetUnit(Hit, HitUnit);
	TArray<uint32> Dozers;
	SelectedWith(true, Dozers);
	if (bHit && !Dozers.IsEmpty() && HitUnit.Owner == M->LocalPlayer() && HitUnit.bStructure && HitUnit.BuildFraction < 1.0f)
	{
		M->OrderResumeBuild(Dozers, Hit);   // ZH: o Dozer retoma a obra parada
		++Orders;
		return true;
	}
	if (bHit && !Dozers.IsEmpty() && HitUnit.bStructure && M->CanRepair(Hit))
	{
		// (v4, 23/10) o Dozer conserta o prédio avariado (o ZH: o cursor da chave sobre o prédio, o clique manda; o dele ou o do aliado)
		M->OrderRepair(Dozers, Hit);
		const FRTSBakedMap* Map = M->BakedMap();
		AddMarker(FVector(HitUnit.PosCm.X, HitUnit.PosCm.Y, Map ? Map->GroundZCm(HitUnit.PosCm) : 0.0f), false);
		++Orders;
		return true;
	}
	if (bHit)
	{
		return false;   // clique numa unidade: seleção ou ataque, como sempre
	}
	FVector Ground;
	if (!GroundUnderCursor(Mouse, Ground))
	{
		return false;
	}
	TArray<uint32> Trucks;
	SelectedWith(false, Trucks);
	const int32 Dock = DockAt(Ground);
	if (!Trucks.IsEmpty() && Dock >= 0)
	{
		M->OrderGather(Trucks, Dock);
		AddMarker(Ground, false);
		++Orders;
		return true;
	}
	bool bOnlyStructures = true;
	for (const uint32 Id : Selected)
	{
		FRTSUnitState U;
		bOnlyStructures &= M->GetUnit(Id, U) && U.bStructure && U.Owner == M->LocalPlayer();
	}
	if (bOnlyStructures)
	{
		// prédio não anda: o clique no chão marca o ponto de encontro (ZH)
		M->OrderRally(Selected, FVector2D(Ground.X, Ground.Y));
		AddMarker(Ground, false);
		++Orders;
		return true;
	}
	return false;
}

bool ARTSPlayerController::EnsureRepairCursor()
{
	// (v4, 23/10) o cursor do conserto do ZH (a chave): a do kit é um botão (a chave clara na placa escura, 256 px); aqui a chave
	// sai da placa pela luz (fora da moldura, a parte clara), reduzida a 32 px pela média com peso do alfa, com 1 px de contorno
	// escuro (lê na areia clara e no asfalto, como os cursores do ZH). Feito uma vez, no buffer (sem arquivo no pacote)
	if (RepairCursorState != 0)
	{
		return RepairCursorState > 0;
	}
	RepairCursorState = -1;
	const TSharedPtr<ICursor> Platform = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetPlatformCursor() : nullptr;
	if (!Platform.IsValid() || !Platform->IsCreateCursorFromRGBABufferSupported())
	{
		return false;
	}
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../docs/concept/hud/kit/icons/repair.png"));
	TArray<uint8> Png;
	if (!FFileHelper::LoadFileToArray(Png, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS: o cursor do conserto não achou %s (fica a mão)"), *Path);
		return false;
	}
	IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const TSharedPtr<IImageWrapper> Png8 = Images.CreateImageWrapper(EImageFormat::PNG);
	TArray<uint8> Raw;
	if (!Png8.IsValid() || !Png8->SetCompressed(Png.GetData(), Png.Num()) || !Png8->GetRaw(ERGBFormat::BGRA, 8, Raw))
	{
		return false;
	}
	const int32 SW = int32(Png8->GetWidth());
	const int32 SH = int32(Png8->GetHeight());
	if (SW < 32 || SH < 32 || Raw.Num() < SW * SH * 4)
	{
		return false;
	}
	// a chave: dentro da moldura (14% de cada lado), a luz entre 0,36 e 0,50 vira o alfa (o smoothstep); a placa e os rebites ficam fora
	constexpr int32 N = 32;
	constexpr float Inset = 0.14f, LumLo = 0.36f, LumHi = 0.50f;
	TArray<FLinearColor> Cell;
	Cell.SetNumZeroed(N * N);
	for (int32 Y = 0; Y < N; ++Y)
	{
		for (int32 X = 0; X < N; ++X)
		{
			FLinearColor Sum(0.0f, 0.0f, 0.0f, 0.0f);
			float Weight = 0.0f;
			int32 Count = 0;
			for (int32 SY = Y * SH / N; SY < (Y + 1) * SH / N; ++SY)
			{
				for (int32 SX = X * SW / N; SX < (X + 1) * SW / N; ++SX)
				{
					const uint8* Px = &Raw[(SY * SW + SX) * 4];   // B, G, R, A
					const float R = Px[2] / 255.0f, G = Px[1] / 255.0f, B = Px[0] / 255.0f;
					const bool bInside = SX >= Inset * SW && SX < (1.0f - Inset) * SW && SY >= Inset * SH && SY < (1.0f - Inset) * SH;
					const float T = FMath::Clamp((0.299f * R + 0.587f * G + 0.114f * B - LumLo) / (LumHi - LumLo), 0.0f, 1.0f);
					const float A = bInside ? T * T * (3.0f - 2.0f * T) : 0.0f;
					Sum += FLinearColor(R * A, G * A, B * A, 0.0f);
					Weight += A;
					++Count;
				}
			}
			FLinearColor& C = Cell[Y * N + X];
			C = Weight > 0.0f ? FLinearColor(Sum.R / Weight, Sum.G / Weight, Sum.B / Weight, Weight / float(FMath::Max(Count, 1))) : FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(N * N);
	for (int32 Y = 0; Y < N; ++Y)
	{
		for (int32 X = 0; X < N; ++X)
		{
			FLinearColor C = Cell[Y * N + X];
			if (C.A < 0.5f)
			{
				float Near = 0.0f;
				for (int32 DY = -1; DY <= 1; ++DY)
				{
					for (int32 DX = -1; DX <= 1; ++DX)
					{
						const int32 NX = X + DX, NY = Y + DY;
						Near = NX >= 0 && NY >= 0 && NX < N && NY < N ? FMath::Max(Near, Cell[NY * N + NX].A) : Near;
					}
				}
				if (Near >= 0.5f)
				{
					C = FLinearColor(0.05f, 0.05f, 0.05f, FMath::Max(C.A, 0.85f));   // o contorno
				}
			}
			Pixels[Y * N + X] = FColor(uint8(FMath::RoundToInt(C.R * 255.0f)), uint8(FMath::RoundToInt(C.G * 255.0f)), uint8(FMath::RoundToInt(C.B * 255.0f)),
				uint8(FMath::RoundToInt(FMath::Clamp(C.A, 0.0f, 1.0f) * 255.0f)));
		}
	}
	RepairCursorHandle = Platform->CreateCursorFromRGBABuffer(Pixels.GetData(), N, N, FVector2D(0.5, 0.5));
	if (!RepairCursorHandle)
	{
		return false;
	}
	Platform->SetTypeShape(EMouseCursor::Custom, RepairCursorHandle);
	RepairCursorState = 1;
	UE_LOG(LogTemp, Log, TEXT("RTS: o cursor do conserto pronto (a chave do kit, %d px)"), N);
	return true;
}
