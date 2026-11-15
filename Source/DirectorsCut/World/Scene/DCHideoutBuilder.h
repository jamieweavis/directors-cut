// Director's Cut — builds the hideout procedurally at runtime (Scene / House agent owns this + DCHideoutRooms/Kit).
// Spawned by ADCGameMode::StartPlay (after ADCLandscape) before BeginPlay is dispatched. Deterministic and fast.
//
// The look: a small abandoned farmhouse (the Office) in open country, taken over by a crew. Camera-facing walls
// (-X, -Y) and the partitions between rooms are razed to low stubs just above the floor so the iso camera sees into
// every room; the back walls (+X, +Y) stand tall with old windows. No roof timbers. The Office has two warm porch
// lanterns at its open front that come on at dusk.
//
// v3 build grid (DCHideoutGrid.cpp, Core/DCSiteLayout.h DCSite::Grid): every room is one cell of an irregular
// (jittered) grid and the player chooses where each new room goes. All 8 room modules exist from the start; an
// unplaced one is parked off-site and hidden. Whenever a placement or a room's stage changes, the builder regenerates
// the whole house: per cell a floor, and per edge either a tall back wall (exterior +X/+Y), a low stub (exterior
// -X/-Y) or a low partition stub with a doorway gap at the edge midpoint (two finished rooms; built by the -side room). A room
// under its first build is a slab + timber frame + scaffold. Rooms are furnished by a layout solver that fits
// their shape and connections (DCHideoutFurnish*.cpp, one recipe per room; the Garage type is dressed as the Kit Room). One clickable station covers
// each whole room and opens its panel. No house geometry collides (pawns path through the doorway midpoints).
//
// Dev flags (World/Scene only):
//   -DCSceneShots=wide,Office,Lounge,...   scene-review camera screenshots (independent of the gameplay rig), then quit
//   -DCSceneShotDir=/abs/dir  -DCSceneShotDelay=8
//   -DCRoomStates=Workshop:B,Garage:2,Lounge:B1   force room states before the review shots
//        (L locked, B building (first build), B<n> upgrading over level n, <n> built level n)
//   -DCHouseGround         temporary flat ground under the compound (review without the landscape)
//   -DCLightScale=<mult>   practical brightness without rebuilding.   Day/night flags: see DCDayNight.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DCTypes.h"
#include "DCHideoutBuilder.generated.h"

class ADCRoomModule;
namespace DCFurnish { struct FCtx; }
class ADCStation;
class ADCDayNight;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class ULightComponent;
class UPointLightComponent;
class USpotLightComponent;
class URectLightComponent;
class ULocalFogVolumeComponent;
class ACameraActor;
class APostProcessVolume;


enum class EDCShape : uint8 { Cube, Cylinder, Sphere, Cone, Plane };
struct FDCQuad { float X, Y, Z, W; }; // sheet (x, y, size, height)

UCLASS()
class DIRECTORSCUT_API ADCHideoutBuilder : public AActor
{
	GENERATED_BODY()
public:
	ADCHideoutBuilder();

	/** Spawns every room module (all 8) with its room-wide station and spawn zone, and the materials. Rooms start
	 *  Locked except the Office (Built L1); geometry follows the run's grid placement (see SyncGrid). */
	void BuildHideout();
	bool IsBuilt() const { return bBuilt; }

	ADCRoomModule* GetRoom(EDCRoomType Type) const;
	const TArray<ADCRoomModule*>& GetRooms() const { return RoomPtrs; }
	ADCDayNight* GetDayNight() const { return DayNight; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	bool bBuilt = false;
	UPROPERTY() TArray<TObjectPtr<ADCRoomModule>> Rooms;
	TArray<ADCRoomModule*> RoomPtrs;
	UPROPERTY() TObjectPtr<ADCDayNight> DayNight;

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

	// ---------------------------------------------------------------- dressing fit
	// Room dressing (DCHideoutRooms.cpp) is authored on the original, larger grid. While bFit is on, room-local
	// positions are scaled to the real (DCSite::RoomScale) rooms: XY by FitXY, Z by FitZ; sizes by FitS. Composite
	// props (kit pieces, station props) keep their proportions around an anchor so they stay in one piece.
	bool bFit = false;
	float FitXY = 0.64f, FitZ = 0.85f, FitS = 0.82f;
	bool bAnchored = false;
	FVector FitAnchor = FVector::ZeroVector;
	FVector MapP(const FVector& P) const;
	FVector MapSize(const FVector& S) const { return bFit ? S * FitS : S; }
	float MapLen(float L) const { return bFit ? L * FitS : L; }
	struct FAnchorScope
	{
		ADCHideoutBuilder* B; bool bSet;
		FAnchorScope(ADCHideoutBuilder* InB, const FVector& At) : B(InB), bSet(InB->bFit && !InB->bAnchored) { if (bSet) { B->bAnchored = true; B->FitAnchor = At; } }
		~FAnchorScope() { if (bSet) { B->bAnchored = false; } }
	};
	struct FNoFit
	{
		ADCHideoutBuilder* B; bool bWas;
		explicit FNoFit(ADCHideoutBuilder* InB) : B(InB), bWas(InB->bFit) { B->bFit = false; }
		~FNoFit() { B->bFit = bWas; }
	};
	FVector StationAnchor = FVector::ZeroVector; // CurStation floor in authored coordinates

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
	/** Box between two local points (square section). */
	void Bar(const FVector& A, const FVector& B, float W, float H, FName Mat, bool bShadow = true);
	/** Individual mesh component on Owner attached to Parent (relative transform). */
	UStaticMeshComponent* Single(AActor* Owner, USceneComponent* Parent, EDCShape Shape, const FVector& Loc, const FVector& Size, UMaterialInterface* Mat, const FRotator& Rot = FRotator::ZeroRotator);
	/** Walkable / blocking architecture (floors, walls) in CurRoom's CurGroup. bFloor keeps Visibility blocking. */
	UStaticMeshComponent* Solid(const FVector& Loc, const FVector& Size, FName Mat, bool bFloor);
	/** Records a component in a builder-managed group (shared-wall variants). */
	void TrackComponent(UPrimitiveComponent* C, FName Group);

	struct FOpening { float Center; float Width; float Bottom; float Top; };
	/** A wall run along X (bAlongX) or Y at perpendicular coord Fixed, from A to B, with openings. */
	void WallRun(bool bAlongX, float Fixed, float A, float B, float Z0, float Z1, float Thick, const TArray<FOpening>& Ops, FName Mat, bool bSolid);
	/** Old timber window filling an opening of a wall run (frame, cross bars, some panes broken, some boarded). */
	void OldWindow(bool bAlongX, float Fixed, float Inward, const FOpening& O, int32 Seed);
	/** Tall standing wall: masonry + interior skin with exposed patches/lath, wall plate, windows. Solid. */
	void TallWall(bool bAlongX, float Fixed, float Inward, float A, float B, float H, const TArray<FOpening>& Doors, const TArray<FOpening>& Windows, FName Outer, FName Skin, int32 Seed);
	/** Broken wall stub along a run (ragged tops, piers at the ends). bSolid: collidable knee wall (>= 50 cm). */
	void RuinStub(bool bAlongX, float Fixed, float A, float B, int32 Seed, FName Mat, float EndA, float EndB, const TArray<FOpening>& Gaps, bool bSolid);
	/** Fallen masonry, bricks and planks on the Outward side of a run (decoration, no collision). */
	void RubbleRun(bool bAlongX, float Fixed, float A, float B, float Outward, int32 Seed, FName Mat, float Density = 1.f);
	void RubblePile(const FVector& C, float Radius, int32 Count, int32 Seed, FName Mat);
	/** Weeds / grass tufts scattered in a disc. */
	void Weeds(const FVector& C, float Radius, int32 Count, int32 Seed, float MaxH = 60.f);
	/** Ivy clinging to a wall face (Normal points away from the wall into the room/air). */
	void Ivy(const FVector& Base, const FVector& Normal, float W, float H, int32 Seed);
	/** Bare bulb on a flex hanging from TopZ, optional light. */
	void HangingBulb(const FVector& C, float TopZ, const FLinearColor& Col, float Cd, float Radius, bool bShadow);
	/** Amber festoon string lights along a sagging line, with Lights point lights spread along it. */
	void StringLights(const FVector& A, const FVector& B, float Sag, int32 Bulbs, int32 Lights, float Cd);
	/** Tripod work light (warm), optional real light. */
	void WorkLight(const FVector& Base, float Yaw, bool bLight, float Cd, bool bRoomLight);
	void CampChair(const FVector& C, float Yaw, FName Fabric);
	void Generator(const FVector& C, float Yaw);
	void Lantern(const FVector& C, bool bLight);

	// lights (attached to CurRoom, put in CurGroup)
	float LightScale = 1.35f;
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

	// ---------------------------------------------------------------- prop kit (room-local; bOnStation => SP on CurStation)
	bool bKitOnStation = false;
	void K(EDCShape Shape, const FVector& Loc, const FVector& Size, FName Mat, const FRotator& Rot = FRotator::ZeroRotator, bool bShadow = true);
	void KTube(const FVector& A, const FVector& B, float Dia, FName Mat);
	/** Wall/desk monitor: bezel + emissive screen facing local -X rotated by Yaw. */
	void Monitor(const FVector& C, float W, float H, float Yaw, FName ScreenMat, float Pitch = 0.f);
	void Car(const FVector& C, float Yaw, FName Paint, bool bVan);
	void OfficeChair(const FVector& C, float Yaw);
	void Crate(const FVector& C, const FVector& S, float Yaw, FName Mat);
	void ShelfUnit(const FVector& C, float Yaw, float Len, int32 Seed);
	void PalletRack(const FVector& C, float Yaw, int32 Bays, int32 Seed);
	void Pendant(const FVector& C, bool bWarm, bool bLight, float Cd = 40.f);
	void LedBatten(const FVector& C, float Len, float Yaw, bool bLight, float Cd = 60.f);
	void Duffel(const FVector& C, float Yaw, FName Mat);

	// generic dressing helpers
	void Sheet(const FVector& Loc, const FVector& Size, float Yaw = 0.f);           // plastic sheet over a shape (Locked)
	void Tape(const FVector& A, const FVector& B);                                  // caution tape strip
	void Scaffold(const FVector& From, const FVector& To, float Height);             // Building group scaffolding along a wall
	void LockedDressing(const FBox2D& Rect, const TArray<FDCQuad>& Sheets);        // sheets (x,y,size,height), tape, rim light
	void BuildingDressing(const FBox2D& Rect);
	void SunDust(const FVector& A, const FVector& B, int32 Count, float Spread);     // dust motes along a sun path (room local)
	ULocalFogVolumeComponent* ShaftFog(const FVector& Loc, const FVector& Radius, float Density);

	// ---------------------------------------------------------------- layout
	struct FRoomDef { EDCRoomType Type; FName FloorMat; FName OuterMat; FName SkinMat; FName FrontMat; FBox2D Rect; };
	TArray<FRoomDef> Defs;
	TMap<uint8, TArray<TPair<int32, FOpening>>> RoomOpenings; // room -> (side, opening) side: 0 +X,1 +Y,2 -X,3 -Y
	TMap<uint8, TArray<TPair<int32, FVector2D>>> KeepSolid;    // room -> (side, [a,b] local) no windows there
	ADCRoomModule* SpawnRoom(const FRoomDef& Def);
	void BuildShell(const FRoomDef& Def);
	void BuildHouseRoof();
	void BuildWingRoof(const FRoomDef& Def);

	// shared walls between two rooms (A on the -side, B on the +side of the wall line)
	struct FSharedWall
	{
		TWeakObjectPtr<ADCRoomModule> A, B;
		TArray<TWeakObjectPtr<UPrimitiveComponent>> Comps[3]; // 0 stub, 1 tall, 2 partition
		TArray<TEnumAsByte<ECollisionEnabled::Type>> Coll[3];
		int32 Cur = -2;
	};
	TArray<FSharedWall> SharedWalls;
	TMap<FName, FIntPoint> TrackedGroups; // group name -> (wall index, variant)
	void UpdateSharedWalls();

	void BuildOffice();
	void BuildLounge();
	void BuildWorkshop();
	void BuildGarage();
	void BuildOpsRoom();
	void BuildClinic();
	void BuildServerRoom();
	void BuildLawyer();
	void BuildLightingAndPost();

	// ---------------------------------------------------------------- build grid (DCHideoutGrid.cpp)
	struct FGridStyle { FName Floor, Outer, Skin, Front; };
	FGridStyle StyleFor(EDCRoomType T) const;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> GridFloors;   // per room (index = room order in RoomPtrs)
	UPROPERTY() TArray<TObjectPtr<ADCStation>> GridStations;
	UPROPERTY() TArray<TObjectPtr<class ADCCriminalSpawnZone>> GridZones;
	TArray<TArray<TWeakObjectPtr<UActorComponent>>> GridGen;          // generated components per room
	TMap<uint8, FIntPoint> DevCells;                                  // rooms forced on by dev flags get a stable cell
	uint32 GridSignature = 0;
	void SpawnGridRooms();
	/** Re-reads placements + stages; regenerates the house when anything changed (or bForce). */
	void SyncGrid(bool bForce);
	/** 0 = not on site, 1 = first build in progress (frame), 2 = finished (walls). */
	static int32 GridStage(const ADCRoomModule* R);
	void BuildGridRoom(int32 Index, const FIntPoint& Cell, int32 Stage, const TMap<FIntPoint, int32>& StageAt);
	// edge geometry, room-local 2D endpoints (counter-clockwise outline, so +left of A->B is inside)
	void EdgeBox(const FVector2D& A, const FVector2D& B, float T0, float T1, float Inset, float Z0, float Z1, float Thick, FName Mat, float Pitch = 0.f);
	void EdgeWall(const FVector2D& A, const FVector2D& B, float T0, float T1, float Inset, float Z0, float Z1, float Thick, const TArray<FOpening>& Ops, FName Mat);
	void EdgeWindow(const FVector2D& A, const FVector2D& B, const FOpening& O, float Inset, int32 Seed);
	void TallEdge(const FVector2D& A, const FVector2D& B, float H, const FGridStyle& St, int32 Seed, bool bStepStart, bool bStepEnd, uint8 OmitWindows = 0);
	/** Where TallEdge puts its windows and broken ends (shared with the furnisher so furniture respects them).
	 *  OutSteps: (t0, t1, height) of each broken-down stretch. */
	static void TallEdgePlan(float L, float H, int32 Seed, bool bStepStart, bool bStepEnd, TArray<FOpening>& OutWin, TArray<FVector>& OutSteps);
	/** DoorW > 0 cuts a front doorway of that width at the edge midpoint: an open timber frame, threshold and step. */
	void RuinEdge(const FVector2D& A, const FVector2D& B, const FGridStyle& St, int32 Seed, float EndA, float EndB, float DoorW = 0.f);
	void PartitionEdge(const FVector2D& A, const FVector2D& B, const FGridStyle& St, int32 Seed);
	/** Wall lamp on the inside face of edge A->B at fraction T, height Z (room light). */
	void BracketLamp(const FVector2D& A, const FVector2D& B, float T, float Z, const FLinearColor& Col, float Cd, float Radius);
	/** The Office's two porch lantern posts (geometry; the night lights are added once, see BuildLightingAndPost). */
	void OfficePorchLamps(const FVector2D Q[4]);
	/** Porch lantern K (0 front-left, 1 front-right): post position and the direction the lantern hangs (room-local). */
	static void PorchLampSpot(const FVector2D Q[4], int32 K, FVector2D& OutPost, FVector2D& OutDir);
	void FrameRoom(const FVector2D Q[4], float H, int32 Seed, int32 ScaffoldEdge);
	/** Scaffolding along edge E, just outside the room (E < 0: none). */
	void EdgeScaffold(const FVector2D Q[4], int32 E, float H);

	// ---------------------------------------------------------------- furnishing (DCHideoutFurnish*.cpp)
	/** Lays furniture out to fit the room's real shape and connections, then draws it. EdgeKind per edge (see
	 *  DCSite::Grid::Corners): 0 tall wall, 1 low partition with a centred doorway, 2 collapsed open front.
	 *  bExit: pawns walk out over that edge. WallSeed: the per-edge seed TallEdge used (windows).
	 *  OutOmitWindows: per edge, a bit per TallEdgePlan window that furniture now stands against (the wall is built
	 *  without it). Run it BEFORE the walls. Returns false if the room type has no furnishing (the caller then lights
	 *  the room itself). */
	bool FurnishGridRoom(ADCRoomModule* Room, const FVector2D Q[4], const int32 EdgeKind[4], const bool bExit[4], const bool bStepStart[4], const bool bStepEnd[4], float H, int32 WallSeed, int32 Seed, uint8 OutOmitWindows[4]);
	// One recipe per room type, each in its own file (DCHideoutFurnish<Room>.cpp). The Garage is the Kit Room.
	void FurnishOffice(DCFurnish::FCtx& C);
	void FurnishLounge(DCFurnish::FCtx& C);
	void FurnishWorkshop(DCFurnish::FCtx& C);
	void FurnishOpsRoom(DCFurnish::FCtx& C);
	void FurnishClinic(DCFurnish::FCtx& C);
	void FurnishServerRoom(DCFurnish::FCtx& C);
	void FurnishLawyer(DCFurnish::FCtx& C);
	void FurnishKitRoom(DCFurnish::FCtx& C);

	// ---------------------------------------------------------------- runtime FX
	struct FFlicker { TWeakObjectPtr<ULightComponent> Light; float Base; float Rate; float Depth; float Seed; };
	TArray<FFlicker> Flickers;
	void AddFlicker(ULightComponent* L, float Rate, float Depth);
	UPROPERTY() TArray<TObjectPtr<ULocalFogVolumeComponent>> SunFogs;
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
	FString ForcedStates;
	void ApplyForcedStates();
	void TickReview();
	void FrameReview(const FString& Focus);
};
