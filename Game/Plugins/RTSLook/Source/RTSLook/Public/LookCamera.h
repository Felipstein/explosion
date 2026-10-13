#pragma once

// A câmera que o look lê (a do jogo implementa): o ponto olhado, a distância até ele e o giro.
// A névoa e a perspectiva aérea começam numa proporção dessa distância (a regra de RTS: o foco
// fica limpo em qualquer zoom), e o sol é escolhido pelo giro da câmera inicial do mapa.

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LookCamera.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class ULookCamera : public UInterface
{
	GENERATED_BODY()
};

class RTSLOOK_API ILookCamera
{
	GENERATED_BODY()

public:
	/** O ponto do chão que a câmera olha (cm, mundo). */
	virtual FVector GetLookFocusPoint() const = 0;
	/** Da câmera ao ponto olhado (cm). */
	virtual float GetLookFocusDistance() const = 0;
	/** O giro da câmera (graus, o yaw da Unreal). */
	virtual float GetLookYaw() const = 0;
};
