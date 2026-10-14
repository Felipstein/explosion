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
	FVector2f WorldMinCm = FVector2f(0.0f, 0.0f);
	FVector2f SunDir = FVector2f(1.0f, 0.0f);
	float SunTan = 0.577f;
	float PenumbraCm = 0.6f;
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
	int32 Variants = 1;
	int32 Size = 320;
	float SpanCm = 1400.0f;
	float RangeCm = 80.0f;
	/** O G e o A em rampas de distância (bake_craters.py --ramps; o JSON diz "encoding": "ramp"). */
	bool bRamps = false;
};

namespace RTSGroundRelief
{
	/** Profile: o caderno do perfil (R faixa, G bordinha, B frescor); Crater: a altura das crateras (R16F, cm);
	 *  Relief: a saída (precisa de UAV, R8G8B8A8). Os três na mesma resolução; Rect em texels do relevo. */
	RTSSHADERS_API void Compute(UTextureRenderTarget2D* Profile, UTextureRenderTarget2D* Crater, UTextureRenderTarget2D* Relief, const FRTSReliefParams& Params, const FIntRect& Rect);
	/** As marcas no caderno, na ordem dada: o GroundTrack (G escuro, B calor, A claro, pelo máximo) e a altura das
	 *  crateras (somada). Os dois cobrem a mesma janela do mundo (SheetMinCm, SheetSizeCm) e precisam de UAV. */
	RTSSHADERS_API void StampMarks(UTextureRenderTarget2D* Track, UTextureRenderTarget2D* Crater, const FVector2D& SheetMinCm, const FVector2D& SheetSizeCm,
		const FRTSGroundMarkAtlas& CraterAtlas, const FRTSGroundMarkAtlas& WreckAtlas, const TArray<FRTSGroundMarkParams>& Marks);
}
