#pragma once

#include "Sim/SimApi.h"

// IA de skirmish v0: um jogador virtual. Lê o estado (como o desenho lê) e dá
// ordens como um humano daria, pela mesma fila de comandos. Não tem atalho:
// não mexe em nada da simulação, só emite comandos (docs/JOGO.md §3.5).
//
// Comportamento v0:
//  - só tanques: espera juntar o grupo inicial e manda todo mundo em
//    ataque-andando até o inimigo mais perto do centro do próprio exército;
//  - com base (o robô de teste da economia, docs/ECONOMIA.md): uma lista fixa de
//    construção (usina → centro de suprimentos perto do depósito → War Factory),
//    2 caminhões, a fábrica sempre com fila, e ataque quando junta uma onda.
//    Reconstrói o que perder. Não é a IA de verdade (essa vem depois).
//  - (v4, 22/10) o dinheiro inicial de 10 000 a 50 000: o banco grande vira mais fábricas, Dozers e defesas; e a obra
//    nova não vai pra onde o inimigo está nem pra onde uma fundação do mesmo papel acabou de morrer.
//  - (v4, 23/10, a IA que usa tudo que é novo) os papéis saem dos dados (a renda = prédio com avião de carga; a artilharia =
//    arma com alcance mínimo), então unidade nova entra sem código. Na base: 3 caminhões por centro vivo (e a fábrica guarda
//    o dinheiro de um caminhão enquanto falta), a energia olhando a obra que vem (a de −3 não sobe sem energia), a expansão
//    antes do depósito de casa secar, a zona de lançamento atrás do CC (economizando no máximo IncomeSaveMaxS) e a fábrica
//    escolhendo pelo que falta (o Tomahawk quando a patente deixa, com 2 tanques de escolta por lançador). No ataque: a
//    casa se defende, o poder do general mira na célula que mais vale, a onda cresce com a patente, o cerco (a artilharia
//    atira na defesa; os tanques param a SiegeHoldM dela) e o alvo pelo valor (zona, caminhão, centro, usina) em vez do
//    mais perto. Tudo inteiro, desempate pelo Id; o estado dela fica fora do hash (só os comandos entram no replay).
//    Cada decisão vira uma linha de log (TakeLog): "RTS AI p1: drop zone at x,y", "strike at x,y score s"...
//  - (2ª revisão de 23/10) a defesa inimiga (conhecida ou não, a mesma régua do lugar da obra) tira da escolha o depósito
//    com o lugar do centro ou a ida e volta do caminhão no alcance dela; o Dozer contorna o alcance pelo A* do jogo (com os
//    círculos fechados); o CC não repõe Dozer com a porta coberta e repõe com espera crescente depois de 2 mortes em 5 min;
//    e a defesa plantada na porta dela é o 1º alvo da onda.
//  - (3ª revisão de 23/10) sem caminho fora dos alcances por LostSiteMemoryS, o Dozer passa raspando pelo menos exposto se
//    sobrevive ao pior caso nele; sem lugar em volta de casa, a obra vai pra perto do prédio dela mais perto do Dozer (ou do
//    próprio Dozer); o Dozer com obra parado no lote da obra de outro Dozer parado (as duas sem andar por DozerStallS)
//    larga a obra (o pensar seguinte retoma a parada mais perto dele); e o Dozer à toa no lote dessa obra parada (o jogo o
//    manda sair pra dentro do CC) ou encostado no Dozer dela que não consegue andar vai pra volta do CC, fora do alcance.
//  - (4ª revisão de 23/10, a base cercada) a procura de lugar só aceita o lote aonde algum Dozer à toa chega (a região dele
//    na folga com os alcances fechados): por fora dos alcances primeiro, raspando depois, e o lote na névoa vira "ir ver"; o
//    lugar escolhido fica até a obra sair, com o Dozer mais perto dele; o centro sem depósito guarda o lugar da fábrica; e,
//    com um alcance perto, o Dozer recebe a obra já do lado do lote fora do alcance. Sem obra ao alcance de nenhum, o CC faz
//    mais um se o da porta chega. O Dozer no alcance tenta a obra (pelo caminho que ele sobrevive) e, sem ela, foge pelo
//    caminho menos exposto pro lado aberto, perna atrás de perna sem parar. Com a porta coberta e sem fábrica, o CC só
//    faz um a mais se o novo sobrevive à saída pro lado aberto (sem nenhum, repõe), e para depois de 3 Dozers mortos no
//    alcance de uma defesa sem a base andar (prédio pronto ou defesa inimiga caída). E o exército que não cresce mais (sem
//    renda, sem expansão em obra e sem o preço de um armado) sai com o que tem.

#include <cstdint>
#include <string>
#include <vector>

#include "Sim/Commands.h"
#include "Sim/World.h"

namespace Sim
{
	class SIMCORE_API SkirmishAI
	{
	public:
		explicit SkirmishAI(PlayerIndex InMe) : Me(InMe) {}
		PlayerIndex Player() const { return Me; }

		/** Pensa no passo atual e devolve as ordens a emitir agora (pode ser nenhuma). */
		void Think(const World& State, std::vector<Command>& OutCommands);

		/** Espera antes do primeiro ataque, pra o jogador se orientar. */
		int32_t FirstWaveDelayTicks = 20 * TicksPerSecond;
		/** De quanto em quanto tempo ela reavalia o alvo do ataque. */
		int32_t RethinkTicks = 3 * TicksPerSecond;
		/** Com base: quantos tanques juntam antes de atacar. */
		int32_t WaveSize = 6;
		/** Com base: a onda que ficou com isto ou menos volta a juntar, e a próxima cresce WaveGrowth (até MaxWaveSize).
		 *  (v4, 23/10) 4 e 1 (eram 2 e 2): no harness de 40 partidas IA × IA, a mediana passou de ~14,6 pra ~16 min (as bases
		 *  não caem juntas na 2ª onda) e a mistura de gasto mexe mais. */
		int32_t RegroupAt = 4;
		int32_t WaveGrowth = 1;
		int32_t MaxWaveSize = 14;
		/** Com base: caminhões que ela mantém por centro vivo. (v4) 3: o 3º rende ~900 $/min no depósito de casa a 74 m por
		 *  $600 (paga em 40 s); o depósito atende um por vez e chega perto de 3 600 $/min, então o 4º quase não soma. */
		int32_t TrucksWanted = 3;
		/** (v4) Enquanto faltam caminhões, a fábrica só põe na fila com o dinheiro do caminhão sobrando (a renda caía a 0 aos
		 *  ~4 min: os caminhões morriam, o dinheiro ia todo pra tanque e nunca voltava). */
		int32_t TruckReserve = 600;
		/** (v4, o dinheiro inicial de 10 000 a 50 000) Com base: o banco que daria mais de BankHorizonS segundos de tanque
		 *  numa fábrica pede mais uma, até MaxFactories (10 000 → 1, como antes; 20 000 → 2; 30 000 → 3; 40 000 ou mais → 4). */
		int32_t BankHorizonS = 120;
		int32_t MaxFactories = 4;
		/** (v4) As defesas de base: até DefensesBase + as fábricas queridas (no máximo 4); a 1ª logo depois da fábrica, as
		 *  outras com dinheiro sobrando ou, depois de um ataque à casa, com o preço delas. */
		int32_t DefensesBase = 1;
		/** (v4) A expansão cedo: com as caixas ao alcance dos centros vivos abaixo disto (milésimos) do que eles tinham no
		 *  começo, um centro novo no depósito com caixa fora do alcance (uns 2 min antes do de casa secar: fecha o buraco de
		 *  renda zero dos 12:00–14:15). */
		int32_t ExpandAtPermille = 250;
		/** (v4) A zona de lançamento (o papel da renda): a 1ª a partir de IncomeFirstAfterS (ou da 1ª onda), a 2ª a partir de
		 *  IncomeSecondAfterS (ou da base seca), sempre guardando IncomeReserve (um tanque); enquanto falta dinheiro, a fábrica
		 *  para no máximo IncomeSaveMaxS. */
		int32_t IncomeFirstAfterS = 180;
		int32_t IncomeSecondAfterS = 600;
		int32_t IncomeReserve = 900;
		int32_t IncomeSaveMaxS = 60;   // (o spec dizia 40; medido 23/10: com a renda de ~3 600 $/min, 40 s juntam ~2 700 e a zona nunca saía)
		/** (v4, revisão de 23/10) O centro novo (a base seca, a que vai secar, ou sem centro): a fábrica para enquanto falta o
		 *  preço dele, no máximo CenterSaveMaxS (depois gasta normal o mesmo tempo da zona e abre outra janela). Com o preço no
		 *  banco ela segue guardando só ele (o Dozer andando até o depósito na névoa, ou o Dozer novo saindo do CC, não param
		 *  mais as fábricas: medido, 100 a 400 s paradas com $3 000 a $8 000 no banco); sem Dozer nem CC, não guarda nada.
		 *  (2ª revisão de 23/10) 180 (era 60): com 60 s e mais 60 gastando, a base só com a renda da zona (o centro morto, ou
		 *  seco) punha o centro aos 270 s em vez de 121 s; com 180 volta aos 121 s, e a fábrica parada com o preço do centro no
		 *  banco segue de fora (a janela só corre com o dinheiro abaixo do preço). */
		int32_t CenterSaveMaxS = 180;
		/** (v4) A artilharia (o Tomahawk): até 1 + as defesas inimigas conhecidas, no máximo ArtilleryMax, e só com 2 tanques
		 *  de escolta por lançador (mais 2). */
		int32_t ArtilleryMax = 4;
		/** (v4) O poder do general (o Cruise Missile Strike): olha a cada StrikeThinkS e dispara na célula que vale pelo menos
		 *  StrikeMinScore (uma zona sozinha vale 7 500; uma Patriot sozinha 1 500, não; Patriot + usina 2 700, sim). */
		int32_t StrikeThinkS = 3;
		int32_t StrikeMinScore = 2000;
		/** (v4) O cerco: com 2 ou mais de artilharia e uma defesa inimiga a até SiegeRadiusM do exército, a artilharia atira
		 *  nela e os tanques param a SiegeHoldM dela (fora dos 63,2 m da Patriot, com folga). (revisão de 23/10) O grupo do
		 *  cerco é quem está a até SiegeRadiusM da defesa: o cerco segue enquanto ela vive e o grupo tem 2+ lançadores (o
		 *  reforço que cruzava os 200 m puxava a frente pra trás e derrubava o cerco em 3 a 15 s), e o reforço de longe vai
		 *  até o grupo em ataque-andando antes de receber o ponto de espera. */
		int32_t SiegeRadiusM = 140;
		int32_t SiegeHoldM = 72;
		/** (v4) A casa que se defende: inimigo armado a até HomeAlertM de um prédio dela chama quem está a até HomeCallM de
		 *  casa (a onda que já saiu não volta). */
		int32_t HomeAlertM = 90;
		int32_t HomeCallM = 200;
		/** (v4) A onda cresce com a patente: pelo menos a 1ª onda + WavePerRank por patente acima da 1 (até MaxWaveSize).
		 *  (o spec dizia 2; 1 junto com o WaveGrowth 1: a patente chega cedo, R3 aos ~5 min, e a onda de 10 aos 5 min acabava a
		 *  partida na 2ª onda) */
		int32_t WavePerRank = 1;

		/** (v4) Leva as linhas de log das decisões dela desde a última chamada ("RTS AI p1: strike at x,y score s"). */
		void TakeLog(std::vector<std::string>& Out);

	private:
		/** (revisão de 23/10) Um círculo onde a obra não vai: o alcance de verdade de uma defesa inimiga contra ela. */
		struct KeepOut
		{
			Vec2 Pos;
			int64_t RadiusSq = 0;
			/** (3ª revisão de 23/10) O tipo da defesa (a arma dela, pro dano de quem passa raspando); -1 = sem arma. */
			int32_t Def = -1;
		};
		/** A parte da base: obras, caminhões e fila da fábrica. */
		void ThinkBase(const World& State, std::vector<Command>& OutCommands);
		/** O primeiro lugar que a regra deixa, numa espiral em volta de Anchor (determinístico). (revisão de 23/10) Pula os
		 *  lugares dentro de Avoid (a defesa não anda: a espiral segue até sair do alcance dela, em vez de travar a obra).
		 *  (4ª revisão de 23/10) Com Reach, só o lote com o centro numa célula marcada (aonde o Dozer chega). Lax 1: também o
		 *  lote na névoa (o CanPlace só recusa por não ver: o Dozer vai lá ver); 2: também sem o pré-requisito ou o dinheiro
		 *  (o lugar da fábrica que ainda não pode). Com Keep, nenhum lote em cima dele (o lugar guardado pra outra obra). Com
		 *  bFine, a espiral anda uma célula por vez. */
		struct Lot
		{
			Vec2 Pos;
			Fix HalfX;
			Fix HalfY;
		};
		bool FindSpot(const World& State, uint16_t Def, int32_t Dir, Vec2 Anchor, Fix MinRadius, Angle Start, const std::vector<KeepOut>& Avoid, Vec2& Out,
			const std::vector<uint8_t>* Reach = nullptr, int32_t Lax = 0, const Lot* Keep = nullptr, bool bFine = false) const;
		/** O lado (0..3) que aponta de From pra To. */
		static int32_t DirToward(Vec2 From, Vec2 To);
		/** (2ª revisão de 23/10) O próximo passo seguro de Walker até To com Mobility (a folga do A* do jogo): To, se o
		 *  caminho do jogo não passa em nenhum círculo de Danger; senão o 1º ponto de um caminho que contorna os círculos (o
		 *  mesmo A* com eles fechados). O círculo onde Walker ou To já estão encolhe até eles (quem está dentro sai sem ir mais
		 *  pra perto da defesa). (3ª revisão) Com bGraze, sem caminho fora deles, o menos exposto: os círculos encolhem uma
		 *  célula por vez (até GrazeTiers) e o 1º caminho vale se Walker sobrevive ao pior caso nele (a rajada cheia na entrada
		 *  do alcance de cada defesa e o pente pelo tempo lá dentro). Devolve 0 = nenhum caminho, 1 = fora dos círculos, 2 =
		 *  raspando. (4ª revisão) O caminho longo perto de um alcance vai em pernas de RouteLegMaxM (com bLegs); com bAnyDamage
		 *  (a fuga), sem pernas, e o 1º caminho raspando vale qualquer que seja o dano. */
		int32_t SafeStep(const World& State, const std::vector<KeepOut>& Danger, const UnitView& Walker, Vec2 To, int32_t Mobility, bool bGraze, Vec2& OutStep,
			bool bAnyDamage = false, bool bLegs = true);
		/** (4ª revisão de 23/10) Os círculos de Danger que valem de From até To: o círculo onde um dos dois já está encolhe
		 *  até ele, menos uma célula (quem está dentro sai sem ir mais pra perto da defesa). */
		static std::vector<KeepOut> LiveCircles(const World& State, const std::vector<KeepOut>& Danger, Vec2 From, Vec2 To);
		/** (4ª revisão) A folga do mapa em SafeMap com as células dos círculos Live (menos Shrink) fechadas: só ela o A* lê. */
		void CloseCircles(const SimMap& Map, const std::vector<KeepOut>& Live, Fix Shrink);
		/** (4ª revisão) Aonde quem anda de From chega com Mobility sem entrar nos círculos de Danger (Tier células a menos em
		 *  cada um; o de From encolhe até ele): marca as células em ReachMark e devolve quantas (0 = nem a partida). */
		int32_t FloodReach(const World& State, const std::vector<KeepOut>& Danger, Vec2 From, int32_t Mobility, int32_t Tier);
		/** (4ª revisão) A saída de Walker do alcance das defesas: o 1º passo do caminho menos exposto (o tempo em cada círculo
		 *  pesa) até o lado aberto (fora de todo círculo de Danger, numa região grande; bEscapeOpen diz se achou um, senão vai
		 *  pro bolso mais perto). Com bAnyDamage, vale qualquer que seja o dano (quem já está no alcance e fica morre); sem, só
		 *  se ele sobrevive ao pior caso. */
		bool EscapeStep(const World& State, const std::vector<KeepOut>& Danger, const UnitView& Walker, int32_t Mobility, bool bAnyDamage, Vec2& OutStep);
		/** (4ª revisão) O pior caso de Walker andando por Way (a rajada cheia de cada defesa na entrada do alcance dela e o pente
		 *  pelo tempo lá dentro). */
		static int64_t PathDamage(const World& State, const std::vector<KeepOut>& Danger, const UnitView& Walker, const std::vector<Vec2>& Way);
		void Attack(const World& State, std::vector<Command>& OutCommands, bool bBase);
		/** (v4) Ela conhece esse inimigo? À vista agora; o prédio, numa célula que o time já viu (o prédio não anda). */
		bool Knows(const World& State, const UnitView& V) const;
		/** (v4) A casa sob ataque: quem está perto de casa vai no intruso. Devolve os que ficam guardando a casa. */
		void DefendHome(const World& State, std::vector<Command>& OutCommands, std::vector<EntityId>& OutHomeGuard);
		/** (v4) Os poderes do general prontos: na célula conhecida que mais vale, se passar de StrikeMinScore. */
		void UsePowers(const World& State, std::vector<Command>& OutCommands);
		/** (v4) O cerco da defesa mais perto da frente; falso = sem cerco (o alvo pelo valor segue). */
		bool Siege(const World& State, std::vector<Command>& OutCommands, Vec2 Front, const std::vector<EntityId>& HomeGuard);
		/** (revisão de 23/10) O cerco acabou (a defesa caiu, o grupo perdeu os lançadores, a onda voltou pra casa): limpa o
		 *  alvo, pro próximo cerco na mesma defesa sair no log de novo. */
		void EndSiege(const World& State);
		/** (v4) Uma linha de log ("RTS AI p<Me>: " + o texto). */
		void Note(const char* Format, ...);

		PlayerIndex Me = 0;
		TickNumber NextThink = 0;
		EntityId CurrentTarget = NoEntity;
		/** A onda já saiu (com base): os tanques novos vão reforçar. */
		bool bWaveSent = false;
		/** (4ª revisão de 23/10) O exército não cresce mais (sem renda e sem o preço de um armado): a onda sai com o que tem. */
		bool bArmyCapped = false;
		/** (v4) O tamanho da 1ª onda (lido no 1º ataque com base): a onda pela patente cresce a partir dele. */
		int32_t StartWaveSize = 0;
		/** (v4) A janela de economizar pra zona: quando começou (a fábrica para no máximo IncomeSaveMaxS dela). */
		bool bIncomeSaving = false;
		TickNumber IncomeSaveSince = 0;
		/** (revisão de 23/10) A janela de economizar pro centro novo (a fábrica para no máximo CenterSaveMaxS dela). */
		bool bCenterSaving = false;
		TickNumber CenterSaveSince = 0;
		/** (v4) A defesa sob cerco, o intruso da casa e o próximo olhar dos poderes (só pro log e o ritmo; fora do hash).
		 *  (revisão de 23/10) O cerco segura a defesa e o ponto de espera escolhido quando ele começou. */
		EntityId SiegeTarget = NoEntity;
		Vec2 SiegeHold;
		EntityId DefendTarget = NoEntity;
		TickNumber NextStrikeThink = 0;
		/** (v4) Quando o último alarme da casa acabou (0 = nenhum pendente): a próxima defesa sai com o preço dela. */
		TickNumber AlertEndedAt = 0;
		/** (v4) O depósito da expansão pedida (o log diz qual quando a fundação aparece). */
		int32_t ExpandDock = -1;
		std::vector<std::string> LogLines;
		/** (v4) Uma fundação dela: o papel na base, o lugar e o passo em que foi vista (ou em que sumiu). */
		struct Site
		{
			EntityId Id = NoEntity;
			int32_t Role = -1;
			Vec2 Pos;
			TickNumber Tick = 0;
		};
		/** As fundações do último pensar e as que sumiram inacabadas (morreram, ou (v4) ela cancelou a parada em lugar
		 *  perigoso). O inimigo matava a de 1 PV, o Dozer ficava livre e ela recolocava a cada segundo (9 Patriots em 20 s, de
		 *  11 000 pra 1 100). */
		std::vector<Site> Sites;
		std::vector<Site> LostSites;
		/** (v4) Os Dozers do último pensar (o que some morreu: o lugar entra no LostSites como perigoso). */
		std::vector<Site> LastDozers;
		/** (revisão de 23/10) As zonas e os centros prontos do último pensar (o que some morreu: o lugar entra no LostSites, o
		 *  que o spec da zona e o depósito disputado pediam; o centro de casa não) e as obras mandadas no último pensar (a
		 *  fundação de 1 PV que morre antes do pensar seguinte nunca entrava no Sites). */
		std::vector<Site> LastBuilt;
		std::vector<Site> PendingSites;
		/** (2ª revisão de 23/10) Os lugares (e os depósitos, no papel do centro) aonde o Dozer não chega sem passar no alcance
		 *  de uma defesa, há menos de LostSiteMemoryS: a procura passa deles. E os passos em que Dozers morreram (o CC só
		 *  repõe com espera crescente depois de 2 mortes em 5 min: medido, um Dozer a cada 12 s morrendo na porta do CC, 31
		 *  em 8 min). */
		std::vector<Site> Unreachable;
		std::vector<TickNumber> DozerDeaths;
		/** (3ª revisão de 23/10) O Dozer com obra: onde estava, a obra e quanto faltava nela desde Since (sem sair do lugar
		 *  e sem a obra andar por DozerStallS, parado no lote da obra de outro Dozer assim, ele larga a obra). */
		struct DozerWatch
		{
			EntityId Id = NoEntity;
			EntityId Target = NoEntity;
			Vec2 Pos;
			int32_t BuildLeft = 0;
			TickNumber Since = 0;
		};
		std::vector<DozerWatch> DozerWatches;
		/** (3ª revisão de 23/10) Desde quando o Dozer não acha caminho fora dos alcances pra obra nenhuma (0 = chegou numa
		 *  obra por fora deles, ou não há defesa inimiga): passado o LostSiteMemoryS (os lugares sem caminho já deram a volta no Unreachable), ele passa
		 *  raspando. E o Dozer que está raspando (no pensar seguinte ele segue o caminho em vez de fugir do círculo onde parou). */
		TickNumber RouteBlockedSince = 0;
		EntityId GrazeWalker = NoEntity;
		/** (4ª revisão de 23/10) O lugar escolhido pra obra que o Dozer ainda está indo ver ou alcançar (pelo desvio ou
		 *  raspando, um passo por pensar): o pensar seguinte segue nele enquanto ele vale (o lugar ancorado no Dozer mudava a
		 *  cada passo e a ida parava). PlanRole −1 = nenhum. */
		EntityId PlanDozer = NoEntity;
		int32_t PlanRole = -1;
		int32_t PlanDef = -1;
		int32_t PlanDir = 0;
		Vec2 PlanSpot;
		/** (4ª revisão) Onde o Dozer do lugar escolhido estava e desde quando (parado por DozerStallS, os outros não esperam). */
		Vec2 PlanDozerPos;
		TickNumber PlanDozerSince = 0;
		/** (4ª revisão) O Dozer travou andando pro ponto do lado do lote: esse lugar vai com a obra direto. */
		bool bPlanDirect = false;
		/** (4ª revisão) Os Dozers mortos no alcance de uma defesa inimiga desde que a base andou pela última vez (um prédio
		 *  dela ficou pronto ou uma defesa inimiga caiu), e o que ela tinha no último pensar pra ver isso. */
		int32_t FutileDeaths = 0;
		int32_t LastCompleted = 0;
		int32_t LastThreats = 0;
		/** (4ª revisão) As células aonde quem anda chega (FloodReach) e a pilha da inundação. */
		std::vector<uint8_t> ReachMark;
		std::vector<int32_t> ReachStack;
		/** (4ª revisão) O caminho do último SafeStep que achou caminho (a fuga confere o pior caso dele). */
		std::vector<Vec2> LastWay;
		/** (4ª revisão) A última procura de lugar que não achou nada: o que ela leu (PlaceSig), até quando vale e o que ela
		 *  disse do Dozer novo da porta. */
		uint64_t PlaceSig = 0;
		TickNumber PlaceRetryAt = 0;
		bool bPlaceDoorHelps = false;
		/** (4ª revisão) Os Dozers andando numa perna de fuga e o fim da perna de cada um. */
		std::vector<std::pair<EntityId, Vec2>> FleeTo;
		/** (4ª revisão) A última saída do EscapeStep dá no lado aberto (não num bolso entre os alcances). */
		bool bEscapeOpen = false;
		/** (4ª revisão) A fuga da porta coberta (a reposição do Dozer): o resultado, quando foi feita e com quantas defesas. */
		bool bDoorEscapeOk = false;
		bool bDoorEscapeOpen = false;
		TickNumber DoorEscapeAt = 0;
		int32_t DoorEscapeThreats = -1;
		/** (4ª revisão) O Dijkstra da fuga: quantos círculos cobrem cada célula, as regiões fora deles, o custo e de onde veio. */
		std::vector<uint8_t> EscapeInside;
		std::vector<int32_t> EscapeLabels;
		std::vector<int64_t> EscapeCost;
		std::vector<int32_t> EscapeFrom;
		/** (2ª revisão de 23/10) O A* do jogo pro caminho do Dozer e a cópia da folga do mapa com os alcances fechados. */
		Pathfinder Paths;
		SimMap SafeMap;
	};
}
