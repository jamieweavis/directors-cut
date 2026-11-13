// Director's Cut — where crews depart/return. D places one in the Office (street door) and one in the Garage.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DCTypes.h"
#include "DCGarageDoor.generated.h"

UCLASS()
class DIRECTORSCUT_API ADCGarageDoor : public AActor
{
	GENERATED_BODY()
public:
	ADCGarageDoor();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DC") EDCRoomType RoomType = EDCRoomType::Garage;
	/** The moving door panel (D assigns a mesh to it, or adds children). E animates it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> DoorPanel;
	/** Point (relative) crews walk to before vanishing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DC") FVector ExitPoint = FVector(150, 0, 0);

	virtual void Open();
	virtual void Close();
	FVector GetExitLocation() const { return GetActorTransform().TransformPosition(ExitPoint); }
protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
};
