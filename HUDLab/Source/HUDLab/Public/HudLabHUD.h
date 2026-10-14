#pragma once

// O HUD do laboratório: monta um dos 6 estados de teste (os 5 da planta e o 1 com várias superarmas) (HudLabStates) e entrega pro
// FRTSHudPainter, por cima da cena aprovada. As imagens vêm direto dos arquivos do kit
// (docs/concept/hud/kit/) e de HUDLab/LabAssets/ (o fundo, o minimapa e os emblemas em PNG).
//
// Linha de comando: -State=1..6 (o estado), -Shot=<arquivo.png> (tira a foto e fecha),
// -Faction=china|gla (a pele da facção), -Seq=N (N quadros seguidos pro vídeo), -Time=t, -Cols=9 (a grade da planta, 2 × 9; o padrão é 2 × 7, com as casas do tamanho do clássico).
// Na janela: teclas 1 a 6 trocam o estado.

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RTSHudTypes.h"
#include "HudLabHUD.generated.h"

class FRTSHudKit;
class FRTSHudPainter;
class UTexture2D;

UCLASS()
class AHudLabHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

private:
	UTexture2D* GetTexture(FName Name);

	TSharedPtr<FRTSHudKit> Kit;

	TSharedPtr<FRTSHudPainter> Painter;
	TArray<FRTSHudButton> LastButtons;
	FString RepoRoot;
	int32 StateIndex = 1;
	int32 MainColumns = 7; // -Cols=9: a grade da planta (2 × 9), pra comparar com a de 7 (decisão de 13/10)
	FString ShotPath;
	int32 Frames = 0;
	bool bShotRequested = false;
	int32 SeqFrames = 0;   // -Seq=N: N fotos seguidas (o vídeo), o tempo andando 1/30 s por foto
	int32 SeqIndex = 0;
	int32 SeqWait = 0;
	float SimTime = 0.0f;
	FName SkinFaction;     // -Faction=china|gla: a pele da facção
	double DrawMs = 0.0;   // o custo do HUD: a soma do tempo de CPU do Draw (vai pro log ao fechar a foto)
	int32 DrawCount = 0;
	bool bMeasure = false; // -Measure: a foto espera 300 quadros, pra média do custo sair sem a leitura das imagens  // -Time=t: o tempo das pulsações numa foto
};
