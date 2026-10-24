// (23/10) A explosão com vida, do lado do jogo: ver RTSHitLife.h e BlastFxHitLife.h.
#include "RTSHitLife.h"

#include "BlastFxDirector.h"
#include "BlastFxHitLife.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTSCameraPawn.h"
#include "RTSMissileSmoke.h"
#include "RTSTreadDust.h"
#include "RTSUnitActor.h"

// (23/10, a areia do acerto, caso 3) a areia da torre solta, fora das camadas da vida do acerto (a torre não é o míssil): o A/B dela. (A
// revisão) Presa ao -HitLifeV também: com -HitLifeV=1 (a de hoje, exata) ela não sai, como toda areia nova
static TAutoConsoleVariable<int32> CVarTurretSand(TEXT("rts.Turret.Sand"), 1,
	TEXT("A torre solta levanta areia ao bater e ao deslizar na areia (0 = só o empurrão na poeira que já está no ar e a marca; só com -HitLifeV=2, o padrão)"), ECVF_Default);

namespace
{
	bool TurretSandOn()
	{
		return BlastFxHitLife::Version() >= 2 && CVarTurretSand.GetValueOnGameThread() != 0;
	}

	// (5) o tranco: o de hoje (1,2° por passo na mola, ~1,5° de pico) é a âncora pra Patriot (4 kg) num tanque de 60 t; o resto pela física,
	// × (W/4 kg)·(60 t/M) (a quantidade de movimento do sopro sobre a massa: num tanque de 60 t, 0,2–0,3° com 4 kg e 0,6–1° com 12 kg; a
	// âncora porque o real não se lê a 70–120 m), até o teto de 5° de pico (o limite da mola, AccelPitchLimit do ZH): o pico ∝ o impulso
	constexpr float KickAnchorDeg = 1.2f, KickAnchorPeakDeg = 1.5f, KickPeakMaxDeg = 5.0f, KickRefKg = 4.0f, KickRefMassT = 60.0f;

	// ---- a areia do acerto (23/10; docs/ESTUDO_MISSIL_PATRIOT.md, "A areia do acerto (3 casos)") ----
	// A régua é a poeira das lagartas aprovada (densidade 0,08): o mesmo modelo óptico, a mesma VolDensity; o botão de cada caso é a massa
	// (o A de cada puff: τ do miolo ≈ 0,43·A·a coluna da gravação, linear no volume) e o tamanho. A lagarta lê porque 10–20 cartões se
	// empilham em cada ponto; um acerto não empilha, então cada puff leva a massa de vários (o "quase não muda" e o "parece chão duro"
	// eram o A 0,2 e os puffs de 0,5–3 m, ~0,3 do lado visível: medido na gravação, scratchpad sand/kickcol.py).
	//
	// (4) o véu do choque: a onda pequena que empurra a areia do chão pro lado. O grão de 200 µm (τp 15–30 ms) só ganha ≥ 0,5 m/s onde o
	// vento atrás do choque passa de 100 m/s (o pulso dura 3–9 ms: o ar anda 0,1–0,5 m); os finos (< 63 µm) seguem o ar até ~50 m/s. A
	// faixa densa vai da bola (R ≈ 1·W^⅓, LANL) até o vento de 100 m/s, a franja até o de 50 m/s, com a carga efetiva: 1,8·W do lado
	// atingido (o casco e o chão: a explosão na superfície, arXiv 2009.05674), 1,0·W no teto (a metade de baixo vai pra cima), 0,5·W atrás
	// do casco (a onda que contorna); embaixo do casco nada. Baixo (≤ 1,2 m), nasce na chegada do choque (r/400 s), sai a 2–4 m/s e para em
	// 0,5–1 m (o arrasto 4 da camada impact), some em 4·G s. O vento de 100 m/s cai a Z = 3,80 m/kg^⅓ e o de 50 m/s a Z = 5,76 (esta lei).
	// A densidade de puffs (a revisão: o texto de antes dizia 0,47 por m², faltava o π): 0,47·(R² − r²) puffs com o lado de 5 m, ou seja
	// 0,15 por m² de anel, × (5 m/lado)²; mais de 40 por véu, o lado cresce (a carga grande sem a gravação). Com o miolo visível de ~2,1 m
	// de um puff de A 4 (τ > 0,3, scratchpad sand/kickgain.py) a cobertura do anel fica ~0,5: o anel patchy, com falhas entre os puffs
	constexpr float VeilMass = 4.0f, VeilFringeMass = 1.5f, VeilSizeM = 5.0f, VeilBakeT0 = 0.5f, VeilLifeG = 4.0f, VeilMergeS = 1.5f;
	constexpr float VeilLayerDrag = 4.0f;              // o arrasto da camada impact (tread.json): o puff que sai a v0 para em v0/4
	constexpr float VeilPerSide2 = 11.75f, VeilFringeShare = 0.25f, VeilWindDense = 100.0f, VeilWindFringe = 50.0f;
	constexpr int32 VeilMaxPuffs = 40;
	constexpr float VeilFringeMaxKg = 40.0f;           // a franja longe da carga grande (o ataque sem a gravação) fica de fora: rala e cara
	// -VeilV=1: o véu de antes (dois anéis a 0,45 e 0,8 da metade do raio do vento de 20 m/s, 2,5–3 m, A 0,2), pro A/B
	constexpr float VeilV1Wind20RadiusM = 21.6f, VeilV1Alpha = 0.2f;
	// (3) os pontinhos dos estilhaços na areia (a ~2 km/s cobrem 30 m em 15–20 ms, antes do choque): 10·K², numa elipse à frente (3 a 20·K m,
	// ±25° do rumo do míssil), vida 1,5·G s. (A areia do acerto) com 0,5–0,9 m e A 0,3 não apareciam: 1,5 m, A 2, a gravação já aberta.
	// (A revisão) Só na explosão acima do chão, fora da planta do alvo e fora da sombra dele (a blindagem barra): ver o OnPulse
	constexpr float SpeckLifeG = 1.5f, SpeckSizeM = 1.5f, SpeckMass = 2.0f, SpeckBakeT0 = 0.3f;
	// (23/10, o Felipe corrigiu: "não vai ter muita areia saindo do casco, não tem nada a ver com física nem com a realidade") saiu a camada
	// Under, a poeira saindo de baixo do casco: o chão embaixo do casco fica tapado pelo próprio casco (a onda não chega lá); a areia do acerto
	// no veículo é só o véu do choque (4), a onda pequena que empurra a areia do chão pro lado
	//
	// (caso 2) o míssil na areia (Glasstone §6.70–6.75; Housen e Holsapple): a cratera D = 0,65·W^⅓ (Ra = D/2), escavada em 0,8·√(D/g); a
	// cortina desaba em ~2–4× isso e vira a onda de base, uma corrente de gravidade (U ≈ 1–4 m/s depois do impulso) cuja frente para em
	// R_L ≈ 0,55·R100 (~15 m em ~4,5 s com 150 kg, como a strike_gas2). Com a gravação strike_chao o perto é dela; sem ela, a reserva: o
	// anel nasce em 2,15·Ra (a borda do avental) com 2,5·t_esc ±30% de atraso, sai a v0 = arrasto·(R_L − r0) (a camada freia em 0,8/s), lado
	// min(0,3·R_L/0,45; 8 m), N = 2π·0,6·R_L/(0,45·lado) + 4 no miolo
	constexpr float CraterD = 0.65f, SurgeDelayEsc = 2.5f, SurgeDrag = 0.8f, SurgeMass = 8.0f, SurgeLifeS = 6.0f, SurgeMergeS = 3.0f;
	constexpr float RunoutShare = 0.55f;
	// a poeira que fica (Linger): sob o esmaecer da gravação (o ataque de 1 s da camada é a emenda) ou 1,5 s depois da onda de base de
	// reserva, em células de 6 m dentro de R_L (o mesmo ataque não repete célula por 4 s), lado 7–9 m (visível 3–4 m, topo ≤ 1,9 m: abaixo
	// do casco), A ≈ o τ rente ao chão da gravação no esmaecer (a meta de leitura ≤ 0,7: 50% de transmitância), sai a 1,5–3 m/s (arrasto
	// 0,3), vive 20 s (τ ~0,3 aos 8–10 s, zero aos 20) e deriva no vento do mapa. A régua da massa é a do ataque (150 kg); a carga menor
	// (o míssil fora do ataque, sem a gravação) leva menos por área: os finos ∝ W numa área ∝ W^⅔, então A ∝ (W/150)^⅓ (o mesmo na onda
	// de base); e o lado no máximo 1,5·R_L
	constexpr float LingerCellCm = 600.0f, LingerMass = 6.0f, LingerLifeS = 20.0f, LingerBakeT0 = 1.0f, LingerRepeatS = 4.0f, LingerAfterSurgeS = 1.5f;
	constexpr float SandRefKg = 150.0f;
	// os respingos (Uehara, Ambroso, Ojha e Durian 2003, PRL 90 194301): a cratera Dc = 0,92·[ρb/(ρg·μ²)]^¼·Db^¾·H^¼, H = v²/2g, μ = tan 34°
	// (a areia seca); o que se vê é o avental mais o ar espremido debaixo do corpo, ~2·Dc → o lado do puff ≈ 4,4·Dc. O torrão de 0,45 m a
	// 15 m/s dá Dc 1,1 m
	constexpr float SandMu = 0.67f, Gravity = 9.81f;
	constexpr float ClodDensityRatio = 1.0f, EmberDensityRatio = 4.9f, TurretDensityRatio = 0.9f;
	constexpr float LandMass = 4.0f, LandLifeS = 2.5f, LandBakeT0 = 0.3f, EmberLandMass = 1.5f, EmberLandLifeS = 1.8f;
	// (caso 3) a torre (15 t): a 1ª batida a 13–16 m/s, as seguintes a 2–7 m/s. O corpo que bate: a quina (1,2 m) ou a face (2,7 m) pelo
	// espalhamento dos pontos; o anel visível 2,2·Dc (4,8 m de quina e 9 m de face na 1ª batida), N = W/1,2 m puffs (3–8) mais 1 no meio,
	// lado 0,55·W, A ∝ Dc (a massa ∝ Dc³ no regime da gravidade sobre a área ∝ Dc²), saída 0,25·v_n, o respingo a jusante na batida
	// oblíqua (Gault e Wedekind 1978: até 4:1). Deslizando: um puff de 2,4 m a cada 0,5 m de contato (a garra da lagarta raspando)
	constexpr float TurretRingK = 2.2f, TurretPuffStepM = 1.2f, TurretScrapeSizeM = 2.4f, TurretScrapeLifeS = 2.5f;
	// (a revisão 3) o teto de cartões de impacto vivos e por nascer por preset (Baixa, Média, Alta, Ultra) e a cota de cada classe (ESand: a
	// poeira que fica, os pousos, a torre, o perto), a fração do teto que fica guardada pra ela enquanto ela não usa. A reserva de antes só
	// barrava quem chegava depois: no ataque, o perto e a poeira que fica (mandados no acerto) enchiam o teto e os pousos dos torrões (1,5–3 s
	// depois) não entravam (1 de 72 no Ultra com 9 mísseis; scratchpad sand3/budget.py, o "ANTES"). Agora só quem chega depois tem a cota
	// guardada: os pousos ~24% (o pico deles no ataque de 9 no Ultra, ~90 dos 384) e a torre ~8% (~30); a poeira que fica e o perto dividem o
	// resto na ordem, sem cota (guardar pra eles prendia o perto no caso comum, só acertos em veículos: 219 de 660 contra 347). Na Alta e no
	// Ultra o custo do volume é o dos tijolos (o guarda abaixo segura); só os cartões da Média custam por cartão (a overdraw)
	constexpr int32 ImpactCap[4] = { 0, 64, 256, 384 };
	constexpr float CardShare[4] = { 0.0f, 0.24f, 0.08f, 0.0f };
	// o guarda de tijolos do campo (4.096 no atlas; medido: 40 tanques ≈ 4.000 e +4,1 ms no Ultra, ~1 µs por tijolo): os de impacto até
	// 1.600 na Alta e 2.400 no Ultra (a beleza máxima: o ataque de 9 com a poeira que fica inteira pede ~2.150; o campo inteiro segue abaixo
	// de 3.600, então o pior caso do campo é o de antes, o das lagartas), e a cota guardada dos pousos (20%) e da torre (5%), em fração do
	// teto que vale agora (o menor entre o dos de impacto e o que as lagartas deixam)
	constexpr int32 MaxImpactBricks[4] = { 1600, 1600, 1600, 2400 }, MaxFieldBricks = 3600;
	constexpr float BrickShare[4] = { 0.0f, 0.2f, 0.05f, 0.0f };
	// por preset: as células da poeira que fica por ataque, os puffs por batida da torre e as raspadas por torre
	constexpr int32 LingerCap[4] = { 0, 16, 48, 64 };
	constexpr int32 TurretHitCap[4] = { 0, 3, 6, 8 };
	constexpr int32 TurretScrapeCap[4] = { 0, 4, 8, 12 };

	// (10, a revisão) o fio de fumaça do pedaço em brasa em voo: a fuligem em volume (Kind 1) no RTSMissileSmoke. A esteira se dilui em
	// e^(−t/0,35 s) (a fita aprovada das chapas sumia em 0,8 s) e sai aos 3τ = 1,05 s (5% da massa: não se vê); morna, sobe 0,4 m/s.
	// (A 2ª revisão, o custo: a 1ª conta, um puff a cada 0,35 m com σ 0,14, dava ~50 puffs vivos por pedaço a 10–20 m/s) um puff a cada
	// 0,8 m com σ 0,28 m, já nascendo com ele (RTSMissileSmoke::TickSmolders): ~16 vivos por pedaço (o "fiapo, ≤ 15 puffs" do plano §6). Uma
	// fila de gaussianas a 2,9 σ ondula ±15% (2·e^(−2π²σ²/d²)) e alisa com o puff abrindo pela difusão de Richardson; a 70–120 m o fio tem
	// 5–6 px. A massa por metro é a de antes (0,17 m²/m: a fuligem que o pedaço solta não depende do passo), então τ ~0,24 de lado a lado
	// ao nascer: mais largo e mais ralo, a mesma leitura somada [E: a fechar no olho]
	constexpr float EmberSmokeStepM = 0.8f, EmberSmokeSigmaM = 0.28f, EmberSmokeExtM2 = 0.14f, EmberSmokeFadeS = 0.35f, EmberSmokeRiseMs = 0.4f;
	// o teto de pontos que fumegam por preset
	constexpr int32 SmolderCap[4] = { 0, 3, 6, 8 };
	// (9) a fumaça do furo: a fuligem carvão (o puff do diesel, que absorve) a 4 puffs/s com a massa caindo em e^(−t/τ), τ = 2,5·K s, até 3τ;
	// σ de 0,25 m ao nascer, morna (sobe ~1,5 m/s). A massa de cada puff [J], a medir no jogo: τ ~0,3 no miolo do fio ao nascer
	constexpr float SmolderRate = 4.0f, SmolderExtM2 = 0.12f, SmolderSigmaM = 0.25f, SmolderRiseMs = 1.5f, SmolderTauK = 2.5f;
	// no chão (o ataque, o tiro forçado): o fiapo claro saindo da cratera (a fumaça clara do míssil, fraco, τ 2 s); o pedaço que caiu queimando:
	// o fiapo de 3–4 s (τ 1,2 s, até 3τ)
	constexpr float GroundSmolderTau = 2.0f, GroundSmolderExt = 0.05f, LandSmolderTau = 1.2f, LandSmolderExt = 0.04f;
	// (14) o tremor: 6 cm a partir de 12 kg (um terço da morte de tanque, que tem combustível e munição), atrasado pela distância (d/340 s)
	constexpr float ShakeCm = 6.0f, ShakeMinKg = 11.5f;

	FVector2D Dir2(const FVector& V)
	{
		const FVector2D D(V.X, V.Y);
		return D.SizeSquared() > 1.0e-4 ? D.GetSafeNormal() : FVector2D::ZeroVector;
	}

	/** -VeilV= (2, o padrão: o véu pela lei do choque; 1: o de antes, ralo). */
	int32 VeilVersion()
	{
		static const int32 Ver = [] { int32 V = 2; FParse::Value(FCommandLine::Get(), TEXT("VeilV="), V); return V; }();
		return Ver;
	}

	/** A sobrepressão do choque (kPa) a Z = r/W^⅓ (m/kg^⅓) da carga de TNT ao ar livre: Kinney e Graham 1985 (conferida com a LANL, Rae e
	 *  McAfee 2018). */
	double ShockOverpressureKPa(double Z)
	{
		const double Z2 = Z * Z;
		return 101.325 * 808.0 * (1.0 + Z2 / (4.5 * 4.5)) / FMath::Sqrt((1.0 + Z2 / (0.048 * 0.048)) * (1.0 + Z2 / (0.32 * 0.32)) * (1.0 + Z2 / (1.35 * 1.35)));
	}

	/** O vento atrás do choque (m/s) a Z: Rankine-Hugoniot no ar, u = (5/7)·(Δp/p0)·c0 / √(1 + 6Δp/(7p0)). */
	double ShockWindMs(double Z)
	{
		const double X = ShockOverpressureKPa(Z) / 101.325;
		return (5.0 / 7.0) * X * 340.0 / FMath::Sqrt(1.0 + 6.0 * X / 7.0);
	}

	/** O raio (m) em que o vento atrás do choque cai a U (m/s), pra a carga efetiva W (kg de TNT): 4 kg do lado (W_ef 7,2) a 100 m/s, 7,3 m. */
	double RadiusAtWindM(double WKg, double U)
	{
		double Lo = 0.05, Hi = 60.0;                       // em Z: o vento cai com a distância
		for (int32 I = 0; I < 48; ++I)
		{
			const double Mid = 0.5 * (Lo + Hi);
			(ShockWindMs(Mid) > U ? Lo : Hi) = Mid;
		}
		return Lo * FMath::Pow(FMath::Max(WKg, 0.01), 1.0 / 3.0);
	}

	/** O diâmetro da cratera do respingo (m) de um corpo de tamanho S (m) batendo a V (m/s) na areia seca (Uehara et al. 2003). */
	double SplashCraterM(double DensityRatio, double SizeM, double SpeedMs)
	{
		const double H = SpeedMs * SpeedMs / (2.0 * Gravity);
		return 0.92 * FMath::Pow(DensityRatio / (SandMu * SandMu), 0.25) * FMath::Pow(FMath::Max(SizeM, 0.01), 0.75) * FMath::Pow(FMath::Max(H, 1.0e-4), 0.25);
	}

	/** O segmento A→B passa pelo retângulo [Min, Max] (Liang-Barsky, no plano). */
	bool SegmentCrossesRect(const FVector2D& A, const FVector2D& B, const FVector2D& Min, const FVector2D& Max)
	{
		double T0 = 0.0, T1 = 1.0;
		const double P[2] = { A.X, A.Y }, D[2] = { B.X - A.X, B.Y - A.Y }, Lo[2] = { Min.X, Min.Y }, Hi[2] = { Max.X, Max.Y };
		for (int32 K = 0; K < 2; ++K)
		{
			if (FMath::Abs(D[K]) < 1.0e-9)
			{
				if (P[K] < Lo[K] || P[K] > Hi[K])
				{
					return false;
				}
				continue;
			}
			double Ta = (Lo[K] - P[K]) / D[K], Tb = (Hi[K] - P[K]) / D[K];
			if (Ta > Tb)
			{
				Swap(Ta, Tb);
			}
			T0 = FMath::Max(T0, Ta);
			T1 = FMath::Min(T1, Tb);
			if (T0 > T1)
			{
				return false;
			}
		}
		return true;
	}

	bool InsideRect(const FVector2D& P, const FVector2D& Min, const FVector2D& Max)
	{
		return P.X > Min.X && P.X < Max.X && P.Y > Min.Y && P.Y < Max.Y;
	}

	/** (a revisão) A planta do alvo no chão, a mesma pro véu e pros pontinhos: a caixa das peças no espaço dele (cm, +X a frente; sem a caixa,
	 *  a do tanque USA no veículo; o prédio sem caixa fica sem a planta). Embaixo dela (+ 0,5 m) nada nasce (o casco tapa o chão); atrás
	 *  dela, vista do ponto do acerto (o segmento cruza a planta encolhida 0,3 m), o casco barra os estilhaços e só a onda que contorna passa. */
	struct FTargetPlan
	{
		bool bOn = false;
		FTransform Xf = FTransform::Identity;
		FVector2D Min = FVector2D::ZeroVector, Max = FVector2D::ZeroVector;

		FVector2D ToLocal(const FVector2D& At) const
		{
			const FVector L = Xf.InverseTransformPositionNoScale(FVector(At.X, At.Y, Xf.GetLocation().Z));
			return FVector2D(L.X, L.Y);
		}
		/** O chão embaixo do alvo (a planta + 0,5 m). */
		bool Under(const FVector2D& At) const
		{
			return bOn && InsideRect(ToLocal(At), Min - FVector2D(50.0, 50.0), Max + FVector2D(50.0, 50.0));
		}
		/** O ponto em cima do alvo (dentro da planta encolhida 0,3 m): o teto, a face da torre em cima do convés. */
		bool Over(const FVector2D& At) const
		{
			return bOn && InsideRect(ToLocal(At), Min + FVector2D(30.0, 30.0), Max - FVector2D(30.0, 30.0));
		}
		/** O segmento From → At passa pela planta encolhida: At fica atrás do alvo, visto de From. */
		bool Behind(const FVector2D& From, const FVector2D& At) const
		{
			return bOn && SegmentCrossesRect(ToLocal(From), ToLocal(At), Min + FVector2D(30.0, 30.0), Max - FVector2D(30.0, 30.0));
		}
	};

	FTargetPlan PlanOf(const FRTSBlastPulse& P)
	{
		FTargetPlan T;
		const BlastFxHitLife::FHitOut& H = P.Hit;
		const ARTSUnitActor* Victim = P.VictimActor.Get();
		T.bOn = Victim && P.Victim != 0 && (H.bHullBox || P.bVehicle);
		if (T.bOn)
		{
			// (23/10 noite) no rumo do casco (o Body: o ator do veículo não gira); sem a caixa, o tanque USA no rumo do ator
			T.Xf = H.bHullBox ? FTransform(FRotator(0.0, H.HullYawDeg, 0.0), Victim->GetActorLocation()) : Victim->GetActorTransform();
			T.Min = H.bHullBox ? FVector2D(H.HullMinCm.X, H.HullMinCm.Y) : FVector2D(-395.0, -188.0);
			T.Max = H.bHullBox ? FVector2D(H.HullMaxCm.X, H.HullMaxCm.Y) : FVector2D(395.0, 188.0);
		}
		return T;
	}
}

double FRTSHitLife::DustYieldOf(const FVector4f& Surface)
{
	// (a revisão) a tabela única do chão: a da poeira das lagartas aprovada, igual bit a bit (1 − max(asfalto, molhado), em double), agora
	// também a do acerto (antes 0,17 na terra batida e 0,05 na rocha: um tanque passando ali levantaria mais que o véu do míssil)
	return 1.0 - double(FMath::Max(Surface.X, Surface.W));
}

bool FRTSHitLife::DrawsShockVeil(const ARTSTreadDust* Dust)
{
	// as mesmas condições do véu no OnPulse que não dependem do acerto: a camada, o véu de agora e o preset com cartões de impacto
	return Dust && BlastFxHitLife::On(BlastFxHitLife::Veil) && VeilVersion() != 1 && ImpactCap[FMath::Clamp(BlastFxHitLife::Preset(), 0, 3)] > 0;
}

float FRTSHitLife::KickRate(float Kg, float MassT)
{
	const float Ratio = (FMath::Max(Kg, 0.1f) / KickRefKg) * (KickRefMassT / FMath::Max(MassT > 0.0f ? MassT : KickRefMassT, 1.0f));
	return FMath::Min(KickAnchorDeg * Ratio, KickAnchorDeg * KickPeakMaxDeg / KickAnchorPeakDeg);
}

void FRTSHitLife::KickHull(ARTSUnitActor* A, const FVector2D& FromCm, float RateDeg)
{
	// o PlayHit de hoje com o tranco pela física: o empurrão vai de quem atirou pro alvo; de frente levanta o nariz, de lado deita pro lado de lá
	if (!A || A->IsStructure() || A->IsDead() || RateDeg <= 0.0f)
	{
		return;
	}
	const FVector Push = FVector(FVector2D(A->GetActorLocation()) - FromCm, 0.0).GetSafeNormal();
	if (Push.IsNearlyZero())
	{
		return;
	}
	const FVector Local = A->GetActorTransform().InverseTransformVectorNoScale(Push);
	A->PlayBump(RateDeg * float(-Local.X), RateDeg * float(Local.Y));
}

void FRTSHitLife::SendPuffs(TArray<FPuffSpec>& Puffs, const FRTSHitLifeCtx& C, uint32 Seed, ESand Class)
{
	ARTSTreadDust* Dust = C.Dust;
	if (!Dust || Puffs.IsEmpty())
	{
		return;
	}
	const int32 Ci = int32(Class);
	const int32 Pr = FMath::Clamp(BlastFxHitLife::Preset(), 0, 3);
	const int32 Cap = ImpactCap[Pr];
	// (a revisão 3) os cartões de cada classe, vivos e por nascer (o livro daqui: o plugin só conta o total), e o lugar: o teto menos todos
	// menos a parte livre da cota das outras classes (a guardada pra quem chega depois: não depende da ordem de chegada)
	int32 Room = Cap;
	for (int32 K = 0; K < 4; ++K)
	{
		const double Now = C.Now;
		CardUntil[K].RemoveAllSwap([Now](double Until) { return Until <= Now; });
		const int32 N = CardUntil[K].Num();
		Room -= N + (K != Ci ? FMath::Max(FMath::FloorToInt32(float(Cap) * CardShare[K]) - N, 0) : 0);
	}
	if (Room <= 0)
	{
		return;
	}
	if (Puffs.Num() > Room)
	{
		// no teto de cartões: fica uma amostra espalhada (um a cada tantos), não só os primeiros (o custo é por cartão: o anel inteiro mais
		// ralo, não meio anel)
		TArray<FPuffSpec> Kept;
		for (int32 I = 0; I < Room; ++I)
		{
			Kept.Add(Puffs[I * Puffs.Num() / Room]);
		}
		Puffs = MoveTemp(Kept);
	}
	if (Dust->IsVolumePath())
	{
		// o guarda de tijolos (no volume). As colunas de 2 m do campo que cada puff ocupa: as células que o puff cobre no meio da vida (o campo
		// pega o círculo da caixa da gravação × 1,415 + 0,3 m, ~0,8 do lado; na vida a impact cresce 1,3× e as outras 1,6×, no meio dela 1,15×
		// e 1,3×); os tijolos ≈ 1,5 × as
		// colunas (do chão −0,3 m ao topo +0,3 m o puff atravessa ~1,5 fileiras de 2 m)
		constexpr float BricksPerColumn = 1.5f;
		constexpr double RefreshS = 0.25;
		auto ColumnsOf = [](const FPuffSpec& S, TArray<FIntPoint>& Cols)
		{
			Cols.Reset();
			const double Rm = 0.8 * S.SizeM * (S.Layer == 0 ? 1.15 : 1.3) + 0.3;
			const double Xc = S.PosCm.X / 100.0, Yc = S.PosCm.Y / 100.0;
			const int32 X0 = FMath::FloorToInt32((Xc - Rm) / 2.0), X1 = FMath::FloorToInt32((Xc + Rm) / 2.0);
			const int32 Y0 = FMath::FloorToInt32((Yc - Rm) / 2.0), Y1 = FMath::FloorToInt32((Yc + Rm) / 2.0);
			for (int32 Y = Y0; Y <= Y1; ++Y)
			{
				for (int32 X = X0; X <= X1; ++X)
				{
					const double Cx = FMath::Clamp(Xc, 2.0 * X, 2.0 * (X + 1)), Cy = FMath::Clamp(Yc, 2.0 * Y, 2.0 * (Y + 1));
					if (FMath::Square(Cx - Xc) + FMath::Square(Cy - Yc) <= Rm * Rm)
					{
						Cols.Add(FIntPoint(X, Y));
					}
				}
			}
		};
		// (a revisão, o processador) as vencidas saem e as que já nasceram se contam no máximo a cada 0,25 s (entre uma e outra a conta erra pro
		// lado seguro: a vencida ainda conta como ocupada)
		if (C.Now >= BrickRefreshAt)
		{
			BrickColumnsNow = 0;
			FMemory::Memzero(BrickColumnsOf, sizeof(BrickColumnsOf));
			for (auto It = BrickColumns.CreateIterator(); It; ++It)
			{
				if (It.Value().Until <= C.Now)
				{
					It.RemoveCurrent();
					continue;
				}
				BrickColumnsNow += It.Value().From <= C.Now ? 1 : 0;
				++BrickColumnsOf[int32(It.Value().Owner)];
			}
			BrickRefreshAt = C.Now + RefreshS;
		}
		// o lugar, em tijolos: os de impacto (os vivos e os que ainda vão nascer) até o teto do preset; o campo inteiro (as lagartas ≈ o que o
		// campo tem menos os de impacto que já nasceram, mais os de impacto vivos e por nascer) abaixo de 3.600; (a revisão 3) menos a parte
		// livre da cota das outras classes (a dos pousos e a da torre, na fração do teto que vale agora)
		const float Live = BricksPerColumn * float(BrickColumns.Num());
		const float Tread = FMath::Max(float(Dust->BricksInUse()) - BricksPerColumn * float(BrickColumnsNow), 0.0f);
		const float TopNow = FMath::Min(float(MaxImpactBricks[Pr]), float(MaxFieldBricks) - Tread);
		float Budget = FMath::Min(float(MaxImpactBricks[Pr]) - Live, float(MaxFieldBricks) - Tread - Live);
		for (int32 K = 0; K < 4; ++K)
		{
			Budget -= K != Ci ? FMath::Max(BrickShare[K] * TopNow - BricksPerColumn * float(BrickColumnsOf[K]), 0.0f) : 0.0f;
		}
		// (a revisão) sem lugar, saem os de fora pra dentro (Rank: a distância do centro do evento), cada um pelas colunas novas que acrescenta;
		// cortar a contagem por amostra deixava a área quase igual (os puffs grandes se sobrepõem: a leva ficava mais rala sem liberar tijolos).
		// O de fora que não cabe sai; um que só cobre colunas já ocupadas não custa tijolo e entra
		TArray<int32> Order;
		Order.Reserve(Puffs.Num());
		for (int32 I = 0; I < Puffs.Num(); ++I)
		{
			Order.Add(I);
		}
		Order.StableSort([&Puffs](int32 A, int32 B) { return Puffs[A].Rank < Puffs[B].Rank; });
		TArray<FPuffSpec> Kept;
		Kept.Reserve(Puffs.Num());
		TArray<FIntPoint> Cells;
		for (const int32 I : Order)
		{
			const FPuffSpec& S = Puffs[I];
			ColumnsOf(S, Cells);
			int32 Added = 0;
			for (const FIntPoint& Cell : Cells)
			{
				Added += BrickColumns.Contains(Cell) ? 0 : 1;
			}
			if (Added > 0 && BricksPerColumn * float(Added) > Budget)
			{
				continue;                            // (com o lugar já negativo, o que só cobre colunas ocupadas ainda entra: não soma tijolo)
			}
			Budget -= BricksPerColumn * float(Added);
			const double From = C.Now + double(S.DelayS), Until = From + double(S.LifeS);
			for (const FIntPoint& Cell : Cells)
			{
				if (FBrickColumn* Col = BrickColumns.Find(Cell))
				{
					Col->From = FMath::Min(Col->From, From);
					if (Until > Col->Until)
					{
						// a coluna passa pra classe que a segura até mais tarde (a cota de tijolos é de quem a prende)
						--BrickColumnsOf[int32(Col->Owner)];
						++BrickColumnsOf[Ci];
						Col->Until = Until;
						Col->Owner = Class;
					}
				}
				else
				{
					BrickColumns.Add(Cell, FBrickColumn{ From, Until, Class });
					++BrickColumnsOf[Ci];
				}
			}
			Kept.Add(S);
		}
		Puffs = MoveTemp(Kept);
		if (Puffs.IsEmpty())
		{
			return;
		}
	}
	TArray<ARTSTreadDust::FImpactPuff> Out;
	Out.Reserve(Puffs.Num());
	for (const FPuffSpec& S : Puffs)
	{
		ARTSTreadDust::FImpactPuff& O = Out.AddDefaulted_GetRef();
		O.PosM = S.PosCm / 100.0;
		O.VelM = S.VelMs;
		O.SizeM = S.SizeM;
		O.LifeS = S.LifeS;
		O.Mass = S.Mass;
		O.DelayS = S.DelayS;
		O.BakeT0 = S.BakeT0;
		O.Layer = S.Layer;
		CardUntil[Ci].Add(C.Now + double(FMath::Max(S.DelayS, 0.0f)) + double(FMath::Max(S.LifeS, 0.1f)));   // (o plugin: a vida ≥ 0,1 s)
	}
	Dust->AddImpactPuffs(Out, Seed);
}

void FRTSHitLife::AddVeil(const FRTSBlastPulse& P, const FRTSHitLifeCtx& C, float VeilKg, bool bFringe, float HandOffS, FRandomStream& Rng, TArray<FPuffSpec>& Out)
{
	// (4, a areia do acerto, caso 1) a onda pequena que empurra a areia do chão pro lado: a faixa densa da bola até o vento de 100 m/s, a
	// franja até o de 50 m/s; do lado atingido W_ef = 1,8·W, no teto 1,0·W (o centro é o do alvo), atrás do casco 0,5·W (o segmento do ponto
	// do acerto até o puff cruza a planta do alvo), embaixo do casco (a planta + 0,5 m) nada. No chão, 1,8·W em volta
	const BlastFxHitLife::FHitOut& H = P.Hit;
	const double W = FMath::Max(double(VeilKg), 0.5);
	const ARTSUnitActor* Victim = P.VictimActor.Get();
	const FTargetPlan Plan = PlanOf(P);
	const FVector2D Point2(H.PointCm.X, H.PointCm.Y);
	FVector2D Centre = Point2;
	double Wlit = 1.8 * W;
	bool bShadow = false;
	if (Plan.bOn)
	{
		if (H.bTop)
		{
			Centre = FVector2D(Victim->GetActorLocation().X, Victim->GetActorLocation().Y);
			Wlit = 1.0 * W;
		}
		else if (Plan.Over(Centre))
		{
			Wlit = 1.0 * W;                          // a face da torre, em cima do convés: a onda desce por cima do casco, como no teto
		}
		else
		{
			bShadow = true;
		}
	}
	// (a revisão) a altura da explosão sobre o chão: o choque anda a distância inclinada √(r² + h²), então no chão o raio de cada vento é
	// √(R² − h²) e a chegada é √(r² + h²)/400 s. No tanque (h 1–2,4 m) quase não muda; o acerto alto na parede do prédio (v4_siege) deixa no
	// chão só o que o raio alcança (acima do raio do vento de 100 m/s, nada)
	const double Hm = FMath::Max(double(H.PointCm.Z) - (C.GroundZCm ? double(C.GroundZCm(Point2)) : 0.0), 0.0) / 100.0;
	auto OnGround = [Hm](double Slant) { return FMath::Sqrt(FMath::Max(Slant * Slant - Hm * Hm, 0.0)); };
	const double Rb = FMath::Pow(W, 1.0 / 3.0);   // a bola (m): ~1·W^⅓ (LANL)
	const double R0 = OnGround(Rb);
	const double R100 = OnGround(RadiusAtWindM(Wlit, VeilWindDense)), R50 = OnGround(RadiusAtWindM(Wlit, VeilWindFringe));
	const double R100s = OnGround(RadiusAtWindM(0.5 * W, VeilWindDense)), R50s = OnGround(RadiusAtWindM(0.5 * W, VeilWindFringe));
	if (R100 <= R0)
	{
		return;
	}
	double Side = VeilSizeM * BlastFxHitLife::G(float(W));
	int32 Dense = FMath::RoundToInt32(VeilPerSide2 * float((R100 * R100 - R0 * R0) / (Side * Side)));
	if (Dense > VeilMaxPuffs)
	{
		Side = FMath::Sqrt(VeilPerSide2 * (R100 * R100 - R0 * R0) / double(VeilMaxPuffs));
		Dense = VeilMaxPuffs;
	}
	Dense = FMath::Max(Dense, 6);
	const int32 Fringe = bFringe ? FMath::Clamp(FMath::RoundToInt32(VeilFringeShare * VeilPerSide2 * float((R50 * R50 - R100 * R100) / (Side * Side))), 0, VeilMaxPuffs / 2) : 0;
	const float Life = VeilLifeG * BlastFxHitLife::G(float(W));
	for (int32 I = 0; I < Dense + Fringe; ++I)
	{
		const bool bF = I >= Dense;
		const int32 Ring = bF ? Fringe : Dense, J = bF ? I - Dense : I;
		const double Ang = 2.0 * PI * (double(J) + Rng.FRandRange(-0.35f, 0.35f)) / double(Ring) + (bF ? PI / double(Ring) : 0.0);
		const FVector2D Dir(FMath::Cos(Ang), FMath::Sin(Ang));
		const double U = Rng.FRand();
		auto InBand = [U](double In, double Outer) { return FMath::Sqrt(In * In + U * FMath::Max(Outer * Outer - In * In, 0.0)); };
		double R = bF ? InBand(R100, R50) : InBand(R0, R100);
		FVector2D At = Centre + Dir * (R * 100.0);
		if (bShadow && Plan.Behind(Centre, At))
		{
			// atrás do casco: só a onda que contorna (~0,5·W), que recupera em 1–2 alturas do obstáculo
			if ((bF ? R50s : R100s) <= (bF ? R100s : R0))
			{
				continue;
			}
			R = bF ? InBand(R100s, R50s) : InBand(R0, R100s);
			At = Centre + Dir * (R * 100.0);
		}
		if (Plan.Under(At))
		{
			continue;                                // embaixo do casco: o casco tapa o chão (a onda não chega lá)
		}
		const float Yield = C.DustYield ? FMath::Clamp(C.DustYield(At), 0.0f, 1.0f) : 1.0f;
		if (Yield < 0.05f || (C.IsShown && !C.IsShown(At)))
		{
			continue;
		}
		const float V0 = Rng.FRandRange(2.0f, 4.0f);
		FPuffSpec& S = Out.AddDefaulted_GetRef();
		S.PosCm = FVector(At.X, At.Y, C.GroundZCm ? C.GroundZCm(At) : 0.0f);
		S.VelMs = FVector(Dir.X, Dir.Y, 0.0) * V0;
		S.SizeM = float(Side) * Rng.FRandRange(0.85f, 1.15f);
		S.LifeS = Life;
		S.Mass = (bF ? VeilFringeMass : VeilMass) * Yield * Rng.FRandRange(0.7f, 1.3f);   // patchy: a areia solta não é igual em todo lugar
		S.DelayS = float(FMath::Sqrt(R * R + Hm * Hm) / 400.0);
		S.BakeT0 = VeilBakeT0;
		S.Layer = 0;
		S.Rank = float(R);
		if (HandOffS > 0.0f)
		{
			// (a revisão) a continuação da gravação dedicada: no esmaecer dela o puff já andou o que anda (v0/arrasto: 0,5–1 m) e parou (a
			// e^(−4·t) do arrasto; dali só o vento do mapa), a gravação dele vai na idade (a camada anda a 1×) e vive o que sobra (no mínimo a
			// metade). A massa é a do véu nessa idade (a da gravação dedicada no esmaecer, a casar quando ela existir e for medida)
			const FVector Push = FVector(Dir.X, Dir.Y, 0.0) * (100.0 * double(V0) / double(VeilLayerDrag));
			S.PosCm += Push;
			S.PosCm.Z = C.GroundZCm ? C.GroundZCm(FVector2D(S.PosCm.X, S.PosCm.Y)) : S.PosCm.Z;
			S.VelMs = FVector::ZeroVector;
			S.DelayS += HandOffS;
			S.BakeT0 = VeilBakeT0 + HandOffS;
			S.LifeS = FMath::Max(Life - HandOffS, 0.5f * Life);
		}
	}
}

void FRTSHitLife::AddVeilV1(const FRTSBlastPulse& P, const FRTSHitLifeCtx& C, FRandomStream& Rng, TArray<FPuffSpec>& Out)
{
	// -VeilV=1: o véu de antes, exato na lei (dois anéis a 0,45 e 0,8 da metade do raio do vento de 20 m/s, 16·K puffs de 2,5–3 m, A 0,2)
	const float Kk = BlastFxHitLife::K(FMath::Max(P.Kg, 0.5f)), Gg = BlastFxHitLife::G(FMath::Max(P.Kg, 0.5f));
	const FVector2D Point2(P.Hit.PointCm.X, P.Hit.PointCm.Y);
	const float VeilRcm = 0.5f * VeilV1Wind20RadiusM * Kk * 100.0f;
	const int32 Count = FMath::Max(6, FMath::RoundToInt32(16.0f * Kk));
	const int32 Inner = FMath::RoundToInt32(0.4f * float(Count));
	for (int32 I = 0; I < Count; ++I)
	{
		const bool bInner = I < Inner;
		const int32 Ring = bInner ? Inner : Count - Inner;
		const int32 J = bInner ? I : I - Inner;
		const float Ang = 2.0f * PI * (float(J) + Rng.FRandRange(-0.3f, 0.3f)) / float(FMath::Max(Ring, 1)) + (bInner ? 0.0f : PI / float(FMath::Max(Ring, 1)));
		const float Rcm = VeilRcm * (bInner ? 0.45f : 0.8f) * Rng.FRandRange(0.88f, 1.12f);
		const FVector2D Dir(FMath::Cos(Ang), FMath::Sin(Ang));
		const FVector2D At = Point2 + Dir * Rcm;
		const float Yield = C.DustYield ? FMath::Clamp(C.DustYield(At), 0.0f, 1.0f) : 1.0f;
		if (Yield < 0.05f || (C.IsShown && !C.IsShown(At)))
		{
			continue;
		}
		FPuffSpec& S = Out.AddDefaulted_GetRef();
		S.PosCm = FVector(At.X, At.Y, C.GroundZCm ? C.GroundZCm(At) : 0.0f);
		S.VelMs = FVector(Dir.X, Dir.Y, 0.0) * Rng.FRandRange(4.0f, 8.0f);
		S.SizeM = Rng.FRandRange(2.5f, 3.0f);
		S.LifeS = VeilLifeG * Gg;
		S.Mass = VeilV1Alpha * Yield;
		S.DelayS = Rcm / 100.0f / 400.0f;
		S.Rank = Rcm / 100.0f;
	}
}

void FRTSHitLife::AddSmolder(const FSmolder& S, double Now)
{
	// o mesmo ponto (o mesmo alvo) renova o tempo, não soma (a rajada de 4: um fiapo só); no teto do preset sai o mais velho de menor
	// prioridade (o pedaço que caiu antes da cratera, a cratera antes do furo); sem lugar, o novo não nasce
	if (FSmolder* Same = Smolders.FindByPredicate([&S](const FSmolder& X) { return X.Id == S.Id; }))
	{
		*Same = S;
		Same->T0 = Now;
		return;
	}
	const int32 Cap = SmolderCap[BlastFxHitLife::Preset()];
	if (Cap <= 0)
	{
		return;
	}
	if (Smolders.Num() >= Cap)
	{
		int32 Victim = INDEX_NONE;
		for (int32 I = 0; I < Smolders.Num(); ++I)
		{
			const FSmolder& X = Smolders[I];
			if (X.Prio <= S.Prio && (Victim == INDEX_NONE || X.Prio < Smolders[Victim].Prio || (X.Prio == Smolders[Victim].Prio && X.T0 < Smolders[Victim].T0)))
			{
				Victim = I;
			}
		}
		if (Victim == INDEX_NONE)
		{
			return;
		}
		Smolders.RemoveAt(Victim);
	}
	FSmolder& N = Smolders.Add_GetRef(S);
	N.T0 = Now;
}

void FRTSHitLife::OnPulse(const FRTSBlastPulse& P, const FRTSHitLifeCtx& C)
{
	namespace HL = BlastFxHitLife;
	StepPulses.RemoveAll([&P](const FRTSBlastPulse& Q) { return Q.Tick != P.Tick; });
	StepPulses.Add(P);
	const HL::FHitOut& H = P.Hit;
	if (!H.bValid)
	{
		return;
	}
	const float Kg = FMath::Max(P.Kg, 0.5f);
	const float Kk = HL::K(Kg);
	ARTSUnitActor* Victim = P.VictimActor.Get();
	const uint32 Seed = (uint32(P.Tick) * 2654435761u) ^ (P.Victim * 0x9E3779B9u) ^ (Serial++ * 0x85EBCA6Bu);
	FRandomStream Rng(int32(Seed & 0x7fffffffu));
	// (5) o tranco do alvo pela física (o prédio não mexe)
	if (Victim && P.bVehicle && HL::On(HL::Kick))
	{
		KickHull(Victim, P.FromCm, KickRate(Kg, P.MassT));
	}
	if (!H.bShown)
	{
		return;                                  // longe da vista (a regra do efeito curto de 17/10): só o tranco e a marca
	}
	const FVector2D Point2(H.PointCm.X, H.PointCm.Y);
	const FVector2D Foot2(H.FootCm.X, H.FootCm.Y);
	auto Yield = [&C](const FVector2D& At) { return C.DustYield ? FMath::Clamp(C.DustYield(At), 0.0f, 1.0f) : 1.0f; };
	auto GroundZ = [&C](const FVector2D& At) { return C.GroundZCm ? C.GroundZCm(At) : 0.0f; };
	auto IsVis = [&C](const FVector2D& At) { return !C.IsShown || C.IsShown(At); };
	// (1) os brilhos dos estilhaços nos vizinhos dentro dos 50 kPa (7,3 m × K: Kinney-Graham), 3–4 cada (os do alvo vêm do BlastFx)
	if (C.Fx && C.UnitsNear && HL::On(HL::Glints))
	{
		TArray<ARTSUnitActor*> Near;
		C.UnitsNear(Foot2, (730.0 * Kk) + 200.0, Near);
		for (ARTSUnitActor* A : Near)
		{
			if (A && A != Victim && !A->IsDead() && !A->IsFogHidden())
			{
				C.Fx->HullGlints(A, H.BallCentreCm, A->GetActorLocation(), 700.0f, Rng.RandRange(3, 4), Rng.GetUnsignedInt());
			}
		}
	}
	// a areia: a poeira que fica e o perto (o véu, os pontinhos, a onda de base de reserva), cada um na classe dele (SendPuffs)
	TArray<FPuffSpec> Near, Linger;
	const bool bGround = P.Victim == 0 && !Victim;
	const float SandKg = FMath::Max(P.SandKg > 0.0f ? P.SandKg : Kg, 0.5f);
	const bool bSandBake = bGround && H.bSandBake;   // a gravação strike_chao nasceu: o perto é dela
	// (a revisão) a gravação dedicada da ogiva (warhead_lado, _teto, _chao) nasceu: ela já tem a areia que o choque levanta (uma representação
	// só). No veículo o véu é a continuação dela, nascendo no esmaecer (OwnSandFadeS, como a poeira que fica depois da strike_chao); no chão o
	// perto é dela e a poeira que fica nasce no esmaecer
	const bool bOwnSand = H.bOwnSand;
	const bool bBakeNear = bSandBake || (bGround && bOwnSand);
	// (4) o véu de areia do choque (a fusão: um véu novo só a mais de meio raio de um nascido há menos de 1,5 s: a rajada de 4 no mesmo alvo dá
	// um véu a cada 1,5 s; no tiro que mata, a morte traz a areia dela). Com o véu de agora o BlastFx não pôs o anel de areia da ogiva
	// (FHitIn::bGameVeil, DrawsShockVeil): a areia que o choque levanta é só ele
	Veils.RemoveAll([&C](const TPair<FVector2D, double>& V) { return C.Now - V.Value > VeilMergeS; });
	if (HL::On(HL::Veil) && !P.bKill && !bBakeNear)
	{
		const float VeilKg = bGround ? SandKg : Kg;
		const double MergeCm = VeilVersion() == 1 ? 0.25 * VeilV1Wind20RadiusM * Kk * 100.0 : 50.0 * RadiusAtWindM(1.8 * VeilKg, VeilWindDense);
		if (!Veils.ContainsByPredicate([&Point2, MergeCm](const TPair<FVector2D, double>& V) { return FVector2D::Distance(V.Key, Point2) < MergeCm; }))
		{
			Veils.Add({ Point2, C.Now });
			if (VeilVersion() == 1)
			{
				AddVeilV1(P, C, Rng, Near);
			}
			else
			{
				AddVeil(P, C, VeilKg, VeilKg <= VeilFringeMaxKg, bOwnSand ? FMath::Max(H.OwnSandFadeS, 0.0f) : 0.0f, Rng, Near);
			}
		}
	}
	// (3) os pontinhos dos estilhaços na areia, numa elipse à frente do míssil (o que diz, visto de cima, de onde veio o tiro); a fusão do
	// véu (a rajada no mesmo alvo, os mísseis do mesmo ataque: um leque por lugar a cada 1,5 s).
	// (A revisão, pela física) Só na explosão acima do chão (no casco, no prédio): o estilhaço que sai quase deitado de 1–2,4 m de altura
	// cai 3–20 m à frente. No chão a explosão não tem altura: o que desce entra na cratera e o que sai deitado ou pra cima cai a centenas de
	// metros; quem diz ali de onde veio o tiro são os torrões no rumo do míssil (e a gravação strike_chao já traz a ejeção dela). O alvo barra
	// o que vai pra ele: nada nasce embaixo da planta (+ 0,5 m: não é areia saindo de baixo do casco) nem atrás dela vista do ponto do acerto
	// no lado (no teto e na face da torre em cima do convés o leque passa por cima do casco). -VeilV=1 deixa os do chão como antes (menos
	// sobre a gravação, que é nova)
	const FVector2D Fwd = Dir2(H.IncomingW);
	const bool bV1 = VeilVersion() == 1;
	const bool bSpeckHere = !bGround || (bV1 && !bBakeNear);
	SpeckSets.RemoveAll([&C](const TPair<FVector2D, double>& V) { return C.Now - V.Value > VeilMergeS; });
	const double SpeckMergeCm = 50.0 * RadiusAtWindM(1.8 * Kg, VeilWindDense);
	if (HL::On(HL::Specks) && bSpeckHere && !Fwd.IsZero()
		&& !SpeckSets.ContainsByPredicate([&Point2, SpeckMergeCm](const TPair<FVector2D, double>& V) { return FVector2D::Distance(V.Key, Point2) < SpeckMergeCm; }))
	{
		SpeckSets.Add({ Point2, C.Now });
		const int32 Count = FMath::Clamp(FMath::RoundToInt32(10.0f * Kk * Kk), 1, 24);
		const FTargetPlan Plan = PlanOf(P);
		const bool bSideHit = Plan.bOn && !H.bTop && !Plan.Over(Point2);
		for (int32 I = 0; I < Count; ++I)
		{
			const float Ang = FMath::DegreesToRadians(Rng.FRandRange(-25.0f, 25.0f));
			const FVector2D Dir = Fwd.GetRotated(FMath::RadiansToDegrees(Ang));
			const float Dist = Rng.FRandRange(300.0f, 2000.0f * Kk);
			const FVector2D At = Point2 + Dir * Dist;
			if (Plan.Under(At) || (bSideHit && Plan.Behind(Point2, At)))
			{
				continue;                            // embaixo do alvo, ou o alvo no caminho: a blindagem barra
			}
			const float Ya = Yield(At);
			if (Ya < 0.1f || !IsVis(At))
			{
				continue;
			}
			FPuffSpec& S = Near.AddDefaulted_GetRef();
			S.PosCm = FVector(At.X, At.Y, GroundZ(At));
			S.VelMs = FVector(Dir.X, Dir.Y, 0.0) * Rng.FRandRange(1.5f, 3.0f) + FVector(0.0, 0.0, 0.5);
			S.SizeM = bV1 ? Rng.FRandRange(0.5f, 0.9f) : SpeckSizeM * Rng.FRandRange(0.85f, 1.15f);
			S.LifeS = SpeckLifeG * HL::G(Kg);
			S.Mass = (bV1 ? 0.3f : SpeckMass) * Ya;
			S.DelayS = Rng.FRandRange(0.015f, 0.03f);
			S.BakeT0 = bV1 ? -1.0f : SpeckBakeT0;
			S.Rank = Dist / 100.0f;
		}
	}
	// (caso 2) o míssil na areia: a onda de base de reserva (sem a gravação) e a poeira que fica (Linger)
	if (bGround)
	{
		const double W3 = FMath::Pow(double(SandKg), 1.0 / 3.0);
		const double Dcr = CraterD * W3, Ra = 0.5 * Dcr;          // a cratera (m)
		const double TEsc = 0.8 * FMath::Sqrt(Dcr / Gravity);      // a escavação (s)
		const double RL = RunoutShare * RadiusAtWindM(1.8 * SandKg, VeilWindDense);   // a frente da onda de base para aqui (m)
		const float MassK = FMath::Min(1.0f, FMath::Pow(SandKg / SandRefKg, 1.0f / 3.0f));
		double LingerDelay = 0.0;
		if (bBakeNear)
		{
			// sob o esmaecer da gravação (a strike_chao, ou a warhead_chao do míssil no chão fora do ataque; o ataque de 1 s da camada é a emenda)
			LingerDelay = FMath::Max(bSandBake ? H.SandFadeS : H.OwnSandFadeS, 0.0f);
		}
		else
		{
			const double SurgeDelay = SurgeDelayEsc * TEsc;
			LingerDelay = SurgeDelay + LingerAfterSurgeS;
			Surges.RemoveAll([&C](const TPair<FVector2D, double>& V) { return C.Now - V.Value > SurgeMergeS; });
			const bool bSurgeOn = P.bStrike ? HL::On(HL::Strike) : HL::On(HL::Veil);
			const double R0 = 2.15 * Ra;
			// (a fusão: a onda de base de um míssil a menos de R_L de outra dos últimos 3 s já varre aquele chão)
			if (bSurgeOn && RL > R0 && !Surges.ContainsByPredicate([&Point2, RL](const TPair<FVector2D, double>& V) { return FVector2D::Distance(V.Key, Point2) < 100.0 * RL; }))
			{
				Surges.Add({ Point2, C.Now });
				const float Side = float(FMath::Min(0.3 * RL / 0.45, 8.0));
				const int32 Ring = FMath::Max(6, FMath::RoundToInt32(float(2.0 * PI * 0.6 * RL / (0.45 * double(Side)))));
				const float V0 = float(SurgeDrag * (RL - R0));
				for (int32 I = 0; I < Ring + 4; ++I)
				{
					const bool bCore = I >= Ring;
					const int32 N = bCore ? 4 : Ring, J = bCore ? I - Ring : I;
					const float Ang = 2.0f * PI * (float(J) + Rng.FRandRange(-0.35f, 0.35f)) / float(N);
					const FVector2D Dir(FMath::Cos(Ang), FMath::Sin(Ang));
					const FVector2D At = Point2 + Dir * (100.0 * (bCore ? 0.5 : 1.0) * R0 * Rng.FRandRange(0.85f, 1.15f));
					const float Ya = Yield(At);
					if (Ya < 0.05f || !IsVis(At))
					{
						continue;
					}
					FPuffSpec& S = Near.AddDefaulted_GetRef();
					S.PosCm = FVector(At.X, At.Y, GroundZ(At));
					S.VelMs = FVector(Dir.X, Dir.Y, 0.0) * (V0 * (bCore ? 0.3f : 1.0f) * Rng.FRandRange(0.85f, 1.15f));
					S.SizeM = Side * Rng.FRandRange(0.85f, 1.15f);
					S.LifeS = SurgeLifeS * FMath::Pow(FMath::Min(1.0f, SandKg / SandRefKg), 1.0f / 6.0f);
					S.Mass = SurgeMass * MassK * Ya;
					S.DelayS = float(SurgeDelay) * Rng.FRandRange(0.7f, 1.3f);
					S.BakeT0 = 0.3f;
					S.Layer = 1;
					S.Rank = float(FVector2D::Distance(At, Point2) / 100.0);
				}
			}
		}
		if (HL::On(HL::Linger))
		{
			// as células de 6 m dentro de R_L, as de perto primeiro (o teto do preset por ataque corta as de longe)
			for (auto It = LingerCells.CreateIterator(); It; ++It)
			{
				if (C.Now - It.Value() > LingerLifeS)
				{
					It.RemoveCurrent();
				}
			}
			int32 Recent = 0;
			for (const TPair<FIntPoint, double>& Cell : LingerCells)
			{
				Recent += C.Now - Cell.Value < LingerRepeatS ? 1 : 0;
			}
			const int32 Cap = LingerCap[FMath::Clamp(HL::Preset(), 0, 3)];
			const double Rcm = RL * 100.0;
			const int32 X0 = FMath::FloorToInt32((Point2.X - Rcm) / LingerCellCm), X1 = FMath::FloorToInt32((Point2.X + Rcm) / LingerCellCm);
			const int32 Y0 = FMath::FloorToInt32((Point2.Y - Rcm) / LingerCellCm), Y1 = FMath::FloorToInt32((Point2.Y + Rcm) / LingerCellCm);
			TArray<TPair<double, FIntPoint>> Cand;
			for (int32 Y = Y0; Y <= Y1; ++Y)
			{
				for (int32 X = X0; X <= X1; ++X)
				{
					const FVector2D Ctr((X + 0.5) * LingerCellCm, (Y + 0.5) * LingerCellCm);
					const double D = FVector2D::Distance(Ctr, Point2);
					const double* Last = LingerCells.Find(FIntPoint(X, Y));
					if (D <= Rcm + 0.5 * LingerCellCm && !(Last && C.Now - *Last < LingerRepeatS))
					{
						Cand.Add({ D, FIntPoint(X, Y) });
					}
				}
			}
			Cand.Sort([](const TPair<double, FIntPoint>& A, const TPair<double, FIntPoint>& B) { return A.Key < B.Key; });
			const float SideMax = float(FMath::Max(3.0, 1.5 * RL));
			for (const TPair<double, FIntPoint>& Cd : Cand)
			{
				if (Recent >= Cap)
				{
					break;
				}
				LingerCells.Add(Cd.Value, C.Now);
				++Recent;
				const FVector2D At = FVector2D((Cd.Value.X + 0.5) * LingerCellCm, (Cd.Value.Y + 0.5) * LingerCellCm)
					+ FVector2D(Rng.FRandRange(-150.0f, 150.0f), Rng.FRandRange(-150.0f, 150.0f));
				const float Ya = Yield(At);
				if (Ya < 0.05f || !IsVis(At))
				{
					continue;
				}
				const FVector2D Out = (At - Point2).GetSafeNormal();
				FPuffSpec& S = Linger.AddDefaulted_GetRef();
				S.PosCm = FVector(At.X, At.Y, GroundZ(At));
				S.VelMs = FVector(Out.X, Out.Y, 0.0) * Rng.FRandRange(1.5f, 3.0f);
				S.SizeM = FMath::Min(Rng.FRandRange(7.0f, 9.0f), SideMax);
				S.LifeS = LingerLifeS;
				S.Mass = LingerMass * MassK * Ya;
				S.DelayS = float(LingerDelay) + Rng.FRandRange(0.0f, 0.4f);
				S.BakeT0 = LingerBakeT0;
				S.Layer = 2;
				S.Rank = float(Cd.Key / 100.0);
			}
		}
	}
	// (a revisão 3) a poeira que fica antes, nas células dela no miolo (o perto deste míssil reaproveita as colunas dela e põe a borda de fora);
	// os dois dividem o que sobra da cota guardada dos pousos e da torre, que chegam 1,5–4 s depois
	SendPuffs(Linger, C, Seed ^ 0x1196u, ESand::Linger);
	SendPuffs(Near, C, Seed ^ 0xD057u, ESand::Veil);
	// (9) a fumaça do furo, presa ao casco (anda com o tanque; o prédio, da face atingida); no chão, o fiapo claro da cratera
	if (HL::On(HL::Smolder))
	{
		FSmolder S;
		if (Victim && H.bOnHull && !Victim->IsDead())
		{
			S.Id = P.Victim;
			S.Actor = Victim;
			S.bAttached = true;
			S.LocalCm = Victim->GetActorTransform().InverseTransformPosition(H.PointCm + H.NormalW * 10.0);
			S.TauS = SmolderTauK * Kk;
			S.Rate = SmolderRate;
			S.Ext = SmolderExtM2;
			S.Sigma = SmolderSigmaM;
			S.Rise = SmolderRiseMs;
			S.Kind = 1;
			S.Prio = 2;
			AddSmolder(S, C.Now);
		}
		else if (!Victim)
		{
			S.Id = 0x20000000u | (HashCombine(GetTypeHash(FMath::RoundToInt(Point2.X / 200.0)), GetTypeHash(FMath::RoundToInt(Point2.Y / 200.0))) & 0x0FFFFFFFu);
			S.WorldCm = FVector(Point2.X, Point2.Y, GroundZ(Point2) + 20.0f);
			S.TauS = GroundSmolderTau;
			S.Rate = SmolderRate;
			S.Ext = GroundSmolderExt;
			S.Sigma = 0.3f;
			S.Rise = 1.0f;
			S.Kind = 0;
			S.Prio = 1;
			AddSmolder(S, C.Now);
		}
	}
	// (14) o tremor (a partir de 12 kg), atrasado pela distância da câmera (a onda chega em d/340 s; a Patriot não treme)
	if (HL::On(HL::Shake) && P.Kg >= ShakeMinKg && C.World)
	{
		const APlayerController* Pc = C.World->GetFirstPlayerController();
		const double Dist = Pc && Pc->PlayerCameraManager ? FVector::Dist(Pc->PlayerCameraManager->GetCameraLocation(), H.PointCm) : 0.0;
		FShake& Sh = Shakes.AddDefaulted_GetRef();
		Sh.Cm = Point2;
		Sh.StrengthCm = ShakeCm;
		Sh.At = C.Now + Dist / 34000.0;
	}
}

void FRTSHitLife::OnTurretImpact(const FVector& AtCm, const FVector2D& SlideDir, float NormalSpeedCmS, float TangentSpeedCmS, float SpreadCm, const FRTSHitLifeCtx& C)
{
	// (a areia do acerto, caso 3) a torre de 15 t batendo na areia: o respingo pela energia da batida (Uehara et al. 2003, o regime da
	// gravidade; ρ_torre/ρ_areia ≈ 0,9: 15 t numa caixa de ~10,5 m³), o corpo que bate pelo espalhamento dos pontos (a quina 1,2 m, a face
	// 2,7 m), o anel visível 2,2·Dc, mais a jusante na batida oblíqua (Gault e Wedekind 1978). (A revisão) Só com a vida: -HitLifeV=1 é a de
	// hoje, exata, sem nenhuma areia nova
	if (!TurretSandOn() || !C.Dust)
	{
		return;
	}
	const FVector2D At(AtCm.X, AtCm.Y);
	const int32 PerHit = TurretHitCap[FMath::Clamp(BlastFxHitLife::Preset(), 0, 3)];
	const float Ya = C.DustYield ? FMath::Clamp(C.DustYield(At), 0.0f, 1.0f) : 1.0f;
	if (PerHit <= 0 || Ya < 0.05f || (C.IsShown && !C.IsShown(At)))
	{
		return;
	}
	const double Vn = FMath::Max(double(NormalSpeedCmS) / 100.0, 0.0), Vt = FMath::Max(double(TangentSpeedCmS) / 100.0, 0.0);
	const double Db = FMath::Clamp(2.0 * double(SpreadCm) / 100.0 + 0.4, 1.2, 2.7);
	const double Dc = SplashCraterM(TurretDensityRatio, Db, Vn);
	const double Wvis = TurretRingK * Dc;
	const int32 Ring = FMath::Min(FMath::Clamp(FMath::RoundToInt32(float(Wvis / TurretPuffStepM)), 3, 8), PerHit);
	const float Side = float(0.55 * Wvis);
	const float Mass = float(FMath::Clamp(6.0 * Dc / 2.2, 1.5, 8.0)) * Ya;
	const float Life = float(2.5 * FMath::Sqrt(Dc / 2.2));
	const double Beta = 0.6 * FMath::Min(Vn > 0.1 ? Vt / Vn : 0.0, 1.0);
	const FVector2D Heading = SlideDir.IsNearlyZero() ? FVector2D(1.0, 0.0) : SlideDir.GetSafeNormal();
	FRandomStream Rng(int32((HashCombine(GetTypeHash(FMath::RoundToInt(AtCm.X)), GetTypeHash(FMath::RoundToInt(AtCm.Y))) ^ (Serial++ * 0x85EBCA6Bu)) & 0x7fffffffu));
	TArray<FPuffSpec> Puffs;
	for (int32 I = 0; I <= Ring; ++I)
	{
		const bool bMid = I == Ring;
		// o rumo pelo peso 1 + β·cos(φ − φ_rumo) (a rejeição: o respingo a jusante)
		FVector2D Dir = Heading;
		for (int32 Try = 0; Try < 8; ++Try)
		{
			const float Ang = Rng.FRandRange(0.0f, 2.0f * PI);
			const FVector2D Cand(FMath::Cos(Ang), FMath::Sin(Ang));
			if (Rng.FRand() * (1.0 + Beta) <= 1.0 + Beta * FVector2D::DotProduct(Cand, Heading))
			{
				Dir = Cand;
				break;
			}
		}
		const double R = bMid ? 0.0 : Wvis * Rng.FRandRange(0.1f, 0.35f);
		const FVector2D P = At + Dir * (R * 100.0);
		FPuffSpec& S = Puffs.AddDefaulted_GetRef();
		S.PosCm = FVector(P.X, P.Y, C.GroundZCm ? C.GroundZCm(P) : AtCm.Z);
		S.VelMs = FVector(Dir.X, Dir.Y, 0.0) * (0.25 * Vn * (bMid ? 0.3 : 1.0)) + FVector(Heading.X, Heading.Y, 0.0) * (0.3 * Vt);
		S.SizeM = Side * Rng.FRandRange(0.85f, 1.15f);
		S.LifeS = Life;
		S.Mass = Mass * Rng.FRandRange(0.8f, 1.2f);
		S.DelayS = 0.0f;
		S.BakeT0 = 0.3f;                         // o respingo é um jato: a gravação já aberta
		S.Layer = 0;
		S.Rank = float(R);
	}
	SendPuffs(Puffs, C, Rng.GetUnsignedInt(), ESand::Turret);
}

void FRTSHitLife::OnTurretScrape(const FVector& AtCm, const FVector2D& Dir, float SpeedCmS, int32 Index, const FRTSHitLifeCtx& C)
{
	// (a areia do acerto, caso 3) a torre deslizando ou rolando: o mesmo fenômeno da garra da lagarta raspando a areia, um puff de 2,4 m a
	// cada 0,5 m de contato, a massa pela velocidade (A 0,5 a 2), indo pra frente com 0,3 da velocidade
	if (!TurretSandOn() || !C.Dust || Index >= TurretScrapeCap[FMath::Clamp(BlastFxHitLife::Preset(), 0, 3)])
	{
		return;
	}
	const FVector2D At(AtCm.X, AtCm.Y);
	const float Ya = C.DustYield ? FMath::Clamp(C.DustYield(At), 0.0f, 1.0f) : 1.0f;
	if (Ya < 0.05f || (C.IsShown && !C.IsShown(At)))
	{
		return;
	}
	const float Vt = FMath::Max(SpeedCmS / 100.0f, 0.0f);
	const FVector2D D = Dir.IsNearlyZero() ? FVector2D::ZeroVector : Dir.GetSafeNormal();
	TArray<FPuffSpec> Puffs;
	FPuffSpec& S = Puffs.AddDefaulted_GetRef();
	S.PosCm = FVector(At.X, At.Y, C.GroundZCm ? C.GroundZCm(At) : AtCm.Z);
	S.VelMs = FVector(D.X, D.Y, 0.0) * (0.3f * Vt);
	S.SizeM = TurretScrapeSizeM;
	S.LifeS = TurretScrapeLifeS;
	S.Mass = 2.0f * FMath::Clamp(Vt / 4.0f, 0.25f, 1.0f) * Ya;
	S.BakeT0 = 0.3f;
	S.Layer = 0;
	SendPuffs(Puffs, C, (Serial++ * 2654435761u) ^ uint32(Index), ESand::Turret);
}

void FRTSHitLife::OnDamaged(int64 Tick, uint32 Target, uint32 Source, float Amount, ARTSUnitActor* Actor, const FRTSHitLifeCtx& C)
{
	// (6) o vizinho do dano em área (a simulação já calculou quem levou e quanto): o tranco = o do alvo × (o dano no vizinho ÷ o dano no
	// alvo), sem constante nova, e a marca do míssil na face virada pra explosão. O acerto (ProjectileHit) sai antes, no mesmo passo; e o dano
	// do próprio alvo vem antes do dos vizinhos (World::ResolveDamage: o cheio do alvo entra na fila antes da área, ApplySplash)
	namespace HL = BlastFxHitLife;
	if (Amount <= 0.0f || !HL::On(HL::Neighbors))
	{
		return;
	}
	// (a revisão) o dano de verdade no alvo, depois da blindagem: a régua dos vizinhos deste passo (no lugar do dano-base da arma)
	for (FRTSBlastPulse& P : StepPulses)
	{
		if (P.Tick == Tick && P.Shooter == Source && P.Victim == Target && P.Victim != 0 && P.VictimDamage <= 0.0f)
		{
			P.VictimDamage = Amount;
			return;
		}
	}
	if (!Actor || Actor->IsDead())
	{
		return;
	}
	const FVector2D At(Actor->GetActorLocation().X, Actor->GetActorLocation().Y);
	const FRTSBlastPulse* Best = nullptr;
	double BestD = TNumericLimits<double>::Max();
	for (const FRTSBlastPulse& P : StepPulses)
	{
		if (P.Tick != Tick || P.Shooter != Source || P.Victim == Target || !P.Hit.bValid)
		{
			continue;
		}
		const double D = FVector2D::Distance(FVector2D(P.Hit.PointCm.X, P.Hit.PointCm.Y), At);
		if (D < BestD && D < 1500.0)
		{
			BestD = D;
			Best = &P;
		}
	}
	if (!Best)
	{
		return;
	}
	// a régua: o dano no alvo neste passo (o plano §6); sem ele (o míssil no chão, ou o alvo que a blindagem zerou), o dano cheio da arma
	const float Ref = Best->VictimDamage > 0.0f ? Best->VictimDamage : Best->RefDamage;
	const float Share = Ref > 0.0f ? FMath::Clamp(Amount / Ref, 0.0f, 1.0f) : 0.5f;
	if (!Actor->IsStructure())
	{
		// de quem vem: da explosão (o pé da bola), não do atirador; o do alvo (a massa dele, 60 t sem alvo) × a parte do dano
		KickHull(Actor, FVector2D(Best->Hit.FootCm.X, Best->Hit.FootCm.Y), KickRate(Best->Kg, Best->MassT) * Share);
	}
	if (C.Fx && Best->Hit.bShown)
	{
		C.Fx->NeighborMark(Actor, Best->Hit.BallCentreCm, Target * 2654435761u);
	}
}

void FRTSHitLife::Tick(const FRTSHitLifeCtx& C)
{
	namespace HL = BlastFxHitLife;
	// a fila de pousos do BlastFx: o torrão grande do chão e o pedaço em brasa que caiu, no 1º toque (o respingo de areia pela energia da
	// batida, Uehara et al. 2003: o lado do puff ≈ 4,4·Dc, saindo a ~1,5 m/s mais 0,3 da velocidade de quem caiu) e, ainda queimando, o
	// fiapo de 3–4 s
	if (C.Fx)
	{
		TArray<HL::FLanding> Landed;
		C.Fx->ConsumeLandings(Landed);
		if (!Landed.IsEmpty())
		{
			TArray<FPuffSpec> Puffs;
			FRandomStream Rng(int32((Serial++ * 2654435761u) & 0x7fffffffu));
			for (const HL::FLanding& L : Landed)
			{
				const FVector2D At(L.PosCm.X, L.PosCm.Y);
				if (C.IsShown && !C.IsShown(At))
				{
					continue;
				}
				const float Ya = C.DustYield ? FMath::Clamp(C.DustYield(At), 0.0f, 1.0f) : 1.0f;
				if (((L.Kind == 0 && HL::On(HL::Embers)) || (L.Kind == 1 && HL::On(HL::Ground))) && Ya >= 0.05f)
				{
					const bool bClod = L.Kind == 1;
					const double Speed = L.VelMs.Size();
					const double Dc = SplashCraterM(bClod ? ClodDensityRatio : EmberDensityRatio, bClod ? double(L.SizeM) : FMath::Min(double(L.SizeM), 0.15), Speed);
					const FVector2D Vh(L.VelMs.X, L.VelMs.Y);
					const FVector2D Away = Vh.IsNearlyZero() ? FVector2D(Rng.FRandRange(-1.0f, 1.0f), Rng.FRandRange(-1.0f, 1.0f)).GetSafeNormal() : Vh.GetSafeNormal();
					FPuffSpec& S = Puffs.AddDefaulted_GetRef();
					S.PosCm = FVector(At.X, At.Y, C.GroundZCm ? C.GroundZCm(At) : L.PosCm.Z);
					S.VelMs = FVector(Away.X, Away.Y, 0.0) * 1.5 + FVector(Vh.X, Vh.Y, 0.0) * 0.3;
					S.SizeM = bClod ? float(FMath::Clamp(4.4 * Dc, 2.0, 5.0)) : float(FMath::Clamp(4.4 * Dc, 0.8, 3.0));
					S.LifeS = bClod ? LandLifeS : EmberLandLifeS;
					S.Mass = (bClod ? LandMass : EmberLandMass) * Ya;
					S.BakeT0 = LandBakeT0;
					S.Layer = 0;
					S.Rank = -S.SizeM;                   // no guarda, os respingos maiores primeiro
				}
				if (L.Kind == 0 && L.bHot && HL::On(HL::Embers) && HL::On(HL::Smolder))
				{
					FSmolder Sm;
					Sm.Id = 0x30000000u | (Serial++ & 0x0FFFFFFFu);
					Sm.WorldCm = L.PosCm + FVector(0.0, 0.0, 5.0);
					Sm.TauS = LandSmolderTau;
					Sm.Rate = SmolderRate;
					Sm.Ext = LandSmolderExt;
					Sm.Sigma = 0.12f;
					Sm.Rise = 1.0f;
					Sm.Kind = 1;
					Sm.Prio = 0;
					AddSmolder(Sm, C.Now);
				}
			}
			SendPuffs(Puffs, C, uint32(Rng.GetUnsignedInt()), ESand::Landing);
		}
	}
	// (a revisão) os pedaços em brasa queimando em voo: o fio de fumaça em volume deste quadro (sob a névoa não solta: não entrega o lugar)
	EmberPoints.Reset();
	if (C.Fx)
	{
		TArray<HL::FEmberSmoke> Flying;
		C.Fx->ConsumeEmberSmoke(Flying);
		if (HL::On(HL::Embers) && HL::On(HL::Smolder))
		{
			for (const HL::FEmberSmoke& E : Flying)
			{
				if (!C.IsShown || C.IsShown(FVector2D(E.PosCm.X, E.PosCm.Y)))
				{
					EmberPoints.Add({ E.Id, E.PosCm });
				}
			}
		}
	}
	// os tremores que chegaram
	for (int32 I = Shakes.Num() - 1; I >= 0; --I)
	{
		if (C.Now >= Shakes[I].At)
		{
			if (const APlayerController* Pc = C.World ? C.World->GetFirstPlayerController() : nullptr)
			{
				if (ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(Pc->GetPawn()))
				{
					Cam->AddShake(Shakes[I].Cm, Shakes[I].StrengthCm);
				}
			}
			Shakes.RemoveAtSwap(I);
		}
	}
	// os pontos que fumegam: acabam em 3τ, e o preso ao casco acaba com a vítima (a carcaça assume)
	Smolders.RemoveAll([&C](const FSmolder& S)
	{
		const ARTSUnitActor* A = S.Actor.Get();
		return C.Now - S.T0 > 3.0 * S.TauS || (S.bAttached && (!A || A->IsDead()));
	});
	// (a revisão) os parados no mundo (a cratera, o pedaço que caiu) sob a névoa também não soltam (os presos ao casco conferem a vítima)
	for (FSmolder& S : Smolders)
	{
		S.bHidden = !S.bAttached && C.IsShown && !C.IsShown(FVector2D(S.WorldCm.X, S.WorldCm.Y));
	}
}

void FRTSHitLife::GetSmolders(double Now, TArray<FRTSSmokeSmolder>& Out) const
{
	for (const FSmolder& S : Smolders)
	{
		const float Age = float(Now - S.T0);
		if (Age < 0.0f || Age > 3.0f * S.TauS)
		{
			continue;
		}
		FVector At = S.WorldCm;
		if (S.bHidden)
		{
			continue;
		}
		if (S.bAttached)
		{
			ARTSUnitActor* A = S.Actor.Get();
			// sob a névoa não solta (não entrega a posição); morto, a carcaça assume
			if (!A || A->IsDead() || A->IsFogHidden())
			{
				continue;
			}
			At = A->GetActorTransform().TransformPosition(S.LocalCm);
		}
		FRTSSmokeSmolder& O = Out.AddDefaulted_GetRef();
		O.Id = S.Id;
		O.PosCm = At;
		O.RatePerS = S.Rate;
		O.ExtM2 = S.Ext * FMath::Exp(-Age / FMath::Max(S.TauS, 0.1f));
		O.SigmaM = S.Sigma;
		O.RiseMs = S.Rise;
		O.Kind = S.Kind;
	}
	// (10, a revisão) o fio de fumaça dos pedaços em brasa em voo: a fuligem em volume pelo caminho (no lugar da fita chapada das chapas)
	for (const FEmberPoint& E : EmberPoints)
	{
		FRTSSmokeSmolder& O = Out.AddDefaulted_GetRef();
		O.Id = 0x10000000u | (E.Id & 0x0FFFFFFFu);
		O.PosCm = E.PosCm;
		O.RatePerS = 0.0f;
		O.StepM = EmberSmokeStepM;
		O.ExtM2 = EmberSmokeExtM2;
		O.SigmaM = EmberSmokeSigmaM;
		O.RiseMs = EmberSmokeRiseMs;
		O.FadeTauS = EmberSmokeFadeS;
		O.Kind = 1;
	}
}

void FRTSHitLife::Reset()
{
	Smolders.Reset();
	EmberPoints.Reset();
	Veils.Reset();
	SpeckSets.Reset();
	Surges.Reset();
	LingerCells.Reset();
	BrickColumns.Reset();
	BrickColumnsNow = 0;
	FMemory::Memzero(BrickColumnsOf, sizeof(BrickColumnsOf));
	BrickRefreshAt = 0.0;
	for (TArray<double>& Until : CardUntil)
	{
		Until.Reset();
	}
	Shakes.Reset();
	StepPulses.Reset();
}
