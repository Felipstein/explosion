// (23/10) A explosão com vida: o acerto de míssil (a Patriot no veículo, o lançador Tomahawk no prédio e no veículo, o Cruise
// Missile Strike no chão). O Felipe: "é spawnado ali e pronto, sem interação, sem natureza, sem vida"; "a explosão nasce de onde
// bateu e sai torta (mas é física, não regra: no chão sobe do chão plano)"; e no ataque "explosão linda, mas falta a interação no
// chão: areia grossa voando, um impacto e uma cratera bem maiores".
//
// O plano (o diretor das explosões, 23/10; as fases 1 a 14, as palmeiras e a luz herói ficam pra fase 2) e o estudo: docs/
// ESTUDO_MISSIL_PATRIOT.md, "A explosão com vida". Um evento (o pulso do jogo, FRTSHitLife no RTSGame) e muitas reações, cada uma
// atrás da sua variável blast.HitLife.<camada> (0/1) pro A/B e pra tirar uma camada se ele reprovar. -HitLifeV=1 volta a de hoje,
// exata; -HitLifeV=2 (o padrão) é a com vida. Só desenho: nada volta pra simulação (o lockstep fica igual em todo PC).
//
// Sem UCLASS aqui (o cc_check usa os .generated.h da última compilação: este cabeçalho não mexe em nenhum).
#pragma once

#include "CoreMinimal.h"

namespace BlastFxHitLife
{
	/** As camadas (as variáveis blast.HitLife.<nome>, 1 = ligada, o padrão). Os números entre parênteses são os itens do plano. */
	enum ELayer : int32
	{
		Dir,        // a direção ℓ = n + 0,3·cima·[face de pé] + w·t: a bola nasce no ponto e sai torta, o giro, as faíscas, as lascas, a luz e o anel por ℓ
		Glints,     // (1) os brilhos dos estilhaços no casco do alvo e dos vizinhos dentro dos 50 kPa
		Sparks,     // (2) as faíscas em jatos inclinadas pra frente (fragment_tilt_deg), 50·K² (teto 90), a vida sem o ×K
		Specks,     // (3) os pontinhos dos estilhaços no chão, numa elipse à frente, só na areia e só na explosão acima do chão (fora da
		            // planta do alvo e da sombra dele)
		Veil,       // (4) o véu de areia do choque: a onda pequena que empurra a areia do chão pro lado (a faixa densa até o vento de
		            // 100 m/s atrás do choque e a franja até 50 m/s, Kinney-Graham; o vazio atrás do casco), nascendo com a chegada do choque.
		            // Com ele o anel de areia da ogiva não nasce (FHitIn::bGameVeil: uma representação só); com a gravação dedicada, ele é a
		            // continuação dela (FHitOut::bOwnSand)
		            // (23/10, o Felipe) a camada Under (a poeira saindo de baixo do casco) saiu: o chão embaixo do casco fica tapado por ele
		Kick,      // (5) o tranco do alvo pela física, × (W/4 kg)·(60 t/M), até o teto da mola
		Neighbors,  // (6) os vizinhos do dano em área: o tranco pela razão do dano e a marca na face virada pra explosão
		Cloud,      // (8) a nuvem continua viva: o quadro parado, sobe e alarga como térmica, dilui, vai com o vento
		Smolder,    // (9) a fumaça saindo do ponto do acerto (o RTSMissileSmoke), presa ao casco
		Embers,     // (10) os pedaços em brasa: voam com o fio de fumaça, caem, ficam piscando, levantam areia e fumegam
		Ground,     // (11) o chão: o chamuscado no pé da bola, o míssil no chão com a terra e os torrões na escala K, a terra escura no 2º
		TrailRise,  // (12) a subida que puxa a ponta do rastro dura o que a nuvem viva dura
		Shake,      // (14) o tremor atrasado pela distância (a partir de 12 kg)
		Strike,     // o chão do ataque de mísseis: a cratera maior, os torrões na escala dele, a gravação strike_chao em todo míssil (com
		            // a camada Bake) e, sem ela, a onda de base de reserva na poeira em volume
		Bake,       // as gravações dedicadas (warhead_lado, warhead_teto, warhead_chao, strike_chao), quando existem
		Linger,     // (a areia do acerto) a poeira que fica do ataque de mísseis na poeira em volume: rola e deriva no vento depois da gravação
		Num
	};

	/** -HitLifeV= (2, o padrão: com vida; 1: a de hoje, exata). Com -MissileBlastV=1 (a ogiva de antes) nunca há vida. */
	BLASTFX_API int32 Version();
	/** A camada está ligada: a versão 2, a ogiva v2 e a variável dela ≠ 0. */
	BLASTFX_API bool On(ELayer Layer);
	/** O preset do jogo: 0 baixa, 1 média, 2 alta, 3 ultra (o sg.EffectsQuality 1–4 do jogo). */
	BLASTFX_API int32 Preset();
	/** K = (W/4 kg)^⅓ (os tamanhos e os tempos de Hopkinson-Cranz, a receita aprovada é a da ogiva de 4 kg) e G = (W/4 kg)^⅙ (as fases
	 *  de gravidade e empuxo: o voo dos torrões, a subida, o assentar; v ∝ √(gR)). 1 | 1,44 | 1,71 e 1 | 1,20 | 1,31 a 4, 12 e 20 kg. */
	inline float K(float Kg) { return FMath::Pow(FMath::Max(Kg, 0.01f) / 4.0f, 1.0f / 3.0f); }
	inline float G(float Kg) { return FMath::Pow(FMath::Max(Kg, 0.01f) / 4.0f, 1.0f / 6.0f); }

	/** O que o jogo passa a mais pro acerto com vida (UBlastFxDirector::MissileHit). */
	struct FHitIn
	{
		/** A inclinação do leque dos estilhaços pra frente, atan(Vm/Vf) de Gurney (Units.json hit_life.fragment_tilt_deg): a Patriot 34°
		 *  (PAC-2: 31–38°), o Tomahawk 8° (7–9°). */
		float TiltDeg = 20.0f;
		/** w: o peso do que o míssil traz na direção ℓ (hit_life.carry): 0,8 na Patriot (o interceptador rápido), 0,5 no Tomahawk e no
		 *  ataque (subsônicos). [E], a fechar no A/B. */
		float Carry = 0.5f;
		/** A carga que joga a terra no chão (kg; hit_life.ejecta_kg): o ataque de mísseis, 20 (a clássica do jogo, nas duas metades: o
		 *  mesmo míssil); 0 = a da ogiva. */
		float EjectaKg = 0.0f;
		/** O ataque de mísseis (a gravação strike_chao no chão, a terra e os torrões na escala dele). */
		bool bStrike = false;
		/** (a revisão) O jogo desenha a areia que o choque levanta no chão (o véu, na poeira em volume: FRTSHitLife::DrawsShockVeil): o anel
		 *  de areia da ogiva (a sim_chao_anel) não nasce, uma representação só (os dois juntos eram a mesma areia duas vezes). Sem o véu (a
		 *  camada desligada, -VeilV=1, -HitLifeV=1) o anel de antes. */
		bool bGameVeil = false;
	};

	/** O que o BlastFx devolve do acerto com vida (o jogo distribui as reações pelo pulso). bValid só com a vida. */
	struct FHitOut
	{
		bool bValid = false;
		bool bShown = true;           // perto da vista (fora dela o BlastFx deixa só a marca no casco e a carcaça)
		bool bOnHull = false;         // o raio achou a caixa de uma peça do alvo
		bool bTop = false;            // a face de cima (o teto)
		bool bBody = false;           // nasceu (ou se fundiu numa acesa) uma bola de ogiva em volume
		FVector PointCm = FVector::ZeroVector;          // o ponto do acerto (no casco ou no chão)
		FVector NormalW = FVector::UpVector;            // a normal da face (no chão, a do terreno)
		FVector IncomingW = FVector::ForwardVector;     // d: o rumo do míssil em 3D (o mergulho guardado)
		FVector AimW = FVector::UpVector;               // ℓ: pra onde a explosão sai
		FVector FootCm = FVector::ZeroVector;           // o pé da bola ao nascer (no chão)
		FVector BallCentreCm = FVector::ZeroVector;     // o centro da bola acesa que se vê
		float BallM = 0.0f;           // o raio da bola de fogo (m; 0 sem bola)
		float FireEndS = 0.0f;        // o fim da bola acesa (s de idade)
		float CloudEndS = 0.0f;       // o fim da nuvem viva (s de idade; 0 = sem a nuvem viva)
		/** (a revisão da vida) A caixa do alvo no espaço dele, sem escala (cm; +X = a frente do ator): a soma das caixas das peças que o raio
		 *  enxerga (o casco, a torre, as lagartas): a planta do alvo no chão (a sombra do casco na onda do choque, cada veículo com a sua). */
		bool bHullBox = false;
		FVector HullMinCm = FVector::ZeroVector, HullMaxCm = FVector::ZeroVector;
		/** (23/10 noite) O rumo do casco (graus): o espaço da caixa acima é o do pé do ator girado só por este rumo (o do componente Body: o ator
		 *  do veículo não gira, RTSUnitActor), não o do ator. */
		float HullYawDeg = 0.0f;
		/** (a areia do acerto) A gravação de areia do ataque (strike_chao, só areia, na escala 1) nasceu neste míssil: o perto (o véu, a
		 *  cortina, a onda de base) é dela; o jogo põe só a poeira que fica (Linger) a partir do esmaecer dela (SandFadeS, s de idade) até o
		 *  fim (SandEndS). Sem ela (sem o arquivo, sem vaga, a camada Bake desligada, o míssil fora do ataque) o jogo faz o perto também. */
		bool bSandBake = false;
		float SandFadeS = 0.0f, SandEndS = 0.0f;
		/** (a revisão) A gravação dedicada da ogiva (warhead_lado, _teto, _chao) nasceu neste acerto: ela já tem a areia que o choque levanta.
		 *  O jogo não desenha o perto por cima dela: o véu (no veículo) e a poeira que fica (no chão) nascem no esmaecer dela (OwnSandFadeS, s
		 *  de idade), a continuação, como a poeira que fica depois da strike_chao. */
		bool bOwnSand = false;
		float OwnSandFadeS = 0.0f;
	};

	/** (a revisão da vida) Um pedaço em brasa ainda queimando em voo, neste quadro: o jogo solta o fio de fumaça dele no RTSMissileSmoke (em
	 *  volume, com a sombra e a luz da cena), um puff a cada tantos cm do caminho (UBlastFxDirector::ConsumeEmberSmoke). */
	struct FEmberSmoke
	{
		uint32 Id = 0;                // estável enquanto o pedaço voa queimando
		FVector PosCm = FVector::ZeroVector;
	};

	/** Um pouso (o pedaço em brasa que caiu, o torrão grande do chão): a fila que o jogo esvazia a cada quadro
	 *  (UBlastFxDirector::ConsumeLandings) pro puff de areia e o fiapo. */
	struct FLanding
	{
		FVector PosCm = FVector::ZeroVector;
		float SizeM = 0.3f;
		uint8 Kind = 0;               // 0 o pedaço da ogiva, 1 o torrão do chão
		bool bHot = false;            // o pedaço caiu ainda queimando (fica piscando e fumega)
		/** (a areia do acerto) A velocidade no 1º toque no chão (m/s): a cratera do respingo cresce com a energia da batida (Uehara et al.
		 *  2003, Dc ∝ s^¾·H^¼, H = v²/2g) e a areia sai pra onde ele ia. Zero = o pouso parado (o tamanho só pelo pedaço). */
		FVector VelMs = FVector::ZeroVector;
	};
}
