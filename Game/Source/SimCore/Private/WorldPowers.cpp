#include "Sim/World.h"

#include <algorithm>

// (v4, 22/10) Os poderes do general (o Cruise Missile Strike): o comando UsePower e os mísseis que saem dos ataques em
// andamento. As mesmas regras do World.cpp: só inteiro e ponto fixo, varredura em ordem de ID, desempate pelo ID, nada
// sorteado (o girassol dos pontos de mira gira pelo passo do uso).

namespace Sim
{
	namespace
	{
		/** O passo do girassol: 137,5° (o ângulo de ouro) em bam; os pontos se espalham sem fila nem buraco no meio. */
		constexpr int32_t GoldenAngleBam = 25033;
		/** A fase do girassol sai do passo do uso (um hash multiplicativo de 16 bits): o mesmo clique, os mesmos pontos. */
		constexpr uint32_t PhasePerTick = 40503u;
	}

	// ---------------------------------------------------------------- leitura

	bool World::HasPowerGranter(PlayerIndex P) const
	{
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			if (U.Owner[I] == P && DefOf(I).bGrantsPowers && IsComplete(I) && U.Hp[I] > 0 && !IsSelling(I))   // (v4) o vendido não liga (o ZH pula o SOLD)
			{
				return true;
			}
		}
		return false;
	}

	bool World::IsPowerReady(PlayerIndex P, int32_t K) const
	{
		if (P >= Players.size() || K < 0 || K >= int32_t(RulesData.Powers.size()) || K >= MaxPowers)
		{
			return false;
		}
		const PlayerState& S = Players[P];
		return !S.bDefeated && int32_t(S.Rank) >= RulesData.Powers[size_t(K)].RequiresRank && CurrentTick >= S.PowerReadyAt[K] && HasPowerGranter(P);
	}

	Vec2 World::StrikeAimPoint(int32_t Index, int32_t K) const
	{
		// o girassol (Vogel): o míssil k no ângulo fase + k × 137,5° e no raio R × √((k + ½) ÷ N), área igual pra cada um
		const Strike& S = Strikes[size_t(Index)];
		const Angle A = Angle::FromBam(int32_t(S.Phase) + K * GoldenAngleBam);
		const Fix R = S.Total > 0 ? S.Radius * Sqrt(Fix::FromRatio(2 * K + 1, 2 * int64_t(S.Total))) : Fix::Zero();
		return ClampToMap(S.Center + Vec2::FromAngle(A) * R);
	}

	// ---------------------------------------------------------------- comando e sistema

	void World::ApplyPowerCommand(const Command& C)
	{
		// (v4) o Cruise Missile Strike: o poder existe, a patente chegou, a espera acabou, o jogador tem o prédio que liga os
		// poderes (o CC pronto e vivo) e o ponto está no mapa e, com névoa, já foi visto pelo time. Qualquer recusa não mexe na
		// espera. Valendo, a espera começa agora e o ataque entra no ar (os mísseis saem no UpdatePowers deste mesmo passo)
		const int32_t K = C.Value;
		if (K < 0 || K >= int32_t(RulesData.Powers.size()) || K >= MaxPowers)
		{
			return;
		}
		PlayerState& S = Players[C.Player];
		const PowerDef& Pw = RulesData.Powers[size_t(K)];
		if (int32_t(S.Rank) < Pw.RequiresRank || CurrentTick < S.PowerReadyAt[K] || !HasPowerGranter(C.Player))
		{
			return;
		}
		const Cell At = MapData.CellOf(C.Point);
		if (!MapData.InBounds(At.X, At.Y) || (bFogOfWar && !IsCellExplored(S.Team, At.X, At.Y)))
		{
			return;
		}
		if (Pw.Munition >= RulesData.Units.size() || !RulesData.Units[Pw.Munition].Weapon.IsMissile())
		{
			return;   // o Ruleset::Build já confere; aqui só não deixa um dado quebrado derrubar a partida
		}
		// na patente da melhoria o ataque cresce sozinho (R4: 9 mísseis em 14 m)
		const bool bUpgraded = Pw.UpgradeRank > 0 && int32_t(S.Rank) >= Pw.UpgradeRank;
		const int32_t Missiles = std::clamp(bUpgraded ? Pw.MissilesUpgraded : Pw.Missiles, 0, MaxStrikeMissiles);
		if (Missiles <= 0)
		{
			return;
		}
		S.PowerReadyAt[K] = CurrentTick + TickNumber(std::max(Pw.CooldownTicks, 0));
		Strike St;
		St.Owner = C.Player;
		St.Power = uint8_t(K);
		St.Total = Missiles;
		St.Fired = 0;
		St.Center = C.Point;
		// os mísseis vêm do lado de quem atirou: da largada dele pro ponto (o ponto em cima da largada: do +X)
		const Vec2 Along = C.Point - PlayerStart(C.Player);
		St.From = DistSqRaw(C.Point, PlayerStart(C.Player)) < RadiusSqRaw(Fix::One()) ? Vec2::Make(Fix::One(), Fix::Zero()) : Along.Normalized();
		St.Radius = bUpgraded ? Pw.PatternRadiusUpgraded : Pw.PatternRadius;
		St.Phase = uint16_t((CurrentTick * PhasePerTick) & 0xFFFF);
		St.NextAt = CurrentTick;
		Strikes.push_back(St);
		Events.push_back({ EventType::PowerUsed, NoEntity, NoEntity, C.Point, int32_t(C.Player) | (K << 8) | (Missiles << 16) });
	}

	void World::UpdatePowers()
	{
		// (v4) os mísseis dos ataques em andamento, na ordem do uso: um a cada IntervalTicks, cada um SpawnBack atrás do seu
		// ponto de mira (do lado de quem atirou), SpawnUp acima dele, já na velocidade de cruzeiro e apontado pro ponto. Daí
		// em diante é um tiro como o da Patriot sem alvo (o FlyMissile): explode no ponto e o dano em área faz o resto. Sem
		// atirador (Shooter NoEntity): o XP vai pro dono do ataque (Projectile.Owner)
		for (int32_t Index = 0; Index < int32_t(Strikes.size()); ++Index)
		{
			Strike& S = Strikes[size_t(Index)];
			const PowerDef& Pw = RulesData.Powers[S.Power];
			const WeaponDef& W = RulesData.Units[Pw.Munition].Weapon;
			while (S.Fired < S.Total && S.NextAt <= CurrentTick)
			{
				const Vec2 Aim = StrikeAimPoint(Index, S.Fired);
				Projectile P;
				P.Id = NextProjectileId++;
				P.Shooter = NoEntity;
				P.Target = NoEntity;
				P.Owner = S.Owner;
				P.Def = Pw.Munition;
				P.Damage = W.Damage;
				P.Type = W.Type;
				P.Step = W.ProjectileStep;
				P.bMissile = 1;
				P.Aim = Aim;
				P.AimZ = GroundHeightAt(Aim);
				P.Pos = ClampToMap(Aim - S.From * Pw.SpawnBack);
				P.Z = P.AimZ + Pw.SpawnUp;
				const Vec2 To = Aim - P.Pos;
				P.Yaw = To.Heading();
				P.Pitch = Atan2(P.AimZ - P.Z, To.Length());
				P.Speed = W.MissileLaunchStep;
				Projectiles.push_back(P);
				Events.push_back({ EventType::WeaponFired, NoEntity, NoEntity, Aim, int32_t(P.Id) });
				++S.Fired;
				S.NextAt += TickNumber(std::max(Pw.IntervalTicks, 1));
			}
		}
		Strikes.erase(std::remove_if(Strikes.begin(), Strikes.end(), [](const Strike& S) { return S.Fired >= S.Total; }), Strikes.end());
	}
}
