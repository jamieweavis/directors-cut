// Director's Cut — room furnishing on the build grid. A small layout solver fits furniture to the room's real
// (jittered) outline and to its connections, then draws it from primitives. This file holds the shared prop kit, the
// steps any room can use and the driver; each room's recipe is in its own file (DCHideoutFurnishOffice.cpp, ...).
// The Garage is not furnished yet.
//
// How a layout is found (all room-local, cm):
//  1. Every edge is classified: tall wall (its windows and broken-down ends come from TallEdgePlan), low partition
//     with a centred doorway, or collapsed open front. Edges add height BANDS: nothing tall near an open front (the
//     camera looks in over it), in front of a window or a broken-down stretch of wall, or under the wall lamp.
//  2. Circulation is reserved: a clear box at each doorway and way out, clear lanes from each of them to the room's
//     hub, and the hub itself, so crew still walk where they always did.
//  3. Furniture groups go in by priority (mission board first, small fillers last). A group is a few oriented boxes,
//     each with a Z range (a rug, a wall-hung picture and a sofa can share floor), in a local frame whose +X faces
//     into the room. Candidate frames slide along every eligible wall, sweep a free grid, or hang off something
//     already placed (the table in front of the board, the lamp beside the sofa). Every candidate that fits is scored
//     (centred on its wall, tucked into a corner, facing the room, beside related furniture, ...) and the best wins.
//     A group that can't fit falls back to a smaller or freestanding variant, or is left out.
//  4. The winners are drawn, their solid footprints go to the room (pawns avoid them, and walks that would cut
//     through furniture detour via the room's middle) and their use spots become places where crew look busy.
//
// Tall wall furniture may stand against a window; the wall is then built without it (so the furnisher runs before
// the walls). -DCFurnishDebug logs why each key piece was rejected where it was.
#include "World/Scene/DCHideoutFurnish.h"
#include "Components/PointLightComponent.h"

namespace DCFurnish
{
	// ------------------------------------------------------------------------------------------------- props
	// Each prop draws around its frame origin (on the floor). Wall props: origin on the wall's inner face.

	void DrawWoodChair(const FPen& P, int32 Seed)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			const float X = (i % 2) ? 18.f : -18.f, Y = (i / 2) ? 18.f : -18.f;
			P.Box(X, Y, 23.f, 4.f, 4.f, 46.f, "TimberDark");
		}
		P.Box(0.f, -18.f, 13.f, 36.f, 2.5f, 2.5f, "TimberDark");
		P.Box(0.f, 18.f, 13.f, 36.f, 2.5f, 2.5f, "TimberDark");
		P.Box(1.f, 0.f, 47.5f, 46.f, 45.f, 4.f, "Timber");
		for (float Y : { -18.f, 18.f }) { P.Box(-20.f, Y, 72.f, 4.5f, 4.5f, 48.f, "TimberDark", 0.f, -5.f); }
		P.Box(-21.5f, 0.f, 90.f, 3.5f, 40.f, 9.f, "Timber", 0.f, -5.f);
		P.Box(-20.5f, 0.f, 74.f, 2.5f, 36.f, 5.f, "Timber", 0.f, -5.f);
		if (U(Seed, 1) > 0.6f) { P.Box(2.f, 0.f, 50.5f, 38.f, 38.f, 2.f, "Blanket", J(Seed, 2) * 10.f); } // a folded cushion
	}

	void DrawCaptainChair(const FPen& P)
	{
		for (int32 i = 0; i < 4; ++i) { P.Box(0.f, 0.f, 3.f, 52.f, 5.f, 3.f, "Black", 45.f + i * 90.f); }
		P.Cyl(0.f, 0.f, 22.f, 7.f, 36.f, "Black");
		P.Box(0.f, 0.f, 45.f, 50.f, 52.f, 10.f, "LeatherBrn");
		P.Box(2.f, 0.f, 51.f, 42.f, 44.f, 4.f, "LeatherBrn");
		P.Box(-25.f, 0.f, 76.f, 10.f, 52.f, 54.f, "LeatherBrn", 0.f, -9.f);
		for (float Y : { -27.f, 27.f })
		{
			P.Box(-2.f, Y, 63.f, 44.f, 6.f, 5.f, "Walnut");
			P.Box(12.f, Y, 56.f, 4.f, 4.f, 14.f, "Walnut");
		}
	}

	void DrawArmchair(const FPen& P, FName Mat, int32 Seed)
	{
		for (int32 i = 0; i < 4; ++i) { P.Box((i % 2) ? 32.f : -32.f, (i / 2) ? 34.f : -34.f, 3.f, 6.f, 6.f, 6.f, "Walnut"); }
		P.Box(2.f, 0.f, 22.f, 76.f, 80.f, 26.f, Mat);
		P.Box(10.f, 0.f, 41.f, 60.f, 54.f, 12.f, Mat, J(Seed, 1) * 2.f);
		P.Box(-30.f, 0.f, 57.f, 20.f, 80.f, 48.f, Mat, 0.f, -8.f);
		P.Box(-29.f, 0.f, 81.f, 17.f, 78.f, 8.f, Mat, 0.f, -8.f);
		for (float Y : { -34.f, 34.f }) { P.Box(2.f, Y, 47.f, 76.f, 13.f, 26.f, Mat); P.Box(4.f, Y, 61.f, 72.f, 15.f, 4.f, Mat); }
		if (U(Seed, 2) > 0.4f) { P.Box(-12.f, J(Seed, 3) * 12.f, 60.f, 12.f, 34.f, 30.f, "Linen", J(Seed, 4) * 25.f, -22.f); }
	}

	void DrawSofa(const FPen& P, float W, FName Mat, int32 Seed)
	{
		const float HW = W * 0.5f;
		for (float X : { 10.f, 80.f }) { for (float Y : { -HW + 8.f, HW - 8.f }) { P.Box(X, Y, 3.f, 6.f, 6.f, 6.f, "Walnut"); } }
		P.Box(46.f, 0.f, 22.f, 80.f, W, 26.f, Mat);
		P.Box(14.f, 0.f, 57.f, 22.f, W - 6.f, 52.f, Mat, 0.f, -6.f);
		for (float S : { -1.f, 1.f })
		{
			P.Box(46.f, S * (HW - 11.f), 47.f, 82.f, 22.f, 30.f, Mat);
			P.Box(48.f, S * (HW - 11.f), 63.f, 78.f, 24.f, 5.f, Mat);
		}
		const int32 N = W > 180.f ? 3 : 2;
		const float CW = (W - 44.f) / N;
		for (int32 i = 0; i < N; ++i)
		{
			const float Y = -HW + 22.f + CW * (i + 0.5f);
			P.Box(52.f + J(Seed, 10 + i) * 2.f, Y, 41.5f + U(Seed, 20 + i) * 1.5f, 62.f, CW - 2.f, 13.f, Mat, J(Seed, 30 + i) * 2.f);
			P.Box(30.f, Y, 66.f, 15.f, CW - 4.f, 34.f, Mat, J(Seed, 40 + i) * 3.f, -14.f);
		}
		const float Side = U(Seed, 1) > 0.5f ? 1.f : -1.f;
		// a throw blanket over one arm, and a scatter cushion in the other corner
		P.Box(48.f, Side * (HW - 11.f), 66.5f, 56.f, 28.f, 2.f, "Blanket", J(Seed, 2) * 6.f);
		P.Box(48.f, Side * (HW + 0.8f), 54.f, 54.f, 2.f, 26.f, "Blanket", J(Seed, 2) * 6.f);
		P.Box(40.f, -Side * (HW - 34.f), 62.f, 12.f, 38.f, 34.f, "Linen", -Side * 18.f, -18.f);
	}

	void DrawCoffeeTable(const FPen& P, float W, float D, int32 Seed)
	{
		P.Box(0.f, 0.f, 40.f, D, W, 4.f, "Walnut");
		P.Box(0.f, 0.f, 12.f, D - 10.f, W - 10.f, 2.f, "Walnut");
		for (int32 i = 0; i < 4; ++i) { P.Box((i % 2) ? D * 0.5f - 4.f : -D * 0.5f + 4.f, (i / 2) ? W * 0.5f - 4.f : -W * 0.5f + 4.f, 20.f, 5.f, 5.f, 38.f, "Walnut"); }
		// magazines, mugs, an ashtray, a candle in a bottle, cards
		P.Box(-6.f, -W * 0.22f, 42.6f, 26.f, 20.f, 1.f, "Paper", 12.f + J(Seed, 1) * 8.f);
		P.Box(-4.f, -W * 0.24f, 43.4f, 26.f, 20.f, 0.6f, "Photo", -6.f + J(Seed, 2) * 8.f);
		P.Cyl(10.f, W * 0.05f, 46.5f, 8.f, 9.f, "Enamel");
		P.Cyl(-12.f, W * 0.12f, 46.5f, 8.f, 9.f, "Enamel");
		P.Cyl(6.f, W * 0.3f, 43.f, 13.f, 2.f, "Glass");
		P.Cyl(-8.f, W * 0.34f, 53.f, 7.f, 22.f, "Bottle");
		P.Cyl(-8.f, W * 0.34f, 66.f, 2.5f, 5.f, "Paper");
		P.Ball(-8.f, W * 0.34f, 70.f, 1.6f, 1.6f, 3.f, "Candle", false);
		for (int32 i = 0; i < 4; ++i) { P.Box(14.f + J(Seed, 5 + i) * 6.f, -W * 0.02f + J(Seed, 9 + i) * 10.f, 42.3f, 9.f, 6.f, 0.4f, "Paper", J(Seed, 13 + i) * 60.f); }
		P.Box(-12.f, -W * 0.05f, 12.f + 3.5f, 24.f, 30.f, 5.f, "Paper", J(Seed, 20) * 10.f); // newspapers on the shelf
	}

	void DrawRug(const FPen& P, float LX, float LY, FName Field, FName Border)
	{
		P.Box(0.f, 0.f, 0.6f, LX, LY, 1.2f, Border, 0.f, 0.f, 0.f, false);
		P.Box(0.f, 0.f, 1.3f, LX - 16.f, LY - 16.f, 0.4f, Field, 0.f, 0.f, 0.f, false);
		if (LX < LY * 2.2f) { P.Box(0.f, 0.f, 1.6f, LX * 0.38f, LY * 0.3f, 0.3f, Border, 45.f, 0.f, 0.f, false); } // centre medallion (not on runners)
		else { for (float S : { -1.f, 1.f }) { P.Box(0.f, S * (LY * 0.5f - 14.f), 1.6f, LX - 30.f, 3.f, 0.3f, Border, 0.f, 0.f, 0.f, false); } } // runner stripes
		for (float S : { -1.f, 1.f }) { P.Box(S * (LX * 0.5f + 2.5f), 0.f, 0.4f, 5.f, LY - 8.f, 0.6f, "Linen", 0.f, 0.f, 0.f, false); } // fringes
	}

	void DrawLaptop(const FPen& P, float Zt, FName Screen)
	{
		P.Box(0.f, 0.f, Zt + 0.8f, 24.f, 34.f, 1.6f, "Alu");
		P.Box(0.6f, 0.f, Zt + 1.7f, 12.f, 30.f, 0.2f, "PlasticBlack", 0.f, 0.f, 0.f, false);
		P.Box(-12.5f + 2.6f, 0.f, Zt + 11.2f, 1.4f, 34.f, 22.f, "Alu", 0.f, 14.f);
		P.Box(-11.4f + 2.6f, 0.f, Zt + 11.4f, 0.3f, 31.f, 19.f, Screen, 0.f, 14.f, 0.f, false);
	}

	void DrawPlanningTable(const FPen& P, float W, float D, int32 Seed)
	{
		P.Box(0.f, 0.f, 78.f, D, W, 5.f, "Timber");
		P.Box(0.f, 0.f, 71.f, D - 12.f, W - 16.f, 9.f, "TimberDark");
		for (int32 i = 0; i < 4; ++i) { P.Box((i % 2) ? D * 0.5f - 7.f : -D * 0.5f + 7.f, (i / 2) ? W * 0.5f - 8.f : -W * 0.5f + 8.f, 35.f, 7.5f, 7.5f, 70.f, "TimberDark"); }
		P.Box(0.f, 0.f, 13.f, 5.f, W - 22.f, 5.f, "TimberDark");
		// the job: a map spread out with a route drawn on, plans, photos of the target
		P.Box(2.f, -W * 0.1f, 80.8f, D * 0.66f, W * 0.44f, 0.5f, "Paper", 3.f + J(Seed, 1) * 3.f);
		for (int32 i = 0; i < 4; ++i)
		{
			const FV2 A(J(Seed, 10 + i) * D * 0.25f, -W * 0.1f + J(Seed, 20 + i) * W * 0.18f);
			const FV2 B(J(Seed, 11 + i) * D * 0.25f, -W * 0.1f + J(Seed, 21 + i) * W * 0.18f);
			P.Rod(FVector(A.X, A.Y, 81.2f), FVector(B.X, B.Y, 81.2f), 0.7f, "Pin", false);
		}
		P.Box(-D * 0.12f, W * 0.28f, 80.9f, D * 0.5f, W * 0.24f, 0.4f, "Blueprint", -7.f + J(Seed, 2) * 4.f);
		P.S(EDCShape::Cylinder, -D * 0.33f, W * 0.3f, 84.f, 7.f, 7.f, W * 0.3f, "Blueprint", 90.f, 90.f, 0.f);
		P.S(EDCShape::Cylinder, -D * 0.3f, W * 0.18f, 84.f, 6.f, 6.f, W * 0.24f, "Paper", 98.f, 90.f, 0.f);
		for (int32 i = 0; i < 5; ++i) { P.Box(D * 0.28f + J(Seed, 30 + i) * 6.f, -W * 0.36f + i * 11.f, 80.9f + i * 0.15f, 11.f, 14.f, 0.3f, "Photo", J(Seed, 40 + i) * 18.f); }
		P.Cyl(D * 0.3f, W * 0.18f, 85.f, 8.f, 9.f, "Enamel");
		P.Cyl(-D * 0.28f, -W * 0.38f, 85.f, 8.f, 9.f, "Enamel");
		DrawLaptop(P.At(D * 0.18f, W * 0.36f, 180.f + J(Seed, 3) * 15.f), 80.5f, "Screen1");
		// walkie-talkies and a box of shells
		P.Box(-D * 0.3f, -W * 0.02f, 85.f, 6.f, 4.f, 14.f, "PlasticBlack", 20.f);
		P.Rod(FVector(-D * 0.3f, -W * 0.02f, 92.f), FVector(-D * 0.3f, -W * 0.02f, 104.f), 0.8f, "PlasticBlack");
		P.Box(-D * 0.22f, -W * 0.06f, 81.5f, 13.f, 6.f, 3.f, "PlasticBlack", -35.f);
		P.Box(D * 0.05f, W * 0.08f, 83.f, 10.f, 14.f, 5.f, "PaintRed", 12.f);
	}

	/** Cork board content: map, photos, cards, pins and string. Origin on the board's face plane, CentreZ = middle. */
	void DrawBoardFace(const FPen& P, float W, float CentreZ, float H, int32 Seed)
	{
		P.Box(1.5f, 0.f, CentreZ, 3.f, W, H, "Cork");
		P.Box(2.5f, 0.f, CentreZ + H * 0.5f + 2.5f, 5.f, W + 10.f, 5.f, "TimberDark");
		P.Box(2.5f, 0.f, CentreZ - H * 0.5f - 2.5f, 5.f, W + 10.f, 5.f, "TimberDark");
		for (float S : { -1.f, 1.f }) { P.Box(2.5f, S * (W * 0.5f + 2.5f), CentreZ, 5.f, 5.f, H + 10.f, "TimberDark"); }
		P.Box(3.3f, -W * 0.13f, CentreZ + 4.f, 0.5f, W * 0.44f, H * 0.62f, "Paper");
		P.Box(3.4f, W * 0.27f, CentreZ + H * 0.2f, 0.5f, W * 0.26f, H * 0.34f, "Blueprint");
		TArray<FV2> Pins;
		const int32 NPhotos = FMath::Clamp(FMath::RoundToInt(W / 28.f), 4, 9);
		for (int32 i = 0; i < NPhotos; ++i)
		{
			const float Y = (-0.42f + 0.84f * (i + 0.5f) / NPhotos) * W + J(Seed, 50 + i) * 8.f;
			const float Zp = CentreZ + ((i % 2) ? -1.f : 1.f) * H * (0.2f + 0.12f * U(Seed, 60 + i));
			const bool bCard = U(Seed, 70 + i) > 0.6f;
			P.Box(3.6f + i * 0.02f, Y, Zp, 0.4f, bCard ? 12.f : 14.f, bCard ? 8.f : 11.f, bCard ? FName("Paper") : FName("Photo"), 0.f, 0.f, J(Seed, 80 + i) * 9.f);
			Pins.Add(FV2(Y, Zp + 4.f));
			P.Ball(4.4f, Y, Zp + 4.f, 1.8f, 1.8f, 1.8f, "Pin", false);
		}
		for (int32 i = 0; i + 1 < Pins.Num(); i += (U(Seed, 90 + i) > 0.7f ? 2 : 1))
		{
			const FV2 A = Pins[i], B = Pins[(i + 1 + (i % 3 == 0 ? 1 : 0)) % Pins.Num()];
			P.Rod(FVector(4.2f, A.X, A.Y), FVector(4.2f, B.X, B.Y), 0.45f, "Pin", false);
		}
		// one photo circled: the target
		const int32 T = (int32)(U(Seed, 99) * NPhotos) % NPhotos;
		P.Box(3.5f, Pins[T].X, Pins[T].Y - 4.f, 0.3f, 19.f, 15.f, "PaintRed");
	}

	void DrawBoard(const FPen& P, float W, float Top, int32 Seed)
	{
		const float H = 112.f;
		DrawBoardFace(P, W, Top - H * 0.5f - 5.f, H, Seed);
		// a ledge with marker pens and a mug under it
		P.Box(5.f, 0.f, Top - H - 12.f, 9.f, W * 0.7f, 2.5f, "TimberDark");
		for (int32 i = 0; i < 3; ++i) { P.Box(5.f, -W * 0.2f + i * 6.f, Top - H - 10.f, 1.6f, 13.f, 1.6f, i == 1 ? FName("PaintRed") : FName("PlasticBlack")); }
	}

	void DrawEaselBoard(const FPen& P, float W, int32 Seed)
	{
		// a board on timber trestle legs, kept low (rooms with no tall wall: it mustn't hide the room behind).
		// Origin: the back of its footprint; the face looks along +X.
		for (float S : { -1.f, 1.f })
		{
			const float Y = S * (W * 0.5f - 6.f);
			P.Rod(FVector(50.f, Y, 0.f), FVector(36.f, Y, 146.f), 5.f, "Timber");
			P.Rod(FVector(3.f, Y, 0.f), FVector(33.f, Y, 143.f), 4.f, "TimberDark");
			P.Rod(FVector(46.f, Y, 34.f), FVector(10.f, Y, 34.f), 3.f, "TimberDark");
		}
		DrawBoardFace(P.At(37.f, 0.f), W, 98.f, 80.f, Seed);
		P.Box(44.f, 0.f, 55.f, 12.f, W * 0.8f, 2.5f, "Timber");
	}

	void DrawDesk(const FPen& P, float W, float D, int32 Seed)
	{
		// origin: desk centre. Sitter on -X, the room (and the camera) on +X. W 152 = partners' desk with two
		// pedestals; narrower = a writing desk with one pedestal and legs.
		const float HW = W * 0.5f, HD = D * 0.5f, Ky = W / 152.f;
		P.Box(0.f, 0.f, 76.f, D, W, 4.f, "Walnut");
		P.Box(-1.f, 0.f, 78.2f, D * 0.63f, W * 0.5f, 0.4f, "LeatherBrn", 0.f, 0.f, 0.f, false);
		const bool bTwo = W > 130.f;
		for (float S : { -1.f, 1.f })
		{
			if (!bTwo && S < 0.f)
			{
				for (float X : { -HD + 4.f, HD - 4.f }) { P.Box(X, -HW + 5.f, 37.f, 5.f, 5.f, 74.f, "Walnut"); }
				P.Box(-HD + 6.f, -HW + 22.f, 70.f, 2.f, 30.f, 8.f, "Wood");
				continue;
			}
			const float Y = S * (HW - 24.f);
			P.Box(0.f, Y, 37.f, D - 6.f, 44.f, 74.f, "Walnut");
			for (int32 k = 0; k < 3; ++k)
			{
				const float Z = 15.f + k * 23.f;
				P.Box(-HD + 2.5f, Y, Z, 1.2f, 38.f, 19.f, "Wood");
				P.Box(-HD + 1.4f, Y, Z + 3.f, 1.2f, 8.f, 2.f, "Brass");
			}
		}
		P.Box(HD - 4.f, bTwo ? 0.f : -12.f, 46.f, 3.f, bTwo ? W - 92.f : W - 60.f, 56.f, "Walnut");
		// banker's lamp
		const FPen L = P.At(HD * 0.37f, HW - 22.f, 0.f);
		L.Cyl(0.f, 0.f, 79.f, 14.f, 2.f, "Brass");
		L.Cyl(0.f, 0.f, 93.f, 2.f, 28.f, "Brass");
		L.Box(-4.f, 0.f, 108.f, 13.f, 30.f, 7.f, "PaintGreen", 0.f, -8.f);
		L.Box(-4.f, 0.f, 104.4f, 10.f, 26.f, 0.4f, "Tungsten", 0.f, -8.f, 0.f, false);
		DrawLaptop(P.At(-HD * 0.2f, -8.f * Ky, 180.f + J(Seed, 1) * 12.f), 78.f, "Screen0");
		// paperwork, a telephone, whisky, cash
		P.Box(HD * 0.1f, -HW * 0.58f, 79.5f, 22.f, 30.f, 3.f, "Paper", J(Seed, 2) * 10.f);
		if (bTwo) { P.Box(-14.f, 44.f, 79.f, 20.f, 26.f, 1.f, "Paper", J(Seed, 3) * 10.f); }
		P.Box(HD * 0.47f, -8.f * Ky, 81.f, 16.f, 20.f, 6.f, "PlasticBlack", 25.f);
		P.Box(HD * 0.47f, -8.f * Ky, 85.f, 6.f, 20.f, 3.f, "PlasticBlack", 25.f);
		P.Cyl(HD * 0.58f, -HW + 12.f, 90.f, 8.f, 24.f, "Bottle");
		P.Cyl(HD * 0.58f, -HW + 12.f, 104.f, 3.f, 5.f, "Bottle");
		P.Cyl(HD * 0.26f, -HW + 16.f, 82.f, 6.f, 8.f, "Glass");
		if (bTwo) { for (int32 i = 0; i < 4; ++i) { P.Box(-20.f + (i % 2) * 9.f, -60.f + (i / 2) * 17.f, 79.5f + (i % 2) * 2.5f, 7.5f, 16.f, 3.f + (i % 2) * 2.5f, "Cash", J(Seed, 10 + i) * 6.f); } }
	}

	void DrawStove(const FPen& P, float FlueTop, int32 Seed)
	{
		P.Box(44.f, 0.f, 2.f, 88.f, 100.f, 4.f, "StoneDark");
		P.Box(44.f, 0.f, 4.3f, 84.f, 96.f, 0.6f, "Stone", 0.f, 0.f, 0.f, false);
		for (int32 i = 0; i < 4; ++i) { P.Box((i % 2) ? 56.f : 16.f, (i / 2) ? 24.f : -24.f, 11.f, 6.f, 6.f, 14.f, "CastIron"); }
		P.Box(36.f, 0.f, 46.f, 46.f, 54.f, 52.f, "CastIron");
		P.Box(36.f, 0.f, 73.5f, 54.f, 62.f, 4.f, "CastIron");
		P.Box(36.f, 0.f, 18.5f, 50.f, 58.f, 3.f, "CastIron");
		P.Box(59.6f, 0.f, 45.f, 2.f, 34.f, 30.f, "CastIron");
		P.Box(60.8f, 0.f, 46.f, 0.6f, 24.f, 16.f, "Ember", 0.f, 0.f, 0.f, false);
		P.Box(61.4f, 15.f, 46.f, 2.f, 2.f, 10.f, "Brass");
		// the flue up and into the wall
		P.Cyl(36.f, 0.f, 78.f, 18.f, 6.f, "CastIron");
		P.Cyl(36.f, 0.f, (80.f + FlueTop) * 0.5f, 13.f, FlueTop - 80.f, "Black");
		P.Cyl(36.f, 0.f, 140.f, 15.f, 3.f, "CastIron");
		P.Rod(FVector(36.f, 0.f, FlueTop - 7.f), FVector(-6.f, 0.f, FlueTop + 6.f), 13.f, "Black");
		P.Ball(36.f, 0.f, FlueTop - 6.f, 14.f, 14.f, 14.f, "Black");
		// a kettle and a poker
		P.Cyl(30.f, 12.f, 83.f, 18.f, 14.f, "Enamel");
		P.Ball(30.f, 12.f, 90.f, 16.f, 16.f, 6.f, "Enamel");
		P.Rod(FVector(36.f, 12.f, 84.f), FVector(46.f, 12.f, 92.f), 2.f, "Enamel");
		P.Rod(FVector(62.f, -38.f, 2.f), FVector(52.f, -42.f, 70.f), 1.6f, "Black");
		(void)Seed;
	}

	void DrawLogBasket(const FPen& P, int32 Seed)
	{
		P.Cyl(0.f, 0.f, 16.f, 44.f, 32.f, "Canvas");
		P.Cyl(0.f, 0.f, 31.f, 46.f, 3.f, "TimberDark");
		for (int32 i = 0; i < 6; ++i)
		{
			const float A = i * 60.f + J(Seed, i) * 15.f;
			const FV2 O = Rot(FV2(9.f, 0.f), A);
			P.S(EDCShape::Cylinder, O.X, O.Y, 36.f + (i % 2) * 6.f, 9.f, 9.f, 44.f, (i % 3) ? FName("TimberDark") : FName("Wood"), A, 90.f, 0.f);
		}
	}

	void DrawBookcase(const FPen& P, float W, int32 Seed)
	{
		const float D = 34.f, X = D * 0.5f;
		P.Box(1.f, 0.f, 96.f, 2.f, W, 190.f, "Wood");
		for (float S : { -1.f, 1.f }) { P.Box(X, S * (W * 0.5f - 1.5f), 96.f, D, 3.f, 192.f, "Walnut"); }
		P.Box(X, 0.f, 191.f, D + 2.f, W + 4.f, 3.f, "Walnut");
		P.Box(X + 1.f, 0.f, 5.f, D - 2.f, W - 4.f, 10.f, "Walnut");
		static const FName Books[] = { "BookRed", "BookBlue", "BookGreen", "BookTan", "LeatherBrn", "Walnut", "BookRed", "PaintOlive", "BookTan" };
		const float Shelves[] = { 10.f, 48.f, 86.f, 124.f, 158.f };
		for (int32 s = 0; s < 5; ++s)
		{
			const float Z = Shelves[s];
			P.Box(X, 0.f, Z, D - 2.f, W - 6.f, 2.5f, "Walnut");
			if (s == 4 && U(Seed, 200) > 0.5f) { continue; }
			float Y = -W * 0.5f + 5.f;
			int32 k = Seed * 7 + s * 31;
			const float Room = (s < 4 ? Shelves[s + 1] : 189.f) - Z - 4.f;
			while (Y < W * 0.5f - 6.f)
			{
				const float R = U(k, 1);
				if (R < 0.07f) { Y += 10.f + U(k, 2) * 10.f; ++k; continue; } // a gap
				if (R < 0.15f)
				{
					// a little stack lying flat
					const int32 N = 2 + (int32)(U(k, 3) * 3.f);
					for (int32 i = 0; i < N; ++i) { P.Box(X + 2.f, Y + 11.f, Z + 3.f + i * 3.6f, 22.f, 20.f, 3.4f, Books[(k + i) % 9], J(k, 4 + i) * 6.f); }
					Y += 24.f; ++k; continue;
				}
				if (R < 0.2f && Y < W * 0.5f - 20.f)
				{
					// an odd object: a bottle, a box, a jar
					if (U(k, 5) > 0.5f) { P.Cyl(X, Y + 5.f, Z + 13.f, 8.f, 22.f, "Bottle"); }
					else { P.Box(X, Y + 7.f, Z + 8.f, 18.f, 14.f, 12.f, "Cardboard"); }
					Y += 16.f; ++k; continue;
				}
				const float BW = 3.f + U(k, 6) * 3.5f, BH = FMath::Min(Room - 2.f, 20.f + U(k, 7) * 12.f), BD = 18.f + U(k, 8) * 6.f;
				P.Box(X + 1.f + (D - BD) * 0.3f, Y + BW * 0.5f, Z + 1.25f + BH * 0.5f, BD, BW, BH, Books[k % 9]);
				Y += BW + 0.4f; ++k;
			}
		}
	}

	void DrawSideboard(const FPen& P, float W, int32 Seed)
	{
		const float D = 46.f;
		for (float S : { -1.f, 1.f }) { for (float X : { 4.f, D - 4.f }) { P.Box(X, S * (W * 0.5f - 4.f), 6.f, 5.f, 5.f, 12.f, "Walnut"); } }
		P.Box(D * 0.5f, 0.f, 46.f, D, W, 68.f, "Wood");
		P.Box(D * 0.5f + 1.f, 0.f, 82.f, D + 4.f, W + 4.f, 4.f, "Walnut");
		const int32 Doors = W > 110.f ? 3 : 2;
		for (int32 i = 0; i < Doors; ++i)
		{
			const float Y = -W * 0.5f + W * (i + 0.5f) / Doors;
			P.Box(D + 0.6f, Y, 44.f, 1.2f, W / Doors - 6.f, 56.f, "Walnut");
			P.Ball(D + 1.8f, Y + (i % 2 ? -1.f : 1.f) * (W / Doors * 0.32f), 48.f, 2.6f, 2.6f, 2.6f, "Brass");
		}
		// an old valve radio, bottles and a photo in a frame
		const FPen R = P.At(22.f, -W * 0.22f, J(Seed, 1) * 6.f);
		R.Box(0.f, 0.f, 94.f, 22.f, 36.f, 20.f, "Walnut");
		R.Box(11.2f, -5.f, 94.f, 0.6f, 18.f, 14.f, "Felt");
		R.Cyl(11.5f, 11.f, 96.f, 6.f, 1.2f, "Brass", 90.f);
		R.Cyl(11.5f, 11.f, 89.f, 5.f, 1.2f, "Brass", 90.f);
		R.Box(11.4f, 11.f, 102.f, 0.4f, 8.f, 2.f, "LedA", 0.f, 0.f, 0.f, false);
		for (int32 i = 0; i < 3; ++i) { P.Cyl(20.f + J(Seed, 2 + i) * 6.f, W * 0.18f + i * 9.f, 84.f + 12.f + i * 1.5f, 7.f, 24.f + i * 3.f, i == 1 ? FName("Glass") : FName("Bottle")); }
		P.Box(14.f, W * 0.4f, 96.f, 2.f, 14.f, 18.f, "Brass", -20.f, -8.f);
		P.Box(15.2f, W * 0.4f, 96.f, 0.4f, 11.f, 14.f, "Photo", -20.f, -8.f);
	}

	void DrawFilingCabinets(const FPen& P, int32 N, int32 Seed)
	{
		const float CW = 46.f, D = 62.f, H = 132.f;
		for (int32 c = 0; c < N; ++c)
		{
			const float Y = (c - (N - 1) * 0.5f) * (CW + 1.f);
			const FName M = (c + Seed) % 2 ? FName("PaintOlive") : FName("PaintGrey");
			P.Box(D * 0.5f, Y, H * 0.5f, D, CW, H, M);
			const int32 Open = (U(Seed, 10 + c) > 0.65f) ? (int32)(U(Seed, 20 + c) * 4.f) : -1;
			for (int32 k = 0; k < 4; ++k)
			{
				const float Z = 18.f + k * 31.f;
				const float Out = k == Open ? 26.f : 0.f;
				if (Out > 0.f)
				{
					P.Box(D * 0.5f + Out, Y, Z, D, CW - 4.f, 26.f, M);
					for (int32 f = 0; f < 6; ++f) { P.Box(D * 0.5f + Out + 18.f - f * 7.f, Y, Z + 13.f, 1.2f, CW - 10.f, 8.f + U(Seed, 30 + f) * 4.f, f % 2 ? FName("Cardboard") : FName("Paper")); }
				}
				P.Box(D + 0.6f + Out, Y, Z, 1.2f, CW - 4.f, 27.f, M);
				P.Box(D + 2.f + Out, Y, Z + 7.f, 2.f, 12.f, 2.f, "Chrome");
				P.Box(D + 1.4f + Out, Y, Z + 2.f, 0.4f, 8.f, 3.f, "Paper", 0.f, 0.f, 0.f, false);
			}
		}
		P.Box(D * 0.5f, -10.f, H + 6.f, 30.f, 36.f, 12.f, "Cardboard", J(Seed, 1) * 10.f);
		P.Box(D * 0.4f, 12.f, H + 2.f, 26.f, 30.f, 4.f, "Paper", J(Seed, 2) * 15.f);
	}

	void DrawSafe(const FPen& P)
	{
		P.Box(31.f, 0.f, 46.f, 62.f, 62.f, 92.f, "PaintGreen");
		P.Box(62.4f, 0.f, 48.f, 1.f, 52.f, 80.f, "CastIron");
		P.Cyl(63.5f, -6.f, 60.f, 12.f, 2.f, "Chrome", 90.f);
		P.Box(64.f, 14.f, 50.f, 3.f, 3.f, 20.f, "Chrome");
		P.Box(62.6f, 0.f, 84.f, 0.4f, 30.f, 4.f, "Brass", 0.f, 0.f, 0.f, false);
	}

	void DrawFloorLamp(const FPen& P)
	{
		P.Cyl(0.f, 0.f, 1.5f, 28.f, 3.f, "Brass");
		P.Cyl(0.f, 0.f, 78.f, 2.6f, 152.f, "Brass");
		P.Cyl(0.f, 0.f, 160.f, 40.f, 28.f, "LampShade");
		P.Cyl(0.f, 0.f, 147.f, 34.f, 0.4f, "Tungsten", 0.f, 0.f, false);
	}

	void DrawCrates(const FPen& P, int32 N, int32 Seed)
	{
		for (int32 i = 0; i < N; ++i)
		{
			const float SX = 48.f - i * 4.f, SY = 62.f - i * 6.f, SZ = 38.f;
			const FName M = U(Seed, i) > 0.5f ? FName("Wood") : FName("Plywood");
			const float Yaw = J(Seed, 10 + i) * 7.f;
			P.Box(0.f, 0.f, i * SZ + SZ * 0.5f, SX, SY, SZ, M, Yaw);
			P.Box(0.f, 0.f, i * SZ + SZ * 0.5f, SX + 0.6f, 5.f, SZ - 6.f, "TimberDark", Yaw);
			P.Box(0.f, 0.f, i * SZ + SZ * 0.5f, 5.f, SY + 0.6f, SZ - 6.f, "TimberDark", Yaw);
		}
		if (U(Seed, 30) > 0.4f) { P.Box(J(Seed, 31) * 6.f, J(Seed, 32) * 6.f, N * 38.f + 9.f, 30.f, 22.f, 18.f, "PaintOlive", 30.f); } // ammo tin on top
	}

	void DrawDuffels(const FPen& P, int32 Seed)
	{
		// soft holdalls: squashed spheres with a zip line and carry straps
		struct FBag { float X, Y, Z, L, W, H, Yaw; FName M; };
		const FBag Bags[] = {
			{ 0.f, -15.f, 14.f, 68.f, 32.f, 28.f, 90.f + J(Seed, 1) * 10.f, "NylonOlive" },
			{ 3.f, 17.f, 13.f, 60.f, 30.f, 26.f, 96.f + J(Seed, 2) * 14.f, "Canvas" },
			{ 1.f, 0.f, 36.f, 56.f, 28.f, 22.f, 84.f + J(Seed, 3) * 12.f, "Tarp" } };
		for (const FBag& B : Bags)
		{
			P.S(EDCShape::Sphere, B.X, B.Y, B.Z, B.L, B.W, B.H, B.M, B.Yaw);
			P.Box(B.X, B.Y, B.Z + B.H * 0.5f - 0.5f, B.L * 0.7f, 1.2f, 1.f, "PlasticBlack", B.Yaw, 0.f, 0.f, false);
			P.Box(B.X, B.Y, B.Z + B.H * 0.5f + 1.5f, 5.f, 14.f, 1.5f, "PlasticBlack", B.Yaw + 90.f);
		}
	}

	void DrawPlant(const FPen& P, int32 Seed)
	{
		P.Cyl(0.f, 0.f, 18.f, 34.f, 36.f, "Terracotta");
		P.Cyl(0.f, 0.f, 36.f, 38.f, 4.f, "Terracotta");
		P.Cyl(0.f, 0.f, 37.f, 30.f, 1.f, "Dirt", 0.f, 0.f, false);
		// a leafy mound: overlapping blobs, bigger and lower at the rim, a few sprigs poking out
		P.Rod(FVector(0.f, 0.f, 36.f), FVector(J(Seed, 1) * 4.f, J(Seed, 2) * 4.f, 62.f), 3.f, "TimberDark");
		for (int32 i = 0; i < 14; ++i)
		{
			const float A = i * 137.5f + J(Seed, i) * 20.f, Rr = (i < 8 ? 16.f : 7.f) + U(Seed, 20 + i) * 6.f;
			const FV2 D = Dir(A) * Rr;
			const float Sz = (i < 8 ? 26.f : 30.f) + U(Seed, 40 + i) * 10.f;
			const float Zc = (i < 8 ? 56.f : 72.f) + U(Seed, 60 + i) * 12.f;
			P.Ball(D.X, D.Y, Zc, Sz, Sz * 0.9f, Sz * 0.8f, "Plant");
		}
		for (int32 i = 0; i < 4; ++i)
		{
			const FV2 D = Dir(i * 90.f + 40.f + J(Seed, 80 + i) * 20.f) * 24.f;
			P.Rod(FVector(D.X * 0.4f, D.Y * 0.4f, 66.f), FVector(D.X, D.Y, 92.f + U(Seed, 90 + i) * 10.f), 1.2f, "Plant");
		}
	}

	void DrawJerrycans(const FPen& P, int32 Seed)
	{
		for (int32 i = 0; i < 2; ++i)
		{
			const FPen C = P.At(0.f, (i - 0.5f) * 22.f, J(Seed, i) * 8.f);
			const FName M = i == 0 ? FName("PaintRed") : FName("PaintOlive");
			C.Box(0.f, 0.f, 23.f, 34.f, 17.f, 46.f, M);
			C.Box(-6.f, 0.f, 48.f, 14.f, 3.f, 4.f, M);
			C.Cyl(11.f, 0.f, 48.f, 5.f, 5.f, M);
		}
	}

	void DrawNewspapers(const FPen& P, int32 Seed)
	{
		const int32 N = 6 + (int32)(U(Seed, 1) * 5.f);
		for (int32 i = 0; i < N; ++i) { P.Box(J(Seed, 10 + i) * 2.f, J(Seed, 20 + i) * 2.f, 1.6f + i * 3.f, 30.f, 40.f, 3.f, i % 3 ? FName("Paper") : FName("Cardboard"), J(Seed, 30 + i) * 8.f); }
		P.Rod(FVector(-14.f, 0.f, N * 3.f + 1.f), FVector(14.f, 0.f, N * 3.f + 1.f), 0.6f, "Rope");
	}

	void DrawPictures(const FPen& P, float W, int32 Seed)
	{
		const int32 N = W > 100.f ? 3 : 2;
		for (int32 i = 0; i < N; ++i)
		{
			const float Y = (i - (N - 1) * 0.5f) * (W / N) + J(Seed, i) * 3.f;
			const bool bTall = (i + Seed) % 2 == 0;
			const float FW = bTall ? 26.f : 36.f, FH = bTall ? 36.f : 26.f;
			const float Z = 160.f + J(Seed, 10 + i) * 10.f;
			const FName Frame = (i == 1) ? FName("Brass") : FName("TimberDark");
			P.Box(1.5f, Y, Z, 3.f, FW + 5.f, FH + 5.f, Frame, 0.f, 0.f, J(Seed, 20 + i) * 2.f);
			P.Box(3.1f, Y, Z, 0.4f, FW, FH, i == 1 ? FName("Photo") : FName("Paper"), 0.f, 0.f, J(Seed, 20 + i) * 2.f);
			if (i != 1) { P.Box(3.4f, Y, Z - FH * 0.15f, 0.3f, FW * 0.7f, FH * 0.4f, (Seed + i) % 2 ? FName("PaintGreen") : FName("BookTan"), 0.f, 0.f, J(Seed, 20 + i) * 2.f, false); }
		}
	}

	void DrawClock(const FPen& P)
	{
		P.Cyl(2.f, 0.f, 172.f, 34.f, 4.f, "Walnut", 90.f);
		P.Cyl(4.3f, 0.f, 172.f, 28.f, 0.6f, "Paper", 90.f);
		P.Box(4.9f, 0.f, 175.f, 0.4f, 1.4f, 9.f, "Black", 0.f, 0.f, 20.f);
		P.Box(4.9f, 3.f, 171.f, 0.4f, 8.f, 1.2f, "Black", 0.f, 0.f, -15.f);
		P.Box(2.f, 0.f, 148.f, 3.f, 10.f, 22.f, "Walnut");
		P.Ball(3.f, 0.f, 140.f, 7.f, 2.f, 7.f, "Brass");
	}

	void DrawShotgunRack(const FPen& P)
	{
		for (float Y : { -38.f, 38.f }) { P.Box(4.f, Y, 166.f, 8.f, 4.f, 10.f, "Walnut"); }
		P.Box(7.f, 0.f, 170.f, 4.f, 104.f, 3.f, "CastIron", 0.f, 0.f, 0.f);
		P.Box(7.f, 46.f, 168.f, 5.f, 34.f, 7.f, "Walnut", 0.f, 0.f, -6.f);
		P.Box(7.f, 18.f, 168.5f, 4.f, 24.f, 4.f, "Walnut");
	}

	void DrawWallShelf(const FPen& P, float W, int32 Seed)
	{
		P.Box(11.f, 0.f, 158.f, 20.f, W, 2.5f, "Walnut");
		for (float Y : { -W * 0.35f, W * 0.35f }) { P.Box(4.f, Y, 150.f, 8.f, 2.5f, 14.f, "CastIron"); }
		float Y = -W * 0.5f + 8.f;
		int32 k = 0;
		while (Y < W * 0.5f - 8.f)
		{
			const float R = U(Seed, k);
			if (R < 0.45f) { P.Cyl(11.f, Y, 170.f, 8.f, 20.f + U(Seed, 50 + k) * 6.f, R < 0.2f ? FName("Glass") : FName("Bottle")); Y += 11.f; }
			else if (R < 0.75f) { P.Cyl(11.f, Y, 165.f, 11.f, 12.f, "Glass"); P.Cyl(11.f, Y, 172.f, 11.5f, 2.f, "Rust"); Y += 14.f; }
			else { P.Box(11.f, Y + 6.f, 168.f, 16.f, 12.f, 16.f, "Cardboard", J(Seed, 70 + k) * 8.f); Y += 15.f; }
			++k;
		}
	}

	void DrawWallScreen(const FPen& P)
	{
		P.Box(2.f, 0.f, 168.f, 4.f, 92.f, 55.f, "PlasticBlack");
		P.Box(4.2f, 0.f, 168.f, 0.4f, 86.f, 49.f, "Screen2", 0.f, 0.f, 0.f, false);
		P.Rod(FVector(1.f, 30.f, 140.f), FVector(1.f, 34.f, 2.f), 1.2f, "Rubber");
	}

	// ------------------------------------------------------------------------------------------------- context
	void FCtx::Spot(const FFrame& F, float X, float Y)
	{
		const FV2 P = F.O + Rot(FV2(X, Y), F.Yaw);
		if (Plan.Inside(P, 25.f)) { Spots.Add({ P, F.Yaw + 180.f }); }
	}

	void FCtx::Place(const TCHAR* Name, const TArray<FLocal>& Ls, const FFrame& F, FDrawFn Draw, FName Group, bool bCentreOnFirst)
	{
		Plan.Commit(Ls, F);
		FPlaced Pl;
		Pl.Name = Name;
		Pl.F = F;
		if (bCentreOnFirst && Ls.Num() > 0) { Pl.F.O = F.O + Rot(Ls[0].C, F.Yaw); } // a rug is drawn around its own centre
		Pl.Group = Group == NAME_None ? DCGroups::Shell : Group;
		Pl.Draw = MoveTemp(Draw);
		Out.Add(MoveTemp(Pl));
	}

	FFrame FCtx::BestSoft(const TArray<FLocal>& Ls, const TArray<FFrame>& Frames, TFunctionRef<float(const FFrame&)> Score, float Pad, float Penalty)
	{
		Plan.bSoftWindows = true;
		const FFrame F = Plan.Best(Ls, Frames, [&](const FFrame& Cand) -> float
		{
			Plan.WindowsHit(Ls, Cand, Hit);
			if (Hit.Num() > 0 && Plan.WindowsLeft - Hit.Num() < 1) { return -1e9f; }
			return Score(Cand) - Penalty * Hit.Num();
		}, Pad);
		Plan.bSoftWindows = false;
		return F;
	}

	void FCtx::PlaceSoft(const TCHAR* Name, const TArray<FLocal>& Ls, const FFrame& F, FDrawFn Draw, FName Group)
	{
		Plan.WindowsHit(Ls, F, Hit);
		if (Omit) { Plan.Brickup(Hit, Omit); }
		Place(Name, Ls, F, MoveTemp(Draw), Group);
	}

	bool FCtx::PlaceRug(const FFrame& F, float CX, float LX, float LY, FName Field, std::initializer_list<float> Scales)
	{
		for (float K : Scales)
		{
			const TArray<FLocal> Ls = { FLocal::Rug(CX, 0.f, LX * 0.5f * K, LY * 0.5f * K) };
			if (!Plan.Fits(Ls, F, 0.f)) { continue; }
			const float DX = LX * K, DY = LY * K;
			Place(TEXT("rug"), Ls, F, [DX, DY, Field](const FPen& P, int32) { DrawRug(P, DX, DY, Field, "RugBorder"); }, NAME_None, true);
			return true;
		}
		return false;
	}

	void FCtx::SetLamp(int32 Edge, float T)
	{
		if (Edge < 0 || Edge > 3) { return; }
		LampEdge = Edge; LampT = T;
		Plan.E[Edge].Bands.Add({ T - 30.f, T + 30.f, LampZ - 40.f, LampZ + 25.f, 75.f });
	}

	void FCtx::DefaultLamp()
	{
		for (int32 e : { 2, 1 })
		{
			if (Plan.E[e].Kind == EKind::Wall) { SetLamp(e, Plan.E[e].L * (0.5f + Jit(10) * 0.08f)); break; }
		}
	}

	// a runner along the main way in: the walking lane becomes part of the room
	void FCtx::Runner()
	{
		for (int32 e : { 0, 3 })
		{
			const FEdge& Ed = Plan.E[e];
			if (!Ed.bExit) { continue; }
			const FV2 A = Ed.At(Ed.L * 0.5f, Ed.Inset + 18.f);
			const FV2 D = Plan.Hub - A;
			const float Len = (float)D.Size() + 30.f;
			FFrame F; F.O = A; F.Yaw = YawOf(D); F.bValid = true;
			for (float K : { 1.f, 0.8f, 0.62f })
			{
				const TArray<FLocal> Ls = { FLocal::Rug(Len * K * 0.5f, 0.f, Len * K * 0.5f, 42.f) };
				if (!Plan.Fits(Ls, F, 0.f)) { continue; }
				// it's a walkway: nothing may stand on it
				FLocal Clear = Ls[0]; Clear.bRug = false; Clear.Z1 = 200.f;
				const FPart CP = Plan.Realise(Clear, F);
				bool bFree = true;
				for (const FPart& O : Plan.Parts) { if (!O.bRug && O.Z0 < 100.f && Overlap(CP.Box, O.Box, 6.f)) { bFree = false; break; } }
				if (!bFree) { continue; }
				const float LX = Len * K;
				Place(TEXT("rug"), Ls, F, [LX](const FPen& P, int32) { DrawRug(P, LX, 84.f, "RugBlue", "RugBorder"); }, NAME_None, true);
				Plan.Keep.Add(CP.Box);
				return;
			}
		}
	}

	// clutter, in clusters (corners, beside furniture)
	void FCtx::Clutter(float MaxCoverage)
	{
		enum { FCrates, FPlant, FDuffels, FCans, FPapers };
		struct FFill { int32 Kind; FV2 Half; float Z1; };
		const FFill Fills[] = {
			{ FCrates, FV2(30.f, 36.f), 90.f }, { FPlant, FV2(26.f, 26.f), 70.f }, { FDuffels, FV2(30.f, 40.f), 54.f },
			{ FCans, FV2(20.f, 26.f), 52.f }, { FPapers, FV2(24.f, 24.f), 40.f }, { FCrates, FV2(30.f, 36.f), 58.f } };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Fills) && Plan.Coverage() < MaxCoverage; ++i)
		{
			const FFill& Fl = Fills[i];
			const TArray<FLocal> Ls = { FLocal::Solid(Fl.Half.X + 2.f, 0.f, Fl.Half.X, Fl.Half.Y, Fl.Z1) };
			const FFrame F = Plan.Best(Ls, AnyWallF, [&](const FFrame& Cand)
			{
				const float Near = Plan.NearestSolid(Cand.O), Corner = Plan.CornerDist(Cand.O);
				if (Near > 70.f && Corner > 90.f) { return -1e9f; } // clutter gathers, it doesn't float mid-wall
				return (float)(-Near * 0.2f - Corner * 0.12f + J(Seed, 700 + i * 13 + Cand.Edge) * 10.f + U(Seed, 800 + (int32)Cand.T) * 4.f
					- FV2::DotProduct(Cand.O - Plan.Hub, KToCam) * 0.06f);
			}, 6.f);
			if (!F.bValid) { continue; }
			FFrame G = F; G.Yaw += J(Seed, 720 + i) * 12.f; // set down a bit askew
			const int32 N = Fl.Z1 > 80.f ? 2 : 1;
			switch (Fl.Kind)
			{
			case FCrates: Place(TEXT("crates"), Ls, G, [N](const FPen& P, int32 S) { DrawCrates(P.At(32.f, 0.f), N, S); }); break;
			case FPlant: Place(TEXT("plant"), Ls, G, [](const FPen& P, int32 S) { DrawPlant(P.At(28.f, 0.f), S); }); break;
			case FDuffels: Place(TEXT("bags"), Ls, G, [](const FPen& P, int32 S) { DrawDuffels(P.At(32.f, 0.f), S); }); break;
			case FCans: Place(TEXT("cans"), Ls, G, [](const FPen& P, int32 S) { DrawJerrycans(P.At(22.f, 0.f), S); }); break;
			default: Place(TEXT("papers"), Ls, G, [](const FPen& P, int32 S) { DrawNewspapers(P.At(26.f, 0.f), S); }); break;
			}
		}
	}

	// on the walls
	void FCtx::WallDecor()
	{
		enum { DPictures, DShotgun, DClock, DShelf };
		struct FDecor { int32 Kind; float W; float Z0, Z1, D; };
		const FDecor Decor[] = { { DPictures, 120.f, 135.f, 190.f, 6.f }, { DShotgun, 116.f, 158.f, 180.f, 12.f }, { DClock, 40.f, 128.f, 192.f, 8.f }, { DShelf, 86.f, 150.f, 186.f, 22.f } };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Decor); ++i)
		{
			const FDecor& Dc = Decor[i];
			TArray<FLocal> Ls = { FLocal::Solid(Dc.D * 0.5f, 0.f, Dc.D * 0.5f, Dc.W * 0.5f, Dc.Z1, Dc.Z0) };
			Ls[0].bSolid = false;
			const FFrame F = Plan.Best(Ls, WallF, [&](const FFrame& Cand)
			{
				// best above low furniture (a sofa, the desk, the sideboard), centred on it
				float Above = 0.f;
				FObb Probe; Probe.C = Cand.O + Dir(Cand.Yaw) * 45.f; Probe.H = FV2(40.f, Dc.W * 0.4f); Probe.Yaw = Cand.Yaw;
				for (const FPart& P : Plan.Parts)
				{
					if (P.bSolid && !P.bRug && P.Z1 < 110.f && Overlap(Probe, P.Box, 0.f)) { Above = FMath::Max(Above, 20.f - FV2::Distance(P.Box.C, Probe.C) * 0.05f); }
				}
				return (float)(Above + J(Seed, 900 + i * 7 + Cand.Edge) * 6.f - FMath::Abs(Cand.T - Plan.E[Cand.Edge].L * 0.5f) * 0.01f);
			}, 22.f);
			if (!F.bValid) { continue; }
			const float W = Dc.W;
			switch (Dc.Kind)
			{
			case DPictures: Place(TEXT("pictures"), Ls, F, [W](const FPen& P, int32 S) { DrawPictures(P, W, S); }); break;
			case DShotgun: Place(TEXT("shotgun"), Ls, F, [](const FPen& P, int32) { DrawShotgunRack(P); }); break;
			case DClock: Place(TEXT("clock"), Ls, F, [](const FPen& P, int32) { DrawClock(P); }); break;
			default: Place(TEXT("shelf"), Ls, F, [W](const FPen& P, int32 S) { DrawWallShelf(P, W, S); }); break;
			}
		}
	}
}

using namespace DCFurnish;

// =====================================================================================================================

bool ADCHideoutBuilder::FurnishGridRoom(ADCRoomModule* Room, const FVector2D Q[4], const int32 EdgeKind[4], const bool bExit[4], const bool bStepStart[4], const bool bStepEnd[4], float H, int32 WallSeed, int32 Seed, uint8 OutOmitWindows[4])
{
	if (!Room || Room->RoomType == EDCRoomType::Garage || Room->RoomType >= EDCRoomType::MAX) { return false; }
	const double T0 = FPlatformTime::Seconds();
	static const bool bDebug = FParse::Param(FCommandLine::Get(), TEXT("DCFurnishDebug"));
	DCFurnish::GDebug = bDebug;

	// ---------------------------------------------------------------- the room as the solver sees it
	DCFurnish::FCtx C;
	C.Room = Room; C.Seed = Seed; C.H = H; C.bDebug = bDebug; C.Omit = OutOmitWindows;
	DCFurnish::FPlan& Plan = C.Plan;
	Plan.Init(Q, EdgeKind, bExit);
	for (int32 e = 0; e < 4; ++e)
	{
		DCFurnish::FEdge& Ed = Plan.E[e];
		if (Ed.Kind != DCFurnish::EKind::Wall) { continue; }
		TArray<FOpening> Win;
		TArray<FVector> Steps;
		TallEdgePlan(Ed.L, H, WallSeed + e * 7, bStepStart[e], bStepEnd[e], Win, Steps);
		for (int32 w = 0; w < Win.Num(); ++w)
		{
			const FOpening& O = Win[w];
			Ed.Windows.Add({ O.Center, O.Width, O.Bottom, O.Top, w });
			Ed.Bands.Add({ O.Center - O.Width * 0.5f - 8.f, O.Center + O.Width * 0.5f + 8.f, O.Bottom - 2.f, O.Top + 14.f, 50.f, w });
			++Plan.WindowsLeft;
		}
		for (const FVector& S : Steps) { Ed.Bands.Add({ (float)S.X - 10.f, (float)S.Y + 10.f, (float)S.Z - 10.f, KTall, 60.f }); }
	}
	if (Plan.E[1].Kind == DCFurnish::EKind::Wall && Plan.E[2].Kind == DCFurnish::EKind::Wall)
	{
		// the corner pier where the two back walls meet
		DCFurnish::FPart Pier;
		Pier.Box.C = Q[2] + (-Q[2]).GetSafeNormal() * 14.f; Pier.Box.H = FV2(24.f, 24.f); Pier.Box.Yaw = YawOf(Plan.E[1].D);
		Pier.Z0 = 0.f; Pier.Z1 = KTall;
		Plan.Parts.Add(Pier);
	}
	C.LampZ = FMath::Max(FMath::Min(H - 35.f, 235.f), 214.f);
	Plan.WallFrames(MWall, false, 6.f, C.WallF);
	Plan.WallFrames(MWall | MPart, true, 6.f, C.WallPartF);
	Plan.WallFrames(MAny, false, 8.f, C.AnyWallF);
	Plan.FreeFrames(12.f, C.FreeF);
	for (int32 e = 0; e < 4; ++e) { OutOmitWindows[e] = 0; }

	// ---------------------------------------------------------------- the room's recipe
	switch (Room->RoomType)
	{
	case EDCRoomType::Office:       FurnishOffice(C); break;
	case EDCRoomType::Lounge:       FurnishLounge(C); break;
	case EDCRoomType::Workshop:     FurnishWorkshop(C); break;
	case EDCRoomType::OpsRoom:      FurnishOpsRoom(C); break;
	case EDCRoomType::Clinic:       FurnishClinic(C); break;
	case EDCRoomType::ServerRoom:   FurnishServerRoom(C); break;
	case EDCRoomType::LawyerOffice: FurnishLawyer(C); break;
	default: return false;
	}

	// ---------------------------------------------------------------- draw
	const FName Group0 = CurGroup;
	DCFurnish::FEmitFn Emit = [this](EDCShape S, const FVector& L, const FVector& Sz, FName M, const FRotator& R, bool bSh) { Deco(S, L, Sz, M, R, bSh); };
	for (const DCFurnish::FPlaced& Pl : C.Out)
	{
		if (!Pl.Draw) { continue; }
		CurGroup = Pl.Group;
		DCFurnish::FPen P; P.Emit = &Emit; P.O = Pl.F.O; P.Yaw = Pl.F.Yaw;
		Pl.Draw(P, Seed + (int32)(Pl.F.O.X * 0.37f) + (int32)(Pl.F.O.Y * 0.71f));
	}
	CurGroup = Group0;

	// the room light
	if (C.LampEdge >= 0)
	{
		BracketLamp(Q[C.LampEdge], Q[(C.LampEdge + 1) & 3], C.LampT / Plan.E[C.LampEdge].L, C.LampZ, FLinearColor(1.f, 0.66f, 0.38f), 30.f, 760.f);
	}
	else
	{
		Point(FVector(Plan.Hub.X, Plan.Hub.Y, FMath::Min(H - 40.f, 230.f)), FLinearColor(1.f, 0.66f, 0.38f), 26.f, 760.f, true, true, 4.f);
	}

	// hand the layout to the room: pawns keep out of the furniture and use the spots
	for (const DCFurnish::FPart& P : Plan.Parts)
	{
		if (!P.bSolid || P.bRug || P.Z0 > 60.f || (P.Z1 > 5000.f && P.Box.H.X < 26.f)) { continue; } // skip the corner pier
		Room->Footprints.Add({ P.Box.C, P.Box.H, P.Box.Yaw });
	}
	Room->UseSpots = C.Spots;
	if (OutOmitWindows[1] || OutOmitWindows[2]) { UE_LOG(LogDC, Log, TEXT("[Scene] furnishing bricked up windows: edge1 %d edge2 %d"), OutOmitWindows[1], OutOmitWindows[2]); }
	{
		FString Got;
		for (const DCFurnish::FPlaced& Pl : C.Out) { Got += Pl.Name; Got += TEXT(" "); }
		UE_LOG(LogDC, Log, TEXT("[Scene] furnished: %s"), *Got);
	}
	UE_LOG(LogDC, Log, TEXT("[Scene] furnished %s: %d groups, %d footprints, %d use spots, coverage %.0f%%, %.2f ms"),
		DC::RoomName(Room->RoomType), C.Out.Num(), Room->Footprints.Num(), C.Spots.Num(), Plan.Coverage() * 100.f, (FPlatformTime::Seconds() - T0) * 1000.0);
	return true;
}
