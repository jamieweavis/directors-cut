// Director's Cut — the Clinic's furnishing recipe (see DCHideoutFurnish.cpp): a treatment bed with its drip stand,
// instrument trolley and lamp, a second cot, the medicine cabinet (a low chest where no tall wall stands), a counter
// with a basin, a bench to wait on, a folding screen and the signs on the walls.
#include "World/Scene/DCHideoutFurnish.h"
#include "Components/PointLightComponent.h"

using namespace DCFurnish;

namespace DCFurnishClinic
{
	// ------------------------------------------------------------------------------------------------- props
	/** A bed on castors: head at the origin, lying along +X. Stays under 86 cm. */
	void DrawBed(const DCFurnish::FPen& P, float L, float W, int32 Seed, bool bCot)
	{
		const FName Frame = bCot ? FName("Alu") : FName("Steel");
		for (int32 i = 0; i < 4; ++i)
		{
			const float X = (i % 2) ? L - 12.f : 12.f, Y = ((i / 2) ? 1.f : -1.f) * (W * 0.5f - 7.f);
			P.Cyl(X, Y, 33.f, 3.4f, 52.f, Frame);
			P.Ball(X, Y, 5.f, 10.f, 10.f, 10.f, "Rubber");
		}
		P.Box(L * 0.5f, 0.f, 60.f, L, W, 5.f, Frame);
		P.Box(L * 0.5f, 0.f, 69.f, L - 6.f, W - 6.f, 13.f, "PlasticWhite");
		P.Box(24.f, J(Seed, 1) * 4.f, 79.f, 28.f, W * 0.62f, 8.f, "Linen", J(Seed, 2) * 6.f);
		P.Box(L * 0.64f, 0.f, 77.f, L * 0.62f, W - 3.f, 3.5f, bCot ? FName("BlanketGrey") : FName("Linen"), J(Seed, 3) * 1.5f);
		if (!bCot)
		{
			P.Box(1.5f, 0.f, 68.f, 3.f, W, 30.f, "Steel");                 // headboard
			P.Box(L - 1.5f, 0.f, 66.f, 3.f, W, 16.f, "Steel");             // foot rail
			P.Box(L - 4.f, W * 0.2f, 62.f, 1.f, 20.f, 26.f, "Paper", 0.f, 8.f); // the chart
		}
	}

	void DrawDrip(const DCFurnish::FPen& P, float Top)
	{
		P.Cyl(0.f, 0.f, 1.5f, 34.f, 3.f, "Chrome");
		P.Cyl(0.f, 0.f, Top * 0.5f, 2.6f, Top, "Chrome");
		P.Box(0.f, 0.f, Top, 2.f, 20.f, 2.f, "Chrome");
		P.Box(0.f, 8.f, Top - 14.f, 4.f, 10.f, 22.f, "Plastic", 0.f, 0.f, 0.f, false);
		P.Rod(FVector(0.f, 8.f, Top - 25.f), FVector(6.f, 14.f, 88.f), 0.7f, "PlasticWhite", false);
	}

	void DrawTrolley(const DCFurnish::FPen& P, int32 Seed)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			const float X = (i % 2) ? 15.f : -15.f, Y = (i / 2) ? 21.f : -21.f;
			P.Cyl(X, Y, 43.f, 2.2f, 78.f, "Steel");
			P.Ball(X, Y, 4.f, 8.f, 8.f, 8.f, "Rubber");
		}
		P.Box(0.f, 0.f, 80.f, 36.f, 48.f, 2.5f, "Steel");
		P.Box(0.f, 0.f, 36.f, 36.f, 48.f, 2.5f, "Steel");
		P.Box(-3.f, -9.f, 82.5f, 18.f, 24.f, 2.f, "Enamel");
		for (int32 i = 0; i < 3; ++i) { P.Rod(FVector(-9.f + i * 5.f, -17.f, 84.f), FVector(-8.f + i * 5.f, -2.f, 84.f), 0.9f, "Chrome", false); }
		P.Cyl(6.f, 14.f, 87.f, 7.f, 11.f, "Bottle");
		P.Cyl(-6.f, 15.f, 85.5f, 6.f, 8.f, "PlasticWhite");
		P.Box(2.f, J(Seed, 4) * 6.f, 42.f, 22.f, 26.f, 9.f, "Linen");
	}

	/** A lamp on a stand, its head reaching out along +X. */
	void DrawExamLamp(const DCFurnish::FPen& P)
	{
		P.Cyl(0.f, 0.f, 2.f, 30.f, 4.f, "PlasticWhite");
		P.Cyl(0.f, 0.f, 84.f, 3.2f, 164.f, "PlasticWhite");
		P.Rod(FVector(0.f, 0.f, 166.f), FVector(34.f, 0.f, 160.f), 2.6f, "PlasticWhite");
		P.Ball(38.f, 0.f, 154.f, 32.f, 32.f, 13.f, "PlasticWhite");
		P.Cyl(38.f, 0.f, 147.5f, 23.f, 1.f, "LedCool", 0.f, 0.f, false);
	}

	void DrawCross(const DCFurnish::FPen& P, float X, float Y, float Z, float Sz, FName Mat)
	{
		P.Box(X, Y, Z, 1.f, Sz, Sz * 0.3f, Mat, 0.f, 0.f, 0.f, false);
		P.Box(X, Y, Z, 1.2f, Sz * 0.3f, Sz, Mat, 0.f, 0.f, 0.f, false);
	}

	/** Tall, glass-fronted above, against the wall. */
	void DrawCabinet(const DCFurnish::FPen& P, int32 Seed)
	{
		const float W = 96.f, D = 40.f;
		P.Box(D * 0.5f, 0.f, 44.f, D, W, 88.f, "PlasticWhite");
		P.Box(3.f, 0.f, 138.f, 6.f, W, 100.f, "PlasticWhite");
		for (float S : { -1.f, 1.f }) { P.Box(D * 0.5f, S * (W * 0.5f - 1.5f), 138.f, D, 3.f, 100.f, "PlasticWhite"); }
		P.Box(D * 0.5f, 0.f, 188.5f, D + 2.f, W + 2.f, 3.f, "PlasticWhite");
		for (int32 r = 0; r < 3; ++r)
		{
			const float Z = 88.f + r * 33.f;
			if (r > 0) { P.Box(D * 0.5f, 0.f, Z, D - 4.f, W - 6.f, 2.f, "Steel"); }
			for (int32 i = 0; i < 5; ++i)
			{
				if (U(Seed, 10 + r * 7 + i) < 0.2f) { continue; }
				const float Hh = 12.f + U(Seed, 40 + r * 7 + i) * 10.f;
				const FName M = U(Seed, 70 + r * 7 + i) > 0.5f ? FName("Bottle") : (i % 2 ? FName("PlasticWhite") : FName("PaintRed"));
				P.Cyl(22.f + J(Seed, 90 + i + r) * 5.f, -36.f + i * 18.f, Z + 1.f + Hh * 0.5f, 9.f, Hh, M);
			}
		}
		P.Box(D + 0.4f, 0.f, 138.f, 0.8f, W - 6.f, 96.f, "Glass", 0.f, 0.f, 0.f, false);
		P.Box(D + 0.5f, 0.f, 44.f, 1.f, 1.5f, 80.f, "Steel", 0.f, 0.f, 0.f, false);
		DrawCross(P, D + 0.6f, -24.f, 52.f, 22.f, "CrossGreen");
	}

	/** The cabinet where no tall wall stands: a low chest. */
	void DrawChest(const DCFurnish::FPen& P, int32 Seed)
	{
		P.Box(23.f, 0.f, 39.f, 44.f, 92.f, 78.f, "PlasticWhite");
		P.Box(23.f, 0.f, 79.5f, 47.f, 95.f, 3.f, "Steel");
		for (int32 r = 0; r < 3; ++r) { P.Box(45.4f, 0.f, 16.f + r * 24.f, 1.f, 30.f, 3.f, "Chrome", 0.f, 0.f, 0.f, false); }
		DrawCross(P, 45.5f, 30.f, 58.f, 18.f, "CrossGreen");
		P.Box(20.f, -22.f + J(Seed, 5) * 6.f, 84.f, 24.f, 34.f, 6.f, "PlasticWhite", J(Seed, 6) * 10.f);
	}

	void DrawCounter(const DCFurnish::FPen& P, int32 Seed)
	{
		P.Box(27.f, 0.f, 39.f, 52.f, 116.f, 78.f, "Steel");
		P.Box(28.f, 0.f, 80.f, 58.f, 120.f, 4.f, "PlasticWhite");
		P.Box(28.f, -28.f, 82.3f, 34.f, 40.f, 1.f, "Black", 0.f, 0.f, 0.f, false); // the basin
		P.Cyl(8.f, -28.f, 89.f, 3.f, 14.f, "Chrome");
		P.Rod(FVector(8.f, -28.f, 95.f), FVector(20.f, -28.f, 93.f), 2.4f, "Chrome", false);
		P.Box(53.4f, 0.f, 40.f, 1.f, 1.5f, 70.f, "Black", 0.f, 0.f, 0.f, false);
		for (int32 i = 0; i < 3; ++i)
		{
			const float Hh = 12.f + U(Seed, 20 + i) * 8.f;
			P.Cyl(14.f + J(Seed, 30 + i) * 4.f, 18.f + i * 13.f, 82.f + Hh * 0.5f, 8.f, Hh, i == 1 ? FName("PlasticWhite") : FName("Bottle"));
		}
		P.Box(38.f, 30.f, 86.f, 20.f, 26.f, 8.f, "Linen", J(Seed, 7) * 12.f);
	}

	void DrawStool(const DCFurnish::FPen& P)
	{
		P.Cyl(0.f, 0.f, 2.f, 28.f, 3.f, "Chrome");
		P.Cyl(0.f, 0.f, 23.f, 3.5f, 42.f, "Chrome");
		P.Cyl(0.f, 0.f, 46.f, 30.f, 6.f, "Black");
	}

	void DrawBench(const DCFurnish::FPen& P, float W)
	{
		for (float S : { -1.f, 1.f })
		{
			P.Box(8.f, S * (W * 0.5f - 6.f), 40.f, 4.f, 4.f, 80.f, "Steel");
			P.Box(38.f, S * (W * 0.5f - 6.f), 21.f, 4.f, 4.f, 42.f, "Steel");
		}
		P.Box(24.f, 0.f, 44.f, 40.f, W, 5.f, "Wood");
		P.Box(7.f, 0.f, 70.f, 3.f, W, 20.f, "Wood", 0.f, -6.f);
	}

	/** Three cloth panels on a frame, zigzag, running along Y. */
	void DrawScreen(const DCFurnish::FPen& P, float W, float Top)
	{
		const float Pw = W / 3.f;
		for (int32 i = 0; i < 3; ++i)
		{
			const float Y = (i - 1) * Pw * 0.96f;
			P.Box(0.f, Y, Top * 0.5f + 8.f, 1.6f, Pw - 2.f, Top - 22.f, "BlanketGrey", (i % 2) ? 12.f : -12.f);
		}
		for (int32 i = 0; i < 4; ++i)
		{
			const float Y = (i - 1.5f) * Pw * 0.96f, X = (i % 2) ? -5.f : 5.f;
			P.Cyl(X, Y, Top * 0.5f, 2.6f, Top, "Alu");
		}
	}

	/** A monitor on a pole; the screen faces +X. */
	void DrawMonitor(const DCFurnish::FPen& P)
	{
		P.Cyl(0.f, 0.f, 1.5f, 30.f, 3.f, "Chrome");
		P.Cyl(0.f, 0.f, 60.f, 3.f, 118.f, "Chrome");
		P.Box(0.f, 0.f, 128.f, 9.f, 36.f, 28.f, "PlasticWhite");
		P.Box(4.8f, 0.f, 128.f, 0.5f, 31.f, 23.f, "Screen1", 0.f, 0.f, 0.f, false);
	}

	void DrawOxygen(const DCFurnish::FPen& P)
	{
		P.Cyl(0.f, 0.f, 58.f, 21.f, 116.f, "PaintGreen");
		P.Ball(0.f, 0.f, 116.f, 21.f, 21.f, 14.f, "PaintGreen");
		P.Cyl(0.f, 0.f, 127.f, 7.f, 10.f, "Chrome");
		P.Cyl(7.f, 0.f, 130.f, 9.f, 3.f, "Black", 90.f);
	}

	/** A glass-doored fridge, Top high. */
	void DrawFridge(const DCFurnish::FPen& P, float Top)
	{
		P.Box(31.f, 0.f, Top * 0.5f, 62.f, 72.f, Top, "PlasticWhite");
		P.Box(62.4f, 0.f, Top * 0.56f, 0.8f, 60.f, Top * 0.7f, "Glass", 0.f, 0.f, 0.f, false);
		const int32 Rows = Top > 120.f ? 3 : 1;
		for (int32 r = 0; r < Rows; ++r)
		{
			for (int32 i = 0; i < 4; ++i) { P.Box(62.f, -22.f + i * 15.f, Top * 0.34f + r * Top * 0.2f, 0.5f, 10.f, 15.f, "Pin", 0.f, 0.f, 0.f, false); }
		}
		P.Box(62.6f, 31.f, Top * 0.56f, 1.6f, 2.f, 26.f, "Chrome", 0.f, 0.f, 0.f, false);
	}

	// on the wall (origin on the wall's inner face, Z: centre height)
	void DrawSign(const DCFurnish::FPen& P, float Z)
	{
		P.Box(1.f, 0.f, Z, 2.f, 46.f, 46.f, "PlasticWhite", 0.f, 0.f, 0.f, false);
		DrawCross(P, 2.2f, 0.f, Z, 34.f, "CrossGreen");
	}
	void DrawAidBox(const DCFurnish::FPen& P, float Z)
	{
		P.Box(5.f, 0.f, Z, 10.f, 34.f, 28.f, "PlasticWhite");
		DrawCross(P, 10.3f, 0.f, Z, 14.f, "PaintRed");
	}
	void DrawEyeChart(const DCFurnish::FPen& P, float Z)
	{
		P.Box(0.8f, 0.f, Z, 1.6f, 38.f, 58.f, "Paper", 0.f, 0.f, 0.f, false);
		for (int32 r = 0; r < 6; ++r)
		{
			const float Hh = 7.f - r * 0.9f, Ww = 9.f + r * 4.f;
			P.Box(1.8f, 0.f, Z + 22.f - r * 8.6f, 0.4f, Ww, Hh * 0.6f, "Black", 0.f, 0.f, 0.f, false);
		}
	}
}

void ADCHideoutBuilder::FurnishClinic(DCFurnish::FCtx& Ctx)
{
	using namespace DCFurnishClinic;
	DCFurnish::FPlan& Plan = Ctx.Plan;
	const int32 Seed = Ctx.Seed;
	// a use spot at (X, Y) in F's frame, facing F.Yaw + YawOff
	auto SpotAt = [&](const DCFurnish::FFrame& F, float X, float Y, float YawOff)
	{
		const FV2 Pt = F.O + Rot(FV2(X, Y), F.Yaw);
		if (Plan.Inside(Pt, 25.f)) { Ctx.Spots.Add({ Pt, F.Yaw + YawOff }); }
	};

	Ctx.DefaultLamp();

	// ---------------------------------------------------------------- 1. the treatment bed, head to a wall
	const float BedL = 200.f, BedW = 78.f;
	const TArray<DCFurnish::FLocal> BedLs = { DCFurnish::FLocal::Solid(BedL * 0.5f + 2.f, 0.f, BedL * 0.5f, BedW * 0.5f, 84.f) };
	auto BedScore = [&](const DCFurnish::FFrame& F)
	{
		// a third of the way along, so the rest of the wall stays useful
		const DCFurnish::FEdge& Ed = Plan.E[F.Edge];
		const float Off = FMath::Min(FMath::Abs(F.T - Ed.L * 0.32f), FMath::Abs(F.T - Ed.L * 0.68f));
		return (float)((Ed.Kind == EKind::Wall ? 14.f : 0.f) - Off * 0.05f + Ctx.FacingCam(F) * 8.f + Ctx.Jit(F.Edge) * 4.f);
	};
	DCFurnish::FFrame Bed = Plan.Best(BedLs, Ctx.WallPartF, BedScore, 6.f, TEXT("bed"));
	if (!Bed.bValid) { Bed = Plan.Best(BedLs, Ctx.AnyWallF, BedScore, 6.f, TEXT("bed any")); }
	if (!Bed.bValid)
	{
		// a room of doorways: the bed stands free towards the back, its foot to the camera
		TArray<DCFurnish::FFrame> Fr;
		for (const DCFurnish::FFrame& F0 : Ctx.FreeF)
		{
			for (float Y : { YawOf(KToCam), Plan.RoomYaw, Plan.RoomYaw + 90.f, Plan.RoomYaw + 180.f, Plan.RoomYaw - 90.f }) { DCFurnish::FFrame F = F0; F.Yaw = Y; Fr.Add(F); }
		}
		Bed = Plan.Best(BedLs, Fr, [&](const DCFurnish::FFrame& F)
		{
			const FV2 Mid = F.O + Dir(F.Yaw) * (BedL * 0.5f);
			return (float)(-FV2::Distance(Mid, Plan.Hub - KToCam * 130.f) * 0.3f + Ctx.FacingCam(F) * 8.f);
		}, 6.f, TEXT("bed free"));
	}
	float Access = 0.f; // the side kept clear to stand at (+1 / -1 along the bed's Y)
	if (Bed.bValid)
	{
		Ctx.Place(TEXT("bed"), BedLs, Bed, [BedL, BedW](const DCFurnish::FPen& P, int32 S) { DrawBed(P.At(2.f, 0.f), BedL, BedW, S, false); });
		const float First = FV2::DotProduct(Rot(FV2(0.f, 1.f), Bed.Yaw), KToCam) > 0.f ? 1.f : -1.f; // the camera's side first
		for (float S : { First, -First })
		{
			const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Space(108.f, S * (BedW * 0.5f + 25.f), 62.f, 22.f, 60.f) };
			if (!Plan.Fits(Ls, Bed, 2.f)) { continue; }
			Plan.Commit(Ls, Bed);
			Access = S;
			SpotAt(Bed, 80.f, S * (BedW * 0.5f + 27.f), -90.f * S);
			SpotAt(Bed, 150.f, S * (BedW * 0.5f + 27.f), -90.f * S);
			break;
		}
	}

	// Things that stand around the bed: the best free place out of a few (X, Y, preference) in the bed's frame.
	auto Beside = [&](const TCHAR* Name, const FV2& Half, float Z1, std::initializer_list<FVector> Where, bool bFaceBed, DCFurnish::FDrawFn Draw, FName Group = NAME_None) -> DCFurnish::FFrame
	{
		if (!Bed.bValid) { return DCFurnish::FFrame(); }
		TArray<DCFurnish::FFrame> Fr;
		for (const FVector& W : Where)
		{
			for (float S : { 1.f, -1.f })
			{
				DCFurnish::FFrame F = Bed; F.O = Bed.O + Rot(FV2(W.X, W.Y * S), Bed.Yaw); F.Edge = -1;
				F.Yaw = Bed.Yaw + (bFaceBed ? -90.f * S : 0.f);
				F.T = (float)W.Z + (S == Access ? -6.f : 0.f); // leave the standing side alone if we can
				Fr.Add(F);
			}
		}
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(0.f, 0.f, Half.X, Half.Y, Z1) };
		const DCFurnish::FFrame F = Plan.Best(Ls, Fr, [](const DCFurnish::FFrame& Cand) { return Cand.T; }, 2.f, Name);
		if (F.bValid) { Ctx.Place(Name, Ls, F, MoveTemp(Draw), Group); }
		return F;
	};
	const float Side = BedW * 0.5f;
	if (!Beside(TEXT("drip"), FV2(13.f, 13.f), 192.f, { FVector(20.f, Side + 16.f, 4.f), FVector(60.f, Side + 18.f, 0.f) }, true,
		[](const DCFurnish::FPen& P, int32) { DrawDrip(P, 190.f); }).bValid)
	{
		// under a partition's height band: a shorter stand
		Beside(TEXT("drip"), FV2(13.f, 13.f), 146.f, { FVector(20.f, Side + 16.f, 4.f), FVector(60.f, Side + 18.f, 0.f) }, true, [](const DCFurnish::FPen& P, int32) { DrawDrip(P, 144.f); });
	}
	Beside(TEXT("trolley"), FV2(26.f, 20.f), 84.f, { FVector(110.f, Side + 31.f, 4.f), FVector(160.f, Side + 31.f, 3.f), FVector(60.f, Side + 31.f, 2.f), FVector(BedL + 30.f, Side * 0.4f, 1.f), FVector(110.f, Side + 46.f, 0.f) }, true,
		[](const DCFurnish::FPen& P, int32 S) { DrawTrolley(P.At(0.f, 0.f, 90.f), S); });
	Beside(TEXT("lamp"), FV2(15.f, 15.f), 172.f, { FVector(64.f, Side + 20.f, 4.f), FVector(150.f, Side + 20.f, 2.f), FVector(24.f, Side + 20.f, 0.f) }, true,
		[this](const DCFurnish::FPen& P, int32)
		{
			DrawExamLamp(P);
			Point(P.P(40.f, 0.f, 136.f), FLinearColor(0.86f, 0.94f, 1.f), 10.f, 380.f, false, true, 6.f);
		});

	// ---------------------------------------------------------------- 2. the medicine cabinet (a low chest without a tall wall)
	auto Tucked = [&](const DCFurnish::FFrame& F) { return -Plan.CornerDist(F.O) * 0.04f - FMath::Min(Plan.NearestSolid(F.O), 200.f) * 0.08f; };
	DCFurnish::FFrame Cab;
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(21.f, 0.f, 21.f, 50.f, 192.f) };
		Cab = Ctx.BestSoft(Ls, Ctx.WallF, [&](const DCFurnish::FFrame& F) { return (float)(Tucked(F) + Ctx.FacingCam(F) * 6.f + Ctx.Jit(20 + F.Edge) * 4.f); }, 5.f, 30.f);
		if (Cab.bValid) { Ctx.PlaceSoft(TEXT("cabinet"), Ls, Cab, [](const DCFurnish::FPen& P, int32 S) { DrawCabinet(P, S); }); }
		else
		{
			const TArray<DCFurnish::FLocal> Low = { DCFurnish::FLocal::Solid(24.f, 0.f, 24.f, 49.f, 84.f) };
			auto Score = [&](const DCFurnish::FFrame& F) { return (float)(Tucked(F) + Ctx.FacingCam(F) * 8.f + Ctx.Jit(20 + F.Edge) * 4.f); };
			Cab = Plan.Best(Low, Ctx.WallPartF, Score, 5.f);
			if (!Cab.bValid) { Cab = Plan.Best(Low, Ctx.AnyWallF, Score, 5.f, TEXT("chest")); }
			if (Cab.bValid) { Ctx.Place(TEXT("chest"), Low, Cab, [](const DCFurnish::FPen& P, int32 S) { DrawChest(P, S); }); }
		}
		if (Cab.bValid) { Ctx.Spot(Cab, 74.f, 0.f); }
	}

	// ---------------------------------------------------------------- 3. the counter with its basin, and a stool
	DCFurnish::FFrame Counter;
	{
		TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(29.f, 0.f, 29.f, 61.f, 84.f), DCFurnish::FLocal::Space(76.f, 0.f, 18.f, 34.f, 60.f) };
		Ls[1].Tag = TagSeat;
		auto Score = [&](const DCFurnish::FFrame& F)
		{
			return (float)((Plan.InWindow(F.Edge, F.T, 60.f) ? 10.f : 0.f) + (F.Edge == Bed.Edge ? -8.f : 0.f) + (F.Edge == Cab.Edge ? 6.f : 0.f)
				+ Tucked(F) * 0.5f + Ctx.FacingCam(F) * 5.f + Ctx.Jit(30 + F.Edge) * 4.f);
		};
		Counter = Plan.Best(Ls, Ctx.WallPartF, Score, 5.f);
		if (!Counter.bValid) { Counter = Plan.Best(Ls, Ctx.AnyWallF, Score, 5.f, TEXT("counter")); }
		if (Counter.bValid)
		{
			Ctx.Place(TEXT("counter"), Ls, Counter, [](const DCFurnish::FPen& P, int32 S) { DrawCounter(P, S); });
			Ctx.Spot(Counter, 80.f, -26.f);
			TArray<DCFurnish::FLocal> St = { DCFurnish::FLocal::Solid(0.f, 0.f, 15.f, 15.f, 50.f) };
			St[0].Tag = TagSeat;
			Plan.CapScale = 0.6f; Plan.bHubKeep = false; // a stool may stand in the way a little
			for (const FV2& At : { FV2(78.f, 24.f), FV2(76.f, -40.f), FV2(70.f, 64.f), FV2(70.f, -76.f), FV2(104.f, 0.f) })
			{
				DCFurnish::FFrame F = Counter; F.O = Counter.O + Rot(FV2(At.X + U(Seed, 600) * 8.f, At.Y), Counter.Yaw); F.Yaw += J(Seed, 601) * 30.f; F.Edge = -1;
				if (!Plan.Fits(St, F, 3.f)) { continue; }
				Ctx.Place(TEXT("stool"), St, F, [](const DCFurnish::FPen& P, int32) { DrawStool(P); });
				break;
			}
			Plan.CapScale = 1.f; Plan.bHubKeep = true;
			Plan.Parts.RemoveAll([](const DCFurnish::FPart& Pt) { return Pt.Tag == TagSeat && !Pt.bSolid; });
		}
	}

	// ---------------------------------------------------------------- 4. a second cot, side-on to a wall
	DCFurnish::FFrame Cot;
	if (Plan.Coverage() < 0.36f)
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(40.f, 0.f, 38.f, 97.f, 82.f), DCFurnish::FLocal::Space(96.f, 0.f, 16.f, 50.f, 60.f) };
		auto Score = [&](const DCFurnish::FFrame& F)
		{
			return (float)((F.Edge == Bed.Edge ? -6.f : 0.f) + (Plan.E[F.Edge].bBack ? 8.f : 0.f) + Tucked(F) + Ctx.Jit(40 + F.Edge) * 4.f);
		};
		Cot = Plan.Best(Ls, Ctx.WallPartF, Score, 6.f);
		if (!Cot.bValid) { Cot = Plan.Best(Ls, Ctx.AnyWallF, Score, 6.f, TEXT("cot")); }
		if (Cot.bValid)
		{
			const float Flip = U(Seed, 610) > 0.5f ? 1.f : -1.f;
			Ctx.Place(TEXT("cot"), Ls, Cot, [Flip](const DCFurnish::FPen& P, int32 S) { DrawBed(P.At(41.f, -95.f * Flip, 90.f * Flip), 190.f, 72.f, S, true); });
			Ctx.Spot(Cot, 100.f, 0.f);
		}
	}

	// ---------------------------------------------------------------- 5. a bench to wait on
	{
		const float W = 112.f;
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(23.f, 0.f, 23.f, W * 0.5f + 2.f, 84.f), DCFurnish::FLocal::Space(62.f, 0.f, 14.f, W * 0.4f, 50.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, Ctx.AnyWallF, [&](const DCFurnish::FFrame& Cand)
		{
			return (float)(Ctx.FacingHub(Cand) * 6.f + Ctx.FacingCam(Cand) * 8.f + (Cand.Edge == Bed.Edge ? -6.f : 0.f) + Tucked(Cand) * 0.5f + Ctx.Jit(50 + Cand.Edge) * 4.f);
		}, 6.f, TEXT("bench"));
		if (F.bValid) { Ctx.Place(TEXT("bench"), Ls, F, [W](const DCFurnish::FPen& P, int32) { DrawBench(P, W); }); }
		// a chair to wait on: beside the bench if it can, else against any wall
		const TArray<DCFurnish::FLocal> CL = { DCFurnish::FLocal::Solid(28.f, 0.f, 25.f, 25.f, 84.f) };
		for (int32 k = 0; k < 2; ++k)
		{
			const DCFurnish::FFrame C = Plan.Best(CL, Ctx.AnyWallF, [&](const DCFurnish::FFrame& Cand)
			{
				const float Near = F.bValid ? -FMath::Abs(FV2::Distance(Cand.O, F.O) - 100.f) * 0.06f : 0.f;
				return (float)(Near + Ctx.FacingCam(Cand) * 4.f + Tucked(Cand) * 0.3f + Ctx.Jit(55 + k * 5 + Cand.Edge) * 3.f);
			}, 5.f, TEXT("chair"));
			const float Turn = J(Seed, 620 + k) * 14.f;
			if (C.bValid) { Ctx.Place(TEXT("chair"), CL, C, [Turn](const DCFurnish::FPen& P, int32 S) { DrawWoodChair(P.At(28.f, 0.f, Turn), S); }); }
		}
	}

	// ---------------------------------------------------------------- 6. a folding screen on the bed's far side
	if (Bed.bValid)
	{
		TArray<DCFurnish::FFrame> Fr;
		for (float S : { 1.f, -1.f })
		{
			if (FV2::DotProduct(Rot(FV2(0.f, S), Bed.Yaw), KToCam) > 0.2f) { continue; } // never between the camera and the bed
			for (float X : { 112.f, 80.f, 150.f, 50.f })
			{
				for (float D : { 40.f, 56.f, 74.f, 96.f })
				{
					DCFurnish::FFrame F = Bed; F.O = Bed.O + Rot(FV2(X, S * (Side + D)), Bed.Yaw); F.Yaw = Bed.Yaw + 90.f; F.Edge = -1; F.T = -D * 0.2f - FMath::Abs(X - 112.f) * 0.1f;
					Fr.Add(F);
				}
			}
		}
		bool bScreen = false;
		for (float Top : { 148.f, 84.f })
		{
			const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(0.f, 0.f, 9.f, 62.f, Top) };
			const DCFurnish::FFrame F = Plan.Best(Ls, Fr, [](const DCFurnish::FFrame& Cand) { return Cand.T; }, 3.f, TEXT("screen"));
			if (!F.bValid) { continue; }
			Ctx.Place(TEXT("screen"), Ls, F, [Top](const DCFurnish::FPen& P, int32) { DrawScreen(P, 120.f, Top - 2.f); });
			bScreen = true;
			break;
		}
		if (!bScreen)
		{
			// folded back against a wall or partition near the bed
			const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(16.f, 0.f, 10.f, 62.f, 148.f) };
			const FV2 BedMid = Bed.O + Rot(FV2(BedL * 0.5f, 0.f), Bed.Yaw);
			const DCFurnish::FFrame F = Ctx.BestSoft(Ls, Ctx.WallPartF, [&](const DCFurnish::FFrame& Cand) { return (float)(-FV2::Distance(Cand.O, BedMid) * 0.05f); }, 4.f, 20.f);
			if (F.bValid) { Ctx.PlaceSoft(TEXT("screen"), Ls, F, [](const DCFurnish::FPen& P, int32) { DrawScreen(P.At(16.f, 0.f), 120.f, 146.f); }); }
			else
			{
				// standing free on the floor, a little way off the bed
				TArray<DCFurnish::FFrame> Fr2;
				for (const DCFurnish::FFrame& F0 : Ctx.FreeF)
				{
					for (float Y : { Plan.RoomYaw, Plan.RoomYaw + 90.f, Plan.RoomYaw + 45.f, Plan.RoomYaw - 45.f }) { DCFurnish::FFrame Fy = F0; Fy.Yaw = Y; Fr2.Add(Fy); }
				}
				const TArray<DCFurnish::FLocal> FreeLs = { DCFurnish::FLocal::Solid(0.f, 0.f, 9.f, 54.f, 140.f) };
				const DCFurnish::FFrame G = Plan.Best(FreeLs, Fr2, [&](const DCFurnish::FFrame& Cand)
				{
					return (float)(-FMath::Abs(FV2::Distance(Cand.O, BedMid) - 140.f) * 0.05f - FV2::Distance(Cand.O, Plan.Hub - KToCam * 150.f) * 0.02f);
				}, 4.f, TEXT("screen free"));
				if (G.bValid) { Ctx.Place(TEXT("screen"), FreeLs, G, [](const DCFurnish::FPen& P, int32) { DrawScreen(P, 105.f, 138.f); }); }
			}
		}
	}

	Ctx.Runner();

	// ---------------------------------------------------------------- 7. upgrades: a monitor and oxygen by the bed, a fridge
	Beside(TEXT("monitor"), FV2(15.f, 18.f), 146.f, { FVector(24.f, Side + 20.f, 4.f), FVector(70.f, Side + 22.f, 2.f), FVector(150.f, Side + 22.f, 0.f) }, false,
		[](const DCFurnish::FPen& P, int32) { DrawMonitor(P); }, DCGroups::Level2);
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(15.f, 0.f, 13.f, 13.f, 134.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, Ctx.WallPartF, [&](const DCFurnish::FFrame& Cand) { return (float)(-FV2::Distance(Cand.O, Bed.bValid ? Bed.O : Plan.Hub) * 0.1f + Tucked(Cand)); }, 4.f);
		if (F.bValid) { Ctx.Place(TEXT("oxygen"), Ls, F, [](const DCFurnish::FPen& P, int32) { DrawOxygen(P.At(15.f, 0.f)); }, DCGroups::Level2); }
	}
	{
		// tall if a wall has room without costing a window, else a low one
		for (float Top : { 176.f, 84.f })
		{
			const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(32.f, 0.f, 32.f, 38.f, Top) };
			auto Score = [&](const DCFurnish::FFrame& Cand) { return (float)(Tucked(Cand) + Ctx.FacingCam(Cand) * 6.f + Ctx.Jit(60 + Cand.Edge) * 4.f); };
			DCFurnish::FFrame F = Plan.Best(Ls, Top > 100.f ? Ctx.WallF : Ctx.WallPartF, Score, 5.f);
			if (!F.bValid && Top < 100.f) { F = Plan.Best(Ls, Ctx.AnyWallF, Score, 5.f, TEXT("fridge")); }
			if (!F.bValid) { continue; }
			Ctx.Place(TEXT("fridge"), Ls, F, [Top](const DCFurnish::FPen& P, int32) { DrawFridge(P, Top - 2.f); }, DCGroups::Level3);
			break;
		}
	}

	// ---------------------------------------------------------------- 8. fillers, then the walls
	Ctx.Clutter(0.38f);
	{
		struct FDecor { const TCHAR* Name; float W, Z0, Z1, D; };
		const FDecor Decor[] = { { TEXT("sign"), 50.f, 170.f, 222.f, 4.f }, { TEXT("aidbox"), 38.f, 128.f, 162.f, 12.f }, { TEXT("chart"), 42.f, 118.f, 182.f, 4.f } };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Decor); ++i)
		{
			const FDecor& Dc = Decor[i];
			TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(Dc.D * 0.5f, 0.f, Dc.D * 0.5f, Dc.W * 0.5f, Dc.Z1, Dc.Z0) };
			Ls[0].bSolid = false;
			const DCFurnish::FFrame F = Plan.Best(Ls, Ctx.WallF, [&](const DCFurnish::FFrame& Cand)
			{
				const float Near = i == 0 && Cab.bValid ? -FV2::Distance(Cand.O, Cab.O) * 0.02f : 0.f;
				return (float)(Near + Ctx.FacingCam(Cand) * 4.f + J(Seed, 900 + i * 7 + Cand.Edge) * 6.f - FMath::Abs(Cand.T - Plan.E[Cand.Edge].L * 0.5f) * 0.01f);
			}, 18.f);
			if (!F.bValid) { continue; }
			const float Z = (Dc.Z0 + Dc.Z1) * 0.5f;
			switch (i)
			{
			case 0: Ctx.Place(Dc.Name, Ls, F, [Z](const DCFurnish::FPen& P, int32) { DrawSign(P, Z); }); break;
			case 1: Ctx.Place(Dc.Name, Ls, F, [Z](const DCFurnish::FPen& P, int32) { DrawAidBox(P, Z); }); break;
			default: Ctx.Place(Dc.Name, Ls, F, [Z](const DCFurnish::FPen& P, int32) { DrawEyeChart(P, Z); }); break;
			}
		}
	}
}
