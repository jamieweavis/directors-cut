#include "World/DCVehicle.h"
#include "Core/DCStyle.h"
#include "Core/DCAssetPaths.h"
#include "Core/DCSiteLayout.h"
#include "Core/DCRunSubsystem.h"
#include "Core/DCSettings.h"
#include "World/Scene/DCLandscapeMeshes.h"
#include "DirectorsCut.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// Speeds (cm/s) and accelerations (cm/s^2): gentle numbers, a farmyard car park rather than a race track.
	constexpr float Cruise = 1700.f;      // on the lane
	constexpr float YardSpeed = 520.f;    // straights in the yard and on the driveway (~19 km/h)
	constexpr float LatAccel = 330.f;     // sideways comfort limit through corners
	constexpr float AccelYard = 230.f, AccelLane = 420.f;
	constexpr float DecelYard = 270.f, DecelLane = 520.f;
	constexpr float StopDecel = 140.f;    // the last couple of metres into the bay
	constexpr float Step = 10.f;          // route sample spacing (cm)
	constexpr float MaxLock = 40.f;       // front wheel steer limit for the visuals (deg)

	template <typename T>
	T* LoadQuiet(const TCHAR* Path) { return LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet); }

	/** Straight legs between the corners, each interior corner rounded with a circular arc of radius R[i] (shrunk where
	 *  two neighbouring arcs would overlap). Tangent-continuous, so the heading never jumps. */
	void FilletPath(const TArray<FVector2D>& C, const TArray<float>& R, TArray<FVector2D>& Out)
	{
		Out.Reset();
		const int32 N = C.Num();
		if (N < 2) { return; }
		TArray<float> TL, Theta;
		TL.Init(0.f, N);
		Theta.Init(0.f, N);
		for (int32 i = 1; i < N - 1; ++i)
		{
			const FVector2D A = (C[i] - C[i - 1]).GetSafeNormal(), B = (C[i + 1] - C[i]).GetSafeNormal();
			Theta[i] = FMath::Acos(FMath::Clamp(FVector2D::DotProduct(A, B), -1.f, 1.f));
			TL[i] = R[i] * FMath::Tan(Theta[i] * 0.5f);
		}
		for (int32 i = 0; i + 1 < N; ++i)
		{
			const float L = FVector2D::Distance(C[i], C[i + 1]), Sum = TL[i] + TL[i + 1];
			if (Sum > L && Sum > 0.f) { const float K = L / Sum; TL[i] *= K; TL[i + 1] *= K; }
		}
		auto Line = [&Out](const FVector2D& B)
		{
			const FVector2D A = Out.Last();
			const float L = FVector2D::Distance(A, B);
			if (L < 0.5f) { return; }
			const int32 M = FMath::Max(1, FMath::CeilToInt(L / Step));
			for (int32 k = 1; k <= M; ++k) { Out.Add(FMath::Lerp(A, B, (float)k / M)); }
		};
		Out.Add(C[0]);
		for (int32 i = 1; i < N - 1; ++i)
		{
			const FVector2D A = (C[i] - C[i - 1]).GetSafeNormal(), B = (C[i + 1] - C[i]).GetSafeNormal();
			const FVector2D P0 = C[i] - A * TL[i];
			Line(P0);
			if (Theta[i] < 1e-3f || TL[i] < 0.5f) { continue; }
			const float Rad = TL[i] / FMath::Tan(Theta[i] * 0.5f);
			const float Cross = A.X * B.Y - A.Y * B.X;
			const FVector2D Nrm = Cross > 0.f ? FVector2D(-A.Y, A.X) : FVector2D(A.Y, -A.X);
			const FVector2D Ctr = P0 + Nrm * Rad;
			const float A0 = FMath::Atan2(P0.Y - Ctr.Y, P0.X - Ctr.X), Sgn = Cross > 0.f ? 1.f : -1.f;
			const int32 M = FMath::Max(2, FMath::CeilToInt(Rad * Theta[i] / Step));
			for (int32 k = 1; k <= M; ++k)
			{
				const float Ang = A0 + Sgn * Theta[i] * k / M;
				Out.Add(Ctr + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * Rad);
			}
		}
		Line(C.Last());
	}

	/** Even spacing along the polyline. */
	void Resample(const TArray<FVector2D>& In, TArray<FVector2D>& Out)
	{
		Out.Reset();
		if (In.Num() < 2) { Out = In; return; }
		Out.Add(In[0]);
		float Carry = 0.f;
		for (int32 i = 1; i < In.Num(); ++i)
		{
			const FVector2D A = In[i - 1], B = In[i];
			const float L = FVector2D::Distance(A, B);
			float At = Step - Carry;
			while (At <= L) { Out.Add(FMath::Lerp(A, B, At / L)); At += Step; }
			Carry = L - (At - Step);
		}
		if (FVector2D::Distance(Out.Last(), In.Last()) > 1.f) { Out.Add(In.Last()); } else { Out.Last() = In.Last(); }
	}

	/** Moving average with the window tapering to nothing at the ends: the arcs ease in and out (no step in the
	 *  steering) while the ends, and every straight, stay exactly where they were. */
	template <typename T>
	void SmoothEnds(TArray<T>& V, int32 W, int32 Passes)
	{
		const int32 N = V.Num();
		TArray<T> Q;
		for (int32 p = 0; p < Passes; ++p)
		{
			Q = V;
			for (int32 i = 1; i < N - 1; ++i)
			{
				const int32 Ww = FMath::Min3(W, i, N - 1 - i);
				T Sum = V[i] * 0.f;
				for (int32 j = i - Ww; j <= i + Ww; ++j) { Sum += V[j]; }
				Q[i] = Sum / (float)(2 * Ww + 1);
			}
			V = Q;
		}
	}

	/** Generated car meshes, shared between cars (kept alive by the components that use them). */
	TMap<FString, TWeakObjectPtr<UStaticMesh>> GCarMeshes;
	template <typename FMake>
	UStaticMesh* CachedMesh(const FString& Key, FMake&& Make)
	{
		if (const TWeakObjectPtr<UStaticMesh>* Found = GCarMeshes.Find(Key)) { if (Found->IsValid()) { return Found->Get(); } }
		UStaticMesh* Mesh = Make();
		GCarMeshes.Add(Key, Mesh);
		return Mesh;
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
	using DCLandMesh::FLoftSection;
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

	// Dimensions per style (local: +X nose, +Y right, Z up, origin = ground centre).
	//   HoodDrop: how far the nose sits below the belt line. Rake / RearRun: horizontal run of the windscreen / rear
	//   window. Tumble: how far the side windows lean in at the roof. CabInset: glass set in from the body side.
	float BodyTop = 88.f, CabX0 = -120.f, CabX1 = 70.f, CabTop = 146.f, DoorX0 = -55.f, DoorX1 = 60.f;
	float HoodDrop = 8.f, Rake = 40.f, RearRun = 38.f, Tumble = 10.f, CabInset = 7.f;
	switch (Style)
	{
	case EDCCarStyle::Hatch:  Length = 380.f; Width = 166.f; WheelBase = 245.f; CabX0 = -150.f; CabX1 = 60.f; break;
	case EDCCarStyle::Estate: Length = 440.f; Width = 170.f; WheelBase = 280.f; CabX0 = -205.f; CabX1 = 70.f; DoorX0 = -70.f; DoorX1 = 65.f; HoodDrop = 6.f; RearRun = 14.f; break;
	case EDCCarStyle::Pickup: Length = 455.f; Width = 174.f; WheelBase = 300.f; CabX0 = -20.f; CabX1 = 95.f; CabTop = 152.f; DoorX0 = -15.f; DoorX1 = 90.f; BodyTop = 92.f; WheelR = 34.f; HoodDrop = 3.f; Rake = 36.f; RearRun = 8.f; Tumble = 8.f; break;
	case EDCCarStyle::Van:    Length = 450.f; Width = 176.f; WheelBase = 290.f; CabX0 = -225.f; CabX1 = 120.f; CabTop = 205.f; DoorX0 = 18.f; DoorX1 = 100.f; BodyTop = 100.f; WheelR = 33.f; Rake = 32.f; RearRun = 6.f; Tumble = 5.f; CabInset = 3.f; break;
	}
	const float HL = Length * 0.5f, HW = Width * 0.5f;
	const float Floor = 30.f;
	const bool bPickup = Style == EDCCarStyle::Pickup, bVan = Style == EDCCarStyle::Van;
	const FString Key = FString::Printf(TEXT("S%d_"), (int32)Style);

	// Rounded pieces are generated once (DCLandMesh::Loft) and shared by every car that needs the same one.
	auto Shell = [&](const TCHAR* Name, const TArray<FLoftSection>& Sections) -> UStaticMesh*
	{
		return CachedMesh(Key + Name, [&]() { return DCLandMesh::Loft(GetTransientPackage(), Sections); });
	};
	auto Box = [&](USceneComponent* Parent, const FVector& Loc, const FVector& Size, float Radius, UMaterialInterface* Mat, float EndRound = -1.f, bool bShadow = true, const FRotator& Rot = FRotator::ZeroRotator) -> UStaticMeshComponent*
	{
		FString BoxKey(TEXT("B"));
		for (const float V : { (float)Size.X, (float)Size.Y, (float)Size.Z, Radius, EndRound }) { BoxKey.AppendChar(TEXT('_')); BoxKey.AppendInt(FMath::RoundToInt(V * 10.f)); }
		UStaticMesh* Mesh = CachedMesh(BoxKey, [&]() { return DCLandMesh::RoundBox(GetTransientPackage(), Size, Radius, EndRound); });
		return Part(Parent, Mesh, Loc, FVector(100.f), Mat, Rot, bShadow);
	};

	// Sill, then the painted lower body: one smooth shell from the nose over the bonnet to the tail.
	Box(BodyPivot, FVector(0, 0, Floor + 1.f), FVector(Length - 30.f, Width - 8.f, 16.f), 5.f, MTrim);
	const float WsX0 = CabX1 + 16.f, WsX1 = WsX0 - Rake; // windscreen: base and top
	const float NoseTop = BodyTop - HoodDrop, BedZ = 64.f;
	{
		TArray<FLoftSection> S;
		S.Add({ HL, Floor + 12.f, NoseTop - 4.f, HW - 8.f, HW - 8.f, 9.f, 0, 5.f });
		S.Add({ HL - 9.f, Floor + 2.f, NoseTop, HW, HW, 12.f, 0, 7.f });
		S.Add({ WsX0 - 4.f, Floor, BodyTop, HW, HW, 12.f, 0, 10.f });
		if (bPickup)
		{
			// Steps down behind the cab to the load bed.
			S.Add({ CabX0 - 4.f, Floor, BodyTop, HW, HW, 12.f, 0, 3.f });
			S.Add({ CabX0 - 10.f, Floor, BedZ, HW, HW, 6.f, 0, 3.f });
			S.Add({ -HL + 4.f, Floor + 2.f, BedZ, HW, HW, 6.f, 0, 3.f });
			S.Add({ -HL, Floor + 8.f, BedZ - 2.f, HW - 3.f, HW - 3.f, 5.f });
		}
		else
		{
			S.Add({ -HL + 9.f, Floor + 2.f, BodyTop - 1.f, HW, HW, 12.f, 0, 7.f });
			S.Add({ -HL, Floor + 12.f, BodyTop - 6.f, HW - 8.f, HW - 8.f, 9.f });
		}
		Part(BodyPivot, Shell(TEXT("Body"), S), FVector::ZeroVector, FVector(100.f), MPaint);
	}
	if (bPickup)
	{
		// Bed walls + tailgate + bed floor.
		const float BedX0 = -HL, BedX1 = CabX0 - 6.f, BedLen = BedX1 - BedX0, BedMid = (BedX0 + BedX1) * 0.5f;
		Box(BodyPivot, FVector(BedMid, -HW + 5.f, 78.f), FVector(BedLen, 10.f, 36.f), 4.f, MPaint, 3.f);
		Box(BodyPivot, FVector(BedMid, HW - 5.f, 78.f), FVector(BedLen, 10.f, 36.f), 4.f, MPaint, 3.f);
		Box(BodyPivot, FVector(BedX0 + 5.f, 0, 78.f), FVector(10.f, Width - 12.f, 34.f), 4.f, MPaint, 3.f);
		Part(BodyPivot, Cube, FVector(BedMid, 0, BedZ + 1.f), FVector(BedLen - 10.f, Width - 20.f, 3.f), MTrim);
		// A tarp-covered load in the bed.
		Box(BodyPivot, FVector(BedMid + 10.f, 0, 78.f), FVector(BedLen * 0.6f, Width * 0.6f, 26.f), 9.f, MakeMID(DCPaths::MatSurface, FLinearColor(0.09f, 0.11f, 0.06f), 0.9f, 0.f), -1.f, true, FRotator(0, 4, 0));
	}
	// Bumpers, grille, chrome side strips.
	Box(BodyPivot, FVector(HL - 3.f, 0, Floor + 12.f), FVector(10.f, Width + 4.f, 18.f), 6.f, MTrim, 3.f);
	Box(BodyPivot, FVector(-HL + 3.f, 0, Floor + 12.f), FVector(10.f, Width + 4.f, 18.f), 6.f, MTrim, 3.f);
	Box(BodyPivot, FVector(HL - 1.f, 0, Floor + 34.f), FVector(4.f, Width * 0.42f, 16.f), 4.f, MTrim, 1.5f);
	Part(BodyPivot, Cube, FVector(0, -HW - 0.5f, BodyTop - 14.f), FVector(Length - 44.f, 2.f, 3.f), MChrome);
	Part(BodyPivot, Cube, FVector(0, HW + 0.5f, BodyTop - 14.f), FVector(Length - 44.f, 2.f, 3.f), MChrome);

	// Cabin: one shell. Raked windscreen, side windows that lean in, rear window; the rounded corners between them
	// are the painted pillars and roof rails, the flat faces flagged below are glass (material slot 1).
	const float GZ0 = BodyTop - 4.f, WB = HW - CabInset, WT = WB - Tumble;
	{
		const uint8 Screen = DCLandMesh::LoftTopAlt | DCLandMesh::LoftSideAlt, Side = DCLandMesh::LoftSideAlt;
		TArray<FLoftSection> S;
		S.Add({ WsX0, GZ0, BodyTop - 1.f, WB, WB, 3.f, Screen, 4.f });
		S.Add({ WsX1, GZ0, CabTop, WB, WT, 11.f, Side, 9.f });
		if (bVan)
		{
			// Panel van: painted load box behind the cab doors.
			S.Add({ CabX1 - 105.f, GZ0, CabTop, WB, WT, 11.f, 0, 0.f });
		}
		else if (!bPickup)
		{
			// B pillar behind the front doors.
			S.Add({ DoorX0 - 1.f, GZ0, CabTop, WB, WT, 11.f, 0, 0.f });
			S.Add({ DoorX0 - 8.f, GZ0, CabTop, WB, WT, 11.f, Side, 0.f });
		}
		S.Add({ CabX0 + RearRun, GZ0, CabTop - 2.f, WB, WT, 11.f, bVan ? uint8(0) : Screen, 9.f });
		S.Add({ CabX0 + 1.f, GZ0, BodyTop - 1.f, WB, WB, 3.f });
		if (UStaticMeshComponent* C = Part(BodyPivot, Shell(TEXT("Cab"), S), FVector::ZeroVector, FVector(100.f), MPaint)) { C->SetMaterial(1, MGlass); }
	}
	if (bVan)
	{
		// Side stripe on the load box.
		const float BoxX1 = CabX1 - 105.f, Y = WB - Tumble * (128.f - GZ0) / (CabTop - GZ0) + 0.4f;
		UMaterialInstanceDynamic* MStripe = MakeMID(DCPaths::MatSurface, Paint * 0.35f + FLinearColor(0.1f, 0.08f, 0.04f), 0.4f, 0.2f);
		for (int32 s = -1; s <= 1; s += 2) { Part(BodyPivot, Cube, FVector((CabX0 + BoxX1) * 0.5f, s * Y, 128.f), FVector(BoxX1 - CabX0 - 30.f, 2.f, 10.f), MStripe); }
	}
	if (Style == EDCCarStyle::Estate)
	{
		// Roof rails.
		const float RoofX0 = CabX0 + RearRun, RoofLen = WsX1 - RoofX0;
		for (int32 s = -1; s <= 1; s += 2) { Box(BodyPivot, FVector(RoofX0 + RoofLen * 0.5f, s * (WT - 13.f), CabTop + 1.5f), FVector(RoofLen - 30.f, 4.f, 4.f), 1.8f, MChrome); }
	}

	// Lights (emissive lens pads).
	const float LampZ = bVan ? BodyTop - 22.f : BodyTop - 20.f;
	for (int32 s = -1; s <= 1; s += 2)
	{
		Box(BodyPivot, FVector(HL + 0.5f, s * (HW - 27.f), LampZ), FVector(4.f, 30.f, 12.f), 5.f, HeadMID, 1.5f, false);
		Box(BodyPivot, FVector(-HL - 0.5f, s * (HW - 22.f), LampZ + (bPickup ? 0.f : 4.f)), FVector(4.f, 24.f, 14.f), 5.f, TailMID, 1.5f, false);
	}
	// Indicators: small amber lenses at the outer corners, front and back, one material per side so each side blinks.
	IndMID.Reset();
	for (int32 s = -1; s <= 1; s += 2)
	{
		UMaterialInstanceDynamic* M = MakeMID(DCPaths::MatEmissive, FLinearColor(1.f, 0.42f, 0.04f), 0.25f, 0.f);
		IndMID.Add(M);
		Box(BodyPivot, FVector(HL + 0.3f, s * (HW - 6.5f), LampZ - 1.f), FVector(4.f, 10.f, 9.f), 3.5f, M, 1.5f, false);
		Box(BodyPivot, FVector(-HL - 0.3f, s * (HW - 6.5f), LampZ + (bPickup ? -1.f : 3.f)), FVector(4.f, 10.f, 10.f), 3.5f, M, 1.5f, false);
	}
	// Number plates.
	UMaterialInstanceDynamic* MPlate = MakeMID(DCPaths::MatSurface, FLinearColor(0.6f, 0.58f, 0.45f), 0.5f, 0.f);
	Part(BodyPivot, Cube, FVector(HL + 2.3f, 0, Floor + 12.f), FVector(2.f, 44.f, 10.f), MPlate, FRotator::ZeroRotator, false);
	Part(BodyPivot, Cube, FVector(-HL - 1.f, 0, Floor + 30.f), FVector(2.f, 44.f, 10.f), MPlate, FRotator::ZeroRotator, false);
	// Wing mirrors.
	for (int32 s = -1; s <= 1; s += 2) { Box(BodyPivot, FVector(WsX0 - 22.f, s * (HW + 4.f), BodyTop + 5.f), FVector(9.f, 16.f, 9.f), 3.5f, MTrim, 3.f); }

	// Doors (front doors): painted panel hinged at its front edge, slightly proud of the body, with a window pane
	// that follows the windscreen rake and leans in with the cabin side.
	DoorHinge.Reset();
	const float PaneZ0 = BodyTop - 2.f, PaneZ1 = CabTop - 12.f;
	const float PaneFront0 = FMath::Min(DoorX1 - 3.f, WsX0 - 9.f);
	const float PaneFront1 = FMath::Min(DoorX1 - 3.f, WsX0 - Rake * (PaneZ1 - BodyTop) / (CabTop - BodyTop) - 9.f);
	for (int32 s = -1; s <= 1; s += 2)
	{
		USceneComponent* H = NewObject<USceneComponent>(this);
		H->SetupAttachment(BodyPivot);
		H->SetRelativeLocation(FVector(DoorX1, s * (HW + 1.5f), 0.f));
		H->RegisterComponent();
		AddInstanceComponent(H);
		const float DL = DoorX1 - DoorX0;
		const float PanelZ0 = Floor + 8.f, PanelZ1 = BodyTop - 9.f;
		Box(H, FVector(-DL * 0.5f, 0, (PanelZ0 + PanelZ1) * 0.5f), FVector(DL, 3.5f, PanelZ1 - PanelZ0), 1.7f, MPaint, 5.f);
		Part(H, Cube, FVector(-DL + 10.f, s * 2.5f, BodyTop - 18.f), FVector(10.f, 2.f, 3.f), MChrome, FRotator::ZeroRotator, false); // handle
		const float LeanPerCm = -s * Tumble / (CabTop - GZ0);
		auto Lean = [&](float Z) { return LeanPerCm * (Z - PaneZ0); };
		TArray<FLoftSection> S;
		S.Add({ PaneFront0 - DoorX1, PaneZ0, PaneZ0 + 3.f, 1.f, 1.f, 0.8f, 0, 3.f, 0.f, Lean(PaneZ0 + 3.f) });
		S.Add({ PaneFront1 - DoorX1, PaneZ0, PaneZ1, 1.f, 1.f, 0.8f, 0, 4.f, 0.f, Lean(PaneZ1) });
		S.Add({ DoorX0 + 3.f - DoorX1, PaneZ0, PaneZ1, 1.f, 1.f, 0.8f, 0, 4.f, 0.f, Lean(PaneZ1) });
		const float PaneY = (WB - Tumble * (PaneZ0 - GZ0) / (CabTop - GZ0) + 1.2f) - (HW + 1.5f);
		Part(H, Shell(s < 0 ? TEXT("PaneL") : TEXT("PaneR"), S), FVector(0, s * PaneY, 0), FVector(100.f), MGlass, FRotator::ZeroRotator, false);
		DoorHinge.Add(H);
	}

	// Wheels: steer (yaw) -> spin (pitch) -> tyre + hub + spoke bars. They stand slightly proud of the body, each in
	// a dark wheel arch.
	WheelSteer.Reset(); WheelSpin.Reset();
	const float WX[4] = { WheelBase * 0.5f, WheelBase * 0.5f, -WheelBase * 0.5f, -WheelBase * 0.5f };
	const float WY[4] = { -(HW - 9.f), HW - 9.f, -(HW - 9.f), HW - 9.f };
	UStaticMesh* Arch = CachedMesh(FString::Printf(TEXT("Arch_%d"), FMath::RoundToInt(WheelR)), [&]()
	{
		const float Ra = WheelR + 7.f, Zc = WheelR + 2.f, Z0 = Floor - 4.f;
		TArray<FLoftSection> S;
		for (int32 k = 0; k <= 10; ++k)
		{
			const float A = PI * k / 10.f;
			S.Add({ Ra * FMath::Cos(A), Z0, FMath::Max(Zc + Ra * FMath::Sin(A), Z0 + 2.f), 6.6f, 6.6f, 1.5f, 0, 0.f });
		}
		return DCLandMesh::Loft(GetTransientPackage(), S, 2);
	});
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
		Part(BodyPivot, Arch, FVector(WX[i], Out * (HW - 6.f), 0.f), FVector(100.f), MTrim, FRotator::ZeroRotator, false);
		Part(Sp, Cyl, FVector::ZeroVector, FVector(WheelR * 2.f, WheelR * 2.f, 24.f), MTyre, FRotator(0, 0, 90));
		Part(Sp, Cyl, FVector(0, Out * 3.f, 0), FVector(WheelR * 1.15f, WheelR * 1.15f, 21.f), MHub, FRotator(0, 0, 90), false);
		Part(Sp, Cube, FVector(0, Out * 13.6f, 0), FVector(WheelR * 1.05f, 2.f, 6.f), MTrim, FRotator::ZeroRotator, false);
		Part(Sp, Cube, FVector(0, Out * 13.6f, 0), FVector(6.f, 2.f, WheelR * 1.05f), MTrim, FRotator::ZeroRotator, false);
		WheelSteer.Add(S);
		WheelSpin.Add(Sp);
	}

	BuildLights(HL, HW, LampZ);
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

void ADCVehicle::GetFootprintOriented(FVector2D& OutCentre, FVector2D& OutHalf, float& OutYawRad) const
{
	OutCentre = FVector2D(GetActorLocation());
	OutHalf = FVector2D(Length * 0.5f, Width * 0.5f);
	OutYawRad = FMath::DegreesToRadians(GetActorRotation().Yaw);
}

// ─── Planned drives ──────────────────────────────────────────────────────────

FDCCarPlan::FSample FDCCarPlan::At(float T) const
{
	if (!IsValid()) { return FSample(); }
	if (T <= 0.f) { return Samples[0]; }
	if (T >= Samples.Last().Time) { return Samples.Last(); }
	int32 Lo = 0, Hi = Samples.Num() - 1;
	while (Hi - Lo > 1) { const int32 M = (Lo + Hi) / 2; if (Samples[M].Time <= T) { Lo = M; } else { Hi = M; } }
	const FSample& A = Samples[Lo];
	const FSample& B = Samples[Hi];
	const float K = FMath::Clamp((T - A.Time) / FMath::Max(1e-4f, B.Time - A.Time), 0.f, 1.f);
	FSample O;
	O.P = FMath::Lerp(A.P, B.P, K);
	O.T = FMath::Lerp(A.T, B.T, K).GetSafeNormal();
	if (O.T.IsNearlyZero()) { O.T = B.T; }
	O.S = FMath::Lerp(A.S, B.S, K);
	O.Curv = FMath::Lerp(A.Curv, B.Curv, K);
	O.V = FMath::Lerp(A.V, B.V, K);
	O.A = FMath::Lerp(A.A, B.A, K);
	O.Time = T;
	O.Signal = K < 0.5f ? A.Signal : B.Signal;
	return O;
}

void FDCCarPlan::PoseAt(float T, FVector2D& OutCentre, FVector2D& OutFwd) const
{
	const FSample S = At(T);
	OutFwd = S.T;
	OutCentre = S.P + S.T * (WheelBase * 0.5f);
}

FDCCarPlan ADCVehicle::PlanDrive(int32 SlotIndex, bool bArrive) const
{
	FDCCarPlan Out;
	Out.WheelBase = WheelBase; Out.Length = Length; Out.Width = Width;
	Out.Slot = SlotIndex;
	Out.Bay = DCSite::SlotBay(SlotIndex);
	Out.bArrive = bArrive;

	TArray<FVector2D> Corners, Pts, Even;
	TArray<float> Radii;
	DCSite::CarRoute(Out.Bay, bArrive, WheelBase, Corners, Radii);
	FilletPath(Corners, Radii, Pts);
	Resample(Pts, Even);
	SmoothEnds(Even, 6, 3);
	const int32 N = Even.Num();
	if (N < 8) { return Out; }

	TArray<FDCCarPlan::FSample>& S = Out.Samples;
	S.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		S[i].P = Even[i];
		S[i].S = i ? S[i - 1].S + FVector2D::Distance(Even[i], Even[i - 1]) : 0.f;
	}
	// Heading: central difference (the ends hold the first/last leg's direction exactly).
	for (int32 i = 0; i < N; ++i)
	{
		const FVector2D D = Even[FMath::Min(N - 1, i + 2)] - Even[FMath::Max(0, i - 2)];
		S[i].T = D.GetSafeNormal();
	}
	// Curvature from the turn over +-3 samples, then eased a little so the steering winds on and off smoothly.
	TArray<float> K; K.Init(0.f, N);
	for (int32 i = 3; i < N - 3; ++i)
	{
		const FVector2D A = (Even[i] - Even[i - 3]).GetSafeNormal(), B = (Even[i + 3] - Even[i]).GetSafeNormal();
		const float Ds = FMath::Max(1.f, (S[i + 3].S - S[i - 3].S) * 0.5f);
		K[i] = FMath::Atan2(A.X * B.Y - A.Y * B.X, FVector2D::DotProduct(A, B)) / Ds;
	}
	SmoothEnds(K, 4, 2);
	for (int32 i = 0; i < N; ++i) { S[i].Curv = K[i]; }

	// Speed: cap per sample (yard / lane, and what the corner allows), then accelerate / brake into those caps.
	auto InYard = [](const FVector2D& P) { return P.Y > DCSite::FenceY - 250.f; };
	TArray<float> V; V.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		float Cap = InYard(S[i].P) ? YardSpeed : Cruise;
		if (FMath::Abs(K[i]) > 1e-5f) { Cap = FMath::Min(Cap, FMath::Sqrt(LatAccel / FMath::Abs(K[i]))); }
		V[i] = Cap;
	}
	V[0] = bArrive ? Cruise : 0.f;
	V[N - 1] = bArrive ? 0.f : Cruise;
	const float Len = S[N - 1].S;
	for (int32 i = 1; i < N; ++i)
	{
		const float Ds = S[i].S - S[i - 1].S, A = InYard(S[i].P) ? AccelYard : AccelLane;
		V[i] = FMath::Min(V[i], FMath::Sqrt(FMath::Square(V[i - 1]) + 2.f * A * Ds));
	}
	for (int32 i = N - 2; i >= 0; --i)
	{
		const float Ds = S[i + 1].S - S[i].S;
		const float D = (bArrive && Len - S[i].S < 260.f) ? StopDecel : InYard(S[i].P) ? DecelYard : DecelLane;
		V[i] = FMath::Min(V[i], FMath::Sqrt(FMath::Square(V[i + 1]) + 2.f * D * Ds));
	}
	// Round off the corners of the speed profile (no jolt where braking starts or a corner opens up).
	TArray<float> Env = V;
	SmoothEnds(V, 8, 2);
	for (int32 i = 0; i < N; ++i) { V[i] = FMath::Min(V[i], Env[i]); S[i].V = V[i]; }
	for (int32 i = 0; i < N; ++i)
	{
		const int32 I0 = FMath::Max(0, i - 1), I1 = FMath::Min(N - 1, i + 1);
		const float Ds = FMath::Max(1.f, S[I1].S - S[I0].S);
		S[i].A = (FMath::Square(V[I1]) - FMath::Square(V[I0])) / (2.f * Ds);
		if (i) { S[i].Time = S[i - 1].Time + (S[i].S - S[i - 1].S) / FMath::Max(8.f, 0.5f * (V[i] + V[i - 1])); }
	}
	// Indicators: on from about 6 m before a real turn until it's done (only near the site, not out on the lane).
	constexpr int32 Look = 60;
	for (int32 i = 0; i < N; ++i)
	{
		if (S[i].P.X < DCSite::ArriveFromX + 2500.f || S[i].P.X > DCSite::LeaveToX - 2500.f) { continue; }
		const int32 J = FMath::Min(N - 1, i + Look);
		const float Turn = FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(FMath::Atan2(S[i].T.Y, S[i].T.X)), FMath::RadiansToDegrees(FMath::Atan2(S[J].T.Y, S[J].T.X)));
		if (FMath::Abs(Turn) > 30.f) { S[i].Signal = Turn > 0.f ? 1 : -1; }
		else if (FMath::Abs(K[i]) > 1.f / 900.f) { S[i].Signal = K[i] > 0.f ? 1 : -1; }
	}
	if (!bArrive) { Out.HideAtTime = S[N - 1].Time; }
	return Out;
}

void ADCVehicle::ParkAt(const FTransform& Slot)
{
	Plan = FDCCarPlan();
	Speed = PrevSpeed = 0.f;
	Steer = 0.f;
	Signal = 0;
	FVector L = Slot.GetLocation();
	L.Z = TraceGround(L);
	GroundZ = L.Z;
	SetActorLocationAndRotation(L, FRotator(0.f, Slot.Rotator().Yaw, 0.f));
	SetActorHiddenInGame(false);
	CarState = EDCCarState::Parked;
}

void ADCVehicle::SetGone()
{
	Plan = FDCCarPlan();
	Speed = PrevSpeed = 0.f;
	Signal = 0;
	bEngine = false;
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

void ADCVehicle::StartDrive(const FDCCarPlan& InPlan)
{
	if (!InPlan.IsValid()) { return; }
	Plan = InPlan;
	DriveTime = 0.f;
	TargetSlot = Plan.bArrive ? Plan.Slot : INDEX_NONE;
	CheckGateClearance(Plan, Plan.Slot);
	FVector2D C, F;
	Plan.PoseAt(0.f, C, F);
	const FVector L(C.X, C.Y, 0.f);
	if (Plan.bArrive) { GroundZ = TraceGround(L); }
	SetActorLocationAndRotation(FVector(C.X, C.Y, GroundZ), FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(F.Y, F.X)), 0.f));
	Speed = PrevSpeed = Plan.Samples[0].V;
	SetDoorsOpen(false);
	SetActorHiddenInGame(false);
	CarState = EDCCarState::Driving;
	bEngine = false;
	UE_LOG(LogDC, Log, TEXT("[Vehicle] slot %d bay %d %s: %.0f m, %.1f s"), Plan.Slot, Plan.Bay, Plan.bArrive ? TEXT("drives in") : TEXT("drives off"), Plan.Samples.Last().S / 100.f, Plan.Duration());
}

bool ADCVehicle::PredictPose(float Ahead, FVector2D& OutCentre, FVector2D& OutFwd) const
{
	if (CarState != EDCCarState::Driving || !Plan.IsValid()) { return false; }
	const float T = DriveTime + Ahead;
	if (T >= Plan.Duration()) { return false; }
	Plan.PoseAt(T, OutCentre, OutFwd);
	return true;
}

float ADCVehicle::ArriveSeconds(int32 SlotIndex) const
{
	if (const float* Found = ArriveSecondsCache.Find(SlotIndex)) { return *Found; }
	const float T = PlanDrive(SlotIndex, true).Duration();
	ArriveSecondsCache.Add(SlotIndex, T);
	return T;
}

void ADCVehicle::CheckGateClearance(const FDCCarPlan& P, int32 SlotIndex) const
{
	static TSet<int32> Checked;
	const int32 Key = SlotIndex * 2 + (P.bArrive ? 1 : 0) + (int32)Style * 64;
	if (Checked.Contains(Key) || !P.IsValid()) { return; }
	Checked.Add(Key);
	TArray<FVector4f> Obs;
	DCSite::GateObstacles(Obs);
	float Best = 1e9f;
	FVector2D BestAt = FVector2D::ZeroVector;
	for (const FDCCarPlan::FSample& Smp : P.Samples)
	{
		if (FMath::Abs(Smp.P.Y - DCSite::FenceY) > 900.f || FMath::Abs(Smp.P.X - DCSite::DriveX) > 1500.f) { continue; }
		const FVector2D D = Smp.T, C = Smp.P + Smp.T * (P.WheelBase * 0.5f);
		for (const FVector4f& O : Obs)
		{
			const FVector2D L = FVector2D(O.X, O.Y) - C;
			const float X = FVector2D::DotProduct(L, D), Y = -D.Y * L.X + D.X * L.Y;
			const float Dx = FMath::Max(FMath::Abs(X) - Length * 0.5f, 0.f), Dy = FMath::Max(FMath::Abs(Y) - Width * 0.5f, 0.f);
			const float Dist = FMath::Sqrt(Dx * Dx + Dy * Dy) - O.Z;
			if (Dist < Best) { Best = Dist; BestAt = FVector2D(O.X, O.Y); }
		}
	}
	if (Best < 25.f)
	{
		UE_LOG(LogDC, Warning, TEXT("[Vehicle] slot %d %s: only %.0f cm from the gateway at (%.0f, %.0f)"), SlotIndex, P.bArrive ? TEXT("in") : TEXT("out"), Best, BestAt.X, BestAt.Y);
	}
	else
	{
		UE_LOG(LogDC, Display, TEXT("[Vehicle] slot %d %s: gateway clearance %.0f cm"), SlotIndex, P.bArrive ? TEXT("in") : TEXT("out"), Best);
	}
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

void ADCVehicle::UpdateIndicators(float Dt)
{
	BlinkT = Signal != 0 ? BlinkT + Dt : 0.f; // each signal starts with the lamp on
	const bool bOn = Signal != 0 && FMath::Fmod(BlinkT, 0.72f) < 0.4f;
	for (int32 i = 0; i < IndMID.Num(); ++i)
	{
		if (!IndMID[i]) { continue; }
		const bool bThis = bOn && ((i == 0 && Signal < 0) || (i == 1 && Signal > 0));
		IndMID[i]->SetScalarParameterValue(TEXT("Strength"), bThis ? 26.f : 0.25f);
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

	const bool bDriving = CarState == EDCCarState::Driving && Plan.IsValid();
	float AccelNow = 0.f, Lateral = 0.f;
	if (bDriving)
	{
		// Play the plan back on its own timeline: position, heading, speed and acceleration all come from the
		// precomputed profile, so the motion is smooth at any frame rate and exactly what the director predicted.
		DriveTime += Dt;
		const float Dur = Plan.Duration();
		const FDCCarPlan::FSample S = Plan.At(FMath::Min(DriveTime, Dur));
		Speed = S.V;
		AccelNow = S.A;
		PrevSpeed = Speed;
		// The plan traces the rear axle; the body sits half a wheelbase ahead of it, pointing along the path.
		const FVector2D Centre = S.P + S.T * (WheelBase * 0.5f);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(S.T.Y, S.T.X));
		FVector L(Centre.X, Centre.Y, 0.f);
		GroundZ = FMath::FInterpTo(GroundZ, TraceGround(L), Dt, 10.f);
		L.Z = GroundZ;
		SetActorLocationAndRotation(L, FRotator(0.f, Yaw, 0.f));
		Lateral = Speed * Speed * S.Curv;
		// Bicycle model: the front wheels point where the path bends (positive curvature = turning towards +Y = right).
		const float SteerTarget = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan(WheelBase * S.Curv)), -MaxLock, MaxLock);
		Steer = FMath::FInterpTo(Steer, SteerTarget, Dt, 9.f);
		SpinAngle = FMath::Fmod(SpinAngle - Speed * Dt / WheelR * (180.f / UE_PI), 360.f);
		const bool bStopping = Plan.bArrive && Dur - DriveTime < 0.9f;
		Brake = FMath::FInterpTo(Brake, (AccelNow < -70.f || bStopping) ? 1.f : 0.f, Dt, 8.f);
		Signal = S.Signal;

		if (DriveTime >= Dur)
		{
			if (Plan.bArrive && TargetSlot != INDEX_NONE)
			{
				UE_LOG(LogDC, Log, TEXT("[Vehicle] parked in slot %d after %.1fs"), TargetSlot, DriveTime);
				ParkAt(DCSite::ParkingSlot(TargetSlot));
				Brake = 1.f;
				TargetSlot = INDEX_NONE;
			}
			else
			{
				UE_LOG(LogDC, Log, TEXT("[Vehicle] off-screen after %.1fs"), DriveTime);
				SetGone();
				return;
			}
		}
	}
	else
	{
		Speed = 0.f;
		Signal = 0;
		Steer = FMath::FInterpTo(Steer, 0.f, Dt, 2.5f);
		Brake = FMath::FInterpTo(Brake, bEngine ? 0.35f : 0.f, Dt, 1.5f);
	}

	for (int32 i = 0; i < WheelSteer.Num(); ++i)
	{
		if (WheelSteer[i]) { WheelSteer[i]->SetRelativeRotation(FRotator(0.f, i < 2 ? Steer : 0.f, 0.f)); }
		if (WheelSpin.IsValidIndex(i) && WheelSpin[i]) { WheelSpin[i]->SetRelativeRotation(FRotator(SpinAngle, 0.f, 0.f)); }
	}
	// Body: dives under braking, squats pulling away, leans out of corners, and shimmies with the engine.
	Pitch = FMath::FInterpTo(Pitch, FMath::Clamp(AccelNow * 0.0042f, -1.6f, 1.6f), Dt, 4.f);
	Roll = FMath::FInterpTo(Roll, FMath::Clamp(-Lateral * 0.0016f, -2.f, 2.f), Dt, 4.f);
	BobT += Dt;
	const bool bRunning = bDriving || bEngine;
	const float Shimmy = DCSettings::ReduceMotion() ? 0.f : bDriving ? (0.3f + FMath::Min(Speed, 1200.f) / 1200.f * 0.6f) : bEngine ? 0.3f : 0.1f;
	BodyPivot->SetRelativeLocationAndRotation(FVector(0.f, 0.f, FMath::Sin(BobT * 23.f) * Shimmy + FMath::Sin(BobT * 3.1f) * Shimmy * 0.8f),
		FRotator(Pitch, 0.f, Roll + FMath::Sin(BobT * 17.f) * 0.08f * Shimmy));
	UpdateIndicators(Dt);
	UpdateLights(Dt, bRunning);
}
