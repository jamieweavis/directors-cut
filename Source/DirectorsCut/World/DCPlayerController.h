// Director's Cut — player controller: mouse cursor, hover/click on stations, camera input, build placement mode.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/DCTypes.h"
#include "DCPlayerController.generated.h"

class ADCCameraRig;
class ADCStation;
class ADCWorldDirector;
class ADCBuildPlacer;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class DIRECTORSCUT_API ADCPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ADCPlayerController();
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

	ADCCameraRig* GetCameraRig() const { return CameraRig; }

	// ── Additions (Agent E) ──────────────────────────────────────────────────
	ADCWorldDirector* GetWorldDirector() const { return WorldDirector; }
	ADCStation* GetHoveredStation() const { return HoveredStation.Get(); }
	/** A station click focused the camera: when its panel later closes, the camera eases back to the overview. */
	void NotifyStationFocused(ADCStation* Station);
	/** Build placement mode (picking a grid cell for a new room). */
	ADCBuildPlacer* GetBuildPlacer() const { return Placer; }
	bool IsPlacing() const;

protected:
	UPROPERTY() TObjectPtr<ADCCameraRig> CameraRig;
	TWeakObjectPtr<ADCStation> HoveredStation;

	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY() TObjectPtr<ADCWorldDirector> WorldDirector;
	UPROPERTY() TObjectPtr<ADCBuildPlacer> Placer;
	FDelegateHandle PlaceHandle;
	void OnPlacementRequested(EDCRoomType Room);
	UPROPERTY() TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY() TObjectPtr<UInputAction> ActClick;
	UPROPERTY() TObjectPtr<UInputAction> ActZoom;
	UPROPERTY() TObjectPtr<UInputAction> ActBack;

	void OnClickAction(const FInputActionValue& Value);
	void OnZoomAction(const FInputActionValue& Value);
	void OnBackAction(const FInputActionValue& Value);
	void OnPanelChanged(EDCPanel Panel);
	bool IsWorldBlocked() const;

	bool bEnhancedInputBound = false;
	bool bFocusFromStation = false;
	bool bRmbDragging = false;
	FVector2D LastMousePos = FVector2D::ZeroVector;
	FDelegateHandle PanelHandle;
	bool bPanelBound = false;
	EDCPanel LastPanel = EDCPanel::None;
	float ZoomAccum = 0.f;

	float SettingsPoll = 0.f;
	bool bEdgeScroll = false;
};
