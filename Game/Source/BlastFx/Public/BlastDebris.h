// Cópia do BlastLab (BlastDebris, 14/10/2026): os destroços da morte de tanque da v2 no jogo (o Felipe: "coloca
// rastro de fumaça também, é tudo que a gente homologou"). A original continua sendo a referência; aqui o relógio
// é o do jogo, o chão é o BlastFxGround e o tamanho, a cor e o brilho vêm do diretor (BlastFxDirector) em vez da
// linha de comando, e o rastro da torre segue o componente que o jogo arremessa.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlastDebris.generated.h"

class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class USceneComponent;

/**
 * Os destroços da morte de tanque (docs/CONCEITO_EXPLOSOES.md §4b, a "estrela
 * de faíscas e destroços" do vídeo do Veo, 0.05–3 s):
 *  - chapas pretas recortadas, girando, com sombra, que caem e ficam no chão;
 *  - faíscas-cometa: cabeça branca-amarela e rastro de fumaça clara que engorda
 *    e se desfaz;
 *  - rastros presos a quem voa (a torre arremessada: fumaça escura com fogo
 *    perto da peça).
 * Tudo em três malhas procedurais (uma chamada de desenho cada) no espaço do
 * mundo: chapas e cabeças de topologia fixa, só com os vértices atualizados; os
 * rastros refeitos a cada quadro, reamostrados em poucos pontos. As chapas com sombra (M_Base), as cabeças aditivas
 * (M_DebrisGlow) e os rastros em fita virada pra câmera, com perfil redondo e a
 * borda comida por Worley (M_TrailSmoke; Tools/build_debris.py).
 * O relógio é o do simulador (pausa e câmera lenta valem).
 */
UCLASS()
class BLASTFX_API ABlastDebris : public AActor
{
	GENERATED_BODY()

public:
	ABlastDebris();

	/** Arremessa as chapas e os cometas a partir de OriginCm (o alto do casco). */
	void Start(const FVector& OriginCm, int32 Seed);

	/**
	 * Antes do Start (o diretor do jogo): o tamanho da bola relativo à gravação (as faíscas encolhem e esquentam
	 * junto, 0,2–1; ≤ 0 = o -BlastScale), a cor da fuligem dos rastros e o brilho das cabeças e faíscas (1 = o do
	 * M_DebrisGlow). As opções do BlastLab (-BlastScale, -RmSoot, -DebrisGlow) continuam passando por cima.
	 */
	float BallScale = -1.0f;
	FLinearColor TrailTint = FLinearColor(0.95f, 0.92f, 0.88f, 1.0f);
	float GlowScale = 1.0f;

	/** Um rastro preso a um componente que o jogo move (a torre arremessada do RTSUnitActor): solta fumaça só
	 *  enquanto ele voa, e para de vez quando ele assenta. Devolve o índice. */
	int32 FollowComponent(USceneComponent* Comp, float Width0M, float GrowthMps, float LifeS, float Albedo, float GlowS, float Opacity0);

	/** Um rastro preso a algo que o simulador move (a torre). Devolve o índice. */
	int32 AddFollowTrail(float Width0M, float GrowthMps, float LifeS, float Albedo, float GlowS, float Opacity0);
	/** A cabeça do rastro agora (cm) e se ele ainda solta fumaça. */
	void SetTrailHead(int32 Trail, const FVector& HeadCm, bool bEmitting);

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> PlateMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> GlowMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> TrailMesh;

private:
	struct FTrailPoint
	{
		FVector P = FVector::ZeroVector;    // m, mundo
		float Age = 0.0f;
		float Dist = 0.0f;                  // m ao longo do rastro (a coordenada do ruído)
	};
	struct FTrail
	{
		TArray<FTrailPoint> Points;
		FVector Head = FVector::ZeroVector;
		bool bEmitting = true;
		float Width0 = 0.1f, Growth = 0.5f, Life = 2.5f, Albedo = 0.4f, GlowS = 0.0f, Opacity0 = 0.6f;
		float Seed = 0.0f, Age = 0.0f;
	};
	struct FPlate
	{
		FVector P = FVector::ZeroVector, V = FVector::ZeroVector;   // m, m/s
		FQuat Q = FQuat::Identity;
		FVector Spin = FVector::ZeroVector;                          // eixo · rad/s
		float Size = 0.5f;
		int32 Shape = 0;
		int32 Trail = -1;
		int32 Bounces = 0;
		bool bRest = false;
	};
	struct FComet
	{
		FVector P = FVector::ZeroVector, V = FVector::ZeroVector;
		float Burn = 1.5f, Age = 0.0f;
		int32 Trail = -1;
	};
	/** Faísca: espinho (rápido, curto, risco esticado) ou fagulha (lenta, cai piscando). */
	struct FSpark
	{
		FVector P = FVector::ZeroVector, V = FVector::ZeroVector;
		float Age = 0.0f, Life = 0.3f, Size = 0.08f, Bright = 1.0f, Drag = 0.0f, Lift = 0.0f, Seed = 0.0f;
		bool bEmber = false;
	};
	struct FShape
	{
		TArray<FVector> Verts;              // m, chapa de 1 m
		TArray<int32> Tris;
		TArray<FVector> Normals;
	};

	struct FFollow
	{
		int32 Trail = -1;
		TWeakObjectPtr<USceneComponent> Comp;
		FVector Last = FVector::ZeroVector;    // cm
		float Still = 0.0f;                    // s parado depois de ter voado
		bool bFlew = false, bDone = false;
	};
	TArray<FFollow> Follows;
	void UpdateFollows(float Dt);
	float SparkScaleNow() const;

	void BuildShapes();
	void Integrate(float Dt);
	void UpdatePlates();
	void UpdateGlows(const FVector& CamM);
	void UpdateTrails(const FVector& CamM);
	int32 NewTrail(float Width0M, float GrowthMps, float LifeS, float Albedo, float GlowS, float Opacity0);

	FRandomStream Rand;
	TArray<FShape> Shapes;
	TArray<FPlate> Plates;
	TArray<FComet> Comets;
	TArray<FSpark> Sparks;
	TArray<FTrail> Trails;
	float T = 0.0f;
	bool bPlateSection = false;
	int32 PlateVerts = 0;
	// a seção das cabeças já criada (com quantos vértices): depois só atualizar
	int32 GlowVerts = -1;
	// o que já acabou e não precisa mais ir pra placa (14/10, otimização sem mudar a imagem): as chapas deitadas, as
	// cabeças e faíscas apagadas, os rastros desfeitos. Com tudo acabado o ator para de vez até o fim da vida (120 s)
	bool bPlatesSettled = false, bGlowsDone = false, bTrailsDone = false;
};
