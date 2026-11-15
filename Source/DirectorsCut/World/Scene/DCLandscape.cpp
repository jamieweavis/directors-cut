// Director's Cut — rural landscape + global lighting. See DCLandscape.h. Content placement: DCLandscapeBuild.cpp.
#include "World/Scene/DCLandscape.h"
#include "World/Scene/DCDayNight.h"
#include "World/Scene/DCLandscapeMeshes.h"
#include "Core/DCAssetPaths.h"
#include "Core/DCSiteLayout.h"
#include "DirectorsCut.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Core/DCStyle.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	const TCHAR* GroundPath  = TEXT("/Game/DC/Materials/M_DC_Ground.M_DC_Ground");
	const TCHAR* FoliagePath = TEXT("/Game/DC/Materials/M_DC_Foliage.M_DC_Foliage");
	const TCHAR* WaterPath   = TEXT("/Game/DC/Materials/M_DC_Water.M_DC_Water");
	const TCHAR* ProcPath    = TEXT("/Game/DC/Materials/M_DC_Proc.M_DC_Proc");
	const TCHAR* NeonPath    = TEXT("/Game/DC/Materials/M_DC_Neon.M_DC_Neon");
	const TCHAR* DustPath    = TEXT("/Game/DC/Materials/M_DC_Dust.M_DC_Dust");
}

ADCLandscape::ADCLandscape()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);
}

ADCLandscape* ADCLandscape::Get(const UWorld* World)
{
	if (!World) { return nullptr; }
	TActorIterator<ADCLandscape> It(World);
	return It ? *It : nullptr;
}

void ADCLandscape::BuildLandscape()
{
	if (bBuilt) { return; }
	bBuilt = true;
	const double T0 = FPlatformTime::Seconds();
	// garden layout derived from the site contract (the keep-clear zone + yard)
	{
		const FBox2D Clear = DCSite::BuildingClearRect();
		LawnC = Clear.GetCenter();
		LawnR = Clear.GetExtent() + FVector2D(600.f, 520.f);
		FireC = FVector2D(Clear.Min.X - 450.f, FMath::Lerp(DCSite::YardRect().Max.Y, Clear.Max.Y, 0.32f));
		PondC = FVector2D(Clear.Min.X - 1350.f, Clear.Max.Y - 150.f);
	}
	BuildLightingAndPost();
	InitAssets();
	BuildTerrain();
	if (!FParse::Param(FCommandLine::Get(), TEXT("DCNoLand")))
	{
		BuildVegetation();
		BuildTrees();
		BuildFieldsAndFences();
		BuildLaneProps();
		BuildGarden();
		BuildDistant();
		BuildNightLights();
	}
	int32 Inst = 0;
	for (const auto& KV : Batches) { if (KV.Value) { Inst += KV.Value->GetInstanceCount(); } }
	UE_LOG(LogDC, Display, TEXT("[Landscape] built: %d batches, %d instances, %d night lights, %.1f ms"),
		Batches.Num(), Inst, NightLights.Num() + Fireflies.Num(), (FPlatformTime::Seconds() - T0) * 1000.0);

	// review shots
	FString Shots;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCLandShots="), Shots, false))
	{
		FString HoursStr = Shots, FramesStr = TEXT("near");
		Shots.Split(TEXT(":"), &HoursStr, &FramesStr);
		TArray<FString> Parts; HoursStr.ParseIntoArray(Parts, TEXT(","));
		for (const FString& P : Parts) { ShotHours.Add(FCString::Atof(*P)); }
		FramesStr.ParseIntoArray(ShotFrames, TEXT("+"));
		ShotDir = FPaths::ProjectSavedDir() / TEXT("DCLandShots");
		FParse::Value(FCommandLine::Get(), TEXT("DCLandShotDir="), ShotDir);
		IFileManager::Get().MakeDirectory(*ShotDir, true);
		ShotCam = GetWorld()->SpawnActor<ACameraActor>();
		ShotCam->GetCameraComponent()->SetFieldOfView(18.f);
		ShotCam->GetCameraComponent()->SetConstraintAspectRatio(false);
		ShotNext = 6.0;
	}
}

// =====================================================================================================================
// Assets
// =====================================================================================================================

UMaterialInstanceDynamic* ADCLandscape::MakeMID(const TCHAR* ParentPath, FName Key)
{
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, ParentPath);
	if (!Parent) { Parent = LoadObject<UMaterialInterface>(nullptr, DCPaths::BasicShapeMaterial); }
	if (!Parent) { return nullptr; }
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this, Key);
	Mats.Add(Key, MID);
	return MID;
}

void ADCLandscape::Ground(FName Key, int32 Pattern, const FLinearColor& A, const FLinearColor& B, const FLinearColor& C, float Rough, float Axis, float Center, float Width)
{
	UMaterialInstanceDynamic* D = MakeMID(GroundPath, Key);
	if (!D) { return; }
	D->SetVectorParameterValue("ColorA", DCStyle::Tone(A));
	D->SetVectorParameterValue("ColorB", DCStyle::Tone(B));
	D->SetVectorParameterValue("ColorC", DCStyle::Tone(C));
	D->SetScalarParameterValue("Pattern", (float)Pattern);
	D->SetScalarParameterValue("Roughness", Rough);
	D->SetScalarParameterValue("Axis", Axis);
	D->SetScalarParameterValue("Center", Center);
	D->SetScalarParameterValue("Width", Width);
	GroundMIDs.Add(D);
}

void ADCLandscape::Foliage(FName Key, const FLinearColor& A, const FLinearColor& B, const FLinearColor& C, float CMix, float Wind, float AOMin, float UpN)
{
	UMaterialInstanceDynamic* D = MakeMID(FoliagePath, Key);
	if (!D) { return; }
	D->SetVectorParameterValue("ColorA", DCStyle::Tone(A));
	D->SetVectorParameterValue("ColorB", DCStyle::Tone(B));
	D->SetVectorParameterValue("ColorC", DCStyle::Tone(C));
	D->SetScalarParameterValue("CMix", CMix);
	D->SetScalarParameterValue("Wind", Wind);
	D->SetScalarParameterValue("AOMin", AOMin);
	D->SetScalarParameterValue("UpN", UpN);
}

void ADCLandscape::Proc(FName Key, int32 Pattern, const FLinearColor& A, const FLinearColor& B, float Rough, float Scale, float Dirt, float Axis)
{
	UMaterialInstanceDynamic* D = MakeMID(ProcPath, Key);
	if (!D) { return; }
	D->SetVectorParameterValue("ColorA", DCStyle::Tone(A));
	D->SetVectorParameterValue("ColorB", DCStyle::Tone(B));
	D->SetScalarParameterValue("Pattern", (float)Pattern);
	D->SetScalarParameterValue("Roughness", Rough);
	D->SetScalarParameterValue("Scale", Scale);
	D->SetScalarParameterValue("Dirt", DCStyle::Dirt(Dirt));
	D->SetScalarParameterValue("Axis", Axis);
}

void ADCLandscape::Glow(FName Key, const FLinearColor& Color, float Strength, float Flicker)
{
	UMaterialInstanceDynamic* D = MakeMID(NeonPath, Key);
	if (!D) { return; }
	D->SetVectorParameterValue("Color", Color);
	D->SetScalarParameterValue("Strength", 0.f);
	D->SetScalarParameterValue("Flicker", Flicker);
	D->SetScalarParameterValue("Seed", (float)NightGlowMIDs.Num() * 1.7f);
	NightGlowMIDs.Add(D);
	NightGlowBase.Add(Strength);
}

UMaterialInterface* ADCLandscape::M(FName Key) const
{
	const TObjectPtr<UMaterialInterface>* F = Mats.Find(Key);
	return F ? F->Get() : nullptr;
}

void ADCLandscape::InitAssets()
{
	using C = FLinearColor;
	// ---- meshes
	MeshLib.Add("Cube", LoadObject<UStaticMesh>(nullptr, DCPaths::Cube));
	MeshLib.Add("Cyl", LoadObject<UStaticMesh>(nullptr, DCPaths::Cylinder));
	MeshLib.Add("Sphere", LoadObject<UStaticMesh>(nullptr, DCPaths::Sphere));
	MeshLib.Add("Cone", LoadObject<UStaticMesh>(nullptr, DCPaths::Cone));
	MeshLib.Add("Tuft0", DCLandMesh::GrassTuft(this, 9, 0.35f, 11));
	MeshLib.Add("Tuft1", DCLandMesh::GrassTuft(this, 12, 0.55f, 23));
	MeshLib.Add("Tuft2", DCLandMesh::GrassTuft(this, 6, 0.2f, 37));
	MeshLib.Add("Reed", DCLandMesh::GrassTuft(this, 10, 0.12f, 51));
	MeshLib.Add("Flowers0", DCLandMesh::FlowerHeads(this, 3));
	MeshLib.Add("Flowers1", DCLandMesh::FlowerHeads(this, 8));
	MeshLib.Add("Clump0", DCLandMesh::Blob(this, 2, 0.16f, false, 5, 1.f));
	MeshLib.Add("Clump1", DCLandMesh::Blob(this, 2, 0.22f, false, 9, 1.f));
	MeshLib.Add("Hill", DCLandMesh::Blob(this, 3, 0.08f, false, 13, 0.f));
	MeshLib.Add("Rock0", DCLandMesh::Blob(this, 1, 0.3f, true, 17, 0.f));
	MeshLib.Add("Rock1", DCLandMesh::Blob(this, 1, 0.38f, true, 29, 0.f));
	MeshLib.Add("Trunk", DCLandMesh::Tube(this, 7, 0.55f));
	MeshLib.Add("Post", DCLandMesh::Tube(this, 6, 0.9f));
	MeshLib.Add("Skirt", DCLandMesh::Skirt(this, 9, 41));
	MeshLib.Add("Disc", DCLandMesh::Disc(this, 40, 0.12f, 7));
	MeshLib.Add("DiscRound", DCLandMesh::Disc(this, 40, 0.f, 1));

	// ---- ground (M_DC_Ground)
	const C GrassA(0.055f, 0.12f, 0.03f), GrassB(0.15f, 0.155f, 0.06f), GrassC(0.035f, 0.08f, 0.025f);
	Ground("Meadow", 0, GrassA, GrassB, GrassC, 0.92f);
	// the grass round the house: plain, natural meadow ground (no mowing stripes), a touch greener than the field
	Ground("Lawn", 0, C(0.05f, 0.12f, 0.03f), C(0.12f, 0.145f, 0.05f), C(0.035f, 0.085f, 0.025f), 0.92f);
	Ground("Gravel", 2, C(0.30f, 0.27f, 0.22f), C(0.12f, 0.105f, 0.085f), C(0.16f, 0.12f, 0.08f), 0.85f);
	Ground("Lane", 3, C(0.22f, 0.16f, 0.10f), C(0.10f, 0.07f, 0.045f), GrassA, 0.85f, 0.f, DCSite::LaneY, DCSite::LaneW + 240.f);
	Ground("Drive", 3, C(0.22f, 0.16f, 0.10f), C(0.10f, 0.07f, 0.045f), GrassA, 0.85f, 1.f, DCSite::DriveX, DCSite::DriveW + 160.f);
	Ground("Soil", 4, C(0.14f, 0.09f, 0.05f), C(0.06f, 0.038f, 0.022f), C(0.1f, 0.07f, 0.04f), 0.95f, 0.f, 0.f);
	Ground("Stubble", 4, C(0.42f, 0.33f, 0.14f), C(0.25f, 0.19f, 0.08f), C(0.3f, 0.25f, 0.1f), 0.9f, 1.f, 0.f);
	Ground("Path", 5, C(0.20f, 0.15f, 0.09f), C(0.11f, 0.08f, 0.05f), C(0.1f, 0.1f, 0.05f), 0.9f);
	Ground("Mud", 5, C(0.07f, 0.06f, 0.035f), C(0.035f, 0.03f, 0.02f), C(0.05f, 0.05f, 0.03f), 0.5f);
	Ground("HillMat", 0, C(0.05f, 0.11f, 0.035f), C(0.13f, 0.14f, 0.06f), C(0.035f, 0.075f, 0.03f), 0.95f);

	// ---- foliage (M_DC_Foliage)
	Foliage("Grass", C(0.06f, 0.14f, 0.03f), C(0.17f, 0.19f, 0.06f), C(0.26f, 0.22f, 0.09f), 0.55f, 5.f, 0.85f, 1.f);
	Foliage("GrassLush", C(0.045f, 0.13f, 0.03f), C(0.09f, 0.18f, 0.04f), C(0.16f, 0.2f, 0.06f), 0.3f, 4.f, 0.85f, 1.f);
	Foliage("Reeds", C(0.13f, 0.17f, 0.05f), C(0.30f, 0.26f, 0.10f), C(0.35f, 0.3f, 0.12f), 0.4f, 7.f, 0.35f, 0.7f);
	Foliage("Crop", C(0.08f, 0.18f, 0.03f), C(0.14f, 0.21f, 0.05f), C(0.2f, 0.25f, 0.05f), 0.2f, 3.f, 0.55f, 1.f);
	Foliage("PetalWhite", C(0.85f, 0.85f, 0.80f), C(0.95f, 0.92f, 0.75f), C(0.9f, 0.9f, 0.8f), 0.f, 4.f, 1.f);
	Foliage("PetalYellow", C(0.85f, 0.62f, 0.04f), C(0.95f, 0.80f, 0.10f), C(0.9f, 0.7f, 0.1f), 0.f, 4.f, 1.f);
	Foliage("PetalViolet", C(0.35f, 0.15f, 0.65f), C(0.55f, 0.30f, 0.80f), C(0.5f, 0.2f, 0.7f), 0.f, 4.f, 1.f);
	Foliage("PetalRed", C(0.75f, 0.06f, 0.05f), C(0.85f, 0.25f, 0.35f), C(0.8f, 0.1f, 0.1f), 0.f, 4.f, 1.f);
	Foliage("LeafGreen", C(0.045f, 0.11f, 0.03f), C(0.10f, 0.16f, 0.04f), C(0.16f, 0.2f, 0.05f), 0.35f, 2.f, 0.3f);
	Foliage("LeafDeep", C(0.03f, 0.08f, 0.03f), C(0.06f, 0.13f, 0.045f), C(0.08f, 0.13f, 0.04f), 0.2f, 2.f, 0.3f);
	Foliage("LeafGold", C(0.30f, 0.17f, 0.03f), C(0.40f, 0.28f, 0.05f), C(0.5f, 0.2f, 0.03f), 0.3f, 2.f, 0.35f);
	Foliage("LeafRust", C(0.30f, 0.08f, 0.03f), C(0.40f, 0.15f, 0.04f), C(0.45f, 0.25f, 0.05f), 0.3f, 2.f, 0.35f);
	Foliage("Pine", C(0.02f, 0.07f, 0.04f), C(0.04f, 0.10f, 0.05f), C(0.04f, 0.08f, 0.04f), 0.f, 1.f, 0.25f);
	Foliage("Hedge", C(0.035f, 0.10f, 0.025f), C(0.07f, 0.15f, 0.035f), C(0.12f, 0.14f, 0.04f), 0.3f, 1.f, 0.3f);
	Foliage("Lily", C(0.06f, 0.16f, 0.04f), C(0.10f, 0.20f, 0.05f), C(0.1f, 0.2f, 0.05f), 0.f, 0.f, 1.f);

	// ---- hard surfaces (M_DC_Proc)
	Proc("Bark", 1, C(0.10f, 0.075f, 0.055f), C(0.04f, 0.03f, 0.025f), 0.9f, 1.f, 0.5f, 1.f);
	Proc("Timber", 1, C(0.24f, 0.20f, 0.15f), C(0.12f, 0.10f, 0.08f), 0.85f, 1.f, 0.6f, 0.f);
	Proc("TimberV", 1, C(0.24f, 0.20f, 0.15f), C(0.12f, 0.10f, 0.08f), 0.85f, 1.f, 0.6f, 1.f);
	Proc("LogWood", 1, C(0.18f, 0.11f, 0.06f), C(0.08f, 0.05f, 0.03f), 0.85f, 1.f, 0.4f, 1.f);
	Proc("Stone", 3, C(0.34f, 0.33f, 0.30f), C(0.16f, 0.155f, 0.14f), 0.9f, 1.f, 0.8f);
	Proc("StoneDark", 3, C(0.20f, 0.19f, 0.18f), C(0.09f, 0.09f, 0.085f), 0.9f, 1.f, 0.8f);
	Proc("Hay", 12, C(0.52f, 0.40f, 0.15f), C(0.3f, 0.22f, 0.08f), 0.95f, 1.f, 0.3f);
	Proc("BarnRed", 1, C(0.26f, 0.05f, 0.03f), C(0.14f, 0.03f, 0.02f), 0.8f, 1.f, 0.6f, 1.f);
	Proc("Roof", 10, C(0.13f, 0.13f, 0.14f), C(0.17f, 0.08f, 0.04f), 0.55f, 1.f, 0.5f, 1.f);
	Proc("Metal", 7, C(0.28f, 0.29f, 0.30f), C(0.16f, 0.12f, 0.08f), 0.5f, 1.f, 0.4f);
	Proc("PaintWhite", 7, C(0.55f, 0.55f, 0.52f), C(0.3f, 0.3f, 0.28f), 0.5f, 1.f, 0.4f);
	Proc("PaintGreen", 7, C(0.04f, 0.13f, 0.06f), C(0.03f, 0.06f, 0.03f), 0.5f, 1.f, 0.3f);
	Proc("Wire", 0, C(0.02f, 0.02f, 0.02f), C(0.03f, 0.03f, 0.03f), 0.5f, 1.f, 0.f);
	Proc("Ash", 3, C(0.08f, 0.075f, 0.07f), C(0.02f, 0.02f, 0.02f), 0.95f, 2.f, 0.5f);
	Proc("Window", 0, C(0.02f, 0.025f, 0.03f), C(0.02f, 0.025f, 0.03f), 0.2f);

	// ---- night glow (M_DC_Neon, strength fades in at dusk)
	Glow("BulbAmber", C(1.f, 0.45f, 0.12f), 9.f, 0.04f);
	Glow("Lantern", C(1.f, 0.55f, 0.2f), 14.f, 0.06f);
	Glow("FarmWindow", C(1.f, 0.6f, 0.28f), 12.f);
	Glow("Ember", C(1.f, 0.2f, 0.02f), 10.f, 0.6f);
	Glow("Flame", C(1.f, 0.32f, 0.04f), 14.f, 0.35f);
	FireMID = Cast<UMaterialInstanceDynamic>(M("Flame"));

	WaterMID = MakeMID(WaterPath, "Water");
	if (WaterMID)
	{
		WaterMID->SetVectorParameterValue("Color", C(0.012f, 0.03f, 0.03f));
		WaterMID->SetVectorParameterValue("GlowColor", C(0.04f, 0.55f, 0.5f));
		WaterMID->SetScalarParameterValue("Ripple", 0.8f);
	}
	FireflyMID = MakeMID(DustPath, "Firefly");
	if (FireflyMID)
	{
		FireflyMID->SetVectorParameterValue("Color", C(0.75f, 1.f, 0.25f));
		FireflyMID->SetScalarParameterValue("Strength", 0.f);
		FireflyMID->SetScalarParameterValue("Drift", 45.f);
	}
}

// =====================================================================================================================
// Primitive helpers
// =====================================================================================================================

UInstancedStaticMeshComponent* ADCLandscape::Batch(FName MeshKey, FName MatKey, bool bShadow, bool bCollide)
{
	const uint64 Key = ((uint64)GetTypeHash(MeshKey) << 32) ^ ((uint64)GetTypeHash(MatKey) << 1) ^ (bShadow ? 1ull : 0ull) ^ (bCollide ? 0x8000000000000000ull : 0ull);
	if (TObjectPtr<UInstancedStaticMeshComponent>* F = Batches.Find(Key)) { return F->Get(); }
	const TObjectPtr<UStaticMesh>* Mesh = MeshLib.Find(MeshKey);
	if (!Mesh || !*Mesh) { return nullptr; }
	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
	ISM->SetStaticMesh(*Mesh);
	if (UMaterialInterface* Mat = M(MatKey)) { ISM->SetMaterial(0, Mat); }
	ISM->SetMobility(EComponentMobility::Static);
	ISM->SetCastShadow(bShadow);
	ISM->bAffectDistanceFieldLighting = false;
	ISM->bVisibleInRayTracing = false;
	ISM->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Static;
	if (bCollide)
	{
		ISM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		ISM->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	else
	{
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->bDisableCollision = true;
	}
	ISM->SetupAttachment(GetRootComponent());
	ISM->RegisterComponent();
	AddInstanceComponent(ISM);
	Batches.Add(Key, ISM);
	return ISM;
}

void ADCLandscape::Add(FName MeshKey, FName MatKey, const FTransform& T, bool bShadow)
{
	if (UInstancedStaticMeshComponent* ISM = Batch(MeshKey, MatKey, bShadow)) { ISM->AddInstance(T, true); }
}

void ADCLandscape::Box(FName MatKey, const FVector& Ctr, const FVector& Size, const FRotator& R, bool bShadow)
{
	Add("Cube", MatKey, FTransform(R, Ctr, Size / 100.f), bShadow);
}

void ADCLandscape::Rod(FName MatKey, const FVector& A, const FVector& B, float Thick, bool bShadow, bool bRound)
{
	const FVector D = B - A;
	const float Len = D.Size();
	if (Len < 0.1f) { return; }
	const FRotator R = FRotationMatrix::MakeFromZ(D / Len).Rotator();
	Add(bRound ? FName("Cyl") : FName("Cube"), MatKey, FTransform(R, (A + B) * 0.5f, FVector(Thick, Thick, Len) / 100.f), bShadow);
}

UStaticMeshComponent* ADCLandscape::Slab(FName MatKey, UStaticMesh* Mesh, const FVector& Ctr, const FVector& Scale, float Yaw, bool bCollide)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	if (UMaterialInterface* Mat = M(MatKey)) { C->SetMaterial(0, Mat); }
	C->SetMobility(EComponentMobility::Static);
	C->SetupAttachment(GetRootComponent());
	C->SetWorldLocationAndRotation(Ctr, FRotator(0.f, Yaw, 0.f));
	C->SetWorldScale3D(Scale);
	C->bVisibleInRayTracing = false;
	if (bCollide)
	{
		C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	else
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->bAffectDistanceFieldLighting = false;
	}
	C->RegisterComponent();
	AddInstanceComponent(C);
	return C;
}

UPointLightComponent* ADCLandscape::NightPoint(const FVector& P, const FLinearColor& Col, float Candelas, float Radius, bool bShadow, float Flicker)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetMobility(EComponentMobility::Movable);
	L->SetupAttachment(GetRootComponent());
	L->SetWorldLocation(P);
	L->IntensityUnits = ELightUnits::Candelas;
	L->Intensity = 0.f;
	L->SetLightColor(Col);
	L->AttenuationRadius = Radius;
	L->SourceRadius = 4.f;
	L->SetCastShadows(bShadow);
	L->bAffectTranslucentLighting = false;
	L->SetVolumetricScatteringIntensity(0.f); // per-light volumetric injection is the night-time cost
	L->SetVisibility(false);
	L->RegisterComponent();
	AddInstanceComponent(L);
	NightLights.Add({ L, Candelas, (float)NightLights.Num() * 2.37f, Flicker });
	return L;
}

// =====================================================================================================================
// Lighting / post
// =====================================================================================================================

void ADCLandscape::SetCVars()
{
	auto Set = [](const TCHAR* Name, const TCHAR* Value)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name)) { V->Set(Value, ECVF_SetByCode); }
	};
	Set(TEXT("r.EyeAdaptation.CachedLightingPreExposure"), TEXT("4"));
	Set(TEXT("r.LocalFogVolume.RenderIntoVolumetricFog"), TEXT("1"));
	Set(TEXT("r.LocalFogVolume.MaxDensityIntoVolumetricFog"), TEXT("0.5"));
	Set(TEXT("r.VolumetricFog.GridPixelSize"), TEXT("16"));
	Set(TEXT("r.VolumetricFog.GridSizeZ"), TEXT("64"));
	Set(TEXT("r.VolumetricFog.HistoryWeight"), TEXT("0.9"));
	Set(TEXT("r.MotionBlurQuality"), TEXT("0"));
	// the camera sits 25-45 m away: keep shadow detail across the whole diorama
	Set(TEXT("r.Shadow.Virtual.ResolutionLodBiasDirectional"), TEXT("-0.5"));
	FString Extra;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCCVars="), Extra, false))
	{
		// -DCCVars=name=value;name=value  (perf experiments)
		TArray<FString> Pairs; Extra.ParseIntoArray(Pairs, TEXT(";"));
		for (const FString& P : Pairs) { FString K, V; if (P.Split(TEXT("="), &K, &V)) { Set(*K, *V); } }
	}
}

void ADCLandscape::BuildLightingAndPost()
{
	SetCVars();

	APostProcessVolume* PPV = GetWorld()->SpawnActor<APostProcessVolume>();
	Post = PPV;
	PPV->bUnbound = true;
	PPV->Priority = -1.f; // gameplay (camera / reveal) volumes can override
	FPostProcessSettings& S = PPV->Settings;
	S.bOverride_AutoExposureMethod = true; S.AutoExposureMethod = AEM_Manual;
	S.bOverride_AutoExposureBias = true; S.AutoExposureBias = -2.f; // driven by ADCDayNight
	S.bOverride_AutoExposureApplyPhysicalCameraExposure = true; S.AutoExposureApplyPhysicalCameraExposure = false;
	S.bOverride_LocalExposureMethod = true; S.LocalExposureMethod = ELocalExposureMethod::Bilateral;
	S.bOverride_LocalExposureHighlightContrastScale = true; S.LocalExposureHighlightContrastScale = 0.85f;
	S.bOverride_LocalExposureShadowContrastScale = true; S.LocalExposureShadowContrastScale = 0.9f;
	S.bOverride_BloomMethod = true; S.BloomMethod = BM_SOG;
	S.bOverride_BloomIntensity = true; S.BloomIntensity = 0.55f;  // driven by ADCDayNight (stronger at night)
	S.bOverride_BloomThreshold = true; S.BloomThreshold = -1.f;
	S.bOverride_FilmGrainIntensity = true; S.FilmGrainIntensity = 0.f;
	S.bOverride_VignetteIntensity = true; S.VignetteIntensity = 0.28f;
	S.bOverride_SceneFringeIntensity = true; S.SceneFringeIntensity = 0.f;
	S.bOverride_MotionBlurAmount = true; S.MotionBlurAmount = 0.f;
	S.bOverride_LensFlareIntensity = true; S.LensFlareIntensity = 0.f;
	// toybox grade: clean saturated colour, soft violet-blue shadows that never go black, warm highlights
	S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(1.f, 1.f, 1.f, 1.12f);
	S.bOverride_ColorContrast = true; S.ColorContrast = FVector4(1.f, 1.f, 1.f, 1.03f);
	S.bOverride_ColorGainShadows = true; S.ColorGainShadows = FVector4(0.95f, 0.98f, 1.10f, 1.06f);
	S.bOverride_ColorOffsetShadows = true; S.ColorOffsetShadows = FVector4(0.004f, 0.003f, 0.010f, 0.f);
	S.bOverride_ColorGainHighlights = true; S.ColorGainHighlights = FVector4(1.04f, 1.0f, 0.95f, 1.f);
	S.bOverride_FilmToe = true; S.FilmToe = 0.42f;
	S.bOverride_ToneCurveAmount = true; S.ToneCurveAmount = 1.f;
	S.bOverride_LumenSceneLightingQuality = true; S.LumenSceneLightingQuality = 1.f;
	S.bOverride_LumenFinalGatherQuality = true; S.LumenFinalGatherQuality = 1.f;
	S.bOverride_LumenSkylightLeaking = true; S.LumenSkylightLeaking = 0.3f;
	S.bOverride_IndirectLightingIntensity = true; S.IndirectLightingIntensity = 1.1f;

	// Ink outlines + cel-banded lighting (one cheap post pass; see Tools/style_materials.py and Core/DCStyle.h).
	if (DCStyle::ToonEnabled())
	{
		if (UMaterialInterface* Toon = LoadObject<UMaterialInterface>(nullptr, DCStyle::ToonMaterialPath))
		{
			ToonMID = UMaterialInstanceDynamic::Create(Toon, this);
			auto Param = [this](const TCHAR* Flag, const TCHAR* Name)
			{
				float V = 0.f;
				if (FParse::Value(FCommandLine::Get(), Flag, V)) { ToonMID->SetScalarParameterValue(Name, V); }
			};
			Param(TEXT("DCToonOutline="), TEXT("Outline"));
			Param(TEXT("DCToonThick="), TEXT("Thickness"));
			Param(TEXT("DCToonBand="), TEXT("Band"));
			Param(TEXT("DCToonStep="), TEXT("BandStep"));
			Param(TEXT("DCToonSoft="), TEXT("BandSoft"));
			Param(TEXT("DCToonDepth="), TEXT("DepthEdge"));
			Param(TEXT("DCToonNormal="), TEXT("NormalEdge"));
			S.AddBlendable(ToonMID, 1.f);
		}
		else { UE_LOG(LogDC, Warning, TEXT("[Landscape] M_DC_Toon missing: run Tools/style_materials.py")); }
	}

	// Height fog: a thin distance haze + a ground-hugging mist layer (both driven per hour by ADCDayNight).
	AExponentialHeightFog* Fog = GetWorld()->SpawnActor<AExponentialHeightFog>(FVector(0.f, 0.f, -3.f), FRotator::ZeroRotator);
	UExponentialHeightFogComponent* F = Fog ? Fog->GetComponent() : nullptr;
	if (F)
	{
		F->SetFogDensity(0.01f);
		F->SetFogHeightFalloff(0.2f);
		F->SetSecondFogDensity(0.f);
		F->SetSecondFogHeightFalloff(2.5f);
		F->SetSecondFogHeightOffset(0.f);
		F->SetFogInscatteringColor(FLinearColor(0.4f, 0.48f, 0.6f));
		F->SetDirectionalInscatteringColor(FLinearColor(0.3f, 0.2f, 0.1f));
		F->SetDirectionalInscatteringExponent(6.f);
		F->SetStartDistance(0.f);
		F->SetVolumetricFog(true);
		F->SetVolumetricFogScatteringDistribution(0.5f);
		F->SetVolumetricFogAlbedo(FColor::White);
		F->SetVolumetricFogExtinctionScale(1.f);
		F->SetVolumetricFogStartDistance(2500.f);
		F->SetVolumetricFogDistance(26000.f);
	}

	// Day/night rig
	FActorSpawnParameters P; P.Name = TEXT("DayNight");
	DayNight = GetWorld()->SpawnActor<ADCDayNight>(ADCDayNight::StaticClass(), FTransform::Identity, P);
	if (DayNight)
	{
		DayNight->Post = PPV;
		DayNight->HeightFog = F;
	}
	HeightFog = F;
}

// =====================================================================================================================
// Runtime: night lights, fireflies, wind rotor, haze colour, review shots
// =====================================================================================================================

void ADCLandscape::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	PerfAccum += DeltaSeconds; ++PerfFrames;
	if (PerfAccum > 2.0) { PerfAccum = DeltaSeconds; PerfFrames = 1; }

	const float Target = DayNight ? DayNight->GetLampAlpha() : 0.f;
	NightK = Target;
	const float T = (float)Time;

	for (const FNightLight& N : NightLights)
	{
		ULightComponent* L = N.Light.Get();
		if (!L) { continue; }
		float K = NightK;
		if (N.Flicker > 0.f)
		{
			const float f = 0.55f * FMath::Sin(T * 9.1f + N.Seed) + 0.3f * FMath::Sin(T * 23.7f + N.Seed * 3.f) + 0.15f * FMath::Sin(T * 51.f);
			K *= 1.f + N.Flicker * f;
		}
		L->SetIntensity(N.Base * K);
		L->SetVisibility(NightK > 0.005f);
	}
	for (int32 i = 0; i < NightGlowMIDs.Num(); ++i)
	{
		if (NightGlowMIDs[i]) { NightGlowMIDs[i]->SetScalarParameterValue("Strength", NightGlowBase[i] * (0.02f + 0.98f * NightK)); }
	}
	if (WaterMID) { WaterMID->SetScalarParameterValue("Glow", 0.18f * NightK); }
	if (FireflyMID) { FireflyMID->SetScalarParameterValue("Strength", 40.f * NightK); }
	for (const FFirefly& F : Fireflies)
	{
		UPointLightComponent* L = F.Light.Get();
		if (!L) { continue; }
		const float s = F.Seed;
		const FVector Off(FMath::Sin(T * 0.23f + s) * 260.f + FMath::Sin(T * 0.61f + s * 2.f) * 90.f,
		                  FMath::Cos(T * 0.19f + s * 1.3f) * 260.f + FMath::Cos(T * 0.53f + s) * 90.f,
		                  FMath::Sin(T * 0.41f + s * 0.7f) * 35.f);
		L->SetWorldLocation(F.Home + Off);
		const float Blink = FMath::Clamp(FMath::Sin(T * (0.9f + 0.13f * s) + s * 5.f) * 1.6f + 0.2f, 0.f, 1.f);
		L->SetIntensity(2.5f * NightK * Blink);
		L->SetVisibility(NightK > 0.01f);
	}
	if (WindRotor) { WindRotor->AddLocalRotation(FRotator(0.f, 0.f, DeltaSeconds * 40.f)); }
	if (DayNight)
	{
		const FLinearColor Haze = DayNight->GetHazeColor();
		for (UMaterialInstanceDynamic* G : GroundMIDs) { if (G) { G->SetVectorParameterValue("FarColor", Haze); } }
	}
	if (ShotHours.Num()) { TickShots(); }
}

void ADCLandscape::TickShots()
{
	if (Time < ShotNext || !DayNight) { return; }
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const int32 NumFrames = FMath::Max(1, ShotFrames.Num());
	const int32 Total = ShotHours.Num() * NumFrames;
	if (ShotStep >= Total)
	{
		ShotHours.Reset();
		if (GEngine) { GEngine->Exec(GetWorld(), TEXT("quit")); }
		return;
	}
	const int32 HI = ShotStep / NumFrames, FI = ShotStep % NumFrames;
	const FString Frame = ShotFrames.IsValidIndex(FI) ? ShotFrames[FI] : TEXT("near");
	if (ShotPhase == 0)
	{
		if (FI == 0) { DayNight->SetFixedHour(ShotHours[HI], true); }
		// frame: near = default overview of CameraBounds, far = zoomed right out, close = the yard/garden
		const FBox B = DCSite::CameraBounds();
		FVector Pivot = B.GetCenter(); Pivot.Z = 0.f;
		float Dist = B.GetExtent().Size2D() / FMath::Tan(FMath::DegreesToRadians(9.f)) * 0.82f;
		if (Frame == TEXT("far")) { Dist *= 1.3f; Pivot += FVector(1500.f, 1500.f, 0.f); }
		else if (Frame == TEXT("close")) { Dist *= 0.4f; Pivot = FVector((DCSite::YardRect().Min.X + FireC.X) * 0.5f, (DCSite::YardRect().Max.Y + FireC.Y) * 0.5f, 0.f); }
		else if (Frame == TEXT("pond")) { Dist *= 0.3f; Pivot = FVector(PondC.X, PondC.Y, 0.f); }
		const FRotator Rot(-35.f, 45.f, 0.f);
		ShotCam->SetActorLocationAndRotation(Pivot - Rot.Vector() * Dist, Rot);
		if (PC) { PC->SetViewTarget(ShotCam); }
		ShotPhase = 1;
		ShotNext = Time + (FI == 0 ? 5.0 : 2.0);
		return;
	}
	const FString Hour = FString::Printf(TEXT("%04.1f"), ShotHours[HI]).Replace(TEXT("."), TEXT("_"));
	const FString File = ShotDir / FString::Printf(TEXT("land_%s_%s.png"), *Hour, *Frame);
	FScreenshotRequest::RequestScreenshot(File, false, false);
	UE_LOG(LogDC, Display, TEXT("[LandShots] %s  avg frame %.2f ms (%.0f fps) hour %.2f"), *File,
		PerfFrames ? PerfAccum / PerfFrames * 1000.0 : 0.0, PerfFrames ? PerfFrames / PerfAccum : 0.0, DayNight->GetDisplayHour());
	ShotPhase = 0;
	++ShotStep;
	ShotNext = Time + 0.5;
}
