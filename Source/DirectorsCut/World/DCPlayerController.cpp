#include "World/DCPlayerController.h"
#include "World/DCCameraRig.h"
#include "World/DCStation.h"
#include "Core/DCUISubsystem.h"
#include "Engine/World.h"

ADCPlayerController::ADCPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ADCPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);

	CameraRig = GetWorld()->SpawnActor<ADCCameraRig>(ADCCameraRig::StaticClass(), FTransform(FVector(0, 0, 100)));
	if (CameraRig) { SetViewTarget(CameraRig); }

	if (UDCUISubsystem* UI = UDCUISubsystem::Get(this))
	{
		UI->InitializeUI(this);
	}
}

void ADCPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	UDCUISubsystem* UI = UDCUISubsystem::Get(this);
	const bool bBlocked = UI && UI->IsWorldInputBlocked();

	// Hover
	ADCStation* NewHover = nullptr;
	if (!bBlocked)
	{
		FHitResult Hit;
		if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
		{
			NewHover = Cast<ADCStation>(Hit.GetActor());
			if (NewHover && !NewHover->IsStationEnabled()) { NewHover = nullptr; }
		}
	}
	if (NewHover != HoveredStation.Get())
	{
		if (HoveredStation.IsValid()) { HoveredStation->OnHoverEnd(); }
		HoveredStation = NewHover;
		if (NewHover) { NewHover->OnHoverBegin(); }
	}

	if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && HoveredStation.IsValid())
	{
		HoveredStation->OnClicked();
	}
	if (WasInputKeyJustPressed(EKeys::Escape) && UI) { UI->HandleBack(); }
	if (WasInputKeyJustPressed(EKeys::SpaceBar) && UI) { UI->HandleRollCamera(); }
	if (CameraRig)
	{
		if (WasInputKeyJustPressed(EKeys::MouseScrollUp)) { CameraRig->Zoom(1.f); }
		if (WasInputKeyJustPressed(EKeys::MouseScrollDown)) { CameraRig->Zoom(-1.f); }
	}
}
