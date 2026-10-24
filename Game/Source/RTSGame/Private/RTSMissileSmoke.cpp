// O rastro de fumaça dos mísseis (16/10). O estudo, as fontes e os números: docs/FUMACA_DO_MISSIL.md.
#include "RTSMissileSmoke.h"

#include "Async/ParallelFor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "HAL/IConsoleManager.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Math/SHMath.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"
#include "RTSSceneLights.h"
#include "RTSSmokeShadow.h"
#include "RTSSunShadow.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSmokeLook(TEXT("rts.MissileSmoke.Look"), 7,
		TEXT("A versão do visual da fumaça do míssil (17/10): 7 = o contorno visível (nasce denso e fino, se parte em bolotas e some pela borda), 6 e 5 = as reprovadas (o A/B)."));
	TAutoConsoleVariable<float> CVarSmokeDump(TEXT("rts.MissileSmoke.Dump"), 0.0f,
		TEXT("(17/10, o Fluxo e a Arte: dimensionar o campo da poeira pros rastros) > 0: grava uma vez, aos N s de partida, todos os puffs em captures/game/missile_puffs_<N>s.csv."));
	TAutoConsoleVariable<int32> CVarSmokeLog(TEXT("rts.MissileSmoke.Log"), 0,
		TEXT("1 = registra a cada 0,25 s a fumaça do míssil que se vê (a massa visível, os puffs à vista): a medida do que sobra quando sai o próximo tiro."));
	TAutoConsoleVariable<int32> CVarSmokeDebug(TEXT("rts.MissileSmoke.Debug"), 0,
		TEXT("A fumaça do míssil: 1 = pinta onde os quadrados da sombra caem (a cobertura, só na ShadowPath 1), 2 = a sombra 4× mais escura (pra achar; no mapa e nos quadrados), 3 = a fumaça vermelha onde o mapa do sol diz sombra (o recebimento), 0 = normal."));
	TAutoConsoleVariable<int32> CVarSmokeShadowPath(TEXT("rts.MissileSmoke.ShadowPath"), 2,
		TEXT("A sombra da fumaça do míssil (23/10): 2 = no mapa do sol que a light function lê (o mesmo das explosões e da poeira: só o sol direto, no chão, nas unidades e nos prédios; o padrão), 1 = os quadrados no chão de antes (o A/B), 0 = sem sombra."));

	// a camada junto do chão (neutra; Stull 1988, cap. 9; Kaimal e Finnigan 1994)
	constexpr float Kappa = 0.4f;                  // von Kármán
	constexpr float Z0M = 0.01f;                   // a rugosidade da areia com moita rala (a tabela de Davenport/WMO)
	constexpr float ZRefM = 10.0f;                 // o vento do Maps.json é o de 10 m
	constexpr float SigmaUPerUStar = 2.4f;         // σu ≈ 2,4 u* e σw ≈ 1,25 u* (camada neutra)
	constexpr float SigmaWPerUStar = 1.25f;
	// o jato e o puff
	constexpr float EmitStepM = 0.45f;             // um puff a cada 0,45 m do caminho (≥ 2 por σ do jato: a fita fica contínua)
	constexpr float NozzleBackM = 1.7f;            // o bocal: meio míssil atrás do centro (~3,4 m de corpo)
	constexpr float NozzleSigmaM = 0.12f;          // a boca do bocal (Ø ~0,4 m)
	constexpr float JetSigmaM = 0.45f;             // ~4–5 diâmetros de largura logo atrás da chama a 110 m/s (largura ≈ 4σ; o estudo do Fluxo)
	constexpr float JetSigmaMaxM = 1.0f;           // o raio do jato ∝ √(J/ρ)/V: mais grosso onde o míssil vai devagar (perto da caixa)
	constexpr float JetTauS = 0.06f;               // ~6 m atrás do bocal a 100 m/s
	constexpr float JetResidualMs = 6.0f;          // o que sobra da velocidade do gás pra trás, no chão parado [E]
	// a densidade óptica de lado a lado do rastro logo atrás, a 110 m/s [E pelas fotos]; o 1º vídeo (3,0) fez cortina sobre
	// as baterias: 1,6 ainda nasce branco cheio (63% de cobertura no miolo) e afina antes
	constexpr float TauLineCruise = 1.6f;
	constexpr float CruiseMs = 110.0f;
	constexpr float MinEmitMs = 20.0f;             // a fumaça por metro = a vazão / a velocidade
	constexpr float SlowMax = 3.0f;                // ... até 3× a de cruzeiro (5,5× na caixa virou uma nuvem que tapa a bateria)
	constexpr float SolidShare = 0.5f;             // a parte da extinção que é Al2O3 sólido e fica [E]
	constexpr float EvapTauS = 2.5f;               // a névoa de água e HCl que seca no ar do deserto [E]
	constexpr float BuoyCruise = 1.5f;             // o fluxo de empuxo por metro a 110 m/s, B = g Q′/(ρ cp T) (m³/s²) [E]; ∝ 1/V
	constexpr float BuoyStopS = 8.0f;              // a turbulência do ar quebra a subida (a térmica em linha não sobe pra sempre)
	constexpr float RelaxTauS = 0.35f;             // o puff acompanha o ar em ~1/3 s
	constexpr float RichardsonC = 0.64f;           // dσ²/dt = 2·0,64 ε^⅓ σ^(4/3) por eixo (Richardson–Obukhov, g ≈ 0,47: Biferale et al. 2005)
	constexpr float LagrangianTimeS = 60.0f;       // o teto: 2 σw² tL (Taylor; Stull, Practical Meteorology cap. 19)
	constexpr float CompressFromS = 2.0f;          // a compressão do tempo do jogo: o ε cresce até 8× entre 2 e 5 s (o rastro
	constexpr float CompressToS = 5.0f;            // de verdade dura minutos; aqui ele abre e afina antes de virar cortina)
	constexpr float CompressGain = 7.0f;
	constexpr float CoverTauMax = 0.25f;           // na frente de uma unidade, no máximo τ 0,25 somado (~22% de cobertura: lê inteira)
	constexpr float GlowTauS = 0.03f;              // o gás ainda incandescente: ~1–2 corpos do míssil atrás da chama (as fotos)
	constexpr int32 MaxPuffs = 6000;
	constexpr float SpriteSigmas = 2.2f;           // meio lado do quadrado em σ (a gaussiana some em ~2σ)
	constexpr float MergeSpacing = 0.8f;           // junta dois vizinhos a menos de 0,8 σ: uma fila de gaussianas a ≤ 1 σ soma
	                                               // um cilindro liso (ondulação < 1%), e o número cai ∝ 1/σ conforme engordam
	constexpr float MinVisibleTau = 0.004f;
	// (17/10, a v7: o Felipe reprovou a 5 e a 6, "espessa por muito tempo, sujando a tela, sem formato, um borrão"; o estudo
	// em docs/FUMACA_DO_MISSIL.md, "A v7") o rastro nasce denso e fino, se parte em bolotas e some pela BORDA: o contorno
	// visível de Roberts (1923; Gifford 1959; Frenkiel e Katz), a borda é onde a coluna passa de τ*, e τ* sobe com a idade (o
	// jogo comprime em ~3 s o rastro que de verdade dura minutos). A meia-largura visível w = σ √(2 ln(τ0/τ*)) cresce, chega ao
	// máximo e encolhe até zero num tempo finito: o rastro afina e se parte em vez de engordar e desbotar
	constexpr float V7TauLine = 3.2f;              // nasce denso: ~96% de cobertura no miolo (a v6 nascia com 1,6, ~80%)
	constexpr float V7JetSigmaM = 0.40f;           // e fino (largura ~1,6 m no cruzeiro)
	constexpr float V7JetSigmaMaxM = 0.6f;         // perto da caixa (a v6 ia a 1,0 m: a nuvem larga que sobrava pro tiro seguinte)
	// (a 1ª volta da v7, 1,6: a nuvem em cima da bateria, realimentada a cada rajada, ainda sobrava no tiro seguinte)
	constexpr float V7SlowMax = 1.3f;              // a fumaça por metro perto da caixa: some junto com o resto até o tiro seguinte
	constexpr float V7SolidShare = 0.3f;           // 70% condensado (a água e o HCl, que secam no ar do deserto) e 30% Al2O3
	constexpr float V7EvapTauS = 1.0f;
	constexpr float CutTau0 = 0.05f;               // τ*(t) = 0,05 + 0,40 t: o botão de arte [E], acertado no vídeo
	// (a 1ª volta, 0,40: no jogo sobravam 18–29% do pico de uma rajada quando saía a seguinte; o alvo é ≤ 10%)
	constexpr float CutTauRate = 0.50f;
	// (a 1ª volta, ±40%, a gama até 3 e o pescoço 0,55–1,45: as bolotas saíam redondas e iguais, um colar de algodão; a b,
	// com o pescoço 0,7–1,3, ainda fazia esferas perto da caixa: agora a massa passa lisa de um nó pro outro)
	constexpr float V7BlobLenM = 3.0f;             // um nó a cada ~2 diâmetros do rastro (Crow e Champagne 1971), ±60%
	constexpr float V7BlobJitter = 0.6f;
	constexpr float V7BlobGMax = 2.2f;
	// (17/10, o Felipe: "numa base com 25 Patriots atirando ... exagerado daquele jeito vai ser uma catástrofe") o orçamento
	// da tela: a massa visível perto do ponto que a câmera olha, até a de ~4 baterias no pico da rajada (~250 m² cada, medido
	// na fumaca_predio2); acima dela o τ* sobe na raiz da razão e o rastro some mais rápido. Jogabilidade antes do realismo
	// (a volta c, 1000: nas 25 Patriots o corte só subia 1,0–1,24× e a base continuava coberta)
	constexpr float ViewBudgetM2 = 600.0f;
	constexpr float ViewBudgetRadiusM = 120.0f;
	constexpr float V7MaxLifeS = 4.0f;             // só a trava: quem tira o rastro é o contorno (~2–3 s)
	// a parte que seca (a v7 seca mais e mais rápido): posta no começo de cada quadro pela versão
	float GSolidShare = SolidShare;
	float GEvapTauS = EvapTauS;
	// o diesel (o estudo: docs/DIESEL_DO_TANQUE.md; Bond e Bergstrom 2006, SAE J1667, SAE 2010-01-1287, as fichas Caterpillar)
	constexpr float ExhaustRefKw = 1100.0f;        // o tanque de ~1100 kW: a referência das quantidades (a vazão ∝ a potência)
	// a fuligem pela opacidade do gás (k, m⁻¹) × a vazão (~3,5 m³/s a plena carga num diesel de 1100 kW). O limite de
	// estrada de antes de 2008 na aceleração livre é 3,0 m⁻¹ (o MOT; SAE J1667); o diesel militar velho e sem filtro fuma
	// bem mais, e a esteira do próprio veículo dilui o gás ~1000:1 em 1–2 s (o estudo), então só o 1º metro lê de cima.
	// As duas primeiras voltas (k de estrada: 0,3 e 1 m²/s no cruzeiro) saíram invisíveis no jogo (a depuração 4 achou as
	// plumas finas). Aqui a baforada é a de um diesel bem sujo (k ~8: ~27 m²/s) e o cruzeiro na areia ~k 1,7 [E] (~3 m²/s:
	// τ ~0,3 no 1º metro do rastro), pela leitura que o Felipe pediu (o vídeo de referência: o rastro preto andando)
	constexpr float SootBurstM2 = 30.0f;           // m²/s por (acelerador − turbo)
	constexpr float SootBaseM2 = 6.0f;             // m²/s por acelerador
	constexpr float TurboUpS = 0.8f;               // o turbo enche em 0,5–1,5 s (o "turbo lag": SAE 2010-01-1287)
	constexpr float TurboDownS = 0.3f;             // e esvazia mais rápido [E]
	constexpr float ShiftCutS = 0.25f;             // a troca de marcha tira o pé por um instante [E]
	constexpr float IdleThrottle = 0.06f;          // a marcha lenta [E]
	constexpr float LaunchSpeedMs = 1.5f;          // a tração no arranque (o conversor de torque): a potência ÷ ~1,5 m/s [E]
	constexpr float ExhaustEmitS = 0.05f;          // um puff a cada 0,05 s (a 8 m/s ficam a 0,4 m: ~1 σ do jato)
	constexpr float ExhaustRelaxS = 0.1f;          // o jato do escape freia em ~0,1–0,2 s (10–20 diâmetros do cano) [E]
	constexpr float SootMinPuffM2 = 0.004f;        // menos que isso nem nasce (a marcha lenta quente quase não solta)
	constexpr float ExhaustBuoyF = 8.0f;           // o empuxo a plena carga: F = g Q/(π cp ρ T) ≈ 8 m⁴/s³ com ~0,9 MW no escape (Briggs)

	float JetSigma(float Age, float JetMax)
	{
		return NozzleSigmaM + (JetMax - NozzleSigmaM) * (1.0f - FMath::Exp(-Age / JetTauS));
	}

	// a rampa de Bridson (2007): o vento vertical some junto do chão (nada atravessa a areia)
	float Ramp(float R)
	{
		R = FMath::Clamp(R, 0.0f, 1.0f);
		return 1.875f * R - 1.25f * R * R * R + 0.375f * R * R * R * R * R;
	}

	float Visible(float Age)
	{
		return GSolidShare + (1.0f - GSolidShare) * FMath::Exp(-Age / GEvapTauS);
	}

	// (17/10, a v7) a parte do centro do puff que fica acima do contorno visível (a densidade que se vê no miolo dele)
	template <typename TPuffT>
	float ShownOf(const TPuffT& P)
	{
		return FMath::Max(1.0f - P.Cut, 0.0f);
	}

	// a fuligem é toda sólida (não seca como a névoa de água e HCl do míssil); (a revisão da vida) a da esteira do pedaço em brasa se
	// dilui no ar que ele arrasta (FadeTauS)
	template <typename TPuffT>
	float VisibleOf(const TPuffT& P)
	{
		return P.Kind == 1 ? (P.FadeTauS > 0.0f ? FMath::Exp(-P.Age / P.FadeTauS) : 1.0f) : Visible(P.Age);
	}

	FVector Hermite(const FVector& P0, const FVector& T0, const FVector& P1, const FVector& T1, float U)
	{
		const float U2 = U * U;
		const float U3 = U2 * U;
		return P0 * (2.0f * U3 - 3.0f * U2 + 1.0f) + T0 * (U3 - 2.0f * U2 + U) + P1 * (-2.0f * U3 + 3.0f * U2) + T1 * (U3 - U2);
	}

	// a iluminância das luzes perto na cor do vértice (8 bits por canal): RGBE de Ward (Graphics Gems II, 1991), o expoente
	// comum no alfa; o material decodifica (byte + 0,5) · 2^(A − 136). Preto = (0, 0, 0, 0)
	FColor EncodeRGBE(const FLinearColor& C)
	{
		const float M = FMath::Max3(C.R, C.G, C.B);
		if (!(M > 1e-6f))
		{
			return FColor(0, 0, 0, 0);
		}
		int32 E = FMath::FloorToInt32(FMath::Log2(M)) + 1;   // M = m · 2^E, m em [0,5; 1)
		float Scale = 256.0f * FMath::Exp2(-float(E));
		if (M * Scale >= 256.0f)
		{
			++E;
			Scale *= 0.5f;
		}
		auto Q = [Scale](float X) { return uint8(FMath::Clamp(FMath::FloorToInt32(FMath::Max(X, 0.0f) * Scale), 0, 255)); };
		return FColor(Q(C.R), Q(C.G), Q(C.B), uint8(FMath::Clamp(E + 128, 0, 255)));
	}

	FVector HermiteTangent(const FVector& P0, const FVector& T0, const FVector& P1, const FVector& T1, float U)
	{
		const float U2 = U * U;
		return P0 * (6.0f * U2 - 6.0f * U) + T0 * (3.0f * U2 - 4.0f * U + 1.0f) + P1 * (-6.0f * U2 + 6.0f * U) + T1 * (3.0f * U2 - 2.0f * U);
	}

	float DistToSegment(const FVector& P, const FVector& A, const FVector& B, FVector& OutAway)
	{
		const FVector AB = B - A;
		const double L2 = AB.SizeSquared();
		const double T = L2 > 1e-6 ? FMath::Clamp(FVector::DotProduct(P - A, AB) / L2, 0.0, 1.0) : 0.0;
		const FVector Away = P - (A + AB * T);
		const float D = float(Away.Size());
		OutAway = D > 1e-4f ? Away / D : FVector::UpVector;
		return D;
	}
}

float FRTSMissileSmoke::FPuff::Sigma() const
{
	const float J = JetSigma(Age, JetMax);
	return FMath::Sqrt(S2 + J * J);
}

bool FRTSMissileSmoke::Init(UWorld* World)
{
	Shutdown();
	if (!World)
	{
		return false;
	}
	UMaterialInterface* SmokeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FX/MissileSmoke/M_MissileSmoke.M_MissileSmoke"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* ShadowMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FX/MissileSmoke/M_MissileSmokeShadow.M_MissileSmokeShadow"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!SmokeMat || !ShadowMat)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS fumaça do míssil: sem os materiais (/Game/FX/MissileSmoke; game.ps1 content); fica o rastro do BlastFx"));
		return false;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	AActor* A = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!A)
	{
		return false;
	}
	USceneComponent* Root = NewObject<USceneComponent>(A, TEXT("Root"));
	A->SetRootComponent(Root);
	Root->RegisterComponent();
	auto MakeMesh = [A, Root](const TCHAR* Name, int32 SortPriority) -> UProceduralMeshComponent*
	{
		UProceduralMeshComponent* M = NewObject<UProceduralMeshComponent>(A, Name);
		M->SetupAttachment(Root);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetGenerateOverlapEvents(false);
		M->SetCastShadow(false);
		M->bReceivesDecals = false;
		M->bUseAsOccluder = false;
		M->SetTranslucentSortPriority(SortPriority);
		M->RegisterComponent();
		return M;
	};
	UProceduralMeshComponent* SmokeMesh = MakeMesh(TEXT("MissileSmoke"), 2);
	UProceduralMeshComponent* GroundMesh = MakeMesh(TEXT("MissileSmokeShadow"), -2);
	SmokeMID.Reset(UMaterialInstanceDynamic::Create(SmokeMat, SmokeMesh));
	ShadowMID.Reset(UMaterialInstanceDynamic::Create(ShadowMat, GroundMesh));
	// já nas malhas desde o começo (a lista de materiais da malha também segura): nunca o material padrão cinza
	SmokeMesh->SetMaterial(0, SmokeMID.Get());
	GroundMesh->SetMaterial(0, ShadowMID.Get());
	// o material novo lê as luzes perto; o velho (sem o parâmetro) fica com a normal da vista e sem a cor
	float LocalLightsV = 0.0f;
	bLocalLightsMat = SmokeMID.IsValid() && SmokeMID->GetScalarParameterValue(FMaterialParameterInfo(TEXT("LocalLightsV1")), LocalLightsV) && LocalLightsV > 0.5f;
	WorldRef = World;
	Actor = A;
	Mesh = SmokeMesh;
	ShadowMesh = GroundMesh;
	Capacity = 0;
	bReady = true;
	UE_LOG(LogTemp, Log, TEXT("RTS fumaça do míssil: pronta (puffs, vento do mapa, turbulência, empurrão e sombra; luzes perto: %s)"),
		bLocalLightsMat ? TEXT("sim") : TEXT("não, material velho"));
	return true;
}

void FRTSMissileSmoke::Shutdown()
{
	if (AActor* A = Actor.Get())
	{
		A->Destroy();
	}
	Actor = nullptr;
	Mesh = nullptr;
	ShadowMesh = nullptr;
	SmokeMID.Reset();
	ShadowMID.Reset();
	Trails.Reset();
	Blasts.Reset();
	Capacity = 0;
	bReady = false;
	// (23/10) o mapa do sol sem os puffs deste rastro (o registro também os dá por velhos em poucos quadros sem o Set)
	if (ShadowExported > 0)
	{
		RTSSmokeShadow::SetPuffs_GameThread({});
	}
	ShadowExported = 0;
	MapMissS = 0.0f;
	bShadowQuads = false;
}

int32 FRTSMissileSmoke::NumPuffs() const
{
	int32 N = 0;
	for (const FTrail& T : Trails)
	{
		N += T.Puffs.Num();
	}
	return N;
}

void FRTSMissileSmoke::Launch(const FVector& MouthCm, const FVector& Dir, uint32 Seed)
{
	if (!bReady)
	{
		return;
	}
	// o lançamento quente da Patriot: a caixa é aberta atrás e o motor acende dentro dela; o jato sai pela ré, bate no chão
	// e abre rente a ele (o jato de parede), e a nuvem branca envolve o lançador por ~1–2 s. Uma "trilha" sem míssil, com
	// os puffs já grossos (a fumaça por metro a ~0 m/s é a maior de todas: aqui vai como a de 20 m/s, a densa)
	FRandomStream Rng(int32(Seed * 2654435761u >> 1));
	FTrail& T = Trails.AddDefaulted_GetRef();
	T.Missile = 0;
	T.bLive = false;
	const FVector D = Dir.GetSafeNormal();
	const FVector Back = -FVector(D.X, D.Y, 0.0).GetSafeNormal();
	const FVector Rear = MouthCm / 100.0 - D * 3.5;          // a ré da caixa do modelo (~3,5 m da boca; a real tem ~5 m)
	const float LineExt = TauLineCruise * UE_SQRT_2 * FMath::Sqrt(UE_PI) * JetSigmaM * SlowMax;
	for (int32 K = 0; K < 6; ++K)
	{
		FPuff P;
		const float Spread = Rng.FRandRange(-0.9f, 0.9f);
		const FVector Out = (Back + FVector(-Back.Y, Back.X, 0.0) * Spread).GetSafeNormal();
		P.P = Rear + Out * Rng.FRandRange(0.0f, 1.5f);
		P.V = Out * Rng.FRandRange(7.0f, 15.0f) + FVector(0.0, 0.0, Rng.FRandRange(-2.0f, 1.0f));
		P.S2 = FMath::Square(Rng.FRandRange(0.3f, 0.6f));
		P.JetMax = Rng.FRandRange(0.9f, 1.4f);
		P.Extinction = LineExt * 0.6f;
		P.Age = Rng.FRandRange(0.0f, 0.05f);
		P.Heat = 0.5f;
		P.Buoy = BuoyCruise * SlowMax;
		P.Seed = Rng.FRand();
		T.Puffs.Add(P);
	}
}

void FRTSMissileSmoke::Blast(const FVector& AtCm, float RadiusM, float RiseMs, float RiseTauS)
{
	if (!bReady || RadiusM <= 0.0f)
	{
		return;
	}
	FBlast& B = Blasts.AddDefaulted_GetRef();
	B.P = AtCm / 100.0;
	B.R = RadiusM;
	B.W = RiseMs;
	B.Age = 0.0f;
	B.RiseTau = FMath::Max(RiseTauS, 0.1f);
}

void FRTSMissileSmoke::TickSmolders(float Dt)
{
	// (23/10, a explosão com vida; o plano, item 9) o ponto atingido fumega (WT: a fumaça no ponto do golpe; de verdade por dezenas de
	// segundos, aqui comprimido): um "rastro" sem míssil por ponto, com os puffs saindo dele no ritmo que o jogo dá (a massa de cada um cai
	// em e^(−t/τ) do lado do jogo: o fio afina em vez de virar contas). A fuligem (Kind 1) é a do diesel: escura, absorve, sem o contorno
	// da v7 (que é da névoa do motor do míssil); a clara (Kind 0) é a do míssil, com o contorno. Quando o ponto sai da lista (acabou, a
	// vítima morreu ou entrou na névoa), o rastro fica e envelhece como os outros
	for (const FRTSSmokeSmolder& Sm : F.Smolders)
	{
		const bool bPath = Sm.StepM > 0.0f;       // (a revisão da vida) o ponto que anda: um puff a cada StepM m do caminho
		if ((Sm.RatePerS <= 0.0f && !bPath) || Sm.ExtM2 <= 0.0f)
		{
			continue;
		}
		const uint32 Key = 0x40000000u | (Sm.Id & 0x3FFFFFFFu);
		FTrail* T = Trails.FindByPredicate([Key](const FTrail& Tr) { return Tr.bLive && Tr.Missile == Key; });
		const FVector At = Sm.PosCm / 100.0;
		if (!T)
		{
			T = &Trails.AddDefaulted_GetRef();
			T->Missile = Key;
			T->bLive = true;
			T->Carry = 1.0f;                      // o primeiro puff já no quadro em que começa
			T->HeadM = At;
		}
		T->SinceSeen = 0.0f;
		// o caminho deste quadro (o ponto que anda: os puffs ao longo dele, o mais velho atrás, cada um com a idade de quando o ponto passou)
		const FVector Back = T->HeadM - At;
		const float PathM = bPath ? float(Back.Size()) : 0.0f;
		T->Carry += bPath ? PathM / Sm.StepM : Sm.RatePerS * Dt;
		T->Carry = FMath::Min(T->Carry, 64.0f);   // um salto (o ponto que voltou da névoa): no máximo 64 de uma vez
		FRandomStream Rng(int32((Sm.Id * 2654435761u) ^ (SeedCounter++ * 0x9E3779B9u)) & 0x7fffffff);
		while (T->Carry >= 1.0f)
		{
			T->Carry -= 1.0f;
			FPuff Pf;
			const float BackM = bPath ? FMath::Min(T->Carry * Sm.StepM, PathM) : 0.0f;
			const FVector Base = bPath && PathM > 1.0e-3f ? At + Back * double(BackM / PathM) : At;
			Pf.P = Base + Rng.GetUnitVector() * (0.25 * Sm.SigmaM);
			// a fumaça morna sobe: a térmica em linha de Briggs (w = ⅔·1,6·B^⅓·t^−⅓) com o B que dá a subida pedida aos ~1 s
			Pf.V = FVector(0.0, 0.0, Sm.RiseMs) + Rng.GetUnitVector() * 0.3;
			Pf.S2 = 0.0f;
			Pf.JetMax = Sm.SigmaM * Rng.FRandRange(0.85f, 1.15f);
			if (bPath)
			{
				// (a 2ª revisão da vida) o puff do caminho é um trecho de StepM da esteira já misturada atrás do pedaço: nasce com o σ dela, sem
				// a abertura do jato a partir do bocal (com o passo largo, os primeiros saíam contas de 0,12 m atrás da cabeça)
				Pf.S2 = FMath::Max(FMath::Square(Pf.JetMax) - FMath::Square(NozzleSigmaM), 0.0f);
				Pf.JetMax = NozzleSigmaM;
			}
			Pf.Extinction = Sm.ExtM2 * Rng.FRandRange(0.8f, 1.2f);
			Pf.Age = bPath && PathM > 1.0e-3f ? Dt * (BackM / PathM) : Rng.FRandRange(0.0f, FMath::Min(Dt, 0.05f));
			Pf.Heat = 0.0f;
			Pf.Buoy = FMath::Pow(FMath::Max(Sm.RiseMs, 0.1f) / ((2.0f / 3.0f) * 1.6f), 3.0f);
			Pf.Seed = Rng.FRand();
			Pf.Kind = Sm.Kind;
			Pf.FadeTauS = Sm.FadeTauS;
			Pf.SunT = T->Puffs.Num() ? T->Puffs.Last().SunT : 1.0f;
			Pf.SkyT = T->Puffs.Num() ? T->Puffs.Last().SkyT : 1.0f;
			T->Puffs.Add(Pf);
		}
		T->HeadM = At;
	}
}

void FRTSMissileSmoke::Muzzle(const FVector& AtCm, const FVector& Dir)
{
	if (!bReady)
	{
		return;
	}
	FBlast& B = Blasts.AddDefaulted_GetRef();
	B.P = AtCm / 100.0;
	B.R = 2.5f;
	B.W = 0.0f;
	B.Dir = Dir.GetSafeNormal();
	B.Age = 0.0f;
}

void FRTSMissileSmoke::Emit(FTrail& T, const FVector& ToM, const FVector& ToDir, float SpeedMs, float MissileAge)
{
	const FVector From = T.HeadM;
	const double L = FVector::Dist(From, ToM);
	if (L < 1e-3)
	{
		return;
	}
	// o caminho entre os dois quadros por uma curva de Hermite com as direções do voo (o míssil faz curva lisa: a
	// navegação proporcional), soltando um puff a cada EmitStepM; o puff solto antes no trecho já nasce mais velho
	const FVector T0 = T.HeadDir * L;
	const FVector T1 = ToDir * L;
	const float Speed = FMath::Max(0.5f * (T.HeadSpeed + SpeedMs), 1.0f);
	const bool bV7 = LookVersion >= 7;
	const float JetS = bV7 ? V7JetSigmaM : JetSigmaM;
	const float Slow = FMath::Min(CruiseMs / FMath::Max(Speed, MinEmitMs), bV7 ? V7SlowMax : SlowMax);   // a fumaça e o calor por metro = a vazão / a velocidade
	const float Ext = (bV7 ? V7TauLine : TauLineCruise) * UE_SQRT_2 * FMath::Sqrt(UE_PI) * JetS * Slow * EmitStepM;
	const float JetMax = FMath::Clamp(JetS * Slow, JetS, bV7 ? V7JetSigmaMaxM : JetSigmaMaxM);
	FRandomStream Rng(int32((T.Missile * 2654435761u) ^ (SeedCounter++ * 0x9E3779B9u)) & 0x7fffffff);
	double S = EmitStepM - T.Carry;
	while (S <= L)
	{
		const float U = float(S / L);
		FPuff P;
		P.P = Hermite(From, T0, ToM, T1, U);
		const FVector Dn = HermiteTangent(From, T0, ToM, T1, U).GetSafeNormal();
		// o sorteio de lado e de tamanho: a borda do rastro não fica uma fila de bolinhas iguais (o 1º vídeo)
		const FVector Side = FVector::CrossProduct(Dn, Rng.GetUnitVector()).GetSafeNormal();
		P.P += Side * (Rng.FRandRange(0.0f, 0.35f) * JetMax);
		P.V = -Dn * JetResidualMs + Rng.GetUnitVector() * 0.6f;
		P.S2 = FMath::Square(Rng.FRandRange(0.0f, 0.3f) * JetMax);
		P.Extinction = Ext * Rng.FRandRange(0.75f, 1.25f);
		P.JetMax = JetMax * Rng.FRandRange(0.85f, 1.15f);
		P.Buoy = BuoyCruise * Slow;
		P.Age = float((L - S) / Speed);
		P.Heat = 0.0f;
		P.Seed = Rng.FRand();
		P.SunT = T.Puffs.Num() ? T.Puffs.Last().SunT : 1.0f;
		P.SkyT = T.Puffs.Num() ? T.Puffs.Last().SkyT : 1.0f;
		if (bV7)
		{
			// (17/10, a v7) a forma: a massa por metro flutua ao longo do rastro (as flutuações de concentração nascem perto da
			// fonte; Fackrell e Robins 1982), com a escala do trem de vórtices do jato (~2 diâmetros; Crow e Champagne 1971): nós a
			// cada ~3 m ±60%, cada um com uma gama k = 2 de média 1, e a massa passando lisa de um nó pro outro. Com o contorno, os
			// trechos fracos somem primeiro e o rastro se parte em pedaços irregulares antes de sumir. (A volta b fazia bolotas
			// separadas, com o pescoço entre elas e a união sem atravessar: cada uma virava uma esfera, um colar de pérolas)
			if (T.BlobLeftM <= 0.0f)
			{
				T.BlobLenM = V7BlobLenM * Rng.FRandRange(1.0f - V7BlobJitter, 1.0f + V7BlobJitter);
				T.BlobLeftM = T.BlobLenM;
				T.BlobPrevG = T.BlobG;
				const float G = 0.5f * (-FMath::Loge(FMath::Max(Rng.FRand(), 1e-4f)) - FMath::Loge(FMath::Max(Rng.FRand(), 1e-4f)));
				T.BlobG = FMath::Min(G, V7BlobGMax);
			}
			// (a volta e: a força vai no corte, não na massa; na massa, as variações de até 5× em 3 m faziam bolotas já no rastro
			// novo, perto da caixa)
			const float Ub = 1.0f - T.BlobLeftM / T.BlobLenM;
			P.Fluct = FMath::Lerp(T.BlobPrevG, T.BlobG, FMath::SmoothStep(0.0f, 1.0f, Ub));
			T.BlobLeftM -= EmitStepM;
		}
		T.Puffs.Add(P);
		S += EmitStepM;
	}
	T.Carry = float(L - (S - EmitStepM));
	T.HeadM = ToM;
	T.HeadDir = ToDir;
	T.HeadSpeed = SpeedMs;
	(void)MissileAge;
}

FVector FRTSMissileSmoke::AirVelocity(const FVector& P, float Sigma, FVector* InOutTurb) const
{
	// o vento médio: o perfil logarítmico da camada neutra, U(z) = U10 ln(z/z0) / ln(10/z0)
	const float Ground = F.GroundZCm ? F.GroundZCm(P.X * 100.0, P.Y * 100.0) / 100.0f : 0.0f;
	const float Z = FMath::Clamp(float(P.Z) - Ground, 2.0f * Z0M, 100.0f);
	FVector U = WindUnit * (WindMs * FMath::Loge(Z / Z0M) / FMath::Loge(ZRefM / Z0M));
	// os redemoinhos maiores que o puff entortam o rastro (os menores já estão no σ que cresce): cada modo é uma onda
	// sem divergência (a amplitude ⊥ ao número de onda; Kraichnan 1970), levada pelo vento médio (a hipótese de Taylor)
	if (InOutTurb && !InOutTurb->IsZero())
	{
		return U + *InOutTurb;                 // o deste quadro é o do quadro de antes
	}
	const FVector Adv = P - WindUnit * (double(WindMs) * Clock);
	FVector Turb = FVector::ZeroVector;
	for (const FMode& M : Modes)
	{
		const float Lambda = 2.0f * UE_PI / float(M.K.Size());
		const float W = FMath::SmoothStep(2.0f * Sigma, 4.0f * Sigma, Lambda);
		if (W <= 0.0f)
		{
			continue;
		}
		Turb += M.A * (W * FMath::Cos(float(FVector::DotProduct(M.K, Adv)) + M.Omega * float(Clock) + M.Phase));
	}
	Turb.Z *= Ramp(Z / 3.0f);
	if (InOutTurb)
	{
		*InOutTurb = Turb.IsZero() ? FVector(0.0, 0.0, 1e-4) : Turb;
	}
	return U + Turb;
}

namespace
{
	// (17/10) o alcance de cada um (m), pra caixa do rastro: o veículo 6 a (a = o raio + meio σ), o tiro o túnel, a explosão o
	// sopro (Δr ≈ R³/3r²: a 5 R é ~1% de R) e o anel que sobe (e^−(r/1,5R)²: a 4,5 R é e^−9)
	constexpr float MoverReachA = 6.0f;
	constexpr float BlastPushReachR = 5.0f;
	constexpr float BlastRingReachR = 4.5f;
}

void FRTSMissileSmoke::NearOf(const FTrail& T, FNear& Out) const
{
	FBox Box(EForceInit::ForceInit);
	float MaxS = 0.0f;
	for (const FPuff& P : T.Puffs)
	{
		Box += P.P;
		MaxS = FMath::Max(MaxS, P.Sigma());
	}
	if (!Box.IsValid)
	{
		return;
	}
	// a folga: o maior σ e o que o puff anda num quadro
	Box = Box.ExpandBy(double(MaxS) + 1.0);
	for (int32 K = 0; K < F.Movers.Num(); ++K)
	{
		const FRTSSmokeMover& M = F.Movers[K];
		if (M.VelCmS.SizeSquared() < 30.0 * 30.0)
		{
			continue;
		}
		const float Reach = MoverReachA * (M.RadiusCm / 100.0f + 0.5f * MaxS);
		if (Box.ComputeSquaredDistanceToPoint(M.PosCm / 100.0 + FVector(0.0, 0.0, 1.2)) < double(Reach) * Reach)
		{
			Out.Movers.Add(K);
		}
	}
	for (int32 K = 0; K < F.Shots.Num(); ++K)
	{
		const TPair<FVector, FVector>& S = F.Shots[K];
		FBox Seg(EForceInit::ForceInit);
		Seg += S.Key / 100.0;
		Seg += S.Value / 100.0;
		if (Seg.ExpandBy(0.6 + 0.5 * MaxS).Intersect(Box))
		{
			Out.Shots.Add(K);
		}
	}
	for (int32 K = 0; K < Blasts.Num(); ++K)
	{
		const FBlast& B = Blasts[K];
		const bool bPush = B.Age <= 0.0f;
		if (!bPush && B.W <= 0.0f)
		{
			continue;   // o sopro de boca já passou e não sobe: não faz mais nada
		}
		const float Reach = (bPush ? BlastPushReachR : BlastRingReachR) * B.R;
		FVector Q = B.P;
		if (!bPush)
		{
			Q.Z = FMath::Clamp(Q.Z, Box.Min.Z, Box.Max.Z);   // o anel só depende da distância na horizontal
		}
		if (Box.ComputeSquaredDistanceToPoint(Q) < double(Reach) * Reach)
		{
			Out.Blasts.Add(K);
		}
	}
}

void FRTSMissileSmoke::ApplyMovers(FPuff& Pf, float Sigma, float Dt, const FNear& Near) const
{
	// (1) os veículos: o ar em volta de uma esfera de raio a andando a U (o escoamento potencial: o dipolo
	// u = a³/(2r³) (3 (U·r̂) r̂ − U)), mais a esteira atrás dela, que arrasta o ar junto e mistura (a fumaça rasga)
	FVector Push = FVector::ZeroVector;
	for (const int32 Mi : Near.Movers)
	{
		const FRTSSmokeMover& M = F.Movers[Mi];
		const FVector C = M.PosCm / 100.0 + FVector(0.0, 0.0, 1.2);
		const FVector U = M.VelCmS / 100.0;
		const float Us = float(U.Size());
		const float A = M.RadiusCm / 100.0f + 0.5f * Sigma;
		const FVector R = Pf.P - C;
		const float D = float(R.Size());
		if (Us < 0.3f || D > 6.0f * A)
		{
			continue;
		}
		const FVector Rh = D > 1e-3f ? R / D : FVector::UpVector;
		if (D > A)
		{
			const float K = A * A * A / (2.0f * D * D * D);
			Push += (Rh * (3.0 * FVector::DotProduct(U, Rh)) - U) * K;
		}
		else
		{
			Push += U + Rh * Us;                    // dentro do casco: o ar sai da frente
		}
		const FVector Uh = U / Us;
		const float Along = -float(FVector::DotProduct(R, Uh));
		if (Along > 0.0f)
		{
			const float Lat = float((R + Uh * Along).Size());
			const float W = FMath::Exp(-Lat * Lat / (A * A)) * FMath::Exp(-Along / (4.0f * A));
			Push += U * (0.45f * W);
			// a viscosidade turbulenta da esteira, ~0,1 U a [E]: o puff engorda e afina (a massa fica)
			Pf.S2 += 2.0f * 0.1f * Us * A * W * Dt;
		}
	}
	Pf.V += Push * (1.0f - FMath::Exp(-Dt / RelaxTauS));
	// (2) o tiro e o outro míssil furam um túnel: o choque empurra pra fora do trecho que o projétil andou
	for (const int32 Si : Near.Shots)
	{
		const TPair<FVector, FVector>& S = F.Shots[Si];
		FVector Away;
		const float D = DistToSegment(Pf.P, S.Key / 100.0, S.Value / 100.0, Away);
		// o choque e a esteira do projétil furam pouco de verdade (centímetros); um túnel pequeno que lê de cima [E]
		const float R = 0.6f + 0.5f * Sigma;
		if (D < R)
		{
			const float W = 1.0f - D / R;
			Pf.V += Away * (8.0f * W);
			Pf.S2 += 0.15f * W;
		}
	}
	// (3) as explosões. A onda de choque desloca o ar só uns centímetros (a fase positiva dura ~10 ms); quem mexe na
	// fumaça é a bola de fogo: ela abre o ar em volta (incompressível: Δr = ∛(r³ + R³) − r, 26% de R na borda, R³/3r² longe)
	// e depois sobe quente como um anel, puxando a fumaça de perto pra dentro e pra cima por alguns segundos
	for (const int32 Bi : Near.Blasts)
	{
		const FBlast& B = Blasts[Bi];
		const FVector R = Pf.P - B.P;
		const float D = float(R.Size());
		if (D > BlastPushReachR * B.R && B.Age <= 0.0f)
		{
			continue;
		}
		const FVector Rh = D > 1e-3f ? R / D : FVector::UpVector;
		float Cone = 1.0f;
		if (!B.Dir.IsZero())
		{
			Cone = FMath::SmoothStep(0.77f, 0.97f, float(FVector::DotProduct(Rh, B.Dir)));
		}
		if (B.Age <= 0.0f)
		{
			const float Dr = FMath::Pow(D * D * D + B.R * B.R * B.R, 1.0f / 3.0f) - D;
			Pf.P += Rh * (Dr * Cone);
			Pf.S2 += 0.3f * B.R * FMath::Square(B.R / FMath::Max(D, B.R)) * Cone;   // a mistura na passagem [E]
			if (D < 1.5f * B.R && B.Dir.IsZero())
			{
				Pf.Heat = FMath::Max(Pf.Heat, 1.0f - D / (1.5f * B.R));
			}
		}
		else if (B.W > 0.0f)
		{
			// o anel que sobe: a coluna de ar quente no eixo e a entrada por baixo (a fumaça perto é sugada e vai junto)
			const FVector Hor(R.X, R.Y, 0.0);
			const float Dh = float(Hor.Size());
			const float Rise = B.W * FMath::Exp(-B.Age / B.RiseTau) * FMath::Exp(-FMath::Square(Dh / (1.5f * B.R)));
			const FVector In = Dh > 1e-3f ? -Hor / Dh : FVector::ZeroVector;
			const FVector Want = FVector(0.0, 0.0, Rise) + In * (0.3f * Rise * FMath::Min(Dh / B.R, 1.0f));
			Pf.V += Want * (1.0f - FMath::Exp(-Dt / RelaxTauS));
		}
	}
}

void FRTSMissileSmoke::TickExhausts(float Dt)
{
	// o motor de cada veículo a diesel (só desenho): um veículo "virtual" que ganha velocidade pela potência, a = P/(m·v) − μg
	// (na simulação do RTS a arrancada é quase instantânea; o motor de verdade leva ~2–5 s), o acelerador que vai a fundo
	// enquanto ele não alcança a velocidade da simulação, as marchas automáticas tirando o pé, e o turbo que atrasa: a
	// fuligem sai onde o combustível passa o ar (o acelerador acima da pressão do turbo), mais uma base ∝ a carga. A baforada
	// nasce sozinha na arrancada e em cada troca, e some quando o turbo enche (SAE 2010-01-1287, a patente US4502437)
	for (TPair<uint32, FEngine>& It : Engines)
	{
		It.Value.Unseen += Dt;
	}
	for (const FRTSSmokeExhaust& X : F.Exhausts)
	{
		const int32 NOut = X.Outlets.Num();
		if (NOut == 0)
		{
			continue;
		}
		FEngine& E = Engines.FindOrAdd(X.Id);
		E.Unseen = 0.0f;
		const float Vmax = FMath::Max(X.MaxSpeedMs, 1.0f);
		const float K = 1.0f - FMath::Exp(-12.0f * Dt);
		E.Speed = E.bInit ? E.Speed + (FMath::Abs(X.SpeedMs) - E.Speed) * K : FMath::Abs(X.SpeedMs);
		const float Target = FMath::Min(E.Speed, Vmax);
		if (E.LastOutletM.Num() != NOut)
		{
			E.LastOutletM.SetNum(NOut);
			for (int32 Kx = 0; Kx < NOut; ++Kx)
			{
				E.LastOutletM[Kx] = X.Outlets[Kx].Key / 100.0;
			}
		}
		if (!E.bInit)
		{
			E.U = Target;                        // o primeiro quadro à vista: o motor já no regime (sem baforada falsa)
			E.Boost = IdleThrottle;
		}
		const float M = FMath::Max(X.MassT, 1.0f) * 1000.0f;
		const float P = FMath::Max(X.PowerKw, 1.0f) * 1000.0f;
		const float Amax = FMath::Max(P / (M * FMath::Max(E.U, LaunchSpeedMs)) - X.Roll * 9.81f, 0.2f);
		bool bAccel = false;
		if (Target > E.U + 0.05f)
		{
			E.U = FMath::Min(Target, E.U + Amax * Dt);
			bAccel = true;
		}
		else
		{
			E.U = Target;                        // freia na hora (o freio não precisa do motor)
		}
		// a carga sem acelerar: rolar na areia (μ m g v), virar (a lagarta escorrega de lado) e trabalhar (a lâmina) [E]
		const float Cruise = X.Roll * M * 9.81f * E.U / P;
		const float Turn = 0.5f * FMath::Clamp(FMath::Abs(X.YawRateDps) / FMath::Max(X.TurnDps, 1.0f), 0.0f, 1.0f);
		float Throttle = bAccel ? 1.0f : FMath::Clamp(IdleThrottle + Cruise + Turn + (X.bWorking ? X.WorkLoad : 0.0f), IdleThrottle, 1.0f);
		// as marchas: sobe quando o veículo passa cada 1/N da velocidade máxima acelerando, e tira o pé um instante
		const int32 Gears = FMath::Max(X.Gears, 1);
		const int32 G = FMath::Clamp(FMath::FloorToInt32(E.U / Vmax * float(Gears)), 0, Gears - 1);
		if (G > E.Gear && bAccel)
		{
			E.Shift = ShiftCutS;
		}
		E.Gear = G;
		if (E.Shift > 0.0f)
		{
			Throttle = FMath::Min(Throttle, 0.25f);
			E.Shift -= Dt;
		}
		E.Throttle = Throttle;
		E.Boost += (E.Throttle - E.Boost) * (1.0f - FMath::Exp(-Dt / (E.Throttle > E.Boost ? TurboUpS : TurboDownS)));
		E.bInit = true;
		const float Scale = X.PowerKw / ExhaustRefKw;
		E.Carry += Scale * (SootBurstM2 * FMath::Max(E.Throttle - E.Boost, 0.0f) + SootBaseM2 * E.Throttle) * Dt;
		E.Clock += Dt;
		const bool bEmit = E.Clock >= ExhaustEmitS && E.Carry >= SootMinPuffM2;
		const FVector VehV = X.FwdW * double(X.SpeedMs);
		const FVector Rel = VehV - FVector(WindUnit.X, WindUnit.Y, 0.0) * double(WindMs);
		const float URel = FMath::Max(float(Rel.Size2D()), 1.0f);
		FRandomStream Rng(int32((X.Id * 2654435761u) ^ (SeedCounter++ * 0x9E3779B9u)) & 0x7fffffff);
		for (int32 Kx = 0; Kx < NOut; ++Kx)
		{
			// uma trilha por saída (o vizinho no caminho é o puff de antes da mesma saída: o Merge junta ao longo dela)
			const uint32 Key = 0x80000000u | ((X.Id & 0x0FFFFFFFu) << 3) | uint32(Kx & 7);
			FTrail* T = Trails.FindByPredicate([Key](const FTrail& Tr) { return Tr.bLive && Tr.Missile == Key; });
			if (!T)
			{
				T = &Trails.AddDefaulted_GetRef();
				T->Missile = Key;
				T->bLive = true;
			}
			T->SinceSeen = 0.0f;
			const FVector At = X.Outlets[Kx].Key / 100.0;
			if (bEmit)
			{
				const FVector Dir = X.Outlets[Kx].Value.GetSafeNormal();
				FPuff Pf;
				// o puff nasce entre a saída de agora e a de antes (o veículo andou), com o jato pra fora e a velocidade dele
				Pf.P = FMath::Lerp(E.LastOutletM[Kx], At, double(Rng.FRandRange(0.5f, 1.0f))) + Rng.GetUnitVector() * (0.05 * X.JetSigmaM);
				Pf.V = VehV + Dir * double(X.ExitMs * (0.4f + 0.6f * Throttle)) + Rng.GetUnitVector() * 0.5;
				Pf.S2 = 0.0f;
				Pf.JetMax = X.JetSigmaM * Rng.FRandRange(0.85f, 1.15f);
				Pf.Extinction = E.Carry / float(NOut) * Rng.FRandRange(0.8f, 1.2f);
				Pf.Age = 0.0f;
				Pf.Heat = 0.0f;
				// a pluma inclinada de Briggs (Δh = 1,6 F^⅓ x^⅔ / u): no puff, a térmica em linha com B = F/u, u = o ar que passa
				Pf.Buoy = ExhaustBuoyF * (Throttle + 0.1f) / 1.1f * Scale / float(NOut) / URel;
				Pf.Seed = Rng.FRand();
				Pf.Kind = 1;
				Pf.RelaxS = ExhaustRelaxS;
				Pf.SunT = T->Puffs.Num() ? T->Puffs.Last().SunT : 1.0f;
				Pf.SkyT = T->Puffs.Num() ? T->Puffs.Last().SkyT : 1.0f;
				static int32 GLoggedSoot = 0;
				if (GLoggedSoot < 12 && (GLoggedSoot < 3 || Pf.Extinction > 0.3f))
				{
					++GLoggedSoot;
					UE_LOG(LogTemp, Log, TEXT("RTS diesel: puff do veículo %u saída %d em (%.1f, %.1f, %.1f) m, extinção %.3f m², acelerador %.2f, turbo %.2f, v %.1f m/s"),
						X.Id, Kx, Pf.P.X, Pf.P.Y, Pf.P.Z, Pf.Extinction, Throttle, E.Boost, E.Speed);
				}
				T->Puffs.Add(Pf);
			}
			E.LastOutletM[Kx] = At;
		}
		if (bEmit)
		{
			E.Carry = 0.0f;
			E.Clock = 0.0f;
		}
	}
	for (auto It = Engines.CreateIterator(); It; ++It)
	{
		if (It.Value().Unseen > 2.0f)
		{
			It.RemoveCurrent();
		}
	}
}

void FRTSMissileSmoke::Merge(FTrail& T)
{
	// dois vizinhos do caminho muito juntos viram um (a extinção soma; a posição e a velocidade pela média pesada; o σ²
	// pelo segundo momento, a distância entre eles dividida nos 3 eixos): menos puffs e a sobreposição fica ~2–3
	for (int32 I = 0; I + 1 < T.Puffs.Num(); ++I)
	{
		FPuff& A = T.Puffs[I];
		const FPuff& B = T.Puffs[I + 1];
		if (A.Age < 0.3f || B.Age < 0.3f)
		{
			continue;
		}
		const float Sa = A.Sigma();
		const float Sb = B.Sigma();
		const float D2 = float(FVector::DistSquared(A.P, B.P));
		if (D2 > FMath::Square(MergeSpacing * 0.5f * (Sa + Sb)))
		{
			continue;
		}
		const float E = A.Extinction + B.Extinction;
		if (E <= 0.0f)
		{
			continue;
		}
		const float Wa = A.Extinction / E;
		const float Wb = B.Extinction / E;
		const float Age = Wa * A.Age + Wb * B.Age;
		const float Total = Wa * Sa * Sa + Wb * Sb * Sb + Wa * Wb * D2 / 3.0f;
		const float JetMax = Wa * A.JetMax + Wb * B.JetMax;
		const float J = JetSigma(Age, JetMax);
		A.P = A.P * Wa + B.P * Wb;
		A.V = A.V * Wa + B.V * Wb;
		A.Age = Age;
		A.S2 = FMath::Max(Total - J * J, 0.0f);
		A.Extinction = E;
		A.JetMax = JetMax;
		A.Buoy = Wa * A.Buoy + Wb * B.Buoy;
		A.Heat = FMath::Max(A.Heat, B.Heat);
		A.Fluct = Wa * A.Fluct + Wb * B.Fluct;
		A.SunT = Wa * A.SunT + Wb * B.SunT;
		A.SkyT = Wa * A.SkyT + Wb * B.SkyT;
		A.CoverScale = FMath::Min(A.CoverScale, B.CoverScale);
		T.Puffs.RemoveAt(I + 1, EAllowShrinking::No);
	}
}

void FRTSMissileSmoke::Cuts()
{
	// (17/10, a v7) o contorno visível (Roberts 1923, usado por Gifford e por Frenkiel e Katz): a borda que se vê é onde a
	// coluna de fumaça passa de τ*, e τ* sobe com a idade. Por puff, a coluna no centro dele: a soma projetada no plano da vista
	// dos vizinhos do mesmo rastro (eles estão na ordem do caminho: anda pros dois lados até sair do alcance), e o corte relativo
	// τ*/τ da linha, que o material aplica no perfil do quadrado (cada quadrado corta no mesmo contorno absoluto). A fuligem
	// e as versões de antes ficam sem corte
	const FVector Fw = F.CamForward.GetSafeNormal();
	const bool bV7 = LookVersion >= 7;
	// (23/10) a mesma linha vista do sol (a direção do mapa do sol, com o piso de elevação das sombras das fumaças): o contorno
	// da sombra no chão, que se parte e some junto com o rastro (a regra de Roberts na linha de visada do sol)
	const FVector Ls = ShadowSunDir();
	ParallelFor(Trails.Num(), [&](int32 Ti)
	{
		FTrail& T = Trails[Ti];
		const int32 N = T.Puffs.Num();
		for (int32 I = 0; I < N; ++I)
		{
			FPuff& Pi = T.Puffs[I];
			if (!bV7 || Pi.Kind != 0)
			{
				Pi.Cut = 0.0f;
				Pi.CutSun = 0.0f;
				Pi.Axis = FVector::ZeroVector;
				continue;
			}
			// (17/10, a v7d) o rumo do rastro pelos vizinhos do caminho (a nuvem da largada não tem rumo)
			Pi.Axis = (T.Missile != 0 && N > 1) ? (T.Puffs[FMath::Min(I + 1, N - 1)].P - T.Puffs[FMath::Max(I - 1, 0)].P).GetSafeNormal() : FVector::ZeroVector;
			const double Reach = 4.0 * Pi.Sigma() + 2.0;
			float Line = 0.0f;
			float LineSun = 0.0f;
			for (const int32 Step : { -1, 1 })
			{
				for (int32 J = Step < 0 ? I : I + 1; J >= 0 && J < N; J += Step)
				{
					const FPuff& Pj = T.Puffs[J];
					const FVector D = Pj.P - Pi.P;
					if (D.SizeSquared() > Reach * Reach)
					{
						break;
					}
					if (Pj.Kind != 0)
					{
						continue;
					}
					const float Sj = Pj.Sigma();
					const float TauJ = Pj.Extinction * VisibleOf(Pj) / (2.0f * UE_PI * Sj * Sj);
					const FVector Dp = D - Fw * FVector::DotProduct(D, Fw);
					const float Q = float(Dp.SizeSquared()) / (Sj * Sj);
					if (Q < 9.0f)
					{
						Line += TauJ * FMath::Exp(-0.5f * Q);
					}
					const FVector Ds = D - Ls * FVector::DotProduct(D, Ls);
					const float Qs = float(Ds.SizeSquared()) / (Sj * Sj);
					if (Qs < 9.0f)
					{
						LineSun += TauJ * FMath::Exp(-0.5f * Qs);
					}
				}
			}
			Pi.Cut = (CutTau0 + CutTauRate * Pi.Age) * BudgetK / (FMath::Max(Line, 1e-4f) * FMath::Max(Pi.Fluct, 0.05f));
			Pi.CutSun = (CutTau0 + CutTauRate * Pi.Age) * BudgetK / (FMath::Max(LineSun, 1e-4f) * FMath::Max(Pi.Fluct, 0.05f));
		}
	});
}

void FRTSMissileSmoke::Light()
{
	// a sombra de um puff no outro: a extinção de todos numa grade (a célula de ≥ 1 m), e de cada puff uma marcha até o
	// sol e outra pra cima (o céu); T = e^(−τ) (Beer-Lambert). O próprio puff fica fora (a marcha sai a 1,2 σ): a sombra
	// dentro dele é a da esfera, no material
	FBox Box(EForceInit::ForceInit);
	int32 N = 0;
	for (const FTrail& T : Trails)
	{
		for (const FPuff& P : T.Puffs)
		{
			Box += P.P;
			++N;
		}
	}
	if (N == 0)
	{
		return;
	}
	Box = Box.ExpandBy(4.0);
	const FVector Ext = Box.GetSize();
	const float Cell = FMath::Max3(1.0f, float(FMath::Max(Ext.X, Ext.Y)) / 96.0f, float(Ext.Z) / 32.0f);
	const int32 Nx = FMath::Clamp(FMath::CeilToInt32(float(Ext.X) / Cell), 1, 96);
	const int32 Ny = FMath::Clamp(FMath::CeilToInt32(float(Ext.Y) / Cell), 1, 96);
	const int32 Nz = FMath::Clamp(FMath::CeilToInt32(float(Ext.Z) / Cell), 1, 32);
	TArray<float> Grid;
	Grid.SetNumZeroed(Nx * Ny * Nz);
	auto Index = [&](const FVector& P, int32& Out) -> bool
	{
		const FVector Q = (P - Box.Min) / Cell;
		const int32 X = FMath::FloorToInt32(float(Q.X));
		const int32 Y = FMath::FloorToInt32(float(Q.Y));
		const int32 Z = FMath::FloorToInt32(float(Q.Z));
		if (X < 0 || Y < 0 || Z < 0 || X >= Nx || Y >= Ny || Z >= Nz)
		{
			return false;
		}
		Out = (Z * Ny + Y) * Nx + X;
		return true;
	};
	const float CellVol = Cell * Cell * Cell;
	for (const FTrail& T : Trails)
	{
		for (const FPuff& P : T.Puffs)
		{
			int32 I;
			if (Index(P.P, I))
			{
				Grid[I] += P.Extinction * VisibleOf(P) * ShownOf(P) / CellVol;   // σt médio da célula (1/m); na v7 só o que se vê
			}
		}
	}
	// as luzes perto (as explosões, o fogo da carcaça, os tiros: a lista do BlastFx, RTSSceneLights), em todo puff a cada
	// quadro (o clarão dura um ou dois quadros): Σ I/d² com a janela da Unreal, (1 − (d/R)⁴)², e d ≥ 0,5 m, a mesma conta
	// que acende o chão (DeferredLightingCommon.ush); a direção é a média pesada pelo brilho. A sombra até a luz é a do
	// céu (o combinado no docs/ESTUDO_LUZ_NA_FUMACA.md), no material
	{
		const TArray<RTSSceneLights::FLight>& All = RTSSceneLights::Get_GameThread();
		TArray<const RTSSceneLights::FLight*, TInlineAllocator<RTSSceneLights::MaxLights>> Near;
		for (const RTSSceneLights::FLight& Lt : All)
		{
			const float R = Lt.RadiusM;
			if (R > 0.0f && Box.ComputeSquaredDistanceToPoint(Lt.PosM) < double(R) * R && !Lt.Candela.IsAlmostBlack())
			{
				Near.Add(&Lt);
			}
		}
		// (17/10) cada rastro numa thread; o registro do brilho depois, numa passada só
		const float LitLux = 0.01f * FMath::Max(SunLux.GetLuminance(), 1.0f);
		ParallelFor(Trails.Num(), [&](int32 Ti)
		{
			for (FPuff& P : Trails[Ti].Puffs)
			{
				FLinearColor E = FLinearColor::Black;
				FVector Wd = FVector::ZeroVector;
				for (const RTSSceneLights::FLight* Lt : Near)
				{
					const FVector D = Lt->PosM - P.P;
					const float D2Raw = float(D.SizeSquared());
					const float R2 = FMath::Max(Lt->RadiusM * Lt->RadiusM, 1e-4f);
					if (D2Raw >= R2)
					{
						continue;
					}
					const float D2 = FMath::Max(D2Raw, 0.25f);
					const float Win = FMath::Square(FMath::Clamp(1.0f - FMath::Square(D2Raw / R2), 0.0f, 1.0f));
					const FLinearColor Ei = Lt->Candela * (Win / D2);
					E += Ei;
					Wd += D * (Ei.GetLuminance() / FMath::Sqrt(FMath::Max(D2Raw, 1e-6f)));
				}
				P.LocE = E;
				P.LocDir = Wd.IsNearlyZero() ? FVector::UpVector : Wd.GetSafeNormal();
			}
		});
		if (Near.Num() > 0)
		{
			for (const FTrail& T : Trails)
			{
				for (const FPuff& P : T.Puffs)
				{
					const float Lum = P.LocE.GetLuminance();
					LocalMaxLux = FMath::Max(LocalMaxLux, Lum);
					LocalLitPuffs += Lum > LitLux ? 1 : 0;
				}
			}
		}
	}

	const float Blend = 0.5f;
	LightParity ^= 1;
	ParallelFor(Trails.Num(), [&](int32 Ti)
	{
		FTrail& T = Trails[Ti];
		for (int32 Pi = 0; Pi < T.Puffs.Num(); ++Pi)
		{
			FPuff& P = T.Puffs[Pi];
			if (((Pi + Ti) & 1) != LightParity && P.Age >= 0.1f)
			{
				continue;   // a outra metade no quadro que vem (a luz muda devagar)
			}
			const float S = P.Sigma();
			auto March = [&](const FVector& Dir, float MaxM) -> float
			{
				float Tau = 0.0f;
				const int32 Steps = FMath::Clamp(FMath::CeilToInt32(MaxM / Cell), 1, 24);
				const float Step = MaxM / float(Steps);
				FVector X = P.P + Dir * (1.2f * S + 0.5f * Step);
				for (int32 K = 0; K < Steps; ++K)
				{
					int32 I;
					if (Index(X, I))
					{
						Tau += Grid[I] * Step;
					}
					X += Dir * Step;
				}
				return FMath::Exp(-Tau);
			};
			const float Sun = March(SunDir, 24.0f);
			const float Sky = March(FVector::UpVector, 12.0f);
			const bool bNew = P.Age < 0.1f;
			P.SunT = bNew ? Sun : FMath::Lerp(P.SunT, Sun, Blend);
			P.SkyT = bNew ? Sky : FMath::Lerp(P.SkyT, Sky, Blend);
		}
	});
}

void FRTSMissileSmoke::Readability()
{
	// a regra do jogo: efeito que cobre unidade é defeito. A fumaça entre a câmera e uma unidade fica com no máximo
	// τ 0,5 (~40% de cobertura), mais quanto mais perto do raio da vista ela passa; o resto do rastro fica inteiro
	for (FTrail& T : Trails)
	{
		for (FPuff& P : T.Puffs)
		{
			P.CoverScale = 1.0f;
		}
	}
	const float FadeS = LookVersion >= 7 ? 1.0f : 3.0f;   // (17/10) o apagamento no fim da vida (na v7 só a trava)
	const FVector Cam = F.CamCm / 100.0;
	// a caixa de cada rastro (com a folga do maior sprite dele): o raio da vista que nem passa por ela pula o rastro inteiro
	// (17/10, a carga das 25 Patriots: ~50 unidades × ~3000 puffs era 3 ms; agora cada raio só olha os rastros no caminho,
	// e os raios rodam em paralelo; quem rareia é aplicado depois, numa passada só)
	TArray<FBox> Boxes;
	Boxes.SetNum(Trails.Num());
	FBox All(EForceInit::ForceInit);
	for (int32 Ti = 0; Ti < Trails.Num(); ++Ti)
	{
		FBox B(EForceInit::ForceInit);
		float MaxS = 0.0f;
		for (const FPuff& P : Trails[Ti].Puffs)
		{
			B += P.P;
			MaxS = FMath::Max(MaxS, P.Sigma());
		}
		Boxes[Ti] = B.IsValid ? B.ExpandBy(double(SpriteSigmas * MaxS + 2.5f)) : B;
		if (B.IsValid)
		{
			All += Boxes[Ti];
		}
	}
	if (!All.IsValid)
	{
		return;
	}
	const int32 NU = F.UnitTopsCm.Num();
	TArray<TArray<TPair<FPuff*, float>>> OnRays;
	OnRays.SetNum(NU);
	TArray<float> Sums;
	Sums.SetNumZeroed(NU);
	ParallelFor(NU, [&](int32 Ui)
	{
		const FVector Top = F.UnitTopsCm[Ui] / 100.0;
		const FVector Ray = Top - Cam;
		const double Dist = Ray.Size();
		if (Dist < 1.0 || !FMath::LineBoxIntersection(All, Cam, Top, Ray))
		{
			return;
		}
		const FVector Rd = Ray / Dist;
		// a soma da densidade óptica no raio da vista até a unidade (a coluna de cada gaussiana no ponto mais perto do raio,
		// com 1 m de folga pro tamanho da unidade); acima do limite, os puffs desse raio rareiam na proporção
		TArray<TPair<FPuff*, float>>& OnRay = OnRays[Ui];
		float Sum = 0.0f;
		for (int32 Ti = 0; Ti < Trails.Num(); ++Ti)
		{
			if (!Boxes[Ti].IsValid || !FMath::LineBoxIntersection(Boxes[Ti], Cam, Top, Ray))
			{
				continue;
			}
			for (FPuff& P : Trails[Ti].Puffs)
			{
				const double Tt = FVector::DotProduct(P.P - Cam, Rd);
				if (Tt < 0.0 || Tt > Dist - 0.5)
				{
					continue;
				}
				const float S = P.Sigma();
				const float Perp = FMath::Max(float((P.P - Cam - Rd * Tt).Size()) - 1.0f, 0.0f);
				if (Perp >= SpriteSigmas * S)
				{
					continue;
				}
				const float Fade = (1.0f - FMath::SmoothStep(MaxLifeS - FadeS, MaxLifeS, P.Age)) * ShownOf(P);
				const float Tau = P.Extinction * VisibleOf(P) / (2.0f * UE_PI * S * S) * Fade * FMath::Exp(-0.5f * FMath::Square(Perp / S));
				if (Tau > 1e-3f)
				{
					OnRay.Add(TPair<FPuff*, float>(&P, Tau));
					Sum += Tau;
				}
			}
		}
		Sums[Ui] = Sum;
	});
	for (int32 Ui = 0; Ui < NU; ++Ui)
	{
		if (Sums[Ui] > CoverTauMax)
		{
			const float Scale = CoverTauMax / Sums[Ui];
			for (const TPair<FPuff*, float>& It : OnRays[Ui])
			{
				It.Key->CoverScale = FMath::Min(It.Key->CoverScale, Scale);
			}
		}
	}
}

void FRTSMissileSmoke::Tick(float DeltaSeconds, const FRTSSmokeFrame& Frame)
{
	if (!bReady)
	{
		return;
	}
	const double T0 = FPlatformTime::Seconds();
	const float Dt = FMath::Clamp(DeltaSeconds, 0.0f, 0.1f);
	F = Frame;
	Clock += Dt;
	// (17/10) a versão do visual: na 6 o rastro vive 3/4 da vida do preset e engorda menos (o estudo do detalhe: os RTS fazem o
	// rastro fino e curto, 0,3–4 s; a compressão do ε engordava +30–45% aos 6–8 s)
	LookVersion = CVarSmokeLook.GetValueOnGameThread();
	if (BaseLifeS < 0.0f)
	{
		BaseLifeS = MaxLifeS;
	}
	// (17/10, a v7) a vida é só a trava (quem tira o rastro é o contorno) e a névoa seca mais e mais rápido
	const bool bV7 = LookVersion >= 7;
	MaxLifeS = bV7 ? FMath::Min(BaseLifeS, V7MaxLifeS) : BaseLifeS * (LookVersion >= 6 ? 0.75f : 1.0f);
	GSolidShare = bV7 ? V7SolidShare : SolidShare;
	GEvapTauS = bV7 ? V7EvapTauS : EvapTauS;
	for (UMaterialInstanceDynamic* M : { SmokeMID.Get() })
	{
		if (M)
		{
			M->SetScalarParameterValue(TEXT("LookV6"), LookVersion >= 6 ? 1.0f : 0.0f);
			M->SetVectorParameterValue(TEXT("WindCpu"), FLinearColor(float(WindDir.X), float(WindDir.Y), WindMs, 0.0f));
		}
	}
	for (UMaterialInstanceDynamic* M : { SmokeMID.Get(), ShadowMID.Get() })
	{
		if (M)
		{
			M->SetScalarParameterValue(TEXT("LookV7"), bV7 ? 1.0f : 0.0f);
		}
	}
	if (F.SimTick == LastShotTick)
	{
		F.Shots.Reset();                     // o mesmo passo da simulação: o túnel já foi aberto
	}
	LastShotTick = F.SimTick;

	// o vento e os modos da turbulência (refeitos se o vento do mapa mudar)
	WindUnit = FVector(WindDir.X, WindDir.Y, 0.0).GetSafeNormal();
	if (WindUnit.IsNearlyZero())
	{
		WindUnit = FVector(1.0, 0.0, 0.0);
	}
	UStar = Kappa * FMath::Max(WindMs, 0.5f) / FMath::Loge(ZRefM / Z0M);
	if (!FMath::IsNearlyEqual(ModesForWind, WindMs, 0.01f))
	{
		ModesForWind = WindMs;
		Modes.Reset();
		FRandomStream Rng(424242);
		// 7 oitavas de 30 m a ~1,7 m; a amplitude ∝ λ^⅓ (a cascata de Kolmogorov: u(ℓ) ∝ (εℓ)^⅓), normalizada pra
		// variância horizontal σu² (cada cosseno tem média ½ no quadrado)
		float SumL = 0.0f;
		float Lambdas[7];
		for (int32 K = 0; K < 7; ++K)
		{
			Lambdas[K] = 30.0f * FMath::Pow(0.62f, float(K));
			SumL += FMath::Pow(Lambdas[K], 2.0f / 3.0f);
		}
		const float SigmaU = SigmaUPerUStar * UStar;
		for (int32 K = 0; K < 7; ++K)
		{
			FMode M;
			const FVector Kh = Rng.GetUnitVector();
			M.K = Kh * (2.0f * UE_PI / Lambdas[K]);
			FVector A = FVector::CrossProduct(Kh, Rng.GetUnitVector()).GetSafeNormal();
			const float Amp = SigmaU * UE_SQRT_2 * FMath::Pow(Lambdas[K], 1.0f / 3.0f) / FMath::Sqrt(SumL);
			A *= Amp;
			A.Z *= SigmaWPerUStar / SigmaUPerUStar;   // o vertical é mais fraco junto do chão
			M.A = A;
			M.Omega = 2.0f * UE_PI * Amp / Lambdas[K];   // o tempo de giro do redemoinho
			M.Phase = Rng.FRandRange(0.0f, 2.0f * UE_PI);
			Modes.Add(M);
		}
	}
	// o sol: a luz direcional do mapa (o diretor do Look a põe), relida a cada segundo
	SunClock += Dt;
	if (SunClock > 1.0f)
	{
		SunClock = 0.0f;
		if (UWorld* World = WorldRef.Get())
		{
			float Best = -1.0f;
			UDirectionalLightComponent* Sun = nullptr;
			for (TActorIterator<ADirectionalLight> It(World); It; ++It)
			{
				if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(It->GetLightComponent()))
				{
					const float Score = (L->IsUsedAsAtmosphereSunLight() ? 1.0e9f : 0.0f) + L->Intensity;
					if (L->IsVisible() && Score > Best)
					{
						Best = Score;
						Sun = L;
					}
				}
			}
			if (Sun)
			{
				// a iluminância do sol no chão, como o motor manda pro translúcido: a cor × a intensidade e, se o sol é o da
				// atmosfera, a transmitância até o chão no alto do planeta (LightGridInjection.cpp)
				SunDir = -Sun->GetDirection();
				FLinearColor Lux = Sun->GetColoredLightBrightness();
				if (Sun->IsUsedAsAtmosphereSunLight())
				{
					for (TActorIterator<AActor> It(World); It; ++It)
					{
						if (USkyAtmosphereComponent* Atm = It->FindComponentByClass<USkyAtmosphereComponent>())
						{
							Lux *= Atm->GetAtmosphereTransmitanceOnGroundAtPlanetTop(Sun);
							break;
						}
					}
				}
				const bool bFirst = !bSunFound;
				SunLux = Lux;
				bSunFound = true;
				// (17/10) a luz do céu num chão plano, em lux: o motor monta a do céu como o SH da SkyLight convoluído com o
				// cosseno (TSHVector::CalcDiffuseTransfer) × a cor × a intensidade (View.SkyLightColor). O material da sombra no
				// chão multiplica a cena e não recebe o céu do motor (o preto e o magenta na sombra dos prédios, 17/10)
				SkyIrrLux = FLinearColor::Black;
				for (TActorIterator<ASkyLight> It(World); It; ++It)
				{
					USkyLightComponent* Sk = It->GetLightComponent();
					if (Sk && Sk->IsVisible())
					{
						const FLinearColor Irr = Dot(Sk->GetIrradianceEnvironmentMap(), FSHVector3::CalcDiffuseTransfer(FVector::UpVector));
						SkyIrrLux = Irr * Sk->GetLightColor() * Sk->Intensity;
						break;
					}
				}
				for (UMaterialInstanceDynamic* M : { SmokeMID.Get(), ShadowMID.Get() })
				{
					if (M)
					{
						M->SetVectorParameterValue(TEXT("SunDirCpu"), FLinearColor(float(SunDir.X), float(SunDir.Y), float(SunDir.Z), 1.0f));
						M->SetVectorParameterValue(TEXT("SunLuxCpu"), SunLux);
						M->SetVectorParameterValue(TEXT("SkyIrrCpu"), SkyIrrLux);
						M->SetScalarParameterValue(TEXT("Debug"), float(CVarSmokeDebug.GetValueOnGameThread()));
					}
				}
				if (bFirst)
				{
					UE_LOG(LogTemp, Log, TEXT("RTS fumaça do míssil: o sol pra (%.2f, %.2f, %.2f), elevação %.0f°, iluminância (%.0f, %.0f, %.0f) lux; o céu num chão plano (%.0f, %.0f, %.0f) lux"),
						SunDir.X, SunDir.Y, SunDir.Z, FMath::RadiansToDegrees(FMath::Asin(float(FMath::Clamp(SunDir.Z, -1.0, 1.0)))), SunLux.R, SunLux.G, SunLux.B,
						SkyIrrLux.R, SkyIrrLux.G, SkyIrrLux.B);
				}
			}
		}
	}

	// os mísseis: cada um solta os puffs pelo caminho desde o quadro de antes
	for (FTrail& T : Trails)
	{
		T.SinceSeen += Dt;
	}
	for (const FRTSSmokeMissile& M : F.Missiles)
	{
		const FVector Dir = M.Dir.GetSafeNormal();
		const FVector Nozzle = M.PosCm / 100.0 - Dir * NozzleBackM;
		FTrail* T = Trails.FindByPredicate([&M](const FTrail& X) { return X.bLive && X.Missile == M.Id; });
		if (!T)
		{
			T = &Trails.AddDefaulted_GetRef();
			T->Missile = M.Id;
			T->bLive = true;
			T->HeadM = Nozzle;
			T->HeadDir = Dir;
			T->HeadSpeed = M.SpeedCmS / 100.0f;
			T->Carry = EmitStepM;           // o primeiro puff na boca
		}
		T->SinceSeen = 0.0f;
		Emit(*T, Nozzle, Dir, M.SpeedCmS / 100.0f, M.AgeS);
		// o míssil fura o rastro dos outros da rajada (o choque dele): o trecho deste quadro vai pros tiros
		F.Shots.Add(TPair<FVector, FVector>(T->HeadM * 100.0 - Dir * 50.0, M.PosCm));
	}
	TickExhausts(Dt);
	TickSmolders(Dt);
	for (FTrail& T : Trails)
	{
		if (T.bLive && T.SinceSeen > 0.0f)
		{
			T.bLive = false;                 // acertou (ou entrou na névoa): o rastro fica e envelhece
		}
	}

	// os puffs: o ar (vento + redemoinhos + quem passa), o empuxo, o chão, o crescimento e a massa. (17/10, a carga das 25
	// Patriots: 14–16 ms na CPU com ~3000 puffs e ~100 mísseis no ar, cada puff testando todos os veículos, os trechos de
	// tiro e as explosões) cada rastro numa thread, só com quem encosta na caixa dele
	TurbParity ^= 1;
	const float Gain = bV7 ? 0.0f : (LookVersion >= 6 ? 2.0f : CompressGain);
	const float MaxLife = MaxLifeS;
	TArray<int32> Counts;
	Counts.SetNumZeroed(Trails.Num());
	ParallelFor(Trails.Num(), [&](int32 Ti)
	{
		FTrail& T = Trails[Ti];
		FNear Near;
		NearOf(T, Near);
		for (int32 I = 0; I < T.Puffs.Num(); ++I)
		{
			FPuff& P = T.Puffs[I];
			const float S = P.Sigma();
			if (((I + Ti) & 1) == TurbParity)
			{
				P.Turb = FVector::ZeroVector;      // este quadro recalcula (a outra metade no quadro que vem)
			}
			const FVector Air = AirVelocity(P.P, S, &P.Turb);
			// a térmica em linha (Briggs: z = 1,6 (B t²)^⅓, então w = ⅔·1,6 B^⅓ t^−⅓), quebrada pela turbulência depois de ~8 s;
			// o calor de uma explosão perto sobe junto
			const float Rise = (2.0f / 3.0f) * 1.6f * FMath::Pow(P.Buoy, 1.0f / 3.0f) * FMath::Pow(FMath::Max(P.Age, 0.05f), -1.0f / 3.0f)
				* (1.0f - FMath::SmoothStep(BuoyStopS - 2.0f, BuoyStopS + 2.0f, P.Age));
			const FVector Buoy(0.0, 0.0, Rise + 2.0f * P.Heat);
			P.V += (Air + Buoy - P.V) * (1.0f - FMath::Exp(-Dt / P.RelaxS));
			P.RelaxS = FMath::Min(P.RelaxS + Dt, RelaxTauS);   // o jato do escape freia e daí acompanha o ar como os outros
			ApplyMovers(P, S, Dt, Near);
			P.P += P.V * Dt;
			const float Ground = F.GroundZCm ? F.GroundZCm(P.P.X * 100.0, P.P.Y * 100.0) / 100.0f : 0.0f;
			P.GroundM = Ground;                    // (23/10) a sombra no mapa do sol: só a parte acima do chão
			const float Floor = Ground + 0.5f * S;
			if (P.P.Z < Floor)
			{
				P.P.Z = Floor;
				P.V.Z = FMath::Max(P.V.Z, 0.0);
				// no chão o puff abre pros lados (o jato de parede): a velocidade que descia vira horizontal
			}
			// a difusão relativa (a forma do puff; os redemoinhos maiores só o carregam, nos modos de Fourier): Richardson–
			// Obukhov, σ²′ = 2·0,64 ε^⅓ σ^(4/3), com ε = u*³/(κ z) da camada neutra e o teto 2 σw² tL; o jogo comprime o tempo
			// (o ε até 4× entre 6 e 12 s)
			const float Z = FMath::Clamp(float(P.P.Z) - Ground, 1.0f, 50.0f);
			// (17/10, a v7) sem a compressão do ε (o Gain 0): o rastro some pela borda, não engordando ("abre pros lados")
			const float Eps = UStar * UStar * UStar / (Kappa * Z) * (1.0f + Gain * FMath::SmoothStep(CompressFromS, CompressToS, P.Age));
			const float Rich = RichardsonC * FMath::Pow(Eps, 1.0f / 3.0f) * FMath::Pow(S, 4.0f / 3.0f);
			const float Cap = FMath::Square(SigmaWPerUStar * UStar) * LagrangianTimeS;
			P.S2 += 2.0f * FMath::Min(Rich, Cap) * Dt;
			P.Heat *= FMath::Exp(-Dt / 0.25f);
			P.Age += Dt;
		}
		T.Puffs.RemoveAll([MaxLife](const FPuff& P)
		{
			const float S = P.Sigma();
			const float Tau = P.Extinction * VisibleOf(P) / (2.0f * UE_PI * S * S);
			// (17/10, a v7) e o que já passou muito do contorno (o corte 3× acima do centro: não se vê e quase não pesa nos vizinhos);
			// (a revisão da vida) a esteira do pedaço em brasa aos 3τ (5% da massa, τ ~0,01 de lado a lado: não se vê; o custo, a 2ª revisão)
			return P.Age > MaxLife || (P.Age > 0.5f && Tau < MinVisibleTau) || (P.Age > 1.0f && P.Cut > 3.0f) || (P.FadeTauS > 0.0f && P.Age > 3.0f * P.FadeTauS);
		});
		Merge(T);
		Counts[Ti] = T.Puffs.Num();
	});
	int32 Total = 0;
	for (const int32 C : Counts)
	{
		Total += C;
	}
	Trails.RemoveAll([](const FTrail& T) { return !T.bLive && T.Puffs.Num() == 0; });
	// o teto: junta mais forte e, se ainda passar, tira o mais velho
	if (Total > MaxPuffs)
	{
		for (FTrail& T : Trails)
		{
			if (!T.bLive && T.Puffs.Num() > 8)
			{
				T.Puffs.RemoveAt(0, T.Puffs.Num() / 4);
			}
		}
	}
	// (17/10, a v7) o contorno visível e o deslocamento médio do que se vê (o ruído do material anda com ele)
	Cuts();
	{
		// o ponto que a câmera olha, no chão (o orçamento da tela conta a fumaça em volta dele)
		const FVector CamM = F.CamCm / 100.0;
		const float CamGround = F.GroundZCm ? F.GroundZCm(F.CamCm.X, F.CamCm.Y) / 100.0f : 0.0f;
		const double Down = -F.CamForward.Z;
		const FVector Look = Down > 0.05 ? CamM + F.CamForward * ((CamM.Z - CamGround) / Down) : CamM;
		FVector SumV = FVector::ZeroVector;
		double SumW = 0.0;
		double ViewMass = 0.0;
		for (const FTrail& T : Trails)
		{
			for (const FPuff& P : T.Puffs)
			{
				if (P.Kind == 0 && P.Age > 0.3f)
				{
					const double W = double(P.Extinction) * VisibleOf(P) * ShownOf(P);
					SumV += P.V * W;
					SumW += W;
					if (FVector::DistSquaredXY(P.P, Look) < double(ViewBudgetRadiusM) * ViewBudgetRadiusM)
					{
						ViewMass += W;
					}
				}
			}
		}
		if (SumW > 0.0)
		{
			NoiseOffM += SumV / SumW * Dt;
		}
		ViewMassM2 = float(ViewMass);
		// (a volta d, na raiz: 1,25× em média nas 25 Patriots, a base ainda coberta; agora linear, até 3×)
		const float WantK = bV7 ? FMath::Clamp(ViewMassM2 / ViewBudgetM2, 1.0f, 3.0f) : 1.0f;
		BudgetK += (WantK - BudgetK) * (1.0f - FMath::Exp(-Dt / 0.5f));
		if (UMaterialInstanceDynamic* M = SmokeMID.Get())
		{
			M->SetVectorParameterValue(TEXT("NoiseOffCpu"), FLinearColor(float(NoiseOffM.X), float(NoiseOffM.Y), float(NoiseOffM.Z), 0.0f));
		}
	}
	// (23/10) a sombra: os puffs pro mapa do sol (ou, sem ele, os quadrados de antes no BuildMesh)
	ExportShadow(Dt);
	// (17/10, o Fluxo e a Arte) a foto de todos os puffs num quadro (rts.MissileSmoke.Dump N), pra contar os tijolos do campo
	static bool bDumped = false;
	const float DumpAt = CVarSmokeDump.GetValueOnGameThread();
	if (DumpAt > 0.0f && !bDumped && Clock >= DumpAt)
	{
		bDumped = true;
		FString Csv = TEXT("trail,missile,x_m,y_m,z_m,sigma_m,extinction_m2,visible,cut,shown,age_s,heat,kind,axis_x,axis_y,axis_z,fluct,cut_sun,ground_m\n");
		int32 Rows = 0;
		for (int32 Ti = 0; Ti < Trails.Num(); ++Ti)
		{
			for (const FPuff& P : Trails[Ti].Puffs)
			{
				Csv += FString::Printf(TEXT("%d,%u,%.3f,%.3f,%.3f,%.4f,%.5f,%.4f,%.4f,%.4f,%.3f,%.3f,%d,%.3f,%.3f,%.3f,%.3f,%.4f,%.3f\n"), Ti, Trails[Ti].Missile,
					P.P.X, P.P.Y, P.P.Z, P.Sigma(), P.Extinction, VisibleOf(P), P.Cut, ShownOf(P), P.Age, P.Heat, int32(P.Kind), P.Axis.X, P.Axis.Y, P.Axis.Z, P.Fluct,
					P.CutSun, P.GroundM);
				++Rows;
			}
		}
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("captures"), TEXT("game"),
			FString::Printf(TEXT("missile_puffs_%.0fs.csv"), DumpAt)));
		FFileHelper::SaveStringToFile(Csv, *Path);
		UE_LOG(LogTemp, Log, TEXT("RTS fumaça do míssil: %d puffs de %d rastros gravados em %s"), Rows, Trails.Num(), *Path);
	}
	// (17/10) o registro fino (rts.MissileSmoke.Log 1): a massa que se vê a cada 0,25 s, pra medir o que sobra no tiro seguinte
	static float FineClock = 0.0f;
	FineClock += Dt;
	if (CVarSmokeLog.GetValueOnGameThread() > 0 && FineClock >= 0.25f)
	{
		FineClock = 0.0f;
		int32 Shown = 0, All = 0;
		double Mass = 0.0;
		for (const FTrail& T : Trails)
		{
			for (const FPuff& P : T.Puffs)
			{
				if (P.Kind != 0)
				{
					continue;
				}
				++All;
				const float Fd = 1.0f - FMath::SmoothStep(MaxLifeS - (bV7 ? 1.0f : 3.0f), MaxLifeS, P.Age);
				const float Vis = VisibleOf(P) * ShownOf(P) * Fd;
				Shown += Vis > 0.0f ? 1 : 0;
				Mass += P.Extinction * Vis;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("RTS fumaça fina: t %.2f s, v%d, %d mísseis, %d puffs (%d à vista), massa visível %.3f m² (perto da câmera %.0f, orçamento ×%.2f)"),
			Clock, LookVersion, F.Missiles.Num(), All, Shown, Mass, ViewMassM2, BudgetK);
	}
	for (FBlast& B : Blasts)
	{
		B.Age += Dt;
	}
	// (23/10) a subida da nuvem viva da ogiva dura mais (3 τ); a de sempre (τ 1,5 s) sai aos 4 s, como antes
	Blasts.RemoveAll([](const FBlast& B) { return B.Age > (B.RiseTau > 1.5f ? 3.0f * B.RiseTau : 4.0f); });
	// as explosões novas já aplicaram o sopro (Age 0 no ApplyMovers); daqui pra frente só a volta

	const double T1 = FPlatformTime::Seconds();
	Light();
	const double T2 = FPlatformTime::Seconds();
	Readability();
	const double T3 = FPlatformTime::Seconds();
	BuildMesh();
	const double T4 = FPlatformTime::Seconds();
	LastCostMs = (T4 - T0) * 1000.0;
	CostSplit[0] = float((T1 - T0) * 1000.0);
	CostSplit[1] = float((T2 - T1) * 1000.0);
	CostSplit[2] = float((T3 - T2) * 1000.0);
	CostSplit[3] = float((T4 - T3) * 1000.0);
	LogClock += Dt;
	if (LogClock > 5.0f && Trails.Num() > 0)
	{
		LogClock = 0.0f;
		int32 Soot = 0;
		float SootExtMax = 0.0f, SootTauMax = 0.0f, SootTauShownMax = 0.0f, SootCoverMin = 1.0f, SootSigmaMin = 1e9f;
		for (const FTrail& T : Trails)
		{
			for (const FPuff& P : T.Puffs)
			{
				if (P.Kind != 1)
				{
					continue;
				}
				++Soot;
				const float S = P.Sigma();
				const float Tau = P.Extinction * VisibleOf(P) / (2.0f * UE_PI * S * S);
				SootExtMax = FMath::Max(SootExtMax, P.Extinction);
				SootTauMax = FMath::Max(SootTauMax, Tau);
				SootTauShownMax = FMath::Max(SootTauShownMax, Tau * P.CoverScale);
				SootCoverMin = FMath::Min(SootCoverMin, P.CoverScale);
				SootSigmaMin = FMath::Min(SootSigmaMin, S);
			}
		}
		if (Soot > 0)
		{
			// (16/10) a depuração do diesel: a fuligem (a extinção, a densidade antes e depois da leitura) e um motor
			const FEngine* E0 = Engines.Num() ? &Engines.CreateConstIterator().Value() : nullptr;
			UE_LOG(LogTemp, Log, TEXT("RTS diesel: fuligem extinção até %.3f m², τ até %.2f (mostrado %.2f), leitura mín %.2f, σ mín %.2f m; um motor: v %.1f m/s, U %.1f, acelerador %.2f, turbo %.2f, marcha %d"),
				SootExtMax, SootTauMax, SootTauShownMax, SootCoverMin, SootSigmaMin, E0 ? E0->Speed : 0.0f, E0 ? E0->U : 0.0f, E0 ? E0->Throttle : 0.0f, E0 ? E0->Boost : 0.0f, E0 ? E0->Gear : 0);
		}
		const int32 ShadowPath = CVarSmokeShadowPath.GetValueOnGameThread();
		UE_LOG(LogTemp, Log, TEXT("RTS fumaça do míssil: %d puffs em %d rastros (%d de fuligem, %d motores), %.2f ms na CPU (simulação %.2f, luz %.2f, leitura %.2f, malha %.2f; vida %.0f s); luzes perto: até %.0f lux, %d puffs-quadro acima de 1%% do sol; sombra %s, %d puffs no mapa"),
			NumPuffs(), Trails.Num(), Soot, Engines.Num(), LastCostMs, CostSplit[0], CostSplit[1], CostSplit[2], CostSplit[3], MaxLifeS, LocalMaxLux, LocalLitPuffs,
			ShadowPath <= 0 ? TEXT("desligada") : (bShadowQuads ? (ShadowPath >= 2 ? TEXT("nos quadrados (sem o mapa do sol)") : TEXT("nos quadrados")) : TEXT("no mapa do sol")), ShadowExported);
		LocalMaxLux = 0.0f;
		LocalLitPuffs = 0;
	}
}

void FRTSMissileSmoke::BuildMesh()
{
	UProceduralMeshComponent* SmokeMesh = Mesh.Get();
	UProceduralMeshComponent* GroundMesh = ShadowMesh.Get();
	if (!SmokeMesh || !GroundMesh)
	{
		return;
	}
	struct FItem
	{
		const FPuff* P;
		double Depth;
	};
	TArray<FItem> Items;
	const bool bV7 = LookVersion >= 7;
	const float FadeS = bV7 ? 1.0f : 3.0f;
	for (const FTrail& T : Trails)
	{
		for (const FPuff& P : T.Puffs)
		{
			if (P.Cut >= 1.0f)
			{
				continue;   // (17/10, a v7) o centro já está abaixo do contorno: não se vê (só pesa na coluna dos vizinhos)
			}
			Items.Add({ &P, FVector::DotProduct(P.P * 100.0 - F.CamCm, F.CamForward) });
		}
	}
	const int32 N = Items.Num();
	// (23/10) os quadrados da sombra no chão só na ShadowPath 1 (o A/B) ou sem o mapa do sol; senão a sombra vai pelo mapa
	const bool bQuads = bShadowQuads;
	SmokeMesh->SetVisibility(N > 0);
	GroundMesh->SetVisibility(N > 0 && bQuads);
	if (N == 0)
	{
		return;
	}
	// de trás pra frente (o translúcido compõe na ordem)
	Items.Sort([](const FItem& A, const FItem& B) { return A.Depth > B.Depth; });

	const bool bGrow = N > Capacity;
	if (bGrow)
	{
		Capacity = FMath::DivideAndRoundUp(N, 256) * 256;
		Tris.SetNumUninitialized(Capacity * 6);
		for (int32 Q = 0; Q < Capacity; ++Q)
		{
			const int32 B = 4 * Q;
			Tris[6 * Q + 0] = B + 0;
			Tris[6 * Q + 1] = B + 2;
			Tris[6 * Q + 2] = B + 1;
			Tris[6 * Q + 3] = B + 0;
			Tris[6 * Q + 4] = B + 3;
			Tris[6 * Q + 5] = B + 2;
		}
	}
	// a sombra: só os puffs que sombreiam de verdade (τ > 0,04 e com mais de 0,15 s: o miolo branco perto da chama é pequeno)
	int32 NS = 0;
	for (const FItem& It : Items)
	{
		const float S = It.P->Sigma();
		const float Tau = It.P->Extinction * VisibleOf(*It.P) * ShownOf(*It.P) / (2.0f * UE_PI * S * S);
		NS += (bQuads && Tau > 0.04f && It.P->Age > 0.15f) ? 1 : 0;
	}
	const bool bGrowShadow = bQuads && (NS > ShadowCapacity || ShadowCapacity == 0);
	if (bGrowShadow)
	{
		ShadowCapacity = FMath::Max(FMath::DivideAndRoundUp(NS, 256) * 256, 256);
		ShadowTris.SetNumUninitialized(ShadowCapacity * 6);
		for (int32 Q = 0; Q < ShadowCapacity; ++Q)
		{
			const int32 B = 4 * Q;
			ShadowTris[6 * Q + 0] = B + 0;
			ShadowTris[6 * Q + 1] = B + 2;
			ShadowTris[6 * Q + 2] = B + 1;
			ShadowTris[6 * Q + 3] = B + 0;
			ShadowTris[6 * Q + 4] = B + 3;
			ShadowTris[6 * Q + 5] = B + 2;
		}
	}
	const int32 NV = 4 * Capacity;
	const int32 NSV = 4 * ShadowCapacity;
	Verts.SetNumUninitialized(NV);
	ShadowVerts.SetNumUninitialized(NSV);
	Normals.SetNumUninitialized(NV);
	ShadowNormals.SetNumUninitialized(NSV);
	UV0.SetNumUninitialized(NV);
	UV1.SetNumUninitialized(NV);
	UV2.SetNumUninitialized(NV);
	UV3.SetNumUninitialized(NV);
	SUV0.SetNumUninitialized(NSV);
	SUV1.SetNumUninitialized(NSV);
	SUV2.SetNumUninitialized(NSV);
	SUV3.SetNumUninitialized(NSV);
	Tangents.SetNumUninitialized(NV);
	ShadowTangents.SetNumUninitialized(NSV);
	Colors.SetNumUninitialized(bLocalLightsMat ? NV : 0);
	int32 SQ = 0;                              // o próximo quad da sombra

	const FVector R = F.CamRight;
	const FVector U = F.CamUp;
	const FVector ToCam = -F.CamForward;
	// (16/10) o mapa do sol comum (a Arte): as duas texturas e a grade. A posição vai em coordenada absoluta (o material
	// usa a posição do pixel no mundo, sem o atraso de um quadro da câmera): CamToMap com a câmera na origem = − a origem
	{
		URTSSunShadowSubsystem* Sun = WorldRef.IsValid() ? WorldRef->GetSubsystem<URTSSunShadowSubsystem>() : nullptr;
		const bool bSunMap = Sun && Sun->IsReady() && !Sun->HasGivenUp() && Sun->GetMapTexture();
		FLinearColor ToMap(0.0f, 0.0f, 0.0f, 0.0f), Axis(1.0f, 0.0f, 0.0f, 0.0f), Window(0.0f, 0.0f, 0.0f, 0.0f);
		float TanE = 0.0f;
		if (bSunMap)
		{
			Sun->GetMaterialParams(FVector::ZeroVector, ToMap, Axis, Window, TanE);
		}
		for (UMaterialInstanceDynamic* M : { SmokeMID.Get(), ShadowMID.Get() })
		{
			if (!M)
			{
				continue;
			}
			M->SetScalarParameterValue(TEXT("SunValid"), bSunMap ? 1.0f : 0.0f);
			if (bSunMap)
			{
				M->SetTextureParameterValue(TEXT("SunHMap"), Sun->GetMapTexture());
				if (Sun->GetVehicleTexture())
				{
					M->SetTextureParameterValue(TEXT("SunTMap"), Sun->GetVehicleTexture());
				}
				M->SetVectorParameterValue(TEXT("SunToMap"), ToMap);
				M->SetVectorParameterValue(TEXT("SunAxis"), Axis);
				M->SetVectorParameterValue(TEXT("SunWindow"), Window);
				M->SetScalarParameterValue(TEXT("SunTanE"), TanE);
				M->SetScalarParameterValue(TEXT("SunMapTexels"), Axis.A);
			}
		}
	}
	if (bLocalLightsMat)
	{
		// o material novo monta a esfera com a vista daqui (a normal do vértice leva a direção das luzes perto)
		if (UMaterialInstanceDynamic* M = SmokeMID.Get())
		{
			M->SetVectorParameterValue(TEXT("CamRightCpu"), FLinearColor(float(R.X), float(R.Y), float(R.Z), 0.0f));
			M->SetVectorParameterValue(TEXT("CamToCpu"), FLinearColor(float(ToCam.X), float(ToCam.Y), float(ToCam.Z), 0.0f));
		}
	}
	// a sombra no chão: a projeção do puff pela direção do sol, esticada 1/sen(elevação) no rumo do sol
	const FVector L = SunDir.GetSafeNormal();
	const float SinElev = FMath::Max(float(L.Z), 0.12f);
	FVector Az(L.X, L.Y, 0.0);
	Az = Az.IsNearlyZero() ? FVector(1.0, 0.0, 0.0) : Az.GetSafeNormal();
	const FVector Side(-Az.Y, Az.X, 0.0);
	const FVector2D Corner[4] = { FVector2D(-1, -1), FVector2D(1, -1), FVector2D(1, 1), FVector2D(-1, 1) };
	// (17/10, a carga das 25 Patriots: a malha era ~2 ms) a vaga de cada sombra antes, e os quads em paralelo
	TArray<int32> ShadowSlot;
	ShadowSlot.SetNumUninitialized(N);
	for (int32 Q = 0; Q < N; ++Q)
	{
		const FPuff& P = *Items[Q].P;
		const float S = P.Sigma();
		const float Tau = P.Extinction * VisibleOf(P) * ShownOf(P) / (2.0f * UE_PI * S * S);
		const bool bShadow = bQuads && Tau > 0.04f && P.Age > 0.15f && SQ < ShadowCapacity;
		ShadowSlot[Q] = bShadow ? SQ++ : -1;
	}
	ParallelFor(Capacity, [&](int32 Q)
	{
		const int32 B = 4 * Q;
		if (Q >= N)
		{
			// sobra: os 4 cantos num ponto só (área zero), no primeiro puff pra não inchar os limites da malha
			const FVector C = Items[0].P->P * 100.0;
			for (int32 K = 0; K < 4; ++K)
			{
				Verts[B + K] = C;
				Normals[B + K] = ToCam;
				UV0[B + K] = Corner[K];
				UV1[B + K] = FVector2D(0.0, 0.0);
				UV2[B + K] = FVector2D(1.0, 1.0);
				UV3[B + K] = FVector2D(0.0, 0.0);
				Tangents[B + K] = FProcMeshTangent(R, false);
				if (bLocalLightsMat)
				{
					Colors[B + K] = FColor(0, 0, 0, 0);
				}
			}
			return;
		}
		const FPuff& P = *Items[Q].P;
		const float S = P.Sigma();
		const float Fade = 1.0f - FMath::SmoothStep(MaxLifeS - FadeS, MaxLifeS, P.Age);
		const float TauC = P.Extinction * VisibleOf(P) / (2.0f * UE_PI * S * S) * Fade;
		// (17/10, a v7) o material corta pelo contorno: a semente dá lugar ao corte relativo (a v7 lê o ruído no mundo)
		const float UvY = bV7 ? P.Cut : P.Seed;
		// o gás ainda incandescente atrás da chama; a explosão perto agora acende pela luz dela (as luzes perto), e o calor
		// só brilha por conta própria no material velho
		// a fuligem marca o tipo com a "chama" negativa (o material lê o albedo e a fase dela)
		const float Glow = P.Kind == 1 ? -1.0f : FMath::Exp(-P.Age / GlowTauS) + (bLocalLightsMat ? 0.0f : P.Heat);
		const FColor LocColor = bLocalLightsMat ? EncodeRGBE(P.LocE) : FColor(0, 0, 0, 0);
		const double H = SpriteSigmas * S * 100.0;
		const FVector C = P.P * 100.0;
		// a sombra: a pegada no chão (o ponto que o puff tapa, pela direção do sol), com folga pras dunas; o material pega o
		// ponto da cena atrás de cada pixel (a profundidade) e a gaussiana do puff no raio do sol, então ela cai certa no
		// chão de verdade e escurece o tanque que passa por baixo
		const float G0 = F.GroundZCm ? F.GroundZCm(C.X, C.Y) : 0.0f;
		const double Up = FMath::Max(C.Z - G0, 0.0);
		const FVector Cg(C.X - L.X * (Up / SinElev), C.Y - L.Y * (Up / SinElev), G0);
		const double Hs = 1.3 * H / SinElev + 150.0;
		const double Hw = 1.3 * H + 100.0;
		const bool bShadow = ShadowSlot[Q] >= 0;
		const int32 SB = 4 * FMath::Max(ShadowSlot[Q], 0);
		for (int32 K = 0; K < 4; ++K)
		{
			const FVector2D Cr = Corner[K];
			Verts[B + K] = C + (R * Cr.X + U * Cr.Y) * H;
			Normals[B + K] = bLocalLightsMat ? P.LocDir : ToCam;
			if (bLocalLightsMat)
			{
				Colors[B + K] = LocColor;
			}
			// (17/10, a v7d) a tangente leva o rumo do rastro (o material novo monta a vista pelos parâmetros e lê a tangente
			// como o eixo do tubo); sem rumo, a vista (projetada na tela some: o puff fica esfera)
			Tangents[B + K] = FProcMeshTangent(!bLocalLightsMat ? R : (P.Axis.IsNearlyZero() ? ToCam : P.Axis), false);
			UV0[B + K] = Cr;
			UV1[B + K] = FVector2D(TauC * P.CoverScale, UvY);
			UV2[B + K] = FVector2D(P.SunT, P.SkyT);
			UV3[B + K] = FVector2D(Glow, P.Age);
			if (bShadow)
			{
				const FVector Sg = Cg + Az * (Cr.X * Hs) + Side * (Cr.Y * Hw);
				const FVector Off = C - Sg;          // do canto até o centro do puff (cm; interpolado, dá o centro em todo pixel)
				ShadowVerts[SB + K] = Sg;
				ShadowNormals[SB + K] = FVector::UpVector;
				ShadowTangents[SB + K] = FProcMeshTangent(Az, false);
				SUV0[SB + K] = Cr;
				SUV1[SB + K] = FVector2D(TauC, UvY);
				SUV2[SB + K] = FVector2D(Off.X, Off.Y);
				SUV3[SB + K] = FVector2D(Off.Z, S * 100.0f);
			}
		}
	});
	// as sobras da sombra: área zero, τ 0 (o material devolve 1)
	const FVector Rest = Items[0].P->P * 100.0;
	for (int32 V = 4 * SQ; bQuads && V < NSV; ++V)
	{
		ShadowVerts[V] = Rest;
		ShadowNormals[V] = FVector::UpVector;
		ShadowTangents[V] = FProcMeshTangent(Az, false);
		SUV0[V] = Corner[V & 3];
		SUV1[V] = FVector2D(0.0, 0.0);
		SUV2[V] = FVector2D(0.0, 0.0);
		SUV3[V] = FVector2D(0.0, 1.0);
	}
	static const TArray<FColor> NoColors;
	if (bGrow)
	{
		SmokeMesh->CreateMeshSection(0, Verts, Tris, Normals, UV0, UV1, UV2, UV3, bLocalLightsMat ? Colors : NoColors, Tangents, false);
		if (UMaterialInstanceDynamic* M = SmokeMID.Get())
		{
			SmokeMesh->SetMaterial(0, M);
		}
	}
	else
	{
		SmokeMesh->UpdateMeshSection(0, Verts, Normals, UV0, UV1, UV2, UV3, bLocalLightsMat ? Colors : NoColors, Tangents);
	}
	if (bGrowShadow)
	{
		GroundMesh->CreateMeshSection(0, ShadowVerts, ShadowTris, ShadowNormals, SUV0, SUV1, SUV2, SUV3, NoColors, ShadowTangents, false);
		if (UMaterialInstanceDynamic* M = ShadowMID.Get())
		{
			GroundMesh->SetMaterial(0, M);
		}
	}
	else if (bQuads)
	{
		GroundMesh->UpdateMeshSection(0, ShadowVerts, ShadowNormals, SUV0, SUV1, SUV2, SUV3, NoColors, ShadowTangents);
	}
}

FVector FRTSMissileSmoke::ShadowSunDir() const
{
	FVector Ls;
	if (RTSSmokeShadow::GetMap_GameThread(Ls) && Ls.Z > 0.0)
	{
		return Ls.GetSafeNormal();
	}
	// sem o mapa: o sol do mapa com o mesmo piso de elevação que o BlastFx põe nas sombras das fumaças (UBlastRaymarchSubsystem::
	// SubmitShadow, -RmShadowMinElev=, 20° por padrão)
	static const double MinElevDeg = [] { float V = 20.0f; FParse::Value(FCommandLine::Get(), TEXT("RmShadowMinElev="), V); return double(V); }();
	const double MinZ = FMath::Sin(FMath::DegreesToRadians(MinElevDeg));
	FVector S = SunDir.GetSafeNormal();
	if (S.Z > 0.0 && S.Z < MinZ)
	{
		const FVector2D Across = FVector2D(S.X, S.Y).GetSafeNormal() * FMath::Sqrt(1.0 - MinZ * MinZ);
		S = FVector(Across.X, Across.Y, MinZ);
	}
	return S.IsNearlyZero() ? FVector::UpVector : S;
}

void FRTSMissileSmoke::ExportShadow(float Dt)
{
	// (23/10; o Felipe: "as fumaças do Tomahawk, do Patriot e do carrinho não projetam sombras bonitas e reais") a sombra do rastro
	// no mapa do sol comum (RTSSmokeShadow; docs/FUMACA_DO_MISSIL.md, "A sombra no mapa do sol"): por puff, a coluna no centro como
	// o sprite a desenha (sem o CoverScale, que é a leitura câmera → unidade e não a física), o corte do contorno visto do sol e o
	// chão debaixo; o render soma a gaussiana 2D de cada um ao mapa das explosões e da poeira, e a light function tira só o sol
	// direto (no chão, nas unidades e nos prédios, sem escurecer de novo a sombra de um prédio). Os quadrados de antes (a sombra
	// por modulação, que escurecia também o céu e a névoa e empilhava ~70 M px por quadro nas 25 Patriots) ficam na ShadowPath 1
	const int32 Path = CVarSmokeShadowPath.GetValueOnGameThread();
	const int32 Total = NumPuffs();
	FVector MapSun;
	if (RTSSmokeShadow::GetMap_GameThread(MapSun))
	{
		MapMissS = 0.0f;
	}
	else if (Total > 0)
	{
		MapMissS += Dt;
	}
	// sem o mapa vivo em 1 s com puffs (o ambiente do BlastLab, -Raymarch=0, -RmShadow=0, outro dono da light function do sol):
	// os quadrados de antes. Os puffs continuam indo pro registro: o mapa pode nascer depois (o BlastFx só roda com algo vivo)
	bShadowQuads = Path == 1 || (Path >= 2 && MapMissS >= 1.0f);
	TArray<RTSSmokeShadow::FPuff> Out;
	if (Path >= 2 && Total > 0)
	{
		const bool bV7 = LookVersion >= 7;
		const float FadeS = bV7 ? 1.0f : 3.0f;
		const float Gain = CVarSmokeDebug.GetValueOnGameThread() == 2 ? 4.0f : 1.0f;   // a depuração 2: 4× mais escura, pra achar
		Out.Reserve(Total);
		for (const FTrail& T : Trails)
		{
			for (const FPuff& P : T.Puffs)
			{
				if (P.CutSun >= 1.0f)
				{
					continue;           // abaixo do contorno visto do sol: não faz sombra
				}
				const float S = P.Sigma();
				const float Fade = 1.0f - FMath::SmoothStep(MaxLifeS - FadeS, MaxLifeS, P.Age);
				const float TauC = P.Extinction * VisibleOf(P) / (2.0f * UE_PI * S * S) * Fade * Gain;
				if (TauC < 0.001f)
				{
					continue;           // o ε do mapa (RTSSmokeShadow.cpp): nem o centro passa dele
				}
				RTSSmokeShadow::FPuff& O = Out.AddDefaulted_GetRef();
				O.PosM = P.P;
				O.SigmaM = S;
				O.TauC = TauC;
				O.Cut = P.CutSun;
				O.GroundZM = P.GroundM;
				O.Axis = FVector3f(P.Axis);
			}
		}
	}
	// a lista vazia vai uma vez (o mapa fica sem o rastro); depois disso não precisa mandar de novo
	if (Out.Num() > 0 || ShadowExported > 0)
	{
		ShadowExported = Out.Num();
		RTSSmokeShadow::SetPuffs_GameThread(MoveTemp(Out));
	}
}
