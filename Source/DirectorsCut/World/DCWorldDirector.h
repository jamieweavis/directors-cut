// Director's Cut — keeps the diorama in sync with UDCRunSubsystem: room states & reveals, camera bounds,
// criminal pawns (spawn / wander / depart / return / leave), and the doors. Spawned by ADCPlayerController. Agent E owns.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DCTypes.h"
#include "DCWorldDirector.generated.h"

class ADCPlayerController;
class ADCRoomModule;
class ADCCriminalPawn;
class ADCCriminalSpawnZone;
class ADCGarageDoor;

UCLASS()
class DIRECTORSCUT_API ADCWorldDirector : public AActor
{
	GENERATED_BODY()
public:
	ADCWorldDirector();
	void Initialize(ADCPlayerController* InPC);
	virtual void Tick(float DeltaSeconds) override;

	/** The door crews use tonight: the Garage's if built, else the Office street door. */
	ADCGarageDoor* GetActiveDoor() const;
	/** Where pawns hang out: the Lounge if built, else the Office. */
	ADCCriminalSpawnZone* GetActiveZone() const;
	ADCRoomModule* FindRoom(EDCRoomType Type) const;
	ADCCriminalPawn* FindPawn(int32 CriminalId) const;
	int32 GetPawnCount() const { return Pawns.Num(); }

	static constexpr int32 MaxPawns = 10;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	TWeakObjectPtr<ADCPlayerController> PC;
	UPROPERTY() TArray<TObjectPtr<ADCCriminalPawn>> Pawns;
	TArray<TWeakObjectPtr<ADCRoomModule>> Rooms;
	TArray<TWeakObjectPtr<ADCCriminalSpawnZone>> Zones;
	TArray<TWeakObjectPtr<ADCGarageDoor>> Doors;
	TArray<EDCRoomType> RevealQueue;
	TWeakObjectPtr<ADCRoomModule> RevealingRoom;
	TSet<int32> PendingHires;
	TMap<TWeakObjectPtr<ADCGarageDoor>, float> DoorIdleTime;

	EDCGamePhase LastPhase = EDCGamePhase::Title;
	bool bHadRun = false;
	int32 LastNight = 0;
	int32 LastSeed = 0;
	bool bBoundsInitialized = false;
	bool bInitialized = false;

	// Subsystem bindings
	FDelegateHandle HState, HPhase, HNight, HRoom, HLevel, HHired;

	void GatherWorld();
	void OnRunStateChanged();
	void OnPhaseChanged(EDCGamePhase Phase);
	void OnNightResolved(const FDCNightReport& Report);
	void OnRoomChanged(const FDCRoom& Room);
	void OnCriminalLevelUp(int32 CriminalId, int32 NewLevel);
	void OnCriminalHired(const FDCCriminal& C);

	void ResetForNewRun();
	void SyncRooms(bool bAllowReveal);
	void SyncPawns();
	void UpdateBounds();
	void UpdateDoors(float Dt);
	ADCCriminalPawn* SpawnPawn(const FDCCriminal& C, bool bWalkIn);
	bool IsDetectingNewRun();

	// Dev: -DCWorldTest runs a scripted choreography test with screenshots (see TickWorldTest).
	bool bWorldTest = false;
	double TestTime = 0.0;
	int32 TestStep = 0;
	FString TestShotDir;
	void TickWorldTest(float Dt);
	void TestShot(const FString& Name);
};
