#include "BlastFxDirector.h"

#include "BlastDebris.h"
#include "BlastFxHitLife.h"
#include "BlastRaymarchSubsystem.h"
#include "RTSSceneLights.h"
#include "RTSSunShadow.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "UObject/UObjectIterator.h"
#include "BlastFxGround.h"
#include "HAL/IConsoleManager.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/HeterogeneousVolumeComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "ExplosionBake.h"
#include "ExplosionVolume.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ShaderCompiler.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/Paths.h"

// as mortes no perfil (-csvGpuStats -csvCaptureFrames=N): um evento por morte de tanque, pra achar o quadro
CSV_DEFINE_CATEGORY(BlastFxDirector, true);

namespace
{
	const TCHAR* BakePaths[] = {
		TEXT("/Game/Explosions/DA_HeavyCharge_v0.DA_HeavyCharge_v0"),
		TEXT("/Game/Explosions/DA_HeavyCharge_v1.DA_HeavyCharge_v1"),
		TEXT("/Game/Explosions/DA_HeavyCharge_v2.DA_HeavyCharge_v2"),
	};
	const TCHAR* MaterialPath = TEXT("/Game/Explosions/M_Explosion.M_Explosion");

	// A morte de tanque sem o corpo do EmberGen, na proporção medida contra o
	// vídeo da cena herói (BlastLabSimulation.cpp Detonate, 07/10): bola 0.55,
	// poeira com 40% da altura e metade da vida, a gravação no relógio dela.
	// (Desde 13/10 a morte de tanque é a nova, abaixo; estas ficam pro aquecimento e pra volta, -TankBlast=0.)
	constexpr float TankBallScale = 0.55f;
	// (30/10) o fogo da carcaça espera o jato da munição começar a apagar (o emissor do EmberGen cai de 3,0 a 4,5 s)
	constexpr double JetWreckDelayS = 3.0;

	// A morte de tanque da v2 (o Felipe, 13/10: "ficaremos desse jeito pra explosão de tanques"; as construções mantêm
	// a antiga): a gravação do EmberGen do BlastLab (eg_tq_v2: a bola e a areia que sobe), do tamanho e no ritmo medidos
	// contra os game shots (escala 0,35 do Carro-Bomba, relógio 0,59 = Froude), tão opaca quanto a gravação, com a
	// fuligem cinza-carvão das colunas de tanque dos game shots e a luz do fim de tarde: o sol inteiro, a luz do céu
	// física e a areia que devolve o sol (BlastLab: -RmSwap=eg_cb_v5f_96:eg_tq_v2 -BlastScale=0.35 -BlastTime=0.59
	// -RmSoot=1.0,0.97,0.94,0.30 -RmDust=0.90,0.72,0.50,0.80 -RmFullSun=1 -RmSkyGain=6.28 -RmGround=0.50,0.38,0.26).
	// Sem o fogo da carcaça (fica pra v3).
	constexpr float TankBlastScale = 0.35f, TankBlastTime = 0.59f;
	const FVector4f TankSoot(1.0f, 0.97f, 0.94f, 0.30f), TankDust(0.90f, 0.72f, 0.50f, 0.80f);
	constexpr float TankSunFull = 1.0f, TankSkyGain = 6.28f;
	const FVector4f TankGround(0.50f, 0.38f, 0.26f, 1.0f);

	// A morte v3 (29/10, o Felipe: "a explosão em si é a que mais falta"; o alvo r1_raid_slabs e a cena do Blender, que mediu
	// nele: a nuvem de ~12 × 15 m, a fumaça marrom-cinza de albedo ~0,21, a bola laranja no miolo e a coluna escura que fica).
	// Na v2 a mesma gravação (eg_tq_v2, 3 s) tocava a 0,35 e acabava em 1,8 s: uma nuvem creme que sumia antes de virar coluna.
	// Aqui: a gravação a 0,5 (≈ 12 m) no relógio de Froude (√0,5), a fuligem marrom-cinza mais escura, o fogo mais forte só nela
	// e a nuvem viva: a gravação para no quadro da fumaça cheia (antes da areia assentar, que dava a borda clara em volta do casco)
	// e a nuvem sobe e esmaece por alguns segundos. A/B pela linha de comando: -DeathScale= -DeathEmit= -DeathSoot=r,g,b,albedo
	// -DeathHold= (s da gravação; 0 = sem parar) -DeathLinger= (s) -DeathRise= (cm/s) -DeathWind= (a fração do vento do mapa); -DeathV=2 volta a v2 de 13/10.
	struct FDeathLook
	{
		float Scale = TankBlastScale, Time = TankBlastTime, Emit = 1.0f, HoldS = 0.0f, LingerS = 0.0f, RiseCmS = 0.0f, WindK = 0.0f, Ring = 0.0f, RingThick = 1.0f, Jets = 1.0f; int32 RingV = 1;
		FVector4f RingCol = FVector4f(-1.0f, 0.0f, 0.0f, 0.0f);
		FString Src = TEXT("eg_tq_v2");   // a gravação da bola (-DeathSrc=; a simulação nova entra por aqui no A/B)
		float SrcScale = 0.0f;              // a escala dela quando não é a eg_tq_v2 (-DeathSrcScale=; 0 = a Scale de sempre)
		FVector4f Soot = TankSoot;
	};
	const FDeathLook& DeathLook()
	{
		static const FDeathLook L = []
		{
			FDeathLook O;
			int32 V = 3;
			FParse::Value(FCommandLine::Get(), TEXT("DeathV="), V);
			if (V >= 3)
			{
				O.Scale = 0.5f;
				O.Emit = 1.6f;      // (a folha explo_e: 2,5 deixava a bola branca até 0,3 s; 1,6 dá o amarelo-laranja do alvo aos 0,25 s)
				O.HoldS = 0.9f;
				O.LingerS = 3.5f;   // (5 s e 0,8 m/s: com 3 mortes juntas as nuvens cobriam as unidades, o defeito 7; v11_seq.jpg)
				O.RiseCmS = 250.0f;  // a térmica quente sobe a metros por segundo: em ~1 s a base da nuvem sai da altura das unidades
				// a nuvem vai com o ar a 0,8 do vento na altura dela (a inércia da térmica quente no começo)
				O.WindK = 0.8f;
				// o anel de areia rente ao chão (o alvo: a areia clara correndo até ~15 m, o contraste com a fumaça escura; a gravação
				// não tem a saia, BASE_JOGAVEL "anel de poeira da morte"): o anel simulado do tiro na areia aprovado (sim_chao_anel,
				// ~7 m a 1) a 2,0 (≈ 14 m, o do alvo; a 1,6 quase não lia, explo_j.jpg), no relógio de Froude
				O.Ring = 2.0f;
				// 3× mais denso e na cor da poeira levantada pela explosão (a da cena do Blender, 0,50/0,42/0,32: mais escura e quente
				// que o véu): na cor clara do véu ele virava uma névoa pálida que lavava as unidades (rc_cmp.jpg)
				// (a folha sk_cmp: o anel do tiro, baixo e com o meio limpo, não fazia a base da nuvem do alvo; a saia simétrica do
				// EmberGen, o domo que sobe ~metade da bola, faz) a saia v2, 6× a espessura
				O.RingV = 2;
				O.RingThick = 6.0f;
				// (st_cmp.jpg) melhor ainda: a areia simulada do ataque de mísseis aprovada em 24/10 (a cortina, a onda de base que espalha
				// pros lados como no alvo) a 0,55 (Ring 2,75 ÷ 5): a saia larga e baixa do r1 sem cobrir as unidades em volta
				O.RingV = 4;
				O.Ring = 2.75f;
				// os rastros dos estilhaços como os jatos de fumaça escura do alvo (~1 m de largura, ~1,5 s): 3× a v2 (a 1 eram fios; jets_cmp.jpg)
				O.Jets = 3.0f;
				O.RingCol = FVector4f(0.78f, 0.62f, 0.45f, 0.6f);
				O.Soot = FVector4f(1.0f, 0.80f, 0.62f, 0.14f);   // (a folha explo_d: 0,22 ainda lia creme no sol forte; 0,14 dá o marrom-escuro do alvo)
			}
			FParse::Value(FCommandLine::Get(), TEXT("DeathScale="), O.Scale);
			O.Time = V >= 3 ? FMath::Sqrt(O.Scale) : TankBlastTime;
			FParse::Value(FCommandLine::Get(), TEXT("DeathEmit="), O.Emit);
			FParse::Value(FCommandLine::Get(), TEXT("DeathHold="), O.HoldS);
			FParse::Value(FCommandLine::Get(), TEXT("DeathLinger="), O.LingerS);
			FParse::Value(FCommandLine::Get(), TEXT("DeathRise="), O.RiseCmS);
			FParse::Value(FCommandLine::Get(), TEXT("DeathWind="), O.WindK);
			FParse::Value(FCommandLine::Get(), TEXT("DeathRing="), O.Ring);
			FParse::Value(FCommandLine::Get(), TEXT("DeathRingThick="), O.RingThick);
			FParse::Value(FCommandLine::Get(), TEXT("DeathRingV="), O.RingV);
			FParse::Value(FCommandLine::Get(), TEXT("DeathJets="), O.Jets);
			FParse::Value(FCommandLine::Get(), TEXT("DeathSrc="), O.Src);
			FParse::Value(FCommandLine::Get(), TEXT("DeathSrcScale="), O.SrcScale);
			FString S;
			TArray<FString> P;
			if (FParse::Value(FCommandLine::Get(), TEXT("DeathSoot="), S, false) && S.ParseIntoArray(P, TEXT(",")) == 4)
			{
				O.Soot = FVector4f(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2]), FCString::Atof(*P[3]));
			}
			P.Reset();
			if (FParse::Value(FCommandLine::Get(), TEXT("DeathRingCol="), S, false) && S.ParseIntoArray(P, TEXT(",")) == 4)
			{
				O.RingCol = FVector4f(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2]), FCString::Atof(*P[3]));
			}
			UE_LOG(LogTemp, Display, TEXT("BlastFx morte v%d: escala %.2f, relógio %.2f, fogo %.2f, parada %.2f s, nuvem %.1f s a %.0f cm/s, fuligem %.2f,%.2f,%.2f,%.2f"),
				V, O.Scale, O.Time, O.Emit, O.HoldS, O.LingerS, O.RiseCmS, O.Soot.X, O.Soot.Y, O.Soot.Z, O.Soot.W);
			return O;
		}();
		return L;
	}
	// os destroços da v2 (BlastDebris, 14/10: "rastro de fumaça também, é tudo que a gente homologou"): chapas, cometas
	// com rastro da cor da fuligem e faíscas do alto do casco (~1,8 m), como no vídeo aprovado (-DebrisGlow=0.15: as
	// cabeças e faíscas laranja de dia, não brancas), e o rastro escuro da torre arremessada (BlastLabSimulation.cpp)
	constexpr float DebrisHeightCm = 180.0f, DebrisGlow = 0.15f;
	// O tiro (14/10, a v3; números de partida, a conferir no jogo contra a arte 4.2 e os game shots: docs/TIRO_E_ACERTO.md).
	// A areia é a DustImpact da JangaFX (40 m de domínio): a escala dá o tamanho, o relógio a pressa.
	const TCHAR* DustSourceName = TEXT("jg_dust_impact_96");
	// (17/10, a falha crítica que o Felipe viu: 6 tanques atirando juntos, metade dos disparos nem nascia e os que nasciam
	// sumiam quando os tiros caíam na areia) cada tiro gasta 2–3 (o disparo, a areia do sopro, o tiro na areia): 12
	// (17/10 tarde, de novo com 6 tanques: com 3–4 por tiro, 12 lotava no 4º) 24, com o raymarch em 32 por quadro
	constexpr int32 MaxDustsHigh = 24;               // no Alta/Ultra; Média metade, Baixa 1
	// o brilho das cabeças e faíscas (× os 2·10⁵ cd/m² do M_DebrisGlow). 16/10, no jogo: com 1 tudo estourava em branco
	// (as línguas e a bola laranja do disparo, o traçante laranja); a morte usa 0,15 pras faíscas laranja. O acerto
	// fica mais alto: o miolo do metal é branco-quente na arte 4.2
	// (16/10) na exposição de referência dos destroços (BlastDebris, a compensação): o acerto a 0,8 é o 0,25 sem compensar
	// da fx_acerto_v8, cuja cor dos riscos bateu com a 4.2 (matiz 41°, saturação 0,07)
	// (16/10, depois da correção da conversão sRGB dos brilhos: a cor do miolo ficou igual e a área acesa do leque caiu 30%,
	// srgb_acerto × fx_acerto_v10; 0,8 → 1,1 devolve a área com a cor mais quente)
	// (29/10, a cena do r1 contra os game shots: o leque de faíscas do acerto, o do tiro na areia e o clarão da boca saíam riscos e
	// borrões brancos, a saturação da curva de tom; nos game shots não há leque branco e as faíscas da morte, a 0,15, já saem laranja)
	// o acerto 1,1 → 0,35, o chão 0,5 → 0,2 e a boca 0,3 → 0,2: as faíscas e o clarão em amarelo-laranja. -ImpactGlow= -GroundGlow=
	// -MuzzleGlow= trocam (os de antes: 1.1, 0.5, 0.3)
	float CmdGlow(const TCHAR* Key, float Def) { float V = Def; FParse::Value(FCommandLine::Get(), Key, V); return V; }
	const float MuzzleGlow = CmdGlow(TEXT("MuzzleGlow="), 0.2f), TracerGlow = 0.3f, ImpactGlow = CmdGlow(TEXT("ImpactGlow="), 0.35f);
	// a bola do disparo no raymarch: a eg_tq_v2 (38 m) a 0,07 (~2,7 m), tocada a 0,18 (~1 s inteira, o fogo nos primeiros
	// ~0,15–0,25 s), 1,6 m à frente da boca; a fumaça do propelente cinza-clara (não a fuligem preta da morte)
	constexpr float MuzzleBallScale = 0.07f, MuzzleBallTime = 0.18f, MuzzleBallThickness = 6.0f;
	constexpr float MuzzleBallAheadCm = 160.0f, MuzzleBallDropCm = 80.0f;
	// (16/10) o fogo nos primeiros 0,6 s da gravação (~0,11 s de verdade: o clarão secundário de um canhão grande dura
	// dezenas de ms, Klingenberg & Heimerl, "Gun Muzzle Blast and Flash", AIAA 1992), e o resto (a fumaça) a 0,35, ~2 s
	// (as gravações têm ~6 s, medido no registro de 16/10: a 0,65 a fumaça ficava 3,5 s);
	// a fumaça cinza-areia (a 0,92 ela saía branca ao sol, a arte é cinza-bege)
	// (A/B no jogo, 16/10: emissão 1 / 0,55 / 2 com a fumaça 0,74 ou 0,60: com menos fogo saía mais creme, com 2 e a fumaça
	// cinza o fogo aparece amarelo-laranja nos primeiros 0,15 s e a fumaça sai cinza-acastanhada; captures/game/fx_disparo_v6*)
	constexpr float MuzzleBallSlowAfterS = 0.6f, MuzzleBallSlowTime = 0.35f, MuzzleBallEmit = 2.0f;
	const FVector4f MuzzleSmokeSoot(0.60f, 0.57f, 0.52f, 0.40f), MuzzleSmokeDust(0.78f, 0.74f, 0.68f, 0.6f);
	// a areia do sopro: o golpe nos primeiros 0,5 s da gravação (0,25 s de verdade) e o resto a 0,35 (~2 s pairando;
	// sumia em ~0,8 s como um disco, e a 0,9 ficava 5,3 s)
	constexpr float MuzzleDustSlowAfterS = 0.5f, MuzzleDustSlowTime = 0.35f;
	// (v7, 17/10) o disparo simulado (BlastLab/Tools/blender_pyro_muzzle.py, sim_boca; o item 8 da fila, contra o vídeo de
	// referência do Felipe): o jato pelo eixo do cano (sem freio de boca), a bola do clarão secundário a ~1–4 m da boca e a
	// fumaça do propelente. A boca fica no meio da gravação, 2,3 m acima do chão dela, e a gravação gira pro rumo do cano.
	// Os dois andamentos da simulação: os 36 primeiros quadros (1,2 s da gravação) em câmera lenta 0,25× (o fogo, ~0,13 s
	// de verdade) e o resto em tempo real
	const TCHAR* MuzzleSimSource = TEXT("sim_boca");
	// (17/10, A/B no jogo, disp_v7b–d: a gravação com a fumaça ×3 e a curva 0,6 na conversão; a ×8 saía preta e opaca)
	// a fumaça do propelente cinza-clara (albedo 0,75: a da pólvora, não a fuligem) e o fogo a 4 (a bola branco-amarela dos
	// primeiros ~0,07 s, como no vídeo de referência); a fumaça depois a 1,6 (some em ~1 s em vez de ~0,5)
	// (17/10, o Felipe: "grande, lento e formato errado"; o vídeo de referência medido quadro a quadro: a bola de ~1,5 m colada
	// na boca por ~0,2 s e um sopro pequeno e ralo que some por volta de 0,5 s, sem nuvem) a gravação a 0,4 (a bola de ~3–4 m
	// vira ~1,5 m; a fumaça fica rala sozinha, a espessura óptica cai com o tamanho), o fogo a 0,1 (~0,12 s) e o resto a 0,2
	// (~0,35 s): tudo some em ~0,5 s. -ShotFxSimScale=, -ShotFxSimFast=, -ShotFxSimSlow= trocam, pro A/B
	// (17/10, rodadas disp_v8a–e contra o vídeo: a 0,1 a bola durava ~0,07 s; com o fogo a 0,4 ela fica ~0,2 s como no vídeo;
	// a fumaça a 0,3 de espessura e albedo 0,85 sai clara e rala, sumindo antes de 1 s; o fogo a 8 pra bola branco-amarela)
	constexpr float MuzzleSimHeightCm = 230.0f, MuzzleSimFastTime = 0.4f, MuzzleSimSlowAfterS = 1.2f, MuzzleSimThickness = 0.3f, MuzzleSimEmit = 8.0f;
	constexpr float MuzzleSimSlowTime = 0.15f, MuzzleSimScale = 0.4f;
	// (17/10 tarde, o Felipe no vídeo de perto: "na hora que ele vai desfazer ele infla do nada"; quadro a quadro, aos 0,6 s a
	// fumaça dobrava em 2 quadros e sumia no seguinte) o 2º andamento a 0,15 atravessava o resto da simulação a 6,7×: a
	// fumaça se espalhando de 2 s de gravação em 0,25 s. Agora a gravação toca num andamento só (0,4) e a fumaça some pela
	// opacidade entre 0,3 e 0,7 s, rala aos poucos (o vídeo de referência: o sopro ralo some por volta de 0,5 s).
	// -ShotFxSimLife= e -ShotFxSimFade= trocam, pro A/B
	constexpr float MuzzleSimLifeS = 0.7f, MuzzleSimFadeFromS = 0.3f;
	const FVector4f MuzzleSimSoot(0.85f, 0.82f, 0.78f, 0.85f);

	float CmdFloat(const TCHAR* Key, float Default)
	{
		float V = Default;
		FParse::Value(FCommandLine::Get(), Key, V);
		return V;
	}

	FVector4f CmdColor(const TCHAR* Key, const FVector4f& Default)
	{
		FString S;
		if (!FParse::Value(FCommandLine::Get(), Key, S, false))
		{
			return Default;
		}
		TArray<FString> P;
		S.ParseIntoArray(P, TEXT(","));
		return P.Num() == 4 ? FVector4f(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2]), FCString::Atof(*P[3])) : Default;
	}
	constexpr float MuzzleDustAheadCm = 150.0f, MuzzleDustScale = 0.15f, MuzzleDustTime = 0.5f, MuzzleDustOpacity = 0.6f;
	constexpr float HitHeightMinCm = 150.0f, HitHeightMaxCm = 230.0f;   // do alto do casco à torre (a arte acerta a bochecha dela)
	// 16/10, no jogo contra a arte 4.2: com 0,18/0,7 a areia em volta quase não aparecia e a baforada (0,07/0,45) era um fio
	// (16/10, lado a lado com a 4.2: a fumaça cinza da arte sobe atrás do leque e a areia cobre a base; as nossas mal
	// apareciam) a baforada maior, mais rápida e cinza-escura; a areia um pouco maior
	constexpr float HitDustScale = 0.28f, HitDustTime = 0.6f, HitDustOpacity = 0.9f;
	// (29/10, "falta vida no impacto dos tiros") a baforada 0,14 → 0,2 (no relógio de Froude, √(0,2/0,14) × 0,6) e um tom mais escura
	constexpr float HitPuffScale = 0.2f, HitPuffTime = 0.72f, HitPuffOpacity = 0.85f;
	const FVector4f HitPuffColor(0.38f, 0.36f, 0.33f, 0.6f);         // cinza: a fumaça do metal e da tinta, não a areia
	constexpr float GroundDustScale = 0.25f, GroundDustTime = 0.6f, GroundDustOpacity = 0.85f;
	// (16/10, contra a 4.1: o anel de areia da onda de choque; a saia simétrica do EmberGen a 0,6 (raio final ~8,5 m), o
	// golpe (3,6 s da gravação) em 0,3 s e o resto a 0,6 (~1,4 s), rala no alto (espessura 0,7)
	// (v6, contra a 4.1 lado a lado: na arte o anel é o herói, marcado, ~10 m de raio; a 0,6 com espessura 0,7 ele era
	// uma névoa rala) a 0,8 (raio final ~11 m) e denso
	constexpr float GroundRingScale = 0.8f, GroundRingTime = 0.083f, GroundRingOpacity = 1.0f, GroundRingThickness = 1.4f;
	constexpr float GroundRingSlowAfterS = 3.6f, GroundRingSlowTime = 0.6f;
	// (v7) a gravação simulada do anel: metros reais (escala 1), tempo real (1), a densidade de areia da simulação
	const TCHAR* GroundSimSource = TEXT("sim_chao_anel");
	// (v7b: os gomos do anel saíam opacos e volumosos contra a 4.1, mais baixa e translúcida)
	constexpr float GroundSimScale = 1.0f, GroundSimTime = 1.0f, GroundSimOpacity = 0.75f, GroundSimThickness = 0.8f;
	// (v8, 16/10 noite) a cortina de ejeção: a terra fina que a cratera joga num cone invertido com raios, simulada como
	// grãos (BlastLab/Tools/ejecta_curtain.py: Housen e Holsapple, o arrasto de Schiller e Naumann, o gás empoeirado que
	// leva os finos no jato denso); sobe a ~8 m, abre a ~8 m de raio e desce pairando. Escura: a terra revirada da 4.1
	const TCHAR* GroundCurtainSource = TEXT("sim_chao_cortina");
	// (17/10, o Felipe) o tiro na areia a 0,85 do tamanho (o anel, a cortina, e o leque balístico sobe menos)
	constexpr float GroundHitSize = 0.85f;
	constexpr float GroundCurtainOpacity = 0.9f, GroundCurtainThickness = 1.0f;
	const FVector4f GroundCurtainDirt(0.22f, 0.17f, 0.12f, 0.3f);
	const float GroundImpactGlow = CmdGlow(TEXT("GroundGlow="), 0.2f);
	// (30/10) o foguinho do tiro no chão: a ignição do fogo da carcaça (a escala 1 = o fogo de veículo, chama de ~3 m) a ~0,22, a
	// chama de ~0,5 m (Heskestad: ~30 kW num buraco de 0,3 m). -GroundFireScale=X pro A/B; 0 desliga
	const float GroundFireScale = CmdGlow(TEXT("GroundFireScale="), 0.22f);   // (29/10: 0,5 → 0,2, o leque branco na areia; ver o MuzzleGlow)
	// a Patriot (16/10; docs/ESTUDO_MISSIL_PATRIOT.md): o brilho da chama, a poeira do lançamento (a areia gravada a 0,2,
	// ~8 m, o golpe a 0,5 e o resto a 0,35) e a bola da ogiva (a de antes: a Carga Pesada a 0,07; a v2, de 23/10, mais abaixo)
	constexpr float MissileGlow = 0.3f;
	// o carro esmagado (16/10; docs/ESTUDO_ESMAGAR_E_POSTE.md): a poeira do contato (a areia gravada a 0,07, ~2,8 m), a dos
	// pneus (0,05) e a das janelas (0,06), rasteiras e curtas (teto de ~1,5 m: não cobrir o tanque); o brilho dos cacos
	// (16/10 noite, carro_picape_v4 ampliado 5×: a poeira na cor da areia sobre a areia sumia; o FX_CarCrush do ZH usa 5
	// nuvens CINZA, que é o que lê de cima) o contato cinza-terra (o pó velho do carro e da lataria), maior e mais denso; a
	// das janelas mais escura (o pó e a fuligem de dentro); o pneu na cor do chão
	constexpr float CrushDustScale = 0.1f, CrushDustTime = 0.6f, CrushDustOpacity = 0.9f;
	constexpr float TireDustScale = 0.07f, TireDustTime = 0.5f, TireDustOpacity = 0.7f;
	constexpr float WindowDustScale = 0.07f, WindowDustTime = 0.6f, WindowDustOpacity = 0.7f;
	const FVector4f CrushGray(0.62f, 0.58f, 0.53f, 0.75f), WindowGray(0.48f, 0.46f, 0.43f, 0.6f);
	// (16/10 noite, carro_picape_v2b: a poeira não aparecia) a espessura não acompanha o tamanho: a 0,05–0,09 a gravação ficava
	// 11–20× mais transparente (o jato da v6 usava 1/escala); ~4× devolve a densidade de uma poeira de ~3 m
	constexpr float CrushThick = 4.0f;
	constexpr float GlassGlow = 0.6f;
	// o arco do poste: o brilho (o núcleo estoura no branco-azulado de propósito: é a coisa mais clara da cena) e a fumacinha
	// do metal e do isolante queimando (branco-acinzentada, ~1 m, 2–4 s)
	constexpr float ArcGlow = 1.0f;
	// a marca do tiro no casco: os dados de primitiva 12–35 da peça (0–9 são do jogo), 6 vagas; o reforço a menos de 25 cm
	constexpr int32 HullMarkBase = 12, HullMarkSlots = 6;
	constexpr float HullMarkMergeCm = 25.0f;
	constexpr float ArcSmokeScale = 0.03f, ArcSmokeTime = 0.5f, ArcSmokeOpacity = 0.5f;
	const FVector4f ArcSmokeColor(0.78f, 0.77f, 0.75f, 0.8f);
	// a carcaça queimada: a cinza (fuligem com a tinta queimada, escura e fosca) e as lascas; a picape: lanterna e borracha
	constexpr float AshDustScale = 0.08f, AshDustOpacity = 0.6f;
	const FVector4f CrushAsh(0.40f, 0.38f, 0.36f, 0.45f);
	const FLinearColor CrushRust(0.20f, 0.085f, 0.04f, 1.0f), CrushChar(0.035f, 0.033f, 0.03f, 1.0f);
	const FLinearColor CrushLampRed(0.6f, 0.04f, 0.03f, 1.0f), CrushLampAmber(0.85f, 0.4f, 0.04f, 1.0f), CrushRubber(0.03f, 0.03f, 0.03f, 1.0f);
	// (16/10, patriot_v2: a 0,2 e 0,8 a poeira saía uma bola branca atrás da caixa, a areia ao sol estourada; mais rala,
	// menor e na cor da terra um pouco mais escura)
	constexpr float LaunchDustScale = 0.16f, LaunchDustTime = 0.5f, LaunchDustOpacity = 0.45f;
	const FVector4f LaunchDustColor(0.78f, 0.62f, 0.44f, 0.7f);
	// (a de antes, -MissileBlastV=1) o comentário dizia "~3 m de bola", mas era o domínio × a escala (38 m × 0,07): a bola acesa
	// da gravação tem 5,0 m de raio, então a 0,07 ela tinha 0,35 m e tocava a gravação inteira em 0,42 s (não aparecia; sobravam
	// o clarão e as faíscas, a receita do tiro de canhão)
	constexpr float LegacyWarheadScale = 0.07f;
	const FVector4f LegacyWarheadSoot(0.22f, 0.21f, 0.20f, 0.3f);
	// (23/10) a ogiva v2 (o Felipe: "o míssil só impacta como um tiro normal de tanque [...] tem que ter uma explosão, menor do que a
	// carga pesada, mais fraca"; o estudo: docs/ESTUDO_MISSIL_PATRIOT.md). A Carga Pesada medida nas três gravações (r96_v0..2, na
	// escala 1; scratchpad warhead/measure.txt e measure_v12.txt): a bola quente (T > 0,5) com 5,0 m de raio equivalente de 0,28 a
	// 0,9 s da gravação e ainda ~4,5 m aos 1,36 s (4,74–4,81 m aos 1,17 s nas três, 4,46 m aos 1,36 s na v0); dali ela desaba
	// (4,15–4,34 m aos 1,47 s, 2,9 m aos 1,79 s, some aos ~2,3 s). É isso que sustenta os 1,36 s (CpGlowEndS: o fim da fase A, a
	// troca de relógio Hopkinson-Cranz → Froude, a janela da fusão e o fim do afinamento da fuligem), não a luz: a luz física da
	// gravação (a que a clássica toca, CpLightPeakS/CpLightTauS abaixo) cai a 10% aos 0,84–0,87 s e aos 1,36 s está em ~0,6%; o "a luz
	// a 10% aos 1,36 s" de antes era a curva comprimida do WebGL. Aos 2,0 s a bola quente quase se desfez (1,8 m) e a espessura óptica
	// no centro cai pra ~10 na escala 1 (3,4 na Patriot, 4,9 no Tomahawk: ainda opaca, por isso a fuligem afina antes, SootKeep4Kg):
	// começa o esmaecer; a coluna bate no teto do domínio (38 m) aos ~3,3 s (a vida acaba antes, aos 3,2 s)
	constexpr float CpFireballM = 5.0f, CpGlowEndS = 1.36f, CpThinS = 2.0f, CpValidS = 3.2f;
	// a âncora: a clássica do jogo (a bola a 0,55 = 2,75 m) é a carga de 20 kg (o nominal da Carga Pesada, instances.js MAGNITUDES:
	// 1,00 = 20 kg; e pela LANL, ~1 m de bola por kg^⅓: 2,75³ = 20,8 kg); λ(W) = 0,55·(W/20 kg)^⅓ (UBlastFxDirector::WarheadScale).
	// A gravação crua na escala 1 é uma carga de ~125 kg (o tamanho de vitrine do WebGL)
	// (23/10, a 2ª revisão) o anel de areia: o tiro de canhão na areia aprovado (a sim_chao_anel a GroundHitSize, 0,85; a carga de
	// ~1,6 kg, a média geométrica dos 1–2,5 kg do HE de 120 mm, docs/TIRO_E_ACERTO.md) levado pra carga da ogiva na distância escalada
	// de Hopkinson-Cranz: s = 0,85·(W/1,6 kg)^⅓, 1,15 na Patriot e 1,66 no Tomahawk (WarheadRingScale). É o mesmo fenômeno: o choque de
	// uma carga de alto explosivo no chão levantando a areia (a areia levantada por área na mesma distância escalada é da areia, não da
	// carga: a espessura dá o mesmo τ do anel aprovado em qualquer escala, SpawnWarhead). A borda que se vê da sim_chao_anel (tau > 0,3
	// com a espessura e a opacidade aprovadas, 0,6·tau) chega a 7,0 m na escala 1: 6,0 m no tiro de canhão, 8,1 m na Patriot e 11,6 m
	// no Tomahawk, ~5× a bola (1,61 e 2,32 m; a borda dela vista de cima, 5,2 m × λ: 1,67 e 2,41 m), dentro dos 2,5–6× das artes
	// aprovadas da 4.7 (docs/concept/explosions/type_4.7_precision_strike e type_4.7_ring_reference_gpt). A leitura do alvo com o anel
	// por cima do corpo (scratchpad warhead/ring_readability.py, a mesma conta da SootKeep4Kg: 8 rumos da câmera × 12 do míssil): na
	// câmera do jogo (37,5°) a Patriot fica com a pior 0,53 de 0,5 s em diante (só o corpo: 0,56) e o Tomahawk com 0,57 de 0,85 s em
	// diante (0,63); a 28° a Patriot cai pra 0,40 em 1 das 96 e a 44° o Tomahawk pra 0,46 em 1. O custo é na bola acesa: o Tomahawk
	// aos 0,4 s tem 41 das 96 abaixo de 50% (só o corpo: 20), a média 0,56 (0,75). O A/B: -MissileBlastRing=0 ou
	// blast.MissileBlastRings=0.
	// (Desfeita: a razão anel/bola da morte de tanque, s = 1,22 × 5,2·λ / 7,0. Aquela é a areia de uma bola de combustível em cima do
	// casco, não de uma ogiva no chão: o anel saía 2–3× menor que o do tiro de canhão, a borda a 0,3–0,4 m fora da bola por ~0,1 s, e
	// o volume mais caro por quadro, 3,1 MB, porque o relógio dele encolhe junto.)
	// O R90 da poeira (a massa, o maior, aos 1,2 s da gravação) é 9,9 m na escala 1 (o alcance pra vista e pra vizinhança dos anéis:
	// 11,4 m na Patriot, 16,5 m no Tomahawk). A parte que se vê (tau > 0,3) acaba entre 0,8 e 1,2 s da gravação (tau máx 0,36 e 0,16):
	// o anel some aí, não aos 2,47 s (sobe 76 MB em vez de 103 MB de quadros)
	constexpr float RingRefKg = 1.6f, RingR90M = 9.9f, RingFadeFromS = 0.8f, RingSeenEndS = 1.2f;
	// (23/10, a revisão: a leitura do alvo) a fuligem depois da bola quente. Medido pela câmera do jogo (37,5°; e 28 e 44°) sobre a
	// gravação (scratchpad warhead/readability.py): a silhueta do tanque atingido (7,9 × 3,6 × 2,4 m, o teto e as faces viradas pra
	// câmera) com transmitância ≥ 50%, em 8 rumos da câmera × 12 do míssil. Com a fuligem física (espessura 1) a Patriot deixa 2–6 das
	// 96 combinações com menos da metade legível até 1,0–1,15 s (a pior com 31–33%: o míssil vindo do lado da câmera, a fumaça subindo
	// entre ela e o casco) e o Tomahawk 6–16 até 1,2–1,3 s. A fuligem afina com a bola esfriando (de 0,8 s da gravação, a luz física
	// a 12%, até o fim da fase A, 1,36 s) pra SootKeep4Kg·√(4 kg/W) da física: 0,18 na Patriot (nenhuma das 96 abaixo de 50% a partir
	// de 0,45 s; a pior 51–58%) e 0,10 no Tomahawk (nenhuma a partir de 0,85 s, ~0,5 s × K: a bola acesa dele, 4,6 m, vai até 0,63 s;
	// scratchpad warhead/legibilidade.txt). O √(4 kg/W) não é lei física: é o ajuste que passa pelos dois pontos medidos (0,18 a 4 kg,
	// 0,104 a 12 kg). Uma carga nova fora de 4–12 kg pede medir de novo (readability.py), não confiar na curva.
	// A bola acesa fica com a espessura inteira (opaca). É a "pouca fumaça" da arte 4.7; e as ogivas de hoje são de RDX/HMX (o
	// equilíbrio de oxigênio −21,6%, contra −74% do TNT da equivalência): bem menos fuligem que a Carga Pesada de vitrine
	constexpr float CpCoolS = 0.8f, SootKeep4Kg = 0.18f;
	// a luz da bola (a curva da gravação sem a compressão do WebGL, a que a clássica toca, ExplosionVolume LightCurveK): o pico aos
	// 0,23 s e depois exp(−(Δt/0,35 s)^1,4) (medido 0,82 / 0,48 / 0,12 / 0,01 aos 0,1 / 0,29 / 0,59 / 1,04 s depois do pico: 10% aos
	// 0,84–0,87 s); no relógio do corpo, a luz dos destroços (ABlastDebris::StartMissileHit) segue esta curva quando a bola é da ogiva
	// (o corpo que nasceu, ou o aceso de outra ogiva em que o acerto se fundiu); na morte e na clássica fica só a queda do clarão
	constexpr float CpLightPeakS = 0.23f, CpLightTauS = 0.35f;
	// o centro da bola acesa: a altura média de T > 0,5 no pico da luz (3,2 m na escala 1; 2,0–4,0 m de 0,04 a 0,39 s): o empurrão e o
	// calor no rastro do míssil (RTSMissileSmoke::Blast) ficam nele
	constexpr float CpBallCentreM = 3.2f;
	// o orçamento: os volumes de ogiva vivos no Alta/Ultra (Média metade, Baixa 1, só o corpo), na proporção do teto aprovado dos
	// tiros (24 de 32); e as vagas do raymarch que as ogivas nunca pegam (as mortes vêm primeiro: o raymarch corta pela distância)
	constexpr int32 WarheadCapHigh = 8, DeathReserve = 2;
	// a ogiva que cede a vaga sai esmaecendo neste tempo (o removeOldestParticles do ZH na prioridade mais baixa, WEAPON_EXPLOSION,
	// sem o estalo)
	constexpr float WarheadRetireS = 0.25f;
	// as opções de A/B da ogiva v2, sem recompilar: -MissileBlastScale= (× λ: o tamanho e os relógios dele), -MissileBlastTime= (× os
	// dois relógios; 1,8 imita o andamento da clássica), -MissileBlastFroude=0 (a fase B no relógio de Hopkinson-Cranz também),
	// -MissileBlastRing= (× o anel; 0 = sem anel), -MissileBlastThick= (a espessura do corpo; negativo = 1/λ, a convenção da
	// clássica), -MissileBlastSoot=r,g,b,albedo (a fuligem do corpo; 0.22,0.21,0.20,0.3 = a de antes), -MissileBlastFlash=0 (sem o
	// clarão do raymarch), -MissileBlastLog=1 (cada acerto: a carga, a escala, os relógios, as vagas e a decisão); (23/10, a revisão)
	// -MissileBlastKeep= (a fuligem que fica depois da bola acesa, × a física; negativo = 0,18·√(4 kg/W); 1 = sem afinar),
	// -MissileBlastCap= e -MissileBlastRings= (o teto de volumes de ogiva e de anéis vivos; negativo = o do preset; também nas
	// variáveis blast.MissileBlastCap e blast.MissileBlastRings, ao vivo)
	struct FWarheadKnobs
	{
		float Scale = 1.0f, Time = 1.0f, Ring = 1.0f, Thick = 1.0f, Keep = -1.0f;
		int32 Cap = -1, Rings = -1;
		bool bFroude = true, bFlash = true, bLog = false;
		FVector4f Soot = FVector4f(-1.0f, 0.0f, 0.0f, 0.0f);
	};
	const FWarheadKnobs& WarheadKnobs()
	{
		static const FWarheadKnobs Knobs = []
		{
			FWarheadKnobs O;
			O.Scale = FMath::Max(CmdFloat(TEXT("MissileBlastScale="), 1.0f), 0.0f);
			O.Time = FMath::Max(CmdFloat(TEXT("MissileBlastTime="), 1.0f), 0.05f);
			O.Ring = FMath::Max(CmdFloat(TEXT("MissileBlastRing="), 1.0f), 0.0f);
			O.Thick = CmdFloat(TEXT("MissileBlastThick="), 1.0f);
			O.Keep = CmdFloat(TEXT("MissileBlastKeep="), -1.0f);
			O.Cap = FMath::RoundToInt32(CmdFloat(TEXT("MissileBlastCap="), -1.0f));
			O.Rings = FMath::RoundToInt32(CmdFloat(TEXT("MissileBlastRings="), -1.0f));
			O.bFroude = CmdFloat(TEXT("MissileBlastFroude="), 1.0f) != 0.0f;
			O.bFlash = CmdFloat(TEXT("MissileBlastFlash="), 1.0f) != 0.0f;
			O.bLog = CmdFloat(TEXT("MissileBlastLog="), 0.0f) != 0.0f;
			// a fuligem do corpo: a do visual da munição (a carvão aprovada dos tiros e das mortes de tanque, a TankSoot; ou a global da
			// clássica, "warhead_look": "classic"); passada aqui, vale pras duas (a de antes, cinza-escura, é o A/B)
			O.Soot = CmdColor(TEXT("MissileBlastSoot="), FVector4f(-1.0f, 0.0f, 0.0f, 0.0f));
			return O;
		}();
		return Knobs;
	}
	// a escala da sim_chao_anel pro anel da ogiva de escala λ (0 sem anel): o tiro de canhão na areia (GroundHitSize, a carga de
	// RingRefKg) na distância escalada, s = 0,85·(W/1,6 kg)^⅓ = 0,85·λ/λ(1,6 kg) (o -MissileBlastScale vale pros dois)
	float WarheadRingScale(float Lambda)
	{
		return GroundHitSize * Lambda / UBlastFxDirector::WarheadScale(RingRefKg) * WarheadKnobs().Ring;
	}
	// os dois relógios do corpo de escala λ (Hopkinson-Cranz na bola acesa, Froude na fumaça que sobe) e a idade em que a gravação
	// chega em Raw s (o inverso do AExplosionVolume::BakeTime)
	struct FWarheadClock
	{
		float Ts = 1.0f, SlowAfter = 0.0f, SlowTs = 1.0f;
		float AgeAt(float Raw) const { return SlowAfter > 0.0f && Raw > SlowAfter ? SlowAfter * Ts + (Raw - SlowAfter) * SlowTs : Raw * Ts; }
	};
	FWarheadClock WarheadClockOf(float Lambda)
	{
		FWarheadClock C;
		C.Ts = Lambda * WarheadKnobs().Time;
		C.SlowAfter = WarheadKnobs().bFroude ? CpGlowEndS : 0.0f;
		C.SlowTs = FMath::Sqrt(Lambda) * WarheadKnobs().Time;
		return C;
	}
	// (23/10, a revisão: o custo da subida) cada quadro novo de uma gravação sobe pra placa (BlastRaymarchSubsystem::Tick: a tabela
	// 12³×4 B, o macro 24³ B e os blocos de 4 864 B). Contado a 60 qps, uma explosão sozinha: o corpo da Patriot sobe 50 MB em 1,48 s
	// (34 MB/s, 0,56 MB por quadro do jogo, pico 3,0 MB no 1º quadro; a fase A, a rápida, 16 MB em 0,45 s), o do Tomahawk 50 MB em
	// 1,9 s (26 MB/s, 0,44 MB/quadro); a clássica 99 MB em 6 s (0,28 MB/quadro) e a morte de tanque aprovada 271 MB em 3,5 s (1,28
	// MB/quadro, pico 6,4). O anel é o mais caro por quadro: a sim_chao_anel tem 75 quadros cheios; cortado na parte que se vê
	// (RingSeenEndS) sobe 76 MB em 1,4 s na Patriot (s 1,15: 0,90 MB/quadro, pico 2,7) e 76 MB em 2,0 s no Tomahawk (s 1,66: 0,63
	// MB/quadro); o do tiro de canhão na areia aprovado (0,85, inteiro) sobe 0,82 MB/quadro (scratchpad warhead/ring_hc_cost.py). (O de
	// antes, a 0,29–0,42, tocava rápido: 2,3–3,1 MB/quadro, pico 5,3.) Por isso os tetos: os volumes de ogiva (o preset; 8 no Alta: no
	// pior caso ~4,5 MB por quadro de corpos) e os anéis vivos ao mesmo tempo (1 no Alta/Ultra, nenhum abaixo). blast.MissileBlastCap e
	// blast.MissileBlastRings trocam ao vivo (negativo = o do preset)
	TAutoConsoleVariable<int32> CVarMissileBlastCap(TEXT("blast.MissileBlastCap"), -1,
		TEXT("O teto de volumes de ogiva de míssil vivos, o corpo e o anel (negativo = o do preset: 8 no Alta/Ultra, 4 na Média, 1 na Baixa)"));
	TAutoConsoleVariable<int32> CVarMissileBlastRings(TEXT("blast.MissileBlastRings"), -1,
		TEXT("O teto de anéis de areia de ogiva vivos ao mesmo tempo, o volume que mais sobe quadros (negativo = o do preset: 1 no Alta/Ultra, 0 abaixo)"));
	int32 WarheadCapFor(int32 Eq)
	{
		const int32 Live = CVarMissileBlastCap.GetValueOnGameThread();
		const int32 Cap = Live >= 0 ? Live : WarheadKnobs().Cap;
		return Cap >= 0 ? Cap : (Eq >= 3 ? WarheadCapHigh : (Eq == 2 ? WarheadCapHigh / 2 : 1));
	}
	int32 WarheadRingCapFor(int32 Eq)
	{
		const int32 Live = CVarMissileBlastRings.GetValueOnGameThread();
		const int32 Cap = Live >= 0 ? Live : WarheadKnobs().Rings;
		return Cap >= 0 ? Cap : (Eq >= 3 ? 1 : 0);
	}
	// (23/10) o teste "na tela" do raymarch (BlastRaymarchSubsystem::Tick: a esfera em volta da caixa da gravação contra o cone da
	// diagonal da vista), com o domínio da maior gravação do jogo (40 m, a jg_dust_impact_96): esta conta nunca vê menos que a dele,
	// e as vagas contadas aqui ficam do lado seguro
	constexpr double RaymarchDomainM = 40.0;
	struct FRmView
	{
		bool bHave = false;
		FVector Eye = FVector::ZeroVector;
		FVector Fwd = FVector::ForwardVector;
		double Half = PI;
	};
	FRmView RmViewOf(const UWorld* InWorld)
	{
		FRmView Rv;
		const APlayerController* PC = InWorld ? InWorld->GetFirstPlayerController() : nullptr;
		if (PC && PC->PlayerCameraManager)
		{
			Rv.bHave = true;
			Rv.Eye = PC->PlayerCameraManager->GetCameraLocation();
			Rv.Fwd = PC->PlayerCameraManager->GetCameraRotation().Vector();
			Rv.Half = FMath::Atan(FMath::Tan(FMath::DegreesToRadians(0.5 * PC->PlayerCameraManager->GetFOVAngle())) * 1.15);
		}
		return Rv;
	}
	bool RmSees(const FRmView& Rv, const FVector& FootCm, float Scale)
	{
		if (!Rv.bHave)
		{
			return true;
		}
		const double S = FMath::Max(double(Scale), 0.01);
		const FVector D = FootCm + FVector(0.0, 0.0, 50.0 * RaymarchDomainM * S) - Rv.Eye;
		const double Dist = D.Size(), R = 87.0 * RaymarchDomainM * S;
		return Dist <= R || FMath::Acos(FMath::Clamp(FVector::DotProduct(D / Dist, Rv.Fwd), -1.0, 1.0)) <= Rv.Half + FMath::Asin(FMath::Min(R / Dist, 1.0));
	}
	// (23/10) o raymarch desenha este volume neste quadro: o filtro do BlastRaymarchSubsystem::Tick (vivo, visível, desenhado por ele)
	// e o cone dele
	bool RmDraws(const AExplosionVolume* E, const FRmView& Rv)
	{
		return E && E->Age >= 0.0f && E->Opacity > 0.0f && !E->IsFinished() && UBlastRaymarchSubsystem::Handles(E) && RmSees(Rv, E->GetActorLocation(), E->SizeScale);
	}
	// (23/10, a 2ª revisão) o anel de ogiva que nasceu com opacidade 0 (entra pela gravação no próximo UpdateWarheads): o raymarch
	// ainda não o desenhou, mas a vaga já é dele. O RaymarchOnScreen conta, e o RetireWarheads tira primeiro (sai sem estalo nenhum)
	bool RmPending(const AExplosionVolume* E, const FRmView& Rv, float DurationS)
	{
		return E && E->Age >= 0.0f && E->Opacity <= 0.0f && !E->IsFinished() && E->BakeTime() < DurationS && UBlastRaymarchSubsystem::Handles(E)
			&& RmSees(Rv, E->GetActorLocation(), E->SizeScale);
	}
	// o jato: a jg_ground_96 (30 m de domínio, a coluna sobe até ~29 m em 6,6 s) a 0,28 (~8 m) tocada a 0,15 (~1 s), na
	// cor da terra revirada (albedo 0,45)
	constexpr float GroundJetScale = 0.28f, GroundJetTime = 0.15f, GroundJetOpacity = 0.9f;
	const FVector4f GroundJetDirt(0.26f, 0.20f, 0.14f, 0.35f);   // (v6: mais escura, a terra revirada da 4.1)
	// o tanque ferido (16/10): os limiares do ZH; o deck do motor 2,3 m atrás do centro a 2,0 m, o anel da torre a 2,3 m
	constexpr float DamagedHealth = 0.7f, ReallyDamagedHealth = 0.35f;
	constexpr double FireDeckCm = 182.0;   // (30/10) a altura da chama na gravação do fogo (o 'hull_h' do firesim)
	constexpr float DeckBackCm = 230.0f, DeckUpCm = 200.0f, RingUpCm = 230.0f;
	// a carcaça (16/10): começa 4 s depois da morte (a bola e a areia da morte já se desfizeram: duas fumaças no mesmo
	// quadro é o defeito nº 1) e dura 30 s, o mesmo tempo do miolo quente do material da carcaça (RTSUnitActor)
	constexpr float WreckDelayS = 4.0f, WreckFireS = 30.0f;
	// (17/10) a luz do fogo da carcaça acende 0,8 s depois da morte (a bola gravada apaga em ~1 s; o diesel segue queimando
	// embaixo e pinta a fumaça da morte por baixo, a pergunta da Arte à noite); a fumaça e as chamas seguem aos 4 s.
	// -WreckLightDelay= troca (4 = o de antes)
	constexpr float WreckLightDelayS = 0.8f;
	// (v2, 17/10) a coluna da carcaça em volume (docs/ESTUDO_CARCACA.md; BlastLab/Tools/blender_pyro_wreck.py, gravação
	// sim_carcaca: o casco de 8,1 m queimando no anel da torre e na grade do motor, a chama de ~1,5–2 m pulsando a ~1 Hz e
	// a fumaça preta subindo ~24 m; o loop de 10 s do trecho estável, inclinado 15° na conversão): no pé da carcaça, girada
	// pro vento do mapa, entra em 1,5 s (dentro da fumaça da morte) e sai nos últimos 5 s. A fuligem preta (albedo 0,25: a
	// da fuligem fresca) e o fogo baixo. Por enquanto só com -WreckV=2 (a 1ª coluna saiu um "bastão" escuro e fino)
	const TCHAR* WreckColumnSource = TEXT("sim_carcaca");
	constexpr float WreckColumnInS = 1.5f, WreckColumnOutS = 5.0f, WreckColumnOpacity = 1.0f, WreckColumnThickness = 1.0f, WreckColumnEmit = 0.6f;
	const FVector4f WreckColumnSoot(0.10f, 0.095f, 0.09f, 0.25f);
	// a torre do jogo: o componente que o RTSUnitActor arremessa, na unidade que morreu (o centro dela a menos de 1,5 m
	// do ponto da morte); o nome é o do RTSUnitActor
	USceneComponent* FindTurret(const UWorld* World, const FVector& Ground)
	{
		USceneComponent* Best = nullptr;
		double BestD = 150.0 * 150.0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const double D = FVector::DistSquared2D(It->GetActorLocation(), Ground);
			if (D < BestD)
			{
				for (UActorComponent* C : It->GetComponents())
				{
					if (C->GetFName() == TEXT("TurretPivot"))
					{
						Best = Cast<USceneComponent>(C);
						BestD = D;
						break;
					}
				}
			}
		}
		return Best;
	}
	// a unidade perto de um ponto (o alvo do traçante: o jogo manda o centro dela): o ator com a TurretPivot mais perto
	/** (01/11) O prédio cuja caixa (no plano) tem o ponto (o tiro de tanque mirado nele: a simulação mira o centro). */
	AActor* FindStructureAt(const UWorld* World, const FVector& At)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(TEXT("RTSStructure")))
			{
				FVector C, E;
				It->GetActorBounds(false, C, E);
				if (FMath::Abs(At.X - C.X) <= E.X + 50.0 && FMath::Abs(At.Y - C.Y) <= E.Y + 50.0)
				{
					return *It;
				}
			}
		}
		return nullptr;
	}

	AActor* FindUnitNear(const UWorld* World, const FVector& At)
	{
		AActor* Best = nullptr;
		double BestD = 300.0 * 300.0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const double D = FVector::DistSquared2D(It->GetActorLocation(), At);
			// (30/10) o prédio leva o acerto de prédio (o BuildingHit, chamado pelo jogo), não as faíscas de metal do traçante
			if (D < BestD && !It->ActorHasTag(TEXT("RTSStructure")))
			{
				for (UActorComponent* C : It->GetComponents())
				{
					if (C->GetFName() == TEXT("TurretPivot"))
					{
						Best = *It;
						BestD = D;
						break;
					}
				}
			}
		}
		return Best;
	}
	// a peça do tanque mais perto de um ponto (até 1,5 m) e o ponto mais perto na caixa dela, no espaço da malha
	UStaticMeshComponent* PartNear(AActor* Target, const FVector& PointCm, FVector& OutLocal)
	{
		UStaticMeshComponent* Best = nullptr;
		double BestD = 150.0 * 150.0;
		for (UActorComponent* Comp : Target->GetComponents())
		{
			UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Comp);
			if (!Mesh || !Mesh->IsVisible() || !Mesh->GetStaticMesh() || !Mesh->CastShadow)
			{
				continue;
			}
			const FBox Box = Mesh->GetStaticMesh()->GetBoundingBox();
			if (Box.GetSize().Z < 30.0)
			{
				continue;
			}
			const FVector L = Mesh->GetComponentTransform().InverseTransformPosition(PointCm);
			const FVector Near = Box.GetClosestPointTo(L);
			const double D = FVector::DistSquared(Near, L);
			if (D < BestD)
			{
				BestD = D;
				Best = Mesh;
				OutLocal = Near;
			}
		}
		return Best;
	}
	bool NewTankBlast()
	{
		static const bool bOn = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("TankBlast="), V); return V != 0; }();
		return bOn;
	}
	constexpr float TankSkirtHeight = 0.4f;
	constexpr float TankSkirtLife = 0.5f;
	constexpr float SizeJitter = 0.1f;
	/** As 3 maiores na tela em detalhe total; as 3 seguintes um nível abaixo; o resto dois (BlastLab). */
	constexpr int32 FullDetailBudget = 3;
	constexpr float WarmSeconds = 2.5f;

	float VariantFraction(uint32 V, uint32 Salt)
	{
		uint32 X = (V ^ Salt) * 0x9E3779B9u;
		X ^= X >> 16; X *= 0x7feb352dU; X ^= X >> 15;
		return float(X & 0xFFFF) / 65535.0f;
	}

	// (23/10, a explosão com vida; BlastFxHitLife.h) uma variável por camada, pro A/B camada por camada (o Felipe aprova ou tira cada uma);
	// valem ao vivo. -HitLifeV=1 desliga todas (a de hoje, exata)
	TAutoConsoleVariable<int32> CVarHitLifeDir(TEXT("blast.HitLife.Dir"), 1,
		TEXT("A vida do acerto: a direção ℓ (a bola nasce no ponto e sai torta pela física; o giro, as faíscas, as lascas, a luz e o anel por ℓ). 0 = como hoje"));
	TAutoConsoleVariable<int32> CVarHitLifeGlints(TEXT("blast.HitLife.Glints"), 1,
		TEXT("A vida do acerto: os brilhos dos estilhaços no casco do alvo e dos vizinhos perto (1–2 quadros)"));
	TAutoConsoleVariable<int32> CVarHitLifeSparks(TEXT("blast.HitLife.Sparks"), 1,
		TEXT("A vida do acerto: as faíscas em jatos inclinadas pra frente (50·K², teto 90; a vida sem o ×K)"));
	TAutoConsoleVariable<int32> CVarHitLifeSpecks(TEXT("blast.HitLife.Specks"), 1,
		TEXT("A vida do acerto: os pontinhos dos estilhaços na areia, à frente do míssil (só na explosão acima do chão, fora da planta e da sombra do alvo)"));
	TAutoConsoleVariable<int32> CVarHitLifeVeil(TEXT("blast.HitLife.Veil"), 1,
		TEXT("A vida do acerto: o véu de areia do choque, a onda pequena que empurra a areia do chão pro lado (a faixa densa até o vento de 100 m/s, a franja até 50 m/s, o vazio atrás do casco; na poeira em volume). -VeilV=1 = o de antes"));
	// (23/10, o Felipe: "não vai ter muita areia saindo do casco") a blast.HitLife.Under, a poeira saindo de baixo do casco, saiu junto com a
	// camada: o chão embaixo do casco fica tapado pelo próprio casco
	TAutoConsoleVariable<int32> CVarHitLifeKick(TEXT("blast.HitLife.Kick"), 1,
		TEXT("A vida do acerto: o tranco do alvo pela física, × (W/4 kg)·(60 t/M), até o teto da mola"));
	TAutoConsoleVariable<int32> CVarHitLifeNeighbors(TEXT("blast.HitLife.Neighbors"), 1,
		TEXT("A vida do acerto: os vizinhos do dano em área (o tranco pela razão do dano e a marca na face)"));
	TAutoConsoleVariable<int32> CVarHitLifeCloud(TEXT("blast.HitLife.Cloud"), 1,
		TEXT("A vida do acerto: a nuvem continua viva depois da gravação (parada, sobe e alarga como térmica, dilui, vai com o vento)"));
	TAutoConsoleVariable<int32> CVarHitLifeSmolder(TEXT("blast.HitLife.Smolder"), 1,
		TEXT("A vida do acerto: a fumaça saindo do ponto do acerto (presa ao casco; o rastro do míssil, com sombra e luz)"));
	TAutoConsoleVariable<int32> CVarHitLifeEmbers(TEXT("blast.HitLife.Embers"), 1,
		TEXT("A vida do acerto: os pedaços em brasa (voam, caem, ficam piscando, levantam areia e fumegam)"));
	TAutoConsoleVariable<int32> CVarHitLifeGround(TEXT("blast.HitLife.Ground"), 1,
		TEXT("A vida do acerto: o chão (o chamuscado no pé da bola; o míssil no chão com a terra e os torrões na escala K; a terra escura no 2º)"));
	TAutoConsoleVariable<int32> CVarHitLifeTrailRise(TEXT("blast.HitLife.TrailRise"), 1,
		TEXT("A vida do acerto: a subida que puxa a ponta do rastro dura o que a nuvem viva dura"));
	TAutoConsoleVariable<int32> CVarHitLifeShake(TEXT("blast.HitLife.Shake"), 1,
		TEXT("A vida do acerto: o tremor atrasado pela distância (a partir de 12 kg)"));
	TAutoConsoleVariable<int32> CVarHitLifeStrike(TEXT("blast.HitLife.Strike"), 1,
		TEXT("A vida do acerto: o chão do ataque de mísseis (a cratera maior, os torrões na escala dele, a onda de base)"));
	TAutoConsoleVariable<int32> CVarHitLifeBake(TEXT("blast.HitLife.Bake"), 1,
		TEXT("A vida do acerto: as gravações dedicadas (warhead_lado, warhead_teto, warhead_chao, strike_chao), quando existem"));
	TAutoConsoleVariable<int32> CVarHitLifeLinger(TEXT("blast.HitLife.Linger"), 1,
		TEXT("A vida do acerto: a poeira que fica do ataque de mísseis (rola e deriva no vento depois da gravação strike_chao, na poeira em volume)"));
	// na ordem do BlastFxHitLife::ELayer
	const TAutoConsoleVariable<int32>* const HitLifeVars[] = { &CVarHitLifeDir, &CVarHitLifeGlints, &CVarHitLifeSparks, &CVarHitLifeSpecks,
		&CVarHitLifeVeil, &CVarHitLifeKick, &CVarHitLifeNeighbors, &CVarHitLifeCloud, &CVarHitLifeSmolder, &CVarHitLifeEmbers,
		&CVarHitLifeGround, &CVarHitLifeTrailRise, &CVarHitLifeShake, &CVarHitLifeStrike, &CVarHitLifeBake, &CVarHitLifeLinger };
	static_assert(UE_ARRAY_COUNT(HitLifeVars) == BlastFxHitLife::Num, "uma variável por camada, na ordem do ELayer");

	// (23/10, a vida) as gravações dedicadas (o diretor das explosões encomendou: o casco de lado, o teto, o chão e o chão do ataque) e a carga
	// em que cada uma foi simulada (a gravação toca a (W/W0)^⅓ do tamanho e do relógio, Hopkinson-Cranz). Os warhead_* nascem na carga do
	// Tomahawk (o estudo: o domínio de ~16 m na bola de 12 kg; a Patriot toca a ×0,69). Trocáveis pro A/B: -HitBakeRefKg=; a espessura
	// -HitBakeThick= (1 = a física). (23/10, a areia do acerto) O strike_chao é só a areia do ataque (o Tomahawk de verdade no chão, ~150 kg:
	// a cratera, a cortina em dedos, a onda de base, o véu): não é mais a fonte da ogiva da metade leve, toca à parte em TODO míssil do
	// ataque, na escala 0,7 (-StrikeBakeScale=; a prova de 23/10 noite em MissileHitLife), com a bola clássica ou a ogiva em cima
	const TCHAR* const WarheadBakeSide = TEXT("warhead_lado");
	const TCHAR* const WarheadBakeTop = TEXT("warhead_teto");
	const TCHAR* const WarheadBakeGround = TEXT("warhead_chao");
	const TCHAR* const StrikeBakeGround = TEXT("strike_chao");
	// (23/10 noite, a revisão das gravações) a warhead_lado tem uma mão só: o lobo puxa ~18° pro −Y dela (o lado em que o míssil corria ao
	// longo da face; medido: o centro da fuligem quente em y −0,37 a −0,53 m). O jogo só gira o rumo, então o acerto que corre pro outro
	// lado toca a espelhada (o y virado, soot_keep.py espelho=1). E a warhead_chao_s: só o fogo e a fuligem da warhead_chao (sem a areia
	// em gás nem a cortina de grãos), a ogiva da metade leve do ataque em cima da strike_chao (uma representação só da areia)
	const TCHAR* const WarheadBakeSideMirror = TEXT("warhead_lado_m");
	const TCHAR* const WarheadBakeGroundSoot = TEXT("warhead_chao_s");
	// o casco gravado nas dedicadas (blender_pyro_tank.py: 7,9 × 3,6 m, 1,85 m e a torre a 2,45 m; o acerto de lado no meio do costado, o
	// de teto no meio do teto da torre, o casco 0,4 m à frente dele). A dedicada só toca no alvo desse tamanho (o tanque, Units.json hull_m
	// 7,9 × 3,7) e com o acerto perto do ponto gravado: num casco menor, de frente/de trás ou longe do meio, a fuligem gravada contorna um
	// casco que não está lá (o vão reto de ~2 m da revisão) e fica a Carga Pesada em escala
	constexpr float BakeHullLongM = 7.9f, BakeHullWideM = 3.6f, BakeHullOffTopM = 0.4f;
	constexpr float BakeFitLongTolM = 1.3f, BakeFitWideTolM = 0.8f, BakeFitHitTolM = 1.3f, BakeFitMinHighM = 1.9f;
	constexpr float BakeFitMinHullM = 1.2f, BakeFitMaxHullM = 3.0f;   // a altura do casco e das lagartas, sem a torre (o gravado: 1,85 m)
	// (23/10 noite) a strike_chao entra em 0,06 s (o impacto: a coroa nasce aos 0,04–0,14 s; a entrada de 0,25 s das areias, feita pra
	// bola lisa do 1º quadro do sopro de disparo, comia ~40% dela) e a terra escura dela (o jato central e o miolo dos jatos: a terra úmida
	// de baixo e a fuligem da carga, no canal da fuligem) na cor da cortina de terra do tiro na areia aprovado (v8), não na carvão
	constexpr float StrikeSandFadeInS = 0.06f;

	// (25/10, a poeira que fica; o Felipe: "a areia que sobe na explosão, depois cai e desaparece, e outra coisa que aparece como poeira
	// flutuando: duas coisas distintas mal encaixadas, sem vida") a fração fina da cortina e da onda de base é a nuvem que fica: o MESMO
	// volume da gravação (a mesma forma, a mesma cor, a mesma luz), parado no quadro da parada, segue como nuvem rala (MakeLinger):
	//  - a parada: a strike_chao em 0,75 da gravação (o começo do esmaecer de antes: o aprovado de 24/10 fica exato até ali; dali a areia
	//    grossa já caiu e o que se vê é o fino); o anel do tipo (sim_chao_anel, a neve e a terra) em 0,31 (0,76 s de 2,47: a poeira dele
	//    em ~40% do pico, antes de a dissipação do Mantaflow comer o resto, que no ar de verdade fica);
	//  - a frente da onda de base, que ainda abria na parada (a velocidade radial medida na gravação), para em ~1 s [E];
	//  - o espalhamento turbulento (Taylor 1921): σ(t) = σv·t/√(1 + t/2T_L), σv = 1,9·u*, σw = 1,25·u* (Panofsky e Dutton 1984, a camada
	//    neutra perto do chão), u* = κ·U10/ln(10/z0), T_L = 0,5·z/σw [E, a ordem de grandeza de Hanna 1982]; mais o cisalhamento no rumo do
	//    vento, σx,c = (∂U/∂z)·σz·t/√3 (Smith 1965), com ∂U/∂z = u*/(κz). Sem vento sobra a turbulência fraca do ar parado [E];
	//  - a massa se conserva: a profundidade óptica vista de cima cai com a área (1/(Gx·Gy)); o volume da Unreal só cresce por igual, então
	//    o tamanho vai pelo lado (Gy) e o resto do rumo do vento entra na espessura (o estiramento no rumo do vento pede o shader);
	//  - os finos assentam: Stokes de 25 µm de quartzo, ~0,05 m/s (a areia e a terra); o pó de neve, 0,08 [E]; dDep/dt = −Dep·vs/z;
	//  - deriva no vento do mapa na altura do meio da nuvem (o perfil logarítmico, o mesmo do rastro do míssil), pegando o vento em ~1 s [E];
	//  - acaba quando a profundidade óptica cai a 6% da da parada, ou no fim da vida do preset (Média 10 s, Alta 16, Ultra 20: o tempo de
	//    jogo comprimido), num esmaecer de 1,5 s. O teto de nuvens vivas por preset (3, 6, 8): a mais velha sai esmaecendo
	constexpr float LingerZ0M = 0.01f, LingerKappa = 0.4f, LingerSigVK = 1.9f, LingerSigWK = 1.25f;
	constexpr float LingerCalmSigVMs = 0.15f, LingerCalmSigWMs = 0.08f, LingerTLK = 0.5f;
	constexpr float LingerFrontTauS = 1.0f, LingerAirTauS = 1.0f, LingerSettleSand = 0.05f, LingerSettleSnow = 0.08f;
	constexpr float LingerStrikeHold = 0.75f, LingerRingHold = 0.31f, LingerEndTau = 0.06f, LingerRetireS = 1.5f;
	constexpr float LingerLifeByPreset[4] = { 0.0f, 10.0f, 16.0f, 20.0f };
	constexpr int32 LingerMaxByPreset[4] = { 0, 3, 6, 8 };
	// (25/10, o vento ao vivo) a coluna da carcaça vira pro vento novo com τ = 4 s: a fumaça nova sai no rumo novo e a velha vai sendo
	// trocada (a coluna de ~20 m se renova em ~10 s a ~2 m/s de subida) [E]
	constexpr float WreckColumnTurnTauS = 4.0f;

	// (25/10, o fogo simulado; docs/FOGO.md) a gravação do firesim (BlastLab/Tools/firesim: o anel da torre arrancada queimando em escala
	// real, o vento de 1,5 m/s a 10 m) e como ela toca: a fuligem, o sol, o céu e o chão da morte de tanque aprovada (o mesmo material:
	// um fenômeno só), a emissão da chama e o ruído de detalhe do WebGL quase desligado (a simulação já tem o detalhe de 10 cm). A
	// entrada: 0,4 s (a gravação começa na ignição, por baixo da bola da morte); o giro pro vento com o τ da coluna de 17/10
	const FName FireSource(TEXT("fire_wreck"));
	// (01/11) o fim: 4 s (antes 2) em que a fumaça que sobra se solta e sobe (abaixo, no Tick do fogo)
	constexpr float FireFadeInS = 0.4f, FireYawTauS = 4.0f, FireTailS = 4.0f;
	// (31/10, o Felipe vendo a sucata: "a fumaça do fogo [...] parece que virou um PNG") depois do último quadro do fim simulado, a fumaça
	// que sobra segue viva FireHoldS como a nuvem da ogiva: sobe e alarga como uma térmica (o tamanho × √((t₁ + t)/t₁), Morton, Taylor e
	// Turner), vai com o vento do mapa e rareia (a massa se conserva: ÷ o cubo do crescimento) até zero. Antes ela parava e esmaecia por
	// 2 s no lugar: a mesma forma clareando, o carimbo
	constexpr float FireHoldS = 8.0f;
	TAutoConsoleVariable<int32> CVarFireVersion(TEXT("blast.Fire.Version"), 2,   // (30/10) o simulado é o padrão: o de sprite deixava a carcaça sem fogo de cima
		TEXT("O fogo das carcaças (tanque e aeronave caída): 1 = o de hoje (o fogo de sprite do tanque, o eg_fogo_ar_64 das aeronaves); 2 = o simulado (fire_wreck; docs/FOGO.md)"));
	// (30/10: a 1,0 a chama quase não lia de cima)
	TAutoConsoleVariable<float> CVarFireEmit(TEXT("blast.Fire.Emit"), 1.4f, TEXT("O fogo simulado: a emissão da chama (× a do raymarch)"));
	TAutoConsoleVariable<float> CVarFireNoise(TEXT("blast.Fire.Noise"), 0.15f, TEXT("O fogo simulado: o ruído de detalhe do WebGL (0 = só a simulação)"));
	TAutoConsoleVariable<float> CVarFireThick(TEXT("blast.Fire.Thick"), 1.0f, TEXT("O fogo simulado: a fuligem por metro (× 1/escala)"));
	TAutoConsoleVariable<int32> CVarFireLog(TEXT("blast.Fire.Log"), 0, TEXT("O fogo simulado: o quadro, o rumo e a luz de cada fogo a cada 1 s no registro"));

	/** O que o firesim.py grava no cabeçalho da gravação ("fire": os quadros do início, do laço, a saída do laço e o fim; o anel; a luz
	 *  do fogo estável), lido uma vez do arquivo (o cabeçalho JSON no começo do .brm2). */
	struct FFireMeta
	{
		bool bValid = false;
		float Fps = 10.0f, RingX = -4.0f, LightSteady = 1.0f;
		int32 Intro = 0, Loop = 1, Exit = 0, Outro = 0, Frames = 0;
	};
	const FFireMeta& FireMeta()
	{
		static FFireMeta M;
		static bool bRead = false;
		if (bRead)
		{
			return M;
		}
		bRead = true;
		const FString Path = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("Raymarch"), FireSource.ToString() + TEXT(".brm2"));
		TUniquePtr<IFileHandle> H(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*Path));
		if (!H)
		{
			return M;
		}
		uint8 Head[8];
		if (!H->Read(Head, 8) || FMemory::Memcmp(Head, "BRM2", 4) != 0)
		{
			return M;
		}
		const uint32 Len = uint32(Head[4]) | (uint32(Head[5]) << 8) | (uint32(Head[6]) << 16) | (uint32(Head[7]) << 24);
		if (Len == 0 || Len > (64u << 20))
		{
			return M;
		}
		TArray<uint8> Json;
		Json.SetNumUninitialized(int32(Len) + 1);
		if (!H->Read(Json.GetData(), Len))
		{
			return M;
		}
		Json[Len] = 0;
		const FString S = UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Json.GetData()));
		const int32 At = S.Find(TEXT("\"fire\":{"));
		if (At < 0)
		{
			return M;
		}
		const FString Fire = S.Mid(At, 600);
		auto Num = [&Fire](const TCHAR* Key, float Default)
		{
			const FString K = FString::Printf(TEXT("\"%s\":"), Key);
			const int32 I = Fire.Find(K);
			return I < 0 ? Default : FCString::Atof(*Fire.Mid(I + K.Len(), 24));
		};
		const int32 FramesAt = S.Find(TEXT("\"frames\":"));
		const int32 Frames = FramesAt >= 0 ? FCString::Atoi(*S.Mid(FramesAt + 9, 12)) : 0;
		M.Fps = Num(TEXT("fps"), 10.0f);
		M.Intro = FMath::RoundToInt32(Num(TEXT("intro"), 0.0f));
		M.Loop = FMath::Max(FMath::RoundToInt32(Num(TEXT("loop"), 1.0f)), 1);
		M.Exit = FMath::RoundToInt32(Num(TEXT("exit"), 0.0f));
		M.Outro = FMath::RoundToInt32(Num(TEXT("outro"), 0.0f));
		M.RingX = Num(TEXT("ringX"), -4.0f);
		M.LightSteady = FMath::Max(Num(TEXT("lightSteady"), 1.0f), 1e-4f);
		M.Frames = Frames;
		M.bValid = Frames > 0 && M.Intro + M.Loop + M.Outro == Frames && M.Exit >= 0 && M.Exit < M.Loop && M.Fps > 0.0f;
		UE_LOG(LogTemp, Display, TEXT("BlastFx fogo simulado: %s, %d quadros a %.0f qps (início %d, laço %d com a saída no %d, fim %d), o anel em x %.2f m, %s"),
			*Path, M.Frames, M.Fps, M.Intro, M.Loop, M.Exit, M.Outro, M.RingX, M.bValid ? TEXT("ok") : TEXT("INVÁLIDO"));
		return M;
	}

	/** A normal do terreno num ponto (cm), pelas diferenças de ±1,5 m na grade guardada do chão. */
	FVector GroundNormalAt(const UWorld* World, const FVector& AtCm)
	{
		constexpr double H = 150.0;
		const double Zx0 = BlastFxGround::HeightAtCached(World, AtCm.X - H, AtCm.Y), Zx1 = BlastFxGround::HeightAtCached(World, AtCm.X + H, AtCm.Y);
		const double Zy0 = BlastFxGround::HeightAtCached(World, AtCm.X, AtCm.Y - H), Zy1 = BlastFxGround::HeightAtCached(World, AtCm.X, AtCm.Y + H);
		const FVector Nn(-(Zx1 - Zx0) / (2.0 * H), -(Zy1 - Zy0) / (2.0 * H), 1.0);
		return Nn.GetSafeNormal();
	}
}

int32 BlastFxHitLife::Version()
{
	static const int32 Ver = [] { int32 V = 2; FParse::Value(FCommandLine::Get(), TEXT("HitLifeV="), V); return V; }();
	return Ver;
}

bool BlastFxHitLife::On(ELayer Layer)
{
	return Version() >= 2 && UBlastFxDirector::MissileBlastVer() >= 2 && Layer >= 0 && Layer < Num && HitLifeVars[Layer]->GetValueOnGameThread() != 0;
}

int32 BlastFxHitLife::Preset()
{
	// o jogo usa o sg.EffectsQuality de 1 (baixa) a 4 (ultra), como o RTSMatchSubsystem (a vida do rastro por preset)
	static const IConsoleVariable* EffectsQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.EffectsQuality"));
	return FMath::Clamp((EffectsQuality ? EffectsQuality->GetInt() : 3) - 1, 0, 3);
}

UBlastFxDirector* UBlastFxDirector::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBlastFxDirector>() : nullptr;
}

TStatId UBlastFxDirector::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBlastFxDirector, STATGROUP_Tickables);
}

bool UBlastFxDirector::LoadAssets()
{
	if (Bakes.Num() && VolumeMaterial)
	{
		return true;
	}
	Bakes.Reset();
	for (const TCHAR* Path : BakePaths)
	{
		if (UExplosionBake* B = LoadObject<UExplosionBake>(nullptr, Path))
		{
			Bakes.Add(B);
		}
	}
	VolumeMaterial = LoadObject<UMaterialInterface>(nullptr, MaterialPath);
	if (!Bakes.Num() || !VolumeMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("BlastFx: faltam as explosões gravadas (Game/Tools/game.ps1 worlds traz do BlastLab)"));
		return false;
	}
	return true;
}

AExplosionVolume* UBlastFxDirector::Spawn(const FVector& Ground, UExplosionBake* Bake, float SizeScale, float Seed, bool bLit)
{
	AExplosionVolume* E = GetWorld()->SpawnActorDeferred<AExplosionVolume>(AExplosionVolume::StaticClass(), FTransform(Ground),
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!E)
	{
		return nullptr;
	}
	E->Bake = Bake;
	E->SizeScale = SizeScale;
	E->Seed = Seed;
	E->Age = 0.0f;
	E->bSelfAdvance = true;
	E->bLights = bLit;
	E->bFlash = bLit;
	E->Volume->SetMaterial(0, VolumeMaterial.Get());
	E->FinishSpawning(FTransform(Ground));
	return E;
}

AExplosionVolume* UBlastFxDirector::FireLoop(const FVector& FootCm, float Scale, uint32 Variant)
{
	// (24/10, o escopo B do ar) a gravação do fogo em loop; o relógio e a chama da noite de 13/10 (o fogo_carcaca_loop_v4: relógio 0,5,
	// a chama a 50%, a espessura 4×), a fuligem cinza da morte de tanque (a regra da cor de 13/10)
	static const FName Source(TEXT("eg_fogo_ar_64"));
	const UBlastRaymarchSubsystem* Rm = GetWorld() ? GetWorld()->GetSubsystem<UBlastRaymarchSubsystem>() : nullptr;
	const float Dur = Rm ? Rm->SourceDuration(Source) : 0.0f;
	if (Dur <= 0.0f || !LoadAssets() || Bakes.Num() == 0)
	{
		return nullptr;
	}
	// (25/10) sem a luz do volume: ela segue a curva da Carga Pesada (a gravação do ator), e o clarão dela voltava a cada volta do
	// loop (a luz piscando). A luz é a da carcaça (UpdateLoopFires), pela potência desta gravação
	AExplosionVolume* E = Spawn(FootCm, Bakes[Variant % uint32(Bakes.Num())], Scale, VariantFraction(Variant, 0xF1AE), false);
	if (!E)
	{
		return nullptr;
	}
	E->RaymarchSource = Source;
	E->bFlash = false;
	E->TimeScale = 0.5f;
	// o ciclo da gravação em loop: um quadro a mais que a duração (o último emenda no primeiro; com a duração o raymarch pulava a emenda)
	const int32 Frames = Rm->SourceFrames(Source);
	E->LoopPeriod = Frames > 1 ? Dur * float(Frames) / float(Frames - 1) : Dur;
	E->LoopLife = 1.0e6f;
	E->Thickness = 4.0f;
	E->RaymarchEmit = 0.5f;
	E->RaymarchSoot = TankSoot;
	E->RaymarchSunFull = TankSunFull;
	E->RaymarchSkyGain = TankSkyGain;
	E->RaymarchGround = TankGround;
	E->Age = Dur * VariantFraction(Variant, 0x10F) / 0.5f;   // a fase do loop sorteada: duas carcaças não pulsam juntas
	if (ABlastDebris* L = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		L->StartWreckFire(1.0e6f, false);   // só a luz (sem as chamas de sprite); quem apaga é o UpdateLoopFires
		L->SetFireDrive(0.0f, FootCm);
		LoopFires.Add({ E, L, Scale });
	}
	return E;
}

void UBlastFxDirector::UpdateLoopFires()
{
	if (LoopFires.IsEmpty() || !GetWorld())
	{
		return;
	}
	static const FName Source(TEXT("eg_fogo_ar_64"));
	const UBlastRaymarchSubsystem* Rm = GetWorld()->GetSubsystem<UBlastRaymarchSubsystem>();
	const int32 Frames = Rm ? Rm->SourceFrames(Source) : 0;
	if (LoopFireMeanPower < 0.0f && Frames > 1)
	{
		// a média do ciclo (a gravação de 13/10 sai normalizada no pico da Carga Pesada, ~48–83: só a forma da curva vale)
		double Sum = 0.0;
		FVector P;
		float Pw = 0.0f;
		for (int32 F = 0; F < Frames; ++F)
		{
			Sum += Rm->SourceLight(Source, float(F), (F + 1) % Frames, P, Pw) ? Pw : 0.0f;
		}
		LoopFireMeanPower = float(Sum / Frames);
	}
	for (int32 K = LoopFires.Num() - 1; K >= 0; --K)
	{
		FLoopFire& F = LoopFires[K];
		AExplosionVolume* E = F.E.Get();
		ABlastDebris* L = F.Light.Get();
		if (!E || E->IsFinished() || !L)
		{
			if (L)
			{
				L->SetFireDrive(0.0f, L->GetActorLocation());
				L->SetLifeSpan(0.5f);
			}
			LoopFires.RemoveAtSwap(K);
			continue;
		}
		// o quadro do raymarch (o ciclo com a emenda: FrameOfTime da gravação em loop) e a potência gravada nele, no centro da queima.
		// A força: a média do ciclo = a luz da carcaça de hoje na escala da carcaça da aeronave (0,17; o tremor médio 0,78 do
		// ABlastDebris::UpdateLight), pela área (Scale², como a luz das explosões e a do fogo simulado), e a entrada/saída de quem chama
		FVector Lp = FVector(0.0, 0.0, 2.5);
		float Pw = LoopFireMeanPower;
		if (Frames > 1 && E->LoopPeriod > 0.0f)
		{
			const float Fr = FMath::Fmod(FMath::Max(E->BakeTime(), 0.0f), E->LoopPeriod) / E->LoopPeriod * float(Frames);
			const int32 F0 = FMath::Clamp(FMath::FloorToInt32(Fr), 0, Frames - 1);
			if (!Rm->SourceLight(Source, Fr, (F0 + 1) % Frames, Lp, Pw))
			{
				Pw = LoopFireMeanPower;
			}
		}
		const float Shape = LoopFireMeanPower > 0.0f ? Pw / LoopFireMeanPower : 1.0f;
		L->SetFireDrive(Shape * 0.78f * FMath::Square(F.Scale / 0.17f) * FMath::Clamp(E->Opacity, 0.0f, 1.0f),
			E->GetActorLocation() + Lp * (double(E->SizeScale) * 100.0));
	}
}

void UBlastFxDirector::StartGroundFire(const FVector& AtCm, const FVector& FromCm, uint32 Variant, float Scale, float LifeS)
{
	constexpr int32 MaxGroundFires = 6;
	GroundFires.RemoveAll([this](const FGroundFire& F) { return F.Fire < 0 || !FireAlive(F.Fire); });
	if (GroundFires.Num() >= MaxGroundFires)
	{
		FireOut(GroundFires[0].Fire, 0.3f);
		GroundFires.RemoveAt(0);
	}
	// no buraco, um pouco à frente no rumo do tiro (os pedaços quentes vão pra lá)
	const FVector Fwd = (AtCm - FromCm).GetSafeNormal2D();
	const FVector At = AtCm + Fwd * (40.0 * VariantFraction(Variant, 0xF0C1));
	// (v11: com o fogo em loop da aeronave encolhido, a fumaça batia no teto da caixa da gravação e saía um bloco cinza de bordas retas) a
	// ignição da gravação do fogo da carcaça (docs/FOGO.md: os primeiros segundos são o fogo pequeno nascendo, com pouca fumaça perto da
	// base) no tamanho de um fogo de resto de carga e mato (Scale da carcaça), e apagado antes da coluna crescer. A chama da gravação
	// nasce FireDeckCm acima do chão dela (o teto do casco simulado): o chão dela fica isso abaixo do buraco
	const int32 Id = StartFire(At - FVector(0.0, 0.0, FireDeckCm * Scale), Scale, Variant ^ 0xF0C0u, LifeS);
	if (Id >= 0)
	{
		GroundFires.Add({ Id, 0.0f, LifeS });
	}
}

void UBlastFxDirector::UpdateGroundFires(float Dt)
{
	for (int32 K = GroundFires.Num() - 1; K >= 0; --K)
	{
		FGroundFire& F = GroundFires[K];
		F.Age += Dt;
		if (F.Fire < 0 || !FireAlive(F.Fire) || F.Age > F.LifeS + 3.0f)
		{
			GroundFires.RemoveAtSwap(K);
		}
	}
}

int32 UBlastFxDirector::BulletSandVersion()
{
	// (30/10, item 29) 2 = a areia da bala nova (sem torrões, a cortina fina, o véu ralo e mais longo); -BulletSandV=1 = a de antes, pro A/B
	static const int32 V = [] { int32 X = 2; FParse::Value(FCommandLine::Get(), TEXT("BulletSandV="), X); return X; }();
	return V;
}

int32 UBlastFxDirector::FireVersion()
{
	return CVarFireVersion.GetValueOnGameThread();
}

int32 UBlastFxDirector::StartFire(const FVector& GroundCm, float Scale, uint32 Variant, float BurnS, ABlastDebris* LightOf)
{
	const FFireMeta& M = FireMeta();
	UWorld* World = GetWorld();
	const UBlastRaymarchSubsystem* Rm = World ? World->GetSubsystem<UBlastRaymarchSubsystem>() : nullptr;
	if (!M.bValid || !Rm || Rm->SourceFrames(FireSource) != M.Frames || !LoadAssets() || Bakes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlastFx fogo simulado: sem a gravação %s (o firesim.py grava; o game.ps1 worlds liga)"), *FireSource.ToString());
		return -1;
	}
	Scale = FMath::Max(Scale, 0.1f);
	AExplosionVolume* E = Spawn(GroundCm, Bakes[Variant % uint32(Bakes.Num())], Scale, VariantFraction(Variant, 0xF12E), false);
	if (!E)
	{
		return -1;
	}
	E->RaymarchSource = FireSource;
	E->bFlash = false;
	E->bLights = false;
	E->TimeScale = 1.0f;
	E->HoldAge = BurnS + 120.0f;                // quem apaga é o UpdateFires (o fim simulado ou o esmaecer)
	E->Thickness = CVarFireThick.GetValueOnGameThread() / Scale;
	E->RaymarchEmit = CVarFireEmit.GetValueOnGameThread();
	E->RaymarchNoise = CVarFireNoise.GetValueOnGameThread();
	// (30/10, o Felipe: o fogo da carcaça "nenhum dos dois presta"; a fumaça do simulado saía branca, vapor) a fuligem do incêndio de
	// veículo (diesel, borracha, plástico) é carbono preto: com o albedo 0,30 da bola da morte e o sol cheio ela saía branca, e mais
	// densa (blast.Fire.Thick 2,5, medido) só ficava mais branca e opaca. Com 0,06 a coluna lê escura ao sol, como no vídeo de
	// referência dele (o carro queimando) e nos game shots
	E->RaymarchSoot = FVector4f(1.0f, 0.95f, 0.9f, 0.06f);
	E->RaymarchSunFull = TankSunFull;
	E->RaymarchSkyGain = TankSkyGain;
	E->RaymarchGround = TankGround;
	E->Opacity = 0.0f;
	E->RaymarchFrame = 0.0f;
	E->RaymarchNextFrame = FMath::Min(1, M.Frames - 1);
	FFire F;
	F.Id = ++FireSerial;
	F.E = E;
	F.GroundCm = GroundCm;
	F.Scale = Scale;
	F.YawRad = WindYawRad;
	F.BurnS = BurnS;
	if (LightOf)
	{
		F.Light = LightOf;
	}
	else if (ABlastDebris* L = World->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		L->StartWreckFire(BurnS + 1.0f, false);
		F.Light = L;
		F.bOwnLight = true;
	}
	// a saída do laço: a última volta em que o fim simulado ainda acaba até BurnS (sem nenhuma, a 1ª e esmaece no fim)
	const int32 K = FMath::FloorToInt32((BurnS * M.Fps - float(M.Intro + M.Exit + M.Outro)) / float(M.Loop));
	F.ExitG = float(FMath::Max(K, 0) * M.Loop + M.Exit);
	if (K < 0)
	{
		F.FadeFrom = FMath::Max(BurnS - 1.5f, 0.2f);
		F.FadeS = 1.5f;
	}
	const float EndS = (float(M.Intro) + F.ExitG + float(M.Outro)) / M.Fps;
	Fires.Add(F);
	UE_LOG(LogTemp, Display, TEXT("BlastFx fogo simulado %d: nasceu em %s, escala %.2f, queima %.1f s (%d voltas do laço; o fim simulado acaba aos %.1f s), vento %.0f graus"),
		F.Id, *GroundCm.ToString(), Scale, BurnS, FMath::Max(K, 0), EndS, FMath::RadiansToDegrees(WindYawRad));
	return F.Id;
}

void UBlastFxDirector::FireOut(int32 Id, float WithinS)
{
	const FFireMeta& M = FireMeta();
	FFire* F = Fires.FindByPredicate([Id](const FFire& X) { return X.Id == Id; });
	if (!F || !M.bValid)
	{
		return;
	}
	WithinS = FMath::Max(WithinS, 0.0f);
	const float G = F->Age * M.Fps - float(M.Intro);
	const bool bInOutro = F->ExitG >= 0.0f && G >= F->ExitG;
	if (!bInOutro)
	{
		// a próxima saída que ainda vem e a última que cabe no prazo
		const int32 KMin = FMath::Max(FMath::CeilToInt32((G - float(M.Exit)) / float(M.Loop)), 0);
		const int32 KMax = FMath::FloorToInt32(((F->Age + WithinS) * M.Fps - float(M.Intro + M.Exit + M.Outro)) / float(M.Loop));
		if (KMax >= KMin)
		{
			F->ExitG = FMath::Min(F->ExitG, float(KMax * M.Loop + M.Exit));
			return;
		}
	}
	else if ((float(M.Frames - 1) - (float(M.Intro + M.Loop) + G - F->ExitG)) / M.Fps <= WithinS)
	{
		return;     // o fim simulado já acaba antes
	}
	// não cabe: esmaece (a carcaça afundando, a aeronave que some)
	if (F->FadeFrom < 0.0f || F->FadeFrom + F->FadeS > F->Age + WithinS)
	{
		F->FadeFrom = F->Age;
		F->FadeS = FMath::Max(WithinS, 0.05f);
	}
}

float UBlastFxDirector::FireEndSoon(int32 Id)
{
	// (31/10, a sucata) o FireOut escolhe a última saída do laço que cabe num prazo (e esmaece o volume inteiro se nenhuma cabe); aqui não há
	// prazo: a carcaça está indo embora e o fogo baixa já, na primeira saída que vem (no máximo uma volta do laço), pelo fim simulado
	const FFireMeta& M = FireMeta();
	FFire* F = Fires.FindByPredicate([Id](const FFire& X) { return X.Id == Id; });
	if (!F || !M.bValid)
	{
		return -1.0f;
	}
	const float G = F->Age * M.Fps - float(M.Intro);
	if (F->ExitG < 0.0f || G < F->ExitG)
	{
		const int32 KMin = FMath::Max(FMath::CeilToInt32((G - float(M.Exit)) / float(M.Loop)), 0);
		const float FirstExit = float(KMin * M.Loop + M.Exit);
		F->ExitG = F->ExitG < 0.0f ? FirstExit : FMath::Min(F->ExitG, FirstExit);
	}
	// o último quadro: no UpdateFires o quadro do fim é Intro + Loop + (G − ExitG) e acaba em Frames − 1
	float LeftS = (float(M.Frames - 1 - M.Loop) + F->ExitG) / M.Fps - F->Age;
	if (F->FadeFrom >= 0.0f)
	{
		LeftS = FMath::Min(LeftS, F->FadeFrom + F->FadeS - F->Age);   // já esmaecendo (um prazo curto de antes)
	}
	return FMath::Max(LeftS, 0.0f);
}

void UBlastFxDirector::FireFollow(int32 Id, const FVector& GroundCm)
{
	if (FFire* F = Fires.FindByPredicate([Id](const FFire& X) { return X.Id == Id; }))
	{
		F->GroundCm = GroundCm;
	}
}

bool UBlastFxDirector::FireAlive(int32 Id) const
{
	return Fires.ContainsByPredicate([Id](const FFire& X) { return X.Id == Id && X.E.IsValid(); });
}

void UBlastFxDirector::UpdateFires(float Dt)
{
	if (Fires.IsEmpty() || !GetWorld())
	{
		return;
	}
	const FFireMeta& M = FireMeta();
	const UBlastRaymarchSubsystem* Rm = GetWorld()->GetSubsystem<UBlastRaymarchSubsystem>();
	const float Sd = SimDelta(Dt);
	const float Turn = 1.0f - FMath::Exp(-Sd / FireYawTauS);
	const bool bLog = CVarFireLog.GetValueOnGameThread() != 0;
	// (31/10) a fumaça que sobra no fim da gravação segue viva depois do último quadro (FireHoldS); -BlastFireHold=0 volta a de antes
	// (esmaece nos últimos FireTailS, parada)
	static const bool bHold = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("BlastFireHold="), V); return V != 0; }();   // (01/11) desligado: é o congelar-e-andar que ele reprovou
	for (int32 K = Fires.Num() - 1; K >= 0; --K)
	{
		FFire& F = Fires[K];
		AExplosionVolume* E = F.E.Get();
		bool bDone = !E || !M.bValid;
		float Frame = 0.0f;
		int32 Next = 0;
		if (!bDone)
		{
			F.Age += Sd;
			const float Fr = F.Age * M.Fps;
			if (Fr < float(M.Intro))
			{
				// o início (a ignição): contínuo até o 1º quadro do laço
				Frame = Fr;
				Next = FMath::FloorToInt32(Fr) + 1;
			}
			else
			{
				const float G = Fr - float(M.Intro);
				if (F.ExitG >= 0.0f && G >= F.ExitG)
				{
					// o fim simulado: sai do laço no ponto puro (o 1º quadro do fim é o mesmo instante da simulação)
					Frame = float(M.Intro + M.Loop) + (G - F.ExitG);
					Next = FMath::FloorToInt32(Frame) + 1;
					if (!bHold)
					{
						bDone = Frame >= float(M.Frames - 1);
					}
					else if (Frame >= float(M.Frames - 1))
					{
						// a fumaça que sobra segue sozinha no último quadro (abaixo); a idade dela é o tempo desde a saída do laço
						if (F.HoldAt < 0.0f)
						{
							F.HoldAt = F.Age;
							F.HoldT1 = FMath::Max((G - F.ExitG) / M.Fps, 2.0f);
						}
						Frame = float(M.Frames - 1);
						Next = M.Frames - 1;
						bDone = F.Age - F.HoldAt >= FireHoldS;
					}
				}
				else
				{
					// o laço: o último quadro emenda no primeiro (os do fim da volta foram misturados com os logo antes do começo)
					const float Gm = FMath::Fmod(G, float(M.Loop));
					Frame = float(M.Intro) + Gm;
					const int32 I0 = FMath::FloorToInt32(Gm);
					Next = I0 + 1 >= M.Loop ? M.Intro : M.Intro + I0 + 1;
				}
			}
		}
		float Fade = 1.0f;
		if (!bDone && F.FadeFrom >= 0.0f)
		{
			Fade = 1.0f - FMath::Clamp((F.Age - F.FadeFrom) / FMath::Max(F.FadeS, 0.01f), 0.0f, 1.0f);
			bDone = Fade <= 0.0f;
		}
		if (bDone)
		{
			if (E)
			{
				E->Destroy();
			}
			if (ABlastDebris* L = F.Light.Get())
			{
				L->SetFireDrive(0.0f, L->GetActorLocation());
				if (F.bOwnLight)
				{
					L->SetLifeSpan(0.5f);
				}
			}
			UE_LOG(LogTemp, Display, TEXT("BlastFx fogo simulado %d: apagou aos %.1f s"), F.Id, F.Age);
			Fires.RemoveAtSwap(K);
			continue;
		}
		E->RaymarchFrame = Frame;
		E->RaymarchNextFrame = FMath::Clamp(Next, 0, M.Frames - 1);
		// a fumaça que ainda sobra no fim da gravação (a de antes: esmaece nos últimos FireTailS, parada; agora segue viva, abaixo)
		const float Tail = bHold ? 1.0f : FMath::Clamp((float(M.Frames - 1) - Frame) / (FireTailS * M.Fps), 0.0f, 1.0f);
		// (01/11, o "quadrado" do cerco, v37/v39_v4_siege 15,8–17,4 s: o fogo da Patriot destruída se apagava esvaziando por dentro e ficava um
		// anel preto com a borda da caixa) o apagar (FireOut) e o fim da gravação iam pela Opacity, que é a Keep do raymarch: ela multiplica a
		// densidade ANTES da erosão da borda (BlastRaymarch.usf, SampleInst), então baixar ela era um limiar subindo e o miolo mais ralo da
		// gravação sumia primeiro. Agora vão pelo esvaecer linear depois da erosão (RaymarchFade): a fumaça rareia por igual. O acender fica
		E->Opacity = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(F.Age / FireFadeInS, 0.0f, 1.0f));
		E->RaymarchFade = FMath::SmoothStep(0.0f, 1.0f, Fade) * FMath::SmoothStep(0.0f, 1.0f, Tail);
		float Grow = 1.0f;
		if (F.HoldAt >= 0.0f)
		{
			const float T = F.Age - F.HoldAt;
			Grow = FMath::Sqrt((F.HoldT1 + T) / F.HoldT1);
			E->SizeScale = F.Scale * Grow;
			E->RaymarchDilute = (1.0f - FMath::SmoothStep(0.0f, FireHoldS, T)) / (Grow * Grow * Grow);
			// o vento no meio da fumaça (~8 m × a escala na gravação da carcaça), mais forte no alto (a mesma lei de todos)
			const float Wind = WindSpeedMs * WindProfileK(FMath::Max(8.0f * F.Scale * Grow, 2.0f));
			F.DriftCm += FVector(FMath::Cos(WindYawRad), FMath::Sin(WindYawRad), 0.0f) * (Wind * Sd * 100.0f);
		}
		// (01/11, a sucata: "parece que na parte da névoa ele virou um PNG"; a prova captures/leitura/prova_fumaca_parada: no fim da gravação a
		// fumaça que sobra ficava quase parada, esmaecendo no lugar) nos últimos FireTailS a fumaça vira a térmica que se solta da carcaça
		// quando o fogo apaga: sobe (~1 m/s, a térmica de um fogo pequeno), alarga (√ do tempo, Morton–Taylor; a massa conservada pela
		// diluição) e o campo de deformação do raymarch anda (HoldWarp), com os quadros da gravação seguindo até o último. Nada fica parado
		float RiseCm = 0.0f;
		if (F.HoldAt < 0.0f && !bHold)
		{
			const float TailT = FMath::Max(0.0f, FireTailS - (float(M.Frames - 1) - Frame) / M.Fps);
			if (TailT > 0.0f && Frame >= float(M.Intro + M.Loop))
			{
				Grow = FMath::Sqrt((2.0f + TailT) / 2.0f);
				E->SizeScale = F.Scale * Grow;
				E->RaymarchDilute = 1.0f / (Grow * Grow * Grow);
				E->RaymarchHoldExtraS = TailT;
				RiseCm = 100.0f * TailT * F.Scale;
			}
		}
		// o rumo: a gravação foi simulada com o vento pra +x; gira pro vento do mapa e acompanha o vento ao vivo
		F.YawRad += FMath::FindDeltaAngleRadians(F.YawRad, WindYawRad) * Turn;
		E->RaymarchYawRad = F.YawRad;
		const FVector Dir(FMath::Cos(F.YawRad), FMath::Sin(F.YawRad), 0.0), Side(-Dir.Y, Dir.X, 0.0);
		// o anel (x = RingX na gravação) no chão sob o meio do casco; (31/10) a fumaça que sobra alarga a partir dele e vai com o vento
		E->SetActorLocation(F.GroundCm - Dir * (double(M.RingX) * F.Scale * Grow * 100.0) + F.DriftCm + FVector(0.0, 0.0, RiseCm));
		// a luz da cena pela potência simulada, no centro da queima
		FVector Lp;
		float Pw = 0.0f;
		if (ABlastDebris* L = F.Light.Get(); L && Rm && Rm->SourceLight(FireSource, Frame, E->RaymarchNextFrame, Lp, Pw))
		{
			const FVector W = E->GetActorLocation() + (Dir * Lp.X + Side * Lp.Y + FVector(0.0, 0.0, Lp.Z)) * (F.Scale * 100.0);
			L->SetFireDrive(Pw / M.LightSteady * E->Opacity * E->RaymarchFade * F.Scale * F.Scale, W);
		}
		if (bLog && FMath::FloorToInt32(F.Age) != FMath::FloorToInt32(F.Age - Sd))
		{
			UE_LOG(LogTemp, Display, TEXT("BlastFx fogo simulado %d: %.1f s, quadro %.1f (seguinte %d), opacidade %.2f, rumo %.0f graus, luz %.2f"),
				F.Id, F.Age, Frame, E->RaymarchNextFrame, E->Opacity, FMath::RadiansToDegrees(F.YawRad), Pw / M.LightSteady);
		}
	}
}

AExplosionVolume* UBlastFxDirector::TankDeath(const FVector& Ground, uint32 Variant, bool bClassic, AActor* WreckOf, float SizeK, int32 DeathKind,
	FName JetBake, const FVector& JetRingCm, float JetScale, float JetBaseCm)
{
	SizeK = FMath::Clamp(SizeK, 0.2f, 1.5f);
	if (DeathKind == 3 && GetWorld())
	{
		// (30/10, docs/ESTUDO_IMPACTOS.md D: o antiaéreo de lagarta, "a assinatura são os estouros de munição em 5–10 s e um míssil que sai
		// torto") a munição que cozinha depois da morte: 4–6 estouros pequenos espalhados até ~8 s (cada um um clarão, faíscas pra cima e a
		// luz) e, entre 2 e 4 s, um míssil que sai torto e risca o céu
		const double Now = GetWorld()->GetTimeSeconds();
		const int32 Pops = 4 + int32(VariantFraction(Variant, 0xC00C) * 2.99f);
		for (int32 K = 0; K < Pops; ++K)
		{
			const uint32 Hk = Variant * 2654435761u + uint32(K) * 0x9E3779B9u;
			FCookOff& C = CookOffs.AddDefaulted_GetRef();
			C.At = Now + 1.5 + 6.5 * double(VariantFraction(Hk, 0x11)) ;
			C.Cm = Ground + FVector(FMath::Lerp(-120.0f, 120.0f, VariantFraction(Hk, 0x22)), FMath::Lerp(-90.0f, 90.0f, VariantFraction(Hk, 0x33)), 150.0f);
			C.Seed = Hk;
		}
		FCookOff& M = CookOffs.AddDefaulted_GetRef();
		M.At = Now + 2.0 + 2.0 * double(VariantFraction(Variant, 0x5157));
		M.Cm = Ground + FVector(0.0, 0.0, 220.0);
		M.Seed = Variant ^ 0x51551Eu;
		M.bMissile = true;
	}
	CSV_EVENT(BlastFxDirector, TEXT("TankDeath"));
	CSV_SCOPED_TIMING_STAT(BlastFxDirector, TankDeath);
	// a carcaça acende pela morte (17/10): se o tiro que matou não passou pelo ArmorHit (a morte por míssil, por ordem da cena,
	// ou o aviso da morte antes do acerto), ela ficava sem fogo
	if (WreckOf && !bClassic)
	{
		const FDamaged* D = Damaged.Find(WreckOf);
		if (!D || D->WreckStart < 0.0)
		{
			EndDamaged(WreckOf);
			StartWreck(WreckOf);
		}
		if (FDamaged* Dw = Damaged.Find(WreckOf))
		{
			Dw->FireScale = SizeK;       // (30/10) o fogo da carcaça na escala da morte
		}
	}
	if (!LoadAssets())
	{
		return nullptr;
	}
	// (23/10) as ogivas dos mísseis cedem pra morte: as que estão em cima dela saem esmaecendo (duas bolas no mesmo lugar é o defeito
	// nº 1), a vaga dela no raymarch fica garantida (ele corta pela distância), e o anel de areia de uma ogiva não nasce em cima
	// dela no próximo 1 s
	if (UWorld* WorldB = GetWorld())
	{
		RecentBigBlasts.Add({ FVector2D(Ground.X, Ground.Y), WorldB->GetTimeSeconds() });
		RetireWarheadsNear(Ground, 400.0f);
		MakeRoomForPriority(Ground, (NewTankBlast() && !bClassic ? DeathLook().Scale : TankBallScale) * SizeK);
	}
	UExplosionBake* Bake = Bakes[Variant % uint32(Bakes.Num())];
	const float Jitter = 1.0f + (VariantFraction(Variant, 0x51ED) - 0.5f) * SizeJitter;
	// bClassic: a explosão do prédio fica a clássica (o Felipe, 13/10: "a nova é só pra tanque"; o jogo chama com true)
	if (NewTankBlast() && !bClassic)
	{
		// a nova: a gravação própria no raymarch (a variante da Carga Pesada só segura o ator e o volume de reserva)
		// O pé 70 cm abaixo do chão (16/10, a moldura clara em volta da carcaça, o defeito mais visível na câmera de jogo): a
		// gravação assenta a areia numa laje rente ao chão da simulação nos quadros finais, e no jogo o chão de verdade fica
		// abaixo dela em volta do casco (o avental da cratera afunda até 50–90 cm; na vila as molduras seguiam o contorno de
		// cada casco). A/B fx_acerto_67_v2 × fx67_drop70: a moldura some e a bola fica igual. -DeathFloorDrop= troca
		static const float FloorDrop = [] { float V = 70.0f; FParse::Value(FCommandLine::Get(), TEXT("DeathFloorDrop="), V); return V; }();
		const FDeathLook& DL = DeathLook();      // (29/10) a morte v3 (o tamanho, o relógio, a fuligem, o fogo e a nuvem viva)
		// (30/10) SizeK: o veículo leve morre menor (a bola pela raiz cúbica da massa que queima) e mais rápido (o relógio × √K, Froude)
		AExplosionVolume* N = Spawn(Ground - FVector(0.0, 0.0, FloorDrop * SizeK), Bake, DL.Scale * Jitter * SizeK, VariantFraction(Variant, 0xA11CE));
		if (!N)
		{
			return nullptr;
		}
		N->RaymarchSource = FName(*DL.Src);
		if (DL.SrcScale > 0.0f)
		{
			// outra gravação (a simulação em metros reais): o tamanho e o relógio dela, a espessura de volta à de escala 1
			N->SizeScale = DL.SrcScale * Jitter * SizeK;
			N->Thickness = 1.0f / (DL.SrcScale * SizeK);
		}
		N->TimeScale = (DL.SrcScale > 0.0f ? FMath::Sqrt(DL.SrcScale * SizeK) : DL.Time * FMath::Sqrt(SizeK)) * Jitter;
		if (DL.SrcScale <= 0.0f)
		{
			N->Thickness = 1.0f / (DL.Scale * SizeK);
		}
		N->RaymarchSoot = DL.Soot;
		N->RaymarchDust = TankDust;
		N->RaymarchEmit = DL.Emit;
		// (30/10, a assinatura de cada morte; docs/ESTUDO_IMPACTOS.md D) a gasolina da picape queima mais clara e amarela (a bola de
		// hidrocarboneto leve, quase sem fuligem no começo); o diesel do Stryker solta a fuligem preta (o albedo pela metade)
		if (DeathKind == 1)
		{
			N->RaymarchEmit = DL.Emit * 1.45f;
		}
		else if (DeathKind == 2)
		{
			N->RaymarchSoot = FVector4f(DL.Soot.X, DL.Soot.Y, DL.Soot.Z, DL.Soot.W * 0.5f);
		}
		if (DL.HoldS > 0.0f)
		{
			// a nuvem viva: a gravação para no quadro da fumaça cheia e o volume vive até o fim da nuvem (o Tick sobe e esmaece)
			N->BakeHoldS = DL.HoldS;
			N->HoldAge = DL.HoldS * N->TimeScale + DL.LingerS * SizeK;
		}
		{
			// (25/10, o impacto pelo chão) fora da areia a saia baixa é o que o chão tem (a neve, a terra) ou o gás da carga (o concreto,
			// o asfalto: não há areia pra levantar); na areia, a de hoje
			FVector4f Skirt;
			bool bGas = false;
			if (GroundSkirtAt(Ground, Skirt, bGas))
			{
				N->RaymarchDust = bGas ? TankSoot : Skirt;
			}
		}
		N->RaymarchSunFull = TankSunFull;
		N->RaymarchSkyGain = TankSkyGain;
		N->RaymarchGround = TankGround;
		Alive.Add(N);
		// (30/10, a aula do EmberGen com o Felipe: FXLab/EmberGen/m1_morte_jato_v01.ember) o tanque sem painel de alívio (o T-55 da GLA): a
		// munição do carrossel queima, a torre voa e o fogo sai em jato pelo anel dela por ~3 s, morrendo até os 4,5 s. A gravação (o recorte
		// de 9 m em volta do jato, 5,6 cm por voxel) nasce no anel da torre, na escala do tanque, com a fuligem preta do fogo da carcaça (o
		// albedo 0,06 que ele aprovou). O fogo da carcaça entra quando o jato começa a apagar (dois fogos juntos: "nada a ver")
		if (!JetBake.IsNone())
		{
			const UBlastRaymarchSubsystem* RmJet = GetWorld()->GetSubsystem<UBlastRaymarchSubsystem>();
			if (RmJet && RmJet->SourceDuration(JetBake) > 0.0f)
			{
				const float Js = FMath::Max(JetScale, 0.1f);
				if (AExplosionVolume* J = Spawn(JetRingCm - FVector(0.0, 0.0, JetBaseCm * Js), Bake, Js, VariantFraction(Variant, 0x7E7u)))
				{
					J->RaymarchSource = JetBake;
					J->SizeScale = Js;
					J->Thickness = 1.0f / Js;
					J->TimeScale = FMath::Sqrt(Js);                // Froude
					J->RaymarchSoot = FVector4f(1.0f, 0.95f, 0.9f, 0.06f);
					J->RaymarchDust = TankDust;
					J->RaymarchEmit = DL.Emit;
					J->RaymarchSunFull = TankSunFull;
					J->RaymarchSkyGain = TankSkyGain;
					J->RaymarchGround = TankGround;
					J->bFlash = false;                             // o clarão é o da bola da morte
					Alive.Add(J);
					if (WreckOf)
					{
						if (FDamaged* Dj = Damaged.Find(WreckOf))
						{
							const double Shift = JetWreckDelayS * Js;
							Dj->WreckLightStart += Shift;
							Dj->WreckStart += Shift;
							Dj->WreckUntil += Shift;
						}
					}
					UE_LOG(LogTemp, Display, TEXT("BlastFx morte: o jato %s no anel da torre (%s), escala %.2f"), *JetBake.ToString(), *JetRingCm.ToString(), Js);
				}
			}
		}
		if (DL.Ring > 0.0f)
		{
			// o anel de areia da morte v3 (fora da areia o chão diz o que solta: o GroundDustAt, como no acerto)
			const UBlastRaymarchSubsystem* RmRing = GetWorld()->GetSubsystem<UBlastRaymarchSubsystem>();
			float RingO = GroundSimOpacity;
			FVector4f RingC(-1.0f, 0.0f, 0.0f, 0.0f);
			if (RmRing && RmRing->SourceDuration(FName(GroundSimSource)) > 0.0f && GroundDustAt(Ground, RingO, RingC))
			{
				if (RingC.X < 0.0f && DL.RingCol.X >= 0.0f)
				{
					RingC = DL.RingCol;      // na areia: a poeira levantada pela explosão, mais escura e quente que o véu (a da cena do Blender)
				}
				if (DL.RingV == 4 && RmRing->SourceDuration(FName(StrikeBakeGround)) > 0.0f)
				{
					// (A/B) a areia simulada do ataque de mísseis (aprovada 24/10: a cortina, a onda de base, os anéis do véu) na carga da morte:
					// a 0,7 ela é a areia de ~50 kg; a morte do veículo (o combustível e a munição, ~10 kg) pela raiz cúbica, 0,7·(10/50)^⅓ ≈ 0,4;
					// Ring/5 (a 2,0: 0,4)
					const float Ks = DL.Ring * SizeK / 5.0f;
					if (AExplosionVolume* Sk = SpawnDust(Ground, Ks, Ks, RingO, 1, Variant ^ 0xA9u, FVector4f(-1.0f, 0.0f, 0.0f, 0.0f),
						StrikeBakeGround, 1.0f, GroundCurtainDirt))
					{
						Dusts.Last().FadeInS = StrikeSandFadeInS;
						Sk->RaymarchYawRad = 2.0f * PI * VariantFraction(Variant, 0x5A2Du);
					}
				}
				else if (DL.RingV >= 2 && RmRing->SourceDuration(FName(TEXT("eg_poeira3_96"))) > 0.0f)
				{
					// (A/B) a saia simétrica do EmberGen (o domo de areia que sobe ~metade da bola, a v6 do tiro na areia) no tamanho da morte:
					// a escala da saia × Ring/1,6 (a 2,0: ~1,0 = 28,5 m de domínio), o golpe rápido e o resto pairando
					const float K = DL.Ring * SizeK / 1.6f;
					if (AExplosionVolume* Sk = SpawnDust(Ground, GroundRingScale * K, GroundRingTime * FMath::Sqrt(K), RingO, 1, Variant ^ 0xA9u, RingC,
						TEXT("eg_poeira3_96"), GroundRingThickness * DL.RingThick / (3.0f * K)))
					{
						Sk->SlowAfterS = GroundRingSlowAfterS;
						Sk->SlowTimeScale = GroundRingSlowTime * FMath::Sqrt(K);
					}
				}
				if (DL.RingV != 2 && DL.RingV != 4)
				{
				// o anel baixo que corre o chão (na v3 junto do domo: o domo faz a base da nuvem e o anel o espalhado pros lados)
				SpawnDust(Ground, GroundSimScale * DL.Ring * SizeK, GroundSimTime * FMath::Sqrt(DL.Ring * SizeK), RingO, 1, Variant ^ 0xA9u, RingC,
					GroundSimSource, GroundSimThickness * FMath::Min(DL.RingThick, 3.0f) / (DL.Ring * SizeK));
				}
			}
		}
		if (ABlastDebris* Debris = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
		{
			Debris->BallScale = DL.Scale * SizeK;
			Debris->CometTrailK = DL.Jets * SizeK;
			// o orçamento da morte em massa (16/10, o custo de processador: as 11 mortes juntas da vila davam picos de
			// 2,8 ms nos destroços): da 5ª morte em 3 s em diante, metade das peças (o ZH corta partículas por
			// prioridade quando a cena enche; numa morte em massa a diferença não se vê)
			const double NowD = GetWorld()->GetTimeSeconds();
			RecentDeaths.RemoveAll([NowD](double T0) { return NowD - T0 > 3.0; });
			Debris->DetailK = (RecentDeaths.Num() >= 4 ? 0.5f : 1.0f) * FMath::Max(SizeK, 0.35f);   // (30/10) a morte leve: menos peças
			RecentDeaths.Add(NowD);
			// o rastro dos estilhaços na cor da fuligem, escurecido na proporção do albedo (a v2 tinha 0,30: ~1 de tinta)
			const float TrailK = FMath::Clamp(DL.Soot.W / 0.30f, 0.2f, 1.0f);
			Debris->TrailTint = FLinearColor(DL.Soot.X * TrailK, DL.Soot.Y * TrailK, DL.Soot.Z * TrailK, 1.0f);
			Debris->GlowScale = DebrisGlow;
			Debris->Start(Ground + FVector(0.0, 0.0, DebrisHeightCm), int32(Variant * 2654435761u >> 1));
			// a torre solta fuligem (albedo ~0,07): rastro de ~0,7 m que engorda 0,7 m/s e se desfaz em 2,5 s
			Debris->FollowComponent(FindTurret(GetWorld(), Ground), 0.7f, 0.7f, 2.5f, 0.07f, 0.0f, 0.7f);
		}
		return N;
	}
	AExplosionVolume* E = Spawn(Ground, Bake, TankBallScale * Jitter * SizeK, VariantFraction(Variant, 0xA11CE));
	if (!E)
	{
		return nullptr;
	}
	E->TimeScale = Jitter;                    // a gravação no relógio dela, sem esticar pelo tamanho
	E->Thickness = 1.0f / (TankBallScale * SizeK);   // a bola menor tão opaca quanto a Carga Pesada aprovada
	E->SkirtHeight = TankSkirtHeight;
	E->SkirtLife = TankSkirtLife;
	{
		// (25/10, o impacto pelo chão) a clássica (a metade pesada do ataque, o prédio) fora da areia: a saia do tipo ou o gás da carga, na
		// fuligem da clássica (a global do raymarch, a mesma do teto do casco no SpawnWarhead); na areia, a de hoje
		FVector4f Skirt;
		bool bGas = false;
		if (GroundSkirtAt(Ground, Skirt, bGas))
		{
			E->RaymarchDust = bGas ? FVector4f(0.55f, 0.545f, 0.535f, 0.30f) : Skirt;
		}
	}
	// sem o halo de ofuscamento: é um cartaz virado pra câmera que, na câmera alta do jogo,
	// atravessa o chão e mostra uma borda reta (o BlastLab também o tirou da morte de tanque nova)
	Alive.Add(E);
	return E;
}

void UBlastFxDirector::Prewarm(const FVector& At)
{
	if (!LoadAssets() || bWarm || Warmups.Num())
	{
		return;
	}
	// no centro da vista (a Unreal só compila o shader no primeiro desenho: fora da tela não aquece)
	UpdateView();
	for (int32 I = 0; I < Bakes.Num(); ++I)
	{
		if (AExplosionVolume* E = Spawn(At + ViewRight * (I - 1) * 800.0f, Bakes[I], TankBallScale, 0.0f))
		{
			E->Opacity = 0.0f;       // desenha, mas não mostra nada
			E->Age = 0.3f;
			E->bSelfAdvance = false;
			E->bLights = false;
			Warmups.Add(E);
		}
	}
	// os destroços também (os materiais deles estreavam no 1º quadro da morte e a Unreal montava o PSO ali: quadro de
	// 140 ms no BlastLab, 07/10): mil vezes menores em volta de um ponto 10 m acima, somem em 30 s
	if (NewTankBlast())
	{
		// e o raymarch da morte nova (14/10; a sessão do jogo: "a 1ª morte da partida derruba pra ~15 qps por 2 s"). As
		// de cima têm opacidade 0 e o raymarch pula opacidade 0, então tudo dele estreava na 1ª morte: o atlas de blocos
		// (198 MB), a subida da gravação eg_tq_v2, a light function da sombra no sol (no editor, a compilação do material
		// dela) e o 1º uso de cada passe. Esta passa no filtro (opacidade 0,001, invisível) com a bola já cheia, parada:
		// tudo isso acontece aqui, no carregamento. A sombra no sol, depois de ligada, custa ~0,04 ms por quadro (perfil).
		if (AExplosionVolume* E = Spawn(At, Bakes[0], TankBlastScale, 0.0f))
		{
			E->RaymarchSource = FName(TEXT("eg_tq_v2"));
			E->TimeScale = TankBlastTime;
			E->Thickness = 1.0f / TankBlastScale;
			E->RaymarchSoot = TankSoot;
			E->RaymarchDust = TankDust;
			E->RaymarchSunFull = TankSunFull;
			E->RaymarchSkyGain = TankSkyGain;
			E->RaymarchGround = TankGround;
			E->Opacity = 1.0e-3f;
			E->Age = 0.6f;
			E->bSelfAdvance = false;
			E->bLights = false;
			Warmups.Add(E);
		}
		// e a areia do tiro (a gravação dela sobe aqui, não no 1º disparo)
		if (AExplosionVolume* E = Spawn(At + ViewRight * 1200.0f, Bakes[0], MuzzleDustScale, 0.0f))
		{
			E->RaymarchSource = FName(DustSourceName);
			E->TimeScale = MuzzleDustTime;
			E->Opacity = 1.0e-3f;
			E->Age = 0.3f;
			E->bSelfAdvance = false;
			E->bLights = false;
			Warmups.Add(E);
		}
		const FVector DebrisAt = At + FVector(0.0, 0.0, 1000.0);
		if (ABlastDebris* Debris = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(),
			FTransform(FQuat::Identity, DebrisAt * (1.0 - 0.001), FVector(0.001))))
		{
			Debris->GlowScale = DebrisGlow;
			Debris->Start(DebrisAt, 1);
			Debris->SetLifeSpan(30.0f);
		}
	}
	WarmClock = 0.0f;
}

void UBlastFxDirector::UpdateView()
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	bHasView = false;
	if (!PC || !PC->PlayerCameraManager || !Viewport)
	{
		return;
	}
	FVector2D Size;
	Viewport->GetViewportSize(Size);
	if (Size.X < 1.0 || Size.Y < 1.0)
	{
		return;
	}
	const FRotationMatrix M(PC->PlayerCameraManager->GetCameraRotation());
	ViewLocation = PC->PlayerCameraManager->GetCameraLocation();
	ViewForward = M.GetScaledAxis(EAxis::X);
	ViewRight = M.GetScaledAxis(EAxis::Y);
	ViewUp = M.GetScaledAxis(EAxis::Z);
	// o campo de visão da câmera do jogo é o horizontal (padrão da Unreal)
	TanHalfX = FMath::Tan(FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle()) * 0.5f);
	TanHalfY = TanHalfX * float(Size.Y / Size.X);
	bHasView = true;
}

float UBlastFxDirector::ScreenFraction(const FVector& Center, float Radius, bool& bInView) const
{
	bInView = true;
	if (!bHasView)
	{
		return 1.0f;
	}
	// esfera contra as 4 faces e o plano da câmera, nos eixos da vista (como no BlastLab)
	const FVector P = Center - ViewLocation;
	const float Z = FVector::DotProduct(P, ViewForward);
	const float X = FVector::DotProduct(P, ViewRight);
	const float Y = FVector::DotProduct(P, ViewUp);
	const float KX = Radius * FMath::Sqrt(1.0f + TanHalfX * TanHalfX);
	const float KY = Radius * FMath::Sqrt(1.0f + TanHalfY * TanHalfY);
	bInView = Z > -Radius && FMath::Abs(X) - Z * TanHalfX <= KX && FMath::Abs(Y) - Z * TanHalfY <= KY;
	if (!bInView)
	{
		return 0.0f;
	}
	return Z <= Radius ? 1.0f : FMath::Min(Radius / (Z * TanHalfY), 1.0f);
}

void UBlastFxDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateView();
	UpdateDusts();
	UpdateWarheads();
	UpdateDamaged();
	for (auto It = StructDamaged.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
	// (30/10) o prédio que sumiu sem morrer à vista (vendido, tirado pela simulação): o fogo do telhado apaga
	for (auto It = StructFires.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			for (const int32 Id : It.Value())
			{
				FireOut(Id, 1.0f);
			}
			It.RemoveCurrent();
		}
	}
	UpdateFires(DeltaTime);      // (25/10) depois da carcaça: a luz dela fica com a potência do fogo simulado
	UpdateGroundFires(DeltaTime);   // (30/10) o foguinho do tiro no chão (antes da luz dos fogos em loop: a força segue a opacidade)
	UpdateLoopFires();           // (25/10) a luz do fogo em loop das aeronaves (FireLoop), antes de juntar as luzes da cena
	UpdateCrushes();
	UpdateCrushTest();
	UpdateArcTest();
	GatherSceneLights();
	UpdateShadowTest();
	if (Missiles.Num())
	{
		const double NowM = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		for (auto It = Missiles.CreateIterator(); It; ++It)
		{
			if (!It.Value().Fx.IsValid() || NowM - It.Value().Seen > 1.0)
			{
				if (ABlastDebris* M = It.Value().Fx.Get())
				{
					M->EndMissile();
				}
				It.RemoveCurrent();
			}
		}
	}
	if (CookOffs.Num() && GetWorld())
	{
		// (30/10) a munição do Linebacker cozinhando depois da morte: os estouros e o míssil torto (TankDeath, DeathKind 3)
		const double Now = GetWorld()->GetTimeSeconds();
		for (int32 I = CookOffs.Num() - 1; I >= 0; --I)
		{
			if (Now < CookOffs[I].At)
			{
				continue;
			}
			const FCookOff C = CookOffs[I];
			CookOffs.RemoveAtSwap(I);
			bool bInView = true;
			ScreenFraction(C.Cm, 2500.0f, bInView);   // (o ShotNearView, que vem mais abaixo no arquivo)
			if (!bInView)
			{
				continue;
			}
			if (C.bMissile)
			{
				// o míssil sai torto: 30–60° acima do chão, rumo qualquer, 40–80 m, num clarão de motor
				const float Yaw = 2.0f * PI * VariantFraction(C.Seed, 0x7A);
				const float El = FMath::DegreesToRadians(30.0f + 30.0f * VariantFraction(C.Seed, 0x7B));
				const FVector Dir(FMath::Cos(Yaw) * FMath::Cos(El), FMath::Sin(Yaw) * FMath::Cos(El), FMath::Sin(El));
				const float Reach = 4000.0f + 4000.0f * VariantFraction(C.Seed, 0x7C);
				if (ABlastDebris* T = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
				{
					T->GlowScale = TracerGlow * 1.5f;
					T->StartTracer(C.Cm, C.Cm + Dir * Reach, 4500.0f, int32(C.Seed * 3266489917u >> 1));
				}
			}
			if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
			{
				// o estouro: o clarão curto, as faíscas pra cima e a luz (o acerto em ~0,45 do tiro de tanque, apontado pro céu)
				Fx->GlowScale = ImpactGlow;
				Fx->ImpactK = C.bMissile ? 0.35f : 0.45f;
				Fx->StartImpact(C.Cm, FVector::UpVector, -FVector::UpVector, int32(C.Seed * 2654435761u >> 1));
			}
		}
	}
	for (auto It = BuildingShots.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || It.Value().Num() == 0)
		{
			It.RemoveCurrent();   // (01/11) o prédio que caiu, ou sem tiro a caminho
		}
	}
	if (DelayedHits.Num() || PendingShots.Num())
	{
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		for (int32 I = DelayedHits.Num() - 1; I >= 0; --I)
		{
			if (Now >= DelayedHits[I].Shot.Arrive)
			{
				const FDelayedHit Dh = DelayedHits[I];
				DelayedHits.RemoveAtSwap(I);
				SpawnArmorImpact(Dh.Shot, Dh.Target.Get(), Dh.GroundZCm, Dh.bKill);
			}
		}
		for (auto It = PendingShots.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid() || Now - It.Value().Born > 3.0)
			{
				It.RemoveCurrent();
				continue;
			}
			// (16/10, o Felipe: "chega, some, depois aparece") o que se vê manda: as faíscas nascem no quadro em que o
			// traçante chega, no ponto dele; o aviso da simulação depois só traz a morte (o jeito dos jogos: o efeito
			// segue o projétil desenhado, o dano fica com a simulação)
			FShotPoint& P = It.Value();
			if (!P.bSpawned && Now >= P.Arrive)
			{
				P.bSpawned = true;
				LastHitAt.Add(It.Key(), Now);
				SpawnArmorImpact(P, It.Key().Get(), P.GroundZCm, false);
			}
		}
	}
	if (Warmups.Num())
	{
		WarmClock += DeltaTime;
		const bool bShadersDone = !GShaderCompilingManager || GShaderCompilingManager->GetNumRemainingJobs() == 0;
		if ((WarmClock > WarmSeconds && bShadersDone) || WarmClock > 60.0f)
		{
			for (AExplosionVolume* E : Warmups)
			{
				if (IsValid(E))
				{
					E->Destroy();
				}
			}
			Warmups.Reset();
			bWarm = true;
		}
	}
	// a gravação acabou: a explosão sai de cena. A morte de tanque nova toca a gravação própria (a eg_tq_v2, 3 s) num
	// ator que segura a variante da Carga Pesada; ela acabava pelo relógio da Carga Pesada (16/10, fx_acerto: mais
	// longa), e o último quadro da própria (a areia assentada que enche o chão da caixa) ficava congelado ~1 s em volta
	// da carcaça: a borda clara reta. Agora sai no fim da gravação própria, esmaecendo no último 0,8 s dela (o
	// EndFadeS da noite de 13/10)
	const UBlastRaymarchSubsystem* RmLife = GetWorld() ? GetWorld()->GetSubsystem<UBlastRaymarchSubsystem>() : nullptr;
	for (int32 I = Alive.Num() - 1; I >= 0; --I)
	{
		AExplosionVolume* E = Alive[I];
		const float Own = IsValid(E) && RmLife && !E->RaymarchSource.IsNone() ? RmLife->SourceDuration(E->RaymarchSource) : 0.0f;
		// (29/10) a morte v3 com a nuvem viva: a gravação parada não chega no fim; quem acaba é a idade (HoldAge)
		const bool bHeld = IsValid(E) && Own > 0.0f && E->BakeHoldS > 0.0f && E->HoldAge > 0.0f;
		const bool bDone = !IsValid(E) || (bHeld ? E->IsFinished() : (Own > 0.0f ? E->BakeTime() >= Own : E->IsFinished()));
		if (bDone)
		{
			if (IsValid(E))
			{
				E->Destroy();
			}
			Alive.RemoveAtSwap(I);
			continue;
		}
		if (bHeld)
		{
			// a nuvem viva da morte v3: da parada ao fim ela esmaece (a fuligem dilui) e sobe como térmica
			const float HoldReal = E->BakeHoldS * FMath::Max(E->TimeScale, 0.01f);
			E->Opacity = 1.0f - FMath::SmoothStep(HoldReal, E->HoldAge, E->Age);
			if (E->Age > HoldReal)
			{
				// e anda com o vento do mapa, na mesma lei das nuvens dos mísseis (o perfil logarítmico na altura do miolo: a nuvem
				// da v3 tem o miolo a ~6 m e sobe): a fumaça solta vai com o ar
				const float Zc = 6.0f + (E->Age - HoldReal) * DeathLook().RiseCmS * 0.01f;
				const float Wind = WindSpeedMs * WindProfileK(Zc) * DeathLook().WindK;
				// pelo avanço da idade dela (o relógio do volume, o mesmo do esmaecer). Conferido no registro (-DeathDriftLog): a 0,8 do
				// vento de 5,6 m/s do mapa ela anda 4,2 m/s e sobe 2,5 m/s (o "6×" da folha explo_h era a régua errada de pixel pra metro)
				const float A0 = E->DriftAge < 0.0f ? HoldReal : FMath::Max(E->DriftAge, HoldReal);
				static const bool bDriftLog = FParse::Param(FCommandLine::Get(), TEXT("DeathDriftLog"));
				if (bDriftLog)
				{
					UE_LOG(LogTemp, Display, TEXT("BlastFx nuvem: idade %.3f parada %.3f vento %.2f m/s (mapa %.2f) dt %.3f anda %.1f cm, pos %s"),
						E->Age, HoldReal, Wind, WindSpeedMs, DeltaTime, Wind * 100.0f * FMath::Max(E->Age - A0, 0.0f), *E->GetActorLocation().ToString());
				}
				E->DriftAge = E->Age;
				E->AddActorWorldOffset(FVector(FMath::Cos(WindYawRad) * Wind * 100.0f, FMath::Sin(WindYawRad) * Wind * 100.0f,
					DeathLook().RiseCmS) * FMath::Max(E->Age - A0, 0.0f));
				// o quadro parado ainda tem o miolo quente: o fogo esfria (τ 0,35 s) e fica a fumaça; sem isso a bola ficava acesa
				E->RaymarchEmit = DeathLook().Emit * FMath::Exp(-(E->Age - HoldReal) / 0.35f);
			}
		}
		else if (Own > 0.0f)
		{
			// (A/B, 16/10: -DeathFadeFrom= s da gravação onde começa o esmaecer; padrão os últimos 0,8 s)
			static const float FadeFrom = [] { float V = -1.0f; FParse::Value(FCommandLine::Get(), TEXT("DeathFadeFrom="), V); return V; }();
			E->Opacity = 1.0f - FMath::SmoothStep(FadeFrom >= 0.0f ? FMath::Min(FadeFrom, Own - 0.05f) : Own - 0.8f, Own, E->BakeTime());
		}
	}
	// orçamento de detalhe: as maiores na tela ficam com tudo
	TArray<AExplosionVolume*, TInlineAllocator<32>> Seen;
	for (AExplosionVolume* E : Alive)
	{
		if (E->IsOnScreen())
		{
			Seen.Add(E);
		}
		else
		{
			E->LodFloor = 0;
		}
	}
	Seen.Sort([](const AExplosionVolume& A, const AExplosionVolume& B) { return A.ScreenFraction() > B.ScreenFraction(); });
	for (int32 I = 0; I < Seen.Num(); ++I)
	{
		Seen[I]->LodFloor = I < FullDetailBudget ? 0 : (I < 2 * FullDetailBudget ? 1 : 2);
	}
}

// =============================================================================
// O tiro (14/10, a v3). Os alvos: a arte 4.2 (o acerto na blindagem, docs/concept/explosions/type_4.2_armor_sparks.webp),
// a 4.1 (o tiro na areia) e os game shots (o clarão de boca de close_game_cam_gpt, os traçantes laranja curtos). O estudo
// e os números: docs/TIRO_E_ACERTO.md.

bool UBlastFxDirector::ShotFxEnabled()
{
	static const bool bOn = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("ShotFx="), V); return V != 0; }();
	return bOn;
}

namespace
{
	// (17/10 noite, o Felipe: "quando os tanques de outro time começaram a pipocar guerra lá em cima [...] caiu bastante os fps",
	// com a briga revelada pelo aliado e fora da câmera) o efeito curto do tiro (o disparo, o traçante, o acerto, o tiro na
	// areia: menos de 2 s) só nasce perto da vista, como o Effect Type do Niagara corta pela significância: a esfera dele com
	// uma folga pra sombra comprida do sol baixo. Quem leva a câmera até lá no meio de um tiro não pega aquele tiro; a morte,
	// a carcaça, a cratera e a marca na blindagem continuam. -ShotFxCull=0 volta o de antes (A/B)
	constexpr float ShotViewMarginCm = 2500.0f;

	bool ShotNearView(const UBlastFxDirector& D, const FVector& CenterCm, float RadiusCm)
	{
		static const bool bCull = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("ShotFxCull="), V); return V != 0; }();
		bool bIn = true;
		if (bCull)
		{
			D.ScreenFraction(CenterCm, RadiusCm + ShotViewMarginCm, bIn);
		}
		return bIn;
	}
}

AExplosionVolume* UBlastFxDirector::SpawnDust(const FVector& GroundCm, float Scale, float TimeScale, float Opacity, int32 Prio, uint32 Variant,
	const FVector4f& Color, const TCHAR* Source, float Thickness, const FVector4f& Soot)
{
	const FName SourceName(Source ? Source : DustSourceName);
	if (!LoadAssets())
	{
		return nullptr;
	}
	const UBlastRaymarchSubsystem* Rm = GetWorld()->GetSubsystem<UBlastRaymarchSubsystem>();
	const float Duration = Rm ? Rm->SourceDuration(SourceName) : 0.0f;
	if (Duration <= 0.0f)
	{
		return nullptr;                             // sem a gravação, sem areia (o volume da Unreal não a desenharia igual)
	}
	// o orçamento: o raymarch desenha até 16 por quadro e as mortes vêm primeiro. Cheio, o acerto tira de cena o sopro
	// de disparo mais velho; o sopro, cheio, não nasce
	Dusts.RemoveAll([](const FDust& D) { return !D.E.IsValid(); });
	static const IConsoleVariable* EffectsQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.EffectsQuality"));
	const int32 Eq = EffectsQuality ? EffectsQuality->GetInt() : 3;
	const int32 Budget = Eq >= 3 ? MaxDustsHigh : (Eq == 2 ? MaxDustsHigh / 2 : 1);
	if (Dusts.Num() >= Budget)
	{
		// (17/10) cheio, sai a que está mais perto do fim (a fração já tocada da gravação dela), nunca a que acabou de nascer:
		// antes o tiro na areia tirava de cena o disparo mais velho (vivo, no meio do fogo) e o disparo cheio não nascia
		// (17/10 tarde) pela importância, como o Effect Type do Niagara (significância e orçamento): primeiro as fora da tela,
		// depois a mais perto do fim (a fração da vida dela); as de menos de 0,25 s só se não houver outra
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const APlayerCameraManager* Cam = PC ? PC->PlayerCameraManager.Get() : nullptr;
		auto OnScreen = [Cam](const AExplosionVolume* V)
		{
			if (!Cam)
			{
				return true;
			}
			// um cone na diagonal da vista, alargado pelo raio da areia (~12 m × a escala, folgado)
			const FVector D = V->GetActorLocation() + FVector(0.0, 0.0, 300.0) - Cam->GetCameraLocation();
			const double Dist = D.Size(), R = 1200.0 * FMath::Max(V->SizeScale, 0.3f) + 500.0;
			if (Dist <= R)
			{
				return true;
			}
			const double Half = FMath::Atan(FMath::Tan(FMath::DegreesToRadians(0.5 * Cam->GetFOVAngle())) * 1.15);
			const double Ang = FMath::Acos(FMath::Clamp(FVector::DotProduct(D / Dist, Cam->GetCameraRotation().Vector()), -1.0, 1.0));
			return Ang < Half + FMath::Asin(FMath::Min(R / Dist, 1.0));
		};
		int32 Victim = INDEX_NONE;
		float Most = -1.0f;
		for (int32 k = 0; k < Dusts.Num(); ++k)
		{
			const AExplosionVolume* V = Dusts[k].E.Get();
			float Done = Dusts[k].LifeS > 0.0f ? V->Age / Dusts[k].LifeS : V->BakeTime() / FMath::Max(Dusts[k].Duration, 0.01f);
			if (Dusts[k].bLinger && V->Age > Dusts[k].HoldAge)
			{
				// (25/10) a nuvem que fica é a menos importante (a areia que ainda cai e as explosões vêm antes): sai primeiro, a mais velha
				Done = 2.0f + (V->Age - Dusts[k].HoldAge) / FMath::Max(Dusts[k].LingerLifeS, 1.0f);
			}
			const float Score = (OnScreen(V) ? 0.0f : 10.0f) + (V->Age < 0.25f ? -20.0f : 0.0f) + Done;
			if (Score > Most)
			{
				Most = Score;
				Victim = k;
			}
		}
		if (Victim == INDEX_NONE)
		{
			return nullptr;
		}
		Dusts[Victim].E->Destroy();
		Dusts.RemoveAt(Victim);
	}
	// (23/10) as ogivas dos mísseis cedem a vaga do raymarch (o tiro aprovado não pode sumir no corte dele, que é pela distância)
	MakeRoomForPriority(GroundCm, Scale);
	AExplosionVolume* E = Spawn(GroundCm, Bakes[Variant % uint32(Bakes.Num())], Scale, VariantFraction(Variant, 0xD057));
	if (!E)
	{
		return nullptr;
	}
	E->RaymarchSource = SourceName;
	E->TimeScale = TimeScale;
	E->Thickness = Thickness;
	E->Opacity = Opacity;
	E->bLights = false;                             // a areia não tem fogo: a luz do tiro é a dos destroços
	E->bFlash = false;                              // nem o clarão da detonação (na bola do disparo ele a deixava branca, 16/10)
	E->RaymarchSoot = Soot.X >= 0.0f ? Soot : TankSoot;
	E->RaymarchDust = Color.X >= 0.0f ? Color : TankDust;   // a areia do mapa (a mesma da morte de tanque) ou a cor pedida
	E->RaymarchSunFull = TankSunFull;
	E->RaymarchSkyGain = TankSkyGain;
	E->RaymarchGround = TankGround;
	Dusts.Add({ E, Duration, Opacity, Prio });
	return E;
}

void UBlastFxDirector::UpdateDusts()
{
	// a areia some no último quarto da gravação e sai de cena no fim (a vida do ator seria a da Carga Pesada)
	for (int32 I = Dusts.Num() - 1; I >= 0; --I)
	{
		AExplosionVolume* E = Dusts[I].E.Get();
		if (!E)
		{
			Dusts.RemoveAtSwap(I);
			continue;
		}
		const float T = E->BakeTime();
		const float Life = Dusts[I].LifeS;
		if (Dusts[I].bLinger)
		{
			if (!UpdateLinger(Dusts[I], E))
			{
				E->Destroy();
				Dusts.RemoveAtSwap(I);
			}
			continue;
		}
		if (T >= Dusts[I].Duration || (Life > 0.0f && E->Age >= Life))
		{
			E->Destroy();
			Dusts.RemoveAtSwap(I);
			continue;
		}
		// e entra nos primeiros 0,25 s da gravação (16/10: a gravação da areia nasce como uma esfera lisa, e no 1º quadro
		// do disparo ela era uma bola clara de sabão à frente da boca)
		E->Opacity = Dusts[I].Opacity0 * (1.0f - FMath::SmoothStep(0.75f * Dusts[I].Duration, Dusts[I].Duration, T))
			* FMath::SmoothStep(0.0f, FMath::Max(Dusts[I].FadeInS, 0.01f), T);
		if (Life > 0.0f)
		{
			// (17/10 tarde) a vida de verdade: rala aos poucos até sumir (o disparo)
			E->Opacity *= 1.0f - FMath::SmoothStep(Dusts[I].FadeFromS, Life, E->Age);
		}
	}
}

bool UBlastFxDirector::UpdateLinger(FDust& D, AExplosionVolume* E)
{
	// (25/10, a poeira que fica: a conta em MakeLinger e nas constantes Linger*) até a parada, a gravação de sempre sem o esmaecer do fim;
	// depois, a mesma nuvem parada no quadro (o ruído andando) abrindo, ralando, assentando e indo com o vento
	const float FadeIn = FMath::SmoothStep(0.0f, FMath::Max(D.FadeInS, 0.01f), E->BakeTime());
	const float Dt = FMath::Max(E->Age - D.LastAge, 0.0f);
	D.LastAge = E->Age;
	const float Tl = E->Age - D.HoldAge;
	if (Tl <= 0.0f)
	{
		E->Opacity = D.Opacity0 * FadeIn;
		return true;
	}
	if (!D.bHeld)
	{
		// (25/10, o custo: 6 nuvens do mesmo ataque custavam ~9 ms de marcha na 1660) a que para em cima de outra já parada da mesma gravação
		// passa a massa pra ela: esta esmaece em 1,5 s enquanto aquela engrossa (a massa somada) e alarga (o espalhamento dos centros) e o pé
		// vai pro centro de massa. Vista de cima a soma fica a mesma; a forma é a da que ficou
		D.bHeld = true;
		D.Foot0 = D.Foot1 = D.FootBase;
		FDust* Host = nullptr;
		const FVector At = E->GetActorLocation();
		for (FDust& O : Dusts)
		{
			AExplosionVolume* Oe = O.E.Get();
			if (&O == &D || !Oe || !O.bLinger || !O.bHeld || O.RetireAt >= 0.0f || Oe->RaymarchSource != E->RaymarchSource
				|| FVector::Dist2D(Oe->GetActorLocation(), At) > 80.0 * FMath::Max(O.R0M, D.R0M))
			{
				continue;
			}
			if (!Host || O.MassK > Host->MassK)
			{
				Host = &O;
			}
		}
		if (Host)
		{
			AExplosionVolume* He = Host->E.Get();
			const float M = (D.Opacity0 * D.Thick0) / FMath::Max(Host->Opacity0 * Host->Thick0, 1.0e-3f) * D.MassK;
			const float Mt = Host->MassK + M;
			const FVector Off = FVector(At.X - He->GetActorLocation().X, At.Y - He->GetActorLocation().Y, 0.0) / 100.0;   // m
			const double Dm = Off.Size();
			Host->MassK0 = Host->MassK;
			Host->UnionK0 = Host->UnionK;
			Host->Foot0 = Host->FootBase;
			Host->Foot1 = Host->FootBase + Off * (100.0 * M / Mt);
			// o segundo momento dos centros em volta do centro de massa novo
			Host->VarM2 += D.VarM2 + float(Host->MassK * FMath::Square(Dm * M / Mt) + M * FMath::Square(Dm * Host->MassK / Mt));
			Host->MassK1 = Mt;
			Host->UnionK1 = FMath::Sqrt(1.0f + Host->VarM2 / (Mt * Host->R0M * Host->R0M));
			Host->MergeAt = He->Age;
			D.RetireAt = E->Age;
		}
	}
	if (D.MergeAt >= 0.0f)
	{
		const float W = FMath::SmoothStep(D.MergeAt, D.MergeAt + LingerRetireS, E->Age);
		D.MassK = FMath::Lerp(D.MassK0, D.MassK1, W);
		D.UnionK = FMath::Lerp(D.UnionK0, D.UnionK1, W);
		D.FootBase = FMath::Lerp(D.Foot0, D.Foot1, double(W));
	}
	// o vento de agora (ao vivo: o painel do laboratório muda no meio)
	const float U10 = WindSpeedMs;
	const FVector2D WDir(FMath::Cos(WindYawRad), FMath::Sin(WindYawRad));
	const float UStar = LingerKappa * U10 / FMath::Loge(10.0f / LingerZ0M);
	const float SigV = LingerSigVK * UStar + LingerCalmSigVMs, SigW = LingerSigWK * UStar + LingerCalmSigWMs;
	// o tamanho de agora (pelo lado) e a altura do meio da nuvem (o volume cresce por igual)
	const float Rk = D.R0M + D.VrMs * LingerFrontTauS * (1.0f - FMath::Exp(-Tl / LingerFrontTauS));
	auto SizeK = [&D, Rk](float Sig) { return FMath::Sqrt(Rk * Rk + 4.0f * Sig * Sig) / D.R0M; };
	const float Gy0 = SizeK(D.SigY);
	const float Zc = D.ZcM * Gy0;
	const float TL = LingerTLK * Zc / SigW;
	if (Dt > 0.0f)
	{
		// Taylor: dσ/dt = σv·(1 + t/4T_L)/(1 + t/2T_L)^1,5 (a derivada de σv·t/√(1 + t/2T_L), integrada: o vento pode mudar no meio)
		auto Rate = [Tl, TL](float S) { const float A = Tl / (2.0f * TL); return S * (1.0f + 0.5f * A) / FMath::Pow(1.0f + A, 1.5f); };
		const float DSigZ = Rate(SigW) * Dt;
		const float Shear = UStar / (LingerKappa * FMath::Max(Zc, 0.2f));
		D.SigXs += Shear * (D.SigZ + Tl * DSigZ / FMath::Max(Dt, 1.0e-4f)) * Dt / FMath::Sqrt(3.0f);
		D.SigY += Rate(SigV) * Dt;
		D.SigZ += DSigZ;
		// os finos assentam (a nuvem mais alta leva mais tempo)
		D.Dep *= FMath::Exp(-D.SettleMs * Dt / FMath::Max(Zc, 0.2f));
		// a deriva: o vento na altura do meio, pego em ~1 s
		const FVector2D Target = WDir * (U10 * WindProfileK(Zc));
		D.DriftVelMs += (Target - D.DriftVelMs) * (1.0f - FMath::Exp(-Dt / LingerAirTauS));
		D.DriftCm += FVector(D.DriftVelMs.X, D.DriftVelMs.Y, 0.0) * (double(Dt) * 100.0);
	}
	const float Gy = SizeK(D.SigY);
	const float Gx = SizeK(FMath::Sqrt(D.SigY * D.SigY + D.SigXs * D.SigXs));
	E->SizeScale = D.Scale0 * Gy * D.UnionK;
	// a massa se conserva: vista de cima, ∝ M/(Gx·Gy·U²); o volume cresce Gy·U por igual (o caminho do raio também), então a espessura
	// ÷ Gx·Gy²·U³ (M e U: as nuvens do mesmo ataque somadas nesta)
	E->Thickness = D.Thick0 * D.Dep * D.MassK / (Gx * Gy * Gy * D.UnionK * D.UnionK * D.UnionK);
	const FVector At = D.FootBase + D.DriftCm;
	const double Gz = GetWorld() ? BlastFxGround::HeightAtCached(GetWorld(), At.X, At.Y) : At.Z;
	const FVector Foot(At.X, At.Y, D.DriftCm.IsNearlyZero() ? D.FootBase.Z : Gz);
	if (!Foot.Equals(E->GetActorLocation(), 0.5))
	{
		E->SetActorLocation(Foot);
	}
	// o fim: a nuvem rala (6% da profundidade óptica da parada) ou a vida do preset; a aposentadoria (o teto) esmaece em 1,5 s
	const float TauRel = D.Dep / (Gx * Gy);
	if (D.RetireAt < 0.0f && (TauRel < LingerEndTau || Tl >= D.LingerLifeS))
	{
		D.RetireAt = E->Age;
	}
	float Out = 1.0f;
	if (D.RetireAt >= 0.0f)
	{
		Out = 1.0f - FMath::SmoothStep(0.0f, LingerRetireS, E->Age - D.RetireAt);
		if (Out <= 0.0f)
		{
			return false;
		}
	}
	E->Opacity = D.Opacity0 * FadeIn * Out;
	return true;
}

void UBlastFxDirector::MuzzleBlast(const FVector& MuzzleCm, const FVector& Dir, float GroundZCm, uint32 Variant)
{
	if (!ShotFxEnabled() || !GetWorld() || !ShotNearView(*this, MuzzleCm, 1500.0f))
	{
		return;
	}
	CSV_SCOPED_TIMING_STAT(BlastFxDirector, MuzzleBlast);
	Variant ^= (++ShotSerial) * 0x9E3779B9u;
	const FVector D = FVector(Dir.X, Dir.Y, 0.0).GetSafeNormal();
	if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Fx->GlowScale = MuzzleGlow;
		Fx->StartMuzzle(MuzzleCm, Dir, int32(Variant * 2246822519u >> 1));
	}
	// a bola secundária (o gás que reacende à frente da boca, 15–150 ms) e a fumaça do propelente depois: a gravação da
	// morte de tanque minúscula e rápida no raymarch (fogo de verdade, que tapa o que está atrás; a aditiva virava creme
	// sobre a areia clara, 16/10)
	// (opções de A/B, sem recompilar: -ShotFxBallEmit=, -ShotFxBallSoot=r,g,b,a, -ShotFxBallSlow=, -ShotFxDustSlow=)
	static const float BallEmit = CmdFloat(TEXT("ShotFxBallEmit="), MuzzleBallEmit);
	static const float BallSlow = CmdFloat(TEXT("ShotFxBallSlow="), MuzzleBallSlowTime);
	static const float DustSlow = CmdFloat(TEXT("ShotFxDustSlow="), MuzzleDustSlowTime);
	static const FVector4f BallSoot = CmdColor(TEXT("ShotFxBallSoot="), MuzzleSmokeSoot);
	// (v7, 17/10) a gravação simulada no rumo do cano; -MuzzleV=6 volta a bola de antes (a morte em miniatura)
	static const int32 MuzzleVer = [] { int32 V = 7; FParse::Value(FCommandLine::Get(), TEXT("MuzzleV="), V); return V; }();
	static const float SimEmit = CmdFloat(TEXT("ShotFxSimEmit="), MuzzleSimEmit);
	static const float SimThick = CmdFloat(TEXT("ShotFxSimThick="), MuzzleSimThickness);
	const UBlastRaymarchSubsystem* Rm = GetWorld()->GetSubsystem<UBlastRaymarchSubsystem>();
	if (MuzzleVer >= 7 && Rm && Rm->SourceDuration(FName(MuzzleSimSource)) > 0.0f)
	{
		static const FVector4f SimSoot = CmdColor(TEXT("ShotFxBallSoot="), MuzzleSimSoot);
		static const float SimScale = CmdFloat(TEXT("ShotFxSimScale="), MuzzleSimScale);
		static const float SimFast = CmdFloat(TEXT("ShotFxSimFast="), MuzzleSimFastTime);
		static const float SimSlow = CmdFloat(TEXT("ShotFxSimSlow="), MuzzleSimSlowTime);
		static const float SimLife = CmdFloat(TEXT("ShotFxSimLife="), MuzzleSimLifeS);
		static const float SimFade = CmdFloat(TEXT("ShotFxSimFade="), MuzzleSimFadeFromS);
		if (AExplosionVolume* Sim = SpawnDust(FVector(MuzzleCm.X, MuzzleCm.Y, MuzzleCm.Z - MuzzleSimHeightCm * SimScale), SimScale, SimFast, 1.0f, 0,
			Variant ^ 0xB411u, MuzzleSmokeDust, MuzzleSimSource, SimThick, SimSoot))
		{
			// (17/10 tarde) num andamento só (o 2º, a SimSlow, fazia a fumaça inflar no fim); -ShotFxSimLife=0 volta o de antes
			if (SimLife <= 0.0f)
			{
				Sim->SlowAfterS = MuzzleSimSlowAfterS;
				Sim->SlowTimeScale = SimSlow;
			}
			else if (FDust* Dd = Dusts.FindByPredicate([Sim](const FDust& X) { return X.E.Get() == Sim; }))
			{
				Dd->LifeS = SimLife;
				Dd->FadeFromS = FMath::Min(SimFade, SimLife);
			}
			Sim->RaymarchEmit = SimEmit;
			Sim->RaymarchYawRad = float(FMath::Atan2(Dir.Y, Dir.X));
		}
	}
	else if (AExplosionVolume* Ball = SpawnDust(FVector(MuzzleCm.X + D.X * MuzzleBallAheadCm, MuzzleCm.Y + D.Y * MuzzleBallAheadCm, MuzzleCm.Z - MuzzleBallDropCm),
		MuzzleBallScale, MuzzleBallTime, 1.0f, 0, Variant ^ 0xB411u, MuzzleSmokeDust, TEXT("eg_tq_v2"), MuzzleBallThickness, BallSoot))
	{
		Ball->SlowAfterS = MuzzleBallSlowAfterS;
		Ball->SlowTimeScale = BallSlow;
		Ball->RaymarchEmit = BallEmit;
	}
	// a areia que o sopro levanta do chão em volta, à frente da boca (a sobrepressão bate no chão). (25/10, o impacto pelo chão) só o que
	// o chão dali tem pra soltar (no concreto, nada; na neve, o pó)
	const FVector SandAt(MuzzleCm.X + D.X * MuzzleDustAheadCm, MuzzleCm.Y + D.Y * MuzzleDustAheadCm, GroundZCm);
	float SandO = MuzzleDustOpacity;
	FVector4f SandC(-1.0f, 0.0f, 0.0f, 0.0f);
	if (!GroundDustAt(SandAt, SandO, SandC))
	{
		return;
	}
	if (AExplosionVolume* Sand = SpawnDust(SandAt, MuzzleDustScale, MuzzleDustTime, SandO, 0, Variant, SandC))
	{
		Sand->SlowAfterS = MuzzleDustSlowAfterS;
		Sand->SlowTimeScale = DustSlow;
	}
}

void UBlastFxDirector::Tracer(const FVector& FromCm, const FVector& ToCm, float SpeedCmPerS, uint32 Variant)
{
	if (!ShotFxEnabled() || !GetWorld() || !ShotNearView(*this, 0.5 * (FromCm + ToCm), 0.5f * float(FVector::Dist(FromCm, ToCm)) + 500.0f))
	{
		return;
	}
	// o tiro no tanque: o ponto na blindagem já aqui, e o traçante termina nele (o acerto usa o mesmo ponto)
	FVector End = ToCm;
	// (01/11) o tiro no prédio: o ponto da mira deste atirador na fachada (BuildingAimPoint), e o acerto usa o mesmo (BuildingHit)
	AActor* UnitNear = FindUnitNear(GetWorld(), ToCm);
	if (AActor* Building = UnitNear ? nullptr : FindStructureAt(GetWorld(), ToCm))
	{
		FShotPoint Shot;
		if (BuildingAimPoint(Building, FromCm, float(ToCm.Z - 120.0), Variant, Shot))
		{
			const double Now = GetWorld()->GetTimeSeconds();
			Shot.Born = Now;
			Shot.Arrive = Now + FVector::Dist(FromCm, Shot.Point) / FMath::Max(double(SpeedCmPerS), 1.0);
			BuildingShots.FindOrAdd(Building).Add(Shot);
			End = Shot.Point;
		}
	}
	else if (AActor* Target = UnitNear)
	{
		FShotPoint Shot;
		const uint32 ShotVariant = Variant ^ (++ShotSerial) * 0x9E3779B9u;
		if (ArmorPoint(Target, FromCm, ToCm, float(ToCm.Z - 120.0), ShotVariant, Shot))
		{
			const double Now = GetWorld()->GetTimeSeconds();
			Shot.Born = Now;
			Shot.Arrive = Now + FVector::Dist(FromCm, Shot.Point) / FMath::Max(double(SpeedCmPerS), 1.0);
			Shot.Variant = ShotVariant;
			Shot.GroundZCm = float(ToCm.Z - 120.0);
			Shot.bRicochet = bNextShotRicochet;
			PendingShots.Add(Target, Shot);
			End = Shot.Point;
		}
	}
	bNextShotRicochet = false;
	if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Fx->GlowScale = TracerGlow;
		Fx->StartTracer(FromCm, End, SpeedCmPerS, int32(Variant * 3266489917u >> 1));
	}
}

void UBlastFxDirector::ArmorHit(const FVector& AtCm, const FVector& FromCm, AActor* Target, float GroundZCm, uint32 Variant, bool bKill)
{
	if (!ShotFxEnabled() || !GetWorld())
	{
		return;
	}
	CSV_SCOPED_TIMING_STAT(BlastFxDirector, ArmorHit);
	const bool bRicochetNow = bNextShotRicochet;   // (30/10) o aviso do ricochete vale só pra esta chamada
	bNextShotRicochet = false;
	Variant ^= (++ShotSerial) * 0x9E3779B9u;
	// no máximo um efeito por vítima a cada 100 ms (o ZH, ActiveBody.cpp): vários tiros no mesmo quadro não somam clarões
	const double Now = GetWorld()->GetTimeSeconds();
	if (Target)
	{
		if (const double* Last = LastHitAt.Find(Target); Last && Now - *Last < 0.1)
		{
			return;
		}
		LastHitAt.Add(Target, Now);
		if (LastHitAt.Num() > 256)
		{
			for (auto It = LastHitAt.CreateIterator(); It; ++It)
			{
				if (!It.Key().IsValid() || Now - It.Value() > 1.0)
				{
					It.RemoveCurrent();
				}
			}
		}
	}
	FShotPoint Shot;
	const FShotPoint* Pending = Target ? PendingShots.Find(Target) : nullptr;
	if (Pending && Now - Pending->Born < 3.0 && Pending->bSpawned)
	{
		// (16/10) as faíscas já nasceram quando o traçante chegou (o aviso da simulação vem 1–3 quadros depois: o
		// projétil dela sai do centro do tanque); daqui só a morte
		PendingShots.Remove(Target);
		if (bKill)
		{
			EndDamaged(Target);
			StartWreck(Target);
		}
		return;
	}
	if (Pending && Now - Pending->Born < 3.0)
	{
		Shot = *Pending;
		PendingShots.Remove(Target);
	}
	else
	{
		ArmorPoint(Target, FromCm, AtCm, GroundZCm, Variant, Shot);
		Shot.Variant = Variant;
		Shot.Arrive = Now;
	}
	Shot.bRicochet = Shot.bRicochet || bRicochetNow;
	if (bKill)
	{
		EndDamaged(Target);                         // a morte assume (a fumaça do ferido para)
		StartWreck(Target);                         // e a carcaça fica queimando depois que a explosão se desfaz
	}
	if (!ShotNearView(*this, Shot.Point, 1500.0f))
	{
		MarkHull(Shot.Part.Get(), Shot.LocalCm, 0);   // fora da vista: só a marca, que fica (as faíscas e a areia não nascem)
		return;
	}
	if (Shot.Arrive > Now + 0.005)
	{
		// o traçante ainda não chegou: as faíscas esperam por ele
		FDelayedHit& Dh = DelayedHits.AddDefaulted_GetRef();
		Dh.Shot = Shot;
		Dh.Target = Target;
		Dh.GroundZCm = GroundZCm;
		Dh.bKill = bKill;
		return;
	}
	SpawnArmorImpact(Shot, Target, GroundZCm, bKill);
}

bool UBlastFxDirector::ArmorPoint(AActor* Target, const FVector& FromCm, const FVector& AtCm, float GroundZCm, uint32 Variant, FShotPoint& Out) const
{
	// o ponto: o raio do atirador até o alvo entra na caixa do próprio tanque (no espaço dele, a caixa justa das malhas),
	// na face virada pro atirador; a altura sorteada entre o casco e a torre (a arte acerta a bochecha da torre)
	Out.Point = AtCm;
	Out.Normal = (FromCm - AtCm).GetSafeNormal2D();
	const float AimZ = GroundZCm + FMath::Lerp(HitHeightMinCm, HitHeightMaxCm, VariantFraction(Variant, 0x4A1F));
	const FVector From(FromCm.X, FromCm.Y, AimZ);
	const FVector Across = FVector::CrossProduct((AtCm - FromCm).GetSafeNormal2D(), FVector::UpVector);
	const FVector Aim = FVector(AtCm.X, AtCm.Y, AimZ) + Across * FMath::Lerp(-70.0f, 70.0f, VariantFraction(Variant, 0x51DE));
	Out.ShellDir = (Aim - From).GetSafeNormal();
	float BestT = TNumericLimits<float>::Max();
	if (Target)
	{
		// cada peça visível que faz sombra (casco, torre, lagartas, rodas, cano), na caixa da própria malha e no espaço
		// dela: o 1º toque do raio do atirador. As placas da poeira das lagartas e a sombra de contato não fazem sombra
		const FVector RayEnd = Aim + (Aim - From).GetSafeNormal() * 300.0;
		for (UActorComponent* Comp : Target->GetComponents())
		{
			const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Comp);
			if (!Mesh || !Mesh->IsVisible() || !Mesh->GetStaticMesh() || !Mesh->CastShadow)
			{
				continue;
			}
			if (const UMaterialInterface* M = Mesh->GetMaterial(0); M && M->GetBlendMode() != BLEND_Opaque && M->GetBlendMode() != BLEND_Masked)
			{
				continue;
			}
			const FTransform Cx = Mesh->GetComponentTransform();
			const FBox Box = Mesh->GetStaticMesh()->GetBoundingBox();
			if (Box.GetSize().Z * Cx.GetScale3D().Z < 30.0)
			{
				continue;
			}
			FVector HitL, NormalL;
			float Time = 0.0f;
			if (FMath::LineExtentBoxIntersection(Box, Cx.InverseTransformPosition(From), Cx.InverseTransformPosition(RayEnd), FVector::ZeroVector, HitL, NormalL, Time)
				&& Time < BestT)
			{
				BestT = Time;
				Out.Point = Cx.TransformPosition(HitL);
				Out.Normal = Cx.TransformVectorNoScale(NormalL).GetSafeNormal();
				Out.Part = Cast<UStaticMeshComponent>(Comp);
				Out.LocalCm = HitL;
			}
		}
		static const bool bLog = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxLog="), V); return V != 0; }();
		if (bLog)
		{
			UE_LOG(LogTemp, Display, TEXT("BlastFx acerto: alvo %s em %s; de %s pra %s; achou %d em %s (normal %s)"), *Target->GetName(),
				*Target->GetActorLocation().ToString(), *From.ToString(), *Aim.ToString(), BestT < 2.0f ? 1 : 0, *Out.Point.ToString(), *Out.Normal.ToString());
		}
	}
	return BestT < 2.0f;
}

void UBlastFxDirector::SpawnArmorImpact(const FShotPoint& Shot, AActor* Target, float GroundZCm, bool bKill)
{
	if (Shot.bRicochet && !bKill && Target)
	{
		SpawnRicochet(Shot, Target, GroundZCm);   // (30/10) o tiro que ricocheteou tem a cara dele
		return;
	}
	// (30/10) a mini explosão: a bola da carga no quadro das faíscas (no tiro que mata, a morte traz a dela e tira esta)
	const bool bBall = ShellBallKg > 0.0f && !bKill && Target;
	if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Fx->GlowScale = ImpactGlow;
		Fx->bBallFollows = bBall;
		Fx->StartImpact(Shot.Point, Shot.Normal, Shot.ShellDir, int32(Shot.Variant * 2654435761u >> 1));
	}
	MarkHull(Shot.Part.Get(), Shot.LocalCm, 0);
	if (bBall)
	{
		// (30/10, o Felipe: "não quero bolinha de fogo assim mal feita"; docs/ESTUDO_ACERTO_TANQUE.md §2.2 e §6.2) a flecha não tem
		// explosivo: no lugar da bola bege de 2,5 m, a língua de chama de 0,5–1 m que sai do furo (a bola da carga pequena, rts.ShellBlastKg
		// 0,15) e a fumaça escura saindo dele por uns segundos (q2–q90: 0,3 → 1,2 m, em volume, na cor da fuligem)
		ShellBall(Shot.Point, Shot.Normal, Target, GroundZCm, ShellBallKg, Shot.Variant);
		SpawnDust(Shot.Point + Shot.Normal * 30.0, HitPuffScale * 0.6f, HitPuffTime * 3.0f, 1.0f, 0, Shot.Variant ^ 0x51ADu,
			FVector4f(0.07f, 0.065f, 0.06f, 0.75f));
		return;
	}
	// a areia que a onda de choque levanta em volta do tanque (no tiro que mata, a morte já traz a dela)
	if (!bKill)
	{
		// (25/10, o impacto pelo chão) o chão embaixo do tanque: o que ele tem pra soltar (no concreto, nada; na neve, o pó)
		float DustO = HitDustOpacity;
		FVector4f DustC(-1.0f, 0.0f, 0.0f, 0.0f);
		const FVector DustAt(Shot.Point.X, Shot.Point.Y, GroundZCm);
		if (GroundDustAt(DustAt, DustO, DustC))
		{
			SpawnDust(DustAt, HitDustScale, HitDustTime, DustO, 1, Shot.Variant, DustC);
		}
		// a baforada cinza no ponto do acerto (0,25–0,4 do tanque, alfa 0,3–0,5): a primeira a cair quando falta orçamento
		SpawnDust(Shot.Point - FVector(0.0, 0.0, 60.0), HitPuffScale, HitPuffTime, HitPuffOpacity, 0, Shot.Variant ^ 0x9E37u, HitPuffColor);
	}
}

void UBlastFxDirector::WeldSpark(const FVector& PointCm, const FVector& OutDir, uint32 Seed)
{
	// (30/10, a obra com vida) a solda: o arco é um clarão branco-azulado pequeno com a luz (que pisca na estrutura) e as faíscas que caem
	// da junta, como o FX de construção do ZH; o StartImpact na escala da bala, apontado pra fora e pra baixo
	if (!ShotFxEnabled() || !GetWorld() || !ShotNearView(*this, PointCm, 600.0f))
	{
		return;
	}
	if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Fx->GlowScale = ImpactGlow * 1.5f;   // (2ª volta, obra_v1: na escala da bala era um lampejo de 1 px; o arco da solda é muito claro)
		Fx->ImpactK = 0.4f;
		Fx->StartImpact(PointCm, (OutDir + FVector(0.0, 0.0, -0.6)).GetSafeNormal(), -OutDir, int32(Seed >> 1));
		Fx->SetLifeSpan(1.6f);
	}
}

void UBlastFxDirector::DozerDirtFall(const FVector& LipCm, const FVector& Fwd, float HalfWidthCm, uint32 Variant)
{
	if (!ShotFxEnabled() || !GetWorld() || !ShotNearView(*this, LipCm, 600.0f))
	{
		return;
	}
	Variant ^= (++ShotSerial) * 0x9E3779B9u;
	if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Fx->ClodTint = FLinearColor(0.30f, 0.23f, 0.15f, 1.0f);   // o torrão do tiro na areia (BulletGroundHit)
		Fx->StartDozerDirt(LipCm, Fwd, HalfWidthCm, int32(Variant >> 1));
	}
	// a areia que a terra levanta ao cair (a poeira da bala no chão, BulletGroundHit, no tamanho da largura da lâmina), ~0,8 m na frente
	// da borda, fora da sombra dela pra câmera de cima; só com sobra no orçamento da poeira
	Dusts.RemoveAll([](const FDust& D) { return !D.E.IsValid(); });
	if (Dusts.Num() < MaxDustsHigh / 2)
	{
		FVector At = LipCm + Fwd.GetSafeNormal2D() * 80.0f;
		At.Z = BlastFxGround::HeightAtCached(GetWorld(), At.X, At.Y);
		SpawnDust(At, HitDustScale * 0.5f, HitDustTime, HitDustOpacity * 0.9f, 0, Variant, FVector4f(0.56f, 0.44f, 0.31f, 0.5f));
	}
}

void UBlastFxDirector::SpawnRicochet(const FShotPoint& Shot, AActor* Target, float GroundZCm)
{
	// (30/10, o ricochete decidido pelo Felipe: 25% do dano e "o jogo DAR A ENTENDER"; docs/ESTUDO_ACERTO_TANQUE.md §6.4) o estalo menor
	// (1 quadro), o leque raso de faíscas colado na chapa na direção do tiro, o traçante que bate e sai torto 8–18° acima da face e vai
	// embora, um fiapo de pó e a marca; sem bola, sem fumaça densa
	(void)Target;
	(void)GroundZCm;
	const FVector N = Shot.Normal.GetSafeNormal();
	const FVector D = Shot.ShellDir.GetSafeNormal();
	if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Fx->GlowScale = ImpactGlow;
		Fx->bBallFollows = true;   // o estalo de 1 quadro, sem o brilho que fica
		Fx->ImpactK = 0.6f;        // menor que o acerto que entra (o estalo de 0,8–1,2 m)
		Fx->bGrazing = true;       // o leque raso e rápido, rente à chapa
		Fx->StartImpact(Shot.Point, N, D, int32(Shot.Variant * 2654435761u >> 1));
	}
	MarkHull(Shot.Part.Get(), Shot.LocalCm, 0);
	// o traçante que sai: no rumo refletido na chapa, levantado 8–18°, por 60–120 m (some no ar, longe)
	const FVector R = (D - 2.0f * FVector::DotProduct(D, N) * N).GetSafeNormal();
	FVector Flat(R.X, R.Y, 0.0);
	Flat = Flat.SizeSquared() > 0.01 ? Flat.GetSafeNormal() : FVector(D.X, D.Y, 0.0).GetSafeNormal();
	const float Up = FMath::DegreesToRadians(8.0f + 10.0f * VariantFraction(Shot.Variant, 0x2C1));
	const FVector Out = (Flat * FMath::Cos(Up) + FVector::UpVector * FMath::Sin(Up)).GetSafeNormal();
	const float Reach = 6000.0f + 6000.0f * VariantFraction(Shot.Variant, 0x7E3);
	if (ABlastDebris* T = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		T->GlowScale = TracerGlow;
		T->StartTracer(Shot.Point + Out * 40.0, Shot.Point + Out * Reach, 9000.0f, int32(Shot.Variant * 3266489917u >> 1));
	}
	// o fiapo de pó de tinta (0,5–1 m, ~1 s)
	SpawnDust(Shot.Point - FVector(0.0, 0.0, 30.0), HitPuffScale * 0.35f, HitPuffTime * 0.8f, HitPuffOpacity * 0.5f, 0, Shot.Variant ^ 0x9E37u, HitPuffColor);
}

void UBlastFxDirector::GroundHit(const FVector& AtCm, const FVector& FromCm, uint32 Variant)
{
	if (!ShotFxEnabled() || !GetWorld())
	{
		return;
	}
	Variant ^= (++ShotSerial) * 0x9E3779B9u;
	// o tiro na areia (a arte 4.1; o estudo com fontes: docs/TIRO_E_ACERTO.md, "O tiro na areia"). O anel de areia: a
	// onda de choque levanta a poeira da superfície até onde ela ainda passa de Mach ~1,1 (Ugarte, Houim & Oran 2017),
	// ~7–10 m pra 1–2,5 kg de TNT (Kingery-Bulmash), quase de uma vez, e a poeira fica pairando: a saia de areia
	// simétrica do EmberGen (eg_poeira3_96, 28,5 m) a 0,6 (o anel até ~8,5 m), com o relógio em dois andamentos: o
	// golpe nos primeiros 3,6 s da gravação em ~0,3 s, e o resto pairando ~1,4 s
	// (v7, 16/10) o anel simulado no Mantaflow (BlastLab/Tools/blender_pyro_ground.py, sim_chao_anel): a areia de perto
	// varrida pelo gás do buraco na horizontal rola num anel com o meio limpo até ~7 m em ~0,5 s, a de longe levantada no
	// lugar pelo choque, baixa (2–4 m) e assentando; em metros reais e no tempo real (as versões de antes esticavam
	// gravações feitas pra outra coisa: nascia pronta, domo cheio, bola marrom). A terra escura vai pelo leque balístico
	// (17/10, a escolha do Felipe no A/B v7 × v8: "a primeira tá boa pra um primeiro tiro num lugar limpo, a segunda pra um
	// tiro onde já tem uma cratera, porque não tem mais areia ali"; "reduzir um pouco ambos, uma capadinha no final") o
	// tiro lembra onde caíram os anteriores: a até 2,5 m de uma cratera, a terra escura sobe (a v8: o anel e a cortina); em
	// areia limpa, só o anel (a v7). As duas a 0,85 do tamanho. -GroundHitV=7 ou 8 força uma delas
	static const int32 Ver = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("GroundHitV="), V); return V; }();
	bool bCrater = Ver >= 8;
	if (Ver == 0)
	{
		const FVector2D P(AtCm.X, AtCm.Y);
		bCrater = GroundCraters.ContainsByPredicate([&P](const FVector2D& C) { return FVector2D::DistSquared(C, P) < 250.0 * 250.0; });
		GroundCraters.Add(P);
		if (GroundCraters.Num() > 512)
		{
			GroundCraters.RemoveAt(0, GroundCraters.Num() - 512);
		}
	}
	if (!ShotNearView(*this, AtCm, 1500.0f))
	{
		return;                                     // fora da vista (a cratera de cima já ficou lembrada pro próximo tiro)
	}
	// (24/10, o laboratório) o tipo de chão do ponto (RTSGroundTypes): a areia segue o caminho de sempre, exato
	{
		FGroundResponse Typed;
		if (GroundResponseOf && GroundResponseOf(AtCm, Typed) && Typed.Weight >= 0.5f && Typed.Kind != 0)
		{
			GroundHitTyped(AtCm, FromCm, Variant, bCrater, Typed);
			return;
		}
	}
	const UBlastRaymarchSubsystem* Rm = GetWorld()->GetSubsystem<UBlastRaymarchSubsystem>();
	if ((Ver == 0 || Ver >= 7) && Rm && Rm->SourceDuration(FName(GroundSimSource)) > 0.0f)
	{
		SpawnDust(AtCm, GroundSimScale * GroundHitSize, GroundSimTime * GroundHitSize, GroundSimOpacity, 1, Variant, FVector4f(-1.0f, 0.0f, 0.0f, 0.0f),
			GroundSimSource, GroundSimThickness / GroundHitSize);
		if (bCrater && Rm->SourceDuration(FName(GroundCurtainSource)) > 0.0f)
		{
			SpawnDust(AtCm, GroundHitSize, GroundHitSize, GroundCurtainOpacity, 1, Variant ^ 0xC07u, GroundCurtainDirt, GroundCurtainSource,
				GroundCurtainThickness / GroundHitSize, GroundCurtainDirt);
		}
	}
	else
	{
		if (AExplosionVolume* Ring = SpawnDust(AtCm, GroundRingScale, GroundRingTime, GroundRingOpacity, 1, Variant, FVector4f(-1.0f, 0.0f, 0.0f, 0.0f),
			TEXT("eg_poeira3_96"), GroundRingThickness))
		{
			Ring->SlowAfterS = GroundRingSlowAfterS;
			Ring->SlowTimeScale = GroundRingSlowTime;
		}
		// o jato de terra escura da v6: a explosão no chão gravada da JangaFX (jg_ground_96) pequena, tingida de terra escura
		if (AExplosionVolume* Jet = SpawnDust(AtCm, GroundJetScale, GroundJetTime, GroundJetOpacity, 1, Variant ^ 0x7E7u, GroundJetDirt,
			TEXT("jg_ground_96"), 1.0f / GroundJetScale, GroundJetDirt))
		{
			Jet->bFlash = false;
			Jet->RaymarchEmit = 0.0f;
		}
	}
	// o clarão, as faíscas, a luz e os torrões
	if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Fx->GlowScale = GroundImpactGlow;
		Fx->TrailTint = FLinearColor(1.0f, 0.82f, 0.62f, 1.0f);   // × o albedo 0,18 do rastro: terra marrom-escura
		Fx->StartGroundImpact(AtCm, (AtCm - FromCm).GetSafeNormal2D(), int32(Variant * 3266489917u >> 1));
	}
	// (30/10) o foguinho: na areia pura, pouco e curto (o resto da carga e os pedaços quentes; nada pra pegar fogo em volta): ~0,5 m de
	// chama, 2–3,5 s, em 2 de cada 3 tiros
	if (GroundFireScale > 0.0f && VariantFraction(Variant, 0xF0C2) < 0.67f)
	{
		StartGroundFire(AtCm, FromCm, Variant, GroundFireScale, FMath::Lerp(2.0f, 3.5f, VariantFraction(Variant, 0xF0C3)));
	}
}

bool UBlastFxDirector::BulletHit(const FVector& PointCm, const FVector& FromCm, AActor* Victim, float GroundZCm, uint32 Variant, FVector& OutPointCm, FVector& OutNormal)
{
	OutPointCm = PointCm;
	OutNormal = (FromCm - PointCm).GetSafeNormal();
	if (!ShotFxEnabled() || !GetWorld())
	{
		return false;
	}
	Variant ^= (++ShotSerial) * 0x9E3779B9u;
	// o ponto na chapa virada pro atirador (a mesma conta do tiro de canhão): o leque das faíscas sai pelo ricochete nela
	FShotPoint Shot;
	if (Victim && ArmorPoint(Victim, FromCm, PointCm, GroundZCm, Variant, Shot))
	{
		OutPointCm = Shot.Point;
		OutNormal = Shot.Normal;
	}
	if (!ShotNearView(*this, OutPointCm, 600.0f))
	{
		return false;
	}
	// (docs/ESTUDO_IMPACTOS.md B) o acerto do tiro de tanque em miniatura: o clarão branco-amarelo de ~0,3 m por 2–3 quadros, ~20 faíscas
	// curtas no leque do ricochete, umas fagulhas e a luz de ~4 m
	if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Fx->GlowScale = ImpactGlow;
		Fx->ImpactK = 0.3f;   // (2ª volta: a 0,18 sumia a 28 m) o clarão de ~0,4 m e ~33 faíscas a 8–19 m/s
		// (30/10, o Felipe: "no casco aparece uma estrelinha branca de desenho animado") sem o brilho de 0,3 s que fica: o clarão no
		// metal é de 1 quadro (o estudo B: 0,3–0,6 m por 2–3 quadros) e o que lê são as faíscas finas
		Fx->bBallFollows = true;
		Fx->bNoLight = Variant % 3u != 0u;   // (o custo) a luz de ~4 m em 1 bala de cada 3
		Fx->StartImpact(OutPointCm, OutNormal, (OutPointCm - FromCm).GetSafeNormal(), int32(Variant * 2654435761u >> 1));
		Fx->SetLifeSpan(1.2f);   // (o custo, 30/10) as faíscas vivem 0,45 s; com 3 s e ~50 balas/s no casco eram ~150 atores vivos
	}
	// o pó da tinta e do metal (0,3–0,6 m): em 1 de cada 3, e só com sobra no orçamento da poeira (a rajada não tira a do tanque de cena)
	if (Variant % 3u == 0u)
	{
		Dusts.RemoveAll([](const FDust& D) { return !D.E.IsValid(); });
		if (Dusts.Num() < MaxDustsHigh / 3)
		{
			SpawnDust(OutPointCm - FVector(0.0, 0.0, 40.0), HitPuffScale * 0.3f, HitPuffTime * 0.6f, HitPuffOpacity * 0.6f, 0, Variant ^ 0x9E37u, HitPuffColor);
		}
	}
	return true;
}

void UBlastFxDirector::BulletGroundHit(const FVector& AtCm, const FVector& FromCm, uint32 Variant, bool bPuff)
{
	if (!ShotFxEnabled() || !GetWorld())
	{
		return;
	}
	Variant ^= (++ShotSerial) * 0x9E3779B9u;
	if (!ShotNearView(*this, AtCm, 600.0f))
	{
		return;
	}
	// (docs/ESTUDO_IMPACTOS.md A) o esguicho: os jatos de terra do tiro na areia a ~1/7 (a cortina a 55–65° com 1,5–3 m de alcance), sem o
	// clarão, as faíscas e a luz (a bala comum não acende) e sem cratera; os grãos voltam pro chão em ~0,6 s
	if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		// (30/10, 2ª volta: a 0,14 o esguicho sumia a 28 m) os riscos e os torrões a 0,3 do tiro de tanque, a 5–8,4 m/s a 60°: até ~2,7 m
		Fx->DirtK = 0.45f;        // (2ª volta: a 0,3 o jato da bala não lia no zoom do jogo; o estudo A: 1,5–3 m de alcance, 1,5–2,5 m de altura)
		Fx->DirtSpeedK = 0.5f;
		Fx->DirtElevDeg = 60.0f;
		Fx->bDirtOnly = true;
		Fx->SparkK = 0.0f;
		// (30/10, o Felipe: "na areia quase nada acontece, um pontinho escuro") a bala na areia seca levanta areia CLARA, acesa pelo sol,
		// não a terra escura e úmida de baixo que o tiro de tanque cava: os riscos × o albedo 0,18 do rastro dão ~0,45/0,36/0,25 (a areia
		// do mapa, 0,49/0,40/0,27) e os grãos na mesma cor
		// (2ª volta, mg_v1: o grão claro na areia clara sumia de cima) o que lê é o contraste: a areia revirada de logo abaixo da
		// superfície, mais escura que ela (o estudo A: "tom de areia mais escuro que o chão"), e o véu claro no campo de poeira
		Fx->TrailTint = FLinearColor(1.7f, 1.3f, 0.85f, 1.0f);
		Fx->ClodTint = FLinearColor(0.30f, 0.23f, 0.15f, 1.0f);
		if (BulletSandVersion() >= 2)
		{
			// (30/10, item 29, o Felipe: "parece um monte de pixelado saindo, sem textura, sem vivência"; docs/PENDENCIAS_NOITE.md, o estudo
			// do impacto de bala na areia) os torrões de 5–20 cm eram cubos escuros de 1–6 px: saem. A cortina é de grão fino (Marston, Li e
			// Thoroddsen 2012: a folha de grãos e os finos levados pelo ar), então mais riscos, mais finos, no mesmo leque e alcance
			Fx->bNoClods = true;
			Fx->SprayCountK = 1.8f;
			Fx->SprayWidthK = 0.6f;
		}
		Fx->StartGroundImpact(AtCm, (AtCm - FromCm).GetSafeNormal2D(), int32(Variant * 3266489917u >> 1));
		Fx->SetLifeSpan(1.2f);   // (o custo) os grãos voltam pro chão em ~0,6 s; os 12 s do tiro de tanque deixavam centenas de atores vivos (30/10: 3 → 1,2 s)
	}
	// a baforada baixa de areia em volume (o estudo: 2–2,5 m, 1,5–3 s, rala; a escala 0,12 da areia em volta do tanque, que a 0,28 cobre
	// ~5,5 m): quem chama manda 1 tiro em cada 2, e só com sobra no orçamento (a metade dele fica pras outras)
	if (bPuff)
	{
		Dusts.RemoveAll([](const FDust& D) { return !D.E.IsValid(); });
		if (Dusts.Num() < MaxDustsHigh / 2)
		{
			// (3ª volta: a areia na cor do chão sumia vista de cima) a areia revirada, mais escura que a da superfície (o estudo: "tom de areia
			// mais escuro que o chão"; entre o anel da morte, 0,78/0,62/0,45, e a terra do jato, 0,26/0,20/0,14), e mais densa
			SpawnDust(AtCm, HitDustScale * 0.43f, HitDustTime * 0.85f, HitDustOpacity * 0.9f, 0, Variant, FVector4f(0.56f, 0.44f, 0.31f, 0.5f));
		}
	}
}

void UBlastFxDirector::ShellBlast(const FVector& PointCm, const FVector& FromCm, AActor* Victim, float GroundZCm, float Kg, uint32 Variant)
{
	if (!ShotFxEnabled() || !GetWorld() || Kg <= 0.0f || WarheadKnobs().Scale <= 0.0f || !LoadAssets())
	{
		return;
	}
	Variant = Variant * 2654435761u ^ (++ShotSerial) * 0x9E3779B9u;
	// o ponto e o pé da bola como no MissileHit da ogiva v2: a face do alvo virada pro tiro, e a bola meia bola pra fora dela
	FVector Point = PointCm;
	FVector Out2 = (FromCm - PointCm).GetSafeNormal2D();
	if (Victim)
	{
		FShotPoint Shot;
		if (ArmorPoint(Victim, Out2.IsNearlyZero() ? FromCm : PointCm + Out2 * 3000.0, PointCm, GroundZCm, Variant, Shot))
		{
			Point = Shot.Point;
			const FVector Face2(Shot.Normal.X, Shot.Normal.Y, 0.0);
			if (Face2.SizeSquared() > 0.25)
			{
				Out2 = Face2.GetSafeNormal();
			}
		}
	}
	ShellBall(Point, FVector(Out2.X, Out2.Y, 0.0), Victim, GroundZCm, Kg, Variant);
}

void UBlastFxDirector::ShellBall(const FVector& PointCm, const FVector& Normal, AActor* Victim, float GroundZCm, float Kg, uint32 Variant)
{
	if (!GetWorld() || Kg <= 0.0f || WarheadKnobs().Scale <= 0.0f || !LoadAssets())
	{
		return;
	}
	FVector Out2(Normal.X, Normal.Y, 0.0);
	Out2 = Out2.SizeSquared() > 0.04 ? Out2.GetSafeNormal() : FVector::ForwardVector;
	const float Lambda = WarheadScale(Kg) * WarheadKnobs().Scale;
	const float BallCm = CpFireballM * Lambda * 100.0f;
	// o pé da bola meia bola pra fora da face atingida (como a ogiva v2 do míssil)
	const FVector Foot(PointCm.X + Out2.X * 0.5 * BallCm, PointCm.Y + Out2.Y * 0.5 * BallCm, GroundZCm);
	if (!ShotNearView(*this, Foot, BallCm + 300.0f))
	{
		return;
	}
	const int32 Before = Warheads.Num();
	SpawnWarhead(Foot, Victim, Kg, Variant, false);
	// (30/10, a 3ª volta: medido no vídeo, a bola na emissão da ogiva do míssil passava de 245 em ~8000 px por 7 quadros, o disco
	// branco) a carga do tiro de canhão (o HEAT) gasta a energia no jato; a bola dela é curta e laranja (o estudo C: o clarão branco
	// em 1–2 quadros e a bola laranja que apaga em 0,2–0,3 s). A emissão do corpo × ShellBallEmit; o anel e a fuligem iguais
	constexpr float ShellBallEmit = 0.4f;   // (medido: 1 = 7 quadros brancos; 0,4 = ~3 e depois laranja-marrom; 0,25 perde o laranja e vira poeira bege)
	for (int32 K = Before; K < Warheads.Num(); ++K)
	{
		if (!Warheads[K].bRing)
		{
			if (AExplosionVolume* Body = Warheads[K].E.Get())
			{
				Body->RaymarchEmit *= ShellBallEmit;
				Body->bFlash = false;   // o clarão é o miolo de 1 quadro das faíscas (o mesmo da bola do disparo, 16/10: o da detonação a deixava branca)
			}
		}
	}
}

bool UBlastFxDirector::BuildingAimPoint(AActor* Building, const FVector& FromCm, float GroundZCm, uint32 Shooter, FShotPoint& OutShot)
{
	// (01/11, a avaliação do item 10) cada atirador escolhe um ponto da fachada virada pra ele (o lado de −85% a +85% da meia largura, a
	// altura de 30–75% do prédio) e fica nele 3–6 tiros, com o espalhar de um canhão (±5% de lado, ±4% de altura); depois sorteia outro. Os
	// tiros de um tanque se juntam num lugar (a marca cresce ali e o furo abre ali: UpdateStructureHoles), os de outro, noutro
	if (!Building || !StructureRay)
	{
		return false;
	}
	if (BuildingAims.Num() > 512)
	{
		BuildingAims.Reset();
	}
	const uint32 Serial = ++ShotSerial;
	FBuildingAim& A = BuildingAims.FindOrAdd((uint64(Shooter) << 32) | uint64(Building->GetUniqueID()));
	if (A.Left <= 0)
	{
		A.Side = FMath::Lerp(-0.85f, 0.85f, VariantFraction(Serial, 0xA1D5));
		A.Height = FMath::Lerp(0.30f, 0.75f, VariantFraction(Serial, 0x4E16));
		A.Left = 3 + int32(VariantFraction(Serial, 0x7C0F) * 3.99f);
	}
	--A.Left;
	const float Side = A.Side + 0.10f * (VariantFraction(Serial, 0x51D3) - 0.5f);
	const float Height = A.Height + 0.08f * (VariantFraction(Serial, 0xB11D) - 0.5f);
	FVector C, E;
	Building->GetActorBounds(false, C, E);
	const FVector Dir = (FVector(C.X, C.Y, 0.0) - FVector(FromCm.X, FromCm.Y, 0.0)).GetSafeNormal();
	const FVector Across = FVector::CrossProduct(Dir, FVector::UpVector);
	const double AimZ = GroundZCm + 2.0 * E.Z * Height;
	const FVector AimAt = FVector(C.X, C.Y, AimZ) + Across * (Side * float(FMath::Min(E.X, E.Y)));
	FVector P, N, L;
	UStaticMeshComponent* Part = nullptr;
	for (const double K : { 1.0, 0.6, 0.3, 0.0 })
	{
		const FVector Aim(FMath::Lerp(C.X, AimAt.X, K), FMath::Lerp(C.Y, AimAt.Y, K), AimZ);
		if (StructureRay(Building, FromCm, Aim + (Aim - FromCm).GetSafeNormal() * 4000.0, P, N, Part, L))
		{
			OutShot.Point = P;
			OutShot.Normal = N.IsNearlyZero() ? FVector::UpVector : N;
			OutShot.Part = Part;
			OutShot.LocalCm = L;
			OutShot.GroundZCm = GroundZCm;
			OutShot.Variant = Serial;
			return true;
		}
	}
	return false;
}

void UBlastFxDirector::BuildingHit(const FVector& PointCm, const FVector& FromCm, AActor* Building, float GroundZCm, uint32 Variant)
{
	if (!ShotFxEnabled() || !GetWorld())
	{
		return;
	}
	Variant = Variant * 2654435761u ^ (++ShotSerial) * 0x9E3779B9u;
	FShotPoint Shot;
	FVector Point = PointCm;
	FVector Normal = (FromCm - PointCm).GetSafeNormal2D();
	// (01/11) o tiro que o traçante levou até a fachada (o ponto da mira do atirador): o acerto no mesmo lugar
	bool bQueued = false;
	if (TArray<FShotPoint>* Queue = Building ? BuildingShots.Find(Building) : nullptr)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		while (Queue->Num() > 0 && Now - (*Queue)[0].Born > 3.0)
		{
			Queue->RemoveAt(0);
		}
		if (Queue->Num() > 0)
		{
			Shot = (*Queue)[0];
			Queue->RemoveAt(0);
			Point = Shot.Point;
			Normal = Shot.Normal;
			bQueued = true;
		}
	}
	// (30/10, o Felipe: "muito mal feito"; o tiro entrava pela porta aberta da War Factory e batia no chão de dentro, e a poeira nascia
	// lá dentro) a mira na fachada virada pro tanque: na altura da parede (35–70% do prédio, acima da porta) e de lado (25–45% da
	// largura, pra um lado ou pro outro), não no meio da porta (o ArmorPoint mira na altura do casco de um tanque, 1,5–2,3 m)
	FVector AimAt = PointCm;
	float AimGroundZ = GroundZCm;
	float AimZ = PointCm.Z;
	if (Building && !bQueued)
	{
		FVector C, E;
		Building->GetActorBounds(false, C, E);
		const float Want = 2.0f * float(E.Z) * FMath::Lerp(0.35f, 0.7f, VariantFraction(Variant, 0xB11D));
		AimZ = GroundZCm + Want;
		AimGroundZ = GroundZCm + FMath::Max(0.0f, Want - 0.5f * (HitHeightMinCm + HitHeightMaxCm));
		const FVector Dir = (FVector(C.X, C.Y, 0.0) - FVector(FromCm.X, FromCm.Y, 0.0)).GetSafeNormal();
		const FVector Across = FVector::CrossProduct(Dir, FVector::UpVector);
		const float Side = VariantFraction(Variant, 0x51D3) < 0.5f ? -1.0f : 1.0f;
		// (2ª volta, predio_v2: a 25–45% da meia largura o tiro ainda caía no vão do portão, que ocupa quase a fachada da War Factory) as
		// paredes e as colunas do lado, a 60–90%
		AimAt = FVector(C.X, C.Y, PointCm.Z) + Across * (Side * float(FMath::Max(E.X, E.Y)) * FMath::Lerp(0.6f, 0.9f, VariantFraction(Variant, 0x7AB1)));
	}
	// (30/10, a marca e o pó ficavam no ar: a caixa da malha inteira, com o pátio e a esfera, tem a face metros na frente da parede) o
	// raio do tiro, da boca do canhão pro ponto mirado na altura sorteada e além, no relevo medido do prédio: o 1º toque na parede
	// (ou na marquise, no teto) de verdade
	bool bRay = bQueued;
	if (Building && StructureRay && !bQueued)
	{
		// a mira do lado (60–90% da caixa do ator, que pega a bandeira e o pátio) pode passar fora da lateral: vai pro meio até tocar
		FVector C, E;
		Building->GetActorBounds(false, C, E);
		FVector P = FVector::ZeroVector, N = FVector::UpVector, L = FVector::ZeroVector;
		UStaticMeshComponent* Part = nullptr;
		bool bHit = false;
		for (const double K : { 1.0, 0.6, 0.3, 0.0 })
		{
			const FVector Aim(FMath::Lerp(C.X, AimAt.X, K), FMath::Lerp(C.Y, AimAt.Y, K), AimZ);
			if (StructureRay(Building, FromCm, Aim + (Aim - FromCm).GetSafeNormal() * 4000.0, P, N, Part, L))
			{
				bHit = true;
				break;
			}
		}
		if (bHit)
		{
			Point = Shot.Point = P;
			Normal = Shot.Normal = N;
			Shot.Part = Part;
			Shot.LocalCm = L;
			bRay = true;
		}
	}
	if (!bRay && !bQueued && Building && ArmorPoint(Building, FromCm, AimAt, AimGroundZ, Variant, Shot))
	{
		Point = Shot.Point;
		Normal = Shot.Normal;
	}
	if (Normal.IsNearlyZero())
	{
		Normal = FVector::UpVector;
	}
	if (!ShotNearView(*this, Point, 900.0f))
	{
		return;
	}
	// o clarão do tiro e poucas faíscas (o estudo: 5–10; a 2ª opinião: 0–8 no concreto, o aço da ogiva), bem menores que no casco
	if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Fx->GlowScale = ImpactGlow;
		Fx->ImpactK = 0.25f;        // (30/10: a 0,08 era um ponto de luz) o estalo de ~0,4 m e as poucas faíscas do aço na pedra
		Fx->bBallFollows = true;    // o estalo de 1 quadro, sem o brilho que fica
		Fx->StartImpact(Point, Normal, (Point - FromCm).GetSafeNormal(), int32(Variant * 2654435761u >> 1));
	}
	// os pedaços de reboco e de bloco (15–40, 0,05–0,5 m) que saltam da fachada pra fora e caem no pé dela, na cor do reboco
	const FVector Out2 = FVector(Normal.X, Normal.Y, 0.0).GetSafeNormal();
	if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Fx->DirtK = 0.75f;          // (30/10: a 0,55 eram ~13 pedaços pequenos) ~25 de 9–34 cm (o estudo C: 15–40 de 0,05–0,5 m)
		Fx->DirtSpeedK = 0.55f;
		Fx->DirtElevDeg = 25.0f;
		Fx->bDirtOnly = true;
		Fx->SparkK = 0.0f;
		Fx->ClodTint = FLinearColor(0.42f, 0.38f, 0.32f, 1.0f);
		Fx->TrailTint = FLinearColor(2.4f, 2.2f, 1.95f, 1.0f);   // × o albedo 0,18 do rastro: o pó do reboco, claro
		Fx->StartGroundImpact(Point + Out2 * 40.0, Out2.IsNearlyZero() ? FVector::ForwardVector : Out2, int32(Variant * 3266489917u >> 1));
	}
	// a poeira de concreto: a cortina que desce pela fachada e fica no pé dela (5–8 m, 8–15 s), em volume, cinza-bege (o reboco
	// moído, não a areia; a 2ª opinião: concreto 0,62/0,60/0,57, cinza-claro); a saia no pé da parede, a 0,8 m dela, no chão
	const FVector Foot(Point.X + Out2.X * 80.0, Point.Y + Out2.Y * 80.0, GroundZCm);
	SpawnDust(Foot, HitDustScale * 1.25f, HitDustTime * 2.6f, HitDustOpacity * 0.85f, 1, Variant ^ 0xC0C0u, FVector4f(0.62f, 0.60f, 0.57f, 0.5f));
	// (30/10) a nuvem de reboco que estoura da parede na altura do acerto (2–3 m, clara) e fica descendo e se desfazendo: diz "onde" de cima
	SpawnDust(Point + Out2 * 70.0 - FVector(0.0, 0.0, 80.0), HitDustScale * 0.55f, HitDustTime * 2.2f, HitDustOpacity, 1, Variant ^ 0x9B1Du,
		FVector4f(0.70f, 0.66f, 0.60f, 0.5f));
	if (Shot.Part.IsValid())
	{
		// (30/10) a marca na fachada: o tipo 2 (o concreto: a fuligem de ~1,4 m e a lasca clara; o M_Structure lê, import_provisional.py)
		MarkHull(Shot.Part.Get(), Shot.LocalCm, 2);
	}
}

void UBlastFxDirector::GroundHitTyped(const FVector& AtCm, const FVector& FromCm, uint32 Variant, bool bCrater, const FGroundResponse& R, const FTypedScale* Scale,
	bool* bOutLinger)
{
	// (24/10, o laboratório; docs/TIPOS_DE_CHAO.md) as mesmas peças do tiro na areia aprovado (o anel simulado, a cortina de terra,
	// o clarão, as faíscas, o jato e os torrões), cada uma pelo que o chão tem pra soltar:
	//  - duro (o asfalto): não há areia solta pra onda varrer; sobe o pó do agregado moído e a fumaça do explosivo, cinza, baixo e
	//    curto; a cratera é rasa (o pavimento quebra em placas: as lascas); o estilhaço de aço que bate na pedra solta mais faísca;
	//  - terra e grama: a terra coesa não rola no anel como a areia (a de perto não "escorre"): sobe a cortina escura de terra
	//    revirada, o que o tiro na areia já faz onde caiu outro antes (a v8 que o Felipe escolheu pra "onde não tem mais areia");
	//  - neve: o pó de neve é leve (100–300 kg/m³ contra ~1600 da areia): a onda o joga mais largo e ele paira mais; por baixo a
	//    terra escura vem na cortina, mais rala;
	//  - barro: o molhado não vira poeira (a coesão): só a cortina escura e pesada e os torrões.
	const UBlastRaymarchSubsystem* Rm = GetWorld()->GetSubsystem<UBlastRaymarchSubsystem>();
	const bool bRing = Rm && Rm->SourceDuration(FName(GroundSimSource)) > 0.0f;
	const bool bCurtain = Rm && Rm->SourceDuration(FName(GroundCurtainSource)) > 0.0f;
	const float K = FMath::Clamp(R.DustK, 0.0f, 2.0f);
	const FVector4f Dust = R.Dust.R >= 0.0f ? FVector4f(R.Dust.R, R.Dust.G, R.Dust.B, R.Dust.A > 0.0f ? R.Dust.A : 0.6f) : FVector4f(-1.0f, 0.0f, 0.0f, 0.0f);
	// a cor da cortina: a terra de hoje (0,22/0,17/0,12) no tom do rastro do tipo; na neve, a terra de baixo misturada com a neve;
	// no barro, a terra molhada
	FVector4f Earth(R.Trail.R * 0.22f, R.Trail.G * 0.207f, R.Trail.B * 0.194f, 0.3f);
	if (R.Kind == 3)
	{
		Earth = FVector4f(0.32f, 0.29f, 0.27f, 0.3f);
	}
	else if (R.Kind == 4)
	{
		Earth = FVector4f(0.12f, 0.10f, 0.08f, 0.3f);
	}
	float DirtK = 1.0f, DirtSpeedK = 1.0f, CurtainO = 0.0f, RingScale = 0.0f, RingO = 0.0f;
	switch (R.Kind)
	{
	case 1:   // duro
		RingScale = 0.55f; RingO = 0.45f; DirtK = 0.5f; break;
	case 2:   // terra
		RingScale = 0.7f; RingO = 0.55f; CurtainO = GroundCurtainOpacity; break;
	case 3:   // neve
		RingScale = 1.15f; RingO = 0.95f; CurtainO = 0.5f; DirtK = 0.8f; break;
	case 4:   // barro
		CurtainO = 0.75f; DirtSpeedK = 0.8f; break;
	default:
		break;
	}
	if (bRing && RingScale > 0.0f && K > 0.0f)
	{
		float S = RingScale * FMath::Sqrt(K) * GroundHitSize;
		if (Scale)
		{
			S *= Scale->SizeK;                      // (25/10) o míssil: o pó que o choque levanta na distância escalada (o tempo ∝ o tamanho)
		}
		if (SpawnDust(AtCm, GroundSimScale * S, GroundSimTime * S, FMath::Min(GroundSimOpacity * RingO * K / 0.75f, 1.0f), 1, Variant, Dust, GroundSimSource, GroundSimThickness / S)
			&& Scale && bOutLinger && (R.Kind == 2 || R.Kind == 3))
		{
			// (25/10, a poeira que fica) o míssil na neve e na terra: o pó do tipo que o choque levanta é também a nuvem que fica (o duro fica
			// como era: no concreto não há nuvem; o barro não solta pó)
			*bOutLinger = MakeLinger(FName(GroundSimSource), LingerRingHold, R.Kind == 3 ? LingerSettleSnow : LingerSettleSand);
		}
	}
	if (bCurtain && CurtainO > 0.0f)
	{
		const float S = GroundHitSize * (R.Kind == 4 ? 0.8f : 1.0f);
		if (Scale)
		{
			// (25/10) o míssil: a cortina na escala da carga (o tamanho ∝ R, o tempo ∝ √R: o regime da gravidade, como a do míssil na areia)
			const float Sm = S * Scale->SizeK;
			SpawnDust(AtCm, Sm, S * FMath::Sqrt(Scale->SizeK), CurtainO, 1, Variant ^ 0xC07u, Earth, GroundCurtainSource, GroundCurtainThickness / Sm, Earth);
		}
		else
		{
			SpawnDust(AtCm, S, S, CurtainO, 1, Variant ^ 0xC07u, Earth, GroundCurtainSource, GroundCurtainThickness / S, Earth);
		}
	}
	const FVector Away = (AtCm - FromCm).GetSafeNormal2D();
	if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Fx->GlowScale = GroundImpactGlow;
		Fx->TrailTint = FLinearColor(R.Trail.R, R.Trail.G, R.Trail.B, 1.0f);
		// (31/10) na areia, o torrão no tom da areia revirada e chamuscada (o albedo ~0,11, a metade da superfície seca; a terra que o
		// Dozer revira é 0,23): o quase preto de antes (0,06) lia como cubo de carvão; os outros chãos, os da tabela
		Fx->ClodTint = R.Kind == 0 ? FLinearColor(0.15f, 0.11f, 0.075f, 1.0f) : R.Clods;
		Fx->SparkK = R.SparkK;
		Fx->DirtK = DirtK;
		Fx->DirtSpeedK = DirtSpeedK;
		if (Scale)
		{
			// (25/10) o míssil: os torrões do chão na escala dele, sem o clarão, as faíscas e a luz do tiro (são do acerto do míssil)
			Fx->DirtK = DirtK * Scale->DirtK;
			Fx->DirtSpeedK = DirtSpeedK * Scale->SpeedK;
			Fx->DirtElevDeg = Scale->ElevDeg;
			Fx->DirtPuffs = Scale->Puffs;
			Fx->bDirtOnly = true;
		}
		Fx->StartGroundImpact(AtCm, Away, int32(Variant * 3266489917u >> 1));
	}
	// (30/10) o foguinho do tiro de tanque (não o do míssil, que tem a bola dele): na terra e no chão duro (a palha, o mato seco e o
	// resto da carga) mais e maior que na areia; na neve e no barro, nada
	if (!Scale && GroundFireScale > 0.0f && (R.Kind == 1 || R.Kind == 2) && VariantFraction(Variant, 0xF0C2) < 0.8f)
	{
		StartGroundFire(AtCm, FromCm, Variant, GroundFireScale * 1.35f, FMath::Lerp(3.0f, 5.5f, VariantFraction(Variant, 0xF0C3)));
	}
	if (R.ChipCount > 0)
	{
		// os pedaços do próprio chão: as lascas do pavimento, os tufos de grama com a terra, os blocos de neve, o barro. (25/10) No
		// míssil, × K² na quantidade (teto 2×, como os torrões), × K no tamanho e as velocidades × √K
		const float Kc = Scale ? FMath::Min(Scale->DirtK, 2.0f) : 1.0f;
		const float Kq = Scale ? FMath::Min(Kc * Kc, 2.0f) : 1.0f;
		const float Kv = Scale ? Scale->SpeedK : 1.0f;
		if (ABlastDebris* Chips = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
		{
			Chips->PlateTint = FLinearColor(R.Chips.R, R.Chips.G, R.Chips.B, 1.0f);
			Chips->PlateRoughNow = R.Kind == 1 ? 0.8f : 0.95f;
			Chips->StartChips(AtCm + FVector(0.0, 0.0, 20.0), Away * 0.4f + FVector::UpVector,
				FMath::Clamp(Scale ? FMath::RoundToInt32(float(R.ChipCount) * Kq) : R.ChipCount, 1, 64),
				(R.Kind == 1 ? 0.03f : 0.05f) * Kc, (R.Kind == 1 ? 0.12f : 0.18f) * Kc, (R.Kind == 4 ? 4.0f : 6.0f) * Kv, (R.Kind == 4 ? 10.0f : 14.0f) * Kv,
				int32(Variant * 2654435761u >> 1));
		}
	}
	(void)bCrater;
}

bool UBlastFxDirector::TypedGround(const FVector& AtCm, FGroundResponse& Out) const
{
	return GroundResponseOf && GroundResponseOf(AtCm, Out) && Out.Weight >= 0.5f && Out.Kind != 0;
}

void UBlastFxDirector::SandAround(const FVector& AtCm, float CoreCm, float AreaCm, float& OutCore, float& OutArea) const
{
	OutCore = OutArea = 1.0f;
	if (!SandShareOf)
	{
		return;
	}
	auto At = [this, &AtCm](double R, double Ang)
	{
		return FMath::Clamp(SandShareOf(AtCm + FVector(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, 0.0)), 0.0f, 1.0f);
	};
	// o miolo: o ponto e 6 a 0,6 do raio dele (a cratera e a raiz da cortina); a área: o miolo e dois anéis de 8 (a 0,55 e a 1,0 do raio,
	// desencontrados). Na areia inteira, 1 exato
	float Core = At(0.0, 0.0);
	for (int32 K = 0; K < 6; ++K)
	{
		Core += At(0.6 * CoreCm, 2.0 * PI * K / 6.0);
	}
	float Area = Core;
	int32 N = 7;
	for (int32 Ring = 0; Ring < 2; ++Ring)
	{
		for (int32 K = 0; K < 8; ++K)
		{
			Area += At((Ring ? 1.0 : 0.55) * AreaCm, 2.0 * PI * (K + 0.5 * Ring) / 8.0);
			++N;
		}
	}
	OutCore = Core / 7.0f;
	OutArea = Area / float(N);
}

bool UBlastFxDirector::GroundDustAt(const FVector& AtCm, float& InOutOpacity, FVector4f& InOutColor) const
{
	FGroundResponse T;
	if (TypedGround(AtCm, T))
	{
		// o tipo: a poeira dele (o pó cinza do concreto, o pó da neve, a terra), no tamanho dele; o barro não solta poeira
		if (T.DustK <= 0.0f)
		{
			return false;
		}
		if (T.Dust.R >= 0.0f)
		{
			InOutColor = FVector4f(T.Dust.R, T.Dust.G, T.Dust.B, T.Dust.A > 0.0f ? T.Dust.A : 0.6f);
		}
		InOutOpacity *= FMath::Min(T.DustK, 1.0f);
		return true;
	}
	const float Share = SandShareOf ? FMath::Clamp(SandShareOf(AtCm), 0.0f, 1.0f) : 1.0f;
	if (Share >= 1.0f)
	{
		return true;                                // a areia: a de hoje, exata
	}
	if (Share < 0.05f)
	{
		return false;
	}
	InOutOpacity *= Share;
	return true;
}

bool UBlastFxDirector::GroundSkirtAt(const FVector& AtCm, FVector4f& Out, bool& bGas) const
{
	// a saia baixa da bola no chão é o que o choque levanta dali. No duro (o concreto, o asfalto) não há o que levantar: o que corre rente
	// é o jato de parede dos produtos da detonação, como no teto do casco (o gás quente refletido na laje, na fuligem da carga); no barro,
	// o mesmo (o molhado não vira poeira); na neve e na terra, a poeira do tipo
	bGas = false;
	FGroundResponse T;
	if (TypedGround(AtCm, T))
	{
		if (T.Kind == 1 || T.DustK <= 0.0f || T.Dust.R < 0.0f)
		{
			bGas = true;
		}
		else
		{
			Out = FVector4f(T.Dust.R, T.Dust.G, T.Dust.B, T.Dust.A > 0.0f ? T.Dust.A : 0.6f);
		}
		return true;
	}
	if (SandShareOf && SandShareOf(AtCm) < 0.5f)
	{
		bGas = true;
		return true;
	}
	return false;
}

void UBlastFxDirector::VehicleDamage(AActor* Unit, float Health01)
{
	if (!ShotFxEnabled() || !Unit || !GetWorld())
	{
		return;
	}
	const int32 Stage = Health01 <= ReallyDamagedHealth ? 2 : (Health01 <= DamagedHealth ? 1 : 0);
	FDamaged* D = Damaged.Find(Unit);
	if (Stage == 0 || (D && Stage <= D->Stage))
	{
		return;                                     // só as passagens (o jogo chama a cada quadro)
	}
	if (!D)
	{
		D = &Damaged.Add(Unit);
	}
	ABlastDebris* Fx = D->Fx.Get();
	if (!Fx)
	{
		Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity);
		if (Fx)
		{
			Fx->GlowScale = MuzzleGlow;             // o jato de chama laranja (a 1 estourava em branco)
		}
		D->Fx = Fx;
	}
	if (!Fx)
	{
		return;
	}
	if (Stage >= 1 && D->Light < 0)
	{
		// o fio claro (o BAR: fumaça abaixo de 65%; o ZH liga a fumaça no "danificado"): fino, sobe ~1 m/s, se desfaz em 3 s
		static const bool bSmokeTest = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxSmokeTest="), V); return V != 0; }();
		D->Light = bSmokeTest ? Fx->StartSmoke(2.0f, 0.5f, 3.0f, 0.1f, 1.0f, 1.1f) : Fx->StartSmoke(0.4f, 0.45f, 3.5f, 0.2f, 0.6f, 1.2f);
		static const bool bSmokeDbg = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxSmokeDbg="), V); return V != 0; }();
		if (bSmokeDbg)
		{
			D->Dbg[0] = Fx->StartSmoke(0.4f, 0.5f, 3.5f, 0.2f, 0.45f, 0.35f);
			D->Dbg[1] = Fx->StartSmoke(0.7f, 0.7f, 2.5f, 0.07f, 0.7f, -1.0f);
			D->Dbg[2] = Fx->StartSmoke(0.4f, 0.5f, 3.5f, 0.2f, 0.45f, 1.2f);
		}
	}
	if (Stage >= 2 && D->Stage < 2)
	{
		// a passagem pra "muito danificado": uma vez o jato de chama pelo anel da torre; depois a fumaça escura e grossa. (31/10, a revisão:
		// "a fumaça do veículo ferido vira uma corda preta rente ao chão", v11_fx_veiculo_ferido 6–8 s) a fumaça escura não é mais a fita
		// (StartSmoke): virada pra câmera, com todos os pontos subindo juntos a 1,5 m/s e a largura só pela idade, ela deitava atrás do
		// veículo andando (a 10–12 m/s, 8 m de caminho por metro de subida) num fio na altura do casco, sem abrir, e a ponta presa ao último
		// ponto ligava a coluna de quando ele estava parado ao veículo (o gancho). Agora é a fuligem em volume do fogo do motor no
		// RTSMissileSmoke (EngineSmokeAt; o jogo solta em UpdateMissileSmoke): sobe pelo empuxo, abre, vai com o vento, faz sombra e é
		// acesa pelas luzes perto
		Fx->StartFlameJet(Unit->GetActorLocation() + FVector(0.0, 0.0, RingUpCm), int32(GetTypeHash(Unit)));
		// (30/10, item 32) o motor pega fogo e fica queimando (o fogo do telhado do prédio, no tamanho de um compartimento de motor: ~1,5 m
		// de chama, o fogo em loop a 0,08 da carcaça da aeronave), andando com o veículo; a luz dele acende o casco e o chão
		const FVector Back = -Unit->GetActorForwardVector().GetSafeNormal2D();
		if (AExplosionVolume* E = FireLoop(Unit->GetActorLocation() + Back * DeckBackCm + FVector(0.0, 0.0, DeckUpCm - 60.0), 0.08f, GetTypeHash(Unit) ^ 0xDEC4u))
		{
			Alive.Remove(E);                       // (quem apaga é o EndDamaged ou a morte: a limpeza do diretor tirava o loop na emenda)
			E->HoldAge = 1.0e6f;
			E->Thickness = 1.6f;                   // (a fumaça grossa batia na borda da caixa da gravação encolhida: o bloco quadrado do foguinho)
			E->RaymarchEmit = 1.1f;
			D->DeckFire = E;
		}
	}
	if (ABlastDebris* Bits = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		// (30/10, item 32) cada passagem solta pedaços: a chapa da saia, a caixa de ferramenta, o para-lama (5–25 cm, escuros de metal
		// pintado e queimado), saindo pra cima e pros lados e caindo em volta
		Bits->PlateTint = FLinearColor(0.16f, 0.15f, 0.13f, 1.0f);
		Bits->PlateRoughNow = 0.7f;
		Bits->StartChips(Unit->GetActorLocation() + FVector(0.0, 0.0, DeckUpCm), FVector::UpVector, Stage >= 2 ? 10 : 5, 0.05f, 0.25f, 2.0f, 4.5f,
			int32(GetTypeHash(Unit) * 2654435761u >> 1) + Stage);
		Bits->SetLifeSpan(15.0f);
	}
	D->Stage = Stage;
}

void UBlastFxDirector::StructureDamage(const AActor* Building, float Health01, const TArray<FVector>& RoofCm)
{
	if (!Building || !GetWorld())
	{
		return;
	}
	// a passagem dos 70% (o DamagedParticleSystem1 = StructureTransitionMediumSmoke do ZH): uma baforada de fumaça cinza em volume do
	// telhado, junto com as placas de queimado do material; consertado acima, pode sair de novo
	if (Health01 > 0.0f && Health01 <= DamagedHealth && !StructDamaged.Contains(Building))
	{
		StructDamaged.Add(Building);
		if (ShotFxEnabled() && RoofCm.Num() && ShotNearView(*this, RoofCm[0], 900.0f))
		{
			SpawnDust(RoofCm[0], HitDustScale * 1.2f, HitDustTime * 2.0f, HitDustOpacity, 1, GetTypeHash(Building) ^ 0x70D4u,
				FVector4f(0.30f, 0.29f, 0.28f, 0.5f));
		}
	}
	else if (Health01 > DamagedHealth)
	{
		StructDamaged.Remove(Building);
	}
	const bool bBurn = Health01 > 0.0f && Health01 <= ReallyDamagedHealth;
	TArray<int32>* Lit = StructFires.Find(Building);
	if (bBurn && !Lit && ShotFxEnabled() && FireVersion() >= 2 && RoofCm.Num())
	{
		// o tamanho do fogo da carcaça (1): o fogo do telhado é o de um cômodo ou de uma máquina do teto, a carga de fogo de um
		// veículo (o ZH também acende fogos pequenos nos ossos, não o prédio inteiro); queima até apagarem (consertou, morreu)
		// a passagem (o TransitionDamageFX do ZH: ReallyDamagedParticleSystem2 = StructureTransitionMediumExplosion): um estouro
		// médio no 1º ponto do telhado, a carga de ~1 kg (o tanque de combustível ou a máquina do teto que pega)
		ShellBall(RoofCm[0], FVector::ZeroVector, nullptr, float(RoofCm[0].Z), 1.0f, GetTypeHash(Building) * 2654435761u);
		TArray<int32>& New = StructFires.Add(Building);
		for (int32 K = 0; K < RoofCm.Num(); ++K)
		{
			// a gravação (BlastLab/Tools/firesim: o anel da torre em cima do casco simulado, 'hull_h' 1,82 m) põe a chama 1,82 m acima do
			// chão que recebe: o "chão" é o telhado menos isso, pra chama sair do telhado
			const int32 Id = StartFire(RoofCm[K] - FVector(0.0, 0.0, FireDeckCm), 1.0f, GetTypeHash(Building) + uint32(K) * 977u, 1800.0f);
			if (Id >= 0)
			{
				New.Add(Id);
			}
		}
	}
	else if (!bBurn && Lit)
	{
		for (const int32 Id : *Lit)
		{
			FireOut(Id, Health01 <= 0.0f ? 0.5f : 4.0f);
		}
		StructFires.Remove(Building);
	}
}

void UBlastFxDirector::StructureBreak(const FVector& AtCm, const FVector& Normal, float RadiusCm, uint32 Variant, float GroundZCm,
	const FLinearColor& Tint)
{
	if (!ShotFxEnabled() || !GetWorld() || !ShotNearView(*this, AtCm, 1200.0f))
	{
		return;
	}
	Variant ^= (++ShotSerial) * 0x9E3779B9u;
	const float K = FMath::Clamp(RadiusCm / 150.0f, 0.5f, 2.5f);
	// o que fez ceder: o estouro de uma carga pequena (a máquina, o tanque de combustível, o depósito; ~0,5 kg × o tamanho do furo)
	ShellBall(AtCm + Normal * 40.0, Normal, nullptr, GroundZCm, 0.5f * K, Variant);
	// os pedaços: o concreto e a chapa do furo, de 15 a 60 cm × o tamanho, saindo devagar pela face (1,5–4 m/s; a gravidade faz o resto)
	// e caindo pelo lado do prédio até o chão, onde ficam
	if (ABlastDebris* Chips = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Chips->PlateTint = Tint;
		Chips->PlateRoughNow = 0.9f;
		const FVector Out = (Normal.GetSafeNormal() * 0.8f + FVector(0.0, 0.0, Normal.Z > 0.7 ? 0.6 : 0.15)).GetSafeNormal();
		Chips->StartChips(AtCm, Out, FMath::Clamp(FMath::RoundToInt32(16.0f * K), 8, 40), 0.15f * K, 0.6f * K, 1.5f, 4.0f, int32(Variant * 2654435761u >> 1));
		Chips->SetLifeSpan(25.0f);
	}
	// o pó do reboco e do concreto moído: a nuvem cinza-clara que desce pela fachada e se espalha no pé do prédio
	SpawnDust(FVector(AtCm.X, AtCm.Y, GroundZCm), HitDustScale * 1.6f * K, HitDustTime * 1.8f, HitDustOpacity, 1, Variant ^ 0xD057u,
		FVector4f(0.52f, 0.50f, 0.47f, 0.5f));
}

bool UBlastFxDirector::EngineSmokeAt(AActor* Unit, FVector& OutCm) const
{
	// (31/10, a revisão) o topo do fogo do motor (o FireLoop do deck, na escala dele) do veículo vivo: a mesma lei da coluna do fogo da
	// aeronave (RTSMatchSubsystem::AddAirSmoke: a eg_fogo_ar_64 desfaz a fuligem a 380 cm na escala 0,17), ~1,8 m acima do pé dele na 0,08
	const FDamaged* D = Unit ? Damaged.Find(Unit) : nullptr;
	const AExplosionVolume* Deck = D && D->Stage >= 2 && D->WreckStart < 0.0 ? D->DeckFire.Get() : nullptr;
	if (!Deck)
	{
		return false;
	}
	OutCm = Deck->GetActorLocation() + FVector(0.0, 0.0, 380.0 * double(Deck->SizeScale) / 0.17);
	return true;
}

void UBlastFxDirector::EndDamaged(AActor* Unit)
{
	if (FDamaged* D = Unit ? Damaged.Find(Unit) : nullptr)
	{
		if (AExplosionVolume* Deck = D->DeckFire.Get())
		{
			Deck->Destroy();                      // (30/10) consertou: o fogo do motor apaga
		}
		if (ABlastDebris* Fx = D->Fx.Get())
		{
			// para de soltar e se desfaz (o rastro vive ~3 s)
			for (const int32 Trail : { D->Light, D->Heavy })
			{
				if (Trail >= 0)
				{
					Fx->SetTrailHead(Trail, Unit->GetActorLocation() + FVector(0.0, 0.0, DeckUpCm), false);
				}
			}
			Fx->SetLifeSpan(5.0f);
		}
		Damaged.Remove(Unit);
	}
}

void UBlastFxDirector::UpdateDamaged()
{
	static const bool bLog = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxLog="), V); return V != 0; }();
	static double LastLog = 0.0;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const bool bLogNow = bLog && Now - LastLog > 1.5;
	if (bLogNow)
	{
		LastLog = Now;
	}
	for (auto It = Damaged.CreateIterator(); It; ++It)
	{
		AActor* Unit = It.Key().Get();
		ABlastDebris* Fx = It.Value().Fx.Get();
		if (!Unit || !Fx)
		{
			// (31/10, a sucata) o fogo que já sai pelo fim simulado (WreckSalvage) segue até o fim sem a carcaça (a fumaça já solta sobe e se
			// desfaz, nunca corta) e a luz dele (o Fx) vive até lá
			const bool bLetEnd = It.Value().bFireLetEnd && It.Value().Fire >= 0 && FireAlive(It.Value().Fire);
			if (Fx)
			{
				Fx->SetLifeSpan(bLetEnd ? FMath::Max(5.0f, float(It.Value().FireEndAt - Now) + 0.5f) : 5.0f);
			}
			// (22/10) a carcaça que sumiu (afundou) leva a coluna em volume junto (antes ela ficava no mapa sem dono)
			if (AExplosionVolume* Col = It.Value().Column.Get())
			{
				Col->Destroy();
			}
			if (It.Value().Fire >= 0 && !bLetEnd)
			{
				FireOut(It.Value().Fire, 0.5f);      // (25/10) o fogo simulado também
			}
			It.RemoveCurrent();
			continue;
		}
		const FVector At = Unit->GetActorLocation();
		const FVector Back = -Unit->GetActorForwardVector().GetSafeNormal2D();
		if (It.Value().WreckStart >= 0.0)
		{
			if (AExplosionVolume* Deck = It.Value().DeckFire.Get())
			{
				Deck->Destroy();                  // (30/10) morreu: o fogo do motor vira o da carcaça (a morte e o fogo simulado)
			}
			// a carcaça: espera a explosão se desfazer, queima ~30 s e se apaga
			const double Now2 = GetWorld()->GetTimeSeconds();
			const bool bOn = Now2 >= It.Value().WreckStart && Now2 < It.Value().WreckUntil;
			// a luz do fogo de baixo, antes da fumaça e das chamas (17/10)
			if (!It.Value().bWreckLightOn && Now2 >= It.Value().WreckLightStart && Now2 < It.Value().WreckUntil)
			{
				It.Value().bWreckLightOn = true;
				Fx->StartWreckFire(float(It.Value().WreckUntil - Now2), false);
				// (25/10, o fogo simulado; docs/FOGO.md) com blast.Fire.Version 2 o fogo de verdade nasce aqui, 0,8 s depois da morte, por
				// baixo da bola dela (a ignição da gravação; um fenômeno só), e queima até o fim da carcaça; a luz desta carcaça segue ele
				if (FireVersion() >= 2 && It.Value().Fire < 0)
				{
					const double Gz = BlastFxGround::HeightAtCached(GetWorld(), At.X, At.Y);
					It.Value().Fire = StartFire(FVector(At.X, At.Y, Gz), It.Value().FireScale, uint32(Unit->GetUniqueID()) * 0x9E3779B9u,
						float(It.Value().WreckUntil - Now2), Fx);
				}
			}
			if (bOn && !It.Value().bWreckLit && It.Value().Fire >= 0 && FireAlive(It.Value().Fire))
			{
				It.Value().bWreckLit = true;          // (25/10) o fogo simulado já é o fogo, a fuligem e a luz da carcaça
			}
			if (bOn && !It.Value().bWreckLit)
			{
				It.Value().bWreckLit = true;
				// (v2, 17/10) a coluna em volume; sem a gravação (ou -WreckV=1), a fumaça em fita e as chamas de antes
				static const int32 WreckVer = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("WreckV="), V); return V; }();   // (a v2 só com -WreckV=2 até o visual ficar certo)
				const UBlastRaymarchSubsystem* Rm = GetWorld()->GetSubsystem<UBlastRaymarchSubsystem>();
				AExplosionVolume* Col = nullptr;
				if (WreckVer >= 2 && Rm && Rm->SourceDuration(FName(WreckColumnSource)) > 0.0f)
				{
					const double Gz = BlastFxGround::HeightAtCached(GetWorld(), At.X, At.Y);
					Col = SpawnDust(FVector(At.X, At.Y, Gz), 1.0f, 1.0f, 0.0f, 1, uint32(Unit->GetUniqueID()) * 0x9E3779B9u, FVector4f(-1.0f, 0.0f, 0.0f, 0.0f),
						WreckColumnSource, WreckColumnThickness, WreckColumnSoot);
				}
				if (Col)
				{
					// fora da lista das areias (ela some no fim da gravação, e esta é em loop): a vida e a entrada/saída são daqui
					Dusts.RemoveAll([Col](const FDust& D) { return D.E.Get() == Col; });
					Col->HoldAge = float(It.Value().WreckUntil - Now2) + 0.5f;
					Col->RaymarchEmit = WreckColumnEmit;
					Col->RaymarchYawRad = WindYawRad;
					Col->Seed = float(Unit->GetUniqueID() % 97u) * 0.37f;
					It.Value().Column = Col;
					if (!It.Value().bWreckLightOn)
					{
						Fx->StartWreckFire(float(It.Value().WreckUntil - Now2), false);
					}
				}
				else
				{
					It.Value().Heavy = Fx->StartSmoke(0.7f, 0.7f, 5.0f, 0.06f, 0.85f, 1.6f);
					if (It.Value().bWreckLightOn)
					{
						Fx->EnableWreckFlames();
					}
					else
					{
						Fx->StartWreckFire(float(It.Value().WreckUntil - Now2));
					}
				}
			}
			if (AExplosionVolume* Col = It.Value().Column.Get())
			{
				const float In = FMath::Clamp(float(Now2 - It.Value().WreckStart) / WreckColumnInS, 0.0f, 1.0f);
				const float Out = FMath::Clamp(float(It.Value().WreckUntil - Now2) / WreckColumnOutS, 0.0f, 1.0f);
				Col->Opacity = WreckColumnOpacity * FMath::SmoothStep(0.0f, 1.0f, In) * FMath::SmoothStep(0.0f, 1.0f, Out);
				// (25/10, o vento ao vivo) a coluna vira pro vento do mapa quando ele muda (WreckColumnTurnTauS)
				const float Turn = GetWorld() ? 1.0f - FMath::Exp(-GetWorld()->GetDeltaSeconds() / WreckColumnTurnTauS) : 0.0f;
				Col->RaymarchYawRad += FMath::FindDeltaAngleRadians(Col->RaymarchYawRad, WindYawRad) * Turn;
			}
			if (It.Value().Heavy >= 0)
			{
				Fx->SetTrailHead(It.Value().Heavy, At + FVector(0.0, 0.0, RingUpCm), bOn);
			}
			Fx->SetFirePos(At + FVector(0.0, 0.0, RingUpCm - 20.0 - It.Value().SinkCm));   // (31/10) a desmanchada: desce com o casco
			if (It.Value().Fire >= 0)
			{
				// (25/10) a carcaça com física (empurrada, arrastada): o fogo simulado vai junto
				// (31/10, a sucata) e a desmanchada pelo Dozer: desce com o casco (a chama entra na areia; a fumaça já solta segue no ar)
				FireFollow(It.Value().Fire, FVector(At.X, At.Y, BlastFxGround::HeightAtCached(GetWorld(), At.X, At.Y) - double(It.Value().SinkCm)));
			}
			if (Now2 >= It.Value().WreckUntil + 4.0)
			{
				if (AExplosionVolume* Col = It.Value().Column.Get())
				{
					Col->Destroy();
				}
				if (It.Value().Fire >= 0)
				{
					FireOut(It.Value().Fire, 0.5f);
				}
				Fx->SetLifeSpan(1.0f);
				It.RemoveCurrent();
			}
			continue;
		}
		if (It.Value().Light >= 0)
		{
			// (31/10) no fogo do motor (abaixo de 35%) a fita clara para de soltar: a fuligem em volume assume (com o veículo andando ela
			// também deitava na corda)
			Fx->SetTrailHead(It.Value().Light, At + Back * DeckBackCm + FVector(0.0, 0.0, DeckUpCm), It.Value().Stage < 2);
		}
		if (AExplosionVolume* Deck = It.Value().DeckFire.Get())
		{
			const FVector DeckCm = At + Back * DeckBackCm + FVector(0.0, 0.0, DeckUpCm - 60.0);
			Deck->SetActorLocation(DeckCm);
			// (01/11, o Felipe: "a fumaça/fogo ali no tanque danificado [...] é fixa ali, mesmo problema" do Apache caindo) a gravação é a de
			// um fogo parado: andando, a coluna dela ia presa no deck, rígida. Andando, a fuligem da gravação dilui (a chama fica: a diluição
			// não toca o fogo) e a fumaça sai pelos puffs soltos do fogo do motor (EngineSmokeAt), que ficam no ar onde nasceram; parado,
			// volta a coluna da gravação. A velocidade pelo quadro, suavizada em ~0,5 s
			const double NowD = GetWorld()->GetTimeSeconds();
			FDamaged& Dg = It.Value();
			if (Dg.DeckLastS >= 0.0 && NowD > Dg.DeckLastS)
			{
				const float Ms = float(FVector::Dist2D(DeckCm, Dg.DeckLastCm) / 100.0 / (NowD - Dg.DeckLastS));
				// (01/11, a prova v36_fx_veiculo_ferido: com 0,2 a fuligem da gravação, gravada na escala pequena com a espessura 4×, seguia um
				// bolo opaco preso no deck andando junto) com o ar passando a coluna de ar parado da gravação não existe: a fuligem vai toda pro
				// rastro do mundo (EngineSmokeAt); da gravação fica a chama
				const float Want = FMath::Lerp(1.0f, 0.03f, FMath::Clamp(Ms / 1.5f, 0.0f, 1.0f));
				Dg.DeckDilute = FMath::FInterpTo(Dg.DeckDilute, Want, float(NowD - Dg.DeckLastS), 2.0f);
			}
			Dg.DeckLastCm = DeckCm;
			Dg.DeckLastS = NowD;
			Deck->RaymarchDilute = Dg.DeckDilute;
		}
		for (int32 k = 0; k < 3; ++k)
		{
			if (It.Value().Dbg[k] >= 0)
			{
				const double Tn = GetWorld()->GetTimeSeconds() * 3.0;
				const FVector Spin = k == 0 ? FVector::ZeroVector : FVector(FMath::Cos(Tn), FMath::Sin(Tn), 0.0) * 150.0;
				Fx->SetTrailHead(It.Value().Dbg[k], At + FVector::CrossProduct(FVector::UpVector, Back) * (450.0 + 300.0 * k) + Spin + FVector(0.0, 0.0, DeckUpCm), true);
			}
		}
		if (It.Value().Heavy >= 0)
		{
			Fx->SetTrailHead(It.Value().Heavy, At + Back * (0.5f * DeckBackCm) + FVector(0.0, 0.0, RingUpCm), true);
		}
		if (bLogNow)
		{
			UE_LOG(LogTemp, Display, TEXT("BlastFx ferido: %s estágio %d, fumaça clara %d pontos (%d), escura %d pontos (%d), em %s, ator %s tique %d"),
				*Unit->GetName(), It.Value().Stage, Fx->TrailPointCount(It.Value().Light), Fx->TrailEmitting(It.Value().Light) ? 1 : 0,
				Fx->TrailPointCount(It.Value().Heavy), Fx->TrailEmitting(It.Value().Heavy) ? 1 : 0, *At.ToString(), *Fx->GetName(),
				Fx->IsActorTickEnabled() ? 1 : 0);
		}
	}
}

void UBlastFxDirector::SetWind(float YawDeg, float SpeedMs)
{
	// (25/10) ao vivo: quem deriva lê a cada quadro (a nuvem da ogiva, a poeira que fica, a coluna da carcaça, a fumaça dos destroços)
	WindYawRad = FMath::DegreesToRadians(YawDeg);
	WindSpeedMs = FMath::Max(SpeedMs, 0.0f);
}

float UBlastFxDirector::WindProfileK(float ZM)
{
	const float Z = FMath::Clamp(ZM, 2.0f * LingerZ0M, 100.0f);
	return FMath::Loge(Z / LingerZ0M) / FMath::Loge(10.0f / LingerZ0M);
}

int32 UBlastFxDirector::NumLingering() const
{
	int32 N = 0;
	for (const FDust& D : Dusts)
	{
		N += D.bLinger && D.E.IsValid() && D.RetireAt < 0.0f ? 1 : 0;
	}
	return N;
}

bool UBlastFxDirector::MakeLinger(FName Source, float HoldFrac, float SettleMs)
{
	// -DustLingerV=1: a de antes (a gravação esmaece no último quarto e o jogo põe a camada Linger na poeira em volume); -DustLingerHold=
	// × a fração da parada (o A/B da parada)
	static const int32 Ver = FMath::RoundToInt32(CmdFloat(TEXT("DustLingerV="), 2.0f));
	static const float HoldK = CmdFloat(TEXT("DustLingerHold="), 1.0f);
	const int32 Pr = FMath::Clamp(BlastFxHitLife::Preset(), 0, 3);
	if (Ver < 2 || !BlastFxHitLife::On(BlastFxHitLife::Linger) || LingerMaxByPreset[Pr] <= 0 || Dusts.IsEmpty() || !GetWorld())
	{
		return false;
	}
	FDust& D = Dusts.Last();
	AExplosionVolume* E = D.E.Get();
	const UBlastRaymarchSubsystem* Rm = GetWorld()->GetSubsystem<UBlastRaymarchSubsystem>();
	if (!E || !Rm || D.bLinger)
	{
		return false;
	}
	const float HoldS = FMath::Clamp(HoldFrac * HoldK, 0.05f, 0.95f) * D.Duration;
	UBlastRaymarchSubsystem::FDustStats St;
	if (!Rm->DustStatsAt(Source, HoldS, St) || St.R90M <= 0.0f)
	{
		return false;
	}
	// o teto: a mais velha das que ficam sai esmaecendo (a que nasceu agora entra)
	if (NumLingering() >= LingerMaxByPreset[Pr])
	{
		int32 Oldest = INDEX_NONE;
		for (int32 K = 0; K < Dusts.Num() - 1; ++K)
		{
			const AExplosionVolume* V = Dusts[K].E.Get();
			if (Dusts[K].bLinger && Dusts[K].RetireAt < 0.0f && V && (Oldest == INDEX_NONE || V->Age > Dusts[Oldest].E->Age))
			{
				Oldest = K;
			}
		}
		if (Oldest != INDEX_NONE)
		{
			Dusts[Oldest].RetireAt = Dusts[Oldest].E->Age;
		}
	}
	const float Ts = FMath::Max(E->TimeScale > 0.0f ? E->TimeScale : E->SizeScale, 0.01f);
	D.bLinger = true;
	D.HoldBakeS = HoldS;
	D.HoldAge = HoldS * Ts;
	D.LingerLifeS = LingerLifeByPreset[Pr];
	D.Scale0 = E->SizeScale;
	D.Thick0 = E->Thickness;
	// as medidas da gravação na escala dela no mundo: o tamanho × a escala, a velocidade × a escala ÷ o relógio
	D.R0M = FMath::Max(St.R90M * E->SizeScale, 1.0f);
	D.ZcM = FMath::Max(St.ZMeanM * E->SizeScale, 0.3f);
	D.VrMs = FMath::Max(St.VRadialMs, 0.0f) * E->SizeScale / Ts;
	D.SettleMs = SettleMs;
	D.FootBase = E->GetActorLocation();
	D.LastAge = E->Age;
	E->BakeHoldS = HoldS;
	E->HoldAge = D.HoldAge + D.LingerLifeS + LingerRetireS + 0.5f;
	UE_LOG(LogTemp, Display, TEXT("BlastFx a poeira que fica: %s para aos %.2f s de idade (%.2f s da gravação), R0 %.1f m, meio a %.2f m, frente a %.2f m/s, vida %.0f s, queda %.2f m/s"),
		*Source.ToString(), D.HoldAge, HoldS, D.R0M, D.ZcM, D.VrMs, D.LingerLifeS, D.SettleMs);
	return true;
}

void UBlastFxDirector::StartWreck(AActor* Unit)
{
	if (!Unit || !GetWorld())
	{
		return;
	}
	ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity);
	if (!Fx)
	{
		return;
	}
	Fx->GlowScale = MuzzleGlow;                     // as chamas laranja (a 1 estourava em branco)
	FDamaged& D = Damaged.Add(Unit);
	D.Fx = Fx;
	D.Stage = 3;
	D.Light = -1;
	D.Heavy = -1;
	const double Now = GetWorld()->GetTimeSeconds();
	D.WreckStart = Now + WreckDelayS;
	D.WreckUntil = D.WreckStart + WreckFireS;
	static const float LightDelay = [] { float V = WreckLightDelayS; FParse::Value(FCommandLine::Get(), TEXT("WreckLightDelay="), V); return V; }();
	D.WreckLightStart = Now + FMath::Min(LightDelay, WreckDelayS);
}

void UBlastFxDirector::WreckGone(AActor* Unit, float FadeS)
{
	FDamaged* D = Unit ? Damaged.Find(Unit) : nullptr;
	// (31/10) o fogo que já sai pelo fim simulado (WreckSalvage, a sucata) não ganha um prazo depois (o afundar normal no meio do
	// desmanche esmaeceria o volume inteiro, com a fumaça no ar); só o 0 (já) ainda vale
	if (!D || D->WreckStart < 0.0 || !GetWorld() || (D->bFireLetEnd && FadeS > 0.0f))
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const double Until = Now + FMath::Max(FadeS, 0.0f);
	if (D->Fire >= 0)
	{
		FireOut(D->Fire, FMath::Max(FadeS, 0.0f));   // (25/10) o fogo simulado: o fim simulado se couber, senão esmaece
	}
	if (Until >= D->WreckUntil)
	{
		return;   // já se apaga antes disso
	}
	// o fim adiantado: a fumaça em fita para de soltar, a coluna sai pela opacidade (o Out do UpdateDamaged) e a entrada
	// é tirada 4 s depois, como no fim normal; a que ainda não acendeu (a luz aos 0,8 s, a fumaça aos 4 s) não acende mais
	D->WreckUntil = Until;
	if (ABlastDebris* Fx = D->Fx.Get(); Fx && D->bWreckLightOn)
	{
		// a luz e as chamas do ABlastDebris apagam nos últimos 3 s do tempo dado: o mesmo fogo com o fim novo (as chamas só
		// no caminho da fita; com a coluna em volume, ou antes da fumaça, só a luz)
		Fx->StartWreckFire(float(Until - Now), D->bWreckLit && !D->Column.IsValid());
	}
}

void UBlastFxDirector::WreckSalvage(AActor* Unit, float SinkCm)
{
	// (31/10, a revisão de vídeo do fx_sucata: o fogo e a coluna desligavam no quadro em que a carcaça sumia) a carcaça recolhida pelo Dozer
	// apaga como a que apaga sozinha: o fogo sai do laço na primeira saída que vem e toca o fim simulado (a vazão cai, a chama fica
	// intermitente e a fumaça já solta sobe e se desfaz; docs/FOGO.md), em vez do esmaecer de 1 s do volume inteiro (que levava junto a
	// fumaça no ar). Ele desce com o casco (UpdateDamaged) e, se a carcaça some antes do fim, segue sem ela
	FDamaged* D = Unit ? Damaged.Find(Unit) : nullptr;
	if (!D || D->WreckStart < 0.0 || !GetWorld())
	{
		return;
	}
	D->SinkCm = FMath::Max(D->SinkCm, SinkCm);
	if (D->bFireLetEnd)
	{
		return;
	}
	const float LeftS = D->Fire >= 0 ? FireEndSoon(D->Fire) : -1.0f;
	if (LeftS < 0.0f)
	{
		// sem o fogo simulado (blast.Fire.Version 1): as chamas de sprite e a fita apagam no tempo do afundar normal (o WreckFireFadeS do jogo)
		WreckGone(Unit, 3.0f);
		D->bFireLetEnd = true;
		return;
	}
	D->bFireLetEnd = true;
	D->FireEndAt = GetWorld()->GetTimeSeconds() + double(LeftS);
	if (D->FireEndAt < D->WreckUntil)
	{
		// a luz da carcaça (e a coluna da -WreckV=2) acabam junto com o fogo; só a luz: o fogo é o simulado (sem as chamas de sprite)
		D->WreckUntil = D->FireEndAt;
		if (ABlastDebris* Fx = D->Fx.Get(); Fx && D->bWreckLightOn)
		{
			Fx->StartWreckFire(LeftS, false);
		}
	}
}

void UBlastFxDirector::MissileLaunch(int32 Id, const FVector& PosCm, const FVector& Dir, uint32 Variant, float RailTailM)
{
	if (!ShotFxEnabled() || !GetWorld())
	{
		return;
	}
	Variant ^= (++ShotSerial) * 0x9E3779B9u;
	const FVector D = Dir.GetSafeNormal();
	const double Now = GetWorld()->GetTimeSeconds();
	if (ABlastDebris* L = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		L->GlowScale = MuzzleGlow;
		if (RailTailM < 0.0f)
		{
			L->Destroy();   // (01/11) o lançamento suave: nada na boca (a ignição é no ar, MissileIgnite)
		}
		else if (RailTailM > 0.0f)
		{
			L->StartRailLaunch(PosCm - D * (double(RailTailM) * 100.0), D, int32(Variant * 2246822519u >> 1));   // (01/11) do trilho, na tubeira
		}
		else
		{
			L->StartLaunch(PosCm, D, int32(Variant * 2246822519u >> 1));
		}
	}
	if (ABlastDebris* M = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		M->GlowScale = MissileGlow;
		M->TrailTint = FLinearColor(0.94f, 0.94f, 0.95f, 1.0f);   // a fumaça branca do motor (óxido de alumínio)
		M->StartMissile(PosCm, D, int32(Variant * 3266489917u >> 1), !bExternalMissileSmoke);
		M->SetMissileMotor(RailTailM >= 0.0f);   // (01/11) o suave sai do tubo com o motor de voo apagado
		FMissileFx& F = Missiles.FindOrAdd(Id);
		if (ABlastDebris* Old = F.Fx.Get())
		{
			Old->EndMissile();
		}
		F.Fx = M;
		F.Seen = Now;
	}
	// a poeira do lançamento quente (o motor sai aceso da caixa e sopra o chão: a nuvem cor de terra das fotos da Patriot),
	// uma por rajada (no máximo uma a cada 0,6 s), ~3 m atrás da caixa, na cor do chão: o golpe em ~0,25 s e o resto
	// pairando ~1,5 s, abaixo de ~2,5 m. (25/10) Só do chão: lançado do ar (o Hellfire do Apache a ~13 m, o míssil do Raptor) o
	// motor não sopra a areia lá embaixo (os lançadores de chão saem de 2–3 m)
	// (01/11) só a caixa fechada (RailTailM 0) sopra o chão: o suave (RailTailM < 0) tem o motor de ejeção pequeno e o de voo acende
	// longe; o do trilho e o lançador aberto (RailTailM > 0) acendem na tubeira, sem o jato de ré da caixa
	const bool bFromGround = RailTailM == 0.0f && PosCm.Z - BlastFxGround::HeightAtCached(GetWorld(), PosCm.X, PosCm.Y) < 600.0;
	if (bFromGround && Now - LastLaunchDust > 0.6)
	{
		LastLaunchDust = Now;
		const FVector Back = -FVector(D.X, D.Y, 0.0).GetSafeNormal() * 300.0;
		const FVector At(PosCm.X + Back.X, PosCm.Y + Back.Y, BlastFxGround::HeightAtCached(GetWorld(), PosCm.X + Back.X, PosCm.Y + Back.Y));
		if (AExplosionVolume* Dz = SpawnDust(At, LaunchDustScale, LaunchDustTime, LaunchDustOpacity, 0, Variant, LaunchDustColor))
		{
			Dz->SlowAfterS = 0.5f;
			Dz->SlowTimeScale = 0.35f;
		}
	}
}

void UBlastFxDirector::MissileInFlight(int32 Id, const FVector& PosCm, const FVector& Dir, uint32 Variant)
{
	if (!ShotFxEnabled() || !GetWorld())
	{
		return;
	}
	if (const FMissileFx* Have = Missiles.Find(Id); Have && Have->Fx.IsValid())
	{
		return;   // já tem a chama (o lançamento à vista, ou um quadro de antes)
	}
	// (v4, 23/10) o mesmo míssil do MissileLaunch (a chama, a luz e o rastro branco), sem o StartLaunch (o clarão da caixa) e
	// sem a poeira do chão: ele nasce no ar (o ataque de mísseis, 150 m atrás e 45 m acima) ou aparece saindo da névoa
	Variant ^= (++ShotSerial) * 0x9E3779B9u;
	if (ABlastDebris* M = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		M->GlowScale = MissileGlow;
		M->TrailTint = FLinearColor(0.94f, 0.94f, 0.95f, 1.0f);   // a fumaça branca do motor (óxido de alumínio)
		M->StartMissile(PosCm, Dir.GetSafeNormal(), int32(Variant * 3266489917u >> 1), !bExternalMissileSmoke);
		FMissileFx& F = Missiles.FindOrAdd(Id);
		if (ABlastDebris* Old = F.Fx.Get())
		{
			Old->EndMissile();
		}
		F.Fx = M;
		F.Seen = GetWorld()->GetTimeSeconds();
	}
}

void UBlastFxDirector::MissileMotor(int32 Id, bool bOn)
{
	if (FMissileFx* F = Missiles.Find(Id))
	{
		if (ABlastDebris* M = F->Fx.Get())
		{
			M->SetMissileMotor(bOn);
		}
	}
}

void UBlastFxDirector::MissileIgnite(int32 Id, const FVector& NozzleCm, const FVector& Dir)
{
	if (!ShotFxEnabled() || !GetWorld())
	{
		return;
	}
	MissileMotor(Id, true);
	// o clarão pequeno da ignição do motor de voo (o do trilho: 0,16 m, os riscos e a luz curta), na tubeira, já no ar
	if (ABlastDebris* L = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		L->GlowScale = MuzzleGlow;
		L->StartRailLaunch(NozzleCm, Dir.GetSafeNormal(), int32(uint32(Id) * 2246822519u >> 1));
	}
}

void UBlastFxDirector::MissileMove(int32 Id, const FVector& PosCm, const FVector& Dir, float SpeedCmPerS)
{
	if (FMissileFx* F = Missiles.Find(Id))
	{
		if (ABlastDebris* M = F->Fx.Get())
		{
			M->UpdateMissile(PosCm, Dir);
			F->Seen = GetWorld() ? GetWorld()->GetTimeSeconds() : F->Seen;
		}
	}
}

void UBlastFxDirector::MissileBody(int32 Id, UStaticMesh* Mesh, float LengthCm)
{
	static TWeakObjectPtr<UMaterialInterface> Paint;
	if (!Paint.IsValid())
	{
		Paint = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Soldier.M_Soldier"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (FMissileFx* F = Missiles.Find(Id))
	{
		if (ABlastDebris* M = F->Fx.Get())
		{
			M->SetMissileBody(Mesh, Paint.Get(), LengthCm);
		}
	}
}

int32 UBlastFxDirector::MissileBlastVer()
{
	static const int32 Ver = [] { int32 V = 2; FParse::Value(FCommandLine::Get(), TEXT("MissileBlastV="), V); return V; }();
	return Ver;
}

FVector UBlastFxDirector::MissileHit(int32 Id, const FVector& PointCm, const FVector& FromCm, AActor* Victim, float GroundZCm, bool bKill, float WarheadKg,
	bool bClassicLook, const BlastFxHitLife::FHitIn* LifeIn, BlastFxHitLife::FHitOut* LifeOut)
{
	if (FMissileFx* F = Missiles.Find(Id))
	{
		if (ABlastDebris* M = F->Fx.Get())
		{
			M->EndMissile();
		}
		Missiles.Remove(Id);
	}
	if (!ShotFxEnabled() || !GetWorld())
	{
		return PointCm;
	}
	const uint32 Variant = uint32(Id) * 2654435761u ^ (++ShotSerial) * 0x9E3779B9u;
	const double Now = GetWorld()->GetTimeSeconds();
	if (MissileBlastVer() < 2)
	{
		// (-MissileBlastV=1) a de antes, exata: o clarão, as faíscas e a luz no ponto do jogo, a marca e a bola de 0,35 m
		FVector Normal = FVector::UpVector;
		if (Victim)
		{
			Normal = (PointCm - Victim->GetActorLocation()).GetSafeNormal2D();
			Normal = (Normal + FVector(0.0, 0.0, 0.35)).GetSafeNormal();
		}
		if (ABlastDebris* H = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
		{
			H->GlowScale = ImpactGlow;
			H->StartMissileHit(PointCm, Normal, int32(Variant * 2654435761u >> 1));
		}
		if (Victim)
		{
			FVector Local = FVector::ZeroVector;
			if (UStaticMeshComponent* Part = PartNear(Victim, PointCm, Local))
			{
				MarkHull(Part, Local, 1);
			}
		}
		// a bola da ogiva: a Carga Pesada a 0,07, no relógio do tamanho. Só o 1º acerto da rajada no mesmo alvo (no máximo uma a
		// cada 0,6 s); os outros ficam com o clarão e as faíscas
		const double* Last = Victim ? LastWarheadBall.Find(Victim) : nullptr;
		if (LoadAssets() && (!Last || Now - *Last > 0.6))
		{
			if (Victim)
			{
				LastWarheadBall.Add(Victim, Now);
			}
			if (AExplosionVolume* Ball = Spawn(FVector(PointCm.X, PointCm.Y, FMath::Max(double(GroundZCm), PointCm.Z - 150.0)), Bakes[Variant % uint32(Bakes.Num())],
				LegacyWarheadScale, VariantFraction(Variant, 0xB0B0)))
			{
				Ball->bLights = false;                  // a luz é a dos destroços (a compensação pela exposição)
				Ball->bFlash = false;
				Ball->RaymarchSoot = LegacyWarheadSoot;
				Alive.Add(Ball);
			}
		}
		if (bKill)
		{
			EndDamaged(Victim);
			StartWreck(Victim);
		}
		return PointCm;
	}
	CSV_SCOPED_TIMING_STAT(BlastFxDirector, MissileHit);
	// (23/10, a explosão com vida) com o pulso do jogo e -HitLifeV=2 (o padrão): o acerto com vida (MissileHitLife). O caminho abaixo fica
	// o de hoje, exato (-HitLifeV=1, ou quem chama sem o pulso)
	if (LifeIn && BlastFxHitLife::Version() >= 2)
	{
		return MissileHitLife(Variant, PointCm, FromCm, Victim, GroundZCm, bKill, WarheadKg, bClassicLook, *LifeIn, LifeOut);
	}
	// (23/10, a ogiva v2) o ponto: o raio de onde o míssil vinha entra na caixa das peças do alvo (o ArmorPoint do tiro de canhão), na
	// face virada pra ele (o jogo manda o centro do alvo, e a ogiva explodia dentro do casco). O começo do raio recuado 30 m: o passo
	// de antes do míssil fica a 1–2 m do centro, às vezes dentro da caixa
	FShotPoint Shot;
	bool bOnHull = false;
	FVector Point = PointCm;
	FVector Out2 = (FromCm - PointCm).GetSafeNormal2D();
	if (Victim)
	{
		bOnHull = ArmorPoint(Victim, Out2.IsNearlyZero() ? FromCm : PointCm + Out2 * 3000.0, PointCm, GroundZCm, Variant, Shot);
		if (bOnHull)
		{
			Point = Shot.Point;
			// pra fora pela face da caixa (a do teto não dá rumo: fica o de onde o míssil veio)
			const FVector Face2(Shot.Normal.X, Shot.Normal.Y, 0.0);
			if (Face2.SizeSquared() > 0.25)
			{
				Out2 = Face2.GetSafeNormal();
			}
		}
		if (Out2.IsNearlyZero())
		{
			Out2 = (PointCm - Victim->GetActorLocation()).GetSafeNormal2D();
		}
	}
	if (Out2.IsNearlyZero())
	{
		Out2 = FVector::ForwardVector;
	}
	// os destroços com a mesma inclinação de antes (35% pra cima); no chão, pra cima
	const FVector Normal = Victim ? (Out2 + FVector(0.0, 0.0, 0.35)).GetSafeNormal() : FVector::UpVector;
	const bool bBlast = WarheadKg > 0.0f && WarheadKnobs().Scale > 0.0f;
	const float Lambda = bBlast ? WarheadScale(WarheadKg) * WarheadKnobs().Scale : 0.0f;
	const float BallCm = CpFireballM * Lambda * 100.0f;
	const float ReachCm = bBlast ? FMath::Max(BallCm, RingR90M * WarheadRingScale(Lambda) * 100.0f) : 300.0f;
	// o pé da bola: no chão embaixo do ponto, empurrado pra fora da blindagem meia bola (84% da esfera fica do lado de fora: a ogiva
	// explode na face, não dentro do casco); o acerto no chão, no ponto
	const FVector Foot = Victim ? FVector(Point.X + Out2.X * 0.5 * BallCm, Point.Y + Out2.Y * 0.5 * BallCm, GroundZCm)
		: FVector(PointCm.X, PointCm.Y, GroundZCm);
	// (23/10, a revisão) o centro da bola que se vê (o pé, na altura do miolo quente): o jogo centra nele o empurrão e o calor no rastro
	const FVector BallCentre = bBlast ? Foot + FVector(0.0, 0.0, CpBallCentreM * Lambda * 100.0f) : PointCm;
	auto MarkVictim = [&]()
	{
		if (bOnHull)
		{
			MarkHull(Shot.Part.Get(), Shot.LocalCm, 1);
		}
		else if (Victim)
		{
			FVector Local = FVector::ZeroVector;
			if (UStaticMeshComponent* Part = PartNear(Victim, PointCm, Local))
			{
				MarkHull(Part, Local, 1);
			}
		}
	};
	if (!ShotNearView(*this, Foot, ReachCm))
	{
		// fora da vista (o efeito curto do tiro, a regra de 17/10): só a marca no casco, que fica, e a carcaça
		MarkVictim();
		if (bKill)
		{
			EndDamaged(Victim);
			StartWreck(Victim);
		}
		return BallCentre;
	}
	// (23/10, a revisão) primeiro a explosão em volume (a bola, a fuligem e o anel de areia), depois os destroços. No tiro que mata, a
	// morte traz a dela (a regra do ArmorHit)
	// (23/10, a 2ª revisão) dois sinais separados pros destroços:
	//  - o brilho laranja em sprite nunca sai na ogiva v2 (o CLAUDE.md: sprite chapado não é entrega, mesmo provisório). O fogo é um
	//    volume em todo caso: o corpo da ogiva; a morte, no tiro que mata; a clássica da metade pesada do ataque (WarheadKg 0: o jogo
	//    chama o TankDeath logo depois); ou a morte ou a clássica recente em cima. Sem vaga no raymarch (o teto do preset: na Baixa, da
	//    2ª Patriot em diante) ficam o clarão, as faíscas, as lascas e a luz, sem volume e sem sprite;
	//  - a luz na curva da bola só quando a bola é da ogiva (bBallLight: o corpo que nasceu, ou o aceso de outra ogiva em que este
	//    acerto se fundiu). Na morte e na clássica (a que mata, a pesada do ataque, a recente em cima) a bola é delas, que já acendem a
	//    própria luz (Spawn com bLit): aqui fica só a queda do clarão, sem a luz em dobro lavando o casco
	bool bBallLight = false;
	if (bBlast && !bKill && LoadAssets())
	{
		bBallLight = SpawnWarhead(Foot, Victim, WarheadKg, Variant, bClassicLook);
	}
	// o clarão branco-azulado, as faíscas, as lascas e a luz (a receita aprovada da ogiva de 4 kg, × K = (W/4 kg)^⅓; a luz entra nas
	// luzes da cena e acende as fumaças perto e, de noite, as unidades). Com a bola da ogiva, a luz na forma da curva dela, no relógio
	// do corpo: cheia até o pico (0,23 s da gravação) e caindo a 10% quando a luz física da bola cai (~0,85 s da gravação)
	if (ABlastDebris* H = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		H->GlowScale = ImpactGlow;
		const float K = bBlast ? Lambda / WarheadScale(4.0f) : 1.0f;
		if (bBallLight)
		{
			const FWarheadClock Clock = WarheadClockOf(Lambda);
			H->FireLightHoldS = Clock.AgeAt(CpLightPeakS);
			H->FireLightTauS = CpLightTauS * Clock.Ts;
		}
		H->StartMissileHit(Point, Normal, int32(Variant * 2654435761u >> 1), K, false);
	}
	MarkVictim();
	if (bKill)
	{
		EndDamaged(Victim);
		StartWreck(Victim);
	}
	return BallCentre;
}

void UBlastFxDirector::HullGlints(AActor* Target, const FVector& FromCm, const FVector& NearCm, float RadiusCm, int32 Count, uint32 Seed)
{
	// (23/10, a vida; a camada Glints) os estilhaços da ogiva de proximidade cobrem o alvo (e os vizinhos perto); no aço cada um acende um
	// clarão de microssegundos (JHU): 1–2 quadros, pontos de 0,15–0,3 m. Nas faces das caixas das peças (as do ArmorPoint) viradas pra bola,
	// a até RadiusCm do ponto; sorteados pela área das faces
	if (!Target || Count <= 0 || !GetWorld())
	{
		return;
	}
	struct FFace { const UStaticMeshComponent* Mesh; FTransform Cx; FBox Box; int32 Axis; float Sign; double Area; };
	TArray<FFace, TInlineAllocator<24>> Faces;
	double TotalArea = 0.0;
	for (UActorComponent* Comp : Target->GetComponents())
	{
		const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Comp);
		if (!Mesh || !Mesh->IsVisible() || !Mesh->GetStaticMesh() || !Mesh->CastShadow)
		{
			continue;
		}
		const FTransform Cx = Mesh->GetComponentTransform();
		const FBox Box = Mesh->GetStaticMesh()->GetBoundingBox();
		if (Box.GetSize().Z * Cx.GetScale3D().Z < 30.0 || Box.ComputeSquaredDistanceToPoint(Cx.InverseTransformPosition(NearCm)) > FMath::Square(double(RadiusCm) * 2.0))
		{
			continue;
		}
		const FVector Size = Box.GetSize() * Cx.GetScale3D().GetAbs();
		for (int32 Ax = 0; Ax < 3; ++Ax)
		{
			for (const float Sg : { -1.0f, 1.0f })
			{
				FVector Nl = FVector::ZeroVector;
				Nl[Ax] = Sg;
				const FVector Nw = Cx.TransformVectorNoScale(Nl).GetSafeNormal();
				FVector Center = Box.GetCenter();
				Center[Ax] = Sg > 0.0f ? Box.Max[Ax] : Box.Min[Ax];
				if (FVector::DotProduct(Nw, FromCm - Cx.TransformPosition(Center)) <= 0.0)
				{
					continue;                               // de costas pra bola
				}
				const double Area = Size[(Ax + 1) % 3] * Size[(Ax + 2) % 3];
				Faces.Add({ Mesh, Cx, Box, Ax, Sg, Area });
				TotalArea += Area;
			}
		}
	}
	if (Faces.IsEmpty() || TotalArea <= 0.0)
	{
		return;
	}
	FRandomStream Rng(int32(Seed & 0x7fffffff));
	TArray<FVector> Points, Normals;
	for (int32 Try = 0; Try < Count * 12 && Points.Num() < Count; ++Try)
	{
		double Pick = Rng.FRand() * TotalArea;
		const FFace* F = &Faces[0];
		for (const FFace& Fc : Faces)
		{
			F = &Fc;
			Pick -= Fc.Area;
			if (Pick <= 0.0)
			{
				break;
			}
		}
		FVector L(FMath::Lerp(F->Box.Min.X, F->Box.Max.X, double(Rng.FRand())), FMath::Lerp(F->Box.Min.Y, F->Box.Max.Y, double(Rng.FRand())),
			FMath::Lerp(F->Box.Min.Z, F->Box.Max.Z, double(Rng.FRand())));
		L[F->Axis] = F->Sign > 0.0f ? F->Box.Max[F->Axis] : F->Box.Min[F->Axis];
		const FVector W = F->Cx.TransformPosition(L);
		if (FVector::DistSquared(W, NearCm) > FMath::Square(double(RadiusCm)))
		{
			continue;
		}
		FVector Nl = FVector::ZeroVector;
		Nl[F->Axis] = F->Sign;
		Points.Add(W);
		Normals.Add(F->Cx.TransformVectorNoScale(Nl).GetSafeNormal());
	}
	if (Points.IsEmpty())
	{
		return;
	}
	if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Fx->GlowScale = ImpactGlow;
		Fx->StartHullGlints(Points, Normals, int32(Seed * 2246822519u >> 1));
	}
}

void UBlastFxDirector::NeighborMark(AActor* Neighbor, const FVector& BlastCm, uint32 Seed)
{
	// (23/10, a vida; a camada Neighbors) o vizinho levou o dano em área: a marca do míssil (tipo 1) na face virada pra explosão, onde o raio
	// da bola até o centro dele toca a caixa das peças
	if (!Neighbor)
	{
		return;
	}
	const FVector To = Neighbor->GetActorLocation() + FVector(0.0, 0.0, 120.0 + 60.0 * double(VariantFraction(Seed, 0x3E1u)));
	FShotPoint Hit;
	if (HullRay(Neighbor, BlastCm, To + (To - BlastCm).GetSafeNormal() * 400.0, Hit))
	{
		MarkHull(Hit.Part.Get(), Hit.LocalCm, 1);
	}
}

void UBlastFxDirector::PushLanding(const BlastFxHitLife::FLanding& L)
{
	FLandingRec& R = Landings.AddDefaulted_GetRef();
	R.PosCm = L.PosCm;
	R.SizeM = L.SizeM;
	R.Kind = L.Kind;
	R.bHot = L.bHot;
	R.VelMs = L.VelMs;
	if (Landings.Num() > 64)
	{
		Landings.RemoveAt(0, Landings.Num() - 64);   // ninguém esvaziando (sem o jogo): não cresce sem fim
	}
}

void UBlastFxDirector::ConsumeLandings(TArray<BlastFxHitLife::FLanding>& Out)
{
	for (const FLandingRec& R : Landings)
	{
		BlastFxHitLife::FLanding& L = Out.AddDefaulted_GetRef();
		L.PosCm = R.PosCm;
		L.SizeM = R.SizeM;
		L.Kind = R.Kind;
		L.bHot = R.bHot;
		L.VelMs = R.VelMs;
	}
	Landings.Reset();
}

void UBlastFxDirector::PushEmberSmoke(uint32 Id, const FVector& PosCm)
{
	// um ponto por pedaço (o último do quadro); ninguém esvaziando (sem o jogo): não cresce sem fim
	if (FEmberSmokeRec* Same = EmberSmoke.FindByPredicate([Id](const FEmberSmokeRec& R) { return R.Id == Id; }))
	{
		Same->PosCm = PosCm;
		return;
	}
	if (EmberSmoke.Num() >= 64)
	{
		EmberSmoke.RemoveAt(0);
	}
	EmberSmoke.Add({ Id, PosCm });
}

void UBlastFxDirector::ConsumeEmberSmoke(TArray<BlastFxHitLife::FEmberSmoke>& Out)
{
	for (const FEmberSmokeRec& R : EmberSmoke)
	{
		BlastFxHitLife::FEmberSmoke& E = Out.AddDefaulted_GetRef();
		E.Id = R.Id;
		E.PosCm = R.PosCm;
	}
	EmberSmoke.Reset();
}

namespace
{
	/** (23/10 noite) O espaço do casco: o pé do ator girado só pelo rumo do componente Body (RTSUnitActor: o ator do veículo não gira, quem
	 *  gira é o Body, com o rumo, a inclinação e o balanço); sem o Body (o prédio), o do ator. */
	FTransform HullFrameOf(const AActor* Target)
	{
		if (!Target)
		{
			return FTransform::Identity;
		}
		for (const UActorComponent* Comp : Target->GetComponents())
		{
			if (const USceneComponent* SC = Cast<USceneComponent>(Comp); SC && SC->GetFName() == FName(TEXT("Body")))
			{
				return FTransform(FRotator(0.0, SC->GetComponentRotation().Yaw, 0.0), Target->GetActorLocation());
			}
		}
		return Target->GetActorTransform();
	}
}

bool UBlastFxDirector::HullBox(AActor* Target, FBox& Out) const
{
	// (a revisão da vida) a caixa do alvo no espaço dele, sem escala: a soma das caixas das peças que o HullRay e o ArmorPoint enxergam (as
	// visíveis que fazem sombra, opacas, de 30 cm de altura pra cima: o casco, a torre, as lagartas; a poeira das lagartas, a sombra de contato
	// e o cano fino não)
	Out = FBox(ForceInit);
	if (!Target)
	{
		return false;
	}
	const FTransform Ax = HullFrameOf(Target);   // (23/10 noite) no rumo do casco (o Body), não no do ator
	for (UActorComponent* Comp : Target->GetComponents())
	{
		const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Comp);
		if (!Mesh || !Mesh->IsVisible() || !Mesh->GetStaticMesh() || !Mesh->CastShadow)
		{
			continue;
		}
		if (const UMaterialInterface* M = Mesh->GetMaterial(0); M && M->GetBlendMode() != BLEND_Opaque && M->GetBlendMode() != BLEND_Masked)
		{
			continue;
		}
		const FTransform Cx = Mesh->GetComponentTransform();
		const FBox Box = Mesh->GetStaticMesh()->GetBoundingBox();
		if (Box.GetSize().Z * Cx.GetScale3D().Z < 30.0)
		{
			continue;
		}
		for (int32 k = 0; k < 8; ++k)
		{
			const FVector Corner((k & 1) ? Box.Max.X : Box.Min.X, (k & 2) ? Box.Max.Y : Box.Min.Y, (k & 4) ? Box.Max.Z : Box.Min.Z);
			Out += Ax.InverseTransformPositionNoScale(Cx.TransformPosition(Corner));
		}
	}
	return Out.IsValid != 0;
}

bool UBlastFxDirector::HullRay(AActor* Target, const FVector& FromCm, const FVector& ToCm, FShotPoint& Out) const
{
	// a mesma conta do ArmorPoint (cada peça visível que faz sombra, na caixa da própria malha e no espaço dela: o 1º toque), com o raio de
	// verdade em 3D em vez do horizontal na altura sorteada (o tiro de canhão)
	if (!Target)
	{
		return false;
	}
	float BestT = TNumericLimits<float>::Max();
	for (UActorComponent* Comp : Target->GetComponents())
	{
		const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Comp);
		if (!Mesh || !Mesh->IsVisible() || !Mesh->GetStaticMesh() || !Mesh->CastShadow)
		{
			continue;
		}
		if (const UMaterialInterface* M = Mesh->GetMaterial(0); M && M->GetBlendMode() != BLEND_Opaque && M->GetBlendMode() != BLEND_Masked)
		{
			continue;
		}
		const FTransform Cx = Mesh->GetComponentTransform();
		const FBox Box = Mesh->GetStaticMesh()->GetBoundingBox();
		if (Box.GetSize().Z * Cx.GetScale3D().Z < 30.0)
		{
			continue;
		}
		FVector HitL, NormalL;
		float Time = 0.0f;
		if (FMath::LineExtentBoxIntersection(Box, Cx.InverseTransformPosition(FromCm), Cx.InverseTransformPosition(ToCm), FVector::ZeroVector, HitL, NormalL, Time)
			&& Time < BestT)
		{
			BestT = Time;
			Out.Point = Cx.TransformPosition(HitL);
			Out.Normal = Cx.TransformVectorNoScale(NormalL).GetSafeNormal();
			Out.Part = Cast<UStaticMeshComponent>(Comp);
			Out.LocalCm = HitL;
		}
	}
	return BestT < 2.0f;
}

FVector UBlastFxDirector::MissileHitLife(uint32 Variant, const FVector& PointCm, const FVector& FromCm, AActor* Victim, float GroundZCm, bool bKill,
	float WarheadKg, bool bClassicLook, const BlastFxHitLife::FHitIn& In, BlastFxHitLife::FHitOut* Out)
{
	// A explosão com vida (23/10; o plano e o estudo: docs/ESTUDO_MISSIL_PATRIOT.md, "A explosão com vida"; BlastFxHitLife.h). O mesmo
	// acerto da ogiva v2, mais as reações, cada uma atrás da sua variável:
	//  - Dir: a explosão nasce no ponto e sai torta pela física. Uma carga na face vira meia esfera centrada nela e se abre em volta da
	//    normal (Aouad et al. 2020; Kingery-Bulmash); a parte do que o míssil trazia ao longo da face continua, e a bola escorrega pra frente
	//    (os impactos oblíquos: Schultz e Gault 1989; a ejeção que endireita: arXiv 2105.01474). ℓ = normalize(n + 0,3·cima·[face de pé] +
	//    w·t), d em 3D (o mergulho fica), t = d − (d·n)n. A bola nasce em P + ℓ·0,35·R e anda mais 0,7·R ao longo de ℓ em 1 − e^(−t/τ),
	//    τ = 0,25 × o fim da bola acesa; o giro pelo rumo de ℓ; as faíscas, as lascas, a luz e o anel de areia por ℓ. No chão, ℓ ≈ cima:
	//    sobe do chão plano, puxada pra frente;
	//  - os destroços (Sparks, Embers, Glints), a nuvem viva (Cloud), o míssil no chão (Ground), a gravação dedicada (Bake).
	using namespace BlastFxHitLife;
	UWorld* World = GetWorld();
	FHitOut R;
	R.bValid = true;
	const bool bDir = On(Dir);
	FVector D = (PointCm - FromCm).GetSafeNormal();
	if (D.IsNearlyZero())
	{
		D = FVector(0.0, 0.0, -1.0);
	}
	// o ponto e a face. Com a direção: o raio do míssil de verdade (o mergulho guardado, recuado 30 m) nas caixas das peças: o Tomahawk que
	// cai a 40–60° acha o teto, a Patriot quase rasante acha o costado. Sem achar (ou sem a direção): o ArmorPoint de hoje
	FShotPoint Shot;
	bool bOnHull = false;
	FVector Point = PointCm;
	FVector Out2 = (FromCm - PointCm).GetSafeNormal2D();
	if (Victim)
	{
		if (bDir)
		{
			bOnHull = HullRay(Victim, PointCm - D * 3000.0, PointCm + D * 800.0, Shot);
		}
		if (!bOnHull)
		{
			bOnHull = ArmorPoint(Victim, Out2.IsNearlyZero() ? FromCm : PointCm + Out2 * 3000.0, PointCm, GroundZCm, Variant, Shot);
		}
		if (bOnHull)
		{
			Point = Shot.Point;
			const FVector Face2(Shot.Normal.X, Shot.Normal.Y, 0.0);
			if (Face2.SizeSquared() > 0.25)
			{
				Out2 = Face2.GetSafeNormal();
			}
		}
		if (Out2.IsNearlyZero())
		{
			Out2 = (PointCm - Victim->GetActorLocation()).GetSafeNormal2D();
		}
	}
	if (Out2.IsNearlyZero())
	{
		Out2 = FVector::ForwardVector;
	}
	// a normal: a da face achada (no chão, a do terreno); sem achar, a de hoje (pra fora no chão)
	FVector N = Victim ? (bOnHull ? Shot.Normal.GetSafeNormal() : Out2) : GroundNormalAt(World, Point);
	if (N.IsNearlyZero())
	{
		N = Victim ? Out2 : FVector::UpVector;
	}
	const bool bTop = Victim && N.Z > 0.5;
	// sem a direção, a regra de hoje: pra fora pela face, 35% pra cima; no chão, pra cima
	const FVector Legacy = Victim ? (Out2 + FVector(0.0, 0.0, 0.35)).GetSafeNormal() : FVector::UpVector;
	const FVector Tan = D - N * FVector::DotProduct(D, N);
	const FVector Up03 = Victim && FMath::Abs(N.Z) < 0.5 ? FVector(0.0, 0.0, 0.3) : FVector::ZeroVector;
	FVector L = bDir ? (N + Up03 + Tan * double(FMath::Clamp(In.Carry, 0.0f, 2.0f))).GetSafeNormal() : Legacy;
	if (L.IsNearlyZero())
	{
		L = Legacy;
	}
	// a bola: a escala λ da ogiva (a Carga Pesada em Hopkinson-Cranz) e o K dos destroços (a receita aprovada da ogiva de 4 kg)
	const FWarheadKnobs& Knobs = WarheadKnobs();
	const bool bBlast = WarheadKg > 0.0f && Knobs.Scale > 0.0f;
	const float Lambda = bBlast ? WarheadScale(WarheadKg) * Knobs.Scale : 0.0f;
	const float BallCm = CpFireballM * Lambda * 100.0f;
	const float Kh = bBlast ? Lambda / WarheadScale(4.0f) : 1.0f;
	const FWarheadClock Clock = WarheadClockOf(FMath::Max(Lambda, 0.01f));
	const float FireEnd = bBlast ? Clock.AgeAt(CpGlowEndS) : 0.0f;
	// (a areia do acerto) no ataque a strike_chao chega a ±28 m: a vista confere a areia inteira, não só a bola
	const float ReachCm = FMath::Max(bBlast ? FMath::Max(BallCm, RingR90M * WarheadRingScale(Lambda) * 100.0f) : 300.0f, !Victim && In.bStrike ? 2800.0f : 0.0f);
	// (Bake) a gravação dedicada do caso, decidida antes do pé (com ela o pé e o giro são outros): de lado (a face de pé), no teto (a de
	// cima), no chão (a do ataque também: a strike_chao é a areia, à parte). Só quando o arquivo carregou (BlastRaymarchSubsystem); senão a
	// Carga Pesada em escala
	const UBlastRaymarchSubsystem* Rm = World->GetSubsystem<UBlastRaymarchSubsystem>();
	// (25/10, o impacto pelo chão; o Felipe: "o impacto olha o chão em volta") o míssil no chão: o que o chão em volta tem pra soltar. A
	// areia solta no miolo (a cratera, 3 m) decide se as gravações de areia nascem (a strike_chao, a areia da warhead_chao, o anel, a
	// cortina, a saia da bola); a da área (o alcance da strike_chao na escala dela, ~20 m; a ogiva sozinha, 8 m) diz quanto dela (o chão
	// misto: a cratera na areia ao lado da estrada). Fora da areia (o miolo sem ela, ou um tipo que não é areia): a resposta do tipo na
	// escala da carga (o concreto e o asfalto: as lascas, o pó cinza e a saia de gás; a neve: o pó largo; a terra e a grama: a cortina de
	// terra e os tufos; o barro: o respingo), e a areia que houver em volta sobe pelo jogo, ponto a ponto, na poeira em volume (o véu, a
	// onda de base e a poeira que fica, cada puff pela areia do lugar dele: RTSHitLife.cpp): o vento do choque empurra a areia do
	// acostamento pra fora, e no concreto sem areia não sobe nada. Na areia inteira (o miolo e a área), o de hoje, exato
	const FVector GroundPt(Point.X, Point.Y, GroundZCm);
	FGroundResponse Typed;
	const bool bTyped = !Victim && TypedGround(GroundPt, Typed);
	float SandCore = 1.0f, SandArea = 1.0f;
	if (!Victim)
	{
		SandAround(GroundPt, 300.0f, In.bStrike ? 2000.0f : 800.0f, SandCore, SandArea);
	}
	const bool bSandGround = Victim || (!bTyped && SandCore >= 0.5f);
	FVector4f VictimSkirt;
	bool bVictimGas = false;
	// (a areia do acerto) a strike_chao toca neste míssil (abaixo): ela é a areia do chão do ataque inteira
	const bool bStrikeSand = !Victim && bSandGround && In.bStrike && On(Strike) && On(BlastFxHitLife::Bake) && Rm && Rm->SourceDuration(FName(StrikeBakeGround)) > 0.0f;
	FName OwnSource = NAME_None;
	float OwnScale = 1.0f;
	const TCHAR* OwnWhy = TEXT("sem gravação dedicada");
	FString OwnFit;                                 // (o registro) as medidas do ajuste do casco
	// (23/10 noite, a 2ª prova) o rumo do casco: o ator do veículo não gira, quem gira é o componente Body (RTSUnitActor: o rumo, a inclinação
	// e o balanço); o ajuste e o eixo da warhead_teto usam só o rumo dele (sem o Body, o do ator)
	const FTransform HullX = HullFrameOf(Victim);
	if (bBlast && !bKill && Rm && On(BlastFxHitLife::Bake))
	{
		const TCHAR* Name = nullptr;
		if (!Victim)
		{
			// (23/10 noite) no chão: a warhead_chao (o fogo, a fuligem e a cortina de grãos de 12 kg). No ataque a areia é a strike_chao: a ogiva
			// em cima dela é a warhead_chao_s (só o fogo e a fuligem) com -StrikeWarhead=1, senão a Carga Pesada em escala no visual da clássica
			// (a aprovada do ataque, "explosão bonita"; sem o anel: bNoRing)
			static const bool bStrikeOwn = CmdFloat(TEXT("StrikeWarhead="), 0.0f) != 0.0f;
			Name = !bStrikeSand ? WarheadBakeGround : (bStrikeOwn ? WarheadBakeGroundSoot : nullptr);
			OwnWhy = Name ? TEXT("o chão") : TEXT("a areia é a strike_chao; a ogiva fica a Carga Pesada em escala");
			if (!bSandGround)
			{
				// (25/10, o impacto pelo chão) fora da areia, a mesma bola sem a areia dentro: no ataque a de sempre dele (a Carga Pesada em
				// escala, ou a warhead_chao_s com -StrikeWarhead=1); fora dele a warhead_chao_s (o fogo e a fuligem da warhead_chao)
				Name = In.bStrike && On(Strike) && !bStrikeOwn ? nullptr : WarheadBakeGroundSoot;
				OwnWhy = Name ? TEXT("o chão sem areia (o fogo e a fuligem)") : TEXT("o chão sem areia; a ogiva fica a Carga Pesada em escala");
			}
		}
		else if (bOnHull && GroundSkirtAt(GroundPt, VictimSkirt, bVictimGas))
		{
			// (25/10, o impacto pelo chão) as dedicadas do casco têm a areia do chão dentro: no chão sem areia, a Carga Pesada em escala (a
			// saia dela pelo chão, SpawnWarhead) e o véu do jogo, ponto a ponto
			OwnWhy = TEXT("o chão em volta sem areia (a dedicada tem a areia dentro)");
		}
		else if (bOnHull)
		{
			// (23/10 noite) o ajuste do casco: a caixa só do casco e das lagartas no espaço do alvo (as peças fora do pivô da torre: a torre e o
			// cano giram com a mira, e com eles o tanque mirando de lado passava de 7 m de largura; 1ª prova no jogo, bake_tomahawk), a face
			// atingida (de comprido ou de frente), o ponto e, no teto, o pivô da torre
			FBox HB(ForceInit);
			const FTransform& VX = HullX;
			FVector TurretL = FVector::ZeroVector;
			bool bTurretPivot = false;
			for (UActorComponent* Comp : Victim->GetComponents())
			{
				const USceneComponent* SC = Cast<USceneComponent>(Comp);
				if (SC && SC->GetFName() == FName(TEXT("TurretPivot")))
				{
					TurretL = VX.InverseTransformPositionNoScale(SC->GetComponentLocation());
					bTurretPivot = true;
				}
				const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Comp);
				if (!Mesh || !Mesh->IsVisible() || !Mesh->GetStaticMesh() || !Mesh->CastShadow)
				{
					continue;
				}
				bool bOnTurret = false;
				for (const USceneComponent* Up = Mesh->GetAttachParent(); Up && !bOnTurret; Up = Up->GetAttachParent())
				{
					bOnTurret = Up->GetFName() == FName(TEXT("TurretPivot"));
				}
				const FTransform Cx = Mesh->GetComponentTransform();
				const FBox MB = Mesh->GetStaticMesh()->GetBoundingBox();
				if (bOnTurret || MB.GetSize().Z * Cx.GetScale3D().Z < 30.0)
				{
					continue;
				}
				for (int32 k = 0; k < 8; ++k)
				{
					HB += VX.InverseTransformPositionNoScale(Cx.TransformPosition(FVector((k & 1) ? MB.Max.X : MB.Min.X, (k & 2) ? MB.Max.Y : MB.Min.Y,
						(k & 4) ? MB.Max.Z : MB.Min.Z)));
				}
			}
			if (HB.IsValid)
			{
				const FVector Nl = VX.InverseTransformVectorNoScale(FVector(N.X, N.Y, 0.0)).GetSafeNormal();
				const FVector Pl = VX.InverseTransformPositionNoScale(Point);
				const FVector Sz = HB.GetSize() / 100.0, Ct = HB.GetCenter();
				// a altura do casco sem a torre (o gravado: 1,85 m)
				const bool bLongOk = FMath::Abs(Sz.X - BakeHullLongM) <= BakeFitLongTolM && FMath::Abs(Sz.Y - BakeHullWideM) <= BakeFitWideTolM
					&& Sz.Z >= BakeFitMinHullM && Sz.Z <= BakeFitMaxHullM;
				OwnFit = FString::Printf(TEXT("; casco %.1f × %.1f × %.1f m, acerto a (%.1f, %.1f, %.1f) m do meio, normal (%.2f, %.2f)%s"), Sz.X, Sz.Y, Sz.Z,
					(Pl.X - Ct.X) / 100.0, (Pl.Y - Ct.Y) / 100.0, (Point.Z - GroundZCm) / 100.0, Nl.X, Nl.Y,
					bTurretPivot ? *FString::Printf(TEXT(", torre a (%.1f, %.1f) m"), (TurretL.X - Ct.X) / 100.0, (TurretL.Y - Ct.Y) / 100.0) : TEXT(""));
				if (!bLongOk)
				{
					OwnWhy = TEXT("o casco não é do tamanho do gravado (7,9 × 3,6 m)");
				}
				else if (!bTop)
				{
					// de lado: só o costado (a normal no eixo y do alvo) e o acerto até 1,3 m do meio dele
					const double OffM = (Pl.X - Ct.X) / 100.0;
					if (FMath::Abs(Nl.Y) < 0.8)
					{
						OwnWhy = TEXT("de frente ou de trás (a dedicada é o costado)");
					}
					else if (FMath::Abs(OffM) > BakeFitHitTolM)
					{
						OwnWhy = TEXT("o acerto longe do meio do costado");
					}
					else
					{
						// a mão: o lobo da gravação puxa pro −Y dela, no mundo (N.y, −N.x) com o +X na normal; o míssil correndo ao contrário
						// ao longo da face toca a espelhada
						const FVector Tw = D - N * FVector::DotProduct(D, N);
						const FVector LobeW(N.Y, -N.X, 0.0);
						const bool bMirror = FVector::DotProduct(FVector(Tw.X, Tw.Y, 0.0), LobeW) < 0.0;
						Name = bMirror ? WarheadBakeSideMirror : WarheadBakeSide;
						OwnWhy = bMirror ? TEXT("o costado (espelhada)") : TEXT("o costado");
					}
				}
				else
				{
					// no teto: o ponto até 1,3 m do meio do teto da torre (o pivô dela; sem ele, 0,4 m atrás do meio do casco no rumo do míssil,
					// como na gravação) e na altura dela
					const FVector Lh = VX.InverseTransformVectorNoScale(FVector(L.X, L.Y, 0.0));
					const double Sx = Lh.X < 0.0 ? -1.0 : 1.0;
					const FVector2D Want = bTurretPivot ? FVector2D(TurretL.X, TurretL.Y) : FVector2D(Ct.X - Sx * BakeHullOffTopM * 100.0, Ct.Y);
					const double OffM = FVector2D::Distance(FVector2D(Pl.X, Pl.Y), Want) / 100.0;
					const double HighM = (Point.Z - GroundZCm) / 100.0;
					if (OffM > BakeFitHitTolM || HighM < BakeFitMinHighM)
					{
						OwnWhy = TEXT("o acerto fora do teto da torre");
					}
					else
					{
						Name = WarheadBakeTop;
						OwnWhy = TEXT("o teto da torre");
					}
				}
			}
			else
			{
				OwnWhy = TEXT("sem a caixa do alvo");
			}
		}
		if (Name && Rm->SourceDuration(FName(Name)) > 0.0f)
		{
			static const float RefKg = CmdFloat(TEXT("HitBakeRefKg="), 12.0f);
			OwnSource = FName(Name);
			OwnScale = FMath::Pow(WarheadKg / FMath::Max(RefKg, 0.01f), 1.0f / 3.0f) * Knobs.Scale;
		}
		else if (Name)
		{
			OwnWhy = TEXT("a gravação não carregou");
		}
	}
	const bool bOwn = !OwnSource.IsNone();
	if (Knobs.bLog && bBlast && !bKill)
	{
		UE_LOG(LogTemp, Display, TEXT("BlastFx ogiva dedicada: %s (%s%s)"), bOwn ? *OwnSource.ToString() : TEXT("nenhuma"), OwnWhy, *OwnFit);
	}
	FVector Foot;
	FVector Move = FVector::ZeroVector;
	FVector BallCentre = PointCm;
	const FVector Lift(0.0, 0.0, CpBallCentreM * Lambda * 100.0f);
	bool bRoofFoot = false;
	if (bOwn)
	{
		// (a revisão da vida) a gravação dedicada traz dentro dela o casco (ou o chão), a carga no ponto e o lobo pra onde o míssil ia: o pé no
		// chão sob o ponto do acerto (a origem dela), o giro pela face, pelo casco ou pelo rumo do míssil (abaixo), sem andar. A bola que se
		// vê (o empurrão no rastro, os brilhos) é a da carga no ponto, aberta ao longo de ℓ
		Foot = FVector(Point.X, Point.Y, GroundZCm);
		BallCentre = Point + L * (0.35 * BallCm);
	}
	else if (bDir && bBlast)
	{
		// o centro nasce em P + ℓ·0,35·R e anda mais 0,7·R ao longo de ℓ. A gravação é de chão (a Carga Pesada): de lado e no chão, o pé no
		// chão embaixo do centro. (A revisão) No teto, sem a warhead_teto, o pé no plano do próprio teto: a carga que explode em cima do casco
		// é uma explosão de superfície nesse plano (o teto faz o papel do chão; Kingery-Bulmash, a carga na superfície), e a bola fica em
		// cima dele. Com o pé no chão ela nascia dentro da planta do tanque (o centro a 1,0–1,5 m, o teto a 2,4 m) e andava só ~0,5 m: o
		// "a ogiva explodia dentro do casco" de volta, quase todo Tomahawk (o mergulho de ~31° já acha o teto antes da lateral)
		const FVector C0 = Point + L * (0.35 * BallCm);
		Foot = FVector(C0.X, C0.Y, bTop ? FMath::Max(double(GroundZCm), Point.Z) : double(GroundZCm));
		Move = FVector(L.X, L.Y, 0.0) * (0.7 * BallCm);
		BallCentre = Foot + Lift;
		// (a 2ª revisão da vida) só que a Carga Pesada é uma gravação no chão de areia: o canal da poeira dela é a areia que o choque levanta
		// (a saia baixa, até 0,65–0,93 R do pé), que o corpo pinta de areia do mapa. Aço e telhado não levantam areia: em cima do casco o que
		// corre rente é o jato de parede dos produtos da detonação (o gás quente refletido na placa, escuro da fuligem da carga). Com o pé no
		// teto essa saia sai com a cor e o material da fuligem da própria bola (FWarheadLife::bGasSkirt), não bege; o anel de areia segue no
		// chão de verdade, onde o choque chega
		bRoofFoot = bTop && Foot.Z > double(GroundZCm) + 50.0;
	}
	else
	{
		Foot = Victim ? FVector(Point.X + Out2.X * 0.5 * BallCm, Point.Y + Out2.Y * 0.5 * BallCm, GroundZCm) : FVector(PointCm.X, PointCm.Y, GroundZCm);
		BallCentre = bBlast ? Foot + Lift : PointCm;
	}
	R.PointCm = Point;
	R.NormalW = N;
	R.IncomingW = D;
	R.AimW = L;
	R.bOnHull = bOnHull;
	R.bTop = bTop;
	R.FootCm = Foot;
	R.BallCentreCm = BallCentre;
	R.BallM = BallCm / 100.0f;
	R.FireEndS = FireEnd;
	auto MarkVictim = [&]()
	{
		if (bOnHull)
		{
			MarkHull(Shot.Part.Get(), Shot.LocalCm, 1);
		}
		else if (Victim)
		{
			FVector Local = FVector::ZeroVector;
			if (UStaticMeshComponent* Part = PartNear(Victim, PointCm, Local))
			{
				MarkHull(Part, Local, 1);
			}
		}
	};
	if (!ShotNearView(*this, Foot, ReachCm))
	{
		// fora da vista (o efeito curto do tiro, a regra de 17/10): só a marca no casco, que fica, e a carcaça
		MarkVictim();
		if (bKill)
		{
			EndDamaged(Victim);
			StartWreck(Victim);
		}
		R.bShown = false;
		if (Out)
		{
			*Out = R;
		}
		return BallCentre;
	}
	const int32 Pr = Preset();
	if (Victim)
	{
		// a caixa do alvo (a planta dele no chão: a sombra do casco na onda do choque, cada veículo com a sua)
		FBox Box(ForceInit);
		if (HullBox(Victim, Box))
		{
			R.bHullBox = true;
			R.HullYawDeg = float(HullX.Rotator().Yaw);
			R.HullMinCm = Box.Min;
			R.HullMaxCm = Box.Max;
		}
	}
	// a explosão em volume (a bola, a fuligem e o anel de areia), como na ogiva v2, com a vida: a nuvem viva (Cloud: 6·G s no Alta/Ultra,
	// 3·G na Média, nenhuma na Baixa; o dobro do plano §6 desde 31/10), a bola andando por ℓ e o giro por ℓ (Dir), a gravação dedicada do caso (Bake)
	bool bBallLight = false;
	if (bBlast && !bKill && LoadAssets())
	{
		FWarheadLife Life;
		// (a revisão) uma representação só da areia que o choque levanta: com o véu do jogo ou com a strike_chao, sem o anel da ogiva
		Life.bNoRing = In.bGameVeil || bStrikeSand || !bSandGround;   // (25/10) fora da areia o pó é o da resposta do tipo (abaixo)
		Life.bCloud = On(Cloud) && Pr >= 1;
		// (31/10, a revisão: "fumaça de ogiva fica muitos segundos, sobe, abre e se dilui com o vento") o dobro: a nuvem agora rareia desde
		// a parada (UpdateWarheads, a diluição), não só no fim; com o trecho dobrado ela ainda tem ~2/3 da densidade no instante em que a
		// de antes começava a sumir (3,8 s na Patriot, 4,7 s no Tomahawk) e acaba aos ~7,5 e ~9 s do acerto (antes 4,5 e 5,5 s)
		Life.CloudExtraS = (Pr >= 2 ? 6.0f : 3.0f) * G(WarheadKg);
		if (bOwn)
		{
			// (a revisão da vida) a dedicada: o +X dela é, de lado, a normal da face pra fora (o casco-obstáculo de comprido em Y); no teto, o
			// eixo do casco no sentido de ℓ (o lobo em +X); no chão, o rumo do míssil. Sem andar (a direção já está na simulação)
			Life.Source = OwnSource;
			Life.SourceScale = OwnScale;
			FVector Ax = FVector::ZeroVector;
			if (Victim && !bTop)
			{
				Ax = FVector(N.X, N.Y, 0.0).SizeSquared() > 0.04 ? FVector(N.X, N.Y, 0.0) : Out2;
			}
			else if (Victim)
			{
				const FVector Fwd = HullX.GetUnitAxis(EAxis::X);   // (a 2ª prova) o rumo do casco (o Body), não o do ator
				Ax = FVector(Fwd.X, Fwd.Y, 0.0);
				Ax *= FVector::DotProduct(Ax, FVector(L.X, L.Y, 0.0)) < 0.0 ? -1.0 : 1.0;
			}
			else
			{
				Ax = FVector(D.X, D.Y, 0.0).SizeSquared() > 0.05 * 0.05 ? FVector(D.X, D.Y, 0.0) : FVector(L.X, L.Y, 0.0);
			}
			if (Ax.SizeSquared() > 0.05 * 0.05)
			{
				Life.bYaw = true;
				Life.YawRad = float(FMath::Atan2(Ax.Y, Ax.X));
			}
		}
		else if (bDir)
		{
			Life.MoveCm = Move;
			Life.MoveTauS = FMath::Max(0.25f * FireEnd, 0.02f);
			const FVector Lh(L.X, L.Y, 0.0);
			if (Lh.SizeSquared() > 0.05 * 0.05)
			{
				Life.bYaw = true;
				Life.YawRad = float(FMath::Atan2(L.Y, L.X));
				Life.bRingFoot = true;
				Life.RingFootCm = FVector(Point.X, Point.Y, GroundZCm) + Lh.GetSafeNormal() * (0.3 * BallCm);
			}
		}
		Life.bGasSkirt = bRoofFoot;
		bBallLight = SpawnWarhead(Foot, Victim, WarheadKg, Variant, bClassicLook, &Life);
		R.bBody = bBallLight;
		R.CloudEndS = Life.OutCloudEndS;
		R.bOwnSand = Life.bOutOwn && OwnSource != FName(WarheadBakeGroundSoot);   // (23/10 noite) a _s não tem areia dentro
		R.OwnSandFadeS = Life.OutOwnFadeS;
	}
	// (23/10, a areia do acerto; o Felipe: "os tomahawk quando cai na areia em si, fica igual o gameshot, areia pra caralho, impacto, denso")
	// a areia do ataque em todo míssil, as duas metades: a gravação strike_chao (só a areia do Tomahawk de verdade no chão, ~150 kg: a
	// cortina em dedos a 50–58°, a onda de base, o véu, o jato escuro) na escala 1, no rumo do míssil (ela foi simulada com o míssil indo
	// pra +X), com a bola clássica (a metade pesada) ou a ogiva (a leve) em cima. Ela é a areia: sem luz nem clarão (os do acerto são da
	// bola e dos destroços), a cor da areia do mapa. O jogo põe a poeira que fica a partir do esmaecer dela (FHitOut::SandFadeS); sem ela
	// (sem o arquivo, sem vaga, Bake ou Strike desligadas) o jogo faz a areia de perto na poeira em volume (RTSHitLife.cpp)
	if (bStrikeSand)
	{
		// (23/10 noite, a prova no jogo: v4_strike gravada, A/B na mesma corrente) a 0,7: a gravação é a areia de ~150 kg por míssil e as bolas
		// que se veem são de 12–20 kg; na escala 1 as 6 areias juntas (12–14 m de raio) enterravam as bolas aprovadas por ~1 s e cobriam as
		// Patriots por ~3 s; a 0,55 a areia some atrás das bolas. A 0,7 (~50 kg de areia, o relógio × 0,7: a onda de base acaba em ~4 s) as
		// bolas ficam inteiras com a areia densa em volta e embaixo, e as unidades voltam a ler ~2 s depois do impacto
		static const float StrikeScale = FMath::Max(CmdFloat(TEXT("StrikeBakeScale="), 0.7f), 0.05f);
		// (25/10, o impacto pelo chão) a cratera na areia com menos areia em volta (a estrada, o concreto ao lado): a gravação inteira com
		// menos areia, pela da área (a partir de 85% de areia na área, a inteira: na areia, 1 exato)
		const float StrikeO = SandArea >= 1.0f ? 1.0f : FMath::Clamp(SandArea / 0.85f, 0.0f, 1.0f);
		if (AExplosionVolume* Sand = SpawnDust(FVector(Point.X, Point.Y, GroundZCm), StrikeScale, StrikeScale, StrikeO, 1, Variant ^ 0x5A2Du,
			FVector4f(-1.0f, 0.0f, 0.0f, 0.0f), StrikeBakeGround, 1.0f, GroundCurtainDirt))
		{
			Dusts.Last().FadeInS = StrikeSandFadeInS;
			const FVector Dh(D.X, D.Y, 0.0);
			Sand->RaymarchYawRad = Dh.SizeSquared() > 0.05 * 0.05 ? float(FMath::Atan2(Dh.Y, Dh.X)) : 2.0f * PI * VariantFraction(Variant, 0x5A2Du);
			// o UpdateDusts esmaece a areia no último quarto da gravação e tira no fim (no relógio dela: × a escala de tempo)
			const float Dur = Rm->SourceDuration(FName(StrikeBakeGround)) * StrikeScale;
			R.bSandBake = true;
			R.SandFadeS = 0.75f * Dur;
			R.SandEndS = Dur;
			// (25/10, a poeira que fica) a areia que cai vira a nuvem que fica, no mesmo volume (o jogo não põe a camada Linger por cima)
			R.bBakeLinger = MakeLinger(FName(StrikeBakeGround), LingerStrikeHold, LingerSettleSand);
		}
	}
	// o clarão, as faíscas, as lascas, a luz (a receita aprovada da ogiva de 4 kg × K), com a vida: os jatos (Sparks), os pedaços em brasa
	// (Embers: 2·K², teto 4, no teto vivo do preset) e a luz no centro da bola
	if (ABlastDebris* H = World->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		H->GlowScale = ImpactGlow;
		if (bBallLight)
		{
			H->FireLightHoldS = Clock.AgeAt(CpLightPeakS);
			H->FireLightTauS = CpLightTauS * Clock.Ts;
		}
		// d e a normal sempre (o leque rente à face a favor de t, os pedaços no rumo do míssil); ℓ, o leque inclinado e a luz no centro da bola
		// só com a direção
		H->HitIncoming = D;
		H->HitNormal = N;
		H->HitTiltDeg = In.TiltDeg;
		if (bDir)
		{
			H->HitAim = L;
			H->HitLightAtCm = bBlast ? Point + L * (0.7 * BallCm) : FVector::ZeroVector;
		}
		H->bHitJets = On(Sparks);
		if (On(Embers))
		{
			static const int32 EmberCap[4] = { 4, 8, 16, 32 };
			const double Now = World->GetTimeSeconds();
			EmberUntil.RemoveAll([Now](double Until) { return Until < Now; });
			const int32 Want = FMath::Clamp(FMath::RoundToInt32(2.0f * Kh * Kh), 1, 4);
			H->HitEmbers = FMath::Clamp(EmberCap[Pr] - EmberUntil.Num(), 0, Want);
			for (int32 k = 0; k < H->HitEmbers; ++k)
			{
				EmberUntil.Add(Now + 8.0);         // o voo (~2 s), a queima e os 2–4 s piscando no chão
			}
			// (a revisão da vida) o fio de fumaça de cada pedaço em voo é a fuligem em volume do RTSMissileSmoke (a sombra do mapa, a sombra no
			// chão e a luz da cena), no lugar da fita chapada, atrás da camada Smolder (o mesmo emissor do fiapo). (A 2ª revisão: a 1ª conta dava
			// ~50 puffs vivos por pedaço, ~800 a mais na patriot_video_v4 no Ultra, contra o "fiapo, ≤ 15 puffs" do plano §6) cada pedaço com fio
			// tem ~16 puffs vivos (RTSHitLife.cpp: um a cada 0,8 m, saindo em 1,05 s); por acerto, 1 pedaço com fio (2 a partir de 12 kg: o
			// Tomahawk e o ataque, K ≥ 1,2) e o teto do preset 8 Ultra, 4 Alta, 2 Média, nenhum na Baixa ("4, sem fiapo"): no pior caso ~130
			// puffs a mais no Ultra e ~65 na Alta. Os outros voam só com a cabeça em brasa
			if (On(Smolder))
			{
				static const int32 SmokeCap[4] = { 0, 2, 4, 8 };
				EmberSmokeUntil.RemoveAll([Now](double Until) { return Until < Now; });
				const int32 PerHit = Kh >= 1.2f ? 2 : 1;
				H->HitEmberSmoke = FMath::Clamp(SmokeCap[Pr] - EmberSmokeUntil.Num(), 0, FMath::Min(H->HitEmbers, PerHit));
				for (int32 k = 0; k < H->HitEmberSmoke; ++k)
				{
					EmberSmokeUntil.Add(Now + 3.6);    // a queima (até 2,5 s) e o fio saindo (1,05 s): o teto vale pros puffs vivos, não só pros pedaços
				}
			}
		}
		H->StartMissileHit(Point, bDir ? L : Legacy, int32(Variant * 2654435761u >> 1), Kh, false);
	}
	// (1) os brilhos dos estilhaços nas faces do alvo viradas pra bola (6 | 12, teto 16, até 1,5·K m do ponto); os dos vizinhos vêm do jogo
	if (Victim && bOnHull && On(Glints))
	{
		HullGlints(Victim, bBlast ? BallCentre : Point + L * 100.0, Point, 150.0f * Kh, FMath::Clamp(FMath::RoundToInt32(6.0f * Kh * Kh), 1, 16),
			Variant ^ 0x6117u);
	}
	// (11) o míssil no chão: a terra e os torrões do tiro na areia na escala K (os tamanhos × K, as quantidades × K² com o teto do preset;
	// no ataque, as velocidades × √K: o regime da gravidade), no cone de 45–50° do chão (Tsujido/Arakawa: a cortina a 50–58°), no rumo do
	// míssil; os maiores torrões (8 no Ultra, 6 na Alta, 3 na Média) levantam o respingo de areia no 1º toque (a fila de pousos). O 2º míssil
	// no mesmo buraco levanta a terra escura (a v8 aprovada do tiro na areia, a cortina sim_chao_cortina na distância escalada)
	if (!Victim && On(Ground))
	{
		const bool bStrikeScale = In.bStrike && On(Strike);
		const float EjectaKg = bStrikeScale && In.EjectaKg > 0.0f ? In.EjectaKg : (WarheadKg > 0.0f ? WarheadKg : 4.0f);
		const float Ke = K(EjectaKg);
		static const int32 LandPuffs[4] = { 0, 3, 6, 8 };
		const FVector2D P2(Point.X, Point.Y);
		const double HoleCm = 250.0 * Ke;
		const bool bHole = GroundCraters.ContainsByPredicate([&P2, HoleCm](const FVector2D& C) { return FVector2D::DistSquared(C, P2) < HoleCm * HoleCm; });
		const float CurtainS = WarheadRingScale(WarheadScale(EjectaKg));
		if (!bSandGround)
		{
			// (25/10, o impacto pelo chão) fora da areia, o chão do tipo na escala da carga (as peças do tiro no tipo, GroundHitTyped: o pó
			// do tipo na distância escalada, a cortina de terra, os torrões e as lascas), sem a areia; sem o tipo (o miolo sem areia solta
			// no chão do mapa), o duro: o pó cinza curto e as lascas do asfalto
			FGroundResponse Rr = Typed;
			if (!bTyped)
			{
				Rr.Kind = 1;
				Rr.Weight = 1.0f;
				Rr.Dust = FLinearColor(0.45f, 0.43f, 0.40f, 0.6f);
				Rr.DustK = 0.5f;
				Rr.Clods = FLinearColor(0.035f, 0.035f, 0.035f);
				Rr.Trail = FLinearColor(1.3f, 1.3f, 1.3f);
				Rr.Chips = FLinearColor(0.05f, 0.05f, 0.05f);
				Rr.ChipCount = 22;
				Rr.SparkK = 1.8f;
			}
			FTypedScale Sc;
			Sc.SizeK = CurtainS / GroundHitSize;
			Sc.DirtK = Ke;
			Sc.SpeedK = bStrikeScale ? FMath::Sqrt(Ke) : 1.0f;
			Sc.ElevDeg = 47.5f;
			Sc.Puffs = LandPuffs[FMath::Clamp(Pr, 0, 3)];
			bool bTypedLinger = false;
			GroundHitTyped(GroundPt, FromCm, Variant, bHole, Rr, &Sc, &bTypedLinger);
			R.bBakeLinger = R.bBakeLinger || bTypedLinger;
			if (Knobs.bLog)
			{
				UE_LOG(LogTemp, Display, TEXT("BlastFx míssil no chão sem areia: tipo %d (peso %.2f), areia no miolo %.2f e na área %.2f, cortina × %.2f, torrões × %.2f"),
					Rr.Kind, Rr.Weight, SandCore, SandArea, Sc.SizeK, Ke);
			}
		}
		else if (ABlastDebris* Dirt = World->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
		{
			Dirt->GlowScale = GroundImpactGlow;
			Dirt->TrailTint = FLinearColor(1.0f, 0.82f, 0.62f, 1.0f);   // × o albedo 0,18 do rastro: terra marrom-escura (o GroundHit)
			Dirt->DirtK = Ke;
			Dirt->DirtSpeedK = bStrikeScale ? FMath::Sqrt(Ke) : 1.0f;
			Dirt->DirtElevDeg = 47.5f;
			// (a areia do acerto) os torrões maiores que levantam o respingo de areia no 1º toque: 8 no Ultra, 6 na Alta, 3 na Média, 0 na Baixa
			Dirt->DirtPuffs = LandPuffs[FMath::Clamp(Pr, 0, 3)];
			Dirt->bDirtOnly = true;
			Dirt->StartGroundImpact(Point, FVector(D.X, D.Y, 0.0), int32(Variant * 3266489917u >> 1));
		}
		GroundCraters.Add(P2);
		if (GroundCraters.Num() > 512)
		{
			GroundCraters.RemoveAt(0, GroundCraters.Num() - 512);
		}
		if (bSandGround && bHole && CurtainS > 0.0f && Rm && Rm->SourceDuration(FName(GroundCurtainSource)) > 0.0f)
		{
			// a cortina na escala da carga (o tamanho ∝ R, o tempo ∝ √R: Housen e Holsapple, o regime da gravidade) e a mesma τ do tiro
			SpawnDust(FVector(Point.X, Point.Y, GroundZCm), CurtainS, GroundHitSize * FMath::Sqrt(CurtainS / GroundHitSize), GroundCurtainOpacity, 1,
				Variant ^ 0xC07u, GroundCurtainDirt, GroundCurtainSource, GroundCurtainThickness / CurtainS, GroundCurtainDirt);
		}
	}
	MarkVictim();
	if (bKill)
	{
		EndDamaged(Victim);
		StartWreck(Victim);
	}
	if (Out)
	{
		*Out = R;
	}
	return BallCentre;
}

void UBlastFxDirector::AirBurst(const FVector& AtCm, const FVector& FromCm, AActor* Victim, float Kg, uint32 Variant)
{
	// (30/10, item 38) a ogiva no ar: a Patriot de estilhaço (a PAC-2) e o Stinger. A bola nasce pelo pé meia bola abaixo do ponto (a
	// gravação é a de uma carga no chão: o miolo fica no ponto do acerto); a saia baixa dela vira o gás da carga (bGasSkirt) e não há anel
	// (não tem areia no ar). Os estilhaços e o clarão pelo K da carga (a de 4 kg = 1, ∝ ∛kg), sem o brilho em sprite
	UWorld* World = GetWorld();
	if (!ShotFxEnabled() || !World || Kg <= 0.0f || !LoadAssets())
	{
		return;
	}
	const uint32 V = Variant * 2654435761u ^ (++ShotSerial) * 0x9E3779B9u;
	const float BallCm = CpFireballM * WarheadScale(Kg) * WarheadKnobs().Scale * 100.0f;
	FWarheadLife Life;
	Life.bGasSkirt = true;
	Life.bNoRing = true;
	SpawnWarhead(AtCm - FVector(0.0, 0.0, 0.5 * BallCm), Victim, Kg, V, false, &Life);
	if (ABlastDebris* Fx = World->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		const FVector N = (FromCm - AtCm).GetSafeNormal();
		Fx->GlowScale = ImpactGlow;
		Fx->StartMissileHit(AtCm, N.IsNearlyZero() ? FVector::UpVector : N, int32(V >> 1), FMath::Clamp(FMath::Pow(Kg / 4.0f, 1.0f / 3.0f), 0.42f, 1.6f), false);
	}
	UE_LOG(LogTemp, Log, TEXT("BlastFx ogiva no ar: %.1f kg em (%.0f, %.0f, %.0f) m, bola %.1f m"), Kg, AtCm.X / 100.0, AtCm.Y / 100.0, AtCm.Z / 100.0, BallCm / 100.0f);
}

bool UBlastFxDirector::SpawnWarhead(const FVector& FootCm, AActor* Victim, float Kg, uint32 Variant, bool bClassicLook, FWarheadLife* LifeOpt)
{
	// A ogiva v2 (23/10; o estudo e as medições: docs/ESTUDO_MISSIL_PATRIOT.md). Uma carga de W kg é a Carga Pesada (o mesmo
	// fenômeno: o explosivo sólido, a bola de fogo e a fuligem) na escala de Hopkinson-Cranz: comprimento E tempo ∝ W^⅓ (Baker; a
	// bola de 1 e 3 kg de TNT em túnel, a duração ∝ ∛W; Hargather: 1000× menos carga pede 10× mais quadros por segundo). Âncora: a
	// clássica do jogo é a carga de 20 kg, λ = 0,55·(W/20)^⅓ (a Patriot, 4 kg: 0,32, a bola de 3,2 m de diâmetro; o Tomahawk, 12 kg:
	// 0,46, 4,6 m). Dois relógios: o golpe e a bola quente (até ela desabar, 1,36 s da gravação, CpGlowEndS) no de Hopkinson-Cranz (λ);
	// a fumaça que sobe por empuxo no de Froude (√λ: a térmica sobe a w ∝ √(g′R), Morton, Taylor e Turner; a regra do cinema pra
	// pirotecnia em miniatura; a morte de tanque já usa, 0,59 = √0,35). A espessura 1 é a física (a mesma concentração, τ ∝ L): a
	// bola fica opaca (τ ~16–30 no centro); (23/10, a revisão) depois dela a fuligem afina pra o alvo seguir legível (SootKeep4Kg).
	// (23/10, a 2ª revisão) O anel de areia: o tiro de canhão na areia aprovado na distância escalada (WarheadRingScale).
	// Devolve se a bola de fogo aqui é de uma ogiva (a luz dos destroços segue a curva dela): a que nasceu, ou a acesa de antes em que
	// este acerto se fundiu. false: a bola é da morte ou da clássica recente em cima (elas acendem a própria luz), ou sem vaga
	const FWarheadKnobs& Knobs = WarheadKnobs();
	UWorld* World = GetWorld();
	if (!World || !Bakes.Num())
	{
		return false;
	}
	const float Lambda = WarheadScale(Kg) * Knobs.Scale;
	const float BallCm = CpFireballM * Lambda * 100.0f;
	const float RingS = WarheadRingScale(Lambda);
	const float RingCm = RingR90M * RingS * 100.0f;
	const FWarheadClock Clock = WarheadClockOf(Lambda);
	const float Ts = Clock.Ts, SlowAfter = Clock.SlowAfter, SlowTs = Clock.SlowTs;
	const float FireEnd = Clock.AgeAt(CpGlowEndS), FadeFrom = Clock.AgeAt(CpThinS), Life = Clock.AgeAt(CpValidS);
	const float ThinFrom = Clock.AgeAt(CpCoolS);
	const float SootKeep = Knobs.Keep >= 0.0f ? Knobs.Keep : SootKeep4Kg * FMath::Sqrt(4.0f / FMath::Max(Kg, 0.01f));
	const double Now = World->GetTimeSeconds();
	Warheads.RemoveAll([](const FWarhead& W) { return !W.E.IsValid(); });
	static const IConsoleVariable* EffectsQualityW = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.EffectsQuality"));
	const int32 Eq = EffectsQualityW ? EffectsQualityW->GetInt() : 3;
	const int32 Cap = WarheadCapFor(Eq);
	auto Say = [&](const TCHAR* Msg, int32 FreeN, int32 SeenN)
	{
		if (Knobs.bLog)
		{
			UE_LOG(LogTemp, Display, TEXT("BlastFx ogiva: %.1f kg em (%.1f, %.1f) m, λ %.3f (bola %.2f m), relógios %.3f e %.3f (bola acesa até %.2f s, a fuligem afina de %.2f s até %.2f, esmaece de %.2f a %.2f s), anel %.3f (R90 %.1f m); vagas %d (teto %d, ogivas %d, na tela %d): %s"),
				Kg, FootCm.X / 100.0, FootCm.Y / 100.0, Lambda, BallCm / 100.0f, Ts, SlowAfter > 0.0f ? SlowTs : Ts, FireEnd, ThinFrom, SootKeep, FadeFrom, Life, RingS,
				RingCm / 100.0f, FreeN, Cap, Warheads.Num(), SeenN, Msg);
		}
	};
	// a fusão: o mesmo alvo (ou o chão a menos de uma bola) com uma bola ainda acesa não ganha outra; cada míssil fica com o clarão e
	// as faíscas dele (no lugar da regra de 0,6 s por vítima: a rajada de 4 da Patriot, 0,25 s entre os acertos, dá 2 bolas)
	for (const FWarhead& W : Warheads)
	{
		if (W.bRing || W.RetireAt >= 0.0f || W.E->Age >= W.FireEndS)
		{
			continue;
		}
		if ((Victim && W.Victim.Get() == Victim) || FVector::DistSquared2D(W.FootCm, FootCm) < FMath::Square(double(FMath::Max(W.RadiusCm, BallCm))))
		{
			Say(TEXT("fundiu na bola acesa de antes"), 0, -1);
			return true;
		}
	}
	// em cima de uma morte ou de uma clássica do último 1 s (a metade pesada do ataque, 0,2 s antes, a poucos metros): a bola dela já
	// é a explosão ali (a mesma distância em que a morte tira as ogivas, RetireWarheadsNear: duas bolas no mesmo lugar). (23/10, a 2ª
	// revisão) false: a bola e a luz são dela; os destroços ficam com a queda do clarão
	for (const TPair<FVector2D, double>& Big : RecentBigBlasts)
	{
		if (Now - Big.Value < 1.0 && FVector2D::DistSquared(Big.Key, FVector2D(FootCm.X, FootCm.Y)) < FMath::Square(400.0 + BallCm))
		{
			Say(TEXT("em cima de uma morte ou clássica recente: só os destroços"), 0, -1);
			return false;
		}
	}
	// as vagas: o teto das ogivas (pelo preset) e as do raymarch fora a reserva das mortes
	const int32 OnScreen = RaymarchOnScreen();
	int32 Free = FMath::Min(Cap - Warheads.Num(), BlastRaymarch::MaxInstances - DeathReserve - OnScreen);
	const TCHAR* What = TEXT("o corpo e o anel");
	bool bCeded = false;
	if (Free <= 0)
	{
		// cheio: a ogiva mais velha já na fase B (a fumaça) sai esmaecendo e esta nasce só com o corpo; sem nenhuma pra ceder, só os
		// destroços (nunca estala: a que sai, sai em 0,25 s; a reserva das mortes segura o excesso desse tempo)
		if (Cap > 0 && OnScreen < BlastRaymarch::MaxInstances - 1 && RetireWarheads(1, false, true) > 0)
		{
			Free = 1;
			bCeded = true;
			What = TEXT("cheio: uma velha cedeu, só o corpo");
		}
		else
		{
			Say(TEXT("sem vaga: só os destroços"), Free, OnScreen);
			return false;
		}
	}
	// (23/10, a vida) a gravação dedicada do caso (o casco de lado, o teto, o chão, o chão do ataque), quando existe: o fenômeno inteiro
	// simulado (a bola achatando na face, a saia escorrendo pelo costado, o anel de areia mais forte do lado atingido, a coluna subindo
	// reta), girado só no rumo; tocada a (W/W0)^⅓ no tamanho e no relógio (Hopkinson-Cranz), com a espessura física (-HitBakeThick=)
	const UBlastRaymarchSubsystem* RmOwn = World->GetSubsystem<UBlastRaymarchSubsystem>();
	const float OwnDur = LifeOpt && !LifeOpt->Source.IsNone() && RmOwn ? RmOwn->SourceDuration(LifeOpt->Source) : 0.0f;
	const bool bOwn = OwnDur > 0.0f;
	const float BodyScale = bOwn ? FMath::Max(LifeOpt->SourceScale, 0.01f) : Lambda;
	AExplosionVolume* Body = Spawn(FootCm, Bakes[Variant % uint32(Bakes.Num())], BodyScale, VariantFraction(Variant, 0xB0B0), false);
	if (!Body)
	{
		return false;
	}
	// sem a luz própria (a luz da ogiva é a dos destroços, com a compensação pela exposição) e com o clarão do raymarch (o miolo
	// branco-quente nos primeiros ~0,1 s da gravação: 32 ms na Patriot, 46 no Tomahawk)
	Body->bFlash = Knobs.bFlash;
	Body->TimeScale = Ts;
	Body->SlowAfterS = SlowAfter;
	Body->SlowTimeScale = SlowAfter > 0.0f ? SlowTs : 0.0f;
	Body->Thickness = Knobs.Thick < 0.0f ? 1.0f / Lambda : Knobs.Thick;
	// o fim da gravação que vale (a idade) e o esmaecer de hoje (a Carga Pesada: de 2,0 a 3,2 s da gravação); a dedicada: até 98% dela e o
	// esmaecer nos últimos 37,5% (a mesma proporção)
	float ValidS = Life, FadeS = FadeFrom, HoldRawS = CpValidS;
	if (bOwn)
	{
		static const float OwnThick = CmdFloat(TEXT("HitBakeThick="), 1.0f);
		Body->RaymarchSource = LifeOpt->Source;
		Body->TimeScale = BodyScale * Knobs.Time;
		Body->SlowAfterS = 0.0f;
		Body->SlowTimeScale = 0.0f;
		Body->Thickness = OwnThick;
		HoldRawS = 0.98f * OwnDur;
		ValidS = HoldRawS * Body->TimeScale;
		FadeS = 0.625f * ValidS;
		// (a 2ª revisão da vida) o fim do volume pela idade, não pela Duration() do UAsset da Carga Pesada (o Bakes[Variant] que o corpo
		// carrega), medida contra o relógio da dedicada: uma dedicada mais longa que ela sumia de uma vez antes do esmaecer (o raymarch
		// para de desenhar no IsFinished). A nuvem viva, quando ligada, sobrescreve abaixo
		Body->HoldAge = ValidS + 0.5f;
	}
	if (!bClassicLook)
	{
		// o visual aprovado dos tiros e das mortes no jogo: a fuligem carvão, a areia do mapa, o sol inteiro, a luz do céu física e a
		// areia que devolve o sol
		Body->RaymarchSoot = Knobs.Soot.X >= 0.0f ? Knobs.Soot : TankSoot;
		Body->RaymarchDust = TankDust;
		if (LifeOpt && LifeOpt->bGasSkirt && !bOwn)
		{
			// (a 2ª revisão da vida) o pé no teto do casco: a saia baixa da gravação é o jato de parede do gás da carga, não areia (aço não
			// levanta areia): a cor e o albedo da fuligem da própria bola
			Body->RaymarchDust = Body->RaymarchSoot;
		}
		else if (!bOwn)
		{
			// (25/10, o impacto pelo chão) o pé no chão que não é areia: a saia é a poeira do tipo (a neve, a terra) ou o mesmo jato de gás
			// do teto (o concreto, o asfalto, o barro)
			FVector4f Skirt;
			bool bGas = false;
			if (GroundSkirtAt(FootCm, Skirt, bGas))
			{
				Body->RaymarchDust = bGas ? Body->RaymarchSoot : Skirt;
			}
		}
		Body->RaymarchSunFull = TankSunFull;
		Body->RaymarchSkyGain = TankSkyGain;
		Body->RaymarchGround = TankGround;
	}
	else
	{
		// (23/10, a revisão) a metade leve do ataque de mísseis ao lado da pesada: a mesma gravação da clássica, então a mesma família
		// de luz e cor dela (a fuligem, o sol, o céu e o chão globais do raymarch, os da TankDeath bClassic), não os da morte de tanque
		Body->RaymarchSoot = Knobs.Soot;
		if (LifeOpt && LifeOpt->bGasSkirt && !bOwn)
		{
			// (a 2ª revisão da vida) o mesmo gás no teto, na fuligem da clássica (sem a pedida, a global do raymarch: BlastRaymarch.cpp,
			// SootColor e SootAlbedo)
			Body->RaymarchDust = Knobs.Soot.X >= 0.0f ? Knobs.Soot : FVector4f(0.55f, 0.545f, 0.535f, 0.30f);
		}
		else if (!bOwn)
		{
			// (25/10, o impacto pelo chão) o mesmo fora da areia, na família da clássica
			FVector4f Skirt;
			bool bGas = false;
			if (GroundSkirtAt(FootCm, Skirt, bGas))
			{
				Body->RaymarchDust = bGas ? (Knobs.Soot.X >= 0.0f ? Knobs.Soot : FVector4f(0.55f, 0.545f, 0.535f, 0.30f)) : Skirt;
			}
		}
	}
	Body->RaymarchYawRad = 2.0f * PI * VariantFraction(Variant, 0x7A3D);   // o fenômeno é simétrico: o giro é variedade de graça
	if (LifeOpt && LifeOpt->bYaw)
	{
		Body->RaymarchYawRad = LifeOpt->YawRad;                         // (23/10, a vida) o giro pelo rumo de ℓ (a gravação dedicada: o lobo em +X)
	}
	Body->Opacity = 1.0f;
	{
		FWarhead& W = Warheads.AddDefaulted_GetRef();
		W.E = Body;
		W.Victim = Victim;
		W.FootCm = FootCm;
		W.RadiusCm = BallCm;
		W.BallCm = BallCm;
		W.FireEndS = FireEnd;
		W.FadeFromS = FadeS;
		W.LifeS = ValidS;
		W.ThinFromS = ThinFrom;
		// (23/10 noite) a dedicada já traz a fuligem afinada dentro dela, só a fuligem fria, no relógio do fogo dela (soot_keep.py: o keep do
		// Tomahawk, 0,104): a opacidade do volume multiplica a fuligem E a areia (BlastRaymarch.usf, InstMisc.z em F.rb), então afinar aqui
		// apagava a areia que o choque levanta no pico dela. A Carga Pesada em escala segue afinando pela opacidade (a areia dela é a saia
		// dentro da bola)
		W.SootKeep = bOwn ? 1.0f : FMath::Clamp(SootKeep, 0.0f, 1.0f);
		if (LifeOpt)
		{
			// (23/10, a vida) a bola anda ao longo de ℓ (a dedicada já tem a direção dentro da simulação: não anda) e a nuvem viva: o último
			// quadro válido fica parado (não sobe quadro novo pra placa; o ruído segue andando, ExplosionVolume HoldExtraS) e a nuvem sobe e
			// alarga como térmica até o fim, com o esmaecer de hoje passado pro fim do trecho
			W.FootBase = FootCm;
			W.MoveCm = bOwn ? FVector::ZeroVector : LifeOpt->MoveCm;
			W.MoveTauS = FMath::Max(LifeOpt->MoveTauS, 0.02f);
			W.bMove = !W.MoveCm.IsNearlyZero();
			W.Scale0 = BodyScale;
			W.CloudLambda = Lambda;
			W.Thick0 = Body->Thickness;
			if (LifeOpt->bCloud && LifeOpt->CloudExtraS > 0.0f)
			{
				W.bCloud = true;
				W.HoldAgeS = ValidS;
				W.LifeS = ValidS + LifeOpt->CloudExtraS;
				// (31/10) a diluição desde a parada até o fim (UpdateWarheads: a RaymarchDilute), não nos últimos 37,5%: a nuvem rareia aos poucos
				// enquanto sobe e abre; a continuação da areia da dedicada (OutOwnFadeS, abaixo) nasce junto
				W.FadeFromS = ValidS;
				Body->BakeHoldS = HoldRawS;
				Body->HoldAge = W.LifeS + 0.5f;
				LifeOpt->OutCloudEndS = W.LifeS;
			}
			if (bOwn)
			{
				// (a revisão) a dedicada tem a areia que o choque levanta: o jogo põe a continuação dela a partir do esmaecer (FHitOut::bOwnSand)
				LifeOpt->bOutOwn = true;
				LifeOpt->OutOwnFadeS = W.FadeFromS;
			}
		}
	}
	// o anel de areia (a primeira camada a cair, a que mais sobe quadros pra placa): só com 2 vagas, abaixo do teto de anéis vivos
	// (blast.MissileBlastRings), sem outro anel recente perto (a rajada no mesmo lugar) e sem uma morte ou clássica em cima no último
	// 1 s (ela já levanta a areia dela). No visual da clássica não há anel: a areia da Carga Pesada fica dentro da bola dela (a borda
	// da areia baixa a 0,65–0,93× a da bola, medido), e a metade leve do ataque fica da mesma família da pesada ao lado
	const UBlastRaymarchSubsystem* Rm = World->GetSubsystem<UBlastRaymarchSubsystem>();
	const float RingDur = Rm ? Rm->SourceDuration(FName(GroundSimSource)) : 0.0f;
	int32 RingsAlive = 0;
	for (const FWarhead& W : Warheads)
	{
		RingsAlive += W.bRing && W.E.IsValid() && W.RetireAt < 0.0f ? 1 : 0;
	}
	// (23/10, a vida) a gravação dedicada já tem a areia que o choque levanta (sem o anel de cima dela); com a direção, o anel nasce na
	// projeção do ponto no chão, ~0,3 R pra fora, e gira por ℓ. (A revisão) Uma representação só da areia que o choque levanta: com o véu do
	// jogo (FHitIn::bGameVeil: a mesma areia na poeira em volume, com o vazio atrás do casco e nada embaixo dele) ou com a strike_chao, sem o
	// anel (era a 2ª cópia: até ~8 m na Patriot e ~11,6 m no Tomahawk por baixo do véu de 7–11 m, nascendo no mesmo instante)
	const FVector RingFoot = LifeOpt && LifeOpt->bRingFoot ? LifeOpt->RingFootCm : FootCm;
	const bool bRingOwnedElsewhere = LifeOpt && LifeOpt->bNoRing;
	bool bRing = !bClassicLook && !bOwn && !bRingOwnedElsewhere && Free >= 2 && RingS > 0.0f && RingDur > 0.0f && RingsAlive < WarheadRingCapFor(Eq);
	for (int32 k = 0; bRing && k < Warheads.Num(); ++k)
	{
		const FWarhead& W = Warheads[k];
		bRing = !(W.bRing && W.E.IsValid() && W.E->Age < W.FireEndS && FVector::DistSquared2D(W.FootCm, FootCm) < FMath::Square(0.5 * RingCm));
	}
	for (int32 k = 0; bRing && k < RecentBigBlasts.Num(); ++k)
	{
		bRing = !(Now - RecentBigBlasts[k].Value < 1.0 && FVector2D::DistSquared(RecentBigBlasts[k].Key, FVector2D(FootCm.X, FootCm.Y)) < FMath::Square(double(RingCm)));
	}
	// (25/10, o impacto pelo chão) o anel é a areia que o choque levanta ali: no chão sem ela, o pó do tipo (o cinza do concreto, o da
	// neve) no tamanho dele, ou nada (o barro, o concreto limpo do mapa); na areia, o de hoje
	float RingO = GroundSimOpacity;
	FVector4f RingDust = TankDust;
	bool bRingNoGround = false;
	if (bRing && !GroundDustAt(FVector(RingFoot.X, RingFoot.Y, FootCm.Z), RingO, RingDust))
	{
		bRing = false;
		bRingNoGround = true;
	}
	if (!bRing)
	{
		if (bRingNoGround)
		{
			What = TEXT("só o corpo (o chão ali não tem areia pro anel)");
		}
		else if (Free >= 2)
		{
			What = bClassicLook ? TEXT("só o corpo (o visual da clássica: sem anel)")
				: bRingOwnedElsewhere ? TEXT("só o corpo (a areia do choque é o véu do jogo ou a strike_chao: sem anel)")
				: TEXT("só o corpo (o anel não: o teto de anéis, outro anel perto, uma morte em cima ou sem a gravação)");
		}
		else if (!bCeded)
		{
			What = TEXT("só o corpo (1 vaga)");
		}
	}
	else if (AExplosionVolume* Ring = Spawn(RingFoot, Bakes[(Variant ^ 0x2B1u) % uint32(Bakes.Num())], RingS, VariantFraction(Variant, 0xD057), false))
	{
		// a sim_chao_anel na escala do anel: o tempo também ∝ o tamanho (Hopkinson-Cranz: a chegada do choque; e a queda dos grãos na
		// velocidade terminal, de uma altura ∝ ao tamanho), a espessura que dá o mesmo τ do anel aprovado em qualquer escala (a poeira
		// levantada por área na mesma distância escalada é da areia, não da carga)
		Ring->RaymarchSource = FName(GroundSimSource);
		Ring->TimeScale = GroundSimTime * RingS / GroundSimScale;
		Ring->Thickness = GroundSimThickness * GroundSimScale / RingS;
		Ring->Opacity = 0.0f;                           // entra pela gravação (UpdateWarheads, como as areias; RaymarchOnScreen já o conta)
		Ring->bFlash = false;
		Ring->RaymarchSoot = TankSoot;
		Ring->RaymarchDust = RingDust;
		Ring->RaymarchSunFull = TankSunFull;
		Ring->RaymarchSkyGain = TankSkyGain;
		Ring->RaymarchGround = TankGround;
		Ring->RaymarchYawRad = LifeOpt && LifeOpt->bYaw ? LifeOpt->YawRad : 2.0f * PI * VariantFraction(Variant, 0x41A9);
		FWarhead& W = Warheads.AddDefaulted_GetRef();
		W.E = Ring;
		W.Victim = Victim;
		W.FootCm = RingFoot;
		W.bRing = true;
		W.RadiusCm = RingCm;
		W.BallCm = BallCm;
		W.FireEndS = 0.8f * Ring->TimeScale;            // a janela em que outro anel não nasce perto dele
		W.Duration = FMath::Min(RingDur, RingSeenEndS);
		W.Opacity0 = RingO;
	}
	Say(What, Free, OnScreen);
	return true;
}

void UBlastFxDirector::UpdateWarheads()
{
	// o corpo: a fuligem afina com a bola esfriando (de 0,8 s da gravação até o fim da fase A, pra SootKeep: a leitura do alvo), esmaece
	// de 2,0 a 3,2 s da gravação e sai; o anel entra pela gravação dele e sai no fim da parte que se vê (0,8–1,2 s da gravação); a que
	// cede a vaga sai em 0,25 s, sobre o que já estava esmaecendo (sem pular de volta)
	for (int32 I = Warheads.Num() - 1; I >= 0; --I)
	{
		FWarhead& W = Warheads[I];
		AExplosionVolume* E = W.E.Get();
		if (!E)
		{
			Warheads.RemoveAtSwap(I);
			continue;
		}
		const float Age = E->Age;
		float Op = 1.0f;
		bool bDone = false;
		if (W.bRing)
		{
			const float T = E->BakeTime();
			bDone = T >= W.Duration;
			Op = W.Opacity0 * (1.0f - FMath::SmoothStep(FMath::Min(RingFadeFromS, 0.75f * W.Duration), W.Duration, T)) * FMath::SmoothStep(0.0f, 0.25f, T);
		}
		else
		{
			bDone = Age >= W.LifeS;
			// (31/10, a revisão: "a fumaça preta do acerto morre em ~2 s virando uma rosquinha", v10_v4_siege 16–18 s) a nuvem viva acaba pela
			// diluição, não pela opacidade. A Opacity é a Keep do raymarch, que multiplica a densidade ANTES da erosão da borda (BlastRaymarch.usf,
			// SampleInst: F.r − Erode/(1 + 9·F.r)): levá-la a 0 em 0,7–0,8 s era um limiar subindo, e o miolo da gravação parada (vista de cima,
			// o eixo da coluna e o topo dela no teto do domínio, onde o BoxFade já afina: a fuligem mais rala) sumia primeiro, ficando o anel
			// preto com a areia no meio. Fumaça de verdade não esvazia por dentro: a mesma massa se espalha e τ cai por igual. Na nuvem viva a
			// opacidade fica no patamar da fuligem afinada (SootKeep, a borda gravada parada onde estava) e o fim vai na RaymarchDilute, × a
			// densidade depois da erosão (abaixo)
			const float CloudFade = 1.0f - FMath::SmoothStep(W.FadeFromS, W.LifeS, Age);
			Op = W.Opacity0 * FMath::Lerp(1.0f, W.SootKeep, FMath::SmoothStep(W.ThinFromS, W.FireEndS, Age)) * (W.bCloud ? 1.0f : CloudFade);
			if (!bDone && (W.bMove || W.bCloud))
			{
				// (23/10, a vida) a nuvem continua viva: do último quadro válido em diante (o quadro parado), ela sobe e alarga como uma
				// térmica auto-semelhante a partir do pé, z ∝ t^½ e r ∝ z: o tamanho × √(t/t₁) (Morton, Taylor e Turner; bate com Church
				// 1969), +8,8 m na Patriot e +12,8 m no Tomahawk, 1,7× mais larga; a massa da fuligem se conserva, então a espessura ÷ o
				// cubo do crescimento (τ ÷ 2,9 no fim)
				const float Dt = FMath::Max(Age - W.LastAge, 0.0f);
				W.LastAge = Age;
				float Grow = 1.0f;
				if (W.bCloud && W.HoldAgeS > 0.0f && Age > W.HoldAgeS)
				{
					Grow = FMath::Sqrt(FMath::Min(Age, W.LifeS) / W.HoldAgeS);
					E->SizeScale = W.Scale0 * Grow;
				}
				if (W.bCloud)
				{
					// (31/10) a massa da fuligem se conserva (a densidade ÷ o cubo do crescimento) e a mistura com o ar a leva a zero no fim
					// (CloudFade): os dois na diluição do raymarch, depois da erosão, e por ela também no cache de luz e na sombra no sol (a
					// Thickness de antes só valia na vista e no mapa de sombra do chão: a nuvem que crescia seguia escura por dentro)
					E->RaymarchDilute = CloudFade / (Grow * Grow * Grow);
				}
				// e vai com o vento do mapa depois da bola acesa, mais forte no alto (o perfil da fumaça do míssil, u·(z/10 m)^0,14), na
				// altura do meio da nuvem (~12·λ m na gravação da Carga Pesada; (a revisão) o λ da carga, também com a dedicada)
				if (W.bCloud && Age > W.FireEndS && WindSpeedMs > 0.0f && Dt > 0.0f)
				{
					const float Zc = FMath::Max(12.0f * W.CloudLambda * Grow, 2.0f);
					// (25/10) a mesma lei de todos (o perfil logarítmico, WindProfileK: a 2–20 m difere da potência 0,14 em menos de 4%)
					const float Wind = WindSpeedMs * WindProfileK(Zc);
					W.DriftCm += FVector(FMath::Cos(WindYawRad), FMath::Sin(WindYawRad), 0.0f) * (Wind * Dt * 100.0f);
				}
				// a bola anda ao longo de ℓ e para (1 − e^(−t/τ)): o que o míssil trazia; a gravação de chão segue de pé
				const FVector Foot = W.FootBase + W.MoveCm * (1.0 - FMath::Exp(-double(Age) / double(W.MoveTauS))) + W.DriftCm;
				if (!Foot.Equals(E->GetActorLocation(), 0.1))
				{
					E->SetActorLocation(Foot);
				}
				W.FootCm = Foot;
			}
		}
		if (W.RetireAt >= 0.0f)
		{
			bDone = bDone || Age >= W.RetireAt + WarheadRetireS;
			const float RetireOut = 1.0f - FMath::SmoothStep(W.RetireAt, W.RetireAt + WarheadRetireS, Age);
			if (W.bCloud)
			{
				E->RaymarchDilute *= RetireOut;      // (31/10) a nuvem viva que cede a vaga também sai pela diluição (sem o anel do limiar)
			}
			else
			{
				Op *= RetireOut;
			}
		}
		if (bDone)
		{
			E->Destroy();
			Warheads.RemoveAtSwap(I);
			continue;
		}
		E->Opacity = Op;
	}
	if (RecentBigBlasts.Num())
	{
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		RecentBigBlasts.RemoveAll([Now](const TPair<FVector2D, double>& B) { return Now - B.Value > 1.0; });
	}
}

int32 UBlastFxDirector::RaymarchOnScreen() const
{
	// o mesmo filtro do raymarch (BlastRaymarchSubsystem::Tick: viva, visível, desenhada por ele) e o cone dele
	const FRmView Rv = RmViewOf(GetWorld());
	int32 N = 0;
	for (TActorIterator<AExplosionVolume> It(GetWorld()); It; ++It)
	{
		N += RmDraws(*It, Rv) ? 1 : 0;
	}
	// (23/10, a revisão) e os anéis de ogiva que nasceram com opacidade 0 (entram pela gravação no próximo UpdateWarheads): o filtro
	// de cima não os vê, e uma morte ou um tiro no mesmo quadro contava uma vaga a menos
	for (const FWarhead& W : Warheads)
	{
		N += W.bRing && W.RetireAt < 0.0f && RmPending(W.E.Get(), Rv, W.Duration) ? 1 : 0;
	}
	return N;
}

int32 UBlastFxDirector::RetireWarheads(int32 Count, bool bNow, bool bPhaseBOnly)
{
	const FRmView Rv = RmViewOf(GetWorld());
	int32 Done = 0;
	while (Done < Count)
	{
		int32 Pick = INDEX_NONE;
		float Best = -1.0f;
		bool bPickPending = false;
		for (int32 k = 0; k < Warheads.Num(); ++k)
		{
			const FWarhead& W = Warheads[k];
			const AExplosionVolume* E = W.E.Get();
			if (!E || (!bNow && W.RetireAt >= 0.0f))
			{
				continue;
			}
			const bool bLate = E->Age >= W.FireEndS;
			if (!W.bRing && bPhaseBOnly && !bLate)
			{
				continue;
			}
			// (23/10, a 2ª revisão) o anel que nasceu com opacidade 0: o RaymarchOnScreen já conta a vaga dele e o raymarch ainda não o
			// desenhou. Ele vem primeiro e sai já nos dois modos (sem estalo nenhum: nunca apareceu); antes o tirar já o pulava e
			// tirava um corpo visível no lugar (o estalo), ou não liberava nada
			const bool bPending = W.bRing && W.RetireAt < 0.0f && RmPending(E, Rv, W.Duration);
			if (bNow && !bPending && !RmDraws(E, Rv))
			{
				continue;                               // tirar já só libera vaga se ela ocupa uma na tela
			}
			// a ordem: o anel ainda não desenhado, as que já estão saindo (só no tirar já), os anéis, os corpos na fase B, os outros; a
			// mais velha primeiro
			const float Score = (bPending ? 400.0f : 0.0f) + (W.RetireAt >= 0.0f ? 300.0f : 0.0f) + (W.bRing ? 200.0f : 0.0f) + (bLate ? 100.0f : 0.0f)
				+ FMath::Min(E->Age, 99.0f);
			if (Score > Best)
			{
				Best = Score;
				Pick = k;
				bPickPending = bPending;
			}
		}
		if (Pick == INDEX_NONE)
		{
			break;
		}
		if (bNow || bPickPending)
		{
			if (AExplosionVolume* Gone = Warheads[Pick].E.Get())
			{
				Gone->Destroy();
			}
			Warheads.RemoveAt(Pick);
		}
		else
		{
			Warheads[Pick].RetireAt = Warheads[Pick].E->Age;
		}
		++Done;
	}
	if (Done > 0 && WarheadKnobs().bLog)
	{
		UE_LOG(LogTemp, Display, TEXT("BlastFx ogiva: %d cedeu a vaga (%s), ficam %d volumes de ogiva"), Done,
			bNow ? TEXT("saiu já: passava de 32 na tela") : TEXT("esmaecendo em 0,25 s; o anel ainda não desenhado sai já"), Warheads.Num());
	}
	return Done;
}

void UBlastFxDirector::RetireWarheadsNear(const FVector& AtCm, float RadiusCm)
{
	// a morte em cima: a ogiva a menos de RadiusCm + a bola dela sai esmaecendo (a bola e o anel); (23/10, a 2ª revisão) o anel que
	// nasceu com opacidade 0 e ainda não foi desenhado sai já (sem estalo, e a vaga dele fica pra morte)
	int32 Done = 0;
	for (int32 I = Warheads.Num() - 1; I >= 0; --I)
	{
		FWarhead& W = Warheads[I];
		AExplosionVolume* E = W.E.Get();
		if (W.RetireAt < 0.0f && E && FVector::DistSquared2D(W.FootCm, AtCm) < FMath::Square(double(RadiusCm + W.BallCm)))
		{
			if (W.bRing && E->Opacity <= 0.0f)
			{
				E->Destroy();
				Warheads.RemoveAt(I);
			}
			else
			{
				W.RetireAt = E->Age;
			}
			++Done;
		}
	}
	if (Done > 0 && WarheadKnobs().bLog)
	{
		UE_LOG(LogTemp, Display, TEXT("BlastFx ogiva: %d saindo (uma morte nasceu em cima)"), Done);
	}
}

void UBlastFxDirector::MakeRoomForPriority(const FVector& FootCm, float Scale)
{
	// sem ogiva viva, nada muda (o caminho aprovado das areias e das mortes fica igual); a que nasce fora da tela não pega vaga
	if (Warheads.IsEmpty() || !GetWorld() || !RmSees(RmViewOf(GetWorld()), FootCm, Scale))
	{
		return;
	}
	int32 N = RaymarchOnScreen();
	const int32 MaxRm = BlastRaymarch::MaxInstances;
	// passando de 32, o raymarch corta pela distância (podia ser a morte ou o tiro): a ogiva sai já (a mais baixa na prioridade)
	if (N + 1 > MaxRm)
	{
		N -= RetireWarheads(N + 1 - MaxRm, true);
	}
	// entrando na reserva das mortes, a mais velha começa a sair (a reserva volta sem estalo)
	if (N + 1 > MaxRm - DeathReserve)
	{
		RetireWarheads(1, false);
	}
}

void UBlastFxDirector::DefenseAssist(const FVector& FromCm, const FVector& ToCm)
{
	if (!ShotFxEnabled() || !GetWorld())
	{
		return;
	}
	if (ABlastDebris* L = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		L->GlowScale = MissileGlow;
		L->StartLaser(FromCm, ToCm, 0.5f);
	}
}

void UBlastFxDirector::CarCrushStart(AActor* Car, const FVector& TankDirCm, float PassSeconds, uint32 Variant, bool bAlarm)
{
	if (!ShotFxEnabled() || !Car || !GetWorld())
	{
		return;
	}
	if (Crushes.ContainsByPredicate([Car](const FCrush& C) { return C.Car.Get() == Car; }))
	{
		return;
	}
	FCrush& C = Crushes.AddDefaulted_GetRef();
	C.Car = Car;
	const FBox Box = Car->GetComponentsBoundingBox(true);
	C.Center = Box.GetCenter();
	C.Fwd = Car->GetActorForwardVector().GetSafeNormal2D();
	C.Right = FVector::CrossProduct(FVector::UpVector, C.Fwd);
	// o tamanho pela caixa no eixo do carro (a caixa do mundo girada: o maior lado é o comprimento)
	const FVector Ext = Box.GetExtent();
	const float A = float(FMath::Abs(C.Fwd.X) * Ext.X + FMath::Abs(C.Fwd.Y) * Ext.Y);
	const float B = float(FMath::Abs(C.Right.X) * Ext.X + FMath::Abs(C.Right.Y) * Ext.Y);
	C.HalfLen = FMath::Clamp(FMath::Max(A, B), 150.0f, 320.0f);
	C.HalfWid = FMath::Clamp(FMath::Min(A, B), 70.0f, 120.0f);
	if (B > A)
	{
		Swap(C.Fwd, C.Right);                       // a malha com a frente no Y: o comprimento é o outro eixo
		C.Right = FVector::CrossProduct(FVector::UpVector, C.Fwd);
	}
	C.Height = float(FMath::Clamp(Box.Max.Z - Box.Min.Z, 100.0, 220.0));
	C.GroundZ = float(Box.Min.Z);
	C.TankDir = TankDirCm.GetSafeNormal2D();
	C.T0 = GetWorld()->GetTimeSeconds();
	C.Pass = FMath::Clamp(PassSeconds, 0.6f, 5.0f);
	C.Variant = Variant ^ (++ShotSerial) * 0x9E3779B9u;
	// a carcaça queimada (SM_civ_car_burnt e as amassadas dela): já sem vidro, lanterna, pneu nem bateria (nem alarme)
	TInlineComponentArray<UStaticMeshComponent*> Meshes(Car);
	for (const UStaticMeshComponent* M : Meshes)
	{
		if (M->GetStaticMesh() && M->GetStaticMesh()->GetName().Contains(TEXT("burnt")))
		{
			C.bBurnt = true;
		}
	}
	static const bool bLog = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxLog="), V); return V != 0; }();
	if (bLog)
	{
		UE_LOG(LogTemp, Display, TEXT("BlastFx carro: começo %s, centro %s, meio comprimento %.0f, meia largura %.0f, altura %.0f, queimado %d, passagem %.2f s"),
			*Car->GetName(), *C.Center.ToString(), C.HalfLen, C.HalfWid, C.Height, C.bBurnt ? 1 : 0, C.Pass);
	}
	if (bAlarm && !C.bBurnt)
	{
		if (ABlastDebris* L = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
		{
			L->StartBlink(C.Center + FVector(0.0, 0.0, 0.3 * C.Height), 10.0f + 20.0f * VariantFraction(C.Variant, 0xA1A1));
		}
	}
}

void UBlastFxDirector::CarCrushEnd(AActor* Car)
{
	for (FCrush& C : Crushes)
	{
		if (C.Car.Get() == Car)
		{
			C.bEnded = true;
		}
	}
}

void UBlastFxDirector::UpdateCrushes()
{
	if (!Crushes.Num() || !GetWorld())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 I = Crushes.Num() - 1; I >= 0; --I)
	{
		FCrush& C = Crushes[I];
		const float F = float((Now - C.T0) / C.Pass);
		if (C.bEnded || F > 1.2f)
		{
			static const bool bLogEnd = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxLog="), V); return V != 0; }();
			if (bLogEnd)
			{
				UE_LOG(LogTemp, Display, TEXT("BlastFx carro: fim (%s) na fração %.2f, etapas feitas 0x%x"), C.bEnded ? TEXT("CarCrushEnd") : TEXT("passagem"), F, C.Done);
			}
			Crushes.RemoveAtSwap(I);
			continue;
		}
		// o lado que o tanque pega primeiro: a ponta do carro virada pra ele
		const float Toward = FVector::DotProduct(C.Fwd, -C.TankDir) >= 0.0f ? 1.0f : -1.0f;
		const FVector Near = C.Center + C.Fwd * (Toward * C.HalfLen * 0.85f);
		const FVector Far = C.Center - C.Fwd * (Toward * C.HalfLen * 0.85f);
		const FVector Lamp(0.0, 0.0, 0.55 * C.Height - (C.Center.Z - C.GroundZ));
		auto Due = [&C, F](int32 Bit, float At)
		{
			if (!(C.Done & (1 << Bit)) && F >= At)
			{
				C.Done |= 1 << Bit;
				return true;
			}
			return false;
		};
		auto Debris = [this]() { return GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity); };
		// as lascas de uma cor (lanterna, borracha, ferrugem, tinta queimada)
		auto Chips = [&Debris](const FVector& At, const FVector& Dir, const FLinearColor& Tint, float Rough, int32 Count, float SizeMin, float SizeMax, float SpeedMin, float SpeedMax, int32 Seed)
		{
			if (ABlastDebris* L = Debris())
			{
				L->PlateTint = Tint;
				L->PlateRoughNow = Rough;
				L->StartChips(At, Dir, Count, SizeMin, SizeMax, SpeedMin, SpeedMax, Seed);
			}
		};
		const uint32 V = C.Variant;
		// 0: a lagarta encosta: a poeira do contato (rente, na cor do chão) e a ponta: as lanternas (vermelha e âmbar) ou,
		// na carcaça, a ferrugem e a tinta queimada que se soltam
		if (Due(0, 0.0f))
		{
			const AExplosionVolume* Dz = SpawnDust(FVector(Near.X, Near.Y, C.GroundZ), CrushDustScale, CrushDustTime, CrushDustOpacity, 0, V, CrushGray, nullptr, CrushThick);
			static const bool bLog0 = [] { int32 Vl = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxLog="), Vl); return Vl != 0; }();
			if (bLog0)
			{
				UE_LOG(LogTemp, Display, TEXT("BlastFx carro: etapa 0 em %s, poeira %s, poeiras vivas %d"), *Near.ToString(), Dz ? TEXT("nasceu") : TEXT("NAO nasceu"), Dusts.Num());
			}
			for (int32 k = 0; k < 2; ++k)
			{
				const FVector At = Near + Lamp + C.Right * ((k == 0 ? 0.7f : -0.7f) * C.HalfWid);
				if (C.bBurnt)
				{
					Chips(At, C.Fwd * Toward, k == 0 ? CrushRust : CrushChar, 0.85f, 6, 0.06f, 0.16f, 1.0f, 3.0f, int32(V * (17u + k)));
				}
				else
				{
					Chips(At, C.Fwd * Toward, k == 0 ? CrushLampRed : CrushLampAmber, 0.2f, 6, 0.08f, 0.2f, 1.5f, 4.0f, int32(V * (17u + k)));
				}
			}
		}
		// 0,2T: as laterais da frente cedem: os vidros estouram (piscando ao sol) ou, na carcaça, a cinza sai pelas janelas
		if (Due(1, 0.2f))
		{
			const FVector Front = C.Center + C.Fwd * (Toward * C.HalfLen * 0.25f);
			if (C.bBurnt)
			{
				SpawnDust(FVector(Front.X, Front.Y, C.GroundZ + 0.3 * C.Height), WindowDustScale, WindowDustTime, AshDustOpacity, 0, V ^ 0x3131u, CrushAsh, nullptr, CrushThick);
				Chips(Front + FVector(0.0, 0.0, 0.4 * C.Height), C.Right, CrushRust, 0.85f, 6, 0.03f, 0.1f, 1.0f, 2.5f, int32(V * 31u));
			}
			else
			{
				for (int32 k = 0; k < 2; ++k)
				{
					const float Side = k == 0 ? 1.0f : -1.0f;
					if (ABlastDebris* G = Debris())
					{
						G->GlowScale = GlassGlow;
						G->StartGlass(Front + C.Right * (Side * C.HalfWid) + FVector(0.0, 0.0, 0.25 * C.Height), C.Right * Side, int32(V * (31u + k)));
					}
				}
			}
		}
		// 0,35T e 0,8T: os eixos: o pneu estoura (um sopro de poeira e pedaços de borracha); na carcaça o aro só afunda a
		// areia (o sopro, sem borracha)
		for (int32 k = 0; k < 2; ++k)
		{
			if (Due(2 + k, k == 0 ? 0.35f : 0.8f))
			{
				const FVector Axle = k == 0 ? C.Center + C.Fwd * (Toward * C.HalfLen * 0.6f) : C.Center - C.Fwd * (Toward * C.HalfLen * 0.6f);
				SpawnDust(FVector(Axle.X, Axle.Y, C.GroundZ), TireDustScale, TireDustTime, C.bBurnt ? 0.5f * TireDustOpacity : TireDustOpacity, 0, V ^ (0x7173u + k), FVector4f(-1.0f, 0.0f, 0.0f, 0.0f), nullptr, CrushThick);
				if (!C.bBurnt)
				{
					Chips(Axle + C.Right * C.HalfWid + FVector(0.0, 0.0, 40.0), C.Right, CrushRubber, 0.9f, 4, 0.08f, 0.18f, 1.0f, 3.0f, int32(V * (41u + k)));
				}
			}
		}
		// 0,5T: o teto cede: o vidro de trás estoura e a poeira sai pelas janelas; na carcaça, a nuvem de cinza (a maior)
		if (Due(4, 0.5f))
		{
			if (C.bBurnt)
			{
				SpawnDust(FVector(C.Center.X, C.Center.Y, C.GroundZ + 0.3 * C.Height), AshDustScale, WindowDustTime, AshDustOpacity, 0, V ^ 0x5151u, CrushAsh, nullptr, CrushThick);
				Chips(C.Center + FVector(0.0, 0.0, 0.6 * C.Height), -C.TankDir, CrushChar, 0.85f, 6, 0.04f, 0.14f, 1.0f, 3.0f, int32(V * 53u));
			}
			else
			{
				if (ABlastDebris* G = Debris())
				{
					G->GlowScale = GlassGlow;
					G->StartGlass(Far + FVector(0.0, 0.0, 0.3 * C.Height), -C.Fwd * Toward, int32(V * 53u));
				}
				SpawnDust(FVector(C.Center.X, C.Center.Y, C.GroundZ + 0.3 * C.Height), WindowDustScale, WindowDustTime, WindowDustOpacity, 0, V ^ 0x5151u, WindowGray, nullptr, CrushThick);
			}
		}
		// 0,6T: a outra ponta (as lanternas ou a ferrugem)
		if (Due(5, 0.6f))
		{
			for (int32 k = 0; k < 2; ++k)
			{
				const FVector At = Far + Lamp + C.Right * ((k == 0 ? -0.7f : 0.7f) * C.HalfWid);
				if (C.bBurnt)
				{
					Chips(At, -C.Fwd * Toward, k == 0 ? CrushChar : CrushRust, 0.85f, 6, 0.06f, 0.16f, 1.0f, 3.0f, int32(V * (61u + k)));
				}
				else
				{
					Chips(At, -C.Fwd * Toward, k == 0 ? CrushLampRed : CrushLampAmber, 0.2f, 6, 0.08f, 0.2f, 1.5f, 4.0f, int32(V * (61u + k)));
				}
			}
		}
		// 0,65T: o casco raspa o teto (a faísca, fraca de dia; na carcaça é metal nu, sai igual)
		if (Due(6, 0.65f))
		{
			if (ABlastDebris* S = Debris())
			{
				S->GlowScale = MuzzleGlow;
				S->StartScrape(C.Center + FVector(0.0, 0.0, 0.5 * C.Height), -C.TankDir, int32(V * 71u));
			}
		}
	}
}

void UBlastFxDirector::UpdateCrushTest()
{
	// o teste do carro esmagado sem o jogo ligar a chamada (-CrushFxTest): o jogo troca a instância do carro por um ator
	// próprio (o "Prop") quando o esmaga; aqui ele é achado no quadro seguinte e o tanque mais perto dá a direção e a
	// velocidade. Só pra teste: varre os atores todo quadro
	static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("CrushFxTest"));
	UWorld* World = GetWorld();
	if (!bOn || !World)
	{
		return;
	}
	const float Dt = World->GetDeltaSeconds();
	TArray<AActor*> Tanks, Cars;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Ac = *It;
		if (const UStaticMeshComponent* M = Cast<UStaticMeshComponent>(Ac->GetRootComponent()); M && M->GetFName() == TEXT("Prop"))
		{
			const FString N = M->GetStaticMesh() ? M->GetStaticMesh()->GetName() : FString();
			if ((N.Contains(TEXT("civ_pickup")) || N.Contains(TEXT("civ_car_burnt"))) && !CrushTestSeen.Contains(Ac))
			{
				Cars.Add(Ac);
			}
			continue;
		}
		for (const UActorComponent* Cp : Ac->GetComponents())
		{
			if (Cp && Cp->GetFName() == TEXT("TurretPivot"))
			{
				Tanks.Add(Ac);
				break;
			}
		}
	}
	for (AActor* Tk : Tanks)
	{
		const FVector P = Tk->GetActorLocation();
		if (const FVector* Prev = CrushTestTankPos.Find(Tk); Prev && Dt > 0.0f)
		{
			float& Sp = CrushTestTankSpeed.FindOrAdd(Tk);
			Sp = FMath::Lerp(Sp, float(FVector::Dist2D(P, *Prev) / Dt), 0.2f);
		}
		CrushTestTankPos.Add(Tk, P);
	}
	for (AActor* Car : Cars)
	{
		CrushTestSeen.Add(Car);
		AActor* Best = nullptr;
		double BestD = 900.0 * 900.0;
		for (AActor* Tk : Tanks)
		{
			const double D = FVector::DistSquared2D(Tk->GetActorLocation(), Car->GetActorLocation());
			if (D < BestD)
			{
				BestD = D;
				Best = Tk;
			}
		}
		if (!Best)
		{
			continue;
		}
		const FVector Ext = Car->GetComponentsBoundingBox(true).GetExtent();
		const float Len = 2.0f * float(FMath::Max(Ext.X, Ext.Y));
		const float Speed = FMath::Max(CrushTestTankSpeed.FindRef(Best), 100.0f);
		const bool bAlarm = (CrushTestSeen.Num() & 1) != 0;
		UE_LOG(LogTemp, Display, TEXT("BlastFx carro: %s esmagado por %s a %.1f m/s, passagem %.2f s%s"), *Car->GetName(), *Best->GetName(),
			Speed / 100.0f, (Len + 450.0f) / Speed, bAlarm ? TEXT(", alarme") : TEXT(""));
		CarCrushStart(Car, Best->GetActorForwardVector() * 100.0, (Len + 450.0f) / Speed, uint32(CrushTestSeen.Num()) * 7919u, bAlarm);
	}
}

void UBlastFxDirector::WireArc(const FVector& PointCm, int32 Kind, uint32 Variant)
{
	if (!ShotFxEnabled() || !GetWorld())
	{
		return;
	}
	const uint32 V = Variant ^ (++ShotSerial) * 0x9E3779B9u;
	if (ABlastDebris* A = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		A->GlowScale = ArcGlow;
		A->PlateTint = FLinearColor(0.85f, 0.84f, 0.8f, 1.0f);   // as lascas do isolador: porcelana
		A->PlateRoughNow = 0.35f;
		A->StartArc(PointCm, Kind, int32(V));
	}
	if (Kind == 2)
	{
		const double GroundZ = BlastFxGround::HeightAtCached(GetWorld(), PointCm.X, PointCm.Y);
		SpawnDust(FVector(PointCm.X, PointCm.Y, GroundZ), ArcSmokeScale, ArcSmokeTime, ArcSmokeOpacity, 0, V ^ 0xA3Cu, ArcSmokeColor);
	}
}

void UBlastFxDirector::UpdateArcTest()
{
	// -ArcFxTestX= -ArcFxTestY= (m): a sequência do poste derrubado num ponto, sem o jogo chamar: o fio arrebenta a 8,5 m (4,0 s), a
	// ponta encosta na areia 1,5 m ao lado (4,5 s) e o religador tenta de novo a +2 s e +2,5 s (6,5 s e 9,0 s)
	static const FVector2D At = []
	{
		float X = 1.0e9f, Y = 1.0e9f;
		if (FParse::Value(FCommandLine::Get(), TEXT("ArcFxTestX="), X) && FParse::Value(FCommandLine::Get(), TEXT("ArcFxTestY="), Y))
		{
			return FVector2D(X * 100.0, Y * 100.0);
		}
		return FVector2D(1.0e9, 1.0e9);
	}();
	if (At.X > 1.0e8 || !GetWorld())
	{
		return;
	}
	static const float Times[] = { 6.0f, 6.5f, 8.5f, 11.0f };   // (o vídeo do teste começa ~4,5 s depois da partida)
	static const int32 Kinds[] = { 0, 1, 2, 2 };
	const double Now = GetWorld()->GetTimeSeconds();
	while (ArcTestStep < 4 && Now >= Times[ArcTestStep])
	{
		const int32 K = Kinds[ArcTestStep];
		const double Gz = BlastFxGround::HeightAtCached(GetWorld(), At.X, At.Y);
		const FVector P = K == 0 ? FVector(At.X, At.Y, Gz + 850.0) : FVector(At.X + 150.0, At.Y, Gz + 5.0);
		UE_LOG(LogTemp, Display, TEXT("BlastFx arco: tipo %d em %.0f s"), K, Now);
		WireArc(P, K, 0x5EEDu + uint32(ArcTestStep));
		++ArcTestStep;
	}
}

void UBlastFxDirector::MarkHull(UStaticMeshComponent* Part, const FVector& LocalCm, int32 Kind)
{
	static const bool bOn = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("HullMarks="), V); return V != 0; }();
	if (!bOn || !Part || !GetWorld())
	{
		return;
	}
	const float Now = FMath::Max(float(GetWorld()->GetTimeSeconds()), 0.01f);
	const TArray<float>& D = Part->GetCustomPrimitiveData().Data;
	auto At = [&D](int32 I) { return D.IsValidIndex(I) ? D[I] : 0.0f; };
	int32 Same = -1, Empty = -1, Oldest = -1;
	float OldestBorn = TNumericLimits<float>::Max();
	for (int32 k = 0; k < HullMarkSlots; ++k)
	{
		const int32 B = HullMarkBase + 4 * k;
		const float W = At(B + 3);
		const float K = FMath::FloorToFloat(W / 1.0e5f);
		const float Born = W - 1.0e5f * K;
		if (K >= 3.0f)
		{
			continue;                                // (30/10, item 31) a vaga de um furo do prédio (ARTSUnitActor::OpenStructureHole): fica
		}
		if (W <= 0.0f || Born > Now)
		{
			Empty = Empty < 0 ? k : Empty;          // vaga livre (ou de outra partida: o relógio voltou)
			continue;
		}
		if (int32(K) == Kind && FVector::Dist(FVector(At(B), At(B + 1), At(B + 2)), LocalCm) < (Kind == 2 ? 60.0f : HullMarkMergeCm))
		{
			Same = k;
			break;
		}
		if (Born < OldestBorn)
		{
			OldestBorn = Born;
			Oldest = k;
		}
	}
	const int32 Slot = Same >= 0 ? Same : (Empty >= 0 ? Empty : Oldest);
	if (Slot < 0)
	{
		return;
	}
	const int32 B = HullMarkBase + 4 * Slot;
	// o reforço fica no lugar da marca (a mesma cicatriz, de novo quente)
	const FVector P = Same >= 0 ? FVector(At(B), At(B + 1), At(B + 2)) : LocalCm;
	Part->SetCustomPrimitiveDataVector4(B, FVector4(P.X, P.Y, P.Z, Now + 1.0e5f * float(Kind)));
	// (01/11, a prova da fuligem do míssil: nada apareceu no v41_fuligem_missil) quem levou a marca, onde, e o material de base da peça
	static int32 Logged = 0;
	if (Logged < 40)
	{
		++Logged;
		const UMaterialInterface* Mi = Part->GetMaterial(0);
		UE_LOG(LogTemp, Log, TEXT("BlastFx marca: %s / %s tipo %d em (%.0f, %.0f, %.0f) cm, vaga %d, o material %s"),
			Part->GetOwner() ? *Part->GetOwner()->GetName() : TEXT("?"), *Part->GetName(), Kind, P.X, P.Y, P.Z, Slot,
			Mi && Mi->GetMaterial() ? *Mi->GetMaterial()->GetName() : TEXT("?"));
	}
}

namespace
{
	// o alcance das luzes pras fumaças (17/10): -SmokeLightReach= (1 = o raio da Unreal, o de antes)
	float SmokeLightReach()
	{
		static const float V = [] { float R = 3.0f; FParse::Value(FCommandLine::Get(), TEXT("SmokeLightReach="), R); return R; }();
		return V;
	}
}

void UBlastFxDirector::GatherSceneLights()
{
	// as luzes pontuais acesas da cena (as das explosões, do fogo da carcaça, dos tiros: o que se move ou acende), as 32
	// mais fortes até 250 m da câmera, em candelas (a Unreal converte as unidades), pra toda fumaça ler (RTSSceneLights)
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FVector CamM = FVector::ZeroVector;
	if (const APlayerController* Pc = World->GetFirstPlayerController(); Pc && Pc->PlayerCameraManager)
	{
		CamM = Pc->PlayerCameraManager->GetCameraLocation() / 100.0;
	}
	struct FCand { RTSSceneLights::FLight L; float Score; };
	TArray<FCand> Cands;
	for (TObjectIterator<UPointLightComponent> It; It; ++It)
	{
		const UPointLightComponent* P = *It;
		if (!P || P->GetWorld() != World || !P->IsRegistered() || !P->IsVisible() || P->Intensity <= 0.0f || P->Mobility == EComponentMobility::Static)
		{
			continue;
		}
		const float Cd = P->Intensity * ULocalLightComponent::GetUnitsConversionFactor(P->IntensityUnits, ELightUnits::Candelas);
		if (Cd <= 1.0f)
		{
			continue;
		}
		const FVector PosM = P->GetComponentLocation() / 100.0;
		const float D2 = float(FVector::DistSquared(PosM, CamM));
		if (D2 > 250.0f * 250.0f)
		{
			continue;
		}
		FCand& C = Cands.AddDefaulted_GetRef();
		C.L.PosM = PosM;
		// o alcance pras fumaças (17/10): o raio da Unreal é corte de desempenho (7–9 m nas luzes das explosões); a luz física
		// não acaba, e a fumaça a 6–20 m de uma morte à noite ficava sem o laranja. × 3: a janela (1 − (d/R)⁴)² fica ~1 até ~2 R
		C.L.RadiusM = P->AttenuationRadius / 100.0f * SmokeLightReach();
		C.L.Candela = P->GetLightColor() * Cd;
		C.Score = Cd / FMath::Max(D2, 100.0f);
	}
	Cands.Sort([](const FCand& A, const FCand& B) { return A.Score > B.Score; });
	// (17/10, pro A/B da noite) -SmokeLightUncomp=1: as fumaças recebem a luz sem a compensação da exposição dos destroços
	// (ExpoComp, presa a 1/16 de noite: a lua não leva nenhuma). Só a cópia das fumaças; o chão e as unidades seguem iguais
	static const bool bUncomp = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("SmokeLightUncomp="), V); return V != 0; }();
	if (bUncomp)
	{
		static const float RefExpo = [] { float V = 6.95e-5f; FParse::Value(FCommandLine::Get(), TEXT("DebrisRefExpo="), V); return V; }();
		const float E = BlastRaymarch::LastViewExposure();
		const float Comp = E > 0.0f ? FMath::Clamp(RefExpo / E, 1.0f / 16.0f, 4.0f) : 1.0f;
		for (FCand& C : Cands)
		{
			C.L.Candela /= Comp;
		}
	}
	TArray<RTSSceneLights::FLight> Out;
	for (int32 k = 0; k < Cands.Num() && k < RTSSceneLights::MaxLights; ++k)
	{
		Out.Add(Cands[k].L);
	}
	// -SceneLightsLog=1 (17/10, a prova da poeira acesa pela carcaça): a lista, uma vez por segundo
	static const bool bLightsLog = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("SceneLightsLog="), V); return V != 0; }();
	static double LastLightsLog = -10.0;
	if (bLightsLog && World->GetTimeSeconds() - LastLightsLog >= 1.0)
	{
		LastLightsLog = World->GetTimeSeconds();
		FString Txt;
		for (int32 k = 0; k < Out.Num() && k < 6; ++k)
		{
			Txt += FString::Printf(TEXT(" [(%.1f, %.1f, %.1f) m, raio %.1f m, cd (%.0f, %.0f, %.0f)]"), Out[k].PosM.X, Out[k].PosM.Y, Out[k].PosM.Z, Out[k].RadiusM,
				Out[k].Candela.R, Out[k].Candela.G, Out[k].Candela.B);
		}
		UE_LOG(LogTemp, Display, TEXT("BlastFx luzes da cena aos %.1f s: %d de %d candidatas;%s"), World->GetTimeSeconds(), Out.Num(), Cands.Num(), *Txt);
	}
	RTSSceneLights::Set_GameThread(MoveTemp(Out));
}

void UBlastFxDirector::UpdateShadowTest()
{
	// -ShadowFxTest=1 (16/10 noite, a prova da regra do CLAUDE.md: a fumaça recebe a sombra do mapa): tiros na areia em volta
	// da casa 22 da vila do World_TestSalt2 (-14, 32), pela direção do sol: dois no lado da sombra (a 9 e 16 m do meio da
	// casa) e um no lado do sol (a 12 m); o log escreve a visibilidade do sol do mapa comum (a consulta da Arte) em cada um,
	// a 0,5, 2 e 4 m. Com -RmDebug=4 a fumaça mostra a mesma visibilidade (branco ao sol, preto na sombra do mapa)
	// -ShadowFxTest=2 (a fase 2, uma fumaça na sombra da outra): uma morte de tanque 45 m da casa na direção do sol, aos
	// 7 s (o vídeo começa ~4,5 s depois da partida), e três tiros na areia na sombra da coluna dela (12, 20 e 30 m pro lado
	// da casa), aos 7,6, 8,2 e 8,8 s, enquanto a coluna está densa (a gravação eg_tq_v2 dura ~3 s). Com
	// -RmDebug=5 a fumaça mostra o sol pelas outras explosões; -RmSmokeShadow=0 é o A/B
	static const int32 Mode = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("ShadowFxTest="), V); return V; }();
	UWorld* World = GetWorld();
	if (Mode == 0 || !World)
	{
		return;
	}
	FVector ToSun = FVector(0.3, 0.3, 0.9).GetSafeNormal();
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		if (const UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(It->GetLightComponent()); L && L->IsUsedAsAtmosphereSunLight())
		{
			ToSun = -L->GetDirection();
		}
	}
	const FVector2D Away = -FVector2D(ToSun.X, ToSun.Y).GetSafeNormal();
	const FVector2D C(-14.0, 32.0);
	const FVector2D Points[] = { C + Away * 9.0, C + Away * 16.0, C - Away * 12.0 };
	const double Now = World->GetTimeSeconds();
	if (Mode == 5)
	{
		// -ShadowFxTest=5 (23/10, a prova da ogiva v2: a sombra do mapa, a sombra da outra fumaça e a luz da morte, na mesma cena e
		// câmera das provas 1–4): a morte de tanque 45 m da casa pro lado do sol aos 7,0 s; a Patriot (4 kg) na sombra da casa (9 m)
		// aos 7,4 s; o Tomahawk (12 kg) do lado do sol (12 m) aos 7,8 s; a Patriot na sombra da coluna da morte (12 m dela, pro lado
		// da casa) aos 8,2 s; o Tomahawk 8 m ao lado da morte (aceso pelo fogo dela) aos 8,6 s. Tudo de novo aos 13 s. Com
		// -RmDebug=4 a sombra do mapa, com -RmDebug=5 a da outra fumaça; -MissileBlastLog=1 escreve cada ogiva
		const FVector2D Death = C - Away * 45.0;
		const FVector2D Side(-Away.Y, Away.X);
		const FVector2D Where[] = { Death, C + Away * 9.0, C - Away * 12.0, Death + Away * 12.0, Death + Side * 8.0 };
		const float Kgs[] = { 0.0f, 4.0f, 12.0f, 4.0f, 12.0f };
		while (ShadowTestStep < 10 && Now >= (ShadowTestStep < 5 ? 7.0 : 13.0) + 0.4 * double(ShadowTestStep % 5))
		{
			const int32 K = ShadowTestStep % 5;
			const FVector2D P = Where[K] * 100.0;
			const double Gz = BlastFxGround::HeightAtCached(World, P.X, P.Y);
			if (Kgs[K] <= 0.0f)
			{
				TankDeath(FVector(P.X, P.Y, Gz), 0xDEADu + uint32(ShadowTestStep));
			}
			else
			{
				// o míssil chegando de longe, do alto (no chão a direção não muda nada: os destroços sobem)
				MissileHit(-1000 - ShadowTestStep, FVector(P.X, P.Y, Gz), FVector(P.X, P.Y - 4000.0, Gz + 2000.0), nullptr, float(Gz), false, Kgs[K]);
			}
			UE_LOG(LogTemp, Display, TEXT("BlastFx ogiva (bancada): %s em (%.1f, %.1f) aos %.1f s"), Kgs[K] <= 0.0f ? TEXT("a morte") : (Kgs[K] < 8.0f ? TEXT("a Patriot") : TEXT("o Tomahawk")),
				P.X / 100.0, P.Y / 100.0, Now);
			++ShadowTestStep;
		}
		return;
	}
	if (Mode == 4)
	{
		// -ShadowFxTest=4 (17/10, a bancada da carcaça em volume): a coluna sim_carcaca solta 30 m ao norte da casa 22 (do lado
		// do sol, em campo aberto), aos 5 s, por 30 s, girada pro vento, com a luz do fogo dela (os destroços sem as chamas)
		if (ShadowTestStep == 0 && Now >= 5.0)
		{
			++ShadowTestStep;
			const FVector2D P = (C - Away * 30.0) * 100.0;
			const double Gz = BlastFxGround::HeightAtCached(World, P.X, P.Y);
			if (AExplosionVolume* Col = SpawnDust(FVector(P.X, P.Y, Gz), 1.0f, 1.0f, WreckColumnOpacity, 1, 0xCA2CA5u, FVector4f(-1.0f, 0.0f, 0.0f, 0.0f),
				WreckColumnSource, WreckColumnThickness, WreckColumnSoot))
			{
				Dusts.RemoveAll([Col](const FDust& D) { return D.E.Get() == Col; });
				Col->HoldAge = 30.0f;
				Col->RaymarchEmit = WreckColumnEmit;
				Col->RaymarchYawRad = WindYawRad;
			}
			if (ABlastDebris* Fx = World->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
			{
				Fx->GlowScale = MuzzleGlow;
				Fx->StartWreckFire(30.0f, false);
				Fx->SetFirePos(FVector(P.X, P.Y, Gz + RingUpCm - 20.0));
			}
			UE_LOG(LogTemp, Display, TEXT("BlastFx carcaça (bancada): a coluna em (%.1f, %.1f), vento %.0f°"), P.X / 100.0, P.Y / 100.0, FMath::RadiansToDegrees(WindYawRad));
		}
		return;
	}
	if (Mode == 3)
	{
		// -ShadowFxTest=3 (a prova direta da fase 2): duas mortes de tanque na mesma reta do sol, a 8 m uma da outra, aos 7,0
		// e 7,3 s: a de baixo do sol (a 2ª) fica na sombra da 1ª. Com -RmDebug=5 a 2ª tem de sair escura do lado do sol
		const FVector2D B = C - Away * 45.0;
		while (ShadowTestStep < 2 && Now >= 7.0 + 0.3 * double(ShadowTestStep))
		{
			const FVector2D P = (B + Away * (8.0 * double(ShadowTestStep))) * 100.0;
			const double Gz = BlastFxGround::HeightAtCached(World, P.X, P.Y);
			TankDeath(FVector(P.X, P.Y, Gz), 0xDEADu + uint32(ShadowTestStep));
			UE_LOG(LogTemp, Display, TEXT("BlastFx sombra (fase 2, direta): morte %d em (%.1f, %.1f) aos %.1f s"), ShadowTestStep, P.X / 100.0, P.Y / 100.0, Now);
			++ShadowTestStep;
		}
		return;
	}
	if (Mode == 2)
	{
		const FVector2D B = C - Away * 45.0;
		const FVector2D Hits[] = { B + Away * 12.0, B + Away * 20.0, B + Away * 30.0 };
		while (ShadowTestStep < 4 && Now >= 7.0 + 0.6 * double(ShadowTestStep))
		{
			const FVector2D P = (ShadowTestStep == 0 ? B : Hits[ShadowTestStep - 1]) * 100.0;
			const double Gz = BlastFxGround::HeightAtCached(World, P.X, P.Y);
			if (ShadowTestStep == 0)
			{
				TankDeath(FVector(P.X, P.Y, Gz), 0xDEADu);
			}
			else
			{
				GroundHit(FVector(P.X, P.Y, Gz), FVector(P.X, P.Y - 4000.0, Gz + 300.0), 0xC0FFEEu + uint32(ShadowTestStep));
			}
			UE_LOG(LogTemp, Display, TEXT("BlastFx sombra (fase 2): %s em (%.1f, %.1f) aos %.1f s"), ShadowTestStep == 0 ? TEXT("a morte") : TEXT("tiro"),
				P.X / 100.0, P.Y / 100.0, Now);
			++ShadowTestStep;
		}
		return;
	}
	while (ShadowTestStep < 3 && Now >= 4.0 + 3.0 * ShadowTestStep)
	{
		const FVector2D P = Points[ShadowTestStep] * 100.0;
		const double Gz = BlastFxGround::HeightAtCached(World, P.X, P.Y);
		FString Vis;
		for (const double Hm : { 0.5, 2.0, 4.0 })
		{
			Vis += FString::Printf(TEXT(" %.0f m: %.2f"), Hm, RTSSunShadow::SunVisibilityCPU(World, FVector(P.X, P.Y, Gz) / 100.0 + FVector(0.0, 0.0, Hm), 0.2));
		}
		UE_LOG(LogTemp, Display, TEXT("BlastFx sombra: tiro %d em (%.1f, %.1f) aos %.1f s, sol pra (%.2f, %.2f, %.2f); a visibilidade do sol no mapa:%s"),
			ShadowTestStep, P.X / 100.0, P.Y / 100.0, Now, ToSun.X, ToSun.Y, ToSun.Z, *Vis);
		GroundHit(FVector(P.X, P.Y, Gz), FVector(P.X, P.Y - 4000.0, Gz + 300.0), 0xC0FFEEu + uint32(ShadowTestStep));
		++ShadowTestStep;
	}
}
