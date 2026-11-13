#include "World/DCCameraRig.h"
#include "Camera/CameraComponent.h"

ADCCameraRig::ADCCameraRig()
{
	PrimaryActorTick.bCanEverTick = true;
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	SetRootComponent(Pivot);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Pivot);
	ApplyRig();
}

void ADCCameraRig::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyRig();
}

void ADCCameraRig::ApplyRig()
{
	const FRotator Rot(Pitch, Yaw, 0.f);
	Camera->SetRelativeLocation(-Rot.Vector() * Distance);
	Camera->SetRelativeRotation(Rot);
	Camera->SetFieldOfView(FOV);
}

void ADCCameraRig::FocusOn(const FVector& WorldPoint, float ZoomAlpha) { SetActorLocation(WorldPoint); }
void ADCCameraRig::ResetFocus() { SetActorLocation(Bounds.GetCenter()); }
void ADCCameraRig::Pan(const FVector2D& ScreenDelta) {}
void ADCCameraRig::Zoom(float Delta) { Distance = FMath::Clamp(Distance - Delta * 300.f, 2500.f, 9000.f); ApplyRig(); }
void ADCCameraRig::SetBounds(const FBox& WorldBounds) { Bounds = WorldBounds; }
