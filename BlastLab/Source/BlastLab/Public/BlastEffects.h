#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlastRecipes.h"
#include "BlastEffects.generated.h"

class UDecalComponent;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/**
 * As camadas em volta de uma explosão gravada (docs/EXPLOSOES.md, fase 1),
 * pela receita da arma (Config/BlastRecipes.json):
 *  - clarão: bola branca aditiva de poucos quadros;
 *  - anel de poeira deitado no chão até o raio do dano;
 *  - destroços, torrões (da cor do chão) e faíscas: balísticos com arrasto
 *    quadrático, quicam no chão e ficam, com tamanho mínimo em pixels;
 *  - marca queimada com riscos radiais (decal).
 * O relógio é o do simulador (pausa e câmera lenta valem).
 */
UCLASS()
class BLASTLAB_API ABlastEffects : public AActor
{
	GENERATED_BODY()

public:
	ABlastEffects();

	/** Monta tudo no chão em Ground (cm), pela receita e pelo chão do ponto. */
	void Start(const FBlastRecipe& InRecipe, const FBlastSurface& InSurface, int32 InSeed);

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Flash;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Ring;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> DebrisMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> ClodMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> SparkMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDecalComponent> Scorch;

private:
	struct FPiece
	{
		FVector P = FVector::ZeroVector;      // m, mundo
		FVector V = FVector::ZeroVector;      // m/s
		FQuat Q = FQuat::Identity;
		FVector Spin = FVector::ZeroVector;   // eixo × rad/s
		FVector Size = FVector::OneVector;    // m
		float Life = 1.0e9f;                  // s (faíscas)
		float Drag = 0.0f;                    // g/v_t²
		float RestT = -1.0f;                  // desde quando está parado no chão
		bool bAlive = true;
	};

	void Emit(TArray<FPiece>& Out, int32 Count, const FBlastRange& Speed, const FBlastRange& Elev, const FBlastRange& Size,
		const FVector& SizeShape, float Up);
	void Integrate(TArray<FPiece>& Pieces, float Dt, float Restitution);
	void UpdateInstances(UInstancedStaticMeshComponent* Mesh, const TArray<FPiece>& Pieces, bool bStreak);
	float MinSizeM(const FVector& PM) const;

	FBlastRecipe Recipe;
	FBlastSurface Surface;
	FRandomStream Rand;
	FVector GroundM = FVector::ZeroVector;
	float T = 0.0f;
	float LastDt = 1.0f / 60.0f;
	TArray<FPiece> Debris, Clods, Sparks;
	FLinearColor ScorchParams = FLinearColor::Transparent;   // semente, opacidade, crescimento, fuligem (M_Scorch)
	bool bStarted = false;
};
