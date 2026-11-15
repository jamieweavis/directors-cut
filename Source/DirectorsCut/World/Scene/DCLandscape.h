// Director's Cut — the rural landscape around the house + ALL global lighting (Scene / Landscape agent owns).
// Spawned by ADCGameMode::StartPlay BEFORE ADCHideoutBuilder. Owns: terrain, sky, lane, yard, trees, fields, fences,
// outdoor night lights, the post-process volume, height fog, cvars and the ADCDayNight rig (sun/moon/sky/exposure).
// Layout contract: Core/DCSiteLayout.h. Content placement lives in DCLandscapeBuild.cpp, meshes in DCLandscapeMeshes.
//
// Ground: one big collidable slab whose top is Z = -3 (blocks WorldStatic + Visibility); every other ground layer
// (lawn, gravel yard, lane, paths, pond) is a no-collision skin a few mm above it.
//
// Dev flags: -DCLandShots=7,12,18.5,20.5,23[:wide|far]  one screenshot per hour (landscape review camera), then quit
//            -DCLandShotDir=<abs dir>   -DCNoLand  (skip the vegetation/props; lighting only)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCLandscape.generated.h"

class ADCDayNight;
class APostProcessVolume;
class UExponentialHeightFogComponent;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USpotLightComponent;
class ULightComponent;
class ACameraActor;

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

	virtual void Tick(float DeltaSeconds) override;

protected:
	bool bBuilt = false;
	UPROPERTY() TObjectPtr<ADCDayNight> DayNight;
	UPROPERTY() TObjectPtr<APostProcessVolume> Post;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> HeightFog;

	void SetCVars();
	void BuildLightingAndPost();

	// ------------------------------------------------------------------ assets
	void InitAssets();
	UMaterialInstanceDynamic* MakeMID(const TCHAR* ParentPath, FName Key);
	void Ground(FName Key, int32 Pattern, const FLinearColor& A, const FLinearColor& B, const FLinearColor& C, float Rough, float Axis = 0.f, float Center = 0.f, float Width = 500.f);
	void Foliage(FName Key, const FLinearColor& A, const FLinearColor& B, const FLinearColor& C, float CMix, float Wind, float AOMin = 0.35f, float UpN = 0.f);
	void Proc(FName Key, int32 Pattern, const FLinearColor& A, const FLinearColor& B, float Rough, float Scale = 1.f, float Dirt = 0.4f, float Axis = 0.f);
	void Glow(FName Key, const FLinearColor& Color, float Strength, float Flicker = 0.f);
	UMaterialInterface* M(FName Key) const;

	UPROPERTY() TMap<FName, TObjectPtr<UMaterialInterface>> Mats;
	UPROPERTY() TMap<FName, TObjectPtr<UStaticMesh>> MeshLib;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> GroundMIDs;    // FarColor follows the haze
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> NightGlowMIDs; // Strength fades in at dusk
	TArray<float> NightGlowBase;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ToonMID;  // ink outline + cel band post pass (null when off)
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> WaterMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FireflyMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FireMID;

	/** Instanced batch keyed by mesh + material (one ISM each). */
	UInstancedStaticMeshComponent* Batch(FName MeshKey, FName MatKey, bool bShadow, bool bCollide = false);
	void Add(FName MeshKey, FName MatKey, const FTransform& T, bool bShadow = true);
	/** Engine-primitive helpers (cube/cylinder/sphere sized in cm, pivot centre). */
	void Box(FName MatKey, const FVector& C, const FVector& Size, const FRotator& R = FRotator::ZeroRotator, bool bShadow = true);
	void Rod(FName MatKey, const FVector& A, const FVector& B, float Thick, bool bShadow = true, bool bRound = false);
	UStaticMeshComponent* Slab(FName MatKey, UStaticMesh* Mesh, const FVector& C, const FVector& Scale, float Yaw = 0.f, bool bCollide = false);
	TMap<uint64, TObjectPtr<UInstancedStaticMeshComponent>> Batches;

	// ------------------------------------------------------------------ content (DCLandscapeBuild.cpp)
	void BuildTerrain();
	void BuildVegetation();
	void BuildTrees();
	void BuildFieldsAndFences();
	void BuildLaneProps();
	void BuildGarden();
	void BuildDistant();
	void BuildNightLights();
	void Tree(const FVector& Base, float Height, int32 Kind, uint32 Seed);
	void Sag(const FVector& A, const FVector& B, float Droop, int32 Segs, FName MatKey, float Thick, TArray<FVector>* OutPoints = nullptr);
	bool Blocked(const FVector2D& P, float Margin, bool bAllowLawn) const;
	FVector2D PondC = FVector2D(-2950.f, 1750.f);
	FVector2D PondR = FVector2D(620.f, 430.f);
	FVector2D FireC = FVector2D(-2250.f, -500.f);
	FVector2D LawnC = FVector2D(-150.f, 150.f);
	FVector2D LawnR = FVector2D(2500.f, 2550.f);

	// ------------------------------------------------------------------ night lights
	struct FNightLight { TWeakObjectPtr<ULightComponent> Light; float Base = 0.f; float Seed = 0.f; float Flicker = 0.f; };
	TArray<FNightLight> NightLights;
	UPointLightComponent* NightPoint(const FVector& P, const FLinearColor& Col, float Candelas, float Radius, bool bShadow = false, float Flicker = 0.f);
	struct FFirefly { TWeakObjectPtr<UPointLightComponent> Light; FVector Home; float Seed = 0.f; };
	TArray<FFirefly> Fireflies;
	UPROPERTY() TObjectPtr<USceneComponent> WindRotor;
	float NightK = 0.f;
	double Time = 0.0;

	// ------------------------------------------------------------------ review shots
	TArray<float> ShotHours;
	TArray<FString> ShotFrames;
	int32 ShotStep = 0;
	double ShotNext = 0.0;
	int32 ShotPhase = 0;
	FString ShotDir;
	UPROPERTY() TObjectPtr<ACameraActor> ShotCam;
	double PerfAccum = 0.0; int32 PerfFrames = 0;
	void TickShots();
};
