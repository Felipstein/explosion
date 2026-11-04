// (04/11/2026) O som do jogo, a 1ª versão (o cabeçalho RTSAudio.h diz o quê e por quê; o estudo em docs/elenco/SONS_UNIDADES.md).
#include "RTSAudio.h"

#include "RTSMatchSubsystem.h"
#include "RTSPlayerController.h"
#include "RTSAlerts.h"
#include "Components/AudioComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWaveProcedural.h"

static TAutoConsoleVariable<int32> CVarRTSAudio(TEXT("rts.Audio"), 1, TEXT("O som do jogo (o clique, as falas e a EVA): 0 desliga"));
static TAutoConsoleVariable<float> CVarRTSAudioVolume(TEXT("rts.AudioVolume"), 1.0f, TEXT("O volume geral do som do jogo (0 a 2)"));

namespace
{
	// os volumes de cada categoria (a voz manda; a interface fica por baixo dela, o prédio entre as duas)
	constexpr float UiVolume = 0.7f;
	constexpr float StructureVolume = 0.85f;
	constexpr float VoiceVolume = 1.0f;
	constexpr float EvaVolume = 1.0f;
	// enquanto a EVA fala, as falas das unidades e os prédios abaixam 6 dB (a mixagem dos RTS: o aviso passa por cima da conversa)
	constexpr float EvaDuck = 0.5f;

	/** O WAV (RIFF, PCM de 16 bits, mono ou estéreo): a taxa, os canais e os bytes do "data". */
	bool ParseWav(const TArray<uint8>& B, int32& OutRate, int32& OutChannels, TArray<uint8>& OutPcm)
	{
		if (B.Num() < 44 || FMemory::Memcmp(B.GetData(), "RIFF", 4) != 0 || FMemory::Memcmp(B.GetData() + 8, "WAVE", 4) != 0)
		{
			return false;
		}
		auto U32 = [&B](int32 At) { return uint32(B[At]) | (uint32(B[At + 1]) << 8) | (uint32(B[At + 2]) << 16) | (uint32(B[At + 3]) << 24); };
		auto U16 = [&B](int32 At) { return uint32(B[At]) | (uint32(B[At + 1]) << 8); };
		int32 Pos = 12;
		uint32 Bits = 0;
		bool bPcm = false;
		while (Pos + 8 <= B.Num())
		{
			const uint32 Size = U32(Pos + 4);
			const int32 Body = Pos + 8;
			if (FMemory::Memcmp(B.GetData() + Pos, "fmt ", 4) == 0 && Size >= 16 && Body + 16 <= B.Num())
			{
				bPcm = U16(Body) == 1;
				OutChannels = int32(U16(Body + 2));
				OutRate = int32(U32(Body + 4));
				Bits = U16(Body + 14);
			}
			else if (FMemory::Memcmp(B.GetData() + Pos, "data", 4) == 0)
			{
				const int32 N = int32(FMath::Min<int64>(int64(Size), int64(B.Num() - Body)));
				OutPcm.Append(B.GetData() + Body, N & ~1);
				return bPcm && Bits == 16 && OutChannels >= 1 && OutChannels <= 2 && OutRate > 0;
			}
			Pos = Body + int32(Size) + int32(Size & 1u);
		}
		return false;
	}

	/** Uma lista de caminhos do JSON. */
	TArray<FString> Strings(const TSharedPtr<FJsonObject>& Obj, const FString& Field)
	{
		TArray<FString> Out;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Obj.IsValid() && Obj->TryGetArrayField(Field, Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				Out.Add(V->AsString());
			}
		}
		return Out;
	}
}

URTSAudio* URTSAudio::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<URTSAudio>() : nullptr;
}

bool URTSAudio::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URTSAudio::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UiClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/Classes/SC_UI.SC_UI"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	VoiceClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/Classes/SC_Voice.SC_Voice"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	EffectsClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/Classes/SC_Effects.SC_Effects"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	LoadTable();
}

void URTSAudio::Deinitialize()
{
	for (FPlaying& P : Playing)
	{
		if (P.Comp.IsValid())
		{
			P.Comp->Stop();
		}
	}
	Playing.Reset();
	Super::Deinitialize();
}

TStatId URTSAudio::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URTSAudio, STATGROUP_Tickables);
}

void URTSAudio::LoadTable()
{
	const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("RTS"), TEXT("Sounds.json"));
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS som: sem o %s (o jogo fica mudo)"), *Path);
		return;
	}
	// o que vem em "a chave -> o tipo -> a lista" (as unidades, a EVA de cada facção)
	auto ReadNested = [&Root](const TCHAR* Field, TMap<FString, TMap<FString, FPool>>& Out)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (Root->TryGetObjectField(Field, Obj))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Outer : (*Obj)->Values)
			{
				const TSharedPtr<FJsonObject> Inner = Outer.Value->AsObject();
				if (!Inner.IsValid())
				{
					continue;
				}
				TMap<FString, FPool>& Kinds = Out.FindOrAdd(Outer.Key);
				for (const TPair<FString, TSharedPtr<FJsonValue>>& K : Inner->Values)
				{
					Kinds.FindOrAdd(K.Key).Files = Strings(Inner, K.Key);
				}
			}
		}
	};
	const TSharedPtr<FJsonObject>* Ui = nullptr;
	if (Root->TryGetObjectField(TEXT("ui"), Ui))
	{
		Click.Files = Strings(*Ui, TEXT("click"));
	}
	TMap<FString, TMap<FString, FPool>> Structures;
	ReadNested(TEXT("structures"), Structures);
	for (auto& S : Structures)
	{
		if (FPool* Select = S.Value.Find(TEXT("select")))
		{
			StructureSelect.Add(S.Key, *Select);
		}
	}
	ReadNested(TEXT("units"), Units);
	ReadNested(TEXT("eva"), Eva);
	const TSharedPtr<FJsonObject>* Rules = nullptr;
	if (Root->TryGetObjectField(TEXT("eva_rules"), Rules))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& R : (*Rules)->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>& A = R.Value->AsArray();
			if (A.Num() >= 3)
			{
				FEvaRule Rule;
				Rule.Priority = int32(A[0]->AsNumber());
				Rule.CooldownS = float(A[1]->AsNumber());
				Rule.ExpireS = float(A[2]->AsNumber());
				EvaRules.Add(R.Key, Rule);
			}
		}
	}
	// lê todos agora (~10 MB, uma vez): o primeiro clique não engasga lendo o disco
	int32 Count = 0;
	auto Preload = [this, &Count](const FPool& Pool)
	{
		for (const FString& F : Pool.Files)
		{
			Count += Clip(F) != nullptr ? 1 : 0;
		}
	};
	Preload(Click);
	for (const auto& S : StructureSelect)
	{
		Preload(S.Value);
	}
	for (const auto& U : Units)
	{
		for (const auto& K : U.Value)
		{
			Preload(K.Value);
		}
	}
	for (const auto& F : Eva)
	{
		for (const auto& K : F.Value)
		{
			Preload(K.Value);
		}
	}
	bLoaded = true;
	UE_LOG(LogTemp, Log, TEXT("RTS som: %d sons (o clique, %d prédios, %d unidades, a EVA de %d facções)"), Count, StructureSelect.Num(), Units.Num(), Eva.Num());
}

const URTSAudio::FClip* URTSAudio::Clip(const FString& Rel)
{
	if (const TSharedPtr<FClip>* Found = Clips.Find(Rel))
	{
		return Found->IsValid() ? Found->Get() : nullptr;
	}
	TSharedPtr<FClip> C = MakeShared<FClip>();
	TArray<uint8> Bytes;
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameAudio"), Rel);
	if (!FFileHelper::LoadFileToArray(Bytes, *Path) || !ParseWav(Bytes, C->Rate, C->Channels, C->Pcm))
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS som: não leu %s"), *Path);
		C.Reset();
	}
	else
	{
		C->Seconds = float(C->Pcm.Num() / (2 * C->Channels)) / float(C->Rate);
	}
	Clips.Add(Rel, C);
	return C.Get();
}

float URTSAudio::PlayClip(const FClip& C, ECat Cat, float Volume)
{
	UWorld* World = GetWorld();
	if (!World || CVarRTSAudio.GetValueOnGameThread() == 0 || C.Pcm.Num() == 0)
	{
		return 0.0f;
	}
	// o WAV toca num procedural (sem .uasset): os bytes vão inteiros pra fila dele e o componente para quando o som acaba
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(C.Rate);
	Wave->NumChannels = C.Channels;
	Wave->SoundGroup = SOUNDGROUP_Default;
	Wave->bLooping = false;
	Wave->SoundClassObject = Cat == ECat::Ui ? UiClass.Get() : Cat == ECat::Structure ? EffectsClass.Get() : VoiceClass.Get();
	Wave->QueueAudio(C.Pcm.GetData(), C.Pcm.Num());
	const float V = Volume * FMath::Clamp(CVarRTSAudioVolume.GetValueOnGameThread(), 0.0f, 2.0f);
	UAudioComponent* Comp = UGameplayStatics::CreateSound2D(World, Wave, V, 1.0f, 0.0f, nullptr, false, true);
	if (!Comp)
	{
		return 0.0f;   // sem placa de som (o -nullrhi dos testes)
	}
	Comp->Play();
	FPlaying P;
	P.Comp = Comp;
	P.EndAt = FPlatformTime::Seconds() + C.Seconds + 0.05;
	P.Cat = Cat;
	P.Volume = V;
	Playing.Add(P);
	return C.Seconds;
}

float URTSAudio::PlayPool(FPool& Pool, ECat Cat, float Volume)
{
	if (Pool.Files.IsEmpty())
	{
		return 0.0f;
	}
	// o sorteio sem repetir a última (com 1 só, ela mesma)
	int32 Pick = FMath::RandRange(0, Pool.Files.Num() - 1);
	if (Pool.Files.Num() > 1 && Pick == Pool.Last)
	{
		Pick = (Pick + 1 + FMath::RandRange(0, Pool.Files.Num() - 2)) % Pool.Files.Num();
	}
	Pool.Last = Pick;
	UE_LOG(LogTemp, Log, TEXT("RTS som: %s"), *Pool.Files[Pick]);   // o teste confere pelo log (quem toca e quando)
	const FClip* C = Clip(Pool.Files[Pick]);
	return C ? PlayClip(*C, Cat, Volume) : 0.0f;
}

void URTSAudio::StopCat(ECat Cat)
{
	for (int32 I = Playing.Num() - 1; I >= 0; --I)
	{
		if (Playing[I].Cat == Cat)
		{
			if (Playing[I].Comp.IsValid())
			{
				Playing[I].Comp->Stop();
			}
			Playing.RemoveAtSwap(I);
		}
	}
}

FString URTSAudio::CaptainOf(URTSMatchSubsystem* Match, const TArray<uint32>& Group, bool& bOutStructure) const
{
	bOutStructure = false;
	TMap<uint16, int32> Count;
	uint16 Best = 0;
	int32 BestN = 0;
	for (const uint32 Id : Group)
	{
		FRTSUnitState S;
		if (!Match->GetUnit(Id, S) || S.Owner != Match->LocalPlayer())
		{
			continue;
		}
		int32& N = Count.FindOrAdd(S.Def);
		++N;
		if (N > BestN)
		{
			BestN = N;
			Best = S.Def;
		}
	}
	FRTSDefInfo D;
	if (BestN == 0 || !Match->GetDef(Best, D))
	{
		return FString();
	}
	bOutStructure = D.bStructure;
	return D.Key;
}

void URTSAudio::Speak(const FString& UnitKey, const TCHAR* Kind)
{
	TMap<FString, FPool>* Lines = Units.Find(UnitKey);
	FPool* Pool = Lines ? Lines->Find(Kind) : nullptr;
	const double Now = FPlatformTime::Seconds();
	if (!Pool || Now - LastVoiceAt < 0.15)
	{
		return;   // sem fala pra isso, ou dois pedidos no mesmo quadro
	}
	// uma fala de unidade por vez: a nova corta a velha (o ZH); o som do prédio também sai
	StopCat(ECat::Unit);
	StopCat(ECat::Structure);
	LastVoiceAt = Now;
	UnitBusyUntil = Now + PlayPool(*Pool, ECat::Unit, VoiceVolume);
}

void URTSAudio::OnButton(ARTSPlayerController* PC, const FString& Id)
{
	if (!bLoaded || Id == TEXT("hud_bg") || Id.StartsWith(TEXT("mm_")))
	{
		return;   // a peça do HUD e o minimapa seguram o clique, não são botões
	}
	PlayPool(Click, ECat::Ui, UiVolume);
	// produzir ou construir sem dinheiro: a EVA avisa (o "Insufficient funds" do ZH, a cada clique, com 1,5 s entre dois)
	FString Action;
	FString Arg;
	if (PC && Id.StartsWith(TEXT("eco_")) && Id.Mid(4).Split(TEXT("_"), &Action, &Arg) && (Action == TEXT("produce") || Action == TEXT("build")))
	{
		URTSMatchSubsystem* Match = GetWorld() ? GetWorld()->GetSubsystem<URTSMatchSubsystem>() : nullptr;
		FRTSDefInfo D;
		if (Match && Match->IsReady() && Match->GetDef(uint16(FCString::Atoi(*Arg)), D) && Match->Money(Match->LocalPlayer()) < D.Cost)
		{
			RaiseEva(TEXT("money"));
		}
	}
}

void URTSAudio::OnOrder(const TArray<uint32>& Group, EOrder Kind)
{
	URTSMatchSubsystem* Match = GetWorld() ? GetWorld()->GetSubsystem<URTSMatchSubsystem>() : nullptr;
	if (!bLoaded || !Match)
	{
		return;
	}
	bool bStructure = false;
	const FString Key = CaptainOf(Match, Group, bStructure);
	if (Key.IsEmpty() || bStructure)
	{
		return;
	}
	const TCHAR* Line = Kind == EOrder::Attack ? TEXT("attack") : Kind == EOrder::Build ? TEXT("build") : Kind == EOrder::Repair ? TEXT("repair") : TEXT("move");
	// quem não tem a fala do ataque (o Dozer) responde com a de andar
	TMap<FString, FPool>* Lines = Units.Find(Key);
	Speak(Key, Lines && Lines->Contains(Line) ? Line : TEXT("move"));
}

void URTSAudio::RaiseEva(const FString& Key)
{
	const double Now = FPlatformTime::Seconds();
	const FEvaRule* Found = EvaRules.Find(Key);
	const FEvaRule Rule = Found ? *Found : FEvaRule();
	const double* Last = EvaLastAt.Find(Key);
	if (Last && Now - *Last < Rule.CooldownS)
	{
		return;   // o mesmo aviso há pouco (o TimeBetweenChecksMS do ZH)
	}
	for (const FEvaPending& P : EvaQueue)
	{
		if (P.Key == Key)
		{
			return;
		}
	}
	EvaQueue.Add({ Key, Now });
}

void URTSAudio::TickEva(double Now)
{
	// o que esperou demais na fila sai (o ExpirationTimeMS do ZH: o aviso velho não vale mais)
	EvaQueue.RemoveAll([this, Now](const FEvaPending& P)
	{
		const FEvaRule* R = EvaRules.Find(P.Key);
		return Now - P.At > (R ? R->ExpireS : 5.0f);
	});
	if (Now < EvaBusyUntil || EvaQueue.IsEmpty())
	{
		return;
	}
	// um aviso por vez: o de maior prioridade (empate: o mais antigo)
	int32 Pick = 0;
	for (int32 I = 1; I < EvaQueue.Num(); ++I)
	{
		const FEvaRule* A = EvaRules.Find(EvaQueue[I].Key);
		const FEvaRule* B = EvaRules.Find(EvaQueue[Pick].Key);
		if ((A ? A->Priority : 1) > (B ? B->Priority : 1))
		{
			Pick = I;
		}
	}
	const FString Key = EvaQueue[Pick].Key;
	EvaQueue.RemoveAt(Pick);
	EvaLastAt.Add(Key, Now);
	URTSMatchSubsystem* Match = GetWorld() ? GetWorld()->GetSubsystem<URTSMatchSubsystem>() : nullptr;
	TMap<FString, FPool>* Faction = Match ? Eva.Find(Match->FactionOf(Match->LocalPlayer())) : nullptr;
	FPool* Pool = Faction ? Faction->Find(Key) : nullptr;
	const float Seconds = Pool ? PlayPool(*Pool, ECat::Eva, EvaVolume) : 0.0f;
	EvaBusyUntil = Now + Seconds + (Seconds > 0.0f ? 0.35 : 0.0);
}

void URTSAudio::TickWatch(ARTSPlayerController* PC, URTSMatchSubsystem* Match, double Now)
{
	if (!Match->IsReady())
	{
		WatchStartAt = -1.0;
		KnownUnits.Reset();
		MyStructurePos.Reset();
		LastSelection.Reset();
		bLowPower = false;
		return;
	}
	const bool bFirst = WatchStartAt < 0.0;
	WatchStartAt = bFirst ? Now : WatchStartAt;
	// a seleção: quando entra alguém novo nela (não quando alguém morre e sai), o capitão fala, ou o prédio soa
	const TArray<uint32>& Sel = PC->GetSelection();
	bool bAdded = false;
	for (const uint32 Id : Sel)
	{
		bAdded |= !LastSelection.Contains(Id);
	}
	if ((bAdded || bSelectClick) && !bFirst && Sel.Num())
	{
		bool bStructure = false;
		const FString Key = CaptainOf(Match, Sel, bStructure);
		if (bStructure)
		{
			if (FPool* Pool = StructureSelect.Find(Key))
			{
				StopCat(ECat::Unit);
				StopCat(ECat::Structure);
				PlayPool(*Pool, ECat::Structure, StructureVolume);
			}
		}
		else if (!Key.IsEmpty())
		{
			Speak(Key, TEXT("select"));
		}
	}
	LastSelection = Sel;
	bSelectClick = false;
	// os avisos que o minimapa acabou de pôr (RTSAlerts: a base sob ataque, a unidade perdida); a idade zero = deste quadro
	for (const FRTSMinimapPing& P : PC->GetMinimapPings())
	{
		if (P.Age > 0.0f)
		{
			continue;
		}
		if (P.Kind == RTSAlerts::BaseAttacked)
		{
			RaiseEva(TEXT("base_attack"));
		}
		else if (P.Kind == RTSAlerts::UnitLost)
		{
			// o alerta do minimapa vale pra prédio também; o ZH não fala "unit lost" pro prédio (o BuildingLost da USA é mudo: o "base sob
			// ataque" já avisou). O prédio que caiu é o que estava ali na última varredura
			bool bStructureLost = false;
			for (const TPair<uint32, FVector2D>& S : MyStructurePos)
			{
				bStructureLost |= FVector2D::DistSquared(S.Value, P.Cm) < 100.0 * 100.0;
			}
			if (!bStructureLost)
			{
				RaiseEva(TEXT("unit_lost"));
			}
		}
	}
	// a falta de energia: quando a produção começa a andar devagar (o "LOW POWER" do HUD)
	const bool bLow = Match->ProductionSpeed(Match->LocalPlayer()) < 0.999f;
	if (bLow && !bLowPower && !bFirst)
	{
		RaiseEva(TEXT("low_power"));
	}
	bLowPower = bLow;
	// a unidade nova saindo da fábrica fala (as do começo da partida não: os 3 s iniciais só anotam)
	if (Now >= NextScan || bFirst)
	{
		NextScan = Now + 0.5;
		TArray<FRTSUnitState> All;
		Match->GetUnits(All);
		FString NewKey;
		MyStructurePos.Reset();
		for (const FRTSUnitState& U : All)
		{
			if (U.Owner == Match->LocalPlayer() && U.bStructure)
			{
				MyStructurePos.Add(U.Id, U.PosCm);
			}
			if (U.Owner != Match->LocalPlayer() || U.bStructure || KnownUnits.Contains(U.Id))
			{
				continue;
			}
			KnownUnits.Add(U.Id);
			FRTSDefInfo D;
			if (NewKey.IsEmpty() && Now - WatchStartAt > 3.0 && Match->GetDef(U.Def, D))
			{
				NewKey = D.Key;
			}
		}
		if (!NewKey.IsEmpty() && Now >= UnitBusyUntil)
		{
			Speak(NewKey, TEXT("ready"));
		}
	}
}

void URTSAudio::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const double Now = FPlatformTime::Seconds();
	// o procedural não acaba sozinho: cada som para no fim dele
	for (int32 I = Playing.Num() - 1; I >= 0; --I)
	{
		if (!Playing[I].Comp.IsValid() || Now >= Playing[I].EndAt)
		{
			if (Playing[I].Comp.IsValid())
			{
				Playing[I].Comp->Stop();
			}
			Playing.RemoveAtSwap(I);
		}
	}
	// a mixagem: com a EVA falando, as unidades e os prédios abaixam
	const bool bDuck = Now < EvaBusyUntil;
	for (FPlaying& P : Playing)
	{
		if (P.Cat == ECat::Unit || P.Cat == ECat::Structure)
		{
			P.Comp->SetVolumeMultiplier(bDuck ? P.Volume * EvaDuck : P.Volume);
		}
	}
	UWorld* World = GetWorld();
	ARTSPlayerController* PC = World ? Cast<ARTSPlayerController>(World->GetFirstPlayerController()) : nullptr;
	URTSMatchSubsystem* Match = World ? World->GetSubsystem<URTSMatchSubsystem>() : nullptr;
	if (bLoaded && PC && Match)
	{
		TickWatch(PC, Match, Now);
	}
	TickEva(Now);
}
