// Director's Cut — keeps the diorama in sync with UDCRunSubsystem in real time: room states (build timers, reveals),
// camera bounds, station enabling + badges, crew pawns (idle life tied to the clock, leaving for / returning from
// missions, hires, dismissals) and the doors. Spawned by ADCPlayerController. Agent E owns.
//
// Pawns never path through walls: built rooms form a graph (grid neighbours, doorway at the shared edge's midpoint)
// plus one OUTSIDE region reached through each room's open (collapsed) -X/-Y sides. Outdoors, pawns walk a
// visibility graph around built/under-construction wings and parked cars. Idle goals are only ever spawn-zone points
// or the floor in front of a station, inside usable rooms.
//
// Cars: one per mission slot (UDCRunSubsystem::GetMaxActiveMissions, max DCSite::MaxParkingSlots), parked in the yard.
// A started mission takes the lowest free parked car: crew walk out to it, get in, it drives off along the lane; it
// comes back so it parks about when the mission ends, and the survivors get out and walk back inside.
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
class ADCVehicle;

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

	/** Cars (one per mission slot). */
	int32 GetCarCount() const { return Slots.Num(); }
	ADCVehicle* GetCar(int32 Slot) const;
	/** Points where crews arrive / leave on foot (lane end of the driveway). */
	static FVector DriveMouth();
	void LaneWalkPoints(TArray<FVector>& Out, bool bLeaving) const;

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

	// Room graph (walkable = built rooms only) + outdoor exits
	struct FPortal { int32 A = 0; int32 B = 0; FVector InA = FVector::ZeroVector; FVector InB = FVector::ZeroVector; };
	struct FExit { int32 Room = 0; FVector In = FVector::ZeroVector; FVector Out = FVector::ZeroVector; };
	TArray<FPortal> Portals;
	TArray<FExit> Exits;
	bool bNavBuilt = false;
	uint32 NavSignature = 0;
	void BuildNav();
	void EnsureNav();
	uint32 ComputeNavSignature() const;
	int32 RoomIndexAt(const FVector& P) const;
	int32 NavRoomAt(const FVector& P) const;
	bool IsWalkable(const ADCRoomModule* R) const;
	bool IsSolidOutdoors(const ADCRoomModule* R) const;
	void RoomDistances(int32 S, TArray<float>& Dist, TArray<int32>& Prev, TArray<int32>& Via) const;
	bool PlanRooms(int32 S, int32 G, const FVector& To, TArray<FVector>& Out) const;
	void PlanOutdoor(const FVector& From, const FVector& To, TArray<FVector>& Out) const;
	bool IsRoomUsable(const ADCRoomModule* R) const;
	bool IsRoomUsable(EDCRoomType Type) const { return IsRoomUsable(FindRoom(Type)); }

	// Idle targets
	struct FAnchor { FVector Point = FVector::ZeroVector; float Yaw = 0.f; bool bHasYaw = false; bool bStation = false; };
	void GetAnchors(const ADCRoomModule* Room, TArray<FAnchor>& Out);
	bool IsSpotTaken(const FVector& P, const ADCCriminalPawn* Except) const;
	bool IsSleeper(const ADCCriminalPawn* P) const;
	TArray<FVector> SleepSpots;
	TArray<float> SleepYaws;
	TArray<int32> SleepOwner; // criminal id per spot, INDEX_NONE = free
	FIntPoint SleepCell = FIntPoint(-999, -999); // grid cell the beds were laid out in
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
	void WalkInFromLane(ADCCriminalPawn* P, float Delay);
	void WalkOffDownLane(ADCCriminalPawn* P, float Delay, bool bForever);

	// Cars + mission choreography
	enum class ECarPhase : uint8 { Parked, Boarding, Departing, Away, Returning, Unloading, Arriving };
	struct FCarSlot
	{
		TWeakObjectPtr<ADCVehicle> Car;
		ECarPhase Phase = ECarPhase::Parked;
		int32 MissionId = INDEX_NONE;
		TArray<int32> Crew;       // seat order
		TArray<int32> Riders;     // who gets out on return
		int32 NextRider = 0;
		float PhaseTime = 0.f;
		float Timer = 0.f;
	};
	TArray<FCarSlot> Slots;
	TMap<int32, TArray<int32>> Walkers; // missions without a car: crew left on foot
	void SyncCars(bool bInstant);
	void ClearCars();
	ADCVehicle* SpawnCar(int32 Slot);
	void TickCars(float Dt);
	void TickWalkers();
	int32 SlotOfMission(int32 MissionId) const;
	bool IsPawnTracked(int32 CriminalId) const;
	void SetPhase(FCarSlot& S, ECarPhase Phase);
	void AssignRunningMissionsInstant();
	void LogSlots(const TCHAR* Tag) const;
	bool IsDetectingNewRun();

	// Dev: -DCWorldTest runs a scripted realtime choreography test with screenshots (see TickWorldTest).
	bool bWorldTest = false;
	double TestTime = 0.0;
	double StepAt = 0.0;
	int32 TestStep = 0;
	bool bTestFakeUpgrade = false;
	TArray<TWeakObjectPtr<AActor>> TestActors;
	FString TestShotDir;
	void TickWorldTest(float Dt);
	void TestShot(const FString& Name);
	void LogPawns(const TCHAR* Tag) const;
};
