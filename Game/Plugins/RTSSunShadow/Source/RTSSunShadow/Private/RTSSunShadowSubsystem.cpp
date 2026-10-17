#include "RTSSunShadow.h"

#include "Async/Async.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "RenderResource.h"
#include "RHIStaticStates.h"
#include "Async/ParallelFor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "LandscapeProxy.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
#include "RenderingThread.h"
#include "RHIGPUReadback.h"
#include "RHITypes.h"
#include "TextureResource.h"
#include "UObject/UObjectIterator.h"

// O mapa de sombra do sol de todas as fumaças, fogos e poeiras (16/10; a regra do CLAUDE.md: um sistema só; o estudo do
// Fluxo em docs/ESTUDO_LUZ_NA_FUMACA.md; a conta, as revisões e as medidas no FXLab/Plugins/RTSTreadDust/INTEGRACAO.md):
// - a camada estática: numa grade girada pro azimute do sol (cada linha anda no sentido da luz), quatro fotos ortográficas
//   (o chão desenhado; o topo dos obstáculos presos ao chão; o topo e o fundo das copas, este visto de baixo) e uma
//   varredura ao longo de cada linha dão, em cada ponto do chão, até que altura ele está na sombra (e a faixa solta da
//   sombra da copa, com sol por baixo). Exata pra relevo e prédio, sem folga: o chão e a sombra vêm das mesmas fotos. As
//   fotos são em blocos de 256 m (a câmera perto do que fotografa: o corte por distância e o LOD são os do jogo),
//   espalhadas pelos quadros, e a leitura da placa não trava (a placa avisa quando chegou); a varredura do mapa inteiro
//   roda numa thread de fundo;
// - a camada dos veículos: o topo da sombra das duas caixas de cada veículo acima do chão desenhado, com o texel e o giro
//   do mapa, refeita só quando um veículo anda, numa janela que segue a câmera.
// Quem lê: o SunShadow.ush (RTSSunVisibility), num material ou num shader global.

namespace
{
	const float HNone = 1e4f;                    // m: "nada" no topo (−) e no fundo (+) dos obstáculos
	const int32 HTile = 512;                     // texels por bloco de foto (256 m com o texel de 0,5 m)
	const int32 HMaxInFlight = 8;                // blocos esperando a placa ao mesmo tempo
	const int32 HShootPerTick = 4;               // blocos fotografados por quadro (16 fotos pequenas: só no começo)

	bool MeshNameHas(const UStaticMesh* Mesh, const TArray<FString>& Keys)
	{
		if (!Mesh)
		{
			return false;
		}
		const FString Name = Mesh->GetName();
		for (const FString& K : Keys)
		{
			if (!K.IsEmpty() && Name.Contains(K, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	// a varredura: as linhas J0..J1 (as de fora só dão a vizinhança pro alargamento da copa), a saída as OutJ0..OutJ1
	void SweepRows(const URTSSunShadowSubsystem::FHFrame& F, URTSSunShadowSubsystem::FHMaps& M, int32 J0, int32 J1, int32 OutJ0, int32 OutJ1, TArray<FFloat16Color>& Out)
	{
		// cada linha anda no sentido da luz: o topo da sombra presa ao chão desce tan(e) por metro, e cada obstáculo (casa,
		// tronco, duna) sobe ele; a copa (o fundo dela, visto de baixo, a mais de 1 m do chão e acima da sombra) faz uma faixa
		// solta [FB, FT] que desce junto e vira presa quando encosta na sombra do chão. Tudo em m acima da base O.Z
		const int32 N = F.Res;
		const int32 Rows = J1 - J0 + 1;
		TArray<FFloat16Color> Tmp;
		Tmp.SetNumUninitialized(Rows * N);
		const double Drop = F.Texel * 0.01 * F.Tan;
		const double BaseM = F.O.Z * 0.01;
		ParallelFor(Rows, [&](int32 R)
		{
			const int32 J = J0 + R;
			const float* Gr = &M.G[J * N];
			// a linha começa fora do terreno: até o 1º texel com chão nada faz sombra (antes, o chão da mediana virava
			// uma parede na borda do mapa do lado do sol)
			double LastG = 0.0;
			for (int32 I = 0; I < N; ++I)
			{
				if (Gr[I] > -1e29f)
				{
					LastG = Gr[I] * 0.01 - BaseM;
					break;
				}
			}
			double Top = -1e9, FB = 1e9, FT = -1e9;
			bool bIn = false;
			for (int32 I = 0; I < N; ++I)
			{
				const int32 K = J * N + I;
				const bool bValid = Gr[I] > -1e29f;
				const double G = bValid ? Gr[I] * 0.01 - BaseM : LastG;   // buraco no terreno: o chão de antes
				if (bValid)
				{
					LastG = G;
					bIn = true;
				}
				if (!bIn)
				{
					M.Top[K] = FFloat16(0.0f);
					Tmp[R * N + I] = FFloat16Color(FLinearColor(float(G), 0.0f, -1.0f, -1.0f));
					continue;
				}
				Top = FMath::Max(Top - Drop, G);
				FB -= Drop;
				FT -= Drop;
				const double T = float(M.T[K]);
				if (T > G + 0.2)
				{
					Top = FMath::Max(Top, T);             // casa, muro, pedra: presos ao chão
				}
				const double TT = float(M.TT[K]), B = float(M.B[K]);
				if (TT > G + 0.2)
				{
					if (B < HNone * 0.5 && B > G + 1.0 && B > Top + 0.3)
					{
						FB = FMath::Min(FB, B);           // a copa: solta no ar
						FT = FMath::Max(FT, TT);
					}
					else
					{
						Top = FMath::Max(Top, TT);        // o tronco, e a copa que já encosta na sombra
					}
				}
				if (FB < 1e8 && FB <= Top + 0.3)
				{
					Top = FMath::Max(Top, FT);            // a faixa desceu até a sombra do chão: vira uma só
					FB = 1e9;
					FT = -1e9;
				}
				if (FT > -1e8 && FT < G)
				{
					FB = 1e9;                             // a faixa entrou no chão
					FT = -1e9;
				}
				const bool bSlab = FB < 1e8;
				M.Top[K] = FFloat16(float(Top - G));
				Tmp[R * N + I] = FFloat16Color(FLinearColor(float(G), float(Top - G), bSlab ? float(FB - G) : -1.0f, bSlab ? float(FT - G) : -1.0f));
			}
		});
		// o texel sem faixa ao lado de um com faixa leva fundo = topo = o meio dela: a bilinear entre os dois só afina a
		// faixa (com −1 nos dois, saía uma faixa fantasma de ~0,5 m em volta de toda sombra de copa)
		const int32 OutRows = OutJ1 - OutJ0 + 1;
		Out.SetNumUninitialized(OutRows * N);
		ParallelFor(OutRows, [&](int32 Ro)
		{
			const int32 R = OutJ0 - J0 + Ro;
			for (int32 I = 0; I < N; ++I)
			{
				FFloat16Color C = Tmp[R * N + I];
				if (float(C.A) <= float(C.B))
				{
					double Sum = 0.0;
					int32 Cnt = 0;
					for (int32 Dr = -1; Dr <= 1; ++Dr)
					{
						for (int32 Di = -1; Di <= 1; ++Di)
						{
							const int32 R2 = R + Dr, I2 = I + Di;
							if ((Dr || Di) && R2 >= 0 && R2 < Rows && I2 >= 0 && I2 < N)
							{
								const FFloat16Color& Q = Tmp[R2 * N + I2];
								if (float(Q.A) > float(Q.B))
								{
									Sum += 0.5 * (float(Q.A) + float(Q.B));
									++Cnt;
								}
							}
						}
					}
					if (Cnt)
					{
						C.B = FFloat16(float(Sum / Cnt));
						C.A = C.B;
					}
				}
				Out[Ro * N + I] = C;
				if (M.SB.Num() == M.G.Num())
				{
					M.SB[(OutJ0 + Ro) * N + I] = C.B;
					M.SA[(OutJ0 + Ro) * N + I] = C.A;
				}
			}
		});
	}

	// o chão (cm) no ponto (u, v) cm do quadro, bilinear, só com os texels que têm chão; −1e30 fora
	double GroundAt(const URTSSunShadowSubsystem::FHFrame& F, const TArray<float>& G, double U, double V)
	{
		if (F.Res == 0 || G.Num() != F.Res * F.Res)
		{
			return -1e30;
		}
		const double Fi = U / F.Texel + 0.5 * F.Res - 0.5, Fj = V / F.Texel + 0.5 * F.Res - 0.5;
		const int32 I = FMath::FloorToInt32(Fi), J = FMath::FloorToInt32(Fj);
		if (I < 0 || J < 0 || I + 1 >= F.Res || J + 1 >= F.Res)
		{
			return -1e30;
		}
		const double Ti = Fi - I, Tj = Fj - J;
		double Sum = 0.0, Wt = 0.0;
		for (int32 K = 0; K < 4; ++K)
		{
			const float Gk = G[(J + (K >> 1)) * F.Res + I + (K & 1)];
			const double Wk = ((K & 1) ? Ti : 1.0 - Ti) * ((K >> 1) ? Tj : 1.0 - Tj);
			if (Gk > -1e29f)
			{
				Sum += Wk * Gk;
				Wt += Wk;
			}
		}
		return Wt > 1e-6 ? Sum / Wt : -1e30;
	}
}

// um bloco de fotos esperando a placa (dividido com a thread de desenho)
struct FRTSHeightRead
{
	int32 I0 = 0, J0 = 0, Nb = 0;
	TUniquePtr<FRHIGPUTextureReadback> Rb[4];
	TArray<float> Px[4];
	bool bUsed[4] = { false, false, false, false };
	std::atomic<int32> State{ 0 };             // 0 esperando, 1 conferindo, 2 chegou, 3 falhou
};

// o mapa inteiro sendo montado (os quatro mapas novos; a thread de fundo varre e devolve a imagem)
struct FRTSHeightBuilt
{
	URTSSunShadowSubsystem::FHMaps M;
	TArray<FFloat16Color> Out;
	TArray<double> Off;                          // raio − chão da foto, nos pontos do terreno (a calibração)
	std::atomic<bool> bDone{ false };
	double SweepMs = 0.0;
	int32 Shade = 0, Slab = 0;
};

void URTSSunShadowSubsystem::GatherCasters(TArray<TWeakObjectPtr<UPrimitiveComponent>>& Terrain,
	TArray<TWeakObjectPtr<UPrimitiveComponent>>& Obst, TArray<TWeakObjectPtr<UPrimitiveComponent>>& Trees, FBox& Bounds, const AActor* OnlyActor) const
{
	UWorld* World = GetWorld();
	for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
	{
		UPrimitiveComponent* Pc = *It;
		if (!Pc || Pc->GetWorld() != World || !Pc->IsRegistered() || !Pc->IsVisible() || !Pc->CastShadow || Pc->Bounds.SphereRadius > 200000.0)
		{
			continue;
		}
		AActor* Ow = Pc->GetOwner();
		if ((OnlyActor && Ow != OnlyActor) || (Ow && Ow->ActorHasTag(TEXT("RTSNoSunShadow"))))
		{
			continue;
		}
		const bool bLand = Ow && Ow->IsA<ALandscapeProxy>();
		// o prédio que o jogo mandou (AddShadowCaster) entra mesmo com a etiqueta de unidade
		const bool bCaster = Ow && ShadowCasters.Contains(TWeakObjectPtr<AActor>(Ow));
		if (!bCaster && Ow && (Ow->IsA<APawn>() || IgnoreTags.ContainsByPredicate([Ow](const FName& Tg) { return Ow->ActorHasTag(Tg); })))
		{
			continue;
		}
		const UStaticMeshComponent* Sm = Cast<UStaticMeshComponent>(Pc);
		if (!bLand)
		{
			// o mesmo conjunto que faz sombra no chão (a VSM): sombra dinâmica, o ator visível no jogo; só malhas. Os finos
			// saem pelo lado MAIOR < 1 m (o mourão, a pedrinha) ou o menor < 25 cm (o fio): o muro de 0,36–0,48 m da vila
			// fica (a 2ª revisão: o "menor < 1 m" de antes tirava os 416 muros e portões). O poste sai pelo nome: a caixa
			// dele é a da cruzeta (2,2 m), e a cruzeta a 7 m virava uma parede de sombra de 50 m
			if (!Pc->bCastDynamicShadow || (Ow && Ow->IsHidden()) || !Sm || !Sm->GetStaticMesh() || MeshNameHas(Sm->GetStaticMesh(), SkipMeshes))
			{
				continue;
			}
			FVector Sc = Sm->GetComponentScale().GetAbs();
			if (const UInstancedStaticMeshComponent* Ism = Cast<UInstancedStaticMeshComponent>(Sm))
			{
				double MaxS = 0.0;
				for (const FInstancedStaticMeshInstanceData& D : Ism->PerInstanceSMData)
				{
					MaxS = FMath::Max(MaxS, D.Transform.GetMaximumAxisScale());
				}
				if (Ism->PerInstanceSMData.Num() > 0)
				{
					Sc *= MaxS;
				}
			}
			const FVector Ext = Sm->GetStaticMesh()->GetBoundingBox().GetSize() * Sc;
			if (FMath::Max(Ext.X, Ext.Y) < 100.0 || FMath::Min(Ext.X, Ext.Y) < 25.0)
			{
				continue;
			}
		}
		Bounds += Pc->Bounds.GetBox();
		if (bLand)
		{
			Terrain.Add(Pc);
		}
		else if (Sm && MeshNameHas(Sm->GetStaticMesh(), TreeMeshes))
		{
			Trees.Add(Pc);
		}
		else
		{
			Obst.Add(Pc);
		}
	}
}

bool URTSSunShadowSubsystem::StartJob(bool bFull, const FBox& Area)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	HJobT0 = FPlatformTime::Seconds();
	HJobTiles.Reset();
	HJobInFlight.Reset();
	HJobNext = 0;
	if (bFull)
	{
		HSun = SunDir;
		if (SunDir.Z < 0.02 || SunDir.Z > 0.995)
		{
			// o sol no horizonte (ou a pino): sem mapa; o que estava valendo sai (era de outro sol)
			HMapTex = nullptr;
			HCur = FHFrame();
			HM = FHMaps();
			return false;
		}
		for (TArray<TWeakObjectPtr<UPrimitiveComponent>>& L : HCasters)
		{
			L.Reset();
		}
		FBox Bounds(ForceInit);
		GatherCasters(HCasters[0], HCasters[1], HCasters[2], Bounds);
		if (HCasters[0].Num() == 0 || !Bounds.IsValid)
		{
			FailJob(TEXT("sem terreno"));
			return false;
		}
		// o giro: o eixo da direita da câmera de cima, (−sen yaw, cos yaw), no sentido em que a luz anda
		FHFrame F;
		const FVector2D Sh = FVector2D(SunDir.X, SunDir.Y).GetSafeNormal();
		F.Tan = SunDir.Z / FMath::Max(FVector2D(SunDir.X, SunDir.Y).Size(), 1e-3);
		const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Sh.X, -Sh.Y));
		const FRotationMatrix Rt(FRotator(-90.0, Yaw, 0.0));
		const FVector Ry = Rt.GetScaledAxis(EAxis::Y);
		F.U = FVector2D(Ry.X, Ry.Y).GetSafeNormal();
		F.V = FVector2D(-F.U.Y, F.U.X);
		FVector Corner[8];
		Bounds.GetVertices(Corner);
		const FVector2D C2(Bounds.GetCenter().X, Bounds.GetCenter().Y);
		double U0 = 1e30, U1 = -1e30, V0 = 1e30, V1 = -1e30;
		for (const FVector& Cn : Corner)
		{
			const FVector2D D(Cn.X - C2.X, Cn.Y - C2.Y);
			U0 = FMath::Min(U0, FVector2D::DotProduct(D, F.U));
			U1 = FMath::Max(U1, FVector2D::DotProduct(D, F.U));
			V0 = FMath::Min(V0, FVector2D::DotProduct(D, F.V));
			V1 = FMath::Max(V1, FVector2D::DotProduct(D, F.V));
		}
		const double W = FMath::Max(U1 - U0, V1 - V0) + 2000.0;
		F.Texel = FMath::Max(TexelM * 100.0, W / 4096.0);
		F.Res = FMath::Min(4096, Align(FMath::CeilToInt32(W / F.Texel), 4));
		const FVector2D Cc = C2 + F.U * (0.5 * (U0 + U1)) + F.V * (0.5 * (V0 + V1));
		F.ZLo = Bounds.Min.Z;
		F.ZHi = Bounds.Max.Z;
		// a base das alturas e a calibração: os raios que batem no TERRENO (o telhado e a copa inflavam o p75)
		HJobGround.Reset();
		FCollisionQueryParams Qp(SCENE_QUERY_STAT(RTSSunShadowHeight), false);
		for (int32 Gy = 0; Gy < 9; ++Gy)
		{
			for (int32 Gx = 0; Gx < 9; ++Gx)
			{
				const double Qx = FMath::Lerp(Bounds.Min.X, Bounds.Max.X, 0.2 + 0.075 * Gx), Qy = FMath::Lerp(Bounds.Min.Y, Bounds.Max.Y, 0.2 + 0.075 * Gy);
				FHitResult Hit;
				if (World->LineTraceSingleByChannel(Hit, FVector(Qx, Qy, Bounds.Max.Z + 100.0), FVector(Qx, Qy, Bounds.Min.Z - 100.0), ECC_Visibility, Qp)
					&& Hit.GetActor() && Hit.GetActor()->IsA<ALandscapeProxy>())
				{
					HJobGround.Add(Hit.ImpactPoint);
				}
			}
		}
		TArray<double> Zs;
		for (const FVector& G : HJobGround)
		{
			Zs.Add(G.Z);
		}
		Zs.Sort();
		F.O = FVector(Cc.X, Cc.Y, Zs.Num() ? Zs[Zs.Num() / 2] : Bounds.GetCenter().Z);
		F.Id = ++HFrameIds;
		HJobFrame = F;
		// os mapas novos (os de agora seguem valendo até o fim)
		HJobBuilt = MakeShared<FRTSHeightBuilt, ESPMode::ThreadSafe>();
		const int32 N = F.Res;
		FHMaps& M = HJobBuilt->M;
		M.G.Init(-1e30f, N * N);
		M.T.Init(FFloat16(-HNone), N * N);
		M.TT.Init(FFloat16(-HNone), N * N);
		M.B.Init(FFloat16(HNone), N * N);
		M.Top.Init(FFloat16(0.0f), N * N);
		M.SB.Init(FFloat16(-1.0f), N * N);
		M.SA.Init(FFloat16(-1.0f), N * N);
		const int32 Nt = FMath::Min(HTile, N);
		for (int32 J = 0; J < N; J += Nt)
		{
			for (int32 I = 0; I < N; I += Nt)
			{
				HJobTiles.Add(FIntPoint(FMath::Min(I, N - Nt), FMath::Min(J, N - Nt)));
			}
		}
		HJobNb = Nt;
		HJobKind = 1;
	}
	else
	{
		// uma área: um bloco só, centrado nela, no quadro de agora
		if (HCur.Res == 0 || !HMapTex)
		{
			return false;
		}
		HJobFrame = HCur;
		const int32 N = HCur.Res;
		const int32 Nt = FMath::Min(HTile, N);
		double Fi0 = 1e30, Fi1 = -1e30, Fj0 = 1e30, Fj1 = -1e30;
		for (int32 K = 0; K < 4; ++K)
		{
			const FVector2D Pc((K & 1) ? Area.Max.X : Area.Min.X, (K & 2) ? Area.Max.Y : Area.Min.Y);
			const FVector2D D = Pc - FVector2D(HCur.O.X, HCur.O.Y);
			const double Fi = FVector2D::DotProduct(D, HCur.U) / HCur.Texel + 0.5 * N, Fj = FVector2D::DotProduct(D, HCur.V) / HCur.Texel + 0.5 * N;
			Fi0 = FMath::Min(Fi0, Fi); Fi1 = FMath::Max(Fi1, Fi);
			Fj0 = FMath::Min(Fj0, Fj); Fj1 = FMath::Max(Fj1, Fj);
		}
		// o que muda de fato (com 2 texels de folga) e o bloco da foto em volta
		HJobPatch = FIntRect(FMath::Clamp(FMath::FloorToInt32(Fi0) - 2, 0, N - 1), FMath::Clamp(FMath::FloorToInt32(Fj0) - 2, 0, N - 1),
			FMath::Clamp(FMath::CeilToInt32(Fi1) + 2, 1, N), FMath::Clamp(FMath::CeilToInt32(Fj1) + 2, 1, N));
		const int32 I0 = FMath::Clamp((HJobPatch.Min.X + HJobPatch.Max.X) / 2 - Nt / 2, 0, N - Nt);
		const int32 J0 = FMath::Clamp((HJobPatch.Min.Y + HJobPatch.Max.Y) / 2 - Nt / 2, 0, N - Nt);
		HJobPatch.Clip(FIntRect(I0, J0, I0 + Nt, J0 + Nt));
		HJobTiles.Add(FIntPoint(I0, J0));
		HJobNb = Nt;
		HJobKind = 2;
	}
	// os 4 alvos (um por foto), do tamanho do bloco; a placa copia cada foto antes da próxima escrever no mesmo alvo
	HJobRT.SetNum(4);
	for (int32 S = 0; S < 4; ++S)
	{
		if (!HJobRT[S] || HJobRT[S]->SizeX != HJobNb)
		{
			UTextureRenderTarget2D* Rt = NewObject<UTextureRenderTarget2D>(this);
			Rt->RenderTargetFormat = RTF_R32f;
			Rt->ClearColor = FLinearColor(1e9f, 1e9f, 1e9f, 1.0f);
			Rt->InitAutoFormat(HJobNb, HJobNb);
			Rt->UpdateResourceImmediate(true);
			HJobRT[S] = Rt;
		}
	}
	if (!HeightCapture)
	{
		HeightCapture = NewObject<USceneCaptureComponent2D>(this, TEXT("RTSSunShadowCapture"));
		HeightCapture->bCaptureEveryFrame = false;
		HeightCapture->bCaptureOnMovement = false;
		HeightCapture->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;
		HeightCapture->ProjectionType = ECameraProjectionMode::Orthographic;
		HeightCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		HeightCapture->LODDistanceFactor = 0.01f;   // o LOD de perto (o desenhado na câmera do jogo), não o de 256 m
		HeightCapture->RegisterComponentWithWorld(World);
	}
	return true;
}

void URTSSunShadowSubsystem::ShootJob(int32 Tile)
{
	// as 4 fotos de um bloco (o chão, os obstáculos, o topo e o fundo das copas) e a cópia pra leitura, sem esperar
	const FHFrame& F = HJobFrame;
	const int32 N = F.Res, Nb = HJobNb;
	TSharedPtr<FRTSHeightRead, ESPMode::ThreadSafe> Read = MakeShared<FRTSHeightRead, ESPMode::ThreadSafe>();
	Read->I0 = HJobTiles[Tile].X;
	Read->J0 = HJobTiles[Tile].Y;
	Read->Nb = Nb;
	const double Uc = (Read->I0 + 0.5 * Nb - 0.5 * N) * F.Texel, Vc = (Read->J0 + 0.5 * Nb - 0.5 * N) * F.Texel;
	const FVector2D C = FVector2D(F.O.X, F.O.Y) + F.U * Uc + F.V * Vc;
	const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(-F.U.X, F.U.Y));
	const double CamTop = F.ZHi + 10000.0, CamBot = F.ZLo - 10000.0;
	HeightCapture->OrthoWidth = Nb * F.Texel;
	for (int32 S = 0; S < 4; ++S)
	{
		const TArray<TWeakObjectPtr<UPrimitiveComponent>>& Show = S == 0 ? HCasters[0] : S == 1 ? HCasters[1] : HCasters[2];
		Read->bUsed[S] = Show.Num() > 0;
		if (!Read->bUsed[S])
		{
			continue;
		}
		const bool bBelow = S == 3;
		HeightCapture->TextureTarget = HJobRT[S];
		HeightCapture->ShowOnlyComponents = Show;
		HeightCapture->SetWorldLocationAndRotation(FVector(C.X, C.Y, bBelow ? CamBot : CamTop), FRotator(bBelow ? 90.0 : -90.0, Yaw, 0.0));
		HeightCapture->CaptureScene();
		Read->Rb[S] = MakeUnique<FRHIGPUTextureReadback>(TEXT("RTSSunShadowHeight"));
		FTextureRenderTargetResource* Res = HJobRT[S]->GameThread_GetRenderTargetResource();
		ENQUEUE_RENDER_COMMAND(RTSSunShadowHeightCopy)([Read, S, Res, Nb](FRHICommandListImmediate& RHICmdList)
		{
			if (Res && Res->GetRenderTargetTexture())
			{
				Read->Rb[S]->EnqueueCopy(RHICmdList, Res->GetRenderTargetTexture(), FIntVector::ZeroValue, 0, FIntVector(Nb, Nb, 1));
			}
		});
	}
	HeightCapture->TextureTarget = nullptr;
	HJobInFlight.Add(Read);
}

void URTSSunShadowSubsystem::TickHeightMap()
{
	if (HJobKind == 0)
	{
		const bool bSunMoved = HCur.Res > 0 && FVector::DotProduct(SunDir, HSun) < FMath::Cos(FMath::DegreesToRadians(0.5));
		if (bHMapDirty || bSunMoved)
		{
			if (HMapWaitTicks > 0)
			{
				--HMapWaitTicks;
				return;
			}
			if (bSunMoved && !bHMapDirty)
			{
				HMapTries = 0;                        // o sol mudou: começa de novo
				HPrevP75 = 1e9;
			}
			bHMapDirty = false;
			StartJob(true, FBox(ForceInit));
			return;
		}
		if (HPatches.Num() > 0 && HMapTex)
		{
			if (HPatchWait > 0)
			{
				--HPatchWait;
				return;
			}
			// as áreas da fila que cabem num bloco de 240 m viram uma foto só
			FBox U = HPatches[0];
			TArray<FBox> Rest;
			for (int32 K = 1; K < HPatches.Num(); ++K)
			{
				const FBox T = U + HPatches[K];
				if (T.GetSize().X <= 24000.0 && T.GetSize().Y <= 24000.0)
				{
					U = T;
				}
				else
				{
					Rest.Add(HPatches[K]);
				}
			}
			HPatches = MoveTemp(Rest);
			StartJob(false, U);
		}
		return;
	}
	if (HJobKind == 3)
	{
		if (HJobBuilt && HJobBuilt->bDone.load())
		{
			FinishFullJob();
		}
		return;
	}
	// fotografando: até 4 blocos novos por quadro (até 8 esperando a placa)
	for (int32 K = 0; K < HShootPerTick && HJobNext < HJobTiles.Num() && HJobInFlight.Num() < HMaxInFlight; ++K)
	{
		ShootJob(HJobNext++);
	}
	if (HJobInFlight.Num() == 0)
	{
		return;
	}
	// a placa terminou as cópias? (confere todos de uma vez na thread de desenho e copia pra CPU lá; aqui só olha as
	// bandeiras). Os que chegaram viram alturas neste quadro
	TArray<TSharedPtr<FRTSHeightRead, ESPMode::ThreadSafe>> Ask;
	for (const TSharedPtr<FRTSHeightRead, ESPMode::ThreadSafe>& Rd : HJobInFlight)
	{
		if (Rd->State.load() == 0)
		{
			Rd->State.store(1);
			Ask.Add(Rd);
		}
	}
	if (Ask.Num() > 0)
	{
		ENQUEUE_RENDER_COMMAND(RTSSunShadowHeightPoll)([Ask](FRHICommandListImmediate& RHICmdList)
		{
			for (const TSharedPtr<FRTSHeightRead, ESPMode::ThreadSafe>& Head : Ask)
			{
				bool bReady = true;
				for (int32 S = 0; S < 4; ++S)
				{
					bReady = bReady && (!Head->bUsed[S] || (Head->Rb[S] && Head->Rb[S]->IsReady()));
				}
				if (!bReady)
				{
					Head->State.store(0);
					continue;
				}
				const int32 Nb = Head->Nb;
				bool bOk = true;
				for (int32 S = 0; S < 4 && bOk; ++S)
				{
					if (!Head->bUsed[S])
					{
						continue;
					}
					int32 Pitch = 0;
					const float* Src = static_cast<const float*>(Head->Rb[S]->Lock(Pitch));
					if (!Src || Pitch < Nb)
					{
						if (Src)
						{
							Head->Rb[S]->Unlock();
						}
						bOk = false;
						break;
					}
					Head->Px[S].SetNumUninitialized(Nb * Nb);
					for (int32 Y = 0; Y < Nb; ++Y)
					{
						FMemory::Memcpy(&Head->Px[S][Y * Nb], Src + int64(Y) * Pitch, Nb * sizeof(float));
					}
					Head->Rb[S]->Unlock();
					Head->Rb[S].Reset();
				}
				Head->State.store(bOk ? 2 : 3);
			}
		});
	}
	for (int32 K = 0; K < HJobInFlight.Num(); ++K)
	{
		const int32 St = HJobInFlight[K]->State.load();
		if (St == 3)
		{
			FailJob(TEXT("a leitura da placa falhou"));
			return;
		}
		if (St == 2)
		{
			TSharedPtr<FRTSHeightRead, ESPMode::ThreadSafe> Head = HJobInFlight[K];
			HJobInFlight.RemoveAt(K--);
			ApplyTile(*Head);
			if (HJobKind == 0)
			{
				return;                               // a área acabou (FinishPatchJob)
			}
		}
	}
	if (FPlatformTime::Seconds() - HJobT0 > 30.0)
	{
		FailJob(TEXT("a placa não devolveu as fotos em 30 s"));
		return;
	}
	if (HJobKind != 1 || HJobNext < HJobTiles.Num() || HJobInFlight.Num() > 0)
	{
		return;
	}
	StartFullSweep();
}

void URTSSunShadowSubsystem::ApplyTile(const FRTSHeightRead& Rd)
{
	const FRTSHeightRead* Head = &Rd;
	// o bloco chegou: as fotos viram alturas (os mapas novos do mapa inteiro, ou direto nos de agora numa área)
	FHMaps& M = HJobKind == 1 ? HJobBuilt->M : HM;   // (o bloco: Head)
	const FHFrame& F = HJobFrame;
	const int32 N = F.Res, Nb = Head->Nb;
	const double CamTop = F.ZHi + 10000.0, CamBot = F.ZLo - 10000.0;
	const float LowCm = 50.0f;                    // o obstáculo mais baixo que isso (laje, calçada, pedrinha) é chão
	const double TopMax = CamTop - (F.ZLo - 2000.0), BotMax = (F.ZHi + 2000.0) - CamBot;
	const float BaseM = float(F.O.Z * 0.01);
	// numa área só muda o que está nela (o resto do bloco foi fotografado só pra caber)
	const FIntRect Box = HJobKind == 2 ? HJobPatch : FIntRect(Head->I0, Head->J0, Head->I0 + Nb, Head->J0 + Nb);
	ParallelFor(Box.Height(), [&](int32 Rj)
	{
		const int32 J = Box.Min.Y + Rj, Jl = J - Head->J0;
		for (int32 I = Box.Min.X; I < Box.Max.X; ++I)
		{
			const int32 Il = I - Head->I0, K = J * N + I;
			if (Head->bUsed[0])
			{
				const float D = Head->Px[0][Jl * Nb + Il];
				M.G[K] = D > 0.0f && D < TopMax ? float(CamTop - D) : -1e30f;
			}
			M.T[K] = FFloat16(-HNone);
			if (Head->bUsed[1])
			{
				const float D = Head->Px[1][Jl * Nb + Il];
				if (D > 0.0f && D < TopMax)
				{
					const float Zt = float(CamTop - D);
					if (M.G[K] > -1e29f && Zt < M.G[K] + LowCm)
					{
						// a laje do pátio, a calçada: o chão desenhado é o de cima (a poeira em cima dela mede a altura
						// dali, e ela não faz sombra de 30 cm "dentro" da nuvem)
						M.G[K] = FMath::Max(M.G[K], Zt);
					}
					else
					{
						M.T[K] = FFloat16(Zt * 0.01f - BaseM);
					}
				}
			}
			if (Head->bUsed[2])
			{
				const float D = Head->Px[2][Jl * Nb + Il];
				M.TT[K] = FFloat16(D > 0.0f && D < TopMax ? float(CamTop - D) * 0.01f - BaseM : -HNone);
			}
			else
			{
				M.TT[K] = FFloat16(-HNone);
			}
			if (Head->bUsed[3])
			{
				// a foto de baixo sai espelhada em v (o eixo de cima da câmera vira): a linha j é a Nb − 1 − j
				const float D = Head->Px[3][(Nb - 1 - Jl) * Nb + Il];
				M.B[K] = FFloat16(D > 0.0f && D < BotMax ? float(CamBot + D) * 0.01f - BaseM : HNone);
			}
			else
			{
				M.B[K] = FFloat16(HNone);
			}
		}
	});
	if (HJobKind == 2)
	{
		FinishPatchJob();
	}
}

void URTSSunShadowSubsystem::StartFullSweep()
{
	// todos os blocos chegaram: a varredura numa thread de fundo
	HJobTShots = FPlatformTime::Seconds();
	HJobKind = 3;
	for (TObjectPtr<UTextureRenderTarget2D>& Rt : HJobRT)
	{
		if (Rt)
		{
			Rt->ReleaseResource();
		}
	}
	HJobRT.Reset();
	TSharedPtr<FRTSHeightBuilt, ESPMode::ThreadSafe> Built = HJobBuilt;
	const FHFrame Fr = HJobFrame;
	const TArray<FVector> Ground = HJobGround;
	Async(EAsyncExecution::ThreadPool, [Built, Fr, Ground]()
	{
		const double T0 = FPlatformTime::Seconds();
		SweepRows(Fr, Built->M, 0, Fr.Res - 1, 0, Fr.Res - 1, Built->Out);
		for (const FVector& G : Ground)
		{
			const FVector2D D(G.X - Fr.O.X, G.Y - Fr.O.Y);
			const double Gh = GroundAt(Fr, Built->M.G, FVector2D::DotProduct(D, Fr.U), FVector2D::DotProduct(D, Fr.V));
			if (Gh > -1e29)
			{
				Built->Off.Add(G.Z - Gh);
			}
		}
		for (const FFloat16Color& O : Built->Out)
		{
			Built->Shade += float(O.G) > 0.05f ? 1 : 0;
			Built->Slab += float(O.A) > float(O.B) + 0.05f ? 1 : 0;
		}
		Built->SweepMs = (FPlatformTime::Seconds() - T0) * 1000.0;
		Built->bDone.store(true);
	});
}

void URTSSunShadowSubsystem::FinishFullJob()
{
	// o mapa novo entra inteiro de uma vez (o quadro, os mapas e a textura): até aqui valia o de antes
	TSharedPtr<FRTSHeightBuilt, ESPMode::ThreadSafe> Built = HJobBuilt;
	HJobBuilt.Reset();
	HJobKind = 0;
	const double T0 = FPlatformTime::Seconds();
	HCur = HJobFrame;
	HM = MoveTemp(Built->M);
	const int32 N = HCur.Res;
	// a calibração: o chão da foto contra os raios no terreno. A profundidade ortográfica da Unreal não tem constante
	// (a 2ª revisão: o plano de perto fica no 0), então nada é somado: a mediana e o p75 só dizem se o relevo em Nanite
	// já carregou (o p75 alto manda refazer)
	TArray<double>& Off = Built->Off;
	Off.Sort();
	HCalMed = Off.Num() ? Off[Off.Num() / 2] : 0.0;
	TArray<double> Dev;
	for (double O : Off)
	{
		Dev.Add(FMath::Abs(O - HCalMed));
	}
	Dev.Sort();
	HCalP75 = Dev.Num() ? Dev[(Dev.Num() * 3) / 4] : 1e9;
	if (FParse::Param(FCommandLine::Get(), TEXT("TreadHMapDump")) || FParse::Param(FCommandLine::Get(), TEXT("SunShadowDump")))
	{
		DumpMap(Built->Out);
	}
	// a textura: a mesma se o tamanho não mudou (a imagem nova sobe por cima), senão uma nova, criada sem dados e cheia
	// uma vez só
	if (HMapTex && HMapTex->GetSizeX() == N && HMapTex->GetSizeY() == N)
	{
		TArray<FFloat16Color> Out = MoveTemp(Built->Out);
		UploadRows(0, N - 1, MoveTemp(Out));
	}
	else
	{
		UTexture2D* T = UTexture2D::CreateTransient(N, N, PF_FloatRGBA);
		if (T)
		{
			T->SRGB = false;
			T->CompressionSettings = TC_HDR;
			T->Filter = TF_Bilinear;
			T->AddressX = TA_Clamp;
			T->AddressY = TA_Clamp;
			T->NeverStream = true;
			FTexture2DMipMap& Mip = T->GetPlatformData()->Mips[0];
			void* Dst = Mip.BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(Dst, Built->Out.GetData(), int64(Built->Out.Num()) * sizeof(FFloat16Color));
			Mip.BulkData.Unlock();
			T->UpdateResource();
		}
		HMapTex = T;
	}
	HMapFails = 0;
	UE_LOG(LogTemp, Display, TEXT("RTSSunShadow: o mapa do sol: %d×%d de %.0f cm (%.0f m), %d peças do terreno, %d obstáculos e %d árvores, %d prédios mandados; sol a %.1f° (tan %.3f), base %.0f cm; chão da foto × raios no terreno: mediana %.1f cm, p75 %.1f cm (%d raios); %.1f%% na sombra, %.2f%% com copa; fotos %.0f ms em %d blocos (sem travar), varredura %.0f ms (fundo), textura %.0f ms"),
		N, N, HCur.Texel, N * HCur.Texel / 100.0, HCasters[0].Num(), HCasters[1].Num(), HCasters[2].Num(), ShadowCasters.Num(),
		FMath::RadiansToDegrees(FMath::Atan(HCur.Tan)), HCur.Tan, HCur.O.Z, HCalMed, HCalP75, Off.Num(),
		100.0 * Built->Shade / FMath::Max(1, N * N), 100.0 * Built->Slab / FMath::Max(1, N * N),
		(HJobTShots - HJobT0) * 1000.0, HJobTiles.Num(), Built->SweepMs, (FPlatformTime::Seconds() - T0) * 1000.0);
	// o relevo ainda carregando: refaz só se melhorou bem desde a última (até 3 vezes); a foto de agora já vale
	if (HCalP75 > 15.0 && HMapTries < 3 && HCalP75 < 0.7 * HPrevP75)
	{
		++HMapTries;
		HPrevP75 = HCalP75;
		bHMapDirty = true;
		HMapWaitTicks = 90;
		UE_LOG(LogTemp, Display, TEXT("RTSSunShadow: o chão da foto ainda não bate com os raios (p75 %.1f cm): refaço (%d/3)"), HCalP75, HMapTries);
	}
}

void URTSSunShadowSubsystem::FinishPatchJob()
{
	// a área: as linhas que cruzam ela (a sombra corre pra frente até o fim da linha), com uma de cada lado só de
	// vizinhança pro alargamento da copa; sobe só as linhas da área
	HJobKind = 0;
	HPatchWait = 30;
	if (HJobFrame.Id != HCur.Id)
	{
		return;                                   // o mapa inteiro foi refeito no meio: a área já entrou nele
	}
	const double T0 = FPlatformTime::Seconds();
	const int32 N = HCur.Res;
	const int32 Ja = HJobPatch.Min.Y, Jb = HJobPatch.Max.Y - 1;
	const int32 Js0 = FMath::Max(0, Ja - 1), Js1 = FMath::Min(N - 1, Jb + 1);
	TArray<FFloat16Color> Out;
	SweepRows(HCur, HM, Js0, Js1, Ja, Jb, Out);
	UploadRows(Ja, Jb, MoveTemp(Out));
	UE_LOG(LogTemp, Display, TEXT("RTSSunShadow: sombra refeita numa área de %d×%d texels (linhas %d–%d): fotos e leitura %.0f ms sem travar, varredura %.1f ms"),
		HJobPatch.Width(), HJobPatch.Height(), Ja, Jb, (T0 - HJobT0) * 1000.0, (FPlatformTime::Seconds() - T0) * 1000.0);
}

void URTSSunShadowSubsystem::FailJob(const TCHAR* Why)
{
	// o que estava valendo continua inteiro (o quadro, os mapas e a textura só trocam no fim de uma foto boa)
	const bool bFull = HJobKind == 1 || HJobKind == 3 || HJobKind == 0;
	HJobKind = 0;
	HJobInFlight.Reset();
	HJobBuilt.Reset();
	if (!bFull)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTSSunShadow: a área da sombra não foi refeita (%s)"), Why);
		return;
	}
	++HMapFails;
	if (HMapFails >= 3)
	{
		bHMapGaveUp = true;                       // desiste: quem lê volta pro jeito de antes (HasGivenUp)
		UE_LOG(LogTemp, Warning, TEXT("RTSSunShadow: altura da sombra por pixel falhou 3 vezes (%s): a sombra fica por cartão"), Why);
		return;
	}
	bHMapDirty = true;
	HMapWaitTicks = 120;
	UE_LOG(LogTemp, Warning, TEXT("RTSSunShadow: altura da sombra por pixel falhou (%s): tento de novo em ~2 s (%d/3)"), Why, HMapFails);
}

void URTSSunShadowSubsystem::UploadRows(int32 J0, int32 J1, TArray<FFloat16Color>&& Rows)
{
	const int32 N = HCur.Res;
	if (!HMapTex || J1 < J0 || Rows.Num() != (J1 - J0 + 1) * N)
	{
		return;
	}
	TArray<FFloat16Color>* Buf = new TArray<FFloat16Color>(MoveTemp(Rows));
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, J0, 0, 0, N, J1 - J0 + 1);
	HMapTex->UpdateTextureRegions(0, 1, Region, N * sizeof(FFloat16Color), sizeof(FFloat16Color), reinterpret_cast<uint8*>(Buf->GetData()),
		[Buf, Region](uint8*, const FUpdateTextureRegion2D*) { delete Buf; delete Region; });
}

void URTSSunShadowSubsystem::DumpMap(const TArray<FFloat16Color>& Out)
{
	// depuração (-TreadHMapDump): os 4 canais (m) em float32 e o quadro, em Saved/; e a conferência contra o chão
	// desenhado: uma foto de cima COM luz (a sombra da VSM no chão) de 256 m em volta do ponto que a câmera olha, no
	// mesmo giro, 2 pixels por texel, com o bloco de texels dela
	const int32 N = HCur.Res;
	if (Out.Num() == N * N)
	{
		TArray<float> Raw;
		Raw.SetNumUninitialized(Out.Num() * 4);
		for (int32 I = 0; I < Out.Num(); ++I)
		{
			const FLinearColor L = Out[I].GetFloats();
			Raw[I * 4 + 0] = L.R; Raw[I * 4 + 1] = L.G; Raw[I * 4 + 2] = L.B; Raw[I * 4 + 3] = L.A;
		}
		FFileHelper::SaveArrayToFile(TArrayView<const uint8>(reinterpret_cast<const uint8*>(Raw.GetData()), Raw.Num() * sizeof(float)),
			*(FPaths::ProjectSavedDir() / TEXT("hmap.raw")));
	}
	const FVector Fc = FocusCm();
	const FVector2D Fr = FVector2D(Fc.X, Fc.Y) - FVector2D(HCur.O.X, HCur.O.Y);
	const int32 Nb = FMath::Min(HTile, N), Px = Nb * 2;
	const int32 I0 = FMath::Clamp(FMath::RoundToInt32(FVector2D::DotProduct(Fr, HCur.U) / HCur.Texel + 0.5 * N) - Nb / 2, 0, N - Nb);
	const int32 J0 = FMath::Clamp(FMath::RoundToInt32(FVector2D::DotProduct(Fr, HCur.V) / HCur.Texel + 0.5 * N) - Nb / 2, 0, N - Nb);
	FFileHelper::SaveStringToFile(FString::Printf(TEXT("res %d\ntexel %.3f\no %.2f %.2f %.2f\nu %.6f %.6f\nv %.6f %.6f\ntan %.6f\nlit %d %d %d %d\n"),
		N, HCur.Texel, HCur.O.X, HCur.O.Y, HCur.O.Z, HCur.U.X, HCur.U.Y, HCur.V.X, HCur.V.Y, HCur.Tan, I0, J0, Nb, Px),
		*(FPaths::ProjectSavedDir() / TEXT("hmap_meta.txt")));
	UTextureRenderTarget2D* Rt = NewObject<UTextureRenderTarget2D>(this);
	Rt->RenderTargetFormat = RTF_RGBA16f;
	Rt->InitAutoFormat(Px, Px);
	Rt->UpdateResourceImmediate(true);
	USceneCaptureComponent2D* Lit = NewObject<USceneCaptureComponent2D>(this);
	Lit->bCaptureEveryFrame = false;
	Lit->bCaptureOnMovement = false;
	Lit->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	Lit->ProjectionType = ECameraProjectionMode::Orthographic;
	Lit->OrthoWidth = Nb * HCur.Texel;
	Lit->TextureTarget = Rt;
	Lit->RegisterComponentWithWorld(GetWorld());
	const double Uc = (I0 + 0.5 * Nb - 0.5 * N) * HCur.Texel, Vc = (J0 + 0.5 * Nb - 0.5 * N) * HCur.Texel;
	const FVector2D C = FVector2D(HCur.O.X, HCur.O.Y) + HCur.U * Uc + HCur.V * Vc;
	Lit->SetWorldLocationAndRotation(FVector(C.X, C.Y, HCur.ZHi + 10000.0), FRotator(-90.0, FMath::RadiansToDegrees(FMath::Atan2(-HCur.U.X, HCur.U.Y)), 0.0));
	Lit->CaptureScene();
	TArray<FLinearColor> Pix;
	if (Rt->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Pix, FReadSurfaceDataFlags(RCM_MinMax)) && Pix.Num() == Px * Px)
	{
		TArray<float> Rgb;
		Rgb.SetNumUninitialized(Pix.Num() * 3);
		for (int32 I = 0; I < Pix.Num(); ++I)
		{
			Rgb[I * 3 + 0] = Pix[I].R; Rgb[I * 3 + 1] = Pix[I].G; Rgb[I * 3 + 2] = Pix[I].B;
		}
		FFileHelper::SaveArrayToFile(TArrayView<const uint8>(reinterpret_cast<const uint8*>(Rgb.GetData()), Rgb.Num() * sizeof(float)),
			*(FPaths::ProjectSavedDir() / TEXT("hmap_lit.raw")));
	}
	// a mesma foto com a BaseColor (a sombra = o brilho ÷ a cor: o asfalto e a pedra escura não contam como sombra)
	Lit->CaptureSource = ESceneCaptureSource::SCS_BaseColor;
	Lit->CaptureScene();
	if (Rt->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Pix, FReadSurfaceDataFlags(RCM_MinMax)) && Pix.Num() == Px * Px)
	{
		TArray<float> Rgb;
		Rgb.SetNumUninitialized(Pix.Num() * 3);
		for (int32 I = 0; I < Pix.Num(); ++I)
		{
			Rgb[I * 3 + 0] = Pix[I].R; Rgb[I * 3 + 1] = Pix[I].G; Rgb[I * 3 + 2] = Pix[I].B;
		}
		FFileHelper::SaveArrayToFile(TArrayView<const uint8>(reinterpret_cast<const uint8*>(Rgb.GetData()), Rgb.Num() * sizeof(float)),
			*(FPaths::ProjectSavedDir() / TEXT("hmap_albedo.raw")));
	}
	// e o que tem obstáculo ou copa em cima (o telhado e a copa vistos de cima não são o chão)
	TArray<uint8> Cover;
	Cover.SetNumZeroed(Nb * Nb);
	for (int32 J = 0; J < Nb; ++J)
	{
		for (int32 I = 0; I < Nb; ++I)
		{
			const int32 K = (J0 + J) * N + I0 + I;
			const float Gm = HM.G[K] > -1e29f ? HM.G[K] * 0.01f - float(HCur.O.Z * 0.01) : 0.0f;
			Cover[J * Nb + I] = (float(HM.T[K]) > Gm + 0.3f ? 1 : 0) | (float(HM.TT[K]) > Gm + 0.3f ? 2 : 0);
		}
	}
	FFileHelper::SaveArrayToFile(Cover, *(FPaths::ProjectSavedDir() / TEXT("hmap_cover.raw")));
	Lit->DestroyComponent();
	Rt->ReleaseResource();
	UE_LOG(LogTemp, Display, TEXT("RTSSunShadow: depuração da altura da sombra em Saved/ (hmap.raw, hmap_meta.txt, hmap_lit.raw: o bloco %d,%d de %d texels)"), I0, J0, Nb);
}

void URTSSunShadowSubsystem::AddShadowCaster(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	ShadowCasters.Add(Actor);
	// as peças dele entram na lista das fotos (a área é refeita logo)
	FBox Bounds(ForceInit);
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Terrain, Obst, Trees;
	GatherCasters(Terrain, Obst, Trees, Bounds, Actor);
	for (const TWeakObjectPtr<UPrimitiveComponent>& C : Obst)
	{
		HCasters[1].AddUnique(C);
	}
	for (const TWeakObjectPtr<UPrimitiveComponent>& C : Trees)
	{
		HCasters[2].AddUnique(C);
	}
	FVector C, E;
	Actor->GetActorBounds(false, C, E);
	RefreshShadowArea(C / 100.0, FMath::Max(E.X, E.Y) / 100.0 + 2.0);
}

void URTSSunShadowSubsystem::RemoveShadowCaster(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	ShadowCasters.Remove(Actor);
	for (TArray<TWeakObjectPtr<UPrimitiveComponent>>& L : HCasters)
	{
		L.RemoveAll([Actor](const TWeakObjectPtr<UPrimitiveComponent>& C) { return !C.IsValid() || C->GetOwner() == Actor; });
	}
	FVector C, E;
	Actor->GetActorBounds(false, C, E);
	RefreshShadowArea(C / 100.0, FMath::Max(E.X, E.Y) / 100.0 + 2.0);
}

void URTSSunShadowSubsystem::RefreshShadowArea(const FVector& CenterM, double RadiusM)
{
	if (HCur.Res == 0 || bHMapDirty || HJobKind == 1 || HJobKind == 3)
	{
		return;                                   // o mapa inteiro ainda vai ser tirado: já entra
	}
	const FVector C = CenterM * 100.0;
	const double R = FMath::Clamp(RadiusM, 1.0, 100.0) * 100.0;
	HPatches.Add(FBox(C - FVector(R, R, 0.0), C + FVector(R, R, 0.0)));
}

double URTSSunShadowSubsystem::GroundZ(const FVector& Pm) const
{
	const FVector2D D(Pm.X * 100.0 - HCur.O.X, Pm.Y * 100.0 - HCur.O.Y);
	return GroundAt(HCur, HM.G, FVector2D::DotProduct(D, HCur.U), FVector2D::DotProduct(D, HCur.V));
}

double URTSSunShadowSubsystem::SunVisibility(const FVector& Pm, double AboveM) const
{
	// na CPU (a sombra da nuvem no chão, no ultra): só a sombra presa ao chão, sem a corda, a AboveM m do chão desenhado
	if (HCur.Res == 0 || HM.Top.Num() != HCur.Res * HCur.Res)
	{
		return 1.0;
	}
	const int32 N = HCur.Res;
	const FVector2D D(Pm.X * 100.0 - HCur.O.X, Pm.Y * 100.0 - HCur.O.Y);
	const double U = FVector2D::DotProduct(D, HCur.U), V = FVector2D::DotProduct(D, HCur.V);
	const double Fi = U / HCur.Texel + 0.5 * N - 0.5, Fj = V / HCur.Texel + 0.5 * N - 0.5;
	if (Fi < 0.0 || Fj < 0.0 || Fi > N - 1.0 || Fj > N - 1.0)
	{
		return 1.0;
	}
	const int32 I = FMath::Clamp(FMath::FloorToInt32(Fi), 0, N - 2), J = FMath::Clamp(FMath::FloorToInt32(Fj), 0, N - 2);
	const double Ti = FMath::Clamp(Fi - I, 0.0, 1.0), Tj = FMath::Clamp(Fj - J, 0.0, 1.0);
	const double Top = FMath::Lerp(FMath::Lerp(double(float(HM.Top[J * N + I])), double(float(HM.Top[J * N + I + 1])), Ti),
		FMath::Lerp(double(float(HM.Top[(J + 1) * N + I])), double(float(HM.Top[(J + 1) * N + I + 1])), Ti), Tj);
	return FMath::Clamp((AboveM - Top) / 0.3 + 0.5, 0.0, 1.0);
}

void URTSSunShadowSubsystem::UpdateVehicleMap()
{
	if (!HMapTex || HCur.Res == 0)
	{
		return;
	}
	const int32 N = TRes, Nm = HCur.Res;
	const double Tx = HCur.Texel;
	if (!TMapTex)
	{
		TOut.Init(FFloat16(-1.0f), N * N);
		UTexture2D* T = UTexture2D::CreateTransient(N, N, PF_R16F);
		if (!T)
		{
			return;
		}
		T->SRGB = false;
		T->CompressionSettings = TC_HDR;
		T->Filter = TF_Bilinear;
		T->AddressX = TA_Clamp;
		T->AddressY = TA_Clamp;
		T->NeverStream = true;
		FTexture2DMipMap& Mip = T->GetPlatformData()->Mips[0];
		void* Dst = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Dst, TOut.GetData(), int64(TOut.Num()) * sizeof(FFloat16));
		Mip.BulkData.Unlock();
		T->UpdateResource();
		TMapTex = T;
		TDirty = FIntRect(0, 0, 0, 0);
		TFrameId = HCur.Id;
		TLastHash = 0;
	}
	bool bFull = false;
	if (TFrameId != HCur.Id)
	{
		TFrameId = HCur.Id;                       // o mapa foi refeito: a grade recomeça
		bFull = true;
	}
	// a janela segue o ponto do chão que a câmera olha, em passos de 32 texels (16 m), sempre num texel do mapa: a
	// sombra não nada e o chão de cada texel da janela é um texel do mapa
	const FVector Fc = FocusCm();
	const FVector2D Fr = FVector2D(Fc.X, Fc.Y) - FVector2D(HCur.O.X, HCur.O.Y);
	const FIntPoint Org(FMath::RoundToInt32((FVector2D::DotProduct(Fr, HCur.U) / Tx + 0.5 * Nm) / 32.0) * 32 - N / 2,
		FMath::RoundToInt32((FVector2D::DotProduct(Fr, HCur.V) / Tx + 0.5 * Nm) / 32.0) * 32 - N / 2);
	if (Org != TOrg)
	{
		TOrg = Org;
		bFull = true;
	}
	// sem poeira no ar (ou desligada) não precisa de sombra de tanque; os veículos sob a névoa de guerra não fazem sombra
	// (não entregam a posição); nada andou desde o último quadro: nada a refazer
	const bool bAny = Vehicles.Num() > 0;
	uint32 Hash = HashCombine(GetTypeHash(TOrg), GetTypeHash(HCur.Id));
	if (bAny)
	{
		for (const FRTSShadowVehicle& S : Vehicles)
		{
			if (S.bVisible)
			{
				Hash = HashCombine(Hash, GetTypeHash(FIntVector(FMath::RoundToInt32(S.PosM.X * 100.0), FMath::RoundToInt32(S.PosM.Y * 100.0), FMath::RoundToInt32(S.PosM.Z * 100.0))));
				Hash = HashCombine(Hash, GetTypeHash(FIntVector(FMath::RoundToInt32(S.FwdM.X * 1000.0), FMath::RoundToInt32(S.FwdM.Y * 1000.0),
					FMath::RoundToInt32(FMath::Atan2(S.TurretFwdM.Y, S.TurretFwdM.X) * 1000.0))));
			}
		}
	}
	if (!bFull && Hash == TLastHash)
	{
		return;
	}
	TLastHash = Hash;
	FIntRect Up = TDirty;
	if (bFull)
	{
		for (FFloat16& V : TOut) V = FFloat16(-1.0f);
		Up = FIntRect(0, 0, N, N);
	}
	else if (TDirty.Area() > 0)
	{
		for (int32 J = TDirty.Min.Y; J < TDirty.Max.Y; ++J)
		{
			for (int32 I = TDirty.Min.X; I < TDirty.Max.X; ++I)
			{
				TOut[J * N + I] = FFloat16(-1.0f);
			}
		}
	}
	// os trechos de cada linha: as duas caixas de cada veículo (o casco e a torre, a torre no rumo dela), cortadas pela
	// linha; o topo da sombra no texel u é o topo da caixa menos tan(e) × o que falta andar contra a luz até ela
	struct FSpan { double Ua, Ub, TopZ, Len; };
	TArray<FSpan> Spans;
	TArray<int32> RowsUsed;
	if (TRowSpans.Num() != N)
	{
		TRowSpans.SetNum(N);
	}
	const double TanE = FMath::Max(HCur.Tan, 0.02);
	if (bAny)
	{
		for (const FRTSShadowVehicle& S : Vehicles)
		{
			if (!S.bVisible)
			{
				continue;
			}
			const FVector2D Fh = FVector2D(S.FwdM.X, S.FwdM.Y).GetSafeNormal();
			const FVector2D Ft = S.TurretFwdM.IsNearlyZero() ? Fh : FVector2D(S.TurretFwdM.X, S.TurretFwdM.Y).GetSafeNormal();
			const double Ls = S.HalfLenM / 3.95, Ws = S.HalfWidthM / 1.88;
			struct FBx { FVector2D F; double X0, X1, Y0, Y1, Z1; };
			const FBx Boxes[2] = {
				{ Fh, -S.HalfLenM, S.HalfLenM, -S.HalfWidthM, S.HalfWidthM, S.HullTopM },
				{ Ft, -2.55 * Ls, 2.27 * Ls, -1.5 * Ws, 1.5 * Ws, S.HeightM } };
			const FVector2D Pc(S.PosM.X * 100.0 - HCur.O.X, S.PosM.Y * 100.0 - HCur.O.Y);
			for (const FBx& Bx : Boxes)
			{
				const FVector2D Rr(Bx.F.Y, -Bx.F.X);
				FVector2D Q[4];
				double Vmin = 1e30, Vmax = -1e30;
				for (int32 K = 0; K < 4; ++K)
				{
					const FVector2D W = Pc + Bx.F * (((K & 1) ? Bx.X1 : Bx.X0) * 100.0) + Rr * (((K & 2) ? Bx.Y1 : Bx.Y0) * 100.0);
					Q[K] = FVector2D(FVector2D::DotProduct(W, HCur.U) / Tx + 0.5 * Nm - TOrg.X, FVector2D::DotProduct(W, HCur.V) / Tx + 0.5 * Nm - TOrg.Y);
					Vmin = FMath::Min(Vmin, Q[K].Y);
					Vmax = FMath::Max(Vmax, Q[K].Y);
				}
				const FVector2D Poly[4] = { Q[0], Q[1], Q[3], Q[2] };   // o contorno
				const double TopZ = S.PosM.Z * 100.0 + Bx.Z1 * 100.0;
				const double Len = (Bx.Z1 * 100.0 + 50.0) / (TanE * Tx);   // texels até a sombra entrar no chão (mais folga)
				for (int32 J = FMath::Max(1, FMath::FloorToInt32(Vmin)); J <= FMath::Min(N - 2, FMath::CeilToInt32(Vmax)); ++J)
				{
					const double Vc = J + 0.5;
					double Ua = 1e30, Ub = -1e30;
					for (int32 E = 0; E < 4; ++E)
					{
						const FVector2D& A = Poly[E];
						const FVector2D& Bq = Poly[(E + 1) & 3];
						if ((A.Y <= Vc) != (Bq.Y <= Vc))
						{
							const double X = A.X + (Vc - A.Y) / (Bq.Y - A.Y) * (Bq.X - A.X);
							Ua = FMath::Min(Ua, X);
							Ub = FMath::Max(Ub, X);
						}
					}
					if (Ua > Ub)
					{
						continue;
					}
					if (TRowSpans[J].Num() == 0)
					{
						RowsUsed.Add(J);
					}
					TRowSpans[J].Add(Spans.Num());
					Spans.Add({ Ua, Ub, TopZ, FMath::Min(Len, 300.0) });
				}
			}
		}
	}
	// cada linha é de uma thread só (sem corrida); o chão de cada texel é o texel do mapa embaixo
	ParallelFor(RowsUsed.Num(), [&](int32 K)
	{
		const int32 J = RowsUsed[K], Gj = TOrg.Y + J;
		if (Gj < 0 || Gj >= Nm)
		{
			return;
		}
		for (int32 Si : TRowSpans[J])
		{
			const FSpan& Sp = Spans[Si];
			const int32 Ia = FMath::Max(1, FMath::FloorToInt32(Sp.Ua)), Ib = FMath::Min(N - 2, FMath::CeilToInt32(Sp.Ub + Sp.Len));
			for (int32 I = Ia; I <= Ib; ++I)
			{
				const double Uc = I + 0.5;
				if (Uc < Sp.Ua)
				{
					continue;
				}
				const int32 Gi = TOrg.X + I;
				if (Gi < 0 || Gi >= Nm)
				{
					continue;
				}
				const float G = HM.G[Gj * Nm + Gi];
				if (G < -1e29f)
				{
					continue;
				}
				const double Rel = (Sp.TopZ - FMath::Max(Uc - Sp.Ub, 0.0) * Tx * TanE - G) * 0.01;
				if (Rel < -0.5)
				{
					break;                           // a sombra já entrou no chão
				}
				FFloat16& Tv = TOut[J * N + I];
				if (Rel > float(Tv))
				{
					Tv = FFloat16(float(Rel));
				}
			}
		}
	});
	FIntRect Now(N, N, 0, 0);
	for (const int32 J : RowsUsed)
	{
		for (int32 Si : TRowSpans[J])
		{
			const FSpan& Sp = Spans[Si];
			Now.Min.X = FMath::Min(Now.Min.X, FMath::Max(1, FMath::FloorToInt32(Sp.Ua)));
			Now.Max.X = FMath::Max(Now.Max.X, FMath::Min(N - 1, FMath::CeilToInt32(Sp.Ub + Sp.Len) + 1));
		}
		Now.Min.Y = FMath::Min(Now.Min.Y, J);
		Now.Max.Y = FMath::Max(Now.Max.Y, J + 1);
		TRowSpans[J].Reset();
	}
	if (Now.Min.X >= Now.Max.X || Now.Min.Y >= Now.Max.Y)
	{
		Now = FIntRect(0, 0, 0, 0);
	}
	// sobe a união do que apagou e do que escreveu (um retângulo só)
	if (Now.Area() > 0)
	{
		Up = Up.Area() > 0 ? FIntRect(Up.Min.ComponentMin(Now.Min), Up.Max.ComponentMax(Now.Max)) : Now;
	}
	TDirty = Now;
	if (Up.Area() <= 0)
	{
		return;
	}
	const int32 Wd = Up.Width(), Ht = Up.Height();
	TArray<FFloat16>* Buf = new TArray<FFloat16>();
	Buf->SetNumUninitialized(Wd * Ht);
	for (int32 J = 0; J < Ht; ++J)
	{
		FMemory::Memcpy(Buf->GetData() + J * Wd, TOut.GetData() + (Up.Min.Y + J) * N + Up.Min.X, Wd * sizeof(FFloat16));
	}
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(Up.Min.X, Up.Min.Y, 0, 0, Wd, Ht);
	TMapTex->UpdateTextureRegions(0, 1, Region, Wd * sizeof(FFloat16), sizeof(FFloat16), reinterpret_cast<uint8*>(Buf->GetData()),
		[Buf, Region](uint8*, const FUpdateTextureRegion2D*) { delete Buf; delete Region; });
}

namespace
{
	// rts.SunShadow 0 desliga o mapa (quem lê vê tudo ao sol)
	TAutoConsoleVariable<int32> CVarSunShadow(TEXT("rts.SunShadow"), 1,
		TEXT("O mapa de sombra do sol de todas as fumaças e poeiras (RTSSunShadow): 1 liga, 0 desliga."), ECVF_Default);

}

bool URTSSunShadowSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE) && Super::ShouldCreateSubsystem(Outer);
}

void URTSSunShadowSubsystem::Deinitialize()
{
	RTSSunShadow::ClearRenderData();
	HJobInFlight.Reset();
	HJobBuilt.Reset();
	Super::Deinitialize();
}

TStatId URTSSunShadowSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URTSSunShadowSubsystem, STATGROUP_Tickables);
}

bool URTSSunShadowSubsystem::IsTickable() const
{
	return !IsTemplate() && GetWorld() && GetWorld()->IsGameWorld() && CVarSunShadow.GetValueOnGameThread() > 0;
}

bool URTSSunShadowSubsystem::FindSun(FVector& OutToSun)
{
	// o sol do mapa: o que ilumina a atmosfera (o diretor do Look); senão o primeiro
	if (!SunLight.IsValid() || (TickCount % 120) == 0)
	{
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
		{
			UDirectionalLightComponent* C = It->FindComponentByClass<UDirectionalLightComponent>();
			if (C && (!SunLight.IsValid() || C->IsUsedAsAtmosphereSunLight()))
			{
				SunLight = C;
			}
		}
	}
	if (!SunLight.IsValid())
	{
		return false;
	}
	OutToSun = -SunLight->GetComponentTransform().GetUnitAxis(EAxis::X);
	return true;
}

FVector URTSSunShadowSubsystem::FocusCm() const
{
	// o ponto do chão que a câmera olha (a janela dos veículos e a foto de conferência seguem ele)
	const APlayerCameraManager* Pcm = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
	if (!Pcm)
	{
		return HCur.O;
	}
	const FVector Cm = Pcm->GetCameraLocation();
	const FVector F = Pcm->GetCameraRotation().Vector();
	FVector P = Cm;
	if (F.Z < -0.05)
	{
		P += F * FMath::Min((Cm.Z - HCur.O.Z) / -F.Z, 30000.0);
	}
	return P;
}

void URTSSunShadowSubsystem::Tick(float DeltaTime)
{
	++TickCount;
	FVector ToSun;
	if (FindSun(ToSun))
	{
		SunDir = ToSun;
	}
	// com os shaders prontos (no meio da compilação o terreno sai com o material padrão) e ~0,5 s de partida
	bool bReady = TickCount > 30 && !SunDir.IsNearlyZero();
#if WITH_EDITOR
	bReady = bReady && (!GShaderCompilingManager || GShaderCompilingManager->GetNumRemainingJobs() == 0);
#endif
	if (bReady && !bHMapGaveUp)
	{
		TickHeightMap();
	}
	UpdateVehicleMap();
	PushRenderData();
}

void URTSSunShadowSubsystem::GetMaterialParams(const FVector& CamCm, FLinearColor& OutCamToMap, FLinearColor& OutAxis, FLinearColor& OutWindow, float& OutTanE) const
{
	// um vetor de material chega no Custom com 3 canais (o 4º não): tudo em xyz
	const int32 N = HCur.Res;
	const double Tx = HCur.Texel;
	const FVector C = CamCm - HCur.O;
	OutCamToMap = FLinearColor(float(C.X), float(C.Y), float(C.Z), 0.0f);
	OutAxis = FLinearColor(float(HCur.U.X), float(HCur.U.Y), N > 0 ? float(1.0 / (N * Tx)) : 0.0f, float(N));
	OutWindow = FLinearColor(float((TOrg.X - 0.5 * N) * Tx), float((TOrg.Y - 0.5 * N) * Tx), float(1.0 / (TRes * Tx)), 0.0f);
	OutTanE = float(HCur.Tan);
}

void URTSSunShadowSubsystem::PushRenderData()
{
	FRTSSunShadowRenderData D;
	const int32 N = HCur.Res;
	const double Tx = HCur.Texel;
	D.OriginCm = HCur.O;
	D.Axis = FVector2f(float(HCur.U.X), float(HCur.U.Y));
	D.InvWidthCm = N > 0 ? float(1.0 / (N * Tx)) : 0.0f;
	D.TanE = float(HCur.Tan);
	D.Texels = float(N);
	D.WindowCm = FVector2f(float((TOrg.X - 0.5 * N) * Tx), float((TOrg.Y - 0.5 * N) * Tx));
	D.WindowInvWidthCm = float(1.0 / (TRes * Tx));
	RTSSunShadow::PushRenderData(D, HMapTex ? HMapTex->GetResource() : nullptr, TMapTex ? TMapTex->GetResource() : nullptr);
}

double URTSSunShadowSubsystem::SunVisibilityAt(const FVector& PosWorldM, double SoftM) const
{
	const int32 N = HCur.Res;
	if (N == 0 || HM.Top.Num() != N * N || HM.SB.Num() != N * N)
	{
		return 1.0;
	}
	const FVector2D D(PosWorldM.X * 100.0 - HCur.O.X, PosWorldM.Y * 100.0 - HCur.O.Y);
	const double U = FVector2D::DotProduct(D, HCur.U), V = FVector2D::DotProduct(D, HCur.V);
	const double G = GroundAt(HCur, HM.G, U, V);
	if (G < -1e29)
	{
		return 1.0;
	}
	const double Fi = U / HCur.Texel + 0.5 * N - 0.5, Fj = V / HCur.Texel + 0.5 * N - 0.5;
	const int32 I = FMath::Clamp(FMath::FloorToInt32(Fi), 0, N - 2), J = FMath::Clamp(FMath::FloorToInt32(Fj), 0, N - 2);
	const double Ti = FMath::Clamp(Fi - I, 0.0, 1.0), Tj = FMath::Clamp(Fj - J, 0.0, 1.0);
	const auto Bil = [&](const TArray<FFloat16>& A)
	{
		return FMath::Lerp(FMath::Lerp(double(float(A[J * N + I])), double(float(A[J * N + I + 1])), Ti),
			FMath::Lerp(double(float(A[(J + 1) * N + I])), double(float(A[(J + 1) * N + I + 1])), Ti), Tj);
	};
	double Top = Bil(HM.Top);
	// a dos veículos: o texel da janela que cai sobre este ponto (a janela tem o texel e o giro do mapa)
	if (TOut.Num() == TRes * TRes)
	{
		const int32 Wi = FMath::FloorToInt32(Fi + 0.5) - TOrg.X, Wj = FMath::FloorToInt32(Fj + 0.5) - TOrg.Y;
		if (Wi >= 0 && Wj >= 0 && Wi < TRes && Wj < TRes)
		{
			Top = FMath::Max(Top, double(float(TOut[Wj * TRes + Wi])));
		}
	}
	const double Hg = (PosWorldM.Z * 100.0 - G) * 0.01;   // m acima do chão desenhado
	const double Soft = FMath::Max(SoftM, 0.02);
	const double Lo = FMath::Max(Hg - Soft, 0.0), Hi = FMath::Max(Hg + Soft, Lo + 0.05);
	double Vis = 1.0 - FMath::Clamp((FMath::Max(Top, Lo) - Lo) / (Hi - Lo), 0.0, 1.0);
	const double SB = Bil(HM.SB), SA = Bil(HM.SA);
	Vis *= 1.0 - FMath::Clamp((SA - SB) / 0.3, 0.0, 1.0) * FMath::Clamp((FMath::Min(Hi, SA) - FMath::Max(Lo, SB)) / (Hi - Lo), 0.0, 1.0);
	return Vis;
}

double RTSSunShadow::SunVisibilityCPU(const UWorld* World, const FVector& PosWorldM, double SoftM)
{
	const URTSSunShadowSubsystem* S = World ? World->GetSubsystem<URTSSunShadowSubsystem>() : nullptr;
	return S ? S->SunVisibilityAt(PosWorldM, SoftM) : 1.0;
}
