#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"

class FRDGBuilder;
class FRHIGPUBufferReadback;
class FTextureResource;
class FTextureRenderTargetResource;

/**
 * A fumaça que FICA no campo: o fluido de reference/webgl/js/battlesmoke.js
 * em compute shaders (Shaders/Private/BattleSmoke.usf). Mora no render
 * thread; o jogo monta um FBattleSmokeStepInputs por quadro e manda por valor.
 *
 * Domínio de 96 × 96 × 64 m centrado na origem (como o WebGL): fumaça numa
 * grade de 1 m, velocidade/pressão numa de 2 m (MAC). Ver o .usf.
 */
namespace BattleSmoke
{
	// grades (battlesmoke.js: BoxGrid([96, 64, 96], 1) e ([48, 32, 48], 2), y pra cima lá)
	inline const FIntVector FineDim(96, 96, 64);
	inline const FIntVector VelDim(48, 48, 32);
	constexpr float FineCellM = 1.0f;
	constexpr float VelCellM = 2.0f;
	inline const FVector3f DomainMinM(-48.0f, -48.0f, 0.0f);
	constexpr int32 MaxDisturbers = 8;
	constexpr int32 MaxBlasts = 4;
	constexpr int32 MaxWakes = 2;
	// deformação das explosões gravadas: a grade de D cobre o domínio gravado
	// (64³: 0.59 m locais na Carga Pesada, abaixo do 1 m da fumaça do campo)
	inline const FIntVector WarpDim(64, 64, 64);
	constexpr int32 MaxWarps = 4;
	// |D| máximo, m LOCAIS (na escala da Carga Pesada; cresce com a explosão):
	// além disso o ponto já cai no ar limpo em volta da nuvem (raio de ~10–15 m
	// na Carga Pesada), e um mapa esticado demais dobra sobre si. Em m do mundo
	// ele limitava uma nuvem de ataque aéreo (80 m) a 10% de deformação.
	constexpr float WarpMaxLocalM = 8.0f;
}

/**
 * Uma explosão gravada sendo deformada pelo que passa (BattleSmoke.usf
 * MainWarp): o estado persistente fica no slot; a saída vai pro material.
 */
struct FBattleSmokeWarp
{
	int32 Slot = 0;
	bool bReset = false;                            // explosão nova no slot: D e o recorte começam em zero
	FVector3f FootM = FVector3f::ZeroVector;        // pé da explosão no mundo
	float Scale = 1.0f;
	float DomainLocalM = 38.0f;                     // aresta do domínio gravado (m locais)
	int32 Frame0 = 0;                               // quadros gravados em volta do instante e a fração
	int32 Frame1 = 0;
	float Alpha = 0.0f;
	int32 VelocityResolution = 32;
	int32 VelocityFrames = 1;
	FTextureResource* Velocity = nullptr;           // ExplosionBake Velocity (quadros empilhados em z)
	FTextureRenderTargetResource* Out = nullptr;    // pro material: D (m locais), fração recortada
};

/** O que atravessa o campo neste quadro (movers.js → battlesmoke.js disturb). */
struct FBattleSmokeDisturber
{
	FVector4f A;   // posição atual (m).xyz, raio
	FVector4f B;   // posição anterior.xyz, tipo (1 corpo · 2 projétil · 4 esteira · 5 fumaça de motor · 6 jato · 7 fogo contínuo)
	FVector4f C;   // velocidade.xyz (m/s), intensidade · no fogo: x = calor (0–1), w = fuligem por segundo
	FVector4f D;   // "direita" da asa.xyz, envergadura b0 · no jato: x = velocidade de saída, w = bocal
};

struct FBattleSmokeBlast
{
	FVector4f PosRadius;   // m
	float Source = 0.0f;   // 1/s
};

/** A esteira de um avião: o par de vórtices que fica no caminho dele (BattleSmoke.usf WakeVelocity). */
struct FBattleSmokeWake
{
	FVector4f A;   // início do caminho (m).xyz, instante em que o avião passou ali (relógio do campo)
	FVector4f B;   // direção do voo.xyz, velocidade (m/s)
	FVector4f C;   // "direita" da asa.xyz, envergadura efetiva b0 (m)
	FVector4f D;   // circulação Γ0 (m²/s), descida (m/s), núcleo (m), comprimento já voado (m)
};

struct FBattleSmokeStepInputs
{
	float Dt = 0.0f;
	float Time = 0.0f;
	bool bReset = false;
	TArray<FBattleSmokeWarp> Warps;
	TArray<FBattleSmokeDisturber> Disturbers;
	TArray<FBattleSmokeBlast> Blasts;
	TArray<FBattleSmokeWake> Wakes;
	FTextureResource* Noise = nullptr;                // ruído 3D ladrilhável (T_CurlNoise)
	FTextureRenderTargetResource* FieldOut = nullptr;  // pro material: fumaça (fina)
	FTextureRenderTargetResource* VelOut = nullptr;    // pro material: velocidade no centro (grossa)
};

/** Uma explosão gravada fazendo sombra no chão (BattleSmoke.usf MainShadow). */
struct FSmokeShadowCaster
{
	FVector3f FootM = FVector3f::ZeroVector;
	float Scale = 1.0f;
	float Opacity = 1.0f;
	float DomainLocalM = 38.0f;
	int32 Frame0 = 0;
	int32 Frame1 = 0;
	float Alpha = 0.0f;
	int32 VelocityResolution = 32;
	int32 VelocityFrames = 1;
	FTextureResource* Velocity = nullptr;           // ExplosionBake Velocity: a extinção no alfa
};

/**
 * O mapa de sombra da fumaça: espessura óptica entre um plano horizontal e a
 * luz principal, vista de cima, numa janela que segue a câmera. A light
 * function do sol e da lua (M_SmokeShadowLF) lê.
 */
struct FSmokeShadowInputs
{
	TArray<FSmokeShadowCaster> Casters;
	FVector2f OriginM = FVector2f::ZeroVector;      // canto do mapa no mundo
	float TexelM = 0.5f;
	float GroundZM = 0.0f;
	FVector3f LightDir = FVector3f::UnitZ();        // pra a luz
	int32 Res = 512;
	FTextureRenderTargetResource* Out = nullptr;    // R32F, com UAV
};

class BLASTLABSHADERS_API FBattleSmokeSim
{
public:
	FBattleSmokeSim();
	~FBattleSmokeSim();

	/** Um passo do fluido (render thread). */
	void Step(FRDGBuilder& GraphBuilder, const FBattleSmokeStepInputs& In);

	/** O mapa de sombra da fumaça das explosões (render thread, todo quadro com explosão). */
	void RenderShadow(FRDGBuilder& GraphBuilder, const FSmokeShadowInputs& In);

	/** Caixa ocupada (células finas, inclusiva), lida da GPU com alguns quadros de atraso. */
	bool GetBounds(FIntVector& OutMin, FIntVector& OutMax) const;

	/**
	 * Altura média da fuligem no campo (células finas desde o chão, pesada pela
	 * densidade), lida junto com a caixa: é por onde o avião e o míssil passam
	 * (battlesmoke.js smokeHeightAt). Falso se não há fuligem.
	 */
	bool GetSootHeight(float& OutCells) const;

private:
	void PollBounds();

	TRefCountPtr<IPooledRenderTarget> Vel, Fld, Press;
	TRefCountPtr<IPooledRenderTarget> WarpState[BattleSmoke::MaxWarps];
	// min.xyz, max.xyz da caixa, Σ fuligem e Σ fuligem·altura (BattleSmoke.usf MainCopyField)
	static constexpr int32 NumBoundsWords = 8;
	static constexpr int32 NumReadbacks = 3;
	FRHIGPUBufferReadback* Readbacks[NumReadbacks] = {};
	int32 ReadbackWrite = 0;
	int32 ReadbackPending = 0;
	mutable FCriticalSection BoundsLock;
	FIntVector BoundsMin = FIntVector(0), BoundsMax = FIntVector(-1);
	bool bBoundsValid = false;
	double SootHeight = -1.0;
};
