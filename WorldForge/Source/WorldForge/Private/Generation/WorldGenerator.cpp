#include "Generation/WorldGenerator.h"

#include "Algo/Reverse.h"
#include "Async/ParallelFor.h"
#include "Generation/ErosionFilter.h"
#include "Generation/WorldNoise.h"

using namespace WorldNoise;

namespace
{
	// ---- escala (pesquisa: célula do Generals ~3 m, penhasco a partir de ~44°) ----
	enum ELevel : uint8 { LWater = 0, LLow = 1, LPlateau = 2, LHigh = 3, LMountain = 4 };
	constexpr float LogicCell = 4.0f;        // grade lógica (níveis, caminhos, territórios)
	constexpr float LevelStep = 7.0f;        // altura de um degrau de platô (m)
	constexpr float WaterSurface = -1.0f;    // a baixada fica 1 m acima d'água
	constexpr float CliffSlope = 0.78f;      // tan 38°: acima disso a unidade de chão não sobe
	constexpr float RampHalfLength = 9.0f;   // rampa de 18 m pra subir 7 m (~25° no meio)
	constexpr float RampHalfWidth = 8.0f;

	float LevelHeight(int32 L) { return L <= 0 ? WaterSurface - 3.0f : float(FMath::Min(L, 3) - 1) * LevelStep; }

	/**
	 * Simetria de rotação entre os jogadores (2: 180°, 4: 90°), sempre exata
	 * na grade (centrada; rotações de 90° levam célula em célula).
	 *
	 * Campos contínuos (ruído): a "fatia" do jogador 1 é a fonte e as outras
	 * são cópias giradas, emendadas por pesos suaves numa faixa de Blend
	 * metros ao longo das divisas (soma dos pesos = 1, então a emenda é
	 * contínua e o resultado é invariante pela rotação).
	 * Decisões discretas (árvore ou não, célula de recurso): calculadas na
	 * célula canônica da órbita (menor índice) e copiadas.
	 */
	struct FSym
	{
		int32 K = 2;
		float Axis = 0.0f;     // direção do jogador 1 (rad)
		float Blend = 60.0f;   // m

		int32 Quarter() const { return 4 / K; }

		static FVector2f RotQ(FVector2f P, int32 Q)
		{
			switch (Q & 3)
			{
			case 1: return FVector2f(-P.Y, P.X);
			case 2: return FVector2f(-P.X, -P.Y);
			case 3: return FVector2f(P.Y, -P.X);
			default: return P;
			}
		}
		FVector2f Rot(FVector2f P, int32 R) const { return RotQ(P, R * Quarter()); }

		static FIntPoint RotIdxQ(FIntPoint C, int32 Q, int32 G)
		{
			switch (Q & 3)
			{
			case 1: return FIntPoint(G - 1 - C.Y, C.X);
			case 2: return FIntPoint(G - 1 - C.X, G - 1 - C.Y);
			case 3: return FIntPoint(C.Y, G - 1 - C.X);
			default: return C;
			}
		}
		FIntPoint RotIdx(FIntPoint C, int32 R, int32 G) const { return RotIdxQ(C, ((R % K) + K) % K * Quarter(), G); }

		/** Índice linear canônico da órbita de C; OutR = rotação que leva o canônico até C. */
		int32 Canon(FIntPoint C, int32 G, int32* OutR = nullptr) const
		{
			int32 Best = MAX_int32, BestS = 0;
			for (int32 S = 0; S < K; ++S)
			{
				const FIntPoint Q = RotIdx(C, S, G);
				const int32 L = Q.Y * G + Q.X;
				if (L < Best) { Best = L; BestS = S; }
			}
			if (OutR) *OutR = (K - BestS) % K;
			return Best;
		}

		float Weight(FVector2f Q) const
		{
			const float R = Q.Size();
			float Phi = FMath::Atan2(Q.Y, Q.X) - Axis;
			Phi = FMath::UnwindRadians(Phi);
			const float Alpha = FMath::Abs(Phi) - PI / K;
			const float D = Alpha <= HALF_PI ? R * FMath::Sin(Alpha) : R;
			return 1.0f - FMath::SmoothStep(-Blend, Blend, D);
		}

		/** Campo simétrico a partir de Fn(Q, imagem). Ruído de média zero: normaliza pela variância. */
		template <typename F>
		float Field(FVector2f P, F&& Fn, bool bZeroMean) const
		{
			float Sum = 0.0f, W = 0.0f, W2 = 0.0f;
			for (int32 R = 0; R < K; ++R)
			{
				const FVector2f Q = Rot(P, R);
				const float Wi = Weight(Q);
				if (Wi <= 1e-5f) continue;
				Sum += Wi * Fn(Q, R);
				W += Wi;
				W2 += Wi * Wi;
			}
			return bZeroMean ? Sum / FMath::Sqrt(FMath::Max(W2, 1e-12f)) : Sum / FMath::Max(W, 1e-12f);
		}

		/** Copia o valor da célula canônica pra órbita toda (simetria exata, bit a bit). */
		template <typename T>
		void Enforce(TArray<T>& A, int32 G) const
		{
			for (int32 J = 0; J < G; ++J)
			{
				for (int32 I = 0; I < G; ++I)
				{
					A[J * G + I] = A[Canon(FIntPoint(I, J), G)];
				}
			}
		}
	};

	struct FRamp
	{
		FVector2f Center;     // m, na divisa entre os níveis
		FVector2f Dir;        // de baixo pra cima
		int32 Low = 1;
	};

	/** Caixa da rampa: −1 fora; senão o peso (0–1) e a altura da rampa no ponto. */
	float RampSample(const FRamp& R, FVector2f P, float& OutHeight)
	{
		const FVector2f D = P - R.Center;
		const float U = D | R.Dir;
		const float V = D | FVector2f(-R.Dir.Y, R.Dir.X);
		const float WU = 1.0f - FMath::SmoothStep(RampHalfLength, RampHalfLength + 4.0f, FMath::Abs(U));
		const float WV = 1.0f - FMath::SmoothStep(RampHalfWidth, RampHalfWidth + 4.0f, FMath::Abs(V));
		const float W = WU * WV;
		if (W <= 0.0f)
		{
			return -1.0f;
		}
		const float S = FMath::Clamp((U + RampHalfLength) / (2.0f * RampHalfLength), 0.0f, 1.0f);
		const float Shape = 0.5f * S + 0.5f * S * S * (3.0f - 2.0f * S);   // meio linear, meio suave
		OutHeight = FMath::Lerp(LevelHeight(R.Low), LevelHeight(R.Low + 1), Shape);
		return W;
	}

	/** Desfoque em caixa separável (Passes vezes ≈ gaussiano). */
	void Blur(TArray<float>& F, int32 G, int32 Radius, int32 Passes)
	{
		TArray<float> Tmp;
		Tmp.SetNumUninitialized(F.Num());
		for (int32 P = 0; P < Passes; ++P)
		{
			for (int32 Axis = 0; Axis < 2; ++Axis)
			{
				ParallelFor(G, [&](int32 Line)
				{
					for (int32 K = 0; K < G; ++K)
					{
						float S = 0.0f;
						for (int32 O = -Radius; O <= Radius; ++O)
						{
							const int32 Q = FMath::Clamp(K + O, 0, G - 1);
							S += Axis == 0 ? F[Line * G + Q] : F[Q * G + Line];
						}
						(Axis == 0 ? Tmp[Line * G + K] : Tmp[K * G + Line]) = S / float(2 * Radius + 1);
					}
				});
				Swap(F, Tmp);
			}
		}
	}

	/** Amostra bilinear de um campo da grade lógica em coordenadas de célula (contínuas). */
	float SampleLogic(const TArray<float>& F, int32 G, float U, float V)
	{
		U = FMath::Clamp(U, 0.0f, float(G - 1));
		V = FMath::Clamp(V, 0.0f, float(G - 1));
		const int32 I = FMath::Min(int32(U), G - 2), J = FMath::Min(int32(V), G - 2);
		const float FX = U - I, FY = V - J;
		return FMath::Lerp(FMath::Lerp(F[J * G + I], F[J * G + I + 1], FX), FMath::Lerp(F[(J + 1) * G + I], F[(J + 1) * G + I + 1], FX), FY);
	}

	struct FGen
	{
		FWorldGenSettings S;
		FWorldMap& M;
		FSym Sym;
		int32 Attempt = 0;

		// grade lógica
		int32 NL = 0;
		float Half = 0.0f;
		TArray<uint8> Level;
		TArray<uint8> RampMask;
		TArray<uint8> PathMask;
		TArray<FRamp> Ramps;
		TArray<FVector2f> StartPos;
		int32 BaseLevel = LLow;
		float BaseRadius = 50.0f;
		TArray<float> D0;            // distância de caminho a partir da base 1 (m)
		TArray<int8> Owner;          // jogador mais perto pelo caminho
		TArray<float> TrackField;    // trilhas (caminhos esculpidos) desfocadas
		TArray<float> ShoreField;    // terra (1) × água (0), desfocado: a margem

		// grade visível
		TArray<float> RampWeight;
		TArray<float> MountainField; // S4 por vértice (montanha)

		FGen(const FWorldGenSettings& InS, FWorldMap& InM) : S(InS), M(InM) {}

		uint32 SubSeed(uint32 Stage) const
		{
			return Hash(Hash(uint32(S.Seed) * 0x9E3779B9u + Stage * 0x85EBCA6Bu) + uint32(Attempt) * 0xC2B2AE35u);
		}

		// ---- grade lógica ----
		int32 LIdx(int32 A, int32 B) const { return B * NL + A; }
		FVector2f LPos(int32 A, int32 B) const { return FVector2f((A + 0.5f) * LogicCell - Half, (B + 0.5f) * LogicCell - Half); }
		FIntPoint LCellOf(FVector2f P) const
		{
			return FIntPoint(FMath::Clamp(int32((P.X + Half) / LogicCell), 0, NL - 1), FMath::Clamp(int32((P.Y + Half) / LogicCell), 0, NL - 1));
		}
		bool Passable(int32 C) const { return Level[C] >= LLow && Level[C] <= LHigh; }

		/** Passo entre células vizinhas (4-vizinhança): mesmo nível, ou desnível de 1 com rampa. */
		bool CanMove(int32 A, int32 B) const
		{
			if (!Passable(A) || !Passable(B)) return false;
			const int32 D = FMath::Abs(int32(Level[A]) - int32(Level[B]));
			return D == 0 || (D == 1 && (RampMask[A] || RampMask[B]));
		}

		void Run();
		void PlaceStarts();
		void MakeLevels();
		void Cleanup();
		void ForceBases();
		void CarvePaths();
		TArray<int32> CarveSearch(const TArray<int32>& Sources, const TArray<uint8>& IsTarget, const TArray<float>* Extra) const;
		void CarveOrbit(const TArray<int32>& Path, float MinRampSpacing, EWorldTrack Kind);
		TArray<float> Distances(const TArray<int32>& Sources, TArray<int8>* OutOwner, const TArray<int8>* SourceOwner) const;
		TArray<uint8> Reach(int32 From) const;
		void PlaceResources();
		void BuildHeights();
		void BuildLayers();
		void PlaceProps();
		void Finish();
	};

	// ------------------------------------------------------------------------
	// 1. Bases
	// ------------------------------------------------------------------------
	void FGen::PlaceStarts()
	{
		FRng Rng(SubSeed(1));
		Sym.K = S.Players == 4 ? 4 : 2;
		// cantos (diagonal) ou lados; um pouco de giro e de distância da borda
		const bool bCorners = Rng.Float() < (Sym.K == 4 ? 0.6f : 0.7f);
		const float Base = FMath::DegreesToRadians(bCorners ? 225.0f : 180.0f);
		Sym.Axis = Base + FMath::DegreesToRadians(Rng.Range(-8.0f, 8.0f));
		Sym.Blend = 0.06f * S.MapSize;
		const float Inset = FMath::Clamp(0.17f * S.MapSize, 110.0f, 220.0f) * Rng.Range(0.95f, 1.08f);
		const FVector2f Dir(FMath::Cos(Sym.Axis), FMath::Sin(Sym.Axis));
		const FVector2f P0 = Dir * ((Half - Inset) / FMath::Max(FMath::Abs(Dir.X), FMath::Abs(Dir.Y)));
		BaseRadius = FMath::Clamp(0.055f * S.MapSize, 42.0f, 70.0f);
		M.BaseRadius = BaseRadius;
		StartPos.Reset();
		for (int32 R = 0; R < Sym.K; ++R)
		{
			StartPos.Add(Sym.Rot(P0, R));
		}
	}

	// ------------------------------------------------------------------------
	// 2. Níveis por cortes do ruído em frações-alvo
	// ------------------------------------------------------------------------
	void FGen::MakeLevels()
	{
		const uint32 SeedE = SubSeed(2), SeedW = SubSeed(3), SeedM = SubSeed(4), SeedB = SubSeed(5);
		const float Lambda = 0.32f * S.MapSize;            // tamanho das formas grandes
		const float BorderWidth = 0.06f * S.MapSize;
		TArray<float> E, Mt, Border;
		E.SetNumZeroed(NL * NL);
		Mt.SetNumZeroed(NL * NL);
		Border.SetNumZeroed(NL * NL);

		auto Elevation = [&](FVector2f Q, int32)
		{
			const float F = 1.0f / Lambda;
			// domain warp de Quilez: f(p + k·(fbm(p), fbm(p + desloc)))
			const float WX = Fbm(Q.X * F * 0.8f + 3.1f, Q.Y * F * 0.8f - 1.7f, SeedW, 3);
			const float WY = Fbm(Q.X * F * 0.8f - 5.2f, Q.Y * F * 0.8f + 1.3f, SeedW + 1u, 3);
			// poucas oitavas: contorno de platô grande e limpo (legível pra câmera de RTS)
			return Fbm(Q.X * F + 0.8f * WX, Q.Y * F + 0.8f * WY, SeedE, 3);
		};
		auto Ridges = [&](FVector2f Q, int32)
		{
			const float F = 1.0f / (0.22f * S.MapSize);
			return Ridged(Q.X * F, Q.Y * F, SeedM, 4);
		};
		auto Wobble = [&](FVector2f Q, int32) { return Fbm(Q.X / 90.0f, Q.Y / 90.0f, SeedB, 3); };

		ParallelFor(NL, [&](int32 B)
		{
			for (int32 A = 0; A < NL; ++A)
			{
				const int32 C = LIdx(A, B);
				if (Sym.Canon(FIntPoint(A, B), NL) != C) continue;   // só células canônicas
				const FVector2f P = LPos(A, B);
				E[C] = Sym.Field(P, Elevation, true);
				Mt[C] = Sym.Field(P, Ridges, false);
				const float Edge = Half - FMath::Max(FMath::Abs(P.X), FMath::Abs(P.Y));
				Border[C] = Edge + 22.0f * Sym.Field(P, Wobble, true);
			}
		});
		Sym.Enforce(E, NL);
		Sym.Enforce(Mt, NL);
		Sym.Enforce(Border, NL);

		// frações-alvo (sobre o interior: fora da borda de montanhas e das bases)
		auto NearBase = [&](FVector2f P, float Extra)
		{
			for (const FVector2f& St : StartPos)
			{
				if (FVector2f::Distance(P, St) < BaseRadius + Extra) return true;
			}
			return false;
		};
		TArray<float> InteriorE, InteriorM;
		for (int32 B = 0; B < NL; ++B)
		{
			for (int32 A = 0; A < NL; ++A)
			{
				const int32 C = LIdx(A, B);
				if (Border[C] > BorderWidth && !NearBase(LPos(A, B), 30.0f))
				{
					InteriorE.Add(E[C]);
					InteriorM.Add(Mt[C]);
				}
			}
		}
		InteriorE.Sort();
		InteriorM.Sort();
		auto Quantile = [](const TArray<float>& V, float Q)
		{
			return V.Num() ? V[FMath::Clamp(int32(Q * (V.Num() - 1)), 0, V.Num() - 1)] : 0.0f;
		};
		const float FWater = 0.20f * S.Water;
		const float FPlateau = 0.42f * S.Plateaus;
		const float FMountain = 0.10f * S.Relief;
		const float TWater = FWater > 0.001f ? Quantile(InteriorE, FWater) : -1e9f;
		const float T2 = FPlateau > 0.001f ? Quantile(InteriorE, 1.0f - FPlateau) : 1e9f;
		const float TM = FMountain > 0.001f ? Quantile(InteriorM, 1.0f - FMountain) : 1e9f;

		Level.SetNumZeroed(NL * NL);
		for (int32 C = 0; C < NL * NL; ++C)
		{
			uint8 L = LLow;
			if (Border[C] < BorderWidth || Mt[C] > TM) L = LMountain;
			else if (E[C] < TWater) L = LWater;
			else if (E[C] > T2) L = LPlateau;
			Level[C] = L;
		}

		// Planalto (o 2º degrau): ruído próprio (não concêntrico ao platô) e só
		// a 16 m ou mais da borda do platô. Com o mesmo ruído, cada platô
		// ganhava um anel fino e dois penhascos colados — de cima parecia valeta.
		{
			const uint32 SeedH = SubSeed(15);
			auto HighNoise = [&](FVector2f Q, int32)
			{
				const float F = 1.0f / (0.16f * S.MapSize);
				return Fbm(Q.X * F, Q.Y * F, SeedH, 3);
			};
			constexpr int32 Inset = 4;
			TArray<float> H2;
			H2.SetNumZeroed(NL * NL);
			TArray<int32> Eligible;
			for (int32 B = 0; B < NL; ++B)
			{
				for (int32 A = 0; A < NL; ++A)
				{
					bool bInside = true;
					for (int32 DB = -Inset; DB <= Inset && bInside; ++DB)
					{
						for (int32 DA = -Inset; DA <= Inset && bInside; ++DA)
						{
							const int32 X = A + DA, Y = B + DB;
							bInside = X >= 0 && Y >= 0 && X < NL && Y < NL && Level[LIdx(X, Y)] >= LPlateau;
						}
					}
					const int32 C = LIdx(A, B);
					if (bInside && Level[C] == LPlateau)
					{
						Eligible.Add(C);
					}
				}
			}
			for (int32 B = 0; B < NL; ++B)
			{
				for (int32 A = 0; A < NL; ++A)
				{
					const int32 C = LIdx(A, B);
					if (Sym.Canon(FIntPoint(A, B), NL) == C) H2[C] = Sym.Field(LPos(A, B), HighNoise, true);
				}
			}
			Sym.Enforce(H2, NL);
			TArray<float> V;
			for (int32 C : Eligible) V.Add(H2[C]);
			V.Sort();
			const float T3 = V.Num() ? Quantile(V, 0.62f) : 1e9f;   // ~38% do miolo dos platôs
			for (int32 C : Eligible)
			{
				if (H2[C] > T3) Level[C] = LHigh;
			}
		}

		// nível da base: o mais comum (entre os transitáveis) no disco da base 1
		int32 Count[5] = { 0, 0, 0, 0, 0 };
		const FIntPoint SC = LCellOf(StartPos[0]);
		const int32 RC = FMath::CeilToInt(BaseRadius / LogicCell);
		for (int32 B = SC.Y - RC; B <= SC.Y + RC; ++B)
		{
			for (int32 A = SC.X - RC; A <= SC.X + RC; ++A)
			{
				if (A >= 0 && B >= 0 && A < NL && B < NL && FVector2f::Distance(LPos(A, B), StartPos[0]) < BaseRadius)
				{
					++Count[Level[LIdx(A, B)]];
				}
			}
		}
		BaseLevel = LLow;
		for (int32 L = LLow; L <= LHigh; ++L)
		{
			if (Count[L] > Count[BaseLevel]) BaseLevel = L;
		}
		ForceBases();
	}

	void FGen::ForceBases()
	{
		// disco da base plano num nível só; anel em volta sem água nem montanha
		for (int32 B = 0; B < NL; ++B)
		{
			for (int32 A = 0; A < NL; ++A)
			{
				const FVector2f P = LPos(A, B);
				for (const FVector2f& St : StartPos)
				{
					const float D = FVector2f::Distance(P, St);
					uint8& L = Level[LIdx(A, B)];
					if (D < BaseRadius) L = uint8(BaseLevel);
					else if (D < BaseRadius + 24.0f && (L == LWater || L == LMountain)) L = uint8(BaseLevel);
				}
			}
		}
	}

	// ------------------------------------------------------------------------
	// 3. Limpeza: maioria 3×3 e ilhas pequenas absorvidas pelo vizinho
	// ------------------------------------------------------------------------
	void FGen::Cleanup()
	{
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			TArray<uint8> Out = Level;
			ParallelFor(NL, [&](int32 B)
			{
				for (int32 A = 0; A < NL; ++A)
				{
					int32 Count[5] = { 0, 0, 0, 0, 0 };
					for (int32 DB = -1; DB <= 1; ++DB)
					{
						for (int32 DA = -1; DA <= 1; ++DA)
						{
							++Count[Level[LIdx(FMath::Clamp(A + DA, 0, NL - 1), FMath::Clamp(B + DB, 0, NL - 1))]];
						}
					}
					const uint8 Cur = Level[LIdx(A, B)];
					int32 Best = Cur;
					for (int32 L = 0; L < 5; ++L)
					{
						// empate fica com o atual (regra independente da orientação: simétrica)
						if (Count[L] > Count[Best]) Best = L;
					}
					Out[LIdx(A, B)] = uint8(Best);
				}
			});
			Level = MoveTemp(Out);
		}

		// abertura morfológica (erode e dilata 2 células) nos platôs: faixas com
		// menos de ~20 m de largura viram o nível de baixo (penhasco em
		// "minhoca" não serve pra jogo nem lê bem de cima); depois o fechamento
		for (int32 T = LHigh; T >= LPlateau; --T)
		{
			auto Morph = [&](const TArray<uint8>& In, bool bErode)
			{
				TArray<uint8> Out;
				Out.SetNumZeroed(NL * NL);
				ParallelFor(NL, [&](int32 B)
				{
					for (int32 A = 0; A < NL; ++A)
					{
						bool bAll = true, bAny = false;
						for (int32 DB = -2; DB <= 2; ++DB)
						{
							for (int32 DA = -2; DA <= 2; ++DA)
							{
								const uint8 V = In[LIdx(FMath::Clamp(A + DA, 0, NL - 1), FMath::Clamp(B + DB, 0, NL - 1))];
								bAll &= V != 0;
								bAny |= V != 0;
							}
						}
						Out[LIdx(A, B)] = bErode ? bAll : bAny;
					}
				});
				return Out;
			};
			TArray<uint8> Mask;
			Mask.SetNumUninitialized(NL * NL);
			for (int32 C = 0; C < NL * NL; ++C) Mask[C] = Level[C] >= T ? 1 : 0;
			const TArray<uint8> Opened = Morph(Morph(Mask, true), false);
			for (int32 C = 0; C < NL * NL; ++C)
			{
				if (Mask[C] && !Opened[C] && Level[C] != LMountain) Level[C] = uint8(T - 1);
			}
			// e o contrário (fechamento): vale com menos de ~20 m entre dois
			// platôs vira platô — de cima parecia uma valeta entre dois penhascos
			for (int32 C = 0; C < NL * NL; ++C) Mask[C] = Level[C] >= T ? 1 : 0;
			const TArray<uint8> Closed = Morph(Morph(Mask, false), true);
			for (int32 C = 0; C < NL * NL; ++C)
			{
				if (Closed[C] && !Mask[C] && Level[C] == T - 1) Level[C] = uint8(T);
			}
		}

		// componentes (4-vizinhança, mesmo nível) pequenas viram o nível vizinho mais comum
		constexpr int32 MinCells = 64;      // ~1000 m²: menos que isso vira um toco de penhasco
		TArray<int32> Comp;
		Comp.Init(-1, NL * NL);
		TArray<uint8> NewLevel = Level;
		TArray<int32> Stack, Cells;
		int32 NextComp = 0;
		for (int32 Seed = 0; Seed < NL * NL; ++Seed)
		{
			if (Comp[Seed] >= 0) continue;
			const uint8 L = Level[Seed];
			Cells.Reset();
			Stack.Reset();
			Stack.Add(Seed);
			Comp[Seed] = NextComp;
			int32 NeighborCount[5] = { 0, 0, 0, 0, 0 };
			while (Stack.Num())
			{
				const int32 C = Stack.Pop(EAllowShrinking::No);
				Cells.Add(C);
				const int32 A = C % NL, B = C / NL;
				const int32 N4[4][2] = { { A + 1, B }, { A - 1, B }, { A, B + 1 }, { A, B - 1 } };
				for (const auto& NB : N4)
				{
					if (NB[0] < 0 || NB[1] < 0 || NB[0] >= NL || NB[1] >= NL) continue;
					const int32 Q = LIdx(NB[0], NB[1]);
					if (Level[Q] == L)
					{
						if (Comp[Q] < 0) { Comp[Q] = NextComp; Stack.Add(Q); }
					}
					else
					{
						++NeighborCount[Level[Q]];
					}
				}
			}
			if (Cells.Num() < MinCells)
			{
				int32 Best = -1;
				for (int32 K = 0; K < 5; ++K)
				{
					if (NeighborCount[K] > 0 && (Best < 0 || NeighborCount[K] > NeighborCount[Best])) Best = K;
				}
				if (Best >= 0)
				{
					for (int32 C : Cells) NewLevel[C] = uint8(Best);
				}
			}
			++NextComp;
		}
		Level = MoveTemp(NewLevel);
		Sym.Enforce(Level, NL);
		ForceBases();
	}

	// ------------------------------------------------------------------------
	// 4. Caminhos, rampas e passagens rasas
	// ------------------------------------------------------------------------
	TArray<int32> FGen::CarveSearch(const TArray<int32>& Sources, const TArray<uint8>& IsTarget, const TArray<float>* Extra) const
	{
		// Dijkstra que "pode construir": desnível de 1 sem rampa custa uma
		// rampa, água custa uma passagem rasa; montanha e desnível de 2 não passam
		constexpr float RampCost = 30.0f, FordCost = 10.0f;
		TArray<float> Dist;
		Dist.Init(TNumericLimits<float>::Max(), NL * NL);
		TArray<int32> Prev;
		Prev.Init(-1, NL * NL);
		using FNode = TPair<float, int32>;
		TArray<FNode> Heap;
		auto Less = [](const FNode& X, const FNode& Y) { return X.Key < Y.Key; };
		for (int32 Src : Sources)
		{
			Dist[Src] = 0.0f;
			Heap.HeapPush(FNode(0.0f, Src), Less);
		}
		auto Eff = [this](int32 C) { return Level[C] == LWater ? int32(LLow) : int32(Level[C]); };
		while (Heap.Num())
		{
			FNode Top;
			Heap.HeapPop(Top, Less, EAllowShrinking::No);
			const int32 C = Top.Value;
			if (Top.Key > Dist[C]) continue;
			if (IsTarget[C])
			{
				TArray<int32> Path;
				for (int32 X = C; X >= 0; X = Prev[X]) Path.Add(X);
				Algo::Reverse(Path);
				return Path;
			}
			const int32 A = C % NL, B = C / NL;
			const int32 N4[4][2] = { { A + 1, B }, { A - 1, B }, { A, B + 1 }, { A, B - 1 } };
			for (const auto& NB : N4)
			{
				if (NB[0] < 0 || NB[1] < 0 || NB[0] >= NL || NB[1] >= NL) continue;
				const int32 Q = LIdx(NB[0], NB[1]);
				if (Level[Q] == LMountain) continue;
				const int32 DL = FMath::Abs(Eff(C) - Eff(Q));
				if (DL > 1) continue;
				float Cost = 1.0f;
				if (Level[Q] == LWater) Cost += FordCost;
				if (DL == 1 && !RampMask[C] && !RampMask[Q]) Cost += RampCost;
				if (Extra) Cost += (*Extra)[Q];
				const float ND = Dist[C] + Cost;
				if (ND < Dist[Q])
				{
					Dist[Q] = ND;
					Prev[Q] = C;
					Heap.HeapPush(FNode(ND, Q), Less);
				}
			}
		}
		return TArray<int32>();
	}

	void FGen::CarveOrbit(const TArray<int32>& Path, float MinRampSpacing, EWorldTrack Kind)
	{
		// decide no caminho do jogador 1 e aplica a órbita inteira (simetria)
		TArray<FRamp> NewRamps;
		TArray<int32> Fords;
		auto Eff = [this](int32 C) { return Level[C] == LWater ? int32(LLow) : int32(Level[C]); };
		for (int32 K = 1; K < Path.Num(); ++K)
		{
			const int32 A = Path[K - 1], B = Path[K];
			if (Level[B] == LWater) Fords.Add(B);
			const int32 LA = Eff(A), LB = Eff(B);
			if (LA != LB && !RampMask[A] && !RampMask[B])
			{
				const int32 Lo = LA < LB ? A : B, Hi = LA < LB ? B : A;
				FRamp R;
				R.Center = (LPos(A % NL, A / NL) + LPos(B % NL, B / NL)) * 0.5f;
				R.Dir = (LPos(Hi % NL, Hi / NL) - LPos(Lo % NL, Lo / NL)).GetSafeNormal();
				R.Low = FMath::Min(LA, LB);
				bool bNear = false;
				for (const FRamp& O : Ramps)
				{
					bNear |= O.Low == R.Low && FVector2f::Distance(O.Center, R.Center) < MinRampSpacing;
				}
				for (const FRamp& O : NewRamps)
				{
					bNear |= O.Low == R.Low && FVector2f::Distance(O.Center, R.Center) < MinRampSpacing;
				}
				if (!bNear) NewRamps.Add(R);
			}
		}
		for (int32 Rot = 0; Rot < Sym.K; ++Rot)
		{
			TArray<FVector2f>& Line = M.Tracks.AddDefaulted_GetRef();
			M.TrackKinds.Add(Kind);
			for (int32 C : Path)
			{
				const FIntPoint Q = Sym.RotIdx(FIntPoint(C % NL, C / NL), Rot, NL);
				PathMask[LIdx(Q.X, Q.Y)] = 1;
				Line.Add(LPos(Q.X, Q.Y));
			}
			for (int32 C : Fords)
			{
				// passagem rasa de 3 células de largura
				const FIntPoint Q = Sym.RotIdx(FIntPoint(C % NL, C / NL), Rot, NL);
				for (int32 DB = -1; DB <= 1; ++DB)
				{
					for (int32 DA = -1; DA <= 1; ++DA)
					{
						const int32 A = Q.X + DA, B = Q.Y + DB;
						if (A >= 0 && B >= 0 && A < NL && B < NL && Level[LIdx(A, B)] == LWater) Level[LIdx(A, B)] = LLow;
					}
				}
			}
			for (const FRamp& R : NewRamps)
			{
				FRamp X = R;
				X.Center = Sym.Rot(R.Center, Rot);
				X.Dir = Sym.Rot(R.Dir, Rot);
				Ramps.Add(X);
				// células sob a rampa ligam os dois níveis
				const FIntPoint C = LCellOf(X.Center);
				const int32 Rad = FMath::CeilToInt((RampHalfLength + 2.0f) / LogicCell);
				for (int32 B = C.Y - Rad; B <= C.Y + Rad; ++B)
				{
					for (int32 A = C.X - Rad; A <= C.X + Rad; ++A)
					{
						if (A < 0 || B < 0 || A >= NL || B >= NL) continue;
						const FVector2f D = LPos(A, B) - X.Center;
						if (FMath::Abs(D | X.Dir) <= RampHalfLength && FMath::Abs(D | FVector2f(-X.Dir.Y, X.Dir.X)) <= RampHalfWidth)
						{
							RampMask[LIdx(A, B)] = 1;
						}
					}
				}
			}
		}
	}

	TArray<uint8> FGen::Reach(int32 From) const
	{
		TArray<uint8> R;
		R.SetNumZeroed(NL * NL);
		TArray<int32> Stack = { From };
		R[From] = 1;
		while (Stack.Num())
		{
			const int32 C = Stack.Pop(EAllowShrinking::No);
			const int32 A = C % NL, B = C / NL;
			const int32 N4[4][2] = { { A + 1, B }, { A - 1, B }, { A, B + 1 }, { A, B - 1 } };
			for (const auto& NB : N4)
			{
				if (NB[0] < 0 || NB[1] < 0 || NB[0] >= NL || NB[1] >= NL) continue;
				const int32 Q = LIdx(NB[0], NB[1]);
				if (!R[Q] && CanMove(C, Q)) { R[Q] = 1; Stack.Add(Q); }
			}
		}
		return R;
	}

	void FGen::CarvePaths()
	{
		RampMask.SetNumZeroed(NL * NL);
		PathMask.SetNumZeroed(NL * NL);
		Ramps.Reset();
		M.Tracks.Reset();
		M.TrackKinds.Reset();
		const FIntPoint S0 = LCellOf(StartPos[0]);
		const int32 Start0 = LIdx(S0.X, S0.Y);

		// alvo: o centro (a célula transitável mais perto dele e a órbita dela)
		int32 Center = -1;
		float BestD = TNumericLimits<float>::Max();
		for (int32 C = 0; C < NL * NL; ++C)
		{
			if (!Passable(C)) continue;
			const float D = LPos(C % NL, C / NL).Size();
			if (D < BestD) { BestD = D; Center = C; }
		}
		TArray<uint8> CenterTargets;
		CenterTargets.SetNumZeroed(NL * NL);
		if (Center >= 0)
		{
			for (int32 R = 0; R < Sym.K; ++R)
			{
				const FIntPoint Q = Sym.RotIdx(FIntPoint(Center % NL, Center / NL), R, NL);
				CenterTargets[LIdx(Q.X, Q.Y)] = 1;
			}
		}

		// caminho principal base → centro
		const TArray<int32> Main = CarveSearch({ Start0 }, CenterTargets, nullptr);
		CarveOrbit(Main, 24.0f, EWorldTrack::Main);

		// flanco: outro caminho, longe do principal (rota alternativa, como os
		// caminhos Center/Flank da IA do Generals)
		if (S.Plateaus > 0.15f && Main.Num())
		{
			TArray<float> Away;
			Away.SetNumZeroed(NL * NL);
			for (int32 C : Main)
			{
				const int32 A = C % NL, B = C / NL;
				for (int32 DB = -10; DB <= 10; ++DB)
				{
					for (int32 DA = -10; DA <= 10; ++DA)
					{
						const int32 X = A + DA, Y = B + DB;
						if (X >= 0 && Y >= 0 && X < NL && Y < NL) Away[LIdx(X, Y)] = 4.0f;
					}
				}
			}
			const TArray<int32> Flank = CarveSearch({ Start0 }, CenterTargets, &Away);
			if (Flank.Num() && Flank.Num() < Main.Num() * 2)
			{
				CarveOrbit(Flank, 48.0f, EWorldTrack::Flank);
			}
		}

		// todas as bases se alcançam? se não, caminho direto até a base 2
		for (int32 Try = 0; Try < 3; ++Try)
		{
			const TArray<uint8> R = Reach(Start0);
			bool bAll = true;
			for (const FVector2f& St : StartPos)
			{
				const FIntPoint C = LCellOf(St);
				bAll &= R[LIdx(C.X, C.Y)] != 0;
			}
			if (bAll) break;
			TArray<uint8> Target;
			Target.SetNumZeroed(NL * NL);
			const FIntPoint S1 = LCellOf(StartPos[1]);
			Target[LIdx(S1.X, S1.Y)] = 1;
			CarveOrbit(CarveSearch({ Start0 }, Target, nullptr), 24.0f, EWorldTrack::Link);
		}

		// regiões grandes que ficaram ilhadas ganham rampa (platô inútil é desperdício de mapa)
		constexpr int32 MinRegion = 80;
		for (int32 Iter = 0; Iter < 24; ++Iter)
		{
			const TArray<uint8> R = Reach(Start0);
			TArray<uint8> Seen = R;
			bool bCarved = false;
			for (int32 C0 = 0; C0 < NL * NL && !bCarved; ++C0)
			{
				if (Seen[C0] || !Passable(C0)) continue;
				// componente de C0 (mesmo critério de movimento)
				TArray<int32> Region = { C0 };
				Seen[C0] = 1;
				for (int32 K = 0; K < Region.Num(); ++K)
				{
					const int32 C = Region[K];
					const int32 A = C % NL, B = C / NL;
					const int32 N4[4][2] = { { A + 1, B }, { A - 1, B }, { A, B + 1 }, { A, B - 1 } };
					for (const auto& NB : N4)
					{
						if (NB[0] < 0 || NB[1] < 0 || NB[0] >= NL || NB[1] >= NL) continue;
						const int32 Q = LIdx(NB[0], NB[1]);
						if (!Seen[Q] && CanMove(C, Q)) { Seen[Q] = 1; Region.Add(Q); }
					}
				}
				if (Region.Num() >= MinRegion)
				{
					TArray<int32> Path = CarveSearch(Region, R, nullptr);
					if (Path.Num())
					{
						Algo::Reverse(Path);
						CarveOrbit(Path, 24.0f, EWorldTrack::Link);
						bCarved = true;
					}
				}
			}
			if (!bCarved) break;
		}
		Sym.Enforce(Level, NL);
		Sym.Enforce(RampMask, NL);
		Sym.Enforce(PathMask, NL);
	}

	// ------------------------------------------------------------------------
	// 5. Distâncias de caminho e territórios
	// ------------------------------------------------------------------------
	TArray<float> FGen::Distances(const TArray<int32>& Sources, TArray<int8>* OutOwner, const TArray<int8>* SourceOwner) const
	{
		TArray<float> Dist;
		Dist.Init(TNumericLimits<float>::Max(), NL * NL);
		if (OutOwner) OutOwner->Init(-1, NL * NL);
		using FNode = TPair<float, int32>;
		TArray<FNode> Heap;
		auto Less = [](const FNode& X, const FNode& Y) { return X.Key < Y.Key; };
		for (int32 K = 0; K < Sources.Num(); ++K)
		{
			Dist[Sources[K]] = 0.0f;
			if (OutOwner) (*OutOwner)[Sources[K]] = SourceOwner ? (*SourceOwner)[K] : int8(K);
			Heap.HeapPush(FNode(0.0f, Sources[K]), Less);
		}
		while (Heap.Num())
		{
			FNode Top;
			Heap.HeapPop(Top, Less, EAllowShrinking::No);
			const int32 C = Top.Value;
			if (Top.Key > Dist[C]) continue;
			const int32 A = C % NL, B = C / NL;
			const int32 N4[4][2] = { { A + 1, B }, { A - 1, B }, { A, B + 1 }, { A, B - 1 } };
			for (const auto& NB : N4)
			{
				if (NB[0] < 0 || NB[1] < 0 || NB[0] >= NL || NB[1] >= NL) continue;
				const int32 Q = LIdx(NB[0], NB[1]);
				if (!CanMove(C, Q)) continue;
				const float ND = Dist[C] + LogicCell;
				if (ND < Dist[Q])
				{
					Dist[Q] = ND;
					if (OutOwner) (*OutOwner)[Q] = (*OutOwner)[C];
					Heap.HeapPush(FNode(ND, Q), Less);
				}
			}
		}
		return Dist;
	}

	// ------------------------------------------------------------------------
	// 6. Recursos: principal, expansão e disputados, na mesma distância pra todos
	// ------------------------------------------------------------------------
	void FGen::PlaceResources()
	{
		M.Resources.Reset();
		if (S.Resources <= 0.01f)
		{
			return;
		}
		const float Radius = 8.0f + 6.0f * S.Resources;
		const int32 Clear = FMath::CeilToInt((Radius + 4.0f) / LogicCell);
		// distância (em células) até a trilha mais próxima: campo não fica no meio da estrada
		TArray<float> PathDist;
		{
			TArray<int32> Src;
			for (int32 C = 0; C < NL * NL; ++C) if (PathMask[C]) Src.Add(C);
			PathDist.Init(1e9f, NL * NL);
			for (int32 C : Src) PathDist[C] = 0.0f;
			// distância de Chebyshev aproximada por duas varreduras
			for (int32 Pass = 0; Pass < 2; ++Pass)
			{
				for (int32 B = 0; B < NL; ++B)
				{
					for (int32 A = 0; A < NL; ++A)
					{
						const int32 AA = Pass ? NL - 1 - A : A, BB = Pass ? NL - 1 - B : B;
						float& D = PathDist[LIdx(AA, BB)];
						const int32 DA = Pass ? 1 : -1;
						if (AA + DA >= 0 && AA + DA < NL) D = FMath::Min(D, PathDist[LIdx(AA + DA, BB)] + 1.0f);
						if (BB + DA >= 0 && BB + DA < NL) D = FMath::Min(D, PathDist[LIdx(AA, BB + DA)] + 1.0f);
					}
				}
			}
		}
		auto Flat = [&](int32 C)
		{
			// disco transitável de um nível só, sem rampa
			const int32 A = C % NL, B = C / NL;
			const uint8 L = Level[C];
			for (int32 DB = -Clear; DB <= Clear; ++DB)
			{
				for (int32 DA = -Clear; DA <= Clear; ++DA)
				{
					const int32 X = A + DA, Y = B + DB;
					if (DA * DA + DB * DB > Clear * Clear) continue;
					if (X < 0 || Y < 0 || X >= NL || Y >= NL) return false;
					const int32 Q = LIdx(X, Y);
					if (Level[Q] != L || RampMask[Q]) return false;
				}
			}
			return true;
		};
		auto FarFromBases = [&](FVector2f P)
		{
			for (const FVector2f& St : StartPos)
			{
				if (FVector2f::Distance(P, St) < BaseRadius + Radius + 6.0f) return false;
			}
			return true;
		};
		auto FarFromResources = [&](FVector2f P, float Min)
		{
			for (const FWorldResource& R : M.Resources)
			{
				if (FVector2f::Distance(P, R.Pos) < Min) return false;
			}
			return true;
		};
		const uint32 Seed = SubSeed(6);
		auto Pick = [&](TFunctionRef<bool(int32)> Ok, TFunctionRef<float(int32)> Score)
		{
			int32 Best = -1;
			float BestScore = -1e9f;
			for (int32 C = 0; C < NL * NL; ++C)
			{
				if (!Passable(C) || !Ok(C)) continue;
				const float Sc = Score(C) + 0.5f * Unit(Hash(C, 0, Seed));
				if (Sc > BestScore && Flat(C)) { BestScore = Sc; Best = C; }
			}
			return Best;
		};
		auto AddOrbit = [&](int32 C, float Amount, bool bContested)
		{
			const FVector2f P = LPos(C % NL, C / NL);
			for (int32 R = 0; R < Sym.K; ++R)
			{
				const FVector2f Q = Sym.Rot(P, R);
				if (!FarFromResources(Q, 1.0f)) continue;   // ponto fixo da rotação (o centro)
				FWorldResource Res;
				Res.Pos = Q;
				Res.Radius = Radius;
				Res.Amount = Amount;
				Res.Owner = bContested ? -1 : R;
				M.Resources.Add(Res);
			}
		};

		// principal: perto da base, fora do caminho pro centro
		const float MainD = BaseRadius + 30.0f;
		const int32 Main = Pick(
			[&](int32 C) { return Owner[C] == 0 && FMath::Abs(D0[C] - MainD) < 18.0f && FarFromBases(LPos(C % NL, C / NL)); },
			[&](int32 C) { return FMath::Min(PathDist[C], 12.0f) - 0.05f * FMath::Abs(D0[C] - MainD); });
		if (Main >= 0) AddOrbit(Main, 1.0f, false);

		// expansão: mais longe, ainda no território do jogador
		const float ExpD = 0.36f * S.MapSize;
		const int32 Exp = Pick(
			[&](int32 C) { return Owner[C] == 0 && FMath::Abs(D0[C] - ExpD) < 0.08f * S.MapSize && FarFromBases(LPos(C % NL, C / NL)) && FarFromResources(LPos(C % NL, C / NL), 60.0f); },
			[&](int32 C) { return FMath::Min(PathDist[C], 10.0f) - 0.02f * FMath::Abs(D0[C] - ExpD); });
		if (Exp >= 0) AddOrbit(Exp, 1.4f, false);

		// disputados: na divisa (mesma distância de caminho de duas bases), mais ricos
		const int32 Contested = S.Resources > 0.66f ? 2 : (S.Resources > 0.2f ? 1 : 0);
		for (int32 K = 0; K < Contested; ++K)
		{
			const int32 C = Pick(
				[&](int32 X)
				{
					const FIntPoint Q = Sym.RotIdx(FIntPoint(X % NL, X / NL), -1, NL);
					const float D1 = D0[LIdx(Q.X, Q.Y)];   // distância da base 2 (pela simetria)
					return D0[X] < 1e8f && D1 < 1e8f && FMath::Abs(D0[X] - D1) < 10.0f && FarFromBases(LPos(X % NL, X / NL))
						&& FarFromResources(LPos(X % NL, X / NL), 70.0f);
				},
				[&](int32 X)
				{
					float MinD = 1e9f;
					for (const FWorldResource& R : M.Resources) MinD = FMath::Min(MinD, FVector2f::Distance(R.Pos, LPos(X % NL, X / NL)));
					return FMath::Min(MinD, 200.0f) * 0.05f + FMath::Min(PathDist[X], 8.0f) * 0.3f;
				});
			if (C >= 0) AddOrbit(C, 2.0f, true);
		}
	}

	// ------------------------------------------------------------------------
	// 7. Relevo visível (grade de 2 m)
	// ------------------------------------------------------------------------
	void FGen::BuildHeights()
	{
		const int32 N = M.N;
		// indicadores dos níveis, desfocados: a borda de cada nível vira um
		// contorno suave (penhasco curvo, não serrilhado na grade de 4 m)
		auto Indicator = [&](TFunctionRef<bool(uint8)> Test, int32 Radius, int32 Passes)
		{
			TArray<float> F;
			F.SetNumUninitialized(NL * NL);
			for (int32 C = 0; C < NL * NL; ++C) F[C] = Test(Level[C]) ? 1.0f : 0.0f;
			Blur(F, NL, Radius, Passes);
			Sym.Enforce(F, NL);
			return F;
		};
		const TArray<float> B1 = Indicator([](uint8 L) { return L >= LLow; }, 2, 2);
		const TArray<float> B1W = Indicator([](uint8 L) { return L >= LLow; }, 4, 3);
		ShoreField = Indicator([](uint8 L) { return L >= LLow; }, 2, 2);
		const TArray<float> B2 = Indicator([](uint8 L) { return L >= LPlateau; }, 1, 2);
		const TArray<float> B3 = Indicator([](uint8 L) { return L >= LHigh; }, 1, 2);
		const TArray<float> B4 = Indicator([](uint8 L) { return L == LMountain; }, 1, 2);
		const TArray<float> B4W = Indicator([](uint8 L) { return L == LMountain; }, 5, 3);
		TrackField.SetNumUninitialized(NL * NL);
		for (int32 C = 0; C < NL * NL; ++C) TrackField[C] = PathMask[C] ? 1.0f : 0.0f;
		Blur(TrackField, NL, 1, 1);
		Sym.Enforce(TrackField, NL);

		const uint32 SeedC = SubSeed(7), SeedR = SubSeed(8), SeedU = SubSeed(9);
		const float MountainAmp = 14.0f + 40.0f * S.Relief;
		const float BaseH = LevelHeight(BaseLevel);
		M.Height.SetNumZeroed(N * N);
		MountainField.SetNumZeroed(N * N);
		auto Contour = [&](FVector2f Q, int32) { return Fbm(Q.X / 26.0f, Q.Y / 26.0f, SeedC, 3); };
		auto Ridge = [&](FVector2f Q, int32) { return Ridged(Q.X / 150.0f, Q.Y / 150.0f, SeedR, 5); };
		auto Undulate = [&](FVector2f Q, int32) { return Fbm(Q.X / 55.0f, Q.Y / 55.0f, SeedU, 3) + 1.6f * Fbm(Q.X / 190.0f, Q.Y / 190.0f, SeedU + 5u, 2); };

		ParallelFor(N, [&](int32 J)
		{
			for (int32 I = 0; I < N; ++I)
			{
				const int32 V = M.Index(I, J);
				if (Sym.Canon(FIntPoint(I, J), N) != V) continue;
				const FVector2f P = M.CellPos(I, J);
				const float U = (P.X + Half) / LogicCell - 0.5f, W = (P.Y + Half) / LogicCell - 0.5f;
				const float C = 0.16f * Sym.Field(P, Contour, true);
				const float S1 = FMath::SmoothStep(0.15f, 0.85f, SampleLogic(B1, NL, U, W) + 0.5f * C);
				const float S2 = FMath::SmoothStep(0.33f, 0.67f, SampleLogic(B2, NL, U, W) + C);
				const float S3 = FMath::SmoothStep(0.33f, 0.67f, SampleLogic(B3, NL, U, W) + C);
				const float S4 = FMath::SmoothStep(0.35f, 0.65f, SampleLogic(B4, NL, U, W) + C);
				const float Floor = WaterSurface - (1.2f + 4.5f * (1.0f - SampleLogic(B1W, NL, U, W)));
				float H = Floor + (LevelHeight(LLow) - Floor) * S1 + LevelStep * S2 + LevelStep * S3;
				// montanha: cresce pra dentro da região, cristas "ridged"
				const float Core = FMath::SmoothStep(0.3f, 1.0f, SampleLogic(B4W, NL, U, W));
				const float R = Sym.Field(P, Ridge, false);
				H += S4 * (5.0f + MountainAmp * Core * (0.35f + 0.65f * R));
				// ondulação leve fora das montanhas (platôs continuam legíveis); a
				// baixada nunca afunda até a água (água só onde o mapa decidiu)
				H += (0.4f + 0.4f * S.Relief) * Sym.Field(P, Undulate, true) * (1.0f - S4) * S1;
				H = FMath::Max(H, FMath::Lerp(H, WaterSurface + 0.5f, S1));
				// base: plana no nível dela
				for (const FVector2f& St : StartPos)
				{
					const float D = FVector2f::Distance(P, St);
					H = FMath::Lerp(H, BaseH, 1.0f - FMath::SmoothStep(BaseRadius - 6.0f, BaseRadius + 8.0f, D));
				}
				M.Height[V] = H;
				MountainField[V] = S4 * (0.5f + 0.5f * Core);
			}
		});
		Sym.Enforce(M.Height, N);
		Sym.Enforce(MountainField, N);

		// rampas: rasterizadas por cima (o conjunto é simétrico)
		RampWeight.SetNumZeroed(N * N);
		for (const FRamp& R : Ramps)
		{
			const float Reach = FMath::Sqrt(FMath::Square(RampHalfLength + 4.0f) + FMath::Square(RampHalfWidth + 4.0f));
			const FVector2f G = M.ToGrid(R.Center);
			const int32 Rad = FMath::CeilToInt(Reach / M.Cell);
			for (int32 J = FMath::Max(0, int32(G.Y) - Rad); J <= FMath::Min(N - 1, int32(G.Y) + Rad); ++J)
			{
				for (int32 I = FMath::Max(0, int32(G.X) - Rad); I <= FMath::Min(N - 1, int32(G.X) + Rad); ++I)
				{
					float RH;
					const float W = RampSample(R, M.CellPos(I, J), RH);
					if (W > 0.0f)
					{
						const int32 V = M.Index(I, J);
						M.Height[V] = FMath::Lerp(M.Height[V], RH, W);
						RampWeight[V] = FMath::Max(RampWeight[V], W);
					}
				}
			}
		}
		Sym.Enforce(M.Height, N);
		Sym.Enforce(RampWeight, N);

		// erosão nas encostas e montanhas (filtro analítico do Rune Johansen)
		if (S.Erosion > 0.01f)
		{
			ErosionFilter::FParams Prm;
			Prm.Scale = 26.0f;                 // listras de 26, 13 e 6.5 m
			Prm.Octaves = 3;
			Prm.Strength = 0.10f + 0.16f * S.Erosion;
			Prm.Seed = SubSeed(10);
			const TArray<float> Base = M.Height;
			ParallelFor(N, [&](int32 J)
			{
				for (int32 I = 0; I < N; ++I)
				{
					const int32 V = M.Index(I, J);
					if (Sym.Canon(FIntPoint(I, J), N) != V) continue;
					if (RampWeight[V] > 0.0f) continue;
					const int32 I0 = FMath::Max(I - 1, 0), I1 = FMath::Min(I + 1, N - 1), J0 = FMath::Max(J - 1, 0), J1 = FMath::Min(J + 1, N - 1);
					const FVector2f Grad((Base[M.Index(I1, J)] - Base[M.Index(I0, J)]) / ((I1 - I0) * M.Cell),
						(Base[M.Index(I, J1)] - Base[M.Index(I, J0)]) / ((J1 - J0) * M.Cell));
					const float Mask = FMath::Max(MountainField[V], FMath::SmoothStep(0.2f, 0.6f, Grad.Size()));
					if (Mask < 0.02f) continue;
					const FVector2f P = M.CellPos(I, J);
					const float Fade = MountainField[V] > 0.1f ? FMath::Clamp((Base[V] - LevelHeight(LHigh)) / (0.6f * MountainAmp) * 2.0f - 1.0f, -1.0f, 1.0f) : 0.0f;
					auto Gully = [&](FVector2f Q, int32 Rot)
					{
						const FVector2f GR = Sym.Rot(Grad, Rot);
						return ErosionFilter::Apply(Q, GR.X, GR.Y, Fade, Prm);
					};
					M.Height[V] = Base[V] + Mask * Sym.Field(P, Gully, true);
				}
			});
			Sym.Enforce(M.Height, N);
		}
	}

	// ------------------------------------------------------------------------
	// 8. Camadas do chão, fluxo d'água, passabilidade
	// ------------------------------------------------------------------------
	void FGen::BuildLayers()
	{
		const int32 N = M.N;
		// fluxo D8: cada vértice manda a água acumulada pro vizinho mais baixo
		TArray<int32> Order;
		Order.SetNumUninitialized(N * N);
		for (int32 K = 0; K < N * N; ++K) Order[K] = K;
		Order.Sort([this](int32 A, int32 B) { return M.Height[A] > M.Height[B] || (M.Height[A] == M.Height[B] && A < B); });
		TArray<float> Flow;
		Flow.Init(1.0f, N * N);
		for (int32 V : Order)
		{
			const int32 I = V % N, J = V / N;
			int32 Best = -1;
			float BestDrop = 0.0f;
			for (int32 DJ = -1; DJ <= 1; ++DJ)
			{
				for (int32 DI = -1; DI <= 1; ++DI)
				{
					if ((DI | DJ) == 0) continue;
					const int32 X = I + DI, Y = J + DJ;
					if (X < 0 || Y < 0 || X >= N || Y >= N) continue;
					const float Drop = (M.Height[V] - M.Height[M.Index(X, Y)]) / ((DI && DJ) ? 1.41421356f : 1.0f);
					if (Drop > BestDrop) { BestDrop = Drop; Best = M.Index(X, Y); }
				}
			}
			if (Best >= 0) Flow[Best] += Flow[V];
		}
		Sym.Enforce(Flow, N);

		const uint32 SeedN = SubSeed(11);
		M.Layers.SetNumUninitialized(N * N);
		M.Pass.SetNumUninitialized(N * N);
		M.Flow.SetNumUninitialized(N * N);
		M.Territory.SetNumUninitialized(N * N);
		auto Patches = [&](FVector2f Q, int32) { return Fbm(Q.X / 70.0f, Q.Y / 70.0f, SeedN, 4); };
		auto Grain = [&](FVector2f Q, int32) { return Fbm(Q.X / 14.0f, Q.Y / 14.0f, SeedN + 3u, 3); };
		auto Lobes = [&](FVector2f Q, int32) { return Fbm(Q.X / 6.0f, Q.Y / 6.0f, SeedN + 5u, 3); };
		auto MidLobes = [&](FVector2f Q, int32) { return Fbm(Q.X / 9.0f, Q.Y / 9.0f, SeedN + 6u, 3); };
		auto BigLobes = [&](FVector2f Q, int32) { return Fbm(Q.X / 22.0f, Q.Y / 22.0f, SeedN + 7u, 3); };
		ParallelFor(N, [&](int32 J)
		{
			for (int32 I = 0; I < N; ++I)
			{
				const int32 V = M.Index(I, J);
				const FVector2f P = M.CellPos(I, J);
				const int32 I0 = FMath::Max(I - 1, 0), I1 = FMath::Min(I + 1, N - 1), J0 = FMath::Max(J - 1, 0), J1 = FMath::Min(J + 1, N - 1);
				const float Slope = FVector2f((M.Height[M.Index(I1, J)] - M.Height[M.Index(I0, J)]) / ((I1 - I0) * M.Cell),
					(M.Height[M.Index(I, J1)] - M.Height[M.Index(I, J0)]) / ((J1 - J0) * M.Cell)).Size();
				const float H = M.Height[V];
				const float U = (P.X + Half) / LogicCell - 0.5f, W = (P.Y + Half) / LogicCell - 0.5f;
				const float Patch = Sym.Field(P, Patches, true);
				const float Fine = Sym.Field(P, Grain, true);
				const float Fl = FMath::Clamp(FMath::Loge(Flow[V]) / 9.0f, 0.0f, 1.0f);
				M.Flow[V] = Fl;

				float Rock = FMath::SmoothStep(0.42f, 0.80f, Slope + 0.12f * Fine);
				Rock = FMath::Max(Rock, MountainField[V] * FMath::SmoothStep(0.15f, 0.45f, Slope + 0.2f * Patch));
				// areia: a faixa da margem (o indicador de terra cai perto da água) e o fundo raso
				const float Shore = 1.0f - FMath::SmoothStep(0.55f, 0.92f, SampleLogic(ShoreField, NL, U, W) + 0.12f * Fine);
				float Sand = FMath::Max(Shore, 1.0f - FMath::SmoothStep(WaterSurface - 0.2f, WaterSurface + 0.6f, H));
				// terra batida das bases e dos campos de recurso: sem disco perfeito.
				// A borda é deformada por ruído (domain warp, Quilez "warp"), em
				// lóbulos de ~6 m nos campos (o campo do Age of Empires é um grupo
				// apertado de manchas) e de ~22 m nas bases, com transição de ~4 m
				// (2 células da grade). O ruído é o simétrico: os jogadores veem o mesmo
				float Dirt = 0.0f;
				for (const FVector2f& St : StartPos)
				{
					const float D = FVector2f::Distance(P, St);
					if (D > BaseRadius + 40.0f) continue;
					const float Sh = D + 9.0f * Sym.Field(P, BigLobes, true) + 3.0f * Fine;
					Dirt = FMath::Max(Dirt, 1.0f - FMath::SmoothStep(BaseRadius - 6.0f, BaseRadius + 2.0f, Sh));
				}
				const float Track = SampleLogic(TrackField, NL, U, W);
				Dirt = FMath::Max(Dirt, 0.85f * FMath::SmoothStep(0.25f, 0.6f, Track + 0.25f * Fine));
				// (as rampas não ganham mais um quadrado de terra: a trilha passa por elas)
				for (const FWorldResource& R : M.Resources)
				{
					const float D = FVector2f::Distance(P, R.Pos) / R.Radius;
					if (D > 2.4f) continue;
					// lóbulos do tamanho do campo (~9 m, meio raio de fundo) e finos
					// (~6 m) somados: de longe o campo não lê como disco (o ruído de
					// 22 m é quase constante num campo de 11 m e só mexia o raio
					// inteiro); e manchas soltas em volta, sem anel
					const float Lb = Sym.Field(P, Lobes, true);
					const float Sh = D + 0.55f * Sym.Field(P, MidLobes, true) + 0.2f * Lb + 0.1f * Fine;
					Dirt = FMath::Max(Dirt, 0.9f * (1.0f - FMath::SmoothStep(0.7f, 1.05f, Sh)));
					Dirt = FMath::Max(Dirt, 0.4f * (1.0f - FMath::SmoothStep(1.0f, 2.0f, D)) * FMath::SmoothStep(0.25f, 0.45f, Lb + 0.2f * Fine));
				}
				Dirt = FMath::Max(Dirt, 0.35f * FMath::SmoothStep(0.35f, 0.75f, Patch));   // manchas de terra no campo
				// vegetação: onde a água corre e em manchas largas (campo verde × seco)
				const float Lush = FMath::Clamp(0.30f + 0.5f * Fl + 0.65f * Patch + 0.15f * (1.0f - Track) - 0.4f * Rock, 0.0f, 1.0f);
				M.Layers[V] = FColor(uint8(255.0f * FMath::Clamp(Dirt, 0.0f, 1.0f)), uint8(255.0f * Lush),
					uint8(255.0f * FMath::Clamp(Sand, 0.0f, 1.0f)), uint8(255.0f * FMath::Clamp(Rock, 0.0f, 1.0f)));

				// passabilidade vista pela unidade: água funda, encosta demais ou montanha
				EWorldPass Pass = EWorldPass::Open;
				const FIntPoint LC = LCellOf(P);
				const uint8 L = Level[LIdx(LC.X, LC.Y)];
				if (H < WaterSurface - 0.5f) Pass = EWorldPass::Water;
				else if ((Slope > CliffSlope && RampWeight[V] < 0.5f) || (L == LMountain && MountainField[V] > 0.5f)) Pass = EWorldPass::Cliff;
				M.Pass[V] = Pass;
				M.Territory[V] = Pass == EWorldPass::Open ? Owner[LIdx(LC.X, LC.Y)] : int8(-1);
			}
		});
		Sym.Enforce(M.Layers, N);
		Sym.Enforce(M.Pass, N);
		Sym.Enforce(M.Flow, N);
	}

	// ------------------------------------------------------------------------
	// 9. Árvores e rochas
	// ------------------------------------------------------------------------
	void FGen::PlaceProps()
	{
		M.Props.Reset();
		const uint32 SeedF = SubSeed(12), SeedT = SubSeed(13), SeedK = SubSeed(14);
		const bool bDesert = S.Biome == EWorldBiome::Desert;
		auto Forest = [&](FVector2f Q, int32) { return Fbm(Q.X / 110.0f, Q.Y / 110.0f, SeedF, 4); };
		const float Tau = 0.55f - 0.75f * S.Forests;
		auto Clear = [&](FVector2f P, float Margin)
		{
			for (const FVector2f& St : StartPos)
			{
				if (FVector2f::Distance(P, St) < BaseRadius + Margin) return false;
			}
			for (const FWorldResource& R : M.Resources)
			{
				if (FVector2f::Distance(P, R.Pos) < R.Radius + 0.5f * Margin) return false;
			}
			for (const FRamp& R : Ramps)
			{
				if (FVector2f::Distance(P, R.Center) < RampHalfLength + 6.0f) return false;
			}
			return true;
		};
		auto Scatter = [&](float Spacing, uint32 Seed, TFunctionRef<bool(FVector2f, FRng&, FWorldProp&)> Decide)
		{
			const int32 G = FMath::Max(2, FMath::RoundToInt(S.MapSize / Spacing));
			const float Step = S.MapSize / G;
			for (int32 V = 0; V < G; ++V)
			{
				for (int32 U = 0; U < G; ++U)
				{
					// só a célula canônica decide; as outras da órbita são cópias giradas
					if (Sym.Canon(FIntPoint(U, V), G) != V * G + U) continue;
					FRng Rng(Hash(U, V, Seed));
					const FVector2f P((U + 0.5f) * Step - 0.5f * S.MapSize + (Rng.Float() - 0.5f) * 0.9f * Step,
						(V + 0.5f) * Step - 0.5f * S.MapSize + (Rng.Float() - 0.5f) * 0.9f * Step);
					FWorldProp Prop;
					if (!Decide(P, Rng, Prop)) continue;
					for (int32 R = 0; R < Sym.K; ++R)
					{
						FWorldProp X = Prop;
						X.Pos = Sym.Rot(P, R);
						X.Z = M.HeightAt(X.Pos);
						X.Yaw = Prop.Yaw + R * 360.0f / Sym.K;
						M.Props.Add(X);
					}
				}
			}
		};

		// árvores: manchas de floresta (ruído) onde é plano, seco o bastante e
		// longe de base, recurso, rampa e trilha; árvores soltas pelo campo
		if (S.Forests > 0.01f)
		{
			Scatter(5.0f, SeedT, [&](FVector2f P, FRng& Rng, FWorldProp& Prop)
			{
				const FVector3f Nrm = M.NormalAt(P);
				const float H = M.HeightAt(P);
				if (Nrm.Z < 0.92f || H < WaterSurface + 0.8f || !Clear(P, 22.0f)) return false;
				const FVector2f G = M.ToGrid(P);
				const int32 V = M.Index(FMath::Clamp(FMath::RoundToInt(G.X), 0, M.N - 1), FMath::Clamp(FMath::RoundToInt(G.Y), 0, M.N - 1));
				if (M.Pass[V] != EWorldPass::Open || MountainField[V] > 0.4f) return false;
				const FVector2f LG((P.X + Half) / LogicCell - 0.5f, (P.Y + Half) / LogicCell - 0.5f);
				if (SampleLogic(TrackField, NL, LG.X, LG.Y) > 0.08f) return false;
				const float Lush = M.Layers[V].G / 255.0f;
				const float F = FMath::SmoothStep(Tau, Tau + 0.12f, Sym.Field(P, Forest, true));
				float Prob = F * 0.85f * (0.5f + 0.7f * Lush) + 0.012f * S.Forests;
				if (bDesert) Prob *= 0.2f;
				if (Rng.Float() >= Prob) return false;
				Prop.Type = EWorldPropType::Tree;
				Prop.Scale = Rng.Range(0.75f, 1.25f);
				Prop.Yaw = Rng.Range(0.0f, 360.0f);
				Prop.Variant = uint8(Rng.Int(256));
				return true;
			});
		}
		// rochas: no pé e na beira dos penhascos, nas montanhas, e umas soltas
		Scatter(8.0f, SeedK, [&](FVector2f P, FRng& Rng, FWorldProp& Prop)
		{
			const float H = M.HeightAt(P);
			if (H < WaterSurface + 0.3f || !Clear(P, 12.0f)) return false;
			const FVector2f G = M.ToGrid(P);
			const int32 V = M.Index(FMath::Clamp(FMath::RoundToInt(G.X), 0, M.N - 1), FMath::Clamp(FMath::RoundToInt(G.Y), 0, M.N - 1));
			const float Rock = M.Layers[V].A / 255.0f;
			const float Mtn = MountainField[V];
			const FVector3f Nrm = M.NormalAt(P);
			float Prob = 0.005f;
			if (Rock > 0.12f && Rock < 0.6f && Nrm.Z > 0.75f) Prob = 0.22f;
			if (Mtn > 0.5f && Nrm.Z > 0.6f) Prob = 0.12f;
			if (bDesert) Prob *= 1.8f;
			if (Rng.Float() >= Prob) return false;
			Prop.Type = EWorldPropType::Rock;
			Prop.Scale = Mtn > 0.5f ? Rng.Range(1.2f, 3.2f) : Rng.Range(0.5f, 1.6f);
			Prop.Yaw = Rng.Range(0.0f, 360.0f);
			Prop.Variant = uint8(Rng.Int(256));
			return true;
		});
	}

	// ------------------------------------------------------------------------
	void FGen::Finish()
	{
		M.Starts.Reset();
		for (int32 K = 0; K < StartPos.Num(); ++K)
		{
			FWorldStart St;
			St.Player = K;
			St.Pos = StartPos[K];
			St.Z = M.HeightAt(StartPos[K]);
			M.Starts.Add(St);
		}
		for (FWorldResource& R : M.Resources)
		{
			R.Z = M.HeightAt(R.Pos);
		}
		int32 Open = 0;
		for (int32 C = 0; C < NL * NL; ++C) Open += Passable(C) ? 1 : 0;
		M.OpenFraction = float(Open) / float(NL * NL);
		M.RampsCarved = Ramps.Num();
		bool bWater = false;
		for (uint8 L : Level) bWater |= L == LWater;
		M.WaterLevel = bWater ? WaterSurface : -1000.0f;
	}

	void FGen::Run()
	{
		auto Time = [this](const TCHAR* Name, TFunctionRef<void()> Step)
		{
			const double T0 = FPlatformTime::Seconds();
			Step();
			M.Timings.Add(TPair<FString, double>(Name, 1000.0 * (FPlatformTime::Seconds() - T0)));
		};
		M.Settings = S;
		M.Cell = FWorldGenSettings::CellSize;
		M.N = S.GridSize();
		Half = 0.5f * (M.N - 1) * M.Cell;
		NL = FMath::RoundToInt(2.0f * Half / LogicCell);

		for (Attempt = 0; Attempt < 4; ++Attempt)
		{
			M.Timings.Reset();
			Time(TEXT("bases"), [this] { PlaceStarts(); });
			Time(TEXT("levels"), [this] { MakeLevels(); Cleanup(); });
			Time(TEXT("paths"), [this] { CarvePaths(); });
			Time(TEXT("distances"), [this]
			{
				TArray<int32> Src;
				TArray<int8> Who;
				for (int32 K = 0; K < StartPos.Num(); ++K)
				{
					const FIntPoint C = LCellOf(StartPos[K]);
					Src.Add(LIdx(C.X, C.Y));
					Who.Add(int8(K));
				}
				Distances(Src, &Owner, &Who);
				D0 = Distances({ Src[0] }, nullptr, nullptr);
				M.bConnected = true;
				for (int32 C : Src) M.bConnected &= D0[C] < 1e8f;
				// distância de "rush" (o termo do StarCraft II): da base 1 até a
				// inimiga mais perto, pelo caminho; pela simetria vale pra todas
				M.RushMeters = 1e9f;
				M.RushStraight = 1e9f;
				for (int32 K = 1; K < Src.Num(); ++K)
				{
					M.RushMeters = FMath::Min(M.RushMeters, D0[Src[K]]);
					M.RushStraight = FMath::Min(M.RushStraight, FVector2f::Distance(StartPos[0], StartPos[K]));
				}
			});
			if (M.bConnected) break;
			UE_LOG(LogTemp, Warning, TEXT("FORGE seed %d attempt %d: bases not connected, re-rolling"), S.Seed, Attempt);
		}
		Time(TEXT("resources"), [this] { PlaceResources(); });
		Time(TEXT("heights"), [this] { BuildHeights(); });
		Time(TEXT("layers"), [this] { BuildLayers(); });
		Time(TEXT("props"), [this] { PlaceProps(); });
		Finish();
	}
}

TSharedPtr<FWorldMap> FWorldGenerator::Generate(const FWorldGenSettings& Settings)
{
	TSharedPtr<FWorldMap> Map = MakeShared<FWorldMap>();
	FGen Gen(Settings, *Map);
	Gen.Run();
	return Map;
}
