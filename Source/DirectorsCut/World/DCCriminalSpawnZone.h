// Director's Cut — a box where criminal pawns idle/wander. D places one per room that hosts crew (Office, Lounge).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DCTypes.h"
#include "DCCriminalSpawnZone.generated.h"

class UBoxComponent;

UCLASS()
class DIRECTORSCUT_API ADCCriminalSpawnZone : public AActor
{
	GENERATED_BODY()
public:
	ADCCriminalSpawnZone();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DC") EDCRoomType RoomType = EDCRoomType::Lounge;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> Zone;
	FVector GetRandomPoint(FRandomStream& Rng) const;
};
