// Director's Cut — build placement mode. See DCBuildPlacer.h.
// Dev: -DCPlaceRoom=Workshop starts placement once a run is up; -DCPlaceCursor=X,Y fixes the cursor's ground point.
#include "World/DCBuildPlacer.h"
#include "World/DCWorldLabel.h"
#include "Core/DCSiteLayout.h"
#include "Core/DCRunSubsystem.h"
#include "Core/DCUISubsystem.h"
#include "Core/DCAudioSubsystem.h"
#include "Core/DCAssetPaths.h"
#include "DirectorsCut.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	using namespace DCSite;
	constexpr float NearR = 260.f;    // full-size grid lines within this distance of the cursor...
	constexpr float FarR = 1250.f;    // ...shrinking to nothing out here
	constexpr float LineZ = 5.f;
	const FLinearColor AccentCol(0.30f, 0.85f, 1.f);
	const FLinearColor BadCol(1.f, 0.16f, 0.1f);

	float Falloff(float D) { const float A = FMath::Clamp((D - NearR) / (FarR - NearR), 0.f, 1.f); return 1.f - A * A * (3.f - 2.f * A); }
	float DistToSegment(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const float T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / FMath::Max(1.f, AB.SizeSquared()), 0.f, 1.f);
		return FVector2D::Distance(P, A + AB * T);
	}
	/** Box instance between two ground points. */
	FTransform BarXform(const FVector2D& A, const FVector2D& B, float Z, float W, float H, float Shrink = 0.f)
	{
		const FVector2D D = B - A;
		const float L = FMath::Max(1.f, D.Size() - Shrink);
		const FVector2D M = (A + B) * 0.5f;
		return FTransform(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), 0.f), FVector(M.X, M.Y, Z), FVector(L / 100.f, W / 100.f, H / 100.f));
	}
}

ADCBuildPlacer::ADCBuildPlacer()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

UMaterialInstanceDynamic* ADCBuildPlacer::MakeGlow(const FLinearColor& Color, float Strength)
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DC/Materials/M_DC_Neon.M_DC_Neon"));
	if (!Base) { Base = LoadObject<UMaterialInterface>(nullptr, DCPaths::MatEmissive); }
	if (!Base) { Base = LoadObject<UMaterialInterface>(nullptr, DCPaths::BasicShapeMaterial); }
	if (!Base) { return nullptr; }
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
	M->SetVectorParameterValue(TEXT("Color"), Color);
	M->SetScalarParameterValue(TEXT("Strength"), Strength);
	M->SetScalarParameterValue(TEXT("Flicker"), 0.f);
	return M;
}

UInstancedStaticMeshComponent* ADCBuildPlacer::MakeISM(const TCHAR* Name, const TCHAR* MeshPath, UMaterialInstanceDynamic* Mat)
{
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, Name);
	C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath));
	if (Mat) { C->SetMaterial(0, Mat); }
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCollisionResponseToAllChannels(ECR_Ignore);
	C->SetCastShadow(false);
	C->SetupAttachment(Root);
	C->RegisterComponent();
	AddInstanceComponent(C);
	return C;
}

void ADCBuildPlacer::Begin(EDCRoomType InRoom)
{
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || Run->IsRoomBuilt(InRoom)) { return; }
	if (!Lines)
	{
		LineMat = MakeGlow(FLinearColor(1.f, 0.93f, 0.8f), 9.f);
		GoodMat = MakeGlow(AccentCol, 14.f);
		BadMat = MakeGlow(BadCol, 12.f);
		Lines = MakeISM(TEXT("GridLines"), DCPaths::Cube, LineMat);
		Dots = MakeISM(TEXT("GridDots"), DCPaths::Cylinder, LineMat);
		Marks = MakeISM(TEXT("GridMarks"), DCPaths::Cube, GoodMat);
		HoverGood = MakeISM(TEXT("GridHoverGood"), DCPaths::Cube, GoodMat);
		HoverBad = MakeISM(TEXT("GridHoverBad"), DCPaths::Cube, BadMat);
		Label = UDCWorldLabel::CreateLabelComponent(this, Root, FVector::ZeroVector);
	}
	Room = InRoom;
	bActive = true;
	Appear = 0.f;
	bHaveCursor = false;
	LastCost = -1;
	RefreshCells();
	if (Valid.Num() == 0)
	{
		bActive = false;
		if (UDCUISubsystem* UI = UDCUISubsystem::Get(this)) { UI->ShowToast(TEXT("No room left to build on"), TEXT("ban"), true); }
		UDCAudioSubsystem::Play(this, DCSfx::Error);
		return;
	}
	if (UDCUISubsystem* UI = UDCUISubsystem::Get(this))
	{
		UI->ShowToast(FString::Printf(TEXT("Place the %s  ·  Esc to cancel"), *Run->GetRoom(Room).DisplayName), DC::RoomIcon(Room));
	}
	UE_LOG(LogDC, Log, TEXT("BuildPlacer: placing %s, %d valid cells"), DC::RoomName(Room), Valid.Num());
}

void ADCBuildPlacer::Cancel(bool bQuiet)
{
	if (!bActive) { return; }
	bActive = false;
	HideAll();
	UE_LOG(LogDC, Log, TEXT("BuildPlacer: cancelled%s"), bQuiet ? TEXT(" (quiet)") : TEXT(""));
	if (!bQuiet) { UDCAudioSubsystem::Play(this, DCSfx::PanelClose); }
}

void ADCBuildPlacer::HideAll()
{
	for (UInstancedStaticMeshComponent* C : { Lines.Get(), Dots.Get(), Marks.Get(), HoverGood.Get(), HoverBad.Get() }) { if (C) { C->ClearInstances(); } }
	if (Label) { Label->SetVisibility(false); }
}

void ADCBuildPlacer::RefreshCells()
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	Occupied = Run ? Run->GetOccupiedCells() : TArray<FIntPoint>{ Grid::OfficeCell() };
	Grid::ValidCells(Occupied, Valid);
}

void ADCBuildPlacer::UpdateCursor(APlayerController* PC, float DeltaTime)
{
	if (!bActive) { return; }
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	const FDCRoom R = Run ? Run->GetRoom(Room) : FDCRoom();
	if (!Run || !Run->HasActiveRun() || R.Level >= 1 || !R.bCanAct)
	{
		// Cash spent elsewhere, another build started, run ended...
		Cancel(true);
		if (Run && Run->HasActiveRun()) { if (UDCUISubsystem* UI = UDCUISubsystem::Get(this)) { UI->ShowToast(TEXT("Can't build that now"), TEXT("ban"), true); } }
		return;
	}
	Time += DeltaTime;
	Appear = FMath::Min(1.f, Appear + DeltaTime * 3.5f);
	LastCost = R.NextCost;
	RefreshCells();

	FVector O, D;
	bHaveCursor = false;
	static FString DevCursor = TEXT("?");
	if (DevCursor == TEXT("?")) { DevCursor.Reset(); FParse::Value(FCommandLine::Get(), TEXT("DCPlaceCursor="), DevCursor, false); }
	FString SX, SY;
	if (!DevCursor.IsEmpty() && DevCursor.Split(TEXT(","), &SX, &SY))
	{
		// dev screenshots: a fixed ground point instead of the mouse
		Cursor = FVector2D(FCString::Atof(*SX), FCString::Atof(*SY));
		bHaveCursor = true;
	}
	else if (PC && PC->DeprojectMousePositionToWorld(O, D) && D.Z < -0.01f)
	{
		const float T = (GroundZ - O.Z) / D.Z;
		const FVector P = O + D * T;
		Cursor = FVector2D(P.X, P.Y);
		bHaveCursor = true;
	}
	if (bHaveCursor)
	{
		const float K = 1.f - FMath::Exp(-DeltaTime * 14.f);
		Smooth = (Smooth.IsZero() ? Cursor : FMath::Lerp(Smooth, Cursor, K));
	}
	bHover = bHaveCursor && Grid::CellAt(Cursor, Hover);
	bHoverValid = bHover && Valid.Contains(Hover);
	Redraw();
}

void ADCBuildPlacer::Redraw()
{
	HideAll();
	if (!bHaveCursor) { return; }
	const FVector2D C = Smooth;

	// lattice lines + corner dots, sized by distance to the cursor
	TArray<FTransform> L, Dt;
	auto Edge = [&](const FVector2D& A, const FVector2D& B)
	{
		const float F = Falloff(DistToSegment(C, A, B)) * Appear;
		if (F < 0.03f) { return; }
		L.Add(BarXform(A, B, LineZ, 3.f + 7.f * F, 1.5f + 2.f * F, 10.f));
	};
	for (int32 J = Grid::MinRow; J <= Grid::MaxRow + 1; ++J)
	{
		for (int32 I = Grid::MinCol; I <= Grid::MaxCol + 1; ++I)
		{
			const FVector2D V = Grid::Vertex(I, J);
			if (I <= Grid::MaxCol) { Edge(V, Grid::Vertex(I + 1, J)); }
			if (J <= Grid::MaxRow) { Edge(V, Grid::Vertex(I, J + 1)); }
			const float F = Falloff(FVector2D::Distance(C, V)) * Appear;
			if (F >= 0.03f) { Dt.Add(FTransform(FRotator::ZeroRotator, FVector(V.X, V.Y, LineZ), FVector(0.14f * F + 0.04f, 0.14f * F + 0.04f, 0.04f))); }
		}
	}
	Lines->AddInstances(L, false, false, false);
	Dots->AddInstances(Dt, false, false, false);

	// a small "+" on every free cell where the room may go (bigger near the cursor)
	TArray<FTransform> Mk;
	const float Pulse = 0.85f + 0.15f * FMath::Sin(Time * 4.f);
	for (const FIntPoint& V : Valid)
	{
		if (bHoverValid && V == Hover) { continue; }
		const FVector2D P = Grid::Centre(V);
		const float F = FMath::Max(0.25f, Falloff(FVector2D::Distance(C, P) * 0.7f)) * Appear * Pulse;
		const float S = 45.f + 55.f * F;
		Mk.Add(FTransform(FRotator(0.f, 45.f, 0.f), FVector(P.X, P.Y, LineZ + 1.f), FVector(S / 100.f, 0.1f, 0.03f)));
		Mk.Add(FTransform(FRotator(0.f, -45.f, 0.f), FVector(P.X, P.Y, LineZ + 1.f), FVector(S / 100.f, 0.1f, 0.03f)));
	}
	Marks->AddInstances(Mk, false, false, false);

	// hovered cell: outline + corner posts (a ghost of the room's volume)
	if (bHover)
	{
		FVector2D Q[4]; Grid::Corners(Hover, Q);
		const FVector2D Ctr = Grid::Centre(Hover);
		const bool bTaken = Occupied.Contains(Hover);
		TArray<FTransform> Hv;
		const float Inset = 10.f;
		FVector2D QI[4];
		for (int32 k = 0; k < 4; ++k) { QI[k] = Q[k] + (Ctr - Q[k]).GetSafeNormal() * Inset; }
		const float Rise = bHoverValid ? 110.f + 12.f * FMath::Sin(Time * 3.f) : 40.f;
		for (int32 k = 0; k < 4; ++k)
		{
			Hv.Add(BarXform(QI[k], QI[(k + 1) & 3], LineZ + 2.f, 9.f, 4.f));
			Hv.Add(FTransform(FRotator::ZeroRotator, FVector(QI[k].X, QI[k].Y, Rise * 0.5f), FVector(0.07f, 0.07f, Rise / 100.f)));
			if (bHoverValid) { Hv.Add(BarXform(QI[k], QI[(k + 1) & 3], Rise, 5.f, 4.f)); }
		}
		(bHoverValid ? HoverGood : HoverBad)->AddInstances(Hv, false, false, false);

		if (Label && !bTaken)
		{
			FDCLabelSpec S;
			S.Icon = bHoverValid ? DC::RoomIcon(Room) : FName(TEXT("ban"));
			const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
			S.Text = Run ? Run->GetRoom(Room).DisplayName : FString(DC::RoomName(Room));
			S.Sub = bHoverValid ? FString::Printf(TEXT("$%d  ·  click to build"), LastCost) : FString(TEXT("must join the house"));
			S.Accent = bHoverValid ? AccentCol : BadCol;
			S.bAccentOutline = true;
			S.Size = 12.f;
			UDCWorldLabel::SetComponentSpec(Label, S);
			Label->SetWorldLocation(FVector(Ctr.X, Ctr.Y, Rise + 70.f));
			Label->SetVisibility(true);
		}
	}
}

void ADCBuildPlacer::Click()
{
	if (!bActive) { return; }
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !bHover) { return; }
	if (!bHoverValid)
	{
		UDCAudioSubsystem::Play(this, DCSfx::Error);
		if (UDCUISubsystem* UI = UDCUISubsystem::Get(this))
		{
			UI->ShowToast(Occupied.Contains(Hover) ? TEXT("Something's already there") : TEXT("New rooms must join the house"), TEXT("ban"), true);
		}
		return;
	}
	EDCPlanBlock Block = EDCPlanBlock::None;
	const FIntPoint Cell = Hover;
	if (Run->BuildRoom(Room, &Block, &Cell))
	{
		UE_LOG(LogDC, Log, TEXT("BuildPlacer: %s at (%d,%d)"), DC::RoomName(Room), Cell.X, Cell.Y);
		UDCAudioSubsystem::Play(this, DCSfx::BuildStart);
		Cancel(true);
		return;
	}
	UDCAudioSubsystem::Play(this, DCSfx::Error);
	if (UDCUISubsystem* UI = UDCUISubsystem::Get(this)) { UI->ShowToast(TEXT("Can't build there"), TEXT("ban"), true); }
}
