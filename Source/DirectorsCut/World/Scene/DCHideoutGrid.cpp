// Director's Cut — the house on the build grid. Places the 8 room modules on DCSite::Grid cells (from the run's
// placements) and regenerates floors, walls, frames and scaffolds whenever a placement or a room's stage changes.
// See DCHideoutBuilder.h for the rules and Core/DCSiteLayout.h for the grid itself.
#include "World/Scene/DCHideoutBuilder.h"
#include "World/Scene/DCLandscapeMeshes.h"
#include "World/DCRoomModule.h"
#include "World/DCStation.h"
#include "World/DCCriminalSpawnZone.h"
#include "Core/DCRunSubsystem.h"
#include "Core/DCSiteLayout.h"
#include "Core/DCWorldTags.h"
#include "DirectorsCut.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

namespace
{
	using namespace DCSite;
	constexpr float GWallT = 24.f;    // wall thickness
	constexpr float GDoorW = 150.f;   // doorway in a partition, always centred on the shared edge (nav relies on it)
	constexpr float GBoundsH = 380.f; // room bounds height
	const FLinearColor GWarm(1.f, 0.66f, 0.38f);

	float H01(int32 Seed, int32 K) { return Grid::Hash01(Seed, K, 31); }
	float HS(int32 Seed, int32 K) { return H01(Seed, K) * 2.f - 1.f; }
	/** Where an unplaced room waits (far off-site, under the ground, hidden). */
	FVector ParkSpot(int32 I) { return FVector(-40000.f - I * 3000.f, 40000.f, -5000.f); }

	EDCStationType StationFor(EDCRoomType T)
	{
		switch (T)
		{
		case EDCRoomType::Office:       return EDCStationType::MissionBoard;
		case EDCRoomType::Lounge:       return EDCStationType::Lounge;
		case EDCRoomType::Workshop:     return EDCStationType::Workbench;
		case EDCRoomType::Garage:       return EDCStationType::GarageDoor;
		case EDCRoomType::OpsRoom:      return EDCStationType::OpsRoom;
		case EDCRoomType::Clinic:       return EDCStationType::Clinic;
		case EDCRoomType::ServerRoom:   return EDCStationType::ServerRack;
		case EDCRoomType::LawyerOffice: return EDCStationType::Lawyer;
		default:                        return EDCStationType::MissionBoard;
		}
	}
}

ADCHideoutBuilder::FGridStyle ADCHideoutBuilder::StyleFor(EDCRoomType T) const
{
	switch (T)
	{
	case EDCRoomType::Office:       return { "Floorboard",  "Stone",    "Plaster",      "Stone" };
	case EDCRoomType::Workshop:     return { "Cement",      "BrickOld", "BrickOld",     "BrickOld" };
	case EDCRoomType::Garage:       return { "SlabDark",    "TinRustV", "Board",        "StoneDark" };
	case EDCRoomType::OpsRoom:      return { "Floorboard",  "Board",    "Board",        "Stone" };
	case EDCRoomType::Lounge:       return { "Floorboard",  "Stone",    "PlasterGreen", "Stone" };
	case EDCRoomType::ServerRoom:   return { "RaisedFloor", "BrickOld", "Plaster",      "BrickOld" };
	case EDCRoomType::LawyerOffice: return { "CarpetTile",  "Stone",    "PlasterRose",  "Stone" };
	case EDCRoomType::Clinic:       return { "TileOld",     "BrickOld", "PlasterWhite", "BrickOld" };
	default:                        return { "Floorboard",  "Stone",    "Plaster",      "Stone" };
	}
}

// =====================================================================================================================
// Setup
// =====================================================================================================================

void ADCHideoutBuilder::SpawnGridRooms()
{
	UWorld* W = GetWorld();
	for (int32 i = 0; i < (int32)EDCRoomType::MAX; ++i)
	{
		const EDCRoomType T = (EDCRoomType)i;
		ADCRoomModule* Room = W->SpawnActor<ADCRoomModule>(ADCRoomModule::StaticClass(), FTransform(ParkSpot(i)));
		Room->RoomType = T;
		Room->LocalBounds = FBox(FVector(-100, -100, 0), FVector(100, 100, GBoundsH));
#if WITH_EDITOR
		Room->SetActorLabel(FString::Printf(TEXT("Room_%s"), *StaticEnum<EDCRoomType>()->GetNameStringByValue((int64)T)));
#endif
		Rooms.Add(Room);
		RoomPtrs.Add(Room);

		// Persistent floor (its mesh is swapped on every rebuild) so the station can highlight it on hover.
		UStaticMeshComponent* F = NewObject<UStaticMeshComponent>(Room);
		F->SetMobility(EComponentMobility::Movable);
		F->SetupAttachment(Room->GetRootComponent());
		F->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		F->SetCollisionResponseToAllChannels(ECR_Ignore);
		F->RegisterComponent();
		Room->AddInstanceComponent(F);
		Room->AddToGroup(F, DCGroups::Shell);
		GridFloors.Add(F);

		// The whole room is one station for now (rooms are empty): click anywhere inside to open its panel.
		ADCStation* S = W->SpawnActor<ADCStation>(ADCStation::StaticClass(), FTransform(ParkSpot(i) + FVector(0, 0, 110)));
		S->StationType = StationFor(T);
		S->DisplayName = DC::RoomName(T);
		S->OwningRoom = T;
		S->FocusZoomAlpha = 0.4f;
		S->SetClickExtent(FVector(100, 100, 100));
		S->AttachToActor(Room, FAttachmentTransformRules::KeepWorldTransform);
		Room->RegisterStation(S);
		S->RegisterHighlightComponent(F);
		GridStations.Add(S);

		ADCCriminalSpawnZone* Z = W->SpawnActor<ADCCriminalSpawnZone>(ADCCriminalSpawnZone::StaticClass(), FTransform(ParkSpot(i) + FVector(0, 0, 50)));
		Z->RoomType = T;
		Z->Zone->SetBoxExtent(FVector(100, 100, 50));
		Z->AttachToActor(Room, FAttachmentTransformRules::KeepWorldTransform);
		GridZones.Add(Z);
	}
	GridGen.SetNum(RoomPtrs.Num());
}

int32 ADCHideoutBuilder::GridStage(const ADCRoomModule* R)
{
	if (!R) { return 0; }
	if (R->GetLevel() >= 1 && R->GetState() != EDCRoomState::Locked) { return 2; }
	if (R->GetState() == EDCRoomState::Building || R->IsRevealPlaying()) { return 1; }
	return 0;
}

void ADCHideoutBuilder::SyncGrid(bool bForce)
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	const bool bRun = Run && Run->HasActiveRun();
	const int32 N = RoomPtrs.Num();
	TArray<int32> Stage; Stage.Init(0, N);
	TArray<FIntPoint> Cell; Cell.Init(Grid::NoCell(), N);
	TArray<FIntPoint> Occ;
	for (int32 i = 0; i < N; ++i)
	{
		const ADCRoomModule* R = RoomPtrs[i];
		Stage[i] = GridStage(R);
		if (!R || Stage[i] == 0) { continue; }
		FIntPoint C;
		if (R->RoomType == EDCRoomType::Office) { Cell[i] = Grid::OfficeCell(); }
		else if (bRun && Run->GetRoomCell(R->RoomType, C) && !Occ.Contains(C)) { Cell[i] = C; }
		if (Grid::IsInGrid(Cell[i])) { Occ.Add(Cell[i]); }
	}
	// Rooms switched on without a placement (dev flags, review states): a stable default cell each.
	for (int32 i = 0; i < N; ++i)
	{
		if (Stage[i] == 0 || Grid::IsInGrid(Cell[i])) { continue; }
		const uint8 Key = (uint8)RoomPtrs[i]->RoomType;
		const FIntPoint* D = DevCells.Find(Key);
		const FIntPoint C = (D && Grid::CanPlace(*D, Occ)) ? *D : Grid::AutoPick(Occ);
		if (!Grid::IsInGrid(C)) { Stage[i] = 0; continue; }
		DevCells.Add(Key, C);
		Cell[i] = C;
		Occ.Add(C);
	}

	uint32 Sig = 0x51u + N;
	for (int32 i = 0; i < N; ++i) { Sig = HashCombineFast(Sig, (uint32)Stage[i] | ((uint32)(Cell[i].X + 16) << 4) | ((uint32)(Cell[i].Y + 16) << 12)); }
	if (!bForce && Sig == GridSignature) { return; }
	GridSignature = Sig;

	TMap<FIntPoint, int32> StageAt;
	for (int32 i = 0; i < N; ++i) { if (Stage[i] > 0) { StageAt.Add(Cell[i], Stage[i]); } }
	const double T0 = FPlatformTime::Seconds();
	for (int32 i = 0; i < N; ++i) { BuildGridRoom(i, Cell[i], Stage[i], StageAt); }
	UE_LOG(LogDC, Log, TEXT("[Scene] house rebuilt: %d rooms on the grid, %.1f ms"), StageAt.Num(), (FPlatformTime::Seconds() - T0) * 1000.0);
}

// =====================================================================================================================
// One room
// =====================================================================================================================

void ADCHideoutBuilder::BuildGridRoom(int32 Index, const FIntPoint& Cell, int32 Stage, const TMap<FIntPoint, int32>& StageAt)
{
	ADCRoomModule* Room = RoomPtrs[Index];
	if (!Room) { return; }
	for (const TWeakObjectPtr<UActorComponent>& CW : GridGen[Index])
	{
		if (UActorComponent* C = CW.Get()) { Room->RemoveInstanceComponent(C); C->DestroyComponent(); }
	}
	GridGen[Index].Reset();
	Room->Footprints.Reset();
	Room->UseSpots.Reset();
	UStaticMeshComponent* Floor = GridFloors[Index];
	ADCStation* Station = GridStations[Index];
	ADCCriminalSpawnZone* Zone = GridZones[Index];

	if (Stage == 0 || !Grid::IsInGrid(Cell))
	{
		Room->bPlaced = false;
		Room->GridCell = Grid::NoCell();
		Room->FloorQuad.Reset();
		Room->SetActorLocation(ParkSpot(Index));
		Room->LocalBounds = FBox(FVector(-100, -100, 0), FVector(100, 100, GBoundsH));
		Room->SetActorHiddenInGame(true);
		if (Station) { Station->SetActorLocation(ParkSpot(Index) + FVector(0, 0, 110)); }
		if (Zone) { Zone->SetActorLocation(ParkSpot(Index) + FVector(0, 0, 50)); }
		Room->NotifyGeometryRebuilt();
		return;
	}

	FVector2D Wc[4]; Grid::Corners(Cell, Wc);
	const FVector2D Ctr = Grid::Centre(Cell);
	FVector2D Q[4];
	FBox2D BB(ForceInit);
	for (int32 k = 0; k < 4; ++k) { Q[k] = Wc[k] - Ctr; BB += Q[k]; }
	Room->SetActorLocation(FVector(Ctr.X, Ctr.Y, GroundZ));
	Room->SetActorHiddenInGame(false);
	Room->LocalBounds = FBox(FVector(BB.Min.X, BB.Min.Y, 0.f), FVector(BB.Max.X, BB.Max.Y, GBoundsH));
	Room->FloorQuad = { Wc[0], Wc[1], Wc[2], Wc[3] };
	Room->bPlaced = true;
	Room->GridCell = Cell;

	const FGridStyle St = StyleFor(Room->RoomType);
	if (Floor)
	{
		Floor->SetStaticMesh(DCLandMesh::Prism(this, { Q[0], Q[1], Q[2], Q[3] }, -22.f, 0.f));
		Floor->SetMaterial(0, M(Stage == 2 ? St.Floor : FName("Cement")));
	}
	const FVector2D Half = BB.GetExtent();
	if (Station)
	{
		Station->SetActorLocation(FVector(Ctr.X, Ctr.Y, 110.f));
		Station->SetClickExtent(FVector(FMath::Max(80.f, Half.X - 40.f), FMath::Max(80.f, Half.Y - 40.f), 110.f));
	}
	if (Zone)
	{
		Zone->SetActorLocation(FVector(Ctr.X, Ctr.Y, 50.f));
		Zone->Zone->SetBoxExtent(FVector(FMath::Max(60.f, Half.X - 130.f), FMath::Max(60.f, Half.Y - 130.f), 50.f));
	}

	TSet<UActorComponent*> Before;
	for (UActorComponent* C : Room->GetComponents()) { Before.Add(C); }
	CurRoom = Room;
	bFit = false;
	const int32 Seed = 1000 + Cell.X * 131 + Cell.Y * 17;
	const float H = Grid::WallHeight(Cell);
	auto NStage = [&](int32 E) { const int32* P = StageAt.Find(Grid::Neighbour(Cell, E)); return P ? *P : 0; };
	// scaffolding goes up on an open side (back first, so it doesn't hide the room), never inside a neighbour
	int32 ScafEdge = -1;
	for (int32 E : { 2, 1, 3, 0 }) { if (NStage(E) == 0) { ScafEdge = E; break; } }

	if (Stage == 1)
	{
		CurGroup = DCGroups::Building;
		FrameRoom(Q, H, Seed, ScafEdge);
	}
	else
	{
		CurGroup = DCGroups::Shell;
		bool bTall[4];
		for (int32 e = 0; e < 4; ++e) { bTall[e] = !Grid::IsFrontEdge(e) && NStage(e) < 2; }
		// Furniture first: laid out to fit this room's shape and connections. It lights the room itself and may stand
		// against a window, which the wall then leaves out.
		int32 Kind[4]; bool bExit[4], bSS[4], bSE[4];
		uint8 Omit[4] = { 0, 0, 0, 0 };
		for (int32 e = 0; e < 4; ++e)
		{
			Kind[e] = NStage(e) == 2 ? 1 : (Grid::IsFrontEdge(e) ? 2 : 0);
			bExit[e] = Grid::IsFrontEdge(e) && NStage(e) == 0;
			bSS[e] = bSE[e] = false; // back walls stand full height to both ends
		}
		const bool bFurnished = FurnishGridRoom(Room, Q, Kind, bExit, bSS, bSE, H, Seed, Seed, Omit);
		for (int32 e = 0; e < 4; ++e)
		{
			const FVector2D A = Q[e], B = Q[(e + 1) & 3];
			const int32 S = Seed + e * 7;
			if (NStage(e) == 2)
			{
				// Shared with a finished room: one low partition with a doorway, built by the room on the -side.
				if (!Grid::IsFrontEdge(e)) { PartitionEdge(A, B, St, S); }
				continue;
			}
			if (Grid::IsFrontEdge(e))
			{
				// Collapsed front: razed to a low stub just above the floor so the camera sees the whole room.
				RuinEdge(A, B, St, S, 16.f + H01(S, 1) * 8.f, 16.f + H01(S, 2) * 8.f);
			}
			else
			{
				// Back wall: full height along its whole length (must match bSS / bSE above).
				TallEdge(A, B, H, St, S, bSS[e], bSE[e], Omit[e]);
			}
		}
		if (bTall[1] && bTall[2])
		{
			// corner pier where the two back walls meet
			const FVector2D In = (-Q[2]).GetSafeNormal() * 14.f;
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Q[2].Y - Q[1].Y, Q[2].X - Q[1].X));
			Deco(EDCShape::Cube, FVector(Q[2].X + In.X, Q[2].Y + In.Y, (H + 14.f) * 0.5f), FVector(42.f, 42.f, H + 14.f), St.Outer, FRotator(0, Yaw, 0));
		}
		// The room's light (no roof timbers any more): a bracket lamp on a standing back wall, else a bare light.
		if (bFurnished) {}
		else if (bTall[2] || bTall[1])
		{
			const int32 E = bTall[2] ? 2 : 1;
			BracketLamp(Q[E], Q[(E + 1) & 3], 0.5f + HS(Seed, 10) * 0.08f, FMath::Min(H - 40.f, 215.f), GWarm, 30.f, 760.f);
		}
		else
		{
			const FVector2D C = FMath::Lerp(Q[0], Q[2], 0.5f);
			Point(FVector(C.X, C.Y, FMath::Min(H - 40.f, 230.f)), GWarm, 26.f, 760.f, true, true, 4.f);
		}
		if (Room->RoomType == EDCRoomType::Office) { OfficePorchLamps(Q); }

		// upgrade scaffolding (Building group: only visible while an upgrade is running)
		CurGroup = DCGroups::Building;
		EdgeScaffold(Q, ScafEdge, H + 40.f);
		const FVector2D WL = FMath::Lerp(Q[0], Q[2], 0.22f);
		WorkLight(FVector(WL.X, WL.Y, 0.f), FMath::RadiansToDegrees(FMath::Atan2(-WL.Y, -WL.X)), true, 60.f, false);
		CurGroup = DCGroups::Shell;
	}
	FlushBatches();
	for (UActorComponent* C : Room->GetComponents()) { if (!Before.Contains(C)) { GridGen[Index].Add(C); } }
	CurRoom = nullptr;
	Room->NotifyGeometryRebuilt();
}

// =====================================================================================================================
// Edge pieces (room-local; A->B runs counter-clockwise, so the room is on the left)
// =====================================================================================================================

void ADCHideoutBuilder::EdgeBox(const FVector2D& A, const FVector2D& B, float T0, float T1, float Inset, float Z0, float Z1, float Thick, FName Mat, float Pitch)
{
	if (T1 - T0 < 0.5f || Z1 - Z0 < 0.5f) { return; }
	const FVector2D D = (B - A).GetSafeNormal();
	const FVector2D N(-D.Y, D.X);
	const FVector2D P = A + D * ((T0 + T1) * 0.5f) + N * Inset;
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
	Deco(EDCShape::Cube, FVector(P.X, P.Y, (Z0 + Z1) * 0.5f), FVector(T1 - T0, Thick, Z1 - Z0), Mat, FRotator(Pitch, Yaw, 0.f));
}

void ADCHideoutBuilder::EdgeWall(const FVector2D& A, const FVector2D& B, float T0, float T1, float Inset, float Z0, float Z1, float Thick, const TArray<FOpening>& InOps, FName Mat)
{
	TArray<FOpening> Ops = InOps;
	Ops.Sort([](const FOpening& L, const FOpening& R) { return L.Center < R.Center; });
	float Cursor = T0;
	for (const FOpening& O : Ops)
	{
		const float O0 = FMath::Clamp(O.Center - O.Width * 0.5f, T0, T1), O1 = FMath::Clamp(O.Center + O.Width * 0.5f, T0, T1);
		if (O1 <= O0) { continue; }
		EdgeBox(A, B, Cursor, O0, Inset, Z0, Z1, Thick, Mat);
		EdgeBox(A, B, O0, O1, Inset, Z0, FMath::Clamp(O.Bottom, Z0, Z1), Thick, Mat);
		EdgeBox(A, B, O0, O1, Inset, FMath::Clamp(O.Top, Z0, Z1), Z1, Thick, Mat);
		Cursor = O1;
	}
	EdgeBox(A, B, Cursor, T1, Inset, Z0, Z1, Thick, Mat);
}

void ADCHideoutBuilder::EdgeWindow(const FVector2D& A, const FVector2D& B, const FOpening& O, float Inset, int32 Seed)
{
	const float L0 = O.Center - O.Width * 0.5f, L1 = O.Center + O.Width * 0.5f;
	EdgeBox(A, B, L0 - 8.f, L1 + 8.f, Inset + 4.f, O.Bottom - 7.f, O.Bottom, GWallT + 12.f, "StoneDark");       // sill
	EdgeBox(A, B, L0, L0 + 7.f, Inset, O.Bottom, O.Top, 12.f, "TimberGrey");                                   // jambs
	EdgeBox(A, B, L1 - 7.f, L1, Inset, O.Bottom, O.Top, 12.f, "TimberGrey");
	EdgeBox(A, B, L0, L1, Inset, O.Top - 7.f, O.Top, 12.f, "TimberGrey");                                      // head
	if (H01(Seed, 20) > 0.7f)
	{
		// boarded up: a couple of planks nailed across, a little crooked
		EdgeBox(A, B, L0 - 10.f, L1 + 10.f, Inset - 8.f, O.Bottom + 25.f, O.Bottom + 45.f, 3.f, "Board", HS(Seed, 21) * 7.f);
		EdgeBox(A, B, L0 - 6.f, L1 + 12.f, Inset - 8.f, O.Top - 60.f, O.Top - 40.f, 3.f, "Board", HS(Seed, 22) * 9.f);
		return;
	}
	const float Mid = (O.Bottom + O.Top) * 0.5f;
	EdgeBox(A, B, O.Center - 2.5f, O.Center + 2.5f, Inset, O.Bottom, O.Top, 6.f, "TimberGrey");                 // mullion
	EdgeBox(A, B, L0, L1, Inset, Mid - 2.5f, Mid + 2.5f, 6.f, "TimberGrey");                                    // transom
	EdgeBox(A, B, L0 + 5.f, L1 - 5.f, Inset, O.Bottom, O.Top - 5.f, 1.5f, "Window");                            // glass
}

void ADCHideoutBuilder::TallEdgePlan(float L, float H, int32 Seed, bool bStepStart, bool bStepEnd, TArray<FOpening>& OutWin, TArray<FVector>& OutSteps)
{
	const float HalfT = GWallT * 0.5f;
	const float SA = bStepStart ? 70.f + H01(Seed, 1) * 70.f : 0.f;
	const float SB = bStepEnd ? 70.f + H01(Seed, 2) * 70.f : 0.f;
	const float Main0 = bStepStart ? SA : -HalfT, Main1 = bStepEnd ? L - SB : L + HalfT;
	OutWin.Reset();
	OutSteps.Reset();
	const int32 NW = L > 540.f ? 2 : 1;
	for (int32 k = 0; k < NW; ++k)
	{
		const float W = 100.f + H01(Seed, 3 + k) * 25.f;
		const float C = L * (k + 0.5f) / NW + HS(Seed, 5 + k) * 30.f;
		if (C - W * 0.5f < Main0 + 45.f || C + W * 0.5f > Main1 - 45.f) { continue; }
		OutWin.Add({ C, W, 92.f + H01(Seed, 7 + k) * 10.f, FMath::Min(205.f, H - 42.f) });
	}
	// the broken ends stand at H * (0.42..0.64) (see Step in TallEdge); the lower figure is the safe one
	if (bStepStart) { OutSteps.Add(FVector(-HalfT, SA, H * (0.42f + 0.22f * H01(Seed, 11)))); }
	if (bStepEnd) { OutSteps.Add(FVector(L - SB, L + HalfT, H * (0.42f + 0.22f * H01(Seed, 13)))); }
}

void ADCHideoutBuilder::TallEdge(const FVector2D& A, const FVector2D& B, float H, const FGridStyle& St, int32 Seed, bool bStepStart, bool bStepEnd, uint8 OmitWindows)
{
	const float L = FVector2D::Distance(A, B);
	const float T = GWallT, HalfT = GWallT * 0.5f;
	const float SA = bStepStart ? 70.f + H01(Seed, 1) * 70.f : 0.f;
	const float SB = bStepEnd ? 70.f + H01(Seed, 2) * 70.f : 0.f;
	const float Main0 = bStepStart ? SA : -HalfT, Main1 = bStepEnd ? L - SB : L + HalfT;

	// windows, kept clear of the broken ends
	TArray<FOpening> Win;
	TArray<FVector> Steps;
	TallEdgePlan(L, H, Seed, bStepStart, bStepEnd, Win, Steps);
	for (int32 k = Win.Num() - 1; k >= 0; --k) { if (OmitWindows & (1 << k)) { Win.RemoveAt(k); } } // furniture stands there

	EdgeBox(A, B, -HalfT, L + HalfT, HalfT, 0.f, 16.f, T + 8.f, "StoneDark");                       // plinth course
	EdgeWall(A, B, Main0, Main1, HalfT, 0.f, H, T, Win, St.Outer);                                   // masonry
	EdgeBox(A, B, Main0, Main1, HalfT, H, H + 10.f, T + 4.f, "TimberDark");                          // wall plate
	// interior skin (plaster / boards), short of the corners and the broken ends
	EdgeWall(A, B, FMath::Max(Main0, T), FMath::Min(Main1, L - T), T + 1.5f, 8.f, H - 22.f, 3.f, Win, St.Skin);
	for (int32 k = 0; k < Win.Num(); ++k) { EdgeWindow(A, B, Win[k], HalfT, Seed + 40 + k); }

	// broken ends: a lower piece, then a ragged step
	auto Step = [&](float T0, float T1, int32 K, bool bAtStart)
	{
		const float Hs = H * (0.42f + 0.22f * H01(Seed, K));
		EdgeBox(A, B, T0, T1, HalfT, 0.f, Hs, T, St.Outer);
		const float Len = T1 - T0;
		const float U0 = bAtStart ? T0 + Len * 0.45f : T0, U1 = bAtStart ? T1 : T1 - Len * 0.45f;
		EdgeBox(A, B, U0, U1, HalfT, Hs, Hs + (H - Hs) * (0.35f + 0.3f * H01(Seed, K + 1)), T - 3.f, St.Outer);
	};
	if (bStepStart) { Step(-HalfT, SA, 11, true); }
	if (bStepEnd) { Step(L - SB, L + HalfT, 13, false); }

	// a damp patch low on the inside face
	if (H01(Seed, 15) > 0.45f)
	{
		const float C = FMath::Lerp(Main0 + 60.f, Main1 - 60.f, H01(Seed, 16));
		const float W = 70.f + H01(Seed, 17) * 80.f;
		EdgeBox(A, B, C - W * 0.5f, C + W * 0.5f, T + 3.4f, 8.f, 30.f + H01(Seed, 18) * 45.f, 0.6f, "Moss");
	}
}

void ADCHideoutBuilder::RuinEdge(const FVector2D& A, const FVector2D& B, const FGridStyle& St, int32 Seed, float EndA, float EndB)
{
	const float L = FVector2D::Distance(A, B);
	const float HalfT = GWallT * 0.5f;
	EdgeBox(A, B, -HalfT, L + HalfT, HalfT, -4.f, 9.f, GWallT + 12.f, "StoneDark");                 // foundation lip

	// the last course or two of the old wall: a low, slightly uneven stub along the whole edge, the odd gap
	float Tc = -HalfT;
	int32 K = 0;
	while (Tc < L + HalfT - 1.f)
	{
		const float Len = FMath::Min(40.f + H01(Seed, 100 + K) * 50.f, L + HalfT - Tc);
		const float U = FMath::Clamp((Tc + Len * 0.5f) / L, 0.f, 1.f);
		const bool bEnd = Tc < 40.f || Tc + Len > L - 40.f;
		if (bEnd || H01(Seed, 200 + K) < 0.9f)
		{
			const float Hc = FMath::Max(10.f, FMath::Lerp(EndA, EndB, U) + HS(Seed, 300 + K) * 5.f);
			EdgeBox(A, B, Tc, Tc + Len - 1.5f, HalfT + HS(Seed, 400 + K) * 1.5f, 0.f, Hc, GWallT - 2.f, St.Front);
		}
		Tc += Len;
		++K;
	}

	// weeds pushing through outside the stub
	const FVector2D D = (B - A).GetSafeNormal();
	const FVector2D N(-D.Y, D.X);
	const FVector2D WP = A + D * (L * (0.3f + 0.4f * H01(Seed, 1400))) - N * 55.f;
	Weeds(FVector(WP.X, WP.Y, 0.f), 70.f, 9, Seed, 55.f);
}

void ADCHideoutBuilder::PartitionEdge(const FVector2D& A, const FVector2D& B, const FGridStyle& St, int32 Seed)
{
	// Interior wall razed to a low stub; the gap at the midpoint is the doorway pawns walk through.
	const float L = FVector2D::Distance(A, B);
	const float HalfT = GWallT * 0.5f;
	const float D0 = L * 0.5f - GDoorW * 0.5f, D1 = L * 0.5f + GDoorW * 0.5f;
	const float Base = 14.f + H01(Seed, 1) * 6.f;
	auto Run = [&](float T0, float T1, int32 K)
	{
		const int32 Pieces = 3;
		for (int32 p = 0; p < Pieces; ++p)
		{
			const float U0 = FMath::Lerp(T0, T1, (float)p / Pieces), U1 = FMath::Lerp(T0, T1, (float)(p + 1) / Pieces);
			EdgeBox(A, B, U0, U1 - 1.f, 0.f, 0.f, Base + HS(Seed, K + p) * 4.f, GWallT - 4.f, St.Front);
		}
		EdgeBox(A, B, T0, T1, 0.f, 0.f, 8.f, GWallT + 4.f, "StoneDark");
	};
	Run(-HalfT, D0, 10);
	Run(D1, L + HalfT, 20);
	EdgeBox(A, B, D0, D1, 0.f, -1.f, 2.f, GWallT + 10.f, "Timber");                                    // threshold
}

void ADCHideoutBuilder::BracketLamp(const FVector2D& A, const FVector2D& B, float T, float Z, const FLinearColor& Col, float Cd, float Radius)
{
	// a small enamel shade on an iron bracket, on the inside face of the wall A->B (room on the left)
	const float L = FVector2D::Distance(A, B);
	const FVector2D D = (B - A).GetSafeNormal(), N(-D.Y, D.X);
	const FVector2D W = A + D * (L * T) + N * (GWallT + 2.f);
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(N.Y, N.X));
	Deco(EDCShape::Cube, FVector(W.X, W.Y, Z), FVector(4.f, 14.f, 20.f), "Black", FRotator(0, Yaw, 0), false);             // back plate
	const FVector2D Arm = W + N * 22.f;
	Bar(FVector(W.X, W.Y, Z + 4.f), FVector(Arm.X, Arm.Y, Z + 4.f), 3.f, 3.f, "Black", false);
	Deco(EDCShape::Cone, FVector(Arm.X, Arm.Y, Z - 6.f), FVector(22.f, 22.f, 12.f), "Black", FRotator::ZeroRotator, false); // shade
	Deco(EDCShape::Sphere, FVector(Arm.X, Arm.Y, Z - 13.f), FVector(7.f, 7.f, 8.f), "Tungsten", FRotator::ZeroRotator, false);
	Point(FVector(Arm.X, Arm.Y, Z - 20.f), Col, Cd, Radius, true, true, 2.f);
}

void ADCHideoutBuilder::OfficePorchLamps(const FVector2D Q[4])
{
	// Two lantern posts flanking the Office's open front: glass driven by DayNight (LampHead), the light itself is a
	// DayNight street light added once in BuildLightingAndPost (the Office never moves).
	for (int32 k = 0; k < 2; ++k)
	{
		FVector2D P, Out;
		PorchLampSpot(Q, k, P, Out);
		Deco(EDCShape::Cube, FVector(P.X, P.Y, 115.f), FVector(12.f, 12.f, 230.f), "TimberDark");                             // post
		Deco(EDCShape::Cube, FVector(P.X, P.Y, 8.f), FVector(26.f, 26.f, 16.f), "StoneDark");                                 // footing
		const FVector2D Arm = P + Out * 34.f;
		Bar(FVector(P.X, P.Y, 214.f), FVector(Arm.X, Arm.Y, 214.f), 5.f, 5.f, "Black", false);                               // bracket
		Bar(FVector(P.X, P.Y, 190.f), FVector(Arm.X, Arm.Y, 212.f), 2.5f, 2.5f, "Black", false);                             // brace
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Out.Y, Out.X));
		Deco(EDCShape::Cube, FVector(Arm.X, Arm.Y, 207.f), FVector(4.f, 4.f, 10.f), "Black", FRotator::ZeroRotator, false);   // hook
		Deco(EDCShape::Cube, FVector(Arm.X, Arm.Y, 199.f), FVector(20.f, 20.f, 4.f), "Black", FRotator(0, Yaw, 0), false);    // cap
		Deco(EDCShape::Cube, FVector(Arm.X, Arm.Y, 184.f), FVector(14.f, 14.f, 26.f), "LampHead", FRotator(0, Yaw, 0), false);// glass
		Deco(EDCShape::Cube, FVector(Arm.X, Arm.Y, 170.f), FVector(17.f, 17.f, 3.f), "Black", FRotator(0, Yaw, 0), false);    // base
	}
}

void ADCHideoutBuilder::PorchLampSpot(const FVector2D Q[4], int32 K, FVector2D& OutPost, FVector2D& OutDir)
{
	// K = 0: just outside the front-left corner, K = 1: the front-right corner. Room-local (relative to the cell centre).
	const FVector2D C = K == 0 ? Q[0] : Q[1];
	const FVector2D Front = (Q[1] - Q[0]).GetSafeNormal();
	const FVector2D Outward(Front.Y, -Front.X);               // away from the room (-Y-ish)
	const FVector2D Side = K == 0 ? -Front : Front;
	OutPost = C + Outward * 70.f + Side * 30.f;
	OutDir = (Outward * 0.6f + (-Side) * 0.4f).GetSafeNormal(); // the lantern hangs towards the doorway
}

void ADCHideoutBuilder::EdgeScaffold(const FVector2D Q[4], int32 E, float H)
{
	if (E < 0) { return; }
	const FVector2D A = Q[E], B = Q[(E + 1) & 3];
	const FVector2D D = (B - A).GetSafeNormal(), N(-D.Y, D.X); // N points into the room
	// Scaffold() builds its second post line on the left of From->To (= inwards here): start 95 cm outside
	const FVector2D S0 = A + D * 30.f - N * 95.f, S1 = B - D * 30.f - N * 95.f;
	Scaffold(FVector(S0.X, S0.Y, 0.f), FVector(S1.X, S1.Y, 0.f), H);
}

void ADCHideoutBuilder::FrameRoom(const FVector2D Q[4], float H, int32 Seed, int32 ScaffoldEdge)
{
	// slab is the floor (Cement); here: timber frame, studs, a brace, scaffold along the back, a work light, planks
	for (int32 k = 0; k < 4; ++k)
	{
		const FVector2D P = Q[k] + (-Q[k]).GetSafeNormal() * 14.f;
		Deco(EDCShape::Cube, FVector(P.X, P.Y, H * 0.5f), FVector(12.f, 12.f, H), "FreshTimber");
	}
	for (int32 e = 0; e < 4; ++e)
	{
		const FVector2D A = Q[e], B = Q[(e + 1) & 3];
		const float L = FVector2D::Distance(A, B);
		EdgeBox(A, B, 8.f, L - 8.f, 14.f, 0.f, 8.f, 11.f, "FreshTimber");             // sole plate
		EdgeBox(A, B, 8.f, L - 8.f, 14.f, H - 10.f, H, 11.f, "FreshTimber");          // top plate
		if (!DCSite::Grid::IsFrontEdge(e))
		{
			const int32 Studs = FMath::Max(2, FMath::RoundToInt(L / 115.f));
			for (int32 s = 1; s < Studs; ++s) { EdgeBox(A, B, L * s / Studs - 4.f, L * s / Studs + 4.f, 14.f, 8.f, H - 10.f, 9.f, "FreshTimber"); }
		}
	}
	{
		const FVector2D A = Q[1], B = Q[2];
		const FVector2D D = (B - A).GetSafeNormal(), N(-D.Y, D.X);
		const FVector2D P0 = A + D * 20.f + N * 14.f, P1 = A + D * (FVector2D::Distance(A, B) * 0.45f) + N * 14.f;
		Bar(FVector(P0.X, P0.Y, 10.f), FVector(P1.X, P1.Y, H - 12.f), 8.f, 8.f, "FreshTimber");
	}
	EdgeScaffold(Q, ScaffoldEdge, H + 30.f);
	const FVector2D WL = FMath::Lerp(Q[0], Q[2], 0.2f);
	WorkLight(FVector(WL.X, WL.Y, 0.f), FMath::RadiansToDegrees(FMath::Atan2(-WL.Y, -WL.X)), true, 70.f, false);
	// stacked fresh planks and a few blocks waiting to go up
	const FVector2D PL = FMath::Lerp(Q[3], Q[1], 0.62f);
	for (int32 i = 0; i < 4; ++i)
	{
		Deco(EDCShape::Cube, FVector(PL.X, PL.Y, 5.f + i * 7.f), FVector(220.f, 22.f, 6.f), "FreshTimber", FRotator(0.f, 35.f + HS(Seed, 30 + i) * 5.f, 0.f));
	}
	const FVector2D BL = FMath::Lerp(Q[0], Q[2], 0.7f);
	for (int32 i = 0; i < 6; ++i)
	{
		Deco(EDCShape::Cube, FVector(BL.X + (i % 3) * 42.f - (i / 3) * 20.f, BL.Y, 11.f + (i / 3) * 22.f), FVector(40.f, 20.f, 22.f), "Block");
	}
}
