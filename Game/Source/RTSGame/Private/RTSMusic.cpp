// (06/11/2026) A trilha da partida: o que faz e por quê no cabeçalho (RTSMusic.h) e em docs/TRILHA_DA_PARTIDA.md.
#include "RTSMusic.h"
#include "RTSAudio.h"
#include "RTSMatchSubsystem.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundWave.h"

namespace
{
	// o teste à mão: 0 = o humor pelo combate, 1 = calma, 2 = batalha (a troca na hora, como seria pelo combate)
	TAutoConsoleVariable<int32> CVarMusicMood(TEXT("rts.Music.Mood"), 0, TEXT("A trilha da partida: 0 pelo combate, 1 calma, 2 batalha (teste)."));
	TAutoConsoleVariable<int32> CVarMusicLog(TEXT("rts.Music.Log"), 1, TEXT("A trilha da partida: 1 escreve no log cada troca de faixa e de humor; 2 também o estado a cada 5 s (o teste)."));

	// o calor do combate: a soma do dano visto (em frações da vida de cada unidade), que se apaga com a constante de 10 s
	constexpr float HeatTau = 10.0f;
	constexpr float HeatOn = 1.2f;        // a batalha: o calor acima disso por 3 s seguidos...
	constexpr float HeatOnHold = 3.0f;
	constexpr float HeatInstant = 2.4f;   // ...ou acima disso de uma vez (um prédio caindo, um ataque grande)
	constexpr float HeatOff = 0.3f;       // a calma: o calor abaixo disso por 30 s, com 45 s de batalha no mínimo
	constexpr float QuietHold = 30.0f;
	constexpr float BattleMin = 45.0f;
	// as trocas
	constexpr float BattleInFade = 0.35f, CalmOutFast = 1.5f;   // calma → batalha: a nova entra quase cheia no trecho forte, a velha sai em 1,5 s
	constexpr float ResolveFade = 8.0f;                         // batalha → calma: desce em 8 s a partir de uma barra
	constexpr float LetFinish = 50.0f;                          // ...ou deixa acabar, se faltar até isso
	constexpr float BuildFade = 2.5f;                           // batalha → batalha: a nova entra pela subida
	constexpr float ThemeFade = 3.0f;
	// o abaixar
	constexpr float DuckEva = 0.447f;     // −7 dB
	constexpr float DuckSw = 0.398f;      // −8 dB
	constexpr float DuckPause = 0.631f;   // −4 dB
	constexpr float DuckAttack = 0.15f, DuckRelease = 0.7f;

	// (06/11, ele: "lembre-se que a ordem das trilhas sonoras são aleatórias, senão é fácil de enjoar") as últimas que tocaram, de cada
	// facção e humor, gravadas no GameUserSettings [RTS] MusicRecent_<facção>_<humor> (a mais nova no fim): o saco novo deixa elas pro fim,
	// mesmo depois de fechar e abrir o jogo
	FString RecentKey(const FString& Fac, bool bBattle) { return FString::Printf(TEXT("MusicRecent_%s_%s"), *Fac, bBattle ? TEXT("battle") : TEXT("calm")); }
	TArray<FString> LoadRecent(const FString& Fac, bool bBattle)
	{
		FString V;
		TArray<FString> Out;
		if (GConfig && GConfig->GetString(TEXT("RTS"), *RecentKey(Fac, bBattle), V, GGameUserSettingsIni))
		{
			V.ParseIntoArray(Out, TEXT(","));
		}
		return Out;
	}
	void SaveRecent(const FString& Fac, bool bBattle, const TArray<FString>& List)
	{
		if (GConfig)
		{
			GConfig->SetString(TEXT("RTS"), *RecentKey(Fac, bBattle), *FString::Join(List, TEXT(",")), GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	float Db(float D) { return FMath::Pow(10.0f, D / 20.0f); }
}

URTSMusic* URTSMusic::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<URTSMusic>() : nullptr;
}

bool URTSMusic::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URTSMusic::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadTable();
}

void URTSMusic::Deinitialize()
{
	for (int32 I = 0; I < NumDecks; ++I)
	{
		if (Comp[I])
		{
			Comp[I]->OnAudioFinishedNative.RemoveAll(this);
			Comp[I]->OnAudioPlaybackPercentNative.RemoveAll(this);
			Comp[I]->Stop();
		}
		Comp[I] = nullptr;
	}
	Super::Deinitialize();
}

TStatId URTSMusic::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URTSMusic, STATGROUP_Tickables);
}

void URTSMusic::Log(const FString& What) const
{
	if (CVarMusicLog.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("RTS música: %s | %s"), *What, *DebugLine());
	}
}

void URTSMusic::LoadTable()
{
	bLoaded = true;
	const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("RTS"), TEXT("Music.json"));
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS música: sem o %s (a partida fica sem trilha)"), *Path);
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!Root->TryGetArrayField(TEXT("tracks"), List))
	{
		return;
	}
	for (const TSharedPtr<FJsonValue>& V : *List)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (!O.IsValid())
		{
			continue;
		}
		FTrack T;
		T.Id = O->GetStringField(TEXT("id"));
		T.Faction = O->GetStringField(TEXT("faction"));
		T.Role = O->GetStringField(TEXT("role"));
		T.Asset = O->GetStringField(TEXT("asset"));
		T.Dur = float(O->GetNumberField(TEXT("dur_s")));
		T.Bpm = FMath::Max(40.0f, float(O->GetNumberField(TEXT("bpm"))));
		T.Downbeat = float(O->GetNumberField(TEXT("downbeat_s")));
		T.Entry = float(O->GetNumberField(TEXT("entry_s")));
		T.Build = float(O->GetNumberField(TEXT("build_s")));
		T.GainDb = float(O->GetNumberField(TEXT("gain_db")));
		T.bGrid = O->GetBoolField(TEXT("grid"));
		Tracks.Add(T);
	}
	Waves.SetNum(Tracks.Num());
}

USoundWave* URTSMusic::WaveOf(int32 Track)
{
	if (!Tracks.IsValidIndex(Track))
	{
		return nullptr;
	}
	if (!Waves[Track])
	{
		Waves[Track] = LoadObject<USoundWave>(nullptr, *Tracks[Track].Asset, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Waves[Track])
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS música: o asset %s não existe (rodar Game/Tools/music/import_music.py)"), *Tracks[Track].Asset);
		}
		else
		{
			// (06/11, o 1º teste) a faixa que começa subindo do zero era descartada pela Unreal (o som calado no começo não toca: o tema
			// e a 1ª batalha "acabavam" ao começar); tocando calada ela segue e a subida funciona
			Waves[Track]->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
		}
	}
	return Waves[Track];
}

bool URTSMusic::IsMood(const FTrack& T, EMood M) const
{
	if (T.Role == TEXT("both"))
	{
		return true;
	}
	return M == EMood::Battle ? T.Role == TEXT("battle") : (T.Role == TEXT("calm") || T.Role == TEXT("theme"));
}

int32 URTSMusic::Pick(EMood M)
{
	TArray<int32>& B = Bag[int32(M)];
	const TArray<int32>& P = Pool[int32(M)];
	if (P.IsEmpty())
	{
		return INDEX_NONE;
	}
	const bool bBattle = M == EMood::Battle;
	const int32 Playing = LastStarted;
	TArray<FString> Recent = LoadRecent(Faction, bBattle);
	if (B.IsEmpty())
	{
		// o saco novo: embaralhado; as que tocaram por último (as gravadas) vão pro fundo, a mais nova no fundo de tudo (o saco tira do fim)
		B = P;
		for (int32 I = B.Num() - 1; I > 0; --I)
		{
			B.Swap(I, FMath::RandRange(0, I));
		}
		auto Age = [this, &Recent](int32 Ti) { const int32 K = Recent.IndexOfByKey(Tracks[Ti].Id); return K == INDEX_NONE ? -1 : K; };
		B.StableSort([&Age](int32 A, int32 Bi) { return Age(A) > Age(Bi); });
	}
	int32 T = B.Pop();
	if (T == Playing && !B.IsEmpty())
	{
		B.Insert(T, 0);
		T = B.Pop();
	}
	// a memória: tira e põe no fim; guarda até uma a menos que a roda (a roda inteira de novo antes de repetir)
	Recent.Remove(Tracks[T].Id);
	Recent.Add(Tracks[T].Id);
	while (Recent.Num() > FMath::Max(1, P.Num() - 1))
	{
		Recent.RemoveAt(0);
	}
	SaveRecent(Faction, bBattle, Recent);
	return T;
}

double URTSMusic::Clock() const
{
	UWorld* W = GetWorld();
	if (FAudioDevice* D = W ? W->GetAudioDeviceRaw() : nullptr)
	{
		return D->GetInterpolatedAudioClock();
	}
	return 0.0;   // sem o áudio (o -nullrhi sem som): a trilha espera (o Tick não começa sem o relógio)
}

float URTSMusic::PosOf(int32 D, double Now) const
{
	const FDeck& K = Decks[D];
	if (K.Track == INDEX_NONE)
	{
		return 0.0f;
	}
	// o começo pedido mais o que o relógio do áudio andou desde o Play
	return K.Offset + float(Now - K.StartedAt);
}

float URTSMusic::NextBeat(const FTrack& T, float Pos, bool bBar) const
{
	if (!T.bGrid)
	{
		return Pos;
	}
	const float Step = (bBar ? 240.0f : 60.0f) / T.Bpm;
	const float K = FMath::CeilToFloat((Pos - T.Downbeat) / Step);
	return T.Downbeat + FMath::Max(0.0f, K) * Step;
}

void URTSMusic::OnPercent(const UAudioComponent* C, const USoundWave* Wave, const float Percent)
{
	for (int32 D = 0; D < NumDecks; ++D)
	{
		FDeck& K = Decks[D];
		if (Comp[D] == C && K.Track != INDEX_NONE && !K.bHeard)
		{
			// o 1º bloco que o mixer tocou: a faixa começou agora (menos um bloco, ~20 ms); as subidas também começam daqui
			K.bHeard = true;
			const double Now = Clock();
			const double Late = FMath::Max(0.0, Now - 0.02 - K.StartedAt);
			K.StartedAt += Late;
			K.FadeT0 += Late;
			if (Late < 3.0)
			{
				StartLatency = 0.7f * StartLatency + 0.3f * float(Late + 0.02);
			}
			if (Late > 0.15)
			{
				Log(FString::Printf(TEXT("%s começou a soar %.2f s depois do Play: a contagem acerta"), *Tracks[K.Track].Id, Late));
			}
		}
	}
}

void URTSMusic::OnFinished(UAudioComponent* C)
{
	for (int32 D = 0; D < NumDecks; ++D)
	{
		if (Comp[D] == C)
		{
			Decks[D].bFinished = true;
		}
	}
}

void URTSMusic::Start(int32 Track, float Offset, float FadeIn, float FadeOut)
{
	USoundWave* Wave = WaveOf(Track);
	UWorld* W = GetWorld();
	if (!Wave || !W)
	{
		return;
	}
	const double Now = Clock();
	// a que tocava desce (potência igual); se já descia, segue a descida dela
	FDeck& Old = Decks[Cur];
	if (Old.Track != INDEX_NONE && !Old.bFinished && !Old.bOut)
	{
		Old.FadeFrom = Old.Fade;
		Old.FadeTo = 0.0f;
		Old.FadeT0 = Now;
		Old.FadeDur = FMath::Max(0.05f, FadeOut);
		Old.bOut = true;
	}
	int32 Free = INDEX_NONE;
	for (int32 D = 0; D < NumDecks; ++D)
	{
		if (D != Cur && Decks[D].Track == INDEX_NONE) { Free = D; break; }
	}
	if (Free == INDEX_NONE)
	{
		for (int32 D = 0; D < NumDecks; ++D)
		{
			if (D != Cur && (Free == INDEX_NONE || Decks[D].Fade < Decks[Free].Fade)) Free = D;
		}
	}
	Cur = Free;
	if (!Comp[Cur])
	{
		Comp[Cur] = UGameplayStatics::CreateSound2D(W, Wave, 1.0f, 1.0f, 0.0f, nullptr, false, false);
		if (!Comp[Cur])
		{
			return;
		}
		Comp[Cur]->bIsUISound = true;           // toca com o jogo pausado (o menu do ESC)
		Comp[Cur]->bAllowSpatialization = false;
		Comp[Cur]->OnAudioFinishedNative.AddUObject(this, &URTSMusic::OnFinished);
		Comp[Cur]->OnAudioPlaybackPercentNative.AddUObject(this, &URTSMusic::OnPercent);
	}
	FDeck& K = Decks[Cur];
	Comp[Cur]->Stop();
	K = FDeck();
	K.Track = Track;
	K.Offset = FMath::Clamp(Offset, 0.0f, FMath::Max(0.0f, Tracks[Track].Dur - 1.0f));
	K.StartedAt = Now;
	K.Fade = FadeIn > 0.0f ? 0.0f : 1.0f;
	K.FadeFrom = K.Fade;
	K.FadeTo = 1.0f;
	K.FadeT0 = Now;
	K.FadeDur = FadeIn;
	Comp[Cur]->SetSound(Wave);
	Comp[Cur]->SetVolumeMultiplier(FMath::Max(0.001f, K.Fade * Duck * Db(Tracks[Track].GainDb)));
	Comp[Cur]->Play(K.Offset);
	Phase = EPhase::Playing;
	LastStarted = Track;
	Log(FString::Printf(TEXT("toca %s (%s) de %.1f s, sobe em %.2f s; a de antes desce em %.2f s"), *Tracks[Track].Id, *Tracks[Track].Role, K.Offset, FadeIn, FadeOut));
}

void URTSMusic::FadeOutCurrent(float Seconds)
{
	FDeck& K = Decks[Cur];
	if (K.Track == INDEX_NONE || K.bFinished)
	{
		return;
	}
	// já descendo: só encurta (o fim da partida no meio da descida lenta da batalha)
	const double Now = Clock();
	if (K.bOut && K.FadeDur > 0.0f && K.FadeT0 + K.FadeDur <= Now + Seconds)
	{
		return;
	}
	K.FadeFrom = K.Fade;
	K.FadeTo = 0.0f;
	K.FadeT0 = Now;
	K.FadeDur = FMath::Max(0.05f, Seconds);
	K.bOut = true;
}

void URTSMusic::ScanCombat(URTSMatchSubsystem* Match, double Now)
{
	const float Dt = bScanned ? float(Now - LastTick) : 0.0f;
	LastTick = Now;
	Heat *= FMath::Exp(-Dt / HeatTau);
	const uint8 Me = Match->LocalPlayer();
	TArray<FRTSUnitState> Units;
	Match->GetUnits(Units);
	TSet<uint32> Seen;
	Seen.Reserve(Units.Num());
	float Add = 0.0f;
	for (const FRTSUnitState& U : Units)
	{
		Seen.Add(U.Id);
		const bool bMine = U.Owner == Me;
		const bool bShown = bMine || Match->IsPointShown(U.PosCm);
		if (const int32* Was = LastHp.Find(U.Id))
		{
			if (bScanned && U.Hp < *Was && bShown)
			{
				// o dano em frações da vida: o seu pesa mais; o prédio (vida enorme) pesa menos por fração
				const float Frac = FMath::Clamp(float(*Was - U.Hp) / float(FMath::Max(1, U.MaxHp)), 0.0f, 1.0f);
				const float Who = bMine ? 1.5f : (Match->IsEnemyOfLocal(U.Owner) ? 1.0f : 0.6f);
				Add += Frac * Who * (U.bStructure ? 2.0f : 1.0f);
			}
		}
		LastHp.Add(U.Id, U.Hp);
		LastOwner.Add(U.Id, U.Owner);
		LastShown.Add(U.Id, bShown);
	}
	// os que sumiram: a perda (a sua pesa mais; a do outro, se você via)
	for (auto It = LastHp.CreateIterator(); It; ++It)
	{
		if (!Seen.Contains(It.Key()))
		{
			const uint8 Owner = LastOwner.FindRef(It.Key());
			if (bScanned && It.Value() > 0)
			{
				Add += Owner == Me ? 0.5f : (LastShown.FindRef(It.Key()) ? 0.3f : 0.0f);
			}
			LastOwner.Remove(It.Key());
			LastShown.Remove(It.Key());
			It.RemoveCurrent();
		}
	}
	// (a superarma lançada: o calor e a música abaixando enquanto ela sai, pelo GetSuperweapons e o LaunchTick. Entra no commit seguinte,
	// quando a API das superarmas estiver no repositório: o trecho está na cópia de trabalho)
	Heat += Add;
	bScanned = true;
}

void URTSMusic::UpdateMood(double Now)
{
	EMood Want = Mood;
	const int32 Force = CVarMusicMood.GetValueOnGameThread();
	if (Force == 1 || Force == 2)
	{
		Want = Force == 2 ? EMood::Battle : EMood::Calm;
	}
	else if (Mood == EMood::Calm)
	{
		if (Heat >= HeatOn)
		{
			if (HotSince < 0.0) HotSince = Now;
		}
		else
		{
			HotSince = -1.0;
		}
		if (Heat >= HeatInstant || (HotSince >= 0.0 && Now - HotSince >= HeatOnHold))
		{
			Want = EMood::Battle;
		}
	}
	else
	{
		if (Heat < HeatOff)
		{
			if (QuietSince < 0.0) QuietSince = Now;
		}
		else
		{
			QuietSince = -1.0;
		}
		if (QuietSince >= 0.0 && Now - QuietSince >= QuietHold && Now - MoodSince >= BattleMin)
		{
			Want = EMood::Calm;
		}
	}
	if (Want == Mood)
	{
		return;
	}
	Mood = Want;
	MoodSince = Now;
	HotSince = QuietSince = -1.0;
	const FDeck& K = Decks[Cur];
	const bool bPlaying = Phase == EPhase::Playing && K.Track != INDEX_NONE && !K.bOut;
	const FTrack* T = bPlaying ? &Tracks[K.Track] : nullptr;
	if (Mood == EMood::Battle)
	{
		ResolveAt = -1.0;
		bNextFromBuild = false;
		if (T && T->Role == TEXT("both"))
		{
			Log(TEXT("batalha: a faixa de agora serve às duas, fica"));
		}
		else if (T)
		{
			// a troca no próximo tempo da faixa de agora: o Play sai antes pela demora medida até soar (a nova soa em cima do tempo da velha)
			const float Pos = PosOf(Cur, Now);
			float Wait = NextBeat(*T, Pos, false) - Pos - StartLatency;
			while (T->bGrid && Wait < 0.0f)
			{
				Wait += 60.0f / T->Bpm;
			}
			PendingSwitchAt = Now + FMath::Max(0.0f, Wait);
			Log(FString::Printf(TEXT("batalha (calor %.2f): troca daqui a %.2f s, no tempo"), Heat, float(PendingSwitchAt - Now)));
		}
		else
		{
			PendingSwitchAt = Now;   // no respiro: entra já
			Log(FString::Printf(TEXT("batalha (calor %.2f): entra já"), Heat));
		}
	}
	else
	{
		PendingSwitchAt = -1.0;
		if (T && T->Role == TEXT("battle"))
		{
			const float Pos = PosOf(Cur, Now);
			if (T->Dur - Pos <= LetFinish)
			{
				Log(FString::Printf(TEXT("calma: a batalha de agora acaba sozinha em %.0f s"), T->Dur - Pos));
			}
			else
			{
				ResolveAt = Now + FMath::Max(0.0f, NextBeat(*T, Pos, true) - Pos);
				Log(FString::Printf(TEXT("calma: desce em %.0f s a partir da barra daqui a %.2f s"), ResolveFade, float(ResolveAt - Now)));
			}
		}
		else
		{
			Log(TEXT("calma: a faixa de agora já serve"));
		}
	}
}

void URTSMusic::BeginNext(double Now)
{
	const int32 T = Pick(Mood);
	if (T == INDEX_NONE)
	{
		Phase = EPhase::Gap;
		GapUntil = Now + 5.0;
		return;
	}
	if (Mood == EMood::Battle && bNextFromBuild)
	{
		Start(T, Tracks[T].Build, BuildFade, 0.5f);   // a batalha continua: a nova entra pela subida
	}
	else
	{
		Start(T, 0.0f, 0.0f, 0.5f);                   // do começo (as faixas têm a introdução delas)
	}
	bNextFromBuild = false;
}

void URTSMusic::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* W = GetWorld();
	URTSMatchSubsystem* Match = W ? W->GetSubsystem<URTSMatchSubsystem>() : nullptr;
	if (!Match || Tracks.IsEmpty() || Match->SecondsSinceStart() <= 0.25f || FParse::Param(FCommandLine::Get(), TEXT("RTSNoMusic")))
	{
		return;
	}
	const double Now = Clock();
	if (Now <= 0.0)
	{
		return;
	}
	if (!bStarted)
	{
		bStarted = true;
		Faction = Match->FactionOf(Match->LocalPlayer());
		auto Build = [this](const FString& Fac)
		{
			for (int32 I = 0; I < Tracks.Num(); ++I)
			{
				const FTrack& T = Tracks[I];
				if (T.Faction != Fac) continue;
				if (T.Role == TEXT("theme")) Theme = I;
				else if (T.Role == TEXT("victory")) Victory = I;
				else if (T.Role == TEXT("defeat")) Defeat = I;
				for (int32 M = 0; M < 2; ++M)
				{
					if (IsMood(T, EMood(M))) Pool[M].Add(I);   // (o tema abre a partida e depois entra na roda da calma)
				}
			}
		};
		Build(Faction);
		if (Pool[0].IsEmpty() && Pool[1].IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS música: a facção %s não tem trilha; toca a da USA"), *Faction);
			Faction = TEXT("usa");
			Build(Faction);
		}
		// as faixas da facção na memória já (Force Inline, ~3 MB cada): a troca no meio da batalha não espera o disco
		for (int32 M = 0; M < 2; ++M)
		{
			for (const int32 I : Pool[M]) WaveOf(I);
		}
		WaveOf(Victory);
		WaveOf(Defeat);
		Mood = EMood::Calm;
		MoodSince = Now;
		LastTick = Now;
		// a abertura: sorteada da calma, o tema da facção entre elas (06/11, ele: a ordem aleatória, senão enjoa), subindo em 3 s
		const int32 First = Pick(EMood::Calm);
		if (First != INDEX_NONE)
		{
			Start(First, 0.0f, ThemeFade, 0.0f);
		}
		Log(FString::Printf(TEXT("partida: a trilha da %s (%d de calma, %d de batalha)"), *Faction, Pool[0].Num(), Pool[1].Num()));
	}
	// o fim da partida: a vitória ou a derrota da facção, depois o silêncio (o placar)
	const ERTSMatchResult Res = Match->Result();
	if (!bEnded && Res != ERTSMatchResult::Playing)
	{
		bEnded = true;
		FadeOutCurrent(2.0f);
		const int32 Sting = Res == ERTSMatchResult::Victory ? Victory : (Res == ERTSMatchResult::Defeat && !bDefeatPlayed ? Defeat : INDEX_NONE);
		Phase = EPhase::Ending;
		GapUntil = Now + 1.2;
		PendingSwitchAt = ResolveAt = -1.0;
		Log(FString::Printf(TEXT("fim da partida: %s"), Sting == INDEX_NONE ? TEXT("silêncio") : *Tracks[Sting].Id));
		if (Sting == INDEX_NONE)
		{
			Phase = EPhase::Done;
		}
		else
		{
			PendingSting = Sting;
		}
	}
	// você caiu e a partida segue (o espectador): a derrota uma vez, depois a música volta pelo combate
	if (!bEnded && !bDefeatPlayed && Match->IsLocalDefeated() && Defeat != INDEX_NONE)
	{
		bDefeatPlayed = true;
		FadeOutCurrent(2.0f);
		Phase = EPhase::Ending;
		GapUntil = Now + 1.2;
		PendingSting = Defeat;
		PendingSwitchAt = ResolveAt = -1.0;
		Log(TEXT("você caiu: a derrota"));
	}
	if (CVarMusicLog.GetValueOnGameThread() >= 2 && Now >= NextStateLog)
	{
		NextStateLog = Now + 5.0;
		Log(TEXT("estado"));
	}
	if (!bEnded && Now >= NextScan)
	{
		NextScan = Now + 0.25;
		ScanCombat(Match, Now);
		if (Phase != EPhase::Ending)
		{
			UpdateMood(Now);
		}
	}
	// as trocas marcadas
	if (PendingSwitchAt >= 0.0 && Now >= PendingSwitchAt)
	{
		PendingSwitchAt = -1.0;
		const int32 T = Pick(EMood::Battle);
		if (T != INDEX_NONE)
		{
			Start(T, Tracks[T].Entry, BattleInFade, CalmOutFast);
		}
	}
	if (ResolveAt >= 0.0 && Now >= ResolveAt)
	{
		ResolveAt = -1.0;
		FadeOutCurrent(ResolveFade);
	}
	// os tocadores: as descidas e subidas (potência igual: seno na subida, cosseno na descida) e o fim de cada faixa
	bool bCurDone = false;
	for (int32 D = 0; D < NumDecks; ++D)
	{
		FDeck& K = Decks[D];
		if (K.Track == INDEX_NONE || !Comp[D])
		{
			continue;
		}
		if (K.FadeDur > 0.0f)
		{
			const float A = FMath::Clamp(float(Now - K.FadeT0) / K.FadeDur, 0.0f, 1.0f);
			// subida: de From até 1 pela curva do seno; descida: de From até 0 pelo cosseno (somadas, a potência fica igual)
			K.Fade = K.FadeTo > K.FadeFrom ? FMath::Lerp(K.FadeFrom, K.FadeTo, FMath::Sin(A * UE_HALF_PI)) : K.FadeFrom * FMath::Cos(A * UE_HALF_PI);
			if (A >= 1.0f)
			{
				K.FadeDur = 0.0f;
				K.Fade = K.FadeTo;
			}
		}
		const bool bEnd = K.bFinished || (K.bOut && K.Fade <= 0.0005f) || PosOf(D, Now) >= Tracks[K.Track].Dur + 0.5f;
		if (bEnd)
		{
			if (!K.bFinished)
			{
				Comp[D]->Stop();
			}
			if (D == Cur)
			{
				bCurDone = true;
			}
			K = FDeck();
			continue;
		}
		Comp[D]->SetVolumeMultiplier(FMath::Max(0.0001f, K.Fade * Duck * Db(Tracks[K.Track].GainDb)));
	}
	// o fim da faixa de agora: o respiro e a próxima
	if (bCurDone && Phase == EPhase::Playing)
	{
		Phase = EPhase::Gap;
		GapUntil = Now + (Mood == EMood::Battle ? FMath::FRandRange(1.0f, 2.0f) : FMath::FRandRange(4.0f, 9.0f));
		bNextFromBuild = Mood == EMood::Battle;
		Log(FString::Printf(TEXT("a faixa acabou: respiro de %.1f s"), float(GapUntil - Now)));
	}
	if (Phase == EPhase::Gap && Now >= GapUntil)
	{
		BeginNext(Now);
	}
	if (Phase == EPhase::Ending && Now >= GapUntil && PendingSting != INDEX_NONE)
	{
		// o tema do fim, inteiro, do começo; acabado: o silêncio (fim da partida) ou a música de volta (o espectador)
		Start(PendingSting, 0.0f, 0.0f, 0.3f);
		PendingSting = INDEX_NONE;
		Phase = bEnded ? EPhase::Done : EPhase::Playing;
	}
	if (Phase == EPhase::Done && bCurDone)
	{
		Log(TEXT("o tema do fim acabou: silêncio"));
	}
	// o abaixar: a EVA falando, a superarma saindo, a pausa
	float Target = 1.0f;
	if (const URTSAudio* A = W->GetSubsystem<URTSAudio>())
	{
		if (A->IsEvaSpeaking()) Target *= DuckEva;
	}
	if (Now < SwDuckUntil) Target *= DuckSw;
	if (Match->IsPaused()) Target *= DuckPause;
	const float Dt = LastFrame > 0.0 ? FMath::Clamp(float(Now - LastFrame), 0.0f, 0.1f) : 0.0f;
	LastFrame = Now;
	const float Tau = Target < Duck ? DuckAttack : DuckRelease;
	Duck = Target + (Duck - Target) * FMath::Exp(-Dt / Tau);
}

FString URTSMusic::DebugLine() const
{
	const double Now = Clock();
	const FDeck& K = Decks[Cur];
	const TCHAR* Ph[] = { TEXT("espera"), TEXT("tocando"), TEXT("respiro"), TEXT("fim"), TEXT("acabou") };
	return FString::Printf(TEXT("%s, %s, %s %.1f/%.1f s, calor %.2f, abaixa %.2f"), Mood == EMood::Battle ? TEXT("batalha") : TEXT("calma"),
		Ph[int32(Phase)], K.Track != INDEX_NONE ? *Tracks[K.Track].Id : TEXT("-"), PosOf(Cur, Now), K.Track != INDEX_NONE ? Tracks[K.Track].Dur : 0.0f, Heat, Duck);
}
