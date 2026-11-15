// Director's Cut — the Ops Room's furnishing recipe (see DCHideoutFurnish.cpp): the surveillance wall (a bank of
// monitors over a long console) with its operators' chairs, a lit map table, a comms rack, files and a pin board.
// With no tall wall to hang the bank on, the console keeps its desk monitors and backs onto a partition or stands
// free with its screens to the camera.
#include "World/Scene/DCHideoutFurnish.h"
#include "Components/PointLightComponent.h"

using namespace DCFurnish;

namespace DCFurnishOps
{
	/** A flat monitor facing +X, centred at (X, Y, Zc). */
	void DrawMonitor(const DCFurnish::FPen& P, float X, float Y, float Zc, float W, float H, FName Screen)
	{
		P.Box(X, Y, Zc, 3.5f, W + 3.f, H + 3.f, "PlasticBlack");
		P.Box(X + 1.9f, Y, Zc, 0.4f, W, H, Screen, 0.f, 0.f, 0.f, false);
	}

	/** A swivel chair; the sitter faces +X. */
	void DrawOpChair(const DCFurnish::FPen& P)
	{
		for (int32 i = 0; i < 5; ++i)
		{
			const FV2 A = Dir(i * 72.f) * 15.f;
			P.Box(A.X, A.Y, 5.f, 30.f, 4.5f, 3.5f, "PlasticBlack", i * 72.f);
		}
		P.Cyl(0.f, 0.f, 26.f, 5.5f, 38.f, "Chrome");
		P.Box(0.f, 0.f, 48.f, 48.f, 48.f, 9.f, "Fabric");
		P.Box(-25.f, 0.f, 82.f, 8.f, 44.f, 54.f, "Fabric", 0.f, -8.f);
		for (float S : { -1.f, 1.f }) { P.Box(2.f, S * 25.f, 62.f, 28.f, 4.f, 4.f, "PlasticBlack"); }
	}

	/** The console: a long desk against the frame's back with a row of desk monitors. BankTop > 0: a bank of wall
	 *  monitors above it, up to that height. */
	void DrawConsole(const DCFurnish::FPen& P, float W, float BankTop, int32 Seed)
	{
		P.Box(34.f, 0.f, 74.f, 66.f, W, 4.f, "PaintGrey");
		P.Box(4.f, 0.f, 38.f, 4.f, W - 8.f, 70.f, "SteelBlack");
		for (float S : { -1.f, 1.f }) { P.Box(34.f, S * (W * 0.5f - 3.f), 36.f, 62.f, 4.f, 72.f, "SteelBlack"); }
		P.Box(62.f, 0.f, 70.f, 3.f, W - 12.f, 2.f, "Accent", 0.f, 0.f, 0.f, false);
		const int32 N = FMath::Max(2, FMath::RoundToInt(W / 76.f));
		for (int32 i = 0; i < N; ++i)
		{
			const float Y = (i - (N - 1) * 0.5f) * (W - 26.f) / N;
			const DCFurnish::FPen M = P.At(17.f, Y, (i - (N - 1) * 0.5f) * 7.f);
			M.Box(0.f, 0.f, 78.f, 14.f, 16.f, 4.f, "PlasticBlack");
			M.Box(0.f, 0.f, 84.f, 3.f, 5.f, 10.f, "PlasticBlack");
			DrawMonitor(M, 0.f, 0.f, 100.f, 56.f, 32.f, i == N / 2 ? FName("Screen2") : (i % 2 ? FName("Screen0") : FName("Screen1")));
			P.Box(46.f, Y + J(Seed, 10 + i) * 4.f, 77.f, 15.f, 42.f, 2.f, "PlasticBlack", J(Seed, 20 + i) * 5.f);
			if (U(Seed, 30 + i) > 0.4f) { P.Cyl(52.f, Y + 30.f, 80.f, 8.f, 9.f, i % 2 ? FName("PaintRed") : FName("PlasticWhite")); }
		}
		// a recorder under one end, blinking away
		P.Box(30.f, W * 0.5f - 26.f, 26.f, 44.f, 22.f, 52.f, "Rack");
		for (int32 i = 0; i < 4; ++i) { P.Box(52.4f, W * 0.5f - 32.f + i * 4.f, 44.f - (i % 2) * 6.f, 0.6f, 2.f, 2.f, i % 2 ? FName("LedG") : FName("LedB"), 0.f, 0.f, 0.f, false); }
		if (BankTop <= 0.f) { return; }
		// the wall bank on a steel frame
		const float Z0 = 122.f, CellH = 36.f;
		const int32 Rows = FMath::Clamp(FMath::FloorToInt((BankTop - Z0) / CellH), 1, 3);
		const int32 Cols = FMath::Max(2, FMath::FloorToInt((W - 6.f) / 64.f));
		const float CellW = (W - 6.f) / Cols;
		for (int32 c = 0; c <= Cols; c += FMath::Max(1, Cols / 2)) { P.Box(2.5f, (c - Cols * 0.5f) * CellW, (Z0 + Rows * CellH) * 0.5f + 40.f, 5.f, 5.f, Z0 + Rows * CellH - 80.f, "SteelBlack"); }
		P.Box(2.5f, 0.f, Z0 + Rows * CellH + 1.5f, 5.f, W, 3.f, "SteelBlack");
		for (int32 r = 0; r < Rows; ++r)
		{
			for (int32 c = 0; c < Cols; ++c)
			{
				const float U01 = U(Seed, 40 + r * 9 + c);
				const FName Scr = U01 > 0.8f ? FName("Screen2") : U01 > 0.5f ? FName("Screen1") : FName("Screen0");
				DrawMonitor(P, 5.f, (c + 0.5f - Cols * 0.5f) * CellW, Z0 + (r + 0.5f) * CellH, CellW - 6.f, CellH - 6.f, Scr);
			}
		}
		P.Box(7.f, 0.f, Z0 - 2.f, 1.f, W - 10.f, 1.5f, "Accent", 0.f, 0.f, 0.f, false);
		for (int32 c = 0; c < Cols; c += 2) { P.Rod(FVector(3.f, (c + 0.5f - Cols * 0.5f) * CellW, Z0), FVector(1.5f, (c + 0.8f - Cols * 0.5f) * CellW, 78.f), 1.4f, "Rubber", false); }
	}

	/** A light table with the city plan on it. W along Y, D along X, centred on the frame. */
	void DrawMapTable(const DCFurnish::FPen& P, float W, float D, int32 Seed)
	{
		for (float SX : { -1.f, 1.f }) { for (float SY : { -1.f, 1.f }) { P.Box(SX * (D * 0.5f - 6.f), SY * (W * 0.5f - 6.f), 38.f, 6.f, 6.f, 76.f, "SteelBlack"); } }
		P.Box(0.f, 0.f, 22.f, D - 14.f, W - 14.f, 3.f, "SteelBlack");
		P.Box(0.f, 0.f, 78.f, D, W, 7.f, "SteelBlack");
		P.Box(0.f, 0.f, 82.f, D - 8.f, W - 8.f, 1.5f, "Blueprint", 0.f, 0.f, 0.f, false);
		P.Box(J(Seed, 1) * 6.f, -W * 0.12f, 83.f, D * 0.62f, W * 0.5f, 0.5f, "Paper", J(Seed, 2) * 7.f, 0.f, 0.f, false);
		// the blocks of the target's street, a route in red, a few markers
		for (int32 i = 0; i < 6; ++i)
		{
			const float X = (U(Seed, 10 + i) - 0.5f) * D * 0.5f, Y = W * (0.12f + 0.3f * U(Seed, 20 + i));
			P.Box(X, Y, 84.f + 2.f + U(Seed, 30 + i) * 3.f, 7.f + U(Seed, 40 + i) * 6.f, 7.f + U(Seed, 50 + i) * 6.f, 4.f + U(Seed, 30 + i) * 6.f, i == 2 ? FName("PaintRed") : FName("PlasticWhite"), J(Seed, 60 + i) * 8.f);
		}
		P.Rod(FVector(-D * 0.22f, -W * 0.3f, 83.6f), FVector(D * 0.1f, -W * 0.05f, 83.6f), 1.2f, "PaintRed", false);
		P.Rod(FVector(D * 0.1f, -W * 0.05f, 83.6f), FVector(D * 0.05f, W * 0.2f, 83.6f), 1.2f, "PaintRed", false);
		for (int32 i = 0; i < 3; ++i) { P.Ball(-D * 0.22f + i * D * 0.16f, -W * 0.3f + i * W * 0.13f, 85.f, 2.4f, 2.4f, 2.4f, "Pin", false); }
		P.Cyl(D * 0.3f, -W * 0.38f, 87.f, 8.f, 9.f, "PlasticWhite");
	}

	/** A comms rack against the frame's back, Ht tall, a field radio and its aerial on top. */
	void DrawCommsRack(const DCFurnish::FPen& P, float Ht, int32 Seed)
	{
		P.Box(26.f, 0.f, Ht * 0.5f, 50.f, 64.f, Ht, "Rack");
		const int32 N = FMath::FloorToInt((Ht - 14.f) / 17.f);
		for (int32 i = 0; i < N; ++i)
		{
			const float Z = 14.f + i * 17.f;
			P.Box(51.4f, 0.f, Z, 1.2f, 56.f, 12.f, i % 3 == 1 ? FName("PaintGrey") : FName("SteelBlack"));
			for (int32 k = 0; k < 4; ++k)
			{
				if (U(Seed, i * 7 + k) < 0.3f) { continue; }
				P.Box(52.2f, -20.f + k * 5.f, Z + 2.f, 0.6f, 2.f, 2.f, U(Seed, 90 + i * 5 + k) > 0.75f ? FName("LedR") : (k % 2 ? FName("LedG") : FName("LedB")), 0.f, 0.f, 0.f, false);
			}
			if (i % 3 == 1) { P.Cyl(52.6f, 16.f, Z, 7.f, 1.6f, "PlasticBlack", 90.f); }
		}
		P.Box(24.f, -10.f, Ht + 9.f, 30.f, 36.f, 18.f, "PaintOlive");
		P.Box(39.4f, -10.f, Ht + 10.f, 0.8f, 26.f, 9.f, "PlasticBlack");
		P.Cyl(39.8f, -2.f, Ht + 10.f, 5.f, 1.6f, "Chrome", 90.f);
		P.Rod(FVector(14.f, -24.f, Ht + 18.f), FVector(12.f, -27.f, Ht + 62.f), 1.f, "Chrome", false);
		// a headset hung on the corner
		P.Cyl(40.f, 22.f, Ht + 5.f, 9.f, 4.f, "PlasticBlack", 90.f);
		P.Rod(FVector(40.f, 22.f, Ht + 5.f), FVector(52.f, 26.f, Ht - 22.f), 0.8f, "Rubber", false);
	}

	/** One more operator's place: a short desk with two screens, the chair out front. */
	void DrawOpDesk(const DCFurnish::FPen& P, int32 Seed)
	{
		P.Box(30.f, 0.f, 74.f, 58.f, 118.f, 4.f, "PaintGrey");
		for (float S : { -1.f, 1.f }) { P.Box(30.f, S * 56.f, 36.f, 54.f, 4.f, 72.f, "SteelBlack"); }
		P.Box(4.f, 0.f, 46.f, 3.f, 108.f, 50.f, "SteelBlack");
		for (float S : { -1.f, 1.f })
		{
			const DCFurnish::FPen M = P.At(15.f, S * 29.f, S * 9.f);
			M.Box(0.f, 0.f, 78.f, 13.f, 15.f, 4.f, "PlasticBlack");
			DrawMonitor(M, 0.f, 0.f, 98.f, 52.f, 30.f, S < 0.f ? FName("Screen1") : FName("Screen0"));
		}
		P.Box(42.f, 0.f, 77.f, 15.f, 42.f, 2.f, "PlasticBlack", J(Seed, 1) * 5.f);
		DrawOpChair(P.At(88.f, J(Seed, 2) * 6.f, 180.f + J(Seed, 3) * 20.f));
	}

	/** An aerial mast with a dish, guyed to a weighted base. */
	void DrawMast(const DCFurnish::FPen& P)
	{
		P.Box(22.f, 0.f, 5.f, 40.f, 40.f, 10.f, "SteelBlack");
		P.Cyl(22.f, 0.f, 116.f, 5.f, 212.f, "Alu");
		for (float S : { -1.f, 1.f }) { P.Rod(FVector(38.f, S * 17.f, 9.f), FVector(22.f, 0.f, 120.f), 1.f, "SteelBlack", false); }
		P.Cyl(30.f, 0.f, 186.f, 42.f, 5.f, "PlasticWhite", 72.f);
		P.Rod(FVector(31.f, 0.f, 186.f), FVector(46.f, 0.f, 191.f), 1.4f, "Chrome", false);
		for (int32 i = 0; i < 3; ++i) { P.Box(22.f, 0.f, 204.f + i * 8.f, 2.f, 50.f - i * 12.f, 2.f, "Alu"); }
		P.Ball(22.f, 0.f, 224.f, 4.f, 4.f, 4.f, "LedR", false);
	}

	/** The low stand-in for the mast: a signal case with a small dish on its lid. */
	void DrawSignalCase(const DCFurnish::FPen& P)
	{
		P.Box(26.f, 0.f, 26.f, 48.f, 70.f, 52.f, "PaintOlive");
		P.Box(26.f, 0.f, 54.f, 50.f, 72.f, 4.f, "SteelBlack");
		for (float S : { -1.f, 1.f }) { P.Box(51.f, S * 22.f, 40.f, 2.f, 8.f, 6.f, "Chrome"); }
		P.Cyl(24.f, -12.f, 68.f, 4.f, 24.f, "Alu");
		P.Cyl(28.f, -12.f, 84.f, 30.f, 4.f, "PlasticWhite", 68.f);
		P.Box(40.f, 20.f, 58.f, 14.f, 20.f, 4.f, "PlasticBlack");
		P.Ball(26.f, 26.f, 58.f, 3.f, 3.f, 3.f, "LedG", false);
	}
}

void ADCHideoutBuilder::FurnishOpsRoom(DCFurnish::FCtx& Ctx)
{
	using namespace DCFurnishOps;
	DCFurnish::FPlan& Plan = Ctx.Plan;
	const int32 Seed = Ctx.Seed;
	const float BankTop = FMath::Min(204.f, Ctx.LampZ - 18.f);
	const FLinearColor Cool(0.35f, 0.75f, 1.f);
	// wall frames nudged a few cm off the face: the solver rejects a back exactly on the inner line as "outside"
	auto Nudge = [](const TArray<DCFurnish::FFrame>& In) { TArray<DCFurnish::FFrame> Out = In; for (DCFurnish::FFrame& F : Out) { F.O += Dir(F.Yaw) * 5.f; } return Out; };
	const TArray<DCFurnish::FFrame> WallF = Nudge(Ctx.WallF), WallPartF = Nudge(Ctx.WallPartF);

	// ---------------------------------------------------------------- 1. the surveillance wall (the room's station)
	// The widest console a tall wall takes, its bank of monitors above it (a window may be bricked up for it).
	DCFurnish::FFrame Console;
	float ConW = 0.f;
	bool bBank = false;
	auto ConsoleParts = [&](float W, bool bWall)
	{
		TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(34.f, 0.f, 34.f, W * 0.5f, 118.f), DCFurnish::FLocal::Space(96.f, 0.f, 30.f, W * 0.5f - 12.f, 84.f) };
		Ls[1].Tag = TagSeat; // the operators' chairs go here
		if (bWall) { Ls.Add(DCFurnish::FLocal::Space(5.f, 0.f, 5.f, W * 0.5f, BankTop + 4.f)); Ls.Last().Z0 = 118.f; }
		return Ls;
	};
	{
		float BestS = -1e8f;
		for (float W : { 300.f, 250.f, 200.f, 160.f })
		{
			const TArray<DCFurnish::FLocal> Ls = ConsoleParts(W, true);
			auto Score = [&](const DCFurnish::FFrame& Cand)
			{
				const DCFurnish::FEdge& Ed = Plan.E[Cand.Edge];
				return (float)(W * 0.5f - FMath::Abs(Cand.T - Ed.L * 0.5f) * 0.12f + Ed.L * 0.02f + Ctx.Jit(Cand.Edge) * 3.f);
			};
			const DCFurnish::FFrame F = Ctx.BestSoft(Ls, WallF, Score, 4.f, 45.f);
			if (!F.bValid) { continue; }
			Plan.WindowsHit(Ls, F, Ctx.Hit);
			const float S = Score(F) - 45.f * Ctx.Hit.Num();
			if (S > BestS) { BestS = S; Console = F; ConW = W; }
		}
	}
	auto DrawCon = [this, Cool](float W, float Top) -> DCFurnish::FDrawFn
	{
		return [this, Cool, W, Top](const DCFurnish::FPen& P, int32 S)
		{
			DrawConsole(P, W, Top, S);
			Point(P.P(72.f, 0.f, 122.f), Cool, 9.f, 330.f, false, true, 6.f);
		};
	};
	if (Console.bValid)
	{
		bBank = true;
		Ctx.PlaceSoft(TEXT("console"), ConsoleParts(ConW, true), Console, DrawCon(ConW, BankTop));
		Ctx.SetLamp(Console.Edge, Console.T); // the room light over the bank
	}
	else
	{
		// no tall wall: desk monitors only, backed onto a partition, else standing free with its screens to the camera
		Ctx.DefaultLamp();
		for (float W : { 220.f, 180.f, 140.f })
		{
			const TArray<DCFurnish::FLocal> Ls = ConsoleParts(W, false);
			Console = Plan.Best(Ls, WallPartF, [&](const DCFurnish::FFrame& Cand)
			{
				return (float)(Ctx.FacingCam(Cand) * 10.f - FMath::Abs(Cand.T - Plan.E[Cand.Edge].L * 0.5f) * 0.02f + Ctx.Jit(Cand.Edge) * 3.f);
			}, 4.f, TEXT("console low"));
			if (!Console.bValid)
			{
				TArray<DCFurnish::FFrame> Fr;
				for (const DCFurnish::FFrame& F0 : Ctx.FreeF)
				{
					for (float Y : { YawOf(KToCam), Plan.RoomYaw - 90.f, Plan.RoomYaw + 180.f }) { DCFurnish::FFrame F = F0; F.Yaw = Y; Fr.Add(F); }
				}
				Console = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& Cand)
				{
					if (FV2::DotProduct(Cand.O - Plan.Hub, KToCam) > 0.f) { return -1e9f; } // the back half only
					return (float)(-FV2::Distance(Cand.O, Plan.Hub - KToCam * 170.f) * 0.5f + Ctx.FacingCam(Cand) * 20.f);
				}, 4.f, TEXT("console free"));
				if (Console.bValid) { Console.Edge = -1; }
			}
			if (Console.bValid) { ConW = W; Ctx.Place(TEXT("console"), Ls, Console, DrawCon(W, 0.f)); break; }
		}
	}
	if (Console.bValid)
	{
		// one or two operators' chairs, pushed back a little and turned
		TArray<float> Ys;
		if (ConW >= 240.f) { Ys = { -ConW * 0.25f, ConW * 0.25f }; } else { Ys = { -ConW * 0.2f }; }
		Plan.CapScale = 0.6f; Plan.bHubKeep = false; // a chair may stand in the way a little
		for (int32 i = 0; i < Ys.Num(); ++i)
		{
			DCFurnish::FFrame F = Console;
			F.O = Console.O + Rot(FV2(104.f + U(Seed, 600 + i) * 8.f, Ys[i] + J(Seed, 605 + i) * 6.f), Console.Yaw);
			F.Yaw = Console.Yaw + 180.f + J(Seed, 610 + i) * 18.f; F.Edge = -1;
			TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(0.f, 0.f, 25.f, 25.f, 100.f) };
			Ls[0].Tag = TagSeat;
			if (Plan.Fits(Ls, F, 3.f)) { Ctx.Place(TEXT("chair"), Ls, F, [](const DCFurnish::FPen& P, int32) { DrawOpChair(P); }); }
		}
		Plan.CapScale = 1.f; Plan.bHubKeep = true;
		Plan.Parts.RemoveAll([](const DCFurnish::FPart& Pt) { return Pt.Tag == TagSeat && !Pt.bSolid; });
		Ctx.Spot(Console, 100.f, ConW >= 240.f ? 0.f : ConW * 0.24f);
	}

	// ---------------------------------------------------------------- 2. the map table, out in front of the console
	DCFurnish::FFrame Table;
	float TableW = 0.f, TableD = 0.f;
	for (const FV2& Sz : { FV2(150.f, 92.f), FV2(124.f, 82.f), FV2(104.f, 72.f) })
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(0.f, 0.f, Sz.Y * 0.5f, Sz.X * 0.5f, 85.f) };
		TArray<DCFurnish::FFrame> Fr = Ctx.FreeF;
		for (const DCFurnish::FFrame& F0 : Ctx.FreeF) { DCFurnish::FFrame G = F0; G.Yaw += 90.f; Fr.Add(G); }
		const FV2 Want = Console.bValid ? Console.O + Dir(Console.Yaw) * 235.f : Plan.Hub - KToCam * 110.f;
		Table = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& Cand)
		{
			if (Console.bValid && FV2::DotProduct(Cand.O - Console.O, Dir(Console.Yaw)) < 175.f) { return -1e9f; } // clear of the operators
			return (float)(-FV2::Distance(Cand.O, Want) * 0.4f - FMath::Abs(FMath::FindDeltaAngleDegrees(Cand.Yaw, Console.bValid ? Console.Yaw : Plan.RoomYaw)) * 0.02f);
		}, 8.f, TEXT("map table"));
		if (!Table.bValid) { continue; }
		TableW = Sz.X; TableD = Sz.Y;
		const float TW = TableW, TD = TableD;
		Ctx.Place(TEXT("maptable"), Ls, Table, [this, Cool, TW, TD](const DCFurnish::FPen& P, int32 S)
		{
			DrawMapTable(P, TW, TD, S);
			Point(P.P(0.f, 0.f, 118.f), Cool, 5.f, 260.f, false, true, 8.f);
		});
		Ctx.Spot(Table, TableD * 0.5f + 34.f, 0.f);
		DCFurnish::FFrame Side = Table; Side.Yaw += 90.f;
		Ctx.Spot(Side, TableW * 0.5f + 34.f, 0.f);
		Ctx.PlaceRug(Table, 0.f, TableD + 110.f, TableW + 100.f, "RugBlue");
		break;
	}

	Ctx.Runner();

	// ---------------------------------------------------------------- 3. comms rack, files, the pin board
	auto Adjacent = [&](const DCFurnish::FFrame& F) { return (float)(-FMath::Min(Plan.NearestSolid(F.O), 200.f) * 0.15f - Plan.CornerDist(F.O) * 0.04f); };
	{
		const TArray<DCFurnish::FLocal> Tall = { DCFurnish::FLocal::Solid(27.f, 0.f, 27.f, 35.f, 184.f) };
		DCFurnish::FFrame F = Ctx.BestSoft(Tall, WallF, [&](const DCFurnish::FFrame& Cand) { return (float)(Adjacent(Cand) + Ctx.Jit(20 + Cand.Edge) * 4.f); }, 5.f, 30.f);
		if (F.bValid)
		{
			Ctx.PlaceSoft(TEXT("rack"), Tall, F, [](const DCFurnish::FPen& P, int32 S) { DrawCommsRack(P, 166.f, S); });
			Ctx.Spot(F, 84.f, 0.f);
		}
		else
		{
			const TArray<DCFurnish::FLocal> Low = { DCFurnish::FLocal::Solid(27.f, 0.f, 27.f, 35.f, 122.f) };
			F = Plan.Best(Low, WallPartF, [&](const DCFurnish::FFrame& Cand) { return (float)(Adjacent(Cand) + Ctx.Jit(20 + Cand.Edge) * 4.f); }, 5.f, TEXT("rack low"));
			if (F.bValid)
			{
				Ctx.Place(TEXT("rack"), Low, F, [](const DCFurnish::FPen& P, int32 S) { DrawCommsRack(P, 98.f, S); });
				Ctx.Spot(F, 84.f, 0.f);
			}
		}
	}
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(31.f, 0.f, 31.f, 48.f, 136.f) };
		const DCFurnish::FFrame F = Ctx.BestSoft(Ls, WallPartF, [&](const DCFurnish::FFrame& Cand) { return (float)(Adjacent(Cand) + Ctx.Jit(30 + Cand.Edge) * 4.f); }, 5.f, 30.f);
		if (F.bValid) { Ctx.PlaceSoft(TEXT("filing"), Ls, F, [](const DCFurnish::FPen& P, int32 S) { DrawFilingCabinets(P, 2, S); }); }
	}
	for (float W : { 150.f, 116.f })
	{
		TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Space(5.f, 0.f, 5.f, W * 0.5f + 6.f, BankTop + 4.f) };
		Ls[0].Z0 = BankTop - 128.f;
		const DCFurnish::FFrame F = Plan.Best(Ls, WallF, [&](const DCFurnish::FFrame& Cand)
		{
			return (float)((Cand.Edge == Console.Edge ? -20.f : 0.f) - FMath::Abs(Cand.T - Plan.E[Cand.Edge].L * 0.5f) * 0.05f + Ctx.Jit(40 + Cand.Edge) * 4.f);
		}, 10.f, TEXT("pinboard"));
		if (!F.bValid) { continue; }
		const float Top = BankTop;
		Ctx.Place(TEXT("pinboard"), Ls, F, [W, Top](const DCFurnish::FPen& P, int32 S) { DrawBoard(P, W, Top, S); });
		Ctx.Spot(F, 70.f, 0.f);
		break;
	}

	// ---------------------------------------------------------------- 4. upgrades: another operator, more screens, the aerial
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(30.f, 0.f, 30.f, 61.f, 116.f), DCFurnish::FLocal::Space(88.f, 0.f, 27.f, 28.f, 100.f) };
		auto DeskScore = [&](const DCFurnish::FFrame& Cand)
		{
			return (float)(Ctx.FacingCam(Cand) * 6.f + Adjacent(Cand) * 0.5f + (Cand.Edge == Console.Edge ? 8.f : 0.f) + Ctx.Jit(50 + Cand.Edge) * 4.f);
		};
		DCFurnish::FFrame F = Ctx.BestSoft(Ls, WallPartF, DeskScore, 6.f, 30.f);
		if (F.bValid) { Ctx.PlaceSoft(TEXT("opdesk"), Ls, F, [](const DCFurnish::FPen& P, int32 S) { DrawOpDesk(P, S); }, DCGroups::Level2); }
		else
		{
			// no wall for it: a free-standing desk, screens to the camera, out of the middle
			TArray<DCFurnish::FFrame> Fr;
			for (const DCFurnish::FFrame& F0 : Ctx.FreeF) { for (float Y : { YawOf(KToCam), Plan.RoomYaw - 90.f, Plan.RoomYaw + 180.f }) { DCFurnish::FFrame G = F0; G.Yaw = Y; Fr.Add(G); } }
			F = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& Cand) { return (float)(Ctx.FacingCam(Cand) * 20.f + FMath::Min(FV2::Distance(Cand.O, Plan.Hub), 220.f) * 0.1f + Adjacent(Cand) * 0.5f); }, 6.f, TEXT("op desk free"));
			if (F.bValid) { Ctx.Place(TEXT("opdesk"), Ls, F, [](const DCFurnish::FPen& P, int32 S) { DrawOpDesk(P, S); }, DCGroups::Level2); }
		}
	}
	{
		TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Space(4.f, 0.f, 4.f, 50.f, 198.f) };
		Ls[0].Z0 = 138.f;
		const DCFurnish::FFrame F = Ctx.BestSoft(Ls, WallF, [&](const DCFurnish::FFrame& Cand) { return (float)(Console.bValid ? -FV2::Distance(Cand.O, Console.O) * 0.1f : 0.f); }, 16.f, 60.f);
		if (F.bValid) { Ctx.PlaceSoft(TEXT("screen"), Ls, F, [](const DCFurnish::FPen& P, int32) { DrawWallScreen(P); }, DCGroups::Level2); }
	}
	{
		const TArray<DCFurnish::FLocal> Tall = { DCFurnish::FLocal::Solid(22.f, 0.f, 22.f, 22.f, 232.f) };
		DCFurnish::FFrame F = Plan.Best(Tall, WallF, [&](const DCFurnish::FFrame& Cand) { return (float)(-Plan.CornerDist(Cand.O) * 0.3f + Ctx.Jit(60 + Cand.Edge) * 4.f); }, 5.f, TEXT("mast"));
		if (F.bValid) { Ctx.Place(TEXT("mast"), Tall, F, [](const DCFurnish::FPen& P, int32) { DrawMast(P); }, DCGroups::Level3); }
		else
		{
			const TArray<DCFurnish::FLocal> Low = { DCFurnish::FLocal::Solid(26.f, 0.f, 26.f, 37.f, 104.f) };
			F = Plan.Best(Low, WallPartF, [&](const DCFurnish::FFrame& Cand) { return (float)(Adjacent(Cand) + Ctx.Jit(60 + Cand.Edge) * 4.f); }, 5.f, TEXT("signalcase"));
			if (F.bValid) { Ctx.Place(TEXT("signalcase"), Low, F, [](const DCFurnish::FPen& P, int32) { DrawSignalCase(P); }, DCGroups::Level3); }
		}
	}

	// ---------------------------------------------------------------- 5. clutter and what's left of the walls
	Ctx.Clutter(0.4f);
	Ctx.WallDecor();
	(void)bBank;
}
