// Director's Cut — builds the whole hideout diorama procedurally at runtime (Agent D owns World/Scene/).
// Spawned by ADCGameMode::StartPlay before BeginPlay is dispatched. Must be deterministic and fast (< 1 s).
//
// Layout (world, cm; camera looks from -X-Y towards +X+Y so +X/+Y walls are the tall "back" walls):
//   Rows X:  R1 [-1300,-500]  R2 [-500,500]  R3 [500,1300]      Cols Y: A [-1400,-400]  B [-400,400]  C [400,1600]
//   Office R2B (centre) · Workshop R2A · Garage R3A · Screening Room R2-3 x C · Lounge R1A · Server Closet R1B
//   Lawyer R1C(front half) · Clinic R1C(back half). R3B is open void (the street notch behind the office).
//
// Dev flags (World/Scene only):
//   -DCSceneShots=wide,Office,Lounge,...   scene-review camera screenshots (independent of the gameplay rig), then quit
//   -DCSceneShotDir=/abs/dir  -DCSceneShotDelay=8
//   -DCExposure=<EV bias>  -DCLightScale=<mult>   tune exposure / practical brightness without rebuilding
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DCTypes.h"
#include "DCHideoutBuilder.generated.h"

class ADCRoomModule;
class ADCStation;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class ULightComponent;
class UPointLightComponent;
class USpotLightComponent;
class URectLightComponent;
class ACameraActor;


enum class EDCShape : uint8 { Cube, Cylinder, Sphere, Cone, Plane };
struct FDCQuad { float X, Y, Z, W; }; // sheet (x, y, size, height) / glyph segment (x0, y0, x1, y1)

UCLASS()
class DIRECTORSCUT_API ADCHideoutBuilder : public AActor
{
	GENERATED_BODY()
public:
	ADCHideoutBuilder();

	/** Spawns every room module (all 8, each with Shell/Locked/Building/Level groups), stations, spawn zones,
	 *  garage doors, lights, fog, and post-process. Rooms start Locked except the Office (Built L1). */
	void BuildHideout();
	bool IsBuilt() const { return bBuilt; }

	ADCRoomModule* GetRoom(EDCRoomType Type) const;
	const TArray<ADCRoomModule*>& GetRooms() const { return RoomPtrs; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	bool bBuilt = false;
	UPROPERTY() TArray<TObjectPtr<ADCRoomModule>> Rooms;
	TArray<ADCRoomModule*> RoomPtrs;

	// ---------------------------------------------------------------- materials
	UPROPERTY() TMap<FName, TObjectPtr<UMaterialInterface>> Mats;
	UPROPERTY() TArray<TObjectPtr<UStaticMesh>> Meshes; // indexed by EDCShape
	void InitAssets();
	UMaterialInterface* M(FName Key) const;
	void Proc(FName Key, int32 Pattern, const FLinearColor& A, const FLinearColor& B, float Rough = 0.6f, float Metal = 0.f,
		float Scale = 1.f, float Dirt = 0.5f, float Axis = 0.f, const FLinearColor& Emissive = FLinearColor::Black, float EmissiveStrength = 0.f);
	void Emis(FName Key, const FLinearColor& Color, float Strength);
	void Neon(FName Key, const FLinearColor& Color, float Strength, float Flicker, float Seed);
	void Special(FName Key, const TCHAR* Path, TFunctionRef<void(UMaterialInstanceDynamic*)> Setup);

	// ---------------------------------------------------------------- build context
	ADCRoomModule* CurRoom = nullptr; // decor goes into this room...
	FName CurGroup;                   // ...in this group
	struct FInstBatch { ADCRoomModule* Room; FName Group; UStaticMesh* Mesh; UMaterialInterface* Mat; bool bShadow; TArray<FTransform> Xforms; };
	TArray<FInstBatch> Batches;
	TMap<FString, int32> BatchIndex;
	void FlushBatches();

	/** Batched decoration (instanced, no collision). Loc = centre in CurRoom local space, Size in cm. */
	void Deco(EDCShape Shape, const FVector& Loc, const FVector& Size, FName Mat, const FRotator& Rot = FRotator::ZeroRotator, bool bShadow = true);
	/** Cylinder between two local points. */
	void Tube(const FVector& A, const FVector& B, float Dia, FName Mat, bool bShadow = true);
	/** Individual mesh component on Owner attached to Parent (relative transform). */
	UStaticMeshComponent* Single(AActor* Owner, USceneComponent* Parent, EDCShape Shape, const FVector& Loc, const FVector& Size, UMaterialInterface* Mat, const FRotator& Rot = FRotator::ZeroRotator);
	/** Walkable / blocking architecture (floors, walls) in CurRoom Shell. bFloor keeps Visibility blocking. */
	UStaticMeshComponent* Solid(const FVector& Loc, const FVector& Size, FName Mat, bool bFloor);

	struct FOpening { float Center; float Width; float Bottom; float Top; };
	/** A wall run along X (bAlongX) or Y at perpendicular coord Fixed, from A to B, with openings. */
	void WallRun(bool bAlongX, float Fixed, float A, float B, float Z0, float Z1, float Thick, const TArray<FOpening>& Ops, FName Mat, bool bSolid);

	// lights (attached to CurRoom, put in CurGroup)
	float LightScale = 1.f;
	void FinishLight(ULightComponent* L, bool bRoomLight);
	UPointLightComponent* Point(const FVector& Loc, const FLinearColor& Col, float Cd, float Radius, bool bShadows, bool bRoomLight, float SourceRadius = 3.f);
	USpotLightComponent* Spot(const FVector& Loc, const FVector& Target, const FLinearColor& Col, float Cd, float Radius, float Inner, float Outer, bool bShadows, bool bRoomLight);
	URectLightComponent* Rect(const FVector& Loc, const FVector& Target, const FLinearColor& Col, float Cd, float W, float H, float Radius, bool bShadows, bool bRoomLight);

	// stations
	ADCStation* CurStation = nullptr;
	float CurStationHalfZ = 0.f;
	ADCStation* BeginStation(EDCStationType Type, const FString& Name, const FVector& LocalFloorPos, const FVector& HalfExtent, float Yaw = 0.f);
	/** Prop on CurStation. Loc is relative to the station's floor point (z=0 at floor). Also put in CurRoom's CurGroup. */
	UStaticMeshComponent* SP(EDCShape Shape, const FVector& Loc, const FVector& Size, FName Mat, const FRotator& Rot = FRotator::ZeroRotator, bool bShadow = true);
	void SPTube(const FVector& A, const FVector& B, float Dia, FName Mat);

	// generic dressing helpers
	void Sheet(const FVector& Loc, const FVector& Size, float Yaw = 0.f);           // dust sheet over a shape (Locked)
	void Tape(const FVector& A, const FVector& B);                                  // hazard tape strip
	void NeonText(const FString& Text, const FVector& Origin, const FVector& Right, const FVector& Up, float Unit, float Dia, FName Mat, bool bVertical = false);
	void Scaffold(const FVector& From, const FVector& To, float Height);             // Building group scaffolding along a wall
	void LockedDressing(const FBox2D& Rect, const TArray<FDCQuad>& Sheets);        // sheets (x,y,size,height), tape, rim light
	void BuildingDressing(const FBox2D& Rect);

	// ---------------------------------------------------------------- layout
	struct FRoomDef { EDCRoomType Type; FBox2D Rect; FName FloorMat; FName WallMat; bool bWainscot; };
	TArray<FRoomDef> Defs;
	TMap<uint8, TArray<TPair<int32, FOpening>>> RoomOpenings; // room -> (side, opening) side: 0 +X,1 +Y,2 -X,3 -Y
	ADCRoomModule* SpawnRoom(const FRoomDef& Def);
	void BuildShell(const FRoomDef& Def);

	void BuildOffice();
	void BuildLounge();
	void BuildWorkshop();
	void BuildGarage();
	void BuildScreening();
	void BuildClinic();
	void BuildServerCloset();
	void BuildLawyer();
	void BuildLightingAndPost();
	void SetCVars();

	// ---------------------------------------------------------------- runtime FX
	struct FFlicker { TWeakObjectPtr<ULightComponent> Light; float Base; float Rate; float Depth; float Seed; };
	TArray<FFlicker> Flickers;
	void AddFlicker(ULightComponent* L, float Rate, float Depth);
	double Time = 0.0;
	double PerfAccum = 0.0;
	int32 PerfFrames = 0;

	// scene review camera
	UPROPERTY() TObjectPtr<ACameraActor> ReviewCam;
	TArray<FString> ReviewShots;
	FString ReviewDir;
	int32 ReviewStep = 0;
	double ReviewNext = 0.0;
	bool bReviewPrepared = false;
	void TickReview();
	void FrameReview(const FString& Focus);
};
