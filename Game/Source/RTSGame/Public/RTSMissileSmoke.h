// O rastro de fumaça dos mísseis (16/10, o Felipe: "o Patriot solta mísseis com fumaça e não borrões brancos igual tiro de
// tanque ... bem pensado, bem estruturado, bem consultado, algoritmos, matemática, bem simulado"; o estudo em
// docs/FUMACA_DO_MISSIL.md). Só desenho: nada volta pra simulação (o lockstep fica igual em todo PC).
//
// O que é simulado, por "puff" (a nuvem gaussiana da dispersão atmosférica, solta a cada ~0,45 m do caminho do míssil):
//   - a fumaça por metro é a vazão dividida pela velocidade (perto da caixa, a 20 m/s, sai ~5× mais densa que no fim);
//   - o jato abre em ~0,06 s até ~4–5 diâmetros do míssil; depois a pluma engorda pela turbulência do ar: a lei de
//     Richardson (σ² ∝ ε^⅓ σ^⅔ t, os redemoinhos menores que o puff) até a escala da camada, e aí a difusão de K
//     (K = κ u* z, a camada junto do chão neutra); a massa se conserva, então a densidade óptica cai sozinha;
//   - o gás quente sobe (o empuxo que esfria pela mistura) e o vento do mapa leva tudo, mais forte no alto (o perfil
//     logarítmico); os redemoinhos maiores que o puff entortam o rastro (modos de Fourier sem divergência, Kraichnan);
//   - quem passa empurra: o veículo pelo escoamento em volta de uma esfera (o dipolo) e a esteira atrás dele, que mistura
//     e rasga; o tiro e o outro míssil furam um túnel; a explosão sopra pra fora (a onda e o vento atrás dela);
//   - a luz: a sombra de um puff no outro rumo ao sol (uma grade de densidade, a marcha de raio), o céu por cima, as
//     luzes perto (as explosões, o fogo, os tiros: RTSSceneLights, somadas por puff), e no material a esfera iluminada
//     pelas duas, o espalhamento pra frente (o contraluz prateado);
//   - (23/10) a sombra no chão, nas unidades e nos prédios: a coluna de cada puff vista do sol, no mapa do sol que a light
//     function lê (RTSSmokeShadow, o mesmo das explosões e da poeira; rts.MissileSmoke.ShadowPath).
// A leitura do RTS: a fumaça na frente de uma unidade (entre a câmera e ela) rareia; o rastro some antes de virar
// cortina (os presets cortam a vida).
#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "UObject/StrongObjectPtr.h"

class AActor;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class UWorld;

/** Um míssil em voo neste quadro (cm, unitária, cm/s). */
struct FRTSSmokeMissile
{
	uint32 Id = 0;
	FVector PosCm = FVector::ZeroVector;
	FVector Dir = FVector::ForwardVector;
	float SpeedCmS = 0.0f;
	float AgeS = 0.0f;
};

/** Um veículo andando (a esfera que empurra o ar) neste quadro. */
struct FRTSSmokeMover
{
	FVector PosCm = FVector::ZeroVector;
	FVector VelCmS = FVector::ZeroVector;
	float RadiusCm = 260.0f;
};

/** (16/10) Um veículo a diesel neste quadro, perto da câmera e à vista (o estudo: docs/DIESEL_DO_TANQUE.md). */
struct FRTSSmokeExhaust
{
	uint32 Id = 0;
	FVector PosCm = FVector::ZeroVector;      // o centro do veículo
	FVector FwdW = FVector::ForwardVector;    // a frente do casco (mundo)
	float SpeedMs = 0.0f;                     // a velocidade da simulação (m/s)
	float YawRateDps = 0.0f;
	bool bWorking = false;                    // o Dozer na obra (a lâmina empurrando)
	/** As saídas (mundo): o ponto (cm) e pra onde o gás sai (unitária). */
	TArray<TPair<FVector, FVector>> Outlets;
	// o motor (a ficha visual, Units.json "exhaust")
	float PowerKw = 1100.0f;
	float MassT = 60.0f;
	float Roll = 0.12f;                       // a resistência ao rolamento no chão de areia (adimensional)
	float MaxSpeedMs = 8.0f;
	float TurnDps = 90.0f;
	float WorkLoad = 0.0f;
	int32 Gears = 4;
	float JetSigmaM = 0.4f;                   // o σ do jato logo depois da saída
	float ExitMs = 25.0f;                     // a velocidade do gás na saída, a plena carga
};

/**
 * (23/10, a explosão com vida; BlastFxHitLife.h) Um ponto que fumega neste quadro: o furo do acerto no casco (anda com o tanque), a cratera
 * do míssil no chão, o pedaço em brasa que caiu. O jogo passa a posição e a força de agora; a fumaça sai em puffs dali (no mesmo sistema do
 * rastro: o vento, os empurrões, a luz da cena, a sombra no mapa do sol e a leitura do RTS valem de graça).
 */
struct FRTSSmokeSmolder
{
	uint32 Id = 0;                            // estável enquanto fumega (o mesmo alvo renova o tempo, não soma)
	FVector PosCm = FVector::ZeroVector;
	float RatePerS = 4.0f;                    // puffs por segundo
	float ExtM2 = 0.1f;                       // a extinção de cada puff (m²: a massa de fuligem)
	float SigmaM = 0.25f;                     // o σ do puff ao nascer
	float RiseMs = 1.5f;                      // a subida (a fumaça morna, 1–2 m/s)
	uint8 Kind = 1;                           // 1 a fuligem (escura, absorve: a do diesel); 0 a clara (a do míssil)
	/** (a revisão da vida) O ponto que anda (o pedaço em brasa em voo): > 0 solta um puff a cada StepM m do caminho desde o quadro de antes
	 *  (como o rastro do míssil; o puff já nasce com o SigmaM), no lugar do ritmo RatePerS; e o rastro do corpo que voa se dilui na esteira: a
	 *  massa visível de cada puff cai em e^(−idade/FadeTauS) (0 = conserva, a de sempre) e ele sai aos 3·FadeTauS. */
	float StepM = 0.0f;
	float FadeTauS = 0.0f;
};

/** O que o jogo passa a cada quadro. */
struct FRTSSmokeFrame
{
	TArray<FRTSSmokeMissile> Missiles;
	TArray<FRTSSmokeExhaust> Exhausts;
	/** (23/10, a vida do acerto) Os pontos que fumegam. */
	TArray<FRTSSmokeSmolder> Smolders;
	TArray<FRTSSmokeMover> Movers;
	/** Os tiros em voo: o trecho que o projétil andou neste quadro (de, até), cm. */
	TArray<TPair<FVector, FVector>> Shots;
	/** O alto das unidades que têm de ficar legíveis (a fumaça na frente delas rareia). */
	TArray<FVector> UnitTopsCm;
	FVector CamCm = FVector::ZeroVector;
	FVector CamForward = FVector::ForwardVector;
	FVector CamRight = FVector::RightVector;
	FVector CamUp = FVector::UpVector;
	/** O passo da simulação (os tiros valem uma vez por passo, mesmo com vários quadros no passo). */
	int64 SimTick = -1;
	/** A altura do chão (cm) num ponto (cm). */
	TFunction<float(double, double)> GroundZCm;
};

class FRTSMissileSmoke
{
public:
	/** Carrega os materiais e cria a malha; false sem os materiais (aí o rastro fica o do BlastFx). */
	bool Init(UWorld* World);
	void Shutdown();
	bool IsReady() const { return bReady; }

	/** O lançamento quente: o motor acende na caixa e a ré dela sopra o chão (a nuvem branca que envolve a bateria). */
	void Launch(const FVector& MouthCm, const FVector& Dir, uint32 Seed);
	/** Uma explosão perto: a bola de fogo empurra o ar em volta (o deslocamento incompressível, Δr = ∛(r³ + R³) − r),
	 *  esquenta a fumaça de dentro e sobe levando a de perto (o anel). RadiusM = o raio da bola; RiseMs = a subida dela. (23/10, a vida)
	 *  RiseTauS: o τ da subida que puxa a fumaça perto pra coluna (1,5 s de sempre; com a nuvem viva da ogiva, o que ela dura). */
	void Blast(const FVector& AtCm, float RadiusM, float RiseMs, float RiseTauS = 1.5f);
	/** O sopro de boca de um canhão (pra frente, num cone). */
	void Muzzle(const FVector& AtCm, const FVector& Dir);

	void Tick(float DeltaSeconds, const FRTSSmokeFrame& Frame);

	int32 NumPuffs() const;
	double LastCostMs = 0.0;
	/** O vento do mapa (Maps.json, o mesmo do MPC_Wind): pra onde sopra (unitária) e a velocidade a 10 m, m/s. */
	FVector2D WindDir = FVector2D(1.0, 0.0);
	float WindMs = 3.0f;
	/** A vida máxima de um puff (s). De verdade o rastro dura minutos; o jogo comprime (a turbulência acelera de 2 a 5 s)
	 *  e corta antes de virar cortina: ultra 8, alta 6, média 4,5, baixa 3. */
	float MaxLifeS = 6.0f;
	/** (17/10) A vida do preset, guardada no 1º quadro (a v6 encurta a partir dela). */
	float BaseLifeS = -1.0f;
	/** (17/10) A versão do visual (rts.MissileSmoke.Look): 7 a do contorno visível (o rastro nasce denso e fino, se parte em
	 *  bolotas e some pela borda em ~2–3 s; o Felipe reprovou a 5 e a 6), 6 e 5 as de antes, pro A/B. */
	int32 LookVersion = 7;

private:
	struct FPuff
	{
		FVector P = FVector::ZeroVector;       // m
		FVector V = FVector::ZeroVector;       // m/s, a velocidade própria (o jato, os empurrões), que relaxa pra do ar
		float S2 = 0.0f;                       // σ² da difusão (m²), somado ao do jato
		float Extinction = 0.0f;               // a extinção integrada do puff (m²): a "massa" que se conserva
		float Age = 0.0f;
		float Heat = 0.0f;                     // 0..1, o calor de uma explosão perto (brilha e sobe)
		float Buoy = 0.0f;                     // o fluxo de empuxo por metro do rastro, B (m³/s²): a térmica em linha de Briggs
		float JetMax = 0.45f;                  // o σ do jato aberto (m): grosso onde o míssil ia devagar
		float Seed = 0.0f;
		float SunT = 1.0f;                     // a transmitância até o sol (os outros puffs no caminho)
		float SkyT = 1.0f;                     // a do céu por cima
		float CoverScale = 1.0f;               // a densidade multiplicada pra a soma na frente de uma unidade ficar no limite
		FLinearColor LocE = FLinearColor::Black;   // a luz das explosões, do fogo e dos tiros perto (lux, RGB; RTSSceneLights)
		FVector LocDir = FVector::UpVector;    // pra onde ela está (a média das direções pesada pelo brilho de cada uma)
		uint8 Kind = 0;                        // 0 a fumaça do míssil (Al2O3, branca); 1 a fuligem do diesel (escura, absorve)
		float FadeTauS = 0.0f;                 // (a revisão da vida) > 0: a massa visível cai em e^(−idade/FadeTauS) (a esteira do pedaço em brasa)
		float RelaxS = 0.35f;                  // o tempo pra acompanhar o ar (o jato do escape freia mais rápido)
		FVector Turb = FVector::ZeroVector;    // os redemoinhos (m/s), recalculados num quadro sim e outro não
		/** (17/10, a v7) O corte do contorno visível relativo ao centro: τ*(idade) / τ da linha aqui (≥ 1 = não se vê). */
		float Cut = 0.0f;
		/** (23/10) O mesmo corte com a linha vista do sol (a direção do mapa do sol): o contorno da sombra, que se parte e some
		 *  junto com o rastro sem depender de pra onde a câmera gira (≥ 1 = não faz sombra). */
		float CutSun = 0.0f;
		/** (23/10) O chão debaixo dele (m): a parte do puff abaixo do chão não faz sombra. */
		float GroundM = 0.0f;
		/** (17/10, a v7e) A força deste trecho do rastro (a gama de média 1 ao longo do caminho): divide o corte do contorno,
		 *  então os trechos fracos somem primeiro e o rastro se parte com a idade; a densidade fica lisa (o tubo novo sem bolotas). */
		float Fluct = 1.0f;
		/** (17/10, a v7d) O rumo do rastro aqui (os vizinhos do caminho; zero na nuvem da largada): o material sombreia como tubo. */
		FVector Axis = FVector::ZeroVector;
		float Sigma() const;
	};
	struct FTrail
	{
		uint32 Missile = 0;
		bool bLive = false;                    // o míssil ainda voa
		FVector HeadM = FVector::ZeroVector;   // o último ponto solto (m) e a direção ali
		FVector HeadDir = FVector::ForwardVector;
		float HeadSpeed = 0.0f;
		float Carry = 0.0f;                    // o pedaço de caminho que sobrou pra próxima soltura (m)
		float SinceSeen = 0.0f;
		// (17/10, a v7) a massa por metro ao longo do rastro: o trecho entre dois nós (o que falta, o comprimento) e a massa dos
		// dois (a gama de média 1)
		float BlobLeftM = 0.0f;
		float BlobLenM = 3.0f;
		float BlobPrevG = 1.0f;
		float BlobG = 1.0f;
		TArray<FPuff> Puffs;                   // na ordem do caminho (o mais velho primeiro)
	};
	struct FBlast
	{
		FVector P = FVector::ZeroVector;
		float R = 3.0f;                        // o raio da bola de fogo (m)
		float W = 3.0f;                        // a subida do quente no centro (m/s), o anel que leva a fumaça junto
		FVector Dir = FVector::ZeroVector;     // ≠ 0: o sopro de boca, num cone
		float Age = 0.0f;
		float RiseTau = 1.5f;                  // (23/10) o τ da subida (s)
	};
	/** (23/10, a vida do acerto) Os pontos que fumegam: os puffs pelo caminho de cada um (FRTSSmokeSmolder). */
	void TickSmolders(float Dt);

	void Emit(FTrail& T, const FVector& ToM, const FVector& ToDir, float SpeedMs, float MissileAge);
	/** (16/10) O diesel: o motor de cada veículo (o acelerador, o turbo que atrasa, as marchas) e a fuligem pelas saídas. */
	void TickExhausts(float Dt);
	struct FEngine
	{
		float U = 0.0f;                        // a velocidade que o motor já deu ao veículo (m/s): sobe pela potência
		float Speed = 0.0f;                    // a da simulação, suavizada
		float Throttle = 0.0f;                 // o acelerador 0..1
		float Boost = 0.0f;                    // a pressão do turbo 0..1 (segue o acelerador com atraso)
		float Shift = 0.0f;                    // a troca de marcha em andamento (s que faltam)
		int32 Gear = 0;
		float Carry = 0.0f;                    // a fuligem acumulada pro próximo puff (m² de extinção)
		float Clock = 0.0f;                    // o tempo desde o último puff
		float Unseen = 0.0f;
		bool bInit = false;
		TArray<FVector> LastOutletM;           // onde cada saída estava no quadro de antes (m)
	};
	TMap<uint32, FEngine> Engines;
	FVector AirVelocity(const FVector& P, float Sigma, FVector* InOutTurb) const;
	/** (17/10, a carga das 25 Patriots) Quem encosta na caixa de um rastro: os índices dos veículos, dos trechos de tiro e
	 *  das explosões que podem mexer em algum puff dele (cada rastro roda numa thread com a sua lista). */
	struct FNear
	{
		TArray<int32, TInlineAllocator<8>> Movers;
		TArray<int32, TInlineAllocator<8>> Shots;
		TArray<int32, TInlineAllocator<8>> Blasts;
	};
	void NearOf(const FTrail& T, FNear& Out) const;
	void ApplyMovers(FPuff& Pf, float Sigma, float Dt, const FNear& Near) const;
	void Merge(FTrail& T);
	/** (17/10, a v7) O contorno visível de cada puff: a coluna de fumaça no centro dele (a soma dos vizinhos no plano da vista)
	 *  contra o τ* da idade. */
	void Cuts();
	/** (17/10, a v7) O deslocamento médio dos puffs visíveis (m), somado a cada quadro: o ruído do material anda com ele. */
	FVector NoiseOffM = FVector::ZeroVector;
	/** (17/10) O orçamento da tela: o τ* multiplicado quando a massa visível perto do que a câmera olha passa do limite
	 *  (a base com 25 Patriots), suavizado em ~0,5 s; e a última massa medida (m²), pro registro. */
	float BudgetK = 1.0f;
	float ViewMassM2 = 0.0f;
	void Light();
	void Readability();
	void BuildMesh();
	/** (23/10) A sombra no mapa do sol (RTSSmokeShadow; rts.MissileSmoke.ShadowPath 2, o padrão): os puffs que fazem sombra vão pro
	 *  mapa que a light function do sol lê, o mesmo das explosões e da poeira. Sem o mapa vivo em 1 s com puffs (o ambiente do
	 *  BlastLab, -Raymarch=0, -RmShadow=0), voltam os quadrados no chão de antes (ShadowPath 1). */
	void ExportShadow(float Dt);
	/** A direção pro sol do mapa (com o piso de elevação das sombras das fumaças); sem o mapa, a do sol com o mesmo piso. */
	FVector ShadowSunDir() const;
	float MapMissS = 0.0f;                     // há quanto tempo há puffs sem o mapa vivo (s)
	bool bShadowQuads = false;                 // neste quadro a sombra vai pelos quadrados de antes
	int32 ShadowExported = 0;                  // os puffs mandados pro mapa no último quadro

	TWeakObjectPtr<UWorld> WorldRef;
	TWeakObjectPtr<AActor> Actor;
	TWeakObjectPtr<UProceduralMeshComponent> Mesh;
	TWeakObjectPtr<UProceduralMeshComponent> ShadowMesh;
	// referências FORTES (22/10, v4): com TWeakObjectPtr nada segurava os dois materiais, a coleta de lixo do motor (a cada
	// 61 s) apagava, e as malhas caíam no material padrão: as "faixas cinza" atrás do Patriot e o rastro sumido, em toda
	// partida onde o 1º tiro vinha depois do 1º minuto
	TStrongObjectPtr<UMaterialInstanceDynamic> SmokeMID;
	TStrongObjectPtr<UMaterialInstanceDynamic> ShadowMID;
	bool bReady = false;
	/** O material lê as luzes perto (o parâmetro LocalLightsV1): a normal do vértice leva a direção da luz e a cor a
	 *  iluminância (RGBE); o material velho lê a normal como a da vista. */
	bool bLocalLightsMat = false;

	TArray<FTrail> Trails;
	TArray<FBlast> Blasts;
	FRTSSmokeFrame F;                          // o quadro atual (a câmera, o vento, os que passam)
	double Clock = 0.0;
	uint32 SeedCounter = 1;
	int64 LastShotTick = -1;
	float LogClock = 0.0f;

	// o vento e a turbulência do quadro
	float UStar = 0.2f;                        // a velocidade de atrito (m/s)
	FVector WindUnit = FVector(1.0, 0.0, 0.0);
	struct FMode
	{
		FVector K = FVector::ZeroVector;       // o número de onda (rad/m)
		FVector A = FVector::ZeroVector;       // a amplitude (m/s), ⊥ K
		float Omega = 0.0f;
		float Phase = 0.0f;
	};
	TArray<FMode> Modes;
	float ModesForWind = -1.0f;

	// o sol (da luz direcional do mapa), relido a cada segundo
	FVector SunDir = FVector(0.0, 0.0, 1.0);   // pro sol
	FLinearColor SunLux = FLinearColor::Black; // a iluminância do sol no chão (lux): a cor × a intensidade × a atmosfera
	/** (17/10) A luz do céu num chão plano (lux): o SH da SkyLight convoluído com o cosseno × a cor × a intensidade. O
	 *  material da sombra no chão (que multiplica a cena) não recebe o céu do motor. */
	FLinearColor SkyIrrLux = FLinearColor::Black;
	bool bSunFound = false;
	float SunClock = 1.0e9f;

	// a malha: capacidade em quads (cresce de 1024 em 1024; os que sobram ficam num ponto só)
	int32 Capacity = 0;
	int32 ShadowCapacity = 0;
	TArray<int32> ShadowTris;
	int32 TurbParity = 0;
	TArray<FVector> Verts, ShadowVerts;
	TArray<FVector> Normals, ShadowNormals;
	TArray<FProcMeshTangent> Tangents, ShadowTangents;
	TArray<FVector2D> UV0, UV1, UV2, UV3, SUV0, SUV1, SUV2, SUV3;
	TArray<FColor> Colors;
	int32 LightParity = 0;
	float CostSplit[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	// o registro (16/10): o maior brilho das luzes perto num puff (lux) e quantos passam de 1% do sol, desde o último log
	float LocalMaxLux = 0.0f;
	int32 LocalLitPuffs = 0;
	TArray<int32> Tris;
};
