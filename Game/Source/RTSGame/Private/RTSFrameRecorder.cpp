// (17/10) A gravação dos testes sem travar a placa: o porquê e os números em RTSFrameRecorder.h.
#include "RTSFrameRecorder.h"

#include "Async/Async.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformProcess.h"
#include "HAL/ThreadSafeCounter.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "RHICommandList.h"
#include "RHIGPUReadback.h"
#include "Rendering/SlateRenderer.h"
#include "RenderingThread.h"
#include "Slate/SlateViewportProvider.h"
#include "Widgets/SWindow.h"

namespace
{
	// as áreas de leitura em voo (como o FFrameGrabber: 3–4). Só a thread de render mexe nelas
	struct FSlot
	{
		TUniquePtr<FRHIGPUTextureReadback> Readback;
		FString Path;
		FIntPoint Size = FIntPoint::ZeroValue;
		EPixelFormat Format = PF_Unknown;
		uint64 Order = 0;
		bool bBusy = false;
	};
	constexpr int32 RecNumSlots = 4;
	FSlot GRecSlots[RecNumSlots];
	TArray<FString> GRecPending;                   // os pedidos que ainda não pegaram uma tela (thread de render)
	uint64 GRecNextOrder = 0;
	FDelegateHandle GRecPresentHandle;
	const SWindow* GRecWindow = nullptr;       // a janela do jogo (posta no game thread antes de ligar)
	IImageWrapperModule* GRecImageModule = nullptr;
	FThreadSafeCounter GRecOutstanding;       // pedidos ainda não salvos
	bool bRecActive = false;
	bool bRecWarnedFormat = false;

	// o JPG nos núcleos livres (a 1600×900, ~10–20 ms cada)
	void Save(TArray<FColor>&& Px, int32 W, int32 H, FString&& Path)
	{
		Async(EAsyncExecution::ThreadPool, [Px = MoveTemp(Px), W, H, Path = MoveTemp(Path)]()
		{
			TSharedPtr<IImageWrapper> Wr = GRecImageModule ? GRecImageModule->CreateImageWrapper(EImageFormat::JPEG) : nullptr;
			if (Wr && Wr->SetRaw(Px.GetData(), int64(Px.Num()) * sizeof(FColor), W, H, ERGBFormat::BGRA, 8))
			{
				const TArray64<uint8> Data = Wr->GetCompressed(92);
				FFileHelper::SaveArrayToFile(Data, *Path);
			}
			GRecOutstanding.Decrement();
		});
	}

	// a leitura pronta → BGRA de 8 bits (a tela é B8G8R8A8 no comum; R8G8B8A8 e a de 10 bits por garantia)
	void Drain(FSlot& S)
	{
		int32 Pitch = 0;
		int32 BufH = 0;
		const uint8* Src = static_cast<const uint8*>(S.Readback->Lock(Pitch, &BufH));
		TArray<FColor> Px;
		const int32 W = S.Size.X;
		const int32 H = FMath::Min(S.Size.Y, BufH > 0 ? BufH : S.Size.Y);
		if (Src && W > 0 && H > 0 && Pitch >= W)
		{
			Px.SetNumUninitialized(W * H);
			for (int32 Y = 0; Y < H; ++Y)
			{
				const uint8* Row = Src + size_t(Y) * size_t(Pitch) * 4;
				FColor* Out = &Px[Y * W];
				if (S.Format == PF_B8G8R8A8)
				{
					FMemory::Memcpy(Out, Row, size_t(W) * 4);
					for (int32 X = 0; X < W; ++X)
					{
						Out[X].A = 255;
					}
				}
				else if (S.Format == PF_R8G8B8A8)
				{
					for (int32 X = 0; X < W; ++X)
					{
						Out[X] = FColor(Row[4 * X + 0], Row[4 * X + 1], Row[4 * X + 2], 255);
					}
				}
				else
				{
					// A2B10G10R10 (a tela de 10 bits): R nos bits 0–9, G 10–19, B 20–29
					const uint32* Row32 = reinterpret_cast<const uint32*>(Row);
					for (int32 X = 0; X < W; ++X)
					{
						const uint32 V = Row32[X];
						Out[X] = FColor(uint8((V & 1023u) >> 2), uint8(((V >> 10) & 1023u) >> 2), uint8(((V >> 20) & 1023u) >> 2), 255);
					}
				}
			}
		}
		S.Readback->Unlock();
		S.bBusy = false;
		if (Px.Num() > 0)
		{
			Save(MoveTemp(Px), W, H, MoveTemp(S.Path));
		}
		else
		{
			GRecOutstanding.Decrement();
		}
	}

	FSlot* Oldest()
	{
		FSlot* Best = nullptr;
		for (FSlot& S : GRecSlots)
		{
			if (S.bBusy && (!Best || S.Order < Best->Order))
			{
				Best = &S;
			}
		}
		return Best;
	}

	void WaitAndDrain(FSlot& S, FRHICommandListImmediate& RHICmdList, double MaxS)
	{
		const double Until = FPlatformTime::Seconds() + MaxS;
		while (!S.Readback->IsReady() && FPlatformTime::Seconds() < Until)
		{
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			FPlatformProcess::SleepNoStats(0.0005f);
		}
		if (S.Readback->IsReady())
		{
			Drain(S);
		}
		else
		{
			S.bBusy = false;                      // não ficou pronta a tempo: perde esse quadro
			GRecOutstanding.Decrement();
		}
	}

	// a thread de render, logo antes da janela ser apresentada
	void OnPresent(SWindow& Window, ISlateViewportProvider& Provider)
	{
		if (&Window != GRecWindow)
		{
			return;
		}
		// as leituras que já chegaram, na ordem
		while (FSlot* S = Oldest())
		{
			if (!S->Readback->IsReady())
			{
				break;
			}
			Drain(*S);
		}
		if (GRecPending.IsEmpty())
		{
			return;
		}
		FRHITexture* Back = Provider.GetBackBufferResource();
		FRHICommandListImmediate& RHICmdList = FRHICommandListImmediate::Get();
		if (!Back)
		{
			return;
		}
		const EPixelFormat Fmt = Back->GetFormat();
		if (Fmt != PF_B8G8R8A8 && Fmt != PF_R8G8B8A8 && Fmt != PF_A2B10G10R10)
		{
			if (!bRecWarnedFormat)
			{
				bRecWarnedFormat = true;
				UE_LOG(LogTemp, Warning, TEXT("RTS gravação: a tela tem o formato %d, que não sei ler; os quadros saem vazios"), int32(Fmt));
			}
			GRecOutstanding.Subtract(GRecPending.Num());
			GRecPending.Reset();
			return;
		}
		FSlot* Free = nullptr;
		for (FSlot& S : GRecSlots)
		{
			if (!S.bBusy)
			{
				Free = &S;
				break;
			}
		}
		if (!Free)
		{
			Free = Oldest();                      // as 4 em voo: espera a mais velha (o BlockUntilAvailable do FFrameGrabber)
			WaitAndDrain(*Free, RHICmdList, 1.0);
		}
		if (!Free->Readback)
		{
			Free->Readback = MakeUnique<FRHIGPUTextureReadback>(TEXT("RTSFrameRecorder"));
		}
		// a tela chega pronta pra apresentar (SetTextureAccessFinal Present, SlateRHIRenderer.cpp): copia e devolve
		RHICmdList.Transition(FRHITransitionInfo(Back, ERHIAccess::Present, ERHIAccess::CopySrc));
		Free->Readback->EnqueueCopy(RHICmdList, Back);
		RHICmdList.Transition(FRHITransitionInfo(Back, ERHIAccess::CopySrc, ERHIAccess::Present));
		Free->Path = GRecPending[0];
		GRecPending.RemoveAt(0);
		Free->Size = FIntPoint(int32(Back->GetSizeX()), int32(Back->GetSizeY()));
		Free->Format = Fmt;
		Free->Order = GRecNextOrder++;
		Free->bBusy = true;
	}
}

namespace
{
	TAutoConsoleVariable<int32> CVarRecordFast(TEXT("rts.Record.Fast"), 0,
		TEXT("(17/10, desligada até a prova) 1 = grava o -RTSRecord sem travar a placa (a tela copiada na apresentação, JPG nos núcleos livres); 0 = o print PNG de cada quadro, de antes."));
}

namespace RTSFrameRecorder
{
	bool WantFast()
	{
		return CVarRecordFast.GetValueOnGameThread() != 0;
	}

	bool Start()
	{
		if (bRecActive)
		{
			return true;
		}
		if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer() || !GEngine || !GEngine->GameViewport)
		{
			return false;
		}
		const TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
		if (!Window.IsValid())
		{
			return false;
		}
		GRecImageModule = &FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		GRecWindow = Window.Get();
		FlushRenderingCommands();
		GRecPresentHandle = FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddStatic(&OnPresent);
		bRecActive = true;
		UE_LOG(LogTemp, Log, TEXT("RTS gravação: ligada sem travar a placa (a tela copiada na apresentação, %d em voo, JPG nos núcleos livres)"), RecNumSlots);
		return true;
	}

	bool IsActive()
	{
		return bRecActive;
	}

	void Capture(const FString& JpgPath)
	{
		if (!bRecActive)
		{
			return;
		}
		GRecOutstanding.Increment();
		// na ordem dos comandos de render: a apresentação deste quadro vem depois e pega o pedido
		ENQUEUE_RENDER_COMMAND(RTSFrameRecorderCapture)([Path = JpgPath](FRHICommandListImmediate&)
		{
			GRecPending.Add(Path);
		});
	}

	int32 Outstanding()
	{
		return GRecOutstanding.GetValue();
	}

	void Stop(float MaxWaitS)
	{
		if (!bRecActive)
		{
			return;
		}
		bRecActive = false;
		if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
		{
			FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().Remove(GRecPresentHandle);
		}
		GRecPresentHandle.Reset();
		ENQUEUE_RENDER_COMMAND(RTSFrameRecorderStop)([](FRHICommandListImmediate& RHICmdList)
		{
			while (FSlot* S = Oldest())
			{
				WaitAndDrain(*S, RHICmdList, 2.0);
			}
			GRecOutstanding.Subtract(GRecPending.Num());   // pedidos que nem chegaram a uma tela
			GRecPending.Reset();
			for (FSlot& S : GRecSlots)
			{
				S.Readback.Reset();                    // solta as áreas da placa aqui (no fim do jogo a RHI já fechou)
			}
		});
		FlushRenderingCommands();
		const double Until = FPlatformTime::Seconds() + MaxWaitS;
		while (GRecOutstanding.GetValue() > 0 && FPlatformTime::Seconds() < Until)
		{
			FPlatformProcess::SleepNoStats(0.01f);
		}
		UE_LOG(LogTemp, Log, TEXT("RTS gravação: desligada (%d quadros sem salvar)"), FMath::Max(GRecOutstanding.GetValue(), 0));
		GRecWindow = nullptr;
	}
}
