// Wave 0 GREYBOX stub. Agent D replaces this with the real diorama.
#include "World/Scene/DCHideoutBuilder.h"
#include "World/DCRoomModule.h"
#include "World/DCStation.h"
#include "World/DCCriminalSpawnZone.h"
#include "World/DCGarageDoor.h"
#include "Core/DCAssetPaths.h"
#include "Core/DCWorldTags.h"
#include "Core/DCDevTools.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

ADCHideoutBuilder::ADCHideoutBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

static UStaticMesh* LoadCube() { return LoadObject<UStaticMesh>(nullptr, DCPaths::Cube); }

UMaterialInstanceDynamic* ADCHideoutBuilder::MakeSurface(const FLinearColor& Color, float Roughness, float Metallic)
{
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, DCPaths::MatSurface))
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetVectorParameterValue(TEXT("BaseColor"), Color);
		M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		M->SetScalarParameterValue(TEXT("Metallic"), Metallic);
		return M;
	}
	UMaterialInterface* Fallback = LoadObject<UMaterialInterface>(nullptr, DCPaths::BasicShapeMaterial);
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Fallback, this);
	M->SetVectorParameterValue(TEXT("Color"), Color);
	return M;
}

UMaterialInstanceDynamic* ADCHideoutBuilder::MakeEmissive(const FLinearColor& Color, float Strength)
{
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, DCPaths::MatEmissive))
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetVectorParameterValue(TEXT("Color"), Color);
		M->SetScalarParameterValue(TEXT("Strength"), Strength);
		return M;
	}
	return MakeSurface(Color);
}

UStaticMeshComponent* ADCHideoutBuilder::AddBox(AActor* Owner, USceneComponent* Parent, const FVector& Center, const FVector& Size, UMaterialInterface* Mat)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
	C->SetStaticMesh(LoadCube());
	C->SetupAttachment(Parent);
	C->SetRelativeLocation(Center);
	C->SetRelativeScale3D(Size / 100.f);
	if (Mat) { C->SetMaterial(0, Mat); }
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->RegisterComponent();
	Owner->AddInstanceComponent(C);
	return C;
}

ADCStation* ADCHideoutBuilder::SpawnStation(ADCRoomModule* Room, EDCStationType Type, const FString& Name, const FVector& Location, const FVector& PropSize, const FLinearColor& Color)
{
	ADCStation* S = GetWorld()->SpawnActor<ADCStation>(ADCStation::StaticClass(), FTransform(Location + FVector(0, 0, PropSize.Z * 0.5f)));
	S->StationType = Type;
	S->DisplayName = Name;
	S->SetClickExtent(PropSize * 0.5f + FVector(10));
	S->AddPropMesh(LoadCube(), MakeSurface(Color, 0.5f), FTransform(FRotator::ZeroRotator, FVector::ZeroVector, PropSize / 100.f));
	S->AttachToActor(Room, FAttachmentTransformRules::KeepWorldTransform);
	Room->RegisterStation(S);
	return S;
}

void ADCHideoutBuilder::BuildRoom(EDCRoomType Type, const FVector& Origin, const FVector2D& Size)
{
	ADCRoomModule* Room = GetWorld()->SpawnActor<ADCRoomModule>(ADCRoomModule::StaticClass(), FTransform(Origin));
	Room->RoomType = Type;
	Room->LocalBounds = FBox(FVector(-Size.X * 0.5f, -Size.Y * 0.5f, 0), FVector(Size.X * 0.5f, Size.Y * 0.5f, 320));
	USceneComponent* R = Room->GetRootComponent();

	const FLinearColor FloorCol(0.08f, 0.05f, 0.04f);
	const FLinearColor WallCol(0.12f, 0.09f, 0.07f);
	Room->AddToGroup(AddBox(Room, R, FVector(0, 0, -10), FVector(Size.X, Size.Y, 20), MakeSurface(FloorCol, 0.7f)), DCGroups::Shell);
	// Back walls only (cutaway): +X and +Y sides face away from a camera at yaw 45 looking from (-X,-Y).
	Room->AddToGroup(AddBox(Room, R, FVector(Size.X * 0.5f - 10, 0, 150), FVector(20, Size.Y, 300), MakeSurface(WallCol, 0.8f)), DCGroups::Shell);
	Room->AddToGroup(AddBox(Room, R, FVector(0, Size.Y * 0.5f - 10, 150), FVector(Size.X, 20, 300), MakeSurface(WallCol, 0.8f)), DCGroups::Shell);
	// Locked: a dust sheet blob
	Room->AddToGroup(AddBox(Room, R, FVector(0, 0, 60), FVector(Size.X * 0.4f, Size.Y * 0.4f, 120), MakeSurface(FLinearColor(0.3f, 0.3f, 0.32f), 0.9f)), DCGroups::Locked);
	// Level 1 light
	UPointLightComponent* L = NewObject<UPointLightComponent>(Room);
	L->SetupAttachment(R);
	L->SetRelativeLocation(FVector(0, 0, 260));
	L->SetIntensity(6000.f);
	L->SetAttenuationRadius(FMath::Max(Size.X, Size.Y));
	L->SetLightColor(FLinearColor(1.f, 0.75f, 0.45f));
	L->ComponentTags.Add(DCTags::RoomLight);
	L->RegisterComponent();
	Room->AddInstanceComponent(L);
	Room->AddToGroup(L, DCGroups::Level1);

	Rooms.Add(Room);
	RoomPtrs.Add(Room);
}

ADCRoomModule* ADCHideoutBuilder::GetRoom(EDCRoomType Type) const
{
	for (ADCRoomModule* R : RoomPtrs) { if (R && R->RoomType == Type) { return R; } }
	return nullptr;
}

void ADCHideoutBuilder::BuildLightingAndPost()
{
	APostProcessVolume* PPV = GetWorld()->SpawnActor<APostProcessVolume>();
	PPV->bUnbound = true;
	FPostProcessSettings& S = PPV->Settings;
	S.bOverride_AutoExposureMethod = true; S.AutoExposureMethod = AEM_Manual;
	S.bOverride_AutoExposureBias = true; S.AutoExposureBias = 10.f;
	S.bOverride_AutoExposureApplyPhysicalCameraExposure = true; S.AutoExposureApplyPhysicalCameraExposure = false;
}

void ADCHideoutBuilder::BuildHideout()
{
	if (bBuilt) { return; }
	bBuilt = true;

	// Greybox layout (X = right-back, Y = left-back). Office in the middle.
	const FVector2D Std(1000, 800);
	BuildRoom(EDCRoomType::Office,        FVector(0, 0, 0),        Std);
	BuildRoom(EDCRoomType::Lounge,        FVector(1050, 0, 0),     Std);
	BuildRoom(EDCRoomType::Workshop,      FVector(0, -850, 0),     Std);
	BuildRoom(EDCRoomType::Garage,        FVector(1050, -850, 0),  Std);
	BuildRoom(EDCRoomType::ScreeningRoom, FVector(0, 1050, 0),     FVector2D(2050, 1200));
	BuildRoom(EDCRoomType::Clinic,        FVector(-1050, 0, 0),    Std);
	BuildRoom(EDCRoomType::ServerCloset,  FVector(-1050, -850, 0), Std);
	BuildRoom(EDCRoomType::LawyerOffice,  FVector(-1050, 1050, 0), Std);

	ADCRoomModule* Office = GetRoom(EDCRoomType::Office);
	SpawnStation(Office, EDCStationType::DirectorDesk,   TEXT("Director's Desk"), FVector(-100, 50, 0),  FVector(180, 90, 80),  FLinearColor(0.25f, 0.1f, 0.05f));
	SpawnStation(Office, EDCStationType::JobBoard,       TEXT("Job Board"),       FVector(300, 330, 0),  FVector(200, 20, 160), FLinearColor(0.5f, 0.35f, 0.2f));
	SpawnStation(Office, EDCStationType::BlueprintTable, TEXT("Blueprint Table"), FVector(300, -200, 0), FVector(160, 110, 90), FLinearColor(0.15f, 0.25f, 0.45f));
	SpawnStation(Office, EDCStationType::Projector,      TEXT("Projector"),       FVector(-380, 300, 0), FVector(60, 60, 120),  FLinearColor(0.2f, 0.2f, 0.2f));
	SpawnStation(GetRoom(EDCRoomType::Lounge),        EDCStationType::Lounge,        TEXT("The Lounge"),     FVector(1050, 100, 0),    FVector(240, 100, 80), FLinearColor(0.35f, 0.12f, 0.08f));
	SpawnStation(GetRoom(EDCRoomType::Workshop),      EDCStationType::Workbench,     TEXT("Workbench"),      FVector(100, -1100, 0),   FVector(220, 80, 100), FLinearColor(0.3f, 0.3f, 0.3f));
	SpawnStation(GetRoom(EDCRoomType::Garage),        EDCStationType::GarageDoor,    TEXT("Garage"),         FVector(1300, -900, 0),   FVector(300, 160, 120), FLinearColor(0.1f, 0.1f, 0.12f));
	SpawnStation(GetRoom(EDCRoomType::ScreeningRoom), EDCStationType::ScreeningRoom, TEXT("Screening Room"), FVector(0, 1500, 0),      FVector(600, 40, 300), FLinearColor(0.8f, 0.8f, 0.8f));
	SpawnStation(GetRoom(EDCRoomType::Clinic),        EDCStationType::Clinic,        TEXT("Clinic"),         FVector(-1050, 0, 0),     FVector(200, 80, 90),  FLinearColor(0.7f, 0.75f, 0.75f));
	SpawnStation(GetRoom(EDCRoomType::ServerCloset),  EDCStationType::ServerCloset,  TEXT("Server Closet"),  FVector(-1050, -850, 0),  FVector(80, 80, 200),  FLinearColor(0.05f, 0.1f, 0.08f));
	SpawnStation(GetRoom(EDCRoomType::LawyerOffice),  EDCStationType::Lawyer,        TEXT("Lawyer"),         FVector(-1050, 1050, 0),  FVector(160, 80, 80),  FLinearColor(0.2f, 0.12f, 0.06f));

	for (EDCRoomType T : { EDCRoomType::Office, EDCRoomType::Lounge })
	{
		ADCCriminalSpawnZone* Z = GetWorld()->SpawnActor<ADCCriminalSpawnZone>(ADCCriminalSpawnZone::StaticClass(), FTransform(GetRoom(T)->GetActorLocation() + FVector(-150, -150, 50)));
		Z->RoomType = T;
	}
	ADCGarageDoor* Street = GetWorld()->SpawnActor<ADCGarageDoor>(ADCGarageDoor::StaticClass(), FTransform(FVector(-500, -300, 0)));
	Street->RoomType = EDCRoomType::Office;
	ADCGarageDoor* GarageDoor = GetWorld()->SpawnActor<ADCGarageDoor>(ADCGarageDoor::StaticClass(), FTransform(FVector(1050, -1275, 0)));
	GarageDoor->RoomType = EDCRoomType::Garage;

	BuildLightingAndPost();

	for (ADCRoomModule* R : RoomPtrs)
	{
		if (UDCDevTools::IsBuildAll()) { R->ApplyState(EDCRoomState::Built, 3); continue; }
		R->ApplyState(R->RoomType == EDCRoomType::Office ? EDCRoomState::Built : EDCRoomState::Locked, R->RoomType == EDCRoomType::Office ? 1 : 0);
	}
}
