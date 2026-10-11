#include "RTSPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
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
