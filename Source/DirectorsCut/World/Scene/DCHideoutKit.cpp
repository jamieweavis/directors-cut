// Director's Cut — reusable prop kit for the warehouse hideout (cars, monitors, chairs, racking, lamps...).
// All positions are CurRoom-local. With bKitOnStation the parts become CurStation props (hover highlight, clickable).
#include "World/Scene/DCHideoutBuilder.h"
#include "World/DCRoomModule.h"
#include "World/DCStation.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Core/DCWorldTags.h"
#include "Core/DCSiteLayout.h"

namespace
{
	float KJ(int32 I, float Amp) { return (FMath::Frac(FMath::Sin(I * 12.9898f + 4.1f) * 43758.5453f) * 2.f - 1.f) * Amp; }
	FRotator Compose(float Yaw, const FRotator& Local) { return (FRotator(0, Yaw, 0).Quaternion() * Local.Quaternion()).Rotator(); }
}

void ADCHideoutBuilder::K(EDCShape Shape, const FVector& Loc, const FVector& Size, FName Mat, const FRotator& Rot, bool bShadow)
{
	if (bKitOnStation && CurStation)
	{
		const FVector StationFloor = CurRoom->GetActorTransform().InverseTransformPosition(CurStation->GetActorLocation()) - FVector(0, 0, CurStationHalfZ);
		SP(Shape, Loc - StationFloor, Size, Mat, Rot, bShadow);
		return;
	}
	Deco(Shape, Loc, Size, Mat, Rot, bShadow);
}

void ADCHideoutBuilder::KTube(const FVector& A, const FVector& B, float Dia, FName Mat)
{
	const FVector D = B - A;
	const float Len = D.Size();
	if (Len < KINDA_SMALL_NUMBER) { return; }
	K(EDCShape::Cylinder, (A + B) * 0.5f, FVector(Dia, Dia, Len), Mat, FRotationMatrix::MakeFromZ(D / Len).Rotator());
}

void ADCHideoutBuilder::Monitor(const FVector& C, float W, float H, float Yaw, FName ScreenMat, float Pitch)
{
	const FRotator R(Pitch, Yaw, 0);
	K(EDCShape::Cube, C, FVector(3.5f, W + 3.f, H + 3.f), "PlasticBlack", R);
	K(EDCShape::Cube, C + R.RotateVector(FVector(-1.9f, 0, 0)), FVector(0.4f, W, H), ScreenMat, R, false);
}

void ADCHideoutBuilder::Car(const FVector& C, float Yaw, FName Paint, bool bVan)
{
	const FRotator YR(0, Yaw, 0);
	auto P = [&](EDCShape S, const FVector& L, const FVector& Sz, FName M, const FRotator& R = FRotator::ZeroRotator, bool bSh = true)
	{
		K(S, C + YR.RotateVector(L), Sz, M, Compose(Yaw, R), bSh);
	};
	auto Slab = [&](const FVector& A, const FVector& B, float Wd, float Th, FName M)
	{
		const FVector D = B - A; const float Len = D.Size();
		P(EDCShape::Cube, (A + B) * 0.5f, FVector(Len, Wd, Th), M, FRotationMatrix::MakeFromX(D / Len).Rotator());
	};
	const float WX = bVan ? 165.f : 145.f, WY = 80.f;
	if (!bVan)
	{
		// modern saloon, nose to +X
		P(EDCShape::Cube, FVector(0, 0, 55), FVector(462, 184, 52), Paint);                 // lower body
		P(EDCShape::Cube, FVector(0, 0, 30), FVector(440, 176, 14), "PlasticBlack");       // sills / underbody
		Slab(FVector(232, 0, 76), FVector(80, 0, 90), 178, 10, Paint);                         // bonnet
		Slab(FVector(-232, 0, 84), FVector(-150, 0, 92), 178, 10, Paint);                      // boot lid
		Slab(FVector(80, 0, 88), FVector(15, 0, 136), 158, 4, "TintGlass");                   // windscreen
		Slab(FVector(-95, 0, 136), FVector(-152, 0, 92), 158, 4, "TintGlass");                // rear screen
		P(EDCShape::Cube, FVector(-40, 0, 138), FVector(116, 150, 5), Paint);                 // roof
		P(EDCShape::Cube, FVector(-38, 0, 112), FVector(176, 156, 44), "TintGlass", FRotator::ZeroRotator, false); // side glass
		for (float S : { -1.f, 1.f })
		{
			P(EDCShape::Cube, FVector(20, S * 78.f, 113), FVector(6, 4, 46), Paint, FRotator(-35, 0, 0));       // A/B pillars
			P(EDCShape::Cube, FVector(-40, S * 78.f, 113), FVector(6, 3, 46), Paint);
			P(EDCShape::Cube, FVector(72, S * 96.f, 102), FVector(10, 12, 9), Paint);                            // mirrors
			P(EDCShape::Cube, FVector(232, S * 66.f, 72), FVector(4, 46, 4), "Head", FRotator::ZeroRotator, false); // LED headlights
			P(EDCShape::Cube, FVector(40, S * 92.5f, 58), FVector(1, 1, 46), "PlasticBlack");                    // door shut lines
			P(EDCShape::Cube, FVector(-62, S * 92.5f, 60), FVector(1, 1, 50), "PlasticBlack");
		}
		P(EDCShape::Cube, FVector(-232, 0, 80), FVector(3, 168, 5), "Tail", FRotator::ZeroRotator, false);       // light bar
		P(EDCShape::Cube, FVector(233, 0, 52), FVector(3, 96, 22), "PlasticBlack");                               // grille
		P(EDCShape::Cube, FVector(234, 0, 32), FVector(6, 180, 12), "PlasticBlack");
		P(EDCShape::Cube, FVector(-234, 0, 36), FVector(6, 180, 14), "PlasticBlack");
	}
	else
	{
		// panel van, cab to +X
		P(EDCShape::Cube, FVector(-50, 0, 140), FVector(400, 200, 200), Paint);              // load box
		P(EDCShape::Cube, FVector(195, 0, 72), FVector(110, 196, 70), Paint);                // nose
		Slab(FVector(250, 0, 108), FVector(160, 0, 210), 186, 6, "TintGlass");                 // windscreen
		P(EDCShape::Cube, FVector(175, 0, 160), FVector(70, 198, 70), "TintGlass", FRotator::ZeroRotator, false);
		P(EDCShape::Cube, FVector(145, 0, 238), FVector(20, 200, 6), Paint);
		P(EDCShape::Cube, FVector(0, 0, 32), FVector(500, 190, 14), "PlasticBlack");
		for (float S : { -1.f, 1.f })
		{
			P(EDCShape::Cube, FVector(252, S * 70.f, 86), FVector(4, 40, 10), "Head", FRotator::ZeroRotator, false);
			P(EDCShape::Cube, FVector(200, S * 108.f, 150), FVector(12, 14, 22), "PlasticBlack");
			P(EDCShape::Cube, FVector(-251, S * 80.f, 100), FVector(3, 16, 40), "Tail", FRotator::ZeroRotator, false);
			P(EDCShape::Cube, FVector(20, S * 100.5f, 130), FVector(2, 1, 180), "PlasticBlack");               // sliding door seam
		}
		P(EDCShape::Cube, FVector(252, 0, 60), FVector(3, 120, 30), "PlasticBlack");
		P(EDCShape::Cube, FVector(-251, 0, 140), FVector(1, 2, 190), "PlasticBlack");                          // barn door seam
	}
	for (int32 i = 0; i < 4; ++i)
	{
		const float X = (i % 2) ? WX : -WX, Y = (i / 2) ? WY : -WY;
		P(EDCShape::Cylinder, FVector(X, Y, 34), FVector(68, 68, 24), "Tire", FRotator(0, 0, 90));
		P(EDCShape::Cylinder, FVector(X, Y * 1.16f, 34), FVector(42, 42, 2), "Alu", FRotator(0, 0, 90));
		P(EDCShape::Cylinder, FVector(X, Y * 1.17f, 34), FVector(12, 12, 2), "SteelBlack", FRotator(0, 0, 90));
	}
}

void ADCHideoutBuilder::OfficeChair(const FVector& C, float Yaw)
{
	const FRotator YR(0, Yaw, 0);
	auto P = [&](EDCShape S, const FVector& L, const FVector& Sz, FName M, const FRotator& R = FRotator::ZeroRotator)
	{
		K(S, C + YR.RotateVector(L), Sz, M, Compose(Yaw, R));
	};
	for (int32 i = 0; i < 5; ++i)
	{
		const float A = i * 72.f;
		P(EDCShape::Cube, FRotator(0, A, 0).RotateVector(FVector(16, 0, 6)), FVector(32, 4, 3), "PlasticBlack", FRotator(0, A, 0));
	}
	P(EDCShape::Cylinder, FVector(0, 0, 26), FVector(5, 5, 38), "Chrome");
	P(EDCShape::Cube, FVector(0, 0, 48), FVector(50, 50, 9), "Fabric");
	P(EDCShape::Cube, FVector(-26, 0, 82), FVector(8, 46, 56), "Fabric", FRotator(-8, 0, 0));
	P(EDCShape::Cube, FVector(2, -26, 62), FVector(30, 4, 4), "PlasticBlack");
	P(EDCShape::Cube, FVector(2, 26, 62), FVector(30, 4, 4), "PlasticBlack");
}

void ADCHideoutBuilder::Crate(const FVector& C, const FVector& S, float Yaw, FName Mat)
{
	K(EDCShape::Cube, C + FVector(0, 0, S.Z * 0.5f), S, Mat, FRotator(0, Yaw, 0));
	if (Mat == FName("Cardboard")) { K(EDCShape::Cube, C + FVector(0, 0, S.Z * 0.5f), FVector(S.X + 0.4f, 5.f, S.Z + 0.4f), "Paper", FRotator(0, Yaw, 0)); } // tape band
}

void ADCHideoutBuilder::ShelfUnit(const FVector& C, float Yaw, float Len, int32 Seed)
{
	const FRotator YR(0, Yaw, 0);
	auto At = [&](float X, float Y, float Z) { return C + YR.RotateVector(FVector(X, Y, Z)); };
	for (int32 i = 0; i < 4; ++i) { K(EDCShape::Cube, At((i % 2) ? 22.f : -22.f, (i / 2) ? Len * 0.5f : -Len * 0.5f, 100), FVector(4, 4, 200), "Galv", YR); }
	for (float Z : { 12.f, 70.f, 128.f, 186.f })
	{
		K(EDCShape::Cube, At(0, 0, Z), FVector(48, Len, 3), "Galv", YR);
		int32 k = Seed + (int32)Z;
		float Y = -Len * 0.5f + 8.f;
		while (Y < Len * 0.5f - 30.f)
		{
			const float W = 28.f + FMath::Abs(KJ(k, 14.f)), H = 18.f + FMath::Abs(KJ(k + 3, 16.f));
			if (KJ(k + 7, 1.f) > -0.6f)
			{
				static const FName Boxes[] = { "Cardboard", "Cardboard", "PlasticBlack", "Cardboard", "PaintGrey" };
				K(EDCShape::Cube, At(KJ(k + 1, 3.f), Y + W * 0.5f, Z + 1.5f + H * 0.5f), FVector(40, W, H), Boxes[k % 5], Compose(Yaw, FRotator(0, KJ(k + 2, 4.f), 0)));
			}
			Y += W + 4.f; ++k;
		}
	}
}

void ADCHideoutBuilder::PalletRack(const FVector& C, float Yaw, int32 Bays, int32 Seed)
{
	const FRotator YR(0, Yaw, 0);
	auto At = [&](float X, float Y, float Z) { return C + YR.RotateVector(FVector(X, Y, Z)); };
	const float BayW = 140.f, D = 90.f, H = 330.f;
	const float Y0 = -Bays * BayW * 0.5f;
	for (int32 b = 0; b <= Bays; ++b)
	{
		const float Y = Y0 + b * BayW;
		K(EDCShape::Cube, At(-D * 0.5f, Y, H * 0.5f), FVector(7, 7, H), "PaintBlue", YR);
		K(EDCShape::Cube, At(D * 0.5f, Y, H * 0.5f), FVector(7, 7, H), "PaintBlue", YR);
		for (float Z : { 60.f, 160.f, 260.f }) { K(EDCShape::Cube, At(0, Y, Z), FVector(D, 3, 3), "PaintBlue", YR); }
	}
	for (float Z : { 0.f, 125.f, 245.f })
	{
		if (Z > 0.f)
		{
			K(EDCShape::Cube, At(-D * 0.5f, 0, Z - 6.f), FVector(6, Bays * BayW, 12), "PaintOrange", YR);
			K(EDCShape::Cube, At(D * 0.5f, 0, Z - 6.f), FVector(6, Bays * BayW, 12), "PaintOrange", YR);
		}
		for (int32 b = 0; b < Bays; ++b)
		{
			const int32 k = Seed + b * 7 + (int32)Z;
			if (KJ(k, 1.f) < -0.55f) { continue; }
			const float Y = Y0 + (b + 0.5f) * BayW;
			K(EDCShape::Cube, At(0, Y, Z + 7.f), FVector(D - 6.f, BayW - 24.f, 13.f), "Wood", YR);
			const float BH = 50.f + FMath::Abs(KJ(k + 1, 45.f));
			const bool bWrap = KJ(k + 2, 1.f) > 0.f;
			K(EDCShape::Cube, At(0, Y, Z + 14.f + BH * 0.5f), FVector(D - 12.f, BayW - 30.f, BH), bWrap ? FName("PlasticBlack") : FName("Cardboard"), YR);
			if (bWrap) { K(EDCShape::Cube, At(0, Y, Z + 14.f + BH * 0.5f), FVector(D - 9.f, BayW - 27.f, BH + 2.f), "Plastic", YR, false); }
		}
	}
}

void ADCHideoutBuilder::Pendant(const FVector& C, bool bWarm, bool bLight, float Cd)
{
	Deco(EDCShape::Cylinder, FVector(C.X, C.Y, (C.Z + 380.f) * 0.5f + 10.f), FVector(1.f, 1.f, 380.f - C.Z - 10.f), "Black", FRotator::ZeroRotator, false);
	Deco(EDCShape::Cone, C + FVector(0, 0, 6), FVector(46, 46, 22), "SteelBlack", FRotator(0, 0, 0));
	Deco(EDCShape::Cylinder, C + FVector(0, 0, -4), FVector(34, 34, 1), bWarm ? FName("LedWarm") : FName("LedCool"), FRotator::ZeroRotator, false);
	if (bLight)
	{
		const FLinearColor Col = bWarm ? FLinearColor(1.f, 0.72f, 0.48f) : FLinearColor(0.85f, 0.92f, 1.f);
		Spot(C - FVector(0, 0, 8), C - FVector(0, 0, 200), Col, Cd, 900.f, 35.f, 70.f, false, true);
	}
}

void ADCHideoutBuilder::LedBatten(const FVector& C, float Len, float Yaw, bool bLight, float Cd)
{
	const FRotator YR(0, Yaw, 0);
	for (float S : { -0.4f, 0.4f })
	{
		const FVector P = C + YR.RotateVector(FVector(Len * S, 0, 0));
		Deco(EDCShape::Cylinder, FVector(P.X, P.Y, (C.Z + 380.f) * 0.5f), FVector(0.6f, 0.6f, 380.f - C.Z), "Black", FRotator::ZeroRotator, false);
	}
	Deco(EDCShape::Cube, C, FVector(Len, 12, 5), "Alu", YR);
	Deco(EDCShape::Cube, C - FVector(0, 0, 3), FVector(Len - 4, 8, 1.2f), "LedCool", YR, false);
	if (bLight)
	{
		URectLightComponent* L = Rect(C - FVector(0, 0, 6), C - FVector(0, 0, 300), FLinearColor(0.88f, 0.94f, 1.f), Cd, Len, 10.f, 1000.f, false, true);
		L->SetRelativeRotation(FRotator(-90, Yaw, 0));
	}
}

void ADCHideoutBuilder::Duffel(const FVector& C, float Yaw, FName Mat)
{
	K(EDCShape::Cylinder, C + FVector(0, 0, 15), FVector(32, 30, 68), Mat, FRotator(0, Yaw, 90));
	K(EDCShape::Cube, C + FVector(0, 0, 30), FVector(26, 6, 3), "PlasticBlack", FRotator(0, Yaw + 90.f, 0));
}

// =====================================================================================================================
// Ruin kit: walls, stubs, rubble, windows, growth (room-local, CurRoom / CurGroup)
// =====================================================================================================================

namespace
{
	constexpr float RWallT = 24.f;
	constexpr float RHouseH = 340.f;
	constexpr float RWingH = 300.f;
	float KA(int32 I, float Amp) { return FMath::Abs(KJ(I, Amp)); }
}

void ADCHideoutBuilder::RuinStub(bool bAlongX, float Fixed, float A, float B, int32 Seed, FName Mat, float EndA, float EndB, const TArray<FOpening>& InGaps, bool bSolid)
{
	auto P = [&](float Al, float Ac, float Z) { return bAlongX ? FVector(Al, Fixed + Ac, Z) : FVector(Fixed + Ac, Al, Z); };
	auto S = [&](float Al, float Ac, float H) { return bAlongX ? FVector(Al, Ac, H) : FVector(Ac, Al, H); };
	auto InPlane = [&](float Deg) { return bAlongX ? FRotator(Deg, 0, 0) : FRotator(0, 0, Deg); };
	TArray<FOpening> Gaps = InGaps;
	Gaps.Sort([](const FOpening& L, const FOpening& R) { return L.Center < R.Center; });
	TArray<TPair<float, float>> Runs;
	float Cur = A;
	for (const FOpening& G : Gaps)
	{
		const float G0 = FMath::Clamp(G.Center - G.Width * 0.5f, A, B), G1 = FMath::Clamp(G.Center + G.Width * 0.5f, A, B);
		if (G0 > Cur + 1.f) { Runs.Add({ Cur, G0 }); }
		Cur = FMath::Max(Cur, G1);
	}
	if (B > Cur + 1.f) { Runs.Add({ Cur, B }); }
	int32 k = Seed * 31 + 5;
	for (const auto& Run : Runs)
	{
		float X = Run.Key;
		while (X < Run.Value - 2.f)
		{
			float Len = 32.f + KA(k, 70.f);
			if (Run.Value - (X + Len) < 25.f) { Len = Run.Value - X; }
			const float Mid = X + Len * 0.5f;
			float H = bSolid ? 52.f + KA(k + 1, 20.f) : 10.f + KA(k + 1, 1.f) * KA(k + 5, 1.f) * 85.f;
			H = FMath::Max(H, EndA * FMath::Clamp(1.f - (Mid - A) / 170.f, 0.f, 1.f) + KJ(k + 2, 12.f));
			H = FMath::Max(H, EndB * FMath::Clamp(1.f - (B - Mid) / 170.f, 0.f, 1.f) + KJ(k + 3, 12.f));
			if (!bSolid && KJ(k + 4, 1.f) > 0.72f && H < 70.f) { H = 4.f + KA(k + 6, 5.f); } // broken down to the footing
			const float T = RWallT + KJ(k + 14, 3.f);
			const FVector L = P(Mid, KJ(k + 15, 1.5f), H * 0.5f);
			const FVector Sz = S(Len - 0.6f, T, H);
			if (bSolid) { Solid(L, Sz, Mat, false); }
			else { Deco(EDCShape::Cube, L, Sz, Mat); }
			// ragged top: loose blocks, a little moss
			if (KJ(k + 7, 1.f) > -0.25f)
			{
				Deco(EDCShape::Cube, P(Mid + KJ(k + 8, Len * 0.3f), KJ(k + 9, 3.f), H + 5.f), S(14.f + KA(k + 10, 10.f), T - 6.f, 11.f), Mat, InPlane(KJ(k + 11, 14.f)));
			}
			if (KJ(k + 13, 1.f) > 0.35f && H > 15.f) { Deco(EDCShape::Cube, P(Mid, 0, H + 0.6f), S(Len * 0.75f, T * 0.85f, 1.4f), "Moss", FRotator::ZeroRotator, false); }
			X += Len;
			k += 17;
		}
	}
}

void ADCHideoutBuilder::RubblePile(const FVector& C, float Radius, int32 Count, int32 Seed, FName Mat)
{
	for (int32 i = 0; i < Count; ++i)
	{
		const int32 k = Seed * 13 + i * 7;
		const float A = KJ(k, PI), R = Radius * FMath::Sqrt(KA(k + 1, 1.f));
		const float Sz = 9.f + KA(k + 2, 18.f) * (1.f - R / (Radius + 1.f) * 0.6f);
		const FVector P = C + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Sz * 0.3f);
		if (i % 3 == 2) { Deco(EDCShape::Sphere, P, FVector(Sz * 1.6f, Sz * 1.4f, Sz * 0.7f), "Rubble", FRotator(0, KJ(k + 3, 90.f), 0)); }
		else { Deco(EDCShape::Cube, P, FVector(Sz * 1.3f, Sz, Sz * 0.7f), (i % 4 == 1) ? FName("Rubble") : Mat, FRotator(KJ(k + 4, 25.f), KJ(k + 5, 90.f), KJ(k + 6, 25.f))); }
	}
}

void ADCHideoutBuilder::RubbleRun(bool bAlongX, float Fixed, float A, float B, float Outward, int32 Seed, FName Mat, float Density)
{
	auto P = [&](float Al, float Ac, float Z) { return bAlongX ? FVector(Al, Fixed + Ac, Z) : FVector(Fixed + Ac, Al, Z); };
	const int32 N = FMath::Max(2, FMath::RoundToInt((B - A) / 40.f * Density));
	for (int32 i = 0; i < N; ++i)
	{
		const int32 k = Seed * 7 + i * 11;
		const float T = FMath::Frac(i * 0.618034f + Seed * 0.13f);
		const float Al = FMath::Lerp(A + 15.f, B - 15.f, T);
		const bool bIn = KJ(k, 1.f) > 0.75f;
		const float Ac = (bIn ? -Outward : Outward) * (RWallT * 0.5f + 4.f + KA(k + 1, 1.f) * KA(k + 2, 1.f) * (bIn ? 50.f : 110.f));
		const float Sz = 8.f + KA(k + 3, 16.f);
		Deco(EDCShape::Cube, P(Al, Ac, Sz * 0.3f), FVector(Sz * 1.4f, Sz, Sz * 0.65f), (i % 3 == 0) ? FName("Rubble") : Mat, FRotator(KJ(k + 4, 20.f), KJ(k + 5, 90.f), KJ(k + 6, 20.f)));
	}
	// heaps where the wall came down, a fallen plank or two
	const int32 NP = FMath::Max(1, FMath::RoundToInt((B - A) / 260.f * Density));
	for (int32 i = 0; i < NP; ++i)
	{
		const int32 k = Seed * 3 + i * 23;
		const float Al = FMath::Lerp(A + 60.f, B - 60.f, FMath::Frac(i * 0.381966f + Seed * 0.21f));
		RubblePile(P(Al, Outward * (RWallT * 0.5f + 30.f + KA(k, 25.f)), 0), 45.f + KA(k + 1, 25.f), 7, k, Mat);
		if (KJ(k + 2, 1.f) > -0.2f)
		{
			const float Yaw = (bAlongX ? 0.f : 90.f) + KJ(k + 3, 35.f);
			Deco(EDCShape::Cube, P(Al + KJ(k + 4, 60.f), Outward * (RWallT * 0.5f + 40.f + KA(k + 5, 40.f)), 5.f), FVector(150.f + KA(k + 6, 80.f), 14.f, 4.f), "TimberGrey", FRotator(KJ(k + 7, 8.f), Yaw, 0));
		}
		if (KJ(k + 8, 1.f) > 0.2f) { Weeds(P(Al + KJ(k + 9, 80.f), Outward * (RWallT * 0.5f + 25.f), 0), 35.f, 4, k, 55.f); }
	}
}

void ADCHideoutBuilder::Weeds(const FVector& C, float Radius, int32 Count, int32 Seed, float MaxH)
{
	for (int32 i = 0; i < Count; ++i)
	{
		const int32 k = Seed * 17 + i * 5;
		const float A = KJ(k, PI), R = Radius * FMath::Sqrt(KA(k + 1, 1.f));
		const FVector P = C + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0);
		const float H = MaxH * (0.35f + KA(k + 2, 0.65f));
		const FName Mat = KJ(k + 3, 1.f) > 0.35f ? FName("WeedDry") : FName("Weed");
		if (i % 4 == 3) { Deco(EDCShape::Sphere, P + FVector(0, 0, H * 0.12f), FVector(H * 0.7f, H * 0.6f, H * 0.3f), "Weed", FRotator(0, KJ(k + 4, 90.f), 0)); continue; }
		// a tuft: a few thin blades leaning outwards
		for (int32 b = 0; b < 5; ++b)
		{
			const float BA = b * 1.2566f + KJ(k + b, 0.5f);
			const float BH = H * (0.6f + KA(k + b + 11, 0.4f));
			const float Lean = 10.f + KA(k + b + 3, 16.f);
			const FVector Dir(FMath::Cos(BA), FMath::Sin(BA), 0);
			Deco(EDCShape::Cone, P + Dir * (2.f + BH * 0.08f) + FVector(0, 0, BH * 0.48f), FVector(3.5f, 2.f, BH), b == 2 ? FName("Weed") : Mat,
				FRotationMatrix::MakeFromZ(FVector(0, 0, 1) + Dir * FMath::Tan(FMath::DegreesToRadians(Lean))).Rotator(), false);
		}
	}
}

void ADCHideoutBuilder::Ivy(const FVector& Base, const FVector& Normal, float W, float H, int32 Seed)
{
	const bool bX = FMath::Abs(Normal.X) > 0.5f;
	const FVector Tan = bX ? FVector(0, 1, 0) : FVector(1, 0, 0);
	const int32 N = FMath::Clamp(FMath::RoundToInt(W * H / 700.f), 6, 70);
	for (int32 i = 0; i < N; ++i)
	{
		const int32 k = Seed * 19 + i * 3;
		const float Z = H * FMath::Pow(KA(k, 1.f), 0.8f);
		const float Spread = W * 0.5f * (1.f - 0.55f * Z / H);
		const float S = 14.f + KA(k + 1, 20.f) * (1.f - 0.4f * Z / H);
		const FVector P = Base + Tan * KJ(k + 2, Spread) + FVector(0, 0, Z) + Normal * (2.f + KA(k + 3, 3.f));
		Deco(EDCShape::Sphere, P, bX ? FVector(5.f, S, S * 0.8f) : FVector(S, 5.f, S * 0.8f), "Ivy", FRotator::ZeroRotator, false);
	}
}

void ADCHideoutBuilder::OldWindow(bool bAlongX, float Fixed, float Inward, const FOpening& O, int32 Seed)
{
	auto P = [&](float Al, float Ac, float Z) { return bAlongX ? FVector(Al, Fixed + Ac, Z) : FVector(Fixed + Ac, Al, Z); };
	auto S = [&](float Al, float Ac, float H) { return bAlongX ? FVector(Al, Ac, H) : FVector(Ac, Al, H); };
	auto InPlane = [&](float Deg) { return bAlongX ? FRotator(Deg, 0, 0) : FRotator(0, 0, Deg); };
	const float W = O.Width, H = O.Top - O.Bottom, Z0 = O.Bottom, Mid = O.Center;
	// frame, lintel, sill
	for (float Sg : { -1.f, 1.f }) { Deco(EDCShape::Cube, P(Mid + Sg * (W * 0.5f - 4.f), 0, Z0 + H * 0.5f), S(8, RWallT * 0.55f, H), "TimberDark"); }
	Deco(EDCShape::Cube, P(Mid, 0, O.Top + 8.f), S(W + 36.f, RWallT + 4.f, 16.f), "TimberDark");
	Deco(EDCShape::Cube, P(Mid, Inward * 5.f, Z0 - 4.f), S(W + 22.f, RWallT + 14.f, 8.f), "StoneDark");
	// cross bars + panes (some intact, some cracked, some gone)
	Deco(EDCShape::Cube, P(Mid, 0, Z0 + H * 0.5f), S(5.f, 6.f, H), "Timber");
	Deco(EDCShape::Cube, P(Mid, 0, Z0 + H * 0.55f), S(W - 8.f, 6.f, 5.f), "Timber");
	for (int32 c = 0; c < 4; ++c)
	{
		const float CW = (W - 13.f) * 0.5f, CH = (c / 2) ? H * 0.45f - 6.f : H * 0.55f - 6.f;
		const float CX = Mid + ((c % 2) ? 1.f : -1.f) * (CW * 0.5f + 2.5f);
		const float CZ = (c / 2) ? Z0 + H * 0.55f + 2.5f + CH * 0.5f : Z0 + 3.f + CH * 0.5f;
		const float R = KJ(Seed * 5 + c, 1.f);
		if (R > -0.05f) { Deco(EDCShape::Cube, P(CX, 0, CZ), S(CW, 1.f, CH), "Window", FRotator::ZeroRotator, false); }
		else if (R > -0.55f) { Deco(EDCShape::Cube, P(CX - CW * 0.2f, 0, CZ - CH * 0.25f), S(CW * 0.6f, 1.f, CH * 0.5f), "Window", InPlane(25.f + R * 30.f), false); }
	}
	if (Seed % 4 == 0)
	{
		// boarded up from the inside
		Deco(EDCShape::Cube, P(Mid, Inward * (RWallT * 0.5f + 3.f), Z0 + H * 0.35f), S(W + 24.f, 2.5f, 15.f), "TimberGrey", InPlane(7.f));
		Deco(EDCShape::Cube, P(Mid, Inward * (RWallT * 0.5f + 5.f), Z0 + H * 0.7f), S(W + 20.f, 2.5f, 14.f), "TimberGrey", InPlane(-5.f));
	}
	else if (Seed % 3 == 1)
	{
		// an old green shutter folded back against the inside wall, sagging on one hinge
		Deco(EDCShape::Cube, P(Mid - W * 0.5f - W * 0.25f - 6.f, Inward * (RWallT * 0.5f + 2.5f), Z0 + H * 0.48f), S(W * 0.5f, 3.f, H - 6.f), "PaintGreen", InPlane(-4.f));
	}
}

void ADCHideoutBuilder::TallWall(bool bAlongX, float Fixed, float Inward, float A, float B, float H, const TArray<FOpening>& Doors, const TArray<FOpening>& Windows, FName Outer, FName Skin, int32 Seed)
{
	auto P = [&](float Al, float Ac, float Z) { return bAlongX ? FVector(Al, Fixed + Ac, Z) : FVector(Fixed + Ac, Al, Z); };
	auto S = [&](float Al, float Ac, float Hh) { return bAlongX ? FVector(Al, Ac, Hh) : FVector(Ac, Al, Hh); };
	const FVector N = bAlongX ? FVector(0, Inward, 0) : FVector(Inward, 0, 0);
	TArray<FOpening> All = Doors; All.Append(Windows);
	auto Clear = [&](float C, float HalfW) { for (const FOpening& O : All) { if (FMath::Abs(C - O.Center) < O.Width * 0.5f + HalfW + 12.f) { return false; } } return true; };
	WallRun(bAlongX, Fixed, A, B, 0.f, H, RWallT, All, Outer, true);
	if (Skin != Outer)
	{
		const float SkinF = Fixed + Inward * (RWallT * 0.5f + 1.f);
		WallRun(bAlongX, SkinF, A, B, 0.f, H - 16.f, 2.f, All, Skin, false);
		// fallen plaster: exposed masonry, some with the lath still on
		const int32 NP = FMath::Max(1, FMath::RoundToInt((B - A) / 200.f));
		for (int32 i = 0; i < NP; ++i)
		{
			const int32 k = Seed * 11 + i * 7;
			const float W = 50.f + KA(k, 90.f), Hh = 40.f + KA(k + 1, 70.f);
			const float C = FMath::Lerp(A + 40.f + W * 0.5f, B - 40.f - W * 0.5f, FMath::Frac(i * 0.618034f + Seed * 0.37f));
			const float Z = FMath::Clamp(70.f + KA(k + 2, H - 150.f), Hh * 0.5f + 5.f, H - 30.f - Hh * 0.5f);
			if (!Clear(C, W * 0.5f)) { continue; }
			Deco(EDCShape::Cube, P(C, Inward * (RWallT * 0.5f + 2.3f), Z), S(W, 0.8f, Hh), Outer);
			if (KJ(k + 3, 1.f) > 0.f)
			{
				for (float ZZ = Z - Hh * 0.5f + 4.f; ZZ < Z + Hh * 0.5f - 3.f; ZZ += 6.f) { Deco(EDCShape::Cube, P(C + KJ(k + (int32)ZZ, 3.f), Inward * (RWallT * 0.5f + 3.3f), ZZ), S(W - 6.f, 0.8f, 2.6f), "Lath"); }
			}
		}
	}
	// wall plate on top, timber lintels over doors
	Deco(EDCShape::Cube, P((A + B) * 0.5f, Inward * 2.f, H + 6.f), S(B - A + 8.f, RWallT + 8.f, 12.f), "TimberDark");
	for (const FOpening& D : Doors) { Deco(EDCShape::Cube, P(D.Center, 0, D.Top + 8.f), S(D.Width + 30.f, RWallT + 4.f, 16.f), "TimberDark"); }
	for (int32 i = 0; i < Windows.Num(); ++i) { OldWindow(bAlongX, Fixed, Inward, Windows[i], Seed + i * 7); }
	// ivy climbing the inside face and spilling over the top, weeds at the foot
	const int32 NI = FMath::Max(1, FMath::RoundToInt((B - A) / 420.f));
	for (int32 i = 0; i < NI; ++i)
	{
		const int32 k = Seed * 5 + i * 13;
		const float C = FMath::Lerp(A + 70.f, B - 70.f, FMath::Frac(i * 0.618034f + Seed * 0.29f));
		if (Clear(C, 50.f)) { Ivy(P(C, Inward * (RWallT * 0.5f + 1.f), 0), N, 90.f + KA(k, 60.f), H * (0.5f + KA(k + 1, 0.45f)), k); }
		Ivy(P(C + KJ(k + 2, 120.f), Inward * (RWallT * 0.5f + 1.f), H - 70.f), N, 120.f, 80.f, k + 3);
		Weeds(P(C + KJ(k + 4, 60.f), Inward * 32.f, 0), 40.f, 5, k, 50.f);
	}
}

// =====================================================================================================================
// Roofs
// =====================================================================================================================

void ADCHideoutBuilder::BuildHouseRoof()
{
	// Office local == world. Ridge along X at Y=0; only the back (+Y) slope survives, mostly as rafters with a few
	// courses of tiles near the eave (the slope is steep enough that the iso camera looks under it).
	CurRoom = GetRoom(EDCRoomType::Office);
	CurGroup = DCGroups::Shell;
	const float Ze = RHouseH + 12.f, Tan = 0.7f;   // ~35 degree pitch
	const float Zr = Ze + 400.f * Tan;
	const float GX = 500.f - RWallT * 0.5f;
	// +X gable (stone courses, crumbling on the front edge)
	for (int32 i = 0; Ze + i * 28.f < Zr - 10.f; ++i)
	{
		const float Z = Ze + i * 28.f + 14.f;
		const float HW = 412.f * (1.f - (Z - Ze) / (Zr - Ze));
		const float Bite = (i > 2) ? KA(i * 3 + 1, 50.f) : 0.f;
		Deco(EDCShape::Cube, FVector(GX, Bite * 0.5f, Z), FVector(RWallT, 2.f * HW - Bite, 28.f), "Stone");
	}
	Ivy(FVector(GX - RWallT * 0.5f - 1.f, 250.f, Ze - 40.f), FVector(-1, 0, 0), 160.f, 200.f, 77);
	// chimney stack in the back-left corner (with a dead hearth)
	Deco(EDCShape::Cube, FVector(-440.f, 350.f, 380.f), FVector(110.f, 76.f, 760.f), "StoneDark");
	Deco(EDCShape::Cube, FVector(-440.f, 350.f, 765.f), FVector(122.f, 88.f, 14.f), "Stone");
	Deco(EDCShape::Cylinder, FVector(-455.f, 350.f, 790.f), FVector(22.f, 22.f, 40.f), "BrickOld");
	Deco(EDCShape::Cube, FVector(-384.f, 350.f, 55.f), FVector(4.f, 60.f, 90.f), "Black", FRotator::ZeroRotator, false);
	Deco(EDCShape::Cube, FVector(-380.f, 350.f, 104.f), FVector(16.f, 110.f, 10.f), "TimberDark");
	RubblePile(FVector(-370.f, 350.f, 0.f), 30.f, 5, 41, "BrickOld");
	// ridge beam: rests on the gable, snapped where the front gable fell, the broken end down in the rubble
	Bar(FVector(GX + 6.f, 0, Zr - 8.f), FVector(-160.f, 0, Zr - 22.f), 18.f, 22.f, "TimberDark");
	Bar(FVector(-160.f, 0, Zr - 22.f), FVector(-215.f, -12.f, Zr - 70.f), 16.f, 20.f, "TimberDark");
	// back slope rafters (full where the ridge holds, dangling stubs beyond)
	auto Slope = [&](float Y) { return Zr - Y * Tan; };
	for (float X = 470.f; X > -470.f; X -= 62.f)
	{
		const int32 k = (int32)(X + 600.f);
		if (X > -150.f) { Bar(FVector(X, -6.f, Slope(-6.f)), FVector(X, 440.f, Slope(440.f)), 7.f, 14.f, "TimberDark"); }
		else if (KJ(k, 1.f) > -0.4f)
		{
			const float Y0 = 120.f + KA(k + 1, 160.f);
			Bar(FVector(X, Y0, Slope(Y0) - 30.f - KA(k + 2, 40.f)), FVector(X, 440.f, Slope(440.f)), 7.f, 14.f, "TimberDark");
		}
	}
	// battens + surviving tiles (more towards the eave)
	for (float T = 0.12f; T < 0.98f; T += 0.085f)
	{
		const float Y = 440.f * T;
		Bar(FVector(-150.f, Y, Slope(Y) + 9.f), FVector(GX, Y, Slope(Y) + 9.f), 4.f, 3.f, "Timber");
	}
	for (int32 c = 0; c < 6; ++c)
	{
		for (int32 r = 0; r < 7; ++r)
		{
			const float X0 = -140.f + c * 104.f;
			const float T0 = 0.08f + r * 0.13f, T1 = T0 + 0.13f;
			const float Keep = T0 > 0.6f ? 0.85f : T0 > 0.35f ? 0.4f : 0.1f;
			if ((KJ(c * 31 + r * 7 + 3, 1.f) * 0.5f + 0.5f) > Keep) { continue; }
			const float Y0 = 440.f * T0, Y1 = 440.f * T1;
			Bar(FVector(X0 + 52.f, Y0, Slope(Y0) + 13.f), FVector(X0 + 52.f, Y1, Slope(Y1) + 13.f), 102.f, 4.f, "RoofTile");
			if (KJ(c * 13 + r, 1.f) > 0.5f) { Deco(EDCShape::Cube, FVector(X0 + 52.f + KJ(c + r, 30.f), (Y0 + Y1) * 0.5f, Slope((Y0 + Y1) * 0.5f) + 16.f), FVector(50.f, 30.f, 1.5f), "Moss", FRotator(0, 0, -35.f), false); }
		}
	}
	// a few front rafters: bare ribs over the open front, some snapped
	for (float X : { 440.f })
	{
		const int32 k = (int32)X + 900;
		if (KJ(k, 1.f) > -0.1f || X > 400.f) { Bar(FVector(X, 6.f, Slope(6.f)), FVector(X, -430.f, Zr - 430.f * Tan), 7.f, 14.f, "TimberDark"); }
		else
		{
			Bar(FVector(X, 6.f, Slope(6.f)), FVector(X, -180.f, Zr - 180.f * Tan), 7.f, 14.f, "TimberDark");
			Bar(FVector(X, -180.f, Zr - 180.f * Tan), FVector(X + 30.f, -240.f, Zr - 180.f * Tan - 170.f), 7.f, 14.f, "TimberDark");
		}
	}
	// loft joists: from the back wall plate out over the room, broken where the front wall gave way
	for (float X : { 330.f, 40.f, -250.f })
	{
		const float YEnd = (X > 300.f) ? -412.f : -150.f - KA((int32)X + 70, 60.f);
		Bar(FVector(X, 412.f, Ze), FVector(X, YEnd, Ze), 14.f, 18.f, "TimberDark");
		if (X < 300.f) { Bar(FVector(X, YEnd, Ze - 4.f), FVector(X + 18.f, YEnd - 60.f, Ze - 110.f), 12.f, 16.f, "TimberDark"); }
	}
	// the corner pier where the front walls met, and a fallen roof slab of tiles in the front-left
	Deco(EDCShape::Cube, FVector(-500.f, -400.f, 105.f), FVector(RWallT + 10.f, RWallT + 10.f, 210.f), "Stone");
	Deco(EDCShape::Cube, FVector(-505.f, -395.f, 222.f), FVector(20.f, 22.f, 24.f), "Stone", FRotator(10, 20, 5));
	Ivy(FVector(-500.f, -418.f, 0.f), FVector(0, -1, 0), 50.f, 190.f, 91);
}

void ADCHideoutBuilder::BuildWingRoof(const FRoomDef& Def)
{
	// Skeletal lean-to: rafters out from the tall exterior back walls, a few rusty tin sheets near the wall.
	CurGroup = DCGroups::Level1;
	const FVector2D C = Def.Rect.GetCenter(), Half = Def.Rect.GetSize() * 0.5f;
	const FBox2D Comp = DCSite::CompoundRect();
	const bool bGarage = Def.Type == EDCRoomType::Garage;
	const float Z = RWingH + 12.f;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const bool bExt = Side == 0 ? FMath::IsNearlyEqual(Def.Rect.Max.X, Comp.Max.X, 1.f) : FMath::IsNearlyEqual(Def.Rect.Max.Y, Comp.Max.Y, 1.f);
		if (!bExt) { continue; }
		const bool bAlongX = Side == 1;
		const float Line = Side == 0 ? Half.X - RWallT : Half.Y - RWallT;
		const float L0 = bAlongX ? -Half.X + 30.f : -Half.Y + 30.f, L1 = -L0;
		const float Depth = bGarage ? 230.f : 170.f;
		auto P = [&](float Al, float In, float Zz) { return bAlongX ? FVector(Al, Line - In, Zz) : FVector(Line - In, Al, Zz); };
		int32 i = 0;
		for (float Al = L0; Al <= L1 + 1.f; Al += 110.f, ++i)
		{
			const int32 k = (int32)Def.Type * 37 + Side * 11 + i;
			const float D = (KJ(k, 1.f) > -0.3f) ? Depth : Depth * 0.45f;
			Bar(P(Al, -6.f, Z), P(Al, D, Z + D * 0.18f), 7.f, 12.f, "Timber");
			if (Al + 110.f <= L1 + 1.f && KJ(k + 5, 1.f) > (bGarage ? -0.5f : 0.35f))
			{
				const float SD = Depth * (bGarage ? 0.95f : 0.6f);
				const FVector A = P(Al + 55.f, -10.f, Z + 9.f), B = P(Al + 55.f, SD, Z + 9.f + SD * 0.18f);
				Bar(A, B, 112.f, 2.f, bAlongX ? FName("TinRust") : FName("TinRustV"));
			}
		}
		Bar(P(L0, Depth * 0.5f, Z + Depth * 0.09f + 8.f), P(L1, Depth * 0.5f, Z + Depth * 0.09f + 8.f), 6.f, 6.f, "Timber");
	}
}

// =====================================================================================================================
// Practicals & props
// =====================================================================================================================

void ADCHideoutBuilder::HangingBulb(const FVector& C, float TopZ, const FLinearColor& Col, float Cd, float Radius, bool bShadow)
{
	Tube(FVector(C.X, C.Y, TopZ), C + FVector(0, 0, 9), 0.8f, "Black", false);
	Deco(EDCShape::Cylinder, C + FVector(0, 0, 8), FVector(4, 4, 6), "Black", FRotator::ZeroRotator, false);
	Deco(EDCShape::Sphere, C, FVector(7, 7, 9), "Tungsten", FRotator::ZeroRotator, false);
	Point(C - FVector(0, 0, 4), Col, Cd, Radius, bShadow, true, 2.f);
}

void ADCHideoutBuilder::StringLights(const FVector& A, const FVector& B, float Sag, int32 Bulbs, int32 Lights, float Cd)
{
	auto At = [&](float T) { return FMath::Lerp(A, B, T) - FVector(0, 0, Sag * 4.f * T * (1.f - T)); };
	const int32 Seg = FMath::Max(4, Bulbs);
	for (int32 i = 0; i < Seg; ++i) { Tube(At((float)i / Seg), At((float)(i + 1) / Seg), 0.6f, "Black", false); }
	for (int32 i = 0; i < Bulbs; ++i)
	{
		const FVector P = At((i + 0.5f) / Bulbs);
		Deco(EDCShape::Sphere, P - FVector(0, 0, 5), FVector(4.5f, 4.5f, 6.f), "StringBulb", FRotator::ZeroRotator, false);
	}
	for (int32 i = 0; i < Lights; ++i) { Point(At((i + 0.5f) / Lights) - FVector(0, 0, 12), FLinearColor(1.f, 0.55f, 0.2f), Cd, 460.f, false, true, 20.f); }
}

void ADCHideoutBuilder::WorkLight(const FVector& B, float Yaw, bool bLight, float Cd, bool bRoomLight)
{
	const FRotator YR(0, Yaw, 0);
	Tube(B + YR.RotateVector(FVector(-25, -22, 0)), B + FVector(0, 0, 165), 3, "SteelBlack");
	Tube(B + YR.RotateVector(FVector(-25, 22, 0)), B + FVector(0, 0, 165), 3, "SteelBlack");
	Tube(B + YR.RotateVector(FVector(28, 0, 0)), B + FVector(0, 0, 165), 3, "SteelBlack");
	const FVector H = B + FVector(0, 0, 178);
	Deco(EDCShape::Cube, H, FVector(14, 30, 24), "PaintYellow", Compose(Yaw, FRotator(-25, 0, 0)));
	Deco(EDCShape::Cube, H + YR.RotateVector(FVector(7.5f, 0, -2)), FVector(1.5f, 25, 19), "Tungsten", Compose(Yaw, FRotator(-25, 0, 0)), false);
	if (bLight)
	{
		Spot(H + YR.RotateVector(FVector(12, 0, -4)), B + YR.RotateVector(FVector(260, 0, 0)), FLinearColor(1.f, 0.7f, 0.4f), Cd, 900.f, 30.f, 62.f, false, bRoomLight);
	}
}

void ADCHideoutBuilder::CampChair(const FVector& C, float Yaw, FName Fabric)
{
	const FRotator YR(0, Yaw, 0);
	auto At = [&](float X, float Y, float Z) { return C + YR.RotateVector(FVector(X, Y, Z)); };
	for (float S : { -1.f, 1.f })
	{
		KTube(At(-20, S * 22, 0), At(20, S * 22, 42), 2.2f, "SteelBlack");
		KTube(At(20, S * 22, 0), At(-20, S * 22, 42), 2.2f, "SteelBlack");
		KTube(At(-20, S * 22, 42), At(-26, S * 22, 92), 2.2f, "SteelBlack");
	}
	K(EDCShape::Cube, At(0, 0, 40), FVector(44, 46, 3), Fabric, Compose(Yaw, FRotator(-6, 0, 0)));
	K(EDCShape::Cube, At(-24, 0, 70), FVector(3, 46, 40), Fabric, Compose(Yaw, FRotator(-10, 0, 0)));
	K(EDCShape::Cylinder, At(20, 26, 44), FVector(8, 8, 3), "SteelBlack"); // cup holder
}

void ADCHideoutBuilder::Generator(const FVector& C, float Yaw)
{
	const FRotator YR(0, Yaw, 0);
	auto At = [&](float X, float Y, float Z) { return C + YR.RotateVector(FVector(X, Y, Z)); };
	for (float SX : { -1.f, 1.f }) { for (float SY : { -1.f, 1.f }) { KTube(At(SX * 38, SY * 26, 0), At(SX * 38, SY * 26, 62), 3.f, "SteelBlack"); } }
	for (float SY : { -1.f, 1.f }) { KTube(At(-38, SY * 26, 62), At(38, SY * 26, 62), 3.f, "SteelBlack"); }
	K(EDCShape::Cube, At(0, 0, 30), FVector(66, 44, 44), "PaintRed", YR);
	K(EDCShape::Cube, At(8, 0, 56), FVector(40, 36, 12), "PaintRed", YR);       // tank
	K(EDCShape::Cylinder, At(14, 0, 64), FVector(8, 8, 4), "SteelBlack");
	K(EDCShape::Cube, At(-34, 0, 30), FVector(2, 30, 26), "SteelBlack", YR);     // control panel
	K(EDCShape::Cube, At(-35.5f, -8, 36), FVector(1, 3, 3), "LedG", YR, false);
	K(EDCShape::Cube, At(-35.5f, 2, 36), FVector(1, 3, 3), "LedR", YR, false);
	KTube(At(30, 18, 30), At(46, 18, 90), 4.f, "Rust");                          // exhaust
	K(EDCShape::Cube, At(0, 0, 2), FVector(90, 60, 4), "RubberMat", YR);
}

void ADCHideoutBuilder::Lantern(const FVector& C, bool bLight)
{
	K(EDCShape::Cylinder, C + FVector(0, 0, 2), FVector(14, 14, 4), "Rust");
	K(EDCShape::Cylinder, C + FVector(0, 0, 14), FVector(11, 11, 20), "Glass", FRotator::ZeroRotator, false);
	K(EDCShape::Sphere, C + FVector(0, 0, 12), FVector(3.5f, 3.5f, 6.f), "Candle", FRotator::ZeroRotator, false);
	K(EDCShape::Cone, C + FVector(0, 0, 28), FVector(14, 14, 8), "Rust");
	KTube(C + FVector(-6, 0, 32), C + FVector(6, 0, 32), 1.f, "Rust");
	if (bLight)
	{
		UPointLightComponent* L = Point(C + FVector(0, 0, 14), FLinearColor(1.f, 0.45f, 0.12f), 7.f, 320.f, false, true, 3.f);
		AddFlicker(L, 6.f, 0.45f);
	}
}

// =====================================================================================================================
// Plot (Locked) and construction (Building) dressing
// =====================================================================================================================

void ADCHideoutBuilder::LockedDressing(const FBox2D& R, const TArray<FDCQuad>& Piles)
{
	// An empty, overgrown plot: survey stakes and a string line outlining the footprint, a few stacked materials.
	CurGroup = DCGroups::Locked;
	const FVector2D C = R.GetCenter(), S = R.GetSize();
	const int32 Seed = (int32)(C.X * 0.37f + C.Y * 0.11f + S.X) & 0xffff;
	Deco(EDCShape::Cube, FVector(C.X + KJ(Seed, 40.f), C.Y + KJ(Seed + 1, 40.f), -2.4f), FVector(S.X * 0.72f, S.Y * 0.66f, 1.f), "Dirt", FRotator(0, KJ(Seed + 2, 6.f), 0), false);
	const float In = 12.f;
	TArray<FVector> Stakes = { FVector(R.Min.X + In, R.Min.Y + In, 0), FVector(R.Max.X - In, R.Min.Y + In, 0), FVector(R.Max.X - In, R.Max.Y - In, 0), FVector(R.Min.X + In, R.Max.Y - In, 0) };
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector A = Stakes[i], B = Stakes[(i + 1) % 4];
		const int32 NMid = FMath::FloorToInt((B - A).Size() / 420.f);
		for (int32 m = 0; m <= NMid; ++m)
		{
			const FVector P = FMath::Lerp(A, B, (float)m / (NMid + 1));
			Deco(EDCShape::Cube, P + FVector(0, 0, 28), FVector(4, 4, 62), "FreshTimber", FRotator(KJ(Seed + i * 9 + m, 4.f), 0, KJ(Seed + i * 5 + m, 4.f)));
			Deco(EDCShape::Cube, P + FVector(0, 0, 54), FVector(1.5f, 9, 6), "PaintOrange", FRotator::ZeroRotator, false);
		}
		Tube(A + FVector(0, 0, 44), B + FVector(0, 0, 44), 0.9f, "Rope", false);
	}
	// tall weeds across the plot
	for (int32 i = 0; i < 9; ++i)
	{
		const FVector P(FMath::Lerp(R.Min.X + 60.f, R.Max.X - 60.f, FMath::Frac(i * 0.618034f + Seed * 0.01f)), FMath::Lerp(R.Min.Y + 60.f, R.Max.Y - 60.f, FMath::Frac(i * 0.381966f + Seed * 0.03f)), 0);
		Weeds(P, 70.f + KA(Seed + i, 40.f), 7, Seed + i * 3, 75.f);
	}
	// materials waiting for the builders
	for (int32 j = 0; j < Piles.Num(); ++j)
	{
		const FVector P(Piles[j].X, Piles[j].Y, 0);
		const int32 k = Seed + j * 17;
		switch (j % 3)
		{
		case 0: // stacked timbers
			for (int32 b = 0; b < 6; ++b) { Deco(EDCShape::Cube, P + FVector(KJ(k + b, 4.f), (b % 3) * 16.f - 16.f, 5.f + (b / 3) * 10.f), FVector(240, 14, 9), b % 2 ? FName("TimberGrey") : FName("FreshTimber"), FRotator(0, 8.f + KJ(k + b, 3.f), 0)); }
			Deco(EDCShape::Cube, P + FVector(0, 0, 2), FVector(10, 60, 4), "Timber", FRotator(0, 8, 0));
			break;
		case 1: // pallet of reclaimed bricks
			Deco(EDCShape::Cube, P + FVector(0, 0, 7), FVector(100, 100, 13), "Wood", FRotator(0, KJ(k, 10.f), 0));
			Deco(EDCShape::Cube, P + FVector(0, 0, 38), FVector(88, 88, 50), "BrickOld", FRotator(0, KJ(k, 10.f), 0));
			Deco(EDCShape::Cube, P + FVector(10, -10, 70), FVector(60, 50, 14), "BrickOld", FRotator(0, KJ(k, 10.f) + 12.f, 0));
			break;
		default: // tarp over a heap of stone
			RubblePile(P, 70.f, 10, k, "Stone");
			Deco(EDCShape::Sphere, P + FVector(0, 0, 10), FVector(150, 120, 40), "Canvas", FRotator(0, KJ(k, 40.f), 0));
			break;
		}
	}
}

void ADCHideoutBuilder::BuildingDressing(const FBox2D& R)
{
	// Construction in progress: poured slab, timber stud frame, scaffold, mixer, warm work lights.
	CurGroup = DCGroups::Building;
	const FVector2D C = R.GetCenter(), S = R.GetSize();
	const FVector Up(0, 0, 1);
	Deco(EDCShape::Cube, FVector(C.X, C.Y, -4.6f), FVector(S.X - 12.f, S.Y - 12.f, 7.f), "Cement");   // just under the finished floor
	Deco(EDCShape::Cube, FVector(R.Min.X + 3.f, C.Y, -1.f), FVector(3, S.Y - 4.f, 12.f), "Plywood");
	Deco(EDCShape::Cube, FVector(C.X, R.Min.Y + 3.f, -1.f), FVector(S.X - 4.f, 3, 12.f), "Plywood");
	auto Frame = [&](const FVector2D& P0, const FVector2D& P1, float H, float Step)
	{
		const FVector A(P0.X, P0.Y, 0), B(P1.X, P1.Y, 0);
		const FVector D = B - A;
		const float Len = D.Size();
		Bar(A + Up * 3.f, B + Up * 3.f, 10.f, 6.f, "FreshTimber");
		Bar(A + Up * (H - 3.f), B + Up * (H - 3.f), 10.f, 6.f, "FreshTimber");
		const int32 N = FMath::Max(1, FMath::RoundToInt(Len / Step));
		for (int32 i = 0; i <= N; ++i) { const FVector P = A + D * ((float)i / N); Deco(EDCShape::Cube, P + Up * (H * 0.5f), FVector(5, 9, H - 12.f), "FreshTimber", D.Rotation()); }
		Bar(A + Up * 12.f, A + D * FMath::Min(1.f, 230.f / Len) + Up * (H - 12.f), 4.f, 9.f, "FreshTimber");
	};
	const float In = 16.f, H = RWingH - 8.f;
	Frame(FVector2D(R.Max.X - In, R.Min.Y + In), FVector2D(R.Max.X - In, R.Max.Y - In), H, 61.f);
	Frame(FVector2D(R.Min.X + In, R.Max.Y - In), FVector2D(R.Max.X - In, R.Max.Y - In), H, 61.f);
	Frame(FVector2D(R.Min.X + In, R.Min.Y + In), FVector2D(R.Min.X + In, R.Max.Y - In), H, 122.f);
	Frame(FVector2D(R.Min.X + In, R.Min.Y + In), FVector2D(R.Max.X - In, R.Min.Y + In), H, 122.f);
	// scaffold along the back
	Scaffold(FVector(R.Min.X + 80.f, R.Max.Y - 120.f, 0), FVector(R.Max.X - 80.f, R.Max.Y - 120.f, 0), 260.f);
	// cement mixer
	{
		const FVector M(R.Min.X + 120.f, R.Min.Y + 140.f, 0);
		Tube(M + FVector(-35, -30, 0), M + FVector(0, 0, 70), 4, "SteelBlack");
		Tube(M + FVector(35, -30, 0), M + FVector(0, 0, 70), 4, "SteelBlack");
		Tube(M + FVector(0, 40, 0), M + FVector(0, 0, 70), 4, "SteelBlack");
		Deco(EDCShape::Cylinder, M + FVector(-38, -30, 14), FVector(28, 28, 8), "Tire", FRotator(0, 0, 90));
		Deco(EDCShape::Sphere, M + FVector(0, 0, 100), FVector(70, 70, 76), "PaintOrange", FRotator(35, 30, 0));
		Deco(EDCShape::Cone, M + FVector(18, 12, 138), FVector(50, 50, 34), "PaintOrange", FRotator(35, 30, 0));
		Deco(EDCShape::Cylinder, M + FVector(26, 17, 154), FVector(30, 30, 2), "Black", FRotator(35, 30, 0), false);
	}
	// sand heap, brick pallet, timber stack, wheelbarrow
	Deco(EDCShape::Cone, FVector(R.Min.X + 110.f, C.Y + 40.f, 26.f), FVector(150, 130, 52), "Sand");
	Deco(EDCShape::Cube, FVector(C.X - 60.f, C.Y - 70.f, 7.f), FVector(100, 100, 13), "Wood");
	Deco(EDCShape::Cube, FVector(C.X - 60.f, C.Y - 70.f, 40.f), FVector(90, 90, 54), "BrickOld");
	for (int32 b = 0; b < 8; ++b) { Deco(EDCShape::Cube, FVector(C.X + 110.f, C.Y + 90.f + (b % 4) * 14.f, 5.f + (b / 4) * 10.f), FVector(260, 12, 9), "FreshTimber", FRotator(0, 4, 0)); }
	{
		const FVector W(C.X + 40.f, R.Min.Y + 110.f, 0);
		Deco(EDCShape::Cube, W + FVector(0, 0, 50), FVector(90, 60, 30), "Galv", FRotator(-10, 20, 0));
		Deco(EDCShape::Cylinder, W + FVector(52, 18, 18), FVector(34, 34, 8), "Tire", FRotator(0, 20, 90));
		Tube(W + FVector(-40, -30, 45), W + FVector(-110, -55, 55), 3, "Wood");
		Tube(W + FVector(-30, 25, 45), W + FVector(-95, 5, 55), 3, "Wood");
	}
	// warm work lights on tripods (only shown while building)
	WorkLight(FVector(R.Min.X + 70.f, R.Min.Y + 70.f, 0), 45.f, true, 130.f, false);
	WorkLight(FVector(R.Max.X - 120.f, C.Y - 40.f, 0), 200.f, true, 90.f, false);
}
