// Director's Cut — the Lounge's furnishing recipe (see DCHideoutFurnish.cpp): bunks against the tall walls (camp cots
// where there are none), footlockers and lockers, somewhere to sit, a card table, and a TV corner once upgraded.
#include "World/Scene/DCHideoutFurnish.h"
#include "Components/PointLightComponent.h"

using namespace DCFurnish;

namespace DCFurnishLounge
{
	constexpr float KBedL = 200.f, KCotL = 176.f, KBedD = 92.f;

	FName BlanketFor(int32 Seed, int32 K)
	{
		static const FName Mats[] = { "Blanket", "BlanketGrey", "FabricOlive", "Velvet", "PaintRed" };
		return Mats[FMath::Clamp((int32)(U(Seed, K) * 4.99f), 0, 4)];
	}

	/** One made (or not so made) bed: mattress, blanket, pillow. Deck top at Z; the head is at -Y. */
	void DrawBedding(const FPen& P, float Z, float L, int32 Seed, int32 K)
	{
		const float CX = KBedD * 0.5f + 2.f;
		P.Box(CX, 0.f, Z + 6.f, KBedD - 8.f, L - 10.f, 12.f, "Linen");
		const float Pull = U(Seed, K + 1) * 26.f; // how far the blanket got pulled up
		P.Box(CX + J(Seed, K + 2) * 2.f, L * 0.17f - Pull * 0.5f, Z + 13.5f, KBedD - 4.f, L * 0.59f + Pull, 3.f, BlanketFor(Seed, K), J(Seed, K + 3) * 2.5f);
		P.Box(CX + J(Seed, K + 4) * 6.f, -L * 0.5f + 24.f, Z + 16.f, 52.f, 26.f, 8.f, "Enamel", J(Seed, K + 5) * 8.f);
		if (U(Seed, K + 6) > 0.55f) { P.Box(CX + 12.f, L * 0.5f - 22.f, Z + 17.f, 34.f, 26.f, 4.f, "Canvas", J(Seed, K + 7) * 20.f); } // folded kit at the foot
	}

	/** Two-tier timber bunk along a wall (origin on the wall face, long side on the wall). */
	void DrawBunk(const FPen& P, int32 Seed)
	{
		const float X0 = 5.f, X1 = KBedD - 1.f, HY = KBedL * 0.5f - 3.f;
		for (float X : { X0, X1 }) { for (float Y : { -HY, HY }) { P.Box(X, Y, 91.f, 7.f, 7.f, 182.f, "TimberDark"); } }
		for (float Z : { 36.f, 122.f })
		{
			for (float X : { X0, X1 }) { P.Box(X, 0.f, Z - 5.f, 5.f, KBedL - 8.f, 12.f, "TimberDark"); }
			P.Box((X0 + X1) * 0.5f, 0.f, Z - 2.f, X1 - X0, KBedL - 10.f, 4.f, "Plywood");
			for (float Y : { -HY, HY }) { P.Box((X0 + X1) * 0.5f, Y, Z + 12.f, X1 - X0, 4.f, 26.f, "Timber"); } // head and foot boards
		}
		DrawBedding(P, 36.f, KBedL, Seed, 10);
		DrawBedding(P, 122.f, KBedL, Seed, 30);
		P.Box(X1, 22.f, 146.f, 4.f, KBedL - 62.f, 5.f, "TimberDark"); // guard rail
		// ladder at the foot end
		const float LY = HY - 26.f;
		for (float S : { -1.f, 1.f }) { P.Rod(FVector(X1 + 14.f, LY + S * 14.f, 0.f), FVector(X1 + 3.f, LY + S * 14.f, 150.f), 3.2f, "SteelBlack"); }
		for (int32 i = 0; i < 4; ++i)
		{
			const float T = (i + 0.7f) / 4.6f;
			P.Rod(FVector(X1 + 14.f - 11.f * T, LY - 14.f, 150.f * T), FVector(X1 + 14.f - 11.f * T, LY + 14.f, 150.f * T), 2.6f, "SteelBlack");
		}
		if (U(Seed, 50) > 0.4f) { P.Box(X1 + 3.5f, -40.f, 118.f, 2.f, 40.f, 46.f, "FabricOlive", 0.f, 0.f, J(Seed, 51) * 4.f); } // a jacket over the rail
	}

	/** Low folding camp cot (same footprint as the bunk). */
	void DrawCot(const FPen& P, int32 Seed)
	{
		const float CX = KBedD * 0.5f + 2.f, HY = KCotL * 0.5f - 6.f;
		for (float Y : { -HY + 14.f, 0.f, HY - 14.f })
		{
			P.Rod(FVector(8.f, Y, 0.f), FVector(KBedD - 8.f, Y, 30.f), 3.f, "Steel");
			P.Rod(FVector(KBedD - 8.f, Y, 0.f), FVector(8.f, Y, 30.f), 3.f, "Steel");
		}
		for (float X : { 7.f, KBedD - 5.f }) { P.Rod(FVector(X, -HY, 31.f), FVector(X, HY, 31.f), 3.6f, "Steel"); }
		P.Box(CX, 0.f, 31.f, KBedD - 10.f, KCotL - 12.f, 2.f, "Canvas");
		DrawBedding(P, 30.f, KCotL, Seed, 10);
	}

	void DrawFootlocker(const FPen& P, int32 Seed)
	{
		const FName Mat = U(Seed, 1) > 0.5f ? FName("PaintOlive") : FName("TimberDark");
		P.Box(0.f, 0.f, 18.f, 38.f, 50.f, 36.f, Mat);
		P.Box(0.f, 0.f, 38.f, 40.f, 52.f, 5.f, Mat);
		for (float Y : { -19.f, 19.f }) { P.Box(0.f, Y, 20.f, 39.f, 3.f, 40.5f, "SteelBlack"); }
		P.Box(19.6f, 0.f, 31.f, 1.6f, 7.f, 8.f, "Brass");
		if (U(Seed, 2) > 0.5f) { P.Box(J(Seed, 3) * 4.f, J(Seed, 4) * 8.f, 43.f, 26.f, 30.f, 5.f, "Linen", J(Seed, 5) * 25.f); } // folded towel
	}

	/** A bank of N tall steel lockers (origin on the wall face). */
	void DrawLockers(const FPen& P, int32 N, int32 Seed)
	{
		const float DW = 34.f, W = N * DW;
		P.Box(23.f, 0.f, 4.f, 40.f, W - 4.f, 8.f, "SteelBlack");
		for (int32 i = 0; i < N; ++i)
		{
			const float Y = -W * 0.5f + DW * (i + 0.5f);
			P.Box(23.f, Y, 98.f, 44.f, DW - 1.f, 180.f, "PaintGrey");
			const bool bAjar = U(Seed, 10 + i) > 0.72f;
			const float Yaw = bAjar ? 28.f + U(Seed, 20 + i) * 30.f : 0.f;
			const FPen D = P.At(45.5f, Y - DW * 0.5f + 1.f, Yaw);
			D.Box(0.8f, DW * 0.5f - 1.f, 98.f, 1.6f, DW - 3.f, 174.f, U(Seed, 30 + i) > 0.5f ? FName("PaintOlive") : FName("PaintLight"));
			for (float Z : { 158.f, 164.f, 170.f }) { D.Box(1.8f, DW * 0.5f - 1.f, Z, 0.6f, DW - 14.f, 2.f, "SteelBlack", 0.f, 0.f, 0.f, false); }
			D.Box(2.2f, DW - 8.f, 100.f, 1.6f, 2.4f, 12.f, "Chrome");
			if (bAjar) { P.Box(24.f, Y, 120.f, 30.f, DW - 8.f, 50.f, "FabricOlive"); } // something hanging inside
		}
		if (U(Seed, 2) > 0.4f) { P.Box(20.f, J(Seed, 3) * W * 0.25f, 196.f, 30.f, 40.f, 16.f, "Cardboard", J(Seed, 4) * 10.f); }
	}

	/** Round card table (origin at its centre). */
	void DrawCardTable(const FPen& P, int32 Seed)
	{
		P.Cyl(0.f, 0.f, 2.f, 46.f, 4.f, "TimberDark");
		P.Cyl(0.f, 0.f, 37.f, 9.f, 68.f, "TimberDark");
		P.Cyl(0.f, 0.f, 72.f, 88.f, 5.f, "Walnut");
		P.Cyl(0.f, 0.f, 74.8f, 76.f, 0.8f, "Felt", 0.f, 0.f, false);
		for (int32 i = 0; i < 5; ++i)
		{
			const FV2 C = Dir(U(Seed, 10 + i) * 360.f) * (8.f + U(Seed, 20 + i) * 22.f);
			P.Box(C.X, C.Y, 75.5f, 6.f, 9.f, 0.5f, "Paper", U(Seed, 30 + i) * 180.f, 0.f, 0.f, false);
		}
		const FV2 B = Dir(U(Seed, 40) * 360.f) * 26.f;
		P.Cyl(B.X, B.Y, 86.f, 7.f, 22.f, "Bottle");
		P.Cyl(-B.X * 0.8f, -B.Y * 0.8f, 79.5f, 8.f, 9.f, "Enamel");
		P.Box(B.Y * 0.6f, -B.X * 0.6f, 76.5f, 12.f, 8.f, 2.4f, "Cash", U(Seed, 41) * 90.f);
	}

	/** A low unit with a TV and a radio (origin on the wall face). */
	void DrawTVUnit(const FPen& P, int32 Seed)
	{
		P.Box(23.f, 0.f, 24.f, 42.f, 118.f, 40.f, "Walnut");
		for (float Y : { -52.f, 52.f }) { P.Box(23.f, Y, 2.f, 36.f, 6.f, 4.f, "TimberDark"); }
		for (float Y : { -29.f, 29.f }) { P.Box(44.4f, Y, 24.f, 1.f, 54.f, 32.f, "TimberDark"); P.Ball(45.6f, Y + (Y < 0.f ? 20.f : -20.f), 26.f, 2.5f, 2.5f, 2.5f, "Brass"); }
		const float TY = -14.f + J(Seed, 1) * 4.f;
		P.Box(22.f, TY, 46.f, 20.f, 34.f, 3.f, "PlasticBlack");
		P.Box(20.f, TY, 51.f, 5.f, 8.f, 8.f, "PlasticBlack");
		P.Box(20.f, TY, 78.f, 5.f, 80.f, 48.f, "PlasticBlack", J(Seed, 2) * 3.f);
		P.Box(22.8f, TY, 78.f, 0.4f, 74.f, 42.f, "TV", J(Seed, 2) * 3.f, 0.f, 0.f, false);
		// radio
		P.Box(24.f, 42.f, 53.f, 14.f, 26.f, 16.f, "PaintRed", -8.f);
		P.Cyl(31.5f, 41.f, 53.f, 10.f, 1.f, "SteelBlack", 90.f, -8.f, false);
		P.Rod(FVector(22.f, 52.f, 60.f), FVector(16.f, 60.f, 92.f), 0.8f, "Chrome", false);
	}
}

void ADCHideoutBuilder::FurnishLounge(DCFurnish::FCtx& Ctx)
{
	using namespace DCFurnishLounge;
	FPlan& Plan = Ctx.Plan;
	const int32 Seed = Ctx.Seed;
	const FLinearColor Warm(1.f, 0.62f, 0.32f);

	Ctx.DefaultLamp();

	// Frames standing free on the floor, turned every way the room's axis allows.
	TArray<DCFurnish::FFrame> FreeYawF;
	for (const DCFurnish::FFrame& F0 : Ctx.FreeF)
	{
		for (float Y : { 0.f, 90.f, 180.f, 270.f }) { DCFurnish::FFrame F = F0; F.Yaw = Plan.RoomYaw + Y; F.Edge = -1; FreeYawF.Add(F); }
	}

	// ---------------------------------------------------------------- 1. somewhere to sleep
	// Two-tier bunks along a tall wall; where no wall can take one, a low cot against whatever edge has room, and
	// failing that a cot out on the floor.
	const float BX = KBedD * 0.5f + 2.f;
	// (backs stay a few cm off the wall line: a box touching it counts as outside the room)
	const TArray<FLocal> BunkLs = { FLocal::Solid(BX + 1.f, 0.f, KBedD * 0.5f, KBedL * 0.5f + 2.f, 184.f), FLocal::Space(KBedD + 24.f, 0.f, 20.f, 40.f, 60.f) };
	const TArray<FLocal> CotLs = { FLocal::Solid(BX + 1.f, 0.f, KBedD * 0.5f, KCotL * 0.5f + 2.f, 56.f), FLocal::Space(KBedD + 24.f, 0.f, 20.f, 36.f, 56.f) };
	const TArray<FLocal> CotFreeLs = { FLocal::Solid(BX, 0.f, KBedD * 0.5f, KCotL * 0.5f + 2.f, 56.f) };
	TArray<DCFurnish::FFrame> Beds;
	auto BedScore = [&](const DCFurnish::FFrame& F) -> float
	{
		// rows read best: beside a bed that's already there, else tucked towards a back corner
		float Near = 400.f;
		for (const DCFurnish::FFrame& B : Beds) { Near = FMath::Min(Near, (float)FV2::Distance(B.O, F.O)); }
		const float Back = (float)FV2::DotProduct(F.O - Plan.Hub, -KToCam);
		return (Beds.Num() > 0 ? -FMath::Abs(Near - (KBedL + 30.f)) * 0.06f : 0.f) - Plan.CornerDist(F.O + Dir(F.Yaw) * BX) * 0.05f + Back * 0.04f
			+ (F.Edge >= 0 ? Ctx.Jit(F.Edge) * 4.f : 0.f);
	};
	auto AddBed = [&](FName Group) -> bool
	{
		bool bCot = false, bFree = false;
		DCFurnish::FFrame F = Ctx.BestSoft(BunkLs, Ctx.WallF, BedScore, 6.f, 45.f);
		if (F.bValid)
		{
			Ctx.PlaceSoft(TEXT("bunk"), BunkLs, F, [](const FPen& P, int32 S) { DrawBunk(P, S); }, Group);
		}
		else
		{
			F = Plan.Best(CotLs, Ctx.WallPartF, BedScore, 6.f, TEXT("cot"));
			if (!F.bValid) { F = Plan.Best(CotLs, Ctx.AnyWallF, BedScore, 6.f); }
			if (!F.bValid)
			{
				bFree = true;
				F = Plan.Best(CotFreeLs, FreeYawF, [&](const DCFurnish::FFrame& Cand) -> float
				{
					const FV2 Mid = Cand.O + Dir(Cand.Yaw) * BX;
					if (FV2::Distance(Mid, Plan.Hub) < 130.f) { return -1e9f; } // not across the middle of the room
					return BedScore(Cand) - FMath::Min(Plan.NearestSolid(Mid), 200.f) * 0.03f;
				}, 8.f, TEXT("cot free"));
			}
			if (!F.bValid) { return false; }
			bCot = true;
			Ctx.Place(TEXT("cot"), bFree ? CotFreeLs : CotLs, F, [](const FPen& P, int32 S) { DrawCot(P, S); }, Group);
		}
		if (Group == NAME_None) { Ctx.Spot(F, KBedD + 30.f, -20.f); }
		Beds.Add(F);
		// a footlocker at one end of it, whichever has room
		const TArray<FLocal> LL = { FLocal::Solid(0.f, 0.f, 21.f, 27.f, 46.f) };
		for (float S : { 1.f, -1.f })
		{
			DCFurnish::FFrame L = F; L.O = F.O + Rot(FV2(26.f, S * ((bCot ? KCotL : KBedL) * 0.5f + 32.f)), F.Yaw); L.Edge = -1;
			if (Plan.Fits(LL, L, 2.f)) { Ctx.Place(TEXT("footlocker"), LL, L, [](const FPen& P, int32 Sd) { DrawFootlocker(P, Sd); }, Group); break; }
		}
		return true;
	};
	AddBed(NAME_None);
	AddBed(NAME_None);

	// ---------------------------------------------------------------- 2. lockers on a tall wall
	bool bLockers = false;
	for (int32 N : { 3, 2 })
	{
		const TArray<FLocal> Ls = { FLocal::Solid(25.f, 0.f, 23.f, N * 17.f + 1.f, 200.f), FLocal::Space(64.f, 0.f, 16.f, N * 17.f - 6.f, 60.f) };
		const DCFurnish::FFrame F = Ctx.BestSoft(Ls, Ctx.WallF, [&](const DCFurnish::FFrame& Cand) -> float
		{
			return (float)(-FMath::Min(Plan.NearestSolid(Cand.O), 200.f) * 0.12f - Plan.CornerDist(Cand.O) * 0.04f + Ctx.Jit(20 + Cand.Edge) * 4.f);
		}, 5.f, 40.f);
		if (!F.bValid) { continue; }
		Ctx.PlaceSoft(TEXT("lockers"), Ls, F, [N](const FPen& P, int32 S) { DrawLockers(P, N, S); });
		Ctx.Spot(F, 72.f, 0.f);
		bLockers = true;
		break;
	}
	if (!bLockers)
	{
		// no tall wall: a pair of footlockers against whatever edge has room
		const TArray<FLocal> Ls = { FLocal::Solid(24.f, 0.f, 21.f, 54.f, 46.f), FLocal::Space(70.f, 0.f, 16.f, 30.f, 50.f) };
		DCFurnish::FFrame F = Plan.Best(Ls, Ctx.WallPartF, [&](const DCFurnish::FFrame& Cand) -> float
		{
			return (float)(-FMath::Min(Plan.NearestSolid(Cand.O), 200.f) * 0.1f - Plan.CornerDist(Cand.O) * 0.03f + Ctx.Jit(20 + Cand.Edge) * 4.f);
		}, 5.f, TEXT("footlockers"));
		if (!F.bValid) { F = Plan.Best(Ls, Ctx.AnyWallF, [&](const DCFurnish::FFrame& Cand) -> float { return (float)(-FMath::Min(Plan.NearestSolid(Cand.O), 200.f) * 0.1f); }, 5.f); }
		if (F.bValid)
		{
			Ctx.Place(TEXT("footlockers"), Ls, F, [](const FPen& P, int32 S)
			{
				DrawFootlocker(P.At(24.f, -27.f, J(S, 7) * 4.f), S);
				DrawFootlocker(P.At(24.f, 27.f, J(S, 8) * 4.f), S + 1);
			});
			Ctx.Spot(F, 76.f, 0.f);
		}
	}

	// ---------------------------------------------------------------- 3. somewhere to sit: a sofa group, else armchairs
	DCFurnish::FFrame Sofa;
	float SofaW = 0.f;
	for (int32 Pass = 0; Pass < 2 && !Sofa.bValid; ++Pass)
	{
		for (float W : { 190.f, 160.f })
		{
			TArray<FLocal> Ls = { FLocal::Solid(46.f, 0.f, 44.f, W * 0.5f, 86.f), FLocal::Space(110.f, 0.f, 20.f, W * 0.5f - 24.f, 40.f) };
			if (Pass == 0) { Ls.Add(FLocal::Solid(158.f, 0.f, 28.f, 50.f, 46.f)); }
			// backed onto a wall or partition, else onto the open front with its back to the camera's side
			for (const TArray<DCFurnish::FFrame>* Fr : { &Ctx.WallPartF, &Ctx.AnyWallF })
			{
				Sofa = Plan.Best(Ls, *Fr, [&](const DCFurnish::FFrame& Cand) -> float
				{
					return (float)(Ctx.FacingHub(Cand) * 10.f + Ctx.Jit(40 + Cand.Edge) * 5.f - FMath::Abs(Cand.T - Plan.E[Cand.Edge].L * 0.5f) * 0.03f);
				}, 6.f, TEXT("sofa"));
				if (Sofa.bValid) { break; }
			}
			if (!Sofa.bValid) { continue; }
			SofaW = W;
			Ctx.Place(TEXT("sofa"), { Ls[0], Ls[1] }, Sofa, [W](const FPen& P, int32 S) { DrawSofa(P, W, U(S, 3) > 0.5f ? FName("Velvet") : FName("LeatherBrn"), S); });
			Ctx.Spot(Sofa, 112.f, 0.f);
			if (Pass == 0)
			{
				DCFurnish::FFrame CT = Sofa; CT.O = Sofa.O + Rot(FV2(158.f, 0.f), Sofa.Yaw); CT.Edge = -1;
				Ctx.Place(TEXT("coffee"), { FLocal::Solid(0.f, 0.f, 28.f, 50.f, 46.f) }, CT, [](const FPen& P, int32 S) { DrawCoffeeTable(P, 100.f, 56.f, S); });
			}
			const float Deep = Pass == 0 ? 92.f : 62.f;
			Ctx.PlaceRug(Sofa, 46.f + Deep, Deep * 2.f, W + 56.f, "Rug");
			break;
		}
	}
	TArray<DCFurnish::FFrame> Chairs;
	if (!Sofa.bValid)
	{
		for (int32 n = 0; n < 2; ++n)
		{
			TArray<DCFurnish::FFrame> Fr;
			for (const DCFurnish::FFrame& W0 : Ctx.AnyWallF)
			{
				DCFurnish::FFrame F = W0; F.O = W0.O + Dir(W0.Yaw) * 46.f; F.Edge = -1;
				F.Yaw = W0.Yaw + FMath::Clamp(FMath::FindDeltaAngleDegrees(W0.Yaw, YawOf(Plan.Hub - F.O)), -25.f, 25.f);
				Fr.Add(F);
			}
			const TArray<FLocal> Ls = { FLocal::Solid(0.f, 0.f, 42.f, 42.f, 86.f) };
			const DCFurnish::FFrame F = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& Cand) -> float
			{
				float Pair = 0.f;
				for (const DCFurnish::FFrame& O : Chairs) { Pair = -FMath::Abs((float)FV2::Distance(O.O, Cand.O) - 120.f) * 0.1f; }
				return (float)(Ctx.FacingHub(Cand) * 6.f + Ctx.FacingCam(Cand) * 4.f + Pair - FMath::Min(Plan.NearestSolid(Cand.O), 150.f) * 0.04f + J(Seed, 640 + n) * 3.f);
			}, 10.f, TEXT("armchair"));
			if (!F.bValid) { break; }
			Ctx.Place(TEXT("armchair"), Ls, F, [](const FPen& P, int32 S) { DrawArmchair(P, U(S, 4) > 0.5f ? FName("Velvet") : FName("LeatherBrn"), S); });
			Chairs.Add(F);
		}
		if (Chairs.Num() > 0) { Ctx.Spot(Chairs[0], 70.f, 0.f); }
	}
	// a lamp beside the seating
	{
		TArray<DCFurnish::FFrame> Fr;
		if (Sofa.bValid) { for (float S : { 1.f, -1.f }) { DCFurnish::FFrame F = Sofa; F.O = Sofa.O + Rot(FV2(30.f, S * (SofaW * 0.5f + 24.f)), Sofa.Yaw); F.Edge = -1; Fr.Add(F); } }
		for (const DCFurnish::FFrame& A : Chairs) { for (float S : { 1.f, -1.f }) { DCFurnish::FFrame F = A; F.O = A.O + Rot(FV2(-18.f, S * 64.f), A.Yaw); Fr.Add(F); } }
		const TArray<FLocal> Ls = { FLocal::Solid(0.f, 0.f, 18.f, 18.f, 84.f) }; // slim: reads fine in front of anything
		DCFurnish::FFrame F = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& Cand) -> float { return (float)(-FV2::DotProduct(Cand.O - Plan.Hub, KToCam) * 0.05f); }, 3.f);
		if (!F.bValid)
		{
			// else in a corner by the wall, as near the seating as it gets
			const FV2 Seat = Sofa.bValid ? Sofa.O : (Chairs.Num() > 0 ? Chairs[0].O : Plan.Hub);
			TArray<DCFurnish::FFrame> WF;
			for (const DCFurnish::FFrame& W0 : Ctx.AnyWallF) { DCFurnish::FFrame W1 = W0; W1.O = W0.O + Dir(W0.Yaw) * 22.f; W1.Edge = -1; WF.Add(W1); }
			F = Plan.Best(Ls, WF, [&](const DCFurnish::FFrame& Cand) -> float { return (float)(-FV2::Distance(Cand.O, Seat) * 0.05f - Plan.CornerDist(Cand.O) * 0.02f); }, 3.f, TEXT("lamp"));
		}
		if (F.bValid)
		{
			Ctx.Place(TEXT("lamp"), Ls, F, [this, Warm](const FPen& P, int32)
			{
				DrawFloorLamp(P);
				Point(P.P(0.f, 0.f, 150.f), Warm, 12.f, 400.f, false, true, 10.f);
			});
		}
	}

	// ---------------------------------------------------------------- 4. the card table
	{
		// chairs sit square to the table's box (it's square, the top is round), far enough out to clear its corners
		const TArray<FLocal> Ls = { FLocal::Solid(0.f, 0.f, 44.f, 44.f, 78.f) };
		const TArray<FLocal> CL = { FLocal::Solid(0.f, 0.f, 23.f, 23.f, 70.f) };
		auto ChairAt = [&](const DCFurnish::FFrame& T, int32 i) -> DCFurnish::FFrame
		{
			const float A = T.Yaw + i * 90.f + J(Seed, 320 + i) * 8.f;
			DCFurnish::FFrame F = T; F.O = T.O + Dir(A) * (78.f + U(Seed, 330 + i) * 6.f); F.Yaw = A + 180.f + J(Seed, 340 + i) * 12.f; F.Edge = -1;
			return F;
		};
		const DCFurnish::FFrame Table = Plan.Best(Ls, Ctx.FreeF, [&](const DCFurnish::FFrame& Cand) -> float
		{
			const float Clear = Plan.NearestSolid(Cand.O);
			if (Clear < 60.f) { return -1e9f; }
			int32 Fit = 0;
			Plan.CapScale = 0.6f; Plan.bHubKeep = false; // judged as the chairs will be placed
			for (int32 i = 0; i < 4; ++i) { Fit += Plan.Fits(CL, ChairAt(Cand, i), 3.f) ? 1 : 0; }
			Plan.CapScale = 1.f; Plan.bHubKeep = true;
			if (Fit < 1) { return -1e9f; } // a seat at it at least
			return (float)(Fit * 4.f - FMath::Abs(FV2::Distance(Cand.O, Plan.Hub) - 150.f) * 0.08f - FMath::Min(Clear, 220.f) * 0.03f + J(Seed, 300 + (int32)Cand.O.X) * 2.f);
		}, 20.f, TEXT("card table"));
		if (Table.bValid)
		{
			Ctx.Place(TEXT("cardtable"), Ls, Table, [](const FPen& P, int32 S) { DrawCardTable(P, S); });
			const int32 I0 = (int32)(U(Seed, 310) * 3.99f);
			int32 NSeats = 0;
			Plan.CapScale = 0.6f; Plan.bHubKeep = false; // a chair may stand in the way a little
			for (int32 k = 0; k < 4 && NSeats < 3; ++k)
			{
				const int32 i = (I0 + k) & 3;
				const DCFurnish::FFrame F = ChairAt(Table, i);
				const float A = Table.Yaw + i * 90.f;
				if (!Plan.Fits(CL, F, 3.f)) { continue; }
				Ctx.Place(TEXT("chair"), CL, F, [](const FPen& P, int32 S) { DrawWoodChair(P, S); });
				if (NSeats == 0) { DCFurnish::FFrame Sp = F; Sp.Yaw = A; Sp.O = Table.O; Ctx.Spot(Sp, 104.f, 34.f); }
				++NSeats;
			}
			Plan.CapScale = 1.f; Plan.bHubKeep = true;
		}
	}

	// ---------------------------------------------------------------- 5. upgrades: another bed, then a TV corner
	if (!AddBed(DCGroups::Level2))
	{
		// no room for another bed: an extra footlocker by the wall
		const TArray<FLocal> LL = { FLocal::Solid(24.f, 0.f, 21.f, 27.f, 46.f) };
		const FV2 Near = Beds.Num() > 0 ? Beds[0].O : Plan.Hub;
		const DCFurnish::FFrame F = Plan.Best(LL, Ctx.AnyWallF, [&](const DCFurnish::FFrame& Cand) -> float
		{
			return (float)(-FV2::Distance(Cand.O, Near) * 0.02f - FMath::Min(Plan.NearestSolid(Cand.O), 200.f) * 0.06f);
		}, 4.f, TEXT("footlocker L2"));
		if (F.bValid) { Ctx.Place(TEXT("footlocker"), LL, F, [](const FPen& P, int32 S) { DrawFootlocker(P.At(24.f, 0.f), S); }, DCGroups::Level2); }
	}
	{
		const TArray<FLocal> Ls = { FLocal::Solid(25.f, 0.f, 23.f, 61.f, 104.f), FLocal::Space(64.f, 0.f, 16.f, 40.f, 50.f) };
		const FV2 Want = Sofa.bValid ? Sofa.O + Dir(Sofa.Yaw) * 260.f : (Chairs.Num() > 0 ? Chairs[0].O + Dir(Chairs[0].Yaw) * 220.f : Plan.Hub);
		const DCFurnish::FFrame F = Plan.Best(Ls, Ctx.WallPartF, [&](const DCFurnish::FFrame& Cand) -> float
		{
			return (float)(-FV2::Distance(Cand.O, Want) * 0.05f + Ctx.FacingCam(Cand) * 6.f + Ctx.Jit(60 + Cand.Edge) * 3.f);
		}, 5.f, TEXT("tv"));
		if (F.bValid)
		{
			Ctx.Place(TEXT("tv"), Ls, F, [this](const FPen& P, int32 S)
			{
				DrawTVUnit(P, S);
				Point(P.P(60.f, -14.f, 80.f), FLinearColor(0.4f, 0.55f, 0.9f), 5.f, 260.f, false, true, 12.f);
			}, DCGroups::Level3);
		}
		else
		{
			// no wall for the set: a second armchair by the beds
			const TArray<FLocal> AL = { FLocal::Solid(0.f, 0.f, 42.f, 42.f, 86.f) };
			TArray<DCFurnish::FFrame> Fr;
			for (const DCFurnish::FFrame& W0 : Ctx.AnyWallF) { DCFurnish::FFrame A = W0; A.O = W0.O + Dir(W0.Yaw) * 46.f; A.Edge = -1; Fr.Add(A); }
			const DCFurnish::FFrame A = Plan.Best(AL, Fr, [&](const DCFurnish::FFrame& Cand) -> float
			{
				return (float)(Ctx.FacingHub(Cand) * 6.f + Ctx.FacingCam(Cand) * 4.f - FMath::Min(Plan.NearestSolid(Cand.O), 150.f) * 0.04f);
			}, 10.f, TEXT("armchair L3"));
			if (A.bValid) { Ctx.Place(TEXT("armchair"), AL, A, [](const FPen& P, int32 S) { DrawArmchair(P, FName("LeatherBrn"), S); }, DCGroups::Level3); }
			else
			{
				// last resort: a radio on an upended crate
				const TArray<FLocal> RL = { FLocal::Solid(22.f, 0.f, 19.f, 22.f, 80.f) };
				const DCFurnish::FFrame R = Plan.Best(RL, Ctx.AnyWallF, [&](const DCFurnish::FFrame& Cand) -> float
				{
					return (float)(-FMath::Min(Plan.NearestSolid(Cand.O), 200.f) * 0.06f + Ctx.Jit(70 + Cand.Edge) * 3.f);
				}, 4.f, TEXT("radio L3"));
				if (R.bValid)
				{
					Ctx.Place(TEXT("radio"), RL, R, [](const FPen& P, int32 S)
					{
						P.Box(22.f, 0.f, 28.f, 36.f, 42.f, 56.f, "Timber", J(S, 1) * 5.f);
						P.Box(22.f, 0.f, 64.f, 14.f, 26.f, 16.f, "PaintRed", J(S, 2) * 10.f);
						P.Cyl(29.5f, -1.f, 64.f, 10.f, 1.f, "SteelBlack", 90.f, J(S, 2) * 10.f, false);
						P.Rod(FVector(20.f, 10.f, 71.f), FVector(14.f, 18.f, 100.f), 0.8f, "Chrome", false);
					}, DCGroups::Level3);
				}
			}
		}
	}

	// ---------------------------------------------------------------- 6. the shared steps
	Ctx.Runner();
	Ctx.Clutter();
	Ctx.WallDecor();
}
