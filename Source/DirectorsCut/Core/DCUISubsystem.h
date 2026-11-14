// Director's Cut — UI entry points (LEAD-OWNED contract).
//
// PUBLIC section frozen during Wave 1. Agent C (UI) owns the implementation (UI/DCUISubsystem.cpp)
// and MAY change the PRIVATE section and add any files under UI/.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Core/DCTypes.h"
#include "DCUISubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnPanelChanged, EDCPanel /*NowOpen, None = closed*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnRoomPlacementRequested, EDCRoomType);

UCLASS()
class DIRECTORSCUT_API UDCUISubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/** Resolves the first local player's UI subsystem. */
	static UDCUISubsystem* Get(const UObject* WorldContext);

	/** Called once by the player controller in BeginPlay. Builds the root HUD and shows the Title panel. */
	void InitializeUI(APlayerController* PC);

	/** Open a panel (closes the current one). Stations call this when clicked. */
	void OpenPanel(EDCPanel Panel);
	/** Open the mission planner for a specific mission. */
	void OpenMissionPlanner(int32 MissionId);
	void ClosePanel();
	EDCPanel GetOpenPanel() const;
	bool IsAnyPanelOpen() const;
	/** True while a modal flow (title, pause, perk draft, game over, victory) should block world clicks. */
	bool IsWorldInputBlocked() const;
	/** Esc: back out of the current panel (or open Pause when nothing is open). */
	void HandleBack();

	/** Short floating toast (icon + a few words). Icon is a Lucide name. */
	void ShowToast(const FString& Message, FName Icon = NAME_None, bool bIsWarning = false);

	FDCOnPanelChanged OnPanelChanged;

	/** "Build" on a room that isn't built yet: closes the panel and asks the world to start grid placement
	 *  (the player controller picks a cell, then calls UDCRunSubsystem::BuildRoom with it). */
	void RequestRoomPlacement(EDCRoomType Room);
	FDCOnRoomPlacementRequested OnRoomPlacementRequested;

private:
	// ═══ Agent C owns everything below this line ═══
	friend class SDCRoot;
	virtual void Deinitialize() override;
	void RemoveRoot();

	EDCPanel OpenPanelType = EDCPanel::None;
	TWeakObjectPtr<APlayerController> OwningPC;
	TSharedPtr<class SDCRoot> Root;
	TWeakObjectPtr<class UGameViewportClient> RootViewport;
	TSharedPtr<class IInputProcessor> Hotkeys;
	TArray<FString> DevViews;           // -DCUIView queue (dev screenshots)
};
