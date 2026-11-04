# As linhas das Telas nos arquivos compartilhados (04/11, a abertura), aplicadas só na vez de compilar e por troca exata de trecho
# (nunca copiando o arquivo inteiro: as outras sessões mexem neles). "apply" troca o que ainda não foi trocado; "revert" desfaz.
#   - o jogo abre no mapa vazio do menu (DefaultEngine.ini) e volta pra ele no fim da partida (RTSSessionSubsystem.cpp);
#   - a tela de carregamento do mapa do menu é o escuro da abertura, sem o "LOADING" reprovado (RTSGameModule.cpp).
import os, sys

G = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
PATCHES = [
    ('Config/DefaultEngine.ini',
     'GameDefaultMap=/Game/Maps/World_TestSalt2.World_TestSalt2',
     'GameDefaultMap=/Game/Maps/FrontEnd.FrontEnd'),
    ('Source/RTSGame/Private/RTSSessionSubsystem.cpp',
     'UGameplayStatics::OpenLevel(GetGameInstance(), FName(*SkirmishMapName()), true, TEXT("game=/Script/RTSGame.RTSMenuGameMode"));',
     'UGameplayStatics::OpenLevel(GetGameInstance(), FName(TEXT("FrontEnd")), true, TEXT("game=/Script/RTSGame.RTSMenuGameMode"));   // (04/11, Telas) o menu mora no mapa vazio'),
    ('Source/RTSGame/Private/RTSGameModule.cpp',
     '''		PreLoadHandle = FCoreUObjectDelegates::PreLoadMap.AddLambda([](const FString&)
		{
			if (!GetMoviePlayer()->IsInitialized())
			{
				return;
			}
			FLoadingScreenAttributes Screen;
			Screen.bAutoCompleteWhenLoadingCompletes = true;
			Screen.MinimumLoadingScreenDisplayTime = 0.4f;
			Screen.WidgetLoadingScreen = MakeLoadingWidget();''',
     '''		PreLoadHandle = FCoreUObjectDelegates::PreLoadMap.AddLambda([](const FString& Map)
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
				Screen.WidgetLoadingScreen = MakeLoadingWidget();
			}'''),
]


def main(mode):
    for rel, old, new in PATCHES:
        path = os.path.join(G, rel)
        raw = open(path, 'rb').read()
        crlf = b'\r\n' in raw
        s = raw.decode('utf-8-sig').replace('\r\n', '\n')
        a, b = (old, new) if mode == 'apply' else (new, old)
        n = s.count(a)
        if n == 0:
            print(f'{rel}: nada a trocar ({"já aplicado" if s.count(b) else "trecho não encontrado"})')
            continue
        s = s.replace(a, b)
        bom = raw.startswith(b'\xef\xbb\xbf')
        out = s.replace('\n', '\r\n') if crlf else s
        open(path, 'wb').write((b'\xef\xbb\xbf' if bom else b'') + out.encode('utf-8'))
        print(f'{rel}: {mode} ({n} trecho{"s" if n > 1 else ""})')


main(sys.argv[1] if len(sys.argv) > 1 else 'apply')
