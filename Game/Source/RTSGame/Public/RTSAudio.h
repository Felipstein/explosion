// (04/11/2026) O som do jogo, a 1ª versão: os sons APROVADOS (docs/APROVADOS.md, Elenco) tocando na partida e no menu. O pedido dele:
// "os sons aprovados pode colocar no jogo". O mapa é o Game/Config/RTS/Sounds.json (gerado por Game/Tools/audio_pack.py, junto com os
// WAV em Game/Audio/); as decisões e o estudo em docs/elenco/SONS_UNIDADES.md:
//  - o clique em todo botão do HUD (igual nas 4 facções);
//  - selecionar = só a voz da unidade (o "capitão": o tipo que mais tem na seleção) ou o som do prédio; as ordens de andar e atacar
//    = a fala do capitão; a unidade nova sai da fábrica falando. Uma fala de unidade por vez (a nova corta a velha, como no ZH);
//    sorteio sem repetir a última;
//  - a EVA da facção do jogador nos avisos que o minimapa já dá (a base sob ataque, a unidade perdida), na falta de energia e na falta
//    de dinheiro, com as regras do ZH (Eva.ini 1.04): a prioridade (o maior fala primeiro, um aviso por vez), o intervalo mínimo
//    entre dois iguais e a validade na fila. Enquanto a EVA fala, as falas das unidades abaixam 6 dB (a mixagem, não efeito);
//  - um padrão só pro jogo (04/11, "mantém padrão no jogo"): toda voz entra limpa e no mesmo nível (o audio_pack.py), sem efeito.
// Os WAV são lidos do disco (Game/GameAudio, PCM 16 bits) e tocam num USoundWaveProcedural: nada de .uasset, então os sons mudam
// só rodando o audio_pack.py de novo. Cada som entra na classe de volume do menu (o /Game/Audio/Classes da sessão das Telas: o
// clique no SC_UI, as falas e a EVA no SC_Voice, o prédio no SC_Effects), então os volumes das opções valem pra eles. No pacote,
// a pasta Game/GameAudio tem de ir junto (docs/elenco/SONS_UNIDADES.md, "No jogo").
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSAudio.generated.h"

class UAudioComponent;
class USoundClass;
class ARTSPlayerController;
class URTSMatchSubsystem;

UCLASS()
class RTSGAME_API URTSAudio : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static URTSAudio* Get(const UObject* WorldContext);

	/** Um botão do HUD foi apertado (o PlayerController::OnButton): o clique; produzir ou construir sem dinheiro chama a EVA. */
	void OnButton(ARTSPlayerController* PC, const FString& Id);
	/** As ordens do jogador local (o URTSMatchSubsystem::Order*): a fala do capitão do grupo. */
	enum class EOrder : uint8 { Move, Attack, Build, Repair };
	void OnOrder(const TArray<uint32>& Group, EOrder Kind);
	/** O clique que seleciona uma unidade sua (o PlayerController): fala de novo mesmo com ela já selecionada (o ZH). */
	void OnSelectClick() { bSelectClick = true; }
	/** (06/11, a trilha da partida, RTSMusic.h) A EVA está falando agora: a música abaixa embaixo dela. */
	bool IsEvaSpeaking() const { return FPlatformTime::Seconds() < EvaBusyUntil; }

	// UTickableWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	/** Um WAV lido do disco: o PCM de 16 bits como está no arquivo. */
	struct FClip
	{
		int32 Rate = 44100;
		int32 Channels = 1;
		TArray<uint8> Pcm;
		float Seconds = 0.0f;
	};
	/** Uma lista de sorteio: os arquivos e o último que tocou (não repete). */
	struct FPool
	{
		TArray<FString> Files;
		int32 Last = -1;
	};
	enum class ECat : uint8 { Ui, Unit, Structure, Eva };
	/** Um som tocando: o componente, quando acaba e a categoria (a EVA abaixa as unidades). */
	struct FPlaying
	{
		TWeakObjectPtr<UAudioComponent> Comp;
		double EndAt = 0.0;
		ECat Cat = ECat::Ui;
		float Volume = 1.0f;
	};
	struct FEvaRule
	{
		int32 Priority = 1;
		float CooldownS = 0.0f;
		float ExpireS = 5.0f;
	};
	struct FEvaPending
	{
		FString Key;
		double At = 0.0;
	};

	void LoadTable();
	const FClip* Clip(const FString& Rel);
	/** Toca o próximo do sorteio; devolve quanto dura (0 = nada tocou). */
	float PlayPool(FPool& Pool, ECat Cat, float Volume);
	float PlayClip(const FClip& C, ECat Cat, float Volume);
	void StopCat(ECat Cat);
	void RaiseEva(const FString& Key);
	void TickEva(double Now);
	void TickWatch(ARTSPlayerController* PC, URTSMatchSubsystem* Match, double Now);
	/** O tipo que fala por um grupo: o que mais tem nele (empate: o primeiro). Vazio = ninguém. */
	FString CaptainOf(URTSMatchSubsystem* Match, const TArray<uint32>& Units, bool& bOutStructure) const;
	void Speak(const FString& UnitKey, const TCHAR* Kind);

	TMap<FString, TSharedPtr<FClip>> Clips;
	FPool Click;
	TMap<FString, TMap<FString, FPool>> Units;        // a chave da unidade -> select/move/attack/ready
	TMap<FString, FPool> StructureSelect;              // a chave do prédio -> o som de selecionar
	TMap<FString, TMap<FString, FPool>> Eva;          // a facção -> o aviso -> as falas
	TMap<FString, FEvaRule> EvaRules;
	TMap<FString, double> EvaLastAt;
	TArray<FEvaPending> EvaQueue;
	double EvaBusyUntil = 0.0;
	double UnitBusyUntil = 0.0;
	TArray<FPlaying> Playing;
	// o que se acompanha a cada quadro: a seleção, os avisos do minimapa, a energia e as unidades novas
	TArray<uint32> LastSelection;
	TSet<uint32> KnownUnits;
	TMap<uint32, FVector2D> MyStructurePos;          // os seus prédios na última varredura (o aviso da perda de prédio não é "unidade perdida")
	double WatchStartAt = -1.0;
	bool bLowPower = false;
	double LastVoiceAt = 0.0;
	double NextScan = 0.0;
	bool bSelectClick = false;
	bool bLoaded = false;
	// as classes de volume do menu (as opções mexem nelas); sem elas, o som toca no Master
	UPROPERTY()
	TObjectPtr<USoundClass> UiClass;
	UPROPERTY()
	TObjectPtr<USoundClass> VoiceClass;
	UPROPERTY()
	TObjectPtr<USoundClass> EffectsClass;
};
