#include "World/DCStation.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/DCUISubsystem.h"
#include "DirectorsCut.h"

ADCStation::ADCStation()
{
	PrimaryActorTick.bCanEverTick = false;
	ClickBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ClickBox"));
	ClickBox->SetBoxExtent(FVector(80, 80, 80));
	ClickBox->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	ClickBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClickBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ClickBox->SetHiddenInGame(true);
	SetRootComponent(ClickBox);
}

void ADCStation::SetClickExtent(const FVector& HalfExtent)
{
	ClickBox->SetBoxExtent(HalfExtent);
}

UStaticMeshComponent* ADCStation::AddPropMesh(UStaticMesh* Mesh, UMaterialInterface* Material, const FTransform& RelativeTransform)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	if (Material) { C->SetMaterial(0, Material); }
	C->SetupAttachment(ClickBox);
	C->SetRelativeTransform(RelativeTransform);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->RegisterComponent();
	AddInstanceComponent(C);
	RegisterHighlightComponent(C);
	return C;
}

void ADCStation::RegisterHighlightComponent(UPrimitiveComponent* Comp)
{
	if (Comp) { HighlightComponents.AddUnique(Comp); }
}

void ADCStation::SetStationEnabled(bool bEnabled)
{
	bStationEnabled = bEnabled;
	ClickBox->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void ADCStation::OnHoverBegin()
{
	bHovered = true;
	for (UPrimitiveComponent* C : HighlightComponents)
	{
		if (C) { C->SetRenderCustomDepth(true); C->SetCustomDepthStencilValue(1); }
	}
}

void ADCStation::OnHoverEnd()
{
	bHovered = false;
	for (UPrimitiveComponent* C : HighlightComponents)
	{
		if (C) { C->SetRenderCustomDepth(false); }
	}
}

void ADCStation::OnClicked()
{
	if (!bStationEnabled) { return; }
	UE_LOG(LogDC, Log, TEXT("Station clicked: %s"), *DisplayName);
	if (UDCUISubsystem* UI = UDCUISubsystem::Get(this))
	{
		EDCPanel Panel = EDCPanel::None;
		switch (StationType)
		{
		case EDCStationType::DirectorDesk:   Panel = EDCPanel::Desk; break;
		case EDCStationType::JobBoard:       Panel = EDCPanel::JobBoard; break;
		case EDCStationType::Lounge:         Panel = EDCPanel::Roster; break;
		case EDCStationType::Workbench:      Panel = EDCPanel::Workbench; break;
		case EDCStationType::GarageDoor:     Panel = EDCPanel::JobBoard; break;
		case EDCStationType::Projector:      Panel = EDCPanel::Settings; break;
		case EDCStationType::BlueprintTable: Panel = EDCPanel::Blueprint; break;
		case EDCStationType::ScreeningRoom:  Panel = EDCPanel::ScreeningRoom; break;
		case EDCStationType::Clinic:         Panel = EDCPanel::Clinic; break;
		case EDCStationType::ServerCloset:   Panel = EDCPanel::ServerCloset; break;
		case EDCStationType::Lawyer:         Panel = EDCPanel::Lawyer; break;
		}
		UI->OpenPanel(Panel);
	}
}
