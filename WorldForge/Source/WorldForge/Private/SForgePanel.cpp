#include "SForgePanel.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "ForgeCameraPawn.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Scalability.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "WorldForgeGameMode.h"
#include "WorldForgeTerrain.h"

#define LOCTEXT_NAMESPACE "WorldForge"

namespace
{
	const FLinearColor Accent(0.95f, 0.55f, 0.10f);          // laranja de C&C
	const FLinearColor ButtonIdle(0.10f, 0.11f, 0.12f);
	const FLinearColor TextDim(0.62f, 0.66f, 0.70f);
	constexpr float PanelWidth = 330.0f;
	constexpr float LabelWidth = 86.0f;

	FSlateFontInfo Font(int32 Size, const TCHAR* Style = TEXT("Regular"))
	{
		return FCoreStyle::GetDefaultFontStyle(Style, Size);
	}

	TSharedRef<SWidget> Heading(const FText& Text)
	{
		return SNew(STextBlock).Text(Text).Font(Font(9, TEXT("Bold"))).ColorAndOpacity(FSlateColor(Accent));
	}
}

void SForgePanel::Construct(const FArguments& InArgs)
{
	TSharedPtr<SButton> DiceButton;
	TSharedPtr<SBorder> MinimapBorder;
	GameMode = InArgs._GameMode;
	if (GameMode.IsValid())
	{
		Edit = GameMode->GetSettings();
		MapShownHandle = GameMode->OnMapShown.AddSP(this, &SForgePanel::OnMapShown);
	}
	MinimapBrush.ImageSize = FVector2D(PanelWidth - 32.0f, PanelWidth - 32.0f);
	SliderStyle = FSliderStyle()
		.SetNormalBarImage(FSlateRoundedBoxBrush(FLinearColor(0.20f, 0.22f, 0.25f), 3.0f))
		.SetHoveredBarImage(FSlateRoundedBoxBrush(FLinearColor(0.28f, 0.30f, 0.34f), 3.0f))
		.SetDisabledBarImage(FSlateRoundedBoxBrush(FLinearColor(0.12f, 0.12f, 0.12f), 3.0f))
		// o mesmo tamanho nos três estados: o Slate converte posição em valor
		// descontando a largura da bolinha (SSlider::PositionToValue)
		.SetNormalThumbImage(FSlateRoundedBoxBrush(Accent, 10.0f, FVector2f(ThumbSize, ThumbSize)))
		.SetHoveredThumbImage(FSlateRoundedBoxBrush(FLinearColor(1.0f, 0.72f, 0.3f), 10.0f, FVector2f(ThumbSize, ThumbSize)))
		.SetDisabledThumbImage(FSlateRoundedBoxBrush(FLinearColor(0.3f, 0.3f, 0.3f), 10.0f, FVector2f(ThumbSize, ThumbSize)))
		.SetBarThickness(6.0f);
	MinimapBrush.DrawAs = ESlateBrushDrawType::Image;

	SetVisibility(EVisibility::SelfHitTestInvisible);
	ChildSlot
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Top)
	.Padding(14.0f)
	[
		SAssignNew(Body, SBorder)
		.BorderImage(&Background)
		.Padding(FMargin(16.0f, 14.0f))
		[
			SNew(SBox)
			.WidthOverride(PanelWidth - 32.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("Title", "WORLDFORGE")).Font(Font(18, TEXT("Bold"))).ColorAndOpacity(FSlateColor(FLinearColor::White))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
				[
					SNew(STextBlock).Text(LOCTEXT("Subtitle", "gerador de mundos · RTS")).Font(Font(9)).ColorAndOpacity(FSlateColor(TextDim))
				]
				// ---- semente ----
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(LabelWidth)
						[
							SNew(STextBlock).Text(LOCTEXT("Seed", "Semente")).Font(Font(10))
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 6, 0)
					[
						SAssignNew(SeedBox, SEditableTextBox)
						.Text_Lambda([this]() { return FText::AsNumber(Edit.Seed, &FNumberFormattingOptions::DefaultNoGrouping()); })
						.Font(Font(10))
						.OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type)
						{
							const int32 Seed = FCString::Atoi(*T.ToString());
							if (Seed != Edit.Seed)
							{
								Edit.Seed = Seed;
								Regenerate();
							}
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(DiceButton, SButton)
						.ButtonColorAndOpacity(Accent)
						.ToolTipText(LOCTEXT("DiceTip", "Mapa novo com outra semente"))
						.OnClicked_Lambda([this]()
						{
							// semente nova: hash do relógio (a semente em si é o que reproduz o mapa)
							Edit.Seed = int32(FCrc::MemCrc32(&Edit, sizeof(Edit), uint32(FPlatformTime::Cycles())) % 1000000);
							Regenerate();
							return FReply::Handled();
						})
						[
							SNew(STextBlock).Text(LOCTEXT("Dice", "Sortear")).Font(Font(10, TEXT("Bold"))).ColorAndOpacity(FSlateColor(FLinearColor::Black))
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 2)
				[
					Heading(LOCTEXT("MapHeading", "MAPA"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
				[
					Choice(TEXT("Size"), LOCTEXT("Size", "Tamanho"),
						{ { LOCTEXT("Small", "Pequeno"), 768 }, { LOCTEXT("Medium", "Médio"), 1024 }, { LOCTEXT("Large", "Grande"), 1536 } },
						[this]() { return FMath::RoundToInt(Edit.MapSize); },
						[this](int32 V) { Edit.MapSize = float(V); Regenerate(); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
				[
					Choice(TEXT("Players"), LOCTEXT("Players", "Jogadores"),
						{ { FText::FromString(TEXT("2")), 2 }, { FText::FromString(TEXT("4")), 4 } },
						[this]() { return Edit.Players; },
						[this](int32 V) { Edit.Players = V; Regenerate(); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
				[
					Choice(TEXT("Biome"), LOCTEXT("Biome", "Bioma"),
						{ { LOCTEXT("Temperate", "Temperado"), 0 }, { LOCTEXT("Desert", "Deserto"), 1 }, { LOCTEXT("Winter", "Inverno"), 2 } },
						[this]() { return int32(Edit.Biome); },
						[this](int32 V) { Edit.Biome = EWorldBiome(V); Regenerate(); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 2)
				[
					Heading(LOCTEXT("TerrainHeading", "TERRENO"))
				]
				+ SVerticalBox::Slot().AutoHeight()[SliderRow(TEXT("Relief"), LOCTEXT("Relief", "Montanhas"), &FWorldGenSettings::Relief)]
				+ SVerticalBox::Slot().AutoHeight()[SliderRow(TEXT("Plateaus"), LOCTEXT("Plateaus", "Platôs"), &FWorldGenSettings::Plateaus)]
				+ SVerticalBox::Slot().AutoHeight()[SliderRow(TEXT("Water"), LOCTEXT("Water", "Água"), &FWorldGenSettings::Water)]
				+ SVerticalBox::Slot().AutoHeight()[SliderRow(TEXT("Erosion"), LOCTEXT("Erosion", "Erosão"), &FWorldGenSettings::Erosion)]
				+ SVerticalBox::Slot().AutoHeight()[SliderRow(TEXT("Forests"), LOCTEXT("Forests", "Florestas"), &FWorldGenSettings::Forests)]
				+ SVerticalBox::Slot().AutoHeight()[SliderRow(TEXT("Resources"), LOCTEXT("Resources", "Recursos"), &FWorldGenSettings::Resources)]
				// ---- minimapa ----
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 4)
				[
					SAssignNew(MinimapBorder, SBorder)
					.BorderImage(&MinimapFrame)
					.Padding(2.0f)
					.OnMouseButtonDown(this, &SForgePanel::OnMinimapClick)
					.ToolTipText(LOCTEXT("MinimapTip", "Clique pra levar a câmera"))
					[
						SNew(SImage).Image(&MinimapBrush)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
				[
					SAssignNew(Stats, STextBlock).Font(Font(9)).ColorAndOpacity(FSlateColor(TextDim)).AutoWrapText(true)
					.Text(LOCTEXT("Generating", "gerando..."))
				]
				// ---- qualidade gráfica: os 4 presets (Ultra = épico da Unreal; o cine é
				// modo de filme, ~2,5× mais caro pela mesma imagem de cima) ----
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 2)
				[
					Choice(TEXT("Quality"), LOCTEXT("Quality", "Qualidade"),
						{ { LOCTEXT("QLow", "Baixa"), 0 }, { LOCTEXT("QMed", "Média"), 1 }, { LOCTEXT("QHigh", "Alta"), 2 }, { LOCTEXT("QUltra", "Ultra"), 3 } },
						[]() { return Scalability::GetQualityLevels().GetSingleQualityLevel(); },
						[this](int32 V)
						{
							if (GameMode.IsValid() && GEngine)
							{
								GEngine->Exec(GameMode->GetWorld(), *FString::Printf(TEXT("Scalability %d"), V));
							}
						})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 2)
				[
					Choice(TEXT("Overlay"), LOCTEXT("Overlay", "Mostrar"),
						{ { LOCTEXT("OverlayNone", "Mapa"), 0 }, { LOCTEXT("OverlayPass", "Passagem"), 1 }, { LOCTEXT("OverlayTerritory", "Áreas"), 2 } },
						[this]() { return OverlayMode; },
						[this](int32 V)
						{
							OverlayMode = V;
							if (GameMode.IsValid() && GameMode->GetTerrain())
							{
								GameMode->GetTerrain()->SetOverlay(V);
							}
						})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.ButtonColorAndOpacity(ButtonIdle)
						.OnClicked_Lambda([this]()
						{
							const FString Folder = GameMode.IsValid() ? GameMode->SaveCurrentMap() : FString();
							SaveStatus->SetText(Folder.IsEmpty() ? LOCTEXT("SaveFail", "não salvou")
								: FText::Format(LOCTEXT("Saved", "salvo em {0}"), FText::FromString(Folder)));
							return FReply::Handled();
						})
						[
							SNew(STextBlock).Text(LOCTEXT("Save", "Salvar mapa")).Font(Font(10))
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(8, 0, 0, 0)
					[
						SAssignNew(SaveStatus, STextBlock).Font(Font(8)).ColorAndOpacity(FSlateColor(TextDim)).AutoWrapText(true)
					]
				]
				// ---- mapas guardados: um clique carrega os controles (e a prévia, se já existe) ----
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 2)
				[
					SNew(STextBlock).Text(LOCTEXT("SavedTitle", "Mapas guardados")).Font(Font(10, TEXT("Bold")))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(SavedList, SVerticalBox)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 2)
				[
					SNew(SButton)
					.ButtonColorAndOpacity(ButtonIdle)
					.OnClicked_Lambda([this]()
					{
						if (GameMode.IsValid())
						{
							GameMode->AddSavedMap();
							RefreshSavedMaps();
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Text(LOCTEXT("AddSaved", "Guardar este mapa na lista")).Font(Font(9))
					]
				]
				// ---- o mapa inteiro no acabamento das fatias (outro processo; abre sozinho) ----
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.ButtonColorAndOpacity(Accent)
						.ToolTipText(LOCTEXT("RealTip", "Monta este mapa inteiro com terreno, rochas, árvores e luz de verdade e abre quando terminar (alguns minutos)"))
						.IsEnabled_Lambda([this]()
						{
							double T;
							return !GameMode.IsValid() || GameMode->GetRealQualityState(T) != 1;
						})
						.OnClicked_Lambda([this]()
						{
							if (GameMode.IsValid())
							{
								GameMode->StartRealQuality();
							}
							return FReply::Handled();
						})
						[
							SNew(STextBlock).Text(LOCTEXT("Real", "Ver em qualidade real")).Font(Font(10))
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(8, 0, 0, 0)
					[
						SNew(STextBlock).Font(Font(8)).ColorAndOpacity(FSlateColor(TextDim)).AutoWrapText(true)
						.Text_Lambda([this]()
						{
							double T = 0.0;
							const int32 State = GameMode.IsValid() ? GameMode->GetRealQualityState(T) : 0;
							const FText Clock = FText::FromString(FString::Printf(TEXT("%d:%02d"), int32(T) / 60, int32(T) % 60));
							if (State == 0 && GameMode.IsValid() && AWorldForgeGameMode::HasPreview(GameMode->GetSettings()))
							{
								return LOCTEXT("RealCached", "prévia pronta: abre na hora");
							}
							switch (State)
							{
							case 1: return FText::Format(LOCTEXT("RealBuilding", "montando o mapa inteiro... {0} (pode continuar mexendo)"), Clock);
							case 2: return FText::Format(LOCTEXT("RealDone", "pronto em {0}, abrindo"), Clock);
							case -1: return LOCTEXT("RealFail", "não montou (log em Saved/WorldForge/world_build.log)");
							default: return FText::GetEmpty();
							}
						})
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
				[
					SNew(STextBlock).Font(Font(8)).ColorAndOpacity(FSlateColor(TextDim)).AutoWrapText(true)
					.Text(LOCTEXT("Help", "WASD / borda da tela: andar · botão direito arrastado: rolar o mapa (como no C&C) · botão do meio, Q/E: girar e inclinar · 2 cliques na roda: volta o ângulo · roda: zoom · Home: mapa todo · F1: esconder o painel · F2: teste de qualidade (câmera perto do chão + mapa com o que a câmera vê) · F3: grupos de triângulos (o detalhe que a Unreal usa)"))
				]
			]
		]
	];
	RefreshSavedMaps();
	TestTargets.Add(TEXT("Dice"), DiceButton);
	TestTargets.Add(TEXT("Minimap"), MinimapBorder);
	OnMapShown();
}

SForgePanel::~SForgePanel()
{
	if (GameMode.IsValid())
	{
		GameMode->OnMapShown.Remove(MapShownHandle);
	}
}

bool SForgePanel::IsCursorOver() const
{
	return Body.IsValid() && Body->IsHovered();
}

TSharedRef<SWidget> SForgePanel::Choice(const FString& Id, const FText& Label, TArray<TPair<FText, int32>> Options, TFunction<int32()> Get, TFunction<void(int32)> Set)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
	[
		SNew(SBox).WidthOverride(LabelWidth)
		[
			SNew(STextBlock).Text(Label).Font(Font(10))
		]
	];
	for (const TPair<FText, int32>& O : Options)
	{
		const int32 Value = O.Value;
		TSharedPtr<SButton> Button;
		Row->AddSlot().FillWidth(1.0f).Padding(1.0f, 0.0f)
		[
			SAssignNew(Button, SButton)
			.HAlign(HAlign_Center)
			.ContentPadding(FMargin(2.0f, 3.0f))   // o padrão cortava "Pequeno", "Temperado"
			.ButtonColorAndOpacity_Lambda([Get, Value]() { return Get() == Value ? Accent : ButtonIdle; })
			.OnClicked_Lambda([Get, Set, Value]()
			{
				if (Get() != Value)
				{
					Set(Value);
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(O.Key).Font(Font(9))
				.ColorAndOpacity_Lambda([Get, Value]() { return FSlateColor(Get() == Value ? FLinearColor::Black : FLinearColor::White); })
			]
		];
		TestTargets.Add(FString::Printf(TEXT("%s_%d"), *Id, Value), Button);
	}
	return Row;
}

TSharedRef<SWidget> SForgePanel::SliderRow(const FString& Id, const FText& Label, float FWorldGenSettings::* Field)
{
	TSharedPtr<SSlider> Slider;
	TSharedRef<SWidget> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(LabelWidth)
			[
				SNew(STextBlock).Text(Label).Font(Font(10))
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			// faixa de 28 px de altura toda clicável: clicar em qualquer ponto
			// pula pra lá e já arrasta (o padrão tinha ~8 px e errava o clique)
			SNew(SBox)
			.HeightOverride(28.0f)
			[
				SAssignNew(Slider, SSlider)
				.Style(&SliderStyle)
				.Value_Lambda([this, Field]() { return Edit.*Field; })
				.OnValueChanged_Lambda([this, Field](float V) { Edit.*Field = V; })
				.OnMouseCaptureEnd_Lambda([this]() { Regenerate(); })
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 0, 0)
		[
			SNew(SBox).WidthOverride(34.0f).HAlign(HAlign_Right)
			[
				SNew(STextBlock).Font(Font(9)).ColorAndOpacity(FSlateColor(TextDim))
				.Text_Lambda([this, Field]() { return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(100.0f * (Edit.*Field)))); })
			]
		];
	TestTargets.Add(TEXT("Slider_") + Id, Slider);
	return Row;
}

void SForgePanel::RefreshSavedMaps()
{
	if (!SavedList.IsValid() || !GameMode.IsValid())
	{
		return;
	}
	SavedList->ClearChildren();
	const TArray<AWorldForgeGameMode::FSavedMap>& Maps = GameMode->GetSavedMaps();
	for (int32 I = 0; I < Maps.Num(); ++I)
	{
		const FWorldGenSettings S = Maps[I].Settings;
		// exemplo: o mundo montado à mão (pedaço de 250 m); os outros: a prévia guardada do mapa inteiro
		const bool bExample = AWorldForgeGameMode::PreviewMapOf(S).StartsWith(TEXT("/Game/Maps/Slice_"));
		const FString Label = Maps[I].Name + (bExample ? TEXT("   · mundo pronto") : AWorldForgeGameMode::HasPreview(S) ? TEXT("   · prévia pronta") : TEXT(""));
		SavedList->AddSlot().AutoHeight().Padding(0, 1)
		[
			SNew(SButton)
			.ButtonColorAndOpacity(FLinearColor(0.10f, 0.11f, 0.12f))
			.ContentPadding(FMargin(6, 3))
			.OnClicked_Lambda([this, S]()
			{
				Edit = S;
				Regenerate();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(FText::FromString(Label)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			]
		];
	}
}

void SForgePanel::Regenerate()
{
	if (GameMode.IsValid())
	{
		GameMode->Generate(Edit);
		Stats->SetText(LOCTEXT("Generating", "gerando..."));
	}
}

void SForgePanel::OnMapShown()
{
	if (!GameMode.IsValid() || !GameMode->GetTerrain() || !GameMode->GetTerrain()->GetMap())
	{
		return;
	}
	AWorldForgeTerrain* Terrain = GameMode->GetTerrain();
	const FWorldMap& M = *Terrain->GetMap();
	MinimapBrush.SetResourceObject(Terrain->GetMinimap());
	double Total = 0.0;
	for (const TPair<FString, double>& T : M.Timings) Total += T.Value;
	int32 Trees = 0;
	for (const FWorldProp& P : M.Props) Trees += P.Type == EWorldPropType::Tree ? 1 : 0;
	int32 Water = 0;
	for (EWorldPass P : M.Pass) Water += P == EWorldPass::Water ? 1 : 0;
	Stats->SetText(FText::FromString(FString::Printf(
		TEXT("Semente %d · %d jogadores · %.0f m\nGerado em %.0f ms · %s\nRush: %.0f m pelo caminho (%.1f× a linha reta), igual pra todos\n%.0f%% transitável · %.0f%% água · %d rampas\n%d campos de recurso · %d árvores"),
		M.Settings.Seed, M.Settings.Players, M.Settings.MapSize, Total,
		M.bConnected ? TEXT("bases ligadas por terra") : TEXT("ATENÇÃO: bases sem caminho"),
		M.RushMeters, M.RushMeters / FMath::Max(M.RushStraight, 1.0f),
		100.0f * M.OpenFraction, 100.0f * Water / FMath::Max(1, M.Pass.Num()), M.RampsCarved,
		M.Resources.Num(), Trees)));
}

FReply SForgePanel::OnMinimapClick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!GameMode.IsValid() || !GameMode->GetTerrain() || !GameMode->GetTerrain()->GetMap())
	{
		return FReply::Unhandled();
	}
	// a mesma orientação do minimapa: +X do mundo pra cima, +Y pra direita
	const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) / Geometry.GetLocalSize();
	const float Size = GameMode->GetTerrain()->GetMap()->Half() * 2.0f;
	const FVector2f P((0.5f - Local.Y) * Size, (Local.X - 0.5f) * Size);
	if (APlayerController* PC = GameMode->GetWorld()->GetFirstPlayerController())
	{
		if (AForgeCameraPawn* Pawn = PC->GetPawn<AForgeCameraPawn>())
		{
			Pawn->JumpTo(P);
		}
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
