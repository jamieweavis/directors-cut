#include "World/DCPlayerController.h"
#include "Core/DCDevTools.h"
#include "Core/DCSettings.h"
#include "World/DCCameraRig.h"
#include "World/DCStation.h"
#include "World/DCWorldDirector.h"
#include "World/DCBuildPlacer.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Core/DCRunSubsystem.h"
#include "Core/DCUISubsystem.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"
#include "Misc/App.h"
#include "DirectorsCut.h"

namespace
{
	/** A text box (search, name entry...) has keyboard focus: world hotkeys must not fire. */
	bool IsTextInputFocused()
	{
		if (!FSlateApplication::IsInitialized()) { return false; }
		const TSharedPtr<SWidget> W = FSlateApplication::Get().GetKeyboardFocusedWidget();
		if (!W.IsValid()) { return false; }
		const FString T = W->GetTypeAsString();
		return T.Contains(TEXT("EditableText")) || T.Contains(TEXT("MultiLineEditable")) || T.Contains(TEXT("SpinBox"));
	}

	/** The game window is the active OS window (edge-pan only then). */
	bool IsGameWindowFocused(const UWorld* World)
	{
		if (!FApp::HasFocus()) { return false; }
		const UGameViewportClient* VP = World ? World->GetGameViewport() : nullptr;
		const TSharedPtr<SWindow> Win = VP ? VP->GetWindow() : nullptr;
		return Win.IsValid() && Win->IsActive();
	}
}

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

		MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_DC_World"));
		MappingContext->MapKey(ActClick, EKeys::LeftMouseButton);
		MappingContext->MapKey(ActZoom, EKeys::MouseWheelAxis);
		MappingContext->MapKey(ActBack, EKeys::Escape);
		// Space / 1 / 2 / 3 (game speed) and the panel letters are UI hotkeys: SDCRoot::HandleHotkey.
	}

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(ActClick, ETriggerEvent::Started, this, &ADCPlayerController::OnClickAction);
		EIC->BindAction(ActZoom, ETriggerEvent::Triggered, this, &ADCPlayerController::OnZoomAction);
		EIC->BindAction(ActBack, ETriggerEvent::Started, this, &ADCPlayerController::OnBackAction);
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

	FActorSpawnParameters PSP;
	PSP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Placer = GetWorld()->SpawnActor<ADCBuildPlacer>(ADCBuildPlacer::StaticClass(), FTransform::Identity, PSP);

	if (UDCUISubsystem* UI = UDCUISubsystem::Get(this))
	{
		PlaceHandle = UI->OnRoomPlacementRequested.AddUObject(this, &ADCPlayerController::OnPlacementRequested);
		PanelHandle = UI->OnPanelChanged.AddUObject(this, &ADCPlayerController::OnPanelChanged);
		bPanelBound = true;
		UI->InitializeUI(this);
	}
}

void ADCPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bPanelBound)
	{
		if (UDCUISubsystem* UI = UDCUISubsystem::Get(this)) { UI->OnPanelChanged.Remove(PanelHandle); UI->OnRoomPlacementRequested.Remove(PlaceHandle); }
		bPanelBound = false;
	}
	Super::EndPlay(Reason);
}

bool ADCPlayerController::IsWorldBlocked() const
{
	if (CameraRig && CameraRig->IsInCinematic()) { return true; }
	// Screenshot runs (-DCShots) drive the camera and panels themselves: a stray mouse must not click a station or pan.
	static const bool bShotRun = FCString::Strifind(FCommandLine::Get(), TEXT("DCShots=")) != nullptr;
	if (bShotRun) { return true; }
	const UDCUISubsystem* UI = UDCUISubsystem::Get(this);
	return UI && UI->IsWorldInputBlocked();
}

void ADCPlayerController::NotifyStationFocused(ADCStation* Station)
{
	bFocusFromStation = Station != nullptr;
}

bool ADCPlayerController::IsPlacing() const { return Placer && Placer->IsActive(); }

void ADCPlayerController::OnPlacementRequested(EDCRoomType Room)
{
	if (!Placer) { return; }
	if (HoveredStation.IsValid()) { HoveredStation->OnHoverEnd(); HoveredStation = nullptr; }
	CurrentMouseCursor = EMouseCursor::Crosshairs;
	if (CameraRig && bFocusFromStation) { bFocusFromStation = false; CameraRig->ResetFocus(); }
	Placer->Begin(Room);
}

void ADCPlayerController::OnPanelChanged(EDCPanel Panel)
{
	const EDCPanel Prev = LastPanel;
	LastPanel = Panel;
	if (Panel != EDCPanel::None && IsPlacing()) { Placer->Cancel(true); }
	if (Panel == EDCPanel::None && Prev != EDCPanel::None && bFocusFromStation)
	{
		bFocusFromStation = false;
		if (CameraRig) { CameraRig->ResetFocus(); }
	}
}

void ADCPlayerController::OnClickAction(const FInputActionValue& Value)
{
	if (IsWorldBlocked()) { return; }
	if (IsPlacing()) { Placer->Click(); return; }
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
	if (IsTextInputFocused()) { return; } // the text box gets Esc
	if (IsPlacing()) { Placer->Cancel(); return; }
	// The UI decides: close the open panel, or open Pause when nothing is open.
	if (UDCUISubsystem* UI = UDCUISubsystem::Get(this)) { UI->HandleBack(); }
}

void ADCPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	const bool bBlocked = IsWorldBlocked();

	// Hover
	ADCStation* NewHover = nullptr;
	{
		// dev: -DCPlaceRoom=<EDCRoomType> enters placement mode once that room can be built (screenshots)
		static FString DevPlace = TEXT("?");
		static float DevPlaceT = 0.f;
		if (DevPlace == TEXT("?")) { DevPlace.Reset(); FParse::Value(FCommandLine::Get(), TEXT("DCPlaceRoom="), DevPlace); }
		const UDCRunSubsystem* DevRun = UDCRunSubsystem::Get(this);
		if (!DevPlace.IsEmpty() && DevRun && DevRun->HasActiveRun() && (DevPlaceT += DeltaTime) > 3.f)
		{
			const int64 V = StaticEnum<EDCRoomType>()->GetValueByNameString(DevPlace);
			if (V == INDEX_NONE) { DevPlace.Reset(); }
			else if (DevRun->GetRoom((EDCRoomType)V).bCanAct) { DevPlace.Reset(); OnPlacementRequested((EDCRoomType)V); }
		}
	}
	const bool bPlacing = IsPlacing();
	if (bPlacing) { Placer->UpdateCursor(this, DeltaTime); }
	// Video reels (-DCReel) keep the mouse out of the picture: no hover highlights from wherever it rests.
	if (!bBlocked && !bRmbDragging && !bPlacing && !UDCDevTools::IsReel())
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
	if (bPlacing) { CurrentMouseCursor = EMouseCursor::Crosshairs; }
	else if (CurrentMouseCursor == EMouseCursor::Crosshairs) { CurrentMouseCursor = EMouseCursor::Default; }

	if (!bEnhancedInputBound)
	{
		// Fallback key polling.
		if (WasInputKeyJustPressed(EKeys::LeftMouseButton)) { OnClickAction(FInputActionValue(true)); }
		if (WasInputKeyJustPressed(EKeys::Escape)) { OnBackAction(FInputActionValue(true)); }
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

	// Options-screen settings (cheap ini reads, refreshed once a second).
	SettingsPoll -= DeltaTime;
	if (SettingsPoll <= 0.f)
	{
		SettingsPoll = 1.f;
		bEdgeScroll = DCSettings::EdgeScroll();
		CameraRig->PanSpeedMul = FMath::Clamp(DCSettings::CameraPanSpeed(), 0.25f, 3.f);
		CameraRig->ZoomSpeedMul = FMath::Clamp(DCSettings::CameraZoomSpeed(), 0.25f, 3.f);
	}

	// WASD / arrows pan (not while typing), plus edge-pan (if enabled) when the window is focused and no panel is open.
	if (!bBlocked)
	{
		FVector2D Axis = FVector2D::ZeroVector;
		if (!IsTextInputFocused())
		{
			if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up)) { Axis.Y += 1.f; }
			if (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down)) { Axis.Y -= 1.f; }
			if (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) { Axis.X += 1.f; }
			if (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left)) { Axis.X -= 1.f; }
		}
		const UDCUISubsystem* UI = UDCUISubsystem::Get(this);
		if (bEdgeScroll && bHaveMouse && !bRmbDragging && !UDCDevTools::IsReel() && (!UI || !UI->IsAnyPanelOpen()) && IsGameWindowFocused(GetWorld()))
		{
			int32 VX = 0, VY = 0;
			GetViewportSize(VX, VY);
			constexpr float Edge = 8.f;
			if (VX > 0 && VY > 0 && Mouse.X >= 0.f && Mouse.Y >= 0.f && Mouse.X < VX && Mouse.Y < VY)
			{
				if (Mouse.X < Edge) { Axis.X -= 1.f; } else if (Mouse.X > VX - Edge) { Axis.X += 1.f; }
				if (Mouse.Y < Edge) { Axis.Y += 1.f; } else if (Mouse.Y > VY - Edge) { Axis.Y -= 1.f; }
			}
		}
		if (!Axis.IsZero()) { CameraRig->PanAxis(Axis, DeltaTime); }
	}
}
