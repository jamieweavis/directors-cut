// Director's Cut — the Lawyer's furnishing recipe (see DCHideoutFurnish.cpp): a partner's desk with a green lamp, a
// high-backed chair behind it and client chairs in front, law books, filing cabinets, boxes of case files, somewhere
// for clients to wait, and certificates on the wall. Tidier and greener than the Director's office.
#include "World/Scene/DCHideoutFurnish.h"
#include "Components/PointLightComponent.h"

using namespace DCFurnish;

namespace DCFurnishLawyer
{
	// origin: desk centre. Sitter on -X, the clients (and the room) on +X.
	void DrawPartnerDesk(const FPen& P, float W, float D, int32 Seed)
	{
		const float HW = W * 0.5f, HD = D * 0.5f;
		P.Box(0.f, 0.f, 77.f, D + 4.f, W + 4.f, 5.f, "TimberDark");
		P.Box(0.f, 0.f, 79.7f, D * 0.66f, W * 0.62f, 0.4f, "Velvet", 0.f, 0.f, 0.f, false); // green leather inlay
		for (float S : { -1.f, 1.f })
		{
			const float Y = S * (HW - 23.f);
			P.Box(0.f, Y, 37.f, D - 4.f, 44.f, 74.f, "Walnut");
			P.Box(0.f, Y, 2.f, D - 1.f, 46.f, 4.f, "TimberDark");
			for (int32 k = 0; k < 3; ++k)
			{
				const float Z = 16.f + k * 23.f;
				P.Box(-HD + 1.6f, Y, Z, 1.2f, 38.f, 19.f, "Wood");
				P.Cyl(-HD + 0.4f, Y, Z + 2.f, 3.f, 2.f, "Brass", 90.f);
				P.Box(HD - 1.6f, Y, Z, 1.2f, 38.f, 19.f, "Wood"); // panelled front
			}
		}
		P.Box(HD - 5.f, 0.f, 46.f, 3.f, W - 90.f, 56.f, "Walnut");
		// banker's lamp
		const FPen L = P.At(HD * 0.3f, HW - 24.f, 0.f);
		L.Cyl(0.f, 0.f, 80.5f, 15.f, 2.f, "Brass");
		L.Cyl(0.f, 0.f, 95.f, 2.2f, 28.f, "Brass");
		L.Box(-4.f, 0.f, 110.f, 14.f, 34.f, 8.f, "PaintGreen", 0.f, -8.f);
		L.Box(-4.f, 0.f, 105.8f, 11.f, 30.f, 0.4f, "Tungsten", 0.f, -8.f, 0.f, false);
		// the brief being read, a stack of files, a nameplate, a telephone, a pen stand
		P.Box(-HD * 0.25f, 0.f, 80.4f, 30.f, 42.f, 1.f, "Paper", J(Seed, 1) * 5.f);
		P.Box(-HD * 0.25f + 2.f, 4.f, 81.2f, 22.f, 30.f, 0.6f, "Cardboard", J(Seed, 2) * 9.f);
		const int32 N = 3 + (int32)(U(Seed, 3) * 3.f);
		for (int32 i = 0; i < N; ++i) { P.Box(HD * 0.1f + J(Seed, 10 + i) * 1.5f, -HW + 26.f, 81.f + i * 2.4f, 24.f, 32.f, 2.2f, i % 2 ? FName("Cardboard") : FName("Paper"), J(Seed, 20 + i) * 7.f); }
		P.Box(HD - 9.f, 0.f, 82.5f, 3.f, 30.f, 6.f, "Brass", 0.f, 20.f);
		P.Box(-HD * 0.1f, HW * 0.42f, 82.f, 16.f, 20.f, 6.f, "PlasticBlack", -20.f);
		P.Box(-HD * 0.1f, HW * 0.42f, 86.f, 6.f, 20.f, 3.f, "PlasticBlack", -20.f);
		P.Box(HD * 0.45f, -HW * 0.35f, 81.f, 8.f, 14.f, 3.f, "Walnut");
		for (float S : { -1.f, 1.f }) { P.Rod(FVector(HD * 0.45f, -HW * 0.35f + S * 4.f, 82.f), FVector(HD * 0.45f - 3.f, -HW * 0.35f + S * 6.f, 96.f), 0.9f, S < 0.f ? FName("Black") : FName("Brass")); }
	}

	// origin: seat centre, facing +X
	void DrawHighBack(const FPen& P)
	{
		for (int32 i = 0; i < 5; ++i)
		{
			const FV2 D = Dir(i * 72.f + 20.f) * 26.f;
			P.Rod(FVector(0.f, 0.f, 8.f), FVector(D.X, D.Y, 4.f), 3.f, "Brass");
		}
		P.Cyl(0.f, 0.f, 24.f, 5.f, 34.f, "Brass");
		P.Box(2.f, 0.f, 46.f, 54.f, 58.f, 12.f, "Leather");
		P.Box(-24.f, 0.f, 92.f, 12.f, 56.f, 86.f, "Leather", 0.f, -6.f);
		P.Box(-19.f, 0.f, 128.f, 4.f, 40.f, 12.f, "Velvet", 0.f, -6.f); // headrest
		for (float S : { -1.f, 1.f })
		{
			P.Box(-2.f, S * 30.f, 66.f, 40.f, 6.f, 5.f, "Walnut");
			P.Box(14.f, S * 30.f, 58.f, 5.f, 5.f, 14.f, "Walnut");
		}
		for (float Z : { 82.f, 106.f }) { for (float S : { -1.f, 1.f }) { P.Ball(-16.5f, S * 14.f, Z, 3.f, 3.f, 3.f, "Brass"); } } // buttons
	}

	// origin: seat centre, facing +X. A square leather tub chair on wooden legs.
	void DrawClientChair(const FPen& P, int32 Seed)
	{
		for (int32 i = 0; i < 4; ++i) { P.Box((i % 2) ? 19.f : -19.f, (i / 2) ? 19.f : -19.f, 20.f, 4.f, 4.f, 40.f, "Walnut"); }
		P.Box(0.f, 0.f, 43.f, 46.f, 46.f, 9.f, "LeatherBrn", J(Seed, 1) * 2.f);
		P.Box(-20.f, 0.f, 64.f, 7.f, 46.f, 36.f, "LeatherBrn", 0.f, -6.f);
		for (float S : { -1.f, 1.f }) { P.Box(-4.f, S * 22.f, 58.f, 34.f, 5.f, 4.f, "Walnut"); P.Box(12.f, S * 22.f, 50.f, 4.f, 4.f, 14.f, "Walnut"); }
	}

	// origin: on the wall, boxes stand out to +X
	void DrawArchiveBoxes(const FPen& P, int32 N, int32 Seed)
	{
		static const FV2 At[] = { FV2(22.f, -20.f), FV2(22.f, 20.f), FV2(23.f, -2.f), FV2(22.f, 22.f) };
		static const float Zs[] = { 0.f, 0.f, 27.f, 27.f };
		for (int32 i = 0; i < FMath::Min(N, 4); ++i)
		{
			const float Tw = i == 3 ? 90.f + J(Seed, 30) * 6.f : J(Seed, i) * 7.f;
			const FPen B = P.At(At[i].X + J(Seed, 10 + i) * 2.f, At[i].Y, Tw, Zs[i]);
			B.Box(0.f, 0.f, 12.f, 40.f, 34.f, 24.f, "Cardboard");
			B.Box(0.f, 0.f, 24.8f, 42.f, 36.f, 4.f, i % 2 ? FName("Paper") : FName("Cardboard")); // lid
			B.Box(20.3f, 0.f, 13.f, 0.4f, 20.f, 9.f, "Paper", 0.f, 0.f, 0.f, false);              // label
			B.Box(20.5f, 0.f, 19.5f, 0.8f, 9.f, 2.f, "Black", 0.f, 0.f, 0.f, false);               // hand hole
		}
		if (N > 4)
		{
			// loose files on top
			for (int32 i = 0; i < 3; ++i) { P.Box(22.f, -18.f, 56.5f + i * 2.f, 24.f, 31.f, 1.8f, i == 1 ? FName("BookGreen") : FName("Paper"), J(Seed, 40 + i) * 9.f); }
		}
	}

	// origin: on the wall. A low cabinet with law books and a decanter (bDrinks: a tray of bottles instead).
	void DrawCredenza(const FPen& P, float W, bool bDrinks, int32 Seed)
	{
		const float D = 44.f, X = D * 0.5f + 1.f;
		for (float S : { -1.f, 1.f }) { for (float LX : { 6.f, D - 4.f }) { P.Box(LX, S * (W * 0.5f - 6.f), 5.f, 5.f, 5.f, 10.f, "Brass"); } }
		P.Box(X, 0.f, 40.f, D, W, 60.f, "Walnut");
		P.Box(X, 0.f, 71.5f, D + 3.f, W + 3.f, 3.f, "TimberDark");
		const int32 Doors = W > 110.f ? 4 : 2;
		for (int32 i = 0; i < Doors; ++i)
		{
			const float Y = (i - (Doors - 1) * 0.5f) * (W / Doors);
			P.Box(D + 1.4f, Y, 40.f, 1.2f, W / Doors - 4.f, 52.f, i % 2 ? FName("Wood") : FName("PaintGreen"));
			P.Ball(D + 2.6f, Y + ((i % 2) ? -1.f : 1.f) * (W / Doors * 0.5f - 6.f), 42.f, 2.5f, 2.5f, 2.5f, "Brass");
		}
		if (bDrinks)
		{
			P.Box(X, -W * 0.18f, 73.8f, 30.f, 46.f, 1.6f, "Brass");
			for (int32 i = 0; i < 3; ++i)
			{
				const float Y = -W * 0.18f + (i - 1) * 13.f, Hh = 20.f + U(Seed, 50 + i) * 8.f;
				P.Cyl(X + J(Seed, 60 + i) * 4.f, Y, 74.6f + Hh * 0.5f, 8.f, Hh, i == 1 ? FName("Glass") : FName("Bottle"));
				P.Cyl(X + J(Seed, 60 + i) * 4.f, Y, 76.f + Hh, 3.f, 5.f, "Brass");
			}
			for (int32 i = 0; i < 2; ++i) { P.Cyl(X + 9.f, W * 0.12f + i * 9.f, 77.f, 6.f, 8.f, "Glass"); }
		}
		else
		{
			static const FName Books[] = { "BookGreen", "BookRed", "BookGreen", "BookTan", "BookGreen", "LeatherBrn", "BookBlue" };
			float Y = -W * 0.5f + 8.f;
			for (int32 k = 0; Y < W * 0.1f; ++k)
			{
				const float T = 5.f + U(Seed, 70 + k) * 3.5f;
				P.Box(X - 4.f, Y + T * 0.5f, 73.f + 13.f + J(Seed, 90 + k) * 1.5f, 22.f, T, 26.f + J(Seed, 90 + k) * 3.f, Books[(k + Seed) % 7]);
				Y += T + 0.4f;
			}
			P.Box(X - 4.f, Y + 3.f, 84.f, 22.f, 6.f, 24.f, "Brass", 0.f, 0.f, 12.f); // bookend
			P.Cyl(X + 2.f, W * 0.32f, 84.f, 10.f, 22.f, "Bottle");
			P.Cyl(X + 2.f, W * 0.32f, 97.5f, 3.5f, 5.f, "Glass");
		}
	}

	void DrawCoatStand(const FPen& P, int32 Seed)
	{
		P.Cyl(0.f, 0.f, 2.f, 34.f, 4.f, "TimberDark");
		P.Cyl(0.f, 0.f, 90.f, 3.5f, 176.f, "Walnut");
		P.Ball(0.f, 0.f, 180.f, 6.f, 6.f, 6.f, "Brass");
		for (int32 i = 0; i < 4; ++i)
		{
			const FV2 D = Dir(i * 90.f + 30.f);
			P.Rod(FVector(D.X * 2.f, D.Y * 2.f, 158.f), FVector(D.X * 14.f, D.Y * 14.f, 170.f), 1.6f, "Brass");
		}
		// an overcoat and a hat
		const FV2 C = Dir(30.f + ((Seed & 1) ? 90.f : 0.f)) * 13.f;
		P.Box(C.X, C.Y, 128.f, 9.f, 26.f, 70.f, "Fabric", 30.f + ((Seed & 1) ? 90.f : 0.f));
		P.Cyl(0.f, 0.f, 186.f, 26.f, 1.5f, "Felt");
		P.Cyl(0.f, 0.f, 191.f, 15.f, 9.f, "Felt");
	}

	void DrawGlobe(const FPen& P)
	{
		for (int32 i = 0; i < 3; ++i)
		{
			const FV2 D = Dir(i * 120.f + 15.f) * 17.f;
			P.Rod(FVector(D.X, D.Y, 0.f), FVector(0.f, 0.f, 40.f), 2.4f, "Walnut");
		}
		P.Cyl(0.f, 0.f, 44.f, 30.f, 3.f, "Walnut");
		P.Ball(0.f, 0.f, 62.f, 34.f, 34.f, 34.f, "PaintBlue");
		P.Ball(5.f, -6.f, 66.f, 26.f, 24.f, 24.f, "PaintGreen");
		P.Ball(-7.f, 7.f, 56.f, 22.f, 20.f, 22.f, "BookTan");
		P.Cyl(0.f, 0.f, 62.f, 37.f, 1.4f, "Brass", 0.f, 0.f, false);
		P.Rod(FVector(0.f, 0.f, 78.f), FVector(0.f, 0.f, 83.f), 1.6f, "Brass");
	}

	// a small round table with magazines and an ashtray
	void DrawSideTable(const FPen& P, int32 Seed)
	{
		P.Cyl(0.f, 0.f, 1.5f, 24.f, 3.f, "TimberDark");
		P.Cyl(0.f, 0.f, 24.f, 4.f, 44.f, "Walnut");
		P.Cyl(0.f, 0.f, 47.f, 44.f, 3.f, "Walnut");
		for (int32 i = 0; i < 3; ++i) { P.Box(-4.f, -3.f, 49.2f + i * 1.2f, 18.f, 25.f, 1.f, i == 1 ? FName("BookGreen") : FName("Paper"), J(Seed, i) * 25.f); }
		P.Cyl(10.f, 12.f, 50.f, 9.f, 2.4f, "Glass");
	}

	// origin: on the wall. Framed certificates in a row, each with a seal.
	void DrawCertificates(const FPen& P, float W, int32 Seed)
	{
		const int32 N = W > 90.f ? 3 : 2;
		for (int32 i = 0; i < N; ++i)
		{
			const float Y = (i - (N - 1) * 0.5f) * (W / N);
			const bool bWide = i == N / 2;
			const float FW = bWide ? 38.f : 28.f, FH = bWide ? 30.f : 36.f, Z = 164.f + J(Seed, i) * 3.f;
			P.Box(1.5f, Y, Z, 3.f, FW + 6.f, FH + 6.f, bWide ? FName("Brass") : FName("TimberDark"));
			P.Box(3.1f, Y, Z, 0.4f, FW, FH, "Paper");
			for (int32 k = 0; k < 3; ++k) { P.Box(3.4f, Y, Z + FH * 0.26f - k * 5.f, 0.3f, FW * (k ? 0.7f : 0.5f), 1.4f, "Black", 0.f, 0.f, 0.f, false); }
			P.Cyl(3.5f, Y + FW * 0.28f, Z - FH * 0.3f, 6.f, 0.6f, i % 2 ? FName("PaintGreen") : FName("PaintRed"), 90.f, 0.f, false);
		}
	}

	// origin: on the wall. Brass scales on a dark shield.
	void DrawScales(const FPen& P)
	{
		const float Z = 166.f;
		P.Box(1.5f, 0.f, Z, 3.f, 46.f, 54.f, "TimberDark");
		P.Box(3.2f, 0.f, Z, 0.6f, 38.f, 46.f, "Velvet", 0.f, 0.f, 0.f, false);
		P.Box(4.2f, 0.f, Z + 1.f, 1.6f, 2.4f, 34.f, "Brass");
		P.Box(4.2f, 0.f, Z + 15.f, 1.6f, 30.f, 2.2f, "Brass");
		P.Box(4.2f, 0.f, Z - 17.f, 1.8f, 14.f, 3.f, "Brass");
		for (float S : { -1.f, 1.f })
		{
			P.Rod(FVector(4.2f, S * 14.f, Z + 15.f), FVector(4.2f, S * 14.f, Z + 2.f), 0.7f, "Brass", false);
			P.Box(4.4f, S * 14.f, Z + 1.f, 2.4f, 10.f, 2.f, "Brass");
		}
	}
}

void ADCHideoutBuilder::FurnishLawyer(DCFurnish::FCtx& Ctx)
{
	using namespace DCFurnishLawyer;
	FPlan& Plan = Ctx.Plan;
	const int32 Seed = Ctx.Seed;
	const bool bHasWall = Ctx.WallF.Num() > 0;
	auto Adjacent = [&](const DCFurnish::FFrame& F) { return (float)(-FMath::Min(Plan.NearestSolid(F.O), 200.f) * 0.15f - Plan.CornerDist(F.O) * 0.04f); };
	auto MidWall = [&](const DCFurnish::FFrame& F) { return F.Edge >= 0 ? (float)FMath::Abs(F.T - Plan.E[F.Edge].L * 0.5f) : 0.f; };

	Ctx.DefaultLamp();

	// ---------------------------------------------------------------- 0. law books
	// Tall bookcases first, in the corners of the standing walls (the desk then sits between them).
	int32 NBooks = 0;
	DCFurnish::FFrame Book0;
	for (int32 i = 0; i < 2 && bHasWall; ++i)
	{
		const TArray<FLocal> Ls = { FLocal::Solid(19.f, 0.f, 17.f, 49.f, 194.f) };
		auto BookScore = [&](const DCFurnish::FFrame& Cand)
		{
			return (float)(-FMath::Min(Plan.CornerDist(Cand.O), 300.f) * 0.1f + (Book0.bValid ? FMath::Min((float)FV2::Distance(Cand.O, Book0.O), 400.f) * 0.06f : 0.f)
				+ (Plan.E[Cand.Edge].bBack ? 6.f : 0.f) + Ctx.Jit(50 + i * 5 + Cand.Edge) * 4.f);
		};
		if (Ctx.bDebug) { Plan.bSoftWindows = true; Plan.Best(Ls, Ctx.WallF, BookScore, 4.f, TEXT("lawyer bookcase")); Plan.bSoftWindows = false; }
		const DCFurnish::FFrame F = Ctx.BestSoft(Ls, Ctx.WallF, BookScore, 4.f, 30.f);
		if (!F.bValid) { continue; }
		Ctx.PlaceSoft(TEXT("bookcase"), Ls, F, [](const FPen& P, int32 S) { DrawBookcase(P, 96.f, S); });
		if (NBooks++ == 0) { Ctx.Spot(F, 74.f, 0.f); Book0 = F; }
	}

	// ---------------------------------------------------------------- 1. the partner's desk and its chairs
	// Facing the room with the high-backed chair to the wall and two client chairs in front; failing that pushed
	// against the wall, or standing free towards the back of the room.
	DCFurnish::FFrame Desk;
	bool bToWall = false;
	float DeskW = 0.f, DeskD = 0.f, DeskX = 0.f; // DeskX: the desk's centre along the frame's +X
	{
		TArray<DCFurnish::FFrame> Free;
		for (const DCFurnish::FFrame& F0 : Ctx.FreeF)
		{
			for (float Y : { YawOf(KToCam), Plan.RoomYaw - 90.f, Plan.RoomYaw + 180.f }) { DCFurnish::FFrame F = F0; F.Yaw = Y; Free.Add(F); }
		}
		auto Score = [&](const DCFurnish::FFrame& F)
		{
			return (float)(Ctx.FacingHub(F) * 12.f + Ctx.FacingCam(F) * 10.f - MidWall(F) * 0.05f
				+ (Plan.E[F.Edge].Kind == EKind::Wall ? 10.f : 0.f) + Ctx.Jit(30 + F.Edge) * 4.f);
		};
		Plan.bHubKeep = false; // the desk group may reach the middle of the room
		for (int32 Pass = 0; Pass < 3 && !Desk.bValid; ++Pass) // pass 0 keeps room for the clients; pass 2 stands free
		{
			for (const FV2& Sz : { FV2(172.f, 84.f), FV2(150.f, 78.f), FV2(124.f, 68.f) })
			{
				const float HW = Sz.X * 0.5f + 2.f, HD = Sz.Y * 0.5f + 2.f, DX = 82.f + HD;
				TArray<FLocal> Ls = { FLocal::Space(50.f, 0.f, 30.f, 36.f, 84.f), FLocal::Solid(DX, 0.f, HD, HW, 84.f) };
				TArray<FLocal> L2 = { FLocal::Solid(HD + 2.f, 0.f, HD, HW, 84.f), FLocal::Space(Sz.Y + 40.f, 0.f, 30.f, 36.f, 84.f) };
				if (Pass == 0)
				{
					FLocal Seat = FLocal::Space(DX + HD + 40.f, 0.f, 30.f, FMath::Min(HW, 66.f), 60.f);
					Seat.Tag = TagSeat;
					Ls.Add(Seat);
				}
				DeskW = Sz.X; DeskD = Sz.Y;
				if (Pass < 2) { Desk = Plan.Best(Ls, Ctx.WallPartF, Score, 6.f, TEXT("lawyer desk")); }
				if (Desk.bValid) { bToWall = false; DeskX = DX; }
				if (!Desk.bValid && Pass == 1)
				{
					Desk = Plan.Best(L2, Ctx.WallPartF, Score, 6.f);
					if (Desk.bValid) { bToWall = true; DeskX = HD + 2.f; Ls = L2; }
				}
				if (!Desk.bValid && Pass == 2)
				{
					Desk = Plan.Best(Ls, Free, [&](const DCFurnish::FFrame& F) { return (float)(Ctx.FacingCam(F) * 10.f - FV2::Distance(F.O + Dir(F.Yaw) * DX, Plan.Hub - KToCam * 80.f) * 0.1f); }, 6.f, TEXT("lawyer desk free"));
					if (Desk.bValid) { bToWall = false; DeskX = DX; Desk.Edge = -1; }
				}
				if (!Desk.bValid) { continue; }
				const float W = DeskW, D = DeskD, DXc = DeskX;
				const bool bWallSide = bToWall;
				Ctx.Place(TEXT("desk"), Ls, Desk, [this, W, D, DXc, bWallSide](const FPen& P, int32 S)
				{
					const float Turn = bWallSide ? 180.f : 0.f;
					const FPen DP = P.At(DXc, 0.f, Turn);
					DrawHighBack(P.At(bWallSide ? D + 40.f : 50.f, J(S, 1) * 5.f, Turn + J(S, 2) * 14.f));
					DrawPartnerDesk(DP, W, D, S);
					Point(DP.P(D * 0.5f * 0.3f - 4.f, W * 0.5f - 24.f, 102.f), FLinearColor(1.f, 0.62f, 0.32f), 8.f, 300.f, false, true, 3.f);
				});
				break;
			}
		}
		Plan.bHubKeep = true;
	}
	if (Desk.bValid)
	{
		// the client chairs: in front of the desk, or beside the pulled-out chair when the desk is at the wall
		const float Front = bToWall ? DeskD + 46.f : DeskX + DeskD * 0.5f + 38.f;
		const float Side = bToWall ? DeskW * 0.5f - 24.f : FMath::Min(DeskW * 0.27f, 40.f);
		int32 NChairs = 0;
		Plan.CapScale = 0.6f; Plan.bHubKeep = false; // a chair may stand in the way a little
		for (int32 i = 0; i < 2 && !bToWall; ++i)
		{
			const float S = i ? 1.f : -1.f;
			DCFurnish::FFrame F = Desk; F.O = Desk.O + Rot(FV2(Front + U(Seed, 600 + i) * 8.f, S * Side), Desk.Yaw); F.Yaw = Desk.Yaw + 180.f - S * 10.f + J(Seed, 610 + i) * 10.f; F.Edge = -1;
			TArray<FLocal> Ls = { FLocal::Solid(0.f, 0.f, 25.f, 25.f, 82.f) };
			Ls[0].Tag = TagSeat;
			if (!Plan.Fits(Ls, F, 3.f)) { continue; }
			Ctx.Place(TEXT("client chair"), Ls, F, [](const FPen& P, int32 Sd) { DrawClientChair(P, Sd); });
			++NChairs;
		}
		Plan.CapScale = 1.f; Plan.bHubKeep = true;
		Plan.Parts.RemoveAll([](const FPart& P) { return P.Tag == TagSeat && !P.bSolid; }); // seats nobody took
		// where a client stands to be advised
		Ctx.Spot(Desk, Front + (NChairs > 0 ? 50.f : 4.f), 0.f);
		// a green rug under the lot
		if (!bToWall) { Ctx.PlaceRug(Desk, DeskX + 14.f, DeskD + 200.f, DeskW + 76.f, "Velvet", { 1.f, 0.86f, 0.74f }); }
		else { Ctx.PlaceRug(Desk, DeskD + 60.f, 150.f, DeskW + 40.f, "Velvet", { 1.f, 0.82f, 0.66f }); }
	}

	// ---------------------------------------------------------------- 2. somewhere for clients to wait
	// A leather sofa with a side table; failing that an armchair or two backed onto whatever edge has room.
	DCFurnish::FFrame Sofa;
	float SofaW = 0.f;
	for (float W : { 180.f, 152.f })
	{
		const TArray<FLocal> Ls = { FLocal::Solid(46.f, 0.f, 44.f, W * 0.5f, 86.f), FLocal::Space(108.f, 0.f, 18.f, W * 0.5f - 24.f, 40.f) };
		Sofa = Plan.Best(Ls, Ctx.AnyWallF, [&](const DCFurnish::FFrame& F)
		{
			return (float)(Ctx.FacingHub(F) * 8.f + Ctx.FacingCam(F) * 14.f + (F.Edge == Desk.Edge ? -25.f : 0.f) + Ctx.Jit(40 + F.Edge) * 5.f - MidWall(F) * 0.03f
				- FMath::Min(Plan.CornerDist(F.O), 200.f) * 0.02f);
		}, 6.f, TEXT("lawyer sofa"));
		if (!Sofa.bValid) { continue; }
		SofaW = W;
		Ctx.Place(TEXT("sofa"), Ls, Sofa, [W](const FPen& P, int32 S) { DrawSofa(P, W, "Leather", S); });
		Ctx.Spot(Sofa, 122.f, 0.f);
		break;
	}
	// no edge to back onto: a sofa standing free across the room from the desk, facing it
	if (!Sofa.bValid)
	{
		TArray<DCFurnish::FFrame> Fr;
		const FV2 Look = Desk.bValid ? Desk.O + Dir(Desk.Yaw) * DeskX : Plan.Hub;
		for (const DCFurnish::FFrame& F0 : Ctx.FreeF)
		{
			DCFurnish::FFrame F = F0;
			F.Yaw = FMath::RoundToFloat(YawOf(Look - F.O) / 90.f - Plan.RoomYaw / 90.f) * 90.f + Plan.RoomYaw; // square to the room
			F.O = F0.O - Dir(F.Yaw) * 46.f; // frame at the sofa's back
			Fr.Add(F);
		}
		for (float W : { 152.f, 124.f })
		{
			const TArray<FLocal> Ls = { FLocal::Solid(46.f, 0.f, 44.f, W * 0.5f, 86.f), FLocal::Space(108.f, 0.f, 18.f, W * 0.5f - 24.f, 40.f) };
			Sofa = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& F)
			{
				return (float)(-FMath::Abs(FV2::Distance(F.O, Look) - 260.f) * 0.1f + Ctx.FacingHub(F) * 6.f - Plan.NearestSolid(F.O) * 0.02f);
			}, 6.f, TEXT("lawyer sofa free"));
			if (!Sofa.bValid) { continue; }
			SofaW = W;
			Ctx.Place(TEXT("sofa"), Ls, Sofa, [W](const FPen& P, int32 S) { DrawSofa(P, W, "Leather", S); });
			Ctx.Spot(Sofa, 122.f, 0.f);
			break;
		}
	}
	TArray<DCFurnish::FFrame> Seats;
	for (int32 n = 0; n < 2 && !Sofa.bValid; ++n)
	{
		TArray<DCFurnish::FFrame> Fr;
		for (const DCFurnish::FFrame& W0 : Ctx.AnyWallF)
		{
			DCFurnish::FFrame F = W0; F.O = W0.O + Dir(W0.Yaw) * 58.f;
			F.Yaw = W0.Yaw + FMath::Clamp(FMath::FindDeltaAngleDegrees(W0.Yaw, YawOf(Plan.Hub - F.O)), -15.f, 15.f);
			Fr.Add(F);
		}
		const TArray<FLocal> Ls = { FLocal::Solid(0.f, 0.f, 42.f, 42.f, 86.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& Cand)
		{
			return (float)(Ctx.FacingCam(Cand) * 8.f + Ctx.FacingHub(Cand) * 6.f + (Cand.Edge == Desk.Edge ? -15.f : 0.f)
				- (Seats.Num() ? FMath::Abs(FV2::Distance(Cand.O, Seats[0].O) - 120.f) * 0.2f : 0.f) + Ctx.Jit(44 + Cand.Edge) * 4.f);
		}, 10.f, TEXT("lawyer armchair"));
		if (!F.bValid) { break; }
		Ctx.Place(TEXT("armchair"), Ls, F, [](const FPen& P, int32 S) { DrawArmchair(P, "Leather", S); });
		if (n == 0) { Ctx.Spot(F, 78.f, 0.f); }
		Seats.Add(F);
	}
	// the side table beside the seating
	{
		TArray<DCFurnish::FFrame> Fr;
		if (Sofa.bValid) { for (float S : { 1.f, -1.f }) { DCFurnish::FFrame F = Sofa; F.O = Sofa.O + Rot(FV2(34.f, S * (SofaW * 0.5f + 30.f)), Sofa.Yaw); F.Edge = -1; Fr.Add(F); } }
		for (const DCFurnish::FFrame& A : Seats) { for (float S : { 1.f, -1.f }) { DCFurnish::FFrame F = A; F.O = A.O + Rot(FV2(-8.f, S * 72.f), A.Yaw); F.Edge = -1; Fr.Add(F); } }
		const TArray<FLocal> Ls = { FLocal::Solid(0.f, 0.f, 23.f, 23.f, 54.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& Cand) { return (float)(-FV2::DotProduct(Cand.O - Plan.Hub, KToCam) * 0.05f); }, 3.f);
		if (F.bValid) { Ctx.Place(TEXT("side table"), Ls, F, [](const FPen& P, int32 S) { DrawSideTable(P, S); }); }
	}

	Ctx.Runner();

	// ---------------------------------------------------------------- 3. low law books
	// A third bookcase with the first upgrade; low cabinets with the books on top otherwise.
	if (bHasWall)
	{
		const TArray<FLocal> Ls = { FLocal::Solid(19.f, 0.f, 17.f, 49.f, 194.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, Ctx.WallF, [&](const DCFurnish::FFrame& Cand) { return (float)(Adjacent(Cand) + Ctx.Jit(60 + Cand.Edge) * 4.f); }, 4.f);
		if (F.bValid) { Ctx.Place(TEXT("bookcase"), Ls, F, [](const FPen& P, int32 S) { DrawBookcase(P, 96.f, S); }, DCGroups::Level2); ++NBooks; }
	}
	for (int32 i = 0; i < 2; ++i)
	{
		// one credenza always (two when there are no bookcases); the upgrade adds one when the walls took the books
		const bool bUpgrade = i == 1 && NBooks > 0;
		for (float W : { 132.f, 96.f })
		{
			const TArray<FLocal> Ls = { FLocal::Solid(26.f, 0.f, 24.f, W * 0.5f + 2.f, 84.f) };
			const DCFurnish::FFrame F = Plan.Best(Ls, Ctx.AnyWallF, [&](const DCFurnish::FFrame& Cand)
			{
				return (float)((Plan.InWindow(Cand.Edge, Cand.T, W * 0.5f) ? 12.f : 0.f) + (Plan.E[Cand.Edge].bBack ? 14.f : 0.f) + Adjacent(Cand) * 0.5f
					- MidWall(Cand) * 0.02f + Ctx.Jit(70 + i * 5 + Cand.Edge) * 4.f);
			}, 5.f, TEXT("lawyer credenza"));
			if (!F.bValid) { continue; }
			Ctx.Place(TEXT("credenza"), Ls, F, [W](const FPen& P, int32 S) { DrawCredenza(P, W, false, S); }, bUpgrade ? DCGroups::Level2 : FName(NAME_None));
			if (NBooks == 0 && i == 0) { Ctx.Spot(F, 82.f, 0.f); }
			break;
		}
	}

	// ---------------------------------------------------------------- 4. the files
	// A row of cabinets where something head-high may stand, and boxes of case files gathered beside them.
	DCFurnish::FFrame Files;
	for (int32 N : { 3, 2 })
	{
		const TArray<FLocal> Ls = { FLocal::Solid(34.f, 0.f, 32.f, N * 23.5f + 1.f, 136.f) };
		Files = Plan.Best(Ls, Ctx.WallPartF, [&](const DCFurnish::FFrame& Cand)
		{
			return (float)((Desk.bValid ? -FMath::Abs(FV2::Distance(Cand.O, Desk.O) - 220.f) * 0.03f : 0.f) + Adjacent(Cand) + Ctx.Jit(80 + Cand.Edge) * 4.f);
		}, 5.f, TEXT("lawyer files"));
		if (!Files.bValid) { continue; }
		Ctx.Place(TEXT("filing"), Ls, Files, [N](const FPen& P, int32 S) { DrawFilingCabinets(P, N, S); });
		Ctx.Spot(Files, 96.f, 0.f);
		break;
	}
	for (int32 i = 0; i < 3; ++i)
	{
		const int32 N = i == 0 ? 5 : (i == 1 ? 3 : 4);
		const TArray<FLocal> Ls = { FLocal::Solid(24.f, 0.f, 23.f, 42.f, N > 2 ? 62.f : 30.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, Ctx.AnyWallF, [&](const DCFurnish::FFrame& Cand)
		{
			const float Near = Plan.NearestSolid(Cand.O), Corner = Plan.CornerDist(Cand.O);
			if (Near > 80.f && Corner > 100.f) { return -1e9f; } // stacked beside something, not adrift mid-wall
			return (float)(-Near * 0.2f - Corner * 0.08f + (Files.bValid ? -FV2::Distance(Cand.O, Files.O) * 0.04f : 0.f) + J(Seed, 820 + i * 11 + Cand.Edge) * 8.f
				- FV2::DotProduct(Cand.O - Plan.Hub, KToCam) * 0.05f);
		}, 5.f);
		if (!F.bValid) { continue; }
		DCFurnish::FFrame G = F; G.Yaw += J(Seed, 830 + i) * 6.f;
		Ctx.Place(TEXT("case files"), Ls, G, [N](const FPen& P, int32 S) { DrawArchiveBoxes(P, N, S); }, i == 2 ? DCGroups::Level2 : FName(NAME_None));
	}

	// ---------------------------------------------------------------- 5. the rest: a globe, a coat stand, the upgrades
	{
		const TArray<FLocal> Ls = { FLocal::Solid(22.f, 0.f, 20.f, 20.f, 84.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, Ctx.AnyWallF, [&](const DCFurnish::FFrame& Cand)
		{
			return (float)((Desk.bValid ? -FMath::Abs(FV2::Distance(Cand.O, Desk.O + Dir(Desk.Yaw) * DeskX) - 150.f) * 0.1f : 0.f) + Adjacent(Cand) * 0.4f
				+ (Plan.E[Cand.Edge].bBack ? 8.f : 0.f));
		}, 6.f);
		if (F.bValid) { Ctx.Place(TEXT("globe"), Ls, F, [](const FPen& P, int32) { DrawGlobe(P.At(22.f, 0.f)); }); }
	}
	{
		// by the way in, where a wall is high enough to stand it against
		const TArray<FLocal> Ls = { FLocal::Solid(20.f, 0.f, 18.f, 18.f, 196.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, Ctx.WallF, [&](const DCFurnish::FFrame& Cand) { return (float)(-Plan.CornerDist(Cand.O) * 0.1f + FV2::DotProduct(Cand.O - Plan.Hub, KToCam) * 0.05f + Ctx.Jit(90 + Cand.Edge) * 4.f); }, 5.f, TEXT("lawyer coat"));
		if (F.bValid) { Ctx.Place(TEXT("coat stand"), Ls, F, [](const FPen& P, int32 S) { DrawCoatStand(P.At(20.f, 0.f), S); }); }
	}
	{
		// the top upgrade: a safe for retainers by the desk and a drinks cabinet
		const TArray<FLocal> Ls = { FLocal::Solid(33.f, 0.f, 31.f, 31.f, 94.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, Ctx.WallPartF, [&](const DCFurnish::FFrame& Cand) { return (float)((Desk.bValid ? -FV2::Distance(Cand.O, Desk.O) * 0.12f : 0.f) + Adjacent(Cand) * 0.5f); }, 5.f);
		if (F.bValid) { Ctx.Place(TEXT("safe"), Ls, F, [](const FPen& P, int32) { DrawSafe(P); }, DCGroups::Level3); }
		const TArray<FLocal> Lc = { FLocal::Solid(26.f, 0.f, 24.f, 50.f, 84.f) };
		const DCFurnish::FFrame Fc = Plan.Best(Lc, Ctx.AnyWallF, [&](const DCFurnish::FFrame& Cand)
		{
			return (float)((Sofa.bValid ? -FV2::Distance(Cand.O, Sofa.O) * 0.05f : 0.f) + (Plan.E[Cand.Edge].bBack ? 10.f : 0.f) + Adjacent(Cand) * 0.5f);
		}, 5.f);
		if (Fc.bValid) { Ctx.Place(TEXT("drinks"), Lc, Fc, [](const FPen& P, int32 S) { DrawCredenza(P, 96.f, true, S); }, DCGroups::Level3); }
	}

	// ---------------------------------------------------------------- 6. on the walls
	int32 NWall = 0;
	{
		struct FDecor { float W; float Z0, Z1, D; };
		const FDecor Decor[] = { { 116.f, 140.f, 190.f, 6.f }, { 50.f, 136.f, 196.f, 8.f } }; // certificates, the scales
		for (int32 i = 0; i < UE_ARRAY_COUNT(Decor); ++i)
		{
			const FDecor& Dc = Decor[i];
			TArray<FLocal> Ls = { FLocal::Solid(Dc.D * 0.5f + 2.f, 0.f, Dc.D * 0.5f, Dc.W * 0.5f, Dc.Z1, Dc.Z0) };
			Ls[0].bSolid = false;
			const DCFurnish::FFrame F = Plan.Best(Ls, Ctx.WallF, [&](const DCFurnish::FFrame& Cand)
			{
				// the certificates hang behind the desk if they can; both read best above low furniture
				float Above = 0.f;
				FObb Probe; Probe.C = Cand.O + Dir(Cand.Yaw) * 45.f; Probe.H = FV2(40.f, Dc.W * 0.4f); Probe.Yaw = Cand.Yaw;
				for (const FPart& Pt : Plan.Parts)
				{
					if (Pt.bSolid && !Pt.bRug && Pt.Z1 < 110.f && Overlap(Probe, Pt.Box, 0.f)) { Above = FMath::Max(Above, 16.f - (float)FV2::Distance(Pt.Box.C, Probe.C) * 0.05f); }
				}
				const float Behind = (i == 0 && Desk.bValid && Cand.Edge == Desk.Edge) ? 30.f - FMath::Abs(Cand.T - Desk.T) * 0.3f : 0.f;
				return (float)(Above + Behind + J(Seed, 900 + i * 7 + Cand.Edge) * 5.f - MidWall(Cand) * 0.01f);
			}, 20.f, TEXT("lawyer decor"));
			if (!F.bValid) { continue; }
			const float W = Dc.W;
			if (i == 0) { Ctx.Place(TEXT("certificates"), Ls, F, [W](const FPen& P, int32 S) { DrawCertificates(P, W, S); }); }
			else { Ctx.Place(TEXT("scales"), Ls, F, [](const FPen& P, int32) { DrawScales(P); }); }
			++NWall;
		}
	}
	if (bHasWall && NWall == 0) { Ctx.WallDecor(); }

	// a tidy room: only a little clutter
	Ctx.Clutter(0.34f);
}
