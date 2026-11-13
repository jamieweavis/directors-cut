// Director's Cut — reusable prop kit for the warehouse hideout (cars, monitors, chairs, racking, lamps...).
// All positions are CurRoom-local. With bKitOnStation the parts become CurStation props (hover highlight, clickable).
#include "World/Scene/DCHideoutBuilder.h"
#include "World/DCRoomModule.h"
#include "World/DCStation.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"

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
