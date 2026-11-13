// Director's Cut — hideout diorama builder: kit, shell, windows, roof, lighting, post, runtime FX.
// Room dressing is in DCHideoutRooms.cpp, the shared prop kit in DCHideoutKit.cpp, the day/night rig in DCDayNight.cpp.
#include "World/Scene/DCHideoutBuilder.h"
#include "World/Scene/DCDayNight.h"
#include "World/DCRoomModule.h"
#include "World/DCStation.h"
#include "World/DCCriminalSpawnZone.h"
#include "World/DCGarageDoor.h"
#include "Core/DCAssetPaths.h"
#include "Core/DCWorldTags.h"
#include "Core/DCDevTools.h"
#include "DirectorsCut.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/BoxComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/StaticMesh.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace DCScene
{
	constexpr float TallH = 380.f;   // exterior back walls / office-ops wall
	constexpr float PartH = 55.f;    // interior partitions: knee height so no room hides its neighbour
	constexpr float KneeH = 45.f;    // camera-facing exterior walls (cutaway)
	constexpr float WallT = 20.f;
	constexpr float WinBottom = 170.f, WinTop = 342.f;
}
using namespace DCScene;

ADCHideoutBuilder::ADCHideoutBuilder()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

// =====================================================================================================================
// Assets
// =====================================================================================================================

void ADCHideoutBuilder::InitAssets()
{
	const TCHAR* Paths[] = { DCPaths::Cube, DCPaths::Cylinder, DCPaths::Sphere, DCPaths::Cone, DCPaths::Plane };
	for (const TCHAR* P : Paths) { Meshes.Add(LoadObject<UStaticMesh>(nullptr, P)); }

	using C = FLinearColor;
	// --- architecture (realistic, dark, desaturated)
	Proc("Slab",       14, C(0.25f, 0.245f, 0.235f), C(0.13f, 0.128f, 0.125f), 0.32f, 0.f, 1.f, 0.6f);
	Proc("SlabDark",   14, C(0.17f, 0.165f, 0.16f), C(0.06f, 0.06f, 0.058f), 0.4f, 0.f, 1.f, 1.0f);
	Proc("Epoxy",      14, C(0.20f, 0.24f, 0.235f), C(0.15f, 0.18f, 0.175f), 0.18f, 0.f, 1.f, 0.4f);
	Proc("CarpetTile",  8, C(0.06f, 0.063f, 0.068f), C(0.045f, 0.048f, 0.052f), 0.95f, 0.f, 0.8f, 0.4f);
	Proc("RaisedFloor", 8, C(0.30f, 0.31f, 0.32f), C(0.26f, 0.27f, 0.28f), 0.45f, 0.f, 0.9f, 0.3f);
	Proc("Brick",       2, C(0.19f, 0.075f, 0.05f), C(0.11f, 0.105f, 0.10f), 0.88f, 0.f, 1.f, 0.7f);
	Proc("BrickGrey",   2, C(0.13f, 0.125f, 0.12f), C(0.07f, 0.07f, 0.068f), 0.9f, 0.f, 1.f, 0.7f);
	Proc("BrickPaint",  2, C(0.42f, 0.42f, 0.40f), C(0.34f, 0.34f, 0.32f), 0.8f, 0.f, 1.f, 0.8f);
	Proc("Block",      15, C(0.30f, 0.305f, 0.30f), C(0.08f, 0.08f, 0.08f), 0.85f, 0.f, 1.f, 0.5f);
	Proc("BlockDark",  15, C(0.06f, 0.063f, 0.068f), C(0.03f, 0.03f, 0.03f), 0.8f, 0.f, 1.f, 0.3f);
	Proc("BlockGreen", 15, C(0.20f, 0.27f, 0.25f), C(0.07f, 0.08f, 0.08f), 0.7f, 0.f, 1.f, 0.4f);
	Proc("Concrete",    3, C(0.20f, 0.195f, 0.19f), C(0.09f, 0.09f, 0.088f), 0.85f, 0.f, 1.f, 0.8f);
	Proc("Plinth",      3, C(0.03f, 0.03f, 0.032f), C(0.015f, 0.015f, 0.016f), 0.8f, 0.f, 1.f, 0.f);
	Proc("Asphalt",    18, C(0.045f, 0.045f, 0.048f), C(0.015f, 0.015f, 0.016f), 0.75f, 0.f, 1.f, 0.4f);
	Proc("Corrugated", 10, C(0.16f, 0.17f, 0.18f), C(0.12f, 0.06f, 0.03f), 0.5f, 0.6f, 1.f, 0.4f, 0.f);
	Proc("CorrugatedV",10, C(0.10f, 0.12f, 0.13f), C(0.11f, 0.06f, 0.03f), 0.5f, 0.6f, 1.f, 0.4f, 1.f);
	Proc("DiamondPlate",16, C(0.40f, 0.41f, 0.42f), C(0.18f, 0.17f, 0.16f), 0.4f, 1.f, 1.f, 0.4f);
	Proc("Plywood",    17, C(0.40f, 0.29f, 0.17f), C(0.24f, 0.16f, 0.09f), 0.85f, 0.f, 1.f, 0.4f);
	Proc("Wood",        1, C(0.16f, 0.09f, 0.05f), C(0.07f, 0.04f, 0.02f), 0.5f, 0.f, 1.f, 0.3f);
	Proc("Walnut",      1, C(0.09f, 0.05f, 0.03f), C(0.035f, 0.02f, 0.012f), 0.35f, 0.f, 1.5f, 0.1f, 1.f);
	Proc("Hazard",      9, C(0.70f, 0.48f, 0.02f), C(0.015f, 0.015f, 0.015f), 0.6f);
	Proc("Caution",     0, C(0.75f, 0.55f, 0.02f), C(0.7f, 0.5f, 0.02f), 0.5f, 0.f, 1.f, 0.f);
	// --- metals & paint
	Proc("SteelBlack",  7, C(0.025f, 0.026f, 0.028f), C(0.05f, 0.04f, 0.035f), 0.55f, 0.6f);
	Proc("Steel",       7, C(0.32f, 0.33f, 0.34f), C(0.16f, 0.12f, 0.09f), 0.42f, 1.f);
	Proc("Galv",        7, C(0.38f, 0.39f, 0.40f), C(0.24f, 0.24f, 0.24f), 0.45f, 1.f);
	Proc("Alu",         7, C(0.55f, 0.56f, 0.58f), C(0.4f, 0.4f, 0.42f), 0.3f, 1.f);
	Proc("Chrome",      7, C(0.75f, 0.75f, 0.78f), C(0.5f, 0.5f, 0.52f), 0.12f, 1.f);
	Proc("PaintGrey",   7, C(0.11f, 0.12f, 0.125f), C(0.07f, 0.07f, 0.07f), 0.5f, 0.f);
	Proc("PaintLight",  7, C(0.38f, 0.39f, 0.38f), C(0.25f, 0.25f, 0.24f), 0.45f, 0.f);
	Proc("PaintRed",    7, C(0.28f, 0.025f, 0.02f), C(0.1f, 0.02f, 0.015f), 0.4f, 0.f);
	Proc("PaintBlue",   7, C(0.02f, 0.06f, 0.13f), C(0.02f, 0.03f, 0.05f), 0.45f, 0.f);
	Proc("PaintOrange", 7, C(0.55f, 0.16f, 0.02f), C(0.2f, 0.08f, 0.02f), 0.45f, 0.f);
	Proc("PaintOlive",  7, C(0.07f, 0.08f, 0.05f), C(0.04f, 0.04f, 0.03f), 0.5f, 0.f);
	Proc("Rack",        7, C(0.02f, 0.022f, 0.025f), C(0.04f, 0.04f, 0.045f), 0.45f, 0.7f);
	// --- soft goods & props
	Proc("Black",       0, C(0.012f, 0.012f, 0.014f), C(0.02f, 0.02f, 0.022f), 0.45f);
	Proc("PlasticBlack",0, C(0.015f, 0.015f, 0.017f), C(0.02f, 0.02f, 0.022f), 0.35f, 0.f, 1.f, 0.f);
	Proc("PlasticWhite",0, C(0.55f, 0.56f, 0.56f), C(0.45f, 0.45f, 0.45f), 0.35f, 0.f, 1.f, 0.1f);
	Proc("Rubber",      0, C(0.012f, 0.012f, 0.012f), C(0.02f, 0.02f, 0.02f), 0.85f);
	Proc("RubberMat",  13, C(0.03f, 0.03f, 0.032f), C(0.012f, 0.012f, 0.013f), 0.8f, 0.f, 1.f, 0.3f);
	Proc("Fabric",      5, C(0.07f, 0.075f, 0.08f), C(0.035f, 0.037f, 0.04f), 0.95f);
	Proc("FabricOlive", 5, C(0.07f, 0.075f, 0.05f), C(0.035f, 0.04f, 0.025f), 0.95f);
	Proc("Leather",     6, C(0.025f, 0.022f, 0.02f), C(0.07f, 0.055f, 0.045f), 0.45f, 0.f, 1.f, 0.2f);
	Proc("LeatherBrn",  6, C(0.09f, 0.045f, 0.025f), C(0.16f, 0.09f, 0.05f), 0.5f, 0.f, 1.f, 0.2f);
	Proc("Linen",      12, C(0.40f, 0.41f, 0.41f), C(0.3f, 0.3f, 0.3f), 0.95f, 0.f, 1.f, 0.3f);
	Proc("Blanket",    12, C(0.08f, 0.09f, 0.06f), C(0.05f, 0.05f, 0.04f), 0.95f, 0.f, 1.f, 0.2f);
	Proc("BlanketGrey",12, C(0.12f, 0.12f, 0.13f), C(0.08f, 0.08f, 0.08f), 0.95f, 0.f, 1.f, 0.2f);
	Proc("Tarp",       12, C(0.30f, 0.33f, 0.36f), C(0.2f, 0.22f, 0.24f), 0.55f, 0.f, 1.f, 0.4f);
	Proc("Nylon",       0, C(0.018f, 0.018f, 0.02f), C(0.03f, 0.03f, 0.035f), 0.6f, 0.f, 1.f, 0.f);
	Proc("NylonOlive",  0, C(0.05f, 0.055f, 0.035f), C(0.035f, 0.04f, 0.025f), 0.7f, 0.f, 1.f, 0.f);
	Proc("Cardboard",   0, C(0.30f, 0.21f, 0.12f), C(0.22f, 0.15f, 0.08f), 0.9f, 0.f, 1.f, 0.4f);
	Proc("Paper",       0, C(0.70f, 0.69f, 0.65f), C(0.55f, 0.54f, 0.5f), 0.85f, 0.f, 3.f, 0.15f);
	Proc("Photo",       0, C(0.05f, 0.05f, 0.06f), C(0.2f, 0.2f, 0.2f), 0.4f, 0.f, 6.f, 0.f);
	Proc("Cork",        4, C(0.36f, 0.22f, 0.10f), C(0.2f, 0.11f, 0.05f), 0.9f);
	Proc("Felt",       12, C(0.02f, 0.045f, 0.07f), C(0.01f, 0.025f, 0.04f), 1.f, 0.f, 2.f, 0.f);
	Proc("Blueprint",  11, C(0.04f, 0.09f, 0.26f), C(0.04f, 0.09f, 0.26f), 0.8f, 0.f, 4.f, 0.f, 0.f, C(0.1f, 0.25f, 0.8f), 0.25f);
	Proc("Cash",        0, C(0.16f, 0.2f, 0.14f), C(0.22f, 0.26f, 0.18f), 0.8f, 0.f, 5.f, 0.f);
	Proc("Pin",         0, C(0.6f, 0.02f, 0.02f), C(0.5f, 0.01f, 0.01f), 0.3f);
	Proc("Ember",       0, C(0.3f, 0.05f, 0.f), C(0.3f, 0.05f, 0.f), 0.5f, 0.f, 1.f, 0.f, 0.f, C(1.f, 0.25f, 0.02f), 25.f);
	Proc("Ball1",       0, C(0.6f, 0.45f, 0.05f), C(0.6f, 0.45f, 0.05f), 0.1f, 0.f, 1.f, 0.f);
	Proc("Ball2",       0, C(0.45f, 0.02f, 0.02f), C(0.45f, 0.02f, 0.02f), 0.1f, 0.f, 1.f, 0.f);
	Proc("BallW",       0, C(0.75f, 0.74f, 0.7f), C(0.75f, 0.74f, 0.7f), 0.1f, 0.f, 1.f, 0.f);
	Proc("Bottle",      0, C(0.05f, 0.08f, 0.03f), C(0.03f, 0.05f, 0.02f), 0.05f, 0.f, 1.f, 0.f);
	Proc("Plant",       5, C(0.03f, 0.07f, 0.03f), C(0.015f, 0.04f, 0.02f), 0.7f, 0.f, 3.f, 0.f);
	Proc("CarPaint",    7, C(0.012f, 0.013f, 0.015f), C(0.02f, 0.02f, 0.022f), 0.18f, 0.6f, 0.3f, 0.1f);
	Proc("CarGrey",     7, C(0.10f, 0.105f, 0.11f), C(0.06f, 0.06f, 0.065f), 0.22f, 0.7f, 0.3f, 0.2f);
	Proc("CarWhite",    7, C(0.50f, 0.51f, 0.51f), C(0.32f, 0.32f, 0.32f), 0.3f, 0.2f, 0.3f, 0.5f);
	Proc("Tire",        0, C(0.015f, 0.015f, 0.015f), C(0.025f, 0.025f, 0.025f), 0.9f);
	// --- emissive (M_DC_Neon, no flicker unless asked)
	Emis("LedCool",     FLinearColor(0.82f, 0.9f, 1.f), 30.f);
	Emis("LedWarm",     FLinearColor(1.f, 0.78f, 0.55f), 22.f);
	Emis("Accent",      FLinearColor(0.05f, 0.65f, 1.f), 14.f);
	Emis("Bulb",        FLinearColor(1.f, 0.7f, 0.42f), 30.f);
	Emis("Head",        FLinearColor(0.9f, 0.95f, 1.f), 14.f);
	Emis("Tail",        FLinearColor(1.f, 0.02f, 0.02f), 10.f);
	Emis("ExitGreen",   FLinearColor(0.1f, 1.f, 0.35f), 8.f);
	Emis("WorkLed",     FLinearColor(0.9f, 0.95f, 1.f), 45.f);
	Emis("Laptop",      FLinearColor(0.35f, 0.55f, 0.85f), 2.5f);
	Emis("TV",          FLinearColor(0.35f, 0.45f, 0.6f), 2.f);
	Neon("LedG",        FLinearColor(0.1f, 1.f, 0.25f), 18.f, 0.9f, 1.f);
	Neon("LedA",        FLinearColor(1.f, 0.5f, 0.05f), 18.f, 0.9f, 2.f);
	Neon("LedR",        FLinearColor(1.f, 0.05f, 0.05f), 18.f, 0.9f, 4.f);
	Neon("LedB",        FLinearColor(0.1f, 0.4f, 1.f), 18.f, 0.9f, 6.f);
	Neon("Fluoro",      FLinearColor(0.85f, 0.95f, 1.f), 30.f, 0.35f, 8.f);
	Neon("LampHead",    FLinearColor(1.f, 0.62f, 0.3f), 0.15f, 0.f, 0.f);  // street lamps: DayNight drives Strength
	// --- special
	Special("Glass", TEXT("/Game/DC/Materials/M_DC_GlassI.M_DC_GlassI"), [](UMaterialInstanceDynamic* D) { D->SetVectorParameterValue("Color", FLinearColor(0.3f, 0.35f, 0.38f)); D->SetScalarParameterValue("Opacity", 0.22f); });
	Special("TintGlass", TEXT("/Game/DC/Materials/M_DC_GlassI.M_DC_GlassI"), [](UMaterialInstanceDynamic* D) { D->SetVectorParameterValue("Color", FLinearColor(0.01f, 0.012f, 0.015f)); D->SetScalarParameterValue("Opacity", 0.88f); D->SetScalarParameterValue("Roughness", 0.04f); });
	Special("Plastic", TEXT("/Game/DC/Materials/M_DC_GlassI.M_DC_GlassI"), [](UMaterialInstanceDynamic* D) { D->SetVectorParameterValue("Color", FLinearColor(0.55f, 0.6f, 0.62f)); D->SetScalarParameterValue("Opacity", 0.45f); D->SetScalarParameterValue("Roughness", 0.3f); });
	Special("Screen0", TEXT("/Game/DC/Materials/M_DC_Screen.M_DC_Screen"), [](UMaterialInstanceDynamic* D) { D->SetScalarParameterValue("Mode", 0.f); D->SetScalarParameterValue("Strength", 2.2f); });
	Special("Screen1", TEXT("/Game/DC/Materials/M_DC_Screen.M_DC_Screen"), [](UMaterialInstanceDynamic* D) { D->SetScalarParameterValue("Mode", 1.f); D->SetScalarParameterValue("Strength", 2.6f); D->SetVectorParameterValue("Color", FLinearColor(0.15f, 0.65f, 1.f)); });
	Special("Screen2", TEXT("/Game/DC/Materials/M_DC_Screen.M_DC_Screen"), [](UMaterialInstanceDynamic* D) { D->SetScalarParameterValue("Mode", 2.f); D->SetScalarParameterValue("Strength", 3.f); D->SetVectorParameterValue("Color", FLinearColor(1.f, 0.25f, 0.1f)); });
	Special("Window", TEXT("/Game/DC/Materials/M_DC_Window.M_DC_Window"), [](UMaterialInstanceDynamic* D) { D->SetScalarParameterValue("Strength", 1.f); });
	Special("Fence", TEXT("/Game/DC/Materials/M_DC_Fence.M_DC_Fence"), [](UMaterialInstanceDynamic* D) { D->SetScalarParameterValue("Scale", 1.f); });
	Special("Smoke", TEXT("/Game/DC/Materials/M_DC_Smoke.M_DC_Smoke"), [](UMaterialInstanceDynamic* D) { D->SetVectorParameterValue("Color", FLinearColor(1.6f, 1.6f, 1.8f)); D->SetScalarParameterValue("Opacity", 0.35f); });
	Special("Dust", TEXT("/Game/DC/Materials/M_DC_Dust.M_DC_Dust"), [](UMaterialInstanceDynamic* D) { D->SetVectorParameterValue("Color", FLinearColor(1.f, 0.88f, 0.7f)); D->SetScalarParameterValue("Strength", 6.f); D->SetScalarParameterValue("Drift", 18.f); });
}

UMaterialInterface* ADCHideoutBuilder::M(FName Key) const
{
	if (const TObjectPtr<UMaterialInterface>* F = Mats.Find(Key)) { return F->Get(); }
	UE_LOG(LogDC, Warning, TEXT("[Scene] missing material key %s"), *Key.ToString());
	return nullptr;
}

void ADCHideoutBuilder::Proc(FName Key, int32 Pattern, const FLinearColor& A, const FLinearColor& B, float Rough, float Metal, float Scale, float Dirt, float Axis, const FLinearColor& Emissive, float EmissiveStrength)
{
	static UMaterialInterface* ProcBase = nullptr;
	if (!ProcBase) { ProcBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DC/Materials/M_DC_Proc.M_DC_Proc")); }
	UMaterialInstanceDynamic* D = nullptr;
	if (ProcBase)
	{
		D = UMaterialInstanceDynamic::Create(ProcBase, this);
		D->SetVectorParameterValue("ColorA", A);
		D->SetVectorParameterValue("ColorB", B);
		D->SetScalarParameterValue("Pattern", (float)Pattern);
		D->SetScalarParameterValue("Scale", Scale);
		D->SetScalarParameterValue("Dirt", Dirt);
		D->SetScalarParameterValue("Axis", Axis);
	}
	else if (UMaterialInterface* Surf = LoadObject<UMaterialInterface>(nullptr, DCPaths::MatSurface))
	{
		D = UMaterialInstanceDynamic::Create(Surf, this);
		D->SetVectorParameterValue("BaseColor", A);
	}
	else
	{
		D = UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr, DCPaths::BasicShapeMaterial), this);
		D->SetVectorParameterValue("Color", A);
	}
	D->SetScalarParameterValue("Roughness", Rough);
	D->SetScalarParameterValue("Metallic", Metal);
	D->SetVectorParameterValue("EmissiveColor", Emissive);
	D->SetScalarParameterValue("EmissiveStrength", EmissiveStrength);
	Mats.Add(Key, D);
}

void ADCHideoutBuilder::Emis(FName Key, const FLinearColor& Color, float Strength)
{
	Neon(Key, Color, Strength, 0.f, 0.f);
}

void ADCHideoutBuilder::Neon(FName Key, const FLinearColor& Color, float Strength, float Flicker, float Seed)
{
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DC/Materials/M_DC_Neon.M_DC_Neon")))
	{
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Base, this);
		D->SetVectorParameterValue("Color", Color);
		D->SetScalarParameterValue("Strength", Strength);
		D->SetScalarParameterValue("Flicker", Flicker);
		D->SetScalarParameterValue("Seed", Seed);
		Mats.Add(Key, D);
		return;
	}
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, DCPaths::MatEmissive))
	{
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Base, this);
		D->SetVectorParameterValue("Color", Color);
		D->SetScalarParameterValue("Strength", Strength);
		Mats.Add(Key, D);
		return;
	}
	Proc(Key, 0, Color, Color, 0.5f, 0.f, 1.f, 0.f, 0.f, Color, Strength);
}

void ADCHideoutBuilder::Special(FName Key, const TCHAR* Path, TFunctionRef<void(UMaterialInstanceDynamic*)> Setup)
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, Path);
	if (!Base) { Base = LoadObject<UMaterialInterface>(nullptr, DCPaths::MatGlass); }
	if (!Base) { return; }
	UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Base, this);
	Setup(D);
	Mats.Add(Key, D);
}

// =====================================================================================================================
// Kit
// =====================================================================================================================

static FTransform ShapeXform(const FVector& Loc, const FVector& Size, const FRotator& Rot)
{
	return FTransform(Rot, Loc, Size / 100.f);
}

void ADCHideoutBuilder::Deco(EDCShape Shape, const FVector& Loc, const FVector& Size, FName MatKey, const FRotator& Rot, bool bShadow)
{
	check(CurRoom);
	UStaticMesh* Mesh = Meshes[(int32)Shape];
	UMaterialInterface* Mat = M(MatKey);
	const FString Key = FString::Printf(TEXT("%p|%s|%p|%p|%d"), CurRoom, *CurGroup.ToString(), Mesh, Mat, bShadow ? 1 : 0);
	int32* Idx = BatchIndex.Find(Key);
	if (!Idx)
	{
		FInstBatch B{ CurRoom, CurGroup, Mesh, Mat, bShadow, {} };
		Idx = &BatchIndex.Add(Key, Batches.Add(MoveTemp(B)));
	}
	Batches[*Idx].Xforms.Add(ShapeXform(Loc, Size, Rot));
}

void ADCHideoutBuilder::Tube(const FVector& A, const FVector& B, float Dia, FName Mat, bool bShadow)
{
	const FVector D = B - A;
	const float Len = D.Size();
	if (Len < KINDA_SMALL_NUMBER) { return; }
	const FRotator Rot = FRotationMatrix::MakeFromZ(D / Len).Rotator();
	Deco(EDCShape::Cylinder, (A + B) * 0.5f, FVector(Dia, Dia, Len), Mat, Rot, bShadow);
}

void ADCHideoutBuilder::Bar(const FVector& A, const FVector& B, float W, float H, FName Mat, bool bShadow)
{
	const FVector D = B - A;
	const float Len = D.Size();
	if (Len < KINDA_SMALL_NUMBER) { return; }
	const FRotator Rot = FRotationMatrix::MakeFromX(D / Len).Rotator();
	Deco(EDCShape::Cube, (A + B) * 0.5f, FVector(Len, W, H), Mat, Rot, bShadow);
}

void ADCHideoutBuilder::FlushBatches()
{
	for (FInstBatch& B : Batches)
	{
		if (!B.Room || B.Xforms.Num() == 0) { continue; }
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(B.Room);
		C->SetStaticMesh(B.Mesh);
		if (B.Mat) { C->SetMaterial(0, B.Mat); }
		C->bDisableCollision = true;
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCollisionResponseToAllChannels(ECR_Ignore);
		C->SetCastShadow(B.bShadow);
		C->SetMobility(EComponentMobility::Movable);
		C->SetupAttachment(B.Room->GetRootComponent());
		C->RegisterComponent();
		C->AddInstances(B.Xforms, false, false, false);
		B.Room->AddInstanceComponent(C);
		B.Room->AddToGroup(C, B.Group);
	}
	Batches.Reset();
	BatchIndex.Reset();
}

UStaticMeshComponent* ADCHideoutBuilder::Single(AActor* Owner, USceneComponent* Parent, EDCShape Shape, const FVector& Loc, const FVector& Size, UMaterialInterface* Mat, const FRotator& Rot)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
	C->SetStaticMesh(Meshes[(int32)Shape]);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Parent);
	C->SetRelativeTransform(ShapeXform(Loc, Size, Rot));
	if (Mat) { C->SetMaterial(0, Mat); }
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCollisionResponseToAllChannels(ECR_Ignore);
	C->RegisterComponent();
	Owner->AddInstanceComponent(C);
	return C;
}

UStaticMeshComponent* ADCHideoutBuilder::Solid(const FVector& Loc, const FVector& Size, FName MatKey, bool bFloor)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(CurRoom);
	C->SetStaticMesh(Meshes[(int32)EDCShape::Cube]);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(CurRoom->GetRootComponent());
	C->SetRelativeTransform(ShapeXform(Loc, Size, FRotator::ZeroRotator));
	C->SetMaterial(0, M(MatKey));
	C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	if (!bFloor)
	{
		// Walls must not eat cursor traces aimed at stations behind them.
		C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	C->RegisterComponent();
	CurRoom->AddInstanceComponent(C);
	CurRoom->AddToGroup(C, DCGroups::Shell);
	return C;
}

void ADCHideoutBuilder::ShadowCaster(const FVector& WorldLoc, const FVector& Size)
{
	static const FName CasterName("ShadowCasters");
	UInstancedStaticMeshComponent* ISM = nullptr;
	TArray<UInstancedStaticMeshComponent*> Existing; GetComponents<UInstancedStaticMeshComponent>(Existing);
	for (UInstancedStaticMeshComponent* E : Existing) { if (E->ComponentHasTag(CasterName)) { ISM = E; break; } }
	if (!ISM)
	{
		ISM = NewObject<UInstancedStaticMeshComponent>(this);
		ISM->ComponentTags.Add(CasterName);
		ISM->SetStaticMesh(Meshes[(int32)EDCShape::Cube]);
		ISM->SetMaterial(0, M("Plinth"));
		ISM->SetMobility(EComponentMobility::Movable);
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->bDisableCollision = true;
		ISM->SetCastShadow(true);
		ISM->bCastHiddenShadow = true;
		ISM->SetHiddenInGame(true);
		ISM->bAffectDistanceFieldLighting = false;
		ISM->SetupAttachment(GetRootComponent());
		ISM->RegisterComponent();
		AddInstanceComponent(ISM);
	}
	ISM->AddInstance(FTransform(FRotator::ZeroRotator, WorldLoc, Size / 100.f), true);
}

void ADCHideoutBuilder::WallRun(bool bAlongX, float Fixed, float A, float B, float Z0, float Z1, float Thick, const TArray<FOpening>& InOps, FName Mat, bool bSolid)
{
	TArray<FOpening> Ops = InOps;
	Ops.Sort([](const FOpening& L, const FOpening& R) { return L.Center < R.Center; });
	auto Piece = [&](float S0, float S1, float H0, float H1)
	{
		if (S1 - S0 < 0.5f || H1 - H0 < 0.5f) { return; }
		const float Mid = (S0 + S1) * 0.5f, Len = S1 - S0, ZC = (H0 + H1) * 0.5f, H = H1 - H0;
		const FVector Loc = bAlongX ? FVector(Mid, Fixed, ZC) : FVector(Fixed, Mid, ZC);
		const FVector Size = bAlongX ? FVector(Len, Thick, H) : FVector(Thick, Len, H);
		if (bSolid) { Solid(Loc, Size, Mat, false); }
		else { Deco(EDCShape::Cube, Loc, Size, Mat); }
	};
	float Cursor = A;
	for (const FOpening& O : Ops)
	{
		const float O0 = FMath::Clamp(O.Center - O.Width * 0.5f, A, B), O1 = FMath::Clamp(O.Center + O.Width * 0.5f, A, B);
		if (O1 <= O0) { continue; }
		Piece(Cursor, O0, Z0, Z1);
		Piece(O0, O1, Z0, FMath::Clamp(O.Bottom, Z0, Z1));
		Piece(O0, O1, FMath::Clamp(O.Top, Z0, Z1), Z1);
		Cursor = O1;
	}
	Piece(Cursor, B, Z0, Z1);
}

void ADCHideoutBuilder::FactoryWindow(bool bAlongX, float Fixed, float Inward, const FOpening& O)
{
	auto P = [&](float Along, float Across, float Z) { return bAlongX ? FVector(Along, Fixed + Across, Z) : FVector(Fixed + Across, Along, Z); };
	auto S = [&](float Along, float Across, float H) { return bAlongX ? FVector(Along, Across, H) : FVector(Across, Along, H); };
	const float W = O.Width, H = O.Top - O.Bottom, A0 = O.Center - W * 0.5f, Z0 = O.Bottom;
	const int32 NX = FMath::Max(2, FMath::RoundToInt(W / 48.f)), NZ = FMath::Max(2, FMath::RoundToInt(H / 40.f));
	const float CW = W / NX, CH = H / NZ;
	// glass: one pane per cell, in the wall plane, never casts shadows (sun passes, mullions don't)
	for (int32 ix = 0; ix < NX; ++ix)
	{
		for (int32 iz = 0; iz < NZ; ++iz)
		{
			Deco(EDCShape::Cube, P(A0 + (ix + 0.5f) * CW, Inward * 2.f, Z0 + (iz + 0.5f) * CH), S(CW, 1.f, CH), "Window", FRotator::ZeroRotator, false);
		}
	}
	// mullion grid (casts the pane pattern), heavier outer frame
	for (int32 ix = 0; ix <= NX; ++ix)
	{
		const bool bEdge = ix == 0 || ix == NX;
		Deco(EDCShape::Cube, P(A0 + ix * CW, Inward * 4.f, Z0 + H * 0.5f), S(bEdge ? 7.f : 3.f, bEdge ? 14.f : 7.f, H), "SteelBlack");
	}
	for (int32 iz = 0; iz <= NZ; ++iz)
	{
		const bool bEdge = iz == 0 || iz == NZ;
		Deco(EDCShape::Cube, P(O.Center, Inward * 4.f, Z0 + iz * CH), S(W, bEdge ? 14.f : 7.f, bEdge ? 7.f : 3.f), "SteelBlack");
	}
	// a transom bar + concrete sill on the room side
	Deco(EDCShape::Cube, P(O.Center, Inward * (WallT * 0.5f + 8.f), Z0 - 4.f), S(W + 16.f, 18.f, 8.f), "Concrete");
}

void ADCHideoutBuilder::FinishLight(ULightComponent* L, bool bRoomLight)
{
	L->SetMobility(EComponentMobility::Movable);
	L->SetupAttachment(CurRoom->GetRootComponent());
	if (bRoomLight) { L->ComponentTags.Add(DCTags::RoomLight); }
	L->RegisterComponent();
	CurRoom->AddInstanceComponent(L);
	CurRoom->AddToGroup(L, CurGroup);
}

UPointLightComponent* ADCHideoutBuilder::Point(const FVector& Loc, const FLinearColor& Col, float Cd, float Radius, bool bShadows, bool bRoomLight, float SourceRadius)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(CurRoom);
	L->SetRelativeLocation(Loc);
	L->IntensityUnits = ELightUnits::Candelas;
	L->Intensity = Cd * LightScale;
	L->SetLightColor(Col);
	L->AttenuationRadius = Radius;
	L->SourceRadius = SourceRadius;
	L->SetCastShadows(bShadows);
	L->bUseInverseSquaredFalloff = true;
	L->SetVolumetricScatteringIntensity(0.4f);
	FinishLight(L, bRoomLight);
	return L;
}

USpotLightComponent* ADCHideoutBuilder::Spot(const FVector& Loc, const FVector& Target, const FLinearColor& Col, float Cd, float Radius, float Inner, float Outer, bool bShadows, bool bRoomLight)
{
	USpotLightComponent* L = NewObject<USpotLightComponent>(CurRoom);
	L->SetRelativeLocation(Loc);
	L->SetRelativeRotation((Target - Loc).Rotation());
	L->IntensityUnits = ELightUnits::Candelas;
	L->Intensity = Cd * LightScale;
	L->SetLightColor(Col);
	L->AttenuationRadius = Radius;
	L->InnerConeAngle = Inner;
	L->OuterConeAngle = Outer;
	L->SourceRadius = 2.f;
	L->SetCastShadows(bShadows);
	L->SetVolumetricScatteringIntensity(0.6f);
	FinishLight(L, bRoomLight);
	return L;
}

URectLightComponent* ADCHideoutBuilder::Rect(const FVector& Loc, const FVector& Target, const FLinearColor& Col, float Cd, float W, float H, float Radius, bool bShadows, bool bRoomLight)
{
	URectLightComponent* L = NewObject<URectLightComponent>(CurRoom);
	L->SetRelativeLocation(Loc);
	L->SetRelativeRotation((Target - Loc).Rotation());
	L->IntensityUnits = ELightUnits::Candelas;
	L->Intensity = Cd * LightScale;
	L->SetLightColor(Col);
	L->AttenuationRadius = Radius;
	L->SourceWidth = W;
	L->SourceHeight = H;
	L->BarnDoorAngle = 70.f;
	L->BarnDoorLength = 15.f;
	L->SetCastShadows(bShadows);
	L->SetVolumetricScatteringIntensity(0.4f);
	FinishLight(L, bRoomLight);
	return L;
}

ADCStation* ADCHideoutBuilder::BeginStation(EDCStationType Type, const FString& Name, const FVector& LocalFloorPos, const FVector& HalfExtent, float Yaw)
{
	const FTransform RoomX = CurRoom->GetActorTransform();
	const FVector World = RoomX.TransformPosition(LocalFloorPos + FVector(0, 0, HalfExtent.Z));
	ADCStation* S = GetWorld()->SpawnActor<ADCStation>(ADCStation::StaticClass(), FTransform(FRotator(0, Yaw, 0), World));
	S->StationType = Type;
	S->DisplayName = Name;
	S->OwningRoom = CurRoom->RoomType;
	S->SetClickExtent(HalfExtent);
	S->AttachToActor(CurRoom, FAttachmentTransformRules::KeepWorldTransform);
	CurRoom->RegisterStation(S);
	CurStation = S;
	CurStationHalfZ = HalfExtent.Z;
	return S;
}

UStaticMeshComponent* ADCHideoutBuilder::SP(EDCShape Shape, const FVector& Loc, const FVector& Size, FName MatKey, const FRotator& Rot, bool bShadow)
{
	check(CurStation);
	UStaticMeshComponent* C = CurStation->AddPropMesh(Meshes[(int32)Shape], M(MatKey), ShapeXform(Loc - FVector(0, 0, CurStationHalfZ), Size, Rot));
	// Props can be hovered (QueryOnly) but never block pawns or the camera.
	C->SetCollisionResponseToAllChannels(ECR_Ignore);
	C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	C->SetCastShadow(bShadow);
	CurRoom->AddToGroup(C, CurGroup);
	return C;
}

void ADCHideoutBuilder::SPTube(const FVector& A, const FVector& B, float Dia, FName Mat)
{
	const FVector D = B - A;
	const float Len = D.Size();
	if (Len < KINDA_SMALL_NUMBER) { return; }
	SP(EDCShape::Cylinder, (A + B) * 0.5f, FVector(Dia, Dia, Len), Mat, FRotationMatrix::MakeFromZ(D / Len).Rotator());
}

// ---------------------------------------------------------------------------------------------------------------- dressing

void ADCHideoutBuilder::Sheet(const FVector& Loc, const FVector& Size, float Yaw)
{
	// a tarp over a shape + a loose plastic skirt
	const FRotator R(0, Yaw, 0);
	Deco(EDCShape::Cube, Loc + FVector(0, 0, Size.Z * 0.42f), FVector(Size.X, Size.Y, Size.Z * 0.84f), "Tarp", R);
	Deco(EDCShape::Sphere, Loc + FVector(0, 0, Size.Z * 0.8f), FVector(Size.X * 1.04f, Size.Y * 1.04f, Size.Z * 0.42f), "Tarp", R);
	Deco(EDCShape::Cube, Loc + FVector(0, 0, Size.Z * 0.45f), FVector(Size.X + 10, Size.Y + 10, Size.Z * 0.9f), "Plastic", R, false);
}

void ADCHideoutBuilder::Tape(const FVector& A, const FVector& B)
{
	const FVector D = B - A;
	const float Len = D.Size();
	if (Len < 1.f) { return; }
	const FRotator Rot = FRotationMatrix::MakeFromX(D / Len).Rotator();
	Deco(EDCShape::Cube, (A + B) * 0.5f, FVector(Len, 1.f, 7.f), "Hazard", Rot, false);
}

void ADCHideoutBuilder::Scaffold(const FVector& From, const FVector& To, float Height)
{
	const FVector D = To - From;
	const int32 Bays = FMath::Max(1, FMath::RoundToInt(D.Size2D() / 180.f));
	const FVector Side = FVector(-D.Y, D.X, 0).GetSafeNormal() * 70.f;
	for (int32 i = 0; i <= Bays; ++i)
	{
		const FVector P = From + D * ((float)i / Bays);
		Tube(P, P + FVector(0, 0, Height), 5.f, "Galv");
		Tube(P + Side, P + Side + FVector(0, 0, Height), 5.f, "Galv");
		for (float Z : { 110.f, 220.f, Height - 10.f })
		{
			if (Z > Height) { continue; }
			Tube(P + FVector(0, 0, Z), P + Side + FVector(0, 0, Z), 4.f, "Galv");
		}
		if (i < Bays)
		{
			const FVector Q = From + D * ((float)(i + 1) / Bays);
			Tube(P + FVector(0, 0, 20), Q + FVector(0, 0, Height * 0.9f), 3.5f, "Galv");
			for (float Z : { 110.f, 220.f })
			{
				if (Z > Height) { continue; }
				Tube(P + FVector(0, 0, Z), Q + FVector(0, 0, Z), 4.f, "Galv");
				Tube(P + Side + FVector(0, 0, Z), Q + Side + FVector(0, 0, Z), 4.f, "Galv");
				const FVector Mid = (P + Q) * 0.5f + Side * 0.5f + FVector(0, 0, Z + 4);
				Deco(EDCShape::Cube, Mid, FVector(D.Size2D() / Bays - 8.f, 64.f, 4.f), "Plywood", D.Rotation());
			}
		}
	}
}

void ADCHideoutBuilder::LockedDressing(const FBox2D& R, const TArray<FDCQuad>& Sheets)
{
	CurGroup = DCGroups::Locked;
	for (const FDCQuad& S : Sheets) { Sheet(FVector(S.X, S.Y, 0), FVector(S.Z, S.Z * 0.8f, S.W), S.X * 0.1f); }
	// plastic curtain walls hung from a cable along the two camera-facing sides
	const float Fx = R.Min.X + 40.f, Fy = R.Min.Y + 40.f;
	Deco(EDCShape::Cube, FVector(Fx, R.GetCenter().Y, 150), FVector(1.5f, R.GetSize().Y - 60.f, 300), "Plastic", FRotator::ZeroRotator, false);
	Deco(EDCShape::Cube, FVector(R.GetCenter().X, Fy, 150), FVector(R.GetSize().X - 60.f, 1.5f, 300), "Plastic", FRotator::ZeroRotator, false);
	Tube(FVector(Fx, R.Min.Y + 30, 302), FVector(Fx, R.Max.Y - 30, 302), 1.2f, "SteelBlack");
	Tube(FVector(R.Min.X + 30, Fy, 302), FVector(R.Max.X - 30, Fy, 302), 1.2f, "SteelBlack");
	// barriers + caution tape at knee height
	for (float T : { 0.25f, 0.75f })
	{
		const FVector B1(Fx - 18.f, FMath::Lerp(R.Min.Y, R.Max.Y, T), 0);
		Deco(EDCShape::Cube, B1 + FVector(0, 0, 45), FVector(8, 110, 18), "Hazard");
		Tube(B1 + FVector(-15, -45, 0), B1 + FVector(0, -45, 50), 4, "PaintOrange");
		Tube(B1 + FVector(15, -45, 0), B1 + FVector(0, -45, 50), 4, "PaintOrange");
		Tube(B1 + FVector(-15, 45, 0), B1 + FVector(0, 45, 50), 4, "PaintOrange");
		Tube(B1 + FVector(15, 45, 0), B1 + FVector(0, 45, 50), 4, "PaintOrange");
	}
	Tape(FVector(Fx - 20, R.Min.Y + 60, 95), FVector(Fx - 20, R.Max.Y - 60, 85));
	Tape(FVector(R.Min.X + 60, Fy - 20, 80), FVector(R.Max.X - 60, Fy - 20, 92));
	Tape(FVector(R.Min.X + 80, Fy - 18, 60), FVector(Fx - 18, R.Min.Y + 80, 70));
	// cold rim light from the back corner so the sheets read as silhouettes
	const FVector2D C = R.GetCenter();
	Spot(FVector(R.Max.X - 40, R.Max.Y - 40, TallH + 60), FVector(C.X - 60, C.Y - 60, 0), FLinearColor(0.45f, 0.6f, 1.f), 120.f, 2400.f, 25.f, 60.f, false, false);
}

void ADCHideoutBuilder::BuildingDressing(const FBox2D& R)
{
	CurGroup = DCGroups::Building;
	Scaffold(FVector(R.Max.X - 50, R.Min.Y + 60, 0), FVector(R.Max.X - 50, R.Max.Y - 60, 0), 300.f);
	Scaffold(FVector(R.Min.X + 60, R.Max.Y - 50, 0), FVector(R.Max.X - 120, R.Max.Y - 50, 0), 300.f);
	// LED work lights on tripods
	const FVector2D C = R.GetCenter();
	for (const FVector2D& P : { FVector2D(R.Min.X + 90, R.Min.Y + 90), FVector2D(C.X + 60, C.Y - 30) })
	{
		const FVector B(P.X, P.Y, 0);
		Tube(B + FVector(-25, -25, 0), B + FVector(0, 0, 170), 3, "SteelBlack");
		Tube(B + FVector(25, -25, 0), B + FVector(0, 0, 170), 3, "SteelBlack");
		Tube(B + FVector(0, 30, 0), B + FVector(0, 0, 170), 3, "SteelBlack");
		Deco(EDCShape::Cube, B + FVector(0, 0, 182), FVector(30, 10, 24), "PaintOrange", FRotator(0, 45, 0));
		Deco(EDCShape::Cube, B + FVector(4, 4, 182), FVector(24, 2, 18), "WorkLed", FRotator(0, 45, 0), false);
		Point(B + FVector(25, 25, 185), FLinearColor(0.9f, 0.95f, 1.f), 160.f, 900.f, false, false, 8.f);
	}
	// paint buckets, plywood stack, cement bags
	Deco(EDCShape::Cylinder, FVector(C.X - 120, C.Y + 120, 18), FVector(30, 30, 36), "PlasticWhite");
	Deco(EDCShape::Cylinder, FVector(C.X - 88, C.Y + 150, 18), FVector(30, 30, 36), "PaintOrange");
	for (int32 i = 0; i < 6; ++i) { Deco(EDCShape::Cube, FVector(C.X + 120, C.Y + 140, 2 + i * 3.5f), FVector(244, 122, 3), "Plywood", FRotator(0, 10.f + i * 1.5f, 0)); }
	for (int32 i = 0; i < 4; ++i) { Deco(EDCShape::Cube, FVector(C.X - 140 + (i % 2) * 50.f, C.Y - 120, 8 + (i / 2) * 14.f), FVector(45, 30, 13), "Paper", FRotator(0, i * 7.f, 0)); }
	// step ladder
	const FVector L0(R.Max.X - 140, R.Min.Y + 140, 0);
	Tube(L0, L0 + FVector(30, 0, 200), 5, "Alu");
	Tube(L0 + FVector(0, 45, 0), L0 + FVector(30, 45, 200), 5, "Alu");
	Tube(L0 + FVector(70, 0, 0), L0 + FVector(32, 0, 200), 5, "Alu");
	Tube(L0 + FVector(70, 45, 0), L0 + FVector(32, 45, 200), 5, "Alu");
	for (int32 i = 1; i < 6; ++i) { Deco(EDCShape::Cube, L0 + FVector(30.f * i / 6.f, 22, 200.f * i / 6.f), FVector(10, 45, 2), "Alu"); }
}

void ADCHideoutBuilder::SunDust(const FVector& A, const FVector& B, int32 Count, float Spread)
{
	for (int32 i = 0; i < Count; ++i)
	{
		const float T = FMath::Frac(i * 0.618034f);
		const float J1 = FMath::Frac(FMath::Sin(i * 12.9898f + 4.1f) * 43758.5453f) * 2.f - 1.f;
		const float J2 = FMath::Frac(FMath::Sin(i * 78.233f + 1.7f) * 43758.5453f) * 2.f - 1.f;
		const float J3 = FMath::Frac(FMath::Sin(i * 39.425f + 9.3f) * 43758.5453f) * 2.f - 1.f;
		const FVector P = FMath::Lerp(A, B, T) + FVector(J1, J2, J3) * Spread;
		if (P.Z < 15.f) { continue; }
		Deco(EDCShape::Sphere, P, FVector(0.8f), "Dust", FRotator::ZeroRotator, false);
	}
}

ULocalFogVolumeComponent* ADCHideoutBuilder::ShaftFog(const FVector& Loc, const FVector& Radius, float Density)
{
	ULocalFogVolumeComponent* F = NewObject<ULocalFogVolumeComponent>(CurRoom);
	F->SetupAttachment(CurRoom->GetRootComponent());
	F->SetRelativeLocation(Loc);
	F->SetRelativeScale3D(Radius / ULocalFogVolumeComponent::GetBaseVolumeSize());
	static float FogK = -1.f;
	if (FogK < 0.f) { FogK = 2.5f; FParse::Value(FCommandLine::Get(), TEXT("DCFogK="), FogK); }
	F->RadialFogExtinction = Density * FogK;
	F->HeightFogExtinction = 0.f;
	F->FogAlbedo = FLinearColor(0.9f, 0.9f, 0.88f);
	F->FogPhaseG = 0.6f;
	F->RegisterComponent();
	CurRoom->AddInstanceComponent(F);
	CurRoom->AddToGroup(F, DCGroups::Shell);
	SunFogs.Add(F);
	return F;
}

// =====================================================================================================================
// Layout & shell
// =====================================================================================================================

ADCRoomModule* ADCHideoutBuilder::SpawnRoom(const FRoomDef& Def)
{
	const FVector2D C = Def.Rect.GetCenter();
	const FVector2D S = Def.Rect.GetSize();
	ADCRoomModule* Room = GetWorld()->SpawnActor<ADCRoomModule>(ADCRoomModule::StaticClass(), FTransform(FVector(C.X, C.Y, 0)));
	Room->RoomType = Def.Type;
	Room->LocalBounds = FBox(FVector(-S.X * 0.5f, -S.Y * 0.5f, 0), FVector(S.X * 0.5f, S.Y * 0.5f, TallH));
#if WITH_EDITOR
	Room->SetActorLabel(FString::Printf(TEXT("Room_%s"), *StaticEnum<EDCRoomType>()->GetNameStringByValue((int64)Def.Type)));
#endif
	Rooms.Add(Room);
	RoomPtrs.Add(Room);
	return Room;
}

void ADCHideoutBuilder::BuildShell(const FRoomDef& Def)
{
	CurRoom = GetRoom(Def.Type);
	CurGroup = DCGroups::Shell;
	const FVector2D C = Def.Rect.GetCenter();
	const FVector2D Half = Def.Rect.GetSize() * 0.5f;
	const float X0 = -Half.X, X1 = Half.X, Y0 = -Half.Y, Y1 = Half.Y;

	// floor slab (walkable), dark foundation, a lighter concrete lip so the silhouette reads in the void
	Solid(FVector(0, 0, -15), FVector(Half.X * 2, Half.Y * 2, 30), Def.FloorMat, true);
	Deco(EDCShape::Cube, FVector(0, 0, -80), FVector(Half.X * 2 - 30, Half.Y * 2 - 30, 100), "Plinth");
	Deco(EDCShape::Cube, FVector(X0 - 2, 0, -14), FVector(5, Half.Y * 2 + 6, 28), "Concrete");
	Deco(EDCShape::Cube, FVector(0, Y0 - 2, -14), FVector(Half.X * 2 + 6, 5, 28), "Concrete");
	Deco(EDCShape::Cube, FVector(X0 - 4, 0, -1), FVector(3, Half.Y * 2 + 8, 3), "SteelBlack");
	Deco(EDCShape::Cube, FVector(0, Y0 - 4, -1), FVector(Half.X * 2 + 8, 3, 3), "SteelBlack");

	const TArray<TPair<int32, FOpening>>* Extra = RoomOpenings.Find((uint8)Def.Type);
	auto OpsFor = [&](int32 Side) { TArray<FOpening> O; if (Extra) { for (const auto& P : *Extra) { if (P.Key == Side) { O.Add(P.Value); } } } return O; };

	// sides: 0 +X, 1 +Y, 2 -X, 3 -Y
	for (int32 Side = 0; Side < 4; ++Side)
	{
		const bool bAlongX = (Side == 1 || Side == 3);          // wall runs along X for the Y sides
		const bool bBack = Side < 2;
		const float LineW = (Side == 0) ? Def.Rect.Max.X : (Side == 1) ? Def.Rect.Max.Y : (Side == 2) ? Def.Rect.Min.X : Def.Rect.Min.Y;
		const float RA = bAlongX ? Def.Rect.Min.X : Def.Rect.Min.Y, RB = bAlongX ? Def.Rect.Max.X : Def.Rect.Max.Y;
		const float Base = bAlongX ? C.X : C.Y; // world -> local along the run
		const float Inward = bBack ? -1.f : 1.f;
		const float LocalLine = LineW - (bAlongX ? C.Y : C.X);

		// interior intervals (shared with a neighbour)
		TArray<TPair<float, float>> Shared; TArray<EDCRoomType> SharedWith;
		for (const FRoomDef& N : Defs)
		{
			if (N.Type == Def.Type) { continue; }
			const float NL = (Side == 0) ? N.Rect.Min.X : (Side == 1) ? N.Rect.Min.Y : (Side == 2) ? N.Rect.Max.X : N.Rect.Max.Y;
			if (!FMath::IsNearlyEqual(NL, LineW, 1.f)) { continue; }
			const float NA = bAlongX ? N.Rect.Min.X : N.Rect.Min.Y, NB = bAlongX ? N.Rect.Max.X : N.Rect.Max.Y;
			const float A = FMath::Max(RA, NA), B = FMath::Min(RB, NB);
			if (B - A > 1.f) { Shared.Add({ A, B }); SharedWith.Add(N.Type); }
		}
		Shared.Sort([](const TPair<float, float>& L, const TPair<float, float>& R) { return L.Key < R.Key; });
		TArray<TPair<float, float>> Ext;
		float Cur = RA;
		for (const auto& S : Shared) { if (S.Key > Cur + 1.f) { Ext.Add({ Cur, S.Key }); } Cur = FMath::Max(Cur, S.Value); }
		if (RB > Cur + 1.f) { Ext.Add({ Cur, RB }); }

		const TArray<FOpening> SideOps = OpsFor(Side);
		auto LocalOps = [&](float A, float B) { TArray<FOpening> O; for (const FOpening& Op : SideOps) { if (Op.Center >= A - Base && Op.Center <= B - Base) { O.Add(Op); } } return O; };

		// exterior runs
		for (const auto& E : Ext)
		{
			const float A = E.Key - Base, B = E.Value - Base;
			const float Fix = LocalLine + Inward * WallT * 0.5f;
			TArray<FOpening> Ops = LocalOps(E.Key, E.Value);
			if (bBack)
			{
				// big factory windows in the free stretches between doors
				TArray<FOpening> Windows;
				{
					TArray<FOpening> Sorted = Ops;
					Sorted.Sort([](const FOpening& L, const FOpening& R) { return L.Center < R.Center; });
					float FreeA = A + 50.f;
					auto Fill = [&](float F0, float F1)
					{
						const float L = F1 - F0;
						if (L < 200.f) { return; }
						const int32 N = FMath::Max(1, FMath::FloorToInt(L / 360.f));
						const float Wd = FMath::Min(270.f, L / N - 90.f);
						for (int32 i = 0; i < N; ++i) { Windows.Add({ F0 + L * (i + 0.5f) / N, Wd, WinBottom, WinTop }); }
					};
					for (const FOpening& O : Sorted) { Fill(FreeA, O.Center - O.Width * 0.5f - 40.f); FreeA = O.Center + O.Width * 0.5f + 40.f; }
					Fill(FreeA, B - 50.f);
				}
				TArray<FOpening> All = Ops; All.Append(Windows);
				WallRun(bAlongX, Fix, A, B, 0.f, TallH, WallT, All, "Brick", true);
				// interior skin + exposed steel ledger at the top + concrete coping
				const float Skin = Fix + Inward * (WallT * 0.5f + 1.f);
				WallRun(bAlongX, Skin, A, B, 0.f, TallH - 26.f, 2.f, All, Def.WallMat, false);
				const float Ledge = Fix + Inward * (WallT * 0.5f + 8.f);
				auto LP = [&](float Along, float Across, float Z) { return bAlongX ? FVector(Along, Across, Z) : FVector(Across, Along, Z); };
				auto LS = [&](float Along, float Across, float H) { return bAlongX ? FVector(Along, Across, H) : FVector(Across, Along, H); };
				Deco(EDCShape::Cube, LP((A + B) * 0.5f, Ledge, TallH - 13.f), LS(B - A, 14.f, 26.f), "SteelBlack");
				WallRun(bAlongX, Fix, A - 4, B + 4, TallH, TallH + 6.f, WallT + 8.f, {}, "Concrete", false);
				// steel columns along the wall (every ~4 m), each with a safety-yellow base
				const int32 NCol = FMath::Max(1, FMath::RoundToInt((B - A) / 400.f));
				for (int32 i = 0; i <= NCol; ++i)
				{
					const float At = FMath::Lerp(A + 14.f, B - 14.f, (float)i / NCol);
					bool bBlocked = false;
					for (const FOpening& O : All) { if (FMath::Abs(At - O.Center) < O.Width * 0.5f + 16.f) { bBlocked = true; } }
					if (bBlocked) { continue; }
					Deco(EDCShape::Cube, LP(At, Fix + Inward * (WallT * 0.5f + 12.f), (TallH - 26.f) * 0.5f), LS(22.f, 22.f, TallH - 26.f), "SteelBlack");
					Deco(EDCShape::Cube, LP(At, Fix + Inward * (WallT * 0.5f + 12.f), 30.f), LS(26.f, 26.f, 60.f), "Hazard");
				}
				for (const FOpening& W : Windows) { FactoryWindow(bAlongX, Fix, Inward, W); }
			}
			else
			{
				WallRun(bAlongX, Fix, A, B, 0.f, KneeH, WallT, Ops, "Block", true);
				WallRun(bAlongX, Fix, A - 3, B + 3, KneeH, KneeH + 5.f, WallT + 6.f, {}, "Concrete", false);
			}
		}
		// interior partitions are built by the room on whose +X/+Y side they lie
		if (bBack)
		{
			for (int32 i = 0; i < Shared.Num(); ++i)
			{
				const float A = Shared[i].Key - Base, B = Shared[i].Value - Base;
				const bool bTall = Def.Type == EDCRoomType::Office && SharedWith[i] == EDCRoomType::OpsRoom;
				const float H = bTall ? TallH : PartH;
				TArray<FOpening> Ops = LocalOps(Shared[i].Key, Shared[i].Value);
				if (Ops.Num() == 0 && (B - A) > 300.f) { Ops.Add({ (A + B) * 0.5f, 130.f, 0.f, 230.f }); }
				WallRun(bAlongX, LocalLine, A, B, 0.f, H, WallT, Ops, bTall ? FName("Brick") : FName("Block"), true);
				WallRun(bAlongX, LocalLine, A, B, H, H + 4.f, WallT + 6.f, Ops, bTall ? FName("Concrete") : FName("SteelBlack"), false);
				if (bTall)
				{
					const float Skin = LocalLine - (WallT * 0.5f + 1.f);
					WallRun(bAlongX, Skin, A, B, 0.f, TallH - 26.f, 2.f, Ops, Def.WallMat, false);
					Deco(EDCShape::Cube, bAlongX ? FVector((A + B) * 0.5f, LocalLine - WallT * 0.5f - 8.f, TallH - 13.f) : FVector(LocalLine - WallT * 0.5f - 8.f, (A + B) * 0.5f, TallH - 13.f),
						bAlongX ? FVector(B - A, 14.f, 26.f) : FVector(14.f, B - A, 26.f), "SteelBlack");
				}
			}
		}
	}
}

void ADCHideoutBuilder::BuildRoof()
{
	// Shadow-only roof over every room (not the yard). Rooms without exterior glazing get a skylight with a hidden
	// mullion grid, so the sun still paints a moving pane pattern on their floors.
	const float Z = TallH + 12.f, T = 20.f;
	for (const FRoomDef& D : Defs)
	{
		const FVector2D C = D.Rect.GetCenter(), S = D.Rect.GetSize();
		if (!D.bSkylight)
		{
			ShadowCaster(FVector(C.X, C.Y, Z), FVector(S.X + 4.f, S.Y + 4.f, T));
			continue;
		}
		// skylight: a band towards the back of the room
		const float SX0 = C.X - S.X * 0.12f, SX1 = C.X + S.X * 0.32f, SY0 = C.Y - S.Y * 0.18f, SY1 = C.Y + S.Y * 0.3f;
		const float RX0 = D.Rect.Min.X - 2.f, RX1 = D.Rect.Max.X + 2.f, RY0 = D.Rect.Min.Y - 2.f, RY1 = D.Rect.Max.Y + 2.f;
		ShadowCaster(FVector((RX0 + SX0) * 0.5f, C.Y, Z), FVector(SX0 - RX0, RY1 - RY0, T));
		ShadowCaster(FVector((SX1 + RX1) * 0.5f, C.Y, Z), FVector(RX1 - SX1, RY1 - RY0, T));
		ShadowCaster(FVector((SX0 + SX1) * 0.5f, (RY0 + SY0) * 0.5f, Z), FVector(SX1 - SX0, SY0 - RY0, T));
		ShadowCaster(FVector((SX0 + SX1) * 0.5f, (SY1 + RY1) * 0.5f, Z), FVector(SX1 - SX0, RY1 - SY1, T));
		for (float X = SX0 + 55.f; X < SX1 - 10.f; X += 55.f) { ShadowCaster(FVector(X, (SY0 + SY1) * 0.5f, Z), FVector(5.f, SY1 - SY0, 12.f)); }
		for (float Y = SY0 + 60.f; Y < SY1 - 10.f; Y += 60.f) { ShadowCaster(FVector((SX0 + SX1) * 0.5f, Y, Z), FVector(SX1 - SX0, 5.f, 12.f)); }
	}
}

ADCRoomModule* ADCHideoutBuilder::GetRoom(EDCRoomType Type) const
{
	for (ADCRoomModule* R : RoomPtrs) { if (R && R->RoomType == Type) { return R; } }
	return nullptr;
}

// =====================================================================================================================
// Lighting / post / cvars
// =====================================================================================================================

void ADCHideoutBuilder::SetCVars()
{
	auto Set = [](const TCHAR* Name, const TCHAR* Value)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name)) { V->Set(Value, ECVF_SetByCode); }
	};
	// Manual exposure lives around EV ~ -3..-1.5. Centre Lumen's cached-lighting range on it so nothing clips.
	Set(TEXT("r.EyeAdaptation.CachedLightingPreExposure"), TEXT("4"));
	Set(TEXT("r.LocalFogVolume.RenderIntoVolumetricFog"), TEXT("1"));
	Set(TEXT("r.LocalFogVolume.MaxDensityIntoVolumetricFog"), TEXT("0.5"));
	Set(TEXT("r.VolumetricFog.GridPixelSize"), TEXT("16"));
	Set(TEXT("r.VolumetricFog.GridSizeZ"), TEXT("64"));
	Set(TEXT("r.VolumetricFog.HistoryWeight"), TEXT("0.9"));
	Set(TEXT("r.MotionBlurQuality"), TEXT("0"));
	FString Extra;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCCVars="), Extra, false))
	{
		// -DCCVars=name=value;name=value  (perf experiments)
		TArray<FString> Pairs; Extra.ParseIntoArray(Pairs, TEXT(";"));
		for (const FString& P : Pairs) { FString K, V; if (P.Split(TEXT("="), &K, &V)) { Set(*K, *V); } }
	}
}

void ADCHideoutBuilder::BuildLightingAndPost()
{
	SetCVars();

	APostProcessVolume* PPV = GetWorld()->SpawnActor<APostProcessVolume>();
	PostVolume = PPV;
	PPV->bUnbound = true;
	PPV->Priority = -1.f; // gameplay (camera / reveal) volumes can override
	FPostProcessSettings& S = PPV->Settings;
	S.bOverride_AutoExposureMethod = true; S.AutoExposureMethod = AEM_Manual;
	S.bOverride_AutoExposureBias = true; S.AutoExposureBias = -2.f; // driven by ADCDayNight
	S.bOverride_AutoExposureApplyPhysicalCameraExposure = true; S.AutoExposureApplyPhysicalCameraExposure = false;
	S.bOverride_LocalExposureMethod = true; S.LocalExposureMethod = ELocalExposureMethod::Bilateral;
	S.bOverride_LocalExposureHighlightContrastScale = true; S.LocalExposureHighlightContrastScale = 0.8f;
	S.bOverride_LocalExposureShadowContrastScale = true; S.LocalExposureShadowContrastScale = 0.95f;
	S.bOverride_BloomMethod = true; S.BloomMethod = BM_SOG;
	S.bOverride_BloomIntensity = true; S.BloomIntensity = 0.4f;
	S.bOverride_BloomThreshold = true; S.BloomThreshold = -1.f;
	S.bOverride_FilmGrainIntensity = true; S.FilmGrainIntensity = 0.f;
	S.bOverride_VignetteIntensity = true; S.VignetteIntensity = 0.45f;
	S.bOverride_SceneFringeIntensity = true; S.SceneFringeIntensity = 0.f;
	S.bOverride_MotionBlurAmount = true; S.MotionBlurAmount = 0.f;
	S.bOverride_LensFlareIntensity = true; S.LensFlareIntensity = 0.f;
	// contemporary, slightly desaturated grade; cool shadows, neutral highlights
	S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(1.f, 1.f, 1.f, 0.86f);
	S.bOverride_ColorContrast = true; S.ColorContrast = FVector4(1.f, 1.f, 1.f, 1.08f);
	S.bOverride_ColorGainShadows = true; S.ColorGainShadows = FVector4(0.94f, 1.0f, 1.06f, 1.f);
	S.bOverride_ColorSaturationHighlights = true; S.ColorSaturationHighlights = FVector4(1.f, 1.f, 1.f, 0.9f);
	S.bOverride_ToneCurveAmount = true; S.ToneCurveAmount = 1.f;
	S.bOverride_LumenSceneLightingQuality = true; S.LumenSceneLightingQuality = 1.f;
	S.bOverride_LumenFinalGatherQuality = true; S.LumenFinalGatherQuality = 1.f;
	S.bOverride_LumenSkylightLeaking = true; S.LumenSkylightLeaking = 0.05f;
	S.bOverride_IndirectLightingIntensity = true; S.IndirectLightingIntensity = 1.2f;

	// Height fog with ~zero density: only here because Volumetric Fog (sun shafts in local fog volumes) hangs off it.
	AExponentialHeightFog* Fog = GetWorld()->SpawnActor<AExponentialHeightFog>();
	UExponentialHeightFogComponent* F = Fog ? Fog->GetComponent() : nullptr;
	if (F)
	{
		F->SetFogDensity(0.0002f);
		F->SetSecondFogDensity(0.f);
		F->SetFogHeightFalloff(0.001f);
		F->SetFogInscatteringColor(FLinearColor::Black);
		F->SetDirectionalInscatteringColor(FLinearColor::Black);
		F->SetVolumetricFog(true);
		F->SetVolumetricFogScatteringDistribution(0.6f);
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
		if (UMaterialInstanceDynamic* W = Cast<UMaterialInstanceDynamic>(M("Window"))) { DayNight->WindowMats.Add(W); }
		DayNight->DustMat = Cast<UMaterialInstanceDynamic>(M("Dust"));
		DayNight->LampMat = Cast<UMaterialInstanceDynamic>(M("LampHead"));
		for (ULocalFogVolumeComponent* LF : SunFogs) { DayNight->AddShaftFog(LF); }
		for (const FStreetSpec& SS : StreetSpecs) { DayNight->AddStreetLight(SS.Pos, SS.Target, SS.Col, SS.Cd, SS.Radius, SS.Cone, SS.bShadow); }
	}
}

void ADCHideoutBuilder::AddFlicker(ULightComponent* L, float Rate, float Depth)
{
	if (L) { Flickers.Add({ L, L->Intensity, Rate, Depth, (float)Flickers.Num() * 7.31f }); }
}

// =====================================================================================================================
// Build
// =====================================================================================================================

void ADCHideoutBuilder::BuildHideout()
{
	if (bBuilt) { return; }
	bBuilt = true;
	const double T0 = FPlatformTime::Seconds();
	FParse::Value(FCommandLine::Get(), TEXT("DCLightScale="), LightScale);
	InitAssets();

	auto R = [](float X0, float X1, float Y0, float Y1) { return FBox2D(FVector2D(X0, Y0), FVector2D(X1, Y1)); };
	Defs = {
		{ EDCRoomType::Office,       R(-500,  500, -400,  400), "Slab",        "Brick",      false },
		{ EDCRoomType::Workshop,     R(-500,  500, -1400, -400), "SlabDark",   "BrickGrey",  true },
		{ EDCRoomType::Garage,       R( 500, 1300, -1400, -400), "SlabDark",   "CorrugatedV", false },
		{ EDCRoomType::OpsRoom,      R(-500, 1300,  400, 1600), "Slab",        "Brick",      false },
		{ EDCRoomType::Lounge,       R(-1300, -500, -1400, -400), "Slab",      "BrickPaint", true },
		{ EDCRoomType::ServerRoom,   R(-1300, -500, -400,  400), "RaisedFloor", "BlockDark", true },
		{ EDCRoomType::LawyerOffice, R(-1300, -500,  400, 1000), "CarpetTile", "Block",      true },
		{ EDCRoomType::Clinic,       R(-1300, -500, 1000, 1600), "Epoxy",      "BlockGreen", false },
	};
	// Openings in room-local coordinates along the side. side: 0 +X, 1 +Y, 2 -X, 3 -Y
	auto Op = [this](EDCRoomType T, int32 Side, float C, float W, float B, float Tp) { RoomOpenings.FindOrAdd((uint8)T).Add(TPair<int32, FOpening>(Side, FOpening{ C, W, B, Tp })); };
	Op(EDCRoomType::Office, 0, -250.f, 120.f, 0.f, 225.f);    // street door to the yard
	Op(EDCRoomType::Office, 1, -430.f, 90.f, 0.f, 215.f);     // door through to the ops room
	Op(EDCRoomType::Garage, 0, 0.f, 360.f, 0.f, 290.f);       // roller door

	for (const FRoomDef& D : Defs) { SpawnRoom(D); }
	for (const FRoomDef& D : Defs) { BuildShell(D); }

	BuildOffice();
	BuildLounge();
	BuildWorkshop();
	BuildGarage();
	BuildOpsRoom();
	BuildClinic();
	BuildServerRoom();
	BuildLawyer();
	BuildYard();
	FlushBatches();
	BuildRoof();
	BuildLightingAndPost();

	const bool bAll = UDCDevTools::IsBuildAll();
	for (ADCRoomModule* Room : RoomPtrs)
	{
		if (bAll) { Room->ApplyState(EDCRoomState::Built, 3); continue; }
		const bool bOffice = Room->RoomType == EDCRoomType::Office;
		Room->ApplyState(bOffice ? EDCRoomState::Built : EDCRoomState::Locked, bOffice ? 1 : 0);
	}

	// scene review camera
	FString ShotsStr;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCSceneShots="), ShotsStr, false))
	{
		ShotsStr.ParseIntoArray(ReviewShots, TEXT(","));
		ReviewDir = FPaths::ProjectSavedDir() / TEXT("DCShots");
		FParse::Value(FCommandLine::Get(), TEXT("DCSceneShotDir="), ReviewDir);
		float Delay = 8.f;
		FParse::Value(FCommandLine::Get(), TEXT("DCSceneShotDelay="), Delay);
		ReviewNext = Delay;
		IFileManager::Get().MakeDirectory(*ReviewDir, true);
		ReviewCam = GetWorld()->SpawnActor<ACameraActor>();
		ReviewCam->GetCameraComponent()->SetFieldOfView(18.f);
		ReviewCam->GetCameraComponent()->SetConstraintAspectRatio(false);
	}
	int32 NumComps = 0;
	for (ADCRoomModule* Room : RoomPtrs) { NumComps += Room->GetComponents().Num(); }
	UE_LOG(LogDC, Display, TEXT("[Scene] BuildHideout: %d rooms, %d components, %.1f ms"), RoomPtrs.Num(), NumComps, (FPlatformTime::Seconds() - T0) * 1000.0);
}

// =====================================================================================================================
// Runtime
// =====================================================================================================================

void ADCHideoutBuilder::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	for (const FFlicker& F : Flickers)
	{
		ULightComponent* L = F.Light.Get();
		if (!L || !L->IsVisible()) { continue; }
		const float T = (float)Time * F.Rate + F.Seed;
		const float Hash = FMath::Frac(FMath::Sin(FMath::FloorToFloat(T) * 12.9898f + F.Seed) * 43758.5453f);
		const float Burst = FMath::Frac(FMath::Sin(FMath::FloorToFloat(T * 0.05f) * 3.17f + F.Seed) * 9631.1f) > 0.8f ? 1.f : 0.f;
		const float K = 1.f - F.Depth * (Burst * (Hash > 0.5f ? 0.85f : 0.f));
		L->SetIntensity(F.Base * K);
	}
	PerfAccum += DeltaSeconds; ++PerfFrames;
	if (PerfAccum > 2.0) { PerfAccum = DeltaSeconds; PerfFrames = 1; }
	if (ReviewShots.Num()) { TickReview(); }
}

void ADCHideoutBuilder::FrameReview(const FString& Focus)
{
	FBox B(ForceInit);
	if (Focus.Equals(TEXT("wide"), ESearchCase::IgnoreCase))
	{
		for (ADCRoomModule* R : RoomPtrs) { B += R->GetWorldBounds(); }
	}
	else
	{
		const int64 V = StaticEnum<EDCRoomType>()->GetValueByNameString(Focus);
		for (ADCRoomModule* R : RoomPtrs) { if ((int64)R->RoomType == V) { B += R->GetWorldBounds(); } }
		if (!B.IsValid) { for (ADCRoomModule* R : RoomPtrs) { B += R->GetWorldBounds(); } }
	}
	const FRotator Rot(-35.f, 45.f, 0.f);
	const float Radius = B.GetExtent().Size2D();
	const float Dist = Radius * 1.12f / FMath::Tan(FMath::DegreesToRadians(9.f));
	const FVector Target(B.GetCenter().X, B.GetCenter().Y, -60.f);
	ReviewCam->SetActorLocationAndRotation(Target - Rot.Vector() * Dist, Rot);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) { PC->SetViewTarget(ReviewCam); }
}

void ADCHideoutBuilder::TickReview()
{
	if (Time < ReviewNext) { return; }
	if (ReviewShots.IsValidIndex(ReviewStep))
	{
		const FString Shot = ReviewShots[ReviewStep];
		if (!bReviewPrepared)
		{
			FrameReview(Shot);
			bReviewPrepared = true;
			ReviewNext = Time + 3.0;
			return;
		}
		const FString File = ReviewDir / (TEXT("scene_") + Shot + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(File, false, false);
		UE_LOG(LogDC, Display, TEXT("[SceneShots] avg frame %.2f ms (%.0f fps) hour %.2f"), PerfFrames ? PerfAccum / PerfFrames * 1000.0 : 0.0, PerfFrames ? PerfFrames / PerfAccum : 0.0, DayNight ? DayNight->GetDisplayHour() : -1.f);
		UE_LOG(LogDC, Display, TEXT("[SceneShots] %s"), *File);
		bReviewPrepared = false;
		++ReviewStep;
		ReviewNext = Time + 1.0;
		return;
	}
	ReviewShots.Reset();
	if (GEngine) { GEngine->Exec(GetWorld(), TEXT("quit")); }
}
