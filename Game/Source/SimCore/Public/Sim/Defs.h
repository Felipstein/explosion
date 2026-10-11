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

		bool IsStructure() const { return Kind == ObjectKind::Structure; }
		bool HasWeapon() const { return Weapon.Damage > 0 && Weapon.Range.Raw > 0; }
		bool CanConstruct() const { return !Constructs.empty(); }

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
	};

	struct SIMCORE_API Ruleset
	{
		std::vector<UnitDef> Units;
		ArmorTable Armor;
		EconomyRules Economy;

		/** Índice da unidade pela chave, −1 se não existir. */
		int32_t FindUnit(const std::string& Key) const;
		uint64_t Hash() const;

		/** Monta as regras a partir dos dados: converte cada unidade e resolve as chaves das listas. */
		static bool Build(const std::vector<UnitSpec>& Specs, const ArmorTable& InArmor, const EconomyRules& InEconomy, Ruleset& Out, std::string* OutError = nullptr);
	};

	// Conversões de unidades de gente pra valores por passo (arredondando pro mais perto).
	inline Fix PerTickFromMilliPerSecond(int64_t MilliPerS) { return Fix::FromRatio(MilliPerS, int64_t(1000) * TicksPerSecond); }
	inline Fix PerTick2FromMilliPerSecond2(int64_t MilliPerS2) { return Fix::FromRatio(MilliPerS2, int64_t(1000) * TicksPerSecond * TicksPerSecond); }
	inline int32_t BamPerTickFromMilliDegPerSecond(int64_t MilliDegPerS) { return int32_t(Fix::RoundedDiv(MilliDegPerS * Angle::FullTurn, int64_t(360000) * TicksPerSecond)); }
	/** Milissegundos → passos, arredondando pra cima (como o Zero Hour faz com os tempos do INI). */
	inline int32_t TicksFromMs(int64_t Ms) { return int32_t((Ms * TicksPerSecond + 999) / 1000); }
}
