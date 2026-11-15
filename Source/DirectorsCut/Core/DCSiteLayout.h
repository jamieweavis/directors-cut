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
// Outside: a gravel yard in front of the house where one car per available mission slot is parked; a dirt
// driveway joins a country lane that runs along X and leaves the screen at both ends. Crew walk from the house to
// a car, get in, and it drives off along the lane; on return the car comes back the same way and parks.
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

	/** Gravel yard where cars park (Landscape draws it; nothing tall inside it). */
	inline FBox2D YardRect() { return FBox2D(FVector2D(-950, -2050), FVector2D(900, -1200)); }

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

	/** Festoon poles along the house side of the yard (just outside it, so nothing tall stands where cars drive). */
	inline TArray<FVector2D> YardFestoonPoles()
	{
		const FBox2D Y = YardRect();
		return { FVector2D(Y.Min.X - 60.f, Y.Max.Y + 40.f), FVector2D(FMath::Lerp(Y.Min.X, Y.Max.X, 0.5f), Y.Max.Y + 40.f), FVector2D(Y.Max.X + 60.f, Y.Max.Y + 40.f) };
	}

	/** Keep-clear zone around the compound, down to the yard: Landscape places no trees/fences/rocks inside it
	 *  (grass/flowers ok). */
	inline FBox2D BuildingClearRect()
	{
		const FBox2D C = CompoundRect();
		return FBox2D(FVector2D(C.Min.X - 300.f, FMath::Min(YardRect().Max.Y, C.Min.Y - 150.f)), C.Max + FVector2D(300.f, 300.f));
	}

	constexpr int32 MaxParkingSlots = 8;
	/** Parking slot i (0..7): car centre on the ground, nose facing -Y (towards the lane). Slot 0 is the first car. */
	inline FTransform ParkingSlot(int32 I)
	{
		const int32 C = FMath::Clamp(I, 0, MaxParkingSlots - 1);
		const float X = -770.f + 225.f * C;   // -770 .. 805
		return FTransform(FRotator(0.f, -90.f, 0.f), FVector(X, -1600.f, GroundZ));
	}

	/** Country lane: straight along X at LaneY, from LaneX0 to LaneX1 (both well off-screen). Width LaneW. */
	constexpr float LaneY = -2650.f;
	constexpr float LaneW = 520.f;
	constexpr float LaneX0 = -14000.f;
	constexpr float LaneX1 = 14000.f;
	/** Dirt driveway from the yard to the lane, centred on X = DriveX, from yard edge (Y=-2050) to the lane. */
	constexpr float DriveX = -100.f;
	constexpr float DriveW = 380.f;

	/** Post-and-rail fence on the house side of the lane, with a gateway where the driveway crosses it. The five-bar
	 *  gate hangs on the -X gate post and stands swung right back OUTWARDS (towards the lane verge, away from the
	 *  yard and the turn onto the lane) so nothing a car drives over is anywhere near it. */
	constexpr float FenceY = LaneY + 380.f;
	constexpr float GateHalf = 320.f;      // gate posts at DriveX +- GateHalf
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

	/** Route a car drives when LEAVING from slot I: out of the slot, down the driveway, onto the lane (left-hand
	 *  lane side, heading +X), and off-screen. Returning cars drive the exact reverse (arrive from +X). */
	inline void DepartRoute(int32 I, TArray<FVector>& Out)
	{
		Out.Reset();
		const FVector P = ParkingSlot(I).GetLocation();
		Out.Add(P);
		Out.Add(FVector(P.X, -1960.f, GroundZ));                 // pull forward out of the bay (clear of the neighbours' noses)
		Out.Add(FVector(DriveX, FenceY + 250.f, GroundZ));       // line up with the driveway
		Out.Add(FVector(DriveX, FenceY - 120.f, GroundZ));       // down the driveway, straight through the gateway
		Out.Add(FVector(DriveX + 400.f, LaneY + 110.f, GroundZ));// turn onto the lane
		Out.Add(FVector(LaneX1 - 1000.f, LaneY + 110.f, GroundZ));// and away
	}

	/** Where crew walk to board a car in slot I (beside the car, house side). */
	inline FVector BoardingPoint(int32 I, int32 Seat)
	{
		const FVector P = ParkingSlot(I).GetLocation();
		const float Side = (Seat % 2 == 0) ? -1.f : 1.f; // alternate sides of the car
		return FVector(P.X + Side * 120.f, P.Y + 60.f + 90.f * (Seat / 2), GroundZ);
	}

	/** Walkable outdoor area pawns may use outside the building (yard + a margin around the compound). */
	inline FBox2D OutdoorWalkRect() { const FBox2D C = CompoundRect(); return FBox2D(FVector2D(FMath::Min(-1250.f, C.Min.X - 400.f), -2100.f), C.Max + FVector2D(400.f, 400.f)); }

	/** Default camera bounds: compound + yard + a strip of lane, with room to see the landscape. */
	inline FBox CameraBounds() { return FBox(FVector(-2900, -3400, 0), FVector(2900, 3200, 400)); }
}
