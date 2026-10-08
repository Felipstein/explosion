#pragma once

#include "Sim/SimApi.h"

// Dados de design das unidades e armas.
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

	/** Unidade em unidades de gente, inteiros em milésimos. */
	struct UnitSpec
	{
		std::string Key;                   // nome estável ("usa_mbt"): os dados e o desenho se acham por ele
		int32_t MaxHp = 0;
		ArmorType Armor = ArmorType::Tank;
		int32_t RadiusMm = 0;              // raio de colisão entre unidades
		int32_t PathRadiusMm = 0;          // meia largura contra o terreno (passa em vão mais estreito que o raio)
		int32_t SpeedMmPerS = 0;
		int32_t AccelMmPerS2 = 0;
		int32_t BrakeMmPerS2 = 0;
		int32_t TurnMilliDegPerS = 0;      // giro do casco
		int32_t TurretTurnMilliDegPerS = 0;
		int32_t TurretIdleReturnMs = 0;    // parada sem alvo, a torre volta pra frente depois disto
		int32_t SightMm = 0;               // raio em que ela procura inimigo sozinha
		WeaponSpec Weapon;
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

		static UnitDef FromSpec(const UnitSpec& S);
	};

	/** Porcentagem de dano que cada tipo de dano causa em cada armadura (100 = cheio). */
	struct ArmorTable
	{
		int32_t Percent[int(ArmorType::Count)][int(DamageType::Count)] = {};

		int32_t Get(ArmorType A, DamageType D) const { return Percent[int(A)][int(D)]; }
		void Set(ArmorType A, DamageType D, int32_t P) { Percent[int(A)][int(D)] = P; }
	};

	struct SIMCORE_API Ruleset
	{
		std::vector<UnitDef> Units;
		ArmorTable Armor;

		/** Índice da unidade pela chave, −1 se não existir. */
		int32_t FindUnit(const std::string& Key) const;
		uint64_t Hash() const;
	};

	// Conversões de unidades de gente pra valores por passo (arredondando pro mais perto).
	inline Fix PerTickFromMilliPerSecond(int64_t MilliPerS) { return Fix::FromRatio(MilliPerS, int64_t(1000) * TicksPerSecond); }
	inline Fix PerTick2FromMilliPerSecond2(int64_t MilliPerS2) { return Fix::FromRatio(MilliPerS2, int64_t(1000) * TicksPerSecond * TicksPerSecond); }
	inline int32_t BamPerTickFromMilliDegPerSecond(int64_t MilliDegPerS) { return int32_t(Fix::RoundedDiv(MilliDegPerS * Angle::FullTurn, int64_t(360000) * TicksPerSecond)); }
	/** Milissegundos → passos, arredondando pra cima (como o Zero Hour faz com os tempos do INI). */
	inline int32_t TicksFromMs(int64_t Ms) { return int32_t((Ms * TicksPerSecond + 999) / 1000); }
}
