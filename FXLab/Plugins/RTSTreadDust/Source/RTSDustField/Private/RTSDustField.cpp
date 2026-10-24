#include "RTSDustField.h"

#include "Async/ParallelFor.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "SceneViewExtension.h"
#include "ScreenPass.h"
#include "GlobalShader.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Compression.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "RTSSunShadowRender.h"
#include "SceneView.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ShaderCore.h"
#include "ShaderParameterStruct.h"
#include "SystemTextures.h"

// O campo da poeira (RTSDustField.h): a gravação do EmberGen como textura 3D, os puffs da simulação em tijolos presos
// ao mundo (o mesmo tijolo guarda o lugar no atlas de um quadro pro outro: metade é refeita por quadro, em xadrez, e os
// novos sempre), e três passes: a tabela de páginas, a densidade e a luz. A marcha é a do raymarch das explosões.

namespace
{
	// os presets: Ultra 16 voxels por tijolo (0,125 m), Alta 8 (0,25 m); guardados com a borda de 1 voxel de cada lado
	int32 StoreOf(bool bFine) { return bFine ? 18 : 10; }
	constexpr double BrickM = 2.0;
	// a janela em tijolos (22/10, v4: 256 × 256 × 64 m; era 192 × 192 × 32 e, com os morros e o zoom de 140 m, a borda dela
	// aparecia como uma linha reta "onde a poeira começa", o Felipe). A borda horizontal ainda some aos poucos (DustField.ush)
	constexpr int32 WinX = 128, WinY = 128, WinZ = 32;
	constexpr int32 AtlasBX = 32, AtlasBY = 32, AtlasBZ = 4;   // 4096 tijolos no atlas
	constexpr int32 MaxBricks = AtlasBX * AtlasBY * AtlasBZ;
	constexpr int32 MaxPuffsPerBrick = 96;               // o mesmo do DustField.usf (a memória do grupo)
	constexpr int32 PuffStride = 5;                      // float4 por puff (o PUFF_STRIDE do DustField.usf)

	// a gravação, como a thread do jogo vê (o cabeçalho) e como a de desenho vê (as texturas)
	struct FKickInfo
	{
		bool bLoaded = false;
		int32 Frames = 0, Cols = 8;
		FIntVector Res = FIntVector(48, 48, 24), TexSize = FIntVector(384, 288, 24);
		float VoxM = 0.1f;
		bool bRho2 = false;                             // a gravação já guarda ρ² (a média de ρ² da grossa), sem elevar
		FVector3f Origin = FVector3f(-2.4f, -2.4f, 0.0f);
		TArray<FBox3f> Boxes;                           // a caixa da nuvem em cada quadro (m da gravação)
		TArray<float> Column;                           // a coluna densa de cada quadro: Σ ρ²·dz no p95 das colunas (m)
		TArray<float> Times;                            // o tempo de cozimento de cada quadro (s; "times_s" do cabeçalho)
	};
	// duas gravações, uma por preset (17/10): [0] a fina de 0,05 m pro campo de 0,125 m (Ultra), [1] a grossa de 0,1 m com
	// a média de ρ² pro campo de 0,25 m (Alta). Cada campo lê a gravação no mesmo passo relativo (o texel × a escala do puff
	// ~1,5× o voxel do campo), então nenhum dos dois serrilha; e a Alta não paga o cache da fina (+0,4 ms com 40 tanques)
	FKickInfo GKick[2];
	FKickInfo GKickRT[2];                               // a cópia da thread de desenho
	FTextureRHIRef GKickRHI[2], GNoiseRHI;

	// um quadro do campo, montado na thread do jogo e entregue à de desenho
	struct FFrameData
	{
		TArray<FUintVector4> Bricks;
		TArray<uint32> Refresh;
		TArray<FVector4f> Puffs;                       // 5 por puff
		TArray<uint32> PuffIdx;
		FVector WindowOriginCm = FVector::ZeroVector;
		FVector3f BoxMinW = FVector3f::ZeroVector, BoxMaxW = FVector3f::ZeroVector;
		FVector3f ToSun = FVector3f(0.0f, 0.0f, 1.0f);
		float Time = 0.0f, Detail = 0.0f;
		bool bFine = true;                             // o preset: 0,125 m (Ultra) ou 0,25 m (Alta)
		int32 Kick = 0;                                // a gravação usada (GKick[0] fina, [1] grossa)
	};
	TSharedPtr<FFrameData, ESPMode::ThreadSafe> GFrameRT;
	TRefCountPtr<IPooledRenderTarget> GPT, GPTCoarse, GAtlas, GColor, GLightA, GLightB;

	// o atlas na thread do jogo: o tijolo do mundo → o lugar dele (preso enquanto tiver poeira)
	TMap<uint64, int32> GSlotOf;
	TArray<int32> GFree;
	TArray<uint64> GSlotKey;
	TArray<int32> GSlotLast;
	int32 GSerial = 0;
	int32 GFineLast = -1;                              // o preset do último quadro (trocou: todos os tijolos são refeitos)
	FIntVector4 GStats = FIntVector4(0, 0, 0, 0);

	uint64 BrickKey(int32 X, int32 Y, int32 Z)
	{
		return uint64(uint32(X + (1 << 20))) | (uint64(uint32(Y + (1 << 20))) << 21) | (uint64(uint32(Z + (1 << 20))) << 42);
	}

	// o ruído de Worley que ladrilha (64³, 3 oitavas de 16, 8 e 4 células: 0,6, 1,2 e 2,4 m num ladrilho de 9,6 m), o
	// mesmo desenho do T_DustErodeNoise da v26 em 3D
	TArray<uint8> MakeWorley3D(int32 N)
	{
		TArray<float> Acc;
		Acc.Init(0.0f, N * N * N);
		const int32 Cells[3] = { 4, 8, 16 };
		const float Weights[3] = { 0.625f, 0.25f, 0.125f };
		for (int32 O = 0; O < 3; ++O)
		{
			const int32 C = Cells[O];
			TArray<FVector3f> Pts;
			Pts.SetNumUninitialized(C * C * C);
			FRandomStream Rng(1234 + O);
			for (FVector3f& P : Pts)
			{
				P = FVector3f(Rng.FRand(), Rng.FRand(), Rng.FRand());
			}
			const float Cs = float(N) / C;
			ParallelFor(N, [&](int32 Z)
			{
				for (int32 Y = 0; Y < N; ++Y)
				{
					for (int32 X = 0; X < N; ++X)
					{
						const FVector3f Q = FVector3f(float(X) + 0.5f, float(Y) + 0.5f, float(Z) + 0.5f) / Cs;
						const FIntVector Ci(FMath::FloorToInt32(Q.X), FMath::FloorToInt32(Q.Y), FMath::FloorToInt32(Q.Z));
						float Best = 1e9f;
						for (int32 Dz = -1; Dz <= 1; ++Dz)
						{
							for (int32 Dy = -1; Dy <= 1; ++Dy)
							{
								for (int32 Dx = -1; Dx <= 1; ++Dx)
								{
									const FIntVector Cn = Ci + FIntVector(Dx, Dy, Dz);
									const FIntVector Cw((Cn.X % C + C) % C, (Cn.Y % C + C) % C, (Cn.Z % C + C) % C);
									const FVector3f F = FVector3f(Cn) + Pts[(Cw.Z * C + Cw.Y) * C + Cw.X];
									Best = FMath::Min(Best, (F - Q).SizeSquared());
								}
							}
						}
						Acc[(Z * N + Y) * N + X] += Weights[O] * (1.0f - FMath::Min(FMath::Sqrt(Best), 1.0f));
					}
				}
			});
		}
		// igualado (o histograma plano, como o 2D): o ruído mexe igual em toda a faixa
		TArray<int32> Order;
		Order.SetNumUninitialized(Acc.Num());
		for (int32 I = 0; I < Order.Num(); ++I)
		{
			Order[I] = I;
		}
		Order.Sort([&Acc](int32 A, int32 B) { return Acc[A] < Acc[B]; });
		TArray<uint8> Out;
		Out.SetNumUninitialized(Acc.Num());
		for (int32 R = 0; R < Order.Num(); ++R)
		{
			Out[Order[R]] = uint8(FMath::Clamp(FMath::RoundToInt32(255.0 * (R + 0.5) / Order.Num()), 0, 255));
		}
		return Out;
	}
}

// ---- os shaders ------------------------------------------------------------------
class FRTSDustScatterCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRTSDustScatterCS);
	SHADER_USE_PARAMETER_STRUCT(FRTSDustScatterCS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint4>, Bricks)
		SHADER_PARAMETER(uint32, NumBricks)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<uint>, PageTableOut)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<uint>, CoarseOut)
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& P) { return IsFeatureLevelSupported(P.Platform, ERHIFeatureLevel::SM5); }
};
IMPLEMENT_GLOBAL_SHADER(FRTSDustScatterCS, "/Plugin/RTSTreadDust/Private/DustField.usf", "MainScatter", SF_Compute);

class FRTSDustFillCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRTSDustFillCS);
	SHADER_USE_PARAMETER_STRUCT(FRTSDustFillCS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint4>, Bricks)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, Refresh)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, Puffs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PuffIdx)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, Kick)
		SHADER_PARAMETER_SAMPLER(SamplerState, KickSampler)
		SHADER_PARAMETER(FVector3f, KickInvSize)
		SHADER_PARAMETER(FVector3f, KickRes)
		SHADER_PARAMETER(float, KickCols)
		SHADER_PARAMETER(float, KickFrames)
		SHADER_PARAMETER(float, KickVoxM)
		SHADER_PARAMETER(FVector3f, KickOrigin)
		SHADER_PARAMETER(float, KickRho2)
		SHADER_PARAMETER(float, FillTime)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, RTSDustNoise)
		SHADER_PARAMETER_SAMPLER(SamplerState, RTSDustNoiseSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, AtlasOut)
		SHADER_PARAMETER(float, FillVoxM)
		SHADER_PARAMETER(uint32, FillStore)
		SHADER_PARAMETER(uint32, FillGroups)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, ColorOut)
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& P) { return IsFeatureLevelSupported(P.Platform, ERHIFeatureLevel::SM5); }
};
IMPLEMENT_GLOBAL_SHADER(FRTSDustFillCS, "/Plugin/RTSTreadDust/Private/DustField.usf", "MainFill", SF_Compute);

class FRTSDustLightCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRTSDustLightCS);
	SHADER_USE_PARAMETER_STRUCT(FRTSDustLightCS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint4>, Bricks)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, Refresh)
		SHADER_PARAMETER_STRUCT_INCLUDE(FRTSDustFieldParameters, Dust)
		SHADER_PARAMETER_STRUCT_INCLUDE(FRTSSunShadowParameters, Sun)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, LightAOut)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, LightBOut)
		SHADER_PARAMETER(FVector3f, SunDirW)
		SHADER_PARAMETER(float, SunStepM)
		SHADER_PARAMETER(int32, SunSteps)
		SHADER_PARAMETER(float, TauCap)
		SHADER_PARAMETER(int32, NumLights)
		SHADER_PARAMETER_ARRAY(FVector4f, LightPosR, [8])
		SHADER_PARAMETER_ARRAY(FVector4f, LightCol, [8])
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& P) { return IsFeatureLevelSupported(P.Platform, ERHIFeatureLevel::SM5); }
};
IMPLEMENT_GLOBAL_SHADER(FRTSDustLightCS, "/Plugin/RTSTreadDust/Private/DustField.usf", "MainLight", SF_Compute);

// ---- o módulo: a pasta dos shaders do plugin -------------------------------------------
class FRTSDustFieldModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("RTSTreadDust"));
		if (Plugin.IsValid())
		{
			AddShaderSourceDirectoryMapping(TEXT("/Plugin/RTSTreadDust"), FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders")));
		}
	}
	virtual void ShutdownModule() override
	{
		GSlotOf.Reset();
		ReleaseDebugView();
	}
	static void ReleaseDebugView();
};
IMPLEMENT_MODULE(FRTSDustFieldModule, RTSDustField)

// ---- a gravação ---------------------------------------------------------------------
bool RTSDustField::LoadKick(const FString& Path)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() < 8 || FMemory::Memcmp(Bytes.GetData(), "RTSD", 4) != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTSDustField: falta a gravação da poeira em volume (%s): a poeira fica nos cartões"), *Path);
		return false;
	}
	uint32 HeaderLen = 0;
	FMemory::Memcpy(&HeaderLen, Bytes.GetData() + 4, 4);
	if (8 + int64(HeaderLen) > Bytes.Num())
	{
		return false;
	}
	const FString Json = FString(FUTF8ToTCHAR(reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + 8), HeaderLen));
	TSharedPtr<FJsonObject> H;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), H) || !H)
	{
		return false;
	}
	FKickInfo K;
	K.Frames = int32(H->GetNumberField(TEXT("frames")));
	const TArray<TSharedPtr<FJsonValue>>& Res = H->GetArrayField(TEXT("res"));
	const TArray<TSharedPtr<FJsonValue>>& Ts = H->GetArrayField(TEXT("tex_size"));
	const TArray<TSharedPtr<FJsonValue>>& Tl = H->GetArrayField(TEXT("tiles"));
	const TArray<TSharedPtr<FJsonValue>>& Or = H->GetArrayField(TEXT("origin_m"));
	K.Res = FIntVector(int32(Res[0]->AsNumber()), int32(Res[1]->AsNumber()), int32(Res[2]->AsNumber()));
	K.TexSize = FIntVector(int32(Ts[0]->AsNumber()), int32(Ts[1]->AsNumber()), int32(Ts[2]->AsNumber()));
	K.Cols = int32(Tl[0]->AsNumber());
	K.VoxM = float(H->GetNumberField(TEXT("voxel_m")));
	const TArray<TSharedPtr<FJsonValue>>* TimesJ = nullptr;
	if (H->TryGetArrayField(TEXT("times_s"), TimesJ))
	{
		for (const TSharedPtr<FJsonValue>& V : *TimesJ)
		{
			K.Times.Add(float(V->AsNumber()));
		}
	}
	K.bRho2 = H->HasField(TEXT("rho2")) && H->GetNumberField(TEXT("rho2")) > 0.5;
	const int32 Slot = K.VoxM < 0.075f ? 0 : 1;          // a fina (0,05 m) ou a grossa (0,1 m)
	if (GKick[Slot].bLoaded)
	{
		return true;
	}
	K.Origin = FVector3f(float(Or[0]->AsNumber()), float(Or[1]->AsNumber()), float(Or[2]->AsNumber()));
	for (const TSharedPtr<FJsonValue>& B : H->GetArrayField(TEXT("box_m")))
	{
		const TArray<TSharedPtr<FJsonValue>>& V = B->AsArray();
		K.Boxes.Add(FBox3f(FVector3f(float(V[0]->AsNumber()), float(V[1]->AsNumber()), float(V[2]->AsNumber())),
			FVector3f(float(V[3]->AsNumber()), float(V[4]->AsNumber()), float(V[5]->AsNumber()))));
	}
	const int64 DataBytes = int64(K.TexSize.X) * K.TexSize.Y * K.TexSize.Z * 2;
	// os quadros vêm comprimidos com zlib quando o cabeçalho traz "zlib" (o tamanho descomprimido; 17/10: quase tudo é ar,
	// o de 0,05 m cai de 42 MB pra 1,8 MB); sem a chave, crus logo depois do cabeçalho
	TArray<uint8> Data;
	const int64 Payload = Bytes.Num() - 8 - int64(HeaderLen);
	if (H->HasField(TEXT("zlib")))
	{
		Data.SetNumUninitialized(int32(DataBytes));
		if (int64(H->GetNumberField(TEXT("zlib"))) != DataBytes || K.Boxes.Num() != K.Frames
			|| !FCompression::UncompressMemory(NAME_Zlib, Data.GetData(), int32(DataBytes), Bytes.GetData() + 8 + HeaderLen, int32(Payload)))
		{
			UE_LOG(LogTemp, Warning, TEXT("RTSDustField: a gravação da poeira não descomprime (%s)"), *Path);
			return false;
		}
	}
	else
	{
		if (DataBytes > Payload || K.Boxes.Num() != K.Frames)
		{
			UE_LOG(LogTemp, Warning, TEXT("RTSDustField: a gravação da poeira está truncada (%s)"), *Path);
			return false;
		}
		Data = TArray<uint8>(Bytes.GetData() + 8 + HeaderLen, int32(DataBytes));
	}
	// a coluna densa de cada quadro (17/10, o nascer do puff): Σ ρ²·dz em cada coluna vertical do quadro, no p95 das
	// colunas com poeira. × σ do puff dá a profundidade óptica do miolo, a que o alfa do cartão escalava em opacidade
	{
		const FFloat16* V = reinterpret_cast<const FFloat16*>(Data.GetData());
		TArray<float> Cols;
		for (int32 F = 0; F < K.Frames; ++F)
		{
			const int32 Cx = (F % K.Cols) * K.Res.X, Cy = (F / K.Cols) * K.Res.Y;
			Cols.Reset();
			for (int32 Y = 0; Y < K.Res.Y; ++Y)
			{
				for (int32 X = 0; X < K.Res.X; ++X)
				{
					double S = 0.0;
					for (int32 Z = 0; Z < K.Res.Z; ++Z)
					{
						const float Rv = float(V[(int64(Z) * K.TexSize.Y + (Cy + Y)) * K.TexSize.X + (Cx + X)]);
						S += K.bRho2 ? Rv : double(Rv) * Rv;
					}
					if (S * K.VoxM > 1e-3)
					{
						Cols.Add(float(S * K.VoxM));
					}
				}
			}
			Cols.Sort();
			K.Column.Add(Cols.Num() ? Cols[FMath::Min(Cols.Num() - 1, int32(Cols.Num() * 0.95f))] : 0.0f);
		}
	}
	K.bLoaded = true;
	GKick[Slot] = K;
	const bool bNoise = !GKick[1 - Slot].bLoaded;        // o ruído nasce junto com a primeira gravação
	TArray<uint8> Noise = bNoise ? MakeWorley3D(64) : TArray<uint8>();
	ENQUEUE_RENDER_COMMAND(RTSDustFieldLoadKick)([K, Slot, bNoise, Data = MoveTemp(Data), Noise = MoveTemp(Noise)](FRHICommandListImmediate& RHICmdList)
	{
		const FRHITextureCreateDesc Desc = FRHITextureCreateDesc::Create3D(Slot == 0 ? TEXT("RTSDust.KickFine") : TEXT("RTSDust.KickCoarse"),
			K.TexSize, PF_R16F).SetFlags(ETextureCreateFlags::ShaderResource);
		GKickRHI[Slot] = RHICmdList.CreateTexture(Desc);
		RHICmdList.UpdateTexture3D(GKickRHI[Slot], 0, FUpdateTextureRegion3D(0, 0, 0, 0, 0, 0, K.TexSize.X, K.TexSize.Y, K.TexSize.Z),
			K.TexSize.X * 2, K.TexSize.X * K.TexSize.Y * 2, Data.GetData());
		if (bNoise)
		{
			const FRHITextureCreateDesc NDesc = FRHITextureCreateDesc::Create3D(TEXT("RTSDust.Noise"), FIntVector(64, 64, 64), PF_G8)
				.SetFlags(ETextureCreateFlags::ShaderResource);
			GNoiseRHI = RHICmdList.CreateTexture(NDesc);
			RHICmdList.UpdateTexture3D(GNoiseRHI, 0, FUpdateTextureRegion3D(0, 0, 0, 0, 0, 0, 64, 64, 64), 64, 64 * 64, Noise.GetData());
		}
		GKickRT[Slot] = K;
	});
	UE_LOG(LogTemp, Display, TEXT("RTSDustField: a gravação da poeira em volume (%s): %d quadros de %d×%d×%d voxels (%.2f m%s), %.1f MB"),
		Slot == 0 ? TEXT("fina, Ultra") : TEXT("grossa, Alta"), K.Frames, K.Res.X, K.Res.Y, K.Res.Z, K.VoxM,
		K.bRho2 ? TEXT(", média de ρ²") : TEXT(""), DataBytes / 1e6);
	return true;
}

bool RTSDustField::IsKickLoaded()
{
	return GKick[0].bLoaded || GKick[1].bLoaded;
}

int32 RTSDustField::KickFrames()
{
	return GKick[0].bLoaded ? GKick[0].Frames : GKick[1].Frames;
}

float RTSDustField::KickFrameAt(double BakeT)
{
	const FKickInfo& K = GKick[0].bLoaded ? GKick[0] : GKick[1];
	if (!K.bLoaded || K.Times.Num() < 2 || K.Times.Num() != K.Frames)
	{
		return -1.0f;
	}
	if (BakeT <= K.Times[0])
	{
		return 0.0f;
	}
	int32 Lo = 0, Hi = K.Times.Num() - 1;
	if (BakeT >= K.Times[Hi])
	{
		return float(Hi);
	}
	while (Hi - Lo > 1)
	{
		const int32 Mid = (Lo + Hi) / 2;
		(K.Times[Mid] < BakeT ? Lo : Hi) = Mid;
	}
	return float(Lo + (BakeT - K.Times[Lo]) / FMath::Max(double(K.Times[Hi] - K.Times[Lo]), 1e-6));
}

float RTSDustField::KickColumn(float Frame)
{
	const FKickInfo& K = GKick[0].bLoaded ? GKick[0] : GKick[1];
	if (!K.bLoaded || K.Column.Num() == 0)
	{
		return 0.0f;
	}
	const float F = FMath::Clamp(Frame, 0.0f, float(K.Column.Num() - 1));
	const int32 F0 = FMath::FloorToInt32(F), F1 = FMath::Min(F0 + 1, K.Column.Num() - 1);
	return FMath::Lerp(K.Column[F0], K.Column[F1], F - float(F0));
}

FIntVector4 RTSDustField::GetStats()
{
	return GStats;
}

// o preset do campo: -1 = pela qualidade de efeitos do jogo (sg.EffectsQuality 4 = Ultra), 1 = 0,125 m, 0 = 0,25 m. O jogo põe
// baixa/média/alta/ultra nos níveis 1/2/3/4 do motor (RTSPlayerController::ApplyQuality); até 17/10 o corte era 3 e a Alta
// pagava o campo fino do Ultra (+3,2 ms sobre os cartões em vez de −0,8; achado do estudo da nitidez)
static TAutoConsoleVariable<int32> CVarDustRefresh(TEXT("rts.DustField.Refresh"), 1,
	TEXT("Os tijolos da poeira refeitos por quadro: 1 = todos (o padrão desde 22/10), 2 = metade, 4 = um quarto (em xadrez)"), ECVF_Default);

static TAutoConsoleVariable<int32> CVarDustFine(TEXT("rts.DustField.Fine"), -1,
	TEXT("O campo da poeira: -1 pela qualidade de efeitos (Ultra = 0,125 m; Alta = 0,25 m), 1 fino, 0 grosso"),
	ECVF_Default);

// ---- o quadro: os puffs em tijolos ---------------------------------------------------------
void RTSDustField::SubmitFrame(const FVector& FocusM, const FVector& ToSun, double TimeS, float Detail, TArray<FRTSDustPuff>&& Puffs)
{
	++GSerial;
	TSharedPtr<FFrameData, ESPMode::ThreadSafe> D = MakeShared<FFrameData, ESPMode::ThreadSafe>();
	static const IConsoleVariable* RefreshVar = IConsoleManager::Get().FindConsoleVariable(TEXT("rts.DustField.Refresh"));
	const int32 RefreshDiv = RefreshVar ? RefreshVar->GetInt() : 1;
	const int32 RefreshMask = RefreshDiv >= 4 ? 3 : RefreshDiv >= 2 ? 1 : 0;
	// o preset: rts.DustField.Fine (1 = 0,125 m, 0 = 0,25 m) ou, no automático, a qualidade de efeitos (4 = Ultra)
	{
		static const IConsoleVariable* Fine = IConsoleManager::Get().FindConsoleVariable(TEXT("rts.DustField.Fine"));
		static const IConsoleVariable* Fx = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.EffectsQuality"));
		const int32 F = Fine ? Fine->GetInt() : -1;
		D->bFine = F >= 0 ? F > 0 : (!Fx || Fx->GetInt() >= 4);
		if (int32(D->bFine) != GFineLast)
		{
			GFineLast = int32(D->bFine);
			GSlotOf.Reset();                           // o atlas muda de tamanho: todos os tijolos nascem de novo
			GSlotKey.Reset();
		}
	}
	D->ToSun = FVector3f(ToSun.GetSafeNormal());
	D->Time = float(FMath::Fmod(TimeS, 1e4));
	D->Detail = Detail;
	// a gravação do preset (sem a dele carregada, a outra)
	D->Kick = (D->bFine || !GKick[1].bLoaded) && GKick[0].bLoaded ? 0 : 1;
	const FKickInfo& Kk = GKick[D->Kick];
	int32 Overflow = 0, Trunc = 0;
	if (Kk.bLoaded && Puffs.Num() > 0)
	{
		// a janela: 256 × 256 m em volta do foco, de 32 m abaixo dele a 32 m acima (os vales e os morros), presa à grade
		const FIntVector WinB(FMath::FloorToInt32(FocusM.X / BrickM) - WinX / 2, FMath::FloorToInt32(FocusM.Y / BrickM) - WinY / 2,
			FMath::FloorToInt32(FocusM.Z / BrickM) - WinZ / 2);
		const FVector WinO = FVector(WinB) * BrickM;
		D->WindowOriginCm = WinO * 100.0;
		struct FPair { uint64 Key; int32 Puff; int32 Bx, By, Bz; };
		TArray<FPair> Pairs;
		Pairs.Reserve(Puffs.Num() * 24);
		D->Puffs.Reserve(Puffs.Num() * PuffStride);
		for (const FRTSDustPuff& Pf : Puffs)
		{
			if (Pf.Sigma <= 0.0f || Pf.Scale <= 0.0f)
			{
				continue;
			}
			const FVector3f P = FVector3f(Pf.PosM - WinO);
			// a caixa da nuvem neste quadro (a união dos dois quadros misturados), girada: um círculo em xy
			const int32 F0 = FMath::Clamp(FMath::FloorToInt32(Pf.Frame), 0, Kk.Frames - 1), F1 = FMath::Min(F0 + 1, Kk.Frames - 1);
			FBox3f Bx = Kk.Boxes[F0] + Kk.Boxes[F1];
			const float Rxy = FMath::Max(FMath::Max(FMath::Abs(Bx.Min.X), FMath::Abs(Bx.Max.X)), FMath::Max(FMath::Abs(Bx.Min.Y), FMath::Abs(Bx.Max.Y)))
				* 1.415f * Pf.Scale + 0.3f;
			const float Z0 = P.Z + Bx.Min.Z * Pf.Scale - 0.3f, Z1 = P.Z + Bx.Max.Z * Pf.Scale + 0.3f;
			const int32 X0 = FMath::Max(0, FMath::FloorToInt32((P.X - Rxy) / BrickM)), X1 = FMath::Min(WinX - 1, FMath::FloorToInt32((P.X + Rxy) / BrickM));
			const int32 Y0 = FMath::Max(0, FMath::FloorToInt32((P.Y - Rxy) / BrickM)), Y1 = FMath::Min(WinY - 1, FMath::FloorToInt32((P.Y + Rxy) / BrickM));
			const int32 Zb0 = FMath::Max(0, FMath::FloorToInt32(Z0 / BrickM)), Zb1 = FMath::Min(WinZ - 1, FMath::FloorToInt32(Z1 / BrickM));
			if (X0 > X1 || Y0 > Y1 || Zb0 > Zb1)
			{
				continue;                                 // fora da janela
			}
			const int32 Pi = D->Puffs.Num() / PuffStride;
			D->Puffs.Add(FVector4f(P.X, P.Y, P.Z, Pf.bMirror ? -Pf.Scale : Pf.Scale));
			D->Puffs.Add(FVector4f(FMath::Cos(Pf.Yaw), FMath::Sin(Pf.Yaw), Pf.Frame, Pf.Sigma));
			D->Puffs.Add(FVector4f(Pf.Albedo.R, Pf.Albedo.G, Pf.Albedo.B, Pf.Erode));
			// a caixa da nuvem nos dois quadros (com 1 voxel da gravação de folga): o preenchimento não lê fora dela
			D->Puffs.Add(FVector4f(Bx.Min.X - Kk.VoxM, Bx.Min.Y - Kk.VoxM, Bx.Max.X + Kk.VoxM, Bx.Max.Y + Kk.VoxM));
			D->Puffs.Add(FVector4f(Bx.Max.Z + Kk.VoxM, 0.0f, 0.0f, 0.0f));
			for (int32 Z = Zb0; Z <= Zb1; ++Z)
			{
				for (int32 Y = Y0; Y <= Y1; ++Y)
				{
					for (int32 X = X0; X <= X1; ++X)
					{
						// o círculo contra o quadrado do tijolo
						const float Cx = FMath::Clamp(P.X, float(X) * float(BrickM), float(X + 1) * float(BrickM));
						const float Cy = FMath::Clamp(P.Y, float(Y) * float(BrickM), float(Y + 1) * float(BrickM));
						if (FMath::Square(Cx - P.X) + FMath::Square(Cy - P.Y) > Rxy * Rxy)
						{
							continue;
						}
						Pairs.Add({ BrickKey(WinB.X + X, WinB.Y + Y, WinB.Z + Z), Pi, X, Y, Z });
					}
				}
			}
		}
		Pairs.Sort([](const FPair& A, const FPair& B) { return A.Key < B.Key || (A.Key == B.Key && A.Puff < B.Puff); });
		if (GSlotKey.Num() != MaxBricks)
		{
			GSlotKey.Init(0, MaxBricks);
			GSlotLast.Init(-1, MaxBricks);
			GFree.Reset();
			for (int32 S = MaxBricks - 1; S >= 0; --S)
			{
				GFree.Add(S);
			}
			GSlotOf.Reset();
		}
		FVector3f Lo(1e9f), Hi(-1e9f);
		for (int32 I = 0; I < Pairs.Num();)
		{
			int32 J = I;
			while (J < Pairs.Num() && Pairs[J].Key == Pairs[I].Key)
			{
				++J;
			}
			const FPair& Pr = Pairs[I];
			bool bNew = false;
			int32* Found = GSlotOf.Find(Pr.Key);
			int32 Slot = Found ? *Found : -1;
			if (Slot < 0)
			{
				if (GFree.Num() == 0)
				{
					++Overflow;                            // o atlas encheu: este tijolo fica sem poeira neste quadro
					I = J;
					continue;
				}
				Slot = GFree.Pop(EAllowShrinking::No);
				GSlotOf.Add(Pr.Key, Slot);
				GSlotKey[Slot] = Pr.Key;
				bNew = true;
			}
			GSlotLast[Slot] = GSerial;
			const int32 Count = FMath::Min(J - I, MaxPuffsPerBrick);
			Trunc += (J - I) - Count;                       // os puffs que não couberam no tijolo (a memória do grupo)
			const uint32 Packed = uint32(Pr.Bx) | (uint32(Pr.By) << 8) | (uint32(Pr.Bz) << 16);
			// todos os tijolos a cada quadro (22/10, o Felipe: "ela tem fps baixo"): com metade (Ultra) ou um quarto (Alta) por
			// quadro, em xadrez, cada tijolo mostrava a poeira de 2 a 4 quadros atrás e a nuvem andava aos degraus (~8 por
			// segundo na Alta a 30 qps). rts.DustField.Refresh 2 ou 4 volta ao xadrez (só pra medir); os novos sempre
			if (bNew || ((Pr.Bx + Pr.By * 2 + Pr.Bz * 3 + GSerial) & RefreshMask) == 0)
			{
				D->Refresh.Add(uint32(D->Bricks.Num()));
			}
			D->Bricks.Add(FUintVector4(Packed, uint32(Slot), uint32(D->PuffIdx.Num()), uint32(Count)));
			for (int32 K = I; K < I + Count; ++K)
			{
				D->PuffIdx.Add(uint32(Pairs[K].Puff));
			}
			Lo = Lo.ComponentMin(FVector3f(float(Pr.Bx), float(Pr.By), float(Pr.Bz)) * float(BrickM));
			Hi = Hi.ComponentMax(FVector3f(float(Pr.Bx + 1), float(Pr.By + 1), float(Pr.Bz + 1)) * float(BrickM));
			I = J;
		}
		D->BoxMinW = Lo;
		D->BoxMaxW = Hi;
	}
	// os lugares que ficaram sem poeira voltam pra fila
	if (GSlotKey.Num() == MaxBricks)
	{
		for (auto It = GSlotOf.CreateIterator(); It; ++It)
		{
			if (GSlotLast[It.Value()] != GSerial)
			{
				GFree.Add(It.Value());
				It.RemoveCurrent();
			}
		}
	}
	GStats = FIntVector4(D->Bricks.Num(), D->Puffs.Num() / PuffStride, D->Refresh.Num(), Overflow);
	if (Trunc > 0 && GSerial % 150 == 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTSDustField: %d pares puff-tijolo cortados (mais de %d puffs num tijolo)"), Trunc, MaxPuffsPerBrick);
	}
	ENQUEUE_RENDER_COMMAND(RTSDustFieldSubmit)([D](FRHICommandListImmediate&) { GFrameRT = D; });
}

// ---- os passes ---------------------------------------------------------------------
namespace
{
	FRDGTextureRef PersistentVolume(FRDGBuilder& GraphBuilder, TRefCountPtr<IPooledRenderTarget>& Pooled, FIntVector Size, EPixelFormat Format, const TCHAR* Name)
	{
		if (Pooled.IsValid() && Pooled->GetDesc().GetSize() == Size)
		{
			return GraphBuilder.RegisterExternalTexture(Pooled, Name);
		}
		const FRDGTextureDesc Desc = FRDGTextureDesc::Create3D(Size, Format, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV);
		FRDGTextureRef T = GraphBuilder.CreateTexture(Desc, Name);
		// nasce zerada (o grafo só extrai o que foi escrito; e o tijolo que não for refeito neste quadro lê zero, não lixo)
		if (Format == PF_R32_UINT)
		{
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(T), 0u);
		}
		else
		{
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(T), FVector4f(0.0f, 0.0f, 0.0f, 0.0f));
		}
		GraphBuilder.QueueTextureExtraction(T, &Pooled);
		return T;
	}
}

bool RTSDustField::AddPasses(FRDGBuilder& GraphBuilder, const FSceneView& View, TConstArrayView<FRTSDustLight> Lights,
	FRTSDustFieldParameters& Out, FVector3f& OutBoxMinM, FVector3f& OutBoxMaxM)
{
	check(IsInRenderingThread());
	// os vazios (o GSystemTextures vale fora da cena também)
	FRDGTextureRef Black = GSystemTextures.GetVolumetricBlackDummy(GraphBuilder);
	Out.RTSDustSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Out.RTSDustNoiseSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	Out.RTSDustWindowBricks = FIntVector(WinX, WinY, WinZ);
	const TSharedPtr<FFrameData, ESPMode::ThreadSafe> D0 = GFrameRT;
	const int32 VoxStore = StoreOf(!D0 || D0->bFine);
	Out.RTSDustAtlasInvSize = FVector3f(1.0f / (AtlasBX * VoxStore), 1.0f / (AtlasBY * VoxStore), 1.0f / (AtlasBZ * VoxStore));
	Out.RTSDustVoxStore = VoxStore;
	Out.RTSDustVoxM = 2.0f / float(VoxStore - 2);
	Out.RTSDustLightInvSize = FVector3f(1.0f / (AtlasBX * 6), 1.0f / (AtlasBY * 6), 1.0f / (AtlasBZ * 6));
	const TSharedPtr<FFrameData, ESPMode::ThreadSafe> D = GFrameRT;
	if (!D || D->Bricks.Num() == 0 || !GKickRHI[D->Kick].IsValid() || !GNoiseRHI.IsValid())
	{
		Out.RTSDustPT = GSystemTextures.GetVolumetricBlackUintDummy(GraphBuilder);
		Out.RTSDustPTCoarse = Out.RTSDustPT;
		Out.RTSDustAtlas = Black;
		Out.RTSDustColor = Black;
		Out.RTSDustLightA = Black;
		Out.RTSDustLightB = Black;
		Out.RTSDustNoise = Black;
		Out.RTSDustCamToWindowM = FVector3f::ZeroVector;
		Out.RTSDustDetail = 0.0f;
		Out.RTSDustTime = 0.0f;
		Out.RTSDustValid = 0.0f;
		Out.RTSDustSunDirW = FVector3f(0.0f, 0.0f, 1.0f);
		OutBoxMinM = OutBoxMaxM = FVector3f::ZeroVector;
		return false;
	}
	RDG_EVENT_SCOPE(GraphBuilder, "RTSDustField");
	const FVector3f CamToWindow = FVector3f((View.ViewMatrices.GetViewOrigin() - D->WindowOriginCm) * 0.01);
	FRDGTextureRef PT = PersistentVolume(GraphBuilder, GPT, FIntVector(WinX, WinY, WinZ), PF_R32_UINT, TEXT("RTSDust.PageTable"));
	FRDGTextureRef PTCoarse = PersistentVolume(GraphBuilder, GPTCoarse, FIntVector(WinX / 4, WinY / 4, WinZ / 4), PF_R32_UINT, TEXT("RTSDust.PageTableCoarse"));
	FRDGTextureRef Atlas = PersistentVolume(GraphBuilder, GAtlas, FIntVector(AtlasBX * VoxStore, AtlasBY * VoxStore, AtlasBZ * VoxStore), PF_R16F, TEXT("RTSDust.Atlas"));
	FRDGTextureRef Color = PersistentVolume(GraphBuilder, GColor, FIntVector(AtlasBX * 6, AtlasBY * 6, AtlasBZ * 6), PF_FloatRGBA, TEXT("RTSDust.Color"));
	FRDGTextureRef LightA = PersistentVolume(GraphBuilder, GLightA, FIntVector(AtlasBX * 6, AtlasBY * 6, AtlasBZ * 6), PF_FloatRGBA, TEXT("RTSDust.LightA"));
	FRDGTextureRef LightB = PersistentVolume(GraphBuilder, GLightB, FIntVector(AtlasBX * 6, AtlasBY * 6, AtlasBZ * 6), PF_FloatRGBA, TEXT("RTSDust.LightB"));
	FRDGTextureRef Kick = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(GKickRHI[D->Kick], TEXT("RTSDust.Kick")));
	const FKickInfo& KR = GKickRT[D->Kick];
	FRDGTextureRef Noise = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(GNoiseRHI, TEXT("RTSDust.Noise")));
	FRDGBufferRef Bricks = CreateStructuredBuffer(GraphBuilder, TEXT("RTSDust.Bricks"), D->Bricks);
	FRDGBufferRef Refresh = CreateStructuredBuffer(GraphBuilder, TEXT("RTSDust.Refresh"), D->Refresh);
	FRDGBufferRef Puffs = CreateStructuredBuffer(GraphBuilder, TEXT("RTSDust.Puffs"), D->Puffs);
	FRDGBufferRef PuffIdx = CreateStructuredBuffer(GraphBuilder, TEXT("RTSDust.PuffIdx"), D->PuffIdx);
	const FGlobalShaderMap* Map = GetGlobalShaderMap(View.GetFeatureLevel());
	const uint32 NumBricks = uint32(D->Bricks.Num()), NumRefresh = uint32(D->Refresh.Num());

	// 1. a tabela de páginas: zera e marca os tijolos deste quadro
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(PT), 0u);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(PTCoarse), 0u);
	{
		FRTSDustScatterCS::FParameters* P = GraphBuilder.AllocParameters<FRTSDustScatterCS::FParameters>();
		P->Bricks = GraphBuilder.CreateSRV(Bricks);
		P->NumBricks = NumBricks;
		P->PageTableOut = GraphBuilder.CreateUAV(PT);
		P->CoarseOut = GraphBuilder.CreateUAV(PTCoarse);
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RTSDust.PageTable"), TShaderMapRef<FRTSDustScatterCS>(Map), P,
			FIntVector(FMath::DivideAndRoundUp(NumBricks, 64u), 1, 1));
	}
	// 2. a densidade dos tijolos refeitos (todos por quadro desde 22/10; os novos sempre). O Dispatch aceita no máximo 65535
	// grupos por eixo (o D3D12) e cada tijolo usa Groups³ (27 no Ultra): com todos refeitos a cada quadro, o Ultra passava disso
	// com mais de 2427 tijolos (~25 tanques andando) e o quadro ficava sem a densidade. Vai em partes, cada uma com a sua fatia
	// da lista dos refeitos (o shader conta do 0); numa parte só, igual a antes
	const uint32 Groups = uint32(FMath::DivideAndRoundUp(VoxStore, 6));
	const uint32 Per = Groups * Groups * Groups;                // grupos de 6³ por tijolo (27 no Ultra, 8 na Alta)
	const uint32 MaxPerPass = FMath::Max(uint32(FMath::Clamp(GRHIMaxDispatchThreadGroupsPerDimension.X, 1, 65535)) / Per, 1u);
	for (uint32 First = 0; First < NumRefresh; First += MaxPerPass)
	{
		const uint32 Num = FMath::Min(NumRefresh - First, MaxPerPass);
		FRTSDustFillCS::FParameters* P = GraphBuilder.AllocParameters<FRTSDustFillCS::FParameters>();
		P->Bricks = GraphBuilder.CreateSRV(Bricks);
		P->Refresh = GraphBuilder.CreateSRV(Num == NumRefresh ? Refresh
			: CreateStructuredBuffer(GraphBuilder, TEXT("RTSDust.RefreshPart"), sizeof(uint32), Num, D->Refresh.GetData() + First,
				uint64(Num) * sizeof(uint32), ERDGInitialDataFlags::None));
		P->Puffs = GraphBuilder.CreateSRV(Puffs);
		P->PuffIdx = GraphBuilder.CreateSRV(PuffIdx);
		P->Kick = Kick;
		P->KickSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		P->KickInvSize = FVector3f(1.0f / KR.TexSize.X, 1.0f / KR.TexSize.Y, 1.0f / KR.TexSize.Z);
		P->KickRes = FVector3f(KR.Res);
		P->KickCols = float(KR.Cols);
		P->KickFrames = float(KR.Frames);
		P->KickVoxM = KR.VoxM;
		P->KickOrigin = KR.Origin;
		P->KickRho2 = KR.bRho2 ? 1.0f : 0.0f;
		P->FillTime = D->Time;
		P->RTSDustNoise = Noise;
		P->RTSDustNoiseSampler = Out.RTSDustNoiseSampler;
		P->AtlasOut = GraphBuilder.CreateUAV(Atlas);
		P->ColorOut = GraphBuilder.CreateUAV(Color);
		P->FillVoxM = Out.RTSDustVoxM;
		P->FillStore = uint32(VoxStore);
		P->FillGroups = Groups;
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RTSDust.Fill"), TShaderMapRef<FRTSDustFillCS>(Map), P,
			FIntVector(int32(Num * Per), 1, 1));
	}
	// os parâmetros de leitura (a luz lê a densidade deste quadro; o raymarch lê tudo)
	Out.RTSDustPT = PT;
	Out.RTSDustPTCoarse = PTCoarse;
	Out.RTSDustAtlas = Atlas;
	Out.RTSDustColor = Color;
	Out.RTSDustLightA = LightA;
	Out.RTSDustLightB = LightB;
	Out.RTSDustNoise = Noise;
	Out.RTSDustCamToWindowM = CamToWindow;
	Out.RTSDustDetail = D->Detail;
	Out.RTSDustTime = D->Time;
	Out.RTSDustValid = 1.0f;
	Out.RTSDustSunDirW = D->ToSun;
	// 3. a luz dos tijolos refeitos: a sombra da poeira até o sol, a do mapa, o céu e as luzes da cena
	if (NumRefresh > 0)
	{
		FRTSDustLightCS::FParameters* P = GraphBuilder.AllocParameters<FRTSDustLightCS::FParameters>();
		P->Bricks = GraphBuilder.CreateSRV(Bricks);
		P->Refresh = GraphBuilder.CreateSRV(Refresh);
		P->Dust = Out;
		P->Dust.RTSDustLightA = Black;                   // escritas neste passe (não lidas)
		P->Dust.RTSDustLightB = Black;
		RTSSunShadow::SetShaderParameters(P->Sun, View.ViewMatrices.GetViewOrigin());
		P->LightAOut = GraphBuilder.CreateUAV(LightA);
		P->LightBOut = GraphBuilder.CreateUAV(LightB);
		P->SunDirW = D->ToSun;
		P->SunStepM = 0.6f;
		P->SunSteps = 12;
		P->TauCap = 60.0f;
		const int32 NL = FMath::Min(Lights.Num(), 8);
		P->NumLights = NL;
		for (int32 K = 0; K < 8; ++K)
		{
			if (K < NL)
			{
				P->LightPosR[K] = FVector4f(Lights[K].PosCamRelM + CamToWindow, Lights[K].RadiusM);
				P->LightCol[K] = FVector4f(Lights[K].Color, 0.0f);
			}
			else
			{
				P->LightPosR[K] = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
				P->LightCol[K] = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
			}
		}
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RTSDust.Light"), TShaderMapRef<FRTSDustLightCS>(Map), P,
			FIntVector(int32(NumRefresh), 1, 1));
	}
	OutBoxMinM = D->BoxMinW - CamToWindow;
	OutBoxMaxM = D->BoxMaxW - CamToWindow;
	return true;
}

// ---- a vista de depuração (rts.DustField.Debug, só pra conferir o campo no FXLab; ver DustFieldDebug.usf) -------------
static TAutoConsoleVariable<int32> CVarDustDebug(TEXT("rts.DustField.Debug"), 0,
	TEXT("A vista de depuração do campo da poeira: 0 desligada, 1 a luz (a conta do raymarch), 2 a densidade, 3 SunT, 4 SkyT, 5 a luz local, 9 só os passes do campo (custo)"),
	ECVF_RenderThreadSafe);
static TAutoConsoleVariable<float> CVarDustDebugStep(TEXT("rts.DustField.DebugStep"), 0.125f, TEXT("O passo da marcha da depuração (m; o voxel é 0,125)"), ECVF_RenderThreadSafe);
// A luz da poeira: os dois ajustes por instância do raymarch das explosões (InstLight2: SunFull, SkyGain), medidos
// 16/10 com dois alvos na luz do jogo (sol a 8°), poeira ÷ chão em volta, nas fotos (depois da curva de tela): ao sol
// 0,90 (a r1) e na sombra do paredão 0,90 (um pouco abaixo do chão sombreado, a sombra se lê; a faixa física é
// 0,75–1,1). A varredura (sw1): céu 1,1 → sombra 0,19 (mancha); 2,6 → 0,60; 3,4 → 0,92; 5,2 → 1,58 (brilha, como os
// cartões, 1,06–1,14). Com o céu 3,4, o sol 0 (× seno da altura) dá 0,85 ao sol e 0,27 dá 1,03. O 0,08 (0,90 na luz do
// jogo) deixava a poeira marrom e opaca na r1 (sol a 25°: 0,77–0,85, cara de fumaça): fica 0,27 (ao sol 1,03, um pouco
// acima do chão como a v26 aprovada, 1,15; na sombra 0,91)
static TAutoConsoleVariable<float> CVarDustDebugSky(TEXT("rts.DustField.DebugSky"), 3.4f, TEXT("O ganho do céu na poeira (2π = o físico)"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarDustDebugOctExp(TEXT("rts.DustField.DebugOctExp"), 1,
	TEXT("As oitavas da depuração: 1 = a sombra exponencial (Wrenninge, o UE), 0 = o lerp do WebGL"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<float> CVarDustDebugSunFull(TEXT("rts.DustField.DebugSunFull"), 0.27f, TEXT("O sol na poeira: 0 = × seno da altura, 1 = inteiro"), ECVF_RenderThreadSafe);

class FRTSDustDebugCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRTSDustDebugCS);
	SHADER_USE_PARAMETER_STRUCT(FRTSDustDebugCS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FRTSDustFieldParameters, Dust)
		SHADER_PARAMETER_STRUCT_INCLUDE(FRTSSunShadowParameters, Sun)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, SceneColorIn)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, SceneDepthIn)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, SceneColorOut)
		SHADER_PARAMETER(FUintVector2, RectMin)
		SHADER_PARAMETER(FUintVector2, RectMax)
		SHADER_PARAMETER(FVector3f, BoxMin)
		SHADER_PARAMETER(FVector3f, BoxMax)
		SHADER_PARAMETER(float, StepM)
		SHADER_PARAMETER(int32, MaxSteps)
		SHADER_PARAMETER(float, SkyGainK)
		SHADER_PARAMETER(float, SunFullK)
		SHADER_PARAMETER(float, GroundAlbedoK)
		SHADER_PARAMETER(float, ShowMode)
		SHADER_PARAMETER(float, OctExp)
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& P) { return IsFeatureLevelSupported(P.Platform, ERHIFeatureLevel::SM5); }
};
IMPLEMENT_GLOBAL_SHADER(FRTSDustDebugCS, "/Plugin/RTSTreadDust/Private/DustFieldDebug.usf", "MainDebug", SF_Compute);

class FRTSDustDebugExtension : public FSceneViewExtensionBase
{
public:
	FRTSDustDebugExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister) {}
	TArray<FRTSDustLight> DebugLightsRT;

	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& View, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override
	{
		if (Pass == EPostProcessingPass::AfterDOF && CVarDustDebug.GetValueOnRenderThread() > 0
			&& !View.bIsSceneCapture && !View.bIsReflectionCapture && !View.bIsPlanarReflection)
		{
			InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateRaw(this, &FRTSDustDebugExtension::Render_RenderThread));
		}
	}

	FScreenPassTexture Render_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs)
	{
		const FScreenPassTexture SceneColor = FScreenPassTexture::CopyFromSlice(GraphBuilder, Inputs.GetInput(EPostProcessMaterialInput::SceneColor));
		const FRDGTextureRef Depth = Inputs.SceneTextures.SceneTextures ? Inputs.SceneTextures.SceneTextures->GetParameters()->SceneDepthTexture : nullptr;
		if (!Depth || !SceneColor.IsValid())
		{
			return SceneColor;
		}
		RDG_EVENT_SCOPE(GraphBuilder, "RTSDustDebug");
		FRTSDustFieldParameters Dust;
		FVector3f BoxMin, BoxMax;
		// as luzes do mundo (m) relativas à câmera desta vista
		TArray<FRTSDustLight> Lights = DebugLightsRT;
		const FVector3f CamM = FVector3f(View.ViewMatrices.GetViewOrigin() * 0.01);
		for (FRTSDustLight& Lt : Lights)
		{
			Lt.PosCamRelM -= CamM;
		}
		if (!RTSDustField::AddPasses(GraphBuilder, View, Lights, Dust, BoxMin, BoxMax) || CVarDustDebug.GetValueOnRenderThread() == 9)
		{
			return SceneColor;                           // 9: só os passes do campo (a medida do custo, sem a marcha daqui)
		}
		FScreenPassRenderTarget Output = Inputs.OverrideOutput;
		FRDGTextureRef OutTex = Output.IsValid() ? Output.Texture : nullptr;
		if (!OutTex || !EnumHasAnyFlags(OutTex->Desc.Flags, TexCreate_UAV))
		{
			FRDGTextureDesc Desc = SceneColor.Texture->Desc;
			Desc.Flags = (Desc.Flags | TexCreate_UAV | TexCreate_ShaderResource) & ~(TexCreate_FastVRAM | TexCreate_DepthStencilTargetable);
			OutTex = GraphBuilder.CreateTexture(Desc, TEXT("RTSDustDebug.SceneColor"));
		}
		const FIntRect R = SceneColor.ViewRect;
		const FIntPoint Extent = SceneColor.Texture->Desc.Extent;
		// fora do retângulo da vista a cena passa igual (a cópia inteira: o resto da textura nova não fica preto)
		AddCopyTexturePass(GraphBuilder, SceneColor.Texture, OutTex);
		FRTSDustDebugCS::FParameters* P = GraphBuilder.AllocParameters<FRTSDustDebugCS::FParameters>();
		P->View = View.ViewUniformBuffer;
		P->Dust = Dust;
		RTSSunShadow::SetShaderParameters(P->Sun, View.ViewMatrices.GetViewOrigin());
		P->SceneColorIn = SceneColor.Texture;
		P->SceneDepthIn = Depth;
		P->SceneColorOut = GraphBuilder.CreateUAV(OutTex);
		P->RectMin = FUintVector2(R.Min.X, R.Min.Y);
		P->RectMax = FUintVector2(R.Max.X, R.Max.Y);
		P->BoxMin = BoxMin;
		P->BoxMax = BoxMax;
		P->StepM = FMath::Clamp(CVarDustDebugStep.GetValueOnRenderThread(), 0.05f, 2.0f);
		P->MaxSteps = 400;
		P->SkyGainK = CVarDustDebugSky.GetValueOnRenderThread();
		P->SunFullK = FMath::Clamp(CVarDustDebugSunFull.GetValueOnRenderThread(), 0.0f, 1.0f);
		P->GroundAlbedoK = 0.35f;
		P->ShowMode = float(CVarDustDebug.GetValueOnRenderThread() - 1);
		P->OctExp = CVarDustDebugOctExp.GetValueOnRenderThread() > 0 ? 1.0f : 0.0f;
		TShaderMapRef<FRTSDustDebugCS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RTSDustDebug.March"), Shader, P,
			FComputeShaderUtils::GetGroupCount(R.Size(), FIntPoint(8, 8)));
		if (Output.IsValid() && Output.Texture != OutTex)
		{
			AddCopyTexturePass(GraphBuilder, OutTex, Output.Texture);
			return FScreenPassTexture(Output);
		}
		return FScreenPassTexture(OutTex, R);
	}
};

namespace
{
	TSharedPtr<FRTSDustDebugExtension, ESPMode::ThreadSafe> GDebugExt;
}

void RTSDustField::EnsureDebugView()
{
	if (!GDebugExt.IsValid() && GEngine)
	{
		GDebugExt = FSceneViewExtensions::NewExtension<FRTSDustDebugExtension>();
	}
}

void FRTSDustFieldModule::ReleaseDebugView()
{
	GDebugExt.Reset();
}

bool RTSDustField::IsDebugViewOn()
{
	return CVarDustDebug.GetValueOnGameThread() > 0;
}

void RTSDustField::SetDebugLights(TArray<FRTSDustLight>&& LightsWorldM)
{
	if (!GDebugExt.IsValid())
	{
		return;
	}
	ENQUEUE_RENDER_COMMAND(RTSDustDebugLights)([Ext = GDebugExt, L = MoveTemp(LightsWorldM)](FRHICommandListImmediate&) mutable
	{
		Ext->DebugLightsRT = MoveTemp(L);
	});
}
