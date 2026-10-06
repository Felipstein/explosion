#pragma once

#include "GameFramework/Actor.h"
#include "BlastLabMover.generated.h"

class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FBattleSmokeDisturber;

UENUM()
enum class EBlastMover : uint8
{
	Plane,       // avião de ataque ao solo
	Missile,     // míssil terra-ar
	Shell,       // disparo de tanque (120 mm, com traçante)
	Bomber,      // bombardeiro pesado (classe B-2), a esteira grande
};

/**
 * O que atravessa o campo (porte de reference/webgl/js/movers.js).
 *
 * Existe pra fumaça REAGIR: a cada quadro vira um "perturbador" do fluido do
 * campo (UBattleSmokeSubsystem) com o modelo do que ele faz no ar:
 *  - avião (o de ataque e o bombardeiro pesado, uma asa voadora que corta a
 *    nuvem como uma lâmina): o corpo arrasta o ar e abre um túnel; a asa deixa o par de
 *    vórtices de ponta de asa (Γ = W/(ρ·U·b0), b0 = π/4·b pra asa elíptica,
 *    Prandtl), que desce sozinho e enrola a fumaça em dois tubos; o jato dos
 *    motores sopra pra trás;
 *  - míssil: jato do motor-foguete e a fumaça branca do propelente sólido
 *    (óxido de alumínio), que fica no campo como fumaça clara;
 *  - disparo: o choque e a esteira turbulenta do projétil abrem um túnel fino.
 * Uma explosão ainda tocando a gravação depois de 2.8 s, se atravessada, passa
 * pro campo na hora (UBlastLabSimulation), pra poder reagir.
 *
 * A geometria é de blocos (as mesmas peças do WebGL), só pra ver o que passou.
 * Física em metros nos eixos da Unreal; o ator anda em cm.
 */
UCLASS()
class BLASTLAB_API ABlastLabMover : public AActor
{
	GENERATED_BODY()

public:
	ABlastLabMover();

	/**
	 * Lança um. TargetCm: o ponto no chão sob a fumaça; Side: direção
	 * horizontal (unitária) em que ele cruza a tela; AltM: altura da fumaça
	 * ali, por onde ele passa (negativa: não se sabe, usa a do WebGL).
	 */
	static ABlastLabMover* Launch(UWorld* World, EBlastMover Kind, const FVector& TargetCm, const FVector& Side, float AltM);

	EBlastMover GetKind() const { return Kind; }

	/** Segundos do lançamento até ele chegar sobre o alvo, passando a AltM de altura (a mira leva a nuvem subindo). */
	static float TimeToTarget(EBlastMover Kind, float AltM);

	virtual void Tick(float DeltaTime) override;

private:
	void Build();
	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& CenterM, const FVector& SizeM,
		bool bAlongForward, const FLinearColor& Color, float Rough, float Metal, bool bEmissive, float YawDeg = 0.0f);
	void Disturb();
	void Send(const FBattleSmokeDisturber& D, float CrossRadiusM);
	void UpdateGeometry();

	EBlastMover Kind = EBlastMover::Plane;
	FVector Pos = FVector::ZeroVector;      // m
	FVector Prev = FVector::ZeroVector;
	FVector Dir = FVector::ForwardVector;
	float Speed = 0.0f;                     // m/s
	float Age = 0.0f;                       // s de simulação
	float Life = 1.0f;
	int32 WakeId = 0;                       // a esteira do avião no campo (UBattleSmokeSubsystem)

	// peças que mudam a cada quadro
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Flame;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Tracer;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> TracerGlow;
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> FlameLight;
};
