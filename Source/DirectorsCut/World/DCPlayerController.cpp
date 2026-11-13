#include "World/DCPlayerController.h"
#include "World/DCCameraRig.h"
#include "World/DCStation.h"
#include "World/DCWorldDirector.h"
#include "Core/DCUISubsystem.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "DirectorsCut.h"

ADCPlayerController::ADCPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ADCPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!MappingContext)
	{
		ActClick = NewObject<UInputAction>(this, TEXT("IA_DC_Click"));
		ActClick->ValueType = EInputActionValueType::Boolean;
		ActZoom = NewObject<UInputAction>(this, TEXT("IA_DC_Zoom"));
		ActZoom->ValueType = EInputActionValueType::Axis1D;
		ActBack = NewObject<UInputAction>(this, TEXT("IA_DC_Back"));
		ActBack->ValueType = EInputActionValueType::Boolean;
		ActRoll = NewObject<UInputAction>(this, TEXT("IA_DC_Roll"));
		ActRoll->ValueType = EInputActionValueType::Boolean;

		MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_DC_World"));
		MappingContext->MapKey(ActClick, EKeys::LeftMouseButton);
		MappingContext->MapKey(ActZoom, EKeys::MouseWheelAxis);
		MappingContext->MapKey(ActBack, EKeys::Escape);
		MappingContext->MapKey(ActRoll, EKeys::SpaceBar);
	}

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(ActClick, ETriggerEvent::Started, this, &ADCPlayerController::OnClickAction);
		EIC->BindAction(ActZoom, ETriggerEvent::Triggered, this, &ADCPlayerController::OnZoomAction);
		EIC->BindAction(ActBack, ETriggerEvent::Started, this, &ADCPlayerController::OnBackAction);
		EIC->BindAction(ActRoll, ETriggerEvent::Started, this, &ADCPlayerController::OnRollAction);
		bEnhancedInputBound = true;
	}
	else
	{
		UE_LOG(LogDC, Warning, TEXT("DCPlayerController: InputComponent is not Enhanced; falling back to key polling."));
	}
}

void ADCPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);

	if (bEnhancedInputBound && MappingContext)
	{
		if (ULocalPlayer* LP = GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Sub->AddMappingContext(MappingContext, 0);
			}
			else { bEnhancedInputBound = false; }
		}
	}

	CameraRig = GetWorld()->SpawnActor<ADCCameraRig>(ADCCameraRig::StaticClass(), FTransform(FVector(0, 0, 100)));
	if (CameraRig) { SetViewTarget(CameraRig); }

	// The world director keeps rooms / pawns / doors in sync with the run. Spawned before the UI so it
	// sees the run start.
	FActorSpawnParameters SP;
	SP.Name = TEXT("DCWorldDirector");
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	WorldDirector = GetWorld()->SpawnActor<ADCWorldDirector>(ADCWorldDirector::StaticClass(), FTransform::Identity, SP);
	if (WorldDirector) { WorldDirector->Initialize(this); }

	if (UDCUISubsystem* UI = UDCUISubsystem::Get(this))
	{
		PanelHandle = UI->OnPanelChanged.AddUObject(this, &ADCPlayerController::OnPanelChanged);
		bPanelBound = true;
		UI->InitializeUI(this);
	}
}

void ADCPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bPanelBound)
	{
		if (UDCUISubsystem* UI = UDCUISubsystem::Get(this)) { UI->OnPanelChanged.Remove(PanelHandle); }
		bPanelBound = false;
	}
	Super::EndPlay(Reason);
}

bool ADCPlayerController::IsWorldBlocked() const
{
	if (CameraRig && CameraRig->IsInCinematic()) { return true; }
	const UDCUISubsystem* UI = UDCUISubsystem::Get(this);
	return UI && UI->IsWorldInputBlocked();
}

void ADCPlayerController::NotifyStationFocused(ADCStation* Station)
{
	bFocusFromStation = Station != nullptr;
}

void ADCPlayerController::OnPanelChanged(EDCPanel Panel)
{
	const EDCPanel Prev = LastPanel;
	LastPanel = Panel;
	if (Panel == EDCPanel::None && Prev != EDCPanel::None && bFocusFromStation)
	{
		bFocusFromStation = false;
		if (CameraRig) { CameraRig->ResetFocus(); }
	}
}

void ADCPlayerController::OnClickAction(const FInputActionValue& Value)
{
	if (IsWorldBlocked()) { return; }
	if (ADCStation* S = HoveredStation.Get())
	{
		if (S->IsStationEnabled()) { S->OnClicked(); }
	}
}

void ADCPlayerController::OnZoomAction(const FInputActionValue& Value)
{
	ZoomAccum += Value.Get<float>();
}

void ADCPlayerController::OnBackAction(const FInputActionValue& Value)
{
	if (UDCUISubsystem* UI = UDCUISubsystem::Get(this)) { UI->HandleBack(); }
}

void ADCPlayerController::OnRollAction(const FInputActionValue& Value)
{
	if (UDCUISubsystem* UI = UDCUISubsystem::Get(this)) { UI->HandleRollCamera(); }
}

void ADCPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	const bool bBlocked = IsWorldBlocked();

	// Hover
	ADCStation* NewHover = nullptr;
	if (!bBlocked && !bRmbDragging)
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
		CurrentMouseCursor = NewHover ? EMouseCursor::Hand : EMouseCursor::Default;
	}

	if (!bEnhancedInputBound)
	{
		// Fallback key polling.
		if (WasInputKeyJustPressed(EKeys::LeftMouseButton)) { OnClickAction(FInputActionValue(true)); }
		if (WasInputKeyJustPressed(EKeys::Escape)) { OnBackAction(FInputActionValue(true)); }
		if (WasInputKeyJustPressed(EKeys::SpaceBar)) { OnRollAction(FInputActionValue(true)); }
		if (WasInputKeyJustPressed(EKeys::MouseScrollUp)) { ZoomAccum += 1.f; }
		if (WasInputKeyJustPressed(EKeys::MouseScrollDown)) { ZoomAccum -= 1.f; }
	}

	if (!CameraRig) { ZoomAccum = 0.f; return; }

	// Zoom (accumulated this frame)
	if (!bBlocked && !FMath::IsNearlyZero(ZoomAccum)) { CameraRig->Zoom(ZoomAccum); }
	ZoomAccum = 0.f;

	// RMB drag pan
	FVector2D Mouse;
	const bool bHaveMouse = GetMousePosition(Mouse.X, Mouse.Y);
	if (!bBlocked && bHaveMouse && IsInputKeyDown(EKeys::RightMouseButton))
	{
		if (bRmbDragging) { CameraRig->Pan(Mouse - LastMousePos); }
		bRmbDragging = true;
		LastMousePos = Mouse;
	}
	else
	{
		bRmbDragging = false;
	}

	// WASD / arrows pan
	if (!bBlocked)
	{
		FVector2D Axis = FVector2D::ZeroVector;
		if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up)) { Axis.Y += 1.f; }
		if (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down)) { Axis.Y -= 1.f; }
		if (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) { Axis.X += 1.f; }
		if (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left)) { Axis.X -= 1.f; }
		if (!Axis.IsZero()) { CameraRig->PanAxis(Axis, DeltaTime); }
	}
}
