#include "RTSShell.h"
#include "RTSShellBridge.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
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
	void ShotMenu(const FString& Path)
	{
		if (!GMenu.IsValid())
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
		Renderer->DrawWidget(Target, GMenu.ToSharedRef(), FVector2D(1920, 1080), 0.0f);
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
		UE_LOG(LogTemp, Log, TEXT("RTS telas: foto do menu nativo em %s"), *Path);
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

void FRTSShell::Show(const FString& Page)
{
	if (!GEngine || !GEngine->GameViewport || !IsAvailable())
	{
		return;
	}
	// a abertura (só a 1ª vez do menu): a música entra no golpe dela, não aqui
	const bool bBoot = Page.StartsWith(TEXT("menu.html")) && !GBootDone && BootAllowed();
	if (!bBoot)
	{
		using RTSShellAudio::EMusic;
		const bool bMatch = Page.StartsWith(TEXT("screens.html")) && (Page.Contains(TEXT("s=esc")) || Page.Contains(TEXT("s=net")) || Page.Contains(TEXT("s=res")));
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
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([ShotPath](float) { ShotMenu(ShotPath); return false; }), 4.0f);
		}
		return;
	}
	if (GMenuShown && GMenu.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(GMenu.ToSharedRef());
		GMenuShown = false;
	}
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
		GEngine->GameViewport->bDisableWorldRendering = false;
	}
	GMenuShown = false;
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
void URTSShellBridge::Log(const FString& Text) { UE_LOG(LogTemp, Log, TEXT("RTS telas (página): %s"), *Text); }
