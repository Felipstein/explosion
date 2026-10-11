// O módulo do jogo. Registra a tela de carregamento: enquanto um mapa abre, a
// Unreal desenha esta tela numa thread própria (MoviePlayer), em vez de a janela
// congelar.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "MoviePlayer.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	TSharedRef<SWidget> MakeLoadingWidget()
	{
		return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.012f, 0.014f, 0.017f, 1.0f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 18)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("RTSHUD", "Loading", "LOADING"))   // o jogo é em inglês por padrão (09/10)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 26))
					.ColorAndOpacity(FLinearColor(0.92f, 0.93f, 0.95f))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SThrobber)
				]
			];
	}
}

class FRTSGameModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		if (IsRunningCommandlet() || IsRunningDedicatedServer())
		{
			return;
		}
		PreLoadHandle = FCoreUObjectDelegates::PreLoadMap.AddLambda([](const FString&)
		{
			if (!GetMoviePlayer()->IsInitialized())
			{
				return;
			}
			FLoadingScreenAttributes Screen;
			Screen.bAutoCompleteWhenLoadingCompletes = true;
			Screen.MinimumLoadingScreenDisplayTime = 0.4f;
			Screen.WidgetLoadingScreen = MakeLoadingWidget();
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
