// Director's Cut — the Office's furnishing recipe (see DCHideoutFurnish.cpp): the mission board with the planning
// table in front of it, the Director's desk, a stove, somewhere to sit, storage, clutter and things on the walls.
#include "World/Scene/DCHideoutFurnish.h"
#include "Components/PointLightComponent.h"

using namespace DCFurnish;

void ADCHideoutBuilder::FurnishOffice(DCFurnish::FCtx& Ctx)
{
	DCFurnish::FPlan& Plan = Ctx.Plan;
	TArray<DCFurnish::FPlaced>& Out = Ctx.Out;
	TArray<ADCRoomModule::FUseSpot>& Spots = Ctx.Spots;
	TArray<FIntPoint>& Hit = Ctx.Hit;
	const TArray<DCFurnish::FFrame>& WallF = Ctx.WallF;
	const TArray<DCFurnish::FFrame>& WallPartF = Ctx.WallPartF;
	const TArray<DCFurnish::FFrame>& FreeF = Ctx.FreeF;
	const int32 Seed = Ctx.Seed;
	const bool bDebug = Ctx.bDebug;
	const float H = Ctx.H;
	const float BoardTop = FMath::Min(206.f, Ctx.LampZ - 18.f);
	// how the desk went in (its drawing needs to know)
	bool bDeskToWall = false;
	float DeskW = 152.f, DeskD = 76.f;

	auto Jit = [&](int32 K) { return Ctx.Jit(K); };
	auto FacingHub = [&](const DCFurnish::FFrame& F) { return Ctx.FacingHub(F); };
	auto FacingCam = [&](const DCFurnish::FFrame& F) { return Ctx.FacingCam(F); };
	auto Spot = [&](const DCFurnish::FFrame& F, float X, float Y) { Ctx.Spot(F, X, Y); };

	enum { PBoard, PEasel, PTable, PChair, PRug, PStove, PLogs, PDesk, PSofa, PCoffee, PArmchair, PLamp, PBookcase, PSideboard, PFiling, PSafe, PScreen };
	static const TCHAR* Names[] = { TEXT("board"), TEXT("easel"), TEXT("table"), TEXT("chair"), TEXT("rug"), TEXT("stove"), TEXT("logs"), TEXT("desk"), TEXT("sofa"), TEXT("coffee"),
		TEXT("armchair"), TEXT("lamp"), TEXT("bookcase"), TEXT("sideboard"), TEXT("filing"), TEXT("safe"), TEXT("screen") };
	// How each kind is drawn once the layout has settled (A, B: sizes passed at placement).
	auto DrawFor = [&](int32 Kind, float A, float B, bool bBlue) -> DCFurnish::FDrawFn
	{
		const FLinearColor Warm(1.f, 0.62f, 0.32f);
		switch (Kind)
		{
		case PBoard: return [A, BoardTop](const FPen& P, int32 S) { DrawBoard(P, A, BoardTop, S); };
		case PEasel: return [A](const FPen& P, int32 S) { DrawEaselBoard(P, A, S); };
		case PTable: return [A, B](const FPen& P, int32 S) { DrawPlanningTable(P, A, B, S); };
		case PChair: return [](const FPen& P, int32 S) { DrawWoodChair(P, S); };
		case PRug: return [A, B, bBlue](const FPen& P, int32) { DrawRug(P, A, B, bBlue ? FName("RugBlue") : FName("Rug"), "RugBorder"); };
		case PStove: return [this, H](const FPen& P, int32 S)
		{
			DrawStove(P, FMath::Min(H - 50.f, 250.f), S);
			UPointLightComponent* L = Point(P.P(68.f, 0.f, 40.f), FLinearColor(1.f, 0.38f, 0.1f), 16.f, 380.f, false, true, 8.f);
			AddFlicker(L, 7.f, 0.35f);
		};
		case PLogs: return [](const FPen& P, int32 S) { DrawLogBasket(P, S); };
		case PDesk: return [this, bDeskToWall, DeskW, DeskD, Warm](const FPen& P, int32 S)
		{
			const float DX = bDeskToWall ? DeskD * 0.5f + 2.f : 80.f + DeskD * 0.5f + 1.f, CX = bDeskToWall ? DeskD + 36.f : 50.f, Turn = bDeskToWall ? 180.f : 0.f;
			const DCFurnish::FPen DP = P.At(DX, 0.f, Turn);
			DrawCaptainChair(P.At(CX, J(S, 1) * 6.f, Turn + J(S, 2) * 18.f));
			DrawDesk(DP, DeskW, DeskD, S);
			Point(DP.P(DeskD * 0.5f * 0.37f, DeskW * 0.5f - 22.f, 100.f), Warm, 8.f, 300.f, false, true, 3.f);
		};
		case PSofa: return [A](const FPen& P, int32 S) { DrawSofa(P, A, U(S, 3) > 0.5f ? FName("LeatherBrn") : FName("Velvet"), S); };
		case PCoffee: return [](const FPen& P, int32 S) { DrawCoffeeTable(P, 100.f, 56.f, S); };
		case PArmchair: return [](const FPen& P, int32 S) { DrawArmchair(P, U(S, 4) > 0.5f ? FName("Velvet") : FName("LeatherBrn"), S); };
		case PLamp: return [this, Warm](const FPen& P, int32)
		{
			DrawFloorLamp(P);
			Point(P.P(0.f, 0.f, 150.f), Warm, 12.f, 400.f, false, true, 10.f);
		};
		case PBookcase: return [A](const FPen& P, int32 S) { DrawBookcase(P, A, S); };
		case PSideboard: return [A](const FPen& P, int32 S) { DrawSideboard(P, A, S); };
		case PFiling: return [](const FPen& P, int32 S) { DrawFilingCabinets(P, 2, S); };
		case PSafe: return [](const FPen& P, int32) { DrawSafe(P); };
		case PScreen: return [](const FPen& P, int32) { DrawWallScreen(P); };
		default: return DCFurnish::FDrawFn();
		}
	};
	// Group "Blue": a blue rug (not a visibility group).
	auto Place = [&](int32 Kind, const TArray<DCFurnish::FLocal>& Ls, const DCFurnish::FFrame& F, float A = 0.f, float B = 0.f, FName Group = NAME_None)
	{
		const bool bBlue = Group == FName("Blue");
		Ctx.Place(Names[Kind], Ls, F, DrawFor(Kind, A, B, bBlue), bBlue ? FName(NAME_None) : Group, Kind == PRug);
	};
	auto PlaceSoft = [&](int32 Kind, const TArray<DCFurnish::FLocal>& Ls, const DCFurnish::FFrame& F, float A = 0.f, float B = 0.f, FName Group = NAME_None)
	{
		Ctx.PlaceSoft(Names[Kind], Ls, F, DrawFor(Kind, A, B, false), Group);
	};
	auto BestSoft = [&](const TArray<DCFurnish::FLocal>& Ls, const TArray<DCFurnish::FFrame>& Frames, TFunctionRef<float(const DCFurnish::FFrame&)> Score, float Pad, float Penalty)
	{
		return Ctx.BestSoft(Ls, Frames, Score, Pad, Penalty);
	};

	// ---------------------------------------------------------------- 1. the mission board (the room's station)
	// Tall wall furniture may stand against a window (the wall is then built without it) at a price, as long as the
	// room keeps a window.
	DCFurnish::FFrame Board;
	float BoardW = 0.f;
	{
		float BestS = -1e8f;
		for (float W : { 230.f, 200.f, 170.f, 145.f })
		{
			TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Space(5.f, 0.f, 5.f, W * 0.5f + 6.f, BoardTop + 4.f) };
			Ls[0].Z0 = BoardTop - 125.f; // the board only claims the wall above the floor
			const TArray<DCFurnish::FLocal> Probe = { DCFurnish::FLocal::Solid(8.f + 95.f + 44.f, 0.f, 44.f, 75.f, 82.f) }; // room for the table
			auto Score = [&](const DCFurnish::FFrame& C)
			{
				const DCFurnish::FEdge& Ed = Plan.E[C.Edge];
				return (float)(W * 0.5f - FMath::Abs(C.T - Ed.L * 0.5f) * 0.12f + Ed.L * 0.02f + Jit(C.Edge) * 3.f + (Plan.Fits(Probe, C, 4.f) ? 0.f : -60.f));
			};
			const DCFurnish::FFrame F = BestSoft(Ls, WallF, Score, 4.f, 55.f);
			if (!F.bValid) { continue; }
			Plan.WindowsHit(Ls, F, Hit);
			const float S = Score(F) - 55.f * Hit.Num();
			if (S > BestS) { BestS = S; Board = F; BoardW = W; }
		}
		if (Board.bValid)
		{
			TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Space(5.f, 0.f, 5.f, BoardW * 0.5f + 6.f, BoardTop + 4.f) };
			Ls[0].Z0 = BoardTop - 125.f;
			PlaceSoft(PBoard, Ls, Board, BoardW);
		}
		else
		{
			// no tall wall: a low board on legs, backed onto a partition if possible, else standing at the back of the
			// room facing the camera
			for (float W : { 170.f, 140.f, 115.f })
			{
				const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(27.f, 0.f, 27.f, W * 0.5f + 4.f, 146.f) };
				Board = Plan.Best(Ls, WallPartF, [&](const DCFurnish::FFrame& F) { return (float)(-FMath::Abs(F.T - Plan.E[F.Edge].L * 0.5f) * 0.05f + FacingCam(F) * 10.f); }, 4.f);
				if (!Board.bValid)
				{
					TArray<DCFurnish::FFrame> Fr;
					for (const DCFurnish::FFrame& F0 : FreeF)
					{
						for (float Y : { YawOf(KToCam), Plan.RoomYaw - 90.f, Plan.RoomYaw + 180.f }) { DCFurnish::FFrame F = F0; F.Yaw = Y; Fr.Add(F); }
					}
					Board = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& F)
					{
						if (FV2::DotProduct(F.O - Plan.Hub, KToCam) > 0.f) { return -1e9f; } // the back half only
						return (float)(-FV2::Distance(F.O, Plan.Hub - KToCam * 150.f) * 0.5f + FacingCam(F) * 20.f);
					}, 4.f);
				}
				if (Board.bValid) { BoardW = W; Place(PEasel, Ls, Board, BoardW); Board.Edge = -1; break; }
			}
		}
	}

	// the room light: a bracket lamp over the board, else on a standing back wall, else a bare bulb
	if (Board.bValid && Board.Edge >= 0) { Ctx.SetLamp(Board.Edge, Board.T); }
	else { Ctx.DefaultLamp(); }

	// ---------------------------------------------------------------- 2+3. the planning table and the Director's desk
	// Solved together: the biggest table in front of the board that still leaves the desk a place. Failing that the
	// table stands free, and as a last resort the walking lanes narrow.
	DCFurnish::FFrame Table;
	float TableW = 0.f, TableD = 0.f;
	auto TryTable = [&](const FV2& Sz) -> bool
	{
		if (!Board.bValid) { return false; }
		const float Front = Board.Edge >= 0 ? 8.f : 56.f;
		TArray<DCFurnish::FFrame> Fr;
		for (float Gap = 70.f; Gap <= 130.f; Gap += 10.f)
		{
			for (float Lat = -40.f; Lat <= 40.f; Lat += 10.f)
			{
				DCFurnish::FFrame F = Board; F.O = Board.O + Rot(FV2(Front + Gap + Sz.Y * 0.5f, Lat), Board.Yaw); F.T = Gap; F.Edge = -1;
				Fr.Add(F);
			}
		}
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(0.f, 0.f, Sz.Y * 0.5f, Sz.X * 0.5f, 84.f) };
		Table = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& F)
		{
			const FV2 L = Rot(F.O - Board.O, -Board.Yaw);
			return (float)(-FMath::Abs(F.T - 90.f) * 0.5f - FMath::Abs(L.Y) * 0.3f);
		}, 4.f);
		if (!Table.bValid) { return false; }
		TableW = Sz.X; TableD = Sz.Y;
		const float Gap = Table.T;
		// keep the standing strip between the board and the table clear
		TArray<DCFurnish::FLocal> All = Ls;
		All.Add(DCFurnish::FLocal::Space(-(Gap * 0.5f + Sz.Y * 0.5f), 0.f, Gap * 0.5f - 2.f, FMath::Min(Sz.X, BoardW) * 0.5f, 60.f));
		for (float S : { -1.f, 1.f })
		{
			// hold the end seats (if they're free now) so the desk doesn't take them
			DCFurnish::FLocal Seat = DCFurnish::FLocal::Space(0.f, S * (Sz.X * 0.5f + 30.f), 26.f, 26.f, 60.f);
			Seat.Tag = DCFurnish::TagSeat;
			if (Plan.PartOk(Plan.Realise(Seat, Table), 2.f)) { All.Add(Seat); }
		}
		Place(PTable, All, Table, TableW, TableD);
		Spot(Table, -(Sz.Y * 0.5f + Gap * 0.5f), -Sz.X * 0.18f);
		Spot(Table, -(Sz.Y * 0.5f + Gap * 0.5f), Sz.X * 0.22f);
		return true;
	};
	auto TryFreeTable = [&]() -> bool
	{
		for (const FV2& Sz : { FV2(150.f, 86.f), FV2(124.f, 80.f) })
		{
			const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(0.f, 0.f, Sz.Y * 0.5f, Sz.X * 0.5f, 84.f) };
			TArray<DCFurnish::FFrame> Fr = FreeF;
			for (const DCFurnish::FFrame& F : FreeF) { DCFurnish::FFrame G = F; G.Yaw += 90.f; Fr.Add(G); }
			const FV2 Want = Board.bValid ? Board.O + Dir(Board.Yaw) * 170.f : Plan.Hub - KToCam * 110.f;
			Table = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& F)
			{
				if (Board.bValid && FV2::DotProduct(F.O - Board.O, Dir(Board.Yaw)) < -20.f) { return -1e9f; } // never behind the board
				return (float)(-FV2::Distance(F.O, Want) * 0.4f + FV2::DotProduct(F.O - Plan.Hub, -KToCam) * 0.1f);
			}, 6.f);
			if (Table.bValid) { TableW = Sz.X; TableD = Sz.Y; Place(PTable, Ls, Table, TableW, TableD); Spot(Table, -(TableD * 0.5f + 34.f), 0.f); return true; }
		}
		return false;
	};

	// The desk faces the room with the chair against the wall; failing that it is pushed against the wall with
	// the chair out front.
	DCFurnish::FFrame Desk;
	auto TryDesk = [&]() -> bool
	{
		auto Score = [&](const DCFurnish::FFrame& F)
		{
			// leave the back corner for the stove; the desk reads best further forward on a side wall
			return (float)(FacingHub(F) * 12.f + FacingCam(F) * 8.f + FMath::Min(FV2::Distance(F.O, Plan.In[2]), 300.f) * 0.06f + (F.Edge == Board.Edge ? -40.f : 0.f)
				+ (Plan.E[F.Edge].Kind == DCFurnish::EKind::Wall ? 6.f : 0.f) + Jit(30 + F.Edge) * 5.f);
		};
		TArray<DCFurnish::FFrame> Fr;
		for (const DCFurnish::FFrame& F0 : FreeF)
		{
			for (float Y : { YawOf(KToCam), Plan.RoomYaw - 90.f, Plan.RoomYaw + 180.f }) { DCFurnish::FFrame F = F0; F.Yaw = Y; Fr.Add(F); }
		}
		for (const FV2& Sz : { FV2(152.f, 76.f), FV2(112.f, 62.f) })
		{
			DeskW = Sz.X; DeskD = Sz.Y;
			const float HW = Sz.X * 0.5f + 1.f, HD = Sz.Y * 0.5f + 1.f;
			// facing the room, chair against the wall
			const float DX = 80.f + HD;
			const TArray<DCFurnish::FLocal> Ls = {
				DCFurnish::FLocal::Space(50.f, 0.f, 28.f, 38.f, 84.f),
				DCFurnish::FLocal::Solid(DX, 0.f, HD, HW, 82.f),
				DCFurnish::FLocal::Space(DX + HD + 16.f, 0.f, 16.f, 38.f, 50.f) };
			Desk = Plan.Best(Ls, WallPartF, Score, 6.f, TEXT("desk"));
			if (Desk.bValid) { bDeskToWall = false; Place(PDesk, Ls, Desk); Spot(Desk, DX + HD + 20.f, 0.f); return true; }
			// pushed against the wall, chair out front
			const TArray<DCFurnish::FLocal> L2 = {
				DCFurnish::FLocal::Solid(HD + 2.f, 0.f, HD, HW, 84.f),
				DCFurnish::FLocal::Space(Sz.Y + 36.f, 0.f, 30.f, 38.f, 84.f) };
			Desk = Plan.Best(L2, WallPartF, Score, 6.f);
			if (Desk.bValid) { bDeskToWall = true; Place(PDesk, L2, Desk); Spot(Desk, Sz.Y + 36.f, 0.f); return true; }
			// a room of doorways: the desk stands free, facing the camera, towards the back
			Desk = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& F) { return (float)(FacingCam(F) * 10.f - FV2::Distance(F.O + Dir(F.Yaw) * DX, Plan.Hub - KToCam * 70.f) * 0.1f); }, 6.f, TEXT("desk free"));
			if (Desk.bValid) { bDeskToWall = false; Place(PDesk, Ls, Desk); Spot(Desk, DX + HD + 20.f, 0.f); return true; }
		}
		return false;
	};

	{
		const int32 P0 = Plan.Parts.Num(), O0 = Out.Num(), S0 = Spots.Num();
		auto Rewind = [&]() { Plan.Parts.SetNum(P0); Out.SetNum(O0); Spots.SetNum(S0); Table = DCFurnish::FFrame(); Desk = DCFurnish::FFrame(); };
		bool bDone = false;
		for (int32 Relax = 0; Relax < 2 && !bDone; ++Relax)
		{
			Plan.CapScale = Relax ? 0.7f : 1.f;
			Plan.bHubKeep = Relax == 0;
			for (const FV2& Sz : { FV2(170.f, 90.f), FV2(150.f, 86.f), FV2(128.f, 80.f), FV2(0.f, 0.f) })
			{
				Rewind();
				const bool bT = Sz.X > 0.f ? TryTable(Sz) : TryFreeTable();
				if (!bT) { continue; }
				if (TryDesk()) { bDone = true; break; }
			}
		}
		if (!bDone)
		{
			// the desk first, then the best table we can still get
			Rewind();
			Plan.CapScale = 0.7f; Plan.bHubKeep = false;
			TryDesk();
			Plan.CapScale = 1.f; Plan.bHubKeep = true;
			if (!TryTable(FV2(170.f, 90.f)) && !TryTable(FV2(128.f, 80.f))) { TryFreeTable(); }
		}
		Plan.CapScale = 1.f; Plan.bHubKeep = true;
	}
	if (Table.bValid)
	{
		// mismatched chairs: the two ends first (side-on to the camera reads best), then the far side
		struct FSeat { float X, Y, Yaw; };
		const TArray<FSeat> Seats = {
			{ 0.f, -(TableW * 0.5f + 24.f), 90.f }, { 0.f, TableW * 0.5f + 24.f, -90.f },
			{ TableD * 0.5f + 24.f, -TableW * 0.22f, 180.f }, { TableD * 0.5f + 24.f, TableW * 0.22f, 180.f } };
		int32 NChairs = 0;
		Plan.CapScale = 0.6f; Plan.bHubKeep = false; // a chair may stand in the way a little
		for (int32 i = 0; i < Seats.Num() && NChairs < 3; ++i)
		{
			const float Pull = 4.f + U(Seed, 600 + i) * 10.f, Twist = J(Seed, 610 + i) * 14.f;
			const FV2 Out2 = FV2(Seats[i].X, Seats[i].Y) + Dir(Seats[i].Yaw + 180.f) * Pull;
			DCFurnish::FFrame F = Table; F.O = Table.O + Rot(Out2, Table.Yaw); F.Yaw = Table.Yaw + Seats[i].Yaw + Twist; F.Edge = -1;
			TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(0.f, 0.f, 24.f, 24.f, 70.f) }; // slatted backs: see-through
			Ls[0].Tag = DCFurnish::TagSeat;
			if (!Plan.Fits(Ls, F, 3.f))
			{
				if (bDebug) { UE_LOG(LogDC, Log, TEXT("[Furnish] chair %d: %s"), i, Plan.WhyNot(Plan.Realise(Ls[0], F), 3.f)); }
				continue;
			}
			Place(PChair, Ls, F, (float)i);
			++NChairs;
		}
		Plan.CapScale = 1.f; Plan.bHubKeep = true;
		Plan.Parts.RemoveAll([](const DCFurnish::FPart& P) { return P.Tag == DCFurnish::TagSeat && !P.bSolid; }); // seats nobody took
		// a rug under the lot
		for (float K : { 1.f, 0.85f, 0.72f })
		{
			const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Rug(0.f, 0.f, (TableD * 0.5f + 70.f) * K, (TableW * 0.5f + 62.f) * K) };
			if (Plan.Fits(Ls, Table, 0.f)) { Place(PRug, Ls, Table, Ls[0].H.X * 2.f, Ls[0].H.Y * 2.f); break; }
		}
	}

	// ---------------------------------------------------------------- 4. the stove in a back corner
	DCFurnish::FFrame Stove;
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(46.f, 0.f, 46.f, 52.f, KTall) };
		Stove = BestSoft(Ls, WallF, [&](const DCFurnish::FFrame& F) { return (float)(-Plan.CornerDist(F.O) * 0.4f + Jit(20 + F.Edge) * 6.f); }, 6.f, 30.f);
		if (!Stove.bValid && bDebug) { Plan.bSoftWindows = true; Plan.Best(Ls, WallF, [&](const DCFurnish::FFrame&) { return 0.f; }, 6.f, TEXT("stove")); Plan.bSoftWindows = false; }
		if (Stove.bValid)
		{
			PlaceSoft(PStove, Ls, Stove);
			Spot(Stove, 125.f, 0.f);
			// logs beside it, on whichever side has room
			for (float S : { 1.f, -1.f })
			{
				DCFurnish::FFrame F = Stove; F.O = Stove.O + Rot(FV2(34.f, S * 82.f), Stove.Yaw); F.Edge = -1;
				const TArray<DCFurnish::FLocal> LL = { DCFurnish::FLocal::Solid(0.f, 0.f, 25.f, 25.f, 50.f) };
				if (Plan.Fits(LL, F, 2.f)) { Place(PLogs, LL, F); break; }
			}
		}
	}

	// ---------------------------------------------------------------- 5. somewhere to sit: a sofa group, else armchairs
	DCFurnish::FFrame Sofa;
	float SofaW = 0.f;
	// the full group (sofa + coffee table) first; a sofa on its own if the table won't fit too
	for (int32 Pass = 0; Pass < 2 && !Sofa.bValid; ++Pass)
	{
		for (float W : { 200.f, 176.f, 150.f })
		{
			TArray<DCFurnish::FLocal> Ls = {
				DCFurnish::FLocal::Solid(46.f, 0.f, 44.f, W * 0.5f, 86.f),
				DCFurnish::FLocal::Space(110.f, 0.f, 20.f, W * 0.5f - 24.f, 40.f) };
			if (Pass == 0) { Ls.Add(DCFurnish::FLocal::Solid(158.f, 0.f, 28.f, 50.f, 46.f)); }
			Sofa = Plan.Best(Ls, WallPartF, [&](const DCFurnish::FFrame& F)
			{
				return FacingHub(F) * 10.f + (F.Edge == Board.Edge ? -30.f : 0.f) + (F.Edge == Desk.Edge ? -12.f : 0.f) + Jit(40 + F.Edge) * 5.f
					- FMath::Abs(F.T - Plan.E[F.Edge].L * 0.5f) * 0.03f;
			}, 6.f, TEXT("sofa"));
			if (!Sofa.bValid) { continue; }
			SofaW = W;
			Place(PSofa, { Ls[0], Ls[1] }, Sofa, W);
			if (Pass == 0)
			{
				DCFurnish::FFrame CT = Sofa; CT.O = Sofa.O + Rot(FV2(158.f, 0.f), Sofa.Yaw); CT.Edge = -1;
				Place(PCoffee, { DCFurnish::FLocal::Solid(0.f, 0.f, 28.f, 50.f, 46.f) }, CT);
			}
			for (float K : { 1.f, 0.85f, 0.7f })
			{
				const float Deep = Pass == 0 ? 92.f : 62.f;
				const TArray<DCFurnish::FLocal> R = { DCFurnish::FLocal::Rug(46.f + Deep, 0.f, Deep * K, (W * 0.5f + 28.f) * K) };
				if (Plan.Fits(R, Sofa, 0.f)) { Place(PRug, R, Sofa, R[0].H.X * 2.f, R[0].H.Y * 2.f, "Blue"); break; }
			}
			break;
		}
	}
	TArray<DCFurnish::FFrame> Seats;
	if (!Sofa.bValid)
	{
		// two armchairs turned towards the stove (or the table), never with their backs to the camera
		const FV2 Focus = Stove.bValid ? Stove.O + Dir(Stove.Yaw) * 90.f : (Table.bValid ? Table.O : Plan.Hub);
		for (int32 n = 0; n < 2; ++n)
		{
			// each spot may turn towards the fire, the room or the camera: whichever reads best
			TArray<DCFurnish::FFrame> Fr;
			for (const DCFurnish::FFrame& F0 : FreeF)
			{
				for (const FV2& T : { Focus, Plan.Hub, F0.O + KToCam * 100.f })
				{
					DCFurnish::FFrame F = F0; F.Yaw = YawOf(T - F0.O) + J(Seed, 640 + n) * 10.f; Fr.Add(F);
				}
			}
			// or backed onto a wall / partition, turned a little towards the room
			for (const DCFurnish::FFrame& W0 : WallPartF)
			{
				DCFurnish::FFrame F = W0; F.O = W0.O + Dir(W0.Yaw) * 46.f; F.Edge = -1;
				const float Turn = FMath::Clamp(FMath::FindDeltaAngleDegrees(W0.Yaw, YawOf(Plan.Hub - F.O)), -25.f, 25.f);
				F.Yaw = W0.Yaw + Turn;
				Fr.Add(F);
			}
			const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(0.f, 0.f, 42.f, 42.f, 86.f) }; // a low club chair
			const DCFurnish::FFrame F = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& C)
			{
				if (FacingCam(C) < -0.3f) { return -1e9f; }
				float WallD = 1e9f;
				for (int32 e = 0; e < 4; ++e) { if (Plan.E[e].bBack) { WallD = FMath::Min(WallD, (float)Plan.E[e].Depth(C.O) - Plan.E[e].Inset); } }
				if (WallD > 130.f) { return -1e9f; } // not adrift in the middle of the floor
				const float FireD = FV2::Distance(C.O, Focus);
				return (float)(-FMath::Max(FireD - 130.f, 0.f) * 0.25f + FV2::DotProduct(Dir(C.Yaw), (Focus - C.O).GetSafeNormal()) * 6.f
					+ FacingHub(C) * 6.f + FacingCam(C) * 5.f - FMath::Min(Plan.NearestSolid(C.O), 150.f) * 0.05f);
			}, 10.f, TEXT("armchair"));
			if (!F.bValid) { break; }
			Place(PArmchair, Ls, F);
			Seats.Add(F);
		}
	}

	// a reading lamp beside the seating
	{
		TArray<DCFurnish::FFrame> Fr;
		if (Sofa.bValid) { for (float S : { 1.f, -1.f }) { DCFurnish::FFrame F = Sofa; F.O = Sofa.O + Rot(FV2(30.f, S * (SofaW * 0.5f + 24.f)), Sofa.Yaw); F.Edge = -1; Fr.Add(F); } }
		for (const DCFurnish::FFrame& A : Seats) { for (float S : { 1.f, -1.f }) { DCFurnish::FFrame F = A; F.O = A.O + Rot(FV2(-22.f, S * 62.f), A.Yaw); Fr.Add(F); } }
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(0.f, 0.f, 20.f, 20.f, 176.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& C) { return -FV2::DotProduct(C.O - Plan.Hub, KToCam) * 0.05f; }, 3.f);
		if (F.bValid) { Place(PLamp, Ls, F); }
	}

	Ctx.Runner();

	// ---------------------------------------------------------------- 6. storage along what's left of the walls
	auto Adjacent = [&](const DCFurnish::FFrame& F) { return -FMath::Min(Plan.NearestSolid(F.O), 200.f) * 0.15f - Plan.CornerDist(F.O) * 0.04f; };
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(17.f, 0.f, 17.f, 49.f, 194.f) };
		const DCFurnish::FFrame F = BestSoft(Ls, WallF, [&](const DCFurnish::FFrame& C) { return (float)(Adjacent(C) + Jit(50 + C.Edge) * 4.f); }, 5.f, 30.f);
		if (F.bValid) { PlaceSoft(PBookcase, Ls, F, 96.f); Spot(F, 72.f, 0.f); }
	}
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(31.f, 0.f, 31.f, 48.f, 136.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, WallPartF, [&](const DCFurnish::FFrame& C) { return (Desk.bValid ? -FV2::Distance(C.O, Desk.O) * 0.08f : 0.f) + Adjacent(C) + Jit(60 + C.Edge) * 4.f; }, 5.f);
		if (F.bValid) { Place(PFiling, Ls, F, 2.f); }
	}
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(24.f, 0.f, 25.f, 62.f, 88.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, WallPartF, [&](const DCFurnish::FFrame& C) { return (Plan.InWindow(C.Edge, C.T, 62.f) ? 14.f : 0.f) + Adjacent(C) * 0.5f + Jit(70 + C.Edge) * 4.f; }, 5.f);
		if (F.bValid) { Place(PSideboard, Ls, F, 124.f); }
	}
	// upgrades bring more: a second bookcase and a safe
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(17.f, 0.f, 17.f, 49.f, 194.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, WallF, [&](const DCFurnish::FFrame& C) { return Adjacent(C) + Jit(80 + C.Edge) * 4.f; }, 5.f);
		if (F.bValid) { Place(PBookcase, Ls, F, 96.f, 0.f, DCGroups::Level2); }
	}
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(31.f, 0.f, 31.f, 31.f, 94.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, WallPartF, [&](const DCFurnish::FFrame& C) { return (Desk.bValid ? -FV2::Distance(C.O, Desk.O) * 0.12f : 0.f) + Adjacent(C) * 0.5f; }, 5.f);
		if (F.bValid) { Place(PSafe, Ls, F, 0.f, 0.f, DCGroups::Level3); }
	}

	Ctx.Clutter();
	Ctx.WallDecor();
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Space(4.f, 0.f, 4.f, 50.f, 198.f) };
		TArray<DCFurnish::FLocal> L2 = Ls; L2[0].Z0 = 138.f;
		const DCFurnish::FFrame F = Plan.Best(L2, WallF, [&](const DCFurnish::FFrame& C) { return Board.bValid ? -FV2::Distance(C.O, Board.O) * 0.1f : 0.f; }, 20.f);
		if (F.bValid) { Place(PScreen, L2, F, 0.f, 0.f, DCGroups::Level2); }
	}
}
