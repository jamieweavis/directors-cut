// Director's Cut — rural landscape content: terrain skins, meadow, trees, fields, fences, lane props, the garden,
// distant farm + windpump, and the night lights. Deterministic (fixed seeds). See DCLandscape.h.
#include "World/Scene/DCLandscape.h"
#include "World/Scene/DCDayNight.h"
#include "Core/DCSiteLayout.h"
#include "DirectorsCut.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Math/RandomStream.h"

namespace
{
	constexpr float GroundTop = -3.f;

	float Hash2(float X, float Y, float S = 0.f)
	{
		return FMath::Frac(FMath::Sin(X * 12.9898f + Y * 78.233f + S * 37.719f) * 43758.5453f);
	}
	float VNoise(float X, float Y, float S = 0.f)
	{
		const float Fx = FMath::Floor(X), Fy = FMath::Floor(Y);
		float Tx = X - Fx, Ty = Y - Fy;
		Tx = Tx * Tx * (3.f - 2.f * Tx); Ty = Ty * Ty * (3.f - 2.f * Ty);
		const float A = Hash2(Fx, Fy, S), B = Hash2(Fx + 1, Fy, S), C = Hash2(Fx, Fy + 1, S), D = Hash2(Fx + 1, Fy + 1, S);
		return FMath::Lerp(FMath::Lerp(A, B, Tx), FMath::Lerp(C, D, Tx), Ty);
	}
	bool InEllipse(const FVector2D& P, const FVector2D& C, const FVector2D& R, float Scale = 1.f)
	{
		const FVector2D D = (P - C) / (R * Scale);
		return D.SizeSquared() < 1.f;
	}

	// field rectangles (across the lane)
	// field rectangles (across the lane, measured from it)
	const FBox2D SoilField(FVector2D(-7200.f, DCSite::LaneY - 5100.f), FVector2D(-1100.f, DCSite::LaneY - 950.f));
	const FBox2D StubbleField(FVector2D(1100.f, DCSite::LaneY - 5100.f), FVector2D(7200.f, DCSite::LaneY - 950.f));
	constexpr float FenceY = DCSite::LaneY + 380.f;   // post-and-rail fence on the house side of the lane
	constexpr float BoundaryY = DCSite::LaneY - 430.f;// stone wall / hedgerow on the field side
	constexpr float PoleY = DCSite::LaneY - 330.f;    // telegraph poles on the far verge
}

bool ADCLandscape::Blocked(const FVector2D& P, float Margin, bool bAllowLawn) const
{
	using namespace DCSite;
	if (CompoundRect().ExpandBy(Margin).IsInside(P)) { return true; }
	if (YardRect().ExpandBy(Margin + 40.f).IsInside(P)) { return true; }
	if (FMath::Abs(P.Y - LaneY) < LaneW * 0.5f + Margin) { return true; }
	if (FMath::Abs(P.X - DriveX) < DriveW * 0.5f + Margin + 30.f && P.Y > LaneY && P.Y < YardRect().Min.Y + 20.f) { return true; }
	if (InEllipse(P, PondC, PondR + FVector2D(Margin + 60.f))) { return true; }
	if (!bAllowLawn && InEllipse(P, LawnC, LawnR, 0.97f)) { return true; }
	if (SoilField.ExpandBy(Margin).IsInside(P) || StubbleField.ExpandBy(Margin).IsInside(P)) { return true; }
	if (FVector2D::Distance(P, FireC) < 330.f + Margin) { return true; }
	return false;
}

void ADCLandscape::Sag(const FVector& A, const FVector& B, float Droop, int32 Segs, FName MatKey, float Thick, TArray<FVector>* OutPoints)
{
	FVector Prev = A;
	if (OutPoints) { OutPoints->Add(A); }
	for (int32 i = 1; i <= Segs; ++i)
	{
		const float t = (float)i / Segs;
		const FVector P = FMath::Lerp(A, B, t) - FVector(0, 0, Droop * 4.f * t * (1.f - t));
		Rod(MatKey, Prev, P, Thick, false);
		if (OutPoints) { OutPoints->Add(P); }
		Prev = P;
	}
}

// =====================================================================================================================
// Terrain: collidable base + ground skins
// =====================================================================================================================

void ADCLandscape::BuildTerrain()
{
	using namespace DCSite;
	UStaticMesh* Cube = MeshLib.FindRef("Cube");
	UStaticMesh* Disc = MeshLib.FindRef("Disc");
	// Base: 1.2 km square, 1 m thick, top at Z = -3. The only collidable ground (WorldStatic + Visibility).
	UStaticMeshComponent* Base = Slab("Meadow", Cube, FVector(0.f, 0.f, GroundTop - 50.f), FVector(1200.f, 1200.f, 1.f), 0.f, true);
	Base->SetCastShadow(false);
	Base->ComponentTags.Add(FName("DCGround"));

	// skins (no collision), each a few mm higher than the one below
	auto Skin = [&](FName Mat, const FBox2D& R, float Top)
	{
		const FVector2D C = R.GetCenter(), S = R.GetSize();
		Slab(Mat, Cube, FVector(C.X, C.Y, Top - 0.5f), FVector(S.X / 100.f, S.Y / 100.f, 0.01f));
	};
	auto Strip = [&](FName Mat, const FVector2D& A, const FVector2D& B, float W, float Top)
	{
		const FVector2D D = B - A;
		const FVector2D C = (A + B) * 0.5f;
		Slab(Mat, Cube, FVector(C.X, C.Y, Top - 0.5f), FVector((D.Size() + W * 0.6f) / 100.f, W / 100.f, 0.01f), FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)));
	};
	// mown lawn round the house
	Slab("Lawn", Disc, FVector(LawnC.X, LawnC.Y, GroundTop + 0.4f), FVector(LawnR.X / 50.f, LawnR.Y / 50.f, 1.f), 12.f);
	// fields across the lane
	Skin("Soil", SoilField, GroundTop + 0.5f);
	Skin("Stubble", StubbleField, GroundTop + 0.5f);
	// lane with verges + centre strip, driveway, gravel yard
	Skin("Lane", FBox2D(FVector2D(LaneX0 - 6000.f, LaneY - (LaneW + 240.f) * 0.5f), FVector2D(LaneX1 + 6000.f, LaneY + (LaneW + 240.f) * 0.5f)), GroundTop + 0.6f);
	Skin("Drive", FBox2D(FVector2D(DriveX - (DriveW + 160.f) * 0.5f, LaneY + LaneW * 0.5f - 40.f), FVector2D(DriveX + (DriveW + 160.f) * 0.5f, YardRect().Min.Y + 40.f)), GroundTop + 0.7f);
	Skin("Gravel", YardRect().ExpandBy(70.f), GroundTop + 0.8f);
	// footpaths: house -> campfire -> pond, yard -> house front
	const FBox2D Comp = CompoundRect();
	const FVector2D Door(Comp.Min.X - 20.f, FireC.Y - 150.f);
	const FVector2D Bend((Door.X + FireC.X) * 0.5f, FireC.Y - 110.f);
	const FVector2D PondEdge(PondC.X + PondR.X * 0.35f, PondC.Y - PondR.Y * 1.05f);
	const FVector2D PondMid(FireC.X - 350.f, (FireC.Y + PondEdge.Y) * 0.5f);
	Strip("Path", Door, Bend, 120.f, GroundTop + 0.65f);
	Strip("Path", Bend, FireC + FVector2D(150.f, -30.f), 120.f, GroundTop + 0.65f);
	Strip("Path", FireC + FVector2D(-120.f, 200.f), PondMid, 100.f, GroundTop + 0.65f);
	Strip("Path", PondMid, PondEdge, 100.f, GroundTop + 0.65f);
	Strip("Path", FVector2D(YardRect().Min.X + 120.f, YardRect().Max.Y + 30.f), FVector2D(Comp.Min.X - 60.f, Comp.Min.Y + 250.f), 110.f, GroundTop + 0.65f);
	// pond: muddy rim + water
	Slab("Mud", Disc, FVector(PondC.X, PondC.Y, GroundTop + 0.9f), FVector(PondR.X * 1.22f / 50.f, PondR.Y * 1.25f / 50.f, 1.f), 8.f);
	Slab("Water", Disc, FVector(PondC.X, PondC.Y, GroundTop + 1.6f), FVector(PondR.X / 50.f, PondR.Y / 50.f, 1.f), 8.f);

	// gentle hills far out (never near the walkable area)
	FRandomStream R(4242);
	for (int32 i = 0; i < 46; ++i)
	{
		const float A = R.FRandRange(0.f, 2.f * PI);
		const float D = R.FRandRange(11000.f, 26000.f);
		const FVector2D P(FMath::Cos(A) * D, FMath::Sin(A) * D);
		if (FMath::Abs(P.Y - LaneY) < 1500.f) { continue; }
		const float S = R.FRandRange(40.f, 110.f);
		Add("Hill", "HillMat", FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, -S * 4.f), FVector(S, S * R.FRandRange(0.5f, 0.9f), S * R.FRandRange(0.11f, 0.2f))), false);
	}
}

// =====================================================================================================================
// Meadow grass, wildflowers, reeds
// =====================================================================================================================

void ADCLandscape::BuildVegetation()
{
	using namespace DCSite;
	FRandomStream R(1337);
	const FName Tufts[3] = { "Tuft0", "Tuft1", "Tuft2" };
	auto Grid = [&](float Ext, float Step, float Inner, float ScaleK)
	{
		for (float Y = -Ext; Y <= Ext; Y += Step)
		{
			for (float X = -Ext; X <= Ext; X += Step)
			{
				const FVector2D P(X + R.FRandRange(-0.5f, 0.5f) * Step, Y + R.FRandRange(-0.5f, 0.5f) * Step);
				const float Rad = P.Size();
				if (Rad < Inner || Rad > Ext) { continue; }
				const float Dens = VNoise(P.X * 0.0012f, P.Y * 0.0012f, 3.f);
				if (R.FRand() > 0.35f + 0.8f * Dens) { continue; }
				if (Blocked(P, 0.f, false)) { continue; }
				const float H = (0.3f + 0.55f * Dens) * R.FRandRange(0.7f, 1.2f) * ScaleK;
				const bool bLush = FVector2D::Distance(P, PondC) < 1400.f;
				const float Sx = R.FRandRange(0.9f, 1.35f);
				Add(Tufts[R.RandRange(0, 2)], bLush ? FName("GrassLush") : FName("Grass"),
					FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, GroundTop), FVector(Sx, Sx, H)), false);
			}
		}
	};
	Grid(7000.f, 85.f, 0.f, 1.f);
	Grid(13500.f, 150.f, 7000.f, 1.15f);

	// a ragged fringe of taller grass along the lawn edge so the mown patch reads as mown
	for (int32 i = 0; i < 520; ++i)
	{
		const float A = R.FRandRange(0.f, 2.f * PI);
		const float K = R.FRandRange(0.93f, 1.03f);
		const FVector2D P = LawnC + FVector2D(FMath::Cos(A) * LawnR.X * K, FMath::Sin(A) * LawnR.Y * K);
		if (Blocked(P, 0.f, true)) { continue; }
		Add(Tufts[i % 3], "Grass", FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, GroundTop), FVector(0.7f, 0.7f, R.FRandRange(0.25f, 0.5f))), false);
	}
	// sparse low tufts on the lawn itself
	for (int32 i = 0; i < 900; ++i)
	{
		const FVector2D P = LawnC + FVector2D(R.FRandRange(-1.f, 1.f) * LawnR.X, R.FRandRange(-1.f, 1.f) * LawnR.Y);
		if (!InEllipse(P, LawnC, LawnR, 0.95f) || Blocked(P, 30.f, true)) { continue; }
		if (P.X < CompoundRect().Min.X && P.X > FireC.X - 300.f && FMath::Abs(P.Y - FireC.Y) < 400.f) { continue; } // keep the fire path clear-ish
		Add("Tuft2", "GrassLush", FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, GroundTop), FVector(0.6f, 0.6f, R.FRandRange(0.1f, 0.18f))), false);
	}

	// wildflower drifts
	const FName Petals[4] = { "PetalWhite", "PetalYellow", "PetalViolet", "PetalRed" };
	int32 Clusters = 0;
	for (int32 Try = 0; Try < 400 && Clusters < 85; ++Try)
	{
		const float A = R.FRandRange(0.f, 2.f * PI);
		const float D = R.FRandRange(2400.f, 9000.f);
		const FVector2D C(FMath::Cos(A) * D, FMath::Sin(A) * D);
		if (Blocked(C, 100.f, false)) { continue; }
		++Clusters;
		const FName Pet = Petals[R.RandRange(0, 3)];
		const int32 N = R.RandRange(10, 26);
		for (int32 k = 0; k < N; ++k)
		{
			const FVector2D P = C + FVector2D(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f)) * R.FRandRange(40.f, 320.f);
			if (Blocked(P, 0.f, false)) { continue; }
			const float S = R.FRandRange(0.8f, 1.25f);
			Add(k % 2 ? FName("Flowers0") : FName("Flowers1"), Pet, FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, GroundTop), FVector(S, S, R.FRandRange(0.8f, 1.2f))), false);
		}
	}
	// a few flowers in the lawn edge near the fire / pond path
	for (int32 k = 0; k < 40; ++k)
	{
		const FVector2D P = FMath::Lerp(FireC, PondC, 0.5f) + FVector2D(R.FRandRange(-450.f, 350.f), R.FRandRange(-600.f, 600.f));
		if (Blocked(P, 20.f, true)) { continue; }
		Add("Flowers0", Petals[k % 4], FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, GroundTop), FVector(0.9f, 0.9f, 0.6f)), false);
	}

	// reeds + lily pads at the pond
	for (int32 i = 0; i < 95; ++i)
	{
		const float A = R.FRandRange(0.f, 2.f * PI);
		if (FMath::Sin(A * 2.f + 0.6f) < -0.35f) { continue; } // leave open banks
		const float K = R.FRandRange(0.88f, 1.18f);
		const FVector2D P = PondC + FVector2D(FMath::Cos(A) * PondR.X * K, FMath::Sin(A) * PondR.Y * K);
		Add("Reed", "Reeds", FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, GroundTop), FVector(0.7f, 0.7f, R.FRandRange(1.1f, 1.7f))), false);
	}
	for (int32 i = 0; i < 11; ++i)
	{
		const float A = R.FRandRange(0.f, 2.f * PI);
		const FVector2D P = PondC + FVector2D(FMath::Cos(A) * PondR.X, FMath::Sin(A) * PondR.Y) * R.FRandRange(0.3f, 0.75f);
		const float S = R.FRandRange(0.35f, 0.6f);
		Add("DiscRound", "Lily", FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, GroundTop + 2.1f), FVector(S, S, 1.f)), false);
	}
	// stepping stones + bank rocks
	for (int32 i = 0; i < 14; ++i)
	{
		const float A = R.FRandRange(0.f, 2.f * PI);
		const FVector2D P = PondC + FVector2D(FMath::Cos(A) * PondR.X, FMath::Sin(A) * PondR.Y) * R.FRandRange(1.05f, 1.2f);
		const float S = R.FRandRange(0.4f, 0.9f);
		Add(i % 2 ? FName("Rock0") : FName("Rock1"), i % 3 ? FName("Stone") : FName("StoneDark"), FTransform(FRotator(R.FRandRange(-10.f, 10.f), R.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, GroundTop), FVector(S, S * 0.8f, S * 0.45f)));
	}
	// scattered field rocks
	for (int32 i = 0, Placed = 0; i < 300 && Placed < 60; ++i)
	{
		const FVector2D P(R.FRandRange(-9000.f, 9000.f), R.FRandRange(-9000.f, 9000.f));
		if (Blocked(P, 150.f, false) || BuildingClearRect().IsInside(P)) { continue; }
		++Placed;
		const float S = R.FRandRange(0.4f, 1.6f);
		Add(i % 2 ? FName("Rock0") : FName("Rock1"), i % 3 ? FName("Stone") : FName("StoneDark"), FTransform(FRotator(R.FRandRange(-12.f, 12.f), R.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, GroundTop - S * 8.f), FVector(S, S * R.FRandRange(0.6f, 1.f), S * R.FRandRange(0.4f, 0.7f))));
	}
}

// =====================================================================================================================
// Trees
// =====================================================================================================================

void ADCLandscape::Tree(const FVector& Base, float Height, int32 Kind, uint32 Seed)
{
	FRandomStream R(Seed);
	const float Yaw = R.FRandRange(0.f, 360.f);
	if (Kind == 2) // pine
	{
		Add("Trunk", "Bark", FTransform(FRotator(0.f, Yaw, 0.f), Base, FVector(Height * 0.028f / 50.f, Height * 0.028f / 50.f, Height * 0.5f / 100.f)));
		const int32 Tiers = 4;
		for (int32 t = 0; t < Tiers; ++t)
		{
			const float u = (float)t / Tiers;
			const float Rad = Height * 0.27f * (1.f - 0.2f * t);
			const float Z = Height * (0.18f + 0.2f * t);
			Add("Skirt", "Pine", FTransform(FRotator(0.f, Yaw + t * 37.f, 0.f), Base + FVector(0, 0, Z), FVector(Rad / 50.f, Rad / 50.f, Height * (0.42f - 0.04f * t) / 100.f)));
			(void)u;
		}
		return;
	}
	if (Kind == 3) // bush
	{
		const int32 N = R.RandRange(2, 4);
		for (int32 i = 0; i < N; ++i)
		{
			const float S = Height * R.FRandRange(0.35f, 0.55f) / 50.f;
			const FVector Off(R.FRandRange(-0.3f, 0.3f) * Height, R.FRandRange(-0.3f, 0.3f) * Height, Height * 0.35f);
			Add(i % 2 ? FName("Clump0") : FName("Clump1"), "Hedge", FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), Base + Off, FVector(S, S, S * 0.75f)));
		}
		return;
	}
	// broadleaf
	const float TrunkR = Height * 0.035f;
	const float TrunkH = Height * 0.58f;
	const FRotator Lean(R.FRandRange(-4.f, 4.f), Yaw, R.FRandRange(-4.f, 4.f));
	Add("Trunk", "Bark", FTransform(Lean, Base - FVector(0, 0, 10.f), FVector(TrunkR / 50.f, TrunkR / 50.f, (TrunkH + 10.f) / 100.f)));
	const FVector Top = Base + Lean.RotateVector(FVector(0, 0, TrunkH));
	// two or three limbs
	const int32 Limbs = R.RandRange(2, 3);
	for (int32 l = 0; l < Limbs; ++l)
	{
		const FRotator LR(R.FRandRange(30.f, 50.f), Yaw + l * (360.f / Limbs) + R.FRandRange(-20.f, 20.f), 0.f);
		const FVector Dir = FRotationMatrix(FRotator(90.f - LR.Pitch, LR.Yaw, 0.f)).GetUnitAxis(EAxis::X);
		Add("Trunk", "Bark", FTransform(FRotationMatrix::MakeFromZ(Dir).Rotator(), Top - FVector(0, 0, Height * 0.1f), FVector(TrunkR * 0.55f / 50.f, TrunkR * 0.55f / 50.f, Height * 0.3f / 100.f)));
	}
	// canopy: clustered lumps of varied greens (some autumn)
	const FName GreenSet[2] = { "LeafGreen", "LeafDeep" };
	const FName AutumnSet[3] = { "LeafGold", "LeafRust", "LeafGreen" };
	const int32 N = R.RandRange(6, 9);
	const FVector CC = Top + FVector(0, 0, Height * 0.12f);
	for (int32 i = 0; i < N; ++i)
	{
		const float A = R.FRandRange(0.f, 2.f * PI);
		const float D = (i == 0) ? 0.f : R.FRandRange(0.12f, 0.27f) * Height;
		const FVector P = CC + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, R.FRandRange(-0.08f, 0.16f) * Height - D * 0.25f);
		const float S = Height * R.FRandRange(0.17f, 0.24f) / 50.f;
		const FName Mat = (Kind == 1) ? AutumnSet[R.RandRange(0, 2)] : GreenSet[(i + Seed) % 2 == 0 ? 0 : (R.FRand() < 0.6f ? 0 : 1)];
		Add(i % 2 ? FName("Clump0") : FName("Clump1"), Mat, FTransform(FRotator(R.FRandRange(-15.f, 15.f), R.FRandRange(0.f, 360.f), 0.f), P, FVector(S, S, S * 0.82f)));
	}
}

void ADCLandscape::BuildTrees()
{
	using namespace DCSite;
	FRandomStream R(777);
	auto Clear = [&](const FVector2D& P, float M) { return !Blocked(P, M, false) && !BuildingClearRect().ExpandBy(M).IsInside(P) && !InEllipse(P, LawnC, LawnR, 1.0f); };

	// woodland stand behind the house (+X+Y), mixed with pines and a few autumn trees
	const FVector2D Wood(BuildingClearRect().Max.X + 2500.f, BuildingClearRect().Max.Y + 2600.f);
	int32 Placed = 0;
	for (int32 i = 0; i < 400 && Placed < 46; ++i)
	{
		const FVector2D P = Wood + FVector2D(R.FRandRange(-1.f, 1.f) * 2900.f, R.FRandRange(-1.f, 1.f) * 2300.f);
		if (!Clear(P, 250.f) || ((P - Wood) / FVector2D(2900.f, 2300.f)).Size() > 1.f) { continue; }
		++Placed;
		const float Pick = R.FRand();
		const int32 Kind = Pick < 0.25f ? 2 : (Pick < 0.42f ? 1 : 0);
		Tree(FVector(P.X, P.Y, GroundTop), R.FRandRange(800.f, 1250.f), Kind, 1000 + i);
		if (R.FRand() < 0.5f) { Tree(FVector(P.X + R.FRandRange(-300.f, 300.f), P.Y + R.FRandRange(-300.f, 300.f), GroundTop), R.FRandRange(150.f, 260.f), 3, 5000 + i); }
	}
	// hand-placed specimen trees framing the scene
	struct FSpec { float X, Y, H; int32 K; };
	const FSpec Specs[] = {
		{ -3900.f,  2900.f, 1150.f, 0 },  // big tree over the pond
		{ -4600.f, -1900.f,  900.f, 1 },  // autumn tree out in the meadow
		{  2700.f, -1300.f,  980.f, 0 },  // beside the yard
		{  2600.f,   900.f, 1050.f, 1 },
		{ -1900.f,  3300.f,  950.f, 0 },
		{  1300.f,  3500.f, 1150.f, 2 },
		{  -500.f,  3700.f, 1000.f, 0 },
		{ -4700.f,   200.f, 1050.f, 2 },
		{ -4300.f, -2200.f,  850.f, 0 },
		{  3600.f, -2400.f,  900.f, 1 },
	};
	for (int32 i = 0; i < UE_ARRAY_COUNT(Specs); ++i)
	{
		const FVector2D P(Specs[i].X * 0.8f, Specs[i].Y * 0.8f); // laid out for the first site plan; the house shrank
		if (!Clear(P, 150.f)) { continue; }
		Tree(FVector(P.X, P.Y, GroundTop), Specs[i].H, Specs[i].K, 300 + i);
	}
	// bushes round the meadow edge
	for (int32 i = 0, N = 0; i < 300 && N < 40; ++i)
	{
		const float A = R.FRandRange(0.f, 2.f * PI);
		const float D = R.FRandRange(3200.f, 7000.f);
		const FVector2D P(FMath::Cos(A) * D, FMath::Sin(A) * D);
		if (!Clear(P, 200.f)) { continue; }
		++N;
		Tree(FVector(P.X, P.Y, GroundTop), R.FRandRange(140.f, 280.f), 3, 6000 + i);
	}
	// outer belt of trees (clustered by noise) so the world continues past the default framing
	for (int32 i = 0, N = 0; i < 2000 && N < 170; ++i)
	{
		const float A = R.FRandRange(0.f, 2.f * PI);
		const float D = R.FRandRange(7600.f, 15000.f);
		const FVector2D P(FMath::Cos(A) * D, FMath::Sin(A) * D);
		if (VNoise(P.X * 0.0004f, P.Y * 0.0004f, 9.f) < 0.5f || !Clear(P, 300.f) || FMath::Abs(P.Y - LaneY) < 900.f) { continue; }
		++N;
		const float Pick = R.FRand();
		Tree(FVector(P.X, P.Y, GroundTop), R.FRandRange(850.f, 1400.f), Pick < 0.3f ? 2 : (Pick < 0.45f ? 1 : 0), 9000 + i);
	}
}

// =====================================================================================================================
// Fields, fences, walls, hedges
// =====================================================================================================================

void ADCLandscape::BuildFieldsAndFences()
{
	using namespace DCSite;
	FRandomStream R(2024);
	// crop rows in the ploughed field (ridges every 90 cm across Y, matching the soil pattern)
	for (float Y = FMath::CeilToFloat(SoilField.Min.Y / 90.f) * 90.f + 22.5f; Y < SoilField.Max.Y - 60.f; Y += 90.f)
	{
		for (float X = SoilField.Min.X + 80.f; X < SoilField.Max.X - 80.f; X += 48.f)
		{
			const float S = R.FRandRange(0.38f, 0.5f);
			Add("Tuft2", "Crop", FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), FVector(X + R.FRandRange(-8.f, 8.f), Y, GroundTop), FVector(S, S, R.FRandRange(0.32f, 0.46f))), false);
		}
	}
	// round hay bales on the stubble
	for (int32 i = 0; i < 16; ++i)
	{
		const FVector2D P(R.FRandRange(StubbleField.Min.X + 400.f, StubbleField.Max.X - 400.f), R.FRandRange(StubbleField.Min.Y + 400.f, StubbleField.Max.Y - 500.f));
		const float Yaw = R.FRandRange(0.f, 180.f);
		Add("Cyl", "Hay", FTransform(FRotator(90.f, Yaw, 0.f), FVector(P.X, P.Y, GroundTop + 72.f), FVector(1.45f, 1.45f, 1.25f)));
		if (i % 5 == 0) { Add("Cyl", "Hay", FTransform(FRotator(90.f, Yaw, 0.f), FVector(P.X + 150.f * FMath::Cos(FMath::DegreesToRadians(Yaw + 90.f)), P.Y + 150.f * FMath::Sin(FMath::DegreesToRadians(Yaw + 90.f)), GroundTop + 72.f), FVector(1.45f, 1.45f, 1.25f))); }
	}

	// post-and-rail fence on the house side of the lane, open at the driveway
	const float GateHalf = 260.f;
	for (float X = -5800.f; X <= 5800.f; X += 260.f)
	{
		const bool bGap = FMath::Abs(X - DriveX) < GateHalf - 1.f;
		if (bGap) { continue; }
		const float Lean = R.FRandRange(-3.f, 3.f);
		Add("Post", "TimberV", FTransform(FRotator(Lean, 0.f, R.FRandRange(-3.f, 3.f)), FVector(X, FenceY, GroundTop - 10.f), FVector(0.15f, 0.15f, 1.25f)));
		const float NX = X + 260.f;
		if (NX > 5800.f || FMath::Abs(NX - DriveX) < GateHalf - 1.f) { continue; }
		if (R.FRand() < 0.06f) { Rod("Timber", FVector(X, FenceY, 60.f), FVector(NX, FenceY + 10.f, 5.f), 9.f); continue; } // broken rail
		Rod("Timber", FVector(X, FenceY, 58.f), FVector(NX, FenceY, 58.f + R.FRandRange(-4.f, 4.f)), 9.f);
		Rod("Timber", FVector(X, FenceY, 98.f), FVector(NX, FenceY, 98.f + R.FRandRange(-4.f, 4.f)), 9.f);
	}
	// gate posts + an open five-bar gate
	const FVector GL(DriveX - GateHalf, FenceY, GroundTop), GR(DriveX + GateHalf, FenceY, GroundTop);
	for (const FVector& G : { GL, GR }) { Add("Post", "TimberV", FTransform(FRotator::ZeroRotator, G - FVector(0, 0, 10.f), FVector(0.24f, 0.24f, 1.45f))); }
	{
		const FRotator GateRot(0.f, 72.f, 0.f); // swung open towards the yard
		const FVector Hinge = GL + FVector(14.f, 0, 0);
		for (int32 b = 0; b < 5; ++b)
		{
			const float Z = 30.f + b * 22.f;
			Rod("PaintWhite", Hinge + FVector(0, 0, Z), Hinge + GateRot.RotateVector(FVector(330.f, 0, 0)) + FVector(0, 0, Z), 7.f);
		}
		Rod("PaintWhite", Hinge + FVector(0, 0, 30.f), Hinge + GateRot.RotateVector(FVector(330.f, 0, 0)) + FVector(0, 0, 118.f), 7.f);
		Rod("PaintWhite", Hinge + GateRot.RotateVector(FVector(330.f, 0, 0)) + FVector(0, 0, 25.f), Hinge + GateRot.RotateVector(FVector(330.f, 0, 0)) + FVector(0, 0, 122.f), 8.f);
	}

	// dry-stone wall along the ploughed field
	for (float X = SoilField.Min.X; X < SoilField.Max.X + 300.f; X += 52.f)
	{
		for (int32 Course = 0; Course < 3; ++Course)
		{
			const float XX = X + (Course % 2) * 26.f + R.FRandRange(-6.f, 6.f);
			const float S = (Course == 2) ? R.FRandRange(0.38f, 0.5f) : R.FRandRange(0.55f, 0.75f);
			const float Z = GroundTop + 18.f + Course * 30.f;
			Add(Course % 2 ? FName("Rock0") : FName("Rock1"), R.FRand() < 0.4f ? FName("StoneDark") : FName("Stone"),
				FTransform(FRotator(R.FRandRange(-10.f, 10.f), R.FRandRange(-15.f, 15.f), Course == 2 ? 70.f : 0.f), FVector(XX, BoundaryY + R.FRandRange(-6.f, 6.f), Z), FVector(S * 1.1f, S * 0.75f, S * 0.6f)));
		}
	}
	// hedgerow along the stubble field, with a field gate gap
	for (float X = StubbleField.Min.X - 300.f; X < StubbleField.Max.X + 400.f; X += 170.f)
	{
		if (FMath::Abs(X - 3600.f) < 250.f) { continue; }
		const float S = R.FRandRange(2.6f, 3.4f);
		Add(R.FRand() < 0.5f ? FName("Clump0") : FName("Clump1"), "Hedge", FTransform(FRotator(0.f, R.FRandRange(-20.f, 20.f), 0.f), FVector(X, BoundaryY + R.FRandRange(-20.f, 20.f), GroundTop + 60.f), FVector(S, S * 0.65f, S * 0.7f)));
	}
	Tree(FVector(5200.f, BoundaryY - 60.f, GroundTop), 1000.f, 0, 4401);
	Tree(FVector(1900.f, BoundaryY - 80.f, GroundTop), 820.f, 1, 4402);
	// short hedge along the far side of both fields
	for (float X = -7400.f; X < 7400.f; X += 190.f)
	{
		const float S = R.FRandRange(2.4f, 3.2f);
		Add("Clump0", "Hedge", FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), FVector(X, SoilField.Min.Y - 150.f + R.FRandRange(-20.f, 20.f), GroundTop + 50.f), FVector(S, S * 0.7f, S * 0.65f)));
	}
}

// =====================================================================================================================
// Lane props: telegraph poles + wires, mailbox
// =====================================================================================================================

void ADCLandscape::BuildLaneProps()
{
	using namespace DCSite;
	FRandomStream R(31);
	TArray<FVector> Tops;
	for (float X = -13000.f; X <= 13000.f; X += 2600.f)
	{
		const FVector B(X + R.FRandRange(-60.f, 60.f), PoleY, GroundTop);
		const float H = 860.f;
		Add("Post", "Timber", FTransform(FRotator(R.FRandRange(-1.5f, 1.5f), 0.f, R.FRandRange(-1.5f, 1.5f)), B - FVector(0, 0, 20.f), FVector(0.26f, 0.26f, (H + 20.f) / 100.f)));
		Box("Timber", B + FVector(0, 0, H - 40.f), FVector(14.f, 170.f, 12.f));
		for (float S : { -70.f, 70.f }) { Add("Cyl", "PaintWhite", FTransform(FRotator::ZeroRotator, B + FVector(0, S, H - 22.f), FVector(0.07f, 0.07f, 0.16f))); }
		Tops.Add(B + FVector(0, 0, H - 14.f));
	}
	for (int32 i = 0; i + 1 < Tops.Num(); ++i)
	{
		for (float S : { -70.f, 70.f }) { Sag(Tops[i] + FVector(0, S, 0), Tops[i + 1] + FVector(0, S, 0), 75.f, 10, "Wire", 2.2f); }
	}
	// mailbox on a post by the gate
	const FVector MB(DriveX + 360.f, FenceY - 70.f, GroundTop);
	Add("Post", "Timber", FTransform(FRotator::ZeroRotator, MB, FVector(0.14f, 0.14f, 1.05f)));
	Box("PaintGreen", MB + FVector(0, 0, 118.f), FVector(48.f, 26.f, 26.f));
	Add("Cyl", "PaintGreen", FTransform(FRotator(90.f, 0.f, 0.f), MB + FVector(0, 0, 131.f), FVector(0.26f, 0.26f, 0.48f)));
	Box("PaintRed", MB + FVector(-10.f, -15.f, 140.f), FVector(4.f, 2.f, 22.f));
	// verge marker posts
	for (float X = -6000.f; X <= 6000.f; X += 1300.f)
	{
		if (FMath::Abs(X - DriveX) < 600.f) { continue; }
		Box("PaintWhite", FVector(X, LaneY + LaneW * 0.5f + 70.f, 45.f), FVector(10.f, 10.f, 96.f));
	}
}

// =====================================================================================================================
// The garden: campfire with log seats, yard festoon poles, a floodlight
// =====================================================================================================================

void ADCLandscape::BuildGarden()
{
	using namespace DCSite;
	FRandomStream R(99);
	const FVector F(FireC.X, FireC.Y, GroundTop);
	// stone ring, ash, crossed logs, embers, flames
	for (int32 i = 0; i < 11; ++i)
	{
		const float A = 2.f * PI * i / 11.f;
		const float S = R.FRandRange(0.38f, 0.5f);
		Add(i % 2 ? FName("Rock0") : FName("Rock1"), "StoneDark", FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), F + FVector(FMath::Cos(A) * 72.f, FMath::Sin(A) * 72.f, 8.f), FVector(S, S * 0.8f, S * 0.55f)));
	}
	Add("DiscRound", "Ash", FTransform(FRotator::ZeroRotator, F + FVector(0, 0, 1.4f), FVector(1.25f, 1.25f, 1.f)), false);
	for (int32 i = 0; i < 5; ++i)
	{
		const float A = 2.f * PI * i / 5.f + 0.3f;
		const FVector Out(FMath::Cos(A) * 46.f, FMath::Sin(A) * 46.f, 6.f);
		Rod("LogWood", F + Out, F + FVector(0, 0, 52.f) - Out * 0.15f, 11.f, true, true);
	}
	for (int32 i = 0; i < 9; ++i)
	{
		const float S = R.FRandRange(0.08f, 0.14f);
		Add("Sphere", "Ember", FTransform(FRotator::ZeroRotator, F + FVector(R.FRandRange(-30.f, 30.f), R.FRandRange(-30.f, 30.f), 5.f), FVector(S, S, S * 0.5f)), false);
	}
	for (int32 i = 0; i < 4; ++i)
	{
		const float S = R.FRandRange(0.18f, 0.3f);
		Add("Cone", "Flame", FTransform(FRotator(R.FRandRange(-8.f, 8.f), 0.f, R.FRandRange(-8.f, 8.f)), F + FVector(R.FRandRange(-14.f, 14.f), R.FRandRange(-14.f, 14.f), 22.f + S * 50.f), FVector(S, S, S * 2.4f)), false);
	}
	// log seats + a stump
	for (int32 i = 0; i < 3; ++i)
	{
		const float A = FMath::DegreesToRadians(60.f + i * 105.f);
		const FVector P = F + FVector(FMath::Cos(A) * 250.f, FMath::Sin(A) * 250.f, 24.f);
		Add("Cyl", "LogWood", FTransform(FRotator(90.f, FMath::RadiansToDegrees(A) + 90.f, 0.f), P, FVector(0.48f, 0.48f, 1.7f)));
	}
	Add("Trunk", "LogWood", FTransform(FRotator::ZeroRotator, F + FVector(-60.f, -260.f, -2.f), FVector(0.55f, 0.55f, 0.48f)));
	// firewood pile
	for (int32 i = 0; i < 9; ++i)
	{
		const int32 Row = i < 4 ? 0 : (i < 7 ? 1 : 2);
		const int32 Col = i < 4 ? i : (i < 7 ? i - 4 : i - 7);
		Add("Cyl", "LogWood", FTransform(FRotator(90.f, 25.f, 0.f), F + FVector(-330.f, 160.f, 0.f) + FRotator(0.f, 25.f, 0.f).RotateVector(FVector(0.f, Col * 26.f + Row * 13.f, 13.f + Row * 22.f)), FVector(0.24f, 0.24f, 0.9f)));
	}

	// festoon poles around the yard (outside the yard rect, nothing tall inside it)
	const float Ys[2] = { YardRect().Min.Y - 80.f, YardRect().Max.Y - 30.f };
	const FBox2D Yd = YardRect();
	const float Xs[4] = { Yd.Min.X - 90.f, FMath::Lerp(Yd.Min.X, Yd.Max.X, 0.3f), FMath::Lerp(Yd.Min.X, Yd.Max.X, 0.68f), Yd.Max.X + 90.f };
	for (float Y : Ys) { for (float X : Xs) { Add("Post", "TimberV", FTransform(FRotator(R.FRandRange(-2.f, 2.f), 0.f, R.FRandRange(-2.f, 2.f)), FVector(X, Y, GroundTop - 15.f), FVector(0.13f, 0.13f, 3.55f))); } }

	// floodlight on a pole at the yard's back corner, aimed into the yard
	const FVector FP(FMath::Max(YardRect().Max.X, BuildingClearRect().Max.X) + 60.f, YardRect().Max.Y - 40.f, GroundTop);
	Add("Post", "Timber", FTransform(FRotator::ZeroRotator, FP, FVector(0.18f, 0.18f, 5.1f)));
	const FVector Lamp = FP + FVector(-35.f, -35.f, 490.f);
	Box("Metal", Lamp, FVector(34.f, 26.f, 22.f), FRotator(-35.f, -135.f, 0.f));
	Box("Flood", Lamp + FRotator(-35.f, -135.f, 0.f).RotateVector(FVector(12.f, 0, 0)), FVector(4.f, 22.f, 16.f), FRotator(-35.f, -135.f, 0.f), false);
	if (DayNight)
	{
		const FVector Aim(YardRect().GetCenter().X - 200.f, YardRect().GetCenter().Y, 0.f);
		DayNight->AddStreetLight(Lamp + FVector(-20.f, -20.f, -10.f), Aim, FLinearColor(1.f, 0.82f, 0.62f), 550.f, 2600.f, 58.f, false);
	}
}

// =====================================================================================================================
// Distant farm + windpump
// =====================================================================================================================

void ADCLandscape::BuildDistant()
{
	FRandomStream R(5150);
	// farmstead (right of the screen): white cottage, red barn, a lit window, trees around
	const FVector FH(-7400.f, 4300.f, GroundTop);
	const FRotator FR(0.f, 20.f, 0.f);
	auto At = [&](const FVector& L) { return FH + FR.RotateVector(L); };
	Box("PaintWhite", At(FVector(0, 0, 160.f)), FVector(900.f, 520.f, 320.f), FR);
	Box("Roof", At(FVector(0, -140.f, 400.f)), FVector(960.f, 330.f, 14.f), FRotator(0.f, FR.Yaw, -32.f));
	Box("Roof", At(FVector(0, 140.f, 400.f)), FVector(960.f, 330.f, 14.f), FRotator(0.f, FR.Yaw, 32.f));
	Box("PaintWhite", At(FVector(0, 0, 400.f)), FVector(890.f, 20.f, 150.f), FR); // gable infill
	Box("StoneDark", At(FVector(330.f, 0, 520.f)), FVector(60.f, 60.f, 160.f), FR); // chimney
	for (float X : { -250.f, 80.f, 300.f })
	{
		Box("FarmWindow", At(FVector(X, -262.f, 170.f)), FVector(70.f, 4.f, 90.f), FR, false);
	}
	const FVector Barn = FH + FVector(1300.f, -1100.f, 0.f);
	const FRotator BR(0.f, -15.f, 0.f);
	Box("BarnRed", Barn + FVector(0, 0, 260.f), FVector(1300.f, 800.f, 520.f), BR);
	Box("Roof", Barn + BR.RotateVector(FVector(0, -220.f, 640.f)), FVector(1360.f, 500.f, 16.f), FRotator(0.f, BR.Yaw, -28.f));
	Box("Roof", Barn + BR.RotateVector(FVector(0, 220.f, 640.f)), FVector(1360.f, 500.f, 16.f), FRotator(0.f, BR.Yaw, 28.f));
	Box("BarnRed", Barn + BR.RotateVector(FVector(0, 0, 640.f)), FVector(1290.f, 20.f, 220.f), BR);
	Box("PaintWhite", Barn + BR.RotateVector(FVector(-655.f, 0, 220.f)), FVector(8.f, 380.f, 420.f), BR);
	Box("Wire", Barn + BR.RotateVector(FVector(-658.f, 0, 190.f)), FVector(6.f, 330.f, 360.f), BR);
	for (int32 i = 0; i < 7; ++i)
	{
		const FVector2D P(FH.X + R.FRandRange(-1800.f, 2200.f), FH.Y + R.FRandRange(600.f, 2200.f));
		Tree(FVector(P.X, P.Y, GroundTop), R.FRandRange(900.f, 1300.f), i % 3 == 0 ? 2 : 0, 7100 + i);
	}
	NightPoint(At(FVector(80.f, -420.f, 170.f)), FLinearColor(1.f, 0.6f, 0.28f), 120.f, 900.f);

	// windpump (upper left): lattice tower with a turning rotor
	const FVector WP(6600.f, -1300.f, GroundTop);
	const float H = 1250.f;
	const FVector Legs[4] = { {-170, -170, 0}, {170, -170, 0}, {170, 170, 0}, {-170, 170, 0} };
	for (int32 i = 0; i < 4; ++i)
	{
		Rod("Metal", WP + Legs[i], WP + Legs[i] * 0.18f + FVector(0, 0, H), 9.f);
		for (int32 b = 0; b < 4; ++b)
		{
			const float z0 = H * b / 4.f, z1 = H * (b + 1) / 4.f;
			const float k0 = 1.f - 0.82f * b / 4.f, k1 = 1.f - 0.82f * (b + 1) / 4.f;
			Rod("Metal", WP + Legs[i] * k0 + FVector(0, 0, z0), WP + Legs[(i + 1) % 4] * k1 + FVector(0, 0, z1), 4.f);
		}
	}
	Box("Metal", WP + FVector(0, 0, H + 10.f), FVector(80.f, 80.f, 20.f));
	USceneComponent* Head = NewObject<USceneComponent>(this);
	Head->SetMobility(EComponentMobility::Movable);
	Head->SetupAttachment(GetRootComponent());
	Head->SetWorldLocationAndRotation(WP + FVector(-40.f, -40.f, H + 60.f), FRotator(0.f, -135.f, 0.f)); // faces the camera side
	Head->RegisterComponent();
	AddInstanceComponent(Head);
	WindRotor = Head;
	UInstancedStaticMeshComponent* Blades = NewObject<UInstancedStaticMeshComponent>(this);
	Blades->SetStaticMesh(MeshLib.FindRef("Cube"));
	Blades->SetMaterial(0, M("PaintWhite"));
	Blades->SetMobility(EComponentMobility::Movable);
	Blades->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Blades->bAffectDistanceFieldLighting = false;
	Blades->SetupAttachment(Head);
	Blades->RegisterComponent();
	AddInstanceComponent(Blades);
	for (int32 b = 0; b < 16; ++b)
	{
		const float A = 360.f * b / 16.f;
		Blades->AddInstance(FTransform(FRotator(0.f, 0.f, A), FRotator(0.f, 0.f, A).RotateVector(FVector(0, 0, 150.f)), FVector(0.02f, 0.38f, 2.2f)));
	}
	Blades->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.4f, 0.4f, 0.4f)));
	// tail vane (static, behind the head)
	Rod("Metal", WP + FVector(0, 0, H + 60.f), WP + FVector(240.f, 240.f, H + 60.f), 6.f);
	Box("PaintWhite", WP + FVector(260.f, 260.f, H + 60.f), FVector(10.f, 200.f, 120.f), FRotator(0.f, -45.f, 0.f));
	// stock tank at its foot
	Add("Cyl", "Metal", FTransform(FRotator::ZeroRotator, WP + FVector(-320.f, 150.f, 40.f), FVector(2.4f, 2.4f, 0.8f)));
}

// =====================================================================================================================
// Night: festoon lights over the yard, lanterns on the fence, campfire light, fireflies, pond glow
// =====================================================================================================================

void ADCLandscape::BuildNightLights()
{
	using namespace DCSite;
	FRandomStream R(8080);
	const FName Bulbs[4] = { "BulbAmber", "BulbRose", "BulbTeal", "BulbLime" };
	const FLinearColor BulbCol[4] = { FLinearColor(1.f, 0.55f, 0.2f), FLinearColor(1.f, 0.35f, 0.5f), FLinearColor(1.f, 0.7f, 0.4f), FLinearColor(0.3f, 0.8f, 0.85f) };

	// festoon strings: front row, back row and a zig-zag across the yard
	const float Ys[2] = { YardRect().Min.Y - 80.f, YardRect().Max.Y - 30.f };
	const FBox2D Yd = YardRect();
	const float Xs[4] = { Yd.Min.X - 90.f, FMath::Lerp(Yd.Min.X, Yd.Max.X, 0.3f), FMath::Lerp(Yd.Min.X, Yd.Max.X, 0.68f), Yd.Max.X + 90.f };
	const float PoleTop = 325.f;
	TArray<TPair<FVector, FVector>> Spans;
	for (int32 r = 0; r < 2; ++r) { for (int32 i = 0; i < 3; ++i) { Spans.Add({ FVector(Xs[i], Ys[r], PoleTop), FVector(Xs[i + 1], Ys[r], PoleTop) }); } }
	for (int32 i = 0; i < 3; ++i) { Spans.Add({ FVector(Xs[i], Ys[i % 2], PoleTop - 8.f), FVector(Xs[i + 1], Ys[(i + 1) % 2], PoleTop - 8.f) }); }
	int32 BulbIdx = 0;
	for (int32 s = 0; s < Spans.Num(); ++s)
	{
		TArray<FVector> Pts;
		const float Len = FVector::Dist(Spans[s].Key, Spans[s].Value);
		Sag(Spans[s].Key, Spans[s].Value, 55.f + Len * 0.02f, 14, "Wire", 1.6f, &Pts);
		const int32 NB = FMath::Max(4, FMath::RoundToInt(Len / 62.f));
		for (int32 b = 1; b < NB; ++b)
		{
			const float u = (float)b / NB * (Pts.Num() - 1);
			const int32 k = FMath::Min((int32)u, Pts.Num() - 2);
			const FVector P = FMath::Lerp(Pts[k], Pts[k + 1], u - k) - FVector(0, 0, 9.f);
			Add("Sphere", Bulbs[BulbIdx++ % 4], FTransform(FRotator::ZeroRotator, P, FVector(0.085f, 0.085f, 0.11f)), false);
		}
		const FVector Mid = Pts[Pts.Num() / 2] - FVector(0, 0, 30.f);
		NightPoint(Mid, BulbCol[s % 4], 55.f, 750.f);
	}

	// lanterns on the gate posts and two fence posts
	const float GateHalf = 260.f;
	const FVector LanternPos[4] = {
		FVector(DriveX - GateHalf, FenceY, 155.f), FVector(DriveX + GateHalf, FenceY, 155.f),
		FVector(YardRect().Min.X - 650.f, FenceY, 135.f), FVector(YardRect().Max.X + 650.f, FenceY, 135.f) };
	for (const FVector& L : LanternPos)
	{
		Box("Metal", L + FVector(0, 0, 18.f), FVector(20.f, 20.f, 5.f));
		Box("Lantern", L, FVector(13.f, 13.f, 26.f), FRotator::ZeroRotator, false);
		Box("Metal", L - FVector(0, 0, 15.f), FVector(18.f, 18.f, 4.f));
		NightPoint(L + FVector(0, 0, 5.f), FLinearColor(1.f, 0.6f, 0.25f), 70.f, 520.f, false, 0.12f);
	}

	// campfire: the one shadowed night light
	NightPoint(FVector(FireC.X, FireC.Y, 120.f), FLinearColor(1.f, 0.36f, 0.07f), 260.f, 1300.f, true, 0.35f);
	// pond: a soft teal glow just above the water
	NightPoint(FVector(PondC.X, PondC.Y, 70.f), FLinearColor(0.1f, 0.85f, 0.75f), 22.f, 900.f);

	// fireflies: drifting emissive motes over the meadow + pond, a handful carry tiny lights
	for (int32 i = 0; i < 170; ++i)
	{
		FVector2D P;
		if (i < 90) { const float A = R.FRandRange(0.f, 2.f * PI); P = PondC + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R.FRandRange(300.f, 1700.f); }
		else { P = FVector2D(R.FRandRange(BuildingClearRect().Min.X - 3000.f, BuildingClearRect().Min.X - 300.f), R.FRandRange(YardRect().Min.Y, BuildingClearRect().Max.Y + 2500.f)); }
		const float S = R.FRandRange(0.08f, 0.12f);
		Add("Sphere", "Firefly", FTransform(FRotator::ZeroRotator, FVector(P.X, P.Y, R.FRandRange(25.f, 170.f)), FVector(S)), false);
	}
	for (int32 i = 0; i < 6; ++i)
	{
		const float A = 2.f * PI * i / 6.f;
		const FVector Home(PondC.X + FMath::Cos(A) * 900.f, PondC.Y + FMath::Sin(A) * 800.f, 80.f);
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetMobility(EComponentMobility::Movable);
		L->SetupAttachment(GetRootComponent());
		L->SetWorldLocation(Home);
		L->IntensityUnits = ELightUnits::Candelas;
		L->Intensity = 0.f;
		L->SetLightColor(FLinearColor(0.7f, 1.f, 0.25f));
		L->AttenuationRadius = 200.f;
		L->SourceRadius = 2.f;
		L->SetVolumetricScatteringIntensity(0.f);
		L->SetCastShadows(false);
		L->bAffectTranslucentLighting = false;
		L->SetVisibility(false);
		L->RegisterComponent();
		AddInstanceComponent(L);
		Fireflies.Add({ L, Home, i * 1.913f });
	}
}
