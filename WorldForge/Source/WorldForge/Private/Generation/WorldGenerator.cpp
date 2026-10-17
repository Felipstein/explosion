#include "Generation/WorldGenerator.h"

#include "Algo/Reverse.h"
#include "Async/ParallelFor.h"
#include "Generation/ErosionFilter.h"
#include "Generation/WorldNoise.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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
		TArray<uint8> FrameMask;     // 1 = moldura do mapa (a borda), fora do filtro de corpo
		TArray<uint8> FrameKind;     // tipo da moldura na célula: 0 nenhum, 1 serra, 2 colinas, 3 mar
		TArray<float> OpenField;     // colinas da moldura por vértice (0–1)
		int32 BodyDemoted = 0;       // formas sem corpo que desceram de nível (KeepBody)
		TArray<uint8> BasinCell;     // grade lógica: 1 salina, 2 oásis (só no deserto)
		TArray<float> SaltField;     // salina desfocada na grade lógica (contorno suave)
		TArray<float> OasisField;    // oásis desfocado (~25 m em volta da água), grade lógica
		TArray<uint8> DipMask;       // borda de platô que é encosta larga (cuesta): passável sem rampa
		TArray<float> TalusField;    // saia de tálus no pé dos maciços, por vértice (0–1)
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

		// monólitos (ReliefType 1): pilares de rocha de parede quase reta
		struct FPillar
		{
			FVector2f C;
			float A = 10.0f, B = 8.0f;   // raios da pegada (m)
			float Yaw = 0.0f;            // rad
			float H = 22.0f;             // altura acima do chão (m)
			uint32 Seed = 0;
			float Pond = 0.0f;           // o oásis encostado: largura (m), 0 = sem
			float PondPhi = 0.0f;        // a direção do meio do primeiro crescente (rad)
			int32 Variant = -1;          // a peça do kit de malhas (MonoKit), −1 = o pilar no relevo (sem o kit)
			float Scale = 1.0f;
		};
		/** O kit de malhas (Config/WorldForge/Monoliths.json, gerado pelo Tools/blender_monoliths.py). */
		struct FMonoPiece
		{
			FString Name;
			float Height = 30.0f;
			TArray<float> Foot;          // o raio da planta a 1 m de altura, 32 direções a partir de +X (m)
			bool bMarker = false;
		};
		TArray<FMonoPiece> MonoKit;
		void LoadMonoKit()
		{
			MonoKit.Reset();
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectConfigDir() / TEXT("WorldForge/Monoliths.json")))) return;
			TSharedPtr<FJsonObject> J;
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid()) return;
			for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("pieces")))
			{
				const TSharedPtr<FJsonObject>& O = V->AsObject();
				FMonoPiece& P = MonoKit.AddDefaulted_GetRef();
				P.Name = O->GetStringField(TEXT("name"));
				P.Height = float(O->GetNumberField(TEXT("height_m")));
				for (const TSharedPtr<FJsonValue>& F : O->GetArrayField(TEXT("foot_r_m"))) P.Foot.Add(float(F->AsNumber()));
				P.bMarker = O->GetStringField(TEXT("kind")) == TEXT("dome");
			}
		}
		/** O raio da pegada (m) na direção Th (rad, do mundo): a peça do kit girada e na escala; sem o kit, a elipse. */
		float FootR(const FPillar& P, float Th) const
		{
			if (P.Variant >= 0 && MonoKit.IsValidIndex(P.Variant) && MonoKit[P.Variant].Foot.Num() >= 4)
			{
				const TArray<float>& F = MonoKit[P.Variant].Foot;
				const float U = FMath::Fmod(FMath::UnwindRadians(Th - P.Yaw) + 2.0f * PI, 2.0f * PI) / (2.0f * PI) * F.Num();
				const int32 I = int32(U) % F.Num();
				return FMath::Lerp(F[I], F[(I + 1) % F.Num()], U - int32(U)) * P.Scale;
			}
			const float Cs = FMath::Cos(Th - P.Yaw), Sn = FMath::Sin(Th - P.Yaw);
			return 1.0f / FMath::Sqrt(FMath::Square(Cs / P.A) + FMath::Square(Sn / P.B));
		}
		/** 0..1: o quanto a direção Th (rad, em volta do pilar) cai num crescente do oásis. */
		float PondSector(const FPillar& P, float Th) const
		{
			if (P.Pond <= 0.0f) return 0.0f;
			const float Step = 2.0f * PI / Sym.K;
			const float D = FMath::Abs(FMath::UnwindRadians(Th - P.PondPhi - Step * FMath::RoundToFloat((Th - P.PondPhi) / Step)));
			return 1.0f - FMath::SmoothStep(FMath::DegreesToRadians(40.0f), FMath::DegreesToRadians(80.0f), D);
		}
		/** Metros pra dentro do crescente do oásis no ponto L (relativo ao pilar); negativo = fora. */
		float PondInside(const FPillar& P, FVector2f L) const
		{
			if (P.Pond <= 0.0f) return -1e9f;
			const float Th = FMath::Atan2(L.Y, L.X);
			float Out;   // metros fora da parede
			if (P.Variant >= 0)
			{
				Out = L.Size() - FootR(P, Th);   // a pegada da malha
			}
			else
			{
				const float Cs = FMath::Cos(P.Yaw), Sn = FMath::Sin(P.Yaw);
				const float Lu = (L.X * Cs + L.Y * Sn) / P.A, Lv = (-L.X * Sn + L.Y * Cs) / P.B;
				const float D = FMath::Sqrt(Lu * Lu + Lv * Lv);
				if (D <= 1.0f) return -1e9f;
				Out = L.Size() * (1.0f - 1.0f / D);   // (elipse)
			}
			if (Out <= 0.0f) return -1e9f;
			// a borda de fora ondula (sorteio pelo ângulo dentro do crescente: as cópias iguais)
			const float Step = 2.0f * PI / Sym.K;
			const float Rel = FMath::UnwindRadians(Th - P.PondPhi - Step * FMath::RoundToFloat((Th - P.PondPhi) / Step));
			const float Width = P.Pond * PondSector(P, Th) * (0.85f + 0.3f * Fbm(Rel * 1.3f, 0.37f, P.Seed + 21u, 2));
			// 7 m da parede (a superelipse passa da elipse nas quinas): a faixa das palmeiras
			return FMath::Min(Out - 7.0f, 7.0f + Width - Out);
		}
		TArray<FPillar> Pillars;
		TArray<uint8> PillarMask;    // grade lógica: 1 = pilar (não passa)

		// grade visível
		TArray<float> RampWeight;
		TArray<float> MountainField; // S4 por vértice (montanha)
		TArray<float> PillarField;   // pilar por vértice (0–1): parede e topo

		// morros e descidas (16/10, S.Hills; docs/WORLDFORGE.md item 7): formas inteiras com intenção no chão aberto. A
		// lista guarda a órbita toda (a forma e as cópias giradas, cada uma no referencial dela: a simetria sai exata)
		struct FForm
		{
			FVector2f C = FVector2f::ZeroVector;
			float A = 60.0f, B = 40.0f;  // semieixos (m): A ao longo do giro, B atravessado
			float Yaw = 0.0f;            // rad
			float H = 4.0f;              // altura (m)
			uint8 Type = 0;              // 0 morro, 1 crista, 2 mesa, 3 vale (dois ombros com o leito no meio)
			float Steep = 1.0f;          // o lado mais íngreme (sinal no eixo B)
			uint32 Seed = 0;
		};
		TArray<FForm> FormList;
		TArray<float> FormField;     // por vértice: o quanto é forma (0–1), a altura normalizada
		TArray<float> FormDB, FormDP;   // grade lógica: a distância (m) à borda de nível/ao bloqueado e à trilha (PlaceForms)

		/** A altura das formas no ponto (m) e o quanto ele é forma (0–1). Tudo no referencial da forma: o contorno entortado
		 *  por ruído (18% dos eixos, comprimento de 0,9 A) e o lado íngreme 30% mais curto. Perfis: o morro e a crista pela
		 *  smoothstep do centro ao pé (topo redondo, pé macio, nunca linear: a borda linear do WorldBuilder deixa vinco); a
		 *  mesa com o topo plano até 35% e a borda até ~22°; o vale com dois ombros ao longo do eixo e o leito no meio. */
		float FormHeight(FVector2f P, float& OutN) const
		{
			float Best = 0.0f;
			OutN = 0.0f;
			for (const FForm& F : FormList)
			{
				const FVector2f D = P - F.C;
				const float Reach = 1.35f * F.A + 20.0f;
				if (D.SizeSquared() > Reach * Reach) continue;
				const float Cs = FMath::Cos(F.Yaw), Sn = FMath::Sin(F.Yaw);
				float U = D.X * Cs + D.Y * Sn, Vv = -D.X * Sn + D.Y * Cs;
				const float Lw = 0.8f * F.A;
				const float U0 = U, V0 = Vv;
				U += 0.25f * F.A * Fbm(U0 / Lw, V0 / Lw, F.Seed, 3);
				Vv += 0.25f * F.B * Fbm(U0 / Lw + 7.3f, V0 / Lw - 3.1f, F.Seed + 1u, 3);
				float Hf, Nf;
				if (F.Type == 3)
				{
					const float Al = FMath::Abs(U) / F.A, Ac = FMath::Abs(Vv) / F.B;
					const float Sh = 1.0f - FMath::SmoothStep(0.0f, 1.0f, FMath::Abs(Ac - 0.55f) / 0.45f);
					Hf = F.H * Sh * (1.0f - FMath::SmoothStep(0.55f, 1.0f, Al));
					Nf = Sh * (1.0f - FMath::SmoothStep(0.55f, 1.0f, Al)) + (Ac < 0.6f && Al < 1.0f ? 0.25f : 0.0f);
				}
				else
				{
					const float Side = Vv * F.Steep > 0.0f ? 1.3f : 1.0f;
					float Nn = FMath::Sqrt(FMath::Square(U / F.A) + FMath::Square(Vv * Side / F.B));
					// (16/10, a 1ª medida: o morro redondo e a crista lisa liam como bolha) o morro e a crista grandes têm 1–2
					// lóbulos a mais, encostados no corpo (o morro de verdade é um maciço de corpos, como os do ZH)
					if (F.Type <= 1 && F.A > 45.0f)
					{
						const int32 NLobes = 1 + int32(Hash(F.Seed + 7u) % 2u);
						for (int32 Lb = 0; Lb < NLobes; ++Lb)
						{
							const float Ang = (Hash(F.Seed + 11u + Lb) % 6283u) / 1000.0f;
							const float Off = 0.55f + 0.25f * ((Hash(F.Seed + 19u + Lb) % 1000u) / 1000.0f);
							const float Sz = 0.45f + 0.2f * ((Hash(F.Seed + 23u + Lb) % 1000u) / 1000.0f);
							const float Lu = U - FMath::Cos(Ang) * Off * F.A, Lv = Vv - FMath::Sin(Ang) * Off * F.B;
							Nn = FMath::Min(Nn, FMath::Sqrt(FMath::Square(Lu / (Sz * F.A)) + FMath::Square(Lv * Side / (Sz * F.B))) / (0.75f + 0.25f * Sz));
						}
					}
					const float S0 = F.Type == 2 ? 0.35f : 0.0f;
					const float Prof = 1.0f - FMath::SmoothStep(S0, 1.0f, Nn);
					// o corpo: a altura varia devagar ao longo da forma (ombros, um cume mais alto que o outro)
					Hf = F.H * Prof * (1.0f + 0.15f * Fbm(U0 / (0.6f * F.A), V0 / (0.6f * F.A), F.Seed + 2u, 2));
					Nf = FMath::Max(Prof, 1.0f - FMath::SmoothStep(0.85f, 1.1f, Nn) > 0.0f ? 0.15f * (1.0f - FMath::SmoothStep(0.85f, 1.1f, Nn)) : 0.0f);
				}
				if (Hf > Best) Best = Hf;
				OutN = FMath::Max(OutN, FMath::Clamp(Nf, 0.0f, 1.0f));
			}
			return Best;
		}

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
		bool Passable(int32 C) const { return Level[C] >= LLow && Level[C] <= LHigh && !(PillarMask.Num() && PillarMask[C]); }

		/** Passo entre células vizinhas (4-vizinhança): mesmo nível, ou desnível de 1 com rampa. */
		bool CanMove(int32 A, int32 B) const
		{
			if (!Passable(A) || !Passable(B)) return false;
			const int32 D = FMath::Abs(int32(Level[A]) - int32(Level[B]));
			return D == 0 || (D == 1 && (RampMask[A] || RampMask[B] || (DipMask.Num() && DipMask[A] && DipMask[B])));
		}

		void Run();
		void PlaceStarts();
		void MakeLevels();
		void Cleanup();
		void KeepBody(uint8 T);
		void MakeBasins();
		void MakeCuestas();
		void MakeMonoliths();
		void ForceBases();
		void CarvePaths();
		TArray<int32> CarveSearch(const TArray<int32>& Sources, const TArray<uint8>& IsTarget, const TArray<float>* Extra) const;
		void CarveOrbit(const TArray<int32>& Path, float MinRampSpacing, EWorldTrack Kind);
		TArray<float> Distances(const TArray<int32>& Sources, TArray<int8>* OutOwner, const TArray<int8>* SourceOwner) const;
		TArray<uint8> Reach(int32 From) const;
		void PlaceResources();
		void PlaceForms();
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
		// montanha do miolo: maciço (ruído liso com domain warp), não crista. O
		// "ridged" de antes já nascia como uma rede de cristas finas, e o topo
		// dele virava minhoca de montanha (07/10)
		auto Ridges = [&](FVector2f Q, int32)
		{
			const float F = 1.0f / (0.2f * S.MapSize);
			const float WX = Fbm(Q.X * F * 0.7f + 7.3f, Q.Y * F * 0.7f - 2.1f, SeedM + 3u, 2);
			const float WY = Fbm(Q.X * F * 0.7f - 4.4f, Q.Y * F * 0.7f + 6.6f, SeedM + 4u, 2);
			return Fbm(Q.X * F + 0.7f * WX, Q.Y * F + 0.7f * WY, SeedM, 3);
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
				Mt[C] = Sym.Field(P, Ridges, true);
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
		// monólitos (o AlUla): o miolo é areia aberta; o relevo vem dos pilares
		// (MakeMonoliths), não dos morros do ruído, e sobram poucos platôs
		const float FPlateau = (S.ReliefType == 1 ? 0.12f : 0.42f) * S.Plateaus;
		const float FMountain = S.ReliefType == 1 ? 0.0f : 0.10f * S.Relief;
		const float TWater = FWater > 0.001f ? Quantile(InteriorE, FWater) : -1e9f;
		const float T2 = FPlateau > 0.001f ? Quantile(InteriorE, 1.0f - FPlateau) : 1e9f;
		const float TM = FMountain > 0.001f ? Quantile(InteriorM, 1.0f - FMountain) : 1e9f;

		// a borda: tipo do mapa (sorteado pelo bioma, ou o do painel). Lados
		// opostos têm o mesmo tipo (o giro de 180° leva um no outro); com 4
		// jogadores (giro de 90°) os quatro, e "mar em dois lados" vira ilha
		{
			FRng Rng(SubSeed(16));
			int32 F = S.Frame;
			if (F < 0)
			{
				// pesos por bioma: Serra, Colinas, Costa, Ilha
				static const float Weights[3][4] = { { 0.30f, 0.25f, 0.35f, 0.10f }, { 0.25f, 0.35f, 0.30f, 0.10f }, { 0.40f, 0.30f, 0.25f, 0.05f } };
				const float* Wt = Weights[FMath::Clamp(int32(S.Biome), 0, 2)];
				float X = Rng.Float() * (Wt[0] + Wt[1] + Wt[2] + Wt[3]);
				F = 0;
				while (F < 3 && X >= Wt[F]) { X -= Wt[F]; ++F; }
			}
			if (Sym.K == 4 && F == int32(EWorldFrame::Coast)) F = int32(EWorldFrame::Island);
			M.Frame = EWorldFrame(F);
			uint8 Sides[2] = { 1, 1 };   // par norte/sul, par leste/oeste
			if (M.Frame == EWorldFrame::Open) Sides[0] = Sides[1] = 2;
			else if (M.Frame == EWorldFrame::Island) Sides[0] = Sides[1] = 3;
			else if (M.Frame == EWorldFrame::Coast)
			{
				const int32 Sea = Rng.Float() < 0.5f ? 0 : 1;
				Sides[Sea] = 3;
				Sides[1 - Sea] = Rng.Float() < 0.5f ? 1 : 2;
			}
			M.FrameSides[0] = M.FrameSides[1] = Sides[0];
			M.FrameSides[2] = M.FrameSides[3] = Sides[1];
		}
		const uint32 SeedCoast = SubSeed(17), SeedIslet = SubSeed(18);
		auto CoastNoise = [&](FVector2f Q, int32) { return Fbm(Q.X / 140.0f, Q.Y / 140.0f, SeedCoast, 3); };
		auto HeadNoise = [&](FVector2f Q, int32) { return Fbm(Q.X / 55.0f, Q.Y / 55.0f, SeedCoast + 9u, 3); };
		auto IsletNoise = [&](FVector2f Q, int32) { return Fbm(Q.X / 30.0f, Q.Y / 30.0f, SeedIslet, 2); };

		Level.SetNumZeroed(NL * NL);
		FrameMask.SetNumZeroed(NL * NL);
		FrameKind.SetNumZeroed(NL * NL);
		ParallelFor(NL, [&](int32 B)
		{
			for (int32 A = 0; A < NL; ++A)
			{
				const int32 C = LIdx(A, B);
				const FVector2f P = LPos(A, B);
				// lado do mapa: o eixo mais longe do centro, com a divisa dos cantos
				// mexida pelo ruído (não uma diagonal reta entre mar e serra)
				const float Jitter = 30.0f * Sym.Field(P, Wobble, true);
				const bool bEW = FMath::Abs(P.X) - FMath::Abs(P.Y) + Jitter > 0.0f;
				const uint8 Kind = M.FrameSides[bEW ? (P.X > 0.0f ? 2 : 3) : (P.Y > 0.0f ? 0 : 1)];
				const float Edge = Half - FMath::Max(FMath::Abs(P.X), FMath::Abs(P.Y));
				uint8 L = LLow;
				if (Kind == 3)
				{
					// mar: faixa 1,6× mais larga, costa recortada em baías e pontas
					// (~140 m), falésia nas pontas, ilhotas no mar
					const float Bd = Edge + 45.0f * Sym.Field(P, CoastNoise, true);
					const float W = 1.6f * BorderWidth;
					if (Bd < W)
					{
						FrameMask[C] = 1;
						FrameKind[C] = 3;
						L = LWater;
						if (Bd > 0.6f * W && Sym.Field(P, HeadNoise, true) > 0.55f) L = LMountain;          // falésia na ponta
						else if (Bd < 0.55f * W && Sym.Field(P, IsletNoise, true) > 0.85f) L = LMountain;    // ilhota
					}
				}
				else if (Border[C] < BorderWidth)
				{
					FrameMask[C] = 1;
					FrameKind[C] = Kind;
					L = LMountain;
				}
				if (!FrameMask[C])
				{
					if (Mt[C] > TM) L = LMountain;
					else if (E[C] < TWater) L = LWater;
					else if (E[C] > T2) L = LPlateau;
				}
				Level[C] = L;
			}
		});
		Sym.Enforce(FrameMask, NL);
		Sym.Enforce(FrameKind, NL);

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
		// monólitos (o AlUla, alula_1v1_candidate): a base em cima do pedestal de pedra, com o paredão em volta e
		// a rampa de saída (as rampas saem da ligação dos caminhos, como em qualquer platô)
		if (S.ReliefType == 1) BaseLevel = FMath::Max(BaseLevel, int32(LPlateau));
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
		// contorno de mesa, não de curva de nível: a máscara de cada degrau é
		// desfocada em ~16 m e cortada de novo no meio (fluxo por curvatura): as
		// dobras menores que ~30 m somem e a borda do platô fica em arcos largos.
		// Com o contorno cru do ruído, a parede serpenteava pelo mapa (07/10)
		for (int32 T = LPlateau; T <= LHigh; ++T)
		{
			TArray<float> F;
			F.SetNumUninitialized(NL * NL);
			for (int32 C = 0; C < NL * NL; ++C) F[C] = (Level[C] >= T && Level[C] != LMountain) ? 1.0f : 0.0f;
			Blur(F, NL, 2, 2);
			Sym.Enforce(F, NL);
			for (int32 C = 0; C < NL * NL; ++C)
			{
				if (Level[C] == LMountain || Level[C] == LWater) continue;
				const bool bIn = Level[C] >= T;
				if (bIn && F[C] < 0.5f) Level[C] = uint8(T - 1);
				else if (!bIn && F[C] >= 0.5f && Level[C] == T - 1) Level[C] = uint8(T);
			}
		}

		// (raio 3 = 12 m na abertura, 2 = 8 m no fechamento: corta pescoço com
		// menos de ~24 m e fecha fenda com menos de ~16 m; pesquisa de 07/10)
		for (int32 T = LHigh; T >= LPlateau; --T)
		{
			auto Morph = [&](const TArray<uint8>& In, bool bErode, int32 R = 2)
			{
				TArray<uint8> Out;
				Out.SetNumZeroed(NL * NL);
				ParallelFor(NL, [&](int32 B)
				{
					for (int32 A = 0; A < NL; ++A)
					{
						bool bAll = true, bAny = false;
						for (int32 DB = -R; DB <= R; ++DB)
						{
							for (int32 DA = -R; DA <= R; ++DA)
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
			const TArray<uint8> Opened = Morph(Morph(Mask, true, 3), false, 3);
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

		// corpo: cada platô e cada maciço do miolo precisa caber um círculo de
		// 28 m de raio (Felzenszwalb; a pesquisa de 07/10) e não ser fita
		// (compacidade 4πA/P² ≥ 0,25 na grade; o círculo dá ~0,6). Quem falha
		// desce um nível (maciço vira baixada): a "minhoca" de 3–20 m de largura
		// e centenas de metros de comprimento deixa de existir
		KeepBody(LHigh);
		KeepBody(LPlateau);
		KeepBody(LMountain);

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
			if (Cells.Num() < MinCells && !FrameMask[Seed])   // (ilhota e falésia da costa ficam)
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

	void FGen::KeepBody(uint8 T)
	{
		// (20 m e 0,18 deixavam faixas de ~40 m × 300 m: ainda fita, 07/10)
		constexpr int32 MinRadius = 7;          // células (28 m: platô de ≥ 56 m de largura)
		constexpr float MinCompact = 0.25f;
		auto In = [&](int32 C)
		{
			if (T == LMountain) return Level[C] == LMountain && !FrameMask[C];
			return Level[C] >= T && Level[C] != LMountain;
		};
		// distância (células, 8 vizinhos) até fora da máscara: BFS de várias fontes
		TArray<int32> Dist;
		Dist.Init(MAX_int32, NL * NL);
		TArray<int32> Queue;
		Queue.Reserve(NL * NL);
		for (int32 C = 0; C < NL * NL; ++C)
		{
			if (!In(C)) { Dist[C] = 0; Queue.Add(C); }
		}
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 C = Queue[Head];
			const int32 A = C % NL, B = C / NL;
			for (int32 DB = -1; DB <= 1; ++DB)
			{
				for (int32 DA = -1; DA <= 1; ++DA)
				{
					const int32 X = A + DA, Y = B + DB;
					if ((DA | DB) == 0 || X < 0 || Y < 0 || X >= NL || Y >= NL) continue;
					const int32 Q = LIdx(X, Y);
					if (Dist[Q] > Dist[C] + 1) { Dist[Q] = Dist[C] + 1; Queue.Add(Q); }
				}
			}
		}
		// componentes: o maior raio inscrito e a compacidade de cada uma
		TArray<int32> Comp;
		Comp.Init(-1, NL * NL);
		TArray<int32> Stack, Cells;
		int32 Demoted = 0, Kept = 0;
		for (int32 Seed = 0; Seed < NL * NL; ++Seed)
		{
			if (Comp[Seed] >= 0 || !In(Seed)) continue;
			Cells.Reset();
			Stack.Reset();
			Stack.Add(Seed);
			Comp[Seed] = Seed;
			int32 MaxR = 0, Perimeter = 0;
			while (Stack.Num())
			{
				const int32 C = Stack.Pop(EAllowShrinking::No);
				Cells.Add(C);
				MaxR = FMath::Max(MaxR, Dist[C]);
				const int32 A = C % NL, B = C / NL;
				const int32 N4[4][2] = { { A + 1, B }, { A - 1, B }, { A, B + 1 }, { A, B - 1 } };
				for (const auto& NB : N4)
				{
					if (NB[0] < 0 || NB[1] < 0 || NB[0] >= NL || NB[1] >= NL) continue;
					const int32 Q = LIdx(NB[0], NB[1]);
					if (In(Q))
					{
						if (Comp[Q] < 0) { Comp[Q] = Seed; Stack.Add(Q); }
					}
					else
					{
						++Perimeter;
					}
				}
			}
			const float Compact = 4.0f * PI * Cells.Num() / FMath::Max(1.0f, float(Perimeter * Perimeter));
			if (MaxR >= MinRadius && Compact >= MinCompact)
			{
				++Kept;
				continue;
			}
			++Demoted;
			for (int32 C : Cells)
			{
				// platô desce um degrau; maciço do miolo vira baixada
				if (T == LMountain) Level[C] = LLow;
				else if (Level[C] == T) Level[C] = uint8(T - 1);
				else if (T == LPlateau && Level[C] == LHigh) Level[C] = LLow;
			}
		}
		UE_LOG(LogTemp, Verbose, TEXT("WorldGen corpo nível %d: %d ficam, %d descem"), int32(T), Kept, Demoted);
		BodyDemoted += Demoted;
	}

	void FGen::MakeCuestas()
	{
		// Cuesta: o platô de estratos inclinados tem um lado em paredão e o outro
		// em encosta larga. Só paredão em volta (o contorno fechado e fino) lia
		// como "minhoca" de cima (revisão de arte de 07/10). A direção da encosta
		// é radial (centro → borda) girada até ±80° por um ruído largo: a mesma
		// regra em toda a órbita da simetria. A borda da encosta é passável
		// (DipMask) e não vale perto das bases (a rampa defensável continua).
		DipMask.SetNumZeroed(NL * NL);
		const uint32 SeedD = SubSeed(19);
		auto DipNoise = [&](FVector2f Q, int32) { return Fbm(Q.X / 320.0f, Q.Y / 320.0f, SeedD, 2); };
		TArray<float> Plat;
		Plat.SetNumUninitialized(NL * NL);
		for (int32 C = 0; C < NL * NL; ++C) Plat[C] = (Level[C] >= LPlateau && Level[C] != LMountain) ? (Level[C] == LHigh ? 2.0f : 1.0f) : 0.0f;
		Blur(Plat, NL, 3, 2);
		int32 Marked = 0;
		for (int32 B = 1; B < NL - 1; ++B)
		{
			for (int32 A = 1; A < NL - 1; ++A)
			{
				const int32 C = LIdx(A, B);
				const uint8 L = Level[C];
				if (L < LPlateau || L == LMountain) continue;
				const FVector2f P = LPos(A, B);
				bool bNearBase = false;
				for (const FVector2f& St : StartPos) bNearBase |= FVector2f::Distance(P, St) < BaseRadius + 45.0f;
				if (bNearBase) continue;
				// normal pra fora do platô (desce o indicador desfocado)
				const FVector2f Out = -FVector2f(Plat[LIdx(A + 1, B)] - Plat[LIdx(A - 1, B)], Plat[LIdx(A, B + 1)] - Plat[LIdx(A, B - 1)]).GetSafeNormal();
				const float Ang = FMath::DegreesToRadians(80.0f) * Sym.Field(P, DipNoise, true);
				const FVector2f Rad = P.SizeSquared() > 1.0f ? P.GetSafeNormal() : FVector2f(1, 0);
				const FVector2f Dip(Rad.X * FMath::Cos(Ang) - Rad.Y * FMath::Sin(Ang), Rad.X * FMath::Sin(Ang) + Rad.Y * FMath::Cos(Ang));
				if ((Out | Dip) < 0.3f) continue;
				const int32 N4[4][2] = { { A + 1, B }, { A - 1, B }, { A, B + 1 }, { A, B - 1 } };
				for (const auto& NB : N4)
				{
					const int32 Q = LIdx(NB[0], NB[1]);
					if (Level[Q] == L - 1 && Level[Q] >= LLow)
					{
						DipMask[C] = 1;
						DipMask[Q] = 1;
						++Marked;
					}
				}
			}
		}
		Sym.Enforce(DipMask, NL);
		UE_LOG(LogTemp, Verbose, TEXT("WorldGen cuestas: %d bordas em encosta"), Marked);
	}

	// ------------------------------------------------------------------------
	// Monólitos (ReliefType 1, o AlUla: docs/concept/maps/alula_1v1_candidate.webp)
	// ------------------------------------------------------------------------
	// Um anel de pilares de arenito em volta do centro, a ~30–55% do caminho da
	// base pro meio, e um rochedo grande no centro (o marco) quando não há vila
	// ali. As regras (docs/WORLDFORGE.md §10):
	// - a rota direta base → centro fica livre (nenhum pilar a menos de 28° dela);
	// - entre dois pilares sobram ≥ 32 m (mais de 2 tanques: ≥ 3 passagens);
	// - longe das bases, da água, das bacias e da moldura;
	// - simetria: cada pilar tem o irmão girado como o mapa.
	// Os pilares do anel têm 18–28 m (silhueta acima da unidade, abaixo do marco)
	// e o marco 38–44 m; a parede e o tálus saem no BuildHeights.
	void FGen::MakeMonoliths()
	{
		Pillars.Reset();
		PillarMask.Init(0, NL * NL);
		if (S.ReliefType != 1 || StartPos.Num() == 0)
		{
			return;
		}
		FRng Rng(SubSeed(31));
		// o kit de malhas (sem ele, o pilar no relevo de antes): a peça dá a pegada, a altura e a escala
		LoadMonoKit();
		TArray<int32> RingKit, MarkKit;
		for (int32 I = 0; I < MonoKit.Num(); ++I) (MonoKit[I].bMarker ? MarkKit : RingKit).Add(I);
		auto Dress = [&](FPillar& P, int32 Piece, float Sc)
		{
			P.Variant = Piece;
			P.Scale = Sc;
			float Mx = 0.0f, Mn = 1e9f;
			for (float F : MonoKit[Piece].Foot) { Mx = FMath::Max(Mx, F); Mn = FMath::Min(Mn, F); }
			P.A = Mx * Sc;
			P.B = Mn * Sc;
			P.H = MonoKit[Piece].Height * Sc;
		};
		const FVector2f St = StartPos[0];
		const float Dist0 = St.Size();
		const float Phi0 = FMath::Atan2(St.Y, St.X);
		// a órbita de um pilar (as cópias giradas)
		auto Orbit = [&](const FPillar& P)
		{
			TArray<FPillar> Out;
			for (int32 K = 0; K < Sym.K; ++K)
			{
				FPillar Q = P;
				Q.C = Sym.Rot(P.C, K);
				Q.Yaw = P.Yaw + K * (2.0f * PI / Sym.K);
				Out.Add(Q);
			}
			return Out;
		};
		// o chão em volta tem de ser jogável: interior, seco, sem bacia, longe das bases
		auto Ground = [&](const FPillar& P, bool bMarker) -> bool
		{
			const float R = FMath::Max(P.A, P.B) + (bMarker ? 4.0f : 10.0f);
			for (const FVector2f& S0 : StartPos)
			{
				if (FVector2f::Distance(S0, P.C) < BaseRadius + 45.0f + R) return false;
			}
			const FIntPoint C0 = LCellOf(P.C - FVector2f(R)), C1 = LCellOf(P.C + FVector2f(R));
			for (int32 B = C0.Y; B <= C1.Y; ++B)
			{
				for (int32 A = C0.X; A <= C1.X; ++A)
				{
					const int32 C = LIdx(A, B);
					if (Level[C] == LWater || Level[C] == LMountain || FrameMask[C] || (!bMarker && BasinCell.Num() && BasinCell[C] && BasinCell[C] != 3)) return false;
				}
			}
			return true;
		};
		auto Apart = [&](const FPillar& P)
		{
			for (const FPillar& Q : Pillars)
			{
				if (FVector2f::Distance(P.C, Q.C) < FMath::Max(P.A, P.B) + FMath::Max(Q.A, Q.B) + 32.0f) return false;
			}
			return true;
		};
		auto TryAdd = [&](const FPillar& P, bool bMarker) -> bool
		{
			const TArray<FPillar> O = Orbit(P);
			// as cópias não podem se encostar entre si nem nos que já estão
			for (int32 I = 0; I < O.Num(); ++I)
			{
				if (!Ground(O[I], bMarker) || !Apart(O[I])) return false;
				for (int32 J = I + 1; J < O.Num(); ++J)
				{
					if (FVector2f::Distance(O[I].C, O[J].C) < 2.0f * FMath::Max(P.A, P.B) + 32.0f && O[I].C.Size() > 1.0f) return false;
				}
			}
			Pillars.Append(O.Num() && O[0].C.Size() <= 1.0f ? TArray<FPillar>{ O[0] } : O);
			return true;
		};
		// o tamanho acompanha o mapa (medido na arte: o pilar tem ~1/12 da distância
		// entre as bases de largura; o rochedo do meio, ~1/7)
		const float Scale = S.MapSize / 1024.0f;
		// o marco no centro (sem a vila do centro): redondo no mapa de 4, oval no de 2.
		// No deserto o oásis é dele (o AlUla: o rochedo com a água e as palmeiras
		// encostadas); os lagos-oásis do miolo secam em leito comum (chão de jogo)
		bool bMarker = false;
		const bool bDesert = S.Biome == EWorldBiome::Desert && BasinCell.Num() == NL * NL;
		if (S.Villages != 1)
		{
			if (bDesert)
			{
				TArray<uint8> Seen;
				Seen.Init(0, NL * NL);
				for (int32 C0 = 0; C0 < NL * NL; ++C0)
				{
					if (Seen[C0] || BasinCell[C0] != 2) continue;
					TArray<int32> Cells, Stack = { C0 };
					Seen[C0] = 1;
					FVector2f Sum = FVector2f::ZeroVector;
					while (Stack.Num())
					{
						const int32 C = Stack.Pop(EAllowShrinking::No);
						Cells.Add(C);
						const int32 A = C % NL, B = C / NL;
						Sum += LPos(A, B);
						const int32 N4[4][2] = { { A + 1, B }, { A - 1, B }, { A, B + 1 }, { A, B - 1 } };
						for (const auto& NB : N4)
						{
							if (NB[0] < 0 || NB[1] < 0 || NB[0] >= NL || NB[1] >= NL) continue;
							const int32 Q = LIdx(NB[0], NB[1]);
							if (!Seen[Q] && BasinCell[Q] == 2) { Seen[Q] = 1; Stack.Add(Q); }
						}
					}
					if ((Sum / float(Cells.Num())).Size() > Dist0 * 0.6f) continue;
					for (int32 C : Cells) { Level[C] = LLow; BasinCell[C] = 0; }
				}
			}
			FPillar P;
			P.C = FVector2f::ZeroVector;
			P.A = 30.0f * Scale;
			P.B = (Sym.K == 4 ? 30.0f : 23.0f) * Scale;
			P.Yaw = Phi0 + HALF_PI;
			P.H = Rng.Range(40.0f, 46.0f);
			P.Seed = Rng.Next();
			if (MarkKit.Num()) Dress(P, MarkKit[0], Scale);
			if (bDesert)
			{
				// o crescente fica na face que olha pra cada base (a que o jogador vê
				// chegando); ~28 m de água a 7 m da parede, a faixa entre os dois é
				// das palmeiras. Mais estreito que ~6 células o lago não fica com
				// água (a altura vem da máscara borrada em 2 células: 16 m saía em poças)
				P.Pond = 28.0f * Scale;
				P.PondPhi = Phi0;
			}
			bMarker = TryAdd(P, true);
			if (bMarker && P.Pond > 0.0f)
			{
				const float Reach = FMath::Max(P.A, P.B) + 6.0f + 1.4f * P.Pond;
				const FIntPoint C0 = LCellOf(P.C - FVector2f(Reach)), C1 = LCellOf(P.C + FVector2f(Reach));
				TArray<int32> Pond, Strip;
				for (int32 B = C0.Y; B <= C1.Y; ++B)
				{
					for (int32 A = C0.X; A <= C1.X; ++A)
					{
						const int32 C = LIdx(A, B);
						if (Level[C] != LLow || FrameMask[C]) continue;
						// a célula conta se a água chega a 1,5 m dentro dela (a margem rasa é chão)
						const float In = PondInside(P, LPos(A, B) - P.C);
						if (In >= 1.5f) Pond.Add(C);
						// a faixa de dentro: 8 m pra fora dela já é água
						else if (In > -7.0f && PondInside(P, (LPos(A, B) - P.C) * (1.0f + 8.0f / FMath::Max(1.0f, (LPos(A, B) - P.C).Size()))) > 1.5f) Strip.Add(C);
					}
				}
				// abertura (erode e dilata 1 célula): a ponta fina do crescente saía em
				// pocinhas soltas, ligadas ao lago por um fio que não vira água
				{
					const TSet<int32> Raw(Pond);
					auto Has = [&](const TSet<int32>& Set, int32 A, int32 B) { return A >= 0 && B >= 0 && A < NL && B < NL && Set.Contains(LIdx(A, B)); };
					TSet<int32> Core;
					for (int32 C : Pond)
					{
						const int32 A = C % NL, B = C / NL;
						if (Has(Raw, A + 1, B) && Has(Raw, A - 1, B) && Has(Raw, A, B + 1) && Has(Raw, A, B - 1)) Core.Add(C);
					}
					Pond.Reset();
					for (int32 C : Raw)
					{
						const int32 A = C % NL, B = C / NL;
						if (Core.Contains(C) || Has(Core, A + 1, B) || Has(Core, A - 1, B) || Has(Core, A, B + 1) || Has(Core, A, B - 1)) Pond.Add(C);
					}
				}
				TSet<int32> InPond(Pond), Seen;
				int32 Wet = 0;
				for (int32 C0i : Pond)
				{
					if (Seen.Contains(C0i)) continue;
					TArray<int32> Comp, Stack = { C0i };
					Seen.Add(C0i);
					while (Stack.Num())
					{
						const int32 C = Stack.Pop(EAllowShrinking::No);
						Comp.Add(C);
						const int32 A = C % NL, B = C / NL;
						for (const int32 Q : { LIdx(FMath::Min(A + 1, NL - 1), B), LIdx(FMath::Max(A - 1, 0), B), LIdx(A, FMath::Min(B + 1, NL - 1)), LIdx(A, FMath::Max(B - 1, 0)) })
						{
							if (InPond.Contains(Q) && !Seen.Contains(Q)) { Seen.Add(Q); Stack.Add(Q); }
						}
					}
					if (Comp.Num() < 12) continue;   // < 190 m²: a ponta solta fica seca
					for (int32 C : Comp) { PillarMask[C] = 1; BasinCell[C] = 2; ++Wet; }
				}
				if (Wet) for (int32 C : Strip) PillarMask[C] = 1;
				Sym.Enforce(PillarMask, NL);
				Sym.Enforce(BasinCell, NL);
				UE_LOG(LogTemp, Display, TEXT("WorldGen oásis do marco: %.0f m²"), Wet * LogicCell * LogicCell);
			}
		}
		// o anel: por jogador, 4 vagas dos dois lados da rota pro centro (a ±35° a
		// ~42% do caminho e a ±75° a ~55%), com folga; a rota direta passa no vão
		// largo entre as duas da frente (~210 m no mapa de 1 km)
		struct FSlot { float Off, R; };
		const TArray<FSlot> Slots = Sym.K == 4
			? TArray<FSlot>{ { 30.0f, 0.42f }, { -30.0f, 0.42f } }
			: TArray<FSlot>{ { 35.0f, 0.42f }, { -35.0f, 0.42f }, { 75.0f, 0.56f }, { -75.0f, 0.56f } };
		int32 Got = 0;
		for (const FSlot& Sl : Slots)
		{
			for (int32 Try = 0; Try < 40; ++Try)
			{
				const float Off = FMath::DegreesToRadians(Sl.Off + Rng.Range(-9.0f, 9.0f));
				const float R = Dist0 * (Sl.R + Rng.Range(-0.05f, 0.05f));
				FPillar P;
				P.C = FVector2f(FMath::Cos(Phi0 + Off), FMath::Sin(Phi0 + Off)) * R;
				P.A = Rng.Range(14.0f, 22.0f) * Scale;
				P.B = P.A * Rng.Range(0.6f, 0.85f);
				P.Yaw = Rng.Range(0.0f, 2.0f * PI);
				P.H = Rng.Range(22.0f, 32.0f);
				P.Seed = Rng.Next();
				// a peça do kit: nunca a mesma nos dois vizinhos do anel (a repetição de cima, 2ª opinião)
				// (na arte o pilar do anel é quase da altura do rochedo do centro: 1,3–1,7× o kit, 13/10)
				if (RingKit.Num()) Dress(P, RingKit[(Got * 2 + int32(Rng.Next() % 2u)) % RingKit.Num()], Rng.Range(1.3f, 1.7f) * Scale);
				if (TryAdd(P, false)) { ++Got; break; }
			}
		}
		// a pegada na grade lógica (+4 m: a saia de tálus também não é passagem)
		for (const FPillar& P : Pillars)
		{
			const float R = FMath::Max(P.A, P.B) + 10.0f;
			const FIntPoint C0 = LCellOf(P.C - FVector2f(R)), C1 = LCellOf(P.C + FVector2f(R));
			for (int32 B = C0.Y; B <= C1.Y; ++B)
			{
				for (int32 A = C0.X; A <= C1.X; ++A)
				{
					const FVector2f L = LPos(A, B) - P.C;
					// (o pilar do anel com 10 m de folga: a estrada é a trilha suavizada em ~30 m e cortava a curva
					// por cima do pé da pedra, 13/10; o marco fica com 4 m, o anel de estrada passa por fora)
					const float Margin = P.Pond > 0.0f ? 4.0f : 10.0f;
					if (L.Size() <= FootR(P, FMath::Atan2(L.Y, L.X)) + Margin) PillarMask[LIdx(A, B)] = 1;
				}
			}
		}
		Sym.Enforce(PillarMask, NL);
		UE_LOG(LogTemp, Display, TEXT("WorldGen monólitos: %d pilares (anel %d por jogador%s)"), Pillars.Num(), Got, bMarker ? TEXT(" + marco no centro") : TEXT(", sem marco no centro"));
	}

	void FGen::MakeBasins()
	{
		// deserto: lago é bacia sem saída. A mais central fica com água (o oásis,
		// o verde e o azul contra o vermelho da direção de arte); as outras secam
		// em salina: chão plano e aberto, crosta clara (Salar de Uyuni, os chotts
		// do Saara). Fora do deserto os lagos ficam.
		BasinCell.SetNumZeroed(NL * NL);
		// a vila do centro (13/10, o Felipe: "se for necessário ajustar os lagos pra vila caber, pode ser"):
		// o lago recua do cruzamento. Uma clareira de ~66 m de raio com a borda ondulada (só harmônicas
		// pares do ângulo: a simetria do mapa fica, 2 ou 4 lados); a água de dentro vira chão baixo. Sem
		// ela os dois lagos do miolo comiam metade dos lotes e dois cantos do cruzamento
		if (S.Villages == 1)
		{
			const int32 Sides = S.Players == 4 ? 4 : 2;
			WorldNoise::FRng Cr(uint64(S.Seed) * 31u + 0x7a11u);
			const float Ph1 = Cr.Range(0.0f, 2.0f * PI), Ph2 = Cr.Range(0.0f, 2.0f * PI);
			int32 Dried = 0;
			for (int32 C = 0; C < NL * NL; ++C)
			{
				if (Level[C] != LWater || FrameMask[C]) continue;
				const FVector2f P = LPos(C % NL, C / NL);
				const float Ang = FMath::Atan2(P.Y, P.X);
				// (80 m: o chão desce até a água em ~30 m, e o lote precisa de 1 m acima dela; com 66 m os
				// lotes do cruzamento ainda caíam "na água")
				const float R = (80.0f + 6.0f * FMath::Sin(Sides * Ang + Ph1) + 3.0f * FMath::Sin(3 * Sides * Ang + Ph2)) * S.MapSize / 1024.0f;
				if (P.Size() < R) { Level[C] = LLow; ++Dried; }
			}
			if (Dried) UE_LOG(LogTemp, Display, TEXT("WorldGen vila do centro: %d células de lago viraram chão (clareira de ~80 m)"), Dried);
		}
		if (S.Biome != EWorldBiome::Desert) return;
		TArray<int32> Comp;
		Comp.Init(-1, NL * NL);
		TArray<TArray<int32>> Lakes;
		TArray<float> Dist;
		for (int32 Seed = 0; Seed < NL * NL; ++Seed)
		{
			if (Comp[Seed] >= 0 || Level[Seed] != LWater || FrameMask[Seed]) continue;
			TArray<int32>& Cells = Lakes.AddDefaulted_GetRef();
			TArray<int32> Stack = { Seed };
			Comp[Seed] = Lakes.Num() - 1;
			FVector2f Sum = FVector2f::ZeroVector;
			while (Stack.Num())
			{
				const int32 C = Stack.Pop(EAllowShrinking::No);
				Cells.Add(C);
				Sum += LPos(C % NL, C / NL);
				const int32 A = C % NL, B = C / NL;
				const int32 N4[4][2] = { { A + 1, B }, { A - 1, B }, { A, B + 1 }, { A, B - 1 } };
				for (const auto& NB : N4)
				{
					if (NB[0] < 0 || NB[1] < 0 || NB[0] >= NL || NB[1] >= NL) continue;
					const int32 Q = LIdx(NB[0], NB[1]);
					if (Comp[Q] < 0 && Level[Q] == LWater && !FrameMask[Q]) { Comp[Q] = Lakes.Num() - 1; Stack.Add(Q); }
				}
			}
			Dist.Add((Sum / float(Cells.Num())).Size());
		}
		if (Lakes.Num() == 0) return;
		// o oásis: o lago grande (≥ 2.400 m²; ou o par simétrico) mais perto do
		// centro. Oásis de lago pequeno ficava um anel ralo de 3–6 árvores, sem
		// leitura de marco (revisão de arte de 07/10)
		float Best = TNumericLimits<float>::Max();
		for (int32 K = 0; K < Lakes.Num(); ++K) if (Lakes[K].Num() >= 150) Best = FMath::Min(Best, Dist[K]);
		int32 Oases = 0, Salts = 0;
		for (int32 K = 0; K < Lakes.Num(); ++K)
		{
			// o controle "Água" no deserto é o quanto ele é úmido: acima de ~55%,
			// mais lagos grandes ficam com água (oásis), do centro pra fora; com 90%
			// quase todos. Sem isso, mais água = lagos maiores = mais salina = menos
			// água no mapa, o contrário do que o controle diz (teste da interface, 07/10)
			const float Reach = Best + 8.0f + 1500.0f * FMath::Max(0.0f, S.Water - 0.55f);
			const bool bOasis = Lakes[K].Num() >= 150 && Dist[K] < Reach;
			const bool bSaltFlat = !bOasis && Lakes[K].Num() >= 150;   // ≥ 2.400 m²
			FVector2f Sum = FVector2f::ZeroVector;
			for (int32 C : Lakes[K])
			{
				// lago pequeno que secou: 3 = leito comum (sem crosta; a drenagem que
				// junta ali não vira o disco de cascalho do leito seco)
				BasinCell[C] = bOasis ? 2 : (bSaltFlat ? 1 : 3);
				if (!bOasis) Level[C] = LLow;   // seca: vira chão de jogo
				Sum += LPos(C % NL, C / NL);
			}
			if (bSaltFlat || bOasis)
			{
				const FVector2f Ctr = Sum / float(Lakes[K].Num());
				UE_LOG(LogTemp, Display, TEXT("WorldGen %s em (%.0f, %.0f), %.0f m²"), bOasis ? TEXT("oásis") : TEXT("salina"), Ctr.X, Ctr.Y, Lakes[K].Num() * LogicCell * LogicCell);
			}
			(bOasis ? Oases : Salts) += 1;
		}
		// a salina é o leito exposto, maior que o lago de antes: cresce 2 células
		// (8 m) sobre a baixada plana em volta
		for (int32 Grow = 0; Grow < 2; ++Grow)
		{
			TArray<uint8> Next = BasinCell;
			for (int32 B = 1; B < NL - 1; ++B)
			{
				for (int32 A = 1; A < NL - 1; ++A)
				{
					const int32 C = LIdx(A, B);
					if (BasinCell[C] || Level[C] != LLow || FrameMask[C]) continue;
					if (BasinCell[LIdx(A + 1, B)] == 1 || BasinCell[LIdx(A - 1, B)] == 1 || BasinCell[LIdx(A, B + 1)] == 1 || BasinCell[LIdx(A, B - 1)] == 1)
					{
						Next[C] = 1;
					}
				}
			}
			BasinCell = MoveTemp(Next);
		}
		Sym.Enforce(BasinCell, NL);
		Sym.Enforce(Level, NL);
		UE_LOG(LogTemp, Verbose, TEXT("WorldGen bacias: %d oásis, %d salinas"), Oases, Salts);
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
				if (Level[Q] == LMountain || FrameMask[Q] || (PillarMask.Num() && PillarMask[Q])) continue;
				const int32 DL = FMath::Abs(Eff(C) - Eff(Q));
				if (DL > 1) continue;
				float Cost = 1.0f;
				if (Level[Q] == LWater) Cost += FordCost;
				if (DL == 1 && !RampMask[C] && !RampMask[Q] && !(DipMask[C] && DipMask[Q])) Cost += RampCost;
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
			if (LA != LB && !RampMask[A] && !RampMask[B] && !(DipMask[A] && DipMask[B]))
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
		// o anel do marco (Monólitos): a estrada contorna o rochedo e o oásis de ponta a ponta (na arte do
		// AlUla ela dá a volta no oásis; sem o anel as duas estradas morriam na ponta do rochedo, 13/10)
		for (const FPillar& Mk : Pillars)
		{
			if (Mk.Pond <= 0.0f || Mk.C.Size() > 1.0f) continue;
			const float Rr = FMath::Max(Mk.A, Mk.B) + 7.0f + Mk.Pond + 14.0f;
			TArray<float> Tips;
			for (int32 T = 0; T < M.Tracks.Num(); ++T)
			{
				TArray<FVector2f>& L = M.Tracks[T];
				if (M.TrackKinds[T] != EWorldTrack::Main || L.Num() < 2) continue;
				if (L[0].Size() < L.Last().Size()) Algo::Reverse(L);   // a ponta de dentro no fim
				if (L.Last().Size() >= Rr) continue;
				// corta a ponta de dentro do anel e termina em cima dele
				while (L.Num() > 1 && L.Last().Size() < Rr) L.Pop();
				const float A = FMath::Atan2(L.Last().Y, L.Last().X);
				L.Add(FVector2f(FMath::Cos(A), FMath::Sin(A)) * Rr);
				Tips.Add(A);
			}
			Tips.Sort();
			for (int32 I = 0; I < Tips.Num() && Tips.Num() >= 2; ++I)
			{
				const float A0 = Tips[I], A1 = I + 1 < Tips.Num() ? Tips[I + 1] : Tips[0] + 2.0f * PI;
				TArray<FVector2f>& Line = M.Tracks.AddDefaulted_GetRef();
				M.TrackKinds.Add(EWorldTrack::Main);
				const int32 Steps = FMath::Max(8, int32((A1 - A0) * Rr / 4.0f));
				for (int32 K = 0; K <= Steps; ++K)
				{
					const float A = FMath::Lerp(A0, A1, float(K) / Steps);
					const FVector2f P = FVector2f(FMath::Cos(A), FMath::Sin(A)) * Rr;
					Line.Add(P);
					const FIntPoint C = LCellOf(P);
					PathMask[LIdx(C.X, C.Y)] = 1;
				}
			}
			UE_LOG(LogTemp, Display, TEXT("WorldGen anel do marco: raio %.0f m, %d pontas"), Rr, Tips.Num());
			break;
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
		// os depósitos do jogo (13/10, o Felipe: "o supply verde do WorldForge é totalmente incorreto"): as folgas
		// com raio < 60 m são os supply_docks do Maps.json (o SupplyDock de 26,33 × 32,12 m, sem giro). Quando o
		// mapa tem os depósitos, os campos são eles: a terra batida, as trilhas, as mini vilas e o app mostram o que
		// o jogo usa. O dono é a largada a até 160 m (senão disputado)
		for (const FVector3f& K : S.KeepOut)
		{
			if (K.Z >= 60.0f) continue;
			FWorldResource Res;
			Res.Pos = FVector2f(K.X, K.Y);
			Res.Radius = 21.0f;   // a meia diagonal do depósito
			Res.Amount = 1.0f;
			float Best = 160.0f;
			for (int32 P = 0; P < StartPos.Num(); ++P)
			{
				const float D = FVector2f::Distance(Res.Pos, StartPos[P]);
				if (D < Best) { Best = D; Res.Owner = P; }
			}
			M.Resources.Add(Res);
		}
		if (M.Resources.Num() || S.Resources <= 0.01f)
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
					if (Level[Q] != L || RampMask[Q] || (PillarMask.Num() && PillarMask[Q])) return false;
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
			// a vila do centro é dela (13/10: com o lago recuado o campo caía no chão que secou e matava lotes)
			if (S.Villages == 1 && P.Size() < 105.0f * S.MapSize / 1024.0f) return false;
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
	/** Os morros e descidas (16/10, o Felipe: "morros e descidas, olha lá o C&C"; os 7 mapas do ranqueado do ZH medidos:
	 *  50–75% plano, o resto formas grandes com intenção). Sorteadas inteiras com a órbita da simetria, no chão aberto da
	 *  baixada: longe do que não é baixada (platô, alto, montanha, água, bacia, moldura, rampa, pilar: 35 m além do eixo
	 *  maior, as bordas de platô e as rampas ficam como estão), das trilhas (a borda da forma a 25 m ou mais; entre as
	 *  boas, a que deixa a borda a ~55 m da trilha: o corredor), das bases, dos campos de recurso e das áreas guardadas.
	 *  6–8 grandes por km² (morro redondo, crista, mesa, vale entre ombros) e ~10 menores, escalados por S.Hills, com
	 *  espaço entre elas. Alturas pela sombra: o sol baixo do jogo estica a sombra de um morro de 15 m por ~100 m e cobre
	 *  unidades; os flancos ficam de 8° a 22°. */
	void FGen::PlaceForms()
	{
		FormList.Reset();
		if (S.Hills <= 0.01f || NL < 4) return;
		auto Chamfer = [&](TFunctionRef<bool(int32)> Src)
		{
			TArray<float> D;
			D.Init(1e9f, NL * NL);
			for (int32 C = 0; C < NL * NL; ++C) if (Src(C)) D[C] = 0.0f;
			const float A = LogicCell, Bd = LogicCell * 1.41421f;
			for (int32 B = 0; B < NL; ++B)
			{
				for (int32 Aa = 0; Aa < NL; ++Aa)
				{
					float& V = D[LIdx(Aa, B)];
					if (Aa > 0) V = FMath::Min(V, D[LIdx(Aa - 1, B)] + A);
					if (B > 0) V = FMath::Min(V, D[LIdx(Aa, B - 1)] + A);
					if (Aa > 0 && B > 0) V = FMath::Min(V, D[LIdx(Aa - 1, B - 1)] + Bd);
					if (Aa + 1 < NL && B > 0) V = FMath::Min(V, D[LIdx(Aa + 1, B - 1)] + Bd);
				}
			}
			for (int32 B = NL - 1; B >= 0; --B)
			{
				for (int32 Aa = NL - 1; Aa >= 0; --Aa)
				{
					float& V = D[LIdx(Aa, B)];
					if (Aa + 1 < NL) V = FMath::Min(V, D[LIdx(Aa + 1, B)] + A);
					if (B + 1 < NL) V = FMath::Min(V, D[LIdx(Aa, B + 1)] + A);
					if (Aa + 1 < NL && B + 1 < NL) V = FMath::Min(V, D[LIdx(Aa + 1, B + 1)] + Bd);
					if (Aa > 0 && B + 1 < NL) V = FMath::Min(V, D[LIdx(Aa - 1, B + 1)] + Bd);
				}
			}
			return D;
		};
		// (16/10, a 1ª medida: só na baixada cabia 1 grande por jogador; o chão aberto do mapa é muito platô, e no ZH tem
		// morro em qualquer nível) a distância à borda de nível (célula com vizinho de outro nível) e ao que não é chão aberto
		auto Blocked = [&](int32 C)
		{
			return Level[C] < LLow || Level[C] > LHigh || (BasinCell.Num() && BasinCell[C]) || (FrameKind.Num() && FrameKind[C] != 0)
				|| (RampMask.Num() && RampMask[C]) || (PillarMask.Num() && PillarMask[C]);
		};
		const TArray<float> DB = Chamfer([&](int32 C)
		{
			if (Blocked(C)) return true;
			const int32 Aa = C % NL, B = C / NL;
			for (const FIntPoint O : { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) })
			{
				const int32 X = Aa + O.X, Y = B + O.Y;
				if (X < 0 || Y < 0 || X >= NL || Y >= NL) return true;
				if (Level[LIdx(X, Y)] != Level[C]) return true;
			}
			return false;
		});
		const TArray<float> DP = Chamfer([&](int32 C) { return PathMask.Num() && PathMask[C] != 0; });
		FormDB = DB;
		FormDP = DP;
		auto At = [&](const TArray<float>& D, FVector2f P) { const FIntPoint Cc = LCellOf(P); return D[LIdx(Cc.X, Cc.Y)]; };
		const float Area = FMath::Square(2.0f * Half) / (1024.0f * 1024.0f);
		// (16/10, a 4ª medida: 8 morros soltos no plano liam como as dunas que ele reprovou, "do nada essas montanhas
		// espalhadas"; o chão inteiro agora ondula, BuildHeights) poucas formas grandes, as marcantes
		const int32 NBig = FMath::Max(1, FMath::RoundToInt(5.0f * S.Hills * Area / Sym.K));
		const int32 NSmall = 0;
		const float Hk = 0.6f + 0.6f * S.Hills;
		FRng Rng(SubSeed(61));
		auto Make = [&](bool bBig, FVector2f C)
		{
			FForm F;
			F.C = C;
			F.Seed = Rng.Next();
			F.Yaw = Rng.Range(0.0f, 2.0f * PI);
			F.Steep = Rng.Float() < 0.5f ? 1.0f : -1.0f;
			if (!bBig)
			{
				// (16/10: as pequenas redondas eram bolinhas) a crista baixa e comprida, o "espinhaço" entre as grandes
				F.Type = 1;
				F.A = Rng.Range(35.0f, 70.0f);
				F.B = FMath::Max(14.0f, F.A / Rng.Range(1.8f, 2.8f));
				F.H = Rng.Range(1.2f, 2.4f) * Hk;
				return F;
			}
			const float T = Rng.Float();
			F.Type = T < 0.45f ? 0 : (T < 0.75f ? 1 : (T < 0.87f ? 2 : 3));
			switch (F.Type)
			{
			// (16/10, a 3ª medida: com 120–300 m quase nada cabia nos campos abertos deste mapa, de 200–350 m entre as bordas
			// de platô; 90–240 m, ainda na faixa dos morros do ZH)
			case 0: F.A = Rng.Range(45.0f, 95.0f); F.B = F.A / Rng.Range(1.0f, 1.4f); F.H = Rng.Range(3.0f, 5.5f) * Hk; break;   // morro
			case 1: F.A = Rng.Range(60.0f, 120.0f); F.B = F.A / Rng.Range(2.0f, 3.0f); F.H = Rng.Range(2.5f, 4.5f) * Hk; break;   // crista
			case 2: F.A = Rng.Range(35.0f, 65.0f); F.B = F.A / Rng.Range(1.0f, 1.3f); F.H = Rng.Range(3.5f, 5.5f) * Hk; break;   // mesa
			default: F.A = Rng.Range(60.0f, 110.0f); F.B = Rng.Range(30.0f, 45.0f); F.H = Rng.Range(2.5f, 4.5f) * Hk; break;   // vale entre ombros
			}
			return F;
		};
		// a forma cabe? (todas as cópias da órbita) e a nota dela (a borda a ~55 m da trilha: o corredor)
		// o contorno (a elipse a 1,2 com a margem do entortado), em 16 pontos e o centro, no referencial da forma
		auto Outline = [&](const FForm& F, TArray<FVector2f>& Out)
		{
			Out.Reset();
			const float Cs = FMath::Cos(F.Yaw), Sn = FMath::Sin(F.Yaw);
			Out.Add(F.C);
			for (int32 K = 0; K < 16; ++K)
			{
				const float A = 2.0f * PI * K / 16.0f;
				const float U = 1.2f * F.A * FMath::Cos(A), V = 1.2f * F.B * FMath::Sin(A);
				Out.Add(F.C + FVector2f(U * Cs - V * Sn, U * Sn + V * Cs));
			}
		};
		TArray<FVector2f> Pts;
		int32 Why[8] = { 0 };   // o motivo de cada recusa (o log): nível, borda do mapa, outro nível, borda de platô, trilha, base, recurso/guardado, outra forma
		auto Score = [&](const FForm& F, bool bBig) -> float
		{
			const uint8 Lv = Level[LIdx(LCellOf(F.C).X, LCellOf(F.C).Y)];
			if (Lv < LLow || Lv > LHigh) { ++Why[0]; return -1.0f; }
			float MinPath = 1e9f;
			for (int32 R = 0; R < Sym.K; ++R)
			{
				FForm Fr = F;
				Fr.C = Sym.Rot(F.C, R);
				Fr.Yaw = F.Yaw + R * Sym.Quarter() * HALF_PI;
				Outline(Fr, Pts);
				for (int32 Pi = 0; Pi < Pts.Num(); ++Pi)
				{
					const FVector2f Q = Pts[Pi];
					if (FMath::Abs(Q.X) > Half - 30.0f || FMath::Abs(Q.Y) > Half - 30.0f) { ++Why[1]; return -1.0f; }
					const FIntPoint Qc = LCellOf(Q);
					if (Level[LIdx(Qc.X, Qc.Y)] != Lv) { ++Why[2]; return -1.0f; }
					if (At(DB, Q) < 20.0f) { ++Why[3]; return -1.0f; }            // a borda do platô e a rampa ficam como estão
					// (16/10, a 2ª medida: o contorno inteiro longe das trilhas não cabia no mapa) a trilha pode passar no
					// flanco (a estrada faz corte e aterro, como no ZH a estrada sobe o morro baixo); o miolo fica livre
					const FVector2f Mid = Pi == 0 ? Q : Fr.C + 0.5f * (Q - Fr.C);
					const float Dp = At(DP, Mid);
					if (Dp < 10.0f) { ++Why[4]; return -1.0f; }
					MinPath = FMath::Min(MinPath, At(DP, Q));
					for (const FVector2f& St : StartPos) if (FVector2f::Distance(Q, St) < BaseRadius + 45.0f) { ++Why[5]; return -1.0f; }
					for (const FWorldResource& Rs : M.Resources) if (FVector2f::Distance(Q, Rs.Pos) < Rs.Radius + 30.0f) { ++Why[6]; return -1.0f; }
					for (const FVector3f& K : S.KeepOut) if (FVector2f::Distance(Q, FVector2f(K.X, K.Y)) < K.Z + 20.0f) { ++Why[6]; return -1.0f; }
				}
				for (const FForm& G : FormList)
				{
					const float Need = bBig && G.A > 45.0f ? FMath::Max(150.0f, F.A + G.A + 40.0f) : 0.8f * (F.A + G.A) + 25.0f;
					if (FVector2f::Distance(Fr.C, G.C) < Need) { ++Why[7]; return -1.0f; }
				}
				for (int32 R2 = R + 1; R2 < Sym.K; ++R2) if (FVector2f::Distance(Fr.C, Sym.Rot(F.C, R2)) < 2.0f * F.A + 40.0f) { ++Why[7]; return -1.0f; }
			}
			// entre as que cabem, a que deixa a borda perto da trilha sem tapar (o corredor)
			return 1.0f / (1.0f + FMath::Abs(MinPath - 20.0f) / 25.0f);
		};
		int32 Placed[2] = { 0, 0 };
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			const bool bBig = Pass == 0;
			const int32 Want = bBig ? NBig : NSmall;
			for (int32 K = 0; K < Want; ++K)
			{
				FForm Best;
				float BestS = -1.0f;
				int32 Found = 0;
				for (int32 Try = 0; Try < 2000 && Found < 12; ++Try)
				{
					const FVector2f C(Rng.Range(-Half, Half), Rng.Range(-Half, Half));
					const FForm F = Make(bBig, C);
					const float Sc = Score(F, bBig);
					if (Sc < 0.0f) continue;
					++Found;
					if (Sc > BestS) { BestS = Sc; Best = F; }
				}
				if (BestS < 0.0f) break;
				for (int32 R = 0; R < Sym.K; ++R)
				{
					FForm F = Best;
					F.C = Sym.Rot(Best.C, R);
					F.Yaw = Best.Yaw + R * Sym.Quarter() * HALF_PI;
					FormList.Add(F);
				}
				++Placed[Pass];
			}
		}
		UE_LOG(LogTemp, Display, TEXT("FORGE morros e descidas: %d grandes e %d pequenas por jogador (pedidas %d e %d), %d formas no mapa; recusas: nível %d, borda %d, outro nível %d, borda de platô %d, trilha %d, base %d, recurso %d, outra forma %d"),
			Placed[0], Placed[1], NBig, NSmall, FormList.Num(), Why[0], Why[1], Why[2], Why[3], Why[4], Why[5], Why[6], Why[7]);
	}

	void FGen::BuildHeights()
	{
		const int32 N = M.N;
		// indicadores dos níveis, desfocados: a borda de cada nível vira um
		// contorno suave (penhasco curvo, não serrilhado na grade de 4 m)
		auto IndicatorC = [&](TFunctionRef<bool(int32)> Test, int32 Radius, int32 Passes)
		{
			TArray<float> F;
			F.SetNumUninitialized(NL * NL);
			for (int32 C = 0; C < NL * NL; ++C) F[C] = Test(C) ? 1.0f : 0.0f;
			Blur(F, NL, Radius, Passes);
			Sym.Enforce(F, NL);
			return F;
		};
		auto Indicator = [&](TFunctionRef<bool(uint8)> Test, int32 Radius, int32 Passes)
		{
			return IndicatorC([&](int32 C) { return Test(Level[C]); }, Radius, Passes);
		};
		const TArray<float> B1 = Indicator([](uint8 L) { return L >= LLow; }, 2, 2);
		const TArray<float> B1W = Indicator([](uint8 L) { return L >= LLow; }, 4, 3);
		ShoreField = Indicator([](uint8 L) { return L >= LLow; }, 2, 2);
		const TArray<float> B2 = Indicator([](uint8 L) { return L >= LPlateau; }, 1, 2);
		const TArray<float> B3 = Indicator([](uint8 L) { return L >= LHigh; }, 1, 2);
		// montanha: a do miolo, a serra da moldura e a falésia da costa; a moldura
		// de colinas fica de fora (ela tem relevo próprio, abaixo)
		auto IsMountain = [&](int32 C) { return Level[C] == LMountain && FrameKind[C] != 2; };
		const TArray<float> B4 = IndicatorC(IsMountain, 1, 2);
		const TArray<float> B4W = IndicatorC(IsMountain, 5, 3);
		// mar da moldura: o fundo desce até ~45 m na borda do mapa (plataforma e
		// talude). Raso, o fundo acabava numa linha reta onde o terreno termina
		const TArray<float> BSea = IndicatorC([&](int32 C) { return FrameKind[C] == 3 && Level[C] == LWater; }, 3, 3);
		const float SeaWidth = 1.6f * 0.06f * S.MapSize;
		const TArray<float> BSalt = IndicatorC([&](int32 C) { return BasinCell[C] == 1; }, 2, 2);
		SaltField = BSalt;
		OasisField = IndicatorC([&](int32 C) { return BasinCell[C] == 2; }, 4, 3);
		const TArray<float> BO = IndicatorC([&](int32 C) { return FrameKind[C] == 2; }, 3, 3);
		const TArray<float> BOW = IndicatorC([&](int32 C) { return FrameKind[C] == 2; }, 6, 3);
		OpenField.SetNumZeroed(N * N);
		TalusField.SetNumZeroed(N * N);
		const TArray<float> BDip = IndicatorC([&](int32 C) { return DipMask.Num() && DipMask[C] != 0; }, 3, 2);
		const TArray<float> B2W = Indicator([](uint8 L) { return L >= LPlateau && L != LMountain; }, 6, 3);
		const TArray<float> B3W = Indicator([](uint8 L) { return L == LHigh; }, 6, 3);
		TrackField.SetNumUninitialized(NL * NL);
		for (int32 C = 0; C < NL * NL; ++C) TrackField[C] = PathMask[C] ? 1.0f : 0.0f;
		Blur(TrackField, NL, 1, 1);
		Sym.Enforce(TrackField, NL);

		const uint32 SeedC = SubSeed(7), SeedR = SubSeed(8), SeedU = SubSeed(9);
		const float MountainAmp = 14.0f + 40.0f * S.Relief;
		const float BaseH = LevelHeight(BaseLevel);
		M.Height.SetNumZeroed(N * N);
		MountainField.SetNumZeroed(N * N);
		FormField.Reset();
		if (FormList.Num()) FormField.SetNumZeroed(N * N);
		auto Contour = [&](FVector2f Q, int32) { return Fbm(Q.X / 26.0f, Q.Y / 26.0f, SeedC, 3); };
		auto Ridge = [&](FVector2f Q, int32) { return Ridged(Q.X / 150.0f, Q.Y / 150.0f, SeedR, 5); };
		auto Undulate = [&](FVector2f Q, int32) { return Fbm(Q.X / 55.0f, Q.Y / 55.0f, SeedU, 3) + 1.6f * Fbm(Q.X / 190.0f, Q.Y / 190.0f, SeedU + 5u, 2); };
		const uint32 SeedH = SubSeed(62);
		auto Rolling = [&](FVector2f Q, int32)
		{
			const FVector2f Wq = Q + 60.0f * FVector2f(Fbm(Q.X / 400.0f, Q.Y / 400.0f, SeedH, 2), Fbm(Q.X / 400.0f + 5.2f, Q.Y / 400.0f - 1.3f, SeedH + 1u, 2));
			const float Ro = Fbm(Wq.X / 150.0f, Wq.Y / 150.0f, SeedH + 2u, 3);
			const float Rg = 1.0f - FMath::Abs(Fbm(Wq.X / 260.0f, Wq.Y / 260.0f, SeedH + 3u, 2));
			// (16/10, a 1ª no jogo: as ondas de 150–260 m (desvio de 1,5 m, rampa de ~1° a 30 m) passavam do quadro de 67 m e
			// o chão lia plano) a escala média, ~45 m e ~0,6 m (rampa de ~5°): é a que o sol baixo desenha na câmera do jogo
			const float Md = Fbm(Wq.X / 45.0f, Wq.Y / 45.0f, SeedH + 4u, 3);
			return Ro + 0.5f * (Rg * Rg - 0.45f) + 0.8f * Md;
		};
		// as dunas da moldura do deserto (13/10, a arte do AlUla: dunas grandes em volta do mapa): dunas transversais
		// ao vento, com a encosta de barlavento a ~12° e a face de deslizamento no ângulo de repouso da areia seca,
		// ~32° (Bagnold 1941; Pye & Tsoar, "Aeolian Sand and Sand Dunes"): o comprimento de onda é a altura ×
		// (1/tg 12° + 1/tg 32°) ≈ 6,3 alturas. O vento é o do mapa (a mesma conta do M.WindYaw lá no fim), a crista
		// serpenteia pelo ruído e a altura varia ao longo dela
		FRng DuneWindRng(SubSeed(41));
		const float DuneYaw = FMath::DegreesToRadians(87.0f - 155.0f + DuneWindRng.Range(-20.0f, 20.0f));
		const FVector2f DuneDir(FMath::Cos(DuneYaw), FMath::Sin(DuneYaw));
		constexpr float DuneMaxH = 14.0f;
		constexpr float DuneLambda = 6.3f * DuneMaxH;
		constexpr float DuneWindward = 4.70f / 6.30f;   // a fração do barlavento no comprimento de onda
		auto DuneProfile = [&](FVector2f Q) -> float
		{
			const float Al = FVector2f::DotProduct(Q, DuneDir);
			const float Ac = DuneDir.X * Q.Y - DuneDir.Y * Q.X;
			const float Sw = Al + 22.0f * Fbm(Ac / 160.0f, Al / 420.0f, SeedU + 17u, 2) + 6.0f * Fbm(Ac / 45.0f, Al / 120.0f, SeedU + 19u, 2);
			const float Ph = FMath::Frac(Sw / DuneLambda);
			// barlavento convexo até a crista; sotavento reto (a face de deslizamento), o pé arredondado
			const float Up = Ph < DuneWindward ? 1.0f - FMath::Square(1.0f - Ph / DuneWindward) : 1.0f - (Ph - DuneWindward) / (1.0f - DuneWindward);
			const float Foot = FMath::SmoothStep(0.0f, 0.18f, Up);
			const float Along = 0.6f + 0.4f * FMath::Clamp(0.5f + Fbm(Ac / 230.0f, Al / 600.0f, SeedU + 23u, 2), 0.0f, 1.0f);
			return DuneMaxH * Along * FMath::Lerp(Up, Up * Foot, 0.5f);
		};

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
				float S2 = FMath::SmoothStep(0.33f, 0.67f, SampleLogic(B2, NL, U, W) + C);
				float S3 = FMath::SmoothStep(0.33f, 0.67f, SampleLogic(B3, NL, U, W) + C);
				// cuesta: do lado da encosta o degrau desce em ~30 m (≈ 13°, passável)
				const float Dip = FMath::SmoothStep(0.15f, 0.6f, SampleLogic(BDip, NL, U, W));
				if (Dip > 0.0f)
				{
					S2 = FMath::Max(S2, Dip * FMath::SmoothStep(0.03f, 0.5f, SampleLogic(B2W, NL, U, W)));
					S3 = FMath::Max(S3, Dip * FMath::SmoothStep(0.03f, 0.5f, SampleLogic(B3W, NL, U, W)));
				}
				const float S4 = FMath::SmoothStep(0.35f, 0.65f, SampleLogic(B4, NL, U, W) + C);
				const float Floor = WaterSurface - (1.2f + 4.5f * (1.0f - SampleLogic(B1W, NL, U, W)));
				float H = Floor + (LevelHeight(LLow) - Floor) * S1 + LevelStep * S2 + LevelStep * S3;
				// montanha: cresce pra dentro da região, cristas "ridged"
				const float Core = FMath::SmoothStep(0.3f, 1.0f, SampleLogic(B4W, NL, U, W));
				const float R = Sym.Field(P, Ridge, false);
				// maciço em estratos (camadas de ~5 m com degrau íngreme e patamar),
				// não bolha: a mesma leitura das escarpas de arenito
				{
					const float Hm = 5.0f + MountainAmp * Core * (0.55f + 0.45f * R);
					const float Layer = 5.0f, K = Hm / Layer;
					const float Strata = (FMath::FloorToFloat(K) + FMath::SmoothStep(0.65f, 1.0f, FMath::Frac(K))) * Layer;
					H += S4 * FMath::Lerp(Hm, Strata, 0.75f);
				}
				// saia de tálus: blocos e cascalho caídos no pé, ~20 m, perfil côncavo
				{
					const float Talus = (1.0f - S4) * FMath::Square(FMath::SmoothStep(0.02f, 0.45f, SampleLogic(B4W, NL, U, W))) * S1;
					H += 3.2f * Talus;
					TalusField[V] = Talus;
				}
				// moldura de colinas: o chão segue além da borda, subindo devagar
				// (dunas de crista no deserto, colinas no resto); não é muro
				const float So = FMath::SmoothStep(0.25f, 0.75f, SampleLogic(BO, NL, U, W));
				if (So > 0.0f)
				{
					const float Deep = FMath::SmoothStep(0.2f, 1.0f, SampleLogic(BOW, NL, U, W));
					if (S.Biome == EWorldBiome::Desert)
					{
						// (a crista de antes, 6 m de ruído, lia como areia remexida; a arte tem duna de verdade)
						H += So * (1.5f + Deep * (2.5f + DuneProfile(P)));
					}
					else
					{
						H += So * (1.5f + Deep * (4.0f + 3.0f + 5.0f * Sym.Field(P, Undulate, true)));
					}
					OpenField[V] = So;
				}
				// ondulação leve fora das montanhas (platôs continuam legíveis); a
				// baixada nunca afunda até a água (água só onde o mapa decidiu)
				H += (0.4f + 0.4f * S.Relief) * Sym.Field(P, Undulate, true) * (1.0f - S4) * S1;
				// (16/10, o Felipe: "é questão de pincelar e naturalizar, morros e descidas [...] olha lá o C&C, é cheio disso")
				// o chão aberto inteiro ondula: o campo contínuo de morros e descidas (não peças soltas). O ruído entortado em
				// ondas de ~150 m com cristas de ~260 m (o "ridged" ao quadrado), simétrico como a ondulação de antes; mais alto
				// no sopé dos maciços e das bordas de platô (o morro nasce da serra, não do nada), mais suave nas trilhas e
				// nada nas bases e nos campos de recurso
				if (S.Hills > 0.01f && FormDB.Num())
				{
					const FIntPoint Lc = LCellOf(P);
					const float Db = FormDB[LIdx(Lc.X, Lc.Y)], Dpth = FormDP[LIdx(Lc.X, Lc.Y)];
					float Am = 8.0f * S.Hills *   // (12 dava só 13% plano e a rampa mediana de 7°; o ZH tem 50–75% plano)
						 (1.0f + 0.8f * FMath::Exp(-Db / 70.0f)) * FMath::Lerp(0.35f, 1.0f, FMath::SmoothStep(8.0f, 60.0f, Dpth));
					for (const FVector2f& St : StartPos) Am *= FMath::SmoothStep(BaseRadius + 10.0f, BaseRadius + 70.0f, FVector2f::Distance(P, St));
					for (const FWorldResource& Rs : M.Resources) Am *= FMath::SmoothStep(Rs.Radius + 5.0f, Rs.Radius + 40.0f, FVector2f::Distance(P, Rs.Pos));
					if (Am > 0.01f) H += Am * Sym.Field(P, Rolling, true) * (1.0f - S4) * S1;
				}
				// (16/10) os morros e descidas marcantes (PlaceForms), só na baixada fora da montanha
				if (FormList.Num())
				{
					float Fn;
					H += FormHeight(P, Fn) * (1.0f - S4) * S1;
					FormField[V] = Fn * (1.0f - S4) * S1;
				}
				H = FMath::Max(H, FMath::Lerp(H, WaterSurface + 0.5f, S1));
				const float Sea = SampleLogic(BSea, NL, U, W);
				if (Sea > 0.0f)
				{
					const float Edge = Half - FMath::Max(FMath::Abs(P.X), FMath::Abs(P.Y));
					// (com ruído: só pela borda o talude fazia faixas retas paralelas a ela)
					const float Out = FMath::Clamp(1.0f - (Edge + 25.0f * Sym.Field(P, Undulate, true)) / SeaWidth, 0.0f, 1.0f);   // 0 na costa, 1 na borda
					H = FMath::Min(H, FMath::Lerp(H, WaterSurface - 2.0f - 43.0f * Out * Out, FMath::SmoothStep(0.3f, 0.9f, Sea)));
				}
				// salina: fundo plano 0,5 m abaixo da baixada, borda suave de ~10 m
				const float Sb = SampleLogic(BSalt, NL, U, W);
				if (Sb > 0.0f) H = FMath::Lerp(H, LevelHeight(LLow) - 0.5f, FMath::SmoothStep(0.15f, 0.7f, Sb));
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
		if (FormField.Num()) Sym.Enforce(FormField, N);
		M.Forms = FormField;
		Sym.Enforce(OpenField, N);
		Sym.Enforce(TalusField, N);

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
			// (16/10) nas formas: a amplitude da de cima (força × escala, 2,6–6,8 m) picava um morro de 5 m
			ErosionFilter::FParams PrmF = Prm;
			PrmF.Scale = 16.0f;
			PrmF.Strength = 0.03f + 0.04f * S.Erosion;   // (a 1ª, 0,05–0,12, deixava os morros com cara de couve-flor)
			PrmF.Seed = SubSeed(11);
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
					const float Fm = FormField.Num() ? FormField[V] : 0.0f;
					const float Mask = FMath::Max(MountainField[V], FMath::SmoothStep(0.2f, 0.6f, Grad.Size()) * (1.0f - FMath::SmoothStep(0.0f, 0.15f, Fm)));
					const float MaskF = Fm > 0.02f ? FMath::SmoothStep(0.02f, 0.2f, Fm) * FMath::SmoothStep(0.02f, 0.12f, Grad.Size()) : 0.0f;
					if (Mask < 0.02f && MaskF < 0.02f) continue;
					const FVector2f P = M.CellPos(I, J);
					const float Fade = MountainField[V] > 0.1f ? FMath::Clamp((Base[V] - LevelHeight(LHigh)) / (0.6f * MountainAmp) * 2.0f - 1.0f, -1.0f, 1.0f) : 0.0f;
					auto Gully = [&](FVector2f Q, int32 Rot)
					{
						const FVector2f GR = Sym.Rot(Grad, Rot);
						return ErosionFilter::Apply(Q, GR.X, GR.Y, Fade, Prm);
					};
					float Add = Mask >= 0.02f ? Mask * Sym.Field(P, Gully, true) : 0.0f;
					if (MaskF >= 0.02f)
					{
						// o sulco da chuva no flanco: −1 no pé, +1 no topo (a forma normalizada)
						const float FadeF = FMath::Clamp(2.0f * Fm - 1.0f, -1.0f, 1.0f);
						auto GullyF = [&](FVector2f Q, int32 Rot)
						{
							const FVector2f GR = Sym.Rot(Grad, Rot);
							return ErosionFilter::Apply(Q, GR.X, GR.Y, FadeF, PrmF);
						};
						Add += MaskF * Sym.Field(P, GullyF, true);
					}
					M.Height[V] = Base[V] + Add;
				}
			});
			Sym.Enforce(M.Height, N);
		}

		// terra seca de verdade: longe da água o chão nunca desce a menos de
		// 0,35 m da cota d'água. Os sulcos da erosão no pé dos penhascos furavam
		// a cota e, com a água cobrindo o mapa todo, viravam poças (07/10)
		{
			const TArray<float> Land = Indicator([](uint8 L) { return L >= LLow; }, 2, 2);
			ParallelFor(N, [&](int32 J)
			{
				for (int32 I = 0; I < N; ++I)
				{
					const FVector2f P = M.CellPos(I, J);
					const float U = (P.X + Half) / LogicCell - 0.5f, W = (P.Y + Half) / LogicCell - 0.5f;
					const float Dry = FMath::SmoothStep(0.85f, 0.98f, SampleLogic(Land, NL, U, W));
					float& H = M.Height[M.Index(I, J)];
					H = FMath::Lerp(H, FMath::Max(H, WaterSurface + 0.35f), Dry);
				}
			});
			Sym.Enforce(M.Height, N);
		}

		// os monólitos: parede quase reta, topo de lajes e a saia de tálus no pé
		// (a pedra que despencou). O contorno é uma superelipse recortada pelo
		// ruído (as reentrâncias dos arenitos de AlUla), a parede tem ~70–80°
		// (2 m de transição) e o topo fica ~8% mais baixo na borda (abaulado)
		PillarField.SetNumZeroed(N * N);
		if (Pillars.Num())
		{
			ParallelFor(N, [&](int32 J)
			{
				for (int32 I = 0; I < N; ++I)
				{
					const int32 V = M.Index(I, J);
					if (Sym.Canon(FIntPoint(I, J), N) != V) continue;
					const FVector2f P = M.CellPos(I, J);
					float Add = 0.0f, Mask = 0.0f, Talus = 0.0f, Wet = 0.0f;
					for (const FPillar& Pl : Pillars)
					{
						const FVector2f L = P - Pl.C;
						const float Reach = FMath::Max(Pl.A, Pl.B) * 1.6f + 10.0f + 1.4f * Pl.Pond;
						if (L.SizeSquared() > Reach * Reach) continue;
						// a margem: a ~1,5 m pra dentro a água já cobre; o meio fica a ~2,8 m
						Wet = FMath::Max(Wet, FMath::SmoothStep(0.0f, 7.0f, PondInside(Pl, L)));
						if (Pl.Variant >= 0)
						{
							// a malha faz o corpo; o relevo, o pé: plano ~0,6 m acima dentro da pegada (a malha
							// enterra 1 m) e a rampa de pedra solta e areia de ~0,1 da altura até ~8 m fora
							const float Th = FMath::Atan2(L.Y, L.X);
							const float Fr = FootR(Pl, Th);
							const float Out = L.Size() - Fr;
							if (Out > 11.0f) continue;
							const float W = 1.0f - FMath::SmoothStep(-1.5f, 0.0f, Out);
							// (mais baixo e liso: com 0,1·H e o ruído de 4 m o pé saía cheio de calombos, 13/10)
							// o monte continua por baixo da pedra (com 0,6 m dentro e o monte de ~2,6 m fora, ficava um
							// fosso em volta do pé, 13/10): a malha nasce de dentro da pedra solta
							const float Sk = 0.065f * Pl.H * FMath::Square(1.0f - FMath::SmoothStep(0.0f, 10.0f, FMath::Max(Out, 0.0f)))
								* (0.9f + 0.1f * Fbm(P.X / 9.0f, P.Y / 9.0f, Pl.Seed + 9u, 2)) * (1.0f - 0.85f * PondSector(Pl, Th));
							Add = FMath::Max(Add, FMath::Max(W * 0.6f, Sk));
							Mask = FMath::Max(Mask, W);
							Talus = FMath::Max(Talus, 0.6f * FMath::Clamp(Sk / (0.065f * Pl.H), 0.0f, 1.0f) * (1.0f - W));
							continue;
						}
						const float Cs = FMath::Cos(Pl.Yaw), Sn = FMath::Sin(Pl.Yaw);
						const float Lu = (L.X * Cs + L.Y * Sn) / Pl.A, Lv = (-L.X * Sn + L.Y * Cs) / Pl.B;
						// superelipse (p = 2,6: quina arredondada, lado reto) e o recorte
						const float D0 = FMath::Pow(FMath::Pow(FMath::Abs(Lu), 2.6f) + FMath::Pow(FMath::Abs(Lv), 2.6f), 1.0f / 2.6f);
						const float Th = FMath::Atan2(Lv, Lu);
						const float Cut = 0.14f * Fbm(FMath::Cos(Th) * 1.7f + Pl.Seed % 97, FMath::Sin(Th) * 1.7f, Pl.Seed, 3)
							+ 0.05f * Fbm(P.X / 3.0f, P.Y / 3.0f, Pl.Seed + 3u, 2);
						const float D = D0 * (1.0f - Cut);
						const float Edge = 2.0f / FMath::Min(Pl.A, Pl.B);   // ~2 m de parede
						const float W = 1.0f - FMath::SmoothStep(1.0f - Edge, 1.0f, D);
						if (W <= 0.0f && D > 1.0f + 12.0f / FMath::Min(Pl.A, Pl.B)) continue;
						// topo: abaulado, lajes de ~3 m (o arenito em camadas) e o recorte fino
						const float Top = Pl.H * (0.92f + 0.08f * (1.0f - FMath::Min(D * D, 1.0f))) + 1.2f * Fbm(P.X / 7.0f, P.Y / 7.0f, Pl.Seed + 5u, 3);
						// tálus: rampa de pedra solta até ~10 m da parede, ~18% da altura
						const float Ft = FMath::Max(0.0f, D - 1.0f) * FMath::Min(Pl.A, Pl.B);   // metros fora da parede
						const float Sk = (D > 0.9f ? 1.0f : 0.0f) * 0.18f * Pl.H * FMath::Square(1.0f - FMath::SmoothStep(0.0f, 10.0f, Ft))
							* (0.75f + 0.25f * Fbm(P.X / 4.0f, P.Y / 4.0f, Pl.Seed + 9u, 2))
							* (1.0f - 0.85f * PondSector(Pl, FMath::Atan2(L.Y, L.X)));
						Add = FMath::Max(Add, FMath::Max(W * Top, Sk));
						Mask = FMath::Max(Mask, W);
						Talus = FMath::Max(Talus, FMath::Clamp(Sk / (0.18f * Pl.H), 0.0f, 1.0f) * (1.0f - W));
					}
					if (Add <= 0.0f && Wet <= 0.0f) continue;
					M.Height[V] = FMath::Lerp(M.Height[V] + Add, WaterSurface - 2.8f, Wet);
					PillarField[V] = Mask;
					MountainField[V] = FMath::Max(MountainField[V], Mask);
					TalusField[V] = FMath::Max(TalusField[V], Talus);
				}
			});
			Sym.Enforce(M.Height, N);
			Sym.Enforce(PillarField, N);
			Sym.Enforce(MountainField, N);
			Sym.Enforce(TalusField, N);
		}
		M.Monoliths.Reset();
		for (const FPillar& Pl : Pillars)
		{
			if (Pl.Variant < 0 || !MonoKit.IsValidIndex(Pl.Variant)) continue;
			FWorldMonolith& Mo = M.Monoliths.AddDefaulted_GetRef();
			Mo.Mesh = MonoKit[Pl.Variant].Name;
			Mo.Pos = Pl.C;
			Mo.Z = M.HeightAt(Pl.C) - 0.6f;
			Mo.Yaw = FMath::RadiansToDegrees(Pl.Yaw);
			Mo.Scale = Pl.Scale;
		}
	}

	// ------------------------------------------------------------------------
	// 8. Camadas do chão, fluxo d'água, passabilidade
	// ------------------------------------------------------------------------
	void FGen::BuildLayers()
	{
		const int32 N = M.N;
		// fluxo D8: cada vértice manda a água acumulada pro vizinho mais baixo.
		// Sobre o relevo SUAVIZADO (~10 m): no relevo cru o chão plano do RTS
		// riscava linhas paralelas; no suave os canais se juntam como numa bacia
		// de verdade (a pesquisa de 07/10: drenar a forma, não o detalhe)
		TArray<float> Hs = M.Height;
		Blur(Hs, N, 4, 3);
		// Enche as depressões antes do D8 (Priority-Flood+ε, Barnes, Lehman &
		// Mulla 2014): sem isso a água parava em cada buraquinho do chão plano do
		// RTS, os leitos saíam em pedaços curtos e nunca chegavam a lugar nenhum.
		// As saídas: a borda do mapa, a água (mar, lago) e as bacias fechadas do
		// deserto (salina, oásis, lagoa seca: é ali que o leito do deserto termina,
		// como nos alvos). O ε por passo varia com um ruído de ~40 m pra que a
		// água atravesse os trechos enchidos serpenteando (com ε fixo os caminhos
		// saíam em linhas retas paralelas)
		{
			TArray<uint8> Closed;
			Closed.SetNumZeroed(N * N);
			typedef TPair<float, int32> FCell;
			auto Lower = [](const FCell& A, const FCell& B) { return A.Key < B.Key; };
			TArray<FCell> Heap;
			for (int32 J = 0; J < N; ++J)
			{
				for (int32 I = 0; I < N; ++I)
				{
					const int32 V = M.Index(I, J);
					const FIntPoint LC = LCellOf(M.CellPos(I, J));
					const bool bBasin = BasinCell.IsValidIndex(LIdx(LC.X, LC.Y)) && BasinCell[LIdx(LC.X, LC.Y)] != 0;
					if (I == 0 || J == 0 || I == N - 1 || J == N - 1 || Hs[V] < WaterSurface || bBasin)
					{
						Closed[V] = 1;
						Heap.HeapPush(FCell(Hs[V], V), Lower);
					}
				}
			}
			const uint32 SeedF = SubSeed(24);
			while (Heap.Num())
			{
				FCell Top;
				Heap.HeapPop(Top, Lower, EAllowShrinking::No);
				const int32 I = Top.Value % N, J = Top.Value / N;
				for (int32 DJ = -1; DJ <= 1; ++DJ)
				{
					for (int32 DI = -1; DI <= 1; ++DI)
					{
						const int32 X = I + DI, Y = J + DJ;
						if ((DI | DJ) == 0 || X < 0 || Y < 0 || X >= N || Y >= N) continue;
						const int32 Q = M.Index(X, Y);
						if (Closed[Q]) continue;
						Closed[Q] = 1;
						const FVector2f Pq = M.CellPos(X, Y);
						const float Eps = 1e-3f * (1.0f + 0.9f * Fbm(Pq.X / 40.0f, Pq.Y / 40.0f, SeedF, 2));
						Hs[Q] = FMath::Max(Hs[Q], Hs[Top.Value] + Eps);
						Heap.HeapPush(FCell(Hs[Q], Q), Lower);
					}
				}
			}
		}
		TArray<int32> Order;
		Order.SetNumUninitialized(N * N);
		for (int32 K = 0; K < N * N; ++K) Order[K] = K;
		Order.Sort([&Hs](int32 A, int32 B) { return Hs[A] > Hs[B] || (Hs[A] == Hs[B] && A < B); });
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
					const float Drop = (Hs[V] - Hs[M.Index(X, Y)]) / ((DI && DJ) ? 1.41421356f : 1.0f);
					if (Drop > BestDrop) { BestDrop = Drop; Best = M.Index(X, Y); }
				}
			}
			if (Best >= 0) Flow[Best] += Flow[V];
		}
		Sym.Enforce(Flow, N);

		// leitos secos (wadi, riacho seco): onde a bacia acima passa de ~0,6 ha.
		// Leito de rio efêmero é largo e raso, de fundo chato (Graf 1988, rios de
		// terra seca; largura ∝ vazão^0,5, Leopold & Maddock 1953): a linha de
		// fluxo do D8 (1 vértice) vira um leito de 4–18 m de largura e 0,3–0,8 m de
		// fundo, com barranco de ~3,5 m (≤ 13°: o tanque passa, vau seco). Antes era
		// o canal estreito do borrão de 2 m, que de cima lia como um degrau fino
		// (revisão de arte de 07/10). Nunca na base, na rampa, na montanha nem na água
		{
			// força do leito na linha de fluxo (0–1)
			TArray<float> Line;
			Line.SetNumZeroed(N * N);
			for (int32 V = 0; V < N * N; ++V)
			{
				Line[V] = FMath::SmoothStep(FMath::Loge(1500.0f), FMath::Loge(25000.0f), FMath::Loge(Flow[V]));
			}
			// distância de cada vértice até a linha (BFS de 8 vizinhos, até 14 m),
			// levando a força do ponto da linha mais perto
			const float MaxD = 14.0f;
			TArray<float> Dist, Str;
			Dist.Init(1e9f, N * N);
			Str.Init(0.0f, N * N);
			TArray<int32> Queue;
			for (int32 V = 0; V < N * N; ++V)
			{
				if (Line[V] > 0.01f) { Dist[V] = 0.0f; Str[V] = Line[V]; Queue.Add(V); }
			}
			for (int32 Head = 0; Head < Queue.Num(); ++Head)
			{
				const int32 V = Queue[Head];
				const int32 I = V % N, J = V / N;
				for (int32 DJ = -1; DJ <= 1; ++DJ)
				{
					for (int32 DI = -1; DI <= 1; ++DI)
					{
						const int32 X = I + DI, Y = J + DJ;
						if ((DI | DJ) == 0 || X < 0 || Y < 0 || X >= N || Y >= N) continue;
						const int32 Q = M.Index(X, Y);
						const float ND = Dist[V] + M.Cell * ((DI && DJ) ? 1.41421356f : 1.0f);
						if (ND < MaxD && (ND < Dist[Q] - 0.01f || (ND < Dist[Q] + 0.01f && Str[V] > Str[Q])))
						{
							Dist[Q] = ND;
							Str[Q] = Str[V];
							Queue.Add(Q);
						}
					}
				}
			}
			const uint32 SeedW = SubSeed(23);
			M.Bed.Init(0.0f, N * N);
			ParallelFor(N, [&](int32 J)
			{
				for (int32 I = 0; I < N; ++I)
				{
					const int32 V = M.Index(I, J);
					const FVector2f P = M.CellPos(I, J);
					// o leito lido num ponto deslocado por ruído de ~45 m (até ~6 m): as
					// linhas do D8 no chão enchido saem retas (0°/45°/90°) e o leito
					// serpenteia em volta delas
					const FVector2f Wq = M.ToGrid(P + 6.0f * FVector2f(Fbm(P.X / 45.0f, P.Y / 45.0f, SeedW + 1u, 2), Fbm(P.X / 45.0f, P.Y / 45.0f, SeedW + 2u, 2)));
					// (bilinear: pelo vértice mais perto a borda do leito saía em dente de
					// serra de 2 m, visto no leito clareado, 07/10)
					const float Wx = FMath::Clamp(Wq.X, 0.0f, float(N - 1)), Wy = FMath::Clamp(Wq.Y, 0.0f, float(N - 1));
					const int32 X0 = FMath::Min(int32(Wx), N - 2), Y0 = FMath::Min(int32(Wy), N - 2);
					const float Fx = Wx - X0, Fy = Wy - Y0;
					auto Bi = [&](const TArray<float>& F)
					{
						return FMath::Lerp(FMath::Lerp(F[M.Index(X0, Y0)], F[M.Index(X0 + 1, Y0)], Fx), FMath::Lerp(F[M.Index(X0, Y0 + 1)], F[M.Index(X0 + 1, Y0 + 1)], Fx), Fy);
					};
					// distância limitada (fora do alcance vale MaxD) e força só onde há leito perto
					auto DistC = [&](int32 Q) { return FMath::Min(Dist[Q], MaxD); };
					const float DistW = FMath::Lerp(FMath::Lerp(DistC(M.Index(X0, Y0)), DistC(M.Index(X0 + 1, Y0)), Fx), FMath::Lerp(DistC(M.Index(X0, Y0 + 1)), DistC(M.Index(X0 + 1, Y0 + 1)), Fx), Fy);
					const float StrW = Bi(Str);
					if (StrW <= 0.01f || DistW >= MaxD) continue;
					const FIntPoint LC = LCellOf(P);
					const uint8 L = Level[LIdx(LC.X, LC.Y)];
					if (L == LWater || L == LMountain || FrameMask[LIdx(LC.X, LC.Y)] || RampWeight[V] > 0.05f || MountainField[V] > 0.05f) continue;
					// o leito acaba na bacia (salina, oásis, barreiro): o plano dela fica liso
					if (BasinCell.IsValidIndex(LIdx(LC.X, LC.Y)) && BasinCell[LIdx(LC.X, LC.Y)] != 0) continue;
					float NearBase = 1.0f;
					for (const FVector2f& St : StartPos) NearBase = FMath::Min(NearBase, FMath::SmoothStep(BaseRadius, BaseRadius + 20.0f, FVector2f::Distance(P, St)));
					// meia largura 2–9 m pela força, ±25% de ruído ao longo do leito. Na
					// floresta, córrego de 2–6 m (clima úmido: a bacia pequena vira
					// riacho estreito com mata em volta, não leito largo e nu)
					const bool bBrook = S.Biome == EWorldBiome::Temperate;
					const float Half = (bBrook ? 1.0f + 2.0f * StrW : 2.0f + 7.0f * StrW) * (1.0f + 0.25f * Fbm(P.X / 30.0f, P.Y / 30.0f, SeedW, 2));
					const float Floor = 1.0f - FMath::SmoothStep(Half - (bBrook ? 1.0f : 2.5f), Half + 1.0f, DistW);
					if (Floor <= 0.0f) continue;
					const float Depth = (bBrook ? 0.2f + 0.3f * StrW : 0.3f + 0.5f * StrW) * FMath::Min(1.0f, 3.0f * StrW);
					const float Cut = Depth * Floor * NearBase;
					// o fundo fica seco: nunca abaixo de 0,4 m sobre a água (fazia poça no pé do penhasco)
					M.Height[V] = FMath::Max(M.Height[V] - Cut, FMath::Min(M.Height[V], WaterSurface + 0.4f));
					// o leito inteiro, pro editor pintar o fundo (WadiAt). Antes ia no
					// fluxo, e os fios do fluxo natural pintavam riscos finos por todo o
					// chão (gelo em fio na neve, 07/10)
					M.Bed[V] = Floor * NearBase * FMath::Min(1.0f, 3.0f * StrW);
				}
			});
			Sym.Enforce(M.Height, N);
			Sym.Enforce(M.Bed, N);
		}

		const uint32 SeedN = SubSeed(11);
		M.Layers.SetNumUninitialized(N * N);
		M.Pass.SetNumUninitialized(N * N);
		M.Flow.SetNumUninitialized(N * N);
		M.Basin.SetNumZeroed(N * N);
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
				float Fl = FMath::Clamp(FMath::Loge(Flow[V]) / 9.0f, 0.0f, 1.0f);
				{
					const FIntPoint DC = LCellOf(P);
					if (BasinCell.IsValidIndex(LIdx(DC.X, DC.Y)) && BasinCell[LIdx(DC.X, DC.Y)] == 3) Fl = FMath::Min(Fl, 0.35f);
				}
				M.Flow[V] = Fl;
				{
					const FIntPoint BC = LCellOf(P);
					const uint8 Bc = BasinCell[LIdx(BC.X, BC.Y)];
					// 3: lagoa seca pequena (o editor pinta como barreiro de argila)
				M.Basin[V] = Bc == 2 ? 2 : (SaltField.Num() && SampleLogic(SaltField, NL, U, W) > 0.5f ? 1 : (Bc == 3 ? 3 : 0));
				}

				float Rock = FMath::Max(FMath::SmoothStep(0.42f, 0.80f, Slope + 0.12f * Fine), 0.5f * TalusField[V] * (0.7f + 0.6f * Fine));
				Rock = FMath::Max(Rock, MountainField[V] * FMath::SmoothStep(0.15f, 0.45f, Slope + 0.2f * Patch));
				// areia: a faixa da margem (o indicador de terra cai perto da água) e o fundo raso
				const float Shore = 1.0f - FMath::SmoothStep(0.55f, 0.92f, SampleLogic(ShoreField, NL, U, W) + 0.12f * Fine);
				float Sand = FMath::Max(Shore, 1.0f - FMath::SmoothStep(WaterSurface - 0.2f, WaterSurface + 0.6f, H));
				// salina: o rascunho mostra a crosta clara pela camada de areia,
				// pelo contorno suave (a célula de 4 m serrilhava a borda)
				const float SaltV = SaltField.Num() ? FMath::SmoothStep(0.35f, 0.65f, SampleLogic(SaltField, NL, U, W) + 0.15f * Fine) : 0.0f;
				Sand = FMath::Max(Sand, SaltV);
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
				const float OasisG = OasisField.Num() ? FMath::SmoothStep(0.03f, 0.25f, SampleLogic(OasisField, NL, U, W)) : 0.0f;
				const float Lush = FMath::Clamp(OasisG + 0.30f + 0.5f * Fl + 0.65f * Patch + 0.15f * (1.0f - Track) - 0.4f * Rock + 1.6f * TalusField[V] * (1.0f - TalusField[V]), 0.0f, 1.0f);   // (verde no pé do tálus: a água escorre ali)
				M.Layers[V] = FColor(uint8(255.0f * FMath::Clamp(Dirt, 0.0f, 1.0f)), uint8(255.0f * Lush),
					uint8(255.0f * FMath::Clamp(Sand, 0.0f, 1.0f)), uint8(255.0f * FMath::Clamp(Rock, 0.0f, 1.0f)));

				// passabilidade vista pela unidade: água funda, encosta demais ou montanha
				EWorldPass Pass = EWorldPass::Open;
				const FIntPoint LC = LCellOf(P);
				const uint8 L = Level[LIdx(LC.X, LC.Y)];
				if (H < WaterSurface - 0.5f) Pass = EWorldPass::Water;
				else if ((Slope > CliffSlope && RampWeight[V] < 0.5f) || (L == LMountain && MountainField[V] > 0.5f)) Pass = EWorldPass::Cliff;
				else if (PillarField.Num() && PillarField[V] > 0.35f) Pass = EWorldPass::Cliff;   // o monólito
				else if (OpenField[V] > 0.5f) Pass = EWorldPass::Cliff;   // colinas da moldura: fora da área de jogo
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

		// oásis: o verde na margem (até ~20 m da água)
		TArray<float> Oasis;
		Oasis.SetNumUninitialized(NL * NL);
		for (int32 C = 0; C < NL * NL; ++C) Oasis[C] = BasinCell.IsValidIndex(C) && BasinCell[C] == 2 ? 1.0f : 0.0f;
		Blur(Oasis, NL, 3, 2);
		Sym.Enforce(Oasis, NL);

		// árvores: manchas de floresta (ruído) onde é plano, seco o bastante e
		// longe de base, recurso, rampa e trilha; árvores soltas pelo campo
		if (S.Forests > 0.01f || bDesert)
		{
			Scatter(5.0f, SeedT, [&](FVector2f P, FRng& Rng, FWorldProp& Prop)
			{
				const FVector3f Nrm = M.NormalAt(P);
				const float H = M.HeightAt(P);
				const bool bNearOasis = bDesert && SampleLogic(Oasis, NL, (P.X + Half) / LogicCell - 0.5f, (P.Y + Half) / LogicCell - 0.5f) > 0.04f;
				if (Nrm.Z < 0.92f || H < WaterSurface + (bNearOasis ? 0.25f : 0.8f) || !Clear(P, 22.0f)) return false;
				const FVector2f G = M.ToGrid(P);
				const int32 V = M.Index(FMath::Clamp(FMath::RoundToInt(G.X), 0, M.N - 1), FMath::Clamp(FMath::RoundToInt(G.Y), 0, M.N - 1));
				// moldura de colinas: mata fechada emoldurando o mapa (fora do deserto)
				const bool bFrame = OpenField[V] > 0.5f;
				if (bFrame && bDesert) return false;
				if ((M.Pass[V] != EWorldPass::Open && !bFrame) || MountainField[V] > 0.4f) return false;
				const FVector2f LG((P.X + Half) / LogicCell - 0.5f, (P.Y + Half) / LogicCell - 0.5f);
				if (SampleLogic(TrackField, NL, LG.X, LG.Y) > 0.08f) return false;
				const float Lush = M.Layers[V].G / 255.0f;
				const float F = FMath::SmoothStep(Tau, Tau + 0.12f, Sym.Field(P, Forest, true));
				float Prob = F * 0.85f * (0.5f + 0.7f * Lush) + 0.012f * S.Forests;
				if (bFrame) Prob = 0.25f + 0.6f * FMath::SmoothStep(Tau - 0.2f, Tau + 0.1f, Sym.Field(P, Forest, true));
				if (bDesert) Prob *= 0.2f;
				// o oásis: denso junto da água e rareando pra fora (não um anel de
				// espaçamento igual, que lia como cerca), tamanhos variados
				const float OasisW = bDesert ? SampleLogic(Oasis, NL, LG.X, LG.Y) : 0.0f;
				const bool bOasis = OasisW > 0.04f;
				if (bOasis) Prob = 0.95f * FMath::SmoothStep(0.04f, 0.55f, OasisW + 0.15f * (Rng.Float() - 0.5f));
				if (Rng.Float() >= Prob) return false;
				Prop.Type = EWorldPropType::Tree;
				Prop.Scale = Rng.Range(0.75f, 1.25f);
				Prop.Yaw = Rng.Range(0.0f, 360.0f);
				Prop.Variant = uint8(Rng.Int(256));
				if (bDesert) Prop.Variant = bOasis ? uint8(128 + Rng.Int(128)) : uint8(Rng.Int(128));   // WorldTrees::KindOf
				if (bOasis) Prop.Scale = Rng.Range(0.55f, 1.3f);
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
		for (const FPillar& P : Pillars) bWater |= P.Pond > 0.0f;
		M.WaterLevel = bWater ? WaterSurface : -1000.0f;
		// o vento do mapa: a sombra das cristas na textura das ondulações (T_LL_Ripples) foi assada pra um sol a
		// 155° do vento, do lado de onde ele vem. A receita do deserto põe o sol a −42° da câmera inicial (yaw
		// 45°): o sol fica a 87° e o vento a 87 − 155 ± 20° (regra do Look, 13/10)
		{
			FRng WindRng(SubSeed(41));
			M.WindYaw = 87.0f - 155.0f + WindRng.Range(-20.0f, 20.0f);
			M.WindSpeed = WindRng.Range(3.0f, 8.0f);
		}
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
			Time(TEXT("levels"), [this] { MakeLevels(); Cleanup(); MakeBasins(); MakeCuestas(); MakeMonoliths(); });
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
		Time(TEXT("forms"), [this] { PlaceForms(); });
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
