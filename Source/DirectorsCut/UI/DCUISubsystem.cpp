// Director's Cut — UI subsystem: hosts the SDCRoot Slate widget in the game viewport (Agent C).
#include "Core/DCUISubsystem.h"
#include "Core/DCRunSubsystem.h"
#include "Core/DCDevTools.h"
#include "UI/SDCRoot.h"
#include "UI/SDCLoading.h"
#include "Containers/Ticker.h"
#include "UI/DCUIStyle.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Input/Events.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "DirectorsCut.h"

/** Panel hotkeys (C, G, R, B, N, M) and game speed (Space, 1, 2, 3). Observes only: never consumes, so the game still sees every key. */
class FDCHotkeyProcessor : public IInputProcessor
{
public:
	FDCHotkeyProcessor(TWeakPtr<SDCRoot> InRoot, TWeakPtr<SDCLoading> InLoading) : Root(InRoot), Loading(InLoading) {}
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.IsRepeat() || InKeyEvent.IsControlDown() || InKeyEvent.IsCommandDown() || InKeyEvent.IsAltDown()) { return false; }
		if (TSharedPtr<SDCLoading> L = Loading.Pin(); L && L->IsBlocking()) { return false; }
		if (TSharedPtr<SDCRoot> R = Root.Pin()) { R->HandleHotkey(InKeyEvent.GetKey()); }
		return false;
	}
	virtual const TCHAR* GetDebugName() const override { return TEXT("DCHotkeys"); }
private:
	TWeakPtr<SDCRoot> Root;
	TWeakPtr<SDCLoading> Loading;
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

void UDCUISubsystem::RemoveLoading()
{
	if (Loading.IsValid())
	{
		if (UGameViewportClient* VP = RootViewport.Get()) { VP->RemoveViewportWidgetContent(Loading.ToSharedRef()); }
		Loading.Reset();
	}
}

bool UDCUISubsystem::IsLoading() const { return Loading.IsValid() && Loading->IsBlocking(); }

void UDCUISubsystem::RemoveRoot()
{
	RemoveLoading();
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
	Root->ApplyPlayerSettings(false);
	RootViewport = VP;
	// Dev screenshots: -DCUIView=options.graphics,slots.load,... (one entry per -DCShots panel open; "-" = none)
	DevViews.Reset();
	FString Views;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCUIView="), Views, false)) { Views.ParseIntoArray(DevViews, TEXT(","), true); }
	VP->AddViewportWidgetContent(Root.ToSharedRef(), 10);

	// Boot loading screen over everything until shaders, loads and texture streaming have settled
	// (-DCNoLoading skips it). The world keeps rendering underneath so streaming sees what it needs.
	if (!FParse::Param(FCommandLine::Get(), TEXT("DCNoLoading")))
	{
		TWeakObjectPtr<UDCUISubsystem> WeakThis(this);
		Loading = SNew(SDCLoading)
			.OnReveal_Lambda([WeakThis]()
			{
				if (WeakThis.IsValid() && WeakThis->Root.IsValid()) { WeakThis->Root->ReplayTitleIntro(); }
			})
			.OnFinished_Lambda([WeakThis]()
			{
				// Not from inside the widget's own tick: remove it on the next core tick.
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
				{
					if (WeakThis.IsValid()) { WeakThis->RemoveLoading(); }
					return false;
				}));
			});
		VP->AddViewportWidgetContent(Loading.ToSharedRef(), 100);
	}

	if (FSlateApplication::IsInitialized())
	{
		Hotkeys = MakeShared<FDCHotkeyProcessor>(Root, Loading);
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
	if (Root.IsValid())
	{
		Root->Open(Panel);
		if (DevViews.Num()) { const FString V = DevViews[0]; DevViews.RemoveAt(0); Root->SetDevView(V); }
	}
	else { OpenPanelType = Panel; OnPanelChanged.Broadcast(Panel); }
}

void UDCUISubsystem::OpenMissionPlanner(int32 MissionId)
{
	if (Root.IsValid()) { Root->OpenPlanner(MissionId); }
	else { OpenPanel(EDCPanel::MissionPlanner); }
}

void UDCUISubsystem::ClosePanel()
{
	if (Root.IsValid()) { Root->Close(false); }
	else { OpenPanelType = EDCPanel::None; OnPanelChanged.Broadcast(EDCPanel::None); }
}

void UDCUISubsystem::RequestRoomPlacement(EDCRoomType Room)
{
	ClosePanel();
	OnRoomPlacementRequested.Broadcast(Room);
}

bool UDCUISubsystem::DevAction(const FString& Verb, const FString& Arg) { return Root.IsValid() && Root->DevAction(Verb, Arg); }

EDCPanel UDCUISubsystem::GetOpenPanel() const { return OpenPanelType; }
bool UDCUISubsystem::IsAnyPanelOpen() const { return OpenPanelType != EDCPanel::None; }
bool UDCUISubsystem::IsWorldInputBlocked() const { return IsLoading() || (Root.IsValid() && Root->IsWorldBlocked()); }
void UDCUISubsystem::HandleBack() { if (Root.IsValid()) { Root->Back(); } }

void UDCUISubsystem::ShowToast(const FString& Message, FName Icon, bool bIsWarning)
{
	if (Root.IsValid()) { Root->Toast(Message, Icon, bIsWarning); return; }
	UE_LOG(LogDC, Log, TEXT("[Toast] %s"), *Message);
}
