#pragma once

// A partida em rede local (LAN), docs/MULTIPLAYER.md: lockstep com o anfitrião como relógio.
// - Só os comandos viajam. O anfitrião monta a partida e manda a montagem inteira (grade,
//   regras, jogadores) pra todos; cada PC roda a mesma simulação.
// - O anfitrião carimba cada comando no próximo passo ainda não fechado e manda a todos o
//   pacote de cada passo (com os ainda não confirmados, contra a perda), com a impressão
//   digital do estado: uma dessincronia aparece na hora.
// - Vive o jogo inteiro (UGameInstanceSubsystem): a sala no menu e a partida no mapa.
// Primeira versão (10/10): até 8 vagas na sala, testada em 1 × 1. Em 13/10 (N5 do plano): a pausa, render-se e a
// queda de quem some (o ZH: aviso aos 5 s, cai aos 60 s ou quando todos os outros votam). Sem reconexão nem
// observador ainda (N6 e N7).

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Sim/Commands.h"
#include "RTSSessionSubsystem.h"
#include "RTSShellGame.h"
#include "RTSNetSession.generated.h"

class FInternetAddr;
class FSocket;

UENUM()
enum class ERTSNetRole : uint8
{
	None,
	Host,
	Client
};

UENUM()
enum class ERTSNetPhase : uint8
{
	Idle,
	Lobby,
	Loading,
	Playing,
	Ended
};

/** Uma partida anunciada na rede local. */
struct FRTSLanGame
{
	FString Name;
	FString Ip;
	int32 Port = 0;
	int32 Players = 0;
	bool bOpen = true;
	double LastSeen = 0.0;
};

/** Um jogador da sala. */
struct FRTSNetMember
{
	FString Name;
	uint8 Slot = 0;
	bool bReady = false;
	/** A cor escolhida na sala (índice da paleta, URTSMatchSubsystem::PaletteColor). */
	uint8 Color = 0;
	/** Já voltou da partida pra sala (o anfitrião só começa outra com todos aqui). */
	bool bInLobby = true;
	/** O time: 0 = sem time (contra todos), 1..4. */
	uint8 Team = 0;
	/** Vaga de computador (a IA roda no anfitrião e manda as ordens como um jogador). */
	bool bAI = false;
	/** (06/11, a sala nova) a facção ("random", "usa", "russia", "china", "gla"), a base do mapa (−1 = sorteada), a cor escolhida
	 *  das 24 da sala (0xRRGGBB; 0 = a da paleta, automática), o nível da IA (0 fácil, 1 médio, 2 difícil) e o ping (ms, o anfitrião mede). */
	FString Faction = TEXT("random");
	int32 Base = -1;
	uint32 Rgb = 0;
	uint8 Difficulty = 1;
	int32 PingMs = 0;
	/** O READY da sala (quem entrou aperta; o anfitrião e a IA sempre prontos). O bReady acima é o da partida (o mapa carregou). */
	bool bLobbyReady = false;
};

UCLASS()
class RTSGAME_API URTSNetSession : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	static constexpr int32 GamePort = 47800;
	static constexpr int32 BeaconPort = 47801;
	static constexpr int32 MaxPlayers = 8;

	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual bool IsTickableWhenPaused() const override { return true; }

	// ---------------------------------------------------------------- a sala
	/** Ouve os anúncios das partidas na LAN (a lista do menu). */
	void StartBrowsing();
	void StopBrowsing();
	const TArray<FRTSLanGame>& FoundGames() const { return Games; }
	/** Cria a sala e anuncia na LAN. */
	bool Host(FString& OutError);
	/** Entra na sala do anfitrião (IP da lista ou digitado). */
	bool Join(const FString& Ip, int32 Port, FString& OutError);
	void Leave();
	/** Anfitrião: todos vão pro mapa e a partida começa a montar. */
	void StartMatch();
	/** A partida acabou: de volta pra sala, com os mesmos jogadores (o anfitrião leva todos junto). */
	void BackToLobby();
	/** Troca a minha cor pela próxima livre na paleta. */
	void CycleColor();
	/** A névoa da próxima partida (o anfitrião escolhe na sala; os outros vêem). */
	ERTSFogMode GetFogMode() const { return ERTSFogMode(FogMode); }
	void SetFogMode(ERTSFogMode Mode);
	/** (v4) O dinheiro inicial da próxima partida (o anfitrião escolhe na sala; os outros vêem), sem o × do -RTSFast. */
	int32 GetStartingMoney() const { return StartingMoney; }
	void SetStartingMoney(int32 Money);
	/** O time de uma vaga: a sua (qualquer um) ou a de qualquer vaga (o anfitrião). Volta pro 0 depois do 4. */
	void CycleTeam(uint8 Slot);
	/** O anfitrião: troca a cor de uma vaga de IA. */
	void CycleColorOf(uint8 Slot);
	/** O anfitrião: uma IA na próxima vaga livre / tirar a IA da vaga. */
	void AddAI();
	void RemoveAI(uint8 Slot);
	/** O anfitrião: sorteia 2 times (2 contra 2, 2 contra 1...). */
	void RandomTeams();
	// (06/11, a sala nova: cada campo com o valor exato, como no skirmish; o cliente só mexe na vaga dele, o anfitrião nas da IA e no
	// time e na base de todos)
	/** O anfitrião: a vaga aberta, fechada ou com uma IA no nível Difficulty (State: 0 aberta, 1 fechada, 2 IA). Tira a IA que estava. */
	void SetSlotState(uint8 Slot, int32 State, uint8 Difficulty = 1);
	void SetTeamOf(uint8 Slot, uint8 Team);
	void SetFactionOf(uint8 Slot, const FString& Faction);
	/** A base do mapa (−1 = sorteada); se outro já está nela, os dois trocam. */
	void SetBaseOf(uint8 Slot, int32 Base);
	/** A cor das 24 da sala (0xRRGGBB; 0 = automática); uma cor é de um só. */
	void SetColorOf(uint8 Slot, uint32 Rgb);
	/** O anfitrião: tira da sala quem entrou (ou a IA). */
	void Kick(uint8 Slot);
	/** O anfitrião: o mapa da sala (o nome do Maps.json); as bases voltam pra sorteadas. Falso se não existe ou não cabem todos. */
	bool SetMap(const FString& Name);
	const FString& GetMapName() const { return MapName; }
	/** As vagas fechadas (um bit por vaga). */
	uint8 GetClosedMask() const { return ClosedMask; }
	/** Quem entrou: o READY da sala. */
	void SetLobbyReady(bool bReady);
	/** O anfitrião: a contagem antes de começar (5..1; 0 = sem contagem); todos vêem. */
	void SetCountdown(int32 Seconds);
	int32 GetCountdown() const { return Countdown; }
	/** A partida: as vagas na ordem dela (a mesma montagem do skirmish local: IA, time, base, facção, cor, nível). */
	const TArray<FRTSLocalSlot>* MatchSlots() const;
	/** Dá pra começar: 2 ou mais na sala, todos de volta, e pelo menos 2 lados. */
	bool CanStart() const;
	/** Vagas do mapa (as largadas): a sala não passa disso. */
	int32 SlotLimit() const { return MapSlots; }
	bool IsAISlot(uint8 Slot) const;
	uint8 TeamOfSlot(uint8 Slot) const;
	/** Anfitrião: a ordem de uma IA dele, carimbada com a vaga dela. */
	void SubmitAs(uint8 Player, const Sim::Command& C);
	/** A cor de cada vaga (pra partida: a paleta igual em todos os PCs). */
	uint8 ColorOfSlot(uint8 Slot) const;
	/** Todos na sala (ninguém mais no resultado da partida anterior). */
	bool EveryoneInLobby() const;
	/** Quantas partidas esta sala já jogou (o robô do teste sai depois da 1ª). */
	int32 MatchesPlayed() const { return Matches; }

	bool IsActive() const { return Role != ERTSNetRole::None; }
	bool IsHost() const { return Role == ERTSNetRole::Host; }
	ERTSNetRole GetRole() const { return Role; }
	ERTSNetPhase GetPhase() const { return Phase; }
	const TArray<FRTSNetMember>& Members() const { return Lobby; }
	uint8 LocalSlot() const { return MySlot; }
	int32 PlayerCount() const { return Lobby.Num(); }
	/** A frase da sala ("Esperando o anfitrião...", o erro). */
	FString StatusText() const;
	FString PlayerName() const;

	// ---------------------------------------------------------------- a partida
	/** Anfitrião: a montagem da partida (WriteSetup) pra mandar a todos. */
	void SetSetup(TArray<uint8>&& Bytes);
	/** Cliente: a montagem chegou inteira. */
	bool HasSetup() const { return bSetupComplete; }
	const TArray<uint8>& SetupBytes() const { return Setup; }
	/** A cena carregou e a montagem está aqui: pronto pra começar. */
	void SetLocalReady();
	/** Anfitrião: todos prontos (e ele já mandou começar). Cliente: o anfitrião mandou começar. */
	bool CanStartTicking() const { return bGo; }

	/** O comando local: o cliente manda pro anfitrião; o anfitrião guarda pro próximo passo. */
	void SubmitLocal(const Sim::Command& C);
	/** Anfitrião: fecha o passo T com o que chegou (carimbado com a vaga de quem mandou). */
	void SealTick(Sim::TickNumber T);
	/** Quantos passos já fechados o anfitrião tem (o próximo a fechar). */
	Sim::TickNumber NextSealTick() const { return Sim::TickNumber(Sealed.Num()); }
	/** O passo T já chegou (ou foi fechado aqui)? */
	bool HasTick(Sim::TickNumber T) const { return int64(T) < int64(Sealed.Num()); }
	/** Os comandos do passo T, na ordem do anfitrião. */
	std::vector<Sim::Command> TickCommands(Sim::TickNumber T) const;
	/** Quantos passos fechados há à frente do passo T (a folga do cliente). */
	int32 Ahead(Sim::TickNumber T) const { return FMath::Max(0, Sealed.Num() - int32(T)); }

	/** A impressão digital do estado depois do passo T (confere com o anfitrião). */
	void ReportHash(Sim::TickNumber T, uint64 Hash);
	bool IsDesynced() const { return DesyncTick >= 0; }
	int64 GetDesyncTick() const { return DesyncTick; }
	/** Quantas vezes a impressão digital de outro jogo bateu com a minha (a prova da sincronia). */
	int64 HashMatches() const { return Matched; }
	/** Alguém sumiu (8 s sem notícia na partida). */
	bool IsPeerLost() const { return bPeerLost; }
	/** Ida e volta do pior jogador (ms), pra tela. */
	int32 WorstRttMs() const;
	/** A partida acabou: a sala continua até sair. */
	void SetEnded() { Phase = ERTSNetPhase::Ended; }

	// ---------------------------------------------------------------- a pausa e a queda (N5, 13/10)
	/** A pausa da partida em rede: o anfitrião decide e todos vêem o mesmo. Parado, ninguém passa do mesmo passo,
	 *  porque o anfitrião (o relógio) não fecha passo novo. */
	struct FPauseState
	{
		bool bPaused = false;
		/** Quem pausou (255 = a espera por quem sumiu). */
		uint8 BySlot = 255;
		/** A contagem pra voltar (s; < 0 = sem contagem). */
		float ResumeIn = -1.0f;
		/** A vaga sem notícia há 5 s ou mais (255 = ninguém) e quanto falta pra ela cair (s). */
		uint8 WaitingFor = 255;
		float WaitLeft = 0.0f;
		/** Os votos pra derrubar quem sumiu (um bit por vaga) e quantos precisa (todos os outros). */
		uint8 Votes = 0;
		uint8 VotesNeeded = 0;
		/** As vagas que caíram (um bit por vaga). */
		uint8 Dropped = 0;
	};
	const FPauseState& GetPause() const { return Pause; }
	bool IsNetPaused() const { return Pause.bPaused; }
	/** Cliente: o anfitrião sem notícia há 5 s ou mais (o jogo dele engasgou ou caiu): a partida espera até 60 s. */
	float HostSilentSeconds() const;
	/** Qualquer jogador pausa; qualquer um retoma (uma contagem de 3 s); na LAN, sem limite de pausas. */
	void RequestPause();
	void RequestResume();
	/** O voto pra derrubar quem sumiu (a vaga que a partida espera). */
	void VoteDrop(uint8 Slot);

	// ---------------------------------------------------------------- o chat da partida (17/10)
	/** Uma linha do chat: quem mandou (a vaga), o texto, se foi só pros aliados e quando chegou (FPlatformTime). */
	struct FChatLine
	{
		uint8 Slot = 0;
		FString Text;
		bool bTeam = false;
		double Time = 0.0;
	};
	/** Manda uma linha. Fora da simulação (não mexe no lockstep): o cliente manda pro anfitrião e o anfitrião repassa;
	 *  só pros aliados vai só pra quem é do mesmo time. Sem rede (a escaramuça), fica só aqui. */
	void SendChat(const FString& Text, bool bTeam);
	const TArray<FChatLine>& ChatLines() const { return Chat; }
	/** (06/11) Quantas linhas já chegaram nesta sala (só sobe; a lista guarda as últimas 50): a sala nova sabe quais são novas. */
	int32 ChatTotalCount() const { return ChatTotal; }

private:
	TArray<FChatLine> Chat;
	int32 ChatTotal = 0;
	// (a vaga << 16) | o número: a mesma linha chega 3 vezes (UDP). (06/11) O anfitrião numera de novo tudo o que repassa (o número
	// dele não se repete na sala); as minhas levam o bit 31 (as minhas e as dos outros nunca colidem, nem depois das vagas mudarem)
	TSet<uint32> ChatSeen;
	uint16 NextChatId = 1;
	void AddChat(uint8 Slot, const FString& Text, bool bTeam);
	/** Anfitrião: repassa a linha pra quem deve ver (menos quem mandou). */
	void RelayChat(uint8 FromSlot, uint16 Id, bool bTeam, const FString& Text);
	/** Quem vê a linha de uma vaga: todos, ou só quem é do mesmo time (time 0 = sem aliados). */
	bool SeesChat(uint8 Viewer, uint8 FromSlot, bool bTeam) const;
	struct FPeer
	{
		TSharedPtr<FInternetAddr> Addr;
		FString Key;            // ip:porta
		FString Name;
		uint8 Slot = 0;
		double LastHeard = 0.0;
		bool bReady = false;
		int64 AckTick = -1;     // o último passo contínuo que ele confirmou
		uint32 NextInputSeq = 0;
		TArray<Sim::Command> Inbox;   // os comandos dele que chegaram e ainda não entraram num passo
		int32 SetupAcked = 0;
		double LastPingSent = 0.0;
		int32 RttMs = 0;
		uint8 Color = 0;
		uint8 Team = 0;
		FString Faction = TEXT("random");
		int32 Base = -1;
		uint32 Rgb = 0;
		bool bLobbyReady = false;
		bool bInLobby = true;
		/** Caiu da partida (sumiu 60 s, votado, ou saiu): o anfitrião mandou o render-se por ele. */
		bool bDropped = false;
	};

	void Pump();
	void Receive(const uint8* Data, int32 Size, const FInternetAddr& From);
	void SendTo(const FInternetAddr& To, const TArray<uint8>& Packet);
	void SendHost(const TArray<uint8>& Packet);
	void SendLobby();
	void SendBeacon();
	void SendSetupChunks(FPeer& P);
	void SendTicks(FPeer& P);
	void SendInputs();
	void RebuildLobby();
	void ResetMatchData();
	uint8 FirstFreeColor(uint8 From, uint8 Except) const;
	void CompareHash(int64 T, uint64 Remote);
	/** Anfitrião: um pedido de pausa (1), volta (2) ou voto (3, Arg = a vaga) da vaga Slot. */
	void HostControl(uint8 Slot, uint8 Kind, uint8 Arg);
	void UpdatePauseHost(float DeltaTime);
	void DropPeer(FPeer& P, const TCHAR* Why);
	void SendPause();
	FPeer* FindPeer(const FInternetAddr& From);
	uint64 VersionHash() const;
	void CloseSockets();

	ERTSNetRole Role = ERTSNetRole::None;
	ERTSNetPhase Phase = ERTSNetPhase::Idle;
	FSocket* GameSocket = nullptr;
	FSocket* BeaconSocket = nullptr;
	TSharedPtr<FInternetAddr> HostAddr;
	TArray<FPeer> Peers;
	TArray<FRTSNetMember> Lobby;
	TArray<FRTSLanGame> Games;
	uint8 MySlot = 0;
	uint8 MyColor = 0;
	uint8 MyTeam = 0;
	uint8 FogMode = uint8(ERTSFogMode::Explored);
	int32 StartingMoney = 10000;
	int32 MapSlots = MaxPlayers;
	/** As vagas de computador (só no anfitrião; viajam na sala como membros). */
	struct FAiSlot
	{
		uint8 Slot = 0;
		uint8 Color = 0;
		uint8 Team = 0;
		FString Faction = TEXT("random");
		int32 Base = -1;
		uint32 Rgb = 0;
		uint8 Difficulty = 1;
	};
	TArray<FAiSlot> Ais;
	// (06/11) a minha vaga (o anfitrião é a 0), as fechadas e o mapa da sala
	FString MyFaction = TEXT("random");
	int32 MyBase = -1;
	uint32 MyRgb = 0;
	uint8 ClosedMask = 0;
	FString MapName;
	uint8 Countdown = 0;
	mutable TArray<FRTSLocalSlot> MatchSlotList;
	/** O anfitrião: aplica um campo numa vaga (0 time, 1 facção, 2 base, 3 cor), conferindo quem pode. */
	void HostSetField(uint8 Slot, uint8 Field, int32 Value, const FString& Text);
	/** O cliente: pede ao anfitrião um campo da vaga dele. */
	void ClientSetField(uint8 Field, int32 Value, const FString& Text);
	/** A base / a cor de uma vaga (o anfitrião), como ponteiros pro campo dela (nullptr = vaga vazia). */
	int32* BaseRef(uint8 Slot);
	uint32* RgbRef(uint8 Slot);
	uint8 FirstFreeSlot() const;
	/** Cliente: o anfitrião voltou pra sala (a partida acabou): todos voltam junto. */
	bool bHostInLobby = false;
	/** A rodada: cada partida da sala tem a sua; pacote de outra rodada (atrasado) não entra. */
	uint8 Epoch = 0;
	int32 Matches = 0;
	FString Error;
	FString HostName;
	double Clock = 0.0;
	double LastBeacon = 0.0;
	double LastLobby = 0.0;
	double LastSend = 0.0;
	double LastHeardHost = 0.0;
	bool bPeerLost = false;
	mutable uint64 CachedVersion = 0;

	// a montagem
	TArray<uint8> Setup;
	TArray<bool> SetupGot;
	int32 SetupTotal = 0;
	bool bSetupComplete = false;
	bool bLocalReady = false;
	bool bGo = false;
	bool bStartLoadSeen = false;

	// os passos fechados (no anfitrião: os que ele fechou; no cliente: os contínuos que chegaram)
	TArray<TArray<Sim::Command>> Sealed;
	TMap<int64, TArray<Sim::Command>> Early;   // cliente: chegou fora de ordem
	TArray<Sim::Command> LocalInbox;           // anfitrião: os comandos dele pro próximo passo
	// cliente: os comandos ainda não confirmados pelo anfitrião
	TArray<TPair<uint32, Sim::Command>> Outbox;
	uint32 NextSeq = 0;
	uint32 HostAckedSeq = 0;

	// a impressão digital: as minhas e as dos outros (uma por jogador no anfitrião), até conferir
	TMap<int64, uint64> LocalHashes;
	TMultiMap<int64, uint64> RemoteHashes;
	int64 LastLocalHashTick = -1;
	uint64 LastLocalHash = 0;
	int64 DesyncTick = -1;
	int64 Matched = 0;

	FPauseState Pause;
	bool bPauseDirty = false;
	double LastPauseSend = 0.0;
	double WaitStart = 0.0;
};
