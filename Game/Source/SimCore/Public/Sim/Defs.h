#pragma once

#include "Sim/SimApi.h"

// Dados de design das unidades, dos prédios e das armas.
//
// Os números entram em unidades de gente (m/s, graus/s, milissegundos), como
// inteiros em milésimos (UnitSpec). Na carga viram valores por passo (UnitDef),
// que é o que a simulação lê. Nenhum float no caminho.

#include <cstdint>
#include <string>
#include <vector>

#include "Sim/FixedMath.h"
#include "Sim/Types.h"

namespace Sim
{
	enum class DamageType : uint8_t
	{
		ArmorPiercing,
		SmallArms,
		Explosive,
		Count
	};

	enum class ArmorType : uint8_t
	{
		Tank,
		Structure,
		Count
	};

	/** Tudo é objeto, como no ZH: a unidade anda; o prédio fica parado e ocupa células da grade. */
	enum class ObjectKind : uint8_t
	{
		Unit,
		Structure,
		Count
	};

	/** Arma em unidades de gente, inteiros em milésimos. */
	struct WeaponSpec
	{
		int32_t RangeMm = 0;               // alcance (mm)
		int32_t Damage = 0;                // pontos de dano por tiro
		DamageType Type = DamageType::ArmorPiercing;
		int32_t ReloadMs = 0;              // entre tiros
		int32_t AimToleranceMilliDeg = 0;  // a torre atira se estiver a até isto do alvo
		int32_t ProjectileSpeedMmPerS = 0; // 0 = acerta no passo seguinte
		int32_t ClipSize = 0;              // tiros por rajada (ClipSize do ZH; 0 = sem rajada, todo tiro espera o ReloadMs)
		int32_t ClipReloadMs = 0;          // depois do último tiro da rajada (ClipReloadTime)
		// (16/10) o míssil (a Patriot): sai inclinado da caixa, acelera até o ProjectileSpeed, faz a curva atrás do
		// ponto onde o alvo vai estar e explode no contato. LaunchSpeed 0 = projétil reto (o tiro do tanque)
		int32_t MissileLaunchSpeedMmPerS = 0;
		int32_t MissileAccelMmPerS2 = 0;
		int32_t MissileTurnMilliDegPerS = 0;  // giro máximo do rumo e da inclinação
		int32_t MissileBoostMs = 0;           // sai reto da caixa antes de começar a guiar
		int32_t MissileLaunchPitchMilliDeg = 0;
		int32_t MissileLaunchHeightMm = 0;    // a boca da caixa acima do chão
		int32_t MissileSpreadMilliDeg = 0;    // o leque das caixas da rajada (o rumo de cada uma, ± isto)
		int32_t MissileLifeMs = 0;            // depois disto ele explode onde estiver
		// (v4, 22/10) o alcance mínimo da artilharia (o Tomahawk: 20 m): não mira nem atira mais perto que isto; 0 = sem
		int32_t MinRangeMm = 0;
		// (v4) o dano em área, medido da borda do casco ou do lote: cheio até SplashFull, cai em linha reta até SplashEdgePct
		// % em Splash, zero além. Splash 0 = o tiro só acerta o alvo (como antes)
		int32_t SplashFullMm = 0;
		int32_t SplashMm = 0;
		int32_t SplashEdgePct = 0;
		// (v4, 23/10) a mira antes do tiro (o AIM do TurretAI com o PreAttackDelay do ZH; o lançador que levanta o tubo): o relógio
		// conta desde que há alvo no alcance, e o tiro só sai alinhado e com o relógio em PreAttack; 0 = atira assim que alinha
		int32_t PreAttackMs = 0;
		// (v4) a boca do tubo: o pivô da torre no casco (x pra frente, y pro lado; mm) e, dele, a boca na torre (pra frente, pro
		// lado, alternando os tubos, e a altura do chão; mm). LaunchUp 0 = o míssil sai como antes (as caixas da Patriot)
		int32_t LaunchPivotXMm = 0;
		int32_t LaunchPivotYMm = 0;
		int32_t LaunchForwardMm = 0;
		int32_t LaunchSideMm = 0;
		int32_t LaunchUpMm = 0;
	};

	/** Unidade ou prédio em unidades de gente, inteiros em milésimos. */
	struct UnitSpec
	{
		std::string Key;                   // nome estável ("usa_mbt"): os dados e o desenho se acham por ele
		int32_t MaxHp = 0;
		ArmorType Armor = ArmorType::Tank;
		int32_t RadiusMm = 0;              // raio de colisão entre unidades (prédio: 0 = sai do tamanho)
		int32_t PathRadiusMm = 0;          // meia largura contra o terreno (passa em vão mais estreito que o raio)
		int32_t SpeedMmPerS = 0;
		int32_t AccelMmPerS2 = 0;
		int32_t BrakeMmPerS2 = 0;
		int32_t TurnMilliDegPerS = 0;      // giro do casco
		int32_t TurretTurnMilliDegPerS = 0;
		int32_t TurretIdleReturnMs = 0;    // parada sem alvo, a torre volta pra frente depois disto
		int32_t SightMm = 0;               // raio em que ela procura inimigo sozinha
		WeaponSpec Weapon;                 // dano 0 = desarmada (Dozer, caminhão, prédio)

		// produção e economia (números do ZH 1.04, em docs/JOGO.md)
		ObjectKind Kind = ObjectKind::Unit;
		int32_t Cost = 0;                  // $ (BuildCost)
		int32_t BuildTimeMs = 0;           // treinar ou construir (BuildTime)
		int32_t Energy = 0;                // prédio pronto: + gera, − consome (EnergyProduction)
		int32_t SizeXMm = 0;               // prédio: lados no chão, virado pra +X (ocupa células inteiras)
		int32_t SizeYMm = 0;
		std::vector<std::string> Produces;   // prédio: o que treina (CommandSet)
		std::vector<std::string> Constructs; // unidade: os prédios que constrói (o Dozer)
		std::vector<std::string> FreeUnits;  // nascem quando o prédio fica pronto (OCL do ZH)
		int32_t SupplyBoxes = 0;           // caminhão: caixas por viagem
		bool bSupplyCenter = false;        // prédio que recebe as caixas
		std::vector<std::string> Requires;   // prédios prontos que o jogador precisa ter (Prerequisites)
		bool bCountsForVictory = false;    // prédio que segura o jogador vivo (MP_COUNT_FOR_VICTORY)
		int32_t ExitLaneMm = 0;            // fábrica: faixa livre na frente da porta (FactoryExitWidth)
		bool bNeedsPower = false;          // sem energia fica parado (KindOf POWERED do ZH: a Patriot, o radar)
		// (16/10) o esmagar do ZH (CrusherLevel): 2 esmaga o carro e derruba a árvore (o tanque, o Dozer); 1 só o que for 0
		int32_t CrusherLevel = 0;
		int32_t HullHalfLengthMm = 0;      // a caixa do casco (0 = sai do raio: 1,08 × e 0,51 ×)
		int32_t HullHalfWidthMm = 0;
		// (16/10) a ajuda entre as defesas (o RequestAssistRange do ZH): as do mesmo tipo a até isto passam a mirar no alvo
		// de quem atirou; 0 = sem ajuda
		int32_t AssistRangeMm = 0;
		// (22/10) a carcaça do veículo: fica sólida (empurrável) por WreckLife, depois afunda em WreckSink e some (o hulk do
		// ZH: SinkDelay e DestructionDelay). 48 s = o fogo da carcaça (4 + 30 s) e ~10 s de casco frio; 7 s = o afundar do
		// ZH. Life 0 = só os tetos da partida tiram
		int32_t WreckLifeMs = 48000;
		int32_t WreckSinkMs = 7000;
		// (v4, 22/10) a patente do general: o tipo só sai (produção e obra) com o dono nesta patente; 0 = sem exigência
		int32_t RequiresRank = 0;
		// quantos o jogador pode ter de uma vez, fundações inclusive (a zona de lançamento: 2); 0 = sem teto
		int32_t MaxPerPlayer = 0;
		// o prédio pronto e vivo liga os poderes do general (o Command Center)
		bool bGrantsPowers = false;
		// (v4) o avião de carga da zona de lançamento: o 1º pouso First depois de pronta, depois um a cada Period; o aviso
		// (o avião aparece) Lead antes de cada pouso; o pouso n traz min(PalletsMin + n, PalletsMax) paletes de BoxesPerPallet
		// caixas no pátio, e a empilhadeira leva um palete pro banco a cada UnloadPerPallet. Period 0 = sem avião
		int32_t AirliftFirstMs = 0;
		int32_t AirliftPeriodMs = 0;
		int32_t AirliftLeadMs = 0;
		int32_t AirliftPalletsMin = 0;
		int32_t AirliftPalletsMax = 0;
		int32_t AirliftBoxesPerPallet = 0;
		int32_t AirliftUnloadMsPerPallet = 0;
		// (v4, 23/10) o conserto do Dozer (o RepairHealthPercentPerSecond do DozerAIUpdate do ZH: 2%): milésimos de % da vida cheia
		// do prédio por segundo; 0 = não conserta
		int32_t RepairMilliPctPerS = 0;
		// (v4) a massa do veículo (kg; o "mass_t" do visual): o choque que derruba a palmeira e o poste (½·m·v², KnocksOver)
		int32_t MassKg = 0;
	};

	/** Arma em valores por passo: o que a simulação usa. */
	struct WeaponDef
	{
		Fix Range;
		int32_t Damage = 0;
		DamageType Type = DamageType::ArmorPiercing;
		int32_t ReloadTicks = 0;
		int32_t AimToleranceBam = 0;
		Fix ProjectileStep;                // metros por passo; 0 = acerta no passo seguinte
		int32_t ClipSize = 0;
		int32_t ClipReloadTicks = 0;
		// o míssil (zero = projétil reto)
		Fix MissileLaunchStep;             // m por passo
		Fix MissileAccel;                  // m por passo, por passo
		int32_t MissileTurnBam = 0;        // bam por passo
		int32_t MissileBoostTicks = 0;
		Angle MissileLaunchPitch;
		Fix MissileLaunchHeight;
		int32_t MissileSpreadBam = 0;
		int32_t MissileLifeTicks = 0;
		// (v4) o alcance mínimo e o dano em área (zero = desligados)
		Fix MinRange;
		Fix SplashFull;
		Fix Splash;
		int32_t SplashEdgePct = 0;
		/** (v4, 23/10) A mira antes do tiro, em passos (0 = sem), e a boca do tubo: o pivô da torre no casco e a boca a partir dele. */
		int32_t PreAttackTicks = 0;
		Vec2 LaunchPivot;
		Fix LaunchForward;
		Fix LaunchSide;
		Fix LaunchUp;

		bool IsMissile() const { return MissileLaunchStep.Raw > 0; }
		bool HasSplash() const { return Splash.Raw > 0; }
		bool HasLaunchMouth() const { return LaunchUp.Raw > 0; }
	};

	struct SIMCORE_API UnitDef
	{
		std::string Key;
		int32_t MaxHp = 0;
		ArmorType Armor = ArmorType::Tank;
		Fix Radius;
		Fix PathRadius;
		Fix MaxSpeed;                      // m por passo
		Fix Accel;                         // m por passo, por passo
		Fix Brake;
		int32_t TurnBam = 0;               // bam por passo
		int32_t TurretTurnBam = 0;
		int32_t TurretIdleReturnTicks = 0;
		Fix Sight;
		WeaponDef Weapon;

		ObjectKind Kind = ObjectKind::Unit;
		int32_t Cost = 0;
		int32_t BuildTicks = 0;
		int32_t Energy = 0;
		/** Prédio virado pra +X: meio lado em X e em Y (as células que ocupa saem daqui). */
		Fix HalfX;
		Fix HalfY;
		std::vector<uint16_t> Produces;
		std::vector<uint16_t> Constructs;
		std::vector<uint16_t> FreeUnits;
		int32_t SupplyBoxes = 0;
		bool bSupplyCenter = false;
		std::vector<uint16_t> Requires;
		bool bCountsForVictory = false;
		Fix ExitLane;
		bool bNeedsPower = false;
		int32_t CrusherLevel = 0;
		Fix HullHalfLength;
		Fix HullHalfWidth;
		Fix AssistRange;
		/** A carcaça: passos sólida (0 = só os tetos tiram) e passos afundando (pelo menos 1). */
		int32_t WreckLifeTicks = 0;
		int32_t WreckSinkTicks = 1;
		/** (v4) A patente que o tipo exige (0 = nenhuma), o teto por jogador (0 = sem) e se liga os poderes do general. */
		int32_t RequiresRank = 0;
		int32_t MaxPerPlayer = 0;
		bool bGrantsPowers = false;
		/** (v4) O avião de carga, em passos (AirliftPeriodTicks 0 = sem avião): o 1º pouso, o intervalo, o aviso antes do
		 *  pouso, os paletes (o mínimo e o máximo da rota que amadurece), as caixas por palete e a empilhadeira por palete. */
		int32_t AirliftFirstTicks = 0;
		int32_t AirliftPeriodTicks = 0;
		int32_t AirliftLeadTicks = 0;
		int32_t AirliftPalletsMin = 0;
		int32_t AirliftPalletsMax = 0;
		int32_t AirliftBoxesPerPallet = 0;
		int32_t AirliftUnloadTicks = 0;
		/** (v4, 23/10) O conserto (milésimos de % da vida cheia do prédio por segundo; 0 = não conserta) e a massa (kg). */
		int32_t RepairMilliPctPerS = 0;
		int32_t MassKg = 0;

		bool IsStructure() const { return Kind == ObjectKind::Structure; }
		bool HasAirlift() const { return AirliftPeriodTicks > 0; }
		bool HasWeapon() const { return Weapon.Damage > 0 && Weapon.Range.Raw > 0; }
		bool CanConstruct() const { return !Constructs.empty(); }
		bool CanRepair() const { return RepairMilliPctPerS > 0 && !IsStructure(); }

		/** Converte os números. As listas (Produces...) ficam vazias: quem resolve as chaves é o Ruleset::Build. */
		static UnitDef FromSpec(const UnitSpec& S);
	};

	/** Porcentagem de dano que cada tipo de dano causa em cada armadura (100 = cheio). */
	struct ArmorTable
	{
		int32_t Percent[int(ArmorType::Count)][int(DamageType::Count)] = {};

		int32_t Get(ArmorType A, DamageType D) const { return Percent[int(A)][int(D)]; }
		void Set(ArmorType A, DamageType D, int32_t P) { Percent[int(A)][int(D)] = P; }
	};

	/** As regras da economia (ZH 1.04: GameData.ini, SupplyTruckAIUpdate, ProductionUpdate, BuildAssistant). */
	struct EconomyRules
	{
		int32_t StartingMoney = 0;
		int32_t BoxValue = 0;              // $ por caixa entregue
		int32_t TicksPerBox = 0;           // no depósito: espera antes de cada caixa (SupplyWarehouseActionDelay)
		int32_t CenterTicks = 0;           // no centro: espera antes de entregar tudo (SupplyCenterActionDelay)
		Fix DockScanDistance;              // até onde o caminhão procura depósito (SupplyWarehouseScanDistance)
		int32_t MaxQueue = 9;              // itens na fila de um prédio
		// energia faltando (r = gerada ÷ gasta): o ritmo é r, preso entre estes dois (ThingTemplate)
		int32_t LowPowerMinPermille = 500;
		int32_t LowPowerMaxPermille = 800;
		int32_t MaxFlatnessCm = 0;         // diferença de altura máxima embaixo do prédio (0 = não confere)
		Fix EdgeMargin;                    // o prédio longe da borda do mapa
		Fix SupplyCenterDockGap;           // o centro de suprimentos longe do depósito
		int32_t DockClearCells = 1;        // todo prédio deixa estas células livres em volta do depósito
		Fix DropPileHalf;                  // meia largura da pilha que o caminhão destruído solta
		// (v4, 22/10) a patente do general, só pela luta: o XP de cada patente (RankXp[r − 1] = o da patente r; a 1ª é 0; um
		// limiar ≤ 0 acima da 1ª fecha a escada ali) e o valor por XP: 1 XP por XpDealtDivisor $ de valor inimigo destruído pro
		// dono de quem acertou, 1 por XpTakenDivisor $ do próprio pro dono do alvo. Divisor 0 = esse lado não ganha XP
		int32_t RankXp[5] = { 0, 600, 1500, 3000, 5000 };
		int32_t XpDealtDivisor = 10;
		int32_t XpTakenDivisor = 30;
		// (v4, 23/10) vender (o BuildAssistant do ZH): a % do custo que volta (SellPercentage, GameData.ini: 50%) e os passos da
		// desmontagem (o ZH: 1,5 s de andaime + 3 s até 0% + 1,5 s até −50% = 6 s); o dinheiro só no fim
		int32_t SellPercent = 50;
		int32_t SellTicks = 180;
	};

	/** (v4) A patente mais alta, o máximo de poderes do general (o tamanho dos relógios por jogador) e de mísseis num ataque. */
	constexpr int32_t MaxRank = 5;
	constexpr int32_t MaxPowers = 4;
	constexpr int32_t MaxStrikeMissiles = 64;

	/** (v4, 22/10) Um poder do general em unidades de gente (o Cruise Missile Strike): liga na patente RequiresRank (com um
	 *  prédio pronto que liga poderes), cresce na UpgradeRank (0 = nunca) e espera Cooldown depois de cada uso. Os mísseis saem
	 *  um a cada Interval, SpawnBack atrás do ponto (do lado de quem atirou) e SpawnUp acima dele, mirando num girassol de raio
	 *  PatternRadius; a arma e o voo são os da unidade só de dados Munition (a arma dela tem de ser míssil). */
	struct PowerSpec
	{
		std::string Key;
		int32_t RequiresRank = 0;
		int32_t UpgradeRank = 0;
		int32_t CooldownMs = 0;
		int32_t Missiles = 0;
		int32_t MissilesUpgraded = 0;
		int32_t PatternRadiusMm = 0;
		int32_t PatternRadiusUpgradedMm = 0;
		int32_t IntervalMs = 0;
		int32_t SpawnBackMm = 0;
		int32_t SpawnUpMm = 0;
		std::string Munition;
	};

	/** O poder em valores por passo: o que a simulação usa (Ruleset::Powers; o índice é o Value do comando UsePower). */
	struct SIMCORE_API PowerDef
	{
		std::string Key;
		int32_t RequiresRank = 0;
		int32_t UpgradeRank = 0;
		int32_t CooldownTicks = 0;
		int32_t Missiles = 0;
		int32_t MissilesUpgraded = 0;
		Fix PatternRadius;
		Fix PatternRadiusUpgraded;
		int32_t IntervalTicks = 0;
		Fix SpawnBack;
		Fix SpawnUp;
		/** A unidade só de dados com a arma do míssil (o índice em Ruleset::Units). */
		uint16_t Munition = 0;

		/** Converte os números; a munição fica 0 (quem resolve a chave é o Ruleset::Build). */
		static PowerDef FromSpec(const PowerSpec& S);
	};

	struct SIMCORE_API Ruleset
	{
		std::vector<UnitDef> Units;
		ArmorTable Armor;
		EconomyRules Economy;
		/** (v4) Os poderes do general, na ordem dos dados (o índice vai no comando UsePower). */
		std::vector<PowerDef> Powers;

		/** Índice da unidade pela chave, −1 se não existir. */
		int32_t FindUnit(const std::string& Key) const;
		uint64_t Hash() const;

		/** Monta as regras a partir dos dados: converte cada unidade e resolve as chaves das listas. */
		static bool Build(const std::vector<UnitSpec>& Specs, const ArmorTable& InArmor, const EconomyRules& InEconomy, Ruleset& Out, std::string* OutError = nullptr);
		/** (v4) O mesmo, com os poderes do general (a munição de cada um tem de ser uma unidade com arma de míssil). */
		static bool Build(const std::vector<UnitSpec>& Specs, const std::vector<PowerSpec>& InPowers, const ArmorTable& InArmor, const EconomyRules& InEconomy, Ruleset& Out, std::string* OutError = nullptr);
	};

	// Conversões de unidades de gente pra valores por passo (arredondando pro mais perto).
	inline Fix PerTickFromMilliPerSecond(int64_t MilliPerS) { return Fix::FromRatio(MilliPerS, int64_t(1000) * TicksPerSecond); }
	inline Fix PerTick2FromMilliPerSecond2(int64_t MilliPerS2) { return Fix::FromRatio(MilliPerS2, int64_t(1000) * TicksPerSecond * TicksPerSecond); }
	inline int32_t BamPerTickFromMilliDegPerSecond(int64_t MilliDegPerS) { return int32_t(Fix::RoundedDiv(MilliDegPerS * Angle::FullTurn, int64_t(360000) * TicksPerSecond)); }
	/** Milissegundos → passos, arredondando pra cima (como o Zero Hour faz com os tempos do INI). */
	inline int32_t TicksFromMs(int64_t Ms) { return int32_t((Ms * TicksPerSecond + 999) / 1000); }
}
