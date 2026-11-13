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

	// ── Additions (Agent E) ──────────────────────────────────────────────────
	/** Garage = roll-up (panel slides up + squashes); anything else swings open on its pivot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DC") bool bRollUp = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DC") float RollUpHeight = 250.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DC") float SwingAngle = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DC") float OpenSeconds = 0.9f;
	/** Spawn a simple primitive panel when D hasn't attached anything to DoorPanel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DC") bool bFallbackPanel = true;
	bool IsOpen() const { return bWantOpen; }
	/** Point just inside the door that crews walk through (world). */
	FVector GetEntryLocation() const { return GetActorTransform().TransformPosition(-ExitPoint * 0.6f); }

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	bool bWantOpen = false;
	float OpenAlpha = 0.f;
	FTransform PanelClosed;
	bool bCapturedClosed = false;
	void ApplyAlpha();
};
