#include "WorldForgeEditorLib.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Generation/WorldGenerator.h"
#include "Generation/WorldNoise.h"
#include "Generation/WorldTrees.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeEditTypes.h"
#include "LandscapeSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionShadowReplace.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialParameterCollection.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	// o último recorte gerado: as outras funções (espalhamento, trilhas) leem dele
	TSharedPtr<FWorldMap> GMap;
	FVector2f GCenter = FVector2f::ZeroVector;   // centro do recorte no mapa gerado (m)
	float GHalf = 0.0f;                           // meio lado do recorte (m)

	FVector2f ToMap(FVector2D Cm) { return GCenter + FVector2f(Cm.X / 100.0, Cm.Y / 100.0); }

	bool GFrozen = false;   // inverno: o lago vira uma placa de gelo plana (camada "molhado" = gelo)

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
		bool bPaved = false;     // asfalto (pintura, mureta) ou terra
		bool bOpen[2] = { false, false };   // ponta dentro de base/campo: o asfalto se desfaz
		int32 Host[2] = { INDEX_NONE, INDEX_NONE };   // estrada onde cada ponta entronca
	};
	TArray<FRoad> GRoads;
	// grade de 8 m com os trechos de estrada perto de cada célula: o mapa inteiro
	// tem milhões de vértices e as estradas, milhares de trechos
	constexpr float RoadCell = 8.0f;
	TMap<FIntPoint, TArray<FIntPoint>> GRoadGrid;   // célula → (estrada, trecho)
	float GRoadReach = 0.0f;   // além disto a distância não importa

	FIntPoint RoadCellOf(FVector2f P) { return FIntPoint(FMath::FloorToInt(P.X / RoadCell), FMath::FloorToInt(P.Y / RoadCell)); }

	struct FRoadHit
	{
		float D = 1e9f;          // distância à linha central (m)
		float Z = 0.0f;          // cota do leito no ponto mais perto
		float End = 1e9f;        // metros até a ponta solta mais perto (1e9: não tem)
		int32 Road = INDEX_NONE;
		float Half() const { return Road != INDEX_NONE ? GRoads[Road].Half : 0.0f; }
		bool Paved() const { return Road != INDEX_NONE && GRoads[Road].bPaved; }
	};

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
			// o asfalto manda no cruzamento: a de terra que chega não corta a pista
			const float Score = D - R.Half - (R.bPaved ? 2.0f : 0.0f);
			if (Score < Best)
			{
				Best = Score;
				H.D = D;
				H.Road = RS.X;
				H.Z = FMath::Lerp(R.Z[K - 1], R.Z[K], T);
				const float S = float(K - 1) + T;   // metros desde o começo
				H.End = FMath::Min(R.bOpen[0] ? S : 1e9f, R.bOpen[1] ? float(R.P.Num() - 1) - S : 1e9f);
			}
		}
		return H;
	}

	TArray<FVector2f> Resample(const TArray<FVector2f>& In, float Step)
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
	TArray<FVector2f> Smooth(TArray<FVector2f> In, int32 Radius, int32 Passes)
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
	float NearestOn(const TArray<FVector2f>& L, FVector2f P, int32& OutIndex)
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

	void IndexRoads()
	{
		GRoadGrid.Reset();
		GRoadReach = 0.0f;
		for (int32 R = 0; R < GRoads.Num(); ++R)
		{
			const FRoad& Road = GRoads[R];
			const float Reach = Road.Half + 12.0f;
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
	TOptional<FWorldGenSettings> GSettingsOverride;

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
			const float R = 2.2f + 1.5f * P.Scale;
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
		if (GFrozen && M.HeightAt(P) < M.WaterLevel + 0.15f) Shore = 1.0f;   // a placa de gelo inteira
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
		// terra rachada: leitos secos (margem dos lagos e por onde a água escorre)
		float Cr = FMath::Clamp(FMath::Max(Shore, FMath::SmoothStep(0.45f, 0.8f, Flow)) + 0.15f * Fine, 0.0f, 1.0f) * (1.0f - Rk) * (1.0f - Dr);
		// cascalho ("pavimento do deserto"): manchas de ~45 m, ~15% do chão (o
		// Arabia do AoE2 tem ~13% de manchas de textura), com a borda recortada
		// em ~8 m. A borda larga e lisa de antes (0,05–0,45 do ruído) lia como
		// mancha de sujeira vista de cima (06/10); e o pé das encostas
		// Na neve fica a mancha suave de antes, mais fraca (neve suja, cascalho
		// aparecendo): com a borda dura virava mancha marrom no branco (06/10)
		const float Ragged = WorldNoise::Fbm(P.X / 8.0f, P.Y / 8.0f, 925u, 3);
		const float Pavement = GWinter ? 0.5f * FMath::SmoothStep(0.05f, 0.45f, Patch + 0.3f * Lush - 0.15f)
			: FMath::SmoothStep(0.2f, 0.32f, Patch + 0.3f * Lush - 0.15f + 0.1f * Fine + 0.08f * Ragged);
		float Gv = FMath::Clamp(Pavement + 0.6f * FMath::SmoothStep(0.05f, 0.3f, Rock), 0.0f, 1.0f)
			* (1.0f - Rk) * (1.0f - Dr) * (1.0f - Cr);
		if (GForest)
		{
			// folhiço sob a copa, com a borda do bosque quebrada pelo ruído fino
			// só onde a mata fecha (árvore solta ganhava um disco laranja de folhas)
			Gv = FMath::SmoothStep(0.05f, 0.35f, WoodAt(M, P) + 0.08f * Fine) * (1.0f - Rk) * (1.0f - Dr) * (1.0f - Cr);
		}
		float Sd = FMath::Max(0.0f, 1.0f - Rk - Dr - Cr - Gv);
		const float Sum = Sd + Gv + Dr + Rk + Cr;
		W[0] = Sd / Sum; W[1] = Gv / Sum; W[2] = Dr / Sum; W[3] = Rk / Sum; W[4] = Cr / Sum; W[5] = 0.0f;
		// terra/cascalho na base das rochas, com a borda quebrada pelo ruído fino
		// (na floresta a terra da trilha; no resto o cascalho)
		const float Ring = 0.85f * FMath::SmoothStep(0.15f, 0.55f, RockRingAt(M, P) + 0.2f * Fine);
		if (Ring > 0.0f)
		{
			for (int32 K = 0; K < 5; ++K) W[K] *= (1.0f - Ring);
			W[GForest ? 2 : 1] += Ring;
		}
		if (GWinter)
		{
			// bases e campos na neve: neve pisada (lama, cascalho e neve), como
			// a estrada de terra; lama pura era mancha marrom no branco (06/10)
			const float Packed = W[2];
			W[2] = 0.4f * Packed;
			W[1] += 0.2f * Packed;
			W[0] += 0.4f * Packed;
		}
		const FRoadHit Hit = RoadAt(P);
		if (Hit.Road != INDEX_NONE)
		{
			const float Half = Hit.Half();
			const float Edge = 0.35f * WorldNoise::Fbm(P.X / 2.5f, P.Y / 2.5f, 921u, 3);
			// talude (o corte ou o aterro da estrada, SliceHeight): onde passa de
			// ~40°, rocha, que é projetada dos três lados; a areia de cima esticava
			// na parede e fazia uma mancha clara na passagem das rampas (06/10)
			const float Fall = Hit.Paved() ? 6.2f : 3.2f;
			const float Grade = FMath::Abs(M.HeightAt(P) - Hit.Z) * 1.5f / Fall;
			const float Bank = FMath::SmoothStep(0.7f, 1.1f, Grade) * FMath::SmoothStep(Half + 0.6f, Half + 1.4f, Hit.D)
				* (1.0f - FMath::SmoothStep(Half + Fall, Half + Fall + 1.5f, Hit.D));
			for (int32 K = 0; K < 5; ++K) W[K] *= (1.0f - Bank);
			W[3] += Bank;
			if (Hit.Paved())
			{
				// asfalto com borda gasta e buracos (o chão de baixo aparece), e o
				// acostamento de terra/cascalho dos dois lados. Na ponta dentro da
				// base o asfalto se desfaz em buracos até virar terra (sem "toco")
				// (45 m afinando até 70% da largura: em 25 m e na largura cheia a
				// ponta lia como toco esburacado, 06/10)
				const float Ends = FMath::SmoothStep(0.0f, 45.0f, Hit.End);
				const float Holes = FMath::SmoothStep(0.42f - 0.5f * (1.0f - Ends), 0.62f - 0.5f * (1.0f - Ends), WorldNoise::Fbm(P.X / 4.0f, P.Y / 4.0f, 923u, 4));
				const float Narrow = Half * FMath::Lerp(0.7f, 1.0f, Ends);
				// borda com 1,2 m de transição: mais fina que o espaçamento do terreno
				// (0,5 m) a borda virava escadinha na diagonal (visto na neve em 06/10)
				const float A = (1.0f - FMath::SmoothStep(Narrow - 0.6f, Narrow + 0.6f, Hit.D + Edge)) * (1.0f - Holes) * FMath::SmoothStep(0.0f, 0.25f, Ends);
				const float Sh = (1.0f - A) * (1.0f - FMath::SmoothStep(Half + 0.5f, Half + 3.5f, Hit.D + 1.5f * Edge));
				for (int32 K = 0; K < 5; ++K) W[K] *= (1.0f - A - Sh);
				W[2] += Sh * 0.6f;
				W[1] += Sh * 0.4f;
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
	}

	/** Altura do recorte (m): o relevo do gerador + ondulação baixa da areia (dunas de ~6 e 20 m). */
	float SliceHeight(const FWorldMap& M, FVector2f P)
	{
		float W[6];
		DesertWeights(M, P, W);
		float H = M.HeightAt(P);
		if (GFrozen) H = FMath::Max(H, M.WaterLevel);   // gelo plano na cota d'água
		// ondulação na camada base: dunas no deserto; na neve e na grama só um
		// rolado largo (a de 6 m, com o sol baixo, virava "caixa de ovo" na neve)
		if (GWinter || GForest) H += W[0] * 0.18f * WorldNoise::Fbm(P.X / 28.0f, P.Y / 28.0f, 919u, 2);
		else H += W[0] * (0.12f * WorldNoise::Fbm(P.X / 6.0f, P.Y / 6.0f, 917u, 3) + 0.25f * WorldNoise::Fbm(P.X / 20.0f, P.Y / 20.0f, 919u, 2));
		const FRoadHit Hit = RoadAt(P);
		if (Hit.Road != INDEX_NONE)
		{
			// a estrada aplaina o chão (corte e aterro) com rampa suave nas laterais
			const float Half = Hit.Half();
			H = FMath::Lerp(H, Hit.Z, 1.0f - FMath::SmoothStep(Half + 0.8f, Half + (Hit.Paved() ? 7.0f : 4.0f), Hit.D));
		}
		return H;
	}
}

ALandscape* UWorldForgeEditorLib::BuildSliceLandscape(int32 Seed, int32 Players, int32 Biome, float MapSize, FVector2D CenterM,
	int32 Components, int32 QuadsPerComponent, float QuadSizeM, UMaterialInterface* Material,
	const TArray<ULandscapeLayerInfoObject*>& LayerInfos, float RoadWidthM, FString& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = TEXT("sem mundo do editor");
		return nullptr;
	}
	const int32 NumLayers = LayerInfos.Num();
	if (NumLayers != 5 && NumLayers != 6)
	{
		Error = TEXT("esperava 5 camadas (base, manchas, trilha, rocha, molhado) e a 6ª opcional (asfalto)");
		return nullptr;
	}

	FWorldGenSettings S;
	S.Seed = Seed;
	S.Players = Players;
	S.Biome = EWorldBiome(FMath::Clamp(Biome, 0, 2));
	S.MapSize = MapSize;
	if (GSettingsOverride.IsSet())
	{
		S = GSettingsOverride.GetValue();
	}
	const int32 N = Components * QuadsPerComponent + 1;                // vértices por lado
	if (CenterM.X > 1e8)
	{
		CenterM = SuggestSliceCenter(Seed, Players, Biome, MapSize, (N - 1) * QuadSizeM);
	}
	GMap = FWorldGenerator::Generate(S);
	const FWorldMap& M = *GMap;
	GFrozen = S.Biome == EWorldBiome::Winter && M.WaterLevel > -100.0f;
	GForest = S.Biome == EWorldBiome::Temperate;
	GWinter = S.Biome == EWorldBiome::Winter;
	BuildCanopy(M);
	BuildRockRing(M);
	GCenter = FVector2f(CenterM.X, CenterM.Y);
	GHalf = 0.5f * (N - 1) * QuadSizeM;
	// a principal: asfalto com a 6ª camada (deserto, neve); sem ela (floresta),
	// estrada de terra de 6 m. As de terra têm 4,4 m (pista simples de estrada
	// de serviço: 3,5–4,5 m, Keller & Sherar 2003)
	const bool bPaved = NumLayers == 6 && RoadWidthM > 0.0f;
	BuildRoads(M, bPaved ? 0.5f * RoadWidthM : 3.0f, bPaved, 2.2f);

	// alturas: 32768 = 0 m; com escala Z de 100 cada unidade vale 1/128 m
	// (LANDSCAPE_ZSCALE): ±256 m com precisão de ~8 mm
	TArray<uint16> Heights;
	Heights.SetNumUninitialized(N * N);
	TArray<TArray<uint8>> Weights;
	Weights.SetNum(NumLayers);
	for (TArray<uint8>& L : Weights) L.SetNumUninitialized(N * N);
	for (int32 Y = 0; Y < N; ++Y)
	{
		for (int32 X = 0; X < N; ++X)
		{
			const FVector2f P = GCenter + FVector2f(X * QuadSizeM - GHalf, Y * QuadSizeM - GHalf);
			const float H = SliceHeight(M, P);
			float W[6];
			DesertWeights(M, P, W);
			Heights[Y * N + X] = uint16(FMath::Clamp(FMath::RoundToInt(32768.0f + H * 128.0f), 0, 65535));
			// pesos em 8 bits somando 255 (o resto vai pra camada base)
			int32 Acc = 0;
			for (int32 K = 1; K < NumLayers; ++K)
			{
				const int32 V = FMath::RoundToInt(W[K] * 255.0f);
				Weights[K][Y * N + X] = uint8(V);
				Acc += V;
			}
			Weights[0][Y * N + X] = uint8(FMath::Clamp(255 - Acc, 0, 255));
		}
	}

	// o mesmo caminho do "New Landscape" do editor (LandscapeEditorDetailCustomization_NewLandscape.cpp)
	const FVector Scale(QuadSizeM * 100.0, QuadSizeM * 100.0, 100.0);
	const FVector Location(-GHalf * 100.0, -GHalf * 100.0, 0.0);
	ALandscape* Landscape = World->SpawnActor<ALandscape>(Location, FRotator::ZeroRotator);
	Landscape->LandscapeMaterial = Material;
	Landscape->SetActorRelativeScale3D(Scale);

	TMap<FGuid, TArray<uint16>> HeightDataPerLayers;
	HeightDataPerLayers.Add(FGuid(), MoveTemp(Heights));
	TArray<FLandscapeImportLayerInfo> Layers;
	for (int32 K = 0; K < NumLayers; ++K)
	{
		FLandscapeImportLayerInfo& L = Layers.AddDefaulted_GetRef();
		L.LayerName = LayerInfos[K]->GetLayerName();
		L.LayerInfo = LayerInfos[K];
		L.LayerData = MoveTemp(Weights[K]);
	}
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayers;
	MaterialLayerDataPerLayers.Add(FGuid(), MoveTemp(Layers));
	Landscape->Import(FGuid::NewGuid(), 0, 0, N - 1, N - 1, 1, QuadsPerComponent, HeightDataPerLayers, TEXT(""),
		MaterialLayerDataPerLayers, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		Error = TEXT("Landscape sem LandscapeInfo depois do Import");
		return Landscape;
	}
	Landscape->SetActorLabel(TEXT("SliceLandscape"));
	Info->UpdateLayerInfoMap(Landscape);
	for (ULandscapeLayerInfoObject* LI : LayerInfos)
	{
		Landscape->AddTargetLayer(LI->GetLayerName(), FLandscapeTargetLayerSettings(LI));
		const int32 Index = Info->GetLayerInfoIndex(LI->GetLayerName());
		if (Info->Layers.IsValidIndex(Index))
		{
			Info->Layers[Index].LayerInfoObj = LI;
		}
	}
	// Chão em Nanite (Landscape > Nanite > Enable Nanite, o mesmo caminho do
	// painel do editor): o chão comum entupia a fila das sombras virtuais ("[VSM]
	// Non-Nanite Marking Job Queue overflow") e era o maior pedaço da tela sem o
	// detalhe automático por distância (preto no F3)
	if (FBoolProperty* NaniteProp = FindFProperty<FBoolProperty>(ALandscapeProxy::StaticClass(), TEXT("bEnableNanite")))
	{
		NaniteProp->SetPropertyValue_InContainer(Landscape, true);
		FPropertyChangedEvent Changed(NaniteProp);
		Landscape->PostEditChangeProperty(Changed);
		if (ULandscapeSubsystem* LS = World->GetSubsystem<ULandscapeSubsystem>())
		{
			ALandscapeProxy* Proxy = Landscape;
			LS->BuildNanite(UE::Landscape::EBuildFlags::WriteFinalLog, MakeArrayView(&Proxy, 1));
		}
		UE_LOG(LogTemp, Display, TEXT("FORGE slice landscape Nanite: %s"), Landscape->IsNaniteMeshUpToDate() ? TEXT("built") : TEXT("NOT built"));
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE slice landscape %dx%d verts, %.0f m, seed %d, center (%.0f, %.0f)"), N, N, 2.0f * GHalf, S.Seed, GCenter.X, GCenter.Y);
	return Landscape;
}

namespace
{
	/** Um lugar sorteado pelo espalhamento, antes de assentar a malha (SliceScatterMeshes). */
	struct FScatterCand
	{
		FVector2f P;
		float Yaw = 0.0f, Scale = 1.0f, Sink = 0.0f, Tilt = 0.0f;
		uint32 Seed = 0;
		FVector2f Up = FVector2f::ZeroVector;   // paredão: direção morro acima
		float Drop = 0.0f;                       // paredão: desnível (m)
	};
	TArray<FScatterCand>* GCands = nullptr;   // ligado: SliceScatter só junta os candidatos
}

FWorldForgeScatter UWorldForgeEditorLib::SliceScatter(FName Kind, float SpacingM, float ScaleMin, float ScaleMax, float SinkM, float YawOffset)
{
	FWorldForgeScatter Out;
	if (!GMap.IsValid())
	{
		return Out;
	}
	const FWorldMap& M = *GMap;
	const uint32 Seed = WorldNoise::Hash(uint32(Kind.GetNumber()) ^ GetTypeHash(Kind.ToString()));
	const FString K = Kind.ToString();
	FVector2f CliffUp = FVector2f::ZeroVector;
	float CliffDrop = 0.0f;
	auto Add = [&](FVector2f P, float Yaw, float Scale, float Sink, float Tilt, WorldNoise::FRng& Rng)
	{
		const FVector2f L = P - GCenter;
		if (FMath::Abs(L.X) > GHalf - 1.0f || FMath::Abs(L.Y) > GHalf - 1.0f) return;
		const FRoadHit Hit = RoadAt(P);
		if (Hit.Road != INDEX_NONE && Hit.D < Hit.Half() + (K == TEXT("Pebble") ? 0.3f : 1.8f)) return;   // pista limpa
		if (GCands)
		{
			FScatterCand& C = GCands->AddDefaulted_GetRef();
			C.P = P;
			C.Yaw = Yaw + YawOffset;
			C.Scale = Scale;
			C.Sink = Sink + SinkM;
			C.Tilt = Tilt;
			C.Seed = Rng.Next();
			C.Up = CliffUp;
			C.Drop = CliffDrop;
			return;
		}
		const float Z = SliceHeight(M, P);
		const FRotator R(Rng.Range(-Tilt, Tilt), Yaw + YawOffset, Rng.Range(-Tilt, Tilt));
		Out.Transforms.Add(FTransform(R, FVector(L.X * 100.0, L.Y * 100.0, (Z - (Sink + SinkM) * Scale) * 100.0), FVector(Scale)));
	};
	if (K == TEXT("TreeBroadleaf") || K == TEXT("TreeConifer"))
	{
		// as árvores do gerador, uma por uma: a mesma posição, tipo e tamanho que
		// o painel mostra (WorldTrees.h). Spacing e escala daqui não valem.
		const WorldTrees::EKind Want = K == TEXT("TreeConifer") ? WorldTrees::EKind::Conifer : WorldTrees::EKind::Broadleaf;
		for (const FWorldProp& P : M.Props)
		{
			if (P.Type != EWorldPropType::Tree || WorldTrees::KindOf(M.Settings.Biome, P.Variant) != Want) continue;
			WorldNoise::FRng Rng(uint64(P.Variant) * 104729u + uint64(FMath::Abs(P.Pos.X * 31.0f + P.Pos.Y * 7.0f)));
			Add(P.Pos, P.Yaw, WorldTrees::ScaleOf(P, M.Settings.Biome), 0.3f, 2.0f, Rng);
		}
		UE_LOG(LogTemp, Display, TEXT("FORGE slice scatter %s: %d"), *K, Out.Transforms.Num());
		return Out;
	}
	if (K == TEXT("Undergrowth") || K == TEXT("Deadwood"))
	{
		// sub-bosque (samambaia, musgo) e madeira morta (toco, tronco caído):
		// dentro da mata e na borda dela, fora de trilha e rocha
		const int32 G = FMath::Max(1, FMath::RoundToInt(2.0f * GHalf / SpacingM));
		const bool bDead = K == TEXT("Deadwood");
		for (int32 V = 0; V < G; ++V)
		{
			for (int32 U = 0; U < G; ++U)
			{
				WorldNoise::FRng Rng(WorldNoise::Hash(U, V, Seed));
				const FVector2f P = GCenter + FVector2f((U + Rng.Float()) * SpacingM - GHalf, (V + Rng.Float()) * SpacingM - GHalf);
				float W[6];
				DesertWeights(M, P, W);
				const bool bFlat = M.NormalAt(P).Z > 0.8f && M.HeightAt(P) > M.WaterLevel + 0.3f;
				const float Wood = WoodAt(M, P);
				const float Clump = WorldNoise::Fbm(P.X / 9.0f, P.Y / 9.0f, Seed + 7u, 3);
				const float Prob = (bDead ? 0.35f : 0.75f) * FMath::SmoothStep(-0.15f, 0.15f, Wood)
					* (bDead ? 1.0f : FMath::SmoothStep(-0.2f, 0.2f, Clump)) * (1.0f - W[2]) * (1.0f - W[3]) * (bFlat ? 1.0f : 0.0f);
				if (Rng.Float() >= Prob) continue;
				Add(P, Rng.Range(0.0f, 360.0f), FMath::Lerp(ScaleMin, ScaleMax, Rng.Float()), bDead ? 0.1f : 0.05f, bDead ? 4.0f : 8.0f, Rng);
			}
		}
		UE_LOG(LogTemp, Display, TEXT("FORGE slice scatter %s: %d"), *K, Out.Transforms.Num());
		return Out;
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
		for (int32 V = 0; V < G; ++V)
		{
			for (int32 U = 0; U < G; ++U)
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
				const FRoadHit Road = RoadAt(P);
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
		UE_LOG(LogTemp, Display, TEXT("FORGE slice scatter %s: %d"), *K, Out.Transforms.Num());
		return Out;
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
	const int32 G = FMath::Max(1, FMath::RoundToInt(2.0f * GHalf / SpacingM));
	for (int32 V = 0; V < G; ++V)
	{
		for (int32 U = 0; U < G; ++U)
		{
			WorldNoise::FRng Rng(WorldNoise::Hash(U, V, Seed));
			const FVector2f P = GCenter + FVector2f((U + Rng.Float()) * SpacingM - GHalf, (V + Rng.Float()) * SpacingM - GHalf);
			float W[6];
			DesertWeights(M, P, W);
			const FVector3f N = M.NormalAt(P);
			const float Clump = WorldNoise::Fbm(P.X / 18.0f, P.Y / 18.0f, Seed + 3u, 3);
			float Prob = 0.0f;
			if (K == TEXT("Pebble")) Prob = 0.15f + 0.85f * W[1] + 0.4f * W[2] + 0.5f * W[4];
			// (sem o 0,05 de fundo: pedregulho solto no gramado, sem motivo, lia
			// como confete; fica no cascalho e no pé da rocha)
			else if (K == TEXT("RockSmall")) Prob = (0.7f * W[1] + 0.9f * FMath::SmoothStep(0.05f, 0.4f, W[3])) * (N.Z > 0.6f ? 1.0f : 0.2f);
			else if (K == TEXT("Tuft") && GForest) Prob = (0.25f + 0.75f * FMath::SmoothStep(-0.35f, 0.25f, Clump)) * (0.9f * W[0] + 0.2f * W[1] + 0.12f * W[2]) * (1.0f - 0.7f * W[2]) * (N.Z > 0.8f ? 1.0f : 0.0f);
			else if (K == TEXT("Tuft")) Prob = FMath::SmoothStep(-0.05f, 0.35f, Clump) * (0.3f * W[0] + 0.8f * W[1] + 0.2f * W[4]) * (1.0f - W[2]) * (N.Z > 0.85f ? 1.0f : 0.0f);
			else if (K == TEXT("Shrub")) Prob = 0.25f * FMath::SmoothStep(0.1f, 0.5f, Clump) * (W[0] + W[1]) * (1.0f - W[2]) * (N.Z > 0.85f ? 1.0f : 0.0f);
			else if (K == TEXT("Branch")) Prob = 0.05f * (W[0] + W[1]) * (N.Z > 0.85f ? 1.0f : 0.0f);
			if (Rng.Float() >= FMath::Clamp(Prob, 0.0f, 1.0f)) continue;
			const float Sc = FMath::Lerp(ScaleMin, ScaleMax, FMath::Square(Rng.Float()));   // mais pequenos que grandes
			Add(P, Rng.Range(0.0f, 360.0f), Sc, K == TEXT("Tuft") ? 0.02f : 0.2f, K == TEXT("Tuft") ? 6.0f : 25.0f, Rng);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE slice scatter %s: %d"), *K, Out.Transforms.Num());
	return Out;
}

TArray<FWorldForgeScatter> UWorldForgeEditorLib::SliceScatterMeshes(FName Kind, float SpacingM, float ScaleMin, float ScaleMax,
	const TArray<UStaticMesh*>& Meshes, float SinkM)
{
	TArray<FWorldForgeScatter> Out;
	Out.SetNum(Meshes.Num());
	if (!GMap.IsValid() || Meshes.Num() == 0)
	{
		return Out;
	}
	const FWorldMap& M = *GMap;
	TArray<FScatterCand> Cands;
	GCands = &Cands;
	SliceScatter(Kind, SpacingM, ScaleMin, ScaleMax, SinkM, 0.0f);
	GCands = nullptr;

	const FString K = Kind.ToString();
	const bool bCliff = K == TEXT("Cliff");
	const bool bRock = K.StartsWith(TEXT("Rock")) || K == TEXT("Pebble") || K == TEXT("Deadwood");
	int32 Rejected = 0;
	auto H = [&M](FVector2f Q) { return SliceHeight(M, Q); };
	for (const FScatterCand& C : Cands)
	{
		const int32 Vi = int32((C.Seed * 2654435761u) % uint32(Meshes.Num()));
		const UStaticMesh* Mesh = Meshes[Vi];
		if (!Mesh) continue;
		WorldNoise::FRng Rng(uint64(C.Seed) * 7u + 1u);
		// a caixa da malha (cm, espaço local): o ponto de origem das peças de
		// conjunto fica a até 3,5 m da pedra (rock_moss_set_01), e a pedra
		// aparecia longe do lugar medido, flutuando ou afundada
		const FBox B = Mesh->GetBoundingBox();
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
			Out[Vi].Transforms.Add(FTransform(Rot, Target - Rot.RotateVector(Ctr * Sc), FVector(Sc)));
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
			if (Hmax - Hmin > 0.45f * Hh + 0.15f)
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
			Out[Vi].Transforms.Add(FTransform(Rot, Target - Rot.RotateVector(Ctr * C.Scale), FVector(C.Scale)));
			continue;
		}
		// o resto (capim, arbusto, galho): pela base, como antes
		const float Z = SliceHeight(M, C.P);
		const FRotator R(Rng.Range(-C.Tilt, C.Tilt), C.Yaw, Rng.Range(-C.Tilt, C.Tilt));
		Out[Vi].Transforms.Add(FTransform(R, FVector(L.X * 100.0, L.Y * 100.0, (Z - C.Sink * C.Scale) * 100.0), FVector(C.Scale)));
	}
	int32 Total = 0;
	for (const FWorldForgeScatter& O : Out) Total += O.Transforms.Num();
	UE_LOG(LogTemp, Display, TEXT("FORGE scatter %s: %d assentadas em %d malhas (%d candidatas, %d fora: chão desnivelado demais)"),
		*K, Total, Meshes.Num(), Cands.Num(), Rejected);
	return Out;
}

namespace
{
	/**
	 * Pontos das estradas (cm, cota do leito) dentro do recorte, a cada StepM,
	 * com (0,0,−1e9) separando os trechos. bMarks: só onde cabe pintura e mureta
	 * (fora dos entroncamentos e a mais de 50 m de uma ponta solta).
	 */
	TArray<FVector> RoadPoints(float StepM, bool bPaved, bool bMarks)
	{
		TArray<FVector> Out;
		const int32 Step = FMath::Max(1, FMath::RoundToInt(StepM));   // a linha já tem 1 m entre pontos
		for (int32 R = 0; R < GRoads.Num(); ++R)
		{
			const FRoad& Road = GRoads[R];
			if (Road.bPaved != bPaved) continue;
			bool bRun = false;
			for (int32 I = 0; I < Road.P.Num(); I += Step)
			{
				const FVector2f L = Road.P[I] - GCenter;
				bool bKeep = FMath::Abs(L.X) < GHalf && FMath::Abs(L.Y) < GHalf;
				if (bKeep && bMarks)
				{
					const float End = FMath::Min(Road.bOpen[0] ? float(I) : 1e9f, Road.bOpen[1] ? float(Road.P.Num() - 1 - I) : 1e9f);
					const FRoadHit Other = RoadAt(Road.P[I], R);
					bKeep = End > 50.0f && (Other.Road == INDEX_NONE || Other.D > Other.Half() + Road.Half + 4.0f);
				}
				if (bKeep)
				{
					Out.Add(FVector(L.X * 100.0, L.Y * 100.0, Road.Z[I] * 100.0));
					bRun = true;
				}
				else if (bRun)
				{
					Out.Add(FVector(0, 0, -1e9));
					bRun = false;
				}
			}
			if (bRun) Out.Add(FVector(0, 0, -1e9));
		}
		return Out;
	}
}

TArray<FVector> UWorldForgeEditorLib::SliceTracks(float StepM)
{
	// as marcas de pneu vão nas estradas de terra (a rede do recorte), na cota
	// do chão (o leito pode estar um pouco abaixo da média do relevo)
	if (!GMap.IsValid()) return TArray<FVector>();
	TArray<FVector> Out = RoadPoints(StepM, false, false);
	for (FVector& P : Out)
	{
		if (P.Z > -1e8) P.Z = SliceHeight(*GMap, ToMap(FVector2D(P.X, P.Y))) * 100.0;
	}
	return Out;
}

float UWorldForgeEditorLib::SliceHeightAt(FVector2D PointCm)
{
	return GMap.IsValid() ? SliceHeight(*GMap, ToMap(PointCm)) * 100.0f : 0.0f;
}

float UWorldForgeEditorLib::SliceRoadEndDistance(FVector2D PointCm)
{
	const FRoadHit Hit = RoadAt(ToMap(PointCm), INDEX_NONE, true);
	return Hit.Road != INDEX_NONE ? FMath::Min(Hit.End, 1e6f) : 0.0f;
}

float UWorldForgeEditorLib::SliceWaterLevel()
{
	return GMap.IsValid() ? GMap->WaterLevel * 100.0f : -1e9f;
}

FVector2D UWorldForgeEditorLib::SuggestSliceCenter(int32 Seed, int32 Players, int32 Biome, float MapSize, float SizeM)
{
	FWorldGenSettings S;
	S.Seed = Seed;
	S.Players = Players;
	S.Biome = EWorldBiome(FMath::Clamp(Biome, 0, 2));
	S.MapSize = MapSize;
	if (GSettingsOverride.IsSet())
	{
		S = GSettingsOverride.GetValue();
	}
	const TSharedPtr<FWorldMap> Map = FWorldGenerator::Generate(S);
	const FWorldMap& M = *Map;
	// os pesos dependem do bioma e da estrada: zera o que sobrou do recorte
	// anterior (montando várias fatias seguidas, o deserto pegava a estrada da neve)
	GForest = S.Biome == EWorldBiome::Temperate;
	GWinter = S.Biome == EWorldBiome::Winter;
	GFrozen = GWinter && M.WaterLevel > -100.0f;
	GRoads.Reset();
	GRoadGrid.Reset();
	BuildCanopy(M);
	BuildRockRing(M);
	const float Half = 0.5f * SizeM, Margin = Half + 0.07f * MapSize;   // fora da moldura de montanhas
	FVector2D Best(0, 0);
	float BestScore = -1.0f;
	for (float CY = -M.Half() + Margin; CY <= M.Half() - Margin; CY += 32.0f)
	{
		for (float CX = -M.Half() + Margin; CX <= M.Half() - Margin; CX += 32.0f)
		{
			// média dos pesos num reticulado de 12 m dentro do quadrado
			float Sum[5] = { 0, 0, 0, 0, 0 };
			int32 Count = 0, Water = 0;
			for (float Y = -Half; Y <= Half; Y += 12.0f)
			{
				for (float X = -Half; X <= Half; X += 12.0f)
				{
					const FVector2f P(CX + X, CY + Y);
					float W[6];
					DesertWeights(M, P, W);
					for (int32 K = 0; K < 5; ++K) Sum[K] += W[K];
					Water += M.HeightAt(P) < M.WaterLevel ? 1 : 0;
					++Count;
				}
			}
			// variedade = entropia da mistura; trilha e rocha presentes valem mais
			float Entropy = 0.0f;
			for (int32 K = 0; K < 5; ++K)
			{
				const float F = Sum[K] / Count;
				if (F > 1e-4f) Entropy -= F * FMath::Loge(F);
			}
			const float Dirt = Sum[2] / Count, Rock = Sum[3] / Count, Wet = float(Water) / Count;
			// fora do deserto, um lago no recorte vale mais (o rio congelado do
			// Company of Heroes 2 é a cara do inverno; a lagoa, a da mata)
			const bool bLake = S.Biome != EWorldBiome::Desert && Wet > 0.03f && Wet < 0.22f;
			// a estrada principal (asfalto no deserto e na neve) atravessando o
			// miolo do recorte: as fatias de exemplo são "com estrada"
			// (pelo meio: a distância do centro do recorte até a principal, fora
			// das bases, onde ela acaba)
			float RoadNear = 1e9f;
			for (int32 T = 0; T < M.Tracks.Num(); ++T)
			{
				if (!M.TrackKinds.IsValidIndex(T) || M.TrackKinds[T] != EWorldTrack::Main) continue;
				for (const FVector2f& Q : M.Tracks[T])
				{
					bool bBase = false;
					for (const FWorldStart& St : M.Starts) bBase |= FVector2f::Distance(Q, St.Pos) < M.BaseRadius + 20.0f;
					if (!bBase) RoadNear = FMath::Min(RoadNear, FVector2f::Distance(Q, FVector2f(CX, CY)));
				}
			}
			const float Road = 1.0f - FMath::SmoothStep(15.0f, 70.0f, RoadNear);
			const float Score = Entropy + (Dirt > 0.04f ? 0.3f : 0.0f) + (Rock > 0.05f ? 0.3f : 0.0f) + (bLake ? 0.6f : 0.0f)
				+ 1.6f * Road - 2.0f * FMath::Max(0.0f, Wet - 0.1f);
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = FVector2D(CX, CY);
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE slice center (%.0f, %.0f) score %.2f"), Best.X, Best.Y, BestScore);
	return Best;
}

AActor* UWorldForgeEditorLib::SpawnGroupActor(const FString& Label)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	AActor* A = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	USceneComponent* Root = NewObject<USceneComponent>(A, TEXT("Root"), RF_Transactional);
	A->SetRootComponent(Root);
	A->AddInstanceComponent(Root);
	Root->RegisterComponent();
	A->SetActorLabel(Label);
	return A;
}

bool UWorldForgeEditorLib::AddInstances(AActor* Actor, UStaticMesh* Mesh, const TArray<FTransform>& Transforms, bool bCastShadow, float CullDistanceM,
	const TArray<UMaterialInterface*>& Materials)
{
	if (!Actor || !Mesh)
	{
		return false;
	}
	// Em blocos de 64 m: um componente por malha e por bloco, como a vegetação
	// e o PCG da Unreal particionam (Instanced Foliage Actor, grade do PCG).
	// A câmera descarta o bloco inteiro fora da vista (CPU e sombras), e o modo
	// teste (F2) mostra bloco a bloco o que a câmera desenha.
	constexpr double CellCm = 6400.0;
	TMap<FIntPoint, TArray<FTransform>> Cells;
	for (const FTransform& X : Transforms)
	{
		const FVector L = X.GetLocation();
		Cells.FindOrAdd(FIntPoint(FMath::FloorToInt(L.X / CellCm), FMath::FloorToInt(L.Y / CellCm))).Add(X);
	}
	for (const TPair<FIntPoint, TArray<FTransform>>& Cell : Cells)
	{
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(Actor, NAME_None, RF_Transactional);
		C->SetStaticMesh(Mesh);
		C->SetMobility(EComponentMobility::Static);
		C->SetCastShadow(bCastShadow);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (CullDistanceM > 0.0f)
		{
			// miúdo (capim, seixo, galho, muda: o que some de longe) fica fora da
			// iluminação global e do campo de distância — só o grande (terreno,
			// rocha, árvore) precisa estar lá; o campo de distância custava ~5 ms
			// por quadro na floresta (perfil de 06/10)
			C->SetCullDistances(0, int32(CullDistanceM * 100.0f));
			C->bAffectDistanceFieldLighting = false;
			C->bAffectDynamicIndirectLighting = false;
			C->bAffectIndirectLightingWhileHidden = false;
		}
		for (int32 I = 0; I < Materials.Num(); ++I)
		{
			if (Materials[I])
			{
				C->SetMaterial(I, Materials[I]);
			}
		}
		C->SetupAttachment(Actor->GetRootComponent());
		Actor->AddInstanceComponent(C);
		C->RegisterComponent();
		C->AddInstances(Cell.Value, false, true, false);
	}
	return true;
}

bool UWorldForgeEditorLib::EnableNanite(UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return false;
	}
	Mesh->Modify();
	FMeshNaniteSettings Settings = Mesh->GetNaniteSettings();
	Settings.bEnabled = true;
	Mesh->SetNaniteSettings(Settings);
	Mesh->Build();
	Mesh->PostEditChange();
	return Mesh->IsNaniteEnabled();
}

void UWorldForgeEditorLib::SetLayerInfoName(ULandscapeLayerInfoObject* LayerInfo, FName LayerName)
{
	if (LayerInfo)
	{
		LayerInfo->SetLayerName(LayerName, true);
	}
}

TArray<FVector> UWorldForgeEditorLib::SliceRoad(float StepM, bool bMarksOnly)
{
	return GMap.IsValid() ? RoadPoints(StepM, true, bMarksOnly) : TArray<FVector>();
}

float UWorldForgeEditorLib::SliceRoadDistance(FVector2D PointCm)
{
	const FRoadHit Hit = RoadAt(ToMap(PointCm), INDEX_NONE, true);
	return Hit.Road != INDEX_NONE ? Hit.D * 100.0f : 1e9f;
}

UStaticMesh* UWorldForgeEditorLib::VegetationToStaticMesh(USkeletalMesh* Source, const FString& AssetPath, bool bBakeParts, bool bNoVoxels)
{
	if (!Source)
	{
		return nullptr;
	}
	FMeshDescription Mesh;
	if (!Source->CloneMeshDescription(0, Mesh))
	{
		UE_LOG(LogTemp, Warning, TEXT("FORGE vegetation: %s has no mesh description"), *Source->GetName());
		return nullptr;
	}
	FStaticMeshAttributes Attributes(Mesh);
	Attributes.Register();

	// materiais: os mesmos da árvore, na mesma ordem (a assembly já aponta pra eles)
	TArray<FStaticMaterial> Materials;
	for (const FSkeletalMaterial& M : Source->GetMaterials())
	{
		Materials.Emplace(M.MaterialInterface, M.MaterialSlotName, M.ImportedMaterialSlotName);
	}
	auto MaterialIndexOf = [&Materials](UMaterialInterface* Mat, FName Slot) -> int32
	{
		for (int32 I = 0; I < Materials.Num(); ++I)
		{
			if ((Mat && Materials[I].MaterialInterface == Mat) || (!Mat && Materials[I].ImportedMaterialSlotName == Slot))
			{
				return I;
			}
		}
		return INDEX_NONE;
	};

	// pose de referência no espaço da malha (os ossos vêm em ordem: pai antes do filho)
	const FReferenceSkeleton& Ref = Source->GetRefSkeleton();
	const TArray<FTransform>& LocalPose = Ref.GetRefBonePose();
	TArray<FTransform> Pose;
	Pose.SetNum(LocalPose.Num());
	for (int32 B = 0; B < LocalPose.Num(); ++B)
	{
		const int32 Parent = Ref.GetParentIndex(B);
		Pose[B] = Parent >= 0 ? LocalPose[B] * Pose[Parent] : LocalPose[B];
	}

	const FNaniteAssemblyData& SourceAssembly = Source->GetNaniteSettings().NaniteAssemblyData;
	TArray<UStaticMesh*> Parts;
	TArray<TArray<int32>> PartRemap;   // material local da peça → material da árvore
	for (const FNaniteAssemblyPart& Part : SourceAssembly.Parts)
	{
		UObject* Object = Part.MeshObjectPath.TryLoad();
		UStaticMesh* Static = Cast<UStaticMesh>(Object);
		if (!Static && Object)
		{
			FString PartName = Object->GetName();
			PartName.RemoveFromStart(TEXT("SKM_"));
			const FString Folder = FPackageName::GetLongPackagePath(Object->GetOutermost()->GetName());
			Static = LoadObject<UStaticMesh>(nullptr, *(Folder / PartName + TEXT(".") + PartName));
		}
		TArray<int32> Remap;
		if (Static)
		{
			const TArray<FStaticMaterial>& PartMaterials = Static->GetStaticMaterials();
			for (int32 L = 0; L < PartMaterials.Num(); ++L)
			{
				int32 Index = MaterialIndexOf(PartMaterials[L].MaterialInterface, PartMaterials[L].ImportedMaterialSlotName);
				if (Index == INDEX_NONE)
				{
					Index = Materials.Emplace(PartMaterials[L].MaterialInterface, PartMaterials[L].MaterialSlotName, PartMaterials[L].ImportedMaterialSlotName);
				}
				Remap.Add(Index);
			}
		}
		UE_LOG(LogTemp, Display, TEXT("FORGE vegetation part %s -> %s"), *Part.MeshObjectPath.ToString(), Static ? *Static->GetPathName() : TEXT("(missing)"));
		Parts.Add(Static);
		PartRemap.Add(Remap);
	}

	auto NodeTransform = [&Pose](const FNaniteAssemblyNode& Node) -> FTransform
	{
		FTransform T(Node.Transform);
		if (Node.TransformSpace == ENaniteAssemblyNodeTransformSpace::BoneRelative && Node.BoneInfluences.Num() > 0)
		{
			const FNaniteAssemblyBoneInfluence* Best = &Node.BoneInfluences[0];
			for (const FNaniteAssemblyBoneInfluence& I : Node.BoneInfluences)
			{
				Best = I.BoneWeight > Best->BoneWeight ? &I : Best;
			}
			if (Pose.IsValidIndex(Best->BoneIndex))
			{
				T = T * Pose[Best->BoneIndex];
			}
		}
		return T;
	};

	// os grupos de polígonos da malha apontam pros materiais pelo nome
	TMap<int32, FPolygonGroupID> GroupOfMaterial;
	{
		TPolygonGroupAttributesRef<FName> SlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
		for (const FPolygonGroupID G : Mesh.PolygonGroups().GetElementIDs())
		{
			const int32 Index = MaterialIndexOf(nullptr, SlotNames[G]);
			GroupOfMaterial.Add(Index == INDEX_NONE ? G.GetValue() : Index, G);
		}
	}

	int64 BakedTriangles = 0;
	FNaniteAssemblyData Assembly;
	if (bBakeParts)
	{
		TArray<TOptional<FMeshDescription>> PartMeshes;
		PartMeshes.SetNum(Parts.Num());
		for (const FNaniteAssemblyNode& Node : SourceAssembly.Nodes)
		{
			if (!Parts.IsValidIndex(Node.PartIndex) || !Parts[Node.PartIndex])
			{
				continue;
			}
			TOptional<FMeshDescription>& PartMesh = PartMeshes[Node.PartIndex];
			if (!PartMesh.IsSet())
			{
				const FMeshDescription* Desc = Parts[Node.PartIndex]->GetMeshDescription(0);
				if (!Desc)
				{
					continue;
				}
				PartMesh = *Desc;
				FStaticMeshAttributes(*PartMesh).Register();
			}
			const TArray<int32>& Remap = PartRemap[Node.PartIndex];
			const TArray<FStaticMaterial>& PartMaterials = Parts[Node.PartIndex]->GetStaticMaterials();
			FStaticMeshOperations::FAppendSettings Append;
			Append.MeshTransform = NodeTransform(Node);
			Append.PolygonGroupsDelegate = FAppendPolygonGroupsDelegate::CreateLambda(
				[&](const FMeshDescription& From, FMeshDescription& To, PolygonGroupMap& Map)
				{
					const TPolygonGroupAttributesConstRef<FName> FromNames = FStaticMeshConstAttributes(From).GetPolygonGroupMaterialSlotNames();
					TPolygonGroupAttributesRef<FName> ToNames = FStaticMeshAttributes(To).GetPolygonGroupMaterialSlotNames();
					for (const FPolygonGroupID G : From.PolygonGroups().GetElementIDs())
					{
						int32 Local = G.GetValue();
						for (int32 L = 0; L < PartMaterials.Num(); ++L)
						{
							if (PartMaterials[L].ImportedMaterialSlotName == FromNames[G])
							{
								Local = L;
							}
						}
						const int32 Final = Remap.IsValidIndex(Local) ? Remap[Local] : 0;
						FPolygonGroupID* Target = GroupOfMaterial.Find(Final);
						if (!Target)
						{
							const FPolygonGroupID New = To.CreatePolygonGroup();
							ToNames[New] = Materials[Final].ImportedMaterialSlotName;
							Target = &GroupOfMaterial.Add(Final, New);
						}
						Map.Add(G, *Target);
					}
				});
			FStaticMeshOperations::AppendMeshDescription(*PartMesh, Mesh, Append);
			BakedTriangles += PartMesh->Triangles().Num();
		}
	}
	else
	{
		for (int32 P = 0; P < Parts.Num(); ++P)
		{
			FNaniteAssemblyPart& Part = Assembly.Parts.AddDefaulted_GetRef();
			Part.MeshObjectPath = FSoftObjectPath(Parts[P]);
			Part.MaterialRemap = PartRemap[P];
		}
		for (const FNaniteAssemblyNode& Node : SourceAssembly.Nodes)
		{
			if (Parts.IsValidIndex(Node.PartIndex) && Parts[Node.PartIndex])
			{
				FNaniteAssemblyNode& Out = Assembly.Nodes.AddDefaulted_GetRef();
				Out.PartIndex = Node.PartIndex;
				Out.TransformSpace = ENaniteAssemblyNodeTransformSpace::Local;
				Out.Transform = FTransform3f(NodeTransform(Node));
			}
		}
	}

	// seções: uma por grupo de polígonos, na ordem, com o material pelo nome
	TArray<int32> SectionMaterials;
	{
		TPolygonGroupAttributesRef<FName> SlotNames = FStaticMeshAttributes(Mesh).GetPolygonGroupMaterialSlotNames();
		for (const FPolygonGroupID G : Mesh.PolygonGroups().GetElementIDs())
		{
			const int32 Index = MaterialIndexOf(nullptr, SlotNames[G]);
			SectionMaterials.Add(Index == INDEX_NONE ? 0 : Index);
		}
	}
	const int32 Triangles = Mesh.Triangles().Num();

	// a malha estática nova (ou a de antes, refeita)
	const FString Name = FPackageName::GetShortName(AssetPath);
	UPackage* Package = CreatePackage(*AssetPath);
	UStaticMesh* Out = FindObject<UStaticMesh>(Package, *Name);
	if (!Out)
	{
		Out = NewObject<UStaticMesh>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Out);
	}
	Out->Modify();
	Out->SetNumSourceModels(1);
	FStaticMeshSourceModel& Model = Out->GetSourceModel(0);
	Model.BuildSettings.bRecomputeNormals = false;
	Model.BuildSettings.bRecomputeTangents = false;
	Model.BuildSettings.bGenerateLightmapUVs = false;
	Out->SetStaticMaterials(Materials);
	Out->GetSectionInfoMap().Clear();
	for (int32 S = 0; S < SectionMaterials.Num(); ++S)
	{
		Out->GetSectionInfoMap().Set(0, S, FMeshSectionInfo(SectionMaterials[S]));
	}
	Out->CreateMeshDescription(0, MoveTemp(Mesh));
	Out->CommitMeshDescription(0);
	FMeshNaniteSettings Nanite = Source->GetNaniteSettings();
	Nanite.bEnabled = true;
	Nanite.NaniteAssemblyData = Assembly;
	if (bNoVoxels)
	{
		// "área", a técnica antiga de folhagem (as peças da assembly herdam a forma
		// da árvore, NaniteHelper.cpp InheritAssemblySettings)
		Nanite.ShapePreservation = ENaniteShapePreservation::PreserveArea;
	}
	Out->SetNaniteSettings(Nanite);
	Out->Build(true);
	Out->PostEditChange();
	Out->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("FORGE vegetation %s: %d triangles (%lld baked from parts), %d parts, %d assembly nodes (source %d), %d materials, extent %s"),
		*Name, Triangles, BakedTriangles, Parts.Num(), Assembly.Nodes.Num(), SourceAssembly.Nodes.Num(), Materials.Num(), *Out->GetBounds().BoxExtent.ToString());
	return Out;
}

bool UWorldForgeEditorLib::AddSnowToMaterial(UMaterial* Material)
{
	UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
	if (!Data || !Material->bUseMaterialAttributes || !Data->MaterialAttributes.Expression)
	{
		UE_LOG(LogTemp, Warning, TEXT("FORGE snow: %s does not use material attributes"), Material ? *Material->GetName() : TEXT("null"));
		return false;
	}
	UMaterialExpression* Source = Data->MaterialAttributes.Expression;
	const int32 SourceOutput = Data->MaterialAttributes.OutputIndex;

	auto* Break = Cast<UMaterialExpressionBreakMaterialAttributes>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionBreakMaterialAttributes::StaticClass(), 2400, 0));
	Break->MaterialAttributes.Connect(SourceOutput, Source);
	auto OutputOf = [Break](const TCHAR* OutputName)
	{
		for (int32 I = 0; I < Break->Outputs.Num(); ++I)
		{
			if (Break->Outputs[I].OutputName == FName(OutputName))
			{
				return I;
			}
		}
		return 0;
	};
	auto* Snow = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass(), 2400, 300));
	Snow->ParameterName = TEXT("Snow");
	Snow->DefaultValue = 0.0f;
	UMaterialExpression* Normal = UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVertexNormalWS::StaticClass(), 2400, 400);
	UMaterialExpression* WorldPos = UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionWorldPosition::StaticClass(), 2400, 500);

	// Neve assenta no que olha pra cima e falha em manchas de ~3 m (o vento
	// varre). A agulha tem dois lados: vale a normal do lado que se vê (a do
	// vértice × TwoSidedSign). Só a do vértice deixava metade das agulhas sem
	// neve, e a árvore só ficava branca de longe, nos voxels (teste de 06/10:
	// com mais detalhe ela ficava verde). Rugosidade da neve ~0.65.
	auto* Code = Cast<UMaterialExpressionCustom>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionCustom::StaticClass(), 2700, 200));
	Code->OutputType = CMOT_Float3;
	Code->Code = TEXT(
		"float n = frac(sin(dot(floor(P.xy / 300.0), float2(12.9898, 78.233))) * 43758.5453);\n"
		"float up = saturate((N.z * Parameters.TwoSidedSign - 0.1) * 1.8) * lerp(0.75, 1.0, n);\n"
		"float s = saturate(Snow * up);\n"
		"Rough = lerp(R, 0.65, s);\n"
		"return lerp(C, float3(0.80, 0.83, 0.88), s);");
	Code->Inputs.Empty();
	for (const TCHAR* InputName : { TEXT("C"), TEXT("R"), TEXT("N"), TEXT("P"), TEXT("Snow") })
	{
		FCustomInput& In = Code->Inputs.AddDefaulted_GetRef();
		In.InputName = InputName;
	}
	Code->Inputs[0].Input.Connect(OutputOf(TEXT("BaseColor")), Break);
	Code->Inputs[1].Input.Connect(OutputOf(TEXT("Roughness")), Break);
	Code->Inputs[2].Input.Connect(0, Normal);
	Code->Inputs[3].Input.Connect(0, WorldPos);
	Code->Inputs[4].Input.Connect(0, Snow);
	FCustomOutput& Rough = Code->AdditionalOutputs.AddDefaulted_GetRef();
	Rough.OutputName = TEXT("Rough");
	Rough.OutputType = CMOT_Float1;
	Code->RebuildOutputs();

	auto* Set = Cast<UMaterialExpressionSetMaterialAttributes>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionSetMaterialAttributes::StaticClass(), 3000, 0));
	Set->Inputs[0].Connect(SourceOutput, Source);
	Set->ConnectInputAttribute(MP_BaseColor, Code, 0);
	Set->ConnectInputAttribute(MP_Roughness, Code, 1);
	Data->MaterialAttributes.Connect(0, Set);
	UMaterialEditingLibrary::RecompileMaterial(Material);
	Material->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("FORGE snow added to %s (break outputs: base %d, rough %d)"), *Material->GetName(), OutputOf(TEXT("BaseColor")), OutputOf(TEXT("Roughness")));
	return true;
}

bool UWorldForgeEditorLib::LoadWorldSettings(const FString& JsonPath, int32& OutBiome, float& OutMapSize)
{
	FString Text;
	TSharedPtr<FJsonObject> J;
	if (!FFileHelper::LoadFileToString(Text, *JsonPath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("FORGE world settings: could not read %s"), *JsonPath);
		return false;
	}
	FWorldGenSettings S;
	S.Seed = int32(J->GetNumberField(TEXT("seed")));
	S.Players = int32(J->GetNumberField(TEXT("players")));
	S.MapSize = float(J->GetNumberField(TEXT("size")));
	S.Biome = EWorldBiome(FMath::Clamp(int32(J->GetNumberField(TEXT("biome"))), 0, 2));
	S.Relief = float(J->GetNumberField(TEXT("relief")));
	S.Plateaus = float(J->GetNumberField(TEXT("plateaus")));
	S.Water = float(J->GetNumberField(TEXT("water")));
	S.Erosion = float(J->GetNumberField(TEXT("erosion")));
	S.Forests = float(J->GetNumberField(TEXT("forests")));
	S.Resources = float(J->GetNumberField(TEXT("resources")));
	GSettingsOverride = S;
	OutBiome = int32(S.Biome);
	OutMapSize = S.MapSize;
	UE_LOG(LogTemp, Display, TEXT("FORGE world settings: seed %d, %d players, %.0f m, biome %d"), S.Seed, S.Players, S.MapSize, OutBiome);
	return true;
}

bool UWorldForgeEditorLib::AddOcclusionFadeToMaterial(UMaterial* Material, UMaterialParameterCollection* Collection)
{
	UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
	if (!Data || !Collection || !Material->bUseMaterialAttributes || !Data->MaterialAttributes.Expression)
	{
		UE_LOG(LogTemp, Warning, TEXT("FORGE fade: %s does not use material attributes"), Material ? *Material->GetName() : TEXT("null"));
		return false;
	}
	UMaterialExpression* Source = Data->MaterialAttributes.Expression;
	const int32 SourceOutput = Data->MaterialAttributes.OutputIndex;
	auto New = [Material](UClass* Class, int32 X, int32 Y) { return UMaterialEditingLibrary::CreateMaterialExpression(Material, Class, X, Y); };

	auto Param = [&](const TCHAR* Name, int32 Y)
	{
		auto* P = Cast<UMaterialExpressionCollectionParameter>(New(UMaterialExpressionCollectionParameter::StaticClass(), 3400, Y));
		P->Collection = Collection;
		P->ParameterName = Name;
		P->ParameterId = Collection->GetParameterId(Name);
		return P;
	};
	UMaterialExpression* Cam = Param(TEXT("TreeFadeCamera"), 700);
	UMaterialExpression* Cur = Param(TEXT("TreeFadeCursor"), 800);
	UMaterialExpression* WorldPos = New(UMaterialExpressionWorldPosition::StaticClass(), 3400, 900);
	UMaterialExpression* Screen = New(UMaterialExpressionScreenPosition::StaticClass(), 3400, 1000);

	// distância do pixel até o segmento câmera → cursor; dentro do cilindro, e só
	// entre os dois, o pixel some em pontilhado (ruído de gradiente intercalado).
	// Os materiais das árvores do PVE são opacos (folhas de geometria; a máscara
	// deles não vale): fora do cilindro a máscara é 1, igual ao opaco, e as cópias
	// do WorldForge ligam o modo "masked" (build_slice.py, tree_materials)
	auto* Code = Cast<UMaterialExpressionCustom>(New(UMaterialExpressionCustom::StaticClass(), 3700, 700));
	Code->OutputType = CMOT_Float1;
	Code->Code = TEXT(
		"float R = U.w;\n"
		"if (R <= 0) return 1.0;\n"
		"float3 ab = U.xyz - C.xyz;\n"
		"float t = saturate(dot(P - C.xyz, ab) / max(dot(ab, ab), 1.0));\n"
		"float d = length(P - (C.xyz + ab * t));\n"
		"float fade = (1.0 - smoothstep(R * 0.55, R, d)) * step(t, 0.995);\n"
		"float2 q = floor(Pix) + 5.588238 * (float)(View.StateFrameIndexMod8);\n"
		"float n = frac(52.9829189 * frac(dot(q, float2(0.06711056, 0.00583715))));\n"
		"return fade > n ? 0.0 : 1.0;");
	Code->Inputs.Empty();
	for (const TCHAR* InputName : { TEXT("C"), TEXT("U"), TEXT("P"), TEXT("Pix") })
	{
		FCustomInput& In = Code->Inputs.AddDefaulted_GetRef();
		In.InputName = InputName;
	}
	Code->Inputs[0].Input.Connect(0, Cam);
	Code->Inputs[1].Input.Connect(0, Cur);
	Code->Inputs[2].Input.Connect(0, WorldPos);
	Code->Inputs[3].Input.Connect(1, Screen);   // PixelPosition
	Code->RebuildOutputs();

	// a sombra da árvore fica (a árvore some só da vista, não do chão): o Nanite
	// rasteriza a sombra com SHADOW_DEPTH_SHADER, que o ShadowPassSwitch lê
	auto* One = Cast<UMaterialExpressionConstant>(New(UMaterialExpressionConstant::StaticClass(), 3700, 1000));
	One->R = 1.0f;
	auto* Switch = Cast<UMaterialExpressionShadowReplace>(New(UMaterialExpressionShadowReplace::StaticClass(), 4000, 700));
	Switch->Default.Connect(0, Code);
	Switch->Shadow.Connect(0, One);

	auto* Set = Cast<UMaterialExpressionSetMaterialAttributes>(New(UMaterialExpressionSetMaterialAttributes::StaticClass(), 4300, 400));
	Set->Inputs[0].Connect(SourceOutput, Source);
	Set->ConnectInputAttribute(MP_OpacityMask, Switch, 0);
	Data->MaterialAttributes.Connect(0, Set);
	UMaterialEditingLibrary::RecompileMaterial(Material);
	Material->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("FORGE fade added to %s"), *Material->GetName());
	return true;
}
