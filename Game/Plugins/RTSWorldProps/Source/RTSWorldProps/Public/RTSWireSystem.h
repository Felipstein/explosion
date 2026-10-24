#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSWireSystem.generated.h"

class AActor;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/** O handle do vão e o ponto (cm, mundo). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FRTSWireEvent, int32 /*Handle*/, const FVector& /*PointCm*/);

/**
 * Os fios de luz soltos quando o tanque derruba o poste (16/10, WorldForge; o pedido do Felipe pelo Look: "os postes e
 * os fios com física pro tanque derrubar, com o choque"; o estudo em docs/WORLDFORGE.md §12.8).
 *
 * O fio parado é a catenária da camada de fios do jogo. No tombo, o jogo esconde o vão e chama ReleaseSpan: o vão vira
 * uma corrente de pontos (Verlet, como o Cable Component da Unreal), presa no poste de pé e na cruzeta do poste que cai
 * (MoveSpanEnd a cada quadro). Ela balança, deita no chão (SetGroundHeight) e, se esticar mais de 1,5× o comprimento,
 * arrebenta na ponta do poste que cai: a ponta solta cai e quica no chão (OnWireSnap e OnWireGround, pro arco e a
 * faísca, que são do jogo e do Fluxo). Quando assenta, o vão dorme (sem custo) e fica desenhado como parou.
 *
 * Só desenho: nada volta pra simulação, e o resultado não precisa ser igual em todos os PCs.
 */
UCLASS()
class RTSWORLDPROPS_API URTSWireSystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Solta um vão. A e B (cm, mundo) são as pontas na cruzeta; SagM, a flecha do meio (m); RadiusCm, o raio do fio.
	 * Mesh: o pedaço de fio (nulo = o cilindro da Unreal, de pé no Z, 100 cm); Material: o do fio. FollowEnd: 0 ou 1, a
	 * ponta presa no poste que cai (a outra fica presa no poste de pé). Devolve o handle (INDEX_NONE se não deu).
	 */
	int32 ReleaseSpan(const FVector& ACm, const FVector& BCm, float SagM, float RadiusCm, UStaticMesh* Mesh, UMaterialInterface* Material, int32 FollowEnd);

	/** A ponta que segue a cruzeta do poste que cai, a cada quadro (End = o FollowEnd do ReleaseSpan). */
	void MoveSpanEnd(int32 Handle, int32 End, const FVector& PosCm);

	/** A altura do chão (cm) no ponto (cm): o fio deita e quica nela. Sem ela, o chão é o mais baixo das pontas − 800 cm. */
	void SetGroundHeight(TFunction<float(float XCm, float YCm)> InGround);

	/** O fio arrebentou (passou de 1,5× o comprimento): o ponto da quebra, na ponta do poste que cai. */
	FRTSWireEvent OnWireSnap;

	/** A ponta solta tocou o chão se mexendo (no máximo uma vez a cada ~0,2 s por vão). */
	FRTSWireEvent OnWireGround;

	/** Quantos vãos ainda se mexem (os que dormiram ficam desenhados e não contam). */
	int32 NumActiveSpans() const;

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	struct FSpan
	{
		TArray<FVector> P, Prev;      // os pontos (cm) e os do passo anterior (Verlet)
		float Rest = 0.0f;            // o comprimento de cada trecho (cm)
		float Total = 0.0f;           // o comprimento do fio inteiro (cm)
		float Radius = 1.0f;
		FVector EndPos[2];            // onde as pontas presas estão
		bool bPinned[2] = { true, true };
		int32 Follow = 1;
		float Still = 0.0f;           // há quanto tempo quase parado (s)
		float GroundCooldown = 0.0f;
		bool bAsleep = false;
		bool bSnapped = false;
		bool bDirty = false;          // mexeu desde o último desenho
		bool bMoved = false;          // o último desenho andou (o próximo quadro parado zera o movimento)
		TArray<FTransform> Drawn;     // os pedaços do último desenho (o quadro anterior do movimento)
		UInstancedStaticMeshComponent* Comp = nullptr;
		int32 FirstInstance = 0;
	};
	TArray<FSpan> Spans;
	TFunction<float(float, float)> Ground;
	float Accum = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Holder;
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> Comps;   // um componente por malha e material

	float GroundAt(const FSpan& S, float X, float Y) const;
	void Step(FSpan& S, int32 Handle, float Dt);
	void Draw(FSpan& S);
	UInstancedStaticMeshComponent* CompFor(UStaticMesh* Mesh, UMaterialInterface* Material);
};
