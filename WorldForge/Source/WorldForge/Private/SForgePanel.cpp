#include "SForgePanel.h"

#include "Widgets/Layout/SWidgetSwitcher.h"

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
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "WorldForgeGameMode.h"
#include "WorldForgeTerrain.h"

#define LOCTEXT_NAMESPACE "WorldForge"

namespace
{
	const FLinearColor Accent(0.95f, 0.55f, 0.10f);          // laranja de C&C
	const FLinearColor ButtonIdle(0.10f, 0.11f, 0.12f);
	const FLinearColor TextDim(0.62f, 0.66f, 0.70f);
	constexpr float PanelWidth = 350.0f;
	constexpr float LabelWidth = 86.0f;

	FSlateFontInfo Font(int32 Size, const TCHAR* Style = TEXT("Regular"))
	{
		return FCoreStyle::GetDefaultFontStyle(Style, Size);
	}

	/** Título de seção: o nome em laranja e um fio embaixo. */
	TSharedRef<SWidget> Section(const FText& Text)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(Text).Font(Font(9, TEXT("Bold"))).ColorAndOpacity(FSlateColor(Accent))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 4)
			[
				SNew(SBox).HeightOverride(1.0f)
				[
					SNew(SImage).ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.12f))
				]
			];
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
	MinimapBrush.ImageSize = FVector2D(MinimapSize, MinimapSize);
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

	// Arranjo de ferramenta (o mesmo das ferramentas de mundo e do editor da
	// Unreal): à esquerda o painel de propriedades, da altura da janela, com o
	// cabeçalho fixo (a ação principal sempre à vista) e o resto rolando; no alto
	// à direita a barra da vista (o que mostrar, qualidade), como a barra da
	// viewport do editor; embaixo à direita o minimapa com os números do mapa,
	// onde o RTS põe o minimapa. Antes era uma coluna só, mais alta que a janela:
	// a lista de mapas e o "qualidade real" ficavam fora da tela (07/10)
	SetVisibility(EVisibility::SelfHitTestInvisible);
	ChildSlot
	[
		SNew(SOverlay)
		// ---- painel de propriedades ----
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill).Padding(12.0f)
		[
			SAssignNew(Body, SBorder)
			.BorderImage(&Background)
			.Padding(FMargin(14.0f, 12.0f, 6.0f, 12.0f))
			[
				SNew(SBox)
				.WidthOverride(PanelWidth - 20.0f)
				[
					SNew(SVerticalBox)
					// cabeçalho fixo
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 8, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
						[
							SNew(STextBlock).Text(LOCTEXT("Title", "WORLDFORGE")).Font(Font(17, TEXT("Bold"))).ColorAndOpacity(FSlateColor(FLinearColor::White))
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Bottom).HAlign(HAlign_Right).Padding(0, 0, 0, 3)
						[
							SNew(STextBlock).Text(LOCTEXT("Subtitle", "criador de mundos · RTS")).Font(Font(9)).ColorAndOpacity(FSlateColor(TextDim))
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 8, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[
							SNew(STextBlock).Text(LOCTEXT("Seed", "Semente")).Font(Font(10))
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SAssignNew(SeedBox, SEditableTextBox)
							.ToolTipText(LOCTEXT("SeedTip", "O número que reproduz este mapa. Digite um e aperte Enter pra gerar de novo"))
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
					]
					// a ação principal
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 8, 0)
					[
						SAssignNew(DiceButton, SButton)
						.ButtonColorAndOpacity(Accent)
						.HAlign(HAlign_Center)
						.ContentPadding(FMargin(0.0f, 7.0f))
						.ToolTipText(LOCTEXT("DiceTip", "Mundo novo: sorteia outra semente e mantém os controles abaixo"))
						.OnClicked_Lambda([this]()
						{
							// semente nova: hash do relógio (a semente em si é o que reproduz o mapa)
							Edit.Seed = int32(FCrc::MemCrc32(&Edit, sizeof(Edit), uint32(FPlatformTime::Cycles())) % 1000000);
							Regenerate();
							return FReply::Handled();
						})
						[
							SNew(STextBlock).Text(LOCTEXT("Dice", "GERAR MUNDO NOVO")).Font(Font(11, TEXT("Bold"))).ColorAndOpacity(FSlateColor(FLinearColor::Black))
						]
					]
					// o mapa inteiro no acabamento das fatias (outro processo; abre sozinho)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 8, 0)
					[
						SNew(SButton)
						.ButtonColorAndOpacity(FLinearColor(0.22f, 0.24f, 0.27f))
						.HAlign(HAlign_Center)
						.ContentPadding(FMargin(0.0f, 5.0f))
						.ToolTipText(LOCTEXT("RealTip", "Monta este mapa inteiro com terreno, água, rochas, árvores e luz de verdade e abre quando terminar (alguns minutos)"))
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
							SNew(STextBlock).Text(LOCTEXT("Real", "Ver em qualidade real")).Font(Font(10, TEXT("Bold")))
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 3, 8, 0)
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
							default: return LOCTEXT("RealIdle", "mar, gelo, leitos, salina, estradas e árvores de verdade (alguns minutos)");
							}
						})
					]
					// ---- as abas (o jeito dos editores de mapa: o WorldBuilder do C&C mostra só as opções da
					// ferramenta escolhida; o modo de terreno da Unreal, por abas). O Felipe, 12/10: o painel
					// "sujo e cheio" e a lista de mapas sempre aberta ----
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 8, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 2, 0)[TabButton(0, LOCTEXT("TabWorld", "Mundo"), LOCTEXT("TabWorldTip", "O tipo de mapa e o terreno do gerador"))]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2, 0, 2, 0)[TabButton(1, LOCTEXT("TabEdit", "Editar"), LOCTEXT("TabEditTip", "Peça por peça, por cima do gerador: mover, apagar, pintar, altura, estrada"))]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2, 0, 0, 0)[TabButton(2, LOCTEXT("TabMaps", "Mapas"), LOCTEXT("TabMapsTip", "Os mapas guardados e os trazidos do Zero Hour; guardar e exportar"))]
					]
					// ---- o resto rola ----
					+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0, 8, 0, 0)
					[
						SAssignNew(Scroll, SScrollBox)
						.ScrollBarThickness(FVector2D(6.0f, 6.0f))
						+ SScrollBox::Slot().Padding(0, 0, 10, 0)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
							SNew(SWidgetSwitcher)
							.WidgetIndex_Lambda([this]() { return PanelTab; })
							// ---- aba Mundo ----
							+ SWidgetSwitcher::Slot()
							[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("MapHeading", "TIPO DE MAPA"))]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
							[
								Choice(TEXT("Biome"), LOCTEXT("Biome", "Bioma"),
									{ { LOCTEXT("Temperate", "Temperado"), 0 }, { LOCTEXT("Desert", "Deserto"), 1 }, { LOCTEXT("Winter", "Inverno"), 2 } },
									[this]() { return int32(Edit.Biome); },
									[this](int32 V) { Edit.Biome = EWorldBiome(V); Regenerate(); })
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
							[
								// o tipo de borda: sorteado pela semente ou escolhido (mapa sem mar, com
								// mar em dois lados, ilha...); não é regra pra todo mapa
								Choice(TEXT("Frame"), LOCTEXT("Frame", "Borda (o que cerca o mapa)"),
									{ { LOCTEXT("FrameAuto", "Sorteio"), -1 }, { LOCTEXT("FrameRange", "Serra"), 0 }, { LOCTEXT("FrameOpen", "Aberta"), 1 },
									  { LOCTEXT("FrameCoast", "Costa"), 2 }, { LOCTEXT("FrameIsland", "Ilha"), 3 } },
									[this]() { return Edit.Frame; },
									[this](int32 V) { Edit.Frame = V; Regenerate(); })
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
								Choice(TEXT("Size"), LOCTEXT("Size", "Tamanho"),
									{ { LOCTEXT("Small", "Pequeno"), 768 }, { LOCTEXT("Medium", "Médio"), 1024 }, { LOCTEXT("Large", "Grande"), 1536 } },
									[this]() { return FMath::RoundToInt(Edit.MapSize); },
									[this](int32 V) { Edit.MapSize = float(V); Regenerate(); })
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)[Section(LOCTEXT("TerrainHeading", "TERRENO"))]
							+ SVerticalBox::Slot().AutoHeight()[SliderRow(TEXT("Relief"), LOCTEXT("Relief", "Montanhas"), &FWorldGenSettings::Relief)]
							+ SVerticalBox::Slot().AutoHeight()[SliderRow(TEXT("Plateaus"), LOCTEXT("Plateaus", "Platôs"), &FWorldGenSettings::Plateaus)]
							+ SVerticalBox::Slot().AutoHeight()[SliderRow(TEXT("Water"), LOCTEXT("Water", "Água"), &FWorldGenSettings::Water)]
							+ SVerticalBox::Slot().AutoHeight()[SliderRow(TEXT("Erosion"), LOCTEXT("Erosion", "Erosão"), &FWorldGenSettings::Erosion)]
							// (16/10) morros e descidas no chão aberto, como nos mapas do ZH
							+ SVerticalBox::Slot().AutoHeight()[SliderRow(TEXT("Hills"), LOCTEXT("Hills", "Morros e descidas"), &FWorldGenSettings::Hills)]
							+ SVerticalBox::Slot().AutoHeight()[SliderRow(TEXT("Forests"), LOCTEXT("Forests", "Florestas"), &FWorldGenSettings::Forests)]
							+ SVerticalBox::Slot().AutoHeight()[SliderRow(TEXT("Resources"), LOCTEXT("Resources", "Recursos"), &FWorldGenSettings::Resources)]
							// o tipo de relevo (fase 2): morros e platôs, ou monólitos (o AlUla)
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
							[
								Choice(TEXT("ReliefType"), LOCTEXT("ReliefType", "Relevo"),
									{ { LOCTEXT("ReliefHills", "Morros e platôs"), 0 }, { LOCTEXT("ReliefMonoliths", "Monólitos"), 1 } },
									[this]() { return Edit.ReliefType; },
									[this](int32 V) { Edit.ReliefType = V; Regenerate(); })
							]
							// a areia do deserto (16/10, o Felipe: as três como opção pra construir e variar; a de duna é o
							// padrão). Só o acabamento: não gera o mapa de novo, vale no "Ver em qualidade real"
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
							[
								SNew(SBox)
								.Visibility_Lambda([this]() { return Edit.Biome == EWorldBiome::Desert ? EVisibility::Visible : EVisibility::Collapsed; })
								[
									Choice(TEXT("Sand"), LOCTEXT("Sand", "Areia (no acabamento)"),
										{ { LOCTEXT("SandDune", "Duna"), 3 }, { LOCTEXT("SandYellow", "Amarela"), 2 }, { LOCTEXT("SandClassic", "Clássica"), 1 } },
										[this]() { return Edit.Sand; },
										[this](int32 V) { Edit.Sand = V; if (GameMode.IsValid()) GameMode->SetSand(V); })
								]
							]
							// a vila do kit civil (docs §12): uma no centro é a escolha do Felipe
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
							[
								Choice(TEXT("Village"), LOCTEXT("Village", "Vila"),
									{ { LOCTEXT("VillageNone", "Sem"), 0 }, { LOCTEXT("VillageCenter", "No centro"), 1 }, { LOCTEXT("VillageSides", "Uma por lado"), 2 } },
									[this]() { return Edit.Villages; },
									[this](int32 V) { Edit.Villages = V; Regenerate(); })
							]
							]
							// ---- aba Editar: a edição à mão (peça por peça, por cima do gerador; docs §11) ----
							+ SWidgetSwitcher::Slot()
							[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("EditTitle", "EDIÇÃO"))]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
							[
								Choice(TEXT("Tool"), LOCTEXT("Tool", "Ferramenta (clique no mapa)"),
									{ { LOCTEXT("ToolLook", "Olhar"), 0 }, { LOCTEXT("ToolMove", "Mover"), 5 }, { LOCTEXT("ToolDelete", "Apagar"), 1 },
									  { LOCTEXT("ToolPaint", "Pintar"), 2 }, { LOCTEXT("ToolHeight", "Altura"), 3 }, { LOCTEXT("ToolRoad", "Estrada"), 4 } },
									[this]() { return GameMode.IsValid() ? GameMode->EditTool : 0; },
									[this](int32 V) { if (GameMode.IsValid()) GameMode->EditTool = V; })
							]
							// o pincel de chão: a camada (os nomes mudam com o bioma) e o tamanho
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
							[
								SNew(SBox)
								.Visibility_Lambda([this]() { return GameMode.IsValid() && (GameMode->EditTool == 2 || GameMode->EditTool == 3) ? EVisibility::Visible : EVisibility::Collapsed; })
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SBox)
										.Visibility_Lambda([this]() { return GameMode.IsValid() && GameMode->EditTool == 2 ? EVisibility::Visible : EVisibility::Collapsed; })
										[
											BrushLayerRow()
										]
									]
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SBox)
										.Visibility_Lambda([this]() { return GameMode.IsValid() && GameMode->EditTool == 3 ? EVisibility::Visible : EVisibility::Collapsed; })
										[
											Choice(TEXT("HeightMode"), LOCTEXT("HeightMode", "Altura (segure no mapa)"),
												{ { LOCTEXT("HeightHill", "Morro"), 3 }, { LOCTEXT("HeightVale", "Vale"), 4 }, { LOCTEXT("HeightSmooth", "Suavizar"), 5 },
												  { LOCTEXT("HeightNatural", "Naturalizar"), 6 }, { LOCTEXT("HeightUp", "Subir"), 0 }, { LOCTEXT("HeightDown", "Baixar"), 1 },
												  { LOCTEXT("HeightFlat", "Aplainar"), 2 } },
												[this]() { return GameMode.IsValid() ? GameMode->HeightMode : 0; },
												[this](int32 V) { if (GameMode.IsValid()) GameMode->HeightMode = V; })
										]
									]
									// (16/10) o pincel de altura repete nos outros lados (a simetria do mapa)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SBox)
										.Visibility_Lambda([this]() { return GameMode.IsValid() && GameMode->EditTool == 3 ? EVisibility::Visible : EVisibility::Collapsed; })
										[
											Choice(TEXT("HeightMirror"), LOCTEXT("HeightMirror", "Espelhar nos outros lados"),
												{ { LOCTEXT("MirrorYes", "Sim"), 1 }, { LOCTEXT("MirrorNo", "Não"), 0 } },
												[this]() { return GameMode.IsValid() ? GameMode->HeightMirror : 1; },
												[this](int32 V) { if (GameMode.IsValid()) GameMode->HeightMirror = V; })
										]
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
									[
										Choice(TEXT("BrushSize"), LOCTEXT("BrushSize", "Tamanho do pincel"),
											{ { LOCTEXT("BrushS", "3 m"), 3 }, { LOCTEXT("BrushM", "6 m"), 6 }, { LOCTEXT("BrushL", "12 m"), 12 }, { LOCTEXT("BrushXL", "24 m"), 24 },
											  { LOCTEXT("BrushXXL", "50 m"), 50 }, { LOCTEXT("BrushXXXL", "100 m"), 100 } },
											[this]() { return GameMode.IsValid() ? FMath::RoundToInt(GameMode->BrushRadius) : 6; },
											[this](int32 V) { if (GameMode.IsValid()) GameMode->BrushRadius = float(V); })
									]
								]
							]
							// mover e apagar: o que fazer
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
							[
								SNew(STextBlock).Font(Font(8)).ColorAndOpacity(FSlateColor(TextDim)).AutoWrapText(true)
								.Visibility_Lambda([this]() { return GameMode.IsValid() && (GameMode->EditTool == 5 || GameMode->EditTool == 1) ? EVisibility::Visible : EVisibility::Collapsed; })
								.Text_Lambda([this]()
								{
									return GameMode.IsValid() && GameMode->EditTool == 5
										? LOCTEXT("MoveHint", "arraste uma pedra, árvore ou moita pra outro lugar · R gira 15° (Shift+R pro outro lado)")
										: LOCTEXT("DeleteHint", "clique na peça pra tirar (a do círculo branco)");
								})
							]
							// a estrada: o tipo, o que fazer agora e os botões da escolhida
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
							[
								SNew(SBox)
								.Visibility_Lambda([this]() { return GameMode.IsValid() && GameMode->EditTool == 4 ? EVisibility::Visible : EVisibility::Collapsed; })
								[
									RoadRows()
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
								[
									SNew(STextBlock).Font(Font(8)).ColorAndOpacity(FSlateColor(TextDim)).AutoWrapText(true)
									.Text_Lambda([this]()
									{
										const int32 N = GameMode.IsValid() ? GameMode->EditCount() : 0;
										return N == 0 ? LOCTEXT("EditNone", "nenhuma edição (vale pro qualidade real; semente nova zera)")
											: FText::Format(LOCTEXT("EditCount", "{0} edições · Ctrl+Z desfaz"), FText::AsNumber(N));
									})
								]
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(SButton)
									.ButtonColorAndOpacity(ButtonIdle)
									.IsEnabled_Lambda([this]() { return GameMode.IsValid() && GameMode->EditCount() > 0; })
									.OnClicked_Lambda([this]() { if (GameMode.IsValid()) GameMode->EditUndo(); return FReply::Handled(); })
									[
										SNew(STextBlock).Text(LOCTEXT("Undo", "Desfazer")).Font(Font(9))
									]
								]
							]
							]
							// ---- aba Mapas: os guardados (um clique carrega os controles e a prévia, se já existe) ----
							+ SWidgetSwitcher::Slot()
							[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SavedTitle", "MAPAS GUARDADOS"))]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SAssignNew(SavedList, SVerticalBox)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 2, 0)
								[
									SNew(SButton)
									.ButtonColorAndOpacity(ButtonIdle)
									.HAlign(HAlign_Center)
									.ToolTipText(LOCTEXT("AddSavedTip", "Põe este mapa (semente e controles) na lista acima"))
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
										SNew(STextBlock).Text(LOCTEXT("AddSaved", "Guardar na lista")).Font(Font(9))
									]
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2, 0, 0, 0)
								[
									SNew(SButton)
									.ButtonColorAndOpacity(ButtonIdle)
									.HAlign(HAlign_Center)
									.ToolTipText(LOCTEXT("SaveTip", "Grava os arquivos do mapa (relevo, camadas, minimapa) pro jogo"))
									.OnClicked_Lambda([this]()
									{
										const FString Folder = GameMode.IsValid() ? GameMode->SaveCurrentMap() : FString();
										SaveStatus->SetText(Folder.IsEmpty() ? LOCTEXT("SaveFail", "não salvou")
											: FText::Format(LOCTEXT("Saved", "salvo em {0}"), FText::FromString(Folder)));
										return FReply::Handled();
									})
									[
										SNew(STextBlock).Text(LOCTEXT("Save", "Exportar arquivos")).Font(Font(9))
									]
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
							[
								SAssignNew(SaveStatus, STextBlock).Font(Font(8)).ColorAndOpacity(FSlateColor(TextDim)).AutoWrapText(true)
							]
							]
							]
							// ---- atalhos (fechados: abre num clique; em todas as abas) ----
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
							[
								SNew(SButton)
								.ButtonStyle(FCoreStyle::Get(), "NoBorder")
								.OnClicked_Lambda([this]() { bShowHelp = !bShowHelp; return FReply::Handled(); })
								[
									SNew(STextBlock).Font(Font(9, TEXT("Bold"))).ColorAndOpacity(FSlateColor(Accent))
									.Text_Lambda([this]() { return bShowHelp ? LOCTEXT("HelpOpen", "ATALHOS  ▾") : LOCTEXT("HelpClosed", "ATALHOS  ▸"); })
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 6)
							[
								SNew(STextBlock).Font(Font(8)).ColorAndOpacity(FSlateColor(TextDim)).AutoWrapText(true)
								.Visibility_Lambda([this]() { return bShowHelp ? EVisibility::Visible : EVisibility::Collapsed; })
								.Text(LOCTEXT("Help", "WASD / setas: andar\nbotão direito arrastado: rolar o mapa (como no C&C)\nbotão do meio, Q/E: girar e inclinar\n2 cliques na roda: volta o ângulo\nroda: zoom · Home: mapa todo\nF1: esconder o painel\nF2: teste de qualidade (câmera perto do chão)\nF3: grupos de triângulos (o detalhe que a Unreal usa)"))
							]
						]
					]
				]
			]
		]
		// ---- barra da vista (alto, à direita) ----
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(12.0f)
		[
			SAssignNew(Toolbar, SBorder)
			.BorderImage(&Background)
			.Padding(FMargin(10.0f, 6.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
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
						}, true)
				]
				// qualidade gráfica: os 4 presets (Ultra = épico da Unreal; o cine é
				// modo de filme, ~2,5× mais caro pela mesma imagem de cima)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16, 0, 0, 0)
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
						}, true)
				]
			]
		]
		// ---- minimapa e números do mapa (embaixo, à direita) ----
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(12.0f)
		[
			SAssignNew(MapBox, SBorder)
			.BorderImage(&Background)
			.Padding(8.0f)
			[
				SNew(SBox)
				.WidthOverride(MinimapSize + 4.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
					[
						SAssignNew(Stats, STextBlock).Font(Font(8)).ColorAndOpacity(FSlateColor(TextDim)).AutoWrapText(true)
						.Text(LOCTEXT("Generating", "gerando..."))
					]
					+ SVerticalBox::Slot().AutoHeight()
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

TSharedRef<SWidget> SForgePanel::TabButton(int32 Tab, const FText& Label, const FText& Tip)
{
	TSharedRef<SButton> Button = SNew(SButton)
		.ButtonColorAndOpacity_Lambda([this, Tab]() { return PanelTab == Tab ? FSlateColor(Accent) : FSlateColor(ButtonIdle); })
		.HAlign(HAlign_Center)
		.ContentPadding(FMargin(0.0f, 4.0f))
		.ToolTipText(Tip)
		.OnClicked_Lambda([this, Tab]() { SetTab(Tab); return FReply::Handled(); })
		[
			SNew(STextBlock).Text(Label).Font(Font(10, TEXT("Bold")))
			.ColorAndOpacity_Lambda([this, Tab]() { return PanelTab == Tab ? FSlateColor(FLinearColor::Black) : FSlateColor(FLinearColor::White); })
		];
	TestTargets.Add(FString::Printf(TEXT("Tab_%d"), Tab), Button);
	return Button;
}

void SForgePanel::SetTab(int32 Tab)
{
	// fora da aba Editar a ferramenta volta pra "Olhar": o clique no mapa não edita escondido
	if (Tab != 1 && PanelTab == 1 && GameMode.IsValid())
	{
		GameMode->EditTool = 0;
	}
	PanelTab = FMath::Clamp(Tab, 0, 2);
	if (Tab == 2)
	{
		RefreshSavedMaps();
	}
	if (Scroll.IsValid())
	{
		Scroll->ScrollToStart();
	}
}

void SForgePanel::ScrollToTarget(const FString& Id)
{
	const TSharedPtr<SWidget>* W = TestTargets.Find(Id);
	if (Scroll.IsValid() && W && W->IsValid())
	{
		Scroll->ScrollDescendantIntoView(*W, false, EDescendantScrollDestination::Center);
	}
}

bool SForgePanel::IsCursorOver() const
{
	return (Body.IsValid() && Body->IsHovered()) || (Toolbar.IsValid() && Toolbar->IsHovered()) || (MapBox.IsValid() && MapBox->IsHovered());
}

TSharedRef<SWidget> SForgePanel::Choice(const FString& Id, const FText& Label, TArray<TPair<FText, int32>> Options, TFunction<int32()> Get, TFunction<void(int32)> Set, bool bCompact)
{
	// no painel: o rótulo em cima e os botões dividindo a largura toda (cabem os
	// 5 da borda; com o rótulo ao lado os textos saíam cortados, 07/10). Na barra
	// da vista, compacto: rótulo à esquerda e botões do tamanho do texto
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (const TPair<FText, int32>& O : Options)
	{
		const int32 Value = O.Value;
		TSharedPtr<SButton> Button;
		SAssignNew(Button, SButton)
		.HAlign(HAlign_Center)
		// (5 opções numa linha: letra 8 e quase sem margem, senão "Sorteio" cortava)
		.ContentPadding(bCompact ? FMargin(8.0f, 3.0f) : (Options.Num() >= 5 ? FMargin(0.0f, 4.0f) : FMargin(2.0f, 4.0f)))
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
			SNew(STextBlock).Text(O.Key).Font(Font(!bCompact && Options.Num() >= 5 ? 8 : 9))
			.ColorAndOpacity_Lambda([Get, Value]() { return FSlateColor(Get() == Value ? FLinearColor::Black : FLinearColor::White); })
		];
		if (bCompact)
		{
			Row->AddSlot().AutoWidth().Padding(1.0f, 0.0f)[Button.ToSharedRef()];
		}
		else
		{
			Row->AddSlot().FillWidth(1.0f).Padding(1.0f, 0.0f)[Button.ToSharedRef()];
		}
		TestTargets.Add(FString::Printf(TEXT("%s_%d"), *Id, Value), Button);
	}
	if (bCompact)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
			[
				SNew(STextBlock).Text(Label).Font(Font(9)).ColorAndOpacity(FSlateColor(TextDim))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Row
			];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
		[
			SNew(STextBlock).Text(Label).Font(Font(9)).ColorAndOpacity(FSlateColor(TextDim))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Row
		];
}

TSharedRef<SWidget> SForgePanel::RoadRows()
{
	auto SmallButton = [this](const FString& Id, const FText& Label, TFunction<bool()> Visible, TFunction<void()> Act) -> TSharedRef<SWidget>
	{
		TSharedPtr<SButton> Button;
		SAssignNew(Button, SButton)
		.ButtonColorAndOpacity(ButtonIdle)
		.HAlign(HAlign_Center)
		.ContentPadding(FMargin(6.0f, 3.0f))
		.Visibility_Lambda([Visible]() { return Visible() ? EVisibility::Visible : EVisibility::Collapsed; })
		.IsEnabled_Lambda([this]() { return GameMode.IsValid() && !GameMode->IsGenerating(); })
		.OnClicked_Lambda([Act]() { Act(); return FReply::Handled(); })
		[
			SNew(STextBlock).Text(Label).Font(Font(9))
		];
		TestTargets.Add(Id, Button);
		return Button.ToSharedRef();
	};
	return SNew(SVerticalBox)
		// o tipo (na floresta só tem terra: a linha some)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return GameMode.IsValid() && GameMode->BiomeHasPavedRoads() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				Choice(TEXT("RoadType"), LOCTEXT("RoadType", "Tipo da estrada"),
					{ { LOCTEXT("RoadDirt", "Terra"), 0 }, { LOCTEXT("RoadPaved", "Asfalto"), 1 } },
					[this]() { return GameMode.IsValid() && GameMode->IsSelectedRoadPaved() ? 1 : 0; },
					[this](int32 V) { if (GameMode.IsValid()) GameMode->RoadSetPaved(V == 1); })
			]
		]
		// o acabamento do asfalto (16/10, o Felipe: a laje aprovada "mas lembre-se que tinha mais de uma variação" e "também
		// não descartei a original que é colada no chão"): vale no "Ver em qualidade real"
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
		[
			SNew(SBox)
			.Visibility_Lambda([this]()
			{
				return GameMode.IsValid() && GameMode->HasRoadSelection() && GameMode->IsSelectedRoadPaved()
					&& GameMode->GetSettings().Biome == EWorldBiome::Desert ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				Choice(TEXT("RoadStyle"), LOCTEXT("RoadStyle", "Acabamento do asfalto"),
					{ { LOCTEXT("RoadStyleAuto", "Auto"), 0 }, { LOCTEXT("RoadStyleFlat", "Colada"), 1 }, { LOCTEXT("RoadStyleWhole", "Inteira"), 2 },
					  { LOCTEXT("RoadStyleWorn", "Gasta"), 3 }, { LOCTEXT("RoadStyleBroken", "Destruída"), 4 } },
					[this]() { return GameMode.IsValid() ? GameMode->GetSelectedRoadStyle() : 0; },
					[this](int32 V) { if (GameMode.IsValid()) GameMode->RoadSetStyle(V); })
			]
		]
		// a sujeira na beira (16/10, o Felipe: "uma vez por outra" e "a gente suja a estrada usando a textura que tá ao lado
		// dela"): o chão do lado entra na pista em trechos; Auto põe mais onde há areia solta em volta. À mão, o pincel de
		// Pintar também suja a estrada
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
		[
			SNew(SBox)
			.Visibility_Lambda([this]()
			{
				return GameMode.IsValid() && GameMode->HasRoadSelection() && GameMode->IsSelectedRoadPaved()
					&& GameMode->GetSettings().Biome == EWorldBiome::Desert ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				Choice(TEXT("RoadDirt"), LOCTEXT("RoadDirt", "Sujeira na beira"),
					{ { LOCTEXT("RoadDirtAuto", "Auto"), 0 }, { LOCTEXT("RoadDirtNone", "Nenhuma"), 1 }, { LOCTEXT("RoadDirtLow", "Pouca"), 2 },
					  { LOCTEXT("RoadDirtMid", "Média"), 3 }, { LOCTEXT("RoadDirtHigh", "Muita"), 4 } },
					[this]() { return GameMode.IsValid() ? GameMode->GetSelectedRoadDirt() : 0; },
					[this](int32 V) { if (GameMode.IsValid()) GameMode->RoadSetDirt(V); })
			]
		]
		// o que fazer agora (muda com o estado)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
		[
			SNew(STextBlock).Font(Font(8)).ColorAndOpacity(FSlateColor(TextDim)).AutoWrapText(true)
			.Text_Lambda([this]()
			{
				if (!GameMode.IsValid()) return FText::GetEmpty();
				if (GameMode->IsGenerating()) return LOCTEXT("RoadBusy", "refazendo o mundo com a estrada…");
				if (GameMode->RoadDraft.Num())
				{
					return FText::Format(LOCTEXT("RoadDrafting", "{0} pontos · clique põe mais · Enter ou duplo clique termina · Backspace tira o último · Esc cancela"),
						FText::AsNumber(GameMode->RoadDraft.Num()));
				}
				if (GameMode->HasRoadSelection())
				{
					return LOCTEXT("RoadSelected", "estrada escolhida (amarela): arraste os pontos brancos pra mudar o caminho · Delete apaga · Esc solta");
				}
				return LOCTEXT("RoadIdle", "clique no chão pra começar uma estrada nova · clique numa estrada pra escolher");
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SmallButton(TEXT("RoadFinish"), LOCTEXT("RoadFinish", "Terminar estrada"),
					[this]() { return GameMode.IsValid() && GameMode->RoadDraft.Num() >= 2; },
					[this]() { if (GameMode.IsValid()) GameMode->RoadFinishDraft(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SmallButton(TEXT("RoadCancel"), LOCTEXT("RoadCancel", "Cancelar"),
					[this]() { return GameMode.IsValid() && GameMode->RoadDraft.Num() > 0; },
					[this]() { if (GameMode.IsValid()) GameMode->RoadDraft.Reset(); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SmallButton(TEXT("RoadDelete"), LOCTEXT("RoadDelete", "Apagar estrada"),
					[this]() { return GameMode.IsValid() && GameMode->HasRoadSelection() && GameMode->RoadDraft.Num() == 0; },
					[this]() { if (GameMode.IsValid()) GameMode->RoadDeleteSelected(); })
			]
		];
}

TSharedRef<SWidget> SForgePanel::BrushLayerRow()
{
	// as camadas do chão do bioma, na ordem do material (build_slice.py, SLICES)
	auto Names = [this](int32 I) -> FText
	{
		static const TCHAR* Desert[] = { TEXT("Areia"), TEXT("Cascalho"), TEXT("Terra"), TEXT("Rocha"), TEXT("Sal"), TEXT("Asfalto") };
		static const TCHAR* Snow[] = { TEXT("Neve"), TEXT("Cascalho"), TEXT("Lama"), TEXT("Rocha"), TEXT("Gelo"), TEXT("Asfalto") };
		static const TCHAR* Forest[] = { TEXT("Grama"), TEXT("Folhiço"), TEXT("Trilha"), TEXT("Rocha"), TEXT("Lama"), TEXT("") };
		const EWorldBiome B = GameMode.IsValid() ? GameMode->GetSettings().Biome : EWorldBiome::Temperate;
		const TCHAR* const* L = B == EWorldBiome::Desert ? Desert : (B == EWorldBiome::Winter ? Snow : Forest);
		return FText::FromString(L[I]);
	};
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (int32 I = 0; I < 6; ++I)
	{
		TSharedPtr<SButton> Button;
		Row->AddSlot().FillWidth(1.0f).Padding(1.0f, 0.0f)
		[
			SAssignNew(Button, SButton)
			.HAlign(HAlign_Center)
			.ContentPadding(FMargin(0.0f, 4.0f))
			.Visibility_Lambda([Names, I]() { return Names(I).IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.ButtonColorAndOpacity_Lambda([this, I]() { return GameMode.IsValid() && GameMode->BrushLayer == I ? Accent : ButtonIdle; })
			.OnClicked_Lambda([this, I]() { if (GameMode.IsValid()) GameMode->BrushLayer = I; return FReply::Handled(); })
			[
				SNew(STextBlock).Font(Font(8)).Text_Lambda([Names, I]() { return Names(I); })
				.ColorAndOpacity_Lambda([this, I]() { return FSlateColor(GameMode.IsValid() && GameMode->BrushLayer == I ? FLinearColor::Black : FLinearColor::White); })
			]
		];
		TestTargets.Add(FString::Printf(TEXT("BrushLayer_%d"), I), Button);
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
		[
			SNew(STextBlock).Text(LOCTEXT("BrushLayer", "Camada do pincel (arraste no mapa)")).Font(Font(9)).ColorAndOpacity(FSlateColor(TextDim))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Row
		];
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
		TSharedPtr<SButton> Button;
		SavedList->AddSlot().AutoHeight().Padding(0, 1)
		[
			SAssignNew(Button, SButton)
			.ButtonColorAndOpacity(FLinearColor(0.10f, 0.11f, 0.12f))
			.ContentPadding(FMargin(6, 3))
			.OnClicked_Lambda([this, S, I]()
			{
				Edit = S;
				if (GameMode.IsValid())
				{
					GameMode->OpenSavedMap(I);   // com as edições à mão dele
					Stats->SetText(LOCTEXT("Generating", "gerando..."));
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(FText::FromString(Label)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			]
		];
		TestTargets.Add(FString::Printf(TEXT("Saved_%d"), I), Button);
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
	if (!GameMode->GetImportTitle().IsEmpty())
	{
		// mapa trazido de fora: o nome, o que ficou de fora e o que veio
		const FString& Note = GameMode->GetImportNote();
		Stats->SetText(FText::FromString(FString::Printf(
			TEXT("%s (Zero Hour) · %d jogadores · %.0f m\nrelevo, texturas, árvores, pedras e estradas de lá, com o nosso acabamento\n%s%s%.0f%% transitável · %d campos de recurso · %d árvores"),
			*GameMode->GetImportTitle(), M.Starts.Num(), M.Settings.MapSize, *Note, Note.IsEmpty() ? TEXT("") : TEXT("\n"),
			100.0f * M.OpenFraction, M.Resources.Num(), Trees)));
		return;
	}
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
