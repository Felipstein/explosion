// A bancada da areia simulada (16/10): roda o DuneSim.h fora da Unreal (o mesmo código do gerador) e grava a altura
// acabada em binário (o formato do dune_shade.py: nx, ny, x0, y0, x1, y1 e as alturas em cm), pro sombreado do
// Saved/Scratch/dune_shade_png.py. Uso: dunesim_bench <saída.bin> N hop lo hi cover seed [vscale] [sigma] [modo]
//   modo 0 (padrão) = conservada: areia média lo lajes, hi rodadas (cover ignorado); modo 1 = a perda do Paris
#include "../../Source/WorldForge/Public/Generation/DuneSim.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>

int main(int argc, char** argv)
{
	if (argc < 8) { std::printf("uso: dunesim_bench out.bin N hop lo hi cover seed [vscale] [sigma]\n"); return 1; }
	DuneSim::FParams P;
	P.N = std::atoi(argv[2]);
	P.Hop = float(std::atof(argv[3]));
	const float Lo = float(std::atof(argv[4])), Hi = float(std::atof(argv[5]));
	const float Target = float(std::atof(argv[6]));
	P.Seed = uint32_t(std::atoi(argv[7]));
	const float VScale = argc > 8 ? float(std::atof(argv[8])) : 1.0f;
	const float Sigma = argc > 9 ? float(std::atof(argv[9])) : 0.7f;
	const int Mode = argc > 10 ? std::atoi(argv[10]) : 0;
	if (Mode == 1) { P.MaxBounce = 3; P.ShadowLoDeg = 14.0f; P.ReposeDeg = 31.0f; P.ShadowReach = 10; P.Lateral = 1.0f; P.HopJitter = 0.2f; }
	DuneSim::FState S;
	S.Init(P);
	if (Mode == 1) { DuneSim::SeedSand(S, Lo, Hi); S.Settle(); }
	else DuneSim::SeedSandMean(S, Lo);
	std::printf("inicio: %llu lajes, cobertura %.1f%%\n", (unsigned long long)S.TotalSlabs(), 100.0 * S.Cover(2));
	const auto T0 = std::chrono::steady_clock::now();
	int Sweeps = 0;
	const int MaxSweeps = Mode == 1 ? 5000 : int(Hi);
	while (Sweeps < MaxSweeps && (Mode == 0 || S.Cover(2) > Target))
	{
		S.Run(uint64_t(P.N) * P.N * 25);
		Sweeps += 25;
		if (Sweeps % 500 == 0) std::printf("  %d rodadas: %llu lajes, cobertura %.1f%%\n", Sweeps, (unsigned long long)S.TotalSlabs(), 100.0 * S.Cover(2));
	}
	const double Sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count();
	const std::vector<float> Hf = DuneSim::Finish(S, Sigma, VScale, Mode == 1 ? 31.0f : 30.0f);
	std::vector<int> Lab, Near;
	std::vector<float> Nd;
	const std::vector<DuneSim::FDune> D = DuneSim::Label(Hf, P.N, 0.05f, 60, 8, Lab, Near, Nd);
	std::vector<float> W, Hm;
	for (const auto& X : D) { Hm.push_back(X.HMax); W.push_back(2.0f * X.R); }
	std::sort(W.begin(), W.end()); std::sort(Hm.begin(), Hm.end());
	auto Pc = [](const std::vector<float>& V, float Q) { return V.empty() ? 0.0f : V[size_t(Q * (V.size() - 1))]; };
	size_t Cov = 0; for (float V : Hf) Cov += V > 0.2f;
	std::printf("N %d: %d rodadas em %.1f s; %zu dunas; diâmetro p10/50/90 %.0f/%.0f/%.0f m; altura p10/50/90/max %.2f/%.2f/%.2f/%.2f m; cobertura >20 cm %.1f%%\n",
		P.N, Sweeps, Sec, D.size(), Pc(W, 0.1f), Pc(W, 0.5f), Pc(W, 0.9f), Pc(Hm, 0.1f), Pc(Hm, 0.5f), Pc(Hm, 0.9f), Hm.empty() ? 0.0f : Hm.back(), 100.0 * Cov / Hf.size());
	FILE* F = std::fopen(argv[1], "wb");
	const int Nx = P.N, Ny = P.N;
	const float Box[4] = { 0.0f, 0.0f, P.N * P.Cell, P.N * P.Cell };
	std::fwrite(&Nx, 4, 1, F); std::fwrite(&Ny, 4, 1, F); std::fwrite(Box, 4, 4, F);
	std::vector<float> Row(Nx);
	for (int Y = 0; Y < Ny; ++Y)
	{
		for (int X = 0; X < Nx; ++X) Row[X] = Hf[size_t(Y) * Nx + X] * 100.0f;
		std::fwrite(Row.data(), 4, Nx, F);
	}
	std::fclose(F);
	return 0;
}
