#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/Float16.h"
#include "Math/Float16Color.h"
#include "RTSTreadDust.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneCaptureComponent2D;
class UTexture2D;
class UTextureRenderTarget2D;

/**
 * Um veículo que levanta poeira (o contrato da ponte com o jogo, docs/POEIRA_DAS_LAGARTAS.md):
 * - a ponte manda TODOS os veículos perto da câmera a cada quadro, os parados também (Speed 0): o parado não solta poeira,
 *   mas continua fazendo sombra nela (sem ele na lista, a poeira na sombra dele acenderia num quadro);
 * - Id estável; Speed com sinal (negativa de ré); YawRate = a derivada do yaw da Unreal em rad/s (positivo virando pra +Y);
 * - Sand: o peso da areia (1 − max(asfalto, molhado) da grade do WorldForge, FRTSBakedMap::SurfaceAt) no pé de trás
 *   (x −2,40 m) e na frente (x +2,55 m) de cada lagarta; quem nasce no meio (a cortina) interpola entre os dois;
 * - bVisible: falso sob a névoa de guerra (a poeira do inimigo escondido não aparece, como no Zero Hour).
 */
USTRUCT(BlueprintType)
struct RTSTREADDUST_API FRTSTreadSource
{
	GENERATED_BODY()
	int32 Id = 0;
	FVector PosM = FVector::ZeroVector;        // m: o centro do veículo no chão
	FVector FwdM = FVector(1, 0, 0);           // a frente, no chão
	double Speed = 0.0;                        // m/s (negativa de ré)
	double YawRate = 0.0;                      // rad/s: d(yaw)/dt da Unreal (no giro no lugar cada lagarta anda ±ω·1,425)
	double Sand[2] = { 1.0, 1.0 };             // pé de trás: [0] em −R (+Y local, a direita na Unreal), [1] em +R
	double SandFront[2] = { 1.0, 1.0 };        // frente: [0] em −R (+Y local, a direita na Unreal), [1] em +R
	bool bVisible = true;
	// as medidas do veículo (m, m²), pro ar que ele empurra na poeira dos OUTROS (16/10); o padrão é o tanque USA, as
	// mesmas duas caixas da sombra do tanque (SunVisAt): meio comprimento, altura e área frontal (3,76×1,55 + 3,0×0,93)
	double HalfLenM = 3.95, HeightM = 2.48, FrontalM2 = 8.62;
	// a sombra na poeira por pixel (16/10, o modo 2): a meia largura e o topo do casco (m) e a frente da torre (zero =
	// a do casco). As caixas são as do tanque USA na escala de cada veículo
	double HalfWidthM = 1.88, HullTopM = 1.55;
	FVector TurretFwdM = FVector::ZeroVector;
	// falso: só faz sombra e empurra o ar (o veículo de roda, a carcaça), não levanta poeira
	bool bEmitsDust = true;
};

/**
 * Uma explosão que empurra a poeira (16/10): o jogo chama onde chama o BlastFx, com a escala que ele desenha. O ar sai
 * em r/340 s e anda por 0,15 s (o pulso aprovado do BlastLab), radial e no chão; dentro da bola desenhada não empurra.
 */
USTRUCT(BlueprintType)
struct RTSTREADDUST_API FRTSDustBlast
{
	GENERATED_BODY()
	FVector PosM = FVector::ZeroVector;        // m: a detonação, no chão
	double Scale = 0.25;                       // a escala desenhada pelo BlastFx (1 = a Carga Pesada do BlastLab)
	FVector DirM = FVector::ZeroVector;        // a boca do canhão: a direção do cano no chão; zero = pra todo lado
	double HalfConeDeg = 180.0;                // a boca: 60 (EXPLOSOES.md §5)
	bool bVisible = true;                      // falso sob a névoa de guerra: não empurra (não entrega a posição)
};

/** Uma camada da poeira (o jato K1, a nuvem K2, a cortina C): os mesmos campos do FXLab (FFXTreadLayer). */
struct FRTSTreadLayer
{
	FString Name;
	double XMin = -3.84, XMax = -3.48, YOff = 1.425, YJit = 0.16, Z0 = 0.0;
	double Spacing = 0.25, SpacingJit = 1.0, Life = 0.35, LifeJit = 0.0, MaskDense = 0.0, MaskEnd = 0.0;
	double BakeStart = 0.0, BakeStartJit = 0.0, BakeRate = 1.0, Scale = 1.0, ScaleJit = 0.15, GrowTo = 1.0;
	double Alpha = 0.8, Attack = 0.05, Decay = 0.5, Inherit = 0.3, Drag = 1.6, Kick = 0.0, Side = 0.0, Up = 0.0, Sink = 0.0;
	double WakeRise = 0.0, WakePull = 0.0, WakeGrow = 0.0, WakeLen = 1.0;
	double Orient = 0.0, Flip = 0.0, RotMax = 0.17, PivotBottom = 1.0, CamOffset = 0.0, Erode = 0.2;
	double V0 = 1.88, V1 = 2.5, FollowLoad = 0.0, LoadGain = 1.0, Shadow = 0.0, BaseShade = 1.0, ScaleSlow = 1.0, VFull = 7.9;
	// sem rosário (o passo 4 do redesenho): desligado por padrão
	// a base escura pela altura acima do chão (m), igual pra todos os cartões; 0 = a base de cada cartão (os anéis escuros)
	double BaseShadeWorld = 0.0;
	// a luz no volume inteiro (a 2ª opinião de 16/10): o lado do rastro virado pro sol mais claro, o outro mais escuro
	// (0 = desligado; 0,4 = o lado de lá recebe 60 % do sol), na escala da nuvem e não de cada bolo; SunSideWidth em m
	double SunSide = 0.0, SunSideWidth = 3.0;
	double SpacingPoisson = 0.0, BakeJitFrames = 0.0, ScaleLogMin = 0.0, ScaleLogMax = 0.0, RotFreeAfter = 0.0, ErodeFromBirth = 0.0;
	// o sumiço (16/10, o Felipe: "ela vira 90 graus pra baixo e abaixa a opacidade pra 0... não se desfaz no céu bonito
	// como o BlastLab"): no trecho em que a máscara apaga (maskDense → maskEnd), a nuvem se desfaz em vez de só ficar
	// transparente: a erosão do alfa (as partes finas somem primeiro, a borda vira fiapo), o crescimento (espalha) e a
	// subida (m). Tudo pelo mesmo andamento da máscara; 0 = desligado
	double ErodeFade = 0.0, GrowFade = 0.0, RiseFade = 0.0;
	// o sumiço pela densidade (o estudo de 16/10, como a fumaça do WebGL e do BlastLab): no trecho da máscara a
	// profundidade óptica do cartão cai como e^(−FadeTau·m)/g² (g = o crescimento: crescer é diluir) e a opacidade só cai
	// por igual no fim (de FadeAlphaFrom a 1 do andamento m), pra limpar o resto. FadeTau 0 = o sumiço antigo (A × (1 − m))
	double FadeTau = 0.0, FadeAlphaFrom = 0.85;
	// a erosão pelo ruído do sumiço (no material: E·(0,5 − 0,5n + 0,1h) na profundidade óptica), E = ErodeFade·m^2,5
};

/** O perfil (Resources/tread.json: a cópia do FXLab/tread_*.json escolhido, pack_tread_dust.py com FXLAB_TREAD_PROFILE). */
struct FRTSTreadProfile
{
	FString Bake;
	double SoftDist = 35.0, LoadTau = 0.27, HullLen = 7.93, WindX = 0.0, WindY = 0.0;
	double TintR = 1.0, TintG = 0.64, TintB = 0.38, Albedo = 1.2, Density = 0.9;
	// o albedo do chão quando não há captura (a areia da tabela do WorldForge, build_slice.py), linear
	double GroundR = 0.49, GroundG = 0.40, GroundB = 0.27;
	// o mip mínimo da folha (16/10): a placa lê no mínimo nesse nível (2^MipMin texels do quadro por amostra), um borrado
	// do mesmo tamanho no mundo em todo zoom: menos bolinho miúdo, a mesma forma grande. Longe, o nível da distância já é
	// maior e manda (antes era um a mais sobre o da distância: de longe virava mingau e vazava do octógono, a quina de 16/10)
	double MipMin = 0.0;
	// o expoente do pálido (a regra de Hapke): 1 = a parte densa fica com a cor do chão; maior = até o miolo fica com a
	// cor do grão fino (mais claro e menos saturado, como no game shot r1)
	double PaleK = 1.0;
	// o contraste das 6 luzes da gravação puxado pro neutro (0 = como gravado; 0,45 = a 2ª opinião de 16/10)
	double Flat6 = 0.0;
	// a saturação da poeira relativa à regra (1 = como a regra de Hapke dá; o alvo r1 fica a ~0,96× a saturação do chão)
	double DustSat = 1.0;
	// a interação (16/10, o bloco "interact"): os outros veículos e as explosões empurram a poeira. O ar entra direto na
	// posição do cartão (o pó é traçador: Stokes 0,8–59 ms, Hinds 1999), só no plano; nada sobe e o alfa nunca aumenta
	bool bInteract = false;
	double WakeB0 = 1.85;                      // a meia largura da esteira a meia altura (m): a meia largura do casco [E]
	double BubbleH = 1.2;                      // o fim da bolha atrás do casco, em alturas (Fan et al. 2025, JFM 1023 A30)
	int32 BlastModel = 0;                      // 0 = o pulso aprovado (WebGL v6 e BlastLab); 1 = Kinney–Graham (física)
	double BlastGain = 1.0, BlastT = 0.15;     // o ganho (A/B) e a duração espalhada do pulso (s, a aprovada)
	// a sombra na poeira (16/10, o Felipe: "as sombras do mapa não projetam na poeira, ela sempre brilha"; o bloco
	// "shadow"). 0 = a de antes (o cenário por cartão em 3 alturas, o tanque no UV3.y, só o sol cortado); 1 = por cartão,
	// com o céu de volume na sombra; 2 = por pixel: a altura da sombra do mapa numa grade alinhada ao sol (prédio, casa,
	// copa, duna) mais a grade dos tanques refeita a cada quadro, com o mesmo céu. Ao sol a cor é a de antes, bit a bit
	int32 ShadowMode = 0;
	double ShadowOcc = 0.15;                   // o céu tapado pelo obstáculo, na sombra (um tanque de 2,5 m a 2 m: ~17 %)
	double ShadowTexelM = 0.5;                 // o texel da altura da sombra do mapa (m)
	double ShadowSoftM = 0.18;                 // a borda da sombra na altura (m), somada à meia corda
	double ShadowChord = 0.2;                  // a meia corda da nuvem que o raio de vista atravessa, em lados do cartão
	double ShadowSunSide = 1.0;                // o lado do sol (o sunSide das camadas) nos modos 1 e 2: o A/B com 0
	// o céu na sombra: 0 = o do visual aprovado (só sai o sol), 1 = o de um volume (a física da revisão). Medido na câmera do
	// jogo (16/10, poeira ÷ chão em volta, ao sol 1,16): 0 dava 0,71 na sombra (mancha escura), 1 dava 1,36 (brilhava, o
	// defeito que o Felipe apontou), 0,5 dá 1,03 (o tom do chão na sombra; a faixa física da revisão é 0,75–1,1)
	double ShadowSky = 0.5;
	// a poeira em VOLUME (16/10 noite, o bloco "volume"; rts.TreadDust.Path 1): cada cartão vira um puff da mesma gravação
	// do EmberGen, marchado no raymarch das explosões (RTSDustField). A extinção sai da mesma conta do cozimento das folhas
	// (σ = 6·ρ²·Density·Kd·A ÷ escala, o mesmo ρ² do cozimento das folhas). VolDensity é o ajuste fino: 1 = igual à folha
	// (medido 17/10 com a gravação de 0,05 m: τ 0,94–1,01 e cobertura 1,00 nos 9 quadros; o 0,62 de 16/10 compensava o ρ
	// linear e a média 2×2×2, e a borda ficava névoa)
	double VolDensity = 1.0;
	// a erosão do cartão (τ − E·(0,5 − 0,5n)) na extinção: E ÷ a corda da nuvem (1,2 m × a escala); 1 = igual ao cartão
	double VolErode = 1.0;
	// a erosão fina na marcha (1/m; 0 = desligada). Testada 16/10 com 2: a poeira ficou granulada e suja (pontinhos
	// escuros), pior que o campo limpo; a nitidez vem da luz fina (a sombra própria em duas escalas), não do ruído
	double VolDetail = 0.0;
	double VolAlbedoMax = 0.97;                // o albedo de espalhamento simples máximo (Hapke, a partir da cor do chão)
	// a saturação da cor do chão antes da inversão de Hapke (1 = a do chão). O dustSat da v26 (0,6) era da regra do
	// material dos cartões; no volume a inversão já empalidece: medido 16/10 na régua r1, 0,6 dava 0,73× a saturação da
	// areia (o alvo, 0,96×)
	double VolSat = 1.0;
	// as gravações em volume (Resources/, comprimidas): a fina, dust_kick_v2_05.rtsdust = 0,05 m (a gravação inteira do
	// EmberGen), pro Ultra; a grossa, dust_kick_v2_r2.rtsdust = 0,1 m com a média de ρ² (a massa certa), pra Alta (17/10)
	FString VolKick = TEXT("dust_kick_v2_05.rtsdust");
	FString VolKickCoarse = TEXT("dust_kick_v2_r2.rtsdust");
	TArray<FRTSTreadLayer> Layers;
	bool Load(const FString& Path);
};

/** O atlas do cozimento (Resources/<cozimento>.atlas.json): grade, tempos, pivô, caixa e octógono de cada quadro. */
struct FRTSTreadAtlas
{
	int32 Cols = 8, Rows = 6, Frames = 48;
	double SideM = 4.2;
	FVector2D Pivot = FVector2D(0.5, 0.5);
	TArray<double> Times, BallBottom;
	TArray<FVector4> BBox;
	TArray<TArray<FVector2D>> Cut8;
	FVector2D LightScale = FVector2D(1.0, 1.0);   // as folhas normalizadas pra 8 bits: a luz × ganho (bake_norm8.py)
	bool Load(const FString& Path);
	double FrameAt(double BakeT) const;
};

/**
 * A poeira das lagartas no jogo (a v12 do FXLab: o jato que nasce na roda motriz, a nuvem que abre em leque até a altura
 * da torre, a cortina ao longo da lagarta, a sombra do tanque na poeira e a sombra da nuvem no chão). Um ator só pra
 * todos os veículos: cartões 6-way virados pra câmera (o cozimento do EmberGen tread_kick_v2c), recortados por octógono,
 * numa malha procedural. Os presets: 0 baixa (só a nuvem), 1 média (+ jato), 2 alta (+ cortina), 3 ultra (+ sombra no chão).
 */
UCLASS()
class RTSTREADDUST_API ARTSTreadDust : public AActor
{
	GENERATED_BODY()

public:
	ARTSTreadDust();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** A poeira está em volume (rts.TreadDust.Path 1 e a gravação carregada): o raymarch das explosões desenha, os cartões somem. */
	bool IsVolumePath() const;

	/** Os veículos deste quadro (chamar antes do Tick do ator, ou no mesmo quadro). */
	void SetSources(const TArray<FRTSTreadSource>& InSources) { Sources = InSources; }

	/** 0 baixa, 1 média, 2 alta, 3 ultra. */
	UFUNCTION(BlueprintCallable, Category = "RTS|Dust")
	void SetQuality(int32 Level) { Quality = FMath::Clamp(Level, 0, 3); }

	/** Liga e desliga tudo (some na hora; o que estava no ar some junto). */
	UFUNCTION(BlueprintCallable, Category = "RTS|Dust")
	void SetDustEnabled(bool bOn) { bEnabled = bOn; if (!bOn) Cards.Reset(); }

	/**
	 * A sombra do cenário na poeira (casas, árvores, pedras, o relevo): um mapa de profundidade visto do sol, tirado uma
	 * vez (e de novo se o sol mudar); cada vértice da poeira compara a sua distância ao sol com a do mapa. Sem custo na
	 * placa por quadro. Chamar quando o cenário que faz sombra mudar (prédio construído ou destruído).
	 */
	UFUNCTION(BlueprintCallable, Category = "RTS|Dust")
	void RefreshMapShadows() { bShadowDirty = true; }

	// A sombra por pixel (o modo 2) lê o mapa do sol COMUM de todas as fumaças: o URTSSunShadowSubsystem (o plugin
	// RTSSunShadow). Os prédios, as áreas que mudam e os veículos vão pra ele (AddShadowCaster, RefreshShadowArea,
	// SetVehicles); aqui a poeira só repassa os veículos dela enquanto o jogo não manda todos (bFeedSunShadow)

	/** Atores com estas etiquetas (e os Pawns) não entram no mapa de sombra: as unidades andam (a sombra delas é a das caixas). */
	UPROPERTY(EditAnywhere, Category = "RTS|Dust")
	TArray<FName> MapShadowIgnoreTags = { TEXT("GameTank"), TEXT("RTSUnit") };

	/** A semente do sorteio (a mesma história a cada partida; o efeito não volta pra simulação). */
	UPROPERTY(EditAnywhere, Category = "RTS|Dust")
	int32 Seed = 5;

	int32 NumCards() const { return Cards.Num(); }

	/** Medida (o A/B no mesmo processo): esconde a poeira na placa sem perder os cartões no ar. */
	void SetDustVisible(bool bOn);

	/** Teste: um parâmetro do material dos cartões (GroundRule, UnitMask...). */
	void SetMaterialScalar(FName Name, float Value);

	/** Medida: troca as folhas (false: as HDR de 16 bits; true: as normalizadas em BC7). */
	void UseNorm8Sheets(bool bOn);

	/** O custo da última conta na CPU (ms: o passo + a montagem da malha), pra medir. */
	double LastMs = 0.0;

	/** Uma explosão que empurra a poeira (vai pra fila com o relógio do plugin; ver FRTSDustBlast). */
	void AddBlast(const FRTSDustBlast& B);

	/** A partida (ou a rede) pausada: a poeira para no ar junto com as explosões. */
	void SetPaused(bool bOn) { bPaused = bOn; }

	/** A/B: a interação (os outros veículos e as explosões empurrando a poeira); o padrão vem do perfil. */
	void SetInteraction(bool bOn) { P.bInteract = bOn; }

	/** A/B (a medida do custo): a sombra na poeira, 0 = a de antes, 1 = por cartão, 2 = por pixel; o padrão vem do perfil. */
	void SetShadowMode(int32 Mode) { P.ShadowMode = FMath::Clamp(Mode, 0, 2); }

	/** Teste (o FXLab grava depois disso): a sombra por pixel já chegou (ou não é usada). */
	bool IsShadowReady() const;

	/** Os veículos desta poeira vão pro mapa do sol comum (até o jogo mandar todos, de roda e carcaças também). */
	bool bFeedSunShadow = true;

	/** Teste (o FXLab compara com a poeira dele): o relógio vem de fora, por Advance(); o Tick só desenha. */
	void SetExternalClock(bool bOn) { bExternalClock = bOn; }
	void Advance(double Dt);

private:
	struct FCard
	{
		FVector P = FVector::ZeroVector, V = FVector::ZeroVector, Back = FVector(-1, 0, 0);
		double Age = 0.0, Life = 1.0, S = 1.0, Rot = 0.0, Flip = 1.0, A = 1.0, GroundZ = 0.0;
		double Odo0 = 0.0, Bake0 = 0.0, Wake = 1.0, Sun = -1.0;
		double Map[3] = { -1.0, -1.0, -1.0 };    // o sol que passa pelo cenário no pé, no meio e no topo do cartão
		FLinearColor Col = FLinearColor(0.49f, 0.40f, 0.27f);   // o albedo do chão onde nasceu (linear)
		double RotFree = 0.0;                    // o giro livre (rad) do véu, sorteado ao nascer
		int32 Layer = 0, Src = 0;
		bool bDouble = false;                  // a baixa: um cartão no lugar de dois
	};

	// a esteira de cada veículo: as migalhas da traseira (o caminho, a direção, a velocidade e o relógio de cada uma)
	struct FCrumb
	{
		FVector2D Pos = FVector2D::ZeroVector, Dir = FVector2D(1, 0);
		double U = 0.0, T = 0.0;
		bool bBreak = false;                     // começa um trecho novo: não liga com a migalha de antes
	};
	struct FBlastEv
	{
		FVector2D Pos = FVector2D::ZeroVector, Dir = FVector2D::ZeroVector;
		double T0 = 0.0, R = 1.0, W = 1.0, CosLo = -2.0, CosHi = -1.0, Reach = 0.0;
	};
	double BlastXi(const FBlastEv& B, double R) const;
	FVector2D AirAt(const FCard& C, const FRTSTreadLayer& L, const TMap<int32, int32>& SrcIndex) const;
	FVector2D BlastShift(const FCard& C, double T0, double T1) const;
	TMap<int32, TArray<FCrumb>> Crumbs;
	TMap<int32, double> CrumbOdo, SrcRamp;
	TArray<FBlastEv> Blasts;
	double Clock = 0.0;
	bool bPaused = false;

	double Rand();
	double Rnd(double A, double B) { return A + Rand() * (B - A); }
	void Emit(const FRTSTreadSource& Src, double Dt);
	void Step(double Dt);
	void Draw(const FVector& CamM, const FVector& CamRight, const FVector& CamUp, const FVector& CamForward);
	double SunVisAt(const FVector& Pm, const FVector& ToSun) const;
	void FindSun();
	bool Init();
	void CaptureMapShadows();
	void CaptureGroundColor();
	FLinearColor GroundAlbedoAt(const FVector& Pm) const;
	double MapSunVis(const FVector& Pm) const;
	double TerrainDepthAt(double Fu, double Fv) const;
	bool ReadShadowRT(TFunctionRef<void(int32, float)> Each);
	// a sombra por pixel (o modo 2, RTSTreadDustShadow.cpp): a poeira lê o mapa do sol comum
	int32 ShadowModeNow() const;
	bool ProbeShadowMaterial(UMaterialInterface* M);
	class URTSSunShadowSubsystem* SunShadow() const;
	void UpdateShadowParams(const FVector& CamM, const FVector& CamRight, const FVector& CamUp, const FVector& CamFwd);
	// a poeira em volume (RTSTreadDustVolume.cpp): os cartões viram puffs do campo RTSDustField
	void SubmitVolume(const FVector& CamM, const FVector& CamFwd);
	bool bVolumeOn = false, bCardsVisible = true;
	int32 VolumeLogCount = 0;
	void PushMesh(UProceduralMeshComponent* Mesh, int32 Slot, TArray<FVector>& V, TArray<FVector>& N, TArray<FVector2D>& U0,
		TArray<FVector2D>& U1, TArray<FVector2D>& U2, TArray<FVector2D>& U3, TArray<FLinearColor>& Cl, int32 VertsPerCard);

	FRTSTreadProfile P;
	FRTSTreadAtlas Atlas;
	TArray<FCard> Cards;
	TArray<FRTSTreadSource> Sources;
	TMap<int64, double> LayerAcc, LayerNext;
	TMap<int32, double> Odo;
	TMap<int32, FVector2D> Load;
	TMap<int32, bool> Visible;
	uint32 RngState = 5, StepCount = 0;
	int32 Quality = 3;
	bool bEnabled = true, bExternalClock = false, bInit = false, bReady = false;
	double StepMs = 0.0, TanHalfH = 1.0, TanHalfV = 0.5625;
	// o mapa de sombra do cenário (cm): a profundidade vista do sol, a base da imagem e a calibração. Cada pixel guarda o
	// primeiro obstáculo que não é terreno (casa, pedra, árvore: positivo) ou, onde o sol bate direto no terreno, a
	// profundidade do terreno com sinal trocado (negativa). O terreno e o resto têm folgas diferentes (16/10)
	TArray<float> ShadowDepth;
	int32 ShadowRes = 4096;
	FVector ShadowO = FVector::ZeroVector, ShadowX = FVector::YAxisVector, ShadowY = FVector::ZAxisVector, ShadowF = FVector::XAxisVector;
	FVector ShadowSun = FVector::ZeroVector;
	double ShadowW = 1.0, ShadowCal = 0.0, ShadowSu = 1.0, ShadowSv = -1.0, ShadowBias = 30.0, ShadowBiasObj = 50.0;
	int32 ShadowObjTexels = 0;
	bool bShadowDirty = true;
	// a cor do chão (o albedo da BaseColor do terreno visto de cima, sRGB em 8 bits): a poeira nasce com a cor de onde sai
	TArray<FColor> GroundCol;
	int32 GroundRes = 2048;
	FVector GroundO = FVector::ZeroVector, GroundX = FVector::YAxisVector, GroundY = FVector::XAxisVector;
	double GroundW = 1.0;
	bool bGroundDirty = true;
	int32 TickCount = 0;
	// a areia presa na lagarta leva a cor de onde foi pega (id × 2 + lagarta)
	TMap<int64, FLinearColor> PickedCol;
	FVector SunDir = FVector(0.3, 0.3, 0.9).GetSafeNormal();
	TWeakObjectPtr<class UDirectionalLightComponent> Sun;
	bool bShadowMatOk = true;
	FVector SkyW = FVector(1.0 / 3.0, 2.0 / 3.0, 1.0 / 3.0);   // o céu de volume: os pesos do hemisfério de cima no cartão

	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> CardMesh;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> ShadowMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CardMid;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ShadowMid;
	UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> ShadowCapture;
	UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> GroundCapture;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> ShadowRT;
	int32 MeshCap[2] = { 0, 0 }, MeshVerts[2] = { 0, 0 };
	bool bMeshEmpty[2] = { false, false };
};
