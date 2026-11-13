// Director's Cut — player controller: mouse cursor, hover/click on stations, camera input. Agent E owns.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DCPlayerController.generated.h"

class ADCCameraRig;
class ADCStation;

UCLASS()
class DIRECTORSCUT_API ADCPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ADCPlayerController();
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

	ADCCameraRig* GetCameraRig() const { return CameraRig; }

protected:
	UPROPERTY() TObjectPtr<ADCCameraRig> CameraRig;
	TWeakObjectPtr<ADCStation> HoveredStation;
};
