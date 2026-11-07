#include "SRTSSkirmish.h"
#include "RTSShell.h"
#include "RTSShellAudio.h"
#include "Dom/JsonObject.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

using namespace RTSShellUI;

namespace
{
	// os dados do mockup (skirmish.html, "os dados")
	struct FColorDef { const TCHAR* Name; uint32 Hex; };
	const FColorDef COLORS[24] = {
		{ TEXT("RED"), 0xd83a2e }, { TEXT("ORANGE"), 0xef7f22 }, { TEXT("GOLD"), 0xe3b23c }, { TEXT("YELLOW"), 0xf2dc3d }, { TEXT("LIME"), 0x9bd13b }, { TEXT("GREEN"), 0x3fae4f },
		{ TEXT("TEAL"), 0x1fa88f }, { TEXT("CYAN"), 0x3fc6d6 }, { TEXT("SKY"), 0x5aaef0 }, { TEXT("BLUE"), 0x3d7fe0 }, { TEXT("NAVY"), 0x2b4fa8 }, { TEXT("INDIGO"), 0x5b4bd6 },
		{ TEXT("PURPLE"), 0x9a5ad8 }, { TEXT("VIOLET"), 0xc06be6 }, { TEXT("MAGENTA"), 0xd94fb8 }, { TEXT("PINK"), 0xe86aa8 }, { TEXT("ROSE"), 0xf08a8a }, { TEXT("CORAL"), 0xf2785a },
		{ TEXT("WHITE"), 0xecebe6 }, { TEXT("SILVER"), 0xa8adb3 }, { TEXT("GRAPHITE"), 0x5d6268 }, { TEXT("BROWN"), 0x8a5a36 }, { TEXT("OLIVE"), 0x7d8a3a }, { TEXT("MAROON"), 0x8e2a3a } };
	const uint32 RANDOM_WHEEL[8] = { 0xd83a2e, 0xe3b23c, 0x3fae4f, 0x3fc6d6, 0x3d7fe0, 0x9a5ad8, 0xe86aa8, 0xd83a2e };   // o conic-gradient da cor aleatória
	const TCHAR* FACS[5] = { TEXT("random"), TEXT("usa"), TEXT("russia"), TEXT("china"), TEXT("gla") };
	const TCHAR* FNAME[5] = { TEXT("RANDOM"), TEXT("USA"), TEXT("RUSSIA"), TEXT("CHINA"), TEXT("GLA") };
	const TCHAR* AIS[3] = { TEXT("EASY"), TEXT("MEDIUM"), TEXT("HARD") };
	const int32 CASH[5] = { 10000, 20000, 30000, 40000, 50000 };
	const TCHAR* SWS[3] = { TEXT("UNLIMITED"), TEXT("LIMITED"), TEXT("OFF") };
	const TCHAR* ADJ[16] = { TEXT("IRON"), TEXT("SILENT"), TEXT("BROKEN"), TEXT("GLASS"), TEXT("RED"), TEXT("COLD"), TEXT("HOLLOW"), TEXT("BURNING"), TEXT("NIGHT"), TEXT("SALT"), TEXT("BLACK"),
		TEXT("LONG"), TEXT("STEEL"), TEXT("DUST"), TEXT("PALE"), TEXT("LAST") };
	const TCHAR* NOUN[16] = { TEXT("DUNE"), TEXT("ANVIL"), TEXT("SPEAR"), TEXT("HAMMER"), TEXT("FALCON"), TEXT("VIPER"), TEXT("MIRAGE"), TEXT("LANTERN"), TEXT("TIDE"), TEXT("WARDEN"),
		TEXT("SCORPION"), TEXT("HORIZON"), TEXT("CITADEL"), TEXT("EMBER"), TEXT("RAVEN"), TEXT("GATE") };
	const TCHAR* COMPASS[8] = { TEXT("NORTH"), TEXT("NORTH-EAST"), TEXT("EAST"), TEXT("SOUTH-EAST"), TEXT("SOUTH"), TEXT("SOUTH-WEST"), TEXT("WEST"), TEXT("NORTH-WEST") };
	const TCHAR* KIND_JSON[5] = { TEXT("you"), TEXT("ai"), TEXT("open"), TEXT("closed"), TEXT("human") };
	constexpr int32 AC[3] = { 216, 173, 78 };

	// o losango (skirmish.html, "o losango"): o centro, a metade da diagonal e a escala dos metros do mapa
	constexpr float DX = 1348.0f, DY = 520.0f, HALF = 420.0f, DIA = HALF * 2.0f + 120.0f;
	const float SCL = HALF / (512.0f * UE_SQRT_2);
	// a bandeja da largada sorteada
	constexpr float TRX = 930.0f, TRY = 950.0f, TRW = 360.0f, TRH = 66.0f;
	// a coluna: as medidas do Chrome (Tools/skirmish_layout.json)
	constexpr float ROW0 = 381.89f;
	constexpr float LAN_DY = 0.0f;   // (06/11) a sala da LAN tem a barra do FILL/CLEAR AI e o CHANGE MAP do anfitrião (o mockup ?mp=1): nada sobe
	struct FCell { float X, W; };
	const FCell CELLS[5] = { { 117, 194 }, { 311, 160 }, { 471, 80 }, { 551, 78 }, { 629, 160 } };
	const FVector4f BACK(110, 30, 156.55f, 44), CHANGE(637.52f, 234.89f, 137.48f, 39), FILL(110, 305.89f, 131.86f, 38), CLEAR(438.2f, 305.89f, 102.06f, 38),
		TEAMS(690.94f, 305.89f, 99.06f, 38), START(1542, 958, 330, 72);
	const FVector4f DIFF[3] = { FVector4f(248.86f, 305.89f, 55.22f, 38), FVector4f(303.08f, 305.89f, 71.45f, 38), FVector4f(373.53f, 305.89f, 56.67f, 38) };

	// (o Smooth do menu tem o mesmo nome: aqui SkSmooth, pra não colidir quando os dois entram no mesmo arquivo unificado)
	float SkSmooth(float X) { X = FMath::Clamp(X, 0.0f, 1.0f); return X * X * (3.0f - 2.0f * X); }

	/** A cor no espaço Lab (o lab() do mockup), pra medir o quanto uma cor some no chão do mapa. */
	FVector3f Lab(uint32 H)
	{
		float V[3];
		for (int32 I = 0; I < 3; ++I)
		{
			const float C = float((H >> (16 - 8 * I)) & 255) / 255.0f;
			V[I] = C > 0.04045f ? FMath::Pow((C + 0.055f) / 1.055f, 2.4f) : C / 12.92f;
		}
		const float X = (V[0] * 0.4124f + V[1] * 0.3576f + V[2] * 0.1805f) / 0.9505f, Y = V[0] * 0.2126f + V[1] * 0.7152f + V[2] * 0.0722f,
			Z = (V[0] * 0.0193f + V[1] * 0.1192f + V[2] * 0.9505f) / 1.089f;
		const auto F = [](float T) { return T > 0.008856f ? FMath::Pow(T, 1.0f / 3.0f) : 7.787f * T + 16.0f / 116.0f; };
		return FVector3f(116.0f * F(Y) - 16.0f, 500.0f * (F(X) - F(Y)), 200.0f * (F(Y) - F(Z)));
	}
	/** O chão claro de cada tipo de mapa (o sand2 da paleta do minimapa do mockup). */
	uint32 GroundOf(const FString& Terr)
	{
		static const TMap<FString, uint32> G = { { TEXT("DESERT"), 0xc49e70 }, { TEXT("TEMPERATE"), 0x8a9656 }, { TEXT("JUNGLE"), 0x527038 },
			{ TEXT("STEPPE"), 0xc8ae6e }, { TEXT("ISLANDS"), 0xdcca9c }, { TEXT("CITY"), 0xa69c8c }, { TEXT("URBAN"), 0xa69c8c }, { TEXT("COAST"), 0xccb486 },
			{ TEXT("SNOW"), 0xe8eef4 }, { TEXT("FOREST"), 0x6c8048 }, { TEXT("MOUNTAIN"), 0xa6947a } };
		const uint32* V = G.Find(Terr);
		return V ? *V : 0xc49e70;
	}

	FString SkirmishDir() { return PluginDir() / TEXT("Resources/Skirmish"); }
	/** As imagens da sala ficam em Resources/Skirmish (o Image do kit lê de Resources/Menu). */
	const FSlateBrush* SkImage(const FString& File, FVector2f Size) { return Image(TEXT("../Skirmish/") + File, Size); }
	FString SavePath(const FString& Map) { return FPaths::ProjectSavedDir() / TEXT("RTSShell") / FString::Printf(TEXT("skirmish_%s.json"), *Map); }
	/** A foto do teste compara a sala de fábrica com o mockup: sem ler nem gravar a última configuração. */
	bool IsShotTest() { FString P; return FParse::Value(FCommandLine::Get(), TEXT("-RTSSkirmishShot="), P); }
}

void SRTSSkirmish::Construct(const FArguments& InArgs)
{
	// o catálogo: os mapas de verdade que o export_skirmish_maps.py tirou do mockup (o nome, as bases, as docas do meio, o tipo)
	FString Text;
	if (FFileHelper::LoadFileToString(Text, *(SkirmishDir() / TEXT("maps.json"))))
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : Arr)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FMapEntry E;
				E.Real = O->GetStringField(TEXT("real"));
				E.Name = O->GetStringField(TEXT("name"));
				E.Size = O->GetStringField(TEXT("size"));
				E.Terr = O->GetStringField(TEXT("terr"));
				E.Cond = O->GetStringField(TEXT("cond"));
				E.Src = O->GetStringField(TEXT("src"));
				E.Players = int32(O->GetNumberField(TEXT("players")));
				E.Plays = int32(O->GetNumberField(TEXT("plays")));
				E.Date = int32(O->GetNumberField(TEXT("date")));
				E.bNaval = O->GetBoolField(TEXT("naval"));
				for (const TSharedPtr<FJsonValue>& P : O->GetArrayField(TEXT("starts")))
				{
					const TArray<TSharedPtr<FJsonValue>>& XY = P->AsArray();
					E.Starts.Add(FVector2f(float(XY[0]->AsNumber()), float(XY[1]->AsNumber())));
				}
				for (const TSharedPtr<FJsonValue>& P : O->GetArrayField(TEXT("docks")))
				{
					const TArray<TSharedPtr<FJsonValue>>& XY = P->AsArray();
					E.Docks.Add(FVector2f(float(XY[0]->AsNumber()), float(XY[1]->AsNumber())));
				}
				Maps.Add(MoveTemp(E));
			}
		}
	}
	if (Maps.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS telas: o skirmish sem mapas (rodar Tools/export_skirmish_maps.mjs)"));
	}
	LoadFavs();
	Restart();
}

void SRTSSkirmish::Restart()
{
	// a sala de começo do mockup: você (USA, azul), a Rússia e a China difíceis, a GLA média; os times 1 e 2; a largada sorteada
	const auto Mk = [](EKind K, int32 Fac, const TCHAR* Color, int32 Team, int32 Ai)
	{
		FSlot S;
		S.Kind = K; S.Fac = Fac; S.Team = Team; S.Ai = Ai; S.Base = -1;
		for (int32 I = 0; I < 24; ++I) if (FCString::Strcmp(COLORS[I].Name, Color) == 0) S.Color = I;
		return S;
	};
	Slots = { Mk(EKind::You, 1, TEXT("BLUE"), 1, 1), Mk(EKind::Ai, 2, TEXT("PURPLE"), 1, 2), Mk(EKind::Ai, 3, TEXT("RED"), 2, 2), Mk(EKind::Ai, 4, TEXT("GOLD"), 2, 1) };
	Cash = 0; Sw = 0; Sel = 0; SelSet = { 0 };
	PopField = PopRow = PopHover = -1;
	DragSlot = -1; bDragMoved = bFaderDrag = false;
	Hover = FHit(); HoverRow = HoverBase = -1;
	Launch = 0.0f; bStarting = false; Pending.Reset();
	bBrowser = false; HoverEase.Reset();
	ChooseMap(FMath::Clamp(MapIdx, 0, FMath::Max(0, Maps.Num() - 1)));
	// os estados do teste (as fotos do mockup: ?hs=1 o hover numa vaga, ?pop=0,fac a janelinha aberta)
	int32 TestRow = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("-RTSSkHover="), TestRow)) HoverRow = TestRow;
	FString PopArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("-RTSSkPop="), PopArg, false))   // false: o valor tem vírgula (a Value padrão corta nela)
	{
		FString R, F;
		if (PopArg.Split(TEXT(","), &R, &F)) OpenPop(FCString::Atoi(*R), FCString::Atoi(*F));
		FParse::Value(FCommandLine::Get(), TEXT("-RTSSkPopHover="), PopHover);   // o item com o mouse em cima (na cor, 1 + o índice dela)
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("RTSSkBrowser")))   // ?browser=1, com a busca e o hover num cartão pras fotos
	{
		FParse::Value(FCommandLine::Get(), TEXT("-RTSSkQuery="), Query);
		OpenBrowser();
		int32 Card = -1;
		if (FParse::Value(FCommandLine::Get(), TEXT("-RTSSkCardHover="), Card) && List.IsValidIndex(Card))
		{
			Hover = { EHit::BwCard, Card, -1 };
			HoverEase.Add((uint32(EHit::BwCard) << 16) | uint32(Card + 1), 1.0f);
		}
	}
}

void SRTSSkirmish::ChooseMap(int32 Index)
{
	MapIdx = Index;
	Fade = 0.0f;
	Load();   // a última configuração deste mapa
	const int32 N = Rows();
	while (Slots.Num() < N)
	{
		FSlot S;
		S.Kind = EKind::Open;
		Slots.Add(S);
	}
	for (FSlot& S : Slots)
	{
		if (S.Base >= N) S.Base = -1;
	}
	if (Sel >= N) Sel = 0;
}

int32 SRTSSkirmish::Rows() const
{
	return Maps.IsValidIndex(MapIdx) ? Maps[MapIdx].Starts.Num() : 4;
}

float SRTSSkirmish::RowH() const
{
	const int32 N = Rows();
	return (N > 8 ? 38.0f : N > 4 ? 46.0f : 58.0f) + 2.0f;
}

float SRTSSkirmish::RowTop(int32 I) const
{
	const int32 N = Rows();
	return ROW0 + (bLan ? LAN_DY : 0.0f) + I * (RowH() + (N > 8 ? 4.0f : 6.0f));
}

FLinearColor SRTSSkirmish::SlotColor(const FSlot& S) const
{
	return S.Color < 0 ? Css(154, 160, 166) : Hex(COLORS[S.Color].Hex);
}

FVector2f SRTSSkirmish::ToDiamond(FVector2f M) const
{
	// girado como a câmera da partida (yaw 45: a frente da tela é +X+Y, a direita é Y−X) e o minimapa do HUD; antes ficava 180° ao
	// contrário e a base escolhida aparecia do outro lado na partida (06/11). As imagens dos mapas estão assadas no mesmo giro (225°).
	return FVector2f(DX + (M.Y - M.X) * UE_INV_SQRT_2 * SCL, DY - (M.X + M.Y) * UE_INV_SQRT_2 * SCL);
}

int32 SRTSSkirmish::Occupant(int32 Base) const
{
	for (int32 I = 0; I < FMath::Min(Rows(), Slots.Num()); ++I)
	{
		if (Live(Slots[I]) && Slots[I].Base == Base) return I;
	}
	return -1;
}

TArray<int32> SRTSSkirmish::TrayList() const
{
	TArray<int32> L;
	for (int32 I = 0; I < FMath::Min(Rows(), Slots.Num()); ++I)
	{
		if (Live(Slots[I]) && Slots[I].Base < 0) L.Add(I);
	}
	return L;
}

float SRTSSkirmish::TraySpacing() const
{
	const int32 N = TrayList().Num();
	return FMath::Min(56.0f, (TRW - 130.0f) / FMath::Max(1, N - 1 > 0 ? N - 1 : 1));
}

FVector2f SRTSSkirmish::TrayPos(int32 K) const
{
	return FVector2f(TRX + 88.0f + K * TraySpacing(), TRY + TRH * 0.5f);
}

int32 SRTSSkirmish::BaseAt(FVector2f P) const
{
	if (!Maps.IsValidIndex(MapIdx)) return -1;
	const int32 N = Rows();
	const float TR = N > 8 ? 17.0f : N > 4 ? 22.0f : 27.0f;
	int32 K = -1;
	for (int32 I = 0; I < N; ++I)
	{
		if (FVector2f::Distance(P, ToDiamond(Maps[MapIdx].Starts[I])) < TR + 5.0f) K = I;
	}
	return K;
}

FString SRTSSkirmish::Compass(int32 Base) const
{
	const FVector2f D = ToDiamond(Maps[MapIdx].Starts[Base]);
	const float An = FMath::RadiansToDegrees(FMath::Atan2(D.X - DX, -(D.Y - DY)));
	const int32 K = FMath::RoundToInt(FMath::Fmod(An + 360.0f, 360.0f) / 45.0f) % 8;
	return COMPASS[K];
}

FString SRTSSkirmish::WhoText(const FSlot& S) const
{
	switch (S.Kind)
	{
	case EKind::You: return bLan ? S.Name : FString(TEXT("YOU"));
	case EKind::Human: return S.Name;
	case EKind::Ai: return FString::Printf(TEXT("AI · %s"), AIS[FMath::Clamp(S.Ai, 0, 2)]);   // (06/11) a rede leva o nível também
	case EKind::Open: return TEXT("OPEN");
	default: return TEXT("CLOSED");
	}
}

FString SRTSSkirmish::OpName() const
{
	// o nome da operação (opName do mockup): o FNV-1a do JSON do mapa, das vagas, do dinheiro e das superarmas, num adjetivo e num nome
	FString Key = FString::Printf(TEXT("[%d,["), MapIdx);
	for (int32 I = 0; I < Slots.Num(); ++I)
	{
		const FSlot& S = Slots[I];
		Key += FString::Printf(TEXT("%s[\"%s\",\"%s\",%d,%d,%d]"), I ? TEXT(",") : TEXT(""), KIND_JSON[int32(S.Kind)], FACS[S.Fac], S.Team, S.Base, S.Ai);
	}
	Key += FString::Printf(TEXT("],%d,%d]"), Cash, Sw);
	uint32 H = 2166136261u;
	for (const TCHAR Ch : Key)
	{
		H ^= uint32(Ch);
		H *= 16777619u;
	}
	return FString::Printf(TEXT("%s %s"), ADJ[H % 16], NOUN[(H >> 8) % 16]);
}

void SRTSSkirmish::Place(int32 Slot, int32 Base)
{
	const int32 O = Occupant(Base);
	if (O == Slot) return;
	if (O >= 0) Slots[O].Base = Slots[Slot].Base;
	Slots[Slot].Base = Base;
}

void SRTSSkirmish::ApplyTeams(int32 Kind)
{
	// os jeitos de montar os times (applyTeams do mockup): todos contra todos, 2 ou 4 times, norte × sul, oeste × leste, você × a IA
	const int32 N = Rows();
	TArray<int32> L;
	for (int32 K = 0; K < N; ++K) if (Live(Slots[K])) L.Add(K);
	switch (Kind)
	{
	case 0: for (const int32 K : L) Slots[K].Team = 0; break;
	case 1: case 2:
	{
		const int32 G = Kind == 1 ? 2 : 4;
		for (int32 J = 0; J < L.Num(); ++J) Slots[L[J]].Team = 1 + J * G / L.Num();
		break;
	}
	case 3: case 4:
		for (const int32 K : L)
		{
			if (Slots[K].Base < 0)
			{
				for (int32 B = 0; B < N; ++B) if (Occupant(B) < 0) { Slots[K].Base = B; break; }
			}
		}
		for (const int32 K : L)
		{
			if (Slots[K].Base < 0) continue;
			const FVector2f D = ToDiamond(Maps[MapIdx].Starts[Slots[K].Base]);
			Slots[K].Team = Kind == 3 ? (D.Y < DY - 2 ? 1 : D.Y > DY + 2 ? 2 : (D.X < DX ? 1 : 2)) : (D.X < DX - 2 ? 1 : D.X > DX + 2 ? 2 : (D.Y < DY ? 1 : 2));
		}
		break;
	case 5: for (const int32 K : L) Slots[K].Team = Slots[K].Kind == EKind::Ai ? 2 : 1; break;
	}
	Save();
}

void SRTSSkirmish::Save()
{
	// a última configuração de cada mapa (o localStorage do mockup), no Saved do jogo; na sala da rede, o que mudou vai pro jogo
	if (bLan) { LanSync(); return; }
	if (!Maps.IsValidIndex(MapIdx) || IsShotTest()) return;
	FString Out = FString::Printf(TEXT("{\"cash\":%d,\"sw\":%d,\"slots\":["), Cash, Sw);
	for (int32 I = 0; I < Rows(); ++I)
	{
		const FSlot& S = Slots[I];
		Out += FString::Printf(TEXT("%s{\"kind\":%d,\"fac\":%d,\"color\":%d,\"team\":%d,\"base\":%d,\"ai\":%d}"), I ? TEXT(",") : TEXT(""), int32(S.Kind), S.Fac, S.Color, S.Team, S.Base, S.Ai);
	}
	Out += TEXT("]}");
	FFileHelper::SaveStringToFile(Out, *SavePath(Maps[MapIdx].Real));
}

bool SRTSSkirmish::Load()
{
	if (!Maps.IsValidIndex(MapIdx) || IsShotTest() || bLan) return false;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *SavePath(Maps[MapIdx].Real))) return false;
	TSharedPtr<FJsonObject> O;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O) || !O.IsValid()) return false;
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!O->TryGetArrayField(TEXT("slots"), Arr) || Arr->Num() != Rows()) return false;
	Slots.Reset();
	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject> J = V->AsObject();
		FSlot S;
		S.Kind = EKind(FMath::Clamp(int32(J->GetNumberField(TEXT("kind"))), 0, 3));
		S.Fac = FMath::Clamp(int32(J->GetNumberField(TEXT("fac"))), 0, 4);
		S.Color = FMath::Clamp(int32(J->GetNumberField(TEXT("color"))), -1, 23);
		S.Team = int32(J->GetNumberField(TEXT("team")));
		S.Base = int32(J->GetNumberField(TEXT("base")));
		S.Ai = FMath::Clamp(int32(J->GetNumberField(TEXT("ai"))), 0, 2);
		Slots.Add(S);
	}
	Cash = FMath::Clamp(int32(O->GetNumberField(TEXT("cash"))), 0, 4);
	Sw = FMath::Clamp(int32(O->GetNumberField(TEXT("sw"))), 0, 2);
	SelSet = { 0 };
	Sel = 0;
	return true;
}

void SRTSSkirmish::Back()
{
	if (bLan)
	{
		// sai da sala no jogo: a página do menu do jogo volta pra lista da LAN, e o RTSShellGame mostra a lista
		LanAct(TEXT("leave"));
		return;
	}
	Save();
	FRTSShell::Show(TEXT("menu.html"));
}

void SRTSSkirmish::StartMatch()
{
	// o mesmo JSON que a página mandava (gameSetup do mockup): o mapa, o dinheiro, as superarmas e as vagas vivas, a cor aleatória
	// virando uma cor livre
	if (bLan)
	{
		// (06/11, o mockup) quem entrou aperta READY (de novo = CANCEL); o anfitrião, com todos prontos, começa uma contagem de 5 s que
		// todos vêem e que cancela se alguém mexer em algo (ou se ele apertar de novo)
		if (!bLanHost)
		{
			FSlot* Me = Slots.FindByPredicate([](const FSlot& X) { return X.Kind == EKind::You; });
			if (!Me) return;
			Me->bReady = !Me->bReady;
			LanAct(Me->bReady ? TEXT("ready:1") : TEXT("ready:0"));
			LanSent = Slots;
			LanPollAt = Now + 0.35;
			return;
		}
		if (HostCd > 0)
		{
			HostCd = 0;
			LanAct(TEXT("countdown:0"));
			ChatSay(FString(), 0, TEXT("Countdown canceled: canceled by the host"), true);
			return;
		}
		if (!bLanCanStart) return;
		HostCd = 5;
		CdKey = LanSetupKey();
		CdNext = Now + 1.0;
		LanAct(TEXT("countdown:5"));
		ChatSay(FString(), 0, TEXT("Match starting in 5…"), true);
		return;
	}
	if (bStarting || !Maps.IsValidIndex(MapIdx)) return;
	bStarting = true;
	Launch = 1.4f;
	const int32 N = Rows();
	TSet<int32> Used;
	for (int32 I = 0; I < N; ++I) if (Live(Slots[I]) && Slots[I].Color >= 0) Used.Add(Slots[I].Color);
	TArray<int32> Free;
	for (int32 K = 0; K < 24; ++K) if (!Used.Contains(K)) Free.Add(K);
	const TArray<int32> FreeAtStart = Free;
	FString J = FString::Printf(TEXT("{\"map\":\"%s\",\"money\":%d,\"superweapons\":\"%s\",\"slots\":["), *Maps[MapIdx].Real, CASH[Cash], SWS[Sw]);
	bool bFirst = true;
	for (int32 I = 0; I < N; ++I)
	{
		const FSlot& S = Slots[I];
		if (!Live(S)) continue;
		const int32 C = S.Color >= 0 ? S.Color : (Free.Num() ? Free[0] : 0);
		if (S.Color < 0 && Free.Num()) Free.RemoveAt(0);
		J += FString::Printf(TEXT("%s{\"kind\":\"%s\",\"fac\":\"%s\",\"color\":\"#%06x\",\"team\":%d,\"base\":%d,\"ai\":%d}"), bFirst ? TEXT("") : TEXT(","),
			S.Kind == EKind::You ? TEXT("you") : TEXT("ai"), FACS[S.Fac], COLORS[C].Hex, S.Team, S.Base, S.Kind == EKind::Ai ? S.Ai : 0);
		bFirst = false;
	}
	J += TEXT("]}");
	{
		// a largada pra tela de carregar, com as cores aleatórias já resolvidas como no JSON
		TArray<uint32> Hx;
		TArray<int32> Free2 = FreeAtStart;
		for (const FSlot& X : Slots)
		{
			const int32 C = X.Color >= 0 ? X.Color : (Live(X) && Free2.Num() ? Free2[0] : 0);
			if (X.Color < 0 && Live(X) && Free2.Num()) Free2.RemoveAt(0);
			Hx.Add(COLORS[C].Hex);
		}
		PushIntro(Hx);
	}
	UE_LOG(LogTemp, Log, TEXT("RTS telas: skirmish nativo, START %s"), *J);
	Save();
	Pending.Add({ Now + 0.9, [J]() { if (FRTSShell::Handlers().StartSkirmish) FRTSShell::Handlers().StartSkirmish(J); } });
}

void SRTSSkirmish::SetCash(int32 V)
{
	V = FMath::Clamp(V, 0, 4);
	if (V == Cash) return;
	Cash = V;
	if (!bLan) Save();
	else if (bLanHost) LanAct(FString::Printf(TEXT("money:%d"), CASH[Cash]));
}

void SRTSSkirmish::SetLan(bool bOn, bool bHost, const FString& HostName)
{
	bLan = bOn;
	if (!bOn) return;
	bLanHost = bHost;
	bLanCanStart = false;
	LanSent.Reset();
	ChatLog.Reset();
	ChatSeenTotal = -1;
	ChatDraft.Reset();
	bChatFocus = false;
	LanPeople.Reset();
	bLanFirstPoll = true;
	LanCountdown = HostCd = 0;
	RoomTest.Reset();
	RoomNext = 0;
	RoomT0 = -1.0;   // o relógio do roteiro começa no 1º quadro da sala (o Now daqui pode estar parado)
	{
		FString File, Text;
		if (FParse::Value(FCommandLine::Get(), TEXT("-RTSRoomTest="), File) && FFileHelper::LoadFileToString(Text, *File))
		{
			TArray<FString> Lines;
			Text.ParseIntoArrayLines(Lines);
			for (const FString& Ln : Lines)
			{
				FString A, B;
				if (Ln.Split(TEXT("|"), &A, &B) && A.TrimStartAndEnd().IsNumeric()) RoomTest.Add({ FCString::Atod(*A), B.TrimStartAndEnd() });
			}
		}
	}
	LanKicker = FString::Printf(TEXT("LAN ROOM · %s'S ROOM"), *HostName.ToUpper());
	Sel = 0;
	SelSet = { 0 };
	PopField = -1;
	bBrowser = false;
	DragSlot = -1;
	PollLobby();
	LanPollAt = Now + 0.5;
}

void SRTSSkirmish::LanAct(const FString& Command)
{
	UE_LOG(LogTemp, Log, TEXT("RTS telas: sala da LAN, %s"), *Command);
	if (FRTSShell::Handlers().Lan) FRTSShell::Handlers().Lan(Command);
	LanPollAt = Now + 0.15;   // como o act da página: relê logo depois do comando
}

bool SRTSSkirmish::LanMayEdit(int32 I, int32 F) const
{
	if (!bLan) return true;
	if (!Slots.IsValidIndex(I)) return false;
	const FSlot& S = Slots[I];
	if (F == FldWho) return bLanHost && S.Kind != EKind::You && S.Kind != EKind::Human;
	if (F < 0 || !Live(S)) return false;
	return S.Kind == EKind::You || (bLanHost && S.Kind == EKind::Ai);
}

void SRTSSkirmish::LanSync()
{
	// o que a tela mudou desde a última leitura (a mesma janelinha, o FILL, o CLEAR, os times, as bases) vira o comando exato da rede;
	// só o que esta tela pode mexer (o resto o anfitrião não aceitaria e a próxima leitura desfaz)
	const int32 N = FMath::Min(Slots.Num(), LanSent.Num());
	for (int32 I = 0; I < N; ++I)
	{
		const FSlot& A = Slots[I];
		const FSlot& B = LanSent[I];
		const bool bWasPerson = B.Kind == EKind::You || B.Kind == EKind::Human;
		if (bLanHost && I > 0 && !bWasPerson && (A.Kind != B.Kind || (A.Kind == EKind::Ai && A.Ai != B.Ai)))
		{
			if (A.Kind == EKind::Ai) LanAct(FString::Printf(TEXT("slot:%d=ai:%d"), I, FMath::Clamp(A.Ai, 0, 2)));
			else if (A.Kind == EKind::Closed) LanAct(FString::Printf(TEXT("slot:%d=closed"), I));
			else LanAct(FString::Printf(TEXT("slot:%d=open"), I));
		}
		if (!Live(A) || !LanMayEdit(I, FldFac)) continue;
		const FSlot Def;   // a IA nova entra com tudo aleatório e sem time: só vai o que for diferente disso
		const FSlot& Ref = Live(B) ? B : Def;
		if (A.Fac != Ref.Fac) LanAct(FString::Printf(TEXT("fac:%d=%s"), I, FACS[FMath::Clamp(A.Fac, 0, 4)]));
		if (A.Color != Ref.Color) LanAct(A.Color >= 0 ? FString::Printf(TEXT("color:%d=#%06x"), I, COLORS[A.Color].Hex) : FString::Printf(TEXT("color:%d=-1"), I));
		if (A.Team != Ref.Team) LanAct(FString::Printf(TEXT("team:%d=%d"), I, A.Team));
		if (A.Base != Ref.Base) LanAct(FString::Printf(TEXT("base:%d=%d"), I, A.Base));
	}
	LanSent = Slots;
	if (!bLanHost) LanPollAt = Now + 0.35;   // o pedido vai pela rede: a leitura espera a resposta do anfitrião (sem piscar o valor antigo)
}

FString SRTSSkirmish::LanSetupKey() const
{
	FString Key = FString::Printf(TEXT("%d|%d|"), MapIdx, Cash);
	for (const FSlot& S : Slots) Key += FString::Printf(TEXT("%d,%d,%d,%d,%d,%d,%d;"), int32(S.Kind), S.Fac, S.Color, S.Team, S.Base, S.Ai, S.bReady ? 1 : 0);
	return Key;
}

void SRTSSkirmish::ChatSay(const FString& Who, uint32 Hex, const FString& Text, bool bSys)
{
	ChatLog.Add({ Who, Hex, Text, bSys });
	if (ChatLog.Num() > 60) ChatLog.RemoveAt(0, ChatLog.Num() - 60);
}

void SRTSSkirmish::PushIntro(const TArray<uint32>& Hexes) const
{
	if (!Maps.IsValidIndex(MapIdx)) return;
	FRTSMatchIntro In;
	In.Op = OpName();
	In.Map = Maps[MapIdx].Name;
	In.MapReal = Maps[MapIdx].Real;
	In.bLan = bLan;
	for (int32 I = 0; I < FMath::Min(Rows(), Slots.Num()); ++I)
	{
		const FSlot& S = Slots[I];
		if (!Live(S)) continue;
		FRTSMatchIntro::FPlayer P;
		P.Name = S.Kind == EKind::Ai ? FString::Printf(TEXT("AI · %s"), AIS[FMath::Clamp(S.Ai, 0, 2)])
			: S.Name.IsEmpty() ? FString(FPlatformProcess::ComputerName()).ToUpper() : S.Name;
		P.Fac = FACS[FMath::Clamp(S.Fac, 0, 4)];
		P.Team = S.Team;
		P.Base = S.Base;
		P.Diff = S.Ai;
		P.Hex = Hexes.IsValidIndex(I) ? Hexes[I] : 0xffffffu;
		P.bAi = S.Kind == EKind::Ai;
		P.bYou = S.Kind == EKind::You;
		In.Players.Add(P);
	}
	FRTSShell::SetIntro(In);
}

float SRTSSkirmish::ChatTop() const
{
	return RowTop(Rows()) + 10.0f + 104.0f + 14.0f;   // embaixo das regras (o .chat: margin-top 14), 773,89 com 4 vagas
}

void SRTSSkirmish::PaintChat(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	// o .chat do mockup: a caixa de 680 × 152 (1 px de borda), as linhas em 16 px com 1,35 de entrelinha (o nome em negrito na cor do
	// jogador; o sistema em 14 px, cinza), a última embaixo; o campo de 40 px com a borda de cima e o "Say something · ENTER"
	const float Y = ChatTop();
	{
		FMesh M(S);
		BoxRect(M, FVector4f(110, Y, 680, 152), CssV(16, 21, 19, 0.55f), CssV(230, 220, 190, 0.10f));
		M.Rect(111, Y + 110, 678, 1, CssV(230, 220, 190, 0.10f));
		M.Rect(111, Y + 111, 678, 40, CssV(0, 0, 0, 0.25f));
		M.Draw(Out, L++, White());
	}
	const float Scale = S.GetAccumulatedLayoutTransform().GetScale();
	const FSlateFontInfo Fs = Font(EFace::Barlow500, 14, 1), Fn = Font(EFace::Barlow700, 16, 1), Ft = Font(EFace::Barlow500, 16);
	const float W = 650.0f;
	// as linhas quebradas por palavra (a 1ª de cada fala começa depois do nome)
	struct FRow { FString Who, Text; uint32 Hex; bool bSys; float NameW; };
	TArray<FRow> Rws;
	TArray<float> Hs;
	for (const FChatMsg& C : ChatLog)
	{
		const FSlateFontInfo& F = C.bSys ? Fs : Ft;
		const FString Name = C.bSys ? FString() : C.Who + TEXT(": ");
		const float Nw = C.bSys ? 0.0f : TextWidth(Name, Fn, Scale);
		TArray<FString> Words;
		C.Text.ParseIntoArray(Words, TEXT(" "), true);
		FString Cur;
		float Avail = W - Nw;
		bool bFirst = true;
		const auto Flush = [&]()
		{
			Rws.Add({ bFirst ? Name : FString(), Cur, C.Hex, C.bSys, bFirst ? Nw : 0.0f });
			Hs.Add(C.bSys ? 18.9f : 21.6f);
			bFirst = false;
			Cur.Reset();
			Avail = W;
		};
		for (const FString& Wd : Words)
		{
			const FString Try = Cur.IsEmpty() ? Wd : Cur + TEXT(" ") + Wd;
			if (!Cur.IsEmpty() && TextWidth(Try, F, Scale) > Avail) Flush();
			Cur = Cur.IsEmpty() ? Wd : Cur + TEXT(" ") + Wd;
		}
		Flush();
		Hs.Last() += 3.0f;   // o margin-bottom de cada <p>
	}
	// de baixo pra cima a partir do fim da área (a rolagem no fim, como o scrollTop da página); recortado na caixa
	const float Bottom = Y + 1 + 109 - 4;
	float Yb = Bottom;
	int32 First = Rws.Num();
	float Total = 0.0f;
	for (int32 K = Rws.Num() - 1; K >= 0; --K) { Total += Hs[K]; First = K; if (Total > 109.0f + 40.0f) break; }
	Yb = Bottom - Total;
	if (Total < 109.0f - 14.0f) Yb = Y + 1 + 10;   // poucas linhas: começam em cima (o padding de 10)
	Out.PushClip(FSlateClippingZone(S.ToPaintGeometry(FVector2f(678, 109), FSlateLayoutTransform(FVector2f(111, Y + 1)))));
	for (int32 K = First; K < Rws.Num(); ++K)
	{
		const FRow& R = Rws[K];
		if (R.bSys) Text(Out, L, S, R.Text, Fs, 125, Yb + 14.2f, Css(230, 220, 190, 0.45f));
		else
		{
			if (!R.Who.IsEmpty()) Text(Out, L, S, R.Who, Fn, 125, Yb + 16.2f, Hex(R.Hex));
			Text(Out, L, S, R.Text, Ft, 125 + R.NameW, Yb + 16.2f, Css(238, 241, 244, 0.9f));
		}
		Yb += Hs[K];
	}
	Out.PopClip();
	// o campo: o texto digitado (o fim, se não couber) com o cursor piscando, ou a dica
	const float By = Y + 111 + 26.0f;
	if (ChatDraft.IsEmpty() && !bChatFocus) Text(Out, L, S, TEXT("Say something · ENTER"), Font(EFace::Barlow500, 16, 1), 125, By, Css(230, 220, 190, 0.35f));
	else
	{
		FString Shown = ChatDraft;
		while (Shown.Len() > 1 && TextWidth(Shown, Ft, Scale) > W - 8) Shown.RightChopInline(1);
		const float Tw = Shown.IsEmpty() ? 0.0f : Text(Out, L, S, Shown, Ft, 125, By, FLinearColor::White);
		if (bChatFocus && FMath::Fmod(Now - CaretT0, 1.06) < 0.53)
		{
			FMesh M(S);
			M.Rect(126 + Tw, Y + 121, 1.5f, 21, CssV(238, 241, 244));
			M.Draw(Out, L++, White());
		}
	}
	++L;
}

void SRTSSkirmish::PollLobby()
{
	// a sala como o jogo a vê (o poll da página): o mapa, as vagas pela ordem do jogo (a sua é a LocalSlot), a cor mais perto das 24,
	// o time, se está pronto, o dinheiro, quem é o anfitrião e se dá pra começar. -RTSLobbyFake=<arquivo.json> pras fotos de teste
	if (DragSlot >= 0 || bFaderDrag) { LanPollAt = Now + 0.1; return; }   // (06/11) o arraste não briga com a leitura
	FString Json, Fake;
	bool bOk = false;
	if (FParse::Value(FCommandLine::Get(), TEXT("-RTSLobbyFake="), Fake)) bOk = FFileHelper::LoadFileToString(Json, *Fake);
	else if (FRTSShell::Handlers().Api) bOk = FRTSShell::Handlers().Api(TEXT("lobby.json"), Json);
	TSharedPtr<FJsonObject> O;
	if (!bOk || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), O) || !O.IsValid()) return;
	FString Map;
	O->TryGetStringField(TEXT("map"), Map);
	int32 Mi = Maps.IndexOfByPredicate([&](const FMapEntry& E) { return E.Real == Map; });
	if (Mi < 0) Mi = FMath::Clamp(MapIdx, 0, FMath::Max(0, Maps.Num() - 1));
	if (Mi != MapIdx) ChooseMap(Mi);
	O->TryGetBoolField(TEXT("host"), bLanHost);
	bool bCan = false;
	O->TryGetBoolField(TEXT("canStart"), bCan);
	bLanCanStart = bCan;
	double Me = -1.0, Limit = 8.0, Money = 10000.0, Cd = 0.0, ChatTotal = 0.0;
	O->TryGetNumberField(TEXT("me"), Me);
	O->TryGetNumberField(TEXT("slotLimit"), Limit);
	O->TryGetNumberField(TEXT("money"), Money);
	O->TryGetNumberField(TEXT("countdown"), Cd);
	O->TryGetNumberField(TEXT("chatTotal"), ChatTotal);
	const int32 N = FMath::Min(Rows(), Limit > 0.0 ? int32(Limit) : 8);
	const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
	O->TryGetArrayField(TEXT("members"), Members);
	TSet<int32> ClosedSet;
	const TArray<TSharedPtr<FJsonValue>>* ClosedArr = nullptr;
	if (O->TryGetArrayField(TEXT("closed"), ClosedArr)) for (const TSharedPtr<FJsonValue>& V : *ClosedArr) ClosedSet.Add(int32(V->AsNumber()));
	TMap<int32, uint32> SlotHex;   // a cor de cada vaga (pros nomes no chat)
	TMap<FString, bool> People;    // quem está na sala e se está pronto
	FString HostN;
	TArray<FSlot> Next;
	for (int32 K = 0; K < Rows(); ++K)   // as vagas além do limite da rede ficam abertas (o render da página completa até as bases do mapa)
	{
		FSlot S;
		S.Kind = K < N && ClosedSet.Contains(K) ? EKind::Closed : EKind::Open;
		if (Members && K < N)
		{
			for (const TSharedPtr<FJsonValue>& V : *Members)
			{
				const TSharedPtr<FJsonObject> M = V->AsObject();
				double Slot = -1.0;
				if (!M.IsValid() || !M->TryGetNumberField(TEXT("slot"), Slot) || int32(Slot) != K) continue;
				bool bAi = false, bReady = false, bAuto = false;
				M->TryGetBoolField(TEXT("ai"), bAi);
				M->TryGetBoolField(TEXT("ready"), bReady);
				M->TryGetBoolField(TEXT("colorAuto"), bAuto);
				double Team = 0.0, Base = -1.0, Diff = 1.0, Ping = 0.0;
				M->TryGetNumberField(TEXT("team"), Team);
				M->TryGetNumberField(TEXT("base"), Base);
				M->TryGetNumberField(TEXT("diff"), Diff);
				M->TryGetNumberField(TEXT("ping"), Ping);
				FString Name, HexS, FacS;
				M->TryGetStringField(TEXT("name"), Name);
				M->TryGetStringField(TEXT("hex"), HexS);
				M->TryGetStringField(TEXT("fac"), FacS);
				S.Kind = K == int32(Me) ? EKind::You : bAi ? EKind::Ai : EKind::Human;
				S.Fac = 0;
				for (int32 F = 0; F < 5; ++F) if (FacS == FACS[F]) S.Fac = F;
				S.Team = int32(Team);
				S.Base = int32(Base) >= 0 && int32(Base) < Rows() ? int32(Base) : -1;
				S.Ai = FMath::Clamp(int32(Diff), 0, 2);
				S.Ping = int32(Ping);
				S.bHost = K == 0;
				S.Name = Name.ToUpper();
				S.bReady = bReady;
				if (!bAi) People.Add(S.Name, bReady || K == 0);
				if (K == 0) HostN = S.Name;
				// a cor mais perto das 24 (o near da página)
				const uint32 Hx = FParse::HexNumber(*HexS.Replace(TEXT("#"), TEXT("")));
				int32 Best = 0;
				int64 Bd = TNumericLimits<int64>::Max();
				for (int32 C = 0; C < 24; ++C)
				{
					const int64 Dr = int64((Hx >> 16) & 255) - int64((COLORS[C].Hex >> 16) & 255), Dg = int64((Hx >> 8) & 255) - int64((COLORS[C].Hex >> 8) & 255),
						Db = int64(Hx & 255) - int64(COLORS[C].Hex & 255);
					const int64 Dd = Dr * Dr + Dg * Dg + Db * Db;
					if (Dd < Bd) { Bd = Dd; Best = C; }
				}
				S.Color = bAuto ? -1 : Best;
				S.NetHex = Hx & 0xFFFFFFu;
				SlotHex.Add(K, Hx & 0xFFFFFFu);
				break;
			}
		}
		Next.Add(S);
	}
	Slots = MoveTemp(Next);
	LanSent = Slots;
	{
		// a largada da partida da rede (os outros começam pelo pacote: a tela de carregar já tem o que mostrar)
		TArray<uint32> Hx;
		for (const FSlot& X : Slots) Hx.Add(X.Color >= 0 ? COLORS[X.Color].Hex : X.NetHex);
		PushIntro(Hx);
	}
	if (!bFaderDrag)
	{
		int32 Bc = 0;
		for (int32 K = 1; K < 5; ++K) if (FMath::Abs(CASH[K] - Money) < FMath::Abs(CASH[Bc] - Money)) Bc = K;
		Cash = Bc;
	}
	// a seleção fica (antes voltava pra 1ª a cada leitura e o Ctrl/Shift não durava)
	if (!Slots.IsValidIndex(Sel)) Sel = 0;
	for (auto It = SelSet.CreateIterator(); It; ++It) if (!Slots.IsValidIndex(*It)) It.RemoveCurrent();
	if (!SelSet.Num()) SelSet = { Sel };
	// as linhas do sistema (o mockup): quem entrou e saiu, quem ficou pronto; na 1ª leitura, a sala criada ou a entrada nela
	if (bLanFirstPoll)
	{
		bLanFirstPoll = false;
		if (bLanHost) ChatSay(FString(), 0, TEXT("Room created · LAN"), true);
		else
		{
			ChatSay(FString(), 0, FString::Printf(TEXT("You joined %s's room"), *HostN), true);
			ChatSay(FString(), 0, FString::Printf(TEXT("%s is the host"), *HostN), true);
		}
	}
	else
	{
		for (const TPair<FString, bool>& P : People)
		{
			const bool* Was = LanPeople.Find(P.Key);
			if (!Was) ChatSay(FString(), 0, FString::Printf(TEXT("%s joined the room"), *P.Key), true);
			else if (*Was != P.Value) ChatSay(FString(), 0, FString::Printf(TEXT("%s is %s"), *P.Key, P.Value ? TEXT("ready") : TEXT("not ready")), true);
		}
		for (const TPair<FString, bool>& P : LanPeople) if (!People.Contains(P.Key)) ChatSay(FString(), 0, FString::Printf(TEXT("%s left the room"), *P.Key), true);
	}
	LanPeople = MoveTemp(People);
	// a contagem do anfitrião (quem entrou vê; o anfitrião já disse as dele)
	const int32 NewCd = int32(Cd);
	if (!bLanHost && NewCd != LanCountdown)
	{
		if (LanCountdown == 0 && NewCd > 0) ChatSay(FString(), 0, FString::Printf(TEXT("Match starting in %d…"), NewCd), true);
		else if (NewCd == 0 && LanCountdown > 1) ChatSay(FString(), 0, TEXT("Countdown canceled"), true);
		else if (NewCd == 0 && LanCountdown == 1) ChatSay(FString(), 0, TEXT("Launching"), true);
	}
	LanCountdown = NewCd;
	if (RoomTest.Num())
	{
		// o teste da sala: cada mudança da sala no log (o mapa, cada vaga, o dinheiro, a contagem)
		FString Sum = FString::Printf(TEXT("mapa %s, $%d, contagem %d, pode começar %d:"), Maps.IsValidIndex(MapIdx) ? *Maps[MapIdx].Real : TEXT("?"), CASH[Cash], NewCd, bLanCanStart ? 1 : 0);
		for (int32 K = 0; K < Slots.Num(); ++K)
		{
			const FSlot& X = Slots[K];
			static const TCHAR* KN[5] = { TEXT("eu"), TEXT("IA"), TEXT("aberta"), TEXT("fechada"), TEXT("humano") };
			Sum += FString::Printf(TEXT(" [%d %s%s %s cor %d time %d base %d nivel %d%s]"), K, KN[int32(X.Kind)], X.Name.IsEmpty() ? TEXT("") : *(TEXT(" ") + X.Name),
				FACS[FMath::Clamp(X.Fac, 0, 4)], X.Color, X.Team, X.Base, X.Ai, X.bReady ? TEXT(" pronto") : TEXT(""));
		}
		if (Sum != RoomLogged) { RoomLogged = Sum; UE_LOG(LogTemp, Log, TEXT("RTS telas: sala da LAN: %s"), *Sum); }
	}
	// o chat do jogo: as linhas que ainda não entraram (o total só sobe; a lista vem com as últimas 30)
	const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
	if (O->TryGetArrayField(TEXT("chat"), Lines))
	{
		const int32 Total = int32(ChatTotal), Num = Lines->Num();
		for (int32 J = 0; J < Num; ++J)
		{
			const int32 G = Total - Num + J;
			if (ChatSeenTotal >= 0 && G < ChatSeenTotal) continue;
			const TSharedPtr<FJsonObject> Ln = (*Lines)[J]->AsObject();
			if (!Ln.IsValid()) continue;
			FString Who, Tx;
			double Slot = 0.0;
			Ln->TryGetStringField(TEXT("name"), Who);
			Ln->TryGetStringField(TEXT("text"), Tx);
			Ln->TryGetNumberField(TEXT("slot"), Slot);
			const uint32* Hx = SlotHex.Find(int32(Slot));
			ChatSay(Who.IsEmpty() ? FString(TEXT("?")) : Who.ToUpper(), Hx ? *Hx : 0xffffffu, Tx, false);
		}
		ChatSeenTotal = Total;
	}
	// o teste da rede de verdade: o anfitrião põe uma IA na primeira vaga aberta (o mesmo clique no OPEN)
	if (!bTestAi && bLanHost && FParse::Param(FCommandLine::Get(), TEXT("RTSMpTestAddAi")) && Slots.ContainsByPredicate([](const FSlot& S) { return S.Kind == EKind::Open; }))
	{
		bTestAi = true;
		LanAct(TEXT("addai"));
	}
}

void SRTSSkirmish::OpenPop(int32 Row, int32 Field)
{
	PopRow = Row;
	PopField = Field;
	PopHover = -1;
	BuildPop();
}

void SRTSSkirmish::BuildPop()
{
	// as janelinhas (openPop do mockup), nas medidas do Chrome (Tools/skirmish_pops.json): 4 px abaixo da célula, 1 px de borda e 6
	// de respiro; cada item com a altura do seu tipo
	PopItems.Reset();
	if (PopField < 0) return;
	const FSlot& S = Slots[FMath::Max(0, PopRow)];
	float X0, Y0, W;
	if (PopField == FldTeams)
	{
		X0 = TEAMS.X + TEAMS.Z - 234.0f;
		Y0 = TEAMS.Y + TEAMS.W + 4.0f;
		W = 234.0f;
	}
	else
	{
		const FCell& C = CELLS[PopField];
		X0 = C.X;
		Y0 = RowTop(PopRow) + 1.0f + (RowH() - 2.0f) + 4.0f;
		W = FMath::Max(180.0f, C.W) + 14.0f;
	}
	float Y = Y0 + 7.0f;
	const float IX = X0 + 7.0f, IW = W - 14.0f;
	const auto Item = [&](float H, int32 Value, bool bCur) { PopItems.Add({ FVector4f(IX, Y, IW, H), Value, bCur, false, false }); Y += H; };
	const auto Sep = [&]() { PopItems.Add({ FVector4f(IX + 4.0f, Y + 5.0f, IW - 8.0f, 1.0f), -1, false, true, false }); Y += 11.0f; };
	switch (PopField)
	{
	case FldWho:
		Item(34, 100, S.Kind == EKind::Open);
		Item(34, 101, S.Kind == EKind::Closed);
		Sep();
		for (int32 A = 0; A < 3; ++A) Item(34, A, S.Kind == EKind::Ai && S.Ai == A);
		break;
	case FldFac:
		for (int32 F = 0; F < 5; ++F) Item(F == 0 ? 62.0f : 60.0f, F, S.Fac == F);
		break;
	case FldColor:
	{
		W = 270.0f;
		PopItems.Add({ FVector4f(X0 + 13.0f, Y0 + 13.0f, 244.0f, 30.0f), -1, S.Color < 0, false, false });
		for (int32 K = 0; K < 24; ++K)
		{
			bool bUsed = false;
			for (int32 I = 0; I < Rows(); ++I) if (I != PopRow && Live(Slots[I]) && Slots[I].Color == K) bUsed = true;
			PopItems.Add({ FVector4f(X0 + 13.0f + (K % 6) * 42.0f, Y0 + 51.0f + (K / 6) * 42.0f, 34.0f, 34.0f), K, S.Color == K, false, bUsed });
		}
		Y = Y0 + 224.0f - 7.0f;
		break;
	}
	case FldTeam:
		W = 228.0f;
		PopItems.Add({ FVector4f(X0 + 13.0f, Y0 + 13.0f, 202.0f, 36.0f), 0, S.Team == 0, false, false });
		for (int32 K = 1; K <= 8; ++K)
		{
			PopItems.Add({ FVector4f(X0 + 13.0f + ((K - 1) % 4) * 52.0f, Y0 + 55.0f + ((K - 1) / 4) * 54.0f, 46.0f, 48.0f), K, S.Team == K, false, false });
		}
		Y = Y0 + 170.0f - 7.0f;
		break;
	case FldBase:
		Item(42, -1, S.Base < 0);
		Sep();
		for (int32 K = 0; K < Rows(); ++K) Item(42, K, S.Base == K);
		break;
	case FldTeams:
		Item(34, 0, false); Item(34, 1, false); Item(34, 2, false); Sep(); Item(34, 3, false); Item(34, 4, false); Sep(); Item(34, 5, false);
		break;
	}
	PopBox = FVector4f(X0, Y0, W, Y + 7.0f - Y0);
}

void SRTSSkirmish::ChoosePop(int32 Index)
{
	if (!PopItems.IsValidIndex(Index) || PopItems[Index].bSep || PopItems[Index].bDisabled) return;
	const int32 V = PopItems[Index].Value;
	if (PopField == FldTeams)
	{
		PopField = -1;
		ApplyTeams(V);
		return;
	}
	// com várias vagas marcadas, a escolha vale pra todas (a cor é uma por jogador: no lote só a aleatória vale pra todas)
	TArray<int32> Targets;
	if (SelSet.Num() > 1 && SelSet.Contains(PopRow)) { for (const int32 K : SelSet) if (K < Rows()) Targets.Add(K); }
	else Targets.Add(PopRow);
	for (const int32 T0 : Targets)
	{
		FSlot& O = Slots[T0];
		switch (PopField)
		{
		case FldWho:
			if (O.Kind == EKind::You) break;
			if (V < 100)
			{
				if (!Live(O)) { O.Color = -1; O.Fac = 0; O.Base = -1; }   // a IA nova entra com cor, exército e largada aleatórios
				O.Kind = EKind::Ai;
				O.Ai = V;
			}
			else
			{
				O.Kind = V == 100 ? EKind::Open : EKind::Closed;
				O.Base = -1;
				O.Team = 0;
			}
			break;
		case FldFac: if (Live(O)) O.Fac = V; break;
		case FldTeam: if (Live(O)) O.Team = V; break;
		case FldColor: if (Live(O) && (V < 0 || T0 == PopRow)) O.Color = V; break;
		case FldBase: if (Live(O)) { if (V < 0) O.Base = -1; else if (T0 == PopRow) Place(PopRow, V); } break;
		}
	}
	PopField = -1;
	Save();
}

FGeometry SRTSSkirmish::StageOf(const FGeometry& G) const
{
	const FVector2f Local = FVector2f(G.GetLocalSize());
	const float S = FMath::Min(Local.X / StageW, Local.Y / StageH);
	const FVector2f Offset = (Local - FVector2f(StageW, StageH) * S) * 0.5f;
	return G.MakeChild(FVector2f(StageW, StageH), FSlateLayoutTransform(S, Offset));
}

FVector2f SRTSSkirmish::StagePos(const FGeometry& G, const FPointerEvent& E) const
{
	return FVector2f(StageOf(G).AbsoluteToLocal(E.GetScreenSpacePosition()));
}

SRTSSkirmish::FHit SRTSSkirmish::HitAt(FVector2f P) const
{
	if (bBrowser) return BrowserHitAt(P);   // o navegador cobre a sala inteira
	// a janelinha aberta fica por cima de tudo
	if (PopField >= 0 && In(PopBox, P))
	{
		for (int32 I = 0; I < PopItems.Num(); ++I) if (!PopItems[I].bSep && In(PopItems[I].R, P)) return { EHit::PopItem, I, -1 };
		return { EHit::Outside, -1, -1 };
	}
	if (In(BACK, P)) return { EHit::Back, -1, -1 };
	if (In(START, P)) return { EHit::Start, -1, -1 };
	const bool bBoss = !bLan || bLanHost;   // o mapa, o FILL/CLEAR, os times e o dinheiro: no skirmish e o anfitrião da rede
	if (bLan)
	{
		// (06/11) a sala da rede é a do skirmish (o mockup ?mp=1), com o ✕ de tirar quem entrou (o anfitrião, com a linha no mouse) e
		// o campo do chat; quem não pode mexer num campo não abre a janelinha (LanMayEdit)
		if (bLanHost)
		{
			for (int32 I = 0; I < Rows(); ++I)
			{
				if (Slots[I].Kind != EKind::Human || (HoverRow != I && !(Hover.Kind == EHit::Kick && Hover.A == I))) continue;
				if (In(FVector4f(798, RowTop(I) + RowH() * 0.5f - 13.0f, 26, 26), P)) return { EHit::Kick, I, -1 };
			}
		}
		if (In(FVector4f(111, ChatTop() + 110, 678, 41), P)) return { EHit::ChatInput, -1, -1 };
	}
	if (bBoss && In(CHANGE, P)) return { EHit::Change, -1, -1 };
	if (bBoss && In(FILL, P)) return { EHit::Fill, -1, -1 };
	if (bBoss && In(CLEAR, P)) return { EHit::Clear, -1, -1 };
	if (bBoss && In(TEAMS, P)) return { EHit::Teams, -1, -1 };
	for (int32 D = 0; D < 3; ++D) if (bBoss && In(DIFF[D], P)) return { EHit::Diff, D, -1 };
	for (int32 I = 0; I < Rows(); ++I)
	{
		const float Top = RowTop(I);
		if (P.Y < Top || P.Y > Top + RowH() || P.X < 110.0f || P.X > 790.0f) continue;
		for (int32 F = 0; F < 5; ++F)
		{
			if (P.X >= CELLS[F].X && P.X < CELLS[F].X + CELLS[F].W) return { EHit::Cell, I, F };
		}
		return { EHit::Cell, I, -1 };
	}
	const float RT = RowTop(Rows()) + 10.0f;
	if (bBoss && In(FVector4f(119, RT + 55, 368, 34), P)) return { EHit::Fader, -1, -1 };
	for (int32 K = 0; K < 3; ++K) if (!bLan && In(FVector4f(519.03f + K * 86.655f, RT + 41, 82.66f, 35), P)) return { EHit::Seg, K, -1 };
	if (Maps.IsValidIndex(MapIdx))
	{
		const int32 N = Rows();
		const float TR = N > 8 ? 17.0f : N > 4 ? 22.0f : 27.0f;
		for (int32 K = 0; K < N; ++K)
		{
			if (FVector2f::Distance(P, ToDiamond(Maps[MapIdx].Starts[K])) < TR + 5.0f) return { EHit::Base, K, Occupant(K) };
		}
		const TArray<int32> L = TrayList();
		for (int32 K = 0; K < L.Num(); ++K)
		{
			if (FVector2f::Distance(P, TrayPos(K)) < FMath::Min(24.0f, TraySpacing() * 0.5f)) return { EHit::TrayToken, K, L[K] };
		}
		int32 Alive = 0;
		for (int32 I = 0; I < N; ++I) if (Live(Slots[I])) ++Alive;
		if (L.Num() < Alive && FVector2f::Distance(P, FVector2f(TRX + TRW - 30.0f, TRY + TRH * 0.5f)) < 18.0f) return { EHit::TrayReset, -1, -1 };
	}
	return FHit();
}

void SRTSSkirmish::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	Now = InCurrentTime;
	const float Dt = FMath::Clamp(InDeltaTime, 0.0f, 0.1f);
	T += Dt;
	Fade = FMath::Min(1.0f, Fade + Dt * 3.0f);
	Launch = FMath::Max(0.0f, Launch - Dt);
	StartHover += ((Hover.Kind == EHit::Start && LanStartOn() ? 1.0f : 0.0f) - StartHover) * (1.0f - FMath::Exp(-Dt * 12.0f));
	if (bLan && Now >= LanPollAt)
	{
		LanPollAt = Now + 0.5;
		PollLobby();
	}
	// o roteiro do teste da sala (-RTSRoomTest): os comandos na hora marcada, como se fossem os cliques
	if (bLan && RoomTest.Num() && RoomT0 < 0.0) RoomT0 = Now;
	while (bLan && RoomNext < RoomTest.Num() && Now - RoomT0 >= RoomTest[RoomNext].Key)
	{
		const FString C = RoomTest[RoomNext++].Value;
		UE_LOG(LogTemp, Log, TEXT("RTS telas: teste da sala, %s"), *C);
		if (C == TEXT("start")) StartMatch();
		else if (C.StartsWith(TEXT("say:"))) LanAct(TEXT("chat:") + C.Mid(4));
		else LanAct(C);
	}
	if (bLan && bLanHost && HostCd > 0 && Now >= CdNext)
	{
		// a contagem do anfitrião: cancela se a montagem mudou (ou alguém deixou de estar pronto); no fim, começa
		if (LanSetupKey() != CdKey || !bLanCanStart)
		{
			HostCd = 0;
			LanAct(TEXT("countdown:0"));
			ChatSay(FString(), 0, TEXT("Countdown canceled: the setup changed"), true);
		}
		else if (--HostCd <= 0)
		{
			ChatSay(FString(), 0, TEXT("Launching"), true);
			LanAct(TEXT("start"));
		}
		else
		{
			LanAct(FString::Printf(TEXT("countdown:%d"), HostCd));
			CdNext += 1.0;
		}
	}
	// as transições do navegador (o hover dos filtros, dos cartões e do BACK) e a rolagem suave da grade
	if (bBrowser)
	{
		const bool bEase = Hover.Kind == EHit::BwBack || Hover.Kind == EHit::BwChip || Hover.Kind == EHit::BwCard || Hover.Kind == EHit::BwFav;
		const uint32 HKey = bEase ? (uint32(Hover.Kind == EHit::BwFav ? EHit::BwCard : Hover.Kind) << 16) | uint32(Hover.A + 1) : 0u;
		if (HKey) HoverEase.FindOrAdd(HKey);
		for (auto It = HoverEase.CreateIterator(); It; ++It)
		{
			It.Value() = FMath::Clamp(It.Value() + (It.Key() == HKey ? Dt : -Dt) / 0.14f, 0.0f, 1.0f);
			if (It.Value() <= 0.0f && It.Key() != HKey) It.RemoveCurrent();
		}
		Scroll += (ScrollTo - Scroll) * (1.0f - FMath::Exp(-Dt * 16.0f));
		if (FMath::Abs(ScrollTo - Scroll) < 0.25f) Scroll = ScrollTo;
	}
	for (int32 I = 0; I < Pending.Num(); ++I)
	{
		if (Now >= Pending[I].At)
		{
			TFunction<void()> Run = MoveTemp(Pending[I].Run);
			Pending.RemoveAt(I);
			Run();
			return;
		}
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

FReply SRTSSkirmish::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// na foto de teste o estado armado pela linha de comando fica parado: a Slate manda "mouse moveu" sintético com o cursor de verdade
	// (mesmo com a janela fora da tela), e isso apagava o hover da foto (a dica do GOLD sumiu numa das fotos de 05/11)
	if (IsShotTest()) return FReply::Handled();
	const FVector2f P = StagePos(MyGeometry, MouseEvent);
	const FHit H = HitAt(P);
	// o tique do hover a cada peça nova (a estrela é parte do cartão; a busca e o mundo novo não fazem som)
	const auto SoundKey = [](const FHit& X) { FHit Y = X; if (Y.Kind == EHit::BwFav) Y.Kind = EHit::BwCard; return Y; };
	if (!(SoundKey(H) == SoundKey(Hover)) && H.Kind != EHit::None && H.Kind != EHit::Outside && H.Kind != EHit::Fader && H.Kind != EHit::BwSearch && H.Kind != EHit::BwNew)
	{
		RTSShellAudio::Play(RTSShellAudio::EUi::Hover);
	}
	Hover = H;
	HoverRow = H.Kind == EHit::Cell || H.Kind == EHit::Kick ? H.A : -1;
	HoverBase = H.Kind == EHit::Base ? H.A : -1;
	PopHover = H.Kind == EHit::PopItem ? H.A : -1;
	if (DragSlot >= 0)
	{
		DragPos = P;
		if (FVector2f::Distance(P, DragFrom) > 6.0f) bDragMoved = true;
	}
	if (bFaderDrag) SetCash(FMath::RoundToInt((P.X - 125.0f) / 356.03f * 4.0f));
	return FReply::Handled();
}

void SRTSSkirmish::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	if (IsShotTest()) return;   // o mesmo do OnMouseMove: a foto de teste não perde o hover armado
	Hover = FHit();
	HoverRow = HoverBase = PopHover = -1;
}

FReply SRTSSkirmish::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2f P = StagePos(MyGeometry, MouseEvent);
	const FHit H = HitAt(P);
	if (bBrowser)
	{
		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Handled();
		if (H.Kind != EHit::None && H.Kind != EHit::BwSearch && H.Kind != EHit::BwNew) RTSShellAudio::Click();
		switch (H.Kind)
		{
		case EHit::BwBack: CloseBrowser(); break;
		case EHit::BwChip: ToggleChip(H.A); break;
		case EHit::BwFav:
		{
			const FString& R = Maps[List[H.A]].Real;
			if (Favs.Contains(R)) Favs.Remove(R); else Favs.Add(R);
			SaveFavs();
			break;
		}
		case EHit::BwCard:
		{
			const int32 M = List[H.A];
			CloseBrowser();
			if (bLan)
			{
				if (bLanHost) LanAct(TEXT("map:") + Maps[M].Real);   // (06/11) o mapa da sala: o jogo troca e todos vêem
				break;
			}
			Save();
			ChooseMap(M);
			break;
		}
		default: break;
		}
		return FReply::Handled();
	}
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		// o botão direito na base tira a largada de quem está lá
		if (H.Kind == EHit::Base && H.B >= 0 && LanMayEdit(H.B, FldBase)) { Slots[H.B].Base = -1; Save(); }
		return FReply::Handled();
	}
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
	if (H.Kind != EHit::None && H.Kind != EHit::Outside) RTSShellAudio::Click();
	if (PopField >= 0 && H.Kind != EHit::PopItem && H.Kind != EHit::Outside)
	{
		PopField = -1;   // o clique fora fecha a janelinha (e segue pro que foi clicado, como no mockup)
	}
	if (bLan && H.Kind != EHit::ChatInput) bChatFocus = false;
	switch (H.Kind)
	{
	case EHit::Kick: LanAct(FString::Printf(TEXT("kick:%d"), H.A)); break;
	case EHit::ChatInput: bChatFocus = true; CaretT0 = Now; break;
	case EHit::PopItem: ChoosePop(H.A); break;
	case EHit::Back: Back(); break;
	case EHit::Start: StartMatch(); break;
	case EHit::Change: OpenBrowser(); break;
	case EHit::Fill:
		for (int32 K = 0; K < Rows(); ++K)
		{
			FSlot& O = Slots[K];
			if (O.Kind == EKind::Open) { O.Kind = EKind::Ai; O.Ai = FillDiff; O.Fac = 0; O.Color = -1; O.Base = -1; O.Team = 0; }
		}
		Save();
		break;
	case EHit::Diff: FillDiff = H.A; break;
	case EHit::Clear:
		for (int32 K = 0; K < Rows(); ++K) if (Slots[K].Kind == EKind::Ai) Slots[K] = FSlot();
		SelSet = { FMath::Min(Sel, Rows() - 1) };
		Save();
		break;
	case EHit::Teams: OpenPop(-1, FldTeams); break;
	case EHit::Cell:
	{
		const int32 I = H.A;
		if (bLan && (H.B < 0 || !LanMayEdit(I, H.B))) break;   // a sala da rede: o que não é seu não abre (o .lock do mockup)
		const bool bMulti = !bLan || bLanHost;
		if (bMulti && MouseEvent.IsControlDown()) { if (SelSet.Contains(I) && SelSet.Num() > 1) SelSet.Remove(I); else SelSet.Add(I); Sel = I; break; }
		if (bMulti && MouseEvent.IsShiftDown()) { for (int32 K = FMath::Min(Sel, I); K <= FMath::Max(Sel, I); ++K) SelSet.Add(K); break; }
		if (!(SelSet.Contains(I) && SelSet.Num() > 1)) SelSet = { I };
		Sel = I;
		const FSlot& S = Slots[I];
		const bool bFixed = H.B == FldWho && S.Kind == EKind::You;
		const bool bHidden = H.B > FldWho && !Live(S);
		if (H.B >= 0 && !bFixed && !bHidden) OpenPop(I, H.B);
		break;
	}
	case EHit::Fader:
		bFaderDrag = true;
		SetCash(FMath::RoundToInt((P.X - 125.0f) / 356.03f * 4.0f));
		break;
	case EHit::Seg: Sw = H.A; Save(); break;
	case EHit::Base:
		if (H.B >= 0) { if (LanMayEdit(H.B, FldBase)) { DragSlot = H.B; DragFrom = DragPos = P; bDragMoved = false; } }
		else
		{
			// a base livre: entra o próximo da ordem que ainda não tem largada (como no ZH); sem ninguém na bandeja, o selecionado
			// (na sala da rede, o próximo que esta tela pode mexer)
			const TArray<int32> L = TrayList();
			int32 Who = -1;
			for (const int32 K : L) if (LanMayEdit(K, FldBase)) { Who = K; break; }
			if (Who >= 0) Place(Who, H.A);
			else if (Live(Slots[Sel]) && LanMayEdit(Sel, FldBase)) Place(Sel, H.A);
			Save();
		}
		break;
	case EHit::TrayToken: if (LanMayEdit(H.B, FldBase)) { DragSlot = H.B; DragFrom = DragPos = P; bDragMoved = false; } break;
	case EHit::TrayReset: for (int32 K = 0; K < Slots.Num(); ++K) if (LanMayEdit(K, FldBase)) Slots[K].Base = -1; Save(); break;
	default: break;
	}
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SRTSSkirmish::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2f P = StagePos(MyGeometry, MouseEvent);
	bFaderDrag = false;
	if (DragSlot >= 0)
	{
		const int32 D = DragSlot;
		DragSlot = -1;
		if (bDragMoved)
		{
			const int32 B = BaseAt(P);
			if (B >= 0) Place(D, B);
			else if (In(FVector4f(TRX, TRY, TRW, TRH), P)) Slots[D].Base = -1;
		}
		else
		{
			Sel = D;
		}
		bDragMoved = false;
		Save();
	}
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SRTSSkirmish::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey K = InKeyEvent.GetKey();
	if (bBrowser)
	{
		// o navegador: o Esc fecha, o Backspace apaga (com o Ctrl, a palavra), o Ctrl+V cola; as letras chegam no OnKeyChar
		if (K == EKeys::Escape) CloseBrowser();
		else if (K == EKeys::BackSpace)
		{
			if (InKeyEvent.IsControlDown())
			{
				Query.TrimEndInline();
				int32 Sp = INDEX_NONE;
				Query = Query.FindLastChar(TEXT(' '), Sp) ? Query.Left(Sp + 1) : FString();
			}
			else if (Query.Len()) Query.LeftChopInline(1);
			CaretT0 = Now;
			Refilter();
		}
		else if (K == EKeys::V && InKeyEvent.IsControlDown())
		{
			FString Clip;
			FPlatformApplicationMisc::ClipboardPaste(Clip);
			Clip.ReplaceInline(TEXT("\r"), TEXT(" "));
			Clip.ReplaceInline(TEXT("\n"), TEXT(" "));
			Query = (Query + Clip).Left(40);
			CaretT0 = Now;
			Refilter();
		}
		return FReply::Handled();
	}
	if (bLan)
	{
		if (bChatFocus)
		{
			// o campo do chat: o Enter manda (vazio não faz nada, como o mockup), o Esc solta o campo, o Backspace apaga (com o Ctrl, a
			// palavra), o Ctrl+V cola; as letras chegam no OnKeyChar
			if (K == EKeys::Escape) bChatFocus = false;
			else if (K == EKeys::Enter)
			{
				const FString Tx = ChatDraft.TrimStartAndEnd();
				if (Tx.Len()) { LanAct(TEXT("chat:") + Tx); ChatDraft.Reset(); }
			}
			else if (K == EKeys::BackSpace)
			{
				if (InKeyEvent.IsControlDown())
				{
					ChatDraft.TrimEndInline();
					int32 Sp = INDEX_NONE;
					ChatDraft = ChatDraft.FindLastChar(TEXT(' '), Sp) ? ChatDraft.Left(Sp + 1) : FString();
				}
				else if (ChatDraft.Len()) ChatDraft.LeftChopInline(1);
			}
			else if (K == EKeys::V && InKeyEvent.IsControlDown())
			{
				FString Clip;
				FPlatformApplicationMisc::ClipboardPaste(Clip);
				Clip.ReplaceInline(TEXT("\r"), TEXT(" "));
				Clip.ReplaceInline(TEXT("\n"), TEXT(" "));
				ChatDraft = (ChatDraft + Clip).Left(120);
			}
			CaretT0 = Now;
			return FReply::Handled();
		}
		if (K == EKeys::Escape)
		{
			if (PopField >= 0) PopField = -1;
			else if (SelSet.Num() > 1) SelSet = { Sel };
			else Back();
			return FReply::Handled();
		}
		if (K == EKeys::Enter) { StartMatch(); return FReply::Handled(); }
		if (K == EKeys::Delete && bLanHost)
		{
			for (const int32 I : SelSet) if (Slots.IsValidIndex(I) && Slots[I].Kind == EKind::Ai) Slots[I] = FSlot();
			Save();
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}
	if (K == EKeys::Escape)
	{
		if (PopField >= 0) PopField = -1;
		else if (SelSet.Num() > 1) SelSet = { Sel };
		else Back();
		return FReply::Handled();
	}
	if (K == EKeys::Delete)
	{
		for (const int32 I : SelSet) if (Slots.IsValidIndex(I) && Slots[I].Kind == EKind::Ai) Slots[I] = FSlot();
		Save();
		return FReply::Handled();
	}
	if (K == EKeys::Enter)
	{
		StartMatch();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FCursorReply SRTSSkirmish::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (bBrowser)
	{
		if (Hover.Kind == EHit::BwSearch) return FCursorReply::Cursor(EMouseCursor::TextEditBeam);
		const bool bH = Hover.Kind == EHit::BwBack || Hover.Kind == EHit::BwChip || Hover.Kind == EHit::BwCard || Hover.Kind == EHit::BwFav;
		return FCursorReply::Cursor(bH ? EMouseCursor::Hand : EMouseCursor::Default);
	}
	if (bLan)
	{
		// a mão no que esta tela pode mexer (o .lock do mockup não tem), a barra de texto no campo do chat
		if (Hover.Kind == EHit::ChatInput) return FCursorReply::Cursor(EMouseCursor::TextEditBeam);
		bool bHand = Hover.Kind != EHit::None && Hover.Kind != EHit::Outside;
		if (Hover.Kind == EHit::Cell) bHand = Hover.B >= 0 && LanMayEdit(Hover.A, Hover.B);
		else if (Hover.Kind == EHit::Base) bHand = Hover.B < 0 || LanMayEdit(Hover.B, FldBase);
		else if (Hover.Kind == EHit::TrayToken) bHand = LanMayEdit(Hover.B, FldBase);
		else if (Hover.Kind == EHit::Start) bHand = LanStartOn();
		if (DragSlot >= 0 && bDragMoved) return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
		return FCursorReply::Cursor(bHand ? EMouseCursor::Hand : EMouseCursor::Default);
	}
	if (DragSlot >= 0 && bDragMoved) return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
	const bool bHand = Hover.Kind != EHit::None && Hover.Kind != EHit::Outside
		&& !(Hover.Kind == EHit::Cell && (Hover.B < 0 || (Hover.B == FldWho && Slots[Hover.A].Kind == EKind::You)));
	return FCursorReply::Cursor(bHand ? EMouseCursor::Hand : EMouseCursor::Default);
}

namespace
{
	FString Money(int32 V)
	{
		FString D = FString::FromInt(V), Out;
		for (int32 I = 0; I < D.Len(); ++I) { if (I && (D.Len() - I) % 3 == 0) Out += TEXT(","); Out += D[I]; }
		return TEXT("$") + Out;
	}
	FString ShortCompass(const FString& C) { return C.Replace(TEXT("NORTH-"), TEXT("N-")).Replace(TEXT("SOUTH-"), TEXT("S-")); }
	/** A roda de cores da cor aleatória: o conic-gradient do mockup (as 8 paradas pelo ângulo a partir do centro, 0 pra cima e no
	 *  sentido horário, interpoladas em sRGB como o navegador), num leque de 112 fatias com os quatro cantos do retângulo. */
	void Wheel(FMesh& M, const FVector4f& R)
	{
		const FVector2f C(R.X + R.Z * 0.5f, R.Y + R.W * 0.5f);
		const float Hw = R.Z * 0.5f, Hh = R.W * 0.5f;
		const auto ColorAt = [](float T)
		{
			T = FMath::Clamp(T, 0.0f, 1.0f) * 7.0f;
			const int32 K = FMath::Min(6, int32(T));
			const FColor A = HexV(RANDOM_WHEEL[K]), B = HexV(RANDOM_WHEEL[K + 1]);
			const float F = T - K;
			return FColor(uint8(FMath::RoundToInt(A.R + (B.R - A.R) * F)), uint8(FMath::RoundToInt(A.G + (B.G - A.G) * F)),
				uint8(FMath::RoundToInt(A.B + (B.B - A.B) * F)), 255);
		};
		const auto Edge = [&](float Ang)   // o ponto da borda na direção do ângulo (do CSS)
		{
			const FVector2f Dir(FMath::Sin(Ang), -FMath::Cos(Ang));
			const float T = FMath::Min(FMath::Abs(Dir.X) > 1e-6f ? Hw / FMath::Abs(Dir.X) : 1e9f, FMath::Abs(Dir.Y) > 1e-6f ? Hh / FMath::Abs(Dir.Y) : 1e9f);
			return C + Dir * T;
		};
		TArray<float> Angs;
		for (int32 I = 0; I <= 112; ++I) Angs.Add(2.0f * PI * I / 112.0f);
		const float Ac = FMath::Atan2(Hw, Hh);   // os cantos: em cima à direita, embaixo à direita, embaixo à esquerda, em cima à esquerda
		Angs.Append({ Ac, PI - Ac, PI + Ac, 2.0f * PI - Ac });
		Angs.Sort();
		for (int32 I = 0; I + 1 < Angs.Num(); ++I)
		{
			const float A0 = Angs[I], A1 = Angs[I + 1];
			if (A1 - A0 < 1e-5f) continue;
			const int32 V0 = M.Vert(C, FVector2f(0, 0), ColorAt((A0 + A1) * 0.25f / PI));
			const int32 V1 = M.Vert(Edge(A0), FVector2f(0, 0), ColorAt(A0 * 0.5f / PI)), V2 = M.Vert(Edge(A1), FVector2f(0, 0), ColorAt(A1 * 0.5f / PI));
			M.Tri(V0, V1, V2);
		}
	}
}

int32 SRTSSkirmish::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FGeometry S = StageOf(AllottedGeometry);
	int32 L = LayerId;
	FSlateDrawElement::MakeBox(Out, L++, AllottedGeometry.ToPaintGeometry(), White(), ESlateDrawEffect::None, Css(10, 13, 11));
	// o fundo do palco (#stage): a cor, o brilho âmbar no alto à direita e a grade de 48 px
	const FPaintGeometry Full = S.ToPaintGeometry(FVector2f(StageW, StageH), FSlateLayoutTransform());
	FSlateDrawElement::MakeBox(Out, L++, Full, Image(TEXT("skirmish_glow.png"), FVector2f(480, 270)), ESlateDrawEffect::None, FLinearColor::White);
	{
		FMesh Grid(S);
		const FColor C = CssV(255, 255, 255, 0.02f);
		for (float X = 0.0f; X < StageW; X += 48.0f) Grid.Rect(X, 0.0f, 1.0f, StageH, C);
		for (float Y = 0.0f; Y < StageH; Y += 48.0f) Grid.Rect(0.0f, Y, StageW, 1.0f, C);
		Grid.Draw(Out, L++, White());
	}
	PaintMap(Out, L, S);
	FSlateDrawElement::MakeBox(Out, L++, Full, Image(TEXT("skirmish_vign.png"), FVector2f(480, 270)), ESlateDrawEffect::None, FLinearColor::White);
	PaintLink(Out, L, S);
	PaintColumn(Out, L, S);
	PaintButtons(Out, L, S);
	PaintPop(Out, L, S);
	PaintTip(Out, L, S);
	if (bBrowser) PaintBrowser(Out, L, S);
	return L;
}

void SRTSSkirmish::PaintColumn(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	const FLinearColor Label = Css(230, 220, 190, 0.42f);
	Text(Out, L, S, bLan ? LanKicker : FString(TEXT("SKIRMISH")), Font(EFace::Barlow600, 16, 8), 110, 104, Ac());
	const float Yo = bLan ? LAN_DY : 0.0f;
	Text(Out, L, S, TEXT("OPERATION"), Font(EFace::Barlow600, 14, 6), 110, 133, Css(230, 220, 190, 0.5f));
	Text(Out, L, S, OpName(), Font(EFace::Shoulders900, 62, 2), 110, 191, FLinearColor::White);
	// o mapa escolhido
	{
		FMesh M(S);
		BoxRect(M, FVector4f(110, 216.89f, 680, 75), CssV(16, 21, 19, 0.62f), CssV(230, 220, 190, 0.10f));
		const bool bHv = Hover.Kind == EHit::Change;
		BoxRect(M, CHANGE, bHv ? CssV(216, 173, 78, 0.06f) : FColor(0, 0, 0, 0), bHv ? AcV(0.75f) : CssV(230, 220, 190, 0.22f));
		// o FILL WITH AI, a dificuldade, o CLEAR AI e o TEAMS (na sala da rede também; quem entrou vê apagado)
		for (const FVector4f& R : TArray<FVector4f>{ FILL, CLEAR, TEAMS })
		{
			const bool bH = (R == FILL && Hover.Kind == EHit::Fill) || (R == CLEAR && Hover.Kind == EHit::Clear) || (R == TEAMS && Hover.Kind == EHit::Teams);
			BoxRect(M, R, CssV(16, 21, 19, 0.6f), bH ? AcV(0.7f) : CssV(230, 220, 190, 0.22f));
		}
		for (int32 D = 0; D < 3; ++D)
		{
			BoxRect(M, DIFF[D], D == FillDiff ? CssV(216, 173, 78, 0.07f) : FColor(0, 0, 0, 0), D == FillDiff ? AcV(0.6f) : CssV(230, 220, 190, 0.12f));
		}
		M.Draw(Out, L++, White());
	}
	if (Maps.IsValidIndex(MapIdx))
	{
		const FMapEntry& E = Maps[MapIdx];
		const TCHAR* Km = E.Size == TEXT("S") ? TEXT("0.5 KM") : E.Size == TEXT("L") ? TEXT("1.5 KM") : E.Size == TEXT("XL") ? TEXT("2 KM") : TEXT("1 KM");
		Text(Out, L, S, E.Name, Font(EFace::Shoulders800, 28, 3), 125, 254.89f, FLinearColor::White);
		Text(Out, L, S, FString::Printf(TEXT("%d PLAYERS · %s · %s · %s%s"), E.Players, Km, *E.Terr, *E.Cond, E.bNaval ? TEXT(" · ⚓ NAVAL") : TEXT("")),
			Font(EFace::Barlow600, 13, 3), 125, 275.89f, Css(152, 161, 154));
	}
	const FLinearColor Btn = Css(238, 241, 244, 0.85f);
	{
		Text(Out, L, S, TEXT("CHANGE MAP"), Font(EFace::Barlow700, 14, 3), CHANGE.X + 17, 259.89f, Hover.Kind == EHit::Change ? Ac() : Btn);
		Text(Out, L, S, TEXT("FILL WITH AI"), Font(EFace::Barlow700, 13, 3), FILL.X + 15, 329.89f, Hover.Kind == EHit::Fill ? Ac() : Btn);
		Text(Out, L, S, TEXT("CLEAR AI"), Font(EFace::Barlow700, 13, 3), CLEAR.X + 15, 329.89f, Hover.Kind == EHit::Clear ? Ac() : Btn);
		Text(Out, L, S, TEXT("TEAMS"), Font(EFace::Barlow700, 13, 3), TEAMS.X + 15, 329.89f, Hover.Kind == EHit::Teams ? Ac() : Btn);
		Chev(Out, L, S, FVector2f(772.4f, 322.0f), Hover.Kind == EHit::Teams ? Ac() : Btn, 3.5f);
		for (int32 D = 0; D < 3; ++D)
		{
			const FLinearColor C = D == FillDiff ? Ac() : (Hover.Kind == EHit::Diff && Hover.A == D ? FLinearColor::White : Css(238, 241, 244, 0.5f));
			Text(Out, L, S, AIS[D], Font(EFace::Barlow700, 12, 2), DIFF[D].X + 11, 329.89f, C);
		}
		if (SelSet.Num() > 1 && (!bLan || bLanHost))
		{
			// as vagas marcadas juntas (Ctrl e Shift): a etiqueta âmbar ao lado do TEAMS
			const FString Sx = FString::Printf(TEXT("%d SELECTED "), SelSet.Num());
			const float W = TextWidth(Sx, Font(EFace::Barlow700, 12, 2)) + 22.0f + 12.0f;
			FMesh M(S);
			M.Rect(TEAMS.X - 8 - W, TEAMS.Y, W, TEAMS.W, AcV());
			M.Draw(Out, L++, White());
			const float X = Text(Out, L, S, Sx, Font(EFace::Barlow700, 12, 2), TEAMS.X - 8 - W + 12, 329.89f, Css(10, 13, 11));
			Text(Out, L, S, TEXT("✕"), Font(EFace::Barlow700, 14), TEAMS.X - 8 - W + 12 + X, 330.0f, Css(10, 13, 11));
		}
	}
	// o cabeçalho da tabela
	const FSlateFontInfo Th = Font(EFace::Barlow600, 11.5f, 4);
	Text(Out, L, S, TEXT("PLAYER"), Th, 128, 373.89f + Yo, Label);
	Text(Out, L, S, TEXT("ARMY"), Th, 322, 373.89f + Yo, Label);
	Text(Out, L, S, TEXT("COLOR"), Th, 482, 373.89f + Yo, Label);
	Text(Out, L, S, TEXT("TEAM"), Th, 562, 373.89f + Yo, Label);
	Text(Out, L, S, TEXT("START"), Th, 640, 373.89f + Yo, Label);
	for (int32 I = 0; I < Rows(); ++I) PaintRow(Out, L, S, I);
	// as regras: o dinheiro (o seletor deslizante com os entalhes) e as superarmas
	const float RT = RowTop(Rows()) + 10.0f;
	const float Pct = Cash / 4.0f;
	{
		FMesh M(S);
		BoxRect(M, FVector4f(110, RT, 386.03f, 104), CssV(16, 21, 19, 0.5f), CssV(230, 220, 190, 0.10f));
		if (!bLan) BoxRect(M, FVector4f(504.03f, RT, 285.97f, 104), CssV(16, 21, 19, 0.5f), CssV(230, 220, 190, 0.10f));   // a rede de hoje não tem superarmas
		M.Rect(131, RT + 63, 344.03f, 4, CssV(230, 220, 190, 0.12f));
		if (Pct > 0.0f)
		{
			GlowRect(M, FVector4f(131, RT + 63, 344.03f * Pct, 4), 0xd8ad4e, 0.6f, 10.0f);
			M.Rect(131, RT + 63, 344.03f * Pct, 4, AcV());
		}
		for (int32 K = 0; K < 5; ++K) M.Rect(130 + 86.0075f * K, RT + 59, 2, 12, K <= Cash ? AcV() : CssV(230, 220, 190, 0.3f));
		const float Kx = 123 + 344.03f * Pct;
		GlowRect(M, FVector4f(Kx, RT + 58, 16, 18), 0x000000, 0.6f, 6.0f);   // a sombra do botão (0 2px 6px, 0,6)
		M.Rect(Kx, RT + 56, 16, 18, CssV(233, 226, 207));
		M.Rect(Kx, RT + 72, 16, 2, CssV(0, 0, 0, 0.25f));
		for (int32 K = 0; K < (bLan ? 0 : 3); ++K)
		{
			const FVector4f R(519.03f + K * 86.655f, RT + 41, 82.66f, 35);
			const bool bOn = K == Sw, bH = Hover.Kind == EHit::Seg && Hover.A == K;
			BoxRect(M, R, bOn ? CssV(216, 173, 78, 0.07f) : FColor(0, 0, 0, 0), bOn ? AcV(0.7f) : (bH ? CssV(230, 220, 190, 0.3f) : CssV(230, 220, 190, 0.12f)));
		}
		M.Draw(Out, L++, White());
	}
	Text(Out, L, S, TEXT("STARTING CASH"), Th, 125, RT + 40, Label);
	Text(Out, L, S, Money(CASH[Cash]), Font(EFace::Shoulders800, 30, 2), 481.04f, RT + 40, Ac(), 2);
	for (int32 K = 0; K < 5; ++K)
	{
		Text(Out, L, S, FString::Printf(TEXT("%dK"), CASH[K] / 1000), Font(EFace::Mono500, 11), 131 + 86.0075f * K, RT + 86, K <= Cash ? Css(238, 241, 244, 0.85f) : Css(230, 220, 190, 0.45f), 1);
	}
	if (!bLan)
	{
		Text(Out, L, S, TEXT("SUPERWEAPONS"), Th, 519.03f, RT + 25, Label);
		for (int32 K = 0; K < 3; ++K)
		{
			const bool bOn = K == Sw, bH = Hover.Kind == EHit::Seg && Hover.A == K;
			Text(Out, L, S, SWS[K], Font(EFace::Barlow700, 14, 2), 519.03f + K * 86.655f + 41.33f, RT + 64, bOn ? Ac() : (bH ? FLinearColor::White : Css(238, 241, 244, 0.55f)), 1);
		}
	}
	else if (!bLanHost)
	{
		// o .guest .rules e o .guest #change da página (opacity 0,55): o fundo do palco por cima, a 45%; a barra do FILL também (só o
		// anfitrião mexe na IA)
		++L;
		FMesh M(S);
		M.Rect(110, RT, 386.03f, 104, CssV(10, 13, 11, 0.45f));
		M.Rect(CHANGE.X, CHANGE.Y, CHANGE.Z, CHANGE.W, CssV(10, 13, 11, 0.45f));
		M.Rect(110, FILL.Y, 680, FILL.W, CssV(10, 13, 11, 0.45f));
		M.Draw(Out, L++, White());
	}
	if (bLan) PaintChat(Out, L, S);
}

void SRTSSkirmish::PaintRow(FSlateWindowElementList& Out, int32& L, const FGeometry& S, int32 I) const
{
	const FSlot& Sl = Slots[I];
	const float Top = RowTop(I), H = RowH(), Cy = Top + H * 0.5f;
	const bool bLive = Live(Sl), bHv = HoverRow == I, bSel = I == Sel, bMulti = SelSet.Num() > 1 && SelSet.Contains(I);
	{
		FMesh M(S);
		FColor Fill = bHv ? CssV(22, 28, 26, 0.78f) : CssV(16, 21, 19, 0.62f);
		FColor Border = bSel ? AcV(0.7f) : (bHv ? CssV(230, 220, 190, 0.24f) : CssV(230, 220, 190, 0.10f));
		if (bMulti) { Fill = CssV(216, 173, 78, 0.07f); Border = AcV(0.55f); }
		BoxRect(M, FVector4f(110, Top, 680, H), Fill, Border);
		// a barrinha da cor do jogador, com o brilho (box-shadow 0 0 12px)
		const FVector4f Bar(111, Top + 1, 6, H - 2);
		if (bLive)
		{
			const uint32 C = Sl.Color < 0 ? 0x9aa0a6 : COLORS[Sl.Color].Hex;
			GlowRect(M, Bar, C, 1.0f, 12.0f);
			M.Rect(Bar.X, Bar.Y, Bar.Z, Bar.W, HexV(C));
		}
		else
		{
			M.Rect(Bar.X, Bar.Y, Bar.Z, Bar.W, CssV(230, 220, 190, 0.12f));
		}
		for (int32 F = 0; F < 5; ++F)
		{
			M.Rect(CELLS[F].X, Top + 1, 1, H - 2, CssV(230, 220, 190, 0.05f));
			const bool bClickable = bLan ? (F == FldWho ? !LanWhoFixed(I) : bLive && CanEdit(I)) : (F == FldWho && Sl.Kind != EKind::You) || (F > FldWho && bLive);
			if (bHv && Hover.B == F && bClickable) M.Rect(CELLS[F].X, Top + 1, CELLS[F].W, H - 2, CssV(255, 255, 255, 0.035f));
		}
		if (bLive)
		{
			// a cor: o quadrado com o contorno escuro e o brilho; a aleatória é a roda de cores
			const FVector4f SwR(482, Cy - 13, 26, 26);
			M.Rect(SwR.X - 1, SwR.Y - 1, 28, 28, CssV(0, 0, 0, 0.5f));
			if (Sl.Color >= 0)
			{
				GlowRect(M, SwR, COLORS[Sl.Color].Hex, 1.0f, 10.0f);
				M.Rect(SwR.X, SwR.Y, SwR.Z, SwR.W, HexV(COLORS[Sl.Color].Hex));
			}
			else
			{
				Wheel(M, SwR);
			}
		}
		M.Draw(Out, L++, White());
	}
	const auto ChevColor = [&](int32 F) { return bHv && Hover.B == F ? Ac() : Css(230, 220, 190, 0.45f); };
	const auto ChevAt = [&](int32 F) { Chev(Out, L, S, FVector2f(CELLS[F].X + CELLS[F].W - 12.4f, Cy - 1.5f), ChevColor(F)); };
	const bool bNet = bLan && (Sl.Kind == EKind::You || Sl.Kind == EKind::Human);
	const FSlateFontInfo WhoF = Font(EFace::Shoulders800, bNet ? 19.0f : 22.0f, 2);
	const float Nw = Text(Out, L, S, WhoText(Sl), WhoF, 128, Cy + (bNet ? 7.5f : 8.0f), bLive ? FLinearColor::White : Css(238, 241, 244, 0.5f));
	if (bNet)
	{
		// o YOU âmbar ao lado do seu nome e o pronto (o círculo verde com o tique; tracejado se não está pronto)
		const FVector2f Rc(291, Top + 30);
		FMesh M(S);
		if (Sl.Kind == EKind::You) M.Rect(128 + Nw + 9, Top + 21.5f, 33, 17, AcV());
		if (Sl.bReady) Disc(M, Rc, 10, CssV(120, 200, 120, 0.18f));
		M.Draw(Out, L++, White());
		if (Sl.Kind == EKind::You) Text(Out, L, S, TEXT("YOU"), Font(EFace::Barlow700, 10.5f, 2), 128 + Nw + 14, Top + 34.5f, Css(10, 13, 11));
		if (Sl.bReady)
		{
			Lines(Out, L, S, Arc(Rc, 9.25f, 0.0f, 2.0f * PI, 40), Css(120, 200, 120), 1.5f);
			Lines(Out, L, S, { Rc + FVector2f(-3.5f, 0.2f), Rc + FVector2f(-1.2f, 2.5f), Rc + FVector2f(3.5f, -2.5f) }, Css(120, 200, 120), 1.8f);
		}
		else
		{
			Dashed(Out, L, S, Arc(Rc, 9.25f, 0.0f, 2.0f * PI, 40), Css(230, 220, 190, 0.3f), 1.5f, 3.0f);
		}
	}
	if (bNet)
	{
		// (06/11, o mockup) a coroa do anfitrião depois do nome (e do YOU) e o ping: as 4 barrinhas antes do pronto, na cor do ping
		FMesh M(S);
		if (Sl.bHost)
		{
			const float Cx = 128 + Nw + 9 + (Sl.Kind == EKind::You ? 42.0f : 0.0f), Ct = Cy - 5.5f;
			const auto Pt = [&](float X, float Yv) { return FVector2f(Cx + X * 15.0f / 16.0f, Ct + Yv * 11.0f / 12.0f); };
			const FColor Cr = AcV();
			M.Poly({ Pt(1, 11), Pt(3, 3), Pt(6, 7) }, Cr);
			M.Poly({ Pt(1, 11), Pt(6, 7), Pt(10, 7) }, Cr);
			M.Poly({ Pt(1, 11), Pt(10, 7), Pt(15, 11) }, Cr);
			M.Poly({ Pt(15, 11), Pt(10, 7), Pt(13, 3) }, Cr);
			M.Poly({ Pt(6, 7), Pt(8, 1), Pt(10, 7) }, Cr);
		}
		if (Sl.Ping > 0)
		{
			const FColor G = Sl.Ping < 60 ? CssV(120, 200, 120) : Sl.Ping < 120 ? AcV() : CssV(240, 110, 90);
			const int32 Bars = Sl.Ping < 40 ? 4 : Sl.Ping < 80 ? 3 : 2;
			const float Hb[4] = { 4, 7, 10, 14 };
			for (int32 K = 0; K < 4; ++K) M.Rect(256 + K * 5, Cy + 7 - Hb[K], 3, Hb[K], K < Bars ? G : CssV(230, 220, 190, 0.18f));
		}
		M.Draw(Out, L++, White());
	}
	if (bLan && bLanHost && Sl.Kind == EKind::Human && (bHv || (Hover.Kind == EHit::Kick && Hover.A == I)))
	{
		// o ✕ de tirar quem entrou (o .kick: 34 px pra fora da linha, aparece com o mouse na linha)
		const bool bK = Hover.Kind == EHit::Kick && Hover.A == I;
		FMesh M(S);
		BoxRect(M, FVector4f(798, Cy - 13, 26, 26), bK ? CssV(240, 110, 90, 0.15f) : FColor(0, 0, 0, 0), CssV(240, 110, 90, 0.5f));
		M.Draw(Out, L++, White());
		Text(Out, L, S, TEXT("✕"), Font(EFace::Barlow700, 14), 811, Cy + 5, Css(240, 110, 90), 1);
	}
	if (bLan ? !LanWhoFixed(I) : Sl.Kind != EKind::You) ChevAt(FldWho);
	if (!bLive) return;
	PaintFacToken(Out, L, S, FVector2f(322, Cy - 19), Sl.Fac, 38);
	Text(Out, L, S, FNAME[Sl.Fac], Font(EFace::Barlow700, 15, 2), 369, Cy + 6, FLinearColor::White);
	if (CanEdit(I)) ChevAt(FldFac);
	if (Sl.Color < 0) Text(Out, L, S, TEXT("?"), Font(EFace::Shoulders800, 15), 495, Cy + 5.5f, FLinearColor::White, 1);
	if (CanEdit(I)) ChevAt(FldColor);
	Text(Out, L, S, Sl.Team ? FString::FromInt(Sl.Team) : FString(TEXT("—")), Font(EFace::Shoulders800, 22, 2), 562, Cy + 8, FLinearColor::White);
	if (CanEdit(I)) ChevAt(FldTeam);
	PaintMini(Out, L, S, FVector2f(640, Cy - 13), Sl.Base);
	Text(Out, L, S, Sl.Base >= 0 && Maps.IsValidIndex(MapIdx) ? ShortCompass(Compass(Sl.Base)) : FString(TEXT("RANDOM")), Font(EFace::Barlow700, 15, 2), 675, Cy + 6, FLinearColor::White);
	if (CanEdit(I)) ChevAt(FldBase);
}

void SRTSSkirmish::PaintFacToken(FSlateWindowElementList& Out, int32 L, const FGeometry& S, FVector2f TopLeft, int32 Fac, float Size) const
{
	if (Fac == 0)
	{
		// o aleatório: o círculo tracejado com o "?"
		const FVector2f C = TopLeft + FVector2f(Size, Size) * 0.5f;
		Dashed(Out, L, S, Arc(C, Size * 0.5f - 0.75f, 0.0f, 2.0f * PI, 48), Css(230, 220, 190, 0.45f), 1.5f, 3.0f);
		Text(Out, L, S, TEXT("?"), Font(EFace::Shoulders800, 18), C.X, C.Y + 6.5f, Css(230, 220, 190, 0.7f), 1);
		return;
	}
	FSlateDrawElement::MakeBox(Out, L, S.ToPaintGeometry(FVector2f(Size, Size), FSlateLayoutTransform(TopLeft)),
		SkImage(FString::Printf(TEXT("token_%s.png"), FACS[Fac]), FVector2f(Size, Size)), ESlateDrawEffect::None, FLinearColor::White);
}

void SRTSSkirmish::PaintMini(FSlateWindowElementList& Out, int32 L, const FGeometry& S, FVector2f O, int32 Base) const
{
	// o minimapinha da largada (miniSvg do mockup): o losango e as bases, a escolhida em branco
	FMesh M(S);
	M.Poly({ O + FVector2f(13, 1.5f), O + FVector2f(24.5f, 13), O + FVector2f(13, 24.5f), O + FVector2f(1.5f, 13) }, CssV(216, 173, 78, 0.08f));
	if (Base >= 0 && Maps.IsValidIndex(MapIdx))
	{
		for (int32 K = 0; K < Maps[MapIdx].Starts.Num(); ++K)
		{
			const FVector2f D = ToDiamond(Maps[MapIdx].Starts[K]);
			const FVector2f P = O + FVector2f(13 + (D.X - DX) / HALF * 11, 13 + (D.Y - DY) / HALF * 11);
			Disc(M, P, K == Base ? 3.2f : 1.6f, K == Base ? FColor::White : CssV(230, 220, 190, 0.35f), 16);
		}
	}
	M.Draw(Out, L, White());
	Lines(Out, L, S, { O + FVector2f(13, 1.5f), O + FVector2f(24.5f, 13), O + FVector2f(13, 24.5f), O + FVector2f(1.5f, 13), O + FVector2f(13, 1.5f) }, Ac(0.55f), 1.0f);
	if (Base < 0) Text(Out, L, S, TEXT("?"), Font(EFace::Shoulders800, 11), O.X + 13, O.Y + 17, Css(230, 220, 190, 0.7f), 1);
}

void SRTSSkirmish::PaintToken(FSlateWindowElementList& Out, int32 L, const FGeometry& S, FVector2f C, const FSlot& Slot, float R, float Alpha) const
{
	// a ficha (token do mockup): a sombra, o disco na cor do jogador com a luz de cima e o escuro de baixo, o aro branco e o brasão
	FMesh M(S);
	Disc(M, C + FVector2f(2, 4), R + 2, CssV(0, 0, 0, 0.45f * Alpha));
	const uint32 Hc = Slot.Color < 0 ? 0x9aa0a6 : COLORS[Slot.Color].Hex;
	Disc(M, C, R, HexV(Hc, Alpha));
	M.Draw(Out, L, White());
	FMesh G(S);
	const int32 Steps = FMath::Max(8, int32(R));
	for (int32 K = 0; K < Steps; ++K)
	{
		const float Y0 = -R + 2 * R * K / Steps, Y1 = -R + 2 * R * (K + 1) / Steps;
		const auto Col = [&](float Y)
		{
			const float Tt = (Y + R) / (2 * R);
			if (Tt < 0.5f) { const uint8 A = uint8(255 * 0.22f * (1 - 2 * Tt) * Alpha); return FColor(A, A, A, A); }
			return FColor(0, 0, 0, uint8(255 * 0.28f * (2 * Tt - 1) * Alpha));
		};
		const float W0 = FMath::Sqrt(FMath::Max(0.0f, R * R - Y0 * Y0)), W1 = FMath::Sqrt(FMath::Max(0.0f, R * R - Y1 * Y1));
		const int32 A = G.Vert(C + FVector2f(-W0, Y0), FVector2f(0, 0), Col(Y0)), B = G.Vert(C + FVector2f(W0, Y0), FVector2f(0, 0), Col(Y0));
		const int32 D = G.Vert(C + FVector2f(W1, Y1), FVector2f(0, 0), Col(Y1)), E = G.Vert(C + FVector2f(-W1, Y1), FVector2f(0, 0), Col(Y1));
		G.Tri(A, B, D); G.Tri(A, D, E);
	}
	G.Draw(Out, L, White(), ESlateDrawEffect::PreMultipliedAlpha);
	Lines(Out, L, S, Arc(C, R, 0.0f, 2.0f * PI, 48), FLinearColor(1, 1, 1, 0.9f * Alpha), 2.0f);
	if (Slot.Fac > 0)
	{
		FSlateDrawElement::MakeBox(Out, L + 1, S.ToPaintGeometry(FVector2f(R * 1.4f, R * 1.4f), FSlateLayoutTransform(C - FVector2f(R * 0.7f, R * 0.7f))),
			SkImage(FString::Printf(TEXT("glyph_%s.png"), FACS[Slot.Fac]), FVector2f(64, 64)), ESlateDrawEffect::None, FLinearColor(1, 1, 1, Alpha));
	}
	else
	{
		Text(Out, L + 1, S, TEXT("?"), Font(EFace::Shoulders800, R), C.X, C.Y + R * 0.36f, FLinearColor(1, 1, 1, Alpha), 1);
	}
}

void SRTSSkirmish::PaintLabel(FSlateWindowElementList& Out, int32 L, const FGeometry& S, FVector2f At, const FSlot& Slot, float Alpha) const
{
	const FString Lab = Slot.Kind == EKind::You ? FString(TEXT("YOU")) : WhoText(Slot);
	const FSlateFontInfo F = Font(EFace::Shoulders800, 15);
	const float Lw = TextWidth(Lab, F, S.GetAccumulatedLayoutTransform().GetScale()) + 16.0f;
	FMesh M(S);
	M.Rect(At.X - Lw * 0.5f, At.Y, Lw, 22, CssV(8, 10, 9, 0.88f * Alpha));
	M.Rect(At.X - Lw * 0.5f, At.Y, 3, 22, HexV(Slot.Color < 0 ? 0x9aa0a6 : COLORS[Slot.Color].Hex, Alpha));
	M.Draw(Out, L, White());
	Text(Out, L, S, Lab, F, At.X + 1, At.Y + 17, FLinearColor(1, 1, 1, Alpha), 1);
}

void SRTSSkirmish::PaintMap(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	if (!Maps.IsValidIndex(MapIdx)) return;
	const FMapEntry& E = Maps[MapIdx];
	// o losango pronto (o chão, a sombra, a vinheta e as molduras; Tools/export_skirmish_maps.py, em 2×)
	FSlateDrawElement::MakeBox(Out, L++, S.ToPaintGeometry(FVector2f(DIA, DIA), FSlateLayoutTransform(FVector2f(DX - DIA * 0.5f, DY - DIA * 0.5f))),
		SkImage(FString::Printf(TEXT("maps/%s_diamond.png"), *E.Real), FVector2f(DIA * 2, DIA * 2)), ESlateDrawEffect::None, FLinearColor(1, 1, 1, Fade));
	// os pontos cardeais
	{
		const TPair<const TCHAR*, FVector2f> Pts[4] = { { TEXT("N"), FVector2f(DX, DY - HALF - 30) }, { TEXT("E"), FVector2f(DX + HALF + 30, DY) },
			{ TEXT("S"), FVector2f(DX, DY + HALF + 30) }, { TEXT("W"), FVector2f(DX - HALF - 30, DY) } };
		FMesh M(S);
		for (const auto& P : Pts) Disc(M, P.Value, 15, CssV(8, 10, 9, 0.9f));
		M.Draw(Out, L++, White());
		for (const auto& P : Pts)
		{
			Lines(Out, L, S, Arc(P.Value, 15, 0.0f, 2.0f * PI, 40), Ac(0.6f), 1.2f);
			Text(Out, L, S, P.Key, Font(EFace::Shoulders800, 20), P.Value.X, P.Value.Y + 8.0f, FCString::Strcmp(P.Key, TEXT("N")) == 0 ? Ac() : Css(238, 241, 244, 0.8f), 1);
		}
		++L;
	}
	const int32 N = Rows();
	const float TR = N > 8 ? 17.0f : N > 4 ? 22.0f : 27.0f;
	TArray<FVector2f> P;
	for (const FVector2f& St : E.Starts) P.Add(ToDiamond(St));
	TArray<int32> Placed;
	for (int32 I = 0; I < FMath::Min(N, Slots.Num()); ++I) if (Live(Slots[I]) && Slots[I].Base >= 0) Placed.Add(I);
	// as docas do meio (as disputadas) pulsam
	for (const FVector2f& Dk : E.Docks)
	{
		const float Ph = FMath::Fmod(T * 0.7f, 1.0f);
		Lines(Out, L, S, Arc(ToDiamond(Dk), 8 + Ph * 18, 0.0f, 2.0f * PI, 40), Css(255, 224, 150, (1 - Ph) * 0.7f), 1.5f);
	}
	// as alianças
	for (int32 A = 0; A < Placed.Num(); ++A)
	{
		for (int32 B = A + 1; B < Placed.Num(); ++B)
		{
			const FSlot& Sa = Slots[Placed[A]];
			const FSlot& Sb = Slots[Placed[B]];
			if (!Sa.Team || Sa.Team != Sb.Team) continue;
			Lines(Out, L, S, { P[Sa.Base], P[Sb.Base] }, Css(0, 0, 0, 0.5f), 5.0f);
			Lines(Out, L + 1, S, { P[Sa.Base], P[Sb.Base] }, Css(232, 192, 96, 0.95f), 2.4f);
		}
	}
	L += 2;
	// o tempo até o 1º contato com o inimigo mais perto (cada par uma vez): pelo caminho ~1,37× a reta, o tanque a ~7 m/s
	TSet<int32> Pairs;
	for (const int32 Ia : Placed)
	{
		int32 Best = -1;
		float Bd = 1e9f;
		for (const int32 Ib : Placed)
		{
			if (Ib == Ia || (Slots[Ia].Team && Slots[Ia].Team == Slots[Ib].Team)) continue;
			const float D = FVector2f::Distance(E.Starts[Slots[Ia].Base], E.Starts[Slots[Ib].Base]);
			if (D < Bd) { Bd = D; Best = Ib; }
		}
		if (Best < 0) continue;
		const int32 Key = FMath::Min(Ia, Best) * 64 + FMath::Max(Ia, Best);
		if (Pairs.Contains(Key)) continue;
		Pairs.Add(Key);
		const FVector2f Pa = P[Slots[Ia].Base], Pb = P[Slots[Best].Base], Mid = (Pa + Pb) * 0.5f;
		const float Secs = Bd * 1.37f / 7.0f;
		const FString Txt = FString::Printf(TEXT("%d:%02d"), FMath::FloorToInt(Secs / 60.0f), FMath::RoundToInt(FMath::Fmod(Secs, 60.0f)));
		Dashed(Out, L, S, { Pa, Pb }, Css(0, 0, 0, 0.45f), 3.5f, 5.0f);
		Dashed(Out, L + 1, S, { Pa, Pb }, Css(255, 120, 96, 0.85f), 1.6f, 5.0f);
		const FSlateFontInfo Mf = Font(EFace::Mono600, 13);
		const float Tw = TextWidth(Txt, Mf, S.GetAccumulatedLayoutTransform().GetScale()) + 20.0f;
		FMesh M(S);
		BoxRect(M, FVector4f(Mid.X - Tw * 0.5f - 6, Mid.Y - 12, Tw + 12, 24), CssV(10, 12, 11, 0.92f), CssV(255, 120, 96, 0.7f));
		M.Draw(Out, L + 2, White());
		// as espadas cruzadas: o encontro
		const FVector2f G(Mid.X - Tw * 0.5f + 5, Mid.Y);
		const FLinearColor Sc = Css(255, 150, 130, 0.95f);
		Lines(Out, L + 3, S, { G + FVector2f(-5, -5), G + FVector2f(5, 5) }, Sc, 1.5f);
		Lines(Out, L + 3, S, { G + FVector2f(5, -5), G + FVector2f(-5, 5) }, Sc, 1.5f);
		Lines(Out, L + 3, S, { G + FVector2f(-6, 2), G + FVector2f(-2, 6) }, Sc, 1.5f);
		Lines(Out, L + 3, S, { G + FVector2f(6, 2), G + FVector2f(2, 6) }, Sc, 1.5f);
		Text(Out, L + 3, S, Txt, Mf, Mid.X - Tw * 0.5f + 16, Mid.Y + 5.5f, Css(255, 170, 150, 0.95f));
	}
	L += 4;
	// as bases: a ficha na cor do jogador; a livre, tracejada (no hover, a ficha fantasma de quem cai ali)
	const int32 Dragging = DragSlot >= 0 && bDragMoved ? DragSlot : -1;
	const TArray<int32> Tray = TrayList();
	for (int32 K = 0; K < P.Num(); ++K)
	{
		const int32 O = Occupant(K);
		const bool bHv = K == HoverBase || (HoverRow >= 0 && HoverRow < Slots.Num() && Slots[HoverRow].Base == K);
		const FVector2f C = P[K];
		if (O < 0 || O == Dragging)
		{
			const float Pulse = Dragging >= 0 ? 0.5f + 0.5f * FMath::Sin(T * 6.0f) : 0.0f;
			FMesh M(S);
			Disc(M, C, TR - 3, CssV(8, 10, 9, 0.55f));
			M.Draw(Out, L, White());
			Dashed(Out, L + 1, S, Arc(C, TR - 3 + (Dragging >= 0 ? Pulse * 3 : 0), 0.0f, 2.0f * PI, 48),
				(bHv || Dragging >= 0) ? FLinearColor(1, 1, 1, 0.75f + Pulse * 0.25f) : Css(238, 241, 244, 0.7f), Dragging >= 0 ? 2.2f : 1.6f, 4.0f);
			if (K == HoverBase && Dragging < 0 && Tray.Num())
			{
				PaintToken(Out, L + 2, S, C, Slots[Tray[0]], TR - 2, 0.6f);
				PaintLabel(Out, L + 3, S, FVector2f(C.X, C.Y + TR + 5), Slots[Tray[0]], 0.85f);
			}
			else
			{
				Lines(Out, L + 1, S, { C + FVector2f(-7, 0), C + FVector2f(7, 0) }, Css(238, 241, 244, 0.8f), 2.0f);
				Lines(Out, L + 1, S, { C + FVector2f(0, -7), C + FVector2f(0, 7) }, Css(238, 241, 244, 0.8f), 2.0f);
			}
			continue;
		}
		PaintToken(Out, L + 2, S, C, Slots[O], TR);
		if (N <= 8 || bHv) PaintLabel(Out, L + 3, S, FVector2f(C.X, C.Y + TR + 7), Slots[O], 1.0f);
		if (bHv || O == Sel)
		{
			const float Rr = TR + 10 + FMath::Sin(T * 5.0f) * 1.5f, Cn = 8.0f;
			for (const FVector2f Sg : { FVector2f(-1, -1), FVector2f(1, -1), FVector2f(-1, 1), FVector2f(1, 1) })
			{
				Lines(Out, L + 3, S, { C + FVector2f(Sg.X * Rr, Sg.Y * (Rr - Cn)), C + FVector2f(Sg.X * Rr, Sg.Y * Rr), C + FVector2f(Sg.X * (Rr - Cn), Sg.Y * Rr) }, FLinearColor(1, 1, 1, 0.9f), 1.6f);
			}
		}
	}
	L += 4;
	PaintTray(Out, L, S);
	if (Dragging >= 0) { PaintToken(Out, L, S, DragPos, Slots[Dragging], TR, 0.92f); L += 2; }   // a ficha arrastada segue o mouse
	if (Launch > 0.0f)
	{
		for (const FVector2f& C : P) Lines(Out, L, S, Arc(C, 30 + (1.4f - Launch) * 60, 0.0f, 2.0f * PI, 64), FLinearColor(1, 1, 1, Launch * 0.5f), 2.0f);
		++L;
	}
}

void SRTSSkirmish::PaintTray(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	// a bandeja da largada sorteada: quem está aqui começa numa base livre qualquer
	const TArray<int32> Tl = TrayList();
	const bool bOver = DragSlot >= 0 && bDragMoved && In(FVector4f(TRX, TRY, TRW, TRH), DragPos);
	const float Cy = TRY + TRH * 0.5f;
	Text(Out, L, S, TEXT("RANDOM START"), Font(EFace::Barlow600, 11.5f, 4), TRX, TRY - 9, Css(230, 220, 190, 0.5f));
	{
		FMesh M(S);
		M.Rect(TRX, TRY, TRW, TRH, bOver ? CssV(40, 34, 20, 0.9f) : CssV(12, 15, 14, 0.85f));
		const float Ix = TRX + 36;
		M.Poly({ FVector2f(Ix, Cy - 17), FVector2f(Ix + 17, Cy), FVector2f(Ix, Cy + 17), FVector2f(Ix - 17, Cy) }, CssV(216, 173, 78, 0.1f));
		M.Draw(Out, L++, White());
		Dashed(Out, L, S, { FVector2f(TRX + 0.5f, TRY + 0.5f), FVector2f(TRX + TRW - 0.5f, TRY + 0.5f), FVector2f(TRX + TRW - 0.5f, TRY + TRH - 0.5f),
			FVector2f(TRX + 0.5f, TRY + TRH - 0.5f), FVector2f(TRX + 0.5f, TRY + 0.5f) }, bOver ? Ac(0.95f) : Css(230, 220, 190, 0.25f), 1.2f, 4.5f);
		Lines(Out, L, S, { FVector2f(Ix, Cy - 17), FVector2f(Ix + 17, Cy), FVector2f(Ix, Cy + 17), FVector2f(Ix - 17, Cy), FVector2f(Ix, Cy - 17) }, Ac(0.7f), 1.2f);
		Text(Out, L, S, TEXT("?"), Font(EFace::Shoulders800, 18), Ix, Cy + 7, Ac(), 1);
		++L;
	}
	const float Tr = FMath::Min(21.0f, TraySpacing() * 0.5f - 3.0f);
	for (int32 K = 0; K < Tl.Num(); ++K)
	{
		if (DragSlot == Tl[K] && bDragMoved) continue;
		const FVector2f C = TrayPos(K);
		PaintToken(Out, L, S, C, Slots[Tl[K]], Tr);
		if (HoverRow == Tl[K] || Sel == Tl[K])
		{
			const float Rr = Tr + 8, Cn = 8.0f;
			for (const FVector2f Sg : { FVector2f(-1, -1), FVector2f(1, -1), FVector2f(-1, 1), FVector2f(1, 1) })
			{
				Lines(Out, L + 1, S, { C + FVector2f(Sg.X * Rr, Sg.Y * (Rr - Cn)), C + FVector2f(Sg.X * Rr, Sg.Y * Rr), C + FVector2f(Sg.X * (Rr - Cn), Sg.Y * Rr) }, FLinearColor(1, 1, 1, 0.9f), 1.6f);
			}
		}
	}
	L += 2;
	int32 Alive = 0;
	for (int32 I = 0; I < FMath::Min(Rows(), Slots.Num()); ++I) if (Live(Slots[I])) ++Alive;
	if (Tl.Num() < Alive)
	{
		// ↺: todos de volta pra largada sorteada
		const FVector2f R(TRX + TRW - 30, Cy);
		const FLinearColor C = Css(238, 241, 244, 0.75f);
		Lines(Out, L, S, Arc(R, 9, -PI * 0.35f, PI * 1.35f, 24), C, 1.8f);
		const FVector2f A = R + FVector2f(FMath::Cos(-PI * 0.35f), FMath::Sin(-PI * 0.35f)) * 9.0f;
		Lines(Out, L, S, { A + FVector2f(4, -3), A, A + FVector2f(5, 3) }, C, 1.8f);
		++L;
	}
}

void SRTSSkirmish::PaintLink(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	// a linha tracejada da vaga em hover até a base dela (ou até a ficha na bandeja)
	if (HoverRow < 0 || HoverRow >= Rows() || !Live(Slots[HoverRow]) || !Maps.IsValidIndex(MapIdx)) return;
	const FSlot& Sl = Slots[HoverRow];
	FVector2f P1;
	if (Sl.Base >= 0) P1 = ToDiamond(Maps[MapIdx].Starts[Sl.Base]);
	else
	{
		const int32 K = TrayList().Find(HoverRow);
		if (K < 0) return;
		P1 = TrayPos(K);
	}
	const float X0 = 790 + 12, Y0 = RowTop(HoverRow) + RowH() * 0.5f, Off = Sl.Base >= 0 ? 38.0f : 30.0f;
	Dashed(Out, L++, S, { FVector2f(X0, Y0), FVector2f(X0 + 36, Y0), FVector2f(P1.X - Off - 14, P1.Y), FVector2f(P1.X - Off, P1.Y) }, Ac(0.9f), 2.0f, 5.0f);
}

void SRTSSkirmish::PaintButtons(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	// o BACK
	const bool bBack = Hover.Kind == EHit::Back;
	const FLinearColor Bc = bBack ? Ac() : Css(238, 241, 244, 0.9f);
	{
		FMesh M(S);
		BoxRect(M, BACK, CssV(16, 21, 19, 0.85f), bBack ? AcV(0.8f) : CssV(230, 220, 190, 0.32f));
		// a moldura do START com o brilho (box-shadow 0 0 0 1px e 0 0 40px; no hover, 60px mais forte)
		const FVector4f Sr = START;
		// (06/11) o .start.cd do mockup (o fundo creme): a contagem e o READY já apertado
		const FSlot* MeS = Slots.FindByPredicate([](const FSlot& X) { return X.Kind == EKind::You; });
		const bool bCd = bLan && (bLanHost ? HostCd > 0 : (LanCountdown > 0 || (MeS && MeS->bReady)));
		if (LanStartOn())
		{
			GlowRect(M, Sr, 0xd8ad4e, 0.25f + 0.2f * StartHover, 40.0f + 20.0f * StartHover);
			M.Rect(Sr.X - 1, Sr.Y - 1, Sr.Z + 2, Sr.W + 2, AcV(0.9f + 0.1f * StartHover));
			M.Rect(Sr.X, Sr.Y, Sr.Z, Sr.W, bCd ? CssV(233, 226, 207) : AcV());
		}
		else
		{
			M.Rect(Sr.X, Sr.Y, Sr.Z, Sr.W, CssV(230, 220, 190, 0.12f));   // o .start.no
		}
		// as cantoneiras do palco
		const FColor Cc = CssV(216, 173, 78, 0.45f);
		M.Rect(40, 40, 24, 2, Cc); M.Rect(40, 40, 2, 24, Cc);
		M.Rect(1856, 40, 24, 2, Cc); M.Rect(1878, 40, 2, 24, Cc);
		M.Rect(40, 1038, 24, 2, Cc); M.Rect(40, 1016, 2, 24, Cc);
		M.Rect(1856, 1038, 24, 2, Cc); M.Rect(1878, 1016, 2, 24, Cc);
		M.Draw(Out, L++, White());
	}
	Lines(Out, L, S, { FVector2f(134.75f, 45.58f), FVector2f(128.33f, 52.0f), FVector2f(134.75f, 58.42f) }, Bc, 2.2f);
	Text(Out, L, S, TEXT("BACK"), Font(EFace::Shoulders800, 19, 3), 155, 59.5f, Bc);
	{
		const FLinearColor Kc(Bc.R, Bc.G, Bc.B, Bc.A * 0.7f);
		Lines(Out, L, S, { FVector2f(213.23f, 42.5f), FVector2f(249.04f, 42.5f), FVector2f(249.04f, 61.5f), FVector2f(213.23f, 61.5f), FVector2f(213.23f, 42.5f) }, Kc, 1.0f);
		Text(Out, L, S, TEXT("ESC"), Font(EFace::Mono600, 11, 1), 219.73f, 56, Kc);
	}
	if (!LanStartOn())
	{
		// o .start.no (só o anfitrião da rede): 24 px, espaçamento 3, apagado; espera quem não apertou READY, ou alguém entrar
		int32 Waiting = 0;
		for (const FSlot& X : Slots) if (X.Kind == EKind::Human && !X.bReady) ++Waiting;
		const FString No = Waiting ? FString::Printf(TEXT("WAITING · %d NOT READY"), Waiting) : FString(TEXT("WAITING FOR PLAYERS"));
		Text(Out, L, S, No, Font(EFace::Shoulders900, 24, 3), START.X + START.Z * 0.5f, 1003.5f, Css(238, 241, 244, 0.5f), 1);
	}
	// o texto e a tecla, centrados juntos (o espaçamento abre no hover): START MATCH; na sala da rede, a contagem (STARTING n, CANCEL) e
	// o READY de quem entrou (READY ✓ e CANCEL depois de apertado), como o startBtn do mockup
	else
	{
		FString Label = TEXT("START MATCH"), Kbd = TEXT("ENTER");
		bool bTick = false;
		if (bLan && bLanHost && HostCd > 0) { Label = FString::Printf(TEXT("STARTING %d"), HostCd); Kbd = TEXT("CANCEL"); }
		else if (bLan && !bLanHost)
		{
			const FSlot* Me = Slots.FindByPredicate([](const FSlot& X) { return X.Kind == EKind::You; });
			const bool bR = Me && Me->bReady;
			Kbd = bR ? TEXT("CANCEL") : TEXT("ENTER");
			if (LanCountdown > 0) Label = FString::Printf(TEXT("STARTING %d"), LanCountdown);
			else { Label = TEXT("READY"); bTick = bR; }
		}
		const FSlateFontInfo F = Font(EFace::Shoulders900, 34, 6.0f + 2.0f * StartHover), Kf = Font(EFace::Mono600, 12, 1);
		const float Scale = S.GetAccumulatedLayoutTransform().GetScale();
		const float Tw = TextWidth(Label + TEXT(" "), F, Scale) - TextWidth(TEXT(" "), F, Scale);
		const float Kw = FMath::RoundToFloat(TextWidth(Kbd, Kf, Scale) + 14.0f), TickW = bTick ? 30.0f : 0.0f;
		const float Total = Tw + TickW + 14.0f + Kw;
		const float X0 = START.X + (START.Z - Total) * 0.5f;
		Text(Out, L, S, Label, F, X0, 1007, Css(10, 13, 11));
		if (bTick) Lines(Out, L, S, { FVector2f(X0 + Tw + 4, 995), FVector2f(X0 + Tw + 11, 1003), FVector2f(X0 + Tw + 25, 985) }, Css(10, 13, 11), 3.5f);
		const float Kx = X0 + Total - Kw;
		Lines(Out, L, S, { FVector2f(Kx + 0.5f, 982.5f), FVector2f(Kx + Kw - 0.5f, 982.5f), FVector2f(Kx + Kw - 0.5f, 1005.0f), FVector2f(Kx + 0.5f, 1005.0f), FVector2f(Kx + 0.5f, 982.5f) },
			Css(10, 13, 11, 0.4f), 1.0f);
		Text(Out, L, S, Kbd, Kf, Kx + 7, 998.5f, Css(10, 13, 11, 0.7f));
	}
	++L;
}

void SRTSSkirmish::PaintPop(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	if (PopField < 0) return;
	const FVector4f B = PopBox;
	{
		FMesh M(S);
		// a sombra (0 18px 40px, 0,6) e a caixa
		GlowRect(M, FVector4f(B.X, B.Y + 18.0f, B.Z, B.W), 0x000000, 0.6f, 40.0f);
		BoxRect(M, B, CssV(18, 23, 21), CssV(230, 220, 190, 0.18f));
		for (int32 I = 0; I < PopItems.Num(); ++I)
		{
			const FPopItem& It = PopItems[I];
			if (It.bSep) { M.Rect(It.R.X, It.R.Y, It.R.Z, 1, CssV(230, 220, 190, 0.1f)); continue; }
			if (PopField == FldColor)
			{
				if (I == 0)
				{
					if (It.bCur) M.Rect(It.R.X - 2, It.R.Y - 2, It.R.Z + 4, It.R.W + 4, FColor::White);
					Wheel(M, It.R);
					continue;
				}
				const float Grow = I == PopHover && !It.bDisabled ? 34.0f * 0.06f : 0.0f;
				const FVector4f R(It.R.X - Grow, It.R.Y - Grow, It.R.Z + 2 * Grow, It.R.W + 2 * Grow);
				const float A = It.bDisabled ? 0.3f : 1.0f;
				M.Rect(R.X - 1, R.Y - 1, R.Z + 2, R.W + 2, It.bCur ? FColor::White : CssV(0, 0, 0, 0.5f * A));
				if (It.bCur) M.Rect(R.X - 2, R.Y - 2, R.Z + 4, R.W + 4, FColor::White);
				M.Rect(R.X, R.Y, R.Z, R.W, HexV(COLORS[It.Value].Hex, A));
				continue;
			}
			if (PopField == FldTeam)
			{
				const bool bH = I == PopHover;
				BoxRect(M, It.R, It.bCur ? CssV(216, 173, 78, 0.08f) : FColor(0, 0, 0, 0), It.bCur ? AcV(0.8f) : (bH ? AcV(0.6f) : CssV(230, 220, 190, 0.14f)));
				continue;
			}
			if (I == PopHover) M.Rect(It.R.X, It.R.Y, It.R.Z, It.R.W, CssV(216, 173, 78, 0.1f));
		}
		M.Draw(Out, L++, White());
	}
	const auto ItemColor = [&](int32 I) { return PopItems[I].bCur ? Ac() : (I == PopHover ? FLinearColor::White : Css(238, 241, 244, 0.85f)); };
	for (int32 I = 0; I < PopItems.Num(); ++I)
	{
		const FPopItem& It = PopItems[I];
		if (It.bSep) continue;
		const FVector4f R = It.R;
		switch (PopField)
		{
		case FldWho:
			Text(Out, L, S, It.Value == 100 ? FString(TEXT("OPEN")) : It.Value == 101 ? FString(TEXT("CLOSED")) : FString::Printf(TEXT("AI · %s"), AIS[It.Value]),
				Font(EFace::Barlow700, 15, 2), R.X + 10, R.Y + 23, ItemColor(I));
			break;
		case FldTeams:
		{
			const TCHAR* Names[6] = { TEXT("FREE FOR ALL"), TEXT("2 TEAMS"), TEXT("4 TEAMS"), TEXT("NORTH VS SOUTH"), TEXT("WEST VS EAST"), TEXT("YOU VS ALL AI") };
			Text(Out, L, S, Names[It.Value], Font(EFace::Barlow700, 15, 2), R.X + 10, R.Y + 23, ItemColor(I));
			break;
		}
		case FldFac:
			PaintFacToken(Out, L, S, FVector2f(R.X + 10, R.Y + 8), It.Value, It.Value == 0 ? 46.0f : 44.0f);
			Text(Out, L, S, FNAME[It.Value], Font(EFace::Barlow700, 17, 2), R.X + (It.Value == 0 ? 66.0f : 64.0f), R.Y + (It.Value == 0 ? 38.0f : 37.0f), ItemColor(I));
			break;
		case FldBase:
		{
			PaintMini(Out, L, S, FVector2f(R.X + 10, R.Y + 8), It.Value);
			Text(Out, L, S, It.Value < 0 ? FString(TEXT("RANDOM")) : Compass(It.Value), Font(EFace::Barlow700, 15, 2), R.X + 46, R.Y + 27, ItemColor(I));
			const int32 O = It.Value >= 0 ? [&]() { for (int32 K = 0; K < Rows(); ++K) if (K != PopRow && Live(Slots[K]) && Slots[K].Base == It.Value) return K; return -1; }() : -1;
			if (O >= 0)
			{
				Text(Out, L, S, Slots[O].Kind == EKind::You ? TEXT("● YOU") : TEXT("● AI"), Font(EFace::Mono500, 11), R.X + R.Z - 10, R.Y + 27, SlotColor(Slots[O]), 2);
			}
			break;
		}
		case FldColor:
			if (I == 0)
			{
				// o text-shadow 0 1px 3px #000 do mockup: cópias pretas fracas em volta do deslocamento de 1 px, e o texto branco por cima
				const FSlateFontInfo Rf = Font(EFace::Shoulders800, 16, 3);
				for (int32 Dy = -1; Dy <= 1; ++Dy)
					for (int32 Dx = -1; Dx <= 1; ++Dx)
						Text(Out, L, S, TEXT("? RANDOM"), Rf, R.X + R.Z * 0.5f + Dx * 1.5f, R.Y + 22.5f + Dy * 1.5f, FLinearColor(0, 0, 0, Dx == 0 && Dy == 0 ? 0.45f : 0.16f), 1);
				Text(Out, L, S, TEXT("? RANDOM"), Rf, R.X + R.Z * 0.5f, R.Y + 21.5f, FLinearColor::White, 1);
			}
			else if (It.bDisabled)
			{
				const FVector2f Cc(R.X + R.Z * 0.5f + 1.0f, R.Y + R.W * 0.5f);
				const float H = (R.W + 6.0f) * 0.5f * 0.70711f;
				Lines(Out, L, S, { Cc + FVector2f(H, -H), Cc + FVector2f(-H, H) }, FLinearColor(1, 1, 1, 0.3f), 2.0f);
			}
			break;
		case FldTeam:
		{
			const FLinearColor C = It.bCur ? Ac() : (I == PopHover ? FLinearColor::White : Css(238, 241, 244, It.Value == 0 ? 0.8f : 0.85f));
			if (It.Value == 0) { Text(Out, L, S, TEXT("NO TEAM"), Font(EFace::Barlow700, 14, 3), R.X + R.Z * 0.5f, R.Y + 23.5f, C, 1); break; }
			Text(Out, L, S, FString::FromInt(It.Value), Font(EFace::Shoulders800, 22), R.X + R.Z * 0.5f, R.Y + 27.5f, C, 1);
			// os quadradinhos de quem já está nesse time
			TArray<uint32> Cs;
			for (int32 K = 0; K < Rows(); ++K) if (K != PopRow && Live(Slots[K]) && Slots[K].Team == It.Value) Cs.Add(Slots[K].Color < 0 ? 0x9aa0a6 : COLORS[Slots[K].Color].Hex);
			if (Cs.Num())
			{
				FMesh M(S);
				const float W = Cs.Num() * 5.0f + (Cs.Num() - 1) * 2.0f;
				for (int32 K = 0; K < Cs.Num(); ++K) M.Rect(R.X + R.Z * 0.5f - W * 0.5f + K * 7.0f, R.Y + 34.5f, 5, 5, HexV(Cs[K]));
				M.Draw(Out, L, White());
			}
			break;
		}
		}
	}
	++L;
}

// ---------- o navegador de mapas (o #browser do mockup): a busca, os filtros, a ordem e a grade que só desenha o que aparece ----------
namespace
{
	// os filtros medidos no Chrome (Tools/measure_skirmish_browser.js → skirmish_browser.json): o grupo, o índice no grupo (−1 = o
	// rótulo do grupo), o texto, o retângulo e a linha de base. Os grupos: 0 a ordem, 1 os favoritos e o naval, 2 os jogadores,
	// 3 o tamanho, 4 o terreno, 5 a condição, 6 a origem.
	struct FChip { int32 Group, Index; const TCHAR* Text; float X, Y, W, H, Base; };
	const FChip CHIPS[] = {
		{ 0, -1, TEXT("SORT"), 110.0f, 172.0f, 0.0f, 0.0f, 184.0f },
		{ 0, 0, TEXT("PLAYERS"), 110.0f, 194.0f, 83.67f, 30.0f, 214.0f },
		{ 0, 1, TEXT("POPULAR"), 199.67f, 194.0f, 85.39f, 30.0f, 214.0f },
		{ 0, 2, TEXT("NEWEST"), 110.0f, 230.0f, 78.61f, 30.0f, 250.0f },
		{ 0, 3, TEXT("NAME"), 194.61f, 230.0f, 60.03f, 30.0f, 250.0f },
		{ 1, 0, TEXT("FAVORITES"), 110.0f, 280.0f, 114.42f, 31.0f, 301.0f },
		{ 1, 1, TEXT("NAVAL"), 230.42f, 280.0f, 91.08f, 31.0f, 301.0f },
		{ 2, -1, TEXT("PLAYERS"), 110.0f, 331.0f, 0.0f, 0.0f, 343.0f },
		{ 2, 0, TEXT("2"), 110.0f, 353.0f, 30.5f, 30.0f, 373.0f },
		{ 2, 1, TEXT("3"), 146.5f, 353.0f, 30.34f, 30.0f, 373.0f },
		{ 2, 2, TEXT("4"), 182.84f, 353.0f, 31.08f, 30.0f, 373.0f },
		{ 2, 3, TEXT("5"), 219.92f, 353.0f, 30.36f, 30.0f, 373.0f },
		{ 2, 4, TEXT("6"), 256.28f, 353.0f, 30.36f, 30.0f, 373.0f },
		{ 2, 5, TEXT("8"), 292.64f, 353.0f, 30.38f, 30.0f, 373.0f },
		{ 2, 6, TEXT("12"), 110.0f, 389.0f, 36.67f, 30.0f, 409.0f },
		{ 2, 7, TEXT("16"), 152.67f, 389.0f, 36.5f, 30.0f, 409.0f },
		{ 3, -1, TEXT("SIZE"), 110.0f, 439.0f, 0.0f, 0.0f, 451.0f },
		{ 3, 0, TEXT("S"), 110.0f, 461.0f, 30.77f, 30.0f, 481.0f },
		{ 3, 1, TEXT("M"), 146.77f, 461.0f, 32.2f, 30.0f, 481.0f },
		{ 3, 2, TEXT("L"), 184.97f, 461.0f, 30.5f, 30.0f, 481.0f },
		{ 3, 3, TEXT("XL"), 221.47f, 461.0f, 39.69f, 30.0f, 481.0f },
		{ 4, -1, TEXT("TERRAIN"), 110.0f, 511.0f, 0.0f, 0.0f, 523.0f },
		{ 4, 0, TEXT("DESERT"), 110.0f, 533.0f, 75.02f, 30.0f, 553.0f },
		{ 4, 1, TEXT("TEMPERATE"), 191.02f, 533.0f, 102.45f, 30.0f, 553.0f },
		{ 4, 2, TEXT("FOREST"), 110.0f, 569.0f, 74.69f, 30.0f, 589.0f },
		{ 4, 3, TEXT("JUNGLE"), 190.69f, 569.0f, 75.72f, 30.0f, 589.0f },
		{ 4, 4, TEXT("SNOW"), 272.41f, 569.0f, 61.61f, 30.0f, 589.0f },
		{ 4, 5, TEXT("STEPPE"), 110.0f, 605.0f, 74.61f, 30.0f, 625.0f },
		{ 4, 6, TEXT("MOUNTAIN"), 190.61f, 605.0f, 92.58f, 30.0f, 625.0f },
		{ 4, 7, TEXT("CITY"), 289.19f, 605.0f, 54.11f, 30.0f, 625.0f },
		{ 4, 8, TEXT("ISLANDS"), 110.0f, 641.0f, 81.67f, 30.0f, 661.0f },
		{ 4, 9, TEXT("COAST"), 197.67f, 641.0f, 66.48f, 30.0f, 661.0f },
		{ 5, -1, TEXT("CONDITIONS"), 110.0f, 691.0f, 0.0f, 0.0f, 703.0f },
		{ 5, 0, TEXT("MORNING"), 110.0f, 713.0f, 83.94f, 30.0f, 733.0f },
		{ 5, 1, TEXT("DAY"), 199.94f, 713.0f, 48.41f, 30.0f, 733.0f },
		{ 5, 2, TEXT("LOW SUN"), 254.34f, 713.0f, 83.8f, 30.0f, 733.0f },
		{ 5, 3, TEXT("DUSK"), 110.0f, 749.0f, 58.33f, 30.0f, 769.0f },
		{ 5, 4, TEXT("NIGHT"), 174.33f, 749.0f, 63.94f, 30.0f, 769.0f },
		{ 5, 5, TEXT("RAIN"), 244.27f, 749.0f, 55.56f, 30.0f, 769.0f },
		{ 5, 6, TEXT("MONSOON"), 110.0f, 785.0f, 87.64f, 30.0f, 805.0f },
		{ 5, 7, TEXT("SNOWFALL"), 203.64f, 785.0f, 95.66f, 30.0f, 805.0f },
		{ 5, 8, TEXT("BLIZZARD"), 110.0f, 821.0f, 89.06f, 30.0f, 841.0f },
		{ 5, 9, TEXT("STORM"), 205.06f, 821.0f, 67.59f, 30.0f, 841.0f },
		{ 6, -1, TEXT("SOURCE"), 110.0f, 871.0f, 0.0f, 0.0f, 883.0f },
		{ 6, 0, TEXT("OFFICIAL"), 110.0f, 893.0f, 85.2f, 30.0f, 913.0f },
		{ 6, 1, TEXT("WORLDFORGE"), 201.2f, 893.0f, 113.81f, 30.0f, 913.0f },
	};
	constexpr int32 NCHIPS = UE_ARRAY_COUNT(CHIPS);
	// a busca e a grade: 6 colunas de 236 px, linhas de 284 px, o cartão de 224 × 272 (com a borda)
	const FVector4f SEARCH(368.88f, 95.5f, 550, 48), GRID(390, 172, 1460, 858);
	constexpr int32 COLS = 6;
	constexpr float CW = 236.0f, RH = 284.0f, CARDW = 224.0f, CARDH = 272.0f;
	constexpr float STAR_K = 20.0f / 24.0f;   // a estrela do cartão: o path de 24 de lado desenhado em 20

	FString FavPath() { return FPaths::ProjectSavedDir() / TEXT("RTSShell") / TEXT("skirmish_favorites.json"); }
	FString Thousands(int32 V)
	{
		const FString D = FString::FromInt(V);
		FString O;
		for (int32 I = 0; I < D.Len(); ++I) { if (I && (D.Len() - I) % 3 == 0) O += TEXT(","); O += D[I]; }
		return O;
	}
	FColor MixV(FColor A, FColor B, float T)
	{
		const auto C = [T](uint8 X, uint8 Y) { return uint8(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(float(X), float(Y), T)), 0, 255)); };
		return FColor(C(A.R, B.R), C(A.G, B.G), C(A.B, B.B), C(A.A, B.A));
	}
	FLinearColor Faded(FLinearColor C, float A) { C.A *= A; return C; }
	FColor FadedV(FColor C, float A) { C.A = uint8(FMath::Clamp(FMath::RoundToInt(C.A * A), 0, 255)); return C; }
	/** O texto que não cabe: corta no fim (com reticências no nome, como o text-overflow: ellipsis; a ficha só esconde). */
	FString Fit(const FString& S, const FSlateFontInfo& F, float Max, float Scale, bool bEllipsis)
	{
		if (TextWidth(S, F, Scale) <= Max) return S;
		const FString Tail = bEllipsis ? FString(TEXT("…")) : FString();
		FString R = S;
		while (R.Len() > 1 && TextWidth(R + Tail, F, Scale) > Max) R.LeftChopInline(1);
		return R.TrimEnd() + Tail;
	}
	/** A estrela do favorito: o path do mockup (M12 3l2.7 5.6 6.1.9…), 24 de lado, no lugar e na escala dados. */
	TArray<FVector2f> CardStar(FVector2f O, float K)
	{
		static const float P[20] = { 12, 3, 14.7f, 8.6f, 20.8f, 9.5f, 16.4f, 13.8f, 17.4f, 19.9f, 12, 17, 6.6f, 19.9f, 7.6f, 13.8f, 3.2f, 9.5f, 9.3f, 8.6f };
		TArray<FVector2f> R;
		for (int32 I = 0; I < 10; ++I) R.Add(O + FVector2f(P[2 * I], P[2 * I + 1]) * K);
		return R;
	}
	/** Uma estrela de 5 pontas regular (o ★ do filtro), a ponta pra cima. */
	TArray<FVector2f> RegularStar(FVector2f C, float R, float Ri)
	{
		TArray<FVector2f> P;
		for (int32 I = 0; I < 10; ++I)
		{
			const float A = -0.5f * PI + I * PI / 5.0f, Rr = (I & 1) ? Ri : R;
			P.Add(C + FVector2f(FMath::Cos(A), FMath::Sin(A)) * Rr);
		}
		return P;
	}
	/** A estrela cheia: um leque a partir do meio (vista do meio, a estrela não tem dobra). */
	void StarFill(FMesh& M, const TArray<FVector2f>& Pts, FVector2f C, FColor Col)
	{
		const int32 Ci = M.Vert(C, FVector2f(0, 0), Col);
		TArray<int32> V;
		for (const FVector2f& P : Pts) V.Add(M.Vert(P, FVector2f(0, 0), Col));
		for (int32 I = 0; I < V.Num(); ++I) M.Tri(Ci, V[I], V[(I + 1) % V.Num()]);
	}
	/** O ⚓ do NAVAL (o glifo azul do Windows que o Chrome desenha): o anel, a haste, a barra e os braços com as pontas. H = a altura. */
	void Anchor(FSlateWindowElementList& Out, int32 L, const FGeometry& S, FVector2f C, float H, const FLinearColor& Col)
	{
		const float K = H / 11.25f, W = 1.45f * K;
		const auto At = [&](float X, float Y) { return C + FVector2f(X, Y) * K; };
		Lines(Out, L, S, Arc(At(0, -4.0f), 1.55f * K, 0.0f, 2.0f * PI, 16), Col, 1.2f * K);
		Lines(Out, L, S, { At(0, -2.4f), At(0, 5.4f) }, Col, W);
		Lines(Out, L, S, { At(-3.1f, -0.9f), At(3.1f, -0.9f) }, Col, 1.3f * K);
		Lines(Out, L, S, Arc(At(0, 0.2f), 5.4f * K, FMath::DegreesToRadians(18.0f), FMath::DegreesToRadians(162.0f), 18), Col, W);
		for (const float Sg : { -1.0f, 1.0f })
		{
			const FVector2f Tip = At(Sg * 5.15f, 1.85f);
			Lines(Out, L, S, { At(Sg * 6.6f, 0.9f), Tip, At(Sg * 4.6f, 0.2f) }, Col, 1.2f * K);
		}
	}
}

void SRTSSkirmish::OpenBrowser()
{
	// como o openBrowser do mockup: fecha a janelinha, abre por cima da sala e refaz a lista (a busca digitada fica)
	PopField = -1;
	bBrowser = true;
	BwT0 = CaretT0 = Now;
	Hover = FHit();
	HoverRow = HoverBase = PopHover = -1;
	Refilter();
}

void SRTSSkirmish::CloseBrowser()
{
	bBrowser = false;
	Hover = FHit();
	HoverEase.Reset();
}

void SRTSSkirmish::Refilter()
{
	const FString Q = Query.TrimStartAndEnd();
	List.Reset();
	for (int32 I = 0; I < Maps.Num(); ++I)
	{
		const FMapEntry& M = Maps[I];
		const FString Field[5] = { FString::FromInt(M.Players), M.Size, M.Terr, M.Cond, M.Src };
		bool bOk = Q.IsEmpty() || M.Name.Contains(Q);
		for (int32 F = 0; F < 5 && bOk; ++F) if (Filt[F].Num() && !Filt[F].Contains(Field[F])) bOk = false;
		if (bFavOnly && !Favs.Contains(M.Real)) bOk = false;
		if (bNavalOnly && !M.bNaval) bOk = false;
		if (bOk) List.Add(I);
	}
	// a ordem do mockup: PLAYERS (menos jogadores primeiro, depois os mais jogados), POPULAR, NEWEST e NAME
	List.StableSort([this](int32 A, int32 B)
	{
		const FMapEntry& X = Maps[A];
		const FMapEntry& Y = Maps[B];
		switch (SortBy)
		{
		case 0: return X.Players != Y.Players ? X.Players < Y.Players : X.Plays > Y.Plays;
		case 1: return X.Plays > Y.Plays;
		case 2: return X.Date > Y.Date;
		default: return X.Name < Y.Name;
		}
	});
	Scroll = ScrollTo = 0.0f;
	HoverEase.Reset();
}

float SRTSSkirmish::BwMaxScroll() const
{
	const int32 Rws = (List.Num() + 1 + COLS - 1) / COLS;
	return FMath::Max(0.0f, Rws * RH - GRID.W);
}

bool SRTSSkirmish::ChipOn(int32 I) const
{
	const FChip& C = CHIPS[I];
	if (C.Group == 0) return SortBy == C.Index;
	if (C.Group == 1) return C.Index == 0 ? bFavOnly : bNavalOnly;
	return Filt[C.Group - 2].Contains(C.Text);
}

void SRTSSkirmish::ToggleChip(int32 I)
{
	const FChip& C = CHIPS[I];
	if (C.Group == 0) SortBy = C.Index;
	else if (C.Group == 1) { if (C.Index == 0) bFavOnly = !bFavOnly; else bNavalOnly = !bNavalOnly; }
	else
	{
		TSet<FString>& F = Filt[C.Group - 2];
		if (F.Contains(C.Text)) F.Remove(C.Text); else F.Add(C.Text);
	}
	Refilter();
}

void SRTSSkirmish::LoadFavs()
{
	Favs.Reset();
	FString Txt;
	if (!IsShotTest() && FFileHelper::LoadFileToString(Txt, *FavPath()))
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Txt), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : Arr) Favs.Add(V->AsString());
			return;
		}
	}
	if (Maps.Num()) Favs.Add(Maps[0].Real);   // de fábrica, o mapa oficial de cartaz já vem marcado (o FAV = {0} do mockup)
}

void SRTSSkirmish::SaveFavs() const
{
	if (IsShotTest()) return;
	FString Out = TEXT("[");
	bool bFirst = true;
	for (const FString& F : Favs) { Out += FString::Printf(TEXT("%s\"%s\""), bFirst ? TEXT("") : TEXT(","), *F); bFirst = false; }
	Out += TEXT("]");
	FFileHelper::SaveStringToFile(Out, *FavPath());
}

float SRTSSkirmish::HoverOf(EHit Kind, int32 A) const
{
	const float* V = HoverEase.Find((uint32(Kind) << 16) | uint32(A + 1));
	return V ? SkSmooth(*V) : 0.0f;
}

SRTSSkirmish::FHit SRTSSkirmish::BrowserHitAt(FVector2f P) const
{
	if (In(BACK, P)) return { EHit::BwBack, -1, -1 };
	if (In(SEARCH, P)) return { EHit::BwSearch, -1, -1 };
	for (int32 I = 0; I < NCHIPS; ++I)
	{
		const FChip& C = CHIPS[I];
		if (C.Index >= 0 && In(FVector4f(C.X, C.Y, C.W, C.H), P)) return { EHit::BwChip, I, -1 };
	}
	if (In(GRID, P))
	{
		const FVector2f Q(P.X - GRID.X, P.Y - GRID.Y + Scroll);
		const int32 C = int32(Q.X / CW), R = int32(Q.Y / RH), K = R * COLS + C;
		const FVector2f In0(Q.X - C * CW, Q.Y - R * RH);
		if (C < COLS && In0.X < CARDW && In0.Y < CARDH && K <= List.Num())
		{
			if (K == 0) return { EHit::BwNew, -1, -1 };
			if (In0.X >= 193.0f && In0.X <= 213.0f && In0.Y >= 217.0f && In0.Y <= 237.0f) return { EHit::BwFav, K - 1, -1 };
			return { EHit::BwCard, K - 1, -1 };
		}
	}
	return FHit();
}

FReply SRTSSkirmish::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
{
	// o chat da sala da rede (o campo com o foco): até 120 letras, como o maxlength da página
	if (bLan && bChatFocus && !bBrowser)
	{
		const TCHAR Ch = InCharacterEvent.GetCharacter();
		if (Ch < 32 || Ch == 127 || (InCharacterEvent.IsControlDown() && !InCharacterEvent.IsAltDown())) return FReply::Handled();
		if (ChatDraft.Len() < 120) ChatDraft.AppendChar(Ch);
		CaretT0 = Now;
		return FReply::Handled();
	}
	// a busca: as letras entram direto (o #q já ganha o foco quando o navegador abre)
	if (!bBrowser) return FReply::Unhandled();
	const TCHAR C = InCharacterEvent.GetCharacter();
	if (C < 32 || C == 127 || (InCharacterEvent.IsControlDown() && !InCharacterEvent.IsAltDown())) return FReply::Handled();
	if (Query.Len() < 40)
	{
		Query.AppendChar(C);
		CaretT0 = Now;
		Refilter();
	}
	return FReply::Handled();
}

FReply SRTSSkirmish::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bBrowser || !In(GRID, StagePos(MyGeometry, MouseEvent))) return FReply::Unhandled();
	ScrollTo = FMath::Clamp(ScrollTo - MouseEvent.GetWheelDelta() * 100.0f, 0.0f, BwMaxScroll());
	return FReply::Handled();
}

void SRTSSkirmish::PaintBrowser(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	// a entrada (popin .18s ease-out): de transparente e 4 px acima até o lugar
	const float E = Bezier(0.0f, 0.0f, 0.58f, 1.0f, FMath::Clamp(float((Now - BwT0) / 0.18), 0.0f, 1.0f));
	BwA = E;
	const FGeometry B = S.MakeChild(FVector2f(StageW, StageH), FSlateLayoutTransform(FVector2f(0.0f, -4.0f * (1.0f - E))));
	const float Scale = B.GetAccumulatedLayoutTransform().GetScale();
	const auto Fa = [this](const FLinearColor& C) { return Faded(C, BwA); };
	const auto FaV = [this](FColor C) { return FadedV(C, BwA); };
	const float Hb = HoverOf(EHit::BwBack);
	const FLinearColor ChipText = Css(238, 241, 244, 0.6f);
	{
		FMesh M(B);
		M.Rect(0, 0, StageW, StageH, FaV(CssV(6, 8, 7, 0.985f)));
		BoxRect(M, BACK, FaV(CssV(16, 21, 19, 0.85f)), FaV(MixV(CssV(230, 220, 190, 0.32f), AcV(0.8f), Hb)));
		BoxRect(M, SEARCH, FaV(CssV(16, 21, 19, 0.8f)), FaV(CssV(230, 220, 190, 0.2f)));
		for (int32 I = 0; I < NCHIPS; ++I)
		{
			const FChip& C = CHIPS[I];
			if (C.Index < 0) continue;
			const bool bOn = ChipOn(I);
			const float H = HoverOf(EHit::BwChip, I);
			BoxRect(M, FVector4f(C.X, C.Y, C.W, C.H), bOn ? FaV(CssV(216, 173, 78, 0.07f)) : FColor(0, 0, 0, 0),
				FaV(bOn ? AcV(0.75f) : MixV(CssV(230, 220, 190, 0.14f), CssV(230, 220, 190, 0.35f), H)));
			if (C.Group == 1 && C.Index == 0)
			{
				// o ★ do FAVORITES, na cor do texto do filtro
				const FLinearColor Sc = bOn ? Ac() : FMath::Lerp(ChipText, FLinearColor::White, H);
				StarFill(M, RegularStar(FVector2f(126.4f, 296.9f), 5.7f, 2.35f), FVector2f(126.4f, 296.9f), FaV(Sc.ToFColorSRGB()));
			}
		}
		// a rolagem da grade (8 px, só quando os cartões passam da altura)
		const int32 Rws = (List.Num() + 1 + COLS - 1) / COLS;
		const float Content = Rws * RH;
		if (Content > GRID.W)
		{
			M.Rect(GRID.X + GRID.Z - 8.0f, GRID.Y + Scroll / Content * GRID.W, 8.0f, GRID.W * GRID.W / Content, FaV(CssV(230, 220, 190, 0.18f)));
		}
		M.Draw(Out, L++, White());
	}
	// o BACK (o mesmo da sala; aqui fecha o navegador)
	const FLinearColor Bc = FMath::Lerp(Css(238, 241, 244, 0.9f), Ac(), Hb);
	Lines(Out, L, B, { FVector2f(134.75f, 45.58f), FVector2f(128.33f, 52.0f), FVector2f(134.75f, 58.42f) }, Fa(Bc), 2.2f);
	Text(Out, L, B, TEXT("BACK"), Font(EFace::Shoulders800, 19, 3), 155, 59.5f, Fa(Bc));
	{
		const FLinearColor Kc(Bc.R, Bc.G, Bc.B, Bc.A * 0.7f);
		Lines(Out, L, B, { FVector2f(213.23f, 42.5f), FVector2f(249.04f, 42.5f), FVector2f(249.04f, 61.5f), FVector2f(213.23f, 61.5f), FVector2f(213.23f, 42.5f) }, Fa(Kc), 1.0f);
		Text(Out, L, B, TEXT("ESC"), Font(EFace::Mono600, 11, 1), 219.73f, 56, Fa(Kc));
	}
	// o título, a busca (a lupa, o texto ou o "Search maps", o cursor piscando) e a contagem
	Text(Out, L, B, TEXT("SELECT MAP"), Font(EFace::Shoulders900, 46, 3), 110, 137, Fa(FLinearColor::White));
	{
		const FLinearColor Ic = Fa(Css(230, 220, 190, 0.6f));
		Lines(Out, L, B, Arc(FVector2f(391.755f, 118.375f), 4.875f, 0.0f, 2.0f * PI, 28), Ic, 1.5f);
		Lines(Out, L, B, { FVector2f(395.53f, 122.15f), FVector2f(399.63f, 126.25f) }, Ic, 1.5f);
	}
	const FSlateFontInfo Qf = Font(EFace::Barlow600, 18, 1);
	if (Query.IsEmpty()) Text(Out, L, B, TEXT("Search maps"), Qf, 413.88f, 126.5f, Fa(Css(230, 220, 190, 0.35f)));
	else Text(Out, L, B, Fit(Query, Qf, 488.0f, Scale, false), Qf, 413.88f, 126.5f, Fa(FLinearColor::White));
	if (HasKeyboardFocus() && !IsShotTest() && FMath::Fmod(float(Now - CaretT0), 1.0f) < 0.5f)
	{
		const float Cx = 413.88f + (Query.IsEmpty() ? 0.0f : TextWidth(Fit(Query, Qf, 488.0f, Scale, false), Qf, Scale));
		FMesh M(B);
		M.Rect(Cx, 109.0f, 1.0f, 21.0f, FaV(FColor::White));
		M.Draw(Out, L++, White());
	}
	const FString Count = List.Num() == Maps.Num() ? FString::Printf(TEXT("%s MAPS"), *Thousands(Maps.Num()))
		: FString::Printf(TEXT("%s OF %s MAPS"), *Thousands(List.Num()), *Thousands(Maps.Num()));
	Text(Out, L, B, Count, Font(EFace::Barlow600, 14, 3), 940.88f, 125, Fa(Css(152, 161, 154)));
	// os filtros: os rótulos dos grupos e o texto de cada um
	const FSlateFontInfo Lf = Font(EFace::Barlow600, 11.5f, 4), Cf = Font(EFace::Barlow700, 13, 2);
	for (int32 I = 0; I < NCHIPS; ++I)
	{
		const FChip& C = CHIPS[I];
		if (C.Index < 0) { Text(Out, L, B, C.Text, Lf, C.X, C.Base, Fa(Css(230, 220, 190, 0.42f))); continue; }
		const FLinearColor Tc = ChipOn(I) ? Ac() : FMath::Lerp(ChipText, FLinearColor::White, HoverOf(EHit::BwChip, I));
		if (C.Group == 1 && C.Index == 0) { Text(Out, L, B, C.Text, Cf, 138.42f, C.Base, Fa(Tc)); continue; }
		if (C.Group == 1) { Anchor(Out, L, B, FVector2f(250.6f, 298.1f), 11.25f, Fa(Css(78, 128, 152))); Text(Out, L, B, C.Text, Cf, 265.86f, C.Base, Fa(Tc)); continue; }
		Text(Out, L, B, C.Text, Cf, C.X + 11.0f, C.Base, Fa(Tc));
	}
	++L;
	// a grade: recortada na caixa dela (o overflow do .bw-grid), só as linhas que aparecem
	Out.PushClip(FSlateClippingZone(B.MakeChild(FVector2f(GRID.Z, GRID.W), FSlateLayoutTransform(FVector2f(GRID.X, GRID.Y)))));
	const int32 Total = List.Num() + 1, Rws = (Total + COLS - 1) / COLS;
	const int32 R0 = FMath::Max(0, int32(Scroll / RH)), R1 = FMath::Min(Rws - 1, int32((Scroll + GRID.W) / RH));
	for (int32 R = R0; R <= R1; ++R)
	{
		for (int32 C = 0; C < COLS; ++C)
		{
			const int32 K = R * COLS + C;
			if (K >= Total) break;
			PaintCard(Out, L, B, K, FVector2f(GRID.X + C * CW, GRID.Y + R * RH - Scroll));
		}
	}
	Out.PopClip();
}

void SRTSSkirmish::PaintCard(FSlateWindowElementList& Out, int32& L, const FGeometry& G, int32 K, FVector2f At) const
{
	const float Scale = G.GetAccumulatedLayoutTransform().GetScale();
	const auto Fa = [this](const FLinearColor& C) { return Faded(C, BwA); };
	const auto FaV = [this](FColor C) { return FadedV(C, BwA); };
	if (K == 0)
	{
		// o mundo novo (o WorldForge pela semente): a caixa tracejada, o "+", o nome e a semente. O jogo ainda não gera mundo pela
		// semente, então o cartão fica parado (sem o hover nem o clique) até o WorldForge entrar no jogo.
		FMesh M(G);
		M.Rect(At.X, At.Y, CARDW, CARDH, FaV(CssV(16, 21, 19, 0.62f)));
		const FColor D = FaV(CssV(216, 173, 78, 0.4f));   // o dashed de 1 px do Chrome: 3 de traço, 2 de vão, a partir do canto
		for (float X = 0.0f; X < CARDW; X += 5.0f)
		{
			const float W = FMath::Min(3.0f, CARDW - X);
			M.Rect(At.X + X, At.Y, W, 1, D);
			M.Rect(At.X + X, At.Y + CARDH - 1, W, 1, D);
		}
		for (float Y = 0.0f; Y < CARDH; Y += 5.0f)
		{
			const float H = FMath::Min(3.0f, CARDH - Y);
			M.Rect(At.X, At.Y + Y, 1, H, D);
			M.Rect(At.X + CARDW - 1, At.Y + Y, 1, H, D);
		}
		M.Draw(Out, L++, White());
		const float Cx = At.X + CARDW * 0.5f;
		Text(Out, L, G, TEXT("+"), Font(EFace::Shoulders800, 64), Cx, At.Y + 133.5f, Fa(Ac()), 1);
		Text(Out, L, G, TEXT("NEW WORLD"), Font(EFace::Shoulders800, 20, 3), Cx, At.Y + 167.5f, Fa(FLinearColor::White), 1);
		Text(Out, L, G, FString::Printf(TEXT("WORLDFORGE · #%d"), NewSeed), Font(EFace::Mono500, 13), Cx, At.Y + 190.5f, Fa(Css(230, 220, 190, 0.6f)), 1);
		++L;
		return;
	}
	const int32 Mi = List[K - 1];
	const FMapEntry& E = Maps[Mi];
	const float H = HoverOf(EHit::BwCard, K - 1);
	const FVector2f O(At.X, At.Y - 2.0f * H);   // o hover sobe 2 px (translateY(-2px) em .15s)
	{
		FMesh M(G);
		BoxRect(M, FVector4f(O.X, O.Y, CARDW, CARDH), FaV(CssV(16, 21, 19, 0.62f)), FaV(Mi == MapIdx ? AcV(0.85f) : MixV(CssV(230, 220, 190, 0.10f), CssV(230, 220, 190, 0.32f), H)));
		M.Draw(Out, L++, White());
	}
	// a miniatura (o losango do mapa com as bases, tirada do mockup em 2×)
	FSlateDrawElement::MakeBox(Out, L++, G.ToPaintGeometry(FVector2f(200, 200), FSlateLayoutTransform(O + FVector2f(12, 11))),
		SkImage(FString::Printf(TEXT("maps/%s_thumb.png"), *E.Real), FVector2f(400, 400)), ESlateDrawEffect::None, FLinearColor(1, 1, 1, BwA));
	// as etiquetas (a origem à esquerda, o naval à direita) e a estrela do favorito
	const FSlateFontInfo Tf = Font(EFace::Barlow700, 10, 2);
	const float SrcW = TextWidth(E.Src, Tf, Scale) + 12.0f;
	const float NavW = TextWidth(TEXT("NAVAL"), Tf, Scale) + 12.0f + 15.0f;
	const TArray<FVector2f> Star = CardStar(O + FVector2f(193, 217), STAR_K);
	{
		FMesh M(G);
		M.Rect(O.X + 11, O.Y + 11, SrcW, 18, FaV(CssV(8, 10, 9, 0.8f)));
		if (E.bNaval) M.Rect(O.X + 213 - NavW, O.Y + 11, NavW, 18, FaV(CssV(20, 60, 80, 0.9f)));
		if (Favs.Contains(E.Real)) StarFill(M, Star, O + FVector2f(193, 217) + FVector2f(12.0f, 12.6f) * STAR_K, FaV(AcV()));
		M.Draw(Out, L++, White());
	}
	TArray<FVector2f> Ring = Star;
	Ring.Add(Star[0]);
	Lines(Out, L, G, Ring, Fa(Css(216, 173, 78, 0.8f)), 1.4f * STAR_K);
	Text(Out, L, G, E.Src, Tf, O.X + 17, O.Y + 24, Fa(E.Src == TEXT("OFFICIAL") ? Ac() : Css(230, 220, 190, 0.7f)));
	if (E.bNaval)
	{
		const float Nx = O.X + 213 - NavW + 6;
		Anchor(Out, L, G, FVector2f(Nx + 5.0f, O.Y + 20.0f), 8.6f, Fa(Css(120, 180, 210)));
		Text(Out, L, G, TEXT("NAVAL"), Tf, Nx + 15.0f, O.Y + 24, Fa(Css(191, 230, 245)));
	}
	// o nome e a ficha (os jogadores, o tamanho, o terreno e a condição)
	const FSlateFontInfo Nf = Font(EFace::Shoulders800, 19, 2), Mf = Font(EFace::Barlow600, 11.5f, 2);
	Text(Out, L, G, Fit(E.Name, Nf, 174.0f, Scale, true), Nf, O.X + 13, O.Y + 232, Fa(FLinearColor::White));
	Text(Out, L, G, Fit(FString::Printf(TEXT("%dP · %s · %s · %s"), E.Players, *E.Size, *E.Terr, *E.Cond), Mf, 200.0f, Scale, false), Mf, O.X + 13, O.Y + 253,
		Fa(Css(152, 161, 154)));
	++L;
}

void SRTSSkirmish::PaintTip(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	if (PopField != FldColor || !PopItems.IsValidIndex(PopHover)) return;
	const FPopItem& It = PopItems[PopHover];
	const FLinearColor Mute = Css(152, 161, 154);
	FString Title, Sub[2];
	FLinearColor SubC[2] = { Mute, Mute };
	int32 N = 0;
	if (It.Value < 0)
	{
		Title = TEXT("RANDOM");
		Sub[N++] = TEXT("A FREE COLOR WHEN THE MATCH STARTS");
	}
	else
	{
		Title = COLORS[It.Value].Name;
		for (int32 K = 0; K < Rows(); ++K)
		{
			if (K == PopRow || !Live(Slots[K]) || Slots[K].Color != It.Value) continue;
			Sub[N++] = Slots[K].Kind == EKind::You ? FString(TEXT("USED BY YOU")) : FString::Printf(TEXT("USED BY AI · %s"), AIS[Slots[K].Ai]);
			break;
		}
		// a cor que some no chão (a distância no Lab abaixo de 24, como no mockup)
		if (Maps.IsValidIndex(MapIdx) && FVector3f::Distance(Lab(COLORS[It.Value].Hex), Lab(GroundOf(Maps[MapIdx].Terr))) < 24.0f)
		{
			SubC[N] = Css(240, 150, 110);
			Sub[N++] = FString::Printf(TEXT("HARD TO SEE ON %s"), *Maps[MapIdx].Terr);
		}
	}
	// as medidas do Chrome: 12 px até o texto, o título com a base a 25, cada linha de baixo 16 abaixo (base a 43); 8 px acima da cor
	const float Scale = S.GetAccumulatedLayoutTransform().GetScale();
	const FSlateFontInfo Bf = Font(EFace::Shoulders800, 17, 2), Sf = Font(EFace::Barlow600, 12, 2);
	float Wt = TextWidth(Title, Bf, Scale);
	for (int32 I = 0; I < N; ++I) Wt = FMath::Max(Wt, TextWidth(Sub[I], Sf, Scale));
	const float W = Wt + 24.0f, H = 37.0f + 16.0f * N;
	const FVector4f R(It.R.X + It.R.Z * 0.5f - W * 0.5f, It.R.Y - H - 8.0f, W, H);
	{
		FMesh M(S);
		GlowRect(M, FVector4f(R.X, R.Y + 8.0f, R.Z, R.W), 0x000000, 0.6f, 20.0f);   // a sombra (0 8px 20px, 0,6)
		BoxRect(M, R, CssV(11, 14, 13), CssV(230, 220, 190, 0.25f));
		M.Draw(Out, L++, White());
	}
	Text(Out, L, S, Title, Bf, R.X + 12.0f, R.Y + 25.0f, FLinearColor::White);
	for (int32 I = 0; I < N; ++I) Text(Out, L, S, Sub[I], Sf, R.X + 12.0f, R.Y + 43.0f + 16.0f * I, SubC[I]);
	++L;
}
