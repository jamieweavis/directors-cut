// Director's Cut — hideout diorama builder: kit, shell, lighting, post, runtime FX. Room dressing is in DCHideoutRooms.cpp.
#include "World/Scene/DCHideoutBuilder.h"
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
	constexpr float TallH = 380.f;   // exterior back walls / projection booth wall
	constexpr float PartH = 55.f;    // interior partitions: knee height so no room hides its neighbour
	constexpr float KneeH = 45.f;    // camera-facing exterior walls (cutaway)
	constexpr float WallT = 20.f;
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

	const FLinearColor GoldCol(0.62f, 0.42f, 0.14f);
	// --- architecture
	Proc("Oak",        1, FLinearColor(0.20f, 0.105f, 0.05f), FLinearColor(0.085f, 0.04f, 0.018f), 0.45f, 0.f, 1.f, 0.4f, 0.f);
	Proc("OakY",       1, FLinearColor(0.20f, 0.105f, 0.05f), FLinearColor(0.085f, 0.04f, 0.018f), 0.45f, 0.f, 1.f, 0.4f, 1.f);
	Proc("DarkWood",   1, FLinearColor(0.075f, 0.032f, 0.016f), FLinearColor(0.03f, 0.013f, 0.007f), 0.4f, 0.f, 1.5f, 0.3f, 0.f);
	Proc("DarkWoodY",  1, FLinearColor(0.075f, 0.032f, 0.016f), FLinearColor(0.03f, 0.013f, 0.007f), 0.4f, 0.f, 1.5f, 0.3f, 1.f);
	Proc("Mahogany",   1, FLinearColor(0.17f, 0.045f, 0.022f), FLinearColor(0.055f, 0.013f, 0.008f), 0.3f, 0.f, 1.6f, 0.2f, 1.f);
	Proc("PaperOffice",11, FLinearColor(0.03f, 0.06f, 0.045f), GoldCol, 0.7f, 0.f, 1.f, 0.6f);
	Proc("PaperLounge",11, FLinearColor(0.10f, 0.02f, 0.025f), GoldCol, 0.7f, 0.f, 1.f, 0.6f);
	Proc("PaperTeal",  11, FLinearColor(0.025f, 0.06f, 0.07f), FLinearColor(0.5f, 0.45f, 0.3f), 0.7f, 0.f, 1.f, 0.7f);
	Proc("Brick",      2, FLinearColor(0.20f, 0.065f, 0.04f), FLinearColor(0.09f, 0.085f, 0.08f), 0.85f, 0.f, 1.f, 0.6f);
	Proc("Concrete",   3, FLinearColor(0.16f, 0.155f, 0.15f), FLinearColor(0.07f, 0.07f, 0.07f), 0.8f, 0.f, 1.f, 0.8f);
	Proc("ConcreteFl", 3, FLinearColor(0.12f, 0.115f, 0.11f), FLinearColor(0.045f, 0.045f, 0.045f), 0.55f, 0.f, 1.f, 1.0f);
	Proc("Tiles",      8, FLinearColor(0.55f, 0.53f, 0.47f), FLinearColor(0.03f, 0.03f, 0.035f), 0.3f, 0.f, 1.f, 0.6f);
	Proc("TileWall",   8, FLinearColor(0.32f, 0.42f, 0.38f), FLinearColor(0.28f, 0.36f, 0.33f), 0.3f, 0.f, 2.5f, 0.7f);
	Proc("CarpetRed",  13, FLinearColor(0.13f, 0.012f, 0.018f), FLinearColor(0.35f, 0.2f, 0.05f), 1.f, 0.f, 1.f, 0.4f);
	Proc("CarpetGrn",  13, FLinearColor(0.02f, 0.06f, 0.04f), FLinearColor(0.3f, 0.2f, 0.06f), 1.f, 0.f, 1.f, 0.4f);
	Proc("Plinth",     3, FLinearColor(0.03f, 0.03f, 0.032f), FLinearColor(0.012f, 0.012f, 0.014f), 0.7f, 0.f, 1.f, 0.f);
	Proc("Trim",       7, FLinearColor(0.75f, 0.58f, 0.32f), FLinearColor(0.3f, 0.22f, 0.12f), 0.28f, 1.f, 1.f, 0.f);
	Proc("Stone",      3, FLinearColor(0.42f, 0.39f, 0.34f), FLinearColor(0.25f, 0.23f, 0.2f), 0.5f, 0.f, 1.f, 0.3f);
	// --- props
	Proc("Brass",      7, FLinearColor(0.85f, 0.6f, 0.28f), FLinearColor(0.28f, 0.22f, 0.1f), 0.25f, 1.f);
	Proc("Chrome",     7, FLinearColor(0.75f, 0.75f, 0.78f), FLinearColor(0.4f, 0.4f, 0.42f), 0.15f, 1.f);
	Proc("Steel",      7, FLinearColor(0.35f, 0.36f, 0.38f), FLinearColor(0.2f, 0.12f, 0.07f), 0.45f, 1.f);
	Proc("Iron",       7, FLinearColor(0.045f, 0.045f, 0.05f), FLinearColor(0.12f, 0.06f, 0.035f), 0.55f, 0.7f);
	Proc("Olive",      7, FLinearColor(0.1f, 0.11f, 0.065f), FLinearColor(0.05f, 0.05f, 0.04f), 0.5f, 0.4f);
	Proc("Black",      0, FLinearColor(0.012f, 0.012f, 0.014f), FLinearColor(0.02f, 0.02f, 0.022f), 0.45f);
	Proc("Rubber",     0, FLinearColor(0.01f, 0.01f, 0.01f), FLinearColor(0.02f, 0.02f, 0.02f), 0.85f);
	Proc("Cream",      0, FLinearColor(0.7f, 0.64f, 0.5f), FLinearColor(0.5f, 0.44f, 0.32f), 0.8f, 0.f, 1.f, 0.3f);
	Proc("Paper",      0, FLinearColor(0.78f, 0.74f, 0.62f), FLinearColor(0.6f, 0.55f, 0.42f), 0.85f, 0.f, 3.f, 0.2f);
	Proc("Photo",      0, FLinearColor(0.06f, 0.06f, 0.07f), FLinearColor(0.22f, 0.2f, 0.18f), 0.4f, 0.f, 6.f, 0.f);
	Proc("Cork",       4, FLinearColor(0.42f, 0.25f, 0.11f), FLinearColor(0.22f, 0.12f, 0.05f), 0.9f);
	Proc("Velvet",     5, FLinearColor(0.2f, 0.012f, 0.018f), FLinearColor(0.55f, 0.1f, 0.1f), 0.9f);
	Proc("VelvetGrn",  5, FLinearColor(0.015f, 0.08f, 0.04f), FLinearColor(0.15f, 0.4f, 0.25f), 0.9f);
	Proc("Leather",    6, FLinearColor(0.12f, 0.035f, 0.02f), FLinearColor(0.24f, 0.11f, 0.06f), 0.45f, 0.f, 1.f, 0.2f);
	Proc("LeatherBrn", 6, FLinearColor(0.11f, 0.05f, 0.025f), FLinearColor(0.22f, 0.13f, 0.07f), 0.5f, 0.f, 1.f, 0.2f);
	Proc("LeatherGrn", 6, FLinearColor(0.02f, 0.07f, 0.035f), FLinearColor(0.05f, 0.12f, 0.06f), 0.45f, 0.f, 2.f, 0.f);
	Proc("Felt",       12, FLinearColor(0.02f, 0.13f, 0.05f), FLinearColor(0.01f, 0.05f, 0.02f), 1.f, 0.f, 2.f, 0.f);
	Proc("Cloth",      12, FLinearColor(0.42f, 0.42f, 0.40f), FLinearColor(0.3f, 0.3f, 0.3f), 0.95f, 0.f, 1.f, 0.4f);
	Proc("Linen",      12, FLinearColor(0.55f, 0.57f, 0.55f), FLinearColor(0.4f, 0.4f, 0.4f), 0.95f, 0.f, 1.f, 0.1f);
	Proc("Canvas",     12, FLinearColor(0.16f, 0.13f, 0.08f), FLinearColor(0.1f, 0.08f, 0.05f), 0.95f, 0.f, 2.f, 0.4f);
	Proc("Hazard",     9, FLinearColor(0.75f, 0.5f, 0.02f), FLinearColor(0.015f, 0.015f, 0.015f), 0.6f);
	Proc("Corrugated", 10, FLinearColor(0.2f, 0.22f, 0.23f), FLinearColor(0.18f, 0.07f, 0.03f), 0.5f, 0.6f);
	Proc("CarPaint",   7, FLinearColor(0.06f, 0.008f, 0.012f), FLinearColor(0.02f, 0.004f, 0.006f), 0.18f, 0.5f, 0.3f, 0.1f);
	Proc("Plank",      1, FLinearColor(0.32f, 0.21f, 0.1f), FLinearColor(0.18f, 0.11f, 0.05f), 0.8f, 0.f, 1.f, 0.6f);
	Proc("Galv",       7, FLinearColor(0.4f, 0.41f, 0.42f), FLinearColor(0.25f, 0.25f, 0.25f), 0.4f, 1.f);
	Proc("Rack",       7, FLinearColor(0.03f, 0.035f, 0.04f), FLinearColor(0.06f, 0.06f, 0.07f), 0.4f, 0.8f);
	Proc("Enamel",     0, FLinearColor(0.6f, 0.62f, 0.58f), FLinearColor(0.45f, 0.45f, 0.42f), 0.3f, 0.f, 1.f, 0.4f);
	Proc("Blueprint",  11, FLinearColor(0.03f, 0.08f, 0.3f), FLinearColor(0.35f, 0.5f, 0.8f), 0.8f, 0.f, 4.f, 0.f, 0.f, FLinearColor(0.1f, 0.25f, 0.8f), 0.4f);
	Proc("Cash",       0, FLinearColor(0.12f, 0.2f, 0.1f), FLinearColor(0.2f, 0.28f, 0.15f), 0.8f, 0.f, 5.f, 0.f);
	Proc("BookRed",    0, FLinearColor(0.2f, 0.02f, 0.02f), FLinearColor(0.1f, 0.01f, 0.01f), 0.6f, 0.f, 4.f, 0.f);
	Proc("BookGrn",    0, FLinearColor(0.02f, 0.1f, 0.05f), FLinearColor(0.01f, 0.05f, 0.03f), 0.6f, 0.f, 4.f, 0.f);
	Proc("BookBlu",    0, FLinearColor(0.02f, 0.04f, 0.12f), FLinearColor(0.01f, 0.02f, 0.06f), 0.6f, 0.f, 4.f, 0.f);
	Proc("BookTan",    0, FLinearColor(0.3f, 0.2f, 0.1f), FLinearColor(0.18f, 0.12f, 0.06f), 0.6f, 0.f, 4.f, 0.f);
	Proc("RedString",  0, FLinearColor(0.6f, 0.01f, 0.01f), FLinearColor(0.4f, 0.0f, 0.0f), 0.6f, 0.f, 1.f, 0.f, 0.f, FLinearColor(1.f, 0.02f, 0.02f), 0.6f);
	Proc("Pin",        0, FLinearColor(0.7f, 0.02f, 0.02f), FLinearColor(0.5f, 0.01f, 0.01f), 0.3f);
	Proc("LampGreen",  0, FLinearColor(0.01f, 0.2f, 0.06f), FLinearColor(0.01f, 0.15f, 0.04f), 0.12f, 0.f, 1.f, 0.f, 0.f, FLinearColor(0.1f, 1.f, 0.3f), 0.35f);
	Proc("Whisky",     0, FLinearColor(0.5f, 0.2f, 0.03f), FLinearColor(0.4f, 0.15f, 0.02f), 0.05f, 0.f, 1.f, 0.f, 0.f, FLinearColor(1.f, 0.45f, 0.08f), 0.6f);
	Proc("Bottle",     0, FLinearColor(0.05f, 0.1f, 0.03f), FLinearColor(0.03f, 0.06f, 0.02f), 0.05f, 0.f, 1.f, 0.f, 0.f, FLinearColor(0.2f, 0.5f, 0.1f), 0.15f);
	Proc("Ember",      0, FLinearColor(0.3f, 0.05f, 0.f), FLinearColor(0.3f, 0.05f, 0.f), 0.5f, 0.f, 1.f, 0.f, 0.f, FLinearColor(1.f, 0.25f, 0.02f), 25.f);
	Proc("Ball1",      0, FLinearColor(0.6f, 0.5f, 0.05f), FLinearColor(0.6f, 0.5f, 0.05f), 0.1f, 0.f, 1.f, 0.f);
	Proc("Ball2",      0, FLinearColor(0.5f, 0.02f, 0.02f), FLinearColor(0.5f, 0.02f, 0.02f), 0.1f, 0.f, 1.f, 0.f);
	Proc("Ball3",      0, FLinearColor(0.02f, 0.05f, 0.4f), FLinearColor(0.02f, 0.05f, 0.4f), 0.1f, 0.f, 1.f, 0.f);
	Proc("BallW",      0, FLinearColor(0.8f, 0.78f, 0.7f), FLinearColor(0.8f, 0.78f, 0.7f), 0.1f, 0.f, 1.f, 0.f);
	// --- emissive
	Emis("Bulb",       FLinearColor(1.f, 0.62f, 0.3f), 40.f);
	Emis("BulbDim",    FLinearColor(1.f, 0.55f, 0.25f), 8.f);
	Emis("BulbCool",   FLinearColor(0.75f, 0.85f, 1.f), 30.f);
	Emis("Tail",       FLinearColor(1.f, 0.03f, 0.02f), 12.f);
	Emis("Head",       FLinearColor(1.f, 0.9f, 0.7f), 15.f);
	Emis("Dial",       FLinearColor(1.f, 0.6f, 0.2f), 3.f);
	Emis("Monitor",    FLinearColor(0.2f, 1.f, 0.5f), 4.f);
	Emis("Radio",      FLinearColor(1.f, 0.55f, 0.15f), 4.f);
	Emis("ExitRed",    FLinearColor(1.f, 0.05f, 0.03f), 10.f);
	Neon("NeonPink",   FLinearColor(1.f, 0.08f, 0.45f), 60.f, 0.6f, 0.f);
	Neon("NeonCyan",   FLinearColor(0.05f, 0.75f, 1.f), 50.f, 0.2f, 3.f);
	Neon("NeonAmber",  FLinearColor(1.f, 0.45f, 0.05f), 40.f, 0.15f, 5.f);
	Neon("NeonGreen",  FLinearColor(0.1f, 1.f, 0.3f), 40.f, 0.3f, 7.f);
	Neon("NeonRed",    FLinearColor(1.f, 0.03f, 0.02f), 30.f, 0.4f, 9.f);
	Neon("LedG",       FLinearColor(0.1f, 1.f, 0.25f), 25.f, 0.9f, 1.f);
	Neon("LedA",       FLinearColor(1.f, 0.5f, 0.05f), 25.f, 0.9f, 2.f);
	Neon("LedR",       FLinearColor(1.f, 0.05f, 0.05f), 25.f, 0.9f, 4.f);
	Neon("CloseRed",   FLinearColor(1.f, 0.02f, 0.02f), 6.f, 0.5f, 11.f);
	Neon("Marquee",    FLinearColor(1.f, 0.7f, 0.35f), 30.f, 0.1f, 13.f);
	// --- special
	Special("Glass", TEXT("/Game/DC/Materials/M_DC_GlassI.M_DC_GlassI"), [](UMaterialInstanceDynamic* D) { D->SetVectorParameterValue("Color", FLinearColor(0.4f, 0.5f, 0.55f)); D->SetScalarParameterValue("Opacity", 0.2f); });
	Special("Rain", TEXT("/Game/DC/Materials/M_DC_Rain.M_DC_Rain"), [](UMaterialInstanceDynamic* D) { D->SetVectorParameterValue("Color", FLinearColor(1.2f, 2.f, 2.8f)); });
	Special("Screen", TEXT("/Game/DC/Materials/M_DC_Screen.M_DC_Screen"), [](UMaterialInstanceDynamic* D) { D->SetScalarParameterValue("Strength", 5.f); });
	Special("ScreenOff", TEXT("/Game/DC/Materials/M_DC_Screen.M_DC_Screen"), [](UMaterialInstanceDynamic* D) { D->SetScalarParameterValue("Strength", 0.05f); });
	Special("Smoke", TEXT("/Game/DC/Materials/M_DC_Smoke.M_DC_Smoke"), [](UMaterialInstanceDynamic* D) { D->SetVectorParameterValue("Color", FLinearColor(2.2f, 2.2f, 2.5f)); D->SetScalarParameterValue("Opacity", 0.45f); });
	Special("Beam", TEXT("/Game/DC/Materials/M_DC_Beam.M_DC_Beam"), [](UMaterialInstanceDynamic* D) { D->SetScalarParameterValue("Strength", 0.9f); });
	Special("ShaftPink", TEXT("/Game/DC/Materials/M_DC_Beam.M_DC_Beam"), [](UMaterialInstanceDynamic* D) { D->SetVectorParameterValue("Color", FLinearColor(1.f, 0.2f, 0.55f)); D->SetScalarParameterValue("Strength", 0.35f); D->SetScalarParameterValue("Flicker", 0.f); });
	Special("Dust", TEXT("/Game/DC/Materials/M_DC_Dust.M_DC_Dust"), [](UMaterialInstanceDynamic* D) { D->SetScalarParameterValue("Strength", 10.f); });
	Special("DustPink", TEXT("/Game/DC/Materials/M_DC_Dust.M_DC_Dust"), [](UMaterialInstanceDynamic* D) { D->SetVectorParameterValue("Color", FLinearColor(1.f, 0.4f, 0.7f)); D->SetScalarParameterValue("Strength", 10.f); });
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
	// M_DC_Neon with no flicker (it carries the instanced-mesh usage flag; M_DC_Emissive does not).
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
	L->SetRelativeLocation(Loc);
	L->IntensityUnits = ELightUnits::Candelas;
	L->Intensity = Cd * LightScale;
	L->SetLightColor(Col);
	L->AttenuationRadius = Radius;
	L->SourceRadius = SourceRadius;
	L->SetCastShadows(bShadows);
	L->bUseInverseSquaredFalloff = true;
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
	L->BarnDoorAngle = 60.f;
	L->BarnDoorLength = 20.f;
	L->SetCastShadows(bShadows);
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
	const FRotator R(0, Yaw, 0);
	const FVector Off = R.RotateVector(FVector::ZeroVector);
	Deco(EDCShape::Cube, Loc + Off + FVector(0, 0, Size.Z * 0.4f), FVector(Size.X, Size.Y, Size.Z * 0.8f), "Cloth", R);
	Deco(EDCShape::Sphere, Loc + FVector(0, 0, Size.Z * 0.78f), FVector(Size.X * 1.06f, Size.Y * 1.06f, Size.Z * 0.5f), "Cloth", R);
	// hem flaring at the floor
	Deco(EDCShape::Cube, Loc + FVector(0, 0, 4), FVector(Size.X + 14, Size.Y + 14, 8), "Cloth", R);
}

void ADCHideoutBuilder::Tape(const FVector& A, const FVector& B)
{
	const FVector D = B - A;
	const float Len = D.Size();
	if (Len < 1.f) { return; }
	const FRotator Rot = FRotationMatrix::MakeFromX(D / Len).Rotator();
	Deco(EDCShape::Cube, (A + B) * 0.5f, FVector(Len, 1.5f, 8.f), "Hazard", Rot, false);
}

namespace
{
	// Tiny stroke font on a 2x3 grid: (x0,y0,x1,y1) segments.
	const TArray<FDCQuad>* Glyph(TCHAR C)
	{
		static TMap<TCHAR, TArray<FDCQuad>> G;
		if (G.Num() == 0)
		{
			G.Add('A', { {0,0,0,2},{0,2,1,3},{1,3,2,2},{2,2,2,0},{0,1.4,2,1.4} });
			G.Add('B', { {0,0,0,3},{0,3,1.5,3},{1.5,3,1.5,1.5},{0,1.5,2,1.5},{2,1.5,2,0},{2,0,0,0} });
			G.Add('C', { {2,3,0,3},{0,3,0,0},{0,0,2,0} });
			G.Add('D', { {0,0,0,3},{0,3,1.2,3},{1.2,3,2,2.2},{2,2.2,2,0.8},{2,0.8,1.2,0},{1.2,0,0,0} });
			G.Add('E', { {2,3,0,3},{0,3,0,0},{0,0,2,0},{0,1.5,1.5,1.5} });
			G.Add('G', { {2,3,0,3},{0,3,0,0},{0,0,2,0},{2,0,2,1.3},{2,1.3,1,1.3} });
			G.Add('H', { {0,0,0,3},{2,0,2,3},{0,1.5,2,1.5} });
			G.Add('I', { {1,0,1,3},{0.4,3,1.6,3},{0.4,0,1.6,0} });
			G.Add('L', { {0,3,0,0},{0,0,2,0} });
			G.Add('M', { {0,0,0,3},{0,3,1,1.6},{1,1.6,2,3},{2,3,2,0} });
			G.Add('N', { {0,0,0,3},{0,3,2,0},{2,0,2,3} });
			G.Add('O', { {0,0,0,3},{0,3,2,3},{2,3,2,0},{2,0,0,0} });
			G.Add('P', { {0,0,0,3},{0,3,2,3},{2,3,2,1.5},{2,1.5,0,1.5} });
			G.Add('R', { {0,0,0,3},{0,3,2,3},{2,3,2,1.5},{2,1.5,0,1.5},{0.8,1.5,2,0} });
			G.Add('S', { {2,3,0,3},{0,3,0,1.5},{0,1.5,2,1.5},{2,1.5,2,0},{2,0,0,0} });
			G.Add('T', { {0,3,2,3},{1,3,1,0} });
			G.Add('U', { {0,3,0,0},{0,0,2,0},{2,0,2,3} });
			G.Add('W', { {0,3,0.5,0},{0.5,0,1,1.6},{1,1.6,1.5,0},{1.5,0,2,3} });
			G.Add('X', { {0,0,2,3},{0,3,2,0} });
			G.Add('Y', { {0,3,1,1.6},{2,3,1,1.6},{1,1.6,1,0} });
			G.Add('+', { {1,0.3,1,2.7},{-0.2,1.5,2.2,1.5} });
		}
		return G.Find(C);
	}
}

void ADCHideoutBuilder::NeonText(const FString& Text, const FVector& Origin, const FVector& Right, const FVector& Up, float Unit, float Dia, FName Mat, bool bVertical)
{
	FVector Cursor = Origin;
	for (int32 i = 0; i < Text.Len(); ++i)
	{
		// vertical text stacks top-down: Origin is the bottom-left of the FIRST letter
		if (const TArray<FDCQuad>* Segs = Glyph(FChar::ToUpper(Text[i])))
		{
			for (const FDCQuad& S : *Segs)
			{
				const FVector A = Cursor + Right * (S.X * Unit) + Up * (S.Y * Unit);
				const FVector B = Cursor + Right * (S.Z * Unit) + Up * (S.W * Unit);
				Tube(A, B, Dia, Mat, false);
			}
		}
		Cursor += bVertical ? -Up * (3.8f * Unit) : Right * (2.8f * Unit);
	}
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
			Tube(P + FVector(0, 0, 20), Q + FVector(0, 0, Height * 0.9f), 3.5f, "Galv"); // diagonal brace
			for (float Z : { 110.f, 220.f })
			{
				if (Z > Height) { continue; }
				Tube(P + FVector(0, 0, Z), Q + FVector(0, 0, Z), 4.f, "Galv");
				Tube(P + Side + FVector(0, 0, Z), Q + Side + FVector(0, 0, Z), 4.f, "Galv");
				// plank deck
				const FVector Mid = (P + Q) * 0.5f + Side * 0.5f + FVector(0, 0, Z + 4);
				Deco(EDCShape::Cube, Mid, FVector(D.Size2D() / Bays - 8.f, 64.f, 4.f), "Plank", D.Rotation());
			}
		}
	}
}

void ADCHideoutBuilder::LockedDressing(const FBox2D& R, const TArray<FDCQuad>& Sheets)
{
	CurGroup = DCGroups::Locked;
	for (const FDCQuad& S : Sheets) { Sheet(FVector(S.X, S.Y, 0), FVector(S.Z, S.Z * 0.8f, S.W), S.X * 0.1f); }
	// tape X across the camera-facing side, strung between two sawhorses
	const FVector2D C = R.GetCenter();
	const float Fx = R.Min.X + 60.f, Y0 = R.Min.Y + 80.f, Y1 = R.Max.Y - 80.f;
	for (float Y : { Y0, Y1 })
	{
		Deco(EDCShape::Cube, FVector(Fx, Y, 80), FVector(10, 90, 8), "Hazard");
		Tube(FVector(Fx - 25, Y - 40, 0), FVector(Fx, Y - 35, 80), 5, "Plank");
		Tube(FVector(Fx + 25, Y - 40, 0), FVector(Fx, Y - 35, 80), 5, "Plank");
		Tube(FVector(Fx - 25, Y + 40, 0), FVector(Fx, Y + 35, 80), 5, "Plank");
		Tube(FVector(Fx + 25, Y + 40, 0), FVector(Fx, Y + 35, 80), 5, "Plank");
	}
	Tape(FVector(Fx, Y0, 95), FVector(Fx, Y1, 70));
	Tape(FVector(Fx, Y0, 70), FVector(Fx, Y1, 95));
	const float Fy = R.Min.Y + 60.f, X0 = R.Min.X + 80.f, X1 = R.Max.X - 80.f;
	Tape(FVector(X0, Fy, 30), FVector(X1, Fy, 40));
	// "CLOSED" in faint red neon on the floor-front
	NeonText(TEXT("CLOSED"), FVector(Fx - 20.f, C.Y - 70.f, 2.f), FVector(0, 1, 0), FVector(1, 0, 0), 8.f, 2.2f, "CloseRed");
	// cold rim light from the back corner so the sheets read as silhouettes
	Spot(FVector(R.Max.X - 40, R.Max.Y - 40, TallH + 80), FVector(C.X - 60, C.Y - 60, 0), FLinearColor(0.35f, 0.5f, 1.f), 260.f, 2600.f, 25.f, 60.f, false, false);
}

void ADCHideoutBuilder::BuildingDressing(const FBox2D& R)
{
	CurGroup = DCGroups::Building;
	Scaffold(FVector(R.Max.X - 50, R.Min.Y + 60, 0), FVector(R.Max.X - 50, R.Max.Y - 60, 0), 300.f);
	Scaffold(FVector(R.Min.X + 60, R.Max.Y - 50, 0), FVector(R.Max.X - 120, R.Max.Y - 50, 0), 300.f);
	// work lights on tripods
	const FVector2D C = R.GetCenter();
	for (const FVector2D& P : { FVector2D(R.Min.X + 90, R.Min.Y + 90), FVector2D(C.X + 60, C.Y - 30) })
	{
		const FVector B(P.X, P.Y, 0);
		Tube(B + FVector(-25, -25, 0), B + FVector(0, 0, 170), 3, "Iron");
		Tube(B + FVector(25, -25, 0), B + FVector(0, 0, 170), 3, "Iron");
		Tube(B + FVector(0, 30, 0), B + FVector(0, 0, 170), 3, "Iron");
		Deco(EDCShape::Cube, B + FVector(0, 0, 180), FVector(26, 12, 20), "Hazard");
		Deco(EDCShape::Cube, B + FVector(7, 7, 180), FVector(14, 2, 16), "Bulb", FRotator(0, 45, 0), false);
		Point(B + FVector(25, 25, 185), FLinearColor(1.f, 0.85f, 0.6f), 200.f, 900.f, false, false, 8.f);
	}
	// paint buckets, sawhorse, plank stack
	Deco(EDCShape::Cylinder, FVector(C.X - 120, C.Y + 120, 15), FVector(28, 28, 30), "Enamel");
	Deco(EDCShape::Cylinder, FVector(C.X - 90, C.Y + 150, 15), FVector(28, 28, 30), "Galv");
	for (int32 i = 0; i < 5; ++i) { Deco(EDCShape::Cube, FVector(C.X + 120, C.Y + 140, 3 + i * 6), FVector(220, 22, 5), "Plank", FRotator(0, 10.f + i * 3.f, 0)); }
	// ladder
	const FVector L0(R.Max.X - 140, R.Min.Y + 140, 0);
	Tube(L0, L0 + FVector(60, 0, 280), 5, "Plank");
	Tube(L0 + FVector(0, 45, 0), L0 + FVector(60, 45, 280), 5, "Plank");
	for (int32 i = 1; i < 9; ++i) { Tube(L0 + FVector(60, 0, 280) * (i / 9.f), L0 + FVector(60, 0, 280) * (i / 9.f) + FVector(0, 45, 0), 3.5f, "Plank"); }
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

	// floor slab (walkable), foundation plinth, bright bevel trim around the edge so the silhouette reads in the void
	Solid(FVector(0, 0, -15), FVector(Half.X * 2, Half.Y * 2, 30), Def.FloorMat, true);
	Deco(EDCShape::Cube, FVector(0, 0, -70), FVector(Half.X * 2 - 30, Half.Y * 2 - 30, 80), "Plinth");
	Deco(EDCShape::Cube, FVector(X0 - 2, 0, -18), FVector(6, Half.Y * 2 + 8, 6), "Trim");
	Deco(EDCShape::Cube, FVector(X1 + 2, 0, -18), FVector(6, Half.Y * 2 + 8, 6), "Trim");
	Deco(EDCShape::Cube, FVector(0, Y0 - 2, -18), FVector(Half.X * 2 + 8, 6, 6), "Trim");
	Deco(EDCShape::Cube, FVector(0, Y1 + 2, -18), FVector(Half.X * 2 + 8, 6, 6), "Trim");
	Deco(EDCShape::Cube, FVector(X0 - 1, 0, -2), FVector(3, Half.Y * 2 + 2, 4), "Stone");
	Deco(EDCShape::Cube, FVector(0, Y0 - 1, -2), FVector(Half.X * 2 + 2, 3, 4), "Stone");

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
			const TArray<FOpening> Ops = LocalOps(E.Key, E.Value);
			if (bBack)
			{
				WallRun(bAlongX, Fix, A, B, 0.f, TallH, WallT, Ops, "Brick", true);
				// interior skin: wallpaper + dark wainscot + brass dado rail + crown band
				const float Skin = Fix + Inward * (WallT * 0.5f + 1.f);
				WallRun(bAlongX, Skin, A, B, 100.f, TallH - 30.f, 2.f, Ops, Def.WallMat, false);
				if (Def.bWainscot)
				{
					WallRun(bAlongX, Skin + Inward * 1.f, A, B, 0.f, 100.f, 4.f, Ops, "DarkWood", false);
					WallRun(bAlongX, Skin + Inward * 2.5f, A, B, 98.f, 104.f, 3.f, Ops, "Trim", false);
				}
				else
				{
					WallRun(bAlongX, Skin, A, B, 0.f, 100.f, 2.f, Ops, Def.WallMat, false);
				}
				WallRun(bAlongX, Skin + Inward * 2.f, A, B, TallH - 30.f, TallH - 22.f, 4.f, Ops, "Trim", false);
				WallRun(bAlongX, Skin, A, B, TallH - 22.f, TallH, 2.f, Ops, "DarkWood", false);
				// coping on top catches the light
				WallRun(bAlongX, Fix, A - 4, B + 4, TallH, TallH + 6.f, WallT + 10.f, {}, "Stone", false);
			}
			else
			{
				WallRun(bAlongX, Fix, A, B, 0.f, KneeH, WallT, Ops, "Brick", true);
				WallRun(bAlongX, Fix, A - 3, B + 3, KneeH, KneeH + 5.f, WallT + 8.f, {}, "Trim", false);
			}
		}
		// interior partitions are built by the room on whose +X/+Y side they lie
		if (bBack)
		{
			for (int32 i = 0; i < Shared.Num(); ++i)
			{
				const float A = Shared[i].Key - Base, B = Shared[i].Value - Base;
				const bool bBooth = Def.Type == EDCRoomType::Office && SharedWith[i] == EDCRoomType::OpsRoom;
				const float H = bBooth ? TallH : PartH;
				TArray<FOpening> Ops = LocalOps(Shared[i].Key, Shared[i].Value);
				if (Ops.Num() == 0 && (B - A) > 300.f) { Ops.Add({ (A + B) * 0.5f, 130.f, 0.f, 230.f }); }
				WallRun(bAlongX, LocalLine, A, B, 0.f, H, WallT, Ops, bBooth ? FName("Brick") : Def.WallMat, true);
				WallRun(bAlongX, LocalLine, A, B, H, H + 5.f, WallT + 8.f, Ops, "Trim", false);
				if (bBooth)
				{
					const float Skin = LocalLine - (WallT * 0.5f + 1.f);
					WallRun(bAlongX, Skin, A, B, 100.f, TallH - 30.f, 2.f, Ops, Def.WallMat, false);
					WallRun(bAlongX, Skin - 1.f, A, B, 0.f, 100.f, 4.f, Ops, "DarkWood", false);
					WallRun(bAlongX, Skin - 2.5f, A, B, 98.f, 104.f, 3.f, Ops, "Trim", false);
					WallRun(bAlongX, Skin - 2.f, A, B, TallH - 30.f, TallH - 22.f, 4.f, Ops, "Trim", false);
					WallRun(bAlongX, LocalLine, A - 4, B + 4, TallH, TallH + 6.f, WallT + 10.f, {}, "Stone", false);
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

void ADCHideoutBuilder::SetCVars()
{
	auto Set = [](const TCHAR* Name, const TCHAR* Value)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name)) { V->Set(Value, ECVF_SetByCode); }
	};
	// Manual exposure lives around EV ~ -3..-1 (dim practicals). Centre Lumen's cached-lighting range on it so nothing clips
	// (removes the "Cached lighting in Lumen ... r.EyeAdaptation.CachedLightingPreExposure" warning).
	float EV = -2.0f;
	FParse::Value(FCommandLine::Get(), TEXT("DCExposure="), EV);
	float CachedEV = FMath::Clamp(2.f - EV, -16.f, 16.f);
	FParse::Value(FCommandLine::Get(), TEXT("DCCachedEV="), CachedEV);
	Set(TEXT("r.EyeAdaptation.CachedLightingPreExposure"), *FString::SanitizeFloat(CachedEV));
	Set(TEXT("r.LocalFogVolume.RenderIntoVolumetricFog"), TEXT("1"));
	Set(TEXT("r.LocalFogVolume.MaxDensityIntoVolumetricFog"), TEXT("0.5"));
	Set(TEXT("r.VolumetricFog.GridPixelSize"), TEXT("12"));
	Set(TEXT("r.VolumetricFog.GridSizeZ"), TEXT("96"));
	Set(TEXT("r.VolumetricFog.HistoryWeight"), TEXT("0.9"));
	Set(TEXT("r.MotionBlurQuality"), TEXT("0"));
}

void ADCHideoutBuilder::BuildLightingAndPost()
{
	SetCVars();

	APostProcessVolume* PPV = GetWorld()->SpawnActor<APostProcessVolume>();
	PPV->bUnbound = true;
	PPV->Priority = -1.f; // gameplay (camera / reveal) volumes can override
	FPostProcessSettings& S = PPV->Settings;
	float EV = -2.0f;
	FParse::Value(FCommandLine::Get(), TEXT("DCExposure="), EV);
	S.bOverride_AutoExposureMethod = true; S.AutoExposureMethod = AEM_Manual;
	S.bOverride_AutoExposureBias = true; S.AutoExposureBias = EV;
	S.bOverride_AutoExposureApplyPhysicalCameraExposure = true; S.AutoExposureApplyPhysicalCameraExposure = false;
	S.bOverride_LocalExposureMethod = true; S.LocalExposureMethod = ELocalExposureMethod::Bilateral;
	S.bOverride_LocalExposureHighlightContrastScale = true; S.LocalExposureHighlightContrastScale = 0.85f;
	S.bOverride_LocalExposureShadowContrastScale = true; S.LocalExposureShadowContrastScale = 1.0f;
	S.bOverride_BloomMethod = true; S.BloomMethod = BM_SOG;
	S.bOverride_BloomIntensity = true; S.BloomIntensity = 0.55f;
	S.bOverride_BloomThreshold = true; S.BloomThreshold = -1.f;
	S.bOverride_FilmGrainIntensity = true; S.FilmGrainIntensity = 0.22f;
	S.bOverride_FilmGrainIntensityShadows = true; S.FilmGrainIntensityShadows = 0.6f;
	S.bOverride_VignetteIntensity = true; S.VignetteIntensity = 0.55f;
	S.bOverride_SceneFringeIntensity = true; S.SceneFringeIntensity = 0.9f;
	S.bOverride_MotionBlurAmount = true; S.MotionBlurAmount = 0.f;
	S.bOverride_LensFlareIntensity = true; S.LensFlareIntensity = 0.f;
	// teal shadows / amber highlights
	S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(1.0f, 1.0f, 1.0f, 1.08f);
	S.bOverride_ColorContrast = true; S.ColorContrast = FVector4(1.f, 1.f, 1.f, 1.06f);
	S.bOverride_ColorGainShadows = true; S.ColorGainShadows = FVector4(0.88f, 1.0f, 1.08f, 1.f);
	S.bOverride_ColorOffsetShadows = true; S.ColorOffsetShadows = FVector4(0.f, 0.002f, 0.004f, 0.f);
	S.bOverride_ColorGainHighlights = true; S.ColorGainHighlights = FVector4(1.08f, 1.0f, 0.86f, 1.f);
	S.bOverride_ColorSaturationHighlights = true; S.ColorSaturationHighlights = FVector4(1.f, 1.f, 1.f, 0.95f);
	S.bOverride_ToneCurveAmount = true; S.ToneCurveAmount = 1.f;
	// Lumen: no sky, so make indoor bounce count
	S.bOverride_LumenSceneLightingQuality = true; S.LumenSceneLightingQuality = 1.f;
	S.bOverride_LumenFinalGatherQuality = true; S.LumenFinalGatherQuality = 1.f;
	S.bOverride_LumenSkylightLeaking = true; S.LumenSkylightLeaking = 0.f;
	S.bOverride_IndirectLightingIntensity = true; S.IndirectLightingIntensity = 1.2f;

	// Height fog with ZERO density: only here because Volumetric Fog (needed for the light shafts in local fog volumes)
	// hangs off the height fog component. The void itself gets no fog at all.
	AExponentialHeightFog* Fog = GetWorld()->SpawnActor<AExponentialHeightFog>();
	if (UExponentialHeightFogComponent* F = Fog ? Fog->GetComponent() : nullptr)
	{
		F->SetFogDensity(0.0002f);
		F->SetSecondFogDensity(0.f);
		F->SetFogHeightFalloff(0.001f);
		F->SetFogInscatteringColor(FLinearColor::Black);
		F->SetDirectionalInscatteringColor(FLinearColor::Black);
		F->SetVolumetricFog(true);
		F->SetVolumetricFogScatteringDistribution(0.55f);
		F->SetVolumetricFogAlbedo(FColor::White);
		F->SetVolumetricFogExtinctionScale(1.f);
		F->SetVolumetricFogStartDistance(2500.f);
		F->SetVolumetricFogDistance(26000.f);
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
		{ EDCRoomType::Office,        R(-500,  500, -400,  400), "OakY",       "PaperOffice", true },
		{ EDCRoomType::Workshop,      R(-500,  500, -1400, -400), "ConcreteFl", "Brick",      false },
		{ EDCRoomType::Garage,        R( 500, 1300, -1400, -400), "ConcreteFl", "Concrete",   false },
		{ EDCRoomType::OpsRoom, R(-500, 1300,  400, 1600), "CarpetRed",  "PaperLounge", true },
		{ EDCRoomType::Lounge,        R(-1300, -500, -1400, -400), "Oak",       "PaperLounge", true },
		{ EDCRoomType::ServerRoom,  R(-1300, -500, -400,  400), "ConcreteFl", "Concrete",   false },
		{ EDCRoomType::LawyerOffice,  R(-1300, -500,  400, 1000), "CarpetGrn",  "PaperTeal",  true },
		{ EDCRoomType::Clinic,        R(-1300, -500, 1000, 1600), "Tiles",      "TileWall",   false },
	};
	// Openings in room-local coordinates along the side. side: 0 +X, 1 +Y, 2 -X, 3 -Y
	auto Op = [this](EDCRoomType T, int32 Side, float C, float W, float B, float Tp) { RoomOpenings.FindOrAdd((uint8)T).Add(TPair<int32, FOpening>(Side, FOpening{ C, W, B, Tp })); };
	Op(EDCRoomType::Office, 0, 150.f, 220.f, 165.f, 335.f);   // high window (neon spill)
	Op(EDCRoomType::Office, 0, -250.f, 120.f, 0.f, 225.f);    // street door
	Op(EDCRoomType::Office, 1, 300.f, 56.f, 95.f, 140.f);     // projection port
	Op(EDCRoomType::Office, 1, -430.f, 90.f, 0.f, 215.f);     // booth door to the auditorium
	Op(EDCRoomType::Garage, 0, 0.f, 360.f, 0.f, 290.f);       // roll-up door

	for (const FRoomDef& D : Defs) { SpawnRoom(D); }
	for (const FRoomDef& D : Defs) { BuildShell(D); }

	BuildOffice();
	BuildLounge();
	BuildWorkshop();
	BuildGarage();
	BuildScreening();
	BuildClinic();
	BuildServerCloset();
	BuildLawyer();
	FlushBatches();
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
	if (UStaticMesh* Cone = Meshes[(int32)EDCShape::Cone]) { UE_LOG(LogDC, Display, TEXT("[Scene] cone bounds %s"), *Cone->GetBoundingBox().ToString()); }
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
		const float Burst = FMath::Frac(FMath::Sin(FMath::FloorToFloat(T * 0.05f) * 3.17f + F.Seed) * 9631.1f) > 0.75f ? 1.f : 0.f;
		const float K = 1.f - F.Depth * (Hash * 0.4f + Burst * (Hash > 0.5f ? 0.6f : 0.f));
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
		UE_LOG(LogDC, Display, TEXT("[SceneShots] avg frame %.2f ms (%.0f fps)"), PerfFrames ? PerfAccum / PerfFrames * 1000.0 : 0.0, PerfFrames ? PerfFrames / PerfAccum : 0.0);
		UE_LOG(LogDC, Display, TEXT("[SceneShots] %s"), *File);
		bReviewPrepared = false;
		++ReviewStep;
		ReviewNext = Time + 1.0;
		return;
	}
	ReviewShots.Reset();
	if (GEngine) { GEngine->Exec(GetWorld(), TEXT("quit")); }
}
