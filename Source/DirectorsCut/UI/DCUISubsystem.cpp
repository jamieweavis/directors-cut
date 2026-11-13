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
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Input/Events.h"
#include "DirectorsCut.h"

/** Panel hotkeys (C, G, R, B, N, M). Observes only: never consumes, so the game still sees every key. */
class FDCHotkeyProcessor : public IInputProcessor
{
public:
	explicit FDCHotkeyProcessor(TWeakPtr<SDCRoot> InRoot) : Root(InRoot) {}
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.IsRepeat() || InKeyEvent.IsControlDown() || InKeyEvent.IsCommandDown() || InKeyEvent.IsAltDown()) { return false; }
		if (TSharedPtr<SDCRoot> R = Root.Pin()) { R->HandleHotkey(InKeyEvent.GetKey()); }
		return false;
	}
	virtual const TCHAR* GetDebugName() const override { return TEXT("DCHotkeys"); }
private:
	TWeakPtr<SDCRoot> Root;
};

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
	if (Hotkeys.IsValid())
	{
		if (FSlateApplication::IsInitialized()) { FSlateApplication::Get().UnregisterInputPreProcessor(Hotkeys); }
		Hotkeys.Reset();
	}
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

	UDCRunSubsystem* Run = UDCRunSubsystem::Get(PC);
	const int32 Seed = UDCDevTools::GetCmdSeed();
	if (!VP)
	{
		UE_LOG(LogDC, Warning, TEXT("[UI] No game viewport; UI not created (headless?)"));
		if (Seed != 0 && Run && !Run->HasActiveRun()) { Run->StartNewRun(Seed); }
		return;
	}

	Root = SNew(SDCRoot).Owner(this);
	RootViewport = VP;
	VP->AddViewportWidgetContent(Root.ToSharedRef(), 10);
	if (FSlateApplication::IsInitialized())
	{
		Hotkeys = MakeShared<FDCHotkeyProcessor>(Root);
		FSlateApplication::Get().RegisterInputPreProcessor(Hotkeys);
	}

	if (Seed != 0 && Run)
	{
		// Dev tools / screenshots: skip the title and start straight away.
		if (!Run->HasActiveRun()) { Run->StartNewRun(Seed); }
	}
	else if (!Run || !Run->HasActiveRun())
	{
		Root->Open(EDCPanel::Title);
	}
}

void UDCUISubsystem::OpenPanel(EDCPanel Panel)
{
	if (Root.IsValid()) { Root->Open(Panel); }
	else { OpenPanelType = Panel; OnPanelChanged.Broadcast(Panel); }
}

void UDCUISubsystem::OpenMissionPlanner(int32 MissionId)
{
	if (Root.IsValid()) { Root->OpenPlanner(MissionId); }
	else { OpenPanel(EDCPanel::MissionPlanner); }
}

void UDCUISubsystem::ClosePanel()
{
	if (Root.IsValid()) { Root->Close(true); }
	else { OpenPanelType = EDCPanel::None; OnPanelChanged.Broadcast(EDCPanel::None); }
}

EDCPanel UDCUISubsystem::GetOpenPanel() const { return OpenPanelType; }
bool UDCUISubsystem::IsAnyPanelOpen() const { return OpenPanelType != EDCPanel::None; }
bool UDCUISubsystem::IsWorldInputBlocked() const { return Root.IsValid() && Root->IsWorldBlocked(); }
void UDCUISubsystem::HandleBack() { if (Root.IsValid()) { Root->Back(); } }

void UDCUISubsystem::ShowToast(const FString& Message, FName Icon, bool bIsWarning)
{
	if (Root.IsValid()) { Root->Toast(Message, Icon, bIsWarning); return; }
	UE_LOG(LogDC, Log, TEXT("[Toast] %s"), *Message);
}
