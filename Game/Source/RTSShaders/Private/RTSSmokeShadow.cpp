// A sombra das fumaças de puffs no mapa do sol (23/10): o registro dos puffs e a composição do mapa (RTSSmokeShadow.h; o
// estudo em docs/FUMACA_DO_MISSIL.md, "A sombra no mapa do sol", e docs/ESTUDO_LUZ_NA_FUMACA.md, a fase 2b).
#include "RTSSmokeShadow.h"
#include "RTSSmokeShadowPass.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "RHIStaticStates.h"
#include "ShaderParameterStruct.h"
#include "SystemTextures.h"

// o custo no perfil (stat gpu; -csvGpuStats: GPU/BlastRmTrailShadow), junto dos passes do raymarch
DECLARE_GPU_STAT_NAMED(BlastRmTrailShadow, TEXT("BlastRmTrailShadow"));

BEGIN_SHADER_PARAMETER_STRUCT(FRTSTrailShadowParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VolTau)
	SHADER_PARAMETER_SAMPLER(SamplerState, VolSampler)
	SHADER_PARAMETER(float, VolOn)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutTau)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, Puffs)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileStart)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileItems)
	SHADER_PARAMETER(FVector2f, SunXY)
	SHADER_PARAMETER(float, TexelM)
	SHADER_PARAMETER(float, InvWindowM)
	SHADER_PARAMETER(FUintVector2, OutSize)
	SHADER_PARAMETER(uint32, TilesX)
END_SHADER_PARAMETER_STRUCT()

class FRTSTrailShadowCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRTSTrailShadowCS);
	using FParameters = FRTSTrailShadowParameters;
	SHADER_USE_PARAMETER_STRUCT(FRTSTrailShadowCS, FGlobalShader);
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FRTSTrailShadowCS, "/RTSGame/Private/SmokeShadowSplat.usf", "MainTrailShadow", SF_Compute);

namespace RTSSmokeShadow
{
	namespace
	{
		// um Set que não veio nos últimos quadros vale como vazio (a partida acabou, o mundo trocou, o rastro parou de mandar)
		constexpr int64 StaleFrames = 3;
		// o ladrilho (texels por lado) e o teto de pares puff-ladrilho (nas 25 Patriots a conta dá ~55 mil)
		constexpr int32 TileTexels = 8;
		constexpr int32 MaxItems = 160000;
		// a menor espessura óptica que entra, por puff: a 0,002 (meio degrau de 8 bits na transmitância) as caudas cortadas de
		// 2–3 puffs vizinhos somavam até 0,27% do sol na borda de um ladrilho; a 0,001 ficam em 0,11% (scratchpad
		// trailshadow/verify_perpuff.py), com 7% de pares a mais
		constexpr float Eps = 0.001f;

		// o game thread
		int32 GGameNum = 0;
		int64 GGameFrame = -1000;
		bool GMapSeen = false;
		bool GMapOn = false;
		int64 GMapFrame = -1000;
		FVector GMapToSun = FVector(0.0, 0.0, 1.0);
		// o render thread
		TArray<FPuff> GRender;
		int64 GRenderFrame = -1000;
		// o registro a cada 5 s (render thread)
		double GLogAt = 0.0;
		int32 GLogCalls = 0;
		double GLogMs = 0.0;
		double GLogMaxMs = 0.0;
		int32 GLogMaxPuffs = 0;
		int32 GLogMaxShown = 0;
		int32 GLogMaxItems = 0;
		int32 GLogFull = 0;

		bool FreshOnRender()
		{
			return GRender.Num() > 0 && int64(GFrameCounterRenderThread) - GRenderFrame <= StaleFrames;
		}
	}

	void SetPuffs_GameThread(TArray<FPuff>&& Puffs)
	{
		check(IsInGameThread());
		GGameNum = Puffs.Num();
		GGameFrame = int64(GFrameCounter);
		ENQUEUE_RENDER_COMMAND(RTSSmokeShadowPuffs)([Moved = MoveTemp(Puffs), Frame = GGameFrame](FRHICommandListImmediate&) mutable
		{
			GRender = MoveTemp(Moved);
			GRenderFrame = Frame;
		});
	}

	int32 NumPuffs_GameThread()
	{
		return int64(GFrameCounter) - GGameFrame <= StaleFrames ? GGameNum : 0;
	}

	bool HasPuffs_RenderThread()
	{
		return FreshOnRender();
	}

	const TArray<FPuff>& Get_RenderThread()
	{
		static const TArray<FPuff> Empty;
		return FreshOnRender() ? GRender : Empty;
	}

	void SetMap_GameThread(bool bOn, const FVector& ToSun)
	{
		GMapSeen = true;
		GMapOn = bOn;
		GMapFrame = int64(GFrameCounter);
		GMapToSun = ToSun;
	}

	bool GetMap_GameThread(FVector& OutToSun)
	{
		if (!GMapSeen || int64(GFrameCounter) - GMapFrame > StaleFrames)
		{
			return false;
		}
		OutToSun = GMapToSun;
		return true;
	}

	void AddComposePass(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGTextureRef VolTau, FRDGTextureRef Out, const FMap& Map)
	{
		check(IsInRenderingThread());
		if (!Out || !ShaderMap)
		{
			return;
		}
		const uint64 C0 = FPlatformTime::Cycles64();
		const FIntPoint Size = Out->Desc.Extent;
		const float WindowM = FMath::Max(Map.WindowM, 1.0f);
		const float TexelM = WindowM / float(FMath::Max(Size.X, 1));
		const int32 TilesX = FMath::DivideAndRoundUp(Size.X, TileTexels);
		const int32 TilesY = FMath::DivideAndRoundUp(Size.Y, TileTexels);
		const int32 NumTiles = TilesX * TilesY;
		const float TileM = float(TileTexels) * TexelM;
		const float HalfDiag = 0.5f * UE_SQRT_2 * TileM;
		// a direção da light function (com o piso de elevação): o mesmo rumo e a mesma queda das sombras das explosões
		const FVector3f L = Map.ToSun.GetSafeNormal();
		const FVector2f Lxy(L.X, L.Y);
		const float SinE = L.Z;
		const float CosE = Lxy.Size();
		const FVector2f Az = CosE > 1e-4f ? Lxy / CosE : FVector2f(1.0f, 0.0f);
		const float TanA = FMath::Tan(FMath::Clamp(Map.SunHalfAngleRad, 0.0f, 0.05f));
		// o piso do σ: uma gaussiana amostrada a Δ dobra (alias) e^(−π²σ²/2Δ²) da energia; com σ ≥ Δ fica abaixo de 1%
		const float MinS2 = TexelM * TexelM;

		const TArray<FPuff>& Src = Get_RenderThread();
		TArray<FVector4f> Gpu;
		TArray<uint32> Start;
		Start.SetNumZeroed(NumTiles + 1);
		TArray<FUintVector2> Pairs;       // (o ladrilho, o puff)
		int32 Shown = 0;
		bool bFull = false;
		if (SinE > 0.05f && Src.Num() > 0)
		{
			Gpu.Reserve(Src.Num() * 3);
			Pairs.Reserve(Src.Num() * 8);
			for (const FPuff& P : Src)
			{
				const float Ce = FMath::Max(P.Cut, 0.0f);
				const float S = FMath::Max(P.SigmaM, 1e-3f);
				if (Ce >= 1.0f || P.TauC <= Eps)
				{
					continue;
				}
				// onde o raio do sol pelo centro corta o plano do mapa (m, relativo ao canto da janela; a precisão em double até aqui)
				const float T0 = float(P.PosM.Z - double(Map.GroundZM)) / SinE;
				const FVector2f Cp(float(P.PosM.X - Map.OriginM.X) - Lxy.X * T0, float(P.PosM.Y - Map.OriginM.Y) - Lxy.Y * T0);
				// do chão local até o centro, ao longo do raio: a parte do puff abaixo do chão não faz sombra (Φ, no passe), e a
				// penumbra cresce com a distância até o chão (o disco do sol de raio α a s metros: variância (s tan α)²/4 por eixo, a
				// geometria do PCSS, Fernando 2005); o pico cai na mesma razão (a massa se conserva)
				const float TG = float(P.PosM.Z - double(P.GroundZM)) / SinE;
				const float Pen = FMath::Max(TG, 0.0f) * TanA;
				const float Se2 = FMath::Max(S * S, MinS2) + 0.25f * Pen * Pen;
				const float Peak = P.TauC * S * S / Se2;
				if (Peak <= Eps)
				{
					continue;
				}
				// o alcance: o perfil cai a ε (R) e o corte de través zera (R⊥)
				const float R = FMath::Sqrt(2.0f * Se2 * FMath::Loge(Peak / Eps));
				const float RPerp = Ce > 0.0f ? FMath::Sqrt(2.0f * Se2 * FMath::Loge(1.0f / Ce)) : 1.0e9f;
				// o máximo de |u·l| dentro da elipse (|u| ≤ R / sen E no rumo do sol)
				const float Reach = R * CosE / SinE;
				float InvSigma = 1.0f / S;
				if (TG - Reach > 3.0f * S)
				{
					InvSigma = 0.0f;               // o puff inteiro acima do chão em todo o alcance: Φ = 1
				}
				else if (TG + Reach < -3.0f * S)
				{
					continue;                      // inteiro abaixo do chão
				}
				// o eixo de través: o rumo do rastro visto do sol (a_p = A − L (A·L)), com a rampa do material (|a_p| de 0,2 a 0,5);
				// b ⊥ L, então (perp·b)² = (u·b.xy)² e o perfil de través é d² − (u·b.xy)²
				FVector2f Bxy(0.0f, 0.0f);
				const FVector3f Ap = P.Axis - L * FVector3f::DotProduct(P.Axis, L);
				const float Al = Ap.Size();
				if (Al > 0.2f)
				{
					const float W = FMath::Clamp((Al - 0.2f) / 0.3f, 0.0f, 1.0f);
					Bxy = FVector2f(Ap.X, Ap.Y) * (FMath::Sqrt(W) / Al);
				}
				// a caixa da elipse (semieixos R / sen E no rumo do sol e R de lado), recortada pela janela
				const float Ra = R / SinE;
				const float Hx = FMath::Sqrt(Ra * Ra * Az.X * Az.X + R * R * Az.Y * Az.Y);
				const float Hy = FMath::Sqrt(Ra * Ra * Az.Y * Az.Y + R * R * Az.X * Az.X);
				if (Cp.X + Hx < 0.0f || Cp.Y + Hy < 0.0f || Cp.X - Hx > WindowM || Cp.Y - Hy > WindowM)
				{
					continue;
				}
				const int32 AX0 = FMath::Clamp(FMath::FloorToInt32((Cp.X - Hx) / TileM), 0, TilesX - 1);
				const int32 AX1 = FMath::Clamp(FMath::FloorToInt32((Cp.X + Hx) / TileM), 0, TilesX - 1);
				const int32 Y0 = FMath::Clamp(FMath::FloorToInt32((Cp.Y - Hy) / TileM), 0, TilesY - 1);
				const int32 Y1 = FMath::Clamp(FMath::FloorToInt32((Cp.Y + Hy) / TileM), 0, TilesY - 1);
				// cada ladrilho pelo centro c: √(cᵀQc) com Q = I − l lᵀ (e Q − b bᵀ) é uma seminorma de constante de Lipschitz ≤ 1
				// (os autovalores de Q ≤ 1), então ladrilho nenhum com alguma contribuição fica de fora com a folga h da meia
				// diagonal. Por linha de ladrilhos, cᵀQc ≤ (R + h)² é uma quadrática em x: só os ladrilhos dentro dela são visitados
				const float K = FMath::Square(R + HalfDiag);
				const float Qa = 1.0f - Lxy.X * Lxy.X;            // ≥ sen² E > 0
				const uint32 Item = uint32(Gpu.Num() / 3);
				bool bAny = false;
				for (int32 Ty = Y0; Ty <= Y1 && !bFull; ++Ty)
				{
					const float Yc = (float(Ty) + 0.5f) * TileM - Cp.Y;
					const float Qb = -2.0f * Yc * Lxy.X * Lxy.Y;
					const float Qc = Yc * Yc * (1.0f - Lxy.Y * Lxy.Y) - K;
					const float Disc = Qb * Qb - 4.0f * Qa * Qc;
					if (Disc < 0.0f)
					{
						continue;
					}
					const float Sq = FMath::Sqrt(Disc);
					const float XLo = Cp.X + (-Qb - Sq) / (2.0f * Qa);
					const float XHi = Cp.X + (-Qb + Sq) / (2.0f * Qa);
					// os centros (Tx + ½) TileM dentro de [XLo, XHi], com um ladrilho de folga pro arredondamento (o teste exato abaixo),
					// e dentro das colunas da caixa da elipse (a folga h da seminorma passaria dela à toa)
					const int32 X0 = FMath::Clamp(FMath::FloorToInt32(XLo / TileM - 0.5f), AX0, AX1);
					const int32 X1 = FMath::Clamp(FMath::CeilToInt32(XHi / TileM - 0.5f), AX0, AX1);
					for (int32 Tx = X0; Tx <= X1; ++Tx)
					{
						const FVector2f C = FVector2f((float(Tx) + 0.5f) * TileM, Yc + Cp.Y) - Cp;
						const float Cl = FVector2f::DotProduct(C, Lxy);
						const float Qd = FVector2f::DotProduct(C, C) - Cl * Cl;
						if (Qd > K)
						{
							continue;
						}
						if (Ce > 0.0f)
						{
							const float Cb = FVector2f::DotProduct(C, Bxy);
							if (Qd - Cb * Cb > FMath::Square(RPerp + HalfDiag))
							{
								continue;
							}
						}
						if (Pairs.Num() >= MaxItems)
						{
							bFull = true;
							break;
						}
						Pairs.Add(FUintVector2(uint32(Ty * TilesX + Tx), Item));
						bAny = true;
					}
				}
				if (bAny)
				{
					Gpu.Add(FVector4f(Cp.X, Cp.Y, TG, InvSigma));
					Gpu.Add(FVector4f(0.5f / Se2, Peak, Ce, 0.0f));
					Gpu.Add(FVector4f(Bxy.X, Bxy.Y, 0.0f, 0.0f));
					++Shown;
				}
				if (bFull)
				{
					break;
				}
			}
		}
		// a lista por ladrilho (a ordenação por contagem: o começo de cada um e os puffs em sequência)
		for (const FUintVector2& Pr : Pairs)
		{
			++Start[Pr.X + 1];
		}
		for (int32 T = 1; T <= NumTiles; ++T)
		{
			Start[T] += Start[T - 1];
		}
		TArray<uint32> Items;
		Items.SetNumUninitialized(Pairs.Num());
		{
			TArray<uint32> Cursor(Start.GetData(), NumTiles);
			for (const FUintVector2& Pr : Pairs)
			{
				Items[Cursor[Pr.X]++] = Pr.Y;
			}
		}
		const int32 NumItems = Items.Num();
		const double Ms = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - C0);

		FRTSTrailShadowParameters* Pp = GraphBuilder.AllocParameters<FRTSTrailShadowParameters>();
		Pp->VolTau = VolTau ? VolTau : GSystemTextures.GetBlackDummy(GraphBuilder);
		Pp->VolSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Pp->VolOn = VolTau ? 1.0f : 0.0f;
		Pp->OutTau = GraphBuilder.CreateUAV(Out);
		Pp->Puffs = GraphBuilder.CreateSRV(CreateStructuredBuffer(GraphBuilder, TEXT("RTSSmokeShadow.Puffs"), MoveTemp(Gpu)));
		Pp->TileStart = GraphBuilder.CreateSRV(CreateStructuredBuffer(GraphBuilder, TEXT("RTSSmokeShadow.TileStart"), MoveTemp(Start)));
		Pp->TileItems = GraphBuilder.CreateSRV(CreateStructuredBuffer(GraphBuilder, TEXT("RTSSmokeShadow.TileItems"), MoveTemp(Items)));
		Pp->SunXY = Lxy;
		Pp->TexelM = TexelM;
		Pp->InvWindowM = 1.0f / WindowM;
		Pp->OutSize = FUintVector2(uint32(Size.X), uint32(Size.Y));
		Pp->TilesX = uint32(TilesX);
		{
			RDG_EVENT_SCOPE_STAT(GraphBuilder, BlastRmTrailShadow, "BlastRmTrailShadow");
			TShaderMapRef<FRTSTrailShadowCS> Shader(ShaderMap);
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RTSSmokeShadow Compose %d puffs %d pares", Shown, NumItems),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, Shader, Pp, FIntVector(TilesX, TilesY, 1));
		}

		// o registro: quantos puffs, quantos na janela, os pares e o tempo do agrupamento (render thread)
		++GLogCalls;
		GLogMs += Ms;
		GLogMaxMs = FMath::Max(GLogMaxMs, Ms);
		GLogMaxPuffs = FMath::Max(GLogMaxPuffs, Src.Num());
		GLogMaxShown = FMath::Max(GLogMaxShown, Shown);
		GLogMaxItems = FMath::Max(GLogMaxItems, NumItems);
		GLogFull += bFull ? 1 : 0;
		const double Now = FPlatformTime::Seconds();
		if (Now - GLogAt >= 5.0 && GLogMaxPuffs > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("RTS sombra das fumaças: até %d puffs do rastro (%d na janela), até %d pares puff-ladrilho, agrupamento %.3f ms em média (máx %.3f) em %d quadros; mapa %d², o sol a %.0f° (penumbra α %.2f°)%s"),
				GLogMaxPuffs, GLogMaxShown, GLogMaxItems, GLogMs / FMath::Max(GLogCalls, 1), GLogMaxMs, GLogCalls, Size.X,
				FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(SinE, -1.0f, 1.0f))), FMath::RadiansToDegrees(Map.SunHalfAngleRad),
				GLogFull > 0 ? *FString::Printf(TEXT("; o teto de %d pares estourou em %d quadros"), MaxItems, GLogFull) : TEXT(""));
		}
		if (Now - GLogAt >= 5.0)
		{
			GLogAt = Now;
			GLogCalls = 0;
			GLogMs = 0.0;
			GLogMaxMs = 0.0;
			GLogMaxPuffs = 0;
			GLogMaxShown = 0;
			GLogMaxItems = 0;
			GLogFull = 0;
		}
	}
}
