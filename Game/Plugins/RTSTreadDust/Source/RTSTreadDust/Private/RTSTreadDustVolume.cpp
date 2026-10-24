#include "RTSTreadDust.h"
#include "RTSDustField.h"

#include "Components/PointLightComponent.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

// A poeira em VOLUME (16/10 noite, o redesenho; docs/POEIRA_DAS_LAGARTAS.md): a simulação é a mesma da v26 (os cartões
// nascem, andam, crescem e somem igual); em vez de desenhar cada um como um cartão 6-way, cada um vira um puff da MESMA
// gravação do EmberGen num campo de densidade (RTSDustField), que o raymarch das explosões marcha com a luz do BlastLab:
// a sombra do mapa, a própria sombra, o céu e as luzes do fogo e das explosões. Sem sombra pintada nem lado do sol.

// o padrão (17/10, os presets aprovados de manhã, "Média e Baixa ficam nos cartões"): −1 = pelo preset, o volume na Alta e
// no Ultra (sg.EffectsQuality 3 e 4) e os cartões na Média e na Baixa
static TAutoConsoleVariable<int32> CVarTreadDustPath(TEXT("rts.TreadDust.Path"), -1,
	TEXT("A poeira das lagartas: -1 = pelo preset (Alta e Ultra o volume, Média e Baixa os cartões), 0 = os cartões 6-way (a v26), 1 = o volume no raymarch das explosões (RTSDustField)"), ECVF_Default);

namespace
{
	double Smooth(double X, double Lo, double Hi)
	{
		if (X <= Lo) return 0.0;
		if (X >= Hi) return 1.0;
		const double K = (X - Lo) / (Hi - Lo);
		return K * K * (3.0 - 2.0 * K);
	}

	// o albedo de espalhamento simples do grão a partir da cor do chão (Hapke: a refletância difusa de um meio espesso
	// r0 = (1 − γ)/(1 + γ), γ = √(1 − w), invertida). O pó fino é mais pálido que o chão (a saturação relativa do perfil)
	FLinearColor DustAlbedo(const FLinearColor& Ground, double Sat, double WMax)
	{
		const double Lum = 0.2126 * Ground.R + 0.7152 * Ground.G + 0.0722 * Ground.B;
		auto W = [&](double C)
		{
			const double R = FMath::Clamp(Lum + Sat * (C - Lum), 0.0, 0.99);
			const double G = (1.0 - R) / (1.0 + R);
			return float(FMath::Min(1.0 - G * G, WMax));
		};
		return FLinearColor(W(Ground.R), W(Ground.G), W(Ground.B));
	}
}

// a densidade do volume por cima do perfil, pros ajustes no jogo sem recompilar (−1 = a do perfil, o "volume.density")
static TAutoConsoleVariable<float> CVarTreadDustVolDensity(TEXT("rts.TreadDust.VolDensity"), -1.0f,
	TEXT("A densidade da poeira em volume (× a da folha); -1 = a do perfil (Resources/tread.json, volume.density)"), ECVF_Default);

// (23/10, a areia do acerto) o ganho da massa de todos os puffs de impacto (× o A de cada um, ao vivo): a calibração no jogo sem recompilar
static TAutoConsoleVariable<float> CVarTreadDustImpactGain(TEXT("rts.TreadDust.ImpactGain"), 1.0f,
	TEXT("A areia dos acertos (os puffs de impacto: o véu, os pousos, a torre, a onda de base e a poeira que fica do ataque): × a massa de cada um, ao vivo (1 = a do jogo)"), ECVF_Default);

namespace RTSTreadDustVolume
{
	double ImpactGain()
	{
		return FMath::Max(double(CVarTreadDustImpactGain.GetValueOnGameThread()), 0.0);
	}
}

int32 ARTSTreadDust::BricksInUse() const
{
	return bVolumeOn ? RTSDustField::GetStats().X : 0;
}

bool ARTSTreadDust::IsVolumePath() const
{
	const int32 Path = CVarTreadDustPath.GetValueOnGameThread();
	bool bVol = Path == 1;
	if (Path < 0)
	{
		static const IConsoleVariable* Fx = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.EffectsQuality"));
		bVol = !Fx || Fx->GetInt() >= 3;
	}
	return bVol && RTSDustField::IsKickLoaded();
}

void ARTSTreadDust::SubmitVolume(const FVector& CamM, const FVector& CamFwd)
{
	const double VolDensity = CVarTreadDustVolDensity.GetValueOnGameThread() >= 0.0f
		? double(CVarTreadDustVolDensity.GetValueOnGameThread()) : P.VolDensity;
	// a erosão está em unidades de extinção: ela escala junto com a densidade (senão a poeira fraca é comida inteira e o rastro
	// encurta; o mapeamento de 22/10)
	const double VolErode = P.VolErode * VolDensity / FMath::Max(P.VolDensity, 1e-6);
	RTSDustField::EnsureDebugView();               // a vista de depuração (rts.DustField.Debug; desligada não custa nada)
	// o foco: onde a câmera olha no chão dos veículos (o centro da tela)
	double Zg = 0.0;
	for (const FRTSTreadSource& S : Sources)
	{
		Zg += S.PosM.Z;
	}
	Zg = Sources.Num() ? Zg / Sources.Num() : 0.0;
	const bool bDown = CamFwd.Z < -0.05;
	FVector Focus = bDown ? CamM + CamFwd * ((Zg - CamM.Z) / CamFwd.Z) : CamM + CamFwd * 60.0;
	// a janela da poeira (256 m, a borda some nos últimos 16 m) puxada 40 m pra frente da câmera: a câmera do RTS é inclinada
	// e vê muito mais chão acima do centro da tela (o fundo) do que abaixo; assim o fundo cabe mesmo no zoom de 140 m (22/10).
	// Com a câmera deitada (23/10, a revisão da v4: a 10° e a vista alinhada aos eixos o pé da tela ficava no sumiço da borda,
	// ~20 %) puxa menos: nunca tanto que o chão do pé da tela (a meia abertura vertical de 15°, a do FOV de 50° em 16:9 com
	// folga) saia da parte cheia da janela (128 − 16 do sumiço − 4 de folga pro encaixe na grade). Não centra a janela entre o
	// pé e o topo: medido, isso puxava só 20 a 25 m no padrão e o canto de cima da tela (a vista de 45°) perdia a poeira (0,53 no
	// zoom de 140 m, 0,05 no de 170 m). Sem olhar pra baixo, os 40 m
	double Shift = 40.0;
	if (bDown)
	{
		const double H = FMath::Max(CamM.Z - Zg, 0.0);
		const double Pitch = FMath::Asin(FMath::Clamp(-CamFwd.Z, -1.0, 1.0));
		// o chão do pé da tela, atrás do foco
		const double Back = H / FMath::Tan(Pitch) - H / FMath::Tan(Pitch + FMath::DegreesToRadians(15.0));
		Shift = FMath::Clamp(128.0 - 16.0 - 4.0 - Back, 0.0, 40.0);
	}
	const FVector FwdXY = FVector(CamFwd.X, CamFwd.Y, 0.0).GetSafeNormal();
	Focus += FwdXY * Shift;

	TArray<FRTSDustPuff> Puffs;
	Puffs.Reserve(Cards.Num());
	for (const FCard& C : Cards)
	{
		if (!Visible.FindRef(C.Src) && Visible.Contains(C.Src))
		{
			continue;                                  // a névoa de guerra
		}
		// a mesma conta do Draw (o tamanho, o quadro, o sumiço pela densidade, a erosão): o volume é o cartão aprovado
		const FRTSTreadLayer& L = P.Layers[C.Layer];
		const double K = C.Age / FMath::Max(C.Life, 1e-3);
		// o nascer fica à parte (Att): no volume ele é aplicado na opacidade, como no cartão (abaixo)
		const double Att = Smooth(K, 0.0, FMath::Max(L.Attack, 1e-3));
		// (23/10, a areia do acerto) o puff de impacto leva a massa de vários cartões (A até 16): no volume a extinção é linear, sem o corte
		// em 1 dos cartões das lagartas (que segue abaixo pra eles)
		const bool bImpact = C.Src == ImpactSrcId;
		double A = L.Alpha * C.A * (bImpact ? RTSTreadDustVolume::ImpactGain() : 1.0) * (1.0 - Smooth(K, L.Decay, 1.0));
		double Mf = L.MaskEnd > 0.0 ? Smooth((Odo.FindRef(C.Src) - C.Odo0) / C.Len, L.MaskDense, L.MaskEnd) : 0.0;
		if (L.FadeTau > 0.0)
		{
			Mf = FMath::Max(Mf, Smooth(K, 0.6, 1.0));
		}
		const double Gf = 1.0 + L.GrowFade * (1.0 - (1.0 - Mf) * (1.0 - Mf));
		double Kd = 1.0;
		if (L.FadeTau > 0.0)
		{
			Kd = FMath::Exp(-L.FadeTau * Mf) / (Gf * Gf);
			A *= 1.0 - Smooth(Mf, L.FadeAlphaFrom, 1.0);
		}
		else
		{
			A *= 1.0 - Mf;
		}
		if (!bImpact)
		{
			A = FMath::Min(1.0, A);
		}
		if (C.bDouble)
		{
			A = 1.0 - (1.0 - A) * (1.0 - A);
		}
		if (A * Att <= 1e-3 || Kd <= 1e-3)
		{
			continue;
		}
		const double Size = Atlas.SideM * L.Scale * C.S * (1.0 + (L.GrowTo - 1.0) * K) * C.Wake * Gf;
		FRTSDustPuff Pf;
		Pf.PosM = C.P + FVector(0.0, 0.0, L.RiseFade * Mf);
		Pf.Scale = float(Size / FMath::Max(Atlas.SideM, 1e-3));
		// o quadro pelo tempo da própria gravação (a de 30 qps tem 150 quadros; a folha dos cartões, 48)
		const double BakeT = C.Bake0 + C.Age * L.BakeRate;
		const float KickFrame = RTSDustField::KickFrameAt(BakeT);
		Pf.Frame = KickFrame >= 0.0f ? KickFrame : float(Atlas.FrameAt(BakeT));
		// a extinção: a profundidade óptica do cozimento das folhas (σ = 6ρ² por m, bake_sixway.py; o ρ² no shader) × a densidade do
		// perfil × o sumiço (Kd) × o alfa da camada (A), na escala do puff (o cartão tem o mesmo alfa em qualquer tamanho)
		// o nascer (17/10, a conferência do vídeo: o tufo aparecia inteiro de um quadro pro outro, 2,6× o salto dos cartões).
		// No cartão o fade de entrada escala a OPACIDADE; no volume, escalando σ, um puff de miolo τ ~8 com 20 % do fade já
		// saía ~80 % opaco (1 − e^(−1,6)). Aqui o fade vira o σ que dá a mesma opacidade no miolo: −ln(1 − Att·(1 − e^(−τ)))/τ,
		// com τ o miolo do puff adulto. Adulto (Att = 1) fica igual ao aprovado
		const double TauS = 6.0 * P.Density * Kd * A * VolDensity * RTSDustField::KickColumn(Pf.Frame);
		const double Ga = Att >= 1.0 ? 1.0 : (TauS > 1e-3 ? -FMath::Loge(FMath::Max(1.0 - Att * (1.0 - FMath::Exp(-TauS)), 1e-6)) / TauS : Att);
		Pf.Sigma = float(6.0 * P.Density * Kd * A * Ga * VolDensity / FMath::Max(double(Pf.Scale), 1e-3));
		// a erosão do cartão (em profundidade óptica, numa corda da nuvem) vira extinção: ÷ a corda que a câmera de cima
		// atravessa (a altura da nuvem da gravação, ~1,2 m na escala 1, × a escala); VolErode é o ajuste (1 = igual)
		const double ErodeTau = FMath::Max(L.Erode * (L.ErodeFromBirth > 0.5 ? 1.0 : Smooth(K, 0.35, 1.0)), L.ErodeFade * FMath::Pow(Mf, 2.5));
		Pf.Erode = float(VolErode * ErodeTau / (1.2 * FMath::Max(double(Pf.Scale), 1e-3)));
		// o giro: a câmera do cozimento olha pra +x com a direita em −y, então o lado cheio da gravação (o u+ do cartão) é o
		// −y dela. "orient": o −y vai pra trás do veículo; o espelho (em y) é o mesmo sorteio do cartão
		if (L.Orient > 0.5 && !C.Back.IsNearlyZero())
		{
			Pf.Yaw = float(FMath::Atan2(C.Back.X, -C.Back.Y));
		}
		else
		{
			Pf.Yaw = float(C.RotFree);
		}
		Pf.bMirror = C.Flip < 0.0;
		Pf.Albedo = DustAlbedo(C.Col, P.VolSat, P.VolAlbedoMax);
		Puffs.Add(Pf);
	}
	RTSDustField::SubmitFrame(Focus, SunDir, Clock, float(P.VolDetail), MoveTemp(Puffs));
	if (RTSDustField::IsDebugViewOn())
	{
		// só o FXLab (a vista de depuração): as luzes pontuais do mundo perto do foco, as 8 mais fortes (no jogo quem
		// manda as luzes é o raymarch, do RTSSceneLights). Cor × candela ÷ 128 000, a escala do raymarch
		struct FL { FRTSDustLight L; float W; };
		TArray<FL> Found;
		for (TObjectIterator<UPointLightComponent> It; It; ++It)
		{
			const UPointLightComponent* Lc = *It;
			if (Lc->GetWorld() != GetWorld() || !Lc->IsVisible() || Lc->Intensity <= 0.0f)
			{
				continue;
			}
			const FVector Pm = Lc->GetComponentLocation() / 100.0;
			if (FVector::Dist2D(Pm, Focus) > 120.0)
			{
				continue;
			}
			const float Cd = Lc->Intensity * ULocalLightComponent::GetUnitsConversionFactor(Lc->IntensityUnits, ELightUnits::Candelas);
			const FLinearColor C = Lc->GetLightColor() * (Cd / 128000.0f);
			FL F;
			F.L.PosCamRelM = FVector3f(Pm);
			F.L.RadiusM = Lc->AttenuationRadius / 100.0f;
			F.L.Color = FVector3f(C.R, C.G, C.B);
			F.W = Cd;
			Found.Add(F);
		}
		Found.Sort([](const FL& A, const FL& B) { return A.W > B.W; });
		TArray<FRTSDustLight> Dl;
		for (int32 I = 0; I < FMath::Min(Found.Num(), 8); ++I)
		{
			Dl.Add(Found[I].L);
		}
		RTSDustField::SetDebugLights(MoveTemp(Dl));
	}
	if (++VolumeLogCount % 150 == 1)
	{
		const FIntVector4 St = RTSDustField::GetStats();
		UE_LOG(LogTemp, Display, TEXT("RTSDustField: %d tijolos, %d puffs, %d refeitos neste quadro, %d sem lugar no atlas (cartões %d)"),
			St.X, St.Y, St.Z, St.W, Cards.Num());
	}
}

// a gravação carrega junto com o ator (Init chama; fora dela a poeira fica nos cartões)
namespace RTSTreadDustVolume
{
	// uma vez por arquivo: a fina (Ultra) e a grossa (Alta) entram cada uma no seu lugar (RTSDustField::LoadKick)
	bool LoadKickOnce(const FString& Name)
	{
		static TSet<FString> Tried;
		if (Name.IsEmpty() || Tried.Contains(Name))
		{
			return RTSDustField::IsKickLoaded();
		}
		Tried.Add(Name);
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("RTSTreadDust"));
		return Plugin.IsValid() && RTSDustField::LoadKick(Plugin->GetBaseDir() / TEXT("Resources") / Name);
	}
}
