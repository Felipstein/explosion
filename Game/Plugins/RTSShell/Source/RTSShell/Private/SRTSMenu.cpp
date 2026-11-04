#include "SRTSMenu.h"
#include "RTSShell.h"
#include "RTSShellAudio.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Rendering/DrawElements.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TextureResource.h"
#include "RenderTimer.h"
#include "DynamicRHI.h"
#include "ShaderPipelineCache.h"

using namespace RTSShellUI;

namespace
{
	// a geometria do painel e do tubo (menu.html, "a geometria do painel e do tubo")
	constexpr float PX0 = 818.0f, PY0 = 64.0f, PX1 = 1866.0f, PY1 = 1016.0f;
	constexpr float SCX = 1312.0f, SCY = 540.0f, R = 424.0f, RANGE = 40.0f, KM = R / RANGE;
	constexpr float PERIOD = 6.5f, TAU = 3.6f;
	constexpr int32 NB = 1440, NR = 300;
	constexpr float D2R = PI / 180.0f;
	constexpr float TWO_PI_F = 2.0f * PI;
	constexpr int32 AMB[3] = { 255, 172, 62 }, FLASH[3] = { 255, 238, 205 };

	FVector2f Polar(float Brg, float Rr) { return FVector2f(SCX + FMath::Sin(Brg) * Rr, SCY - FMath::Cos(Brg) * Rr); }
	float BrgOf(float X, float Y) { return FMath::Fmod(FMath::Atan2(X - SCX, -(Y - SCY)) + TWO_PI_F * 2.0f, TWO_PI_F); }
	float RiverY(float X) { return 60.0f + 70.0f * FMath::Sin(X / 150.0f + 0.6f) + 20.0f * FMath::Sin(X / 47.0f); }
	const FVector2f TOWN = Polar(35.0f * D2R, 0.45f * R);

	struct FArmy { const TCHAR* Id; const TCHAR* Name; int32 C[3]; int32 Frame; float FromBrg, FromR; int32 Spd; };
	// os frames do APP-6: 0 retângulo (amigo), 1 quadrado (neutro), 2 losango (hostil), 3 trevo (desconhecido)
	const FArmy ARMY[4] = {
		{ TEXT("usa"), TEXT("USA · 1-64 AR"), { 92, 160, 240 }, 0, 225.0f, 0.95f, 11 },
		{ TEXT("russia"), TEXT("RUS · 2 GTD"), { 214, 222, 232 }, 1, 318.0f, 0.95f, 10 },
		{ TEXT("china"), TEXT("PLA · 112 MECH"), { 226, 86, 70 }, 2, 72.0f, 0.96f, 9 },
		{ TEXT("gla"), TEXT("GLA · CELL 7"), { 214, 182, 104 }, 3, 142.0f, 0.94f, 14 } };
	constexpr float LEG = 240.0f;
	constexpr int32 AIRC[3] = { 92, 160, 240 };

	FVector3f ArmyPos(int32 I, float T)
	{
		const FArmy& A = ARMY[I];
		const FVector2f P0 = Polar(A.FromBrg * D2R, A.FromR * R);
		const float K = FMath::Fmod(T / LEG + I * 0.13f, 1.0f), E = K * 0.82f;
		const float Wob = FMath::Sin(T * 0.05f + A.Spd) * 8.0f;
		const float Dx = TOWN.X - P0.X, Dy = TOWN.Y - P0.Y, L = FMath::Sqrt(Dx * Dx + Dy * Dy);
		const float X = P0.X + Dx * E, Y = P0.Y + Dy * E;
		return FVector3f(X - Dy / L * Wob, Y + Dx / L * Wob, FMath::Atan2(Dx, -Dy));
	}
	FVector3f JetPos(float T)
	{
		const float K = FMath::Fmod(T, 26.0f) / 26.0f;
		const FVector2f A = Polar(262.0f * D2R, R * 1.05f), B = Polar(78.0f * D2R, R * 1.05f);
		return FVector3f(A.X + (B.X - A.X) * K, A.Y + (B.Y - A.Y) * K, FMath::Atan2(B.X - A.X, -(B.Y - A.Y)));
	}
	FVector2f HeloPos(float T) { const float A = T / 20.0f * TWO_PI_F; return FVector2f(TOWN.X + FMath::Cos(A) * 30.0f, TOWN.Y + FMath::Sin(A) * 30.0f); }
	constexpr float MIS_EVERY = 44.0f, MIS_FLY = 9.0f;
	const FVector2f MIS_FROM = Polar(58.0f * D2R, R * 1.02f);
	FVector2f RainCenter(float T) { return Polar((300.0f + T * 0.04f) * D2R, 0.62f * R); }
	const FVector2f DIALS[2] = { FVector2f(868.0f, 182.0f), FVector2f(930.0f, 182.0f) };

	float Glow(float S) { return FMath::Exp(-S / TAU); }
	FLinearColor Col(const int32 C[3], float A) { return Css(C[0], C[1], C[2], A); }
	FColor ColV(const int32 C[3], float A, bool bPremul = false) { return CssV(C[0], C[1], C[2], A, bPremul); }

	// as medidas do mockup no Chrome (Tools/measure_menu.js → menu_layout.json)
	constexpr float ItemTop0 = 453.5f, ItemH = 67.0f;
	// o topo do bloco do desenvolvimento e o das duas caixas sem hover na lista (remedido em 03/11: a 1ª medida pegou o item ainda
	// fechando e saiu 30 px abaixo)
	constexpr float DevY0 = 752.5f, TileY0 = 782.5f, TileW = 265.0f, TileH = 100.0f;
	const TCHAR* ItemTitle[4] = { TEXT("SKIRMISH"), TEXT("MULTIPLAYER"), TEXT("OPTIONS"), TEXT("QUIT") };
	const TCHAR* ItemDesc[4] = { TEXT("You against the AI · up to 8 players"), TEXT("Steam · LAN · Direct IP"), TEXT("Quality · Sound · Controls"), TEXT("Back to the desktop") };

	FString Pad3(int32 V) { return FString::Printf(TEXT("%03d"), ((V % 360) + 360) % 360); }

	// a abertura (04/11): as linhas da checagem do console, o rótulo e o estado (a dos shaders mostra o progresso de verdade)
	constexpr int32 BootLines = 6;
	const TCHAR* BootLabel[BootLines] = { TEXT("POWER BUS"), TEXT("SATCOM UPLINK"), TEXT("CRYPTO KEYS"), TEXT("IFF  MODE 3/C"), TEXT("SHADER CACHE"), TEXT("PPI RADAR  40 KM") };
	const TCHAR* BootState[BootLines] = { TEXT("NOMINAL"), TEXT("ESTABLISHED"), TEXT("LOADED"), TEXT("ARMED"), nullptr, TEXT("ONLINE") };
	constexpr float RiserS = 1.58f;   // a subida (UI_Boot_Riser, Tools/make_menu_sounds.py): começa no silêncio e termina no golpe
	float Smooth(float X) { X = FMath::Clamp(X, 0.0f, 1.0f); return X * X * (3.0f - 2.0f * X); }
}

void SRTSMenu::Construct(const FArguments& InArgs)
{
	// o eco polar (o A-scope lê ao longo do feixe), feito pela mesma conta do mockup (Tools/make_menu_assets.py)
	TArray<uint8> Raw;
	if (FFileHelper::LoadFileToArray(Raw, *(PluginDir() / TEXT("Resources/Menu/radar_polar.bin"))) && Raw.Num() == NB * NR * 4)
	{
		Ret.SetNumUninitialized(NB * NR);
		FMemory::Memcpy(Ret.GetData(), Raw.GetData(), Raw.Num());
	}
	else
	{
		Ret.Init(0.0f, NB * NR);
	}
	// a chuva (eco de tempo): 420 pontinhos fixos em volta de um centro que anda
	FRandomStream Rnd(7);
	for (int32 I = 0; I < 420; ++I)
	{
		const float A = Rnd.FRand() * 7.0f, Rr = FMath::Pow(Rnd.FRand(), 0.7f) * 80.0f;
		Rain.Add(FVector4f(FMath::Cos(A) * Rr * 1.3f, FMath::Sin(A) * Rr, Rnd.FRand(), 0.0f));
	}
	// a cascata do ESM numa textura pequena que sobe uma linha por quadro
	WfPixels.Init(FColor(0, 0, 0, 0), 210 * 98);
	if (UTexture2D* Tex = UTexture2D::CreateTransient(210, 98, PF_B8G8R8A8))
	{
		Tex->SRGB = true;
		Tex->Filter = TF_Nearest;
		Tex->UpdateResource();
		WfTexture.Reset(Tex);
		WfBrush = MakeShared<FSlateBrush>();
		WfBrush->SetResourceObject(Tex);
		WfBrush->ImageSize = FVector2D(210, 98);
	}
	// os tempos e as curvas das transições do CSS (menu.html, .menu li e .dv)
	for (int32 I = 0; I < 4; ++I)
	{
		ItemColor[I].Duration = 0.18f;
		ItemSlide[I].Duration = 0.25f;
		ItemSlide[I].X1 = 0.2f; ItemSlide[I].Y1 = 0.9f; ItemSlide[I].X2 = 0.3f; ItemSlide[I].Y2 = 1.2f;
		ItemOpen[I].Duration = 0.2f;
		ItemBar[I].Duration = 0.25f;
		ItemNum[I].Duration = 0.2f;
	}
	for (int32 I = 0; I < 2; ++I)
	{
		TileHover[I].Duration = 0.3f;
		TileHover[I].X1 = 0.2f; TileHover[I].Y1 = 0.9f; TileHover[I].X2 = 0.3f; TileHover[I].Y2 = 1.2f;
	}
	// o LAB e o SCRIPT: só no desenvolvimento (a cena da linha de comando vem armada; senão, quantas cenas há)
	const FRTSShellHandlers& H = FRTSShell::Handlers();
	bDev = H.IsDev ? H.IsDev() : false;
	Armed = H.ArmedScenario ? H.ArmedScenario() : FString();
	FString Json;
	TSharedPtr<FJsonObject> Root;
	if (bDev && H.Api && H.Api(TEXT("scenarios.json"), Json) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) && Root.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		Scenes = Root->TryGetArrayField(TEXT("scenarios"), List) ? FString::Printf(TEXT("%d SCENES"), List->Num()) : FString();
	}
	Restart();
}

void SRTSMenu::Restart()
{
	// aquece como o mockup: o feixe já deu umas voltas, as trilhas já têm rastro, a cascata já encheu. O teste pode fixar o instante
	// (-RTSMenuT=33.93, o ?t=…&still=1 do mockup: dois quadros de 1/30 s e o radar parado) pra comparar pixel a pixel
	float T0 = 30.0f;
	StillFrames = FParse::Value(FCommandLine::Get(), TEXT("-RTSMenuT="), T0) ? 0 : -1;
	T = T0;
	Beam = FMath::Fmod(T * TWO_PI_F / PERIOD, TWO_PI_F);
	Impacts = FMath::FloorToInt(T / MIS_EVERY);
	bSpec = false;
	Level = 0.0f;
	for (int32 I = 0; I < 98; ++I)
	{
		T += 1.0f / 30.0f;
		UpdateWaterfall(1.0f / 30.0f);
	}
	T = T0;
	for (int32 I = 0; I < 4; ++I)
	{
		Hist[I].Reset();
		for (int32 K = 8; K >= 1; --K)
		{
			const FVector3f P = ArmyPos(I, T - K * PERIOD);
			Hist[I].Add(FVector2f(P.X, P.Y));
		}
	}
	Crater.Reset();
	Hover = HitNone;
	DimT = Dim = 1.0f; GainT = Gain = 1.0f; KnobT = Knob = 0.0f; Off = OffT = 0.0f; OffUntil = 0.0;
	Pending.Reset();
	int32 TestHover = HitNone;
	FParse::Value(FCommandLine::Get(), TEXT("-RTSMenuHover="), TestHover);   // o teste fotografa o menu com um item em hover
	for (int32 I = 0; I < 4; ++I)
	{
		ItemColor[I].From = ItemColor[I].To = 0.0f; ItemColor[I].Start = -1.0;
		ItemSlide[I].From = ItemSlide[I].To = 0.0f; ItemSlide[I].Start = -1.0;
		ItemOpen[I].From = ItemOpen[I].To = 0.0f; ItemOpen[I].Start = -1.0;
		ItemBar[I].From = ItemBar[I].To = 0.0f; ItemBar[I].Start = -1.0;
		ItemNum[I].From = ItemNum[I].To = 0.0f; ItemNum[I].Start = -1.0;
	}
	for (int32 I = 0; I < 2; ++I)
	{
		TileHover[I].From = TileHover[I].To = 0.0f; TileHover[I].Start = -1.0;
	}
	if (TestHover != HitNone)
	{
		SetHover(TestHover);
	}
}

float SRTSMenu::Since(float Brg) const
{
	return FMath::Fmod(Beam - Brg + TWO_PI_F * 2.0f, TWO_PI_F) / TWO_PI_F * PERIOD;
}

void SRTSMenu::UpdateWaterfall(float Dt)
{
	// a cascata: a linha nova em cima, as outras descem uma (o drawImage dela mesma um pixel abaixo)
	FMemory::Memmove(WfPixels.GetData() + 210, WfPixels.GetData(), sizeof(FColor) * 210 * 97);
	const bool bJam = FMath::Fmod(T, 33.0f) < 3.4f;
	static const float Emit[4][2] = { { 34, 0.7f }, { 61, 0.45f }, { 118, 0.8f }, { 163, 0.35f } };
	for (int32 I = 0; I < 210; ++I)
	{
		float V = FMath::FRand() * 0.12f;
		if (bSpec)
		{
			V += Spec[FMath::FloorToInt(I / 210.0f * Spec.Num() * 0.6f)] / 255.0f * 0.9f;   // a trilha do menu, quando toca
		}
		for (const auto& E : Emit)
		{
			const float D = I - E[0] - FMath::Sin(T * 0.3f + E[0]) * 1.5f;
			V += E[1] * FMath::Exp(-D * D / 3.0f) * (0.7f + 0.3f * FMath::Sin(T * 2.0f + E[0]));
		}
		if (bJam)
		{
			V += 0.6f * FMath::FRand();
		}
		V = FMath::Min(1.0f, V);
		// a cor AMB × v com alfa v, sem pré-multiplicar (o ImageData do canvas)
		WfPixels[I] = FColor(uint8(AMB[0] * V), uint8(AMB[1] * V), uint8(AMB[2] * V), uint8(255.0f * V));
	}
	if (WfTexture.IsValid() && WfTexture->GetResource())
	{
		FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, 210, 98);
		const int32 Bytes = WfPixels.Num() * sizeof(FColor);
		uint8* Copy = static_cast<uint8*>(FMemory::Malloc(Bytes));
		FMemory::Memcpy(Copy, WfPixels.GetData(), Bytes);
		WfTexture->UpdateTextureRegions(0, 1, Region, 210 * sizeof(FColor), sizeof(FColor), Copy,
			[](uint8* Data, const FUpdateTextureRegion2D* Regions) { FMemory::Free(Data); delete Regions; });
	}
}

void SRTSMenu::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	Now = InCurrentTime;
	float Dt = FMath::Clamp(InDeltaTime, 0.0f, 0.1f);   // o passo nunca é negativo nem enorme
	if (StillFrames >= 0)
	{
		Dt = StillFrames < 2 ? 1.0f / 30.0f : 0.0f;
		++StillFrames;
	}
	T += Dt;
	Dim += (DimT - Dim) * (1.0f - FMath::Exp(-Dt * 3.0f));
	Gain += (GainT - Gain) * (1.0f - FMath::Exp(-Dt * 4.0f));
	Knob += (KnobT - Knob) * (1.0f - FMath::Exp(-Dt * 5.0f));
	if (OffUntil > 0.0 && Now >= OffUntil)
	{
		OffT = 0.0f;
		OffUntil = 0.0;
	}
	Off += (OffT - Off) * (1.0f - FMath::Exp(-Dt * 6.0f));
	if (BootT >= 0.0f)
	{
		StepBoot(Dt);
	}
	const float Spd = (TWO_PI_F / PERIOD) * (Hover == HitQuit ? 0.25f : 1.0f);
	Beam = FMath::Fmod(Beam + Spd * Dt, TWO_PI_F);
	// o rastro das trilhas: um ponto a cada passagem do feixe
	for (int32 I = 0; I < 4; ++I)
	{
		const FVector3f P = ArmyPos(I, T);
		if (Since(BrgOf(P.X, P.Y)) < Dt * 1.5f || Hist[I].IsEmpty())
		{
			Hist[I].Add(FVector2f(P.X, P.Y));
			if (Hist[I].Num() > 9)
			{
				Hist[I].RemoveAt(0);
			}
		}
	}
	// o impacto do míssil deixa a cratera no eco
	const float SinceImp = FMath::Fmod(T, MIS_EVERY) - MIS_FLY;
	const int32 N = FMath::FloorToInt(T / MIS_EVERY);
	if (SinceImp > 0.0f && SinceImp < 4.0f && N > Impacts)
	{
		Impacts = N;
		for (int32 I = 0; I < 40; ++I)
		{
			const float A = FMath::FRand() * 7.0f, Rr = FMath::FRand() * 9.0f;
			Crater.Add(FVector3f(TOWN.X + FMath::Cos(A) * Rr, TOWN.Y + FMath::Sin(A) * Rr, 0.5f + FMath::FRand() * 0.5f));
		}
	}
	bSpec = RTSShellAudio::Analyse(Spec, Level);
	if (StillFrames < 0 || Dt > 0.0f)
	{
		UpdateWaterfall(Dt);
	}
	// o custo: a média de 2 s no log (o desenho do menu é medido no OnPaint; o resto são os tempos do quadro da própria Unreal)
	CostFrames++;
	CostFrame += InDeltaTime * 1000.0;
	CostGame += FPlatformTime::ToMilliseconds(GGameThreadTime);
	CostRender += FPlatformTime::ToMilliseconds(GRenderThreadTime);
	CostGpu += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
	if (CostFrom == 0.0)
	{
		CostFrom = Now;
	}
	else if (Now - CostFrom >= 2.0 && CostFrames > 0)
	{
		const double K = 1.0 / CostFrames;
		UE_LOG(LogTemp, Log, TEXT("RTS telas: custo do menu nativo: desenho %.3f ms (processador) | quadro %.2f ms, jogo %.2f, render %.2f, placa %.2f (%d quadros) | radar %s (pico %.2f)"),
			PaintSeconds * 1000.0 * K, CostFrame * K, CostGame * K, CostRender * K, CostGpu * K, CostFrames, bSpec ? TEXT("ouvindo a música") : TEXT("sem música"), Level);
		CostFrom = Now;
		CostFrames = 0;
		CostFrame = CostGame = CostRender = CostGpu = PaintSeconds = 0.0;
	}
	// as ações com a espera do mockup (o clique anda antes de trocar de tela)
	for (int32 I = 0; I < Pending.Num(); ++I)
	{
		if (Now >= Pending[I].At)
		{
			TFunction<void()> Run = MoveTemp(Pending[I].Run);
			Pending.RemoveAt(I);
			Run();
			return;   // a tela pode ter saído
		}
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

FGeometry SRTSMenu::StageOf(const FGeometry& G) const
{
	const FVector2f Local = FVector2f(G.GetLocalSize());
	const float S = FMath::Min(Local.X / StageW, Local.Y / StageH);
	const FVector2f Offset = (Local - FVector2f(StageW, StageH) * S) * 0.5f;
	return G.MakeChild(FVector2f(StageW, StageH), FSlateLayoutTransform(S, Offset));
}

FVector2f SRTSMenu::StagePos(const FGeometry& G, const FPointerEvent& E) const
{
	return FVector2f(StageOf(G).AbsoluteToLocal(E.GetScreenSpacePosition()));
}

void SRTSMenu::ItemLayout(float OutTop[4], float OutExtra[4]) const
{
	float Y = ItemTop0;
	for (int32 I = 0; I < 4; ++I)
	{
		OutTop[I] = Y;
		OutExtra[I] = 30.0f * ItemOpen[I].Value(Now);   // a descrição abre 24 px + 6 de margem
		Y += ItemH + OutExtra[I];
	}
}

int32 SRTSMenu::HitAt(FVector2f P) const
{
	float Top[4], Extra[4];
	ItemLayout(Top, Extra);
	for (int32 I = 0; I < 4; ++I)
	{
		if (P.X >= 110.0f && P.X <= 650.0f && P.Y >= Top[I] && P.Y < Top[I] + ItemH + Extra[I])
		{
			return I;
		}
	}
	if (bDev)
	{
		const float Dy = Extra[0] + Extra[1] + Extra[2] + Extra[3];
		for (int32 K = 0; K < 2; ++K)
		{
			const float X = K == 0 ? 110.0f : 385.0f;
			if (P.X >= X && P.X <= X + TileW && P.Y >= TileY0 + Dy && P.Y <= TileY0 + TileH + Dy)
			{
				return HitLab + K;
			}
		}
	}
	return HitNone;
}

void SRTSMenu::SetHover(int32 Hit)
{
	if (Hit == Hover)
	{
		return;
	}
	Hover = Hit;
	if (Hit != HitNone)
	{
		RTSShellAudio::Play(RTSShellAudio::EUi::Hover);
	}
	for (int32 I = 0; I < 4; ++I)
	{
		const float On = I == Hover ? 1.0f : 0.0f;
		ItemColor[I].Set(On, Now, ItemColor[I].Value(Now));
		ItemSlide[I].Set(On, Now, ItemSlide[I].Value(Now));
		ItemOpen[I].Set(On, Now, ItemOpen[I].Value(Now));
		ItemBar[I].Set(On, Now, ItemBar[I].Value(Now));
		ItemNum[I].Set(On, Now, ItemNum[I].Value(Now));
	}
	for (int32 K = 0; K < 2; ++K)
	{
		TileHover[K].Set(Hover == HitLab + K ? 1.0f : 0.0f, Now, TileHover[K].Value(Now));
	}
	// o radar responde ao item (menu.html, hover()): o QUIT apaga o tubo, o OPTIONS aumenta o ganho e mexe os botões
	DimT = Hover == HitQuit ? 0.35f : 1.0f;
	GainT = Hover == HitOptions ? 1.6f : 1.0f;
	KnobT = Hover == HitOptions ? 1.0f : 0.0f;
}

FReply SRTSMenu::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!IsBooting())
	{
		SetHover(HitAt(StagePos(MyGeometry, MouseEvent)));
	}
	return FReply::Handled();
}

FReply SRTSMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (IsBooting())
	{
		SkipBoot();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SRTSMenu::StartBoot(bool bFirstEver)
{
	Restart();
	BootReveal = bFirstEver ? 3.9f : 2.5f;
	BootRelayAt = bFirstEver ? 0.8f : 0.3f;
	BootLine0 = bFirstEver ? 1.0f : 0.45f;
	BootLineDt = bFirstEver ? 0.27f : 0.12f;
	BootT = 0.0f;
	BootHeld = 0.0f;
	bBootRelay = bBootRiser = bBootRevealed = false;
	BootPso = BootPsoMax = 0;
	Off = OffT = 1.0f;
	UE_LOG(LogTemp, Log, TEXT("RTS telas: abertura (%s): o relé em %.2f s, o golpe em %.2f s"), bFirstEver ? TEXT("a 1ª vez") : TEXT("a curta"), BootRelayAt, BootReveal);
}

void SRTSMenu::StepBoot(float Dt)
{
	// na 1ª vez do pacote os shaders compilam: a abertura segura na linha deles, antes da subida (a Epic recomenda a tela até o
	// NumPrecompilesRemaining zerar), no máximo 60 s; no editor é sempre 0
	BootPso = FShaderPipelineCache::NumPrecompilesRemaining();
	BootPsoMax = FMath::Max(BootPsoMax, BootPso);
	const float Hold = BootReveal - RiserS - 0.05f;
	if (BootPso > 0 && BootT >= Hold && BootHeld < 60.0f)
	{
		BootHeld += Dt;
	}
	else
	{
		BootT += Dt;
	}
	if (!bBootRelay && BootT >= BootRelayAt)
	{
		bBootRelay = true;
		RTSShellAudio::Play(RTSShellAudio::EUi::BootRelay);
	}
	if (!bBootRiser && BootT >= BootReveal - RiserS)
	{
		bBootRiser = true;
		RTSShellAudio::Play(RTSShellAudio::EUi::BootRiser);
	}
	if (!bBootRevealed && BootT >= BootReveal)
	{
		// o golpe: a música entra junto e o tubo acende estourado e assenta (o ganho volta pro 1 no Tick)
		bBootRevealed = true;
		RTSShellAudio::Play(RTSShellAudio::EUi::BootReveal);
		RTSShellAudio::SetMusic(RTSShellAudio::EMusic::Menu);
		Gain = 1.9f;
	}
	// o tubo acende no último segundo antes do golpe: a linha que abre na imagem (o desligar do QUIT ao contrário)
	const float Ig = FMath::Clamp((BootT - (BootReveal - 1.0f)) / 1.0f, 0.0f, 1.0f);
	Off = OffT = 1.0f - FMath::Pow(Ig, 0.8f);
	if (BootT >= BootReveal + 1.6f)
	{
		BootT = -1.0f;
		Off = OffT = 0.0f;
	}
}

void SRTSMenu::SkipBoot()
{
	// pular: direto pro fim da ignição (o tubo acende rápido e o golpe vem), sem o resto da checagem nem a espera dos shaders
	if (BootT >= 0.0f && BootT < BootReveal - 0.35f)
	{
		BootT = BootReveal - 0.35f;
		bBootRelay = bBootRiser = true;
		BootHeld = 60.0f;
	}
}

void SRTSMenu::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SetHover(HitNone);
}

FCursorReply SRTSMenu::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return FCursorReply::Cursor(Hover != HitNone ? EMouseCursor::Hand : EMouseCursor::Default);
}

FReply SRTSMenu::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	if (IsBooting())
	{
		SkipBoot();
		return FReply::Handled();
	}
	const int32 Hit = HitAt(StagePos(MyGeometry, MouseEvent));
	if (Hit != HitNone)
	{
		RTSShellAudio::Click();
	}
	const auto Later = [this](float Wait, TFunction<void()> Run) { Pending.Add({ Now + Wait, MoveTemp(Run) }); };
	switch (Hit)
	{
	case HitSkirmish: Later(0.26f, []() { FRTSShell::Show(TEXT("skirmish.html")); }); break;
	case HitMulti: Later(0.26f, []() { FRTSShell::Show(TEXT("multiplayer.html")); }); break;
	case HitOptions: Later(0.26f, []() { FRTSShell::Show(TEXT("screens.html?s=opt&from=menu")); }); break;
	case HitQuit:
		// o tubo desliga (fecha numa linha e num ponto) e o jogo fecha depois
		OffT = 1.0f;
		OffUntil = Now + 1.8;
		Later(0.9f, []() { if (FRTSShell::Handlers().Quit) FRTSShell::Handlers().Quit(); });
		break;
	case HitLab: Later(0.2f, []() { if (FRTSShell::Handlers().OpenLab) FRTSShell::Handlers().OpenLab(); }); break;
	case HitScript:
	{
		const FString Page = Armed.IsEmpty() ? FString(TEXT("script.html")) : TEXT("script.html?armed=") + Armed;
		Later(0.2f, [Page]() { FRTSShell::Show(Page); });
		break;
	}
	default: return FReply::Handled();
	}
	return FReply::Handled();
}

// ---------------------------------------------------------------------------------------------------------------------------
// o desenho

int32 SRTSMenu::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const double PaintStart = FPlatformTime::Seconds();
	const FGeometry S = StageOf(AllottedGeometry);
	int32 L = LayerId;
	// fora do palco (tela mais larga ou mais alta que 16:9): a cor do fundo
	FSlateDrawElement::MakeBox(Out, L, AllottedGeometry.ToPaintGeometry(), White(), ESlateDrawEffect::None, Css(10, 13, 11));
	++L;
	// o fundo do palco (#stage): a cor, o brilho âmbar do alto e a grade de 48 px
	const FPaintGeometry Full = S.ToPaintGeometry(FVector2f(StageW, StageH), FSlateLayoutTransform());
	FSlateDrawElement::MakeBox(Out, L, Full, Image(TEXT("backdrop_glow.png"), FVector2f(480, 270)), ESlateDrawEffect::None, FLinearColor::White);
	++L;
	{
		FMesh Grid(S);
		const FColor C = CssV(255, 255, 255, 0.02f);
		for (float X = 0.0f; X < StageW; X += 48.0f) Grid.Rect(X, 0.0f, 1.0f, StageH, C);
		for (float Y = 0.0f; Y < StageH; Y += 48.0f) Grid.Rect(0.0f, Y, StageW, 1.0f, C);
		Grid.Draw(Out, L++, White());
	}
	PaintRadar(Out, L, S);
	// as linhas de varredura e a vinheta (por cima do radar, por baixo do texto)
	{
		FMesh Scan(S);
		const FColor C = CssV(255, 255, 255, 0.018f);
		for (float Y = 0.0f; Y < StageH; Y += 3.0f) Scan.Rect(0.0f, Y, StageW, 1.0f, C);
		Scan.Draw(Out, L++, White());
	}
	FSlateDrawElement::MakeBox(Out, L++, Full, Image(TEXT("vignette.png"), FVector2f(480, 270)), ESlateDrawEffect::None, FLinearColor::White);
	if (BootT < 0.0f)
	{
		PaintLink(Out, L, S);
		PaintSide(Out, L, S);
	}
	else
	{
		PaintSideReveal(Out, L, S);
		PaintBootCover(Out, L, S);
		PaintBootLines(Out, L, S);
	}
	PaintSeconds += FPlatformTime::Seconds() - PaintStart;
	return L;
}

void SRTSMenu::PaintStatic(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	// a camada de vetor do tubo (o "over" do mockup): grade de 5 km, rio, estradas, anéis, rosa, a moldura e as caixas
	const FLinearColor Ac = Css(216, 173, 78, 0.05f);
	for (int32 K = -8; K <= 8; ++K)
	{
		const float O = K * 5.0f * KM;
		const float H = FMath::Sqrt(FMath::Max(0.0f, R * R - O * O));
		Lines(Out, L, S, { FVector2f(SCX + O, SCY - H), FVector2f(SCX + O, SCY + H) }, Ac);
		Lines(Out, L, S, { FVector2f(SCX - H, SCY + O), FVector2f(SCX + H, SCY + O) }, Ac);
	}
	{
		TArray<FVector2f> Seg;
		for (float X = -R; X <= R; X += 4.0f)
		{
			const FVector2f P(SCX + X, SCY + RiverY(X));
			if (FVector2f::Distance(P, FVector2f(SCX, SCY)) <= R) Seg.Add(P);
			else if (Seg.Num()) { Lines(Out, L, S, Seg, Css(120, 190, 200, 0.16f), 2.0f); Seg.Reset(); }
		}
		Lines(Out, L, S, Seg, Css(120, 190, 200, 0.16f), 2.0f);
	}
	const FLinearColor Road = Css(216, 173, 78, 0.13f);
	Dashed(Out, L, S, { Polar(225 * D2R, R), Polar(215 * D2R, 0.6f * R), Polar(120 * D2R, 0.1f * R), TOWN, Polar(48 * D2R, R) }, Road, 1.2f, 7.0f);
	Dashed(Out, L, S, { Polar(300 * D2R, R), Polar(320 * D2R, 0.55f * R), TOWN }, Road, 1.2f, 7.0f);
	Dashed(Out, L, S, { TOWN, Polar(110 * D2R, 0.55f * R), Polar(140 * D2R, R) }, Road, 1.2f, 7.0f);
	++L;
	for (int32 K = 1; K <= 4; ++K) Lines(Out, L, S, Arc(FVector2f(SCX, SCY), K * 10.0f * KM), Css(216, 173, 78, 0.16f));
	for (int32 A = 0; A < 360; A += 30) Lines(Out, L, S, { FVector2f(SCX, SCY), Polar(A * D2R, R) }, Css(216, 173, 78, 0.07f));
	const FSlateFontInfo Mono11 = Font(EFace::Mono500, 11.0f), Mono12 = Font(EFace::Mono500, 12.0f), Mono10b = Font(EFace::Mono600, 10.0f);
	for (int32 K = 1; K <= 4; ++K) Text(Out, L, S, FString::FromInt(K * 10), Mono11, SCX + 4.0f, SCY - K * 10.0f * KM + 13.0f, Css(216, 173, 78, 0.35f));
	for (int32 A = 0; A < 360; A += 2)
	{
		const bool bBig = A % 10 == 0;
		Lines(Out, L, S, { Polar(A * D2R, R + 3.0f), Polar(A * D2R, R + (bBig ? 15.0f : 8.0f)) }, Css(216, 173, 78, bBig ? 0.5f : 0.25f));
	}
	for (int32 A = 0; A < 360; A += 30)
	{
		const FVector2f P = Polar(A * D2R, R + 30.0f);
		Text(Out, L, S, Pad3(A), Mono12, P.X, P.Y + 4.5f, Css(216, 173, 78, 0.55f), 1);   // textBaseline middle
	}
	Lines(Out, L, S, Arc(FVector2f(SCX, SCY), R), Css(216, 173, 78, 0.3f), 1.2f);
	Lines(Out, L, S, Arc(FVector2f(SCX, SCY), R + 44.0f), Css(216, 173, 78, 0.12f), 1.2f);
	++L;
	// a moldura do painel (o fundo bem leve por cima do tubo, a borda fina, as cantoneiras douradas)
	{
		FMesh Panel(S);
		Panel.Rect(PX0, PY0, PX1 - PX0, PY1 - PY0, CssV(18, 24, 22, 0.35f));
		for (const FVector4f B : { FVector4f(842, 872, 238, 120), FVector4f(1630, 872, 212, 120), FVector4f(1630, 88, 212, 118) })
		{
			Panel.Rect(B.X + 1, B.Y + 1, B.Z - 1, B.W - 1, CssV(0, 0, 0, 0.25f));
		}
		Panel.Draw(Out, L++, White());
	}
	Lines(Out, L, S, { FVector2f(PX0 + 0.5f, PY0 + 0.5f), FVector2f(PX1 + 0.5f, PY0 + 0.5f), FVector2f(PX1 + 0.5f, PY1 + 0.5f), FVector2f(PX0 + 0.5f, PY1 + 0.5f), FVector2f(PX0 + 0.5f, PY0 + 0.5f) },
		Css(230, 220, 190, 0.08f));
	const float Cl = 16.0f;
	for (const FVector4f C : { FVector4f(PX0, PY0, 1, 1), FVector4f(PX1, PY0, -1, 1), FVector4f(PX0, PY1, 1, -1), FVector4f(PX1, PY1, -1, -1) })
	{
		Lines(Out, L, S, { FVector2f(C.X + C.Z * Cl, C.Y), FVector2f(C.X, C.Y), FVector2f(C.X, C.Y + C.W * Cl) }, Css(216, 173, 78, 0.6f), 2.0f);
	}
	for (const FVector4f B : { FVector4f(842, 872, 238, 120), FVector4f(1630, 872, 212, 120), FVector4f(1630, 88, 212, 118) })
	{
		Lines(Out, L, S, { FVector2f(B.X + 0.5f, B.Y + 0.5f), FVector2f(B.X + B.Z + 0.5f, B.Y + 0.5f), FVector2f(B.X + B.Z + 0.5f, B.Y + B.W + 0.5f), FVector2f(B.X + 0.5f, B.Y + B.W + 0.5f), FVector2f(B.X + 0.5f, B.Y + 0.5f) },
			Css(230, 220, 190, 0.10f));
	}
	Text(Out, L, S, TEXT("A-SCOPE"), Mono10b, 850, 886, Css(230, 220, 190, 0.32f));
	Text(Out, L, S, TEXT("ESM · 2–18 GHZ"), Mono10b, 1638, 886, Css(230, 220, 190, 0.32f));
	Text(Out, L, S, TEXT("TRACKS"), Mono10b, 1638, 102, Css(230, 220, 190, 0.32f));
	++L;
}

void SRTSMenu::PaintRadar(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	PaintStatic(Out, L, S);
	const float D = Dim * (1.0f - Off);
	const FVector2f C(SCX, SCY);
	// o brilho de dentro do tubo (o gradiente radial de dois círculos do mockup, bem fraco)
	{
		FMesh M(S);
		const FVector2f C0(SCX - 60.0f, SCY - 80.0f);
		const FVector2f E = C - C0;
		const float Dr = R - 20.0f;
		const auto AlphaAt = [&](FVector2f P)
		{
			const FVector2f Dd = P - C0;
			const float A = FVector2f::DotProduct(E, E) - Dr * Dr, B = -2.0f * (FVector2f::DotProduct(Dd, E) + 20.0f * Dr), Cc = FVector2f::DotProduct(Dd, Dd) - 400.0f;
			float Tt = 0.0f;
			if (FMath::Abs(A) < 1e-4f) Tt = -Cc / B;
			else { const float Disc = FMath::Max(0.0f, B * B - 4 * A * Cc); Tt = FMath::Max((-B + FMath::Sqrt(Disc)) / (2 * A), (-B - FMath::Sqrt(Disc)) / (2 * A)); }
			return 0.035f * D * (1.0f - FMath::Clamp(Tt, 0.0f, 1.0f));
		};
		const int32 NA = 96, NRg = 14;
		TArray<int32> Prev, Cur;
		for (int32 Ri = 0; Ri <= NRg; ++Ri)
		{
			Cur.Reset();
			const float Rr = R * Ri / NRg;
			for (int32 Ai = 0; Ai <= NA; ++Ai)
			{
				const FVector2f P = Polar(TWO_PI_F * Ai / NA, Rr);
				Cur.Add(M.Vert(P, FVector2f(0, 0), ColV(AMB, AlphaAt(P), true)));
			}
			if (Prev.Num())
			{
				for (int32 Ai = 0; Ai < NA; ++Ai) { M.Tri(Prev[Ai], Cur[Ai], Cur[Ai + 1]); M.Tri(Prev[Ai], Cur[Ai + 1], Prev[Ai + 1]); }
			}
			Prev = Cur;
		}
		M.Draw(Out, L++, White(), ESlateDrawEffect::PreMultipliedAlpha);
	}
	// o eco do chão com a persistência do fósforo: um leque de 720 fatias que começa no feixe e anda pra trás; cada fatia tem o
	// brilho do tempo desde a passagem (o gradiente cônico do mockup) e a textura do eco por baixo. Âmbar por cima (source-over) e o
	// clarão somando luz (o "lighter": alfa pré-multiplicado com alfa zero)
	{
		const FSlateBrush* RetBrush = Image(TEXT("radar_returns.png"), FVector2f(852, 852));
		const float U0 = SCX - R - 2.0f, V0 = SCY - R - 2.0f, Size = 2.0f * R + 4.0f;
		const auto UV = [&](FVector2f P) { return FVector2f((P.X - U0) / Size, (P.Y - V0) / Size); };
		const int32 N = 720;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			const float Strength = Pass == 0 ? D * 0.34f * Gain : D * 0.4f * Gain;
			if (Strength < 0.005f) continue;
			FMesh M(S);
			int32 Last0 = -1, Last1 = -1;
			for (int32 K = 0; K <= N; ++K)
			{
				const float Tsec = float(K) / N * PERIOD;
				const float Mask = Pass == 0 ? FMath::Min(1.0f, Glow(Tsec)) : FMath::Min(1.0f, FMath::Exp(-Tsec / 0.16f));
				const float A = Mask * Strength;
				if (Pass == 1 && A < 0.002f && K > 0) break;
				const float Brg = Beam - TWO_PI_F * K / N;
				const FVector2f Pi = Polar(Brg, 0.5f), Po = Polar(Brg, R + 2.0f);
				const FColor Vc = Pass == 0 ? ColV(AMB, A, true) : FColor(uint8(FLASH[0] * A), uint8(FLASH[1] * A), uint8(FLASH[2] * A), 0);
				const int32 I0 = M.Vert(Pi, UV(Pi), Vc), I1 = M.Vert(Po, UV(Po), Vc);
				if (Last0 >= 0) { M.Tri(Last0, Last1, I1); M.Tri(Last0, I1, I0); }
				Last0 = I0; Last1 = I1;
			}
			M.Draw(Out, L++, RetBrush, ESlateDrawEffect::PreMultipliedAlpha);
		}
		// a cratera do último impacto (o mockup risca no próprio eco âmbar): pontos com a mesma persistência
		if (Crater.Num())
		{
			FMesh M(S);
			for (const FVector3f& P : Crater)
			{
				const float A = P.Z * FMath::Min(1.0f, Glow(Since(BrgOf(P.X, P.Y)))) * D * 0.34f * Gain;
				if (A > 0.004f) M.Rect(P.X, P.Y, 1.6f, 1.6f, ColV(AMB, A));
			}
			M.Draw(Out, L++, White());
		}
	}
	// o fio do feixe (do claro no centro ao fraco na borda)
	FSlateDrawElement::MakeLines(Out, L++, S.ToPaintGeometry(), TArray<FVector2f>{ C, Polar(Beam, R) },
		TArray<FLinearColor>{ Col(FLASH, 0.5f * D), Col(FLASH, 0.12f * D) }, ESlateDrawEffect::None, FLinearColor::White, true, 1.4f);
	// a névoa do fósforo logo atrás do feixe (meio radiano, de 0 a 0,05)
	{
		FMesh M(S);
		int32 Last0 = -1, Last1 = -1;
		const int32 N = 40;
		for (int32 K = 0; K <= N; ++K)
		{
			const float F = float(K) / N;   // 0 = o começo da cunha (meio radiano atrás), 1 = o feixe
			const float Brg = Beam - 0.5f + 0.5f * F;
			const FColor Vc = ColV(AMB, 0.05f * D * F, true);
			const int32 I0 = M.Vert(Polar(Brg, 0.5f), FVector2f(0, 0), Vc), I1 = M.Vert(Polar(Brg, R), FVector2f(0, 0), Vc);
			if (Last0 >= 0) { M.Tri(Last0, Last1, I1); M.Tri(Last0, I1, I0); }
			Last0 = I0; Last1 = I1;
		}
		M.Draw(Out, L++, White(), ESlateDrawEffect::PreMultipliedAlpha);
	}
	// a chuva e o bloqueio (pontinhos com a persistência)
	{
		FMesh M(S);
		const FVector2f Rc = RainCenter(T);
		for (const FVector4f& P : Rain)
		{
			const float X = Rc.X + P.X, Y = Rc.Y + P.Y;
			if (FVector2f::Distance(FVector2f(X, Y), C) > R) continue;
			const float A = Glow(Since(BrgOf(X, Y))) * (0.18f + 0.3f * P.Z) * D * Gain;
			if (A >= 0.01f) M.Rect(X, Y, 1.6f, 1.6f, ColV(AMB, A));
		}
		if (FMath::Fmod(T, 33.0f) < 3.4f)
		{
			const float Jb = 142.0f * D2R + FMath::Sin(T * 3.0f) * 0.02f;
			const float G = Glow(Since(Jb));
			for (int32 I = 0; I < 160; ++I)
			{
				const FVector2f P = Polar(Jb + (FMath::FRand() - 0.5f) * 0.03f, FMath::FRand() * R);
				M.Rect(P.X, P.Y, 2.0f, 2.0f, ColV(AMB, (0.15f + FMath::FRand() * 0.4f) * D * G));
			}
		}
		M.Draw(Out, L++, White());
	}
	PaintTracks(Out, L, S, D);
	PaintInstruments(Out, L, S, D);
	// o hover do SKIRMISH: o cursor de rumo e o estrobo de distância travam no objetivo
	if (Hover == HitSkirmish)
	{
		const float B = BrgOf(TOWN.X, TOWN.Y), Rr = FVector2f::Distance(TOWN, C);
		Dashed(Out, L, S, { C, Polar(B, R) }, Css(FLASH[0], FLASH[1], FLASH[2], 0.4f), 1.0f, 4.0f);
		TArray<FVector2f> ArcP;
		for (int32 I = 0; I <= 24; ++I) ArcP.Add(Polar(B - 0.25f + 0.5f * I / 24, Rr));
		Dashed(Out, L, S, ArcP, Css(FLASH[0], FLASH[1], FLASH[2], 0.4f), 1.0f, 4.0f);
		const float Sz = 18.0f + FMath::Sin(T * 6.0f) * 2.0f, Cn = 5.0f;
		for (const FVector2f Sg : { FVector2f(-1, -1), FVector2f(1, -1), FVector2f(-1, 1), FVector2f(1, 1) })
		{
			Lines(Out, L, S, { FVector2f(TOWN.X + Sg.X * Sz, TOWN.Y + Sg.Y * (Sz - Cn)), FVector2f(TOWN.X + Sg.X * Sz, TOWN.Y + Sg.Y * Sz), FVector2f(TOWN.X + Sg.X * (Sz - Cn), TOWN.Y + Sg.Y * Sz) },
				Col(FLASH, 0.8f));
		}
		++L;
	}
	// o tubo desligando (o QUIT): a imagem fecha numa linha e num ponto
	if (Off > 0.01f)
	{
		const float K = Off;
		const float Ry = FMath::Max(1.0f, R * (1.0f - FMath::Min(1.0f, K * 1.6f)));
		const float Rx = R * (K < 0.62f ? 1.0f : 1.0f - (K - 0.62f) / 0.38f);
		FMesh M(S);
		TArray<FVector2f> Disc = Arc(C, R + 1.0f, 0.0f, TWO_PI_F, 120);
		M.Poly(Disc, CssV(10, 13, 11, FMath::Min(1.0f, K * 1.4f)));
		if (Ry < 3.0f)
		{
			TArray<FVector2f> El;
			for (int32 I = 0; I < 48; ++I) { const float A = TWO_PI_F * I / 48; El.Add(FVector2f(SCX + FMath::Cos(A) * FMath::Max(1.0f, Rx), SCY + FMath::Sin(A) * 1.2f)); }
			M.Poly(El, ColV(FLASH, (1.0f - FMath::Max(0.0f, K - 0.7f) / 0.3f) * 0.7f));
		}
		M.Draw(Out, L++, White());
	}
}

void SRTSMenu::PaintTracks(FSlateWindowElementList& Out, int32& L, const FGeometry& S, float D) const
{
	const FSlateFontInfo Mono11 = Font(EFace::Mono500, 11.0f);
	FMesh Fill(S), Dots(S);
	struct FLabel { FString S; FVector2f P; FLinearColor C; };
	TArray<FLabel> Labels;
	const int32 LS = L + 1;   // os contornos por cima do preenchimento
	const auto Symbol = [&](int32 Kind, float X, float Y, const int32 Cc[3], float A)
	{
		const FLinearColor Stroke = Col(Cc, A);
		const FColor FillC = ColV(Cc, A * 0.18f);
		TArray<FVector2f> P;
		switch (Kind)
		{
		case 0: P = { FVector2f(X - 9, Y - 6), FVector2f(X + 9, Y - 6), FVector2f(X + 9, Y + 6), FVector2f(X - 9, Y + 6) }; break;
		case 1: P = { FVector2f(X - 7, Y - 7), FVector2f(X + 7, Y - 7), FVector2f(X + 7, Y + 7), FVector2f(X - 7, Y + 7) }; break;
		case 2: P = { FVector2f(X, Y - 9), FVector2f(X + 9, Y), FVector2f(X, Y + 9), FVector2f(X - 9, Y) }; break;
		default: break;
		}
		if (P.Num())
		{
			Fill.Poly(P, FillC);
			const FVector2f First = P[0];   // copiado antes: o Add pode realocar a lista
			P.Add(First);
			Lines(Out, LS, S, P, Stroke, 1.3f);
		}
		else if (Kind == 3)
		{
			for (const FVector2f O : { FVector2f(0, -4), FVector2f(4, 0), FVector2f(0, 4), FVector2f(-4, 0) })
			{
				TArray<FVector2f> Cir = Arc(FVector2f(X + O.X, Y + O.Y), 4.5f, 0.0f, TWO_PI_F, 20);
				Fill.Poly(Cir, ColV(Cc, A * 0.18f * 0.5f));
				Lines(Out, LS, S, Cir, Stroke, 1.3f);
			}
		}
		else if (Kind == 4)   // avião: o meio círculo de cima
		{
			TArray<FVector2f> Half = Arc(FVector2f(X, Y + 3), 6.0f, PI, TWO_PI_F, 16);
			Fill.Poly(Half, FillC);
			Lines(Out, LS, S, Half, Stroke, 1.3f);
		}
		else if (Kind == 5)   // helicóptero: a cruz e o rotor
		{
			Lines(Out, LS, S, { FVector2f(X - 6, Y), FVector2f(X + 6, Y) }, Stroke, 1.3f);
			Lines(Out, LS, S, { FVector2f(X, Y - 6), FVector2f(X, Y + 6) }, Stroke, 1.3f);
			TArray<FVector2f> Cir = Arc(FVector2f(X, Y), 4.0f, 0.0f, TWO_PI_F, 16);
			Fill.Poly(Cir, FillC);
			Lines(Out, LS, S, Cir, Stroke, 1.3f);
		}
		if (Kind <= 3)   // o escalão (batalhão)
		{
			Lines(Out, LS, S, { FVector2f(X - 2.5f, Y - 12), FVector2f(X - 2.5f, Y - 16) }, Stroke, 1.3f);
			Lines(Out, LS, S, { FVector2f(X + 2.5f, Y - 12), FVector2f(X + 2.5f, Y - 16) }, Stroke, 1.3f);
		}
	};
	const auto Vector = [&](float X, float Y, float Hd, float Len, const int32 Cc[3], float A)
	{
		Lines(Out, LS, S, { FVector2f(X + FMath::Sin(Hd) * 10, Y - FMath::Cos(Hd) * 10), FVector2f(X + FMath::Sin(Hd) * (10 + Len), Y - FMath::Cos(Hd) * (10 + Len)) }, Col(Cc, A * 0.6f));
	};
	const auto Brackets = [&](float X, float Y, float Sz, const FLinearColor& Cc)
	{
		const float Cn = 5.0f;
		for (const FVector2f Sg : { FVector2f(-1, -1), FVector2f(1, -1), FVector2f(-1, 1), FVector2f(1, 1) })
		{
			Lines(Out, LS, S, { FVector2f(X + Sg.X * Sz, Y + Sg.Y * (Sz - Cn)), FVector2f(X + Sg.X * Sz, Y + Sg.Y * Sz), FVector2f(X + Sg.X * (Sz - Cn), Y + Sg.Y * Sz) }, Cc);
		}
	};
	for (int32 I = 0; I < 4; ++I)
	{
		const FArmy& A = ARMY[I];
		const FVector3f P = ArmyPos(I, T);
		const float Al = (0.3f + 0.7f * Glow(Since(BrgOf(P.X, P.Y)))) * D;
		for (int32 J = 0; J < Hist[I].Num(); ++J)
		{
			Dots.Rect(Hist[I][J].X - 1, Hist[I][J].Y - 1, 2, 2, ColV(A.C, Al * 0.35f * (J + 1) / Hist[I].Num()));
		}
		Symbol(A.Frame, P.X, P.Y, A.C, Al);
		Vector(P.X, P.Y, P.Z, 18.0f + A.Spd * 1.6f, A.C, Al);
		Labels.Add({ A.Name, FVector2f(P.X + 14, P.Y - 8), Col(A.C, Al * 0.85f) });
		Labels.Add({ FString::Printf(TEXT("%s %dKT"), *Pad3(FMath::RoundToInt(P.Z / D2R + 360.0f)), A.Spd), FVector2f(P.X + 14, P.Y + 5), Col(AMB, Al * 0.5f) });
		if (Hover == HitMulti)   // IFF: a trilha responde e mostra o emblema
		{
			FSlateDrawElement::MakeBox(Out, L + 2, S.ToPaintGeometry(FVector2f(28, 28), FSlateLayoutTransform(FVector2f(P.X - 44, P.Y - 14))),
				Image(FString::Printf(TEXT("emblem_%s.png"), A.Id), FVector2f(56, 56)), ESlateDrawEffect::None, FLinearColor(1, 1, 1, 0.9f * D));
			Brackets(P.X, P.Y, 12.0f, Col(A.C, 0.7f * D));
		}
	}
	{
		const FVector3f J = JetPos(T);
		const float Al = (0.3f + 0.7f * Glow(Since(BrgOf(J.X, J.Y)))) * D;
		if (FVector2f::Distance(FVector2f(J.X, J.Y), FVector2f(SCX, SCY)) < R)
		{
			Symbol(4, J.X, J.Y, AIRC, Al);
			Symbol(4, J.X - 10, J.Y + 7, AIRC, Al * 0.8f);
			Vector(J.X, J.Y, J.Z, 46.0f, AIRC, Al);
			Labels.Add({ TEXT("F-22 ×2 · 480KT"), FVector2f(J.X + 12, J.Y - 9), Col(AIRC, Al * 0.8f) });
		}
	}
	{
		const FVector2f Hp = HeloPos(T);
		Symbol(5, Hp.X, Hp.Y, AIRC, (0.3f + 0.7f * Glow(Since(BrgOf(Hp.X, Hp.Y)))) * D);
	}
	// o objetivo
	{
		const float Al = (0.35f + 0.4f * Glow(Since(BrgOf(TOWN.X, TOWN.Y)))) * D;
		Lines(Out, LS, S, { FVector2f(TOWN.X, TOWN.Y - 11), FVector2f(TOWN.X + 11, TOWN.Y), FVector2f(TOWN.X, TOWN.Y + 11), FVector2f(TOWN.X - 11, TOWN.Y), FVector2f(TOWN.X, TOWN.Y - 11) },
			Col(FLASH, Al), 1.2f);
		Labels.Add({ TEXT("OBJ ALPHA"), FVector2f(TOWN.X + 16, TOWN.Y + 22), Col(FLASH, Al * 0.8f) });
	}
	// o míssil e o eco do impacto
	{
		const float Cm = FMath::Fmod(T, MIS_EVERY);
		static const int32 Red[3] = { 255, 90, 60 }, Red2[3] = { 255, 110, 80 };
		if (Cm <= MIS_FLY)
		{
			const float K = Cm / MIS_FLY;
			const FVector2f P = MIS_FROM + (TOWN - MIS_FROM) * K;
			Fill.Poly(Arc(P, 2.4f, 0.0f, TWO_PI_F, 12), ColV(Red, D));
			Vector(P.X, P.Y, FMath::Atan2(TOWN.X - MIS_FROM.X, -(TOWN.Y - MIS_FROM.Y)), 34.0f, Red, D);
			if (int32(T * 3.0f) % 2) Labels.Add({ TEXT("VAMPIRE"), FVector2f(P.X + 10, P.Y - 8), Col(Red2, D) });
		}
		const float SinceImp = Cm - MIS_FLY;
		if (SinceImp > 0.0f && SinceImp < 4.0f)
		{
			const float K = SinceImp / 4.0f;
			for (int32 I = 0; I < 3; ++I)
			{
				const float Kk = FMath::Max(0.0f, K - I * 0.12f);
				Lines(Out, LS, S, Arc(TOWN, 6.0f + Kk * 60.0f), Col(FLASH, (1.0f - Kk) * 0.5f * D), 1.2f);
			}
		}
	}
	Fill.Draw(Out, L, White());
	Dots.Draw(Out, L, White());
	for (const FLabel& Tx : Labels) Text(Out, L + 2, S, Tx.S, Mono11, Tx.P.X, Tx.P.Y, Tx.C);
	L += 3;
}

void SRTSMenu::PaintInstruments(FSlateWindowElementList& Out, int32& L, const FGeometry& S, float D) const
{
	// o A-scope: o eco ao longo do feixe agora, com o capim de ruído
	{
		const float X0 = 852, Y0 = 980, W = 218, H = 80;
		const int32 B = FMath::FloorToInt(Beam / TWO_PI_F * NB) % NB;
		for (int32 K = 1; K < 4; ++K) Lines(Out, L, S, { FVector2f(X0 + K * W / 4, Y0 - H), FVector2f(X0 + K * W / 4, Y0) }, Css(230, 220, 190, 0.06f));
		TArray<FVector2f> P;
		for (int32 I = 0; I < int32(W); ++I)
		{
			const int32 Ri = FMath::FloorToInt(I / W * NR);
			const float Sv = Ret[B * NR + Ri] * Gain, Grass = FMath::FRand() * (0.06f + Level * 0.25f);
			P.Add(FVector2f(X0 + I, Y0 - 4 - FMath::Min(1.0f, Sv * 0.9f + Grass) * H * 0.85f));
		}
		Lines(Out, L, S, P, Col(AMB, 0.75f * D), 1.2f);
		Text(Out, L, S, FString::Printf(TEXT("BRG %s"), *Pad3(FMath::RoundToInt(Beam / D2R))), Font(EFace::Mono500, 10.0f), X0 + 150, Y0 - H + 2, Col(AMB, 0.45f * D));
	}
	// a cascata do ESM
	if (WfBrush.IsValid())
	{
		FSlateDrawElement::MakeBox(Out, L, S.ToPaintGeometry(FVector2f(210, 98), FSlateLayoutTransform(FVector2f(1632, 892))), WfBrush.Get(),
			ESlateDrawEffect::None, FLinearColor(1, 1, 1, 0.7f * D));
	}
	// a lista das trilhas
	{
		const FSlateFontInfo F = Font(EFace::Mono500, 10.5f);
		float Y = 120;
		Text(Out, L, S, TEXT("ID    SIDE BRG  RNG   SPD"), F, 1638, Y, Col(AMB, 0.3f * D));
		Y += 15;
		const auto Row = [&](const FString& Id, const FString& Side, float Brg, float Rng, int32 Spd, const int32 Cc[3])
		{
			Text(Out, L, S, FString::Printf(TEXT("%s  %-4s %s  %4.1f  %3d"), *Id, *Side, *Pad3(FMath::RoundToInt(Brg)), Rng, Spd), F, 1638, Y, Col(Cc, 0.6f * D));
			Y += 14;
		};
		for (int32 I = 0; I < 4; ++I)
		{
			const FVector3f P = ArmyPos(I, T);
			Row(FString::Printf(TEXT("T0%d"), 14 + I * 7), FString(ARMY[I].Id).Left(3).ToUpper(), BrgOf(P.X, P.Y) / D2R, FVector2f::Distance(FVector2f(P.X, P.Y), FVector2f(SCX, SCY)) / KM, ARMY[I].Spd, ARMY[I].C);
		}
		const FVector3f J = JetPos(T);
		Row(TEXT("T051"), TEXT("AIR"), BrgOf(J.X, J.Y) / D2R, FVector2f::Distance(FVector2f(J.X, J.Y), FVector2f(SCX, SCY)) / KM, 480, AIRC);
	}
	// o relógio
	{
		const FDateTime U = FDateTime::UtcNow();
		Text(Out, L, S, FString::Printf(TEXT("%02d:%02d:%02dZ"), U.GetHour(), U.GetMinute(), U.GetSecond()), Font(EFace::Mono500, 22.0f), 842, 108, Col(AMB, 0.7f * D));
		const FSlateFontInfo F = Font(EFace::Mono500, 10.5f);
		Text(Out, L, S, FString::Printf(TEXT("PPI · %d KM · %.1f RPM"), int32(RANGE), 60.0f / PERIOD), F, 842, 128, Col(AMB, 0.38f * D));
		Text(Out, L, S, TEXT("MODE 3/C · IFF ON"), F, 842, 143, Col(AMB, 0.38f * D));
	}
	// os dois botões do tubo: ganho e brilho (o OPTIONS mexe neles)
	for (int32 I = 0; I < 2; ++I)
	{
		const FVector2f P = DIALS[I];
		Lines(Out, L, S, Arc(P, 18.0f), Col(AMB, 0.35f * D));
		for (int32 K = 0; K <= 10; ++K)
		{
			const float A = (-135.0f + K * 27.0f) * D2R;
			Lines(Out, L, S, { FVector2f(P.X + FMath::Sin(A) * 21, P.Y - FMath::Cos(A) * 21), FVector2f(P.X + FMath::Sin(A) * 24, P.Y - FMath::Cos(A) * 24) }, Col(AMB, 0.35f * D));
		}
		const float A = (-30.0f + Knob * (I ? -70.0f : 80.0f) + I * 50.0f) * D2R;
		Lines(Out, L, S, { P, FVector2f(P.X + FMath::Sin(A) * 14, P.Y - FMath::Cos(A) * 14) }, Col(FLASH, 0.7f * D), 2.0f);
		Text(Out, L, S, I ? TEXT("BRILL") : TEXT("GAIN"), Font(EFace::Mono600, 9.5f), P.X, P.Y + 36, Col(AMB, 0.35f * D), 1);
	}
	L += 2;
}

void SRTSMenu::PaintLink(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	// a linha do item até o ponto do radar (a aprovada): sai do fim do título, dobra e chega num alvo que pulsa
	if (Hover < 0 || Hover > 3)
	{
		return;
	}
	float Top[4], Extra[4];
	ItemLayout(Top, Extra);
	const float P = ItemSlide[Hover].Value(Now);
	const FSlateFontInfo F = Font(EFace::Shoulders800, 40.0f, 3.0f + 2.0f * P);
	const float X0 = 174.0f + 8.0f * P + TextWidth(ItemTitle[Hover], F, S.GetAccumulatedLayoutTransform().GetScale()) + 20.0f, Y0 = Top[Hover] + 14.0f + 20.0f;
	FVector2f A;
	switch (Hover)
	{
	case HitSkirmish: A = TOWN; break;
	case HitMulti: { const FVector3f U = ArmyPos(0, T); A = FVector2f(U.X, U.Y); break; }
	case HitOptions: A = DIALS[0]; break;
	default: A = FVector2f(SCX, SCY); break;
	}
	Dashed(Out, L, S, { FVector2f(X0, Y0), FVector2f(X0 + 50, Y0), FVector2f(A.X - 40, A.Y), FVector2f(A.X - 24, A.Y) }, Css(216, 173, 78, 0.85f), 2.0f, 5.0f);
	Lines(Out, L, S, Arc(A, 20.0f), FLinearColor(1, 1, 1, 0.85f), 2.0f);
	const float Ph = FMath::Fmod(float(FPlatformTime::Seconds()) / 1.2f, 1.0f);
	Lines(Out, L, S, Arc(A, 20.0f + Ph * 26.0f), Css(216, 173, 78, (1.0f - Ph) * 0.5f), 1.5f);
	FMesh M(S);
	M.Poly(Arc(A, 3.5f, 0.0f, TWO_PI_F, 14), CssV(216, 173, 78, 1.0f));
	M.Draw(Out, L + 1, White());
	L += 2;
}

void SRTSMenu::PaintSide(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	// o lado esquerdo (.side): as bases medidas no Chrome (menu_layout.json)
	// (04/11) sem o título e sem o kicker: o "FOUR ARMIES. ONE MAP." e o "OPERATIONS · THEATER 04" eram provisórios ("o título tem
	// que ir embora"; o kicker "sem criatividade"). O jogo ainda não tem nome: o alto da coluna fica livre pro nome e o logo
	const FSlateFontInfo Sub = Font(EFace::Barlow500, 18.0f, 1.0f);
	Text(Out, L, S, TEXT("The ground changes as you fight: craters, ruins,"), Sub, 110, 375.9f, Css(152, 161, 154));
	Text(Out, L, S, TEXT("burned palms. Read it, then take it."), Sub, 110, 400.2f, Css(152, 161, 154));
	// a lista: a linha de cima de cada item, o número, a barrinha que acende, o título que anda e a descrição que abre
	float Top[4], Extra[4];
	ItemLayout(Top, Extra);
	FMesh Lines1(S);
	for (int32 I = 0; I < 4; ++I)
	{
		Lines1.Rect(110, Top[I], 540, 1, CssV(230, 220, 190, 0.10f));
	}
	Lines1.Rect(110, Top[3] + ItemH + Extra[3], 540, 1, CssV(230, 220, 190, 0.10f));
	Lines1.Draw(Out, L++, White());
	for (int32 I = 0; I < 4; ++I)
	{
		const float Y = Top[I];
		const float Cn = ItemNum[I].Value(Now);
		const FLinearColor NumC = FMath::Lerp(Css(230, 220, 190, 0.42f), Css(216, 173, 78), Cn);
		Text(Out, L, S, FString::Printf(TEXT("0%d"), I + 1), Font(EFace::Barlow600, 16.0f, 2.0f), 110, Y + 37.0f, NumC);
		// a barrinha âmbar com o brilho (box-shadow 0 0 10px): três camadas mais largas e fracas por baixo
		const float Bw = 18.0f * ItemBar[I].Value(Now);
		if (Bw > 0.2f)
		{
			FMesh Bar(S);
			for (const FVector2f G : { FVector2f(7, 0.05f), FVector2f(4, 0.10f), FVector2f(2, 0.20f) })
			{
				Bar.Rect(144 - G.X, Y + 33 - G.X, Bw + 2 * G.X, 3 + 2 * G.X, CssV(216, 173, 78, 0.85f * G.Y));
			}
			Bar.Rect(144, Y + 33, Bw, 3, CssV(216, 173, 78, 1.0f));
			Bar.Draw(Out, L, White());
		}
		const float Sl = ItemSlide[I].Value(Now), Cc = ItemColor[I].Value(Now);
		const FLinearColor TitleC = FMath::Lerp(Css(238, 241, 244, 0.62f), FLinearColor::White, Cc);
		Text(Out, L + 1, S, ItemTitle[I], Font(EFace::Shoulders800, 40.0f, 3.0f + 2.0f * Sl), 174.0f + 8.0f * Sl, Y + 49.0f, TitleC);
		const float Open = ItemOpen[I].Value(Now);
		if (Open > 0.01f)
		{
			const float DTop = Y + 54.0f + 6.0f * Open, DH = 24.0f * Open;
			Out.PushClip(FSlateClippingZone(S.ToPaintGeometry(FVector2f(476, DH), FSlateLayoutTransform(FVector2f(174, DTop)))));
			Text(Out, L + 1, S, ItemDesc[I], Font(EFace::Barlow500, 17.0f, 1.0f), 182.0f, DTop + 17.0f, Css(152, 161, 154));
			Out.PopClip();
		}
	}
	L += 2;
	// o bloco do desenvolvimento (LAB e SCRIPT): ciano de planta, borda tracejada, fora da lista do jogo
	if (bDev)
	{
		const float Dy = Extra[0] + Extra[1] + Extra[2] + Extra[3];
		const float Y = DevY0 + Dy;
		Text(Out, L, S, TEXT("DEVELOPMENT"), Font(EFace::Barlow600, 12.0f, 6.0f), 110, Y + 14.0f, Css(120, 190, 220, 0.85f));
		const FVector4f TagBox(258.94f, Y, 144.31f, 18.0f);
		const FLinearColor TagC = Css(120, 190, 220, 0.55f);
		Dashed(Out, L, S, { FVector2f(TagBox.X + 0.5f, TagBox.Y + 0.5f), FVector2f(TagBox.X + TagBox.Z - 0.5f, TagBox.Y + 0.5f), FVector2f(TagBox.X + TagBox.Z - 0.5f, TagBox.Y + TagBox.W - 0.5f),
			FVector2f(TagBox.X + 0.5f, TagBox.Y + TagBox.W - 0.5f), FVector2f(TagBox.X + 0.5f, TagBox.Y + 0.5f) }, TagC, 1.0f, 3.0f);
		Text(Out, L, S, TEXT("NOT IN THE RELEASE"), Font(EFace::Barlow700, 10.0f, 2.5f), TagBox.X + 8.0f, Y + 13.0f, Css(120, 190, 220, 0.75f));
		Dashed(Out, L, S, { FVector2f(TagBox.X + TagBox.Z + 12.0f, Y + 9.0f), FVector2f(650.0f, Y + 9.0f) }, Css(120, 190, 220, 0.35f), 1.0f, 3.0f);
		for (int32 K = 0; K < 2; ++K)
		{
			const float X = K == 0 ? 110.0f : 385.0f, Ty = TileY0 + Dy, W = TileW, H = TileH;
			const float Hv = TileHover[K].Value(Now), HvL = FMath::Clamp(Hv, 0.0f, 1.0f);
			FMesh M(S);
			M.Rect(X, Ty, W, H, Hv > 0.5f ? CssV(14, 34, 40, 0.7f) : CssV(10, 22, 26, 0.55f));
			for (float Gx = X + 1.0f; Gx < X + W - 1.0f; Gx += 12.0f) M.Rect(Gx, Ty + 1, 1, H - 2, CssV(120, 190, 220, 0.05f));
			for (float Gy = Ty + 1.0f; Gy < Ty + H - 1.0f; Gy += 12.0f) M.Rect(X + 1, Gy, W - 2, 1, CssV(120, 190, 220, 0.05f));
			M.Draw(Out, L, White());
			const TArray<FVector2f> Box = { FVector2f(X + 0.5f, Ty + 0.5f), FVector2f(X + W - 0.5f, Ty + 0.5f), FVector2f(X + W - 0.5f, Ty + H - 0.5f), FVector2f(X + 0.5f, Ty + H - 0.5f), FVector2f(X + 0.5f, Ty + 0.5f) };
			if (HvL > 0.5f) Lines(Out, L + 1, S, Box, Css(120, 190, 220, 0.85f));
			else Dashed(Out, L + 1, S, Box, Css(120, 190, 220, 0.32f), 1.0f, 3.0f);
			// o ícone (36 × 36 do svg, traço de 1,8): cresce 12% e gira −6° no hover, com o pulo da curva
			const float Sc = 1.0f + 0.12f * Hv, Rot = -6.0f * D2R * Hv;
			const FVector2f Ic(X + 1 + 16 + 18, Ty + 1 + 18 + 18);
			const auto Tf = [&](float Px, float Py) { const float Dx = (Px - 18) * Sc, Dy2 = (Py - 18) * Sc; return FVector2f(Ic.X + Dx * FMath::Cos(Rot) - Dy2 * FMath::Sin(Rot), Ic.Y + Dx * FMath::Sin(Rot) + Dy2 * FMath::Cos(Rot)); };
			const FLinearColor IcC = FMath::Lerp(Css(120, 190, 220, 0.8f), Css(150, 215, 240), HvL);
			const FLinearColor IcF = FLinearColor(IcC.R, IcC.G, IcC.B, IcC.A * 0.45f);
			const float Lw = 1.8f * Sc;
			if (K == 0)
			{
				Lines(Out, L + 1, S, { Tf(4, 4), Tf(32, 4), Tf(32, 32), Tf(4, 32), Tf(4, 4) }, IcC, Lw);
				for (const float V : { 13.0f, 23.0f })
				{
					Lines(Out, L + 1, S, { Tf(4, V), Tf(32, V) }, IcF, Lw);
					Lines(Out, L + 1, S, { Tf(V, 4), Tf(V, 32) }, IcF, Lw);
				}
				TArray<FVector2f> Cir;
				for (int32 I = 0; I <= 24; ++I) { const float A = TWO_PI_F * I / 24; Cir.Add(Tf(18 + FMath::Cos(A) * 5.5f, 18 + FMath::Sin(A) * 5.5f)); }
				Lines(Out, L + 1, S, Cir, IcC, Lw);
				Lines(Out, L + 1, S, { Tf(18, 9), Tf(18, 13) }, IcC, Lw);
				Lines(Out, L + 1, S, { Tf(18, 23), Tf(18, 27) }, IcC, Lw);
				Lines(Out, L + 1, S, { Tf(9, 18), Tf(13, 18) }, IcC, Lw);
				Lines(Out, L + 1, S, { Tf(23, 18), Tf(27, 18) }, IcC, Lw);
			}
			else
			{
				Lines(Out, L + 1, S, { Tf(7, 5), Tf(23, 5), Tf(29, 11), Tf(29, 31), Tf(7, 31), Tf(7, 5) }, IcC, Lw);
				Lines(Out, L + 1, S, { Tf(23, 5), Tf(23, 11), Tf(29, 11) }, IcC, Lw);
				FMesh Tri(S);
				const FLinearColor Lc = IcC;
				const FColor Tc = FLinearColor(Lc).ToFColor(true);
				Tri.Poly({ Tf(15, 15), Tf(23, 20), Tf(15, 25) }, Tc);
				Tri.Draw(Out, L + 1, White());
			}
			const float Ls = 3.0f + 2.0f * FMath::Clamp(Hv, 0.0f, 1.2f);
			Text(Out, L + 1, S, K == 0 ? TEXT("LAB") : TEXT("SCRIPT"), Font(EFace::Shoulders800, 30.0f, Ls), X + 67.0f, Ty + 42.0f,
				FMath::Lerp(Css(238, 241, 244, 0.8f), FLinearColor::White, HvL));
			Text(Out, L + 1, S, K == 0 ? TEXT("Sandbox world · test live") : TEXT("Ready-made test match"), Font(EFace::Barlow500, 14.0f, 1.0f), X + 67.0f, Ty + 65.0f, Css(152, 161, 154, 0.95f));
			if (K == 1)
			{
				const bool bArmed = !Armed.IsEmpty();
				FString Em = bArmed ? TEXT("▸ ") + Armed : Scenes;
				const FSlateFontInfo EmF = Font(EFace::Mono600, 11.0f);
				while (Em.Len() > 3 && TextWidth(Em, EmF, S.GetAccumulatedLayoutTransform().GetScale()) > 150.0f) Em = Em.LeftChop(2) + TEXT("…");
				Text(Out, L + 1, S, Em, EmF, 639.0f, Ty + 22.0f, bArmed ? Css(216, 173, 78, 0.95f) : Css(120, 190, 220, 0.7f), 2);
			}
		}
		L += 2;
	}
	Text(Out, L, S, TEXT("v0.5.2"), Font(EFace::Barlow500, 15.0f, 2.0f), 110, 1033.0f, Css(230, 220, 190, 0.38f));
	// as cantoneiras do palco
	FMesh Cor(S);
	const FColor Cc = CssV(216, 173, 78, 0.45f);
	Cor.Rect(40, 40, 22, 2, Cc); Cor.Rect(40, 40, 2, 22, Cc);
	Cor.Rect(1858, 40, 22, 2, Cc); Cor.Rect(1878, 40, 2, 22, Cc);
	Cor.Rect(40, 1038, 22, 2, Cc); Cor.Rect(40, 1018, 2, 22, Cc);
	Cor.Rect(1858, 1038, 22, 2, Cc); Cor.Rect(1878, 1018, 2, 22, Cc);
	Cor.Draw(Out, L++, White());
}

void SRTSMenu::PaintSideReveal(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	// depois do golpe o lado esquerdo monta de cima pra baixo, faixa por faixa, cada uma escrita da esquerda pra direita como a
	// linha do tubo (o recorte anda e deixa a cabeça de escrita âmbar na frente)
	const float A = BootT - BootReveal;
	if (A <= 0.0f)
	{
		return;
	}
	struct FBand { float Y0, Y1, Delay; bool bFull; };
	static const FBand Bands[] = { { 30, 72, 0.0f, true }, { 140, 178, 0.05f, false }, { 178, 345, 0.12f, false }, { 345, 412, 0.2f, false },
		{ 450, 520.5f, 0.28f, false }, { 520.5f, 587.5f, 0.34f, false }, { 587.5f, 654.5f, 0.4f, false }, { 654.5f, 725, 0.46f, false },
		{ 740, 900, 0.56f, false }, { 1005, 1050, 0.62f, true } };
	FMesh Head(S);
	for (const FBand& B : Bands)
	{
		const float Ab = Smooth((A - 0.1f - B.Delay) / 0.45f);
		if (Ab <= 0.0f)
		{
			continue;
		}
		const float X1 = Ab >= 1.0f ? StageW : (B.bFull ? StageW * Ab : 100.0f + 570.0f * Ab);
		Out.PushClip(FSlateClippingZone(S.ToPaintGeometry(FVector2f(X1, B.Y1 - B.Y0), FSlateLayoutTransform(FVector2f(0.0f, B.Y0)))));
		PaintSide(Out, L, S);
		Out.PopClip();
		if (Ab < 1.0f)
		{
			Head.Rect(X1 - 1.0f, B.Y0, 2.0f, B.Y1 - B.Y0, ColV(AMB, 0.5f * (1.0f - Ab)));
		}
	}
	Head.Draw(Out, L++, White());
}

void SRTSMenu::PaintBootCover(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	// o escuro por cima de tudo: inteiro até o tubo começar a acender; dali, com o furo do tubo (a ignição aparece); com o golpe,
	// some em 0,5 s e o painel e os instrumentos aparecem
	const float A = BootT - BootReveal;
	const float G = A > 0.0f ? 1.0f - Smooth(A / 0.5f) : 1.0f;
	if (G <= 0.001f)
	{
		return;
	}
	const FColor C = CssV(10, 13, 11, G);
	FMesh M(S);
	if (BootT < BootReveal - 1.0f)
	{
		M.Rect(0.0f, 0.0f, StageW, StageH, C);
	}
	else
	{
		// o anel entre o tubo e a borda do palco: um raio por ângulo (mais os quatro cantos, pra borda sair reta)
		const FVector2f Ctr(SCX, SCY);
		TArray<float> Ang;
		for (int32 I = 0; I < 96; ++I)
		{
			Ang.Add(TWO_PI_F * I / 96.0f - PI);
		}
		for (const FVector2f Cn : { FVector2f(0, 0), FVector2f(StageW, 0), FVector2f(StageW, StageH), FVector2f(0, StageH) })
		{
			Ang.Add(FMath::Atan2(Cn.Y - Ctr.Y, Cn.X - Ctr.X));
		}
		Ang.Sort();
		const auto Edge = [&](float An)
		{
			const FVector2f D(FMath::Cos(An), FMath::Sin(An));
			float T = 1e9f;
			if (D.X > 1e-6f) T = FMath::Min(T, (StageW - Ctr.X) / D.X);
			if (D.X < -1e-6f) T = FMath::Min(T, -Ctr.X / D.X);
			if (D.Y > 1e-6f) T = FMath::Min(T, (StageH - Ctr.Y) / D.Y);
			if (D.Y < -1e-6f) T = FMath::Min(T, -Ctr.Y / D.Y);
			return Ctr + D * T;
		};
		for (int32 I = 0; I < Ang.Num(); ++I)
		{
			const float A0 = Ang[I], A1 = Ang[(I + 1) % Ang.Num()] + (I + 1 == Ang.Num() ? TWO_PI_F : 0.0f);
			const int32 P0 = M.Vert(Ctr + FVector2f(FMath::Cos(A0), FMath::Sin(A0)) * (R + 3.0f), FVector2f(0, 0), C);
			const int32 P1 = M.Vert(Edge(A0), FVector2f(0, 0), C);
			const int32 P2 = M.Vert(Edge(A1), FVector2f(0, 0), C);
			const int32 P3 = M.Vert(Ctr + FVector2f(FMath::Cos(A1), FMath::Sin(A1)) * (R + 3.0f), FVector2f(0, 0), C);
			M.Tri(P0, P1, P2);
			M.Tri(P0, P2, P3);
		}
	}
	M.Draw(Out, L++, White());
}

void SRTSMenu::PaintBootLines(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	// a checagem do console, na coluna onde o menu vai montar: o cabeçalho acende com o relé, cada linha se escreve, os pontos
	// correm até o estado e o cursor pisca no fim; tudo apaga na hora do golpe
	const float Fade = 1.0f - Smooth((BootT - (BootReveal - 0.2f)) / 0.5f);
	if (Fade <= 0.0f || BootT < BootRelayAt)
	{
		return;
	}
	const FSlateFontInfo Head = Font(EFace::Mono600, 12.0f, 3.0f), Row = Font(EFace::Mono500, 13.0f);
	const FLinearColor Amber = Css(216, 173, 78, 0.85f * Fade), Faint = Css(216, 173, 78, 0.4f * Fade), Ok = Css(230, 220, 190, 0.9f * Fade);
	const float X = 110.0f, Y0 = 470.0f, Lh = 26.0f;
	Text(Out, L, S, TEXT("C2 CONSOLE  ·  THEATER 04  ·  COLD START"), Head, X, Y0 - 40.0f, Faint);
	float CursorX = X, CursorY = Y0;
	for (int32 I = 0; I < BootLines; ++I)
	{
		const float Tl = BootT - (BootLine0 + I * BootLineDt);
		if (Tl < 0.0f)
		{
			break;
		}
		const FString Label = BootLabel[I];
		const float Y = Y0 + I * Lh;
		const int32 NChars = FMath::Clamp(FMath::CeilToInt(Tl / 0.1f * Label.Len()), 0, Label.Len());
		CursorX = X + Text(Out, L, S, Label.Left(NChars), Row, X, Y, Amber) + 4.0f;
		CursorY = Y;
		const float Dots = Smooth((Tl - 0.08f) / 0.08f);
		if (Dots > 0.0f)
		{
			CursorX = X + 150.0f + Text(Out, L, S, FString::ChrN(FMath::Max(1, int32(24 * Dots)), TEXT('.')), Row, X + 150.0f, Y, Faint) + 4.0f;
		}
		if (Tl >= BootLineDt * 0.6f)
		{
			FString State = BootState[I] ? BootState[I] : TEXT("");
			if (!BootState[I])
			{
				// os shaders: a porcentagem de verdade (no editor e depois da 1ª vez, 100%)
				const int32 Pct = BootPsoMax > 0 ? FMath::RoundToInt(100.0f * float(BootPsoMax - BootPso) / float(BootPsoMax)) : 100;
				State = FString::Printf(TEXT("%d%%"), Pct);
			}
			CursorX = X + 352.0f + Text(Out, L, S, State, Row, X + 352.0f, Y, Ok) + 4.0f;
		}
	}
	// o cursor de bloco piscando (2 vezes por segundo) até o tubo acender
	if (BootT < BootReveal - 1.0f && FMath::Fmod(BootT * 2.0f, 1.0f) < 0.55f)
	{
		FMesh Cur(S);
		Cur.Rect(CursorX, CursorY - 12.0f, 8.0f, 14.0f, ColV(AMB, 0.8f * Fade));
		Cur.Draw(Out, L++, White());
	}
}
