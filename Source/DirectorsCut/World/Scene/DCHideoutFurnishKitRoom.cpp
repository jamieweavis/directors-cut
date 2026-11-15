// Director's Cut — the Kit Room's furnishing recipe (see DCHideoutFurnish.cpp; the room type is still
// EDCRoomType::Garage). Where crews grab their gear and go: a bank of steel lockers with a bench in front, the
// dispatch board (who's out, when they're due back) beside the key cabinet, a packing table with go-bags, coveralls
// on a peg rail with boots under it. Upgrades add a shelf of ready-packed bags and a radio charging station.
// With no tall wall for the lockers, a low row of half-lockers backs onto a partition or stands free.
#include "World/Scene/DCHideoutFurnish.h"
#include "Components/PointLightComponent.h"

using namespace DCFurnish;

namespace DCFurnishKit
{
	const FName LockerPaints[] = { "PaintBlue", "PaintGreen", "PaintGrey", "PaintOlive" };

	/** A packed go-bag lying along Y, centred at (X, Y) with its bottom at Z. */
	void DrawGoBag(const DCFurnish::FPen& P, float X, float Y, float Z, float L, FName Mat, float YawOff)
	{
		const DCFurnish::FPen B = P.At(X, Y, YawOff, Z);
		B.Box(0.f, 0.f, 13.f, 28.f, L, 26.f, Mat);
		B.Cyl(0.f, L * 0.5f, 13.f, 26.f, 3.f, Mat, 90.f);
		B.Cyl(0.f, -L * 0.5f, 13.f, 26.f, 3.f, Mat, 90.f);
		B.Box(14.5f, 0.f, 15.f, 1.f, L * 0.7f, 2.f, "Black");          // zip
		for (float S : { -1.f, 1.f }) { B.Box(0.f, S * L * 0.18f, 27.f, 4.f, 3.f, 6.f, "Black"); } // handle loops
		B.Box(0.f, 0.f, 30.f, 4.f, L * 0.36f + 3.f, 2.5f, "Black");
	}

	/** A bank of steel lockers against the frame's back, Ht tall. iOpen: the locker standing ajar (-1 none). */
	void DrawLockers(const DCFurnish::FPen& P, float W, float Ht, int32 iOpen, int32 Seed)
	{
		const int32 N = FMath::Max(2, FMath::FloorToInt(W / 42.f));
		const float CW = W / N, D = 44.f;
		const FName Main = LockerPaints[FMath::Min(3, (int32)(U(Seed, 1) * 4.f))];
		P.Box(D * 0.5f, 0.f, 4.f, D, W, 8.f, "SteelBlack");
		P.Box(D * 0.5f, 0.f, Ht + 1.5f, D + 2.f, W + 2.f, 3.f, "SteelBlack");
		for (int32 i = 0; i < N; ++i)
		{
			const float Y = (i + 0.5f) * CW - W * 0.5f;
			const FName Paint = U(Seed, 10 + i) > 0.85f ? FName("PaintRed") : Main; // one odd replacement now and then
			const float Zc = 8.f + (Ht - 8.f) * 0.5f, DH = Ht - 18.f;
			P.Box(D * 0.5f, Y, Zc, D, CW - 1.2f, Ht - 8.f, Paint);
			if (i == iOpen)
			{
				// open: the dark inside, a coverall on the hook, a bag at the bottom, the door swung into the room
				P.Box(D - 1.f, Y, Zc, 1.f, CW - 5.f, DH, "Black", 0.f, 0.f, 0.f, false);
				P.Box(D - 12.f, Y, Ht - 62.f, 10.f, CW - 12.f, 76.f, "FabricOlive");
				P.Box(D - 16.f, Y, 26.f, 22.f, CW - 10.f, 22.f, "NylonOlive");
				P.Box(D - 22.f, Y, Ht - 28.f, 20.f, CW - 8.f, 2.f, "Steel"); // the shelf
				const DCFurnish::FPen Door = P.At(D + 0.8f, Y - CW * 0.5f + 2.5f, -62.f - U(Seed, 3) * 20.f);
				Door.Box(0.f, (CW - 5.f) * 0.5f, Zc, 1.4f, CW - 5.f, DH, Paint);
				Door.Box(0.9f, CW - 9.f, Zc, 1.5f, 2.f, 10.f, "Chrome");
				continue;
			}
			P.Box(D + 0.5f, Y, Zc, 1.2f, CW - 5.f, DH, Paint);
			for (int32 k = 0; k < 4; ++k) { P.Box(D + 1.3f, Y, Ht - 20.f - k * 5.f, 0.6f, CW * 0.55f, 1.6f, "Black", 0.f, 0.f, 0.f, false); } // vents
			for (int32 k = 0; k < 3; ++k) { P.Box(D + 1.3f, Y, 26.f + k * 5.f, 0.6f, CW * 0.55f, 1.6f, "Black", 0.f, 0.f, 0.f, false); }
			P.Box(D + 1.6f, Y + CW * 0.32f, Zc, 1.6f, 2.f, 11.f, "Chrome");
			P.Box(D + 1.3f, Y, Ht - 46.f, 0.6f, CW * 0.45f, 6.f, "Paper", 0.f, 0.f, 0.f, false); // name card
			if (U(Seed, 30 + i) > 0.7f) { P.Box(D + 1.4f, Y + J(Seed, 40 + i) * 5.f, Zc + 22.f, 0.5f, 12.f, 15.f, "Photo", J(Seed, 50 + i) * 8.f, 0.f, 0.f, false); }
		}
		// whatever got slung on top
		if (U(Seed, 60) > 0.25f) { P.Box(D * 0.5f, -W * 0.25f, Ht + 15.f, 36.f, 44.f, 24.f, "Cardboard", J(Seed, 61) * 8.f); }
		if (U(Seed, 62) > 0.4f) { DrawGoBag(P, D * 0.5f, W * 0.2f, Ht + 3.f, 50.f, "Nylon", 4.f + J(Seed, 63) * 6.f); }
		if (U(Seed, 64) > 0.5f) { P.Ball(D * 0.5f, W * 0.45f - 14.f, Ht + 10.f, 24.f, 24.f, 16.f, "PaintYellow"); } // a hard hat
	}

	/** The changing bench, along Y, centred on the frame. */
	void DrawBench(const DCFurnish::FPen& P, float W, int32 Seed)
	{
		for (float S : { -1.f, 1.f })
		{
			P.Box(0.f, S * (W * 0.5f - 10.f), 20.f, 28.f, 4.f, 40.f, "SteelBlack");
			P.Box(0.f, S * (W * 0.5f - 10.f), 2.f, 34.f, 6.f, 4.f, "SteelBlack");
		}
		P.Box(0.f, 0.f, 42.f, 34.f, W, 5.f, "Timber");
		P.Box(0.f, 0.f, 14.f, 4.f, W - 24.f, 4.f, "SteelBlack");
		if (U(Seed, 1) > 0.35f) { P.Box(J(Seed, 2) * 4.f, W * 0.22f, 46.f, 26.f, 34.f, 3.f, "Linen", J(Seed, 3) * 20.f); } // a towel
		if (U(Seed, 4) > 0.5f)
		{
			for (float S : { -1.f, 1.f }) { P.Box(-6.f + S * 7.f, -W * 0.3f, 7.f, 12.f, 28.f, 14.f, "LeatherBrn", 90.f + J(Seed, 5) * 15.f); } // boots kicked off
		}
	}

	/** Dispatch board (W wide), hung on the wall with the key cabinet beside it (bKeys). Due-back cards in rows. */
	void DrawDispatch(const DCFurnish::FPen& P, float W, float Top, int32 Seed, bool bKeys = true)
	{
		const float H = 86.f, Zc = Top - H * 0.5f - 6.f, BY = bKeys ? -30.f : 0.f;
		P.Box(2.f, BY, Zc, 4.f, W + 6.f, H + 6.f, "Alu");
		P.Box(4.3f, BY, Zc, 0.6f, W, H, "PlasticWhite", 0.f, 0.f, 0.f, false);
		P.Box(4.8f, BY, Zc + H * 0.5f - 9.f, 0.4f, W - 10.f, 7.f, "Accent", 0.f, 0.f, 0.f, false); // header strip
		const int32 Rows = 4;
		for (int32 r = 0; r < Rows; ++r)
		{
			const float Z = Zc + H * 0.5f - 22.f - r * 16.f;
			P.Box(4.7f, BY, Z - 7.f, 0.3f, W - 10.f, 0.8f, "Black", 0.f, 0.f, 0.f, false);
			// a name card, a time card, a magnet: red out, green back
			P.Box(4.9f, BY - W * 0.3f, Z, 0.4f, W * 0.28f, 9.f, "Paper", 0.f, 0.f, 0.f, false);
			if (U(Seed, 10 + r) > 0.3f) { P.Box(4.9f, BY + W * 0.08f, Z, 0.4f, W * 0.2f, 9.f, "Paper", J(Seed, 20 + r) * 3.f, 0.f, 0.f, false); }
			P.Cyl(5.6f, BY + W * 0.38f, Z, 6.f, 2.f, U(Seed, 30 + r) > 0.5f ? FName("PaintRed") : FName("PaintGreen"), 90.f);
		}
		P.Box(6.f, BY, Zc - H * 0.5f - 4.f, 8.f, W * 0.6f, 2.f, "Alu");                 // pen tray
		P.Cyl(9.f, BY - W * 0.1f, Zc - H * 0.5f - 2.f, 2.f, 12.f, "PaintRed", 90.f, 90.f);
		if (!bKeys) { return; }
		// the key cabinet, door open, keys on hooks
		const float KY = W * 0.5f + 8.f, KZ = Top - 34.f;
		P.Box(6.f, KY, KZ, 12.f, 42.f, 54.f, "PaintGrey");
		P.Box(12.3f, KY, KZ, 0.5f, 36.f, 48.f, "Board", 0.f, 0.f, 0.f, false);
		for (int32 r = 0; r < 3; ++r)
		{
			for (int32 c = 0; c < 4; ++c)
			{
				if (U(Seed, 40 + r * 4 + c) < 0.25f) { continue; } // that car is out
				const float Y = KY - 13.f + c * 8.7f, Z = KZ + 14.f - r * 15.f;
				P.Box(13.f, Y, Z, 1.5f, 1.f, 1.f, "Chrome");
				P.Box(13.5f, Y, Z - 4.f, 0.6f, 2.f, 5.f, "Brass", 0.f, 0.f, 0.f, false);
				P.Box(13.6f, Y, Z - 8.5f, 0.5f, 3.f, 3.f, c % 2 ? FName("PaintRed") : FName("PaintYellow"), 0.f, 0.f, 0.f, false);
			}
		}
		const DCFurnish::FPen Door = P.At(12.f, KY + 21.f, 110.f);
		Door.Box(0.f, -21.f, KZ, 1.2f, 42.f, 54.f, "PaintGrey");
	}

	/** The dispatch board on a rolling frame, for a room with no tall wall to hang it on. Faces +X. */
	void DrawRollingBoard(const DCFurnish::FPen& P, float W, int32 Seed)
	{
		for (float S : { -1.f, 1.f })
		{
			const float Y = S * (W * 0.5f + 3.f);
			P.Box(0.f, Y, 4.f, 44.f, 5.f, 4.f, "SteelBlack");
			for (float X : { -18.f, 18.f }) { P.Cyl(X, Y, 3.f, 6.f, 3.f, "Rubber", 90.f, 90.f); }
			P.Box(0.f, Y, 74.f, 4.f, 4.f, 140.f, "Alu");
		}
		P.Box(0.f, 0.f, 26.f, 3.f, W, 3.f, "Alu");
		// the board face sits on the frame's front (no key cabinet: nothing to hang it on)
		DrawDispatch(P.At(-2.f, 0.f), W - 20.f, 146.f, Seed, false);
	}

	/** The packing table: W along Y, D along X, go-bags and kit laid out on it. */
	void DrawPackingTable(const DCFurnish::FPen& P, float W, float D, int32 Seed)
	{
		for (float SX : { -1.f, 1.f }) { for (float SY : { -1.f, 1.f }) { P.Box(SX * (D * 0.5f - 5.f), SY * (W * 0.5f - 5.f), 39.f, 5.f, 5.f, 78.f, "SteelBlack"); } }
		P.Box(0.f, 0.f, 18.f, D - 10.f, W - 10.f, 3.f, "Plywood");
		P.Box(0.f, 0.f, 80.f, D, W, 4.f, "Plywood");
		DrawGoBag(P, -D * 0.12f, -W * 0.22f, 82.f, FMath::Min(70.f, W * 0.4f), U(Seed, 1) > 0.5f ? FName("NylonOlive") : FName("Black"), 90.f + J(Seed, 2) * 10.f);
		// a bag still open: its kit lined up beside it
		P.Box(D * 0.12f, W * 0.12f, 85.f, 30.f, 26.f, 6.f, "Paper", J(Seed, 3) * 8.f);                 // map
		P.Cyl(D * 0.25f, W * 0.3f, 87.f, 8.f, 10.f, "Alu");                                               // tape
		P.Cyl(-D * 0.2f, W * 0.3f, 85.f, 5.f, 22.f, "PlasticBlack", 90.f, J(Seed, 4) * 30.f);           // torch
		P.Box(-D * 0.25f, W * 0.12f, 84.f, 10.f, 6.f, 4.f, "PlasticBlack", J(Seed, 5) * 20.f);          // radio
		P.Box(-D * 0.25f, W * 0.12f + 2.f, 92.f, 1.f, 1.f, 12.f, "PlasticBlack");
		P.Box(D * 0.2f, -W * 0.02f, 83.f, 14.f, 20.f, 2.f, "Board", J(Seed, 6) * 10.f);               // clipboard
		P.Box(D * 0.2f, -W * 0.02f, 84.2f, 11.f, 16.f, 0.5f, "Paper", J(Seed, 6) * 10.f, 0.f, 0.f, false);
		if (U(Seed, 7) > 0.4f) { DrawGoBag(P, 0.f, W * 0.15f, 0.f, 56.f, "Nylon", J(Seed, 8) * 20.f); } // one under the table
	}

	/** A peg rail on the wall: coveralls and a hi-vis vest hung up, boots on a tray below. W along Y. */
	void DrawPegRail(const DCFurnish::FPen& P, float W, int32 Seed)
	{
		P.Box(2.f, 0.f, 140.f, 3.f, W, 10.f, "Timber");
		const int32 N = FMath::Max(3, FMath::FloorToInt(W / 30.f));
		for (int32 i = 0; i < N; ++i)
		{
			const float Y = (i + 0.5f) * W / N - W * 0.5f;
			P.Rod(FVector(3.f, Y, 140.f), FVector(10.f, Y, 144.f), 2.f, "Chrome");
			const float R = U(Seed, 10 + i);
			if (R < 0.2f) { continue; } // an empty peg
			const FName Mat = R > 0.85f ? FName("PaintOrange") : R > 0.55f ? FName("Nylon") : FName("FabricOlive");
			const float Len = R > 0.85f ? 52.f : 88.f;
			P.Box(9.f, Y, 140.f - Len * 0.5f, 7.f, 24.f, Len, Mat, J(Seed, 20 + i) * 4.f, 0.f, J(Seed, 30 + i) * 3.f);
			if (R > 0.85f) { P.Box(13.f, Y, 128.f, 0.5f, 22.f, 3.f, "PaintYellow", 0.f, 0.f, 0.f, false); } // the reflective band
		}
		P.Box(18.f, 0.f, 1.5f, 32.f, W, 3.f, "RubberMat");
		for (int32 i = 0; i < N - 1; ++i)
		{
			if (U(Seed, 40 + i) < 0.35f) { continue; }
			const float Y = (i + 0.5f) * W / N - W * 0.5f + 6.f;
			const FName Mat = U(Seed, 50 + i) > 0.5f ? FName("Rubber") : FName("LeatherBrn");
			for (float S : { -1.f, 1.f }) { P.Box(18.f, Y + S * 6.f, 10.f, 26.f, 10.f, 16.f, Mat, J(Seed, 60 + i) * 8.f); }
		}
	}

	/** Level 2: steel shelving of ready-packed go-bags, Ht tall, W along Y. */
	void DrawBagShelves(const DCFurnish::FPen& P, float W, float Ht, int32 Seed)
	{
		for (float SY : { -1.f, 1.f }) { for (float X : { 3.f, 39.f }) { P.Box(X, SY * (W * 0.5f - 2.f), Ht * 0.5f, 3.f, 3.f, Ht, "Galv"); } }
		const int32 Levels = Ht > 150.f ? 4 : Ht > 100.f ? 3 : 2;
		for (int32 l = 0; l < Levels; ++l)
		{
			const float Z = 10.f + l * (Ht - 14.f) / (Levels - 1);
			P.Box(21.f, 0.f, Z, 40.f, W, 2.f, "Galv");
			if (l == Levels - 1 && Ht > 150.f) { continue; } // nothing reachable up there
			for (int32 k = 0; k < 2; ++k)
			{
				if (U(Seed, 10 + l * 3 + k) < 0.2f) { continue; }
				const FName Mat = U(Seed, 20 + l * 3 + k) > 0.6f ? FName("NylonOlive") : U(Seed, 30 + l * 3 + k) > 0.5f ? FName("Black") : FName("Nylon");
				DrawGoBag(P, 21.f, (k - 0.5f) * W * 0.48f, Z + 1.f, FMath::Min(54.f, W * 0.42f), Mat, J(Seed, 40 + l * 3 + k) * 6.f);
			}
		}
		P.Box(42.f, -W * 0.3f, 10.f + (Ht - 14.f) / (Levels - 1) * 0.5f, 0.5f, 10.f, 7.f, "Paper", 0.f, 0.f, 0.f, false); // "READY" tag
	}

	/** Level 3: a low cabinet with a row of radios in their chargers, LEDs lit. */
	void DrawRadioStation(const DCFurnish::FPen& P, float W, int32 Seed)
	{
		P.Box(22.f, 0.f, 42.f, 44.f, W, 84.f, "PaintGrey");
		P.Box(44.5f, 0.f, 42.f, 1.f, W - 8.f, 76.f, "PaintGrey");
		P.Box(45.3f, W * 0.3f, 60.f, 1.5f, 10.f, 2.f, "Chrome");
		P.Box(22.f, 0.f, 85.f, 46.f, W + 2.f, 2.f, "SteelBlack");
		P.Box(14.f, 0.f, 89.f, 22.f, W - 12.f, 6.f, "PlasticBlack");
		const int32 N = FMath::Max(3, FMath::FloorToInt((W - 16.f) / 15.f));
		for (int32 i = 0; i < N; ++i)
		{
			const float Y = (i + 0.5f) * (W - 16.f) / N - (W - 16.f) * 0.5f;
			if (U(Seed, 10 + i) < 0.15f) { continue; } // one's out
			P.Box(14.f, Y, 104.f, 6.f, 8.f, 24.f, "PlasticBlack");
			P.Cyl(14.f, Y + 2.f, 122.f, 1.6f, 14.f, "Black");
			P.Box(25.4f, Y, 93.f, 0.6f, 2.f, 2.f, U(Seed, 20 + i) > 0.7f ? FName("LedR") : FName("LedG"), 0.f, 0.f, 0.f, false);
		}
	}
}

void ADCHideoutBuilder::FurnishKitRoom(DCFurnish::FCtx& Ctx)
{
	using namespace DCFurnishKit;
	DCFurnish::FPlan& Plan = Ctx.Plan;
	const int32 Seed = Ctx.Seed;
	const float LockerHt = FMath::Min(186.f, Ctx.LampZ - 30.f);
	auto Nudge = [](const TArray<DCFurnish::FFrame>& In) { TArray<DCFurnish::FFrame> Out = In; for (DCFurnish::FFrame& F : Out) { F.O += Dir(F.Yaw) * 5.f; } return Out; };
	const TArray<DCFurnish::FFrame> WallF = Nudge(Ctx.WallF), WallPartF = Nudge(Ctx.WallPartF);
	auto Adjacent = [&](const DCFurnish::FFrame& F) { return (float)(-FMath::Min(Plan.NearestSolid(F.O), 200.f) * 0.15f - Plan.CornerDist(F.O) * 0.04f); };
	auto FreeTurned = [&](std::initializer_list<float> Offs)
	{
		TArray<DCFurnish::FFrame> Fr;
		for (const DCFurnish::FFrame& F0 : Ctx.FreeF) { for (float Off : Offs) { DCFurnish::FFrame F = F0; F.Yaw += Off; Fr.Add(F); } }
		return Fr;
	};

	// ---------------------------------------------------------------- 1. the lockers (the room's station)
	// The widest bank a tall wall takes (a window may be bricked up for it), the bench out in front.
	DCFurnish::FFrame Lockers;
	float LockW = 0.f;
	bool bTall = false;
	{
		float BestS = -1e8f;
		for (float W : { 300.f, 252.f, 210.f, 168.f })
		{
			const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(23.f, 0.f, 23.f, W * 0.5f, LockerHt + 30.f) };
			auto Score = [&](const DCFurnish::FFrame& Cand)
			{
				const DCFurnish::FEdge& Ed = Plan.E[Cand.Edge];
				return (float)(W * 0.5f - FMath::Abs(Cand.T - Ed.L * 0.5f) * 0.1f + Ed.L * 0.02f + Ctx.Jit(Cand.Edge) * 3.f);
			};
			const DCFurnish::FFrame F = Ctx.BestSoft(Ls, WallF, Score, 4.f, 45.f);
			if (!F.bValid) { continue; }
			Plan.WindowsHit(Ls, F, Ctx.Hit);
			const float S = Score(F) - 45.f * Ctx.Hit.Num();
			if (S > BestS) { BestS = S; Lockers = F; LockW = W; }
		}
	}
	if (Lockers.bValid)
	{
		bTall = true;
		const float W = LockW, Ht = LockerHt;
		const int32 Open = (int32)(U(Seed, 5) * FMath::Max(2, FMath::FloorToInt(W / 42.f)));
		Ctx.PlaceSoft(TEXT("lockers"), { DCFurnish::FLocal::Solid(23.f, 0.f, 23.f, W * 0.5f, Ht + 30.f) }, Lockers,
			[W, Ht, Open](const DCFurnish::FPen& P, int32 S) { DrawLockers(P, W, Ht, Open, S); });
		Ctx.SetLamp(Lockers.Edge, Lockers.T);
	}
	else
	{
		// no tall wall: a low row of half-lockers on a partition, else standing free with their doors to the camera
		Ctx.DefaultLamp();
		for (float W : { 210.f, 168.f, 126.f })
		{
			const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(23.f, 0.f, 23.f, W * 0.5f, 118.f) };
			DCFurnish::FFrame F = Plan.Best(Ls, WallPartF, [&](const DCFurnish::FFrame& Cand)
			{
				return (float)(Ctx.FacingCam(Cand) * 10.f - FMath::Abs(Cand.T - Plan.E[Cand.Edge].L * 0.5f) * 0.02f + Ctx.Jit(Cand.Edge) * 3.f);
			}, 4.f, TEXT("lockers low"));
			if (!F.bValid)
			{
				const TArray<DCFurnish::FFrame> Fr = FreeTurned({ 0.f, -90.f, 180.f });
				F = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& Cand)
				{
					if (FV2::DotProduct(Cand.O - Plan.Hub, KToCam) > 0.f) { return -1e9f; } // the back half only
					return (float)(-FV2::Distance(Cand.O, Plan.Hub - KToCam * 160.f) * 0.5f + Ctx.FacingCam(Cand) * 20.f);
				}, 4.f, TEXT("lockers free"));
				if (F.bValid) { F.Edge = -1; }
			}
			if (!F.bValid) { continue; }
			Lockers = F; LockW = W;
			const int32 Open = (int32)(U(Seed, 5) * FMath::Max(2, FMath::FloorToInt(W / 42.f)));
			Ctx.Place(TEXT("lockers"), Ls, F, [W, Open](const DCFurnish::FPen& P, int32 S) { DrawLockers(P, W, 108.f, Open, S); });
			break;
		}
	}
	if (Lockers.bValid)
	{
		Ctx.Spot(Lockers, 72.f, -LockW * 0.25f);
		// the bench: in front of the doors; a free-standing bank may have it behind instead
		Plan.CapScale = 0.7f; Plan.bHubKeep = false; // the bench may crowd the walk a little
		bool bBench = false;
		for (float BW : { FMath::Min(LockW * 0.7f, 190.f), 110.f })
		{
			for (const FV2& At : { FV2(98.f, 0.f), FV2(98.f, -LockW * 0.2f), FV2(98.f, LockW * 0.2f), FV2(-56.f, 0.f) })
			{
				if (At.X < 0.f && Lockers.Edge >= 0) { continue; }
				DCFurnish::FFrame B = Lockers;
				B.O = Lockers.O + Rot(FV2(At.X, At.Y + J(Seed, 6) * 8.f), Lockers.Yaw); B.Edge = -1;
				if (At.X < 0.f) { B.Yaw += 180.f; }
				const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(0.f, 0.f, 18.f, BW * 0.5f, 50.f) };
				if (!Plan.Fits(Ls, B, 3.f)) { continue; }
				Ctx.Place(TEXT("bench"), Ls, B, [BW](const DCFurnish::FPen& P, int32 S) { DrawBench(P, BW, S); });
				Ctx.Spot(B, 40.f, BW * 0.2f);
				bBench = true;
				break;
			}
			if (bBench) { break; }
		}
		Plan.CapScale = 1.f; Plan.bHubKeep = true;
	}

	// ---------------------------------------------------------------- 2. the dispatch board and key cabinet
	const float BoardTop = FMath::Min(206.f, Ctx.LampZ - 16.f);
	DCFurnish::FFrame Board;
	for (float W : { 140.f, 110.f })
	{
		TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Space(7.f, 0.f, 7.f, W * 0.5f + 40.f, BoardTop + 4.f) };
		Ls[0].Z0 = BoardTop - 104.f;
		Board = Plan.Best(Ls, WallF, [&](const DCFurnish::FFrame& Cand)
		{
			return (float)((Cand.Edge == Lockers.Edge ? -25.f : 0.f) - FMath::Abs(Cand.T - Plan.E[Cand.Edge].L * 0.5f) * 0.05f + Ctx.Jit(40 + Cand.Edge) * 4.f);
		}, 10.f, TEXT("dispatch"));
		if (!Board.bValid) { continue; }
		const float Top = BoardTop;
		Ctx.Place(TEXT("dispatch"), Ls, Board, [W, Top](const DCFurnish::FPen& P, int32 S) { DrawDispatch(P, W, Top, S); });
		Ctx.Spot(Board, 72.f, -20.f);
		break;
	}
	if (!Board.bValid)
	{
		// no tall wall: the board on a rolling frame, turned to the camera, near the lockers
		Plan.bHubKeep = false; Plan.CapScale = 0.8f;
		for (float W : { 170.f, 140.f })
		{
			const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(0.f, 0.f, 24.f, W * 0.5f + 8.f, 148.f), DCFurnish::FLocal::Space(56.f, 0.f, 30.f, W * 0.35f, 100.f) };
			const TArray<DCFurnish::FFrame> Fr = FreeTurned({ 0.f, -90.f, 90.f, 180.f });
			Board = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& Cand)
			{
				if (Ctx.FacingCam(Cand) < 0.3f) { return -1e9f; }
				return (float)(Ctx.FacingCam(Cand) * 20.f + Adjacent(Cand) * 0.6f + FMath::Min(FV2::Distance(Cand.O, Plan.Hub), 200.f) * 0.08f);
			}, 6.f, TEXT("dispatch free"));
			if (!Board.bValid) { continue; }
			Board.Edge = -1;
			Ctx.Place(TEXT("dispatch"), Ls, Board, [W](const DCFurnish::FPen& P, int32 S) { DrawRollingBoard(P, W, S); });
			Ctx.Spot(Board, 72.f, -20.f);
			break;
		}
		Plan.bHubKeep = true; Plan.CapScale = 1.f;
	}

	// ---------------------------------------------------------------- 3. the packing table, out in the room
	for (const FV2& Sz : { FV2(150.f, 80.f), FV2(124.f, 72.f), FV2(100.f, 64.f) })
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(0.f, 0.f, Sz.Y * 0.5f, Sz.X * 0.5f, 100.f) };
		const TArray<DCFurnish::FFrame> Fr = FreeTurned({ 0.f, 90.f });
		const FV2 Want = Lockers.bValid ? Lockers.O + Dir(Lockers.Yaw) * 250.f : Plan.Hub - KToCam * 100.f;
		const DCFurnish::FFrame F = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& Cand)
		{
			if (Lockers.bValid && FV2::DotProduct(Cand.O - Lockers.O, Dir(Lockers.Yaw)) < 175.f) { return -1e9f; } // clear of the bench
			return (float)(-FV2::Distance(Cand.O, Want) * 0.4f - FMath::Abs(FMath::FindDeltaAngleDegrees(Cand.Yaw, Lockers.bValid ? Lockers.Yaw : Plan.RoomYaw)) * 0.02f);
		}, 8.f, TEXT("packing table"));
		if (!F.bValid) { continue; }
		const float TW = Sz.X, TD = Sz.Y;
		Ctx.Place(TEXT("packtable"), Ls, F, [TW, TD](const DCFurnish::FPen& P, int32 S) { DrawPackingTable(P, TW, TD, S); });
		Ctx.Spot(F, TD * 0.5f + 34.f, 0.f);
		DCFurnish::FFrame Back = F; Back.Yaw += 180.f;
		Ctx.Spot(Back, TD * 0.5f + 34.f, TW * 0.15f);
		Ctx.PlaceRug(F, 0.f, TD + 100.f, TW + 90.f, "RubberMat");
		break;
	}

	Ctx.Runner();

	// ---------------------------------------------------------------- 4. the peg rail with coveralls and boots
	for (float W : { 150.f, 110.f, 84.f })
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(17.f, 0.f, 17.f, W * 0.5f, 148.f) };
		const DCFurnish::FFrame F = Ctx.BestSoft(Ls, WallPartF, [&](const DCFurnish::FFrame& Cand)
		{
			return (float)(Adjacent(Cand) + (Cand.Edge == Lockers.Edge ? 6.f : 0.f) + Ctx.Jit(20 + Cand.Edge) * 4.f);
		}, 6.f, 30.f);
		if (!F.bValid) { continue; }
		Ctx.PlaceSoft(TEXT("pegrail"), Ls, F, [W](const DCFurnish::FPen& P, int32 S) { DrawPegRail(P, W, S); });
		Ctx.Spot(F, 66.f, 0.f);
		break;
	}

	// ---------------------------------------------------------------- 5. upgrades: the bag shelves, the radios
	{
		const float Ht = bTall ? 178.f : 120.f;
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(21.f, 0.f, 21.f, 56.f, Ht + 30.f) };
		DCFurnish::FFrame F = Ctx.BestSoft(Ls, WallF, [&](const DCFurnish::FFrame& Cand) { return (float)(Adjacent(Cand) + Ctx.Jit(30 + Cand.Edge) * 4.f); }, 5.f, 35.f);
		float H2 = Ht;
		if (!F.bValid)
		{
			TArray<DCFurnish::FLocal> Low = { DCFurnish::FLocal::Solid(21.f, 0.f, 21.f, 56.f, 132.f) };
			F = Plan.Best(Low, WallPartF, [&](const DCFurnish::FFrame& Cand) { return (float)(Adjacent(Cand) + Ctx.Jit(30 + Cand.Edge) * 4.f); }, 5.f, TEXT("bag shelves low"));
			H2 = 120.f;
			if (!F.bValid)
			{
				// a knee-high bag rack: fits along any edge, even the open front
				const TArray<DCFurnish::FLocal> Knee = { DCFurnish::FLocal::Solid(21.f, 0.f, 21.f, 56.f, 84.f) };
				F = Plan.Best(Knee, Ctx.AnyWallF, [&](const DCFurnish::FFrame& Cand) { return (float)(Adjacent(Cand) + Ctx.Jit(30 + Cand.Edge) * 4.f); }, 5.f, TEXT("bag rack"));
				if (!F.bValid)
				{
					F = Plan.Best(Knee, FreeTurned({ 0.f, 90.f, 180.f, -90.f }), [&](const DCFurnish::FFrame& Cand) { return (float)(Ctx.FacingCam(Cand) * 10.f + Adjacent(Cand)); }, 6.f, TEXT("bag rack free"));
				}
				H2 = 78.f;
				Low = Knee;
			}
			if (F.bValid) { Ctx.Place(TEXT("bagshelves"), Low, F, [H2](const DCFurnish::FPen& P, int32 S) { DrawBagShelves(P, 112.f, H2, S); }, DCGroups::Level2); }
		}
		else { Ctx.PlaceSoft(TEXT("bagshelves"), Ls, F, [H2](const DCFurnish::FPen& P, int32 S) { DrawBagShelves(P, 112.f, H2, S); }, DCGroups::Level2); }
		if (F.bValid) { Ctx.Spot(F, 76.f, 0.f); }
	}
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(23.f, 0.f, 23.f, 48.f, 128.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, Ctx.AnyWallF, [&](const DCFurnish::FFrame& Cand)
		{
			return (float)(Adjacent(Cand) + (Board.bValid ? -FMath::Min(FV2::Distance(Cand.O, Board.O), 400.f) * 0.05f : 0.f) + Ctx.Jit(50 + Cand.Edge) * 4.f);
		}, 5.f, TEXT("radios"));
		if (F.bValid)
		{
			Ctx.Place(TEXT("radios"), Ls, F, [this](const DCFurnish::FPen& P, int32 S)
			{
				DrawRadioStation(P, 96.f, S);
				Point(P.P(40.f, 0.f, 110.f), FLinearColor(0.4f, 1.f, 0.55f), 2.5f, 160.f, false, true, 8.f);
			}, DCGroups::Level3);
			Ctx.Spot(F, 78.f, 0.f);
		}
	}

	// ---------------------------------------------------------------- 6. clutter and what's left of the walls
	Ctx.Clutter(0.4f);
	Ctx.WallDecor();
}
