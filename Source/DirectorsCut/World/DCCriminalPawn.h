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

UENUM()
enum class EDCPawnMode : uint8
{
	Wander,   // idle-wandering in the home zone
	WalkOut,  // heading to a door, then vanishes
	Away,     // hidden (on a job / gone)
	WalkIn    // coming back through a door
};

UCLASS()
class DIRECTORSCUT_API ADCCriminalPawn : public ACharacter
{
	GENERATED_BODY()
public:
	ADCCriminalPawn(const FObjectInitializer& ObjectInitializer);

	/** Call once right after spawning. */
	void InitFromCriminal(const FDCCriminal& C);
	/** Refresh level / injury / name. Safe to call every sync. */
	void UpdateFromCriminal(const FDCCriminal& C);
	int32 GetCriminalId() const { return CriminalId; }

	void SetHomeZone(ADCCriminalSpawnZone* Zone);
	/** Teleport to a random point in the home zone and start wandering. */
	void PlaceInZone();

	/** Walk to Door and vanish. bForever = destroy on arrival (arrested / fired). */
	void Depart(ADCGarageDoor* Door, float Delay, bool bForever);
	/** Reappear at Door (after Delay) and walk back into the home zone. If still walking out, queues the return. */
	void ReturnHome(ADCGarageDoor* Door, float Delay);
	void PlayLevelUp(int32 NewLevel);
	/** Hide immediately (e.g. a recruit that will walk in through the door). */
	void SetAway();

	EDCPawnMode GetMode() const { return Mode; }
	bool IsAway() const { return Mode == EDCPawnMode::Away; }
	bool IsLeavingForever() const { return bLeaveForever; }
	void MarkLeaveForever() { bLeaveForever = true; }
	bool IsReturnQueued() const { return bReturnQueued; }
	ADCGarageDoor* GetActiveDoor() const { return ActiveDoor.Get(); }
	/** True while walking through ActiveDoor (door should be open). */
	bool IsNearDoor(float Radius) const;
	bool WantsDestroy() const { return bWantsDestroy; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	int32 CriminalId = INDEX_NONE;
	EDCCriminalType Type = EDCCriminalType::Muscle;
	FString ShortName;
	int32 Level = 1;
	bool bInjured = false;
	bool bInitialized = false;

	EDCPawnMode Mode = EDCPawnMode::Wander;
	TArray<FVector> Path;
	float WaitTime = 0.f;
	float StartDelay = 0.f;
	bool bLeaveForever = false;
	bool bWantsDestroy = false;
	bool bReturnQueued = false;
	float QueuedReturnDelay = 0.f;
	float StuckTime = 0.f;
	float LimpPhase = 0.f;
	TWeakObjectPtr<ADCCriminalSpawnZone> HomeZone;
	TWeakObjectPtr<ADCGarageDoor> ActiveDoor;
	FRandomStream Rng;

	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyMIDs;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> PropComps;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Bandage;
	UPROPERTY() TObjectPtr<UWidgetComponent> Plate;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> LevelRing;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LevelRingMID;
	UPROPERTY() TObjectPtr<UPointLightComponent> LevelLight;
	float LevelUpTime = -1.f;
	bool bLevelUpPending = false;
	bool bUsingFallbackBody = false;

	void SetupVisuals();
	void BuildProps();
	UStaticMeshComponent* AddProp(UStaticMesh* Mesh, const FName& Bone, const FVector& MeshSpaceOffset, const FRotator& MeshSpaceRot, const FVector& Scale, const FLinearColor& Color, float Emissive = 0.f);
	UMaterialInstanceDynamic* MakeSurfaceMID(const FLinearColor& Color, float Emissive);
	void ApplyBodyTint();
	void RefreshPlate();
	void UpdateSpeed();
	void SetHidden(bool bHide);
	void PickWanderTarget();
	FVector Ground(const FVector& P) const;
	void BeginWalkIn(ADCGarageDoor* Door);
	void TickLevelUp(float Dt);
};
