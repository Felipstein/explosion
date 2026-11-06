#include "RTSCursors.h"

#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"

namespace
{
	const TCHAR* const CursorNames[] = {
		TEXT("pointer"), TEXT("select"),
		TEXT("scroll_n"), TEXT("scroll_ne"), TEXT("scroll_e"), TEXT("scroll_se"), TEXT("scroll_s"), TEXT("scroll_sw"), TEXT("scroll_w"), TEXT("scroll_nw"),
		TEXT("pan"), TEXT("invalid"), TEXT("move"), TEXT("queue"), TEXT("attack"), TEXT("cant"), TEXT("force"), TEXT("ground"), TEXT("amove"),
		TEXT("guard"), TEXT("escort"), TEXT("patrol"), TEXT("enter"), TEXT("rally"), TEXT("build"), TEXT("nobuild"),
		TEXT("repair"), TEXT("resume"), TEXT("salvage"), TEXT("gather"), TEXT("sell"), TEXT("strike"), TEXT("support"),
		TEXT("rotate_00") };
	static_assert(UE_ARRAY_COUNT(CursorNames) == int32(ERTSCursor::Count), "um nome de arquivo pra cada cursor");

	// (01/11, aprovado) as ordens que entram na fila do Alt: cada uma tem a ficha com o número da vez (q_<ordem>_<1..9|plus>)
	const ERTSCursor QueueOrders[] = { ERTSCursor::Move, ERTSCursor::AttackMove, ERTSCursor::Attack, ERTSCursor::Force, ERTSCursor::Ground,
		ERTSCursor::Guard, ERTSCursor::Escort, ERTSCursor::Patrol, ERTSCursor::Enter, ERTSCursor::Repair, ERTSCursor::Resume, ERTSCursor::Salvage,
		ERTSCursor::Gather, ERTSCursor::Build };
	static_assert(UE_ARRAY_COUNT(QueueOrders) == FRTSCursors::QueueOrderCount, "uma linha de fichas pra cada ordem da fila");
	int32 QueueSlot(ERTSCursor C)
	{
		for (int32 K = 0; K < UE_ARRAY_COUNT(QueueOrders); ++K)
		{
			if (QueueOrders[K] == C)
			{
				return K;
			}
		}
		return -1;
	}

	// sem os arquivos: os do Windows de antes (a mira, a cruz de andar, a mão)
	EMouseCursor::Type Fallback(ERTSCursor C)
	{
		switch (C)
		{
		case ERTSCursor::Select: case ERTSCursor::Repair: case ERTSCursor::Resume: case ERTSCursor::Salvage: case ERTSCursor::Gather:
		case ERTSCursor::Sell: case ERTSCursor::Enter:
			return EMouseCursor::Hand;
		case ERTSCursor::Attack: case ERTSCursor::Force: case ERTSCursor::Ground: case ERTSCursor::AttackMove:
		case ERTSCursor::Strike: case ERTSCursor::Support:
			return EMouseCursor::Crosshairs;
		case ERTSCursor::Move: case ERTSCursor::Queue: case ERTSCursor::Cant: case ERTSCursor::Guard: case ERTSCursor::Escort:
		case ERTSCursor::Patrol: case ERTSCursor::Rally: case ERTSCursor::Pan:
			return EMouseCursor::CardinalCross;
		case ERTSCursor::Invalid: case ERTSCursor::NoBuild:
			return EMouseCursor::SlashedCircle;
		default:
			return EMouseCursor::Default;
		}
	}
}

bool FRTSCursors::IsQueueable(ERTSCursor C)
{
	return QueueSlot(C) >= 0;
}

const TCHAR* FRTSCursors::Name(ERTSCursor C)
{
	return C < ERTSCursor::Count ? CursorNames[int32(C)] : TEXT("?");
}

bool FRTSCursors::Load(int32 Px)
{
	const TSharedPtr<ICursor> Platform = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetPlatformCursor() : nullptr;
	if (!Platform.IsValid())
	{
		return false;
	}
	const FString Dir = FPaths::ProjectContentDir() / TEXT("Cursors") / FString::FromInt(Px);
	void* Loaded[int32(ERTSCursor::Count)] = {};
	for (int32 K = 0; K < int32(ERTSCursor::Count); ++K)
	{
		// o Windows procura o .ani e depois o .cur; o ponto do clique vem dentro do arquivo
		Loaded[K] = Platform->CreateCursorFromFile(Dir / CursorNames[K], FVector2D::ZeroVector);
		if (!Loaded[K])
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS: o cursor %s não carregou de %s (ficam os do Windows)"), CursorNames[K], *Dir);
			return false;
		}
	}
	FMemory::Memcpy(Handles, Loaded, sizeof(Handles));
	// as fichas da fila são opcionais: sem o arquivo, a ordem fica sem a ficha
	int32 QueueFiles = 0;
	for (int32 Q = 0; Q < QueueOrderCount; ++Q)
	{
		for (int32 N = 0; N < QueueNumbers; ++N)
		{
			const FString File = FString::Printf(TEXT("q_%s_%s"), CursorNames[int32(QueueOrders[Q])], N < 9 ? *FString::FromInt(N + 1) : TEXT("plus"));
			QueueHandles[Q][N] = Platform->CreateCursorFromFile(Dir / File, FVector2D::ZeroVector);
			QueueFiles += QueueHandles[Q][N] ? 1 : 0;
		}
	}
	// (04/11) o girar: as 24 posições da volta (a 1ª já veio acima, como o cursor Rotate)
	for (int32 K = 0; K < RotateSteps; ++K)
	{
		RotateHandles[K] = Platform->CreateCursorFromFile(Dir / FString::Printf(TEXT("rotate_%02d"), K), FVector2D::ZeroVector);
	}
	LoadedPx = Px;
	Shown = ERTSCursor::Count;
	ShownRotate = -1;
	ShownQueue = 0;
	Platform->SetTypeShape(EMouseCursor::Default, Handles[int32(ERTSCursor::Pointer)]);
	UE_LOG(LogTemp, Log, TEXT("RTS: os cursores prontos (%d px, %d arquivos e %d fichas da fila)"), Px, int32(ERTSCursor::Count), QueueFiles);
	return true;
}

void FRTSCursors::Invalidate()
{
	Shown = ERTSCursor::Count;
	ShownQueue = 0;
	ShownRotate = -1;
	const TSharedPtr<ICursor> Platform = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetPlatformCursor() : nullptr;
	if (LoadedPx > 0 && Platform.IsValid())
	{
		Platform->SetTypeShape(EMouseCursor::Default, Handles[int32(ERTSCursor::Pointer)]);
	}
}

EMouseCursor::Type FRTSCursors::Apply(ERTSCursor C, int32 ScreenHeight, int32 QueueN, float RotateDeg)
{
	const int32 Px = ScreenHeight > 1800 ? 96 : (ScreenHeight > 1200 ? 64 : 48);
	if (Px != LoadedPx && Px != FailedPx && !Load(Px))
	{
		FailedPx = Px;
	}
	if (LoadedPx == 0)
	{
		return Fallback(C);
	}
	// a fila do Alt: a ficha com o número da vez (1 a 9; 10 ou mais = "+")
	const int32 Slot = QueueN > 0 ? QueueSlot(C) : -1;
	void* QueueHandle = Slot >= 0 ? QueueHandles[Slot][FMath::Clamp(QueueN, 1, QueueNumbers) - 1] : nullptr;
	const int32 Queue = QueueHandle ? QueueN : 0;
	// girar a obra: a volta na posição mais perto da direção do arraste
	const int32 Turn = C == ERTSCursor::Rotate ? (FMath::RoundToInt32(FMath::Fmod(RotateDeg + 360.0f, 360.0f) / (360.0f / RotateSteps)) % RotateSteps) : -1;
	void* RotateHandle = Turn >= 0 ? RotateHandles[Turn] : nullptr;
	if (C != Shown || Queue != ShownQueue || Turn != ShownRotate)
	{
		Shown = C;
		ShownQueue = Queue;
		ShownRotate = Turn;
		if (C != ERTSCursor::Pointer)
		{
			if (const TSharedPtr<ICursor> Platform = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetPlatformCursor() : nullptr)
			{
				Platform->SetTypeShape(EMouseCursor::Custom, QueueHandle ? QueueHandle : (RotateHandle ? RotateHandle : Handles[int32(C)]));
			}
		}
	}
	return C == ERTSCursor::Pointer ? EMouseCursor::Default : EMouseCursor::Custom;
}
