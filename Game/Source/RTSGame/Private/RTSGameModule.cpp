// O módulo do jogo. Registra a tela de carregamento: enquanto um mapa abre, a
// Unreal desenha esta tela numa thread própria (MoviePlayer), em vez de a janela
// congelar.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "MoviePlayer.h"
#include "Misc/CommandLine.h"
#include "RTSShell.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// (06/11, Telas) o widget velho do carregar ("LOADING" com a rodinha) saiu: a tela é a aprovada, do plugin RTSShell (SRTSMatch)

class FRTSGameModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		if (IsRunningCommandlet() || IsRunningDedicatedServer())
		{
			return;
		}
		PreLoadHandle = FCoreUObjectDelegates::PreLoadMap.AddLambda([](const FString& Map)
		{
			if (!GetMoviePlayer()->IsInitialized())
			{
				return;
			}
			// (04/11, Telas) o mapa do menu (FrontEnd, vazio) abre em instantes: o escuro da abertura, sem o "LOADING" (reprovado)
			const bool bFrontEnd = Map.Contains(TEXT("FrontEnd"));
			FLoadingScreenAttributes Screen;
			Screen.bAutoCompleteWhenLoadingCompletes = true;
			Screen.MinimumLoadingScreenDisplayTime = bFrontEnd ? 0.0f : 0.4f;
			if (bFrontEnd)
			{
				TSharedRef<SBorder> Dark = SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.003f, 0.004f, 0.0033f, 1.0f));
				Screen.WidgetLoadingScreen = Dark;
			}
			else
			{
				// (06/11, Telas) a tela de carregar aprovada (o mockup screens.html, a "load"; o plugin RTSShell, SRTSMatch), em todo modo
				Screen.WidgetLoadingScreen = FRTSShell::MakeLoadingScreen();
			}
			GetMoviePlayer()->SetupLoadingScreen(Screen);
		});
	}

	virtual void ShutdownModule() override
	{
		FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadHandle);
	}

private:
	FDelegateHandle PreLoadHandle;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FRTSGameModule, RTSGame, "RTSGame");
