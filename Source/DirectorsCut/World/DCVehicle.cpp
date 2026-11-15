#include "World/DCVehicle.h"
#include "Core/DCStyle.h"
#include "Core/DCAssetPaths.h"
#include "Core/DCSiteLayout.h"
#include "Core/DCRunSubsystem.h"
#include "Core/DCSettings.h"
#include "DirectorsCut.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	constexpr float Cruise = 2300.f;     // cm/s on the lane
	constexpr float YardSpeed = 650.f;   // cm/s cap near the house
	constexpr float ReverseSpeed = 230.f;
	constexpr float Accel = 520.f;       // cm/s^2
	constexpr float Decel = 820.f;
	constexpr float LatAccel = 520.f;
	constexpr float MinCrawl = 45.f;     // profile floor so the car always creeps forward
	constexpr float OffscreenX = 7000.f; // departures vanish past this, arrivals start here

	template <typename T>
	T* LoadQuiet(const TCHAR* Path) { return LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet); }

	/** Centripetal Catmull-Rom between P1 and P2. */
	FVector CatmullRom(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float T)
	{
		auto Knot = [](float Ti, const FVector& A, const FVector& B) { return Ti + FMath::Max(1.f, FMath::Sqrt(FVector::Dist(A, B))); };
		const float T0 = 0.f, T1 = Knot(T0, P0, P1), T2 = Knot(T1, P1, P2), T3 = Knot(T2, P2, P3);
		const float U = FMath::Lerp(T1, T2, T);
		const FVector A1 = (T1 - U) / (T1 - T0) * P0 + (U - T0) / (T1 - T0) * P1;
		const FVector A2 = (T2 - U) / (T2 - T1) * P1 + (U - T1) / (T2 - T1) * P2;
		const FVector A3 = (T3 - U) / (T3 - T2) * P2 + (U - T2) / (T3 - T2) * P3;
		const FVector B1 = (T2 - U) / (T2 - T0) * A1 + (U - T0) / (T2 - T0) * A2;
		const FVector B2 = (T3 - U) / (T3 - T1) * A2 + (U - T1) / (T3 - T1) * A3;
		return (T2 - U) / (T2 - T1) * B1 + (U - T1) / (T2 - T1) * B2;
	}

	const FLinearColor Paints[] = {
		FLinearColor(0.30f, 0.035f, 0.03f),  // oxblood
		FLinearColor(0.035f, 0.13f, 0.075f), // bottle green
		FLinearColor(0.03f, 0.055f, 0.14f),  // navy
		FLinearColor(0.58f, 0.52f, 0.40f),   // cream
		FLinearColor(0.46f, 0.29f, 0.05f),   // mustard
		FLinearColor(0.05f, 0.05f, 0.055f),  // graphite
		FLinearColor(0.18f, 0.32f, 0.42f),   // faded sky
		FLinearColor(0.62f, 0.62f, 0.60f),   // white
	};
}

ADCVehicle::ADCVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	BodyPivot = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	BodyPivot->SetupAttachment(Root);
}

UMaterialInstanceDynamic* ADCVehicle::MakeMID(const TCHAR* Path, const FLinearColor& Color, float A, float B)
{
	UMaterialInterface* Base = LoadQuiet<UMaterialInterface>(Path);
	const bool bBasic = !Base;
	if (!Base) { Base = LoadQuiet<UMaterialInterface>(DCPaths::BasicShapeMaterial); }
	if (!Base) { return nullptr; }
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
	if (bBasic) { M->SetVectorParameterValue(TEXT("Color"), Color); Owned.Add(M); return M; }
	if (Path == DCPaths::MatSurface)
	{
		M->SetVectorParameterValue(TEXT("BaseColor"), DCStyle::Tone(Color));
		M->SetScalarParameterValue(TEXT("Roughness"), A);
		M->SetScalarParameterValue(TEXT("Metallic"), B);
	}
	else if (Path == DCPaths::MatEmissive)
	{
		M->SetVectorParameterValue(TEXT("Color"), Color);
		M->SetScalarParameterValue(TEXT("Strength"), A);
	}
	else
	{
		M->SetVectorParameterValue(TEXT("Color"), Color);
		M->SetScalarParameterValue(TEXT("Opacity"), A);
		M->SetScalarParameterValue(TEXT("Roughness"), B);
	}
	Owned.Add(M);
	return M;
}

UStaticMeshComponent* ADCVehicle::Part(USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Loc, const FVector& Size, UMaterialInterface* Mat, const FRotator& Rot, bool bShadow)
{
	if (!Mesh) { return nullptr; }
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(Parent);
	C->SetRelativeLocation(Loc);
	C->SetRelativeRotation(Rot);
	C->SetRelativeScale3D(Size / 100.f);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(bShadow);
	if (Mat) { C->SetMaterial(0, Mat); }
	C->RegisterComponent();
	AddInstanceComponent(C);
	return C;
}

void ADCVehicle::Setup(int32 Seed)
{
	BuildBody(Seed);
	SetActorTickEnabled(true);
}

void ADCVehicle::BuildBody(int32 Seed)
{
	FRandomStream R(0xCA2 + Seed * 7919);
	Style = (EDCCarStyle)(Seed % 4 == 0 ? 0 : Seed % 4 == 1 ? 1 : Seed % 4 == 2 ? 3 : 2); // hatch, estate, van, pickup, ...
	const FLinearColor Paint = Paints[(Seed * 5 + 3) % UE_ARRAY_COUNT(Paints)];

	UStaticMesh* Cube = LoadQuiet<UStaticMesh>(DCPaths::Cube);
	UStaticMesh* Cyl = LoadQuiet<UStaticMesh>(DCPaths::Cylinder);
	UMaterialInstanceDynamic* MPaint = MakeMID(DCPaths::MatSurface, Paint, 0.28f, 0.35f);
	UMaterialInstanceDynamic* MTrim = MakeMID(DCPaths::MatSurface, FLinearColor(0.018f, 0.018f, 0.02f), 0.6f, 0.f);
	UMaterialInstanceDynamic* MChrome = MakeMID(DCPaths::MatSurface, FLinearColor(0.5f, 0.5f, 0.52f), 0.22f, 1.f);
	UMaterialInstanceDynamic* MTyre = MakeMID(DCPaths::MatSurface, FLinearColor(0.012f, 0.012f, 0.012f), 0.92f, 0.f);
	UMaterialInstanceDynamic* MHub = MakeMID(DCPaths::MatSurface, FLinearColor(0.32f, 0.32f, 0.33f), 0.35f, 0.9f);
	// Tinted windows: opaque, near-black and glossy (reads as glass from above, no sorting issues).
	UMaterialInstanceDynamic* MGlass = MakeMID(DCPaths::MatSurface, FLinearColor(0.006f, 0.009f, 0.013f), 0.06f, 0.f);
	if (MGlass) { MGlass->SetScalarParameterValue(TEXT("Specular"), 0.9f); }
	HeadMID = MakeMID(DCPaths::MatEmissive, FLinearColor(1.f, 0.9f, 0.72f), 0.5f, 0.f);
	TailMID = MakeMID(DCPaths::MatEmissive, FLinearColor(1.f, 0.03f, 0.02f), 0.4f, 0.f);

	if (BuildModel(Seed)) { return; }

	// Dimensions per style (local: +X nose, +Y right, Z up, origin = ground centre).
	float BodyTop = 88.f, CabX0 = -120.f, CabX1 = 70.f, CabTop = 146.f, DoorX0 = -55.f, DoorX1 = 60.f;
	switch (Style)
	{
	case EDCCarStyle::Hatch:  Length = 380.f; Width = 166.f; WheelBase = 245.f; CabX0 = -150.f; CabX1 = 60.f; break;
	case EDCCarStyle::Estate: Length = 440.f; Width = 170.f; WheelBase = 280.f; CabX0 = -205.f; CabX1 = 70.f; DoorX0 = -70.f; DoorX1 = 65.f; break;
	case EDCCarStyle::Pickup: Length = 455.f; Width = 174.f; WheelBase = 300.f; CabX0 = -20.f; CabX1 = 95.f; CabTop = 152.f; DoorX0 = -15.f; DoorX1 = 90.f; BodyTop = 92.f; WheelR = 34.f; break;
	case EDCCarStyle::Van:    Length = 450.f; Width = 176.f; WheelBase = 290.f; CabX0 = -225.f; CabX1 = 120.f; CabTop = 205.f; DoorX0 = 40.f; DoorX1 = 135.f; BodyTop = 100.f; WheelR = 33.f; break;
	}
	const float HL = Length * 0.5f, HW = Width * 0.5f;
	const float Floor = 30.f;

	// Lower body + skirts + bumpers.
	Part(BodyPivot, Cube, FVector(0, 0, Floor + 6.f), FVector(Length - 30.f, Width - 10.f, 20.f), MTrim);
	if (Style == EDCCarStyle::Pickup)
	{
		Part(BodyPivot, Cube, FVector(0, 0, (Floor + 62.f) * 0.5f + 4.f), FVector(Length - 8.f, Width, 62.f - Floor), MPaint);
		const float FrontLen = HL - (-25.f);
		Part(BodyPivot, Cube, FVector(-25.f + FrontLen * 0.5f - 4.f, 0, (62.f + BodyTop) * 0.5f), FVector(FrontLen - 8.f, Width, BodyTop - 62.f), MPaint);
		// Bed walls + tailgate + bed floor.
		const float BedX0 = -HL + 4.f, BedX1 = -30.f, BedLen = BedX1 - BedX0;
		Part(BodyPivot, Cube, FVector((BedX0 + BedX1) * 0.5f, -HW + 5.f, 80.f), FVector(BedLen, 10.f, 36.f), MPaint);
		Part(BodyPivot, Cube, FVector((BedX0 + BedX1) * 0.5f, HW - 5.f, 80.f), FVector(BedLen, 10.f, 36.f), MPaint);
		Part(BodyPivot, Cube, FVector(BedX0 + 5.f, 0, 80.f), FVector(10.f, Width - 10.f, 36.f), MPaint);
		Part(BodyPivot, Cube, FVector((BedX0 + BedX1) * 0.5f, 0, 64.f), FVector(BedLen, Width - 20.f, 4.f), MTrim);
		// A tarp-covered load in the bed.
		Part(BodyPivot, Cube, FVector((BedX0 + BedX1) * 0.5f + 10.f, 0, 78.f), FVector(BedLen * 0.6f, Width * 0.6f, 26.f), MakeMID(DCPaths::MatSurface, FLinearColor(0.09f, 0.11f, 0.06f), 0.9f, 0.f), FRotator(0, 4, 0));
	}
	else
	{
		Part(BodyPivot, Cube, FVector(0, 0, (Floor + BodyTop) * 0.5f + 4.f), FVector(Length - 8.f, Width, BodyTop - Floor - 8.f), MPaint);
	}
	Part(BodyPivot, Cube, FVector(HL - 3.f, 0, Floor + 12.f), FVector(10.f, Width + 4.f, 18.f), MTrim);
	Part(BodyPivot, Cube, FVector(-HL + 3.f, 0, Floor + 12.f), FVector(10.f, Width + 4.f, 18.f), MTrim);
	// Grille + chrome strip.
	Part(BodyPivot, Cube, FVector(HL - 1.f, 0, Floor + 34.f), FVector(4.f, Width * 0.42f, 16.f), MTrim);
	Part(BodyPivot, Cube, FVector(0, -HW - 0.5f, Floor + 30.f), FVector(Length - 40.f, 2.f, 3.f), MChrome);
	Part(BodyPivot, Cube, FVector(0, HW + 0.5f, Floor + 30.f), FVector(Length - 40.f, 2.f, 3.f), MChrome);

	// Cabin: tinted glass box with a painted roof + pillars.
	const float CabW = Width - 14.f;
	const float CabLen = CabX1 - CabX0;
	const float GlassZ0 = BodyTop - 2.f, GlassH = CabTop - BodyTop - 6.f;
	Part(BodyPivot, Cube, FVector((CabX0 + CabX1) * 0.5f, 0, GlassZ0 + GlassH * 0.5f), FVector(CabLen, CabW, GlassH), MGlass);
	if (Style == EDCCarStyle::Van)
	{
		// Panel van: solid painted load box behind the cab glass.
		const float BoxX1 = CabX1 - 105.f;
		Part(BodyPivot, Cube, FVector((CabX0 + BoxX1) * 0.5f, 0, GlassZ0 + GlassH * 0.5f), FVector(BoxX1 - CabX0 + 2.f, Width, GlassH + 2.f), MPaint);
		// Low hood in front of a tall cab.
		Part(BodyPivot, Cube, FVector((CabX1 + HL) * 0.5f, 0, BodyTop + 2.f), FVector(HL - CabX1, Width - 6.f, 6.f), MPaint);
		// Side stripe.
		Part(BodyPivot, Cube, FVector((CabX0 + BoxX1) * 0.5f, -HW - 0.6f, 128.f), FVector(BoxX1 - CabX0 - 20.f, 2.f, 10.f), MakeMID(DCPaths::MatSurface, Paint * 0.35f + FLinearColor(0.1f, 0.08f, 0.04f), 0.4f, 0.2f));
		Part(BodyPivot, Cube, FVector((CabX0 + BoxX1) * 0.5f, HW + 0.6f, 128.f), FVector(BoxX1 - CabX0 - 20.f, 2.f, 10.f), MakeMID(DCPaths::MatSurface, Paint * 0.35f + FLinearColor(0.1f, 0.08f, 0.04f), 0.4f, 0.2f));
	}
	Part(BodyPivot, Cube, FVector((CabX0 + CabX1) * 0.5f - 6.f, 0, CabTop - 3.f), FVector(CabLen - 30.f, CabW - 14.f, 7.f), MPaint);
	for (int32 s = -1; s <= 1; s += 2)
	{
		const float Y = s * (CabW * 0.5f - 2.f);
		Part(BodyPivot, Cube, FVector(CabX1 - 5.f, Y, GlassZ0 + GlassH * 0.5f), FVector(8.f, 6.f, GlassH), MPaint);   // A pillar
		Part(BodyPivot, Cube, FVector(CabX0 + 6.f, Y, GlassZ0 + GlassH * 0.5f), FVector(12.f, 6.f, GlassH), MPaint);  // C pillar
		if (Style == EDCCarStyle::Hatch || Style == EDCCarStyle::Estate)
		{
			Part(BodyPivot, Cube, FVector(DoorX0 - 4.f, Y, GlassZ0 + GlassH * 0.5f), FVector(7.f, 6.f, GlassH), MPaint); // B pillar
		}
	}
	if (Style == EDCCarStyle::Estate)
	{
		// Roof rails.
		for (int32 s = -1; s <= 1; s += 2) { Part(BodyPivot, Cube, FVector((CabX0 + CabX1) * 0.5f - 10.f, s * (CabW * 0.5f - 12.f), CabTop + 3.f), FVector(CabLen - 50.f, 4.f, 4.f), MChrome); }
	}

	// Lights (emissive lenses).
	const float LampZ = Style == EDCCarStyle::Van ? BodyTop - 22.f : BodyTop - 20.f;
	for (int32 s = -1; s <= 1; s += 2)
	{
		Part(BodyPivot, Cube, FVector(HL + 0.5f, s * (HW - 24.f), LampZ), FVector(4.f, 30.f, 12.f), HeadMID, FRotator::ZeroRotator, false);
		Part(BodyPivot, Cube, FVector(-HL - 0.5f, s * (HW - 18.f), LampZ + (Style == EDCCarStyle::Pickup ? 0.f : 4.f)), FVector(4.f, 24.f, 14.f), TailMID, FRotator::ZeroRotator, false);
	}
	// Number plates.
	UMaterialInstanceDynamic* MPlate = MakeMID(DCPaths::MatSurface, FLinearColor(0.6f, 0.58f, 0.45f), 0.5f, 0.f);
	Part(BodyPivot, Cube, FVector(HL + 1.f, 0, Floor + 14.f), FVector(2.f, 44.f, 10.f), MPlate, FRotator::ZeroRotator, false);
	Part(BodyPivot, Cube, FVector(-HL - 1.f, 0, Floor + 30.f), FVector(2.f, 44.f, 10.f), MPlate, FRotator::ZeroRotator, false);
	// Wing mirrors.
	for (int32 s = -1; s <= 1; s += 2) { Part(BodyPivot, Cube, FVector(CabX1 - 12.f, s * (HW + 9.f), BodyTop + 10.f), FVector(10.f, 14.f, 10.f), MTrim); }

	// Doors (front doors): painted panel hinged at its front edge, slightly proud of the body.
	DoorHinge.Reset();
	for (int32 s = -1; s <= 1; s += 2)
	{
		USceneComponent* H = NewObject<USceneComponent>(this);
		H->SetupAttachment(BodyPivot);
		H->SetRelativeLocation(FVector(DoorX1, s * (HW + 1.5f), 0.f));
		H->RegisterComponent();
		AddInstanceComponent(H);
		const float DL = DoorX1 - DoorX0;
		Part(H, Cube, FVector(-DL * 0.5f, 0, (Floor + 10.f + BodyTop) * 0.5f), FVector(DL, 3.f, BodyTop - Floor - 12.f), MPaint);
		Part(H, Cube, FVector(-DL + 10.f, s * 2.5f, BodyTop - 18.f), FVector(10.f, 2.f, 3.f), MChrome, FRotator::ZeroRotator, false); // handle
		Part(H, Cube, FVector(-DL * 0.5f, 0, BodyTop + GlassH * 0.45f), FVector(DL - 6.f, 2.f, GlassH * 0.9f), MGlass, FRotator::ZeroRotator, false);
		DoorHinge.Add(H);
	}

	// Wheels: steer (yaw) -> spin (pitch) -> tyre + hub + spoke bar.
	WheelSteer.Reset(); WheelSpin.Reset();
	const float WX[4] = { WheelBase * 0.5f, WheelBase * 0.5f, -WheelBase * 0.5f, -WheelBase * 0.5f };
	const float WY[4] = { -(HW - 20.f), HW - 20.f, -(HW - 20.f), HW - 20.f };
	for (int32 i = 0; i < 4; ++i)
	{
		USceneComponent* S = NewObject<USceneComponent>(this);
		S->SetupAttachment(Root);
		S->SetRelativeLocation(FVector(WX[i], WY[i], WheelR));
		S->RegisterComponent();
		AddInstanceComponent(S);
		USceneComponent* Sp = NewObject<USceneComponent>(this);
		Sp->SetupAttachment(S);
		Sp->RegisterComponent();
		AddInstanceComponent(Sp);
		const float Out = WY[i] > 0.f ? 1.f : -1.f;
		Part(Sp, Cyl, FVector::ZeroVector, FVector(WheelR * 2.f, WheelR * 2.f, 24.f), MTyre, FRotator(0, 0, 90));
		Part(Sp, Cyl, FVector(0, Out * 3.f, 0), FVector(WheelR * 1.15f, WheelR * 1.15f, 21.f), MHub, FRotator(0, 0, 90), false);
		Part(Sp, Cube, FVector(0, Out * 11.f, 0), FVector(WheelR * 1.05f, 2.f, 6.f), MTrim, FRotator::ZeroRotator, false);
		Part(Sp, Cube, FVector(0, Out * 11.f, 0), FVector(6.f, 2.f, WheelR * 1.05f), MTrim, FRotator::ZeroRotator, false);
		WheelSteer.Add(S);
		WheelSpin.Add(Sp);
	}

	BuildLights(HL, HW, LampZ);
	(void)R;
}

bool ADCVehicle::BuildModel(int32 Seed)
{
	static const bool bEnabled = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("DCCarModel="), V); return V != 0; }();
	if (!bEnabled || UE_ARRAY_COUNT(DCPaths::CarModels) == 0) { return false; }
	int32 Pick = Seed % UE_ARRAY_COUNT(DCPaths::CarModels);
	// Dev: -DCCarOnly=<Name> makes every car SM_Car_<Name> (checking one model's fit / lamps).
	FString Only;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCCarOnly="), Only))
	{
		for (int32 i = 0; i < UE_ARRAY_COUNT(DCPaths::CarModels); ++i)
		{
			if (FString(DCPaths::CarModels[i].Mesh).Contains(TEXT("/SM_Car_") + Only + TEXT("."))) { Pick = i; }
		}
	}
	const DCPaths::FCarModel& Def = DCPaths::CarModels[Pick];
	UStaticMesh* Mesh = LoadQuiet<UStaticMesh>(Def.Mesh);
	UMaterialInterface* Skin = LoadQuiet<UMaterialInterface>(Def.Skin);
	if (!Mesh) { UE_LOG(LogTemp, Warning, TEXT("[DCVehicle] car model %s missing, using primitive car"), Def.Mesh); return false; }

	// Model space: ground at Z=0 (Tools/car_import.py); Def.Yaw turns the nose to +X. Fit every model to the same
	// length/width caps (slots are 225 apart) so parking gaps and gate clearance stay as tuned.
	constexpr float MaxLength = 450.f, MaxWidth = 195.f;
	const FBox B = Mesh->GetBoundingBox();
	const bool bSideways = FMath::Abs(FMath::Sin(FMath::DegreesToRadians(Def.Yaw))) > 0.5f;
	const FVector Ext = B.GetSize();
	const float ExtL = bSideways ? Ext.Y : Ext.X, ExtW = bSideways ? Ext.X : Ext.Y;
	if (ExtL < 1.f || ExtW < 1.f) { return false; }
	const float S = FMath::Min(MaxLength / ExtL, MaxWidth / ExtW);
	Style = EDCCarStyle::Estate;
	Length = ExtL * S;
	Width = ExtW * S;
	const float Height = Ext.Z * S;
	WheelR = 0.f; // unused: wheels are part of the mesh

	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(BodyPivot);
	const FVector Centre = B.GetCenter();
	const FRotator Yaw(0.f, Def.Yaw, 0.f);
	C->SetRelativeRotation(Yaw);
	C->SetRelativeScale3D(FVector(S));
	C->SetRelativeLocation(Yaw.RotateVector(-FVector(Centre.X, Centre.Y, B.Min.Z) * S));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Skin) { C->SetMaterial(0, Skin); } // paint colour is baked into each model's texture
	C->RegisterComponent();
	AddInstanceComponent(C);

	// No glowing lens pads: lamp positions differ per model, so only the real spot / tail lights light up.
	const float HL = Length * 0.5f, HW = Width * 0.5f;
	const float HeadZ = Height * 0.42f;

	// No hinged doors or separate wheels: DoorHinge / WheelSteer / WheelSpin stay empty and Tick skips them.
	DoorHinge.Reset(); WheelSteer.Reset(); WheelSpin.Reset();
	BuildLights(HL, HW, HeadZ);
	UE_LOG(LogTemp, Log, TEXT("[DCVehicle] model %s scale %.2f -> %.0f x %.0f x %.0f"), Def.Mesh, S, Length, Width, Height);
	return true;
}

void ADCVehicle::BuildLights(float HL, float HW, float LampZ)
{
	HeadLights.Reset();
	for (int32 s = -1; s <= 1; s += 2)
	{
		USpotLightComponent* L = NewObject<USpotLightComponent>(this);
		L->SetupAttachment(BodyPivot);
		L->SetRelativeLocation(FVector(HL + 6.f, s * (HW - 24.f), LampZ));
		L->SetRelativeRotation(FRotator(-9.f, s * 3.f, 0.f));
		L->IntensityUnits = ELightUnits::Candelas;
		L->Intensity = 0.f;
		L->SetLightColor(FLinearColor(1.f, 0.88f, 0.7f));
		L->AttenuationRadius = 2400.f;
		L->InnerConeAngle = 14.f;
		L->OuterConeAngle = 32.f;
		L->SourceRadius = 4.f;
		L->SetCastShadows(false);
		L->SetVisibility(false);
		L->RegisterComponent();
		AddInstanceComponent(L);
		HeadLights.Add(L);
	}
	TailLight = NewObject<UPointLightComponent>(this);
	TailLight->SetupAttachment(BodyPivot);
	TailLight->SetRelativeLocation(FVector(-HL - 30.f, 0.f, LampZ));
	TailLight->IntensityUnits = ELightUnits::Candelas;
	TailLight->Intensity = 0.f;
	TailLight->SetLightColor(FLinearColor(1.f, 0.05f, 0.03f));
	TailLight->AttenuationRadius = 320.f;
	TailLight->SetCastShadows(false);
	TailLight->SetVisibility(false);
	TailLight->RegisterComponent();
	AddInstanceComponent(TailLight);
}

FBox2D ADCVehicle::GetFootprint() const
{
	FBox2D B(ForceInit);
	const FTransform T = GetActorTransform();
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector C = T.TransformPositionNoScale(FVector((i & 1) ? Length * 0.5f : -Length * 0.5f, (i & 2) ? Width * 0.5f : -Width * 0.5f, 0.f));
		B += FVector2D(C.X, C.Y);
	}
	return B;
}

void ADCVehicle::ParkAt(const FTransform& Slot)
{
	Route.Reset();
	Speed = PrevSpeed = 0.f;
	Steer = 0.f;
	Brake = 0.f;
	FVector L = Slot.GetLocation();
	L.Z = TraceGround(L);
	GroundZ = L.Z;
	SetActorLocationAndRotation(L, FRotator(0.f, Slot.Rotator().Yaw, 0.f));
	SetActorHiddenInGame(false);
	CarState = EDCCarState::Parked;
}

void ADCVehicle::SetGone()
{
	Route.Reset();
	Speed = PrevSpeed = 0.f;
	DoorTarget[0] = DoorTarget[1] = DoorAlpha[0] = DoorAlpha[1] = 0.f;
	for (USceneComponent* H : DoorHinge) { if (H) { H->SetRelativeRotation(FRotator::ZeroRotator); } }
	SetActorHiddenInGame(true);
	CarState = EDCCarState::Gone;
	for (USpotLightComponent* L : HeadLights) { if (L) { L->SetVisibility(false); } }
	if (TailLight) { TailLight->SetVisibility(false); }
}

void ADCVehicle::SetDoorsOpen(bool bOpen, int32 Side)
{
	if (Side <= 0) { DoorTarget[0] = bOpen ? 1.f : 0.f; }
	if (Side >= 0) { DoorTarget[1] = bOpen ? 1.f : 0.f; }
}

void ADCVehicle::DriveAway(int32 SlotIndex)
{
	TArray<FVector> Pts;
	DCSite::DepartRoute(SlotIndex, Pts);
	// Start from where we actually are (in case we were nudged).
	if (Pts.Num()) { Pts[0] = FVector(GetActorLocation().X, GetActorLocation().Y, 0.f); }
	BuildRoute({ Pts }, { false }, 0.f, Cruise);
	bAxles = false;
	BayPos = FVector2D(GetActorLocation()); bBayAtStart = true; bBayAtEnd = false;
	CheckGateClearance(SlotIndex, false);
	HideAtS = -1.f;
	for (const FSample& S : Route) { if (S.P.X > OffscreenX) { HideAtS = S.S; break; } }
	TargetSlot = INDEX_NONE;
	SetDoorsOpen(false);
	SetActorHiddenInGame(false);
	CarState = EDCCarState::Driving;
	StuckTime = 0.f;
	DriveTime = 0.f;
}

void ADCVehicle::DriveIn(int32 SlotIndex)
{
	TArray<FVector> Dep;
	DCSite::DepartRoute(SlotIndex, Dep);
	const FVector P = DCSite::ParkingSlot(SlotIndex).GetLocation();
	// Forward leg: the departure route reversed, from just off-screen to the top of the driveway, then past the bay.
	TArray<FVector> Fwd;
	Fwd.Add(FVector(OffscreenX, DCSite::LaneY + 110.f, 0.f));
	for (int32 i = Dep.Num() - 2; i >= 2; --i) { Fwd.Add(Dep[i]); }
	const float Dir = (P.X >= DCSite::DriveX) ? 1.f : -1.f;
	const float PullY = Dep.IsValidIndex(1) ? Dep[1].Y : P.Y - 280.f;   // the lane in front of the bays
	const float LineY = Dep.IsValidIndex(2) ? Dep[2].Y : PullY - 140.f; // top of the driveway
	Fwd.Add(FVector(FMath::Lerp(DCSite::DriveX, P.X, 0.5f) + Dir * 60.f, FMath::Lerp(LineY, PullY, 0.6f), 0.f));
	const FVector Pull(P.X + Dir * 330.f, PullY - 20.f, 0.f);
	Fwd.Add(Pull);
	// Reverse leg: back into the bay so the car ends nose-out (facing the lane), like the parked ones.
	TArray<FVector> Rev;
	Rev.Add(Pull);
	// Back out straight along the line we arrived on first, so the nose doesn't have to swing at the stop.
	if (Fwd.Num() >= 2) { Rev.Add(Pull + (Pull - Fwd[Fwd.Num() - 2]).GetSafeNormal2D() * -70.f); }
	Rev.Add(FVector(P.X + Dir * 90.f, PullY + 40.f, 0.f));
	Rev.Add(FVector(P.X + Dir * 8.f, FMath::Lerp(PullY, P.Y, 0.55f), 0.f));
	Rev.Add(FVector(P.X, P.Y, 0.f));
	BuildRoute({ Fwd, Rev }, { false, true }, Cruise, 0.f);
	bAxles = false;
	BayPos = FVector2D(P); bBayAtStart = false; bBayAtEnd = true;
	CheckGateClearance(SlotIndex, true);
	HideAtS = -1.f;
	TargetSlot = SlotIndex;
	const FVector Start = Route.Num() ? Route[0].P : FVector(OffscreenX, DCSite::LaneY, 0.f);
	FVector Tan; SampleAt(0.f, &Tan);
	SetActorLocationAndRotation(FVector(Start.X, Start.Y, TraceGround(Start)), FRotator(0.f, Tan.Rotation().Yaw, 0.f));
	GroundZ = GetActorLocation().Z;
	SetDoorsOpen(false);
	SetActorHiddenInGame(false);
	CarState = EDCCarState::Driving;
	StuckTime = 0.f;
	DriveTime = 0.f;
}

void ADCVehicle::CheckGateClearance(int32 SlotIndex, bool bArriving) const
{
	static TSet<int32> Checked;
	const int32 Key = SlotIndex * 2 + (bArriving ? 1 : 0);
	if (Checked.Contains(Key) || Route.Num() < 2) { return; }
	Checked.Add(Key);
	TArray<FVector4f> Obs;
	DCSite::GateObstacles(Obs);
	float Best = 1e9f;
	FVector2D BestAt = FVector2D::ZeroVector;
	for (int32 i = 0; i + 1 < Route.Num(); ++i)
	{
		const FVector2D P(Route[i].P.X, Route[i].P.Y);
		if (FMath::Abs(P.Y - DCSite::FenceY) > 900.f || FMath::Abs(P.X - DCSite::DriveX) > 1500.f) { continue; }
		const FVector2D D = FVector2D(Route[i + 1].P.X - P.X, Route[i + 1].P.Y - P.Y).GetSafeNormal();
		if (D.IsNearlyZero()) { continue; }
		for (const FVector4f& O : Obs)
		{
			const FVector2D L = FVector2D(O.X, O.Y) - P;
			const float X = FVector2D::DotProduct(L, D), Y = -D.Y * L.X + D.X * L.Y;
			const float Dx = FMath::Max(FMath::Abs(X) - Length * 0.5f, 0.f), Dy = FMath::Max(FMath::Abs(Y) - Width * 0.5f, 0.f);
			const float Dist = FMath::Sqrt(Dx * Dx + Dy * Dy) - O.Z;
			if (Dist < Best) { Best = Dist; BestAt = FVector2D(O.X, O.Y); }
		}
	}
	if (Best < 25.f)
	{
		UE_LOG(LogDC, Warning, TEXT("[Vehicle] slot %d %s: only %.0f cm from the gateway at (%.0f, %.0f)"), SlotIndex, bArriving ? TEXT("in") : TEXT("out"), Best, BestAt.X, BestAt.Y);
	}
	else
	{
		UE_LOG(LogDC, Display, TEXT("[Vehicle] slot %d %s: gateway clearance %.0f cm"), SlotIndex, bArriving ? TEXT("in") : TEXT("out"), Best);
	}
}

void ADCVehicle::BuildRoute(const TArray<TArray<FVector>>& Segments, const TArray<bool>& Reverse, float StartSpeed, float EndSpeed)
{
	Route.Reset();
	RouteS = 0.f;
	Speed = PrevSpeed = 0.f;
	TArray<int32> GearBreaks; // sample indices where the gear changes (full stop)
	for (int32 Seg = 0; Seg < Segments.Num(); ++Seg)
	{
		const TArray<FVector>& Pts = Segments[Seg];
		if (Pts.Num() < 2) { continue; }
		const bool bRev = Reverse.IsValidIndex(Seg) && Reverse[Seg];
		const int32 N = Pts.Num();
		if (Route.Num()) { GearBreaks.Add(Route.Num() - 1); }
		for (int32 i = 0; i < N - 1; ++i)
		{
			const FVector P1 = Pts[i], P2 = Pts[i + 1];
			const FVector P0 = i > 0 ? Pts[i - 1] : P1 - (P2 - P1);
			const FVector P3 = i + 2 < N ? Pts[i + 2] : P2 + (P2 - P1);
			const int32 Steps = FMath::Clamp(FMath::CeilToInt(FVector::Dist(P1, P2) / 20.f), 2, 2000);
			for (int32 k = (i == 0 && Route.Num() == 0) ? 0 : 1; k <= Steps; ++k)
			{
				if (k == 0 && Route.Num()) { continue; }
				FSample S;
				S.P = CatmullRom(P0, P1, P2, P3, (float)k / Steps);
				S.P.Z = 0.f;
				S.bReverse = bRev;
				Route.Add(S);
			}
		}
	}
	const int32 N = Route.Num();
	if (N < 2) { Route.Reset(); return; }
	for (int32 i = 1; i < N; ++i) { Route[i].S = Route[i - 1].S + FVector::Dist2D(Route[i].P, Route[i - 1].P); }
	// Curvature + speed caps.
	for (int32 i = 0; i < N; ++i)
	{
		float K = 0.f;
		if (i > 0 && i < N - 1)
		{
			const FVector A = (Route[i].P - Route[i - 1].P).GetSafeNormal2D();
			const FVector B = (Route[i + 1].P - Route[i].P).GetSafeNormal2D();
			const float Ds = FMath::Max(1.f, (Route[i + 1].S - Route[i - 1].S) * 0.5f);
			K = FMath::Atan2(A.X * B.Y - A.Y * B.X, FVector::DotProduct(A, B)) / Ds;
		}
		Route[i].Curv = K;
		float V = Route[i].bReverse ? ReverseSpeed : Cruise;
		// Slow in the yard / on the driveway.
		if (Route[i].P.Y > DCSite::LaneY + 300.f) { V = FMath::Min(V, YardSpeed); }
		if (FMath::Abs(K) > 1e-5f) { V = FMath::Min(V, FMath::Sqrt(LatAccel / FMath::Abs(K))); }
		Route[i].VMax = V;
	}
	// Smooth curvature caps a little (spline noise).
	Route[0].VMax = StartSpeed;
	Route[N - 1].VMax = EndSpeed;
	for (int32 G : GearBreaks) { Route[G].VMax = 0.f; }
	for (int32 i = 1; i < N; ++i)
	{
		const float Ds = Route[i].S - Route[i - 1].S;
		Route[i].VMax = FMath::Min(Route[i].VMax, FMath::Sqrt(FMath::Square(Route[i - 1].VMax) + 2.f * Accel * Ds));
	}
	for (int32 i = N - 2; i >= 0; --i)
	{
		const float Ds = Route[i + 1].S - Route[i].S;
		Route[i].VMax = FMath::Min(Route[i].VMax, FMath::Sqrt(FMath::Square(Route[i + 1].VMax) + 2.f * Decel * Ds));
	}
}

ADCVehicle::FSample ADCVehicle::SampleAt(float S, FVector* OutTangent) const
{
	if (Route.Num() == 0) { if (OutTangent) { *OutTangent = GetActorForwardVector(); } return FSample(); }
	int32 Lo = 0, Hi = Route.Num() - 1;
	if (S <= 0.f) { Hi = 1; }
	else if (S >= Route.Last().S) { Lo = Route.Num() - 2; }
	else
	{
		while (Hi - Lo > 1) { const int32 M = (Lo + Hi) / 2; if (Route[M].S <= S) { Lo = M; } else { Hi = M; } }
	}
	const FSample& A = Route[Lo];
	const FSample& B = Route[Hi];
	const float T = FMath::Clamp((S - A.S) / FMath::Max(0.01f, B.S - A.S), 0.f, 1.f);
	FSample Out;
	Out.P = FMath::Lerp(A.P, B.P, T);
	Out.S = S;
	Out.VMax = FMath::Lerp(A.VMax, B.VMax, T);
	Out.Curv = FMath::Lerp(A.Curv, B.Curv, T);
	Out.bReverse = T < 0.5f ? A.bReverse : B.bReverse;
	if (OutTangent)
	{
		// Central-difference tangent at each end of the span, blended across it: a tangent held constant per span
		// stepped the heading every 20 cm in tight turns.
		// Neighbours never cross a gear change: the legs either side run in opposite directions, and mixing them
		// flipped the nose round at the stop before reversing.
		auto TanAt = [this](int32 I)
		{
			int32 I0 = FMath::Max(0, I - 1), I1 = FMath::Min(Route.Num() - 1, I + 1);
			if (Route[I0].bReverse != Route[I].bReverse) { I0 = I; }
			if (Route[I1].bReverse != Route[I].bReverse) { I1 = I; }
			if (I0 == I1) { I0 = FMath::Max(0, I - 1); I1 = I; }
			return (Route[I1].P - Route[I0].P).GetSafeNormal2D();
		};
		const FVector TA = TanAt(Lo), TB = TanAt(Hi);
		if (A.bReverse != B.bReverse) { *OutTangent = T < 0.5f ? TA : TB; } // matches Out.bReverse, so nose = Tan * Dir stays put
		else
		{
			const FVector Blend = FMath::Lerp(TA, TB, T);
			*OutTangent = Blend.IsNearlyZero() ? TB : Blend.GetSafeNormal2D();
		}
	}
	return Out;
}

float ADCVehicle::TraceGround(const FVector& P) const
{
	const UWorld* W = GetWorld();
	if (!W) { return -3.f; }
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(DCCarGround), false, this);
	if (W->LineTraceSingleByChannel(Hit, FVector(P.X, P.Y, 400.f), FVector(P.X, P.Y, -600.f), ECC_WorldStatic, Q))
	{
		return Hit.ImpactPoint.Z;
	}
	return -3.f;
}

void ADCVehicle::UpdateLights(float Dt, bool bDriving)
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	const bool bDark = Run && Run->HasActiveRun() && (Run->IsNight() || Run->GetSunAlpha() < 0.22f);
	const float Want = (bDriving && bDark) ? 1.f : 0.f;
	LightsAlpha = FMath::FInterpTo(LightsAlpha, Want, Dt, 6.f);
	const bool bOn = LightsAlpha > 0.01f;
	for (USpotLightComponent* L : HeadLights)
	{
		if (!L) { continue; }
		L->SetVisibility(bOn);
		L->SetIntensity(320.f * LightsAlpha);
	}
	if (HeadMID) { HeadMID->SetScalarParameterValue(TEXT("Strength"), 0.5f + (bDriving ? 3.f : 0.f) + 40.f * LightsAlpha); }
	const float TailK = (bDriving ? (0.25f + 0.75f * LightsAlpha) : 0.f) + Brake;
	if (TailMID) { TailMID->SetScalarParameterValue(TEXT("Strength"), 0.4f + TailK * 14.f); }
	if (TailLight)
	{
		TailLight->SetVisibility(TailK > 0.02f);
		TailLight->SetIntensity(TailK * 18.f);
	}
}

void ADCVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (CarState == EDCCarState::Gone) { return; }
	const float Dt = FMath::Clamp(DeltaSeconds, 0.f, 0.1f);

	// Doors.
	for (int32 i = 0; i < 2 && i < DoorHinge.Num(); ++i)
	{
		DoorAlpha[i] = FMath::FInterpConstantTo(DoorAlpha[i], DoorTarget[i], Dt, 3.2f);
		const float E = DoorAlpha[i] * DoorAlpha[i] * (3.f - 2.f * DoorAlpha[i]);
		if (DoorHinge[i]) { DoorHinge[i]->SetRelativeRotation(FRotator(0.f, (i == 0 ? 1.f : -1.f) * 62.f * E, 0.f)); }
	}

	const bool bDriving = CarState == EDCCarState::Driving && Route.Num() > 1;
	float AccelNow = 0.f, Lateral = 0.f;
	if (bDriving)
	{
		DriveTime += Dt;
		const FSample Cur = SampleAt(RouteS, nullptr);
		// Speed follows the precomputed profile (with a crawl floor so the car never stalls short of a stop).
		float Target = FMath::Max(Cur.VMax, MinCrawl);
		// Look ahead for an imminent gear change / end: brake into it.
		Speed = FMath::FInterpConstantTo(Speed, Target, Dt, FMath::Max(Accel, Decel) * 1.2f);
		float NewS = RouteS + Speed * Dt;
		// Full stop at gear changes: snap to the break point, wait a beat.
		bool bStopped = false;
		for (int32 i = 1; i < Route.Num(); ++i)
		{
			if (Route[i].bReverse != Route[i - 1].bReverse && RouteS < Route[i - 1].S - 0.5f && NewS >= Route[i - 1].S)
			{
				NewS = Route[i - 1].S + 0.6f;
				Speed = 0.f;
				bStopped = true;
				break;
			}
		}
		RouteS = FMath::Min(NewS, Route.Last().S);
		AccelNow = (Speed - PrevSpeed) / FMath::Max(Dt, 1e-3f);
		PrevSpeed = Speed;

		FVector Tan;
		const FSample S = SampleAt(RouteS, &Tan);
		const float Dir = S.bReverse ? -1.f : 1.f;
		const float A = AxleHalf();
		if (!bAxles)
		{
			const FVector2D C(GetActorLocation()), N(GetActorForwardVector().GetSafeNormal2D());
			FrontAx = C + N * A; RearAx = C - N * A;
			bPrevReverse = S.bReverse; LeadOffset = FVector2D::ZeroVector; bAxles = true;
		}
		// Leading axle: A further along the route, but never past the next gear change / the end (extrapolate there).
		float GearS = Route.Last().S;
		for (int32 i = 1; i < Route.Num(); ++i)
		{
			if (Route[i].bReverse != Route[i - 1].bReverse && Route[i - 1].S > RouteS) { GearS = Route[i - 1].S; break; }
		}
		FVector2D Lead;
		if (RouteS + A <= GearS) { Lead = FVector2D(SampleAt(RouteS + A, nullptr).P); }
		else
		{
			FVector TG; const FSample G = SampleAt(FMath::Max(GearS - 2.f, 0.f), &TG);
			Lead = FVector2D(G.P) + FVector2D(TG.GetSafeNormal2D()) * (RouteS + A - GearS + 2.f);
		}
		if (S.bReverse != bPrevReverse)
		{
			// The other axle now leads: start from where it actually is and ease onto the route.
			LeadOffset = (S.bReverse ? RearAx : FrontAx) - Lead;
			bPrevReverse = S.bReverse;
		}
		LeadOffset = FMath::Vector2DInterpTo(LeadOffset, FVector2D::ZeroVector, Dt, 3.f);
		Lead += LeadOffset;
		FVector2D& LeadAx = S.bReverse ? RearAx : FrontAx;
		FVector2D& TrailAx = S.bReverse ? FrontAx : RearAx;
		LeadAx = Lead;
		const FVector2D Back = TrailAx - LeadAx;
		TrailAx = LeadAx + (Back.SizeSquared() > 1.f ? Back.GetSafeNormal() : -FVector2D(Tan.GetSafeNormal2D()) * Dir) * (2.f * A);

		FVector2D Centre = (FrontAx + RearAx) * 0.5f;
		const FVector2D Nose = FrontAx - RearAx;
		float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Nose.Y, Nose.X));
		// Around the parking bay the yard manoeuvres (pull out, the whole reverse in) follow the tuned route pose
		// exactly: bays are only ~30 cm apart and the reverse leg is too tight for a dragged front axle.
		// Departing: route pose within 3 m of the bay, axle model beyond 6 m. Arriving: blend in on the forward
		// approach (7 m -> 4.5 m from the bay) so the reverse leg starts already on the route.
		float W = 0.f;
		const FVector2D Here(S.P);
		if (bBayAtStart) { W = FMath::Max(W, 1.f - FMath::SmoothStep(300.f, 600.f, (float)FVector2D::Distance(Here, BayPos))); }
		if (bBayAtEnd) { W = FMath::Max(W, S.bReverse ? 1.f : 1.f - FMath::SmoothStep(450.f, 700.f, (float)FVector2D::Distance(Here, BayPos))); }
		if (W > 0.f)
		{
			Centre = FMath::Lerp(Centre, Here, W);
			Yaw += FMath::FindDeltaAngleDegrees(Yaw, (Tan * Dir).Rotation().Yaw) * W;
		}
		{
			// Ease toward the target heading (no hard cap that clips into visible steps), at most 240 deg/s.
			const float CurYaw = GetActorRotation().Yaw;
			const float Delta = FMath::FindDeltaAngleDegrees(CurYaw, Yaw);
			Yaw = CurYaw + FMath::Clamp(Delta * FMath::Min(1.f, Dt * 14.f), -240.f * Dt, 240.f * Dt);
		}
		FVector L(Centre.X, Centre.Y, 0.f);
		const float Gz = TraceGround(L);
		GroundZ = FMath::FInterpTo(GroundZ, Gz, Dt, 10.f);
		L.Z = GroundZ;
		static const bool bYawLog = FParse::Param(FCommandLine::Get(), TEXT("DCCarYawLog"));
		if (bYawLog) { UE_LOG(LogTemp, Log, TEXT("[DCYaw] %.1f %d %.3f"), RouteS, S.bReverse ? 1 : 0, FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, Yaw)); }
		SetActorLocationAndRotation(L, FRotator(0.f, Yaw, 0.f));
		Lateral = Speed * Speed * S.Curv;
		const float SteerTarget = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan(WheelBase * S.Curv)), -34.f, 34.f) * Dir;
		Steer = FMath::FInterpTo(Steer, SteerTarget, Dt, 6.f);
		SpinAngle = FMath::Fmod(SpinAngle - Dir * Speed * Dt / WheelR * (180.f / UE_PI), 360.f);
		Brake = FMath::FInterpTo(Brake, (AccelNow < -120.f || (Speed < 60.f && S.S > 1.f)) ? 1.f : 0.f, Dt, 10.f);
		if (bStopped) { Brake = 1.f; }

		if (HideAtS > 0.f && RouteS >= HideAtS) { UE_LOG(LogTemp, Log, TEXT("DCVehicle: off-screen after %.1fs"), DriveTime); SetGone(); return; }
		if (RouteS >= Route.Last().S - 0.5f)
		{
			if (TargetSlot != INDEX_NONE)
			{
				UE_LOG(LogTemp, Log, TEXT("DCVehicle: parked in slot %d after %.1fs"), TargetSlot, DriveTime);
				ParkAt(DCSite::ParkingSlot(TargetSlot));
				TargetSlot = INDEX_NONE;
			}
			else { SetGone(); return; }
		}
	}
	else
	{
		Speed = 0.f;
		Steer = FMath::FInterpTo(Steer, 0.f, Dt, 3.f);
		Brake = FMath::FInterpTo(Brake, 0.f, Dt, 2.f);
	}

	for (int32 i = 0; i < WheelSteer.Num(); ++i)
	{
		if (WheelSteer[i]) { WheelSteer[i]->SetRelativeRotation(FRotator(0.f, i < 2 ? Steer : 0.f, 0.f)); }
		if (WheelSpin.IsValidIndex(i) && WheelSpin[i]) { WheelSpin[i]->SetRelativeRotation(FRotator(SpinAngle, 0.f, 0.f)); }
	}
	// Body: dive under braking, squat accelerating, lean out of corners, engine shimmy.
	const float Dir = (Route.Num() && SampleAt(RouteS, nullptr).bReverse) ? -1.f : 1.f;
	Pitch = FMath::FInterpTo(Pitch, FMath::Clamp(AccelNow * Dir * 0.0022f, -1.8f, 1.8f), Dt, 5.f);
	Roll = FMath::FInterpTo(Roll, FMath::Clamp(-Lateral * 0.0012f * Dir, -2.2f, 2.2f), Dt, 5.f);
	BobT += Dt;
	const float Shimmy = DCSettings::ReduceMotion() ? 0.f : bDriving ? (0.35f + FMath::Min(Speed, 1200.f) / 1200.f * 0.6f) : 0.12f;
	BodyPivot->SetRelativeLocationAndRotation(FVector(0.f, 0.f, FMath::Sin(BobT * 23.f) * Shimmy + FMath::Sin(BobT * 3.1f) * Shimmy * 0.8f),
		FRotator(Pitch, 0.f, Roll + FMath::Sin(BobT * 17.f) * 0.08f * Shimmy));
	UpdateLights(Dt, bDriving);
}
