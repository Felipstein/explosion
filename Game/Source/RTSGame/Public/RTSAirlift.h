#pragma once

// (v4, 23/10) O avião de carga da zona de lançamento (o Supply Drop Zone da USA). Só desenho: a simulação já decidiu quando
// e quanto (AirliftInbound, AirliftLanded, a Cargo da zona). O spec v4 (usa_drop_zone, "Airlift actor"):
//  - no aviso o C-130 nasce 210 m antes do pátio, na linha que sai da largada do dono e passa pela zona, a altitude_m acima
//    do pátio, a 60 m/s; passa sobre o pátio 6,5 s antes do pouso e solta um paraquedas por palete pela rampa (na chegada,
//    antes do pátio: o embalo pra frente termina na vaga, sem voltar contra o rumo);
//  - os paraquedas caem no relógio da simulação (o passo do aviso + LeadTicks): tocam as vagas dos paletes no passo do pouso
//    e somem no AirliftLanded, no quadro em que as caixas aparecem no pátio (ARTSUnitActor::SetCargo); o pano murcha 0,6 s;
//  - o avião segue até a borda do mapa e some;
//  - tudo só onde o jogador local vê (IsPointShown); sem fumaça e sem flare (a regra da fumaça não entra).
// A carga pendurada é a mesma caixa do pátio (Units.json cargo.mesh), nas mesmas vagas 2 × 2 do palete: ao tocar o chão a
// troca pelas caixas do prédio não pula. Os modelos provisórios (game.ps1 v4models): o avião, o paraquedas e a caixa do
// lançamento; sem a malha, um desenho de reserva com as formas da engine (nada some se a importação atrasar).

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class AActor;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UWorld;

/** A zona como o desenho vê neste quadro (a partida preenche). */
struct FRTSAirliftZone
{
	/** O quadro do lote (o Body do ator: o centro no chão e o giro do prédio), cm. */
	FTransform Body = FTransform::Identity;
	/** As caixas no pátio agora (a simulação) e quantas fazem um palete. */
	int32 Cargo = 0;
	int32 BoxesPerPallet = 4;
	/** As vagas das caixas no quadro do lote (cm; Units.json "cargo.slots_m"), na ordem em que enchem, e a malha da caixa
	 *  (vazias: o visual de reserva da zona, sem caixas no pátio; os paraquedas usam as vagas padrão do modelo). */
	TArray<FVector> SlotsCm;
	FString CrateMesh;
};

class FRTSAirlift
{
public:
	FRTSAirlift() = default;
	~FRTSAirlift();
	FRTSAirlift(const FRTSAirlift&) = delete;
	FRTSAirlift& operator=(const FRTSAirlift&) = delete;

	/** O aviso: o avião da zona Zone nasce, com Pallets paletes; as caixas tocam o pátio em LandTicks (o relógio do desenho, em
	 *  passos da simulação). FromCm = a largada do dono; AltitudeM = a altura do voo acima do pátio (Units.json airlift). */
	void Inbound(UWorld* World, uint32 Zone, int32 Pallets, double LandTicks, const FRTSAirliftZone& Info, const FVector2D& FromCm,
		float AltitudeM, const FLinearColor& OwnerColor);
	/** O pouso: os paraquedas da zona entregam as caixas agora (o quadro em que o pátio mostra as caixas). */
	void Landed(uint32 Zone);
	/** A cada quadro, no relógio do desenho (passos, contínuo; a pausa congela). ZoneOf: a zona viva agora (falso = morreu: os
	 *  paraquedas terminam a descida no pátio de antes); IsShown: o ponto à vista do jogador local; MapCm: o retângulo do mapa
	 *  (o avião some depois de sair dele). */
	void Tick(double NowTicks, TFunctionRef<bool(uint32, FRTSAirliftZone&)> ZoneOf, TFunctionRef<bool(const FVector2D&)> IsShown,
		const FBox2D& MapCm);
	void Shutdown();
	int32 Num() const { return Flights.Num(); }

private:
	struct FChute
	{
		TWeakObjectPtr<AActor> Actor;
		/** O pivô do pano (abre, gira devagar e murcha) e as caixas penduradas. */
		TWeakObjectPtr<USceneComponent> Canopy;
		TArray<TWeakObjectPtr<UStaticMeshComponent>> Crates;
		/** A ordem na rampa (0..N−1): desce na vaga do palete (os paletes no pátio agora) + Index. */
		int32 Index = 0;
		double ReleaseTicks = 0.0;
		FVector ReleaseCm = FVector::ZeroVector;
		/** O alvo no pátio (o centro do palete, cm): anda devagar se a empilhadeira esvaziar uma vaga durante a queda. */
		FVector2D SpotCm = FVector2D::ZeroVector;
		bool bSpot = false;
		/** O arremesso pra frente (cm, no rumo do avião): da soltura até a vaga, medido quando a vaga é achada. */
		double ThrowCm = 0.0;
		float Phase = 0.0f;
		/** O toque no chão (passos): as caixas somem e o pano murcha. < 0 = ainda no ar. */
		double LandedTicks = -1.0;
		bool bShown = true;
	};
	struct FFlight
	{
		uint32 Zone = 0;
		int32 Pallets = 0;
		double PassTicks = 0.0;
		double LandTicks = 0.0;
		FVector2D Dir = FVector2D(1.0, 0.0);
		float AltitudeCm = 7000.0f;
		FLinearColor Color = FLinearColor::White;
		TWeakObjectPtr<AActor> Plane;
		bool bPlaneShown = true;
		bool bPlaneGone = false;
		int32 Released = 0;
		bool bLanded = false;
		/** A zona vista por último (o quadro do lote, as vagas, a malha da caixa). */
		FRTSAirliftZone LastZone;
		TArray<FChute> Chutes;
	};

	/** O avião no relógio Ticks: a reta pelo pátio, na altura do voo. */
	FVector PlaneAt(const FFlight& F, double Ticks) const;
	double ReleaseTicksOf(const FFlight& F, int32 Index) const;
	void SpawnPlane(FFlight& F);
	void SpawnChute(FFlight& F, FChute& C);
	void UpdateChute(FFlight& F, FChute& C, double Now, float DtS, TFunctionRef<bool(const FVector2D&)> IsShown);
	void MarkLanded(FFlight& F, double AtTicks);
	/** A malha (o caminho de pacote do Units.json, "…/StaticMeshes/<nome>"); nula se não existe. Guardada (o GC não descarrega
	 *  entre um avião e outro, e a que falta não é procurada de novo). */
	UStaticMesh* MeshAt(const FString& PackagePath);
	UMaterialInterface* MaterialAt(const TCHAR* ObjectPath);
	AActor* SpawnBare(const TCHAR* Name) const;

	TWeakObjectPtr<UWorld> WorldRef;
	TArray<FFlight> Flights;
	TMap<FString, TStrongObjectPtr<UStaticMesh>> Meshes;
	TMap<FString, TStrongObjectPtr<UMaterialInterface>> Materials;
	double LastNowTicks = -1.0;
};
