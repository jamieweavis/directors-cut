#include "World/DCNavGrid.h"
#include "Algo/Reverse.h"

namespace
{
	/** Walking cost per tile by clearance: hugging an obstacle is expensive, open floor is cheap. */
	constexpr float ClearCost[4] = { 1e6f, 2.6f, 1.45f, 1.f };
	constexpr float Diag = 1.41421356f;
	constexpr int32 SnapRings = 12; // 2.4 m

	struct FOpen { float F; int32 I; };
	struct FOpenLess { bool operator()(const FOpen& A, const FOpen& B) const { return A.F < B.F; } };
}

void FDCNavGrid::Begin(const FBox2D& InBounds)
{
	Origin = InBounds.Min;
	Nx = FMath::Max(1, FMath::CeilToInt((InBounds.Max.X - InBounds.Min.X) / Cell));
	Ny = FMath::Max(1, FMath::CeilToInt((InBounds.Max.Y - InBounds.Min.Y) / Cell));
	const int32 N = Nx * Ny;
	Clear.Init(3, N);
	Region.Init(-1, N);
	Component.Init(0, N);
	Doors.Reset();
	bBuilt = false;
	FreeCount = ComponentCount = 0;
}

template <typename FInside>
void FDCNavGrid::Stamp2D(const FBox2D& Box, FInside&& Inside)
{
	const int32 X0 = FMath::Max(0, FMath::FloorToInt((Box.Min.X - Origin.X) / Cell)), X1 = FMath::Min(Nx - 1, FMath::FloorToInt((Box.Max.X - Origin.X) / Cell));
	const int32 Y0 = FMath::Max(0, FMath::FloorToInt((Box.Min.Y - Origin.Y) / Cell)), Y1 = FMath::Min(Ny - 1, FMath::FloorToInt((Box.Max.Y - Origin.Y) / Cell));
	for (int32 Y = Y0; Y <= Y1; ++Y)
	{
		for (int32 X = X0; X <= X1; ++X)
		{
			const int32 I = Y * Nx + X;
			Inside(I, Origin + FVector2D((X + 0.5f) * Cell, (Y + 0.5f) * Cell));
		}
	}
}

void FDCNavGrid::BlockQuad(const FVector2D Q[4])
{
	FBox2D Box(ForceInit);
	for (int32 i = 0; i < 4; ++i) { Box += Q[i]; }
	Stamp2D(Box.ExpandBy(Radius + Cell), [&](int32 I, const FVector2D& P)
	{
		for (int32 e = 0; e < 4; ++e)
		{
			const FVector2D D = (Q[(e + 1) & 3] - Q[e]).GetSafeNormal();
			if (FVector2D::CrossProduct(D, P - Q[e]) < -Radius) { return; }
		}
		Clear[I] = 0;
	});
}

void FDCNavGrid::BlockWall(const FVector2D& A, const FVector2D& B, float T0, float T1, float In, float Out)
{
	if (T1 <= T0) { return; }
	const FVector2D D = (B - A).GetSafeNormal();
	const FVector2D N(-D.Y, D.X);
	FBox2D Box(ForceInit);
	Box += A + D * T0; Box += A + D * T1;
	Stamp2D(Box.ExpandBy(FMath::Max(In, Out) + Radius + Cell), [&](int32 I, const FVector2D& P)
	{
		const FVector2D R = P - A;
		const float U = FVector2D::DotProduct(R, D), V = FVector2D::DotProduct(R, N);
		if (U >= T0 - Radius && U <= T1 + Radius && V <= In + Radius && V >= -Out - Radius) { Clear[I] = 0; }
	});
}

void FDCNavGrid::BlockBox(const FVector2D& Centre, const FVector2D& Half, float YawRad, float Pad)
{
	const FVector2D X(FMath::Cos(YawRad), FMath::Sin(YawRad)), Y(-X.Y, X.X);
	const float Reach = Half.Size() + Pad + Cell;
	Stamp2D(FBox2D(Centre - FVector2D(Reach, Reach), Centre + FVector2D(Reach, Reach)), [&](int32 I, const FVector2D& P)
	{
		const FVector2D R = P - Centre;
		if (FMath::Abs(FVector2D::DotProduct(R, X)) <= Half.X + Pad && FMath::Abs(FVector2D::DotProduct(R, Y)) <= Half.Y + Pad) { Clear[I] = 0; }
	});
}

void FDCNavGrid::BlockDisc(const FVector2D& Centre, float R)
{
	Stamp2D(FBox2D(Centre - FVector2D(R + Cell, R + Cell), Centre + FVector2D(R + Cell, R + Cell)), [&](int32 I, const FVector2D& P)
	{
		if (FVector2D::DistSquared(P, Centre) <= R * R) { Clear[I] = 0; }
	});
}

void FDCNavGrid::SetRegion(const FVector2D Q[4], int32 InRegion)
{
	FBox2D Box(ForceInit);
	for (int32 i = 0; i < 4; ++i) { Box += Q[i]; }
	Stamp2D(Box, [&](int32 I, const FVector2D& P)
	{
		for (int32 e = 0; e < 4; ++e)
		{
			const FVector2D D = Q[(e + 1) & 3] - Q[e];
			if (FVector2D::CrossProduct(D, P - Q[e]) < 0.f) { return; }
		}
		Region[I] = (int8)FMath::Clamp(InRegion, -1, 126);
	});
}

void FDCNavGrid::Finish()
{
	const int32 N = Nx * Ny;
	// Clearance: 1 = touches a blocked tile, 2 = touches a 1, else 3.
	for (uint8 Ring = 1; Ring <= 2; ++Ring)
	{
		for (int32 Y = 0; Y < Ny; ++Y)
		{
			for (int32 X = 0; X < Nx; ++X)
			{
				const int32 I = Y * Nx + X;
				if (Clear[I] != 3) { continue; }
				bool bNear = false;
				for (int32 dy = -1; dy <= 1 && !bNear; ++dy)
				{
					for (int32 dx = -1; dx <= 1; ++dx)
					{
						const int32 Xn = X + dx, Yn = Y + dy;
						if (Xn < 0 || Yn < 0 || Xn >= Nx || Yn >= Ny) { continue; }
						if (Clear[Yn * Nx + Xn] == Ring - 1) { bNear = true; break; }
					}
				}
				if (bNear) { Clear[I] = Ring | 0x80; } // marked, so this pass doesn't chain off its own writes
			}
		}
		for (int32 I = 0; I < N; ++I) { Clear[I] &= 0x7F; }
	}

	// Connected walkable areas (4-neighbour flood fill).
	Component.Init(0, N);
	ComponentCount = 0;
	FreeCount = 0;
	TArray<int32> Stack;
	for (int32 Seed = 0; Seed < N; ++Seed)
	{
		if (Clear[Seed] == 0) { continue; }
		++FreeCount;
		if (Component[Seed] != 0) { continue; }
		const uint16 Id = (uint16)FMath::Min(++ComponentCount, 65535);
		Component[Seed] = Id;
		Stack.Reset();
		Stack.Add(Seed);
		while (Stack.Num())
		{
			const int32 I = Stack.Pop(EAllowShrinking::No);
			const int32 X = I % Nx, Y = I / Nx;
			const int32 Nb[4] = { X > 0 ? I - 1 : -1, X < Nx - 1 ? I + 1 : -1, Y > 0 ? I - Nx : -1, Y < Ny - 1 ? I + Nx : -1 };
			for (int32 J : Nb)
			{
				if (J >= 0 && Clear[J] != 0 && Component[J] == 0) { Component[J] = Id; Stack.Add(J); }
			}
		}
	}

	GCost.SetNumUninitialized(N);
	Parent.SetNumUninitialized(N);
	Stamp.Init(0, N);
	Generation = 0;
	bBuilt = true;
}

int32 FDCNavGrid::CellIndexClamped(const FVector2D& P) const
{
	const int32 X = FMath::Clamp(FMath::FloorToInt((P.X - Origin.X) / Cell), 0, Nx - 1);
	const int32 Y = FMath::Clamp(FMath::FloorToInt((P.Y - Origin.Y) / Cell), 0, Ny - 1);
	return Y * Nx + X;
}

bool FDCNavGrid::IsFree(const FVector2D& P) const
{
	if (!bBuilt || !Contains(P)) { return true; }
	return Clear[CellIndexClamped(P)] != 0;
}

int32 FDCNavGrid::SnapFree(const FVector2D& P) const
{
	if (!bBuilt) { return INDEX_NONE; }
	const int32 I0 = CellIndexClamped(P);
	if (Clear[I0] != 0) { return I0; }
	const int32 X0 = I0 % Nx, Y0 = I0 / Nx;
	const int32 Want = Region[I0];
	int32 Best = INDEX_NONE;
	float BestScore = 1e12f;
	for (int32 R = 1; R <= SnapRings; ++R)
	{
		for (int32 Y = Y0 - R; Y <= Y0 + R; ++Y)
		{
			if (Y < 0 || Y >= Ny) { continue; }
			const bool bEdgeRow = Y == Y0 - R || Y == Y0 + R;
			for (int32 X = X0 - R; X <= X0 + R; X += (bEdgeRow ? 1 : 2 * R))
			{
				if (X < 0 || X >= Nx) { continue; }
				const int32 I = Y * Nx + X;
				if (Clear[I] == 0) { continue; }
				// the far side of a wall is a last resort
				const float Score = FVector2D::Distance(CellCentre(I), P) + (Region[I] == Want ? 0.f : 10000.f);
				if (Score < BestScore) { BestScore = Score; Best = I; }
			}
		}
		// a ring further out can't beat a same-region hit by more than the ring spacing
		if (Best != INDEX_NONE && BestScore < 10000.f && BestScore <= R * Cell) { break; }
	}
	return Best;
}

bool FDCNavGrid::IsReachable(const FVector2D& A, const FVector2D& B) const
{
	const int32 Ia = SnapFree(A), Ib = SnapFree(B);
	return Ia != INDEX_NONE && Ib != INDEX_NONE && Component[Ia] == Component[Ib];
}

bool FDCNavGrid::Line(const FVector2D& A, const FVector2D& B, uint8 MinClear, int32 SkipA, int32 SkipB) const
{
	const float Len = FVector2D::Distance(A, B);
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(Len / (Cell * 0.4f)));
	for (int32 s = 0; s <= Steps; ++s)
	{
		const FVector2D P = FMath::Lerp(A, B, (float)s / Steps);
		if (!Contains(P)) { continue; }
		const int32 I = CellIndexClamped(P);
		if (I == SkipA || I == SkipB) { continue; }
		if (Clear[I] < MinClear) { return false; }
	}
	return true;
}

bool FDCNavGrid::SegmentClear(const FVector2D& A, const FVector2D& B, uint8 MinClear) const
{
	if (!bBuilt) { return true; }
	return Line(A, B, MinClear, INDEX_NONE, INDEX_NONE);
}

bool FDCNavGrid::FindPath(const FVector2D& From, const FVector2D& To, TArray<FVector2D>& Out) const
{
	LastExpanded = 0;
	if (!bBuilt) { return false; }
	const int32 S = SnapFree(From);
	int32 G = SnapFree(To);
	if (S == INDEX_NONE || G == INDEX_NONE) { return false; }

	if (Component[S] != Component[G])
	{
		// Cut off (a wing whose only neighbour is still a building site): get as close as the floor allows.
		const FVector2D Gc = CellCentre(G);
		const uint16 Mine = Component[S];
		float Best = 1e18f;
		const int32 N = Nx * Ny;
		for (int32 I = 0; I < N; ++I)
		{
			if (Component[I] != Mine) { continue; }
			const float D = FVector2D::DistSquared(CellCentre(I), Gc);
			if (D < Best) { Best = D; G = I; }
		}
	}

	// ── A* ──
	TArray<int32> Chain;
	if (S != G)
	{
		if (++Generation == 0) { Stamp.Init(0, Nx * Ny); Generation = 1; }
		const int32 Gx = G % Nx, Gy = G / Nx;
		auto Heuristic = [&](int32 I)
		{
			const int32 Dx = FMath::Abs(I % Nx - Gx), Dy = FMath::Abs(I / Nx - Gy);
			return (float)(FMath::Max(Dx, Dy) + (Diag - 1.f) * FMath::Min(Dx, Dy));
		};
		TArray<FOpen> Open;
		Open.Reserve(1024);
		GCost[S] = 0.f; Parent[S] = INDEX_NONE; Stamp[S] = Generation;
		Open.HeapPush(FOpen{ Heuristic(S), S }, FOpenLess());
		bool bFound = false;
		while (Open.Num())
		{
			FOpen Top;
			Open.HeapPop(Top, FOpenLess(), EAllowShrinking::No);
			const int32 I = Top.I;
			if (Top.F > GCost[I] + Heuristic(I) + 1e-3f) { continue; } // stale entry
			if (I == G) { bFound = true; break; }
			++LastExpanded;
			const int32 X = I % Nx, Y = I / Nx;
			for (int32 dy = -1; dy <= 1; ++dy)
			{
				for (int32 dx = -1; dx <= 1; ++dx)
				{
					if (!dx && !dy) { continue; }
					const int32 Xn = X + dx, Yn = Y + dy;
					if (Xn < 0 || Yn < 0 || Xn >= Nx || Yn >= Ny) { continue; }
					const int32 J = Yn * Nx + Xn;
					if (Clear[J] == 0) { continue; }
					// no squeezing diagonally past a corner
					if (dx && dy && (Clear[Y * Nx + Xn] == 0 || Clear[Yn * Nx + X] == 0)) { continue; }
					const float Cost = GCost[I] + ((dx && dy) ? Diag : 1.f) * ClearCost[Clear[J]];
					if (Stamp[J] == Generation && Cost >= GCost[J]) { continue; }
					Stamp[J] = Generation; GCost[J] = Cost; Parent[J] = I;
					Open.HeapPush(FOpen{ Cost + Heuristic(J), J }, FOpenLess());
				}
			}
		}
		if (!bFound) { return false; }
		for (int32 I = G; I != INDEX_NONE; I = Parent[I]) { Chain.Add(I); }
		Algo::Reverse(Chain);
	}
	else { Chain.Add(S); }

	// ── Pull the tile chain tight ──
	TArray<FVector2D> Pts;
	TArray<int32> Tiles;
	Pts.Reserve(Chain.Num() + 2);
	Pts.Add(From); Tiles.Add(CellIndexClamped(From));
	for (int32 k = 0; k < Chain.Num(); ++k)
	{
		if (k == 0 && Chain[0] == Tiles[0]) { continue; } // already standing on it
		Pts.Add(CellCentre(Chain[k])); Tiles.Add(Chain[k]);
	}
	if (Tiles.Last() == CellIndexClamped(To) && Pts.Num() > 1) { Pts.Last() = To; }
	else { Pts.Add(To); Tiles.Add(CellIndexClamped(To)); }

	const int32 N = Pts.Num();
	int32 A = 0;
	while (A < N - 1)
	{
		int32 J = A + 1;
		// The leg may run as tight to obstacles as its own ends already are, but no tighter: straight legs through
		// open floor stay off the walls, and only the first / last step to a spot at the furniture hugs it.
		while (J + 1 < N)
		{
			const uint8 Need = (uint8)FMath::Clamp<int32>(FMath::Min<int32>(Clear[Tiles[A]], Clear[Tiles[J + 1]]), 1, 2);
			if (!Line(Pts[A], Pts[J + 1], Need, Tiles[A], Tiles[J + 1])) { break; }
			++J;
		}
		Out.Add(Pts[J]);
		A = J;
	}
	return true;
}
