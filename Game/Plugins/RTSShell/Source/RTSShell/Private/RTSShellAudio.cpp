#include "RTSShellAudio.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "ISubmixBufferListener.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeLock.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundWave.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	constexpr int32 FftN = 512;
	// os volumes do mockup aprovado: a trilha a 0,55 e o clique a 0,6 (são os padrões MUSIC 55 e INTERFACE 60 da tela de opções; a
	// tela de opções nativa vai pôr aqui o que o jogador escolher, com o MASTER)
	constexpr float MusicVolume = 0.55f, UiVolume = 0.6f;

	/** As amostras mais novas da música (o barramento SMX_Music), em mono como o analisador do navegador (L + R) / 2. Escreve a linha
	 *  do áudio, lê o menu. */
	class FMusicTap : public ISubmixBufferListener
	{
	public:
		virtual void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock) override
		{
			const int32 Ch = FMath::Max(1, NumChannels), Frames = NumSamples / Ch;
			FScopeLock Lock(&Guard);
			for (int32 F = 0; F < Frames; ++F)
			{
				float M = 0.0f;
				for (int32 C = 0; C < Ch; ++C)
				{
					M += AudioData[F * Ch + C];
				}
				Ring[Head] = M / Ch;
				Head = (Head + 1) % FftN;
			}
			LastAt = FPlatformTime::Seconds();
		}
		virtual const FString& GetListenerName() const override
		{
			static const FString Name = TEXT("RTSShellMusicTap");
			return Name;
		}
		/** A janela mais nova, da mais velha pra mais nova; falso se a música não manda nada há meio segundo. */
		bool Read(float Out[FftN])
		{
			FScopeLock Lock(&Guard);
			for (int32 I = 0; I < FftN; ++I)
			{
				Out[I] = Ring[(Head + I) % FftN];
			}
			return FPlatformTime::Seconds() - LastAt < 0.5;
		}

	private:
		FCriticalSection Guard;
		float Ring[FftN] = {};
		int32 Head = 0;
		double LastAt = 0.0;
	};

	TStrongObjectPtr<USoundWave> GTracks[3];
	TStrongObjectPtr<USoundWave> GClick;
	TStrongObjectPtr<USoundWave> GUi[int32(RTSShellAudio::EUi::Count)];
	double GLastHover = 0.0;
	TStrongObjectPtr<USoundMix> GDuck, GUser;
	TStrongObjectPtr<USoundClass> GMusicClass, GUiClass;
	TStrongObjectPtr<USoundSubmix> GBus;
	TStrongObjectPtr<UAudioComponent> GMusic;
	TSharedPtr<FMusicTap, ESPMode::ThreadSafe> GTap;
	Audio::FDeviceId GTapDevice = INDEX_NONE;
	int32 GTrack = 0;
	bool GLoaded = false, GDuckOn = false, GUserOn = false;
	bool GSwapping = false;
	bool GMuted = false;   // a troca de faixa pelo código para a anterior: o fim dela não conta como fim de trilha
	RTSShellAudio::EMusic GMode = RTSShellAudio::EMusic::Off;
	float GSmooth[FftN / 2] = {};

	UWorld* World()
	{
		return GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
	}

	template <typename T> T* Load(const TCHAR* Path)
	{
		T* O = LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!O)
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS telas: o som %s não existe (rodar Tools/import_menu_audio.py)"), Path);
		}
		return O;
	}

	bool Ensure()
	{
		if (!GLoaded)
		{
			GLoaded = true;
			for (int32 I = 0; I < 3; ++I)
			{
				GTracks[I].Reset(Load<USoundWave>(*FString::Printf(TEXT("/Game/Audio/Music/Main_Menu_%d.Main_Menu_%d"), I + 1, I + 1)));
			}
			GClick.Reset(Load<USoundWave>(TEXT("/Game/Audio/UI/UI_Menu_Click.UI_Menu_Click")));
			static const TCHAR* UiNames[int32(RTSShellAudio::EUi::Count)] = { TEXT("UI_Menu_Hover"), TEXT("UI_Menu_Enter"), TEXT("UI_Boot_Relay"), TEXT("UI_Boot_Riser"), TEXT("UI_Boot_Reveal") };
			for (int32 I = 0; I < int32(RTSShellAudio::EUi::Count); ++I)
			{
				GUi[I].Reset(Load<USoundWave>(*FString::Printf(TEXT("/Game/Audio/UI/%s.%s"), UiNames[I], UiNames[I])));
			}
			GDuck.Reset(Load<USoundMix>(TEXT("/Game/Audio/Mix/SM_ShellDuck.SM_ShellDuck")));
			GUser.Reset(Load<USoundMix>(TEXT("/Game/Audio/Mix/SM_UserVolumes.SM_UserVolumes")));
			GMusicClass.Reset(Load<USoundClass>(TEXT("/Game/Audio/Classes/SC_Music.SC_Music")));
			GUiClass.Reset(Load<USoundClass>(TEXT("/Game/Audio/Classes/SC_UI.SC_UI")));
			GBus.Reset(Load<USoundSubmix>(TEXT("/Game/Audio/Submix/SMX_Music.SMX_Music")));
		}
		UWorld* W = World();
		if (!W)
		{
			return false;
		}
		// os volumes do jogador: um mix sempre ligado com o volume de cada classe
		if (!GUserOn && GUser.IsValid() && GMusicClass.IsValid() && GUiClass.IsValid())
		{
			UGameplayStatics::SetSoundMixClassOverride(W, GUser.Get(), GMusicClass.Get(), MusicVolume, 1.0f, 0.0f, true);
			UGameplayStatics::SetSoundMixClassOverride(W, GUser.Get(), GUiClass.Get(), UiVolume, 1.0f, 0.0f, true);
			UGameplayStatics::PushSoundMixModifier(W, GUser.Get());
			GUserOn = true;
		}
		// o ouvido do radar no barramento da música
		if (!GTap.IsValid() && GBus.IsValid())
		{
			if (FAudioDevice* Device = W->GetAudioDeviceRaw())
			{
				GTap = MakeShared<FMusicTap, ESPMode::ThreadSafe>();
				Device->RegisterSubmixBufferListener(GTap.ToSharedRef(), *GBus.Get());
				GTapDevice = Device->DeviceID;
			}
		}
		// o teste (-RTSMuteOutput) não toca nos alto-falantes: a saída principal fica em zero e o radar segue ouvindo o barramento da
		// música, que vem antes dela; e o jogo sem foco (o teste fora da tela) não emudece as fontes
		if (!GMuted && FParse::Param(FCommandLine::Get(), TEXT("RTSMuteOutput")))
		{
			if (FAudioDevice* Device = W->GetAudioDeviceRaw())
			{
				Device->GetMainSubmixObject().SetSubmixOutputVolume(W, 0.0f);
				FApp::SetUnfocusedVolumeMultiplier(1.0f);
				GMuted = true;
				UE_LOG(LogTemp, Log, TEXT("RTS telas: som do teste sem saída nos alto-falantes"));
			}
		}
		return true;
	}

	void PlayTrack()
	{
		UWorld* W = World();
		USoundWave* Wave = GTracks[GTrack].Get();
		if (!W || !Wave)
		{
			return;
		}
		if (!GMusic.IsValid())
		{
			// o som 2D que atravessa a troca de mapa (o menu, o lobby e a volta da partida usam a mesma trilha)
			GMusic.Reset(UGameplayStatics::CreateSound2D(W, Wave, 1.0f, 1.0f, 0.0f, nullptr, true, false));
			if (!GMusic.IsValid())
			{
				return;
			}
			// uma trilha acaba, entra a próxima (V1, V2, V3 e volta), como o "ended" do mockup
			GMusic->OnAudioFinishedNative.AddLambda([](UAudioComponent*)
			{
				if (!GSwapping && GMode != RTSShellAudio::EMusic::Off)
				{
					GTrack = (GTrack + 1) % 3;
					PlayTrack();
				}
			});
		}
		GSwapping = true;
		GMusic->Stop();
		GSwapping = false;
		GMusic->SetSound(Wave);
		GMusic->Play();
		UE_LOG(LogTemp, Log, TEXT("RTS telas: música %s"), *Wave->GetName());
	}

	// a FFT do analisador (raiz 2, no lugar)
	void Fft(float Re[FftN], float Im[FftN])
	{
		for (int32 I = 1, J = 0; I < FftN; ++I)
		{
			int32 Bit = FftN >> 1;
			for (; J & Bit; Bit >>= 1) J ^= Bit;
			J ^= Bit;
			if (I < J) { Swap(Re[I], Re[J]); Swap(Im[I], Im[J]); }
		}
		for (int32 Len = 2; Len <= FftN; Len <<= 1)
		{
			const float A = -2.0f * PI / Len;
			const float Wr = FMath::Cos(A), Wi = FMath::Sin(A);
			for (int32 I = 0; I < FftN; I += Len)
			{
				float Cr = 1.0f, Ci = 0.0f;
				for (int32 K = 0; K < Len / 2; ++K)
				{
					const int32 P = I + K, Q = I + K + Len / 2;
					const float Tr = Re[Q] * Cr - Im[Q] * Ci, Ti = Re[Q] * Ci + Im[Q] * Cr;
					Re[Q] = Re[P] - Tr; Im[Q] = Im[P] - Ti;
					Re[P] += Tr; Im[P] += Ti;
					const float Nr = Cr * Wr - Ci * Wi;
					Ci = Cr * Wi + Ci * Wr;
					Cr = Nr;
				}
			}
		}
	}
}

namespace RTSShellAudio
{
	void SetMusic(EMusic Mode)
	{
		if (!Ensure())
		{
			return;
		}
		UWorld* W = World();
		const EMusic Was = GMode;
		GMode = Mode;
		if (Mode == EMusic::Off)
		{
			if (GMusic.IsValid() && GMusic->IsPlaying())
			{
				GMusic->FadeOut(1.0f, 0.0f);
			}
		}
		else if (Was == EMusic::Off || !GMusic.IsValid() || !GMusic->IsPlaying())
		{
			PlayTrack();
		}
		// o abaixar nas telas de conversa: o mix desce e volta em 1,4 s (o musicFade do mockup)
		const bool bDuck = Mode == EMusic::Duck;
		if (bDuck != GDuckOn && GDuck.IsValid() && W)
		{
			if (bDuck) UGameplayStatics::PushSoundMixModifier(W, GDuck.Get());
			else UGameplayStatics::PopSoundMixModifier(W, GDuck.Get());
			GDuckOn = bDuck;
		}
	}

	void Click()
	{
		if (Ensure() && GClick.IsValid())
		{
			UGameplayStatics::PlaySound2D(World(), GClick.Get(), 1.0f);
		}
	}

	void Play(EUi Sound)
	{
		if (!Ensure() || !GUi[int32(Sound)].IsValid())
		{
			return;
		}
		float Pitch = 1.0f;
		if (Sound == EUi::Hover)
		{
			// passar o mouse depressa pela lista não vira metralhadora: um toque a cada 45 ms, cada um num tom um pouco diferente
			const double At = FPlatformTime::Seconds();
			if (At - GLastHover < 0.045)
			{
				return;
			}
			GLastHover = At;
			Pitch = FMath::FRandRange(0.993f, 1.007f);   // o hover é o Lá da trilha (Ré maior): varia só o bastante pra não soar colado
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("RTS telas: som %s"), *GUi[int32(Sound)]->GetName());
		}
		UGameplayStatics::PlaySound2D(World(), GUi[int32(Sound)].Get(), 1.0f, Pitch);
	}

	bool Analyse(TArray<uint8>& OutBytes, float& OutLevel)
	{
		float X[FftN];
		if (!GTap.IsValid() || !GTap->Read(X))
		{
			OutLevel = 0.0f;
			FMemory::Memzero(GSmooth, sizeof(GSmooth));
			return false;
		}
		// o nível: o getByteTimeDomainData (128 + 128 x, em bytes) de 8 em 8
		OutLevel = 0.0f;
		for (int32 I = 0; I < FftN; I += 8)
		{
			const float B = FMath::Clamp(FMath::FloorToFloat(128.0f * (1.0f + X[I])), 0.0f, 255.0f);
			OutLevel = FMath::Max(OutLevel, FMath::Abs(B - 128.0f) / 128.0f);
		}
		// o espectro: a janela de Blackman, a FFT normalizada por N, a suavização de 0,8 entre chamadas e os decibéis de −100 a −30
		// em bytes (a especificação do AnalyserNode, os padrões do Chrome)
		float Re[FftN], Im[FftN];
		for (int32 I = 0; I < FftN; ++I)
		{
			const float A = 2.0f * PI * I / FftN;
			Re[I] = X[I] * (0.42f - 0.5f * FMath::Cos(A) + 0.08f * FMath::Cos(2.0f * A));
			Im[I] = 0.0f;
		}
		Fft(Re, Im);
		OutBytes.SetNumUninitialized(FftN / 2);
		for (int32 K = 0; K < FftN / 2; ++K)
		{
			const float Mag = FMath::Sqrt(Re[K] * Re[K] + Im[K] * Im[K]) / FftN;
			GSmooth[K] = 0.8f * GSmooth[K] + 0.2f * Mag;
			const float Db = GSmooth[K] > 0.0f ? 20.0f * FMath::LogX(10.0f, GSmooth[K]) : -1000.0f;
			OutBytes[K] = uint8(FMath::Clamp(FMath::FloorToInt(255.0f / 70.0f * (Db + 100.0f)), 0, 255));
		}
		return true;
	}

	void Shutdown()
	{
		if (GTap.IsValid() && GBus.IsValid() && GEngine)
		{
			if (FAudioDeviceManager* Manager = GEngine->GetAudioDeviceManager())
			{
				if (FAudioDevice* Device = Manager->GetAudioDeviceRaw(GTapDevice))
				{
					Device->UnregisterSubmixBufferListener(GTap.ToSharedRef(), *GBus.Get());
				}
			}
		}
		GTap.Reset();
		if (GMusic.IsValid())
		{
			GMusic->Stop();
		}
		GMusic.Reset();
		GMode = EMusic::Off;
		GDuckOn = GUserOn = false;
	}
}
