// Director's Cut — per-room dressing for the warehouse hideout (Agent D). All coordinates are room-local cm
// (room actor sits at the centre of its floor rect, floor top at z=0). Camera looks from -X-Y, so tall props are
// pushed towards the +X/+Y walls and silhouettes face -X/-Y. Station props use the kit with bKitOnStation = true.
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
#include "Engine/World.h"

namespace
{
	// deterministic jitter
	float J(int32 I, float Amp) { return (FMath::Frac(FMath::Sin(I * 12.9898f + 4.1f) * 43758.5453f) * 2.f - 1.f) * Amp; }
	const FLinearColor Warm(1.f, 0.7f, 0.45f);    // ~3000K LED
	const FLinearColor Neutral(1.f, 0.9f, 0.8f);  // ~4000K
	const FLinearColor Cool(0.82f, 0.9f, 1.f);    // ~5500K
	const FLinearColor ScreenBlue(0.45f, 0.65f, 1.f);
	FRotator Compose(float Yaw, const FRotator& Local) { return (FRotator(0, Yaw, 0).Quaternion() * Local.Quaternion()).Rotator(); }
}

static void SpawnZoneAt(UWorld* W, ADCRoomModule* Room, const FVector& Local, const FVector& Extent)
{
	ADCCriminalSpawnZone* Z = W->SpawnActor<ADCCriminalSpawnZone>(ADCCriminalSpawnZone::StaticClass(), FTransform(Room->GetActorTransform().TransformPosition(Local)));
	Z->RoomType = Room->RoomType;
	Z->Zone->SetBoxExtent(Extent);
	Z->AttachToActor(Room, FAttachmentTransformRules::KeepWorldTransform);
}

// =====================================================================================================================
// OFFICE  (local == world; x[-500,500] y[-400,400]; +X exterior wall: windows + street door to the yard; +Y tall wall)
// =====================================================================================================================
void ADCHideoutBuilder::BuildOffice()
{
	CurRoom = GetRoom(EDCRoomType::Office);

	// ---------------- shell extras: door frame, exit sign, sun shaft volume + motes
	CurGroup = DCGroups::Shell;
	const float WX = 478.f;
	Deco(EDCShape::Cube, FVector(WX - 2, -250, 229), FVector(10, 136, 8), "SteelBlack");
	Deco(EDCShape::Cube, FVector(WX - 2, -314, 112), FVector(10, 8, 226), "SteelBlack");
	Deco(EDCShape::Cube, FVector(WX - 2, -186, 112), FVector(10, 8, 226), "SteelBlack");
	Deco(EDCShape::Cube, FVector(WX - 6, -250, 250), FVector(4, 34, 14), "PlasticWhite");
	Deco(EDCShape::Cube, FVector(WX - 8.5f, -250, 250), FVector(1, 30, 10), "ExitGreen", FRotator::ZeroRotator, false);
	ShaftFog(FVector(100, 0, 170), FVector(400, 380, 190), 0.3f);
	SunDust(FVector(150, 80, 330), FVector(-150, -150, 20), 150, 100.f);

	CurGroup = DCGroups::Level1;
	// concrete floor with a worn dark rug under the desk zone
	Deco(EDCShape::Cube, FVector(60, -30, 0.6f), FVector(320, 420, 1.2f), "Fabric");
	Pendant(FVector(40, -90, 255), true, true, 45.f);
	Pendant(FVector(40, 60, 255), true, true, 30.f);
	LedBatten(FVector(-300, -230, 300), 150.f, 0.f, true, 30.f);

	// ---------------- Desk: steel desk, laptop, burner phones, cash counter
	BeginStation(EDCStationType::Desk, TEXT("Desk"), FVector(40, -30, 0), FVector(80, 130, 75));
	bKitOnStation = true;
	{
		const FVector D(40, -30, 0);
		K(EDCShape::Cube, D + FVector(0, 0, 75), FVector(100, 220, 4), "PaintGrey");
		K(EDCShape::Cube, D + FVector(0, 0, 72), FVector(96, 216, 3), "SteelBlack");
		for (int32 i = 0; i < 2; ++i)
		{
			const float Y = i ? 100.f : -100.f;
			K(EDCShape::Cube, D + FVector(0, Y, 36), FVector(90, 6, 72), "SteelBlack");
		}
		K(EDCShape::Cube, D + FVector(0, -62, 36), FVector(84, 56, 68), "PaintGrey");                 // drawer pedestal
		for (int32 i = 0; i < 3; ++i) { K(EDCShape::Cube, D + FVector(43, -62, 14 + i * 22.f), FVector(1, 40, 2), "Alu"); }
		K(EDCShape::Cube, D + FVector(-40, 0, 42), FVector(4, 190, 50), "SteelBlack");                 // modesty panel
		// laptop (boss side is +X)
		K(EDCShape::Cube, D + FVector(18, 10, 77.8f), FVector(24, 34, 1.6f), "Alu", FRotator(0, 0, 0));
		K(EDCShape::Cube, D + FVector(6, 10, 88), FVector(1.4f, 34, 22), "Alu", FRotator(-18, 0, 0));
		K(EDCShape::Cube, D + FVector(7.2f, 10, 88.4f), FVector(0.3f, 31, 19), "Screen1", FRotator(-18, 0, 0), false);
		// burner phones + SIM packs
		for (int32 i = 0; i < 4; ++i) { K(EDCShape::Cube, D + FVector(-12 + J(i, 6), -40 + i * 11.f, 77.6f), FVector(13, 6.5f, 1.4f), "PlasticBlack", FRotator(0, J(i + 4, 20), 0)); }
		K(EDCShape::Cube, D + FVector(-14, -42, 78.4f), FVector(9, 5, 0.2f), "Laptop", FRotator(0, J(4, 20), 0), false);
		// cash-counting machine + stacks
		K(EDCShape::Cube, D + FVector(-18, 62, 84), FVector(30, 30, 18), "PaintGrey");
		K(EDCShape::Cube, D + FVector(-18, 62, 94), FVector(18, 8, 4), "Cash", FRotator(-25, 0, 0));
		K(EDCShape::Cube, D + FVector(-3, 62, 88), FVector(1, 10, 3), "LedG", FRotator::ZeroRotator, false);
		for (int32 i = 0; i < 9; ++i) { K(EDCShape::Cube, D + FVector(20 + (i % 3) * 9.f, 50 + (i / 3) * 17.f - 9.f, 79 + (i % 2) * 2.5f), FVector(7.5f, 16, 2.5f + (i % 2) * 2.5f), "Cash", FRotator(0, J(i, 6), 0)); }
		for (int32 i = 0; i < 9; ++i) { K(EDCShape::Cube, D + FVector(20 + (i % 3) * 9.f, 50 + (i / 3) * 17.f - 9.f, 79.2f), FVector(7.7f, 2.f, 3.f + (i % 2) * 2.5f), "Paper", FRotator(0, J(i, 6), 0)); }
		// pistol, ashtray, coffee
		K(EDCShape::Cube, D + FVector(-20, -70, 78.2f), FVector(18, 3, 4), "PlasticBlack", FRotator(0, 25, 0));
		K(EDCShape::Cube, D + FVector(-15, -66, 77.6f), FVector(4, 3, 10), "PlasticBlack", FRotator(0, 25, 90));
		K(EDCShape::Cylinder, D + FVector(25, -78, 81), FVector(8, 8, 10), "PlasticWhite");
		// architect lamp
		K(EDCShape::Cylinder, D + FVector(30, 98, 78), FVector(14, 14, 3), "SteelBlack");
		KTube(D + FVector(30, 98, 78), D + FVector(10, 92, 118), 1.6f, "SteelBlack");
		KTube(D + FVector(10, 92, 118), D + FVector(-5, 78, 110), 1.6f, "SteelBlack");
		K(EDCShape::Cone, D + FVector(-6, 76, 104), FVector(16, 16, 12), "SteelBlack");
		K(EDCShape::Cylinder, D + FVector(-6, 76, 98.5f), FVector(10, 10, 0.6f), "LedWarm", FRotator::ZeroRotator, false);
		OfficeChair(D + FVector(95, 0, 0), 180.f);
	}
	bKitOnStation = false;
	{
		USpotLightComponent* Key = Spot(FVector(34, 46, 96), FVector(20, -10, 75), Warm, 30.f, 500.f, 30.f, 70.f, true, true);
		Key->SourceRadius = 5.f;
	}

	// ---------------- Mission Board: wall of monitors + city map with pins, on the tall +Y wall
	BeginStation(EDCStationType::MissionBoard, TEXT("Mission Board"), FVector(-150, 360, 0), FVector(190, 30, 150));
	bKitOnStation = true;
	{
		const float Y = 383.f;
		// steel frame
		K(EDCShape::Cube, FVector(-150, Y + 2, 300), FVector(380, 6, 6), "SteelBlack");
		K(EDCShape::Cube, FVector(-150, Y + 2, 120), FVector(380, 6, 6), "SteelBlack");
		// centre: big map screen; flanks: 2x2 feeds and data
		Monitor(FVector(-150, Y, 212), 150, 88, 90.f, "Screen2");
		Monitor(FVector(-268, Y, 172), 64, 38, 90.f, "Screen0");
		Monitor(FVector(-268, Y, 224), 64, 38, 90.f, "Screen1");
		Monitor(FVector(-32, Y, 172), 64, 38, 90.f, "Screen1");
		Monitor(FVector(-32, Y, 224), 64, 38, 90.f, "Screen0");
		// paper city map with pins and string, right of the screens
		K(EDCShape::Cube, FVector(55, Y + 1, 200), FVector(110, 3, 80), "Cork");
		K(EDCShape::Cube, FVector(55, Y - 1, 200), FVector(100, 1, 70), "Paper");
		for (int32 i = 0; i < 7; ++i)
		{
			K(EDCShape::Cube, FVector(55 + J(i, 40), Y - 1.6f, 200 + J(i + 30, 28)), FVector(5 + FMath::Abs(J(i + 7, 4)), 0.4f, 3 + FMath::Abs(J(i + 9, 3))), "PaintGrey");
			K(EDCShape::Sphere, FVector(55 + J(i + 50, 42), Y - 2.5f, 200 + J(i + 60, 30)), FVector(2.4f), "Pin");
		}
		for (int32 i = 0; i < 4; ++i) { K(EDCShape::Cube, FVector(55 + J(i + 70, 30), Y - 2.f, 200 + J(i + 80, 22)), FVector(40, 0.3f, 0.5f), "Pin", FRotator(0, 0, J(i, 40)), false); }
		for (int32 i = 0; i < 5; ++i) { K(EDCShape::Cube, FVector(-15 + i * 3.f + 70.f + J(i, 10), Y - 1.7f, 140 + J(i + 3, 6)), FVector(14, 0.4f, 10), "Photo", FRotator(0, 0, J(i + 2, 8))); }
		// planning table in front of the wall
		K(EDCShape::Cube, FVector(-150, 330, 88), FVector(300, 70, 4), "Plywood");
		for (int32 i = 0; i < 4; ++i) { K(EDCShape::Cube, FVector(-150 + ((i % 2) ? 140.f : -140.f), 330 + ((i / 2) ? 28.f : -28.f), 43), FVector(5, 5, 86), "SteelBlack"); }
		for (int32 i = 0; i < 6; ++i) { K(EDCShape::Cube, FVector(-250 + i * 38.f + J(i, 8), 328 + J(i + 2, 10), 90.6f), FVector(24, 30, 0.6f), (i % 3 == 0) ? FName("Photo") : FName("Paper"), FRotator(0, J(i + 5, 15), 0)); }
		K(EDCShape::Cube, FVector(-40, 322, 91.5f), FVector(20, 28, 1.5f), "PlasticBlack", FRotator(0, -12, 0));  // tablet
		K(EDCShape::Cube, FVector(-40, 322, 92.4f), FVector(18, 25, 0.2f), "Screen2", FRotator(0, -12, 0), false);
	}
	bKitOnStation = false;
	// LED accent strip washing the wall + screen glow
	Deco(EDCShape::Cube, FVector(-150, 381, 312), FVector(380, 3, 2), "Accent", FRotator::ZeroRotator, false);
	Rect(FVector(-150, 360, 215), FVector(-150, 0, 120), ScreenBlue, 14.f, 300.f, 90.f, 700.f, false, true);

	// ---------------- Build Table: drafting table with site plans (front-left)
	BeginStation(EDCStationType::BuildTable, TEXT("Plans Table"), FVector(-300, -235, 0), FVector(78, 95, 80));
	bKitOnStation = true;
	{
		const FVector T(-300, -235, 0);
		for (int32 sx = -1; sx <= 1; sx += 2) { KTube(T + FVector(sx * 42, -60, 0), T + FVector(sx * 30, 0, 88), 3.f, "SteelBlack"); KTube(T + FVector(sx * 42, 60, 0), T + FVector(sx * 30, 0, 88), 3.f, "SteelBlack"); }
		KTube(T + FVector(-36, -50, 20), T + FVector(-36, 50, 20), 2.5f, "SteelBlack");
		KTube(T + FVector(36, -50, 20), T + FVector(36, 50, 20), 2.5f, "SteelBlack");
		K(EDCShape::Cube, T + FVector(0, 0, 94), FVector(100, 150, 3), "PlasticWhite", FRotator(14, 0, 0));
		K(EDCShape::Cube, T + FVector(-1, 0, 96), FVector(86, 126, 0.5f), "Blueprint", FRotator(14, 0, 0));
		K(EDCShape::Cube, T + FVector(4, 20, 96.6f), FVector(50, 60, 0.5f), "Paper", FRotator(14, 12, 0));
		K(EDCShape::Cube, T + FVector(-46, 0, 85), FVector(3, 150, 3), "Alu");
		K(EDCShape::Cube, T + FVector(0, -30, 97.5f), FVector(2.5f, 90, 0.8f), "Alu", FRotator(14, 0, 0));
		K(EDCShape::Cylinder, T + FVector(30, 88, 30), FVector(30, 30, 60), "SteelBlack");
		for (int32 i = 0; i < 5; ++i) { K(EDCShape::Cylinder, T + FVector(30 + J(i, 8), 88 + J(i + 4, 8), 64), FVector(7, 7, 80), (i % 2) ? FName("Paper") : FName("Blueprint"), FRotator(J(i + 2, 8), 0, J(i + 6, 8))); }
		K(EDCShape::Cylinder, T + FVector(-80, 30, 62), FVector(34, 34, 4), "PlasticBlack");
		KTube(T + FVector(-80, 30, 0), T + FVector(-80, 30, 60), 4.f, "Chrome");
	}
	bKitOnStation = false;

	// ---------------- room dressing: shelving, safe, sofa, water cooler
	ShelfUnit(FVector(455, 300, 0), 0.f, 160.f, 3);
	Deco(EDCShape::Cube, FVector(-440, 160, 22), FVector(80, 200, 40), "Leather");             // sofa
	Deco(EDCShape::Cube, FVector(-470, 160, 60), FVector(22, 200, 50), "Leather");
	Deco(EDCShape::Cube, FVector(-440, 52, 40), FVector(80, 18, 34), "Leather");
	Deco(EDCShape::Cube, FVector(-440, 268, 40), FVector(80, 18, 34), "Leather");
	Deco(EDCShape::Cylinder, FVector(440, -360, 55), FVector(28, 28, 110), "PlasticWhite");   // water cooler
	Deco(EDCShape::Cylinder, FVector(440, -360, 128), FVector(26, 26, 38), "Glass", FRotator::ZeroRotator, false);
	Duffel(FVector(-110, -150, 0), 30.f, "Nylon");

	// ---------------- L2: safe + cash on a pallet; L3: second screen bank, gold
	CurGroup = DCGroups::Level2;
	Deco(EDCShape::Cube, FVector(430, 180, 45), FVector(70, 70, 90), "PaintGrey");
	Deco(EDCShape::Cylinder, FVector(394, 180, 55), FVector(16, 16, 3), "Chrome", FRotator(90, 0, 0));
	Deco(EDCShape::Cube, FVector(395, 205, 40), FVector(3, 4, 18), "Chrome");
	Deco(EDCShape::Cube, FVector(-430, -300, 7), FVector(110, 110, 14), "Wood");
	for (int32 i = 0; i < 12; ++i) { Deco(EDCShape::Cube, FVector(-460 + (i % 3) * 30.f, -330 + ((i / 3) % 2) * 60.f, 24 + (i / 6) * 20.f), FVector(28, 56, 19), "Cash"); }
	Deco(EDCShape::Cube, FVector(-430, -300, 44), FVector(100, 124, 62), "Plastic", FRotator::ZeroRotator, false);
	CurGroup = DCGroups::Level3;
	Monitor(FVector(-380, 383, 172), 64, 38, 90.f, "Screen0");
	Monitor(FVector(-380, 383, 224), 64, 38, 90.f, "Screen1");
	Monitor(FVector(-380, 383, 276), 64, 38, 90.f, "Screen0");
	for (int32 i = 0; i < 6; ++i) { Deco(EDCShape::Cube, FVector(430 + (i % 3) * 0.f - 10.f + (i % 2) * 18.f, 180 - 16 + (i / 2) * 11.f, 93 + (i / 3) * 4.f), FVector(16, 8, 4), "Ball1"); }

	// ---------------- spawn zone + street door
	SpawnZoneAt(GetWorld(), CurRoom, FVector(-170, 70, 50), FVector(90, 110, 50));
	{
		ADCGarageDoor* D = GetWorld()->SpawnActor<ADCGarageDoor>(ADCGarageDoor::StaticClass(), FTransform(CurRoom->GetActorTransform().TransformPosition(FVector(490, -250, 0))));
		D->RoomType = EDCRoomType::Office;
		D->ExitPoint = FVector(170, 0, 0); // through into the garage
		D->bRollUp = false;
		D->SwingAngle = 95.f;
		D->bFallbackPanel = false;
		D->DoorPanel->SetRelativeLocation(FVector(0, -59, 0)); // pivot on the hinge jamb
		Single(D, D->DoorPanel, EDCShape::Cube, FVector(0, 59, 112), FVector(6, 118, 222), M("PaintGrey"));
		Single(D, D->DoorPanel, EDCShape::Cube, FVector(-1, 59, 170), FVector(7, 30, 40), M("Glass"));
		Single(D, D->DoorPanel, EDCShape::Cube, FVector(-7, 59, 105), FVector(4, 90, 5), M("Alu"));
		D->AttachToActor(CurRoom, FAttachmentTransformRules::KeepWorldTransform);
	}
}

// =====================================================================================================================
// BUNKROOM (Lounge)  (centre (-900,-900); x[-400,400] y[-500,500]; +X/+Y are partitions; skylight)
// =====================================================================================================================
void ADCHideoutBuilder::BuildLounge()
{
	CurRoom = GetRoom(EDCRoomType::Lounge);
	const FBox2D R(FVector2D(-400, -500), FVector2D(400, 500));
	CurGroup = DCGroups::Shell;
	ShaftFog(FVector(0, 0, 180), FVector(380, 480, 200), 0.25f);
	SunDust(FVector(80, 150, 330), FVector(-200, -150, 20), 120, 110.f);

	CurGroup = DCGroups::Level1;
	// station: sectional sofa + coffee table + TV
	BeginStation(EDCStationType::Lounge, TEXT("Bunkroom"), FVector(120, -80, 0), FVector(190, 170, 60));
	bKitOnStation = true;
	{
		const FVector S(290, -80, 0); // sofa against the +X partition, facing -X
		K(EDCShape::Cube, S + FVector(0, 0, 22), FVector(90, 260, 30), "Fabric");
		K(EDCShape::Cube, S + FVector(-4, -64, 42), FVector(82, 124, 12), "Fabric", FRotator(0, 0, 1));
		K(EDCShape::Cube, S + FVector(-4, 64, 42), FVector(82, 124, 12), "Fabric", FRotator(0, 0, -1));
		K(EDCShape::Cube, S + FVector(36, 0, 64), FVector(20, 260, 52), "Fabric", FRotator(-8, 0, 0));
		K(EDCShape::Cube, S + FVector(-60, 110, 22), FVector(100, 90, 30), "Fabric");       // chaise
		K(EDCShape::Cube, S + FVector(-60, 110, 40), FVector(92, 84, 10), "Fabric");
		for (int32 i = 0; i < 4; ++i) { K(EDCShape::Cube, S + FVector(30, -95 + i * 60.f + J(i, 8), 58), FVector(14, 40, 34), (i % 2) ? FName("FabricOlive") : FName("BlanketGrey"), FRotator(-12, J(i + 3, 10), 0)); }
		// coffee table: plywood top on black steel; pizza boxes, beer, ashtray
		const FVector T(140, -80, 0);
		K(EDCShape::Cube, T + FVector(0, 0, 40), FVector(70, 130, 4), "Plywood");
		for (int32 i = 0; i < 4; ++i) { K(EDCShape::Cube, T + FVector((i % 2) ? 30.f : -30.f, (i / 2) ? 60.f : -60.f, 19), FVector(3, 3, 38), "SteelBlack"); }
		K(EDCShape::Cube, T + FVector(4, 30, 44), FVector(36, 36, 4), "Cardboard", FRotator(0, 12, 0));
		K(EDCShape::Cube, T + FVector(4, 30, 48), FVector(36, 36, 4), "Cardboard", FRotator(0, 4, 0));
		for (int32 i = 0; i < 4; ++i) { K(EDCShape::Cylinder, T + FVector(-15 + J(i, 12), -30 + J(i + 2, 20), 51), FVector(6, 6, 18), "Bottle"); }
		K(EDCShape::Cylinder, T + FVector(15, -45, 43), FVector(14, 14, 2), "Glass", FRotator::ZeroRotator, false);
		// TV on a low black unit, screen facing +X (the sofa)
		const FVector V(-40, -80, 0);
		K(EDCShape::Cube, V + FVector(0, 0, 22), FVector(40, 170, 44), "PlasticBlack");
		K(EDCShape::Cube, V + FVector(0, 0, 47), FVector(10, 30, 6), "PlasticBlack");
		Monitor(V + FVector(0, 0, 92), 140, 80, 180.f, "TV");
		K(EDCShape::Cube, V + FVector(4, 50, 48), FVector(18, 30, 6), "PlasticBlack");  // console
		K(EDCShape::Cube, V + FVector(14, 50, 48), FVector(1, 4, 1), "LedB", FRotator::ZeroRotator, false);
	}
	bKitOnStation = false;
	Point(FVector(10, -80, 95), ScreenBlue, 4.f, 450.f, false, true, 40.f); // TV glow

	// steel bunk beds along the +Y partition (capacity grows with level)
	auto Bunk = [&](float X, float Y, float Yaw)
	{
		const FRotator YR(0, Yaw, 0);
		auto A = [&](float LX, float LY, float LZ) { return FVector(X, Y, 0) + YR.RotateVector(FVector(LX, LY, LZ)); };
		for (int32 i = 0; i < 4; ++i) { Deco(EDCShape::Cube, A((i % 2) ? 97.f : -97.f, (i / 2) ? 44.f : -44.f, 92), FVector(5, 5, 184), "SteelBlack", YR); }
		for (float Z : { 32.f, 128.f })
		{
			Deco(EDCShape::Cube, A(0, 0, Z), FVector(200, 92, 5), "SteelBlack", YR);
			Deco(EDCShape::Cube, A(0, 0, Z + 9), FVector(190, 86, 13), "Linen", YR);
			Deco(EDCShape::Cube, A(-76, 0, Z + 19), FVector(30, 56, 9), "PlasticWhite", YR);
			Deco(EDCShape::Cube, A(28, 0, Z + 17), FVector(120, 88, 5), (Z > 100.f) ? FName("Blanket") : FName("BlanketGrey"), Compose(Yaw, FRotator(0, 0, 2)));
			// crew sleep here at night: mattress top centre, +X head (pillow) -> foot
			USceneComponent* BedC = NewObject<USceneComponent>(CurRoom);
			BedC->SetupAttachment(CurRoom->GetRootComponent());
			BedC->SetRelativeLocationAndRotation(A(0, 0, Z + 15.5f), FRotator(0, Yaw, 0));
			BedC->ComponentTags.Add(FName("DC.Bed"));
			BedC->RegisterComponent();
			CurRoom->AddInstanceComponent(BedC);
			CurRoom->AddToGroup(BedC, CurGroup);
		}
		Deco(EDCShape::Cube, A(97, 48, 150), FVector(4, 4, 60), "SteelBlack", YR);
		for (int32 i = 0; i < 4; ++i) { Deco(EDCShape::Cube, A(97, 48, 60 + i * 26.f), FVector(4, 30, 3), "SteelBlack", Compose(Yaw, FRotator(0, 90, 0))); }
	};
	Bunk(-260, 430, 0.f);
	Duffel(FVector(-180, 360, 0), 10.f, "NylonOlive");
	// lockers
	for (int32 i = 0; i < 4; ++i)
	{
		const float Y = 280 + i * 42.f;
		Deco(EDCShape::Cube, FVector(370, Y, 95), FVector(46, 40, 190), "PaintGrey");
		Deco(EDCShape::Cube, FVector(346.5f, Y, 160), FVector(1, 26, 12), "Black");
		Deco(EDCShape::Cube, FVector(346.5f, Y + 12, 100), FVector(2, 3, 14), "Alu");
	}
	// pool table with a long LED pendant (key light)
	{
		const FVector P(-170, 120, 0);
		Deco(EDCShape::Cube, P + FVector(0, 0, 68), FVector(150, 260, 16), "Black");
		Deco(EDCShape::Cube, P + FVector(0, 0, 77), FVector(128, 238, 2), "Felt");
		for (int32 i = 0; i < 4; ++i) { Deco(EDCShape::Cube, P + FVector((i % 2) ? 55.f : -55.f, (i / 2) ? 105.f : -105.f, 30), FVector(16, 16, 60), "Black"); }
		for (float S : { -1.f, 1.f }) { Deco(EDCShape::Cube, P + FVector(S * 71.f, 0, 81), FVector(8, 260, 6), "Black"); Deco(EDCShape::Cube, P + FVector(0, S * 126.f, 81), FVector(150, 8, 6), "Black"); }
		static const FName Balls[] = { "Ball1", "Ball2", "BallW", "Ball2", "Ball1", "PlasticBlack", "Ball2" };
		for (int32 i = 0; i < 7; ++i) { Deco(EDCShape::Sphere, P + FVector(J(i, 40), 40 + J(i + 20, 60), 81), FVector(5.5f), Balls[i]); }
		Tube(P + FVector(-30, -60, 80), P + FVector(20, 70, 82), 1.8f, "Wood");
		LedBatten(P + FVector(0, 0, 175), 160.f, 90.f, false);
		Spot(P + FVector(0, 0, 168), P, Neutral, 45.f, 650.f, 40.f, 72.f, false, true);
	}
	// kitchenette along the +X partition (back corner)
	{
		const float X = 365.f;
		Deco(EDCShape::Cube, FVector(X, 90, 44), FVector(60, 180, 88), "PaintLight");
		Deco(EDCShape::Cube, FVector(X, 90, 90), FVector(64, 184, 4), "Steel");
		Deco(EDCShape::Cube, FVector(X - 4, 60, 91), FVector(40, 46, 2), "Black");           // sink
		Tube(FVector(X + 18, 60, 92), FVector(X + 18, 60, 122), 2.5f, "Chrome");
		Tube(FVector(X + 18, 60, 122), FVector(X + 2, 60, 118), 2.f, "Chrome");
		Deco(EDCShape::Cube, FVector(X, 140, 106), FVector(40, 50, 28), "PlasticBlack");    // microwave
		Deco(EDCShape::Cube, FVector(X - 20.5f, 136, 106), FVector(1, 30, 18), "Glass", FRotator::ZeroRotator, false);
		Deco(EDCShape::Cube, FVector(X, 210, 95), FVector(64, 70, 190), "Alu");             // fridge
		Deco(EDCShape::Cube, FVector(X - 33, 236, 120), FVector(2, 4, 60), "PlasticBlack");
		Deco(EDCShape::Cylinder, FVector(X, 110, 98), FVector(12, 12, 14), "PlasticBlack");  // kettle
	}

	CurGroup = DCGroups::Level2;
	Bunk(-40, 430, 0.f);
	Deco(EDCShape::Cube, FVector(80, -330, 30), FVector(80, 80, 60), "Fabric");            // armchair
	Deco(EDCShape::Cube, FVector(110, -330, 70), FVector(20, 80, 40), "Fabric");
	Point(FVector(-40, 400, 250), Warm, 10.f, 500.f, false, true, 20.f);
	CurGroup = DCGroups::Level3;
	Bunk(-310, 230, 90.f);
	Deco(EDCShape::Cube, FVector(395, -80, 120), FVector(3, 280, 3), "Accent", FRotator::ZeroRotator, false); // LED strip behind sofa
	Rect(FVector(380, -80, 120), FVector(0, -80, 40), FLinearColor(0.2f, 0.6f, 1.f), 6.f, 280.f, 6.f, 500.f, false, true);
	Deco(EDCShape::Cube, FVector(200, 300, 45), FVector(60, 60, 90), "PlasticBlack");     // arcade / speaker stack
	Deco(EDCShape::Cylinder, FVector(170.5f, 300, 60), FVector(30, 30, 1), "Rubber", FRotator(0, 0, 0));

	LockedDressing(R, { {230, -80, 230, 90}, {-170, 120, 270, 95}, {365, 150, 120, 120}, {-260, 430, 200, 170} });
	BuildingDressing(R);
	SpawnZoneAt(GetWorld(), CurRoom, FVector(-240, -340, 50), FVector(120, 110, 50));
}

// =====================================================================================================================
// WORKSHOP  (centre (0,-900); x[-500,500] y[-500,500]; +X/+Y partitions; skylight)
// =====================================================================================================================
void ADCHideoutBuilder::BuildWorkshop()
{
	CurRoom = GetRoom(EDCRoomType::Workshop);
	const FBox2D R(FVector2D(-500, -500), FVector2D(500, 500));
	CurGroup = DCGroups::Shell;
	ShaftFog(FVector(0, 0, 180), FVector(480, 480, 200), 0.25f);
	SunDust(FVector(100, 150, 330), FVector(-250, -200, 20), 140, 120.f);
	CurGroup = DCGroups::Level1;

	BeginStation(EDCStationType::Workbench, TEXT("Workbench"), FVector(-60, 420, 0), FVector(150, 75, 125));
	bKitOnStation = true;
	{
		const FVector B(-60, 420, 0);
		K(EDCShape::Cube, B + FVector(0, 0, 90), FVector(270, 80, 6), "Plywood");
		K(EDCShape::Cube, B + FVector(0, -40, 87), FVector(272, 3, 8), "SteelBlack");
		for (int32 i = 0; i < 4; ++i) { K(EDCShape::Cube, B + FVector((i % 2) ? 125.f : -125.f, (i / 2) ? 34.f : -34.f, 43), FVector(6, 6, 86), "SteelBlack"); }
		K(EDCShape::Cube, B + FVector(0, 0, 22), FVector(260, 74, 3), "Plywood");
		K(EDCShape::Cube, B + FVector(80, 0, 55), FVector(70, 66, 56), "PaintRed");                 // drawer unit
		for (int32 i = 0; i < 4; ++i) { K(EDCShape::Cube, B + FVector(80, -34, 35 + i * 13.f), FVector(60, 1, 1.5f), "Alu"); }
		// pegboard rising above the partition, with tool silhouettes
		K(EDCShape::Cube, B + FVector(0, 62, 185), FVector(280, 3, 170), "PaintGrey");
		K(EDCShape::Cube, B + FVector(0, 63, 272), FVector(286, 5, 6), "SteelBlack");
		KTube(B + FVector(-140, 64, 0), B + FVector(-140, 64, 270), 4, "SteelBlack");
		KTube(B + FVector(140, 64, 0), B + FVector(140, 64, 270), 4, "SteelBlack");
		for (int32 i = 0; i < 5; ++i) { KTube(B + FVector(-120 + i * 14.f, 59, 140), B + FVector(-120 + i * 14.f, 59, 170 + i * 5.f), 2.f, "Chrome"); }    // wrenches
		for (int32 i = 0; i < 3; ++i) { KTube(B + FVector(-40 + i * 22.f, 59, 135), B + FVector(-40 + i * 22.f, 59, 168), 2.4f, "PaintRed"); K(EDCShape::Cube, B + FVector(-40 + i * 22.f, 59, 172), FVector(12, 3, 4), "SteelBlack"); }
		K(EDCShape::Cube, B + FVector(50, 59, 160), FVector(55, 1, 18), "Steel", FRotator(10, 0, 0));    // saw
		KTube(B + FVector(100, 59, 130), B + FVector(108, 59, 215), 2.5f, "PaintRed");                      // crowbar
		K(EDCShape::Cube, B + FVector(-90, 59, 215), FVector(70, 3, 30), "PlasticBlack");                   // cordless drill rack
		for (int32 i = 0; i < 3; ++i) { K(EDCShape::Cube, B + FVector(-112 + i * 22.f, 56, 222), FVector(10, 6, 20), (i == 1) ? FName("PaintOrange") : FName("PaintRed")); }
		// hard gun case open on the bench + rifle, ammo cans, duffels
		K(EDCShape::Cube, B + FVector(-60, -8, 98), FVector(100, 34, 10), "PlasticBlack", FRotator(0, 6, 0));
		K(EDCShape::Cube, B + FVector(-60, -8, 103.5f), FVector(94, 30, 1), "Rubber", FRotator(0, 6, 0));
		K(EDCShape::Cube, B + FVector(-60, -8, 106), FVector(80, 4, 4), "SteelBlack", FRotator(0, 6, 0));
		K(EDCShape::Cube, B + FVector(-80, -8, 106), FVector(22, 6, 7), "SteelBlack", FRotator(0, 6, 0));
		K(EDCShape::Cube, B + FVector(-60, 18, 115), FVector(100, 3, 30), "PlasticBlack", FRotator(-70, 6, 0)); // lid
		for (int32 i = 0; i < 2; ++i) { K(EDCShape::Cube, B + FVector(30 + i * 30.f, -10, 104), FVector(26, 14, 18), "PaintOlive"); }
		K(EDCShape::Cube, B + FVector(110, -10, 100), FVector(20, 16, 14), "SteelBlack");                     // vise
		Duffel(B + FVector(40, -80, 0), 15.f, "Nylon");
		Duffel(B + FVector(-30, -88, 0), -10.f, "NylonOlive");
		Duffel(B + FVector(5, -84, 30), 70.f, "Nylon");
	}
	bKitOnStation = false;
	Spot(FVector(-60, 380, 230), FVector(-60, 410, 90), Neutral, 40.f, 500.f, 35.f, 70.f, false, true);
	LedBatten(FVector(-60, 300, 300), 160.f, 0.f, true, 50.f);
	{
		LedBatten(FVector(-60, -60, 300), 160.f, 0.f, false);
		URectLightComponent* F = Rect(FVector(-60, -60, 294), FVector(-60, -60, 0), FLinearColor(0.85f, 0.95f, 1.f), 40.f, 150.f, 10.f, 900.f, false, true);
		AddFlicker(F, 9.f, 0.9f);
		Deco(EDCShape::Cube, FVector(-60, -60, 296.5f), FVector(152, 8, 1.4f), "Fluoro", FRotator::ZeroRotator, false);
	}
	// centre: a safe on a pallet mid-crack, mag drill on it
	Deco(EDCShape::Cube, FVector(-40, -60, 7), FVector(120, 100, 14), "Wood");
	Deco(EDCShape::Cube, FVector(-40, -60, 64), FVector(86, 80, 100), "PaintGrey");
	Deco(EDCShape::Cylinder, FVector(-84, -60, 74), FVector(26, 26, 3), "Chrome", FRotator(90, 0, 0));
	Deco(EDCShape::Cube, FVector(-110, -60, 90), FVector(20, 30, 40), "PaintOrange");
	Tube(FVector(-110, -60, 90), FVector(-86, -60, 80), 4, "Steel");
	Tube(FVector(-120, -60, 70), FVector(-200, -120, 0), 1.5f, "Black");
	// steel worktable with a grinder, welding cart
	Deco(EDCShape::Cube, FVector(200, -40, 88), FVector(180, 90, 5), "DiamondPlate");
	for (int32 i = 0; i < 4; ++i) { Deco(EDCShape::Cube, FVector(200 + ((i % 2) ? 84.f : -84.f), -40 + ((i / 2) ? 40.f : -40.f), 43), FVector(6, 6, 86), "SteelBlack"); }
	Deco(EDCShape::Cube, FVector(240, -30, 100), FVector(30, 14, 16), "PaintOrange");
	for (int32 i = 0; i < 4; ++i) { Deco(EDCShape::Cube, FVector(160 + J(i, 20), -40 + J(i + 3, 20), 92), FVector(30, 4, 2), "Steel", FRotator(0, J(i + 5, 60), 0)); }
	// tool chests + shelving + drums on the +X partition side
	Deco(EDCShape::Cube, FVector(330, 420, 60), FVector(70, 120, 120), "PaintRed");
	for (int32 i = 0; i < 6; ++i) { Deco(EDCShape::Cube, FVector(294.5f, 420, 20 + i * 18.f), FVector(1, 110, 1.5f), "Alu"); }
	ShelfUnit(FVector(455, -200, 0), 0.f, 200.f, 11);
	for (int32 i = 0; i < 3; ++i) { Deco(EDCShape::Cylinder, FVector(400 - i * 8.f, -440 + i * 62.f, 45), FVector(58, 58, 90), i == 1 ? FName("PaintBlue") : FName("SteelBlack")); }
	Crate(FVector(160, 260, 0), FVector(80, 60, 60), 8.f, "Cardboard");
	Crate(FVector(170, 262, 60), FVector(60, 50, 40), -6.f, "Cardboard");

	CurGroup = DCGroups::Level2;
	// welding cart + gas bottles, drill press
	Deco(EDCShape::Cube, FVector(260, 200, 40), FVector(60, 90, 6), "SteelBlack");
	Deco(EDCShape::Cylinder, FVector(260, 180, 80), FVector(24, 24, 140), "PaintRed");
	Deco(EDCShape::Cylinder, FVector(260, 220, 80), FVector(24, 24, 140), "PaintBlue");
	Deco(EDCShape::Cube, FVector(120, 430, 70), FVector(40, 40, 140), "PaintGrey");
	Deco(EDCShape::Cube, FVector(120, 410, 150), FVector(30, 60, 30), "PaintGrey");
	Point(FVector(260, 200, 220), Neutral, 8.f, 500.f, false, true, 4.f);
	CurGroup = DCGroups::Level3;
	// open gun locker
	Deco(EDCShape::Cube, FVector(-400, 380, 100), FVector(60, 120, 200), "PaintOlive");
	Deco(EDCShape::Cube, FVector(-432, 380, 100), FVector(2, 112, 190), "Black");
	for (int32 i = 0; i < 5; ++i) { Tube(FVector(-420, 335 + i * 22.f, 30), FVector(-420, 335 + i * 22.f, 150), 4, "SteelBlack"); }
	Deco(EDCShape::Cube, FVector(-440, 300, 100), FVector(4, 6, 190), "PaintOlive", FRotator(0, -70, 0));
	Point(FVector(-330, 380, 220), Neutral, 6.f, 400.f, false, true, 4.f);

	LockedDressing(R, { {-60, 420, 260, 110}, {455, -200, 200, 190}, {-40, -60, 140, 110} });
	BuildingDressing(R);
}

// =====================================================================================================================
// GARAGE  (centre (900,-900); x[-400,400] y[-500,500]; +X exterior with the roller door, +Y exterior to the yard)
// =====================================================================================================================
void ADCHideoutBuilder::BuildGarage()
{
	// x[-400,400] y[-900,900]: the old R3A bay plus the former yard notch. -X: Workshop (y<100) / Office (tall wall with
	// the street door, y>100). +X exterior: roller door at y=-400 + factory windows. +Y: knee partition to the Ops Room.
	CurRoom = GetRoom(EDCRoomType::Garage);
	const FBox2D R(FVector2D(-400, -900), FVector2D(400, 900));
	const float DY = -400.f; // roller-door bay centre

	CurGroup = DCGroups::Shell;
	Deco(EDCShape::Cube, FVector(372, DY, 312), FVector(34, 390, 34), "SteelBlack"); // roll drum housing
	Deco(EDCShape::Cube, FVector(372, DY - 186, 145), FVector(14, 10, 290), "SteelBlack");
	Deco(EDCShape::Cube, FVector(372, DY + 186, 145), FVector(14, 10, 290), "SteelBlack");
	Deco(EDCShape::Cube, FVector(372, DY - 200, 160), FVector(8, 14, 20), "PlasticBlack");   // door control box
	Deco(EDCShape::Cube, FVector(367.5f, DY - 200, 164), FVector(1, 3, 3), "LedG", FRotator::ZeroRotator, false);
	// oil stains, painted bay lines, drain
	for (int32 i = 0; i < 5; ++i) { Deco(EDCShape::Cube, FVector(-40 + i * 25.f, DY - 20 + J(i, 60), 0.3f), FVector(50 + i * 9.f, 40 + i * 6.f, 0.6f), "Black", FRotator(0, i * 31.f, 0), false); }
	for (float Y : { -870.f, -570.f, -250.f, 200.f }) { Deco(EDCShape::Cube, FVector(-10, Y, 0.4f), FVector(560, 10, 0.8f), "Caution", FRotator::ZeroRotator, false); }
	Deco(EDCShape::Cube, FVector(150, DY, 0.3f), FVector(14, 200, 0.8f), "SteelBlack", FRotator::ZeroRotator, false);
	ShaftFog(FVector(120, -50, 170), FVector(300, 820, 190), 0.3f);
	SunDust(FVector(360, 300, 290), FVector(-150, 0, 20), 160, 120.f);
	SunDust(FVector(360, -750, 290), FVector(-100, -650, 20), 80, 80.f);

	CurGroup = DCGroups::Level1;
	BeginStation(EDCStationType::GarageDoor, TEXT("Garage"), FVector(30, DY, 0), FVector(240, 105, 85));
	bKitOnStation = true;
	Car(FVector(30, DY, 0), 0.f, "CarPaint", false);
	bKitOnStation = false;
	// LED battens over the bay (key)
	LedBatten(FVector(30, DY - 60, 290), 200.f, 0.f, true, 70.f);
	LedBatten(FVector(30, DY + 60, 290), 200.f, 0.f, false);
	Rect(FVector(30, DY + 60, 284), FVector(30, DY + 60, 0), FLinearColor(0.88f, 0.94f, 1.f), 50.f, 200.f, 10.f, 1000.f, false, true);
	Point(FVector(-215, DY, 70), FLinearColor(1.f, 0.05f, 0.03f), 2.f, 220.f, false, true, 4.f);
	// second car parked in the back half
	Car(FVector(60, 380, 0), 0.f, "CarGrey", false);
	LedBatten(FVector(60, 380, 300), 200.f, 0.f, true, 35.f);
	// workbench along the -X partition, tyre stack, jerry cans, compressor
	Deco(EDCShape::Cube, FVector(-340, -250, 88), FVector(70, 220, 6), "Plywood");
	for (int32 i = 0; i < 4; ++i) { Deco(EDCShape::Cube, FVector(-340 + ((i % 2) ? 30.f : -30.f), -250 + ((i / 2) ? 100.f : -100.f), 43), FVector(6, 6, 86), "SteelBlack"); }
	Deco(EDCShape::Cube, FVector(-340, -300, 98), FVector(30, 40, 14), "PaintRed");
	Deco(EDCShape::Cube, FVector(-345, -200, 96), FVector(24, 30, 10), "PlasticBlack");
	for (int32 i = 0; i < 5; ++i) { Deco(EDCShape::Cylinder, FVector(-330, -820, 12 + i * 22.f), FVector(66, 66, 22), "Tire"); }
	for (int32 i = 0; i < 3; ++i) { Deco(EDCShape::Cube, FVector(-340 + i * 32.f, -80, 24), FVector(26, 14, 48), "PaintRed"); }
	Deco(EDCShape::Cylinder, FVector(-230, -830, 40), FVector(50, 50, 80), "PaintBlue");
	Deco(EDCShape::Cube, FVector(-230, -830, 90), FVector(30, 30, 20), "SteelBlack");
	// chain-link storage cage in the back corner
	{
		const float X0 = 150.f, X1 = 360.f, Y0 = 680.f, Y1 = 870.f, H = 240.f;
		Deco(EDCShape::Cube, FVector((X0 + X1) * 0.5f, Y0, H * 0.5f), FVector(X1 - X0, 1.f, H), "Fence");
		Deco(EDCShape::Cube, FVector(X0, (Y0 + Y1) * 0.5f, H * 0.5f), FVector(1.f, Y1 - Y0, H), "Fence");
		for (const FVector2D& P : { FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X0, Y1), FVector2D((X0 + X1) * 0.5f, Y0) }) { Tube(FVector(P.X, P.Y, 0), FVector(P.X, P.Y, H), 5.f, "Galv"); }
		Tube(FVector(X0, Y0, H), FVector(X1, Y0, H), 4.f, "Galv");
		Tube(FVector(X0, Y0, H), FVector(X0, Y1, H), 4.f, "Galv");
		Crate(FVector(250, 790, 0), FVector(90, 70, 70), 5.f, "PaintOlive");
		Crate(FVector(300, 800, 70), FVector(60, 50, 40), -8.f, "PlasticBlack");
		Crate(FVector(210, 800, 0), FVector(50, 60, 120), 0.f, "Cardboard");
	}
	ShelfUnit(FVector(-250, 850, 0), 90.f, 200.f, 17);
	for (int32 i = 0; i < 3; ++i) { Deco(EDCShape::Cube, FVector(-300 + J(i, 6), 640, 7 + i * 14.f), FVector(110, 110, 12), "Wood", FRotator(0, J(i + 2, 8), 0)); }
	CurGroup = DCGroups::Level2;
	Car(FVector(0, -730, 0), 0.f, "CarWhite", true); // the van
	LedBatten(FVector(0, -730, 300), 200.f, 0.f, true, 22.f);
	CurGroup = DCGroups::Level3;
	// two-post lift around the saloon + tool chest
	for (float X : { -150.f, 190.f }) { Deco(EDCShape::Cube, FVector(X, DY - 120, 150), FVector(26, 26, 300), "PaintBlue"); Deco(EDCShape::Cube, FVector(X, DY - 120, 6), FVector(50, 50, 12), "PaintBlue"); }
	Deco(EDCShape::Cube, FVector(20, DY - 120, 296), FVector(360, 14, 14), "PaintBlue");
	Deco(EDCShape::Cube, FVector(-300, -560, 55), FVector(60, 120, 110), "PaintRed");
	Point(FVector(-300, -560, 220), Neutral, 8.f, 500.f, false, true, 4.f);

	// roller door actor (E animates DoorPanel upwards)
	{
		ADCGarageDoor* D = GetWorld()->SpawnActor<ADCGarageDoor>(ADCGarageDoor::StaticClass(), FTransform(CurRoom->GetActorTransform().TransformPosition(FVector(390, DY, 0))));
		D->RoomType = EDCRoomType::Garage;
		D->ExitPoint = FVector(320, 0, 0); // out through the roller door onto the street
		D->bRollUp = true;
		D->RollUpHeight = 280.f;
		D->bFallbackPanel = false;
		Single(D, D->DoorPanel, EDCShape::Cube, FVector(0, 0, 145), FVector(6, 360, 290), M("Corrugated"));
		Single(D, D->DoorPanel, EDCShape::Cube, FVector(-4, 0, 5), FVector(6, 362, 10), M("Rubber"));
		Single(D, D->DoorPanel, EDCShape::Cube, FVector(-5, 0, 40), FVector(4, 40, 6), M("Alu"));
		D->AttachToActor(CurRoom, FAttachmentTransformRules::KeepWorldTransform);
	}

	LockedDressing(R, { {30, DY, 470, 150}, {60, 380, 470, 150}, {-300, 640, 110, 60} });
	BuildingDressing(R);
}

// =====================================================================================================================
// OPS ROOM  (centre (400,1000); x[-900,900] y[-600,600]; +X and +Y exterior walls with windows)
// =====================================================================================================================
void ADCHideoutBuilder::BuildOpsRoom()
{
	CurRoom = GetRoom(EDCRoomType::OpsRoom);
	const FBox2D R(FVector2D(-900, -600), FVector2D(900, 600));
	CurGroup = DCGroups::Shell;
	ShaftFog(FVector(200, 100, 180), FVector(700, 520, 200), 0.3f);
	SunDust(FVector(600, 560, 300), FVector(0, -100, 20), 220, 160.f);
	SunDust(FVector(860, 200, 300), FVector(200, -150, 20), 160, 140.f);

	CurGroup = DCGroups::Level1;
	BeginStation(EDCStationType::OpsRoom, TEXT("Ops Room"), FVector(0, 480, 0), FVector(330, 110, 160));
	bKitOnStation = true;
	{
		const float Y = 560.f;
		// steel frame + 6x3 CCTV wall
		for (int32 i = 0; i < 4; ++i) { K(EDCShape::Cube, FVector(-270 + i * 180.f, Y + 6, 160), FVector(8, 8, 320), "SteelBlack"); }
		K(EDCShape::Cube, FVector(0, Y + 6, 318), FVector(560, 8, 6), "SteelBlack");
		for (int32 r = 0; r < 3; ++r)
		{
			for (int32 c = 0; c < 6; ++c)
			{
				const bool bData = (r == 1 && (c == 2 || c == 3));
				Monitor(FVector(-225 + c * 90.f, Y, 130 + r * 56.f), 84, 50, 90.f, bData ? FName("Screen1") : FName("Screen0"));
			}
		}
		// operator desk (three segments, slight curve) + keyboards + chairs
		for (int32 s = -1; s <= 1; ++s)
		{
			const float Yaw = s * -12.f;
			const FVector C(s * 170.f, 430 - FMath::Abs(s) * 18.f, 0);
			K(EDCShape::Cube, C + FVector(0, 0, 74), FVector(170, 70, 4), "PaintGrey", FRotator(0, Yaw, 0));
			K(EDCShape::Cube, C + FVector(0, 0, 38), FVector(160, 4, 70), "SteelBlack", FRotator(0, Yaw, 0));
			K(EDCShape::Cube, C + FVector(-10, -12, 77), FVector(44, 15, 2), "PlasticBlack", FRotator(0, Yaw, 0));
			Monitor(C + FVector(0, 18, 100), 56, 32, 90.f + Yaw, (s == 0) ? FName("Screen2") : FName("Screen1"), 0.f);
			K(EDCShape::Cube, C + FVector(0, 26, 79), FVector(14, 14, 4), "PlasticBlack", FRotator(0, Yaw, 0));
			K(EDCShape::Cylinder, C + FVector(50, -14, 81), FVector(8, 8, 10), (s == 0) ? FName("PlasticWhite") : FName("PaintRed"));
			OfficeChair(C + FVector(0, -70, 0), 90.f + Yaw);
		}
		// cable runs down from the wall to a floor trunk
		for (int32 i = 0; i < 6; ++i) { KTube(FVector(-225 + i * 90.f, Y + 2, 105), FVector(-200 + i * 70.f, Y - 30, 1.5f), 1.5f, "Rubber"); }
		K(EDCShape::Cube, FVector(0, Y - 30, 3), FVector(500, 14, 6), "SteelBlack");
		K(EDCShape::Cube, FVector(300, 530, 50), FVector(40, 50, 100), "Rack");   // PC tower / recorder
		for (int32 i = 0; i < 5; ++i) { K(EDCShape::Cube, FVector(279.5f, 520 + i * 4.f, 80 - i * 6.f), FVector(1, 2, 2), (i % 2) ? FName("LedG") : FName("LedB"), FRotator::ZeroRotator, false); }
	}
	bKitOnStation = false;
	Rect(FVector(0, 500, 200), FVector(0, 0, 60), ScreenBlue, 22.f, 520.f, 160.f, 1100.f, false, true);
	Deco(EDCShape::Cube, FVector(0, 557, 326), FVector(560, 3, 2), "Accent", FRotator::ZeroRotator, false);
	// LED battens + pallet racking along the -X side (back of the lawyer / clinic)
	LedBatten(FVector(0, 300, 320), 220.f, 0.f, true, 45.f);
	LedBatten(FVector(450, 0, 320), 220.f, 90.f, true, 35.f);
	PalletRack(FVector(-820, 100, 0), 0.f, 5, 3);
	// lounge corner: couch + low table + whiteboard on wheels
	Deco(EDCShape::Cube, FVector(500, -350, 22), FVector(90, 220, 30), "Leather");
	Deco(EDCShape::Cube, FVector(538, -350, 60), FVector(20, 220, 50), "Leather");
	Deco(EDCShape::Cube, FVector(380, -350, 38), FVector(60, 110, 4), "Plywood");
	for (int32 i = 0; i < 4; ++i) { Deco(EDCShape::Cube, FVector(380 + ((i % 2) ? 25.f : -25.f), -350 + ((i / 2) ? 50.f : -50.f), 18), FVector(3, 3, 36), "SteelBlack"); }
	Deco(EDCShape::Cube, FVector(700, 200, 140), FVector(4, 160, 100), "PlasticWhite");
	Deco(EDCShape::Cube, FVector(698, 200, 140), FVector(1, 140, 80), "Paper");
	for (float Y : { 130.f, 270.f }) { Tube(FVector(700, Y, 0), FVector(700, Y, 190), 3.f, "Alu"); }
	for (int32 i = 0; i < 6; ++i) { Deco(EDCShape::Cube, FVector(697, 150 + i * 18.f + J(i, 5), 150 + J(i + 4, 25)), FVector(0.5f, 14, 1.2f), "Black"); }
	// stacked crates by the +X windows
	Crate(FVector(780, -300, 0), FVector(110, 90, 80), 4.f, "Wood");
	Crate(FVector(780, -300, 80), FVector(100, 80, 70), -3.f, "PaintOlive");
	Crate(FVector(790, -170, 0), FVector(90, 90, 90), 9.f, "Cardboard");

	CurGroup = DCGroups::Level2;
	// forklift
	{
		const FVector F(300, 60, 0);
		Deco(EDCShape::Cube, F + FVector(0, 0, 60), FVector(200, 110, 80), "PaintOrange");
		Deco(EDCShape::Cube, F + FVector(-80, 0, 75), FVector(50, 110, 70), "SteelBlack");
		Deco(EDCShape::Cube, F + FVector(-10, 0, 110), FVector(50, 60, 10), "Black");
		for (float S : { -1.f, 1.f })
		{
			Tube(F + FVector(-50, S * 50, 100), F + FVector(-30, S * 50, 220), 4.f, "SteelBlack");
			Tube(F + FVector(60, S * 50, 100), F + FVector(40, S * 50, 220), 4.f, "SteelBlack");
			Deco(EDCShape::Cube, F + FVector(112, S * 35, 140), FVector(8, 8, 280), "SteelBlack");
			Deco(EDCShape::Cube, F + FVector(165, S * 35, 8), FVector(100, 12, 5), "SteelBlack");
		}
		Deco(EDCShape::Cube, F + FVector(-10, 0, 222), FVector(120, 110, 5), "SteelBlack");
		for (int32 i = 0; i < 4; ++i) { Deco(EDCShape::Cylinder, F + FVector((i % 2) ? 60.f : -60.f, (i / 2) ? 58.f : -58.f, 25), FVector(50, 50, 22), "Tire", FRotator(0, 0, 90)); }
		Deco(EDCShape::Cube, F + FVector(165, 0, 22), FVector(110, 120, 12), "Wood");
		Deco(EDCShape::Cube, F + FVector(165, 0, 60), FVector(100, 110, 64), "Cardboard");
	}
	// second desk row
	for (int32 s = 0; s < 2; ++s)
	{
		const FVector C(-250 + s * 200.f, 220, 0);
		Deco(EDCShape::Cube, C + FVector(0, 0, 74), FVector(170, 70, 4), "PaintGrey");
		for (int32 i = 0; i < 2; ++i) { Deco(EDCShape::Cube, C + FVector((i ? 80.f : -80.f), 0, 37), FVector(4, 66, 74), "SteelBlack"); }
		Monitor(C + FVector(-30, 20, 100), 56, 32, 90.f, "Screen1");
		Monitor(C + FVector(30, 20, 100), 56, 32, 90.f, "Screen0");
		OfficeChair(C + FVector(0, -60, 0), 90.f);
	}
	CurGroup = DCGroups::Level3;
	PalletRack(FVector(-820, -400, 0), 0.f, 2, 9);
	// motorbike
	{
		const FVector B(600, 300, 0);
		Deco(EDCShape::Cylinder, B + FVector(-70, 0, 32), FVector(64, 64, 14), "Tire", FRotator(0, 0, 90));
		Deco(EDCShape::Cylinder, B + FVector(75, 0, 32), FVector(64, 64, 12), "Tire", FRotator(0, 0, 90));
		Deco(EDCShape::Cube, B + FVector(0, 0, 62), FVector(90, 26, 36), "SteelBlack");
		Deco(EDCShape::Cube, B + FVector(10, 0, 90), FVector(60, 28, 22), "CarPaint");
		Deco(EDCShape::Cube, B + FVector(-45, 0, 86), FVector(60, 24, 8), "Leather");
		Tube(B + FVector(75, 0, 32), B + FVector(50, 0, 110), 4.f, "Chrome");
		Tube(B + FVector(50, -32, 112), B + FVector(50, 32, 112), 3.f, "SteelBlack");
		Deco(EDCShape::Cube, B + FVector(58, 0, 100), FVector(6, 18, 12), "Head", FRotator::ZeroRotator, false);
	}
	Point(FVector(600, 300, 250), Neutral, 8.f, 600.f, false, true, 10.f);

	LockedDressing(R, { {0, 480, 600, 330}, {-820, 100, 200, 260}, {300, 60, 300, 160}, {500, -350, 240, 90} });
	BuildingDressing(R);
}

// =====================================================================================================================
// CLINIC  (centre (-900,1300); x[-400,400] y[-300,300]; +Y exterior with windows; epoxy floor)
// =====================================================================================================================
void ADCHideoutBuilder::BuildClinic()
{
	CurRoom = GetRoom(EDCRoomType::Clinic);
	const FBox2D R(FVector2D(-400, -300), FVector2D(400, 300));
	CurGroup = DCGroups::Shell;
	ShaftFog(FVector(0, 80, 170), FVector(380, 240, 180), 0.3f);
	SunDust(FVector(0, 270, 290), FVector(-150, -100, 20), 100, 90.f);
	CurGroup = DCGroups::Level1;
	BeginStation(EDCStationType::Clinic, TEXT("Clinic"), FVector(80, 60, 0), FVector(115, 60, 100));
	bKitOnStation = true;
	{
		const FVector B(80, 60, 0);
		K(EDCShape::Cube, B + FVector(0, 0, 70), FVector(200, 76, 6), "Steel");
		K(EDCShape::Cube, B + FVector(0, 0, 80), FVector(194, 70, 14), "PlasticWhite");
		K(EDCShape::Cube, B + FVector(-80, 0, 92), FVector(30, 52, 10), "Linen", FRotator(10, 0, 0));
		K(EDCShape::Cube, B + FVector(25, 0, 89), FVector(130, 74, 4), "Linen", FRotator(0, 0, 1));
		K(EDCShape::Cube, B + FVector(-98, 0, 100), FVector(4, 74, 50), "Steel");
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector L((i % 2) ? 90.f : -90.f, (i / 2) ? 32.f : -32.f, 0);
			KTube(B + L + FVector(0, 0, 10), B + L + FVector(0, 0, 68), 3, "Steel");
			K(EDCShape::Sphere, B + L + FVector(0, 0, 6), FVector(10), "Rubber");
		}
		// IV stand + bag + line
		KTube(B + FVector(-100, -50, 0), B + FVector(-100, -50, 195), 2.5f, "Chrome");
		K(EDCShape::Cube, B + FVector(-100, -50, 178), FVector(10, 4, 22), "Plastic", FRotator::ZeroRotator, false);
		KTube(B + FVector(-100, -50, 166), B + FVector(-60, -25, 92), 0.6f, "PlasticWhite");
		K(EDCShape::Cylinder, B + FVector(-100, -50, 2), FVector(40, 40, 3), "Chrome");
		// instrument trolley + vitals monitor on a pole
		K(EDCShape::Cube, B + FVector(100, -60, 90), FVector(44, 32, 2), "Steel");
		K(EDCShape::Cube, B + FVector(100, -60, 40), FVector(44, 32, 2), "Steel");
		KTube(B + FVector(100, -60, 0), B + FVector(100, -60, 89), 2, "Steel");
		for (int32 i = 0; i < 4; ++i) { KTube(B + FVector(88 + i * 7.f, -67, 91.5f), B + FVector(90 + i * 7.f, -53, 91.5f), 0.8f, "Chrome"); }
		KTube(B + FVector(-60, 45, 0), B + FVector(-60, 45, 150), 3.f, "Chrome");
		Monitor(B + FVector(-60, 45, 158), 36, 26, -150.f, "Screen1", 0.f);
		// oxygen bottle
		K(EDCShape::Cylinder, B + FVector(-120, 40, 60), FVector(20, 20, 120), "PaintGrey");
		K(EDCShape::Cylinder, B + FVector(-120, 40, 124), FVector(8, 8, 10), "Chrome");
	}
	bKitOnStation = false;
	// surgical lamp (shadowed, the clinic's key)
	Tube(FVector(80, 60, 380), FVector(80, 60, 240), 3, "PlasticWhite");
	Deco(EDCShape::Sphere, FVector(80, 60, 228), FVector(74, 74, 30), "PlasticWhite");
	Deco(EDCShape::Cylinder, FVector(80, 60, 214), FVector(56, 56, 1.5f), "LedCool", FRotator::ZeroRotator, false);
	Spot(FVector(80, 60, 205), FVector(80, 60, 0), FLinearColor(0.88f, 0.95f, 1.f), 90.f, 600.f, 25.f, 48.f, true, true);
	LedBatten(FVector(-180, -60, 310), 140.f, 90.f, true, 30.f);
	// medical cabinet + sink under the windows
	Deco(EDCShape::Cube, FVector(-220, 255, 75), FVector(160, 40, 150), "PlasticWhite");
	Deco(EDCShape::Cube, FVector(-220, 234, 105), FVector(150, 2, 84), "Glass", FRotator::ZeroRotator, false);
	for (int32 i = 0; i < 9; ++i) { Deco(EDCShape::Cylinder, FVector(-270 + (i % 3) * 45.f, 252, 72 + (i / 3) * 34.f), FVector(10, 10, 18), (i % 2) ? FName("PlasticWhite") : FName("Bottle")); }
	Deco(EDCShape::Cube, FVector(-60, 260, 45), FVector(90, 50, 90), "Steel");
	Deco(EDCShape::Cube, FVector(-60, 258, 88), FVector(60, 36, 4), "Black");
	Deco(EDCShape::Cube, FVector(250, 260, 30), FVector(36, 36, 60), "PaintRed");    // sharps / biohazard bin
	CurGroup = DCGroups::Level2;
	// privacy curtain on a ceiling rail + second gurney
	Tube(FVector(-120, -250, 240), FVector(-120, 200, 240), 2.5f, "Alu");
	for (int32 i = 0; i < 5; ++i) { Deco(EDCShape::Cube, FVector(-120 + J(i, 6), -200 + i * 40.f, 140), FVector(3, 42, 200), "BlanketGrey", FRotator(0, -20 + (i % 2) * 40.f, 0)); }
	Deco(EDCShape::Cube, FVector(-280, -150, 74), FVector(70, 180, 14), "PlasticWhite");
	Deco(EDCShape::Cube, FVector(-280, -150, 40), FVector(60, 170, 4), "Steel");
	CurGroup = DCGroups::Level3;
	Deco(EDCShape::Cube, FVector(330, -180, 90), FVector(60, 70, 180), "PlasticWhite");          // blood fridge
	Deco(EDCShape::Cube, FVector(299, -180, 100), FVector(2, 60, 150), "Glass", FRotator::ZeroRotator, false);
	for (int32 i = 0; i < 8; ++i) { Deco(EDCShape::Cube, FVector(320, -205 + (i % 4) * 16.f, 60 + (i / 4) * 50.f), FVector(10, 12, 18), "Pin"); }
	Point(FVector(300, -180, 120), FLinearColor(0.8f, 0.9f, 1.f), 4.f, 300.f, false, true, 10.f);

	LockedDressing(R, { {80, 60, 220, 110}, {-220, 255, 150, 150} });
	BuildingDressing(R);
}

// =====================================================================================================================
// SERVER ROOM  (centre (-900,0); x[-400,400] y[-400,400]; raised floor; skylight)
// =====================================================================================================================
void ADCHideoutBuilder::BuildServerRoom()
{
	CurRoom = GetRoom(EDCRoomType::ServerRoom);
	const FBox2D R(FVector2D(-400, -400), FVector2D(400, 400));
	CurGroup = DCGroups::Shell;
	ShaftFog(FVector(0, 0, 170), FVector(380, 380, 180), 0.22f);
	CurGroup = DCGroups::Level1;
	auto RackAt = [&](const FVector& P, int32 Seed)
	{
		static const FName Leds[] = { "LedG", "LedB", "LedG", "LedA", "LedG", "LedB" };
		K(EDCShape::Cube, P + FVector(0, 0, 105), FVector(64, 78, 210), "Rack");
		K(EDCShape::Cube, P + FVector(-32.5f, 0, 105), FVector(1, 70, 200), "Fence");
		for (int32 u = 0; u < 10; ++u)
		{
			K(EDCShape::Cube, P + FVector(-30.f, 0, 20 + u * 19.f), FVector(1, 66, 14), "SteelBlack");
			for (int32 l = 0; l < 3; ++l) { K(EDCShape::Cube, P + FVector(-31.f, -26 + l * 6.f, 20 + u * 19.f), FVector(1, 2, 2), Leds[(Seed + u * 3 + l) % 6], FRotator::ZeroRotator, false); }
		}
	};
	BeginStation(EDCStationType::ServerRack, TEXT("Server Room"), FVector(220, 0, 0), FVector(50, 130, 110));
	bKitOnStation = true;
	RackAt(FVector(220, -85, 0), 1);
	RackAt(FVector(220, 0, 0), 2);
	RackAt(FVector(220, 85, 0), 3);
	KTube(FVector(220, -85, 210), FVector(220, -85, 300), 6, "Rubber");
	KTube(FVector(220, 85, 210), FVector(220, 85, 300), 6, "Rubber");
	bKitOnStation = false;
	// overhead cable tray
	Deco(EDCShape::Cube, FVector(220, 0, 302), FVector(40, 760, 6), "Galv");
	for (float Y : { -380.f, 0.f, 380.f }) { Tube(FVector(220, Y, 305), FVector(220, Y, 380), 1.5f, "Galv"); }
	for (int32 i = 0; i < 6; ++i) { Deco(EDCShape::Cylinder, FVector(205 + i * 6.f, 0, 308), FVector(3, 3, 740), (i % 2) ? FName("PaintBlue") : FName("Rubber"), FRotator(90, 90, 0)); }
	// chain-link cage in front of the racks
	{
		const float X = 140.f, H = 250.f;
		Deco(EDCShape::Cube, FVector(X, -150, H * 0.5f), FVector(1.f, 260, H), "Fence");
		Deco(EDCShape::Cube, FVector(X, 250, H * 0.5f), FVector(1.f, 260, H), "Fence");
		for (float Y : { -280.f, -20.f, 120.f, 380.f }) { Tube(FVector(X, Y, 0), FVector(X, Y, H), 5.f, "Galv"); }
		Tube(FVector(X, -280, H), FVector(X, 380, H), 4.f, "Galv");
		// gate swung open
		Deco(EDCShape::Cube, FVector(X - 60, -20 + 20, H * 0.45f), FVector(120.f, 1.f, H * 0.9f), "Fence", FRotator(0, -70, 0));
	}
	// operator desk
	Deco(EDCShape::Cube, FVector(-180, 300, 74), FVector(140, 70, 4), "PaintGrey");
	for (int32 i = 0; i < 2; ++i) { Deco(EDCShape::Cube, FVector(-180 + (i ? 66.f : -66.f), 300, 37), FVector(4, 66, 74), "SteelBlack"); }
	Monitor(FVector(-200, 320, 100), 50, 30, 90.f, "Screen1");
	Monitor(FVector(-145, 320, 100), 50, 30, 90.f, "Screen0");
	OfficeChair(FVector(-180, 250, 0), 90.f);
	Deco(EDCShape::Cube, FVector(-340, -300, 90), FVector(60, 80, 180), "PlasticWhite");      // CRAC unit
	for (int32 i = 0; i < 5; ++i) { Deco(EDCShape::Cube, FVector(-371, -300, 120 + i * 10.f), FVector(1, 60, 3), "Black"); }
	Deco(EDCShape::Cube, FVector(-372, -300, 60), FVector(1, 10, 6), "LedG", FRotator::ZeroRotator, false);
	LedBatten(FVector(0, 0, 320), 180.f, 90.f, true, 30.f);
	Rect(FVector(130, 0, 200), FVector(-200, 0, 60), FLinearColor(0.3f, 0.6f, 1.f), 8.f, 30.f, 200.f, 600.f, false, true);
	CurGroup = DCGroups::Level2;
	RackAt(FVector(220, -260, 0), 4);
	RackAt(FVector(220, 260, 0), 5);
	Deco(EDCShape::Cube, FVector(220, -340, 40), FVector(60, 40, 80), "PlasticBlack");          // UPS
	CurGroup = DCGroups::Level3;
	RackAt(FVector(220, -175, 0), 6);
	RackAt(FVector(220, 175, 0), 7);
	Deco(EDCShape::Cube, FVector(190, 0, 1), FVector(4, 760, 2), "Accent", FRotator::ZeroRotator, false);

	LockedDressing(R, { {220, 0, 120, 220}, {-180, 300, 130, 110} });
	BuildingDressing(R);
}

// =====================================================================================================================
// LAWYER  (centre (-900,700); x[-400,400] y[-300,300]; carpet tiles; skylight)
// =====================================================================================================================
void ADCHideoutBuilder::BuildLawyer()
{
	CurRoom = GetRoom(EDCRoomType::LawyerOffice);
	const FBox2D R(FVector2D(-400, -300), FVector2D(400, 300));
	CurGroup = DCGroups::Shell;
	ShaftFog(FVector(0, 0, 170), FVector(380, 280, 180), 0.2f);
	CurGroup = DCGroups::Level1;
	BeginStation(EDCStationType::Lawyer, TEXT("Lawyer"), FVector(150, 30, 0), FVector(120, 200, 75));
	bKitOnStation = true;
	{
		const FVector D(150, 30, 0);
		K(EDCShape::Cube, D + FVector(0, 0, 74), FVector(95, 190, 5), "Walnut");
		for (float S : { -1.f, 1.f }) { K(EDCShape::Cube, D + FVector(0, S * 88.f, 36), FVector(85, 5, 72), "SteelBlack"); }
		K(EDCShape::Cube, D + FVector(-40, 0, 46), FVector(4, 170, 50), "Walnut");
		K(EDCShape::Cube, D + FVector(10, -10, 77.5f), FVector(24, 34, 1.6f), "Alu");
		K(EDCShape::Cube, D + FVector(0, -10, 88), FVector(1.4f, 34, 22), "Alu", FRotator(-18, 0, 0));
		K(EDCShape::Cube, D + FVector(1.2f, -10, 88.4f), FVector(0.3f, 31, 19), "Laptop", FRotator(-18, 0, 0), false);
		for (int32 i = 0; i < 5; ++i) { K(EDCShape::Cube, D + FVector(-10 + J(i, 4), 50, 78 + i * 2.2f), FVector(26, 34, 2), (i % 2) ? FName("Paper") : FName("Cardboard"), FRotator(0, J(i, 10), 0)); }
		K(EDCShape::Cylinder, D + FVector(15, 70, 78), FVector(12, 12, 3), "SteelBlack");
		KTube(D + FVector(15, 70, 78), D + FVector(8, 64, 115), 1.6f, "SteelBlack");
		K(EDCShape::Cone, D + FVector(0, 58, 112), FVector(18, 18, 12), "SteelBlack");
		K(EDCShape::Cylinder, D + FVector(0, 58, 106.5f), FVector(11, 11, 0.6f), "LedWarm", FRotator::ZeroRotator, false);
		// executive chair
		K(EDCShape::Cube, D + FVector(72, 0, 48), FVector(56, 56, 12), "Leather");
		K(EDCShape::Cube, D + FVector(100, 0, 92), FVector(12, 56, 80), "Leather", FRotator(-8, 0, 0));
		KTube(D + FVector(72, 0, 0), D + FVector(72, 0, 42), 6, "Chrome");
		// 4-drawer filing cabinets along the +Y partition
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector C(-110 + i * 52.f, 228, 0);
			K(EDCShape::Cube, C + FVector(0, 0, 68), FVector(48, 62, 136), "PaintGrey");
			for (int32 d = 0; d < 4; ++d)
			{
				K(EDCShape::Cube, C + FVector(0, -31.5f, 17 + d * 33.f), FVector(44, 1, 29), "PaintGrey");
				K(EDCShape::Cube, C + FVector(0, -33, 26 + d * 33.f), FVector(12, 2, 2.5f), "Alu");
			}
		}
		K(EDCShape::Cube, D + FVector(-180 + 30, 198, 140), FVector(40, 30, 8), "Paper", FRotator(0, 10, 0));
	}
	bKitOnStation = false;
	Spot(FVector(150, 88, 105), FVector(140, 40, 0), Warm, 30.f, 500.f, 30.f, 66.f, false, true);
	Pendant(FVector(60, 30, 260), true, true, 22.f);
	// client chairs, rug, plant, bookshelf
	Deco(EDCShape::Cube, FVector(80, 30, 0.6f), FVector(300, 340, 1.2f), "Fabric");
	for (int32 i = 0; i < 2; ++i)
	{
		const FVector C(20, -30 + i * 110.f, 0);
		Deco(EDCShape::Cube, C + FVector(0, 0, 44), FVector(52, 52, 10), "LeatherBrn");
		Deco(EDCShape::Cube, C + FVector(-24, 0, 76), FVector(8, 54, 56), "LeatherBrn");
		for (int32 l = 0; l < 4; ++l) { Tube(C + FVector((l % 2) ? 20.f : -20.f, (l / 2) ? 20.f : -20.f, 0), C + FVector((l % 2) ? 20.f : -20.f, (l / 2) ? 20.f : -20.f, 40), 2.5f, "SteelBlack"); }
	}
	Deco(EDCShape::Cylinder, FVector(340, -250, 25), FVector(40, 40, 50), "PlasticBlack");
	for (int32 i = 0; i < 7; ++i) { Deco(EDCShape::Sphere, FVector(340 + J(i, 15), -250 + J(i + 3, 15), 70 + i * 12.f), FVector(40 - i * 3.f, 40 - i * 3.f, 28), "Plant"); }
	CurGroup = DCGroups::Level2;
	Deco(EDCShape::Cube, FVector(375, -100, 95), FVector(36, 200, 190), "SteelBlack");
	for (int32 s = 0; s < 4; ++s)
	{
		float Y = -195.f; int32 k = s * 40;
		while (Y < -5.f)
		{
			const float W = 6.f + FMath::Abs(J(k, 2.f));
			static const FName Binders[] = { "PlasticBlack", "PaintBlue", "Paper", "PlasticBlack", "PaintRed" };
			Deco(EDCShape::Cube, FVector(358, Y + W * 0.5f, 20 + s * 45.f + 16), FVector(28, W, 30 + J(k + 1, 3)), Binders[k % 5]);
			Y += W + 0.5f; ++k;
		}
	}
	Crate(FVector(-300, 150, 0), FVector(70, 50, 40), 6.f, "Cardboard");  // evidence boxes
	Crate(FVector(-300, 150, 40), FVector(70, 50, 40), -4.f, "Cardboard");
	Crate(FVector(-230, 170, 0), FVector(70, 50, 40), 12.f, "Cardboard");
	CurGroup = DCGroups::Level3;
	Deco(EDCShape::Cube, FVector(-300, -150, 45), FVector(70, 180, 40), "Leather");           // leather sofa
	Deco(EDCShape::Cube, FVector(-330, -150, 75), FVector(16, 180, 40), "Leather");
	Deco(EDCShape::Cube, FVector(-200, -150, 22), FVector(60, 60, 44), "Walnut");             // side table + decanter
	Deco(EDCShape::Cylinder, FVector(-200, -150, 56), FVector(10, 10, 24), "Bottle");
	Point(FVector(-250, -150, 200), Warm, 8.f, 500.f, false, true, 10.f);

	LockedDressing(R, { {150, 30, 140, 90}, {-30, 228, 220, 140} });
	BuildingDressing(R);
}

// =====================================================================================================================
// STREET  (a dark street strip beyond the garage's roller door; built into the Office room's Shell group, whose
// local space == world). Lamps are owned/driven by ADCDayNight (on at dusk).
// =====================================================================================================================
void ADCHideoutBuilder::BuildYard()
{
	CurRoom = GetRoom(EDCRoomType::Office);
	CurGroup = DCGroups::Shell;
	Deco(EDCShape::Cube, FVector(1560, -500, -18), FVector(520, 2300, 30), "Asphalt");
	Deco(EDCShape::Cube, FVector(1560, -500, -80), FVector(500, 2280, 100), "Plinth");
	Deco(EDCShape::Cube, FVector(1330, -500, -8), FVector(60, 2300, 16), "Concrete");     // kerb / pavement
	for (int32 i = 0; i < 8; ++i) { Deco(EDCShape::Cube, FVector(1600, -1500 + i * 300.f, -2.5f), FVector(10, 150, 1), "PaintLight", FRotator::ZeroRotator, false); }
	Deco(EDCShape::Cylinder, FVector(1480, -300, -2.6f), FVector(200, 130, 1), "TintGlass", FRotator(0, 20, 0), false); // puddle
	// dumpster + bags against the building, bollards either side of the roller door
	Deco(EDCShape::Cube, FVector(1400, 150, 60), FVector(110, 180, 120), "PaintBlue");
	Deco(EDCShape::Cube, FVector(1400, 150, 124), FVector(116, 186, 6), "PlasticBlack", FRotator(4, 0, 0));
	for (int32 i = 0; i < 4; ++i) { Deco(EDCShape::Sphere, FVector(1390 + J(i, 30), 290 + J(i + 4, 40), 22), FVector(50, 45, 44), "Black"); }
	for (float Y : { -1130.f, -670.f }) { Deco(EDCShape::Cylinder, FVector(1350, Y, 50), FVector(18, 18, 100), "Hazard"); }
	// wall pack over the roller door, two street lamps
	Deco(EDCShape::Cube, FVector(1312, -900, 330), FVector(16, 34, 20), "SteelBlack");
	Deco(EDCShape::Cube, FVector(1320.5f, -900, 325), FVector(1, 28, 10), "LampHead", FRotator::ZeroRotator, false);
	StreetSpecs.Add({ FVector(1335, -900, 320), FVector(1550, -900, 0), FLinearColor(0.85f, 0.92f, 1.f), 700.f, 1400.f, 65.f, false });
	for (const FVector& P : { FVector(1700, -1620, 0), FVector(1760, 200, 0) })
	{
		Tube(P, P + FVector(0, 0, 560), 12.f, "PaintGrey");
		Tube(P + FVector(0, 0, 550), P + FVector(-140, 0, 570), 7.f, "PaintGrey");
		Deco(EDCShape::Cube, P + FVector(-150, 0, 565), FVector(56, 26, 10), "PaintGrey");
		Deco(EDCShape::Cube, P + FVector(-150, 0, 559), FVector(46, 18, 2), "LampHead", FRotator::ZeroRotator, false);
		StreetSpecs.Add({ P + FVector(-150, 0, 550), P + FVector(-200, 0, 0), FLinearColor(1.f, 0.66f, 0.35f), 900.f, 2000.f, 70.f, P.Y > 0.f });
	}
}
