// Director's Cut — keeps the diorama in sync with UDCRunSubsystem in real time: room states (build timers, reveals),
// camera bounds, station enabling + badges, crew pawns (idle life tied to the clock, leaving for / returning from
// missions, hires, dismissals) and the doors. Spawned by ADCPlayerController. Agent E owns.
//
// Pawns never path through walls: the house generates a tile map (FDCNavGrid) from its own layout: walls with the
// doorways the builder cut, furniture, parked cars. Routes are found on it (A*, pulled tight), so crew move between
// rooms through the doorways and in and out of the house through the front door. While walking, pawns steer round
// each other and re-route when the map changes under them. Idle goals are only ever spawn-zone points or the floor
// in front of a station / piece of furniture, inside usable rooms they can reach.
//
// Cars: one per mission slot (UDCRunSubsystem::GetMaxActiveMissions, max DCSite::MaxParkingSlots), parked in the yard.
// A started mission takes the lowest free parked car: crew walk out to it, get in, it drives off along the lane; it
// comes back so it parks about when the mission ends, and the survivors get out and walk back inside.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DCTypes.h"
#include "World/DCNavGrid.h"
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

	/** Floor-level route from From to To: through the doorways between rooms, in and out of the house by the front
	 *  door, round furniture and parked cars (excludes From, ends with To). */
	void PlanPath(const FVector& From, const FVector& To, TArray<FVector>& Out);
	/** Bumped whenever the walkable map is regenerated: routes planned on an older revision are stale. */
	uint32 GetNavRevision() const { return NavRevision; }
	/** P lies on the generated map (the lane beyond the yard doesn't). */
	bool IsOnNavMap(const FVector& P) const;
	/** Local steering for a walking pawn: Dir (unit, towards Target, its next waypoint) is bent round the other crew
	 *  (bCrowd) and kept off the walls; Scale (0..1) drops when it has to queue. */
	void SteerPawn(const ADCCriminalPawn* Self, const FVector& Target, bool bCrowd, FVector& Dir, float& Scale) const;
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

	/** Dev (-DCAutoPlay): rebuild the whole world from the run, as a loaded save does (no reveals, crew and cars in place). */
	void DevResync() { ResetForNewRun(); }

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

	// Generated tile map (see DCNavGrid.h). Rebuilt whenever its inputs change (NavSignature); NavRevision tells
	// walking pawns their route is stale.
	FDCNavGrid Nav;
	bool bNavBuilt = false;
	uint32 NavSignature = 0;
	uint32 NavRevision = 0;
	float NavBuildMs = 0.f;
	void BuildNav();
	void EnsureNav();
	uint32 ComputeNavSignature() const;
	int32 RoomIndexAt(const FVector& P) const;
	bool IsWalkable(const ADCRoomModule* R) const;
	bool IsSolidOutdoors(const ADCRoomModule* R) const;

	// Debug view (Options > Gameplay > Debug view, or -DCDebugView): what the generators produced.
	UPROPERTY() TObjectPtr<class ULineBatchComponent> DebugStatic;
	UPROPERTY() TObjectPtr<class ULineBatchComponent> DebugLive;
	bool bDebugView = false;
	bool bDebugDirty = true;
	float DebugPoll = 0.f;
	void UpdateDebugView(float Dt);
	void DrawDebugStatic();
	void DrawDebugLive();
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
