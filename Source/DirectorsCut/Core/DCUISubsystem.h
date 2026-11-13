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
	/** Open the crew planner for a specific job. */
	void OpenCrewPlanner(int32 JobId);
	void ClosePanel();
	EDCPanel GetOpenPanel() const;
	bool IsAnyPanelOpen() const;
	/** True while a modal flow (title, dailies, perk draft, game over, credits) should block world clicks. */
	bool IsWorldInputBlocked() const;
	/** Esc: back out of the current panel (or open settings when nothing is open). */
	void HandleBack();
	/** Space: "Roll Camera" if any crew is committed. */
	void HandleRollCamera();

	/** Short floating toast at the top of the screen. */
	void ShowToast(const FString& Message, bool bIsWarning = false);

	FDCOnPanelChanged OnPanelChanged;

private:
	// ═══ Agent C owns everything below this line ═══
	EDCPanel OpenPanelType = EDCPanel::None;
	TWeakObjectPtr<APlayerController> OwningPC;
};
