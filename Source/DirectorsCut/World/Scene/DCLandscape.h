// Director's Cut — the rural landscape around the house + ALL global lighting (Scene / Landscape agent owns).
// Spawned by ADCGameMode::StartPlay BEFORE ADCHideoutBuilder. Owns: terrain, sky, lane, yard, trees, fields, fences,
// outdoor night lights, the post-process volume, height fog, cvars and the ADCDayNight rig (sun/moon/sky/exposure).
// Layout contract: Core/DCSiteLayout.h.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCLandscape.generated.h"

class ADCDayNight;
class APostProcessVolume;
class UExponentialHeightFogComponent;

UCLASS()
class DIRECTORSCUT_API ADCLandscape : public AActor
{
	GENERATED_BODY()
public:
	ADCLandscape();

	void BuildLandscape();
	bool IsBuilt() const { return bBuilt; }
	ADCDayNight* GetDayNight() const { return DayNight; }
	APostProcessVolume* GetPostVolume() const { return Post; }

	/** First landscape in the world, or null. */
	static ADCLandscape* Get(const UWorld* World);

protected:
	bool bBuilt = false;
	UPROPERTY() TObjectPtr<ADCDayNight> DayNight;
	UPROPERTY() TObjectPtr<APostProcessVolume> Post;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> HeightFog;

	void SetCVars();
	void BuildLightingAndPost();
};
