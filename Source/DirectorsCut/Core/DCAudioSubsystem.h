// Director's Cut — audio entry points (LEAD-OWNED contract).
//
// PUBLIC section frozen during Wave 1. Agent H (Audio) owns the implementation (Audio/DCAudioSubsystem.cpp)
// and MAY change the PRIVATE section and add files under Audio/.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/DCTypes.h"
#include "DCAudioSubsystem.generated.h"

// Canonical SFX names. Anyone may call UDCAudioSubsystem::Play(DCSfx::Click).
namespace DCSfx
{
	inline const FName Click        = TEXT("Click");         // generic UI click
	inline const FName Hover        = TEXT("Hover");         // soft tick
	inline const FName PanelOpen    = TEXT("PanelOpen");     // paper slide
	inline const FName PanelClose   = TEXT("PanelClose");
	inline const FName Typewriter   = TEXT("Typewriter");    // typewriter clack
	inline const FName Stamp        = TEXT("Stamp");         // rubber stamp thunk
	inline const FName Cash         = TEXT("Cash");          // cash register
	inline const FName CounterTick  = TEXT("CounterTick");   // number ticker
	inline const FName Clapper      = TEXT("Clapper");       // clapperboard snap
	inline const FName DiceRoll     = TEXT("DiceRoll");      // film sprocket / dice roll
	inline const FName Commit       = TEXT("Commit");        // plan committed
	inline const FName Error        = TEXT("Error");         // can't do that
	inline const FName GarageDoor   = TEXT("GarageDoor");
	inline const FName CarDepart    = TEXT("CarDepart");
	// Stingers
	inline const FName StingWrap    = TEXT("StingWrap");     // clean/messy success
	inline const FName StingCut     = TEXT("StingCut");      // failed/busted
	inline const FName StingLevelUp = TEXT("StingLevelUp");
	inline const FName StingRoom    = TEXT("StingRoom");     // room built
	inline const FName StingPerk    = TEXT("StingPerk");
}

UCLASS()
class DIRECTORSCUT_API UDCAudioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UDCAudioSubsystem* Get(const UObject* WorldContext);
	/** Convenience: Get(WorldContext)->PlaySFX(Name), null-safe. */
	static void Play(const UObject* WorldContext, FName SfxName);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void PlaySFX(FName SfxName, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f);
	void SetMusicState(EDCMusicState State);
	EDCMusicState GetMusicState() const;

	void SetVolume(EDCVolumeChannel Channel, float Volume01);
	float GetVolume(EDCVolumeChannel Channel) const;

private:
	// ═══ Agent H owns everything below this line ═══
	EDCMusicState MusicState = EDCMusicState::None;
	float Volumes[4] = { 1.f, 0.8f, 1.f, 0.7f };
};
