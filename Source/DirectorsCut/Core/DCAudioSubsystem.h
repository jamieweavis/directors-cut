// Director's Cut — audio entry points (LEAD-OWNED contract).
//
// PUBLIC section frozen during Wave 1. Agent H (Audio) owns the implementation (Audio/DCAudioSubsystem.cpp)
// and MAY change the PRIVATE section and add files under Audio/.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/DCTypes.h"
#include "DCAudioSubsystem.generated.h"

// Canonical SFX names. Anyone may call UDCAudioSubsystem::Play(DCSfx::Click). The lounge score runs by itself
// (UDCAudioSubsystem::UpdateMusic): layered stems that follow the clock and the run's progress.
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
	friend struct FDCAudioTestAccess; // automation tests (Audio/DCAudioTests.cpp)

	float Volumes[4] = { 1.f, 1.f, 0.2f, 0.6f };

	UPROPERTY(Transient) TMap<FName, TObjectPtr<class USoundBase>> SoundCache;
	/** Looping ambience beds (DCAudio::EBed order), living in the game instance's current world. */
	UPROPERTY(Transient) TArray<TObjectPtr<class UAudioComponent>> BedComps;
	/** The playing music set's stems (DCAudio::EMusicStem order), all started in the same frame so they stay locked. */
	UPROPERTY(Transient) TArray<TObjectPtr<class UAudioComponent>> MusicComps;
	/** The previous set's stems while they fade out under the new one. */
	UPROPERTY(Transient) TArray<TObjectPtr<class UAudioComponent>> MusicFadeComps;

	/** Non-UObject runtime state (ticker, timers, event queue); defined in Audio/DCAudioSubsystem.cpp. */
	TSharedPtr<struct FDCAudioRuntime> RT;

	bool TickAudio(float DeltaTime);
	UWorld* GetAudioWorld() const;
	bool EnsureComponents();
	void TearDownComponents();
	void UpdateAmbience(float DeltaTime, bool bSnap);
	void TickAmbienceOneShots();
	void UpdateMusic(float DeltaTime);
	void StartMusicSet(int32 Set, bool bTitle, int32 Chapter);
	void PlanMusicPhrase(bool bTitle, int32 Chapter);
	void ApplyMusicVolumes(float DeltaTime);
	void StopMusic(bool bFadeOut);
	void TickQueue();
	/** Queues an SFX after Delay real seconds; dropped if the same SFX is already queued close by (burst throttle). */
	void Schedule(FName SfxName, float Delay, float VolumeMultiplier = 1.f);
	class USoundBase* LoadSound(const FSoftObjectPath& Path);
	class UAudioComponent* MakeComponent(class USoundBase* Sound);
	void LoadVolumes();
	void SaveVolumes();

	void HandleMissionStarted(const struct FDCMission& Mission);
	void HandleMissionCompleted(const struct FDCMissionReport& Report);
	void HandleLevelUp(int32 CriminalId, int32 NewLevel);
	void HandleRoomChanged(const struct FDCRoom& Room);
	void HandlePerkOffered(const TArray<struct FDCPerk>& Perks);
	void HandlePayday(int32 Day, int32 Amount);
	void HandlePhaseChanged(EDCGamePhase Phase);
};
