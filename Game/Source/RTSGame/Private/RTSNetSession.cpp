#include "RTSNetSession.h"

#include "Common/UdpSocketBuilder.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "RTSGameData.h"
#include "RTSSessionSubsystem.h"
#include "Sim/Types.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

#define LOCTEXT_NAMESPACE "RTSNet"

namespace
{
	constexpr uint32 Magic = 0x31535452;   // "RTS1"
	// o pacote fica abaixo de ~1100 B (o limite seguro da internet; o EOS aceita 1170)
	constexpr int32 MaxPacket = 1100;
	constexpr int32 ChunkBytes = 1000;
	constexpr int32 SetupWindowChunks = 48;
	constexpr int32 MaxTicksPerPacket = 90;
	constexpr double SendInterval = 1.0 / 60.0;
	constexpr double LostSeconds = 8.0;
	constexpr int64 HashKeepTicks = 900;

	enum class EMsg : uint8
	{
		Beacon = 1, Hello, Welcome, Reject, Lobby, SetupChunk, SetupAck, Ready, Go, Input, Ticks, Ping, Pong, Bye, Color, Team
	};

	void WriteString(Sim::ByteWriter& W, const FString& S)
	{
		const FTCHARToUTF8 U(*S);
		const int32 N = FMath::Min(U.Length(), 64);
		W.U16(uint16(N));
		W.Raw(reinterpret_cast<const uint8_t*>(U.Get()), size_t(N));
	}

	bool ReadString(Sim::ByteReader& R, FString& Out)
	{
		uint16 N = 0;
		if (!R.U16(N) || N > 64)
		{
			return false;
		}
		uint8_t Buf[65] = {};
		if (!R.Raw(Buf, N))
		{
			return false;
		}
		Out = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buf)));
		return true;
	}

	Sim::ByteWriter Begin(EMsg Type)
	{
		Sim::ByteWriter W;
		W.U32(Magic);
		W.U8(uint8(Type));
		return W;
	}

	TArray<uint8> ToArray(const Sim::ByteWriter& W)
	{
		return TArray<uint8>(W.Bytes.data(), int32(W.Bytes.size()));
	}

	/** O comando em bytes, com o tamanho na frente. */
	void WriteSizedCommand(Sim::ByteWriter& W, const Sim::Command& C)
	{
		Sim::ByteWriter Inner;
		Sim::WriteCommand(Inner, C);
		W.U16(uint16(Inner.Bytes.size()));
		W.Raw(Inner.Bytes.data(), Inner.Bytes.size());
	}

	bool ReadSizedCommand(Sim::ByteReader& R, Sim::Command& Out)
	{
		uint16 N = 0;
		if (!R.U16(N) || N > 4096)
		{
			return false;
		}
		TArray<uint8> Buf;
		Buf.SetNumUninitialized(N);
		if (!R.Raw(Buf.GetData(), N))
		{
			return false;
		}
		Sim::ByteReader Inner(Buf.GetData(), N);
		return Sim::ReadCommand(Inner, Out);
	}

	size_t SizedCommandBytes(const Sim::Command& C)
	{
		Sim::ByteWriter Inner;
		Sim::WriteCommand(Inner, C);
		return Inner.Bytes.size() + 2;
	}

	ISocketSubsystem* Sockets()
	{
		return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	}

	FSocket* MakeSocket(const TCHAR* Name, int32 Port, bool bBroadcast)
	{
		FUdpSocketBuilder B(Name);
		B.AsNonBlocking().AsReusable().WithReceiveBufferSize(1 << 20).WithSendBufferSize(1 << 20);
		if (bBroadcast)
		{
			B.WithBroadcast();
		}
		B.BoundToPort(Port);
		return B.Build();
	}
}

// ---------------------------------------------------------------- ciclo de vida

void URTSNetSession::Deinitialize()
{
	Leave();
	StopBrowsing();
	Super::Deinitialize();
}

TStatId URTSNetSession::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URTSNetSession, STATGROUP_Tickables);
}

uint64 URTSNetSession::VersionHash() const
{
	// a mesma simulação e as mesmas regras (os números do Units.json), senão recusa na entrada
	if (CachedVersion == 0)
	{
		FRTSGameData Data;
		FString Err;
		const uint64 Rules = Data.Load(Err) ? Data.Rules.Hash() : 0;
		// (0002, 11/10: a sala ganhou time, IA e a névoa de 3 tipos; o jogo velho não entra)
		CachedVersion = (Rules * 0x9E3779B97F4A7C15ull) ^ uint64(Sim::SimVersion) ^ 0x5254534C414E0002ull;
	}
	return CachedVersion;
}

FString URTSNetSession::PlayerName() const
{
	return FPlatformProcess::ComputerName();
}

FString URTSNetSession::StatusText() const
{
	if (!Error.IsEmpty())
	{
		return Error;
	}
	if (Role == ERTSNetRole::Host)
	{
		return Lobby.Num() < 2 ? LOCTEXT("WaitingPlayers", "Waiting for players to join...").ToString()
			: LOCTEXT("ReadyToStart", "Ready: press START").ToString();
	}
	if (Role == ERTSNetRole::Client)
	{
		return Lobby.Num() == 0 ? LOCTEXT("Connecting", "Connecting...").ToString()
			: LOCTEXT("WaitingHost", "Waiting for the host to start").ToString();
	}
	return FString();
}

void URTSNetSession::CloseSockets()
{
	if (GameSocket)
	{
		GameSocket->Close();
		Sockets()->DestroySocket(GameSocket);
		GameSocket = nullptr;
	}
}

void URTSNetSession::StartBrowsing()
{
	if (BeaconSocket)
	{
		return;
	}
	BeaconSocket = MakeSocket(TEXT("RTSLanBrowse"), BeaconPort, true);
	Games.Reset();
	if (!BeaconSocket)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS LAN: não deu pra ouvir a porta %d (outro jogo aberto?)"), BeaconPort);
	}
}

void URTSNetSession::StopBrowsing()
{
	if (BeaconSocket)
	{
		BeaconSocket->Close();
		Sockets()->DestroySocket(BeaconSocket);
		BeaconSocket = nullptr;
	}
}

bool URTSNetSession::Host(FString& OutError)
{
	Leave();
	GameSocket = MakeSocket(TEXT("RTSLanHost"), GamePort, true);
	if (!GameSocket)
	{
		OutError = FString::Printf(TEXT("Couldn't open UDP port %d"), GamePort);
		Error = OutError;
		return false;
	}
	Role = ERTSNetRole::Host;
	Phase = ERTSNetPhase::Lobby;
	MySlot = 0;
	MyColor = 0;
	MyTeam = 0;
	Ais.Reset();
	if (const URTSSessionSubsystem* S = GetGameInstance()->GetSubsystem<URTSSessionSubsystem>())
	{
		FogMode = uint8(S->FogMode);
	}
	{
		// a sala não passa das largadas do mapa (4 no de hoje)
		FRTSGameData Data;
		FString Err;
		const FRTSMapInfo* Info = Data.Load(Err) ? Data.FindMap(URTSSessionSubsystem::SkirmishMapName()) : nullptr;
		MapSlots = Info ? FMath::Clamp(Info->StartsM.Num(), 2, int32(MaxPlayers)) : int32(MaxPlayers);
	}
	HostName = PlayerName();
	Error.Reset();
	RebuildLobby();
	UE_LOG(LogTemp, Log, TEXT("RTS LAN: sala criada na porta %d (%s)"), GamePort, *HostName);
	return true;
}

bool URTSNetSession::Join(const FString& Ip, int32 Port, FString& OutError)
{
	Leave();
	bool bValid = false;
	HostAddr = Sockets()->CreateInternetAddr();
	HostAddr->SetIp(*Ip, bValid);
	HostAddr->SetPort(Port);
	if (!bValid)
	{
		OutError = FString::Printf(TEXT("Invalid address: %s"), *Ip);
		Error = OutError;
		return false;
	}
	GameSocket = MakeSocket(TEXT("RTSLanClient"), 0, false);
	if (!GameSocket)
	{
		OutError = TEXT("Couldn't open a UDP socket");
		Error = OutError;
		return false;
	}
	Role = ERTSNetRole::Client;
	Phase = ERTSNetPhase::Idle;
	Error.Reset();
	LastHeardHost = Clock;
	UE_LOG(LogTemp, Log, TEXT("RTS LAN: entrando em %s:%d"), *Ip, Port);
	return true;
}

uint8 URTSNetSession::ColorOfSlot(uint8 Slot) const
{
	for (const FRTSNetMember& M : Lobby)
	{
		if (M.Slot == Slot)
		{
			return M.Color;
		}
	}
	return Slot;
}

bool URTSNetSession::EveryoneInLobby() const
{
	return !Lobby.ContainsByPredicate([](const FRTSNetMember& M) { return !M.bInLobby; });
}

uint8 URTSNetSession::FirstFreeColor(uint8 From, uint8 Except) const
{
	// a próxima cor da paleta que ninguém (além de quem troca) está usando
	for (int32 K = 0; K < MaxPlayers; ++K)
	{
		const uint8 C = uint8((From + K) % MaxPlayers);
		bool bTaken = false;
		if (Role == ERTSNetRole::Host)
		{
			bTaken = (Except != 0 && MyColor == C);
			for (const FPeer& P : Peers)
			{
				bTaken |= P.Slot != Except && P.Color == C;
			}
			for (const FAiSlot& A : Ais)
			{
				bTaken |= A.Slot != Except && A.Color == C;
			}
		}
		if (!bTaken)
		{
			return C;
		}
	}
	return From;
}

void URTSNetSession::CycleColor()
{
	if (Role == ERTSNetRole::Host)
	{
		MyColor = FirstFreeColor(uint8((MyColor + 1) % MaxPlayers), 0);
		RebuildLobby();
		SendLobby();
	}
	else if (Role == ERTSNetRole::Client)
	{
		// o pedido vai pro anfitrião, que confere se a cor está livre
		Sim::ByteWriter W = Begin(EMsg::Color);
		W.U8(uint8((MyColor + 1) % MaxPlayers));
		SendHost(ToArray(W));
	}
}

void URTSNetSession::SetFogMode(ERTSFogMode Mode)
{
	if (Role == ERTSNetRole::Host)
	{
		FogMode = uint8(Mode);
		if (URTSSessionSubsystem* S = GetGameInstance()->GetSubsystem<URTSSessionSubsystem>())
		{
			S->FogMode = Mode;   // a montagem leva a regra pra todos; o desenho, a sala
		}
		SendLobby();
	}
}

uint8 URTSNetSession::FirstFreeSlot() const
{
	uint8 Slot = 1;
	while (Peers.ContainsByPredicate([Slot](const FPeer& X) { return X.Slot == Slot; })
		|| Ais.ContainsByPredicate([Slot](const FAiSlot& X) { return X.Slot == Slot; }))
	{
		++Slot;
	}
	return Slot;
}

bool URTSNetSession::IsAISlot(uint8 Slot) const
{
	const FRTSNetMember* M = Lobby.FindByPredicate([Slot](const FRTSNetMember& X) { return X.Slot == Slot; });
	return M && M->bAI;
}

uint8 URTSNetSession::TeamOfSlot(uint8 Slot) const
{
	const FRTSNetMember* M = Lobby.FindByPredicate([Slot](const FRTSNetMember& X) { return X.Slot == Slot; });
	return M ? M->Team : 0;
}

void URTSNetSession::CycleTeam(uint8 Slot)
{
	if (Phase != ERTSNetPhase::Lobby)
	{
		return;
	}
	if (Role == ERTSNetRole::Host)
	{
		// o anfitrião mexe em qualquer vaga (escolha do Felipe, 11/10: cada um o seu, o anfitrião ajusta)
		const auto Next = [](uint8 T) { return uint8((T + 1) % 5); };
		if (Slot == 0)
		{
			MyTeam = Next(MyTeam);
		}
		else if (FPeer* P = Peers.FindByPredicate([Slot](const FPeer& X) { return X.Slot == Slot; }))
		{
			P->Team = Next(P->Team);
		}
		else if (FAiSlot* A = Ais.FindByPredicate([Slot](const FAiSlot& X) { return X.Slot == Slot; }))
		{
			A->Team = Next(A->Team);
		}
		RebuildLobby();
		SendLobby();
	}
	else if (Role == ERTSNetRole::Client && Slot == MySlot)
	{
		Sim::ByteWriter W = Begin(EMsg::Team);
		W.U8(uint8((MyTeam + 1) % 5));
		SendHost(ToArray(W));
	}
}

void URTSNetSession::CycleColorOf(uint8 Slot)
{
	if (Role != ERTSNetRole::Host || Phase != ERTSNetPhase::Lobby)
	{
		return;
	}
	if (Slot == 0)
	{
		CycleColor();
	}
	else if (FAiSlot* A = Ais.FindByPredicate([Slot](const FAiSlot& X) { return X.Slot == Slot; }))
	{
		A->Color = FirstFreeColor(uint8((A->Color + 1) % MaxPlayers), Slot);
		RebuildLobby();
		SendLobby();
	}
}

void URTSNetSession::AddAI()
{
	if (Role != ERTSNetRole::Host || Phase != ERTSNetPhase::Lobby || Lobby.Num() >= MapSlots)
	{
		return;
	}
	FAiSlot A;
	A.Slot = FirstFreeSlot();
	A.Color = FirstFreeColor(A.Slot, A.Slot);
	Ais.Add(A);
	RebuildLobby();
	SendLobby();
	UE_LOG(LogTemp, Log, TEXT("RTS LAN: IA na vaga %d"), A.Slot);
}

void URTSNetSession::RemoveAI(uint8 Slot)
{
	if (Role == ERTSNetRole::Host && Phase == ERTSNetPhase::Lobby && Ais.RemoveAll([Slot](const FAiSlot& X) { return X.Slot == Slot; }) > 0)
	{
		RebuildLobby();
		SendLobby();
	}
}

void URTSNetSession::RandomTeams()
{
	if (Role != ERTSNetRole::Host || Phase != ERTSNetPhase::Lobby || Lobby.Num() < 2)
	{
		return;
	}
	// a ordem embaralhada, e os times 1 e 2 alternando (4 → 2 contra 2; 3 → 2 contra 1)
	TArray<uint8> Slots;
	for (const FRTSNetMember& M : Lobby)
	{
		Slots.Add(M.Slot);
	}
	for (int32 K = Slots.Num() - 1; K > 0; --K)
	{
		Slots.Swap(K, FMath::RandRange(0, K));
	}
	for (int32 K = 0; K < Slots.Num(); ++K)
	{
		const uint8 T = uint8(1 + K % 2);
		const uint8 Slot = Slots[K];
		if (Slot == 0)
		{
			MyTeam = T;
		}
		else if (FPeer* P = Peers.FindByPredicate([Slot](const FPeer& X) { return X.Slot == Slot; }))
		{
			P->Team = T;
		}
		else if (FAiSlot* A = Ais.FindByPredicate([Slot](const FAiSlot& X) { return X.Slot == Slot; }))
		{
			A->Team = T;
		}
	}
	RebuildLobby();
	SendLobby();
}

bool URTSNetSession::CanStart() const
{
	if (Role != ERTSNetRole::Host || Phase != ERTSNetPhase::Lobby || Lobby.Num() < 2 || Lobby.Num() > MapSlots || !EveryoneInLobby())
	{
		return false;
	}
	// pelo menos 2 lados: quem está sem time é um lado sozinho
	TSet<uint8> Teams;
	int32 Alone = 0;
	for (const FRTSNetMember& M : Lobby)
	{
		if (M.Team == 0)
		{
			++Alone;
		}
		else
		{
			Teams.Add(M.Team);
		}
	}
	return Alone + Teams.Num() >= 2;
}

void URTSNetSession::SubmitAs(uint8 Player, const Sim::Command& C)
{
	if (Role == ERTSNetRole::Host)
	{
		Sim::Command Stamped = C;
		Stamped.Player = Player;
		LocalInbox.Add(Stamped);
	}
}

void URTSNetSession::ResetMatchData()
{
	Setup.Reset();
	SetupGot.Reset();
	SetupTotal = 0;
	bSetupComplete = false;
	bLocalReady = false;
	bGo = false;
	bStartLoadSeen = false;
	Sealed.Reset();
	Early.Reset();
	LocalInbox.Reset();
	Outbox.Reset();
	NextSeq = 0;
	HostAckedSeq = 0;
	LocalHashes.Reset();
	RemoteHashes.Reset();
	LastLocalHashTick = -1;
	DesyncTick = -1;
	Matched = 0;
	bPeerLost = false;
	for (FPeer& P : Peers)
	{
		P.bReady = false;
		P.AckTick = -1;
		P.NextInputSeq = 0;
		P.Inbox.Reset();
		P.SetupAcked = 0;
	}
}

void URTSNetSession::BackToLobby()
{
	if (Role == ERTSNetRole::None)
	{
		return;
	}
	ResetMatchData();
	Phase = ERTSNetPhase::Lobby;
	bHostInLobby = Role == ERTSNetRole::Host;
	if (Role == ERTSNetRole::Host)
	{
		for (FPeer& P : Peers)
		{
			P.bInLobby = false;   // até o "estou na sala" de cada um
			P.LastHeard = Clock;
		}
		RebuildLobby();
		SendLobby();   // os clientes no resultado vêem a sala e voltam junto
	}
	UE_LOG(LogTemp, Log, TEXT("RTS LAN: de volta na sala"));
}

void URTSNetSession::Leave()
{
	if (GameSocket)
	{
		const TArray<uint8> Bye = ToArray(Begin(EMsg::Bye));
		if (Role == ERTSNetRole::Client && HostAddr)
		{
			SendTo(*HostAddr, Bye);
		}
		for (FPeer& P : Peers)
		{
			SendTo(*P.Addr, Bye);
		}
	}
	CloseSockets();
	Role = ERTSNetRole::None;
	Phase = ERTSNetPhase::Idle;
	Peers.Reset();
	Lobby.Reset();
	HostAddr.Reset();
	MySlot = 0;
	MyColor = 0;
	MyTeam = 0;
	Ais.Reset();
	bHostInLobby = false;
	ResetMatchData();
}

void URTSNetSession::StartMatch()
{
	if (!CanStart())
	{
		return;
	}
	// as vagas ficam contínuas (0..N-1), na ordem da sala, com as IAs no meio
	TArray<TPair<uint8, int32>> Order;   // vaga antiga, índice (≥ 0 = jogador, < 0 = IA)
	for (int32 K = 0; K < Peers.Num(); ++K)
	{
		Order.Add(TPair<uint8, int32>(Peers[K].Slot, K));
	}
	for (int32 K = 0; K < Ais.Num(); ++K)
	{
		Order.Add(TPair<uint8, int32>(Ais[K].Slot, -1 - K));
	}
	Order.Sort([](const TPair<uint8, int32>& A, const TPair<uint8, int32>& B) { return A.Key < B.Key; });
	for (int32 K = 0; K < Order.Num(); ++K)
	{
		const uint8 NewSlot = uint8(K + 1);
		if (Order[K].Value >= 0)
		{
			Peers[Order[K].Value].Slot = NewSlot;
			Peers[Order[K].Value].bInLobby = false;
		}
		else
		{
			Ais[-1 - Order[K].Value].Slot = NewSlot;
		}
	}
	Peers.Sort([](const FPeer& A, const FPeer& B) { return A.Slot < B.Slot; });
	ResetMatchData();
	++Epoch;
	++Matches;
	Phase = ERTSNetPhase::Loading;
	SendLobby();   // os clientes vêem a fase (e a vaga deles) e vão pro mapa também
	UE_LOG(LogTemp, Log, TEXT("RTS LAN: começando com %d jogadores"), Lobby.Num());
	if (URTSSessionSubsystem* S = GetGameInstance()->GetSubsystem<URTSSessionSubsystem>())
	{
		S->StartSkirmish();
	}
}

void URTSNetSession::RebuildLobby()
{
	Lobby.Reset();
	FRTSNetMember Me;
	Me.Name = HostName;
	Me.Slot = 0;
	Me.bReady = bLocalReady;
	Me.Color = MyColor;
	Me.Team = MyTeam;
	Me.bInLobby = Phase == ERTSNetPhase::Lobby;
	Lobby.Add(Me);
	for (const FPeer& P : Peers)
	{
		FRTSNetMember M;
		M.Name = P.Name;
		M.Slot = P.Slot;
		M.bReady = P.bReady;
		M.Color = P.Color;
		M.Team = P.Team;
		M.bInLobby = P.bInLobby;
		Lobby.Add(M);
	}
	for (const FAiSlot& A : Ais)
	{
		FRTSNetMember M;
		M.Name = TEXT("AI");
		M.Slot = A.Slot;
		M.bReady = true;
		M.Color = A.Color;
		M.Team = A.Team;
		M.bAI = true;
		Lobby.Add(M);
	}
	Lobby.Sort([](const FRTSNetMember& A, const FRTSNetMember& B) { return A.Slot < B.Slot; });
}

// ---------------------------------------------------------------- a partida

void URTSNetSession::SetSetup(TArray<uint8>&& Bytes)
{
	Setup = MoveTemp(Bytes);
	SetupTotal = Setup.Num();
	bSetupComplete = true;
	for (FPeer& P : Peers)
	{
		P.SetupAcked = 0;
	}
	UE_LOG(LogTemp, Log, TEXT("RTS LAN: montagem pronta (%d bytes) pra mandar"), SetupTotal);
}

void URTSNetSession::SetLocalReady()
{
	bLocalReady = true;
	if (Role == ERTSNetRole::Host)
	{
		RebuildLobby();
	}
}

void URTSNetSession::SubmitLocal(const Sim::Command& C)
{
	if (Role == ERTSNetRole::Host)
	{
		Sim::Command Stamped = C;
		Stamped.Player = 0;
		LocalInbox.Add(Stamped);
	}
	else if (Role == ERTSNetRole::Client)
	{
		Outbox.Add(TPair<uint32, Sim::Command>(NextSeq++, C));
	}
}

void URTSNetSession::SealTick(Sim::TickNumber T)
{
	check(Role == ERTSNetRole::Host && int64(T) == int64(Sealed.Num()));
	// a ordem do passo: vaga por vaga, e dentro dela a ordem em que chegaram; o anfitrião
	// carimba o jogador pela vaga (um cliente modificado não comanda o exército do outro)
	TArray<Sim::Command> Cmds;
	for (Sim::Command C : LocalInbox)
	{
		C.Tick = T;   // a vaga já vem carimbada (0 = o anfitrião; as IAs, a delas)
		Cmds.Add(MoveTemp(C));
	}
	LocalInbox.Reset();
	Peers.Sort([](const FPeer& A, const FPeer& B) { return A.Slot < B.Slot; });
	for (FPeer& P : Peers)
	{
		for (Sim::Command C : P.Inbox)
		{
			C.Player = P.Slot;
			C.Tick = T;
			Cmds.Add(MoveTemp(C));
		}
		P.Inbox.Reset();
	}
	Sealed.Add(MoveTemp(Cmds));
}

std::vector<Sim::Command> URTSNetSession::TickCommands(Sim::TickNumber T) const
{
	std::vector<Sim::Command> Out;
	if (HasTick(T))
	{
		for (const Sim::Command& C : Sealed[int32(T)])
		{
			Out.push_back(C);
		}
	}
	return Out;
}

void URTSNetSession::ReportHash(Sim::TickNumber T, uint64 Hash)
{
	LocalHashes.Add(int64(T), Hash);
	LastLocalHashTick = int64(T);
	LastLocalHash = Hash;
	TArray<uint64> Remotes;
	RemoteHashes.MultiFind(int64(T), Remotes);
	for (const uint64 Remote : Remotes)
	{
		CompareHash(int64(T), Remote);
	}
	LocalHashes.Remove(int64(T) - HashKeepTicks);
	RemoteHashes.Remove(int64(T) - HashKeepTicks);
}

void URTSNetSession::CompareHash(int64 T, uint64 Remote)
{
	const uint64* Mine = LocalHashes.Find(T);
	if (!Mine)
	{
		RemoteHashes.AddUnique(T, Remote);   // a minha ainda não saiu: confere quando sair
		return;
	}
	Matched += *Mine == Remote ? 1 : 0;
	if (*Mine != Remote && DesyncTick < 0)
	{
		DesyncTick = T;
		UE_LOG(LogTemp, Error, TEXT("RTS LAN DESSINCRONIA no passo %lld: aqui %016llx, lá %016llx"), T, (unsigned long long)*Mine, (unsigned long long)Remote);
	}
}

int32 URTSNetSession::WorstRttMs() const
{
	int32 Worst = 0;
	for (const FPeer& P : Peers)
	{
		Worst = FMath::Max(Worst, P.RttMs);
	}
	return Worst;
}

// ---------------------------------------------------------------- rede

void URTSNetSession::SendTo(const FInternetAddr& To, const TArray<uint8>& Packet)
{
	if (GameSocket)
	{
		int32 Sent = 0;
		GameSocket->SendTo(Packet.GetData(), Packet.Num(), Sent, To);
	}
}

void URTSNetSession::SendHost(const TArray<uint8>& Packet)
{
	if (HostAddr)
	{
		SendTo(*HostAddr, Packet);
	}
}

URTSNetSession::FPeer* URTSNetSession::FindPeer(const FInternetAddr& From)
{
	const FString Key = From.ToString(true);
	return Peers.FindByPredicate([&Key](const FPeer& P) { return P.Key == Key; });
}

void URTSNetSession::SendBeacon()
{
	if (!GameSocket)
	{
		return;
	}
	Sim::ByteWriter W = Begin(EMsg::Beacon);
	WriteString(W, HostName);
	W.U16(uint16(GamePort));
	W.U8(uint8(Lobby.Num()));
	W.U8(Phase == ERTSNetPhase::Lobby ? 1 : 0);
	W.U64(VersionHash());
	const TArray<uint8> Packet = ToArray(W);
	// o anúncio pra rede toda, e pro próprio PC (dois jogos na mesma máquina, o teste)
	TSharedRef<FInternetAddr> All = Sockets()->CreateInternetAddr();
	All->SetBroadcastAddress();
	All->SetPort(BeaconPort);
	SendTo(*All, Packet);
	TSharedRef<FInternetAddr> Local = Sockets()->CreateInternetAddr();
	bool bValid = false;
	Local->SetIp(TEXT("127.0.0.1"), bValid);
	Local->SetPort(BeaconPort);
	SendTo(*Local, Packet);
}

void URTSNetSession::SendLobby()
{
	RebuildLobby();
	Sim::ByteWriter W = Begin(EMsg::Lobby);
	W.U8(uint8(Phase));
	W.U8(Epoch);
	W.U8(FogMode);
	W.U8(uint8(Lobby.Num()));
	for (const FRTSNetMember& M : Lobby)
	{
		W.U8(M.Slot);
		WriteString(W, M.Name);
		W.U8((M.bReady ? 1 : 0) | (M.bInLobby ? 2 : 0) | (M.bAI ? 4 : 0));
		W.U8(M.Color);
		W.U8(M.Team);
	}
	for (FPeer& P : Peers)
	{
		Sim::ByteWriter Per = W;
		Per.U8(P.Slot);   // a vaga de quem recebe (muda ao começar: ficam contínuas)
		SendTo(*P.Addr, ToArray(Per));
	}
}

void URTSNetSession::SendSetupChunks(FPeer& P)
{
	// uma janela de pedaços depois do último confirmado, de novo a cada envio (sem esperar o tempo)
	for (int32 K = 0; K < SetupWindowChunks; ++K)
	{
		const int32 Offset = P.SetupAcked + K * ChunkBytes;
		if (Offset >= SetupTotal)
		{
			break;
		}
		const int32 Len = FMath::Min(ChunkBytes, SetupTotal - Offset);
		Sim::ByteWriter W = Begin(EMsg::SetupChunk);
		W.U8(Epoch);
		W.U32(uint32(SetupTotal));
		W.U32(uint32(Offset));
		W.U16(uint16(Len));
		W.Raw(Setup.GetData() + Offset, size_t(Len));
		SendTo(*P.Addr, ToArray(W));
	}
}

void URTSNetSession::SendTicks(FPeer& P)
{
	Sim::ByteWriter W = Begin(EMsg::Ticks);
	W.U8(Epoch);
	W.U32(P.NextInputSeq);
	W.I64(LastLocalHashTick);
	W.U64(LastLocalHash);
	const int64 First = P.AckTick + 1;
	W.I64(First);
	// quantos passos cabem: os não confirmados, do mais velho pro mais novo
	int32 Count = 0;
	size_t Bytes = W.Bytes.size() + 2;
	for (int64 T = First; T < Sealed.Num() && Count < MaxTicksPerPacket; ++T)
	{
		size_t TickBytes = 2;
		for (const Sim::Command& C : Sealed[int32(T)])
		{
			TickBytes += SizedCommandBytes(C);
		}
		if (Count > 0 && Bytes + TickBytes > size_t(MaxPacket))
		{
			break;
		}
		Bytes += TickBytes;
		++Count;
	}
	W.U16(uint16(Count));
	for (int32 K = 0; K < Count; ++K)
	{
		const TArray<Sim::Command>& Cmds = Sealed[int32(First + K)];
		W.U16(uint16(Cmds.Num()));
		for (const Sim::Command& C : Cmds)
		{
			WriteSizedCommand(W, C);
		}
	}
	SendTo(*P.Addr, ToArray(W));
}

void URTSNetSession::SendInputs()
{
	Sim::ByteWriter W = Begin(EMsg::Input);
	W.U8(Epoch);
	W.I64(int64(Sealed.Num()) - 1);
	W.I64(LastLocalHashTick);
	W.U64(LastLocalHash);
	W.U32(HostAckedSeq);
	int32 Count = 0;
	size_t Bytes = W.Bytes.size() + 1;
	for (const TPair<uint32, Sim::Command>& E : Outbox)
	{
		const size_t N = SizedCommandBytes(E.Value);
		if (Count >= 200 || (Count > 0 && Bytes + N > size_t(MaxPacket)))
		{
			break;
		}
		Bytes += N;
		++Count;
	}
	W.U8(uint8(Count));
	for (int32 K = 0; K < Count; ++K)
	{
		WriteSizedCommand(W, Outbox[K].Value);
	}
	SendHost(ToArray(W));
}

void URTSNetSession::Tick(float DeltaTime)
{
	Clock = FPlatformTime::Seconds();
	Pump();
	if (Role == ERTSNetRole::None && !BeaconSocket)
	{
		return;
	}
	// a lista do menu: some a partida que parou de anunciar
	Games.RemoveAll([this](const FRTSLanGame& G) { return Clock - G.LastSeen > 3.0; });

	if (Role == ERTSNetRole::Host)
	{
		if (Clock - LastBeacon > 1.0 && Phase == ERTSNetPhase::Lobby)
		{
			LastBeacon = Clock;
			SendBeacon();
		}
		if (Clock - LastLobby > 0.5)
		{
			LastLobby = Clock;
			SendLobby();
			for (FPeer& P : Peers)
			{
				Sim::ByteWriter W = Begin(EMsg::Ping);
				W.U64(uint64(Clock * 1.0e6));
				SendTo(*P.Addr, ToArray(W));
			}
		}
		// quem sumiu: na sala sai; na partida, a partida avisa
		for (int32 K = Peers.Num() - 1; K >= 0; --K)
		{
			if (Clock - Peers[K].LastHeard > LostSeconds)
			{
				if (Phase == ERTSNetPhase::Lobby)
				{
					UE_LOG(LogTemp, Log, TEXT("RTS LAN: %s saiu (sem resposta)"), *Peers[K].Name);
					Peers.RemoveAt(K);
					RebuildLobby();
				}
				else if (Phase == ERTSNetPhase::Playing || Phase == ERTSNetPhase::Loading)
				{
					bPeerLost = true;
				}
			}
		}
		if (Clock - LastSend >= SendInterval)
		{
			LastSend = Clock;
			if (bSetupComplete)
			{
				for (FPeer& P : Peers)
				{
					if (P.SetupAcked < SetupTotal)
					{
						SendSetupChunks(P);
					}
				}
			}
			// todos prontos: começa (e o "comece" vai junto com os passos)
			if (!bGo && bLocalReady && Peers.Num() > 0 && !Peers.ContainsByPredicate([](const FPeer& P) { return !P.bReady; }))
			{
				bGo = true;
				Phase = ERTSNetPhase::Playing;
				UE_LOG(LogTemp, Log, TEXT("RTS LAN: todos prontos, a partida começa"));
			}
				if (bGo)
			{
				Sim::ByteWriter GoMsg = Begin(EMsg::Go);
				GoMsg.U8(Epoch);
				const TArray<uint8> GoPacket = ToArray(GoMsg);
				for (FPeer& P : Peers)
				{
					SendTo(*P.Addr, GoPacket);
					SendTicks(P);
				}
			}
		}
	}
	else if (Role == ERTSNetRole::Client)
	{
		if (Clock - LastSend >= SendInterval)
		{
			LastSend = Clock;
			if (MySlot == 0 && Lobby.Num() == 0)
			{
				// ainda sem resposta: o "olá" de novo
				Sim::ByteWriter W = Begin(EMsg::Hello);
				W.U64(VersionHash());
				WriteString(W, PlayerName());
				SendHost(ToArray(W));
			}
				if (bSetupComplete && bLocalReady && !bGo)
			{
				Sim::ByteWriter W = Begin(EMsg::Ready);
				W.U8(Epoch);
				SendHost(ToArray(W));
			}
			if (bGo)
			{
				SendInputs();
			}
			else if (Phase == ERTSNetPhase::Lobby || Phase == ERTSNetPhase::Loading)
			{
				// "ainda estou aqui", e onde (o anfitrião sabe quem já voltou pra sala)
				Sim::ByteWriter W = Begin(EMsg::Pong);
				W.U64(0);
				W.U8(uint8(Phase));
				SendHost(ToArray(W));
			}
		}
		// sem resposta: antes de entrar, segue tentando (o anfitrião pode abrir depois); na sala ou
		// na partida, a conexão caiu
		if (Clock - LastHeardHost > LostSeconds && Phase != ERTSNetPhase::Ended)
		{
			if (Phase == ERTSNetPhase::Idle)
			{
				Error = LOCTEXT("HostNotFound", "Looking for the host...").ToString();
			}
			else
			{
				bPeerLost = true;
			}
		}
	}
}

void URTSNetSession::Pump()
{
	uint8 Buf[2048];
	for (FSocket* S : { GameSocket, BeaconSocket })
	{
		if (!S)
		{
			continue;
		}
		uint32 Pending = 0;
		int32 Guard = 0;
		while (S->HasPendingData(Pending) && Guard++ < 512)
		{
			TSharedRef<FInternetAddr> From = Sockets()->CreateInternetAddr();
			int32 Read = 0;
			if (!S->RecvFrom(Buf, sizeof(Buf), Read, *From) || Read <= 0)
			{
				break;
			}
			Receive(Buf, Read, *From);
		}
	}
}

void URTSNetSession::Receive(const uint8* Data, int32 Size, const FInternetAddr& From)
{
	Sim::ByteReader R(Data, size_t(Size));
	uint32 M = 0;
	uint8 TypeByte = 0;
	if (!R.U32(M) || M != Magic || !R.U8(TypeByte))
	{
		return;
	}
	const EMsg Type = EMsg(TypeByte);

	if (Type == EMsg::Beacon)
	{
		FRTSLanGame G;
		uint16 Port = 0;
		uint8 Players = 0;
		uint8 Open = 0;
		uint64 Version = 0;
		if (!ReadString(R, G.Name) || !R.U16(Port) || !R.U8(Players) || !R.U8(Open) || !R.U64(Version))
		{
			return;
		}
		G.Ip = From.ToString(false);
		G.Port = Port;
		G.Players = Players;
		G.bOpen = Open != 0 && Version == VersionHash();
		G.LastSeen = Clock;
		if (FRTSLanGame* Old = Games.FindByPredicate([&G](const FRTSLanGame& X) { return X.Ip == G.Ip && X.Port == G.Port; }))
		{
			*Old = G;
		}
		else
		{
			Games.Add(G);
		}
		return;
	}

	if (Role == ERTSNetRole::Host)
	{
		FPeer* P = FindPeer(From);
		if (P)
		{
			P->LastHeard = Clock;
		}
		switch (Type)
		{
		case EMsg::Hello:
		{
			uint64 Version = 0;
			FString Name;
			if (!R.U64(Version) || !ReadString(R, Name))
			{
				return;
			}
			if (P)
			{
				Sim::ByteWriter W = Begin(EMsg::Welcome);
				W.U8(P->Slot);
				SendTo(From, ToArray(W));
				return;
			}
			auto Reject = [&](const FString& Why)
			{
				Sim::ByteWriter W = Begin(EMsg::Reject);
				WriteString(W, Why);
				SendTo(From, ToArray(W));
			};
			if (Version != VersionHash())
			{
				Reject(TEXT("Different game version"));
				return;
			}
			if (Phase != ERTSNetPhase::Lobby)
			{
				Reject(TEXT("The match already started"));
				return;
			}
			if (Lobby.Num() >= MapSlots)
			{
				Reject(TEXT("The room is full"));
				return;
			}
			const uint8 Slot = FirstFreeSlot();
			FPeer New;
			New.Addr = From.Clone();
			New.Key = From.ToString(true);
			New.Name = Name;
			New.Slot = Slot;
			New.LastHeard = Clock;
			New.Color = FirstFreeColor(Slot, Slot);
			Peers.Add(MoveTemp(New));
			RebuildLobby();
			Sim::ByteWriter W = Begin(EMsg::Welcome);
			W.U8(Slot);
			SendTo(From, ToArray(W));
			SendLobby();
			UE_LOG(LogTemp, Log, TEXT("RTS LAN: %s entrou (%s), vaga %d"), *Name, *From.ToString(true), Slot);
			return;
		}
		case EMsg::SetupAck:
		{
			uint8 E = 0;
			uint32 Got = 0;
			if (P && R.U8(E) && E == Epoch && R.U32(Got))
			{
				P->SetupAcked = FMath::Max(P->SetupAcked, int32(FMath::Min(Got, uint32(SetupTotal))));
			}
			return;
		}
		case EMsg::Ready:
		{
			uint8 E = 0;
			if (P && !P->bReady && R.U8(E) && E == Epoch && Phase == ERTSNetPhase::Loading)
			{
				P->bReady = true;
				RebuildLobby();
				UE_LOG(LogTemp, Log, TEXT("RTS LAN: %s está pronto"), *P->Name);
			}
			return;
		}
		case EMsg::Input:
		{
			uint8 E = 0;
			if (!P || !R.U8(E) || E != Epoch || !(Phase == ERTSNetPhase::Playing || Phase == ERTSNetPhase::Ended))
			{
				return;   // de outra rodada (atrasado) ou fora da partida
			}
			int64 Ack = -1;
			int64 HashTick = -1;
			uint64 Hash = 0;
			uint32 FirstSeq = 0;
			uint8 Count = 0;
			if (!P || !R.I64(Ack) || !R.I64(HashTick) || !R.U64(Hash) || !R.U32(FirstSeq) || !R.U8(Count))
			{
				return;
			}
			P->bReady = true;
			P->AckTick = FMath::Max(P->AckTick, FMath::Min(Ack, int64(Sealed.Num()) - 1));
			if (HashTick >= 0)
			{
				CompareHash(HashTick, Hash);
			}
			for (uint32 K = 0; K < Count; ++K)
			{
				Sim::Command C;
				if (!ReadSizedCommand(R, C))
				{
					return;
				}
				// só o próximo da sequência: o repetido (pacote duplicado ou reenviado) não entra duas vezes
				if (FirstSeq + K == P->NextInputSeq)
				{
					P->Inbox.Add(MoveTemp(C));
					++P->NextInputSeq;
				}
			}
			return;
		}
		case EMsg::Team:
		{
			uint8 Want = 0;
			if (P && R.U8(Want) && Phase == ERTSNetPhase::Lobby)
			{
				P->Team = uint8(Want % 5);
				RebuildLobby();
				SendLobby();
			}
			return;
		}
		case EMsg::Color:
		{
			uint8 Want = 0;
			if (P && R.U8(Want) && Phase == ERTSNetPhase::Lobby)
			{
				P->Color = FirstFreeColor(uint8(Want % MaxPlayers), P->Slot);
				RebuildLobby();
				SendLobby();
			}
			return;
		}
		case EMsg::Pong:
		{
			uint64 Sent = 0;
			if (P && R.U64(Sent) && Sent > 0)
			{
				P->RttMs = int32((Clock * 1.0e6 - double(Sent)) / 1000.0);
			}
			else if (uint8 Where = 0; P && R.U8(Where))
			{
				const bool bWas = P->bInLobby;
				P->bInLobby = ERTSNetPhase(Where) == ERTSNetPhase::Lobby;
				if (P->bInLobby && !bWas && Phase == ERTSNetPhase::Lobby)
				{
					UE_LOG(LogTemp, Log, TEXT("RTS LAN: %s voltou pra sala"), *P->Name);
					RebuildLobby();
				}
			}
			return;
		}
		case EMsg::Bye:
			if (P)
			{
				UE_LOG(LogTemp, Log, TEXT("RTS LAN: %s saiu"), *P->Name);
				if (Phase == ERTSNetPhase::Lobby)
				{
					const FString Key = P->Key;
					Peers.RemoveAll([&Key](const FPeer& X) { return X.Key == Key; });
					RebuildLobby();
				}
				else
				{
					bPeerLost = true;
				}
			}
			return;
		default:
			return;
		}
	}

	if (Role == ERTSNetRole::Client)
	{
		// só o anfitrião fala com o cliente
		if (!HostAddr || From.ToString(true) != HostAddr->ToString(true))
		{
			return;
		}
		LastHeardHost = Clock;
		switch (Type)
		{
		case EMsg::Welcome:
		{
			uint8 Slot = 0;
			if (R.U8(Slot) && Slot > 0)
			{
				MySlot = Slot;
				if (Phase == ERTSNetPhase::Idle)
				{
					Phase = ERTSNetPhase::Lobby;
				}
				Error.Reset();
			}
			return;
		}
		case EMsg::Reject:
		{
			FString Why;
			ReadString(R, Why);
			Error = Why;
			UE_LOG(LogTemp, Warning, TEXT("RTS LAN: recusado: %s"), *Why);
			CloseSockets();
			Role = ERTSNetRole::None;
			Phase = ERTSNetPhase::Idle;
			return;
		}
		case EMsg::Lobby:
		{
			uint8 HostPhase = 0;
			uint8 HostEpoch = 0;
			uint8 Fog = 1;
			uint8 Count = 0;
			if (!R.U8(HostPhase) || !R.U8(HostEpoch) || !R.U8(Fog) || Fog > uint8(ERTSFogMode::Explored) || !R.U8(Count) || Count > MaxPlayers)
			{
				return;
			}
			TArray<FRTSNetMember> List;
			for (uint8 K = 0; K < Count; ++K)
			{
				FRTSNetMember Mb;
				uint8 Flags = 0;
				if (!R.U8(Mb.Slot) || !ReadString(R, Mb.Name) || !R.U8(Flags) || !R.U8(Mb.Color) || !R.U8(Mb.Team))
				{
					return;
				}
				Mb.bReady = (Flags & 1) != 0;
				Mb.bInLobby = (Flags & 2) != 0;
				Mb.bAI = (Flags & 4) != 0;
				List.Add(Mb);
			}
			uint8 YourSlot = MySlot;
			if (R.U8(YourSlot) && YourSlot > 0)
			{
				MySlot = YourSlot;
			}
			Lobby = MoveTemp(List);
			FogMode = Fog;
			for (const FRTSNetMember& Mb : Lobby)
			{
				if (Mb.Slot == MySlot)
				{
					MyColor = Mb.Color;
					MyTeam = Mb.Team;
				}
			}
			bHostInLobby = ERTSNetPhase(HostPhase) == ERTSNetPhase::Lobby;
			// a partida acabou e o anfitrião voltou pra sala: todos voltam junto (a mesma tela)
			if (bHostInLobby && (Phase == ERTSNetPhase::Ended || Phase == ERTSNetPhase::Playing) && bGo)
			{
				UE_LOG(LogTemp, Log, TEXT("RTS LAN: o anfitrião voltou pra sala; voltando junto"));
				ResetMatchData();
				Phase = ERTSNetPhase::Lobby;
				if (URTSSessionSubsystem* S = GetGameInstance()->GetSubsystem<URTSSessionSubsystem>())
				{
					S->ReturnToLobby();
				}
				return;
			}
			// o anfitrião começou: todos pro mapa
			if (ERTSNetPhase(HostPhase) == ERTSNetPhase::Loading && Phase == ERTSNetPhase::Lobby && !bStartLoadSeen)
			{
				bStartLoadSeen = true;
				Epoch = HostEpoch;
				++Matches;
				Phase = ERTSNetPhase::Loading;
				UE_LOG(LogTemp, Log, TEXT("RTS LAN: o anfitrião começou; carregando o mapa"));
				if (URTSSessionSubsystem* S = GetGameInstance()->GetSubsystem<URTSSessionSubsystem>())
				{
					S->StartSkirmish();
				}
			}
			return;
		}
		case EMsg::SetupChunk:
		{
			uint8 E = 0;
			uint32 Total = 0;
			uint32 Offset = 0;
			uint16 Len = 0;
			if (!R.U8(E) || E != Epoch || Phase != ERTSNetPhase::Loading || !R.U32(Total) || !R.U32(Offset) || !R.U16(Len) || Total == 0 || Total > (64u << 20) || Offset + Len > Total)
			{
				return;
			}
			if (SetupTotal != int32(Total))
			{
				SetupTotal = int32(Total);
				Setup.SetNumZeroed(SetupTotal);
				SetupGot.Init(false, (SetupTotal + ChunkBytes - 1) / ChunkBytes);
				bSetupComplete = false;
			}
			const int32 Chunk = int32(Offset) / ChunkBytes;
			if (SetupGot.IsValidIndex(Chunk) && !SetupGot[Chunk] && R.Raw(Setup.GetData() + Offset, Len))
			{
				SetupGot[Chunk] = true;
			}
			int32 Contiguous = 0;
			while (Contiguous < SetupGot.Num() && SetupGot[Contiguous])
			{
				++Contiguous;
			}
			const int32 GotBytes = FMath::Min(SetupTotal, Contiguous * ChunkBytes);
			if (GotBytes >= SetupTotal && !bSetupComplete)
			{
				bSetupComplete = true;
				UE_LOG(LogTemp, Log, TEXT("RTS LAN: a montagem chegou (%d bytes)"), SetupTotal);
			}
			Sim::ByteWriter W = Begin(EMsg::SetupAck);
			W.U8(Epoch);
			W.U32(uint32(GotBytes));
			SendHost(ToArray(W));
			return;
		}
		case EMsg::Go:
		{
			uint8 E = 0;
			if (!bGo && bSetupComplete && R.U8(E) && E == Epoch && Phase == ERTSNetPhase::Loading)
			{
				bGo = true;
				Phase = ERTSNetPhase::Playing;
				UE_LOG(LogTemp, Log, TEXT("RTS LAN: o anfitrião mandou começar"));
			}
			return;
		}
		case EMsg::Ping:
		{
			uint64 Sent = 0;
			if (R.U64(Sent))
			{
				Sim::ByteWriter W = Begin(EMsg::Pong);
				W.U64(Sent);
				SendHost(ToArray(W));
			}
			return;
		}
		case EMsg::Ticks:
		{
			uint8 E = 0;
			uint32 AckSeq = 0;
			int64 HashTick = -1;
			uint64 Hash = 0;
			int64 First = 0;
			uint16 Count = 0;
			if (!R.U8(E) || E != Epoch || !bSetupComplete || Phase == ERTSNetPhase::Lobby || Phase == ERTSNetPhase::Idle)
			{
				return;   // de outra rodada (atrasado), ou já de volta na sala
			}
			if (!R.U32(AckSeq) || !R.I64(HashTick) || !R.U64(Hash) || !R.I64(First) || !R.U16(Count))
			{
				return;
			}
			if (!bGo)
			{
				bGo = true;
				Phase = ERTSNetPhase::Playing;
			}
			if (AckSeq > HostAckedSeq)
			{
				HostAckedSeq = AckSeq;
				Outbox.RemoveAll([AckSeq](const TPair<uint32, Sim::Command>& E) { return E.Key < AckSeq; });
			}
			if (HashTick >= 0)
			{
				CompareHash(HashTick, Hash);
			}
			for (uint16 K = 0; K < Count; ++K)
			{
				uint16 N = 0;
				if (!R.U16(N))
				{
					return;
				}
				TArray<Sim::Command> Cmds;
				for (uint16 J = 0; J < N; ++J)
				{
					Sim::Command C;
					if (!ReadSizedCommand(R, C))
					{
						return;
					}
					Cmds.Add(MoveTemp(C));
				}
				const int64 T = First + K;
				if (T == Sealed.Num())
				{
					Sealed.Add(MoveTemp(Cmds));
				}
				else if (T > Sealed.Num())
				{
					Early.Add(T, MoveTemp(Cmds));
				}
			}
			// o que chegou adiantado e agora encaixa
			while (TArray<Sim::Command>* Next = Early.Find(Sealed.Num()))
			{
				TArray<Sim::Command> Moved = MoveTemp(*Next);
				Early.Remove(Sealed.Num());
				Sealed.Add(MoveTemp(Moved));
			}
			return;
		}
		case EMsg::Bye:
			bPeerLost = true;
			Error = LOCTEXT("HostLeft", "The host left").ToString();
			return;
		default:
			return;
		}
	}
}

#undef LOCTEXT_NAMESPACE
