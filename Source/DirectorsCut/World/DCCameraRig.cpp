#include "World/DCCameraRig.h"
#include "Camera/CameraComponent.h"
#include "Core/DCSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"

namespace
{
	// Critically-damped spring (frame-rate independent, eases in and out).
	template <typename T>
	T SmoothDamp(const T& Current, const T& Target, T& Velocity, float SmoothTime, float Dt)
	{
		SmoothTime = FMath::Max(0.0001f, SmoothTime);
		const float Omega = 2.f / SmoothTime;
		const float X = Omega * Dt;
		const float Exp = 1.f / (1.f + X + 0.48f * X * X + 0.235f * X * X * X);
		const T Change = Current - Target;
		const T Temp = (Velocity + Change * Omega) * Dt;
		Velocity = (Velocity - Temp * Omega) * Exp;
		return Target + (Change + Temp) * Exp;
	}
}

ADCCameraRig::ADCCameraRig()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	SetRootComponent(Pivot);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Pivot);
	Camera->bConstrainAspectRatio = false;
	ApplyRig();
}

void ADCCameraRig::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyRig();
}

void ADCCameraRig::BeginPlay()
{
	Super::BeginPlay();
	CurPivot = TargetPivot = GetActorLocation();
	CurDistance = TargetDistance = Distance;
	RecomputeOverview();
	ResetFocus();
	SnapToTarget();
}

FVector ADCCameraRig::FlatForward() const
{
	return FRotator(0.f, Yaw, 0.f).Vector();
}

FVector ADCCameraRig::FlatRight() const
{
	return FRotator(0.f, Yaw + 90.f, 0.f).Vector();
}

FVector2D ADCCameraRig::GetViewportSize() const
{
	FVector2D Size(1600.f, 900.f);
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D S; GEngine->GameViewport->GetViewportSize(S);
		if (S.X > 8.f && S.Y > 8.f) { Size = S; }
	}
	return Size;
}

void ADCCameraRig::ApplyRig()
{
	if (!Camera) { return; }
	const FRotator Rot(Pitch, Yaw, 0.f);
	Camera->SetRelativeLocation(-Rot.Vector() * Distance);
	Camera->SetRelativeRotation(Rot);
	Camera->SetFieldOfView(FOV);
}

void ADCCameraRig::RecomputeOverview()
{
	if (!Bounds.IsValid) { return; }
	const FBox B = (Framing.IsValid ? Framing : Bounds).ExpandBy(FVector(OverviewMargin, OverviewMargin, 0.f));
	const FRotationMatrix M(FRotator(Pitch, Yaw, 0.f));
	const FVector F = M.GetUnitAxis(EAxis::X), R = M.GetUnitAxis(EAxis::Y), U = M.GetUnitAxis(EAxis::Z);
	const FVector2D VP = GetViewportSize();
	const float Aspect = VP.X / FMath::Max(1.f, VP.Y);
	LastAspect = Aspect;
	const float TanH = FMath::Tan(FMath::DegreesToRadians(FOV * 0.5f));
	const float TanV = TanH / Aspect;
	const FVector C = B.GetCenter();
	float Need = MinDistance;
	for (int32 i = 0; i < 8; ++i)
	{
		const FVector Corner((i & 1) ? B.Max.X : B.Min.X, (i & 2) ? B.Max.Y : B.Min.Y, (i & 4) ? B.Max.Z : B.Min.Z);
		const FVector D = Corner - C;
		const float x = FMath::Abs(FVector::DotProduct(D, R));
		const float y = FMath::Abs(FVector::DotProduct(D, U));
		const float z = FVector::DotProduct(D, F);
		Need = FMath::Max(Need, x / TanH + z);
		Need = FMath::Max(Need, y / TanV + z);
	}
	OverviewPivot = C;
	OverviewDistance = Need;
}

FVector ADCCameraRig::ClampPivot(const FVector& P) const
{
	if (!Bounds.IsValid) { return P; }
	const FBox B = Bounds.ExpandBy(FVector(OverviewMargin * 0.5f, OverviewMargin * 0.5f, 0.f));
	return FVector(FMath::Clamp(P.X, B.Min.X, B.Max.X), FMath::Clamp(P.Y, B.Min.Y, B.Max.Y), P.Z);
}

void ADCCameraRig::FocusOn(const FVector& WorldPoint, float ZoomAlpha)
{
	Mode = EMode::Focus;
	FocusZoomAlpha = FMath::Clamp(ZoomAlpha, 0.f, 1.f);
	TargetPivot = WorldPoint;
	TargetDistance = FMath::Lerp(OverviewDistance, MinDistance, FocusZoomAlpha);
	ActiveSmoothTime = FocusSmoothTime;
}

void ADCCameraRig::ResetFocus()
{
	if (bCinematic) { SavedMode = EMode::Overview; SavedPivot = OverviewPivot; SavedDistance = OverviewDistance; return; }
	Mode = EMode::Overview;
	TargetPivot = OverviewPivot;
	TargetDistance = OverviewDistance;
	ActiveSmoothTime = FocusSmoothTime;
}

void ADCCameraRig::Pan(const FVector2D& ScreenDelta)
{
	if (bCinematic || ScreenDelta.IsNearlyZero()) { return; }
	const FVector2D VP = GetViewportSize();
	const float WorldPerPixel = 2.f * CurDistance * FMath::Tan(FMath::DegreesToRadians(FOV * 0.5f)) / FMath::Max(1.f, VP.X);
	const float SinPitch = FMath::Max(0.2f, FMath::Abs(FMath::Sin(FMath::DegreesToRadians(Pitch))));
	// Grab-the-world: dragging right moves the world right (camera left); dragging down moves the camera forward.
	const FVector Move = -FlatRight() * ScreenDelta.X * WorldPerPixel + FlatForward() * ScreenDelta.Y * WorldPerPixel / SinPitch;
	Mode = EMode::Free;
	TargetPivot = ClampPivot(TargetPivot + Move);
	ActiveSmoothTime = PanSmoothTime;
}

void ADCCameraRig::PanAxis(const FVector2D& Axis, float DeltaTime)
{
	if (bCinematic || Axis.IsNearlyZero()) { return; }
	const FVector2D A = Axis.GetSafeNormal() * FMath::Min(1.f, Axis.Size());
	const float Speed = CurDistance * KeyPanSpeed * PanSpeedMul;
	Mode = EMode::Free;
	TargetPivot = ClampPivot(TargetPivot + (FlatRight() * A.X + FlatForward() * A.Y) * Speed * DeltaTime);
	ActiveSmoothTime = PanSmoothTime;
}

void ADCCameraRig::Zoom(float Delta)
{
	if (bCinematic || FMath::IsNearlyZero(Delta)) { return; }
	if (Mode == EMode::Overview) { Mode = EMode::Free; }
	TargetDistance = FMath::Clamp(TargetDistance * FMath::Pow(0.87f, Delta * ZoomSpeedMul), MinDistance, GetMaxDistance());
	if (ActiveSmoothTime > ZoomSmoothTime) { ActiveSmoothTime = ZoomSmoothTime; }
	Distance = TargetDistance;
}

void ADCCameraRig::SetBounds(const FBox& WorldBounds)
{
	if (!WorldBounds.IsValid) { return; }
	const bool bChanged = !Bounds.Equals(WorldBounds, 1.f);
	Bounds = WorldBounds;
	RecomputeOverview();
	if (bChanged && Mode == EMode::Overview && !bCinematic)
	{
		TargetPivot = OverviewPivot;
		TargetDistance = OverviewDistance;
		ActiveSmoothTime = FocusSmoothTime * 1.6f;
	}
	if (!bInitialized) { SnapToTarget(); }
}

void ADCCameraRig::SetFraming(const FBox& WorldFraming)
{
	const bool bChanged = WorldFraming.IsValid != Framing.IsValid || (WorldFraming.IsValid && !Framing.Equals(WorldFraming, 1.f));
	Framing = WorldFraming;
	if (!bChanged) { return; }
	RecomputeOverview();
	if (Mode == EMode::Overview && !bCinematic)
	{
		TargetPivot = OverviewPivot;
		TargetDistance = OverviewDistance;
		ActiveSmoothTime = FocusSmoothTime * 1.6f;
	}
	if (!bInitialized) { SnapToTarget(); }
}

void ADCCameraRig::BeginCinematic(const FVector& WorldPoint, float ZoomAlpha, float MaxSeconds)
{
	if (!bCinematic)
	{
		SavedMode = Mode;
		SavedPivot = TargetPivot;
		SavedDistance = TargetDistance;
	}
	bCinematic = true;
	CinematicTimeLeft = FMath::Max(0.5f, MaxSeconds);
	TargetPivot = WorldPoint;
	TargetDistance = FMath::Lerp(OverviewDistance, MinDistance, FMath::Clamp(ZoomAlpha, 0.f, 1.f));
	ActiveSmoothTime = FocusSmoothTime * 1.2f;
}

void ADCCameraRig::EndCinematic()
{
	if (!bCinematic) { return; }
	bCinematic = false;
	Mode = SavedMode;
	if (Mode == EMode::Overview) { TargetPivot = OverviewPivot; TargetDistance = OverviewDistance; }
	else { TargetPivot = SavedPivot; TargetDistance = SavedDistance; }
	ActiveSmoothTime = FocusSmoothTime * 1.2f;
}

void ADCCameraRig::SnapToTarget()
{
	CurPivot = TargetPivot;
	CurDistance = TargetDistance;
	PivotVel = FVector::ZeroVector;
	DistanceVel = 0.f;
	bInitialized = true;
	SetActorLocation(CurPivot);
}

void ADCCameraRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Clamp(DeltaSeconds, 0.f, 0.1f);

	if (bCinematic)
	{
		CinematicTimeLeft -= Dt;
		if (CinematicTimeLeft <= 0.f) { EndCinematic(); }
	}

	// Re-fit when the window aspect changes.
	const FVector2D VP = GetViewportSize();
	const float Aspect = VP.X / FMath::Max(1.f, VP.Y);
	if (!FMath::IsNearlyEqual(Aspect, LastAspect, 0.01f))
	{
		RecomputeOverview();
		if (Mode == EMode::Overview && !bCinematic) { TargetPivot = OverviewPivot; TargetDistance = OverviewDistance; }
		else if (Mode == EMode::Focus && !bCinematic) { TargetDistance = FMath::Lerp(OverviewDistance, MinDistance, FocusZoomAlpha); }
	}

	CurPivot = SmoothDamp(CurPivot, TargetPivot, PivotVel, ActiveSmoothTime, Dt);
	CurDistance = SmoothDamp(CurDistance, TargetDistance, DistanceVel, FMath::Max(ActiveSmoothTime, ZoomSmoothTime), Dt);
	SetActorLocation(CurPivot);

	const FRotator Rot(Pitch, Yaw, 0.f);
	Camera->SetRelativeLocation(-Rot.Vector() * CurDistance);
	Camera->SetRelativeRotation(Rot);
	Camera->SetFieldOfView(FOV);

	// Tilt-shift: shallow cinematic DOF focused on the pivot.
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	PP.bOverride_DepthOfFieldFocalDistance = bTiltShiftDOF;
	PP.bOverride_DepthOfFieldFstop = bTiltShiftDOF;
	PP.bOverride_DepthOfFieldMinFstop = bTiltShiftDOF;
	PP.bOverride_DepthOfFieldSensorWidth = bTiltShiftDOF;
	PP.DepthOfFieldFocalDistance = CurDistance;
	PP.DepthOfFieldFstop = DOFFstop;
	PP.DepthOfFieldMinFstop = FMath::Min(DOFFstop, 1.2f);
	PP.DepthOfFieldSensorWidth = DOFSensorWidth;
	Camera->PostProcessBlendWeight = 1.f;
}
