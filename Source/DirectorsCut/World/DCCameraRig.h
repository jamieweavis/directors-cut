// Director's Cut — telephoto iso camera. Agent E owns behaviour; class name and public API are a contract.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCameraRig.generated.h"

class UCameraComponent;
class USpringArmComponent;

UCLASS()
class DIRECTORSCUT_API ADCCameraRig : public AActor
{
	GENERATED_BODY()
public:
	ADCCameraRig();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> Pivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditAnywhere, Category = "DC") float Yaw = 45.f;
	UPROPERTY(EditAnywhere, Category = "DC") float Pitch = -35.f;
	UPROPERTY(EditAnywhere, Category = "DC") float FOV = 18.f;
	UPROPERTY(EditAnywhere, Category = "DC") float Distance = 6000.f;

	/** Smoothly move the pivot to a world point (station / room focus). */
	virtual void FocusOn(const FVector& WorldPoint, float ZoomAlpha = 0.5f);
	/** Return to the overview framing. */
	virtual void ResetFocus();
	virtual void Pan(const FVector2D& ScreenDelta);
	virtual void Zoom(float Delta);
	/** Clamp pivot to this box (grows as rooms are built). */
	virtual void SetBounds(const FBox& WorldBounds);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	void ApplyRig();
	FBox Bounds = FBox(FVector(-2000), FVector(2000));
};
