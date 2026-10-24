// (17/10) A caixa de digitar do chat da partida: o porquê em RTSChat.h.
#include "RTSChat.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "RTSMatchSubsystem.h"
#include "RTSNetSession.h"
#include "RTSPlayerController.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCompoundWidget.h"

namespace
{
	constexpr int32 ChatMaxChars = 120;

	// Em volta da caixa de texto: a tecla que ela não usou para aqui. A Slate devolve a tecla pros pais, e o pai da caixa
	// é a tela do jogo: sem isto o F4, o Ctrl+número dos grupos e as setas (a câmera) agiam no jogo enquanto se digita
	class SRTSChatSink : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SRTSChatSink) {}
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			ChildSlot[Args._Content.Widget];
		}

		virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override
		{
			return FReply::Handled();
		}
	};

	// As teclas do chat passam por aqui antes de tudo (o Enter que abre, e com a caixa aberta o Enter, o Esc e o Tab). As
	// letras vão pra uma caixa de texto de verdade da Slate (SEditableText), invisível, com o foco enquanto o chat está
	// aberto: ela recebe o texto com acento (as teclas mortas, o ç) e, com o foco fora do jogo, os atalhos ficam quietos.
	// Quem desenha é o HUD (ARTSHUD::DrawChat), lendo o rascunho daqui
	class FRTSChatInput : public IInputProcessor
	{
	public:
		TWeakObjectPtr<ARTSPlayerController> Owner;
		bool bOpen = false;
		bool bTeam = false;
		bool bCloseSoon = false;                // a caixa perdeu o foco (um clique no jogo): fecha no próximo quadro
		FString Text;
		double OpenTime = 0.0;
		TSharedPtr<SWidget> Box;
		TSharedPtr<SEditableText> Edit;

		virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override
		{
			if (bOpen && (bCloseSoon || !InMatch()))
			{
				Close();
			}
		}

		virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& E) override
		{
			const FKey K = E.GetKey();
			if (!bOpen)
			{
				if (K == EKeys::Enter && InMatch())
				{
					Open(E.IsShiftDown() || E.IsControlDown());
					return true;
				}
				return false;
			}
			if (K == EKeys::Enter)
			{
				ARTSPlayerController* PC = Owner.Get();
				URTSNetSession* Net = PC && PC->GetGameInstance() ? PC->GetGameInstance()->GetSubsystem<URTSNetSession>() : nullptr;
				if (Net && !Text.TrimStartAndEnd().IsEmpty())
				{
					Net->SendChat(Text.Left(ChatMaxChars), bTeam);
				}
				Close();
				return true;
			}
			if (K == EKeys::Escape)
			{
				Close();
				return true;
			}
			if (K == EKeys::Tab)
			{
				bTeam = !bTeam;
				return true;
			}
			return false;                       // o resto vai pra caixa de texto
		}

		virtual const TCHAR* GetDebugName() const override { return TEXT("RTSChatInput"); }

		void Open(bool bAllies)
		{
			if (!GEngine || !GEngine->GameViewport)
			{
				return;
			}
			bOpen = true;
			bTeam = bAllies;
			bCloseSoon = false;
			Text.Reset();
			OpenTime = FPlatformTime::Seconds();
			SAssignNew(Edit, SEditableText)
				.OnTextChanged_Lambda([this](const FText& T) { Text = T.ToString().Left(ChatMaxChars); })
				.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type Kind)
				{
					if (Kind == ETextCommit::OnUserMovedFocus)
					{
						bCloseSoon = true;      // não tira a caixa de dentro do aviso dela mesma
					}
				});
			Box = SNew(SRTSChatSink).RenderOpacity(0.0f)
				[
					SNew(SBox).WidthOverride(400.0f).HeightOverride(30.0f)
					[
						Edit.ToSharedRef()
					]
				];
			GEngine->GameViewport->AddViewportWidgetContent(Box.ToSharedRef(), 1000);
			FSlateApplication::Get().SetKeyboardFocus(Edit, EFocusCause::SetDirectly);
		}

		void Close()
		{
			bOpen = false;
			bCloseSoon = false;
			Text.Reset();
			if (Box.IsValid() && GEngine && GEngine->GameViewport)
			{
				GEngine->GameViewport->RemoveViewportWidgetContent(Box.ToSharedRef());
			}
			Box.Reset();
			Edit.Reset();
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().SetAllUserFocusToGameViewport();
			}
		}

	private:
		bool InMatch() const
		{
			const ARTSPlayerController* PC = Owner.Get();
			if (!PC || PC->IsMenu() || PC->IsPauseMenuOpen() || !PC->GetWorld())
			{
				return false;
			}
			const URTSMatchSubsystem* M = PC->GetWorld()->GetSubsystem<URTSMatchSubsystem>();
			return M && M->IsRunning() && M->Result() == ERTSMatchResult::Playing;
		}
	};

	TSharedPtr<FRTSChatInput> GChatInput;
	const FString GEmpty;
}

namespace RTSChat
{
	void SimulateTyping(const FString& Text)
	{
		// (o teste da LAN) as letras como o teclado manda: a tecla e o caractere, pela Slate (o mesmo caminho do Felipe)
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}
		// (as letras sem acento descem também como tecla: o A e o S são ordens no jogo, e com a caixa aberta não podem chegar lá)
		FSlateApplication& App = FSlateApplication::Get();
		const uint32 User = App.GetUserIndexForKeyboard();
		for (const TCHAR C : Text)
		{
			const TCHAR Up = FChar::ToUpper(C);
			const FKey Key = C == TEXT(' ') ? EKeys::SpaceBar : (Up >= TEXT('A') && Up <= TEXT('Z') ? FKey(FName(*FString::Chr(Up))) : EKeys::Invalid);
			const FKeyEvent KeyEvent(Key, FModifierKeysState(), User, false, uint32(C), 0);
			if (Key.IsValid())
			{
				App.ProcessKeyDownEvent(KeyEvent);
			}
			App.ProcessKeyCharEvent(FCharacterEvent(C, FModifierKeysState(), User, false));
			if (Key.IsValid())
			{
				App.ProcessKeyUpEvent(KeyEvent);
			}
		}
	}
}

namespace RTSChat
{
	void Attach(ARTSPlayerController* PC)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}
		if (!GChatInput.IsValid())
		{
			GChatInput = MakeShared<FRTSChatInput>();
			FSlateApplication::Get().RegisterInputPreProcessor(GChatInput, 0);
		}
		if (GChatInput->bOpen)
		{
			GChatInput->Close();
		}
		GChatInput->Owner = PC;
	}

	bool IsTyping()
	{
		return GChatInput.IsValid() && GChatInput->bOpen;
	}

	bool IsTeam()
	{
		return GChatInput.IsValid() && GChatInput->bTeam;
	}

	const FString& Draft()
	{
		return GChatInput.IsValid() ? GChatInput->Text : GEmpty;
	}

	double OpenedAt()
	{
		return GChatInput.IsValid() ? GChatInput->OpenTime : 0.0;
	}
}
