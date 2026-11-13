// Director's Cut — UI subsystem. R0 PLACEHOLDER (lead): a one-line debug HUD. Agent C replaces this with the v2 modern UI.
#include "Core/DCUISubsystem.h"
#include "Core/DCRunSubsystem.h"
#include "Core/DCDevTools.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "DirectorsCut.h"

UDCUISubsystem* UDCUISubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = (WorldContext && GEngine) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	return LP ? LP->GetSubsystem<UDCUISubsystem>() : nullptr;
}

void UDCUISubsystem::Deinitialize() { RemoveRoot(); Super::Deinitialize(); }

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
	RemoveRoot();
	UWorld* World = PC->GetWorld();
	UGameViewportClient* VP = World ? World->GetGameViewport() : nullptr;
	if (!VP && GEngine) { VP = GEngine->GameViewport; }
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(PC);
	if (Run && !Run->HasActiveRun()) { Run->StartNewRun(UDCDevTools::GetCmdSeed()); }
	if (!VP) { return; }
	TWeakObjectPtr<APlayerController> WeakPC = PC;
	Root = SNew(SBox).Padding(FMargin(16.f)).HAlign(HAlign_Left).VAlign(VAlign_Top)
	[
		SNew(STextBlock).Text_Lambda([WeakPC]()
		{
			const UDCRunSubsystem* R = UDCRunSubsystem::Get(WeakPC.Get());
			if (!R || !R->HasActiveRun()) { return FText::FromString(TEXT("—")); }
			const float H = R->GetHourOfDay();
			return FText::FromString(FString::Printf(TEXT("Day %d  %02d:%02d  $%d  missions %d/%d  [R0 placeholder UI]"),
				R->GetDay(), (int32)H, (int32)(FMath::Frac(H) * 60.f), R->GetCash(), R->GetActiveMissionCount(), R->GetMaxActiveMissions()));
		})
	];
	RootViewport = VP;
	VP->AddViewportWidgetContent(Root.ToSharedRef(), 10);
}

void UDCUISubsystem::OpenPanel(EDCPanel Panel) { OpenPanelType = Panel; OnPanelChanged.Broadcast(Panel); }
void UDCUISubsystem::OpenMissionPlanner(int32) { OpenPanel(EDCPanel::MissionPlanner); }
void UDCUISubsystem::ClosePanel() { OpenPanel(EDCPanel::None); }
EDCPanel UDCUISubsystem::GetOpenPanel() const { return OpenPanelType; }
bool UDCUISubsystem::IsAnyPanelOpen() const { return OpenPanelType != EDCPanel::None; }
bool UDCUISubsystem::IsWorldInputBlocked() const { return false; }
void UDCUISubsystem::HandleBack() { ClosePanel(); }
void UDCUISubsystem::ShowToast(const FString& Message, FName, bool) { UE_LOG(LogDC, Log, TEXT("[Toast] %s"), *Message); }
