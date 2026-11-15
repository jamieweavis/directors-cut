// Director's Cut — the Server Room's furnishing recipe (see DCHideoutFurnish.cpp): a row of server racks (tall against a
// wall, else half-height against a partition or standing free), the hacker's desk with its monitors, cooling, a UPS,
// spare parts, cable everywhere.
#include "World/Scene/DCHideoutFurnish.h"
#include "Components/PointLightComponent.h"

using namespace DCFurnish;

namespace DCFurnishServer
{
	constexpr float KRackPitch = 80.f;

	/** One rack, H tall: origin on the wall's inner face, doors facing +X. */
	void DrawRack(const FPen& P, float H, int32 Seed)
	{
		static const FName Leds[] = { "LedG", "LedB", "LedG", "LedA", "LedG", "LedB" };
		P.Box(33.f, 0.f, H * 0.5f, 64.f, 76.f, H, "Rack");
		const int32 NU = FMath::Max(1, FMath::FloorToInt((H - 16.f) / 26.f));
		for (int32 u = 0; u < NU; ++u)
		{
			const float Z = 27.f + u * 26.f;
			P.Box(64.5f, 0.f, Z, 1.5f, 66.f, 20.f, "SteelBlack");
			for (int32 l = 0; l < 2; ++l)
			{
				P.Box(65.6f, -24.f + l * 8.f, Z + 4.f, 1.f, 3.f, 3.f, Leds[FMath::Abs(Seed + u * 3 + l) % 6], 0.f, 0.f, 0.f, false);
			}
		}
	}

	/** N racks side by side; tall rows carry a cable tray. */
	void DrawRackRow(const FPen& P, int32 N, float H, int32 Seed)
	{
		for (int32 i = 0; i < N; ++i) { DrawRack(P.At(0.f, (i - (N - 1) * 0.5f) * KRackPitch), H, Seed + i * 5); }
		if (H > 180.f)
		{
			const float W = N * KRackPitch;
			P.Box(30.f, 0.f, H + 9.f, 34.f, W + 16.f, 4.f, "Galv");
			for (int32 i = 0; i < 4; ++i) { P.Cyl(20.f + i * 6.f, 0.f, H + 13.f, 3.f, W + 10.f, (i % 2) ? FName("PaintBlue") : FName("Rubber"), 90.f, 90.f, false); }
			for (float S : { -1.f, 1.f }) { P.Cyl(30.f, S * (W * 0.5f - 30.f), H + 4.f, 6.f, 8.f, "Rubber"); }
		}
	}

	void DrawMonitor(const FPen& P, float X, float Y, float Z, float W, float H, float Yaw, FName Screen)
	{
		const FV2 D = Dir(Yaw);
		P.Cyl(X, Y, Z - H * 0.5f - 5.f, 3.f, 12.f, "PlasticBlack");
		P.Box(X, Y, Z - H * 0.5f - 11.f, 12.f, 18.f, 1.5f, "PlasticBlack");
		P.Box(X, Y, Z, 3.5f, W + 3.f, H + 3.f, "PlasticBlack", Yaw);
		P.Box(X - D.X * 1.9f, Y - D.Y * 1.9f, Z, 0.4f, W, H, Screen, Yaw, 0.f, 0.f, false);
	}

	/** The desk around its own centre; whoever sits at it is on the -X side, the screens face them. */
	void DrawHackDesk(const FPen& P, float W, float D, int32 NMon, int32 Seed)
	{
		P.Box(0.f, 0.f, 74.f, D, W, 4.f, "PaintGrey");
		for (float S : { -1.f, 1.f }) { P.Box(0.f, S * (W * 0.5f - 4.f), 36.f, D - 8.f, 4.f, 72.f, "SteelBlack"); }
		P.Box(D * 0.5f - 4.f, 0.f, 50.f, 2.f, W - 10.f, 36.f, "SteelBlack");
		static const FName Scr[] = { "Screen1", "Screen0", "Screen1" };
		const float MW = FMath::Min(46.f, (W - 14.f) / NMon - 4.f);
		for (int32 i = 0; i < NMon; ++i)
		{
			const float K = i - (NMon - 1) * 0.5f;
			DrawMonitor(P, D * 0.5f - 14.f - FMath::Abs(K) * 4.f, K * (MW + 5.f), 103.f, MW, 28.f, -K * 14.f, Scr[(i + FMath::Abs(Seed)) % 3]);
		}
		P.Box(-D * 0.5f + 16.f, J(Seed, 1) * 6.f, 77.f, 13.f, 40.f, 2.f, "PlasticBlack", J(Seed, 2) * 5.f);   // keyboard
		P.Cyl(-D * 0.5f + 18.f, 32.f, 80.5f, 7.f, 9.f, "Enamel");                                            // mug
		P.Box(-4.f, -W * 0.5f + 18.f, 77.5f, 22.f, 16.f, 3.f, "Black", J(Seed, 3) * 20.f);                    // a drive
		// the tower underneath
		P.Box(4.f, W * 0.5f - 18.f, 23.f, 44.f, 18.f, 44.f, "PlasticBlack");
		P.Box(-18.2f, W * 0.5f - 18.f, 36.f, 0.6f, 3.f, 3.f, "LedB", 0.f, 0.f, 0.f, false);
	}

	/** Swivel chair, facing +X. */
	void DrawSwivelChair(const FPen& P)
	{
		P.Cyl(0.f, 0.f, 4.f, 46.f, 4.f, "PlasticBlack");
		P.Cyl(0.f, 0.f, 26.f, 5.f, 38.f, "Chrome");
		P.Box(0.f, 0.f, 48.f, 48.f, 48.f, 9.f, "Fabric");
		P.Box(-25.f, 0.f, 82.f, 8.f, 44.f, 54.f, "Fabric", 0.f, -8.f);
		for (float S : { -1.f, 1.f }) { P.Box(2.f, S * 25.f, 62.f, 30.f, 4.f, 4.f, "PlasticBlack"); }
	}

	/** Cooling unit H tall, grille facing +X. */
	void DrawCooler(const FPen& P, float H)
	{
		P.Box(22.f, 0.f, H * 0.5f, 42.f, 72.f, H, "PlasticWhite");
		const int32 N = H > 120.f ? 5 : 3;
		for (int32 i = 0; i < N; ++i) { P.Box(43.4f, 0.f, H - 16.f - i * 9.f, 1.f, 60.f, 3.f, "Black"); }
		P.Box(43.4f, 24.f, H * 0.3f, 1.f, 10.f, 5.f, "LedG", 0.f, 0.f, 0.f, false);
		P.Cyl(22.f, -22.f, H + 8.f, 9.f, 16.f, "Galv");
	}

	void DrawUps(const FPen& P, int32 Seed)
	{
		P.Box(24.f, 0.f, 40.f, 46.f, 58.f, 80.f, "PlasticBlack");
		P.Box(47.4f, 0.f, 62.f, 1.f, 40.f, 10.f, "SteelBlack");
		for (int32 i = 0; i < 3; ++i) { P.Box(48.f, -12.f + i * 12.f, 62.f, 1.f, 5.f, 4.f, i == 2 && U(Seed, 5) > 0.5f ? FName("LedA") : FName("LedG"), 0.f, 0.f, 0.f, false); }
		for (int32 i = 0; i < 3; ++i) { P.Box(47.4f, 0.f, 14.f + i * 9.f, 1.f, 46.f, 3.f, "Black"); }
		P.Rod(FVector(8.f, 20.f, 80.f), FVector(2.f, 24.f, 150.f), 2.5f, "Rubber", false);
	}

	/** Low steel shelving of spares, W wide. */
	void DrawPartsShelf(const FPen& P, float W, int32 Seed, bool bLow = false)
	{
		const float Top = bLow ? 70.f : 98.f, Mid = bLow ? 40.f : 52.f;
		for (float SX : { 3.f, 35.f }) { for (float SY : { -1.f, 1.f }) { P.Box(SX, SY * (W * 0.5f - 2.f), Top * 0.5f, 3.f, 3.f, Top, "Galv"); } }
		for (float Z : { 12.f, Mid, Top - 2.f }) { P.Box(19.f, 0.f, Z, 36.f, W, 3.f, "Galv"); }
		P.Box(19.f, -W * 0.5f + 22.f, 26.f, 30.f, 34.f, 24.f, "Cardboard", J(Seed, 1) * 6.f);
		P.Box(19.f, W * 0.5f - 24.f, 22.f, 28.f, 30.f, 16.f, "PlasticBlack");
		P.Box(18.f, -W * 0.5f + 20.f, Mid + 7.f, 26.f, 22.f, 10.f, "Rack", J(Seed, 2) * 8.f);
		P.Box(18.f, -W * 0.5f + 20.f, Mid + 16.f, 24.f, 20.f, 7.f, "SteelBlack", J(Seed, 3) * 10.f);
		P.Cyl(19.f, W * 0.5f - 22.f, Mid + 8.f, 22.f, 12.f, "PaintBlue");
		P.Cyl(19.f, 4.f, Mid + 6.f, 16.f, 9.f, "Rubber");
		P.Box(19.f, 0.f, Top + 6.f, 30.f, 40.f, 12.f, "Cardboard", J(Seed, 4) * 7.f);
	}

	/** A cable drum lying on its rim. */
	void DrawCableReel(const FPen& P, int32 Seed)
	{
		const float Yw = J(Seed, 1) * 25.f;
		for (float S : { -1.f, 1.f }) { P.Cyl(0.f, S * 15.f, 29.f, 58.f, 3.f, "Plywood", 90.f, 90.f + Yw); }
		P.Cyl(0.f, 0.f, 29.f, 36.f, 27.f, (Seed & 1) ? FName("PaintBlue") : FName("Rubber"), 90.f, 90.f + Yw);
		P.Rod(FVector(16.f, 0.f, 16.f), FVector(46.f, J(Seed, 2) * 16.f, 1.5f), 3.f, "Rubber", false);
	}

	/** Cable runs across the floor, fanning out along +X over a patch LX deep, LY wide (centred on the pen). */
	void DrawCables(const FPen& P, float LX, float LY, int32 Seed)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			const float Y0 = (i - 1.5f) * LY * 0.22f;
			const FVector A(-LX * 0.5f, Y0, 1.6f), B(J(Seed, 10 + i) * LX * 0.15f, Y0 + J(Seed, 20 + i) * LY * 0.2f, 1.6f), C2(LX * 0.5f, Y0 * 0.4f + J(Seed, 30 + i) * LY * 0.15f, 1.6f);
			const FName M = (i % 2) ? FName("PaintBlue") : FName("Rubber");
			P.Rod(A, B, 3.f, M, false);
			P.Rod(B, C2, 3.f, M, false);
		}
	}

	/** Uplink: a cabinet with a small dish on a mast. */
	void DrawUplink(const FPen& P)
	{
		P.Box(24.f, 0.f, 30.f, 44.f, 48.f, 60.f, "PaintGrey");
		P.Box(46.4f, 0.f, 44.f, 1.f, 30.f, 8.f, "Screen1", 0.f, 0.f, 0.f, false);
		P.Box(46.4f, -14.f, 24.f, 1.f, 4.f, 4.f, "LedB", 0.f, 0.f, 0.f, false);
		P.Cyl(24.f, 0.f, 84.f, 5.f, 48.f, "Galv");
		P.Ball(30.f, 0.f, 112.f, 10.f, 50.f, 50.f, "PlasticWhite");
		P.Rod(FVector(34.f, 0.f, 112.f), FVector(50.f, 0.f, 116.f), 2.5f, "Galv");
		P.Ball(51.f, 0.f, 116.f, 6.f, 6.f, 6.f, "SteelBlack");
	}
}

using namespace DCFurnishServer;

void ADCHideoutBuilder::FurnishServerRoom(DCFurnish::FCtx& Ctx)
{
	using FFrame = DCFurnish::FFrame; // (the engine has its own FFrame / FEdge)
	using FEdge = DCFurnish::FEdge;
	DCFurnish::FPlan& Plan = Ctx.Plan;
	const int32 Seed = Ctx.Seed;
	const FLinearColor Cool(0.2f, 0.55f, 1.f);

	// free-standing candidates: turned to the camera or to either open side
	TArray<FFrame> TurnedF;
	for (const FFrame& F0 : Ctx.FreeF)
	{
		for (float Y : { YawOf(KToCam), Plan.RoomYaw - 90.f, Plan.RoomYaw + 180.f }) { FFrame F = F0; F.Yaw = Y; TurnedF.Add(F); }
	}
	auto AtBack = [&](const FFrame& Cand) -> float
	{
		if (FV2::DotProduct(Cand.O - Plan.Hub, KToCam) > 0.f) { return -1e9f; } // the back half only
		return (float)(-FV2::Distance(Cand.O, Plan.Hub - KToCam * 150.f) * 0.5f + Ctx.FacingCam(Cand) * 20.f);
	};
	auto Adjacent = [&](const FFrame& Cand) -> float { return (float)(-FMath::Min(Plan.NearestSolid(Cand.O), 200.f) * 0.15f - Plan.CornerDist(Cand.O) * 0.04f); };

	Ctx.DefaultLamp();

	// ---------------------------------------------------------------- 1. the racks (the room's station)
	// A row of full-height racks on a tall wall (bricking a window up if need be); else half-height ones backed onto a
	// partition, else low ones wherever they fit.
	FFrame Racks;
	bool bTall = false;
	auto RackRow = [&](int32 N, float RH, bool bGlow) -> FDrawFn
	{
		return [this, N, RH, bGlow, Cool](const FPen& P, int32 S)
		{
			DrawRackRow(P, N, RH, S);
			if (bGlow) { Point(P.P(105.f, 0.f, 95.f), Cool, 7.f, 340.f, false, true, 6.f); }
		};
	};
	auto RackParts = [](int32 N, float Z1) -> TArray<FLocal>
	{
		return { FLocal::Solid(34.f, 0.f, 33.f, N * KRackPitch * 0.5f + 1.f, Z1), FLocal::Space(92.f, 0.f, 24.f, N * KRackPitch * 0.5f - 12.f, 60.f) };
	};
	for (int32 N : { 3, 2, 1 })
	{
		const TArray<FLocal> Ls = RackParts(N, 222.f);
		Racks = Ctx.BestSoft(Ls, Ctx.WallF, [&](const FFrame& Cand)
		{
			const FEdge& Ed = Plan.E[Cand.Edge];
			return (float)(-FMath::Abs(Cand.T - Ed.L * 0.5f) * 0.1f + Ed.L * 0.02f + Ctx.Jit(Cand.Edge) * 3.f);
		}, 4.f, 40.f);
		if (!Racks.bValid) { continue; }
		Ctx.PlaceSoft(TEXT("racks"), Ls, Racks, RackRow(N, 205.f, true));
		bTall = true;
		break;
	}
	if (!Racks.bValid)
	{
		struct FTry { const TArray<FFrame>* Frames; float RH; bool bFree; };
		const FTry Tries[] = { { &Ctx.WallPartF, 140.f, false }, { &Ctx.AnyWallF, 82.f, false }, { &TurnedF, 82.f, true } };
		for (const FTry& T : Tries)
		{
			for (int32 N : { 3, 2 })
			{
				const TArray<FLocal> Ls = RackParts(N, T.RH + 2.f);
				Racks = Plan.Best(Ls, *T.Frames, [&](const FFrame& Cand)
				{
					if (T.bFree) { return AtBack(Cand); }
					return (float)(Ctx.FacingCam(Cand) * 10.f - FMath::Abs(Cand.T - Plan.E[Cand.Edge].L * 0.3f) * 0.03f + Ctx.Jit(Cand.Edge) * 3.f);
				}, 4.f, TEXT("low racks"));
				if (!Racks.bValid) { continue; }
				Ctx.Place(TEXT("racks"), Ls, Racks, RackRow(N, T.RH, true));
				if (T.bFree) { Racks.Edge = -1; }
				break;
			}
			if (Racks.bValid) { break; }
		}
	}
	if (Racks.bValid)
	{
		Ctx.Spot(Racks, 96.f, -20.f);
		// cable spills out from under them
		for (float K : { 1.f, 0.75f })
		{
			const TArray<FLocal> Ls = { FLocal::Rug(120.f, 0.f, 52.f * K, 100.f * K) };
			if (!Plan.Fits(Ls, Racks, 0.f)) { continue; }
			const float LX = 104.f * K, LY = 200.f * K;
			Ctx.Place(TEXT("cables"), Ls, Racks, [LX, LY](const FPen& P, int32 S) { DrawCables(P, LX, LY, S); }, NAME_None, true);
			break;
		}
	}

	// ---------------------------------------------------------------- 2. the hacker's desk
	// Pushed against a back wall or partition so the screens face the camera; else standing free, turned to it.
	FFrame Desk;
	auto TryDesk = [&](const TCHAR* Name, const FV2& Sz, int32 NMon, FName Group, bool bGlow) -> FFrame
	{
		const float HW = Sz.X * 0.5f + 1.f, HD = Sz.Y * 0.5f + 1.f;
		const TArray<FLocal> Ls = { FLocal::Solid(HD + 2.f, 0.f, HD, HW, 120.f), FLocal::Space(Sz.Y + 36.f, 0.f, 30.f, 36.f, 84.f) };
		FFrame F = Plan.Best(Ls, Ctx.WallPartF, [&](const FFrame& Cand)
		{
			return (float)(Ctx.FacingCam(Cand) * 8.f + Ctx.FacingHub(Cand) * 6.f + (Racks.bValid && Cand.Edge == Racks.Edge ? -14.f : 0.f)
				- Plan.CornerDist(Cand.O) * 0.02f + Ctx.Jit(30 + Cand.Edge) * 5.f);
		}, 6.f, Name);
		if (!F.bValid)
		{
			F = Plan.Best(Ls, TurnedF, [&](const FFrame& Cand)
			{
				if (Ctx.FacingCam(Cand) < 0.2f) { return -1e9f; }
				return (float)(Ctx.FacingCam(Cand) * 10.f - FV2::Distance(Cand.O, Plan.Hub - KToCam * 120.f) * 0.1f - FMath::Min(Plan.NearestSolid(Cand.O), 150.f) * 0.05f);
			}, 6.f);
		}
		if (!F.bValid) { return F; }
		const float W = Sz.X, D = Sz.Y;
		Ctx.Place(Name, Ls, F, [this, W, D, NMon, bGlow, Cool](const FPen& P, int32 S)
		{
			DrawHackDesk(P.At(D * 0.5f + 3.f, 0.f, 180.f), W, D, NMon, S);
			DrawSwivelChair(P.At(D + 36.f, J(S, 1) * 6.f, 180.f + J(S, 2) * 22.f));
			if (bGlow) { Point(P.P(D + 20.f, 0.f, 108.f), Cool, 4.f, 240.f, false, true, 4.f); }
		}, Group);
		Ctx.Spot(F, Sz.Y + 36.f, 0.f);
		return F;
	};
	for (const FV2& Sz : { FV2(156.f, 72.f), FV2(118.f, 64.f) })
	{
		Desk = TryDesk(TEXT("desk"), Sz, Sz.X > 130.f ? 3 : 2, NAME_None, true);
		if (Desk.bValid) { break; }
	}

	// ---------------------------------------------------------------- 3. cooling, power, spares
	{
		// the cooling unit: tall in a corner if a wall will take it, else a low one
		const TArray<FLocal> Tall = { FLocal::Solid(22.f, 0.f, 22.f, 37.f, 186.f) };
		const TArray<FLocal> Low = { FLocal::Solid(22.f, 0.f, 22.f, 37.f, 84.f) };
		auto Score = [&](const FFrame& Cand) { return (float)(-Plan.CornerDist(Cand.O) * 0.3f + Ctx.Jit(20 + Cand.Edge) * 6.f); };
		FFrame F = Ctx.BestSoft(Tall, Ctx.WallF, Score, 6.f, 30.f);
		if (F.bValid) { Ctx.PlaceSoft(TEXT("cooler"), Tall, F, [](const FPen& P, int32) { DrawCooler(P, 170.f); }); }
		else
		{
			F = Plan.Best(Low, Ctx.AnyWallF, Score, 6.f);
			if (F.bValid) { Ctx.Place(TEXT("cooler"), Low, F, [](const FPen& P, int32) { DrawCooler(P, 80.f); }); }
		}
	}
	{
		// the UPS, as near the racks as it gets
		const TArray<FLocal> Ls = { FLocal::Solid(24.f, 0.f, 24.f, 30.f, 84.f) };
		const FFrame F = Plan.Best(Ls, Ctx.AnyWallF, [&](const FFrame& Cand)
		{
			return (float)((Racks.bValid ? -FV2::Distance(Cand.O, Racks.O) * 0.1f : 0.f) + Adjacent(Cand) + Ctx.Jit(40 + Cand.Edge) * 4.f);
		}, 5.f);
		if (F.bValid) { Ctx.Place(TEXT("ups"), Ls, F, [](const FPen& P, int32 S) { DrawUps(P, S); }); Ctx.Spot(F, 78.f, 0.f); }
	}
	{
		// spares: a tall shelf on a wall (may cover a window), on a partition, else a low one anywhere
		bool bDone = false;
		auto SpareScore = [&](const FFrame& Cand) -> float { return (float)(Adjacent(Cand) + Ctx.Jit(50 + Cand.Edge) * 4.f); };
		for (float W : { 110.f, 84.f })
		{
			const TArray<FLocal> Ls = { FLocal::Solid(20.f, 0.f, 20.f, W * 0.5f + 1.f, 118.f) };
			FFrame F = Ctx.BestSoft(Ls, Ctx.WallF, SpareScore, 5.f, 30.f);
			if (F.bValid) { Ctx.PlaceSoft(TEXT("spares"), Ls, F, [W](const FPen& P, int32 S) { DrawPartsShelf(P, W, S); }); }
			else
			{
				F = Plan.Best(Ls, Ctx.WallPartF, SpareScore, 5.f);
				if (F.bValid) { Ctx.Place(TEXT("spares"), Ls, F, [W](const FPen& P, int32 S) { DrawPartsShelf(P, W, S); }); }
			}
			if (!F.bValid) { continue; }
			Ctx.Spot(F, 70.f, 0.f);
			bDone = true;
			break;
		}
		if (!bDone)
		{
			const TArray<FLocal> Ls = { FLocal::Solid(20.f, 0.f, 20.f, 43.f, 84.f) };
			const FFrame F = Plan.Best(Ls, Ctx.AnyWallF, SpareScore, 5.f);
			if (F.bValid) { Ctx.Place(TEXT("spares"), Ls, F, [](const FPen& P, int32 S) { DrawPartsShelf(P, 84.f, S, true); }); }
		}
	}
	{
		// a cable drum left where it was last used
		const TArray<FLocal> Ls = { FLocal::Solid(34.f, 0.f, 30.f, 30.f, 60.f) };
		const FFrame F = Plan.Best(Ls, Ctx.AnyWallF, [&](const FFrame& Cand)
		{
			return (float)(Adjacent(Cand) - FV2::DotProduct(Cand.O - Plan.Hub, KToCam) * 0.05f + J(Seed, 760 + Cand.Edge) * 8.f);
		}, 6.f);
		if (F.bValid) { Ctx.Place(TEXT("reel"), Ls, F, [](const FPen& P, int32 S) { DrawCableReel(P.At(34.f, 0.f), S); }); }
	}

	// ---------------------------------------------------------------- 4. upgrades: more racks, a second seat, the uplink
	{
		// extra racks: tall on a wall (windows allowed), else half-height on a partition, else low anywhere
		const FName Groups[] = { DCGroups::Level2, DCGroups::Level3 };
		auto NearRacks = [&](const FFrame& Cand) -> float
		{
			return (float)((Racks.bValid ? -FV2::Distance(Cand.O, Racks.O) * 0.08f : 0.f) + Adjacent(Cand) * 0.5f);
		};
		for (int32 i = 0; i < 2; ++i)
		{
			const int32 N = i == 0 ? 2 : 1;
			const float HY = N * KRackPitch * 0.5f + 1.f;
			const TArray<FLocal> TallLs = { FLocal::Solid(34.f, 0.f, 33.f, HY, 222.f) };
			FFrame F = Ctx.BestSoft(TallLs, Ctx.WallF, NearRacks, 4.f, 40.f);
			if (F.bValid) { Ctx.PlaceSoft(TEXT("racks+"), TallLs, F, RackRow(N, 205.f, false), Groups[i]); continue; }
			for (float RH : { 140.f, 82.f })
			{
				const TArray<FLocal> Ls = { FLocal::Solid(34.f, 0.f, 33.f, HY, RH + 2.f) };
				F = Plan.Best(Ls, RH > 100.f ? Ctx.WallPartF : Ctx.AnyWallF, NearRacks, 4.f);
				if (F.bValid) { Ctx.Place(TEXT("racks+"), Ls, F, RackRow(N, RH, false), Groups[i]); break; }
			}
		}
	}
	TryDesk(TEXT("desk 2"), FV2(118.f, 64.f), 2, DCGroups::Level2, false);
	{
		// the uplink: against a wall or partition, else standing free away from the open edges
		const TArray<FLocal> Ls = { FLocal::Solid(26.f, 0.f, 26.f, 27.f, 142.f) };
		auto UpScore = [&](const FFrame& Cand) -> float { return (float)(Adjacent(Cand) + Ctx.FacingCam(Cand) * 4.f); };
		FFrame F = Ctx.BestSoft(Ls, Ctx.WallF, UpScore, 5.f, 30.f);
		if (F.bValid) { Ctx.PlaceSoft(TEXT("uplink"), Ls, F, [](const FPen& P, int32) { DrawUplink(P); }, DCGroups::Level3); }
		else
		{
			F = Plan.Best(Ls, Ctx.WallPartF, UpScore, 5.f);
			if (!F.bValid) { F = Plan.Best(Ls, TurnedF, UpScore, 5.f); }
			if (F.bValid) { Ctx.Place(TEXT("uplink"), Ls, F, [](const FPen& P, int32) { DrawUplink(P); }, DCGroups::Level3); }
		}
	}

	// ---------------------------------------------------------------- 5. the rest
	Ctx.Runner();
	Ctx.Clutter(0.38f);
}
