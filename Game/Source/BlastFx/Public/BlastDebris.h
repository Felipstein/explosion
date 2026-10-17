// Cópia do BlastLab (BlastDebris, 14/10/2026): os destroços da morte de tanque da v2 no jogo (o Felipe: "coloca
// rastro de fumaça também, é tudo que a gente homologou"). A original continua sendo a referência; aqui o relógio
// é o do jogo, o chão é o BlastFxGround e o tamanho, a cor e o brilho vêm do diretor (BlastFxDirector) em vez da
// linha de comando, e o rastro da torre segue o componente que o jogo arremessa.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlastDebris.generated.h"

class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class UPointLightComponent;
class USceneComponent;

/**
 * Os destroços da morte de tanque (docs/CONCEITO_EXPLOSOES.md §4b, a "estrela
 * de faíscas e destroços" do vídeo do Veo, 0.05–3 s):
 *  - chapas pretas recortadas, girando, com sombra, que caem e ficam no chão;
 *  - faíscas-cometa: cabeça branca-amarela e rastro de fumaça clara que engorda
 *    e se desfaz;
 *  - rastros presos a quem voa (a torre arremessada: fumaça escura com fogo
 *    perto da peça).
 * Tudo em três malhas procedurais (uma chamada de desenho cada) no espaço do
 * mundo: chapas e cabeças de topologia fixa, só com os vértices atualizados; os
 * rastros refeitos a cada quadro, reamostrados em poucos pontos. As chapas com sombra (M_Base), as cabeças aditivas
 * (M_DebrisGlow) e os rastros em fita virada pra câmera, com perfil redondo e a
 * borda comida por Worley (M_TrailSmoke; Tools/build_debris.py).
 * O relógio é o do simulador (pausa e câmera lenta valem).
 */
UCLASS()
class BLASTFX_API ABlastDebris : public AActor
{
	GENERATED_BODY()

public:
	ABlastDebris();

	/** Arremessa as chapas e os cometas a partir de OriginCm (o alto do casco). */
	void Start(const FVector& OriginCm, int32 Seed);

	/**
	 * O tiro (14/10, a v3; o Felipe: "o tiro acertando no tanque" como a arte 4.2, docs/concept/explosions/
	 * type_4.2_armor_sparks.webp). Os mesmos riscos, fagulhas, chapas e rastros da morte, noutros arranjos:
	 *  - StartImpact: o acerto na blindagem em PointCm (na superfície, normal Normal, o projétil vindo em ShellDir):
	 *    o clarão branco no metal, o leque de faíscas saindo pro lado do ricochete, lascas e uma luz curta;
	 *  - StartMuzzle: o clarão de boca em MuzzleCm, na direção Dir: o miolo, as línguas à frente e uma luz curta;
	 *  - StartTracer: o projétil de FromCm a ToCm a SpeedCmPerS: a cabeça acesa com o traçante e uma luz que anda.
	 */
	void StartImpact(const FVector& PointCm, const FVector& Normal, const FVector& ShellDir, int32 Seed);
	void StartMuzzle(const FVector& MuzzleCm, const FVector& Dir, int32 Seed);
	void StartTracer(const FVector& FromCm, const FVector& ToCm, float SpeedCmPerS, int32 Seed);

	/**
	 * O tanque ferido (16/10; os limiares do ZH, docs/TIRO_E_ACERTO.md): StartSmoke abre uma fumaça que sobe de onde o
	 * diretor puser a cabeça (SetTrailHead a cada quadro), e o ator não morre sozinho; StartFlameJet é o jato de chama
	 * pelo anel da torre na passagem pra "muito danificado" (0,3–0,5 s), com uma luz laranja.
	 */
	int32 StartSmoke(float Width0M, float GrowthMps, float LifeS, float Albedo, float Opacity0, float RiseMps);
	void StartFlameJet(const FVector& PointCm, int32 Seed);
	/**
	 * O fogo da carcaça (16/10; o estudo de War Thunder de 13/10: a fumaça manda, chamas pequenas nas aberturas, ritmo
	 * calmo, uma luz): chamas pequenas laranja que sobem e somem em FirePos (o diretor move a cada quadro), recicladas
	 * num grupo de até 14 (a malha das cabeças não é refeita), e a luz laranja tremendo. Apaga nos últimos 3 s.
	 */
	void StartWreckFire(float DurationS, bool bFlames = true);
	/** (17/10) Liga as chamas de sprite de um fogo que começou só com a luz (a carcaça: a luz logo depois da bola, as chamas 4 s depois). */
	void EnableWreckFlames() { bWreckFlames = true; }
	/** Os torrões do tiro na areia (16/10, a arte 4.1: o jato de terra escura): pedaços que sobem num cone, caem e ficam. */
	void StartDirt(const FVector& PointCm, const FVector& AwayDir, int32 Seed);
	/**
	 * O tiro na areia inteiro, menos o anel (16/10; o estudo com fontes em docs/TIRO_E_ACERTO.md, "O tiro na areia"): o
	 * clarão de ~2,5 m por 2 quadros e as faíscas, a luz curta, o jato de terra escura (torrões com rastro de terra,
	 * 50–70° do chão, mais pra frente na direção do tiro) e os torrões que caem e ficam. AwayDir = o rumo do tiro no chão.
	 */
	void StartGroundImpact(const FVector& PointCm, const FVector& AwayDir, int32 Seed);

	/**
	 * O míssil da Patriot (16/10; o estudo com fontes: docs/ESTUDO_MISSIL_PATRIOT.md): a chama atrás do bocal (miolo,
	 * língua e halo, tremendo), a luz quente que anda junto e o rastro de fumaça branca (um sopro grosso só na saída,
	 * depois o fio de cruzeiro, com o fogo do gás só nos ~2–3 m de trás). O diretor move a cabeça a cada quadro
	 * (UpdateMissile) e encerra no acerto (EndMissile).
	 */
	void StartMissile(const FVector& PosCm, const FVector& Dir, int32 Seed, bool bSmokeTrail = true);
	void UpdateMissile(const FVector& PosCm, const FVector& Dir);
	void EndMissile();
	/** O lançamento na boca da caixa: o clarão, a língua, o jato de ré pela grelha de trás e os pedaços da tampa. */
	void StartLaunch(const FVector& MouthCm, const FVector& Dir, int32 Seed);
	/** O acerto do míssil no tanque: o clarão branco-azulado, as faíscas radiais, as fagulhas e as lascas. */
	void StartMissileHit(const FVector& PointCm, const FVector& Normal, int32 Seed);
	/** O laser de mira entre as baterias (o LaserFromAssisted do ZH): um risco vermelho fino de A a B. */
	void StartLaser(const FVector& FromCm, const FVector& ToCm, float LifeS);

	/**
	 * O carro esmagado (16/10; o estudo com fontes: docs/ESTUDO_ESMAGAR_E_POSTE.md). O vidro temperado estoura em
	 * cubinhos de 2,5–8 mm que voam pouco (1–2 m/s) e piscam ao sol: cada caco tem uma normal sorteada que gira, e só
	 * brilha quando fica entre o sol e a câmera (o reflexo do vidro é estreito: é assim que se vê o vidro a 100 m mesmo com
	 * o caco menor que um pixel). Mais blocos de cubos ainda grudados que caem.
	 */
	void StartGlass(const FVector& CenterCm, const FVector& OutDir, int32 Seed);
	/** Lascas pequenas de uma cor (lanterna vermelha/âmbar, borracha do pneu): Count pedaços de SizeMin–SizeMax m. */
	void StartChips(const FVector& Cm, const FVector& Dir, int32 Count, float SizeMin, float SizeMax, float SpeedMin, float SpeedMax, int32 Seed);
	/** A faísca do raspão do casco no teto (rara e fraca de dia: 5–12 faíscas de 0,1–0,3 s). */
	void StartScrape(const FVector& Cm, const FVector& Dir, int32 Seed);
	/** O pisca-alerta: luz âmbar a 1,5 Hz por DurationS s. */
	void StartBlink(const FVector& Cm, float DurationS);
	/**
	 * O arco elétrico do fio de poste (16/10; o estudo: docs/ESTUDO_ESMAGAR_E_POSTE.md, parte B). Kind 0: o fio arrebentou
	 * no ar (os fios se tocando: o clarão de 3–4 quadros, 0,1–0,6 m, o chuveiro de 40–80 gotas e as lascas do isolador);
	 * 1: a ponta viva encostou no chão (o clarão rápido rente à areia); 2: a religada (o arco de 0,6–1,0 s, 0,3–1,5 m,
	 * pulando pela areia a cada 0,1–0,2 s, gotas o tempo todo). A fita é refeita todo quadro (o ponto médio deslocado em 4
	 * níveis); a 30–60 qps o piscar de 120 Hz da rede vira um brilho sorteado por quadro.
	 */
	void StartArc(const FVector& PointCm, int32 Kind, int32 Seed);
	/** A cor das chapas deste ator (R < 0 = o aço queimado) e a rugosidade (< 0 = a das chapas). */
	FLinearColor PlateTint = FLinearColor(-1.0f, 0.0f, 0.0f, 0.0f);
	float PlateRoughNow = -1.0f;
	void SetFirePos(const FVector& PointCm) { FirePos = PointCm / 100.0; }

	/** Depuração (-ShotFxLog=1): quantos pontos o rastro tem agora e se solta. */
	int32 TrailPointCount(int32 Trail) const { return Trails.IsValidIndex(Trail) ? Trails[Trail].Points.Num() : -1; }
	bool TrailEmitting(int32 Trail) const { return Trails.IsValidIndex(Trail) && Trails[Trail].bEmitting; }

	/**
	 * Antes do Start (o diretor do jogo): o tamanho da bola relativo à gravação (as faíscas encolhem e esquentam
	 * junto, 0,2–1; ≤ 0 = o -BlastScale), a cor da fuligem dos rastros e o brilho das cabeças e faíscas (1 = o do
	 * M_DebrisGlow). As opções do BlastLab (-BlastScale, -RmSoot, -DebrisGlow) continuam passando por cima.
	 */
	float BallScale = -1.0f;
	FLinearColor TrailTint = FLinearColor(0.95f, 0.92f, 0.88f, 1.0f);
	float GlowScale = 1.0f;
	/** A fração das peças da morte (16/10, o orçamento da morte em massa: o diretor põe 0,5 da 5ª morte em 3 s em diante). */
	float DetailK = 1.0f;

	/** Um rastro preso a um componente que o jogo move (a torre arremessada do RTSUnitActor): solta fumaça só
	 *  enquanto ele voa, e para de vez quando ele assenta. Devolve o índice. */
	int32 FollowComponent(USceneComponent* Comp, float Width0M, float GrowthMps, float LifeS, float Albedo, float GlowS, float Opacity0);

	/** Um rastro preso a algo que o simulador move (a torre). Devolve o índice. */
	int32 AddFollowTrail(float Width0M, float GrowthMps, float LifeS, float Albedo, float GlowS, float Opacity0);
	/** A cabeça do rastro agora (cm) e se ele ainda solta fumaça. */
	void SetTrailHead(int32 Trail, const FVector& HeadCm, bool bEmitting);

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> PlateMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> GlowMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> TrailMesh;

	/** A luz do tiro (clarão de boca, acerto, projétil): só nos modos do tiro, sem sombra. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Light;

private:
	struct FTrailPoint
	{
		FVector P = FVector::ZeroVector;    // m, mundo
		float Age = 0.0f;
		float Dist = 0.0f;                  // m ao longo do rastro (a coordenada do ruído)
	};
	struct FTrail
	{
		TArray<FTrailPoint> Points;
		FVector Head = FVector::ZeroVector;
		bool bEmitting = true;
		float Width0 = 0.1f, Growth = 0.5f, Life = 2.5f, Albedo = 0.4f, GlowS = 0.0f, Opacity0 = 0.6f;
		float Seed = 0.0f, Age = 0.0f;
		float Rise = -1.0f;                         // m/s que a fumaça sobe (< 0 = a dos destroços)
		FVector Wind = FVector::ZeroVector;         // m/s que ela deita com o vento (a fumaça parada; os destroços não)
		float GlowTau = 0.25f;                      // s de idade em que o fogo do gás atrás da cabeça apaga (o míssil: 0,025)
	};
	struct FPlate
	{
		FVector P = FVector::ZeroVector, V = FVector::ZeroVector;   // m, m/s
		FQuat Q = FQuat::Identity;
		FVector Spin = FVector::ZeroVector;                          // eixo · rad/s
		float Size = 0.5f;
		int32 Shape = 0;
		int32 Trail = -1;
		int32 Bounces = 0;
		bool bRest = false;
	};
	struct FComet
	{
		FVector P = FVector::ZeroVector, V = FVector::ZeroVector;
		float Burn = 1.5f, Age = 0.0f;
		int32 Trail = -1;
		// o projétil: voa reto, sem gravidade nem arrasto, com a cabeça e a cor dele (A < 0 = as da morte)
		bool bStraight = false;
		float HeadM = -1.0f;
		FLinearColor Color = FLinearColor(0.0f, 0.0f, 0.0f, -1.0f);
	};
	/** Faísca: espinho (rápido, curto, risco esticado) ou fagulha (lenta, cai piscando). */
	struct FSpark
	{
		FVector P = FVector::ZeroVector, V = FVector::ZeroVector;
		float Age = 0.0f, Life = 0.3f, Size = 0.08f, Bright = 1.0f, Drag = 0.0f, Lift = 0.0f, Seed = 0.0f;
		bool bEmber = false;
		// o tiro: o quanto cresce (m/s, a bola secundária do disparo), a cor dela (A < 0 = a dos espinhos/fagulhas) e
		// a espessura mínima na tela (o traçante ≥ 3 px; as faíscas do acerto 1,5–2 px)
		float Grow = 0.0f;
		FLinearColor Color = FLinearColor(0.0f, 0.0f, 0.0f, -1.0f);
		float MinPxK = 0.0f;                        // 0,00075 ≈ 3 px na câmera do jogo; 0,0004 ≈ 1,5–2 px
		// o borrão do risco (s de voo esticados; 1/30 = o quadro) e o clarão redondo (parado: sem isto o perfil do
		// material só arredonda a largura e o quad vira um retângulo, 16/10)
		float StreakT = 1.0f / 30.0f;
		bool bRound = false;
		bool bBounce = false;                       // quica na areia em vez de morrer (as faíscas do acerto)
		bool bCool = false;                         // esfria do branco-amarelo pro laranja ao longo da vida (o metal: ~2200 → ~1300 K)
		bool bTrail = false;                        // o risco inteiro atrás do ponto (o traçante: a cabeça na frente)
		bool bPinned = false;                       // não anda pela velocidade (a chama do míssil, o laser: o diretor põe o ponto)
		bool bGlint = false;                        // um caco de vidro: brilha só com a normal entre o sol e a câmera
		FVector GlintN = FVector::UpVector, GlintSpin = FVector::ZeroVector;
	};
	struct FShape
	{
		TArray<FVector> Verts;              // m, chapa de 1 m
		TArray<int32> Tris;
		TArray<FVector> Normals;
	};

	struct FFollow
	{
		int32 Trail = -1;
		TWeakObjectPtr<USceneComponent> Comp;
		FVector Last = FVector::ZeroVector;    // cm
		float Still = 0.0f;                    // s parado depois de ter voado
		bool bFlew = false, bDone = false;
	};
	TArray<FFollow> Follows;
	void UpdateFollows(float Dt);
	float SparkScaleNow() const;

	void BuildShapes();
	void BuildPlateSection();
	void Integrate(float Dt);
	void UpdatePlates();
	void UpdateGlows(const FVector& CamM);
	void UpdateTrails(const FVector& CamM);
	int32 NewTrail(float Width0M, float GrowthMps, float LifeS, float Albedo, float GlowS, float Opacity0);

	FRandomStream Rand;
	TArray<FShape> Shapes;
	TArray<FPlate> Plates;
	TArray<FComet> Comets;
	TArray<FSpark> Sparks;
	TArray<FTrail> Trails;
	float T = 0.0f;
	bool bPlateSection = false;
	int32 PlateVerts = 0;
	// a seção das cabeças já criada (com quantos vértices): depois só atualizar
	int32 GlowVerts = -1;
	int32 TrailVerts = -1;                          // a seção dos rastros já criada (com quantos vértices): depois só atualizar
	// o que já acabou e não precisa mais ir pra placa (14/10, otimização sem mudar a imagem): as chapas deitadas, as
	// cabeças e faíscas apagadas, os rastros desfeitos. Com tudo acabado o ator para de vez até o fim da vida (120 s)
	bool bPlatesSettled = false, bGlowsDone = false, bTrailsDone = false;
	// a luz do tiro: o pico (cd), a queda (s), a idade, a cor e o cometa que ela segue (-1 = parada em LightAt)
	float LightPeak = 0.0f, LightTau = 0.05f, LightAge = 0.0f;
	// a compensação pela exposição (16/10): a de agora e a aplicada nos materiais
	float ExpoComp = 1.0f, ExpoApplied = -1.0f, GlowNow = 1.0f;
	// o chão perto do centro do efeito, consultado uma vez (16/10, o custo de processador: cada faísca consultava 4 nós do
	// mapa de alturas a cada quadro); fora de 6 m do centro segue a consulta por peça
	FVector OriginM = FVector::ZeroVector;
	// o míssil: as faíscas da chama (miolo, língua, halo), os rastros (sopro, cruzeiro), quando nasceu e a luz presa nele
	float BlinkUntil = -1.0f;                       // o pisca-alerta: até quando
	// os arcos do fio: as pontas (m), a âncora (a ponta do fio), a vida, as larguras (meia largura do núcleo e do halo), a
	// amplitude do zigue-zague (fração do comprimento) e o pulo pela areia (o raio e quando)
	struct FArc
	{
		FVector A = FVector::ZeroVector, B = FVector::ZeroVector, Anchor = FVector::ZeroVector;
		float Age = 0.0f, Life = 0.06f, Core = 0.035f, Halo = 0.16f, Amp = 0.16f;
		float JumpR = 0.0f, NextJump = 1.0e9f, GroundZ = 0.0f;
		int32 Seed = 0;
	};
	TArray<FArc> Arcs;
	float ArcLightUntil = -1.0f;                    // o arco temporizado: a luz piscando até quando
	FVector ArcGroundPoint(const FArc& Ac);
	int32 MissileSpark[3] = { -1, -1, -1 };
	int32 MissileTrail[2] = { -1, -1 };
	float MissileT0 = -1.0f;
	bool bLightHold = false;
	bool bHasOrigin = false;
	float GroundRefM = -1.0e9f;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlowMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TrailMID;
	int32 LightFollow = -1;
	FVector LightAt = FVector::ZeroVector;
	void EnsureLight(const FLinearColor& Color, float RadiusCm);
	void UpdateLight(float Dt);
	// o fogo da carcaça: até quando, o ponto (m), o acumulado de chamas por nascer e as vagas delas nas faíscas
	float FireUntil = -1.0f, FireAcc = 0.0f;
	bool bWreckFlames = true;   // (17/10) false: a coluna em volume faz o fogo; aqui só a luz
	FVector FirePos = FVector::ZeroVector;
	TArray<int32> FireSlots;
	void UpdateWreckFire(float Dt);
};
