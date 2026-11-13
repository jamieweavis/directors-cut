// Director's Cut — keeps the diorama in sync with UDCRunSubsystem in real time: room states (build timers, reveals),
// camera bounds, station enabling + badges, crew pawns (idle life tied to the clock, leaving for / returning from
// missions, hires, dismissals) and the doors. Spawned by ADCPlayerController. Agent E owns.
//
// Pawns never path through walls: rooms form a graph (adjacent rooms, doorway found by tracing the shared wall), and
// idle goals are only ever spawn-zone points or the floor in front of a station, inside usable rooms.
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
class ADCStation;

UCLASS()
class DIRECTORSCUT_API ADCWorldDirector : public AActor
{
	GENERATED_BODY()
public:
	ADCWorldDirector();
	void Initialize(ADCPlayerController* InPC);
	virtual void Tick(float DeltaSeconds) override;

	/** The door crews use for missions: the Garage's if built, else the street door. */
	ADCGarageDoor* GetMissionDoor() const;
	/** The street door (hires come in, dismissed crew leave). */
	ADCGarageDoor* GetStreetDoor() const;
	ADCRoomModule* FindRoom(EDCRoomType Type) const;
	/** Room whose floor contains P (2D), or null. */
	ADCRoomModule* RoomAt(const FVector& P) const;
	ADCCriminalPawn* FindPawn(int32 CriminalId) const;
	int32 GetPawnCount() const { return Pawns.Num(); }

	/** Floor-level route from From to To through doorways (excludes From, ends with To). */
	void PlanPath(const FVector& From, const FVector& To, TArray<FVector>& Out);
	/** A free floor spot in a usable room for P to appear / head to. */
	FVector PickSpawnPoint(ADCCriminalPawn* P);
	/** Called by an idle pawn that wants something to do. */
	void AssignIdleGoal(ADCCriminalPawn* P);

	static constexpr int32 MaxPawns = 12;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	TWeakObjectPtr<ADCPlayerController> PC;
	UPROPERTY() TArray<TObjectPtr<ADCCriminalPawn>> Pawns;
	TArray<TWeakObjectPtr<ADCRoomModule>> Rooms;
	TArray<TWeakObjectPtr<ADCCriminalSpawnZone>> Zones;
	TArray<TWeakObjectPtr<ADCGarageDoor>> Doors;
	TArray<TWeakObjectPtr<ADCStation>> Stations;
	TArray<EDCRoomType> RevealQueue;
	TWeakObjectPtr<ADCRoomModule> RevealingRoom;
	TMap<TWeakObjectPtr<ADCGarageDoor>, float> DoorIdleTime;
	FRandomStream Rng;

	// Room graph
	struct FPortal { int32 A = 0; int32 B = 0; FVector InA = FVector::ZeroVector; FVector InB = FVector::ZeroVector; };
	TArray<FPortal> Portals;
	bool bNavBuilt = false;
	void BuildNav();
	int32 RoomIndexAt(const FVector& P) const;
	bool IsRoomUsable(const ADCRoomModule* R) const;
	bool IsRoomUsable(EDCRoomType Type) const { return IsRoomUsable(FindRoom(Type)); }

	// Idle targets
	struct FAnchor { FVector Point = FVector::ZeroVector; float Yaw = 0.f; bool bHasYaw = false; bool bStation = false; };
	void GetAnchors(const ADCRoomModule* Room, TArray<FAnchor>& Out);
	bool IsSpotTaken(const FVector& P, const ADCCriminalPawn* Except) const;
	bool IsSleeper(const ADCCriminalPawn* P) const;
	TArray<FVector> SleepSpots;
	float SleepYaw = 0.f;
	TArray<int32> SleepOwner; // criminal id per spot, INDEX_NONE = free
	void BuildSleepSpots();
	int32 AcquireSleepSlot(ADCCriminalPawn* P);
	bool TrySendToBed(ADCCriminalPawn* P, bool bInstant);

	// Badges
	TSet<int32> SeenMissionIds;
	EDCPanel LastPanel = EDCPanel::None;

	EDCGamePhase LastPhase = EDCGamePhase::Title;
	bool bHadRun = false;
	double LastRunTime = 0.0;
	int32 LastSeed = 0;
	bool bBoundsInitialized = false;
	bool bInitialized = false;
	bool bWasNight = false;
	float SlowTick = 0.f;

	// Subsystem bindings
	FDelegateHandle HState, HPhase, HStart, HDone, HRoom, HLevel, HHired, HPanel;

	void GatherWorld();
	void OnRunStateChanged();
	void OnPhaseChanged(EDCGamePhase Phase);
	void OnMissionStarted(const FDCMission& Mission);
	void OnMissionCompleted(const FDCMissionReport& Report);
	void OnRoomChanged(const FDCRoom& Room);
	void OnCriminalLevelUp(int32 CriminalId, int32 NewLevel);
	void OnCriminalHired(const FDCCriminal& C);
	void OnPanelChanged(EDCPanel Panel);

	void ResetForNewRun();
	void SyncRooms();
	void SyncPawns();
	void UpdateBounds();
	void UpdateDoors(float Dt);
	void UpdateStations();
	void UpdateBuildTimers();
	void UpdateClockLife();
	ADCCriminalPawn* SpawnPawn(const FDCCriminal& C, bool bWalkIn);
	bool IsDetectingNewRun();

	// Dev: -DCWorldTest runs a scripted realtime choreography test with screenshots (see TickWorldTest).
	bool bWorldTest = false;
	double TestTime = 0.0;
	double StepAt = 0.0;
	int32 TestStep = 0;
	FString TestShotDir;
	void TickWorldTest(float Dt);
	void TestShot(const FString& Name);
	void LogPawns(const TCHAR* Tag) const;
};
