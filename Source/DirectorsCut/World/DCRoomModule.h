// Director's Cut — one room of the hideout. PUBLIC API is a frozen contract between Scene (D) and World (E).
// D builds the content by calling AddToGroup(); E implements state switching and the reveal.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DCTypes.h"
#include "DCRoomModule.generated.h"

class ADCStation;

UCLASS()
class DIRECTORSCUT_API ADCRoomModule : public AActor
{
	GENERATED_BODY()

public:
	ADCRoomModule();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DC") EDCRoomType RoomType = EDCRoomType::Office;
	/** Floor footprint in local space (D sets it). Used for camera bounds and the reveal focus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DC") FBox LocalBounds = FBox(FVector(-500, -400, 0), FVector(500, 400, 300));

	/** Register a component (already created/attached/registered on this actor) as part of a group (see DCGroups). */
	void AddToGroup(USceneComponent* Component, FName Group);
	/** Stations inside this room are disabled while the room isn't built. */
	void RegisterStation(ADCStation* Station);

	/** Apply a state instantly (no animation). */
	virtual void ApplyState(EDCRoomState State, int32 Level);
	/** Play the room-built / upgraded reveal (lights flicker on, dust puff). Ends in Built/NewLevel. */
	virtual void PlayReveal(int32 NewLevel);

	EDCRoomState GetState() const { return CurrentState; }
	int32 GetLevel() const { return CurrentLevel; }
	FBox GetWorldBounds() const;
	FVector GetFocusPoint() const;
	const TArray<ADCStation*>& GetStations() const { return Stations; }

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	TMap<FName, TArray<TWeakObjectPtr<USceneComponent>>> Groups;
	UPROPERTY() TArray<TObjectPtr<ADCStation>> Stations;
	EDCRoomState CurrentState = EDCRoomState::Locked;
	int32 CurrentLevel = 0;

	void SetGroupVisible(FName Group, bool bVisible);
};
