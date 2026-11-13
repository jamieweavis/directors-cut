// Wave 0 STUB of UDCUISubsystem: a single debug text overlay. Agent C replaces this.
#include "Core/DCUISubsystem.h"
#include "Core/DCRunSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "DirectorsCut.h"

UDCUISubsystem* UDCUISubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	return LP ? LP->GetSubsystem<UDCUISubsystem>() : nullptr;
}

void UDCUISubsystem::InitializeUI(APlayerController* PC)
{
	OwningPC = PC;
	if (UDCRunSubsystem* Run = UDCRunSubsystem::Get(PC))
	{
		if (!Run->HasActiveRun()) { Run->StartNewRun(0); }
	}
	ShowToast(TEXT("Director's Cut — Wave 0 stub UI"));
}

void UDCUISubsystem::OpenPanel(EDCPanel Panel)
{
	OpenPanelType = Panel;
	ShowToast(FString::Printf(TEXT("OpenPanel %d"), (int32)Panel));
	OnPanelChanged.Broadcast(Panel);
}

void UDCUISubsystem::OpenCrewPlanner(int32 JobId) { OpenPanel(EDCPanel::CrewPlanner); }
void UDCUISubsystem::ClosePanel() { OpenPanelType = EDCPanel::None; OnPanelChanged.Broadcast(EDCPanel::None); }
EDCPanel UDCUISubsystem::GetOpenPanel() const { return OpenPanelType; }
bool UDCUISubsystem::IsAnyPanelOpen() const { return OpenPanelType != EDCPanel::None; }
bool UDCUISubsystem::IsWorldInputBlocked() const { return false; }
void UDCUISubsystem::HandleBack() { ClosePanel(); }
void UDCUISubsystem::HandleRollCamera()
{
	if (UDCRunSubsystem* Run = UDCRunSubsystem::Get(OwningPC.Get()))
	{
		if (Run->GetCommittedPlans().Num() > 0) { Run->EndNight(); Run->FinishDailies(); }
	}
}

void UDCUISubsystem::ShowToast(const FString& Message, bool bIsWarning)
{
	UE_LOG(LogDC, Log, TEXT("[Toast] %s"), *Message);
	if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 4.f, bIsWarning ? FColor::Red : FColor::Yellow, Message); }
}
