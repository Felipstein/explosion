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

namespace
{
	const TCHAR* const Origin = TEXT("https://rts.ui/");

	FRTSShellHandlers GHandlers;
	TSharedPtr<SWebBrowser> GBrowser;
	TStrongObjectPtr<URTSShellBridge> GBridge;
	bool GShown = false;
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
	const FString Url = FString(Origin) + Page;
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
		GEngine->GameViewport->bDisableWorldRendering = false;
	}
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
