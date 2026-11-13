#include "World/DCStation.h"
#include "World/DCWorldLabel.h"
#include "World/DCPlayerController.h"
#include "World/DCCameraRig.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Core/DCUISubsystem.h"
#include "Core/DCRunSubsystem.h"
#include "Core/DCAssetPaths.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DirectorsCut.h"

ADCStation::ADCStation()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	ClickBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ClickBox"));
	ClickBox->SetBoxExtent(FVector(80, 80, 80));
	ClickBox->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	ClickBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClickBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ClickBox->SetHiddenInGame(true);
	SetRootComponent(ClickBox);

	Label = CreateDefaultSubobject<UWidgetComponent>(TEXT("Label"));
	Label->SetupAttachment(ClickBox);
	Label->SetWidgetSpace(EWidgetSpace::Screen);
	Label->SetWidgetClass(UDCWorldLabel::StaticClass());
	Label->SetDrawAtDesiredSize(true);
	Label->SetPivot(FVector2D(0.5f, 1.f));
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetGenerateOverlapEvents(false);
	Label->SetUsingAbsoluteScale(true);
	Label->SetVisibility(false);
}

void ADCStation::BeginPlay()
{
	Super::BeginPlay();
	UpdateLabel();
}

void ADCStation::SetClickExtent(const FVector& HalfExtent)
{
	ClickBox->SetBoxExtent(HalfExtent);
	UpdateLabel();
}

void ADCStation::UpdateLabel()
{
	if (!Label) { return; }
	const FVector E = ClickBox->GetUnscaledBoxExtent();
	Label->SetRelativeLocation(FVector(0.f, 0.f, E.Z + 40.f));
	FString Name = DisplayName.IsEmpty() ? StaticEnum<EDCStationType>()->GetDisplayNameTextByValue((int64)StationType).ToString() : DisplayName;
	UDCWorldLabel::SetComponentLabel(Label, Name.ToUpper(), FLinearColor(1.f, 0.86f, 0.6f), 13);
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
	if (!bEnabled && bHovered) { OnHoverEnd(); }
}

UMaterialInterface* ADCStation::GetHighlightMaterial()
{
	if (HighlightMID) { return HighlightMID; }
	if (UMaterialInterface* H = LoadObject<UMaterialInterface>(nullptr, DCPaths::MatHighlight, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		HighlightMID = UMaterialInstanceDynamic::Create(H, this);
		HighlightMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.72f, 0.35f));
		HighlightParam = TEXT("Strength");
	}
	else if (UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, DCPaths::MatGlass, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		HighlightMID = UMaterialInstanceDynamic::Create(G, this);
		HighlightMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.72f, 0.35f));
		HighlightMID->SetScalarParameterValue(TEXT("Opacity"), 0.22f);
		HighlightParam = TEXT("Opacity");
	}
	return HighlightMID;
}

void ADCStation::OnHoverBegin()
{
	if (bHovered) { return; }
	bHovered = true;
	HoverTime = 0.f;
	UMaterialInterface* Mat = GetHighlightMaterial();
	for (UPrimitiveComponent* C : HighlightComponents)
	{
		if (!C) { continue; }
		C->SetRenderCustomDepth(true);
		C->SetCustomDepthStencilValue(1);
		if (UMeshComponent* MC = Cast<UMeshComponent>(C))
		{
			if (Mat) { MC->SetOverlayMaterial(Mat); }
		}
	}
	UpdateLabel();
	if (Label) { Label->SetVisibility(true); }
	SetActorTickEnabled(true);
}

void ADCStation::OnHoverEnd()
{
	if (!bHovered) { return; }
	bHovered = false;
	for (UPrimitiveComponent* C : HighlightComponents)
	{
		if (!C) { continue; }
		C->SetRenderCustomDepth(false);
		if (UMeshComponent* MC = Cast<UMeshComponent>(C)) { MC->SetOverlayMaterial(nullptr); }
	}
	if (Label) { Label->SetVisibility(false); }
	SetActorTickEnabled(false);
}

void ADCStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	HoverTime += DeltaSeconds;
	if (HighlightMID && HighlightParam != NAME_None)
	{
		// Gentle pulse while hovered.
		const float P = 0.5f + 0.5f * FMath::Sin(HoverTime * 5.f);
		const float V = HighlightParam == TEXT("Opacity") ? FMath::Lerp(0.14f, 0.30f, P) : FMath::Lerp(0.7f, 1.4f, P);
		HighlightMID->SetScalarParameterValue(HighlightParam, V);
	}
}

EDCPanel ADCStation::GetPanel() const
{
	switch (StationType)
	{
	case EDCStationType::DirectorDesk:   return EDCPanel::Desk;
	case EDCStationType::JobBoard:       return EDCPanel::JobBoard;
	case EDCStationType::Lounge:         return EDCPanel::Roster;
	case EDCStationType::Workbench:      return EDCPanel::Workbench;
	case EDCStationType::GarageDoor:     return EDCPanel::JobBoard;
	case EDCStationType::Projector:      return EDCPanel::Settings;
	case EDCStationType::BlueprintTable: return EDCPanel::Blueprint;
	case EDCStationType::ScreeningRoom:  return EDCPanel::ScreeningRoom;
	case EDCStationType::Clinic:         return EDCPanel::Clinic;
	case EDCStationType::ServerCloset:   return EDCPanel::ServerCloset;
	case EDCStationType::Lawyer:         return EDCPanel::Lawyer;
	}
	return EDCPanel::None;
}

void ADCStation::OpenStationPanel()
{
	UDCUISubsystem* UI = UDCUISubsystem::Get(this);
	if (!UI) { return; }
	UI->OpenPanel(GetPanel());
	if (StationType == EDCStationType::GarageDoor)
	{
		if (const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this))
		{
			const int32 N = Run->GetCommittedPlans().Num();
			if (N > 0)
			{
				UI->ShowToast(FString::Printf(TEXT("%d crew%s waiting by the car. Press SPACE to Roll Camera."), N, N == 1 ? TEXT("") : TEXT("s")));
			}
		}
	}
}

void ADCStation::OnClicked()
{
	if (!bStationEnabled) { return; }
	UE_LOG(LogDC, Log, TEXT("Station clicked: %s"), *DisplayName);

	// Camera dollies in first, then the panel opens.
	ADCPlayerController* PC = GetWorld() ? Cast<ADCPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
	if (PC) { PC->NotifyStationFocused(this); }
	if (ADCCameraRig* Rig = PC ? PC->GetCameraRig() : nullptr)
	{
		Rig->FocusOn(GetFocusPoint(), FocusZoomAlpha);
	}
	if (PanelOpenDelay > 0.f && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(OpenTimer, this, &ADCStation::OpenStationPanel, PanelOpenDelay, false);
	}
	else
	{
		OpenStationPanel();
	}
}
