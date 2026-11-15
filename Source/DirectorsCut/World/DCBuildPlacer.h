// Director's Cut — build placement mode. "Build" on a new room (UI -> UDCUISubsystem::RequestRoomPlacement) starts
// it: every cell of the build grid (DCSite::Grid) where the room may go is drawn, the hovered
// one is outlined with a label (cells it can't go on show nothing). Click builds there (UDCRunSubsystem::BuildRoom
// with the cell); Esc, opening a panel or losing the ability to build cancels. Owned and driven by
// ADCPlayerController. All visuals are emissive (readable day and night) and never collide.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DCTypes.h"
#include "DCBuildPlacer.generated.h"

class APlayerController;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UWidgetComponent;

UCLASS()
class DIRECTORSCUT_API ADCBuildPlacer : public AActor
{
	GENERATED_BODY()
public:
	ADCBuildPlacer();

	void Begin(EDCRoomType InRoom);
	void Cancel(bool bQuiet = false);
	bool IsActive() const { return bActive; }
	EDCRoomType GetRoom() const { return Room; }

	/** Per frame (player controller): mouse -> ground -> grid, then redraw. */
	void UpdateCursor(APlayerController* PC, float DeltaTime);
	/** Left click while active. Builds on the hovered cell when allowed. */
	void Click();

protected:
	bool bActive = false;
	EDCRoomType Room = EDCRoomType::Office;
	bool bHaveCursor = false;
	FVector2D Cursor = FVector2D::ZeroVector;
	FVector2D Smooth = FVector2D::ZeroVector;
	bool bHover = false;          // cursor over a grid cell
	bool bHoverValid = false;     // ... where the room may go
	FIntPoint Hover = FIntPoint(-999, -999);
	TArray<FIntPoint> Occupied;
	TArray<FIntPoint> Valid;
	float Appear = 0.f;           // fade-in 0..1
	float Time = 0.f;
	int32 LastCost = -1;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Lines;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Dots;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Marks;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> HoverGood;
	UPROPERTY() TObjectPtr<UWidgetComponent> Label;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LineMat;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GoodMat;

	UInstancedStaticMeshComponent* MakeISM(const TCHAR* Name, const TCHAR* MeshPath, UMaterialInstanceDynamic* Mat);
	UMaterialInstanceDynamic* MakeGlow(const FLinearColor& Color, float Strength);
	void RefreshCells();
	void Redraw();
	void HideAll();
};
