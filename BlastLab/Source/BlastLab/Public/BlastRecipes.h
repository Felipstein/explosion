#pragma once

#include "CoreMinimal.h"

/** Faixa sorteada [Min, Max]. */
struct FBlastRange
{
	float Min = 0.0f;
	float Max = 0.0f;
	float Sample(FRandomStream& R) const { return R.FRandRange(Min, Max); }
};

/**
 * A receita de camadas de uma arma (docs/EXPLOSOES.md): o que acontece em volta
 * da explosão gravada, com tempos em ms e tamanhos em m. Vem de
 * Config/BlastRecipes.json, que o simulador relê quando o arquivo muda: dá pra
 * ajustar e detonar de novo sem reabrir.
 */
struct FBlastRecipe
{
	// clarão: bola branca rápida no ponto da detonação (2–3 quadros do ZH a 30 qps)
	float FlashMs = 80.0f;
	float FlashRadiusM = 2.0f;
	float FlashRadiance = 30.0f;          // unidade do render (sol = 1; o bloom começa em 1.7)
	// anel de poeira deitado no chão até o raio do dano (comunica a área: "peso visual = peso no jogo")
	float RingDelayMs = 20.0f;
	float RingMs = 500.0f;
	float RingRadiusM = 12.5f;
	float RingOpacity = 0.6f;
	// destroços (sucata e pedra escura), torrões do chão e faíscas
	int32 Debris = 12;
	FBlastRange DebrisSpeed{ 15.0f, 40.0f };
	FBlastRange DebrisElevDeg{ 25.0f, 75.0f };
	FBlastRange DebrisSizeM{ 0.25f, 0.7f };
	int32 Clods = 40;
	FBlastRange ClodSpeed{ 6.0f, 22.0f };
	FBlastRange ClodElevDeg{ 40.0f, 80.0f };
	FBlastRange ClodSizeM{ 0.12f, 0.35f };
	int32 Sparks = 50;
	FBlastRange SparkSpeed{ 25.0f, 70.0f };
	FBlastRange SparkElevDeg{ 5.0f, 70.0f };
	FBlastRange SparkLifeMs{ 250.0f, 900.0f };
	float TerminalSpeed = 30.0f;          // arrasto quadrático: velocidade terminal típica dos pedaços (m/s)
	float RestS = 20.0f;                  // pedaços no chão antes de afundar
	float MinPixels = 3.0f;               // tamanho mínimo na tela (como o traçante)
	// marca queimada com riscos radiais
	float ScorchRadiusM = 12.5f;
	float ScorchMs = 200.0f;              // tempo pra marca crescer até o raio
	float ScorchLifeS = 90.0f;
};

/** O que muda com o chão (o material físico no ponto, Config/BlastRecipes.json "surfaces"). */
struct FBlastSurface
{
	FLinearColor ClodColor = FLinearColor(0.22f, 0.17f, 0.12f, 0.95f);   // rgb + rugosidade
	FLinearColor RingColor = FLinearColor(0.55f, 0.47f, 0.37f, 1.0f);
	FLinearColor ScorchColor = FLinearColor(0.03f, 0.028f, 0.025f, 1.0f);
	float Clods = 1.0f;                   // multiplica a quantidade de torrões
	float Ring = 1.0f;                    // multiplica a opacidade do anel
};

class BLASTLAB_API FBlastRecipes
{
public:
	/** A receita da arma (chave do arsenal: "granada", "tanque"...). */
	static const FBlastRecipe& Get(const FString& Weapon);
	/** O chão pelo nome do material físico (PM_Areia → "areia"); "terra" se não houver. */
	static const FBlastSurface& Surface(const FString& PhysicalMaterialName);
	/** Relê o arquivo se ele mudou (chamado pelo simulador). Devolve verdadeiro se releu. */
	static bool Poll();
	static FString FilePath();
};
