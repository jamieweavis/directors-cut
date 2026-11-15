#include "World/DCWorldDirector.h"
#include "World/DCPlayerController.h"
#include "World/DCCameraRig.h"
#include "World/DCRoomModule.h"
#include "World/DCCriminalPawn.h"
#include "World/DCCriminalSpawnZone.h"
#include "World/DCGarageDoor.h"
#include "World/DCStation.h"
#include "World/DCVehicle.h"
#include "Core/DCSettings.h"
#include "Components/LineBatchComponent.h"
#include "Core/DCSiteLayout.h"
#include "Core/DCAssetPaths.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Core/DCRunSubsystem.h"
#include "Core/DCUISubsystem.h"
#include "Core/DCDevTools.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "DirectorsCut.h"

ADCWorldDirector::ADCWorldDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Rng.Initialize(0xD1C7);
}

void ADCWorldDirector::Initialize(ADCPlayerController* InPC)
{
	if (bInitialized) { return; }
	bInitialized = true;
	PC = InPC;
	GatherWorld();
	bWorldTest = FParse::Param(FCommandLine::Get(), TEXT("DCWorldTest"));
	TestShotDir = FPaths::ProjectSavedDir() / TEXT("DCShots");
	FParse::Value(FCommandLine::Get(), TEXT("DCShotDir="), TestShotDir);

	if (UDCRunSubsystem* Run = UDCRunSubsystem::Get(this))
	{
		HState = Run->OnRunStateChanged.AddUObject(this, &ADCWorldDirector::OnRunStateChanged);
		HPhase = Run->OnPhaseChanged.AddUObject(this, &ADCWorldDirector::OnPhaseChanged);
		HStart = Run->OnMissionStarted.AddUObject(this, &ADCWorldDirector::OnMissionStarted);
		HDone = Run->OnMissionCompleted.AddUObject(this, &ADCWorldDirector::OnMissionCompleted);
		HRoom = Run->OnRoomChanged.AddUObject(this, &ADCWorldDirector::OnRoomChanged);
		HLevel = Run->OnCriminalLevelUp.AddUObject(this, &ADCWorldDirector::OnCriminalLevelUp);
		HHired = Run->OnCriminalHired.AddUObject(this, &ADCWorldDirector::OnCriminalHired);
		if (Run->HasActiveRun())
		{
			LastPhase = Run->GetPhase();
			ResetForNewRun();
		}
	}
	if (UDCUISubsystem* UI = UDCUISubsystem::Get(this))
	{
		HPanel = UI->OnPanelChanged.AddUObject(this, &ADCWorldDirector::OnPanelChanged);
	}
	UpdateBounds();
}

void ADCWorldDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDCRunSubsystem* Run = UDCRunSubsystem::Get(this))
	{
		Run->OnRunStateChanged.Remove(HState);
		Run->OnPhaseChanged.Remove(HPhase);
		Run->OnMissionStarted.Remove(HStart);
		Run->OnMissionCompleted.Remove(HDone);
		Run->OnRoomChanged.Remove(HRoom);
		Run->OnCriminalLevelUp.Remove(HLevel);
		Run->OnCriminalHired.Remove(HHired);
	}
	if (UDCUISubsystem* UI = UDCUISubsystem::Get(this)) { UI->OnPanelChanged.Remove(HPanel); }
	ClearCars();
	Super::EndPlay(Reason);
}

void ADCWorldDirector::GatherWorld()
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	Rooms.Reset(); Zones.Reset(); Doors.Reset(); Stations.Reset();
	for (TActorIterator<ADCRoomModule> It(W); It; ++It) { Rooms.Add(*It); }
	for (TActorIterator<ADCCriminalSpawnZone> It(W); It; ++It) { Zones.Add(*It); }
	for (TActorIterator<ADCGarageDoor> It(W); It; ++It) { Doors.Add(*It); }
	for (TActorIterator<ADCStation> It(W); It; ++It) { Stations.Add(*It); }
	bNavBuilt = false;
	SleepSpots.Reset();
	UE_LOG(LogDC, Log, TEXT("WorldDirector: %d rooms, %d spawn zones, %d doors, %d stations"), Rooms.Num(), Zones.Num(), Doors.Num(), Stations.Num());
}

ADCRoomModule* ADCWorldDirector::FindRoom(EDCRoomType Type) const
{
	for (const TWeakObjectPtr<ADCRoomModule>& R : Rooms) { if (R.IsValid() && R->RoomType == Type) { return R.Get(); } }
	return nullptr;
}

int32 ADCWorldDirector::RoomIndexAt(const FVector& P) const
{
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		const ADCRoomModule* R = Rooms[i].Get();
		if (!R || !R->bPlaced) { continue; }
		if (R->ContainsPoint2D(P)) { return i; }
	}
	return INDEX_NONE;
}

ADCRoomModule* ADCWorldDirector::RoomAt(const FVector& P) const
{
	const int32 I = RoomIndexAt(P);
	return I == INDEX_NONE ? nullptr : Rooms[I].Get();
}

ADCCriminalPawn* ADCWorldDirector::FindPawn(int32 CriminalId) const
{
	for (ADCCriminalPawn* P : Pawns) { if (P && P->GetCriminalId() == CriminalId) { return P; } }
	return nullptr;
}

bool ADCWorldDirector::IsRoomUsable(const ADCRoomModule* R) const
{
	if (!R) { return false; }
	if (UDCDevTools::IsBuildAll()) { return true; }
	return R->IsUsable() && !RevealQueue.Contains(R->RoomType);
}

ADCGarageDoor* ADCWorldDirector::GetStreetDoor() const
{
	ADCGarageDoor* Any = nullptr;
	for (const TWeakObjectPtr<ADCGarageDoor>& D : Doors)
	{
		if (!D.IsValid()) { continue; }
		if (D->RoomType == EDCRoomType::Office) { return D.Get(); }
		if (!Any || (Any->RoomType == EDCRoomType::Garage && D->RoomType != EDCRoomType::Garage)) { Any = D.Get(); }
	}
	return Any;
}

ADCGarageDoor* ADCWorldDirector::GetMissionDoor() const
{
	if (IsRoomUsable(EDCRoomType::Garage))
	{
		for (const TWeakObjectPtr<ADCGarageDoor>& D : Doors) { if (D.IsValid() && D->RoomType == EDCRoomType::Garage) { return D.Get(); } }
	}
	return GetStreetDoor();
}

// ─── Navigation: generated tile map (FDCNavGrid) ─────────────────────────────

bool ADCWorldDirector::IsWalkable(const ADCRoomModule* R) const
{
	if (!R || !R->bPlaced) { return false; }
	if (UDCDevTools::IsBuildAll()) { return true; }
	return R->IsUsable() && !RevealQueue.Contains(R->RoomType);
}

bool ADCWorldDirector::IsSolidOutdoors(const ADCRoomModule* R) const
{
	// Anything standing on the plot: finished wings, frames under construction, a wing mid-reveal.
	if (!R || !R->bPlaced) { return false; }
	if (UDCDevTools::IsBuildAll()) { return true; }
	return R->GetState() != EDCRoomState::Locked || R->IsRevealPlaying() || RevealQueue.Contains(R->RoomType);
}

uint32 ADCWorldDirector::ComputeNavSignature() const
{
	// Everything the map is generated from: which rooms stand where, their furniture, and the parked cars.
	uint32 H = 0x9E37u + Rooms.Num();
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		const ADCRoomModule* R = Rooms[i].Get();
		H = HashCombineFast(H, (IsWalkable(R) ? 1u : 0u) | (IsSolidOutdoors(R) ? 2u : 0u) | (R && R->IsRevealPlaying() ? 4u : 0u) | (R && R->bPlaced ? 8u : 0u));
		if (!R || !R->bPlaced) { continue; }
		H = HashCombineFast(H, GetTypeHash(R->GridCell));
		H = HashCombineFast(H, (uint32)R->Footprints.Num());
		for (const ADCRoomModule::FFootprint& F : R->Footprints)
		{
			H = HashCombineFast(H, GetTypeHash(FIntVector4(FMath::RoundToInt(F.Centre.X), FMath::RoundToInt(F.Centre.Y), FMath::RoundToInt(F.Half.X), FMath::RoundToInt(F.Half.Y))));
			H = HashCombineFast(H, (uint32)FMath::RoundToInt(F.Yaw));
		}
	}
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const ADCVehicle* C = Slots[i].Car.Get();
		H = HashCombineFast(H, (C && C->IsParked()) ? (uint32)(i + 1) * 31u : 0u);
	}
	return H;
}

void ADCWorldDirector::EnsureNav()
{
	const uint32 Sig = ComputeNavSignature();
	if (!bNavBuilt || Sig != NavSignature) { BuildNav(); }
}

void ADCWorldDirector::BuildNav()
{
	using namespace DCSite;
	const double T0 = FPlatformTime::Seconds();
	bNavBuilt = true;
	NavSignature = ComputeNavSignature();
	++NavRevision;
	bDebugDirty = true;
	// Rooms appeared on the grid: framing follows; beds follow the Bunkroom if it moved.
	if (const ADCRoomModule* Lounge = FindRoom(EDCRoomType::Lounge))
	{
		if (Lounge->GridCell != SleepCell) { SleepCell = Lounge->GridCell; SleepSpots.Reset(); SleepYaws.Reset(); SleepOwner.Reset(); }
	}
	UpdateBounds();

	Nav.Begin(OutdoorWalkRect().ExpandBy(60.f));

	TMap<FIntPoint, const ADCRoomModule*> WalkableAt;
	for (const TWeakObjectPtr<ADCRoomModule>& RW : Rooms)
	{
		const ADCRoomModule* R = RW.Get();
		if (IsWalkable(R) && R->FloorQuad.Num() == 4) { WalkableAt.Add(R->GridCell, R); }
	}

	const float HalfT = Grid::WallT * 0.5f;
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		const ADCRoomModule* R = Rooms[i].Get();
		if (!R || !R->bPlaced || R->FloorQuad.Num() != 4) { continue; }
		const FVector2D Q[4] = { R->FloorQuad[0], R->FloorQuad[1], R->FloorQuad[2], R->FloorQuad[3] };
		if (!IsWalkable(R))
		{
			// A building site (or a wing mid-reveal): nobody walks through it.
			if (IsSolidOutdoors(R)) { Nav.BlockQuad(Q); }
			continue;
		}
		Nav.SetRegion(Q, i);
		// Walls: every edge is solid except for the doorway the builder cut (DCHideoutGrid.cpp): one centred on each
		// edge shared with another finished room, and the front door in the Office's front (-Y) wall.
		for (int32 E = 0; E < 4; ++E)
		{
			const FVector2D A = Q[E], B = Q[(E + 1) & 3];
			const float L = FVector2D::Distance(A, B);
			const bool bShared = WalkableAt.Contains(Grid::Neighbour(R->GridCell, E));
			const bool bFront = !bShared && E == 0 && R->GridCell == Grid::OfficeCell();
			const float DoorW = bShared ? Grid::DoorW : (bFront ? FrontDoorW : 0.f);
			// a partition straddles the shared edge; an outer wall stands just inside it
			const float In = bShared ? HalfT : Grid::WallT, Out = bShared ? HalfT : 4.f;
			if (DoorW <= 0.f) { Nav.BlockWall(A, B, -HalfT, L + HalfT, In, Out); continue; }
			Nav.BlockWall(A, B, -HalfT, L * 0.5f - DoorW * 0.5f, In, Out);
			Nav.BlockWall(A, B, L * 0.5f + DoorW * 0.5f, L + HalfT, In, Out);
			if (bFront || E == 1 || E == 2) // a shared doorway is listed once (by the room on its -side)
			{
				FDCNavDoor D;
				D.Centre = (A + B) * 0.5f;
				D.Along = (B - A).GetSafeNormal();
				D.Width = DoorW;
				D.bFront = bFront;
				Nav.AddDoor(D);
			}
		}
		// Furniture (laid out by the furnisher for this room's shape and doorways).
		const FTransform RT = R->GetActorTransform();
		const float RoomYaw = FMath::DegreesToRadians(RT.Rotator().Yaw);
		for (const ADCRoomModule::FFootprint& F : R->Footprints)
		{
			const FVector C = RT.TransformPositionNoScale(FVector(F.Centre.X, F.Centre.Y, 0.f));
			Nav.BlockBox(FVector2D(C.X, C.Y), F.Half, RoomYaw + FMath::DegreesToRadians(F.Yaw), 14.f);
		}
	}

	// Outdoors: parked cars, the festoon poles by the yard and the lantern posts at the Office's front corners.
	for (const FCarSlot& S : Slots)
	{
		const ADCVehicle* C = S.Car.Get();
		if (!C || !C->IsParked()) { continue; }
		const FBox2D B = C->GetFootprint();
		Nav.BlockBox(B.GetCenter(), B.GetExtent(), 0.f, 10.f);
	}
	for (const FVector2D& P : YardFestoonPoles()) { Nav.BlockDisc(P, 10.f + FDCNavGrid::Radius); }
	{
		FVector2D Q[4]; Grid::Corners(Grid::OfficeCell(), Q);
		const FVector2D Front = (Q[1] - Q[0]).GetSafeNormal();
		const FVector2D Outward(Front.Y, -Front.X);
		Nav.BlockDisc(Q[0] + Outward * 70.f - Front * 30.f, 13.f + FDCNavGrid::Radius);
		Nav.BlockDisc(Q[1] + Outward * 70.f + Front * 30.f, 13.f + FDCNavGrid::Radius);
	}
	Nav.Finish();
	NavBuildMs = (float)((FPlatformTime::Seconds() - T0) * 1000.0);
	UE_LOG(LogDC, Log, TEXT("WorldDirector nav: %dx%d tiles, %d walkable in %d areas, %d doorways, built in %.2f ms"),
		Nav.SizeX(), Nav.SizeY(), Nav.NumFree(), Nav.NumComponents(), Nav.GetDoors().Num(), NavBuildMs);
}

void ADCWorldDirector::PlanPath(const FVector& From, const FVector& To, TArray<FVector>& Out)
{
	EnsureNav();
	TArray<FVector2D> Pts;
	if (!Nav.FindPath(FVector2D(From.X, From.Y), FVector2D(To.X, To.Y), Pts)) { Out.Add(To); return; } // no map yet: best effort
	for (const FVector2D& P : Pts) { Out.Add(FVector(P.X, P.Y, To.Z)); }
}

bool ADCWorldDirector::IsOnNavMap(const FVector& P) const
{
	return Nav.IsBuilt() && Nav.Contains(FVector2D(P.X, P.Y));
}

void ADCWorldDirector::SteerPawn(const ADCCriminalPawn* Self, const FVector& Target, bool bCrowd, FVector& Dir, float& Scale) const
{
	// Dir comes in as the unit direction to the next waypoint. Two adjustments, both local and cheap:
	//  1. the others: never overlap, queue behind someone going our way, pass anyone else on a consistent side;
	//  2. the walls: whatever (1) asked for, the next step stays on free tiles.
	const FVector Feet = Self->GetFeet();
	const FVector2D Pos(Feet.X, Feet.Y);
	const FVector2D Fwd(Dir.X, Dir.Y);
	const FVector2D Right(-Fwd.Y, Fwd.X);
	FVector2D Want = Fwd;
	if (bCrowd)
	{
		constexpr float Personal = 62.f, Look = 125.f;
		FVector2D Push = FVector2D::ZeroVector;
		for (const ADCCriminalPawn* O : Pawns)
		{
			if (!O || O == Self || O->IsAway() || O->IsHidden() || O->IsSleeping()) { continue; }
			const FVector OF = O->GetFeet();
			FVector2D D(OF.X - Pos.X, OF.Y - Pos.Y);
			float Dist = D.Size();
			if (Dist > Look) { continue; }
			if (Dist < 1.f) { D = (Self->GetCriminalId() < O->GetCriminalId()) ? -Right : Right; Dist = 1.f; }
			const FVector2D N = D / Dist;
			if (Dist < Personal) { Push -= N * (1.f - Dist / Personal) * 1.7f; }
			const float Ahead = FVector2D::DotProduct(N, Fwd);
			if (Ahead < 0.3f) { continue; }
			const FVector OV = O->GetVelocity();
			const bool bSameWay = FVector2D::DotProduct(FVector2D(OV.X, OV.Y), Fwd) > 55.f;
			// Step round: away from the side they're on, or to our right if they're dead ahead (two pawns meeting
			// head-on both pick their own right, so they never mirror each other).
			const float Lat = FVector2D::DotProduct(D, Right);
			const float Side = Lat > 10.f ? -1.f : 1.f;
			const float W = (1.f - Dist / Look) * Ahead;
			Push += Right * Side * W * (bSameWay ? 0.7f : 1.6f);
			if (bSameWay) { Scale = FMath::Min(Scale, FMath::Clamp((Dist - 56.f) / 50.f, 0.f, 1.f)); }
		}
		Want = (Fwd + Push).GetSafeNormal();
		if (Want.IsNearlyZero()) { Want = Fwd; }
	}

	// Walls only bind while we're walking between two free tiles (stepping off a bed, or up to a desk, cuts across
	// the padding around the furniture on purpose).
	if (Nav.IsBuilt() && Nav.IsFree(Pos) && Nav.IsFree(FVector2D(Target.X, Target.Y)))
	{
		constexpr float Probe = 34.f;
		auto Open = [&](const FVector2D& V) { return Nav.IsFree(Pos + V * Probe) && Nav.IsFree(Pos + V * (Probe * 0.5f)); };
		if (!Open(Want))
		{
			bool bFound = false;
			// nearest open heading: first around the way we wanted to go, then around the straight line
			for (const FVector2D& Base : { Want, Fwd })
			{
				for (float Deg : { 0.f, 25.f, -25.f, 50.f, -50.f, 75.f, -75.f, 100.f, -100.f })
				{
					const FVector2D V = Base.GetRotated(Deg);
					if (Open(V)) { Want = V; bFound = true; break; }
				}
				if (bFound) { break; }
			}
			if (!bFound) { Want = Fwd; Scale = FMath::Min(Scale, 0.5f); }
		}
	}
	Dir = FVector(Want.X, Want.Y, 0.f);
}

FVector ADCWorldDirector::DriveMouth()
{
	return FVector(DCSite::DriveX, DCSite::YardRect().Min.Y + 20.f, 0.f);
}

void ADCWorldDirector::LaneWalkPoints(TArray<FVector>& Out, bool bLeaving) const
{
	// From the bottom of the driveway along the house-side verge of the lane, towards +X (where cars go too).
	const float VergeY = DCSite::LaneY + DCSite::LaneW * 0.5f + 40.f;
	const FVector Bottom(DCSite::DriveX + 60.f, VergeY, 0.f);
	const FVector Far(DCSite::DriveX + 1300.f, VergeY, 0.f);
	if (bLeaving) { Out.Add(Bottom); Out.Add(Far); }
	else { Out.Add(Far); Out.Add(Bottom); }
}

// ─── Idle life ───────────────────────────────────────────────────────────────

void ADCWorldDirector::GetAnchors(const ADCRoomModule* Room, TArray<FAnchor>& Out)
{
	if (!Room) { return; }
	const FBox RB = Room->GetWorldBounds();
	const float FloorZ = Room->GetActorLocation().Z;
	const FBox Inner(FVector(RB.Min.X + 60.f, RB.Min.Y + 60.f, RB.Min.Z), FVector(RB.Max.X - 60.f, RB.Max.Y - 60.f, RB.Max.Z));
	for (const TWeakObjectPtr<ADCCriminalSpawnZone>& Z : Zones)
	{
		if (!Z.IsValid() || RoomAt(Z->GetActorLocation()) != Room) { continue; }
		for (int32 k = 0; k < 4; ++k)
		{
			// a free bit of floor (not inside the furniture)
			FAnchor A;
			bool bFree = false;
			for (int32 Try = 0; Try < 8 && !bFree; ++Try)
			{
				A.Point = Z->GetRandomPoint(Rng);
				A.Point.Z = FloorZ;
				bFree = !Room->IsFloorBlocked(A.Point, 30.f);
			}
			if (bFree) { Out.Add(A); }
		}
	}
	for (const TWeakObjectPtr<ADCStation>& SW : Stations)
	{
		const ADCStation* S = SW.Get();
		if (!S || !S->IsStationEnabled() || RoomAt(S->GetActorLocation()) != Room) { continue; }
		// Stand in front of the click box face that looks towards the room centre.
		const FTransform T = S->GetActorTransform();
		const FVector E = S->ClickBox->GetScaledBoxExtent();
		const FVector D = T.InverseTransformVectorNoScale(RB.GetCenter() - S->GetActorLocation());
		FVector Local = FVector::ZeroVector;
		if (FMath::Abs(D.X) / FMath::Max(1.f, E.X) > FMath::Abs(D.Y) / FMath::Max(1.f, E.Y)) { Local.X = FMath::Sign(D.X) * (E.X + 48.f); }
		else { Local.Y = FMath::Sign(D.Y) * (E.Y + 48.f); }
		FVector P = T.TransformPositionNoScale(Local);
		P.X = FMath::Clamp(P.X, Inner.Min.X, Inner.Max.X);
		P.Y = FMath::Clamp(P.Y, Inner.Min.Y, Inner.Max.Y);
		P.Z = FloorZ;
		if (Room->IsFloorBlocked(P, 25.f) || Room->UseSpots.Num() > 0) { continue; } // furnished rooms use their own spots
		FAnchor A;
		A.Point = P + FVector(Rng.FRandRange(-15.f, 15.f), Rng.FRandRange(-15.f, 15.f), 0.f);
		A.Yaw = (S->GetActorLocation() - P).GetSafeNormal2D().Rotation().Yaw;
		A.bHasYaw = true;
		A.bStation = true;
		Out.Add(A);
	}
	// Furnished rooms: busy spots at the furniture (the board, the desk, the stove...).
	const FTransform RT = Room->GetActorTransform();
	for (const ADCRoomModule::FUseSpot& U : Room->UseSpots)
	{
		FAnchor A;
		A.Point = RT.TransformPositionNoScale(FVector(U.Point.X, U.Point.Y, 0.f));
		A.Point.Z = FloorZ;
		A.Yaw = RT.Rotator().Yaw + U.Yaw;
		A.bHasYaw = true;
		A.bStation = true;
		Out.Add(A);
	}
}

bool ADCWorldDirector::IsSpotTaken(const FVector& P, const ADCCriminalPawn* Except) const
{
	for (const ADCCriminalPawn* O : Pawns)
	{
		if (!O || O == Except || O->IsAway() || O->IsHidden()) { continue; }
		if (FVector::Dist2D(O->GetGoalLocation(), P) < 75.f || FVector::Dist2D(O->GetFeet(), P) < 60.f) { return true; }
	}
	return false;
}

bool ADCWorldDirector::IsSleeper(const ADCCriminalPawn* P) const
{
	// About one in four stays up all night.
	const uint32 H = ::GetTypeHash(P->GetCriminalId() * 2654435761u);
	return (H % 4u) != 1u;
}

void ADCWorldDirector::BuildSleepSpots()
{
	SleepSpots.Reset();
	SleepYaws.Reset();
	SleepOwner.Reset();
	const ADCRoomModule* Lounge = FindRoom(EDCRoomType::Lounge);
	if (!Lounge) { return; }
	// Preferred: bed anchors from the scene (any component in the Bunkroom tagged "DC.Bed": location = mattress top
	// centre, +X = head-to-foot direction).
	TArray<USceneComponent*> Comps;
	Lounge->GetComponents<USceneComponent>(Comps);
	for (const USceneComponent* C : Comps)
	{
		if (C && C->ComponentHasTag(TEXT("DC.Bed")))
		{
			SleepSpots.Add(C->GetComponentLocation());
			SleepYaws.Add(C->GetComponentRotation().Yaw);
		}
	}
	if (SleepSpots.Num() > 0)
	{
		SleepOwner.Init(INDEX_NONE, SleepSpots.Num());
		UE_LOG(LogDC, Log, TEXT("WorldDirector: %d beds"), SleepSpots.Num());
		return;
	}
	const ADCCriminalSpawnZone* Zone = nullptr;
	for (const TWeakObjectPtr<ADCCriminalSpawnZone>& Z : Zones)
	{
		if (Z.IsValid() && (Z->RoomType == EDCRoomType::Lounge || RoomAt(Z->GetActorLocation()) == Lounge)) { Zone = Z.Get(); break; }
	}
	if (!Zone) { return; }
	const FVector E = Zone->Zone->GetScaledBoxExtent();
	const FTransform T = Zone->GetActorTransform();
	// Rows of "beds" along the zone's long axis; the body lies along the short axis.
	const bool bLongX = E.X >= E.Y;
	const float Long = bLongX ? E.X : E.Y, Short = bLongX ? E.Y : E.X;
	const int32 Cols = FMath::Clamp(FMath::FloorToInt(Long * 2.f / 85.f), 1, 6);
	const int32 Rows = Short * 2.f > 260.f ? 2 : 1;
	for (int32 r = 0; r < Rows; ++r)
	{
		for (int32 c = 0; c < Cols; ++c)
		{
			const float U = -Long + (c + 0.5f) * (Long * 2.f / Cols);
			const float V = Rows == 1 ? 0.f : (r == 0 ? -Short * 0.5f : Short * 0.5f);
			const FVector Local = bLongX ? FVector(U, V, -E.Z) : FVector(V, U, -E.Z);
			FVector P = T.TransformPosition(Local);
			P.Z = Lounge->GetActorLocation().Z;
			SleepSpots.Add(P);
			SleepYaws.Add(T.GetRotation().Rotator().Yaw + (bLongX ? 90.f : 0.f) + ((r + c) % 2 ? 180.f : 0.f));
		}
	}
	SleepOwner.Init(INDEX_NONE, SleepSpots.Num());
}

int32 ADCWorldDirector::AcquireSleepSlot(ADCCriminalPawn* P)
{
	if (SleepSpots.Num() == 0) { BuildSleepSpots(); }
	for (int32 i = 0; i < SleepOwner.Num(); ++i) { if (SleepOwner[i] == P->GetCriminalId()) { return i; } }
	TArray<int32> Free;
	for (int32 i = 0; i < SleepOwner.Num(); ++i) { if (SleepOwner[i] == INDEX_NONE) { Free.Add(i); } }
	if (Free.Num() == 0) { return INDEX_NONE; }
	const int32 Slot = Free[Rng.RandHelper(Free.Num())];
	SleepOwner[Slot] = P->GetCriminalId();
	return Slot;
}

bool ADCWorldDirector::TrySendToBed(ADCCriminalPawn* P, bool bInstant)
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->IsNight() || !IsSleeper(P) || !IsRoomUsable(EDCRoomType::Lounge)) { return false; }
	const int32 Slot = AcquireSleepSlot(P);
	if (Slot == INDEX_NONE) { return false; }
	P->SetSleepSlot(Slot);
	const FVector Spot = SleepSpots[Slot];
	const float Yaw = SleepYaws.IsValidIndex(Slot) ? SleepYaws[Slot] : 0.f;
	TArray<FVector> Route;
	if (bInstant) { P->PlaceAt(Spot); }
	else { PlanPath(P->GetFeet(), Spot, Route); }
	P->SetIdleGoal(Route, EDCPawnActivity::Sleep, Yaw, 0.f);
	UE_LOG(LogDC, Log, TEXT("WorldDirector: %d -> bed %d %s%s"), P->GetCriminalId(), Slot, *Spot.ToCompactString(), bInstant ? TEXT(" (instant)") : TEXT(""));
	return true;
}

FVector ADCWorldDirector::PickSpawnPoint(ADCCriminalPawn* P)
{
	// Hangouts (rooms with a spawn zone) first, then any usable room's station fronts.
	TArray<FAnchor> Anchors;
	for (const TWeakObjectPtr<ADCCriminalSpawnZone>& Z : Zones)
	{
		ADCRoomModule* R = Z.IsValid() ? RoomAt(Z->GetActorLocation()) : nullptr;
		if (IsRoomUsable(R)) { GetAnchors(R, Anchors); }
	}
	Anchors.RemoveAll([](const FAnchor& A) { return A.bStation; });
	if (Anchors.Num() == 0)
	{
		for (const TWeakObjectPtr<ADCRoomModule>& R : Rooms) { if (IsRoomUsable(R.Get())) { GetAnchors(R.Get(), Anchors); } }
	}
	if (Anchors.Num() == 0)
	{
		const ADCRoomModule* Office = FindRoom(EDCRoomType::Office);
		return Office ? Office->GetFocusPoint() * FVector(1, 1, 0) + FVector(0, 0, Office->GetActorLocation().Z) : FVector::ZeroVector;
	}
	for (int32 Try = 0; Try < 10; ++Try)
	{
		const FAnchor& A = Anchors[Rng.RandHelper(Anchors.Num())];
		if (!IsSpotTaken(A.Point, P)) { return A.Point; }
	}
	return Anchors[Rng.RandHelper(Anchors.Num())].Point;
}

void ADCWorldDirector::AssignIdleGoal(ADCCriminalPawn* P)
{
	if (!P || P->GetMode() != EDCPawnMode::Wander) { return; }
	if (TrySendToBed(P, false)) { return; }
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	const bool bNight = Run && Run->IsNight();
	const ADCRoomModule* Here = RoomAt(P->GetFeet());

	// Pick a room: stay put more often than not; night owls favour screens.
	TArray<TPair<ADCRoomModule*, float>> Cands;
	float Total = 0.f;
	for (const TWeakObjectPtr<ADCRoomModule>& RW : Rooms)
	{
		ADCRoomModule* R = RW.Get();
		if (!IsRoomUsable(R)) { continue; }
		float W = 1.f;
		if (R == Here) { W *= 2.5f; }
		if (R->RoomType == EDCRoomType::Lounge || R->RoomType == EDCRoomType::Office) { W *= 1.5f; }
		if (bNight && (R->RoomType == EDCRoomType::OpsRoom || R->RoomType == EDCRoomType::ServerRoom)) { W *= 2.f; }
		if (bNight && R->RoomType == EDCRoomType::Lounge) { W *= 0.3f; } // let the sleepers sleep
		if (P->IsInjured() && R->RoomType == EDCRoomType::Clinic) { W *= 4.f; }
		Cands.Add({ R, W });
		Total += W;
	}
	if (Cands.Num() == 0) { return; }
	float Pick = Rng.FRandRange(0.f, Total);
	ADCRoomModule* Target = Cands.Last().Key;
	for (const auto& C : Cands) { Pick -= C.Value; if (Pick <= 0.f) { Target = C.Key; break; } }

	TArray<FAnchor> Anchors;
	GetAnchors(Target, Anchors);
	if (Anchors.Num() == 0 && Here && Here != Target) { GetAnchors(Here, Anchors); }
	if (Anchors.Num() == 0) { return; }
	for (int32 Try = 0; Try < 6; ++Try)
	{
		const FAnchor& A = Anchors[Rng.RandHelper(Anchors.Num())];
		if (IsSpotTaken(A.Point, P)) { continue; }
		if (!Nav.IsReachable(FVector2D(P->GetFeet()), FVector2D(A.Point))) { continue; } // a wing cut off by a building site
		TArray<FVector> Route;
		PlanPath(P->GetFeet(), A.Point, Route);
		const float Hold = A.bStation ? Rng.FRandRange(6.f, 14.f) : Rng.FRandRange(2.5f, 7.f) * (bNight ? 1.8f : 1.f);
		P->SetIdleGoal(Route, A.bStation ? EDCPawnActivity::Work : EDCPawnActivity::Stand, A.bHasYaw ? A.Yaw : NAN, Hold);
		return;
	}
}

void ADCWorldDirector::UpdateClockLife()
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->HasActiveRun()) { return; }
	const bool bNight = Run->IsNight();
	// Release beds nobody is using any more.
	for (int32 i = 0; i < SleepOwner.Num(); ++i)
	{
		if (SleepOwner[i] == INDEX_NONE) { continue; }
		const ADCCriminalPawn* P = FindPawn(SleepOwner[i]);
		if (!P || P->GetSleepSlot() != i || !(P->IsSleeping() || P->IsHeadingToSleep())) { SleepOwner[i] = INDEX_NONE; }
	}
	if (bNight != bWasNight)
	{
		bWasNight = bNight;
		for (ADCCriminalPawn* P : Pawns)
		{
			if (!P || P->GetMode() != EDCPawnMode::Wander) { continue; }
			if (bNight && IsSleeper(P)) { P->InterruptIdle(Rng.FRandRange(0.f, 6.f)); }      // head to bed soon
			else if (!bNight && (P->IsSleeping() || P->IsHeadingToSleep())) { P->InterruptIdle(Rng.FRandRange(0.5f, 8.f)); } // staggered wake-up
		}
	}
}

// ─── Run events ──────────────────────────────────────────────────────────────

bool ADCWorldDirector::IsDetectingNewRun()
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->HasActiveRun()) { return false; }
	if (!bHadRun) { return true; }
	return Run->GetSeed() != LastSeed || Run->GetRunTime() < LastRunTime - 0.01;
}

void ADCWorldDirector::OnPhaseChanged(EDCGamePhase Phase)
{
	const EDCGamePhase Prev = LastPhase;
	LastPhase = Phase;
	// New run or Continue (loaded save): rebuild the whole world state from the run.
	if (Phase == EDCGamePhase::Playing && (Prev != EDCGamePhase::Playing || IsDetectingNewRun()))
	{
		ResetForNewRun();
		return;
	}
	SyncPawns();
}

void ADCWorldDirector::OnRunStateChanged()
{
	if (IsDetectingNewRun()) { ResetForNewRun(); return; }
	if (const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this)) { LastRunTime = Run->GetRunTime(); }
	SyncRooms();
	SyncPawns();
	SyncCars(false); // a Garage/Office upgrade or perk raised the mission cap: a new car drives in
}

void ADCWorldDirector::OnMissionStarted(const FDCMission& Mission)
{
	if (SlotOfMission(Mission.Id) != INDEX_NONE || Walkers.Contains(Mission.Id)) { return; }
	int32 Free = INDEX_NONE;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const ADCVehicle* C = Slots[i].Car.Get();
		if (Slots[i].Phase == ECarPhase::Parked && C && C->IsParked()) { Free = i; break; }
	}
	if (Free == INDEX_NONE)
	{
		// More jobs than cars: this crew sets off on foot down the driveway.
		Walkers.Add(Mission.Id, Mission.CrewIds);
		float Delay = 0.2f;
		for (int32 Id : Mission.CrewIds)
		{
			ADCCriminalPawn* P = FindPawn(Id);
			if (P && !P->IsAway()) { WalkOffDownLane(P, Delay, false); Delay += 0.6f; }
		}
		UE_LOG(LogDC, Log, TEXT("WorldDirector: mission %d has no free car, crew walk"), Mission.Id);
		return;
	}
	FCarSlot& S = Slots[Free];
	S.MissionId = Mission.Id;
	S.Crew = Mission.CrewIds;
	SetPhase(S, ECarPhase::Boarding);
	float Delay = 0.15f;
	for (int32 Seat = 0; Seat < S.Crew.Num(); ++Seat)
	{
		ADCCriminalPawn* P = FindPawn(S.Crew[Seat]);
		if (!P || P->IsAway()) { continue; }
		TArray<FVector> Route;
		PlanPath(P->GetFeet(), DCSite::BoardingPoint(Free, Seat), Route);
		P->WalkOutAlong(Route, Delay, false);
		Delay += 0.45f;
	}
	UE_LOG(LogDC, Log, TEXT("WorldDirector: mission %d -> car %d, crew %d boarding"), Mission.Id, Free, S.Crew.Num());
}

void ADCWorldDirector::OnMissionCompleted(const FDCMissionReport& Report)
{
	// Cars bring the survivors home (TickCars); the arrested never come back.
	for (const FDCCrewMemberResult& M : Report.Members)
	{
		if (!M.bArrested) { continue; }
		if (ADCCriminalPawn* P = FindPawn(M.CriminalId))
		{
			if (P->IsWalkingOut()) { P->CancelWalkOut(); } // caught before the car even left: SyncPawns walks them off
			else { P->MarkLeaveForever(); }
		}
	}
	TickWalkers();
}

void ADCWorldDirector::OnRoomChanged(const FDCRoom& Room)
{
	if (UDCDevTools::IsBuildAll()) { return; }
	ADCRoomModule* M = FindRoom(Room.Type);
	if (!M) { return; }
	if (Room.State == EDCRoomState::Building)
	{
		// Scaffolding + work lights (and the build timer) for the whole build.
		if (!M->IsRevealPlaying() && !RevealQueue.Contains(Room.Type)) { M->ApplyState(EDCRoomState::Building, Room.Level); }
		return;
	}
	if (Room.State == EDCRoomState::Built && (M->GetState() != EDCRoomState::Built || Room.Level > M->GetLevel()))
	{
		if (M->GetLevel() >= 1 && !RevealQueue.Contains(Room.Type)) { M->PlayUpgradeReveal(Room.Level); }
		else { RevealQueue.AddUnique(Room.Type); }
		return;
	}
	if (!RevealQueue.Contains(Room.Type) && RevealingRoom.Get() != M) { M->ApplyState(Room.State, Room.Level); }
}

void ADCWorldDirector::OnCriminalLevelUp(int32 CriminalId, int32 NewLevel)
{
	if (ADCCriminalPawn* P = FindPawn(CriminalId)) { P->PlayLevelUp(NewLevel); }
}

void ADCWorldDirector::OnCriminalHired(const FDCCriminal& C)
{
	if (ADCCriminalPawn* Existing = FindPawn(C.Id))
	{
		Existing->SetAway();
		WalkInFromLane(Existing, 0.4f);
		return;
	}
	if (Pawns.Num() < MaxPawns) { SpawnPawn(C, true); }
}

void ADCWorldDirector::OnPanelChanged(EDCPanel Panel)
{
	LastPanel = Panel;
	if (Panel == EDCPanel::Missions || Panel == EDCPanel::MissionPlanner)
	{
		if (const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this))
		{
			for (const FDCMission& M : Run->GetMissions()) { SeenMissionIds.Add(M.Id); }
		}
	}
}

// ─── Cars & mission choreography ─────────────────────────────────────────────

ADCVehicle* ADCWorldDirector::GetCar(int32 Slot) const
{
	return Slots.IsValidIndex(Slot) ? Slots[Slot].Car.Get() : nullptr;
}

ADCVehicle* ADCWorldDirector::SpawnCar(int32 Slot)
{
	UWorld* W = GetWorld();
	if (!W) { return nullptr; }
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADCVehicle* Car = W->SpawnActor<ADCVehicle>(ADCVehicle::StaticClass(), DCSite::ParkingSlot(Slot), SP);
	if (!Car) { return nullptr; }
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	Car->Setup(Slot + (Run ? (Run->GetSeed() & 3) : 0));
	return Car;
}

void ADCWorldDirector::ClearCars()
{
	for (FCarSlot& S : Slots) { if (ADCVehicle* C = S.Car.Get()) { C->Destroy(); } }
	Slots.Reset();
	Walkers.Reset();
}

void ADCWorldDirector::SetPhase(FCarSlot& S, ECarPhase Phase)
{
	S.Phase = Phase;
	S.PhaseTime = 0.f;
	S.Timer = 0.f;
}

void ADCWorldDirector::SyncCars(bool bInstant)
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->HasActiveRun()) { return; }
	const int32 Want = FMath::Clamp(Run->GetMaxActiveMissions(), 1, DCSite::MaxParkingSlots);
	while (Slots.Num() < Want)
	{
		const int32 I = Slots.Num();
		FCarSlot& S = Slots.AddDefaulted_GetRef();
		S.Car = SpawnCar(I);
		if (ADCVehicle* C = S.Car.Get())
		{
			if (bInstant) { C->ParkAt(DCSite::ParkingSlot(I)); SetPhase(S, ECarPhase::Parked); }
			else
			{
				C->DriveIn(I);
				SetPhase(S, ECarPhase::Arriving);
				UE_LOG(LogDC, Log, TEXT("WorldDirector: new car for slot %d drives in"), I);
			}
		}
	}
	// Fewer slots (shouldn't happen): retire trailing idle cars.
	while (Slots.Num() > Want && Slots.Last().Phase == ECarPhase::Parked)
	{
		if (ADCVehicle* C = Slots.Last().Car.Get()) { C->Destroy(); }
		Slots.Pop();
	}
}

int32 ADCWorldDirector::SlotOfMission(int32 MissionId) const
{
	for (int32 i = 0; i < Slots.Num(); ++i) { if (Slots[i].MissionId == MissionId && Slots[i].Phase != ECarPhase::Parked) { return i; } }
	return INDEX_NONE;
}

bool ADCWorldDirector::IsPawnTracked(int32 CriminalId) const
{
	for (const FCarSlot& S : Slots)
	{
		if (S.Phase == ECarPhase::Parked || S.Phase == ECarPhase::Arriving) { continue; }
		if (S.Phase == ECarPhase::Unloading)
		{
			const int32 Idx = S.Riders.IndexOfByKey(CriminalId);
			if (Idx != INDEX_NONE && Idx >= S.NextRider) { return true; }
			continue;
		}
		if (S.Crew.Contains(CriminalId)) { return true; }
	}
	for (const TPair<int32, TArray<int32>>& W : Walkers) { if (W.Value.Contains(CriminalId)) { return true; } }
	return false;
}

void ADCWorldDirector::AssignRunningMissionsInstant()
{
	// Loaded save / new world with missions already out: their cars are away, their crews with them.
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run) { return; }
	TArray<const FDCMission*> Running;
	for (const FDCMission& M : Run->GetMissions()) { if (M.bInProgress) { Running.Add(&M); } }
	Running.Sort([](const FDCMission& A, const FDCMission& B) { return A.Id < B.Id; });
	for (const FDCMission* M : Running)
	{
		int32 Free = INDEX_NONE;
		for (int32 i = 0; i < Slots.Num(); ++i) { if (Slots[i].Phase == ECarPhase::Parked && Slots[i].Car.IsValid()) { Free = i; break; } }
		for (int32 Id : M->CrewIds) { if (ADCCriminalPawn* P = FindPawn(Id)) { P->SetAway(); } }
		if (Free == INDEX_NONE) { Walkers.Add(M->Id, M->CrewIds); continue; }
		FCarSlot& S = Slots[Free];
		S.MissionId = M->Id;
		S.Crew = M->CrewIds;
		SetPhase(S, ECarPhase::Away);
		S.Car->SetGone();
	}
}

void ADCWorldDirector::WalkInFromLane(ADCCriminalPawn* P, float Delay)
{
	if (!P) { return; }
	TArray<FVector> Lane; LaneWalkPoints(Lane, false);
	TArray<FVector> Route;
	for (int32 i = 1; i < Lane.Num(); ++i) { Route.Add(Lane[i]); }
	Route.Add(DriveMouth());
	PlanPath(DriveMouth(), PickSpawnPoint(P), Route);
	P->WalkInAlong(Lane[0], Route, Delay);
}

void ADCWorldDirector::WalkOffDownLane(ADCCriminalPawn* P, float Delay, bool bForever)
{
	if (!P) { return; }
	TArray<FVector> Route;
	PlanPath(P->GetFeet(), DriveMouth(), Route);
	LaneWalkPoints(Route, true);
	P->WalkOutAlong(Route, Delay, bForever);
}

void ADCWorldDirector::TickWalkers()
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run) { return; }
	for (auto It = Walkers.CreateIterator(); It; ++It)
	{
		const FDCMission* M = Run->FindMission(It->Key);
		if (M && M->bInProgress) { continue; }
		float Delay = 0.3f;
		for (int32 Id : It->Value)
		{
			ADCCriminalPawn* P = FindPawn(Id);
			const FDCCriminal* C = Run->FindCriminal(Id);
			if (!P || !C || P->IsLeavingForever()) { continue; }
			if (P->IsWalkingOut()) { P->CancelWalkOut(); continue; }
			if (P->IsAway()) { WalkInFromLane(P, Delay); Delay += 0.9f; }
		}
		It.RemoveCurrent();
	}
}

void ADCWorldDirector::TickCars(float Dt)
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->HasActiveRun()) { return; }
	for (int32 SI = 0; SI < Slots.Num(); ++SI)
	{
		FCarSlot& S = Slots[SI];
		ADCVehicle* Car = S.Car.Get();
		S.PhaseTime += Dt;
		if (!Car)
		{
			// Lost the car somehow: respawn it parked.
			S.Car = SpawnCar(SI);
			Car = S.Car.Get();
			if (!Car) { continue; }
			Car->ParkAt(DCSite::ParkingSlot(SI));
		}
		const FDCMission* M = S.MissionId != INDEX_NONE ? Run->FindMission(S.MissionId) : nullptr;
		const bool bRunning = M && M->bInProgress;
		switch (S.Phase)
		{
		case ECarPhase::Arriving:
			if (Car->IsParked()) { SetPhase(S, ECarPhase::Parked); }
			break;

		case ECarPhase::Boarding:
		{
			if (!bRunning)
			{
				// Done (or cancelled) before the car left: everyone back inside, car stays.
				float Delay = 0.2f;
				for (int32 Seat = 0; Seat < S.Crew.Num(); ++Seat)
				{
					ADCCriminalPawn* P = FindPawn(S.Crew[Seat]);
					if (!P || P->IsLeavingForever() || !Run->FindCriminal(S.Crew[Seat])) { continue; }
					if (P->IsWalkingOut()) { P->CancelWalkOut(); }
					else if (P->IsAway())
					{
						const FVector BP = DCSite::BoardingPoint(SI, Seat);
						TArray<FVector> Route; PlanPath(BP, PickSpawnPoint(P), Route);
						P->WalkInAlong(BP, Route, Delay);
						Delay += 0.5f;
					}
				}
				Car->SetDoorsOpen(false);
				S.MissionId = INDEX_NONE;
				S.Crew.Reset();
				SetPhase(S, ECarPhase::Parked);
				UE_LOG(LogDC, Log, TEXT("WorldDirector: slot %d mission ended before departure, crew turn back"), SI);
				break;
			}
			int32 Aboard = 0;
			bool bDoor[2] = { false, false };
			for (int32 Seat = 0; Seat < S.Crew.Num(); ++Seat)
			{
				ADCCriminalPawn* P = FindPawn(S.Crew[Seat]);
				if (!P || P->IsAway() || P->WantsDestroy()) { ++Aboard; continue; }
				if (!P->IsWalkingOut()) { continue; }
				if (FVector::Dist2D(P->GetFeet(), DCSite::BoardingPoint(SI, Seat)) < 230.f) { bDoor[Seat % 2] = true; }
			}
			// Seat side -1 (world -X) is the car's left: the car faces -Y.
			if (bDoor[0]) { Car->SetDoorsOpen(true, -1); }
			if (bDoor[1]) { Car->SetDoorsOpen(true, 1); }
			const bool bAll = Aboard >= S.Crew.Num();
			if (bAll || S.PhaseTime > 45.f)
			{
				if (!bAll) { for (int32 Id : S.Crew) { if (ADCCriminalPawn* P = FindPawn(Id)) { P->SetAway(); } } }
				S.Timer += Dt;
				if (S.Timer > 0.35f) { Car->SetDoorsOpen(false); }
				if (S.Timer > 1.0f)
				{
					Car->DriveAway(SI);
					SetPhase(S, ECarPhase::Departing);
					UE_LOG(LogDC, Log, TEXT("WorldDirector: slot %d drives off (mission %d)"), SI, S.MissionId);
				}
			}
			break;
		}

		case ECarPhase::Departing:
			if (Car->IsGone()) { SetPhase(S, ECarPhase::Away); }
			break;

		case ECarPhase::Away:
		{
			// Leave so the car parks about when the job finishes. The car runs at game speed (CustomTimeDilation),
			// so its own clock needs the run time left over the dev time scale only.
			const double RealLeft = bRunning ? Run->GetMissionTimeRemaining(S.MissionId) / FMath::Max(0.01f, Run->GetTimeScale()) : 0.0;
			if (!bRunning || RealLeft <= ADCVehicle::EstimateDriveInSeconds())
			{
				Car->DriveIn(SI);
				SetPhase(S, ECarPhase::Returning);
				UE_LOG(LogDC, Log, TEXT("WorldDirector: slot %d heading home (mission %d, %.1fs left)"), SI, S.MissionId, RealLeft);
			}
			break;
		}

		case ECarPhase::Returning:
			if (Car->IsParked() && !bRunning)
			{
				S.Riders.Reset();
				for (int32 Id : S.Crew)
				{
					const FDCCriminal* C = Run->FindCriminal(Id);
					ADCCriminalPawn* P = FindPawn(Id);
					if (C && P && !P->IsLeavingForever() && P->IsAway()) { S.Riders.Add(Id); }
				}
				S.NextRider = 0;
				SetPhase(S, ECarPhase::Unloading);
				S.Timer = 0.45f;
			}
			break;

		case ECarPhase::Unloading:
		{
			S.Timer -= Dt;
			if (S.NextRider < S.Riders.Num())
			{
				if (S.Timer <= 0.f)
				{
					const int32 Id = S.Riders[S.NextRider];
					const int32 Seat = FMath::Max(0, S.Crew.IndexOfByKey(Id));
					Car->SetDoorsOpen(true, Seat % 2 == 0 ? -1 : 1);
					if (ADCCriminalPawn* P = FindPawn(Id))
					{
						const FVector BP = DCSite::BoardingPoint(SI, Seat);
						TArray<FVector> Route; PlanPath(BP, PickSpawnPoint(P), Route);
						P->WalkInAlong(BP, Route, 0.25f);
					}
					++S.NextRider;
					S.Timer = 0.6f;
				}
			}
			else if (S.Timer <= -0.6f)
			{
				Car->SetDoorsOpen(false);
				S.MissionId = INDEX_NONE;
				S.Crew.Reset();
				S.Riders.Reset();
				SetPhase(S, ECarPhase::Parked);
			}
			break;
		}

		case ECarPhase::Parked:
		default:
			break;
		}
	}
	// Retire extra idle slots if the cap ever shrinks.
	SyncCars(false);
}

void ADCWorldDirector::LogSlots(const TCHAR* Tag) const
{
	static const TCHAR* Names[] = { TEXT("Parked"), TEXT("Boarding"), TEXT("Departing"), TEXT("Away"), TEXT("Returning"), TEXT("Unloading"), TEXT("Arriving") };
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FCarSlot& S = Slots[i];
		const ADCVehicle* C = S.Car.Get();
		UE_LOG(LogDC, Display, TEXT("[WorldTest] %s slot %d %s mission %d crew %d car %s %s"), Tag, i, Names[(int32)S.Phase], S.MissionId, S.Crew.Num(),
			C ? (C->IsParked() ? TEXT("parked") : C->IsGone() ? TEXT("gone") : TEXT("driving")) : TEXT("none"), C ? *C->GetActorLocation().ToCompactString() : TEXT(""));
	}
	for (const TPair<int32, TArray<int32>>& W : Walkers) { UE_LOG(LogDC, Display, TEXT("[WorldTest] %s on foot: mission %d crew %d"), Tag, W.Key, W.Value.Num()); }
}

// ─── Sync ────────────────────────────────────────────────────────────────────

void ADCWorldDirector::ResetForNewRun()
{
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->HasActiveRun()) { return; }
	if (Rooms.Num() == 0) { GatherWorld(); }
	bHadRun = true;
	LastSeed = Run->GetSeed();
	LastRunTime = Run->GetRunTime();
	LastPhase = Run->GetPhase();
	bWasNight = Run->IsNight();

	for (ADCCriminalPawn* P : Pawns) { if (P) { P->Destroy(); } }
	Pawns.Reset();
	ClearCars();
	bNavBuilt = false;
	RevealQueue.Reset();
	RevealingRoom = nullptr;
	SeenMissionIds.Reset();
	SleepOwner.Init(INDEX_NONE, SleepSpots.Num());

	SyncRooms();
	UpdateBounds();
	if (PC.IsValid() && PC->GetCameraRig()) { PC->GetCameraRig()->ResetFocus(); }
	for (const FDCCriminal& C : Run->GetRoster())
	{
		if (Pawns.Num() >= MaxPawns) { break; }
		SpawnPawn(C, false);
	}
	SyncCars(true);
	AssignRunningMissionsInstant();
	UE_LOG(LogDC, Log, TEXT("WorldDirector: run synced (seed %d, t=%.0f, %02.0fh), %d pawns"), LastSeed, LastRunTime, Run->GetHourOfDay(), Pawns.Num());
}

void ADCWorldDirector::SyncRooms()
{
	if (UDCDevTools::IsBuildAll()) { return; }
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->HasActiveRun()) { return; }
	for (const TWeakObjectPtr<ADCRoomModule>& RW : Rooms)
	{
		ADCRoomModule* M = RW.Get();
		if (!M || M->IsRevealPlaying() || RevealQueue.Contains(M->RoomType)) { continue; }
		const FDCRoom R = Run->GetRoom(M->RoomType);
		if (R.State != M->GetState() || R.Level != M->GetLevel()) { M->ApplyState(R.State, R.Level); }
	}
	UpdateStations();
}

ADCCriminalPawn* ADCWorldDirector::SpawnPawn(const FDCCriminal& C, bool bWalkIn)
{
	UWorld* W = GetWorld();
	if (!W) { return nullptr; }
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADCCriminalPawn* P = W->SpawnActor<ADCCriminalPawn>(ADCCriminalPawn::StaticClass(), FTransform(FVector(0, 0, 100)), SP);
	if (!P) { return nullptr; }
	Pawns.Add(P);
	P->InitFromCriminal(C, this);
	if (bWalkIn)
	{
		P->SetAway();
		WalkInFromLane(P, 0.4f); // new recruit: walks up the driveway
	}
	else if (C.Status == EDCCriminalStatus::OnMission)
	{
		P->SetAway(); // continued run: still out on the job
	}
	else
	{
		P->PlaceAt(PickSpawnPoint(P));
		TrySendToBed(P, true);
	}
	return P;
}

void ADCWorldDirector::SyncPawns()
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->HasActiveRun()) { return; }
	const TArray<FDCCriminal>& Roster = Run->GetRoster();

	// Gone from the roster: dismissed (walks off down the driveway) or arrested (never comes back).
	for (int32 i = Pawns.Num() - 1; i >= 0; --i)
	{
		ADCCriminalPawn* P = Pawns[i];
		if (!P) { Pawns.RemoveAt(i); continue; }
		const bool bInRoster = Roster.ContainsByPredicate([P](const FDCCriminal& C) { return C.Id == P->GetCriminalId(); });
		if (bInRoster) { continue; }
		if (P->IsAway()) { P->Destroy(); Pawns.RemoveAt(i); continue; }
		if (P->GetMode() == EDCPawnMode::WalkOut) { P->MarkLeaveForever(); continue; }
		if (!P->IsLeavingForever()) { WalkOffDownLane(P, 0.f, true); }
	}

	for (const FDCCriminal& C : Roster)
	{
		ADCCriminalPawn* P = FindPawn(C.Id);
		if (!P)
		{
			if (Pawns.Num() < MaxPawns) { SpawnPawn(C, false); }
			continue;
		}
		P->UpdateFromCriminal(C);
		if (IsPawnTracked(C.Id)) { continue; } // a car / walk-off owns this one
		// Self-heal against missed events: on a job but still idling -> leave; back but still away -> come home.
		const bool bOnJob = C.Status == EDCCriminalStatus::OnMission;
		if (bOnJob && P->GetMode() == EDCPawnMode::Wander)
		{
			const FDCMission* Job = nullptr;
			for (const FDCMission& M : Run->GetMissions()) { if (M.bInProgress && M.CrewIds.Contains(C.Id)) { Job = &M; break; } }
			if (Job && SlotOfMission(Job->Id) == INDEX_NONE && !Walkers.Contains(Job->Id)) { OnMissionStarted(*Job); }
			if (!IsPawnTracked(C.Id) || P->GetMode() == EDCPawnMode::Wander)
			{
				if (Job) { Walkers.FindOrAdd(Job->Id).AddUnique(C.Id); }
				WalkOffDownLane(P, 0.2f, false);
			}
		}
		else if (!bOnJob && P->IsAway() && !P->IsLeavingForever()) { WalkInFromLane(P, 0.5f); }
	}
}

void ADCWorldDirector::UpdateStations()
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	int32 NewPostings = 0, Out = 0, Back = 0;
	if (Run && Run->HasActiveRun())
	{
		for (const FDCMission& M : Run->GetMissions())
		{
			if (M.bInProgress) { ++Out; }
			else if (!SeenMissionIds.Contains(M.Id)) { ++NewPostings; }
		}
		Back = Run->GetUnseenReportCount();
	}
	for (const TWeakObjectPtr<ADCStation>& SW : Stations)
	{
		ADCStation* S = SW.Get();
		if (!S) { continue; }
		const ADCRoomModule* Own = RoomAt(S->GetActorLocation());
		const bool bEnabled = (!Own || IsRoomUsable(Own)) && IsRoomUsable(S->GetRequiredRoom());
		S->SetStationEnabled(bEnabled);
		if (S->StationType == EDCStationType::MissionBoard)
		{
			S->SetBadge(TEXT("bell-dot"), NewPostings > 0 ? FString::Printf(TEXT("%d new"), NewPostings) : FString(), FLinearColor(1.f, 0.7f, 0.2f));
		}
		else if (S->StationType == EDCStationType::GarageDoor)
		{
			if (Back > 0) { S->SetBadge(TEXT("inbox"), FString::Printf(TEXT("%d back"), Back), FLinearColor(0.3f, 0.9f, 0.5f)); }
			else { S->SetBadge(TEXT("car-front"), Out > 0 ? FString::Printf(TEXT("%d out"), Out) : FString(), FLinearColor(0.35f, 0.65f, 1.f)); }
		}
	}
}

void ADCWorldDirector::UpdateBuildTimers()
{
	if (UDCDevTools::IsBuildAll()) { return; }
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->HasActiveRun()) { return; }
	const double Now = Run->GetRunTime();
	for (const FDCRoom& R : Run->GetRooms())
	{
		if (R.State != EDCRoomState::Building) { continue; }
		ADCRoomModule* M = FindRoom(R.Type);
		if (!M || M->IsRevealPlaying()) { continue; }
		const double Len = FMath::Max(0.01, R.BuildEndsAt - R.BuildStartedAt);
		M->SetBuildProgress((float)((Now - R.BuildStartedAt) / Len), R.BuildEndsAt - Now, R.Level + 1);
	}
}

void ADCWorldDirector::UpdateBounds()
{
	ADCCameraRig* Rig = PC.IsValid() ? PC->GetCameraRig() : nullptr;
	if (!Rig) { return; }
	FBox B(ForceInit);
	for (const TWeakObjectPtr<ADCRoomModule>& R : Rooms)
	{
		if (R.IsValid() && (R->GetState() != EDCRoomState::Locked || R->IsRevealPlaying() || UDCDevTools::IsBuildAll())) { B += R->GetWorldBounds(); }
	}
	if (!B.IsValid)
	{
		for (const TWeakObjectPtr<ADCRoomModule>& R : Rooms) { if (R.IsValid()) { B += R->GetWorldBounds(); } }
	}
	if (!B.IsValid) { return; }
	// Frame the house and the yard (so the cars are in shot); pan anywhere over the site.
	const FBox2D Y = DCSite::YardRect();
	FBox Frame = B;
	Frame += FVector(Y.Min.X, Y.Min.Y, 0.f);
	Frame += FVector(Y.Max.X, Y.Max.Y, 0.f);
	FBox Pan = DCSite::CameraBounds();
	Pan += B;
	Rig->SetFraming(Frame);
	Rig->SetBounds(Pan);
	if (!bBoundsInitialized)
	{
		bBoundsInitialized = true;
		Rig->ResetFocus();
		Rig->SnapToTarget();
	}
}

void ADCWorldDirector::UpdateDoors(float Dt)
{
	for (const TWeakObjectPtr<ADCGarageDoor>& DW : Doors)
	{
		ADCGarageDoor* D = DW.Get();
		if (!D) { continue; }
		bool bNear = false;
		for (ADCCriminalPawn* P : Pawns)
		{
			if (P && P->GetActiveDoor() == D && P->IsNearDoor(D->bRollUp ? 520.f : 300.f)) { bNear = true; break; }
		}
		float& Idle = DoorIdleTime.FindOrAdd(D);
		if (bNear) { Idle = 0.f; D->Open(); }
		else
		{
			Idle += Dt;
			if (Idle > 0.9f) { D->Close(); }
		}
	}
}

void ADCWorldDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Rooms.Num() == 0) { GatherWorld(); if (Rooms.Num()) { UpdateBounds(); } }
	if (Rooms.Num()) { EnsureNav(); }

	// One full reveal at a time.
	ADCRoomModule* Revealing = RevealingRoom.Get();
	if (Revealing && !Revealing->IsRevealPlaying())
	{
		RevealingRoom = nullptr;
		Revealing = nullptr;
		UpdateBounds();
		UpdateStations();
	}
	if (!Revealing && RevealQueue.Num() > 0)
	{
		const EDCRoomType T = RevealQueue[0];
		RevealQueue.RemoveAt(0);
		if (ADCRoomModule* M = FindRoom(T))
		{
			const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
			const int32 Level = Run ? FMath::Max(1, Run->GetRoom(T).Level) : FMath::Max(1, M->GetLevel() + 1);
			M->PlayReveal(Level);
			RevealingRoom = M;
			UpdateBounds();
		}
	}

	for (int32 i = Pawns.Num() - 1; i >= 0; --i)
	{
		ADCCriminalPawn* P = Pawns[i];
		if (!P) { Pawns.RemoveAt(i); continue; }
		if (P->WantsDestroy()) { P->Destroy(); Pawns.RemoveAt(i); continue; }
	}
	// Game speed (1x / 2x / 4x): crew, cars and garage doors play at the sim's pace so the choreography keeps up
	// with faster missions. The camera, UI and ambient scene stay on real time. A pause leaves them idling at 1x.
	float WorldSpeed = 1.f;
	if (const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this))
	{
		if (Run->HasActiveRun() && Run->GetPhase() == EDCGamePhase::Playing && !Run->IsPaused() && !Run->IsSpeedPaused())
		{
			WorldSpeed = Run->GetGameSpeedMultiplier();
		}
	}
	for (ADCCriminalPawn* P : Pawns) { if (P) { P->CustomTimeDilation = WorldSpeed; } }
	for (const FCarSlot& S : Slots) { if (ADCVehicle* Car = S.Car.Get()) { Car->CustomTimeDilation = WorldSpeed; } }
	for (const TWeakObjectPtr<ADCGarageDoor>& D : Doors) { if (ADCGarageDoor* Door = D.Get()) { Door->CustomTimeDilation = WorldSpeed; } }
	UpdateDoors(DeltaSeconds * WorldSpeed);
	TickCars(DeltaSeconds * WorldSpeed);

	SlowTick += DeltaSeconds;
	if (SlowTick >= 0.25f)
	{
		SlowTick = 0.f;
		UpdateBuildTimers();
		UpdateStations();
		UpdateClockLife();
		TickWalkers();
	}
	UpdateDebugView(DeltaSeconds);
	if (bWorldTest) { TickWorldTest(DeltaSeconds); }
}

// ─── Debug view ──────────────────────────────────────────────────────────────
// Draws what the generators produced, as lines over the scene:
//   grey lattice      the build grid (every cell a room could stand on, with its jittered corners)
//   teal / amber      room layout: finished rooms / building sites
//   green / yellow    doorways between rooms / the front door
//   magenta, yellow   furniture footprints and the spots where crew look busy
//   blue, red         the walkable tile map inside the house / the outline of every blocked tile
//   per-pawn colour   each walking pawn's route, waypoints and goal; a ring shows its personal space
//                     (red while it shoulders through a jam)

namespace
{
	constexpr uint8 DbgDepth = SDPG_Foreground;
	struct FDbgLines
	{
		TArray<FBatchedLine> Lines;
		void Add(const FVector2D& A, const FVector2D& B, float Z, const FLinearColor& C, float Thick) { Lines.Emplace(FVector(A.X, A.Y, Z), FVector(B.X, B.Y, Z), C, 0.f, Thick, DbgDepth); }
		void Loop(const FVector2D* P, int32 N, float Z, const FLinearColor& C, float Thick) { for (int32 i = 0; i < N; ++i) { Add(P[i], P[(i + 1) % N], Z, C, Thick); } }
		void Cross(const FVector2D& P, float R, float Z, const FLinearColor& C, float Thick) { Add(P - FVector2D(R, 0), P + FVector2D(R, 0), Z, C, Thick); Add(P - FVector2D(0, R), P + FVector2D(0, R), Z, C, Thick); }
	};
}

void ADCWorldDirector::UpdateDebugView(float Dt)
{
	DebugPoll -= Dt;
	if (DebugPoll <= 0.f)
	{
		DebugPoll = 0.4f;
		static const bool bCmd = FParse::Param(FCommandLine::Get(), TEXT("DCDebugView"));
		const bool bWant = bCmd || DCSettings::DebugView();
		if (bWant != bDebugView) { bDebugView = bWant; bDebugDirty = true; }
	}
	if (!bDebugView)
	{
		if (DebugStatic && bDebugDirty) { DebugStatic->Flush(); DebugLive->Flush(); }
		bDebugDirty = false;
		return;
	}
	if (!DebugStatic)
	{
		auto Make = [this](const TCHAR* Name)
		{
			ULineBatchComponent* C = NewObject<ULineBatchComponent>(this, Name);
			C->bCalculateAccurateBounds = false;
			C->SetupAttachment(GetRootComponent());
			C->RegisterComponent();
			return C;
		};
		DebugStatic = Make(TEXT("DebugStatic"));
		DebugLive = Make(TEXT("DebugLive"));
	}
	if (bDebugDirty) { bDebugDirty = false; DrawDebugStatic(); }
	DrawDebugLive();
}

void ADCWorldDirector::DrawDebugStatic()
{
	using namespace DCSite;
	DebugStatic->Flush();
	FDbgLines L;

	// Build grid
	const FLinearColor GridCol(0.62f, 0.68f, 0.78f);
	for (int32 Y = Grid::MinRow; Y <= Grid::MaxRow; ++Y)
	{
		for (int32 X = Grid::MinCol; X <= Grid::MaxCol; ++X)
		{
			FVector2D Q[4]; Grid::Corners(FIntPoint(X, Y), Q);
			L.Loop(Q, 4, 5.f, GridCol, 2.5f);
			L.Cross(Grid::Centre(FIntPoint(X, Y)), 14.f, 5.f, GridCol, 2.5f);
		}
	}

	// Tile map
	if (Nav.IsBuilt())
	{
		const int32 Nx = Nav.SizeX(), Ny = Nav.SizeY();
		const FVector2D O = Nav.GetOrigin();
		const float C = FDCNavGrid::Cell;
		const FLinearColor Wall(1.f, 0.22f, 0.12f), Tile(0.1f, 0.3f, 0.55f), Tight(0.75f, 0.5f, 0.1f);
		// Kind of the border between two neighbouring tiles: 0 none, 1 blocked|free, 2 indoor floor, 3 tight indoor floor.
		auto Kind = [&](int32 Xa, int32 Ya, int32 Xb, int32 Yb)
		{
			const uint8 A = Nav.ClearAt(Xa, Ya), B = Nav.ClearAt(Xb, Yb);
			if ((A == 0) != (B == 0)) { return 1; }
			if (A == 0 || Nav.RegionAt(Xa, Ya) < 0 || Nav.RegionAt(Xb, Yb) < 0) { return 0; }
			return (A == 1 || B == 1) ? 3 : 2;
		};
		auto Emit = [&](int32 K, const FVector2D& A, const FVector2D& B)
		{
			if (K == 1) { L.Add(A, B, 7.f, Wall, 2.f); }
			else if (K == 2) { L.Add(A, B, 4.f, Tile, 0.f); }
			else if (K == 3) { L.Add(A, B, 4.f, Tight, 0.f); }
		};
		// borders are merged into runs so a whole wall is one line
		for (int32 Y = 1; Y < Ny; ++Y)
		{
			int32 Run = 0, X0 = 0;
			for (int32 X = 0; X <= Nx; ++X)
			{
				const int32 K = X < Nx ? Kind(X, Y - 1, X, Y) : 0;
				if (K == Run) { continue; }
				Emit(Run, O + FVector2D(X0 * C, Y * C), O + FVector2D(X * C, Y * C));
				Run = K; X0 = X;
			}
		}
		for (int32 X = 1; X < Nx; ++X)
		{
			int32 Run = 0, Y0 = 0;
			for (int32 Y = 0; Y <= Ny; ++Y)
			{
				const int32 K = Y < Ny ? Kind(X - 1, Y, X, Y) : 0;
				if (K == Run) { continue; }
				Emit(Run, O + FVector2D(X * C, Y0 * C), O + FVector2D(X * C, Y * C));
				Run = K; Y0 = Y;
			}
		}

		// Doorways
		for (const FDCNavDoor& D : Nav.GetDoors())
		{
			const FLinearColor Col = D.bFront ? FLinearColor(1.f, 0.9f, 0.15f) : FLinearColor(0.2f, 1.f, 0.3f);
			const FVector2D H = D.Along * (D.Width * 0.5f), N(-D.Along.Y, D.Along.X);
			L.Add(D.Centre - H, D.Centre + H, 14.f, Col, 7.f);
			L.Add(D.Centre - H - N * 22.f, D.Centre - H + N * 22.f, 14.f, Col, 4.f);
			L.Add(D.Centre + H - N * 22.f, D.Centre + H + N * 22.f, 14.f, Col, 4.f);
			L.Add(D.Centre - N * 70.f, D.Centre + N * 70.f, 14.f, Col, 2.5f);
		}
	}

	// Room layout + furniture
	for (const TWeakObjectPtr<ADCRoomModule>& RW : Rooms)
	{
		const ADCRoomModule* R = RW.Get();
		if (!R || !R->bPlaced || R->FloorQuad.Num() != 4) { continue; }
		const bool bWalk = IsWalkable(R);
		if (!bWalk && !IsSolidOutdoors(R)) { continue; }
		const FLinearColor Col = bWalk ? FLinearColor(0.1f, 0.95f, 0.85f) : FLinearColor(1.f, 0.6f, 0.1f);
		L.Loop(R->FloorQuad.GetData(), 4, 10.f, Col, 4.5f);
		if (!bWalk)
		{
			L.Add(R->FloorQuad[0], R->FloorQuad[2], 10.f, Col, 2.f);
			L.Add(R->FloorQuad[1], R->FloorQuad[3], 10.f, Col, 2.f);
			continue;
		}
		const FTransform RT = R->GetActorTransform();
		const float RoomYaw = FMath::DegreesToRadians(RT.Rotator().Yaw);
		auto ToWorld = [&](const FVector2D& P) { const FVector W = RT.TransformPositionNoScale(FVector(P.X, P.Y, 0.f)); return FVector2D(W.X, W.Y); };
		for (const ADCRoomModule::FFootprint& F : R->Footprints)
		{
			const float A = RoomYaw + FMath::DegreesToRadians(F.Yaw);
			const FVector2D X(FMath::Cos(A), FMath::Sin(A)), Y(-X.Y, X.X), Cn = ToWorld(F.Centre);
			const FVector2D P[4] = { Cn - X * F.Half.X - Y * F.Half.Y, Cn + X * F.Half.X - Y * F.Half.Y, Cn + X * F.Half.X + Y * F.Half.Y, Cn - X * F.Half.X + Y * F.Half.Y };
			L.Loop(P, 4, 12.f, FLinearColor(1.f, 0.2f, 0.9f), 2.f);
		}
		for (const ADCRoomModule::FUseSpot& U : R->UseSpots)
		{
			const FVector2D P = ToWorld(U.Point);
			const float A = RoomYaw + FMath::DegreesToRadians(U.Yaw);
			L.Cross(P, 9.f, 12.f, FLinearColor(1.f, 0.85f, 0.1f), 2.5f);
			L.Add(P, P + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 28.f, 12.f, FLinearColor(1.f, 0.85f, 0.1f), 2.5f);
		}
	}
	for (const FVector& S : SleepSpots) { L.Cross(FVector2D(S.X, S.Y), 12.f, 60.f, FLinearColor(0.45f, 0.55f, 1.f), 2.5f); }

	DebugStatic->DrawLines(L.Lines);
	UE_LOG(LogDC, Log, TEXT("WorldDirector debug view: %d lines (nav rev %u, %d walkable tiles, %d areas, built in %.2f ms)"), L.Lines.Num(), NavRevision, Nav.NumFree(), Nav.NumComponents(), NavBuildMs);
}

void ADCWorldDirector::DrawDebugLive()
{
	DebugLive->Flush();
	FDbgLines L;
	for (const ADCCriminalPawn* P : Pawns)
	{
		if (!P || P->IsAway() || P->IsHidden()) { continue; }
		const FVector Feet = P->GetFeet();
		const float Z = Feet.Z + 16.f;
		const FLinearColor Col = FLinearColor::MakeFromHSV8((uint8)((P->GetCriminalId() * 53) & 0xFF), 190, 255);
		const FVector2D Pos(Feet.X, Feet.Y);
		// personal space
		const FLinearColor Ring = P->IsShoulderingThrough() ? FLinearColor(1.f, 0.1f, 0.1f) : Col;
		FVector2D Prev = Pos + FVector2D(31.f, 0.f);
		for (int32 k = 1; k <= 12; ++k)
		{
			const float A = k * (2.f * UE_PI / 12.f);
			const FVector2D Q = Pos + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 31.f;
			L.Add(Prev, Q, Z, Ring, 2.f);
			Prev = Q;
		}
		// route
		const TArray<FVector>& Route = P->GetRoute();
		FVector2D A = Pos;
		for (int32 i = 0; i < Route.Num(); ++i)
		{
			const FVector2D B(Route[i].X, Route[i].Y);
			L.Add(A, B, Z, Col, 4.f);
			if (i + 1 < Route.Num()) { L.Cross(B, 7.f, Z, Col, 3.f); }
			else
			{
				const FVector2D G[4] = { B + FVector2D(14, 0), B + FVector2D(0, 14), B - FVector2D(14, 0), B - FVector2D(0, 14) };
				L.Loop(G, 4, Z, Col, 4.f);
			}
			A = B;
		}
	}
	if (L.Lines.Num()) { DebugLive->DrawLines(L.Lines); }
}

// ─── -DCWorldTest ────────────────────────────────────────────────────────────

void ADCWorldDirector::TestShot(const FString& Name)
{
	const FString File = TestShotDir / (Name + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(File, true, false);
	UE_LOG(LogDC, Display, TEXT("[WorldTest] shot %s"), *File);
}

void ADCWorldDirector::LogPawns(const TCHAR* Tag) const
{
	static const TCHAR* Modes[] = { TEXT("Wander"), TEXT("WalkOut"), TEXT("Away"), TEXT("WalkIn") };
	static const TCHAR* Acts[] = { TEXT("Stand"), TEXT("Work"), TEXT("Sleep") };
	for (const ADCCriminalPawn* P : Pawns)
	{
		if (!P) { continue; }
		const ADCRoomModule* R = RoomAt(P->GetFeet());
		UE_LOG(LogDC, Display, TEXT("[WorldTest] %s pawn %d %s/%s%s%s hidden %d room %s loc %s"), Tag, P->GetCriminalId(), Modes[(int32)P->GetMode()], Acts[(int32)P->GetActivity()],
			P->IsSleeping() ? TEXT(" lying") : P->IsInjured() ? TEXT(" injured") : TEXT(""), P->IsReturnQueued() ? TEXT(" return-queued") : TEXT(""), P->IsHidden() ? 1 : 0,
			R ? DC::RoomName(R->RoomType) : TEXT("OUTSIDE"), *P->GetActorLocation().ToCompactString());
	}
}

void ADCWorldDirector::TickWorldTest(float Dt)
{
	TestTime += Dt;
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	ADCCameraRig* Rig = PC.IsValid() ? PC->GetCameraRig() : nullptr;
	if (!Run || !Rig || !Run->HasActiveRun()) { return; }
	float EV = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCWorldTestEV="), EV))
	{
		Rig->Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
		Rig->Camera->PostProcessSettings.AutoExposureBias = EV;
	}
	if (TestTime < StepAt) { return; }
	// Perk offers pause the clock: take the first one so scripted time skips keep working.
	while (Run->HasPendingPerkOffer()) { const TArray<FDCPerk> O = Run->GetPerkOffer(); if (O.Num() == 0) { break; } Run->PickPerk(O[0].Id); }
	auto Next = [this](double Wait) { StepAt = TestTime + Wait; ++TestStep; };
	auto Focus = [&](EDCRoomType T, float Zoom) { if (ADCRoomModule* M = FindRoom(T)) { Rig->FocusOn(M->GetFocusPoint(), Zoom); } };
	auto FocusYard = [&](float Zoom) { const FVector2D C = DCSite::YardRect().GetCenter(); Rig->FocusOn(FVector(C.X, C.Y - 150.f, 0.f), Zoom); };
	auto SkipTo = [Run](float Hour) { const float DeltaH = FMath::Fmod(Hour - Run->GetHourOfDay() + 24.f, 24.f); Run->AdvanceTime(DeltaH * 60.0 / FMath::Max(0.01f, Run->GetGameMinutesPerSecond())); };
	auto Build = [Run](EDCRoomType T)
	{
		const bool bOk = Run->BuildRoom(T);
		const FDCRoom R = Run->GetRoom(T);
		UE_LOG(LogDC, Display, TEXT("[WorldTest] BuildRoom %s: %d (state %d level %d, %.0fs, cash $%d, max active %d)"), DC::RoomName(T), bOk ? 1 : 0, (int32)R.State, R.Level, R.BuildEndsAt - R.BuildStartedAt, Run->GetCash(), Run->GetMaxActiveMissions());
		return bOk;
	};
	auto FinishBuild = [Run]() { for (const FDCRoom& R : Run->GetRooms()) { if (R.State == EDCRoomState::Building) { Run->AdvanceTime(R.BuildEndsAt - Run->GetRunTime() + 0.05); } } };
	auto Shortest = [Run]()
	{
		double S = 1e9;
		for (const FDCMission& M : Run->GetMissions()) { if (M.bInProgress) { S = FMath::Min(S, M.EndsAt - Run->GetRunTime()); } }
		return S;
	};
	auto StartAll = [Run, &Shortest](int32 Limit)
	{
		int32 Started = 0;
		for (const FDCMission& M : Run->GetMissions())
		{
			if (Started >= Limit) { break; }
			if (M.bInProgress || Run->GetActiveMissionCount() >= Run->GetMaxActiveMissions()) { continue; }
			if (Run->StartMission(Run->SuggestPlan(M.Id))) { ++Started; }
		}
		UE_LOG(LogDC, Display, TEXT("[WorldTest] started %d missions (active %d / max %d), shortest %.0fs"), Started, Run->GetActiveMissionCount(), Run->GetMaxActiveMissions(), Shortest());
		return Started;
	};
	auto GiveCash = [Run]()
	{
		// Builds need cash + successes: run jobs instantly until the early wings unlock.
		for (int32 k = 0; k < 40 && Run->GetPhase() == EDCGamePhase::Playing && (Run->GetCash() < 11000 || Run->GetMissionsSucceeded() < 6); ++k)
		{
			while (Run->HasPendingPerkOffer()) { const TArray<FDCPerk> O = Run->GetPerkOffer(); if (O.Num() == 0) { break; } Run->PickPerk(O[0].Id); }
			for (const FDCMission& M : Run->GetMissions())
			{
				if (M.bInProgress || Run->GetActiveMissionCount() >= Run->GetMaxActiveMissions()) { continue; }
				const FDCCrewPlan P = Run->SuggestPlan(M.Id);
				if (Run->PreviewPlan(P).bValid && Run->PreviewPlan(P).SuccessChance >= 0.6f) { Run->StartMission(P); }
			}
			Run->AdvanceTime(150.0);
		}
	};

	switch (TestStep)
	{
	case 0:
		if (TestActors.Num() == 0)
		{
			// No landscape yet (pre-merge worktree): lay a plain ground so outdoor shots read.
			FHitResult Hit;
			if (!GetWorld()->LineTraceSingleByChannel(Hit, FVector(0, DCSite::YardRect().GetCenter().Y, 500), FVector(0, DCSite::YardRect().GetCenter().Y, -500), ECC_WorldStatic))
			{
				FActorSpawnParameters GP; GP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AStaticMeshActor* G = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FRotator::ZeroRotator, FVector(0, 0, -3), FVector(400, 400, 1)), GP))
				{
					G->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
					G->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, DCPaths::Plane));
					if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, DCPaths::MatSurface))
					{
						UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(M, G);
						D->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.09f, 0.11f, 0.06f));
						D->SetScalarParameterValue(TEXT("Roughness"), 0.95f);
						G->GetStaticMeshComponent()->SetMaterial(0, D);
					}
					TestActors.Add(G);
				}
			}
			TestActors.Add(this);
		}
		if (TestTime >= 6.0) { LogPawns(TEXT("start")); LogSlots(TEXT("start")); TestShot(TEXT("t_start")); Next(0.5); }
		break;
	case 1:
		GiveCash();
		while (Run->HasPendingPerkOffer()) { const TArray<FDCPerk> O = Run->GetPerkOffer(); if (O.Num() == 0) { break; } Run->PickPerk(O[0].Id); }
		SkipTo(9.5f);
		if (UDCUISubsystem* UI = UDCUISubsystem::Get(this)) { UI->ClosePanel(); }
		UE_LOG(LogDC, Display, TEXT("[WorldTest] grind: cash $%d, successes %d, roster %d"), Run->GetCash(), Run->GetMissionsSucceeded(), Run->GetRoster().Num());
		// Showroom: fill the other bays so every body style / paint is in one shot.
		for (int32 i = Slots.Num(); i < DCSite::MaxParkingSlots; ++i)
		{
			if (ADCVehicle* C = SpawnCar(i)) { C->ParkAt(DCSite::ParkingSlot(i)); TestActors.Add(C); if (i == 3) { C->SetDoorsOpen(true); } }
		}
		FocusYard(0.45f);
		Next(2.5);
		break;
	case 2: TestShot(TEXT("t_car_lineup")); Rig->FocusOn(DCSite::ParkingSlot(3).GetLocation(), 1.f); Next(2.5); break;
	// First build of a wing: timber frame + scaffold for the whole build, then the reveal.
	case 3:
		TestShot(TEXT("t_car_close"));
		Build(EDCRoomType::Lounge); Run->AdvanceTime(12.0); Focus(EDCRoomType::Lounge, 0.45f); Next(2.5); break;
	case 4: for (const TWeakObjectPtr<AActor>& A : TestActors) { if (ADCVehicle* C = Cast<ADCVehicle>(A.Get())) { C->Destroy(); } }
		TestShot(TEXT("t_building")); Next(0.5); break;
	case 5: FinishBuild(); Rig->ResetFocus(); Next(0.9); break;
	case 6: TestShot(TEXT("t_reveal_a")); Next(1.2); break;
	case 7: TestShot(TEXT("t_reveal_b")); Next(3.0); break;
	case 8:
		{
			TestShot(TEXT("t_after_reveal")); LogPawns(TEXT("after_reveal"));
			// Hire the cheapest recruit: walks up the driveway.
			const FDCCriminal* Cheapest = nullptr;
			for (const FDCCriminal& C : Run->GetRecruits()) { if (!Cheapest || C.HireCost < Cheapest->HireCost) { Cheapest = &C; } }
			if (Cheapest) { FString E; const int32 Id = Cheapest->Id; const bool bOk = Run->Hire(Id, &E); UE_LOG(LogDC, Display, TEXT("[WorldTest] Hire %d: %d %s (cash $%d)"), Id, bOk ? 1 : 0, *E, Run->GetCash()); }
			FocusYard(0.3f);
			Next(6.0);
		}
		break;
	case 9: TestShot(TEXT("t_hire_walkin")); LogPawns(TEXT("hire")); Next(0.5); break;
	// Garage: raises the mission cap, so a new car drives in along the lane and parks.
	case 10: if (!Build(EDCRoomType::Garage)) { GiveCash(); Build(EDCRoomType::Garage); } FinishBuild(); FocusYard(0.3f); Next(4.0); break;
	case 11: TestShot(TEXT("t_new_car_arriving")); LogSlots(TEXT("new_car")); Next(8.0); break;
	case 12: TestShot(TEXT("t_new_car_parked")); LogSlots(TEXT("new_car_parked")); Next(0.5); break;
	// Missions: crews walk to the cars, get in, drive off.
	case 13: StartAll(99); FocusYard(0.25f); LogSlots(TEXT("started")); Next(3.0); break;
	case 14: TestShot(TEXT("t_walk_to_car")); LogPawns(TEXT("walking")); Next(3.5); break;
	case 15: TestShot(TEXT("t_boarding")); LogSlots(TEXT("boarding")); LogPawns(TEXT("boarding")); Next(3.0); break;
	case 16: TestShot(TEXT("t_driving_off")); LogSlots(TEXT("driving")); Next(3.0); break;
	case 17: TestShot(TEXT("t_driving_lane")); Next(4.0); break;
	case 18:
		{
			TestShot(TEXT("t_cars_gone")); LogSlots(TEXT("gone")); LogPawns(TEXT("away"));
			// Bring the shortest job to its last seconds: its car heads home and parks about when it ends.
			const double S = Shortest();
			if (S < 1e8 && S > 6.0) { Run->AdvanceTime(S - 6.0); }
			Next(4.0);
		}
		break;
	case 19: TestShot(TEXT("t_car_returning")); LogSlots(TEXT("returning")); Next(4.0); break;
	case 20: TestShot(TEXT("t_car_parking")); LogSlots(TEXT("parking")); Next(3.0); break;
	case 21: TestShot(TEXT("t_unloading")); LogSlots(TEXT("unloading")); LogPawns(TEXT("unloading")); Next(3.0); break;
	case 22: TestShot(TEXT("t_walk_in")); LogPawns(TEXT("walk_in")); Run->AdvanceTime(1300.0); Next(16.0); break;
	case 23: TestShot(TEXT("t_all_back")); LogSlots(TEXT("all_back")); LogPawns(TEXT("all_back")); Next(0.5); break;
	// A job that finishes while its crew is still walking to the car: they turn round, the car stays.
	case 24: StartAll(1); Next(2.0); break;
	case 25: { const double S = Shortest(); if (S < 1e8) { Run->AdvanceTime(S + 0.5); } LogSlots(TEXT("early_done")); Next(2.5); } break;
	case 26: TestShot(TEXT("t_turnaround")); LogSlots(TEXT("turnaround")); LogPawns(TEXT("turnaround")); Next(6.0); break;
	// Office upgrade: quick reveal of the new level.
	case 27:
		Focus(EDCRoomType::Office, 0.35f);
		bTestFakeUpgrade = !Build(EDCRoomType::Office);
		if (bTestFakeUpgrade)
		{
			if (ADCRoomModule* M = FindRoom(EDCRoomType::Office)) { M->ApplyState(EDCRoomState::Building, 1); M->SetBuildProgress(0.4f, 42.0, 2); }
		}
		else { Run->AdvanceTime(5.0); }
		Next(2.0);
		break;
	case 28:
		TestShot(TEXT("t_upgrading"));
		if (bTestFakeUpgrade) { if (ADCRoomModule* M = FindRoom(EDCRoomType::Office)) { M->PlayUpgradeReveal(2); } }
		else { FinishBuild(); }
		Next(0.5);
		break;
	case 29: TestShot(TEXT("t_upgrade_reveal")); Next(2.0); break;
	// Dismiss one: walks off down the driveway.
	case 30:
		for (const FDCCriminal& C : Run->GetRoster()) { if (C.IsAvailable()) { UE_LOG(LogDC, Display, TEXT("[WorldTest] dismiss %d"), C.Id); Run->Dismiss(C.Id); break; } }
		Rig->FocusOn(FVector(0.f, DCSite::YardRect().GetCenter().Y, 0.f), 0.2f);
		Next(6.0);
		break;
	case 31: TestShot(TEXT("t_dismissed")); LogPawns(TEXT("dismissed")); Next(4.0); break;
	case 32:
		for (ADCCriminalPawn* P : Pawns) { if (P && !P->IsHidden() && P->GetMode() == EDCPawnMode::Wander) { Run->OnCriminalLevelUp.Broadcast(P->GetCriminalId(), 5); Rig->FocusOn(P->GetActorLocation(), 0.85f); break; } }
		Next(0.6);
		break;
	case 33: TestShot(TEXT("t_levelup")); Next(0.5); break;
	// Night: most crew go to bed in the bunkroom; a night job drives off with its headlights on.
	case 34: SkipTo(22.5f); if (!Run->IsNight()) { SkipTo(22.5f); } Rig->ResetFocus(); UE_LOG(LogDC, Display, TEXT("[WorldTest] hour %.1f night %d"), Run->GetHourOfDay(), Run->IsNight() ? 1 : 0); Next(12.0); break;
	case 35: TestShot(TEXT("t_night_wide")); Focus(EDCRoomType::Lounge, 0.55f); Next(2.0); break;
	case 36: TestShot(TEXT("t_night_bunks")); LogPawns(TEXT("night")); StartAll(1); FocusYard(0.2f); Next(13.0); break;
	case 37: TestShot(TEXT("t_night_car")); LogSlots(TEXT("night")); Next(2.0); break;
	case 38: TestShot(TEXT("t_night_car_b")); Next(0.5); break;
	case 39: SkipTo(8.f); Rig->ResetFocus(); Next(10.0); break;
	case 40: TestShot(TEXT("t_morning")); LogPawns(TEXT("morning")); LogSlots(TEXT("morning")); Next(0.5); break;
	// Zoomed right out over the landscape.
	case 41: Rig->ResetFocus(); Rig->Zoom(-30.f); Next(2.5); break;
	case 42: TestShot(TEXT("t_far")); Rig->ResetFocus(); Next(1.5); break;
	// Station hover + click (camera focus, then the panel opens); badge on the board.
	case 43:
		for (TActorIterator<ADCStation> It(GetWorld()); It; ++It)
		{
			if (It->StationType == EDCStationType::MissionBoard) { It->OnHoverBegin(); It->OnClicked(); break; }
		}
		Next(2.0);
		break;
	case 44: TestShot(TEXT("t_station_focus")); Next(0.5); break;
	case 45:
		for (TActorIterator<ADCStation> It(GetWorld()); It; ++It) { It->OnHoverEnd(); }
		if (UDCUISubsystem* UI = UDCUISubsystem::Get(this)) { UI->ClosePanel(); }
		Next(2.0);
		break;
	case 46: TestShot(TEXT("t_station_closed")); LogPawns(TEXT("end")); Next(1.0); break;
	case 47: UE_LOG(LogDC, Display, TEXT("[WorldTest] done")); if (GEngine) { GEngine->Exec(GetWorld(), TEXT("QUIT")); } ++TestStep; break;
	default: break;
	}
}
