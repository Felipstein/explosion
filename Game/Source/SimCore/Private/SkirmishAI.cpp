#include "Sim/SkirmishAI.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>

namespace Sim
{
	namespace
	{
		/** Com base, ela reavalia a obra e a fila a cada 1 s (o ataque segue no ritmo do RethinkTicks). */
		constexpr int32_t BaseThinkTicks = TicksPerSecond;
		/** A espiral da procura de lugar: passo entre anéis e entre pontos, e até onde vai. */
		constexpr int32_t SpotRingStepM = 6;
		constexpr int32_t SpotPointStepM = 6;
		constexpr int32_t SpotMaxRingsM = 120;
		/** A folga a mais em volta do CC e do depósito quando ela começa a procurar. */
		constexpr int32_t SpotMarginM = 6;
		// a defesa de base (14/10): até 2, a 2ª só com dinheiro sobrando (a fábrica não para), a 45 m da base rumo ao
		// inimigo (o alcance da Patriot é 59 m: cobre a frente do CC e a saída da fábrica). (v4, 22/10) Com o dinheiro
		// inicial maior: uma defesa a mais por fábrica querida (1 + fábricas, até 4) e um Dozer por fábrica, até 3 (com 1
		// fábrica, como antes: 2 defesas e 1 Dozer)
		constexpr int32_t MaxDefensesCap = 4;
		constexpr int32_t MaxDozers = 3;
		constexpr int32_t SecondDefenseMoney = 3000;
		// (v4) depois de um ataque à casa: por quanto tempo a próxima defesa sai com o preço dela, e quanto a fábrica guarda
		constexpr int32_t DefenseAfterAlertS = 90;
		constexpr int32_t DefenseSaveMaxS = 30;
		// (v4) a obra sob fogo: a fundação tem 1 PV e o dinheiro não volta. Nada novo com inimigo armado a menos de 60 m do
		// lugar (o tanque alcança 39,5 m, a Patriot 59 m), nem a menos de 30 m de onde uma do mesmo papel morreu há < 30 s.
		// (revisão de 23/10) Os 60 m esqueciam o tamanho da obra: a arma mede do centro dela até o raio do prédio-alvo
		// (World.cpp InReach; Defs.cpp: o raio do prédio é a metade do lado menor), então a Patriot pega a fundação da Patriot
		// a 63,2 m, a do centro a 70,8 m (medido: a mesma Patriot matou 11 fundações em 12 min, $11 000). Agora é o alcance
		// da arma + o raio da obra + ThreatMarginM; a unidade anda, então nunca menos que os 60 m de antes
		constexpr int32_t ThreatRadiusM = 60;
		constexpr int32_t ThreatMarginM = 4;
		constexpr int32_t LostSiteRadiusM = 30;
		constexpr int32_t LostSiteMemoryS = 30;
		constexpr int32_t DefenseAheadM = 45;
		// (16/10, a IA da v3) a expansão: com os depósitos perto da base secos (nenhum com caixa ao alcance do caminhão de
		// nenhum centro dela), um centro novo no depósito com caixa mais perto de casa, até 3; o caminhão parado sem
		// depósito vai pro com caixa mais perto dele, do lado de casa primeiro (sem isso, robô × robô empatava em 15 min
		// com o mapa seco; a IA do ZH vai buscar)
		constexpr int32_t MaxSupplyCenters = 3;
		// (v4) o depósito disputado: inimigo armado (ou prédio dele) conhecido a até isto, ou um centro dela morto a até
		// DockLostRadiusM há menos de LostSiteMemoryS
		constexpr int32_t DockThreatM = 90;
		constexpr int32_t DockLostRadiusM = 60;
		// (v4) o papel de mentira do LostSites: "um Dozer morreu aqui" (o lugar fica perigoso por LostSiteMemoryS)
		constexpr int32_t DozerLostRole = -2;
		// (v4) a zona de lançamento atrás do CC: 40 m do centro da base pro lado oposto ao inimigo (dentro da base, longe
		// da frente)
		constexpr int32_t IncomeBehindM = 40;
		// (v4) depois da janela de economizar pra zona (IncomeSaveMaxS), tanto tempo gastando normal antes de outra janela
		constexpr int32_t IncomeSaveRetryS = 60;
		// (v4) o ataque do poder pega o que está a até o raio do girassol + isto do ponto (a borda do casco ou do lote)
		constexpr int32_t StrikeReachMarginM = 4;
		// (v4) o cerco: o tanque que já está indo pro ponto de espera (a até 8 m dele) ou parado perto (24 m: o grupo se
		// espalha em volta) não recebe a ordem de novo; o defensor da casa que já vai pro intruso (a até 16 m), também não
		constexpr int32_t SiegeOrderSlackM = 8;
		constexpr int32_t SiegeHoldSlackM = 24;
		constexpr int32_t DefendOrderSlackM = 16;
		// (v4) o teto das linhas de log guardadas sem ninguém levar (o jogo sem o encaminhamento não cresce sem fim)
		constexpr size_t MaxLogLines = 256;
		// (2ª revisão de 23/10) a reposição do Dozer: com 2 ou mais mortos nos últimos DozerDeathWindowS, o CC espera
		// DozerRebuyWaitS desde a última morte, dobrando a cada morte a mais, até DozerRebuyMaxS (medido: a porta do CC no
		// alcance de uma Patriot, um Dozer de $1 000 a cada 12 s, 31 em 8 min)
		constexpr int32_t DozerDeathWindowS = 300;
		constexpr int32_t DozerRebuyWaitS = 20;
		constexpr int32_t DozerRebuyMaxS = 240;
		// (2ª revisão) o lugar aonde o Dozer não chega sem passar no alcance de uma defesa: a procura passa a até isto dele
		constexpr int32_t UnreachableRadiusM = 12;
		// (2ª revisão) o ponto do desvio colado em quem anda (a menos disto) passa pro seguinte
		constexpr int32_t RouteStepMinM = 4;
		// (2ª revisão) o Dozer à toa a até isto da saída do CC sai da frente (a saída e o ponto de encontro natural ficam a ~6 m da
		// borda; a passagem entre o CC e um prédio colado na frente dele tem ~8 m)
		constexpr int32_t ParkFromDoorM = 16;
		// (3ª revisão de 23/10) o caminho raspando: sem caminho fora dos círculos (o Dozer num bolso entre dois alcances e o
		// próprio CC: os círculos só encolhiam com ele dentro; medido, 20 min parado com $10 000 e 0 tanques), eles encolhem
		// uma célula por vez, até GrazeTiers, e o 1º caminho que sai vale se quem anda sobrevive ao pior caso nele (a rajada
		// cheia de cada defesa na entrada do alcance dela e o ritmo do pente pelo tempo lá dentro). A 1ª célula é a folga
		// (ThreatMarginM = 4 m, a célula do mapa); as outras já entram no alcance, e o dano estimado decide
		constexpr int32_t GrazeTiers = 3;
		// (3ª revisão) a obra que não anda: o Dozer com obra que não sai de DozerStallMoveM com a obra sem avançar por
		// DozerStallS, parado no lote da obra de outro Dozer assim, larga a obra (o pensar seguinte retoma a obra parada mais
		// perto dele). Medido: dois Dozers, cada um no lote da obra do outro (o jogo só tira do lote o amigo sem tarefa),
		// parados 20 min com $48 800; o normal é a obra andar a cada passo, e o amigo à toa sai do lote em ~3 s (7,9 m/s)
		constexpr int32_t DozerStallS = 10;
		constexpr int32_t DozerStallMoveM = 2;
		// (4ª revisão de 23/10) a fuga: o peso de cada círculo no custo do passo (1 m dentro de um alcance vale 9 m fora) e a
		// perna mais longa dela (o jogo refaz o caminho até o ponto)
		constexpr int32_t EscapeInsideWeight = 8;
		constexpr int32_t EscapeLegM = 24;
		// (4ª revisão) a perna mais longa do desvio (o jogo refaz o caminho até o ponto pelo mapa sem os círculos)
		constexpr int32_t RouteLegMaxM = 48;
		// (4ª revisão) com um alcance a até isto do caminho, o caminho longo vai em pernas
		constexpr int32_t WorkNearM = 40;
		// (4ª revisão) a obra mais perto que isto vai direto, sem pernas (medido: com pernas nas obras da base, ao lado das
		// Patriots, cada perna custava um pensar e a zona saía 2 min depois: metade dos tanques em 20 min); mais longe (o
		// depósito do meio, a volta pra casa), em pernas, com o preço guardado pelo PlanHeld
		constexpr int32_t BuildLegMinM = 120;
		// (4ª revisão) o Dozer fugindo a até isto do fim da perna recebe a próxima já (um pensar de Dozer andando, 7,9 m/s, e
		// a folga: parado no fim da perna até o pensar seguinte, ele levava mais uma rajada)
		constexpr int32_t FleeAheadM = 10;
		// (4ª revisão) de quanto em quanto tempo ela refaz a fuga da porta coberta (a reposição do Dozer)
		constexpr int32_t DoorEscapeEveryS = 5;
		// (4ª revisão) a procura de lugar que não achou nada (nenhum Dozer à toa chega numa obra) só é refeita depois disto,
		// enquanto nada muda (a obra, o dinheiro dela, as defesas, os Dozers, os prédios, os lugares perdidos): medido, a base
		// cercada refazia a procura toda a cada pensar, 34 espirais e 6 ms por pensar por 30 min
		constexpr int32_t PlaceRetryS = 4;
		// (4ª revisão) a região fora dos alcances com pelo menos tantas células (32 000 m²) é o lado aberto; menor, um bolso
		constexpr int32_t OpenRegionCells = 2000;
		// (4ª revisão) com tantos Dozers mortos no alcance de uma defesa inimiga sem a base andar (um prédio dela pronto ou uma
		// defesa inimiga caída), o CC não repõe mais (medido: a porta coberta por uma Patriot e a fuga pelo alcance de outra,
		// 32 Dozers em 30 min com 0 tanques)
		constexpr int32_t FutileDozerDeathsMax = 3;

		bool IsArmed(const World& State, const UnitView& V)
		{
			// quem vai pra ofensiva: tem arma e anda (a defesa de base atira, mas fica; 14/10, a Patriot entrava na onda e
			// puxava o centro do exército pra casa)
			return State.Rules().Units[V.Def].HasWeapon() && !State.Rules().Units[V.Def].IsStructure();
		}

		/** (v4) A artilharia sai dos dados: a unidade cuja arma tem alcance mínimo (o Tomahawk). */
		bool IsArtillery(const UnitDef& D)
		{
			return !D.IsStructure() && D.HasWeapon() && D.Weapon.MinRange.Raw > 0;
		}

		bool Contains(const std::vector<EntityId>& List, EntityId Id)
		{
			return std::find(List.begin(), List.end(), Id) != List.end();
		}

		int32_t Meters(Fix V)
		{
			return int32_t(V.Floor());
		}

		/** (2ª revisão de 23/10) A distância² (crua, como o DistSqRaw) de C até o segmento AB. */
		int64_t SegmentDistSqRaw(Vec2 A, Vec2 B, Vec2 C)
		{
			const Vec2 D = B - A;
			const Vec2 E = C - A;
			const int64_t Len2 = D.LengthSqRaw();
			const int64_t Along = E.X.Raw * D.X.Raw + E.Y.Raw * D.Y.Raw;   // na escala do LengthSqRaw (Q32)
			if (Len2 == 0 || Along <= 0)
			{
				return DistSqRaw(A, C);
			}
			if (Along >= Len2)
			{
				return DistSqRaw(B, C);
			}
			// o ponto mais perto: A + D·t, t = Along / Len2 em Q16 (erro de ~2 cm num segmento de 1 km)
			const Fix T = Fix::FromRaw(Along >> Fix::FracBits) / Fix::FromRaw(std::max<int64_t>(1, Len2 >> Fix::FracBits));
			return DistSqRaw(A + D * T, C);
		}

		/** (3ª revisão de 23/10) Quanto do segmento AB fica dentro do círculo (C, Radius). */
		Fix ChordInside(Vec2 A, Vec2 B, Vec2 C, Fix Radius)
		{
			const Vec2 D = B - A;
			const Fix Len = D.Length();
			if (Len.Raw <= 0)
			{
				return Fix::Zero();
			}
			const Vec2 E = C - A;
			const Fix Along = (E.X * D.X + E.Y * D.Y) / Len;
			const Fix Off2 = Max(E.X * E.X + E.Y * E.Y - Along * Along, Fix::Zero());
			const Fix R2 = Radius * Radius;
			if (Off2 >= R2)
			{
				return Fix::Zero();
			}
			const Fix Half = Sqrt(R2 - Off2);
			return Max(Min(Along + Half, Len) - Max(Along - Half, Fix::Zero()), Fix::Zero());
		}

		/** (3ª revisão) O dano da defesa Shooter em quem passa Length metros no alcance dela na velocidade máxima: a rajada
		 *  cheia na entrada (o pior caso: o pente cheio, a mira pronta), depois o ritmo do pente (World::AfterShot). */
		int64_t ExposureDamage(const Ruleset& R, const UnitDef& Shooter, const UnitDef& Walker, Fix Length)
		{
			if (Length.Raw <= 0)
			{
				return 0;
			}
			const WeaponDef& W = Shooter.Weapon;
			const int64_t Ticks = Walker.MaxSpeed.Raw > 0 ? int64_t((Length / Walker.MaxSpeed).Floor()) : int64_t(1) << 20;
			const int64_t Reload = std::max<int64_t>(1, W.ReloadTicks);
			int64_t Shots = 1 + Ticks / Reload;
			if (W.ClipSize > 1)
			{
				const int64_t Cycle = (W.ClipSize - 1) * Reload + std::max<int64_t>(1, W.ClipReloadTicks);
				Shots = Ticks / Cycle * W.ClipSize + std::min<int64_t>(W.ClipSize, 1 + Ticks % Cycle / Reload);
			}
			return Shots * W.Damage * R.Armor.Get(Walker.Armor, W.Type) / 100;
		}
	}

	int32_t SkirmishAI::DirToward(Vec2 From, Vec2 To)
	{
		const Vec2 D = To - From;
		if (Abs(D.X) >= Abs(D.Y))
		{
			return D.X.Raw >= 0 ? 0 : 2;
		}
		return D.Y.Raw >= 0 ? 1 : 3;
	}

	void SkirmishAI::TakeLog(std::vector<std::string>& Out)
	{
		Out.insert(Out.end(), LogLines.begin(), LogLines.end());
		LogLines.clear();
	}

	void SkirmishAI::Note(const char* Format, ...)
	{
		char Text[256];
		const int32_t Head = std::snprintf(Text, sizeof(Text), "RTS AI p%d: ", int32_t(Me));
		va_list Args;
		va_start(Args, Format);
		std::vsnprintf(Text + Head, sizeof(Text) - size_t(Head), Format, Args);
		va_end(Args);
		if (LogLines.size() >= MaxLogLines)
		{
			LogLines.erase(LogLines.begin(), LogLines.begin() + MaxLogLines / 2);
		}
		LogLines.emplace_back(Text);
	}

	bool SkirmishAI::Knows(const World& State, const UnitView& V) const
	{
		if (!State.IsFogOfWar())
		{
			return true;
		}
		if (V.bStructure)
		{
			const Cell C = State.Map().CellOf(V.Pos);
			return State.IsCellExplored(State.TeamOf(Me), C.X, C.Y);
		}
		return State.IsVisibleTo(Me, V.Pos);
	}

	bool SkirmishAI::FindSpot(const World& State, uint16_t Def, int32_t Dir, Vec2 Anchor, Fix MinRadius, Angle Start, const std::vector<KeepOut>& Avoid, Vec2& Out,
		const std::vector<uint8_t>* Reach, int32_t Lax, const Lot* Keep, bool bFine) const
	{
		// anéis a partir de MinRadius; em cada anel, pontos a cada ~6 m, sempre na mesma ordem, começando
		// virado pro inimigo (num mapa espelhado, os dois lados constroem no mesmo lugar relativo). (revisão de 23/10) O
		// lugar no alcance de uma defesa inimiga (Avoid) fica pra trás e a espiral segue. (4ª revisão) Com Reach, também o
		// lote com o centro fora das células aonde o Dozer chega; as contas baratas antes do CanPlace (ele passa por todos os
		// prédios)
		const SimMap& Map = State.Map();
		const auto Clear = [&](Vec2 P)
		{
			const Vec2 S = State.SnapStructure(Def, Dir, P);
			if (Reach != nullptr)
			{
				const Cell C = Map.CellOf(S);
				if (!Map.InBounds(C.X, C.Y) || (*Reach)[size_t(Map.Index(C.X, C.Y))] == 0)
				{
					return false;
				}
			}
			if (Keep != nullptr)
			{
				// o lote (os lados 1 e 3 trocam X e Y) longe uma célula do lote guardado
				const UnitDef& D = State.Rules().Units[Def];
				const Fix HX = (Dir & 1) != 0 ? D.HalfY : D.HalfX;
				const Fix HY = (Dir & 1) != 0 ? D.HalfX : D.HalfY;
				if (Abs(S.X - Keep->Pos.X) < HX + Keep->HalfX + Map.CellSize && Abs(S.Y - Keep->Pos.Y) < HY + Keep->HalfY + Map.CellSize)
				{
					return false;
				}
			}
			return std::none_of(Avoid.begin(), Avoid.end(), [&](const KeepOut& K) { return DistSqRaw(K.Pos, S) <= K.RadiusSq; });
		};
		const auto Placeable = [&](Vec2 P)
		{
			const PlaceResult Result = State.CanPlace(Me, Def, Dir, P);
			return Result == PlaceResult::Ok || (Lax >= 1 && Result == PlaceResult::NotVisible)
				|| (Lax >= 2 && (Result == PlaceResult::MissingPrereq || Result == PlaceResult::NoMoney));
		};
		if (MinRadius.Raw <= 0 && Clear(Anchor) && Placeable(Anchor))
		{
			Out = State.SnapStructure(Def, Dir, Anchor);
			return true;
		}
		// (4ª revisão) bFine: anéis e pontos a cada célula (o lugar que sobra entre os alcances tem uma ou duas células de
		// largura e a espiral de 6 m passava por cima dele, dependendo de onde ela começava)
		const int32_t RingStepM = bFine ? int32_t(Map.CellSize.Floor()) : SpotRingStepM;
		const int32_t PointStepM = bFine ? int32_t(Map.CellSize.Floor()) : SpotPointStepM;
		for (int32_t RingM = 0; RingM <= SpotMaxRingsM; RingM += std::max<int32_t>(1, RingStepM))
		{
			const Fix Radius = MinRadius + Fix::FromInt(RingM);
			if (Radius.Raw <= 0)
			{
				continue;
			}
			const int32_t Points = std::max<int32_t>(8, int32_t((Radius * 6).Floor() / std::max<int32_t>(1, PointStepM)));
			for (int32_t K = 0; K < Points; ++K)
			{
				const Angle A = Angle::FromBam(int32_t(Start.Bam) + int32_t(int64_t(K) * Angle::FullTurn / Points));
				const Vec2 P = Anchor + Vec2::FromAngle(A) * Radius;
				if (Clear(P) && Placeable(P))
				{
					Out = State.SnapStructure(Def, Dir, P);
					return true;
				}
			}
		}
		return false;
	}

	std::vector<SkirmishAI::KeepOut> SkirmishAI::LiveCircles(const World& State, const std::vector<KeepOut>& Danger, Vec2 From, Vec2 To)
	{
		std::vector<KeepOut> Live;
		for (const KeepOut& K : Danger)
		{
			// quem já está dentro (o Dozer que nasceu na porta do CC) não vai mais pra dentro: o círculo encolhe até ele, menos
			// uma célula (fora da conta, o caminho podia cortar o meio do alcance: medido, Dozers mortos a 30 m da Patriot)
			KeepOut L = K;
			for (const Vec2 P : { From, To })
			{
				if (DistSqRaw(L.Pos, P) <= L.RadiusSq)
				{
					L.RadiusSq = RadiusSqRaw(Max((P - L.Pos).Length() - State.Map().CellSize, Fix::Zero()));
				}
			}
			if (L.RadiusSq > 0)
			{
				Live.push_back(L);
			}
		}
		return Live;
	}

	void SkirmishAI::CloseCircles(const SimMap& Map, const std::vector<KeepOut>& Live, Fix Shrink)
	{
		// a folga do mapa (só ela o A* lê) com as células dos círculos fechadas: o caminho que sai contorna o alcance.
		// (3ª revisão) Os círculos menos Shrink (o caminho raspando)
		SafeMap.Width = Map.Width;
		SafeMap.Height = Map.Height;
		SafeMap.CellSize = Map.CellSize;
		SafeMap.Origin = Map.Origin;
		SafeMap.Clearance = Map.Clearance;
		SafeMap.CrushClearance = Map.CrushClearance;
		for (const KeepOut& K : Live)
		{
			const Fix Radius = Sqrt(Fix::FromRaw(K.RadiusSq >> Fix::FracBits)) - Shrink;
			if (Radius.Raw <= 0)
			{
				continue;
			}
			const int64_t RadiusSq = Shrink.Raw > 0 ? RadiusSqRaw(Radius) : K.RadiusSq;
			int32_t Span = 0;
			while (Span < 64 && RadiusSqRaw(Map.CellSize * Span) < RadiusSq)
			{
				++Span;
			}
			const Cell C = Map.CellOf(K.Pos);
			for (int32_t Y = C.Y - Span; Y <= C.Y + Span; ++Y)
			{
				for (int32_t X = C.X - Span; X <= C.X + Span; ++X)
				{
					if (Map.InBounds(X, Y) && DistSqRaw(Map.CellCenter(Cell{ X, Y }), K.Pos) <= RadiusSq)
					{
						const int32_t I = Map.Index(X, Y);
						SafeMap.Clearance[size_t(I)] = 0;
						if (size_t(I) < SafeMap.CrushClearance.size())
						{
							SafeMap.CrushClearance[size_t(I)] = 0;
						}
					}
				}
			}
		}
	}

	int32_t SkirmishAI::FloodReach(const World& State, const std::vector<KeepOut>& Danger, Vec2 From, int32_t Mobility, int32_t Tier)
	{
		// (4ª revisão de 23/10) a mesma folga do SafeStep (o círculo onde quem anda está encolhe até ele; Tier células a menos
		// em cada um, o raspando), inundada pelos 4 lados a partir dele (o A* não corta quina): o que fica marcado ele alcança
		const SimMap& Map = State.Map();
		CloseCircles(Map, LiveCircles(State, Danger, From, From), Map.CellSize * Tier);
		ReachMark.assign(size_t(Map.Width) * size_t(Map.Height), 0);
		Cell Start = Map.CellOf(From);
		if (!Map.InBounds(Start.X, Start.Y) || !Pathfinder::NearestWalkable(SafeMap, Start, Mobility, 3, Start))
		{
			return 0;
		}
		static constexpr int32_t SideX[4] = { 1, -1, 0, 0 };
		static constexpr int32_t SideY[4] = { 0, 0, 1, -1 };
		int32_t Count = 0;
		ReachStack.clear();
		ReachStack.push_back(Map.Index(Start.X, Start.Y));
		ReachMark[size_t(ReachStack.back())] = 1;
		while (!ReachStack.empty())
		{
			const int32_t I = ReachStack.back();
			ReachStack.pop_back();
			++Count;
			const int32_t CX = I % Map.Width;
			const int32_t CY = I / Map.Width;
			for (int32_t D = 0; D < 4; ++D)
			{
				const int32_t NX = CX + SideX[D];
				const int32_t NY = CY + SideY[D];
				if (SafeMap.IsWalkable(NX, NY, Mobility) && ReachMark[size_t(Map.Index(NX, NY))] == 0)
				{
					ReachMark[size_t(Map.Index(NX, NY))] = 1;
					ReachStack.push_back(Map.Index(NX, NY));
				}
			}
		}
		return Count;
	}

	bool SkirmishAI::EscapeStep(const World& State, const std::vector<KeepOut>& Danger, const UnitView& Walker, int32_t Mobility, bool bAnyDamage, Vec2& OutStep)
	{
		// (4ª revisão de 23/10) a fuga ia pra um ponto logo fora do alcance da defesa mais perto e, sem caminho fora dos outros
		// alcances, direto: o Dozer novo da porta coberta cruzava o alcance de outra Patriot e morria (medido: 32 Dozers em
		// 30 min, aos pares); e o ponto mais perto fora dos alcances dava num bolso na borda do mapa (o Dozer chegava com 70
		// de vida onde nada cabia). Agora é o caminho menos exposto até o lado aberto: Dijkstra nas células andáveis, cada passo
		// custando o comprimento × (1 + EscapeInsideWeight × os círculos onde a célula está), até a 1ª célula fora de todo
		// círculo numa região aberta (a região com os círculos fechados de pelo menos OpenRegionCells); sem nenhuma, a 1ª fora
		// de todo círculo. Inteiro e com desempate pelo índice: a mesma fuga em qualquer PC
		const SimMap& Map = State.Map();
		const int32_t Cells = Map.Width * Map.Height;
		bEscapeOpen = false;
		Cell Start = Map.CellOf(Walker.Pos);
		if (Cells <= 0 || !Map.InBounds(Start.X, Start.Y) || !Pathfinder::NearestWalkable(Map, Start, Mobility, 3, Start))
		{
			return false;
		}
		// o ponto de antes primeiro (logo fora do alcance da defesa mais perto onde ele está, do lado oposto a ela, girando de
		// 20 em 20° pros dois lados), se ele chega lá sem ir mais fundo nem entrar em outro alcance (FloodReach no nível 0): é
		// a saída mais curta desse alcance (medido: o Dozer da largada dentro de 2 ou 3 alcances, que essa fuga tirava vivo,
		// morria pelo Dijkstra até o lado aberto). Fica o de menor pior caso entre ele e o Dijkstra abaixo (sem ele, ou quando
		// a fuga de antes cruzava outro alcance, o Dijkstra)
		const KeepOut* Near = nullptr;
		for (const KeepOut& K : Danger)
		{
			if (DistSqRaw(K.Pos, Walker.Pos) <= K.RadiusSq && (Near == nullptr || DistSqRaw(K.Pos, Walker.Pos) < DistSqRaw(Near->Pos, Walker.Pos)))
			{
				Near = &K;
			}
		}
		std::vector<Vec2> AwayWay;
		Vec2 AwayStep;
		int64_t AwayDamage = -1;
		bool bAwayOpen = false;
		if (Near != nullptr && FloodReach(State, Danger, Walker.Pos, Mobility, 0) > 0)
		{
			const Fix Away = Sqrt(Fix::FromRaw(Near->RadiusSq >> Fix::FracBits)) + Fix::FromInt(2 * SpotMarginM);
			const int32_t Base = int32_t((Walker.Pos - Near->Pos).Heading().Bam);
			for (int32_t K = 0; K <= 16; ++K)
			{
				const int32_t Turn = (K + 1) / 2 * (K % 2 == 0 ? -1 : 1) * Angle::FullTurn / 18;
				const Vec2 P = Near->Pos + Vec2::FromAngle(Angle::FromBam(Base + Turn)) * Away;
				const Cell C = Map.CellOf(P);
				const Vec2 Center = Map.CellCenter(C);
				if (!Map.InBounds(C.X, C.Y) || ReachMark[size_t(Map.Index(C.X, C.Y))] == 0
					|| std::any_of(Danger.begin(), Danger.end(), [&](const KeepOut& O) { return DistSqRaw(O.Pos, Center) <= O.RadiusSq; }))
				{
					continue;
				}
				Vec2 Step;
				if (SafeStep(State, Danger, Walker, Center, Mobility, true, Step, true) == 0)
				{
					break;
				}
				AwayWay = LastWay;
				AwayStep = Step;
				AwayDamage = PathDamage(State, Danger, Walker, AwayWay);
				bAwayOpen = FloodReach(State, Danger, Center, Mobility, 0) >= OpenRegionCells;
				break;
			}
		}
		if (AwayDamage == 0)
		{
			LastWay = AwayWay;
			OutStep = AwayStep;
			bEscapeOpen = bAwayOpen;
			return true;
		}
		// quantos círculos cobrem cada célula
		EscapeInside.assign(size_t(Cells), 0);
		for (const KeepOut& K : Danger)
		{
			int32_t Span = 0;
			while (Span < 64 && RadiusSqRaw(Map.CellSize * Span) < K.RadiusSq)
			{
				++Span;
			}
			const Cell C = Map.CellOf(K.Pos);
			for (int32_t Y = C.Y - Span; Y <= C.Y + Span; ++Y)
			{
				for (int32_t X = C.X - Span; X <= C.X + Span; ++X)
				{
					if (Map.InBounds(X, Y) && DistSqRaw(Map.CellCenter(Cell{ X, Y }), K.Pos) <= K.RadiusSq)
					{
						uint8_t& Count = EscapeInside[size_t(Map.Index(X, Y))];
						Count = uint8_t(std::min(Count + 1, 255));
					}
				}
			}
		}
		// as regiões fora de todo círculo e o tamanho de cada uma
		CloseCircles(Map, Danger, Fix::Zero());
		const std::vector<int32_t> Sizes = Pathfinder::LabelRegions(SafeMap, Mobility, EscapeLabels);
		const auto Open = [&](int32_t I) { const int32_t L = EscapeLabels[size_t(I)]; return L > 0 && Sizes[size_t(L)] >= OpenRegionCells; };
		// o Dijkstra (8 lados sem cortar quina, como o A* do jogo)
		static constexpr int32_t SideX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
		static constexpr int32_t SideY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
		EscapeCost.assign(size_t(Cells), INT64_MAX);
		EscapeFrom.assign(size_t(Cells), -1);
		std::vector<std::pair<int64_t, int32_t>> Heap;
		const auto Later = [](const std::pair<int64_t, int32_t>& A, const std::pair<int64_t, int32_t>& B) { return A > B; };
		const int32_t First = Map.Index(Start.X, Start.Y);
		EscapeCost[size_t(First)] = 0;
		Heap.push_back({ 0, First });
		int32_t Goal = -1;
		int32_t Fallback = -1;
		int32_t Expanded = 0;
		while (!Heap.empty() && Expanded < Pathfinder::MaxExpansions)
		{
			std::pop_heap(Heap.begin(), Heap.end(), Later);
			const auto [Cost, I] = Heap.back();
			Heap.pop_back();
			if (Cost != EscapeCost[size_t(I)])
			{
				continue;
			}
			++Expanded;
			if (EscapeInside[size_t(I)] == 0 && EscapeLabels[size_t(I)] > 0)
			{
				Fallback = Fallback < 0 ? I : Fallback;
				if (Open(I))
				{
					Goal = I;
					break;
				}
			}
			const int32_t CX = I % Map.Width;
			const int32_t CY = I / Map.Width;
			for (int32_t D = 0; D < 8; ++D)
			{
				const int32_t NX = CX + SideX[D];
				const int32_t NY = CY + SideY[D];
				if (!Map.IsWalkable(NX, NY, Mobility) || (D >= 4 && (!Map.IsWalkable(NX, CY, Mobility) || !Map.IsWalkable(CX, NY, Mobility))))
				{
					continue;
				}
				const int32_t J = Map.Index(NX, NY);
				const int64_t Step = int64_t(D < 4 ? 10 : 14) * (1 + int64_t(EscapeInsideWeight) * EscapeInside[size_t(J)]);
				if (Cost + Step < EscapeCost[size_t(J)])
				{
					EscapeCost[size_t(J)] = Cost + Step;
					EscapeFrom[size_t(J)] = I;
					Heap.push_back({ Cost + Step, J });
					std::push_heap(Heap.begin(), Heap.end(), Later);
				}
			}
		}
		if (Goal < 0 && Fallback < 0)
		{
			if (AwayDamage < 0)
			{
				return false;
			}
			LastWay = AwayWay;
			OutStep = AwayStep;
			bEscapeOpen = bAwayOpen;
			return bAnyDamage || AwayDamage < int64_t(Walker.Hp);
		}
		// o caminho (centros de célula, da partida até o fim) e o pior caso nele
		const auto WayTo = [&](int32_t End)
		{
			std::vector<Vec2> Out;
			for (int32_t I = End; I >= 0 && I != First; I = EscapeFrom[size_t(I)])
			{
				Out.push_back(Map.CellCenter(Cell{ I % Map.Width, I / Map.Width }));
			}
			std::reverse(Out.begin(), Out.end());
			if (Out.empty())
			{
				Out.push_back(Map.CellCenter(Start));
			}
			return Out;
		};
		std::vector<Vec2> Way = WayTo(Goal >= 0 ? Goal : Fallback);
		bEscapeOpen = Goal >= 0;
		if (Goal >= 0 && Fallback >= 0 && Fallback != Goal && PathDamage(State, Danger, Walker, Way) >= int64_t(Walker.Hp))
		{
			// o lado aberto mata no pior caso: o bolso mais perto, se ele sobrevive até lá (medido: o Dozer da largada dentro de
			// um anel de 3 Patriots fugia pra fora do anel e morria no caminho)
			std::vector<Vec2> Pocket = WayTo(Fallback);
			if (PathDamage(State, Danger, Walker, Pocket) < int64_t(Walker.Hp))
			{
				Way = std::move(Pocket);
				bEscapeOpen = false;
			}
		}
		// o ponto de antes, se o pior caso dele não é maior (mais curto, e era o que tirava o Dozer da largada vivo)
		if (AwayDamage >= 0 && AwayDamage <= PathDamage(State, Danger, Walker, Way))
		{
			LastWay = AwayWay;
			OutStep = AwayStep;
			bEscapeOpen = bAwayOpen;
			return bAnyDamage || AwayDamage < int64_t(Walker.Hp);
		}
		LastWay = Way;
		// o passo: o ponto do caminho a até EscapeLegM dele (o jogo refaz o caminho até o ponto: a perna curta fica perto do
		// traçado)
		OutStep = Way.back();
		Fix Walked = Fix::Zero();
		Vec2 A = Walker.Pos;
		for (const Vec2& B : Way)
		{
			Walked += (B - A).Length();
			if (Walked > Fix::FromInt(EscapeLegM))
			{
				break;
			}
			OutStep = B;
			A = B;
		}
		return bAnyDamage || PathDamage(State, Danger, Walker, Way) < int64_t(Walker.Hp);
	}

	int32_t SkirmishAI::SafeStep(const World& State, const std::vector<KeepOut>& Danger, const UnitView& Walker, Vec2 To, int32_t Mobility, bool bGraze, Vec2& OutStep,
		bool bAnyDamage, bool bLegs)
	{
		// (2ª revisão de 23/10) o caminho do Dozer passava no alcance da defesa sem ninguém olhar: os dois lugares fora do
		// alcance, e o A* contornando o CC por dentro dele (medido: o 1º Dozer morria assim e o resto na porta do CC)
		const Vec2 From = Walker.Pos;
		OutStep = To;
		const std::vector<KeepOut> Live = LiveCircles(State, Danger, From, To);
		LastWay = { To };
		if (Live.empty())
		{
			return 1;
		}
		const auto Hits = [&](const std::vector<Vec2>& Way)
		{
			Vec2 A = From;
			for (const Vec2& B : Way)
			{
				for (const KeepOut& K : Live)
				{
					if (SegmentDistSqRaw(A, B, K.Pos) <= K.RadiusSq)
					{
						return true;
					}
				}
				A = B;
			}
			return false;
		};
		const SimMap& Map = State.Map();
		std::vector<Vec2> Way;
		if (!Paths.FindPath(Map, From, To, Mobility, Way) || Way.empty())
		{
			Way = { To };
		}
		// (4ª revisão de 23/10) quem está dentro de um círculo (o círculo encolheu até ele) sai "por fora" ainda no alcance: o
		// caminho por fora dos círculos também passa pelo pior caso (medido: o Dozer que parou raspando foi mandado direto pra
		// obra a 500 m, saiu pela tangente do alcance e morreu). E com folga: a defesa já está atirando nele (o pior caso dá
		// um tiro a menos que o medido: o Dozer da largada a 25 m da Patriot, mandado pra usina com 240 de 250 no pior caso,
		// morreu no caminho), então só vale com o pior caso abaixo da metade da vida
		const bool bInside = std::any_of(Danger.begin(), Danger.end(), [&](const KeepOut& K) { return DistSqRaw(K.Pos, From) <= K.RadiusSq; });
		const auto Lethal = [&](const std::vector<Vec2>& Path)
		{
			return bInside && !bAnyDamage && PathDamage(State, Danger, Walker, Path) * 2 >= int64_t(Walker.Hp);
		};
		if (!Hits(Way) && !Lethal(Way))
		{
			// (4ª revisão de 23/10) o caminho longo que passa perto de um alcance vai em pernas de RouteLegMaxM: o mundo muda no
			// meio (o CC morto abre o atalho pelo lote dele) e o jogo refaz o caminho pelo alcance (medido: o Dozer voltando do
			// depósito do meio morreu na Patriot de casa)
			LastWay = Way;
			const bool bNear = std::any_of(Live.begin(), Live.end(), [&](const KeepOut& K)
			{
				const int64_t NearSq = RadiusSqRaw(Sqrt(Fix::FromRaw(K.RadiusSq >> Fix::FracBits)) + Fix::FromInt(WorkNearM));
				Vec2 A = From;
				for (const Vec2& B : Way)
				{
					if (SegmentDistSqRaw(A, B, K.Pos) <= NearSq)
					{
						return true;
					}
					A = B;
				}
				return false;
			});
			// (a fuga, bAnyDamage, não para no meio: parar dentro do alcance mata)
			Fix Left = Fix::FromInt(RouteLegMaxM);
			Vec2 A = From;
			for (const Vec2& B : Way)
			{
				const Fix Len = (B - A).Length();
				if (bNear && bLegs && !bAnyDamage && Len > Left)
				{
					OutStep = A + (B - A) * (Left / Max(Len, Fix::FromRaw(1)));
					break;
				}
				Left -= Len;
				A = B;
			}
			return 1;
		}
		// a folga do mapa com as células dos círculos fechadas (menos Shrink: o caminho raspando, abaixo)
		const auto Around = [&](Fix Shrink)
		{
			CloseCircles(Map, Live, Shrink);
			bool bReached = false;
			return Paths.FindPath(SafeMap, From, To, Mobility, Way, &bReached) && bReached && !Way.empty();
		};
		const auto FirstStep = [&](bool bCheckGamePath)
		{
			LastWay = Way;
			OutStep = Way.back();
			for (const Vec2& P : Way)
			{
				if (DistSqRaw(P, From) > RadiusSqRaw(Fix::FromInt(RouteStepMinM)))
				{
					OutStep = P;
					break;
				}
			}
			// (4ª revisão de 23/10) o jogo faz o caminho dele até o ponto, pelo mapa sem os círculos: com o ponto longe (o caminho
			// puxado deixa uma reta de centenas de metros), ele dobrava nas pedras e cruzava o alcance (medido: o Dozer voltando do
			// depósito do meio passou entre o CC morto e a Patriot e morreu). A perna vai até RouteLegMaxM pelo caminho seguro, e
			// o caminho do jogo até ela não pode tocar nos círculos (senão a perna cai pela metade, até 3 vezes)
			Fix Leg = Fix::FromInt(RouteLegMaxM);
			for (int32_t Try = 0; Try < 4 && !bAnyDamage; ++Try)
			{
				if (DistSqRaw(OutStep, From) > RadiusSqRaw(Leg))
				{
					Fix Left = Leg;
					Vec2 A = From;
					for (const Vec2& B : Way)
					{
						const Fix Len = (B - A).Length();
						if (Len >= Left)
						{
							OutStep = A + (B - A) * (Left / Max(Len, Fix::FromRaw(1)));
							break;
						}
						Left -= Len;
						A = B;
					}
				}
				std::vector<Vec2> Game;
				if (!bCheckGamePath || !Paths.FindPath(Map, From, OutStep, Mobility, Game) || !Hits(Game))
				{
					break;
				}
				Leg = Min(Leg, (OutStep - From).Length()) / 2;
			}
		};
		if (Around(Fix::Zero()) && !Lethal(Way))
		{
			FirstStep(true);
			return 1;
		}
		// (3ª revisão de 23/10) sem caminho fora dos círculos: o menos exposto (os círculos encolhem uma célula por vez), se
		// ele sobrevive ao pior caso nele: a rajada cheia logo na entrada do alcance de verdade de cada defesa (a arma até o
		// casco dele) e o ritmo do pente pelo tempo lá dentro. Medido (Dozer reto pela Patriot, 250 de vida): a 61,5 m do
		// centro dela ou mais não leva nada (a mira e o voo do míssil passam de 2 s), a 60 m leva uma rajada (120), a 58 m
		// duas, a 56 m morre. O mais fundo só pode ser pior: o 1º caminho que sai decide
		if (!bGraze)
		{
			return 0;
		}
		for (int32_t Tier = 1; Tier <= GrazeTiers; ++Tier)
		{
			if (!Around(Map.CellSize * Tier))
			{
				continue;
			}
			if (PathDamage(State, Danger, Walker, Way) >= int64_t(Walker.Hp) && !bAnyDamage)
			{
				return 0;
			}
			FirstStep(false);
			return 2;
		}
		return 0;
	}

	int64_t SkirmishAI::PathDamage(const World& State, const std::vector<KeepOut>& Danger, const UnitView& Walker, const std::vector<Vec2>& Way)
	{
		// o pior caso de quem anda por Way a partir de onde está: de cada defesa, a rajada cheia na entrada do alcance de
		// verdade (a arma até o casco dele) e o ritmo do pente pelo tempo lá dentro
		const Ruleset& R = State.Rules();
		const UnitDef& WalkerDef = R.Units[Walker.Def];
		int64_t Damage = 0;
		for (const KeepOut& K : Danger)
		{
			if (K.Def < 0)
			{
				continue;
			}
			const UnitDef& Shooter = R.Units[size_t(K.Def)];
			const Fix Reach = Shooter.Weapon.Range + WalkerDef.Radius;
			Fix Inside = Fix::Zero();
			Vec2 A = Walker.Pos;
			for (const Vec2& B : Way)
			{
				Inside += ChordInside(A, B, K.Pos, Reach);
				A = B;
			}
			Damage += ExposureDamage(R, Shooter, WalkerDef, Inside);
		}
		return Damage;
	}

	void SkirmishAI::Think(const World& State, std::vector<Command>& OutCommands)
	{
		const TickNumber Now = State.Tick();
		if (State.IsMatchOver() || Me >= State.PlayerCount() || State.Player(Me).bDefeated || Now < NextThink)
		{
			return;
		}
		bool bBase = false;
		for (int32_t I = 0; I < State.UnitCount() && !bBase; ++I)
		{
			const UnitView V = State.Unit(I);
			bBase = V.Owner == Me && V.bStructure;
		}
		if (bBase)
		{
			NextThink = Now + TickNumber(BaseThinkTicks);
			ThinkBase(State, OutCommands);
			if (Now % TickNumber(RethinkTicks) < TickNumber(BaseThinkTicks))
			{
				Attack(State, OutCommands, true);
			}
			return;
		}
		if (Now < TickNumber(FirstWaveDelayTicks))
		{
			NextThink = TickNumber(FirstWaveDelayTicks);
			return;
		}
		NextThink = Now + TickNumber(RethinkTicks);
		Attack(State, OutCommands, false);
	}

	void SkirmishAI::ThinkBase(const World& State, std::vector<Command>& OutCommands)
	{
		const Ruleset& R = State.Rules();
		const PlayerState& Self = State.Player(Me);
		// o inimigo: a largada do primeiro jogador inimigo
		Vec2 EnemyStart = State.PlayerStart(Me);
		for (int32_t P = 0; P < State.PlayerCount(); ++P)
		{
			if (State.AreEnemies(Me, PlayerIndex(P)))
			{
				EnemyStart = State.PlayerStart(PlayerIndex(P));
				break;
			}
		}
		const Vec2 Home = State.PlayerStart(Me);

		// os papéis saem dos dados, não do nome: usina gera energia, centro recebe caixas,
		// fábrica treina quem tem arma, CC treina quem constrói
		const auto ProducesArmed = [&](const UnitDef& D)
		{
			return std::any_of(D.Produces.begin(), D.Produces.end(), [&](uint16_t K) { return R.Units[K].HasWeapon(); });
		};
		const auto ProducesBuilder = [&](const UnitDef& D)
		{
			return std::any_of(D.Produces.begin(), D.Produces.end(), [&](uint16_t K) { return R.Units[K].CanConstruct(); });
		};
		// (14/10) a defesa de base (a Patriot): o prédio que atira; a 1ª logo depois da fábrica, a 2ª quando sobra dinheiro.
		// (v4) a renda (a zona de lançamento): o prédio com avião de carga; fora da lista fixa (a regra dela vem abaixo)
		enum Role { Power, Supply, Factory, Defense, Income, RoleCount };
		const auto RoleOf = [&](const UnitDef& D) -> int32_t
		{
			if (!D.IsStructure())
			{
				return -1;
			}
			if (D.Energy > 0)
			{
				return Power;
			}
			if (D.bSupplyCenter)
			{
				return Supply;
			}
			if (D.HasWeapon())
			{
				return Defense;
			}
			if (D.HasAirlift())
			{
				return Income;
			}
			return ProducesArmed(D) ? Factory : -1;
		};

		// o que ela tem
		int32_t Have[RoleCount] = {};
		int32_t Unfinished[RoleCount] = {};
		EntityId Abandoned[RoleCount] = {};
		Vec2 AbandonedPos[RoleCount];
		uint16_t AbandonedDef[RoleCount] = {};
		// (v4) a energia contando as obras: a obra não gera nem gasta até ficar pronta, mas vai (com 2 Dozers, sem isso,
		// subiam 2 usinas pra mesma falta, e as fábricas novas ficavam sem energia quando ficavam prontas)
		int32_t MadeSoon = Self.PowerMade;
		int32_t UsedSoon = Self.PowerUsed;
		const TickNumber Now = State.Tick();
		std::vector<Site> LiveSites;
		std::vector<Site> Built;
		std::vector<UnitView> Dozers;
		int32_t Trucks = 0;
		int32_t Hauling = 0;
		EntityId CommandCenter = NoEntity;
		std::vector<UnitView> Factories;
		std::vector<UnitView> Centers;    // os centros de suprimentos prontos
		std::vector<UnitView> IdleTrucks; // os caminhões vazios esperando um depósito ao alcance
		std::vector<EntityId> Building;   // obras com Dozer trabalhando
		// (v4) o exército pela produção: tanques (armados sem alcance mínimo) e artilharia, vivos (a fila soma abaixo); e as
		// defesas inimigas prontas que ela conhece (quantos lançadores ela quer)
		int32_t TanksHave = 0;
		int32_t ArtilleryHave = 0;
		int32_t EnemyDefenses = 0;
		// (2ª revisão de 23/10) as defesas inimigas prontas, conhecidas ou não, como no SiteIsSafe e no Avoid (a escolha do
		// depósito só com as conhecidas e o lugar da obra com todas: a Patriot na névoa cobrindo o lugar do centro de casa
		// travava a base pra sempre, com $9 200 e 0 tanques)
		std::vector<UnitView> Threats;
		int32_t Completed = 0;   // (4ª revisão) os prédios dela prontos (a base andou?)
		for (int32_t I = 0; I < State.UnitCount(); ++I)
		{
			const UnitView V = State.Unit(I);
			const UnitDef& D = R.Units[V.Def];
			if (V.Owner != Me)
			{
				const bool bDefense = State.AreEnemies(Me, V.Owner) && D.IsStructure() && D.HasWeapon() && V.BuildLeft == 0;
				EnemyDefenses += bDefense && Knows(State, V) ? 1 : 0;
				if (bDefense)
				{
					Threats.push_back(V);
				}
				continue;
			}
			if (!D.IsStructure() && D.HasWeapon())
			{
				(IsArtillery(D) ? ArtilleryHave : TanksHave) += 1;
			}
			if (D.CanConstruct())
			{
				Dozers.push_back(V);
				if (V.Task == UnitTask::Construct)
				{
					Building.push_back(V.TaskTarget);
				}
			}
			Trucks += D.SupplyBoxes > 0 ? 1 : 0;
			// (4ª revisão) o caminhão no vai e vem (do depósito à entrega)
			Hauling += D.SupplyBoxes > 0 && (V.Task == UnitTask::ToDock || V.Task == UnitTask::Loading || V.Task == UnitTask::LeaveDock
				|| V.Task == UnitTask::ToCenter || V.Task == UnitTask::Unloading) ? 1 : 0;
			if (D.SupplyBoxes > 0 && V.Task == UnitTask::WaitDock && V.Cargo == 0)
			{
				IdleTrucks.push_back(V);
			}
			const int32_t Role = RoleOf(D);
			if (Role >= 0)
			{
				++Have[Role];
				if (V.BuildLeft > 0)
				{
					Abandoned[Role] = V.Id;   // conferido abaixo: só é abandonada se nenhum Dozer está nela
					AbandonedPos[Role] = V.Pos;
					AbandonedDef[Role] = V.Def;
					++Unfinished[Role];
				}
				else if (Role == Income || Role == Supply)
				{
					Built.push_back({ V.Id, Role, V.Pos, Now });   // (revisão de 23/10) a zona e o centro prontos
				}
			}
			if (D.IsStructure() && V.BuildLeft > 0)
			{
				MadeSoon += D.Energy > 0 ? D.Energy : 0;
				UsedSoon += D.Energy < 0 ? -D.Energy : 0;
				LiveSites.push_back({ V.Id, Role, V.Pos, Now });
			}
			if (D.bSupplyCenter && V.BuildLeft == 0)
			{
				Centers.push_back(V);
			}
			if (D.IsStructure() && V.BuildLeft == 0 && ProducesBuilder(D))
			{
				CommandCenter = V.Id;
			}
			if (Role == Factory && V.BuildLeft == 0)
			{
				Factories.push_back(V);
			}
			Completed += D.IsStructure() && V.BuildLeft == 0 ? 1 : 0;
		}
		// (v4) a fila das fábricas conta como já tendo (a escolta e o teto da artilharia)
		std::vector<uint16_t> Queued;
		for (const UnitView& F : Factories)
		{
			State.QueueOf(F.Id, Queued);
			for (const uint16_t K : Queued)
			{
				const UnitDef& Q = R.Units[K];
				if (!Q.IsStructure() && Q.HasWeapon())
				{
					(IsArtillery(Q) ? ArtilleryHave : TanksHave) += 1;
				}
			}
		}
		for (EntityId& A : Abandoned)
		{
			if (A != NoEntity && std::find(Building.begin(), Building.end(), A) != Building.end())
			{
				A = NoEntity;
			}
		}
		// (v4) a fundação do último pensar que sumiu inacabada morreu: o lugar fica marcado por LostSiteMemoryS
		for (const Site& S : Sites)
		{
			UnitView Still;
			if (!State.FindUnit(S.Id, Still))
			{
				LostSites.push_back({ S.Id, S.Role, S.Pos, Now });
			}
		}
		// (v4) o Dozer do último pensar que sumiu morreu: o lugar fica marcado (medido 23/10: o Dozer ia retomar a expansão no
		// meio disputado, morria, o CC fazia outro e ele ia de novo: $1 000 a cada 90 s)
		std::vector<Vec2> DiedAt;   // (4ª revisão) onde eles morreram (no alcance de uma defesa? abaixo)
		for (const Site& D : LastDozers)
		{
			UnitView Still;
			if (!State.FindUnit(D.Id, Still))
			{
				LostSites.push_back({ D.Id, DozerLostRole, D.Pos, Now });
				DozerDeaths.push_back(Now);
				DiedAt.push_back(D.Pos);
			}
		}
		DozerDeaths.erase(std::remove_if(DozerDeaths.begin(), DozerDeaths.end(), [&](TickNumber T) { return T + TickNumber(DozerDeathWindowS * TicksPerSecond) <= Now; }),
			DozerDeaths.end());
		LastDozers.clear();
		for (const UnitView& Dz : Dozers)
		{
			LastDozers.push_back({ Dz.Id, DozerLostRole, Dz.Pos, Now });
		}
		// (3ª revisão de 23/10) a obra que não anda: o Dozer com obra que não sai do lugar e a obra dele sem avançar por
		// DozerStallS. Se ele está no lote da obra de outro Dozer parado assim, larga a obra dele (o jogo só tira do lote o
		// amigo sem tarefa; a obra fica parada e o pensar seguinte o manda na parada mais perto dele). Os dois Dozers, cada um no
		// lote da obra do outro, ficavam assim pra sempre. A espera por um lote com tanque com ordem (medido em 1 200 partidas
		// IA × IA: até 260 s) não é isso, e segue como antes
		std::vector<DozerWatch> Watches;
		std::vector<std::pair<UnitView, UnitView>> Stuck;   // o Dozer parado e a obra dele
		for (const UnitView& Dz : Dozers)
		{
			UnitView Work;
			if (Dz.Task != UnitTask::Construct || !State.FindUnit(Dz.TaskTarget, Work))
			{
				continue;
			}
			DozerWatch W{ Dz.Id, Dz.TaskTarget, Dz.Pos, Work.BuildLeft, Now };
			const auto Old = std::find_if(DozerWatches.begin(), DozerWatches.end(), [&](const DozerWatch& O) { return O.Id == Dz.Id; });
			if (Old != DozerWatches.end() && Old->Target == W.Target && Old->BuildLeft == W.BuildLeft
				&& DistSqRaw(Old->Pos, Dz.Pos) <= RadiusSqRaw(Fix::FromInt(DozerStallMoveM)))
			{
				W = *Old;
				if (Now >= Old->Since + TickNumber(DozerStallS * TicksPerSecond))
				{
					Stuck.push_back({ Dz, Work });
				}
			}
			Watches.push_back(W);
		}
		// o lote: o retângulo do prédio (os lados 1 e 3 trocam X e Y); quem está a menos do raio dele está no lote
		// (World::ClaimFootprint)
		const auto OnLot = [&](Vec2 P, Fix Radius, const UnitView& Work)
		{
			const UnitDef& WD = R.Units[Work.Def];
			const bool bTurned = (Work.Dir & 1) != 0;
			const Fix DX = Max(Abs(P.X - Work.Pos.X) - (bTurned ? WD.HalfY : WD.HalfX), Fix::Zero());
			const Fix DY = Max(Abs(P.Y - Work.Pos.Y) - (bTurned ? WD.HalfX : WD.HalfY), Fix::Zero());
			return Vec2::Make(DX, DY).LengthSqRaw() < RadiusSqRaw(Radius);
		};
		std::vector<UnitView> LotSites;   // as obras dela no chão
		for (int32_t I = 0; I < State.UnitCount(); ++I)
		{
			const UnitView V = State.Unit(I);
			if (V.Owner == Me && V.bStructure && V.BuildLeft > 0)
			{
				LotSites.push_back(V);
			}
		}
		std::vector<EntityId> Stalled;
		for (const auto& [Builder, Work] : Stuck)
		{
			for (const auto& [Other, OtherWork] : Stuck)
			{
				if (Other.Id != Builder.Id && !Contains(Stalled, Other.Id) && !Contains(Stalled, Builder.Id) && OnLot(Other.Pos, R.Units[Other.Def].Radius, Work))
				{
					Stalled.push_back(Other.Id);
					Note("dozer %u off the lot of site %u (dozer %u waits)", unsigned(Other.Id), unsigned(Work.Id), unsigned(Builder.Id));
				}
			}
		}
		for (DozerWatch& W : Watches)
		{
			W.Since = Contains(Stalled, W.Id) ? Now : W.Since;
		}
		DozerWatches = std::move(Watches);
		if (!Stalled.empty())
		{
			Command G;
			G.Player = Me;
			G.Type = CommandType::SetGroup;
			G.Units = Stalled;
			OutCommands.push_back(G);
			Command X;
			X.Player = Me;
			X.Type = CommandType::Stop;
			OutCommands.push_back(X);
		}
		// (revisão de 23/10) a zona ou o centro pronto do último pensar que sumiu morreu: o lugar fica marcado do mesmo jeito
		// (antes só a fundação inacabada marcava; a zona pronta morta pela artilharia de 85 m era recolocada no mesmo lugar
		// meio segundo depois: $2 500 a cada vez). O centro de casa (no depósito mais perto da largada) não marca: o depósito
		// de casa marcado mandava o Dozer pro do meio, e ele morria no caminho (medido: renda de caminhão 0 por 160 s aos 4:20);
		// o inimigo que ainda está lá o SiteIsSafe vê
		int32_t HomeDock = -1;
		for (int32_t K = 0; K < State.DockCount(); ++K)
		{
			const DockView Dk = State.Dock(K);
			if (!Dk.bPile && (HomeDock < 0 || DistSqRaw(Dk.Pos, State.PlayerStart(Me)) < DistSqRaw(State.Dock(HomeDock).Pos, State.PlayerStart(Me))))
			{
				HomeDock = K;
			}
		}
		for (const Site& B : LastBuilt)
		{
			UnitView Still;
			const bool bHomeCenter = B.Role == Supply && HomeDock >= 0 && DistSqRaw(B.Pos, State.Dock(HomeDock).Pos) <= RadiusSqRaw(R.Economy.DockScanDistance);
			if (!State.FindUnit(B.Id, Still) && !bHomeCenter)
			{
				LostSites.push_back({ B.Id, B.Role, B.Pos, Now });
			}
		}
		LastBuilt = std::move(Built);
		// (revisão de 23/10) a obra mandada no último pensar que não está no chão: a fundação de 1 PV morreu antes deste pensar
		// (ou a obra foi recusada; marcar também só adia a próxima tentativa no mesmo lugar)
		for (const Site& P : PendingSites)
		{
			const bool bThere = std::any_of(LiveSites.begin(), LiveSites.end(), [&](const Site& S)
			{
				return S.Role == P.Role && DistSqRaw(S.Pos, P.Pos) <= RadiusSqRaw(Fix::FromInt(2));
			});
			if (!bThere)
			{
				LostSites.push_back({ NoEntity, P.Role, P.Pos, Now });
			}
		}
		PendingSites.clear();
		// (v4) a fundação nova da zona e da expansão vai pro log (a obra que de fato começou)
		for (const Site& S : LiveSites)
		{
			const bool bNew = std::none_of(Sites.begin(), Sites.end(), [&](const Site& Old) { return Old.Id == S.Id; });
			if (bNew && S.Role == Income)
			{
				Note("drop zone at %d,%d", Meters(S.Pos.X), Meters(S.Pos.Y));
			}
			if (bNew && S.Role == Supply && !Centers.empty())
			{
				Note("expand to dock %d", ExpandDock);
			}
		}
		Sites = std::move(LiveSites);
		const TickNumber LostMemory = TickNumber(LostSiteMemoryS * TicksPerSecond);
		LostSites.erase(std::remove_if(LostSites.begin(), LostSites.end(), [&](const Site& L) { return L.Tick + LostMemory <= Now; }), LostSites.end());
		Unreachable.erase(std::remove_if(Unreachable.begin(), Unreachable.end(), [&](const Site& L) { return L.Tick + LostMemory <= Now; }), Unreachable.end());

		// (v4) quantas fábricas o banco paga: uma fábrica gasta um tanque a cada obra dele; o banco que daria mais de
		// BankHorizonS segundos disso pede mais uma (o tanque e a fábrica saem dos dados: o Dozer constrói a fábrica, a
		// fábrica treina o 1º armado)
		int32_t FactoryDef = -1;
		for (const UnitView& Dz : Dozers)
		{
			for (const uint16_t K : R.Units[Dz.Def].Constructs)
			{
				FactoryDef = FactoryDef < 0 && RoleOf(R.Units[K]) == Factory ? int32_t(K) : FactoryDef;
			}
		}
		if (FactoryDef < 0 && !Factories.empty())
		{
			FactoryDef = Factories[0].Def;
		}
		int32_t TankDef = -1;
		if (FactoryDef >= 0)
		{
			for (const uint16_t K : R.Units[size_t(FactoryDef)].Produces)
			{
				if (R.Units[K].HasWeapon())
				{
					TankDef = K;
					break;
				}
			}
		}
		int32_t FactoriesWanted = 1;
		if (TankDef >= 0 && R.Units[size_t(TankDef)].BuildTicks > 0)
		{
			const UnitDef& Tank = R.Units[size_t(TankDef)];
			const int64_t PerFactory = int64_t(Tank.Cost) * BankHorizonS * TicksPerSecond / Tank.BuildTicks;
			if (PerFactory > 0)
			{
				FactoriesWanted = int32_t(std::clamp<int64_t>((int64_t(Self.Money) + PerFactory - 1) / PerFactory, 1, std::max<int32_t>(1, MaxFactories)));
			}
		}
		const int32_t DozersWanted = std::min(MaxDozers, FactoriesWanted);
		const int32_t MaxDefenses = std::min(MaxDefensesCap, DefensesBase + FactoriesWanted);
		// várias obras ao mesmo tempo só com Dozer sobrando (com 1 Dozer, como antes: nada novo com obra andando)
		const bool bDozerSpare = int32_t(Building.size()) < std::max<int32_t>(1, int32_t(Dozers.size()));

		// (revisão de 23/10) quem ergue: um Dozer vivo, ou o que o CC faz; e o centro que ele constrói (o preço, o tamanho)
		int32_t BuilderDef = Dozers.empty() ? -1 : int32_t(Dozers[0].Def);
		UnitView CCView;
		const bool bHaveCC = CommandCenter != NoEntity && State.FindUnit(CommandCenter, CCView);
		if (BuilderDef < 0 && bHaveCC)
		{
			for (const uint16_t K : R.Units[CCView.Def].Produces)
			{
				if (R.Units[K].CanConstruct())
				{
					BuilderDef = K;
					break;
				}
			}
		}
		int32_t CenterDef = -1;
		if (BuilderDef >= 0)
		{
			for (const uint16_t K : R.Units[size_t(BuilderDef)].Constructs)
			{
				if (CenterDef < 0 && RoleOf(R.Units[K]) == Supply)
				{
					CenterDef = K;
				}
			}
		}
		const int32_t CenterCost = CenterDef >= 0 ? R.Units[size_t(CenterDef)].Cost : 0;
		// (2ª revisão de 23/10) o alcance das defesas inimigas: contra a obra (a arma mede até a borda dela) e contra o Dozer
		// (o caminho dele e a porta do CC). A defesa não anda: o lugar coberto fica coberto até ela cair
		const Fix BuilderRadius = BuilderDef >= 0 ? R.Units[size_t(BuilderDef)].Radius : Fix::Zero();
		const int32_t BuilderMobility = BuilderDef >= 0 ? State.Map().ClearanceFor(R.Units[size_t(BuilderDef)].PathRadius)
			| (R.Units[size_t(BuilderDef)].CrusherLevel >= 2 ? SimMap::CrusherBit : 0) : 1;
		const auto DefenseReachSq = [&](const UnitView& T, Fix TargetRadius)
		{
			return RadiusSqRaw(R.Units[T.Def].Weapon.Range + TargetRadius + Fix::FromInt(ThreatMarginM));
		};
		const auto Covered = [&](Vec2 P, Fix TargetRadius)
		{
			return std::any_of(Threats.begin(), Threats.end(), [&](const UnitView& T) { return DistSqRaw(T.Pos, P) <= DefenseReachSq(T, TargetRadius); });
		};
		std::vector<KeepOut> DozerDanger;
		for (const UnitView& T : Threats)
		{
			DozerDanger.push_back({ T.Pos, DefenseReachSq(T, BuilderRadius), int32_t(T.Def) });
		}
		// (4ª revisão de 23/10) os Dozers mortos no alcance de uma defesa desde que a base andou (um prédio dela pronto a mais,
		// ou uma defesa inimiga a menos): a reposição para no FutileDozerDeathsMax (abaixo)
		if (Completed > LastCompleted || int32_t(Threats.size()) < LastThreats)
		{
			FutileDeaths = 0;
		}
		for (const Vec2& P : DiedAt)
		{
			FutileDeaths += Covered(P, BuilderRadius) ? 1 : 0;
		}
		LastCompleted = Completed;
		LastThreats = int32_t(Threats.size());
		// a saída do CC (onde o Dozer novo nasce: a frente do prédio, o lado X dele olha pra Dir, + o raio do Dozer + meia célula,
		// como no World::ExitPoint) e o lado de trás dele
		Vec2 CCOut;
		Vec2 CCDoor;
		if (bHaveCC)
		{
			const int32_t Side = CCView.Dir & 3;
			CCOut = Vec2::Make(Fix::FromInt(Side == 0 ? 1 : (Side == 2 ? -1 : 0)), Fix::FromInt(Side == 1 ? 1 : (Side == 3 ? -1 : 0)));
			CCDoor = CCView.Pos + CCOut * (R.Units[CCView.Def].HalfX + BuilderRadius + State.Map().CellSize / 2);
		}
		// o lugar do centro num depósito: do lado de casa, com a porta virada pra ele (o mesmo da obra, abaixo)
		const auto CenterAnchor = [&](const DockView& Dk)
		{
			if (CenterDef < 0)
			{
				return Dk.Pos;
			}
			const UnitDef& C = R.Units[size_t(CenterDef)];
			const Fix Away = Max(Dk.HalfX, Dk.HalfY) + Max(C.HalfX, C.HalfY) + R.Economy.SupplyCenterDockGap + Fix::FromInt(SpotMarginM);
			return Dk.Pos + (Home - Dk.Pos).Normalized() * Away;
		};
		// (2ª revisão de 23/10) a ida e volta do caminhão (do depósito até o centro, com a largura do depósito: as vagas e as
		// manobras ficam do lado dele) no alcance de uma defesa inimiga: o depósito não serve (medido: a Patriot a 65-72 m do
		// depósito de casa e fora do alcance do lugar do centro matava os caminhões um a um, a 11 m da linha reta; a base
		// ficava com 3 tanques e $0 nos 8 min)
		Fix TruckRadius = Fix::Zero();
		int32_t TruckCost = 0;
		if (CenterDef >= 0)
		{
			for (const uint16_t K : R.Units[size_t(CenterDef)].Produces)
			{
				TruckRadius = R.Units[K].SupplyBoxes > 0 ? R.Units[K].Radius : TruckRadius;
				TruckCost = R.Units[K].SupplyBoxes > 0 ? R.Units[K].Cost : TruckCost;
			}
		}
		const auto LaneCovered = [&](const DockView& Dk, Vec2 CenterPos)
		{
			const Fix Width = TruckRadius + Max(Dk.HalfX, Dk.HalfY);
			return std::any_of(Threats.begin(), Threats.end(), [&](const UnitView& T) { return SegmentDistSqRaw(Dk.Pos, CenterPos, T.Pos) <= DefenseReachSq(T, Width); });
		};

		// o centro vivo: o que ainda tem depósito com caixa ao alcance do caminhão (sem nenhum, a base secou). (2ª revisão)
		// O depósito com a ida e volta do caminhão no alcance de uma defesa inimiga não conta
		const int64_t ScanSq = RadiusSqRaw(R.Economy.DockScanDistance);
		const auto DockInReach = [&](Vec2 P)
		{
			for (int32_t K = 0; K < State.DockCount(); ++K)
			{
				const DockView Dk = State.Dock(K);
				if (Dk.Boxes > 0 && DistSqRaw(Dk.Pos, P) <= ScanSq && !LaneCovered(Dk, P))
				{
					return true;
				}
			}
			return false;
		};
		EntityId Center = NoEntity;
		int32_t CenterQueue = 0;
		int32_t LiveCenters = 0;
		for (const UnitView& C : Centers)
		{
			if (DockInReach(C.Pos))
			{
				if (Center == NoEntity)
				{
					Center = C.Id;
					CenterQueue = C.QueueSize;
				}
				++LiveCenters;
			}
		}
		const bool bDry = !Centers.empty() && LiveCenters == 0;

		// (v4) a expansão cedo: o que sobra nos depósitos ao alcance dos centros vivos contra o que eles tinham no começo; e o
		// depósito da expansão: o com caixa mais perto de casa fora do alcance de todo centro, do lado de casa primeiro (o 1º
		// centro, sem nenhum, vai no de casa; a base seca, no do meio do lado dela)
		int64_t ReachBoxes = 0;
		int64_t ReachStart = 0;
		ExpandDock = -1;
		{
			// (v4) o depósito disputado fica pro fim: inimigo conhecido armado (ou a base dele) a até DockThreatM, ou um centro
			// dela que morreu ali há pouco (medido 23/10: a expansão no meio morria na passagem das ondas e ela insistia no
			// mesmo lugar; a base secava e a renda ficava só no avião)
			const auto DockThreatened = [&](Vec2 P)
			{
				for (const Site& L : LostSites)
				{
					if ((L.Role == Supply || L.Role == DozerLostRole) && DistSqRaw(L.Pos, P) <= RadiusSqRaw(Fix::FromInt(DockLostRadiusM)))
					{
						return true;
					}
				}
				const int64_t ThreatSq = RadiusSqRaw(Fix::FromInt(DockThreatM));
				for (int32_t I = 0; I < State.UnitCount(); ++I)
				{
					const UnitView V = State.Unit(I);
					if (State.AreEnemies(Me, V.Owner) && (V.bStructure || R.Units[V.Def].HasWeapon()) && DistSqRaw(V.Pos, P) <= ThreatSq && Knows(State, V))
					{
						return true;
					}
				}
				return false;
			};
			// a ordem: seguro antes de disputado, o lado de casa antes do do inimigo, o mais perto de casa
			int64_t BestKey = 0;
			int64_t BestD2 = 0;
			for (int32_t K = 0; K < State.DockCount(); ++K)
			{
				const DockView Dk = State.Dock(K);
				if (Dk.bPile)
				{
					continue;
				}
				bool bReach = false;
				bool bLiveReach = false;
				for (const UnitView& C : Centers)
				{
					const bool bIn = DistSqRaw(Dk.Pos, C.Pos) <= ScanSq;
					bReach |= bIn;
					bLiveReach |= bIn && DockInReach(C.Pos);
				}
				if (bLiveReach)
				{
					ReachBoxes += Dk.Boxes;
					ReachStart += Dk.StartBoxes;
				}
				if (bReach || Dk.Boxes <= 0)
				{
					continue;
				}
				// (2ª revisão de 23/10) o depósito com o lugar do centro no alcance de uma defesa (a obra nunca sai: o SiteIsSafe
				// recusa), ou aonde o Dozer não chega sem passar no alcance dela (Unreachable), não entra: a escolha passa pro
				// seguinte. Sem nenhum, o 1º centro vai pra perto de casa, fora do alcance (sem renda, mas abre a War Factory: os
				// tanques limpam o depósito)
				const bool bUnreachable = std::any_of(Unreachable.begin(), Unreachable.end(), [&](const Site& U)
				{
					return U.Role == Supply && DistSqRaw(U.Pos, Dk.Pos) <= RadiusSqRaw(Fix::FromInt(DockLostRadiusM));
				});
				if (bUnreachable || (CenterDef >= 0 && (Covered(CenterAnchor(Dk), R.Units[size_t(CenterDef)].Radius) || LaneCovered(Dk, CenterAnchor(Dk)))))
				{
					continue;
				}
				const int64_t Key = (DockThreatened(Dk.Pos) ? 2 : 0) + (DistSqRaw(Dk.Pos, Home) > DistSqRaw(Dk.Pos, EnemyStart) ? 1 : 0);
				const int64_t D2 = DistSqRaw(Dk.Pos, Home);
				if (ExpandDock < 0 || Key < BestKey || (Key == BestKey && D2 < BestD2))
				{
					ExpandDock = K;
					BestKey = Key;
					BestD2 = D2;
				}
			}
		}
		const bool bRunningLow = LiveCenters > 0 && ReachBoxes * 1000 < int64_t(ExpandAtPermille) * ReachStart;
		// (4ª revisão de 23/10) o exército não cresce mais: nenhuma zona, menos que o preço do armado, nenhum centro ou zona em
		// obra (a expansão paga: medido, a base seca soltava os tanques enquanto o centro do meio subia) e nenhum centro vivo
		// nem caminhão trabalhando (o do depósito coberto ainda entrega até morrer: medido, o tanque saía sozinho), ou centro
		// vivo sem caminhão nenhum (nem na fila) e sem o preço de um (medido: o anel de 4 Patriots, a 1ª onda matou as 2 do
		// depósito, o caminhão já tinha morrido e os 4 tanques ficaram em casa com $0)
		const bool bNoTrucks = Trucks == 0 && Self.Money < TruckCost && std::none_of(Centers.begin(), Centers.end(), [](const UnitView& C) { return C.QueueSize > 0; });
		const bool bNoHauling = LiveCenters == 0 && Hauling == 0;
		bArmyCapped = (bNoHauling || bNoTrucks) && Have[Income] == 0 && Unfinished[Supply] == 0 && Unfinished[Income] == 0
			&& (TankDef < 0 || Self.Money < R.Units[size_t(TankDef)].Cost);

		// (2ª revisão de 23/10) o Dozer à toa dentro do alcance de uma defesa (o novo que nasceu na porta do CC coberta, ou o que
		// parou ali) sai: pra fora do alcance da mais perto, pelo caminho que não entra mais nele (medido: o Dozer novo esperava
		// na porta sem obra e morria lá; 8 em 8 min com $30 000)
		// (3ª revisão de 23/10) menos o que está passando raspando (parou num ponto do caminho dentro do círculo: fugir dali o
		// mandava de volta pro bolso, ou pela beira de dentro do alcance; medido, um Dozer morto assim). Ele segue marcado
		// enquanto anda até o ponto; parado, só se o SendDozer abaixo o mandar raspando de novo
		EntityId Grazing = NoEntity;
		for (const UnitView& D : Dozers)
		{
			Grazing = D.Id == GrazeWalker && D.Task == UnitTask::None ? D.Id : Grazing;
		}
		GrazeWalker = NoEntity;
		for (const UnitView& D : Dozers)
		{
			GrazeWalker = D.Id == Grazing && D.Order != OrderType::Idle ? Grazing : GrazeWalker;
		}
		// (4ª revisão de 23/10) quem está à toa no alcance foge primeiro (EscapeStep: a saída mais curta do alcance onde está e,
		// sem ela, o caminho menos exposto pro lado aberto)
		// (e o que já está fugindo, perto do fim da perna, ainda no alcance: a próxima perna sai antes dele parar; medido, o
		// Dozer da largada dentro de 3 alcances parava duas vezes no caminho e morria a 1 s da saída que a 2ª revisão achava)
		std::vector<EntityId> InDanger;
		FleeTo.erase(std::remove_if(FleeTo.begin(), FleeTo.end(), [&](const std::pair<EntityId, Vec2>& F)
		{
			return std::none_of(Dozers.begin(), Dozers.end(), [&](const UnitView& D) { return D.Id == F.first && D.Task == UnitTask::None && D.Order == OrderType::Move; });
		}), FleeTo.end());
		for (const UnitView& D : Dozers)
		{
			bool bLegEnd = false;
			for (const std::pair<EntityId, Vec2>& F : FleeTo)
			{
				bLegEnd |= F.first == D.Id && D.Order == OrderType::Move && DistSqRaw(D.Pos, F.second) <= RadiusSqRaw(Fix::FromInt(FleeAheadM));
			}
			if (D.Task == UnitTask::None && (D.Order == OrderType::Idle || bLegEnd) && D.Id != Grazing && Covered(D.Pos, BuilderRadius))
			{
				InDanger.push_back(D.Id);
			}
		}
		std::vector<EntityId> Fleeing;
		const auto Flee = [&](const UnitView& D)
		{
			const UnitView* Near = nullptr;
			for (const UnitView& T : Threats)
			{
				if (DistSqRaw(T.Pos, D.Pos) <= DefenseReachSq(T, BuilderRadius) && (Near == nullptr || DistSqRaw(T.Pos, D.Pos) < DistSqRaw(Near->Pos, D.Pos)))
				{
					Near = &T;
				}
			}
			if (Near == nullptr)
			{
				return;
			}
			// (4ª revisão de 23/10) a saída: o caminho menos exposto pro lado aberto (EscapeStep; o ponto logo fora do alcance da
			// defesa mais perto, abaixo, cruzava o alcance de outra: os Dozers novos da porta coberta morriam assim, aos pares)
			Vec2 Step;
			if (!EscapeStep(State, DozerDanger, D, BuilderMobility, true, Step) || DistSqRaw(Step, D.Pos) <= RadiusSqRaw(Fix::FromInt(RouteStepMinM)))
			{
				// sem nenhuma: o ponto de fuga logo fora do alcance, a partir do lado oposto à defesa e girando de 20 em 20° pros
				// dois lados até um lugar andável fora de todo alcance (direto pra trás, a porta do CC dava no meio do próprio CC)
				const Fix Reach = R.Units[Near->Def].Weapon.Range + BuilderRadius + Fix::FromInt(ThreatMarginM);
				const int32_t Base = int32_t((D.Pos - Near->Pos).Heading().Bam);
				Vec2 Away = Near->Pos + (D.Pos - Near->Pos).Normalized() * (Reach + Fix::FromInt(2 * SpotMarginM));
				for (int32_t K = 0; K <= 16; ++K)
				{
					const int32_t Turn = (K + 1) / 2 * (K % 2 == 0 ? -1 : 1) * Angle::FullTurn / 18;
					const Vec2 P = Near->Pos + Vec2::FromAngle(Angle::FromBam(Base + Turn)) * (Reach + Fix::FromInt(2 * SpotMarginM));
					const Cell C = State.Map().CellOf(P);
					if (State.Map().IsWalkable(C.X, C.Y, BuilderMobility) && !Covered(P, BuilderRadius))
					{
						Away = P;
						break;
					}
				}
				Step = Away;
				SafeStep(State, DozerDanger, D, Away, BuilderMobility, false, Step);   // sem caminho fora: direto pro ponto de fuga
			}
			Command G;
			G.Player = Me;
			G.Type = CommandType::SetGroup;
			G.Units = { D.Id };
			OutCommands.push_back(G);
			Command M;
			M.Player = Me;
			M.Type = CommandType::Move;
			M.Point = Step;
			OutCommands.push_back(M);
			Fleeing.push_back(D.Id);
			FleeTo.erase(std::remove_if(FleeTo.begin(), FleeTo.end(), [&](const std::pair<EntityId, Vec2>& F) { return F.first == D.Id; }), FleeTo.end());
			FleeTo.push_back({ D.Id, Step });
		};
		// o Dozer à toa (3ª revisão: o que está passando raspando primeiro, pra seguir o caminho). (4ª revisão) Os à toa na
		// ordem em que tentam a obra: o raspando, depois o de mais vida (o Dozer ferido na porta coberta não passa raspando
		// em lugar nenhum; o novo, inteiro, passa), desempate pela ordem
		for (const UnitView& D : Dozers)
		{
			if (Contains(InDanger, D.Id))
			{
				Flee(D);
			}
		}
		std::vector<const UnitView*> FreeDozers;
		for (const UnitView& D : Dozers)
		{
			if (D.Task == UnitTask::None && D.Order == OrderType::Idle && !Contains(Fleeing, D.Id))
			{
				FreeDozers.push_back(&D);
			}
		}
		// (com um lugar escolhido, o mais perto dele: o Dozer novo chegou no depósito do meio e o outro, em casa, foi mandado
		// raspando pra lá, a 500 m; e o ferido com o lugar fazia a volta enquanto os inteiros esperavam no lote)
		std::stable_sort(FreeDozers.begin(), FreeDozers.end(), [&](const UnitView* A, const UnitView* B)
		{
			if ((A->Id == Grazing) != (B->Id == Grazing))
			{
				return A->Id == Grazing;
			}
			if (PlanRole >= 0)
			{
				return DistSqRaw(A->Pos, PlanSpot) < DistSqRaw(B->Pos, PlanSpot);
			}
			return A->Hp > B->Hp;
		});
		const UnitView* Free = FreeDozers.empty() ? nullptr : FreeDozers[0];

		// a defesa só entra na lista se algum Dozer sabe fazer uma (outra facção pode não ter)
		bool bCanDefend = false;
		// (v4) a zona: o prédio de renda que um Dozer sabe fazer
		int32_t ZoneDef = -1;
		for (const UnitView& Dz : Dozers)
		{
			for (const uint16_t K : R.Units[Dz.Def].Constructs)
			{
				bCanDefend |= RoleOf(R.Units[K]) == Defense;
				ZoneDef = ZoneDef < 0 && RoleOf(R.Units[K]) == Income ? int32_t(K) : ZoneDef;
			}
		}
		// (v4) a zona de lançamento (usa_drop_zone, ai_rule): a 1ª com a fábrica pronta e depois de IncomeFirstAfterS ou da
		// 1ª onda; a 2ª depois de IncomeSecondAfterS ou com a base seca; nunca com outra em obra nem acima do teto dela.
		// Reconstrói pela mesma regra (o lugar da que morreu fica marcado pelo LostSites)
		bool bZoneWanted = false;
		int32_t ZoneMoney = 0;
		if (ZoneDef >= 0 && !Factories.empty() && Unfinished[Income] == 0 && State.HasPrerequisites(Me, uint16_t(ZoneDef)))   // (o ZoneDef sai de um Dozer vivo)
		{
			const UnitDef& Z = R.Units[size_t(ZoneDef)];
			if (Have[Income] == 0)
			{
				bZoneWanted = Now >= TickNumber(IncomeFirstAfterS * TicksPerSecond) || bWaveSent;
			}
			else if (Have[Income] == 1)
			{
				bZoneWanted = Now >= TickNumber(IncomeSecondAfterS * TicksPerSecond) || bDry;
			}
			bZoneWanted &= Z.MaxPerPlayer <= 0 || Have[Income] < Z.MaxPerPlayer;
			ZoneMoney = Z.Cost + IncomeReserve;
		}
		// a janela de economizar: a fábrica para enquanto falta dinheiro pra zona, no máximo IncomeSaveMaxS; depois gasta
		// normal por IncomeSaveRetryS e abre outra (o mesmo jeito do bSaveForCenter)
		bool bSaveForZone = false;
		if (bZoneWanted && Self.Money < ZoneMoney)
		{
			if (!bIncomeSaving || Now >= IncomeSaveSince + TickNumber((IncomeSaveMaxS + IncomeSaveRetryS) * TicksPerSecond))
			{
				bIncomeSaving = true;
				IncomeSaveSince = Now;
			}
			bSaveForZone = Now < IncomeSaveSince + TickNumber(IncomeSaveMaxS * TicksPerSecond);
		}
		else if (!bZoneWanted && ZoneDef >= 0)
		{
			// (revisão de 23/10) só quem não quer mais a zona fecha a janela: sem Dozer vivo o ZoneDef some (ele sai do Dozer), e
			// o Dozer que morria no meio da janela abria outra inteira assim que o novo saía (medido: 120 s de 124 parada)
			bIncomeSaving = false;
		}

		// a próxima obra da lista fixa (usina → centro → fábrica → defesa)
		int32_t Next = -1;
		for (int32_t Role = 0; Role < Income && Next < 0; ++Role)
		{
			Next = Have[Role] == 0 && (Role != Defense || bCanDefend) ? Role : -1;
		}
		// a base secou, ou (v4) vai secar: o centro novo vai pro depósito da expansão (a escolha abaixo)
		if (Next < 0 && (bDry || bRunningLow) && ExpandDock >= 0 && Have[Supply] < MaxSupplyCenters && Unfinished[Supply] == 0 && bDozerSpare)
		{
			Next = Supply;
		}
		// (v4) a zona, com o dinheiro dela e de um tanque
		if (Next < 0 && bZoneWanted && Self.Money >= ZoneMoney && bDozerSpare)
		{
			Next = Income;
		}
		// (v4) a fábrica a mais: a base pronta, o banco pede, e sobra pra ela e 2 tanques. A fábrica e a defesa a mais não
		// gastam o dinheiro guardado pra zona (medido 23/10: a 2ª Patriot comia a economia da zona aos $3 000, e a zona nunca
		// saía)
		const int32_t ZoneHeld = bZoneWanted ? ZoneMoney : 0;
		if (Next < 0 && TankDef >= 0 && Have[Factory] < FactoriesWanted && Abandoned[Factory] == NoEntity && bDozerSpare
			&& Self.Money >= ZoneHeld + R.Units[size_t(FactoryDef)].Cost + 2 * R.Units[size_t(TankDef)].Cost)
		{
			Next = Factory;
		}
		// o tipo que o Dozer faria pro papel (o primeiro da lista dele que o jogador já pode)
		const auto DefFor = [&](int32_t Want, const UnitView& Dz)
		{
			for (const uint16_t K : R.Units[Dz.Def].Constructs)
			{
				if (RoleOf(R.Units[K]) == Want && State.HasPrerequisites(Me, K))
				{
					return int32_t(K);
				}
			}
			return -1;
		};
		// a defesa a mais com dinheiro sobrando. (v4) A casa que acabou de ser atacada (o alarme da DefendHome terminou há
		// menos de DefenseAfterAlertS) põe a próxima com o preço dela, guardando no máximo DefenseSaveMaxS (medido 23/10: o
		// dinheiro vive abaixo de $1 000, a fábrica compra a cada $900, e a 2ª defesa com $3 000 sobrando nunca saía; as
		// bases caíam juntas na 2ª onda)
		const int32_t DefenseDef = bCanDefend && !Dozers.empty() ? DefFor(Defense, Dozers[0]) : -1;
		const bool bAfterAlert = AlertEndedAt > 0 && Now < AlertEndedAt + TickNumber(DefenseAfterAlertS * TicksPerSecond);
		const bool bDefenseWanted = DefenseDef >= 0 && Have[Defense] < MaxDefenses && Unfinished[Defense] == 0;
		const int32_t DefenseMoney = ZoneHeld + (bAfterAlert && DefenseDef >= 0 ? R.Units[size_t(DefenseDef)].Cost : SecondDefenseMoney);
		const bool bSaveForDefense = bDefenseWanted && bAfterAlert && Self.Money < DefenseMoney
			&& Now < AlertEndedAt + TickNumber(DefenseSaveMaxS * TicksPerSecond);
		if (Next < 0 && bDefenseWanted && Self.Money >= DefenseMoney && bDozerSpare)
		{
			Next = Defense;
		}
		if (Unfinished[Defense] > 0)
		{
			AlertEndedAt = 0;   // a defesa nova saiu: o próximo alarme pede a seguinte
		}
		// uma usina antes quando a energia não fecha. (v4) Olhando a obra que vem: ela pede o que gasta (a Patriot e a zona
		// gastam 3; antes a conta supunha 1 e subiam sem energia); sem obra na vez, 1 de folga, como antes
		if (Next != Power && (Next >= 0 || bDozerSpare) && !Dozers.empty())
		{
			const int32_t NextDef = Next >= 0 ? DefFor(Next, Dozers[0]) : -1;
			const int32_t Need = NextDef >= 0 ? std::max<int32_t>(1, -R.Units[size_t(NextDef)].Energy) : 1;
			if (UsedSoon + Need > MadeSoon)
			{
				Next = Power;
			}
		}
		// (v4) o lugar da obra nova: sem inimigo armado perto e longe de onde uma do mesmo papel acabou de morrer (ou um Dozer).
		// (revisão de 23/10) "Perto" é o alcance de verdade contra a obra: o da arma + o raio dela + ThreatMarginM (a unidade
		// anda: pelo menos ThreatRadiusM). Conta todo inimigo armado, como antes: só os que ela conhece (a unidade à vista)
		// deixava o centro da expansão cair no tanque escondido na névoa (medido em 20 partidas: 23 centros perdidos pra
		// tanque contra 7)
		const auto ThreatReachSq = [&](const UnitView& V, uint16_t SiteDef)
		{
			const UnitDef& VD = R.Units[V.Def];
			Fix Reach = VD.Weapon.Range + R.Units[SiteDef].Radius + Fix::FromInt(ThreatMarginM);
			if (!VD.IsStructure())
			{
				Reach = Max(Reach, Fix::FromInt(ThreatRadiusM));
			}
			return RadiusSqRaw(Reach);
		};
		const auto IsThreat = [&](const UnitView& V)
		{
			return State.AreEnemies(Me, V.Owner) && V.BuildLeft == 0 && R.Units[V.Def].HasWeapon();
		};
		const auto SiteIsSafe = [&](Vec2 P, int32_t Role, uint16_t SiteDef)
		{
			for (const Site& L : LostSites)
			{
				if ((L.Role == Role && DistSqRaw(L.Pos, P) <= RadiusSqRaw(Fix::FromInt(LostSiteRadiusM)))
					|| (L.Role == DozerLostRole && DistSqRaw(L.Pos, P) <= RadiusSqRaw(Fix::FromInt(DockLostRadiusM))))
				{
					return false;
				}
			}
			for (int32_t I = 0; I < State.UnitCount(); ++I)
			{
				const UnitView V = State.Unit(I);
				if (IsThreat(V) && DistSqRaw(V.Pos, P) <= ThreatReachSq(V, SiteDef))
				{
					return false;
				}
			}
			return true;
		};
		// (v4) a obra parada num lugar perigoso (inimigo armado perto, ou um Dozer morreu ali há pouco) é cancelada: o dinheiro
		// volta inteiro, e a fundação de 1 PV morreria de qualquer jeito. O lugar fica marcado pelo LostSites
		for (int32_t Role = 0; Role < RoleCount; ++Role)
		{
			if (Abandoned[Role] != NoEntity && !SiteIsSafe(AbandonedPos[Role], Role, AbandonedDef[Role]))
			{
				Command X;
				X.Player = Me;
				X.Type = CommandType::CancelBuild;
				X.Target = Abandoned[Role];
				OutCommands.push_back(X);
				Abandoned[Role] = NoEntity;
			}
		}
		// (2ª revisão de 23/10) o Dozer vai pelo caminho seguro: direto, se o caminho do jogo não passa no alcance de uma
		// defesa; senão só até o 1º ponto do desvio (o próximo pensar segue dali, com ele à toa de novo). Devolve 0 = não há
		// caminho fora do alcance (nada mandado), 1 = foi pro desvio, 2 = foi direto (Then mandado). (3ª revisão) Sem caminho
		// fora dos alcances há LostSiteMemoryS (todo lugar da procura já foi tentado e marcado; a espera só recomeça com uma
		// obra alcançada por fora dos alcances, ou sem defesa inimiga), o caminho raspando vale; e vale pro Dozer que já está
		// raspando
		EntityId Sent = NoEntity;
		const auto SendDozer = [&](const UnitView& Dz, Vec2 Target, const Command& Then)
		{
			const bool bGraze = Dz.Id == Grazing || (RouteBlockedSince > 0 && Now >= RouteBlockedSince + TickNumber(LostSiteMemoryS * TicksPerSecond));
			Vec2 Step;
			const bool bLegs = Then.Type != CommandType::Build || DistSqRaw(Dz.Pos, Target) > RadiusSqRaw(Fix::FromInt(BuildLegMinM));
			const int32_t Route = SafeStep(State, DozerDanger, Dz, Target, BuilderMobility, bGraze, Step, false, bLegs);
			if (Route == 0)
			{
				RouteBlockedSince = RouteBlockedSince > 0 ? RouteBlockedSince : std::max<TickNumber>(1, Now);
				return 0;
			}
			if (Route == 2)
			{
				GrazeWalker = Dz.Id;
				if (Grazing != Dz.Id)
				{
					Note("dozer %u grazing to %d,%d", unsigned(Dz.Id), Meters(Target.X), Meters(Target.Y));
				}
			}
			Sent = Dz.Id;
			Command G;
			G.Player = Me;
			G.Type = CommandType::SetGroup;
			G.Units = { Dz.Id };
			OutCommands.push_back(G);
			if (DistSqRaw(Step, Target) > RadiusSqRaw(Fix::FromInt(RouteStepMinM)))
			{
				Command M;
				M.Player = Me;
				M.Type = CommandType::Move;
				M.Point = Step;
				OutCommands.push_back(M);
				return 1;
			}
			OutCommands.push_back(Then);
			if (Route == 1 && Then.Type == CommandType::Build)
			{
				RouteBlockedSince = 0;   // chegou pra obra por fora dos alcances (o passo do desvio e a ida ver o depósito não contam)
			}
			return 2;
		};
		EntityId Resumed = NoEntity;
		// (4ª revisão) a obra parada também quando a da vez ainda não pode (o centro parado e a fábrica da vez sem ele: a base
		// ficava parada pra sempre)
		if (Free != nullptr && (Next < 0 || DefFor(Next, *Free) < 0))
		{
			// (3ª revisão de 23/10) a parada mais perto do Dozer (antes, a do 1º papel: os dois Dozers que largaram as obras
			// trocadas iam cada um pra mais longe, cruzando de novo)
			int32_t Nearest = -1;
			for (int32_t Role = 0; Role < RoleCount; ++Role)
			{
				if (Abandoned[Role] != NoEntity
					&& (Nearest < 0 || DistSqRaw(AbandonedPos[Role], Free->Pos) < DistSqRaw(AbandonedPos[Nearest], Free->Pos)))
				{
					Nearest = Role;
				}
			}
			for (int32_t Role = 0; Role < RoleCount; ++Role)
			{
				if (Role == Nearest)
				{
					// retoma a obra parada antes de começar outra. (2ª revisão) Sem caminho fora do alcance das defesas, desiste
					// dela (o dinheiro volta) e a procura passa do lugar
					Command B;
					B.Player = Me;
					B.Type = CommandType::Build;
					B.Target = Abandoned[Role];
					if (SendDozer(*Free, AbandonedPos[Role], B) == 0)
					{
						Command X;
						X.Player = Me;
						X.Type = CommandType::CancelBuild;
						X.Target = Abandoned[Role];
						OutCommands.push_back(X);
						Unreachable.push_back({ NoEntity, Role, AbandonedPos[Role], Now });
					}
					Resumed = Free->Id;
					Free = nullptr;
					break;
				}
			}
		}
		// (4ª revisão de 23/10, a base cercada) o lugar e o caminho juntos: a procura só aceita o lote aonde o Dozer chega (a
		// região dele na folga com os alcances fechados, FloodReach): por fora dos alcances primeiro e, sem nenhum por
		// LostSiteMemoryS, raspando (os círculos uma célula menores por vez, até GrazeTiers; o SafeStep confere o pior caso). Em
		// cada nível, o lote à vista (a obra) antes do lote na névoa (o Dozer vai lá ver: o CanPlace só aceita o lote à vista, e
		// atrás do cerco nada está à vista); a casa, depois o prédio dela mais perto do Dozer, depois o próprio Dozer (o centro
		// também, sem depósito). Antes, o lugar saía sem olhar o caminho, ia pro Unreachable e a procura girava um lugar por
		// pensar; com todos os lugares atrás de um alcance, a base parava pra sempre (medido: base no canto entre 2 Patriots,
		// 30 min com $7 700 a $9 200 e 0 tanques). O lugar mandado fica (Plan) enquanto o Dozer vai até ele, um passo por pensar
		const SimMap& BaseMap = State.Map();
		const auto TryPlace = [&](const UnitView& Dz, bool bSend) -> int32_t   // 0 = nada, 1 = mandou (sem bSend: achou), 2 = inimigo perto do lugar
		{
			const int32_t Def = DefFor(Next, Dz);
			if (Def < 0 || Self.Money < R.Units[size_t(Def)].Cost)
			{
				return 0;
			}
			const UnitDef& D = R.Units[size_t(Def)];
			Vec2 Anchor = Home;
			int32_t Dir = DirToward(Home, EnemyStart);
			Fix MinRadius = Fix::FromInt(30 + SpotMarginM);
			Angle Start = (EnemyStart - Home).Heading();
			if (Next == Defense)
			{
				// a defesa na frente da base, rumo ao inimigo (a 1ª no meio, a 2ª de lado: o FindSpot acha o livre)
				Anchor = Home + (EnemyStart - Home).Normalized() * Fix::FromInt(DefenseAheadM);
				MinRadius = Fix::Zero();
			}
			if (Next == Income)
			{
				// (v4) a zona atrás do CC, do lado oposto ao inimigo (dentro da base, coberta pela defesa da frente);
				// a espiral começa virada pra trás
				Anchor = Home - (EnemyStart - Home).Normalized() * Fix::FromInt(IncomeBehindM);
				MinRadius = Fix::Zero();
				Start = (Home - EnemyStart).Heading();
			}
			const bool bAtDock = Next == Supply && ExpandDock >= 0;
			if (bAtDock)
			{
				// o centro do lado de casa do depósito, com a porta virada pra ele
				const DockView Dk = State.Dock(ExpandDock);
				const Fix Away = Max(Dk.HalfX, Dk.HalfY) + Max(D.HalfX, D.HalfY) + R.Economy.SupplyCenterDockGap + Fix::FromInt(SpotMarginM);
				Anchor = Dk.Pos + (Home - Dk.Pos).Normalized() * Away;
				Dir = DirToward(Anchor, Dk.Pos);
				MinRadius = Fix::Zero();
			}
			// (revisão de 23/10) a defesa inimiga não anda: a espiral pula o alcance dela (com o alcance maior do
			// SiteIsSafe, sem isso a 1ª usina no alcance de uma Patriot avançada travava a base inteira). (2ª revisão) O
			// centro também: o depósito com o lugar dele coberto já ficou fora da escolha, e o de perto de casa (sem
			// depósito livre) precisa pular. E os lugares do mesmo papel sem caminho seguro, há pouco (Unreachable)
			std::vector<KeepOut> Avoid;
			for (const UnitView& T : Threats)
			{
				Avoid.push_back({ T.Pos, ThreatReachSq(T, uint16_t(Def)) });
			}
			for (const Site& U : Unreachable)
			{
				if (U.Role == Next)
				{
					Avoid.push_back({ U.Pos, RadiusSqRaw(Fix::FromInt(UnreachableRadiusM)) });
				}
			}
			// (4ª revisão) e o lugar onde uma do mesmo papel morreu, ou a obra mandada não apareceu, ou um Dozer morreu, há pouco
			// (a mesma régua do SiteIsSafe): a espiral segue pro seguinte em vez de esperar o LostSites esquecer (medido: a obra
			// no lote do CC morto, recusada pelo jogo, mandada de novo a cada 30 s com $47 000 no banco; e a 1ª usina esperando
			// 30 s o Dozer morto a 50 m, com o CC no alcance de duas Patriots caindo aos 61 s)
			for (const Site& L : LostSites)
			{
				if (L.Role == Next || L.Role == DozerLostRole)
				{
					Avoid.push_back({ L.Pos, RadiusSqRaw(Fix::FromInt(L.Role == Next ? LostSiteRadiusM : DockLostRadiusM)) });
				}
			}
			// (3ª revisão de 23/10) nada em volta de casa (o alcance de uma defesa, e a névoa: com o CC morto ninguém vê a casa):
			// perto do prédio dela mais perto do Dozer e, sem lugar ali, perto do próprio Dozer. Medido: o centro no depósito do
			// meio, o CC morto pela Patriot a 65 m da largada e a War Factory sem lugar em volta de casa, 588 s parada com o
			// dinheiro subindo a $24 800. (4ª revisão) O centro sem depósito (perto de casa) também
			struct Try
			{
				Vec2 Anchor;
				int32_t Dir = 0;
				Fix MinRadius;
				Angle Start;
			};
			std::vector<Try> Tries = { { Anchor, Dir, MinRadius, Start } };
			if (!bAtDock)
			{
				bool bNear = false;
				Vec2 Near;
				for (int32_t I = 0; I < State.UnitCount(); ++I)
				{
					const UnitView V = State.Unit(I);
					if (V.Owner == Me && V.bStructure && V.BuildLeft == 0 && (!bNear || DistSqRaw(V.Pos, Dz.Pos) < DistSqRaw(Near, Dz.Pos)))
					{
						Near = V.Pos;
						bNear = true;
					}
				}
				if (bNear)
				{
					Tries.push_back({ Near, DirToward(Near, EnemyStart), Fix::Zero(), (EnemyStart - Near).Heading() });
				}
				Tries.push_back({ Dz.Pos, DirToward(Dz.Pos, EnemyStart), Fix::Zero(), (EnemyStart - Dz.Pos).Heading() });
			}
			const bool bGrazeOk = (Dz.Id != NoEntity && Dz.Id == Grazing) || (RouteBlockedSince > 0 && Now >= RouteBlockedSince + TickNumber(LostSiteMemoryS * TicksPerSecond));
			const int32_t FirstTier = Threats.empty() ? -1 : 0;
			const int32_t LastTier = Threats.empty() ? -1 : (bGrazeOk ? GrazeTiers : 0);
			for (int32_t Tier = FirstTier; Tier <= LastTier; ++Tier)
			{
				const std::vector<uint8_t>* Reach = nullptr;
				if (Tier >= 0)
				{
					if (FloodReach(State, DozerDanger, Dz.Pos, BuilderMobility, Tier) == 0)
					{
						continue;
					}
					Reach = &ReachMark;
				}
				Vec2 Spot;
				int32_t SpotDir = Dir;
				bool bFound = false;
				// o lugar que ele já está indo ver ou alcançar, enquanto vale
				if (Dz.Id != NoEntity && PlanRole == Next && PlanDef == Def)
				{
					const Cell C = BaseMap.CellOf(PlanSpot);
					const PlaceResult Plan = State.CanPlace(Me, uint16_t(Def), PlanDir, PlanSpot);
					bFound = (Reach == nullptr || (BaseMap.InBounds(C.X, C.Y) && ReachMark[size_t(BaseMap.Index(C.X, C.Y))] != 0))
						&& std::none_of(Avoid.begin(), Avoid.end(), [&](const KeepOut& K) { return DistSqRaw(K.Pos, PlanSpot) <= K.RadiusSq; })
						&& (Plan == PlaceResult::Ok || Plan == PlaceResult::NotVisible);
					Spot = PlanSpot;
					SpotDir = PlanDir;
				}
				// (4ª revisão) o centro sem depósito (perto de casa: sem renda, só pra abrir a fábrica) não fica no lugar da
				// fábrica: o lugar dela (com a faixa de saída) fica guardado (medido: a base cercada pôs o centro no único
				// corredor livre e a War Factory não coube mais: 30 min com $9 200 e 0 tanques)
				Lot Keep;
				bool bKeep = false;
				if (Next == Supply && !bAtDock && Have[Factory] == 0 && FactoryDef >= 0 && !bFound)
				{
					const UnitDef& F = R.Units[size_t(FactoryDef)];
					std::vector<KeepOut> FactoryAvoid;
					for (const UnitView& T : Threats)
					{
						FactoryAvoid.push_back({ T.Pos, ThreatReachSq(T, uint16_t(FactoryDef)) });
					}
					Vec2 FactorySpot;
					for (const Try& T : Tries)
					{
						if (FindSpot(State, uint16_t(FactoryDef), T.Dir, T.Anchor, T.MinRadius, T.Start, FactoryAvoid, FactorySpot, Reach, 2))
						{
							const Fix HX = (T.Dir & 1) != 0 ? F.HalfY : F.HalfX;
							const Fix HY = (T.Dir & 1) != 0 ? F.HalfX : F.HalfY;
							const Fix Half = F.ExitLane / 2;
							const Vec2 Out = Vec2::Make(Fix::FromInt(T.Dir == 0 ? 1 : (T.Dir == 2 ? -1 : 0)), Fix::FromInt(T.Dir == 1 ? 1 : (T.Dir == 3 ? -1 : 0)));
							Keep.Pos = FactorySpot + Out * Half;
							Keep.HalfX = HX + ((T.Dir & 1) == 0 ? Half : Fix::Zero());
							Keep.HalfY = HY + ((T.Dir & 1) != 0 ? Half : Fix::Zero());
							bKeep = true;
							break;
						}
					}
				}
				for (int32_t Keeping = bKeep ? 1 : 0; Keeping >= 0 && !bFound; --Keeping)
				{
					// (o lote à vista, o lote na névoa e, com alcance, o lote à vista na espiral fina)
					for (int32_t Pass = 0; Pass < (Reach != nullptr ? 3 : 2) && !bFound; ++Pass)
					{
						for (const Try& T : Tries)
						{
							if (FindSpot(State, uint16_t(Def), T.Dir, T.Anchor, T.MinRadius, T.Start, Avoid, Spot, Reach, Pass == 1 ? 1 : 0, Keeping != 0 ? &Keep : nullptr, Pass == 2))
							{
								SpotDir = T.Dir;
								bFound = true;
								break;
							}
						}
					}
				}
				if (!bFound)
				{
					continue;
				}
				if (!bSend)
				{
					// (o Dozer novo do CC: só se chega, e raspando só se sobrevive ao pior caso)
					Vec2 Step;
					return Tier <= 0 || SafeStep(State, DozerDanger, Dz, Spot, BuilderMobility, true, Step) != 0 ? 1 : 0;
				}
				const bool bInSight = State.CanPlace(Me, uint16_t(Def), SpotDir, Spot) == PlaceResult::Ok;
				if (bInSight && !SiteIsSafe(Spot, Next, uint16_t(Def)))
				{
					return 2;
				}
				Command C;
				C.Player = Me;
				if (bInSight)
				{
					C.Type = CommandType::Build;
					C.Value = BuildValue(uint16_t(Def), SpotDir);
				}
				else
				{
					C.Type = CommandType::Move;   // o lote na névoa: vai lá ver
				}
				C.Point = Spot;
				// (4ª revisão) o Dozer constrói do lado do lote por onde chega (até o raio dele + 2 células do retângulo,
				// World::UpdateConstruction): o lote fora do alcance com o lado de cá dentro matava o Dozer na obra (medido: a War
				// Factory a 78 m da Patriot, o Dozer na borda de oeste a 60 m, 3 Dozers mortos e a base parada com $28 500). Com
				// parte da volta do lote num alcance, ele vai antes pro ponto da volta fora dos alcances mais perto dele e recebe a
				// obra ali, sem andar mais
				Vec2 Target = Spot;
				if (bInSight && Tier >= 0)
				{
					const UnitDef& D2 = R.Units[size_t(Def)];
					const Fix HX = (SpotDir & 1) != 0 ? D2.HalfY : D2.HalfX;
					const Fix HY = (SpotDir & 1) != 0 ? D2.HalfX : D2.HalfY;
					const Fix Band = BuilderRadius + BaseMap.CellSize;
					const auto RectGap = [&](Vec2 P)
					{
						return Vec2::Make(Max(Abs(P.X - Spot.X) - HX, Fix::Zero()), Max(Abs(P.Y - Spot.Y) - HY, Fix::Zero())).Length();
					};
					// a volta: pontos a cada ~4 m a Band do retângulo (os 4 lados e as 4 quinas), na mesma ordem sempre
					std::vector<Vec2> Ring;
					const int32_t NX = std::max<int32_t>(1, int32_t(((HX + Band) * 2 / BaseMap.CellSize).Floor()));
					const int32_t NY = std::max<int32_t>(1, int32_t(((HY + Band) * 2 / BaseMap.CellSize).Floor()));
					for (int32_t K = 0; K <= NX; ++K)
					{
						const Fix X = Spot.X - HX - Band + (HX + Band) * 2 * K / NX;
						Ring.push_back(Vec2::Make(X, Spot.Y - HY - Band));
						Ring.push_back(Vec2::Make(X, Spot.Y + HY + Band));
					}
					for (int32_t K = 1; K < NY; ++K)
					{
						const Fix Y = Spot.Y - HY - Band + (HY + Band) * 2 * K / NY;
						Ring.push_back(Vec2::Make(Spot.X - HX - Band, Y));
						Ring.push_back(Vec2::Make(Spot.X + HX + Band, Y));
					}
					// (o lugar onde o Dozer travou indo pro ponto, os tanques parados em casa fechando a passagem, vai direto: o jogo tira
					// os amigos da frente de quem constrói, não de quem só anda)
					bool bDirect = bPlanDirect && PlanRole == Next && DistSqRaw(Spot, PlanSpot) <= RadiusSqRaw(Fix::FromInt(RouteStepMinM));
					// (e o lado por onde ele chega, na reta até o lote, fora dos alcances também vai direto: o ponto antes custava ~7 s
					// por obra e a base com as Patriots a 80-100 m fazia 10 a 20 tanques a menos em 20 min)
					if (!bDirect)
					{
						const Vec2 Toward = Spot - Dz.Pos;
						const Fix Len = Toward.Length();
						Vec2 Arrive = Dz.Pos;
						for (Fix Walked = Fix::Zero(); Walked < Len && RectGap(Arrive) > Band; Walked += Fix::FromInt(2))
						{
							Arrive = Dz.Pos + Toward * (Walked / Max(Len, Fix::FromRaw(1)));
						}
						bDirect = !Covered(Arrive, BuilderRadius);
					}
					if (!bDirect && std::any_of(Ring.begin(), Ring.end(), [&](Vec2 P) { return Covered(P, BuilderRadius); }))
					{
						const Fix Gap = RectGap(Dz.Pos);
						const bool bAtWork = Gap >= BuilderRadius && Gap <= BuilderRadius + BaseMap.CellSize * 2 && !Covered(Dz.Pos, BuilderRadius);
						bool bWork = bAtWork;
						for (const Vec2& P : Ring)
						{
							const Cell PC = BaseMap.CellOf(P);
							if (!bAtWork && BaseMap.InBounds(PC.X, PC.Y) && ReachMark[size_t(BaseMap.Index(PC.X, PC.Y))] != 0 && !Covered(P, BuilderRadius)
								&& (!bWork || DistSqRaw(P, Dz.Pos) < DistSqRaw(Target, Dz.Pos)))
							{
								Target = P;
								bWork = true;
							}
						}
						if (!bWork)
						{
							continue;   // nenhum lado do lote fora dos alcances ao alcance dele: o próximo nível
						}
						if (bAtWork || DistSqRaw(Target, Dz.Pos) <= RadiusSqRaw(Fix::FromInt(RouteStepMinM)))
						{
							Target = Dz.Pos;   // já está do lado bom (ou colado no ponto dele): a obra sai daqui
						}
						else
						{
							C.Type = CommandType::Move;
							C.Value = 0;
							C.Point = Target;
						}
					}
				}
				const int32_t Result = SendDozer(Dz, Target, C);
				if (Result == 0)
				{
					// sem caminho seguro (ou raspando, com o pior caso acima da vida dele): o lugar entra no Unreachable (o do
					// centro, no depósito: a escolha passa pro seguinte) e nada sai neste pensar
					Unreachable.push_back({ NoEntity, Next, bAtDock ? State.Dock(ExpandDock).Pos : Spot, Now });
					PlanRole = -1;
					return 0;
				}
				if (C.Type == CommandType::Build && Result == 2)
				{
					PendingSites.push_back({ NoEntity, Next, Spot, Now });
					PlanRole = -1;
				}
				else
				{
					bPlanDirect = bPlanDirect && PlanRole == Next && DistSqRaw(Spot, PlanSpot) <= RadiusSqRaw(Fix::FromInt(RouteStepMinM));
					PlanDozer = Dz.Id;
					PlanRole = Next;
					PlanDef = Def;
					PlanDir = SpotDir;
					PlanSpot = Spot;
				}
				return 1;
			}
			if (FirstTier == 0 && bSend)
			{
				// nem por fora dos alcances nem na névoa: começa a espera pelo caminho raspando; e o depósito sem lote ao alcance
				// nem raspando entra no Unreachable (a escolha passa pro seguinte e, sem nenhum, pra perto de casa: o centro no
				// pouco lugar de casa cercada tirava o lugar da fábrica)
				RouteBlockedSince = RouteBlockedSince > 0 ? RouteBlockedSince : std::max<TickNumber>(1, Now);
				if (bAtDock && LastTier >= GrazeTiers)
				{
					Unreachable.push_back({ NoEntity, Next, State.Dock(ExpandDock).Pos, Now });
				}
			}
			return 0;
		};
		bool bSiteBlocked = false;
		bool bDoorHelps = false;   // (4ª revisão) nenhum Dozer à toa chega numa obra, e o Dozer novo que sai do CC chegaria
		bool bAllStuck = false;    // (4ª revisão) todo Dozer à toa, com o dinheiro da obra, e nenhum chega num lugar dela
		if (Free != nullptr && Next >= 0)
		{
			// o Dozer do lugar escolhido ainda indo até ele, e andando: os outros esperam (dois Dozers pro mesmo papel). Parado por
			// DozerStallS (os Dozers à toa em volta dele fechando a passagem entre o CC e a usina), os outros seguem a vida
			UnitView Planner;
			const bool bPlanWalks = PlanRole == Next && PlanDozer != NoEntity && State.FindUnit(PlanDozer, Planner) && Planner.Order != OrderType::Idle;
			if (!bPlanWalks || DistSqRaw(Planner.Pos, PlanDozerPos) > RadiusSqRaw(Fix::FromInt(DozerStallMoveM)))
			{
				PlanDozerPos = bPlanWalks ? Planner.Pos : PlanDozerPos;
				PlanDozerSince = Now;
			}
			// (e nenhum à toa mais perto do lugar que ele: o mais perto pega o lugar)
			const bool bCloser = bPlanWalks && std::any_of(FreeDozers.begin(), FreeDozers.end(), [&](const UnitView* D)
			{
				return DistSqRaw(D->Pos, PlanSpot) < DistSqRaw(Planner.Pos, PlanSpot);
			});
			const bool bPlanBusy = bPlanWalks && !bCloser && Now < PlanDozerSince + TickNumber(DozerStallS * TicksPerSecond);
			if (bPlanWalks && !bPlanBusy && !bCloser && Planner.Task == UnitTask::None)
			{
				// (4ª revisão) parado andando pro lugar (os tanques à toa em casa fechando a passagem: medido, o Dozer 20 min parado
				// com a ordem de andar e nenhuma zona): para, e o lugar vai direto (a obra o jogo leva até o lote)
				Command G;
				G.Player = Me;
				G.Type = CommandType::SetGroup;
				G.Units = { Planner.Id };
				OutCommands.push_back(G);
				Command X;
				X.Player = Me;
				X.Type = CommandType::Stop;
				OutCommands.push_back(X);
				bPlanDirect = true;
				PlanDozerSince = Now;
			}
			bool bSentHere = false;
			bool bEnemyNear = false;
			const int32_t NextDef = DefFor(Next, *Free);
			const bool bCanPay = NextDef >= 0 && Self.Money >= R.Units[size_t(NextDef)].Cost;
			// (4ª revisão) a procura que não achou nada fica até PlaceRetryS, com tudo que ela lê igual (inteiro: o mesmo em
			// qualquer PC)
			uint64_t Sig = 0;
			const auto Mix = [&Sig](int64_t V) { Sig = Sig * 1000003u + uint64_t(V); };   // (sem sinal: a volta é definida)
			Mix(Next);
			Mix(NextDef);
			Mix(int64_t(bCanPay));
			Mix(int64_t(Threats.size()));
			Mix(Completed);
			Mix(int64_t(LiveSites.size()));
			Mix(int64_t(LostSites.size()));
			Mix(int64_t(Unreachable.size()));
			Mix(PlanRole);
			for (const UnitView* D : FreeDozers)
			{
				const Cell C = State.Map().CellOf(D->Pos);
				Mix(int64_t(D->Id));
				Mix(int64_t(C.X) * 4096 + C.Y);
				Mix(D->Hp);
			}
			const bool bPlaceSkip = !bPlanBusy && Sig == PlaceSig && Now < PlaceRetryAt;
			for (const UnitView* Dz : FreeDozers)
			{
				if (bPlanBusy || bPlaceSkip || Dz->Id == Resumed)
				{
					continue;
				}
				const int32_t Result = TryPlace(*Dz, true);
				bSentHere |= Result == 1;
				bEnemyNear |= Result == 2;
				if (Result != 0)
				{
					break;
				}
			}
			// o lugar travado (a fábrica não guarda o preço do centro): inimigo perto do lugar, ou a obra que dá pra pagar sem lugar
			// nem caminho (sem o dinheiro, a fábrica guarda: a base só com a renda da zona)
			bSiteBlocked = !bSentHere && !bPlanBusy && (bEnemyNear || bCanPay);
			bAllStuck = !bSentHere && !bEnemyNear && !bPlanBusy && !Threats.empty() && bCanPay && FreeDozers.size() == Dozers.size();
			// (4ª revisão) nenhum Dozer à toa chega numa obra (o bolso entre os alcances, ou ferido demais pra passar raspando): o
			// CC faz mais um se o Dozer novo, inteiro, na porta descoberta, chega (medido: o Dozer ferido no bolso de baixo e a
			// porta do CC virada pro corredor livre de cima, 30 min com $9 200 e 0 tanques). A porta coberta: na reposição, abaixo
			if (bAllStuck && bHaveCC && BuilderDef >= 0 && int32_t(Dozers.size()) < MaxDozers && !Covered(CCDoor, BuilderRadius))
			{
				UnitView Fresh;
				Fresh.Id = NoEntity;
				Fresh.Owner = Me;
				Fresh.Def = uint16_t(BuilderDef);
				Fresh.Pos = CCDoor;
				Fresh.Hp = R.Units[size_t(BuilderDef)].MaxHp;
				Fresh.MaxHp = Fresh.Hp;
				bDoorHelps = bPlaceSkip ? bPlaceDoorHelps : TryPlace(Fresh, false) == 1;
			}
			if (!bPlaceSkip)
			{
				const bool bNothing = !bPlanBusy && !bSentHere && !bEnemyNear;
				PlaceSig = Sig;
				PlaceRetryAt = bNothing ? Now + TickNumber(PlaceRetryS * TicksPerSecond) : 0;
				bPlaceDoorHelps = bDoorHelps;
			}
		}

		if (Threats.empty())
		{
			RouteBlockedSince = 0;   // sem defesa inimiga, nada bloqueia: a espera pelo caminho raspando recomeça na próxima falta
		}

		// (2ª revisão de 23/10) com 2 Dozers ou mais, o à toa na porta do CC (sem obra pra ele neste pensar) sai da frente, pra
		// trás do CC: os Dozers a mais (o banco grande pede até 3) paravam no ponto de encontro e fechavam a passagem entre o CC
		// e a usina colada na frente dele; o que ia erguer o centro ficava preso e a base parava pra sempre (medido: $30 000 a
		// $50 000, 0 tanques em 8 min). Com 1 Dozer só, ele fica (mandar o único pra trás atrasava a obra seguinte)
		std::vector<EntityId> Parked;
		if (bHaveCC && BuilderDef >= 0 && Dozers.size() >= 2)
		{
			const Vec2 Park = CCView.Pos - CCOut * (R.Units[CCView.Def].HalfX + BuilderRadius + Fix::FromInt(2 * SpotMarginM));
			const Cell PC = State.Map().CellOf(Park);
			const bool bParkOk = State.Map().IsWalkable(PC.X, PC.Y, BuilderMobility) && !Covered(Park, BuilderRadius);
			for (const UnitView& D : Dozers)
			{
				// (4ª revisão de 23/10) pelo caminho seguro (o desvio do SafeStep); sem ele, fica (medido: o caminho pra trás do CC
				// passava no alcance da Patriot do lado e dois Dozers morreram indo estacionar)
				Vec2 Step;
				if (bParkOk && D.Task == UnitTask::None && D.Order == OrderType::Idle && D.Id != Sent && !Contains(Fleeing, D.Id)
					&& DistSqRaw(D.Pos, CCDoor) <= RadiusSqRaw(Fix::FromInt(ParkFromDoorM)) && SafeStep(State, DozerDanger, D, Park, BuilderMobility, false, Step) != 0)
				{
					Command G;
					G.Player = Me;
					G.Type = CommandType::SetGroup;
					G.Units = { D.Id };
					OutCommands.push_back(G);
					Command M;
					M.Player = Me;
					M.Type = CommandType::Move;
					M.Point = Step;
					OutCommands.push_back(M);
					Parked.push_back(D.Id);
				}
			}
		}
		// (3ª revisão de 23/10) o Dozer à toa que trava a obra parada de outro (no lote dela, de onde o jogo o manda pra dentro
		// do CC e ele não sai; ou encostado no Dozer dela que tenta andar e não sai do lugar, na porta do CC) vai pra volta do CC, fora do alcance das defesas e do
		// lote das obras: atrás primeiro, depois girando de 45 em 45° pros dois lados. Medido: com a Patriot cobrindo o ponto de
		// trás, os Dozers a mais ficavam na porta, um no lote da usina e dois prendendo o que ia erguer o centro: 8 min com
		// $18 200 e $45 700 parados
		if (bHaveCC && BuilderDef >= 0 && !Stuck.empty())
		{
			const UnitDef& CCDef = R.Units[CCView.Def];
			const Fix Ring = Sqrt(CCDef.HalfX * CCDef.HalfX + CCDef.HalfY * CCDef.HalfY) + BuilderRadius + Fix::FromInt(2 * SpotMarginM);
			const int32_t Behind = int32_t((-CCOut).Heading().Bam);
			bool bSpot = false;
			Vec2 Spot;
			for (int32_t K = 0; K < 8 && !bSpot; ++K)
			{
				const int32_t Turn = (K + 1) / 2 * (K % 2 == 0 ? -1 : 1) * Angle::FullTurn / 8;
				const Vec2 P = CCView.Pos + Vec2::FromAngle(Angle::FromBam(Behind + Turn)) * Ring;
				const Cell PC = State.Map().CellOf(P);
				const bool bOnSite = std::any_of(LotSites.begin(), LotSites.end(), [&](const UnitView& W) { return OnLot(P, BuilderRadius, W); });
				bSpot = State.Map().IsWalkable(PC.X, PC.Y, BuilderMobility) && !Covered(P, BuilderRadius) && !bOnSite;
				Spot = P;
			}
			for (const UnitView& D : Dozers)
			{
				if (!bSpot || D.Task != UnitTask::None || D.Order != OrderType::Idle || D.Id == Sent || Contains(Fleeing, D.Id) || Contains(Parked, D.Id))
				{
					continue;
				}
				const Fix Touch = R.Units[D.Def].Radius + State.Map().CellSize;
				const auto Jam = std::find_if(Stuck.begin(), Stuck.end(), [&](const std::pair<UnitView, UnitView>& S)
				{
					return OnLot(D.Pos, R.Units[D.Def].Radius, S.second)
						|| (S.first.Order != OrderType::Idle && DistSqRaw(D.Pos, S.first.Pos) <= RadiusSqRaw(Touch + R.Units[S.first.Def].Radius));
				});
				Vec2 Step;
				if (Jam == Stuck.end() || SafeStep(State, DozerDanger, D, Spot, BuilderMobility, false, Step) == 0)
				{
					continue;   // (4ª revisão) só pelo caminho seguro
				}
				Command G;
				G.Player = Me;
				G.Type = CommandType::SetGroup;
				G.Units = { D.Id };
				OutCommands.push_back(G);
				Command M;
				M.Player = Me;
				M.Type = CommandType::Move;
				M.Point = Step;
				OutCommands.push_back(M);
				Note("dozer %u out of the way of dozer %u", unsigned(D.Id), unsigned(Jam->first.Id));
			}
		}

		// produção: os Dozers (de reposição; v4: um por fábrica querida, até 3), caminhões e a fábrica sempre com fila
		int32_t Money = Self.Money;
		const auto Produce = [&](EntityId At, uint16_t Def)
		{
			if (Money < R.Units[Def].Cost || !State.HasPrerequisites(Me, Def))
			{
				return false;
			}
			Money -= R.Units[Def].Cost;
			Command P;
			P.Player = Me;
			P.Type = CommandType::Produce;
			P.Target = At;
			P.Value = Def;
			OutCommands.push_back(P);
			return true;
		};
		// (2ª revisão de 23/10) a reposição do Dozer. A porta do CC no alcance de uma defesa (o Dozer novo nasce dentro dele):
		// com fábrica, não repõe (os tanques limpam a porta antes). E depois de 2 mortes em DozerDeathWindowS, o CC espera
		// DozerRebuyWaitS desde a última, dobrando a cada morte a mais (até DozerRebuyMaxS). Medido: a porta coberta, um Dozer
		// a cada 12 s, 31 em 8 min. (4ª revisão de 23/10) Com a porta coberta, sem fábrica e com Dozer vivo, só se o novo
		// sobrevive ao pior caso da saída (EscapeStep: da porta pro lado aberto); o "sem Dozer morto a 30 m da porta" deixava
		// passar os que morriam a 32 m, aos pares, um a cada ~50 s por 30 min. E com FutileDozerDeathsMax mortos no alcance de uma defesa sem a base
		// andar, não repõe mais até ela andar
		bool bRebuyOk = true;
		if (bHaveCC && BuilderDef >= 0 && Covered(CCDoor, BuilderRadius))
		{
			// (a porta coberta, sem fábrica: o Dozer a mais só se o novo sobrevive ao pior caso da saída da porta pro lado
			// aberto. Medido: com $50 000 e a porta coberta pela Patriot a 65 m, sem os Dozers a mais a base subia devagar, 26
			// tanques e $25 100 parados contra 44 e $6 900)
			bRebuyOk = Factories.empty();
			if (bRebuyOk)
			{
				// (a fuga da porta é o Dijkstra no mapa inteiro: refeita a cada DoorEscapeEveryS, ou quando as defesas mudam)
				if (DoorEscapeAt == 0 || Now >= DoorEscapeAt + TickNumber(DoorEscapeEveryS * TicksPerSecond) || DoorEscapeThreats != int32_t(Threats.size()))
				{
					UnitView Fresh;
					Fresh.Id = NoEntity;
					Fresh.Owner = Me;
					Fresh.Def = uint16_t(BuilderDef);
					Fresh.Pos = CCDoor;
					Fresh.Hp = R.Units[size_t(BuilderDef)].MaxHp;
					Fresh.MaxHp = Fresh.Hp;
					Vec2 Step;
					bDoorEscapeOk = EscapeStep(State, DozerDanger, Fresh, BuilderMobility, false, Step);
					bDoorEscapeOpen = bEscapeOpen;
					DoorEscapeAt = std::max<TickNumber>(1, Now);
					DoorEscapeThreats = int32_t(Threats.size());
				}
				// (sem Dozer nenhum, repõe mesmo sem a saída sobrevivível no pior caso: sem ele a base não faz mais nada, e as
				// defesas na porta costumam estar atirando no CC. Medido: o Dozer da largada morto a 4 s, o CC sem repor morria
				// aos 41 s; o novo saía inteiro. O FutileDozerDeathsMax segura a conta)
				bRebuyOk = Dozers.empty() || (bDoorEscapeOk && bDoorEscapeOpen);
			}
			bDoorHelps |= bRebuyOk && bAllStuck;
		}
		bRebuyOk &= FutileDeaths < FutileDozerDeathsMax;
		if (DozerDeaths.size() >= 2)
		{
			const int32_t Extra = std::min<int32_t>(int32_t(DozerDeaths.size()) - 2, 4);
			const int32_t WaitS = std::min(DozerRebuyMaxS, DozerRebuyWaitS << Extra);
			bRebuyOk &= Now >= DozerDeaths.back() + TickNumber(WaitS * TicksPerSecond);
		}
		// (4ª revisão) um a mais quando nenhum Dozer à toa chega numa obra e o novo, da porta, chega (bDoorHelps)
		const int32_t WantDozers = bDoorHelps ? std::max(DozersWanted, std::min<int32_t>(MaxDozers, int32_t(Dozers.size()) + 1)) : DozersWanted;
		if (int32_t(Dozers.size()) < WantDozers && bHaveCC && bRebuyOk && CCView.QueueSize == 0)
		{
			for (const uint16_t K : R.Units[CCView.Def].Produces)
			{
				if (R.Units[K].CanConstruct())
				{
					Produce(CommandCenter, K);
					break;
				}
			}
		}
		// os caminhões: TrucksWanted por centro vivo (o centro seco não faz mais)
		const bool bTrucksShort = Center != NoEntity && Trucks < TrucksWanted * LiveCenters;
		if (bTrucksShort && CenterQueue == 0)
		{
			UnitView C;
			State.FindUnit(Center, C);
			for (const uint16_t K : R.Units[C.Def].Produces)
			{
				if (R.Units[K].SupplyBoxes > 0)
				{
					Produce(Center, K);
					break;
				}
			}
		}
		// o caminhão parado sem depósito ao alcance: o com caixa mais perto dele, do lado de casa primeiro (o do lado do
		// inimigo só quando o de casa acabou). (2ª revisão de 23/10) Nunca o depósito no alcance de uma defesa inimiga (o
		// centro perto de casa, sem depósito livre, mandava o caminhão grátis morrer no depósito coberto)
		for (const UnitView& T : IdleTrucks)
		{
			int32_t Best = -1;
			int64_t BestD2 = 0;
			for (int32_t Pass = 0; Pass < 2 && Best < 0; ++Pass)
			{
				for (int32_t K = 0; K < State.DockCount(); ++K)
				{
					const DockView Dk = State.Dock(K);
					if (Dk.Boxes <= 0 || (Pass == 0 && DistSqRaw(Dk.Pos, Home) > DistSqRaw(Dk.Pos, EnemyStart))
						|| Covered(Dk.Pos, R.Units[T.Def].Radius + Max(Dk.HalfX, Dk.HalfY)))
					{
						continue;
					}
					const int64_t D2 = DistSqRaw(Dk.Pos, T.Pos);
					if (Best < 0 || D2 < BestD2)
					{
						Best = K;
						BestD2 = D2;
					}
				}
			}
			if (Best >= 0)
			{
				Command G;
				G.Player = Me;
				G.Type = CommandType::SetGroup;
				G.Units = { T.Id };
				OutCommands.push_back(G);
				Command C;
				C.Player = Me;
				C.Type = CommandType::Gather;
				C.Value = Best;
				OutCommands.push_back(C);
			}
		}
		// a base seca esperando o centro novo: a fábrica para de gastar até o centro sair (com a fila de 2 tanques
		// sempre cheia, o dinheiro nunca chegava no preço dele). (v4) Com o inimigo no lugar do centro, a fábrica segue:
		// são os tanques que limpam o lugar. (v4) O mesmo pro centro que vai secar (a expansão cedo) e pro único centro que
		// morreu (medido 23/10: sem centro, a renda era só a da zona e a fábrica comia tudo; o centro nunca voltava).
		// (revisão de 23/10) Só com quem erga o centro (um Dozer, ou o CC que faz um: sem os dois, a fábrica parava até a
		// derrota), só enquanto falta o preço dele (com o preço no banco, o Dozer andando até o depósito na névoa ou o Dozer
		// novo saindo do CC paravam as fábricas 100 a 400 s com $3 000 a $8 000: agora elas seguem guardando o preço) e no
		// máximo CenterSaveMaxS por janela, como a zona (depois gasta normal IncomeSaveRetryS e abre outra). O preço sai do
		// centro que quem ergue constrói (CenterCost, lá em cima). (2ª revisão) Sem Dozer vivo, só se o CC pode repor agora (a
		// porta coberta ou a espera depois das mortes seguram a reposição: guardar o preço parava a fábrica com $2 000+ por 87 s)
		const bool bBuilderSoon = !Dozers.empty() || (bHaveCC && bRebuyOk);
		const bool bCenterNeeded = (bDry || bRunningLow || Have[Supply] == 0) && Unfinished[Supply] == 0 && CenterCost > 0 && bBuilderSoon;
		const bool bCenterWanted = bCenterNeeded && Next == Supply && !bSiteBlocked;
		bool bSaveForCenter = false;
		int32_t CenterHeld = 0;
		if (bCenterWanted && Self.Money < CenterCost)
		{
			if (!bCenterSaving || Now >= CenterSaveSince + TickNumber((CenterSaveMaxS + IncomeSaveRetryS) * TicksPerSecond))
			{
				bCenterSaving = true;
				CenterSaveSince = Now;
			}
			bSaveForCenter = Now < CenterSaveSince + TickNumber(CenterSaveMaxS * TicksPerSecond);
		}
		else if (bCenterWanted)
		{
			CenterHeld = CenterCost;
		}
		else if (!bCenterNeeded)
		{
			bCenterSaving = false;   // o centro saiu (ou a base não precisa mais): a próxima falta abre uma janela nova
		}
		// (v4) a fábrica escolhe pelo que falta (ai_v4, regra 6): a artilharia quer 1 + as defesas inimigas conhecidas (até
		// ArtilleryMax), só com 2 tanques de escolta por lançador (mais 2); senão o 1º armado (o Crusader). O lançador
		// escolhido sem dinheiro espera (comprar o tanque no lugar dele adiava o lançador pra sempre: o dinheiro vive perto
		// de 0). Com caminhão faltando, a fila guarda o dinheiro dele (TruckReserve); (revisão de 23/10) com o centro querido
		// e o preço dele no banco, guarda o preço (CenterHeld)
		const int32_t ArtilleryWant = std::min(ArtilleryMax, 1 + EnemyDefenses);
		// (4ª revisão) e o preço da obra que o Dozer está indo marcar (o lugar escolhido, andando em pernas, até o lado do lote ou
		// ver na névoa: a fundação só sai na chegada; medido, a fábrica gastava o dinheiro da zona no caminho e a zona nunca saía).
		// Uma vez só: com o centro querido, o lugar escolhido é o dele (as duas somadas paravam a fábrica com $4 000+)
		const int32_t PlanHeld = PlanRole >= 0 && PlanRole == Next && PlanDef >= 0 ? R.Units[size_t(PlanDef)].Cost : 0;
		const int32_t Reserve = (bTrucksShort ? TruckReserve : 0) + std::max(CenterHeld, PlanHeld);
		for (const UnitView& F : Factories)
		{
			if (F.QueueSize >= 2 || bSaveForCenter || bSaveForZone || bSaveForDefense)
			{
				continue;
			}
			int32_t Pick = -1;
			for (const uint16_t K : R.Units[F.Def].Produces)
			{
				if (IsArtillery(R.Units[K]) && State.HasPrerequisites(Me, K) && ArtilleryHave < ArtilleryWant && TanksHave >= 2 * ArtilleryHave + 2)
				{
					Pick = K;
					break;
				}
			}
			const bool bArtillery = Pick >= 0;
			for (const uint16_t K : R.Units[F.Def].Produces)
			{
				if (Pick < 0 && R.Units[K].HasWeapon() && State.HasPrerequisites(Me, K))
				{
					Pick = K;
				}
			}
			if (Pick < 0 || Money < R.Units[size_t(Pick)].Cost + Reserve || !Produce(F.Id, uint16_t(Pick)))
			{
				continue;
			}
			if (bArtillery)
			{
				++ArtilleryHave;
				Note("%s queued", R.Units[size_t(Pick)].Key.c_str());
			}
			else
			{
				++TanksHave;
			}
		}
	}

	void SkirmishAI::DefendHome(const World& State, std::vector<Command>& OutCommands, std::vector<EntityId>& OutHomeGuard)
	{
		// (v4, ai_v4 regra 7) inimigo armado conhecido (unidade, não prédio) a até HomeAlertM de um prédio dela: quem está a
		// até HomeCallM de casa (ou do prédio ameaçado: a expansão no meio) e não está atirando vai no intruso mais perto de
		// casa (desempate pelo Id). A onda que já saiu não volta pra casa
		const Ruleset& R = State.Rules();
		const Vec2 Home = State.PlayerStart(Me);
		std::vector<UnitView> Mine;
		for (int32_t I = 0; I < State.UnitCount(); ++I)
		{
			const UnitView V = State.Unit(I);
			if (V.Owner == Me && V.bStructure)
			{
				Mine.push_back(V);
			}
		}
		EntityId Intruder = NoEntity;
		Vec2 IntruderPos;
		Vec2 Threatened = Home;   // (v4) o prédio dela mais perto do intruso (a expansão no meio também chama quem está perto dela)
		int64_t BestD2 = 0;
		for (int32_t I = 0; I < State.UnitCount(); ++I)
		{
			const UnitView V = State.Unit(I);
			const UnitDef& D = R.Units[V.Def];
			if (!State.AreEnemies(Me, V.Owner) || D.IsStructure() || !D.HasWeapon() || !Knows(State, V))
			{
				continue;
			}
			bool bNear = false;
			Vec2 Nearest = Home;
			int64_t NearestD2 = -1;
			for (const UnitView& S : Mine)
			{
				const UnitDef& SD = R.Units[S.Def];
				const int64_t SD2 = DistSqRaw(V.Pos, S.Pos);
				bNear |= SD2 <= RadiusSqRaw(Fix::FromInt(HomeAlertM) + Max(SD.HalfX, SD.HalfY));
				if (NearestD2 < 0 || SD2 < NearestD2)
				{
					Nearest = S.Pos;
					NearestD2 = SD2;
				}
			}
			const int64_t D2 = DistSqRaw(V.Pos, Home);
			if (bNear && (Intruder == NoEntity || D2 < BestD2))
			{
				Intruder = V.Id;
				IntruderPos = V.Pos;
				Threatened = Nearest;
				BestD2 = D2;
			}
		}
		if (Intruder == NoEntity)
		{
			// o alarme acabou: a base pede a próxima defesa (a ThinkBase guarda o dinheiro dela por um tempo)
			AlertEndedAt = DefendTarget != NoEntity ? std::max<TickNumber>(1, State.Tick()) : AlertEndedAt;
			DefendTarget = NoEntity;
			return;
		}
		if (DefendTarget == NoEntity)
		{
			Note("defend at %d,%d", Meters(IntruderPos.X), Meters(IntruderPos.Y));   // uma linha por alarme (não por intruso)
		}
		DefendTarget = Intruder;
		const int64_t CallSq = RadiusSqRaw(Fix::FromInt(HomeCallM));
		std::vector<EntityId> Who;
		for (int32_t I = 0; I < State.UnitCount(); ++I)
		{
			const UnitView V = State.Unit(I);
			if (V.Owner != Me || !IsArmed(State, V) || (DistSqRaw(V.Pos, Home) > CallSq && DistSqRaw(V.Pos, Threatened) > CallSq))
			{
				continue;
			}
			OutHomeGuard.push_back(V.Id);
			const bool bGoing = V.Order == OrderType::AttackMove && DistSqRaw(V.OrderPoint, IntruderPos) <= RadiusSqRaw(Fix::FromInt(DefendOrderSlackM));
			if (V.FireTarget == NoEntity && !bGoing)
			{
				Who.push_back(V.Id);
			}
		}
		if (Who.empty())
		{
			return;
		}
		Command Group;
		Group.Player = Me;
		Group.Type = CommandType::SetGroup;
		Group.Units = Who;
		OutCommands.push_back(Group);
		Command Go;
		Go.Player = Me;
		Go.Type = CommandType::AttackMove;
		Go.Point = IntruderPos;
		OutCommands.push_back(Go);
	}

	void SkirmishAI::UsePowers(const World& State, std::vector<Command>& OutCommands)
	{
		// (v4, power_cruise_strike ai_rule) o poder pronto mira na célula conhecida que mais vale: os candidatos são as
		// posições dos inimigos que ela conhece; o valor soma, de cada inimigo conhecido a até o raio + 4 m (do lote, no
		// prédio), custo × vida/vida máxima × peso/100. Peso: 300 a renda (a zona), 150 defesa, usina e artilharia, 100 os
		// outros prédios e a unidade parada, 40 a unidade andando. Dispara com StrikeMinScore ou mais; desempate pelo Id
		const TickNumber Now = State.Tick();
		if (Now < NextStrikeThink)
		{
			return;
		}
		NextStrikeThink = Now + TickNumber(std::max<int32_t>(1, StrikeThinkS) * TicksPerSecond) - 1;
		const Ruleset& R = State.Rules();
		struct Mark
		{
			Vec2 Pos;
			Fix HalfX;
			Fix HalfY;
			int64_t Value = 0;
		};
		std::vector<Mark> Marks;
		for (int32_t K = 0; K < int32_t(R.Powers.size()) && K < MaxPowers; ++K)
		{
			const PowerDef& P = R.Powers[size_t(K)];
			if (P.Missiles <= 0 || !State.IsPowerReady(Me, K))
			{
				continue;
			}
			if (Marks.empty())
			{
				for (int32_t I = 0; I < State.UnitCount(); ++I)
				{
					const UnitView V = State.Unit(I);
					const UnitDef& D = R.Units[V.Def];
					if (!State.AreEnemies(Me, V.Owner) || !Knows(State, V))
					{
						continue;
					}
					int32_t Weight = 100;
					if (D.IsStructure())
					{
						Weight = D.HasAirlift() ? 300 : (D.HasWeapon() || D.Energy > 0 ? 150 : 100);
					}
					else
					{
						Weight = IsArtillery(D) ? 150 : (V.Speed.Raw == 0 ? 100 : 40);
					}
					Mark M;
					M.Pos = V.Pos;
					if (D.IsStructure())
					{
						const bool bTurned = (V.Dir & 1) != 0;   // os lados 1 e 3 trocam X e Y
						M.HalfX = bTurned ? D.HalfY : D.HalfX;
						M.HalfY = bTurned ? D.HalfX : D.HalfY;
					}
					M.Value = V.MaxHp > 0 ? int64_t(D.Cost) * V.Hp * Weight / (int64_t(V.MaxHp) * 100) : 0;
					Marks.push_back(M);
				}
			}
			const bool bUpgraded = P.UpgradeRank > 0 && State.Rank(Me) >= P.UpgradeRank;
			const Fix Reach = (bUpgraded ? P.PatternRadiusUpgraded : P.PatternRadius) + Fix::FromInt(StrikeReachMarginM);
			const int64_t ReachSq = RadiusSqRaw(Reach);
			int64_t BestScore = -1;
			Vec2 BestPos;
			for (const Mark& C : Marks)
			{
				int64_t Score = 0;
				for (const Mark& O : Marks)
				{
					const Fix DX = Max(Abs(C.Pos.X - O.Pos.X) - O.HalfX, Fix::Zero());
					const Fix DY = Max(Abs(C.Pos.Y - O.Pos.Y) - O.HalfY, Fix::Zero());
					Score += Vec2::Make(DX, DY).LengthSqRaw() <= ReachSq ? O.Value : 0;
				}
				if (Score > BestScore)
				{
					BestScore = Score;
					BestPos = C.Pos;
				}
			}
			if (BestScore < int64_t(StrikeMinScore))
			{
				continue;
			}
			Command Use;
			Use.Player = Me;
			Use.Type = CommandType::UsePower;
			Use.Value = K;
			Use.Point = BestPos;
			OutCommands.push_back(Use);
			Note("strike at %d,%d score %lld", Meters(BestPos.X), Meters(BestPos.Y), (long long)BestScore);
			return;   // um poder por vez
		}
	}

	void SkirmishAI::EndSiege(const World& State)
	{
		if (SiegeTarget == NoEntity)
		{
			return;
		}
		UnitView Defense;
		Note("siege off %u (%s)", unsigned(SiegeTarget), State.FindUnit(SiegeTarget, Defense) ? "defense alive" : "defense down");
		SiegeTarget = NoEntity;
	}

	bool SkirmishAI::Siege(const World& State, std::vector<Command>& OutCommands, Vec2 Front, const std::vector<EntityId>& HomeGuard)
	{
		// (v4, ai_v4 regra 10) com 2 ou mais de artilharia e uma defesa inimiga conhecida a até SiegeRadiusM da frente (a mais
		// perto, desempate pelo Id): a artilharia ataca a defesa (de novo só quando ela muda) e os outros armados param a
		// SiegeHoldM dela, do lado da frente (fora do alcance da Patriot); a defesa morta passa pra próxima.
		// (revisão de 23/10) A frente do cerco é o grupo dele: quem está a até SiegeRadiusM da defesa. Antes era a média de
		// todo armado a mais de 200 m de casa, e o reforço que cruzava os 200 m (até o 2º lançador, mandado pelo próprio cerco)
		// puxava a frente pra trás: 10 de 12 cercos caíram em 3 a 15 s com a defesa viva, e quem esperava no ponto ia atacar
		// outra coisa 300 m pra trás. Agora o cerco segue enquanto a defesa vive e o grupo tem 2+ lançadores; o ponto de
		// espera sai do grupo quando o cerco começa e fica. Quem está longe do grupo não recebe o ponto de espera: o parado vai
		// até ele em ataque-andando (briga no caminho, como todo reforço) e entra no grupo ao chegar; quem tem outra ordem segue
		const Ruleset& R = State.Rules();
		std::vector<UnitView> Mine;
		int32_t ArtilleryCount = 0;
		for (int32_t I = 0; I < State.UnitCount(); ++I)
		{
			const UnitView V = State.Unit(I);
			if (V.Owner != Me || !IsArmed(State, V) || Contains(HomeGuard, V.Id))
			{
				continue;
			}
			Mine.push_back(V);
			ArtilleryCount += IsArtillery(R.Units[V.Def]) ? 1 : 0;
		}
		const int64_t GroupSq = RadiusSqRaw(Fix::FromInt(SiegeRadiusM));
		const auto IsDefense = [&](const UnitView& V)
		{
			const UnitDef& D = R.Units[V.Def];
			return State.AreEnemies(Me, V.Owner) && D.IsStructure() && D.HasWeapon() && V.BuildLeft == 0 && Knows(State, V);
		};
		const auto GroupArtillery = [&](Vec2 P)
		{
			int32_t N = 0;
			for (const UnitView& V : Mine)
			{
				N += IsArtillery(R.Units[V.Def]) && DistSqRaw(V.Pos, P) <= GroupSq ? 1 : 0;
			}
			return N;
		};
		EntityId Target = NoEntity;
		Vec2 TargetPos;
		UnitView Held;
		if (SiegeTarget != NoEntity && ArtilleryCount >= 2 && State.FindUnit(SiegeTarget, Held) && IsDefense(Held) && GroupArtillery(Held.Pos) >= 2)
		{
			Target = SiegeTarget;
			TargetPos = Held.Pos;
		}
		else
		{
			EndSiege(State);
			// um cerco novo: a defesa conhecida mais perto da frente que tem 2+ lançadores dela a até SiegeRadiusM
			int64_t BestD2 = 0;
			for (int32_t I = 0; I < State.UnitCount() && ArtilleryCount >= 2; ++I)
			{
				const UnitView V = State.Unit(I);
				if (!IsDefense(V) || GroupArtillery(V.Pos) < 2)
				{
					continue;
				}
				const int64_t D2 = DistSqRaw(V.Pos, Front);
				if (Target == NoEntity || D2 < BestD2)
				{
					Target = V.Id;
					TargetPos = V.Pos;
					BestD2 = D2;
				}
			}
			if (Target == NoEntity)
			{
				return false;
			}
			// o ponto de espera: SiegeHoldM da defesa, na direção do centro do grupo (o grupo em cima dela: na direção de casa)
			Vec2 GroupSum = Vec2::Zero();
			int64_t GroupCount = 0;
			for (const UnitView& V : Mine)
			{
				if (DistSqRaw(V.Pos, TargetPos) <= GroupSq)
				{
					GroupSum += V.Pos;
					++GroupCount;
				}
			}
			Vec2 Away = Vec2::Make(GroupSum.X / GroupCount, GroupSum.Y / GroupCount) - TargetPos;
			if (Away.LengthSqRaw() == 0)
			{
				Away = State.PlayerStart(Me) - TargetPos;
			}
			SiegeHold = TargetPos + Away.Normalized() * Fix::FromInt(SiegeHoldM);
			SiegeTarget = Target;
			Note("siege on %u", unsigned(Target));
		}
		const Vec2 Hold = SiegeHold;
		std::vector<EntityId> Shooters;
		std::vector<EntityId> Holders;
		std::vector<EntityId> Joiners;
		for (const UnitView& V : Mine)
		{
			if (DistSqRaw(V.Pos, TargetPos) > GroupSq)
			{
				// o reforço de longe: o parado vai até o grupo em ataque-andando (quem já vai, ou tem outra ordem, segue)
				const bool bJoining = V.Order == OrderType::AttackMove && DistSqRaw(V.OrderPoint, Hold) <= RadiusSqRaw(Fix::FromInt(SiegeOrderSlackM));
				if (V.Order == OrderType::Idle && V.FireTarget == NoEntity && !bJoining)
				{
					Joiners.push_back(V.Id);
				}
				continue;
			}
			if (IsArtillery(R.Units[V.Def]))
			{
				if (!(V.Order == OrderType::Attack && (V.Engage == Target || V.FireTarget == Target)))
				{
					Shooters.push_back(V.Id);
				}
				continue;
			}
			// quem briga com unidade (o defensor que saiu) segue brigando; quem já vai ou já está no ponto fica
			UnitView Foe;
			const bool bFightingUnit = V.FireTarget != NoEntity && State.FindUnit(V.FireTarget, Foe) && !Foe.bStructure;
			const bool bGoing = V.Order == OrderType::Move && DistSqRaw(V.OrderPoint, Hold) <= RadiusSqRaw(Fix::FromInt(SiegeOrderSlackM));
			const bool bThere = V.Order == OrderType::Idle && DistSqRaw(V.Pos, Hold) <= RadiusSqRaw(Fix::FromInt(SiegeHoldSlackM));
			if (!bFightingUnit && !bGoing && !bThere)
			{
				Holders.push_back(V.Id);
			}
		}
		const auto Order = [&](const std::vector<EntityId>& Who, CommandType Type)
		{
			if (Who.empty())
			{
				return;
			}
			Command Group;
			Group.Player = Me;
			Group.Type = CommandType::SetGroup;
			Group.Units = Who;
			OutCommands.push_back(Group);
			Command Go;
			Go.Player = Me;
			Go.Type = Type;
			if (Type == CommandType::Attack)
			{
				Go.Target = Target;
			}
			else
			{
				Go.Point = Hold;
			}
			OutCommands.push_back(Go);
		};
		Order(Shooters, CommandType::Attack);
		Order(Holders, CommandType::Move);
		Order(Joiners, CommandType::AttackMove);
		return true;
	}

	void SkirmishAI::Attack(const World& State, std::vector<Command>& OutCommands, bool bBase)
	{
		// (v4) com base, antes da onda: a casa que se defende e os poderes do general (valem mesmo sem exército)
		std::vector<EntityId> HomeGuard;
		if (bBase)
		{
			DefendHome(State, OutCommands, HomeGuard);
			UsePowers(State, OutCommands);
		}
		std::vector<EntityId> Army;
		std::vector<EntityId> Idle;
		Vec2 Sum = Vec2::Zero();
		// (v4) a frente: quem está longe de casa (os novos esperando em casa puxavam o centro do exército pra trás)
		Vec2 FrontSum = Vec2::Zero();
		int64_t FrontCount = 0;
		int32_t ArtilleryCount = 0;
		const Vec2 Home = State.PlayerStart(Me);
		const int64_t CallSq = RadiusSqRaw(Fix::FromInt(HomeCallM));
		for (int32_t I = 0; I < State.UnitCount(); ++I)
		{
			const UnitView V = State.Unit(I);
			// o exército é quem tem arma: prédio, Dozer e caminhão ficam fora da ofensiva
			if (V.Owner != Me || !IsArmed(State, V))
			{
				continue;
			}
			Army.push_back(V.Id);
			Sum += V.Pos;
			if (bBase)
			{
				ArtilleryCount += IsArtillery(State.Rules().Units[V.Def]) ? 1 : 0;
				if (DistSqRaw(V.Pos, Home) > CallSq)
				{
					FrontSum += V.Pos;
					++FrontCount;
				}
			}
			if (V.FireTarget == NoEntity && (V.Order == OrderType::Idle || CurrentTarget == NoEntity) && !Contains(HomeGuard, V.Id))
			{
				Idle.push_back(V.Id);
			}
		}
		if (Army.empty())
		{
			EndSiege(State);   // (revisão de 23/10) o cerco acaba junto (antes o alvo ficava e o próximo cerco nela não saía no log)
			bWaveSent = false;
			return;
		}
		if (bBase)
		{
			// (v4) a onda cresce com a patente: pelo menos a 1ª onda + WavePerRank por patente acima da 1 (até MaxWaveSize)
			StartWaveSize = StartWaveSize > 0 ? StartWaveSize : WaveSize;
			WaveSize = std::max(WaveSize, std::min(MaxWaveSize, StartWaveSize + WavePerRank * (State.Rank(Me) - 1)));
		}
		if (bBase && bWaveSent && int32_t(Army.size()) <= RegroupAt)
		{
			// a onda quase acabou: os novos esperam em casa a próxima, maior (14/10: com a defesa de base, os tanques indo
			// um a um morriam na Patriot e a partida não acabava; a IA do ZH também cresce as ondas)
			bWaveSent = false;
			WaveSize = WaveSize + WaveGrowth < MaxWaveSize ? WaveSize + WaveGrowth : MaxWaveSize;
		}
		// (4ª revisão de 23/10) o exército que não cresce mais (sem renda e sem o dinheiro de um armado: bArmyCapped) sai com o
		// que tem: esperar a onda é esperar pra sempre (medido: a base cercada ergueu a fábrica, perdeu a renda do depósito
		// coberto e ficou 30 min com 5 tanques em casa esperando o 6º, e a Patriot na porta do CC viva; e o anel de 4 Patriots,
		// a 1ª onda matou 2, sobraram 4 tanques e eles ficaram 26 min em casa com $0)
		if (bBase && !bWaveSent && int32_t(Army.size()) < WaveSize && !bArmyCapped)
		{
			EndSiege(State);
			return;   // com base: junta a onda em casa antes de sair
		}
		bWaveSent = true;
		const Vec2 Center = Vec2::Make(Sum.X / int64_t(Army.size()), Sum.Y / int64_t(Army.size()));
		const Vec2 Front = FrontCount > 0 ? Vec2::Make(FrontSum.X / FrontCount, FrontSum.Y / FrontCount) : Center;
		if (!bBase)
		{
			EndSiege(State);
		}
		else if (Siege(State, OutCommands, Front, HomeGuard))
		{
			return;
		}

		EntityId Target = NoEntity;
		Vec2 TargetPos;
		if (bBase)
		{
			// (v4, ai_v4 regra 11) o alvo pelo valor: entre os inimigos que ela conhece, custo × prioridade × 1000 ÷ (distância
			// em m + 100) da frente. Prioridade (× 10): a zona 30 (renda e a carga que cai), caminhão 20, centro e usina 15
			// (a renda e a energia das defesas), defesa 10 com 2+ de artilharia e 3 sem (a Patriot vale 2,45 Crusaders por $:
			// só tanque troca mal), o resto 10; desempate pelo Id
			const Ruleset& R = State.Rules();
			// (2ª revisão de 23/10) a defesa inimiga plantada na porta dela (a até HomeAlertM de um prédio dela, a mesma régua
			// da DefendHome) vem antes de tudo: é ela que cobre a saída do CC, o depósito de casa ou o caminho do Dozer
			std::vector<UnitView> MyStructures;
			for (int32_t I = 0; I < State.UnitCount(); ++I)
			{
				const UnitView V = State.Unit(I);
				if (V.Owner == Me && V.bStructure)
				{
					MyStructures.push_back(V);
				}
			}
			int64_t BestScore = -1;
			for (int32_t I = 0; I < State.UnitCount(); ++I)
			{
				const UnitView V = State.Unit(I);
				const UnitDef& D = R.Units[V.Def];
				if (!State.AreEnemies(Me, V.Owner) || !Knows(State, V))
				{
					continue;
				}
				int64_t Prio = 10;
				if (D.HasAirlift())
				{
					Prio = 30;
				}
				else if (D.SupplyBoxes > 0)
				{
					Prio = 20;
				}
				else if (D.bSupplyCenter || (D.IsStructure() && D.Energy > 0))
				{
					Prio = 15;
				}
				else if (D.IsStructure() && D.HasWeapon())
				{
					Prio = ArtilleryCount >= 2 ? 10 : 3;
					for (const UnitView& S : MyStructures)
					{
						const UnitDef& SD = R.Units[S.Def];
						Prio = DistSqRaw(V.Pos, S.Pos) <= RadiusSqRaw(Fix::FromInt(HomeAlertM) + Max(SD.HalfX, SD.HalfY)) ? 40 : Prio;
					}
				}
				const int64_t Score = int64_t(D.Cost) * Prio * 1000 / ((V.Pos - Front).Length().Floor() + 100);
				if (Score > BestScore)
				{
					BestScore = Score;
					Target = V.Id;
					TargetPos = V.Pos;
				}
			}
		}
		if (Target == NoEntity)
		{
			// o inimigo mais perto do centro do exército (desempate pelo ID). (v4) Com base, só quando ela não conhece nenhum
			// inimigo: o mapa ela conhece (a IA do ZH também), e assim a onda nunca fica parada sem alvo
			int64_t BestD2 = 0;
			for (int32_t I = 0; I < State.UnitCount(); ++I)
			{
				const UnitView V = State.Unit(I);
				if (!State.AreEnemies(Me, V.Owner))
				{
					continue;
				}
				const int64_t D2 = DistSqRaw(Center, V.Pos);
				if (Target == NoEntity || D2 < BestD2)
				{
					Target = V.Id;
					TargetPos = V.Pos;
					BestD2 = D2;
				}
			}
		}
		if (Target == NoEntity)
		{
			return;
		}

		// só quem está sem alvo recebe a ordem: reenviar pro exército inteiro apagava o alvo de
		// quem já estava atirando (o "tranco" a cada morte); a primeira onda vai inteira
		std::vector<EntityId> Who = Idle;
		if (CurrentTarget != Target)
		{
			for (int32_t I = 0; I < State.UnitCount(); ++I)
			{
				const UnitView V = State.Unit(I);
				if (V.Owner == Me && IsArmed(State, V) && V.FireTarget == NoEntity && V.Order != OrderType::Idle && V.Order != OrderType::AttackMove
					&& !Contains(HomeGuard, V.Id))
				{
					Who.push_back(V.Id);
				}
			}
		}
		if (Who.empty())
		{
			CurrentTarget = Target;
			return;
		}
		if (Who.size() > size_t(MaxGroupSize))
		{
			Who.resize(size_t(MaxGroupSize));
		}
		CurrentTarget = Target;

		Command Group;
		Group.Player = Me;
		Group.Type = CommandType::SetGroup;
		Group.Units = Who;
		OutCommands.push_back(Group);

		Command Go;
		Go.Player = Me;
		Go.Type = CommandType::AttackMove;
		Go.Point = TargetPos;
		OutCommands.push_back(Go);
	}
}
