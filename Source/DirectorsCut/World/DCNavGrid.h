// Director's Cut — the crew's walkable tile map (World). Plain C++, no UObjects, so it is cheap to rebuild and test.
//
// The map is GENERATED from the house, never authored: ADCWorldDirector::BuildNav stamps every standing wall (with the
// doorway gap the builder cut into it), every furniture footprint, the parked cars and the yard posts onto a grid of
// 20 cm tiles, then Finish() derives two things per tile:
//   Clear      distance to the nearest blocked tile (0 = blocked, 1 = touching, 2, 3 = open floor)
//   Component  which connected walkable area the tile belongs to (reachability in O(1))
// Obstacles are grown by the pawn radius while stamping, so a pawn whose CENTRE stays on free tiles never clips a wall.
//
// Routes: A* over the tiles (tiles that hug an obstacle cost more, so routes run down the middle of doorways and
// lanes), then the tile chain is pulled tight into a few straight legs. Since walls are solid except at doorways,
// every route between rooms goes through the doorways, and every route in or out of the house through the front door.
// Start / goal points that sit on a blocked tile (a bed, a spot at a desk, beside a car) are joined to the nearest
// free tile on their own side of the wall.
#pragma once

#include "CoreMinimal.h"

/** A doorway the map was built with (debug view + tests). */
struct FDCNavDoor
{
	FVector2D Centre = FVector2D::ZeroVector;
	FVector2D Along = FVector2D(1, 0); // unit, along the wall
	float Width = 0.f;
	bool bFront = false;               // the house's front door
};

class DIRECTORSCUT_API FDCNavGrid
{
public:
	static constexpr float Cell = 20.f;
	/** Pawn body radius every obstacle is grown by. */
	static constexpr float Radius = 22.f;

	// ── Building ────────────────────────────────────────────────────────────────────────────────────────────────
	void Begin(const FBox2D& InBounds);
	/** Solid convex quad (counter-clockwise), grown by Radius. */
	void BlockQuad(const FVector2D Q[4]);
	/** Piece of the wall on edge A -> B from T0 to T1 (cm along the edge). The wall body reaches In cm to the left of
	 *  the edge (the room side for counter-clockwise rooms) and Out cm to the right; all grown by Radius. */
	void BlockWall(const FVector2D& A, const FVector2D& B, float T0, float T1, float In, float Out);
	/** Oriented box (centre, half extents, yaw in radians), grown by Pad. */
	void BlockBox(const FVector2D& Centre, const FVector2D& Half, float YawRad, float Pad);
	void BlockDisc(const FVector2D& Centre, float R);
	/** Tiles inside the quad belong to Region (>= 0; outdoors is -1). Keeps start/goal snapping on the right side of a wall. */
	void SetRegion(const FVector2D Q[4], int32 Region);
	void AddDoor(const FDCNavDoor& D) { Doors.Add(D); }
	/** Derive clearance + connectivity. Call once after the last Block*. */
	void Finish();

	// ── Queries ─────────────────────────────────────────────────────────────────────────────────────────────────
	bool IsBuilt() const { return bBuilt; }
	bool Contains(const FVector2D& P) const { return P.X >= Origin.X && P.Y >= Origin.Y && P.X < Origin.X + Nx * Cell && P.Y < Origin.Y + Ny * Cell; }
	/** Free to stand on (points off the map count as free: the lane beyond the yard is open ground). */
	bool IsFree(const FVector2D& P) const;
	/** A route exists between the two points (after snapping each to free floor). */
	bool IsReachable(const FVector2D& A, const FVector2D& B) const;
	/** Straight walk A -> B stays on tiles with at least MinClear clearance. */
	bool SegmentClear(const FVector2D& A, const FVector2D& B, uint8 MinClear = 1) const;
	/** Route From -> To as a few straight legs (excludes From, ends with To). False = no map / nowhere to stand; the
	 *  caller decides the fallback. An unreachable goal still yields a route to the closest reachable floor, then To. */
	bool FindPath(const FVector2D& From, const FVector2D& To, TArray<FVector2D>& Out) const;

	// ── Introspection (debug view, tests) ───────────────────────────────────────────────────────────────────────
	int32 SizeX() const { return Nx; }
	int32 SizeY() const { return Ny; }
	FVector2D GetOrigin() const { return Origin; }
	uint8 ClearAt(int32 X, int32 Y) const { return (X >= 0 && Y >= 0 && X < Nx && Y < Ny) ? Clear[Y * Nx + X] : 3; }
	int32 RegionAt(int32 X, int32 Y) const { return (X >= 0 && Y >= 0 && X < Nx && Y < Ny) ? Region[Y * Nx + X] : -1; }
	const TArray<FDCNavDoor>& GetDoors() const { return Doors; }
	int32 NumFree() const { return FreeCount; }
	int32 NumComponents() const { return ComponentCount; }
	/** Stats of the last FindPath (debug view). */
	mutable int32 LastExpanded = 0;

private:
	FVector2D Origin = FVector2D::ZeroVector;
	int32 Nx = 0, Ny = 0;
	bool bBuilt = false;
	TArray<uint8> Clear;      // 0 blocked .. 3 open
	TArray<int8> Region;
	TArray<uint16> Component; // 0 = blocked
	TArray<FDCNavDoor> Doors;
	int32 FreeCount = 0, ComponentCount = 0;

	// A* scratch (generation-stamped so a search never clears the whole map)
	mutable TArray<float> GCost;
	mutable TArray<int32> Parent;
	mutable TArray<uint32> Stamp;
	mutable uint32 Generation = 0;

	FVector2D CellCentre(int32 I) const { return Origin + FVector2D((I % Nx + 0.5f) * Cell, (I / Nx + 0.5f) * Cell); }
	int32 CellIndexClamped(const FVector2D& P) const;
	/** Nearest free tile to P, preferring P's own region. INDEX_NONE if none within reach. */
	int32 SnapFree(const FVector2D& P) const;
	bool Line(const FVector2D& A, const FVector2D& B, uint8 MinClear, int32 SkipA, int32 SkipB) const;
	template <typename FInside> void Stamp2D(const FBox2D& Box, FInside&& Inside);
};
