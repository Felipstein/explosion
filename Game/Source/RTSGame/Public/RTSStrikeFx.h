#pragma once

// (v4, 23/10) O anel de aviso do Cruise Missile Strike no chão (só desenho; nada volta pra simulação). Onde o ataque vai
// cair, por 4 s: azul pra quem lançou (e os aliados dele), em vermelho pro inimigo que vê o ponto (o spec v4,
// power_cruise_strike: "the caster sees the blue ring for 4 s; an enemy of the caster sees a red ring"). Uma fita fina
// rente ao chão (segue o relevo) com 4 marcas pro centro, a primeira apontando pra quem lançou (de onde os mísseis vêm),
// no material aditivo dos efeitos (M_Glow: a cor é a radiância, pelo dado de primitiva 0–2); pisca devagar e some no fim.
// O aviso escrito (sem a seta: a do HUD só aponta pra direita) é do HUD (URTSMatchSubsystem::FHudNotice). Sem fumaça: a regra da fumaça não entra.

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class AActor;
class UMaterialInterface;
class UProceduralMeshComponent;
class UWorld;

class FRTSStrikeRings
{
public:
	FRTSStrikeRings() = default;
	~FRTSStrikeRings();
	FRTSStrikeRings(const FRTSStrikeRings&) = delete;
	FRTSStrikeRings& operator=(const FRTSStrikeRings&) = delete;

	/** A cor do anel do inimigo (o "red ring" do spec), antes da radiância. */
	static FLinearColor WarningColor() { return FLinearColor(1.0f, 0.06f, 0.03f); }

	/**
	 * Um anel novo em CenterCm com o raio do girassol (RadiusCm). Color = a cor (o azul de quem lançou ou WarningColor; o brilho sai daqui);
	 * FromYawDeg = de onde os mísseis vêm (graus, o rumo de quem lançou pro ponto). NowTicks = o relógio do desenho em passos
	 * da simulação (contínuo: a pausa congela o anel junto). GroundZCm = a altura do chão (cm) num ponto (cm). ViewCm = a
	 * câmera agora (nula = normais pra cima): o M_Glow brilha por (N·V)², e a fita deitada vista a 37,5° ficaria com ~37% do
	 * brilho; com a normal virada pra câmera ela acende inteira (e a câmera do RTS anda sem girar: muda pouco nos 4 s).
	 */
	void Add(UWorld* World, const FVector2D& CenterCm, float RadiusCm, const FLinearColor& Color, float FromYawDeg, double NowTicks,
		TFunctionRef<float(const FVector2D&)> GroundZCm, const FVector* ViewCm = nullptr);
	/** A cada quadro: acende, pisca e some; tira os que acabaram. */
	void Tick(double NowTicks);
	void Shutdown();
	int32 Num() const { return Rings.Num(); }

private:
	struct FRing
	{
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<UProceduralMeshComponent> Mesh;
		double BornTicks = 0.0;
		FLinearColor Radiance = FLinearColor::Black;
		float LastLevel = -1.0f;
	};
	TArray<FRing> Rings;
	/** Preso enquanto a partida dura (o GC não descarrega entre um ataque e outro). */
	TStrongObjectPtr<UMaterialInterface> Material;
};
