// Director's Cut — UI subsystem: hosts the SDCRoot Slate widget in the game viewport (Agent C).
#include "Core/DCUISubsystem.h"
#include "Core/DCRunSubsystem.h"
#include "Core/DCDevTools.h"
#include "UI/SDCRoot.h"
#include "UI/DCUIStyle.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "DirectorsCut.h"

UDCUISubsystem* UDCUISubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = (WorldContext && GEngine) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	return LP ? LP->GetSubsystem<UDCUISubsystem>() : nullptr;
}

void UDCUISubsystem::Deinitialize()
{
	RemoveRoot();
	Super::Deinitialize();
}

void UDCUISubsystem::RemoveRoot()
{
	if (Root.IsValid())
	{
		if (UGameViewportClient* VP = RootViewport.Get()) { VP->RemoveViewportWidgetContent(Root.ToSharedRef()); }
		Root.Reset();
	}
	OpenPanelType = EDCPanel::None;
}

void UDCUISubsystem::InitializeUI(APlayerController* PC)
{
	OwningPC = PC;
	if (!PC) { return; }
	DCUI::SetContext(PC->GetGameInstance() ? (UObject*)PC->GetGameInstance() : (UObject*)PC);

	RemoveRoot();
	UWorld* World = PC->GetWorld();
	UGameViewportClient* VP = World ? World->GetGameViewport() : nullptr;
	if (!VP && GEngine) { VP = GEngine->GameViewport; }
	if (!VP)
	{
		UE_LOG(LogDC, Warning, TEXT("[UI] No game viewport; UI not created (headless?)"));
		return;
	}
	Root = SNew(SDCRoot).Owner(this);
	RootViewport = VP;
	VP->AddViewportWidgetContent(Root.ToSharedRef(), 10);

	UDCRunSubsystem* Run = UDCRunSubsystem::Get(PC);
	const int32 Seed = UDCDevTools::GetCmdSeed();
	if (Seed != 0 && Run)
	{
		if (!Run->HasActiveRun()) { Run->StartNewRun(Seed); }
	}
	else
	{
		Root->ShowStartScreen();
	}
}

void UDCUISubsystem::OpenPanel(EDCPanel Panel)
{
	if (Root.IsValid()) { Root->Open(Panel); }
	else { OpenPanelType = Panel; OnPanelChanged.Broadcast(Panel); }
}

void UDCUISubsystem::OpenCrewPlanner(int32 JobId)
{
	if (Root.IsValid()) { Root->OpenPlanner(JobId); }
	else { OpenPanel(EDCPanel::CrewPlanner); }
}

void UDCUISubsystem::ClosePanel()
{
	if (Root.IsValid()) { Root->Close(false); }
	else { OpenPanelType = EDCPanel::None; OnPanelChanged.Broadcast(EDCPanel::None); }
}

EDCPanel UDCUISubsystem::GetOpenPanel() const { return OpenPanelType; }
bool UDCUISubsystem::IsAnyPanelOpen() const { return OpenPanelType != EDCPanel::None; }
bool UDCUISubsystem::IsWorldInputBlocked() const { return Root.IsValid() && Root->IsWorldBlocked(); }
void UDCUISubsystem::HandleBack() { if (Root.IsValid()) { Root->Back(); } }

void UDCUISubsystem::HandleRollCamera()
{
	if (Root.IsValid()) { Root->RollCamera(false); return; }
	// No UI (headless): behave like the old stub.
	if (UDCRunSubsystem* Run = UDCRunSubsystem::Get(OwningPC.Get()))
	{
		if (Run->GetPhase() == EDCGamePhase::Planning && Run->GetCommittedPlans().Num() > 0) { Run->EndNight(); Run->FinishDailies(); }
	}
}

void UDCUISubsystem::ShowToast(const FString& Message, bool bIsWarning)
{
	if (Root.IsValid()) { Root->Toast(Message, bIsWarning); return; }
	UE_LOG(LogDC, Log, TEXT("[Toast] %s"), *Message);
}
