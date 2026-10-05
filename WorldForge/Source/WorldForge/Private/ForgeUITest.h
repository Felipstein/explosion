#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class AWorldForgeGameMode;

/**
 * Teste da interface do jeito que o Felipe usa: cliques, arrastes, teclado e
 * roda do mouse sintéticos entregues ao Slate (o mesmo caminho do mouse de
 * verdade, sem mexer no cursor do Windows). Cada passo age, espera o mapa
 * novo aparecer, fotografa a tela com o painel e confere o resultado no log
 * ("FORGE UI ok/FALHOU ...").
 *
 * Linha de comando: -ForgeCapture=<pasta> -ForgeUITest
 */
class FForgeUITest
{
public:
	FForgeUITest(AWorldForgeGameMode* InGameMode, const FString& InFolder);

	/** Avança o teste; devolve true quando termina. */
	bool Tick(float DeltaSeconds);

private:
	struct FStep
	{
		FString Name;
		TFunction<void()> Act;
		TFunction<bool()> Ready;          // pronto pra fotografar e conferir
		TFunction<FString()> Check;       // vazio = ok; senão o motivo da falha
		float MinWait = 1.5f;
		// arraste em vários quadros (o chão e a câmera seguem o mouse)
		FKey DragButton = EKeys::Invalid;
		FVector2D DragFrom = FVector2D::ZeroVector, DragTo = FVector2D::ZeroVector;   // frações da tela
		float DragTime = 0.4f;
		FKey SecondClick = EKeys::Invalid;   // duplo clique: o 2º clique sai 0.15 s depois
	};

	void Build();
	FVector2D Target(const FString& Id, FVector2D Frac = FVector2D(0.5, 0.5)) const;
	FVector2D ViewportPoint(FVector2D Frac) const;
	/** Ponto na faixa do controle deslizante que corresponde ao valor (0–1). */
	FVector2D SliderPoint(const FString& Id, float Value, float YFrac) const;
	void Move(FVector2D Pos);
	void Down(FVector2D Pos, FKey Button);
	void Up(FVector2D Pos, FKey Button);
	void Click(FVector2D Pos, FKey Button = EKeys::LeftMouseButton) { Move(Pos); Down(Pos, Button); Up(Pos, Button); }
	void Wheel(FVector2D Pos, float Delta);
	void Key(FKey K, bool bDown);
	bool NewMapSince(int32 Ticket) const;
	bool RealCursorOverWindow() const;

	TWeakObjectPtr<AWorldForgeGameMode> GameMode;
	FString Folder;
	TArray<FStep> Steps;
	int32 Index = 0;
	int32 Phase = 0;
	float Timer = 0.0f;
	FVector2D LastPos = FVector2D::ZeroVector;
	TSet<FKey> Pressed;
	int32 Failures = 0;
	bool bDragStarted = false;

	// estado guardado antes de cada passo, pra conferir depois
	int32 TicketBefore = 0;
	int32 SeedBefore = 0;
	FVector2f FocusBefore = FVector2f::ZeroVector;
	float DistanceBefore = 0.0f;
	float WaterFractionBefore = 0.0f;
	float YawBefore = 0.0f;
};
