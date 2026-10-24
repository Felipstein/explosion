#include "RTSGroundMemory.h"

#include "CanvasItem.h"
#include "Components/DecalComponent.h"
#include "Engine/Canvas.h"
#include "Engine/DecalActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Components/LightComponent.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "HAL/IConsoleManager.h"
#include "LandscapeComponent.h"
#include "LandscapeNaniteComponent.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Scalability.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	// ruído de valor (hash da grade, nada sorteado em tempo de jogo) e a soma em oitavas: a borda orgânica
	float Hash2(int32 X, int32 Y, int32 Seed)
	{
		uint32 H = uint32(X) * 374761393u + uint32(Y) * 668265263u + uint32(Seed) * 2246822519u;
		H = (H ^ (H >> 13)) * 1274126177u;
		return float((H ^ (H >> 16)) & 0xFFFFu) / 65535.0f;
	}
	float ValueNoise(float X, float Y, int32 Seed)
	{
		const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
		const float FX = FMath::SmoothStep(0.0f, 1.0f, X - X0), FY = FMath::SmoothStep(0.0f, 1.0f, Y - Y0);
		const float A = FMath::Lerp(Hash2(X0, Y0, Seed), Hash2(X0 + 1, Y0, Seed), FX);
		const float B = FMath::Lerp(Hash2(X0, Y0 + 1, Seed), Hash2(X0 + 1, Y0 + 1, Seed), FX);
		return FMath::Lerp(A, B, FY);
	}
	float Fbm(float X, float Y, int32 Seed)
	{
		float Sum = 0.0f, Amp = 0.5f, Norm = 0.0f;
		for (int32 O = 0; O < 5; ++O)
		{
			Sum += ValueNoise(X, Y, Seed + O * 17) * Amp;
			Norm += Amp;
			X *= 2.03f;
			Y *= 2.03f;
			Amp *= 0.5f;
		}
		return Sum / Norm;
	}

	const TCHAR* DecalMaterialPath = TEXT("/Game/Materials/M_GroundMemoryDecal.M_GroundMemoryDecal");
	// a esteira some em ~5 min (ZH: 300 s): a cada 2 s o canal da esteira vezes 0,987 (0,987^150 ≈ 0,14)
	constexpr float DecaySeconds = 2.0f;
	constexpr float TrackDecay = 0.987f;
	// a brasa das marcas (o B do caderno) apaga em ~30 s: × 0,88 a cada 2 s (0,88^15 ≈ 0,15)
	constexpr float HeatDecay = 0.88f;
	// a opção do atlas das marcas (Game/Tools/bake_craters.py: A, B ou C; -RTSMarks=A pro A/B)
	// (13/10, o Felipe: as três ficam, cada uma pra uma coisa) a cratera A, B ou C por marca; a morte do tanque é o
	// avental da C ("um tanque explodindo ... o 3"). -RTSMarks=X força uma só, pro A/B.
	const TCHAR* const CraterOptions[3] = { TEXT("A"), TEXT("B"), TEXT("C") };
	const TCHAR* WreckOption = TEXT("C");
	// o caderno grosso: o mapa inteiro (1024 m → 50 cm por pixel)
	constexpr int32 CoarseRes = 2048;
	// o de detalhe: 192 m em volta do foco da câmera, 4096² → 4,7 cm por pixel (no zoom do jogo o
	// pixel da tela cobre 3–5 cm do chão). O centro fica 25 m à frente do foco (a câmera inclinada vê
	// mais longe pra frente) e só anda quando o desejado sai 28 m do atual.
	constexpr int32 DetailRes = 4096;
	constexpr double DetailSizeCm = 19200.0;
	constexpr double DetailAheadCm = 2500.0;
	constexpr double DetailSlackCm = 2800.0;
	// uma fita nova a cada 1 m andado ou 6° girados (o pivô no lugar também risca, cada lagarta pro seu lado)
	constexpr double SegmentCm = 100.0;
	constexpr float SegmentTurnDeg = 6.0f;
	// uma passada risca 0,55: duas no mesmo lugar já marcam forte (o caderno satura em 1)
	constexpr float TrackPass = 0.55f;
	// o que cabe guardado: ~5 min de 50 tanques andando à vista; as cicatrizes, a partida toda
	constexpr int32 MaxQuads = 1 << 18;
	constexpr int32 MaxScars = 1 << 14;
	constexpr int32 TrisPerItem = 8192;
	// o relevo (12/10): o de detalhe na metade da resolução da esteira (9,4 cm: a bordinha e a sombra são
	// macias; as garras finas ficam no R da esteira a 4,7 cm) e o do meio, 384 m em volta da câmera
	constexpr int32 ReliefRes = 2048;
	constexpr int32 MidRes = 2048;
	constexpr double MidSizeCm = 38400.0;
	constexpr double MidAheadCm = 6000.0;
	constexpr double MidSlackCm = 6000.0;
	// o perfil da esteira: a versão C do Look (13/10, a escolha do Felipe; Game/Tools/look_materials/
	// sand_desert_v2.json, track_profile): o sulco de 2,2 cm, a bordinha de 2 cm e 9 cm de largura em
	// 0,31 m + 0,6 × a largura, a faixa de ±4 cm no 8 bits (passo de 0,03 cm), a penumbra de ~6 mm
	constexpr float ReliefRangeCm = 4.0f;
	constexpr float ReliefGrooveCm = 2.2f;
	constexpr float ReliefBermCm = 2.0f;
	constexpr float ReliefBermWidthCm = 9.0f;
	constexpr float ReliefPenumbraCm = 0.6f;
	// o carimbo do perfil cobre ±60 cm do meio da fita (a cobertura vai a zero em 0,6 m); as pontas somem em 1,2 m
	constexpr double ProfileHalfCm = 60.0;
	constexpr float EndFadeM = 1.2f;

	// a v3 do chão (16/10, docs/CHAO_V3_ESTUDO.md §5.2, -RTSGroundV3): o perfil novo da trilha e a luz assada.
	// O sulco de 4 cm de média (o HeightCS multiplica por 0,75 e por 0,65–1,35 do ruído: 5,33 × 0,75 = 4), a bordinha
	// de 3 × 20 cm (física 1–3 cm; 20 cm = 2,7 px no zoom do jogo), a faixa da altura de ±10 cm (o sulco da curva tem
	// 6–8 cm) e o carimbo do perfil até 85 cm do meio da fita (a bordinha larga passa dos 60)
	bool GGroundV3 = false;
	float RangeCmNow() { return GGroundV3 ? 10.0f : ReliefRangeCm; }
	float GrooveCmNow() { return GGroundV3 ? 5.33f : ReliefGrooveCm; }
	float BermCmNow() { return GGroundV3 ? 3.0f : ReliefBermCm; }
	float BermWidthCmNow() { return GGroundV3 ? 20.0f : ReliefBermWidthCm; }
	double ProfileHalfNow() { return GGroundV3 ? 85.0 : ProfileHalfCm; }
	// a curva (16/10, §5.2 do CHAO_V3_ESTUDO; Liu 2009: raio < 30 m alarga, várias passadas): no raio ≤ 6 m e no pivô a
	// fita vai a ~1 m (×1,55) e o sulco a ~7 cm (×1,75); a bordinha de fora da curva dobra (a terra raspada vai pra fora)
	// e a de dentro some. O perfil grava a faixa ÷ 1,75 e a bordinha ÷ 2 (a folga: o caderno de 10 bits para em 1)
	constexpr float CurveFullRadiusM = 6.0f;
	constexpr float CurveNoneRadiusM = 30.0f;
	constexpr float CurveWiden = 0.55f;
	constexpr float CurveDeepen = 0.75f;
	constexpr float CurveBandMax = 1.0f + CurveDeepen;
	constexpr float CurveBermMax = 2.0f;
	// a curvatura é alisada pela distância (a fita nasce a cada 1 m ou 6°: uma fita de 6° em 30 cm seguida de uma reta
	// dava raio de 3 m e depois nada, e a trilha saía em blocos); no pivô a distância é a do arco da lagarta
	constexpr float CurveSmoothM = 3.0f;
	// o quanto o perfil passa do meio da lagarta (a fita de 64 cm), com a curva: a margem do retângulo sujo
	double ProfileReachNow() { return GGroundV3 ? 85.0 * (1.0 + CurveWiden) : ProfileHalfCm; }
	// (16/10, a v3, o Look pediu) a garra e o halo: a garra do M1 (19,4 cm; 64 m ÷ 330 = 19,39, pra a volta do U em 64 m
	// não emendar), assada só na luz, sob a faixa até 40 cm do meio; o halo revirado até 50 cm além da trilha (Prose 1985)
	constexpr float ClawPitchCm = 6400.0f / 330.0f;
	constexpr float AlongRangeM = 128.0f;   // o U vai de 0 a ~65 m (dá a volta em 64): cabe em 0..0,51 do G16
	constexpr double AlongHalfCm = 40.0;
	float HaloCmNow() { return GGroundV3 ? 50.0f : 0.0f; }
	// as mips da v3: até a 6 (1/64; na câmera do jogo a luz e o relevo não passam da 3 ou 4), uma vez por quadro
	constexpr int32 GroundMipLevels = 6;
	// a lista do que mudou: junta com o vizinho quando a caixa unida custa até 1,5 × as duas; passou de 8, junta tudo
	int64 RectArea(const FIntRect& R) { return int64(FMath::Max(R.Width(), 0)) * FMath::Max(R.Height(), 0); }
	void GrowRect(TArray<FIntRect, TInlineAllocator<8>>& Into, const FIntRect& In)
	{
		if (In.Width() <= 0 || In.Height() <= 0)
		{
			return;
		}
		FIntRect R = In;
		for (int32 K = 0; K < Into.Num(); ++K)
		{
			FIntRect U(Into[K].Min.ComponentMin(R.Min), Into[K].Max.ComponentMax(R.Max));
			if (RectArea(U) * 2 <= (RectArea(Into[K]) + RectArea(R)) * 3)
			{
				R = U;
				Into.RemoveAtSwap(K);
				K = -1;   // recomeça: a unida pode alcançar outras
			}
		}
		Into.Add(R);
		if (Into.Num() > 4)
		{
			FIntRect All = Into[0];
			for (const FIntRect& Q : Into)
			{
				All = FIntRect(All.Min.ComponentMin(Q.Min), All.Max.ComponentMax(Q.Max));
			}
			Into.Reset();
			Into.Add(All);
		}
	}
	// (16/10) a cota da luz assada por quadro, pra medir o custo dela separado do material (0 = não refaz nada: a luz
	// fica a do chão liso)
	TAutoConsoleVariable<int32> CVarGroundSunLitJobs(TEXT("rts.Ground.SunLitJobs"), 6,
		TEXT("Quantos ladrilhos da luz assada da v3 o chão refaz por quadro (0 desliga a varredura; o material segue lendo)."));
	// (16/10) as mips da v3 ligadas (1) ou paradas (0: ficam as de antes), pra medir o custo delas
	TAutoConsoleVariable<int32> CVarGroundMips(TEXT("rts.Ground.Mips"), 1,
		TEXT("Refaz as mips do relevo e da luz assada da v3 no que mudou (0 = para; pra medir)."));
	// (16/10, o pedido 4 de hoje cedo) o sulco por tipo de chão (o surface.png): 0 = tudo como areia (o A/B)
	TAutoConsoleVariable<int32> CVarGroundSurface(TEXT("rts.Ground.Surface"), 1,
		TEXT("O sulco, a bordinha e a garra de cada chão pelo surface.png (v3): areia, terra batida, barro, pedra e asfalto (0 = tudo areia). Vale pras marcas novas."));
	// (16/10, o WorldForge) o capim, as moitas e os arbustos deitando no caminho batido (o material deles lê o caderno)
	TAutoConsoleVariable<int32> CVarGroundTrample(TEXT("rts.Ground.Trample"), 1,
		TEXT("As plantas da montagem (etiqueta WorldForgeTrample) deitam no caminho batido e escurecem no queimado (0 = em pé, o A/B). Vale na próxima olhada (1 s)."));
	TAutoConsoleVariable<float> CVarGroundClawCm(TEXT("rts.Ground.ClawCm"), 2.0f,   // (16/10, o Look: 2 cm lê, sem cintilar no vídeo da volta da câmera)
		
		TEXT("A garra da trilha na luz assada da v3 (-RTSGroundV3): a fundura de pico a pico, em cm (0 desliga). Vale pras marcas novas."));
	// a luz assada: a queda mais funda que faz sombra (a bacia de 50 cm, o avental de até 90, a borda), o raio do céu,
	// o limite do claro e a cota de ladrilhos de 512² por quadro
	constexpr float SunLitMaxDropCm = 100.0f;
	constexpr float SunLitAORadiusCm = 30.0f;
	constexpr float SunLitMaxLit = 1.2f;
	constexpr int32 SunLitJobsPerFrame = 6;
	// a volta lenta: o sulco perde até 45% e a bordinha 30% em minutos (o frescor × 0,987 a cada 2 s, meia-vida
	// ~1,8 min); um ladrilho de 512 a cada 0,5 s dá a volta no detalhe (64) em ~32 s e no meio (16) em ~8 s
	constexpr float SunLitRollSeconds = 0.5f;
}

UTexture2D* URTSGroundMemory::MakeStamp(int32 W, int32 H, int32 Kind)
{
	// os carimbos são feitos aqui (sem asset; sorteio de semente fixa: o mesmo em todo PC e toda partida):
	// - a esteira: 1 m de lagarta (U ao longo, repete), 6 garras de 17 cm (o tanque do Look), a borda
	//   macia dos lados; a garra afunda mais que o vão entre elas, só no miolo da fita;
	// - o queimado: a borda recortada por ruído fractal, a fuligem em pontinhos e 5 riscos saindo do
	//   centro, como a explosão espirra no chão (a 1ª versão, em senos, saía estrela; a 2ª, mancha borrada);
	// - a cratera: uma bacia lisa e redonda (a borda em senos fazia o relevo virar flor no sol baixo).
	const bool bTrack = Kind == 0;
	UTexture2D* Tex = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
	if (!Tex)
	{
		return nullptr;
	}
	Tex->SRGB = false;
	Tex->Filter = TF_Bilinear;
	Tex->AddressX = bTrack ? TA_Wrap : TA_Clamp;
	Tex->AddressY = TA_Clamp;
	FRandomStream Rng(1871 + Kind);
	float RayAngle[5];
	float RayLen[5];
	for (int32 K = 0; K < 5; ++K)
	{
		RayAngle[K] = Rng.FRandRange(0.0f, 2.0f * PI);
		RayLen[K] = Rng.FRandRange(0.8f, 1.0f);
	}
	FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
	FColor* Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			const float U = (X + 0.5f) / W;
			const float V = (Y + 0.5f) / H;
			float A = 0.0f;
			if (bTrack)
			{
				const float Side = FMath::SmoothStep(0.0f, 0.16f, V) * FMath::SmoothStep(0.0f, 0.16f, 1.0f - V);
				// a garra: um degrau macio (não serrilha quando o caderno amostra)
				const float G = FMath::Frac(U * 6.0f);
				const float Grouser = FMath::SmoothStep(0.05f, 0.15f, G) * (1.0f - FMath::SmoothStep(0.45f, 0.55f, G));
				// a garra só no miolo da fita (a versão C do Look, a 0,16–0,30 m do meio): na borda a marca é
				// contínua, com a média da garra (0,4), porque a garra na borda fazia a sombra pontilhada
				const float D = FMath::Abs(V - 0.5f) * 0.64f;
				const float Core = 1.0f - FMath::SmoothStep(0.16f, 0.30f, D);
				A = Side * (0.62f + 0.38f * (Grouser * Core + 0.4f * (1.0f - Core)));
			}
			else
			{
				const float DX = U * 2.0f - 1.0f;
				const float DY = V * 2.0f - 1.0f;
				const float R = FMath::Sqrt(DX * DX + DY * DY);
				if (Kind == 2)
				{
					const float Q = FMath::Clamp(1.0f - R * R, 0.0f, 1.0f);
					A = Q * Q;
				}
				else
				{
					// a borda: o raio empurrado pelo ruído fractal (recorte orgânico, sem forma de ângulo); o miolo
					// escuro e a fuligem ralinha na borda, em pontinhos; os 5 riscos saem do centro, rasgados pelo ruído
					const float N = Fbm(U * 5.0f, V * 5.0f, 7);
					const float Rp = R + (N - 0.5f) * 0.42f;
					// a borda curta (o recorte do ruído aparece; larga, virava mancha borrada) e o miolo mais
					// escuro que a borda (a fuligem assenta mais no meio)
					const float Core = (1.0f - FMath::SmoothStep(0.50f, 0.60f, Rp)) * (0.62f + 0.38f * (1.0f - FMath::SmoothStep(0.0f, 0.5f, R)));
					const float Fleck = FMath::SmoothStep(0.62f, 0.72f, Fbm(U * 22.0f, V * 22.0f, 29)) * (1.0f - FMath::SmoothStep(0.55f, 0.95f, R)) * 0.6f;
					const float Ang = FMath::Atan2(DY, DX) + PI;
					float Ray = 0.0f;
					for (int32 K = 0; K < 5; ++K)
					{
						const float C = FMath::Cos(Ang - RayAngle[K]);
						Ray = FMath::Max(Ray, FMath::Pow(FMath::Max(C, 0.0f), 60.0f) * (1.0f - FMath::SmoothStep(0.4f, RayLen[K], R)));
					}
					Ray *= 0.5f * FMath::SmoothStep(0.35f, 0.65f, Fbm(U * 12.0f, V * 12.0f, 41));
					A = FMath::Clamp(FMath::Max(FMath::Max(Core, Fleck), Ray), 0.0f, 1.0f);
				}
			}
			const uint8 B = uint8(FMath::Clamp(FMath::RoundToInt(A * 255.0f), 0, 255));
			Pixels[Y * W + X] = FColor(B, B, B, B);
		}
	}
	Mip.BulkData.Unlock();
	Tex->UpdateResource();
	return Tex;
}

UTexture2D* URTSGroundMemory::MakeProfileStamp(float TexelCm)
{
	// a conta do track_relief do Look (Game/Tools/look_ground.py), atravessando a fita (V: −60 a +60 cm do
	// meio): a faixa smoothstep(0,34; 0,27; d), a bordinha exp(−((d − 0,31 − 0,6·0,09)/0,09)²) e a cobertura
	// smoothstep(0,6; 0,38; d). Cada valor é a média num texel do caderno (sem serrilhar a bordinha de 9 cm
	// no caderno de 9,4 ou 19 cm). A faixa e a bordinha vão divididas pela cobertura: a mistura translúcida
	// (cor × alfa + velho × (1 − alfa)) dá a conta do Look, h·(1 − cobertura) + hh.
	constexpr int32 W = 4;
	constexpr int32 H = 128;
	UTexture2D* Tex = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
	if (!Tex)
	{
		return nullptr;
	}
	Tex->SRGB = false;
	Tex->Filter = TF_Bilinear;
	Tex->AddressX = TA_Wrap;
	Tex->AddressY = TA_Clamp;
	const float BermW = BermWidthCmNow() / 100.0f;
	const float BermAt = 0.31f + 0.6f * BermW;
	const float BoxM = TexelCm / 100.0f;
	FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
	FColor* Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	for (int32 Y = 0; Y < H; ++Y)
	{
		const float X = ((Y + 0.5f) / H - 0.5f) * float(ProfileHalfNow()) * 0.02f;
		float Band = 0.0f, Berm = 0.0f, Cover = 0.0f;
		constexpr int32 Sub = 16;
		for (int32 K = 0; K < Sub; ++K)
		{
			const float D = FMath::Abs(X + ((K + 0.5f) / Sub - 0.5f) * BoxM);
			Band += 1.0f - FMath::SmoothStep(0.27f, 0.34f, D);
			Berm += FMath::Exp(-FMath::Square((D - BermAt) / BermW));
			Cover += 1.0f - (GGroundV3 ? FMath::SmoothStep(0.45f, 0.80f, D) : FMath::SmoothStep(0.38f, 0.60f, D));
		}
		Band /= Sub;
		Berm /= Sub;
		Cover /= Sub;
		const float Inv = Cover > 1e-3f ? 1.0f / Cover : 0.0f;
		const FColor C(uint8(FMath::RoundToInt(FMath::Clamp(Band * Inv, 0.0f, 1.0f) * 255.0f)),
			uint8(FMath::RoundToInt(FMath::Clamp(Berm * Inv, 0.0f, 1.0f) * 255.0f)), 255,
			uint8(FMath::RoundToInt(FMath::Clamp(Cover, 0.0f, 1.0f) * 255.0f)));
		for (int32 Xp = 0; Xp < W; ++Xp)
		{
			Pixels[Y * W + Xp] = C;
		}
	}
	Mip.BulkData.Unlock();
	Tex->UpdateResource();
	return Tex;
}

UTextureRenderTarget2D* URTSGroundMemory::MakeTarget(int32 Res)
{
	// R8G8B8A8 com escrita da placa: o compute das marcas lê e escreve nele (o RTF_RGBA8 vira B8G8R8A8, que não
	// tem leitura garantida na escrita da placa: a revisão de 13/10)
	UTextureRenderTarget2D* T = NewObject<UTextureRenderTarget2D>(this);
	T->bCanCreateUAV = true;
	T->ClearColor = FLinearColor::Transparent;   // o A é o claro das marcas: o preto da Unreal tem alfa 1
	T->AddressX = TA_Clamp;
	T->AddressY = TA_Clamp;
	T->InitCustomFormat(Res, Res, PF_R8G8B8A8, true);
	T->UpdateResourceImmediate(true);
	return T;
}

UTextureRenderTarget2D* URTSGroundMemory::MakeCraterTarget(int32 Res)
{
	UTextureRenderTarget2D* T = NewObject<UTextureRenderTarget2D>(this);
	T->bCanCreateUAV = true;
	T->ClearColor = FLinearColor::Black;
	T->AddressX = TA_Clamp;
	T->AddressY = TA_Clamp;
	// R a altura cheia, G a que faz sombra do sol (sem os torrões; 16/10)
	T->InitCustomFormat(Res, Res, PF_G16R16F, true);
	T->UpdateResourceImmediate(true);
	return T;
}

bool URTSGroundMemory::LoadAtlas(const FString& Kind, const FString& InOption, FRTSGroundMarkAtlas& Out, TArray<TArray<FVector3f>>* OutClods)
{
	FString Option = InOption;
	FParse::Value(FCommandLine::Get(), TEXT("-RTSMarks="), Option);
	const FString Base = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("RTS"), TEXT("Ground"), Option.IsEmpty() ? Kind : Kind + TEXT("_") + Option);
	TArray<uint8> Png;
	FString Meta;
	TSharedPtr<FJsonObject> Json;
	if (!FFileHelper::LoadFileToArray(Png, *(Base + TEXT(".png"))) || !FFileHelper::LoadFileToString(Meta, *(Base + TEXT(".json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Meta), Json) || !Json.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS chão: sem o atlas %s (python Game/Tools/bake_craters.py): sem %s"), *Base, *Kind);
		return false;
	}
	IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Png16 = Images.CreateImageWrapper(EImageFormat::PNG);
	TArray64<uint8> Pixels;
	if (!Png16.IsValid() || !Png16->SetCompressed(Png.GetData(), Png.Num()) || !Png16->GetRaw(ERGBFormat::RGBA, 16, Pixels))
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS chão: o atlas %s.png não abriu"), *Base);
		return false;
	}
	const int32 W = int32(Png16->GetWidth());
	const int32 H = int32(Png16->GetHeight());
	UTexture2D* Tex = UTexture2D::CreateTransient(W, H, PF_A16B16G16R16);
	if (!Tex || Pixels.Num() != int64(W) * H * 8)
	{
		return false;
	}
	Tex->SRGB = false;
	Tex->Filter = TF_Bilinear;
	Tex->AddressX = TA_Clamp;
	Tex->AddressY = TA_Clamp;
	FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
	FMemory::Memcpy(Mip.BulkData.Lock(LOCK_READ_WRITE), Pixels.GetData(), Pixels.Num());
	Mip.BulkData.Unlock();
	Tex->UpdateResource();
	Out.Texture = Tex;
	// (16/10) a altura que faz sombra do sol, à parte (bake_craters.py; só a cratera tem)
	Out.SunTexture = nullptr;
	TArray<uint8> SunPng;
	if (FPaths::FileExists(Base + TEXT("_sun.png")) && FFileHelper::LoadFileToArray(SunPng, *(Base + TEXT("_sun.png"))))
	{
		TSharedPtr<IImageWrapper> Gray = Images.CreateImageWrapper(EImageFormat::PNG);
		TArray64<uint8> SunPixels;
		if (Gray.IsValid() && Gray->SetCompressed(SunPng.GetData(), SunPng.Num()) && Gray->GetRaw(ERGBFormat::Gray, 16, SunPixels)
			&& int32(Gray->GetWidth()) == W && int32(Gray->GetHeight()) == H && SunPixels.Num() == int64(W) * H * 2)
		{
			UTexture2D* SunTex = UTexture2D::CreateTransient(W, H, PF_G16);
			if (SunTex)
			{
				SunTex->SRGB = false;
				SunTex->Filter = TF_Bilinear;
				SunTex->AddressX = TA_Clamp;
				SunTex->AddressY = TA_Clamp;
				FTexture2DMipMap& SunMip = SunTex->GetPlatformData()->Mips[0];
				FMemory::Memcpy(SunMip.BulkData.Lock(LOCK_READ_WRITE), SunPixels.GetData(), SunPixels.Num());
				SunMip.BulkData.Unlock();
				SunTex->UpdateResource();
				// (17/10, a queda do Felipe ao atirar no chão: o carimbo da cratera leu a textura já recolhida) só a textura
				// principal do atlas fica num UPROPERTY; esta não tinha dono e o GC (~60 s) a levava no meio da partida. Presa
				// até o fim da partida (solta no Deinitialize)
				SunTex->AddToRoot();
				Out.SunTexture = SunTex;
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS chão: a altura da sombra %s_sun.png não abriu (o sol usa a altura cheia)"), *Base);
		}
	}
	Out.Variants = int32(Json->GetNumberField(TEXT("variants")));
	Out.Size = int32(Json->GetNumberField(TEXT("size")));
	Out.SpanCm = float(Json->GetNumberField(TEXT("span_m")) * 100.0);
	Out.RangeCm = float(Json->GetNumberField(TEXT("range_m")) * 100.0);
	FString Encoding;
	Out.bRamps = Json->TryGetStringField(TEXT("encoding"), Encoding) && Encoding == TEXT("ramp");
	const TArray<TSharedPtr<FJsonValue>>* Clods = nullptr;
	if (OutClods && Json->TryGetArrayField(TEXT("clods"), Clods))
	{
		OutClods->Reset();
		for (const TSharedPtr<FJsonValue>& V : *Clods)
		{
			TArray<FVector3f>& List = OutClods->AddDefaulted_GetRef();
			for (const TSharedPtr<FJsonValue>& C : V->AsArray())
			{
				const TArray<TSharedPtr<FJsonValue>>& A = C->AsArray();
				if (A.Num() >= 3)
				{
					List.Add(FVector3f(float(A[0]->AsNumber() * 100.0), float(A[1]->AsNumber() * 100.0), float(A[2]->AsNumber() * 100.0)));
				}
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RTS chão: atlas %s, %d variantes de %.1f m, %s%s"), *FPaths::GetBaseFilename(Base), Out.Variants, Out.SpanCm / 100.0f,
		Out.bRamps ? TEXT("em rampas") : TEXT("linear"), Out.SunTexture ? TEXT(", com a altura da sombra") : TEXT(""));
	return Out.Variants > 0 && Out.Size > 0;
}

UTextureRenderTarget2D* URTSGroundMemory::MakeReliefTarget(int32 Res)
{
	UTextureRenderTarget2D* T = NewObject<UTextureRenderTarget2D>(this);
	T->bCanCreateUAV = true;
	T->ClearColor = FLinearColor(0.5f, 1.0f, 1.0f, 0.0f);   // altura zero, velha, ao sol, intacta
	T->bAutoGenerateMips = GGroundV3;   // a v3: a cadeia de mips (o FlushMips refaz no que mudou)
	T->AddressX = TA_Clamp;
	T->AddressY = TA_Clamp;
	T->InitCustomFormat(Res, Res, PF_R8G8B8A8, true);
	T->UpdateResourceImmediate(true);
	return T;
}

UTextureRenderTarget2D* URTSGroundMemory::MakeAlongTarget(int32 Res)
{
	// G16 (0,2 cm de U), linear; o andado só é lido sob a faixa, onde a cobertura é inteira
	UTextureRenderTarget2D* T = NewObject<UTextureRenderTarget2D>(this);
	T->ClearColor = FLinearColor::Black;
	T->AddressX = TA_Clamp;
	T->AddressY = TA_Clamp;
	T->InitCustomFormat(Res, Res, PF_G16, true);
	T->UpdateResourceImmediate(true);
	return T;
}

void URTSGroundMemory::DrawAlong(UCanvas* Canvas, const FSheet& Sheet, const TArray<int32>& Indices) const
{
	if (Indices.IsEmpty() || !Sheet.Along || !GWhiteTexture)
	{
		return;
	}
	const FVector2D Scale(Sheet.Along->SizeX / Sheet.SizeCm.X, Sheet.Along->SizeY / Sheet.SizeCm.Y);
	const auto Px = [&Sheet, &Scale](const FVector2D& P) { return (P - Sheet.MinCm) * Scale; };
	TArray<FCanvasUVTri> Tris;
	const auto Flush = [&]()
	{
		if (!Tris.IsEmpty())
		{
			FCanvasTriangleItem Item(Tris, GWhiteTexture);
			Item.BlendMode = SE_BLEND_Opaque;   // a mais nova por cima (a ordem do tempo)
			Canvas->DrawItem(Item);
			Tris.Reset();
		}
	};
	for (const int32 I : Indices)
	{
		const FTrackQuad& Q = Quads[I];
		if (!Touches(Sheet, Q.L0, 300.0f))
		{
			continue;
		}
		const FVector2D C0 = (FVector2D(Q.L0) + FVector2D(Q.R0)) * 0.5, C1 = (FVector2D(Q.L1) + FVector2D(Q.R1)) * 0.5;
		const double W0 = (1.0 + CurveWiden * Q.Curve0) * Q.WidthK, W1 = (1.0 + CurveWiden * Q.Curve1) * Q.WidthK;
		const FVector2D N0 = (FVector2D(Q.R0) - FVector2D(Q.L0)).GetSafeNormal() * AlongHalfCm * W0;
		const FVector2D N1 = (FVector2D(Q.R1) - FVector2D(Q.L1)).GetSafeNormal() * AlongHalfCm * W1;
		const FVector2D L0 = Px(C0 - N0), R0 = Px(C0 + N0), L1 = Px(C1 - N1), R1 = Px(C1 + N1);
		const FLinearColor A0(Q.U0 / AlongRangeM, 0.0f, 0.0f, 1.0f), A1(Q.U1 / AlongRangeM, 0.0f, 0.0f, 1.0f);
		FCanvasUVTri T0;
		T0.V0_Pos = L0; T0.V0_UV = FVector2D(0.5, 0.5); T0.V0_Color = A0;
		T0.V1_Pos = R0; T0.V1_UV = FVector2D(0.5, 0.5); T0.V1_Color = A0;
		T0.V2_Pos = R1; T0.V2_UV = FVector2D(0.5, 0.5); T0.V2_Color = A1;
		FCanvasUVTri T1;
		T1.V0_Pos = L0; T1.V0_UV = FVector2D(0.5, 0.5); T1.V0_Color = A0;
		T1.V1_Pos = R1; T1.V1_UV = FVector2D(0.5, 0.5); T1.V1_Color = A1;
		T1.V2_Pos = L1; T1.V2_UV = FVector2D(0.5, 0.5); T1.V2_Color = A1;
		Tris.Add(T0);
		Tris.Add(T1);
		if (Tris.Num() >= TrisPerItem)
		{
			Flush();
		}
	}
	Flush();
}

UTextureRenderTarget2D* URTSGroundMemory::MakeProfileTarget(int32 Res)
{
	// 10 bits: o frescor desbota × 0,987 a cada 2 s; no 8 bits o arredondamento travava em ~0,15 (a fita
	// nunca ficava velha), no de 10 trava em ~0,04
	UTextureRenderTarget2D* T = UKismetRenderingLibrary::CreateRenderTarget2D(WorldRef.Get(), Res, Res, RTF_RGB10A2, FLinearColor::Black, false);
	if (T)
	{
		T->AddressX = TA_Clamp;
		T->AddressY = TA_Clamp;
		T->UpdateResourceImmediate(true);
	}
	return T;
}

void URTSGroundMemory::ReadSun()
{
	UWorld* World = WorldRef.Get();
	if (!World)
	{
		return;
	}
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		// a luz anda na direção do Forward: o sol fica do lado contrário
		const FVector F = It->GetLightComponent() ? It->GetLightComponent()->GetForwardVector() : It->GetActorForwardVector();
		const FVector2D Toward(-F.X, -F.Y);
		if (Toward.SizeSquared() > 1e-6)
		{
			SunDir = FVector2f(Toward.GetSafeNormal());
		}
		const float Elev = FMath::Asin(FMath::Clamp(float(-F.Z), 0.02f, 0.999f));
		SunTan = FMath::Tan(Elev);
		break;
	}
}

namespace
{
	// Os parâmetros do Landscape (SetLandscapeMaterial*ParameterValue) só chegam ao chão com bUseDynamicMaterialInstance,
	// e a Unreal só cria as instâncias dinâmicas no registro do pedaço (ULandscapeComponent::OnRegister). O mapa do
	// WorldForge vem sem: ligo aqui, crio as instâncias de cada pedaço do mesmo jeito e ponho as mesmas na malha Nanite
	// do chão, que desenha com o material do pedaço (ULandscapeNaniteComponent::UpdateMaterials copia o
	// GetMaterialInstance(0)). Devolve quantas casas da malha Nanite trocaram.
	int32 MakeLandscapeDynamic(ALandscapeProxy* Proxy, int32& OutComponents)
	{
		TMap<UMaterialInterface*, UMaterialInterface*> ToDynamic;
		Proxy->bUseDynamicMaterialInstance = true;
		for (ULandscapeComponent* C : Proxy->LandscapeComponents)
		{
			if (!C)
			{
				continue;
			}
			if (C->MaterialInstancesDynamic.Num() != C->MaterialInstances.Num())
			{
				C->MaterialInstancesDynamic.Reset();
				for (UMaterialInstanceConstant* MIC : C->MaterialInstances)
				{
					C->MaterialInstancesDynamic.Add(MIC ? UMaterialInstanceDynamic::Create(MIC, C) : nullptr);
				}
			}
			for (int32 I = 0; I < C->MaterialInstances.Num(); ++I)
			{
				if (C->MaterialInstances[I] && C->MaterialInstancesDynamic[I])
				{
					ToDynamic.Add(C->MaterialInstances[I], C->MaterialInstancesDynamic[I]);
				}
			}
			C->MarkRenderStateDirty();
			++OutComponents;
		}
		int32 Slots = 0;
		for (ULandscapeNaniteComponent* N : Proxy->NaniteComponents)
		{
			UStaticMeshComponent* Mesh = N;
			if (!Mesh)
			{
				continue;
			}
			for (int32 I = 0; I < Mesh->GetNumMaterials(); ++I)
			{
				if (UMaterialInterface** D = ToDynamic.Find(Mesh->GetMaterial(I)))
				{
					Mesh->SetMaterial(I, *D);
					++Slots;
				}
			}
		}
		return Slots;
	}
}

void URTSGroundMemory::SetSurface(const TArray<FColor>& Pixels, int32 Res, const FVector2D& MinM, float CellM)
{
	SurfaceTex = nullptr;
	if (!GGroundV3 || Res <= 0 || Pixels.Num() != Res * Res)
	{
		return;
	}
	UTexture2D* Tex = UTexture2D::CreateTransient(Res, Res, PF_B8G8R8A8, TEXT("RTSGround.SurfaceMap"));
	if (!Tex)
	{
		return;
	}
	Tex->SRGB = false;
	Tex->Filter = TF_Bilinear;
	Tex->AddressX = TA_Clamp;
	Tex->AddressY = TA_Clamp;
	Tex->NeverStream = true;
	FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
	FColor* Out = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	FMemory::Memcpy(Out, Pixels.GetData(), Pixels.Num() * sizeof(FColor));   // a linha 0 é o −Y, a mesma do V do caderno
	Mip.BulkData.Unlock();
	Tex->UpdateResource();
	SurfaceTex = Tex;
	SurfaceMinCm = FVector2f(MinM * 100.0);
	SurfaceSizeCm = float(Res) * CellM * 100.0f;
	UE_LOG(LogTemp, Log, TEXT("RTS chão v3: o sulco por tipo de chão ligado (%d × %d a %.1f m; rts.Ground.Surface 0 desliga)"), Res, Res, CellM);
}

void URTSGroundMemory::SetDisplacement(bool bOn)
{
	if (bDispForced || bDispOn == bOn)
	{
		return;
	}
	bDispOn = bOn;
	if (UWorld* World = WorldRef.Get())
	{
		for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
		{
			It->SetLandscapeMaterialScalarParameterValue(TEXT("DispOn"), bDispOn ? 1.0f : 0.0f);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RTS chão: o afundar de verdade %s (preset)"), bDispOn ? TEXT("ligado") : TEXT("desligado"));
}

void URTSGroundMemory::PushToSlabs(bool bBounds)
{
	UWorld* World = WorldRef.Get();
	if (!World || !Detail.bPlaced || !Mid.bPlaced)
	{
		return;
	}
	static const FName SlabTag(TEXT("WorldForgeRoadSlab"));
	int32 Added = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!It->ActorHasTag(SlabTag))
		{
			continue;
		}
		TInlineComponentArray<UMeshComponent*> Meshes(*It);
		for (UMeshComponent* Mc : Meshes)
		{
			if (!Mc)
			{
				continue;
			}
			const bool bNew = !PushedSlabs.Contains(Mc);
			if (!bNew && !bBounds)
			{
				continue;
			}
			for (int32 Slot = 0; Slot < Mc->GetNumMaterials(); ++Slot)
			{
				UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mc->GetMaterial(Slot));
				if (!MID)
				{
					UMaterialInterface* Base = Mc->GetMaterial(Slot);
					float Use = 0.0f;
					if (!Base || !Base->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("UseCrater")), Use))
					{
						continue;   // não é o material da laje com a MF
					}
					MID = Mc->CreateDynamicMaterialInstance(Slot, Base);
					SlabMIDs.Add(MID);
				}
				if (!MID)
				{
					continue;
				}
				if (bNew)
				{
					MID->SetTextureParameterValue(TEXT("GroundRelief"), DetailRelief);
					MID->SetTextureParameterValue(TEXT("GroundReliefMid"), MidRelief);
					MID->SetTextureParameterValue(TEXT("GroundTrack"), DetailTarget);
					MID->SetTextureParameterValue(TEXT("GroundTrackMid"), MidTarget);
					MID->SetTextureParameterValue(TEXT("GroundCrater"), DetailCrater);
					MID->SetTextureParameterValue(TEXT("GroundCraterMid"), MidCrater);
					MID->SetScalarParameterValue(TEXT("UseCrater"), CraterAtlases[0].Texture || CraterAtlases[1].Texture || CraterAtlases[2].Texture || WreckAtlas.Texture ? 1.0f : 0.0f);
					MID->SetScalarParameterValue(TEXT("ReliefRangeCm"), RangeCmNow());
					MID->SetScalarParameterValue(TEXT("ReliefTexel"), 1.0f / ReliefRes);
					MID->SetScalarParameterValue(TEXT("HeatNits"), 3000.0f);
					MID->SetScalarParameterValue(TEXT("MarkRamps"), bMarkRamps ? 1.0f : 0.0f);
					if (DetailSunLit && MidSunLit)
					{
						MID->SetTextureParameterValue(TEXT("GroundSunLit"), DetailSunLit);
						MID->SetTextureParameterValue(TEXT("GroundSunLitMid"), MidSunLit);
						MID->SetScalarParameterValue(TEXT("UseSunLit"), 1.0f);
					}
					if (BattleTarget)
					{
						MID->SetTextureParameterValue(TEXT("GroundBattle"), BattleTarget);
						MID->SetVectorParameterValue(TEXT("BattleBounds"), FLinearColor(Coarse.MinCm.X, Coarse.MinCm.Y, Coarse.SizeCm.X, Coarse.SizeCm.Y));
					}
					// (16/10, o formato do Look) a borracha no B do relevo: a MF só lê com isto (o padrão dela é 0)
					MID->SetScalarParameterValue(TEXT("UseRubber"), SurfaceTex && CVarGroundSurface.GetValueOnGameThread() != 0 ? 1.0f : 0.0f);
				}
				MID->SetVectorParameterValue(TEXT("DetailBounds"), FLinearColor(Detail.MinCm.X, Detail.MinCm.Y, Detail.SizeCm.X, Detail.SizeCm.Y));
				MID->SetVectorParameterValue(TEXT("MidBounds"), FLinearColor(Mid.MinCm.X, Mid.MinCm.Y, Mid.SizeCm.X, Mid.SizeCm.Y));
			}
			if (bNew)
			{
				PushedSlabs.Add(Mc);
				++Added;
			}
		}
	}
	if (Added > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("RTS chão: %d lajes da estrada com os cadernos (%d no total)"), Added, PushedSlabs.Num());
	}
}

void URTSGroundMemory::PushToTrample(bool bBounds)
{
	UWorld* World = WorldRef.Get();
	if (!World || !Detail.bPlaced || !Mid.bPlaced)
	{
		return;
	}
	static const FName TrampleTag(TEXT("WorldForgeTrample"));
	const float UseTrample = CVarGroundTrample.GetValueOnGameThread() != 0 && BattleTarget ? 1.0f : 0.0f;
	int32 Added = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const bool bActorTagged = It->ActorHasTag(TrampleTag);
		TInlineComponentArray<UMeshComponent*> Meshes(*It);
		for (UMeshComponent* Mc : Meshes)
		{
			if (!Mc || !(bActorTagged || Mc->ComponentHasTag(TrampleTag)))
			{
				continue;
			}
			const bool bNew = !PushedTrample.Contains(Mc);
			if (!bNew && !bBounds)
			{
				continue;
			}
			for (int32 Slot = 0; Slot < Mc->GetNumMaterials(); ++Slot)
			{
				UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mc->GetMaterial(Slot));
				if (!MID)
				{
					UMaterialInterface* Base = Mc->GetMaterial(Slot);
					float Use = 0.0f;
					if (!Base || !Base->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("UseTrample")), Use))
					{
						continue;   // não é o material das plantas do WorldForge
					}
					MID = Mc->CreateDynamicMaterialInstance(Slot, Base);
					SlabMIDs.Add(MID);
				}
				if (!MID)
				{
					continue;
				}
				if (bNew)
				{
					if (BattleTarget)
					{
						MID->SetTextureParameterValue(TEXT("GroundBattle"), BattleTarget);
						MID->SetVectorParameterValue(TEXT("BattleBounds"), FLinearColor(Coarse.MinCm.X, Coarse.MinCm.Y, Coarse.SizeCm.X, Coarse.SizeCm.Y));
					}
					MID->SetTextureParameterValue(TEXT("GroundTrack"), DetailTarget);
					MID->SetTextureParameterValue(TEXT("GroundTrackMid"), MidTarget);
				}
				MID->SetScalarParameterValue(TEXT("UseTrample"), UseTrample);
				MID->SetVectorParameterValue(TEXT("DetailBounds"), FLinearColor(Detail.MinCm.X, Detail.MinCm.Y, Detail.SizeCm.X, Detail.SizeCm.Y));
				MID->SetVectorParameterValue(TEXT("MidBounds"), FLinearColor(Mid.MinCm.X, Mid.MinCm.Y, Mid.SizeCm.X, Mid.SizeCm.Y));
			}
			if (bNew)
			{
				PushedTrample.Add(Mc);
				++Added;
			}
		}
	}
	if (Added > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("RTS chão: %d grupos de plantas com o caderno da batalha (%d no total, UseTrample %.0f)"), Added, PushedTrample.Num(), UseTrample);
	}
}

void URTSGroundMemory::PushToLandscape(bool bBounds)
{
	UWorld* World = WorldRef.Get();
	if (!World || !Detail.bPlaced || !Mid.bPlaced)
	{
		return;
	}
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		const bool bNew = !PushedProxies.Contains(*It);
		if (!bNew && !bBounds)
		{
			continue;
		}
		if (bNew)
		{
			PushedProxies.Add(*It);
			// a MF do Look no chão do mapa (o WorldForge liga): ela pinta como fica, e o decal de piso sai
			float Use = 0.0f;
			UMaterialInterface* LM = It->GetLandscapeMaterial();
			const bool bHasUse = LM && LM->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("UseCrater")), Use);
			if (PushedProxies.Num() <= 3)
			{
				UE_LOG(LogTemp, Log, TEXT("RTS chão: pedaço do Landscape %s, material %s (%s), UseCrater %s"), *It->GetName(),
					LM ? *LM->GetName() : TEXT("nenhum"), LM ? *LM->GetMaterial()->GetName() : TEXT("-"), bHasUse ? TEXT("sim") : TEXT("não"));
			}
			if (bHasUse && !It->bUseDynamicMaterialInstance)
			{
				int32 Components = 0;
				const int32 Slots = MakeLandscapeDynamic(*It, Components);
				UE_LOG(LogTemp, Log, TEXT("RTS chão: %s com instâncias dinâmicas (%d pedaços; %d casas da malha Nanite)"), *It->GetName(), Components, Slots);
			}
			if (!bLookGround && bHasUse)
			{
				bLookGround = true;
				// a MF pinta o escuro e o claro só pelo caderno de detalhe, que ela deixa de ler quando o pixel do chão
				// passa de ~9 cm (a câmera do RTS): sem o caderno do meio nela, o decal fica só com essas marcas
				UTexture* TrackMid = nullptr;
				const bool bReadsMid = LM->GetTextureParameterValue(FHashedMaterialParameterInfo(TEXT("GroundTrackMid")), TrackMid);
				if (FParse::Param(FCommandLine::Get(), TEXT("RTSGroundDecal")))
				{
					UE_LOG(LogTemp, Log, TEXT("RTS chão: -RTSGroundDecal, o decal de piso fica inteiro por cima da MF (A/B)"));
				}
				else if (bReadsMid)
				{
					if (DecalActor)
					{
						DecalActor->SetActorHiddenInGame(true);
					}
					UE_LOG(LogTemp, Log, TEXT("RTS chão: o chão do mapa já lê os cadernos (%s, a MF do Look, com o do meio): o decal de piso fica desligado"), *LM->GetName());
				}
				else
				{
					if (DecalMID)
					{
						DecalMID->SetScalarParameterValue(TEXT("MarksOnly"), 1.0f);
					}
					UE_LOG(LogTemp, Log, TEXT("RTS chão: o chão do mapa já lê os cadernos (%s, a MF do Look); o decal fica só com o escuro e o claro de longe"), *LM->GetName());
				}
			}
			It->SetLandscapeMaterialTextureParameterValue(TEXT("GroundRelief"), DetailRelief);
			It->SetLandscapeMaterialTextureParameterValue(TEXT("GroundReliefMid"), MidRelief);
			It->SetLandscapeMaterialTextureParameterValue(TEXT("GroundTrack"), DetailTarget);
			It->SetLandscapeMaterialTextureParameterValue(TEXT("GroundTrackMid"), MidTarget);   // (pra quando a MF ler o meio)
			It->SetLandscapeMaterialTextureParameterValue(TEXT("GroundCrater"), DetailCrater);
			It->SetLandscapeMaterialTextureParameterValue(TEXT("GroundCraterMid"), MidCrater);
			// a MF do Look só lê as crateras com isto ligado (o padrão dela é 0)
			It->SetLandscapeMaterialScalarParameterValue(TEXT("UseCrater"), CraterAtlases[0].Texture || CraterAtlases[1].Texture || CraterAtlases[2].Texture || WreckAtlas.Texture ? 1.0f : 0.0f);
			It->SetLandscapeMaterialScalarParameterValue(TEXT("ReliefRangeCm"), RangeCmNow());
			if (DetailSunLit && MidSunLit)
			{
				// a luz assada (a v3; a MF do Look gira a normal até o N·L dar ela). Sem a chave, os padrões da MF
				It->SetLandscapeMaterialTextureParameterValue(TEXT("GroundSunLit"), DetailSunLit);
				It->SetLandscapeMaterialTextureParameterValue(TEXT("GroundSunLitMid"), MidSunLit);
				It->SetLandscapeMaterialScalarParameterValue(TEXT("UseSunLit"), 1.0f);
			}
			// (16/10) o acúmulo da batalha (docs/MARCAS_DA_BATALHA.md) pro Look e o WorldForge: o tráfego, o combustível, o
			// queimado largo e a densidade de crateras, nos limites do grosso (fixos a partida toda)
			if (BattleTarget)
			{
				It->SetLandscapeMaterialTextureParameterValue(TEXT("GroundBattle"), BattleTarget);
				It->SetLandscapeMaterialVectorParameterValue(TEXT("BattleBounds"), FLinearColor(Coarse.MinCm.X, Coarse.MinCm.Y, Coarse.SizeCm.X, Coarse.SizeCm.Y));
			}
			// (16/10, o formato do Look) a borracha no B do relevo: a MF só lê com isto (o padrão dela é 0)
			It->SetLandscapeMaterialScalarParameterValue(TEXT("UseRubber"), SurfaceTex && CVarGroundSurface.GetValueOnGameThread() != 0 ? 1.0f : 0.0f);
			// o chão que afunda (o deslocamento Nanite; sem o DispOn no material, o chão fica como era)
			It->SetLandscapeMaterialScalarParameterValue(TEXT("DispOn"), bDispOn ? 1.0f : 0.0f);
			float HasDisp = 0.0f;
			if (PushedProxies.Num() == 1)
			{
				UE_LOG(LogTemp, Log, TEXT("RTS chão: o afundar de verdade %s (%s)"), bDispOn ? TEXT("ligado") : TEXT("desligado"),
					LM && LM->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("DispOn")), HasDisp) ? TEXT("o chão tem DispOn") : TEXT("o chão ainda não tem DispOn"));
			}
			It->SetLandscapeMaterialScalarParameterValue(TEXT("ReliefTexel"), 1.0f / ReliefRes);
			// a brasa do avental no mesmo pico do miolo da carcaça (o M_Structure: ~3000 nits × calor², que com o
			// ev100 14,3 do jogo lê laranja); o padrão da MF (40 000) estourava em manchas amarelo-claras (carcaca3)
			It->SetLandscapeMaterialScalarParameterValue(TEXT("HeatNits"), 3000.0f);
			// (13/10, o plano com o Look) o G e o A em rampas: a MF corta a borda nítida
			It->SetLandscapeMaterialScalarParameterValue(TEXT("MarkRamps"), bMarkRamps ? 1.0f : 0.0f);
			float HasRamps = 0.0f;
			if (bMarkRamps && LM && !LM->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("MarkRamps")), HasRamps))
			{
				UE_LOG(LogTemp, Warning, TEXT("RTS chão: os atlas vieram em rampas, mas o chão (%s) não tem MarkRamps: as marcas saem erradas"), *LM->GetName());
			}
		}
		It->SetLandscapeMaterialVectorParameterValue(TEXT("DetailBounds"), FLinearColor(Detail.MinCm.X, Detail.MinCm.Y, Detail.SizeCm.X, Detail.SizeCm.Y));
		It->SetLandscapeMaterialVectorParameterValue(TEXT("MidBounds"), FLinearColor(Mid.MinCm.X, Mid.MinCm.Y, Mid.SizeCm.X, Mid.SizeCm.Y));
	}
}

UTextureRenderTarget2D* URTSGroundMemory::MakeSunLitTarget(int32 Res)
{
	UTextureRenderTarget2D* T = NewObject<UTextureRenderTarget2D>(this);
	T->bCanCreateUAV = true;
	T->ClearColor = FLinearColor(1.0f, 1.0f, 0.0f, 0.0f);   // o chão liso: a luz do plano, o céu inteiro
	T->bAutoGenerateMips = true;   // (só a v3 tem luz assada) a cadeia de mips, refeita no que mudou
	T->AddressX = TA_Clamp;
	T->AddressY = TA_Clamp;
	T->InitCustomFormat(Res, Res, PF_G16R16F, true);
	T->UpdateResourceImmediate(true);
	return T;
}

bool URTSGroundMemory::Init(UWorld* World, const FVector2D& InMapMinCm, const FVector2D& InMapSizeCm, float GroundZMinCm, float GroundZMaxCm, float WindYawDeg,
	bool bMapGroundV3)
{
	WorldRef = World;
	// a chave liga em qualquer mapa; o mapa montado com o chão v3 liga sozinho, e -RTSNoGroundV3 desliga (o A/B)
	GGroundV3 = FParse::Param(FCommandLine::Get(), TEXT("RTSGroundV3"))
		|| (bMapGroundV3 && !FParse::Param(FCommandLine::Get(), TEXT("RTSNoGroundV3")));
	const float WindRad = FMath::DegreesToRadians(WindYawDeg);
	WindDir = FVector2f(FMath::Cos(WindRad), FMath::Sin(WindRad));
	UMaterialInterface* DecalMaterial = LoadObject<UMaterialInterface>(nullptr, DecalMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!World || !DecalMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS chão: sem o material %s (game.ps1 content): sem marcas no chão"), DecalMaterialPath);
		return false;
	}
	// o afundar de verdade no Alta e no Ultra (os níveis 3 e 4 da Unreal; nos menores o DefaultScalability.ini desliga a
	// tesselação inteira, que custa mesmo com o deslocamento zerado)
	{
		const Scalability::FQualityLevels Q = Scalability::GetQualityLevels();
		bDispOn = FMath::Min(Q.ShadingQuality, Q.ShadowQuality) >= 3;
		int32 Forced = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("RTSDisp="), Forced))
		{
			bDispOn = Forced != 0;
			bDispForced = true;
		}
	}
	Coarse.MinCm = InMapMinCm;
	Coarse.SizeCm = FVector2D(FMath::Max(InMapSizeCm.X, 100.0), FMath::Max(InMapSizeCm.Y, 100.0));
	Coarse.Res = CoarseRes;
	Coarse.Target = CoarseTarget = MakeTarget(CoarseRes);
	BattleTarget = MakeTarget(CoarseRes);   // (16/10) o acúmulo da batalha, nos limites do grosso
	BattleOverlay = MakeTarget(256);   // 4 m por texel: o pixel do minimapa
	Coarse.Crater = CoarseCrater = MakeCraterTarget(CoarseRes);
	Detail.SizeCm = FVector2D(DetailSizeCm, DetailSizeCm);
	Detail.Res = DetailRes;
	Detail.Target = DetailTarget = MakeTarget(DetailRes);
	Detail.Profile = DetailProfile = MakeProfileTarget(ReliefRes);
	if (GGroundV3)
	{
		Detail.Along = DetailAlong = MakeAlongTarget(ReliefRes);
	}
	Detail.Relief = DetailRelief = MakeReliefTarget(ReliefRes);
	Detail.Crater = DetailCrater = MakeCraterTarget(ReliefRes);
	Detail.AheadCm = DetailAheadCm;
	Detail.SlackCm = DetailSlackCm;
	Detail.Name = TEXT("detalhe");
	Mid.SizeCm = FVector2D(MidSizeCm, MidSizeCm);
	Mid.Res = MidRes;
	Mid.Target = MidTarget = MakeTarget(MidRes);
	Mid.Profile = MidProfile = MakeProfileTarget(MidRes);
	Mid.Relief = MidRelief = MakeReliefTarget(MidRes);
	Mid.Crater = MidCrater = MakeCraterTarget(MidRes);
	Mid.AheadCm = MidAheadCm;
	Mid.SlackCm = MidSlackCm;
	Mid.Name = TEXT("meio");
	for (FSheet* S : { &Detail, &Mid })
	{
		const float TexelCm = float(S->SizeCm.X / ReliefRes);
		S->Relief_.TexelCm = TexelCm;
		S->Relief_.RangeCm = RangeCmNow();
		S->Relief_.GrooveCm = GrooveCmNow();
		S->Relief_.BermCm = BermCmNow();
		S->Relief_.BermWidthCm = BermWidthCmNow();
		S->Relief_.BandScale = GGroundV3 ? CurveBandMax : 1.0f;
		S->Relief_.HaloCm = HaloCmNow();
		S->Relief_.BermScale = GGroundV3 ? CurveBermMax : 1.0f;
		S->Relief_.PenumbraCm = ReliefPenumbraCm;
	}
	bReliefCycle = FParse::Param(FCommandLine::Get(), TEXT("RTSGroundReliefCycle"));
	if (GGroundV3)
	{
		// a luz assada por preset (o estudo §5.4): Ultra 4,7 cm com 4 × 4, Alta 4,7 cm com 2 × 2, Média e Baixa 9,4 cm; o meio
		// a 18,75 cm
		const int32 Shading = Scalability::GetQualityLevels().ShadingQuality;
		const int32 LitRes = Shading >= 3 ? DetailRes : DetailRes / 2;
		Detail.SunLit = DetailSunLit = MakeSunLitTarget(LitRes);
		Detail.SunLitSS = Shading >= 4 ? 4 : (Shading >= 3 ? 2 : 1);
		Mid.SunLit = MidSunLit = MakeSunLitTarget(MidRes);
		Mid.SunLitSS = 1;
		UE_LOG(LogTemp, Log, TEXT("RTS chão v3: a luz assada a %.1f cm (%d × %d por texel), o meio a %.1f cm; a trilha com o sulco de 4 cm e a bordinha de 3 × 20"),
			DetailSizeCm / LitRes, Detail.SunLitSS, Detail.SunLitSS, MidSizeCm / MidRes);
	}
	TrackStamp = MakeStamp(128, 32, 0);
	BlotStamp = MakeStamp(256, 256, 1);
	CraterStamp = MakeStamp(64, 64, 2);
	Detail.ProfileStamp = ProfileStampDetail = MakeProfileStamp(float(DetailSizeCm / ReliefRes));
	Mid.ProfileStamp = ProfileStampMid = MakeProfileStamp(float(MidSizeCm / MidRes));
	if (!CoarseTarget || !DetailTarget || !TrackStamp || !BlotStamp || !CraterStamp || !DetailProfile || !DetailRelief || !MidTarget || !MidProfile || !MidRelief
		|| !ProfileStampDetail || !ProfileStampMid || !CoarseCrater || !DetailCrater || !MidCrater)
	{
		return false;
	}
	// as marcas de explosão (13/10): sem o atlas, a cratera e o avental só não aparecem
	for (int32 K = 0; K < 3; ++K)
	{
		if (LoadAtlas(TEXT("crater"), CraterOptions[K], CraterAtlases[K], &CraterClods[K]))
		{
			CraterAtlasTex[K] = CraterAtlases[K].Texture;
		}
	}
	bMarkRamps = CraterAtlases[0].bRamps || CraterAtlases[1].bRamps || CraterAtlases[2].bRamps;
	// a mancha só vale com a mesma codificação das crateras (o mesmo canal G nos cadernos)
	if (LoadAtlas(TEXT("blot"), FString(), BlotAtlas) && BlotAtlas.bRamps == CraterAtlases[1].bRamps)
	{
		BlotAtlasTex = BlotAtlas.Texture;
	}
	else
	{
		BlotAtlas = FRTSGroundMarkAtlas();
	}
	if (LoadAtlas(TEXT("wreck"), WreckOption, WreckAtlas))
	{
		WreckAtlasTex = WreckAtlas.Texture;
	}
	ReadSun();
	Quads.Reserve(4096);
	Scars.Reserve(512);

	// o decal do mapa inteiro, olhando pra baixo (o X do decal é a profundidade da projeção). O decal
	// apaga nos 12,5% de cada ponta da profundidade (DeferredDecal.usf): a caixa vai bem além do relevo.
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Center(Coarse.MinCm.X + Coarse.SizeCm.X * 0.5, Coarse.MinCm.Y + Coarse.SizeCm.Y * 0.5, (GroundZMinCm + GroundZMaxCm) * 0.5);
	ADecalActor* Decal = World->SpawnActor<ADecalActor>(Center, FRotator(-90.0f, 0.0f, 0.0f), P);
	if (!Decal)
	{
		return false;
	}
	DecalActor = Decal;
	DecalMID = UMaterialInstanceDynamic::Create(DecalMaterial, this);
	DecalMID->SetTextureParameterValue(TEXT("GroundCoarse"), CoarseTarget);
	DecalMID->SetScalarParameterValue(TEXT("MarkRamps"), bMarkRamps ? 1.0f : 0.0f);
	DecalMID->SetTextureParameterValue(TEXT("GroundDetail"), DetailTarget);
	DecalMID->SetTextureParameterValue(TEXT("GroundRelief"), DetailRelief);
	DecalMID->SetTextureParameterValue(TEXT("GroundCraterCoarse"), CoarseCrater);
	DecalMID->SetTextureParameterValue(TEXT("GroundCraterDetail"), DetailCrater);
	DecalMID->SetVectorParameterValue(TEXT("CoarseBounds"), FLinearColor(Coarse.MinCm.X, Coarse.MinCm.Y, Coarse.SizeCm.X, Coarse.SizeCm.Y));
	DecalMID->SetVectorParameterValue(TEXT("DetailBounds"), FLinearColor(0.0f, 0.0f, -1.0f, -1.0f));   // ainda sem lugar
	DecalMID->SetVectorParameterValue(TEXT("Texels"), FLinearColor(1.0f / CoarseRes, 1.0f / DetailRes, 1.0f / ReliefRes, 0.0f));
	DecalMID->SetScalarParameterValue(TEXT("DebugView"), FParse::Param(FCommandLine::Get(), TEXT("RTSGroundDebug")) ? 1.0f : 0.0f);
	bDebugCycle = FParse::Param(FCommandLine::Get(), TEXT("RTSGroundDebugCycle"));
	UDecalComponent* C = Decal->GetDecal();
	C->SetDecalMaterial(DecalMID);
	C->DecalSize = FVector(FMath::Max(GroundZMaxCm - GroundZMinCm, 0.0f) + 2000.0, Coarse.SizeCm.Y * 0.5, Coarse.SizeCm.X * 0.5);
	C->SetFadeScreenSize(0.0f);
	C->SortOrder = -10;
	C->MarkRenderStateDirty();
	// (quando o material do chão do mapa já tem a MF do Look, o PushToLandscape desliga o decal de piso:
	// lá o Landscape já carregou; -RTSGroundDecal mantém, pro A/B)
	if (FParse::Param(FCommandLine::Get(), TEXT("RTSGroundDebug")))
	{
		// o que pode receber o decal: as maiores peças do mapa (o chão), se recebem decal e se são Nanite
		TArray<TPair<double, FString>> Big;
		for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
		{
			UPrimitiveComponent* Prim = *It;
			if (!Prim || Prim->GetWorld() != World || !Prim->IsRegistered())
			{
				continue;
			}
			const FBox B = Prim->Bounds.GetBox();
			const double Area = B.GetSize().X * B.GetSize().Y;
			if (Area < 1.0e8)   // menos de 100 × 100 m: não é o chão
			{
				continue;
			}
			const UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(Prim);
			Big.Emplace(Area, FString::Printf(TEXT("%s (%s) recebe decal %d, Nanite %d, visível %d, %.0f × %.0f m"), *Prim->GetOwner()->GetName(), *Prim->GetClass()->GetName(),
				int32(Prim->bReceivesDecals), SM && SM->GetStaticMesh() ? int32(SM->GetStaticMesh()->HasValidNaniteData()) : -1, int32(Prim->IsVisible()), B.GetSize().X / 100.0, B.GetSize().Y / 100.0));
		}
		Big.Sort([](const TPair<double, FString>& A, const TPair<double, FString>& B) { return A.Key > B.Key; });
		for (int32 K = 0; K < FMath::Min(Big.Num(), 12); ++K)
		{
			UE_LOG(LogTemp, Log, TEXT("RTS chão (depuração): %s"), *Big[K].Value);
		}
		UE_LOG(LogTemp, Log, TEXT("RTS chão (depuração): decal em %s, rotação %s, tamanho %s"), *Decal->GetActorLocation().ToString(), *Decal->GetActorRotation().ToString(), *C->DecalSize.ToString());
	}
	UE_LOG(LogTemp, Log, TEXT("RTS chão: memória %d² do mapa (%.0f cm por pixel) + %d² de detalhe (%.1f cm por pixel), relevo %.0f a %.0f m"),
		CoarseRes, Coarse.SizeCm.X / CoarseRes, DetailRes, DetailSizeCm / DetailRes, GroundZMinCm / 100.0f, GroundZMaxCm / 100.0f);
	return true;
}

void URTSGroundMemory::TrackUnit(uint32 Id, const FVector2D& PosCm, float YawDeg, float GaugeCm, float WidthCm, bool bShown, const FTrackStyle& Style)
{
	if (!CoarseTarget)
	{
		return;
	}
	FTrackPair& T = Tracks.FindOrAdd(Id);
	if (!bShown)
	{
		// fora da vista: a fita quebra aqui e recomeça onde ele for visto de novo
		T.Side[0].bValid = T.Side[1].bValid = false;
		return;
	}
	const double Yaw = FMath::DegreesToRadians(double(YawDeg));
	const FVector2D Lat(-FMath::Sin(Yaw), FMath::Cos(Yaw));
	for (int32 S = 0; S < 2; ++S)
	{
		const FVector2D C = PosCm + Lat * ((S == 0 ? -0.5 : 0.5) * GaugeCm);
		const FVector2D L = C - Lat * (WidthCm * 0.5);
		const FVector2D R = C + Lat * (WidthCm * 0.5);
		FTrackSide& Side = T.Side[S];
		const double Moved = FVector2D::Distance(C, Side.C);
		if (!Side.bValid || Moved > 3000.0)
		{
			// começo (ou um salto: nasceu, foi carregado): sem fita até aqui
			Side.L = L;
			Side.R = R;
			Side.C = C;
			Side.Along = 0.0f;
			Side.Yaw = YawDeg;
			Side.Curve = 0.0f;
			Side.Kappa = 0.0f;
			Side.bValid = true;
			continue;
		}
		const float DeltaYaw = FMath::FindDeltaAngleDegrees(Side.Yaw, YawDeg);
		const float Turned = FMath::Abs(DeltaYaw);
		if (Moved < SegmentCm * Style.SegmentK && Turned < SegmentTurnDeg)
		{
			continue;
		}
		// o raio da curva desta lagarta (o pivô: ~1,4 m, o meio da bitola) e o lado de fora: virando pra direita (o yaw
		// sobe) andando pra frente, o centro fica no +Lat e o de fora é o L; de ré, o contrário
		const double Ahead = FVector2D::DotProduct(C - Side.C, FVector2D(FMath::Cos(Yaw), FMath::Sin(Yaw)));
		const double DistM = FMath::Max(Moved / 100.0, 0.01);
		const float Sign = (DeltaYaw > 0.0f) == (Ahead >= -1.0) ? 1.0f : -1.0f;
		const float KappaNow = Sign * float(FMath::DegreesToRadians(double(Turned)) / DistM);
		Side.Kappa += (KappaNow - Side.Kappa) * (1.0f - FMath::Exp(-float(DistM) / CurveSmoothM));
		const float RadiusM = FMath::Abs(Side.Kappa) > 1.0e-4f ? 1.0f / FMath::Abs(Side.Kappa) : 1.0e4f;
		const float CurveEnd = 1.0f - FMath::SmoothStep(CurveFullRadiusM, CurveNoneRadiusM, RadiusM);
		FTrackQuad Q;
		Q.L0 = FVector2f(Side.L);
		Q.R0 = FVector2f(Side.R);
		Q.L1 = FVector2f(L);
		Q.R1 = FVector2f(R);
		Q.U0 = Side.U;
		Q.U1 = Side.U + float(Moved / 100.0) * Style.LugK;   // o carimbo é 1 m de lagarta: as garras seguem em fase (o pneu: mais miúdas)
		Q.Along0 = Side.Along;
		Q.Along1 = FMath::Min(Side.Along + float(Moved / 100.0), 10.0f);
		Q.Strength = TrackPass * Style.Dark;
		Q.Depth = Style.Depth;
		Q.WidthK = Style.WidthK;
		Q.Time = Clock;
		Q.Curve0 = Side.Curve;
		Q.Curve1 = CurveEnd;
		Q.Outer = CurveEnd < 0.01f ? 0 : int8(Side.Kappa > 0.0f ? 1 : -1);
		int32 Index;
		if (Quads.Num() < MaxQuads)
		{
			Index = Quads.Add(Q);
		}
		else
		{
			Index = QuadHead;
			Quads[Index] = Q;
			QuadHead = (QuadHead + 1) % MaxQuads;
		}
		NewQuads.Add(Index);
		Side.L = L;
		Side.R = R;
		Side.C = C;
		Side.U = FMath::Fmod(Q.U1, 64.0f);
		Side.Along = Q.Along1;
		Side.Curve = CurveEnd;
		Side.Yaw = YawDeg;
	}
}

void URTSGroundMemory::ForgetUnit(uint32 Id)
{
	Tracks.Remove(Id);
}

void URTSGroundMemory::AddScar(const FVector2D& AtCm, const FVector2D& FromCm, float RadiusCm, float Scorch, float Crater, int32 Size)
{
	if (!CoarseTarget)
	{
		return;
	}
	FVector2D Dir = AtCm - FromCm;
	const bool bAimed = Dir.SizeSquared() > 100.0 * 100.0;
	FScar S;
	S.At = FVector2f(AtCm);
	S.Dir = FVector2f(bAimed ? Dir.GetSafeNormal() : FVector2D(1.0, 0.0));
	S.RadiusCm = RadiusCm;
	S.Stretch = bAimed ? 1.6f : 1.0f;
	S.Scorch = Scorch;
	S.Crater = Crater;
	S.Time = Clock;
	// a cratera do atlas: a variante, o espelho, o tamanho (±12%) e o giro sorteados pelo lugar (o mesmo em
	// todo PC e todo redesenho); o giro fica a ±15° do tiro (sem atirador, qualquer um)
	const uint32 H = HashCombine(GetTypeHash(FMath::RoundToInt(AtCm.X)), GetTypeHash(FMath::RoundToInt(AtCm.Y)));
	S.Size = uint8(FMath::Clamp(Size, 0, 2));
	S.Kind = Crater > 0.0f && CraterAtlases[S.Size].Texture ? 1 : 0;
	const bool bStones = S.Kind == 1;
	// o tamanho bem distinto na câmera do jogo (as três opções do bake variam só 30% entre si): a do tiro pequena,
	// a pesada claramente grande (bacias de ~1,7 / 2,6 / 3,9 m)
	constexpr float SizeScale[3] = { 0.75f, 1.0f, 1.3f };
	S.Variant = int32(H % 61u);
	S.Mirror = (H & 0x100u) ? -1.0f : 1.0f;
	S.Scale = (0.88f + 0.24f * float((H >> 9) & 0xFFu) / 255.0f) * FMath::Clamp(RadiusCm / 420.0f, 0.4f, 1.6f) * SizeScale[S.Size];
	const float Jitter = FMath::DegreesToRadians(bAimed ? (float((H >> 17) & 0xFFu) / 255.0f - 0.5f) * 30.0f : float((H >> 17) & 0xFFu) / 255.0f * 360.0f);
	if (S.Kind == 1)
	{
		S.Dir = FVector2f(FVector2D(S.Dir).GetRotated(FMath::RadiansToDegrees(Jitter)));
	}
	// as pedras da cratera (a prova aprovada do Look): os torrões da variante, no mundo pelo mesmo giro, espelho e escala
	// do carimbo (o SampleMark do GroundCrater.usf: L = (D·Axis, D·Perp·Mirror) / Scale)
	const TArray<TArray<FVector3f>>& Variants = CraterClods[S.Size];
	if (bStones && OnCraterStones && !Variants.IsEmpty())
	{
		const TArray<FVector3f>& List = Variants[S.Variant % Variants.Num()];
		const FVector2D Axis(S.Dir);
		const FVector2D Perp(-Axis.Y, Axis.X);
		// as graúdas (≥ 20 cm no atlas) e as miúdas (só no zoom mais perto)
		TArray<FVector> Large;
		TArray<FVector> Small;
		for (const FVector3f& C : List)
		{
			const FVector2D P = AtCm + (Axis * double(C.X) + Perp * double(C.Y * S.Mirror)) * double(S.Scale);
			(C.Z >= 20.0f ? Large : Small).Add(FVector(P.X, P.Y, double(C.Z * S.Scale)));
		}
		OnCraterStones(Large, Small, H ^ 0x9E3779B9u);
	}
	int32 Index;
	if (Scars.Num() < MaxScars)
	{
		Index = Scars.Add(S);
	}
	else
	{
		Index = ScarHead;
		Scars[Index] = S;
		ScarHead = (ScarHead + 1) % MaxScars;
	}
	NewScars.Add(Index);
}

void URTSGroundMemory::AddWreck(const FVector2D& AtCm, float HullYawDeg, const FVector2D& FromCm)
{
	if (!CoarseTarget)
	{
		return;
	}
	if (!WreckAtlas.Texture)
	{
		// sem o atlas: o queimado antigo
		AddScar(AtCm, FromCm, 420.0f, 0.9f, 0.0f);
		return;
	}
	const uint32 H = HashCombine(GetTypeHash(FMath::RoundToInt(AtCm.X)), GetTypeHash(FMath::RoundToInt(AtCm.Y)) ^ 0x5bd1e995u);
	FScar S;
	S.At = FVector2f(AtCm);
	const float Yaw = FMath::DegreesToRadians(HullYawDeg);
	S.Dir = FVector2f(FMath::Cos(Yaw), FMath::Sin(Yaw));   // o +X do carimbo é a frente do casco
	S.RadiusCm = 420.0f;
	S.Scorch = 0.9f;
	S.Crater = 0.0f;
	S.Time = Clock;
	S.Kind = 2;
	S.Variant = int32(H % 61u);
	S.Mirror = (H & 0x100u) ? -1.0f : 1.0f;
	S.Scale = 1.0f;   // o casco tem tamanho fixo
	int32 Index;
	if (Scars.Num() < MaxScars)
	{
		Index = Scars.Add(S);
	}
	else
	{
		Index = ScarHead;
		Scars[Index] = S;
		ScarHead = (ScarHead + 1) % MaxScars;
	}
	NewScars.Add(Index);
}

void URTSGroundMemory::StampMarks(FSheet& Sheet, const TArray<int32>& Indices, bool bAged)
{
	if (!Sheet.Target || !Sheet.Crater || Indices.IsEmpty())
	{
		return;
	}
	TArray<FRTSGroundMarkParams> Marks;
	TArray<uint8> MarkSizes;
	for (const int32 I : Indices)
	{
		const FScar& S = Scars[I];
		if (S.Kind == 0)
		{
			// a mancha (13/10): o mesmo desenho do carimbo do Canvas de antes, esticada e empurrada pra frente do tiro,
			// mas pelo máximo (o Canvas somava, e a soma não convive com as rampas)
			if (BlotAtlas.Texture && S.Scorch > 0.0f && Touches(Sheet, S.At, S.RadiusCm * S.Stretch * 1.2f))
			{
				FRTSGroundMarkParams M;
				M.CenterCm = S.At + S.Dir * (S.Stretch > 1.0f ? S.RadiusCm * 0.3f : 0.0f);
				M.Axis = S.Dir;
				M.Scale = S.RadiusCm / 100.0f;
				M.Stretch = S.Stretch;
				M.Mirror = S.Mirror;
				M.Variant = S.Variant;
				M.HeightMul = 0.0f;
				M.DarkMul = S.Scorch;
				M.HeatMul = 0.0f;
				M.LightMul = 0.0f;
				Marks.Add(M);
				MarkSizes.Add(3);
			}
			continue;
		}
		const FRTSGroundMarkAtlas& A = S.Kind == 2 ? WreckAtlas : CraterAtlases[S.Size];
		if (!Touches(Sheet, S.At, A.SpanCm * S.Scale))
		{
			continue;
		}
		if (S.Kind == 1)
		{
			MarkCraterCells(Sheet, FVector2D(S.At), A.SpanCm * 0.5 * S.Scale);   // (a carcaça não tem altura de sol)
		}
		FRTSGroundMarkParams M;
		M.CenterCm = S.At;
		M.Axis = S.Dir;
		M.Scale = S.Scale;
		M.Mirror = S.Mirror;
		M.Variant = S.Variant;
		M.bWreck = S.Kind == 2;
		// a força: a cratera de morte (0,7) é a do atlas; as menores, mais rasas e menos escuras
		M.HeightMul = S.Kind == 1 ? FMath::Clamp(S.Crater / 0.7f, 0.3f, 1.4f) : 1.0f;
		M.DarkMul = FMath::Clamp(S.Scorch / 0.9f, 0.3f, 1.2f);
		M.HeatMul = bAged ? FMath::Pow(HeatDecay, (Clock - S.Time) / DecaySeconds) : 1.0f;
		M.LightMul = 1.0f;
		Marks.Add(M);
		MarkSizes.Add(S.Size);
	}
	// um carimbo por atlas (a composição é por marca: a altura soma, o resto pega o máximo; a ordem não importa)
	for (int32 K = 0; K < 3; ++K)
	{
		TArray<FRTSGroundMarkParams> Group;
		for (int32 J = 0; J < Marks.Num(); ++J)
		{
			if (!Marks[J].bWreck && MarkSizes[J] == K)
			{
				Group.Add(Marks[J]);
			}
		}
		if (!Group.IsEmpty())
		{
			RTSGroundRelief::StampMarks(Sheet.Target, Sheet.Crater, Sheet.MinCm, Sheet.SizeCm, CraterAtlases[K], WreckAtlas, Group);
		}
	}
	TArray<FRTSGroundMarkParams> Blots;
	for (int32 J = 0; J < Marks.Num(); ++J)
	{
		if (MarkSizes[J] == 3)
		{
			Blots.Add(Marks[J]);
		}
	}
	if (!Blots.IsEmpty())
	{
		RTSGroundRelief::StampMarks(Sheet.Target, Sheet.Crater, Sheet.MinCm, Sheet.SizeCm, BlotAtlas, WreckAtlas, Blots);
	}
	TArray<FRTSGroundMarkParams> Wrecks = Marks.FilterByPredicate([](const FRTSGroundMarkParams& M) { return M.bWreck; });
	if (!Wrecks.IsEmpty())
	{
		RTSGroundRelief::StampMarks(Sheet.Target, Sheet.Crater, Sheet.MinCm, Sheet.SizeCm, WreckAtlas, WreckAtlas, Wrecks);
	}
}

bool URTSGroundMemory::Touches(const FSheet& Sheet, const FVector2f& P, float MarginCm) const
{
	return P.X >= Sheet.MinCm.X - MarginCm && P.Y >= Sheet.MinCm.Y - MarginCm
		&& P.X <= Sheet.MinCm.X + Sheet.SizeCm.X + MarginCm && P.Y <= Sheet.MinCm.Y + Sheet.SizeCm.Y + MarginCm;
}

void URTSGroundMemory::DrawTracks(UCanvas* Canvas, const FSheet& Sheet, const TArray<int32>& Indices, bool bAged) const
{
	if (Indices.IsEmpty() || !TrackStamp || !TrackStamp->GetResource())
	{
		return;
	}
	const FVector2D Scale(Sheet.Res / Sheet.SizeCm.X, Sheet.Res / Sheet.SizeCm.Y);
	const auto Px = [&Sheet, &Scale](const FVector2f& P) { return (FVector2D(P) - Sheet.MinCm) * Scale; };
	TArray<FCanvasUVTri> Tris;
	Tris.Reserve(FMath::Min(Indices.Num() * 2, TrisPerItem));
	const auto Flush = [&]()
	{
		if (!Tris.IsEmpty())
		{
			FCanvasTriangleItem Item(Tris, TrackStamp->GetResource());
			Item.BlendMode = SE_BLEND_Additive;
			Canvas->DrawItem(Item);
			Tris.Reset();
		}
	};
	for (const int32 I : Indices)
	{
		const FTrackQuad& Q = Quads[I];
		if (!Touches(Sheet, Q.L0, 300.0f))
		{
			continue;
		}
		// a fita desbota com a idade (no redesenho, o mesmo que as passadas de desbote fizeram)
		const float K = bAged ? Q.Strength * FMath::Pow(TrackDecay, (Clock - Q.Time) / DecaySeconds) : Q.Strength;
		if (K < 0.015f)
		{
			continue;
		}
		const FLinearColor Col(K, 0.0f, 0.0f, 1.0f);
		const FVector2D L0 = Px(Q.L0), R0 = Px(Q.R0), L1 = Px(Q.L1), R1 = Px(Q.R1);
		FCanvasUVTri A;
		A.V0_Pos = L0; A.V0_UV = FVector2D(Q.U0, 0.0); A.V0_Color = Col;
		A.V1_Pos = R0; A.V1_UV = FVector2D(Q.U0, 1.0); A.V1_Color = Col;
		A.V2_Pos = R1; A.V2_UV = FVector2D(Q.U1, 1.0); A.V2_Color = Col;
		FCanvasUVTri B;
		B.V0_Pos = L0; B.V0_UV = FVector2D(Q.U0, 0.0); B.V0_Color = Col;
		B.V1_Pos = R1; B.V1_UV = FVector2D(Q.U1, 1.0); B.V1_Color = Col;
		B.V2_Pos = L1; B.V2_UV = FVector2D(Q.U1, 0.0); B.V2_Color = Col;
		Tris.Add(A);
		Tris.Add(B);
		if (Tris.Num() >= TrisPerItem)
		{
			Flush();
		}
	}
	Flush();
}

void URTSGroundMemory::DrawProfile(UCanvas* Canvas, const FSheet& Sheet, const TArray<int32>& Indices, bool bAged) const
{
	if (Indices.IsEmpty() || !Sheet.Profile || !Sheet.ProfileStamp || !Sheet.ProfileStamp->GetResource())
	{
		return;
	}
	const FVector2D Scale(Sheet.Profile->SizeX / Sheet.SizeCm.X, Sheet.Profile->SizeY / Sheet.SizeCm.Y);
	const auto Px = [&Sheet, &Scale](const FVector2D& P) { return (P - Sheet.MinCm) * Scale; };
	TArray<FCanvasUVTri> Tris;
	const auto Flush = [&]()
	{
		if (!Tris.IsEmpty())
		{
			FCanvasTriangleItem Item(Tris, Sheet.ProfileStamp->GetResource());
			Item.BlendMode = SE_BLEND_Translucent;   // a nova redesenha o chão por cima da velha, pela cobertura dela
			Canvas->DrawItem(Item);
			Tris.Reset();
		}
	};
	for (const int32 I : Indices)
	{
		const FTrackQuad& Q = Quads[I];
		if (!Touches(Sheet, Q.L0, 300.0f))
		{
			continue;
		}
		const float F = bAged ? FMath::Pow(TrackDecay, (Clock - Q.Time) / DecaySeconds) : 1.0f;
		// a fita alargada pra ±60 cm em volta do meio da lagarta (a bordinha e a cobertura passam dos 32 cm dela)
		const FVector2D C0 = (FVector2D(Q.L0) + FVector2D(Q.R0)) * 0.5, C1 = (FVector2D(Q.L1) + FVector2D(Q.R1)) * 0.5;
		// (v3) a curva alarga a fita inteira (a faixa, a bordinha e a cobertura juntas); (23/10) o pneu leva o perfil na largura dele
		const double W0 = (GGroundV3 ? 1.0 + CurveWiden * Q.Curve0 : 1.0) * Q.WidthK, W1 = (GGroundV3 ? 1.0 + CurveWiden * Q.Curve1 : 1.0) * Q.WidthK;
		const FVector2D N0 = (FVector2D(Q.R0) - FVector2D(Q.L0)).GetSafeNormal() * ProfileHalfNow() * W0;
		const FVector2D N1 = (FVector2D(Q.R1) - FVector2D(Q.L1)).GetSafeNormal() * ProfileHalfNow() * W1;
		const FVector2D L0 = Px(C0 - N0), R0 = Px(C0 + N0), L1 = Px(C1 - N1), R1 = Px(C1 + N1);
		// as pontas somem macias em 1,2 m (o corte seco desenhava um anel de sombra na ponta)
		const float Fade0 = FMath::SmoothStep(0.0f, EndFadeM, Q.Along0), Fade1 = FMath::SmoothStep(0.0f, EndFadeM, Q.Along1);
		if (GGroundV3)
		{
			// a faixa: o sulco afunda com a curva (÷ a folga); a bordinha: × (1 ± curva) do lado de fora e do de dentro (÷ a
			// folga), uma cor por metade da fita (a bordinha mora perto da borda, longe do meio)
			// (23/10) o pneu: o sulco e a bordinha mais rasos (o revolvido do relevo vem deles)
			const float D0 = (1.0f + CurveDeepen * Q.Curve0) / CurveBandMax * Q.Depth, D1 = (1.0f + CurveDeepen * Q.Curve1) / CurveBandMax * Q.Depth;
			const float SideL = float(Q.Outer), SideR = -float(Q.Outer);
			const FVector2D M0 = Px(C0), M1 = Px(C1);
			const auto Half = [&](const FVector2D& A0, const FVector2D& A1, double V, float Side)
			{
				// A0/A1: a borda desta metade (no começo e no fim), V: o V do carimbo nela (0 o L, 1 o R)
				const FLinearColor E0(D0, (1.0f + Side * Q.Curve0) / CurveBermMax * Q.Depth, F, Fade0);
				const FLinearColor E1(D1, (1.0f + Side * Q.Curve1) / CurveBermMax * Q.Depth, F, Fade1);
				FCanvasUVTri T0;
				T0.V0_Pos = A0; T0.V0_UV = FVector2D(Q.U0, V); T0.V0_Color = E0;
				T0.V1_Pos = M0; T0.V1_UV = FVector2D(Q.U0, 0.5); T0.V1_Color = E0;
				T0.V2_Pos = M1; T0.V2_UV = FVector2D(Q.U1, 0.5); T0.V2_Color = E1;
				FCanvasUVTri T1;
				T1.V0_Pos = A0; T1.V0_UV = FVector2D(Q.U0, V); T1.V0_Color = E0;
				T1.V1_Pos = M1; T1.V1_UV = FVector2D(Q.U1, 0.5); T1.V1_Color = E1;
				T1.V2_Pos = A1; T1.V2_UV = FVector2D(Q.U1, V); T1.V2_Color = E1;
				Tris.Add(T0);
				Tris.Add(T1);
			};
			Half(L0, L1, 0.0, SideL);
			Half(R0, R1, 1.0, SideR);
			if (Tris.Num() >= TrisPerItem)
			{
				Flush();
			}
			continue;
		}
		const FLinearColor Col0(Q.Depth, Q.Depth, F, Fade0);
		const FLinearColor Col1(Q.Depth, Q.Depth, F, Fade1);
		FCanvasUVTri A;
		A.V0_Pos = L0; A.V0_UV = FVector2D(Q.U0, 0.0); A.V0_Color = Col0;
		A.V1_Pos = R0; A.V1_UV = FVector2D(Q.U0, 1.0); A.V1_Color = Col0;
		A.V2_Pos = R1; A.V2_UV = FVector2D(Q.U1, 1.0); A.V2_Color = Col1;
		FCanvasUVTri B;
		B.V0_Pos = L0; B.V0_UV = FVector2D(Q.U0, 0.0); B.V0_Color = Col0;
		B.V1_Pos = R1; B.V1_UV = FVector2D(Q.U1, 1.0); B.V1_Color = Col1;
		B.V2_Pos = L1; B.V2_UV = FVector2D(Q.U1, 0.0); B.V2_Color = Col1;
		Tris.Add(A);
		Tris.Add(B);
		if (Tris.Num() >= TrisPerItem)
		{
			Flush();
		}
	}
	Flush();
}

FIntRect URTSGroundMemory::DirtyRect(const FSheet& Sheet) const
{
	if (!Sheet.Relief)
	{
		return FIntRect();
	}
	const FVector2D Scale(Sheet.Relief->SizeX / Sheet.SizeCm.X, Sheet.Relief->SizeY / Sheet.SizeCm.Y);
	FBox2D Box(ForceInit);
	for (const int32 I : NewQuads)
	{
		const FTrackQuad& Q = Quads[I];
		for (const FVector2f& P : { Q.L0, Q.R0, Q.L1, Q.R1 })
		{
			Box += (FVector2D(P) - Sheet.MinCm) * Scale;
		}
	}
	for (const int32 I : NewScars)
	{
		const FScar& S = Scars[I];
		const FVector2D C = (FVector2D(S.At) - Sheet.MinCm) * Scale;
		const double R = (S.Kind == 0 ? S.RadiusCm * 2.0 : (S.Kind == 2 ? WreckAtlas.SpanCm : CraterAtlases[S.Size].SpanCm) * 0.71 * S.Scale) * Scale.X;
		Box += C - FVector2D(R, R);
		Box += C + FVector2D(R, R);
	}
	if (!Box.bIsValid)
	{
		return FIntRect();
	}
	// a margem: o perfil passa da lagarta (28 cm na v2; na v3 o carimbo vai a 85 cm do meio, até 1,32 m na curva) e a
	// bordinha velha alarga mais ~1 texel
	const int32 M = FMath::CeilToInt(FMath::Max(40.0, ProfileReachNow() - 32.0 + 10.0 + HaloCmNow()) * Scale.X) + 2;
	const FIntRect R(FMath::FloorToInt(Box.Min.X) - M, FMath::FloorToInt(Box.Min.Y) - M, FMath::CeilToInt(Box.Max.X) + M, FMath::CeilToInt(Box.Max.Y) + M);
	const FIntRect Out(FMath::Max(R.Min.X, 0), FMath::Max(R.Min.Y, 0), FMath::Min(R.Max.X, int32(Sheet.Relief->SizeX)), FMath::Min(R.Max.Y, int32(Sheet.Relief->SizeY)));
	// (14/10, o travamento da placa no jogo do Felipe: a marca toda fora do caderno, na diagonal, dava largura E altura
	// negativas, a área "positiva" passava e o compute saía com bilhões de grupos) fora do caderno = nada
	return Out.Width() > 0 && Out.Height() > 0 ? Out : FIntRect();
}

void URTSGroundMemory::QueueSunLitNew(FSheet& Sheet)
{
	if (!Sheet.SunLit || !Sheet.Relief)
	{
		return;
	}
	// as caixas em texels do relevo: cada fita (os 4 cantos e a bordinha) e cada cicatriz (o raio do carimbo)
	const FVector2D Scale(Sheet.Relief->SizeX / Sheet.SizeCm.X, Sheet.Relief->SizeY / Sheet.SizeCm.Y);
	const double Margin = (ProfileReachNow() + 40.0) * Scale.X + 2.0;
	struct FBoxDrop
	{
		FBox2D Box;
		float DropCm;
	};
	TArray<FBoxDrop> Boxes;
	for (const int32 I : NewQuads)
	{
		const FTrackQuad& Q = Quads[I];
		FBox2D B(ForceInit);
		for (const FVector2f& P : { Q.L0, Q.R0, Q.L1, Q.R1 })
		{
			B += (FVector2D(P) - Sheet.MinCm) * Scale;
		}
		Boxes.Add({ B.ExpandBy(Margin), 2.0f * RangeCmNow() });
	}
	for (const int32 I : NewScars)
	{
		const FScar& S = Scars[I];
		const FVector2D Cc = (FVector2D(S.At) - Sheet.MinCm) * Scale;
		const double R = (S.Kind == 0 ? S.RadiusCm * 2.0 : (S.Kind == 2 ? WreckAtlas.SpanCm : CraterAtlases[S.Size].SpanCm) * 0.71 * S.Scale) * Scale.X + 2.0;
		Boxes.Add({ FBox2D(Cc - FVector2D(R, R), Cc + FVector2D(R, R)), SunLitMaxDropCm });
	}
	// junta as vizinhas (a caixa unida custa até 1,5 × as duas): um tanque andando vira uma caixa por quadro
	for (int32 A = 0; A < Boxes.Num(); ++A)
	{
		for (int32 B = A + 1; B < Boxes.Num(); ++B)
		{
			FBox2D U = Boxes[A].Box;
			U += Boxes[B].Box;
			const double Sum = Boxes[A].Box.GetArea() + Boxes[B].Box.GetArea();
			if (U.GetArea() <= 1.5 * Sum)
			{
				Boxes[A].Box = U;
				Boxes[A].DropCm = FMath::Max(Boxes[A].DropCm, Boxes[B].DropCm);
				Boxes.RemoveAtSwap(B);
				B = A;   // recomeça: a unida pode alcançar outras
			}
		}
	}
	for (const FBoxDrop& B : Boxes)
	{
		const FIntRect R(FMath::FloorToInt(B.Box.Min.X), FMath::FloorToInt(B.Box.Min.Y), FMath::CeilToInt(B.Box.Max.X), FMath::CeilToInt(B.Box.Max.Y));
		QueueSunLit(Sheet, R, B.DropCm, B.DropCm >= SunLitMaxDropCm ? SunLitMaxDropCm : ContextFor(Sheet, R));
	}
}

void URTSGroundMemory::MarkCraterCells(FSheet& Sheet, const FVector2D& AtCm, double RadiusCm) const
{
	const double CellX = Sheet.SizeCm.X / 64.0, CellY = Sheet.SizeCm.Y / 64.0;
	const int32 X0 = FMath::Clamp(int32(FMath::FloorToInt((AtCm.X - RadiusCm - Sheet.MinCm.X) / CellX)), 0, 63);
	const int32 X1 = FMath::Clamp(int32(FMath::FloorToInt((AtCm.X + RadiusCm - Sheet.MinCm.X) / CellX)), 0, 63);
	const int32 Y0 = FMath::Clamp(int32(FMath::FloorToInt((AtCm.Y - RadiusCm - Sheet.MinCm.Y) / CellY)), 0, 63);
	const int32 Y1 = FMath::Clamp(int32(FMath::FloorToInt((AtCm.Y + RadiusCm - Sheet.MinCm.Y) / CellY)), 0, 63);
	for (int32 Y = Y0; Y <= Y1; ++Y)
	{
		for (int32 X = X0; X <= X1; ++X)
		{
			Sheet.CraterRows[Y] |= uint64(1) << X;
		}
	}
}

bool URTSGroundMemory::CraterNear(const FSheet& Sheet, const FIntRect& ReliefRect, double ExtraCm) const
{
	if (!Sheet.Relief)
	{
		return true;
	}
	const double TexX = Sheet.SizeCm.X / Sheet.Relief->SizeX, TexY = Sheet.SizeCm.Y / Sheet.Relief->SizeY;
	const double CellX = Sheet.SizeCm.X / 64.0, CellY = Sheet.SizeCm.Y / 64.0;
	const int32 X0 = FMath::Clamp(int32(FMath::FloorToInt((ReliefRect.Min.X * TexX - ExtraCm) / CellX)), 0, 63);
	const int32 X1 = FMath::Clamp(int32(FMath::FloorToInt((ReliefRect.Max.X * TexX + ExtraCm) / CellX)), 0, 63);
	const int32 Y0 = FMath::Clamp(int32(FMath::FloorToInt((ReliefRect.Min.Y * TexY - ExtraCm) / CellY)), 0, 63);
	const int32 Y1 = FMath::Clamp(int32(FMath::FloorToInt((ReliefRect.Max.Y * TexY + ExtraCm) / CellY)), 0, 63);
	const uint64 Mask = (X1 >= 63 ? ~uint64(0) : (uint64(1) << (X1 + 1)) - 1) & ~((uint64(1) << X0) - 1);
	for (int32 Y = Y0; Y <= Y1; ++Y)
	{
		if (Sheet.CraterRows[Y] & Mask)
		{
			return true;
		}
	}
	return false;
}

float URTSGroundMemory::ContextFor(const FSheet& Sheet, const FIntRect& ReliefRect) const
{
	// a sombra mais longa que pode entrar ali vem de cratera a até MaxDrop ÷ tan do lado do sol; sem cratera perto, a trilha
	// (±RangeCm: a queda de 2 × RangeCm, ~1,4 m de sombra a 8°)
	const double ReachCm = SunLitMaxDropCm / FMath::Max(SunTan, 0.03f);
	return CraterNear(Sheet, ReliefRect, ReachCm) ? SunLitMaxDropCm : 2.0f * RangeCmNow();
}

void URTSGroundMemory::QueueSunLit(FSheet& Sheet, const FIntRect& ReliefRect, float WriteDropCm, float ContextDropCm)
{
	if (!Sheet.SunLit || !Sheet.Relief || ReliefRect.Width() <= 0 || ReliefRect.Height() <= 0)
	{
		return;
	}
	// do texel do relevo pro do caderno de luz, e em ladrilhos de até 512 (a cota por quadro conta ladrilhos)
	const double K = double(Sheet.SunLit->SizeX) / Sheet.Relief->SizeX;
	FIntRect R(FMath::FloorToInt(ReliefRect.Min.X * K), FMath::FloorToInt(ReliefRect.Min.Y * K), FMath::CeilToInt(ReliefRect.Max.X * K), FMath::CeilToInt(ReliefRect.Max.Y * K));
	R.Clip(FIntRect(0, 0, Sheet.SunLit->SizeX, Sheet.SunLit->SizeY));
	for (int32 Y0 = R.Min.Y; Y0 < R.Max.Y; Y0 += 512)
	{
		for (int32 X0 = R.Min.X; X0 < R.Max.X; X0 += 512)
		{
			SunLitQueue.Add({ &Sheet, FIntRect(X0, Y0, FMath::Min(X0 + 512, R.Max.X), FMath::Min(Y0 + 512, R.Max.Y)), WriteDropCm, ContextDropCm });
		}
	}
}

void URTSGroundMemory::RunSunLit(float DeltaSeconds)
{
	// o sol mudou (a receita, outro mapa): refaz os cadernos inteiros (conferido também com a fila vazia)
	if (SunLitSunTan > 0.0f && (FVector2f::DotProduct(SunLitSunDir, SunDir) < 0.9998f || FMath::Abs(SunLitSunTan - SunTan) > 0.002f))
	{
		SunLitQueue.Reset();
		for (FSheet* S : { &Detail, &Mid })
		{
			if (S->Relief)
			{
				QueueSunLit(*S, FIntRect(0, 0, S->Relief->SizeX, S->Relief->SizeY));
			}
		}
	}
	SunLitSunDir = SunDir;
	SunLitSunTan = SunTan;
	// a volta lenta, só com a fila vazia (a marca nova tem a vez): o próximo ladrilho, alternando detalhe e meio
	SunLitRollClock += DeltaSeconds;
	if (SunLitQueue.IsEmpty() && SunLitRollClock >= SunLitRollSeconds)
	{
		SunLitRollClock = 0.0f;
		FSheet& S = (SunLitRollFlip = !SunLitRollFlip) ? Detail : Mid;
		if (S.SunLit && S.Relief && S.bPlaced)
		{
			const int32 TX = FMath::DivideAndRoundUp(int32(S.SunLit->SizeX), 512);
			const int32 TY = FMath::DivideAndRoundUp(int32(S.SunLit->SizeY), 512);
			const int32 T = S.SunLitRollTile++ % (TX * TY);
			const FIntRect Tile((T % TX) * 512, (T / TX) * 512, FMath::Min((T % TX + 1) * 512, int32(S.SunLit->SizeX)), FMath::Min((T / TX + 1) * 512, int32(S.SunLit->SizeY)));
			// a fila é em texels da luz; o QueueSunLit recebe os do relevo
			const double K = double(S.Relief->SizeX) / S.SunLit->SizeX;
			const FIntRect Rr(FMath::FloorToInt(Tile.Min.X * K), FMath::FloorToInt(Tile.Min.Y * K), FMath::CeilToInt(Tile.Max.X * K), FMath::CeilToInt(Tile.Max.Y * K));
			QueueSunLit(S, Rr, 2.0f * RangeCmNow(), ContextFor(S, Rr));
		}
	}
	if (SunLitQueue.IsEmpty())
	{
		return;
	}
	const int32 Jobs = CVarGroundSunLitJobs.GetValueOnGameThread();
	if (Jobs <= 0)
	{
		SunLitQueue.Reset();   // desligada: a fila não cresce
		return;
	}
	const int32 N = FMath::Min(Jobs, SunLitQueue.Num());
	for (int32 I = 0; I < N; ++I)
	{
		const FSunLitJob Job = SunLitQueue[I];
		FSheet& S = *Job.Sheet;
		FRTSSunLitParams P;
		P.RangeCm = S.Relief_.RangeCm;
		P.SunDir = SunDir;
		P.SunTan = SunTan;
		P.PenumbraCm = S.Relief_.PenumbraCm;
		P.MaxLit = SunLitMaxLit;
		P.Supersample = S.SunLitSS;
		P.SheetSizeCm = float(S.SizeCm.X);
		P.MaxDropCm = Job.ContextDropCm;
		P.WriteDropCm = Job.WriteDropCm;
		P.AORadiusCm = SunLitAORadiusCm;
		P.ClawCm = S.Along ? FMath::Max(CVarGroundClawCm.GetValueOnGameThread(), 0.0f) : 0.0f;
		P.ClawPitchCm = ClawPitchCm;
		P.AlongRangeCm = AlongRangeM * 100.0f;
		P.BandScale = S.Relief_.BandScale;
		P.SheetMinCm = FVector2f(S.MinCm);
		if (SurfaceTex && CVarGroundSurface.GetValueOnGameThread() != 0)
		{
			P.Surface = SurfaceTex;
			P.SurfaceMinCm = SurfaceMinCm;
			P.SurfaceSizeCm = SurfaceSizeCm;
		}
		RTSGroundRelief::ComputeSunLit(S.Relief, S.Crater, S.SunLit, P, Job.Rect, S.Profile, S.Along);
		{
			const int32 Grow = FMath::CeilToInt(Job.WriteDropCm / FMath::Max(SunTan, 0.03f) / float(S.SizeCm.X / S.SunLit->SizeX)) + 2;
			GrowRect(S.SunLitMipDirty, FIntRect(Job.Rect.Min - FIntPoint(Grow, Grow), Job.Rect.Max + FIntPoint(Grow, Grow)));
		}
	}
	SunLitQueue.RemoveAt(0, N);
}

void URTSGroundMemory::FlushMips()
{
	if (!GGroundV3)
	{
		return;
	}
	if (CVarGroundMips.GetValueOnGameThread() == 0)
	{
		for (FSheet* S : { &Detail, &Mid })
		{
			S->ReliefMipDirty.Reset();
			S->SunLitMipDirty.Reset();
		}
		return;
	}
	static bool MipFlip = false;
	MipFlip = !MipFlip;
	for (FSheet* S : { MipFlip ? &Detail : &Mid })
	{
		for (const FIntRect& R : S->ReliefMipDirty)
		{
			RTSGroundRelief::BuildMips(S->Relief, R, GroundMipLevels);
		}
		for (const FIntRect& R : S->SunLitMipDirty)
		{
			RTSGroundRelief::BuildMips(S->SunLit, R, GroundMipLevels);
		}
		S->ReliefMipDirty.Reset();
		S->SunLitMipDirty.Reset();
	}
}

void URTSGroundMemory::UpdateRelief(FSheet& Sheet, const FIntRect& Rect, bool bLight)
{
	if (!Sheet.Relief || !Sheet.Profile || !Sheet.Crater || Rect.Width() <= 0 || Rect.Height() <= 0)
	{
		return;
	}
	FRTSReliefParams P = Sheet.Relief_;
	P.WorldMinCm = FVector2f(Sheet.MinCm);
	P.SunDir = SunDir;
	P.SunTan = SunTan;
	if (SurfaceTex && CVarGroundSurface.GetValueOnGameThread() != 0)
	{
		P.Surface = SurfaceTex;
		P.SurfaceMinCm = SurfaceMinCm;
		P.SurfaceSizeCm = SurfaceSizeCm;
	}
	RTSGroundRelief::Compute(Sheet.Profile, Sheet.Crater, Sheet.Relief, P, Rect);
	if (GGroundV3)
	{
		GrowRect(Sheet.ReliefMipDirty, Rect);
	}
	if (bLight)
	{
		QueueSunLit(Sheet, Rect);
	}
}

void URTSGroundMemory::DrawScars(UCanvas* Canvas, const FSheet& Sheet, const TArray<int32>& Indices) const
{
	if (Indices.IsEmpty() || !BlotStamp || !BlotStamp->GetResource() || !CraterStamp || !CraterStamp->GetResource())
	{
		return;
	}
	const FVector2D Scale(Sheet.Res / Sheet.SizeCm.X, Sheet.Res / Sheet.SizeCm.Y);
	TArray<FCanvasUVTri> ScorchTris;
	TArray<FCanvasUVTri> CraterTris;
	// uma mancha girada: o eixo U ao longo do tiro
	const auto AddBlot = [&](TArray<FCanvasUVTri>& Tris, const FVector2D& Center, const FVector2D& Dir, double HalfAlong, double HalfAcross, const FLinearColor& Col)
	{
		const FVector2D Side(-Dir.Y, Dir.X);
		const FVector2D P00 = (Center - Dir * HalfAlong - Side * HalfAcross - Sheet.MinCm) * Scale;
		const FVector2D P10 = (Center + Dir * HalfAlong - Side * HalfAcross - Sheet.MinCm) * Scale;
		const FVector2D P11 = (Center + Dir * HalfAlong + Side * HalfAcross - Sheet.MinCm) * Scale;
		const FVector2D P01 = (Center - Dir * HalfAlong + Side * HalfAcross - Sheet.MinCm) * Scale;
		FCanvasUVTri A;
		A.V0_Pos = P00; A.V0_UV = FVector2D(0, 0); A.V0_Color = Col;
		A.V1_Pos = P10; A.V1_UV = FVector2D(1, 0); A.V1_Color = Col;
		A.V2_Pos = P11; A.V2_UV = FVector2D(1, 1); A.V2_Color = Col;
		FCanvasUVTri B;
		B.V0_Pos = P00; B.V0_UV = FVector2D(0, 0); B.V0_Color = Col;
		B.V1_Pos = P11; B.V1_UV = FVector2D(1, 1); B.V1_Color = Col;
		B.V2_Pos = P01; B.V2_UV = FVector2D(0, 1); B.V2_Color = Col;
		Tris.Add(A);
		Tris.Add(B);
	};
	for (const int32 I : Indices)
	{
		const FScar& S = Scars[I];
		if (S.Kind != 0 || !Touches(Sheet, S.At, S.RadiusCm * 2.0f))
		{
			continue;   // a cratera e o avental vão pelo compute do atlas (StampMarks)
		}
		const FVector2D At(S.At);
		const FVector2D Dir(S.Dir);
		// o queimado: esticado e empurrado pra frente do tiro (o leque de terra e fogo vai pra longe
		// do atirador); a cratera: no ponto, um pouco alongada
		if (!BlotAtlas.Texture)   // (com o atlas da mancha, ela vai pelo carimbo: StampMarks)
		{
			AddBlot(ScorchTris, At + Dir * (S.Stretch > 1.0f ? S.RadiusCm * 0.3 : 0.0), Dir, S.RadiusCm * S.Stretch, S.RadiusCm, FLinearColor(0.0f, S.Scorch, 0.0f, 1.0f));
		}
		if (S.Crater > 0.0f)
		{
			AddBlot(CraterTris, At, Dir, S.RadiusCm * 0.45 * (S.Stretch > 1.0f ? 1.25 : 1.0), S.RadiusCm * 0.45, FLinearColor(0.0f, 0.0f, S.Crater, 1.0f));
		}
	}
	if (!ScorchTris.IsEmpty())
	{
		FCanvasTriangleItem Item(ScorchTris, BlotStamp->GetResource());
		Item.BlendMode = SE_BLEND_Additive;
		Canvas->DrawItem(Item);
	}
	if (!CraterTris.IsEmpty())
	{
		FCanvasTriangleItem Item(CraterTris, CraterStamp->GetResource());
		Item.BlendMode = SE_BLEND_Additive;
		Canvas->DrawItem(Item);
	}
}

void URTSGroundMemory::Redraw(FSheet& Sheet)
{
	UWorld* World = WorldRef.Get();
	if (!World || !Sheet.Target)
	{
		return;
	}
	UKismetRenderingLibrary::ClearRenderTarget2D(World, Sheet.Target, FLinearColor::Transparent);
	if (Sheet.Crater)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(World, Sheet.Crater, FLinearColor::Black);
	}
	if (Sheet.SunLit)
	{
		// a janela andou: a luz velha não vale mais; o chão liso até a fila refazer (o redesenho abaixo pede o caderno todo)
		UKismetRenderingLibrary::ClearRenderTarget2D(World, Sheet.SunLit, FLinearColor(1.0f, 1.0f, 0.0f, 0.0f));
		SunLitQueue.RemoveAll([&Sheet](const FSunLitJob& J) { return J.Sheet == &Sheet; });
		FMemory::Memzero(Sheet.CraterRows, sizeof(Sheet.CraterRows));   // o carimbo abaixo marca de novo
	}
	TArray<int32> All;
	All.Reserve(FMath::Max(Quads.Num(), Scars.Num()));
	UCanvas* Canvas = nullptr;
	FVector2D Size;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, Sheet.Target, Canvas, Size, Context);
	if (Canvas)
	{
		for (int32 I = 0; I < Scars.Num(); ++I)
		{
			All.Add(I);
		}
		DrawScars(Canvas, Sheet, All);
		All.Reset();
		for (int32 I = 0; I < Quads.Num(); ++I)
		{
			All.Add(I);
		}
		DrawTracks(Canvas, Sheet, All, true);
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, Context);
	// as marcas do atlas (a composição não depende da ordem: soma e máximo)
	All.Reset();
	for (int32 I = 0; I < Scars.Num(); ++I)
	{
		All.Add(I);
	}
	StampMarks(Sheet, All, true);
	if (Sheet.Profile)
	{
		// o perfil na ordem do tempo (a mais nova cobre): o anel começa na mais velha quando já deu a volta
		UKismetRenderingLibrary::ClearRenderTarget2D(World, Sheet.Profile, FLinearColor::Black);
		All.Reset();
		const bool bWrapped = Quads.Num() >= MaxQuads;
		for (int32 K = 0; K < Quads.Num(); ++K)
		{
			All.Add(bWrapped ? (QuadHead + K) % Quads.Num() : K);
		}
		UCanvas* ProfileCanvas = nullptr;
		FDrawToRenderTargetContext ProfileContext;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, Sheet.Profile, ProfileCanvas, Size, ProfileContext);
		if (ProfileCanvas)
		{
			DrawProfile(ProfileCanvas, Sheet, All, true);
		}
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, ProfileContext);
		if (Sheet.Along)
		{
			UKismetRenderingLibrary::ClearRenderTarget2D(World, Sheet.Along, FLinearColor::Black);
			UCanvas* AlongCanvas = nullptr;
			FDrawToRenderTargetContext AlongContext;
			UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, Sheet.Along, AlongCanvas, Size, AlongContext);
			if (AlongCanvas)
			{
				DrawAlong(AlongCanvas, Sheet, All);
			}
			UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, AlongContext);
		}
		UpdateRelief(Sheet, FIntRect(0, 0, Sheet.Relief->SizeX, Sheet.Relief->SizeY));
		Sheet.RollRow = 0;
	}
}

void URTSGroundMemory::Recenter(FSheet& Sheet, const FVector2D& CenterCm)
{
	// no grid dos pixels do caderno: o desenho não escorrega meio pixel quando o caderno anda
	const double Texel = Sheet.SizeCm.X / Sheet.Res;
	const FVector2D Min = CenterCm - Sheet.SizeCm * 0.5;
	Sheet.MinCm = FVector2D(FMath::GridSnap(Min.X, Texel * 16.0), FMath::GridSnap(Min.Y, Texel * 16.0));
	const double T0 = FPlatformTime::Seconds();
	ReadSun();
	Redraw(Sheet);
	Sheet.bPlaced = true;
	if (&Sheet == &Detail)
	{
		bDetailPlaced = true;
		if (DecalMID)
		{
			DecalMID->SetVectorParameterValue(TEXT("DetailBounds"), FLinearColor(Detail.MinCm.X, Detail.MinCm.Y, Detail.SizeCm.X, Detail.SizeCm.Y));
		}
	}
	PushToLandscape(true);
	PushToSlabs(true);
	PushToTrample(true);
	// (23/10, v4) o redesenho que passa de 8 ms (um soluço num quadro de 30 qps) vai pro log de sempre: o custo das fitas guardadas
	const double RedrawMs = (FPlatformTime::Seconds() - T0) * 1000.0;
	if (RedrawMs > 8.0)
	{
		UE_LOG(LogTemp, Log, TEXT("RTS chão: caderno %s em (%.0f, %.0f) m, %d fitas e %d cicatrizes guardadas, %.2f ms (soluço)"),
			Sheet.Name, CenterCm.X / 100.0, CenterCm.Y / 100.0, Quads.Num(), Scars.Num(), RedrawMs);
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("RTS chão: caderno %s em (%.0f, %.0f) m, %d fitas e %d cicatrizes guardadas, %.2f ms"),
			Sheet.Name, CenterCm.X / 100.0, CenterCm.Y / 100.0, Quads.Num(), Scars.Num(), RedrawMs);
	}
}

void URTSGroundMemory::Update(float DeltaSeconds, const FVector2D& FocusCm, float YawDeg)
{
	UWorld* World = WorldRef.Get();
	if (!CoarseTarget || !World)
	{
		return;
	}
	Clock += DeltaSeconds;
	DecayClock += DeltaSeconds;
	FlushMips();   // o que mudou no quadro de antes
	RunSunLit(DeltaSeconds);
	// os pedaços do Landscape que o streaming carregou depois recebem os cadernos (uma olhada por segundo)
	PushClock += DeltaSeconds;
	if (PushClock >= 1.0f)
	{
		PushClock = 0.0f;
		PushToLandscape(false);
		PushToSlabs(false);
		// o liga-desliga das plantas vale na olhada de cada segundo (bBounds: passa por todas, barato com poucos ISM)
		PushToTrample(true);
	}
	if (bDebugCycle && DecalMID)
	{
		// a depuração em rodízio: cada foto do cenário pega um modo (0 normal, 2 sem inclinar a normal,
		// 3 opacidade forçada, 1 vermelho), trocando a cada 4 s com as fotos no meio
		static const float Modes[4] = { 0.0f, 2.0f, 3.0f, 1.0f };
		const int32 K = FMath::FloorToInt((Clock + 2.0f) / 4.0f) % 4;
		if (K != DebugMode)
		{
			DebugMode = K;
			DecalMID->SetScalarParameterValue(TEXT("DebugView"), Modes[K]);
			UE_LOG(LogTemp, Log, TEXT("RTS chão (depuração): modo %.0f aos %.1f s"), Modes[K], Clock);
		}
	}

	if (bReliefCycle && DecalMID)
	{
		// a depuração do relevo: 4 altura (vermelho sulco, azul bordinha, verde revolvido), 5 idade, 6 sol, 0 normal
		static const float Modes[4] = { 4.0f, 5.0f, 6.0f, 0.0f };
		const int32 K = FMath::FloorToInt((Clock + 2.0f) / 4.0f) % 4;
		if (K != DebugMode)
		{
			DebugMode = K;
			DecalMID->SetScalarParameterValue(TEXT("DebugView"), Modes[K]);
			UE_LOG(LogTemp, Log, TEXT("RTS chão (depuração): modo %.0f aos %.1f s"), Modes[K], Clock);
		}
	}
	const double Yaw = FMath::DegreesToRadians(double(YawDeg));
	const FVector2D Forward(FMath::Cos(Yaw), FMath::Sin(Yaw));
	bool bDetailFresh = false;
	bool bMidFresh = false;
	for (FSheet* Sheet : { &Detail, &Mid })
	{
		const FVector2D Want = FocusCm + Forward * Sheet->AheadCm;
		if (!Sheet->bPlaced || FVector2D::Distance(Want, Sheet->MinCm + Sheet->SizeCm * 0.5) > Sheet->SlackCm)
		{
			// o redesenho já põe tudo, inclusive as marcas deste quadro
			Recenter(*Sheet, Want);
			(Sheet == &Detail ? bDetailFresh : bMidFresh) = true;
		}
	}
	const bool bDecay = DecayClock >= DecaySeconds;
	if (bDecay)
	{
		DecayClock -= DecaySeconds;
	}
	// o relevo envelhece aos poucos: uma faixa de linhas por quadro, o caderno inteiro a cada ~2 s (refazer
	// tudo de uma vez a cada 2 s dava um soluço)
	for (FSheet* Sheet : { &Detail, &Mid })
	{
		if (Sheet->bPlaced && Sheet->Relief)
		{
			const int32 Rows = Sheet->Relief->SizeY;
			const int32 N = FMath::Clamp(FMath::CeilToInt(Rows * DeltaSeconds / DecaySeconds), 1, Rows);
			UpdateRelief(*Sheet, FIntRect(0, Sheet->RollRow, Sheet->Relief->SizeX, FMath::Min(Sheet->RollRow + N, Rows)), false);
			Sheet->RollRow = Sheet->RollRow + N >= Rows ? 0 : Sheet->RollRow + N;
		}
	}
	// (16/10) a batalha no minimapa: a camada refeita no máximo uma vez por segundo, se o acúmulo mudou
	BattleOverlayClock += DeltaSeconds;
	if (bBattleDirty && BattleOverlayClock >= 1.0f && BattleTarget && BattleOverlay)
	{
		BattleOverlayClock = 0.0f;
		bBattleDirty = false;
		RTSGroundRelief::BattleOverlay(BattleTarget, BattleOverlay);
	}
	if (NewQuads.IsEmpty() && NewScars.IsEmpty() && !bDecay)
	{
		return;
	}
	for (FSheet* Sheet : { &Coarse, &Detail, &Mid })
	{
		const bool bJustRedrawn = (Sheet == &Detail && bDetailFresh) || (Sheet == &Mid && bMidFresh);
		if (!Sheet->Target || bJustRedrawn || (Sheet != &Coarse && !Sheet->bPlaced))
		{
			continue;
		}
		const FIntRect Dirty = DirtyRect(*Sheet);
		UCanvas* Canvas = nullptr;
		FVector2D Size;
		FDrawToRenderTargetContext Context;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, Sheet->Target, Canvas, Size, Context);
		if (Canvas)
		{
			if (bDecay)
			{
				// a esteira desbota (o R) e a brasa apaga (o B); o escuro e o claro ficam
				FCanvasTileItem Fade(FVector2D::ZeroVector, GWhiteTexture, Size, FLinearColor(TrackDecay, 1.0f, HeatDecay, 1.0f));
				Fade.BlendMode = SE_BLEND_Modulate;
				Canvas->DrawItem(Fade);
			}
			DrawScars(Canvas, *Sheet, NewScars);
			DrawTracks(Canvas, *Sheet, NewQuads, false);
		}
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, Context);
		if (Sheet == &Coarse && BattleTarget)
		{
			// (16/10) o acúmulo da batalha: a partida toda (só o combustível desbota)
			UCanvas* BattleCanvas = nullptr;
			FVector2D BattleSize;
			FDrawToRenderTargetContext BattleContext;
			UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, BattleTarget, BattleCanvas, BattleSize, BattleContext);
			if (BattleCanvas)
			{
				DrawBattle(BattleCanvas, BattleSize, bDecay);
			}
			UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, BattleContext);
			bBattleDirty = true;
		}
		StampMarks(*Sheet, NewScars, false);
		if (Sheet->Profile)
		{
			UCanvas* ProfileCanvas = nullptr;
			FDrawToRenderTargetContext ProfileContext;
			UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, Sheet->Profile, ProfileCanvas, Size, ProfileContext);
			if (ProfileCanvas)
			{
				if (bDecay)
				{
					// o frescor (B) envelhece no mesmo passo da esteira; a faixa e a bordinha ficam
					FCanvasTileItem Fade(FVector2D::ZeroVector, GWhiteTexture, Size, FLinearColor(1.0f, 1.0f, TrackDecay, 1.0f));
					Fade.BlendMode = SE_BLEND_Modulate;
					ProfileCanvas->DrawItem(Fade);
				}
				DrawProfile(ProfileCanvas, *Sheet, NewQuads, false);
			}
			UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, ProfileContext);
			if (Sheet->Along && !NewQuads.IsEmpty())
			{
				UCanvas* AlongCanvas = nullptr;
				FDrawToRenderTargetContext AlongContext;
				UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, Sheet->Along, AlongCanvas, Size, AlongContext);
				if (AlongCanvas)
				{
					DrawAlong(AlongCanvas, *Sheet, NewQuads);
				}
				UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, AlongContext);
			}
			// a luz assada refaz só as caixas das marcas novas (a união de toda a batalha custava ~1 ms por quadro)
			UpdateRelief(*Sheet, Dirty, false);
			QueueSunLitNew(*Sheet);
		}
	}
	NewQuads.Reset();
	NewScars.Reset();
}

void URTSGroundMemory::DrawBattle(UCanvas* Canvas, const FVector2D& Size, bool bDecay)
{
	const FVector2D Scale(Size.X / Coarse.SizeCm.X, Size.Y / Coarse.SizeCm.Y);
	const auto Px = [this, &Scale](const FVector2D& P) { return (P - Coarse.MinCm) * Scale; };
	if (bDecay)
	{
		// o combustível fresco seca (o brilho some em ~5 min); o resto fica a partida toda (as trilhas do Patton no Mojave
		// seguem visíveis 55 anos depois: Belnap e Warren 2002)
		FCanvasTileItem Fade(FVector2D::ZeroVector, GWhiteTexture, Size, FLinearColor(1.0f, 0.985f, 1.0f, 1.0f));
		Fade.BlendMode = SE_BLEND_Modulate;
		Canvas->DrawItem(Fade);
	}
	TArray<FCanvasUVTri> Tris;
	const auto Flush = [&]()
	{
		if (!Tris.IsEmpty())
		{
			FCanvasTriangleItem Item(Tris, GWhiteTexture);
			Item.BlendMode = SE_BLEND_Additive;
			Canvas->DrawItem(Item);
			Tris.Reset();
		}
	};
	// o tráfego: a faixa de 1,5× a lagarta em volta do meio dela, 1/32 por passada (o caminho batido satura em ~32)
	const FLinearColor Pass(1.0f / 32.0f, 0.0f, 0.0f, 0.0f);
	for (const int32 I : NewQuads)
	{
		const FTrackQuad& Q = Quads[I];
		const FVector2D C0 = (FVector2D(Q.L0) + FVector2D(Q.R0)) * 0.5, C1 = (FVector2D(Q.L1) + FVector2D(Q.R1)) * 0.5;
		const FVector2D N0 = (FVector2D(Q.R0) - FVector2D(Q.L0)) * 0.75, N1 = (FVector2D(Q.R1) - FVector2D(Q.L1)) * 0.75;
		const FVector2D A = Px(C0 - N0), B = Px(C0 + N0), C = Px(C1 + N1), D = Px(C1 - N1);
		FCanvasUVTri T0;
		T0.V0_Pos = A; T0.V0_Color = Pass; T0.V1_Pos = B; T0.V1_Color = Pass; T0.V2_Pos = C; T0.V2_Color = Pass;
		FCanvasUVTri T1;
		T1.V0_Pos = A; T1.V0_Color = Pass; T1.V1_Pos = C; T1.V1_Color = Pass; T1.V2_Pos = D; T1.V2_Color = Pass;
		Tris.Add(T0);
		Tris.Add(T1);
		if (Tris.Num() >= TrisPerItem)
		{
			Flush();
		}
	}
	// o disco macio (o meio cheio, a borda zero), somado
	const auto Disc = [&](const FVector2D& AtCm, double RadiusCm, const FLinearColor& Col)
	{
		const FVector2D C = Px(AtCm);
		const double R = RadiusCm * Scale.X;
		if (R < 0.5)
		{
			return;
		}
		constexpr int32 Segments = 12;
		for (int32 K = 0; K < Segments; ++K)
		{
			const double A0 = 2.0 * PI * K / Segments, A1 = 2.0 * PI * (K + 1) / Segments;
			FCanvasUVTri T;
			T.V0_Pos = C; T.V0_Color = Col;
			T.V1_Pos = C + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R; T.V1_Color = FLinearColor::Transparent;
			T.V2_Pos = C + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R; T.V2_Color = FLinearColor::Transparent;
			Tris.Add(T);
		}
	};
	for (const int32 I : NewScars)
	{
		const FScar& S = Scars[I];
		const FVector2D At(S.At);
		if (S.Kind == 0)
		{
			Disc(At, S.RadiusCm * 1.5, FLinearColor(0.0f, 0.0f, 0.6f * S.Scorch, 0.0f));   // o queimado largo
		}
		else if (S.Kind == 1)
		{
			const double Rc = CraterAtlases[S.Size].SpanCm * 0.35 * S.Scale;
			Disc(At, Rc * 1.5, FLinearColor(0.0f, 0.0f, 0.8f, 0.0f));
			Disc(At, Rc, FLinearColor(0.0f, 0.0f, 0.0f, 1.0f / 6.0f));   // a densidade (o campo revirado com ~3 sobrepostas)
		}
		else
		{
			const double Rw = WreckAtlas.SpanCm * 0.3 * S.Scale;
			Disc(At, Rw, FLinearColor(0.0f, 1.0f, 0.0f, 0.0f));   // o combustível fresco em volta do casco
			Disc(At, Rw * 1.5, FLinearColor(0.0f, 0.0f, 0.7f, 0.0f));
		}
		if (Tris.Num() >= TrisPerItem)
		{
			Flush();
		}
	}
	Flush();
}

void URTSGroundMemory::DebugDump(const FString& Folder)
{
	UWorld* World = WorldRef.Get();
	if (!World || !CoarseTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS chão: nada pra gravar (sem caderno)"));
		return;
	}
	UKismetRenderingLibrary::ExportRenderTarget(World, CoarseTarget, Folder, TEXT("chao_caderno_mapa.png"));
	if (BattleTarget && BattleOverlay)
	{
		// (16/10) o acúmulo da batalha e a camada do minimapa (refeita agora: a última pode ter até 1 s)
		RTSGroundRelief::BattleOverlay(BattleTarget, BattleOverlay);
		UKismetRenderingLibrary::ExportRenderTarget(World, BattleTarget, Folder, TEXT("chao_batalha.png"));
		UKismetRenderingLibrary::ExportRenderTarget(World, BattleOverlay, Folder, TEXT("chao_batalha_minimapa.png"));
	}
	if (bDetailPlaced)
	{
		UKismetRenderingLibrary::ExportRenderTarget(World, DetailRelief, Folder, TEXT("chao_relevo_detalhe.png"));
		UKismetRenderingLibrary::ExportRenderTarget(World, MidRelief, Folder, TEXT("chao_relevo_meio.png"));
		UKismetRenderingLibrary::ExportRenderTarget(World, DetailProfile, Folder, TEXT("chao_perfil_detalhe.png"));
		UKismetRenderingLibrary::ExportRenderTarget(World, DetailCrater, Folder, TEXT("chao_cratera_detalhe.png"));
		UKismetRenderingLibrary::ExportRenderTarget(World, DetailTarget, Folder, TEXT("chao_caderno_detalhe.png"));
		// a luz assada (a v3): o SunLit em linear ÷ 1,25 e o SkyAO, 16 bits (a mesma escala da RippleLit do Look)
		const auto DumpSunLit = [&Folder](UTextureRenderTarget2D* RT, const TCHAR* LitName, const TCHAR* AoName)
		{
			FTextureRenderTargetResource* Res = RT ? RT->GameThread_GetRenderTargetResource() : nullptr;
			if (!Res)
			{
				return;
			}
			const int32 W = RT->SizeX, Hh = RT->SizeY;
			TArray<uint16> Lit, Ao;
			Lit.SetNumZeroed(W * Hh);
			Ao.SetNumZeroed(W * Hh);
			for (int32 Y0 = 0; Y0 < Hh; Y0 += 512)
			{
				// em faixas: a imagem inteira em FLinearColor seriam 256 MB
				TArray<FLinearColor> Px;
				const int32 Rows = FMath::Min(512, Hh - Y0);
				if (!Res->ReadLinearColorPixels(Px, FReadSurfaceDataFlags(RCM_MinMax), FIntRect(0, Y0, W, Y0 + Rows)) || Px.Num() != W * Rows)
				{
					UE_LOG(LogTemp, Warning, TEXT("RTS chão v3: não li a luz assada (%s)"), LitName);
					return;
				}
				for (int32 I = 0; I < Px.Num(); ++I)
				{
					Lit[Y0 * W + I] = uint16(FMath::Clamp(Px[I].R / 1.25f, 0.0f, 1.0f) * 65535.0f + 0.5f);
					Ao[Y0 * W + I] = uint16(FMath::Clamp(Px[I].G, 0.0f, 1.0f) * 65535.0f + 0.5f);
				}
			}
			IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
			for (int32 K = 0; K < 2; ++K)
			{
				TSharedPtr<IImageWrapper> Png = Images.CreateImageWrapper(EImageFormat::PNG);
				const TArray<uint16>& Data = K == 0 ? Lit : Ao;
				if (Png.IsValid() && Png->SetRaw(Data.GetData(), int64(Data.Num()) * 2, W, Hh, ERGBFormat::Gray, 16))
				{
					FFileHelper::SaveArrayToFile(Png->GetCompressed(), *FPaths::Combine(Folder, K == 0 ? LitName : AoName));
				}
			}
		};
		DumpSunLit(DetailSunLit, TEXT("chao_sol_detalhe.png"), TEXT("chao_ceu_detalhe.png"));
		DumpSunLit(MidSunLit, TEXT("chao_sol_meio.png"), TEXT("chao_ceu_meio.png"));
	}
	UE_LOG(LogTemp, Log, TEXT("RTS chão: %d fitas e %d cicatrizes guardadas, %d veículos riscando (lagarta e pneu); detalhe em (%.0f, %.0f) m; gravado em %s"),
		Quads.Num(), Scars.Num(), Tracks.Num(), (Detail.MinCm.X + Detail.SizeCm.X * 0.5) / 100.0, (Detail.MinCm.Y + Detail.SizeCm.Y * 0.5) / 100.0, *Folder);
}

void URTSGroundMemory::Shutdown()
{
	if (AActor* A = DecalActor.Get())
	{
		A->Destroy();
	}
	DecalActor = nullptr;
	Tracks.Reset();
	Quads.Reset();
	Scars.Reset();
	NewQuads.Reset();
	NewScars.Reset();
	CoarseTarget = DetailTarget = MidTarget = nullptr;
	BattleTarget = BattleOverlay = nullptr;
	DetailProfile = DetailRelief = MidProfile = MidRelief = nullptr;
	DetailAlong = nullptr;
	PushedProxies.Reset();
	bLookGround = false;
	CoarseCrater = DetailCrater = MidCrater = nullptr;
	CraterAtlasTex[0] = CraterAtlasTex[1] = CraterAtlasTex[2] = WreckAtlasTex = BlotAtlasTex = nullptr;
	// (17/10) as texturas de sol dos atlas presas no LoadAtlas (AddToRoot): soltas aqui
	for (FRTSGroundMarkAtlas* A : { &CraterAtlases[0], &CraterAtlases[1], &CraterAtlases[2], &WreckAtlas, &BlotAtlas })
	{
		if (A->SunTexture && A->SunTexture->IsRooted())
		{
			A->SunTexture->RemoveFromRoot();
		}
	}
	BlotAtlas = FRTSGroundMarkAtlas();
	bMarkRamps = false;
	for (FRTSGroundMarkAtlas& A : CraterAtlases)
	{
		A = FRTSGroundMarkAtlas();
	}
	WreckAtlas = FRTSGroundMarkAtlas();
	Coarse = FSheet();
	Detail = FSheet();
	Mid = FSheet();
}
