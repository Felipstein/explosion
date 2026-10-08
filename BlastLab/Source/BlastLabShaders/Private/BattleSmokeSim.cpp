#include "BattleSmokeSim.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RHIGPUReadback.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"

// tempos de GPU (blast.ps1 view -ProfileGpu → Tools/gpu_profile.py --live): o fluido do campo e a deformação das explosões
DECLARE_GPU_STAT_NAMED(BattleSmokeField, TEXT("BattleSmoke"));
DECLARE_GPU_STAT_NAMED(BattleSmokeWarp, TEXT("BattleSmokeWarp"));
DECLARE_GPU_STAT_NAMED(BattleSmokeShadow, TEXT("BattleSmokeShadow"));

namespace
{
	// reference/webgl/js/battlesmoke.js BATTLE_DEFAULTS (os comentários de lá
	// explicam cada um: medidos contra a gravação e contra Pasquill–Gifford)
	constexpr int32 PressureIters = 24;
	constexpr float Buoyancy = 13.0f;      // m/s² por unidade de T
	constexpr float SootWeight = 0.6f;
	constexpr float CoolTau = 5.0f;        // s
	constexpr float SootTau = 10.0f;       // s: densa até ~10 s, fiapos até ~15 s (decisão de jogo)
	constexpr float RiseTau = 1.2f;        // s: a subida da pluma desacelera (Morton–Taylor–Turner)
	constexpr float VelTau = 15.0f;        // s
	constexpr float Turb = 3.0f;           // m/s²
	constexpr float TurbScale = 0.025f;    // 1/m
	constexpr float TurbRate = 0.03f;      // 1/s
	constexpr float TurbV = 0.5f;          // σ_w ≈ 0.5·σ_u
	constexpr float Vorticity = 0.5f;      // ε do confinamento
	constexpr int32 FieldSubsteps = 2;     // transporte estável com |u|·Δt/h < ⅓ por eixo
	constexpr float BoundsThreshold = 2e-4f;

	// Deformação das explosões gravadas (MainWarp): o corte que o corpo e o jato
	// abrem. Na física uma fatia de ~1 m (a asa) se fecha por mistura em ~0.3 s
	// (ν_t ≈ 2 m²/s) e de cima mal se via. Decisão de jogo (Felipe, 06/10): como
	// os buracos na fumaça do CS2, o miolo do corte fica aberto 1.5 s e fecha no
	// último segundo; a borda, que nasce com menos tempo, fecha antes (o buraco
	// encolhe das bordas pro meio). Em s do mundo: o corte é do tamanho do avião,
	// não da explosão.
	constexpr float CarveLife = 2.5f;      // s
	constexpr float CarveClose = 1.0f;     // s
}

BEGIN_SHADER_PARAMETER_STRUCT(FBattleSmokeParameters, )
	SHADER_PARAMETER(FIntVector3, FineDim)
	SHADER_PARAMETER(FIntVector3, VelDim)
	SHADER_PARAMETER(float, FineCell)
	SHADER_PARAMETER(float, VelCell)
	SHADER_PARAMETER(FVector3f, DomainMin)
	SHADER_PARAMETER(float, Dt)
	SHADER_PARAMETER(float, Time)
	SHADER_PARAMETER(float, Buoyancy)
	SHADER_PARAMETER(float, SootWeight)
	SHADER_PARAMETER(float, Turb)
	SHADER_PARAMETER(float, TurbScale)
	SHADER_PARAMETER(float, TurbRate)
	SHADER_PARAMETER(float, TurbV)
	SHADER_PARAMETER(float, VelKeep)
	SHADER_PARAMETER(float, RiseKeep)
	SHADER_PARAMETER(float, UMax)
	SHADER_PARAMETER(float, Vorticity)
	SHADER_PARAMETER(FVector4f, Keep)
	SHADER_PARAMETER(int32, DisturbCount)
	SHADER_PARAMETER_ARRAY(FVector4f, DA, [BattleSmoke::MaxDisturbers])
	SHADER_PARAMETER_ARRAY(FVector4f, DB, [BattleSmoke::MaxDisturbers])
	SHADER_PARAMETER_ARRAY(FVector4f, DC, [BattleSmoke::MaxDisturbers])
	SHADER_PARAMETER_ARRAY(FVector4f, DD, [BattleSmoke::MaxDisturbers])
	SHADER_PARAMETER(int32, BlastCount)
	SHADER_PARAMETER_ARRAY(FVector4f, Blast, [BattleSmoke::MaxBlasts])
	SHADER_PARAMETER(FVector4f, BlastSrc)
	SHADER_PARAMETER(FVector4f, WarpFoot)
	SHADER_PARAMETER(FVector4f, WarpFrames)
	SHADER_PARAMETER(float, WarpAtlasDepth)
	SHADER_PARAMETER(float, WarpDomain)
	SHADER_PARAMETER(FIntVector3, WarpDim)
	SHADER_PARAMETER(float, WarpCarveLife)
	SHADER_PARAMETER(float, WarpCarveClose)
	SHADER_PARAMETER(int32, WarpDebug)
	SHADER_PARAMETER(float, WarpMax)
	SHADER_PARAMETER(FVector4f, ShadowMap)
	SHADER_PARAMETER(FVector4f, ShadowLight)
	SHADER_PARAMETER(float, ShadowOpacity)
	SHADER_PARAMETER(int32, ShadowRes)
	SHADER_PARAMETER(int32, WakeCount)
	SHADER_PARAMETER_ARRAY(FVector4f, WakeA, [BattleSmoke::MaxWakes])
	SHADER_PARAMETER_ARRAY(FVector4f, WakeB, [BattleSmoke::MaxWakes])
	SHADER_PARAMETER_ARRAY(FVector4f, WakeC, [BattleSmoke::MaxWakes])
	SHADER_PARAMETER_ARRAY(FVector4f, WakeD, [BattleSmoke::MaxWakes])
	SHADER_PARAMETER(float, BoundsThreshold)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, VelIn)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, FldIn)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, CurlIn)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, PressIn)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, DivIn)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, Noise)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, WarpIn)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, BakeVel)
	SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
	SHADER_PARAMETER_SAMPLER(SamplerState, LinearWrap)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, VelOut)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, FldOut)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, CurlOut)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, PressOut)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, DivOut)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, OutTex)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, WarpOut)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, Bounds)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, ShadowOut)
END_SHADER_PARAMETER_STRUCT()

#define BATTLE_SMOKE_SHADER(Name, Entry)                                                              \
	class Name : public FGlobalShader                                                                 \
	{                                                                                                 \
	public:                                                                                           \
		DECLARE_GLOBAL_SHADER(Name);                                                                  \
		using FParameters = FBattleSmokeParameters;                                                   \
		SHADER_USE_PARAMETER_STRUCT(Name, FGlobalShader);                                             \
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)    \
		{                                                                                             \
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);              \
		}                                                                                             \
	};                                                                                                \
	IMPLEMENT_GLOBAL_SHADER(Name, "/BlastLab/Private/BattleSmoke.usf", Entry, SF_Compute);

BATTLE_SMOKE_SHADER(FBattleAdvectVelCS, "MainAdvectVel")
BATTLE_SMOKE_SHADER(FBattleCurlCS, "MainCurl")
BATTLE_SMOKE_SHADER(FBattleForcesCS, "MainForces")
BATTLE_SMOKE_SHADER(FBattleDivCS, "MainDiv")
BATTLE_SMOKE_SHADER(FBattlePressureCS, "MainPressure")
BATTLE_SMOKE_SHADER(FBattleProjectCS, "MainProject")
BATTLE_SMOKE_SHADER(FBattleAdvectFieldCS, "MainAdvectField")
BATTLE_SMOKE_SHADER(FBattleFieldsFxCS, "MainFieldsFx")
BATTLE_SMOKE_SHADER(FBattleWarpCS, "MainWarp")
BATTLE_SMOKE_SHADER(FBattleWarpRenderCS, "MainWarpRender")
BATTLE_SMOKE_SHADER(FBattleCopyFieldCS, "MainCopyField")
BATTLE_SMOKE_SHADER(FBattleCopyVelCS, "MainCopyVel")
BATTLE_SMOKE_SHADER(FBattleResetBoundsCS, "MainResetBounds")
BATTLE_SMOKE_SHADER(FBattleShadowClearCS, "MainShadowClear")
BATTLE_SMOKE_SHADER(FBattleShadowCS, "MainShadow")

namespace
{
	FRDGTextureRef NewVolume(FRDGBuilder& GraphBuilder, const FIntVector& Dim, EPixelFormat Format, const TCHAR* Name)
	{
		return GraphBuilder.CreateTexture(FRDGTextureDesc::Create3D(Dim, Format, FClearValueBinding::Black,
			TexCreate_ShaderResource | TexCreate_UAV), Name);
	}

	// textura persistente entre quadros: a do quadro anterior, ou uma nova zerada
	FRDGTextureRef Persistent(FRDGBuilder& GraphBuilder, TRefCountPtr<IPooledRenderTarget>& Pooled, bool bReset,
		const FIntVector& Dim, EPixelFormat Format, const TCHAR* Name)
	{
		if (Pooled.IsValid() && !bReset)
		{
			return GraphBuilder.RegisterExternalTexture(Pooled, Name);
		}
		FRDGTextureRef T = NewVolume(GraphBuilder, Dim, Format, Name);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(T), FVector4f(0.0f, 0.0f, 0.0f, 0.0f));
		return T;
	}

	FRDGTextureRef External(FRDGBuilder& GraphBuilder, FRHITexture* Texture, const TCHAR* Name)
	{
		return GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Texture, Name));
	}

	template <typename TShader>
	void Dispatch(FRDGBuilder& GraphBuilder, const TCHAR* Name, FBattleSmokeParameters* P, const FIntVector& Dim)
	{
		TShaderMapRef<TShader> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("BattleSmoke %s", Name), Shader, P,
			FComputeShaderUtils::GetGroupCount(Dim, FIntVector(4)));
	}
}

FBattleSmokeSim::FBattleSmokeSim() = default;

void FBattleSmokeSim::RenderShadow(FRDGBuilder& GraphBuilder, const FSmokeShadowInputs& In)
{
	if (!In.Out || In.Res <= 0)
	{
		return;
	}
	RDG_EVENT_SCOPE_STAT(GraphBuilder, BattleSmokeShadow, "SmokeShadow");
	FRDGTextureRef Out = External(GraphBuilder, In.Out->GetRenderTargetTexture(), TEXT("BattleSmoke.Shadow"));
	FRDGTextureUAVRef OutUAV = GraphBuilder.CreateUAV(Out);
	const FIntVector Groups = FComputeShaderUtils::GetGroupCount(FIntVector(In.Res, In.Res, 1), FIntVector(8, 8, 1));
	auto Common = [&](FBattleSmokeParameters* P)
	{
		P->ShadowMap = FVector4f(In.OriginM.X, In.OriginM.Y, In.TexelM, In.GroundZM);
		P->ShadowLight = FVector4f(In.LightDir, 0.0f);
		P->ShadowRes = In.Res;
		P->ShadowOut = OutUAV;
		P->LinearClamp = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	};
	{
		FBattleSmokeParameters* P = GraphBuilder.AllocParameters<FBattleSmokeParameters>();
		Common(P);
		TShaderMapRef<FBattleShadowClearCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("BattleSmoke ShadowClear"), Shader, P, Groups);
	}
	for (const FSmokeShadowCaster& C : In.Casters)
	{
		if (!C.Velocity || !C.Velocity->TextureRHI)
		{
			continue;
		}
		FBattleSmokeParameters* P = GraphBuilder.AllocParameters<FBattleSmokeParameters>();
		Common(P);
		const float R = float(C.VelocityResolution);
		P->WarpFoot = FVector4f(C.FootM, C.Scale);
		P->WarpFrames = FVector4f(C.Frame0 * R, C.Frame1 * R, C.Alpha, R);
		P->WarpAtlasDepth = R * C.VelocityFrames;
		P->WarpDomain = C.DomainLocalM;
		P->ShadowOpacity = C.Opacity;
		P->BakeVel = External(GraphBuilder, C.Velocity->TextureRHI, TEXT("BattleSmoke.BakeVelocity"));
		TShaderMapRef<FBattleShadowCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("BattleSmoke Shadow"), Shader, P, Groups);
	}
}

FBattleSmokeSim::~FBattleSmokeSim()
{
	for (FRHIGPUBufferReadback*& R : Readbacks)
	{
		delete R;
		R = nullptr;
	}
}

void FBattleSmokeSim::PollBounds()
{
	// a leitura mais antiga que já chegou (o anel tem NumReadbacks quadros de atraso no máximo)
	while (ReadbackPending > 0)
	{
		const int32 Index = (ReadbackWrite - ReadbackPending + NumReadbacks) % NumReadbacks;
		FRHIGPUBufferReadback* R = Readbacks[Index];
		if (!R || !R->IsReady())
		{
			break;
		}
		const uint32* Data = static_cast<const uint32*>(R->Lock(NumBoundsWords * sizeof(uint32)));
		{
			FScopeLock Lock(&BoundsLock);
			bBoundsValid = Data[3] >= Data[0] && Data[4] >= Data[1] && Data[5] >= Data[2] && Data[0] != 0xffffffffu;
			BoundsMin = FIntVector(Data[0], Data[1], Data[2]);
			BoundsMax = FIntVector(Data[3], Data[4], Data[5]);
			// altura média da fuligem, em células (o centro da célula k está em k + ½)
			SootHeight = Data[6] > 0 ? double(Data[7]) / double(Data[6]) + 0.5 : -1.0;
		}
		R->Unlock();
		--ReadbackPending;
	}
}

bool FBattleSmokeSim::GetBounds(FIntVector& OutMin, FIntVector& OutMax) const
{
	FScopeLock Lock(&BoundsLock);
	OutMin = BoundsMin;
	OutMax = BoundsMax;
	return bBoundsValid;
}

bool FBattleSmokeSim::GetSootHeight(float& OutCells) const
{
	FScopeLock Lock(&BoundsLock);
	OutCells = float(SootHeight);
	return bBoundsValid && SootHeight >= 0.0;
}

void FBattleSmokeSim::Step(FRDGBuilder& GraphBuilder, const FBattleSmokeStepInputs& In)
{
	using namespace BattleSmoke;
	PollBounds();
	if (!In.Noise || !In.Noise->TextureRHI || !In.FieldOut || !In.VelOut)
	{
		return;
	}
	RDG_EVENT_SCOPE(GraphBuilder, "BattleSmoke");
	const float Dt = FMath::Min(In.Dt, 1.0f / 30.0f);

	FRDGTextureRef VelT = Persistent(GraphBuilder, Vel, In.bReset, VelDim, PF_FloatRGBA, TEXT("BattleSmoke.Vel"));
	FRDGTextureRef FldT = Persistent(GraphBuilder, Fld, In.bReset, FineDim, PF_A32B32G32R32F, TEXT("BattleSmoke.Field"));
	FRDGTextureRef PressT = Persistent(GraphBuilder, Press, In.bReset, VelDim, PF_A32B32G32R32F, TEXT("BattleSmoke.Pressure"));
	FRDGTextureRef NoiseT = External(GraphBuilder, In.Noise->TextureRHI, TEXT("BattleSmoke.Noise"));

	// parâmetros comuns a todos os passes
	auto Base = [&](FBattleSmokeParameters* P)
	{
		P->FineDim = FIntVector3(FineDim.X, FineDim.Y, FineDim.Z);
		P->VelDim = FIntVector3(VelDim.X, VelDim.Y, VelDim.Z);
		P->FineCell = FineCellM;
		P->VelCell = VelCellM;
		P->DomainMin = DomainMinM;
		P->Dt = Dt;
		P->Time = In.Time;
		P->Buoyancy = Buoyancy;
		P->SootWeight = SootWeight;
		P->Turb = Turb;
		P->TurbScale = TurbScale;
		P->TurbRate = TurbRate;
		P->TurbV = TurbV;
		P->VelKeep = FMath::Exp(-Dt / VelTau);
		P->RiseKeep = FMath::Exp(-Dt / RiseTau);
		// estabilidade do transporte da fumaça na grade fina: até 0.67·h/Δt
		P->UMax = 0.95f * (float(FieldSubsteps) / 3.0f) * FineCellM / FMath::Max(Dt, 1e-4f);
		P->Vorticity = Vorticity;
		P->Keep = FVector4f(1.0f);
		const int32 ND = FMath::Min(In.Disturbers.Num(), MaxDisturbers);
		P->DisturbCount = ND;
		for (int32 i = 0; i < ND; ++i)
		{
			P->DA[i] = In.Disturbers[i].A;
			P->DB[i] = In.Disturbers[i].B;
			P->DC[i] = In.Disturbers[i].C;
			P->DD[i] = In.Disturbers[i].D;
		}
		const int32 NB = FMath::Min(In.Blasts.Num(), MaxBlasts);
		P->BlastCount = NB;
		float Src[4] = { 0, 0, 0, 0 };
		for (int32 i = 0; i < NB; ++i)
		{
			P->Blast[i] = In.Blasts[i].PosRadius;
			Src[i] = In.Blasts[i].Source;
		}
		P->BlastSrc = FVector4f(Src[0], Src[1], Src[2], Src[3]);
		const int32 NW = FMath::Min(In.Wakes.Num(), MaxWakes);
		P->WakeCount = NW;
		for (int32 i = 0; i < NW; ++i)
		{
			P->WakeA[i] = In.Wakes[i].A;
			P->WakeB[i] = In.Wakes[i].B;
			P->WakeC[i] = In.Wakes[i].C;
			P->WakeD[i] = In.Wakes[i].D;
		}
		P->BoundsThreshold = BoundsThreshold;
		P->LinearClamp = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		P->LinearWrap = TStaticSamplerState<SF_Trilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		P->Noise = NoiseT;
	};

	if (Dt > 0.0f)
	{
		RDG_EVENT_SCOPE_STAT(GraphBuilder, BattleSmokeField, "Field");
		// 2) advecção da velocidade
		{
			FRDGTextureRef Out = NewVolume(GraphBuilder, VelDim, PF_FloatRGBA, TEXT("BattleSmoke.Vel"));
			FBattleSmokeParameters* P = GraphBuilder.AllocParameters<FBattleSmokeParameters>();
			Base(P);
			P->VelIn = VelT;
			P->VelOut = GraphBuilder.CreateUAV(Out);
			Dispatch<FBattleAdvectVelCS>(GraphBuilder, TEXT("AdvectVel"), P, VelDim);
			VelT = Out;
		}
		// 3) vorticidade e forças (empuxo, turbulência, o que passa)
		FRDGTextureRef CurlT = NewVolume(GraphBuilder, VelDim, PF_FloatRGBA, TEXT("BattleSmoke.Curl"));
		{
			FBattleSmokeParameters* P = GraphBuilder.AllocParameters<FBattleSmokeParameters>();
			Base(P);
			P->VelIn = VelT;
			P->CurlOut = GraphBuilder.CreateUAV(CurlT);
			Dispatch<FBattleCurlCS>(GraphBuilder, TEXT("Curl"), P, VelDim);
		}
		{
			FRDGTextureRef Out = NewVolume(GraphBuilder, VelDim, PF_FloatRGBA, TEXT("BattleSmoke.Vel"));
			FBattleSmokeParameters* P = GraphBuilder.AllocParameters<FBattleSmokeParameters>();
			Base(P);
			P->VelIn = VelT;
			P->FldIn = FldT;
			P->CurlIn = CurlT;
			P->VelOut = GraphBuilder.CreateUAV(Out);
			Dispatch<FBattleForcesCS>(GraphBuilder, TEXT("Forces"), P, VelDim);
			VelT = Out;
		}
		// 4) divergência (+ explosões) → pressão (Jacobi, partida a quente) → projeção
		FRDGTextureRef DivT = NewVolume(GraphBuilder, VelDim, PF_A32B32G32R32F, TEXT("BattleSmoke.Div"));
		{
			FBattleSmokeParameters* P = GraphBuilder.AllocParameters<FBattleSmokeParameters>();
			Base(P);
			P->VelIn = VelT;
			P->DivOut = GraphBuilder.CreateUAV(DivT);
			Dispatch<FBattleDivCS>(GraphBuilder, TEXT("Div"), P, VelDim);
		}
		for (int32 i = 0; i < PressureIters; ++i)
		{
			FRDGTextureRef Out = NewVolume(GraphBuilder, VelDim, PF_A32B32G32R32F, TEXT("BattleSmoke.Pressure"));
			FBattleSmokeParameters* P = GraphBuilder.AllocParameters<FBattleSmokeParameters>();
			Base(P);
			P->PressIn = PressT;
			P->DivIn = DivT;
			P->PressOut = GraphBuilder.CreateUAV(Out);
			Dispatch<FBattlePressureCS>(GraphBuilder, TEXT("Pressure"), P, VelDim);
			PressT = Out;
		}
		{
			FRDGTextureRef Out = NewVolume(GraphBuilder, VelDim, PF_FloatRGBA, TEXT("BattleSmoke.Vel"));
			FBattleSmokeParameters* P = GraphBuilder.AllocParameters<FBattleSmokeParameters>();
			Base(P);
			P->VelIn = VelT;
			P->PressIn = PressT;
			P->VelOut = GraphBuilder.CreateUAV(Out);
			Dispatch<FBattleProjectCS>(GraphBuilder, TEXT("Project"), P, VelDim);
			VelT = Out;
		}
		// 5) transporte da fumaça com decaimento (dissipação e resfriamento)
		const float H = Dt / FieldSubsteps;
		const float KS = FMath::Exp(-H / SootTau);
		for (int32 i = 0; i < FieldSubsteps; ++i)
		{
			FRDGTextureRef Out = NewVolume(GraphBuilder, FineDim, PF_A32B32G32R32F, TEXT("BattleSmoke.Field"));
			FBattleSmokeParameters* P = GraphBuilder.AllocParameters<FBattleSmokeParameters>();
			Base(P);
			P->Dt = H;
			P->Keep = FVector4f(KS, FMath::Exp(-H / CoolTau), KS, KS);
			P->VelIn = VelT;
			P->FldIn = FldT;
			P->FldOut = GraphBuilder.CreateUAV(Out);
			Dispatch<FBattleAdvectFieldCS>(GraphBuilder, TEXT("AdvectField"), P, FineDim);
			FldT = Out;
		}
		// 6) túneis e fumaça de motor do que passou
		if (In.Disturbers.Num())
		{
			FRDGTextureRef Out = NewVolume(GraphBuilder, FineDim, PF_A32B32G32R32F, TEXT("BattleSmoke.Field"));
			FBattleSmokeParameters* P = GraphBuilder.AllocParameters<FBattleSmokeParameters>();
			Base(P);
			P->FldIn = FldT;
			P->FldOut = GraphBuilder.CreateUAV(Out);
			Dispatch<FBattleFieldsFxCS>(GraphBuilder, TEXT("FieldsFx"), P, FineDim);
			FldT = Out;
		}
	}

	// 6b) deformação das explosões gravadas pelo que passa (com a velocidade final do campo)
	if (In.Warps.Num())
	{
		RDG_EVENT_SCOPE_STAT(GraphBuilder, BattleSmokeWarp, "Warp");
		for (const FBattleSmokeWarp& W : In.Warps)
		{
			if (!W.Velocity || !W.Velocity->TextureRHI || !W.Out || W.Slot < 0 || W.Slot >= MaxWarps)
			{
				continue;
			}
			FRDGTextureRef State = Persistent(GraphBuilder, WarpState[W.Slot], W.bReset, WarpDim, PF_FloatRGBA, TEXT("BattleSmoke.Warp"));
			FRDGTextureRef Next = NewVolume(GraphBuilder, WarpDim, PF_FloatRGBA, TEXT("BattleSmoke.Warp"));
			FRDGTextureRef BakeVelT = External(GraphBuilder, W.Velocity->TextureRHI, TEXT("BattleSmoke.BakeVelocity"));
			auto WarpParams = [&](FBattleSmokeParameters* P)
			{
				Base(P);
				P->WarpFoot = FVector4f(W.FootM, W.Scale);
				const float R = float(W.VelocityResolution);
				P->WarpFrames = FVector4f(W.Frame0 * R, W.Frame1 * R, W.Alpha, R);
				P->WarpAtlasDepth = R * W.VelocityFrames;
				P->WarpDomain = W.DomainLocalM;
				P->WarpDim = FIntVector3(WarpDim.X, WarpDim.Y, WarpDim.Z);
				P->WarpCarveLife = CarveLife;
				P->WarpCarveClose = CarveClose;
				static const int32 Debug = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("WarpDebug="), V); return V; }();
				P->WarpDebug = Debug;
				P->WarpMax = WarpMaxLocalM;
			};
			{
				FBattleSmokeParameters* P = GraphBuilder.AllocParameters<FBattleSmokeParameters>();
				WarpParams(P);
				P->WarpIn = State;
				P->VelIn = VelT;
				P->BakeVel = BakeVelT;
				P->WarpOut = GraphBuilder.CreateUAV(Next);
				Dispatch<FBattleWarpCS>(GraphBuilder, TEXT("Warp"), P, WarpDim);
			}
			{
				FRDGTextureRef Out = External(GraphBuilder, W.Out->GetRenderTargetTexture(), TEXT("BattleSmoke.WarpOut"));
				FBattleSmokeParameters* P = GraphBuilder.AllocParameters<FBattleSmokeParameters>();
				WarpParams(P);
				P->WarpIn = Next;
				P->OutTex = GraphBuilder.CreateUAV(Out);
				Dispatch<FBattleWarpRenderCS>(GraphBuilder, TEXT("WarpRender"), P, WarpDim);
			}
			GraphBuilder.QueueTextureExtraction(Next, &WarpState[W.Slot]);
		}
	}

	// 7) saída pro material e a caixa ocupada
	FRDGBufferRef BoundsBuf = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), NumBoundsWords), TEXT("BattleSmoke.Bounds"));
	FRDGBufferUAVRef BoundsUAV = GraphBuilder.CreateUAV(BoundsBuf, PF_R32_UINT);
	{
		FBattleSmokeParameters* P = GraphBuilder.AllocParameters<FBattleSmokeParameters>();
		Base(P);
		P->Bounds = BoundsUAV;
		TShaderMapRef<FBattleResetBoundsCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("BattleSmoke ResetBounds"), Shader, P, FIntVector(1, 1, 1));
	}
	{
		FRDGTextureRef Out = External(GraphBuilder, In.FieldOut->GetRenderTargetTexture(), TEXT("BattleSmoke.FieldOut"));
		FBattleSmokeParameters* P = GraphBuilder.AllocParameters<FBattleSmokeParameters>();
		Base(P);
		P->FldIn = FldT;
		P->OutTex = GraphBuilder.CreateUAV(Out);
		P->Bounds = BoundsUAV;
		Dispatch<FBattleCopyFieldCS>(GraphBuilder, TEXT("CopyField"), P, FineDim);
	}
	{
		FRDGTextureRef Out = External(GraphBuilder, In.VelOut->GetRenderTargetTexture(), TEXT("BattleSmoke.VelOut"));
		FBattleSmokeParameters* P = GraphBuilder.AllocParameters<FBattleSmokeParameters>();
		Base(P);
		P->VelIn = VelT;
		P->OutTex = GraphBuilder.CreateUAV(Out);
		Dispatch<FBattleCopyVelCS>(GraphBuilder, TEXT("CopyVel"), P, VelDim);
	}
	if (ReadbackPending < NumReadbacks)
	{
		FRHIGPUBufferReadback*& R = Readbacks[ReadbackWrite];
		if (!R)
		{
			R = new FRHIGPUBufferReadback(TEXT("BattleSmoke.BoundsReadback"));
		}
		AddEnqueueCopyPass(GraphBuilder, R, BoundsBuf, NumBoundsWords * sizeof(uint32));
		ReadbackWrite = (ReadbackWrite + 1) % NumReadbacks;
		++ReadbackPending;
	}

	GraphBuilder.QueueTextureExtraction(VelT, &Vel);
	GraphBuilder.QueueTextureExtraction(FldT, &Fld);
	GraphBuilder.QueueTextureExtraction(PressT, &Press);
}
