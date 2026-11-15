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

	// ── Tuning (Agent E) ─────────────────────────────────────────────────────
	/** Extra world-space margin around the built rooms in the overview (shows locked silhouettes). */
	UPROPERTY(EditAnywhere, Category = "DC|Tuning") float OverviewMargin = 350.f;
	/** Closest the camera may get (scroll / focus). */
	UPROPERTY(EditAnywhere, Category = "DC|Tuning") float MinDistance = 6500.f;
	/** Max zoom-out as a multiple of the overview distance. */
	UPROPERTY(EditAnywhere, Category = "DC|Tuning") float MaxDistanceOverOverview = 1.15f;
	/** Smooth time (s) for focus/reset dollies and pan/zoom. */
	UPROPERTY(EditAnywhere, Category = "DC|Tuning") float FocusSmoothTime = 0.45f;
	UPROPERTY(EditAnywhere, Category = "DC|Tuning") float PanSmoothTime = 0.10f;
	UPROPERTY(EditAnywhere, Category = "DC|Tuning") float ZoomSmoothTime = 0.07f; // scroll zoom: exponential time constant (s)
	/** Keyboard pan speed as a fraction of the current distance per second. */
	UPROPERTY(EditAnywhere, Category = "DC|Tuning") float KeyPanSpeed = 0.35f;
	/** Player settings multipliers (options screen), pushed by the player controller. */
	float PanSpeedMul = 1.f;
	float ZoomSpeedMul = 1.f;
	/** Tilt-shift DOF. */
	UPROPERTY(EditAnywhere, Category = "DC|Tuning") bool bTiltShiftDOF = true;
	UPROPERTY(EditAnywhere, Category = "DC|Tuning") float DOFFstop = 0.9f;
	UPROPERTY(EditAnywhere, Category = "DC|Tuning") float DOFSensorWidth = 120.f;
	/** Soft corner darkening (0 = off). Owned here, not by the scene post volume. */
	UPROPERTY(EditAnywhere, Category = "DC|Tuning") float VignetteIntensity = 0.5f;

	/** Smoothly move the pivot to a world point (station / room focus). ZoomAlpha 0 = overview distance, 1 = closest. */
	virtual void FocusOn(const FVector& WorldPoint, float ZoomAlpha = 0.5f);
	/** Return to the overview framing. */
	virtual void ResetFocus();
	/** Grab-pan by a mouse delta in screen pixels (x right, y down). */
	virtual void Pan(const FVector2D& ScreenDelta);
	/** Positive = zoom in (one scroll notch = 1). */
	virtual void Zoom(float Delta);
	/** Clamp pivot to this box (grows as rooms are built). The overview framing fits this box (+ margin). */
	virtual void SetBounds(const FBox& WorldBounds);

	// ── Additions (Agent E) ──────────────────────────────────────────────────
	/** Keyboard pan: Axis.X = right, Axis.Y = forward (-1..1), scaled by DeltaTime internally. */
	void PanAxis(const FVector2D& Axis, float DeltaTime);
	/** Scripted shot (room reveal): remembers the current view, flies to Point, and ignores player pan/zoom until EndCinematic (auto-ends after MaxSeconds). */
	void BeginCinematic(const FVector& WorldPoint, float ZoomAlpha, float MaxSeconds = 5.f);
	/** Returns to the view remembered by BeginCinematic. */
	void EndCinematic();
	bool IsInCinematic() const { return bCinematic; }
	bool IsFocused() const { return Mode == EMode::Focus; }
	/** Snap instantly to the current target (no easing). */
	void SnapToTarget();
	float GetOverviewDistance() const { return OverviewDistance; }
	float GetCurrentDistance() const { return CurDistance; }
	FBox GetBounds() const { return Bounds; }
	/** The box the overview framing fits (house + yard). Pan clamping still uses SetBounds (the whole site). Invalid = use the bounds. */
	void SetFraming(const FBox& WorldFraming);
	FVector GetOverviewPivot() const { return OverviewPivot; }
	/** Dev/video (-DCReel): put the camera exactly here this frame (no easing, no clamping). Call every frame to drive a move. */
	void DevSetView(const FVector& InPivot, float InDistance);
	/** Distance for a zoom alpha (0 = overview .. 1 = closest; below 0 zooms out past the overview, as far as the player may). */
	float DistanceForZoom(float ZoomAlpha) const { return ZoomAlpha >= 0.f ? FMath::Lerp(OverviewDistance, MinDistance, FMath::Min(ZoomAlpha, 1.f)) : FMath::Lerp(OverviewDistance, GetMaxDistance(), FMath::Min(-ZoomAlpha, 1.f)); }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	void ApplyRig();
	FBox Bounds = FBox(FVector(-2000), FVector(2000));
	FBox Framing = FBox(ForceInit);

	enum class EMode : uint8 { Overview, Focus, Free };
	EMode Mode = EMode::Overview;

	FVector TargetPivot = FVector::ZeroVector;
	FVector CurPivot = FVector::ZeroVector;
	FVector PivotVel = FVector::ZeroVector;
	float TargetDistance = 6000.f;
	float CurDistance = 6000.f;
	float DistanceVel = 0.f;
	float ActiveSmoothTime = 0.45f;
	float FocusZoomAlpha = 0.5f;
	FVector OverviewPivot = FVector::ZeroVector;
	float OverviewDistance = 6000.f;
	float LastAspect = 0.f;
	bool bInitialized = false;
	bool bPlayerZoom = false; // distance follows the scroll wheel (exponential), not a dolly (spring)

	// Cinematic
	bool bCinematic = false;
	float CinematicTimeLeft = 0.f;
	EMode SavedMode = EMode::Overview;
	FVector SavedPivot = FVector::ZeroVector;
	float SavedDistance = 6000.f;

	FVector2D GetViewportSize() const;
	void RecomputeOverview();
	FVector ClampPivot(const FVector& P) const;
	float GetMaxDistance() const { return FMath::Max(OverviewDistance * MaxDistanceOverOverview, MinDistance + 500.f); }
	FVector FlatForward() const;
	FVector FlatRight() const;
};
