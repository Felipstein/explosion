#include "RTSShell.h"
#include "RTSShellBridge.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/StrongObjectPtr.h"
#include "WebBrowserModule.h"
#include "IWebBrowserSingleton.h"
#include "IWebBrowserSchemeHandler.h"
#include "IWebBrowserWindow.h"
#include "SWebBrowser.h"
#include "Containers/Ticker.h"
#include "SRTSMenu.h"
#include "SRTSSkirmish.h"
#include "SRTSMultiplayer.h"
#include "SRTSOptions.h"
#include "SRTSScript.h"
#include "SRTSMatch.h"
#include "Widgets/SViewport.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Misc/ScopeLock.h"
#include "RTSShellAudio.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"

namespace
{
	const TCHAR* const Origin = TEXT("https://rts.ui/");

	FRTSShellHandlers GHandlers;
	TSharedPtr<SWebBrowser> GBrowser;
	TStrongObjectPtr<URTSShellBridge> GBridge;
	bool GShown = false;
	// (03/11) o menu principal nativo (SRTSMenu); as outras telas seguem no navegador até a vez delas
	TSharedPtr<SRTSMenu> GMenu;
	bool GMenuShown = false;
	// (05/11) a sala do skirmish nativa (SRTSSkirmish); (06/11) a sala da LAN é a mesma, no modo da rede
	TSharedPtr<SRTSSkirmish> GSkirmish;
	bool GSkirmishShown = false;
	// (06/11) a lista de salas do multiplayer nativa
	TSharedPtr<SRTSMultiplayer> GMultiplayer;
	bool GMultiplayerShown = false;
	// (06/11) as opções nativas
	TSharedPtr<SRTSOptions> GOptions;
	bool GOptionsShown = false;
	// (06/11) o SCRIPT nativo (só no desenvolvimento)
	TSharedPtr<SRTSScript> GScript;
	bool GScriptShown = false;
	// (06/11) as telas da partida: o widget, a caixa com o borrão por cima do jogo, qual está, a largada (a thread do filme lê) e as
	// opções abertas do Esc (o BACK volta pro Esc, não pro menu)
	TSharedPtr<SRTSMatch> GMatch;
	TSharedPtr<SWidget> GMatchBox;
	ERTSMatchOverlay GOverlay = ERTSMatchOverlay::None;
	FRTSMatchIntro GIntro;
	FCriticalSection GIntroLock;
	bool GFirstShow = true;

	/** Tira da tela as telas nativas que não são a pedida. */
	void HideNative(const SWidget* Keep)
	{
		if (!GEngine || !GEngine->GameViewport) return;
		if (GMenuShown && GMenu.IsValid() && GMenu.Get() != Keep) { GEngine->GameViewport->RemoveViewportWidgetContent(GMenu.ToSharedRef()); GMenuShown = false; }
		if (GSkirmishShown && GSkirmish.IsValid() && GSkirmish.Get() != Keep) { GEngine->GameViewport->RemoveViewportWidgetContent(GSkirmish.ToSharedRef()); GSkirmishShown = false; }
		if (GMultiplayerShown && GMultiplayer.IsValid() && GMultiplayer.Get() != Keep)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GMultiplayer.ToSharedRef());
			GMultiplayerShown = false;
		}
		if (GOptionsShown && GOptions.IsValid() && GOptions.Get() != Keep)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GOptions.ToSharedRef());
			GOptionsShown = false;
		}
		if (GScriptShown && GScript.IsValid() && GScript.Get() != Keep)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GScript.ToSharedRef());
			GScriptShown = false;
		}
	}
	/** Um parâmetro da página (skirmish.html?mp=1&host=Felipe): o valor, com o + virando espaço. */
	FString PageArg(const FString& Page, const TCHAR* Key)
	{
		FString Query;
		if (!Page.Split(TEXT("?"), nullptr, &Query)) return FString();
		TArray<FString> Parts;
		Query.ParseIntoArray(Parts, TEXT("&"));
		for (const FString& P : Parts)
		{
			FString K, V;
			if (P.Split(TEXT("="), &K, &V) && K == Key) return V.Replace(TEXT("+"), TEXT(" "));
		}
		return FString();
	}
	// (04/11) a abertura: só na 1ª vez que o menu aparece no processo; os testes nunca passam por ela
	bool GBootDone = false;
	FString GLastPage;

	bool BootAllowed()
	{
		const TCHAR* Cmd = FCommandLine::Get();
		FString Unused;
		return !FParse::Param(Cmd, TEXT("RTSNoBoot")) && !FParse::Value(Cmd, TEXT("-RTSMenuShot="), Unused) && !FParse::Value(Cmd, TEXT("-RTSMenuT="), Unused)
			&& !FParse::Value(Cmd, TEXT("-RTSAutoTest="), Unused) && !FParse::Value(Cmd, TEXT("-RTSScenario="), Unused);
	}

	/** A 1ª vez neste PC (a abertura inteira); as outras, a curta. A marca fica no Saved do jogo. */
	bool BootFirstEver()
	{
		const FString Mark = FPaths::ProjectSavedDir() / TEXT("RTSShell") / TEXT("boot_seen");
		const bool bFirst = !IFileManager::Get().FileExists(*Mark);
		if (bFirst)
		{
			FFileHelper::SaveStringToFile(TEXT("04/11"), *Mark);
		}
		return bFirst;
	}

	/** A foto do menu nativo pro teste (-RTSMenuShot=<arquivo.png>): o menu desenhado numa textura de 1920 × 1080 (o palco do mockup,
	 *  sem escala) e salvo em PNG, pra comparar pixel a pixel com o mockup no Chrome. Não depende da janela (vale com -RenderOffScreen). */
	void ShotWidget(const TSharedPtr<SWidget>& Widget, const FString& Path, const TCHAR* Name)
	{
		if (!Widget.IsValid())
		{
			return;
		}
		// a textura linear (não sRGB), como a imagem da janela do Windows: o shader da Slate já corrige a gama, uma vez. O
		// CreateTargetFor(…, true) faz a textura sRGB e a cor saía corrigida duas vezes (a foto clara de 03/11; IsSRGB() =
		// !bForceLinearGamma em TextureRenderTarget2D.cpp)
		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>();
		Target->ClearColor = FLinearColor::Transparent;
		Target->InitCustomFormat(1920, 1080, PF_B8G8R8A8, true);
		Target->UpdateResourceImmediate(true);
		Target->AddToRoot();
		FWidgetRenderer* Renderer = new FWidgetRenderer(true, true);
		Renderer->DrawWidget(Target, Widget.ToSharedRef(), FVector2D(1920, 1080), 0.0f);
		FlushRenderingCommands();
		TArray<FColor> Pixels;
		if (FTextureRenderTargetResource* Res = Target->GameThread_GetRenderTargetResource())
		{
			Res->ReadPixels(Pixels);
		}
		delete Renderer;
		Target->RemoveFromRoot();
		if (Pixels.Num() == 1920 * 1080)
		{
			for (FColor& C : Pixels)
			{
				C.A = 255;
			}
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(1920, 1080, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
			FFileHelper::SaveArrayToFile(Png, *Path);
		}
		UE_LOG(LogTemp, Log, TEXT("RTS telas: foto do %s nativo em %s"), Name, *Path);
	}
	/** A abertura quadro a quadro (-RTSBootFrames=<pasta>): a inteira, com o passo fixo de 1/30 s, na textura linear da foto do menu;
	 *  o Tools/boot_video.py monta o vídeo com os sons nos instantes do log. */
	void RenderBootFrames(const FString& Dir)
	{
		if (!GMenu.IsValid())
		{
			return;
		}
		IFileManager::Get().MakeDirectory(*Dir, true);
		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>();
		Target->ClearColor = FLinearColor::Transparent;
		Target->InitCustomFormat(1920, 1080, PF_B8G8R8A8, true);
		Target->UpdateResourceImmediate(true);
		Target->AddToRoot();
		FWidgetRenderer* Renderer = new FWidgetRenderer(true, true);
		GMenu->StartBoot(true);
		const float Dt = 1.0f / 30.0f;
		const int32 N = FMath::CeilToInt((GMenu->BootRevealAt() + 2.4f) / Dt);
		TArray<FColor> Pixels;
		for (int32 K = 0; K < N; ++K)
		{
			GMenu->Tick(GMenu->GetCachedGeometry(), FApp::GetCurrentTime(), Dt);
			Renderer->DrawWidget(Target, GMenu.ToSharedRef(), FVector2D(1920, 1080), 0.0f);
			FlushRenderingCommands();
			if (FTextureRenderTargetResource* Res = Target->GameThread_GetRenderTargetResource())
			{
				Res->ReadPixels(Pixels);
			}
			if (Pixels.Num() == 1920 * 1080)
			{
				for (FColor& C : Pixels)
				{
					C.A = 255;
				}
				TArray64<uint8> Png;
				FImageUtils::PNGCompressImageArray(1920, 1080, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
				FFileHelper::SaveArrayToFile(Png, *(Dir / FString::Printf(TEXT("boot_%03d.png"), K)));
			}
		}
		delete Renderer;
		Target->RemoveFromRoot();
		UE_LOG(LogTemp, Log, TEXT("RTS telas: quadros da abertura: %d em %s, o golpe em %.2f s"), N, *Dir, GMenu->BootRevealAt());
	}
	FString GPendingPage;

	const TCHAR* MimeOf(const FString& Path)
	{
		const FString Ext = FPaths::GetExtension(Path).ToLower();
		if (Ext == TEXT("html")) return TEXT("text/html");
		if (Ext == TEXT("js")) return TEXT("text/javascript");
		if (Ext == TEXT("css")) return TEXT("text/css");
		if (Ext == TEXT("json")) return TEXT("application/json");
		if (Ext == TEXT("png")) return TEXT("image/png");
		if (Ext == TEXT("jpg") || Ext == TEXT("jpeg")) return TEXT("image/jpeg");
		if (Ext == TEXT("webp")) return TEXT("image/webp");
		if (Ext == TEXT("svg")) return TEXT("image/svg+xml");
		if (Ext == TEXT("wav")) return TEXT("audio/wav");
		if (Ext == TEXT("mp3")) return TEXT("audio/mpeg");
		if (Ext == TEXT("woff2")) return TEXT("font/woff2");
		if (Ext == TEXT("ttf")) return TEXT("font/ttf");
		return TEXT("application/octet-stream");
	}

	/** O %xx da URL (os nomes das páginas e dos arquivos são ASCII; o resto chega como bytes UTF-8). */
	FString UrlDecode(const FString& In)
	{
		TArray<uint8> Bytes;
		for (int32 I = 0; I < In.Len(); ++I)
		{
			if (In[I] == TEXT('%') && I + 2 < In.Len() && FChar::IsHexDigit(In[I + 1]) && FChar::IsHexDigit(In[I + 2]))
			{
				Bytes.Add(uint8(FParse::HexDigit(In[I + 1]) * 16 + FParse::HexDigit(In[I + 2])));
				I += 2;
			}
			else
			{
				FTCHARToUTF8 C(*In.Mid(I, 1));
				Bytes.Append(reinterpret_cast<const uint8*>(C.Get()), C.Length());
			}
		}
		Bytes.Add(0);
		return FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Bytes.GetData())));
	}

	/** Um pedido ao https://rts.ui/: um arquivo das páginas ou o JSON do jogo (api/). Lê tudo de uma vez (são pequenos). */
	class FShellRequest : public IWebBrowserSchemeHandler
	{
	public:
		virtual bool ProcessRequest(const FString& Verb, const FString& Url, const FSimpleDelegate& OnHeadersReady) override
		{
			FString Path = Url;
			Path.RemoveFromStart(Origin);
			int32 Cut = INDEX_NONE;
			if (Path.FindChar(TEXT('?'), Cut) || Path.FindChar(TEXT('#'), Cut))
			{
				Path.LeftInline(Cut);
			}
			Path = UrlDecode(Path);
			if (Path.IsEmpty())
			{
				Path = TEXT("menu.html");
			}
			if (Path.StartsWith(TEXT("api/")))
			{
				FString Json;
				if (GHandlers.Api && GHandlers.Api(Path.Mid(4), Json))
				{
					FTCHARToUTF8 U(*Json);
					Data.Append(reinterpret_cast<const uint8*>(U.Get()), U.Length());
					Mime = TEXT("application/json");
				}
				else
				{
					Status = 404;
				}
			}
			else if (Path.Contains(TEXT("..")) || !FFileHelper::LoadFileToArray(Data, *(FRTSShell::UiDir() / Path), FILEREAD_Silent))
			{
				Status = 404;
				UE_LOG(LogTemp, Warning, TEXT("RTS telas: não achei %s"), *Path);
			}
			else
			{
				Mime = MimeOf(Path);
			}
			OnHeadersReady.ExecuteIfBound();
			return true;
		}

		virtual void GetResponseHeaders(IHeaders& OutHeaders) override
		{
			OutHeaders.SetMimeType(Mime);
			OutHeaders.SetStatusCode(Status);
			OutHeaders.SetContentLength(Data.Num());
			OutHeaders.SetHeader(TEXT("Cache-Control"), TEXT("no-cache"));
			OutHeaders.SetHeader(TEXT("Access-Control-Allow-Origin"), TEXT("*"));
		}

		virtual bool ReadResponse(uint8* OutBytes, int32 BytesToRead, int32& BytesRead, const FSimpleDelegate& OnMoreDataReady) override
		{
			BytesRead = FMath::Min(BytesToRead, Data.Num() - Offset);
			if (BytesRead <= 0)
			{
				BytesRead = 0;
				return false;
			}
			FMemory::Memcpy(OutBytes, Data.GetData() + Offset, BytesRead);
			Offset += BytesRead;
			return true;
		}

		virtual void Cancel() override {}

	private:
		TArray<uint8> Data;
		int32 Offset = 0;
		int32 Status = 200;
		const TCHAR* Mime = TEXT("text/plain");
	};

	class FShellSchemeFactory : public IWebBrowserSchemeHandlerFactory
	{
	public:
		virtual TUniquePtr<IWebBrowserSchemeHandler> Create(FString Verb, FString Url) override
		{
			return MakeUnique<FShellRequest>();
		}
	};
	FShellSchemeFactory* GFactory = nullptr;   // fica até o fim do processo (o navegador pode pedir até fechar)

	void FocusBrowser()
	{
		if (GShown && GBrowser.IsValid() && FSlateApplication::IsInitialized())   // escondido, nunca rouba o teclado da partida
		{
			FSlateApplication::Get().SetAllUserFocus(GBrowser, EFocusCause::SetDirectly);
		}
	}
}

void FRTSShell::SetHandlers(FRTSShellHandlers In)
{
	GHandlers = MoveTemp(In);
}

const FRTSShellHandlers& FRTSShell::Handlers()
{
	return GHandlers;
}

FString FRTSShell::UiDir()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("RTSShell"));
	return Plugin.IsValid() ? FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("Resources/UI")) : FString();
}

bool FRTSShell::IsAvailable()
{
	if (!FApp::CanEverRender() || !FPaths::FileExists(UiDir() / TEXT("menu.html")))
	{
		return false;
	}
	// carrega o módulo (o IWebBrowserModule::IsAvailable só diz se alguém já carregou, e no jogo ninguém carrega antes)
	return IWebBrowserModule::Get().IsWebModuleAvailable();
}

bool FRTSShell::IsShown()
{
	return GShown;
}

void FRTSShell::Show(const FString& InPage)
{
	if (!GEngine || !GEngine->GameViewport || !IsAvailable())
	{
		return;
	}
	// o teste pode abrir direto numa tela, sem o menu (-RTSShellStart=skirmish.html)
	FString Page = InPage;
	FString StartPage;
	if (GFirstShow && Page.StartsWith(TEXT("menu.html")) && FParse::Value(FCommandLine::Get(), TEXT("-RTSShellStart="), StartPage))
	{
		Page = StartPage;
		GBootDone = true;
	}
	GFirstShow = false;
	// a abertura (só a 1ª vez do menu): a música entra no golpe dela, não aqui
	const bool bBoot = Page.StartsWith(TEXT("menu.html")) && !GBootDone && BootAllowed();
	if (!bBoot)
	{
		using RTSShellAudio::EMusic;
		// (06/11) na partida (as opções abertas do Esc, as fotos das telas da partida) a música é a da partida (o URTSMusic): a das telas fica desligada
		const bool bMatch = (Page.StartsWith(TEXT("screens.html")) && (Page.Contains(TEXT("s=esc")) || Page.Contains(TEXT("s=net")) || Page.Contains(TEXT("s=res"))
			|| Page.Contains(TEXT("from=esc")))) || Page.StartsWith(TEXT("match.html"));
		const bool bTalk = Page.StartsWith(TEXT("skirmish.html")) || Page.StartsWith(TEXT("multiplayer.html")) || Page.StartsWith(TEXT("script.html"));
		RTSShellAudio::SetMusic(bMatch ? EMusic::Off : (bTalk ? EMusic::Duck : EMusic::Menu));
	}
	// a entrada no skirmish e no multiplayer vinda do menu: o golpe grave por uns segundos enquanto a música abaixa
	if ((Page.StartsWith(TEXT("skirmish.html")) || Page.StartsWith(TEXT("multiplayer.html"))) && GLastPage.StartsWith(TEXT("menu.html")))
	{
		RTSShellAudio::Play(RTSShellAudio::EUi::Enter);
	}
	GLastPage = Page;
	if (Page.StartsWith(TEXT("menu.html")))
	{
		// o menu nativo: sai o navegador (se estava), entra o SRTSMenu recomeçado
		HideNative(GMenu.Get());
		if (GBrowser.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GBrowser.ToSharedRef());
			GBrowser->LoadURL(TEXT("about:blank"));
		}
		if (!GMenu.IsValid())
		{
			SAssignNew(GMenu, SRTSMenu);
		}
		else
		{
			GMenu->Restart();
		}
		FString FramesDir;
		const bool bFrames = !GBootDone && FParse::Value(FCommandLine::Get(), TEXT("-RTSBootFrames="), FramesDir);
		if (bBoot && !bFrames)
		{
			GMenu->StartBoot(BootFirstEver());
		}
		GBootDone = true;
		if (bFrames)
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([FramesDir](float) { RenderBootFrames(FramesDir); return false; }), 1.0f);
		}
		GEngine->GameViewport->RemoveViewportWidgetContent(GMenu.ToSharedRef());
		GEngine->GameViewport->AddViewportWidgetContent(GMenu.ToSharedRef(), 1000);
		GEngine->GameViewport->bDisableWorldRendering = true;
		GShown = true;
		GMenuShown = true;
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocus(GMenu, EFocusCause::SetDirectly);
		}
		UE_LOG(LogTemp, Log, TEXT("RTS telas: menu.html (nativo)"));
		FString ShotPath;
		if (FParse::Value(FCommandLine::Get(), TEXT("-RTSMenuShot="), ShotPath))
		{
			// a foto do teste: 4 s depois (o radar aquecido, as transições do hover terminadas)
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([ShotPath](float) { ShotWidget(GMenu, ShotPath, TEXT("menu")); return false; }), 4.0f);
		}
		return;
	}
	if (Page.StartsWith(TEXT("skirmish.html")) && (!Page.Contains(TEXT("mp=1")) || Page.Contains(TEXT("lan=1"))))
	{
		// a sala do skirmish nativa: sai o navegador e o menu, entra a sala com a última configuração; a sala da LAN (mp=1&lan=1) é a
		// mesma no modo da rede (o estado vem do jogo)
		const bool bLanRoom = Page.Contains(TEXT("mp=1"));
		HideNative(GSkirmish.Get());
		if (GBrowser.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GBrowser.ToSharedRef());
			GBrowser->LoadURL(TEXT("about:blank"));
		}
		if (!GSkirmish.IsValid())
		{
			SAssignNew(GSkirmish, SRTSSkirmish);
		}
		else
		{
			GSkirmish->SetLan(false);
			GSkirmish->Restart();
		}
		if (bLanRoom)
		{
			const FString Host = PageArg(Page, TEXT("host"));
			GSkirmish->SetLan(true, PageArg(Page, TEXT("iamhost")) == TEXT("1"), Host.IsEmpty() ? FString(TEXT("HOST")) : Host);
		}
		GEngine->GameViewport->RemoveViewportWidgetContent(GSkirmish.ToSharedRef());
		GEngine->GameViewport->AddViewportWidgetContent(GSkirmish.ToSharedRef(), 1000);
		GEngine->GameViewport->bDisableWorldRendering = true;
		GShown = true;
		GSkirmishShown = true;
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocus(GSkirmish, EFocusCause::SetDirectly);
		}
		UE_LOG(LogTemp, Log, TEXT("RTS telas: skirmish.html (nativo%s)"), bLanRoom ? TEXT(", a sala da LAN") : TEXT(""));
		FString ShotPath;
		if (FParse::Value(FCommandLine::Get(), TEXT("-RTSSkirmishShot="), ShotPath) || FParse::Value(FCommandLine::Get(), TEXT("-RTSScreenShot="), ShotPath))
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([ShotPath](float) { ShotWidget(GSkirmish, ShotPath, TEXT("skirmish")); return false; }), 4.0f);
		}
		return;
	}
	if (Page.StartsWith(TEXT("screens.html")) && Page.Contains(TEXT("s=opt")))
	{
		// as opções nativas: os itens no padrão, a qualidade e o HUD como o jogo tem
		HideNative(GOptions.Get());
		if (GBrowser.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GBrowser.ToSharedRef());
			GBrowser->LoadURL(TEXT("about:blank"));
		}
		if (!GOptions.IsValid())
		{
			SAssignNew(GOptions, SRTSOptions);
		}
		else
		{
			GOptions->Restart();
		}
		GOptions->SetFromMatch(PageArg(Page, TEXT("from")) == TEXT("esc"));
		GEngine->GameViewport->RemoveViewportWidgetContent(GOptions.ToSharedRef());
		GEngine->GameViewport->AddViewportWidgetContent(GOptions.ToSharedRef(), 1000);
		GEngine->GameViewport->bDisableWorldRendering = true;
		GShown = true;
		GOptionsShown = true;
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocus(GOptions, EFocusCause::SetDirectly);
		}
		UE_LOG(LogTemp, Log, TEXT("RTS telas: screens.html?s=opt (nativo)"));
		FString ShotPath;
		if (FParse::Value(FCommandLine::Get(), TEXT("-RTSScreenShot="), ShotPath))
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([ShotPath](float) { ShotWidget(GOptions, ShotPath, TEXT("options")); return false; }), 4.0f);
		}
		return;
	}
	if (Page.StartsWith(TEXT("match.html")))
	{
		const FString M = PageArg(Page, TEXT("m"));
		const ERTSMatchOverlay O = M == TEXT("pause") ? ERTSMatchOverlay::Pause : M == TEXT("result") ? ERTSMatchOverlay::Result : M == TEXT("net") ? ERTSMatchOverlay::Net
			: ERTSMatchOverlay::Loading;
		HideNative(nullptr);
		SetMatchOverlay(ERTSMatchOverlay::None);
		SetMatchOverlay(O);
		UE_LOG(LogTemp, Log, TEXT("RTS telas: %s (nativo)"), *Page);
		FString ShotPath;
		if (FParse::Value(FCommandLine::Get(), TEXT("-RTSScreenShot="), ShotPath))
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([ShotPath](float) { ShotWidget(GMatch, ShotPath, TEXT("partida")); return false; }), 4.0f);
		}
		return;
	}
	if (Page.StartsWith(TEXT("script.html")))
	{
		// o SCRIPT nativo: as cenas do jogo e a armada escolhida
		HideNative(GScript.Get());
		if (GBrowser.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GBrowser.ToSharedRef());
			GBrowser->LoadURL(TEXT("about:blank"));
		}
		if (!GScript.IsValid())
		{
			SAssignNew(GScript, SRTSScript);
		}
		else
		{
			GScript->Restart();
		}
		GEngine->GameViewport->RemoveViewportWidgetContent(GScript.ToSharedRef());
		GEngine->GameViewport->AddViewportWidgetContent(GScript.ToSharedRef(), 1000);
		GEngine->GameViewport->bDisableWorldRendering = true;
		GShown = true;
		GScriptShown = true;
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocus(GScript, EFocusCause::SetDirectly);
		}
		UE_LOG(LogTemp, Log, TEXT("RTS telas: script.html (nativo)"));
		FString ShotPath;
		if (FParse::Value(FCommandLine::Get(), TEXT("-RTSScreenShot="), ShotPath))
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([ShotPath](float) { ShotWidget(GScript, ShotPath, TEXT("script")); return false; }), 4.0f);
		}
		return;
	}
	if (Page.StartsWith(TEXT("multiplayer.html")))
	{
		// a lista de salas nativa: sai o navegador e as outras, entra a lista procurando a LAN de novo
		HideNative(GMultiplayer.Get());
		if (GBrowser.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GBrowser.ToSharedRef());
			GBrowser->LoadURL(TEXT("about:blank"));
		}
		if (!GMultiplayer.IsValid())
		{
			SAssignNew(GMultiplayer, SRTSMultiplayer);
		}
		else
		{
			GMultiplayer->Restart();
		}
		GEngine->GameViewport->RemoveViewportWidgetContent(GMultiplayer.ToSharedRef());
		GEngine->GameViewport->AddViewportWidgetContent(GMultiplayer.ToSharedRef(), 1000);
		GEngine->GameViewport->bDisableWorldRendering = true;
		GShown = true;
		GMultiplayerShown = true;
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocus(GMultiplayer, EFocusCause::SetDirectly);
		}
		UE_LOG(LogTemp, Log, TEXT("RTS telas: multiplayer.html (nativo)"));
		FString ShotPath;
		// no teste da rede de verdade a foto é da sala (que abre depois do HOST), não da lista
		if (FParse::Value(FCommandLine::Get(), TEXT("-RTSScreenShot="), ShotPath) && !FParse::Param(FCommandLine::Get(), TEXT("RTSMpTestHost")))
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([ShotPath](float) { ShotWidget(GMultiplayer, ShotPath, TEXT("multiplayer")); return false; }), 4.0f);
		}
		return;
	}
	HideNative(nullptr);
	const FString Url = FString(Origin) + Page + (Page.Contains(TEXT("?")) ? TEXT("&nomusic=1") : TEXT("?nomusic=1"));
	if (!GBrowser.IsValid())
	{
		IWebBrowserSingleton* Singleton = IWebBrowserModule::Get().GetSingleton();
		if (!Singleton)
		{
			return;
		}
		if (!GFactory)
		{
			GFactory = new FShellSchemeFactory();
			Singleton->RegisterSchemeHandlerFactory(TEXT("https"), TEXT("rts.ui"), GFactory);
		}
		GBridge.Reset(NewObject<URTSShellBridge>());
		// a ponte entra antes da 1ª página: o navegador nasce vazio, liga o window.ue.shell e só então abre a página
		GPendingPage = Url;
		SAssignNew(GBrowser, SWebBrowser)
			.InitialURL(TEXT("about:blank"))
			.ShowControls(false)
			.ShowAddressBar(false)
			.ShowErrorMessage(true)
			.ShowInitialThrobber(false)
			.SupportsTransparency(false)
			.BackgroundColor(FColor(10, 13, 11, 255))
			.BrowserFrameRate(60)
			.OnSuppressContextMenu_Lambda([]() { return true; })
			// as páginas que voltam pro menu (o BACK, o Esc) caem no menu nativo, no próximo quadro
			.OnBeforeNavigation_Lambda([](const FString& Url, const FWebNavigationRequest&)
			{
				// o menu é nativo; as outras páginas passam pelo Show (a música da tela e o nomusic=1 da página), inclusive as que uma
				// página abre sozinha
				const int32 At = Url.Find(TEXT("rts.ui/"));
				if (At != INDEX_NONE && (Url.Contains(TEXT("rts.ui/menu.html")) || !Url.Contains(TEXT("nomusic=1"))))
				{
					const FString Page = Url.Mid(At + 7);
					FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Page](float) { FRTSShell::Show(Page); return false; }));
					return true;
				}
				return false;
			})
			.OnConsoleMessage_Lambda([](const FString& Message, const FString& Source, int32 Line, EWebBrowserConsoleLogSeverity Severity)
			{
				if (Severity >= EWebBrowserConsoleLogSeverity::Warning)
				{
					UE_LOG(LogTemp, Warning, TEXT("RTS telas (página): %s [%s:%d]"), *Message, *FPaths::GetCleanFilename(Source), Line);
				}
			})
			.OnLoadCompleted_Lambda([]()
			{
				if (!GPendingPage.IsEmpty() && GBrowser.IsValid())
				{
					const FString Next = MoveTemp(GPendingPage);
					GPendingPage.Reset();
					GBrowser->LoadURL(Next);
				}
				FocusBrowser();
			});
		GBrowser->BindUObject(TEXT("shell"), GBridge.Get(), true);
		UE_LOG(LogTemp, Log, TEXT("RTS telas: navegador criado (%s)"), *UiDir());
	}
	else if (GPendingPage.IsEmpty())
	{
		GBrowser->LoadURL(Url);
	}
	else
	{
		GPendingPage = Url;
	}
	// tira e põe de novo: o widget solto no viewport atravessa as trocas de mapa, e nunca fica dois
	GEngine->GameViewport->RemoveViewportWidgetContent(GBrowser.ToSharedRef());
	GEngine->GameViewport->AddViewportWidgetContent(GBrowser.ToSharedRef(), 1000);
	GEngine->GameViewport->bDisableWorldRendering = true;   // as telas cobrem tudo: o mapa do fundo não precisa ser desenhado
	GShown = true;
	FocusBrowser();
	UE_LOG(LogTemp, Log, TEXT("RTS telas: %s"), *Page);
}

void FRTSShell::Hide()
{
	if (GEngine && GEngine->GameViewport)
	{
		if (GBrowser.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GBrowser.ToSharedRef());
		}
		if (GMenu.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GMenu.ToSharedRef());
		}
		if (GSkirmish.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GSkirmish.ToSharedRef());
		}
		if (GMultiplayer.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GMultiplayer.ToSharedRef());
		}
		if (GOptions.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GOptions.ToSharedRef());
		}
		if (GScript.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GScript.ToSharedRef());
		}
		GEngine->GameViewport->bDisableWorldRendering = false;
	}
	GMenuShown = false;
	GSkirmishShown = false;
	GMultiplayerShown = false;
	GOptionsShown = false;
	GScriptShown = false;
	RTSShellAudio::SetMusic(RTSShellAudio::EMusic::Off);
	if (GBrowser.IsValid())
	{
		GBrowser->LoadURL(TEXT("about:blank"));   // a música e o radar param junto
	}
	GShown = false;
}

// ---------------------------------------------------------------------------------------------------------------------------
// o módulo: solta o navegador e a ponte antes da Unreal fechar (um widget ou um UObject preso num global estático cairia no fim)

class FRTSShellModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FCoreDelegates::OnEnginePreExit.AddStatic(&FRTSShellModule::Release);
	}
	virtual void ShutdownModule() override
	{
		Release();
	}
	static void Release()
	{
		if (GBrowser.IsValid() && GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GBrowser.ToSharedRef());
		}
		if (GMenu.IsValid() && GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GMenu.ToSharedRef());
		}
		GMenu.Reset();
		GSkirmish.Reset();
		GSkirmishShown = false;
		GMultiplayer.Reset();
		GMultiplayerShown = false;
		GOptions.Reset();
		GOptionsShown = false;
		GScript.Reset();
		GScriptShown = false;
		RTSShellAudio::Shutdown();
		GMenuShown = false;
		GBrowser.Reset();
		GBridge.Reset();
		GShown = false;
	}
};

IMPLEMENT_MODULE(FRTSShellModule, RTSShell)

// ---------------------------------------------------------------------------------------------------------------------------
// a ponte (window.ue.shell)

bool URTSShellBridge::IsDev() { return GHandlers.IsDev ? GHandlers.IsDev() : false; }
FString URTSShellBridge::ArmedScenario() { return GHandlers.ArmedScenario ? GHandlers.ArmedScenario() : FString(); }
void URTSShellBridge::RunScenario(const FString& Name) { if (GHandlers.RunScenario) GHandlers.RunScenario(Name); }
void URTSShellBridge::OpenLab() { if (GHandlers.OpenLab) GHandlers.OpenLab(); }
void URTSShellBridge::StartSkirmish(const FString& SetupJson) { if (GHandlers.StartSkirmish) GHandlers.StartSkirmish(SetupJson); }
void URTSShellBridge::SetQuality(int32 Level) { if (GHandlers.SetQuality) GHandlers.SetQuality(Level); }
int32 URTSShellBridge::GetQuality() { return GHandlers.GetQuality ? GHandlers.GetQuality() : 3; }
void URTSShellBridge::Quit() { if (GHandlers.Quit) GHandlers.Quit(); }
bool URTSShellBridge::Lan(const FString& Command) { return GHandlers.Lan ? GHandlers.Lan(Command) : false; }
namespace
{
	/** Só nomes simples de chave (letras e números): a página não escreve fora do [RTS] nem quebra o arquivo. */
	bool OptionKeyOk(const FString& Key)
	{
		if (Key.IsEmpty() || Key.Len() > 64) return false;
		for (const TCHAR C : Key) if (!FChar::IsAlnum(C)) return false;
		return true;
	}
}
FString URTSShellBridge::GetOption(const FString& Key)
{
	FString V;
	if (GConfig && OptionKeyOk(Key)) GConfig->GetString(TEXT("RTS"), *Key, V, GGameUserSettingsIni);
	return V;
}
void URTSShellBridge::SetOption(const FString& Key, const FString& Value)
{
	if (!GConfig || !OptionKeyOk(Key) || Value.Len() > 256) return;
	GConfig->SetString(TEXT("RTS"), *Key, *Value.Replace(TEXT("\n"), TEXT(" ")), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	UE_LOG(LogTemp, Log, TEXT("RTS telas: opção %s = %s (GameUserSettings [RTS])"), *Key, *Value);
}
void URTSShellBridge::Log(const FString& Text) { UE_LOG(LogTemp, Log, TEXT("RTS telas (página): %s"), *Text); }

// ---------------------------------------------------------------- (06/11) as telas da partida

void FRTSShell::SetIntro(const FRTSMatchIntro& In)
{
	FScopeLock Lock(&GIntroLock);
	GIntro = In;
}

FRTSMatchIntro FRTSShell::GetIntro()
{
	FScopeLock Lock(&GIntroLock);
	return GIntro;
}

TSharedRef<SWidget> FRTSShell::MakeLoadingScreen()
{
	// na thread do jogo (o PreLoadMap): o relógio do carregando começa, e as imagens e as fontes ficam prontas aqui (o filme pinta
	// noutra thread e o cache das imagens não é dela)
	SRTSMatch::LoadStart() = FPlatformTime::Seconds();
	SRTSMatch::Warm(GetIntro());
	return SNew(SRTSMatch).Movie(true);
}

void FRTSShell::SetMatchOverlay(ERTSMatchOverlay Overlay)
{
	if (!GEngine || !GEngine->GameViewport) return;
	// a troca de mapa pode tirar os widgets do viewport sem passar por aqui: a caixa solta volta como se não estivesse
	if (GOverlay != ERTSMatchOverlay::None && GMatchBox.IsValid() && !GMatchBox->GetParentWidget().IsValid()) GOverlay = ERTSMatchOverlay::None;
	if (Overlay == GOverlay) return;
	if (Overlay == ERTSMatchOverlay::None)
	{
		if (GMatchBox.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(GMatchBox.ToSharedRef());
		GOverlay = ERTSMatchOverlay::None;
		UE_LOG(LogTemp, Log, TEXT("RTS telas: tela da partida: nenhuma (o jogo)"));
		return;
	}
	if (!GMatch.IsValid())
	{
		// o .dim do mockup tem backdrop-filter: blur(3px): o borrão da Slate por trás (desligado no carregando, que é opaco, e olhando o mapa)
		SAssignNew(GMatch, SRTSMatch);
		GMatchBox = SNew(SBackgroundBlur)
			.BlurStrength_Lambda([]() { return GMatch.IsValid() ? GMatch->BlurStrength() : 0.0f; })
			.Padding(0.0f)
			[
				GMatch.ToSharedRef()
			];
	}
	if (GOverlay == ERTSMatchOverlay::None) GEngine->GameViewport->AddViewportWidgetContent(GMatchBox.ToSharedRef(), 900);
	UE_LOG(LogTemp, Log, TEXT("RTS telas: tela da partida: %s"), Overlay == ERTSMatchOverlay::Pause ? TEXT("Esc") : Overlay == ERTSMatchOverlay::Result ? TEXT("resultado")
		: Overlay == ERTSMatchOverlay::Net ? TEXT("rede") : TEXT("carregando"));
	RTSShellAudio::SetMusic(RTSShellAudio::EMusic::Off);   // a música é a da partida (o URTSMusic: o fim, a pausa); a das telas nunca por cima
	GMatch->SetMode(Overlay == ERTSMatchOverlay::Pause ? SRTSMatch::EMode::Pause : Overlay == ERTSMatchOverlay::Result ? SRTSMatch::EMode::Result
		: Overlay == ERTSMatchOverlay::Net ? SRTSMatch::EMode::Net : SRTSMatch::EMode::Loading);
	GOverlay = Overlay;
}

ERTSMatchOverlay FRTSShell::GetMatchOverlay()
{
	return GOverlay;
}

bool FRTSShell::MatchButtonCenter(const FString& Id, FVector2D& OutViewportPixel)
{
	// o caminho de volta do ToSlate do teste automático: o ponto do palco vira absoluto, depois local do viewport, depois pixel
	if (!GMatch.IsValid() || GOverlay == ERTSMatchOverlay::None || !GEngine || !GEngine->GameViewport || !GEngine->GameViewport->Viewport) return false;
	FVector2f StagePt;
	if (!GMatch->ButtonCenter(Id, StagePt)) return false;
	const TSharedPtr<SViewport> V = GEngine->GameViewport->GetGameViewportWidget();
	if (!V.IsValid()) return false;
	const FGeometry S = RTSShellUI::StageFit(GMatch->GetTickSpaceGeometry());
	const FVector2D Abs = S.LocalToAbsolute(FVector2D(StagePt));
	const FGeometry& G = V->GetTickSpaceGeometry();
	const FVector2D Local = G.AbsoluteToLocal(Abs), Size = G.GetLocalSize();
	const FIntPoint Px = GEngine->GameViewport->Viewport->GetSizeXY();
	if (Size.X <= 0.0 || Size.Y <= 0.0) return false;
	OutViewportPixel = FVector2D(Local.X / Size.X * Px.X, Local.Y / Size.Y * Px.Y);
	return true;
}

// ---------------------------------------------------------------- (06/11) o teste de ponta a ponta das telas da partida

namespace RTSShellMatchTest
{
	/** -RTSMatchTest=<pasta>: as fotos do jogo inteiro a cada tela, o Esc de verdade, o RESUME e o BACK TO LOBBY (ver o patch_match_test.py). */
	struct FState
	{
		FString Dir;
		ERTSMatchOverlay Last = ERTSMatchOverlay::None;
		double Since = 0.0;
		bool bShot = false, bPlayedEsc = false, bResumed = false, bLeft = false, bSawPlay = false, bStarted = false;
		int32 Count = 0;
	};
	FState GTest;

	bool Tick(float)
	{
		const double Now = FPlatformTime::Seconds();
		const ERTSMatchOverlay O = FRTSShell::GetMatchOverlay();
		if (O != GTest.Last) { GTest.Last = O; GTest.Since = Now; GTest.bShot = false; }
		const double Age = Now - GTest.Since;
		// a foto do jogo inteiro (a interface junto): 1,5 s depois de cada tela da partida
		if (!GTest.bShot && O != ERTSMatchOverlay::None && Age > 1.5)
		{
			GTest.bShot = true;
			const TCHAR* Name = O == ERTSMatchOverlay::Pause ? TEXT("esc") : O == ERTSMatchOverlay::Result ? TEXT("resultado") : O == ERTSMatchOverlay::Net ? TEXT("rede") : TEXT("carregando");
			const FString Path = FPaths::Combine(GTest.Dir, FString::Printf(TEXT("%02d_%s.png"), ++GTest.Count, Name));
			FScreenshotRequest::RequestScreenshot(Path, true, false);
			UE_LOG(LogTemp, Log, TEXT("RTS telas: teste da partida, foto %s"), *Path);
		}
		if (O == ERTSMatchOverlay::None && GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWorld() && !GEngine->GameViewport->GetWorld()->GetName().Contains(TEXT("FrontEnd")))
		{
			GTest.bSawPlay = true;
			if (!GTest.bPlayedEsc && Age > 3.0 && FSlateApplication::IsInitialized())
			{
				// o Esc de verdade: a tecla pela Slate (o controle do jogo abre o Esc como se fosse ele)
				GTest.bPlayedEsc = true;
				UE_LOG(LogTemp, Log, TEXT("RTS telas: teste da partida, a tecla Esc"));
				FKeyEvent Down(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
				FSlateApplication::Get().ProcessKeyDownEvent(Down);
				FSlateApplication::Get().ProcessKeyUpEvent(Down);
			}
		}
		if (O == ERTSMatchOverlay::Pause && !GTest.bResumed && Age > 2.5 && FRTSShell::Handlers().MatchButton)
		{
			GTest.bResumed = true;
			UE_LOG(LogTemp, Log, TEXT("RTS telas: teste da partida, RESUME"));
			FRTSShell::Handlers().MatchButton(TEXT("resume"));
		}
		if ((O == ERTSMatchOverlay::Result || O == ERTSMatchOverlay::Net) && !GTest.bLeft && Age > 4.0)
		{
			// o clique do teste automático: o centro do botão pela ponte (FRTSShell::MatchButtonCenter, em pixels do viewport) e o
			// clique de verdade pela Slate, no caminho do ToSlate do RTSAutoTest. (06/11 à noite) No aviso da partida que não montou
			// (o caso 7, o teste com uma cena que não existe), o MAIN MENU
			GTest.bLeft = true;
			const TCHAR* Ids[3] = { TEXT("back_to_room"), TEXT("lobby"), TEXT("menu") };
			for (const TCHAR* Id : Ids)
			{
				FVector2D Px(0.0, 0.0);
				const TSharedPtr<SViewport> V = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetGameViewportWidget() : nullptr;
				if (!V.IsValid() || !FRTSShell::MatchButtonCenter(Id, Px)) continue;
				const FGeometry& G = V->GetTickSpaceGeometry();
				const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
				const FVector2D Abs = G.LocalToAbsolute(G.GetLocalSize() * FVector2D(Px.X / FMath::Max(1, Size.X), Px.Y / FMath::Max(1, Size.Y)));
				UE_LOG(LogTemp, Log, TEXT("RTS telas: teste da partida, clique no %s (pixel %.0f, %.0f)"), Id, Px.X, Px.Y);
				FSlateApplication& App = FSlateApplication::Get();
				TSet<FKey> Pressed;
				App.ProcessMouseMoveEvent(FPointerEvent(App.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, Abs, Abs, Pressed, EKeys::Invalid, 0.0f, FModifierKeysState()));
				Pressed.Add(EKeys::LeftMouseButton);
				App.ProcessMouseButtonDownEvent(nullptr, FPointerEvent(App.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, Abs, Abs, Pressed, EKeys::LeftMouseButton, 0.0f, FModifierKeysState()));
				Pressed.Remove(EKeys::LeftMouseButton);
				App.ProcessMouseButtonUpEvent(FPointerEvent(App.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, Abs, Abs, Pressed, EKeys::LeftMouseButton, 0.0f, FModifierKeysState()));
				break;
			}
		}
		// -RTSMatchTestStart (o menu antigo, sem o SCRIPT): a partida começa pelo botão do controle ("skirmish"), 3 s depois do menu
		if (!GTest.bStarted && O == ERTSMatchOverlay::None && Age > 3.0 && FParse::Param(FCommandLine::Get(), TEXT("RTSMatchTestStart")) && GEngine && GEngine->GameViewport
			&& GEngine->GameViewport->GetWorld() && GEngine->GameViewport->GetWorld()->GetName().Contains(TEXT("FrontEnd")) && FRTSShell::Handlers().MatchButton)
		{
			GTest.bStarted = true;
			UE_LOG(LogTemp, Log, TEXT("RTS telas: teste da partida, começa pelo menu antigo"));
			FRTSShell::Handlers().MatchButton(TEXT("skirmish"));
		}
		return true;
	}

	struct FInstall
	{
		FInstall()
		{
			FCoreDelegates::GetOnPostEngineInit().AddLambda([]()
			{
				if (!FParse::Value(FCommandLine::Get(), TEXT("-RTSMatchTest="), GTest.Dir)) return;
				IFileManager::Get().MakeDirectory(*GTest.Dir, true);
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 0.1f);
				UE_LOG(LogTemp, Log, TEXT("RTS telas: teste da partida ligado (%s)"), *GTest.Dir);
			});
		}
	};
	FInstall GInstall;
}
