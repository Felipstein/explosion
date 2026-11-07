// (06/11/2026) A trilha da partida (docs/TRILHA_DA_PARTIDA.md). O pedido dele: "o que eu quero é os soundtrack [...] tem que ser muito bem
// feito, cada coisa; por exemplo, uma do nada para e outra começa, me deixa com pé atrás". O que esta classe faz, pelo estudo (o ZH pelo
// código, os RTS adaptativos e a régua da indústria):
//  - as faixas da facção do jogador (Game/Config/RTS/Music.json, do Game/Tools/music/music_pack.py: o volume já igualado em EBU R128, os
//    fins consertados, a grade das batidas, a entrada e a subida de cada faixa); a abertura é sorteada da calma (o tema entre elas);
//  - dois humores, calma e batalha, pelo "calor" do combate que o jogador vê (o dano nas unidades dele e nas que ele enxerga, as perdas),
//    com folga: a batalha entra com o calor alto por 3 s seguidos, e só volta pra calma depois de 30 s de silêncio e 45 s de batalha; as
//    faixas "both" servem aos dois e não trocam;
//  - a troca nunca é corte: calma → batalha no próximo tempo da faixa, a velha descendo em 1,5 s (a curva de potência igual) e a nova
//    entrando no trecho forte dela; batalha → calma deixa a faixa acabar (se faltar pouco) ou desce em 8 s a partir de uma barra de
//    compasso, um respiro e a calma do começo; entre duas faixas do mesmo humor, o fim natural e um respiro (4 a 9 s na calma, 1 a 2 na
//    batalha, a nova da batalha entrando pela subida);
//  - sem repetir: um saco embaralhado por humor; as que tocaram por último vão pro fim do sorteio, gravadas (valem depois de fechar o jogo);
//  - no fim, a vitória ou a derrota da facção (o ScoreScreenMusic do ZH);
//  - a música abaixa quando a EVA fala (−7 dB, 150 ms pra descer, 700 pra voltar), quando sai uma superarma (−8 dB por 6 s) e com o jogo
//    pausado (−4 dB); o volume é o MUSIC das opções (a classe SC_Music, das telas).
// Só som: a simulação não vê nada daqui (o sorteio é do relógio do PC, cada jogador ouve a sua).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSMusic.generated.h"

class UAudioComponent;
class USoundWave;
class URTSMatchSubsystem;

UCLASS()
class RTSGAME_API URTSMusic : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static URTSMusic* Get(const UObject* WorldContext);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** O estado em uma linha (o log e os testes): o humor, a faixa e a posição, o calor. */
	FString DebugLine() const;

private:
	static constexpr int32 NumDecks = 3;   // os tocadores (FDeck)
	struct FTrack
	{
		FString Id;
		FString Faction;
		FString Role;      // theme, calm, both, battle, victory, defeat
		FString Asset;
		float Dur = 0.0f;
		float Bpm = 120.0f;
		float Downbeat = 0.0f;
		float Entry = 0.0f;
		float Build = 0.0f;
		float GainDb = 0.0f;
		bool bGrid = false;
	};
	enum class EMood : uint8 { Calm, Battle };
	enum class EPhase : uint8 { Waiting, Playing, Gap, Ending, Done };
	/** Um dos três tocadores (a faixa de agora, a que está saindo no cruzamento e um livre pra uma troca logo depois da outra). */
	struct FDeck
	{
		int32 Track = INDEX_NONE;
		double StartedAt = 0.0;
		float Offset = 0.0f;
		float Fade = 1.0f;            // o ganho do cruzamento (0..1, potência igual)
		float FadeFrom = 1.0f, FadeTo = 1.0f;
		double FadeT0 = 0.0;
		float FadeDur = 0.0f;
		bool bOut = false;            // descendo pra sair (para no fim da descida)
		bool bFinished = false;
		bool bHeard = false;          // o áudio já avisou que ela toca (a posição conta dali)
	};

	void LoadTable();
	USoundWave* WaveOf(int32 Track);
	/** Toca a faixa no tocador livre a partir de Offset, subindo em FadeIn s (0 = já cheia); a que tocava desce em FadeOut s. */
	void Start(int32 Track, float Offset, float FadeIn, float FadeOut);
	void FadeOutCurrent(float Seconds);
	float PosOf(int32 Deck, double Now) const;
	/** A próxima batida (ou barra de compasso) da faixa depois de Pos, em segundos da faixa; sem grade, Pos. */
	float NextBeat(const FTrack& T, float Pos, bool bBar) const;
	int32 Pick(EMood Mood);
	bool IsMood(const FTrack& T, EMood Mood) const;
	void ScanCombat(URTSMatchSubsystem* Match, double Now);
	void UpdateMood(double Now);
	void BeginNext(double Now);
	/** (06/11, o 1º teste) O relógio do áudio (o do mixer, o mesmo das faixas tocando): com o processador apertado o mixer atrasa e o
	 *  relógio do Windows não; as subidas, as descidas, os respiros e a posição na faixa contam por ele, então a troca cai no tempo da
	 *  música que se ouve. (A posição que o OnAudioPlaybackPercent dá não serve: com o começo no meio da faixa ela veio errada, 16,8 s
	 *  com a faixa em 23,9, conferido na gravação pela forma de onda.) */
	double Clock() const;
	/** (06/11, o 2º teste) O 1º aviso do áudio de que a faixa está tocando: a contagem da posição começa daí. No começo da partida a
	 *  faixa soava ~6 s depois do Play (o jogo carregando), e a troca pro tempo forte caía no lugar errado (o tema estava em 26,5 s
	 *  quando a conta dizia 32,8). O valor do aviso não serve (com o começo no meio ele vem errado), só a hora dele. */
	void OnPercent(const UAudioComponent* C, const USoundWave* Wave, const float Percent);
	void OnFinished(UAudioComponent* Comp);
	void Log(const FString& What) const;

	TArray<FTrack> Tracks;
	TArray<int32> Pool[2];          // as faixas da facção de cada humor
	TArray<int32> Bag[2];           // o saco embaralhado (tira do fim)
	int32 Theme = INDEX_NONE, Victory = INDEX_NONE, Defeat = INDEX_NONE;
	FString Faction;
	bool bLoaded = false;
	bool bStarted = false;
	bool bEnded = false;
	bool bDefeatPlayed = false;
	int32 PendingSting = INDEX_NONE;   // o tema do fim (vitória ou derrota), tocado depois da descida

	FDeck Decks[NumDecks];
	int32 Cur = 0;                   // o tocador da faixa de agora
	int32 LastStarted = INDEX_NONE;  // a última que começou (o saco não a repete logo, mesmo no respiro, com o tocador já vazio)
	EPhase Phase = EPhase::Waiting;
	EMood Mood = EMood::Calm;
	double GapUntil = 0.0;
	double PendingSwitchAt = -1.0;   // a troca calma → batalha marcada pro próximo tempo (relógio real)
	double ResolveAt = -1.0;         // a descida batalha → calma marcada pra próxima barra
	double MoodSince = 0.0;
	bool bNextFromBuild = false;     // a próxima faixa de batalha entra pela subida (a batalha continua)
	float StartLatency = 0.1f;       // quanto uma faixa demora do Play até soar (a média das últimas): a troca dispara antes, e a entrada cai no tempo

	// o calor do combate (as contas da classe, no cabeçalho)
	float Heat = 0.0f;
	double HotSince = -1.0;
	double QuietSince = -1.0;
	double NextScan = 0.0;
	double LastTick = 0.0;
	TMap<uint32, int32> LastHp;
	TMap<uint32, uint8> LastOwner;
	TMap<uint32, bool> LastShown;
	TMap<uint32, int32> SwLaunch;
	bool bScanned = false;

	// o abaixar
	float Duck = 1.0f;
	double LastFrame = 0.0;
	double NextStateLog = 0.0;          // o relógio de verdade do abaixar (com o jogo pausado o quadro do mundo não anda)
	double SwDuckUntil = 0.0;

	UPROPERTY()
	TObjectPtr<UAudioComponent> Comp[NumDecks];
	UPROPERTY()
	TArray<TObjectPtr<USoundWave>> Waves;
};
