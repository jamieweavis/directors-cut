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

FName ADCStation::TypeIcon(EDCStationType Type)
{
	switch (Type)
	{
	case EDCStationType::Desk:         return TEXT("user-plus");
	case EDCStationType::MissionBoard: return TEXT("map-pinned");
	case EDCStationType::Lounge:       return TEXT("users");
	case EDCStationType::Workbench:    return TEXT("wrench");
	case EDCStationType::GarageDoor:   return TEXT("backpack");
	case EDCStationType::BuildTable:   return TEXT("hammer");
	case EDCStationType::OpsRoom:      return TEXT("monitor");
	case EDCStationType::Clinic:       return TEXT("cross");
	case EDCStationType::ServerRack:   return TEXT("server");
	case EDCStationType::Lawyer:       return TEXT("scale");
	}
	return TEXT("circle");
}

FString ADCStation::TypeName(EDCStationType Type)
{
	switch (Type)
	{
	case EDCStationType::Desk:         return TEXT("Recruits");
	case EDCStationType::MissionBoard: return TEXT("Missions");
	case EDCStationType::Lounge:       return TEXT("Crew");
	case EDCStationType::Workbench:    return TEXT("Gear");
	case EDCStationType::GarageDoor:   return TEXT("Kit Room");
	case EDCStationType::BuildTable:   return TEXT("Build");
	case EDCStationType::OpsRoom:      return TEXT("Ops");
	case EDCStationType::Clinic:       return TEXT("Clinic");
	case EDCStationType::ServerRack:   return TEXT("Servers");
	case EDCStationType::Lawyer:       return TEXT("Lawyer");
	}
	return TEXT("");
}

EDCRoomType ADCStation::GetRequiredRoom() const
{
	switch (StationType)
	{
	case EDCStationType::Lounge:     return EDCRoomType::Lounge;
	case EDCStationType::Workbench:  return EDCRoomType::Workshop;
	case EDCStationType::GarageDoor: return EDCRoomType::Garage;
	case EDCStationType::OpsRoom:    return EDCRoomType::OpsRoom;
	case EDCStationType::Clinic:     return EDCRoomType::Clinic;
	case EDCStationType::ServerRack: return EDCRoomType::ServerRoom;
	case EDCStationType::Lawyer:     return EDCRoomType::LawyerOffice;
	default:                         return OwningRoom;
	}
}

void ADCStation::SetBadge(FName Icon, const FString& Text, const FLinearColor& Accent)
{
	if (BadgeIcon == Icon && BadgeText == Text && BadgeAccent == Accent) { return; }
	BadgeIcon = Icon; BadgeText = Text; BadgeAccent = Accent;
	UpdateLabel();
}

void ADCStation::UpdateLabel()
{
	if (!Label) { return; }
	const FVector E = ClickBox->GetUnscaledBoxExtent();
	Label->SetRelativeLocation(FVector(0.f, 0.f, E.Z + 30.f));
	const bool bBadge = !BadgeText.IsEmpty() && bStationEnabled;
	FDCLabelSpec S;
	S.Size = 12.f;
	if (bHovered)
	{
		S.Icon = TypeIcon(StationType);
		S.Text = TypeName(StationType);
		S.Sub = bBadge ? BadgeText : FString();
		S.Accent = bBadge ? BadgeAccent : FLinearColor(0.98f, 0.72f, 0.30f);
		S.bAccentOutline = true;
	}
	else if (bBadge)
	{
		S.Icon = BadgeIcon.IsNone() ? TypeIcon(StationType) : BadgeIcon;
		S.Sub = BadgeText;
		S.Accent = BadgeAccent;
		S.Size = 11.f;
	}
	UDCWorldLabel::SetComponentSpec(Label, S);
	Label->SetVisibility(bHovered || bBadge);
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
	if (bStationEnabled == bEnabled && HasActorBegunPlay()) { return; }
	bStationEnabled = bEnabled;
	ClickBox->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (!bEnabled && bHovered) { OnHoverEnd(); }
	UpdateLabel();
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
	UpdateLabel();
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
	case EDCStationType::Desk:   return EDCPanel::Recruits;
	case EDCStationType::MissionBoard:       return EDCPanel::Missions;
	case EDCStationType::Lounge:         return EDCPanel::Crew;
	case EDCStationType::Workbench:      return EDCPanel::Workbench;
	case EDCStationType::GarageDoor:
	{
		// Crews come back here: unread results first, otherwise the board (with the missions in progress).
		const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
		return (Run && Run->GetUnseenReportCount() > 0) ? EDCPanel::Reports : EDCPanel::Missions;
	}
	case EDCStationType::BuildTable: return EDCPanel::Build;
	case EDCStationType::OpsRoom:  return EDCPanel::OpsRoom;
	case EDCStationType::Clinic:         return EDCPanel::Clinic;
	case EDCStationType::ServerRack:   return EDCPanel::ServerRoom;
	case EDCStationType::Lawyer:         return EDCPanel::Lawyer;
	}
	return EDCPanel::None;
}

void ADCStation::OpenStationPanel()
{
	UDCUISubsystem* UI = UDCUISubsystem::Get(this);
	if (!UI) { return; }
	UI->OpenPanel(GetPanel());
}

void ADCStation::OnClicked()
{
	if (!bStationEnabled) { return; }
	UE_LOG(LogDC, Log, TEXT("Station clicked: %s (%s)"), *TypeName(StationType), *DisplayName);

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
