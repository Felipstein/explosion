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

static TAutoConsoleVariable<int32> CVarTreadDustPath(TEXT("rts.TreadDust.Path"), 0,
	TEXT("A poeira das lagartas: 0 = os cartões 6-way (a v26), 1 = o volume no raymarch das explosões (RTSDustField)"), ECVF_Default);

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

bool ARTSTreadDust::IsVolumePath() const
{
	return CVarTreadDustPath.GetValueOnGameThread() == 1 && RTSDustField::IsKickLoaded();
}

void ARTSTreadDust::SubmitVolume(const FVector& CamM, const FVector& CamFwd)
{
	RTSDustField::EnsureDebugView();               // a vista de depuração (rts.DustField.Debug; desligada não custa nada)
	// o foco (o centro da janela de 192 m): onde a câmera olha no chão dos veículos
	double Zg = 0.0;
	for (const FRTSTreadSource& S : Sources)
	{
		Zg += S.PosM.Z;
	}
	Zg = Sources.Num() ? Zg / Sources.Num() : 0.0;
	const FVector Focus = CamFwd.Z < -0.05 ? CamM + CamFwd * ((Zg - CamM.Z) / CamFwd.Z) : CamM + CamFwd * 60.0;

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
		double A = L.Alpha * C.A * Smooth(K, 0.0, FMath::Max(L.Attack, 1e-3)) * (1.0 - Smooth(K, L.Decay, 1.0));
		double Mf = L.MaskEnd > 0.0 ? Smooth((Odo.FindRef(C.Src) - C.Odo0) / P.HullLen, L.MaskDense, L.MaskEnd) : 0.0;
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
		A = FMath::Min(1.0, A);
		if (C.bDouble)
		{
			A = 1.0 - (1.0 - A) * (1.0 - A);
		}
		if (A <= 1e-3 || Kd <= 1e-3)
		{
			continue;
		}
		const double Size = Atlas.SideM * L.Scale * C.S * (1.0 + (L.GrowTo - 1.0) * K) * C.Wake * Gf;
		FRTSDustPuff Pf;
		Pf.PosM = C.P + FVector(0.0, 0.0, L.RiseFade * Mf);
		Pf.Scale = float(Size / FMath::Max(Atlas.SideM, 1e-3));
		Pf.Frame = float(Atlas.FrameAt(C.Bake0 + C.Age * L.BakeRate));
		// a extinção: a profundidade óptica do cozimento das folhas (σ = 6ρ por m, bake_sixway.py) × a densidade do
		// perfil × o sumiço (Kd) × o alfa da camada (A), na escala do puff (o cartão tem o mesmo alfa em qualquer tamanho)
		Pf.Sigma = float(6.0 * P.Density * Kd * A * P.VolDensity / FMath::Max(double(Pf.Scale), 1e-3));
		// a erosão do cartão (em profundidade óptica, numa corda da nuvem) vira extinção: ÷ a corda que a câmera de cima
		// atravessa (a altura da nuvem da gravação, ~1,2 m na escala 1, × a escala); VolErode é o ajuste (1 = igual)
		const double ErodeTau = FMath::Max(L.Erode * (L.ErodeFromBirth > 0.5 ? 1.0 : Smooth(K, 0.35, 1.0)), L.ErodeFade * FMath::Pow(Mf, 2.5));
		Pf.Erode = float(P.VolErode * ErodeTau / (1.2 * FMath::Max(double(Pf.Scale), 1e-3)));
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
