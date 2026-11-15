// Director's Cut — walkable tile map tests: DirectorsCut.World.NavGrid (routes use doorways and the front door, never
// cross a wall, cope with blocked start / goal points and cut-off rooms, and stay cheap).
#include "Misc/AutomationTest.h"
#include "World/DCNavGrid.h"
#include "Core/DCSiteLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using namespace DCSite;

	/** Stamp the walls of the given cells the way ADCWorldDirector::BuildNav does. */
	void BuildHouse(FDCNavGrid& Nav, const TArray<FIntPoint>& Cells)
	{
		Nav.Begin(OutdoorWalkRect().ExpandBy(60.f));
		const float HalfT = Grid::WallT * 0.5f;
		for (int32 i = 0; i < Cells.Num(); ++i)
		{
			FVector2D Q[4]; Grid::Corners(Cells[i], Q);
			Nav.SetRegion(Q, i);
			for (int32 E = 0; E < 4; ++E)
			{
				const FVector2D A = Q[E], B = Q[(E + 1) & 3];
				const float L = FVector2D::Distance(A, B);
				const bool bShared = Cells.Contains(Grid::Neighbour(Cells[i], E));
				const bool bFront = !bShared && E == 0 && Cells[i] == Grid::OfficeCell();
				const float DoorW = bShared ? Grid::DoorW : (bFront ? FrontDoorW : 0.f);
				const float In = bShared ? HalfT : Grid::WallT, Out = bShared ? HalfT : 4.f;
				if (DoorW <= 0.f) { Nav.BlockWall(A, B, -HalfT, L + HalfT, In, Out); continue; }
				Nav.BlockWall(A, B, -HalfT, L * 0.5f - DoorW * 0.5f, In, Out);
				Nav.BlockWall(A, B, L * 0.5f + DoorW * 0.5f, L + HalfT, In, Out);
			}
		}
	}

	/** Where the polyline crosses edge A -> B, as a distance from the edge midpoint (cm). False if it never does. */
	bool CrossingOffset(const TArray<FVector2D>& Pts, const FVector2D& A, const FVector2D& B, float& OutOffset)
	{
		const FVector2D D = B - A;
		for (int32 i = 0; i + 1 < Pts.Num(); ++i)
		{
			const FVector2D P = Pts[i], R = Pts[i + 1] - Pts[i];
			const float Den = FVector2D::CrossProduct(R, D);
			if (FMath::Abs(Den) < 1e-4f) { continue; }
			const float T = FVector2D::CrossProduct(A - P, D) / Den, U = FVector2D::CrossProduct(A - P, R) / Den;
			if (T >= 0.f && T <= 1.f && U >= 0.f && U <= 1.f) { OutOffset = (U - 0.5f) * D.Size(); return true; }
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCNavGridTest, "DirectorsCut.World.NavGrid", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDCNavGridTest::RunTest(const FString&)
{
	// An L-shaped house: the Office, a room to its right, and one behind that.
	const TArray<FIntPoint> Cells = { FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(1, 1) };
	FDCNavGrid Nav;
	BuildHouse(Nav, Cells);
	// a table in the middle of the back room
	const FVector2D BackCentre = Grid::Centre(Cells[2]);
	Nav.BlockBox(BackCentre, FVector2D(90.f, 50.f), 0.4f, 14.f);
	Nav.Finish();

	const FVector2D Office = Grid::Centre(Cells[0]), Right = Grid::Centre(Cells[1]);
	const FVector2D Yard = YardRect().GetCenter() + FVector2D(0.f, 300.f);
	const FVector2D BackCorner = BackCentre + FVector2D(170.f, 170.f);

	TestTrue(TEXT("room centres are free"), Nav.IsFree(Office) && Nav.IsFree(Right));
	TestFalse(TEXT("the table is blocked"), Nav.IsFree(BackCentre));
	TestEqual(TEXT("house + outdoors are one connected area"), Nav.NumComponents(), 1);

	FVector2D Q0[4], Q1[4];
	Grid::Corners(Cells[0], Q0); Grid::Corners(Cells[1], Q1);
	// walls are solid away from the doorway, open in it
	TestFalse(TEXT("front wall is solid beside the door"), Nav.IsFree(FMath::Lerp(Q0[0], Q0[1], 0.15f)));
	TestTrue(TEXT("front door is open"), Nav.IsFree(FrontDoor()));
	TestFalse(TEXT("side wall of the house is solid"), Nav.IsFree((Q0[3] + Q0[0]) * 0.5f));
	TestTrue(TEXT("doorway between rooms is open"), Nav.IsFree((Q0[1] + Q0[2]) * 0.5f));

	// Back room -> yard: through both doorways and out of the front door, every leg on free tiles.
	TArray<FVector2D> Route;
	TestTrue(TEXT("route found"), Nav.FindPath(BackCorner, Yard, Route));
	Route.Insert(BackCorner, 0);
	bool bLegsClear = true;
	for (int32 i = 0; i + 1 < Route.Num(); ++i) { bLegsClear &= Nav.SegmentClear(Route[i], Route[i + 1]); }
	TestTrue(TEXT("no leg crosses a wall or the table"), bLegsClear);
	TestTrue(TEXT("route ends at the goal"), Route.Last().Equals(Yard, 0.01f));
	TestTrue(TEXT("route is a handful of legs"), Route.Num() <= 14);
	float Off = 0.f;
	TestTrue(TEXT("leaves by the front wall"), CrossingOffset(Route, Q0[0], Q0[1], Off));
	TestTrue(TEXT("... through the front door"), FMath::Abs(Off) < FrontDoorW * 0.5f);
	TestTrue(TEXT("crosses into the Office"), CrossingOffset(Route, Q0[1], Q0[2], Off));
	TestTrue(TEXT("... through the doorway"), FMath::Abs(Off) < Grid::DoorW * 0.5f);
	TestTrue(TEXT("crosses out of the back room"), CrossingOffset(Route, Q1[2], Q1[3], Off));
	TestTrue(TEXT("... through the doorway"), FMath::Abs(Off) < Grid::DoorW * 0.5f);
	TestFalse(TEXT("never over the Office's side wall"), CrossingOffset(Route, Q0[3], Q0[0], Off));

	// A goal on a blocked tile (a spot at the table): the route still ends exactly there.
	Route.Reset();
	const FVector2D AtTable = BackCentre + FVector2D(0.f, 40.f);
	TestTrue(TEXT("route to a blocked spot"), Nav.FindPath(Office, AtTable, Route));
	TestTrue(TEXT("ends on the spot"), Route.Num() > 0 && Route.Last().Equals(AtTable, 0.01f));

	// A wing with no finished neighbour is cut off: reported unreachable, and a route still gets as close as it can.
	FDCNavGrid Split;
	BuildHouse(Split, { FIntPoint(0, 0), FIntPoint(-2, 1) });
	Split.Finish();
	const FVector2D Island = Grid::Centre(FIntPoint(-2, 1));
	TestFalse(TEXT("cut-off room is unreachable"), Split.IsReachable(Office, Island));
	TestTrue(TEXT("outdoors is reachable"), Split.IsReachable(Office, Yard));
	Route.Reset();
	TestTrue(TEXT("best-effort route"), Split.FindPath(Office, Island, Route));

	// Cost: the longest trips on the map, both ways.
	const double T0 = FPlatformTime::Seconds();
	int32 Expanded = 0;
	for (int32 i = 0; i < 200; ++i)
	{
		Route.Reset();
		Nav.FindPath((i & 1) ? BackCorner : Yard, (i & 1) ? Yard : BackCorner, Route);
		Expanded += Nav.LastExpanded;
	}
	const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0 / 200.0;
	AddInfo(FString::Printf(TEXT("FindPath: %.3f ms per route, %d tiles expanded on average, map %dx%d"), Ms, Expanded / 200, Nav.SizeX(), Nav.SizeY()));
	TestTrue(TEXT("a route costs well under a millisecond or two"), Ms < 5.0);
	return true;
}

#endif
