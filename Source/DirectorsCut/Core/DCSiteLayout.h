// Director's Cut — rural site layout. LEAD-OWNED, FROZEN CONTRACT shared by House (Scene builder), Landscape
// (Scene landscape + day/night) and World (pawns, vehicles, camera). Request changes, don't edit.
//
// The scene: a small abandoned farmhouse in the middle of a rural landscape. Camera looks from -X-Y towards +X+Y
// (yaw 45, isometric), so the -X and -Y walls of every building face the camera: those are COLLAPSED (knee-high
// broken stubs + rubble) so you see inside. The +X/+Y walls are the tall back walls.
//
// The Office is the original house (always built). Every other room is a WING that does not exist until built:
// Locked = empty overgrown plot (stakes + string line), Building = timber frame + scaffold, Level1+ = finished wing
// attached to the house. So the building visibly expands as you build.
//
// Outside: a gravel yard (front-right on screen) where one car per available mission slot is parked; a dirt
// driveway joins a country lane that runs along X and leaves the screen at both ends. Crew walk from the house to
// a car, get in, and it drives off along the lane; on return the car comes back the same way and parks.
// All units cm, world space. Ground (and every floor top) is Z = 0.
#pragma once

#include "CoreMinimal.h"
#include "Core/DCTypes.h"

namespace DCSite
{
	constexpr float GroundZ = 0.f;

	/** Room / wing footprints (world XY). The Office is the original house. Wings butt against it or each other.
	 *  v2 (Nov 14): everything is 0.65x the old warehouse grid — a SMALL house (office 6.5 x 5.2 m). */
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
	/** Union of every room footprint (the fully expanded building). */
	inline FBox2D CompoundRect() { return FBox2D(FVector2D(-845, -910), FVector2D(845, 1040)); }

	/** Keep-clear zone around the compound: Landscape places no trees/fences/rocks inside it (grass/flowers ok). */
	inline FBox2D BuildingClearRect() { return FBox2D(FVector2D(-1150, -1200), FVector2D(1150, 1350)); }

	/** Gravel yard where cars park (Landscape draws it; nothing tall inside it). */
	inline FBox2D YardRect() { return FBox2D(FVector2D(-950, -2050), FVector2D(900, -1200)); }

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

	/** Route a car drives when LEAVING from slot I: out of the slot, down the driveway, onto the lane (left-hand
	 *  lane side, heading +X), and off-screen. Returning cars drive the exact reverse (arrive from +X). */
	inline void DepartRoute(int32 I, TArray<FVector>& Out)
	{
		Out.Reset();
		const FVector P = ParkingSlot(I).GetLocation();
		Out.Add(P);
		Out.Add(FVector(P.X, -1880.f, GroundZ));                 // pull forward out of the bay
		Out.Add(FVector(DriveX, -2020.f, GroundZ));              // line up with the driveway
		Out.Add(FVector(DriveX, LaneY + 260.f, GroundZ));        // down the driveway
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
	inline FBox2D OutdoorWalkRect() { return FBox2D(FVector2D(-1250, -2100), FVector2D(1250, 1400)); }

	/** Default camera bounds: compound + yard + a strip of lane, with room to see the landscape. */
	inline FBox CameraBounds() { return FBox(FVector(-2600, -3400, 0), FVector(2600, 2600, 400)); }
}
