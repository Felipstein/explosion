#pragma once

#include "CoreMinimal.h"
#include "Algo/AnyOf.h"
#include "Async/ParallelFor.h"
#include "Math/Float16Color.h"
#include "Generation/WorldEdits.h"
#include "Generation/WorldMap.h"
#include "Generation/WorldNoise.h"
#include "Generation/WorldTrees.h"
#include "Generation/DuneSim.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

/**
 * O mundo "de verdade" a partir do mapa do gerador: a rede de estradas, as
 * máscaras (copa, anel das pedras, salina, mar, oásis), o tom e os pesos das
 * camadas do chão e a altura final (dunas, corte e aterro das estradas). Uma
 * por mapa (sem variáveis globais), pra que o "qualidade real" (editor) e a
 * vista ao vivo do app usem a mesma regra.
 * Os nomes com G na frente eram as variáveis globais do editor (WorldForgeEditorLib).
 */
struct FWorldDetail
{
	// o último recorte gerado: as outras funções (espalhamento, trilhas) leem dele
	TSharedPtr<FWorldMap> GMap;
	FVector2f GCenter = FVector2f::ZeroVector;   // centro do recorte no mapa gerado (m)
	float GHalf = 0.0f;                           // meio lado do recorte (m)

	FVector2f ToMap(FVector2D Cm) { return GCenter + FVector2f(Cm.X / 100.0, Cm.Y / 100.0); }

	bool GFrozen = false;   // inverno: o lago vira uma placa de gelo plana (camada "molhado" = gelo)

	// ---- vila (o kit civil do Look; pedido do jogo de 11/10) -----------------
	// Uma peça por casa, muro, poste ou miudeza, na grade de 4 m do jogo: o jogo
	// lê o JSON (WriteVillageJson) e monta a grade da simulação e o desenho com
	// as malhas do Look; o WorldForge aplaina o chão, tira o espalhamento de
	// baixo e desenha caixas provisórias na vista.
	struct FVillagePiece
	{
		FString Type;                          // civ_house_1story, civ_wall, civ_car_burnt...
		FVector2f Center = FVector2f::ZeroVector;   // centro da pegada (m)
		int32 Rot = 0;                         // 0..3: yaw = 90 × Rot (frente/porta em +Y no 0)
		FIntPoint Cell0 = FIntPoint::ZeroValue;     // canto da pegada na grade de 4 m (a partir de −Half)
		FIntPoint Cells = FIntPoint(1, 1);     // pegada no mundo, em células (já girada)
		FVector2f Size = FVector2f(4.0f);      // a malha no mundo (m, já girada)
		float Height = 4.0f;                   // altura da malha (m), pras caixas provisórias
		float Floor = 0.0f;                    // altura do piso (m): o chão embaixo vem aplainado nela
		int32 Street = -1;                     // lado virado pra rua (0..3, como Rot), −1 sem
		bool bBlocks = true;                   // vira célula bloqueada na simulação
		int32 Village = 0;                     // de qual vila (as cópias giradas são outras)
		// o giro livre (graus, yaw da Unreal) das miudezas e carcaças (13/10); NaN: 90 × Rot. O jogo
		// de hoje usa o Rot (o mais perto); quando ler o yaw_deg, a peça vira de verdade
		float YawDeg = TNumericLimits<float>::Lowest();
		bool HasYaw() const { return YawDeg > -1e30f; }
		bool bGroundFloor = false;             // fora dos lotes (carro, miudeza da rua, poste): o chão de verdade
		bool bRoof = false;                    // em cima da laje (o Floor é a laje; o JSON leva o z)
		// o terreno da peça (13/10, o Look: casa e muros do mesmo lote com o mesmo tom no jogo; só terrenos diferentes
		// variam): vila × 1000 + lote; −1 fora de lote (rua, carro, miudeza solta)
		int32 Lot = -1;
	};
	TArray<FVillagePiece> GVillage;
	/** O poste de luz (13/10; não bloqueia, o tanque derruba no jogo): a cruzeta no X local e o
	 *  fio correndo no Y local (SM_civ_pole, look_houses.py pole()); yaw livre: a cruzeta
	 *  perpendicular à linha, na bissetriz na esquina. */
	struct FVillagePole
	{
		FVector2f P = FVector2f::ZeroVector;
		float Z = 0.0f;          // o pé (m)
		float YawDeg = 0.0f;
		bool bCorner = false;    // na esquina (a linha vira): leva o estai
		int32 Piece = INDEX_NONE;   // a peça civ_pole (em GVillage: o jogo desenha e derruba por ela)
		int32 Road = INDEX_NONE;    // a estrada da linha (a ligação de casa não cruza outra)
	};
	TArray<FVillagePole> GVillagePoles;
	/** Os 4 isoladores do poste (m, local: X da cruzeta, Z da ponta do fio). */
	static FVector PoleInsulator(int32 I)
	{
		static const float X[4] = { -0.95f, -0.35f, 0.35f, 0.95f };
		return FVector(X[FMath::Clamp(I, 0, 3)], 0.0f, 7.572f);
	}
	FVector PoleSocket(int32 Pole, int32 I) const
	{
		const FVillagePole& Pl = GVillagePoles[Pole];
		const FVector L = FRotator(0.0f, Pl.YawDeg, 0.0f).RotateVector(PoleInsulator(I));
		return FVector(Pl.P.X + L.X, Pl.P.Y + L.Y, Pl.Z + L.Z);
	}
	/** Um fio (m, mundo): de um isolador a outro (ou à parede da casa), a flecha no meio do vão.
	 *  A curva é a catenária (≈ parábola com a flecha de ~3,5% do vão; o jogo monta e derruba). */
	struct FVillageWire
	{
		int32 PoleA = INDEX_NONE, InsA = 0;
		int32 PoleB = INDEX_NONE, InsB = 0;   // PoleB = INDEX_NONE: a ponta B é a parede de uma casa
		int32 PieceB = INDEX_NONE;            // a casa (índice em GVillage) da ponta B
		FVector A = FVector::ZeroVector, B = FVector::ZeroVector;
		float Sag = 1.0f;
	};
	TArray<FVillageWire> GVillageWires;
	/** A rua de cada cópia da vila (a estrada endireitada, o centro e o giro da planta local): a linha
	 *  dos postes vai depois, no fim do Setup, com a estrada já na cota (PlaceVillagePoles). */
	struct FVillageLine
	{
		int32 Road = INDEX_NONE;
		FVector2f C = FVector2f::ZeroVector;
		int32 Q = 0;
		float Len = 0.0f;
	};
	TArray<FVillageLine> GVillageLines;
	/** Os postes da vila (fora da pista, de toda peça, da porta, da palmeira e da árvore; a cruzeta
	 *  perpendicular à linha) e os fios em catenária, de poste a poste e até a parede das casas; o chão
	 *  das peças fora dos lotes. No fim do Setup (WorldVillage.cpp). */
	WORLDFORGE_API void PlaceVillagePoles(const FWorldMap& M);
	/** A grade de 8 m das pegadas das peças (OnVillagePiece). */
	void IndexVillagePieces()
	{
		GPieceGrid.Reset();
		for (int32 I = 0; I < GVillage.Num(); ++I)
		{
			const FVillagePiece& V = GVillage[I];
			const FVector2f H = 0.5f * V.Size + FVector2f(2.0f);
			for (int32 Y = FMath::FloorToInt((V.Center.Y - H.Y) / 8.0f); Y <= FMath::FloorToInt((V.Center.Y + H.Y) / 8.0f); ++Y)
				for (int32 X = FMath::FloorToInt((V.Center.X - H.X) / 8.0f); X <= FMath::FloorToInt((V.Center.X + H.X) / 8.0f); ++X)
					GPieceGrid.FindOrAdd(FIntPoint(X, Y)).Add(I);
		}
	}
	/** O chão aplainado: retângulo (m), cota e a borda de transição. */
	struct FVillagePad
	{
		FVector2f Min, Max;
		float Z = 0.0f;
		float Blend = 3.0f;
		bool bFlatten = true;   // falso: a rua da vila (só tira o espalhamento e pinta o chão)
	};
	TArray<FVillagePad> GPads;
	TMap<FIntPoint, TArray<int32>> GPadGrid;   // célula de 8 m → pads que tocam
	/** A distância (m) até o pad mais perto, numa grade de 2 m em volta da vila (0 dentro): o chão
	 *  pisado contínuo da vila (WorldVillage.cpp). Fora da grade, longe. */
	TArray<float> GVillageDist;
	FVector2f GVillageDistMin = FVector2f::ZeroVector;
	int32 GVillageDistW = 0, GVillageDistH = 0;
	float VillageDistAt(FVector2f P) const
	{
		if (GVillageDist.Num() == 0) return 1e9f;
		const float GX = (P.X - GVillageDistMin.X) / 2.0f, GY = (P.Y - GVillageDistMin.Y) / 2.0f;
		if (GX < 0.0f || GY < 0.0f || GX >= GVillageDistW - 1 || GY >= GVillageDistH - 1) return 1e9f;
		const int32 X0 = FMath::FloorToInt(GX), Y0 = FMath::FloorToInt(GY);
		const float FX = GX - X0, FY = GY - Y0;
		auto At = [&](int32 X, int32 Y) { return GVillageDist[Y * GVillageDistW + X]; };
		return FMath::Lerp(FMath::Lerp(At(X0, Y0), At(X0 + 1, Y0), FX), FMath::Lerp(At(X0, Y0 + 1), At(X0 + 1, Y0 + 1), FX), FY);
	}
	/** Palmeiras dos quintais (entram no sorteio das tamareiras). */
	TArray<FVector2f> GVillagePalms;
	/** Depuração: os lotes da 1ª vila (min x, min y, max x, max y) e por que saíram (0 = ficou). */
	TArray<FVector4f> GVillageLots;
	TArray<uint8> GVillageLotWhy;
	/** Monta a(s) vila(s) do mapa (M.Settings.Villages): lugar, rua reta, lotes,
	 *  casas, muros, miudezas, conferência dos caminhos e chão aplainado. Chamada
	 *  dentro do BuildRoads, antes da cota das estradas. */
	WORLDFORGE_API void BuildVillages(const FWorldMap& M, float MainHalf);
	/** As mini vilas em volta dos depósitos de supply (13/10, o Felipe: "pegar os lugares dos supplys e
	 *  adicionar mini vilas em volta também"): casas viradas pro depósito, atrás e nos lados dele, com o pátio
	 *  dos caminhões e o corredor até a base livres. Depois da vila do centro (WorldVillage.cpp). */
	WORLDFORGE_API void BuildHamlets(const FWorldMap& M);
	/** A grade de 8 m dos pads e a distância até o pad mais perto (o chão pisado). */
	void IndexVillagePads();
	/** O chão aplainado da vila em P (H = a altura de antes). */
	float PadHeight(FVector2f P, float H) const
	{
		if (GPads.Num() == 0) return H;
		const TArray<int32>* L = GPadGrid.Find(FIntPoint(FMath::FloorToInt(P.X / 8.0f), FMath::FloorToInt(P.Y / 8.0f)));
		if (!L) return H;
		for (const int32 I : *L)
		{
			const FVillagePad& Pd = GPads[I];
			if (!Pd.bFlatten) continue;
			const float Dx = FMath::Max3(Pd.Min.X - P.X, 0.0f, P.X - Pd.Max.X);
			const float Dy = FMath::Max3(Pd.Min.Y - P.Y, 0.0f, P.Y - Pd.Max.Y);
			const float D = FMath::Sqrt(Dx * Dx + Dy * Dy);
			if (D >= Pd.Blend) continue;
			H = FMath::Lerp(Pd.Z, H, FMath::SmoothStep(0.0f, Pd.Blend, D));
		}
		return H;
	}
	/** P cai numa peça da vila que tem parede (casa, muro, carro), com folga (m). */
	bool InVillage(FVector2f P, float Margin) const
	{
		if (GVillage.Num() == 0) return false;
		const TArray<int32>* L = GPadGrid.Find(FIntPoint(FMath::FloorToInt(P.X / 8.0f), FMath::FloorToInt(P.Y / 8.0f)));
		if (!L) return false;
		for (const int32 I : *L)
		{
			const FVillagePad& Pd = GPads[I];
			if (P.X > Pd.Min.X - Margin && P.X < Pd.Max.X + Margin && P.Y > Pd.Min.Y - Margin && P.Y < Pd.Max.Y + Margin) return true;
		}
		return false;
	}
	/** P cai na pegada de uma peça da vila (casa, muro, carro, miudeza), com folga (m). */
	TMap<FIntPoint, TArray<int32>> GPieceGrid;   // célula de 8 m → peças que tocam
	bool OnVillagePiece(FVector2f P, float Margin) const
	{
		if (GVillage.Num() == 0) return false;
		const TArray<int32>* L = GPieceGrid.Find(FIntPoint(FMath::FloorToInt(P.X / 8.0f), FMath::FloorToInt(P.Y / 8.0f)));
		if (!L) return false;
		for (const int32 I : *L)
		{
			const FVillagePiece& V = GVillage[I];
			const FVector2f H = 0.5f * V.Size + FVector2f(Margin);
			if (FMath::Abs(P.X - V.Center.X) < H.X && FMath::Abs(P.Y - V.Center.Y) < H.Y) return true;
		}
		return false;
	}
	/** O JSON que o jogo lê (uma linha por peça). */
	WORLDFORGE_API FString VillageJson(const FString& MapName) const;
	/** A impressão digital dos campos da simulação da vila (vai no JSON e no mapa montado). */
	WORLDFORGE_API uint32 VillageHash() const;

	// ---- rede de estradas ---------------------------------------------------
	// As trilhas do gerador viram estradas. A principal de cada jogador (base →
	// centro) é a larga: asfalto no deserto e na neve, terra na floresta. Os
	// flancos e as ligações são de terra, e cada campo de recurso longe da rede
	// ganha um ramal de terra até ela, pelo caminho mais barato no relevo
	// (Galin et al. 2010, "Procedural Generation of Roads"). Trecho que repete
	// uma estrada já feita é absorvido por ela (a hierarquia de Galin et al.
	// 2011), e toda ponta termina num destino (base, campo) ou num entroncamento:
	// sem toco de estrada no meio do nada.
	struct FRoad
	{
		TArray<FVector2f> P;     // linha central (m), um ponto por metro
		TArray<float> Z;         // cota do leito (corte e aterro)
		float Half = 0.0f;       // meia largura (m)
		// a meia largura em cada ponto (vazio: Half em todos). A rua da vila alarga a estrada
		// principal (13,5 m dentro, 10 m fora) com a transição de 24 m nas entradas (13/10)
		TArray<float> W;
		float HalfAt(int32 K) const { return W.Num() == P.Num() ? W[FMath::Clamp(K, 0, W.Num() - 1)] : Half; }
		float MaxHalf() const { float H = Half; for (const float X : W) H = FMath::Max(H, X); return H; }
		bool bPaved = false;     // asfalto (pintura, mureta) ou terra
		bool bOpen[2] = { false, false };   // ponta dentro de base/campo: o asfalto se desfaz
		int32 Host[2] = { INDEX_NONE, INDEX_NONE };   // estrada onde cada ponta entronca
		// em quantos metros a ponta solta se desfaz: 45 na do gerador (entra na
		// base); 4 na desenhada à mão, que termina onde o jogador parou (a de
		// ~40 m sumia inteira nos buracos, 07/10)
		float Fade[2] = { 45.0f, 45.0f };
		int32 Style = 0;         // o acabamento do asfalto escolhido no painel (FWorldEdits::FRoadEdit::Style): 0 automático, 1 colada no chão, 2 inteira, 3 gasta, 4 destruída
		int32 Dirt = 0;          // a sujeira na beira (FWorldEdits::FRoadEdit::Dirt): 0 automática, 1 nenhuma, 2 pouca, 3 média, 4 muita
	};
	TArray<FRoad> GRoads;
	int32 NumGenRoads = 0;            // as primeiras são do gerador; depois, as desenhadas à mão
	TArray<int32> UserRoadIndex;      // FWorldEdits::Roads[i] → índice em GRoads (INDEX_NONE: ficou de fora)
	// grade de 8 m com os trechos de estrada perto de cada célula: o mapa inteiro
	// tem milhões de vértices e as estradas, milhares de trechos
	static constexpr float RoadCell = 8.0f;
	TMap<FIntPoint, TArray<FIntPoint>> GRoadGrid;   // célula → (estrada, trecho)
	float GRoadReach = 0.0f;   // além disto a distância não importa

	FIntPoint RoadCellOf(FVector2f P) { return FIntPoint(FMath::FloorToInt(P.X / RoadCell), FMath::FloorToInt(P.Y / RoadCell)); }

	struct FRoadHit
	{
		float D = 1e9f;          // distância à linha central (m)
		float Z = 0.0f;          // cota do leito no ponto mais perto
		float End = 1e9f;        // metros até a ponta solta mais perto (1e9: não tem)
		float EndT = 1e9f;       // a mesma distância na escala do desfazer da ponta (0 na ponta, 1 no asfalto inteiro)
		int32 Road = INDEX_NONE;
		float HalfWidth = 0.0f;  // meia largura e tipo da estrada achada (copiados: a
		bool bPavedRoad = false; // batida não enxerga a lista de estradas)
		FVector2f Dir = FVector2f(1.0f, 0.0f);   // a direção da pista no ponto (o trecho mais perto)
		FVector2f Closest = FVector2f::ZeroVector;   // o ponto do eixo mais perto
		float S = 0.0f;          // metros ao longo da estrada até o ponto mais perto (a fase do tracejado)
		float Half() const { return Road != INDEX_NONE ? HalfWidth : 0.0f; }
		bool Paved() const { return Road != INDEX_NONE && bPavedRoad; }
	};

	/** O corte da estrada no ponto (13/10, o Felipe: "mais econômico ir por baixo das montanhas" do que pela estrada
	 *  entre o lago e o maciço): o leito plano até Flat (a pista e o acostamento) e o talude de Flat a Flat + Bank.
	 *  O talude estica com o desnível medido 10 m além do acostamento, do lado do ponto: a média fica em 0,5 (~27°)
	 *  e o meio da rampa (a smoothstep, 1,5×) abaixo do limite de penhasco do jogo (Maps.json max_slope 0,78). O
	 *  acostamento plano de 4 m no asfalto dá a passagem: no corte da encosta só a pista era plana (~11 m) e o jogo,
	 *  com a célula de folga em volta do tanque, não passava por ali. */
	void RoadCut(const FWorldMap& M, FVector2f P, const FRoadHit& Hit, float& Flat, float& Bank, float& Drop) const
	{
		Flat = Hit.Half() + (Hit.Paved() ? 4.8f : 2.0f);
		FVector2f Out = P - Hit.Closest;
		Out = Out.SizeSquared() > 1e-4f ? Out.GetSafeNormal() : FVector2f(-Hit.Dir.Y, Hit.Dir.X);
		Drop = FMath::Abs(M.HeightAt(Hit.Closest + Out * (Flat + 10.0f)) - Hit.Z);
		Bank = FMath::Clamp(Drop / 0.5f, Hit.Paved() ? 6.2f : 3.2f, 26.0f);
	}

	// ---- a estrada com volume (14/10, v3: a laje do Look, RECEITAS_DO_LOOK §6, Game/Tools/look_ground.py road_slab) ----
	// O asfalto vira uma laje de 12 cm em cima do chão (uma malha Nanite por ladrilho, BuildRoadSlabs no editor), com a
	// borda quebrada em placas onde a estrada é gasta ou destruída. O chão (o Landscape) faz o resto: a areia em volta
	// 6 cm abaixo do asfalto (a maquete: base −0,06), os montes de areia encostados na beira em trechos (o próprio chão
	// levantado, que cobre a laje onde passa do topo dela) e, escondido debaixo do miolo da laje, o chão levantado até
	// 1,5 cm do topo: o jogo tira a altura das unidades do Landscape a cada 2 m (RTSMapBaker), e assim o tanque anda em
	// cima da laje sem mudar nada no jogo. GSlab: ROAD_SLAB=1 (o mapa de teste; depois, uma escolha do mapa).
	bool GSlab = false;
	/** A estrada do ponto tem laje (a laje no mapa e a estrada de asfalto que não pediu "colada no chão"). */
	bool SlabOn(const FRoadHit& Hit) const { return GSlab && Hit.Paved() && GRoads[Hit.Road].Style != 1; }
	static constexpr float SlabThick = 0.12f;      // o topo acima do asfalto (look_layout ROAD_SLAB thick: 8 → 12 cm, "o degrau lia pouco")
	static constexpr float SlabCrown = 0.025f;     // o abaulado no meio (smoothstep de 0 a 4 m da beira)
	static constexpr float SlabSandDrop = 0.06f;   // a areia em volta, abaixo do asfalto

	// ---- os bolsões de dunas (16/10, v3; o estudo com fontes no WORLDFORGE.md §12.7) ----
	// Com o sol a 8° a duna aparece pela inclinação, não pela altura: barcanas baixas (0,8–2 m) no deserto aberto, longe
	// do que se joga (40–100 m das bases, 25–60 m dos depósitos, fora do pisado: OpenAt). O perfil do estudo: barlavento
	// em meia-senoide (máximo de 14°, comprimento 6,3× a altura), face de deslizamento a 31°, crista arredondada, chifres
	// a favor do vento; a altura pela largura das barcanas do Marrocos, H ≈ (1/15)·√(L² − 10²). O lugar e o tamanho seguem
	// a simetria do mapa (o giro de 180° ou 90°); a forma sai sempre do vento do mapa (girar a duna inverteria a face).
	// DUNE_POCKETS=1 (o mapa de teste).
	// (16/10 à tarde, o Felipe sobre as barcanas por fórmula, duas vezes: "muito padronizada e única" e "ainda tá muito
	// padronizado, não tem nada natural aí") o passo atrás: a areia simulada (DuneSim.h, o modelo de lajes de Werner
	// 1995 / Paris 2019). Um ladrilho periódico de 512 m com o vento em +x, a areia esparsa em manchas; as dunas nascem,
	// crescem, se juntam e soltam filhotes sozinhas, cada uma diferente. O acabamento alisa os degraus da laje, achata
	// pela metade (a barcana da simulação sai com ~1/7 da largura de altura; as reais têm 1/10–1/15: a proporção fica a
	// real e, com o sol a 8°, a face de 18° continua inteira na sombra) e escorre acima de 30°. No mapa o ladrilho gira
	// com o vento do mapa e cada duna entra inteira, só onde o contorno todo cabe no DuneAllowed (nada cortado na borda),
	// sem encostar na outra. Os lados não recebem a mesma duna copiada (girada, a duna e o filhote a jusante dela
	// inverteriam o vento): a justiça é a quantidade de duna por lado (o log mede), e a duna não bloqueia ninguém.
	bool GDunes = false;
	static constexpr int32 DuneTileN = 1024;       // o ladrilho (1024 células de 0,5 m: ~40 formas diferentes, cada uma entra no mapa uma vez)
	// (16/10, a 1ª foto no jogo: achatada a 0,45, a face caía pra ~17° e com a luz do jogo não escurecia; a duna sumia)
	// a duna inteira em meia escala, largura e altura juntas: a face fica a 30° e a crista nos mesmos ~1–2,5 m. A
	// simulação não muda (laje/célula = 0,1 nos dois casos)
	static constexpr float DuneTileCell = 0.5f;
	TArray<float> GDuneH;                          // a altura acabada do ladrilho (m)
	std::vector<int> GDuneLab, GDuneNear;          // a duna de cada célula e a mais perto até 8 m (o pé)
	std::vector<float> GDuneNearDist;              // a distância (m) até ela
	std::vector<DuneSim::FDune> GDuneComps;        // as dunas do ladrilho
	struct FDuneInst { int32 Comp = 0; FVector2f A = FVector2f::ZeroVector; float R = 0.0f; bool bFlip = false; };   // a duna Comp com o centro em A (m), espelhada de lado

	TArray<FDuneInst> GDuneInst;
	TMap<FIntPoint, TArray<int32>> GDuneGrid;      // célula de 32 m → as dunas que alcançam
	FVector2f GDuneWd = FVector2f(1.0f, 0.0f), GDuneWp = FVector2f(0.0f, 1.0f), GDuneOff = FVector2f::ZeroVector;

	/** O quanto o ponto aceita duna (0..1): areia aberta, longe das bases, dos depósitos e do pisado. */
	float DuneAllowed(const FWorldMap& M, FVector2f P) const
	{
		float A = OpenAt(P);
		if (A <= 0.0f) return 0.0f;
		A *= 1.0f - FMath::SmoothStep(0.0f, 0.08f, M.FormAt(P));   // (16/10) nada de duna em cima dos morros e da mesa
		for (const FWorldStart& St : M.Starts) A *= FMath::SmoothStep(M.BaseRadius + 40.0f, M.BaseRadius + 100.0f, FVector2f::Distance(P, St.Pos));
		for (const FWorldResource& R : M.Resources) A *= FMath::SmoothStep(R.Radius + 25.0f, R.Radius + 60.0f, FVector2f::Distance(P, R.Pos));
		if (M.WaterLevel > -100.0f) A *= FMath::SmoothStep(M.WaterLevel + 1.0f, M.WaterLevel + 3.0f, M.HeightAt(P));
		// (16/10, a 1ª montagem: uma barcana caiu em cima do maciço) só no chão plano: a encosta medida em 8 m, até ~3°
		const float Gx = M.HeightAt(P + FVector2f(8.0f, 0.0f)) - M.HeightAt(P - FVector2f(8.0f, 0.0f));
		const float Gy = M.HeightAt(P + FVector2f(0.0f, 8.0f)) - M.HeightAt(P - FVector2f(0.0f, 8.0f));
		// (16/10: com os morros e descidas o chão aberto ondula até ~10°; a duna senta na ondulação, só a encosta forte recusa)
		A *= 1.0f - FMath::SmoothStep(GMap.IsValid() && GMap->Settings.Hills > 0.01f ? 0.12f : 0.035f, GMap.IsValid() && GMap->Settings.Hills > 0.01f ? 0.22f : 0.14f, FMath::Sqrt(Gx * Gx + Gy * Gy) / 16.0f);   // (fade largo: estreito, cortava a duna em 2–4 m e passava de 45°)
		return A;
	}

	/** O ladrilho de areia simulada (o arquivo guardado por semente: a simulação leva ~1 min). */
	void BuildDuneTile(const FWorldMap& M)
	{
		const uint32 Seed = 0xD0E5u + uint32(M.Settings.Seed);
		const FString Path = FPaths::ProjectSavedDir() / TEXT("WorldForge/dunes") / FString::Printf(TEXT("tile_%u_%d_v3.bin"), Seed, DuneTileN);
		const int32 N = DuneTileN;
		TArray<uint8> Bytes;
		if (FFileHelper::LoadFileToArray(Bytes, *Path) && Bytes.Num() == N * N * int32(sizeof(float)))
		{
			GDuneH.SetNumUninitialized(N * N);
			FMemory::Memcpy(GDuneH.GetData(), Bytes.GetData(), Bytes.Num());
			UE_LOG(LogTemp, Display, TEXT("FORGE dunes: ladrilho guardado %s"), *Path);
		}
		else
		{
			const double T0 = FPlatformTime::Seconds();
			DuneSim::FParams P;
			P.N = N;
			P.Cell = DuneTileCell;
			P.Seed = Seed;   // (o resto: o modo conservado do DuneSim, o da bancada de 16/10, Tools/dunesim)
			P.Cell = DuneTileCell;
			P.Slab = 0.1f * DuneTileCell;
			DuneSim::FState S;
			S.Init(P);
			DuneSim::SeedSandMean(S, 2.0f);   // 2 lajes (20 cm) em média: com 1–1,6 a areia fica em lençol e não vira duna
			S.Run(uint64_t(N) * N * 2000);
			// sem achatar: a meia escala já põe a crista em ~1–2,5 m com a face a 30°
			const std::vector<float> Hf = DuneSim::Finish(S, 1.0f, 1.0f, 30.0f);
			GDuneH.SetNumUninitialized(N * N);
			FMemory::Memcpy(GDuneH.GetData(), Hf.data(), Hf.size() * sizeof(float));
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
			FFileHelper::SaveArrayToFile(TArrayView<const uint8>(reinterpret_cast<const uint8*>(GDuneH.GetData()), N * N * int32(sizeof(float))), *Path);
			UE_LOG(LogTemp, Display, TEXT("FORGE dunes: ladrilho simulado em %.0f s -> %s"), FPlatformTime::Seconds() - T0, *Path);
		}
		std::vector<float> Hv(GDuneH.GetData(), GDuneH.GetData() + GDuneH.Num());
		// o pé até 9 m (em células: na meia escala, 8 células eram 4 m e a pintura cortava ali numa moldura em volta da duna)
		GDuneComps = DuneSim::Label(Hv, N, 0.05f, 60, FMath::CeilToInt(9.0f / DuneTileCell), GDuneLab, GDuneNear, GDuneNearDist);
	}

	/** A altura do ladrilho no ponto (células, periódico), só das células da duna Comp (bilinear com as de fora em zero). */
	float DuneTileSample(float Tx, float Ty, int32 Comp, const std::vector<int>& Lab, const TArray<float>* H) const
	{
		const int32 N = DuneTileN, Mk = N - 1;
		const float Fx = Tx - 0.5f, Fy = Ty - 0.5f;
		const int32 X0 = FMath::FloorToInt(Fx), Y0 = FMath::FloorToInt(Fy);
		const float Ax = Fx - X0, Ay = Fy - Y0;
		auto At = [&](int32 X, int32 Y)
		{
			const int32 I = (Y & Mk) * N + (X & Mk);
			return Lab[I] == Comp ? (H ? (*H)[I] : 1.0f) : 0.0f;
		};
		return FMath::Lerp(FMath::Lerp(At(X0, Y0), At(X0 + 1, Y0), Ax), FMath::Lerp(At(X0, Y0 + 1), At(X0 + 1, Y0 + 1), Ax), Ay);
	}

	/** O ponto do mapa no ladrilho (células): o ladrilho gira com o vento e anda GDuneOff (a semente). */
	FVector2f DuneTileAt(FVector2f P) const
	{
		return FVector2f((P | GDuneWd), (P | GDuneWp)) / DuneTileCell + GDuneOff;
	}

	void BuildDunePockets(const FWorldMap& M)
	{
		GDuneInst.Reset();
		GDuneGrid.Reset();
		if (!GDunes) return;
		BuildDuneTile(M);
		const float Half = M.Half();
		const int32 K = M.Settings.Players == 4 ? 4 : 2;
		GDuneWd = FVector2f(FMath::Cos(FMath::DegreesToRadians(M.WindYaw)), FMath::Sin(FMath::DegreesToRadians(M.WindYaw)));
		GDuneWp = FVector2f(-GDuneWd.Y, GDuneWd.X);
		// (16/10 à tarde: com o ladrilho preso no mapa só 2 dunas cabiam inteiras nos bolsões) o ladrilho vira a biblioteca
		// de formas: cada duna dele, como nasceu ou espelhada de lado (o vento continua o mesmo), num ponto sorteado do
		// bolsão, com as vizinhas dela no ladrilho a até 25 m (os filhotes, as que encostam) no mesmo arranjo. Para em ~10%
		// do chão onde pode (a pesquisa: o campo esparso cobre 8–12%), os dois lados com a mesma área de duna (±10%)
		const int32 ND = int32(GDuneComps.size());
		TArray<TArray<TPair<int32, FVector2f>>> Groups;
		Groups.SetNum(ND);
		for (int32 A = 0; A < ND; ++A)
			for (int32 B = 0; B < ND; ++B)
			{
				if (A == B) continue;
				FVector2f D(GDuneComps[B].Cx - GDuneComps[A].Cx, GDuneComps[B].Cy - GDuneComps[A].Cy);
				D.X -= DuneTileN * FMath::RoundToFloat(D.X / DuneTileN);   // a menor distância no ladrilho periódico
				D.Y -= DuneTileN * FMath::RoundToFloat(D.Y / DuneTileN);
				if (D.Size() < GDuneComps[A].R + GDuneComps[B].R + 25.0f) Groups[A].Add({ B, D });
			}
		float Open = 0.0f;
		for (float Y = -Half + 4.0f; Y < Half; Y += 8.0f)
			for (float X = -Half + 4.0f; X < Half; X += 8.0f)
				Open += DuneAllowed(M, FVector2f(X, Y)) >= 0.85f ? 64.0f : 0.0f;
		const float Target = 0.10f * Open;
		// (16/10: o montinho de 26 cm cabia em todo canto e entrou 16 vezes, o carimbo de novo) só duna de verdade (a crista
		// a 60 cm ou mais) e cada forma uma vez só, como nasceu ou espelhada
		TArray<uint8> Uses;
		Uses.Init(0, ND);
		TArray<FVector2f> FirstAt;
		FirstAt.Init(FVector2f(1e9f, 1e9f), ND);
		// (normal e espelhada perto liam como gêmeas) e a crista até 2,6 m (16/10: a de 4,4 m passava do que o jogo aguenta:
		// a sombra a 8° de sol cobre as unidades)
		// (16/10, meia escala: só ~24 formas cabiam, 4% do chão) a forma volta uma segunda vez, espelhada e a 200 m ou mais da
		// primeira: ninguém vê as duas juntas
		auto Usable = [&](int32 C, bool bFlip, FVector2f At)
		{
			if (GDuneComps[C].HMax < 0.6f || GDuneComps[C].HMax > 2.6f) return false;
			if (Uses[C] == 0) return true;
			return Uses[C] != 3 && !(Uses[C] & (bFlip ? 2 : 1)) && FVector2f::Distance(FirstAt[C], At) > 200.0f;
		};
		auto Use = [&](int32 C, bool bFlip, FVector2f At)
		{
			if (Uses[C] == 0) FirstAt[C] = At;
			Uses[C] |= bFlip ? 2 : 1;
		};
		float CoverSide[2] = { 0.0f, 0.0f };
		int32 NSide[2] = { 0, 0 };
		// o lado do mapa (o primeiro jogador do lado de x negativo no giro de 180°): a medida da justiça
		auto SideOf = [K](FVector2f A) { return (K == 4 ? (A.X < 0.0f) : (A.X + 0.0001f * A.Y < 0.0f)) ? 0 : 1; };
		auto ToMap = [this](float Tx, float Ty, bool bFlip) { return (GDuneWd * Tx + GDuneWp * (bFlip ? -Ty : Ty)) * DuneTileCell; };
		auto Fits = [&](int32 C, FVector2f A, bool bFlip)
		{
			const DuneSim::FDune& D = GDuneComps[C];
			const float R = D.R * DuneTileCell;
			for (const FDuneInst& O : GDuneInst)
				if (FVector2f::Distance(O.A, A) < O.R + R + 3.0f) return false;
			for (size_t Q = 0; Q < D.ProbeX.size(); ++Q)
			{
				const FVector2f P = A + ToMap(float(D.ProbeX[Q]), float(D.ProbeY[Q]), bFlip);
				if (FMath::Abs(P.X) > Half - 4.0f || FMath::Abs(P.Y) > Half - 4.0f || DuneAllowed(M, P) < 0.85f) return false;
			}
			return true;
		};
		auto Place = [&](int32 C, FVector2f A, bool bFlip)
		{
			const float R = GDuneComps[C].R * DuneTileCell;
			const int32 Idx = GDuneInst.Add({ C, A, R, bFlip });
			const int32 Side = SideOf(A);
			CoverSide[Side] += GDuneComps[C].Cells * DuneTileCell * DuneTileCell;
			NSide[Side]++;
			const float Reach = R + 8.0f;
			for (int32 Y = FMath::FloorToInt((A.Y - Reach) / 32.0f); Y <= FMath::FloorToInt((A.Y + Reach) / 32.0f); ++Y)
				for (int32 X = FMath::FloorToInt((A.X - Reach) / 32.0f); X <= FMath::FloorToInt((A.X + Reach) / 32.0f); ++X)
					GDuneGrid.FindOrAdd(FIntPoint(X, Y)).Add(Idx);
		};
		const uint32 Sd = 0xD0E5u + uint32(M.Settings.Seed);
		for (int32 I = 0; I < 40000 && ND > 0 && CoverSide[0] + CoverSide[1] < Target; ++I)
		{
			const uint32 H1 = WorldNoise::Hash(I, 1, Sd);
			const FVector2f P((2.0f * WorldNoise::Unit(H1) - 1.0f) * Half, (2.0f * WorldNoise::Unit(WorldNoise::Hash(H1)) - 1.0f) * Half);
			if (DuneAllowed(M, P) < 0.85f) continue;
			const int32 Side = SideOf(P);
			if (CoverSide[Side] > CoverSide[1 - Side] + 0.06f * Target) continue;   // o lado com menos duna escolhe primeiro
			const int32 C = FMath::Min(int32(WorldNoise::Unit(WorldNoise::Hash(H1 + 7u)) * ND), ND - 1);
			const bool bFlip = (WorldNoise::Hash(H1 + 11u) & 256u) != 0;
			if (!Usable(C, bFlip, P) || !Fits(C, P, bFlip)) continue;
			Place(C, P, bFlip);
			Use(C, bFlip, P);
			for (const TPair<int32, FVector2f>& G : Groups[C])
			{
				const FVector2f A = P + ToMap(G.Value.X, G.Value.Y, bFlip);
				if (Usable(G.Key, bFlip, A) && Fits(G.Key, A, bFlip))
				{
					Place(G.Key, A, bFlip);
					Use(G.Key, bFlip, A);
				}
			}
		}
		UE_LOG(LogTemp, Display, TEXT("FORGE dunes: %d dunas no ladrilho, %d no mapa (lados %d/%d, %.0f/%.0f m2); ~%.0f%% do chão onde pode (%.0f%% do mapa)"),
			ND, GDuneInst.Num(), NSide[0], NSide[1], CoverSide[0], CoverSide[1],
			100.0f * (CoverSide[0] + CoverSide[1]) / FMath::Max(Open, 1.0f), 100.0f * Open / FMath::Square(2.0f * Half));
		for (const FDuneInst& D : GDuneInst)
			UE_LOG(LogTemp, Display, TEXT("FORGE duna %d em %.0f %.0f raio %.0f altura %.2f%s"), D.Comp, D.A.X, D.A.Y, D.R, GDuneComps[D.Comp].HMax, D.bFlip ? TEXT(" espelhada") : TEXT(""));
	}

	/** O quanto o chão é areia da duna (0..1): o corpo e o pé, caindo de 1 a 8 m fora dela (a pintura da camada; o relevo
	 *  da areia de base enterra por ela, devagar: trocar em 1 m levantava ~1 m de marola de uma vez). */
	float DunePaintAt(const FWorldMap& M, FVector2f P) const
	{
		const TArray<int32>* L = GDuneGrid.Find(FIntPoint(FMath::FloorToInt(P.X / 32.0f), FMath::FloorToInt(P.Y / 32.0f)));
		if (!L) return 0.0f;
		float Best = 0.0f;
		for (const int32 I : *L)
		{
			const FDuneInst& D = GDuneInst[I];
			const FVector2f Local = P - D.A;
			if (Local.SizeSquared() > FMath::Square(D.R + 6.0f)) continue;
			const DuneSim::FDune& C = GDuneComps[D.Comp];
			const FVector2f T = FVector2f(C.Cx, C.Cy) + FVector2f(Local | GDuneWd, (D.bFlip ? -1.0f : 1.0f) * (Local | GDuneWp)) / DuneTileCell;
			const int32 Mk = DuneTileN - 1;
			const int32 Ci = (FMath::FloorToInt(T.Y) & Mk) * DuneTileN + (FMath::FloorToInt(T.X) & Mk);
			if (GDuneNear[Ci] != D.Comp) continue;
			Best = FMath::Max(Best, 1.0f - FMath::SmoothStep(1.0f, 8.0f, GDuneNearDist[Ci] * DuneTileCell));
		}
		return Best;
	}

	/** A altura das dunas no ponto (m). */
	float DuneAt(const FWorldMap& M, FVector2f P) const
	{
		const float Raw = DuneRaw(M, P);
		return Raw > 0.0f ? Raw * DuneAllowed(M, P) : 0.0f;
	}

	/** A altura das dunas antes do DuneAllowed (a medida separa o corte da borda); NHit = quantas dunas no ponto. */
	float DuneRaw(const FWorldMap& M, FVector2f P, int32* NHit = nullptr) const
	{
		if (NHit) *NHit = 0;
		const TArray<int32>* L = GDuneGrid.Find(FIntPoint(FMath::FloorToInt(P.X / 32.0f), FMath::FloorToInt(P.Y / 32.0f)));
		if (!L) return 0.0f;
		float Sum = 0.0f;
		for (const int32 I : *L)
		{
			const FDuneInst& D = GDuneInst[I];
			const FVector2f Local = P - D.A;
			if (Local.SizeSquared() > FMath::Square(D.R + 2.0f)) continue;
			const DuneSim::FDune& C = GDuneComps[D.Comp];
			const FVector2f T = FVector2f(C.Cx, C.Cy) + FVector2f(Local | GDuneWd, (D.bFlip ? -1.0f : 1.0f) * (Local | GDuneWp)) / DuneTileCell;
			const float Z = DuneTileSample(T.X, T.Y, D.Comp, GDuneLab, &GDuneH);
			if (Z > 0.0f)
			{
				Sum += Z;
				if (NHit) ++*NHit;
			}
		}
		return Sum;
	}

	// ---- a areia encostada nas paredes (14/10, v3) ----
	// A maquete do Look tem (look_ground.fields: pile = 0,35·exp(−d/0,9)·(0,4 + 0,6·ruído de 2 m)) e o r1_raid_slabs também:
	// a areia batida contra as paredes, com a crista. No mapa o chão em volta das casas era plano. Aqui o chão levanta em
	// volta das casas e dos muros da vila, mais do lado de onde vem o vento (barlavento: a areia para na parede) e menos
	// no de sotavento (o redemoinho), em trechos; pintado de areia. WALL_SAND=1 (o mapa de teste)
	bool GWallSand = false;
	// (16/10, a 1ª foto: os 35 cm da maquete sumiam na câmera do jogo, a 67–100 m; o r1_raid_slabs tem montes de
	// ~0,7–1 m com rampa de ~2 m) WALL_SAND=1 a da maquete (0,35 m, 0,9 m); WALL_SAND=2 a forte (0,75 m, 1,8 m): o A/B
	float GWallSandPeak = 0.35f, GWallSandFall = 0.9f;
	TMap<FIntPoint, TArray<int32>> GWallGrid;   // célula de 8 m → peças da vila (casas e muros) a até 3 m

	void IndexWallSand()
	{
		GWallGrid.Reset();
		if (!GWallSand) return;
		for (int32 I = 0; I < GVillage.Num(); ++I)
		{
			const FVillagePiece& Pc = GVillage[I];
			if (!(Pc.Type.StartsWith(TEXT("civ_house")) || Pc.Type.StartsWith(TEXT("civ_wall"))) || Pc.Height < 1.0f) continue;
			const float R = 3.5f * GWallSandFall;
			const FVector2f Lo = Pc.Center - 0.5f * Pc.Size - FVector2f(R), Hi = Pc.Center + 0.5f * Pc.Size + FVector2f(R);
			for (int32 Y = FMath::FloorToInt(Lo.Y / 8.0f); Y <= FMath::FloorToInt(Hi.Y / 8.0f); ++Y)
				for (int32 X = FMath::FloorToInt(Lo.X / 8.0f); X <= FMath::FloorToInt(Hi.X / 8.0f); ++X)
					GWallGrid.FindOrAdd(FIntPoint(X, Y)).Add(I);
		}
	}

	/** O monte de areia encostado nas paredes no ponto (m); 0 dentro da pegada e longe. */
	float WallSandAt(const FWorldMap& M, FVector2f P) const
	{
		const TArray<int32>* L = GWallGrid.Find(FIntPoint(FMath::FloorToInt(P.X / 8.0f), FMath::FloorToInt(P.Y / 8.0f)));
		if (!L) return 0.0f;
		const FVector2f Wd(FMath::Cos(FMath::DegreesToRadians(M.WindYaw)), FMath::Sin(FMath::DegreesToRadians(M.WindYaw)));
		float Best = 0.0f;
		for (const int32 I : *L)
		{
			const FVillagePiece& Pc = GVillage[I];
			const FVector2f D = P - Pc.Center;
			const float Dx = FMath::Abs(D.X) - 0.5f * Pc.Size.X, Dy = FMath::Abs(D.Y) - 0.5f * Pc.Size.Y;
			if (Dx <= 0.0f && Dy <= 0.0f) return 0.0f;   // dentro de uma pegada: o piso
			const float Out = FMath::Sqrt(FMath::Square(FMath::Max(Dx, 0.0f)) + FMath::Square(FMath::Max(Dy, 0.0f)));
			if (Out > 3.5f * GWallSandFall) continue;
			// a face mais perto e a normal dela pra fora; o vento (M.WindYaw: pra onde sopra) bate na face que olha contra ele
			const FVector2f N = Dx > Dy ? FVector2f(FMath::Sign(D.X), 0.0f) : FVector2f(0.0f, FMath::Sign(D.Y));
			const float Wind = FMath::Lerp(0.55f, 1.25f, FMath::Clamp(0.5f - 0.5f * (N | Wd), 0.0f, 1.0f));
			const float Big = Pc.Type.StartsWith(TEXT("civ_house")) ? 1.0f : 0.6f;   // o muro baixo junta menos
			Best = FMath::Max(Best, GWallSandPeak * Big * Wind * FMath::Exp(-Out / GWallSandFall));
		}
		return Best * (0.4f + 0.6f * FMath::Clamp(0.5f + 0.9f * WorldNoise::Fbm(P.X / 2.0f, P.Y / 2.0f, 955u, 2), 0.0f, 1.0f));
	}

	/** A borda do asfalto no ponto (a mesma conta do DesertWeights): In = metros até a beira (< 0 dentro), com a beira
	 *  quebrada (EdgeP), os buracos (Holes) e a ponta que se desfaz (Ends), e a esquina arredondada com a outra pista. */
	void PavedEdge(FVector2f P, const FRoadHit& Hit, float& In, float& EdgeP, float& Holes, float& Ends)
	{
		const float Half = Hit.Half();
		Ends = FMath::SmoothStep(0.0f, 1.0f, Hit.EndT);
		Holes = FMath::SmoothStep(0.42f - 0.5f * (1.0f - Ends), 0.62f - 0.5f * (1.0f - Ends), WorldNoise::Fbm(P.X / 4.0f, P.Y / 4.0f, 923u, 4));
		const float Narrow = Half * FMath::Lerp(0.7f, 1.0f, Ends);
		// a beira quebrada da maquete do Look (13/10): ±0,7 m, pedaços de ~0,8 a 2 m
		EdgeP = 0.5f * WorldNoise::Fbm(P.X / 2.0f, P.Y / 2.0f, 921u, 3) + 0.2f * WorldNoise::Fbm(P.X / 0.8f, P.Y / 0.8f, 927u, 2);
		In = Hit.D + EdgeP - Narrow;
		const FRoadHit Other = RoadAt(P, Hit.Road, true);
		if (Other.Road != INDEX_NONE && Other.D < Other.Half() + 8.0f)
		{
			const float B = Other.D + EdgeP - Other.Half() * FMath::Lerp(0.7f, 1.0f, FMath::SmoothStep(0.0f, 1.0f, Other.EndT));
			constexpr float Kr = 4.0f;
			const float Hh = FMath::Clamp(0.5f + 0.5f * (B - In) / Kr, 0.0f, 1.0f);
			In = FMath::Lerp(B, In, Hh) - Kr * Hh * (1.0f - Hh);
		}
	}

	struct FSlabSample
	{
		bool bPaved = false;
		float In = 1e9f;       // metros até a beira (< 0 dentro)
		float A = 0.0f;        // o quanto é asfalto (o do DesertWeights sem as línguas de areia: a laje faz a língua com o chão)
		float Drift = 0.0f;    // o monte de areia encostado na beira (m)
		float Wear = 0.0f;     // o desgaste: < 0,35 inteira, 0,35–0,7 gasta (quebra em trechos), > 0,7 destruída
		float Film = 0.0f;     // a sujeira por cima do asfalto (0–1, RoadDirtAt)
		float Patch = 0.0f;    // o trecho sujo (0–1): fora dele não há película
	};

	/** O quantil do Fbm (a fração Q dos valores fica abaixo dele): a sujeira da estrada acha o limiar pela fração da beira
	 *  pedida. A tabela sai uma vez, de 16 mil amostras (a estática do C++ é segura entre as threads). */
	static float FbmQuantile(float Q, int32 Octaves)
	{
		auto Make = [](int32 Oct)
		{
			TArray<float> T;
			T.Reserve(16384);
			for (int32 I = 0; I < 16384; ++I) T.Add(WorldNoise::Fbm((I % 128) * 1.618034f + 0.37f, (I / 128) * 1.414214f + 0.71f, 977u, Oct));
			T.Sort();
			return T;
		};
		static const TArray<float> T2 = Make(2), T3 = Make(3);
		const TArray<float>& T = Octaves <= 2 ? T2 : T3;
		return T[FMath::Clamp(int32(Q * (T.Num() - 1)), 0, T.Num() - 1)];
	}

	/** A sujeira na beira do asfalto (16/10, o Felipe: "achei bem exagerado a areia passando por cima da estrada, quase não
	 *  dá pra ver nenhuma borda [...] tinha que ser uma vez por outra" e "a gente 'suja' a estrada usando a textura que tá
	 *  ao lado dela"). A pesquisa (Wildlands, a spline do Landscape; o levantamento do Kuwait, ~10% da extensão numa estrada
	 *  moderada, 30% numa severa, 50% numa muito severa): a sujeira é uma máscara de quanto, em trechos ao longo da estrada,
	 *  e o material é o do chão do lado (SlabDirtSource). Pouca/Média/Muita = 10/28/50% da beira de cada lado; Auto pela
	 *  areia solta do lado (4–13%) e as dunas perto (até 45%), menos na vila. Entra mais pelo lado de onde vem o vento e
	 *  menos na estrada que corre com ele. Os trechos têm 25–60 m; neles a película entra 0,2–0,8 m na pista, e nas línguas
	 *  (as pontas dos trechos, ~1 em 10 m de trecho) até 2,5 m. Fora dos trechos, só o monte encostado de antes (aprovado
	 *  com a laje), abaixo do topo dela: a beira fica à vista. Film: 0–1 por cima do asfalto; Drift: o monte (m). */
	struct FRoadDirt { float Film = 0.0f; float Drift = 0.0f; float Patch = 0.0f; };
	FRoadDirt RoadDirtAt(const FWorldMap& M, FVector2f P, const FRoadHit& Hit, float In)
	{
		FRoadDirt R;
		if (Hit.Road == INDEX_NONE || !Hit.Paved())
		{
			return R;
		}
		const FRoad& Road = GRoads[Hit.Road];
		const FVector2f Off = P - Hit.Closest;
		const float Cross = Hit.Dir.X * Off.Y - Hit.Dir.Y * Off.X;
		const float SideSign = Cross >= 0.0f ? 1.0f : -1.0f;
		const FVector2f N = FVector2f(-Hit.Dir.Y, Hit.Dir.X) * SideSign;   // a normal pra fora, do lado do ponto
		const FVector2f Pn = Hit.Closest + N * (Hit.Half() + 3.0f);        // o chão do lado (passado o acostamento)
		const FVector2f Wd(FMath::Cos(FMath::DegreesToRadians(M.WindYaw)), FMath::Sin(FMath::DegreesToRadians(M.WindYaw)));
		float Wn[6];
		ScatterWeights(M, Pn, Wn);
		const float Sandy = FMath::Clamp(Wn[0] + Wn[4] + Wn[5], 0.0f, 1.0f);   // o que o vento carrega (areia, sal); rocha e terra não
		// o monte encostado de antes (aprovado com a laje): até 16 cm, abaixo do topo da laje (12 cm + os 6 da areia)
		R.Drift = 0.16f * Sandy * FMath::Exp(-FMath::Square((In - 0.5f) / 0.9f))
			* (0.55f + 0.45f * FMath::Clamp(0.5f + WorldNoise::Fbm(P.X / 5.0f, P.Y / 5.0f, 943u, 2), 0.0f, 1.0f));
		// a fração da beira suja deste lado
		float C;
		const int32 L = Road.Dirt;
		if (L == 1)
		{
			return R;
		}
		if (L >= 2)
		{
			C = L == 2 ? 0.15f : (L == 3 ? 0.30f : 0.50f);   // (16/10, a 2ª: 10/28/50; o Felipe achou rara demais)
		}
		else
		{
			const float Dn = GDunes ? FMath::Max(DunePaintAt(M, Pn), DunePaintAt(M, Pn + N * 20.0f)) : 0.0f;
			const float VD = GPads.Num() ? VillageDistAt(P) : 1e9f;
			// (16/10, a 2ª, o Felipe: "não ficou tão raro assim? [...] aparecer pelo menos um pedaço grande desse igual da
			// primeira": a 1ª dava 8,8% da beira; agora ~12–26% na areia, até 55% com dunas perto)
			C = FMath::Clamp(0.12f + 0.14f * Sandy + 0.30f * Dn, 0.0f, 0.55f) * FMath::Lerp(0.5f, 1.0f, FMath::SmoothStep(25.0f, 60.0f, VD));
		}
		const float Up = -(N | Wd);                       // 1: este lado olha pra onde o vento vem
		const float Along = FMath::Square(Hit.Dir | Wd);  // 1: a estrada corre com o vento
		C *= FMath::Clamp(1.0f + 0.55f * Up, 0.3f, 1.6f) * FMath::Lerp(1.0f, 0.6f, Along);
		if (C <= 0.002f)
		{
			return R;
		}
		// os trechos: um ruído ao longo da estrada (a fase S), um por lado e por estrada
		const float Lane = SideSign * 17.3f + Hit.Road * 5.71f;
		const float T = FbmQuantile(1.0f - FMath::Min(C, 0.95f), 3);
		const float Nn = WorldNoise::Fbm(Hit.S / 45.0f, Lane, 981u, 3);
		const float Patch = FMath::SmoothStep(T - 0.04f, T + 0.04f, Nn);
		if (Patch <= 0.0f)
		{
			return R;
		}
		R.Patch = Patch;
		const float Depth = FMath::Clamp((Nn - T) / 0.15f, 0.0f, 1.0f);
		// as línguas: os picos curtos (~5 m) dentro dos trechos, esticados no vento
		const float Tq = FbmQuantile(0.90f, 2);
		const float Tongue = FMath::SmoothStep(Tq - 0.03f, Tq + 0.06f, WorldNoise::Fbm(Hit.S / 7.0f, Lane + 40.0f, 983u, 2)) * Patch;
		const float Reach = FMath::Lerp(0.3f, 2.2f, FMath::Pow(Depth, 0.8f)) + 1.0f * Tongue;   // (a 1ª: 0,2–0,8 m; o pedaço grande da 1ª versão entrava ~2–3 m)
		// a beira da película em recortes: o ruído fino e o risco no vento
		const float Streak = WorldNoise::Fbm((P | Wd) / 6.0f, (P | FVector2f(-Wd.Y, Wd.X)) / 1.2f, 941u, 3);
		const float Scal = (0.22f + 0.15f * Depth) * WorldNoise::Fbm(P.X / 2.2f, P.Y / 2.2f, 985u, 2) + (0.18f + 0.15f * Depth) * Streak;
		const float Dd = FMath::Max(-In, 0.0f);
		// (16/10, a 2ª no jogo: a película cheia com a borda dura lia como tinta cor de areia) a borda esfarela em ~1 m: a
		// areia rala, em grãos e fiapos, deixando o asfalto aparecer; cheia só no miolo do trecho
		const float Inner = 1.0f - FMath::SmoothStep(Reach - 1.0f, Reach + 0.1f, Dd + Scal);
		const float Grain = FMath::Clamp(0.5f + 1.3f * WorldNoise::Fbm(P.X / 0.7f, P.Y / 0.7f, 987u, 3), 0.0f, 1.0f);
		R.Film = Patch * Inner * FMath::Lerp(Grain, 1.0f, FMath::SmoothStep(0.55f, 0.95f, Inner));
		// no trecho o monte sobe acima do topo da laje (cobre o degrau) e some 1,8 m pra fora
		const float Mound = Patch * FMath::Lerp(0.20f, 0.25f, Depth) * (1.0f - FMath::SmoothStep(0.2f, 1.8f, In))
			* (1.0f - FMath::SmoothStep(0.0f, 0.5f, Dd));
		R.Drift = FMath::Max(R.Drift, Mound);
		return R;
	}

	/** O material da sujeira no ponto da laje: o chão do lado (3 m além da pista; debaixo dela é o leito), em 4 partes
	 *  (areia, cascalho, terra, sal; a rocha entra como cascalho e terra, a pedra moída). A pintura à mão manda: a camada
	 *  pintada vira a sujeira e o asfalto pintado limpa (Film). Src: as partes, somam 1. */
	void SlabDirtSource(const FWorldMap& M, FVector2f P, const FRoadHit& Hit, float& Film, float Src[4])
	{
		Src[0] = 1.0f; Src[1] = Src[2] = Src[3] = 0.0f;
		if (Hit.Road == INDEX_NONE)
		{
			return;
		}
		const FVector2f Off = P - Hit.Closest;
		const float SideSign = (Hit.Dir.X * Off.Y - Hit.Dir.Y * Off.X) >= 0.0f ? 1.0f : -1.0f;
		const FVector2f Pn = Hit.Closest + FVector2f(-Hit.Dir.Y, Hit.Dir.X) * SideSign * (Hit.Half() + 3.0f);
		float Wn[6];
		ScatterWeights(M, Pn, Wn);
		float S4[4] = { Wn[0] + Wn[5], Wn[1] + 0.7f * Wn[3], Wn[2] + 0.3f * Wn[3], Wn[4] };
		if (Edits.IsValid() && Edits->Paint.Num())
		{
			int32 Layer;
			const float A = Edits->PaintAt(P, Layer);
			if (A > 0.0f && Layer >= 0 && Layer < 6)
			{
				if (Layer == 5)
				{
					Film *= 1.0f - A;   // o asfalto pintado limpa
				}
				else
				{
					const int32 K = Layer == 0 ? 0 : Layer == 1 ? 1 : Layer == 2 ? 2 : Layer == 3 ? 1 : 3;
					for (float& X : S4) X *= 1.0f - A;
					S4[K] += A;
					Film = FMath::Max(Film, A);
				}
			}
		}
		const float Sum = FMath::Max(S4[0] + S4[1] + S4[2] + S4[3], 1e-4f);
		for (int32 K = 0; K < 4; ++K) Src[K] = S4[K] / Sum;
	}

	/** A laje no ponto (só nas estradas de asfalto). A areia encostada é a da maquete (look_ground.fields: a faixa de
	 *  ~2,4 m em volta da beira, deslocada pelo ruído e esticada no vento, em trechos, 0,22 m × (1 + 0,9 sand_over)); o
	 *  desgaste muda em trechos de ~150 m ao longo da estrada e a vila fica inteira (a folha do Look: "a vila inteira,
	 *  intacta com areia"). */
	FSlabSample SlabSample(const FWorldMap& M, FVector2f P, const FRoadHit& Hit)
	{
		FSlabSample S;
		if (Hit.Road == INDEX_NONE || !Hit.Paved() || GRoads[Hit.Road].Style == 1)
		{
			return S;
		}
		S.bPaved = true;
		float EdgeP, Holes, Ends;
		PavedEdge(P, Hit, S.In, EdgeP, Holes, Ends);
		S.A = (1.0f - FMath::SmoothStep(-0.6f, 0.6f, S.In)) * (1.0f - Holes) * FMath::SmoothStep(0.0f, 0.25f, Ends);
		// (16/10, o Felipe no jogo: a faixa de areia de antes cobria a beira em quase toda a estrada, "quase não dá pra ver
		// nenhuma borda"; agora a sujeira em trechos e do chão do lado, RoadDirtAt)
		const FRoadDirt Dirt = RoadDirtAt(M, P, Hit, S.In);
		S.Drift = Dirt.Drift;
		S.Film = Dirt.Film;
		S.Patch = Dirt.Patch;
		const float VD = GPads.Num() ? VillageDistAt(P) : 1e9f;
		S.Wear = FMath::Clamp(0.5f + 1.1f * WorldNoise::Fbm(P.X / 150.0f, P.Y / 150.0f, 947u, 2), 0.0f, 1.0f) * FMath::SmoothStep(25.0f, 60.0f, VD);
		// (16/10) o acabamento escolhido no painel manda na estrada inteira: inteira, gasta ou destruída
		const int32 Style = GRoads[Hit.Road].Style;
		if (Style >= 2) S.Wear = Style == 2 ? 0.1f : (Style == 3 ? 0.52f : 0.85f);
		return S;
	}

	/** A estrada que manda no ponto: a de beira mais perto (distância − meia largura; o asfalto ganha 2 m). */
	FRoadHit RoadAt(FVector2f P, int32 Skip = INDEX_NONE, bool bPavedOnly = false)
	{
		FRoadHit H;
		const TArray<FIntPoint>* Near = GRoadGrid.Find(RoadCellOf(P));
		if (!Near) return H;
		float Best = 1e9f;
		for (const FIntPoint& RS : *Near)
		{
			if (RS.X == Skip) continue;
			const FRoad& R = GRoads[RS.X];
			if (bPavedOnly && !R.bPaved) continue;
			const int32 K = RS.Y;
			const FVector2f A = R.P[K - 1], AB = R.P[K] - A;
			const float T = FMath::Clamp(((P - A) | AB) / FMath::Max(AB | AB, 1e-6f), 0.0f, 1.0f);
			const float D = FVector2f::Distance(P, A + AB * T);
			const float RH = R.W.Num() == R.P.Num() ? FMath::Lerp(R.W[K - 1], R.W[K], T) : R.Half;
			// o asfalto manda no cruzamento: a de terra que chega não corta a pista
			const float Score = D - RH - (R.bPaved ? 2.0f : 0.0f);
			if (Score < Best)
			{
				Best = Score;
				H.D = D;
				H.Road = RS.X;
				H.HalfWidth = RH;
				H.Dir = AB.GetSafeNormal();
				H.Closest = A + AB * T;
				H.bPavedRoad = R.bPaved;
				H.Z = FMath::Lerp(R.Z[K - 1], R.Z[K], T);
				const float S = float(K - 1) + T;   // metros desde o começo
				H.S = S;
				const float E0 = R.bOpen[0] ? S : 1e9f, E1 = R.bOpen[1] ? float(R.P.Num() - 1) - S : 1e9f;
				H.End = FMath::Min(E0, E1);
				H.EndT = FMath::Min(E0 / R.Fade[0], E1 / R.Fade[1]);
			}
		}
		return H;
	}

	static TArray<FVector2f> Resample(const TArray<FVector2f>& In, float Step)
	{
		TArray<FVector2f> Out;
		if (In.Num() == 0) return Out;
		Out.Add(In[0]);
		float Carry = Step;
		for (int32 I = 1; I < In.Num(); ++I)
		{
			const FVector2f A = In[I - 1], B = In[I];
			const float Len = FVector2f::Distance(A, B);
			float T = Carry;
			for (; T <= Len; T += Step) Out.Add(FMath::Lerp(A, B, T / FMath::Max(Len, 1e-6f)));
			Carry = T - Len;
		}
		if (FVector2f::Distance(Out.Last(), In.Last()) > 0.3f * Step) Out.Add(In.Last());
		return Out;
	}

	/** Média móvel de ±Radius pontos, Passes vezes (as pontas ficam no lugar). */
	static TArray<FVector2f> Smooth(TArray<FVector2f> In, int32 Radius, int32 Passes)
	{
		for (int32 Pass = 0; Pass < Passes; ++Pass)
		{
			TArray<FVector2f> Sm = In;
			for (int32 I = 1; I + 1 < In.Num(); ++I)
			{
				const int32 R = FMath::Min3(Radius, I, In.Num() - 1 - I);
				FVector2f Sum = FVector2f::ZeroVector;
				for (int32 K = I - R; K <= I + R; ++K) Sum += In[K];
				Sm[I] = Sum / float(2 * R + 1);
			}
			In = MoveTemp(Sm);
		}
		return In;
	}

	/** Ponto mais perto numa linha (passo de 1 m): índice e distância. */
	static float NearestOn(const TArray<FVector2f>& L, FVector2f P, int32& OutIndex)
	{
		float Best = 1e9f;
		OutIndex = INDEX_NONE;
		for (int32 I = 0; I < L.Num(); ++I)
		{
			const float D = FVector2f::DistSquared(L[I], P);
			if (D < Best) { Best = D; OutIndex = I; }
		}
		return FMath::Sqrt(Best);
	}

	/** Os pontos que bastam pra redesenhar a linha (Douglas-Peucker, 1973): ninguém
	 *  fica a mais de Tol (m) do traçado, e nenhum trecho passa de 60 pontos. */
	static TArray<FVector2f> Decimate(const TArray<FVector2f>& L, float Tol)
	{
		if (L.Num() < 3) return L;
		TArray<bool> Keep;
		Keep.Init(false, L.Num());
		Keep[0] = Keep.Last() = true;
		TArray<FIntPoint> Stack = { FIntPoint(0, L.Num() - 1) };
		while (Stack.Num())
		{
			const FIntPoint S = Stack.Pop();
			const FVector2f A = L[S.X], AB = L[S.Y] - L[S.X];
			float Worst = 0.0f;
			int32 W = INDEX_NONE;
			for (int32 I = S.X + 1; I < S.Y; ++I)
			{
				const float T = FMath::Clamp(FVector2f::DotProduct(L[I] - A, AB) / FMath::Max(AB.SizeSquared(), 1e-6f), 0.0f, 1.0f);
				const float D = FVector2f::Distance(L[I], A + T * AB);
				if (D > Worst) { Worst = D; W = I; }
			}
			const int32 M = Worst > Tol ? W : (S.Y - S.X > 60 ? (S.X + S.Y) / 2 : INDEX_NONE);
			if (M == INDEX_NONE) continue;
			Keep[M] = true;
			Stack.Add(FIntPoint(S.X, M));
			Stack.Add(FIntPoint(M, S.Y));
		}
		TArray<FVector2f> Out;
		for (int32 I = 0; I < L.Num(); ++I) if (Keep[I]) Out.Add(L[I]);
		return Out;
	}

	/** O traçado da estrada desenhada à mão: os pontos de controle suavizados,
	 *  um ponto por metro (o mesmo que a prévia do app desenha). */
	static TArray<FVector2f> UserRoadLine(const TArray<FVector2f>& Points)
	{
		return Resample(Smooth(Resample(Points, 2.0f), 3, 2), 1.0f);
	}

	void IndexRoads()
	{
		GRoadGrid.Reset();
		GRoadReach = 0.0f;
		for (int32 R = 0; R < GRoads.Num(); ++R)
		{
			const FRoad& Road = GRoads[R];
			const float Reach = Road.MaxHalf() + 34.0f;   // (o talude do corte vai até ~36 m do eixo: RoadCut)
			GRoadReach = FMath::Max(GRoadReach, Reach);
			for (int32 K = 1; K < Road.P.Num(); ++K)
			{
				const FVector2f Lo(FMath::Min(Road.P[K - 1].X, Road.P[K].X) - Reach, FMath::Min(Road.P[K - 1].Y, Road.P[K].Y) - Reach);
				const FVector2f Hi(FMath::Max(Road.P[K - 1].X, Road.P[K].X) + Reach, FMath::Max(Road.P[K - 1].Y, Road.P[K].Y) + Reach);
				const FIntPoint A = RoadCellOf(Lo), B = RoadCellOf(Hi);
				for (int32 Y = A.Y; Y <= B.Y; ++Y)
				{
					for (int32 X = A.X; X <= B.X; ++X)
					{
						GRoadGrid.FindOrAdd(FIntPoint(X, Y)).Add(FIntPoint(R, K));
					}
				}
			}
		}
	}

	/**
	 * Ramal de terra de um campo de recurso até a rede: Dijkstra na grade de 2 m
	 * com 16 direções (vizinhança k = 2 de Galin et al. 2010: ângulos de ~27° em
	 * vez de 45°, menos zigue-zague), custo = comprimento × (1 + (rampa/12%)²),
	 * sem passar por penhasco nem água. Para ao chegar a 2 m de uma estrada.
	 */
	TArray<FVector2f> RouteSpur(const FWorldMap& M, FVector2f From, float FromRadius)
	{
		const int32 N = M.N;
		TArray<float> Dist;
		Dist.Init(TNumericLimits<float>::Max(), N * N);
		TArray<int32> Prev;
		Prev.Init(-1, N * N);
		using FNode = TPair<float, int32>;
		TArray<FNode> Heap;
		auto Less = [](const FNode& X, const FNode& Y) { return X.Key < Y.Key; };
		const FVector2f G = M.ToGrid(From);
		const int32 RC = FMath::CeilToInt(FromRadius / M.Cell);
		for (int32 J = FMath::Max(0, int32(G.Y) - RC); J <= FMath::Min(N - 1, int32(G.Y) + RC); ++J)
		{
			for (int32 I = FMath::Max(0, int32(G.X) - RC); I <= FMath::Min(N - 1, int32(G.X) + RC); ++I)
			{
				if (FVector2f::Distance(M.CellPos(I, J), From) > FromRadius) continue;
				const int32 C = M.Index(I, J);
				Dist[C] = 0.0f;
				Heap.HeapPush(FNode(0.0f, C), Less);
			}
		}
		static const int32 Off[16][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 },
			{ 2, 1 }, { 2, -1 }, { -2, 1 }, { -2, -1 }, { 1, 2 }, { 1, -2 }, { -1, 2 }, { -1, -2 } };
		auto Blocked = [&M](int32 C) { return M.Pass[C] != EWorldPass::Open || M.Height[C] < M.WaterLevel + 0.2f; };
		while (Heap.Num())
		{
			FNode Top;
			Heap.HeapPop(Top, Less, EAllowShrinking::No);
			const int32 C = Top.Value;
			if (Top.Key > Dist[C]) continue;
			const int32 I = C % N, J = C / N;
			const FVector2f P = M.CellPos(I, J);
			if (Top.Key > 0.0f && RoadAt(P).D < RoadAt(P).Half() + 2.0f)
			{
				TArray<FVector2f> Path;
				for (int32 X = C; X >= 0; X = Prev[X]) Path.Add(M.CellPos(X % N, X / N));
				Algo::Reverse(Path);   // do campo à estrada
				return Path;
			}
			for (const auto& O : Off)
			{
				const int32 A = I + O[0], B = J + O[1];
				if (A < 0 || B < 0 || A >= N || B >= N) continue;
				const int32 Q = M.Index(A, B);
				if (Blocked(Q)) continue;
				const float Len = M.Cell * FMath::Sqrt(float(O[0] * O[0] + O[1] * O[1]));
				const float Grade = FMath::Abs(M.Height[Q] - M.Height[C]) / Len;
				if (Grade > 0.45f) continue;
				const float ND = Dist[C] + Len * (1.0f + FMath::Square(Grade / 0.12f));
				if (ND < Dist[Q])
				{
					Dist[Q] = ND;
					Prev[Q] = C;
					Heap.HeapPush(FNode(ND, Q), Less);
				}
			}
		}
		return TArray<FVector2f>();
	}

	/**
	 * A rede do mapa inteiro (não só do recorte: assim as pontas e os
	 * entroncamentos são os mesmos em qualquer recorte). MainHalf: meia largura
	 * da principal; bPaved: ela é de asfalto; DirtHalf: a das de terra.
	 */
	void BuildRoads(const FWorldMap& M, float MainHalf, bool bPaved, float DirtHalf)
	{
		GRoads.Reset();
		GRoadGrid.Reset();
		if (MainHalf <= 0.0f) return;
		// a trilha do gerador (grade de 4 m) suavizada em ~30 m e reamostrada a cada metro
		auto Line = [](const TArray<FVector2f>& Track) { return Resample(Smooth(Track, 4, 3), 1.0f); };

		// 1. principais primeiro, depois flancos e ligações. Trecho a menos de
		// (meia largura + 3 m) de uma estrada já feita é absorvido por ela: a
		// trilha se parte ali e os pedaços entroncam na estrada
		TArray<int32> Order;
		for (int32 T = 0; T < M.Tracks.Num(); ++T) if (M.TrackKinds.IsValidIndex(T) && M.TrackKinds[T] == EWorldTrack::Main) Order.Add(T);
		for (int32 T = 0; T < M.Tracks.Num(); ++T) if (!M.TrackKinds.IsValidIndex(T) || M.TrackKinds[T] != EWorldTrack::Main) Order.Add(T);
		for (int32 T : Order)
		{
			const bool bMain = M.TrackKinds.IsValidIndex(T) && M.TrackKinds[T] == EWorldTrack::Main;
			const TArray<FVector2f> L = Line(M.Tracks[T]);
			TArray<FVector2f> Piece;
			auto Flush = [&]()
			{
				if (Piece.Num() >= 20)
				{
					FRoad& R = GRoads.AddDefaulted_GetRef();
					R.P = Piece;
					R.Half = bMain ? MainHalf : DirtHalf;
					R.bPaved = bMain && bPaved;
				}
				Piece.Reset();
			};
			for (const FVector2f& P : L)
			{
				bool bTaken = false;
				for (const FRoad& R : GRoads)
				{
					int32 Idx;
					if (NearestOn(R.P, P, Idx) < R.Half + 3.0f) { bTaken = true; break; }
				}
				if (bTaken) Flush();
				else Piece.Add(P);
			}
			Flush();
		}

		// 2. dentro da base a estrada acaba no platô (um terço pra dentro da
		// beira), onde o asfalto se desfaz na terra batida
		for (int32 R = GRoads.Num() - 1; R >= 0; --R)
		{
			TArray<FVector2f>& P = GRoads[R].P;
			for (int32 Side = 0; Side < 2; ++Side)
			{
				for (const FWorldStart& St : M.Starts)
				{
					const float Inner = 0.62f * M.BaseRadius;
					while (P.Num() > 1 && FVector2f::Distance(Side == 0 ? P[0] : P.Last(), St.Pos) < Inner)
					{
						if (Side == 0) P.RemoveAt(0); else P.Pop();
						GRoads[R].bOpen[Side] = true;
					}
				}
			}
			if (P.Num() < 20) GRoads.RemoveAt(R);
		}

		// 2b. pontas de duas estradas da mesma classe a menos de 30 m uma da outra
		// (as trilhas de cada jogador chegam ao centro por lados opostos) viram
		// uma estrada só, emendada e suavizada: atravessa o centro sem dente, e as
		// de terra cruzam a principal em X
		for (bool bMerged = true; bMerged; )
		{
			bMerged = false;
			for (int32 A = 0; A < GRoads.Num() && !bMerged; ++A)
			{
				for (int32 B = A + 1; B < GRoads.Num() && !bMerged; ++B)
				{
					const FRoad& RA = GRoads[A];
					const FRoad& RB = GRoads[B];
					if (RA.bPaved != RB.bPaved || RA.Half != RB.Half) continue;
					for (int32 SA = 0; SA < 2 && !bMerged; ++SA)
					{
						for (int32 SB = 0; SB < 2 && !bMerged; ++SB)
						{
							if (RA.bOpen[SA] || RB.bOpen[SB]) continue;
							const FVector2f EA = SA == 0 ? RA.P[0] : RA.P.Last(), EB = SB == 0 ? RB.P[0] : RB.P.Last();
							if (FVector2f::Distance(EA, EB) > 30.0f) continue;
							// A termina na emenda, B começa nela
							TArray<FVector2f> PA = RA.P, PB = RB.P;
							bool OA[2] = { RA.bOpen[0], RA.bOpen[1] }, OB[2] = { RB.bOpen[0], RB.bOpen[1] };
							if (SA == 0) { Algo::Reverse(PA); Swap(OA[0], OA[1]); }
							if (SB == 1) { Algo::Reverse(PB); Swap(OB[0], OB[1]); }
							TArray<FVector2f> Joined = PA;
							Joined.Append(PB);
							FRoad Merged;
							Merged.P = Resample(Smooth(Joined, 8, 3), 1.0f);
							Merged.Half = RA.Half;
							Merged.bPaved = RA.bPaved;
							Merged.bOpen[0] = OA[0];
							Merged.bOpen[1] = OB[1];
							GRoads[A] = MoveTemp(Merged);
							GRoads.RemoveAt(B);
							bMerged = true;
						}
					}
				}
			}
		}

		// 3. pontas que não estão numa base entroncam na estrada mais perto
		// (até 45 m: reta até ela); sem nenhuma por perto, a ponta fica solta
		auto Connect = [&](int32 R, int32 Side)
		{
			FRoad& Road = GRoads[R];
			if (Road.bOpen[Side]) return;
			const FVector2f E = Side == 0 ? Road.P[0] : Road.P.Last();
			float Best = 45.0f;
			int32 Host = INDEX_NONE, HostIdx = INDEX_NONE;
			for (int32 O = 0; O < GRoads.Num(); ++O)
			{
				if (O == R) continue;
				int32 Idx;
				const float D = NearestOn(GRoads[O].P, E, Idx);
				if (D < Best) { Best = D; Host = O; HostIdx = Idx; }
			}
			if (Host == INDEX_NONE)
			{
				Road.bOpen[Side] = true;
				return;
			}
			Road.Host[Side] = Host;
			const TArray<FVector2f> Bridge = Resample({ E, GRoads[Host].P[HostIdx] }, 1.0f);
			for (int32 K = 1; K < Bridge.Num(); ++K)
			{
				if (Side == 0) Road.P.Insert(Bridge[K], 0); else Road.P.Add(Bridge[K]);
			}
		};
		const int32 NumTrackRoads = GRoads.Num();
		for (int32 R = 0; R < NumTrackRoads; ++R)
		{
			Connect(R, 0);
			Connect(R, 1);
		}

		// 4. ramais de terra até os campos de recurso fora da rede (os mais
		// perto primeiro: os de depois podem entroncar num ramal já feito)
		auto IndexAll = [&]()
		{
			for (FRoad& R : GRoads) if (R.Z.Num() != R.P.Num()) R.Z.Init(0.0f, R.P.Num());
			IndexRoads();
		};
		IndexAll();
		TArray<const FWorldResource*> Fields;
		for (const FWorldResource& F : M.Resources) Fields.Add(&F);
		Fields.Sort([](const FWorldResource& A, const FWorldResource& B) { return A.Pos.SizeSquared() < B.Pos.SizeSquared(); });
		int32 Spurs = 0;
		for (const FWorldResource* F : Fields)
		{
			float Near = 1e9f;
			for (const FRoad& R : GRoads) { int32 Idx; Near = FMath::Min(Near, NearestOn(R.P, F->Pos, Idx) - R.Half); }
			if (Near < F->Radius + 6.0f) continue;   // o campo já está na beira de uma estrada
			TArray<FVector2f> Path = RouteSpur(M, F->Pos, 0.5f * F->Radius);
			if (Path.Num() < 4) continue;
			FRoad& R = GRoads.AddDefaulted_GetRef();
			R.P = Resample(Smooth(Path, 3, 3), 1.0f);
			R.Half = 0.85f * DirtHalf;
			R.bPaved = false;
			R.bOpen[0] = true;   // no campo (terra na terra)
			Connect(GRoads.Num() - 1, 1);
			IndexAll();
			++Spurs;
		}

		// 4b. as edições à mão: a estrada do gerador tirada sai; a desenhada entra
		// (traçado suavizado pelos pontos, um ponto por metro) e entronca na
		// estrada a até 12 m de cada ponta
		TArray<FIntPoint> Orphans;   // (estrada, ponta) que entroncava numa que saiu
		if (Edits.IsValid() && Edits->RemovedRoads.Num())
		{
			TArray<bool> Gone;
			Gone.Init(false, GRoads.Num());
			for (const FVector2f& Probe : Edits->RemovedRoads)
			{
				for (int32 R = 0; R < GRoads.Num(); ++R)
				{
					int32 Idx;
					Gone[R] |= NearestOn(GRoads[R].P, Probe, Idx) < 3.0f;
				}
			}
			TArray<int32> NewIndex;
			NewIndex.Init(INDEX_NONE, GRoads.Num());
			TArray<FRoad> Kept;
			for (int32 R = 0; R < GRoads.Num(); ++R)
			{
				if (Gone[R]) continue;
				NewIndex[R] = Kept.Num();
				Kept.Add(MoveTemp(GRoads[R]));
			}
			for (int32 R = 0; R < Kept.Num(); ++R)
			{
				for (int32 Side = 0; Side < 2; ++Side)
				{
					int32& Host = Kept[R].Host[Side];
					if (Host == INDEX_NONE) continue;
					Host = NewIndex[Host];
					if (Host == INDEX_NONE) Orphans.Add(FIntPoint(R, Side));
				}
			}
			GRoads = MoveTemp(Kept);
		}
		NumGenRoads = GRoads.Num();
		UserRoadIndex.Reset();
		if (Edits.IsValid() && Edits->Roads.Num())
		{
			for (const FWorldEdits::FRoadEdit& E : Edits->Roads)
			{
				if (E.Points.Num() < 2)
				{
					UserRoadIndex.Add(INDEX_NONE);
					continue;
				}
				// no bioma sem asfalto (floresta) a estrada desenhada é de terra
				const bool bPav = E.bPaved && bPaved;
				FRoad& R = GRoads.AddDefaulted_GetRef();
				R.P = UserRoadLine(E.Points);
				R.Half = bPav ? MainHalf : DirtHalf;
				R.bPaved = bPav;
				R.Style = bPav ? E.Style : 0;
				R.Dirt = bPav ? E.Dirt : 0;
				R.Fade[0] = E.bFade[0] ? 45.0f : 4.0f;
				R.Fade[1] = E.bFade[1] ? 45.0f : 4.0f;
				const int32 Me = GRoads.Num() - 1;
				UserRoadIndex.Add(Me);
				for (int32 Side = 0; Side < 2; ++Side)
				{
					const FVector2f End = Side == 0 ? R.P[0] : R.P.Last();
					float Near = 1e9f;
					for (int32 O = 0; O < Me; ++O) { int32 Idx; Near = FMath::Min(Near, NearestOn(GRoads[O].P, End, Idx)); }
					if (Near < 12.0f) Connect(Me, Side);
					else GRoads[Me].bOpen[Side] = true;
				}
			}
		}
		// a ponta que entroncava na estrada tirada entronca na que ficou ali perto
		// (a redesenhada, por exemplo); sem nenhuma a até 12 m, fica solta
		for (const FIntPoint& O : Orphans)
		{
			const FRoad& R = GRoads[O.X];
			const FVector2f End = O.Y == 0 ? R.P[0] : R.P.Last();
			float Near = 1e9f;
			for (int32 K = 0; K < GRoads.Num(); ++K) { if (K == O.X) continue; int32 Idx; Near = FMath::Min(Near, NearestOn(GRoads[K].P, End, Idx)); }
			if (Near < 12.0f) Connect(O.X, O.Y);
		}
		IndexAll();

		// 4c. a vila: endireita a estrada que passa por ela e põe a rua transversal
		GVillage.Reset();
		GPads.Reset();
		GPadGrid.Reset();
		GVillageDist.Reset();
		GVillagePalms.Reset();
		GVillagePoles.Reset();
		GVillageWires.Reset();
		GVillageLines.Reset();
		GPieceGrid.Reset();
		GVillageLots.Reset();
		GVillageLotWhy.Reset();
		if (M.Settings.Villages > 0)
		{
			BuildVillages(M, MainHalf);
			IndexAll();
			BuildHamlets(M);
		}

		// 5. cota: no asfalto o relevo suavizado em ~50 m (corta morro, aterra
		// vale), 8 cm acima do chão em volta; na ponta que entronca, a cota da
		// estrada de lá (rampa de 30 m), pra não ter degrau no cruzamento
		for (FRoad& R : GRoads)
		{
			TArray<float> Raw;
			for (const FVector2f& P : R.P) Raw.Add(M.HeightAt(P));
			R.Z.SetNum(Raw.Num());
			for (int32 I = 0; I < Raw.Num(); ++I)
			{
				float Sum = 0.0f;
				int32 C = 0;
				// a de terra acompanha o relevo (±8 m): com ±25 m ela afundava num
				// corte de paredes íngremes nas rampas (a areia esticada na parede)
				const int32 W = R.bPaved ? 25 : 8;
				for (int32 K = FMath::Max(0, I - W); K <= FMath::Min(Raw.Num() - 1, I + W); ++K) { Sum += Raw[K]; ++C; }
				R.Z[I] = Sum / float(C) + (R.bPaved ? 0.08f : 0.0f);
			}
		}
		for (int32 R = 0; R < GRoads.Num(); ++R)
		{
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const int32 Host = GRoads[R].Host[Side];
				if (Host == INDEX_NONE) continue;
				FRoad& Road = GRoads[R];
				int32 Idx;
				NearestOn(GRoads[Host].P, Side == 0 ? Road.P[0] : Road.P.Last(), Idx);
				const float HostZ = GRoads[Host].Z[Idx] - (GRoads[Host].bPaved && !Road.bPaved ? 0.08f : 0.0f);
				for (int32 K = 0; K < FMath::Min(30, Road.P.Num()); ++K)
				{
					const int32 I = Side == 0 ? K : Road.P.Num() - 1 - K;
					Road.Z[I] = FMath::Lerp(HostZ, Road.Z[I], FMath::SmoothStep(0.0f, 30.0f, float(K)));
				}
			}
		}
		IndexRoads();
		float Paved = 0.0f, Dirt = 0.0f;
		for (const FRoad& R : GRoads) (R.bPaved ? Paved : Dirt) += float(R.P.Num());
		UE_LOG(LogTemp, Display, TEXT("FORGE roads: %d (%d spurs to fields), %.1f km paved, %.1f km dirt; main half width %.1f m, dirt %.1f m"),
			GRoads.Num(), Spurs, Paved / 1000.0f, Dirt / 1000.0f, MainHalf, DirtHalf);
	}

	/**
	 * Pesos das camadas num ponto do mapa (somam 1), por papel — o nome vem do
	 * bioma (Python): 0 base (areia, neve, grama), 1 manchas (cascalho, neve
	 * suja, folhiço), 2 trilha (terra batida, lama), 3 rocha, 4 molhado/seco
	 * (terra rachada, gelo, lama), 5 asfalto (só com estrada).
	 */
	// os controles do painel do app ("Ver em qualidade real"): valem no lugar dos parâmetros

	bool GForest = false;   // floresta: o folhiço (camada de manchas) vai onde a mata fecha
	bool GWinter = false;   // neve: sem as dunas do deserto

	// cobertura das copas na grade do mapa (2 m): quanto do ponto fica sob uma
	// árvore do gerador. O folhiço, o sub-bosque e a madeira morta seguem as
	// árvores de verdade (as do painel), não um ruído à parte.
	TArray<float> GCanopy;

	void BuildCanopy(const FWorldMap& M)
	{
		GCanopy.Init(0.0f, M.N * M.N);
		for (const FWorldProp& P : M.Props)
		{
			if (P.Type != EWorldPropType::Tree) continue;
			const WorldTrees::EKind Kind = WorldTrees::KindOf(M.Settings.Biome, P.Variant);
			const float R = 0.5f * WorldTrees::MeshSize(Kind).Y * WorldTrees::ScaleOf(P, M.Settings.Biome) + 1.5f;   // copa + a beira
			const FVector2f G = M.ToGrid(P.Pos);
			const int32 Cells = FMath::CeilToInt(R / M.Cell) + 1;
			for (int32 J = FMath::Max(0, int32(G.Y) - Cells); J <= FMath::Min(M.N - 1, int32(G.Y) + Cells); ++J)
			{
				for (int32 I = FMath::Max(0, int32(G.X) - Cells); I <= FMath::Min(M.N - 1, int32(G.X) + Cells); ++I)
				{
					const float D = FVector2f::Distance(FVector2f(I, J), G) * M.Cell;
					float& C = GCanopy[M.Index(I, J)];
					C = FMath::Max(C, 1.0f - FMath::SmoothStep(0.6f * R, R, D));
				}
			}
		}
	}

	// chão gasto em volta das rochas do gerador (grade de 2 m, 0–1): a rocha que
	// sai de um gramado liso, sem terra nem cascalho na base, lia como colada
	// por cima (06/10)
	TArray<float> GRockRing;

	void BuildRockRing(const FWorldMap& M)
	{
		GRockRing.Init(0.0f, M.N * M.N);
		for (const FWorldProp& P : M.Props)
		{
			if (P.Type != EWorldPropType::Rock) continue;
			const float R = 1.6f + 1.0f * P.Scale;
			const FVector2f G = M.ToGrid(P.Pos);
			const int32 Cells = FMath::CeilToInt(R / M.Cell) + 1;
			for (int32 J = FMath::Max(0, int32(G.Y) - Cells); J <= FMath::Min(M.N - 1, int32(G.Y) + Cells); ++J)
			{
				for (int32 I = FMath::Max(0, int32(G.X) - Cells); I <= FMath::Min(M.N - 1, int32(G.X) + Cells); ++I)
				{
					const float D = FVector2f::Distance(FVector2f(I, J), G) * M.Cell;
					float& C = GRockRing[M.Index(I, J)];
					C = FMath::Max(C, 1.0f - FMath::SmoothStep(0.45f * R, R, D));
				}
			}
		}
	}

	float RockRingAt(const FWorldMap& M, FVector2f P)
	{
		if (GRockRing.Num() != M.N * M.N) return 0.0f;
		const FVector2f G = M.ToGrid(P);
		const float X = FMath::Clamp(G.X, 0.0f, float(M.N - 1)), Y = FMath::Clamp(G.Y, 0.0f, float(M.N - 1));
		const int32 I0 = FMath::Min(int32(X), M.N - 2), J0 = FMath::Min(int32(Y), M.N - 2);
		const float FX = X - I0, FY = Y - J0;
		return FMath::Lerp(FMath::Lerp(GRockRing[M.Index(I0, J0)], GRockRing[M.Index(I0 + 1, J0)], FX),
			FMath::Lerp(GRockRing[M.Index(I0, J0 + 1)], GRockRing[M.Index(I0 + 1, J0 + 1)], FX), FY);
	}

	// distância com sinal até a borda da salina (m; > 0 dentro), até ±24 m: a
	// crosta afina na beira e a terra úmida se estende por fora
	TArray<float> GSaltDist;
	// força da bacia mais perto: 1 salina (M.Basin == 1), 0,5 lagoa seca pequena
	// (M.Basin == 3, um barreiro: argila rachada com eflorescência de sal, a
	// mesma forma com a crosta pela metade)
	TArray<float> GSaltStr;

	void BuildSaltField(const FWorldMap& M)
	{
		GSaltDist.Init(-24.0f, M.N * M.N);
		GSaltStr.Init(0.0f, M.N * M.N);
		if (M.Basin.Num() != M.N * M.N) return;
		auto In = [&M](int32 V) { return M.Basin[V] == 1 || M.Basin[V] == 3; };
		{
			TArray<int32> Front;
			for (int32 V = 0; V < M.N * M.N; ++V)
			{
				if (In(V)) { GSaltStr[V] = M.Basin[V] == 1 ? 1.0f : 0.5f; Front.Add(V); }
			}
			// espalha a força pra fora (~24 m): a borda e a faixa úmida sabem de qual bacia são
			for (int32 Ring = 0; Ring < FMath::CeilToInt(24.0f / M.Cell) && Front.Num(); ++Ring)
			{
				TArray<int32> Next;
				for (const int32 V : Front)
				{
					const int32 I = V % M.N, J = V / M.N;
					const int32 N4[4][2] = { { I + 1, J }, { I - 1, J }, { I, J + 1 }, { I, J - 1 } };
					for (const auto& NB : N4)
					{
						if (NB[0] < 0 || NB[1] < 0 || NB[0] >= M.N || NB[1] >= M.N) continue;
						const int32 Q = M.Index(NB[0], NB[1]);
						if (GSaltStr[Q] == 0.0f) { GSaltStr[Q] = GSaltStr[V]; Next.Add(Q); }
					}
				}
				Front = MoveTemp(Next);
			}
		}
		TArray<float> D;
		D.Init(1e9f, M.N * M.N);
		TArray<int32> Queue;
		for (int32 J = 0; J < M.N; ++J)
		{
			for (int32 I = 0; I < M.N; ++I)
			{
				const int32 V = M.Index(I, J);
				bool bEdge = false;
				for (int32 DJ = -1; DJ <= 1 && !bEdge; ++DJ)
				{
					for (int32 DI = -1; DI <= 1 && !bEdge; ++DI)
					{
						const int32 X = I + DI, Y = J + DJ;
						bEdge = X >= 0 && Y >= 0 && X < M.N && Y < M.N && In(M.Index(X, Y)) != In(V);
					}
				}
				if (bEdge) { D[V] = 0.5f * M.Cell; Queue.Add(V); }
			}
		}
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 V = Queue[Head];
			const int32 I = V % M.N, J = V / M.N;
			for (int32 DJ = -1; DJ <= 1; ++DJ)
			{
				for (int32 DI = -1; DI <= 1; ++DI)
				{
					const int32 X = I + DI, Y = J + DJ;
					if ((DI | DJ) == 0 || X < 0 || Y < 0 || X >= M.N || Y >= M.N) continue;
					const int32 Q = M.Index(X, Y);
					const float ND = D[V] + M.Cell * ((DI && DJ) ? 1.41421356f : 1.0f);
					if (ND < D[Q] && ND < 24.0f) { D[Q] = ND; Queue.Add(Q); }
				}
			}
		}
		for (int32 V = 0; V < M.N * M.N; ++V) GSaltDist[V] = FMath::Min(D[V], 24.0f) * (In(V) ? 1.0f : -1.0f);
	}

	float SaltDistAt(const FWorldMap& M, FVector2f P)
	{
		if (GSaltDist.Num() != M.N * M.N) return -24.0f;
		const FVector2f G = M.ToGrid(P);
		const float X = FMath::Clamp(G.X, 0.0f, float(M.N - 1)), Y = FMath::Clamp(G.Y, 0.0f, float(M.N - 1));
		const int32 I0 = FMath::Min(int32(X), M.N - 2), J0 = FMath::Min(int32(Y), M.N - 2);
		const float FX = X - I0, FY = Y - J0;
		return FMath::Lerp(FMath::Lerp(GSaltDist[M.Index(I0, J0)], GSaltDist[M.Index(I0 + 1, J0)], FX),
			FMath::Lerp(GSaltDist[M.Index(I0, J0 + 1)], GSaltDist[M.Index(I0 + 1, J0 + 1)], FX), FY);
	}

	/** A salina num ponto: Crust = a crosta (0–1, em placas recortadas na borda),
	 *  Soft = a faixa de terra úmida em volta (0–1), Damp = umidade pro chão
	 *  (escurece e alisa, 0–1). Sem salina, tudo 0.
	 *  Como uma salina de verdade se vê de cima (Badwater, Chott el Djerid,
	 *  Uyuni na borda): areia → lama úmida escura → crosta fina em placas e
	 *  "renda" por cima da lama → crosta contínua, com manchas mais finas e úmidas
	 *  no miolo. O degradê liso de antes e a forma herdada da grade de 4 m liam
	 *  como decalque recortado (revisão de arte de 07/10). */
	struct FSaltShape { float Crust = 0.0f, Soft = 0.0f, Damp = 0.0f; };

	FSaltShape SaltShapeAt(const FWorldMap& M, FVector2f P)
	{
		FSaltShape Out;
		if (GSaltDist.Num() != M.N * M.N) return Out;
		const float Raw = SaltDistAt(M, P);
		if (Raw <= -23.5f) return Out;
		// lóbulos de ~40 m e recorte de ~9 m: a borda deixa de seguir a grade
		const float Sd = Raw + 7.0f * WorldNoise::Fbm(P.X / 40.0f, P.Y / 40.0f, 941u, 3) + 3.5f * WorldNoise::Fbm(P.X / 9.0f, P.Y / 9.0f, 929u, 3);
		// borda: transição de ~10–20 m em que a crosta fina vai engrossando sobre a
		// lama (peso parcial; a mistura pela altura das texturas desenha as placas),
		// contorno orgânico pelos lóbulos e por ruído de ~6 m. A "renda" de borda
		// nítida com ruído de 1,8 m saía em quadradinhos soltos (revisão de 07/10)
		const float Lace = 0.35f * WorldNoise::Fbm(P.X / 6.0f, P.Y / 6.0f, 943u, 3);
		Out.Crust = FMath::SmoothStep(-0.25f, 0.45f, Sd / 16.0f + Lace);
		Out.Soft = FMath::SmoothStep(-16.0f, -1.0f, Sd);
		// miolo: manchas de crosta fina e úmida (~35 m, ~20% da área)
		const float Patch = FMath::SmoothStep(0.22f, 0.34f, WorldNoise::Fbm(P.X / 35.0f, P.Y / 35.0f, 945u, 3) + 0.15f * WorldNoise::Fbm(P.X / 6.0f, P.Y / 6.0f, 947u, 2));
		const float Inside = FMath::SmoothStep(2.0f, 8.0f, Sd);
		// umidade: forte na lama da borda, fraca nas manchas do miolo
		Out.Damp = FMath::Clamp(Out.Soft * (1.0f - Out.Crust) * (1.0f - Inside) + 0.45f * Patch * Inside, 0.0f, 1.0f);
		// barreiro (lagoa seca pequena): metade da crosta e da umidade
		const FVector2f G = M.ToGrid(P);
		const float Str = GSaltStr.Num() == M.N * M.N
			? GSaltStr[M.Index(FMath::Clamp(FMath::RoundToInt(G.X), 0, M.N - 1), FMath::Clamp(FMath::RoundToInt(G.Y), 0, M.N - 1))] : 1.0f;
		Out.Crust *= Str;
		Out.Soft *= Str;
		Out.Damp *= Str;
		return Out;
	}

	// mar (água ligada à borda do mapa, por vértice): no inverno o lago congela
	// e o mar não (o gelo no mar inteiro virava uma placa até o horizonte)
	TArray<uint8> GSea;

	void BuildSeaMask(const FWorldMap& M)
	{
		GSea.Init(0, M.N * M.N);
		auto Wet = [&M](int32 V) { return M.Height[V] < M.WaterLevel; };
		TArray<int32> Fill;
		for (int32 K = 0; K < M.N; ++K)
		{
			for (const int32 V : { M.Index(K, 0), M.Index(K, M.N - 1), M.Index(0, K), M.Index(M.N - 1, K) })
			{
				if (Wet(V) && !GSea[V]) { GSea[V] = 1; Fill.Add(V); }
			}
		}
		for (int32 Head = 0; Head < Fill.Num(); ++Head)
		{
			const int32 V = Fill[Head];
			const int32 I = V % M.N, J = V / M.N;
			const int32 N4[4][2] = { { I + 1, J }, { I - 1, J }, { I, J + 1 }, { I, J - 1 } };
			for (const auto& NB : N4)
			{
				if (NB[0] < 0 || NB[1] < 0 || NB[0] >= M.N || NB[1] >= M.N) continue;
				const int32 Q = M.Index(NB[0], NB[1]);
				if (Wet(Q) && !GSea[Q]) { GSea[Q] = 1; Fill.Add(Q); }
			}
		}
		// a margem do mar (~4 vértices em terra) também é mar: não congela, e a
		// faixa que a onda lava fica sem neve (DesertWeights)
		for (int32 Pass = 0; Pass < 4; ++Pass)
		{
			TArray<uint8> Grown = GSea;
			for (int32 J = 1; J < M.N - 1; ++J)
			{
				for (int32 I = 1; I < M.N - 1; ++I)
				{
					const int32 V = M.Index(I, J);
					if (GSea[V]) continue;
					if (GSea[V + 1] || GSea[V - 1] || GSea[V + M.N] || GSea[V - M.N]) Grown[V] = 1;
				}
			}
			GSea = MoveTemp(Grown);
		}
	}

	bool SeaAt(const FWorldMap& M, FVector2f P)
	{
		if (GSea.Num() != M.N * M.N) return false;
		const FVector2f G = M.ToGrid(P);
		const int32 I = FMath::Clamp(FMath::RoundToInt(G.X), 0, M.N - 1), J = FMath::Clamp(FMath::RoundToInt(G.Y), 0, M.N - 1);
		return GSea[M.Index(I, J)] != 0;
	}

	// margem do oásis (até ~22 m da água do oásis, M.Basin == 2), 0–1
	TArray<float> GOasis;

	void BuildOasisRing(const FWorldMap& M)
	{
		GOasis.Init(0.0f, M.N * M.N);
		if (M.Basin.Num() != M.N * M.N) return;
		constexpr float R = 22.0f;
		const int32 Cells = FMath::CeilToInt(R / M.Cell);
		for (int32 J = 0; J < M.N; ++J)
		{
			for (int32 I = 0; I < M.N; ++I)
			{
				if (M.Basin[M.Index(I, J)] != 2) continue;
				// só a beira da água (um vizinho que não é oásis)
				bool bEdge = false;
				for (int32 DJ = -1; DJ <= 1 && !bEdge; ++DJ)
				{
					for (int32 DI = -1; DI <= 1 && !bEdge; ++DI)
					{
						const int32 X = I + DI, Y = J + DJ;
						bEdge = X >= 0 && Y >= 0 && X < M.N && Y < M.N && M.Basin[M.Index(X, Y)] != 2;
					}
				}
				if (!bEdge) continue;
				for (int32 Y = FMath::Max(0, J - Cells); Y <= FMath::Min(M.N - 1, J + Cells); ++Y)
				{
					for (int32 X = FMath::Max(0, I - Cells); X <= FMath::Min(M.N - 1, I + Cells); ++X)
					{
						const float D = FMath::Sqrt(float(FMath::Square(X - I) + FMath::Square(Y - J))) * M.Cell;
						float& O = GOasis[M.Index(X, Y)];
						O = FMath::Max(O, 1.0f - FMath::SmoothStep(0.5f * R, R, D));
					}
				}
			}
		}
	}

	/** A terra úmida do oásis no chão (0–0,75): borda orgânica, quebrada por ruído de ~12 m e mais larga no
	 *  lado baixo (a água desce); o disco de borda dura lia como decalque (07/10). */
	float OasisDampAt(const FWorldMap& M, FVector2f P)
	{
		const float Low = FMath::Clamp((M.WaterLevel + 2.5f - M.HeightAt(P)) / 2.5f, 0.0f, 1.0f);
		const float Org = WorldNoise::Fbm(P.X / 12.0f, P.Y / 12.0f, 931u, 3);
		return 0.75f * FMath::SmoothStep(0.15f, 0.65f, OasisAt(M, P) * (0.75f + 0.5f * Low) + 0.3f * Org);
	}

	float OasisAt(const FWorldMap& M, FVector2f P)
	{
		if (GOasis.Num() != M.N * M.N) return 0.0f;
		const FVector2f G = M.ToGrid(P);
		const float X = FMath::Clamp(G.X, 0.0f, float(M.N - 1)), Y = FMath::Clamp(G.Y, 0.0f, float(M.N - 1));
		const int32 I0 = FMath::Min(int32(X), M.N - 2), J0 = FMath::Min(int32(Y), M.N - 2);
		const float FX = X - I0, FY = Y - J0;
		return FMath::Lerp(FMath::Lerp(GOasis[M.Index(I0, J0)], GOasis[M.Index(I0 + 1, J0)], FX),
			FMath::Lerp(GOasis[M.Index(I0, J0 + 1)], GOasis[M.Index(I0 + 1, J0 + 1)], FX), FY);
	}

	/** Leito seco no ponto (0–1), pelo fluxo d'água do gerador. */
	/** Fundo dos leitos (o gerador marca em M.Bed: wadi no deserto, córrego na
	 *  floresta, riacho congelado na neve), 0–1, bilinear. */
	float WadiAt(const FWorldMap& M, FVector2f P)
	{
		if (M.Bed.Num() != M.N * M.N) return 0.0f;
		const FVector2f G = M.ToGrid(P);
		const float X = FMath::Clamp(G.X, 0.0f, float(M.N - 1)), Y = FMath::Clamp(G.Y, 0.0f, float(M.N - 1));
		const int32 I0 = FMath::Min(int32(X), M.N - 2), J0 = FMath::Min(int32(Y), M.N - 2);
		const float FX = X - I0, FY = Y - J0;
		const float F = FMath::Lerp(FMath::Lerp(M.Bed[M.Index(I0, J0)], M.Bed[M.Index(I0 + 1, J0)], FX),
			FMath::Lerp(M.Bed[M.Index(I0, J0 + 1)], M.Bed[M.Index(I0 + 1, J0 + 1)], FX), FY);
		return FMath::SmoothStep(0.25f, 0.6f, F);
	}

	/** Mata no ponto: > 0 sob as copas das árvores do gerador (−0,5 no aberto). */
	float WoodAt(const FWorldMap& M, FVector2f P)
	{
		if (GCanopy.Num() != M.N * M.N) return -0.5f;
		const FVector2f G = M.ToGrid(P);
		const float X = FMath::Clamp(G.X, 0.0f, float(M.N - 1)), Y = FMath::Clamp(G.Y, 0.0f, float(M.N - 1));
		const int32 I0 = FMath::Min(int32(X), M.N - 2), J0 = FMath::Min(int32(Y), M.N - 2);
		const float FX = X - I0, FY = Y - J0;
		const float C = FMath::Lerp(FMath::Lerp(GCanopy[M.Index(I0, J0)], GCanopy[M.Index(I0 + 1, J0)], FX),
			FMath::Lerp(GCanopy[M.Index(I0, J0 + 1)], GCanopy[M.Index(I0 + 1, J0 + 1)], FX), FY);
		return C - 0.5f;
	}

	/** Tom do chão longe d'água (o A da textura da água; 0 = neutro), −1…1.
	 *  Deserto: + leito lavado (o fundo dos leitos secos: areia e cascalho soltos,
	 *  sem o verniz e a crosta que escurecem o chão velho — o leito ativo é a
	 *  faixa mais clara do deserto visto do alto, como nos alvos); − úmido (a lama
	 *  da borda da salina: escurece e alisa).
	 *  Floresta: + capim seco (clareira aberta, longe da mata e da água, em
	 *  manchas de ~60 m); − viçoso (o córrego, a margem d'água). O gramado de um
	 *  verde só lia como campo de futebol (07/10). */
	float GroundToneAt(const FWorldMap& M, FVector2f P, bool bSaltFlat)
	{
		if (GForest)
		{
			const float Open = FMath::SmoothStep(-0.1f, 0.45f, -WoodAt(M, P));
			const float DryPatch = FMath::SmoothStep(-0.15f, 0.35f, WorldNoise::Fbm(P.X / 60.0f, P.Y / 60.0f, 951u, 3) + 0.15f * WorldNoise::Fbm(P.X / 12.0f, P.Y / 12.0f, 953u, 2));
			// perto d'água pela distância (água num anel de 6 e 12 m): pela altura o
			// mapa plano inteiro, a 0,5–1,5 m da cota, contava como margem e o capim
			// saía todo viçoso, de um verde só (medido com debug_maps, 07/10)
			float NearWater = 0.0f;
			if (M.WaterLevel > -100.0f)
			{
				for (int32 K = 0; K < 8; ++K)
				{
					const float A = K * (UE_TWO_PI / 8.0f);
					const FVector2f Dir(FMath::Cos(A), FMath::Sin(A));
					NearWater += (M.HeightAt(P + 6.0f * Dir) < M.WaterLevel ? 0.09f : 0.0f) + (M.HeightAt(P + 12.0f * Dir) < M.WaterLevel ? 0.035f : 0.0f);
				}
				NearWater = FMath::Min(NearWater, 1.0f);
			}
			const float Lush = FMath::Max(WadiAt(M, P), 0.7f * NearWater);
			return 0.75f * Open * DryPatch * (1.0f - Lush) - 0.8f * Lush;
		}
		const FSaltShape Sh = bSaltFlat ? SaltShapeAt(M, P) : FSaltShape();
		float Wash = 0.0f;
		if (!GWinter)
		{
			const FRoadHit Hit = RoadAt(P);
			const float Road = Hit.Road != INDEX_NONE ? 1.0f - FMath::SmoothStep(Hit.Half(), Hit.Half() + 2.0f, Hit.D) : 0.0f;
			Wash = WadiAt(M, P) * (1.0f - Road) * (1.0f - Sh.Crust);
		}
		// a margem do oásis: terra úmida escura debaixo das palmeiras até a água
		// (o que faz a mancha escura e verde do oásis de longe; antes era areia seca
		// até a beira, revisão de arte de 07/10)
		return Wash - FMath::Max(Sh.Damp, 0.7f * OasisAt(M, P));
	}

	void DesertWeights(const FWorldMap& M, FVector2f P, float W[6])
	{
		// camadas do gerador (interpoladas): R terra, G vegetação, B margem, A rocha
		const FVector2f G = M.ToGrid(P);
		auto Bil = [&M, &G](int32 Ch)
		{
			const float X = FMath::Clamp(G.X, 0.0f, float(M.N - 1)), Y = FMath::Clamp(G.Y, 0.0f, float(M.N - 1));
			const int32 I0 = FMath::Min(int32(X), M.N - 2), J0 = FMath::Min(int32(Y), M.N - 2);
			const float FX = X - I0, FY = Y - J0;
			auto At = [&M, Ch](int32 A, int32 B)
			{
				const FColor& C = M.Layers[M.Index(A, B)];
				return (Ch == 0 ? C.R : Ch == 1 ? C.G : Ch == 2 ? C.B : C.A) / 255.0f;
			};
			return FMath::Lerp(FMath::Lerp(At(I0, J0), At(I0 + 1, J0), FX), FMath::Lerp(At(I0, J0 + 1), At(I0 + 1, J0 + 1), FX), FY);
		};
		const float Dirt = Bil(0), Lush = Bil(1), Rock = Bil(3);
		float Shore = Bil(2);
		if (GFrozen && M.HeightAt(P) < M.WaterLevel + 0.15f && !SeaAt(M, P)) Shore = 1.0f;   // a placa de gelo inteira (lago; o mar não)
		// escoamento interpolado: o do vértice mais perto fazia quadrados de 2 m de lama
		float Flow = 0.0f;
		if (M.Flow.Num() == M.N * M.N)
		{
			const float X = FMath::Clamp(G.X, 0.0f, float(M.N - 1)), Y = FMath::Clamp(G.Y, 0.0f, float(M.N - 1));
			const int32 I0 = FMath::Min(int32(X), M.N - 2), J0 = FMath::Min(int32(Y), M.N - 2);
			const float FX = X - I0, FY = Y - J0;
			Flow = FMath::Lerp(FMath::Lerp(M.Flow[M.Index(I0, J0)], M.Flow[M.Index(I0 + 1, J0)], FX),
				FMath::Lerp(M.Flow[M.Index(I0, J0 + 1)], M.Flow[M.Index(I0 + 1, J0 + 1)], FX), FY);
		}
		// manchas largas (ruído) pra variar cascalho × areia
		const float Patch = WorldNoise::Fbm(P.X / 45.0f, P.Y / 45.0f, 911u, 4);
		const float Fine = WorldNoise::Fbm(P.X / 7.0f, P.Y / 7.0f, 913u, 3);
		// rocha também pela inclinação do relevo (acima de ~35°): a camada do
		// gerador vem da grade de 2 m e falhava em faixas finas de encosta, e a
		// grama ou a terra ficavam esticadas na parede (06/10)
		const float Steep = 1.0f - FMath::SmoothStep(0.74f, 0.84f, M.NormalAt(P).Z);
		float Rk = FMath::Max(FMath::SmoothStep(0.25f, 0.7f, Rock), Steep);
		// terra batida; na floresta só o miolo dos trechos de terra (as bases e as
		// trilhas), o resto é grama — campo de terra grande lia como deserto
		// (as manchas largas de terra do gerador, ~0,35, ficam fora: de perto
		// liam como mancha de sujeira; a variação do chão é o cascalho, abaixo)
		float Dr = (GForest ? FMath::SmoothStep(0.5f, 0.85f, Dirt + 0.1f * Fine) : FMath::SmoothStep(0.42f, 0.7f, Dirt + 0.1f * Fine)) * (1.0f - Rk);
		// deserto: o pátio (base, campo de recurso) é terra batida com cascalho e a
		// borda roída em ~3,5 m; o disco liso de terra lia como poça de lama vista
		// de cima (revisão de arte de 07/10)
		float PadGravel = 0.0f;
		if (!GForest && !GWinter)
		{
			Dr = FMath::SmoothStep(0.36f, 0.72f, Dirt + 0.16f * Fine + 0.12f * WorldNoise::Fbm(P.X / 3.5f, P.Y / 3.5f, 949u, 2)) * (1.0f - Rk);
			PadGravel = 0.35f * Dr;
			Dr -= PadGravel;
		}
		// salina (bacia seca do deserto, M.Basin == 1), bilinear e com a borda
		// recortada em ~4 m
		float Salt = 0.0f, SaltSoft = 0.0f;
		if (GSaltDist.Num() == M.N * M.N)
		{
			const FSaltShape Sh = SaltShapeAt(M, P);
			// a crosta fina das manchas úmidas deixa a lama aparecer um pouco
			Salt = Sh.Crust * (1.0f - 0.3f * Sh.Damp);
			SaltSoft = Sh.Soft;
		}
		else if (M.Basin.Num() == M.N * M.N)
		{
			const float X = FMath::Clamp(G.X, 0.0f, float(M.N - 1)), Y = FMath::Clamp(G.Y, 0.0f, float(M.N - 1));
			const int32 I0 = FMath::Min(int32(X), M.N - 2), J0 = FMath::Min(int32(Y), M.N - 2);
			const float FX = X - I0, FY = Y - J0;
			auto S1 = [&M](int32 A, int32 B) { return M.Basin[M.Index(A, B)] == 1 ? 1.0f : 0.0f; };
			Salt = FMath::Lerp(FMath::Lerp(S1(I0, J0), S1(I0 + 1, J0), FX), FMath::Lerp(S1(I0, J0 + 1), S1(I0 + 1, J0 + 1), FX), FY);
			SaltSoft = FMath::SmoothStep(0.0f, 0.35f, Salt);
			Salt = FMath::SmoothStep(0.3f, 0.7f, Salt + 0.25f * WorldNoise::Fbm(P.X / 4.0f, P.Y / 4.0f, 929u, 3));
		}
		// terra rachada: leitos secos (margem dos lagos e por onde a água escorre).
		// No deserto a camada "rachada" é a crosta da salina; o leito seco (wadi)
		// vira cascalho, abaixo, e a margem do oásis fica areia
		// (o leito pelo M.Bed, o fundo marcado pelo gerador; pelo fluxo cru cada
		// fio d'água virava um risco de gelo ou de lama, 07/10)
		float Cr = GWinter || GForest
			? FMath::Clamp(FMath::Max(Shore, FMath::Max(GWinter ? WadiAt(M, P) : 0.0f, FMath::SmoothStep(0.8f, 0.95f, Flow))) + 0.15f * Fine, 0.0f, 1.0f) * (1.0f - Rk) * (1.0f - Dr)
			: Salt * (1.0f - Rk);
		// borda da salina: faixa larga de terra úmida (a crosta afina e o chão
		// escurece antes da areia)
		const float SaltRim = (GWinter || GForest) ? 0.0f : FMath::Clamp(SaltSoft * (1.0f - Salt), 0.0f, 1.0f) * 0.8f;
		const float Wadi = GWinter || GForest ? 0.0f : FMath::Clamp(WadiAt(M, P) + 0.15f * Fine, 0.0f, 1.0f) * (1.0f - Salt);
		// cascalho ("pavimento do deserto"): manchas de ~45 m, ~15% do chão (o
		// Arabia do AoE2 tem ~13% de manchas de textura), com a borda recortada
		// em ~8 m. A borda larga e lisa de antes (0,05–0,45 do ruído) lia como
		// mancha de sujeira vista de cima (06/10); e o pé das encostas
		// Na neve fica a mancha suave de antes, mais fraca (neve suja, cascalho
		// aparecendo): com a borda dura virava mancha marrom no branco (06/10)
		const float Ragged = WorldNoise::Fbm(P.X / 8.0f, P.Y / 8.0f, 925u, 3);
		// (deserto: borda larga e suave, sem o contorno duro de "camuflagem",
		// revisão de 07/10; a mancha só mexe no tom, quem tem pedra é o tálus)
		// Na neve, o cascalho só aparece onde o vento varre a neve: cristas e
		// bordas convexas (o vento tira dos expostos e deposita nos abrigados;
		// Winstral et al. 2002, o índice Sx; Liston & Sturm 1998). A mancha de
		// ruído no plano lia como sujeira cinza no branco (revisão de 07/10)
		float Pavement;
		if (GWinter)
		{
			float Avg = 0.0f;
			for (int32 K = 0; K < 6; ++K)
			{
				const float A = K * (UE_TWO_PI / 6.0f);
				Avg += M.HeightAt(P + 7.0f * FVector2f(FMath::Cos(A), FMath::Sin(A)));
			}
			const float Conv = M.HeightAt(P) - Avg / 6.0f;
			Pavement = 0.7f * FMath::SmoothStep(0.25f, 0.9f, Conv + 0.25f * Fine + 0.1f * Ragged);
		}
		else
		{
			Pavement = 0.8f * FMath::SmoothStep(0.05f, 0.5f, Patch + 0.3f * Lush - 0.15f + 0.1f * Fine + 0.06f * Ragged);
		}
		// (no leito, só um pouco de cascalho: o fundo é areia lavada, clareada pelo
		// tom do chão; com cascalho cheio o leito ficava da cor do chão e sumia na
		// vista geral, 07/10)
		float Gv = FMath::Clamp(Pavement * (1.0f - Wadi) + 0.6f * FMath::SmoothStep(0.05f, 0.3f, Rock) + 0.3f * Wadi, 0.0f, 1.0f) * (1.0f - SaltRim)
			* (1.0f - Rk) * (1.0f - Dr) * (1.0f - Cr);
		if (GForest)
		{
			// folhiço sob a copa, com a borda do bosque quebrada pelo ruído fino
			// só onde a mata fecha (árvore solta ganhava um disco laranja de folhas)
			// (borda larga e quebrada: a de 0,05–0,35 recortava um decalque marrom
			// em volta de cada bosque, revisão de 07/10)
			// folhiço só onde a copa fecha e faz sombra (a folha acumula ali), em
			// manchas de ~15 m: às vezes chega à borda do bosque, às vezes não. Com a
			// borda larga em volta de todo bosque virava um anel igual, como contorno
			// de seleção (revisão de 07/10)
			const float LeafPatch = WorldNoise::Fbm(P.X / 15.0f, P.Y / 15.0f, 955u, 3);
			Gv = FMath::SmoothStep(0.1f, 0.5f, WoodAt(M, P) + 0.3f * LeafPatch + 0.1f * Fine + 0.06f * Ragged) * (1.0f - Rk) * (1.0f - Dr) * (1.0f - Cr);
		}
		Gv += PadGravel;
		Dr = FMath::Max(Dr, SaltRim * (1.0f - Cr));
		float Sd = FMath::Max(0.0f, 1.0f - Rk - Dr - Cr - Gv);
		const float Sum = Sd + Gv + Dr + Rk + Cr;
		W[0] = Sd / Sum; W[1] = Gv / Sum; W[2] = Dr / Sum; W[3] = Rk / Sum; W[4] = Cr / Sum; W[5] = 0.0f;
		// oásis: terra úmida na margem (o capim verde vem por cima, OasisGrass)
		if (!GForest && !GWinter)
		{
			const float Oa = OasisDampAt(M, P);
			if (Oa > 0.0f)
			{
				for (int32 K = 0; K < 5; ++K) W[K] *= (1.0f - Oa);
				W[2] += Oa;
			}
		}
		// terra/cascalho na base das rochas, com a borda quebrada pelo ruído fino
		// (na floresta a terra da trilha; no resto o cascalho)
		// (borda quebrada em ~1,5 m e mais fraca: com a borda lisa lia como um
		// disquinho marrom em volta de cada pedra, 07/10)
		const float Crumb = WorldNoise::Fbm(P.X / 1.5f, P.Y / 1.5f, 927u, 3);
		// (sem anel na salina: a pedra não vai lá, e o anel sozinho virava manchinha
		// marrom solta na crosta, revisão de 07/10)
		const float Ring = 0.6f * FMath::SmoothStep(0.25f, 0.6f, RockRingAt(M, P) + 0.35f * Crumb + 0.1f * Fine) * (1.0f - FMath::SmoothStep(0.05f, 0.2f, Salt));
		if (Ring > 0.0f)
		{
			for (int32 K = 0; K < 5; ++K) W[K] *= (1.0f - Ring);
			W[GForest ? 2 : 1] += Ring;
		}
		if (GWinter)
		{
			// bases e campos na neve: neve pisada (lama, cascalho e neve), como
			// a estrada de terra; lama pura era mancha marrom no branco (06/10)
			// (mais neve que lama: com 40% de lama o campo lia como mancha cinza suja
			// vista de cima, revisão de 07/10; as marcas de pneu dão a leitura de uso)
			const float Packed = W[2];
			W[2] = 0.28f * Packed;
			W[1] += 0.17f * Packed;
			W[0] += 0.55f * Packed;
			// mar de inverno: o fundo e a faixa que a onda lava (até ~1 m acima da
			// água) ficam cascalho e pedra, sem neve nem gelo; a neve pintada no
			// fundo virava manchas verde-limão sob a água (07/10)
			if (SeaAt(M, P))
			{
				// (pedra fria mais que cascalho, e só até ~0,7 m: o cascalho molhado
				// numa faixa larga lia como barro marrom na costa)
				const float Wash = 1.0f - FMath::SmoothStep(M.WaterLevel + 0.2f, M.WaterLevel + 0.7f + 0.3f * Fine, M.HeightAt(P));
				// (a lama da neve pisada da margem também: era a faixa marrom da costa)
				const float Moved = (W[0] + W[2] + W[4]) * Wash;
				W[0] *= 1.0f - Wash;
				W[2] *= 1.0f - Wash;
				W[4] *= 1.0f - Wash;
				// embaixo d'água cascalho (a rocha triplanar no fundo plano repetia em
				// "tijolos" vista pela água clara); na faixa lavada, pedra fria
				const float Under = 1.0f - FMath::SmoothStep(M.WaterLevel - 0.3f, M.WaterLevel + 0.1f, M.HeightAt(P));
				// (na faixa lavada mais cascalho gelado, cinza; a rocha da foto do
				// deserto, mesmo tingida de frio, ficava marrom-barro molhada)
				const float RockShare = FMath::Lerp(0.3f, 0.2f, Under);
				W[1] += (1.0f - RockShare) * Moved;
				W[3] += RockShare * Moved;
			}
		}
		const FRoadHit Hit = RoadAt(P);
		if (Hit.Road != INDEX_NONE)
		{
			const float Half = Hit.Half();
			const float Edge = 0.35f * WorldNoise::Fbm(P.X / 2.5f, P.Y / 2.5f, 921u, 3);
			// talude (o corte ou o aterro da estrada, SliceHeight): onde passa de
			// ~40°, rocha, que é projetada dos três lados; a areia de cima esticava
			// na parede e fazia uma mancha clara na passagem das rampas (06/10)
			// (o talude é o do RoadCut: o acostamento plano e a rampa que estica com o desnível, 13/10)
			float Flat, BankLen, Drop;
			RoadCut(M, P, Hit, Flat, BankLen, Drop);
			const float Grade = FMath::Abs(M.HeightAt(P) - Hit.Z) * 1.5f / BankLen;
			const float Bank = FMath::SmoothStep(0.7f, 1.1f, Grade) * FMath::SmoothStep(Flat - 0.2f, Flat + 0.6f, Hit.D)
				* (1.0f - FMath::SmoothStep(Flat + BankLen, Flat + BankLen + 1.5f, Hit.D));
			for (int32 K = 0; K < 5; ++K) W[K] *= (1.0f - Bank);
			W[3] += Bank;
			if (Hit.Paved())
			{
				// asfalto com borda gasta e buracos (o chão de baixo aparece), e o
				// acostamento de terra/cascalho dos dois lados. Na ponta dentro da
				// base o asfalto se desfaz em buracos até virar terra (sem "toco")
				// (45 m afinando até 70% da largura: em 25 m e na largura cheia a
				// ponta lia como toco esburacado, 06/10)
				// In: metros até a beira (< 0 dentro da pista), com a beira quebrada, os buracos, a ponta que se
				// desfaz e a esquina arredondada com a outra pista (PavedEdge, a mesma da laje)
				float In, EdgeP, Holes, Ends;
				PavedEdge(P, Hit, In, EdgeP, Holes, Ends);
				// a sujeira que entra na pista (16/10: em trechos, do chão do lado, RoadDirtAt; antes eram línguas de areia
				// pela estrada toda). Debaixo da pista os pesos são os do chão em volta: tirando o asfalto, é ele que aparece
				const float Tongue = SlabOn(Hit) ? 0.0f : RoadDirtAt(M, P, Hit, In).Film;
				const float Tn = 0.45f * Tongue;
				// borda com 1,2 m de transição: mais fina que o espaçamento do terreno
				// (0,5 m) a borda virava escadinha na diagonal (visto na neve em 06/10)
				const float A = (1.0f - FMath::SmoothStep(-0.6f, 0.6f, In)) * (1.0f - Holes) * FMath::SmoothStep(0.0f, 0.25f, Ends) * (1.0f - 0.85f * Tongue);
				// o acostamento: uma faixa curta de cascalho e terra (a maquete tem a areia até a
				// beira; os 3 m de terra de antes liam como outra estrada em volta da pista)
				// (onde o vento sopra a língua, a areia cobre o acostamento também)
				const float Sh = 0.55f * (1.0f - A) * (1.0f - 0.7f * FMath::SmoothStep(-0.05f, 0.45f, Tn)) * (1.0f - FMath::SmoothStep(0.2f, 2.0f, In + 0.8f * EdgeP));
				for (int32 K = 0; K < 5; ++K) W[K] *= (1.0f - A - Sh);
				W[1] += Sh * 0.6f;
				W[2] += Sh * 0.4f;
				W[5] = A;
			}
			else
			{
				// estrada de terra: a camada de trilha (terra batida, lama) com a
				// beira irregular de chão pisado, sem acostamento. Na neve, neve
				// pisada: um pouco de lama e cascalho, o resto neve (lama pura era um
				// traço preto no branco, 06/10); as marcas de pneu escurecem os sulcos
				const float Dt = 1.0f - FMath::SmoothStep(Half - 0.8f, Half + 0.9f, Hit.D + 2.0f * Edge);
				for (int32 K = 0; K < 5; ++K) W[K] *= (1.0f - Dt);
				if (GWinter)
				{
					W[2] += 0.35f * Dt;
					W[1] += 0.2f * Dt;
					W[0] += 0.45f * Dt;
				}
				else
				{
					W[2] += Dt;
				}
			}
		}
		// a vila: terra batida com cascalho nos lotes e um chão pisado contínuo em volta, até
		// ~5–16 m dos pads, com a borda de ruído largo (9 m) e fino (2 m). Só os retângulos dos
		// lotes deixavam cunhas de areia pura em quina reta entre eles, que com a areia v1 (mais
		// clara que a terra, como na maquete do Look) liam como manchas (12/10)
		// (13/10, a vila no padrão da maquete do Look: a areia vai até o muro; o chão pisado fica
		// em manchas dentro dos lotes e numa faixa curta em volta, não no lote inteiro, que lia
		// como um tapete de terra em volta de cada casa)
		const float VD = GPads.Num() ? VillageDistAt(P) : 1e9f;
		if (VD < 24.0f)
		{
			const float Wide = WorldNoise::Fbm(P.X / 9.0f, P.Y / 9.0f, 963u, 3);
			const float Grain = FMath::Clamp(WorldNoise::Fbm(P.X / 2.0f, P.Y / 2.0f, 961u, 2), -1.0f, 1.0f);
			const float TrodPatch = FMath::SmoothStep(-0.05f, 0.35f, Wide + 0.25f * Grain);
			const float Yard = 0.6f * TrodPatch * (1.0f - FMath::SmoothStep(0.0f, 7.0f, VD + 3.0f * Wide + 1.5f * Grain));
			const float A = FMath::Clamp(Yard, 0.0f, 1.0f) * (1.0f - W[5]);
			for (int32 K = 0; K < 5; ++K) W[K] *= 1.0f - A;
			W[2] += 0.7f * A;
			W[1] += 0.3f * A;
		}
		if (GDunes)
		{
			// a duna é areia: o chão debaixo dela vira a camada de areia (16/10)
			const float A = DunePaintAt(M, P);   // (o pé largo; pela altura trocava a camada em meio metro)
			if (A > 0.0f)
			{
				for (int32 K = 0; K < 6; ++K) W[K] *= 1.0f - A;
				W[0] += A;
			}
		}
		if (GWallSand)
		{
			const float A = FMath::SmoothStep(0.03f, 0.12f, WallSandAt(M, P));
			if (A > 0.0f)
			{
				for (int32 K = 0; K < 6; ++K) W[K] *= 1.0f - A;
				W[0] += A;
			}
		}
		// por último, a pintura à mão (FWorldEdits::Paint): a camada pintada toma
		// o lugar das outras na medida da pincelada
		if (Edits.IsValid() && Edits->Paint.Num())
		{
			int32 Layer;
			const float A = Edits->PaintAt(P, Layer);
			if (A > 0.0f && Layer >= 0 && Layer < 6)
			{
				for (int32 K = 0; K < 6; ++K) W[K] *= 1.0f - A;
				W[Layer] += A;
			}
		}
	}

	/** Altura do recorte (m): o relevo do gerador + ondulação baixa da areia (dunas de ~6 e 20 m). */
	float SliceHeight(const FWorldMap& M, FVector2f P, bool bIce = true)
	{
		// (os pesos pela grade de 1 m quando há: a altura é pedida milhões de
		// vezes, no assentamento das pedras e no Landscape)
		float W[6];
		ScatterWeights(M, P, W);
		float H = M.HeightAt(P);
		// gelo plano 5 cm acima da cota d'água (no lago; o mar fica água): na cota
		// exata brigava com o plano de água, que agora cobre o mapa todo
		if (bIce && GFrozen && !SeaAt(M, P)) H = FMath::Max(H, M.WaterLevel + 0.05f);
		// ondulação na camada base: dunas no deserto; na neve e na grama só um
		// rolado largo (a de 6 m, com o sol baixo, virava "caixa de ovo" na neve)
		if (GWinter || GForest) H += W[0] * 0.18f * WorldNoise::Fbm(P.X / 28.0f, P.Y / 28.0f, 919u, 2);
		else
		{
			// deserto: marolas de vento (cristas transversais ao vento, ~35 m, alongadas
			// ao longo dele) com até ~0,7 m: de 55° dão leitura de relevo com o sol
			// baixo sem atrapalhar o jogo; o areal liso não tinha escala média (07/10)
			const float U = 0.94f * P.X + 0.34f * P.Y, Vv = -0.34f * P.X + 0.94f * P.Y;   // vento de ~20°
			const float Dune = 1.0f - FMath::Abs(WorldNoise::Fbm(U / 34.0f, Vv / 95.0f, 933u, 3));
			if (GDunes) H += DuneAt(M, P);   // (16/10: × o peso da areia cortava a duna em degrau de 2 m na borda de outra camada, até 70°; a duna pinta o chão de areia no DesertWeights)
			// (16/10 à tarde) debaixo da duna o relevo da areia de base fica enterrado (80% a menos, pelo pé largo)
			const float Buried = GDunes ? 1.0f - 0.8f * DunePaintAt(M, P) : 1.0f;
			H += W[0] * Buried * (0.12f * WorldNoise::Fbm(P.X / 6.0f, P.Y / 6.0f, 917u, 3) + 0.25f * WorldNoise::Fbm(P.X / 20.0f, P.Y / 20.0f, 919u, 2)
				+ 0.7f * FMath::Square(Dune) * FMath::SmoothStep(-0.3f, 0.3f, WorldNoise::Fbm(P.X / 160.0f, P.Y / 160.0f, 935u, 2)));
		}
		const FRoadHit Hit = RoadAt(P);
		if (Hit.Road != INDEX_NONE)
		{
			// a estrada aplaina o chão (corte e aterro): o leito e o acostamento planos, o talude que estica com o
			// desnível (RoadCut, 13/10)
			float Flat, Bank, Drop;
			RoadCut(M, P, Hit, Flat, Bank, Drop);
			H = FMath::Lerp(H, Hit.Z, 1.0f - FMath::SmoothStep(Flat, Flat + Bank, Hit.D));
			if (SlabOn(Hit))
			{
				// a laje (14/10): a areia 6 cm abaixo do asfalto e os montes encostados (some no fim do acostamento);
				// debaixo do miolo (a 1–1,6 m da beira pra dentro, longe dos buracos), o chão a 1,5 cm do topo da laje
				const FSlabSample S = SlabSample(M, P, Hit);
				const float Near = 1.0f - FMath::SmoothStep(Flat - 1.0f, Flat, Hit.D);
				const float Dd = FMath::Max(-S.In, 0.0f);
				const float Under = (SlabThick - 0.015f + SlabCrown * FMath::SmoothStep(0.0f, 4.0f, Dd))
					* FMath::SmoothStep(1.0f, 1.6f, Dd) * FMath::SmoothStep(0.6f, 0.9f, S.A);
				H += FMath::Max((S.Drift - SlabSandDrop) * Near, Under);
			}
		}
		H = PadHeight(P, H);   // o lote da vila: piso plano
		if (GWallSand) H += WallSandAt(M, P);
		return H;
	}

	/** Um lugar sorteado pelo espalhamento, antes de assentar a malha (SliceScatterMeshes). */
	struct FScatterCand
	{
		FVector2f P;
		FVector2f Id = FVector2f::ZeroVector;    // o ponto sorteado: a chave das edições (P muda se a peça foi movida à mão)
		bool bMoved = false;
		float Yaw = 0.0f, Scale = 1.0f, Sink = 0.0f, Tilt = 0.0f;
		uint32 Seed = 0;
		FVector2f Up = FVector2f::ZeroVector;   // paredão: direção morro acima
		float Drop = 0.0f;                       // paredão: desnível (m)
	};

	/** Só um pedaço (o que o pincel mexeu): o sorteio é por célula (semente da
	 *  célula), então refazer o pedaço dá as mesmas peças que o mapa inteiro daria. */
	bool bScatterRegion = false;
	FBox2f ScatterRegion = FBox2f(ForceInit);

	/** Os lugares de um tipo de espalhamento (pedra, capim, árvore, paredão...)
	 *  no recorte, antes de assentar a malha. As regras de cada tipo estão aqui. */
	void ScatterCandidates(const FString& K, float SpacingM, float ScaleMin, float ScaleMax, float SinkM, float YawOffset, TArray<FScatterCand>& OutCands)
	{
		if (!GMap.IsValid())
		{
			return;
		}
		const FWorldMap& M = *GMap;
		const uint32 Seed = WorldNoise::Hash(GetTypeHash(K));
		FVector2f CliffUp = FVector2f::ZeroVector;
		float CliffDrop = 0.0f;
		bool bVillagePalm = false;   // a palmeira do quintal (essa nasce dentro do lote)
		auto Add = [&](FVector2f P, float Yaw, float Scale, float Sink, float Tilt, WorldNoise::FRng& Rng)
		{
			const FVector2f Id = P;
			if (bScatterRegion && !ScatterRegion.IsInside(Id)) return;
			if (Edits.IsValid() && Edits->IsDeleted(K, Id)) return;   // apagado à mão (FWorldEdits)
			// lote da vila: árvore e pedra ficam fora do lote e da rua; o miúdo (capim, moita, pedrinha,
			// graveto) vai até o pé do muro e da casa, como na maquete do Look (13/10)
			if (!bVillagePalm && (K.StartsWith(TEXT("Tree")) || K.StartsWith(TEXT("Rock")) ? InVillage(Id, 1.5f) : OnVillagePiece(Id, 0.4f))) return;
			// movida à mão: o lugar e o giro novos; o que o jogador pôs ali fica
			// (sem as regras de pista, salina e fundo d'água)
			const FWorldEdits::FMoved* Mv = Edits.IsValid() ? Edits->FindMoved(K, Id) : nullptr;
			if (Mv)
			{
				P = Mv->To;
				Yaw += Mv->Yaw;
			}
			const FVector2f L = P - GCenter;
			if (FMath::Abs(L.X) > GHalf - 1.0f || FMath::Abs(L.Y) > GHalf - 1.0f) return;
			if (!Mv)
			{
				const FWorldDetail::FRoadHit Hit = RoadAt(P);
				if (Hit.Road != INDEX_NONE)
				{
					// pista limpa. O que o jogo conta como obstáculo (pedra, paredão, árvore e palmeira: Maps.json
					// obstacle_meshes) fica fora do leito plano da estrada e mais 3 m (13/10, a grade de passagem do jogo:
					// a pedra e a palmeira apertavam o corredor entre o lago e o maciço); o miúdo só fora da pista. A
					// palmeira do quintal da vila fica (o lote já é da vila)
					const bool bBlocker = !bVillagePalm && (K.StartsWith(TEXT("Rock")) || K == TEXT("Cliff") || K.StartsWith(TEXT("Tree")) || K.Contains(TEXT("Palm")));
					float Flat = 0.0f, Bank = 0.0f, Drop = 0.0f;
					if (bBlocker) RoadCut(M, P, Hit, Flat, Bank, Drop);
					const float Clear = bBlocker ? Flat + (Hit.Paved() ? 3.0f : 1.0f) : Hit.Half() + (K == TEXT("Pebble") ? 0.3f : 1.8f);
					if (Hit.D < Clear) return;
				}
				if (K != TEXT("Cliff") && SaltShapeAt(M, P).Crust > 0.2f) return;   // salina limpa
				// debaixo d'água só as pedras grandes (o miúdo virava confete ciano no
				// fundo do mar, 07/10)
				if (K != TEXT("RockLarge") && K != TEXT("Cliff") && M.WaterLevel > -100.0f && M.HeightAt(P) < M.WaterLevel - 0.2f) return;
			}
			FScatterCand& C = OutCands.AddDefaulted_GetRef();
			C.P = P;
			C.Id = Id;
			C.bMoved = Mv != nullptr;
			C.Yaw = Yaw + YawOffset;
			C.Scale = Scale;
			C.Sink = Sink + SinkM;
			C.Tilt = Tilt;
			C.Seed = Rng.Next();
			C.Up = CliffUp;
			C.Drop = CliffDrop;
		};
		auto CellLo = [&](int32 Axis, int32 G)
		{
			if (!bScatterRegion) return 0;
			const float Min = Axis ? ScatterRegion.Min.Y - GCenter.Y : ScatterRegion.Min.X - GCenter.X;
			return FMath::Clamp(FMath::FloorToInt((Min + GHalf) / SpacingM) - 1, 0, G);
		};
		auto CellHi = [&](int32 Axis, int32 G)
		{
			if (!bScatterRegion) return G;
			const float Max = Axis ? ScatterRegion.Max.Y - GCenter.Y : ScatterRegion.Max.X - GCenter.X;
			return FMath::Clamp(FMath::CeilToInt((Max + GHalf) / SpacingM) + 1, 0, G);
		};
		if (K == TEXT("TreeBroadleaf") || K == TEXT("TreeConifer") || K == TEXT("TreePalm"))
		{
			// as árvores do gerador, uma por uma: a mesma posição, tipo e tamanho que
			// o painel mostra (WorldTrees.h). Spacing e escala daqui não valem.
			// TreePalm: as árvores do oásis (copa) viram tamareiras, que já vêm no
			// tamanho real (8,7–18 m): só ±15% de variação
			const WorldTrees::EKind Want = K == TEXT("TreeConifer") ? WorldTrees::EKind::Conifer : WorldTrees::EKind::Broadleaf;
			const bool bPalm = K == TEXT("TreePalm");
			for (const FWorldProp& P : M.Props)
			{
				if (P.Type != EWorldPropType::Tree || WorldTrees::KindOf(M.Settings.Biome, P.Variant) != Want) continue;
				WorldNoise::FRng Rng(uint64(P.Variant) * 104729u + uint64(FMath::Abs(P.Pos.X * 31.0f + P.Pos.Y * 7.0f)));
				Add(P.Pos, P.Yaw, bPalm ? Rng.Range(0.85f, 1.15f) : WorldTrees::ScaleOf(P, M.Settings.Biome), bPalm ? 0.1f : 0.3f, bPalm ? 3.0f : 2.0f, Rng);
			}
			if (bPalm)
			{
				// as do quintal (a casa com palmeira da arte): menores, 6–12 m
				bVillagePalm = true;
				for (const FVector2f& Q : GVillagePalms)
				{
					WorldNoise::FRng Rng(uint64(FMath::Abs(Q.X * 131.0f + Q.Y * 17.0f)) + 77u);
					Add(Q, Rng.Range(0.0f, 360.0f), Rng.Range(0.6f, 0.85f), 0.1f, 3.0f, Rng);
				}
				bVillagePalm = false;
			}
			UE_LOG(LogTemp, Display, TEXT("FORGE slice scatter %s: %d"), *K, OutCands.Num());
			return;
		}
		if (K == TEXT("Undergrowth") || K == TEXT("Deadwood"))
		{
			// sub-bosque (samambaia, musgo) e madeira morta (toco, tronco caído):
			// dentro da mata e na borda dela, fora de trilha e rocha
			const int32 G = FMath::Max(1, FMath::RoundToInt(2.0f * GHalf / SpacingM));
			const bool bDead = K == TEXT("Deadwood");
			for (int32 V = CellLo(1, G); V < CellHi(1, G); ++V)
			{
				for (int32 U = CellLo(0, G); U < CellHi(0, G); ++U)
				{
					WorldNoise::FRng Rng(WorldNoise::Hash(U, V, Seed));
					const FVector2f P = GCenter + FVector2f((U + Rng.Float()) * SpacingM - GHalf, (V + Rng.Float()) * SpacingM - GHalf);
					float W[6];
					ScatterWeights(M, P, W);
					const bool bFlat = M.NormalAt(P).Z > 0.8f && M.HeightAt(P) > M.WaterLevel + 0.3f;
					const float Wood = WoodAt(M, P);
					const float Clump = WorldNoise::Fbm(P.X / 9.0f, P.Y / 9.0f, Seed + 7u, 3);
					const float Prob = (bDead ? 0.35f : 0.75f) * FMath::SmoothStep(-0.15f, 0.15f, Wood)
						* (bDead ? 1.0f : FMath::SmoothStep(-0.2f, 0.2f, Clump)) * (1.0f - W[2]) * (1.0f - W[3]) * (bFlat ? 1.0f : 0.0f);
					if (Rng.Float() >= Prob) continue;
					Add(P, Rng.Range(0.0f, 360.0f), FMath::Lerp(ScaleMin, ScaleMax, Rng.Float()), bDead ? 0.1f : 0.05f, bDead ? 4.0f : 8.0f, Rng);
				}
			}
			UE_LOG(LogTemp, Display, TEXT("FORGE slice scatter %s: %d"), *K, OutCands.Num());
			return;
		}
		if (K == TEXT("RockLarge") || K == TEXT("Shrub"))
		{
			// as posições do gerador (rochas e "árvores" do deserto viram arbustos)
			const EWorldPropType Type = K == TEXT("RockLarge") ? EWorldPropType::Rock : EWorldPropType::Tree;
			for (const FWorldProp& P : M.Props)
			{
				if (P.Type != Type) continue;
				WorldNoise::FRng Rng(uint64(P.Variant) * 7919u + uint64(FMath::Abs(P.Pos.X * 13.0f + P.Pos.Y)));
				// arbusto no lugar da árvore do deserto: o tamanho da regra das árvores (WorldTrees.h) × ScaleMin
				const float Sc = Type == EWorldPropType::Tree ? WorldTrees::ScaleOf(P, M.Settings.Biome) * ScaleMin : FMath::Lerp(ScaleMin, ScaleMax, Rng.Float());
				Add(P.Pos, P.Yaw, Sc, 0.15f, Type == EWorldPropType::Rock ? 12.0f : 4.0f, Rng);
			}
		}
		if (K == TEXT("Cliff"))
		{
			// paredões escaneados nos cortes do terreno: onde a encosta é íngreme,
			// virados pra baixo dela, um a cada ~Spacing metros ao longo do corte
			const int32 G = FMath::Max(1, FMath::RoundToInt(2.0f * GHalf / SpacingM));
			for (int32 V = CellLo(1, G); V < CellHi(1, G); ++V)
			{
				for (int32 U = CellLo(0, G); U < CellHi(0, G); ++U)
				{
					WorldNoise::FRng Rng(WorldNoise::Hash(U, V, Seed));
					// o ponto mais íngreme da célula (5×5 amostras): com um ponto só por
					// célula, metade das células errava a faixa do corte (3–6 m de largura)
					// e a parede do terreno aparecia entre os paredões
					const float E = 1.0f;
					auto GradAt = [&M, E](FVector2f Q)
					{
						return FVector2f((M.HeightAt(Q + FVector2f(E, 0)) - M.HeightAt(Q - FVector2f(E, 0))) / (2 * E),
							(M.HeightAt(Q + FVector2f(0, E)) - M.HeightAt(Q - FVector2f(0, E))) / (2 * E));
					};
					FVector2f P = FVector2f::ZeroVector, Grad = FVector2f::ZeroVector;
					for (int32 SJ = 0; SJ < 5; ++SJ)
					{
						for (int32 SI = 0; SI < 5; ++SI)
						{
							const FVector2f Q = GCenter + FVector2f((U + (SI + 0.5f) / 5.0f) * SpacingM - GHalf, (V + (SJ + 0.5f) / 5.0f) * SpacingM - GHalf);
							const FVector2f GQ = GradAt(Q);
							if (GQ.SizeSquared() > Grad.SizeSquared()) { Grad = GQ; P = Q; }
						}
					}
					if (Grad.Size() < 1.0f) continue;   // mais de ~45°: corte de platô
					// parede de verdade: desnível de 1,8 m ou mais em 8 m na direção da
					// encosta (lombada e beira de rampa davam uma "barbatana" de rocha
					// em pé no chão plano); o paredão cresce com o desnível (~7 m = 1)
					const FVector2f Up = Grad.GetSafeNormal();
					const float Drop = M.HeightAt(P + 4.0f * Up) - M.HeightAt(P - 4.0f * Up);
					if (Drop < 1.8f) continue;
					const FWorldDetail::FRoadHit Road = RoadAt(P);
					if (Road.Road != INDEX_NONE && Road.D < Road.Half() + 3.0f) continue;   // não na pista
					// parede de verdade também pra cima: o chão continua alto depois da
					// borda (numa crista fina ou na beira de um buraco a malha em pé
					// mostrava o vão embaixo)
					if (M.HeightAt(P + 9.0f * Up) < M.HeightAt(P + 4.0f * Up) - 1.0f) continue;
					P += FVector2f(Rng.Range(-0.1f, 0.1f), Rng.Range(-0.1f, 0.1f)) * SpacingM;
					const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Grad.Y, -Grad.X));
					CliffUp = Up;
					CliffDrop = Drop;
					Add(P, Yaw, FMath::Lerp(ScaleMin, ScaleMax, Rng.Float()) * FMath::Clamp(Drop / 7.0f, 0.45f, 1.1f), 0.0f, 3.0f, Rng);
				}
			}
			UE_LOG(LogTemp, Display, TEXT("FORGE slice scatter %s: %d"), *K, OutCands.Num());
			return;
		}
		if (K == TEXT("OasisGrass"))
		{
			// capim verde fechado na margem do oásis (fora d'água)
			const int32 G = FMath::Max(1, FMath::RoundToInt(2.0f * GHalf / SpacingM));
			for (int32 V = CellLo(1, G); V < CellHi(1, G); ++V)
			{
				for (int32 U = CellLo(0, G); U < CellHi(0, G); ++U)
				{
					WorldNoise::FRng Rng(WorldNoise::Hash(U, V, Seed));
					const FVector2f P = GCenter + FVector2f((U + Rng.Float()) * SpacingM - GHalf, (V + Rng.Float()) * SpacingM - GHalf);
					const float Oa = OasisAt(M, P);
					if (Oa <= 0.05f || M.HeightAt(P) < M.WaterLevel + 0.15f) continue;
					if (Rng.Float() >= 0.95f * FMath::SmoothStep(0.05f, 0.5f, Oa)) continue;
					Add(P, Rng.Range(0.0f, 360.0f), FMath::Lerp(ScaleMin, ScaleMax, Rng.Float()), 0.02f, 6.0f, Rng);
				}
			}
			UE_LOG(LogTemp, Display, TEXT("FORGE slice scatter %s: %d"), *K, OutCands.Num());
			return;
		}
		if (K == TEXT("RockSmall"))
		{
			// em grupo: 3–6 pedras menores encostadas em cada pedra grande do gerador
			// (uma grande + médias + pequenas, como a rocha se parte e rola)
			for (const FWorldProp& Pr : M.Props)
			{
				if (Pr.Type != EWorldPropType::Rock) continue;
				WorldNoise::FRng Rng(uint64(Pr.Variant) * 2654435761u + uint64(FMath::Abs(Pr.Pos.X * 17.0f + Pr.Pos.Y * 3.0f)));
				const int32 Count = 3 + int32(Rng.Next() % 4u);
				for (int32 I = 0; I < Count; ++I)
				{
					const float A = Rng.Range(0.0f, 2.0f * PI), R = Rng.Range(1.4f, 3.0f) * FMath::Max(Pr.Scale, 0.6f);
					Add(Pr.Pos + R * FVector2f(FMath::Cos(A), FMath::Sin(A)), Rng.Range(0.0f, 360.0f),
						FMath::Lerp(ScaleMin, ScaleMax, Rng.Range(0.3f, 0.9f)), 0.2f, 5.0f, Rng);
				}
			}
		}
		// grade com sorteio por célula, densidade pelas camadas
		const bool bRockKind = K.StartsWith(TEXT("Rock")) || K == TEXT("Pebble");
		const int32 G = FMath::Max(1, FMath::RoundToInt(2.0f * GHalf / SpacingM));
		for (int32 V = CellLo(1, G); V < CellHi(1, G); ++V)
		{
			for (int32 U = CellLo(0, G); U < CellHi(0, G); ++U)
			{
				WorldNoise::FRng Rng(WorldNoise::Hash(U, V, Seed));
				const FVector2f P = GCenter + FVector2f((U + Rng.Float()) * SpacingM - GHalf, (V + Rng.Float()) * SpacingM - GHalf);
				float W[6];
				ScatterWeights(M, P, W);
				const FVector3f N = M.NormalAt(P);
				// topo plano que não passa = o alto de um pilar/monólito: pedra solta ali
				// lia como entulho em cima do bolo (fotos dos monólitos, 12/10)
				if (bRockKind && N.Z > 0.9f && M.Pass.Num() == M.N * M.N)
				{
					const FVector2f Gp = M.ToGrid(P);
					const int32 Vi = M.Index(FMath::Clamp(FMath::RoundToInt(Gp.X), 0, M.N - 1), FMath::Clamp(FMath::RoundToInt(Gp.Y), 0, M.N - 1));
					if (M.Pass[Vi] == EWorldPass::Cliff) continue;
				}
				const float Clump = WorldNoise::Fbm(P.X / 18.0f, P.Y / 18.0f, Seed + 3u, 3);
				float Prob = 0.0f;
				// (no leito: só no deserto e ralo, cascalho é a textura; o leito virava um
				// tapete de pedras, e no riacho congelado ficavam pedras por cima do gelo)
				// (concentrados no pé do barranco, onde a enxurrada larga a pedra; no
				// meio do leito, poucos: espalhados por igual liam como pontos pretos)
				const float BedW = GForest || GWinter ? 0.0f : WadiAt(M, P);
				const float BedRocks = 0.15f * BedW + 1.4f * BedW * (1.0f - BedW);
				if (K == TEXT("Pebble")) Prob = (0.08f + 0.25f * W[1] + 0.3f * W[2] + 0.6f * FMath::SmoothStep(0.05f, 0.4f, W[3])) * (1.0f - (GWinter ? WadiAt(M, P) : 0.0f)) + 0.25f * BedRocks;
				// (sem o 0,05 de fundo: pedregulho solto no gramado, sem motivo, lia
				// como confete; fica no cascalho e no pé da rocha)
				// pedra solta onde a gravidade e a água levam: pé das escarpas e tálus
				// (camada de rocha), leitos secos (fluxo) e um pouco no cascalho. No
				// plano aberto, densidade baixa e gradual: as manchas de cascalho
				// cheias de pedra liam como "pele de onça" (revisão de arte de 07/10)
				else if (K == TEXT("RockSmall")) Prob = (0.06f * W[1] + 0.9f * FMath::SmoothStep(0.05f, 0.4f, W[3]) + 0.12f * BedRocks) * (N.Z > 0.6f ? 1.0f : 0.2f);
				else if (K == TEXT("Tuft") && GForest) Prob = (0.25f + 0.75f * FMath::SmoothStep(-0.35f, 0.25f, Clump)) * (0.9f * W[0] + 0.2f * W[1] + 0.12f * W[2]) * (1.0f - 0.7f * W[2]) * (N.Z > 0.8f ? 1.0f : 0.0f);
				// capim seco: nos leitos, no cascalho e no pé das pedras (onde a água
				// para); no areal aberto quase nada: de 55° os tufos espalhados liam como
				// risquinhos escuros por todo o plano (revisão de arte de 07/10)
				else if (K == TEXT("Tuft")) Prob = FMath::SmoothStep(-0.05f, 0.35f, Clump) * (0.03f * W[0] + 0.5f * W[1] + 0.8f * WadiAt(M, P) + 0.4f * FMath::SmoothStep(0.05f, 0.3f, W[3])) * (1.0f - W[2]) * (N.Z > 0.85f ? 1.0f : 0.0f);
				// (e ao longo dos leitos secos: a água que passa ali sustenta mais
				// arbusto que o chão em volta, a mata ciliar do deserto)
				else if (K == TEXT("Shrub")) Prob = (0.25f * FMath::SmoothStep(0.1f, 0.5f, Clump) * (W[0] + W[1]) * (1.0f - W[2]) + (GForest || GWinter ? 0.0f : 0.12f * WadiAt(M, P)))
					* (N.Z > 0.85f ? 1.0f : 0.0f);
				else if (K == TEXT("Branch")) Prob = (GForest ? 0.05f * (W[0] + W[1]) : 0.004f + 0.08f * WadiAt(M, P)) * (N.Z > 0.85f ? 1.0f : 0.0f);
				// o deserto pelas zonas do chão (as fórmulas do Look, sand_desert_v2.json, divididas pela média delas
				// num campo sem terreno: a quantidade total fica a de antes, só muda o lugar). O capim se junta no
				// pisado e some no liso e no ondulado; as pedrinhas e os gravetos se juntam no pedregoso
				if (!GForest && !GWinter && Prob > 0.0f && (K == TEXT("Tuft") || K == TEXT("Pebble") || K == TEXT("Branch")))
				{
					const FVector4f Zn = ZonesAt(M, P, W);
					const float Pis = FMath::Clamp(1.0f - Zn.X - Zn.Y - Zn.Z - Zn.W, 0.0f, 1.0f);
					if (K == TEXT("Tuft")) Prob *= (0.15f + 1.6f * Pis + 0.4f * Zn.Z) * (1.0f - 0.85f * Zn.X) * (1.0f - 0.9f * Zn.Y) / 0.81f;
					else if (K == TEXT("Pebble")) Prob *= (0.12f + 3.2f * Zn.Z + 0.4f * Zn.W) / 0.65f;
					else Prob *= (0.2f + 2.4f * Zn.Z + 0.8f * Zn.W + 0.4f * Pis) / 0.80f;
				}
				if (Rng.Float() >= FMath::Clamp(Prob, 0.0f, 1.0f)) continue;
				const float Sc = FMath::Lerp(ScaleMin, ScaleMax, FMath::Square(Rng.Float()));   // mais pequenos que grandes
				Add(P, Rng.Range(0.0f, 360.0f), Sc, K == TEXT("Tuft") ? 0.02f : 0.2f, K == TEXT("Tuft") ? 6.0f : 25.0f, Rng);
			}
		}
		UE_LOG(LogTemp, Display, TEXT("FORGE slice scatter %s: %d"), *K, OutCands.Num());
		return;
	}

	/** Assenta os lugares nas malhas (Bounds = a caixa de cada malha, cm): sorteia
	 *  a malha de cada lugar e devolve as posições (cm, relativas ao centro do
	 *  recorte) por malha. Pedra pela base medida e pela encosta; paredão em pé
	 *  ao longo da parede; o resto pela base. */
	void PlaceMeshes(const FString& K, const TArray<FScatterCand>& Cands, const TArray<FBox>& Bounds, TArray<TArray<FTransform>>& Out,
		TArray<TArray<FVector2f>>* OutOrigins = nullptr)
	{
		if (OutOrigins)
		{
			OutOrigins->Reset();
			OutOrigins->SetNum(Bounds.Num());
		}
		Out.SetNum(Bounds.Num());
		if (!GMap.IsValid() || Bounds.Num() == 0)
		{
			return;
		}
		const FWorldMap& M = *GMap;
		const bool bCliff = K == TEXT("Cliff");
		const bool bRock = K.StartsWith(TEXT("Rock")) || K == TEXT("Pebble") || K == TEXT("Deadwood");
		int32 Rejected = 0;
		auto H = [this, &M](FVector2f Q) { return SliceHeight(M, Q); };
		for (const FScatterCand& C : Cands)
		{
			int32 Vi = int32((C.Seed * 2654435761u) % uint32(Bounds.Num()));
			if (K == TEXT("TreePalm") && Bounds.Num() >= 3)
			{
				// junto da água a touceira e a madura (0, 1); mais longe a madura e a
				// adulta de 18 m (1, 2), que de cima cobriria unidade no meio da passagem
				const float U = float((C.Seed * 2246822519u) % 1000u) / 1000.0f;
				Vi = OasisAt(M, C.P) > 0.55f ? (U < 0.5f ? 0 : 1) : (U < 0.7f ? 1 : 2);
			}
			// a caixa da malha (cm, espaço local): o ponto de origem das peças de
			// conjunto fica a até 3,5 m da pedra (rock_moss_set_01), e a pedra
			// aparecia longe do lugar medido, flutuando ou afundada
			const FBox B = Bounds[Vi];
			if (!B.IsValid) continue;
			WorldNoise::FRng Rng(uint64(C.Seed) * 7u + 1u);
			const FVector Ctr = B.GetCenter(), Ext = B.GetExtent();
			const FVector2f L = C.P - GCenter;
			if (bCliff)
			{
				// paredão escaneado: comprido no X, a face no +Y. Em pé, o X ao
				// longo da parede e a face virada morro abaixo; as costas (o fundo da
				// peça) enterradas na encosta, a base no pé da parede e a altura pelo
				// desnível. Girar o X morro abaixo (como antes) deixava a peça
				// atravessada: a "barbatana" pra fora do penhasco (06/10)
				const FVector2f Down = -C.Up;
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Down.X, Down.Y));
				const float MeshH = FMath::Max(2.0f * Ext.Z / 100.0f, 0.5f);
				const float Sc = FMath::Clamp((C.Drop + 1.2f) / MeshH, 0.5f, 1.7f) * Rng.Range(0.92f, 1.1f);
				const float Foot = H(C.P + 4.0f * Down);
				const FQuat Rot = FRotator(Rng.Range(-2.0f, 2.0f), Yaw, Rng.Range(-2.0f, 2.0f)).Quaternion();
				const FVector2f Mid = L + C.Up * (0.45f * Ext.Y * Sc / 100.0f);
				const FVector Target(Mid.X * 100.0, Mid.Y * 100.0, (Foot - 0.4f) * 100.0 + (Ctr.Z - B.Min.Z) * Sc);
				Out[Vi].Add(FTransform(Rot, Target - Rot.RotateVector(Ctr * Sc), FVector(Sc)));
				if (OutOrigins) (*OutOrigins)[Vi].Add(C.Id);
				continue;
			}
			if (bRock)
			{
				// pedra: o chão medido em 7 pontos da base; desnível grande demais
				// pra assentar sem borda no ar nem chão atravessando, não entra.
				// Inclinada pela encosta (não ao acaso: as lascas espetadas no
				// gramado), pouco giro, enterrada ~25% da altura
				const float Rf = 0.8f * FMath::Max(Ext.X, Ext.Y) * C.Scale / 100.0f;
				const float Hh = 2.0f * Ext.Z * C.Scale / 100.0f;
				float Hmin = H(C.P), Hmax = Hmin;
				for (int32 I = 0; I < 6; ++I)
				{
					const float A = I * (PI / 3.0f);
					const float Hi = H(C.P + Rf * FVector2f(FMath::Cos(A), FMath::Sin(A)));
					Hmin = FMath::Min(Hmin, Hi);
					Hmax = FMath::Max(Hmax, Hi);
				}
				if (Hmax - Hmin > 0.45f * Hh + 0.15f && !C.bMoved)   // (a movida à mão fica onde o jogador pôs)
				{
					++Rejected;
					continue;
				}
				const FVector3f N3 = M.NormalAt(C.P);
				const FVector Up = FMath::Lerp(FVector::UpVector, FVector(N3.X, N3.Y, N3.Z), 0.8f).GetSafeNormal();
				const float Tilt = FMath::Min(C.Tilt, 5.0f);
				const FQuat Lean = FRotator(Rng.Range(-Tilt, Tilt), 0.0f, Rng.Range(-Tilt, Tilt)).Quaternion();
				const FQuat Rot = FQuat::FindBetweenNormals(FVector::UpVector, Up) * Lean * FQuat(FVector::UpVector, FMath::DegreesToRadians(C.Yaw));
				const float Bottom = Hmin + 0.35f * (Hmax - Hmin) - 0.25f * Hh;
				const FVector Target(L.X * 100.0, L.Y * 100.0, Bottom * 100.0 + (Ctr.Z - B.Min.Z) * C.Scale);
				Out[Vi].Add(FTransform(Rot, Target - Rot.RotateVector(Ctr * C.Scale), FVector(C.Scale)));
				if (OutOrigins) (*OutOrigins)[Vi].Add(C.Id);
				continue;
			}
			// o resto (capim, arbusto, galho): pela base, como antes
			const float Z = SliceHeight(M, C.P);
			const FRotator R(Rng.Range(-C.Tilt, C.Tilt), C.Yaw, Rng.Range(-C.Tilt, C.Tilt));
			Out[Vi].Add(FTransform(R, FVector(L.X * 100.0, L.Y * 100.0, (Z - C.Sink * C.Scale) * 100.0), FVector(C.Scale)));
			if (OutOrigins) (*OutOrigins)[Vi].Add(C.Id);
		}
		int32 Total = 0;
		for (const TArray<FTransform>& O : Out) Total += O.Num();
		UE_LOG(LogTemp, Display, TEXT("FORGE scatter %s: %d assentadas em %d malhas (%d candidatas, %d fora: chão desnivelado demais)"),
			*K, Total, Bounds.Num(), Cands.Num(), Rejected);
	}

	/** Prepara tudo a partir do mapa gerado: os tipos do bioma, as máscaras e a
	 *  rede de estradas. Center/HalfM: o recorte (o mapa inteiro: 0 e meio
	 *  lado). MainRoadHalf: meia largura da principal (asfalto se bPaved; as de
	 *  terra têm 2,2 m de meia largura). */
	void Setup(TSharedPtr<FWorldMap> Map, FVector2f Center, float HalfM, float MainRoadHalf, bool bPaved)
	{
		GMap = Map;
		// a altura à mão (FWorldEdits::Height) entra no relevo antes de tudo
		if (Edits.IsValid())
		{
			for (const TPair<FIntPoint, float>& K : Edits->Height)
			{
				if (K.Key.X >= 0 && K.Key.Y >= 0 && K.Key.X < GMap->N && K.Key.Y < GMap->N)
				{
					GMap->Height[GMap->Index(K.Key.X, K.Key.Y)] += K.Value;
				}
			}
		}
		const FWorldMap& M = *GMap;
		const EWorldBiome Biome = M.Settings.Biome;
		GForest = Biome == EWorldBiome::Temperate;
		GWinter = Biome == EWorldBiome::Winter;
		GFrozen = GWinter && M.WaterLevel > -100.0f;
		BuildCanopy(M);
		BuildRockRing(M);
		BuildOasisRing(M);
		BuildSaltField(M);
		BuildSeaMask(M);
		GCenter = Center;
		GHalf = HalfM;
		GWaterRes = 0;
		// (16/10, o Felipe pelo Look: "gostei da laje 3D, tá aprovado") a laje é o padrão do deserto; ROAD_SLAB=0 tira, e a
		// estrada que pediu "colada no chão" no painel fica sem ela (SlabOn)
		GSlab = bPaved && !GWinter && !GForest && FPlatformMisc::GetEnvironmentVariable(TEXT("ROAD_SLAB")) != TEXT("0");
		{
			const FString WS = FPlatformMisc::GetEnvironmentVariable(TEXT("WALL_SAND"));
			GWallSand = !GWinter && !GForest && (WS == TEXT("1") || WS == TEXT("2"));
			GWallSandPeak = WS == TEXT("2") ? 0.75f : 0.35f;
			GWallSandFall = WS == TEXT("2") ? 1.8f : 0.9f;
		}
		GWallGrid.Reset();
		BuildRoads(M, MainRoadHalf, bPaved, 2.2f);
		BuildOpenness(M);
		// (16/10, o Felipe: "ok ta aprovado") as dunas simuladas são o padrão do deserto; DUNE_POCKETS=0 tira
		GDunes = !GWinter && !GForest && FPlatformMisc::GetEnvironmentVariable(TEXT("DUNE_POCKETS")) != TEXT("0");
		BuildDunePockets(M);
		if (GVillage.Num()) PlaceVillagePoles(M);
		IndexWallSand();
	}

	// o deserto aberto (o Look, 13/10, pela referência nova do Felipe: longe do pisado a ondulação de vento
	// domina e se lê de longe; perto da vila e da rua é rara): 0 até ~20 m do pisado (terra batida, rua,
	// vila), 1 a partir de ~45 m. Grade de 2 m no mapa inteiro, feita uma vez por mapa. Vai no G da textura
	// de informação do mapa (em terra) e no B dos pesos da vista ao vivo; as zonas somam +0,25 no ondulado
	TArray<float> GOpen;
	int32 GOpenN = 0;
	static constexpr float GOpenCell = 2.0f;
	void BuildOpenness(const FWorldMap& M)
	{
		GOpen.Reset();
		GOpenN = 0;
		if (M.Settings.Biome != EWorldBiome::Desert) return;
		const float Half = M.Half();
		const int32 N = FMath::CeilToInt(2.0f * Half / GOpenCell) + 1;
		TArray<uint8> Trod;
		Trod.SetNumZeroed(N * N);
		ParallelFor(N, [&](int32 J)
		{
			for (int32 I = 0; I < N; ++I)
			{
				const FVector2f P(-Half + I * GOpenCell, -Half + J * GOpenCell);
				float W[6] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
				DesertWeights(M, P, W);
				Trod[J * N + I] = (W[2] + W[5] > 0.45f || (GPads.Num() && InVillage(P, 2.0f))) ? 1 : 0;
			}
		});
		TArray<float> Dist;
		Dist.Init(1e9f, N * N);
		TArray<int32> Queue;
		for (int32 C = 0; C < N * N; ++C) if (Trod[C]) { Dist[C] = 0.0f; Queue.Add(C); }
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 C = Queue[Head];
			const int32 I = C % N, J = C / N;
			for (int32 DJ = -1; DJ <= 1; ++DJ)
			{
				for (int32 DI = -1; DI <= 1; ++DI)
				{
					const int32 X = I + DI, Y = J + DJ;
					if ((DI | DJ) == 0 || X < 0 || Y < 0 || X >= N || Y >= N) continue;
					const int32 Q = Y * N + X;
					const float ND = Dist[C] + ((DI && DJ) ? 1.41421356f : 1.0f) * GOpenCell;
					if (ND < Dist[Q] && ND < 60.0f) { Dist[Q] = ND; Queue.Add(Q); }
				}
			}
		}
		GOpen.SetNumUninitialized(N * N);
		for (int32 C = 0; C < N * N; ++C) GOpen[C] = FMath::SmoothStep(20.0f, 45.0f, Dist[C]);
		GOpenN = N;
	}
	/** 0..1: o quanto o ponto é deserto aberto, longe do pisado (0 fora do deserto). */
	float OpenAt(FVector2f P) const
	{
		if (GOpenN < 2 || !GMap.IsValid()) return 0.0f;
		const float Half = GMap->Half();
		const float GX = FMath::Clamp((P.X + Half) / GOpenCell, 0.0f, GOpenN - 1.001f), GY = FMath::Clamp((P.Y + Half) / GOpenCell, 0.0f, GOpenN - 1.001f);
		const int32 X0 = FMath::FloorToInt(GX), Y0 = FMath::FloorToInt(GY);
		const float FX = GX - X0, FY = GY - Y0;
		auto At = [&](int32 X, int32 Y) { return GOpen[Y * GOpenN + X]; };
		return FMath::Lerp(FMath::Lerp(At(X0, Y0), At(X0 + 1, Y0), FX), FMath::Lerp(At(X0, Y0 + 1), At(X0 + 1, Y0 + 1), FX), FY);
	}

	/** A estrada principal de cada bioma no mapa inteiro (a mesma do "qualidade
	 *  real", build_slice.py): deserto 10 m e neve 6 m de asfalto; floresta de terra. */
	static void MainRoadOf(EWorldBiome Biome, float& OutHalf, bool& bOutPaved)
	{
		bOutPaved = Biome != EWorldBiome::Temperate;
		// (deserto: 7,5 → 10 m em 13/10, o Felipe: "a estrada um pouco maior", a de duas faixas das
		// favoritas tem ~1,2 tanque de largura, road_town_battle)
		OutHalf = Biome == EWorldBiome::Desert ? 5.0f : 3.0f;
	}

	/** Mar: a borda do tipo costa ou ilha (água ligada à moldura). */
	bool HasSea() const
	{
		return GMap.IsValid() && (GMap->Frame == EWorldFrame::Coast || GMap->Frame == EWorldFrame::Island);
	}

	/** Tem salina neste mapa. */
	bool HasSaltFlat() const
	{
		return GMap.IsValid() && GSaltDist.Num() == GMap->N * GMap->N && Algo::AnyOf(GSaltDist, [](float D) { return D > -23.5f; });
	}

	/** A textura da água do recorte (SizeM de lado, texel de 2 m, R×R): R =
	 *  distância com sinal até a linha d'água, G = profundidade, B = mar, A = tom
	 *  do chão (128 neutro). Usada pela água (espuma, gelo) e pelo chão (molhado,
	 *  leito lavado, terra úmida). */
	void WaterInfoPixels(float SizeM, TArray<FColor>& Pixels, int32& R)
	{
		const bool bSaltFlat = HasSaltFlat();
		const FWorldMap& M = *GMap;
		const bool bSea = HasSea();
		constexpr float Texel = 2.0f;
		R = FMath::Clamp(FMath::RoundToInt(SizeM / Texel), 16, 4096);
		// água/terra por texel (o chão do recorte, com as dunas e o leito da estrada)
		TArray<uint8> Wet;
		TArray<float> Depth;
		Wet.SetNumUninitialized(R * R);
		Depth.SetNumUninitialized(R * R);
		ParallelFor(R, [&](int32 J)
		{
			for (int32 I = 0; I < R; ++I)
			{
				const FVector2f L((I + 0.5f) * Texel - 0.5f * SizeM, (J + 0.5f) * Texel - 0.5f * SizeM);
				const FVector2f P = GCenter + L;
				float H;
				if (M.IsInside(P)) H = SliceHeight(M, P);
				else H = bSea ? M.WaterLevel - 50.0f : M.WaterLevel + 5.0f;
				Wet[J * R + I] = H < M.WaterLevel ? 1 : 0;
				Depth[J * R + I] = FMath::Max(0.0f, M.WaterLevel - H);
			}
		});
		// distância até o outro lado (água ↔ terra), BFS de 8 vizinhos em texels
		TArray<float> Dist;
		Dist.Init(1e9f, R * R);
		TArray<int32> Queue;
		for (int32 J = 0; J < R; ++J)
		{
			for (int32 I = 0; I < R; ++I)
			{
				const int32 C = J * R + I;
				bool bEdge = false;
				for (int32 DJ = -1; DJ <= 1 && !bEdge; ++DJ)
				{
					for (int32 DI = -1; DI <= 1 && !bEdge; ++DI)
					{
						const int32 X = I + DI, Y = J + DJ;
						bEdge = X >= 0 && Y >= 0 && X < R && Y < R && Wet[Y * R + X] != Wet[C];
					}
				}
				if (bEdge) { Dist[C] = 0.5f; Queue.Add(C); }
			}
		}
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 C = Queue[Head];
			const int32 I = C % R, J = C / R;
			for (int32 DJ = -1; DJ <= 1; ++DJ)
			{
				for (int32 DI = -1; DI <= 1; ++DI)
				{
					const int32 X = I + DI, Y = J + DJ;
					if ((DI | DJ) == 0 || X < 0 || Y < 0 || X >= R || Y >= R) continue;
					const int32 Q = Y * R + X;
					const float ND = Dist[C] + ((DI && DJ) ? 1.41421356f : 1.0f);
					if (ND < Dist[Q] && ND < 20.0f) { Dist[Q] = ND; Queue.Add(Q); }
				}
			}
		}
		// mar: a água ligada à borda da textura (a moldura de mar); lago fica de fora
		TArray<uint8> Sea;
		Sea.SetNumZeroed(R * R);
		if (bSea)
		{
			TArray<int32> Fill;
			for (int32 K = 0; K < R; ++K)
			{
				for (const int32 C : { K, (R - 1) * R + K, K * R, K * R + R - 1 })
				{
					if (Wet[C] && !Sea[C]) { Sea[C] = 1; Fill.Add(C); }
				}
			}
			for (int32 Head = 0; Head < Fill.Num(); ++Head)
			{
				const int32 C = Fill[Head];
				const int32 I = C % R, J = C / R;
				const int32 N4[4][2] = { { I + 1, J }, { I - 1, J }, { I, J + 1 }, { I, J - 1 } };
				for (const auto& NB : N4)
				{
					if (NB[0] < 0 || NB[1] < 0 || NB[0] >= R || NB[1] >= R) continue;
					const int32 Q = NB[1] * R + NB[0];
					if (Wet[Q] && !Sea[Q]) { Sea[Q] = 1; Fill.Add(Q); }
				}
			}
		}
		Pixels.SetNumUninitialized(R * R);
		ParallelFor(R, [&](int32 J)
		{
			for (int32 I = 0; I < R; ++I)
			{
				const int32 C = J * R + I;
				const float D = FMath::Min(Dist[C], 20.0f) * Texel * (Wet[C] ? 1.0f : -1.0f);   // m
				const FVector2f P = GCenter + FVector2f((I + 0.5f) * Texel - 0.5f * SizeM, (J + 0.5f) * Texel - 0.5f * SizeM);
				// tom do chão (A, 128 = neutro): abaixo, úmido (a lama da borda da
				// salina: escurece e alisa); acima, leito lavado (o fundo dos leitos secos
				// do deserto: areia e cascalho soltos, sem o verniz e a crosta que
				// escurecem o chão velho — o leito ativo é a faixa mais clara do deserto
				// visto do alto, como nos alvos)
				const float Tone = M.IsInside(P) ? GroundToneAt(M, P, bSaltFlat) : 0.0f;
				// G: debaixo d'água a profundidade; em terra o deserto aberto (OpenAt: a ondulação da areia v2)
				const float G = Wet[C] ? Depth[C] / 50.0f : (M.IsInside(P) ? OpenAt(P) : 0.0f);
				Pixels[C] = FColor(uint8(FMath::Clamp(0.5f + D / 64.0f, 0.0f, 1.0f) * 255.0f + 0.5f),
					uint8(FMath::Clamp(G, 0.0f, 1.0f) * 255.0f + 0.5f), Sea[C] ? 255 : 0,
					uint8(FMath::Clamp(128.0f + 127.0f * Tone, 0.0f, 255.0f) + 0.5f));
			}
		});
	}

	// ---- a água por corpo (13/10, docs/WORLDFORGE.md §12.3, a P0: só dados, a tela não muda) ----------
	// Cada lago, oásis e mar vira um corpo com id, tipo e cota (o plano global numa cota só sai na P1, com
	// a malha de cada corpo). O mapa inteiro numa grade de 1 m: o fundo (o chão sem a placa de gelo), o id
	// e a distância com sinal até a margem, com a linha d'água interpolada entre os texels (quadrados
	// marchantes): a espuma do Look é mais fina que o texel, e a distância em degraus de 1 m serrilharia.
	// Daqui saem a grade de 4 m da simulação (grid.png, o jogo) e a textura do material (T_Water, o Look).
	enum EWaterKind : uint8 { WkLake = 0, WkOasis = 1, WkRiver = 2, WkSea = 3, WkIce = 4 };
	static const TCHAR* WaterKindName(uint8 K)
	{
		static const TCHAR* Names[] = { TEXT("lake"), TEXT("oasis"), TEXT("river"), TEXT("sea"), TEXT("ice") };
		return Names[FMath::Min<int32>(K, 4)];
	}
	struct FWaterBody
	{
		int32 Id = 0;                      // 1..255 (0 = seco)
		uint8 Kind = WkLake;
		int32 LevelMm = 0;                 // a cota da superfície, mm inteiros
		FIntPoint FlowMmS = FIntPoint(0, 0);   // a correnteza (só no rio, P2)
		int32 AreaM2 = 0;
		float MaxDepth = 0.0f;             // m
	};
	/** O vau: até esta profundidade média na célula de 4 m o veículo de chão passa (devagar). O Humvee
	 *  atravessa 0,76 m sem kit, o Abrams e o T-72 1,2 m (as fichas dos fabricantes); 0,8 m é o raso que
	 *  todo veículo de chão passa. Acima é fundo: bloqueia a terra (fica pros navios). */
	static constexpr float WaterFordM = 0.8f;
	/** A mancha menor que isto não é corpo (a poça de 1 texel no fundo de uma duna). */
	static constexpr float WaterMinAreaM2 = 16.0f;
	TArray<FWaterBody> GWaterBodies;
	TArray<float> GWaterBed;      // R×R, a altura do fundo (m)
	TArray<uint8> GWaterId;       // R×R, 0 = seco
	TArray<float> GWaterSd;       // R×R, a distância com sinal até a margem (m, + na água), cortada em ±9
	int32 GWaterRes = 0;
	float GWaterTexel = 1.0f;
	float GWaterMin = 0.0f;

	void BuildWaterField(float TexelM)
	{
		GWaterBodies.Reset();
		GWaterRes = 0;
		if (!GMap.IsValid() || TexelM <= 0.0f) return;
		const FWorldMap& M = *GMap;
		const float Half = M.Half();
		const int32 R = FMath::Clamp(FMath::CeilToInt(2.0f * Half / TexelM - 0.001f), 16, 4096);
		const float T = TexelM;
		const float L = M.WaterLevel;
		GWaterBed.SetNumUninitialized(R * R);
		ParallelFor(R, [&](int32 J)
		{
			for (int32 I = 0; I < R; ++I)
			{
				GWaterBed[J * R + I] = SliceHeight(M, FVector2f(-Half + (I + 0.5f) * T, -Half + (J + 0.5f) * T), false);
			}
		});
		GWaterId.Init(0, R * R);
		GWaterSd.Init(-9.0f, R * R);
		GWaterRes = R;
		GWaterTexel = T;
		GWaterMin = -Half;
		if (L <= -100.0f) return;
		// os corpos: as manchas de água pelos 4 vizinhos, na ordem da varredura (a mesma em todo PC)
		struct FComp { TArray<int32> Cells; bool bBorder = false; int32 Oasis = 0; float MaxD = 0.0f; };
		TArray<FComp> Comps;
		TArray<int32> Lab;
		Lab.Init(0, R * R);
		for (int32 C0 = 0; C0 < R * R; ++C0)
		{
			if (Lab[C0] || GWaterBed[C0] >= L) continue;
			Comps.AddDefaulted();
			const int32 Id = Comps.Num();
			FComp& Co = Comps.Last();
			Lab[C0] = Id;
			Co.Cells.Add(C0);
			for (int32 Head = 0; Head < Co.Cells.Num(); ++Head)
			{
				const int32 C = Co.Cells[Head];
				const int32 I = C % R, J = C / R;
				if (I == 0 || J == 0 || I == R - 1 || J == R - 1) Co.bBorder = true;
				Co.MaxD = FMath::Max(Co.MaxD, L - GWaterBed[C]);
				if (M.Basin.Num() == M.N * M.N)
				{
					const FVector2f G = M.ToGrid(FVector2f(-Half + (I + 0.5f) * T, -Half + (J + 0.5f) * T));
					const int32 VI = FMath::Clamp(FMath::RoundToInt(G.X), 0, M.N - 1), VJ = FMath::Clamp(FMath::RoundToInt(G.Y), 0, M.N - 1);
					if (M.Basin[M.Index(VI, VJ)] == 2) ++Co.Oasis;
				}
				const int32 N4[4][2] = { { I + 1, J }, { I - 1, J }, { I, J + 1 }, { I, J - 1 } };
				for (const auto& NB : N4)
				{
					if (NB[0] < 0 || NB[1] < 0 || NB[0] >= R || NB[1] >= R) continue;
					const int32 Q = NB[1] * R + NB[0];
					if (!Lab[Q] && GWaterBed[Q] < L) { Lab[Q] = Id; Co.Cells.Add(Q); }
				}
			}
		}
		// as que contam: sem a poça; no máximo 255 (o id é um byte), as maiores, na ordem da varredura
		TArray<int32> Keep;
		for (int32 K = 0; K < Comps.Num(); ++K)
		{
			if (Comps[K].Cells.Num() * T * T >= WaterMinAreaM2) Keep.Add(K);
		}
		if (Keep.Num() > 255)
		{
			TArray<int32> BySize = Keep;
			BySize.StableSort([&Comps](int32 A, int32 B) { return Comps[A].Cells.Num() > Comps[B].Cells.Num(); });
			BySize.SetNum(255);
			BySize.Sort();
			Keep = MoveTemp(BySize);
		}
		const bool bSea = HasSea();
		for (const int32 K : Keep)
		{
			const FComp& Co = Comps[K];
			FWaterBody& B = GWaterBodies.AddDefaulted_GetRef();
			B.Id = GWaterBodies.Num();
			B.Kind = (bSea && Co.bBorder) ? WkSea : (2 * Co.Oasis > Co.Cells.Num() ? WkOasis : WkLake);
			if (GFrozen && B.Kind != WkSea) B.Kind = WkIce;
			B.LevelMm = FMath::RoundToInt(L * 1000.0f);
			B.AreaM2 = FMath::RoundToInt(Co.Cells.Num() * T * T);
			B.MaxDepth = Co.MaxD;
			for (const int32 C : Co.Cells) GWaterId[C] = uint8(B.Id);
		}
		// a linha d'água: um segmento por quadrado de 4 texels (os quadrados marchantes), com o ponto
		// interpolado na aresta pela profundidade (f = cota − fundo, > 0 na água); em coordenadas de texel
		auto F = [&](int32 C) { return L - GWaterBed[C]; };
		auto Wet = [&](int32 C) { return GWaterId[C] != 0; };
		const int32 S = R - 1;
		TArray<int32> SegStart;
		SegStart.Init(0, S * S + 1);
		TArray<FVector4f> Segs;
		for (int32 J = 0; J < S; ++J)
		{
			for (int32 I = 0; I < S; ++I)
			{
				SegStart[J * S + I] = Segs.Num();
				const int32 Cn[4] = { J * R + I, J * R + I + 1, (J + 1) * R + I + 1, (J + 1) * R + I };
				const FVector2f Pn[4] = { FVector2f(I, J), FVector2f(I + 1, J), FVector2f(I + 1, J + 1), FVector2f(I, J + 1) };
				const bool Wn[4] = { Wet(Cn[0]), Wet(Cn[1]), Wet(Cn[2]), Wet(Cn[3]) };
				if (Wn[0] == Wn[1] && Wn[1] == Wn[2] && Wn[2] == Wn[3]) continue;
				FVector2f E[4];
				bool bE[4];
				for (int32 K = 0; K < 4; ++K)
				{
					const int32 K1 = (K + 1) & 3;
					bE[K] = Wn[K] != Wn[K1];
					if (!bE[K]) continue;
					const float F0 = F(Cn[K]), F1 = F(Cn[K1]);
					const float Tt = (F0 > 0.0f) != (F1 > 0.0f) ? FMath::Clamp(F0 / (F0 - F1), 0.0f, 1.0f) : 0.5f;
					E[K] = FMath::Lerp(Pn[K], Pn[K1], Tt);
				}
				const int32 NE = int32(bE[0]) + int32(bE[1]) + int32(bE[2]) + int32(bE[3]);
				if (NE == 2)
				{
					FVector2f A2[2];
					int32 Na = 0;
					for (int32 K = 0; K < 4; ++K) if (bE[K]) A2[Na++] = E[K];
					Segs.Add(FVector4f(A2[0].X, A2[0].Y, A2[1].X, A2[1].Y));
				}
				else if (NE == 4)
				{
					// sela: o meio decide se os cantos molhados se ligam
					const bool bMid = (F(Cn[0]) + F(Cn[1]) + F(Cn[2]) + F(Cn[3])) > 0.0f;
					if (bMid == Wn[0])
					{
						Segs.Add(FVector4f(E[0].X, E[0].Y, E[1].X, E[1].Y));
						Segs.Add(FVector4f(E[2].X, E[2].Y, E[3].X, E[3].Y));
					}
					else
					{
						Segs.Add(FVector4f(E[3].X, E[3].Y, E[0].X, E[0].Y));
						Segs.Add(FVector4f(E[1].X, E[1].Y, E[2].X, E[2].Y));
					}
				}
			}
		}
		SegStart[S * S] = Segs.Num();
		// quem está a até 9 m de algum segmento (a tabela de somas dos quadrados com linha)
		const int32 Kr = FMath::CeilToInt(9.0f / T) + 1;
		TArray<int32> Sum;
		Sum.Init(0, (S + 1) * (S + 1));
		for (int32 J = 0; J < S; ++J)
		{
			for (int32 I = 0; I < S; ++I)
			{
				const int32 Has = SegStart[J * S + I + 1] > SegStart[J * S + I] ? 1 : 0;
				Sum[(J + 1) * (S + 1) + I + 1] = Has + Sum[J * (S + 1) + I + 1] + Sum[(J + 1) * (S + 1) + I] - Sum[J * (S + 1) + I];
			}
		}
		ParallelFor(R, [&](int32 J)
		{
			for (int32 I = 0; I < R; ++I)
			{
				const int32 C = J * R + I;
				const int32 X0 = FMath::Max(0, I - Kr), X1 = FMath::Min(S - 1, I + Kr - 1);
				const int32 Y0 = FMath::Max(0, J - Kr), Y1 = FMath::Min(S - 1, J + Kr - 1);
				float Best = 9.0f / T;
				if (X0 <= X1 && Y0 <= Y1 && Sum[(Y1 + 1) * (S + 1) + X1 + 1] - Sum[Y0 * (S + 1) + X1 + 1] - Sum[(Y1 + 1) * (S + 1) + X0] + Sum[Y0 * (S + 1) + X0] > 0)
				{
					const FVector2f Q(I, J);
					for (int32 Y = Y0; Y <= Y1; ++Y)
					{
						for (int32 X = X0; X <= X1; ++X)
						{
							for (int32 K = SegStart[Y * S + X]; K < SegStart[Y * S + X + 1]; ++K)
							{
								const FVector2f A(Segs[K].X, Segs[K].Y), B(Segs[K].Z, Segs[K].W);
								const FVector2f AB = B - A;
								const float Len2 = AB.SizeSquared();
								const float U = Len2 > 1e-8f ? FMath::Clamp(FVector2f::DotProduct(Q - A, AB) / Len2, 0.0f, 1.0f) : 0.0f;
								Best = FMath::Min(Best, FVector2f::Distance(Q, A + U * AB));
							}
						}
					}
				}
				GWaterSd[C] = FMath::Min(Best * T, 9.0f) * (Wet(C) ? 1.0f : -1.0f);
			}
		});
		UE_LOG(LogTemp, Display, TEXT("FORGE water field: %d corpos, %d segmentos de margem, %dx%d a %.1f m"), GWaterBodies.Num(), Segs.Num(), R, R, T);
	}

	/** As células (CellM de lado, a partir do canto −Half) que a malha d'água do corpo cobre (a P1, 13/10:
	 *  uma malha por corpo no lugar do plano global numa cota só): a mancha do corpo crescida GrowM, pra a
	 *  beira ficar embaixo do terreno (o teste de profundidade desenha a margem, como no plano de antes). */
	void WaterBodyCells(int32 Id, float CellM, float GrowM, TArray<FIntPoint>& Out) const
	{
		Out.Reset();
		if (GWaterRes < 2 || Id <= 0 || CellM <= 0.0f) return;
		const int32 R = GWaterRes;
		const int32 G = FMath::Max(0, FMath::CeilToInt(GrowM / GWaterTexel));
		// a mancha crescida (Chebyshev, separável: a soma corrida nas linhas e depois nas colunas)
		TArray<uint8> A, B;
		A.SetNumUninitialized(R * R);
		B.SetNumUninitialized(R * R);
		for (int32 C = 0; C < R * R; ++C) A[C] = GWaterId[C] == Id ? 1 : 0;
		TArray<int32> Run;
		Run.SetNumUninitialized(R + 1);
		for (int32 J = 0; J < R; ++J)
		{
			Run[0] = 0;
			for (int32 I = 0; I < R; ++I) Run[I + 1] = Run[I] + A[J * R + I];
			for (int32 I = 0; I < R; ++I) B[J * R + I] = Run[FMath::Min(R, I + G + 1)] - Run[FMath::Max(0, I - G)] > 0 ? 1 : 0;
		}
		for (int32 I = 0; I < R; ++I)
		{
			Run[0] = 0;
			for (int32 J = 0; J < R; ++J) Run[J + 1] = Run[J] + B[J * R + I];
			for (int32 J = 0; J < R; ++J) A[J * R + I] = Run[FMath::Min(R, J + G + 1)] - Run[FMath::Max(0, J - G)] > 0 ? 1 : 0;
		}
		const float Half = -GWaterMin;
		const int32 Res = FMath::Max(1, FMath::CeilToInt(2.0f * Half / CellM - 0.001f));
		for (int32 CJ = 0; CJ < Res; ++CJ)
		{
			const int32 J0 = FMath::Clamp(FMath::CeilToInt(CJ * CellM / GWaterTexel - 0.5f), 0, R - 1);
			const int32 J1 = FMath::Clamp(FMath::CeilToInt((CJ + 1) * CellM / GWaterTexel - 0.5f) - 1, J0, R - 1);
			for (int32 CI = 0; CI < Res; ++CI)
			{
				const int32 I0 = FMath::Clamp(FMath::CeilToInt(CI * CellM / GWaterTexel - 0.5f), 0, R - 1);
				const int32 I1 = FMath::Clamp(FMath::CeilToInt((CI + 1) * CellM / GWaterTexel - 0.5f) - 1, I0, R - 1);
				bool bAny = false;
				for (int32 J = J0; J <= J1 && !bAny; ++J)
				{
					for (int32 I = I0; I <= I1 && !bAny; ++I) bAny = A[J * R + I] != 0;
				}
				if (bAny) Out.Add(FIntPoint(CI, CJ));
			}
		}
	}

	/** O texel do campo d'água no ponto (o mais perto), −1 fora ou sem campo. */
	int32 WaterTexelAt(FVector2f P) const
	{
		if (GWaterRes < 2) return -1;
		const int32 I = FMath::FloorToInt((P.X - GWaterMin) / GWaterTexel), J = FMath::FloorToInt((P.Y - GWaterMin) / GWaterTexel);
		return (I < 0 || J < 0 || I >= GWaterRes || J >= GWaterRes) ? -1 : J * GWaterRes + I;
	}

	/** A margem molhada (0–1, a grade do chão pro jogo: chão úmido não levanta poeira e a esteira na lama
	 *  fica mais escura e funda): 1 embaixo d'água; a faixa até ~1,2 m acima da cota a até 8 m da margem
	 *  (a areia molhada pela franja e pela capilaridade); a terra úmida do oásis (o anel verde). */
	float WetGroundAt(const FWorldMap& M, FVector2f P)
	{
		const int32 C = WaterTexelAt(P);
		float Wet = 0.0f;
		if (C >= 0 && M.WaterLevel > -100.0f)
		{
			if (GWaterId[C]) return 1.0f;
			Wet = (1.0f - FMath::SmoothStep(M.WaterLevel + 0.3f, M.WaterLevel + 1.2f, GWaterBed[C]))
				* (1.0f - FMath::SmoothStep(4.0f, 8.0f, -GWaterSd[C]));
		}
		if (!GForest && !GWinter) Wet = FMath::Max(Wet, OasisDampAt(M, P) / 0.75f);
		return FMath::Clamp(Wet, 0.0f, 1.0f);
	}

	/** A grade d'água da simulação (CellM de lado, o jogo usa 4 m; o canto em −Half, a linha 0 no −Y):
	 *  R = 0 seco, 1 margem, 2 vau, 3 fundo, 4 gelo passável; G = o id do corpo; B = 0 e A = 255
	 *  (reservados). A célula é água quando metade dos texels de 1 m dela é; vau pela profundidade média
	 *  da parte molhada; a margem é a célula com um pouco d'água ou a seca encostada (4 vizinhos) na água.
	 *  Simetria exata entre os jogadores (o giro de 180° com 2, de 90° com 4, como o gerador): a ondulação
	 *  das dunas não é simétrica e mexia ~5 células de beira por mapa; a célula e as giradas dela somam as
	 *  contagens e decidem juntas, e o id de cada uma é o corpo girado. */
	void WaterGridPixels(float CellM, TArray<FColor>& Px, int32& Res)
	{
		Res = 0;
		if (GWaterRes < 2 || CellM <= 0.0f) return;
		const float Half = -GWaterMin;
		Res = FMath::Clamp(FMath::CeilToInt(2.0f * Half / CellM - 0.001f), 4, 4096);
		Px.Init(FColor(0, 0, 0, 255), Res * Res);
		const float L = GMap->WaterLevel;
		TArray<int32> CellW, CellN;
		TArray<float> CellD;
		TArray<uint8> CellId;
		CellW.Init(0, Res * Res);
		CellN.Init(0, Res * Res);
		CellD.Init(0.0f, Res * Res);
		CellId.Init(0, Res * Res);
		ParallelFor(Res, [&](int32 CJ)
		{
			for (int32 CI = 0; CI < Res; ++CI)
			{
				auto Span = [&](int32 Ci, int32& K0, int32& K1)
				{
					K0 = FMath::Clamp(FMath::CeilToInt(Ci * CellM / GWaterTexel - 0.5f), 0, GWaterRes - 1);
					K1 = FMath::Clamp(FMath::CeilToInt((Ci + 1) * CellM / GWaterTexel - 0.5f) - 1, K0, GWaterRes - 1);
				};
				int32 I0, I1, J0, J1;
				Span(CI, I0, I1);
				Span(CJ, J0, J1);
				int32 Count[256] = {};
				int32 N = 0, W = 0;
				float Depth = 0.0f;
				for (int32 J = J0; J <= J1; ++J)
				{
					for (int32 I = I0; I <= I1; ++I)
					{
						const int32 C = J * GWaterRes + I;
						++N;
						if (const uint8 Id = GWaterId[C])
						{
							++W;
							++Count[Id];
							Depth += L - GWaterBed[C];
						}
					}
				}
				const int32 C = CJ * Res + CI;
				CellN[C] = N;
				if (W == 0) continue;
				int32 Id = 1;
				for (int32 K = 2; K < 256; ++K) if (Count[K] > Count[Id]) Id = K;
				CellW[C] = W;
				CellD[C] = Depth;
				CellId[C] = uint8(Id);
			}
		});
		// a simetria: K giros de 4/K quartos de volta (só quando a grade cai centrada no mapa)
		int32 K = GMap->Settings.Players == 2 || GMap->Settings.Players == 4 ? GMap->Settings.Players : 1;
		if (!FMath::IsNearlyEqual(Res * CellM, 2.0f * Half, 0.01f) || !FMath::IsNearlyEqual(GWaterRes * GWaterTexel, 2.0f * Half, 0.01f)) K = 1;
		const int32 Quarter = 4 / FMath::Max(K, 1);
		auto RotIdx = [](FIntPoint P, int32 Q, int32 G)
		{
			switch (Q & 3)
			{
			case 1: return FIntPoint(G - 1 - P.Y, P.X);
			case 2: return FIntPoint(G - 1 - P.X, G - 1 - P.Y);
			case 3: return FIntPoint(P.Y, G - 1 - P.X);
			default: return P;
			}
		};
		// o corpo girado (um giro): o id no texel girado do primeiro texel de cada corpo
		const int32 NBod = GWaterBodies.Num();
		TArray<uint8> BodyRot, BodyInv;
		BodyRot.Init(0, NBod + 1);
		BodyInv.Init(0, NBod + 1);
		for (int32 B = 0; B <= NBod; ++B) { BodyRot[B] = uint8(B); BodyInv[B] = uint8(B); }
		if (K > 1)
		{
			TArray<int32> First;
			First.Init(-1, NBod + 1);
			for (int32 C = 0; C < GWaterRes * GWaterRes; ++C) if (GWaterId[C] && First[GWaterId[C]] < 0) First[GWaterId[C]] = C;
			for (int32 B = 1; B <= NBod; ++B)
			{
				if (First[B] < 0) continue;
				const FIntPoint P = RotIdx(FIntPoint(First[B] % GWaterRes, First[B] / GWaterRes), Quarter, GWaterRes);
				uint8 To = 0;
				for (int32 Ring = 0; Ring <= 2 && !To; ++Ring)
				{
					for (int32 DJ = -Ring; DJ <= Ring && !To; ++DJ)
					{
						for (int32 DI = -Ring; DI <= Ring && !To; ++DI)
						{
							const int32 X = P.X + DI, Y = P.Y + DJ;
							if (X >= 0 && Y >= 0 && X < GWaterRes && Y < GWaterRes) To = GWaterId[Y * GWaterRes + X];
						}
					}
				}
				if (To) BodyRot[B] = To;
			}
			for (int32 B = 1; B <= NBod; ++B) BodyInv[BodyRot[B]] = uint8(B);
		}
		for (int32 CJ = 0; CJ < Res; ++CJ)
		{
			for (int32 CI = 0; CI < Res; ++CI)
			{
				// a órbita da célula; quem decide é a de menor índice (as outras já foram ou vão ser escritas por ela)
				FIntPoint Orbit[4];
				int32 Canon = MAX_int32;
				for (int32 S = 0; S < K; ++S)
				{
					Orbit[S] = RotIdx(FIntPoint(CI, CJ), S * Quarter, Res);
					Canon = FMath::Min(Canon, Orbit[S].Y * Res + Orbit[S].X);
				}
				if (Canon != CJ * Res + CI) continue;
				int32 W = 0, N = 0;
				float Depth = 0.0f;
				uint8 Id = 0;
				for (int32 S = 0; S < K; ++S)
				{
					const int32 C = Orbit[S].Y * Res + Orbit[S].X;
					W += CellW[C];
					N += CellN[C];
					Depth += CellD[C];
					// o id da célula de partida: o da girada desfeito S giros
					if (!Id && CellId[C])
					{
						uint8 B = CellId[C];
						for (int32 T = 0; T < S; ++T) B = BodyInv[B];
						Id = B;
					}
				}
				if (W == 0) continue;
				uint8 State = 1;
				if (2 * W >= N)
				{
					const uint8 Kind = GWaterBodies[Id - 1].Kind;
					State = Kind == WkIce ? 4 : (Depth / W <= WaterFordM ? 2 : 3);
				}
				uint8 B = Id;
				for (int32 S = 0; S < K; ++S)
				{
					Px[Orbit[S].Y * Res + Orbit[S].X] = FColor(State, B, 0, 255);
					B = BodyRot[B];
				}
			}
		}
		// a margem seca encostada na água (o menor id vizinho)
		TArray<FColor> Out = Px;
		for (int32 CJ = 0; CJ < Res; ++CJ)
		{
			for (int32 CI = 0; CI < Res; ++CI)
			{
				if (Px[CJ * Res + CI].R != 0) continue;
				uint8 Id = 0;
				const int32 N4[4][2] = { { CI + 1, CJ }, { CI - 1, CJ }, { CI, CJ + 1 }, { CI, CJ - 1 } };
				for (const auto& NB : N4)
				{
					if (NB[0] < 0 || NB[1] < 0 || NB[0] >= Res || NB[1] >= Res) continue;
					const FColor& Q = Px[NB[1] * Res + NB[0]];
					if (Q.R >= 2 && (Id == 0 || Q.G < Id)) Id = Q.G;
				}
				if (Id) Out[CJ * Res + CI] = FColor(1, Id, 0, 255);
			}
		}
		Px = MoveTemp(Out);
	}

	/** A direção da pista de asfalto (13/10, o asfalto do Look: as faixas de pneu correm ao longo dela):
	 *  texel de 2 m como a textura de informação, o ângulo dobrado e girado de 45°, R = cos 2(θ − 45°),
	 *  G = sen 2(θ − 45°) em [0, 1]. O sentido não importa e o salto de ±90° do ângulo cai na pista a
	 *  135°/−45°, não na norte–sul nem na leste–oeste. Longe do asfalto, θ = 0.
	 *  B = a distância com sinal até o eixo (0,5 + m/20; positiva à esquerda da direção decodificada, a do
	 *  ângulo em (−45°, 135°]), A = a meia largura (m/12,75): o asfalto v2 do Look (trilhas de roda, meio da
	 *  pista, beira). Linear através da rua: o bilinear de 2 m não erra. */
	void RoadDirPixels(float SizeM, TArray<FColor>& Pixels, int32& R)
	{
		constexpr float Texel = 2.0f;
		R = FMath::Clamp(FMath::RoundToInt(SizeM / Texel), 16, 4096);
		Pixels.SetNumUninitialized(R * R);
		ParallelFor(R, [&](int32 J)
		{
			for (int32 I = 0; I < R; ++I)
			{
				const FVector2f P = GCenter + FVector2f((I + 0.5f) * Texel - 0.5f * SizeM, (J + 0.5f) * Texel - 0.5f * SizeM);
				const FRoadHit Hit = RoadAt(P, INDEX_NONE, true);
				const bool bNear = Hit.Road != INDEX_NONE && Hit.D < Hit.Half() + 6.0f;
				FVector2f D = bNear ? Hit.Dir : FVector2f(1.0f, 0.0f);
				// o sentido que o material decodifica (o ângulo em (−45°, 135°]): o sinal do Across bate com ele
				const float Th = FMath::Atan2(D.Y, D.X);
				if (Th <= -0.25f * PI || Th > 0.75f * PI) D = -D;
				const float A = 2.0f * (FMath::Atan2(D.Y, D.X) - 0.25f * PI);
				const FVector2f Off = P - Hit.Closest;
				const float Across = bNear ? D.X * Off.Y - D.Y * Off.X : 0.0f;
				const float Half = bNear ? Hit.Half() : 0.0f;
				Pixels[J * R + I] = FColor(uint8(FMath::Clamp(0.5f + 0.5f * FMath::Cos(A), 0.0f, 1.0f) * 255.0f + 0.5f),
					uint8(FMath::Clamp(0.5f + 0.5f * FMath::Sin(A), 0.0f, 1.0f) * 255.0f + 0.5f),
					uint8(FMath::Clamp(0.5f + Across / 20.0f, 0.0f, 1.0f) * 255.0f + 0.5f),
					uint8(FMath::Clamp(Half / 12.75f, 0.0f, 1.0f) * 255.0f + 0.5f));
			}
		});
	}

	/** A posição na pista com precisão de centímetro (16/10, pra faixa pintada nova do Look na laje: o RoadDir é de 8 bits,
	 *  ~8 cm por degrau, e a faixa tem 13 cm). A 1 m por texel, em 16 bits: R = a distância com sinal até o eixo (m, o mesmo
	 *  sinal do RoadDir), G = a meia largura (m; 0 longe, no cruzamento e na ponta que se desfaz: ali não se pinta),
	 *  B/A = sen/cos(2π·s/9 m), s ao longo da estrada (o tracejado de 3 m a cada 9 m sem emenda). Só o asfalto. */
	void RoadFramePixels(float SizeM, TArray<FFloat16Color>& Pixels, int32& R)
	{
		constexpr float Texel = 1.0f;
		R = FMath::Clamp(FMath::RoundToInt(SizeM / Texel), 16, 4096);
		Pixels.SetNumUninitialized(R * R);
		ParallelFor(R, [&](int32 J)
		{
			for (int32 I = 0; I < R; ++I)
			{
				const FVector2f P = GCenter + FVector2f((I + 0.5f) * Texel - 0.5f * SizeM, (J + 0.5f) * Texel - 0.5f * SizeM);
				const FRoadHit Hit = RoadAt(P, INDEX_NONE, true);
				FLinearColor C(0.0f, 0.0f, 0.0f, 1.0f);
				if (Hit.Road != INDEX_NONE && Hit.D < Hit.Half() + 6.0f)
				{
					FVector2f D = Hit.Dir;
					const float Th = FMath::Atan2(D.Y, D.X);
					if (Th <= -0.25f * PI || Th > 0.75f * PI) D = -D;
					const FVector2f Off = P - Hit.Closest;
					C.R = D.X * Off.Y - D.Y * Off.X;
					C.G = Hit.Half();
					const FRoadHit Other = RoadAt(P, Hit.Road, true);
					if ((Other.Road != INDEX_NONE && Other.D < Other.Half() + 4.0f) || Hit.EndT < 1.0f) C.G = 0.0f;
					C.B = FMath::Sin(2.0f * PI * Hit.S / 9.0f);
					C.A = FMath::Cos(2.0f * PI * Hit.S / 9.0f);
				}
				Pixels[J * R + I] = FFloat16Color(C);
			}
		});
	}

	// ---- as zonas do chão (a areia v2 do Look, 13/10) -------------------------------------
	// A mesma conta do material do chão (Tools/build_slice.py, SAND_V2_HLSL): o fbm de ruído de valor do Look
	// (3 oitavas, a escala cai à metade, a semente sobe 1) com um hash inteiro, a deformação de 7 m a 9 m, as
	// regras do terreno e a borda rasgada (ruído fino de 0,8 m antes de um limiar estreito). O espalhamento
	// usa pra juntar o capim no pisado, as pedrinhas no pedregoso e os gravetos no pedregoso e no revirado.
	static float ZoneHash(int32 X, int32 Y, uint32 S)
	{
		uint32 H = uint32(X) * 374761393u + uint32(Y) * 668265263u + S * 2246822519u;
		H = (H ^ (H >> 13)) * 1274126177u;
		H = H ^ (H >> 16);
		return float(H & 0x00FFFFFFu) / 16777216.0f;
	}
	static float ZoneFbm(FVector2f Q, float Scale, uint32 Seed)
	{
		float Acc = 0.0f, Amp = 1.0f, Sc = Scale;
		for (int32 O = 0; O < 3; ++O)
		{
			const FVector2f F = Q / Sc;
			const FVector2f Fi(FMath::FloorToFloat(F.X), FMath::FloorToFloat(F.Y));
			FVector2f T = F - Fi;
			T = T * T * (FVector2f(3.0f) - 2.0f * T);
			const int32 CX = int32(Fi.X), CY = int32(Fi.Y);
			const uint32 S = Seed + uint32(O);
			const float A = ZoneHash(CX, CY, S), B = ZoneHash(CX + 1, CY, S), C = ZoneHash(CX, CY + 1, S), D = ZoneHash(CX + 1, CY + 1, S);
			Acc += Amp * FMath::Lerp(FMath::Lerp(A, B, T.X), FMath::Lerp(C, D, T.X), T.Y);
			Sc *= 0.5f;
			Amp *= 0.5f;
		}
		return Acc / 1.75f;
	}
	/** (liso, ondulado, pedregoso, revirado), 0..1; o resto é o pisado. W = os pesos das camadas no ponto. */
	FVector4f ZonesAt(const FWorldMap& M, FVector2f P, const float W[6]) const
	{
		const FVector2f Wq = P + (FVector2f(ZoneFbm(P, 9.0f, 301u), ZoneFbm(P, 9.0f, 302u)) - FVector2f(0.5f)) * 7.0f;
		const float Slope = FMath::SmoothStep(0.03f, 0.25f, 1.0f - FMath::Clamp(M.NormalAt(P).Z, 0.0f, 1.0f));
		const float Trod = FMath::Clamp(W[2] + W[5], 0.0f, 1.0f);
		const float Rock = FMath::Clamp(W[3], 0.0f, 1.0f);
		const float Open = FMath::Clamp(1.0f - Trod - Rock, 0.0f, 1.0f);
		const float Fe = (ZoneFbm(P, 0.8f, 401u) - 0.5f) * 0.45f;
		const float Zs = FMath::SmoothStep(0.595f, 0.625f, ZoneFbm(Wq, 11.0f, 311u) + 0.12f * Trod - 0.10f * Slope + Fe);
		const float Zg = FMath::SmoothStep(0.615f, 0.645f, ZoneFbm(Wq, 6.5f, 312u) + 0.15f * Slope + 0.10f * Rock + Fe) * (1.0f - Zs);
		const float Zr = FMath::SmoothStep(0.575f, 0.605f, ZoneFbm(Wq, 14.0f, 313u) + 0.04f * Open + 0.25f * OpenAt(P) - 0.15f * Trod - 0.15f * Slope + Fe) * (1.0f - Zs) * (1.0f - Zg);
		const float Zd = FMath::SmoothStep(0.695f, 0.725f, ZoneFbm(Wq, 4.5f, 314u) + Fe) * (1.0f - Zr);
		return FVector4f(Zs, Zr, Zg, Zd);
	}

	/** Os pesos das camadas do chão do recorte (SizeM de lado, texel TexelM, R×R):
	 *  A = camadas 0–3, B.rg = 4–5. Pro chão da vista ao vivo do app
	 *  (M_LiveGround_<bioma>), o mesmo que vai pro Landscape no "qualidade real". */
	void WeightPixels(float SizeM, float TexelM, TArray<FColor>& OutA, TArray<FColor>& OutB, int32& R)
	{
		R = FMath::Clamp(FMath::RoundToInt(SizeM / TexelM), 16, 4096);
		OutA.SetNumUninitialized(R * R);
		OutB.SetNumUninitialized(R * R);
		const FWorldMap& M = *GMap;
		ParallelFor(R, [&](int32 J)
		{
			for (int32 I = 0; I < R; ++I)
			{
				const FVector2f P = GCenter + FVector2f((I + 0.5f) * TexelM - 0.5f * SizeM, (J + 0.5f) * TexelM - 0.5f * SizeM);
				float W[6] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
				if (M.IsInside(P)) DesertWeights(M, P, W);
				auto B8 = [](float V) { return uint8(FMath::Clamp(V, 0.0f, 1.0f) * 255.0f + 0.5f); };
				OutA[J * R + I] = FColor(B8(W[0]), B8(W[1]), B8(W[2]), B8(W[3]));
				OutB[J * R + I] = FColor(B8(W[4]), B8(W[5]), M.IsInside(P) ? B8(OpenAt(P)) : 0, 255);   // B: o deserto aberto
			}
		});
	}

	// as edições à mão (apagados): o sorteio pula o que foi apagado
	TSharedPtr<const FWorldEdits> Edits;

	// Os pesos do chão numa grade (1 m), pro espalhamento: são milhões de sorteios
	// por mapa e a conta exata custa ~3 µs (o espalhamento ao vivo levava 9–13 s);
	// na grade, bilinear, ~50 ns. O "qualidade real" usa a mesma grade, pra as
	// duas vistas sortearem igual
	TArray<FColor> WCacheA, WCacheB;
	int32 WCacheRes = 0;
	float WCacheTexel = 1.0f, WCacheSize = 0.0f;

	void BuildWeightCache(float TexelM)
	{
		WCacheSize = 2.0f * GHalf;
		WCacheTexel = TexelM;
		WeightPixels(WCacheSize, TexelM, WCacheA, WCacheB, WCacheRes);
	}

	void ScatterWeights(const FWorldMap& M, FVector2f P, float W[6])
	{
		if (WCacheRes < 2)
		{
			DesertWeights(M, P, W);
			return;
		}
		const FVector2f L = P - GCenter;
		const float X = (L.X + 0.5f * WCacheSize) / WCacheTexel - 0.5f, Y = (L.Y + 0.5f * WCacheSize) / WCacheTexel - 0.5f;
		if (X < 0.0f || Y < 0.0f || X > WCacheRes - 1.001f || Y > WCacheRes - 1.001f)
		{
			DesertWeights(M, P, W);
			return;
		}
		const int32 I0 = int32(X), J0 = int32(Y);
		const float FX = X - I0, FY = Y - J0;
		auto Bi = [&](const TArray<FColor>& C, int32 Ch)
		{
			auto At = [&](int32 I, int32 J)
			{
				const FColor& K = C[J * WCacheRes + I];
				return (Ch == 0 ? K.R : Ch == 1 ? K.G : Ch == 2 ? K.B : K.A) / 255.0f;
			};
			return FMath::Lerp(FMath::Lerp(At(I0, J0), At(I0 + 1, J0), FX), FMath::Lerp(At(I0, J0 + 1), At(I0 + 1, J0 + 1), FX), FY);
		};
		W[0] = Bi(WCacheA, 0); W[1] = Bi(WCacheA, 1); W[2] = Bi(WCacheA, 2); W[3] = Bi(WCacheA, 3);
		W[4] = Bi(WCacheB, 0); W[5] = Bi(WCacheB, 1);
	}
};

/**
 * O que a vista ao vivo do app precisa do mundo de verdade, feito na thread de
 * trabalho junto com a geração: os pesos das camadas do chão (1 m por texel),
 * a textura da água (2 m) e a altura final no próprio mapa (dunas, corte e
 * aterro das estradas, gelo do lago), a mesma do "qualidade real". Assim o
 * mouse, as árvores e o minimapa usam a altura que se vê.
 */
struct WORLDFORGE_API FWorldLive
{
	/** Uma malha de um tipo de espalhamento, com as posições deste mapa (cm). */
	struct FMesh
	{
		FString Mesh;
		TArray<FString> Materials;
		FBox Bounds = FBox(ForceInit);
		TArray<FTransform> Xforms;
		TArray<FVector2f> Origins;   // o ponto sorteado de cada posição (m): a chave da edição
	};
	/** Um tipo (pedra, capim, árvore, paredão...), da tabela do bioma
	 *  (Saved/WorldForge/live_<bioma>.json, gravada pelo build_slice.py). */
	struct FKind
	{
		FString Kind;
		float Spacing = 1.0f, ScaleMin = 1.0f, ScaleMax = 1.0f, Cull = 0.0f;
		TArray<FMesh> Meshes;
	};

	TSharedPtr<FWorldDetail> Detail;   // com uma cópia do mapa na altura do gerador
	float SizeM = 0.0f;                // lado coberto pelas texturas (m), centrado na origem
	int32 WeightRes = 0, WaterRes = 0;
	TArray<FColor> WeightsA, WeightsB, Water;
	bool bSea = false;
	TArray<FKind> Scatter;             // vazio: sem a tabela (projeto sem montar)
	TSharedPtr<FWorldEdits> Edits;     // as edições que o Detail enxerga (a pintura ao vivo mexe aqui)
	TSharedPtr<FWorldMap> Display;     // o mapa mostrado (altura final); a altura à mão refaz aqui

	/** Monta tudo (pesos, água, espalhamento) e troca a altura de Map pela final. */
	static TSharedPtr<FWorldLive> Build(const TSharedPtr<FWorldMap>& Map, TSharedPtr<const FWorldEdits> Edits = nullptr);
	/** O espaçamento usado ao vivo (o capim, o sub-bosque e o capim do oásis 1,5× mais ralos). */
	static float SpacingOf(const FKind& K);
	/** Sorteia de novo só as peças com o ponto sorteado dentro de Region (m),
	 *  com o chão e as edições de agora: Out = os tipos com só essas peças. */
	void ScatterRegion(const FBox2f& Region, TArray<FKind>& Out);
};

