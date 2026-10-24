#pragma once

// (v4, 23/10) A torre solta da morte do tanque (o "jack-in-the-box": a munição estoura e a pressão sai pelo anel da torre).
// O Felipe (23/10): "ela fica voando do chão, não tem física correta; tem que ter uma física top, bonita, linda dessa torre".
// Antes o "chão" do voo era o Z do ator + 60 cm (o pivô no chão do TANQUE, ~2 m abaixo do corpo da torre): de pé ela parava
// ~2,2 m no ar, de ponta-cabeça entrava no chão, e girava em volta de um ponto 2 m abaixo dela.
//
// Agora é um corpo rígido de verdade, só desenho (a simulação não sabe dela), no método dos motores dos jogos (Box2D, PhysX,
// Havok, Chaos):
//  - impulsos sequenciais (Catto, GDC 2006): impulso acumulado e limitado (o normal ≥ 0, o atrito no disco μ·normal), partida
//    quente, contato especulativo (Catto 2013/Box2D v3: o ponto perto do chão só chega até ele neste passo), restituição só
//    acima de 1 m/s (b2_velocityThreshold) e correção de posição NGS com folga (Box2D v2.4: β 0,2, 0,5 cm, no máximo 20 cm);
//  - o estado como no Baraff (SIGGRAPH 97): x, v, q e o momento angular ℓ, com ω = R·K⁻¹·Rᵀ·ℓ (a precessão sem torque de
//    graça: com a inércia real o eixo do meio é o da cambalhota, e ela vira sozinha no ar, o efeito Dzhanibekov);
//  - passos fixos de 1/120 s e a pose desenhada entre os dois últimos (Fiedler, "Fix Your Timestep!").
// Tudo por unidade de massa (K = I/m, em cm²): a massa só entra na energia da batida (a poeira).
// O chão é o do mapa assado (bilinear de 2 m, o mesmo dos cascos) e os obstáculos são caixas orientadas, lidos a cada passo.
// Estrutura C++ pura (sem UObject), pra dar pra medir fora da Unreal: o espelho em Python (o protótipo do plano, 23/10) mediu
// as constantes; elas estão no .cpp com as fontes.

#include "CoreMinimal.h"

/** O chão que a torre enxerga, lido a cada passo (o jogo dá o mapa assado; um teste pode dar um campo à mão). */
class IRTSLooseTurretGround
{
public:
	virtual ~IRTSLooseTurretGround() = default;
	/** A altura do chão (cm) no ponto (cm). */
	virtual float HeightCm(const FVector2D& Cm) const = 0;
	/** A normal do chão no ponto (unitária). */
	virtual FVector Normal(const FVector2D& Cm) const = 0;
	/** O Rn do chão no ponto: a restituição normal das batidas lentas (a tabela da Rocscience: areia 0,30, asfalto 0,40, rocha 0,35). */
	virtual float Restitution(const FVector2D& Cm) const = 0;
};

/** Uma caixa orientada que a torre não atravessa: o casco da própria carcaça, outra unidade, outra carcaça, um prédio, a torre
 *  pousada de outro tanque. Quem chama guarda a lista e atualiza o quadro a cada quadro (a unidade viva anda); a torre marca
 *  quando cada uma fica armada. */
struct FRTSTurretObstacle
{
	/** O quadro da caixa no mundo (sem escala) e a caixa nele (cm). */
	FTransform Frame = FTransform::Identity;
	FBox Box = FBox(EForceInit::ForceInit);
	/** Neste quadro (o dono sumiu ou ficou sem caixa: desligada). */
	bool bActive = false;
	/** Só conta depois que nenhum ponto da torre está dentro dela (a torre nasce encostada no próprio casco). */
	bool bArmed = false;
};

/** Uma batida forte da torre no chão, pro pó e a marca: o centro dos pontos que bateram (pesado pelo impulso), pra onde ela ia,
 *  a velocidade de aproximação (cm/s), o espalhamento dos pontos que bateram (cm) e (23/10, a areia) a velocidade no plano antes da
 *  batida (cm/s: a batida oblíqua joga a areia a jusante). */
struct FRTSTurretImpact
{
	FVector AtCm = FVector::ZeroVector;
	FVector2D SlideDir = FVector2D::ZeroVector;
	float NormalSpeedCmS = 0.0f;
	float SpreadCm = 0.0f;
	float TangentSpeedCmS = 0.0f;
};

/** (23/10, a areia do acerto) A torre deslizando ou rolando no chão: um ponto a cada 0,5 m de contato a mais de 1 m/s (no máximo 12 por
 *  torre), o centro dos pontos que encostam, pra onde eles andam e a velocidade deles no plano (cm/s); Index = a ordem na torre. Só saída:
 *  a física aprovada não muda. */
struct FRTSTurretScrape
{
	FVector AtCm = FVector::ZeroVector;
	FVector2D Dir = FVector2D::ZeroVector;
	float SpeedCmS = 0.0f;
	int32 Index = 0;
};

/** A forma da torre no quadro das malhas (o do casco com a torre a 0°: x pra frente, z pra cima), em cm. */
struct FRTSLooseTurretShape
{
	/** O corpo fundido, sem o cano (e sem o mastro, a estação de armas e as antenas, que quebram na queda). */
	FBox Body = FBox(EForceInit::ForceInit);
	/** O cano ao longo do +X (a caixa da malha "Barrel"; inválida = sem cano). */
	FBox Barrel = FBox(EForceInit::ForceInit);
	/** A escala das peças no mundo (uniforme). */
	float Scale = 1.0f;
};

struct RTSGAME_API FRTSLooseTurret
{
	/** Monta o corpo: a massa (a caixa de densidade uniforme e o tubo do cano como bastão), o centro, a inércia e os pontos. */
	void Init(const FRTSLooseTurretShape& Shape);
	/** O centro de massa no quadro das malhas (cm, sem a escala): o pivô da torre vai pra ele. */
	FVector CenterOfMassLocal() const { return ComLocal; }
	/** A boca do cano no quadro do corpo (relativa ao centro de massa, cm, com a escala; zero sem cano). */
	FVector MuzzlePoint() const { return Muzzle; }
	/** O corpo fundido no quadro do corpo (relativo ao centro de massa, cm, com a escala): a caixa de obstáculo da torre pousada. */
	FBox BodyBoxLocal() const { return BodyBox; }

	/** Larga: a pose (o centro de massa no mundo e o giro das malhas), a velocidade (cm/s) e o giro (rad/s, no mundo), depois de
	 *  DelayS parada no casco. */
	void Launch(const FVector& InX, const FQuat& InQ, const FVector& InV, const FVector& InW, float DelayS);
	/** Avança o tempo do quadro em passos fixos (no máximo 8 por quadro: num tranco, câmera lenta em vez de atravessar o chão). */
	void Advance(float DeltaSeconds, const IRTSLooseTurretGround& Ground, TArrayView<FRTSTurretObstacle> Obstacles);

	/** A pose desenhada: entre os dois últimos passos (parada, a do repouso). */
	FVector ShownLocation() const;
	FQuat ShownRotation() const;
	bool IsAsleep() const { return bAsleep; }
	/** Parou pelo teto de tempo (10 s de voo), não por ficar parada. */
	bool IsCapped() const { return bCapped; }
	bool HasLaunched() const { return bLaunched; }
	/** Os segundos de voo somados (só andam quando a física anda: param na pausa e ficam em câmera lenta num tranco). */
	float FlightSeconds() const { return FlightS; }
	/** Parada e encostada em pelo menos um obstáculo (o próprio casco, outra unidade, outra torre, um prédio): quem chama
	 *  continua relendo a lista a cada quadro e passa pro WakeIfSupportMoved. */
	bool IsRestingOnObstacle() const { return bAsleep && Supports.Num() > 0; }
	/** Acorda a torre parada se um obstáculo em que ela encostou mexeu mais que a folga do solver (0,5 cm em algum canto da
	 *  caixa) ou desligou (o dono sumiu): a unidade viva que sai, a carcaça empurrada, a torre ou o prédio que afunda. Sem isso
	 *  ela ficava parada no ar (o Box2D faz igual: acorda o corpo encostado no que se mexe ou é destruído). Verdadeiro se acordou. */
	bool WakeIfSupportMoved(TArrayView<const FRTSTurretObstacle> Obstacles);

	/** As batidas fortes desde a última vez que quem chama esvaziou a lista (no máximo 4 por torre). */
	TArray<FRTSTurretImpact> Impacts;
	/** (23/10, a areia do acerto) As raspadas no chão desde a última vez que quem chama esvaziou a lista (no máximo 12 por torre). */
	TArray<FRTSTurretScrape> Scrapes;

	// --- medidas pro log (uma linha por torre ao dormir) e pra depuração
	/** Segundos de voo (desde a saída) do 1º toque no chão (−1 sem toque) e onde o centro estava. */
	float FirstTouchS = -1.0f;
	FVector FirstTouchAt = FVector::ZeroVector;
	/** Segundos de voo ao dormir (ou no teto). */
	float RestS = 0.0f;
	/** As batidas (> 1 m/s, a 0,25 s uma da outra) e a maior (cm/s). */
	int32 Hits = 0;
	float MaxHitCmS = 0.0f;
	/** Quantas vezes encostou num obstáculo (o próprio casco, outra unidade) e a maior velocidade contra eles (cm/s). */
	int32 ObstacleTouches = 0;
	float MaxObstacleHitCmS = 0.0f;
	/** Quantas vezes acordou depois de parada (o apoio mexeu ou sumiu). */
	int32 Wakes = 0;
	/** No repouso: a folga do ponto mais baixo (cm, negativa = dentro do chão) e o menor chão sob os pontos encostados (cm). */
	float RestGapCm = 0.0f;
	float RestGroundZ = 0.0f;
	int32 Steps = 0;
	double SolveSeconds = 0.0;
	/** Os pontos de contato no mundo (pose atual) e se encostaram no último passo (o rts.TurretDebug). */
	void GetDebugPoints(TArray<FVector>& OutPoints, TArray<bool>& OutTouching) const;

private:
	struct FPoint
	{
		FVector P = FVector::ZeroVector;   // no quadro do corpo, relativo ao centro de massa (cm)
		float RadiusCm = 0.0f;
	};
	struct FPointState
	{
		FVector Acc = FVector::ZeroVector; // impulsos acumulados: normal, tangente 1, tangente 2 (cm/s, por massa)
		int32 Kind = -1;                   // −1 solto, 0 chão, 1 + k o obstáculo k
	};
	struct FContact
	{
		int32 Index = 0;
		int32 Kind = 0;
		FVector R = FVector::ZeroVector;   // do centro de massa ao ponto (mundo)
		FVector At = FVector::ZeroVector;
		FVector N = FVector::UpVector;
		FVector T1 = FVector::ForwardVector;
		FVector T2 = FVector::RightVector;
		float Kn = 1.0f;
		float K1 = 1.0f;
		float K2 = 1.0f;
		float Bias = 0.0f;
		float Vn0 = 0.0f;
	};
	/** Um obstáculo em que ela encostou ao parar (o índice na lista de quem chama) e a caixa dele naquela hora. */
	struct FSupport
	{
		int32 Index = 0;
		FTransform Frame = FTransform::Identity;
		FBox Box = FBox(EForceInit::ForceInit);
	};

	void Step(const IRTSLooseTurretGround& Ground, TArrayView<FRTSTurretObstacle> Obstacles);
	void GoToSleep(const IRTSLooseTurretGround& Ground, TArrayView<FRTSTurretObstacle> Obstacles, bool bByCap);
	void Wake();
	void UpdateAxes();
	/** K⁻¹ no mundo vezes A (os eixos do corpo no mundo, Ax/Ay/Az). */
	FVector InvInertiaMul(const FVector& A) const;
	FVector InertiaMul(const FVector& A) const;
	/** 1 / a massa efetiva do contato na direção D, no ponto R (por massa: 1 + D·((K⁻¹(R×D))×R)). */
	float EffectiveInv(const FVector& R, const FVector& D) const;
	void ApplyImpulse(const FVector& P, const FVector& R);
	/** O ponto P (com raio) perto do chão: a separação (cm, negativa = dentro) e a normal; falso se está longe (mais que Margin). */
	static bool GroundSeparation(const IRTSLooseTurretGround& Ground, const FVector& P, float RadiusCm, float MarginCm, float& OutSep, FVector& OutN);
	/** O ponto P (com raio) dentro da caixa: a penetração (cm) pela face mais rasa e a normal dela (pra fora, no mundo). */
	static bool BoxPenetration(const FRTSTurretObstacle& O, const FVector& P, float RadiusCm, float& OutDepth, FVector& OutN);

	TArray<FPoint> Points;
	TArray<FPointState> PointStates;
	TArray<FContact> Contacts;
	FVector K = FVector::OneVector;
	FVector InvK = FVector::OneVector;
	FVector ComLocal = FVector::ZeroVector;
	FVector Muzzle = FVector::ZeroVector;
	FBox BodyBox = FBox(EForceInit::ForceInit);
	/** O raio da esfera que contém todos os pontos (cm): o descarte rápido dos obstáculos longe. */
	float BoundRadiusCm = 0.0f;

	FVector X = FVector::ZeroVector;
	FVector V = FVector::ZeroVector;
	FVector L = FVector::ZeroVector;   // o momento angular por massa (cm²/s, mundo)
	FQuat Q = FQuat::Identity;
	FVector PrevX = FVector::ZeroVector;
	FQuat PrevQ = FQuat::Identity;
	FVector Ax = FVector::ForwardVector;
	FVector Ay = FVector::RightVector;
	FVector Az = FVector::UpVector;
	FVector LaunchV = FVector::ZeroVector;
	FVector LaunchL = FVector::ZeroVector;
	float Delay = 0.0f;
	float Age = 0.0f;
	float FlightS = 0.0f;
	float Accum = 0.0f;
	float StillS = 0.0f;
	float LastHitS = -1.0e9f;
	/** O teto (segundos de voo): 10 s depois da saída e, a cada acordar, 10 s depois dele. */
	float CapAtS = 10.0f;
	int32 ImpactsMade = 0;
	/** (a areia) As raspadas feitas e o caminho de contato andado desde a última (cm). */
	int32 ScrapesMade = 0;
	float ScrapePathCm = 0.0f;
	bool bLaunched = false;
	bool bAsleep = false;
	bool bCapped = false;
	bool bWasOnObstacle = false;
	TArray<bool> TouchingNow;
	/** Os obstáculos com impulso normal no último passo (os índices) e os que seguravam a torre quando ela parou. */
	TArray<int32, TInlineAllocator<4>> TouchedObstacles;
	TArray<FSupport, TInlineAllocator<4>> Supports;
};
