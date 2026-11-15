// Director's Cut — hideout builder: materials, kit plumbing, room shells, shared walls, review camera, runtime FX.
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
#include "Core/DCSiteLayout.h"
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
#include "Core/DCStyle.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
	constexpr float TallH = 380.f;   // room bounds height
	constexpr float HouseH = 290.f;  // farmhouse eaves (tall back walls)
	constexpr float WingH = 255.f;   // annex back walls
	constexpr float WallT = 24.f;
	constexpr float DoorW = 140.f;   // doorways in shared walls (nav traces them)
	inline float SJ(int32 I, float Amp) { return (FMath::Frac(FMath::Sin(I * 12.9898f + 4.1f) * 43758.5453f) * 2.f - 1.f) * Amp; }
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
	Proc("Tarp",       12, C(0.055f, 0.06f, 0.065f), C(0.035f, 0.038f, 0.042f), 0.6f, 0.f, 1.f, 0.4f);
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
	Proc("CarWhite",    7, C(0.30f, 0.31f, 0.31f), C(0.2f, 0.2f, 0.2f), 0.35f, 0.2f, 0.3f, 0.6f);
	Proc("Tire",        0, C(0.015f, 0.015f, 0.015f), C(0.025f, 0.025f, 0.025f), 0.9f);
	// --- rural ruin: masonry, plaster, timber, roofing, growth
	Proc("Stone",       2, C(0.30f, 0.27f, 0.22f), C(0.40f, 0.38f, 0.34f), 0.92f, 0.f, 0.5f, 0.85f);
	Proc("StoneDark",   2, C(0.17f, 0.16f, 0.14f), C(0.24f, 0.23f, 0.21f), 0.92f, 0.f, 0.7f, 0.9f);
	Proc("BrickOld",    2, C(0.32f, 0.13f, 0.075f), C(0.36f, 0.33f, 0.29f), 0.9f, 0.f, 1.f, 0.85f);
	Proc("Plaster",    11, C(0.55f, 0.48f, 0.37f), C(0.30f, 0.26f, 0.20f), 0.93f, 0.f, 1.f, 0.9f);
	Proc("PlasterGreen",11, C(0.27f, 0.36f, 0.30f), C(0.16f, 0.18f, 0.14f), 0.9f, 0.f, 1.f, 0.9f);
	Proc("PlasterRose",11, C(0.50f, 0.33f, 0.28f), C(0.26f, 0.18f, 0.15f), 0.9f, 0.f, 1.f, 0.9f);
	Proc("PlasterWhite",11, C(0.60f, 0.60f, 0.56f), C(0.24f, 0.31f, 0.29f), 0.85f, 0.f, 1.f, 0.8f);
	Proc("Timber",      1, C(0.24f, 0.18f, 0.12f), C(0.12f, 0.09f, 0.06f), 0.85f, 0.f, 1.f, 0.6f);
	Proc("TimberDark",  1, C(0.11f, 0.075f, 0.045f), C(0.05f, 0.035f, 0.02f), 0.8f, 0.f, 1.f, 0.5f);
	Proc("TimberGrey",  1, C(0.27f, 0.26f, 0.24f), C(0.14f, 0.13f, 0.12f), 0.9f, 0.f, 1.f, 0.7f);
	Proc("Board",       1, C(0.22f, 0.17f, 0.12f), C(0.10f, 0.08f, 0.06f), 0.88f, 0.f, 1.f, 0.7f, 1.f);
	Proc("FreshTimber", 1, C(0.58f, 0.44f, 0.27f), C(0.42f, 0.30f, 0.16f), 0.7f, 0.f, 1.f, 0.1f);
	Proc("Floorboard",  1, C(0.21f, 0.14f, 0.085f), C(0.09f, 0.06f, 0.035f), 0.75f, 0.f, 1.f, 0.8f);
	Proc("Lath",        1, C(0.36f, 0.29f, 0.19f), C(0.20f, 0.15f, 0.09f), 0.9f);
	Proc("RoofTile",   10, C(0.40f, 0.15f, 0.08f), C(0.12f, 0.11f, 0.07f), 0.85f, 0.f, 1.3f, 0.6f, 0.f);
	Proc("TinRust",    10, C(0.10f, 0.105f, 0.10f), C(0.20f, 0.075f, 0.025f), 0.7f, 0.4f, 1.f, 0.7f, 0.f);
	Proc("TinRustV",   10, C(0.10f, 0.11f, 0.105f), C(0.20f, 0.075f, 0.025f), 0.7f, 0.4f, 1.f, 0.7f, 1.f);
	Proc("Rust",        7, C(0.26f, 0.10f, 0.04f), C(0.12f, 0.05f, 0.02f), 0.8f, 0.3f);
	Proc("Moss",        0, C(0.07f, 0.11f, 0.03f), C(0.03f, 0.05f, 0.015f), 0.95f, 0.f, 2.f, 0.5f);
	Proc("Ivy",         5, C(0.03f, 0.075f, 0.022f), C(0.06f, 0.12f, 0.035f), 0.8f, 0.f, 3.f);
	Proc("Weed",        5, C(0.09f, 0.16f, 0.04f), C(0.05f, 0.10f, 0.02f), 0.9f, 0.f, 2.f);
	Proc("WeedDry",     5, C(0.30f, 0.26f, 0.11f), C(0.16f, 0.13f, 0.05f), 0.95f, 0.f, 2.f);
	Proc("Dirt",        3, C(0.13f, 0.10f, 0.065f), C(0.07f, 0.055f, 0.04f), 0.95f, 0.f, 1.f, 0.5f);
	Proc("Rubble",      3, C(0.30f, 0.28f, 0.24f), C(0.15f, 0.14f, 0.12f), 0.95f, 0.f, 2.f, 0.8f);
	Proc("Cement",      3, C(0.38f, 0.37f, 0.35f), C(0.25f, 0.245f, 0.23f), 0.9f, 0.f, 1.f, 0.3f);
	Proc("TileOld",     8, C(0.42f, 0.40f, 0.36f), C(0.12f, 0.20f, 0.18f), 0.5f, 0.f, 1.f, 0.6f);
	Proc("Rope",        0, C(0.62f, 0.56f, 0.40f), C(0.5f, 0.45f, 0.3f), 0.9f);
	Proc("Canvas",     12, C(0.22f, 0.20f, 0.13f), C(0.14f, 0.12f, 0.08f), 0.95f, 0.f, 1.f, 0.3f);
	Proc("Sand",        3, C(0.46f, 0.39f, 0.26f), C(0.34f, 0.28f, 0.18f), 0.95f);
	Proc("PaintGreen",  7, C(0.05f, 0.13f, 0.07f), C(0.03f, 0.05f, 0.03f), 0.5f, 0.f);
	Proc("PaintYellow", 7, C(0.55f, 0.38f, 0.03f), C(0.25f, 0.15f, 0.02f), 0.5f, 0.f);
	// --- furnishing (DCHideoutFurnish.cpp)
	Proc("Rug",        12, C(0.20f, 0.045f, 0.035f), C(0.09f, 0.025f, 0.02f), 0.95f, 0.f, 2.f, 0.25f);
	Proc("RugBorder",  12, C(0.26f, 0.19f, 0.09f), C(0.12f, 0.08f, 0.04f), 0.95f, 0.f, 2.f, 0.25f);
	Proc("RugBlue",    12, C(0.035f, 0.06f, 0.10f), C(0.02f, 0.03f, 0.055f), 0.95f, 0.f, 2.f, 0.25f);
	Proc("Velvet",      5, C(0.05f, 0.095f, 0.07f), C(0.025f, 0.05f, 0.035f), 0.9f, 0.f, 1.f, 0.2f);
	Proc("Brass",       7, C(0.50f, 0.34f, 0.11f), C(0.30f, 0.19f, 0.06f), 0.35f, 1.f, 1.f, 0.2f);
	Proc("Enamel",      7, C(0.52f, 0.49f, 0.42f), C(0.38f, 0.35f, 0.29f), 0.35f, 0.f, 1.f, 0.4f);
	Proc("Terracotta",  3, C(0.34f, 0.13f, 0.06f), C(0.22f, 0.08f, 0.04f), 0.9f, 0.f, 1.f, 0.4f);
	Proc("CastIron",    7, C(0.03f, 0.03f, 0.032f), C(0.07f, 0.06f, 0.055f), 0.6f, 0.6f, 1.f, 0.5f);
	Proc("BookRed",     0, C(0.20f, 0.03f, 0.025f), C(0.15f, 0.02f, 0.02f), 0.7f);
	Proc("BookBlue",    0, C(0.03f, 0.05f, 0.12f), C(0.02f, 0.04f, 0.09f), 0.7f);
	Proc("BookGreen",   0, C(0.04f, 0.10f, 0.05f), C(0.03f, 0.07f, 0.04f), 0.7f);
	Proc("BookTan",     0, C(0.36f, 0.26f, 0.15f), C(0.28f, 0.2f, 0.11f), 0.8f);
	Proc("LampShade",  12, C(0.62f, 0.50f, 0.32f), C(0.5f, 0.4f, 0.24f), 0.9f, 0.f, 1.f, 0.1f, 0.f, C(1.f, 0.6f, 0.3f), 0.6f);
	// --- emissive (M_DC_Neon, no flicker unless asked)
	Emis("Tungsten",    FLinearColor(1.f, 0.55f, 0.22f), 45.f);
	Emis("StringBulb",  FLinearColor(1.f, 0.52f, 0.14f), 28.f);
	Neon("Candle",      FLinearColor(1.f, 0.45f, 0.1f), 30.f, 0.6f, 3.f);
	Emis("LedMagenta",  FLinearColor(1.f, 0.08f, 0.75f), 22.f);
	Emis("Teal",        FLinearColor(0.08f, 0.9f, 1.f), 14.f);
	Emis("CrossGreen",  FLinearColor(0.1f, 1.f, 0.4f), 10.f);
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
	Special("Plastic", TEXT("/Game/DC/Materials/M_DC_GlassI.M_DC_GlassI"), [](UMaterialInstanceDynamic* D) { D->SetVectorParameterValue("Color", FLinearColor(0.16f, 0.18f, 0.19f)); D->SetScalarParameterValue("Opacity", 0.32f); D->SetScalarParameterValue("Roughness", 0.3f); });
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
		D->SetVectorParameterValue("ColorA", DCStyle::Tone(A));
		D->SetVectorParameterValue("ColorB", DCStyle::Tone(B));
		D->SetScalarParameterValue("Pattern", (float)Pattern);
		D->SetScalarParameterValue("Scale", Scale);
		D->SetScalarParameterValue("Dirt", DCStyle::Dirt(Dirt));
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

FVector ADCHideoutBuilder::MapP(const FVector& P) const
{
	if (!bFit) { return P; }
	auto Plain = [this](const FVector& V) { return FVector(V.X * FitXY, V.Y * FitXY, V.Z * FitZ); };
	if (bKitOnStation && CurStation) { return Plain(StationAnchor) + (P - StationAnchor) * FitS; }
	if (bAnchored) { return Plain(FitAnchor) + (P - FitAnchor) * FitS; }
	return Plain(P);
}

void ADCHideoutBuilder::Deco(EDCShape Shape, const FVector& InLoc, const FVector& InSize, FName MatKey, const FRotator& Rot, bool bShadow)
{
	check(CurRoom);
	const FVector Loc = MapP(InLoc), Size = MapSize(InSize);
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

void ADCHideoutBuilder::Tube(const FVector& InA, const FVector& InB, float InDia, FName Mat, bool bShadow)
{
	const FVector A = MapP(InA), B = MapP(InB);
	const float Dia = MapLen(InDia);
	FNoFit NF(this);
	const FVector D = B - A;
	const float Len = D.Size();
	if (Len < KINDA_SMALL_NUMBER) { return; }
	const FRotator Rot = FRotationMatrix::MakeFromZ(D / Len).Rotator();
	Deco(EDCShape::Cylinder, (A + B) * 0.5f, FVector(Dia, Dia, Len), Mat, Rot, bShadow);
}

void ADCHideoutBuilder::Bar(const FVector& InA, const FVector& InB, float InW, float InH, FName Mat, bool bShadow)
{
	const FVector A = MapP(InA), B = MapP(InB);
	const float W = MapLen(InW), H = MapLen(InH);
	FNoFit NF(this);
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
		if (TrackedGroups.Contains(B.Group)) { TrackComponent(C, B.Group); }
		else { B.Room->AddToGroup(C, B.Group); }
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
	if (TrackedGroups.Contains(CurGroup)) { TrackComponent(C, CurGroup); }
	else { CurRoom->AddToGroup(C, CurGroup); }
	return C;
}

void ADCHideoutBuilder::TrackComponent(UPrimitiveComponent* C, FName Group)
{
	const FIntPoint* W = TrackedGroups.Find(Group);
	if (!C || !W || !SharedWalls.IsValidIndex(W->X)) { return; }
	FSharedWall& SW = SharedWalls[W->X];
	SW.Comps[W->Y].Add(C);
	SW.Coll[W->Y].Add(C->GetCollisionEnabled());
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
	L->SetRelativeLocation(MapP(Loc));
	Radius = MapLen(Radius);
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
	L->SetRelativeLocation(MapP(Loc));
	L->SetRelativeRotation((MapP(Target) - MapP(Loc)).Rotation());
	Radius = MapLen(Radius);
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
	L->SetRelativeLocation(MapP(Loc));
	L->SetRelativeRotation((MapP(Target) - MapP(Loc)).Rotation());
	Radius = MapLen(Radius); W = MapLen(W); H = MapLen(H);
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
	StationAnchor = LocalFloorPos;
	const FVector World = RoomX.TransformPosition(MapP(LocalFloorPos) + FVector(0, 0, MapLen(HalfExtent.Z)));
	ADCStation* S = GetWorld()->SpawnActor<ADCStation>(ADCStation::StaticClass(), FTransform(FRotator(0, Yaw, 0), World));
	S->StationType = Type;
	S->DisplayName = Name;
	S->OwningRoom = CurRoom->RoomType;
	S->SetClickExtent(MapSize(HalfExtent));
	S->AttachToActor(CurRoom, FAttachmentTransformRules::KeepWorldTransform);
	CurRoom->RegisterStation(S);
	CurStation = S;
	CurStationHalfZ = MapLen(HalfExtent.Z);
	return S;
}

UStaticMeshComponent* ADCHideoutBuilder::SP(EDCShape Shape, const FVector& InLoc, const FVector& InSize, FName MatKey, const FRotator& Rot, bool bShadow)
{
	const FVector Loc = bFit ? InLoc * FitS : InLoc, Size = MapSize(InSize);
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
	const bool bOffice = Def.Type == EDCRoomType::Office;
	const FName Main = bOffice ? DCGroups::Shell : DCGroups::Level1;
	CurGroup = Main;
	const FVector2D C = Def.Rect.GetCenter();
	const FVector2D Half = Def.Rect.GetSize() * 0.5f;
	const int32 RSeed = (int32)Def.Type * 101 + 7;

	// floor (walkable, top at z=0) + a stone foundation course along the open fronts
	Solid(FVector(0, 0, -10), FVector(Half.X * 2, Half.Y * 2, 20), Def.FloorMat, true);
	Deco(EDCShape::Cube, FVector(-Half.X + 8, 0, -4.2f), FVector(18, Half.Y * 2, 9), "StoneDark");
	Deco(EDCShape::Cube, FVector(0, -Half.Y + 8, -4.2f), FVector(Half.X * 2, 18, 9), "StoneDark");

	const TArray<TPair<int32, FOpening>>* Extra = RoomOpenings.Find((uint8)Def.Type);
	auto OpsFor = [&](int32 Side) { TArray<FOpening> O; if (Extra) { for (const auto& P : *Extra) { if (P.Key == Side) { O.Add(P.Value); } } } return O; };
	const TArray<TPair<int32, FVector2D>>* Keep = KeepSolid.Find((uint8)Def.Type);

	for (int32 Side = 0; Side < 4; ++Side)
	{
		const bool bAlongX = (Side == 1 || Side == 3);
		const bool bBack = Side < 2;
		const float LineW = (Side == 0) ? Def.Rect.Max.X : (Side == 1) ? Def.Rect.Max.Y : (Side == 2) ? Def.Rect.Min.X : Def.Rect.Min.Y;
		const float RA = bAlongX ? Def.Rect.Min.X : Def.Rect.Min.Y, RB = bAlongX ? Def.Rect.Max.X : Def.Rect.Max.Y;
		const float Base = bAlongX ? C.X : C.Y;
		const float LocalLine = LineW - (bAlongX ? C.Y : C.X);

		struct FShared { float A, B; const FRoomDef* N; };
		TArray<FShared> Shared;
		for (const FRoomDef& N : Defs)
		{
			if (N.Type == Def.Type) { continue; }
			const float NL = (Side == 0) ? N.Rect.Min.X : (Side == 1) ? N.Rect.Min.Y : (Side == 2) ? N.Rect.Max.X : N.Rect.Max.Y;
			if (!FMath::IsNearlyEqual(NL, LineW, 1.f)) { continue; }
			const float NA = bAlongX ? N.Rect.Min.X : N.Rect.Min.Y, NB = bAlongX ? N.Rect.Max.X : N.Rect.Max.Y;
			const float A = FMath::Max(RA, NA), B = FMath::Min(RB, NB);
			if (B - A > 1.f) { Shared.Add({ A, B, &N }); }
		}
		Shared.Sort([](const FShared& L, const FShared& R) { return L.A < R.A; });
		TArray<TPair<float, float>> Ext;
		float Cur = RA;
		for (const FShared& S : Shared) { if (S.A > Cur + 1.f) { Ext.Add({ Cur, S.A }); } Cur = FMath::Max(Cur, S.B); }
		if (RB > Cur + 1.f) { Ext.Add({ Cur, RB }); }

		const TArray<FOpening> SideOps = OpsFor(Side);
		auto LocalOps = [&](float A, float B) { TArray<FOpening> O; for (const FOpening& Op : SideOps) { if (Op.Center >= A && Op.Center <= B) { O.Add(Op); } } return O; };
		auto WindowsFor = [&](float A, float B, const TArray<FOpening>& Doors, float Bottom, float Top)
		{
			TArray<TPair<float, float>> Blocked;
			for (const FOpening& O : Doors) { Blocked.Add({ O.Center - O.Width * 0.5f - 45.f, O.Center + O.Width * 0.5f + 45.f }); }
			if (Keep) { for (const auto& K : *Keep) { if (K.Key == Side) { Blocked.Add({ K.Value.X, K.Value.Y }); } } }
			Blocked.Sort([](const TPair<float, float>& L, const TPair<float, float>& R) { return L.Key < R.Key; });
			TArray<FOpening> W;
			auto Fill = [&](float F0, float F1)
			{
				const float L = F1 - F0;
				if (L < 190.f) { return; }
				const int32 N = FMath::Max(1, FMath::FloorToInt(L / 380.f));
				const float Wd = FMath::Min(130.f, L / N - 70.f);
				for (int32 i = 0; i < N; ++i) { W.Add({ F0 + L * (i + 0.5f) / N, Wd, Bottom, Top }); }
			};
			float FreeA = A + 60.f;
			for (const auto& Bk : Blocked) { if (Bk.Value < FreeA) { continue; } Fill(FreeA, FMath::Min(Bk.Key, B - 60.f)); FreeA = FMath::Max(FreeA, Bk.Value); }
			Fill(FreeA, B - 60.f);
			return W;
		};

		// ---- exterior runs
		for (int32 e = 0; e < Ext.Num(); ++e)
		{
			const float A = Ext[e].Key - Base, B = Ext[e].Value - Base;
			const int32 Seed = RSeed + Side * 13 + e;
			const TArray<FOpening> Ops = LocalOps(A, B);
			if (bBack)
			{
				const float H = bOffice ? HouseH : WingH;
				const TArray<FOpening> Win = WindowsFor(A, B, Ops, bOffice ? 100.f : 92.f, bOffice ? 225.f : 200.f);
				TallWall(bAlongX, LocalLine - WallT * 0.5f, -1.f, A, B, H, Ops, Win, Def.OuterMat, Def.SkinMat, Seed);
			}
			else
			{
				const float Fix = LocalLine + WallT * 0.5f;
				RuinStub(bAlongX, Fix, A, B, Seed, Def.FrontMat, 70.f + FMath::Abs(SJ(Seed, 70.f)), 70.f + FMath::Abs(SJ(Seed + 1, 70.f)), Ops, false);
				RubbleRun(bAlongX, Fix, A, B, -1.f, Seed, Def.FrontMat);
			}
		}

		// ---- shared runs: built by the room on whose back (+X/+Y) side they lie
		if (!bBack) { continue; }
		for (int32 i = 0; i < Shared.Num(); ++i)
		{
			const float A = Shared[i].A - Base, B = Shared[i].B - Base;
			const FRoomDef& N = *Shared[i].N;
			const int32 Seed = RSeed + Side * 29 + i * 5 + 300;
			TArray<FOpening> Ops = LocalOps(A, B);
			if (bOffice)
			{
				// the farmhouse's own tall back walls (the wings behind them butt against the old house)
				CurGroup = DCGroups::Shell;
				const TArray<FOpening> Win = WindowsFor(A, B, Ops, 100.f, 225.f);
				TallWall(bAlongX, LocalLine - WallT * 0.5f, -1.f, A, B, HouseH, Ops, Win, Def.OuterMat, Def.SkinMat, Seed);
				continue;
			}
			if (Ops.Num() == 0) { Ops.Add({ (A + B) * 0.5f, DoorW, 0.f, 210.f }); }
			const int32 Idx = SharedWalls.AddDefaulted();
			SharedWalls[Idx].A = CurRoom;
			SharedWalls[Idx].B = GetRoom(N.Type);
			FName G[3];
			for (int32 v = 0; v < 3; ++v)
			{
				G[v] = FName(*FString::Printf(TEXT("SW.%d.%d"), Idx, v));
				TrackedGroups.Add(G[v], FIntPoint(Idx, v));
			}
			const bool bHouse = N.Type == EDCRoomType::Office;
			const float EA = bHouse ? 120.f + FMath::Abs(SJ(Seed, 30.f)) : 60.f + FMath::Abs(SJ(Seed, 60.f));
			const float EB = bHouse ? 140.f + FMath::Abs(SJ(Seed + 1, 30.f)) : 60.f + FMath::Abs(SJ(Seed + 1, 60.f));
			// 0: collapsed front of the back room (no collision) + rubble spilling towards the front plot
			CurGroup = G[0];
			RuinStub(bAlongX, LocalLine, A, B, Seed, N.FrontMat, EA, EB, Ops, false);
			RubbleRun(bAlongX, LocalLine, A, B, -1.f, Seed, N.FrontMat, 0.8f);
			// 1: tall back wall of the front room (the plot behind it is still empty)
			CurGroup = G[1];
			TallWall(bAlongX, LocalLine - WallT * 0.5f, -1.f, A, B, WingH, Ops, {}, Def.OuterMat, Def.SkinMat, Seed);
			// 2: both rooms exist: a rebuilt knee partition with a doorway
			CurGroup = G[2];
			RuinStub(bAlongX, LocalLine, A, B, Seed + 3, N.FrontMat, bHouse ? EA : 0.f, bHouse ? EB : 0.f, Ops, true);
			CurGroup = Main;
		}
		CurGroup = Main;
	}
	if (!bOffice) { BuildWingRoof(Def); }
}

void ADCHideoutBuilder::UpdateSharedWalls()
{
	auto Exists = [](const ADCRoomModule* R) { return R && R->GetLevel() >= 1 && R->GetState() != EDCRoomState::Locked; };
	for (FSharedWall& SW : SharedWalls)
	{
		const bool bA = Exists(SW.A.Get()), bB = Exists(SW.B.Get());
		const int32 Want = (bA && bB) ? 2 : bA ? 1 : bB ? 0 : -1;
		if (Want == SW.Cur) { continue; }
		SW.Cur = Want;
		for (int32 v = 0; v < 3; ++v)
		{
			const bool bOn = v == Want;
			for (int32 i = 0; i < SW.Comps[v].Num(); ++i)
			{
				if (UPrimitiveComponent* P = SW.Comps[v][i].Get())
				{
					P->SetVisibility(bOn, true);
					P->SetCollisionEnabled(bOn ? SW.Coll[v][i].GetValue() : ECollisionEnabled::NoCollision);
				}
			}
		}
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

void ADCHideoutBuilder::BuildLightingAndPost()
{
	// Global lighting (sun/sky/fog/post/exposure/cvars) belongs to ADCLandscape (spawned first by the game mode).
	// The house only registers its day/night-driven materials (window glass follows the sky, dust motes the sun).
	{ TActorIterator<ADCDayNight> It(GetWorld()); if (It) { DayNight = *It; } }
	if (DayNight)
	{
		if (UMaterialInstanceDynamic* W = Cast<UMaterialInstanceDynamic>(M("Window"))) { DayNight->WindowMats.Add(W); }
		DayNight->DustMat = Cast<UMaterialInstanceDynamic>(M("Dust"));
		for (ULocalFogVolumeComponent* LF : SunFogs) { DayNight->AddShaftFog(LF); }
		// porch lanterns on the Office (its cell never moves): glass + a soft warm pool on the ground at night
		DayNight->LampMat = Cast<UMaterialInstanceDynamic>(M("LampHead"));
		FVector2D Wc[4], Q[4];
		DCSite::Grid::Corners(DCSite::Grid::OfficeCell(), Wc);
		const FVector2D Ctr = DCSite::Grid::Centre(DCSite::Grid::OfficeCell());
		for (int32 k = 0; k < 4; ++k) { Q[k] = Wc[k] - Ctr; }
		for (int32 k = 0; k < 2; ++k)
		{
			FVector2D P, Out;
			PorchLampSpot(Q, k, P, Out);
			const FVector2D Arm = Ctr + P + Out * 34.f;
			DayNight->AddStreetLight(FVector(Arm.X, Arm.Y, 176.f), FVector(Arm.X + Out.X * 90.f, Arm.Y + Out.Y * 90.f, 0.f), FLinearColor(1.f, 0.6f, 0.3f), 60.f, 620.f, 75.f, false);
		}
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

	// v3: rooms live on the build grid (DCHideoutGrid.cpp) and are empty for now; the authored room dressing
	// (DCHideoutRooms.cpp, legacy fixed footprints) is not built.
	SpawnGridRooms();
	if (FParse::Param(FCommandLine::Get(), TEXT("DCHouseGround")))
	{
		CurRoom = GetRoom(EDCRoomType::Office);
		CurGroup = DCGroups::Shell;
		Deco(EDCShape::Cube, FVector(0, -500, -53), FVector(9000, 9000, 100), "Dirt");
	}
	FlushBatches();
	BuildLightingAndPost();

	const bool bAll = UDCDevTools::IsBuildAll();
	for (ADCRoomModule* Room : RoomPtrs)
	{
		if (bAll) { Room->ApplyState(EDCRoomState::Built, 3); continue; }
		const bool bOffice = Room->RoomType == EDCRoomType::Office;
		Room->ApplyState(bOffice ? EDCRoomState::Built : EDCRoomState::Locked, bOffice ? 1 : 0);
	}
	SyncGrid(true);

	// scene review camera
	FParse::Value(FCommandLine::Get(), TEXT("DCRoomStates="), ForcedStates, false);
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
	if (bBuilt) { SyncGrid(false); }
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

void ADCHideoutBuilder::ApplyForcedStates()
{
	// "Workshop:B,Garage:2,Lounge:B1" (L locked, B first build, B<n> upgrading over level n, <n> built level n)
	TArray<FString> Items;
	ForcedStates.ParseIntoArray(Items, TEXT(","));
	for (const FString& It : Items)
	{
		FString Name, Val;
		if (!It.Split(TEXT(":"), &Name, &Val)) { continue; }
		const int64 V = StaticEnum<EDCRoomType>()->GetValueByNameString(Name);
		ADCRoomModule* R = V == INDEX_NONE ? nullptr : GetRoom((EDCRoomType)V);
		if (!R) { continue; }
		if (Val.StartsWith(TEXT("L"))) { R->ApplyState(EDCRoomState::Locked, 0); }
		else if (Val.StartsWith(TEXT("B"))) { R->ApplyState(EDCRoomState::Building, FCString::Atoi(*Val.Mid(1))); }
		else { R->ApplyState(EDCRoomState::Built, FMath::Clamp(FCString::Atoi(*Val), 1, 3)); }
	}
	SyncGrid(false);
}

void ADCHideoutBuilder::FrameReview(const FString& Focus)
{
	if (!ForcedStates.IsEmpty()) { ApplyForcedStates(); }
	FBox B(ForceInit);
	if (Focus.Equals(TEXT("wide"), ESearchCase::IgnoreCase))
	{
		// the whole compound plus a margin of countryside and the front of the yard
		const FBox2D C = DCSite::CompoundRect();
		B = FBox(FVector(C.Min.X - 150.f, C.Min.Y - 350.f, 0.f), FVector(C.Max.X + 150.f, C.Max.Y + 150.f, TallH));
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
	const FVector Target(B.GetCenter().X, B.GetCenter().Y, 40.f);
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
