// (23/10) A explosão com vida, do lado do jogo: o acerto de míssil vira um evento (o pulso) com muitas reações, como os AAA fazem (os
// "nuggets" do FXList do ZH; o Physics Field e os Niagara Data Channels da Unreal). O BlastFx desenha a bola, as faíscas, os brilhos e os
// pedaços (UBlastFxDirector::MissileHit com vida) e devolve o ponto, a normal, d, ℓ, o pé e os relógios; daqui saem as reações que moram no
// jogo: o tranco do alvo e dos vizinhos, a areia na poeira em volume das lagartas, a fumaça do furo no RTSMissileSmoke e o tremor atrasado.
// (A areia saindo de baixo do casco saiu: o chão embaixo do casco fica tapado por ele; o Felipe, 23/10)
// A areia do acerto (23/10; docs/ESTUDO_MISSIL_PATRIOT.md, "A areia do acerto (3 casos)"), pela física e na régua da poeira das lagartas:
//  1. a ogiva no blindado: a onda pequena que empurra a areia do chão pro lado (o véu, pela lei de Kinney-Graham com o vazio atrás do casco)
//     e os pontinhos dos estilhaços;
//  2. o míssil na areia: a gravação strike_chao do BlastFx em todo míssil do ataque e, depois dela, a poeira que fica (Linger); sem ela, o
//     véu, a onda de base de reserva e o Linger aqui; os respingos dos torrões no 1º toque, pela energia da batida;
//  3. a torre solta batendo e deslizando na areia (Uehara et al. 2003: a cratera do respingo pela energia).
// Cada camada atrás da sua variável blast.HitLife.<camada> (BlastFxHitLife.h; a torre, rts.Turret.Sand); -HitLifeV=1 = a de hoje, exata
// (sem nenhuma areia nova, a da torre também); -VeilV=1 = o véu de antes (ralo, com o anel de areia do BlastFx). Uma representação só da
// areia que o choque levanta: com o véu, o BlastFx não põe o anel (FHitIn::bGameVeil); com a gravação dedicada, o véu é a continuação
// dela. Só desenho: nada volta pra simulação (o lockstep fica igual em todo PC).
#pragma once

#include "CoreMinimal.h"
#include "BlastFxHitLife.h"

class ARTSTreadDust;
class ARTSUnitActor;
class UBlastFxDirector;
class UWorld;
struct FRTSSmokeSmolder;

/** O acerto de míssil (o pulso): tudo o que o jogo sabe dele num lugar só, pra cada reação ler daqui. */
struct FRTSBlastPulse
{
	int64 Tick = 0;                          // o passo da simulação (os vizinhos do dano em área vêm no mesmo)
	uint32 Victim = 0;                       // o alvo (0 = no chão)
	uint32 Shooter = 0;                      // o atirador (Sim::NoEntity no ataque de mísseis)
	float Kg = 4.0f;                         // a carga (kg de TNT, Units.json warhead_kg; a metade pesada do ataque: a clássica, 20)
	/** (a areia do acerto) A carga que joga a areia no chão (kg de TNT; Units.json hit_life.sand_kg; 0 = Kg): o ataque de mísseis, 150 (o
	 *  Tomahawk de verdade no chão, a escala da gravação strike_chao). Só o chão (o véu, a onda de base e a poeira que fica sem a gravação). */
	float SandKg = 0.0f;
	float RefDamage = 0.0f;                  // o dano cheio da arma (a régua dos vizinhos sem alvo, o míssil no chão)
	float VictimDamage = 0.0f;               // (a revisão) o dano de verdade no alvo neste passo, depois da blindagem (o UnitDamaged dele vem antes
	                                         // dos vizinhos): a régua dos vizinhos, o tranco deles é o do alvo × dano no vizinho ÷ dano no alvo
	float MassT = 60.0f;                     // a massa do alvo (t): o tranco ∝ W/M
	bool bStrike = false;                    // o ataque de mísseis
	bool bVehicle = false;                   // o alvo é veículo (o prédio não balança)
	bool bKill = false;                      // o tiro que matou: a morte traz a areia dela (sem o véu)
	FVector2D FromCm = FVector2D::ZeroVector;                // de onde o míssil veio (o tranco empurra pro lado de lá)
	TWeakObjectPtr<ARTSUnitActor> VictimActor;
	BlastFxHitLife::FHitOut Hit;             // o que o BlastFx desenhou (o ponto, a normal, d, ℓ, o pé, a bola, os relógios, a gravação de areia)
};

/** O que a vida do acerto precisa do jogo a cada chamada (o mundo, os sistemas de desenho e as consultas do mapa). */
struct FRTSHitLifeCtx
{
	UWorld* World = nullptr;
	ARTSTreadDust* Dust = nullptr;
	UBlastFxDirector* Fx = nullptr;
	double Now = 0.0;                                                        // o relógio do mundo (s)
	TFunction<float(const FVector2D&)> GroundZCm;                            // a altura do chão (cm)
	TFunction<float(const FVector2D&)> DustYield;                            // o quanto o chão solta poeira (FRTSHitLife::DustYieldOf)
	TFunction<bool(const FVector2D&)> IsShown;                               // fora da névoa de guerra
	TFunction<void(const FVector2D&, double, TArray<ARTSUnitActor*>&)> UnitsNear;   // as unidades à vista até o raio (cm)
};

class FRTSHitLife
{
public:
	/** O acerto de míssil à vista: as reações do jogo, cada uma atrás da sua camada. */
	void OnPulse(const FRTSBlastPulse& P, const FRTSHitLifeCtx& C);
	/** O dano de um passo (o UnitDamaged): o vizinho do dano em área reage pelo pulso do mesmo passo (o tranco e a marca). */
	void OnDamaged(int64 Tick, uint32 Target, uint32 Source, float Amount, ARTSUnitActor* Actor, const FRTSHitLifeCtx& C);
	/** (a areia do acerto, caso 3) A torre solta batendo no chão (o centro dos pontos que bateram, pra onde ela ia, as velocidades normal e
	 *  tangencial e o espalhamento dos pontos, cm e cm/s): o respingo de areia pela energia da batida, mais pra frente na batida oblíqua. */
	void OnTurretImpact(const FVector& AtCm, const FVector2D& SlideDir, float NormalSpeedCmS, float TangentSpeedCmS, float SpreadCm, const FRTSHitLifeCtx& C);
	/** (a areia do acerto, caso 3) A torre deslizando ou rolando na areia (um ponto a cada 0,5 m de contato a mais de 1 m/s; Index = a ordem
	 *  dele na torre, pro teto do preset): o mesmo fenômeno da garra da lagarta raspando a areia. */
	void OnTurretScrape(const FVector& AtCm, const FVector2D& Dir, float SpeedCmS, int32 Index, const FRTSHitLifeCtx& C);
	/** A cada quadro: a fila de pousos do BlastFx (o respingo de areia e o fiapo), os pedaços em brasa em voo, os tremores atrasados e os
	 *  pontos que fumegam (e quais estão sob a névoa). */
	void Tick(const FRTSHitLifeCtx& C);
	/** Os pontos que fumegam agora (o furo preso ao casco, a cratera, o pedaço que caiu, o pedaço em brasa em voo), pro RTSMissileSmoke. Sob a
	 *  névoa não soltam (não entregam a posição). */
	void GetSmolders(double Now, TArray<FRTSSmokeSmolder>& Out) const;
	void Reset();

	/** (a areia do acerto; a revisão) O quanto o chão solta poeira, pelo peso de cada chão no ponto (FRTSBakedMap::SurfaceAt: X asfalto,
	 *  Y terra batida, Z rocha, W molhado). A tabela única do chão, a mesma pra lagarta e pro acerto (o RTSMatchSubsystem a usa nas duas):
	 *  1 − max(asfalto, molhado), a da poeira das lagartas aprovada. O que sobe, da lagarta ou do choque, é a camada solta por cima: no
	 *  deserto a terra batida e a rocha têm a mesma areia solta em cima e sobem como a areia; o asfalto varrido e o molhado (a coesão), não.
	 *  Assim "se até o tanque levanta, a explosão levanta muito mais" vale em todo chão. Quando houver mapa com terra batida ou rocha de
	 *  verdade (sem a areia por cima: a areia solta levanta ~6× mais que o solo típico, DNA ADA237024), os dois mudam juntos aqui. */
	static double DustYieldOf(const FVector4f& Surface);
	/** (a revisão) O véu do choque (a camada Veil, o de agora: -VeilV=2) vai pra poeira em volume deste jogo: o BlastFx não põe o anel de
	 *  areia da ogiva por cima (FHitIn::bGameVeil; uma representação só da areia que o choque levanta). */
	static bool DrawsShockVeil(const ARTSTreadDust* Dust);

private:
	/** O tranco do alvo (graus por passo na mola da suspensão, o PlayHit de hoje é 1,2): × (W/4 kg)·(60 t/M), até o teto de 5° de pico. */
	static float KickRate(float Kg, float MassT);
	static void KickHull(ARTSUnitActor* A, const FVector2D& FromCm, float RateDeg);
	/** Um puff de areia na poeira em volume (ARTSTreadDust::FImpactPuff): Mass = o A (a massa de vários cartões do rastro, até 16). */
	struct FPuffSpec
	{
		FVector PosCm = FVector::ZeroVector;
		FVector VelMs = FVector::ZeroVector;
		float SizeM = 3.0f, LifeS = 4.0f, Mass = 1.0f, DelayS = 0.0f, BakeT0 = -1.0f;
		int32 Layer = 0;                     // 0 "impact", 1 "impact_surge", 2 "impact_linger"
		/** (a revisão) A ordem no guarda de tijolos, o menor fica primeiro: a distância do centro do evento (m; os de fora saem primeiro e o
		 *  miolo fica denso); nos pousos, menos o lado (os respingos maiores primeiro). */
		float Rank = 0.0f;
	};
	/** As classes da areia no teto de cartões e no guarda de tijolos. (A revisão 3) A cota não depende da ordem de chegada: os pousos e a torre
	 *  chegam 1,5–4 s depois do acerto, e a parte livre da cota deles fica guardada pra eles (ninguém mais a pega); a poeira que fica e o perto
	 *  dividem o resto na ordem (no mesmo acerto, a poeira que fica antes: o perto reaproveita as colunas dela no miolo). */
	enum class ESand : uint8 { Linger, Landing, Turret, Veil };
	/** Os puffs no teto do preset (cartões de impacto vivos e por nascer: 0 Baixa, 64 Média, 256 Alta, 384 Ultra; a cota guardada dos pousos e
	 *  da torre; acima, uma amostra espalhada: nos cartões da Média o custo é por cartão) e no guarda de tijolos do campo (os de impacto até
	 *  1.600 na Alta e 2.400 no Ultra, o campo inteiro abaixo de 3.600, a cota guardada dos pousos e da torre): sem lugar, saem os puffs de fora
	 *  pra dentro (Rank), cada um pelas colunas novas que acrescenta, até caber. */
	void SendPuffs(TArray<FPuffSpec>& Puffs, const FRTSHitLifeCtx& C, uint32 Seed, ESand Class);
	/** O véu do choque (caso 1, e o chão sem a gravação): a faixa densa até o vento de 100 m/s atrás do choque e a franja até 50 m/s
	 *  (Kinney-Graham com Rankine-Hugoniot), W_ef = 1,8·W do lado atingido, 1,0·W no teto, 0,5·W na sombra do casco (o vazio atrás); os raios
	 *  pela distância inclinada da explosão (√(r² + h²): o acerto alto na parede do prédio deixa no chão só o que o raio alcança). HandOffS > 0
	 *  (a revisão: a gravação dedicada da ogiva já desenha a areia de perto): o véu nasce no esmaecer dela, no estado que teria nessa idade (já
	 *  parado, a gravação dele adiantada, a vida que sobra). */
	void AddVeil(const FRTSBlastPulse& P, const FRTSHitLifeCtx& C, float VeilKg, bool bFringe, float HandOffS, FRandomStream& Rng, TArray<FPuffSpec>& Out);
	/** -VeilV=1: o véu de antes (dois anéis ralos, A 0,2), pro A/B. */
	void AddVeilV1(const FRTSBlastPulse& P, const FRTSHitLifeCtx& C, FRandomStream& Rng, TArray<FPuffSpec>& Out);
	/** Um ponto que fumega: preso ao ator (LocalCm no espaço dele) ou parado no mundo; a massa de cada puff cai em e^(−t/τ) e ele para em 3τ. */
	struct FSmolder
	{
		uint32 Id = 0;
		TWeakObjectPtr<ARTSUnitActor> Actor;
		bool bAttached = false;
		FVector LocalCm = FVector::ZeroVector, WorldCm = FVector::ZeroVector;
		double T0 = 0.0;
		float TauS = 2.5f, Rate = 4.0f, Ext = 0.1f, Sigma = 0.25f, Rise = 1.5f;
		uint8 Kind = 1;
		int32 Prio = 0;                      // 2 o furo no casco, 1 a cratera, 0 o pedaço que caiu (o primeiro a ceder no teto)
		bool bHidden = false;                // (a revisão) o ponto parado no mundo está sob a névoa (o Tick confere a cada quadro)
	};
	TArray<FSmolder> Smolders;
	void AddSmolder(const FSmolder& S, double Now);
	/** (a revisão) Os pedaços em brasa queimando em voo neste quadro (o fio de fumaça em volume; o BlastFx põe, o Tick esvazia), fora da névoa. */
	struct FEmberPoint
	{
		uint32 Id = 0;
		FVector PosCm = FVector::ZeroVector;
	};
	TArray<FEmberPoint> EmberPoints;
	/** Os véus vivos (a fusão: um véu novo só a mais de meio raio de um recente), os pontinhos e as ondas de base de reserva do chão. */
	TArray<TPair<FVector2D, double>> Veils, SpeckSets, Surges;
	/** (a areia do acerto) As células de 6 m da poeira que fica (Linger) já preenchidas e quando (o mesmo ataque não repete por 4 s). */
	TMap<FIntPoint, double> LingerCells;
	/** (a areia do acerto) As colunas de 2 m do campo que os puffs mandados ocupam, de quando a quando (o guarda dos tijolos conta a união: as
	 *  levas do mesmo ataque se sobrepõem no chão). (A revisão) Um mapa só, atualizado uma vez por puff que entra; as vencidas saem e as de
	 *  agora se contam no máximo a cada 0,25 s (não a união de todas as levas a cada chamada). (A revisão 3) A classe dona de cada coluna, a
	 *  que a segura até mais tarde: a cota de tijolos de cada classe. */
	struct FBrickColumn
	{
		double From = 0.0, Until = 0.0;
		ESand Owner = ESand::Veil;
	};
	TMap<FIntPoint, FBrickColumn> BrickColumns;
	int32 BrickColumnsNow = 0;               // as colunas que já nasceram (From ≤ agora) na última contagem: as que o campo já tem
	int32 BrickColumnsOf[4] = { 0, 0, 0, 0 };   // as colunas vivas e por nascer de cada classe (a dona), na última contagem mais as que entraram
	double BrickRefreshAt = 0.0;
	/** (a revisão 3) Até quando vive cada cartão de impacto mandado, por classe (o plugin só conta o total): a cota dos cartões. */
	TArray<double> CardUntil[4];
	/** Os tremores atrasados (a onda chega em d/340 s). */
	struct FShake
	{
		FVector2D Cm = FVector2D::ZeroVector;
		float StrengthCm = 0.0f;
		double At = 0.0;
	};
	TArray<FShake> Shakes;
	/** Os pulsos do passo (o dano em área vem no mesmo passo, depois do acerto). */
	TArray<FRTSBlastPulse> StepPulses;
	uint32 Serial = 1;
};
