#pragma once

// A areia simulada (16/10 à tarde, o Felipe sobre as barcanas por fórmula: "ainda tá muito padronizado, não tem nada
// natural aí"). Em vez de desenhar cada duna, o vento leva a areia em lajes e as dunas nascem sozinhas: o modelo de
// lajes de Werner (1995), o mesmo de Paris et al. 2019 ("Desertscapes", código MIT) e do Dune Solver do Houdini.
// Uma laje sai de uma célula com areia fora da sombra, salta Hop células a favor do vento e para com probabilidade
// Ps (cai em areia) ou Pn (chão nu), ou sempre se cair na sombra de vento (abaixo da reta de 15° a sotavento de um
// alto). Depois de cada mudança, a avalanche escorre o que passa do ângulo de repouso. Só C++ padrão (sem a Unreal):
// o mesmo código roda no gerador e no teste de bancada (Tools/dunesim).
// (a 2ª opinião, que leu o código do Paris 2019 e mediu num porte: as barcanas de lá só existem porque a laje que não
// para em 3 saltos some (MAX_BOUNCE): sem perder areia, pouca vira tapete e muita vira transversal. O salto é de 5 m
// a 1 m por célula, a sombra é uma rampa de chance entre 5° e 15° olhando 10 m a barlavento, o repouso é tg 0,6 (31°),
// e não estabiliza: para pela cobertura (~12% com mais de 20 cm), com largura mediana de 16–35 m e altura de 1,4–2,8 m.
// Na bancada (16/10), com a perda as dunas saíram imaturas, montes sem crescente; a areia conservada, o salto de 2 m e
// a sombra dura de 15° deram as barcanas de forma certa, só altas demais: o acabamento achata. Os dois modos ficam:
// MaxBounce 0 = conservada (o padrão), > 0 = a perda do Paris)

#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>

namespace DuneSim
{
	struct FParams
	{
		int N = 512;                 // a grade (N×N células), periódica
		float Cell = 1.0f;           // m por célula
		float Slab = 0.1f;           // m por laje
		float WindX = 1.0f, WindY = 0.0f;   // a direção do vento (unitária)
		float Hop = 2.0f;            // o salto da laje (células; o Paris usa 5 com a perda)
		float HopJitter = 0.35f;     // o acaso no comprimento do salto (±fração)
		float Lateral = 0.6f;        // o desvio de lado em cada salto (±células; sem ele a areia fina vira listras)
		float Ps = 0.6f, Pn = 0.4f;  // a chance de parar em areia / em chão nu
		float ShadowLoDeg = 15.0f, ShadowHiDeg = 15.0f;   // a sombra de vento: a chance sobe de 0 a 1 entre os dois ângulos (14–15°: quase a sombra dura do Werner, a variante que deu barcana no porte da 2ª opinião; a rampa de 5–15° do Paris freava a erosão e a areia não se perdia)
		float ReposeDeg = 33.0f;     // o ângulo de repouso (a avalanche)
		int ShadowReach = 24;        // até onde a barlavento olha a sombra (células)
		int MaxBounce = 0;           // 0: a areia se conserva (a laje que não para em 400 saltos volta); > 0: some depois de tantos saltos (o Paris)
		uint32_t Seed = 1;
	};

	struct FState
	{
		FParams P;
		std::vector<int16_t> H;      // a espessura de areia em lajes
		std::vector<uint8_t> Mask;   // 1 = a areia pode parar ali
		uint64_t Rng = 0x9E3779B97F4A7C15ull;
		// (a velocidade) os deslocamentos da sombra a barlavento e os limites em lajes, prontos
		std::vector<int> ShX, ShY;
		std::vector<float> ShK;      // lajes → tangente na distância D
		float TanR = 6.5f, TanLo = 0.08f, TanHi = 0.26f;
		int NMask = 511;

		int Idx(int X, int Y) const { return (Y & NMask) * P.N + (X & NMask); }   // N potência de 2: o & dá a volta (periódica)
		uint32_t Next()
		{
			Rng ^= Rng << 13; Rng ^= Rng >> 7; Rng ^= Rng << 17;
			return uint32_t(Rng >> 32);
		}
		float Unit() { return (Next() >> 8) * (1.0f / 16777216.0f); }

		void Init(const FParams& InP)
		{
			P = InP;
			H.assign(size_t(P.N) * P.N, 0);
			Mask.assign(size_t(P.N) * P.N, 1);
			Rng = 0x9E3779B97F4A7C15ull ^ (uint64_t(P.Seed) * 0xD1B54A32D192ED03ull);
			Next();
			NMask = P.N - 1;   // (N tem que ser potência de 2)
			ShX.clear(); ShY.clear(); ShK.clear();
			for (int D = 1; D <= P.ShadowReach; ++D)
			{
				ShX.push_back(-int(std::lround(D * P.WindX)));
				ShY.push_back(-int(std::lround(D * P.WindY)));
				ShK.push_back(P.Slab / (D * P.Cell));
			}
			TanLo = std::tan(P.ShadowLoDeg * 3.14159265f / 180.0f);
			TanHi = std::tan(P.ShadowHiDeg * 3.14159265f / 180.0f);
			TanR = std::tan(P.ReposeDeg * 3.14159265f / 180.0f) * P.Cell / P.Slab;
		}

		/** A chance de a célula estar na sombra de vento (0..1): a maior tangente até um alto a barlavento, entre os dois
		 *  ângulos da sombra. */
		float Shadow(int X, int Y) const
		{
			const float H0 = H[Idx(X, Y)];
			const int R = int(ShX.size());
			float Tan = 0.0f;
			for (int D = 0; D < R; ++D) Tan = std::max(Tan, (H[Idx(X + ShX[D], Y + ShY[D])] - H0) * ShK[D]);
			if (TanHi <= TanLo + 1e-4f) return Tan > TanHi ? 1.0f : 0.0f;   // a sombra dura (Werner)
			return std::min(std::max((Tan - TanLo) / (TanHi - TanLo), 0.0f), 1.0f);
		}

		/** A avalanche a partir da célula: enquanto um vizinho estiver abaixo do repouso, uma laje escorre pro mais íngreme. */
		void SlideFrom(int X, int Y, int Depth = 0)
		{
			static const int Dx[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, Dy[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
			for (int Guard = 0; Guard < 64; ++Guard)
			{
				const int C = Idx(X, Y);
				int Best = -1;
				float BestEx = 0.0f;
				for (int K = 0; K < 8; ++K)
				{
					const int Nb = Idx(X + Dx[K], Y + Dy[K]);
					if (!Mask[Nb]) continue;
					const float Dist = K < 4 ? 1.0f : 1.41421356f;
					const float Ex = (H[C] - H[Nb]) - TanR * Dist;
					if (Ex > BestEx) { BestEx = Ex; Best = K; }
				}
				if (Best < 0 || H[C] <= 0) return;
				const int Nx = X + Dx[Best], Ny = Y + Dy[Best];
				--H[C];
				++H[Idx(Nx, Ny)];
				// o vizinho que recebeu pode passar do repouso também
				if (Depth < 8) SlideFrom(Nx, Ny, Depth + 1);
			}
		}

		/** O buraco deixado pela erosão: os vizinhos altos demais escorrem pra dentro. */
		void SlideInto(int X, int Y)
		{
			static const int Dx[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, Dy[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
			for (int K = 0; K < 8; ++K)
			{
				const int Nb = Idx(X + Dx[K], Y + Dy[K]);
				const float Dist = K < 4 ? 1.0f : 1.41421356f;
				if (H[Nb] - H[Idx(X, Y)] > TanR * Dist) SlideFrom(X + Dx[K], Y + Dy[K]);
			}
		}

		/** Um passo: sorteia uma célula; se tem areia e não está na sombra, uma laje voa a favor do vento até parar. */
		void Step()
		{
			const int N = P.N;
			const uint32_t R = Next();
			const int X = int(R & uint32_t(NMask)), Y = int((R >> 16) & uint32_t(NMask));
			const int C = Y * N + X;
			if (H[C] <= 0 || Unit() < Shadow(X, Y)) return;
			--H[C];
			SlideInto(X, Y);
			float Fx = float(X), Fy = float(Y);
			const int MaxB = P.MaxBounce > 0 ? P.MaxBounce : 400;
			for (int Bounce = 0; Bounce < MaxB; ++Bounce)
			{
				// o salto com um pouco de acaso (comprimento ±HopJitter, de lado ±Lateral): sem isso a areia solta corria em
				// riscos retos nas linhas da grade
				const float Len = P.Hop * (1.0f - P.HopJitter + 2.0f * P.HopJitter * Unit()), Side = 2.0f * P.Lateral * (Unit() - 0.5f);
				Fx += Len * P.WindX - Side * P.WindY;
				Fy += Len * P.WindY + Side * P.WindX;
				const int Tx = int(std::lround(Fx)), Ty = int(std::lround(Fy));
				const int T = Idx(Tx, Ty);
				if (!Mask[T]) continue;   // fora do bolsão a areia não para
				const bool bStop = Unit() < Shadow(Tx, Ty) || Unit() < (H[T] > 0 ? P.Ps : P.Pn);
				if (bStop)
				{
					++H[T];
					SlideFrom(Tx, Ty);
					return;
				}
			}
			// não parou: no modo do Paris a laje sai do campo; no conservado volta pro lugar
			if (P.MaxBounce <= 0) ++H[C];
		}

		void Run(uint64_t Steps) { for (uint64_t I = 0; I < Steps; ++I) Step(); }

		/** A avalanche no campo todo até assentar (a areia inicial é sorteada por célula: sem isso a aspereza punha tudo na
		 *  sombra e nada voava). */
		void Settle(int Passes = 6)
		{
			for (int It = 0; It < Passes; ++It)
				for (int Y = 0; Y < P.N; ++Y)
					for (int X = 0; X < P.N; ++X)
						SlideFrom(X, Y);
		}

		/** A fração das células com mais de MinSlabs de areia. */
		float Cover(int MinSlabs) const { size_t C = 0; for (int16_t V : H) C += V > MinSlabs; return float(C) / float(H.size()); }

		/** Roda até a cobertura (mais de MinSlabs) cair a Target, conferindo a cada Check rodadas; devolve as rodadas. */
		int RunUntilCover(float Target, int MinSlabs, int Check, int MaxSweeps)
		{
			const uint64_t Sweep = uint64_t(P.N) * P.N;
			int Done = 0;
			while (Done < MaxSweeps && Cover(MinSlabs) > Target) { Run(Sweep * Check); Done += Check; }
			return Done;
		}

		uint64_t TotalSlabs() const { uint64_t S = 0; for (int16_t V : H) S += uint64_t(std::max<int16_t>(V, 0)); return S; }
	};

	/** A areia inicial: entre Lo e Hi lajes por célula (o Paris: 0,5–2,0 m nas barcanas), × manchas periódicas leves
	 *  (campos mais e menos cheios), só onde Mask. */
	inline void SeedSand(FState& S, float Lo, float Hi)
	{
		const int N = S.P.N;
		float Ph[6];
		for (float& V : Ph) V = 6.2831853f * S.Unit();
		for (int Y = 0; Y < N; ++Y)
			for (int X = 0; X < N; ++X)
			{
				const float Fx = X * 6.2831853f / N, Fy = Y * 6.2831853f / N;
				// manchas de 1/2 a 1/3 do ladrilho (frequências inteiras: o ladrilho continua periódico)
				const float Patch = 0.5f + 0.25f * std::sin(2 * Fx + Ph[0]) * std::sin(3 * Fy + Ph[1]) + 0.25f * std::sin(3 * Fx + 2 * Fy + Ph[2]);
				const float V = (Lo + (Hi - Lo) * S.Unit()) * (0.7f + 0.6f * Patch);
				const size_t I = size_t(Y) * N + X;
				S.H[I] = S.Mask[I] ? int16_t(V) : 0;
			}
	}

	struct FDune;
	inline std::vector<FDune> Label(const std::vector<float>& H, int N, float Thresh, int MinCells, int Grow,
		std::vector<int>& Lab, std::vector<int>& Near, std::vector<float>& NearDist);

	/** A areia inicial do modo conservado: Mean lajes em média por célula (0 a 2×), × manchas periódicas fortes (campos
	 *  cheios e vazios: 0,2–1,8×). A bancada de 16/10: Mean 2 (20 cm) dá as barcanas; 1–1,6 fica em lençol. */
	inline void SeedSandMean(FState& S, float Mean)
	{
		const int N = S.P.N;
		float Ph[3];
		for (float& V : Ph) V = 6.2831853f * S.Unit();
		for (int Y = 0; Y < N; ++Y)
			for (int X = 0; X < N; ++X)
			{
				const float Fx = X * 6.2831853f / N, Fy = Y * 6.2831853f / N;
				const float Patch = 0.5f + 0.25f * std::sin(2 * Fx + Ph[0]) * std::sin(3 * Fy + Ph[1]) + 0.25f * std::sin(3 * Fx + 2 * Fy + Ph[2]);
				const size_t I = size_t(Y) * N + X;
				S.H[I] = S.Mask[I] ? int16_t(Mean * 2.0f * S.Unit() * (0.2f + 1.6f * Patch)) : 0;
			}
	}

	/** O acabamento: tira os restos pequenos (os pontinhos depois das pontas, antes do desfoque), desfoque gaussiano periódico (Sigma em células: tira os degraus de uma laje), achatado por VScale (a
	 *  simulação de lajes faz a barcana com ~1/7 da largura de altura; as reais têm 1/10–1/15) e avalanche a ReposeDeg.
	 *  Devolve a altura em metros. */
	inline std::vector<float> Finish(const FState& S, float Sigma, float VScale, float ReposeDeg, int MinCells = 60)
	{
		const int N = S.P.N, M = N - 1;
		std::vector<float> A(size_t(N) * N), B(size_t(N) * N);
		for (size_t I = 0; I < A.size(); ++I) A[I] = std::max<int16_t>(S.H[I], 0) * S.P.Slab;
		{
			std::vector<int> Lb, Nr;
			std::vector<float> Nd;
			Label(A, N, 0.05f, MinCells, 0, Lb, Nr, Nd);
			for (size_t I = 0; I < A.size(); ++I) if (Lb[I] < 0) A[I] = 0.0f;
		}
		const int R = std::max(1, int(std::ceil(3.0f * Sigma)));
		std::vector<float> K(2 * R + 1);
		float Ks = 0.0f;
		for (int I = -R; I <= R; ++I) { K[I + R] = std::exp(-0.5f * I * I / (Sigma * Sigma)); Ks += K[I + R]; }
		for (float& V : K) V /= Ks;
		for (int Y = 0; Y < N; ++Y)
			for (int X = 0; X < N; ++X)
			{
				float Sum = 0.0f;
				for (int I = -R; I <= R; ++I) Sum += K[I + R] * A[size_t(Y) * N + ((X + I) & M)];
				B[size_t(Y) * N + X] = Sum;
			}
		for (int Y = 0; Y < N; ++Y)
			for (int X = 0; X < N; ++X)
			{
				float Sum = 0.0f;
				for (int I = -R; I <= R; ++I) Sum += K[I + R] * B[size_t((Y + I) & M) * N + X];
				A[size_t(Y) * N + X] = Sum * VScale;
			}
		// a avalanche: o que passa do repouso escorre pro vizinho (um quarto do excesso por vez, até parar)
		static const int Dx[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, Dy[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
		const float Tan = std::tan(ReposeDeg * 3.14159265f / 180.0f) * S.P.Cell;
		for (int It = 0; It < 400; ++It)
		{
			double Moved = 0.0;
			for (int Y = 0; Y < N; ++Y)
				for (int X = 0; X < N; ++X)
				{
					float& C = A[size_t(Y) * N + X];
					for (int D = 0; D < 8; ++D)
					{
						float& Nb = A[size_t((Y + Dy[D]) & M) * N + ((X + Dx[D]) & M)];
						const float Ex = (C - Nb) - Tan * (D < 4 ? 1.0f : 1.41421356f);
						if (Ex > 0.0f) { const float Mv = 0.25f * Ex; C -= Mv; Nb += Mv; Moved += Mv; }
					}
				}
			if (Moved < 1e-3) break;
		}
		return A;
	}

	/** Uma duna do ladrilho (as células ligadas acima de Thresh): o centro (em células, desenrolado), o raio, a altura
	 *  máxima e pontos de prova do contorno (desvios inteiros em células a partir do centro). */
	struct FDune
	{
		float Cx = 0.0f, Cy = 0.0f, R = 0.0f, HMax = 0.0f;
		int Cells = 0;
		std::vector<int> ProbeX, ProbeY;
	};

	/** As dunas do ladrilho, uma a uma (vizinhança de 8, periódico). Lab recebe a duna de cada célula (−1 = nenhuma);
	 *  Near, a duna mais perto até Grow células, e NearDist, a distância a ela em células (o pé de areia). */
	inline std::vector<FDune> Label(const std::vector<float>& H, int N, float Thresh, int MinCells, int Grow,
		std::vector<int>& Lab, std::vector<int>& Near, std::vector<float>& NearDist)
	{
		const int M = N - 1;
		Lab.assign(size_t(N) * N, -2);
		std::vector<FDune> Out;
		std::vector<int> Qx, Qy;
		for (int Y0 = 0; Y0 < N; ++Y0)
			for (int X0 = 0; X0 < N; ++X0)
			{
				const size_t I0 = size_t(Y0) * N + X0;
				if (Lab[I0] != -2 || H[I0] <= Thresh) continue;
				// as células com as coordenadas desenroladas (atravessar a borda soma ±N)
				Qx.assign(1, X0); Qy.assign(1, Y0);
				const int Id = int(Out.size());
				Lab[I0] = Id;
				for (size_t Q = 0; Q < Qx.size(); ++Q)
					for (int Dy = -1; Dy <= 1; ++Dy)
						for (int Dx = -1; Dx <= 1; ++Dx)
						{
							const int Ux = Qx[Q] + Dx, Uy = Qy[Q] + Dy;
							const size_t J = size_t(Uy & M) * N + (Ux & M);
							if (Lab[J] != -2 || H[J] <= Thresh) continue;
							Lab[J] = Id;
							Qx.push_back(Ux); Qy.push_back(Uy);
						}
				FDune D;
				D.Cells = int(Qx.size());
				double Sx = 0, Sy = 0;
				for (size_t Q = 0; Q < Qx.size(); ++Q)
				{
					Sx += Qx[Q]; Sy += Qy[Q];
					D.HMax = std::max(D.HMax, H[size_t(Qy[Q] & M) * N + (Qx[Q] & M)]);
				}
				D.Cx = float(Sx / Qx.size()); D.Cy = float(Sy / Qy.size());
				for (size_t Q = 0; Q < Qx.size(); ++Q)
				{
					const float Ox = Qx[Q] - D.Cx, Oy = Qy[Q] - D.Cy;
					D.R = std::max(D.R, std::sqrt(Ox * Ox + Oy * Oy));
					if ((Qx[Q] + 2 * Qy[Q]) % 5 == 0) { D.ProbeX.push_back(int(std::lround(Ox))); D.ProbeY.push_back(int(std::lround(Oy))); }
				}
				Out.push_back(std::move(D));
			}
		// as pequenas (riscos de areia solta) ficam de fora: Lab −1
		std::vector<int> Remap(Out.size(), -1);
		std::vector<FDune> Kept;
		for (size_t I = 0; I < Out.size(); ++I)
			if (Out[I].Cells >= MinCells) { Remap[I] = int(Kept.size()); Kept.push_back(std::move(Out[I])); }
		for (int& L : Lab) L = L >= 0 ? Remap[L] : -1;
		// o pé: a duna mais perto até Grow células (busca em largura a partir das dunas)
		Near = Lab;
		std::vector<int> Dist(size_t(N) * N, 1 << 20), Front;
		for (size_t I = 0; I < Near.size(); ++I) if (Near[I] >= 0) { Dist[I] = 0; Front.push_back(int(I)); }
		for (int Step = 1; Step <= Grow && !Front.empty(); ++Step)
		{
			std::vector<int> Next;
			for (int I : Front)
			{
				const int X = I % N, Y = I / N;
				for (int Dy = -1; Dy <= 1; ++Dy)
					for (int Dx = -1; Dx <= 1; ++Dx)
					{
						const size_t J = size_t((Y + Dy) & M) * N + ((X + Dx) & M);
						if (Dist[J] <= Step) continue;
						Dist[J] = Step;
						Near[J] = Near[I];
						Next.push_back(int(J));
					}
			}
			Front.swap(Next);
		}
		NearDist.assign(size_t(N) * N, float(Grow + 1));
		for (size_t I = 0; I < Dist.size(); ++I) if (Near[I] >= 0) NearDist[I] = float(Dist[I]);
		return Kept;
	}
}
