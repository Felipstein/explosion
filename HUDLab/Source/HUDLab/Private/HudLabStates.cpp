#include "HudLabStates.h"

namespace
{
	const FLinearColor PBlue(FColor(77, 143, 224));
	const FLinearColor PRed(FColor(224, 68, 58));

	FRTSHudSlot Unit(FName Id, FName Image, float Health, bool bCaptain = false)
	{
		FRTSHudSlot S;
		S.bEmpty = false; S.Id = Id; S.Image = Image; S.Label = TEXT("USA Battle Tank");
		S.Health = Health; S.bCaptain = bCaptain;
		return S;
	}

	FRTSHudSlot Build(FName Id, TCHAR Key, const FString& Label, const FString& Price, FName Image = NAME_None)
	{
		FRTSHudSlot S;
		S.bEmpty = false; S.Id = Id; S.Key = Key; S.Label = Label; S.Price = Price; S.Image = Image;
		return S;
	}

	FRTSHudSlot Action(FName Id, TCHAR Key, const FString& Label, FName Icon)
	{
		FRTSHudSlot S = Build(Id, Key, Label, FString(), Icon);
		S.bAction = true;
		return S;
	}

	FRTSHudSlot Locked(FRTSHudSlot S) { S.bLocked = true; return S; }
	FRTSHudSlot Upgrade(FRTSHudSlot S) { S.bUpgrade = true; return S; }

	FRTSHudPlayer Player(const FString& Name, const TCHAR* Faction, const FLinearColor& Color)
	{
		FRTSHudPlayer P;
		P.Name = Name; P.Emblem = FName(*FString::Printf(TEXT("emblem:%s"), Faction)); P.Color = Color;
		return P;
	}

	/** Uma superarma pelo nome (o tipo e o desenho saem dele). */
	FRTSHudSuperweapon SW(const FString& Name, int32 Seconds)
	{
		FRTSHudSuperweapon W;
		W.Name = Name; W.Type = FName(*Name); W.Seconds = Seconds;
		const TCHAR* Glyph = Name == TEXT("Orbital Lance") ? TEXT("sw_orbital") : Name == TEXT("Nuclear Missile") ? TEXT("sw_nuclear")
			: Name == TEXT("Plague Storm") ? TEXT("sw_plague") : Name == TEXT("Thermobaric Strike") ? TEXT("sw_thermobaric") : TEXT("sw_scud");
		W.Glyph = FName(*FString::Printf(TEXT("skin:%s"), Glyph));
		return W;
	}

	FRTSHudPower Power(const TCHAR* Icon, float Remaining, const FString& Time)
	{
		FRTSHudPower P; P.Icon = FName(*FString::Printf(TEXT("icon:%s"), Icon)); P.Remaining = Remaining; P.Time = Time;
		return P;
	}

	void Common(FRTSHudState& St, bool bEightPlayers)
	{
		St.Clock = TEXT("12:34");
		St.Money = TEXT("$ 4,250");
		St.PowerMade = 120;
		St.PowerUsed = bEightPlayers ? 130 : 95;
		St.Stars = 3; St.Xp = 0.62f; St.Promote = 1;
		St.Alerts.Add({ TEXT("Unit under attack"), PRed });
		if (bEightPlayers)
		{
			St.Alerts.Add({ TEXT("Low power"), FLinearColor(FColor(240, 160, 32)) });
		}
		St.bOffscreenArrow = true;
		St.Powers = { Power(TEXT("orbital_lance"), 0.62f, TEXT("2:31")), Power(TEXT("orbital_lance"), 0.9f, TEXT("5:40")),
			Power(TEXT("artillery_strike"), 0.0f, FString()), Power(TEXT("paradrop"), 0.3f, TEXT("0:48")), Power(TEXT("spy_drone"), 0.0f, FString()) };

		St.MinimapImage = TEXT("minimap");
		// o mapa do minimapa é o da peça aprovada, que já traz os pontos e a câmera
		St.Groups.SetNum(10);
		St.Groups[0] = { 4, 0.82f };
		St.Groups[1] = { 2, 0.38f };

		if (!bEightPlayers)
		{
			FRTSHudPlayer Me = Player(TEXT("Felipe"), TEXT("usa"), PBlue); Me.bMe = true;
			FRTSHudPlayer En = Player(TEXT("Enemy AI"), TEXT("gla"), PRed); En.Team = 1;
			En.Superweapons = { SW(TEXT("Plague Storm"), 192) };
			St.Players = { Me, En };
		}
		else
		{
			FRTSHudPlayer Me = Player(TEXT("Felipe"), TEXT("usa"), PBlue); Me.bMe = true;
			FRTSHudPlayer Bruno = Player(TEXT("Bruno"), TEXT("china"), FLinearColor(FColor(37, 184, 200)));
			Bruno.bAlly = true; Bruno.bShowEco = true; Bruno.Money = TEXT("$2,140"); Bruno.PowerUsed = 80; Bruno.PowerMade = 100;
			Bruno.Superweapons = { SW(TEXT("Nuclear Missile"), 250) };
			FRTSHudPlayer Ai1 = Player(TEXT("Hard AI 1"), TEXT("gla"), PRed);
			Ai1.Superweapons = { SW(TEXT("Plague Storm"), 125), SW(TEXT("Plague Storm"), 320) };
			FRTSHudPlayer Ai2 = Player(TEXT("Hard AI 2"), TEXT("china"), FLinearColor(FColor(240, 138, 36)));
			Ai2.bSpied = true; Ai2.SpyLeft = 0.6f; Ai2.bShowEco = true; Ai2.Money = TEXT("$3,900"); Ai2.PowerUsed = 90; Ai2.PowerMade = 150;
			Ai2.Superweapons = { SW(TEXT("Nuclear Missile"), 42), SW(TEXT("Nuclear Missile"), 90), SW(TEXT("Nuclear Missile"), 295),
				SW(TEXT("Nuclear Missile"), 362), SW(TEXT("Nuclear Missile"), 460) };
			FRTSHudPlayer Caio = Player(TEXT("Caio"), TEXT("gla"), FLinearColor(FColor(60, 176, 67)));
			FRTSHudPlayer Davi = Player(TEXT("Davi"), TEXT("usa"), FLinearColor(FColor(164, 108, 224)));
			Davi.Superweapons = { SW(TEXT("Orbital Lance"), 225) };
			FRTSHudPlayer Ai3 = Player(TEXT("Hard AI 3"), TEXT("usa"), FLinearColor(FColor(230, 198, 41)));
			FRTSHudPlayer Ai4 = Player(TEXT("Hard AI 4"), TEXT("gla"), FLinearColor(FColor(230, 90, 166))); Ai4.bDead = true;
			for (FRTSHudPlayer* P : { &Ai1, &Ai2, &Caio, &Davi, &Ai3, &Ai4 })
			{
				P->Team = 1;
			}
			St.Players = { Me, Bruno, Ai1, Ai2, Caio, Davi, Ai3, Ai4 };
		}
	}

	void Panel(FRTSHudState& St)
	{
		St.bCommandPanel = true;
		St.Main.SetNum(18);
		St.Actions.SetNum(6);
		St.bDrawCursor = true;
	}
}

FRTSHudState MakeHudLabState(int32 Index, int32 MainColumns)
{
	FRTSHudState St;
	Common(St, Index == 5);
	if (Index == 6)
	{
		// o 6: o 1 com várias superarmas dos dois lados (a caixa cresce pra baixo, até 3, e o resto em "+n more")
		St.Players[0].Superweapons = { SW(TEXT("Orbital Lance"), 260), SW(TEXT("Orbital Lance"), 425) };
		St.Players[1].Superweapons = { SW(TEXT("Plague Storm"), 38), SW(TEXT("Plague Storm"), 170), SW(TEXT("Scud Storm"), 192), SW(TEXT("Scud Storm"), 400) };
		Index = 1;
	}

	if (Index == 1)
	{
		Panel(St);
		St.Main[0] = Unit(TEXT("unit_0"), TEXT("btn:usa_battle_tank"), 0.80f);
		St.Main[1] = Unit(TEXT("unit_1"), TEXT("btn:usa_battle_tank"), 1.00f, true);
		St.Main[2] = Unit(TEXT("unit_2"), TEXT("btn:usa_battle_tank"), 0.55f);
		St.Main[3] = Unit(TEXT("unit_3"), TEXT("btn:usa_battle_tank"), 0.25f);
		St.Actions[0] = Action(TEXT("cmd_attack_move"), 'A', TEXT("Attack Move"), TEXT("icon:attack_move"));
		St.Actions[1] = Action(TEXT("cmd_stop"), 'S', TEXT("Stop"), TEXT("icon:stop"));
		St.Actions[4] = Action(TEXT("cmd_guard"), 'G', TEXT("Guard"), TEXT("icon:guard"));
		St.TooltipFor = TEXT("cmd_attack_move");
		St.TooltipTitle = TEXT("Attack Move"); St.TooltipKey = TEXT("A");
		St.TooltipLine = TEXT("Move and attack anything on the way.");
		St.Card.bShow = true; St.Card.Name = TEXT("USA Battle Tank"); St.Card.Portrait = TEXT("card:usa_battle_tank");
		St.Card.Health = 480; St.Card.MaxHealth = 600; St.Card.Insignia = TEXT("icon:armor_upgrade"); St.Card.Rank = 2;
	}
	else if (Index == 2)
	{
		Panel(St);
		FRTSHudSlot Tank = Build(TEXT("eco_produce_usa_battle_tank"), 'Q', TEXT("Battle Tank"), TEXT("$900"), TEXT("btn:usa_battle_tank"));
		Tank.Count = 3; Tank.Progress = 0.64f;
		St.Main[0] = Tank;
		St.Main[1] = Locked(Build(TEXT("eco_produce_heavy_tank"), 'W', TEXT("Heavy Tank"), TEXT("$1,100"), TEXT("btn:usa_heavy_tank")));
		St.Main[2] = Build(TEXT("eco_produce_humvee"), 'E', TEXT("Humvee"), TEXT("$700"), TEXT("btn:usa_humvee"));
		St.Main[3] = Build(TEXT("eco_produce_rocket"), 'R', TEXT("Rocket Launcher"), TEXT("$1,200"), TEXT("btn:usa_rocket_launcher"));
		St.Main[4] = Build(TEXT("eco_produce_ambulance"), 'T', TEXT("Ambulance"), TEXT("$600"), TEXT("btn:usa_ambulance"));
		St.Main[5] = Build(TEXT("eco_produce_aa"), 'Y', TEXT("Anti-Air"), TEXT("$1,000"), TEXT("btn:usa_anti_air"));
		St.Main[6] = Build(TEXT("eco_produce_drone"), 'U', TEXT("Drone"), TEXT("$500"), TEXT("btn:usa_drone"));
		St.Main[9] = Locked(Build(TEXT("eco_produce_ew_tank"), 'Z', TEXT("EW Tank"), TEXT("$800"), TEXT("btn:usa_ew_tank")));
		FRTSHudSlot Kit = Upgrade(Build(TEXT("upg_missile_kit"), 'X', TEXT("Missile Kit"), FString(), TEXT("btn:upg_missile_kit"))); Kit.bDone = true;
		St.Main[10] = Kit;
		St.Main[11] = Upgrade(Build(TEXT("upg_battle_drone"), 'C', TEXT("Battle Drone"), TEXT("$100"), TEXT("btn:upg_battle_drone")));
		St.Actions[3] = Action(TEXT("cmd_rally"), 'F', TEXT("Rally Point"), TEXT("icon:rally_point"));
		St.TooltipFor = TEXT("eco_produce_usa_battle_tank");
		St.TooltipTitle = TEXT("Battle Tank"); St.TooltipKey = TEXT("Q"); St.TooltipCost = TEXT("$900 · 0:12");
		St.TooltipLine = TEXT("Left-click +1 · Shift-click +5 · Right-click −1");
		St.Card.bShow = true; St.Card.Name = TEXT("War Factory"); St.Card.Portrait = TEXT("card:usa_war_factory");
		St.Card.Health = 1800; St.Card.MaxHealth = 2000;
		FRTSHudSlot Q0 = Build(TEXT("q0"), 0, TEXT("Battle Tank"), FString(), TEXT("btn:usa_battle_tank")); Q0.Progress = 0.64f;
		FRTSHudSlot Q1 = Build(TEXT("q1"), 0, TEXT("Battle Tank"), FString(), TEXT("btn:usa_battle_tank")); Q1.bCancelMark = true;
		FRTSHudSlot Q2 = Build(TEXT("q2"), 0, TEXT("Battle Tank"), FString(), TEXT("btn:usa_battle_tank"));
		FRTSHudSlot Q3 = Build(TEXT("q3"), 0, TEXT("Humvee"), FString(), TEXT("btn:usa_humvee"));
		St.Card.Queue = { Q0, Q1, Q2, Q3, FRTSHudSlot() };
	}
	else if (Index == 3)
	{
		Panel(St);
		St.Main[0] = Build(TEXT("eco_build_cc"), 'Q', TEXT("Command Center"), TEXT("$2,000"), TEXT("btn:usa_command_center"));
		St.Main[1] = Build(TEXT("eco_build_power"), 'W', TEXT("Power Plant"), TEXT("$800"), TEXT("btn:usa_power_plant"));
		St.Main[2] = Build(TEXT("eco_build_barracks"), 'E', TEXT("Barracks"), TEXT("$600"), TEXT("btn:usa_barracks"));
		St.Main[3] = Build(TEXT("eco_build_supply"), 'R', TEXT("Supply Center"), TEXT("$1,500"), TEXT("btn:usa_supply_center"));
		St.Main[4] = Build(TEXT("eco_build_war_factory"), 'T', TEXT("War Factory"), TEXT("$2,000"), TEXT("btn:usa_war_factory"));
		St.Main[5] = Locked(Build(TEXT("eco_build_airfield"), 'Y', TEXT("Airfield"), TEXT("$1,000"), TEXT("btn:usa_airfield")));
		St.Main[6] = Locked(Build(TEXT("eco_build_tech"), 'U', TEXT("Tech Center"), TEXT("$2,500"), TEXT("btn:usa_tech_center")));
		St.Main[7] = Build(TEXT("eco_build_missile"), 'I', TEXT("Missile Defense"), TEXT("$1,000"), TEXT("btn:usa_missile_defense"));
		St.Main[8] = Locked(Build(TEXT("eco_build_firebase"), 'O', TEXT("Fire Base"), TEXT("$1,000"), TEXT("btn:usa_fire_base")));
		St.Main[9] = Build(TEXT("eco_build_radar"), 'Z', TEXT("Radar"), TEXT("$1,200"), TEXT("btn:usa_radar"));
		St.Main[10] = Locked(Build(TEXT("eco_build_dropzone"), 'X', TEXT("Drop Zone"), TEXT("$2,500"), TEXT("btn:usa_drop_zone")));
		FRTSHudSlot Lance = Locked(Build(TEXT("eco_build_orbital"), 'C', TEXT("Orbital Lance"), TEXT("$5,000"), TEXT("btn:usa_orbital_lance"))); Lance.bNoMoney = true;
		St.Main[11] = Lance;
		St.Actions[1] = Action(TEXT("cmd_stop"), 'S', TEXT("Stop"), TEXT("icon:stop"));
		St.TooltipFor = TEXT("eco_build_airfield");
		St.TooltipTitle = TEXT("Airfield"); St.TooltipKey = TEXT("Y"); St.TooltipCost = TEXT("$1,000");
		St.TooltipLine = TEXT("Requires: War Factory"); St.bTooltipLineRed = true;
		St.Card.bShow = true; St.Card.Name = TEXT("USA Dozer"); St.Card.Portrait = TEXT("card:usa_dozer");
		St.Card.Health = 250; St.Card.MaxHealth = 250;
	}
	else if (Index == 4)
	{
		Panel(St);
		FRTSHudSlot Armor = Upgrade(Build(TEXT("upg_composite_armor"), 'Q', TEXT("Composite Armor"), TEXT("$2,000"), TEXT("icon:armor_upgrade"))); Armor.Progress = 0.40f;
		St.Main[0] = Armor;
		FRTSHudSlot Training = Upgrade(Build(TEXT("upg_training"), 'W', TEXT("Advanced Training"), FString(), TEXT("icon:idle_units"))); Training.bDone = true;
		St.Main[1] = Training;
		St.Main[2] = Upgrade(Build(TEXT("upg_drone_armor"), 'E', TEXT("Drone Armor"), TEXT("$1,000"), TEXT("icon:spy_drone")));
		St.Main[3] = Locked(Upgrade(Build(TEXT("upg_laser"), 'R', TEXT("Laser Guidance"), TEXT("$2,000"), TEXT("icon:attack_move"))));
		St.Actions[5] = Action(TEXT("cmd_sell"), 'H', TEXT("Sell"), TEXT("icon:sell"));
		St.TooltipFor = TEXT("upg_composite_armor");
		St.TooltipTitle = TEXT("Composite Armor"); St.TooltipKey = TEXT("Q"); St.TooltipCost = TEXT("$2,000 · 1:00");
		St.TooltipLine = TEXT("+25% armor for all tanks.");
		St.Card.bShow = true; St.Card.Name = TEXT("Tech Center"); St.Card.Health = 2400; St.Card.MaxHealth = 2400;
		FRTSHudSlot Q0 = Build(TEXT("q0"), 0, TEXT("Composite Armor"), FString(), TEXT("icon:armor_upgrade")); Q0.Progress = 0.40f;
		St.Card.Queue = { Q0, FRTSHudSlot(), FRTSHudSlot(), FRTSHudSlot(), FRTSHudSlot() };
	}
	else
	{
		St.bGroupsAtBottom = true;
		St.Card.bShow = true; St.Card.bFactionPlate = true;
		St.Card.FactionName = TEXT("UNITED STATES"); St.Card.FactionEmblem = TEXT("emblem:usa");
		St.Card.PlateLine = TEXT("Army 24 · Buildings 9");
		St.Card.PlateKills = TEXT("KILLS   Units 31 · Buildings 6");
	}
	St.MainColumns = FMath::Clamp(MainColumns, 5, 9);
	if (St.MainColumns < 9)
	{
		// os dados acima seguem a grade da planta (2 × 9, com a 2ª fileira na casa 9); com menos colunas as
		// casas enchem em ordem, sem buraco
		TArray<FRTSHudSlot> Packed;
		for (const FRTSHudSlot& Slot : St.Main)
		{
			if (!Slot.bEmpty)
			{
				Packed.Add(Slot);
			}
		}
		Packed.SetNum(2 * St.MainColumns);
		St.Main = MoveTemp(Packed);
	}
	const TCHAR* Row1 = TEXT("QWERTYUIO");
	const TCHAR* Row2 = TEXT("ZXCVBNM,.");
	for (int32 I = 0; I < St.Main.Num() && I < 2 * St.MainColumns; ++I)
	{
		if (St.Main[I].Key != 0)
		{
			St.Main[I].Key = (I < St.MainColumns ? Row1 : Row2)[I % St.MainColumns];
		}
	}
	return St;
}

// ---------------------------------------------------------------------------------------------
// os formatos do protótipo aprovado (docs/concept/hud/prototipo/jogadores.html, makePlayers): os mesmos jogadores, cores, números e
// superarmas, gerados pela mesma conta, pra a foto do HUD sair comparável lado a lado com a do protótipo

namespace
{
	double Rnd(int32 I, int32 K)
	{
		const double X = FMath::Sin(double(I) * 12.9898 + double(K) * 78.233) * 43758.5453;
		return X - FMath::FloorToDouble(X);
	}

	int32 Round(double V)
	{
		return int32(FMath::FloorToDouble(V + 0.5));
	}
}

bool ApplyHudLabFormat(FRTSHudState& St, const FString& Format, const FString& MyFaction, bool bAbuse, const FString& MeHex)
{
	static const uint32 Colors[16] = { 0x4d8fe0, 0xe0443a, 0x3cb043, 0xe6c629, 0xf08a24, 0xa46ce0, 0x25b8c8, 0xe65aa6,
		0xd9dde3, 0xb07a44, 0x1f9e86, 0x9ad13a, 0xc43fd1, 0x5b7fd6, 0xc9a24a, 0x8a96a2 };
	static const TCHAR* Facs[4] = { TEXT("usa"), TEXT("china"), TEXT("gla"), TEXT("russia") };
	static const TCHAR* Names[16] = { TEXT("Felipe"), TEXT("Bruno"), TEXT("Caio"), TEXT("Davi"), TEXT("Edu"), TEXT("Fabio"), TEXT("Gui"), TEXT("Hugo"),
		TEXT("Igor"), TEXT("Joao"), TEXT("Kaue"), TEXT("Leo"), TEXT("Marco"), TEXT("Nina"), TEXT("Otto"), TEXT("Paulo") };
	TArray<int32> Sizes;
	FString Label;
	if (Format == TEXT("duel")) { Sizes = { 1, 1 }; Label = TEXT("1 vs 1"); }
	else if (Format == TEXT("2v2")) { Sizes = { 2, 2 }; Label = TEXT("2 vs 2"); }
	else if (Format == TEXT("3v5")) { Sizes = { 3, 5 }; Label = TEXT("3 vs 5"); }
	else if (Format == TEXT("8v8")) { Sizes = { 8, 8 }; Label = TEXT("8 vs 8"); }
	else if (Format == TEXT("ffa6")) { Sizes.Init(1, 6); Label = TEXT("Free for all"); }
	else if (Format == TEXT("ffa16")) { Sizes.Init(1, 16); Label = TEXT("Free for all"); }
	else if (Format == TEXT("4x4")) { Sizes = { 4, 4, 4, 4 }; Label = TEXT("4 teams of 4"); }
	else if (Format == TEXT("3v5v8")) { Sizes = { 3, 5, 8 }; Label = TEXT("3 vs 5 vs 8"); }
	else return false;

	auto SwOf = [](const FString& Fac) -> FString
	{
		return Fac == TEXT("china") ? TEXT("Nuclear Missile") : Fac == TEXT("gla") ? TEXT("Plague Storm") : Fac == TEXT("russia") ? TEXT("Thermobaric Strike") : TEXT("Orbital Lance");
	};
	St.Players.Reset();
	St.ModeLabel = Label;
	TArray<FString> PlayerFac;
	int32 N = 0;
	for (int32 T = 0; T < Sizes.Num(); ++T)
	{
		for (int32 J = 0; J < Sizes[T]; ++J, ++N)
		{
			const FString Fac = N == 0 ? MyFaction : FString(Facs[FMath::FloorToInt(Rnd(N, 1) * 4)]);
			const uint32 C = Colors[N % 16];
			const FString Name = N == 0 ? FString(TEXT("Felipe")) : (Rnd(N, 2) < 0.3 ? FString::Printf(TEXT("Hard AI %d"), N) : FString(Names[N % 16]));
			FRTSHudPlayer P = Player(Name, *Fac, FLinearColor(FColor((C >> 16) & 255, (C >> 8) & 255, C & 255)));
			P.Team = T;
			P.bMe = N == 0;
			P.bAlly = T == 0 && N > 0;
			P.bDead = N > 0 && Rnd(N, 3) < 0.12;
			P.bSpied = T != 0 && Rnd(N, 4) < 0.18;
			P.SpyLeft = 0.6f;
			P.bShowEco = P.bAlly || P.bSpied;
			const int32 Rank = 1 + FMath::FloorToInt(Rnd(N, 5) * 5);
			P.RankIcon = Fac == TEXT("usa") ? FName(*FString::Printf(TEXT("skin:rank_usa_%d"), Rank)) : FName(*FString::Printf(TEXT("skin:%s/rank_%s_%d"), *Fac, *Fac, Rank));
			P.Score = Round(800 + Rnd(N, 6) * 5200);
			P.Cash = Round(300 + Rnd(N, 8) * 9000);
			P.Income = Round(200 + Rnd(N, 9) * 1300);
			P.Army = Round(1000 + Rnd(N, 10) * 22000);
			P.Lost = FMath::FloorToInt(Rnd(N, 11) * 40);
			P.Destroyed = FMath::FloorToInt(Rnd(N, 12) * 60);
			int32 Sw = Rnd(N, 13) < 0.55 ? Round(Rnd(N, 14) < 0.15 ? 0.0 : 20 + Rnd(N, 14) * 420) : -1;
			P.PowerMade = 60 + Round(Rnd(N, 15) * 100);
			P.PowerUsed = Round(P.PowerMade * (0.6 + Rnd(N, 16) * 0.55));
			P.Money = FString::Printf(TEXT("$%d"), P.Cash);
			if (P.bDead)
			{
				Sw = -1;
			}
			if (Sw >= 0)
			{
				P.Superweapons.Add(SW(SwOf(Fac), Sw));
				if (Rnd(N, 17) < 0.3)
				{
					P.Superweapons.Add(SW(SwOf(Fac), FMath::Min(600, Sw + 60 + Round(Rnd(N, 18) * 200))));
				}
			}
			if (Fac == TEXT("gla") && !P.bDead && Rnd(N, 19) < 0.6)
			{
				P.Superweapons.Add(SW(TEXT("Scud Storm"), Round(30 + Rnd(N, 20) * 400)));
			}
			St.Players.Add(P);
			PlayerFac.Add(Fac);
		}
	}
	auto FirstEnemy = [&](bool bGla) -> int32
	{
		for (int32 I = 0; I < St.Players.Num(); ++I)
		{
			const FRTSHudPlayer& P = St.Players[I];
			if (!P.bMe && !P.bAlly && !P.bDead && (!bGla || PlayerFac[I] == TEXT("gla")))
			{
				return I;
			}
		}
		return INDEX_NONE;
	};
	if (bAbuse)
	{
		// o abuso: um inimigo com 240 superarmas, 37 prontas
		const int32 A = FirstEnemy(false);
		if (A != INDEX_NONE)
		{
			St.Players[A].Superweapons.Reset();
			for (int32 K = 0; K < 240; ++K)
			{
				St.Players[A].Superweapons.Add(SW(SwOf(PlayerFac[A]), K < 37 ? 0 : 20 + (K * 37) % 560));
			}
		}
	}
	// o caso dele: um jogador com 3 tipos (2 Plague Storm, 3 Scud Storm e um Nuclear Missile capturado da China)
	int32 G = FirstEnemy(true);
	if (G == INDEX_NONE)
	{
		G = FirstEnemy(false);
	}
	if (G != INDEX_NONE && !(bAbuse && St.Players[G].Superweapons.Num() > 100))
	{
		St.Players[G].Superweapons = { SW(TEXT("Plague Storm"), 0), SW(TEXT("Plague Storm"), 255), SW(TEXT("Scud Storm"), 84), SW(TEXT("Scud Storm"), 190),
			SW(TEXT("Scud Storm"), 410), SW(TEXT("Nuclear Missile"), 0) };
	}
	if (!MeHex.IsEmpty())
	{
		// a minha cor trocada: quem tinha essa cor fica com a minha
		const FLinearColor Mine = FLinearColor(FColor::FromHex(MeHex));
		for (int32 I = 1; I < St.Players.Num(); ++I)
		{
			if (St.Players[I].Color.Equals(Mine, 0.002f))
			{
				St.Players[I].Color = St.Players[0].Color;
			}
		}
		St.Players[0].Color = Mine;
	}
	return true;
}
