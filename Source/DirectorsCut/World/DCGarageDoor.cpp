#include "World/DCGarageDoor.h"
#include "Core/DCAssetPaths.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

ADCGarageDoor::ADCGarageDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	DoorPanel = CreateDefaultSubobject<USceneComponent>(TEXT("DoorPanel"));
	DoorPanel->SetupAttachment(Root);
}

void ADCGarageDoor::BeginPlay()
{
	Super::BeginPlay();
	if (RoomType != EDCRoomType::Garage && bRollUp)
	{
		// Street doors swing by default (the Garage rolls up).
		bRollUp = false;
	}
	if (bFallbackPanel && DoorPanel && DoorPanel->GetNumChildrenComponents() == 0)
	{
		if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, DCPaths::Cube))
		{
			// Door plane spans local Y, crews exit along +X. A swing door hinges at its -Y edge.
			const float Width = bRollUp ? 300.f : 110.f;
			const float Height = bRollUp ? 240.f : 220.f;
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, TEXT("FallbackPanel"));
			C->SetStaticMesh(Cube);
			C->SetupAttachment(DoorPanel);
			C->SetRelativeLocation(FVector(0.f, bRollUp ? 0.f : Width * 0.5f, Height * 0.5f));
			C->SetRelativeScale3D(FVector(0.08f, Width / 100.f, Height / 100.f));
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, DCPaths::MatSurface);
			const bool bSurface = Base != nullptr;
			if (!Base) { Base = LoadObject<UMaterialInterface>(nullptr, DCPaths::BasicShapeMaterial); }
			if (Base)
			{
				UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
				const FLinearColor Col = bRollUp ? FLinearColor(0.10f, 0.11f, 0.12f) : FLinearColor(0.14f, 0.07f, 0.04f);
				M->SetVectorParameterValue(bSurface ? TEXT("BaseColor") : TEXT("Color"), Col);
				if (bSurface) { M->SetScalarParameterValue(TEXT("Metallic"), bRollUp ? 0.6f : 0.f); M->SetScalarParameterValue(TEXT("Roughness"), 0.5f); }
				C->SetMaterial(0, M);
			}
			C->RegisterComponent();
			AddInstanceComponent(C);
			if (!bRollUp)
			{
				// Hinge sits at the panel's edge.
				DoorPanel->SetRelativeLocation(DoorPanel->GetRelativeLocation() + FVector(0.f, -Width * 0.5f, 0.f));
			}
		}
	}
	PanelClosed = DoorPanel->GetRelativeTransform();
	bCapturedClosed = true;
}

void ADCGarageDoor::Open()
{
	if (bWantOpen) { return; }
	bWantOpen = true;
	SetActorTickEnabled(true);
}

void ADCGarageDoor::Close()
{
	if (!bWantOpen) { return; }
	bWantOpen = false;
	SetActorTickEnabled(true);
}

void ADCGarageDoor::ApplyAlpha()
{
	if (!DoorPanel || !bCapturedClosed) { return; }
	// Ease in-out with a tiny overshoot on open.
	const float A = FMath::InterpEaseInOut(0.f, 1.f, OpenAlpha, 2.2f);
	FTransform T = PanelClosed;
	if (bRollUp)
	{
		T.SetLocation(PanelClosed.GetLocation() + FVector(0.f, 0.f, RollUpHeight * A));
		const FVector S = PanelClosed.GetScale3D();
		T.SetScale3D(FVector(S.X, S.Y, S.Z * FMath::Lerp(1.f, 0.12f, A)));
	}
	else
	{
		T.SetRotation((PanelClosed.GetRotation().Rotator() + FRotator(0.f, -SwingAngle * A, 0.f)).Quaternion());
	}
	DoorPanel->SetRelativeTransform(T);
}

void ADCGarageDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Step = DeltaSeconds / FMath::Max(0.05f, OpenSeconds);
	OpenAlpha = FMath::Clamp(OpenAlpha + (bWantOpen ? Step : -Step), 0.f, 1.f);
	ApplyAlpha();
	if ((bWantOpen && OpenAlpha >= 1.f) || (!bWantOpen && OpenAlpha <= 0.f))
	{
		SetActorTickEnabled(false);
	}
}
