#include "SRTSOptions.h"
#include "RTSShell.h"
#include "RTSShellAudio.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"

using namespace RTSShellUI;

// As medidas do Chrome (Tools/options_layout.json). Num namespace com nome e sempre com o prefixo (a compilação unificada da Unreal
// junta os .cpp do módulo e os nomes anônimos colidem).
namespace RTSOptScreen
{
	const TCHAR* TAB_T[5] = { TEXT("GRAPHICS"), TEXT("SOUND"), TEXT("CONTROLS"), TEXT("GAME"), TEXT("ACCESSIBILITY") };
	const TCHAR* PRESET_N[6] = { TEXT("CLASSIC"), TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("ULTRA"), TEXT("AUTO") };
	const TCHAR* PRESET_D[6] = { TEXT("THE 2003 LOOK · CRISP, VIVID"), TEXT("PCS FROM 2014"), TEXT("BALANCED"), TEXT("FULL VOLUME SMOKE"), TEXT("EVERYTHING ON"), TEXT("MEASURED ON THIS PC") };
	struct FKey { const TCHAR* Action; const TCHAR* Key; };
	const FKey KEYS[10] = { { TEXT("SELECT ALL UNITS"), TEXT("Q") }, { TEXT("ATTACK MOVE"), TEXT("A") }, { TEXT("FORCE FIRE"), TEXT("CTRL") }, { TEXT("STOP"), TEXT("S") },
		{ TEXT("GUARD"), TEXT("G") }, { TEXT("SCATTER"), TEXT("X") }, { TEXT("CENTER ON BASE"), TEXT("H") }, { TEXT("NEXT IDLE WORKER"), TEXT(".") }, { TEXT("CHAT"), TEXT("ENTER") },
		{ TEXT("GENERAL POWERS"), TEXT("G") } };
	// a coluna da esquerda, a área que rola, o cartão e os botões
	constexpr float TAB_Y0 = 211.23f, TAB_STEP = 72.0f;
	constexpr float MAIN_X = 430.0f, MAIN_Y = 96.0f, MAIN_W = 960.0f, MAIN_H = 844.0f;
	constexpr float ROW_X = 430.0f, ROW_W = 960.0f, SEG_X = 725.0f;
	constexpr float INFO_X = 1450.0f, INFO_W = 400.0f;
	const FVector4f RESET(1358, 966, 240, 64), APPLY(1610, 966, 240, 64);
	// o preset AUTO mede este PC e escolheu o HIGH (a página)
	constexpr int32 AUTO_AS = 3;
	// os volumes (o RTSShellAudio põe no mix; aqui só os itens e o guardado)
	const TCHAR* VOL_KEYS[6] = { TEXT("master"), TEXT("music"), TEXT("sfx"), TEXT("voice"), TEXT("ui"), TEXT("amb") };

	FLinearColor Mute() { return Css(152, 161, 154); }
	/** A quebra de linha do CSS (palavra por palavra, na largura dada). */
	TArray<FString> Wrap(const FString& Text, const FSlateFontInfo& F, float W, float Scale)
	{
		TArray<FString> Words, Lines;
		Text.ParseIntoArray(Words, TEXT(" "));
		FString Cur;
		for (const FString& Wd : Words)
		{
			const FString Try = Cur.IsEmpty() ? Wd : Cur + TEXT(" ") + Wd;
			if (!Cur.IsEmpty() && TextWidth(Try, F, Scale) > W) { Lines.Add(Cur); Cur = Wd; }
			else Cur = Try;
		}
		if (!Cur.IsEmpty()) Lines.Add(Cur);
		return Lines;
	}
	/** O número como o JavaScript imprime (4.5, 5, 5.3). */
	FString JsNum(float V)
	{
		const float R = FMath::RoundToFloat(V * 10.0f) / 10.0f;
		return FMath::IsNearlyEqual(R, FMath::RoundToFloat(R)) ? FString::FromInt(FMath::RoundToInt(R)) : FString::Printf(TEXT("%.1f"), R);
	}
}
namespace OpS = RTSOptScreen;

void SRTSOptions::Construct(const FArguments& InArgs)
{
	const auto Add = [this](int32 T, const TCHAR* G, const TCHAR* K, const TCHAR* N, TArray<const TCHAR*> O, int32 Def, const TCHAR* D) -> FItem&
	{
		FItem I;
		I.Group = G; I.Key = K; I.Name = N; I.Opts = MoveTemp(O); I.Def = Def; I.Desc = D;
		return Tabs[T].Add_GetRef(MoveTemp(I));
	};
	// as tabelas da página (gerado das GFX, SND, CTL, GAMEO e ACC do screens.html pelo Tools/gen_options_tables.mjs e .py)
	// GFX
	{ FItem& I = Add(0, TEXT("DISPLAY"), TEXT("res"), TEXT("RESOLUTION"), { TEXT("1280×720"), TEXT("1600×900"), TEXT("1920×1080"), TEXT("2560×1440"), TEXT("3840×2160") }, 2, TEXT("How many pixels the game draws. Higher is sharper and costs more of everything.")); I.bFixed = true; }
	{ FItem& I = Add(0, TEXT("DISPLAY"), TEXT("mode"), TEXT("DISPLAY MODE"), { TEXT("FULLSCREEN"), TEXT("BORDERLESS"), TEXT("WINDOWED") }, 0, TEXT("Fullscreen is the fastest. Borderless lets you switch windows instantly.")); I.bFixed = true; }
	{ FItem& I = Add(0, TEXT("DISPLAY"), TEXT("fps"), TEXT("FRAME LIMIT"), { TEXT("30"), TEXT("60"), TEXT("120"), TEXT("144"), TEXT("OFF") }, 1, TEXT("The most frames per second the game will draw. 60 keeps the card cool and steady.")); I.bFixed = true; }
	{ FItem& I = Add(0, TEXT("DISPLAY"), TEXT("vsync"), TEXT("V-SYNC"), { TEXT("OFF"), TEXT("ON") }, 0, TEXT("Stops tearing by waiting for the monitor. Adds a little input delay.")); I.bFixed = true; }
	{ FItem& I = Add(0, TEXT("DISPLAY"), TEXT("bright"), TEXT("BRIGHTNESS"), TArray<const TCHAR*>(), 50, TEXT("Adjust until the darkest shadow detail is barely visible.")); I.Min = 0; I.Max = 100; I.bFixed = true; }
	{ FItem& I = Add(0, TEXT("DISPLAY"), TEXT("hdr"), TEXT("HDR"), { TEXT("OFF"), TEXT("ON") }, 0, TEXT("High dynamic range for HDR monitors. No HDR monitor was found.")); I.bFixed = true; I.Na = { 1 }; }
	{ FItem& I = Add(0, TEXT("UPSCALING & PERFORMANCE"), TEXT("up"), TEXT("UPSCALER"), { TEXT("OFF"), TEXT("TSR"), TEXT("FSR 3"), TEXT("DLSS"), TEXT("XeSS") }, 1, TEXT("Draws fewer pixels and rebuilds the image. TSR is Unreal's own; DLSS needs an RTX card.")); I.Preset = { 0, 2, 1, 1, 1 }; I.Na = { 3 }; }
	{ FItem& I = Add(0, TEXT("UPSCALING & PERFORMANCE"), TEXT("upq"), TEXT("UPSCALER QUALITY"), { TEXT("PERFORMANCE"), TEXT("BALANCED"), TEXT("QUALITY"), TEXT("NATIVE") }, 2, TEXT("How much the upscaler cuts. Native draws every pixel and only smooths edges.")); I.Preset = { 3, 0, 1, 2, 3 }; }
	{ FItem& I = Add(0, TEXT("UPSCALING & PERFORMANCE"), TEXT("fg"), TEXT("FRAME GENERATION"), { TEXT("OFF"), TEXT("ON") }, 0, TEXT("Invents a frame between two real ones. Smoother, but adds delay: not for competitive play.")); I.Preset = { 0, 0, 0, 0, 0 }; }
	{ FItem& I = Add(0, TEXT("UPSCALING & PERFORMANCE"), TEXT("lat"), TEXT("LOW LATENCY"), { TEXT("OFF"), TEXT("ON"), TEXT("ON + BOOST") }, 1, TEXT("Makes the card answer your clicks sooner.")); I.Preset = { 1, 1, 1, 1, 1 }; }
	{ FItem& I = Add(0, TEXT("IMAGE"), TEXT("aa"), TEXT("ANTI-ALIASING"), { TEXT("OFF"), TEXT("FXAA"), TEXT("TAA"), TEXT("TSR") }, 3, TEXT("Smooths jagged edges. TSR is the sharpest in motion.")); I.Preset = { 1, 1, 2, 3, 3 }; I.W = 1.2f; }
	{ FItem& I = Add(0, TEXT("IMAGE"), TEXT("sharp"), TEXT("SHARPENING"), { TEXT("OFF"), TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH") }, 1, TEXT("Brings back detail lost to smoothing and upscaling.")); I.Preset = { 3, 1, 1, 1, 1 }; }
	{ FItem& I = Add(0, TEXT("IMAGE"), TEXT("tex"), TEXT("TEXTURES"), { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("ULTRA") }, 2, TEXT("Surface detail of units, buildings and ground. Mostly costs video memory.")); I.Preset = { 1, 0, 1, 2, 3 }; I.W = 0.3f; I.M = 2.2f; }
	{ FItem& I = Add(0, TEXT("IMAGE"), TEXT("aniso"), TEXT("TEXTURE FILTERING"), { TEXT("2X"), TEXT("4X"), TEXT("8X"), TEXT("16X") }, 3, TEXT("Keeps the ground sharp toward the horizon of the camera.")); I.Preset = { 1, 0, 1, 3, 3 }; I.W = 0.3f; }
	{ FItem& I = Add(0, TEXT("IMAGE"), TEXT("unit"), TEXT("UNIT DETAIL"), { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("ULTRA") }, 2, TEXT("How far away units keep their full model. Big battles cost more.")); I.Preset = { 1, 0, 1, 2, 3 }; I.W = 1.4f; I.M = 0.4f; }
	{ FItem& I = Add(0, TEXT("LIGHTING & SHADOWS"), TEXT("gi"), TEXT("GLOBAL ILLUMINATION"), { TEXT("OFF"), TEXT("LOW"), TEXT("HIGH"), TEXT("EPIC") }, 2, TEXT("Light that bounces: fire glowing on walls, sand lighting the shade.")); I.Preset = { 0, 0, 1, 2, 3 }; I.W = 5.5f; I.M = 0.5f; }
	{ FItem& I = Add(0, TEXT("LIGHTING & SHADOWS"), TEXT("shadow"), TEXT("SHADOWS"), { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("ULTRA") }, 2, TEXT("Sharpness and reach of the shadows of the sun and of the fires.")); I.Preset = { 0, 0, 1, 2, 3 }; I.W = 3.2f; I.M = 0.3f; }
	{ FItem& I = Add(0, TEXT("LIGHTING & SHADOWS"), TEXT("contact"), TEXT("CONTACT SHADOWS"), { TEXT("OFF"), TEXT("ON") }, 1, TEXT("The thin shadow where a tank touches the ground.")); I.Preset = { 0, 0, 1, 1, 1 }; I.W = 0.5f; }
	{ FItem& I = Add(0, TEXT("LIGHTING & SHADOWS"), TEXT("ao"), TEXT("AMBIENT OCCLUSION"), { TEXT("OFF"), TEXT("SSAO"), TEXT("GTAO") }, 2, TEXT("Soft darkening in corners, under units and between buildings.")); I.Preset = { 0, 0, 1, 2, 2 }; I.W = 1.0f; }
	{ FItem& I = Add(0, TEXT("LIGHTING & SHADOWS"), TEXT("refl"), TEXT("REFLECTIONS"), { TEXT("OFF"), TEXT("SCREEN SPACE"), TEXT("LUMEN") }, 1, TEXT("Water and wet ground reflecting fires and the sky.")); I.Preset = { 0, 0, 1, 1, 2 }; I.W = 1.8f; }
	{ FItem& I = Add(0, TEXT("LIGHTING & SHADOWS"), TEXT("sky"), TEXT("SKY & ATMOSPHERE"), { TEXT("LOW"), TEXT("HIGH"), TEXT("ULTRA") }, 1, TEXT("The haze, the sun glow and the color of the distance.")); I.Preset = { 0, 0, 1, 1, 2 }; I.W = 0.8f; }
	{ FItem& I = Add(0, TEXT("EFFECTS"), TEXT("boom"), TEXT("EXPLOSIONS"), { TEXT("VOLUME LOW"), TEXT("VOLUME MEDIUM"), TEXT("VOLUME HIGH"), TEXT("VOLUME ULTRA") }, 2, TEXT("Our explosions are real volumes of fire and smoke. Higher adds detail and steps.")); I.Preset = { 0, 0, 1, 2, 3 }; I.W = 4.5f; I.M = 0.6f; }
	{ FItem& I = Add(0, TEXT("EFFECTS"), TEXT("smoke"), TEXT("SMOKE & DUST"), { TEXT("VOLUME LOW"), TEXT("VOLUME MEDIUM"), TEXT("VOLUME HIGH"), TEXT("VOLUME ULTRA") }, 2, TEXT("Always volumetric: it takes the map's shadows, casts its own and is lit by fire.")); I.Preset = { 0, 0, 1, 2, 3 }; I.W = 3.5f; I.M = 0.4f; }
	{ FItem& I = Add(0, TEXT("EFFECTS"), TEXT("smokelit"), TEXT("LIGHT IN SMOKE"), { TEXT("LOW"), TEXT("HIGH"), TEXT("ULTRA") }, 1, TEXT("How finely fire, explosions and lamps light up the smoke around them.")); I.Preset = { 0, 0, 1, 1, 2 }; I.W = 2.0f; }
	{ FItem& I = Add(0, TEXT("EFFECTS"), TEXT("debris"), TEXT("DEBRIS & SPARKS"), { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("ULTRA") }, 2, TEXT("Chunks, sparks and dirt thrown by hits. Mostly costs the processor.")); I.Preset = { 1, 0, 1, 2, 3 }; I.W = 1.1f; }
	{ FItem& I = Add(0, TEXT("EFFECTS"), TEXT("scorch"), TEXT("SCORCH MARKS"), { TEXT("SHORT"), TEXT("LONG"), TEXT("PERMANENT") }, 2, TEXT("How long burns stay on the ground. Craters always stay: they are part of the battle.")); I.Preset = { 2, 0, 1, 2, 2 }; I.M = 0.3f; }
	{ FItem& I = Add(0, TEXT("EFFECTS"), TEXT("part"), TEXT("PARTICLE DENSITY"), { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("ULTRA") }, 2, TEXT("Embers, sand and small effects around the battle.")); I.Preset = { 1, 0, 1, 2, 3 }; I.W = 1.2f; }
	{ FItem& I = Add(0, TEXT("WORLD"), TEXT("terrain"), TEXT("TERRAIN DETAIL"), { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("ULTRA") }, 2, TEXT("The shape of dunes, tracks and craters up close.")); I.Preset = { 1, 0, 1, 2, 3 }; I.W = 1.3f; I.M = 0.4f; }
	{ FItem& I = Add(0, TEXT("WORLD"), TEXT("foliage"), TEXT("FOLIAGE"), { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("ULTRA") }, 2, TEXT("Grass, bushes and palms. Never covers units at any level.")); I.Preset = { 0, 0, 1, 2, 3 }; I.W = 1.6f; I.M = 0.2f; }
	{ FItem& I = Add(0, TEXT("WORLD"), TEXT("dist"), TEXT("DRAW DISTANCE"), { TEXT("NEAR"), TEXT("MEDIUM"), TEXT("FAR"), TEXT("MAX") }, 2, TEXT("How far the detail reaches when the camera tilts.")); I.Preset = { 1, 0, 1, 2, 3 }; I.W = 1.0f; }
	{ FItem& I = Add(0, TEXT("WORLD"), TEXT("water"), TEXT("WATER"), { TEXT("LOW"), TEXT("HIGH"), TEXT("ULTRA") }, 1, TEXT("Waves, foam and wakes behind boats.")); I.Preset = { 0, 0, 1, 1, 2 }; I.W = 0.9f; }
	{ FItem& I = Add(0, TEXT("WORLD"), TEXT("weather"), TEXT("WEATHER EFFECTS"), { TEXT("LOW"), TEXT("HIGH"), TEXT("ULTRA") }, 1, TEXT("Rain, snowfall and storms, when the map has them.")); I.Preset = { 0, 0, 1, 1, 2 }; I.W = 0.9f; }
	{ FItem& I = Add(0, TEXT("POST-PROCESSING"), TEXT("look"), TEXT("COLOR LOOK"), { TEXT("CLASSIC VIVID"), TEXT("NATURAL"), TEXT("CINEMATIC") }, 2, TEXT("Classic is crisp and saturated, like the strategy games of the early 2000s.")); I.Preset = { 0, 1, 1, 2, 2 }; }
	{ FItem& I = Add(0, TEXT("POST-PROCESSING"), TEXT("bloom"), TEXT("BLOOM"), { TEXT("OFF"), TEXT("LOW"), TEXT("HIGH") }, 1, TEXT("The glow around fire and bright light.")); I.Preset = { 0, 0, 1, 1, 2 }; I.W = 0.4f; }
	{ FItem& I = Add(0, TEXT("POST-PROCESSING"), TEXT("dof"), TEXT("DEPTH OF FIELD"), { TEXT("OFF"), TEXT("ON") }, 0, TEXT("Blurs what is far. Off by default: it hides the battle.")); I.Preset = { 0, 0, 0, 0, 0 }; I.W = 0.6f; }
	{ FItem& I = Add(0, TEXT("POST-PROCESSING"), TEXT("mblur"), TEXT("MOTION BLUR"), { TEXT("OFF"), TEXT("LOW"), TEXT("HIGH") }, 0, TEXT("Blur when the camera moves fast.")); I.Preset = { 0, 0, 0, 0, 0 }; I.W = 0.5f; }
	{ FItem& I = Add(0, TEXT("POST-PROCESSING"), TEXT("grain"), TEXT("FILM GRAIN"), { TEXT("OFF"), TEXT("ON") }, 1, TEXT("A fine film texture over the image.")); I.Preset = { 0, 0, 0, 1, 1 }; }
	{ FItem& I = Add(0, TEXT("POST-PROCESSING"), TEXT("lens"), TEXT("LENS EFFECTS"), { TEXT("OFF"), TEXT("ON") }, 1, TEXT("Flares and color fringes from bright explosions.")); I.Preset = { 0, 0, 0, 1, 1 }; I.W = 0.2f; }
	// SND
	{ FItem& I = Add(1, TEXT("VOLUME"), TEXT("master"), TEXT("MASTER"), TArray<const TCHAR*>(), 80, TEXT("")); I.Min = 0; I.Max = 100; }
	{ FItem& I = Add(1, TEXT("VOLUME"), TEXT("music"), TEXT("MUSIC"), TArray<const TCHAR*>(), 55, TEXT("")); I.Min = 0; I.Max = 100; }
	{ FItem& I = Add(1, TEXT("VOLUME"), TEXT("sfx"), TEXT("EFFECTS"), TArray<const TCHAR*>(), 90, TEXT("")); I.Min = 0; I.Max = 100; }
	{ FItem& I = Add(1, TEXT("VOLUME"), TEXT("voice"), TEXT("UNIT VOICES"), TArray<const TCHAR*>(), 75, TEXT("")); I.Min = 0; I.Max = 100; }
	{ FItem& I = Add(1, TEXT("VOLUME"), TEXT("ui"), TEXT("INTERFACE"), TArray<const TCHAR*>(), 60, TEXT("")); I.Min = 0; I.Max = 100; }
	{ FItem& I = Add(1, TEXT("VOLUME"), TEXT("amb"), TEXT("AMBIENCE"), TArray<const TCHAR*>(), 65, TEXT("")); I.Min = 0; I.Max = 100; }
	{ FItem& I = Add(1, TEXT("OUTPUT"), TEXT("out"), TEXT("SPEAKER SETUP"), { TEXT("HEADPHONES"), TEXT("STEREO"), TEXT("SURROUND 5.1"), TEXT("SURROUND 7.1") }, 1, TEXT("Match it to what you listen with. Headphones adds a 3D mix.")); }
	{ FItem& I = Add(1, TEXT("OUTPUT"), TEXT("dyn"), TEXT("DYNAMIC RANGE"), { TEXT("NIGHT"), TEXT("NORMAL"), TEXT("WIDE") }, 1, TEXT("Night keeps explosions from being much louder than voices.")); }
	{ FItem& I = Add(1, TEXT("OUTPUT"), TEXT("bgmute"), TEXT("MUTE IN BACKGROUND"), { TEXT("OFF"), TEXT("ON") }, 1, TEXT("")); }
	// CTL
	{ FItem& I = Add(2, TEXT("CAMERA"), TEXT("camspeed"), TEXT("CAMERA SPEED"), TArray<const TCHAR*>(), 55, TEXT("")); I.Min = 0; I.Max = 100; }
	{ FItem& I = Add(2, TEXT("CAMERA"), TEXT("edge"), TEXT("EDGE SCROLL"), { TEXT("ON"), TEXT("OFF") }, 0, TEXT("The camera moves when the mouse touches the edge of the screen.")); }
	{ FItem& I = Add(2, TEXT("CAMERA"), TEXT("zoom"), TEXT("ZOOM SPEED"), TArray<const TCHAR*>(), 50, TEXT("")); I.Min = 0; I.Max = 100; }
	{ FItem& I = Add(2, TEXT("CAMERA"), TEXT("pan"), TEXT("DRAG TO PAN"), { TEXT("MIDDLE BUTTON"), TEXT("RIGHT BUTTON") }, 0, TEXT("")); }
	{ FItem& I = Add(2, TEXT("KEYS"), TEXT("layout"), TEXT("HOTKEY LAYOUT"), { TEXT("CLASSIC"), TEXT("GRID") }, 0, TEXT("Classic uses each command's letter. Grid maps the build panel to the keys under your left hand.")); }
	// GAMEO
	{ FItem& I = Add(3, TEXT("LANGUAGE"), TEXT("lang"), TEXT("LANGUAGE"), { TEXT("ENGLISH"), TEXT("PORTUGUÊS") }, 0, TEXT("The language of the menus, the texts and, if you choose, the unit voices.")); }
	{ FItem& I = Add(3, TEXT("LANGUAGE"), TEXT("voices"), TEXT("UNIT VOICES"), { TEXT("FACTION LANGUAGE"), TEXT("GAME LANGUAGE") }, 0, TEXT("Faction language: each army speaks its own (English, Russian, Chinese, Arabic). Game language: every unit speaks the language chosen above.")); }
	{ FItem& I = Add(3, TEXT("MATCH"), TEXT("timer"), TEXT("MATCH TIMER"), { TEXT("ON"), TEXT("OFF") }, 0, TEXT("")); }
	{ FItem& I = Add(3, TEXT("MATCH"), TEXT("chat"), TEXT("CHAT"), { TEXT("ALL"), TEXT("ALLIES ONLY"), TEXT("OFF") }, 0, TEXT("")); }
	{ FItem& I = Add(3, TEXT("MATCH"), TEXT("tips"), TEXT("TOOLTIPS"), { TEXT("FAST"), TEXT("NORMAL"), TEXT("OFF") }, 1, TEXT("Off needs the button labels on: with clean buttons the tooltip is where the price shows.")); }
	{ FItem& I = Add(3, TEXT("HUD"), TEXT("hudclean"), TEXT("BUTTON LABELS"), { TEXT("CLEAN"), TEXT("HOTKEYS & PRICES") }, 0, TEXT("Clean: the buttons show only the picture; the tooltip shows the hotkey, the price and the time.")); I.Ini = TEXT("HudCleanSlots"); }
	{ FItem& I = Add(3, TEXT("HUD"), TEXT("hudhide"), TEXT("EMPTY SLOTS"), { TEXT("HIDDEN"), TEXT("SHOWN") }, 0, TEXT("Hidden: the panel shows only the buttons that exist.")); I.Ini = TEXT("HudHideEmptySlots"); }
	// ACC
	{ FItem& I = Add(4, TEXT("VISION"), TEXT("cb"), TEXT("COLORBLIND MODE"), { TEXT("OFF"), TEXT("PROTANOPIA"), TEXT("DEUTERANOPIA"), TEXT("TRITANOPIA") }, 0, TEXT("Shifts player colors and the interface so every army stays different.")); }
	{ FItem& I = Add(4, TEXT("VISION"), TEXT("outline"), TEXT("UNIT OUTLINES"), { TEXT("OFF"), TEXT("ON") }, 0, TEXT("A thin outline in the player color around every unit.")); }
	{ FItem& I = Add(4, TEXT("VISION"), TEXT("uiscale"), TEXT("INTERFACE SIZE"), TArray<const TCHAR*>(), 100, TEXT("")); I.Min = 80; I.Max = 130; }
	{ FItem& I = Add(4, TEXT("COMFORT"), TEXT("shake"), TEXT("SCREEN SHAKE"), TArray<const TCHAR*>(), 70, TEXT("How much big explosions shake the camera.")); I.Min = 0; I.Max = 100; }
	{ FItem& I = Add(4, TEXT("COMFORT"), TEXT("flash"), TEXT("FLASH REDUCTION"), { TEXT("OFF"), TEXT("ON") }, 0, TEXT("Softens the brightest flashes of explosions.")); }
	{ FItem& I = Add(4, TEXT("COMFORT"), TEXT("subs"), TEXT("SUBTITLES"), { TEXT("OFF"), TEXT("ON") }, 0, TEXT("Shows what units and the announcer say.")); }
	Restart();
}

void SRTSOptions::Restart()
{
	// como a página aberta de novo: os itens no padrão, o AUTO e logo a qualidade do jogo; o HUD como está gravado
	Opt.Reset();
	for (const TArray<FItem>& T : Tabs) for (const FItem& I : T) Opt.Add(I.Key, I.Def);
	// os volumes como estão guardados (o jogo e as telas tocam com eles)
	for (const TCHAR* K : OpS::VOL_KEYS)
	{
		int32 Saved = -1;
		if (GConfig && GConfig->GetInt(TEXT("RTS"), *FString::Printf(TEXT("Vol_%s"), K), Saved, GGameUserSettingsIni) && Saved >= 0) Opt.Add(K, FMath::Clamp(Saved, 0, 100));
	}
	// o que foi aplicado da última vez (06/11: as opções lembram tudo; antes só a qualidade, o HUD e os volumes)
	for (const TArray<FItem>& T : Tabs)
	{
		for (const FItem& I : T)
		{
			int32 V = -1;
			if (GConfig && GConfig->GetInt(TEXT("RTS"), *FString::Printf(TEXT("Opt_%s"), I.Key), V, GGameUserSettingsIni) && V >= 0)
			{
				Opt.Add(I.Key, I.Opts.Num() ? FMath::Clamp(V, 0, I.Opts.Num() - 1) : FMath::Clamp(V, I.Min, I.Max));
			}
		}
	}
	int32 SavedPreset = -1;
	bool bWasCustom = false;
	if (GConfig) { GConfig->GetInt(TEXT("RTS"), TEXT("OptPreset"), SavedPreset, GGameUserSettingsIni); GConfig->GetBool(TEXT("RTS"), TEXT("OptCustom"), bWasCustom, GGameUserSettingsIni); }
	const int32 GameQ = FRTSShell::Handlers().GetQuality ? FMath::Clamp(FRTSShell::Handlers().GetQuality(), 0, 4) : 3;
	if (SavedPreset < 0) { ApplyPreset(5); ApplyPreset(GameQ); }        // nunca aplicou: o AUTO e a qualidade do jogo, como a página
	else if (bWasCustom) { Pre = FMath::Clamp(SavedPreset, 0, 5); bCustom = true; }   // a mistura dele fica como estava
	else
	{
		// o preset aplicado; se a qualidade do jogo mudou por fora (o laboratório, a linha de comando), vale a do jogo
		ApplyPreset(FMath::Clamp(SavedPreset, 0, 5));
		if ((SavedPreset == 5 ? OpS::AUTO_AS : SavedPreset) != GameQ) ApplyPreset(GameQ);
	}
	for (const TArray<FItem>& T : Tabs)
	{
		for (const FItem& I : T)
		{
			FString V;
			if (I.Ini && GConfig && GConfig->GetString(TEXT("RTS"), I.Ini, V, GGameUserSettingsIni) && !V.IsEmpty())
			{
				const FString L = V.TrimStartAndEnd().ToLower();
				Opt.Add(I.Key, L == TEXT("true") || L == TEXT("1") || L == TEXT("yes") ? 0 : 1);
			}
		}
	}
	if (Val(TEXT("hudclean")) == 0 && Val(TEXT("tips")) == 2) Opt.Add(TEXT("tips"), 1);
	Tab = 0;
	FParse::Value(FCommandLine::Get(), TEXT("-RTSOptTab="), Tab);   // as fotos de teste de cada aba
	Tab = FMath::Clamp(Tab, 0, 4);
	CardItem = -1;
	KeyQuery.Reset();
	bKeyFocus = false;
	DragItem = -1;
	Scroll = ScrollTo = 0.0f;
	CustomH = bCustom ? 34.0f : 0.0f;
	Hover = FHit();
	HoverEase.Reset();
	Snapshot();
}

void SRTSOptions::Snapshot()
{
	SavedOpt = Opt;
	SavedPre = Pre;
	bSavedCustom = bCustom;
}

bool SRTSOptions::IsDirty() const
{
	if (Pre != SavedPre || bCustom != bSavedCustom || Opt.Num() != SavedOpt.Num()) return true;
	for (const TPair<FString, int32>& P : Opt)
	{
		const int32* S = SavedOpt.Find(P.Key);
		if (!S || *S != P.Value) return true;
	}
	return false;
}

const SRTSOptions::FItem* SRTSOptions::Find(const TCHAR* Key) const
{
	for (const TArray<FItem>& T : Tabs) for (const FItem& I : T) if (FCString::Strcmp(I.Key, Key) == 0) return &I;
	return nullptr;
}

int32 SRTSOptions::Val(const TCHAR* Key) const
{
	const int32* V = Opt.Find(Key);
	return V ? *V : 0;
}

void SRTSOptions::ApplyPreset(int32 K)
{
	Pre = K;
	bCustom = false;
	const int32 Pi = K == 5 ? OpS::AUTO_AS : K;
	for (const FItem& I : Tabs[0]) if (!I.bFixed && I.Preset.Num() == 5) Opt.Add(I.Key, I.Preset[Pi]);
}

TMap<FString, int32> SRTSOptions::PresetVals(int32 K) const
{
	TMap<FString, int32> V = Opt;
	const int32 Pi = K == 5 ? OpS::AUTO_AS : K;
	for (const FItem& I : Tabs[0]) if (!I.bFixed && I.Preset.Num() == 5) V.Add(I.Key, I.Preset[Pi]);
	return V;
}

void SRTSOptions::Estimate(const TMap<FString, int32>& V, int32& OutFps, float& OutGb) const
{
	// o estimate() da página: os quadros e a memória de vídeo pra GTX 1660 (6 GB), da resolução, do upscaler e do custo de cada item
	const auto At = [&](const TCHAR* K) { const int32* P = V.Find(K); return P ? *P : 0; };
	static const float ResF[5] = { 0.44f, 0.69f, 1.0f, 1.78f, 4.0f };
	static const float UpQ[4] = { 0.5f, 0.62f, 0.76f, 1.0f };
	const float Rf = ResF[FMath::Clamp(At(TEXT("res")), 0, 4)];
	const float Uf = At(TEXT("up")) ? UpQ[FMath::Clamp(At(TEXT("upq")), 0, 3)] : 1.0f;
	double Ms = 4.2, Gb = 0.9;
	for (const FItem& I : Tabs[0])
	{
		if (I.W == 0.0f && I.M == 0.0f) continue;
		const double F = double(At(I.Key)) / double(FMath::Max(1, I.Opts.Num() - 1));
		Ms += I.W * F * Rf * Uf;
		Gb += I.M * F * FMath::Sqrt(Rf);
	}
	if (At(TEXT("fg"))) Ms *= 0.62;
	OutFps = FMath::RoundToInt(1000.0 / Ms);
	OutGb = float(FMath::RoundToDouble(Gb * 10.0) / 10.0);
}

TArray<SRTSOptions::FBlock> SRTSOptions::Layout() const
{
	// o fluxo do CSS: o grupo (14 de altura, 24 acima menos o primeiro, 8 abaixo; a margem de baixo da linha anterior se funde com a
	// de cima do grupo), a linha (48 + 2 de borda, 5 abaixo); nos gráficos os presets (158) e a faixa do CUSTOM (10 acima, 34 aberta)
	TArray<FBlock> B;
	float Y = 0.0f;
	bool bFirst = true;
	float Pending = 0.0f;   // a margem de baixo da última linha (5), que se funde com a de cima do grupo
	const auto Group = [&](const FString& Text)
	{
		Y += bFirst ? 0.0f : FMath::Max(Pending, 24.0f) - Pending;
		FBlock G; G.Type = FBlock::Group; G.Text = Text; G.Y = Y; G.H = 14; B.Add(G);
		Y += 14 + 8;
		bFirst = false;
		Pending = 0.0f;
	};
	const auto Row = [&](FBlock::EType Type, int32 Item)
	{
		FBlock R; R.Type = Type; R.Item = Item; R.Y = Y; R.H = 50; B.Add(R);
		Y += 55;
		Pending = 5.0f;
	};
	if (Tab == 0)
	{
		FBlock P; P.Type = FBlock::Presets; P.Y = 0; P.H = 158; B.Add(P);
		FBlock C; C.Type = FBlock::Custom; C.Y = 168; C.H = CustomH; B.Add(C);
		Y = 168 + CustomH;
	}
	const TArray<FItem>& L = Tabs[Tab];
	FString G;
	for (int32 I = 0; I < L.Num(); ++I)
	{
		if (G != L[I].Group) { G = L[I].Group; Group(G); }
		Row(FBlock::Row, I);
	}
	if (Tab == 2)
	{
		// as teclas: o grupo KEYS, a busca (46 + 2, 10 abaixo) e a lista filtrada
		Group(TEXT("KEYS"));
		FBlock S; S.Type = FBlock::KeySearch; S.Y = Y; S.H = 48; B.Add(S);
		Y += 48 + 10;
		const FString Q = KeyQuery.ToUpper();
		for (int32 K = 0; K < 10; ++K)
		{
			if (!Q.IsEmpty() && !FString(OpS::KEYS[K].Action).Contains(Q, ESearchCase::CaseSensitive) && !FString(OpS::KEYS[K].Key).Contains(Q, ESearchCase::CaseSensitive)) continue;
			Row(FBlock::KeyRow, K);
		}
	}
	return B;
}

float SRTSOptions::ContentH() const
{
	const TArray<FBlock> B = Layout();
	return B.Num() ? B.Last().Y + B.Last().H + (B.Last().Type == FBlock::Row || B.Last().Type == FBlock::KeyRow ? 5.0f : 0.0f) : 0.0f;
}

TArray<FVector4f> SRTSOptions::SegBoxes(const FItem& It, float RowY) const
{
	// os spans do .seg: 700 13px, espaçamento 1,5, 6 × 11 de respiro e a borda; 4 entre eles; na altura da linha (10 abaixo do topo)
	TArray<FVector4f> R;
	const FSlateFontInfo F = Font(EFace::Barlow700, 13, 1.5f);
	float X = OpS::SEG_X;
	for (const TCHAR* O : It.Opts)
	{
		const float W = TextWidth(O, F) + 24.0f;
		R.Add(FVector4f(X, RowY + 10, W, 30));
		X += W + 4.0f;
	}
	return R;
}

float SRTSOptions::HoverOf(EHit Kind, int32 A, int32 B) const
{
	const float* V = HoverEase.Find((uint32(Kind) << 24) | (uint32(A + 1) << 8) | uint32(B + 1));
	return V ? FMath::Clamp(*V, 0.0f, 1.0f) : 0.0f;
}

SRTSOptions::FHit SRTSOptions::HitAt(FVector2f P) const
{
	if (In(BackRect(), P)) return { EHit::Back, -1, -1 };
	for (int32 K = 0; K < 5; ++K) if (In(FVector4f(110, OpS::TAB_Y0 + K * OpS::TAB_STEP, 280, 64), P)) return { EHit::Tab, K, -1 };
	if (In(OpS::RESET, P)) return { EHit::Reset, -1, -1 };
	if (In(OpS::APPLY, P)) return { EHit::Apply, -1, -1 };
	if (!In(FVector4f(OpS::MAIN_X, OpS::MAIN_Y, OpS::MAIN_W + 14, OpS::MAIN_H), P)) return FHit();
	const float Cy = P.Y - OpS::MAIN_Y + Scroll;
	for (const FBlock& B : Layout())
	{
		if (B.Type == FBlock::Presets && Cy >= 0 && Cy <= 158)
		{
			for (int32 K = 0; K < 6; ++K) if (P.X >= OpS::MAIN_X + K * (153.33f + 8) && P.X <= OpS::MAIN_X + K * (153.33f + 8) + 153.33f) return { EHit::Preset, K, -1 };
			return FHit();
		}
		if (B.Type == FBlock::KeySearch && Cy >= B.Y && Cy <= B.Y + 48 && P.X <= OpS::ROW_X + 520) return { EHit::Search, -1, -1 };
		if ((B.Type == FBlock::Row || B.Type == FBlock::KeyRow) && Cy >= B.Y && Cy <= B.Y + 50 && P.X >= OpS::ROW_X && P.X <= OpS::ROW_X + OpS::ROW_W)
		{
			if (B.Type == FBlock::KeyRow) return { EHit::Row, -1, B.Item };
			const FItem& It = Tabs[Tab][B.Item];
			const float RowScreenY = OpS::MAIN_Y + B.Y - Scroll;
			if (It.Opts.Num() == 0)
			{
				// o deslizante: a trilha de 460 e 10 acima e abaixo dela (o ::before)
				if (P.X >= OpS::SEG_X - 7 && P.X <= OpS::SEG_X + 460 + 7 && P.Y >= RowScreenY + 13 && P.Y <= RowScreenY + 37) return { EHit::Slider, B.Item, -1 };
			}
			else
			{
				const TArray<FVector4f> Sb = SegBoxes(It, RowScreenY);
				for (int32 O = 0; O < Sb.Num(); ++O) if (In(Sb[O], P)) return { EHit::Seg, B.Item, O };
			}
			return { EHit::Row, B.Item, -1 };
		}
	}
	return FHit();
}

void SRTSOptions::SetSlider(int32 Item, float X)
{
	const FItem& It = Tabs[Tab][Item];
	const float F = FMath::Clamp((X - OpS::SEG_X) / 460.0f, 0.0f, 1.0f);
	Opt.Add(It.Key, FMath::RoundToInt(It.Min + F * (It.Max - It.Min)));
	if (Tab == 1) SetVolumes(false);   // o som muda na hora (o APPLY guarda)
}

void SRTSOptions::SetVolumes(bool bSaved) const
{
	// a conta é do RTSShellAudio (a mesma da partida): a prévia com os da tela, ou os guardados
	if (bSaved) { RTSShellAudio::ApplyUserVolumes(nullptr); return; }
	TMap<FString, int32> V;
	for (const TCHAR* K : OpS::VOL_KEYS) V.Add(K, Val(K));
	RTSShellAudio::ApplyUserVolumes(&V);
}

void SRTSOptions::Back()
{
	SetVolumes(true);   // o que não foi aplicado volta ao guardado
	if (bFromMatch) { FRTSShell::Hide(); return; }   // (06/11) do Esc da partida: o Esc continua embaixo
	FRTSShell::Show(TEXT("menu.html"));
}

void SRTSOptions::Apply()
{
	// o APPLY da página: a qualidade pro jogo (o CUSTOM vai no HIGH), as opções do HUD no GameUserSettings; e os volumes guardados
	if (FRTSShell::Handlers().SetQuality) FRTSShell::Handlers().SetQuality(bCustom ? 3 : Pre);
	if (GConfig)
	{
		for (const TArray<FItem>& T : Tabs) for (const FItem& I : T)
		{
			if (I.Ini) GConfig->SetString(TEXT("RTS"), I.Ini, Val(I.Key) == 0 ? TEXT("True") : TEXT("False"), GGameUserSettingsIni);
		}
		for (const TCHAR* K : OpS::VOL_KEYS) GConfig->SetInt(TEXT("RTS"), *FString::Printf(TEXT("Vol_%s"), K), Val(K), GGameUserSettingsIni);
		// tudo o que está na tela, pra abrir igual da próxima vez (os itens que o jogo ainda não usa ficam guardados pra quando usar)
		for (const TPair<FString, int32>& P : Opt) GConfig->SetInt(TEXT("RTS"), *FString::Printf(TEXT("Opt_%s"), *P.Key), P.Value, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("RTS"), TEXT("OptPreset"), Pre, GGameUserSettingsIni);
		GConfig->SetBool(TEXT("RTS"), TEXT("OptCustom"), bCustom, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	UE_LOG(LogTemp, Log, TEXT("RTS telas: opções, APPLY (qualidade %d, HUD limpo %d, casas vazias %d, volume geral %d)"), bCustom ? 3 : Pre, Val(TEXT("hudclean")), Val(TEXT("hudhide")), Val(TEXT("master")));
	SetVolumes(true);
	Snapshot();   // fica na tela (ele: "não pode voltar pro menu principal"); o APPLY apaga até a próxima mudança
}

void SRTSOptions::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	Now = InCurrentTime;
	const float Dt = FMath::Clamp(InDeltaTime, 0.0f, 0.1f);
	// as transições do CSS (.15 s; a linha .12 s) e a faixa do CUSTOM abrindo (.2 s)
	const uint32 HKey = Hover.Kind != EHit::None ? (uint32(Hover.Kind) << 24) | (uint32(Hover.A + 1) << 8) | uint32(Hover.B + 1) : 0u;
	if (HKey) HoverEase.FindOrAdd(HKey);
	for (auto It = HoverEase.CreateIterator(); It; ++It)
	{
		It.Value() = FMath::Clamp(It.Value() + (It.Key() == HKey ? Dt : -Dt) / 0.15f, 0.0f, 1.0f);
		if (It.Value() <= 0.0f && It.Key() != HKey) It.RemoveCurrent();
	}
	// a linha também acende quando o mouse está numa opção ou no deslizante dela
	if (Hover.Kind == EHit::Seg || Hover.Kind == EHit::Slider)
	{
		float& V = HoverEase.FindOrAdd((uint32(EHit::Row) << 24) | (uint32(Hover.A + 1) << 8));
		V = FMath::Clamp(V + 2.0f * Dt / 0.12f, 0.0f, 1.0f);
	}
	CustomH = FMath::FInterpConstantTo(CustomH, bCustom ? 34.0f : 0.0f, Dt, 34.0f / 0.2f);
	const float Max = FMath::Max(0.0f, ContentH() - OpS::MAIN_H);
	ScrollTo = FMath::Clamp(ScrollTo, 0.0f, Max);
	Scroll += (ScrollTo - Scroll) * (1.0f - FMath::Exp(-Dt * 16.0f));
	if (FMath::Abs(ScrollTo - Scroll) < 0.25f) Scroll = ScrollTo;
	Invalidate(EInvalidateWidgetReason::Paint);
}

FReply SRTSOptions::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FString Shot;
	if (FParse::Value(FCommandLine::Get(), TEXT("-RTSScreenShot="), Shot)) return FReply::Handled();   // a foto de teste fica parada
	const FVector2f P(StageFit(MyGeometry).AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	if (DragItem >= 0) { SetSlider(DragItem, P.X); return FReply::Handled(); }
	FHit H = HitAt(P);
	if (H.Kind == EHit::Apply && !IsDirty()) H = FHit();   // o APPLY apagado não reage
	// as opções riscadas não reagem (pointer-events: none): contam como a linha
	if (H.Kind == EHit::Seg && Tabs[Tab][H.A].Na.Contains(H.B)) H = { EHit::Row, H.A, -1 };
	if (H.Kind == EHit::Seg && FCString::Strcmp(Tabs[Tab][H.A].Key, TEXT("tips")) == 0 && H.B == 2 && Val(TEXT("hudclean")) == 0) H = { EHit::Row, H.A, -1 };
	const bool bLive = H.Kind != EHit::None && H.Kind != EHit::Row && H.Kind != EHit::Search;
	if (!(H == Hover) && bLive) RTSShellAudio::Play(RTSShellAudio::EUi::Hover);
	Hover = H;
	// o cartão: o item da linha com o mouse em cima; fora da área que rola volta pra qualidade
	if (H.Kind == EHit::Row || H.Kind == EHit::Seg || H.Kind == EHit::Slider) CardItem = H.A >= 0 ? H.A : -1;
	else if (!In(FVector4f(OpS::MAIN_X, OpS::MAIN_Y, OpS::MAIN_W + 14, OpS::MAIN_H), P)) CardItem = -1;
	return FReply::Handled();
}

void SRTSOptions::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	FString Shot;
	if (FParse::Value(FCommandLine::Get(), TEXT("-RTSScreenShot="), Shot)) return;
	Hover = FHit();
	CardItem = -1;
}

FReply SRTSOptions::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Handled();
	const FVector2f P(StageFit(MyGeometry).AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	const FHit H = HitAt(P);
	bKeyFocus = H.Kind == EHit::Search;
	if (bKeyFocus) CaretT0 = Now;
	switch (H.Kind)
	{
	case EHit::Back: RTSShellAudio::Click(); Back(); break;
	case EHit::Tab:
		RTSShellAudio::Click();
		Tab = H.A;
		Scroll = ScrollTo = 0.0f;
		CardItem = -1;
		break;
	case EHit::Preset: RTSShellAudio::Click(); ApplyPreset(H.A); break;
	case EHit::Seg:
	{
		const FItem& It = Tabs[Tab][H.A];
		if (It.Na.Contains(H.B)) break;
		if (FCString::Strcmp(It.Key, TEXT("tips")) == 0 && H.B == 2 && Val(TEXT("hudclean")) == 0) break;
		RTSShellAudio::Click();
		Opt.Add(It.Key, H.B);
		if (Val(TEXT("hudclean")) == 0 && Val(TEXT("tips")) == 2) Opt.Add(TEXT("tips"), 1);
		if (Tab == 0 && !It.bFixed && It.Preset.Num()) bCustom = true;
		CardItem = H.A;
		break;
	}
	case EHit::Slider:
		RTSShellAudio::Click();
		DragItem = H.A;
		SetSlider(H.A, P.X);
		return FReply::Handled().CaptureMouse(SharedThis(this));
	case EHit::Reset: RTSShellAudio::Click(); ApplyPreset(5); break;
	case EHit::Apply: if (IsDirty()) { RTSShellAudio::Click(); Apply(); } break;
	default: break;
	}
	return FReply::Handled();
}

FReply SRTSOptions::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	DragItem = -1;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SRTSOptions::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ScrollTo = FMath::Clamp(ScrollTo - MouseEvent.GetWheelDelta() * 100.0f, 0.0f, FMath::Max(0.0f, ContentH() - OpS::MAIN_H));
	return FReply::Handled();
}

FReply SRTSOptions::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey K = InKeyEvent.GetKey();
	if (K == EKeys::Escape)
	{
		if (bKeyFocus && !KeyQuery.IsEmpty()) KeyQuery.Reset();
		else Back();
		return FReply::Handled();
	}
	if (!bKeyFocus) return FReply::Unhandled();
	if (K == EKeys::BackSpace)
	{
		if (InKeyEvent.IsControlDown())
		{
			KeyQuery.TrimEndInline();
			int32 Sp = INDEX_NONE;
			KeyQuery = KeyQuery.FindLastChar(TEXT(' '), Sp) ? KeyQuery.Left(Sp + 1) : FString();
		}
		else if (KeyQuery.Len()) KeyQuery.LeftChopInline(1);
		CaretT0 = Now;
	}
	else if (K == EKeys::V && InKeyEvent.IsControlDown())
	{
		FString Clip;
		FPlatformApplicationMisc::ClipboardPaste(Clip);
		Clip.ReplaceInline(TEXT("\r"), TEXT(" "));
		Clip.ReplaceInline(TEXT("\n"), TEXT(" "));
		KeyQuery = (KeyQuery + Clip).Left(40);
		CaretT0 = Now;
	}
	return FReply::Handled();
}

FReply SRTSOptions::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
{
	const TCHAR C = InCharacterEvent.GetCharacter();
	if (!bKeyFocus || C < 32 || C == 127 || (InCharacterEvent.IsControlDown() && !InCharacterEvent.IsAltDown())) return FReply::Unhandled();
	if (KeyQuery.Len() < 40) KeyQuery.AppendChar(C);
	CaretT0 = Now;
	return FReply::Handled();
}

FCursorReply SRTSOptions::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (Hover.Kind == EHit::Search) return FCursorReply::Cursor(EMouseCursor::TextEditBeam);
	const bool bHand = Hover.Kind != EHit::None && Hover.Kind != EHit::Row;
	return FCursorReply::Cursor(bHand ? EMouseCursor::Hand : EMouseCursor::Default);
}

// ---------------------------------------------------------------------------------------------------------------------------- a pintura

int32 SRTSOptions::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FGeometry S = StageFit(AllottedGeometry);
	int32 L = LayerId;
	Backdrop(Out, L, AllottedGeometry, S, TEXT("screens_glow.png"));
	PaintLeft(Out, L, S);
	PaintMain(Out, L, S);
	PaintInfo(Out, L, S);
	PaintFoot(Out, L, S);
	BackButton(Out, L, S, HoverOf(EHit::Back));
	{
		FMesh M(S);
		StageCorners(M);
		M.Draw(Out, L++, White());
	}
	return L;
}

void SRTSOptions::PaintLeft(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	Text(Out, L, S, TEXT("OPTIONS"), Font(EFace::Barlow600, 16, 8), 110, 104, Ac());
	Text(Out, L, S, TEXT("SETTINGS"), Font(EFace::Shoulders900, 72, 2), 110, 180, FLinearColor::White);
	{
		FMesh M(S);
		for (int32 K = 0; K < 5; ++K)
		{
			const float H = K == Tab ? 0.0f : HoverOf(EHit::Tab, K);
			const FLinearColor Bd = K == Tab ? Ac(0.75f) : FMath::Lerp(Css(230, 220, 190, 0.10f), Css(230, 220, 190, 0.28f), H);
			BoxRect(M, FVector4f(110, OpS::TAB_Y0 + K * OpS::TAB_STEP, 280, 64), CssV(16, 21, 19, 0.62f), Bd.ToFColorSRGB());
		}
		M.Draw(Out, L++, White());
	}
	for (int32 K = 0; K < 5; ++K)
	{
		const FLinearColor C = K == Tab ? FLinearColor::White : FMath::Lerp(Css(238, 241, 244, 0.6f), FLinearColor::White, HoverOf(EHit::Tab, K));
		Text(Out, L, S, OpS::TAB_T[K], Font(EFace::Shoulders800, 26, 3), 129, OpS::TAB_Y0 + K * OpS::TAB_STEP + 42, C);
	}
	++L;
}

void SRTSOptions::PaintMain(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	const float Scale = S.GetAccumulatedLayoutTransform().GetScale();
	Out.PushClip(FSlateClippingZone(S.MakeChild(FVector2f(OpS::MAIN_W + 14, OpS::MAIN_H), FSlateLayoutTransform(FVector2f(OpS::MAIN_X, OpS::MAIN_Y)))));
	const TArray<FBlock> Bl = Layout();
	for (const FBlock& B : Bl)
	{
		const float Y = OpS::MAIN_Y + B.Y - Scroll;
		if (Y > OpS::MAIN_Y + OpS::MAIN_H || Y + FMath::Max(B.H, 50.0f) < OpS::MAIN_Y) continue;
		switch (B.Type)
		{
		case FBlock::Presets: PaintPresets(Out, L, S, Y); break;
		case FBlock::Custom:
			if (B.H > 0.5f)
			{
				// a faixa do CUSTOM: o texto âmbar numa caixa tracejada, aparecendo de cima pra baixo
				Out.PushClip(FSlateClippingZone(S.MakeChild(FVector2f(OpS::MAIN_W, B.H), FSlateLayoutTransform(FVector2f(OpS::MAIN_X, Y)))));
				const FSlateFontInfo F = Font(EFace::Barlow700, 12, 3);
				const float W = TextWidth(TEXT("CUSTOM · YOUR CHANGES"), F, Scale) + 24.0f + 2.0f;
				Dashed(Out, L, S, { FVector2f(OpS::MAIN_X + 0.5f, Y + 0.5f), FVector2f(OpS::MAIN_X + W - 0.5f, Y + 0.5f), FVector2f(OpS::MAIN_X + W - 0.5f, Y + 29.5f),
					FVector2f(OpS::MAIN_X + 0.5f, Y + 29.5f), FVector2f(OpS::MAIN_X + 0.5f, Y + 0.5f) }, Ac(0.6f), 1.0f, 3.0f);
				Text(Out, L, S, TEXT("CUSTOM · YOUR CHANGES"), F, OpS::MAIN_X + 13, Y + 19.5f, Ac());
				Out.PopClip();
			}
			break;
		case FBlock::Group:
		{
			// o grupo: as letras âmbar espaçadas e a linha até o fim
			const float W = Text(Out, L, S, B.Text, Font(EFace::Barlow600, 12, 5), OpS::MAIN_X, Y + 12, Css(216, 173, 78, 0.75f));
			FMesh M(S);
			M.Rect(OpS::MAIN_X + W + 14, Y + 7, OpS::MAIN_W - W - 14, 1, CssV(230, 220, 190, 0.10f));
			M.Draw(Out, L++, White());
			break;
		}
		case FBlock::Row: PaintRow(Out, L, S, Tabs[Tab][B.Item], B.Item, Y); break;
		case FBlock::KeySearch:
		{
			FMesh M(S);
			BoxRect(M, FVector4f(OpS::ROW_X, Y, 520, 48), CssV(16, 21, 19, 0.8f), CssV(230, 220, 190, 0.2f));
			M.Draw(Out, L++, White());
			const FLinearColor Ic = Css(230, 220, 190, 0.6f);
			Lines(Out, L, S, Arc(FVector2f(OpS::ROW_X + 22.875f, Y + 22.875f), 4.875f, 0.0f, 2.0f * PI, 28), Ic, 1.5f);
			Lines(Out, L, S, { FVector2f(OpS::ROW_X + 26.625f, Y + 26.625f), FVector2f(OpS::ROW_X + 30.75f, Y + 30.75f) }, Ic, 1.5f);
			const FSlateFontInfo Qf = Font(EFace::Barlow600, 17);
			if (KeyQuery.IsEmpty()) Text(Out, L, S, TEXT("Search actions or keys"), Qf, OpS::ROW_X + 43, Y + 30, Css(230, 220, 190, 0.35f));
			else Text(Out, L, S, KeyQuery, Qf, OpS::ROW_X + 43, Y + 30, FLinearColor::White);
			if (bKeyFocus && FMath::Fmod(float(Now - CaretT0), 1.0f) < 0.5f)
			{
				FMesh C(S);
				C.Rect(OpS::ROW_X + 43 + (KeyQuery.IsEmpty() ? 0.0f : TextWidth(KeyQuery, Qf, Scale)), Y + 14, 1, 21, CssV(255, 255, 255));
				C.Draw(Out, L++, White());
			}
			++L;
			break;
		}
		case FBlock::KeyRow: PaintKeyRow(Out, L, S, B.Item, Y); break;
		}
	}
	// a rolagem (8 px, só quando passa da altura)
	const float Content = ContentH();
	if (Content > OpS::MAIN_H)
	{
		FMesh M(S);
		M.Rect(OpS::MAIN_X + OpS::MAIN_W + 6, OpS::MAIN_Y + Scroll / Content * OpS::MAIN_H, 8.0f, OpS::MAIN_H * OpS::MAIN_H / Content, CssV(230, 220, 190, 0.18f));
		M.Draw(Out, L++, White());
	}
	Out.PopClip();
}

void SRTSOptions::PaintPresets(FSlateWindowElementList& Out, int32& L, const FGeometry& S, float Y) const
{
	const float Scale = S.GetAccumulatedLayoutTransform().GetScale();
	struct FEst { int32 Fps; float Gb; };
	FEst E[6];
	for (int32 K = 0; K < 6; ++K) Estimate(PresetVals(K), E[K].Fps, E[K].Gb);
	{
		FMesh M(S);
		for (int32 K = 0; K < 6; ++K)
		{
			const FVector4f R(OpS::MAIN_X + K * (153.33f + 8), Y, 153.33f, 158);
			const bool bOn = K == Pre && !bCustom;
			const float H = HoverOf(EHit::Preset, K);
			const FColor Bg = bOn ? CssV(216, 173, 78, 0.06f) : CssV(16, 21, 19, 0.62f);
			const FColor Bd = bOn ? AcV(0.85f) : FMath::Lerp(Css(230, 220, 190, 0.10f), Css(230, 220, 190, 0.3f), H).ToFColorSRGB();
			BoxRect(M, R, Bg, K == 5 ? FColor(0, 0, 0, 0) : Bd);
			const int32 Bars = K == 0 ? 1 : K == 5 ? 4 : K + 1;
			for (int32 I = 0; I < 5; ++I)
			{
				const float Bh = 8.0f + 4.0f * I;
				M.Rect(R.X + R.Z - 15 - 37 + I * 8.0f, R.Y + 143 - Bh, 5, Bh, I < Bars ? AcV() : CssV(230, 220, 190, 0.2f));
			}
		}
		M.Draw(Out, L++, White());
	}
	for (int32 K = 0; K < 6; ++K)
	{
		const FVector4f R(OpS::MAIN_X + K * (153.33f + 8), Y, 153.33f, 158);
		if (K == 5)
		{
			// o AUTO: a borda tracejada
			const bool bOn = K == Pre && !bCustom;
			const FLinearColor Bd = bOn ? Ac(0.85f) : FMath::Lerp(Css(230, 220, 190, 0.10f), Css(230, 220, 190, 0.3f), HoverOf(EHit::Preset, K));
			Dashed(Out, L, S, { FVector2f(R.X + 0.5f, R.Y + 0.5f), FVector2f(R.X + R.Z - 0.5f, R.Y + 0.5f), FVector2f(R.X + R.Z - 0.5f, R.Y + R.W - 0.5f),
				FVector2f(R.X + 0.5f, R.Y + R.W - 0.5f), FVector2f(R.X + 0.5f, R.Y + 0.5f) }, Bd, 1.0f, 3.0f);
		}
		Text(Out, L, S, OpS::PRESET_N[K], Font(EFace::Shoulders900, 28, 2), R.X + 15, R.Y + 40, FLinearColor::White);
		const FSlateFontInfo Df = Font(EFace::Barlow600, 11.5f, 1.5f);
		const TArray<FString> Ln = OpS::Wrap(K == 5 ? FString(TEXT("MEASURED · HIGH")) : FString(OpS::PRESET_D[K]), Df, 123.33f, Scale);
		for (int32 I = 0; I < Ln.Num(); ++I) Text(Out, L, S, Ln[I], Df, R.X + 15, R.Y + 61 + 14.95f * I, OpS::Mute());
		const FSlateFontInfo Ff = Font(EFace::Mono600, 14);
		const float Fw = Text(Out, L, S, FString::Printf(TEXT("%d "), E[K].Fps), Ff, R.X + 15, R.Y + 141, Css(238, 241, 244, 0.85f));
		Text(Out, L, S, TEXT("FPS"), Font(EFace::Mono600, 11), R.X + 15 + Fw, R.Y + 141, OpS::Mute());
		if (K == 4 && E[K].Fps < 45) Text(Out, L, S, TEXT("ABOVE THIS PC"), Font(EFace::Barlow700, 10, 1.5f), R.X + 15, R.Y + 121, Css(240, 110, 90));
	}
	++L;
}

void SRTSOptions::PaintRow(FSlateWindowElementList& Out, int32& L, const FGeometry& S, const FItem& It, int32 Index, float Y) const
{
	const float H = HoverOf(EHit::Row, Index);
	{
		FMesh M(S);
		BoxRect(M, FVector4f(OpS::ROW_X, Y, OpS::ROW_W, 50), CssV(16, 21, 19, 0.62f), FMath::Lerp(Css(230, 220, 190, 0.10f), Css(230, 220, 190, 0.28f), H).ToFColorSRGB());
		if (It.Opts.Num() == 0)
		{
			// o deslizante: a trilha, o cheio âmbar e o botão creme com a sombra
			const float F = float(Val(It.Key) - It.Min) / float(FMath::Max(1, It.Max - It.Min));
			M.Rect(OpS::SEG_X, Y + 23, 460, 4, CssV(230, 220, 190, 0.14f));
			M.Rect(OpS::SEG_X, Y + 23, 460 * F, 4, AcV());
			GlowRect(M, FVector4f(OpS::SEG_X + 460 * F - 7, Y + 18, 14, 18), 0x000000, 0.6f, 6.0f);
			M.Rect(OpS::SEG_X + 460 * F - 7, Y + 16, 14, 18, CssV(233, 226, 207));
		}
		else
		{
			const TArray<FVector4f> Sb = SegBoxes(It, Y);
			for (int32 O = 0; O < Sb.Num(); ++O)
			{
				const bool bOn = Val(It.Key) == O;
				const bool bNa = It.Na.Contains(O) || (FCString::Strcmp(It.Key, TEXT("tips")) == 0 && O == 2 && Val(TEXT("hudclean")) == 0);
				const float Hs = bNa ? 0.0f : HoverOf(EHit::Seg, Index, O);
				FColor Bd = bOn ? AcV(0.7f) : FMath::Lerp(Css(230, 220, 190, 0.12f), Css(230, 220, 190, 0.3f), Hs).ToFColorSRGB();
				if (bNa) Bd.A = uint8(Bd.A * 0.3f);
				BoxRect(M, Sb[O], bOn ? CssV(216, 173, 78, 0.07f) : FColor(0, 0, 0, 0), Bd);
			}
		}
		M.Draw(Out, L++, White());
	}
	Text(Out, L, S, It.Name, Font(EFace::Barlow700, 15, 3), OpS::ROW_X + 15, Y + 31, Css(238, 241, 244, 0.88f));
	if (It.Opts.Num() == 0)
	{
		Text(Out, L, S, FString::Printf(TEXT("%d%s"), Val(It.Key), FCString::Strcmp(It.Key, TEXT("uiscale")) == 0 ? TEXT("%") : TEXT("")), Font(EFace::Mono600, 14), 1205, Y + 30,
			Css(238, 241, 244, 0.8f));
	}
	else
	{
		const TArray<FVector4f> Sb = SegBoxes(It, Y);
		const FSlateFontInfo F = Font(EFace::Barlow700, 13, 1.5f);
		for (int32 O = 0; O < Sb.Num(); ++O)
		{
			const bool bOn = Val(It.Key) == O;
			const bool bNa = It.Na.Contains(O) || (FCString::Strcmp(It.Key, TEXT("tips")) == 0 && O == 2 && Val(TEXT("hudclean")) == 0);
			FLinearColor C = bOn ? Ac() : FMath::Lerp(Css(238, 241, 244, 0.55f), FLinearColor::White, bNa ? 0.0f : HoverOf(EHit::Seg, Index, O));
			if (bNa) C.A *= 0.3f;
			const float W = Text(Out, L, S, It.Opts[O], F, Sb[O].X + 12, Y + 30, C);
			if (bNa)
			{
				// o riscado (text-decoration: line-through) no meio da altura das letras
				FMesh M(S);
				M.Rect(Sb[O].X + 12, Y + 25.5f, W - 1.5f, 1, C.ToFColorSRGB());
				M.Draw(Out, L + 1, White());
			}
		}
	}
	L += 2;
}

void SRTSOptions::PaintKeyRow(FSlateWindowElementList& Out, int32& L, const FGeometry& S, int32 Key, float Y) const
{
	const float Scale = S.GetAccumulatedLayoutTransform().GetScale();
	const OpS::FKey& K = OpS::KEYS[Key];
	// a tecla repetida: a linha e a tecla em vermelho, e com quem divide
	int32 Other = -1;
	for (int32 I = 0; I < 10; ++I) if (I != Key && FCString::Strcmp(OpS::KEYS[I].Key, K.Key) == 0) { Other = I; break; }
	const FSlateFontInfo Kf = Font(EFace::Mono600, 15);
	const float Kw = FMath::Max(56.0f, TextWidth(K.Key, Kf, Scale) + 22.0f);
	const float H = HoverOf(EHit::Row, -1, Key);
	{
		FMesh M(S);
		const FColor Bd = Other >= 0 ? CssV(240, 110, 90, 0.6f) : FMath::Lerp(Css(230, 220, 190, 0.10f), Css(230, 220, 190, 0.28f), H).ToFColorSRGB();
		BoxRect(M, FVector4f(OpS::ROW_X, Y, OpS::ROW_W, 50), CssV(16, 21, 19, 0.62f), Bd);
		BoxRect(M, FVector4f(OpS::SEG_X, Y + 8, Kw, 34), CssV(0, 0, 0, 0.3f), Other >= 0 ? CssV(240, 110, 90) : CssV(230, 220, 190, 0.3f));
		M.Draw(Out, L++, White());
	}
	Text(Out, L, S, K.Action, Font(EFace::Barlow700, 15, 3), OpS::ROW_X + 15, Y + 31, Css(238, 241, 244, 0.88f));
	Text(Out, L, S, K.Key, Kf, OpS::SEG_X + Kw * 0.5f, Y + 30.5f, Other >= 0 ? Css(240, 110, 90) : FLinearColor::White, 1);
	if (Other >= 0)
	{
		Text(Out, L, S, FString::Printf(TEXT("ALSO USED BY %s"), OpS::KEYS[Other].Action), Font(EFace::Barlow700, 12, 2), OpS::ROW_X + OpS::ROW_W - 15, Y + 30,
			Css(240, 110, 90), 2);
	}
	++L;
}

void SRTSOptions::PaintInfo(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	const float Scale = S.GetAccumulatedLayoutTransform().GetScale();
	const float X = OpS::INFO_X + 23;
	const float TextW = OpS::INFO_W - 2 - 44;
	// o cartão: a qualidade, ou o item com o mouse em cima (o grupo, o nome, o que faz e o custo de placa e de memória)
	const FItem* It = Tabs[Tab].IsValidIndex(CardItem) ? &Tabs[Tab][CardItem] : nullptr;
	const FString Lab = It ? FString(It->Group) : FString(TEXT("QUALITY"));
	const FString Title = It ? FString(It->Name) : bCustom ? FString(TEXT("CUSTOM")) : FString(OpS::PRESET_N[Pre]);
	const FString P1 = It ? FString(It->Desc) : bCustom ? FString(TEXT("Your own mix. Pick a preset to start over.")) :
		FString(TEXT("Every graphics setting below follows this preset. Change any of them and it becomes your own mix."));
	const FSlateFontInfo Pf = Font(EFace::Barlow500, 16);
	const TArray<FString> Ln = OpS::Wrap(P1, Pf, TextW, Scale);
	float Bottom = 179.0f + 23.2f * Ln.Num();
	TArray<FString> Ln2;
	const FSlateFontInfo P2f = Font(EFace::Barlow500, 14);
	if (!It) { Ln2 = OpS::Wrap(TEXT("Point at a setting to see what it does and what it costs."), P2f, TextW, Scale); Bottom += 12.0f + 20.3f * Ln2.Num(); }
	const bool bCost = It && (It->W > 0.0f || It->M > 0.0f);
	const float CostY = 179.0f + 23.2f * FMath::Max(0, Ln.Num() - 1) + 37.19f;
	if (bCost) Bottom = CostY + 36.0f;
	const float CardH = FMath::Max(294.0f, Bottom + 22.0f - 96.0f + 1.0f);
	{
		FMesh M(S);
		BoxRect(M, FVector4f(OpS::INFO_X, 96, OpS::INFO_W, CardH), CssV(16, 21, 19, 0.62f), CssV(230, 220, 190, 0.10f));
		if (bCost)
		{
			const int32 Gpu = It->W > 0.0f ? FMath::Min(5, FMath::CeilToInt(It->W / 1.2f)) : 0, Vram = It->M > 0.0f ? FMath::Min(5, FMath::CeilToInt(It->M / 0.45f)) : 0;
			for (int32 Rw = 0; Rw < 2; ++Rw)
			{
				const int32 V = Rw == 0 ? Gpu : Vram;
				for (int32 I = 0; I < 5; ++I)
				{
					const FColor C = I < V ? (V >= 4 ? CssV(240, 110, 90) : AcV()) : CssV(230, 220, 190, 0.12f);
					M.Rect(1555 + I * 30.0f, CostY + 3.5f + Rw * 22.0f, 26, 7, C);
				}
			}
		}
		M.Draw(Out, L++, White());
	}
	Text(Out, L, S, Lab, Font(EFace::Barlow600, 11.5f, 4), X, 129, Css(230, 220, 190, 0.45f));
	Text(Out, L, S, Title, Font(EFace::Shoulders900, 30, 2), X, 162, FLinearColor::White);
	for (int32 I = 0; I < Ln.Num(); ++I) Text(Out, L, S, Ln[I], Pf, X, 194.99f + 23.2f * I, Css(238, 241, 244, 0.78f));
	for (int32 I = 0; I < Ln2.Num(); ++I) Text(Out, L, S, Ln2[I], P2f, X, 179.0f + 23.2f * Ln.Num() + 12.0f + 14.0f + 20.3f * I, OpS::Mute());
	if (bCost)
	{
		Text(Out, L, S, TEXT("GPU"), Font(EFace::Barlow600, 12, 3), X, CostY + 12, OpS::Mute());
		Text(Out, L, S, TEXT("MEMORY"), Font(EFace::Barlow600, 12, 3), X, CostY + 34, OpS::Mute());
	}
	++L;
	// o medidor (só nos gráficos): os quadros estimados, a memória de vídeo na barra e a placa
	if (Tab != 0) return;
	int32 Fps = 0;
	float Gb = 0.0f;
	Estimate(Opt, Fps, Gb);
	const bool bOver = Gb > 6.0f;
	const float My = 96 + CardH + 12;
	{
		FMesh M(S);
		BoxRect(M, FVector4f(OpS::INFO_X, My, OpS::INFO_W, 155), CssV(16, 21, 19, 0.62f), CssV(230, 220, 190, 0.10f));
		M.Rect(X, My + 77, 354, 10, CssV(230, 220, 190, 0.1f));
		M.Rect(X, My + 77, 354 * FMath::Min(1.0f, Gb / 6.0f), 10, bOver ? CssV(240, 110, 90) : AcV());
		M.Draw(Out, L++, White());
	}
	const float Fw = Text(Out, L, S, FString::FromInt(Fps), Font(EFace::Shoulders900, 46, 2), X, My + 59, FLinearColor::White);
	Text(Out, L, S, TEXT("FPS"), Font(EFace::Shoulders900, 18, 2), X + Fw + 6, My + 59, OpS::Mute());
	Text(Out, L, S, TEXT("ESTIMATED IN BATTLE"), Font(EFace::Barlow600, 11.5f, 4), 1827, My + 59, Css(230, 220, 190, 0.45f), 2);
	Text(Out, L, S, FString::Printf(TEXT("VIDEO MEMORY %s GB"), *OpS::JsNum(Gb)), Font(EFace::Mono600, 13), X, My + 108, Css(238, 241, 244, 0.75f));
	Text(Out, L, S, TEXT("6.0 GB"), Font(EFace::Mono600, 13), 1827, My + 108, Css(238, 241, 244, 0.75f), 2);
	Text(Out, L, S, bOver ? TEXT("ABOVE THIS CARD'S MEMORY · EXPECT STUTTERS") : TEXT("NVIDIA GEFORCE GTX 1660 · 6 GB"), Font(EFace::Barlow600, 12, 3), X, My + 134, OpS::Mute());
	++L;
}

void SRTSOptions::PaintFoot(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	// o RESET (escuro) e o APPLY (âmbar com o brilho); no hover o espaçamento abre
	const bool bOn = IsDirty();
	const float Hr = HoverOf(EHit::Reset), Ha = bOn ? HoverOf(EHit::Apply) : 0.0f;
	{
		FMesh M(S);
		BoxRect(M, OpS::RESET, CssV(16, 21, 19, 0.6f), FMath::Lerp(Css(230, 220, 190, 0.22f), Css(230, 220, 190, 0.45f), Hr).ToFColorSRGB());
		if (bOn)
		{
			GlowRect(M, OpS::APPLY, 0xd8ad4e, 0.25f + 0.2f * Ha, 40.0f + 20.0f * Ha);
			BoxRect(M, OpS::APPLY, AcV(), AcV());
		}
		else
		{
			BoxRect(M, OpS::APPLY, CssV(230, 220, 190, 0.12f), FColor(0, 0, 0, 0));
		}
		M.Draw(Out, L++, White());
	}
	const float Lr = 5.0f + 2.0f * Hr, La = 5.0f + 2.0f * Ha;
	Text(Out, L, S, TEXT("RESET"), Font(EFace::Shoulders900, 28, Lr), OpS::RESET.X + 120, 1009, FMath::Lerp(Css(238, 241, 244, 0.9f), FLinearColor::White, Hr), 1);
	Text(Out, L, S, TEXT("APPLY"), Font(EFace::Shoulders900, 28, La), OpS::APPLY.X + 120, 1009, bOn ? Css(10, 13, 11) : Css(238, 241, 244, 0.45f), 1);
	++L;
}
