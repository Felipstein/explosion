#include "Generation/WorldDetail.h"
#include "Async/ParallelFor.h"

/*
 * A vila do kit civil (pedido do jogo e decisão do Felipe, 11/10): uma vila de
 * estrada como as das artes (docs/concept/scenes/desert_golden_hour/best/
 * day_game_cam.webp e a maquete do Look, Game/Tools/look_layout.py): a estrada
 * de asfalto reta pelo meio, um cruzamento (ou T, se um lado não couber), casas
 * em fileira viradas pra rua, quintais murados atrás, palmeira no quintal,
 * postes, carcaças e miudezas.
 *
 * O método (estudo e revisão de 11/10, docs/WORLDFORGE.md §12):
 * - a rua primeiro, depois os lotes (Parish & Müller 2001; Emilien et al. 2012:
 *   o lote toma um trecho da rua e cresce pra trás dela; Vanegas et al. 2012);
 * - vila de estrada (povoamento linear), que engrossa no cruzamento;
 * - casa virada pra dentro, quintal murado com portão (Hakim, "Arabic-Islamic Cities");
 * - jogo: a rua tem 20 m de fachada a fachada e a transversal 16 m; nada de
 *   passagem de 8–12 m; a vila estreita o caminho entre as bases mas não fecha,
 *   e sobra um desvio por fora da rua (conferido na grade de 4 m, tanque com
 *   uma célula de folga em volta);
 * - o lote que cai no lago, no morro, na base ou no depósito sai: a vila toma a
 *   forma do lugar (o contorno orgânico da maquete);
 * - um platô por quarteirão (o muro entre dois lotes não fica em degrau);
 * - a câmera do jogo (37,5°, nasce a 45°): o sobrado só onde a frente olha pra
 *   câmera (esconde o próprio quintal, não a rua);
 * - justiça: a mesma vila em cada lado do mapa, girada como o mapa.
 *
 * Tudo na grade de 4 m do jogo (canto em −Half), giro em passos de 90°.
 *
 * A vila v3 (13/10, o Felipe comparou com a maquete do Look, volta 15, e pediu "traga o que o Look
 * acertou", a estrada um pouco maior e a rua da vila um pouco menor que a da maquete, como nas
 * favoritas; segunda opinião do revisor):
 * - o cruzamento no meio (4 braços) e as travessas de terra nas pontas; 20 m de fachada a fachada
 *   na principal com 13,5 m de asfalto (10 m fora da vila, 24 m de transição), 16 m e 10,5 m na
 *   transversal; a esquina arredondada e a beira quebrada com língua de areia (WorldDetail.h);
 * - ~40% de quintais murados, o resto casa solta na areia (às vezes com a casa dos fundos e um
 *   resto de muro) e lotes vazios; areia até o muro;
 * - carcaças e picape no acostamento, quase paralelas à rua (yaw livre no JSON);
 * - miudezas em grupo no pé do muro, do lado da casa e no acostamento (não bloqueiam; sorteio
 *   próprio, não espelhado);
 * - postes sem bloqueio fora da pista e de toda peça, a cruzeta perpendicular à linha, e os fios em
 *   catenária de poste a poste e até a parede das casas (PlaceVillagePoles).
 */

namespace
{
	constexpr float CellM = 4.0f;

	/** O kit (a malha no giro 0: W ao longo de X, D ao longo de Y, a frente em +Y). */
	struct FKit
	{
		const TCHAR* Type;
		float W, D, H;
		bool bBlocks;
	};
	const FKit Kits[] = {
		{ TEXT("civ_house_1story"), 8.0f, 8.0f, 4.5f, true },
		{ TEXT("civ_house_2story"), 8.0f, 8.0f, 7.0f, true },
		{ TEXT("civ_house_porch"), 8.0f, 12.0f, 4.5f, true },
		{ TEXT("civ_house_long"), 16.0f, 8.0f, 4.5f, true },
		{ TEXT("civ_wall"), 4.0f, 0.5f, 2.3f, true },
		{ TEXT("civ_wall_gate"), 4.0f, 0.5f, 2.6f, true },
		{ TEXT("civ_car_burnt"), 2.0f, 4.6f, 1.5f, true },
		{ TEXT("civ_pickup"), 2.0f, 5.2f, 1.9f, true },
		{ TEXT("civ_sandbags"), 4.0f, 1.6f, 1.1f, true },
		{ TEXT("civ_pole"), 0.4f, 0.4f, 8.0f, true },
		{ TEXT("civ_crates"), 2.4f, 2.4f, 1.6f, false },
		{ TEXT("civ_barrels"), 2.0f, 2.0f, 1.2f, false },
		{ TEXT("civ_ammo"), 1.6f, 1.2f, 0.8f, false },
		// as miudezas da maquete do Look (13/10; captures/look/kit_vila/manifest.json, pivô no pé,
		// provisórios de quina dura): laje, chão e o toldo da parede. Nenhuma bloqueia
		{ TEXT("LL_Prop_WaterTank"), 1.1f, 1.1f, 0.84f, false },
		{ TEXT("LL_Prop_AC"), 0.9f, 0.6f, 0.55f, false },
		{ TEXT("LL_Prop_Dish"), 0.8f, 0.8f, 0.8f, false },
		{ TEXT("LL_Prop_Laundry"), 2.44f, 0.4f, 1.4f, false },
		{ TEXT("LL_Prop_Awning"), 3.0f, 1.67f, 2.44f, false },
		{ TEXT("LL_Prop_Rubble0"), 3.7f, 3.06f, 0.97f, false },
		{ TEXT("LL_Prop_Rubble1"), 4.02f, 3.94f, 1.17f, false },
		{ TEXT("LL_Prop_Rubble2"), 3.7f, 5.13f, 1.08f, false },
		{ TEXT("LL_Prop_Scrap0"), 1.86f, 2.71f, 0.28f, false },
		{ TEXT("LL_Prop_Scrap1"), 3.33f, 2.53f, 0.42f, false },
		{ TEXT("LL_Prop_Fence0"), 4.02f, 0.24f, 1.91f, false },
		{ TEXT("LL_Prop_Fence1"), 3.22f, 0.72f, 1.72f, false },
		{ TEXT("LL_Prop_BrokenWall0"), 4.0f, 2.93f, 2.33f, false },
		{ TEXT("LL_Prop_BrokenWall1"), 4.0f, 2.36f, 2.33f, false },
		{ TEXT("LL_Prop_Tires"), 1.65f, 1.09f, 0.66f, false },
		{ TEXT("LL_Prop_Barrels"), 1.94f, 1.58f, 0.9f, false },
		{ TEXT("LL_Prop_Crates"), 2.47f, 2.08f, 1.35f, false },
		{ TEXT("LL_Prop_Ammo"), 1.07f, 0.8f, 0.64f, false },
	};
	const FKit& KitOf(const TCHAR* Type)
	{
		for (const FKit& K : Kits) if (FCString::Strcmp(K.Type, Type) == 0) return K;
		return Kits[0];
	}

	/** O giro de um vetor em passos de 90° (yaw da Unreal: X → Y). */
	FVector2f Spin(FVector2f V, int32 Q)
	{
		switch (((Q % 4) + 4) % 4)
		{
		case 1: return FVector2f(-V.Y, V.X);
		case 2: return FVector2f(-V.X, -V.Y);
		case 3: return FVector2f(V.Y, -V.X);
		default: return V;
		}
	}
	/** A frente de quem tem o giro Rot (0: +Y). */
	FVector2f FrontOf(int32 Rot) { return Spin(FVector2f(0.0f, 1.0f), Rot); }

	struct FRect
	{
		FVector2f Min = FVector2f::ZeroVector, Max = FVector2f::ZeroVector;
		bool IsEmpty() const { return Max.X - Min.X < 0.01f || Max.Y - Min.Y < 0.01f; }
		FVector2f Center() const { return 0.5f * (Min + Max); }
	};

	/** Um lote na planta local (u ao longo da rua, v pra fora dela). */
	struct FLot
	{
		FRect Lot, House, Yard, Fore;      // o lote, a casa, o quintal de trás e o da frente (recuo)
		const TCHAR* Type = nullptr;       // a casa (nullptr: quintal sem casa)
		int32 Front = 0;                   // o giro da frente (olha a rua)
		int32 Block = 0;                   // o quarteirão (um platô por quarteirão)
		bool bTall = false;                // pode virar sobrado (se olhar pra câmera)
		bool bArm = false;                 // na transversal
		int32 Arm = -1;                    // qual transversal
		int32 CrossSide = -1;              // o lado do quintal que dá pra transversal (portão ali)
		int32 Style = 0;                   // 0 quintal murado, 1 casa solta, 2 pomar murado, 3 lote aberto
		FRect BackHouse;                   // a casa dos fundos da solta
		int32 BackRot = 0;
		// o resto de muro da solta (bloqueia: sorteado aqui pra o irmão girado ter o mesmo)
		int32 LooseSide = -1, LooseCount = 0;
		bool bLooseLo = true;
	};

	/** Uma peça na planta local. */
	struct FLocal
	{
		const TCHAR* Type;
		FVector2f C;
		int32 Rot;
		FRect Foot;            // a pegada na grade (local)
		int32 Lot = -1;
		int32 Street = -1;
		float YawDeg = TNumericLimits<float>::Lowest();   // o giro livre (local); sem: 90 × Rot
		bool bLoose = false;   // miudeza solta: não bloqueia, mesmo sendo de um tipo que bloqueia
		float Lift = 0.0f;     // acima do piso (m): a laje das casas
		int32 Parent = -1;     // a casa da peça de laje (vira sobrado na cópia: sobe 3 m)
	};
}

void FWorldDetail::BuildVillages(const FWorldMap& M, float MainHalf)
{
	const double T0 = FPlatformTime::Seconds();
	const float Half = M.Half();
	const int32 G4 = FMath::FloorToInt(2.0f * Half / CellM);
	// Villages = 1: uma vila só, no centro do mapa, compacta, desenhada com a
	// simetria do mapa (cada lote tem o irmão girado de 180°: justa pros dois
	// lados; decisão do Felipe em 11/10). Villages = 2: uma vila em cada lado.
	const bool bCenter = M.Settings.Villages == 1;
	const int32 Copies = bCenter ? 1 : (M.Settings.Players == 4 ? 4 : 2);
	const int32 Step = FMath::Max(1, 4 / Copies);

	// ---- as áreas que não pode tapar: bases, campos e as do jogo -----------------
	struct FKeep { FVector2f P; float R; bool bBase; };
	TArray<FKeep> Keep;
	for (const FWorldStart& S : M.Starts) Keep.Add({ S.Pos, FMath::Max(88.0f, M.BaseRadius + 30.0f), true });
	for (const FWorldResource& R : M.Resources) Keep.Add({ R.Pos, R.Radius + 20.0f, false });
	for (const FVector3f& K : M.Settings.KeepOut) Keep.Add({ FVector2f(K.X, K.Y), K.Z, K.Z >= 60.0f });   // raio ≥ 60 m = base

	// ---- a planta local: a rua em v = 0 ao longo de u, o cruzamento em u = 0 ------
	// (no centro a rua passa na linha da grade que cruza o meio do mapa: 24 m de
	// rua pra fachada cair na grade; compacta, sem esparramar)
	const float Len = bCenter ? 100.0f : 88.0f;    // meio comprimento ao longo da rua (a fita cresce até onde o terreno deixa)
	// 20 m de fachada a fachada (o revisor: a largura se mede pelo vão na grade de 4 m; nada de
	// passagem de 8–12 m) com 13,5 m de asfalto: 3,25 m de acostamento de cada lado
	const float Face = 10.0f;
	const float Band = Face + 20.0f;               // o fundo dos lotes da principal
	const float ArmEnd = Band + 24.0f;             // até onde vão os lotes da transversal

	const uint32 Seed = WorldNoise::Hash(uint32(M.Settings.Seed) * 2654435761u + 0x51a6e7u);   // a semente só da vila
	WorldNoise::FRng Rng(Seed);
	TArray<FLot> Lots;
	auto Rect = [](bool bAlongU, float A0, float A1, float V0, float V1)
	{
		FRect R;
		if (bAlongU) { R.Min = FVector2f(A0, FMath::Min(V0, V1)); R.Max = FVector2f(A1, FMath::Max(V0, V1)); }
		else { R.Min = FVector2f(FMath::Min(V0, V1), A0); R.Max = FVector2f(FMath::Max(V0, V1), A1); }
		return R;
	};
	// uma fileira: de A0 a A1 ao longo da rua, fachada em F, fundo em B
	int32 Blocks = 0;
	auto Row = [&](float A0, float A1, float F, float B, bool bAlongU, int32 Front, int32 CornerEnd, int32 CrossSide, int32 ArmIndex)
	{
		const bool bArm = ArmIndex >= 0;
		const int32 Block = Blocks++;
		const float Dir = B > F ? 1.0f : -1.0f;
		float A = A0;
		while (A1 - A >= 8.0f - 0.01f)
		{
			const float Width = (A1 - A >= 16.0f - 0.01f && Rng.Float() < 0.25f) ? 16.0f : 8.0f;
			// a esquina do cruzamento: CornerEnd 0 = em A0, 1 = em A1
			const bool bCorner = (CornerEnd == 0 && A == A0) || (CornerEnd == 1 && A + Width >= A1 - 0.01f);
			FLot& L = Lots.AddDefaulted_GetRef();
			L.Front = Front;
			L.Block = Block;
			L.bArm = bArm;
			L.Arm = ArmIndex;
			L.CrossSide = bCorner ? CrossSide : -1;
			L.Lot = Rect(bAlongU, A, A + Width, F, B);
			// o estilo (13/10): ~30% quintal murado (a esquina, metade das vezes), ~56% casa solta na
			// areia, ~5% pomar murado e ~9% lote aberto (antes era tudo murado: a grade repetida; a
			// maquete tem 2 quintais murados em 14 casas, as favoritas mais)
			const float R = Rng.Float();
			L.Style = (bCorner && R < 0.5f) ? 0 : (R < 0.30f ? 0 : (R < 0.86f ? 1 : (R < 0.91f ? 2 : 3)));
			if (bCorner && L.Style >= 2) L.Style = 1;   // a esquina sempre tem casa
			if (L.Style >= 2)
			{
				// sem casa: o pomar murado com palmeira das artes ou o lote aberto (miudeza e moita)
				L.Type = nullptr;
				L.Yard = L.Lot;
			}
			else
			{
				if (Width >= 16.0f) L.Type = TEXT("civ_house_long");
				else
				{
					const float T = Rng.Float();
					L.Type = T < 0.3f ? TEXT("civ_house_porch") : TEXT("civ_house_1story");
				}
				// o sobrado: na esquina e em ~15% das outras (vira sobrado se olhar pra câmera)
				L.bTall = Width < 16.0f && FCString::Strcmp(L.Type, TEXT("civ_house_1story")) == 0 && (bCorner || Rng.Float() < 0.15f);
				// recuo de 4 m em ~25% (o pátio da frente murado): a fachada desencontrada
				const float Depth = KitOf(L.Type).D;
				const float Set = (!bCorner && Depth <= 8.0f && Rng.Float() < 0.25f) ? CellM : 0.0f;
				const float H0 = F + Dir * Set, H1 = H0 + Dir * Depth;
				L.House = Rect(bAlongU, A, A + Width, H0, H1);
				if (Set > 0.0f && L.Style == 0) L.Fore = Rect(bAlongU, A, A + Width, F, H0);
				if (FMath::Abs(B - H1) >= CellM - 0.01f) L.Yard = Rect(bAlongU, A, A + Width, H1, B);
				// a casa dos fundos da solta (~35%; a maquete tem a fileira de trás a 9–24 m da rua):
				// 8×8 no fundo do lote, 4 m atrás da da frente, a porta virada pro lado
				if (L.Style == 1 && Width < 16.0f && FMath::Abs(B - H1) >= 12.0f - 0.01f && Rng.Float() < 0.35f)
				{
					L.BackHouse = Rect(bAlongU, A, A + Width, B - Dir * 8.0f, B);
					L.BackRot = (Front + (Rng.Float() < 0.5f ? 1 : 3)) % 4;
				}
				// o resto de muro da solta: 1–3 peças num lado do quintal (de lado ou no fundo), a partir
				// de um canto, com a ponta solta (a maquete do Look: muros soltos na areia, ~32 m na vila)
				else if (L.Style == 1 && !L.Yard.IsEmpty() && Rng.Float() < 0.35f)
				{
					const int32 Sides[3] = { (Front + 1) % 4, (Front + 3) % 4, (Front + 2) % 4 };
					L.LooseSide = Sides[Rng.Next() % 3u];
					L.LooseCount = 1 + int32(Rng.Next() % 3u);
					L.bLooseLo = Rng.Float() < 0.5f;
				}
			}
			A += Width;
		}
	};
	// as transversais: (u, lado). Fora do centro, o cruzamento no meio (u = 0, os
	// dois lados). No centro a rua passa entre os lagos: as transversais saem a
	// ±56 m pros dois lados, e o terreno decide quais ficam (as do lago morrem)
	// Half: o meio corredor (fachada a fachada); Asphalt: a meia pista. O cruzamento do meio (4 braços
	// com o irmão girado) e, no centro, a travessa de terra de 12 m em cada ponta (um T de cada lado)
	struct FArm { float U; float Side; float Half; float Asphalt; bool bPaved; };
	TArray<FArm> Arms;
	if (bCenter) Arms = { { 0.0f, 1.0f, 8.0f, 5.25f, true }, { 66.0f, 1.0f, 6.0f, 2.6f, false } };
	else Arms = { { 0.0f, 1.0f, 8.0f, 5.25f, true }, { 0.0f, -1.0f, 8.0f, 5.25f, true } };
	// a principal: a fileira de cada lado, cortada onde sai uma transversal
	auto MainRows = [&](float Side)
	{
		TArray<float> Cuts = { -Len };
		for (const FArm& A : Arms)
		{
			if (A.Side != Side && !(bCenter == false && A.U == 0.0f)) continue;
			Cuts.AddUnique(A.U - A.Half);
			Cuts.AddUnique(A.U + A.Half);
		}
		Cuts.Add(Len);
		Cuts.Sort();
		const int32 Front = Side > 0.0f ? 2 : 0;
		for (int32 K = 0; K + 1 < Cuts.Num(); K += 2)
		{
			// as esquinas: a ponta que encosta numa transversal (portão do quintal pra ela)
			const bool bLoCorner = K > 0, bHiCorner = K + 2 < Cuts.Num();
			const int32 Corner = bHiCorner ? 1 : (bLoCorner ? 0 : -1);
			Row(Cuts[K], Cuts[K + 1], Side * Face, Side * Band, true, Front, Corner, bHiCorner ? 3 : 1, -1);
		}
	};
	MainRows(1.0f);
	if (!bCenter) MainRows(-1.0f);
	// os lotes dos dois lados de cada transversal, além da faixa da principal
	for (int32 A = 0; A < Arms.Num(); ++A)
	{
		const FArm& Ar = Arms[A];
		const float V0 = Ar.Side > 0.0f ? Band : -ArmEnd, V1 = Ar.Side > 0.0f ? ArmEnd : -Band;
		Row(V0, V1, Ar.U + Ar.Half, Ar.U + Ar.Half + 20.0f, false, 1, -1, -1, A);
		Row(V0, V1, Ar.U - Ar.Half, Ar.U - Ar.Half - 20.0f, false, 3, -1, -1, A);
	}
	// o irmão girado de 180° de cada lote (só no centro; morre junto)
	TArray<int32> Twin;
	Twin.Init(INDEX_NONE, Lots.Num());
	if (bCenter)
	{
		auto Turn = [](const FRect& R) { FRect T; if (!R.IsEmpty()) { T.Min = -R.Max; T.Max = -R.Min; } return T; };
		const int32 N = Lots.Num();
		const int32 Half0 = Blocks;
		const int32 NArms0 = Arms.Num();
		for (int32 A = 0; A < NArms0; ++A) { FArm T = Arms[A]; T.U = -T.U; T.Side = -T.Side; Arms.Add(T); }
		for (int32 L = 0; L < N; ++L)
		{
			FLot T = Lots[L];
			T.Lot = Turn(Lots[L].Lot);
			T.House = Turn(Lots[L].House);
			T.Yard = Turn(Lots[L].Yard);
			T.Fore = Turn(Lots[L].Fore);
			T.BackHouse = Turn(Lots[L].BackHouse);
			T.BackRot = (Lots[L].BackRot + 2) % 4;
			T.LooseSide = Lots[L].LooseSide >= 0 ? (Lots[L].LooseSide + 2) % 4 : -1;
			T.bLooseLo = !Lots[L].bLooseLo;   // o giro de 180° inverte a ordem ao longo do lado
			T.Front = (Lots[L].Front + 2) % 4;
			T.CrossSide = Lots[L].CrossSide >= 0 ? (Lots[L].CrossSide + 2) % 4 : -1;
			T.Block = Lots[L].Block + Half0;
			T.Arm = Lots[L].Arm >= 0 ? Lots[L].Arm + NArms0 : -1;
			Twin[L] = Lots.Num();
			Lots.Add(T);
			Twin.Add(L);
		}
		Blocks *= 2;
	}

	// os carros largados no acostamento (bloqueiam: entram na conferência do caminho). A maquete do
	// Look e as favoritas: carcaça e picape quase paralelas à rua, sem sacos de areia (13/10). O meio
	// do carro a 1,45 m da beira do asfalto: até ±12° ele fica entre a pista e a fachada; a pegada
	// pega as duas células do lado da rua e sobram ≥ 12 m de passagem. Um na transversal do meio.
	// (pelo comprimento do lugar; sorteio próprio, igual pra qualquer candidato)
	constexpr float StreetHalf = 6.75f;   // o asfalto da principal dentro da vila (13,5 m)
	auto StreetPropsFor = [&Rect, &Arms, Seed, Band](float VLen) -> TArray<FLocal>
	{
		TArray<FLocal> Out;
		WorldNoise::FRng R(Seed + 0x7f4a7c15u);
		// os lugares do acostamento do lado +v, longe das bocas das transversais desse lado
		TArray<float> Us;
		for (float U = -VLen + 12.0f; U <= VLen - 12.0f + 0.01f; U += 8.0f)
		{
			bool bOk = true;
			for (const FArm& A : Arms) if (A.Side > 0.0f && FMath::Abs(U - A.U) < A.Half + 8.0f) bOk = false;
			if (bOk) Us.Add(U);
		}
		const float V = StreetHalf + 1.45f;
		auto Car = [&](const TCHAR* Type, float U, float VV, bool bAlongU, float Jitter)
		{
			FLocal C;
			C.Type = Type;
			C.C = bAlongU ? FVector2f(U, VV) : FVector2f(VV, U);
			C.Rot = bAlongU ? 1 : 0;   // o comprimento do carro (Y da malha) ao longo da rua
			C.YawDeg = 90.0f * C.Rot + R.Range(-Jitter, Jitter) + (R.Float() < 0.5f ? 180.0f : 0.0f);
			const float Cell0 = FMath::FloorToFloat(VV / CellM) * CellM;   // a célula do carro e a de dentro da rua
			C.Foot = bAlongU ? Rect(true, U - 4.0f, U + 4.0f, Cell0 - CellM, Cell0 + CellM) : Rect(false, U - 4.0f, U + 4.0f, Cell0 - CellM, Cell0 + CellM);
			Out.Add(C);
		};
		if (Us.Num() >= 2)
		{
			const int32 I1 = int32(R.Next() % uint32(Us.Num()));
			int32 I2 = int32(R.Next() % uint32(Us.Num()));
			for (int32 T = 0; T < 8 && FMath::Abs(Us[I2] - Us[I1]) < 24.0f; ++T) I2 = int32(R.Next() % uint32(Us.Num()));
			Car(TEXT("civ_car_burnt"), Us[I1], V, true, 12.0f);
			if (FMath::Abs(Us[I2] - Us[I1]) >= 24.0f) Car(TEXT("civ_pickup"), Us[I2], V, true, 10.0f);
		}
		// a carcaça na transversal do meio (a maquete tem uma no cruzamento), no acostamento dela
		for (const FArm& A : Arms)
		{
			if (!(A.bPaved && A.Side > 0.0f)) continue;
			Car(TEXT("civ_car_burnt"), Band + 6.0f, A.U + A.Asphalt + 1.3f, false, 6.0f);
			break;
		}
		return Out;
	};
	// no centro: as que bloqueiam vêm em par (o giro de 180°)
	auto StreetPropsAt = [&](float VLen) -> TArray<FLocal>
	{
		TArray<FLocal> Out = StreetPropsFor(VLen);
		if (!bCenter) return Out;
		const int32 N = Out.Num();
		for (int32 I = 0; I < N; ++I)
		{
			FLocal T = Out[I];
			T.C = -T.C;
			const FRect F = T.Foot;
			T.Foot.Min = -F.Max;
			T.Foot.Max = -F.Min;
			T.Rot = (T.Rot + 2) % 4;
			if (T.YawDeg > -1e30f) T.YawDeg += 180.0f;
			Out.Add(T);
		}
		return Out;
	};

	// ---- o terreno: cada lote em cada cópia (lago, morro, base, depósito) ----------
	auto ToWorld = [](FVector2f L, FVector2f C, int32 Q) { return C + Spin(L, Q); };
	auto WorldRect = [&](const FRect& R, FVector2f C, int32 Q)
	{
		const FVector2f A = ToWorld(R.Min, C, Q), B = ToWorld(R.Max, C, Q);
		FRect W;
		W.Min = FVector2f(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
		W.Max = FVector2f(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
		return W;
	};
	// a nota de um lote numa cópia: falso se não pode; OutHs = as alturas a cada 2 m
	int32 Why = 0;   // o motivo do último lote recusado: 1 borda, 2 base/depósito, 3 água, 4 encosta
	auto LotGround = [&](const FRect& R, FVector2f C, int32 Q, TArray<float>& OutHs) -> bool
	{
		OutHs.Reset();
		int32 Steep = 0;
		for (float U = R.Min.X + 1.0f; U < R.Max.X; U += 2.0f)
		{
			for (float V = R.Min.Y + 1.0f; V < R.Max.Y; V += 2.0f)
			{
				const FVector2f P = ToWorld(FVector2f(U, V), C, Q);
				if (FMath::Abs(P.X) > Half - 24.0f || FMath::Abs(P.Y) > Half - 24.0f) { Why = 1; return false; }
				for (const FKeep& K : Keep) if (FVector2f::Distance(P, K.P) < K.R + 6.0f) { Why = 2; return false; }
				if (M.FormAt(P) > 0.06f) { Why = 2; return false; }   // (16/10) lote no flanco de um morro: o PadHeight ergueria um paredão
				const float H = M.HeightAt(P);
				// (13/10: 0,3 m acima da água e 6 m longe da água de verdade; com 1 m o chão baixo do lago
				// que recuou da vila, seco mas a menos de 1 m da água, matava os cantos do cruzamento)
				if (M.WaterLevel > -100.0f && H < M.WaterLevel + 0.3f) { Why = 3; return false; }
				Steep += M.NormalAt(P).Z < 0.85f ? 1 : 0;
				OutHs.Add(H);
			}
		}
		if (M.WaterLevel > -100.0f)
		{
			// a água de verdade a menos de 6 m da beira do lote
			const FVector2f Mid = R.Center();
			for (const FVector2f& O : { FVector2f(-1, -1), FVector2f(1, -1), FVector2f(-1, 1), FVector2f(1, 1), FVector2f(-1, 0), FVector2f(1, 0), FVector2f(0, -1), FVector2f(0, 1) })
			{
				const FVector2f L(O.X < 0.0f ? R.Min.X - 6.0f : (O.X > 0.0f ? R.Max.X + 6.0f : Mid.X), O.Y < 0.0f ? R.Min.Y - 6.0f : (O.Y > 0.0f ? R.Max.Y + 6.0f : Mid.Y));
				if (M.HeightAt(ToWorld(L, C, Q)) < M.WaterLevel) { Why = 3; return false; }
			}
		}
		Why = 4;
		return Steep * 3 <= OutHs.Num();
	};
	struct FCand
	{
		FVector2f C;
		int32 Q = 0;
		int32 Road = INDEX_NONE;
		float Len = 0.0f;               // o meio comprimento que cabe na reta deste lugar
		float Score = 0.0f;
		TArray<bool> Dead;
		TArray<uint8> Reason;           // 1 borda, 2 base, 3 água, 4 encosta, 5 degrau, 6 longe do platô, 7 caminho
		TArray<TArray<float>> BlockZ;   // [cópia][quarteirão]
	};
	// o chão de um candidato: os lotes que não servem em alguma cópia e o platô de cada quarteirão
	auto Survey = [&](FCand& Cd) -> int32
	{
		Cd.Dead.Init(false, Lots.Num());
		Cd.Reason.Init(0, Lots.Num());
		Cd.BlockZ.SetNum(Copies);
		TArray<TArray<float>> LotMed;
		LotMed.SetNum(Copies);
		for (int32 L = 0; L < Lots.Num(); ++L)
		{
			if (!Lots[L].bArm && FMath::Max(FMath::Abs(Lots[L].Lot.Min.X), FMath::Abs(Lots[L].Lot.Max.X)) > Cd.Len + 0.01f) { Cd.Dead[L] = true; Cd.Reason[L] = 8; }
		}
		for (int32 K = 0; K < Copies; ++K)
		{
			const FVector2f CK = Spin(Cd.C, K * Step);
			const int32 QK = Cd.Q + K * Step;
			LotMed[K].Init(0.0f, Lots.Num());
			TArray<float> Hs;
			for (int32 L = 0; L < Lots.Num(); ++L)
			{
				if (Cd.Reason[L] == 8) continue;
				if (!LotGround(Lots[L].Lot, CK, QK, Hs) || Hs.Num() == 0) { Cd.Dead[L] = true; Cd.Reason[L] = uint8(Why); continue; }
				Hs.Sort();
				LotMed[K][L] = Hs[Hs.Num() / 2];
				if (Hs.Last() - Hs[0] > 5.0f) { Cd.Dead[L] = true; Cd.Reason[L] = 5; }   // torto demais pra aplainar (vila em degraus até ~5 m)
			}
		}
		// o platô do quarteirão: a mediana dos lotes vivos; lote longe dele (> 1,5 m) sai
		int32 Alive = 0;
		for (int32 K = 0; K < Copies; ++K)
		{
			Cd.BlockZ[K].Init(0.0f, Blocks);
			for (int32 B = 0; B < Blocks; ++B)
			{
				TArray<float> Zs;
				for (int32 L = 0; L < Lots.Num(); ++L) if (Lots[L].Block == B && !Cd.Dead[L]) Zs.Add(LotMed[K][L]);
				if (Zs.Num() == 0) continue;
				Zs.Sort();
				Cd.BlockZ[K][B] = Zs[Zs.Num() / 2];
			}
		}
		for (int32 L = 0; L < Lots.Num(); ++L)
		{
			for (int32 K = 0; K < Copies && !Cd.Dead[L]; ++K)
			{
				if (FMath::Abs(LotMed[K][L] - Cd.BlockZ[K][Lots[L].Block]) > 2.5f) { Cd.Dead[L] = true; Cd.Reason[L] = 6; }
			}
		}
		for (int32 L = 0; L < Lots.Num(); ++L)
		{
			if (Twin[L] != INDEX_NONE && Cd.Dead[L] && !Cd.Dead[Twin[L]]) { Cd.Dead[Twin[L]] = true; Cd.Reason[Twin[L]] = 9; }
		}
		for (int32 L = 0; L < Lots.Num(); ++L) Alive += Cd.Dead[L] ? 0 : (Lots[L].Type ? 1 : 0);
		return Alive;
	};

	// ---- os lugares: ao longo da estrada de asfalto principal ----------------------
	TArray<FCand> Cands;
	int32 NPoints = 0, NStraight = 0, BestAlive = 0;
	FVector2f BestAt = FVector2f::ZeroVector;
	if (bCenter)
	{
		// o meio do mapa, a rua no eixo (X ou Y) da estrada de asfalto que passa mais perto
		int32 RN = INDEX_NONE, RI = INDEX_NONE;
		float RD = 80.0f;
		for (int32 O = 0; O < NumGenRoads; ++O)
		{
			if (!GRoads[O].bPaved) continue;
			int32 Idx;
			const float Dd = NearestOn(GRoads[O].P, FVector2f::ZeroVector, Idx);
			if (Dd < RD) { RD = Dd; RN = O; RI = Idx; }
		}
		if (RN != INDEX_NONE)
		{
			const TArray<FVector2f>& P = GRoads[RN].P;
			const FVector2f D = (P[FMath::Min(RI + 40, P.Num() - 1)] - P[FMath::Max(RI - 40, 0)]).GetSafeNormal();
			const int32 RoadAxis = FMath::Abs(D.X) >= FMath::Abs(D.Y) ? 0 : 1;
			for (const int32 Q : { RoadAxis, 1 - RoadAxis })
			{
				FCand Cd;
				Cd.C = FVector2f::ZeroVector;
				Cd.Q = Q;
				Cd.Road = RN;
				Cd.Len = Len;
				++NPoints;
				++NStraight;
				const int32 Alive = Survey(Cd);
				if (Alive > BestAlive) { BestAlive = Alive; BestAt = Cd.C; }
				if (Alive < 8) continue;
				Cd.Score = -2.0f * Alive + (Q == RoadAxis ? 0.0f : 6.0f);   // a rua no eixo da estrada, se der
				Cands.Add(MoveTemp(Cd));
			}
		}
	}
	for (int32 R = 0; R < NumGenRoads && !bCenter; ++R)
	{
		const FRoad& Road = GRoads[R];
		if (!Road.bPaved || Road.Half < MainHalf - 0.01f || Road.P.Num() < 60) continue;
		for (int32 I = 30; I + 30 < Road.P.Num(); I += 6)
		{
			++NPoints;
			const FVector2f D = (Road.P[I + 25] - Road.P[I - 25]).GetSafeNormal();
			const bool bAlongX = FMath::Abs(D.X) >= FMath::Abs(D.Y);
			// o centro: a rua no meio de uma célula, o cruzamento numa linha da grade
			const FVector2f P = Road.P[I];
			FCand Cd;
			Cd.Q = bAlongX ? 0 : 1;
			Cd.Road = R;
			if (bAlongX) Cd.C = FVector2f(FMath::RoundToFloat((P.X + Half) / CellM) * CellM - Half, FMath::FloorToFloat((P.Y + Half) / CellM) * CellM + 2.0f - Half);
			else Cd.C = FVector2f(FMath::FloorToFloat((P.X + Half) / CellM) * CellM + 2.0f - Half, FMath::RoundToFloat((P.Y + Half) / CellM) * CellM - Half);
			// as cópias não se encostam (a vila de cada lado tem ~150 m de diâmetro)
			if (Cd.C.Size() < 0.5f * 190.0f / FMath::Sin(PI / Copies)) continue;
			// a estrada não pode fugir mais de 14 m da reta dentro da vila em nenhuma
			// cópia (endireitar pouco), e tem de atravessar a vila inteira
			bool bOk = true;
			float Dev = 0.0f;
			Cd.Len = 0.0f;
			for (float TryLen = Len; TryLen >= 48.0f - 0.01f && Cd.Len <= 0.0f; TryLen -= 8.0f)
			{
			bOk = true;
			Dev = 0.0f;
			for (int32 K = 0; K < Copies && bOk; ++K)
			{
				const FVector2f CK = Spin(Cd.C, K * Step);
				const int32 QK = Cd.Q + K * Step;
				int32 RK = INDEX_NONE;
				float RD = 20.0f;
				for (int32 O = 0; O < NumGenRoads; ++O)
				{
					if (!GRoads[O].bPaved) continue;
					int32 Idx;
					const float Dd = NearestOn(GRoads[O].P, CK, Idx);
					if (Dd < RD) { RD = Dd; RK = O; }
				}
				if (RK == INDEX_NONE) { bOk = false; break; }
				bool bLo = false, bHi = false;
				for (const FVector2f& RP : GRoads[RK].P)
				{
					const FVector2f L = Spin(RP - CK, -QK);
					if (FMath::Abs(L.X) > TryLen + 8.0f) { bLo |= L.X < 0.0f && FMath::Abs(L.Y) < 50.0f; bHi |= L.X > 0.0f && FMath::Abs(L.Y) < 50.0f; continue; }
					Dev = FMath::Max(Dev, FMath::Abs(L.Y));
				}
				bOk = bLo && bHi && Dev <= 30.0f;
			}
			if (bOk) Cd.Len = TryLen;
			}
			if (Cd.Len <= 0.0f) continue;
			bOk = true;
			++NStraight;
			const int32 Alive = Survey(Cd);
			if (Alive > BestAlive) { BestAlive = Alive; BestAt = Cd.C; }
			if (Alive < 10) continue;
			float NearBase = 1e9f;
			for (const FKeep& K : Keep) if (K.bBase) NearBase = FMath::Min(NearBase, FVector2f::Distance(K.P, Cd.C));
			// mais casas, reta já reta, a ~250 m da própria base (no caminho do meio)
			Cd.Score = -2.0f * Alive + 0.15f * Dev + FMath::Abs(NearBase - 250.0f) / 40.0f
				+ 0.01f * float(WorldNoise::Hash(uint32(I) * 7919u + Seed + uint32(R)) % 100u) / 100.0f;   // a semente só desempata
			Cands.Add(MoveTemp(Cd));
		}
	}
	if (Cands.Num() == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("FORGE vila: nenhum lugar na estrada de asfalto com 10+ lotes (plano, seco, longe das bases e dos depósitos): %d pontos, %d retos, no melhor %d casas em (%.0f, %.0f)"),
			NPoints, NStraight, BestAlive, BestAt.X, BestAt.Y);
		return;
	}
	Cands.Sort([](const FCand& A, const FCand& B) { return A.Score < B.Score; });

	// ---- a conferência dos caminhos na grade de 4 m --------------------------------
	auto CellOf = [&](FVector2f P) { return FIntPoint(FMath::FloorToInt((P.X + Half) / CellM), FMath::FloorToInt((P.Y + Half) / CellM)); };
	// o terreno: a célula com a maioria dos vértices de 2 m em penhasco ou água
	TArray<uint8> Ground;
	Ground.Init(0, G4 * G4);
	if (M.Pass.Num() == M.N * M.N)
	{
		for (int32 J = 0; J < G4; ++J)
		{
			for (int32 I = 0; I < G4; ++I)
			{
				int32 Bad = 0;
				for (int32 B = 0; B < 3; ++B)
					for (int32 A = 0; A < 3; ++A)
					{
						const FVector2f Gr = M.ToGrid(FVector2f(I * CellM - Half + A * 2.0f, J * CellM - Half + B * 2.0f));
						const int32 Vi = FMath::Clamp(FMath::RoundToInt(Gr.X), 0, M.N - 1), Vj = FMath::Clamp(FMath::RoundToInt(Gr.Y), 0, M.N - 1);
						Bad += M.Pass[M.Index(Vi, Vj)] != EWorldPass::Open ? 1 : 0;
					}
				Ground[J * G4 + I] = Bad >= 5 ? 1 : 0;
			}
		}
	}
	auto Stamp = [&](TArray<uint8>& B, const FRect& W)
	{
		const FIntPoint C0 = CellOf(W.Min + FVector2f(0.5f)), C1 = CellOf(W.Max - FVector2f(0.5f));
		for (int32 J = FMath::Max(C0.Y, 0); J <= FMath::Min(C1.Y, G4 - 1); ++J)
			for (int32 I = FMath::Max(C0.X, 0); I <= FMath::Min(C1.X, G4 - 1); ++I) B[J * G4 + I] = 1;
	};
	// a grade com a vila: o lote inteiro (casa e quintal murado) e as miudezas da rua
	auto Blocked = [&](const FCand& Cd, bool bStreetToo) -> TArray<uint8>
	{
		TArray<uint8> B = Ground;
		for (int32 K = 0; K < Copies; ++K)
		{
			const FVector2f CK = Spin(Cd.C, K * Step);
			const int32 QK = Cd.Q + K * Step;
			for (int32 L = 0; L < Lots.Num(); ++L) if (!Cd.Dead[L]) Stamp(B, WorldRect(Lots[L].Lot, CK, QK));
			for (const FLocal& P : StreetPropsAt(Cd.Len)) Stamp(B, WorldRect(P.Foot, CK, QK));
			if (bStreetToo)
			{
				// o desvio: a rua e a transversal fechadas, tem de haver outro caminho
				Stamp(B, WorldRect(Rect(true, -Cd.Len - 4.0f, Cd.Len + 4.0f, -Face, Face), CK, QK));
				for (const FArm& A : Arms)
				{
					Stamp(B, WorldRect(Rect(false, A.Side > 0.0f ? Face : -ArmEnd, A.Side > 0.0f ? ArmEnd : -Face, A.U - A.Half, A.U + A.Half), CK, QK));
				}
			}
		}
		return B;
	};
	auto Clear = [&](const TArray<uint8>& B, int32 I, int32 J)
	{
		for (int32 Dj = -1; Dj <= 1; ++Dj)
			for (int32 Di = -1; Di <= 1; ++Di)
			{
				const int32 X = I + Di, Y = J + Dj;
				if (X < 0 || Y < 0 || X >= G4 || Y >= G4 || B[Y * G4 + X]) return false;
			}
		return true;
	};
	auto Flood = [&](const TArray<uint8>& B, FVector2f From) -> TArray<float>
	{
		TArray<float> D;
		D.Init(1e9f, G4 * G4);
		const FIntPoint S = CellOf(From);
		float BestD = 1e9f;
		FIntPoint Start(-1, -1);
		for (int32 Dj = -8; Dj <= 8; ++Dj)
			for (int32 Di = -8; Di <= 8; ++Di)
			{
				const int32 X = S.X + Di, Y = S.Y + Dj;
				if (X < 1 || Y < 1 || X >= G4 - 1 || Y >= G4 - 1 || !Clear(B, X, Y)) continue;
				const float Dd = float(Di * Di + Dj * Dj);
				if (Dd < BestD) { BestD = Dd; Start = FIntPoint(X, Y); }
			}
		if (Start.X < 0) return D;
		TArray<TPair<float, int32>> Heap;
		auto Less = [](const TPair<float, int32>& A, const TPair<float, int32>& Bb) { return A.Key < Bb.Key; };
		D[Start.Y * G4 + Start.X] = 0.0f;
		Heap.HeapPush(TPair<float, int32>(0.0f, Start.Y * G4 + Start.X), Less);
		while (Heap.Num())
		{
			TPair<float, int32> Top;
			Heap.HeapPop(Top, Less, EAllowShrinking::No);
			if (Top.Key > D[Top.Value]) continue;
			const int32 X0 = Top.Value % G4, Y0 = Top.Value / G4;
			for (int32 Dj = -1; Dj <= 1; ++Dj)
				for (int32 Di = -1; Di <= 1; ++Di)
				{
					if (!Di && !Dj) continue;
					const int32 X = X0 + Di, Y = Y0 + Dj;
					if (X < 1 || Y < 1 || X >= G4 - 1 || Y >= G4 - 1 || !Clear(B, X, Y)) continue;
					const float Nd = Top.Key + ((Di && Dj) ? 1.4142f : 1.0f);
					if (Nd < D[Y * G4 + X]) { D[Y * G4 + X] = Nd; Heap.HeapPush(TPair<float, int32>(Nd, Y * G4 + X), Less); }
				}
		}
		return D;
	};
	TArray<FVector2f> Bases, Depots;
	for (const FKeep& K : Keep) (K.bBase ? Bases : Depots).Add(K.P);
	auto Reach = [&](const TArray<float>& D, FVector2f P)
	{
		const FIntPoint C = CellOf(P);
		float BestD = 1e9f;
		for (int32 Dj = -8; Dj <= 8; ++Dj)
			for (int32 Di = -8; Di <= 8; ++Di)
			{
				const int32 X = C.X + Di, Y = C.Y + Dj;
				if (X >= 0 && Y >= 0 && X < G4 && Y < G4) BestD = FMath::Min(BestD, D[Y * G4 + X]);
			}
		return BestD;
	};
	// os comprimentos que importam: base com base e cada depósito com a base mais perto
	auto Lengths = [&](const TArray<uint8>& B, bool bBasesOnly) -> TArray<float>
	{
		TArray<float> Out;
		for (int32 A = 0; A < Bases.Num(); ++A)
		{
			const TArray<float> D = Flood(B, Bases[A]);
			for (int32 Bb = A + 1; Bb < Bases.Num(); ++Bb) Out.Add(Reach(D, Bases[Bb]));
			if (bBasesOnly) continue;
			for (const FVector2f& Dp : Depots)
			{
				int32 Near = 0;
				for (int32 K = 1; K < Bases.Num(); ++K) if (FVector2f::Distance(Bases[K], Dp) < FVector2f::Distance(Bases[Near], Dp)) Near = K;
				if (Near == A) Out.Add(Reach(D, Dp));
			}
		}
		return Out;
	};
	const TArray<float> Before = Lengths(Ground, false);
	const TArray<float> BeforeBases = Lengths(Ground, true);   // só base com base (o desvio)
	auto Fails = [&](const TArray<float>& After, float Slack)
	{
		const TArray<float>& Ref = After.Num() == BeforeBases.Num() ? BeforeBases : Before;
		for (int32 K = 0; K < After.Num() && K < Ref.Num(); ++K)
		{
			if (Ref[K] > 1e8f) continue;   // já não se alcançava (o terreno)
			if (After[K] > 1e8f || After[K] > Ref[K] * Slack + 3.0f) return true;
		}
		return false;
	};
	// o primeiro candidato (na ordem da nota) que passa: o caminho cresce ≤ 10% e,
	// com a rua fechada, ainda há um desvio (≤ 60% mais longo); senão tira o lote
	// mais longe da rua (o que mais fecha o lado) e confere de novo
	const FCand* Chosen = nullptr;
	FCand Work;
	int32 Removed = 0;
	bool bDetour = true;
	for (int32 CI = 0; CI < FMath::Min(Cands.Num(), 10) && !Chosen; ++CI)
	{
		Work = Cands[CI];
		Removed = 0;
		while (true)
		{
			const bool bPath = !Fails(Lengths(Blocked(Work, false), false), 1.10f);
			const bool bAround = bPath && !Fails(Lengths(Blocked(Work, true), true), 1.60f);
			if (bPath && bAround) { Chosen = &Work; break; }
			int32 Victim = INDEX_NONE;
			float Far = -1.0f;
			for (int32 L = 0; L < Lots.Num(); ++L)
			{
				if (Work.Dead[L]) continue;
				const FRect& R = Lots[L].Lot;
				const float V = (Lots[L].bArm ? 2000.0f : 0.0f) + FMath::Max(FMath::Abs(R.Min.Y), FMath::Abs(R.Max.Y)) * 10.0f + FMath::Abs(R.Center().X);
				if (V > Far) { Far = V; Victim = L; }
			}
			int32 Alive = 0;
			for (int32 L = 0; L < Lots.Num(); ++L) Alive += !Work.Dead[L] && Lots[L].Type ? 1 : 0;
			if (Victim == INDEX_NONE || Alive <= (bCenter ? 8 : 10)) break;
			Work.Dead[Victim] = true;
			Work.Reason[Victim] = 7;
			if (Twin[Victim] != INDEX_NONE) { Work.Dead[Twin[Victim]] = true; Work.Reason[Twin[Victim]] = 7; }
			++Removed;
		}
	}
	if (!Chosen)
	{
		// nenhum lugar com desvio: a vila de menos lotes no melhor lugar que só não fecha o caminho
		bDetour = false;
		Work = Cands[0];
		Removed = 0;
		while (Fails(Lengths(Blocked(Work, false), false), 1.10f))
		{
			int32 Victim = INDEX_NONE;
			float Far = -1.0f;
			for (int32 L = 0; L < Lots.Num(); ++L)
			{
				if (Work.Dead[L]) continue;
				const float V = FMath::Max(FMath::Abs(Lots[L].Lot.Min.Y), FMath::Abs(Lots[L].Lot.Max.Y)) * 10.0f + FMath::Abs(Lots[L].Lot.Center().X);
				if (V > Far) { Far = V; Victim = L; }
			}
			if (Victim == INDEX_NONE) break;
			Work.Dead[Victim] = true;
			Work.Reason[Victim] = 7;
			if (Twin[Victim] != INDEX_NONE) { Work.Dead[Twin[Victim]] = true; Work.Reason[Twin[Victim]] = 7; }
			++Removed;
		}
		Chosen = &Work;
	}
	const FCand& Cd = *Chosen;
	// o quanto a vila estica o caminho (pior par) e o desvio com a rua fechada
	float WorstPath = 0.0f, WorstDetour = 0.0f;
	{
		const TArray<float> A = Lengths(Blocked(Cd, false), false), D = Lengths(Blocked(Cd, true), true);
		for (int32 K = 0; K < A.Num(); ++K) if (Before[K] < 1e8f) WorstPath = FMath::Max(WorstPath, A[K] / FMath::Max(Before[K], 1.0f));
		for (int32 K = 0; K < D.Num(); ++K) if (BeforeBases[K] < 1e8f) WorstDetour = FMath::Max(WorstDetour, D[K] > 1e8f ? 99.0f : D[K] / FMath::Max(BeforeBases[K], 1.0f));
	}
	GVillageLots.Reset();
	for (int32 L = 0; L < Lots.Num(); ++L)
	{
		const FRect W = WorldRect(Lots[L].Lot, Cd.C, Cd.Q);
		GVillageLots.Add(FVector4f(W.Min.X, W.Min.Y, W.Max.X, W.Max.Y));
		GVillageLotWhy.Add(Cd.Dead[L] ? Cd.Reason[L] : 0);
	}

	// ---- a estrada reta pela vila e a transversal (por cópia) ----------------------
	// a transversal vai até o último lote vivo de cada lado (+10 m) e vira terra;
	// sem lote vivo do lado, não tem braço (vira um T)
	TArray<float> ArmReach;   // por transversal: até onde vai (|v|), 0 = não tem
	ArmReach.Init(0.0f, Arms.Num());
	for (int32 L = 0; L < Lots.Num(); ++L)
	{
		if (Cd.Dead[L] || Lots[L].Arm < 0) continue;
		const FRect& R = Lots[L].Lot;
		ArmReach[Lots[L].Arm] = FMath::Max(ArmReach[Lots[L].Arm], FMath::Max(FMath::Abs(R.Min.Y), FMath::Abs(R.Max.Y)));
	}
	for (int32 K = 0; K < Copies; ++K)
	{
		const int32 QK = Cd.Q + K * Step;
		const FVector2f CK = Spin(Cd.C, K * Step);
		int32 R = INDEX_NONE;
		float RD = 20.0f;
		for (int32 O = 0; O < NumGenRoads; ++O)
		{
			if (!GRoads[O].bPaved) continue;
			int32 Idx;
			const float Dd = NearestOn(GRoads[O].P, CK, Idx);
			if (Dd < RD) { RD = Dd; R = O; }
		}
		if (R == INDEX_NONE) continue;
		{
			FRoad& Road = GRoads[R];
			for (FVector2f& P : Road.P)
			{
				const FVector2f L = Spin(P - CK, -QK);
				const float W = 1.0f - FMath::SmoothStep(Cd.Len + 8.0f, Cd.Len + 48.0f, FMath::Abs(L.X));
				if (W <= 0.0f) continue;
				P = CK + Spin(FVector2f(L.X, L.Y * (1.0f - W)), QK);
			}
			Road.P = Resample(Road.P, 1.0f);
			// a rua da vila: o asfalto vai de MainHalf (10 m) a StreetHalf (13,5 m) com 24 m de
			// transição nas entradas (o revisor: sem o alargão de repente)
			if (Road.W.Num() != Road.P.Num()) Road.W.Init(Road.Half, Road.P.Num());
			for (int32 I = 0; I < Road.P.Num(); ++I)
			{
				const FVector2f L = Spin(Road.P[I] - CK, -QK);
				if (FMath::Abs(L.Y) > 20.0f) continue;
				const float In = 1.0f - FMath::SmoothStep(Cd.Len + 4.0f, Cd.Len + 28.0f, FMath::Abs(L.X));
				Road.W[I] = FMath::Max(Road.W[I], FMath::Lerp(Road.Half, StreetHalf, In));
			}
		}
		// quem entroncava nela encosta de novo (a estrada andou até 14 m)
		for (int32 O = 0; O < GRoads.Num(); ++O)
		{
			if (O == R) continue;
			for (int32 Side = 0; Side < 2; ++Side)
			{
				if (GRoads[O].Host[Side] != R) continue;
				const FVector2f E = Side == 0 ? GRoads[O].P[0] : GRoads[O].P.Last();
				int32 Idx;
				if (NearestOn(GRoads[R].P, E, Idx) < 1.5f) continue;
				const TArray<FVector2f> Bridge = Resample({ E, GRoads[R].P[Idx] }, 1.0f);
				for (int32 B = 1; B < Bridge.Num(); ++B)
				{
					if (Side == 0) GRoads[O].P.Insert(Bridge[B], 0); else GRoads[O].P.Add(Bridge[B]);
				}
			}
		}
		for (int32 S = 0; S < Arms.Num(); ++S)
		{
			if (ArmReach[S] <= 0.0f) continue;
			const float Sgn = Arms[S].Side;
			FRoad& Arm = GRoads.AddDefaulted_GetRef();
			Arm.P = Resample({ CK + Spin(FVector2f(Arms[S].U, 0.0f), QK), CK + Spin(FVector2f(Arms[S].U, Sgn * (ArmReach[S] + 10.0f)), QK) }, 1.0f);
			Arm.Half = Arms[S].Asphalt;
			Arm.bPaved = Arms[S].bPaved;
			Arm.Host[0] = R;
			Arm.bOpen[1] = true;
			Arm.Fade[1] = 24.0f;   // o asfalto se desfaz em terra no fim (a rua de cima da maquete)
		}
	}

	// ---- as peças (planta local): casas, muros sem repetição, palmeiras, miudezas ----
	TArray<FLocal> Pieces;
	TArray<FVector2f> Palms;   // local
	TSet<FIntVector> WallKeys;
	// um trecho de muro na linha do lote: Along = a linha corre em u; Line = a cota
	// dela; A = o meio do trecho; Out = o lado de fora (giro da frente); Inside = de
	// que lado fica o quintal (pegada é a célula de dentro)
	auto Wall = [&](bool bAlongU, float Line, float A, int32 Out, float InsideSign, int32 Lot, bool bGate)
	{
		const FIntVector Key(bAlongU ? 1 : 0, FMath::RoundToInt(Line * 10.0f), FMath::RoundToInt(A * 10.0f));
		if (WallKeys.Contains(Key)) return;
		WallKeys.Add(Key);
		FLocal W;
		W.Type = bGate ? TEXT("civ_wall_gate") : TEXT("civ_wall");
		W.C = bAlongU ? FVector2f(A, Line) : FVector2f(Line, A);
		W.Rot = Out;
		const float I0 = Line, I1 = Line + InsideSign * CellM;
		W.Foot = bAlongU ? Rect(true, A - 2.0f, A + 2.0f, I0, I1) : Rect(false, A - 2.0f, A + 2.0f, I0, I1);
		W.Lot = Lot;
		Pieces.Add(W);
	};
	// os muros de um quintal (todos os lados menos o da casa); o portão no fundo
	// ou, na esquina, no lado da transversal
	auto YardWalls = [&](const FRect& Y, int32 Lot, int32 HouseSide, int32 GateSide)
	{
		// lados como o giro da frente que olha pra fora: 0 = +v, 1 = −u, 2 = −v, 3 = +u
		for (int32 Side = 0; Side < 4; ++Side)
		{
			if (Side == HouseSide) continue;
			const bool bAlongU = Side == 0 || Side == 2;
			const float A0 = bAlongU ? Y.Min.X : Y.Min.Y, A1 = bAlongU ? Y.Max.X : Y.Max.Y;
			const float Line = Side == 0 ? Y.Max.Y : (Side == 2 ? Y.Min.Y : (Side == 1 ? Y.Min.X : Y.Max.X));
			const float Inside = (Side == 0 || Side == 3) ? -1.0f : 1.0f;
			const int32 N = FMath::RoundToInt((A1 - A0) / CellM);
			for (int32 K = 0; K < N; ++K) Wall(bAlongU, Line, A0 + (K + 0.5f) * CellM, Side, Inside, Lot, Side == GateSide && K == N / 2);
		}
	};
	auto Opposite = [](int32 Rot) { return (Rot + 2) % 4; };
	// o resto de muro da casa solta (o lado, quantas e de que canto vêm do lote: FLot::LooseSide)
	auto LooseWall = [&](const FRect& Y, int32 Lot, int32 Side, int32 Count, bool bFromLo)
	{
		const bool bAlongU = Side == 0 || Side == 2;
		const float A0 = bAlongU ? Y.Min.X : Y.Min.Y, A1 = bAlongU ? Y.Max.X : Y.Max.Y;
		const float Line = Side == 0 ? Y.Max.Y : (Side == 2 ? Y.Min.Y : (Side == 1 ? Y.Min.X : Y.Max.X));
		const float Inside = (Side == 0 || Side == 3) ? -1.0f : 1.0f;
		const int32 N = FMath::RoundToInt((A1 - A0) / CellM);
		for (int32 K = 0; K < FMath::Min(N, Count); ++K) Wall(bAlongU, Line, A0 + ((bFromLo ? K : N - 1 - K) + 0.5f) * CellM, Side, Inside, Lot, false);
	};
	for (int32 L = 0; L < Lots.Num(); ++L)
	{
		if (Cd.Dead[L]) continue;
		const FLot& Lt = Lots[L];
		const int32 Back = Opposite(Lt.Front);   // o lado de trás do lote (longe da rua)
		if (Lt.Type)
		{
			FLocal H;
			H.Type = Lt.Type;
			H.C = Lt.House.Center();
			H.Rot = Lt.Front;
			H.Foot = Lt.House;
			H.Lot = L;
			H.Street = Lt.Front;
			Pieces.Add(H);
			if (Lt.Style == 0)
			{
				// o quintal murado: o portão no fundo ou, na esquina, no lado da transversal
				if (!Lt.Yard.IsEmpty()) YardWalls(Lt.Yard, L, Lt.Front, Lt.CrossSide >= 0 ? Lt.CrossSide : Back);
				// o pátio da frente (recuo): muro na fachada com portão, a casa fecha o fundo
				if (!Lt.Fore.IsEmpty()) YardWalls(Lt.Fore, L, Back, Lt.Front);
			}
			else if (Lt.LooseSide >= 0 && !Lt.Yard.IsEmpty())
			{
				LooseWall(Lt.Yard, L, Lt.LooseSide, Lt.LooseCount, Lt.bLooseLo);
			}
			if (!Lt.BackHouse.IsEmpty())
			{
				FLocal Bh;
				Bh.Type = TEXT("civ_house_1story");
				Bh.C = Lt.BackHouse.Center();
				Bh.Rot = Lt.BackRot;
				Bh.Foot = Lt.BackHouse;
				Bh.Lot = L;
				Pieces.Add(Bh);
			}
		}
		else if (Lt.Style == 2)
		{
			YardWalls(Lt.Yard, L, -1, Lt.Front);   // o pomar murado, o portão pra rua
		}
		// as palmeiras: no quintal murado (35%), no pomar (1), na solta sem a casa dos fundos (15%)
		// e no lote aberto (30%); a maquete tem 10 na vila, várias soltas na areia
		if (!Lt.Yard.IsEmpty() && Lt.BackHouse.IsEmpty() && (Lt.Yard.Max - Lt.Yard.Min).GetMin() >= 2.0f * CellM - 0.01f)
		{
			const float Pp = Lt.Style == 0 ? 0.35f : (Lt.Style == 1 ? 0.15f : (Lt.Style == 2 ? 1.0f : 0.3f));
			const int32 N = Pp >= 1.0f ? int32(Pp) : (Rng.Float() < Pp ? 1 : 0);
			for (int32 K = 0; K < N; ++K)
			{
				Palms.Add(Lt.Yard.Center() + FVector2f(Rng.Range(-1.5f, 1.5f), Rng.Range(-1.5f, 1.5f)));
			}
		}
	}
	Pieces.Append(StreetPropsAt(Cd.Len));

	// ---- as miudezas em grupo (não bloqueiam; sorteio próprio, não espelhado: o revisor) ----------
	// A maquete do Look: ~200 peças no chão em grupos (1 grande + 1–3 pequenas a 1–2,5 m) no pé do
	// muro, do lado da casa e no acostamento; a pista limpa e a porta livre. Por ora o kit que o jogo
	// já desenha (caixotes, tambores, munição); a sucata, o entulho, a cerca, os pneus e a laje
	// (caixa d'água, antena, ar-condicionado) entram quando a malha for pro kit
	int32 Clusters = 0, Loose = 0;
	{
		WorldNoise::FRng Dr(Seed ^ 0x9e3779b9u);
		TArray<FRect> Solid;      // as pegadas que bloqueiam (casa, muro, carro)
		TArray<FVector2f> Doors;  // a frente da porta e do portão
		for (const FLocal& P : Pieces)
		{
			const FKit& K = KitOf(P.Type);
			if (!K.bBlocks) continue;
			const FVector2f Hs = 0.5f * ((P.Rot % 2 == 0) ? FVector2f(K.W, K.D) : FVector2f(K.D, K.W));
			FRect R;
			R.Min = P.C - Hs;
			R.Max = P.C + Hs;
			Solid.Add(R);
			if (FCString::Strncmp(P.Type, TEXT("civ_house"), 9) == 0 || FCString::Strcmp(P.Type, TEXT("civ_wall_gate")) == 0)
			{
				Doors.Add(P.C + FrontOf(P.Rot) * ((P.Rot % 2 == 0 ? Hs.Y : Hs.X) + 1.0f));
			}
		}
		TArray<FVector2f> Placed;
		auto Free = [&](FVector2f Q, float Rr) -> bool
		{
			// fora da pista (a principal e as transversais de asfalto, com a beira quebrada de ±0,7 m)
			if (FMath::Abs(Q.Y) < StreetHalf + 0.9f + Rr && FMath::Abs(Q.X) < Cd.Len + 30.0f) return false;
			for (const FArm& Ar : Arms)
			{
				if (FMath::Abs(Q.X - Ar.U) < Ar.Asphalt + 0.9f + Rr && Q.Y * Ar.Side > 0.0f) return false;
			}
			for (const FRect& R : Solid)
			{
				if (Q.X > R.Min.X - Rr - 0.3f && Q.X < R.Max.X + Rr + 0.3f && Q.Y > R.Min.Y - Rr - 0.3f && Q.Y < R.Max.Y + Rr + 0.3f) return false;
			}
			for (const FVector2f& D : Doors) if (FVector2f::Distance(Q, D) < 2.6f + Rr) return false;
			for (const FVector2f& Pp : Placed) if (FVector2f::Distance(Q, Pp) < 1.1f + Rr) return false;
			for (const FVector2f& Pm : Palms) if (FVector2f::Distance(Q, Pm) < 1.2f + Rr) return false;
			return true;
		};
		auto Add = [&](const TCHAR* Type, FVector2f Q, float Yaw, int32 Lot)
		{
			FLocal D;
			D.Type = Type;
			D.C = Q;
			D.YawDeg = Yaw;
			D.Rot = ((FMath::RoundToInt(Yaw / 90.0f) % 4) + 4) % 4;
			D.Foot = Rect(true, Q.X - 1.0f, Q.X + 1.0f, Q.Y - 1.0f, Q.Y + 1.0f);
			D.Lot = Lot;
			D.bLoose = true;
			Pieces.Add(D);
			Placed.Add(Q);
			++Loose;
		};
		// os grupos da maquete: 1 grande (entulho, sucata, caixotes, tambores) + 1–3 pequenas (pneus,
		// munição, tambores, caixotes) a 1,3–2,6 m. O raio de cada uma pela pegada do kit
		auto RadiusOf = [](const TCHAR* Type) { const FKit& K = KitOf(Type); return 0.5f * FMath::Max(K.W, K.D); };
		auto PickBig = [&]() -> const TCHAR*
		{
			const float U = Dr.Float();
			if (U < 0.12f) return TEXT("LL_Prop_Rubble0");
			if (U < 0.22f) return TEXT("LL_Prop_Rubble1");
			if (U < 0.32f) return TEXT("LL_Prop_Rubble2");
			if (U < 0.47f) return TEXT("LL_Prop_Scrap0");
			if (U < 0.62f) return TEXT("LL_Prop_Scrap1");
			if (U < 0.82f) return TEXT("LL_Prop_Crates");
			return TEXT("LL_Prop_Barrels");
		};
		static const TCHAR* Small[] = { TEXT("LL_Prop_Tires"), TEXT("LL_Prop_Ammo"), TEXT("LL_Prop_Barrels"), TEXT("LL_Prop_Crates") };
		auto Cluster = [&](FVector2f At0, float Yaw0, int32 Lot)
		{
			// a grande sorteada; se não cabe (o vão entre a casa e a pista é estreito), em volta do
			// lugar e depois as menores
			const TCHAR* Order[4] = { PickBig(), TEXT("LL_Prop_Scrap0"), TEXT("LL_Prop_Crates"), TEXT("LL_Prop_Barrels") };
			const TCHAR* Big = nullptr;
			FVector2f At = At0;
			float Rb = 0.0f;
			for (int32 O = 0; O < 4 && !Big; ++O)
			{
				for (int32 T = 0; T < 5 && !Big; ++T)
				{
					const FVector2f Q = At0 + (T ? FVector2f(Dr.Range(-1.8f, 1.8f), Dr.Range(-1.8f, 1.8f)) : FVector2f::ZeroVector);
					const float R = FMath::Min(RadiusOf(Order[O]), 2.2f);
					if (Free(Q, R)) { Big = Order[O]; At = Q; Rb = R; }
				}
			}
			if (!Big) return;
			Add(Big, At, Yaw0 + Dr.Range(-12.0f, 12.0f), Lot);
			const int32 N = 1 + int32(Dr.Next() % 3u);
			for (int32 K = 0; K < N; ++K)
			{
				const TCHAR* Sm = Small[Dr.Next() % 4u];
				const float Rs = RadiusOf(Sm);
				for (int32 T = 0; T < 6; ++T)
				{
					const float Ang = Dr.Range(0.0f, 2.0f * PI);
					const FVector2f Q = At + Dr.Range(Rb + Rs + 0.2f, Rb + Rs + 1.2f) * FVector2f(FMath::Cos(Ang), FMath::Sin(Ang));
					if (!Free(Q, Rs)) continue;
					Add(Sm, Q, Dr.Range(0.0f, 360.0f), Lot);
					break;
				}
			}
			++Clusters;
		};
		// a peça comprida (cerca, muro quebrado) deitada numa linha, girada ao longo dela
		auto Long = [&](const TCHAR* Type, FVector2f At, float LineYaw, int32 Lot)
		{
			if (!Free(At, 0.6f)) return;
			Add(Type, At, LineYaw + Dr.Range(-4.0f, 4.0f) + (Dr.Float() < 0.5f ? 180.0f : 0.0f), Lot);
		};
		const int32 NPieces = Pieces.Num();
		for (int32 I = 0; I < NPieces; ++I)
		{
			const FLocal P = Pieces[I];   // (cópia: o Add mexe no array)
			if (P.bLoose) continue;
			if (FCString::Strcmp(P.Type, TEXT("civ_wall")) == 0 && Dr.Float() < 0.3f)
			{
				// no pé do muro, do lado de fora (o Rot do muro olha pra fora)
				const FVector2f Out = FrontOf(P.Rot), Along(Out.Y, -Out.X);
				Cluster(P.C + Out * Dr.Range(1.2f, 2.0f) + Along * Dr.Range(-1.5f, 1.5f), 90.0f * P.Rot, P.Lot);
			}
			else if (FCString::Strncmp(P.Type, TEXT("civ_house"), 9) == 0 && Dr.Float() < 0.6f)
			{
				// rente à parede de lado da casa
				const FKit& K = KitOf(P.Type);
				const FVector2f Fr = FrontOf(P.Rot), Side(Fr.Y, -Fr.X);
				const float Sg = Dr.Float() < 0.5f ? 1.0f : -1.0f;
				Cluster(P.C + Side * Sg * (0.5f * K.W + 1.6f) + Fr * Dr.Range(-0.3f * K.D, 0.3f * K.D), 90.0f * P.Rot, P.Lot);
			}
		}
		// no acostamento da principal, onde tem lote vivo do lado
		for (float U = -Cd.Len + 6.0f; U < Cd.Len - 6.0f; U += 7.0f)
		{
			for (const float Sg : { -1.0f, 1.0f })
			{
				if (Dr.Float() > 0.25f) continue;
				bool bNear = false;
				for (int32 L = 0; L < Lots.Num() && !bNear; ++L)
				{
					const FRect& R = Lots[L].Lot;
					bNear = !Cd.Dead[L] && !Lots[L].bArm && U > R.Min.X - 2.0f && U < R.Max.X + 2.0f && R.Center().Y * Sg > 0.0f;
				}
				if (bNear) Cluster(FVector2f(U, Sg * (StreetHalf + Dr.Range(1.7f, 2.6f))), Dr.Range(-20.0f, 20.0f), -1);
			}
		}
		// a laje (a maquete: 1–7 por casa, ~4; caixa d'água, ar-condicionado, varal, antena, caixote) e o
		// toldo na fachada (~30%). A laje do kit civil fica a h − 0,55 m (look_houses.py, PARAPET); a caixa
		// da escada num canto do fundo (o lado de Y dela no kit não é certo daqui: os dois cantos ficam
		// livres), a caixa d'água do sobrado no canto (−, −)
		for (int32 I = 0; I < NPieces; ++I)
		{
			const FLocal Hs = Pieces[I];
			if (Hs.bLoose || FCString::Strncmp(Hs.Type, TEXT("civ_house"), 9) != 0) continue;
			const bool b2 = FCString::Strcmp(Hs.Type, TEXT("civ_house_2story")) == 0;
			const bool bLong = FCString::Strcmp(Hs.Type, TEXT("civ_house_long")) == 0;
			const bool bPorch = FCString::Strcmp(Hs.Type, TEXT("civ_house_porch")) == 0;
			const float Sx = bLong ? 15.2f : 7.2f, Sy = bPorch ? 8.6f : 7.2f;
			const FVector2f Off(0.0f, bPorch ? -1.5f : 0.0f);   // o corpo da varanda vai pro fundo
			const float Slab = (b2 ? 7.5f : 4.5f) - 0.55f;
			TArray<FVector2f> RoofKeep;   // os cantos ocupados (local da malha) e o raio
			TArray<float> RoofKeepR;
			if (!b2 && !bPorch) { const float Bx = bLong ? -5.92f : 1.92f; RoofKeep.Append({ FVector2f(Bx, 2.12f), FVector2f(Bx, -2.12f) }); RoofKeepR.Append({ 1.6f, 1.6f }); }
			// (a casa que pode virar sobrado pela câmera também guarda o canto da caixa d'água)
			if (b2 || (Hs.Lot >= 0 && Lots[Hs.Lot].bTall)) { RoofKeep.Append({ FVector2f(-2.32f, -2.32f), FVector2f(-2.32f, 2.32f) }); RoofKeepR.Append({ 0.9f, 0.9f }); }
			const float HouseYaw = 90.0f * Hs.Rot;
			const FRotator HR(0.0f, HouseYaw, 0.0f);
			auto ToPlan = [&](FVector2f L) { const FVector W = HR.RotateVector(FVector(L.X, L.Y, 0.0)); return Hs.C + FVector2f(float(W.X), float(W.Y)); };
			const int32 N = (bLong ? 3 : 2) + int32(Dr.Next() % (bLong ? 5u : 4u));
			TArray<FVector2f> Placed2;
			TArray<float> PlacedR;
			for (int32 K = 0; K < N; ++K)
			{
				const float U = Dr.Float();
				const TCHAR* Type = U < 0.28f ? TEXT("LL_Prop_WaterTank") : (U < 0.56f ? TEXT("LL_Prop_AC") : (U < 0.75f ? TEXT("LL_Prop_Laundry") : (U < 0.92f ? TEXT("LL_Prop_Dish") : TEXT("LL_Prop_Crates"))));
				const FKit& Kk = KitOf(Type);
				const float R = 0.5f * FMath::Max(Kk.W, Kk.D);
				for (int32 T = 0; T < 8; ++T)
				{
					const FVector2f L = Off + FVector2f(Dr.Range(-1.0f, 1.0f) * FMath::Max(0.0f, 0.5f * Sx - 0.9f - R), Dr.Range(-1.0f, 1.0f) * FMath::Max(0.0f, 0.5f * Sy - 0.9f - R));
					bool bOk = true;
					for (int32 J = 0; J < RoofKeep.Num() && bOk; ++J) bOk = FVector2f::Distance(L, Off + RoofKeep[J]) > RoofKeepR[J] + R;
					for (int32 J = 0; J < Placed2.Num() && bOk; ++J) bOk = FVector2f::Distance(L, Placed2[J]) > PlacedR[J] + R + 0.3f;
					if (!bOk) continue;
					Placed2.Add(L);
					PlacedR.Add(R);
					FLocal D;
					D.Type = Type;
					D.C = ToPlan(L);
					D.YawDeg = HouseYaw + 90.0f * float(Dr.Next() % 4u) + Dr.Range(-8.0f, 8.0f);
					D.Rot = ((FMath::RoundToInt(D.YawDeg / 90.0f) % 4) + 4) % 4;
					D.Foot = Rect(true, D.C.X - 1.0f, D.C.X + 1.0f, D.C.Y - 1.0f, D.C.Y + 1.0f);
					D.Lot = Hs.Lot;
					D.bLoose = true;
					D.Lift = Slab;
					D.Parent = I;
					Pieces.Add(D);
					++Loose;
					break;
				}
			}
			// o toldo na parede da frente (a varanda já tem o dela)
			if (!bPorch && Dr.Float() < 0.3f)
			{
				const float Along = Dr.Range(-1.0f, 1.0f) * (0.5f * Sx - 2.0f);
				FLocal Aw;
				Aw.Type = TEXT("LL_Prop_Awning");
				Aw.C = ToPlan(FVector2f(Along, 0.5f * Sy));
				Aw.YawDeg = HouseYaw;
				Aw.Rot = Hs.Rot;
				Aw.Foot = Rect(true, Aw.C.X - 1.0f, Aw.C.X + 1.0f, Aw.C.Y - 1.0f, Aw.C.Y + 1.0f);
				Aw.Lot = Hs.Lot;
				Aw.bLoose = true;
				Pieces.Add(Aw);
				++Loose;
			}
		}
		// a cerca de lado na casa solta (~30%, no lugar do resto de muro) e o muro quebrado no lote aberto
		for (int32 L = 0; L < Lots.Num(); ++L)
		{
			if (Cd.Dead[L] || Lots[L].Yard.IsEmpty()) continue;
			const FLot& Lt = Lots[L];
			const FRect& Y = Lt.Yard;
			const bool bAlongU = Lt.Front == 0 || Lt.Front == 2;   // a fachada corre em u: o lado do lote corre em v
			if (Lt.Style == 1 && Lt.LooseSide < 0 && Lt.BackHouse.IsEmpty() && Dr.Float() < 0.3f)
			{
				const bool bLo = Dr.Float() < 0.5f;
				const FVector2f At = bAlongU ? FVector2f(bLo ? Y.Min.X + 0.3f : Y.Max.X - 0.3f, 0.5f * (Y.Min.Y + Y.Max.Y))
					: FVector2f(0.5f * (Y.Min.X + Y.Max.X), bLo ? Y.Min.Y + 0.3f : Y.Max.Y - 0.3f);
				Long(Dr.Float() < 0.6f ? TEXT("LL_Prop_Fence0") : TEXT("LL_Prop_Fence1"), At, bAlongU ? 90.0f : 0.0f, L);
			}
			if (Lt.Style == 3)
			{
				const FVector2f At = Lt.Lot.Center() + FVector2f(Dr.Range(-2.0f, 2.0f), Dr.Range(-2.0f, 2.0f));
				Long(Dr.Float() < 0.5f ? TEXT("LL_Prop_BrokenWall0") : TEXT("LL_Prop_BrokenWall1"), At, bAlongU ? 0.0f : 90.0f, L);
			}
		}
		// o lote aberto: dois grupos; o quintal da casa solta (sem muro): um em ~45%
		for (int32 L = 0; L < Lots.Num(); ++L)
		{
			if (Cd.Dead[L]) continue;
			if (Lots[L].Style == 3)
			{
				for (int32 K = 0; K < 2; ++K) Cluster(Lots[L].Lot.Center() + FVector2f(Dr.Range(-2.5f, 2.5f), Dr.Range(-2.5f, 2.5f)), Dr.Range(0.0f, 360.0f), L);
			}
			else if (Lots[L].Style == 1 && !Lots[L].Yard.IsEmpty() && Lots[L].BackHouse.IsEmpty() && Dr.Float() < 0.75f)
			{
				Cluster(Lots[L].Yard.Center() + FVector2f(Dr.Range(-2.0f, 2.0f), Dr.Range(-2.0f, 2.0f)), Dr.Range(0.0f, 360.0f), L);
			}
			// a areia em volta da vila: atrás do fundo do lote (2–5 m pra fora), entulho e sucata
			if (!Lots[L].bArm && Dr.Float() < 0.35f)
			{
				const FRect& R = Lots[L].Lot;
				const float Sg = R.Center().Y > 0.0f ? 1.0f : -1.0f;
				const float Out = Sg > 0.0f ? R.Max.Y : R.Min.Y;
				Cluster(FVector2f(Dr.Range(R.Min.X, R.Max.X), Out + Sg * Dr.Range(2.5f, 5.0f)), Dr.Range(0.0f, 360.0f), -1);
			}
		}
	}

	// ---- no mundo: cada cópia girada, o sobrado pela câmera, o chão aplainado --------
	// a câmera do jogo nasce olhando pra +X+Y (yaw 45°, Game RTSCameraPawn): a casa
	// cuja frente olha pra câmera esconde o próprio quintal; a de costas, a rua
	const FVector2f CamDir = FVector2f(1.0f, 1.0f).GetSafeNormal();
	auto CellOfRect = [&](const FRect& W, FIntPoint& OutC0, FIntPoint& OutN)
	{
		OutC0 = CellOf(W.Min + FVector2f(0.5f));
		const FIntPoint C1 = CellOf(W.Max - FVector2f(0.5f));
		OutN = FIntPoint(FMath::Max(1, C1.X - OutC0.X + 1), FMath::Max(1, C1.Y - OutC0.Y + 1));
	};
	int32 Tall = 0;
	for (int32 K = 0; K < Copies; ++K)
	{
		const int32 QK = Cd.Q + K * Step;
		const FVector2f CK = Spin(Cd.C, K * Step);
		for (int32 L = 0; L < Lots.Num(); ++L)
		{
			if (Cd.Dead[L]) continue;
			FVillagePad& Pd = GPads.AddDefaulted_GetRef();
			const FRect W = WorldRect(Lots[L].Lot, CK, QK);
			Pd.Min = W.Min;
			Pd.Max = W.Max;
			Pd.Z = Cd.BlockZ[K][Lots[L].Block];
			Pd.Blend = 3.0f;
		}
		// a rua e a transversal: chão de terra batida, sem pedra nem árvore (não aplaina)
		{
			FVillagePad& Pd = GPads.AddDefaulted_GetRef();
			const FRect W = WorldRect(Rect(true, -Cd.Len - 8.0f, Cd.Len + 8.0f, -Face, Face), CK, QK);
			Pd.Min = W.Min; Pd.Max = W.Max; Pd.bFlatten = false;
			for (int32 S = 0; S < Arms.Num(); ++S)
			{
				if (ArmReach[S] <= 0.0f) continue;
				FVillagePad& Pa = GPads.AddDefaulted_GetRef();
				const float V0 = Arms[S].Side > 0.0f ? Face : -ArmReach[S], V1 = Arms[S].Side > 0.0f ? ArmReach[S] : -Face;
				const FRect A = WorldRect(Rect(false, V0, V1, Arms[S].U - Arms[S].Half, Arms[S].U + Arms[S].Half), CK, QK);
				Pa.Min = A.Min; Pa.Max = A.Max; Pa.bFlatten = false;
			}
		}
		for (const FLocal& Pc : Pieces)
		{
			FVillagePiece& V = GVillage.AddDefaulted_GetRef();
			V.Type = Pc.Type;
			V.Rot = ((Pc.Rot + QK) % 4 + 4) % 4;
			if (Pc.Lot >= 0 && Lots[Pc.Lot].bTall && V.Type == TEXT("civ_house_1story") && FVector2f::DotProduct(FrontOf(V.Rot), CamDir) < 0.0f)
			{
				V.Type = TEXT("civ_house_2story");   // mesma pegada: a grade não muda
				++Tall;
			}
			const FKit& Kit = KitOf(*V.Type);
			V.Center = ToWorld(Pc.C, CK, QK);
			CellOfRect(WorldRect(Pc.Foot, CK, QK), V.Cell0, V.Cells);
			V.Size = (V.Rot % 2 == 0) ? FVector2f(Kit.W, Kit.D) : FVector2f(Kit.D, Kit.W);
			V.Height = Kit.H;
			float Lift = Pc.Lift;
			if (Pc.Parent >= 0)
			{
				// a casa dela virou sobrado nesta cópia (a mesma conta de cima): a laje sobe 3 m
				const FLocal& Hp = Pieces[Pc.Parent];
				if (Hp.Lot >= 0 && Lots[Hp.Lot].bTall && FCString::Strcmp(Hp.Type, TEXT("civ_house_1story")) == 0 && FVector2f::DotProduct(FrontOf(((Hp.Rot + QK) % 4 + 4) % 4), CamDir) < 0.0f) Lift += 3.0f;
			}
			V.Floor = (Pc.Lot >= 0 ? Cd.BlockZ[K][Lots[Pc.Lot].Block] : M.HeightAt(V.Center)) + Lift;
			V.bRoof = Pc.Lift > 0.0f;
			V.Street = Pc.Street >= 0 ? ((Pc.Street + QK) % 4 + 4) % 4 : -1;
			V.bBlocks = Kit.bBlocks && !Pc.bLoose;
			V.Village = K;
			V.Lot = Pc.Lot >= 0 ? K * 1000 + Pc.Lot : -1;
			if (Pc.YawDeg > -1e30f) V.YawDeg = FRotator::NormalizeAxis(Pc.YawDeg + 90.0f * QK);
			V.bGroundFloor = Pc.Lot < 0;   // o chão de verdade depois da cota da estrada (PlaceVillagePoles)
		}
		for (const FVector2f& P : Palms) GVillagePalms.Add(ToWorld(P, CK, QK));
	}
	IndexVillagePads();
	GVillageLines.Reset();
	for (int32 K = 0; K < Copies; ++K)
	{
		const int32 QK = Cd.Q + K * Step;
		const FVector2f CK = Spin(Cd.C, K * Step);
		int32 R = INDEX_NONE;
		float RD = 20.0f;
		for (int32 O = 0; O < NumGenRoads; ++O)
		{
			if (!GRoads[O].bPaved) continue;
			int32 Idx;
			const float Dd = NearestOn(GRoads[O].P, CK, Idx);
			if (Dd < RD) { RD = Dd; R = O; }
		}
		if (R != INDEX_NONE) GVillageLines.Add({ R, CK, QK, Cd.Len });
	}
	int32 Houses = 0;
	for (const FVillagePiece& V : GVillage) Houses += V.Type.StartsWith(TEXT("civ_house")) ? 1 : 0;
	UE_LOG(LogTemp, Display, TEXT("FORGE vila v3: %d grupos de miudeza (%d peças soltas)"), Clusters, Loose);
	UE_LOG(LogTemp, Display, TEXT("FORGE vila: %d cópias, a 1ª em (%.0f, %.0f), rua em %s, %d transversais; %d casas (%d sobrados), %d peças, %d palmeiras; %d lotes tirados pelo caminho%s; caminho ×%.2f, desvio ×%.2f; %d lugares avaliados; %.0f ms"),
		Copies, Cd.C.X, Cd.C.Y, Cd.Q == 0 ? TEXT("X") : TEXT("Y"), ArmReach.FilterByPredicate([](float R) { return R > 0.0f; }).Num(), Houses, Tall, GVillage.Num(), GVillagePalms.Num(), Removed,
		bDetour ? TEXT("") : TEXT(" (SEM desvio por fora da rua)"), WorstPath, WorstDetour, Cands.Num(), 1000.0 * (FPlatformTime::Seconds() - T0));
}

void FWorldDetail::PlaceVillagePoles(const FWorldMap& M)
{
	GVillagePoles.Reset();
	GVillageWires.Reset();
	// o chão de verdade das peças fora dos lotes (carros, miudeza da rua): a estrada já tem cota
	for (FVillagePiece& V : GVillage) if (V.bGroundFloor) V.Floor = SliceHeight(M, V.Center);
	IndexVillagePieces();
	// a frente das portas e dos portões (poste nenhum a 2,5 m) e as copas (palmeira da vila e árvore do
	// mapa perto da rua: fio nenhum atravessa)
	TArray<FVector2f> Doors;
	for (const FVillagePiece& V : GVillage)
	{
		if (!(V.Type.StartsWith(TEXT("civ_house")) || V.Type == TEXT("civ_wall_gate"))) continue;
		const float Reach = 0.5f * (V.Rot % 2 == 0 ? V.Size.Y : V.Size.X);
		Doors.Add(V.Center + FrontOf(V.Rot) * (Reach + 1.0f));
	}
	TArray<FVector2f> Crowns = GVillagePalms;
	for (const FWorldProp& P : M.Props) if (P.Type == EWorldPropType::Tree) Crowns.Add(P.Pos);
	auto SpotOk = [&](FVector2f P) -> bool
	{
		// fora da pista (de asfalto e de terra, com a beira quebrada e a esquina arredondada)
		const FRoadHit Hit = RoadAt(P);
		if (Hit.Road != INDEX_NONE && Hit.D < Hit.Half() + (Hit.Paved() ? 1.2f : 0.9f)) return false;
		const FRoadHit Other = RoadAt(P, Hit.Road, true);
		if (Other.Road != INDEX_NONE && Other.D < Other.Half() + 2.2f) return false;
		if (OnVillagePiece(P, 1.0f)) return false;
		// fora do pátio dos depósitos (a rua da mini vila começa no meio da praça: o poste caía em cima da pilha)
		for (const FWorldResource& Rs : M.Resources) if (FVector2f::Distance(P, Rs.Pos) < 24.0f) return false;
		for (const FVector2f& D : Doors) if (FVector2f::Distance(P, D) < 2.5f) return false;
		for (const FVector2f& C : Crowns) if (FVector2f::Distance(P, C) < 3.0f) return false;
		if (M.WaterLevel > -100.0f && M.HeightAt(P) < M.WaterLevel + 0.6f) return false;
		return M.NormalAt(P).Z >= 0.9f;
	};
	// o fio não passa por cima do sobrado (a laje a ~7 m) nem pela copa
	auto SpanOk = [&](FVector2f A, FVector2f B, int32 SkipPiece) -> bool
	{
		const FVector2f AB = B - A;
		const float L2 = FMath::Max(AB.SizeSquared(), 1e-4f);
		auto Dist = [&](FVector2f Q) { const float T = FMath::Clamp(((Q - A) | AB) / L2, 0.0f, 1.0f); return FVector2f::Distance(Q, A + AB * T); };
		for (const FVector2f& C : Crowns) if (Dist(C) < 2.6f) return false;
		for (int32 I = 0; I < GVillage.Num(); ++I)
		{
			const FVillagePiece& V = GVillage[I];
			if (I == SkipPiece) continue;
			// (o poste fica; a casa de 1 andar tem a laje a ~4,5 m, embaixo do fio de poste a poste)
			const bool bTall = V.Type == TEXT("civ_house_2story");
			const bool bHouse = V.Type.StartsWith(TEXT("civ_house"));
			if (!(bTall || (SkipPiece != INDEX_NONE && bHouse))) continue;
			const FVector2f H = 0.5f * V.Size + FVector2f(0.5f);
			for (int32 K = 0; K <= 16; ++K)
			{
				const FVector2f Q = A + AB * (K / 16.0f);
				if (FMath::Abs(Q.X - V.Center.X) < H.X && FMath::Abs(Q.Y - V.Center.Y) < H.Y) return false;
			}
		}
		return true;
	};
	WorldNoise::FRng Rng(WorldNoise::Hash(uint32(M.Settings.Seed) * 2246822519u + 0x9013u));
	for (const FVillageLine& Ln : GVillageLines)
	{
		if (!GRoads.IsValidIndex(Ln.Road)) continue;
		const FRoad& Road = GRoads[Ln.Road];
		// os pontos da estrada na vila e 56 m além de cada ponta (a estrada endireitada até Len + 8)
		int32 I0 = INDEX_NONE, I1 = INDEX_NONE;
		for (int32 I = 1; I + 1 < Road.P.Num(); ++I)
		{
			const FVector2f L = Spin(Road.P[I] - Ln.C, -Ln.Q);
			if (FMath::Abs(L.X) <= Ln.Len + 56.0f && FMath::Abs(L.Y) < 40.0f)
			{
				if (I0 == INDEX_NONE) I0 = I;
				I1 = I;
			}
		}
		if (I0 == INDEX_NONE) continue;
		const FVector2f SideDir = Spin(FVector2f(0.0f, -1.0f), Ln.Q);   // o lado −v da planta
		TArray<int32> Line;
		float Next = float(I0) + 4.0f;
		while (Next <= float(I1) - 2.0f)
		{
			bool bPlaced = false;
			for (const float Shift : { 0.0f, 2.5f, -2.5f, 5.0f, -5.0f, 7.5f, -7.5f })
			{
				const int32 I = FMath::Clamp(FMath::RoundToInt(Next + Shift), 1, Road.P.Num() - 2);
				const FVector2f T = (Road.P[I + 1] - Road.P[I - 1]).GetSafeNormal();
				FVector2f N(-T.Y, T.X);
				if ((N | SideDir) < 0.0f) N = -N;
				// a 1,5 m da beira do asfalto (no acostamento de 3,25 m dentro da vila)
				const FVector2f P = Road.P[I] + N * (Road.HalfAt(I) + 1.5f);
				if (!SpotOk(P)) continue;
				if (Line.Num() && FVector2f::Distance(GVillagePoles[Line.Last()].P, P) < 22.0f) continue;
				if (Line.Num() && !SpanOk(GVillagePoles[Line.Last()].P, P, INDEX_NONE)) continue;
				FVillagePole& Pl = GVillagePoles.AddDefaulted_GetRef();
				Pl.P = P;
				Pl.Road = Ln.Road;
				Pl.Z = SliceHeight(M, P);
				// o fio corre no Y local do poste (a cruzeta no X): o Y ao longo da linha
				Pl.YawDeg = FMath::RadiansToDegrees(FMath::Atan2(T.Y, T.X)) - 90.0f;
				Line.Add(GVillagePoles.Num() - 1);
				Next = float(I) + 32.0f;
				bPlaced = true;
				break;
			}
			if (!bPlaced) Next += 4.0f;
		}
		// os vãos: o isolador i com o i do vizinho (com a cruzeta virada, o inverso: o fio não cruza);
		// a flecha de ~3,5% do vão (o revisor: com 2,5% a câmera achata e some)
		for (int32 K = 0; K + 1 < Line.Num(); ++K)
		{
			const int32 A = Line[K], B = Line[K + 1];
			const FVector Xa = FRotator(0.0f, GVillagePoles[A].YawDeg, 0.0f).RotateVector(FVector::XAxisVector);
			const FVector Xb = FRotator(0.0f, GVillagePoles[B].YawDeg, 0.0f).RotateVector(FVector::XAxisVector);
			const bool bFlip = (Xa | Xb) < 0.0;
			for (int32 I = 0; I < 4; ++I)
			{
				FVillageWire& W = GVillageWires.AddDefaulted_GetRef();
				W.PoleA = A;
				W.InsA = I;
				W.PoleB = B;
				W.InsB = bFlip ? 3 - I : I;
				W.A = PoleSocket(A, W.InsA);
				W.B = PoleSocket(B, W.InsB);
				W.Sag = 0.035f * FVector::Dist2D(W.A, W.B);
			}
		}
	}
	// a ligação das casas (metade delas): do isolador de fora mais perto ao suporte na parede da frente
	// (3,4 m na de 1 andar, 6 m no sobrado), a flecha de ~2,5%. Até 22 m, de frente pra fachada (até 53°)
	// e sem passar por cima de outra rua (13/10: a ligação que cruzava o cruzamento na diagonal era um
	// fio solto e comprido no meio do asfalto; com 16 m e 35° não sobrava nenhuma: até 22 m e 53°)
	int32 Drops = 0;
	if (GVillagePoles.Num())
	{
		for (int32 H = 0; H < GVillage.Num(); ++H)
		{
			const FVillagePiece& V = GVillage[H];
			if (!V.Type.StartsWith(TEXT("civ_house")) || Rng.Float() < 0.5f) continue;
			const int32 FaceRot = V.Street >= 0 ? V.Street : V.Rot;
			const FVector2f Fr = FrontOf(FaceRot), Side(Fr.Y, -Fr.X);
			const float Reach = 0.5f * (FaceRot % 2 == 0 ? V.Size.Y : V.Size.X);
			const float Wide = 0.5f * (FaceRot % 2 == 0 ? V.Size.X : V.Size.Y);
			const FVector2f B2 = V.Center + Fr * (Reach + 0.12f) + Side * Rng.Range(-0.35f, 0.35f) * Wide;
			int32 Best = INDEX_NONE;
			float BestD = 22.0f;
			for (int32 Pl = 0; Pl < GVillagePoles.Num(); ++Pl)
			{
				const FVector2f ToPole = GVillagePoles[Pl].P - B2;
				const float D = ToPole.Size();
				// (o fio sai do poste pra frente da casa: o poste do lado da fachada, de frente pra ela)
				if (D >= BestD || ((GVillagePoles[Pl].P - V.Center) | Fr) <= Reach || (ToPole / FMath::Max(D, 0.01f) | Fr) < 0.6f) continue;
				bool bOver = false;
				for (int32 K = 1; K < 8 && !bOver; ++K)
				{
					const FRoadHit Hit = RoadAt(FMath::Lerp(B2, GVillagePoles[Pl].P, K / 8.0f), INDEX_NONE, true);
					bOver = Hit.Road != INDEX_NONE && Hit.Road != GVillagePoles[Pl].Road && Hit.D < Hit.Half() + 0.5f;
				}
				if (!bOver) { BestD = D; Best = Pl; }
			}
			if (Best == INDEX_NONE || !SpanOk(GVillagePoles[Best].P, B2, H)) continue;
			int32 Ins = 0;
			float InsD = 1e9f;
			for (const int32 I : { 0, 3 })
			{
				const float D = FVector::Dist2D(PoleSocket(Best, I), FVector(B2.X, B2.Y, 0.0f));
				if (D < InsD) { InsD = D; Ins = I; }
			}
			FVillageWire& W = GVillageWires.AddDefaulted_GetRef();
			W.PoleA = Best;
			W.InsA = Ins;
			W.PieceB = H;
			W.A = PoleSocket(Best, Ins);
			W.B = FVector(B2.X, B2.Y, V.Floor + (V.Type == TEXT("civ_house_2story") ? 6.0f : 3.4f));
			W.Sag = FMath::Max(0.15f, 0.025f * FVector::Dist2D(W.A, W.B));
			++Drops;
		}
	}
	// os postes viram peças (o jogo desenha e, depois, derruba por elas): não bloqueiam
	for (FVillagePole& Pl : GVillagePoles)
	{
		FVillagePiece& V = GVillage.AddDefaulted_GetRef();
		V.Type = TEXT("civ_pole");
		V.Center = Pl.P;
		V.YawDeg = FRotator::NormalizeAxis(Pl.YawDeg);
		V.Rot = ((FMath::RoundToInt(V.YawDeg / 90.0f) % 4) + 4) % 4;
		const float Half = M.Half();
		V.Cell0 = FIntPoint(FMath::FloorToInt((Pl.P.X + Half) / 4.0f), FMath::FloorToInt((Pl.P.Y + Half) / 4.0f));
		V.Cells = FIntPoint(1, 1);
		V.Size = FVector2f(0.4f);
		V.Height = 8.0f;
		V.Floor = Pl.Z;
		V.bBlocks = false;
		V.bGroundFloor = true;
		Pl.Piece = GVillage.Num() - 1;
	}
	IndexVillagePieces();
	UE_LOG(LogTemp, Display, TEXT("FORGE vila: %d postes, %d fios (%d ligações de casa)"), GVillagePoles.Num(), GVillageWires.Num(), Drops);
}

void FWorldDetail::IndexVillagePads()
{
	// a grade de 8 m dos pads (busca rápida no SliceHeight e no espalhamento)
	GPadGrid.Reset();
	for (int32 I = 0; I < GPads.Num(); ++I)
	{
		const FVillagePad& Pd = GPads[I];
		const int32 X0 = FMath::FloorToInt((Pd.Min.X - Pd.Blend) / 8.0f), X1 = FMath::FloorToInt((Pd.Max.X + Pd.Blend) / 8.0f);
		const int32 Y0 = FMath::FloorToInt((Pd.Min.Y - Pd.Blend) / 8.0f), Y1 = FMath::FloorToInt((Pd.Max.Y + Pd.Blend) / 8.0f);
		for (int32 Y = Y0; Y <= Y1; ++Y) for (int32 X = X0; X <= X1; ++X) GPadGrid.FindOrAdd(FIntPoint(X, Y)).Add(I);
	}
	// o chão pisado: a distância até o pad mais perto numa grade de 2 m (a caixa das vilas + 24 m)
	GVillageDist.Reset();
	if (GPads.Num())
	{
		FVector2f Lo(TNumericLimits<float>::Max()), Hi(-TNumericLimits<float>::Max());
		for (const FVillagePad& Pd : GPads) { Lo = FVector2f::Min(Lo, Pd.Min); Hi = FVector2f::Max(Hi, Pd.Max); }
		Lo -= FVector2f(24.0f);
		Hi += FVector2f(24.0f);
		GVillageDistMin = Lo;
		GVillageDistW = FMath::CeilToInt((Hi.X - Lo.X) / 2.0f) + 1;
		GVillageDistH = FMath::CeilToInt((Hi.Y - Lo.Y) / 2.0f) + 1;
		GVillageDist.SetNumUninitialized(GVillageDistW * GVillageDistH);
		ParallelFor(GVillageDistH, [&](int32 Y)
		{
			for (int32 X = 0; X < GVillageDistW; ++X)
			{
				const FVector2f P = Lo + FVector2f(X * 2.0f, Y * 2.0f);
				float Best = 1e9f;
				// (só os pads a até 24 m contam: a grade de 8 m em volta)
				for (int32 GY = FMath::FloorToInt((P.Y - 24.0f) / 8.0f); GY <= FMath::FloorToInt((P.Y + 24.0f) / 8.0f); ++GY)
				{
					for (int32 GX = FMath::FloorToInt((P.X - 24.0f) / 8.0f); GX <= FMath::FloorToInt((P.X + 24.0f) / 8.0f); ++GX)
					{
						const TArray<int32>* L = GPadGrid.Find(FIntPoint(GX, GY));
						if (!L) continue;
						for (const int32 I : *L)
						{
							const FVillagePad& Pd = GPads[I];
							const float DX = FMath::Max3(Pd.Min.X - P.X, 0.0f, P.X - Pd.Max.X), DY = FMath::Max3(Pd.Min.Y - P.Y, 0.0f, P.Y - Pd.Max.Y);
							Best = FMath::Min(Best, FMath::Sqrt(DX * DX + DY * DY));
						}
					}
				}
				GVillageDist[Y * GVillageDistW + X] = Best;
			}
		});
	}
}

void FWorldDetail::BuildHamlets(const FWorldMap& M)
{
	// As mini vilas v2 (13/10, o Felipe sobre a v1: "parece cabanas jogadas de forma porca"): a gramática da vila do
	// centro, que ele aprovou, em ponto pequeno. Uma rua de terra sai do depósito (a praça), com 20 m de fachada a
	// fachada; os lotes de 8 m (às vezes 16) dos dois lados, a casa de frente pra rua e o quintal atrás com a mesma
	// mistura da vila (quintal murado com portão, casa solta, pomar murado com palmeira, lote aberto); no fundo da
	// praça, uma fileira de casas emendada por muro, virada pro depósito. O povoado do caminho de caravana: o khan
	// no meio e as casas na rua de chegada (Hakim, "Arabic-Islamic Cities"). A rua é o caminho dos caminhões.
	const double T0 = FPlatformTime::Seconds();
	const float Half = M.Half();
	TArray<FVector2f> Docks, Bases;
	for (const FVector3f& K : M.Settings.KeepOut) (K.Z < 60.0f ? Docks : Bases).Add(FVector2f(K.X, K.Y));
	for (const FWorldStart& S : M.Starts) Bases.Add(S.Pos);
	if (Docks.Num() == 0) for (const FWorldResource& Rs : M.Resources) Docks.Add(Rs.Pos);
	if (Docks.Num() == 0) return;
	constexpr float DockFree = 26.0f;      // o pátio dos caminhões: a pilha (26 × 32 m) e a volta
	constexpr float BaseFree = 94.0f;      // a área da base (88 m) e 6 m
	constexpr float LaneHalf = 12.0f;      // o corredor do depósito até a base, quando a rua não vai pra lá
	constexpr float LaneLen = 110.0f;
	constexpr float Face = 10.0f;          // a fachada (20 m de fachada a fachada, como a rua da vila)
	constexpr float Back = 30.0f;          // o fundo dos lotes
	constexpr float Start = 24.0f;         // onde começa a rua (a beira da praça)
	constexpr float Len = 72.0f;           // o comprimento da rua com casa (48 deixava 3 casas no depósito da base)
	const int32 NPads0 = GPads.Num(), NPieces0 = GVillage.Num();
	const FVector2f CamDir = FVector2f(1.0f, 1.0f).GetSafeNormal();   // a câmera do jogo nasce a 45° (o sobrado)
	auto CellOfW = [&](FVector2f P) { return FIntPoint(FMath::FloorToInt((P.X + Half) / CellM), FMath::FloorToInt((P.Y + Half) / CellM)); };
	auto SegDist = [](FVector2f P, FVector2f A, FVector2f B)
	{
		const FVector2f AB = B - A;
		const float T = FMath::Clamp(FVector2f::DotProduct(P - A, AB) / FMath::Max(AB.SizeSquared(), 1e-6f), 0.0f, 1.0f);
		return FVector2f::Distance(P, A + T * AB);
	};
	// o retângulo (mundo) pode? Strict: casa, muro, carro (bloqueiam); senão a miudeza (só o chão). Street: a direção
	// da rua no mundo (o corredor até a base que vai pelo mesmo lado é a própria rua)
	auto Free = [&](FVector2f Min, FVector2f Max, FVector2f Dock, FVector2f Street, bool bStrict, TArray<float>* Hs) -> bool
	{
		int32 Steep = 0, N = 0;
		for (float X = Min.X + 1.0f; X < Max.X; X += 2.0f)
		{
			for (float Y = Min.Y + 1.0f; Y < Max.Y; Y += 2.0f)
			{
				const FVector2f P(X, Y);
				if (FMath::Abs(P.X) > Half - 24.0f || FMath::Abs(P.Y) > Half - 24.0f) return false;
				const float H = M.HeightAt(P);
				if (M.WaterLevel > -100.0f && H < M.WaterLevel + 0.3f) return false;
				const FRoadHit Hit = RoadAt(P);
				if (Hit.Road != INDEX_NONE && Hit.D < Hit.Half() + (bStrict ? 3.0f : 1.0f)) return false;
				if (bStrict)
				{
					for (const FVector2f& B : Bases) if (FVector2f::Distance(P, B) < BaseFree) return false;
					for (const FVector2f& D : Docks) if (FVector2f::Distance(P, D) < DockFree) return false;
					for (const FVector2f& B : Bases)
					{
						const FVector2f Dir = B - Dock;
						if (Dir.Size() > 450.0f || FVector2f::DotProduct(Dir.GetSafeNormal(), Street) > 0.7f) continue;
						if (SegDist(P, Dock, Dock + Dir.GetSafeNormal() * LaneLen) < LaneHalf) return false;
					}
					if (M.Pass.Num() == M.N * M.N)
					{
						const FVector2f G = M.ToGrid(P);
						const int32 VI = FMath::Clamp(FMath::RoundToInt(G.X), 0, M.N - 1), VJ = FMath::Clamp(FMath::RoundToInt(G.Y), 0, M.N - 1);
						if (M.Pass[M.Index(VI, VJ)] != EWorldPass::Open) return false;
					}
					Steep += M.NormalAt(P).Z < 0.85f ? 1 : 0;
				}
				else if (FVector2f::Distance(P, Dock) < 15.0f) return false;   // a pilha em si
				++N;
				if (Hs) Hs->Add(H);
			}
		}
		if (!bStrict) return true;
		for (const FVillagePad& Pd : GPads)
		{
			if (!Pd.bFlatten) continue;   // (o pátio de terra das outras não conta: só os lotes)
			if (Min.X < Pd.Max.X + 6.0f && Max.X > Pd.Min.X - 6.0f && Min.Y < Pd.Max.Y + 6.0f && Max.Y > Pd.Min.Y - 6.0f) return false;
		}
		return N > 0 && Steep * 3 <= N;
	};
	auto WorldRectOf = [&](const FRect& R, FVector2f C, int32 Q)
	{
		const FVector2f A = C + Spin(R.Min, Q), B = C + Spin(R.Max, Q);
		FRect W;
		W.Min = FVector2f(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
		W.Max = FVector2f(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
		return W;
	};
	auto LRect = [](float U0, float U1, float V0, float V1)
	{
		FRect R;
		R.Min = FVector2f(FMath::Min(U0, U1), FMath::Min(V0, V1));
		R.Max = FVector2f(FMath::Max(U0, U1), FMath::Max(V0, V1));
		return R;
	};
	// as cópias: o depósito girado de 180° (2 jogadores) é a mesma mini vila girada (justiça)
	TArray<int32> Twin;
	Twin.Init(INDEX_NONE, Docks.Num());
	if (M.Settings.Players == 2)
	{
		for (int32 I = 0; I < Docks.Num(); ++I)
		{
			for (int32 J = I + 1; J < Docks.Num(); ++J)
			{
				if (Twin[I] == INDEX_NONE && Twin[J] == INDEX_NONE && FVector2f::Distance(Docks[I], -Docks[J]) < 4.0f) { Twin[I] = J; Twin[J] = I; }
			}
		}
	}

	// ---- a planta local de uma mini vila (o depósito em (0, 0), a rua em u = 0 indo pra +v) ------------
	struct FLay
	{
		TArray<FLocal> Pieces;
		TArray<FRect> LotRects;          // o lote inteiro (casa e quintal): a pegada que tem de caber
		TArray<bool> LotHouse;           // o lote tem casa
		TArray<FVector2f> Palms;
	};
	auto MakeLayout = [&](uint32 Seed) -> FLay
	{
		FLay Ly;
		WorldNoise::FRng Rng(Seed);
		TSet<FIntVector> WallKeys;
		auto Wall = [&](bool bAlongU, float Line, float A, int32 Out, float InsideSign, int32 Lot, bool bGate)
		{
			const FIntVector Key(bAlongU ? 1 : 0, FMath::RoundToInt(Line * 10.0f), FMath::RoundToInt(A * 10.0f));
			if (WallKeys.Contains(Key)) return;
			WallKeys.Add(Key);
			FLocal W;
			W.Type = bGate ? TEXT("civ_wall_gate") : TEXT("civ_wall");
			W.C = bAlongU ? FVector2f(A, Line) : FVector2f(Line, A);
			W.Rot = Out;
			const float I0 = Line, I1 = Line + InsideSign * CellM;
			W.Foot = bAlongU ? LRect(A - 2.0f, A + 2.0f, I0, I1) : LRect(I0, I1, A - 2.0f, A + 2.0f);
			W.Lot = Lot;
			Ly.Pieces.Add(W);
		};
		// os muros de um quintal (os lados menos o da casa; lados como o giro da frente: 0 = +v, 1 = −u, 2 = −v, 3 = +u)
		auto YardWalls = [&](const FRect& Y, int32 Lot, int32 HouseSide, int32 GateSide)
		{
			for (int32 Side = 0; Side < 4; ++Side)
			{
				if (Side == HouseSide) continue;
				const bool bAlongU = Side == 0 || Side == 2;
				const float A0 = bAlongU ? Y.Min.X : Y.Min.Y, A1 = bAlongU ? Y.Max.X : Y.Max.Y;
				const float Line = Side == 0 ? Y.Max.Y : (Side == 2 ? Y.Min.Y : (Side == 1 ? Y.Min.X : Y.Max.X));
				const float Inside = (Side == 0 || Side == 3) ? -1.0f : 1.0f;
				const int32 N = FMath::RoundToInt((A1 - A0) / CellM);
				for (int32 K = 0; K < N; ++K) Wall(bAlongU, Line, A0 + (K + 0.5f) * CellM, Side, Inside, Lot, Side == GateSide && K == N / 2);
			}
		};
		auto AddHouse = [&](const TCHAR* Type, const FRect& Foot, int32 Rot, int32 Lot, bool bTall)
		{
			FLocal H;
			H.Type = Type;
			H.C = Foot.Center();
			H.Rot = Rot;
			H.Foot = Foot;
			H.Lot = Lot;
			H.Street = Rot;
			H.Lift = bTall ? 1.0f : 0.0f;   // (marca do sobrado; a laje não entra aqui)
			Ly.Pieces.Add(H);
		};
		// as duas fileiras da rua
		for (const float S : { 1.0f, -1.0f })
		{
			const int32 Front = S > 0.0f ? 1 : 3;           // a frente olha a rua (−u na fileira de +u)
			const int32 Out = S > 0.0f ? 3 : 1;             // o fundo do lote
			float A = Start;
			while (Start + Len - A >= 8.0f - 0.01f)
			{
				const float Wd = (Start + Len - A >= 16.0f - 0.01f && Rng.Float() < 0.25f) ? 16.0f : 8.0f;
				const int32 Lot = Ly.LotRects.Num();
				const FRect LotR = LRect(S * Face, S * Back, A, A + Wd);
				Ly.LotRects.Add(LotR);
				const float R = Rng.Float();
				const int32 Style = R < 0.32f ? 0 : (R < 0.86f ? 1 : (R < 0.92f ? 2 : 3));
				Ly.LotHouse.Add(Style < 2);
				if (Style < 2)
				{
					const bool bLong = Wd >= 16.0f;
					const bool bPorch = !bLong && Rng.Float() < 0.3f;
					const TCHAR* Type = bLong ? TEXT("civ_house_long") : (bPorch ? TEXT("civ_house_porch") : TEXT("civ_house_1story"));
					const float Dp = bPorch ? 12.0f : 8.0f;
					const FRect Hr = LRect(S * Face, S * (Face + Dp), A, A + Wd);
					AddHouse(Type, Hr, Front, Lot, !bLong && !bPorch && Rng.Float() < 0.2f);
					const FRect Yd = LRect(S * (Face + Dp), S * Back, A, A + Wd);
					if (Style == 0) YardWalls(Yd, Lot, Front, Out);
					else if (Rng.Float() < 0.4f)
					{
						// o resto de muro da casa solta: 1–3 peças no fundo do quintal, a partir de um canto
						const int32 N = 1 + int32(Rng.Next() % 3u);
						const bool bLo = Rng.Float() < 0.5f;
						const int32 NMax = FMath::RoundToInt(Wd / CellM);
						for (int32 K = 0; K < FMath::Min(N, NMax); ++K) Wall(false, S * Back, A + ((bLo ? K : NMax - 1 - K) + 0.5f) * CellM, Out, -S, Lot, false);
					}
					const float Pp = Style == 0 ? 0.4f : 0.15f;
					if (Rng.Float() < Pp) Ly.Palms.Add(Yd.Center() + FVector2f(Rng.Range(-1.5f, 1.5f), Rng.Range(-1.5f, 1.5f)));
				}
				else if (Style == 2)
				{
					// o pomar murado (a palmeira das artes), o portão pra rua
					YardWalls(LotR, Lot, -1, Front);
					Ly.Palms.Add(LotR.Center() + FVector2f(Rng.Range(-2.0f, 2.0f), Rng.Range(-2.0f, 2.0f)));
				}
				A += Wd;
			}
		}
		// a praça em volta do depósito: o fundo e os dois lados, uma casa por lote (o lote que não cabe sai sozinho),
		// viradas pro depósito e emendadas por muro onde não tem casa (o depósito apertado, sem rua, ainda vira povoado)
		auto PlazaSlot = [&](const FRect& House, int32 Rot, bool bAlongU, float Line, float A0, float InsideSign)
		{
			const int32 Lot = Ly.LotRects.Num();
			Ly.LotRects.Add(House);
			const bool bHouse = Rng.Float() < 0.75f;
			Ly.LotHouse.Add(bHouse);
			if (bHouse) AddHouse(TEXT("civ_house_1story"), House, Rot, Lot, Rng.Float() < 0.25f);
			else
			{
				Wall(bAlongU, Line, A0 + 2.0f, Rot, InsideSign, Lot, false);
				Wall(bAlongU, Line, A0 + 6.0f, Rot, InsideSign, Lot, Rng.Float() < 0.3f);
			}
		};
		for (float U = -24.0f; U + 8.0f <= 24.0f + 0.01f; U += 8.0f)
		{
			PlazaSlot(LRect(U, U + 8.0f, -34.0f, -26.0f), 0, true, -26.0f, U, -1.0f);   // o fundo: a frente em +v
		}
		for (const float S : { 1.0f, -1.0f })
		{
			for (float V = -16.0f; V + 8.0f <= 16.0f + 0.01f; V += 8.0f)
			{
				// os lados: a frente pro depósito (−u na direita, +u na esquerda)
				PlazaSlot(LRect(S * 26.0f, S * 34.0f, V, V + 8.0f), S > 0.0f ? 1 : 3, false, S * 26.0f, V, S);
			}
		}
		// as miudezas: grupos no pé da fachada, entre as portas, e na beira da praça (caixas e tambores do depósito)
		static const TCHAR* Big[] = { TEXT("LL_Prop_Crates"), TEXT("civ_crates"), TEXT("LL_Prop_Barrels"), TEXT("LL_Prop_Rubble1"), TEXT("LL_Prop_Scrap1") };
		static const TCHAR* Small[] = { TEXT("LL_Prop_Tires"), TEXT("LL_Prop_Ammo"), TEXT("civ_barrels"), TEXT("LL_Prop_Crates") };
		auto Cluster = [&](FVector2f C)
		{
			const int32 NS = 1 + int32(Rng.Next() % 3u);
			for (int32 K = 0; K <= NS; ++K)
			{
				FLocal P;
				P.Type = K == 0 ? Big[Rng.Next() % 5u] : Small[Rng.Next() % 4u];
				P.C = C + (K == 0 ? FVector2f::ZeroVector : FVector2f(Rng.Range(-2.4f, 2.4f), Rng.Range(-2.4f, 2.4f)));
				P.YawDeg = Rng.Range(0.0f, 360.0f);
				P.Rot = ((FMath::RoundToInt(P.YawDeg / 90.0f) % 4) + 4) % 4;
				P.Foot = LRect(P.C.X - 1.0f, P.C.X + 1.0f, P.C.Y - 1.0f, P.C.Y + 1.0f);
				P.bLoose = true;
				Ly.Pieces.Add(P);
			}
		};
		for (const float S : { 1.0f, -1.0f })
		{
			for (float V = Start + 8.0f; V < Start + Len; V += 16.0f)
			{
				if (Rng.Float() < 0.6f) Cluster(FVector2f(S * (Face - 1.6f), V + Rng.Range(-1.5f, 1.5f)));
			}
		}
		for (int32 K = 0; K < 3; ++K)
		{
			const float Ang = Rng.Range(1.1f * PI, 1.9f * PI);   // o fundo e os lados da praça (a rua livre)
			Cluster(FVector2f(19.0f * FMath::Cos(Ang), 19.0f * FMath::Sin(Ang)));
		}
		// um carro no acostamento da rua, quase paralelo a ela (bloqueia)
		if (Rng.Float() < 0.6f)
		{
			const float S = Rng.Float() < 0.5f ? 1.0f : -1.0f;
			const float V = Start + Rng.Range(10.0f, Len - 6.0f);
			FLocal Car;
			Car.Type = Rng.Float() < 0.5f ? TEXT("civ_pickup") : TEXT("civ_car_burnt");
			Car.C = FVector2f(S * (Face - 2.2f), V);
			Car.Rot = 0;
			Car.YawDeg = Rng.Range(-10.0f, 10.0f);
			Car.Foot = LRect(S * (Face - 4.0f), S * Face, V - 4.0f, V + 4.0f);
			Ly.Pieces.Add(Car);
		}
		return Ly;
	};

	int32 Hamlets = 0, Houses = 0, Dropped = 0, NewRoads = 0;
	for (int32 I = 0; I < Docks.Num(); ++I)
	{
		if (Twin[I] != INDEX_NONE && Twin[I] < I) continue;   // o irmão já fez
		const FVector2f D0 = Docks[I];
		const FVector2f O0(FMath::RoundToFloat((D0.X + Half) / CellM) * CellM - Half, FMath::RoundToFloat((D0.Y + Half) / CellM) * CellM - Half);
		FVector2f Near = Bases.Num() ? Bases[0] : FVector2f::ZeroVector;
		for (const FVector2f& B : Bases) if (FVector2f::Distance(B, D0) < FVector2f::Distance(Near, D0)) Near = B;
		const FLay Ly = MakeLayout(WorldNoise::Hash(uint32(M.Settings.Seed) * 2654435761u + 0x7a3b11u + uint32(I) * 97u));
		struct FCopy { FVector2f Dock, O; int32 Q; };
		// a direção da rua: das 4, a que deixa mais casas (nas duas cópias); no empate, a que vai pra base mais perto
		int32 BestQ = -1, BestAlive = 0;
		float BestDot = -2.0f;
		TArray<bool> BestLotDead;
		TArray<TArray<float>> BestLotZ;
		for (int32 Q = 0; Q < 4; ++Q)
		{
			TArray<FCopy> Copies = { { D0, O0, Q } };
			if (Twin[I] != INDEX_NONE) Copies.Add({ Docks[Twin[I]], -O0, Q + 2 });
			TArray<bool> LotDead;
			LotDead.Init(false, Ly.LotRects.Num());
			// a rua: do meio da praça até o fim, o leito (±4 m) sem água, penhasco nem encosta, nas duas cópias
			float StreetMax = Start + Len;
			for (int32 K = 0; K < Copies.Num(); ++K)
			{
				for (float V = 0.0f; V <= StreetMax; V += 2.0f)
				{
					bool bBad = false;
					for (const float U : { -4.0f, 0.0f, 4.0f })
					{
						const FVector2f P = Copies[K].O + Spin(FVector2f(U, V), Copies[K].Q);
						if (FMath::Abs(P.X) > Half - 24.0f || FMath::Abs(P.Y) > Half - 24.0f) { bBad = true; break; }
						if (M.WaterLevel > -100.0f && M.HeightAt(P) < M.WaterLevel + 0.3f) { bBad = true; break; }
						if (M.NormalAt(P).Z < 0.9f) { bBad = true; break; }
						if (M.Pass.Num() == M.N * M.N)
						{
							const FVector2f G = M.ToGrid(P);
							const int32 VI = FMath::Clamp(FMath::RoundToInt(G.X), 0, M.N - 1), VJ = FMath::Clamp(FMath::RoundToInt(G.Y), 0, M.N - 1);
							if (M.Pass[M.Index(VI, VJ)] != EWorldPass::Open) { bBad = true; break; }
						}
					}
					if (bBad) { StreetMax = FMath::Max(0.0f, V - 2.0f); break; }
				}
			}
			for (int32 L = 0; L < Ly.LotRects.Num(); ++L)
			{
				if (Ly.LotRects[L].Min.Y >= Start - 0.01f && Ly.LotRects[L].Max.Y > StreetMax + 0.01f) LotDead[L] = true;
			}
			TArray<TArray<float>> LotZ;
			LotZ.SetNum(Copies.Num());
			for (int32 K = 0; K < Copies.Num(); ++K)
			{
				LotZ[K].Init(0.0f, Ly.LotRects.Num());
				const FVector2f StreetW = Spin(FVector2f(0.0f, 1.0f), Copies[K].Q);
				for (int32 L = 0; L < Ly.LotRects.Num(); ++L)
				{
					if (LotDead[L]) continue;
					TArray<float> Hs;
					const FRect W = WorldRectOf(Ly.LotRects[L], Copies[K].O, Copies[K].Q);
					if (!Free(W.Min, W.Max, Copies[K].Dock, StreetW, true, &Hs) || Hs.Num() == 0) { LotDead[L] = true; continue; }
					Hs.Sort();
					if (Hs.Last() - Hs[0] > 2.5f) { LotDead[L] = true; continue; }
					LotZ[K][L] = Hs[Hs.Num() / 2];
				}
			}
			int32 Alive = 0;
			for (int32 L = 0; L < Ly.LotRects.Num(); ++L) Alive += (!LotDead[L] && Ly.LotHouse[L]) ? 1 : 0;
			const float Dot = FVector2f::DotProduct(Spin(FVector2f(0.0f, 1.0f), Q), (Near - D0).GetSafeNormal());
			if (Alive > BestAlive || (Alive == BestAlive && Alive > 0 && Dot > BestDot))
			{
				BestAlive = Alive;
				BestQ = Q;
				BestDot = Dot;
				BestLotDead = LotDead;
				BestLotZ = LotZ;
			}
		}
		if (BestQ < 0 || BestAlive < 5) { ++Dropped; continue; }   // menos de 5 casas não é povoado
		TArray<FCopy> Copies = { { D0, O0, BestQ } };
		if (Twin[I] != INDEX_NONE) Copies.Add({ Docks[Twin[I]], -O0, BestQ + 2 });
		// as peças soltas (carro e miudeza) que não servem em alguma cópia saem
		TArray<bool> Dead;
		Dead.Init(false, Ly.Pieces.Num());
		for (int32 P = 0; P < Ly.Pieces.Num(); ++P)
		{
			const FLocal& Pc = Ly.Pieces[P];
			if (Pc.Lot >= 0) { Dead[P] = BestLotDead[Pc.Lot]; continue; }
			const bool bBlocks = KitOf(Pc.Type).bBlocks && !Pc.bLoose;
			for (int32 K = 0; K < Copies.Num() && !Dead[P]; ++K)
			{
				const FRect W = WorldRectOf(Pc.Foot, Copies[K].O, Copies[K].Q);
				if (!Free(W.Min, W.Max, Copies[K].Dock, Spin(FVector2f(0.0f, 1.0f), Copies[K].Q), bBlocks, nullptr)) Dead[P] = true;
			}
		}
		// a rua só até o último lote vivo (a rua sem casa no fim não fica)
		float StreetEnd = Start;
		for (int32 L = 0; L < Ly.LotRects.Num(); ++L)
		{
			if (!BestLotDead[L] && Ly.LotRects[L].Min.Y >= Start - 0.01f) StreetEnd = FMath::Max(StreetEnd, Ly.LotRects[L].Max.Y);
		}
		for (int32 K = 0; K < Copies.Num(); ++K)
		{
			const FCopy& Cp = Copies[K];
			const int32 Village = 10 + I * 2 + K;   // as mini vilas: 10 + 2 × depósito + cópia
			for (int32 L = 0; L < Ly.LotRects.Num(); ++L)
			{
				if (BestLotDead[L]) continue;
				const FRect W = WorldRectOf(Ly.LotRects[L], Cp.O, Cp.Q);
				FVillagePad& Pd = GPads.AddDefaulted_GetRef();
				Pd.Min = W.Min;
				Pd.Max = W.Max;
				Pd.Z = BestLotZ[K][L];
				Pd.Blend = 3.0f;
			}
			for (int32 P = 0; P < Ly.Pieces.Num(); ++P)
			{
				if (Dead[P]) continue;
				const FLocal& Pc = Ly.Pieces[P];
				FVillagePiece& V = GVillage.AddDefaulted_GetRef();
				V.Type = Pc.Type;
				V.Rot = ((Pc.Rot + Cp.Q) % 4 + 4) % 4;
				if (Pc.Lift > 0.0f && V.Type == TEXT("civ_house_1story") && FVector2f::DotProduct(FrontOf(V.Rot), CamDir) < 0.0f) V.Type = TEXT("civ_house_2story");
				const FKit& Kit = KitOf(*V.Type);
				V.Center = Cp.O + Spin(Pc.C, Cp.Q);
				const FRect W = WorldRectOf(Pc.Foot, Cp.O, Cp.Q);
				V.Cell0 = CellOfW(W.Min + FVector2f(0.5f));
				const FIntPoint C1 = CellOfW(W.Max - FVector2f(0.5f));
				V.Cells = FIntPoint(FMath::Max(1, C1.X - V.Cell0.X + 1), FMath::Max(1, C1.Y - V.Cell0.Y + 1));
				V.Size = (V.Rot % 2 == 0) ? FVector2f(Kit.W, Kit.D) : FVector2f(Kit.D, Kit.W);
				V.Height = Kit.H;
				V.Floor = Pc.Lot >= 0 ? BestLotZ[K][Pc.Lot] : M.HeightAt(V.Center);
				V.Street = Pc.Street >= 0 ? ((Pc.Street + Cp.Q) % 4 + 4) % 4 : -1;
				V.bBlocks = Kit.bBlocks && !Pc.bLoose;
				V.Village = Village;
				V.Lot = Pc.Lot >= 0 ? Village * 1000 + Pc.Lot : -1;
				if (Pc.YawDeg > -1e30f) V.YawDeg = FRotator::NormalizeAxis(Pc.YawDeg + 90.0f * Cp.Q);
				V.bGroundFloor = Pc.Lot < 0;
				Houses += (K == 0 && V.Type.StartsWith(TEXT("civ_house"))) ? 1 : 0;
			}
			for (const FVector2f& Pl : Ly.Palms)
			{
				// (a palmeira do lote que saiu sai junto)
				bool bAlive = false;
				for (int32 L = 0; L < Ly.LotRects.Num() && !bAlive; ++L)
				{
					const FRect& Lr = Ly.LotRects[L];
					bAlive = !BestLotDead[L] && Pl.X > Lr.Min.X && Pl.X < Lr.Max.X && Pl.Y > Lr.Min.Y && Pl.Y < Lr.Max.Y;
				}
				if (bAlive) GVillagePalms.Add(Cp.O + Spin(Pl, Cp.Q));
			}
			// o pátio de terra da praça (tira pedra e capim; não aplaina)
			{
				const FRect W = WorldRectOf(LRect(-24.0f, 24.0f, -24.0f, 24.0f), Cp.O, Cp.Q);
				FVillagePad& Yd = GPads.AddDefaulted_GetRef();
				Yd.Min = W.Min;
				Yd.Max = W.Max;
				Yd.bFlatten = false;
			}
			// a rua de terra: do meio da praça até o último lote (uma estrada de terra de verdade: o leito plano,
			// a pintura de terra e a cota, como as trilhas)
			if (StreetEnd > Start + 1.0f)
			{
				FRoad& Rd = GRoads.AddDefaulted_GetRef();
				for (float V = 0.0f; V <= StreetEnd + 4.0f + 0.01f; V += 1.0f) Rd.P.Add(Cp.O + Spin(FVector2f(0.0f, V), Cp.Q));
				Rd.Half = 4.0f;
				Rd.bPaved = false;
				Rd.bOpen[0] = Rd.bOpen[1] = true;
				Rd.Fade[0] = Rd.Fade[1] = 4.0f;
				++NewRoads;
				// os postes e os fios ao longo da rua, como na vila do centro (o poste no acostamento, a ligação das casas)
				FVillageLine& Ln = GVillageLines.AddDefaulted_GetRef();
				Ln.Road = GRoads.Num() - 1;
				Ln.C = Cp.O + Spin(FVector2f(0.0f, 0.5f * (Start + StreetEnd)), Cp.Q);
				Ln.Q = Cp.Q + 1;
				Ln.Len = 0.5f * (StreetEnd - Start) + 8.0f;
			}
			++Hamlets;
		}
	}
	if (NewRoads)
	{
		for (FRoad& R : GRoads) if (R.Z.Num() != R.P.Num()) R.Z.Init(0.0f, R.P.Num());
		IndexRoads();
	}
	IndexVillagePads();
	UE_LOG(LogTemp, Display, TEXT("FORGE mini vilas: %d em volta de %d depósitos (%d sem lugar), %d casas no 1º de cada par, %d peças, %d pads, %d ruas; %.0f ms"),
		Hamlets, Docks.Num(), Dropped, Houses, GVillage.Num() - NPieces0, GPads.Num() - NPads0, NewRoads, 1000.0 * (FPlatformTime::Seconds() - T0));
}

uint32 FWorldDetail::VillageHash() const
{
	// a impressão digital: só os campos da simulação (inteiros), na ordem do id
	uint32 H = 0x5a17u;
	for (const FVillagePiece& V : GVillage)
	{
		const int32 F[8] = { V.Rot, V.Cell0.X, V.Cell0.Y, V.Cells.X, V.Cells.Y, FMath::RoundToInt(V.Floor * 100.0f), V.Street, V.bBlocks ? 1 : 0 };
		H = FCrc::MemCrc32(F, sizeof(F), FCrc::StrCrc32(*V.Type, H));
	}
	return H;
}

FString FWorldDetail::VillageJson(const FString& MapName) const
{
	const float Half = GMap.IsValid() ? GMap->Half() : 0.0f;
	FString S = FString::Printf(TEXT("{\n \"map\": \"%s\",\n \"version\": 3,\n \"hash\": \"%08x\",\n \"cell_m\": 4,\n \"origin_m\": [%.1f, %.1f],\n"), *MapName, VillageHash(), -Half, -Half);
	S += TEXT(" \"about\": \"Unreal meters (X, Y). rot 0..3: yaw = 90*rot; the mesh front (door) faces +Y at rot 0; mesh origin at the footprint center on the ground. cell = [i, j] of the footprint corner on the 4 m grid counted from origin_m; cells = [w, h] already rotated. floor_cm = flattened ground height. street = side facing the street (same 0..3 as rot), -1 none. entry = [i, j] of the free cell in front of the door (houses). mesh = the Look civil kit mesh. blocks = blocked cells in the simulation. hash = CRC of the simulation fields (the built map has an actor tagged WorldForgeVillage with village_hash:<hash>). Sorted by id; the simulation fields are integers. v3 (13/10): yaw_deg = free yaw (Unreal yaw, degrees) of loose props, wrecks and poles; rot stays the nearest 90 step for older readers. poles = the power poles (piece id, knockable; the crossarm on the mesh local X, wires along local Y; sockets_local = the 4 insulator tips in meters, mesh space). wires = catenaries: a/b = [pole index, socket] or b_wall = [x, y, z] meters on a house wall (b_piece = its id); sag_m = mid-span drop.\",\n \"pieces\": [");
	const float CellSz = 4.0f;
	// as células que bloqueiam (pra porta cair numa livre)
	TSet<FIntPoint> Blocked;
	for (const FVillagePiece& V : GVillage)
	{
		if (!V.bBlocks) continue;
		for (int32 J = 0; J < V.Cells.Y; ++J) for (int32 I = 0; I < V.Cells.X; ++I) Blocked.Add(V.Cell0 + FIntPoint(I, J));
	}
	for (int32 I = 0; I < GVillage.Num(); ++I)
	{
		const FVillagePiece& V = GVillage[I];
		FString Entry;
		if (V.Type.StartsWith(TEXT("civ_house")))
		{
			// a célula da porta: a primeira livre à frente do meio da fachada, andando
			// pra rua (passa o pátio murado da frente e o carro parado na porta)
			const FVector2f Front = V.Rot == 0 ? FVector2f(0, 1) : (V.Rot == 1 ? FVector2f(-1, 0) : (V.Rot == 2 ? FVector2f(0, -1) : FVector2f(1, 0)));
			const float Reach = 0.5f * (V.Rot % 2 == 0 ? V.Size.Y : V.Size.X) + 0.5f * CellSz;
			FIntPoint C(0, 0);
			for (int32 K = 0; K < 4; ++K)
			{
				const FVector2f P = V.Center + Front * (Reach + K * CellSz);
				C = FIntPoint(FMath::FloorToInt((P.X + Half) / CellSz), FMath::FloorToInt((P.Y + Half) / CellSz));
				if (!Blocked.Contains(C)) break;
			}
			Entry = FString::Printf(TEXT(", \"entry\": [%d, %d]"), C.X, C.Y);
		}
		// size_m e height_m: a caixa provisória (a malha no mundo, já girada) até as malhas do Look
		const FString Yaw = (V.HasYaw() ? FString::Printf(TEXT(", \"yaw_deg\": %.1f"), V.YawDeg) : FString())
			+ (V.bRoof ? FString::Printf(TEXT(", \"z\": %.2f"), V.Floor) : FString());
		S += FString::Printf(TEXT("%s\n  {\"id\": %d, \"type\": \"%s\", \"mesh\": \"SM_%s\", \"x\": %.2f, \"y\": %.2f, \"rot\": %d%s, \"cell\": [%d, %d], \"cells\": [%d, %d], \"floor_cm\": %d, \"street\": %d%s, \"blocks\": %s, \"village\": %d, \"lot\": %d, \"size_m\": [%.1f, %.1f], \"height_m\": %.1f}"),
			I ? TEXT(",") : TEXT(""), I, *V.Type, *V.Type, V.Center.X, V.Center.Y, V.Rot, *Yaw, V.Cell0.X, V.Cell0.Y, V.Cells.X, V.Cells.Y, FMath::RoundToInt(V.Floor * 100.0f), V.Street,
			*Entry, V.bBlocks ? TEXT("true") : TEXT("false"), V.Village, V.Lot, V.Size.X, V.Size.Y, V.Height);
	}
	S += TEXT("\n ],\n \"lots_debug\": [");
	for (int32 I = 0; I < GVillageLots.Num(); ++I)
	{
		const FVector4f& R = GVillageLots[I];
		S += FString::Printf(TEXT("%s[%.1f, %.1f, %.1f, %.1f, %d]"), I ? TEXT(", ") : TEXT(""), R.X, R.Y, R.Z, R.W, GVillageLotWhy.IsValidIndex(I) ? GVillageLotWhy[I] : 0);
	}
	S += TEXT("],\n \"poles\": [");
	for (int32 I = 0; I < GVillagePoles.Num(); ++I)
	{
		const FVillagePole& Pl = GVillagePoles[I];
		S += FString::Printf(TEXT("%s\n  {\"piece\": %d, \"x\": %.2f, \"y\": %.2f, \"z\": %.2f, \"yaw_deg\": %.1f, \"height_m\": 8.0, \"radius_m\": 0.15, \"knockable\": true, \"sockets_local\": [[-0.95, 0, 7.572], [-0.35, 0, 7.572], [0.35, 0, 7.572], [0.95, 0, 7.572]]}"),
			I ? TEXT(",") : TEXT(""), Pl.Piece, Pl.P.X, Pl.P.Y, Pl.Z, FRotator::NormalizeAxis(Pl.YawDeg));
	}
	S += TEXT("\n ],\n \"wires\": [");
	for (int32 I = 0; I < GVillageWires.Num(); ++I)
	{
		const FVillageWire& W = GVillageWires[I];
		const FString B = W.PoleB != INDEX_NONE ? FString::Printf(TEXT("\"b\": [%d, %d]"), W.PoleB, W.InsB)
			: FString::Printf(TEXT("\"b_wall\": [%.2f, %.2f, %.2f], \"b_piece\": %d"), W.B.X, W.B.Y, W.B.Z, W.PieceB);
		S += FString::Printf(TEXT("%s\n  {\"a\": [%d, %d], %s, \"sag_m\": %.2f}"), I ? TEXT(",") : TEXT(""), W.PoleA, W.InsA, *B, W.Sag);
	}
	S += TEXT("\n ],\n \"palms\": [");
	for (int32 I = 0; I < GVillagePalms.Num(); ++I)
	{
		S += FString::Printf(TEXT("%s[%.2f, %.2f]"), I ? TEXT(", ") : TEXT(""), GVillagePalms[I].X, GVillagePalms[I].Y);
	}
	// os depósitos de supply do mapa (13/10, combinado com o jogo: os dois leem o mesmo; o SupplyDock do ZH,
	// 26,33 × 32,12 m em X × Y, 400 caixas, sem giro). owner = a largada dona (−1 disputado)
	S += TEXT("],\n \"supply_docks\": [");
	if (GMap.IsValid())
	{
		for (int32 I = 0; I < GMap->Resources.Num(); ++I)
		{
			const FWorldResource& R = GMap->Resources[I];
			S += FString::Printf(TEXT("%s\n  {\"x\": %.1f, \"y\": %.1f, \"yaw_deg\": 0.0, \"size_m\": [26.33, 32.12], \"boxes\": 400, \"owner\": %d}"),
				I ? TEXT(",") : TEXT(""), R.Pos.X, R.Pos.Y, R.Owner);
		}
	}
	S += TEXT("\n ]\n}\n");
	return S;
}
