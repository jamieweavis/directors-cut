// Director's Cut — rural site layout. Shared contract between House (Scene builder), Landscape (Scene landscape +
// day/night), World (pawns, vehicles, camera, build placement) and Sim (room placement rules).
//
// The scene: a small abandoned farmhouse in a rural landscape. Camera looks from -X-Y towards +X+Y (yaw 45,
// isometric), so the -X and -Y walls of every room face the camera: those are COLLAPSED (a low stub just above the
// floor) so you see inside, and partitions between rooms are low stubs too. The +X/+Y walls are the tall back walls.
//
// v3 (Nov 14): the house grows on an irregular BUILD GRID (DCSite::Grid). Every room is one cell. The Office (the
// original house) sits in the front row at cell (0,0), facing the lane (-Y). New rooms go on any free cell that
// touches an existing room: left (-X), right (+X) or behind (+Y), never in front of the front row. Grid corners are
// jittered so walls run slightly off-square and the building outline is irregular.
//
// Outside: a gravel yard in front of the house where one car per available mission slot is parked in a row of angled
// bays; a one-way loop and a two-lane dirt driveway (at the +X end) join a country lane that runs along X and leaves
// the screen at both ends. Crew walk from the house to a car, get in, it pulls forwards out of the bay and drives off
// along the lane towards +X; on return it arrives from -X, drives round the loop and parks nose first.
// All units cm, world space. Ground (and every floor top) is Z = 0.
#pragma once

#include "CoreMinimal.h"
#include "Core/DCTypes.h"

namespace DCSite
{
	constexpr float GroundZ = 0.f;

	/** LEGACY fixed footprints (v2, before the build grid). Only the unused authored room dressing still reads these;
	 *  live rooms are placed on DCSite::Grid. */
	constexpr float RoomScale = 0.65f;
	inline FBox2D RoomRect(EDCRoomType T)
	{
		auto R = [](float X0, float X1, float Y0, float Y1) { return FBox2D(FVector2D(X0, Y0) * RoomScale, FVector2D(X1, Y1) * RoomScale); };
		switch (T)
		{
		case EDCRoomType::Office:       return R(-500,  500,  -400,  400);   // X[-325,325]  Y[-260,260]
		case EDCRoomType::Workshop:     return R(-500,  500, -1400, -400);   // X[-325,325]  Y[-910,-260]
		case EDCRoomType::Garage:       return R( 500, 1300, -1400,  400);   // X[325,845]   Y[-910,260]
		case EDCRoomType::OpsRoom:      return R(-500, 1300,   400, 1600);   // X[-325,845]  Y[260,1040]
		case EDCRoomType::Lounge:       return R(-1300, -500, -1400, -400);  // X[-845,-325] Y[-910,-260]
		case EDCRoomType::ServerRoom:   return R(-1300, -500,  -400,  400);  // X[-845,-325] Y[-260,260]
		case EDCRoomType::LawyerOffice: return R(-1300, -500,   400, 1000);  // X[-845,-325] Y[260,650]
		case EDCRoomType::Clinic:       return R(-1300, -500,  1000, 1600);  // X[-845,-325] Y[650,1040]
		default:                        return R(-500,  500,  -400,  400);
		}
	}
	// ── Build grid ──────────────────────────────────────────────────────────────────────────────────────────────
	namespace Grid
	{
		constexpr float CellSize = 580.f;     // nominal cell (the Office is roughly 5.8 x 5.8 m)
		constexpr float Jitter = 31.f;       // max corner offset (cm) per axis
		constexpr float WallT = 24.f;        // wall thickness
		/** Doorway between two finished rooms: a gap of this width centred on their shared edge. House cuts it,
		 *  World's nav map (FDCNavGrid) leaves the same gap open. */
		constexpr float DoorW = 150.f;
		constexpr int32 MinCol = -2, MaxCol = 2;
		constexpr int32 MinRow = 0, MaxRow = 3; // row 0 = the front row (the Office's row); rows grow behind (+Y)
		/** World position of the centre of cell (0,0) before jitter. The house sits forward of the world origin, close
		 *  to the yard, so the walk from the front door to the cars is short. Everything on the grid derives from it. */
		constexpr float OriginX = 0.f;
		constexpr float OriginY = -560.f;
		inline FIntPoint OfficeCell() { return FIntPoint(0, 0); }
		/** Sentinel for "not placed". */
		inline FIntPoint NoCell() { return FIntPoint(-999, -999); }

		inline bool IsInGrid(const FIntPoint& C) { return C.X >= MinCol && C.X <= MaxCol && C.Y >= MinRow && C.Y <= MaxRow; }

		inline float Hash01(int32 A, int32 B, int32 Salt)
		{
			uint32 H = ((uint32)A * 73856093u) ^ ((uint32)B * 19349663u) ^ ((uint32)Salt * 83492791u);
			H ^= H >> 13; H *= 0x5bd1e995u; H ^= H >> 15;
			return (float)(H & 0xFFFFFF) / (float)0xFFFFFF;
		}
		/** Lattice corner (I, J) = the -X-Y corner of cell (I, J). Corners are jittered (deterministic). */
		inline FVector2D Vertex(int32 I, int32 J)
		{
			const FVector2D Base(OriginX + (I - 0.5f) * CellSize, OriginY + (J - 0.5f) * CellSize);
			return Base + FVector2D((Hash01(I, J, 1) * 2.f - 1.f) * Jitter, (Hash01(I, J, 2) * 2.f - 1.f) * Jitter);
		}
		/** Cell corners, counter-clockwise: 0 (-X-Y), 1 (+X-Y), 2 (+X+Y), 3 (-X+Y).
		 *  Edge e runs Corner[e] -> Corner[(e+1)%4]: 0 = -Y (front), 1 = +X, 2 = +Y (back), 3 = -X. */
		inline void Corners(const FIntPoint& C, FVector2D Out[4])
		{
			Out[0] = Vertex(C.X, C.Y); Out[1] = Vertex(C.X + 1, C.Y); Out[2] = Vertex(C.X + 1, C.Y + 1); Out[3] = Vertex(C.X, C.Y + 1);
		}
		inline FVector2D Centre(const FIntPoint& C)
		{
			FVector2D Q[4]; Corners(C, Q);
			return (Q[0] + Q[1] + Q[2] + Q[3]) * 0.25f;
		}
		/** Neighbour across edge e (see Corners). */
		inline FIntPoint Neighbour(const FIntPoint& C, int32 Edge)
		{
			static const FIntPoint D[4] = { FIntPoint(0, -1), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0) };
			return C + D[Edge & 3];
		}
		/** Edges facing the camera (-Y, -X) are the collapsed open fronts. */
		inline bool IsFrontEdge(int32 Edge) { return Edge == 0 || Edge == 3; }
		/** Per-cell wall height (cm): a little different everywhere so the roofline is uneven. */
		inline float WallHeight(const FIntPoint& C) { return (C == OfficeCell() ? 285.f : 248.f) + Hash01(C.X, C.Y, 7) * 34.f; }

		inline bool InsideQuad(const FVector2D Q[4], const FVector2D& P, float Tolerance = 0.f)
		{
			for (int32 e = 0; e < 4; ++e)
			{
				const FVector2D A = Q[e], B = Q[(e + 1) & 3];
				const FVector2D D = (B - A).GetSafeNormal();
				if (FVector2D::CrossProduct(D, P - A) < -Tolerance) { return false; }
			}
			return true;
		}
		/** Grid cell containing P (false if outside the grid). */
		inline bool CellAt(const FVector2D& P, FIntPoint& Out)
		{
			const int32 CI = FMath::RoundToInt((P.X - OriginX) / CellSize), CJ = FMath::RoundToInt((P.Y - OriginY) / CellSize);
			for (int32 dy = -1; dy <= 1; ++dy)
			{
				for (int32 dx = -1; dx <= 1; ++dx)
				{
					const FIntPoint C(CI + dx, CJ + dy);
					if (!IsInGrid(C)) { continue; }
					FVector2D Q[4]; Corners(C, Q);
					if (InsideQuad(Q, P)) { Out = C; return true; }
				}
			}
			return false;
		}
		/** A new room may go on a free in-grid cell that shares an edge with an existing room. */
		inline bool CanPlace(const FIntPoint& C, const TArray<FIntPoint>& Occupied)
		{
			if (!IsInGrid(C) || Occupied.Contains(C)) { return false; }
			for (int32 e = 0; e < 4; ++e) { if (Occupied.Contains(Neighbour(C, e))) { return true; } }
			return false;
		}
		inline void ValidCells(const TArray<FIntPoint>& Occupied, TArray<FIntPoint>& Out)
		{
			Out.Reset();
			for (int32 Y = MinRow; Y <= MaxRow; ++Y) { for (int32 X = MinCol; X <= MaxCol; ++X) { if (CanPlace(FIntPoint(X, Y), Occupied)) { Out.Add(FIntPoint(X, Y)); } } }
		}
		/** Deterministic default spot (dev flags, saves from before the grid, tests): nearest the house, alternating sides. */
		inline FIntPoint AutoPick(const TArray<FIntPoint>& Occupied)
		{
			TArray<FIntPoint> V; ValidCells(Occupied, V);
			FIntPoint Best = NoCell(); float BestScore = 1e9f;
			for (const FIntPoint& C : V)
			{
				const float S = FMath::Abs(C.X) * 1.1f + C.Y * 1.0f + (C.X < 0 ? 0.05f : 0.f) + Hash01(C.X, C.Y, 11) * 0.3f;
				if (S < BestScore) { BestScore = S; Best = C; }
			}
			return Best;
		}
		/** World AABB of the whole grid (incl. jitter). */
		inline FBox2D Bounds()
		{
			return FBox2D(FVector2D(OriginX + (MinCol - 0.5f) * CellSize - Jitter, OriginY + (MinRow - 0.5f) * CellSize - Jitter),
				FVector2D(OriginX + (MaxCol + 0.5f) * CellSize + Jitter, OriginY + (MaxRow + 0.5f) * CellSize + Jitter));
		}
	}

	/** Where the fully expanded building can reach (the whole build grid). */
	inline FBox2D CompoundRect() { return Grid::Bounds(); }

	/** Gravel yard where cars park and circulate (Landscape draws it; nothing tall inside it). It covers the whole
	 *  one-way loop: lower lane, U-turn at the -X end, the bay row and the out lane (see the car park below). */
	inline FBox2D YardRect() { return FBox2D(FVector2D(-2210, -2310), FVector2D(2090, -1060)); }

	/** The Office's front door: a doorway cut into its collapsed front (-Y) wall, centred on that edge. House builds
	 *  the frame, World sends crew out through it, Landscape lays the footpath from it to the yard. */
	constexpr float FrontDoorW = 130.f;
	inline FVector2D FrontDoor()
	{
		FVector2D Q[4]; Grid::Corners(Grid::OfficeCell(), Q);
		return (Q[0] + Q[1]) * 0.5f;
	}
	/** Footpath from the front door to the car park: door, a slight bend, then the yard edge. It veers towards -X
	 *  (where the first cars park) and so passes clear of the middle festoon pole. */
	inline void FrontPath(FVector2D Out[3])
	{
		const FVector2D D = FrontDoor();
		Out[0] = D;
		Out[1] = FVector2D(D.X - 25.f, D.Y - 120.f);
		Out[2] = FVector2D(D.X - 185.f, YardRect().Max.Y + 30.f);
	}

	/** Festoon poles between the house and the yard (just outside it, so nothing tall stands where cars drive). */
	inline TArray<FVector2D> YardFestoonPoles()
	{
		return { FVector2D(-1250.f, -945.f), FVector2D(150.f, -945.f), FVector2D(1350.f, -945.f) };
	}

	/** Keep-clear zone around the compound, down to the yard: Landscape places no trees/fences/rocks inside it
	 *  (grass/flowers ok). */
	inline FBox2D BuildingClearRect()
	{
		const FBox2D C = CompoundRect();
		return FBox2D(FVector2D(C.Min.X - 300.f, FMath::Min(YardRect().Max.Y, C.Min.Y - 150.f)), C.Max + FVector2D(300.f, 300.f));
	}

	// ── Car park ────────────────────────────────────────────────────────────────────────────────────────────────
	// One row of angled bays in the yard, served by a ONE-WAY loop so cars only ever drive forwards:
	//   IN:  lane (arriving from -X) -> the driveway's in lane (its -X side) -> left along the LOWER yard lane -> U-turn
	//        at the -X end -> right along the REAR aisle (house side) -> nose first into the bay (slanted, nose towards
	//        +X and the lane).
	//   OUT: pull forwards straight through the bay -> right along the UPPER yard lane (in front of the noses) -> down
	//        the driveway's out lane (its +X side) -> right onto the lane, off towards +X.
	// In and out traffic never cross: the lower lane carries arrivals, the upper lane departures, and the driveway has a
	// lane for each. All routes are traced by the car's REAR axle (DCVehicle turns the body about it like a real car).
	// Checked offline for every bay and car style: >= 33 cm from parked cars and the gateway, <= 48 deg of steer.
	constexpr int32 MaxParkingSlots = 8;
	constexpr float BaySlant = 55.f;          // bay axis vs the aisles (deg)
	constexpr float BayPitch = 270.f;         // centre spacing measured across the cars (old row: 225)
	constexpr float BayRowY = -1600.f;        // bay centres
	constexpr float Bay0X = -1120.f;          // centre of bay 0 (bays run towards +X)
	constexpr float AisleFrontY = BayRowY - 370.f;    // upper yard lane: departures (-1970)
	constexpr float AisleRearY = BayRowY + 420.f;     // rear aisle: arrivals turning in (-1180)
	constexpr float InLaneY = AisleFrontY - 240.f;    // lower yard lane: arrivals (-2210), a car width + 64 cm below
	constexpr float YardTurnR = 330.f;        // rear-axle radius of the yard corners (~40 deg of lock on a long car)
	constexpr float LaneTurnR = 430.f;        // turning off / onto the lane
	constexpr float UTurnR = 340.f;           // the U-turn at the -X end (two quarter turns with a short straight)
	inline float BaySpacing() { return BayPitch / FMath::Sin(FMath::DegreesToRadians(BaySlant)); }
	/** Bay b (0..7, -X to +X): car centre on the ground, nose slanted towards +X and the lane. */
	inline FTransform ParkingBay(int32 B)
	{
		const int32 C = FMath::Clamp(B, 0, MaxParkingSlots - 1);
		return FTransform(FRotator(0.f, -BaySlant, 0.f), FVector(Bay0X + BaySpacing() * C, BayRowY, GroundZ));
	}
	/** Mission slot -> bay: the first cars park nearest the front door (middle of the row), later ones further out. */
	inline int32 SlotBay(int32 Slot)
	{
		static const int32 Order[MaxParkingSlots] = { 3, 4, 2, 5, 1, 6, 0, 7 };
		return Order[FMath::Clamp(Slot, 0, MaxParkingSlots - 1)];
	}
	/** Parking slot i (0..7): the transform of its bay. */
	inline FTransform ParkingSlot(int32 I) { return ParkingBay(SlotBay(I)); }

	/** Country lane: straight along X at LaneY, from LaneX0 to LaneX1 (both well off-screen). Width LaneW. */
	constexpr float LaneY = -2950.f;
	constexpr float LaneW = 520.f;
	constexpr float LaneX0 = -14000.f;
	constexpr float LaneX1 = 14000.f;
	/** Cars drive on the house-side half of the lane; arrivals and departures both head +X, so they never meet. */
	constexpr float LaneTravelY = LaneY + 110.f;
	/** Dirt driveway from the yard to the lane at the +X end of the yard, centred on X = DriveX. Two lanes: in on its
	 *  -X side, out on its +X side (DriveX -+ DriveLaneHalf). */
	constexpr float DriveX = 1805.f;
	constexpr float DriveW = 520.f;
	constexpr float DriveLaneHalf = 165.f;
	inline float DriveInX() { return DriveX - DriveLaneHalf; }
	inline float DriveOutX() { return DriveX + DriveLaneHalf; }

	/** Post-and-rail fence on the house side of the lane, with a gateway where the driveway crosses it. The five-bar
	 *  gate hangs on the -X gate post and stands swung right back OUTWARDS (towards the lane verge, away from the
	 *  yard and the turn onto the lane) so nothing a car drives over is anywhere near it. */
	constexpr float FenceY = LaneY + 380.f;
	constexpr float GateHalf = 380.f;      // gate posts at DriveX +- GateHalf
	constexpr float GateLeafLen = 330.f;
	constexpr float GateOpenYaw = -158.f;  // leaf direction from the hinge (0 = closed, across the gateway towards +X)
	inline FVector2D GateHinge() { return FVector2D(DriveX - GateHalf + 14.f, FenceY); }
	inline FVector2D GateLeafEnd()
	{
		const float A = FMath::DegreesToRadians(GateOpenYaw);
		return GateHinge() + FVector2D(FMath::Cos(A), FMath::Sin(A)) * GateLeafLen;
	}
	/** Things a car must never touch around the gateway (centre, radius): both gate posts, the leaf (sampled), the
	 *  mailbox. Used by the vehicle route check. */
	inline FVector2D MailboxPos() { return FVector2D(DriveX + GateHalf + 170.f, FenceY - 45.f); }
	inline void GateObstacles(TArray<FVector4f>& Out)
	{
		Out.Reset();
		Out.Add(FVector4f(DriveX - GateHalf, FenceY, 14.f, 0.f));
		Out.Add(FVector4f(DriveX + GateHalf, FenceY, 14.f, 0.f));
		const FVector2D H = GateHinge(), E = GateLeafEnd();
		for (int32 i = 0; i <= 16; ++i) { const FVector2D P = FMath::Lerp(H, E, i / 16.f); Out.Add(FVector4f(P.X, P.Y, 6.f, 0.f)); }
		const FVector2D MB = MailboxPos();
		Out.Add(FVector4f(MB.X, MB.Y, 30.f, 0.f));
	}

	/** Off-screen ends of the lane: arrivals appear at ArriveFromX, departures vanish past LeaveToX. */
	constexpr float ArriveFromX = -7000.f;
	constexpr float LeaveToX = 7000.f;

	/** Where a bay's axis crosses the line Y = AtY (the turn in from the rear aisle / out onto the front lane). */
	inline FVector2D BayAxisAtY(int32 B, float AtY)
	{
		const FTransform T = ParkingBay(B);
		const FVector F = T.GetRotation().GetForwardVector();
		const float K = (AtY - T.GetLocation().Y) / F.Y;
		return FVector2D(T.GetLocation().X + F.X * K, AtY);
	}
	/** Rear-axle route as corners + fillet radius per corner (0 at the ends); DCVehicle rounds each corner with an arc
	 *  and eases the joins. Arriving: from off-screen into bay B, ending parked (centre on the bay). Departing: from
	 *  parked in bay B, forwards through the bay and away. WheelBase places the rear axle (half of it behind the centre). */
	inline void CarRoute(int32 B, bool bArrive, float WheelBase, TArray<FVector2D>& Corners, TArray<float>& Radii)
	{
		Corners.Reset(); Radii.Reset();
		const FTransform T = ParkingBay(B);
		const FVector F3 = T.GetRotation().GetForwardVector();
		const FVector2D F(F3.X, F3.Y), C(T.GetLocation().X, T.GetLocation().Y);
		const FVector2D Rear = C - F * (WheelBase * 0.5f);
		const float TanHalf = FMath::Tan(FMath::DegreesToRadians(BaySlant * 0.5f));
		auto Add = [&](const FVector2D& P, float R) { Corners.Add(P); Radii.Add(R); };
		if (bArrive)
		{
			// U-turn at the -X end: up from the lower lane to the rear aisle, just clear of bay 0's turn in.
			const float UR = UTurnR;
			const float UX = BayAxisAtY(0, AisleRearY).X - YardTurnR * TanHalf - UR - 20.f;
			Add(FVector2D(ArriveFromX, LaneTravelY), 0.f);
			Add(FVector2D(DriveInX(), LaneTravelY), LaneTurnR);
			Add(FVector2D(DriveInX(), InLaneY), YardTurnR);
			Add(FVector2D(UX, InLaneY), UR);
			Add(FVector2D(UX, AisleRearY), UR);
			Add(BayAxisAtY(B, AisleRearY), YardTurnR);
			Add(Rear, 0.f);
		}
		else
		{
			Add(Rear, 0.f);
			Add(BayAxisAtY(B, AisleFrontY), YardTurnR);
			Add(FVector2D(DriveOutX(), AisleFrontY), YardTurnR);
			Add(FVector2D(DriveOutX(), LaneTravelY), LaneTurnR);
			Add(FVector2D(LeaveToX, LaneTravelY), 0.f);
		}
	}

	/** Where crew stand to board the car in slot I: beside its doors in the gap between the bays, alternating sides
	 *  (even seats on the car's left, odd on its right), further back for the back seats. */
	inline FVector BoardingPoint(int32 I, int32 Seat)
	{
		const FTransform T = ParkingSlot(I);
		const float Side = (Seat % 2 == 0) ? -1.f : 1.f;
		return T.TransformPositionNoScale(FVector(30.f - 90.f * (Seat / 2), Side * 135.f, 0.f));
	}
	/** The car-park area worth keeping in shot (the bay row and its aisles). */
	inline FBox2D ParkingFrameRect()
	{
		return FBox2D(FVector2D(ParkingBay(0).GetLocation().X - 400.f, InLaneY - 100.f), FVector2D(ParkingBay(MaxParkingSlots - 1).GetLocation().X + 400.f, AisleRearY + 100.f));
	}

	/** Walkable outdoor area pawns may use outside the building (yard + a margin around the compound). */
	inline FBox2D OutdoorWalkRect() { const FBox2D C = CompoundRect(), Y = YardRect(); return FBox2D(FVector2D(FMath::Min(Y.Min.X, C.Min.X - 400.f), Y.Min.Y - 40.f), FVector2D(FMath::Max(Y.Max.X, C.Max.X + 400.f), C.Max.Y + 400.f)); }

	/** Default camera bounds: compound + yard + a strip of lane, with room to see the landscape. */
	inline FBox CameraBounds() { return FBox(FVector(-2900, -3700, 0), FVector(2900, 3200, 400)); }
}
