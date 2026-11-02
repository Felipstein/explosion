#pragma once

// O diretor das explosões no jogo: o que o UBlastLabSimulation faz pras
// explosões no simulador, sem o resto do laboratório (painel, armas de teste,
// passagens de avião). Dá às explosões o relógio, a vista da câmera (pra saber
// o quanto cada uma ocupa da tela) e o orçamento de detalhe; dispara a morte de
// tanque; aquece os shaders na abertura.

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BlastFxDirector.generated.h"

class AExplosionVolume; namespace BlastFxHitLife { struct FHitIn; struct FHitOut; struct FLanding; struct FEmberSmoke; }   // (numa linha só: o GENERATED_BODY não muda de linha, o cc_check usa o .generated.h da última compilação)
class UExplosionBake;
class UMaterialInterface; class UStaticMeshComponent;

namespace BlastFx
{
	/** O quanto a deformação pelo que passa empurra a fumaça (BattleSmokeSim.h no BlastLab). Sem campo no jogo ainda. */
	constexpr float WarpMaxLocalM = 8.0f;
}

UCLASS()
class BLASTFX_API UBlastFxDirector : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBlastFxDirector* Get(const UObject* WorldContext);

	/** O relógio das explosões (para com a partida pausada). */
	float SimDelta(float DeltaSeconds) const { return bPaused ? 0.0f : DeltaSeconds; }
	void SetPaused(bool bInPaused) { bPaused = bInPaused; }

	/** Fração da meia altura da tela que a esfera ocupa (0 fora da vista), como no BlastLab. */
	float ScreenFraction(const FVector& Center, float Radius, bool& bInView) const;
	float AmbientLux() const { return 52000.0f; }

	/** A cor da poeira que a explosão levanta (a areia do deserto). */
	FLinearColor DustTint = FLinearColor(0.80f, 0.68f, 0.55f);

	/**
	 * A morte de um tanque: a Carga Pesada aprovada (uma das 3 variantes) na
	 * proporção medida contra o vídeo da cena herói (bola 0.55, poeira com 40% da
	 * altura e metade da vida; BlastLabSimulation.cpp Detonate). Variant escolhe a
	 * gravação e o ruído (sorteio só visual).
	 */
	/** WreckOf (17/10): o ator do tanque morto; com ele a carcaça acende pela morte, qualquer que seja o caminho dela (antes
	 *  só o tiro que matava pelo ArmorHit acendia, e a morte por outro caminho ficava sem fogo). */
	/** (30/10) SizeK: o tamanho da morte (Units.json visual.death_scale; 1 = o tanque de 60 t, a aprovada; docs/ESTUDO_IMPACTOS.md D: a
	 *  Technical 0,4, o Stryker 0,6, o Linebacker 0,75): a bola, o relógio (× √K), a nuvem que fica, o anel de areia, os destroços e o fogo
	 *  da carcaça. */
	AExplosionVolume* TankDeath(const FVector& Ground, uint32 Variant, bool bClassic = false, AActor* WreckOf = nullptr, float SizeK = 1.0f, int32 DeathKind = 0,
		FName JetBake = NAME_None, const FVector& JetRingCm = FVector::ZeroVector, float JetScale = 1.0f, float JetBaseCm = 0.0f);
	/** (30/10, a obra com vida) A solda na estrutura da obra: o clarão branco-azulado curto, a luz e as faíscas laranja que caem. */
	void WeldSpark(const FVector& PointCm, const FVector& OutDir, uint32 Seed);
	/** (30/10, o DozerDirtFall do ZH) A terra caindo da borda da lâmina do Dozer quando ela sobe, e a areia que levanta no pouso. */
	void DozerDirtFall(const FVector& LipCm, const FVector& Fwd, float HalfWidthCm, uint32 Variant);
	/** (30/10, a assinatura da morte da munição: o Linebacker) os estouros que vêm depois (cook-off) e o míssil que sai torto. */
	struct FCookOff { double At = 0.0; FVector Cm = FVector::ZeroVector; uint32 Seed = 0; bool bMissile = false; };
	TArray<FCookOff> CookOffs;
	/**
	 * (24/10, o escopo B do ar; docs/AEREO.md) O fogo que queima: a gravação do EmberGen em loop (eg_fogo_ar_64, a eg_wreck_loop5 de
	 * 13/10 em 64³: a chama baixa do anel que vira fuligem e se desfaz no alto, com a luz física dela a cada quadro), no raymarch como
	 * as explosões (a sombra do mapa, a sombra no chão, as luzes perto). Pra aeronave caindo (quem chama move o ator junto do motor a
	 * cada quadro) e pra carcaça dela. Scale: a escala da gravação (0,17 = a chama do anel da torre, 1,2 m de raio); a vida e a
	 * entrada/saída (a Opacity, o Destroy) são de quem chama: o ator NÃO entra na lista das explosões (o fim dela esmaece pelo fim da
	 * gravação, e a de loop piscaria em cada emenda). Nulo sem a gravação.
	 */
	AExplosionVolume* FireLoop(const FVector& FootCm, float Scale, uint32 Variant);

	/**
	 * (25/10, o fogo simulado; docs/FOGO.md) O fogo das carcaças simulado de verdade: a gravação do firesim (fire_wreck: o anel da torre
	 * arrancada queimando em escala real, BlastLab/Tools/firesim) no raymarch, com a vida inteira: o início, o laço sem emenda (o fim de
	 * cada volta misturado com o começo dela) e o fim simulado (a chama baixa e vira brasa e fuligem). A gravação gira pro vento do mapa
	 * (e acompanha o vento ao vivo); a luz da cena segue a potência simulada. blast.Fire.Version: 1 = o de hoje (o padrão até o Felipe
	 * aprovar), 2 = este. GroundCm: o chão sob o meio do casco; Scale: 1 = o tanque; BurnS: quanto queima (o fim simulado acaba até
	 * aí); LightOf: a luz da carcaça que já existe (nulo = nasce uma). Devolve o id (−1 sem a gravação).
	 */
	/** (30/10, item 29) A areia da bala: 2 = a nova (padrão), 1 = a de antes (-BulletSandV=1). */
	static int32 BulletSandVersion();
	static int32 FireVersion();
	int32 StartFire(const FVector& GroundCm, float Scale, uint32 Variant, float BurnS, class ABlastDebris* LightOf = nullptr);
	/** Apaga em até WithinS s: sai do laço no ponto em que o fim simulado cabe; se não couber, esmaece. */
	void FireOut(int32 Id, float WithinS);
	/** (31/10, a sucata) Sai do laço na primeira saída que ainda vem (sem prazo: a chama baixa já, pelo fim simulado) e devolve os segundos
	 *  até o último quadro (−1 = sem esse fogo). */
	float FireEndSoon(int32 Id);
	bool FireAlive(int32 Id) const;
	/** O chão sob o meio do casco agora (a carcaça empurrada ou arrastada leva o fogo junto). */
	void FireFollow(int32 Id, const FVector& GroundCm);

	/**
	 * O tiro (14/10, a v3; o Felipe: o acerto como a arte 4.2, a areia que o sopro do disparo levanta em volta da torre,
	 * a areia da onda de choque no tanque atingido, o projétil com luz). Chamados pelo jogo nos eventos da simulação
	 * (WeaponFired, o traçante, ProjectileHit). -ShotFx=0 desliga: o jogo volta pro provisório dele.
	 *  - MuzzleBlast: a boca do cano (cm) e a direção; o chão embaixo (cm) pra areia do sopro.
	 *  - Tracer: o projétil de From a To (cm) na velocidade da simulação (cm/s).
	 *  - ArmorHit: o acerto no tanque Target: o ponto do jogo (o centro) e de onde veio o tiro; o efeito nasce na caixa
	 *    do próprio tanque, no lado virado pro atirador. bKill: o tiro que matou (a morte vem junto: só o clarão e as faíscas).
	 *  - GroundHit: o tiro no chão (o Ctrl, e os que passam raspando).
	 */
	static bool ShotFxEnabled();
	void MuzzleBlast(const FVector& MuzzleCm, const FVector& Dir, float GroundZCm, uint32 Variant);
	void Tracer(const FVector& FromCm, const FVector& ToCm, float SpeedCmPerS, uint32 Variant);
	void ArmorHit(const FVector& AtCm, const FVector& FromCm, AActor* Target, float GroundZCm, uint32 Variant, bool bKill);
	void GroundHit(const FVector& AtCm, const FVector& FromCm, uint32 Variant);
	/**
	 * (30/10, o Felipe: a Technical e o Stryker "atirando no chão de areia mas o impacto do projétil tem a força de um tanque") A bala
	 * de metralhadora (12,7–14,5 mm; docs/ESTUDO_IMPACTOS.md A e B), ~1/500 da energia do tiro de tanque:
	 *  - BulletHit: na lataria. O acerto do tiro de tanque em miniatura (o StartImpact com ImpactK 0,3: o clarão de ~0,4 m, umas 33
	 *    faíscas a 8–19 m/s, a luz de ~4 m) e, em 1 de cada 3, a baforada de pó de tinta em volume. Devolve o ponto no casco e a normal
	 *    da chapa (pro ricochete do jogo).
	 *  - BulletGroundHit: na areia. O esguicho de terra do tiro na areia a ~1/7 (sem clarão, faíscas, luz nem cratera) e, com bPuff, a
	 *    baforada baixa de areia em volume (~2 m), só com sobra no orçamento da poeira (nunca tira de cena a do tanque).
	 */
	bool BulletHit(const FVector& PointCm, const FVector& FromCm, AActor* Victim, float GroundZCm, uint32 Variant, FVector& OutPointCm, FVector& OutNormal);
	void BulletGroundHit(const FVector& AtCm, const FVector& FromCm, uint32 Variant, bool bPuff);
	/**
	 * (30/10, o Felipe: o tiro de tanque no veículo "tem que ter um efeito de mini explosão") A bola da carga do tiro (o HEAT: ~1,5 kg,
	 * docs/ESTUDO_IMPACTOS.md C: a bola de 2,5–3,5 m que apaga em 0,2–0,3 s e a fumaça que fica 5–8 s) na face do alvo virada pro
	 * atirador: o SpawnWarhead da ogiva v2 (a bola, a fuligem e o anel de areia em volume, na escala da carga), por cima do ArmorHit (o
	 * clarão, as faíscas, as lascas, a luz e a marca, que ficam). No tiro que mata não: a morte traz a dela.
	 */
	void ShellBlast(const FVector& PointCm, const FVector& FromCm, AActor* Victim, float GroundZCm, float Kg, uint32 Variant);
	/** (30/10, a 2ª opinião) A carga da mini explosão no acerto de canhão (kg; 0 = sem): o jogo põe antes do ArmorHit (rts.ShellBlastKg) e a bola
	 *  nasce no SpawnArmorImpact, no quadro das faíscas (antes vinha ~4 quadros depois, pelo ProjectileHit), com o miolo de 1 quadro, sem o
	 *  brilho de 0,3 s, a areia em volta e a baforada cinza (a bola traz o anel e a fuligem: uma representação só). */
	float ShellBallKg = 0.0f;
	/** (30/10, o ricochete da simulação 25: o ShotRicochet) o próximo Tracer ou ArmorHit é de um tiro que ricocheteou: o jogo põe antes da
	 *  chamada e ela zera. O acerto vira o do ricochete (SpawnRicochet: o estalo menor, o leque raso, o traçante que sai torto). */
	bool bNextShotRicochet = false;
	void ShellBall(const FVector& PointCm, const FVector& Normal, AActor* Victim, float GroundZCm, float Kg, uint32 Variant);
	/** (30/10, item 31: "quero danificações, desabamentos também enquanto tá sendo destruído") o pedaço do prédio que cede: o estouro pequeno
	 *  no furo, os pedaços de concreto que caem dele (pra fora e pra baixo, e ficam) e o pó do reboco em volume. Normal: a face que abriu.
	 *  (01/11) Tint: a cor dos pedaços, a da parede do prédio (o cinza claro fixo virava papel branco no sol do deserto). */
	void StructureBreak(const FVector& AtCm, const FVector& Normal, float RadiusCm, uint32 Variant, float GroundZCm,
		const FLinearColor& Tint = FLinearColor(0.20f, 0.19f, 0.17f, 1.0f));
	/**
	 * (30/10, o Felipe: os tanques acertando "unidades de construção") O tiro de tanque no prédio (docs/ESTUDO_IMPACTOS.md C, no prédio):
	 * concreto e reboco não soltam faísca de metal nem queimam. O clarão do tiro e poucas faíscas na face virada pro atirador, os
	 * pedaços de reboco que saltam da fachada e caem, a poeira cinza-bege que desce pela parede e fica no pé dela (5–8 m, 8–15 s) e a
	 * marca na parede. Sem a bola da mini explosão (a carga explode na fachada, mas o que se vê é o pó).
	 */
	void BuildingHit(const FVector& PointCm, const FVector& FromCm, AActor* Building, float GroundZCm, uint32 Variant);
	/**
	 * (24/10, o laboratório: os tipos de chão do jogo, RTSGroundTypes e o Config/RTS/Surfaces.json) Como o tiro no chão reage
	 * ao tipo de chão do ponto. O jogo põe a consulta (GroundResponseOf); o GroundHit pergunta. Sem a consulta, ou com o peso
	 * abaixo de 0,5, ou com Kind 0 (a areia), o tiro na areia aprovado, exato. Kind 1 duro (o asfalto: a poeira cinza curta, as
	 * lascas, mais faísca), 2 terra (a terra escura que sobe sempre, como na cratera de antes, e os torrões; a grama com os
	 * tufos), 3 neve (o pó branco largo que paira e a terra escura por baixo), 4 barro (o respingo escuro, sem poeira).
	 */
	struct FGroundResponse
	{
		float Weight = 0.0f;
		int32 Kind = 0;
		FLinearColor Dust = FLinearColor(-1.0f, 0.0f, 0.0f, 0.0f);    // a cor da poeira em volume (o A, o da gravação); R < 0 = a areia
		float DustK = 1.0f;                                            // o tamanho e a opacidade da poeira × isto (0 = sem poeira)
		FLinearColor Clods = FLinearColor(0.07f, 0.055f, 0.04f);       // os torrões que caem e ficam
		FLinearColor Trail = FLinearColor(1.0f, 0.82f, 0.62f);         // os rastros do jato (× o albedo 0,18 dos rastros)
		FLinearColor Chips = FLinearColor(0.0f, 0.0f, 0.0f);           // os pedaços que voam (as lascas, os tufos, a neve)
		int32 ChipCount = 0;
		float SparkK = 1.0f;                                           // × as faíscas
	};
	TFunction<bool(const FVector& AtCm, FGroundResponse& Out)> GroundResponseOf;
	/**
	 * (25/10, o impacto pelo chão; o Felipe: "o impacto olha o chão em volta") A areia solta do ponto (0–1): quanto do chão ali é a
	 * areia aprovada que as gravações de areia desenham (a strike_chao, a areia da warhead_chao, o anel da ogiva, a cortina, a saia da
	 * bola, a areia do disparo e do acerto no tanque). O jogo põe (URTSGroundTypes: a camada do laboratório e o surface.png do mapa; o
	 * asfalto e o molhado não têm, os outros tipos respondem pelo GroundResponseOf). Sem a consulta, areia em todo lugar (o de hoje).
	 * Na areia inteira (1) tudo fica exato, número por número.
	 */
	TFunction<float(const FVector& AtCm)> SandShareOf;
	/** (30/10) O tiro no prédio na geometria de verdade (o jogo marcha o raio no relevo medido do prédio): o 1º toque do raio
	 *  FromCm→ToCm no Building, a normal, a peça e o ponto no espaço da malha. Sem ele (ou sem toque), a caixa da malha (ArmorPoint). */
	TFunction<bool(AActor* Building, const FVector& FromCm, const FVector& ToCm, FVector& OutCm, FVector& OutNormal, UStaticMeshComponent*& OutPart,
		FVector& OutLocalCm)> StructureRay;

	/**
	 * O tanque ferido (16/10): o jogo passa a vida (0–1) de cada veículo vivo a cada quadro (ARTSUnitActor::SetDamage).
	 * Abaixo de 70% um fio de fumaça clara sobe do deck do motor; abaixo de 35%, uma vez, o jato de chama pelo anel da
	 * torre e a fumaça escura e grossa (os limiares do ZH, UnitDamagedThreshold/UnitReallyDamagedThreshold). Na morte
	 * a fumaça para (a morte aprovada assume).
	 */
	void VehicleDamage(AActor* Unit, float Health01);
	/** (31/10, a revisão: a fumaça do veículo ferido virava uma corda preta) O ponto de onde sobe a fuligem do fogo do motor (abaixo de 35%:
	 *  o topo do fogo em loop no deck, onde a fuligem da gravação se desfaz), no mundo (cm); false sem o fogo (acima de 35%, consertado,
	 *  morto). O jogo solta ali a fuligem em volume (RTSMissileSmoke, o sistema de toda fumaça), no lugar da fita de antes. */
	bool EngineSmokeAt(AActor* Unit, FVector& OutCm) const;
	/**
	 * (30/10, os estados de dano do prédio; docs/ESTUDO_IMPACTOS.md E e o TransitionDamageFX do ZH) O jogo passa a vida (0–1) do prédio
	 * pronto e à vista. Abaixo de 35% ele pega fogo no telhado: o fogo simulado das carcaças (StartFire: volumétrico, na luz do mundo,
	 * a fuligem preta) em cada ponto medido do telhado (RoofCm, no mundo). Consertado acima de 35%, apaga em ~4 s; 0 (a morte) apaga
	 * em 0,5 s. Entre 70% e 35% ficam as placas de queimado do material (as rachaduras do ZH).
	 */
	void StructureDamage(const AActor* Building, float Health01, const TArray<FVector>& RoofCm);
	/** (22/10) A carcaça vai embora (começou a afundar, ou a simulação a tirou): o fogo dela, se ainda queima, se apaga em
	 *  FadeS (a luz, a fumaça e as chamas); 0 = já. A que nem acendeu não acende mais. */
	void WreckGone(AActor* Unit, float FadeS);
	/** (31/10, a sucata) O Dozer desmanchando a carcaça, a cada quadro, com o quanto o casco já desceu (cm): na 1ª vez o fogo dela sai pelo
	 *  fim simulado (a chama baixa e a fumaça já solta sobe e se desfaz, como na carcaça que apaga sozinha); ele desce com o casco (a areia
	 *  abafa a chama) e a carcaça que some antes do fim dele não o corta (UpdateDamaged). */
	void WreckSalvage(AActor* Unit, float SinkCm);

	/**
	 * A Patriot (16/10; o estudo: docs/ESTUDO_MISSIL_PATRIOT.md). O jogo chama:
	 *  - MissileLaunch no disparo (Id = o do tiro; Pos e Dir na boca da caixa);
	 *  - MissileMove a cada quadro com o míssil interpolado (a chama, a luz e o rastro andam junto);
	 *  - MissileHit no contato (a explosão da ogiva; bKill = o tiro que matou);
	 *  - DefenseAssist quando uma bateria ajuda outra (o laser de mira vermelho entre elas, o LaserFromAssisted do ZH).
	 */
	void MissileLaunch(int32 Id, const FVector& PosCm, const FVector& Dir, uint32 Variant, float RailTailM = 0.0f);   // (01/11) RailTailM < 0: o lançamento suave (sem clarão nem poeira; o motor apagado até o MissileIgnite)
	/** (01/11, o lançamento suave) O motor de voo do míssil Id: apagado, só o corpo anda (sem a chama, a luz e o rastro). */
	void MissileMotor(int32 Id, bool bOn);
	/** (01/11, o lançamento suave) O motor de voo acende no ar: o motor ligado e o clarão pequeno na tubeira (cm), pra Dir. */
	void MissileIgnite(int32 Id, const FVector& NozzleCm, const FVector& Dir);   // (01/11) RailTailM > 0: do trilho da aeronave (o comprimento do míssil)
	/** (v4, 23/10) O míssil que já nasce voando (o Cruise Missile Strike, sem atirador) ou que entra na vista vindo da névoa:
	 *  a chama, a luz e o rastro do MissileLaunch, sem o clarão da saída e sem a poeira do lançamento. Não faz nada se o míssil
	 *  Id já tem a chama (pode ser chamado a cada quadro). */
	void MissileInFlight(int32 Id, const FVector& PosCm, const FVector& Dir, uint32 Variant);
	void MissileMove(int32 Id, const FVector& PosCm, const FVector& Dir, float SpeedCmPerS);
	/** (30/10, item 37) O corpo do míssil Id (a malha com a ponta na origem; LengthCm até a tubeira). Pode ser chamado a cada quadro. */
	void MissileBody(int32 Id, UStaticMesh* Mesh, float LengthCm);
	/** (23/10, a ogiva v2) FromCm: de onde o míssil vinha (o passo de antes, no ar): a ogiva explode do lado de fora da blindagem,
	 *  virada pra ele. WarheadKg: a carga da ogiva em kg de TNT (Units.json "warhead_kg", só desenho): a bola de fogo, a fuligem e
	 *  o anel de areia em volume, pela escala de Hopkinson-Cranz a partir da Carga Pesada (o anel, a partir do tiro de canhão na
	 *  areia); 0 = sem volume (só o clarão, as faíscas, a luz e a marca, sem o brilho em sprite: a metade pesada do ataque de mísseis,
	 *  que ganha a clássica logo depois). -MissileBlastV=1 volta a de antes, exata.
	 *  (23/10, a revisão) bClassicLook: a fuligem, o sol, o céu e o chão da clássica (Units.json "warhead_look": "classic"; a metade
	 *  leve do ataque de mísseis, ao lado da pesada clássica), em vez dos da morte de tanque e dos tiros. Devolve o centro da bola de
	 *  fogo que se vê (cm; o pé fora da blindagem, virado pro míssil, na altura do miolo quente), pro empurrão e o calor no rastro do
	 *  míssil; sem a ogiva v2, o PointCm. */
	FVector MissileHit(int32 Id, const FVector& PointCm, const FVector& FromCm, AActor* Victim, float GroundZCm, bool bKill, float WarheadKg,
		bool bClassicLook = false, const BlastFxHitLife::FHitIn* LifeIn = nullptr, BlastFxHitLife::FHitOut* LifeOut = nullptr);
	/** (30/10, item 38: "quando chega a explosão nem aparece") A ogiva que estoura no ar (o míssil antiaéreo na aeronave): a mesma bola da
	 *  ogiva do chão (a Carga Pesada na escala de Hopkinson-Cranz da carga Kg), com o pé meia bola abaixo do ponto, sem o anel de areia e
	 *  com a saia baixa da gravação no gás da carga (a cor da fuligem); os estilhaços e o clarão na mesma escala. */
	void AirBurst(const FVector& AtCm, const FVector& FromCm, AActor* Victim, float Kg, uint32 Variant);
	/**
	 * (23/10, a explosão com vida; BlastFxHitLife.h) Com LifeIn e -HitLifeV=2 (o padrão) o MissileHit é o com vida: a direção ℓ (a bola nasce
	 * no ponto do casco ou do chão e sai torta pela física: a normal da face, o que o míssil traz ao longo dela, o mergulho guardado), a
	 * nuvem que continua viva depois da gravação, os brilhos no casco, as faíscas em jatos, os pedaços em brasa, a terra do míssil no chão
	 * na escala da carga, as gravações dedicadas quando existem. Cada camada atrás da sua variável blast.HitLife.<camada>. LifeOut (bValid)
	 * leva pro jogo o ponto, a normal, d, ℓ, o pé, a bola e os relógios (o pulso). Sem LifeIn, ou com -HitLifeV=1, o de hoje, exato.
	 */
	/** (23/10, a vida) Os brilhos dos estilhaços (1–2 quadros) nas faces das peças de Target viradas pra FromCm, a até RadiusCm de NearCm. */
	void HullGlints(AActor* Target, const FVector& FromCm, const FVector& NearCm, float RadiusCm, int32 Count, uint32 Seed);
	/** (23/10, a vida; a camada Neighbors) O vizinho do dano em área: a marca do míssil (tipo 1) na face virada pra explosão. */
	void NeighborMark(AActor* Neighbor, const FVector& BlastCm, uint32 Seed);
	/** (23/10, a vida) A fila de pousos (os pedaços em brasa e os torrões que levantam areia): os destroços põem, o jogo esvazia a cada quadro. */
	void PushLanding(const BlastFxHitLife::FLanding& L);
	void ConsumeLandings(TArray<BlastFxHitLife::FLanding>& Out);
	/** (a revisão da vida) Os pedaços em brasa queimando em voo: os destroços põem o ponto a cada quadro, o jogo esvazia (o fio de fumaça em
	 *  volume no RTSMissileSmoke, no lugar da fita chapada). */
	void PushEmberSmoke(uint32 Id, const FVector& PosCm);
	void ConsumeEmberSmoke(TArray<BlastFxHitLife::FEmberSmoke>& Out);
	/** (23/10) A escala da ogiva de Kg kg: λ = 0,55·(W/20 kg)^⅓. A âncora: a clássica do jogo (a Carga Pesada a 0,55, a TankBallScale)
	 *  é a carga de 20 kg (o nominal, instances.js MAGNITUDES; e a bola dela, 2,75 m, dá 20,8 kg pela LANL, ~1 m por kg^⅓). */
	static float WarheadScale(float Kg) { return 0.55f * FMath::Pow(FMath::Max(Kg, 0.01f) / 20.0f, 1.0f / 3.0f); }
	/** O raio da bola de fogo (m): os 5,0 m medidos na gravação da Carga Pesada (T > 0,5 de 0,28 a 0,9 s) × λ. */
	static float WarheadFireballM(float Kg) { return 5.0f * WarheadScale(Kg); }
	/** -MissileBlastV= (2, o padrão: a ogiva v2; 1: a de antes, exata, no BlastFx e no jogo). */
	static int32 MissileBlastVer();
	void DefenseAssist(const FVector& FromCm, const FVector& ToCm);

	/**
	 * O tanque esmagando o carro civil (16/10; o estudo: docs/ESTUDO_ESMAGAR_E_POSTE.md). CarCrushStart quando a lagarta
	 * encosta (o 1º PropCrushed): o efeito segue a fração T da passagem (PassSeconds = (comprimento do carro + 4,5 m) ÷
	 * velocidade), nos pontos da caixa do carro: lanternas, vidros que estouram e piscam ao sol, pneus, poeira, o raspão do
	 * casco. bAlarm: o pisca-alerta âmbar (o alarme do ZH em 50% dos carros). CarCrushEnd: o tanque parou ou morreu em cima.
	 */
	void CarCrushStart(AActor* Car, const FVector& TankDirCm, float PassSeconds, uint32 Variant, bool bAlarm = false);
	void CarCrushEnd(AActor* Car);

	/**
	 * O arco do fio de poste (16/10; o estudo: docs/ESTUDO_ESMAGAR_E_POSTE.md, parte B), no lugar do WireArc provisório do
	 * jogo. Kind 0: o fio arrebentou no ar (os fios se tocam: o clarão rápido, as gotas, as lascas do isolador); 1: a ponta
	 * viva encostou no chão (o clarão rápido rente à areia); 2: a religada (o arco de 0,6–1,0 s pulando pela areia, as gotas
	 * e a fumacinha). O religador do estudo (padrão do OpenDSS): o 1º a +0,5 s, depois +2 s e +2 s.
	 */
	void WireArc(const FVector& PointCm, int32 Kind, uint32 Variant = 0);
	/** O vento do mapa (17/10; o jogo chama no começo da partida, RTSGameData WindYawDeg/WindSpeedMs): pra onde ele sopra
	 *  (graus, 0 = +X). A coluna da carcaça deita pra esse lado. */
	void SetWind(float YawDeg, float SpeedMs);
	/** (25/10, o vento ao vivo) O vento do mapa a 10 m (m/s, no plano: pra onde sopra × a velocidade), o de SetWind, lido a cada quadro
	 *  por quem deriva nele (a nuvem da ogiva, a poeira que fica, a fumaça dos destroços, a coluna da carcaça). */
	FVector2D WindMs() const { return FVector2D(FMath::Cos(WindYawRad), FMath::Sin(WindYawRad)) * WindSpeedMs; }
	/** (25/10) O vento a Z m do chão sobre o de 10 m: o perfil logarítmico da camada neutra, ln(z/z0)/ln(10/z0) com z0 = 1 cm (a areia com
	 *  moita rala, Davenport/WMO), o mesmo do rastro do míssil (RTSMissileSmoke). */
	static float WindProfileK(float ZM);
	/** O jogo desenha a fumaça do rastro do míssil (RTSMissileSmoke): o BlastFx fica só com a chama, a luz, o lançamento e o acerto. */
	static void SetExternalMissileSmoke(bool bOn) { bExternalMissileSmoke = bOn; }
	static bool HasExternalMissileSmoke() { return bExternalMissileSmoke; }

	/** Desenha cada gravação invisível na frente da câmera por uns quadros: o shader compila antes da 1ª morte. */
	void Prewarm(const FVector& At);
	bool IsWarm() const { return bWarm; }

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	bool LoadAssets();
	/** bLit false: nasce sem a luz e sem o clarão (os volumes da ogiva v2: a luz dela é a dos destroços). As areias aprovadas do
	 *  tiro (SpawnDust) seguem nascendo como antes (true: o ator apaga a luz e o clarão logo depois, no SpawnDust). */
	AExplosionVolume* Spawn(const FVector& Ground, UExplosionBake* Bake, float SizeScale, float Seed, bool bLit = true);
	void UpdateView();

	// a areia do tiro: a poeira gravada (jg_dust_impact) no raymarch, com orçamento (as mortes de tanque vêm primeiro:
	// o raymarch desenha até 16 por quadro). Prioridade 0 = o sopro do disparo, 1 = o acerto e o tiro no chão
	struct FDust
	{
		TWeakObjectPtr<AExplosionVolume> E;
		float Duration = 3.0f, Opacity0 = 1.0f;
		int32 Prio = 0;
		// (17/10 tarde) a vida em segundos de verdade (0 = a da gravação): some pela opacidade de FadeFromS a LifeS
		float LifeS = 0.0f, FadeFromS = 0.0f, FadeInS = 0.25f;   // (23/10 noite) FadeInS: a entrada (s da gravação; a strike_chao nasce no impacto)
		// (25/10, a poeira que fica; o Felipe: "duas coisas distintas mal encaixadas, sem vida") a areia ou o pó que sobe e cai vira a nuvem
		// que fica, no mesmo volume: a gravação para no quadro HoldBakeS (o ruído segue andando) e a nuvem abre, rala, assenta e vai com o
		// vento (UpdateDusts, MakeLinger). Medidas na parada: R0M (o raio com 90% da poeira), ZcM (a altura do meio), VrMs (a frente ainda
		// abrindo); os relógios em s de idade; SigY/SigZ/SigXs: o espalhamento turbulento integrado (m); Dep: a fração que não assentou
		bool bLinger = false;
		float HoldBakeS = 0.0f, HoldAge = 0.0f, LingerLifeS = 0.0f, Scale0 = 1.0f, Thick0 = 1.0f, R0M = 10.0f, ZcM = 1.5f, VrMs = 0.0f;
		float SettleMs = 0.05f, LastAge = 0.0f, RetireAt = -1.0f, SigY = 0.0f, SigZ = 0.0f, SigXs = 0.0f, Dep = 1.0f;
		FVector FootBase = FVector::ZeroVector, DriftCm = FVector::ZeroVector;
		FVector2D DriftVelMs = FVector2D::ZeroVector;
		// (25/10, o custo) as nuvens do mesmo ataque viram uma: na parada, a que cai em cima de outra (a mesma gravação, a menos de 0,8·R0) passa
		// a massa pra ela (esmaece em 1,5 s enquanto a outra engrossa e alarga pelo espalhamento dos centros). MassK = a massa em nuvens; UnionK =
		// o alargamento (√(1 + Var/(M·R0²))); VarM2 = Σ m·d² dos centros (m²); os de 0 e 1: o começo e o fim da emenda (MergeAt, s de idade)
		bool bHeld = false;
		float MassK = 1.0f, MassK0 = 1.0f, MassK1 = 1.0f, UnionK = 1.0f, UnionK0 = 1.0f, UnionK1 = 1.0f, VarM2 = 0.0f, MergeAt = -1.0f;
		FVector Foot0 = FVector::ZeroVector, Foot1 = FVector::ZeroVector;
	};
	/** (25/10, a poeira que fica) A areia que nasceu agora (o último de Dusts, da gravação Source) vira nuvem que fica: para em HoldFrac da
	 *  gravação; SettleMs = a queda dos finos (m/s). Falso (e nada muda) com a camada Linger desligada, -DustLingerV=1 ou sem a medida. */
	bool MakeLinger(FName Source, float HoldFrac, float SettleMs);
	/** As nuvens que ficam vivas (o teto por preset). */
	int32 NumLingering() const;
	TArray<FDust> Dusts;
	AExplosionVolume* SpawnDust(const FVector& GroundCm, float Scale, float TimeScale, float Opacity, int32 Prio, uint32 Variant,
		const FVector4f& Color = FVector4f(-1.0f, 0.0f, 0.0f, 0.0f), const TCHAR* Source = nullptr, float Thickness = 1.0f,
		const FVector4f& Soot = FVector4f(-1.0f, 0.0f, 0.0f, 0.0f));
	// o último acerto de cada tanque (no máximo um efeito por vítima a cada 100 ms, como o ZH)
	TMap<TWeakObjectPtr<AActor>, double> LastHitAt;
	uint32 ShotSerial = 0;                          // misturado na variante: cada tiro com o seu desenho
	// o ponto do acerto escolhido no disparo (o traçante voa até ele) e os acertos esperando o traçante chegar
	struct FShotPoint
	{
		FVector Point = FVector::ZeroVector, Normal = FVector::ZeroVector, ShellDir = FVector::ZeroVector;
		double Arrive = 0.0, Born = 0.0;
		uint32 Variant = 0;
		float GroundZCm = 0.0f;
		bool bSpawned = false;                      // as faíscas já nasceram na chegada do traçante
		bool bRicochet = false;                     // (30/10) o tiro ricocheteou (a simulação 25 decide no disparo)
		TWeakObjectPtr<UStaticMeshComponent> Part;  // a peça que levou o tiro e o ponto no espaço da malha (a marca)
		FVector LocalCm = FVector::ZeroVector;
	};
	TMap<TWeakObjectPtr<AActor>, FShotPoint> PendingShots;
	/** (01/11, a avaliação do item 10: "TODOS SEMPRE ATIRAM NO CENTRO, cada um escolher um lugar, e a cada X tiros aleatórios eles mudarem
	 *  pra uma outra posição aleatória, e daí sim onde o impacto cai fica machucado a construção") a mira de cada atirador em cada prédio
	 *  (o lado e a altura na fachada virada pra ele; muda a cada 3–6 tiros) e os tiros a caminho de cada prédio, na ordem: o traçante
	 *  termina no ponto e o acerto (BuildingHit) usa o mesmo ponto. */
	struct FBuildingAim { float Side = 0.0f; float Height = 0.0f; int32 Left = 0; };
	TMap<uint64, FBuildingAim> BuildingAims;
	TMap<TWeakObjectPtr<AActor>, TArray<FShotPoint>> BuildingShots;
	bool BuildingAimPoint(AActor* Building, const FVector& FromCm, float GroundZCm, uint32 Shooter, FShotPoint& OutShot);
	struct FDelayedHit
	{
		FShotPoint Shot;
		TWeakObjectPtr<AActor> Target;
		float GroundZCm = 0.0f;
		bool bKill = false;
	};
	TArray<FDelayedHit> DelayedHits;
	// a Patriot: os mísseis em voo (o ator da chama e do rastro, e quando foi movido pela última vez), a última poeira de
	// lançamento e a última bola de ogiva por alvo
	struct FMissileFx
	{
		TWeakObjectPtr<class ABlastDebris> Fx;
		double Seen = 0.0;
	};
	TMap<int32, FMissileFx> Missiles;
	double LastLaunchDust = -10.0;
	TMap<TWeakObjectPtr<AActor>, double> LastWarheadBall;   // (a de antes, -MissileBlastV=1)
	/**
	 * (23/10) A ogiva v2: cada volume vivo de uma ogiva (o corpo: a Carga Pesada a λ; o anel: a sim_chao_anel), a vítima e o pé
	 * (a fusão dos acertos no mesmo alvo), os relógios em s de idade e a aposentadoria (o esmaecer curto quando falta vaga no
	 * raymarch ou uma morte nasce em cima). Fora da lista das areias: o teto, a prioridade e a vida são daqui.
	 */
	struct FWarhead
	{
		TWeakObjectPtr<AExplosionVolume> E;
		TWeakObjectPtr<AActor> Victim;
		FVector FootCm = FVector::ZeroVector;
		bool bRing = false;
		float RadiusCm = 0.0f;                      // a bola (o corpo) ou o R90 do anel
		float BallCm = 0.0f;                        // a bola da ogiva (nas duas: as mortes por cima tiram as duas)
		float FireEndS = 0.0f;                      // o fim da fase A (a bola acesa); o anel: a janela da fusão dos anéis
		float FadeFromS = 0.0f, LifeS = 0.0f;       // o corpo: o esmaecer natural (s de idade)
		float ThinFromS = 0.0f, SootKeep = 1.0f;    // o corpo: a fuligem afina de ThinFromS até FireEndS pra SootKeep (a leitura do alvo)
		float Duration = 0.0f, Opacity0 = 1.0f;     // o anel: até onde ele toca (s da gravação: o fim da parte que se vê) e a opacidade
		float RetireAt = -1.0f;                     // a idade em que começou a sair (< 0: não está saindo)
		// (23/10, a vida; BlastFxHitLife.h) o corpo com vida: a bola anda ao longo de ℓ (FootBase + MoveCm·(1 − e^(−t/MoveTauS))), a nuvem
		// segura o último quadro válido a partir de HoldAgeS e sobe e alarga como térmica (o tamanho × √(t/HoldAgeS), a espessura ÷ o cubo)
		// até CloudEndS, indo com o vento depois da bola acesa (DriftCm)
		bool bMove = false, bCloud = false;
		FVector FootBase = FVector::ZeroVector, MoveCm = FVector::ZeroVector, DriftCm = FVector::ZeroVector;
		float MoveTauS = 0.1f, HoldAgeS = 0.0f, Scale0 = 1.0f, Thick0 = 1.0f, LastAge = 0.0f;
		// (a revisão da vida) o λ da carga (a escala de Hopkinson-Cranz na Carga Pesada): a altura do meio da nuvem pro vento (12·λ m) é a da
		// carga, não a do tamanho em que a gravação toca (a dedicada toca a (W/W0)^⅓, outra régua)
		float CloudLambda = 1.0f;
	};
	TArray<FWarhead> Warheads;
	/** (23/10, a vida) O que o MissileHit com vida passa pro SpawnWarhead (nulo = a ogiva v2 de hoje, exata). */
	struct FWarheadLife
	{
		bool bCloud = false;                        // a nuvem viva (a camada Cloud; o preset diz quanto)
		float CloudExtraS = 0.0f;                   // quanto ela dura depois da gravação (s; 6·G no Alta/Ultra, 3·G na Média; 31/10)
		FVector MoveCm = FVector::ZeroVector;       // a bola anda isto ao longo de ℓ (no chão)
		float MoveTauS = 0.1f;                      // τ = 0,25 × o fim da bola acesa
		bool bYaw = false;                          // o giro pelo rumo de ℓ (senão o sorteio de hoje)
		float YawRad = 0.0f;
		bool bRingFoot = false;                     // o anel na projeção do ponto, ~0,3 R pra fora
		FVector RingFootCm = FVector::ZeroVector;
		FName Source = NAME_None;                   // a gravação dedicada (warhead_lado, _teto, _chao), quando existe (a strike_chao, a areia do ataque, toca à parte)
		float SourceScale = 1.0f;                   // o tamanho dela pra esta carga ((W/W da gravação)^⅓)
		bool bGasSkirt = false;                     // o pé no teto do casco (sem a warhead_teto): a saia baixa é o gás da carga, na cor da fuligem
		bool bNoRing = false;                       // (a revisão) sem o anel de areia: o jogo desenha o véu, ou a strike_chao tem a areia (uma representação só)
		float OutCloudEndS = 0.0f;                  // (de volta) o fim da nuvem viva do corpo que nasceu (s de idade; 0 = sem ela)
		bool bOutOwn = false;                       // (de volta, a revisão) nasceu a gravação dedicada (ela tem a areia que o choque levanta)
		float OutOwnFadeS = 0.0f;                   // (de volta) o esmaecer dela (s de idade): a continuação do jogo nasce aí
	};
	/** (23/10, a vida) O MissileHit com vida (o pulso; BlastFxHitLife.h). */
	FVector MissileHitLife(uint32 Variant, const FVector& PointCm, const FVector& FromCm, AActor* Victim, float GroundZCm, bool bKill, float WarheadKg,
		bool bClassicLook, const BlastFxHitLife::FHitIn& In, BlastFxHitLife::FHitOut* Out);
	/** (23/10, a vida) O 1º toque do raio From → To nas caixas das peças do alvo (a mesma conta do ArmorPoint, com o raio de verdade em 3D). */
	bool HullRay(AActor* Target, const FVector& FromCm, const FVector& ToCm, FShotPoint& Out) const;
	/** (a revisão da vida) A caixa do alvo no espaço dele, sem escala: a soma das caixas das peças que o HullRay enxerga. */
	bool HullBox(AActor* Target, FBox& Out) const;
	/** (23/10, a vida) A fila de pousos (FLanding), num tipo daqui (o cabeçalho de fora só é declarado). */
	struct FLandingRec
	{
		FVector PosCm = FVector::ZeroVector;
		float SizeM = 0.3f;
		uint8 Kind = 0;
		bool bHot = false;
		FVector VelMs = FVector::ZeroVector;        // (a areia do acerto) a velocidade no 1º toque (m/s)
	};
	TArray<FLandingRec> Landings;
	/** (23/10, a vida) Até quando cada pedaço em brasa vive (o teto do preset: 32 Ultra, 16 Alta, 8 Média, 4 Baixa). */
	TArray<double> EmberUntil;
	/** (a revisão da vida) Os pedaços que soltam o fio de fumaça em voo: até quando cada um queima e o fio dele sai (o teto do preset: 8 Ultra,
	 *  4 Alta, 2 Média, nenhum na Baixa; 1 por acerto, 2 a partir de 12 kg) e os pontos deste quadro (o Id e onde está), que o jogo esvazia. */
	TArray<double> EmberSmokeUntil;
	struct FEmberSmokeRec
	{
		uint32 Id = 0;
		FVector PosCm = FVector::ZeroVector;
	};
	TArray<FEmberSmokeRec> EmberSmoke;
	TArray<TPair<FVector2D, double>> RecentBigBlasts;   // as mortes e as clássicas do último 1 s (o anel não nasce em cima delas)
	void UpdateWarheads();
	/** Devolve se a bola de fogo deste acerto é de uma ogiva: o corpo que nasceu agora, ou o aceso de antes em que ele se fundiu (a luz
	 *  dos destroços segue a curva dela). false: a bola é da morte ou da clássica recente em cima (elas acendem a própria luz), ou não
	 *  havia vaga no raymarch (os destroços ficam com a queda do clarão). Os destroços da ogiva v2 nunca têm o brilho em sprite. */
	bool SpawnWarhead(const FVector& FootCm, AActor* Victim, float Kg, uint32 Variant, bool bClassicLook, FWarheadLife* LifeOpt = nullptr);
	/** Quantos volumes o raymarch desenha agora na tela (o mesmo filtro e o mesmo cone dele; mais os anéis de ogiva que nasceram com
	 *  opacidade 0 e entram no próximo quadro). */
	int32 RaymarchOnScreen() const;
	/** Tira Count ogivas: já (bNow; só as que contam no RaymarchOnScreen, que liberam vaga) ou esmaecendo. Primeiro o anel que nasceu
	 *  com opacidade 0 e ainda não foi desenhado (sai já nos dois modos, sem estalo), depois os anéis, depois os corpos na fase B (a
	 *  fumaça), a mais velha primeiro; bPhaseBOnly deixa a bola acesa em paz. Devolve quantas saíram. */
	int32 RetireWarheads(int32 Count, bool bNow, bool bPhaseBOnly = false);
	void RetireWarheadsNear(const FVector& AtCm, float RadiusCm);
	/** Antes de nascer um volume que vem primeiro (a morte, a areia do tiro) com o pé em FootCm na escala Scale: as ogivas cedem. */
	void MakeRoomForPriority(const FVector& FootCm, float Scale);
	TArray<double> RecentDeaths;                    // quando nasceram as últimas mortes (o orçamento da morte em massa)
	// os carros sendo esmagados: o eixo e o tamanho do carro, a direção do tanque, quando começou, a passagem e o que já
	// aconteceu (um bit por etapa)
	struct FCrush
	{
		TWeakObjectPtr<AActor> Car;
		FVector Center = FVector::ZeroVector, Fwd = FVector::ForwardVector, Right = FVector::RightVector, TankDir = FVector::ForwardVector;
		float HalfLen = 220.0f, HalfWid = 90.0f, Height = 150.0f, GroundZ = 0.0f;
		double T0 = 0.0;
		float Pass = 2.0f;
		uint32 Variant = 0;
		int32 Done = 0;
		bool bEnded = false, bBurnt = false;        // bBurnt: a carcaça queimada (sem vidro, lanterna nem pneu)
	};
	TArray<FCrush> Crushes;
	void UpdateCrushes();
	// o teste do carro sem o jogo (-CrushFxTest): o proxy que o jogo cria ao esmagar (a malha civ_pickup/civ_car_burnt) e
	// os tanques (a velocidade, pra passagem)
	TSet<TWeakObjectPtr<AActor>> CrushTestSeen;
	TMap<TWeakObjectPtr<AActor>, FVector> CrushTestTankPos;
	TMap<TWeakObjectPtr<AActor>, float> CrushTestTankSpeed;
	void UpdateCrushTest();
	void UpdateArcTest();
	void GatherSceneLights();                       // as luzes pontuais acesas → RTSSceneLights (as fumaças leem)
	void UpdateShadowTest();                        // -ShadowFxTest=1: tiros na areia dos 4 lados da casa 22 (a prova da sombra do mapa)
	int32 ShadowTestStep = 0;
	/**
	 * A marca do tiro no casco (16/10; o estudo: docs/ESTUDO_MARCA_QUENTE.md): grava o acerto nos dados de primitiva
	 * 12–35 da peça (6 vagas: x, y, z no espaço da malha em cm; a hora + 100 000 × o tipo, 0 canhão, 1 míssil). Um acerto
	 * a menos de 25 cm de uma marca do mesmo tipo reforça ela (volta a esquentar); cheia, sai a mais velha. Quem desenha é
	 * a MF_HullMarks (Game/Tools/hull_marks.py) no material das unidades. -HullMarks=0 desliga.
	 */
	void MarkHull(UStaticMeshComponent* Part, const FVector& LocalCm, int32 Kind);                           // -ArcFxTestX/Y: a sequência do poste num ponto, sem o jogo
	int32 ArcTestStep = 0;
	static inline bool bExternalMissileSmoke = false;
	bool ArmorPoint(AActor* Target, const FVector& FromCm, const FVector& AtCm, float GroundZCm, uint32 Variant, FShotPoint& Out) const;
	void SpawnArmorImpact(const FShotPoint& Shot, AActor* Target, float GroundZCm, bool bKill);
	void SpawnRicochet(const FShotPoint& Shot, AActor* Target, float GroundZCm);
	/** (24/10, o laboratório) O tiro no chão de um tipo que não é a areia (FGroundResponse). */
	/** (25/10, o impacto pelo chão) O míssil no chão que não é areia: as mesmas peças na escala da carga (SizeK = o tamanho da cortina
	 *  da carga ÷ o do tiro; DirtK, SpeedK e Puffs: os torrões do míssil), sem o clarão, as faíscas e a luz do tiro (são do acerto do
	 *  míssil). Nulo = o tiro de tanque de hoje, exato. */
	struct FTypedScale
	{
		float SizeK = 1.0f, DirtK = 1.0f, SpeedK = 1.0f, ElevDeg = 0.0f;
		int32 Puffs = 0;
	};
	/** (25/10) bOutLinger: o pó do tipo (o anel) do míssil ficou como nuvem (MakeLinger): ele é o perto e a poeira que fica. */
	void GroundHitTyped(const FVector& AtCm, const FVector& FromCm, uint32 Variant, bool bCrater, const FGroundResponse& R, const FTypedScale* Scale = nullptr,
		bool* bOutLinger = nullptr);
	/** (25/10, o impacto pelo chão) O tipo que manda no ponto, quando não é a areia (o GroundResponseOf com peso ≥ 0,5 e Kind ≠ 0). */
	bool TypedGround(const FVector& AtCm, FGroundResponse& Out) const;
	/** (25/10) A areia solta (SandShareOf) no miolo (até CoreCm: a cratera) e na área (até AreaCm), a média de amostras em anéis. */
	void SandAround(const FVector& AtCm, float CoreCm, float AreaCm, float& OutCore, float& OutArea) const;
	/** (25/10) Uma gravação de areia nascendo no chão no ponto: fora da areia, a opacidade × o que o chão tem (o tipo: a cor da poeira
	 *  dele e o dust_k; o resto: a areia solta) e a cor do tipo. false = o chão ali não solta nada (não nasce). Na areia, nada muda. */
	bool GroundDustAt(const FVector& AtCm, float& InOutOpacity, FVector4f& InOutColor) const;
	/** (25/10) A saia baixa de uma bola no chão que não é areia (Out: a cor da poeira do tipo, ou bGas = o jato de parede do gás da carga,
	 *  na fuligem, como no teto do casco). false = areia (a saia de hoje). */
	bool GroundSkirtAt(const FVector& AtCm, FVector4f& Out, bool& bGas) const;
	// os tanques feridos: a fumaça (um ator de destroços com 1–2 rastros que sobem) e o estágio (1 danificado, 2 muito)
	struct FDamaged
	{
		TWeakObjectPtr<class ABlastDebris> Fx;
		int32 Light = -1, Heavy = -1, Stage = 0;
		/** (30/10, item 32: "quero fogo assim e danificação assim nas unidades") o fogo do motor do veículo muito danificado: o fogo em loop
		 *  (volumétrico, com a luz), preso no deck e andando com ele até consertar ou morrer */
		TWeakObjectPtr<class AExplosionVolume> DeckFire;
		int32 Dbg[3] = { -1, -1, -1 };
		// a carcaça: a fumaça escura e o fogo pequeno começam quando a explosão da morte já se desfez e duram ~30 s
		double WreckStart = -1.0, WreckUntil = -1.0;
		double WreckLightStart = -1.0;   // (17/10) a luz do fogo de baixo acende logo depois da bola, antes da fumaça e das chamas
		bool bWreckLightOn = false;
		// (17/10) a coluna em volume da carcaça (sim_carcaca) e se já acendeu
		TWeakObjectPtr<class AExplosionVolume> Column;
		bool bWreckLit = false;
		int32 Fire = -1;                 // (25/10) o fogo simulado (StartFire), com blast.Fire.Version 2
		float FireScale = 1.0f;          // (30/10) o tamanho do fogo da carcaça (o SizeK da morte: a Technical queima menor que o tanque)
		/** (31/10, a sucata) O fogo já sai pelo fim simulado (WreckSalvage): segue até o fim mesmo sem a carcaça; o quanto o casco desceu
		 *  (cm, o fogo desce junto) e quando o fogo acaba (s de mundo: a luz vive até lá). */
		bool bFireLetEnd = false;
		float SinkCm = 0.0f;
		double FireEndAt = -1.0;
		/** (01/11) o fogo do motor andando: onde o deck estava no quadro anterior e a diluição da fuligem dele (1 parado, ~0,2 andando) */
		FVector DeckLastCm = FVector::ZeroVector;
		double DeckLastS = -1.0;
		float DeckDilute = 1.0f;
	};
	void StartWreck(AActor* Unit);
	TArray<FVector2D> GroundCraters;   // (17/10) onde já caiu tiro na areia (cm): ali a terra escura sobe
	float WindYawRad = -0.785f;   // o padrão: o vento da fumaça dos destroços (BlastDebris SmokeWind, (0,55; −0,55))
	float WindSpeedMs = 4.0f;
	TMap<TWeakObjectPtr<AActor>, FDamaged> Damaged;
	/** (30/10) Os fogos do telhado de cada prédio abaixo de 35% (os ids do StartFire). */
	TMap<TWeakObjectPtr<const AActor>, TArray<int32>> StructFires;
	/** (30/10) Os prédios que já passaram dos 70% (a baforada da passagem sai uma vez; consertado acima, sai da lista). */
	TSet<TWeakObjectPtr<const AActor>> StructDamaged;
	void UpdateDamaged();
	void EndDamaged(AActor* Unit);
	void UpdateDusts();
	/** (25/10, o fogo simulado) Cada fogo vivo: o volume, a luz, o chão, a escala, a idade (s), o rumo (rad, segue o vento), o ponto de
	 *  saída do laço (quadros desde o começo do laço; < 0 = ainda não marcado) e o esmaecer (a idade em que começou e quanto dura). */
	struct FFire
	{
		int32 Id = 0;
		TWeakObjectPtr<AExplosionVolume> E;
		TWeakObjectPtr<class ABlastDebris> Light;
		bool bOwnLight = false;
		FVector GroundCm = FVector::ZeroVector;
		float Scale = 1.0f, Age = 0.0f, YawRad = 0.0f, ExitG = -1.0f, FadeFrom = -1.0f, FadeS = 1.0f, BurnS = 30.0f;
		/** (31/10) a fumaça que sobra depois do último quadro: desde quando ela segue sozinha (s de Age), a idade dela nesse ponto e o
		 *  quanto o vento já a levou */
		float HoldAt = -1.0f, HoldT1 = 3.0f;
		FVector DriftCm = FVector::ZeroVector;
	};
	TArray<FFire> Fires;
	int32 FireSerial = 0;
	void UpdateFires(float Dt);
	/** (25/10) A luz de cada fogo em loop (FireLoop): a luz da carcaça (ABlastDebris, a mesma do fogo de sprite e do simulado) pela
	 *  potência gravada da eg_fogo_ar_64, no centro da queima, andando com o volume; apaga quando quem chama destrói o volume. */
	struct FLoopFire
	{
		TWeakObjectPtr<AExplosionVolume> E;
		TWeakObjectPtr<class ABlastDebris> Light;
		float Scale = 0.17f;
	};
	TArray<FLoopFire> LoopFires;
	float LoopFireMeanPower = -1.0f;   // a média da potência gravada no ciclo (a luz média = a da carcaça)
	void UpdateLoopFires();
	/** (30/10, o Felipe: "onde pega tiro de tanque no chão, por conta do calor, daria pra deixar uns segundinhos um foguinho fraco") o
	 *  resto da carga e os pedaços quentes queimando no buraco: o fogo em loop (FireLoop, volumétrico, com a luz da carcaça) bem pequeno,
	 *  que acende com o tiro e morre em poucos segundos. Teto de MaxGroundFires vivos (o mais velho apaga antes). */
	struct FGroundFire
	{
		int32 Fire = -1;                   // (v11) o fogo simulado (StartFire): a ignição da gravação da carcaça, pequena
		float Age = 0.0f, LifeS = 3.0f;
	};
	TArray<FGroundFire> GroundFires;
	void StartGroundFire(const FVector& AtCm, const FVector& FromCm, uint32 Variant, float Scale, float LifeS);
	void UpdateGroundFires(float Dt);
	/** (25/10) A areia que fica (FDust::bLinger) neste quadro; falso = acabou (sai de cena). */
	bool UpdateLinger(FDust& D, AExplosionVolume* E);

	UPROPERTY() TArray<TObjectPtr<UExplosionBake>> Bakes;
	UPROPERTY() TObjectPtr<UMaterialInterface> VolumeMaterial;
	UPROPERTY() TArray<TObjectPtr<AExplosionVolume>> Alive;
	UPROPERTY() TArray<TObjectPtr<AExplosionVolume>> Warmups;

	bool bPaused = false;
	bool bWarm = false;
	float WarmClock = 0.0f;

	bool bHasView = false;
	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewForward = FVector::ForwardVector;
	FVector ViewRight = FVector::RightVector;
	FVector ViewUp = FVector::UpVector;
	float TanHalfX = 1.0f;
	float TanHalfY = 1.0f;
};
