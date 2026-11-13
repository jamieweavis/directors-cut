// Director's Cut — builds the whole hideout diorama procedurally at runtime (Agent D owns World/Scene/).
// Spawned by ADCGameMode::StartPlay before BeginPlay is dispatched. Must be deterministic and fast (< 1 s).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DCTypes.h"
#include "DCHideoutBuilder.generated.h"

class ADCRoomModule;
class ADCStation;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class DIRECTORSCUT_API ADCHideoutBuilder : public AActor
{
	GENERATED_BODY()
public:
	ADCHideoutBuilder();

	/** Spawns every room module (all 8, each with Shell/Locked/Building/Level groups), stations, spawn zones,
	 *  garage doors, lights, fog, and post-process. Rooms start Locked except the Office (Built L1). */
	void BuildHideout();
	bool IsBuilt() const { return bBuilt; }

	ADCRoomModule* GetRoom(EDCRoomType Type) const;
	const TArray<ADCRoomModule*>& GetRooms() const { return RoomPtrs; }

protected:
	bool bBuilt = false;
	UPROPERTY() TArray<TObjectPtr<ADCRoomModule>> Rooms;
	TArray<ADCRoomModule*> RoomPtrs;

	// Helpers (D may rewrite freely)
	UMaterialInstanceDynamic* MakeSurface(const FLinearColor& Color, float Roughness = 0.6f, float Metallic = 0.f);
	UMaterialInstanceDynamic* MakeEmissive(const FLinearColor& Color, float Strength);
	UStaticMeshComponent* AddBox(AActor* Owner, USceneComponent* Parent, const FVector& Center, const FVector& Size, UMaterialInterface* Mat);
	void BuildRoom(EDCRoomType Type, const FVector& Origin, const FVector2D& Size);
	ADCStation* SpawnStation(ADCRoomModule* Room, EDCStationType Type, const FString& Name, const FVector& Location, const FVector& PropSize, const FLinearColor& Color);
	void BuildLightingAndPost();
};
