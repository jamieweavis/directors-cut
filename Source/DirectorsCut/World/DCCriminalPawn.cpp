#include "World/DCCriminalPawn.h"
#include "World/DCCriminalSpawnZone.h"
#include "World/DCGarageDoor.h"
#include "World/DCWorldLabel.h"
#include "World/DCWorldDirector.h"
#include "Animation/AnimSequence.h"
#include "Core/DCAssetPaths.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ReferenceSkeleton.h"

namespace
{
	constexpr float HalfHeight = 90.f;
	constexpr float WalkSpeed = 165.f;
	constexpr float LimpSpeed = 95.f;
	constexpr float HurrySpeed = 300.f;     // walking out to / in from the door
	constexpr float HurryLimpSpeed = 140.f;
	constexpr float LevelUpLength = 1.1f;

	template <typename T>
	T* LoadQuiet(const TCHAR* Path) { return LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet); }
}

ADCCriminalPawn::ADCCriminalPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCapsuleComponent* Cap = GetCapsuleComponent();
	Cap->InitCapsuleSize(34.f, HalfHeight);
	Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cap->SetCollisionResponseToAllChannels(ECR_Ignore);
	Cap->SetHiddenInGame(true);

	USkeletalMeshComponent* M = GetMesh();
	M->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight));
	M->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;

	UCharacterMovementComponent* CM = GetCharacterMovement();
	CM->bRunPhysicsWithNoController = true;
	// The diorama floors have no collision, so pawns "fly" at a fixed height (not falling = ABP walk/idle).
	CM->DefaultLandMovementMode = MOVE_Flying;
	CM->GravityScale = 0.f;
	CM->MaxFlySpeed = WalkSpeed;
	CM->MaxAcceleration = 700.f;
	CM->BrakingDecelerationFlying = 1100.f;
	CM->bOrientRotationToMovement = true;
	CM->RotationRate = FRotator(0.f, 420.f, 0.f);
}

void ADCCriminalPawn::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
}

void ADCCriminalPawn::InitFromCriminal(const FDCCriminal& C, ADCWorldDirector* InDirector)
{
	Director = InDirector;
	CriminalId = C.Id;
	Type = C.Type;
	Rng.Initialize(0x5EED + C.Id * 977);
	SetupVisuals();
	bInitialized = true;
	UpdateFromCriminal(C);
}

void ADCCriminalPawn::UpdateFromCriminal(const FDCCriminal& C)
{
	ShortName = C.Nickname.IsEmpty() ? C.FirstName : C.Nickname;
	Level = C.Level;
	const bool bNowInjured = C.Status == EDCCriminalStatus::Injured;
	if (bNowInjured != bInjured || !BodyMIDs.Num())
	{
		bInjured = bNowInjured;
		ApplyBodyTint();
	}
	if (Bandage) { Bandage->SetVisibility(bInjured && !IsHidden()); }
	UpdateSpeed();
	if (LevelUpTime < 0.f) { RefreshPlate(); }
}

UMaterialInstanceDynamic* ADCCriminalPawn::MakeSurfaceMID(const FLinearColor& Color, float Emissive)
{
	if (UMaterialInterface* Base = LoadQuiet<UMaterialInterface>(DCPaths::MatSurface))
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetVectorParameterValue(TEXT("BaseColor"), Color);
		M->SetScalarParameterValue(TEXT("Roughness"), 0.7f);
		if (Emissive > 0.f)
		{
			M->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
			M->SetScalarParameterValue(TEXT("EmissiveStrength"), Emissive);
		}
		return M;
	}
	if (UMaterialInterface* Basic = LoadQuiet<UMaterialInterface>(DCPaths::BasicShapeMaterial))
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Basic, this);
		M->SetVectorParameterValue(TEXT("Color"), Color);
		return M;
	}
	return nullptr;
}

void ADCCriminalPawn::SetupVisuals()
{
	USkeletalMeshComponent* M = GetMesh();
	USkeletalMesh* Mesh = LoadQuiet<USkeletalMesh>((CriminalId % 2 == 0) ? DCPaths::QuinnMesh : DCPaths::MannyMesh);
	if (!Mesh) { Mesh = LoadQuiet<USkeletalMesh>(DCPaths::MannyMesh); }
	if (Mesh)
	{
		M->SetSkeletalMesh(Mesh);
		AnimClass = LoadClass<UAnimInstance>(nullptr, DCPaths::UnarmedAnimBP, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (AnimClass)
		{
			M->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			M->SetAnimInstanceClass(AnimClass);
		}
		UMaterialInterface* Crim = LoadQuiet<UMaterialInterface>(DCPaths::MatCriminal);
		for (int32 i = 0; i < M->GetNumMaterials(); ++i)
		{
			UMaterialInstanceDynamic* MID = Crim ? UMaterialInstanceDynamic::Create(Crim, this) : MakeSurfaceMID(FLinearColor(0.04f, 0.04f, 0.045f), 0.f);
			if (MID) { M->SetMaterial(i, MID); BodyMIDs.Add(MID); }
		}
	}
	else
	{
		// Primitive fallback body: a dark pill with a head.
		bUsingFallbackBody = true;
		UStaticMesh* Cyl = LoadQuiet<UStaticMesh>(DCPaths::Cylinder);
		UStaticMesh* Sph = LoadQuiet<UStaticMesh>(DCPaths::Sphere);
		auto AddPart = [&](UStaticMesh* SM, const FVector& Loc, const FVector& Scale)
		{
			if (!SM) { return; }
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
			C->SetStaticMesh(SM);
			C->SetupAttachment(GetCapsuleComponent());
			C->SetRelativeLocation(Loc);
			C->SetRelativeScale3D(Scale);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			UMaterialInstanceDynamic* MID = MakeSurfaceMID(FLinearColor(0.04f, 0.04f, 0.045f), 0.f);
			if (MID) { C->SetMaterial(0, MID); BodyMIDs.Add(MID); }
			C->RegisterComponent();
			AddInstanceComponent(C);
		};
		AddPart(Cyl, FVector(0, 0, -15), FVector(0.5f, 0.42f, 1.4f));
		AddPart(Sph, FVector(0, 0, 70), FVector(0.3f));
	}
	ApplyBodyTint();
	BuildProps();

	Plate = UDCWorldLabel::CreateLabelComponent(this, GetCapsuleComponent(), FVector(0.f, 0.f, HalfHeight + 22.f));

	// Level-up ring + light (hidden until needed).
	if (UStaticMesh* Cyl = LoadQuiet<UStaticMesh>(DCPaths::Cylinder))
	{
		LevelRing = NewObject<UStaticMeshComponent>(this);
		LevelRing->SetStaticMesh(Cyl);
		LevelRing->SetupAttachment(GetCapsuleComponent());
		LevelRing->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight + 2.f));
		LevelRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LevelRing->SetCastShadow(false);
		if (UMaterialInterface* E = LoadQuiet<UMaterialInterface>(DCPaths::MatEmissive))
		{
			LevelRingMID = UMaterialInstanceDynamic::Create(E, this);
			LevelRingMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.78f, 0.25f));
			LevelRing->SetMaterial(0, LevelRingMID);
		}
		LevelRing->SetVisibility(false);
		LevelRing->RegisterComponent();
		AddInstanceComponent(LevelRing);
	}
	LevelLight = NewObject<UPointLightComponent>(this);
	LevelLight->SetupAttachment(GetCapsuleComponent());
	LevelLight->SetRelativeLocation(FVector(0.f, 0.f, 20.f));
	LevelLight->SetLightColor(FLinearColor(1.f, 0.75f, 0.3f));
	LevelLight->SetAttenuationRadius(320.f);
	LevelLight->SetCastShadows(false);
	LevelLight->SetIntensity(0.f);
	LevelLight->SetVisibility(false);
	LevelLight->RegisterComponent();
	AddInstanceComponent(LevelLight);
}

void ADCCriminalPawn::ApplyBodyTint()
{
	const FLinearColor Accent = DC::TypeColor(Type);
	const FLinearColor Base = bInjured ? FLinearColor(0.075f, 0.03f, 0.025f) : FLinearColor(0.035f, 0.033f, 0.032f);
	for (UMaterialInstanceDynamic* MID : BodyMIDs)
	{
		if (!MID) { continue; }
		MID->SetVectorParameterValue(TEXT("BaseColor"), Base);
		MID->SetVectorParameterValue(TEXT("AccentColor"), bInjured ? FMath::Lerp(Accent, FLinearColor(0.6f, 0.6f, 0.6f), 0.5f) : Accent);
		MID->SetVectorParameterValue(TEXT("Color"), bUsingFallbackBody ? Accent * 0.35f : Base); // BasicShapeMaterial fallback
	}
}

UStaticMeshComponent* ADCCriminalPawn::AddProp(UStaticMesh* SM, const FName& Bone, const FVector& MeshSpaceOffset, const FRotator& MeshSpaceRot, const FVector& Scale, const FLinearColor& Color, float Emissive)
{
	if (!SM) { return nullptr; }
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(SM);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(true);
	if (UMaterialInstanceDynamic* MID = MakeSurfaceMID(Color, Emissive)) { C->SetMaterial(0, MID); }

	USkeletalMeshComponent* M = GetMesh();
	USkeletalMesh* Mesh = M ? M->GetSkeletalMeshAsset() : nullptr;
	const int32 BoneIdx = Mesh ? Mesh->GetRefSkeleton().FindBoneIndex(Bone) : INDEX_NONE;
	if (BoneIdx != INDEX_NONE)
	{
		// Author the prop in mesh component space (Z up, character faces +Y), then express it relative to the bone's ref pose.
		const FTransform BoneCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Mesh->GetRefSkeleton(), BoneIdx);
		const FTransform PropCS(MeshSpaceRot, BoneCS.GetLocation() + MeshSpaceOffset, Scale);
		C->SetupAttachment(M, Bone);
		C->SetRelativeTransform(PropCS.GetRelativeTransform(BoneCS));
	}
	else
	{
		// Fallback body: approximate head / hand positions on the capsule.
		const bool bHead = Bone == TEXT("head");
		const FVector Anchor = bHead ? FVector(0, 0, 62) : FVector(Bone == TEXT("hand_l") ? -30.f : 30.f, 0.f, -10.f);
		C->SetupAttachment(GetCapsuleComponent());
		// Mesh-space +Y (forward) maps to actor +X.
		C->SetRelativeLocation(FVector(MeshSpaceOffset.Y, -MeshSpaceOffset.X, MeshSpaceOffset.Z) + Anchor);
		C->SetRelativeRotation(MeshSpaceRot + FRotator(0.f, 90.f, 0.f));
		C->SetRelativeScale3D(Scale);
	}
	C->RegisterComponent();
	AddInstanceComponent(C);
	PropComps.Add(C);
	return C;
}

void ADCCriminalPawn::BuildProps()
{
	UStaticMesh* Cube = LoadQuiet<UStaticMesh>(DCPaths::Cube);
	UStaticMesh* Cyl = LoadQuiet<UStaticMesh>(DCPaths::Cylinder);
	UStaticMesh* Sph = LoadQuiet<UStaticMesh>(DCPaths::Sphere);
	const FLinearColor Accent = DC::TypeColor(Type);
	const FLinearColor Dark(0.03f, 0.03f, 0.03f);
	const FName Head(TEXT("head"));
	// The ref-pose head bone sits low in the skull; offsets are measured from it.
	switch (Type)
	{
	case EDCCriminalType::Wheelman: // flat cap
		AddProp(Cyl, Head, FVector(0, 1, 17), FRotator(-6, 0, 0), FVector(0.215f, 0.235f, 0.06f), FLinearColor(0.16f, 0.1f, 0.06f));
		AddProp(Cube, Head, FVector(0, 12, 15.5f), FRotator(-8, 0, 0), FVector(0.17f, 0.11f, 0.012f), FLinearColor(0.12f, 0.07f, 0.04f));
		AddProp(Cyl, Head, FVector(0, 1, 15), FRotator(-6, 0, 0), FVector(0.22f, 0.24f, 0.012f), Accent, 2.f);
		break;
	case EDCCriminalType::Muscle: // gloves
		AddProp(Sph, TEXT("hand_r"), FVector(0, 0, 0), FRotator::ZeroRotator, FVector(0.14f), Accent * 0.5f, 0.6f);
		AddProp(Sph, TEXT("hand_l"), FVector(0, 0, 0), FRotator::ZeroRotator, FVector(0.14f), Accent * 0.5f, 0.6f);
		break;
	case EDCCriminalType::Hacker: // headset
		AddProp(Cube, Head, FVector(0, 0, 21), FRotator::ZeroRotator, FVector(0.2f, 0.035f, 0.025f), Dark);
		AddProp(Cyl, Head, FVector(10.5f, 0, 9), FRotator(0, 0, 90), FVector(0.08f, 0.08f, 0.035f), Dark);
		AddProp(Cyl, Head, FVector(-10.5f, 0, 9), FRotator(0, 0, 90), FVector(0.08f, 0.08f, 0.035f), Dark);
		AddProp(Cube, Head, FVector(9.5f, 7, 4), FRotator(0, 30, 0), FVector(0.012f, 0.12f, 0.012f), Accent, 4.f);
		break;
	case EDCCriminalType::Ghost: // beanie
		AddProp(Sph, Head, FVector(0, 0, 14), FRotator::ZeroRotator, FVector(0.225f, 0.24f, 0.19f), FLinearColor(0.05f, 0.055f, 0.08f));
		AddProp(Cyl, Head, FVector(0, 0, 10), FRotator::ZeroRotator, FVector(0.23f, 0.245f, 0.04f), Accent * 0.4f, 0.8f);
		break;
	case EDCCriminalType::Face: // fedora
		AddProp(Cyl, Head, FVector(0, 1, 17), FRotator::ZeroRotator, FVector(0.38f, 0.38f, 0.012f), FLinearColor(0.09f, 0.08f, 0.07f));
		AddProp(Cyl, Head, FVector(0, 1, 23), FRotator::ZeroRotator, FVector(0.21f, 0.21f, 0.12f), FLinearColor(0.09f, 0.08f, 0.07f));
		AddProp(Cyl, Head, FVector(0, 1, 19), FRotator::ZeroRotator, FVector(0.215f, 0.215f, 0.025f), Accent, 2.f);
		break;
	case EDCCriminalType::Safecracker: // toolbag
		AddProp(Cube, TEXT("hand_r"), FVector(0, 0, -16), FRotator::ZeroRotator, FVector(0.14f, 0.32f, 0.2f), FLinearColor(0.18f, 0.1f, 0.05f));
		AddProp(Cube, TEXT("hand_r"), FVector(0, 0, -5), FRotator::ZeroRotator, FVector(0.145f, 0.33f, 0.02f), Accent, 1.5f);
		break;
	default: break;
	}
	Bandage = AddProp(Cyl, Head, FVector(0, 0, 8), FRotator(-8, 0, 0), FVector(0.22f, 0.24f, 0.045f), FLinearColor(0.8f, 0.78f, 0.72f));
	if (Bandage) { Bandage->SetVisibility(false); }
}

void ADCCriminalPawn::RefreshPlate()
{
	FDCLabelSpec S;
	S.Text = ShortName;
	S.Sub = FString::Printf(TEXT("%d"), Level);
	S.Size = 10.f;
	S.Icon = bInjured ? FName(TEXT("bandage")) : bLying ? FName(TEXT("moon")) : DC::TypeIcon(Type);
	S.Accent = bInjured ? FLinearColor(1.f, 0.32f, 0.28f) : bLying ? FLinearColor(0.55f, 0.65f, 1.f) : DC::TypeColor(Type);
	S.Opacity = bLying ? 0.55f : 0.9f;
	UDCWorldLabel::SetComponentSpec(Plate, S);
}

void ADCCriminalPawn::UpdateSpeed()
{
	const bool bHurry = Mode == EDCPawnMode::WalkOut || Mode == EDCPawnMode::WalkIn;
	GetCharacterMovement()->MaxFlySpeed = bInjured ? (bHurry ? HurryLimpSpeed : LimpSpeed) : (bHurry ? HurrySpeed : WalkSpeed);
}

void ADCCriminalPawn::SetHidden(bool bHide)
{
	SetActorHiddenInGame(bHide);
	if (Plate) { Plate->SetVisibility(!bHide); }
	if (Bandage) { Bandage->SetVisibility(!bHide && bInjured); }
	if (bHide)
	{
		GetCharacterMovement()->StopMovementImmediately();
		if (LevelRing) { LevelRing->SetVisibility(false); }
		if (LevelLight) { LevelLight->SetVisibility(false); }
	}
}

FVector ADCCriminalPawn::Ground(const FVector& P) const
{
	return FVector(P.X, P.Y, P.Z + HalfHeight);
}

FVector ADCCriminalPawn::GetFeet() const
{
	return GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
}

void ADCCriminalPawn::PlaceAt(const FVector& FloorPoint)
{
	GetUp();
	SetActorLocation(Ground(FloorPoint));
	SetActorRotation(FRotator(0.f, Rng.FRandRange(-180.f, 180.f), 0.f));
	Mode = EDCPawnMode::Wander;
	Activity = EDCPawnActivity::Stand;
	Path.Reset();
	StartDelay = 0.f;
	WaitTime = Rng.FRandRange(0.5f, 3.f);
	SetHidden(false);
}

void ADCCriminalPawn::SetAway()
{
	GetUp();
	Mode = EDCPawnMode::Away;
	Path.Reset();
	StartDelay = 0.f;
	SetHidden(true);
}

void ADCCriminalPawn::SetIdleGoal(const TArray<FVector>& FloorPath, EDCPawnActivity InActivity, float FaceYaw, float Hold)
{
	if (Mode != EDCPawnMode::Wander) { return; }
	GetUp();
	Activity = InActivity;
	Path.Reset();
	for (const FVector& P : FloorPath) { Path.Add(Ground(P)); }
	bHasGoalYaw = !FMath::IsNaN(FaceYaw);
	GoalYaw = bHasGoalYaw ? FaceYaw : 0.f;
	GoalHold = Hold;
	WaitTime = 0.f;
	StuckTime = 0.f;
	if (Path.Num() == 0) { ArriveIdle(); }
}

void ADCCriminalPawn::InterruptIdle(float Delay)
{
	if (Mode != EDCPawnMode::Wander) { return; }
	// A sleeper stays down until the delay runs out (staggered wake-up), then gets up and asks for a goal.
	Activity = EDCPawnActivity::Stand;
	Path.Reset();
	WaitTime = Delay;
}

void ADCCriminalPawn::LieDown()
{
	if (bLying) { return; }
	bLying = true;
	// Lie on the back, head towards actor -X, face up: tip the (idle-animating) mesh over around the actor's Y axis.
	if (USkeletalMeshComponent* M = GetMesh())
	{
		const FQuat Lying = FQuat(FRotator(90.f, 0.f, 0.f)) * FQuat(FRotator(0.f, -90.f, 0.f));
		M->SetRelativeLocationAndRotation(FVector(88.f, 0.f, -HalfHeight + 14.f), Lying);
	}
	if (Plate) { Plate->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight + 60.f)); }
	if (Bandage) { Bandage->SetVisibility(false); }
	RefreshPlate();
}

void ADCCriminalPawn::GetUp()
{
	if (!bLying) { return; }
	bLying = false;
	if (USkeletalMeshComponent* M = GetMesh())
	{
		M->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -HalfHeight), FRotator(0.f, -90.f, 0.f));
	}
	if (Plate) { Plate->SetRelativeLocation(FVector(0.f, 0.f, HalfHeight + 22.f)); }
	if (Bandage) { Bandage->SetVisibility(bInjured && !IsHidden()); }
	RefreshPlate();
}

void ADCCriminalPawn::Depart(ADCGarageDoor* Door, float Delay, bool bForever)
{
	bLeaveForever |= bForever;
	bReturnQueued = false;
	if (Mode == EDCPawnMode::Away || (Mode == EDCPawnMode::WalkIn && IsHidden()))
	{
		// Not visible yet (still outside): just stay away.
		SetAway();
		if (bLeaveForever) { bWantsDestroy = true; }
		return;
	}
	GetUp();
	Mode = EDCPawnMode::WalkOut;
	Activity = EDCPawnActivity::Stand;
	ActiveDoor = Door;
	StartDelay = Delay;
	StuckTime = 0.f;
	Path.Reset();
	if (Door)
	{
		TArray<FVector> Route;
		if (ADCWorldDirector* D = Director.Get()) { D->PlanPath(GetFeet(), Door->GetEntryLocation(), Route); }
		else { Route.Add(Door->GetEntryLocation()); }
		Route.Add(Door->GetActorLocation());
		Route.Add(Door->GetExitLocation());
		for (const FVector& P : Route) { Path.Add(Ground(P)); }
	}
	UpdateSpeed();
}

void ADCCriminalPawn::ReturnHome(ADCGarageDoor* Door, float Delay)
{
	if (bLeaveForever) { return; }
	if (Mode == EDCPawnMode::WalkOut)
	{
		bReturnQueued = true;
		QueuedReturnDelay = Delay;
		ActiveDoor = Door ? Door : ActiveDoor.Get();
		return;
	}
	if (Mode != EDCPawnMode::Away) { return; }
	StartDelay = Delay;
	BeginWalkIn(Door);
}

void ADCCriminalPawn::BeginWalkIn(ADCGarageDoor* Door)
{
	ADCWorldDirector* D = Director.Get();
	ActiveDoor = Door;
	Mode = EDCPawnMode::WalkIn;
	Activity = EDCPawnActivity::Stand;
	bHasGoalYaw = false;
	bReturnQueued = false;
	StuckTime = 0.f;
	Path.Reset();
	if (!Door)
	{
		const FVector P = D ? D->PickSpawnPoint(this) : GetFeet();
		const float Delay = StartDelay;
		PlaceAt(P);
		StartDelay = Delay;
		return;
	}
	const FVector Exit = Door->GetExitLocation();
	SetActorLocation(Ground(Exit));
	SetActorRotation((Door->GetActorLocation() - Exit).GetSafeNormal2D().Rotation());
	Path.Add(Ground(Door->GetActorLocation()));
	Path.Add(Ground(Door->GetEntryLocation()));
	if (D)
	{
		TArray<FVector> Route;
		D->PlanPath(Door->GetEntryLocation(), D->PickSpawnPoint(this), Route);
		for (const FVector& P : Route) { Path.Add(Ground(P)); }
	}
	UpdateSpeed();
	// Stay hidden until StartDelay elapses (Tick reveals us).
}

void ADCCriminalPawn::WalkOutAlong(const TArray<FVector>& FloorPath, float Delay, bool bForever)
{
	bLeaveForever |= bForever;
	bReturnQueued = false;
	if (Mode == EDCPawnMode::Away || (Mode == EDCPawnMode::WalkIn && IsHidden()))
	{
		SetAway();
		if (bLeaveForever) { bWantsDestroy = true; }
		return;
	}
	GetUp();
	Mode = EDCPawnMode::WalkOut;
	Activity = EDCPawnActivity::Stand;
	ActiveDoor = nullptr;
	bHasGoalYaw = false;
	StartDelay = Delay;
	StuckTime = 0.f;
	Path.Reset();
	for (const FVector& P : FloorPath) { Path.Add(Ground(P)); }
	UpdateSpeed();
}

void ADCCriminalPawn::WalkInAlong(const FVector& Start, const TArray<FVector>& FloorPath, float Delay)
{
	if (bLeaveForever) { return; }
	const bool bWasAway = Mode == EDCPawnMode::Away || IsHidden();
	GetUp();
	Mode = EDCPawnMode::WalkIn;
	Activity = EDCPawnActivity::Stand;
	ActiveDoor = nullptr;
	bHasGoalYaw = false;
	bReturnQueued = false;
	StuckTime = 0.f;
	Path.Reset();
	for (const FVector& P : FloorPath) { Path.Add(Ground(P)); }
	if (bWasAway)
	{
		SetActorLocation(Ground(Start));
		const FVector Dir = Path.Num() ? (Path[0] - GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
		if (!Dir.IsNearlyZero()) { SetActorRotation(Dir.Rotation()); }
		SetHidden(true);
		StartDelay = FMath::Max(0.01f, Delay); // Tick reveals us when the delay runs out
	}
	else { StartDelay = Delay; }
	UpdateSpeed();
}

void ADCCriminalPawn::CancelWalkOut()
{
	if (Mode != EDCPawnMode::WalkOut) { return; }
	Mode = EDCPawnMode::Wander;
	Activity = EDCPawnActivity::Stand;
	ActiveDoor = nullptr;
	bReturnQueued = false;
	Path.Reset();
	StartDelay = 0.f;
	WaitTime = Rng.FRandRange(0.2f, 0.8f);
	GetCharacterMovement()->StopMovementImmediately();
	UpdateSpeed();
}

bool ADCCriminalPawn::IsNearDoor(float Radius) const
{
	const ADCGarageDoor* D = ActiveDoor.Get();
	if (!D || IsHidden() || StartDelay > 0.f) { return false; }
	if (Mode != EDCPawnMode::WalkOut && Mode != EDCPawnMode::WalkIn) { return false; }
	return FVector::Dist2D(GetActorLocation(), D->GetActorLocation()) < Radius;
}

void ADCCriminalPawn::PlayLevelUp(int32 NewLevel)
{
	Level = FMath::Max(Level, NewLevel);
	if (Mode == EDCPawnMode::Away || Mode == EDCPawnMode::WalkOut || IsHidden())
	{
		bLevelUpPending = true;
		return;
	}
	bLevelUpPending = false;
	LevelUpTime = 0.f;
	if (LevelRing) { LevelRing->SetVisibility(true); }
	if (LevelLight) { LevelLight->SetVisibility(true); }
	FDCLabelSpec S;
	S.Icon = TEXT("chevrons-up");
	S.Text = ShortName;
	S.Sub = FString::Printf(TEXT("Lv %d"), Level);
	S.Accent = FLinearColor(1.f, 0.78f, 0.25f);
	S.bAccentOutline = true;
	S.Size = 12.f;
	UDCWorldLabel::SetComponentSpec(Plate, S);
}

void ADCCriminalPawn::TickLevelUp(float Dt)
{
	if (LevelUpTime < 0.f) { return; }
	LevelUpTime += Dt;
	const float A = FMath::Clamp(LevelUpTime / LevelUpLength, 0.f, 1.f);
	const float E = 1.f - FMath::Pow(1.f - A, 3.f);
	if (LevelRing)
	{
		const float R = FMath::Lerp(0.5f, 2.8f, E);
		LevelRing->SetRelativeScale3D(FVector(R, R, 0.015f));
		LevelRing->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight + 2.f + 60.f * E));
		if (LevelRingMID) { LevelRingMID->SetScalarParameterValue(TEXT("Strength"), 30.f * (1.f - A)); }
		if (A >= 1.f) { LevelRing->SetVisibility(false); }
	}
	if (LevelLight)
	{
		LevelLight->SetIntensity(12000.f * FMath::Sin(A * UE_PI));
		if (A >= 1.f) { LevelLight->SetVisibility(false); }
	}
	if (LevelUpTime > 2.6f)
	{
		LevelUpTime = -1.f;
		RefreshPlate();
	}
}

void ADCCriminalPawn::ArriveIdle()
{
	if (bHasGoalYaw) { SetActorRotation(FRotator(0.f, GoalYaw, 0.f)); }
	if (Activity == EDCPawnActivity::Sleep)
	{
		LieDown();
		WaitTime = 1e9f; // until the director wakes us
		return;
	}
	WaitTime = FMath::Max(0.5f, GoalHold);
}

void ADCCriminalPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bInitialized) { return; }
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	TickLevelUp(Dt);

	if (Mode == EDCPawnMode::Away) { return; }
	if (StartDelay > 0.f)
	{
		StartDelay -= Dt;
		if (StartDelay > 0.f) { return; }
	}
	if (Mode == EDCPawnMode::WalkIn && IsHidden()) { SetHidden(false); }

	if (Path.Num() == 0)
	{
		switch (Mode)
		{
		case EDCPawnMode::Wander:
			if (WaitTime > 0.f) { WaitTime -= Dt; }
			if (bHasGoalYaw && !bLying)
			{
				const FRotator R = GetActorRotation();
				SetActorRotation(FMath::RInterpTo(R, FRotator(0.f, GoalYaw, 0.f), Dt, 5.f));
			}
			if (WaitTime <= 0.f && (!bLying || Activity != EDCPawnActivity::Sleep))
			{
				GetUp();
				if (ADCWorldDirector* D = Director.Get()) { D->AssignIdleGoal(this); }
				if (Path.Num() == 0 && WaitTime <= 0.f) { WaitTime = Rng.FRandRange(2.f, 5.f); }
			}
			break;
		case EDCPawnMode::WalkOut:
			SetHidden(true);
			Mode = EDCPawnMode::Away;
			if (bLeaveForever) { bWantsDestroy = true; }
			else if (bReturnQueued) { StartDelay = QueuedReturnDelay + 1.2f; BeginWalkIn(ActiveDoor.Get()); }
			break;
		case EDCPawnMode::WalkIn:
			Mode = EDCPawnMode::Wander;
			Activity = EDCPawnActivity::Stand;
			bHasGoalYaw = false;
			WaitTime = Rng.FRandRange(1.f, 3.f);
			ActiveDoor = nullptr;
			UpdateSpeed();
			if (bLevelUpPending) { PlayLevelUp(Level); }
			break;
		default: break;
		}
		return;
	}

	UpdateSpeed();
	const FVector Loc = GetActorLocation();
	FVector To = Path[0] - Loc;
	To.Z = 0.f;
	const float Dist = To.Size();
	StuckTime += Dt;
	if (Dist < 22.f || StuckTime > 12.f)
	{
		if (StuckTime > 12.f || Path.Num() == 1) { SetActorLocation(Path[0]); }
		Path.RemoveAt(0);
		StuckTime = 0.f;
		if (Path.Num() == 0 && Mode == EDCPawnMode::Wander)
		{
			GetCharacterMovement()->StopMovementImmediately();
			ArriveIdle();
		}
		return;
	}
	float Scale = 1.f;
	if (bInjured)
	{
		LimpPhase += Dt * 7.f;
		Scale = 0.55f + 0.45f * FMath::Abs(FMath::Sin(LimpPhase));
	}
	// Slow down for the final approach so braking doesn't overshoot.
	if (Path.Num() == 1) { Scale *= FMath::Clamp(Dist / 60.f, 0.35f, 1.f); }
	AddMovementInput(To / Dist, Scale);
}
