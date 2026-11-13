// Director's Cut — day/night lighting rig (Agent D). Spawned and wired by ADCHideoutBuilder.
// Reads UDCRunSubsystem's clock every tick and drives: the sun/moon directional light (azimuth sweeps behind the +X/+Y
// back walls so light falls through the factory windows), sky light colour/intensity, window-glass glow, sun-shaft
// fog volumes, dust motes, exterior street lamps (on at dusk) and the exposure curve. Interior practicals (RoomLight)
// are never touched. With no active run (title screen) it holds golden hour.
//
// Dev flags:  -DCFixedHour=H   force the displayed hour (ignores the run clock; for lighting review)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCDayNight.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;
class ULightComponent;
class ULocalFogVolumeComponent;
class UMaterialInstanceDynamic;
class APostProcessVolume;
class UExponentialHeightFogComponent;

UCLASS()
class DIRECTORSCUT_API ADCDayNight : public AActor
{
	GENERATED_BODY()
public:
	ADCDayNight();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkyLightComponent> Sky;

	// ---- wiring (set by the builder before the first tick)
	UPROPERTY() TObjectPtr<APostProcessVolume> Post;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> HeightFog;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> WindowMats;   // SkyColor / Strength
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DustMat;              // Strength
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LampMat;              // street-lamp heads: Strength
	void AddShaftFog(ULocalFogVolumeComponent* Fog);
	/** Exterior light that switches on at dusk (owned by this actor). Intensity in candelas. */
	void AddStreetLight(const FVector& WorldPos, const FVector& WorldTarget, const FLinearColor& Color, float Candelas, float Radius, float ConeOuter, bool bShadows);

	/** Fake god-ray volume through a window/skylight (world centre of the opening, its along-wall axis, size). */
	void AddShaft(const FVector& Centre, const FVector& Along, float W, float H);

	/** Hour currently displayed (smoothed). */
	float GetDisplayHour() const { return DisplayHour; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	void Apply(float Hour, float DeltaSeconds);

	struct FShaft { TWeakObjectPtr<ULocalFogVolumeComponent> Fog; float Base = 0.f; };
	TArray<FShaft> Shafts;
	struct FStreet { TWeakObjectPtr<ULightComponent> Light; float Base = 0.f; float Seed = 0.f; };
	TArray<FStreet> Streets;
	struct FBeam { TWeakObjectPtr<class UStaticMeshComponent> Mesh; FVector Centre; FVector Along; float W = 0.f; float H = 0.f; };
	TArray<FBeam> Beams;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BeamMat;
	float BeamK = 1.f;

	float DisplayHour = 17.5f;
	bool bHaveHour = false;
	float FixedHour = -1.f;
	float LampAlpha = 0.f;
	double Time = 0.0;
	float EVDay = -2.5f;
	float EVNight = -1.6f;
	float SunLux = 110.f;
	float MoonLux = 4.f;
	float SkyDay = 10.f;
	float SkyNight = 1.2f;
};
