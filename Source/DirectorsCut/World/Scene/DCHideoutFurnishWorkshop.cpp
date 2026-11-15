// Director's Cut — the Workshop's furnishing recipe (see DCHideoutFurnish.cpp): the long workbench under its tool
// wall, a sawhorse table with a job on it, tool storage, a drill press, a gear rack and drums. Without a tall wall the
// bench backs onto a partition or stands free, and the tall pieces come in low versions.
#include "World/Scene/DCHideoutFurnish.h"
#include "Components/PointLightComponent.h"

using namespace DCFurnish;

namespace DCFurnishWorkshop
{
	using FPen = DCFurnish::FPen;

	constexpr float KBenchD = 70.f;   // bench depth
	constexpr float KBenchTop = 91.f;

	/** The workbench: origin at its back edge (the wall face), W wide. PegTop > 0 adds the tool wall up to there. */
	void DrawBench(const FPen& P, float W, float PegTop, int32 Seed)
	{
		const float HW = W * 0.5f, Mid = KBenchD * 0.5f + 2.f;
		P.Box(Mid, 0.f, 88.f, KBenchD, W, 6.f, "Plywood");
		P.Box(Mid + KBenchD * 0.5f + 0.5f, 0.f, 86.f, 2.f, W + 2.f, 8.f, "SteelBlack");
		for (int32 i = 0; i < 4; ++i) { P.Box((i % 2) ? Mid + 30.f : Mid - 30.f, (i / 2) ? HW - 8.f : -HW + 8.f, 42.5f, 6.f, 6.f, 85.f, "SteelBlack"); }
		P.Box(Mid, 0.f, 22.f, KBenchD - 8.f, W - 14.f, 3.f, "Plywood");
		// drawer unit
		const float DY = W * 0.27f;
		P.Box(Mid + 1.f, DY, 55.f, KBenchD - 8.f, 62.f, 58.f, "PaintRed");
		for (int32 i = 0; i < 4; ++i) { P.Box(Mid + KBenchD * 0.5f - 2.6f, DY, 34.f + i * 13.f, 1.f, 52.f, 1.6f, "Alu"); }
		// a box of parts underneath
		P.Box(Mid, -W * 0.26f, 36.f, 40.f, 48.f, 24.f, "Cardboard", J(Seed, 1) * 8.f);
		// the vice
		const float VY = -HW + 26.f;
		P.Box(Mid + 20.f, VY, KBenchTop + 6.f, 18.f, 20.f, 12.f, "CastIron");
		P.Box(Mid + 31.f, VY, KBenchTop + 10.f, 4.f, 24.f, 9.f, "Steel");
		P.Rod(FVector(Mid + 35.f, VY - 12.f, KBenchTop + 4.f), FVector(Mid + 35.f, VY + 12.f, KBenchTop + 9.f), 1.6f, "Chrome");
		// an open gear case, ammo tins
		const float CY = -W * 0.1f, Skew = 5.f + J(Seed, 2) * 4.f;
		P.Box(Mid + 4.f, CY, KBenchTop + 4.f, 34.f, 88.f, 8.f, "PlasticBlack", Skew);
		P.Box(Mid + 4.f, CY, KBenchTop + 8.4f, 30.f, 82.f, 1.f, "Rubber", Skew);
		P.Box(Mid + 4.f, CY, KBenchTop + 10.5f, 5.f, 68.f, 4.f, "SteelBlack", Skew);
		P.Box(Mid + 6.f, CY - 22.f, KBenchTop + 11.f, 8.f, 20.f, 6.f, "SteelBlack", Skew);
		P.Box(Mid - 15.f, CY, KBenchTop + 21.f, 3.f, 88.f, 32.f, "PlasticBlack", Skew, 14.f);
		for (int32 i = 0; i < 2; ++i) { P.Box(Mid - 4.f + i * 6.f, W * 0.1f + i * 24.f, KBenchTop + 9.f, 26.f, 14.f, 18.f, "PaintOlive", J(Seed, 3 + i) * 10.f); }
		// the task lamp
		const float LY = HW - 18.f;
		P.Cyl(12.f, LY, KBenchTop + 1.5f, 13.f, 3.f, "SteelBlack");
		P.Rod(FVector(12.f, LY, KBenchTop + 2.f), FVector(14.f, LY - 2.f, KBenchTop + 30.f), 1.6f, "SteelBlack");
		P.Rod(FVector(14.f, LY - 2.f, KBenchTop + 30.f), FVector(30.f, LY - 14.f, KBenchTop + 40.f), 1.6f, "SteelBlack");
		P.Box(33.f, LY - 16.f, KBenchTop + 39.f, 14.f, 11.f, 7.f, "PaintOrange", -35.f);
		P.Box(33.f, LY - 16.f, KBenchTop + 35.f, 10.f, 8.f, 0.6f, "Tungsten", -35.f, 0.f, 0.f, false);
		if (PegTop <= 0.f) { return; }
		// the tool wall
		const float Z0 = 106.f, PH = PegTop - Z0;
		P.Box(2.f, 0.f, Z0 + PH * 0.5f, 3.f, W, PH, "PaintGrey");
		P.Box(2.5f, 0.f, PegTop + 2.f, 5.f, W + 6.f, 5.f, "SteelBlack");
		P.Box(2.5f, 0.f, Z0 - 2.f, 5.f, W + 6.f, 4.f, "SteelBlack");
		const float TZ = Z0 + 14.f;
		for (int32 i = 0; i < 5; ++i) { const float Y = -HW + 18.f + i * 12.f; P.Rod(FVector(5.f, Y, TZ), FVector(5.f, Y, TZ + 24.f + i * 5.f), 2.2f, "Chrome"); }  // spanners
		for (int32 i = 0; i < 3; ++i)
		{
			const float Y = -HW * 0.22f + i * 15.f;
			P.Rod(FVector(5.f, Y, TZ), FVector(5.f, Y, TZ + 26.f), 2.4f, "PaintRed");
			P.Box(5.f, Y, TZ + 30.f, 3.f, 10.f, 4.f, "SteelBlack");
		}
		P.Box(4.6f, HW * 0.36f, TZ + 14.f, 1.f, 52.f, 16.f, "Steel", 0.f, 0.f, 9.f);     // a saw
		P.Box(5.f, HW * 0.36f - 30.f, TZ + 12.f, 3.f, 12.f, 14.f, "Timber", 0.f, 0.f, 9.f);
		P.Rod(FVector(5.f, HW - 14.f, TZ - 4.f), FVector(5.f, HW - 22.f, FMath::Min(TZ + 70.f, PegTop - 6.f)), 2.6f, "PaintRed"); // a crowbar
		if (PH > 76.f)
		{
			// the cordless rack
			const float RZ = PegTop - 22.f;
			P.Box(5.f, -HW * 0.45f, RZ, 5.f, 66.f, 26.f, "PlasticBlack");
			for (int32 i = 0; i < 3; ++i) { P.Box(9.f, -HW * 0.45f - 21.f + i * 21.f, RZ + 3.f, 7.f, 10.f, 19.f, i == 1 ? FName("PaintOrange") : FName("PaintRed")); }
		}
	}

	/** Open steel shelving with gear cases: origin on the wall, 104 wide, TopZ tall. */
	void DrawShelving(const FPen& P, float TopZ, int32 Seed)
	{
		const float D = 42.f, W = 104.f;
		for (int32 i = 0; i < 4; ++i) { P.Box((i % 2) ? D - 2.f : 4.f, (i / 2) ? W * 0.5f - 2.5f : -W * 0.5f + 2.5f, TopZ * 0.5f, 4.f, 4.f, TopZ, "SteelBlack"); }
		const int32 N = FMath::Max(2, FMath::FloorToInt((TopZ - 12.f) / 46.f) + 1);
		static const FName Cases[] = { "PlasticBlack", "PaintOlive", "Cardboard", "PaintOrange", "PlasticBlack", "PaintGrey" };
		for (int32 i = 0; i < N; ++i)
		{
			const float Z = 12.f + i * (TopZ - 16.f) / (N - 1);
			P.Box(D * 0.5f + 1.f, 0.f, Z, D - 2.f, W - 2.f, 3.f, "Plywood");
			if (i == N - 1 && TopZ > 150.f) { continue; }
			// two or three things per shelf
			float Y = -W * 0.5f + 8.f;
			for (int32 k = 0; k < 3 && Y < W * 0.5f - 18.f; ++k)
			{
				const float BW = FMath::Min(24.f + U(Seed, i * 7 + k) * 22.f, W * 0.5f - 6.f - Y), BH = 14.f + U(Seed, 40 + i * 5 + k) * 16.f;
				P.Box(D * 0.5f + 1.f + J(Seed, 60 + i * 3 + k) * 2.f, Y + BW * 0.5f, Z + 1.5f + BH * 0.5f, D - 10.f, BW, BH, Cases[(i * 3 + k + (Seed & 3)) % UE_ARRAY_COUNT(Cases)]);
				Y += BW + 5.f;
			}
		}
	}

	/** A rolling tool chest, drawers facing the room. */
	void DrawToolChest(const FPen& P)
	{
		for (float Y : { -30.f, 30.f }) { for (float X : { 10.f, 42.f }) { P.Cyl(X, Y, 5.f, 10.f, 5.f, "Rubber", 90.f, 90.f); } }
		P.Box(26.f, 0.f, 37.f, 44.f, 72.f, 54.f, "PaintRed");
		P.Box(26.f, 0.f, 66.f, 46.f, 74.f, 3.f, "SteelBlack");
		P.Box(25.f, 0.f, 75.f, 38.f, 66.f, 15.f, "PaintRed");
		for (int32 i = 0; i < 4; ++i) { P.Box(48.4f, 0.f, 18.f + i * 12.f, 1.f, 62.f, 1.6f, "Alu"); }
		P.Box(44.4f, 0.f, 75.f, 1.f, 56.f, 1.6f, "Alu");
		P.Rod(FVector(26.f, -39.f, 52.f), FVector(26.f, -39.f, 60.f), 2.f, "Chrome");
	}

	/** Plywood on two sawhorses with a job on it: a strongbox being opened up. W along local Y. */
	void DrawJobTable(const FPen& P, float W, float D, int32 Seed)
	{
		for (float S : { -1.f, 1.f })
		{
			const float Y = S * (W * 0.5f - 22.f);
			P.Box(0.f, Y, 72.f, D - 6.f, 8.f, 8.f, "Timber");
			for (float X : { -1.f, 1.f })
			{
				P.Rod(FVector(X * (D * 0.5f - 10.f), Y - 6.f, 70.f), FVector(X * (D * 0.5f + 2.f), Y - 14.f, 0.f), 5.f, "Timber");
				P.Rod(FVector(X * (D * 0.5f - 10.f), Y + 6.f, 70.f), FVector(X * (D * 0.5f + 2.f), Y + 14.f, 0.f), 5.f, "Timber");
			}
		}
		P.Box(0.f, 0.f, 78.f, D, W, 4.f, "Plywood");
		// the strongbox, its door off beside it, a drill, offcuts
		const float A = J(Seed, 1) * 10.f;
		P.Box(2.f, -W * 0.14f, 80.f + 14.f, 40.f, 46.f, 28.f, "PaintGrey", A);
		P.Box(2.f, -W * 0.14f, 80.f + 14.f, 34.f, 40.f, 28.6f, "Black", A);
		P.Box(-4.f, W * 0.2f, 82.f, 36.f, 40.f, 4.f, "PaintGrey", A - 24.f);
		P.Cyl(-4.f, W * 0.2f, 85.f, 12.f, 2.f, "Chrome");
		P.Box(14.f, W * 0.36f, 86.f, 22.f, 8.f, 12.f, "PaintOrange", 30.f + A);
		P.Box(D * 0.2f, W * 0.05f, 81.f, 3.f, 30.f, 2.f, "Steel", 60.f + A * 2.f);
		P.Box(-D * 0.28f, -W * 0.38f, 81.f, 3.f, 22.f, 2.f, "Steel", -20.f + A);
		P.Box(-D * 0.3f, 0.f, 80.6f, 20.f, 28.f, 0.6f, "Blueprint", 12.f - A);
	}

	void DrawStool(const FPen& P)
	{
		P.Cyl(0.f, 0.f, 58.f, 30.f, 5.f, "Timber");
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector2D D = DCFurnish::Dir(i * 120.f + 20.f);
			P.Rod(FVector(D.X * 8.f, D.Y * 8.f, 56.f), FVector(D.X * 15.f, D.Y * 15.f, 0.f), 2.6f, "SteelBlack");
		}
		P.Cyl(0.f, 0.f, 22.f, 24.f, 1.6f, "SteelBlack");
	}

	/** A bench-top pillar drill on its own stand: origin on the wall, 146 tall. */
	void DrawDrillPress(const FPen& P)
	{
		P.Box(30.f, 0.f, 3.f, 46.f, 40.f, 6.f, "CastIron");
		P.Box(30.f, 0.f, 36.f, 34.f, 34.f, 60.f, "PaintGrey");
		P.Box(30.f, 0.f, 68.f, 44.f, 40.f, 5.f, "SteelBlack");
		P.Cyl(16.f, 0.f, 106.f, 7.f, 74.f, "Chrome");
		P.Box(34.f, 0.f, 92.f, 26.f, 26.f, 3.f, "CastIron");                 // the table
		P.Box(28.f, 0.f, 132.f, 46.f, 22.f, 24.f, "PaintGreen");             // the head
		P.Cyl(8.f, 0.f, 132.f, 20.f, 26.f, "CastIron", 0.f);                 // the motor
		P.Cyl(40.f, 0.f, 114.f, 5.f, 14.f, "Chrome");
		P.Cyl(40.f, 0.f, 103.f, 1.6f, 10.f, "Steel");
		for (int32 i = 0; i < 3; ++i)
		{
			const float R = FMath::DegreesToRadians(i * 120.f + 15.f);
			P.Rod(FVector(34.f, 12.f, 128.f), FVector(34.f + FMath::Cos(R) * 12.f, 16.f, 128.f + FMath::Sin(R) * 12.f), 1.4f, "SteelBlack");
		}
	}

	/** A pipe rail hung with vests and coveralls over a row of bags and boots: origin on the wall, 124 wide. */
	void DrawGearRack(const FPen& P, float TopZ, int32 Seed)
	{
		const float W = 124.f, X = 24.f;
		for (float S : { -1.f, 1.f })
		{
			P.Rod(FVector(X, S * W * 0.5f, 0.f), FVector(X, S * W * 0.5f, TopZ), 3.4f, "SteelBlack");
			P.Box(X, S * W * 0.5f, 2.f, 44.f, 5.f, 4.f, "SteelBlack");
		}
		P.Rod(FVector(X, -W * 0.5f, TopZ - 2.f), FVector(X, W * 0.5f, TopZ - 2.f), 3.4f, "Chrome");
		static const FName Cloth[] = { "PaintOlive", "Black", "Canvas", "NylonOlive", "PaintOrange" };
		const int32 N = 5;
		const float Drop = FMath::Min(TopZ - 34.f, 64.f);
		for (int32 i = 0; i < N; ++i)
		{
			const float Y = -W * 0.5f + 16.f + i * (W - 32.f) / (N - 1), L = Drop * (0.7f + U(Seed, 10 + i) * 0.3f);
			P.Box(X, Y, TopZ - 8.f - L * 0.5f, 26.f, 15.f, L, Cloth[(i + (Seed & 7)) % UE_ARRAY_COUNT(Cloth)], J(Seed, 20 + i) * 8.f);
		}
		P.Box(X, 0.f, 9.f, 40.f, W - 8.f, 3.f, "Plywood");
		for (int32 i = 0; i < 3; ++i) { P.Ball(X + J(Seed, 30 + i) * 3.f, -W * 0.3f + i * W * 0.3f, 19.f, 34.f, 30.f, 18.f, i == 1 ? FName("NylonOlive") : FName("Black")); }
	}

	/** Two oil drums and a pail. */
	void DrawDrums(const FPen& P, int32 Seed)
	{
		for (int32 i = 0; i < 2; ++i)
		{
			const float Y = i ? 31.f : -31.f, X = 32.f + J(Seed, i) * 3.f;
			const FName M = (i == (Seed & 1)) ? FName("PaintBlue") : FName("SteelBlack");
			P.Cyl(X, Y, 41.f, 56.f, 82.f, M);
			P.Cyl(X, Y, 28.f, 57.5f, 2.f, M);
			P.Cyl(X, Y, 56.f, 57.5f, 2.f, M);
			P.Cyl(X + 14.f, Y + 6.f, 82.8f, 8.f, 1.6f, "Chrome");
		}
		P.Cyl(78.f, J(Seed, 5) * 14.f, 14.f, 26.f, 28.f, "PaintYellow");
	}

	/** A welding cart with two gas bottles (Level 2). */
	void DrawWeldingCart(const FPen& P)
	{
		for (float Y : { -26.f, 26.f }) { P.Cyl(40.f, Y, 9.f, 18.f, 5.f, "Rubber", 90.f, 90.f); }
		P.Box(28.f, 0.f, 16.f, 50.f, 60.f, 5.f, "SteelBlack");
		P.Box(34.f, 0.f, 36.f, 34.f, 44.f, 34.f, "PaintOrange");
		P.Box(51.4f, 0.f, 40.f, 1.f, 30.f, 12.f, "PlasticBlack");
		P.Cyl(12.f, -14.f, 80.f, 22.f, 124.f, "PaintRed");
		P.Cyl(12.f, 14.f, 80.f, 22.f, 124.f, "PaintBlue");
		for (float Y : { -14.f, 14.f }) { P.Cyl(12.f, Y, 145.f, 8.f, 6.f, "Brass"); }
		P.Rod(FVector(5.f, -28.f, 20.f), FVector(5.f, -28.f, 120.f), 2.6f, "SteelBlack");
		P.Rod(FVector(5.f, 28.f, 20.f), FVector(5.f, 28.f, 120.f), 2.6f, "SteelBlack");
		P.Rod(FVector(5.f, -28.f, 120.f), FVector(5.f, 28.f, 120.f), 2.6f, "SteelBlack");
		P.Rod(FVector(50.f, 10.f, 54.f), FVector(62.f, 26.f, 4.f), 1.6f, "Black");
	}

	/** Rows of parts bins on the wall (Level 2). */
	void DrawPartsBins(const FPen& P, float W)
	{
		P.Box(1.5f, 0.f, 156.f, 2.f, W, 50.f, "PaintGrey");
		static const FName Bins[] = { "PaintRed", "PaintYellow", "PaintBlue" };
		const int32 N = FMath::Max(3, FMath::FloorToInt(W / 16.f));
		for (int32 r = 0; r < 3; ++r)
		{
			for (int32 i = 0; i < N; ++i) { P.Box(7.f, -W * 0.5f + (i + 0.5f) * W / N, 140.f + r * 16.f, 10.f, W / N - 3.f, 10.f, Bins[r], 0.f, -14.f); }
		}
	}

	/** A small stack of parts bins standing on a bench top (Level 2): origin on the bench's back edge, 40 wide. */
	void DrawBenchBins(const FPen& P)
	{
		const float Z = KBenchTop;
		P.Box(9.f, 0.f, Z + 17.f, 16.f, 40.f, 34.f, "SteelBlack");
		static const FName Bins[] = { "PaintRed", "PaintYellow", "PaintBlue" };
		for (int32 r = 0; r < 3; ++r)
		{
			for (int32 i = 0; i < 3; ++i) { P.Box(15.f, -12.f + i * 12.f, Z + 6.f + r * 11.f, 8.f, 10.f, 8.f, Bins[r], 0.f, -10.f); }
		}
	}

	/** An open gear locker (Level 3): origin on the wall, 110 wide, 196 tall. */
	void DrawGearLocker(const FPen& P)
	{
		const float W = 110.f, D = 50.f, H = 196.f;
		P.Box(3.f, 0.f, H * 0.5f, 3.f, W, H, "PaintOlive");
		for (float S : { -1.f, 1.f }) { P.Box(D * 0.5f, S * (W * 0.5f - 1.5f), H * 0.5f, D, 3.f, H, "PaintOlive"); }
		P.Box(D * 0.5f, 0.f, H - 1.5f, D, W, 3.f, "PaintOlive");
		P.Box(D * 0.5f, 0.f, 4.f, D, W, 8.f, "PaintOlive");
		P.Box(5.f, 0.f, H * 0.5f, 1.f, W - 8.f, H - 12.f, "Black");
		P.Box(D * 0.5f, 0.f, 150.f, D - 6.f, W - 6.f, 2.5f, "SteelBlack");
		for (int32 i = 0; i < 5; ++i) { const float Y = -W * 0.5f + 16.f + i * 19.f; P.Rod(FVector(18.f, Y, 9.f), FVector(10.f, Y, 132.f), 4.f, "SteelBlack"); P.Box(17.f, Y, 22.f, 7.f, 5.f, 28.f, "Walnut"); }
		for (int32 i = 0; i < 3; ++i) { P.Box(D * 0.5f, -W * 0.3f + i * W * 0.3f, 160.f, 30.f, 24.f, 16.f, i == 1 ? FName("Cardboard") : FName("PaintGrey")); }
		// the doors stand open
		for (float S : { -1.f, 1.f }) { P.Box(D + 22.f, S * (W * 0.5f + 12.f), H * 0.5f + 3.f, 50.f, 2.5f, H - 10.f, "PaintOlive", S * 28.f); }
	}

	/** A low steel strongbox for kit, where the locker has no wall to stand on (Level 3). */
	void DrawKitChest(const FPen& P)
	{
		P.Box(27.f, 0.f, 27.f, 48.f, 108.f, 50.f, "PaintOlive");
		P.Box(27.f, 0.f, 54.f, 50.f, 110.f, 5.f, "SteelBlack");
		for (float Y : { -34.f, 34.f }) { P.Box(51.6f, Y, 44.f, 1.4f, 10.f, 12.f, "Brass"); }
		for (float S : { -1.f, 1.f }) { P.Rod(FVector(20.f, S * 55.f, 34.f), FVector(34.f, S * 55.f, 34.f), 2.f, "Chrome"); }
		P.Box(24.f, -18.f, 60.f, 12.f, 64.f, 7.f, "SteelBlack", 8.f);
	}
}

void ADCHideoutBuilder::FurnishWorkshop(DCFurnish::FCtx& Ctx)
{
	using namespace DCFurnishWorkshop;
	using FFr = DCFurnish::FFrame;
	using FLoc = DCFurnish::FLocal;
	DCFurnish::FPlan& Plan = Ctx.Plan;
	const float PegTop = FMath::Min(200.f, Ctx.LampZ - 18.f);

	// frames out on the floor, turned to face the camera or square to the room
	TArray<FFr> FreeCamF;
	for (const FFr& F0 : Ctx.FreeF)
	{
		for (float Y : { YawOf(KToCam), Plan.RoomYaw - 90.f, Plan.RoomYaw + 180.f }) { FFr G = F0; G.Yaw = Y; FreeCamF.Add(G); }
	}
	// Ctx.BestSoft, but named for -DCFurnishDebug
	auto SoftBest = [&](const TArray<FLoc>& Ls, const TArray<FFr>& Frames, TFunctionRef<float(const FFr&)> Score, float Pad, float Penalty, const TCHAR* Name) -> FFr
	{
		TArray<FIntPoint> HitS;
		Plan.bSoftWindows = true;
		const FFr Got = Plan.Best(Ls, Frames, [&](const FFr& Cand) -> float
		{
			Plan.WindowsHit(Ls, Cand, HitS);
			if (HitS.Num() > 0 && Plan.WindowsLeft - HitS.Num() < 1) { return -1e9f; }
			return Score(Cand) - Penalty * HitS.Num();
		}, Pad, Name);
		Plan.bSoftWindows = false;
		return Got;
	};
	auto Near = [&](const FFr& Cand) { return -FMath::Min(Plan.NearestSolid(Cand.O), 200.f) * 0.15f - Plan.CornerDist(Cand.O) * 0.04f; };

	// ---------------------------------------------------------------- 1. the workbench (the room's station)
	// Against a tall wall it gets the tool wall above it (and may brick a window up); else it backs onto a partition,
	// else it stands at the back of the room facing the camera.
	FFr Bench;
	float BenchW = 0.f;
	bool bPeg = false;
	auto BenchParts = [&](float W, bool bWall)
	{
		TArray<FLoc> Ls = { FLoc::Solid(KBenchD * 0.5f + 2.f, 0.f, KBenchD * 0.5f + 2.f, W * 0.5f + 2.f, bWall ? 100.f : 136.f),
			FLoc::Space(KBenchD + 26.f, 0.f, 22.f, W * 0.5f - 24.f, 60.f) };
		if (bWall) { FLoc Peg = FLoc::Space(5.f, 0.f, 5.f, W * 0.5f + 4.f, PegTop + 6.f); Peg.Z0 = 100.f; Ls.Add(Peg); }
		return Ls;
	};
	{
		float BestS = -1e8f;
		for (float W : { 250.f, 215.f, 180.f })
		{
			const TArray<FLoc> Ls = BenchParts(W, true);
			auto Score = [&](const FFr& Cand) -> float
			{
				const DCFurnish::FEdge& Ed = Plan.E[Cand.Edge];
				return (float)(W * 0.5f - FMath::Abs(Cand.T - Ed.L * 0.5f) * 0.1f + Ctx.FacingCam(Cand) * 8.f + Ctx.Jit(Cand.Edge) * 4.f);
			};
			const FFr F = SoftBest(Ls, Ctx.WallF, Score, 4.f, 55.f, TEXT("bench peg"));
			if (!F.bValid) { continue; }
			Plan.WindowsHit(Ls, F, Ctx.Hit);
			const float Sc = Score(F) - 55.f * Ctx.Hit.Num();
			if (Sc > BestS) { BestS = Sc; Bench = F; BenchW = W; }
		}
		bPeg = Bench.bValid;
		for (float W : { 230.f, 190.f, 150.f })
		{
			if (Bench.bValid) { break; }
			const TArray<FLoc> Ls = BenchParts(W, false);
			Bench = Plan.Best(Ls, Ctx.WallPartF, [&](const FFr& Cand) -> float
			{
				// off to one side of the doorway, nearer the back corner
				return (float)(Ctx.FacingCam(Cand) * 10.f - Plan.CornerDist(Cand.O) * 0.02f + Ctx.Jit(Cand.Edge) * 4.f);
			}, 4.f, TEXT("bench"));
			if (!Bench.bValid)
			{
				Bench = Plan.Best(Ls, FreeCamF, [&](const FFr& Cand) -> float
				{
					if (FV2::DotProduct(Cand.O - Plan.Hub, KToCam) > 0.f) { return -1e9f; } // the back half only
					return (float)(-FV2::Distance(Cand.O, Plan.Hub - KToCam * 170.f) * 0.5f + Ctx.FacingCam(Cand) * 20.f);
				}, 4.f, TEXT("bench free"));
				if (Bench.bValid) { Bench.Edge = -1; }
			}
			if (Bench.bValid) { BenchW = W; }
		}
		if (Bench.bValid)
		{
			const float W = BenchW, Peg = bPeg ? PegTop : 0.f;
			const FLinearColor Warm(1.f, 0.62f, 0.32f);
			DCFurnish::FDrawFn Draw = [this, W, Peg, Warm](const FPen& P, int32 S)
			{
				DrawBench(P, W, Peg, S);
				Point(P.P(40.f, W * 0.5f - 40.f, KBenchTop + 30.f), Warm, 8.f, 300.f, false, true, 3.f);
			};
			if (bPeg) { Ctx.PlaceSoft(TEXT("bench"), BenchParts(W, true), Bench, Draw); }
			else { Ctx.Place(TEXT("bench"), BenchParts(W, false), Bench, Draw); }
			Ctx.Spot(Bench, KBenchD + 30.f, -W * 0.2f);
			Ctx.Spot(Bench, KBenchD + 30.f, W * 0.22f);
		}
	}

	// the room light: over the bench, else on a standing back wall, else a bare bulb
	if (bPeg && Bench.Edge >= 0) { Ctx.SetLamp(Bench.Edge, Bench.T); }
	else { Ctx.DefaultLamp(); }

	// the working floor in front of the bench stays clear of free-standing pieces
	auto BenchFront = [&](const FV2& P) -> bool
	{
		if (!Bench.bValid) { return false; }
		const FV2 D = P - Bench.O, X = Dir(Bench.Yaw), Y(-X.Y, X.X);
		const float Fw = FV2::DotProduct(D, X);
		return Fw > -20.f && Fw < KBenchD + 130.f && FMath::Abs(FV2::DotProduct(D, Y)) < BenchW * 0.5f + 60.f;
	};
	// free-standing on the back half of the floor, close to other furniture, off the bench's working floor
	auto FreeBack = [&](const FFr& Cand) -> float
	{
		if (FV2::DotProduct(Cand.O - Plan.Hub, KToCam) > -40.f || BenchFront(Cand.O)) { return -1e9f; }
		return (float)(-FMath::Min(Plan.NearestSolid(Cand.O), 300.f) * 0.3f + Ctx.FacingCam(Cand) * 12.f);
	};

	// ---------------------------------------------------------------- 2. the job table out on the floor, and a stool
	FFr Table;
	float TableW = 0.f, TableD = 0.f;
	{
		TArray<FFr> Fr = Ctx.FreeF;
		for (const FFr& F0 : Ctx.FreeF) { FFr G = F0; G.Yaw += 90.f; Fr.Add(G); }
		const FV2 Want = Bench.bValid ? Bench.O + Dir(Bench.Yaw) * 215.f : Plan.Hub - KToCam * 100.f;
		for (const FV2& Sz : { FV2(150.f, 78.f), FV2(120.f, 70.f) })
		{
			const TArray<FLoc> Ls = { FLoc::Solid(0.f, 0.f, Sz.Y * 0.5f + 4.f, Sz.X * 0.5f, 84.f) };
			Table = Plan.Best(Ls, Fr, [&](const FFr& Cand) -> float
			{
				if (Bench.bValid && FV2::DotProduct(Cand.O - Bench.O, Dir(Bench.Yaw)) < 150.f) { return -1e9f; } // leave the bench its floor
				return (float)(-FV2::Distance(Cand.O, Want) * 0.4f - Plan.CornerDist(Cand.O) * 0.02f);
			}, 8.f, TEXT("job table"));
			if (!Table.bValid) { continue; }
			TableW = Sz.X; TableD = Sz.Y;
			const float W = TableW, D = TableD;
			Ctx.Place(TEXT("jobtable"), Ls, Table, [W, D](const FPen& P, int32 S) { DrawJobTable(P, W, D, S); });
			break;
		}
		if (Table.bValid)
		{
			// the stool at whichever side is free, the camera's side first
			Plan.CapScale = 0.6f; Plan.bHubKeep = false;
			struct FSide { float X, Y, Yaw; };
			TArray<FSide> Sides = { { -(TableD * 0.5f + 26.f), TableW * 0.15f, 0.f }, { TableD * 0.5f + 26.f, -TableW * 0.15f, 180.f }, { 0.f, TableW * 0.5f + 24.f, -90.f }, { 0.f, -(TableW * 0.5f + 24.f), 90.f } };
			Sides.Sort([&](const FSide& A, const FSide& B)
			{
				return FV2::DotProduct(Rot(FV2(A.X, A.Y), Table.Yaw), KToCam) > FV2::DotProduct(Rot(FV2(B.X, B.Y), Table.Yaw), KToCam);
			});
			bool bStool = false;
			for (const FSide& Sd : Sides)
			{
				FFr F = Table; F.O = Table.O + Rot(FV2(Sd.X, Sd.Y), Table.Yaw); F.Yaw = Table.Yaw + Sd.Yaw; F.Edge = -1;
				if (!bStool)
				{
					const TArray<FLoc> Ls = { FLoc::Solid(0.f, 0.f, 17.f, 17.f, 62.f) };
					if (Plan.Fits(Ls, F, 3.f)) { Ctx.Place(TEXT("stool"), Ls, F, [](const FPen& P, int32) { DrawStool(P); }); bStool = true; continue; }
				}
				else
				{
					// somewhere to stand on another side
					FFr G = F; G.O = F.O - Dir(F.Yaw) * 8.f;
					if (Plan.Fits({ FLoc::Space(0.f, 0.f, 18.f, 18.f, 60.f) }, G, 2.f)) { Ctx.Spots.Add({ G.O, G.Yaw }); break; }
				}
			}
			Plan.CapScale = 1.f; Plan.bHubKeep = true;
		}
	}

	// ---------------------------------------------------------------- 3. shelving with gear cases
	{
		const TArray<FLoc> Tall = { FLoc::Solid(22.f, 0.f, 22.f, 54.f, 196.f) };
		FFr F = SoftBest(Tall, Ctx.WallF, [&](const FFr& Cand) -> float { return (float)(Near(Cand) + (Cand.Edge == Bench.Edge ? 6.f : 0.f) + Ctx.Jit(50 + Cand.Edge) * 4.f); }, 5.f, 30.f, TEXT("shelving tall"));
		if (F.bValid)
		{
			Ctx.PlaceSoft(TEXT("shelving"), Tall, F, [](const FPen& P, int32 S) { DrawShelving(P, 192.f, S); });
			Ctx.Spot(F, 76.f, 0.f);
		}
		else
		{
			// no wall for it: a lower unit against a partition, or a low one wherever there's an edge
			for (float Top : { 142.f, 82.f })
			{
				const TArray<FLoc> Ls = { FLoc::Solid(22.f, 0.f, 22.f, 54.f, Top + 3.f) };
				F = Plan.Best(Ls, Top > 100.f ? Ctx.WallPartF : Ctx.AnyWallF, [&](const FFr& Cand) -> float { return (float)(Near(Cand) + Ctx.FacingCam(Cand) * 6.f + Ctx.Jit(50 + Cand.Edge) * 4.f); }, 5.f, TEXT("shelving"));
				if (!F.bValid) { continue; }
				Ctx.Place(TEXT("shelving"), Ls, F, [Top](const FPen& P, int32 S) { DrawShelving(P, Top, S); });
				Ctx.Spot(F, 76.f, 0.f);
				break;
			}
		}
	}

	// ---------------------------------------------------------------- 4. the drill press, the tool chest, the gear rack
	{
		const TArray<FLoc> Ls = { FLoc::Solid(30.f, 0.f, 30.f, 27.f, 147.f), FLoc::Space(78.f, 0.f, 18.f, 22.f, 60.f) };
		FFr F = Plan.Best(Ls, Ctx.WallPartF, [&](const FFr& Cand) -> float { return (float)(Near(Cand) * 0.6f + Ctx.FacingCam(Cand) * 6.f + Ctx.Jit(60 + Cand.Edge) * 5.f); }, 6.f, TEXT("drill press"));
		if (!F.bValid)
		{
			// a room of doorways: it stands beside the bench or out on the floor at the back
			F = Plan.Best(Ls, FreeCamF, FreeBack, 6.f, TEXT("drill press free"));
		}
		if (F.bValid)
		{
			Ctx.Place(TEXT("drillpress"), Ls, F, [](const FPen& P, int32) { DrawDrillPress(P); });
			Ctx.Spot(F, 82.f, 0.f);
		}
	}
	{
		const TArray<FLoc> Ls = { FLoc::Solid(26.f, 0.f, 26.f, 39.f, 84.f) };
		const FFr F = Plan.Best(Ls, Ctx.AnyWallF, [&](const FFr& Cand) -> float
		{
			return (float)((Bench.bValid ? -FV2::Distance(Cand.O, Bench.O) * 0.06f : 0.f) + Near(Cand) * 0.5f + Ctx.FacingCam(Cand) * 4.f + Ctx.Jit(70 + Cand.Edge) * 4.f);
		}, 5.f, TEXT("tool chest"));
		if (F.bValid) { Ctx.Place(TEXT("toolchest"), Ls, F, [](const FPen& P, int32) { DrawToolChest(P); }); }
	}
	for (float Top : { 142.f, 84.f })
	{
		const TArray<FLoc> Ls = { FLoc::Solid(24.f, 0.f, 24.f, 65.f, Top + 2.f) };
		const FFr F = Plan.Best(Ls, Top > 100.f ? Ctx.WallPartF : Ctx.AnyWallF, [&](const FFr& Cand) -> float { return (float)(Near(Cand) + Ctx.Jit(80 + Cand.Edge) * 5.f); }, 5.f, TEXT("gear rack"));
		if (!F.bValid) { continue; }
		Ctx.Place(TEXT("gearrack"), Ls, F, [Top](const FPen& P, int32 S) { DrawGearRack(P, Top, S); });
		Ctx.Spot(F, 70.f, 0.f);
		break;
	}

	// ---------------------------------------------------------------- 5. drums in a corner
	{
		const TArray<FLoc> Ls = { FLoc::Solid(32.f, 0.f, 31.f, 61.f, 84.f) };
		const FFr F = Plan.Best(Ls, Ctx.AnyWallF, [&](const FFr& Cand) -> float { return (float)(-Plan.CornerDist(Cand.O) * 0.3f + Ctx.Jit(90 + Cand.Edge) * 5.f - FV2::DotProduct(Cand.O - Plan.Hub, KToCam) * 0.04f); }, 5.f, TEXT("drums"));
		if (F.bValid) { Ctx.Place(TEXT("drums"), Ls, F, [](const FPen& P, int32 S) { DrawDrums(P, S); }); }
	}

	// ---------------------------------------------------------------- 6. upgrades: welding kit and parts bins, then the locker
	{
		const TArray<FLoc> Ls = { FLoc::Solid(29.f, 0.f, 29.f, 34.f, 149.f) };
		FFr F = Plan.Best(Ls, Ctx.WallPartF, [&](const FFr& Cand) -> float { return (float)(Near(Cand) + Ctx.Jit(100 + Cand.Edge) * 4.f); }, 5.f, TEXT("welder"));
		if (!F.bValid)
		{
			// out on the floor beside other furniture (the height bands keep it off the open edges)
			F = Plan.Best(Ls, FreeCamF, [&](const FFr& Cand) -> float
			{
				return BenchFront(Cand.O) ? -1e9f : (float)(-FMath::Min(Plan.NearestSolid(Cand.O), 300.f) * 0.3f + Ctx.FacingCam(Cand) * 8.f - FV2::DotProduct(Cand.O - Plan.Hub, KToCam) * 0.05f);
			}, 5.f, TEXT("welder free"));
		}
		if (F.bValid) { Ctx.Place(TEXT("welder"), Ls, F, [](const FPen& P, int32) { DrawWeldingCart(P); }, DCGroups::Level2); }
	}
	bool bBins = false;
	for (float W : { 110.f, 80.f })
	{
		TArray<FLoc> Ls = { FLoc::Solid(7.f, 0.f, 7.f, W * 0.5f + 2.f, 184.f, 128.f) };
		Ls[0].bSolid = false;
		const FFr F = Plan.Best(Ls, Ctx.WallF, [&](const FFr& Cand) -> float { return (float)((Bench.bValid ? -FV2::Distance(Cand.O, Bench.O) * 0.03f : 0.f) + Ctx.Jit(110 + Cand.Edge) * 4.f); }, 16.f, TEXT("bins"));
		if (!F.bValid) { continue; }
		Ctx.Place(TEXT("bins"), Ls, F, [W](const FPen& P, int32) { DrawPartsBins(P, W); }, DCGroups::Level2);
		bBins = true;
		break;
	}
	if (!bBins && Bench.bValid)
	{
		// no free wall: a small rack of parts bins stands on the bench top instead
		const float W = BenchW;
		FLoc Top = FLoc::Space(10.f, W * 0.5f - 48.f, 8.f, 20.f, KBenchTop + 36.f);
		Top.Z0 = KBenchTop;
		Ctx.Place(TEXT("bins bench"), { Top }, Bench, [W](const FPen& P, int32) { DrawBenchBins(P.At(0.f, W * 0.5f - 48.f)); }, DCGroups::Level2);
	}
	{
		const TArray<FLoc> Tall = { FLoc::Solid(26.f, 0.f, 26.f, 57.f, 198.f), FLoc::Space(74.f, 0.f, 24.f, 70.f, 198.f) };
		FFr F = Plan.Best(Tall, Ctx.WallF, [&](const FFr& Cand) -> float { return (float)(Near(Cand) + Ctx.Jit(120 + Cand.Edge) * 4.f); }, 5.f, TEXT("locker"));
		if (F.bValid) { Ctx.Place(TEXT("locker"), Tall, F, [](const FPen& P, int32) { DrawGearLocker(P); }, DCGroups::Level3); }
		else
		{
			const TArray<FLoc> Ls = { FLoc::Solid(27.f, 0.f, 27.f, 57.f, 68.f) };
			F = Plan.Best(Ls, Ctx.AnyWallF, [&](const FFr& Cand) -> float { return (float)(Near(Cand) + Ctx.FacingCam(Cand) * 5.f + Ctx.Jit(120 + Cand.Edge) * 4.f); }, 5.f, TEXT("kitchest"));
			if (!F.bValid)
			{
				// out on the floor: anywhere but the lanes and the bench's working floor
				F = Plan.Best(Ls, FreeCamF, [&](const FFr& Cand) -> float
				{
					return BenchFront(Cand.O) ? -1e9f : (float)(-FMath::Min(Plan.NearestSolid(Cand.O), 300.f) * 0.3f - Plan.CornerDist(Cand.O) * 0.05f);
				}, 5.f, TEXT("kitchest free"));
			}
			if (F.bValid) { Ctx.Place(TEXT("kitchest"), Ls, F, [](const FPen& P, int32) { DrawKitChest(P); }, DCGroups::Level3); }
		}
	}

	// ---------------------------------------------------------------- 7. clutter and the walls (no rug: it's a workshop)
	Ctx.Clutter(0.4f);
	Ctx.WallDecor();
}
