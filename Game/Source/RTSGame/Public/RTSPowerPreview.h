// A mira dos poderes do general no chão (05/11; o pedido da sessão dos poderes, docs/poderes/IMPLEMENTACAO.md §2.3; o estudo em
// docs/poderes/MIRA.md; a prancha art/cursors/poderes/mira.html). O ZH desenha um decalque por poder (RadiusCursor, do raio de
// verdade, a opacidade pulsando); aqui as 6 formas que cobrem os ~65 poderes, na língua dos cursores: o contorno escuro e a linha
// clara, a cor pelo tipo do ícone (golpe, apoio, tropas, olhos), a área no chão do tamanho de verdade acompanhando o relevo, a borda
// nítida e o miolo um véu (nunca chapado), o inválido em cinza com o motivo. O cursor do mouse (Strike/Support) marca o centro.
// A sessão dos poderes preenche o FSpec com os números da mecânica (o que a prévia mostra = o que a simulação faz) e chama Show a
// cada quadro enquanto mira; sem o Show de um quadro, a prévia some sozinha.
#pragma once

#include "CoreMinimal.h"

class ARTSPlayerController;
class UCanvas;

namespace RTSPowerPreview
{
	enum class EShape : uint8
	{
		Circle,   // o golpe ou o apoio num ponto (a MOAB, o A-10 I, o conserto, o raio de visão do drone)
		Strip,    // a faixa com direção: clicar o início e arrastar (o A-10 II, o Tornado-S, o Naval I–II, o Carpet Bomb)
		Path,     // o traçado livre (a Fortified Line)
		Drop,     // o ponto de queda das tropas (o Paradrop, o VDV, o SEAL)
		Target,   // o alvo único (a Defection, o Booby Trap, o Wreck Rebuild)
		Orbit,    // a órbita (o AC-130): o anel fica enquanto o poder dura
	};
	enum class EKind : uint8 { Strike, Support, Troops, Eyes };

	struct FSpec
	{
		EShape Shape = EShape::Circle;
		EKind Kind = EKind::Strike;
		/** O ponto: o centro do círculo, da queda e da órbita; o início da faixa. */
		FVector2D CenterCm = FVector2D::ZeroVector;
		/** O raio do círculo, do espalhamento da queda e da órbita. */
		float RadiusCm = 0.0f;
		/** O círculo: o 2º anel tracejado, onde as bombas podem cair (0: sem). */
		float ScatterCm = 0.0f;
		/** O círculo: menor que 1 achata o anel na direção Dir (a elipse do Naval III). */
		float SquashY = 1.0f;
		/** A faixa e a elipse: pra onde vai, no chão (unitário). */
		FVector2D Dir = FVector2D(1.0, 0.0);
		/** A faixa: o comprimento e a largura da passada, e as fileiras lado a lado (o Naval II: 3). */
		float LengthCm = 0.0f;
		float WidthCm = 0.0f;
		int32 Rows = 1;
		/** O traçado: os pontos já desenhados (o último é o mouse) e o comprimento que o poder permite. */
		TArray<FVector2D> PathCm;
		float PathMaxCm = 0.0f;
		/** A queda: onde cai cada tropa. */
		TArray<FVector2D> DropCm;
		/** O alvo único: a unidade ou o prédio debaixo do mouse (0: nenhum alvo que vale). */
		uint32 TargetId = 0;
		/** Quem o poder pega agora (a sessão dos poderes preenche pela regra do poder: no Emergency Repair, os veículos seus e dos
		 *  aliados no raio; no II, os prédios também): cada um ganha um anel fino no chão, na cor do tipo. */
		TArray<uint32> AffectedIds;
		/** Vale? Senão a área em cinza, tracejada e cortada, e o motivo curto em inglês perto do mouse ("OUT OF RANGE", "NO VISION",
		 *  "NO COMMAND CENTER"). */
		bool bValid = true;
		FString Reason;
	};

	/** A mira deste quadro (chamar a cada quadro enquanto mira). */
	void Show(const FSpec& Spec);
	void Hide();
	/** Houve Show neste quadro (ou no anterior)? O círculo antigo do HUD (ARTSHUD::DrawPowerTarget) sai quando há. */
	bool IsShown();
	/** O HUD desenha (ARTSHUD::DrawMarkers). */
	void Draw(UCanvas* Canvas, ARTSPlayerController* PC);
	/** Teste: -RTSPowerPreviewAt=X,Y (m) mostra as 6 formas em volta do ponto, uma a cada 2,5 s, em 3 voltas: a
	 *  válida, a válida com o círculo de apoio acendendo as unidades dentro dele, e a inválida. */
	void DrawTest(UCanvas* Canvas, ARTSPlayerController* PC);
}
