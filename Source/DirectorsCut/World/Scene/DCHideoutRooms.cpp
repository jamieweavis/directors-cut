// Director's Cut — per-room dressing for the hideout diorama (Agent D). All coordinates are room-local cm
// (room actor sits at the centre of its floor rect, floor top at z=0). Camera looks from -X-Y, so props
// are pushed towards the +X/+Y walls and silhouettes face -X/-Y.
#include "World/Scene/DCHideoutBuilder.h"
#include "World/DCRoomModule.h"
#include "World/DCStation.h"
#include "World/DCCriminalSpawnZone.h"
#include "World/DCGarageDoor.h"
#include "Core/DCWorldTags.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Engine/World.h"

namespace
{
	// deterministic jitter
	float J(int32 I, float Amp) { return (FMath::Frac(FMath::Sin(I * 12.9898f + 4.1f) * 43758.5453f) * 2.f - 1.f) * Amp; }
	const FLinearColor Warm(1.f, 0.62f, 0.32f);   // ~2700K
	const FLinearColor Tungsten(1.f, 0.72f, 0.45f);
	const FLinearColor Cool(0.75f, 0.88f, 1.f);
}

static ULocalFogVolumeComponent* AddLocalFog(ADCRoomModule* Room, FName Group, const FVector& Loc, const FVector& Radius, float Density)
{
	ULocalFogVolumeComponent* F = NewObject<ULocalFogVolumeComponent>(Room);
	F->SetupAttachment(Room->GetRootComponent());
	F->SetRelativeLocation(Loc);
	F->SetRelativeScale3D(Radius / ULocalFogVolumeComponent::GetBaseVolumeSize());
	F->RadialFogExtinction = Density;
	F->HeightFogExtinction = 0.f;
	F->FogAlbedo = FLinearColor(0.9f, 0.9f, 0.9f);
	F->FogPhaseG = 0.45f;
	F->RegisterComponent();
	Room->AddInstanceComponent(F);
	Room->AddToGroup(F, Group);
	return F;
}

// =====================================================================================================================
// OFFICE  (local == world; x[-500,500] y[-400,400]; +X exterior wall has the window + street door; +Y is the booth wall)
// =====================================================================================================================
void ADCHideoutBuilder::BuildOffice()
{
	CurRoom = GetRoom(EDCRoomType::Office);

	// ---------------- shell extras: window frame, blinds, rain pane, door frame
	CurGroup = DCGroups::Shell;
	const float WX = 478.f; // inner face of the +X wall
	Deco(EDCShape::Cube, FVector(WX - 6, 150, 162), FVector(16, 236, 6), "DarkWood");
	Deco(EDCShape::Cube, FVector(WX - 2, 150, 338), FVector(8, 236, 6), "DarkWood");
	Deco(EDCShape::Cube, FVector(WX - 2, 37, 250), FVector(8, 6, 176), "DarkWood");
	Deco(EDCShape::Cube, FVector(WX - 2, 263, 250), FVector(8, 6, 176), "DarkWood");
	Deco(EDCShape::Cube, FVector(WX - 2, 150, 250), FVector(6, 4, 170), "DarkWood"); // mullion
	Deco(EDCShape::Cube, FVector(492, 150, 250), FVector(1, 222, 172), "Rain", FRotator::ZeroRotator, false);
	for (int32 i = 0; i < 14; ++i)
	{
		Deco(EDCShape::Cube, FVector(WX - 10, 150, 176 + i * 11.5f), FVector(7, 218, 1.0f), "Cream", FRotator(32.f, 0, 0));
	}
	Deco(EDCShape::Cube, FVector(WX - 10, 150, 336), FVector(8, 224, 7), "Cream");
	Tube(FVector(WX - 13, 100, 336), FVector(WX - 13, 100, 175), 0.6f, "Cream");
	Tube(FVector(WX - 13, 200, 336), FVector(WX - 13, 200, 175), 0.6f, "Cream");
	// street door frame + EXIT
	Deco(EDCShape::Cube, FVector(WX - 2, -250, 229), FVector(8, 136, 8), "DarkWood");
	Deco(EDCShape::Cube, FVector(WX - 2, -314, 112), FVector(8, 8, 226), "DarkWood");
	Deco(EDCShape::Cube, FVector(WX - 2, -186, 112), FVector(8, 8, 226), "DarkWood");
	// projection port frame
	Deco(EDCShape::Cube, FVector(300, 386, 117), FVector(70, 4, 60), "Brass");
	Deco(EDCShape::Cube, FVector(300, 385, 117), FVector(54, 6, 44), "Black");
	// blade sign structure outside (in the notch), board + brackets
	const float SY = -60.f;
	Deco(EDCShape::Cube, FVector(555, SY, 470), FVector(90, 14, 400), "Iron");
	Tube(FVector(500, SY, 640), FVector(600, SY, 660), 4, "Iron");
	Tube(FVector(500, SY, 300), FVector(600, SY, 270), 4, "Iron");

	CurGroup = DCGroups::Level1;
	// neon on the blade sign: vertical CINEMA in pink, cyan border, warm marquee bulbs
	NeonText(TEXT("CINEMA"), FVector(568, SY - 8.5f, 600), FVector(-1, 0, 0), FVector(0, 0, 1), 13.f, 3.5f, "NeonPink", true);
	Tube(FVector(508, SY - 8.5f, 275), FVector(508, SY - 8.5f, 665), 3, "NeonCyan", false);
	Tube(FVector(602, SY - 8.5f, 275), FVector(602, SY - 8.5f, 665), 3, "NeonCyan", false);
	Tube(FVector(508, SY - 8.5f, 665), FVector(602, SY - 8.5f, 665), 3, "NeonCyan", false);
	Tube(FVector(508, SY - 8.5f, 275), FVector(602, SY - 8.5f, 275), 3, "NeonCyan", false);
	for (int32 i = 0; i < 7; ++i) { Deco(EDCShape::Sphere, FVector(515 + i * 13.f, SY - 9, 676), FVector(5), "Marquee", FRotator::ZeroRotator, false); }
	// the spill: pink sign light through the window + blinds into a local fog volume => shafts
	USpotLightComponent* Spill = Spot(FVector(700, 160, 420), FVector(-100, 40, 0), FLinearColor(1.f, 0.22f, 0.55f), 2200.f, 2600.f, 6.f, 17.f, true, false);
	Spill->SetVolumetricScatteringIntensity(4.f);
	Spill->SourceRadius = 8.f;
	AddFlicker(Spill, 9.f, 0.25f);
	// faint cyan bounce from the border tubes (no shadow, low; mostly lights the sign board and window reveal)
	UPointLightComponent* CyanGlow = Point(FVector(540, SY - 40, 560), FLinearColor(0.1f, 0.7f, 1.f), 60.f, 420.f, false, false, 10.f);
	CyanGlow->SetVolumetricScatteringIntensity(0.f);
	AddLocalFog(CurRoom, DCGroups::Level1, FVector(180, 80, 140), FVector(380, 300, 220), 0.35f);
	// dust motes drifting in the shaft
	for (int32 i = 0; i < 240; ++i)
	{
		const float T = FMath::Frac(i * 0.618034f);
		const FVector A(470, 150, 250), B(-100, 40, 0);
		const FVector P = FMath::Lerp(A, B, T) + FVector(J(i, 60.f), J(i + 99, 110.f), J(i + 7, 60.f)) * (0.4f + T);
		if (P.Z < 10.f) { continue; }
		Deco(EDCShape::Sphere, P, FVector(0.9f), "DustPink", FRotator::ZeroRotator, false);
	}

	// rug + cord pendant (fill)
	Deco(EDCShape::Cube, FVector(-10, -20, 0.6f), FVector(340, 440, 1.2f), "CarpetRed");
	Deco(EDCShape::Cube, FVector(-10, -20, 0.5f), FVector(352, 452, 1.0f), "Cream");
	Tube(FVector(-120, 30, 380), FVector(-120, 30, 300), 1.2f, "Black");
	Deco(EDCShape::Cone, FVector(-120, 30, 292), FVector(34, 34, 18), "Iron");
	Deco(EDCShape::Sphere, FVector(-120, 30, 283), FVector(9), "Bulb", FRotator::ZeroRotator, false);
	Point(FVector(-120, 30, 270), Warm, 22.f, 900.f, false, true, 4.f);

	// ---------------- Director's Desk
	BeginStation(EDCStationType::DirectorDesk, TEXT("Director's Desk"), FVector(40, -30, 0), FVector(80, 130, 75));
	SP(EDCShape::Cube, FVector(0, 0, 76), FVector(104, 224, 6), "Mahogany");
	SP(EDCShape::Cube, FVector(0, -78, 37), FVector(94, 60, 72), "Mahogany");
	SP(EDCShape::Cube, FVector(0, 78, 37), FVector(94, 60, 72), "Mahogany");
	SP(EDCShape::Cube, FVector(-42, 0, 45), FVector(6, 100, 56), "Mahogany");
	SP(EDCShape::Cube, FVector(-53, 0, 73), FVector(2, 226, 3), "Brass");
	SP(EDCShape::Cube, FVector(-48, -78, 2), FVector(4, 62, 4), "Brass");
	SP(EDCShape::Cube, FVector(-48, 78, 2), FVector(4, 62, 4), "Brass");
	SP(EDCShape::Cube, FVector(0, 0, 79.6f), FVector(72, 140, 1), "LeatherGrn");
	// banker's lamp
	SP(EDCShape::Cylinder, FVector(12, 72, 80.5f), FVector(18, 18, 3), "Brass");
	SP(EDCShape::Cylinder, FVector(12, 72, 94), FVector(2.5f, 2.5f, 26), "Brass");
	SP(EDCShape::Cylinder, FVector(6, 72, 110), FVector(17, 17, 38), "LampGreen", FRotator(0, 0, 90));
	SP(EDCShape::Cylinder, FVector(6, 72, 105.5f), FVector(9, 9, 32), "Bulb", FRotator(0, 0, 90), false);
	SPTube(FVector(8, 86, 104), FVector(8, 86, 92), 0.5f, "Brass");
	// ledger, pen, phone, glass, bottle, ashtray, cash, nameplate
	SP(EDCShape::Cube, FVector(-10, -18, 80.2f), FVector(34, 52, 1.2f), "Paper", FRotator(0, -8, 0));
	SP(EDCShape::Cube, FVector(-10, -18, 80.6f), FVector(34, 1.6f, 1.4f), "BookRed", FRotator(0, -8, 0));
	SPTube(FVector(-4, -2, 81.2f), FVector(-20, 6, 81.2f), 1.f, "Black");
	SP(EDCShape::Cube, FVector(18, -45, 82), FVector(28, 36, 5), "BookRed", FRotator(0, 12, 0));
	SP(EDCShape::Cube, FVector(-6, -84, 84), FVector(22, 24, 9), "Black", FRotator(0, 20, 0));
	SP(EDCShape::Cylinder, FVector(-6, -84, 92.5f), FVector(5.5f, 5.5f, 26), "Black", FRotator(0, 20, 90));
	SP(EDCShape::Cylinder, FVector(-15, -81, 88.8f), FVector(12, 12, 1.4f), "Cream", FRotator(0, 0, 0));
	SP(EDCShape::Cylinder, FVector(-28, 40, 84.5f), FVector(7, 7, 9), "Glass", FRotator::ZeroRotator, false);
	SP(EDCShape::Cylinder, FVector(-28, 40, 82.3f), FVector(6, 6, 4.4f), "Whisky", FRotator::ZeroRotator, false);
	SP(EDCShape::Cylinder, FVector(28, 104, 92), FVector(10, 10, 22), "Bottle");
	SP(EDCShape::Cylinder, FVector(28, 104, 108), FVector(3.5f, 3.5f, 10), "Bottle");
	SP(EDCShape::Cylinder, FVector(-30, -56, 80.5f), FVector(14, 14, 2.5f), "Chrome");
	SPTube(FVector(-30, -51, 82.2f), FVector(-31, -42, 83.2f), 1.3f, "Paper");
	SP(EDCShape::Sphere, FVector(-31, -42, 83.2f), FVector(1.6f), "Ember", FRotator::ZeroRotator, false);
	for (int32 i = 0; i < 3; ++i) { SP(EDCShape::Cube, FVector(22 + J(i, 3), 30 + i * 2.f, 80.5f + i * 3.1f), FVector(16, 7, 3), "Cash", FRotator(0, J(i + 3, 15), 0)); }
	SP(EDCShape::Cube, FVector(-44, 0, 84), FVector(3, 40, 7), "Brass", FRotator(-15, 0, 0));
	// chair
	SP(EDCShape::Cube, FVector(98, 0, 50), FVector(56, 58, 12), "Leather");
	SP(EDCShape::Cube, FVector(126, 0, 98), FVector(12, 60, 84), "Leather", FRotator(-8, 0, 0));
	SP(EDCShape::Cube, FVector(98, -33, 66), FVector(52, 7, 7), "Leather");
	SP(EDCShape::Cube, FVector(98, 33, 66), FVector(52, 7, 7), "Leather");
	SP(EDCShape::Cylinder, FVector(98, 0, 25), FVector(7, 7, 40), "Chrome");
	SP(EDCShape::Cylinder, FVector(98, 0, 4), FVector(54, 54, 4), "Chrome");
	// the key light: warm pool from the green shade + tiny fill so the shade glows
	{
		USpotLightComponent* Key = Spot(FVector(46, 42, 104), FVector(15, 25, 0), Warm, 55.f, 650.f, 30.f, 68.f, true, true);
		Key->SourceRadius = 6.f;
		Point(FVector(46, 42, 118), Warm, 4.f, 260.f, false, true, 6.f);
	}
	// cigarette smoke curl
	for (int32 i = 0; i < 6; ++i)
	{
		const float S = 6.f + i * 3.5f;
		Deco(EDCShape::Sphere, FVector(9 + FMath::Sin(i * 0.9f) * 6.f, -72 + i * 1.5f, 92 + i * 13.f), FVector(S, S, 16 + i * 5.f), "Smoke", FRotator(0, 0, J(i, 15.f)), false);
	}
	CurGroup = DCGroups::Level3; // gold statuette
	SP(EDCShape::Cube, FVector(30, 100, 82), FVector(9, 9, 5), "Black");
	SP(EDCShape::Cylinder, FVector(30, 100, 93), FVector(4.5f, 4.5f, 18), "Brass");
	SP(EDCShape::Sphere, FVector(30, 100, 104), FVector(5), "Brass");
	CurGroup = DCGroups::Level1;

	// ---------------- Job Board (on the booth wall)
	BeginStation(EDCStationType::JobBoard, TEXT("Job Board"), FVector(-220, 370, 85), FVector(128, 20, 82));
	SP(EDCShape::Cube, FVector(0, 9, 80), FVector(244, 6, 162), "DarkWood");
	SP(EDCShape::Cube, FVector(0, 5, 80), FVector(228, 4, 146), "Cork");
	TArray<FVector> Pins;
	for (int32 i = 0; i < 10; ++i)
	{
		const float PX = -92 + (i % 5) * 46.f + J(i, 8), PZ = 118 - (i / 5) * 62.f + J(i + 50, 8);
		SP(EDCShape::Cube, FVector(PX, 2.5f, PZ), FVector(16, 1, 19), "Paper", FRotator(J(i + 9, 9), 0, 0));
		SP(EDCShape::Cube, FVector(PX, 1.8f, PZ + 1.5f), FVector(13, 0.6f, 13), "Photo", FRotator(J(i + 9, 9), 0, 0));
		SP(EDCShape::Sphere, FVector(PX, 0.5f, PZ + 8), FVector(2.8f), "Pin");
		Pins.Add(FVector(PX, 0.2f, PZ + 8));
	}
	const int32 Links[][2] = { {0,6},{6,2},{2,8},{8,4},{1,7},{7,3},{5,1},{9,3} };
	for (const auto& L : Links) { SPTube(Pins[L[0]], Pins[L[1]], 0.7f, "RedString"); }
	auto Card = [&](float X, float Z, int32 Seed)
	{
		SP(EDCShape::Cube, FVector(X, 2.f, Z), FVector(26, 1, 18), "Cream", FRotator(J(Seed, 4), 0, 0));
		SP(EDCShape::Cube, FVector(X - 2, 1.3f, Z + 4), FVector(16, 0.4f, 1.5f), "Black", FRotator(J(Seed, 4), 0, 0));
		SP(EDCShape::Cube, FVector(X, 1.3f, Z), FVector(20, 0.4f, 1.0f), "Black", FRotator(J(Seed, 4), 0, 0));
		SP(EDCShape::Sphere, FVector(X, 0.8f, Z + 8), FVector(2.6f), "Pin");
	};
	Card(-75, 25, 1); Card(-35, 22, 2); Card(5, 26, 3);
	CurGroup = DCGroups::Level2; Card(45, 24, 4);
	CurGroup = DCGroups::Level3; Card(85, 27, 5);
	CurGroup = DCGroups::Level1;
	SP(EDCShape::Cube, FVector(0, -6, 168), FVector(70, 7, 5), "Brass");
	Spot(FVector(-220, 352, 262), FVector(-220, 384, 150), Tungsten, 30.f, 420.f, 35.f, 70.f, false, true);

	// ---------------- Projector (corner, shoots through the booth port into the auditorium)
	BeginStation(EDCStationType::Projector, TEXT("Projector"), FVector(300, 300, 0), FVector(42, 55, 92));
	SP(EDCShape::Cylinder, FVector(0, 0, 3), FVector(56, 56, 6), "Iron");
	SP(EDCShape::Cylinder, FVector(0, 0, 45), FVector(30, 30, 80), "Iron");
	SP(EDCShape::Cube, FVector(0, 4, 105), FVector(32, 58, 36), "Iron");
	SP(EDCShape::Cylinder, FVector(0, -32, 112), FVector(24, 24, 32), "Steel");
	SP(EDCShape::Cylinder, FVector(0, -32, 134), FVector(10, 10, 16), "Iron");
	SP(EDCShape::Cylinder, FVector(0, -12, 152), FVector(48, 48, 4), "Steel", FRotator(90, 0, 0));
	SP(EDCShape::Cylinder, FVector(0, -12, 152), FVector(12, 12, 6), "Iron", FRotator(90, 0, 0));
	SP(EDCShape::Cylinder, FVector(0, 26, 145), FVector(40, 40, 4), "Steel", FRotator(90, 0, 0));
	SP(EDCShape::Cylinder, FVector(0, 38, 105), FVector(13, 13, 22), "Brass", FRotator(0, 0, 90));
	SP(EDCShape::Sphere, FVector(0, 49, 105), FVector(9), "BulbCool", FRotator::ZeroRotator, false);
	SPTube(FVector(0, -12, 152), FVector(0, 10, 118), 0.8f, "Black");
	SPTube(FVector(0, 26, 145), FVector(0, 18, 118), 0.8f, "Black");
	{
		const FVector Lens(300, 350, 105);
		const FVector ScreenC(400, 1585, 210);
		USpotLightComponent* Proj = Spot(Lens, ScreenC, FLinearColor(0.85f, 0.9f, 1.f), 2600.f, 1800.f, 6.f, 9.f, true, false);
		Proj->SetVolumetricScatteringIntensity(2.f);
		AddFlicker(Proj, 22.f, 0.12f);
		// visible beam: cone, apex at the lens, base on the screen
		const FVector Axis = (Lens - ScreenC);
		const float Len = Axis.Size();
		const FRotator Rot = FRotationMatrix::MakeFromZX(Axis / Len, FVector(1, 0, 0)).Rotator();
		Deco(EDCShape::Cone, (Lens + ScreenC) * 0.5f, FVector(560, 230, Len), "Beam", Rot, false);
		for (int32 i = 0; i < 160; ++i)
		{
			const float T = FMath::Frac(i * 0.618034f);
			const FVector P = FMath::Lerp(Lens, ScreenC, T) + FVector(J(i, 1.f) * 250.f, 0, J(i + 33, 1.f) * 100.f) * T;
			Deco(EDCShape::Sphere, P, FVector(1.1f), "Dust", FRotator::ZeroRotator, false);
		}
	}

	// ---------------- Blueprint Table (front-left)
	BeginStation(EDCStationType::BlueprintTable, TEXT("Blueprint Table"), FVector(-300, -235, 0), FVector(78, 95, 80));
	for (int32 sx = -1; sx <= 1; sx += 2) for (int32 sy = -1; sy <= 1; sy += 2)
	{
		SPTube(FVector(sx * 46, sy * 62, 0), FVector(sx * 40, sy * 56, 90), 3.5f, "Iron");
	}
	SPTube(FVector(-40, -56, 30), FVector(-40, 56, 30), 2.5f, "Iron");
	SPTube(FVector(40, -56, 30), FVector(40, 56, 30), 2.5f, "Iron");
	SP(EDCShape::Cube, FVector(0, 0, 95), FVector(104, 144, 4), "Oak", FRotator(12, 0, 0));
	SP(EDCShape::Cube, FVector(-1, 0, 97.6f), FVector(92, 128, 0.6f), "Blueprint", FRotator(12, 0, 0));
	SP(EDCShape::Cube, FVector(4, -10, 98.6f), FVector(3.5f, 112, 0.8f), "Brass", FRotator(12, 0, 0));
	SP(EDCShape::Cube, FVector(-52, 0, 86), FVector(3, 144, 4), "Brass");
	SP(EDCShape::Cylinder, FVector(30, 82, 25), FVector(32, 32, 50), "Olive");
	for (int32 i = 0; i < 5; ++i)
	{
		SP(EDCShape::Cylinder, FVector(30 + J(i, 8), 82 + J(i + 4, 8), 58), FVector(7, 7, 74), (i % 2) ? FName("Paper") : FName("Blueprint"), FRotator(J(i + 2, 8), 0, J(i + 6, 8)));
	}
	SP(EDCShape::Cylinder, FVector(20, -40, 104), FVector(7, 7, 62), "Paper", FRotator(12, 0, 90));
	SP(EDCShape::Cylinder, FVector(10, 25, 103), FVector(6, 6, 50), "Blueprint", FRotator(12, 30, 90));
	SPTube(FVector(45, -70, 100), FVector(15, -62, 150), 2.f, "Iron");
	SPTube(FVector(15, -62, 150), FVector(-12, -42, 134), 2.f, "Iron");
	SP(EDCShape::Cone, FVector(-14, -40, 128), FVector(16, 16, 14), "Iron");
	SP(EDCShape::Sphere, FVector(-14, -40, 121), FVector(6), "Bulb", FRotator::ZeroRotator, false);
	SP(EDCShape::Cylinder, FVector(-75, 35, 55), FVector(32, 32, 5), "Leather");
	SPTube(FVector(-75, 35, 0), FVector(-75, 35, 53), 4.f, "Iron");
	Spot(FVector(-314, -275, 120), FVector(-300, -230, 80), Warm, 28.f, 450.f, 30.f, 62.f, false, true);

	// ---------------- room dressing: bookshelf, fence crate, coat rack
	CurGroup = DCGroups::Level1;
	{
		const float BX = 458.f, BY = -75.f;
		Deco(EDCShape::Cube, FVector(BX, BY, 106), FVector(34, 176, 212), "DarkWood");
		for (float Z : { 12.f, 62.f, 112.f, 162.f })
		{
			float Y = BY - 80.f; int32 k = (int32)Z;
			while (Y < BY + 76.f)
			{
				const float W = 4.f + FMath::Abs(J(k, 3.f)), H = 30.f + J(k + 1, 8.f);
				static const FName Books[] = { "BookRed", "BookGrn", "BookBlu", "BookTan" };
				Deco(EDCShape::Cube, FVector(BX - 18, Y + W * 0.5f, Z + 4 + H * 0.5f), FVector(26, W, H), Books[k % 4], FRotator(0, 0, (k % 9 == 0) ? 12.f : 0.f));
				Y += W + 0.6f; ++k;
			}
			Deco(EDCShape::Cube, FVector(BX - 16, BY, Z + 2), FVector(30, 172, 4), "DarkWood");
		}
	}
	// fence crate with gear
	Deco(EDCShape::Cube, FVector(-420, 290, 28), FVector(72, 72, 56), "Plank");
	for (int32 i = 0; i < 3; ++i) { Deco(EDCShape::Cube, FVector(-457, 290, 10 + i * 18.f), FVector(2, 74, 6), "DarkWood"); }
	Deco(EDCShape::Cube, FVector(-420, 290, 59), FVector(60, 24, 10), "Black", FRotator(0, 20, 0));
	Tube(FVector(-440, 270, 58), FVector(-400, 320, 60), 3.f, "Steel");
	Deco(EDCShape::Cylinder, FVector(-405, 268, 60), FVector(14, 14, 8), "Canvas");
	// coat rack + fedora by the door
	Tube(FVector(430, -350, 0), FVector(430, -350, 180), 3.f, "DarkWood");
	Deco(EDCShape::Cylinder, FVector(430, -350, 2), FVector(40, 40, 4), "DarkWood");
	Deco(EDCShape::Cylinder, FVector(436, -350, 184), FVector(30, 30, 3), "Black");
	Deco(EDCShape::Cylinder, FVector(436, -350, 190), FVector(18, 18, 10), "Black");
	Deco(EDCShape::Cube, FVector(436, -340, 130), FVector(14, 30, 70), "Canvas", FRotator(0, 0, 6));

	// ---------------- L2: safe, whisky cart, floor lamp
	CurGroup = DCGroups::Level2;
	Deco(EDCShape::Cube, FVector(440, 330, 40), FVector(64, 64, 80), "Iron");
	Deco(EDCShape::Cylinder, FVector(407, 330, 50), FVector(14, 14, 3), "Brass", FRotator(90, 0, 0));
	Deco(EDCShape::Cube, FVector(407, 345, 30), FVector(2, 4, 14), "Brass");
	Deco(EDCShape::Cube, FVector(-440, -120, 45), FVector(44, 80, 3), "Brass");
	Deco(EDCShape::Cube, FVector(-440, -120, 85), FVector(44, 80, 3), "Brass");
	for (int32 i = 0; i < 4; ++i) { Tube(FVector(-460 + (i % 2) * 40, -158 + (i / 2) * 76, 4), FVector(-460 + (i % 2) * 40, -158 + (i / 2) * 76, 88), 2, "Brass"); }
	for (int32 i = 0; i < 5; ++i) { Deco(EDCShape::Cylinder, FVector(-440 + J(i, 10), -145 + i * 12.f, 100), FVector(8, 8, 26), (i % 2) ? FName("Whisky") : FName("Bottle")); }
	Tube(FVector(-440, -340, 0), FVector(-440, -340, 160), 2.5f, "Brass");
	Deco(EDCShape::Cone, FVector(-440, -340, 165), FVector(38, 38, 26), "Cream", FRotator(180, 0, 0));
	Point(FVector(-440, -340, 160), Warm, 18.f, 600.f, false, true, 6.f);

	// ---------------- L3: ACTION neon over the job board + chandelier
	CurGroup = DCGroups::Level3;
	NeonText(TEXT("ACTION"), FVector(-136, 385, 280), FVector(-1, 0, 0), FVector(0, 0, 1), 10.f, 2.8f, "NeonCyan");
	Rect(FVector(-220, 360, 300), FVector(-220, 0, 120), FLinearColor(0.1f, 0.7f, 1.f), 20.f, 160.f, 30.f, 700.f, false, true);
	Tube(FVector(60, -20, 380), FVector(60, -20, 300), 1.2f, "Brass");
	for (int32 i = 0; i < 8; ++i)
	{
		const float A = i * PI / 4.f;
		const FVector P(60 + FMath::Cos(A) * 40.f, -20 + FMath::Sin(A) * 40.f, 290);
		Tube(FVector(60, -20, 300), P, 1.f, "Brass");
		Deco(EDCShape::Sphere, P + FVector(0, 0, 5), FVector(6), "Bulb", FRotator::ZeroRotator, false);
	}
	Point(FVector(60, -20, 280), Warm, 30.f, 900.f, false, true, 30.f);

	// ---------------- spawn zone + street door
	{
		ADCCriminalSpawnZone* Z = GetWorld()->SpawnActor<ADCCriminalSpawnZone>(ADCCriminalSpawnZone::StaticClass(), FTransform(CurRoom->GetActorTransform().TransformPosition(FVector(-170, 70, 50))));
		Z->RoomType = EDCRoomType::Office;
		Z->Zone->SetBoxExtent(FVector(90, 110, 50));
		Z->AttachToActor(CurRoom, FAttachmentTransformRules::KeepWorldTransform);

		ADCGarageDoor* D = GetWorld()->SpawnActor<ADCGarageDoor>(ADCGarageDoor::StaticClass(), FTransform(CurRoom->GetActorTransform().TransformPosition(FVector(490, -250, 0))));
		D->RoomType = EDCRoomType::Office;
		D->ExitPoint = FVector(160, 0, 0);
		Single(D, D->DoorPanel, EDCShape::Cube, FVector(0, 0, 112), FVector(8, 118, 222), M("DarkWood"));
		Single(D, D->DoorPanel, EDCShape::Cube, FVector(-1, 0, 175), FVector(9, 70, 50), M("Glass"));
		Single(D, D->DoorPanel, EDCShape::Sphere, FVector(-8, 44, 105), FVector(6), M("Brass"));
		D->AttachToActor(CurRoom, FAttachmentTransformRules::KeepWorldTransform);
	}
	CurGroup = DCGroups::Shell;
	NeonText(TEXT("EXIT"), FVector(474, -283, 240), FVector(0, 1, 0), FVector(0, 0, 1), 6.f, 1.6f, "ExitRed");
}

// =====================================================================================================================
// LOUNGE  (centre (-900,-900); x[-400,400] y[-500,500]; +X/+Y are partitions)
// =====================================================================================================================
void ADCHideoutBuilder::BuildLounge()
{
	CurRoom = GetRoom(EDCRoomType::Lounge);
	const FBox2D R(FVector2D(-400, -500), FVector2D(400, 500));

	CurGroup = DCGroups::Level1;
	// station: sofa, coffee table, ashtray, console radio
	BeginStation(EDCStationType::Lounge, TEXT("The Lounge"), FVector(255, -70, 0), FVector(140, 210, 60));
	{
		const FVector S(45, -80, 0); // sofa against +X partition, facing -X
		SP(EDCShape::Cube, S + FVector(0, 0, 24), FVector(86, 230, 30), "Leather");
		SP(EDCShape::Cube, S + FVector(-4, -55, 44), FVector(78, 108, 14), "Leather", FRotator(0, 0, 2));
		SP(EDCShape::Cube, S + FVector(-4, 55, 44), FVector(78, 108, 14), "Leather", FRotator(0, 0, -2));
		SP(EDCShape::Cube, S + FVector(34, 0, 70), FVector(22, 230, 56), "Leather", FRotator(-10, 0, 0));
		SP(EDCShape::Cylinder, S + FVector(0, -118, 48), FVector(86, 26, 26), "Leather", FRotator(90, 0, 0));
		SP(EDCShape::Cylinder, S + FVector(0, 118, 48), FVector(86, 26, 26), "Leather", FRotator(90, 0, 0));
		for (int32 i = 0; i < 4; ++i) { SP(EDCShape::Cylinder, S + FVector((i % 2) ? 34.f : -34.f, (i / 2) ? 100.f : -100.f, 4), FVector(6, 6, 8), "Brass"); }
		// coffee table + ashtray + glasses
		const FVector T(-75, -80, 0);
		SP(EDCShape::Cube, T + FVector(0, 0, 42), FVector(62, 120, 5), "Mahogany");
		SP(EDCShape::Cube, T + FVector(0, 0, 20), FVector(50, 100, 3), "Mahogany");
		for (int32 i = 0; i < 4; ++i) { SPTube(T + FVector((i % 2) ? 26.f : -26.f, (i / 2) ? 54.f : -54.f, 0), T + FVector((i % 2) ? 26.f : -26.f, (i / 2) ? 54.f : -54.f, 40), 3, "Brass"); }
		SP(EDCShape::Cylinder, T + FVector(5, 20, 46), FVector(16, 16, 3), "Chrome");
		SPTube(T + FVector(5, 25, 48), T + FVector(4, 33, 48.5f), 1.3f, "Paper");
		SP(EDCShape::Sphere, T + FVector(4, 33, 48.5f), FVector(1.6f), "Ember", FRotator::ZeroRotator, false);
		SP(EDCShape::Cylinder, T + FVector(-10, -30, 49), FVector(7, 7, 9), "Glass", FRotator::ZeroRotator, false);
		SP(EDCShape::Cube, T + FVector(10, -10, 45.5f), FVector(20, 28, 1.5f), "Paper", FRotator(0, 25, 0));
		for (int32 i = 0; i < 5; ++i)
		{
			const float Sz = 5.f + i * 3.f;
			Deco(EDCShape::Sphere, CurRoom->GetActorTransform().InverseTransformPosition(CurStation->GetActorLocation()) - FVector(0, 0, CurStationHalfZ) + T + FVector(5 + FMath::Sin(i * 1.1f) * 5.f, 33, 58 + i * 12.f), FVector(Sz, Sz, 14 + i * 4.f), "Smoke", FRotator::ZeroRotator, false);
		}
		// console radio
		const FVector Rd(80, 175, 0);
		SP(EDCShape::Cube, Rd + FVector(0, 0, 50), FVector(44, 70, 100), "Mahogany");
		SP(EDCShape::Cylinder, Rd + FVector(0, 0, 100), FVector(44, 70, 40), "Mahogany", FRotator(90, 0, 0));
		SP(EDCShape::Cube, Rd + FVector(-23, 0, 60), FVector(2, 52, 40), "Canvas");
		SP(EDCShape::Cube, Rd + FVector(-23, 0, 92), FVector(2, 40, 10), "Radio", FRotator::ZeroRotator, false);
		SP(EDCShape::Cylinder, Rd + FVector(-24, -16, 78), FVector(7, 7, 3), "Brass", FRotator(90, 0, 0));
		SP(EDCShape::Cylinder, Rd + FVector(-24, 16, 78), FVector(7, 7, 3), "Brass", FRotator(90, 0, 0));
	}
	Point(FVector(285, 105, 95), FLinearColor(1.f, 0.5f, 0.15f), 3.f, 220.f, false, true, 4.f); // radio dial glow

	// pool table + pendant (key light)
	{
		const FVector P(-130, -60, 0);
		Deco(EDCShape::Cube, P + FVector(0, 0, 70), FVector(150, 260, 16), "Mahogany");
		Deco(EDCShape::Cube, P + FVector(0, 0, 79), FVector(128, 238, 2), "Felt");
		for (int32 i = 0; i < 4; ++i) { Deco(EDCShape::Cube, P + FVector((i % 2) ? 55.f : -55.f, (i / 2) ? 105.f : -105.f, 31), FVector(18, 18, 62), "Mahogany"); }
		Deco(EDCShape::Cube, P + FVector(-71, 0, 83), FVector(8, 260, 6), "Mahogany");
		Deco(EDCShape::Cube, P + FVector(71, 0, 83), FVector(8, 260, 6), "Mahogany");
		Deco(EDCShape::Cube, P + FVector(0, -126, 83), FVector(150, 8, 6), "Mahogany");
		Deco(EDCShape::Cube, P + FVector(0, 126, 83), FVector(150, 8, 6), "Mahogany");
		static const FName Balls[] = { "Ball1", "Ball2", "Ball3", "BallW", "Ball2", "Ball1", "Ball3" };
		for (int32 i = 0; i < 7; ++i) { Deco(EDCShape::Sphere, P + FVector(J(i, 40), 40 + J(i + 20, 60), 83), FVector(5.5f), Balls[i]); }
		Tube(P + FVector(-30, -60, 82), P + FVector(20, 70, 84), 1.8f, "Oak");
		Tube(P + FVector(0, 0, 380), P + FVector(0, 0, 195), 1.2f, "Black");
		Deco(EDCShape::Cylinder, P + FVector(0, 0, 190), FVector(30, 30, 150), "LampGreen", FRotator(90, 90, 0));
		Deco(EDCShape::Cube, P + FVector(0, 0, 182), FVector(12, 130, 2), "Bulb", FRotator::ZeroRotator, false);
		Spot(P + FVector(0, 0, 178), P, Warm, 70.f, 650.f, 40.f, 72.f, true, true);
	}
	// Edison string across the room between two iron posts
	{
		const FVector A(330, -470, 250), B(-370, 470, 250);
		Tube(FVector(A.X, A.Y, 0), A + FVector(0, 0, 10), 5.f, "Iron");
		Tube(FVector(B.X, B.Y, 0), B + FVector(0, 0, 10), 5.f, "Iron");
		FVector Prev = A;
		for (int32 i = 1; i <= 16; ++i)
		{
			const float T = i / 16.f;
			const FVector P = FMath::Lerp(A, B, T) - FVector(0, 0, 55.f * 4.f * T * (1.f - T));
			Tube(Prev, P, 0.8f, "Black", false);
			if (i < 16) { Deco(EDCShape::Sphere, P - FVector(0, 0, 6), FVector(5.5f, 5.5f, 7.f), "Bulb", FRotator::ZeroRotator, false); }
			Prev = P;
		}
		for (float T : { 0.25f, 0.5f, 0.75f })
		{
			Point(FMath::Lerp(A, B, T) - FVector(0, 0, 55.f * 4.f * T * (1.f - T) + 12.f), Tungsten, 7.f, 500.f, false, true, 3.f);
		}
	}
	// rug, bunk (roster capacity grows with level)
	Deco(EDCShape::Cube, FVector(200, -80, 0.6f), FVector(260, 330, 1.2f), "CarpetGrn");
	auto Bunk = [&](float Y)
	{
		const float X = -280.f;
		for (int32 i = 0; i < 4; ++i) { Deco(EDCShape::Cube, FVector(X + ((i % 2) ? 95.f : -95.f), Y + ((i / 2) ? 40.f : -40.f), 85), FVector(6, 6, 170), "Iron"); }
		for (float Z : { 35.f, 125.f })
		{
			Deco(EDCShape::Cube, FVector(X, Y, Z), FVector(196, 86, 8), "Iron");
			Deco(EDCShape::Cube, FVector(X, Y, Z + 9), FVector(186, 80, 12), "Linen");
			Deco(EDCShape::Cube, FVector(X - 72, Y, Z + 19), FVector(30, 60, 10), "Cream");
			Deco(EDCShape::Cube, FVector(X + 25, Y, Z + 17), FVector(110, 82, 6), "Canvas", FRotator(0, 0, 2));
		}
	};
	Bunk(430.f);
	CurGroup = DCGroups::Level2;
	Bunk(320.f);
	// bar along the +Y partition
	Deco(EDCShape::Cube, FVector(130, 445, 52), FVector(330, 46, 104), "DarkWood");
	Deco(EDCShape::Cube, FVector(130, 440, 107), FVector(344, 60, 6), "Mahogany");
	Deco(EDCShape::Cube, FVector(130, 414, 15), FVector(330, 3, 3), "Brass");
	for (int32 i = 0; i < 9; ++i) { Deco(EDCShape::Cylinder, FVector(-10 + i * 34.f, 452 + J(i, 6), 124), FVector(8, 8, 28), (i % 3) ? FName("Bottle") : FName("Whisky")); }
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector St(20 + i * 75.f, 395, 0);
		Tube(St, St + FVector(0, 0, 70), 4, "Chrome");
		Deco(EDCShape::Cylinder, St + FVector(0, 0, 74), FVector(34, 34, 8), "Leather");
		Deco(EDCShape::Cylinder, St + FVector(0, 0, 2), FVector(30, 30, 4), "Chrome");
	}
	Point(FVector(130, 420, 140), Warm, 10.f, 500.f, false, true, 20.f);
	Deco(EDCShape::Cube, FVector(80, -80, 45), FVector(70, 70, 40), "LeatherBrn"); // armchair
	Deco(EDCShape::Cube, FVector(55, -80, 75), FVector(18, 72, 60), "LeatherBrn");
	CurGroup = DCGroups::Level3;
	Bunk(210.f);
	Deco(EDCShape::Cube, FVector(130, 492, 182), FVector(150, 6, 64), "Black");
	NeonText(TEXT("BAR"), FVector(160, 488, 160), FVector(-1, 0, 0), FVector(0, 0, 1), 14.f, 3.f, "NeonPink");
	Rect(FVector(130, 470, 190), FVector(130, 0, 60), FLinearColor(1.f, 0.2f, 0.55f), 30.f, 150.f, 40.f, 800.f, false, true);
	// jukebox
	Deco(EDCShape::Cube, FVector(350, 440, 70), FVector(50, 60, 140), "Mahogany");
	Deco(EDCShape::Cylinder, FVector(350, 440, 140), FVector(50, 60, 40), "Mahogany", FRotator(90, 0, 0));
	Tube(FVector(324, 412, 20), FVector(324, 412, 140), 3, "NeonAmber", false);
	Tube(FVector(324, 468, 20), FVector(324, 468, 140), 3, "NeonAmber", false);
	Point(FVector(310, 440, 100), FLinearColor(1.f, 0.5f, 0.1f), 6.f, 300.f, false, true, 10.f);

	LockedDressing(R, { {255, -80, 230, 90}, {-130, -60, 270, 95}, {80, 175, 80, 110}, {-280, 430, 200, 170} });
	BuildingDressing(R);

	ADCCriminalSpawnZone* Z = GetWorld()->SpawnActor<ADCCriminalSpawnZone>(ADCCriminalSpawnZone::StaticClass(), FTransform(CurRoom->GetActorTransform().TransformPosition(FVector(-240, -340, 50))));
	Z->RoomType = EDCRoomType::Lounge;
	Z->Zone->SetBoxExtent(FVector(120, 110, 50));
	Z->AttachToActor(CurRoom, FAttachmentTransformRules::KeepWorldTransform);
}

// =====================================================================================================================
// WORKSHOP  (centre (0,-900); x[-500,500] y[-500,500]; +X/+Y partitions)
// =====================================================================================================================
void ADCHideoutBuilder::BuildWorkshop()
{
	CurRoom = GetRoom(EDCRoomType::Workshop);
	const FBox2D R(FVector2D(-500, -500), FVector2D(500, 500));
	CurGroup = DCGroups::Level1;

	BeginStation(EDCStationType::Workbench, TEXT("Workbench"), FVector(-60, 420, 0), FVector(150, 75, 125));
	{
		SP(EDCShape::Cube, FVector(0, 0, 90), FVector(260, 84, 8), "Plank");
		for (int32 i = 0; i < 4; ++i) { SP(EDCShape::Cube, FVector((i % 2) ? 120.f : -120.f, (i / 2) ? 34.f : -34.f, 43), FVector(8, 8, 86), "Iron"); }
		SP(EDCShape::Cube, FVector(0, 0, 25), FVector(250, 76, 4), "Plank");
		// pegboard behind, rising above the partition
		SP(EDCShape::Cube, FVector(0, 62, 175), FVector(270, 3, 160), "Cork");
		SP(EDCShape::Cube, FVector(0, 63, 258), FVector(276, 4, 6), "Iron");
		SPTube(FVector(-136, 64, 0), FVector(-136, 64, 260), 4, "Iron");
		SPTube(FVector(136, 64, 0), FVector(136, 64, 260), 4, "Iron");
		// tools: hammers, wrenches, saw, crowbar
		for (int32 i = 0; i < 4; ++i)
		{
			const float X = -110 + i * 22.f;
			SPTube(FVector(X, 58, 130), FVector(X, 58, 162), 2.2f, "Oak");
			SP(EDCShape::Cube, FVector(X, 58, 166), FVector(12, 3, 4), "Steel");
		}
		for (int32 i = 0; i < 5; ++i) { SPTube(FVector(-10 + i * 12.f, 58, 140), FVector(-10 + i * 12.f, 58, 175 + i * 4.f), 2.f, "Chrome"); }
		SP(EDCShape::Cube, FVector(75, 58, 160), FVector(50, 1, 18), "Steel", FRotator(10, 0, 0));
		SPTube(FVector(110, 58, 125), FVector(118, 58, 200), 2.5f, "Pin");
		// gun case, duffel on bench, vise, task lamp
		SP(EDCShape::Cube, FVector(-60, -8, 100), FVector(80, 30, 12), "Black", FRotator(0, 8, 0));
		SP(EDCShape::Cube, FVector(-60, -8, 106.5f), FVector(4, 31, 2), "Brass", FRotator(0, 8, 0));
		SP(EDCShape::Cylinder, FVector(40, -10, 110), FVector(36, 36, 70), "Canvas", FRotator(0, 0, 90));
		SP(EDCShape::Cube, FVector(110, -20, 102), FVector(22, 16, 16), "Iron");
		SPTube(FVector(-130, 20, 94), FVector(-110, 0, 160), 2.f, "Iron");
		SPTube(FVector(-110, 0, 160), FVector(-80, -10, 150), 2.f, "Iron");
		SP(EDCShape::Cone, FVector(-78, -10, 144), FVector(18, 18, 14), "Hazard");
		SP(EDCShape::Sphere, FVector(-78, -10, 137), FVector(7), "Bulb", FRotator::ZeroRotator, false);
		// duffels on the floor
		SP(EDCShape::Cylinder, FVector(60, -60, 18), FVector(36, 36, 80), "Canvas", FRotator(0, 15, 90));
		SP(EDCShape::Cylinder, FVector(-20, -70, 16), FVector(32, 32, 70), "Olive", FRotator(0, -10, 90));
	}
	Spot(FVector(-138, 410, 138), FVector(-120, 410, 85), Warm, 40.f, 500.f, 30.f, 65.f, true, true);
	// fluorescent tube overhead
	Tube(FVector(-60, 300, 380), FVector(-60, 300, 285), 0.8f, "Black");
	Deco(EDCShape::Cube, FVector(-60, 300, 280), FVector(150, 14, 6), "Iron");
	Deco(EDCShape::Cylinder, FVector(-60, 300, 275), FVector(5, 5, 140), "BulbCool", FRotator(0, 0, 90) + FRotator(0, 90, 0), false);
	Rect(FVector(-60, 300, 270), FVector(-60, 300, 0), Cool, 35.f, 140.f, 10.f, 900.f, false, true);
	// shelving with boxes along +X partition, oil drums
	for (int32 s = 0; s < 2; ++s)
	{
		const float Y = -260 + s * 200.f;
		for (int32 i = 0; i < 4; ++i) { Deco(EDCShape::Cube, FVector(460 + ((i % 2) ? 20.f : -20.f), Y + ((i / 2) ? 85.f : -85.f), 90), FVector(4, 4, 180), "Galv"); }
		for (float Z : { 20.f, 80.f, 140.f })
		{
			Deco(EDCShape::Cube, FVector(460, Y, Z), FVector(46, 176, 3), "Galv");
			for (int32 b = 0; b < 3; ++b) { Deco(EDCShape::Cube, FVector(460, Y - 55 + b * 55.f + J(b + (int32)Z, 8), Z + 15), FVector(38, 40 + J(b, 8), 28), "Cream", FRotator(0, J(b + s, 6), 0)); }
		}
	}
	for (int32 i = 0; i < 3; ++i) { Deco(EDCShape::Cylinder, FVector(380 - i * 10.f, -420 + i * 62.f, 45), FVector(58, 58, 90), i == 1 ? FName("Hazard") : FName("Steel")); }

	CurGroup = DCGroups::Level2;
	// welding cart with gas bottles + anvil
	Deco(EDCShape::Cube, FVector(250, 250, 40), FVector(60, 90, 6), "Iron");
	Deco(EDCShape::Cylinder, FVector(250, 230, 80), FVector(24, 24, 140), "Pin");
	Deco(EDCShape::Cylinder, FVector(250, 270, 80), FVector(24, 24, 140), "Steel");
	Deco(EDCShape::Cube, FVector(100, 150, 30), FVector(60, 40, 60), "Iron");
	Deco(EDCShape::Cube, FVector(100, 150, 66), FVector(80, 26, 14), "Iron");
	Point(FVector(250, 250, 200), Tungsten, 10.f, 500.f, false, true, 4.f);
	CurGroup = DCGroups::Level3;
	NeonText(TEXT("TOOLS"), FVector(-5, 480, 270), FVector(-1, 0, 0), FVector(0, 0, 1), 9.f, 2.4f, "NeonAmber");
	// gun rack
	Deco(EDCShape::Cube, FVector(-380, 300, 90), FVector(20, 160, 180), "DarkWood");
	for (int32 i = 0; i < 5; ++i) { Tube(FVector(-368, 240 + i * 30.f, 20), FVector(-368, 240 + i * 30.f, 150), 4, "Black"); }
	Point(FVector(-330, 300, 220), Warm, 8.f, 400.f, false, true, 4.f);

	LockedDressing(R, { {-60, 420, 260, 110}, {460, -160, 200, 180}, {380, -360, 120, 95} });
	BuildingDressing(R);
}

// =====================================================================================================================
// GARAGE  (centre (900,-900); x[-400,400] y[-500,500]; +X exterior with roll-up door, +Y exterior)
// =====================================================================================================================
void ADCHideoutBuilder::BuildGarage()
{
	CurRoom = GetRoom(EDCRoomType::Garage);
	const FBox2D R(FVector2D(-400, -500), FVector2D(400, 500));

	CurGroup = DCGroups::Shell;
	Deco(EDCShape::Cube, FVector(372, 0, 305), FVector(34, 390, 30), "Iron"); // roll drum housing
	Tube(FVector(374, -184, 0), FVector(374, -184, 292), 6, "Iron");
	Tube(FVector(374, 184, 0), FVector(374, 184, 292), 6, "Iron");
	for (int32 i = 0; i < 6; ++i) { Deco(EDCShape::Cube, FVector(-60 + i * 4.f, -100 + i * 37.f, 0.4f), FVector(40 + i * 7.f, 30 + i * 5.f, 0.8f), "Black", FRotator(0, i * 31.f, 0)); }

	CurGroup = DCGroups::Level1;
	BeginStation(EDCStationType::GarageDoor, TEXT("Garage"), FVector(30, 0, 0), FVector(240, 105, 85));
	{
		// 1940s getaway sedan, nose towards the door (+X)
		SP(EDCShape::Cube, FVector(0, 0, 52), FVector(430, 170, 46), "CarPaint");
		SP(EDCShape::Cube, FVector(-30, 0, 104), FVector(200, 150, 58), "CarPaint", FRotator::ZeroRotator);
		SP(EDCShape::Cube, FVector(-30, 0, 104), FVector(188, 152, 40), "Glass", FRotator::ZeroRotator, false);
		SP(EDCShape::Cube, FVector(-30, 0, 135), FVector(176, 140, 6), "CarPaint");
		SP(EDCShape::Cube, FVector(140, 0, 78), FVector(150, 120, 14), "CarPaint", FRotator(-4, 0, 0));
		for (int32 i = 0; i < 4; ++i)
		{
			const float X = (i % 2) ? 135.f : -140.f, Y = (i / 2) ? 78.f : -78.f;
			SP(EDCShape::Cylinder, FVector(X, Y, 33), FVector(66, 66, 26), "Rubber", FRotator(0, 0, 90));
			SP(EDCShape::Cylinder, FVector(X, Y * 1.14f, 33), FVector(34, 34, 4), "Chrome", FRotator(0, 0, 90));
			SP(EDCShape::Sphere, FVector(X, Y * 0.98f, 58), FVector(100, 44, 50), "CarPaint");
		}
		SP(EDCShape::Cube, FVector(218, 0, 40), FVector(8, 176, 10), "Chrome");
		SP(EDCShape::Cube, FVector(-218, 0, 40), FVector(8, 176, 10), "Chrome");
		SP(EDCShape::Cube, FVector(214, 0, 66), FVector(4, 50, 26), "Chrome");
		SP(EDCShape::Sphere, FVector(205, -62, 70), FVector(18, 18, 18), "Head", FRotator::ZeroRotator, false);
		SP(EDCShape::Sphere, FVector(205, 62, 70), FVector(18, 18, 18), "Head", FRotator::ZeroRotator, false);
		SP(EDCShape::Sphere, FVector(-214, -70, 66), FVector(8, 12, 12), "Tail", FRotator::ZeroRotator, false);
		SP(EDCShape::Sphere, FVector(-214, 70, 66), FVector(8, 12, 12), "Tail", FRotator::ZeroRotator, false);
		SP(EDCShape::Cube, FVector(0, -86, 52), FVector(260, 2, 3), "Chrome");
		SP(EDCShape::Cube, FVector(0, 86, 52), FVector(260, 2, 3), "Chrome");
	}
	Point(FVector(-215, 0, 66), FLinearColor(1.f, 0.05f, 0.03f), 4.f, 260.f, false, true, 4.f);
	// caged bulb (key)
	Tube(FVector(0, 0, 380), FVector(0, 0, 270), 1.2f, "Black");
	Deco(EDCShape::Sphere, FVector(0, 0, 262), FVector(14), "Bulb", FRotator::ZeroRotator, false);
	Deco(EDCShape::Cone, FVector(0, 0, 272), FVector(44, 44, 14), "Iron");
	Spot(FVector(0, 0, 258), FVector(0, 0, 0), Tungsten, 60.f, 800.f, 45.f, 80.f, true, true);
	// tyre stack, jerry cans, tool chest
	for (int32 i = 0; i < 4; ++i) { Deco(EDCShape::Cylinder, FVector(-320, -400, 12 + i * 22.f), FVector(64, 64, 22), "Rubber"); }
	for (int32 i = 0; i < 3; ++i) { Deco(EDCShape::Cube, FVector(-330 + i * 32.f, 430, 22), FVector(24, 14, 44), "Pin"); }
	CurGroup = DCGroups::Level2;
	Deco(EDCShape::Cube, FVector(-300, 400, 50), FVector(60, 120, 100), "Pin");
	for (int32 i = 0; i < 4; ++i) { Deco(EDCShape::Cube, FVector(-331, 400, 25 + i * 22.f), FVector(2, 100, 2), "Chrome"); }
	Deco(EDCShape::Cube, FVector(-300, -150, 15), FVector(50, 160, 30), "Iron"); // car ramp
	Point(FVector(-300, 400, 200), Tungsten, 10.f, 500.f, false, true, 4.f);
	CurGroup = DCGroups::Level3;
	// vintage gas pump + neon EXIT over the door
	Deco(EDCShape::Cube, FVector(250, 420, 80), FVector(50, 44, 160), "Pin");
	Deco(EDCShape::Sphere, FVector(250, 420, 180), FVector(36), "BulbDim", FRotator::ZeroRotator, false);
	NeonText(TEXT("EXIT"), FVector(374, -50, 340), FVector(0, 1, 0), FVector(0, 0, 1), 9.f, 2.4f, "NeonRed");
	Point(FVector(340, 0, 350), FLinearColor(1.f, 0.06f, 0.04f), 12.f, 600.f, false, true, 10.f);

	// roll-up door actor (E animates DoorPanel upwards)
	{
		ADCGarageDoor* D = GetWorld()->SpawnActor<ADCGarageDoor>(ADCGarageDoor::StaticClass(), FTransform(CurRoom->GetActorTransform().TransformPosition(FVector(390, 0, 0))));
		D->RoomType = EDCRoomType::Garage;
		D->ExitPoint = FVector(320, 0, 0);
		Single(D, D->DoorPanel, EDCShape::Cube, FVector(0, 0, 145), FVector(8, 360, 290), M("Corrugated"));
		Single(D, D->DoorPanel, EDCShape::Cube, FVector(-5, 0, 6), FVector(6, 362, 10), M("Iron"));
		Single(D, D->DoorPanel, EDCShape::Cube, FVector(-7, 0, 40), FVector(4, 30, 6), M("Chrome"));
		D->AttachToActor(CurRoom, FAttachmentTransformRules::KeepWorldTransform);
	}

	LockedDressing(R, { {30, 0, 440, 140}, {-300, 400, 100, 100} });
	BuildingDressing(R);
}

// =====================================================================================================================
// SCREENING ROOM  (centre (400,1000); x[-900,900] y[-600,600]; screen on the +Y wall)
// =====================================================================================================================
void ADCHideoutBuilder::BuildScreening()
{
	CurRoom = GetRoom(EDCRoomType::ScreeningRoom);
	const FBox2D R(FVector2D(-900, -600), FVector2D(900, 600));

	CurGroup = DCGroups::Shell;
	// stage, black masking and curtains
	Deco(EDCShape::Cube, FVector(0, 530, 20), FVector(1000, 120, 40), "DarkWood");
	Deco(EDCShape::Cube, FVector(0, 470, 39), FVector(1000, 3, 3), "Trim");
	Deco(EDCShape::Cube, FVector(0, 584, 215), FVector(840, 6, 360), "Black");
	for (int32 s = -1; s <= 1; s += 2)
	{
		for (int32 i = 0; i < 7; ++i) { Deco(EDCShape::Cylinder, FVector(s * (430 + i * 17.f), 560 - (i % 2) * 8.f, 210), FVector(24, 24, 340), "Velvet"); }
	}
	Deco(EDCShape::Cube, FVector(0, 560, 362), FVector(1100, 40, 36), "Velvet");
	Deco(EDCShape::Cube, FVector(0, 540, 345), FVector(1100, 2, 4), "Trim");

	CurGroup = DCGroups::Level1;
	BeginStation(EDCStationType::ScreeningRoom, TEXT("Screening Room"), FVector(0, 545, 40), FVector(390, 50, 160));
	SP(EDCShape::Cube, FVector(0, 37, 170), FVector(740, 2, 300), "Screen", FRotator::ZeroRotator, false);
	SP(EDCShape::Cube, FVector(0, 36, 323), FVector(760, 4, 6), "Brass");
	SP(EDCShape::Cube, FVector(0, 36, 17), FVector(760, 4, 6), "Brass");
	Rect(FVector(0, 560, 210), FVector(0, -400, 120), FLinearColor(0.8f, 0.85f, 1.f), 22.f, 700.f, 280.f, 1800.f, true, true);
	// seats (velvet), raked rows on risers
	auto Row = [&](float Y)
	{
		const float Rise = (330.f - Y) / 85.f * 12.f;
		if (Rise > 1.f) { Deco(EDCShape::Cube, FVector(0, Y - 10, Rise * 0.5f), FVector(1240, 84, Rise), "CarpetRed"); }
		for (int32 s = -1; s <= 1; s += 2)
		{
			for (int32 k = 0; k < 9; ++k)
			{
				const float X = s * (100.f + k * 60.f);
				Deco(EDCShape::Cube, FVector(X, Y, Rise + 42), FVector(50, 44, 10), "Velvet");
				Deco(EDCShape::Cube, FVector(X, Y - 26, Rise + 72), FVector(52, 9, 58), "Velvet", FRotator(-10, 0, 0));
				Deco(EDCShape::Cube, FVector(X + 30, Y - 8, Rise + 30), FVector(6, 46, 58), "Iron");
			}
			Deco(EDCShape::Cube, FVector(s * 76.f, Y - 8, Rise + 30), FVector(6, 46, 58), "Iron");
		}
		Deco(EDCShape::Sphere, FVector(-66, Y + 25, Rise + 3), FVector(4), "Marquee", FRotator::ZeroRotator, false);
		Deco(EDCShape::Sphere, FVector(66, Y + 25, Rise + 3), FVector(4), "Marquee", FRotator::ZeroRotator, false);
	};
	Row(330.f); Row(245.f); Row(160.f);
	AddLocalFog(CurRoom, DCGroups::Level1, FVector(-120, -60, 200), FVector(700, 650, 220), 0.12f);
	// art-deco sconces on the +X wall
	auto Sconce = [&](float Y)
	{
		Deco(EDCShape::Cone, FVector(872, Y, 240), FVector(30, 40, 40), "Brass", FRotator(180, 0, 0));
		Deco(EDCShape::Sphere, FVector(870, Y, 252), FVector(10), "Bulb", FRotator::ZeroRotator, false);
		Point(FVector(860, Y, 262), Warm, 10.f, 450.f, false, true, 6.f);
	};
	Sconce(-200.f); Sconce(250.f);
	CurGroup = DCGroups::Level2;
	Row(75.f); Row(-10.f);
	Sconce(25.f);
	// popcorn / concessions cart
	Deco(EDCShape::Cube, FVector(700, -380, 60), FVector(80, 120, 120), "Pin");
	Deco(EDCShape::Cube, FVector(700, -380, 150), FVector(70, 110, 60), "Glass", FRotator::ZeroRotator, false);
	Deco(EDCShape::Cube, FVector(700, -380, 125), FVector(64, 104, 12), "Ball1");
	Point(FVector(700, -380, 160), Warm, 6.f, 300.f, false, true, 6.f);
	CurGroup = DCGroups::Level3;
	Row(-95.f);
	// marquee bulbs round the screen
	for (int32 i = 0; i < 40; ++i)
	{
		const float X = -380.f + i * 19.5f;
		Deco(EDCShape::Sphere, FVector(X, 575, 378), FVector(6), "Marquee", FRotator::ZeroRotator, false);
		if (i % 4 == 0) { Deco(EDCShape::Sphere, FVector(-392, 575, 60 + i * 7.5f), FVector(6), "Marquee", FRotator::ZeroRotator, false); Deco(EDCShape::Sphere, FVector(392, 575, 60 + i * 7.5f), FVector(6), "Marquee", FRotator::ZeroRotator, false); }
	}
	Rect(FVector(0, 540, 380), FVector(0, 0, 0), Warm, 12.f, 760.f, 20.f, 1400.f, false, true);

	LockedDressing(R, { {0, 520, 760, 330}, {-330, 230, 520, 110}, {330, 230, 520, 110}, {-330, 60, 520, 120}, {330, 60, 520, 120} });
	BuildingDressing(R);
}

// =====================================================================================================================
// CLINIC  (centre (-900,1300); x[-400,400] y[-300,300]; +Y exterior)
// =====================================================================================================================
void ADCHideoutBuilder::BuildClinic()
{
	CurRoom = GetRoom(EDCRoomType::Clinic);
	const FBox2D R(FVector2D(-400, -300), FVector2D(400, 300));
	CurGroup = DCGroups::Level1;
	BeginStation(EDCStationType::Clinic, TEXT("Back-Alley Clinic"), FVector(80, 80, 0), FVector(115, 55, 100));
	{
		SP(EDCShape::Cube, FVector(0, 0, 78), FVector(200, 72, 6), "Chrome");
		SP(EDCShape::Cube, FVector(0, 0, 87), FVector(190, 66, 12), "Linen");
		SP(EDCShape::Cube, FVector(-78, 0, 98), FVector(30, 50, 10), "Linen", FRotator(8, 0, 0));
		SP(EDCShape::Cube, FVector(30, 0, 95), FVector(120, 70, 4), "Cloth", FRotator(0, 0, 1));
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector L((i % 2) ? 90.f : -90.f, (i / 2) ? 30.f : -30.f, 0);
			SPTube(L + FVector(0, 0, 8), L + FVector(0, 0, 76), 3, "Chrome");
			SP(EDCShape::Sphere, L + FVector(0, 0, 5), FVector(9), "Rubber");
		}
		// IV stand
		SPTube(FVector(-100, -45, 0), FVector(-100, -45, 190), 2.5f, "Chrome");
		SP(EDCShape::Cube, FVector(-100, -45, 175), FVector(10, 4, 22), "Glass", FRotator::ZeroRotator, false);
		SPTube(FVector(-100, -45, 164), FVector(-60, -20, 95), 0.6f, "Glass");
		SP(EDCShape::Cylinder, FVector(-100, -45, 2), FVector(40, 40, 3), "Chrome");
		// tray with instruments
		SP(EDCShape::Cube, FVector(100, -55, 90), FVector(40, 30, 2), "Chrome");
		SPTube(FVector(100, -55, 0), FVector(100, -55, 89), 2, "Chrome");
		for (int32 i = 0; i < 4; ++i) { SPTube(FVector(88 + i * 7.f, -62, 91.5f), FVector(90 + i * 7.f, -48, 91.5f), 0.8f, "Chrome"); }
	}
	// surgical lamp
	Tube(FVector(80, 80, 380), FVector(80, 80, 232), 3, "Enamel");
	Deco(EDCShape::Sphere, FVector(80, 80, 222), FVector(70, 70, 34), "Enamel");
	Deco(EDCShape::Cylinder, FVector(80, 80, 207), FVector(52, 52, 2), "BulbCool", FRotator::ZeroRotator, false);
	Spot(FVector(80, 80, 200), FVector(80, 80, 0), FLinearColor(0.85f, 0.95f, 1.f), 90.f, 600.f, 25.f, 50.f, true, true);
	// medicine cabinet on the +Y wall
	Deco(EDCShape::Cube, FVector(-200, 270, 160), FVector(140, 24, 160), "Enamel");
	Deco(EDCShape::Cube, FVector(-200, 257, 160), FVector(124, 2, 144), "Glass", FRotator::ZeroRotator, false);
	for (int32 i = 0; i < 9; ++i) { Deco(EDCShape::Cylinder, FVector(-250 + (i % 3) * 45.f, 268, 105 + (i / 3) * 48.f), FVector(10, 10, 20), (i % 2) ? FName("Whisky") : FName("Bottle")); }
	Point(FVector(-200, 200, 250), Cool, 8.f, 500.f, false, true, 10.f);
	CurGroup = DCGroups::Level2;
	for (int32 i = 0; i < 3; ++i)
	{
		const FVector P(-120 + i * 10.f, -120 + i * 62.f, 0);
		Deco(EDCShape::Cube, P + FVector(0, 0, 95), FVector(3, 58, 150), "Linen", FRotator(0, -20 + i * 20.f, 0));
		Tube(P + FVector(0, -30, 0), P + FVector(0, -30, 172), 2, "Chrome");
	}
	Deco(EDCShape::Cube, FVector(330, -200, 50), FVector(60, 100, 100), "Enamel");
	CurGroup = DCGroups::Level3;
	NeonText(TEXT("+"), FVector(-80, 286, 230), FVector(-1, 0, 0), FVector(0, 0, 1), 20.f, 5.f, "NeonGreen");
	Point(FVector(-100, 250, 260), FLinearColor(0.1f, 1.f, 0.3f), 10.f, 600.f, false, true, 10.f);

	LockedDressing(R, { {80, 80, 220, 110}, {-200, 260, 150, 200} });
	BuildingDressing(R);
}

// =====================================================================================================================
// SERVER CLOSET  (centre (-900,0); x[-400,400] y[-400,400])
// =====================================================================================================================
void ADCHideoutBuilder::BuildServerCloset()
{
	CurRoom = GetRoom(EDCRoomType::ServerCloset);
	const FBox2D R(FVector2D(-400, -400), FVector2D(400, 400));
	CurGroup = DCGroups::Level1;
	auto RackAt = [&](const FVector& P, int32 Seed, bool bStation)
	{
		static const FName Leds[] = { "LedG", "LedA", "LedG", "LedR", "LedG" };
		auto Box = [&](const FVector& L, const FVector& S, FName M, bool bShadow)
		{
			if (bStation) { SP(EDCShape::Cube, L, S, M, FRotator::ZeroRotator, bShadow); }
			else { Deco(EDCShape::Cube, L, S, M, FRotator::ZeroRotator, bShadow); }
		};
		Box(P + FVector(0, 0, 100), FVector(62, 76, 200), "Rack", true);
		for (int32 u = 0; u < 9; ++u)
		{
			Box(P + FVector(-31.5f, 0, 22 + u * 20.f), FVector(1, 68, 16), "Iron", true);
			for (int32 l = 0; l < 3; ++l) { Box(P + FVector(-32.5f, -26 + l * 6.f, 22 + u * 20.f), FVector(1, 2.5f, 2.5f), Leds[(Seed + u * 3 + l) % 5], false); }
		}
	};
	BeginStation(EDCStationType::ServerCloset, TEXT("Server Closet"), FVector(200, 0, 0), FVector(50, 130, 105));
	RackAt(FVector(0, -85, 0), 1, true);
	RackAt(FVector(0, 0, 0), 2, true);
	RackAt(FVector(0, 85, 0), 3, true);
	SPTube(FVector(0, -85, 200), FVector(-20, -40, 230), 3, "Rubber");
	SPTube(FVector(-20, -40, 230), FVector(-10, 60, 225), 3, "Rubber");
	// operator desk with CRT
	Deco(EDCShape::Cube, FVector(-150, 280, 72), FVector(120, 70, 5), "Olive");
	Deco(EDCShape::Cube, FVector(-150, 280, 35), FVector(110, 60, 70), "Olive");
	Deco(EDCShape::Cube, FVector(-140, 290, 102), FVector(44, 40, 40), "Enamel");
	Deco(EDCShape::Cube, FVector(-163, 290, 104), FVector(2, 32, 28), "Monitor", FRotator::ZeroRotator, false);
	Point(FVector(-185, 290, 105), FLinearColor(0.2f, 1.f, 0.5f), 4.f, 300.f, false, true, 8.f);
	// cable trays and bundles on the floor
	for (int32 i = 0; i < 5; ++i) { Tube(FVector(170, -120 + i * 60.f, 3), FVector(-100 + J(i, 30), -380 + i * 10.f, 3), 4, "Rubber"); }
	Rect(FVector(60, 0, 300), FVector(60, 0, 0), FLinearColor(0.35f, 0.85f, 0.9f), 30.f, 140.f, 30.f, 700.f, false, true);
	CurGroup = DCGroups::Level2;
	RackAt(FVector(200, -270, 0), 4, false);
	RackAt(FVector(200, 270, 0), 5, false);
	CurGroup = DCGroups::Level3;
	NeonText(TEXT("INTEL"), FVector(-160, 380, 160), FVector(-1, 0, 0), FVector(0, 0, 1), 9.f, 2.4f, "NeonGreen");
	Deco(EDCShape::Cube, FVector(-215, 392, 175), FVector(160, 6, 50), "Black");
	Point(FVector(-215, 330, 180), FLinearColor(0.1f, 1.f, 0.3f), 8.f, 450.f, false, true, 10.f);

	LockedDressing(R, { {200, 0, 120, 210}, {-150, 280, 130, 110} });
	BuildingDressing(R);
}

// =====================================================================================================================
// LAWYER'S OFFICE  (centre (-900,700); x[-400,400] y[-300,300])
// =====================================================================================================================
void ADCHideoutBuilder::BuildLawyer()
{
	CurRoom = GetRoom(EDCRoomType::LawyerOffice);
	const FBox2D R(FVector2D(-400, -300), FVector2D(400, 300));
	CurGroup = DCGroups::Level1;
	BeginStation(EDCStationType::Lawyer, TEXT("Lawyer's Office"), FVector(150, 30, 0), FVector(120, 200, 75));
	{
		SP(EDCShape::Cube, FVector(0, 0, 74), FVector(90, 170, 5), "DarkWood");
		SP(EDCShape::Cube, FVector(0, -60, 36), FVector(86, 46, 72), "DarkWood");
		SP(EDCShape::Cube, FVector(0, 60, 36), FVector(86, 46, 72), "DarkWood");
		SP(EDCShape::Cube, FVector(0, 0, 77), FVector(60, 110, 1), "LeatherGrn");
		// typewriter
		SP(EDCShape::Cube, FVector(-8, -10, 84), FVector(30, 40, 12), "Black", FRotator(0, -10, 0));
		SP(EDCShape::Cube, FVector(-16, -10, 82), FVector(14, 36, 6), "Black", FRotator(8, -10, 0));
		SP(EDCShape::Cylinder, FVector(4, -10, 92), FVector(6, 6, 44), "Rubber", FRotator(0, -10, 90));
		SP(EDCShape::Cube, FVector(6, -10, 102), FVector(1, 28, 22), "Paper", FRotator(-10, -10, 0));
		for (int32 i = 0; i < 12; ++i) { SP(EDCShape::Cylinder, FVector(-18 + (i / 6) * 6.f, -26 + (i % 6) * 6.5f, 86 + (i / 6) * 2.f), FVector(3, 3, 1), "Cream", FRotator(0, -10, 0)); }
		// lamp
		SP(EDCShape::Cylinder, FVector(10, 60, 78), FVector(14, 14, 3), "Brass");
		SPTube(FVector(10, 60, 78), FVector(4, 55, 118), 1.6f, "Brass");
		SP(EDCShape::Cone, FVector(0, 52, 116), FVector(20, 20, 14), "Brass");
		SP(EDCShape::Sphere, FVector(0, 52, 110), FVector(7), "Bulb", FRotator::ZeroRotator, false);
		// folders
		for (int32 i = 0; i < 4; ++i) { SP(EDCShape::Cube, FVector(15, 25, 78 + i * 2.2f), FVector(26, 34, 2), (i % 2) ? FName("Cream") : FName("Plank"), FRotator(0, J(i, 12), 0)); }
		// chair
		SP(EDCShape::Cube, FVector(70, 0, 46), FVector(50, 54, 10), "LeatherGrn");
		SP(EDCShape::Cube, FVector(96, 0, 85), FVector(10, 56, 70), "LeatherGrn");
		SPTube(FVector(70, 0, 0), FVector(70, 0, 42), 6, "Brass");
		// filing cabinets along +Y partition
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector C(-80 + i * 52.f, 225, 0);
			SP(EDCShape::Cube, C + FVector(0, 0, 66), FVector(48, 60, 132), "Olive");
			for (int32 d = 0; d < 4; ++d)
			{
				SP(EDCShape::Cube, C + FVector(0, -30.5f, 16 + d * 32.f), FVector(44, 1, 28), "Olive");
				SP(EDCShape::Cube, C + FVector(0, -32, 24 + d * 32.f), FVector(12, 2, 2.5f), "Chrome");
			}
		}
		SP(EDCShape::Cube, FVector(-30, 225, 136), FVector(40, 30, 8), "Paper", FRotator(0, 10, 0));
	}
	Spot(FVector(150, 82, 108), FVector(140, 40, 0), Warm, 45.f, 550.f, 30.f, 66.f, true, true);
	Point(FVector(-100, -50, 300), Tungsten, 8.f, 700.f, false, true, 10.f);
	// clients' chairs
	for (int32 i = 0; i < 2; ++i)
	{
		const FVector C(30, -40 + i * 110.f, 0);
		Deco(EDCShape::Cube, C + FVector(0, 0, 45), FVector(50, 50, 10), "LeatherBrn");
		Deco(EDCShape::Cube, C + FVector(-24, 0, 75), FVector(8, 52, 58), "LeatherBrn");
		for (int32 l = 0; l < 4; ++l) { Tube(C + FVector((l % 2) ? 20.f : -20.f, (l / 2) ? 20.f : -20.f, 0), C + FVector((l % 2) ? 20.f : -20.f, (l / 2) ? 20.f : -20.f, 40), 3, "DarkWood"); }
	}
	CurGroup = DCGroups::Level2;
	// law books along the +X partition
	Deco(EDCShape::Cube, FVector(375, -150, 62), FVector(30, 200, 124), "DarkWood");
	for (int32 s = 0; s < 3; ++s)
	{
		float Y = -245.f; int32 k = s * 40;
		while (Y < -55.f)
		{
			const float W = 5.f + FMath::Abs(J(k, 2.f));
			static const FName Books[] = { "BookRed", "BookTan", "BookGrn" };
			Deco(EDCShape::Cube, FVector(358, Y + W * 0.5f, 18 + s * 38.f + 15), FVector(20, W, 28 + J(k + 1, 3)), Books[k % 3]);
			Y += W + 0.5f; ++k;
		}
	}
	Deco(EDCShape::Cube, FVector(-300, 150, 60), FVector(90, 90, 120), "Plank"); // evidence boxes
	Deco(EDCShape::Cube, FVector(-300, 150, 132), FVector(70, 70, 24), "Cream", FRotator(0, 12, 0));
	CurGroup = DCGroups::Level3;
	// scales of justice + LAW neon
	Deco(EDCShape::Cylinder, FVector(-300, 150, 147), FVector(14, 14, 4), "Brass");
	Tube(FVector(-300, 150, 147), FVector(-300, 150, 190), 1.5f, "Brass");
	Tube(FVector(-300, 125, 188), FVector(-300, 175, 188), 1.5f, "Brass");
	Deco(EDCShape::Cylinder, FVector(-300, 125, 170), FVector(16, 16, 2), "Brass");
	Deco(EDCShape::Cylinder, FVector(-300, 175, 170), FVector(16, 16, 2), "Brass");
	Deco(EDCShape::Cube, FVector(150, 292, 175), FVector(130, 6, 56), "Black");
	NeonText(TEXT("LAW"), FVector(185, 288, 158), FVector(-1, 0, 0), FVector(0, 0, 1), 12.f, 2.8f, "NeonAmber");
	Point(FVector(150, 250, 180), FLinearColor(1.f, 0.5f, 0.1f), 8.f, 450.f, false, true, 10.f);

	LockedDressing(R, { {150, 30, 140, 90}, {-30, 225, 170, 140} });
	BuildingDressing(R);
}
