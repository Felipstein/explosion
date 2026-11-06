// Os traços lisos do HUD no Canvas (02/11, o ponto de encontro: "as imagens que você mostrou como prévias estavam bem melhores").
// A linha grossa e o polígono do Canvas saem serrilhados; aqui o traço é uma faixa de triângulos com a textura do perfil (branca,
// o alfa caindo em ~1 px de tela), como a borda dos mestres em SVG (art/cursors). Usado pelo RTSRallyMarker e pelo RTSOrderMarker.
#pragma once

#include "CoreMinimal.h"

class UCanvas;

namespace RTSSoftDraw
{
	/** Mistura em sRGB (como nos quadros aprovados) e devolve a cor linear do Canvas. */
	FLinearColor Mix(const FColor& A, const FColor& B, float T);
	/** O traço da polilinha com a borda lisa: a faixa contínua (as juntas em esquadria) e as pontas redondas; fechado, sem pontas. */
	void Stroke(UCanvas* Canvas, const TArray<FVector2D>& Pts, float Width, const FLinearColor& Color, bool bClosed = false);
	/** O disco cheio com a borda lisa. */
	void Disc(UCanvas* Canvas, const FVector2D& At, float Radius, const FLinearColor& Color);
	/** O triângulo cheio (sem a borda lisa: pras peças miúdas). */
	void Triangle(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color);
	/** O polígono cheio, em leque a partir do 1º ponto (ele tem de ver todos os outros), com a borda lisa: o traço de 1 px da
	 *  mesma cor por cima da borda. */
	void Fill(UCanvas* Canvas, const TArray<FVector2D>& Pts, const FLinearColor& Color);
	/** O contorno do polígono lado a lado, com os cantos redondos (a esquadria do traço fechado dobra no canto côncavo, como a
	 *  farpa da seta, e risca o polígono por dentro). */
	void Outline(UCanvas* Canvas, const TArray<FVector2D>& Pts, float Width, const FLinearColor& Color);
	/** (05/11, o véu da mira dos poderes) A faixa fechada entre dois anéis do mesmo número de pontos, a cor indo de um ao outro
	 *  (o degradê: o miolo quase limpo e a borda mais forte). O anel de dentro pode ser um ponto só repetido (o leque). */
	void Band(UCanvas* Canvas, const TArray<FVector2D>& Inner, const TArray<FVector2D>& Outer, const FLinearColor& InnerColor, const FLinearColor& OuterColor);
}
