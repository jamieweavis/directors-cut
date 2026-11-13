// Director's Cut — audio entry points (LEAD-OWNED contract).
//
// PUBLIC section frozen during Wave 1. Agent H (Audio) owns the implementation (Audio/DCAudioSubsystem.cpp)
// and MAY change the PRIVATE section and add files under Audio/.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/DCTypes.h"
#include "DCAudioSubsystem.generated.h"

// Canonical SFX names. Anyone may call UDCAudioSubsystem::Play(DCSfx::Click). v2: no music anywhere in the game.
namespace DCSfx
{
	inline const FName Click           = TEXT("Click");           // crisp UI click
	inline const FName Hover           = TEXT("Hover");           // very soft tick
	inline const FName PanelOpen       = TEXT("PanelOpen");       // short soft whoosh
	inline const FName PanelClose      = TEXT("PanelClose");
	inline const FName Confirm         = TEXT("Confirm");         // positive confirm (hire, buy, start)
	inline const FName Error           = TEXT("Error");           // can't do that
	inline const FName Cash            = TEXT("Cash");            // money in (bill counter)
	inline const FName SlotFill        = TEXT("SlotFill");        // crew dropped into a mission slot
	inline const FName MissionStart    = TEXT("MissionStart");    // radio chirp + engine start
	inline const FName MissionComplete = TEXT("MissionComplete"); // phone notification ping
	inline const FName Success         = TEXT("Success");         // short positive sting (not musical)
	inline const FName Fail            = TEXT("Fail");            // short negative thud
	inline const FName Busted          = TEXT("Busted");          // distant siren whoop
	inline const FName LevelUp         = TEXT("LevelUp");
	inline const FName BuildStart      = TEXT("BuildStart");      // power tools
	inline const FName RoomBuilt       = TEXT("RoomBuilt");       // breaker switch + lights hum on
	inline const FName Perk            = TEXT("Perk");
	inline const FName Payday          = TEXT("Payday");          // cash out at dawn
	inline const FName GarageDoor      = TEXT("GarageDoor");
	inline const FName CarDepart       = TEXT("CarDepart");
	inline const FName CarArrive       = TEXT("CarArrive");
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

	void SetVolume(EDCVolumeChannel Channel, float Volume01);
	float GetVolume(EDCVolumeChannel Channel) const;

private:
	// ═══ Agent H owns everything below this line ═══
	float Volumes[3] = { 1.f, 1.f, 0.7f };
	UPROPERTY(Transient) TMap<FName, TObjectPtr<class USoundBase>> SoundCache;
	class USoundBase* LoadSound(FName SfxName);
};
