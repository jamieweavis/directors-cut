// Director's Cut — small head-and-shoulders portraits of the crew bodies for the UI avatars.
//
// Each distinct look (body + type accent) is rendered once, on demand, by a scene capture in an empty spot under the
// map, read back asynchronously (no GPU stall), baked into a circular 128 px texture (backdrop + figure + soft edge) and kept for the rest of the
// session. Nothing is captured again once a look is cached: the UI just draws the texture.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Core/DCTypes.h"
#include "DCPortraits.generated.h"

struct FSlateBrush;
class UTexture2D;
class USceneCaptureComponent2D;
class USkeletalMeshComponent;
class UPointLightComponent;
class UTextureRenderTarget2D;

UCLASS()
class DIRECTORSCUT_API UDCPortraits : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
public:
	/** Cache key for a crew member's look: gender, body index (resolved like the pawn does) and type (rim colour). */
	static uint32 KeyFor(const FDCCriminal& C);
	/** The baked portrait for Key, or null while it's being rendered (the request is queued on first ask). */
	static const FSlateBrush* Find(uint32 Key);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject: only ticks while portraits are waiting to be rendered.
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return Pending.Num() > 0 || Busy(); }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDCPortraits, STATGROUP_Tickables); }
	virtual ETickableTickType GetTickableTickType() const override { return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional; }
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	struct FBooth
	{
		TWeakObjectPtr<USceneCaptureComponent2D> Capture;
		TWeakObjectPtr<UTextureRenderTarget2D> Target;
		TArray<TWeakObjectPtr<UPointLightComponent>> Lights;
		TWeakObjectPtr<USkeletalMeshComponent> Driver;   // hidden mannequin holding the idle pose
		TWeakObjectPtr<class UDCCrewBodyComponent> Body;
		uint32 Key = 0;
		int32 Frames = -1;                           // frames since the job started; -1 = free
		bool bCaptured = false;                      // captured; the pixels come back asynchronously
		TSharedPtr<struct FDCPortraitReadback, ESPMode::ThreadSafe> Readback;
		FVector Origin = FVector::ZeroVector;
	};

	bool Busy() const;
	bool EnsureStudio(UWorld* World);
	bool StartJob(FBooth& B, uint32 Key);
	void CaptureJob(FBooth& B);
	void PollReadback(FBooth& B);
	void FinishJob(FBooth& B);
	void ClearBooth(FBooth& B);

	static UDCPortraits* Instance;

	TMap<uint32, TSharedPtr<FSlateBrush>> Brushes;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> Textures;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextureRenderTarget2D>> Targets;
	UPROPERTY(Transient) TObjectPtr<AActor> Studio;
	TArray<uint32> Pending;
	TSet<uint32> Failed;
	TArray<FBooth> Booths;
	int32 Captures = 0;
};
