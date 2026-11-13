// Director's Cut — one room of the hideout. PUBLIC API is a frozen contract between Scene (D) and World (E).
// D builds the content by calling AddToGroup(); E implements state switching and the reveal.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DCTypes.h"
#include "DCRoomModule.generated.h"

class ADCStation;
class ULightComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

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

	// ── Additions (Agent E) ──────────────────────────────────────────────────
	bool IsRevealPlaying() const { return bRevealing; }
	/** Total reveal length (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DC") float RevealDuration = 3.2f;
	/** Whether the reveal drives the camera (pan over and back). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DC") bool bRevealMovesCamera = true;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	// Reveal state
	bool bRevealing = false;
	float RevealTime = 0.f;
	int32 RevealLevel = 1;
	EDCRoomState RevealFromState = EDCRoomState::Locked;
	bool bRevealSwapped = false;
	bool bRevealCameraReturned = false;
	struct FRevealLight { TWeakObjectPtr<ULightComponent> Light; float Target = 0.f; float Start = 0.f; float Seed = 0.f; };
	TArray<FRevealLight> RevealLights;
	TMap<TWeakObjectPtr<ULightComponent>, float> LightBaseIntensity;
	TMap<TWeakObjectPtr<UPrimitiveComponent>, TEnumAsByte<ECollisionEnabled::Type>> OriginalCollision;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> TempScaffold;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> DustPuff;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DustMID;
	FName DustParam = NAME_None;
	float DustBaseValue = 1.f;

	void RevealSwapToBuilt();
	void FinishReveal();
	void SpawnFallbackScaffold();
	void ClearTempScaffold();
	void SpawnDustPuff();
	float GetLightBase(ULightComponent* L);
	bool GroupHasContent(FName Group) const;
	TMap<FName, TArray<TWeakObjectPtr<USceneComponent>>> Groups;
	UPROPERTY() TArray<TObjectPtr<ADCStation>> Stations;
	EDCRoomState CurrentState = EDCRoomState::Locked;
	int32 CurrentLevel = 0;

	void SetGroupVisible(FName Group, bool bVisible);
};
