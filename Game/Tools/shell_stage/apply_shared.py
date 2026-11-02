# As telas novas no jogo: põe os arquivos novos no lugar (o plugin RTSShell e o RTSShellGame) e liga os ganchos nos
# arquivos compartilhados, de uma vez, só na minha vez da fila (sem compilação de ninguém rodando). "revert" desfaz tudo.
import os, shutil, sys
ST = os.path.dirname(os.path.abspath(__file__))
G = r'C:\Users\luisf\explosion\Game'
MODE = sys.argv[1] if len(sys.argv) > 1 else 'apply'
MOVES = [(os.path.join(ST, 'Plugins', 'RTSShell'), os.path.join(G, 'Plugins', 'RTSShell')),
         (os.path.join(ST, 'Source', 'RTSGame', 'Private', 'RTSShellGame.cpp'), os.path.join(G, 'Source', 'RTSGame', 'Private', 'RTSShellGame.cpp')),
         (os.path.join(ST, 'Source', 'RTSGame', 'Public', 'RTSShellGame.h'), os.path.join(G, 'Source', 'RTSGame', 'Public', 'RTSShellGame.h'))]
INC = '#include "RTSShellGame.h"   // (01/11) as telas novas (plugin RTSShell)\n'
PATCHES = {
    'RTSGame.uproject': [
        ('		{ "Name": "RTSWorldProps", "Enabled": true }\n', '		{ "Name": "RTSWorldProps", "Enabled": true },\n		{ "Name": "RTSShell", "Enabled": true }\n')],
    r'Source\RTSGame\RTSGame.Build.cs': [('"RTSSunShadow"', '"RTSSunShadow", "RTSShell"')],
    r'Source\RTSGame\Private\RTSPlayerController.cpp': [
        ('#include "RTSPlayerController.h"\n', '#include "RTSPlayerController.h"\n' + INC),
        ('	for (TActorIterator<ALookDirector> It(GetWorld()); It; ++It)\n	{\n		It->ApplyPreset(Level);\n	}\n}\n',
         '	for (TActorIterator<ALookDirector> It(GetWorld()); It; ++It)\n	{\n		It->ApplyPreset(Level);\n	}\n'
         '	RTSShellGame::OnPlayerBegin(this, bMenu);   // (01/11) no menu, as telas novas cobrem o jogo (RTSShellGame.h)\n}\n')],
    r'Source\RTSGame\Private\RTSHUD.cpp': [
        ('#include "RTSHUD.h"\n', '#include "RTSHUD.h"\n' + INC),
        ('	if (PC->IsMenu())\n	{\n		DrawMenu(PC);\n		return;\n	}\n',
         '	if (PC->IsMenu())\n	{\n		if (!RTSShellGame::IsCovering())   // (01/11) com as telas novas por cima, o menu antigo não se desenha\n		{\n			DrawMenu(PC);\n		}\n		return;\n	}\n')],
    r'Source\RTSGame\Private\RTSSessionSubsystem.cpp': [
        ('#include "RTSSessionSubsystem.h"\n', '#include "RTSSessionSubsystem.h"\n' + INC),
        ('FString URTSSessionSubsystem::SkirmishMapName()\n{\n',
         'FString URTSSessionSubsystem::SkirmishMapName()\n{\n'
         '	// (01/11, as telas novas) o mapa escolhido na sala do skirmish ou o da cena do SCRIPT\n'
         '	const FString Chosen = RTSShellGame::MapOverride();\n	if (!Chosen.IsEmpty())\n	{\n		return Chosen;\n	}\n')],
    r'Source\RTSGame\Private\RTSMatchSubsystem.cpp': [
        ('#include "RTSMatchSubsystem.h"\n', '#include "RTSMatchSubsystem.h"\n' + INC),
        ('	uint8 GPlayerPalette[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };\n}\n\nFLinearColor URTSMatchSubsystem::PlayerColor(uint8 Player)\n{\n',
         '	uint8 GPlayerPalette[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };\n'
         '	/** (01/11) A cor escolhida na sala nova do skirmish (as 24 da tela), no lugar da paleta. */\n'
         '	bool GColorChosen[8] = {};\n	FLinearColor GColorValue[8];\n}\n\n'
         'FLinearColor URTSMatchSubsystem::PlayerColor(uint8 Player)\n{\n'
         '	if (GColorChosen[Player % 8])\n	{\n		return GColorValue[Player % 8];\n	}\n'),
        ('	const int32 PlayerCount = bNet ? NetSession->PlayerCount()\n		: (bLab ? FMath::Clamp(bScenario ? Info->StartsM.Num() : LabPlayers, 2, FMath::Max(2, FMath::Min(int32(Sim::MaxPlayers), Info->StartsM.Num()))) : 2);\n',
         '	// (01/11) o skirmish montado na sala nova (RTSShellGame.h): as vagas dela no lugar do você contra uma IA\n'
         '	const TArray<FRTSLocalSlot>* LocalSlots = (!bNet && !bLab && !bScenario) ? RTSShellGame::LocalSlots() : nullptr;\n'
         '	const int32 PlayerCount = bNet ? NetSession->PlayerCount()\n		: (bLab ? FMath::Clamp(bScenario ? Info->StartsM.Num() : LabPlayers, 2, FMath::Max(2, FMath::Min(int32(Sim::MaxPlayers), Info->StartsM.Num())))\n'
         '			: (LocalSlots ? FMath::Clamp(LocalSlots->Num(), 2, FMath::Max(2, FMath::Min(int32(Sim::MaxPlayers), Info->StartsM.Num()))) : 2));\n'),
        ('		GPlayerPalette[P] = bNet ? NetSession->ColorOfSlot(uint8(P)) : uint8(P);\n	}\n',
         '		GPlayerPalette[P] = bNet ? NetSession->ColorOfSlot(uint8(P)) : uint8(P);\n'
         '		GColorChosen[P] = LocalSlots && LocalSlots->IsValidIndex(P) && (*LocalSlots)[P].bHasColor;\n'
         '		GColorValue[P] = GColorChosen[P] ? (*LocalSlots)[P].Color : FLinearColor::White;\n	}\n'),
        ('				const uint8 LobbyTeam = bNet ? NetSession->TeamOfSlot(uint8(P)) : 0;\n',
         '				const uint8 LobbyTeam = bNet ? NetSession->TeamOfSlot(uint8(P)) : (LocalSlots && LocalSlots->IsValidIndex(P) ? (*LocalSlots)[P].Team : 0);\n'),
        ('					: (bLab ? false : P != 0));   // (v5) no laboratório a IA começa desligada (o painel liga por jogador)\n',
         '					: (LocalSlots && LocalSlots->IsValidIndex(P) ? (*LocalSlots)[P].bAI : (bLab ? false : P != 0)));   // (v5) no laboratório a IA começa desligada (o painel liga por jogador)\n'),
        ('					F = SlotAI[P] ? FacSession->AIFaction : (bNet ? FString(TEXT("usa")) : FacSession->PlayerFaction);\n				}\n',
         '					F = SlotAI[P] ? FacSession->AIFaction : (bNet ? FString(TEXT("usa")) : FacSession->PlayerFaction);\n				}\n'
         '				if (LocalSlots && LocalSlots->IsValidIndex(P))\n				{\n					F = (*LocalSlots)[P].Faction;   // (01/11) a de cada vaga da sala nova\n				}\n'),
        ('		TArray<FVector2D> SlotStarts;\n		for (int32 P = 0; P < PlayerCount; ++P)\n		{\n			SlotStarts.Add(Starts[StartOf[P]]);\n		}\n',
         '		// (01/11, a sala nova) a base escolhida em cada vaga; as sorteadas ficam com as livres, na ordem de cima\n'
         '		if (LocalSlots)\n		{\n			TArray<int32> Chosen;\n			Chosen.Init(-1, PlayerCount);\n'
         '			for (int32 P = 0; P < PlayerCount; ++P)\n			{\n				const int32 B = LocalSlots->IsValidIndex(P) ? (*LocalSlots)[P].Start : -1;\n'
         '				if (Starts.IsValidIndex(B) && !Chosen.Contains(B))\n				{\n					Chosen[P] = B;\n				}\n			}\n'
         '			for (int32 P = 0; P < PlayerCount; ++P)\n			{\n				int32 Pick = StartOf[P];\n'
         '				for (int32 K = 0; Chosen[P] < 0 && Chosen.Contains(Pick) && K < Starts.Num(); ++K)\n				{\n					Pick = K;\n				}\n'
         '				if (Chosen[P] < 0)\n				{\n					Chosen[P] = Pick;\n				}\n			}\n			StartOf = Chosen;\n		}\n'
         '		TArray<FVector2D> SlotStarts;\n		for (int32 P = 0; P < PlayerCount; ++P)\n		{\n			SlotStarts.Add(Starts[StartOf[P]]);\n		}\n')],
}


def rw(rel, fn):
    p = os.path.join(G, rel)
    raw = open(p, 'rb').read(); crlf = b'\r\n' in raw
    s = raw.decode('utf-8-sig' if raw.startswith(b'\xef\xbb\xbf') else 'utf-8').replace('\r\n', '\n')
    s = fn(s)
    out = s.replace('\n', '\r\n') if crlf else s
    open(p, 'wb').write((b'\xef\xbb\xbf' if raw.startswith(b'\xef\xbb\xbf') else b'') + out.encode('utf-8'))


if MODE == 'check':
    for rel, pairs in PATCHES.items():
        s = open(os.path.join(G, rel), encoding='utf-8-sig').read().replace('\r\n', '\n')
        for a, b in pairs:
            print('ok ' if s.count(a) == 1 else f'FALTA ({s.count(a)}) ', rel, a.strip()[:60].replace('\n', ' '))
    sys.exit(0)
if MODE == 'apply':
    for rel, pairs in PATCHES.items():
        def f(s, pairs=pairs, rel=rel):
            for a, b in pairs:
                assert s.count(a) == 1, (rel, a[:60])
                s = s.replace(a, b)
            return s
        rw(rel, f)
    for src, dst in MOVES:
        shutil.move(src, dst)
    print('aplicado')
elif MODE == 'revert':
    for rel, pairs in PATCHES.items():
        def f(s, pairs=pairs, rel=rel):
            for a, b in reversed(pairs):
                if s.count(b) == 1:
                    s = s.replace(b, a)
            return s
        rw(rel, f)
    for src, dst in MOVES:
        if os.path.exists(dst):
            shutil.move(dst, src)
    print('desfeito')
