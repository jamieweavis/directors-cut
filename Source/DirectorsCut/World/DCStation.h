// Director's Cut — a clickable station. PUBLIC API is a frozen contract between Scene (D) and World (E).
// D spawns stations, sets StationType and dresses them with AddPropMesh(); E implements hover/click.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DCTypes.h"
#include "DCStation.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

UCLASS()
class DIRECTORSCUT_API ADCStation : public AActor
{
	GENERATED_BODY()

public:
	ADCStation();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DC") EDCStationType StationType = EDCStationType::DirectorDesk;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DC") FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DC") EDCRoomType OwningRoom = EDCRoomType::Office;
	/** Optional camera focus offset from actor location (world-space units). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DC") FVector FocusOffset = FVector::ZeroVector;

	/** Click/hover volume (root). D sizes it to cover the props. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> ClickBox;

	void SetClickExtent(const FVector& HalfExtent);
	/** Adds a prop mesh as part of this station (highlighted on hover). Material may be null. */
	UStaticMeshComponent* AddPropMesh(UStaticMesh* Mesh, UMaterialInterface* Material, const FTransform& RelativeTransform);
	/** Registers an existing primitive (created by D) as part of the station's highlight set. */
	void RegisterHighlightComponent(UPrimitiveComponent* Comp);

	virtual void SetStationEnabled(bool bEnabled);
	bool IsStationEnabled() const { return bStationEnabled; }

	virtual void OnHoverBegin();
	virtual void OnHoverEnd();
	virtual void OnClicked();

	FVector GetFocusPoint() const { return GetActorLocation() + FocusOffset; }

protected:
	UPROPERTY() TArray<TObjectPtr<UPrimitiveComponent>> HighlightComponents;
	bool bStationEnabled = true;
	bool bHovered = false;
};
