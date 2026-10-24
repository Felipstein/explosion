#pragma once

// O relevo da memória do chão (GroundRelief.usf): duas passadas de compute no retângulo que mudou. O jogo
// (RTSGroundMemory) chama do game thread; a passada vai pro render thread.

#include "CoreMinimal.h"

class UTexture2D;
class UTextureRenderTarget2D;

struct FRTSReliefParams
{
	float TexelCm = 9.4f;
	float RangeCm = 4.0f;
	float GrooveCm = 2.2f;
	float BermCm = 2.0f;
	float BermWidthCm = 9.0f;
	/** (16/10, a curva da v3) a folga do perfil: a faixa e a bordinha vêm gravadas ÷ isso (a curva afunda a faixa até
	 *  1,75× e dobra a bordinha de fora, e o caderno de 10 bits para em 1). 1 = sem folga (a v2). */
	float BandScale = 1.0f;
	float BermScale = 1.0f;
	/** (16/10, a v3) o halo revirado além da trilha (Prose 1985): só no A (o revolvido, que a MF usa no tom e na
	 *  rugosidade), de 0,5 a 0 até HaloCm. 0 = sem halo (a v2). */
	float HaloCm = 0.0f;
	FVector2f WorldMinCm = FVector2f(0.0f, 0.0f);
	FVector2f SunDir = FVector2f(1.0f, 0.0f);
	float SunTan = 0.577f;
	float PenumbraCm = 0.6f;
	/** (16/10) o tipo de chão (o surface.png; nulo = tudo areia) e o efeito de cada um, x areia, y terra, z pedra,
	 *  w asfalto; o molhado (o barro) × SurfWet. */
	UTexture2D* Surface = nullptr;
	FVector2f SurfaceMinCm = FVector2f(0.0f, 0.0f);
	float SurfaceSizeCm = 102400.0f;
	FVector4f SurfDepth = FVector4f(1.0f, 0.5f, 0.06f, 0.0f);
	FVector4f SurfBerm = FVector4f(1.0f, 0.6f, 0.0f, 0.0f);
	FVector4f SurfClaw = FVector4f(1.0f, 1.8f, 0.0f, 0.0f);
	FVector3f SurfWet = FVector3f(1.7f, 1.6f, 1.3f);
};

/** A luz do sol assada no chão (16/10, a v3: docs/CHAO_V3_ESTUDO.md §4), em GroundSunLit.usf. */
struct FRTSSunLitParams
{
	float RangeCm = 10.0f;
	FVector2f SunDir = FVector2f(1.0f, 0.0f);
	float SunTan = 0.14f;
	float PenumbraCm = 0.6f;
	float MaxLit = 1.2f;
	/** A grade fina: SS × SS amostras por texel do caderno de luz (2 no Alta, 4 no Ultra). */
	int32 Supersample = 2;
	/** O lado da janela do caderno (cm): o texel do caderno de luz sai daqui. */
	float SheetSizeCm = 19200.0f;
	/** A queda mais funda que faz sombra (a bacia mais a borda): a sombra chega até MaxDropCm ÷ tan(sol). É o contexto
	 *  que a varredura lê antes, do lado do sol (a cratera vizinha que faz sombra no que mudou). */
	float MaxDropCm = 100.0f;
	/** O quanto a altura mudou no que foi refeito (a trilha: ~20 cm; a cratera: tudo): a escrita cresce pro lado da
	 *  sombra só até WriteDropCm ÷ tan(sol). */
	float WriteDropCm = 100.0f;
	float AORadiusCm = 30.0f;
	/** (16/10, a v3) a garra: as barras da sapata atravessando a faixa, só na altura do sol (sem normal), de pico a pico
	 *  ClawCm, no passo ClawPitchCm, pelo andado da lagarta (o caderno Along: U ÷ AlongRangeCm); some com o frescor².
	 *  0 = sem garra. BandScale: a folga da faixa no perfil (a da curva). */
	float ClawCm = 0.0f;
	float ClawPitchCm = 19.39f;
	float AlongRangeCm = 12800.0f;
	float BandScale = 1.0f;
	/** O canto do caderno no mundo (o tipo de chão de cada ponto da garra). */
	FVector2f SheetMinCm = FVector2f(0.0f, 0.0f);
	/** (16/10) o tipo de chão (o surface.png; nulo = tudo areia) e o efeito de cada um, x areia, y terra, z pedra,
	 *  w asfalto; o molhado (o barro) × SurfWet. */
	UTexture2D* Surface = nullptr;
	FVector2f SurfaceMinCm = FVector2f(0.0f, 0.0f);
	float SurfaceSizeCm = 102400.0f;
	FVector4f SurfDepth = FVector4f(1.0f, 0.5f, 0.06f, 0.0f);
	FVector4f SurfBerm = FVector4f(1.0f, 0.6f, 0.0f, 0.0f);
	FVector4f SurfClaw = FVector4f(1.0f, 1.8f, 0.0f, 0.0f);
	FVector3f SurfWet = FVector3f(1.7f, 1.6f, 1.3f);
};

/** Uma marca de explosão carimbada de um atlas (13/10, Game/Tools/bake_craters.py): a cratera ou o avental da
 *  morte do tanque. O +X do carimbo vai no Axis (pra longe de quem atirou, ou a frente do casco). */
struct FRTSGroundMarkParams
{
	FVector2f CenterCm = FVector2f(0.0f, 0.0f);
	FVector2f Axis = FVector2f(1.0f, 0.0f);
	float Scale = 1.0f;
	/** A esticada no Axis (a mancha do tiro mirado). */
	float Stretch = 1.0f;
	float Mirror = 1.0f;
	float HeightMul = 1.0f;
	float DarkMul = 1.0f;
	float HeatMul = 1.0f;
	float LightMul = 1.0f;
	int32 Variant = 0;
	bool bWreck = false;
};

/** Um atlas: as variantes lado a lado (R altura com sinal, G escuro, B calor, A claro, 16 bits). */
struct FRTSGroundMarkAtlas
{
	UTexture2D* Texture = nullptr;
	/** (16/10) A altura que faz sombra do sol (<atlas>_sun.png, cinza de 16 bits, a faixa do R): a bacia e a borda, sem
	 *  os torrões; vai pro G do caderno de altura. Sem ela (a carcaça), o G não muda. */
	UTexture2D* SunTexture = nullptr;
	int32 Variants = 1;
	int32 Size = 320;
	float SpanCm = 1400.0f;
	float RangeCm = 80.0f;
	/** O G e o A em rampas de distância (bake_craters.py --ramps; o JSON diz "encoding": "ramp"). */
	bool bRamps = false;
};

namespace RTSGroundRelief
{
	/** Profile: o caderno do perfil (R faixa, G bordinha, B frescor); Crater: a altura das crateras (RG16F, cm: R a
	 *  cheia, G a que faz sombra do sol, sem os torrões);
	 *  Relief: a saída (precisa de UAV, R8G8B8A8). Os três na mesma resolução; Rect em texels do relevo. */
	RTSSHADERS_API void Compute(UTextureRenderTarget2D* Profile, UTextureRenderTarget2D* Crater, UTextureRenderTarget2D* Relief, const FRTSReliefParams& Params, const FIntRect& Rect);
	/** As marcas no caderno, na ordem dada: o GroundTrack (G escuro, B calor, A claro, pelo máximo) e a altura das
	 *  crateras (somada). Os dois cobrem a mesma janela do mundo (SheetMinCm, SheetSizeCm) e precisam de UAV. */
	/** A luz do sol assada (16/10): SunLit (R) e SkyAO (G) no caderno de luz (RG16F, com UAV), pela varredura no rumo
	 *  do sol numa grade fina. Relief (o R da esteira) e Crater (cm) dão a altura; Rect em texels do caderno de luz.
	 *  A região escrita cresce pro lado da sombra e a varredura começa antes, pro lado do sol, o bastante pra sombra
	 *  mais longa (MaxDropCm ÷ tan). Retângulos grandes vão em ladrilhos de até 512 texels. */
	RTSSHADERS_API void ComputeSunLit(UTextureRenderTarget2D* Relief, UTextureRenderTarget2D* Crater, UTextureRenderTarget2D* SunLit, const FRTSSunLitParams& Params, const FIntRect& Rect,
		UTextureRenderTarget2D* Profile = nullptr, UTextureRenderTarget2D* Along = nullptr);
	/** (16/10, a v3) as mips de Target (criado com bAutoGenerateMips) só no retângulo Rect da mip 0, até a mip MaxMips:
	 *  a média de 2 × 2, mip por mip (GroundMips.usf). */
	RTSSHADERS_API void BuildMips(UTextureRenderTarget2D* Target, const FIntRect& Rect, int32 MaxMips);
	/** (16/10) A batalha no minimapa (GroundBattle.usf): a camada pintada com alfa (Out, com UAV) a partir do caderno de
	 *  acúmulo do mapa inteiro (Battle). */
	RTSSHADERS_API void BattleOverlay(UTextureRenderTarget2D* Battle, UTextureRenderTarget2D* Out);
	RTSSHADERS_API void StampMarks(UTextureRenderTarget2D* Track, UTextureRenderTarget2D* Crater, const FVector2D& SheetMinCm, const FVector2D& SheetSizeCm,
		const FRTSGroundMarkAtlas& CraterAtlas, const FRTSGroundMarkAtlas& WreckAtlas, const TArray<FRTSGroundMarkParams>& Marks);
}
