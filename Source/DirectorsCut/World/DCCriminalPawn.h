// Director's Cut — a criminal walking around the hideout (one per roster entry). Agent E owns.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/DCTypes.h"
#include "DCCriminalPawn.generated.h"

class ADCCriminalSpawnZone;
class ADCGarageDoor;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UWidgetComponent;
class UPointLightComponent;
class UDCCrewBodyComponent;

UENUM()
enum class EDCPawnMode : uint8
{
	Wander,   // idle life inside built rooms (the director hands out goals)
	WalkOut,  // heading to a door, then vanishes
	Away,     // hidden (on a job / gone)
	WalkIn    // coming back through a door
};

/** What an idle pawn does once it reaches its goal. */
UENUM()
enum class EDCPawnActivity : uint8
{
	Stand,    // hang around
	Work,     // face a station and fiddle with it
	Sleep     // lie down in the bunkroom (night)
};

class ADCWorldDirector;

UCLASS()
class DIRECTORSCUT_API ADCCriminalPawn : public ACharacter
{
	GENERATED_BODY()
public:
	ADCCriminalPawn(const FObjectInitializer& ObjectInitializer);

	/** Call once right after spawning. */
	void InitFromCriminal(const FDCCriminal& C, ADCWorldDirector* InDirector);
	/** Refresh level / injury / name. Safe to call every sync. */
	void UpdateFromCriminal(const FDCCriminal& C);
	int32 GetCriminalId() const { return CriminalId; }

	/** Teleport to Point (floor) and start idling. */
	void PlaceAt(const FVector& FloorPoint);

	/** Walk to Door (via the room graph) and vanish. bForever = destroy on arrival (arrested / fired). */
	void Depart(ADCGarageDoor* Door, float Delay, bool bForever);
	/** Reappear at Door (after Delay) and walk back in. If still walking out, queues the return. */
	void ReturnHome(ADCGarageDoor* Door, float Delay);
	void PlayLevelUp(int32 NewLevel);

	/** Walk this floor path (no door), then vanish (into a car / off down the lane). bForever = destroy on arrival. */
	void WalkOutAlong(const TArray<FVector>& FloorPath, float Delay, bool bForever);
	/** Appear at Start (floor) after Delay (if away) and walk FloorPath back to idle life. */
	void WalkInAlong(const FVector& Start, const TArray<FVector>& FloorPath, float Delay);
	/** Abort a walk-out: stop where we are and go back to idling (the director routes us inside). */
	void CancelWalkOut();
	bool IsWalkingOut() const { return Mode == EDCPawnMode::WalkOut; }
	/** Hide immediately (on a job, or a recruit that will walk in through the door). */
	void SetAway();

	/** Director: walk this path (floor points), then do Activity facing FaceYaw (NaN = keep) for Hold seconds. */
	void SetIdleGoal(const TArray<FVector>& FloorPath, EDCPawnActivity Activity, float FaceYaw, float Hold);
	/** Wake from sleep / cut the current idle activity short. */
	void InterruptIdle(float Delay = 0.f);

	EDCPawnMode GetMode() const { return Mode; }
	EDCPawnActivity GetActivity() const { return Activity; }
	bool IsSleeping() const { return bLying; }
	bool IsIdleReady() const { return Mode == EDCPawnMode::Wander && Path.Num() == 0 && WaitTime <= 0.f && StartDelay <= 0.f; }
	bool IsHeadingToSleep() const { return Mode == EDCPawnMode::Wander && Activity == EDCPawnActivity::Sleep; }
	/** Remaining waypoints (capsule height), for the debug view. */
	const TArray<FVector>& GetRoute() const { return Path; }
	bool IsShoulderingThrough() const { return GhostTime > 0.f; }
	FVector GetGoalLocation() const { return Path.Num() ? Path.Last() : GetActorLocation(); }
	bool IsAway() const { return Mode == EDCPawnMode::Away; }
	bool IsLeavingForever() const { return bLeaveForever; }
	void MarkLeaveForever() { bLeaveForever = true; bReturnQueued = false; }
	bool IsReturnQueued() const { return bReturnQueued; }
	bool IsInjured() const { return bInjured; }
	ADCGarageDoor* GetActiveDoor() const { return ActiveDoor.Get(); }
	/** True while walking through ActiveDoor (door should be open). */
	bool IsNearDoor(float Radius) const;
	bool WantsDestroy() const { return bWantsDestroy; }
	/** Floor point (feet). */
	FVector GetFeet() const;
	int32 GetSleepSlot() const { return SleepSlot; }
	void SetSleepSlot(int32 Slot) { SleepSlot = Slot; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	TWeakObjectPtr<ADCWorldDirector> Director;
	int32 CriminalId = INDEX_NONE;
	bool bFemale = false;          // female crew wear CrewBodiesFemale, driven by the hidden Quinn mannequin
	int32 BodyIndex = INDEX_NONE;  // FDCCriminal::Body; none = pick by id
	EDCCriminalType Type = EDCCriminalType::Muscle;
	FString ShortName;
	int32 Level = 1;
	bool bInjured = false;
	bool bInitialized = false;

	EDCPawnMode Mode = EDCPawnMode::Wander;
	EDCPawnActivity Activity = EDCPawnActivity::Stand;
	TArray<FVector> Path;
	float WaitTime = 0.f;
	float StartDelay = 0.f;
	float GoalYaw = 0.f;
	bool bHasGoalYaw = false;
	float GoalHold = 0.f;
	bool bLying = false;
	int32 SleepSlot = INDEX_NONE;
	bool bLeaveForever = false;
	bool bWantsDestroy = false;
	bool bReturnQueued = false;
	float QueuedReturnDelay = 0.f;
	float StuckTime = 0.f;      // seconds without getting closer to the next waypoint
	float BestDist = 0.f;       // closest we've been to it
	float GhostTime = 0.f;      // > 0: ignore the crowd (shouldering through a jam)
	FVector LegDir = FVector::ZeroVector; // direction of the current leg when it began
	bool bLegValid = false;
	bool bTriedReroute = false;
	uint32 PathRevision = 0;    // director nav revision the route was planned on
	float LimpPhase = 0.f;
	float NamePoll = 0.f;
	TWeakObjectPtr<ADCGarageDoor> ActiveDoor;
	FRandomStream Rng;
	UPROPERTY() TObjectPtr<UClass> AnimClass;

	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyMIDs;
	UPROPERTY() TObjectPtr<UDCCrewBodyComponent> CrewBody; // set = the mannequin is hidden and only drives this body
	UPROPERTY() TObjectPtr<UWidgetComponent> Plate;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> LevelRing;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LevelRingMID;
	UPROPERTY() TObjectPtr<UPointLightComponent> LevelLight;
	float LevelUpTime = -1.f;
	bool bLevelUpPending = false;
	bool bUsingFallbackBody = false;

	void SetupVisuals();
	UMaterialInstanceDynamic* MakeSurfaceMID(const FLinearColor& Color, float Emissive);
	void ApplyBodyTint();
	void RefreshPlate();
	void UpdateSpeed();
	void SetHidden(bool bHide);
	FVector Ground(const FVector& P) const;
	void BeginWalkIn(ADCGarageDoor* Door);
	void TickLevelUp(float Dt);
	void LieDown();
	void GetUp();
	void ArriveIdle();
	/** Call whenever Path is replaced. */
	void BeginRoute();
	/** Plan again from where we stand to the same destination. */
	void Reroute();
};
