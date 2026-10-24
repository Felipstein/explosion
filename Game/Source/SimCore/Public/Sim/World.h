#pragma once

#include "Sim/SimApi.h"

// A simulação da partida (o "juiz"). Recebe comandos, anda um passo de 1/30 s
// e conta o que aconteceu. Todo PC roda a mesma, com os mesmos comandos, e
// chega no mesmo estado bit por bit (StateHash).
//
// Regras do código aqui dentro (docs/JOGO.md §3.2):
//  - só inteiros e ponto fixo; nada de float, relógio, threads ou ponteiro como chave;
//  - toda varredura em ordem de ID; empate sempre desempatado pelo ID;
//  - quem vem de fora (desenho, IA) só lê; a única entrada é Step(comandos).

#include <cstdint>
#include <string>
#include <vector>

#include "Sim/Commands.h"
#include "Sim/Defs.h"
#include "Sim/Events.h"
#include "Sim/FixedMath.h"
#include "Sim/Pathfinder.h"
#include "Sim/Rng.h"
#include "Sim/SimMap.h"
#include "Sim/Types.h"
#include "Sim/Vision.h"

namespace Sim
{
	struct StartingUnit
	{
		std::string Key;
		Vec2 Pos;
		Angle Facing;
	};

	/** Depósito de suprimentos do mapa (o SupplyDock do ZH): parado, de ninguém, com caixas. */
	struct SupplyDockSetup
	{
		Vec2 Pos;
		Fix HalfX;
		Fix HalfY;
		int32_t Boxes = 0;
	};

	/** (16/10) Um objeto do mapa que muda o caminho e o tanque esmaga ou derruba (docs/ESMAGAR_E_DERRUBAR.md). */
	enum class PropKind : uint8_t
	{
		Car = 0,   // o carro civil: achata (frente, traseira ou os dois) e fica no mapa, aberto pra todos
		Tree = 1,  // a palmeira, a árvore: cai e fica deitada, aberta pra todos
	};

	/** (v4, 23/10) O derrubar pelo choque (o caminhão, o Tomahawk: a física, não o nível do ZH). O veículo de massa m a v derruba
	 *  o objeto quando a energia do choque, ½·m·v², passa da que o objeto absorve até cair (estimativas [E]; a fratura do poste de
	 *  madeira não é bem documentada: SAE 980214 e 2011-01-0288): o poste de 30 cm parte com ~40 kJ (um carro de 1,5 t a ~26 km/h
	 *  já parte nas batidas reais); a palmeira de ~35 cm na areia, de raiz rasa, tomba com ~60 kJ (o momento crítico das árvores
	 *  arrancadas, ~140 N·m por kg de tronco, Peltola, e o giro até romper a raiz). O caminhão de 15 t derruba a palmeira acima
	 *  de 2,8 m/s e o poste acima de 2,3; o Tomahawk de 30 t, acima de 2,0 e 1,6. O tanque e o Dozer seguem a regra do ZH (o
	 *  CrusherLevel: passam por cima em qualquer velocidade).
	 *  (revisão de 23/10) A conta vale na velocidade de cruzeiro (MaxSpeed): ela diz QUEM derruba (o caminho abre a célula da
	 *  árvore pra ele), e esse derruba em qualquer toque andando ou empurrado, como o tanque, porque o motor que o leva a
	 *  cruzeiro empurra o tronco até cair; pela velocidade da hora, o toque lento deixava o casco atravessar o tronco de pé. */
	constexpr int64_t TreeToppleEnergyJ = 60000;
	constexpr int64_t PoleToppleEnergyJ = 40000;
	/** O veículo D a SpeedPerTick (m por passo) derruba o que absorve EnergyJ? Sem massa nos dados, nunca. */
	SIMCORE_API bool KnocksOver(const UnitDef& D, Fix SpeedPerTick, int64_t EnergyJ);

	struct PropSetup
	{
		PropKind Kind = PropKind::Car;
		Vec2 Pos;
		Angle Facing;            // o comprimento do carro vai neste rumo
		Fix HalfLength;          // o carro: meio comprimento; a árvore: o raio do tronco
		Fix HalfWidth;
		Fix Height;              // a árvore: até onde a queda chega
		uint8_t CrushableLevel = 1;   // esmaga quem tem CrusherLevel maior (o ZH)
	};

	struct PlayerSetup
	{
		uint8_t Team = 0;
		bool bIsAI = false;
		std::vector<StartingUnit> Units;
		/** (v4) A patente de largada (1..5; as cenas de teste): começa com o XP do limiar dela. */
		uint8_t StartRank = 1;
	};

	struct SIMCORE_API MatchSetup
	{
		uint64_t Seed = 1;
		SimMap Map;
		Ruleset Rules;
		std::vector<PlayerSetup> Players;
		/** Os depósitos de suprimento do mapa (vêm do mapa; hoje fixos no Maps.json). */
		std::vector<SupplyDockSetup> Docks;
		/** (16/10) Os carros e as árvores do mapa, em ordem fixa (o índice é o Id do objeto nos eventos). */
		std::vector<PropSetup> Props;
		/** Névoa de guerra: cada time só enxerga o que as unidades dele veem. */
		bool bFogOfWar = true;
		/** Aceita comandos de depuração (SetFog): sim no skirmish local, não no online. */
		bool bAllowDebugCommands = false;

		uint64_t Hash() const;
	};

	enum class OrderType : uint8_t
	{
		Idle,
		Move,
		Attack,
		AttackMove,
		AttackGround,   // atira no OrderPoint (o tiro forçado no chão)
	};

	/** O que o Dozer ou o caminhão está fazendo por conta própria (fora das ordens de andar e atacar). */
	enum class UnitTask : uint8_t
	{
		None,
		Construct,     // Dozer: indo até a obra ou construindo
		ToDock,        // caminhão: indo pra entrada do lado do depósito
		Loading,       // parado no depósito
		LeaveDock,     // carregado, saindo pela outra ponta do lado do depósito (as vagas do ZH)
		ToCenter,      // indo entregar
		Unloading,     // parado no centro de suprimentos
		WaitCenter,    // carregado, sem centro pra entregar
		WaitDock,      // sem depósito ao alcance: espera perto do centro e procura de novo (ZH)
		Repair,        // (v4, 23/10) Dozer: indo até o prédio avariado (TaskTarget) ou consertando (o DozerAIUpdate do ZH)
	};

	/** Por que o prédio não cabe ali (o fantasma vermelho do HUD diz o motivo). Regras do ZH (BuildAssistant). */
	enum class PlaceResult : uint8_t
	{
		Ok,
		NotAStructure,
		OutOfMap,
		NearEdge,      // perto demais da borda do mapa
		Blocked,       // penhasco, água, pedra, outro prédio ou depósito
		NotFlat,       // chão inclinado demais (o ZH exige terreno plano)
		NearSupplies,  // em cima da folga do depósito
		BlocksExit,    // na faixa de saída de uma fábrica (ou a saída dele cai em cima de algo)
		Occupied,      // tem unidade inimiga em cima
		NotVisible,    // o centro não está à vista agora (o ZH não constrói na névoa)
		MissingPrereq, // falta o prédio pré-requisito (ou a patente)
		NoMoney,
		LimitReached,  // (v4) o jogador já tem o teto desse tipo (MaxPerPlayer, fundações inclusive)
	};

	struct PlayerState
	{
		uint8_t Team = 0;
		bool bDefeated = false;
		/** O grupo de comando atual (SetGroup), em ordem de ID. */
		std::vector<EntityId> Group;
		int32_t Money = 0;
		/** Energia dos prédios prontos: gerada e gasta (recontada todo passo). */
		int32_t PowerMade = 0;
		int32_t PowerUsed = 0;
		/** Já teve um prédio que conta pra vitória (daí em diante, perde quem fica sem eles). */
		bool bHasBase = false;
		/** (v4, 22/10) A patente do general: o XP em milésimos (o crédito de cada acerto fica exato), a patente (1..5) e o
		 *  passo em que cada poder fica pronto de novo (0 = pronto assim que liga). */
		int64_t XpMilli = 0;
		uint8_t Rank = 1;
		TickNumber PowerReadyAt[MaxPowers] = {};
		bool IsLowPower() const { return PowerUsed > PowerMade; }
	};

	/** Leitura de um depósito de suprimentos. */
	struct DockView
	{
		Vec2 Pos;
		Fix HalfX;
		Fix HalfY;
		int32_t Boxes = 0;
		int32_t StartBoxes = 0;
		/** Pilha que um caminhão destruído soltou (some quando esvazia). */
		bool bPile = false;
		/** O caminhão sendo atendido agora (um por vez). */
		EntityId Occupant = NoEntity;
	};

	/** Leitura de uma unidade, pro desenho e pra IA. */
	struct UnitView
	{
		EntityId Id = NoEntity;
		PlayerIndex Owner = 0;
		uint16_t Def = 0;
		Vec2 Pos;
		Angle Hull;
		/** Direção da torre no mundo. */
		Angle Turret;
		Fix Speed;
		int32_t Hp = 0;
		int32_t MaxHp = 0;
		OrderType Order = OrderType::Idle;
		Vec2 OrderPoint;
		/** Em quem a torre está mirando agora (NoEntity = ninguém). */
		EntityId FireTarget = NoEntity;
		/** Atrás de quem a unidade está indo (ordem de ataque, ataque andando ou reação). */
		EntityId Engage = NoEntity;
		/** Pontos que faltam no caminho atual (0 = parada ou sem caminho). */
		int32_t PathLeft = 0;
		int32_t Cooldown = 0;

		// prédio
		bool bStructure = false;
		/** Obra: passos que faltam (0 = pronto). */
		int32_t BuildLeft = 0;
		int32_t BuildTicks = 0;
		/** Lado pra onde o prédio olha (0 +X, 1 +Y, 2 −X, 3 −Y). */
		int32_t Dir = 0;
		int32_t QueueSize = 0;
		/** O primeiro da fila: tipo e quanto já andou (0..1000). */
		int32_t QueueHead = -1;
		int32_t QueuePermille = 0;
		bool bHasRally = false;
		Vec2 Rally;
		// Dozer e caminhão
		UnitTask Task = UnitTask::None;
		EntityId TaskTarget = NoEntity;
		int32_t Cargo = 0;
		// (v4) a zona de lançamento: passos até o próximo pouso do avião e quantos pousos ela já recebeu (a rota que amadurece)
		int32_t IncomeLeft = 0;
		int32_t IncomeCount = 0;
		// (v4, 23/10) vender: os passos que faltam da desmontagem (0 = não está sendo vendido) e o total dela
		int32_t SellLeft = 0;
		int32_t SellTicks = 0;
		/** (v4) A mira antes do tiro (o lançador que levanta): os passos de mira, de 0 até o PreAttackTicks da arma; o desenho
		 *  levanta o tubo pelo mesmo número (em fase com o tiro, também na LAN). */
		int32_t AimTicks = 0;
	};

	class SIMCORE_API World
	{
	public:
		/** Falso se a montagem é inválida (unidade desconhecida, jogador demais, mapa vazio). */
		bool Init(const MatchSetup& Setup);
		/** Anda um passo. Commands = os comandos marcados para este passo, já em ordem (jogador, sequência). */
		void Step(const std::vector<Command>& Commands);

		TickNumber Tick() const { return CurrentTick; }
		uint64_t StateHash() const;
		const std::vector<SimEvent>& LastEvents() const { return Events; }

		int32_t UnitCount() const { return int32_t(U.Id.size()); }
		UnitView Unit(int32_t Index) const;

		/** Uma carcaça (13/10, o Felipe: "a carcaça tem física"): o veículo morto fica no mapa, e o vivo que encosta
		 *  empurra (ela pesa a metade dele) e é segurado por ela (anda mais devagar empurrando; contra o penhasco ou o
		 *  prédio ela trava). O empurrão fora do centro gira o casco. Id = o do veículo que morreu.
		 *  (22/10) Ela vive WreckLife sólida, depois afunda em SinkTicks passos (imóvel; o casco de colisão encolhe) e some
		 *  (os eventos WreckSinking e WreckRemoved). O desenho tira o quanto afundou daqui, sem relógio próprio: no passo
		 *  T da simulação o afundar está em (T − SinkStart) ÷ SinkTicks (o que vale também pausado e no replay). */
		struct WreckView
		{
			EntityId Id = NoEntity;
			uint16_t Def = 0;
			Vec2 Pos;
			Angle Hull;
			/** O passo em que virou carcaça e o em que começou a afundar (0 = sólida). */
			TickNumber Born = 0;
			TickNumber SinkStart = 0;
			int32_t SinkTicks = 1;
		};
		int32_t WreckCount() const { return int32_t(Wrecks.size()); }
		WreckView Wreck(int32_t Index) const
		{
			const WreckState& W = Wrecks[size_t(Index)];
			return WreckView{ W.Id, W.Def, W.Pos, W.Hull, W.Born, W.SinkStart, RulesData.Units[W.Def].WreckSinkTicks };
		}

		/** (16/10) Um tiro em voo, pro desenho seguir: o míssil (a Patriot) anda em 3D a cada passo (a altura Z acima do
		 *  zero do mapa, o rumo e a inclinação); o do tanque vai reto no plano. Id = o Value do WeaponFired e do ProjectileHit. */
		struct ProjectileView
		{
			uint32_t Id = 0;
			EntityId Shooter = NoEntity;
			EntityId Target = NoEntity;
			Vec2 Pos;
			Fix Z;
			Angle Yaw;
			Angle Pitch;
			Fix Speed;            // m por passo
			int32_t Age = 0;      // passos desde o disparo
			bool bMissile = false;
			/** (v4) De quem é a arma (o tipo do atirador, ou a munição do poder: o atirador pode já ter morrido, ou nem existir). */
			uint16_t Def = 0;
		};
		/** (16/10) Os carros e as árvores: o estado de cada um pro desenho (quem entra no meio, o replay, a névoa). */
		struct PropView
		{
			PropKind Kind = PropKind::Car;
			Vec2 Pos;
			Angle Facing;
			Fix HalfLength;
			Fix HalfWidth;
			Fix Height;
			/** O carro: as partes esmagadas (1 frente, 2 traseira); a árvore: 1 derrubada. 0 = de pé. */
			uint8_t State = 0;
			/** O lado da lagarta que passou por cima (o carro: 1 = o lado direito do tanque). */
			uint8_t Side = 0;
			Angle FallDir;
			Fix FallLength;
			TickNumber At = 0;
			EntityId By = NoEntity;
		};
		int32_t PropCount() const { return int32_t(PropDefs.size()); }
		PropView PropAt(int32_t Index) const
		{
			const PropSetup& D = PropDefs[size_t(Index)];
			const PropState& S = Props[size_t(Index)];
			return PropView{ D.Kind, D.Pos, D.Facing, D.HalfLength, D.HalfWidth, D.Height, S.State, S.Side, S.FallDir, S.FallLength, S.At, S.By };
		}
		int32_t ProjectileCount() const { return int32_t(Projectiles.size()); }
		ProjectileView ProjectileAt(int32_t Index) const
		{
			const Projectile& P = Projectiles[size_t(Index)];
			return ProjectileView{ P.Id, P.Shooter, P.Target, P.Pos, P.Z, P.Yaw, P.Pitch, P.Speed, P.Age, P.bMissile != 0, P.Def };
		}
		/** A altura do chão da simulação no ponto (m; o meio entre a menor e a maior da célula), zero sem alturas. O
		 *  desenho põe o míssil no chão de verdade somando a altura dele acima desta. */
		Fix GroundHeightAt(Vec2 P) const;
		bool FindUnit(EntityId Id, UnitView& Out) const;

		int32_t PlayerCount() const { return int32_t(Players.size()); }
		const PlayerState& Player(PlayerIndex P) const { return Players[P]; }
		bool AreEnemies(PlayerIndex A, PlayerIndex B) const { return Players[A].Team != Players[B].Team; }

		// --- (v4, 22/10) a patente e os poderes do general
		/** O XP inteiro do jogador (o crédito fica em milésimos; aqui, arredondado pra baixo). */
		int32_t Xp(PlayerIndex P) const { return int32_t(Players[P].XpMilli / 1000); }
		/** A patente do jogador (1..5). */
		int32_t Rank(PlayerIndex P) const { return Players[P].Rank; }
		/** O passo em que o poder K fica pronto de novo (0 = nunca usado: pronto assim que liga). */
		TickNumber PowerReadyAt(PlayerIndex P, int32_t K) const { return K >= 0 && K < MaxPowers ? Players[P].PowerReadyAt[K] : 0; }
		/** O jogador pode usar o poder K agora? O poder existe, a patente chegou, a espera acabou e ele tem um prédio pronto e
		 *  vivo que liga os poderes (o ponto, o comando confere à parte). */
		bool IsPowerReady(PlayerIndex P, int32_t K) const;
		/** O jogador tem um prédio pronto e vivo que liga os poderes (o Command Center)? */
		bool HasPowerGranter(PlayerIndex P) const;
		/** (v4) Um ataque de poder no ar (o anel vermelho do HUD): o dono, o poder, os mísseis (o total e os que já saíram), o
		 *  centro, o raio do girassol e o passo do próximo míssil. Some da lista quando o último sai (os mísseis seguem como
		 *  tiros: ProjectileAt, com Shooter = NoEntity). */
		struct StrikeView
		{
			PlayerIndex Owner = 0;
			int32_t Power = 0;
			int32_t Total = 0;
			int32_t Fired = 0;
			Vec2 Center;
			Fix Radius;
			TickNumber NextAt = 0;
		};
		int32_t StrikeCount() const { return int32_t(Strikes.size()); }
		StrikeView StrikeAt(int32_t Index) const
		{
			const Strike& S = Strikes[size_t(Index)];
			return StrikeView{ S.Owner, S.Power, S.Total, S.Fired, S.Center, S.Radius, S.NextAt };
		}
		/** O ponto de mira do míssil K (0..Total−1) do ataque Index: o girassol fixo, igual ao da simulação (o HUD marca os
		 *  pontos antes de os mísseis saírem). */
		Vec2 StrikeAimPoint(int32_t Index, int32_t K) const;

		bool IsMatchOver() const { return bMatchOver; }
		/** Time vencedor; −1 = empate ou partida em andamento. */
		int32_t WinningTeam() const { return Winner; }
		/** Passo em que a partida acabou (o relógio do resultado para aqui). */
		TickNumber EndTick() const { return MatchEndTick; }

		const SimMap& Map() const { return MapData; }
		const Ruleset& Rules() const { return RulesData; }

		// --- visão e névoa de guerra (a visão é por time: aliados enxergam juntos)
		/** Névoa ligada (vem da montagem; o comando SetFog troca no meio, pra depuração). */
		bool IsFogOfWar() const { return bFogOfWar; }
		uint8_t TeamOf(PlayerIndex P) const { return Players[P].Team; }
		/** A célula está à vista do time agora / já foi vista alguma vez (sem olhar o liga/desliga). */
		bool IsCellVisible(uint8_t Team, int32_t X, int32_t Y) const;
		bool IsCellExplored(uint8_t Team, int32_t X, int32_t Y) const;
		/** O jogador enxerga esse ponto? Com a névoa desligada, enxerga tudo. */
		bool IsVisibleTo(PlayerIndex P, Vec2 Pos) const;
		/** A visão do time pro desenho, célula a célula (Y·W + X): 0 nunca vista, 1 já vista, 2 à vista. */
		void CopyVisibility(uint8_t Team, std::vector<uint8_t>& Out) const;
		/** Sobe sempre que a visão do time muda (o desenho só refaz a textura quando muda). */
		uint32_t VisionVersion(uint8_t Team) const { return Vision.Version(Team); }
		/** Onde cada jogador começou (o centro das unidades iniciais): todo mundo conhece pelo mapa. */
		Vec2 PlayerStart(PlayerIndex P) const { return P < Starts.size() ? Starts[P] : Vec2::Zero(); }
		/** Refaz a contagem de visão do zero e compara com a incremental (teste: pega carimbo errado). */
		bool VisionSelfCheck() const;

		// --- economia e construção (ZH)
		int32_t Money(PlayerIndex P) const { return Players[P].Money; }
		/** A fila do prédio, em ordem (vazia se não for prédio). */
		void QueueOf(EntityId Structure, std::vector<uint16_t>& Out) const;
		int32_t DockCount() const { return int32_t(Docks.size()); }
		DockView Dock(int32_t Index) const;
		/** Onde o prédio fica de fato: o centro alinhado à grade (o prédio ocupa células inteiras). */
		Vec2 SnapStructure(uint16_t Def, int32_t Dir, Vec2 Center) const;
		/** As células do prédio com o centro já alinhado: [X0, X1) × [Y0, Y1). */
		void FootprintCells(uint16_t Def, int32_t Dir, Vec2 Center, int32_t& X0, int32_t& Y0, int32_t& X1, int32_t& Y1) const;
		/** O jogador pode pôr o prédio ali? (o HUD pinta o fantasma; o comando Build confere de novo). */
		PlaceResult CanPlace(PlayerIndex P, uint16_t Def, int32_t Dir, Vec2 Center) const;
		/** (v4, 23/10) A prévia da linha dos dois botões: as vagas que um Move/AttackMove com direção (MoveHasFacingOf(Value))
		 *  daria AGORA ao grupo Units do jogador P, pela mesma conta do comando (o filtro do SetGroup e do Move, o ClampToMap e o
		 *  LineSlots). Const: nada muda (o hash fica). OutIds e OutSlots na ordem do grupo (IDs crescentes); falso = o comando
		 *  não daria vaga a ninguém (sem direção, jogador caído, partida no fim, grupo sem unidade que anda). */
		bool PreviewLineSlots(PlayerIndex P, const std::vector<EntityId>& Units, Vec2 Point, int32_t Value,
			std::vector<EntityId>& OutIds, std::vector<Vec2>& OutSlots) const;
		/** O jogador tem os pré-requisitos do tipo (prédios prontos)? */
		bool HasPrerequisites(PlayerIndex P, uint16_t Def) const;
		/** (v4, 23/10) O jogador pode vender o prédio (dele, pronto, vivo e ainda não sendo vendido)? E quanto volta ($). */
		bool CanSell(PlayerIndex P, EntityId Structure) const;
		int32_t SellRefund(EntityId Structure) const;
		/** (v4) O Dozer do jogador pode consertar o prédio (dele ou do aliado, pronto, vivo, avariado, sem estar sendo vendido)? */
		bool CanRepair(PlayerIndex P, EntityId Structure) const;
		/** Ritmo da produção e da obra pela energia (ZH): 1000 = normal, 500 = metade. */
		int32_t PowerFactorPermille(PlayerIndex P) const;

	private:
		/** Estado das unidades em arrays paralelos (SoA), sempre em ordem crescente de ID. */
		struct UnitArrays
		{
			std::vector<EntityId> Id;
			std::vector<PlayerIndex> Owner;
			std::vector<uint16_t> Def;
			std::vector<Vec2> Pos;
			std::vector<Angle> Hull;
			std::vector<int32_t> TurretRel;      // torre relativa ao casco (bam, com sinal)
			std::vector<Fix> Speed;
			std::vector<int32_t> Hp;
			std::vector<int32_t> Cooldown;
			std::vector<int32_t> Clip;           // tiros que faltam na rajada (a arma com ClipSize)
			std::vector<OrderType> Order;
			std::vector<Vec2> OrderPoint;
			std::vector<EntityId> OrderTarget;   // alvo da ordem de ataque
			std::vector<uint8_t> ForcedAttack;   // o ataque é forçado (Ctrl): vale contra o próprio dono e aliado
			std::vector<EntityId> Engage;        // em quem a unidade está indo atrás (ordem ou reação)
			std::vector<EntityId> FireTarget;    // em quem a torre mira
			std::vector<EntityId> LastAttacker;
			std::vector<int32_t> TurretIdle;     // passos sem alvo (a torre volta pra frente)
			std::vector<std::vector<Vec2>> Path;
			std::vector<uint16_t> PathIndex;
			std::vector<Vec2> PathGoal;          // pra onde o caminho atual leva
			std::vector<uint8_t> NeedsPath;
			std::vector<int32_t> RepathWait;     // passos até poder refazer o caminho
			std::vector<int32_t> StuckTicks;
			std::vector<Vec2> StuckAnchor;
			std::vector<Vec2> EngageAnchor;      // onde a perseguição começou (a coleira mede daqui)
			std::vector<EntityId> Ignore;        // alvo inalcançável, ignorado até IgnoreUntil
			std::vector<TickNumber> IgnoreUntil;
			std::vector<int32_t> LookCell;       // onde o disco de visão está carimbado (−1 = nenhum)
			// economia e construção
			std::vector<int32_t> BuildLeft;      // obra: passos que faltam (0 = pronto)
			std::vector<EntityId> Builder;       // o Dozer que está construindo agora
			std::vector<std::vector<uint16_t>> Queue;   // fila de produção (tipos)
			std::vector<int32_t> QueueLeft;      // o primeiro da fila: progresso que falta (passos × 100)
			std::vector<Vec2> Rally;
			std::vector<uint8_t> HasRally;
			std::vector<UnitTask> Task;
			std::vector<EntityId> TaskTarget;    // a obra (Dozer) ou o centro de suprimentos (caminhão)
			std::vector<int32_t> TaskDock;       // o depósito do caminhão (−1 = nenhum)
			std::vector<int32_t> TaskTimer;
			std::vector<int32_t> Cargo;          // caixas que o caminhão leva
			std::vector<uint8_t> AutoGather;     // caminhão coleta sozinho (ordem de andar desliga; Gather liga)
			std::vector<uint8_t> Blocking;       // prédio: as células dele já estão bloqueadas na grade
			std::vector<Vec2> TaskPoint;         // caminhão: o ponto de encaixe atual (entrada, saída ou porta)
			std::vector<Angle> ArriveFacing;     // pra onde o casco vira ao chegar (formação, área de reunião)
			std::vector<uint8_t> HasArriveFacing;
			std::vector<Fix> SpeedCap;           // em formação (F): o passo do mais lento do grupo; zero = livre
			std::vector<int32_t> IdleTurret;     // pra onde a torre sem alvo olha (bam, relativa ao casco): o leque do grupo
			// (16/10) o freio do esmagar: passos que faltam, o total e o piso (milésimos da velocidade máxima)
			std::vector<int32_t> CrushSlowLeft;
			std::vector<int32_t> CrushSlowTotal;
			std::vector<int32_t> CrushSlowFloor;
			// (v4, 22/10) a zona de lançamento: passos até o próximo pouso, os pousos já recebidos (a rota que amadurece) e
			// os passos até a empilhadeira levar o próximo palete (0 = parada)
			std::vector<int32_t> IncomeLeft;
			std::vector<int32_t> IncomeCount;
			std::vector<int32_t> UnloadLeft;
			// (v4, 23/10) vender: os passos da desmontagem que faltam (0 = não; −1 = vendido, sai neste passo); o conserto do Dozer:
			// o resto da conta da vida (o passo não perde fração); a mira antes do tiro: os passos de mira e os sem alvo
			std::vector<int32_t> SellLeft;
			std::vector<int32_t> RepairAcc;
			std::vector<int32_t> AimTicks;
			std::vector<int32_t> AimIdle;

			void Resize(size_t N);
		};

		struct SupplyDock
		{
			Vec2 Pos;
			Fix HalfX;
			Fix HalfY;
			int32_t Boxes = 0;
			int32_t StartBoxes = 0;
			bool bPile = false;
			EntityId Occupant = NoEntity;
			/** Distância pelo caminho até o depósito, por célula (10 = uma célula reta; −1 = não chega). Só os do mapa. */
			std::vector<int32_t> Field;
		};

		struct Projectile
		{
			EntityId Shooter = NoEntity;
			EntityId Target = NoEntity;
			Vec2 Pos;
			Vec2 Aim;              // última posição conhecida do alvo
			Fix Step;              // m por passo; 0 = chega no passo seguinte
			int32_t Damage = 0;
			DamageType Type = DamageType::ArmorPiercing;
			// (16/10) o míssil: voa em 3D (Z em m, o rumo e a inclinação), acelera e guia
			uint32_t Id = 0;
			uint8_t bMissile = 0;
			Fix Z;
			Fix AimZ;
			Angle Yaw;
			Angle Pitch;
			Fix Speed;
			int32_t Age = 0;
			uint16_t Def = 0;      // a arma de quem atirou (o atirador pode morrer com o míssil no ar)
			/** (v4) O dono do tiro: o XP vai pra ele mesmo com o atirador já morto (e o míssil do poder, que não tem atirador). */
			PlayerIndex Owner = NoPlayer;
		};
		/** O próximo Id de tiro (nunca reusado). */
		uint32_t NextProjectileId = 1;
		/** O voo de um míssil num passo; falso = explodiu (o acerto ou o chão já foram contados). */
		bool FlyMissile(Projectile& P);
		/** (v4) O dano em área da arma W explodindo em At: cada inimigo de SourcePlayer a até W.Splash (da borda do casco ou do
		 *  lote) leva Damage × a queda (cheio até SplashFull, em linha reta até SplashEdgePct % em Splash); Exclude (o alvo do
		 *  acerto direto, que já levou o cheio) fica de fora. */
		void ApplySplash(Vec2 At, const Projectile& P, EntityId Exclude);

		// (16/10) o esmagar e o derrubar
		struct PropState
		{
			uint8_t State = 0;
			uint8_t Side = 0;
			Angle FallDir;
			Fix FallLength;
			TickNumber At = 0;
			EntityId By = NoEntity;
		};
		std::vector<PropSetup> PropDefs;
		std::vector<PropState> Props;
		/** As células de cada objeto (o carro: o centro dentro da caixa + 0,5 m; a árvore: a do tronco). */
		std::vector<std::vector<int32_t>> PropCells;
		/** Objetos de pé em cada célula (a célula esmagável abre quando chega a 0); (v4) e os carros entre eles (com carro de pé a
		 *  célula é a do carro, só de quem esmaga; só com árvore, a da árvore, também de quem derruba pelo choque). */
		std::vector<uint8_t> PropStanding;
		std::vector<uint8_t> PropStandingCars;
		/** Onde cada unidade estava antes de andar no passo (o contato só vale com o tanque se mexendo). */
		std::vector<Vec2> PrePos;
		std::vector<Angle> PreHull;
		/** As regiões ligadas ficaram velhas (uma célula esmagável abriu): refeitas no máximo a cada RegionRelabelTicks. */
		bool bRegionsStale = false;
		TickNumber RegionsLabeledAt = 0;
		/** A mobilidade da unidade: a folga dela, com o bit de quem esmaga (o carro e a árvore). */
		int32_t MobilityOf(int32_t Index) const;
		int32_t MobilityOfDef(const UnitDef& D) const;
		/** A do grupo: a maior folga, e o bit só se todos esmagam (o caminhão no meio dos tanques não passa no carro). */
		int32_t GroupMobility(const std::vector<int32_t>& Members) const;
		void InitProps(const std::vector<PropSetup>& Setup);
		void UpdateCrush();
		/** A célula esmagável do objeto Prop volta a ser só terreno (o carro achatou, a árvore caiu). */
		void ReleaseProp(int32_t Prop);
		/** O rumo de queda livre de prédio a partir de Want (±22,5° ... ±90°) e até onde ela vai. */
		Angle FreeFall(Vec2 From, Angle Want, Fix Height, Fix& OutLength) const;
		void RelabelRegions();

		struct DamageIntent
		{
			EntityId Source = NoEntity;
			EntityId Target = NoEntity;
			int32_t Amount = 0;
			DamageType Type = DamageType::ArmorPiercing;
			/** (v4) O dono de quem causou (o crédito de XP); NoPlayer = ninguém. */
			PlayerIndex SourcePlayer = NoPlayer;
		};

		/** (v4, 22/10) Um ataque de poder em andamento (o Cruise Missile Strike): Total mísseis, um a cada IntervalTicks a partir
		 *  de NextAt, vindo da direção From (unitária: de quem atirou pro ponto), em volta de Center no raio Radius; Phase gira o
		 *  girassol dos pontos de mira (sai do passo do uso, sem sorteio). */
		struct Strike
		{
			PlayerIndex Owner = 0;
			uint8_t Power = 0;
			int32_t Total = 0;
			int32_t Fired = 0;
			Vec2 Center;
			Vec2 From;
			Fix Radius;
			uint16_t Phase = 0;
			TickNumber NextAt = 0;
		};
		/** Os ataques no ar, na ordem em que foram usados (entram no hash). */
		std::vector<Strike> Strikes;

		// sistemas, na ordem do passo
		void ApplyCommand(const Command& C);
		void BuildBins();
		void UpdateTargets();
		void UpdatePaths();
		void UpdateMovement();
		/** (v4) Atirando parado (o alvo na torre ou o tiro no chão, sem andar): não cede o lugar pro aliado no SeparateUnits. */
		bool HoldsGround(int32_t I) const;
		void SeparateUnits();
		/** (22/10) As carcaças envelhecem (antes do empurrar): o afundar que acabou sai do mapa; a vida que acabou começa a
		 *  afundar; com sólidas demais, as mais velhas (de pelo menos 12 s) afundam já. Sempre na ordem de nascimento. */
		void AgeWrecks();
		/** O vivo empurra a carcaça e ela segura o vivo (depois do separar). */
		void PushWrecks();
		void UpdateTurrets();
		void UpdateWeapons();
		void UpdateProjectiles();
		void ResolveDamage();
		/** (v4) Quem passou do limiar da próxima patente sobe (uma vez em cada), com o evento RankUp. */
		void CheckRanks();
		void RemoveDead();
		void UpdateVision();
		void CheckVictory();
		// economia e construção (WorldEconomy.cpp)
		void ApplyEconomyCommand(const Command& C, const std::vector<int32_t>& Members);
		void UpdatePower();
		void UpdateConstruction();
		void UpdateProduction();
		void UpdateSupply();
		/** (v4) O avião de carga das zonas de lançamento e a empilhadeira (WorldEconomy.cpp; logo depois do UpdateSupply). */
		void UpdateAirlift();
		/** (v4, 23/10) O conserto do Dozer (o DozerAIUpdate do ZH) e a venda andando (WorldEconomy.cpp; logo depois do avião). */
		void UpdateRepair();
		void UpdateSell();
		/** O prédio está sendo vendido (ou acabou de ser): pra simulação ele já saiu (energia, fila, defesa, poderes, pré-requisito). */
		bool IsSelling(int32_t Index) const { return U.SellLeft[Index] != 0; }
		// (v4) os poderes do general (WorldPowers.cpp)
		/** O comando UsePower: confere tudo e, valendo, põe o ataque no ar (a espera começa aqui). */
		void ApplyPowerCommand(const Command& C);
		/** Os mísseis dos ataques em andamento saem na hora deles (logo depois do UpdateWeapons). */
		void UpdatePowers();
		/** Põe o prédio no mapa: as células dele viram bloqueio e os caminhos são refeitos. */
		EntityId PlaceStructure(PlayerIndex Owner, uint16_t Def, int32_t Dir, Vec2 Center, bool bComplete);
		/** Marca (ou libera) as células do prédio na grade e recalcula a folga. */
		void SetFootprint(int32_t Index, bool bBlock);
		void BlockRect(int32_t X0, int32_t Y0, int32_t X1, int32_t Y1, bool bBlock);
		/** Meio lado do prédio no mundo, com o giro (o lado 1 e 3 trocam X e Y). */
		void HalfExtents(uint16_t Def, int32_t Dir, Fix& OutX, Fix& OutY) const;
		int32_t DirOf(int32_t Index) const { return int32_t(((U.Hull[Index].Bam + Angle::QuarterTurn / 2) & 0xFFFF) / Angle::QuarterTurn) & 3; }
		/** Distância do ponto até a borda do retângulo (0 dentro dele). */
		static Fix RectDistance(Vec2 P, Vec2 Center, Fix HalfX, Fix HalfY);
		/** Onde a unidade nova sai do prédio (a frente dele, em chão andável). */
		Vec2 ExitPoint(int32_t Structure, uint16_t UnitDefIndex) const;
		/** Termina a obra: energia passa a contar e as unidades grátis nascem. */
		void CompleteStructure(int32_t Index);
		/** Unidade nova saindo do prédio, já indo pro ponto de encontro. */
		EntityId SpawnFrom(int32_t Structure, uint16_t UnitDefIndex);
		/** O lugar livre mais perto de P pra unidade parar (ZH Pathfinder::adjustDestination): espiral
		 *  pelas células, longe dos amigos parados ali ou indo pra lá. Sem isso, os tanques da fábrica
		 *  paravam em fila indiana no ponto de encontro (o "desfile", 11/10). */
		Vec2 FreeSpotNear(int32_t Unit, Vec2 P) const;
		/** Uma vaga longe dos amigos parados ali ou indo pra lá, com chão andável pra unidade. */
		bool SpotFree(int32_t Unit, Vec2 Q) const;
		/** A área de reunião da fábrica (estudo de 11/10: SupCom, BAR, 0 A.D.): fileiras escalonadas
		 *  de 4 viradas pra longe da fábrica, cada vaga com um tremido fixo de ±0,5 m e ±4°; o tanque
		 *  pega a primeira livre. Devolve a vaga e a direção de chegada. */
		Vec2 StagingSlot(int32_t Structure, int32_t Unit, Vec2 Rally, Angle& OutFacing, int32_t* OutLateralHalf = nullptr) const;
		/** O leque das torres de um grupo que vai pras vagas Slots, marchando na direção March. */
		void FanTurrets(const std::vector<int32_t>& Members, const std::vector<Vec2>& Slots, Angle March);
		bool IsComplete(int32_t Index) const { return U.BuildLeft[Index] == 0; }
		/** A obra já pode começar a ocupar a grade? Se tem unidade em cima, manda as amigas saírem. */
		bool ClaimFootprint(int32_t Structure);
		/** O ponto logo fora do lote, pelo lado mais perto de P (o Dozer que estava em cima sai por ali). */
		Vec2 LotExit(Vec2 P, Vec2 Center, Fix HX, Fix HY, Fix Margin, int32_t MinClearance) const;
		/** O retângulo da faixa de saída da fábrica (centro e meio lado); falso se não é fábrica. */
		bool ExitLaneRect(uint16_t Def, int32_t Dir, Vec2 Center, Vec2& OutCenter, Fix& OutHalfX, Fix& OutHalfY) const;
		/** O caminho até o depósito, calculado uma vez na montagem (o terreno não muda). */
		void BuildDockField(SupplyDock& Dock) const;
		/** Distância pelo caminho do ponto até o depósito (m); negativo se não chega. A pilha mede em linha reta. */
		Fix DockDistance(int32_t Dock, Vec2 From) const;
		/** O lado do depósito por onde o caminhão passa: entra numa ponta, carrega, sai pela outra. */
		void DockLane(int32_t Dock, int32_t Truck, Vec2& OutEnter, Vec2& OutExit) const;
		/** A porta do centro de suprimentos (a frente dele, por fora). */
		Vec2 CenterDoor(int32_t Center, int32_t Truck) const;
		/** O caminhão destruído solta a carga no chão. */
		void DropCargo(Vec2 At, int32_t Boxes);
		int32_t NearestDock(Vec2 From) const;
		int32_t NearestSupplyCenter(PlayerIndex Owner, Vec2 From) const;

		// ajudantes
		EntityId Spawn(PlayerIndex Owner, uint16_t DefIndex, Vec2 Pos, Angle Facing);
		int32_t IndexOf(EntityId Id) const;
		bool IsAliveEnemy(int32_t Self, EntityId Other) const;
		bool IsAliveUnit(EntityId Other) const;
		const UnitDef& DefOf(int32_t Index) const { return RulesData.Units[U.Def[Index]]; }
		Angle TurretWorld(int32_t Index) const { return Angle::FromBam(int32_t(U.Hull[Index].Bam) + U.TurretRel[Index]); }
		/** Depois de um tiro: a espera do próximo (com a rajada, o último da rajada espera a recarga). */
		void AfterShot(int32_t I, const WeaponDef& W);
		EntityId NearestEnemy(int32_t Self, Fix Radius, bool bSkipDoomed = false) const;
		/** (16/10) Os tiros já no ar vão matar o alvo (a vida ≤ o dano a caminho, com a armadura)? */
		bool IsDoomed(int32_t Target) const;
		/** Começa a perseguir Target (guarda de onde saiu, pra coleira). */
		void StartEngage(int32_t Index, EntityId Target);
		Vec2 ClampToMap(Vec2 P) const;
		void GiveMove(int32_t Index, Vec2 Point, OrderType Order);
		/** As vagas de destino de um grupo, uma por membro (na ordem de Members). */
		/** Desfaz cruzamentos entre os caminhos até as vagas (troca dois de vaga quando a soma das distâncias cai). */
		void Uncross(const std::vector<int32_t>& Members, std::vector<Vec2>& Slots) const;
		void GroupSlots(const std::vector<int32_t>& Members, Vec2 Point, std::vector<Vec2>& Out);
		/** Em formação: cada um guarda o lugar que tinha no grupo (o desenho só anda), apertado se o grupo
		 *  estiver espalhado demais. */
		void FormationSlots(const std::vector<int32_t>& Members, Vec2 Point, std::vector<Vec2>& Out);
		/** O arraste com direção: uma linha de frente virada pra Facing, a fileira da frente no ponto, as outras atrás;
		 *  Width = a largura da fileira (zero = a do grupo). */
		void LineSlots(const std::vector<int32_t>& Members, Vec2 Point, Angle Facing, Fix Width, std::vector<Vec2>& Out) const;
		/** Região ligada da célula C pra uma folga (0 = não andável, ou folga sem mapa de regiões). */
		int32_t RegionAt(int32_t Clearance, Cell C) const;
		/** P se ele já está na região; senão a célula da região mais perto; senão Fallback. */
		Vec2 IntoRegion(int32_t Clearance, int32_t Region, Vec2 P, Vec2 Fallback) const;
		/** Pra onde a unidade quer ir neste passo; falso = ficar parada. */
		bool MovementGoal(int32_t Index, Vec2& OutGoal, Fix& OutStopDistance) const;

		uint64_t ComputeHash() const;

		UnitArrays U;
		std::vector<int32_t> IdToIndex;     // −1 = morta ou nunca existiu
		EntityId NextId = 1;
		std::vector<PlayerState> Players;
		SimMap MapData;
		Ruleset RulesData;
		Rng Random;
		Pathfinder Paths;
		TickNumber CurrentTick = 0;
		bool bMatchOver = false;
		int32_t Winner = -1;
		TickNumber MatchEndTick = 0;
		/** Onde a cota de caminhos do passo começa (dá a volta: ninguém fica sempre por último). */
		EntityId PathCursor = NoEntity;
		std::vector<SimEvent> Events;
		std::vector<Projectile> Projectiles;
		std::vector<DamageIntent> PendingDamage;
		/** O maior raio entre as unidades que andam (a visão e a separação medem por ele). */
		Fix MaxUnitRadius;
		/** O maior raio entre todos os objetos, prédios inclusive (as buscas de alvo medem por ele). */
		Fix MaxBodyRadius;
		/** A passagem do terreno sem prédios (o prédio que some devolve a célula como era). */
		std::vector<uint8_t> TerrainPass;
		std::vector<SupplyDock> Docks;
		/** Algum prédio mudou a grade neste passo: quem tem caminho refaz. */
		bool bGridChanged = false;
		uint64_t CachedHash = 0;

		// visão: a grade sai das células carimbadas (LookCell) e dos olhos que ficam (Lingering),
		// que entram no hash; o explorado só o desenho lê
		VisionGrid Vision;
		bool bFogOfWar = true;
		bool bAllowDebug = false;
		std::vector<Vec2> Starts;
		/** Olho que fica um tempo: o de quem morreu (a visão não some no mesmo quadro: SC2) e o
		 *  do tiro vindo da névoa (quem atirou aparece um instante pro time de quem levou). */
		struct LingeringLook
		{
			uint8_t Team = 0;
			int32_t Cell = -1;
			int32_t RadiusCells = 0;
			int64_t RadiusSq = 0;
			TickNumber Until = 0;
		};
		std::vector<LingeringLook> Lingering;

		struct WreckState
		{
			EntityId Id = NoEntity;
			uint16_t Def = 0;
			Vec2 Pos;
			Angle Hull;
			/** Travada (o empurrão bateu no penhasco ou no prédio): passos até tentar de novo; travada, o vivo leva o
			 *  empurrão inteiro. */
			uint8_t Stuck = 0;
			/** (22/10) O passo em que nasceu e o em que começou a afundar (0 = sólida: o afundar nunca começa no passo 0, porque a
			 *  carcaça nasce no fim de um passo e só envelhece do seguinte em diante). */
			TickNumber Born = 0;
			TickNumber SinkStart = 0;
		};
		/** Em ordem de nascimento (a mais velha primeiro): os tetos e o hash dependem dessa ordem. */
		std::vector<WreckState> Wrecks;
		/** A carcaça K começa a afundar neste passo (Reason: 0 a vida acabou, 1 sólidas demais), com o evento. */
		void SinkWreck(size_t K, int32_t Reason);
		/** A carcaça K sai do mapa (Reason: 0 afundou inteira, 1 o teto de todas), com o evento; as outras ficam na ordem. */
		void EraseWreck(size_t K, int32_t Reason);
		/** O quanto a carcaça já afundou neste passo: 0 sólida … 1. */
		Fix WreckSinkProgress(const WreckState& Wr) const;
		/** Raio de visão do tipo, em células (teto e quadrado). */
		void SightCells(uint16_t Def, int32_t& OutRadius, int64_t& OutRadiusSq) const;
		void LookAt(uint8_t Team, uint16_t Def, int32_t Cell, int32_t Delta);
		void AddLingering(uint8_t Team, int32_t Cell, int32_t RadiusCells, int64_t RadiusSq, TickNumber Ticks);
		/** A unidade Self (o time dela) vê a unidade J? Com a névoa desligada, sim. */
		bool SeesUnit(int32_t Self, int32_t J) const;
		/** Regiões ligadas do mapa, uma grade por folga das unidades (sai do mapa: não entra no hash). */
		std::vector<int32_t> RegionClearances;
		std::vector<std::vector<int32_t>> RegionLabels;

		// grade de vizinhança (reconstruída quando as posições mudam)
		Fix BinSize = Fix::FromInt(16);
		int32_t BinsW = 0;
		int32_t BinsH = 0;
		std::vector<int32_t> BinStart;      // BinsW·BinsH + 1
		std::vector<int32_t> BinUnits;      // índices de unidade, por célula, em ordem de índice
		template <typename Fn> void ForEachNear(Vec2 P, Fix Radius, Fn&& Visit) const;
	};
}
