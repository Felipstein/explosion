#include "BlastLabPanel.h"

#include "BlastLabCamera.h"
#include "BlastLabEnvironment.h"
#include "BlastLabMover.h"
#include "BlastLabSimulation.h"
#include "Celestial.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Scalability.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "BlastLabPanel"

EBlastQuality SBlastLabPanel::CurrentQuality = EBlastQuality::High;

namespace
{
	// paleta do WebGL (index.html :root): metal escuro + âmbar
	FLinearColor Hex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }
	const FLinearColor Metal1 = Hex(TEXT("2a2d26")), Metal2 = Hex(TEXT("191b16")), Metal3 = Hex(TEXT("0e100d"));
	const FLinearColor PanelBg = Hex(TEXT("1c1f18")), BrandBg = Hex(TEXT("2b2f24"));
	const FLinearColor Amber = Hex(TEXT("ffb347")), AmberDim = Hex(TEXT("8c6a32")), Green = Hex(TEXT("8ddf6a"));
	const FLinearColor Txt = Hex(TEXT("cfd2c4")), TxtDim = Hex(TEXT("7c8073"));
	const FLinearColor CardBg = Hex(TEXT("0b0c09")), ReadoutBg = Hex(TEXT("070806"));
	const FLinearColor BtnBg = Hex(TEXT("20231c")), BtnHover = Hex(TEXT("2b2f25")), BtnOn = Hex(TEXT("e0a858")), BtnOnTxt = Hex(TEXT("13150f"));
	const FLinearColor BevelHi(0.75f, 0.78f, 0.67f, 0.22f), BevelLo(0.0f, 0.0f, 0.0f, 0.72f);

	const FSlateBrush* White() { return FCoreStyle::Get().GetBrush("GenericWhiteBox"); }

	FSlateFontInfo Mono(int32 Size, int32 LetterSpacing = 0)
	{
		FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Mono", Size);
		F.LetterSpacing = LetterSpacing;
		return F;
	}

	const FButtonStyle& FlatButton()
	{
		static FButtonStyle Style = []
		{
			FButtonStyle S;
			S.SetNormal(FSlateNoResource());
			S.SetHovered(FSlateNoResource());
			S.SetPressed(FSlateNoResource());
			S.SetDisabled(FSlateNoResource());
			S.SetNormalPadding(FMargin(0));
			S.SetPressedPadding(FMargin(0, 1, 0, 0));
			return S;
		}();
		return Style;
	}

	// presets de hora do painel do WebGL (index.html). A "dourada" de lá (17:27)
	// cai com o sol 2° ABAIXO do horizonte (latitude −23.5°, junho): sem luz
	// direta, o chão saía magenta do céu do crepúsculo (medido 06/10: matiz −31°
	// contra 63–72° nas artes de hora dourada). Agora com o sol a +6.4°.
	struct FPreset { const TCHAR* Label; float Hours; };
	const FPreset Presets[] = {
		{ TEXT("amanhecer"), 5.35f }, { TEXT("manhã"), 8.5f }, { TEXT("meio-dia"), 12.6f },
		{ TEXT("dourada"), 16.75f }, { TEXT("crepúsculo"), 18.55f }, { TEXT("noite"), 22.5f },
	};

	// os campos: o de caixas (o layout do WebGL) e os de chão de verdade, feitos
	// das fatias do WorldForge (Tools/blast.ps1 worlds); só aparece o que foi montado
	struct FRangeMap { const TCHAR* Label; const TCHAR* Map; const TCHAR* Tip; };
	const FRangeMap Ranges[] = {
		{ TEXT("teste"), TEXT("/Game/Maps/TestRange"), TEXT("o campo de teste do WebGL: caixas e chão liso") },
		{ TEXT("deserto"), TEXT("/Game/Maps/Range_Desert"), TEXT("deserto com estrada de asfalto (fatia do WorldForge)") },
		{ TEXT("neve"), TEXT("/Game/Maps/Range_Snow"), TEXT("neve com pinheiros e rio congelado (fatia do WorldForge)") },
		{ TEXT("floresta"), TEXT("/Game/Maps/Range_Forest"), TEXT("campo de grama com mata (fatia do WorldForge)") },
		{ TEXT("arena"), TEXT("/Game/Maps/Range_Arena"), TEXT("deserto, neve, grama e estrada juntos (WorldForge)") },
	};

	// Nível do motor e resolução de render de cada preset (o TSR amplia pra
	// tela). Medido ao vivo na GTX 1660 a 1600×900 (05/10): a GPU é o gargalo e
	// o custo segue a área — Épico a 100% ~50 ms, a 50% ~11 ms. Ultra é o
	// visual máximo (Cinematic, resolução cheia), mesmo que trave.
	int32 EngineLevel(EBlastQuality Q)
	{
		switch (Q)
		{
		case EBlastQuality::Low: return 1;       // Médio do motor (o Baixo desliga quase tudo)
		case EBlastQuality::Medium: return 2;    // Alto
		case EBlastQuality::High: return 3;      // Épico
		default: return 4;                       // Cinematic
		}
	}

	float ResolutionPct(EBlastQuality Q)
	{
		switch (Q)
		{
		case EBlastQuality::Low: return 50.0f;
		case EBlastQuality::Medium: return 58.0f;
		case EBlastQuality::High: return 67.0f;
		default: return 100.0f;
		}
	}
}

void SBlastLabPanel::ApplyQuality(EBlastQuality Q)
{
	CurrentQuality = Q;
	Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
	Levels.SetFromSingleQualityLevel(EngineLevel(Q));
	Levels.ResolutionQuality = ResolutionPct(Q);
	Scalability::SetQualityLevels(Levels);
}

void SBlastLabPanel::Construct(const FArguments& InArgs)
{
	World = InArgs._World;
	if (InArgs._Thumbnail)
	{
		ThumbBrush = MakeShared<FSlateBrush>();
		ThumbBrush->SetResourceObject(InArgs._Thumbnail);
		ThumbBrush->ImageSize = FVector2D(220.0, 137.5);
	}

	TSharedRef<SUniformGridPanel> Cards = SNew(SUniformGridPanel).SlotPadding(FMargin(2.0f));
	for (int32 i = 0, n = 0; i < UBlastLabSimulation::Arsenal().Num(); ++i)
	{
		if (!UBlastLabSimulation::Arsenal()[i].bHidden)
		{
			Cards->AddSlot(n % 2, n / 2)[WeaponCard(i)];
			++n;
		}
	}

	TSharedRef<SWrapBox> PresetRow = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(3.0, 3.0));
	for (const FPreset& P : Presets)
	{
		const float Hours = P.Hours;
		PresetRow->AddSlot()[Button(FText::FromString(P.Label),
			[this, Hours]
			{
				const ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(World.Get());
				return Env && FMath::Abs(FMath::Fmod(Hours - Env->TimeOfDay + 36.0f, 24.0f) - 12.0f) > 11.96f;
			},
			[this, Hours]
			{
				if (ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(World.Get()))
				{
					Env->bAutoCycle = false;
					Env->SetTimeOfDay(Hours);
				}
			})];
	}
	PresetRow->AddSlot()[Button(LOCTEXT("Cycle", "ciclo"),
		[this] { const ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(World.Get()); return Env && Env->bAutoCycle; },
		[this] { if (ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(World.Get())) Env->bAutoCycle = !Env->bAutoCycle; },
		LOCTEXT("CycleTip", "ciclo automático (tecla t)"))];
	// o ar sujo do deserto (aerossol de poeira e névoa quente): o sol baixo fica dourado
	PresetRow->AddSlot()[Button(LOCTEXT("DesertAir", "poeira no ar"),
		[this] { const ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(World.Get()); return Env && Env->bDesertAir; },
		[this] { if (ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(World.Get())) Env->SetDesertAir(!Env->bDesertAir); },
		LOCTEXT("DesertAirTip", "ar de deserto: aerossol de poeira e névoa quente (com a hora dourada, o vídeo)"))];

	// trocar de campo leva a hora junto (BlastLabController lê a opção TimeOfDay)
	TSharedRef<SWrapBox> FieldRow = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(3.0, 3.0));
	for (const FRangeMap& F : Ranges)
	{
		if (!FPackageName::DoesPackageExist(F.Map))
		{
			continue;
		}
		const FString Map = F.Map;
		FieldRow->AddSlot()[Button(FText::FromString(F.Label),
			[this, Map] { return World.IsValid() && World->GetOutermost()->GetName() == Map; },
			[this, Map]
			{
				const ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(World.Get());
				UGameplayStatics::OpenLevel(World.Get(), FName(*Map), true,
					Env ? FString::Printf(TEXT("TimeOfDay=%.4f"), Env->TimeOfDay) : FString());
			},
			FText::FromString(F.Tip))];
	}

	TSharedRef<SHorizontalBox> QualityRow = SNew(SHorizontalBox);
	const TCHAR* QualityLabels[] = { TEXT("baixa"), TEXT("média"), TEXT("alta"), TEXT("ultra") };
	for (int32 q = 0; q < 4; ++q)
	{
		const EBlastQuality Q = static_cast<EBlastQuality>(q);
		QualityRow->AddSlot().FillWidth(1.0f).Padding(1.5f, 0.0f)[Button(FText::FromString(QualityLabels[q]),
			[Q] { return CurrentQuality == Q; }, [Q] { ApplyQuality(Q); },
			FText::Format(LOCTEXT("QualityTip", "qualidade {0} (tecla {1})"), FText::FromString(QualityLabels[q]), FText::AsNumber(q + 1)))];
	}
	// acessibilidade: o tremor da câmera quando o choque chega
	QualityRow->AddSlot().FillWidth(1.0f).Padding(1.5f, 0.0f)[Button(LOCTEXT("Shake", "tremor"),
		[] { return ABlastLabCamera::bShakeEnabled; }, [] { ABlastLabCamera::bShakeEnabled = !ABlastLabCamera::bShakeEnabled; },
		LOCTEXT("ShakeTip", "tremor da câmera quando o choque chega (acessibilidade)"))];

	// avião, míssil e disparo atravessando a fumaça da última explosão (aceso enquanto passa)
	auto Pass = [this](const FText& Label, EBlastMover Kind, const FText& Tip)
	{
		return Button(Label,
			[this, Kind] { const UBlastLabSimulation* Sim = UBlastLabSimulation::Get(World.Get()); return Sim && Sim->IsMoverActive(Kind); },
			[this, Kind] { if (UBlastLabSimulation* Sim = UBlastLabSimulation::Get(World.Get())) Sim->SpawnMover(Kind); },
			Tip);
	};
	auto Stat = [](const FText& Label, TAttribute<FText> Value)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(70.0f)[SNew(STextBlock).Text(Label).Font(Mono(8, 80)).ColorAndOpacity(AmberDim)]]
			+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(STextBlock).Text(Value).Font(Mono(8)).ColorAndOpacity(Txt)];
	};

	ChildSlot
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Fill)
	[
		SNew(SBox).WidthOverride(Width)
		[
			// o fundo do painel segura o clique: sem isso, clicar num vão
			// entre os botões detonaria no terreno atrás dele
			SNew(SBorder)
			.BorderImage(White())
			.BorderBackgroundColor(Metal2)
			.Padding(FMargin(8.0f, 8.0f))
			.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
			[
				SNew(SVerticalBox)
				// ---- marca + qps ----
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[
					SNew(SBorder).BorderImage(White()).BorderBackgroundColor(BrandBg).Padding(FMargin(9.0f, 6.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(STextBlock).Text(LOCTEXT("Brand", "ARSENAL")).Font(Mono(11, 220)).ColorAndOpacity(Amber)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(Mono(8, 100)).ColorAndOpacity(TxtDim)
							.Text_Lambda([this] { return FpsSmoothed > 0.0f ? FText::Format(LOCTEXT("Fps", "{0} QPS"), FText::AsNumber(FMath::RoundToInt(FpsSmoothed))) : FText::FromString(TEXT("—")); })
						]
					]
				]
				// ---- armamento ----
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[
					// o "relógio de construção" do WebGL: enquanto os shaders das
					// explosões compilam, a primeira detonação sairia sem bola de fogo
					Section(LOCTEXT("Weapons", "ARMAMENTO"), Cards, TAttribute<FText>::CreateLambda([this]
					{
						const UBlastLabSimulation* Sim = UBlastLabSimulation::Get(World.Get());
						if (!Sim || !Sim->IsPreparing()) return FText();
						const int32 N = UBlastLabSimulation::PendingShaders();
						return N > 0 ? FText::Format(LOCTEXT("Preparing", "preparando {0}"), FText::AsNumber(N)) : LOCTEXT("Warming", "preparando…");
					}))
				]
				// ---- passagem pela fumaça ----
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[
					Section(LOCTEXT("Pass", "PASSAGEM PELA FUMAÇA"),
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(1.5f, 0.0f)[Pass(LOCTEXT("Plane", "avião"), EBlastMover::Plane, LOCTEXT("PlaneTip", "avião de ataque atravessando a fumaça (tecla a)"))]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(1.5f, 0.0f)[Pass(LOCTEXT("Missile", "míssil"), EBlastMover::Missile, LOCTEXT("MissileTip", "míssil terra-ar subindo pela fumaça (tecla m)"))]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(1.5f, 0.0f)[Pass(LOCTEXT("Shell", "disparo"), EBlastMover::Shell, LOCTEXT("ShellTip", "disparo de tanque atravessando a fumaça (tecla d)"))]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(1.5f, 0.0f)[Pass(LOCTEXT("Bomber", "B-2"), EBlastMover::Bomber, LOCTEXT("BomberTip", "bombardeiro pesado (classe B-2, 150 t) atravessando a fumaça: a esteira grande (tecla b)"))])
				]
				// ---- ambiente ----
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[
					Section(LOCTEXT("Environment", "AMBIENTE"),
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
						[
							SNew(SSlider)
							.IsFocusable(false)
							.SliderBarColor(Metal1)
							.SliderHandleColor(Txt)
							.Value_Lambda([this] { const ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(World.Get()); return Env ? Env->TimeOfDay / 24.0f : 0.5f; })
							.OnValueChanged_Lambda([this](float V)
							{
								if (ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(World.Get()))
								{
									Env->bAutoCycle = false;
									Env->SetTimeOfDay(FMath::RoundToFloat(V * 1439.0f) / 60.0f);
								}
							})
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
						[
							SNew(SBorder).BorderImage(White()).BorderBackgroundColor(ReadoutBg).Padding(FMargin(7.0f, 4.0f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0, 0, 8, 0)
								[
									SNew(STextBlock).Font(Mono(13, 60)).ColorAndOpacity(Green)
									.Text_Lambda([this]
									{
										const ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(World.Get());
										const int32 Minutes = Env ? FMath::FloorToInt(Env->TimeOfDay * 60.0f + 0.5f) % 1440 : 0;
										return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes / 60, Minutes % 60));
									})
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Bottom)
								[
									SNew(STextBlock).Font(Mono(8, 60)).ColorAndOpacity(TxtDim)
									.Text_Lambda([this]
									{
										const ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(World.Get());
										if (!Env) return FText();
										const float Alt = Env->SunAltitudeDeg();
										return FText::FromString(FString::Printf(TEXT("%+.0f° · %s"), Alt, FCelestial::PhaseName(Alt)));
									})
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight()[PresetRow])
				]
				// ---- campo ----
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[
					Section(LOCTEXT("Field", "CAMPO"), FieldRow)
				]
				// ---- sistema ----
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[
					Section(LOCTEXT("System", "SISTEMA"),
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[QualityRow]
						+ SVerticalBox::Slot().AutoHeight()[Stat(LOCTEXT("Alive", "explosões"), TAttribute<FText>::CreateLambda([this]
							{
								const UBlastLabSimulation* Sim = UBlastLabSimulation::Get(World.Get());
								return FText::AsNumber(Sim ? Sim->NumAlive() : 0);
							}))]
						+ SVerticalBox::Slot().AutoHeight()[Stat(LOCTEXT("Time", "tempo"), TAttribute<FText>::CreateLambda([this]
							{
								const UBlastLabSimulation* Sim = UBlastLabSimulation::Get(World.Get());
								if (!Sim) return FText();
								return Sim->bPaused ? LOCTEXT("Paused", "pausado (P)")
									: FText::FromString(FString::Printf(TEXT("%.2f×  ([ ] \\)"), Sim->TimeScale));
							}))])
				]
				// ---- dica de uso ----
				+ SVerticalBox::Slot().FillHeight(1.0f).VAlign(VAlign_Bottom)
				[
					SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Metal3).Padding(FMargin(6.0f, 5.0f))
					[
						SNew(STextBlock).Font(Mono(7, 90)).ColorAndOpacity(TxtDim).Justification(ETextJustify::Center)
						.Text(LOCTEXT("Hint", "CLIQUE NO TERRENO para detonar\narraste para orbitar · roda: zoom\nbotão direito arrastado: rolar o mapa"))
					]
				]
			]
		]
	];
	SetVisibility(EVisibility::SelfHitTestInvisible);
}

TSharedRef<SWidget> SBlastLabPanel::Section(const FText& Title, TSharedRef<SWidget> Content, TAttribute<FText> Status)
{
	return SNew(SBorder).BorderImage(White()).BorderBackgroundColor(PanelBg).Padding(FMargin(8.0f, 7.0f))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(STextBlock).Text(Title).Font(Mono(7, 200)).ColorAndOpacity(AmberDim)]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(Status).Font(Mono(7, 80)).ColorAndOpacity(Amber)]
		]
		+ SVerticalBox::Slot().AutoHeight()[Content]
	];
}

TSharedRef<SWidget> SBlastLabPanel::Button(const FText& Label, TFunction<bool()> IsOn, TFunction<void()> OnClick, const FText& Tooltip, bool bEnabled)
{
	TSharedPtr<SButton> B;
	SAssignNew(B, SButton)
	.ButtonStyle(&FlatButton())
	.IsFocusable(false)                           // espaço e enter não reclicam o botão
	.IsEnabled(bEnabled)
	.ToolTipText(Tooltip)
	.OnClicked_Lambda([OnClick] { OnClick(); return FReply::Handled(); })
	[
		SNew(SBorder).BorderImage(White()).Padding(FMargin(4.0f, 4.0f)).HAlign(HAlign_Center)
		.BorderBackgroundColor_Lambda([IsOn, bEnabled, WeakB = TWeakPtr<SButton>()]() { return FSlateColor(IsOn() ? BtnOn : BtnBg); })
		[
			SNew(STextBlock).Text(Label).Font(Mono(7, 60))
			.ColorAndOpacity_Lambda([IsOn, bEnabled] { return FSlateColor(IsOn() ? BtnOnTxt : bEnabled ? TxtDim : TxtDim.CopyWithNewOpacity(0.45f)); })
		]
	];
	return B.ToSharedRef();
}

TSharedRef<SWidget> SBlastLabPanel::WeaponCard(int32 Index)
{
	const FBlastWeapon& W = UBlastLabSimulation::Arsenal()[Index];
	auto IsSelected = [this, Index]
	{
		const UBlastLabSimulation* Sim = UBlastLabSimulation::Get(World.Get());
		return Sim && Sim->SelectedWeapon == Index;
	};
	return SNew(SButton)
	.ButtonStyle(&FlatButton())
	.IsFocusable(false)
	.ToolTipText(FText::FromString(FString::Printf(TEXT("%s — %s"), W.Name, W.Meta)))
	.OnClicked_Lambda([this, Index]
	{
		if (UBlastLabSimulation* Sim = UBlastLabSimulation::Get(World.Get()))
		{
			Sim->SelectedWeapon = Index;
		}
		return FReply::Handled();
	})
	[
		// moldura: âmbar na arma escolhida
		SNew(SBorder).BorderImage(White()).Padding(FMargin(1.0f))
		.BorderBackgroundColor_Lambda([IsSelected] { return FSlateColor(IsSelected() ? Amber : FLinearColor::Black); })
		[
			SNew(SBorder).BorderImage(White()).BorderBackgroundColor(CardBg).Padding(0)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SBox).HeightOverride(64.0f)
						[
							SNew(SImage).Image(ThumbBrush.IsValid() ? ThumbBrush.Get() : White())
							.ColorAndOpacity(ThumbBrush.IsValid() ? FLinearColor::White : CardBg)
						]
					]
					// clarão ao detonar (o "flash" do cartão no WebGL)
					+ SOverlay::Slot()
					[
						SNew(SImage).Image(White()).Visibility(EVisibility::HitTestInvisible)
						.ColorAndOpacity_Lambda([this, Index]
						{
							const float Age = float(FPlatformTime::Seconds() - FlashStart);
							const float A = (FlashWeapon == Index && Age < 0.45f) ? 0.55f * (1.0f - Age / 0.45f) : 0.0f;
							return FSlateColor(FLinearColor(1.0f, 0.85f, 0.6f, A));
						})
					]
					// tamanho relativo: W^(1/3)
					+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0, 3, 4, 0)
					[
						SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FLinearColor(0, 0, 0, 0.62f)).Padding(FMargin(3.0f, 0.0f))
						[
							SNew(STextBlock).Font(Mono(6, 40)).ColorAndOpacity(Amber)
							.Text(FText::FromString(FString::Printf(TEXT("%.2f×"), W.SizeScale())))
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(5.0f, 3.0f, 5.0f, 4.0f))
				[
					SNew(STextBlock).Text(FText::FromString(FString(W.Name).ToUpper())).Font(Mono(7, 60))
					.ColorAndOpacity_Lambda([IsSelected] { return FSlateColor(IsSelected() ? Amber : Txt); })
				]
			]
		]
	];
}

void SBlastLabPanel::Fired(int32 Weapon)
{
	FlashWeapon = Weapon;
	FlashStart = FPlatformTime::Seconds();
}

void SBlastLabPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	// média exponencial de ~0.5 s (o WebGL atualizava o leitor a cada 0.25 s)
	const float Dt = FMath::Max(float(FApp::GetDeltaTime()), 1e-4f);
	FpsSmoothed = FpsSmoothed <= 0.0f ? 1.0f / Dt : FMath::Lerp(FpsSmoothed, 1.0f / Dt, 1.0f - FMath::Exp(-Dt / 0.5f));
}

#undef LOCTEXT_NAMESPACE
