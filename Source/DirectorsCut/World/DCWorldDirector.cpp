#include "World/DCWorldDirector.h"
#include "World/DCPlayerController.h"
#include "World/DCCameraRig.h"
#include "World/DCRoomModule.h"
#include "World/DCCriminalPawn.h"
#include "World/DCCriminalSpawnZone.h"
#include "World/DCGarageDoor.h"
#include "World/DCStation.h"
#include "Core/DCRunSubsystem.h"
#include "Core/DCUISubsystem.h"
#include "Core/DCDevTools.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "DirectorsCut.h"

ADCWorldDirector::ADCWorldDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Rng.Initialize(0xD1C7);
}

void ADCWorldDirector::Initialize(ADCPlayerController* InPC)
{
	if (bInitialized) { return; }
	bInitialized = true;
	PC = InPC;
	GatherWorld();
	bWorldTest = FParse::Param(FCommandLine::Get(), TEXT("DCWorldTest"));
	TestShotDir = FPaths::ProjectSavedDir() / TEXT("DCShots");
	FParse::Value(FCommandLine::Get(), TEXT("DCShotDir="), TestShotDir);

	if (UDCRunSubsystem* Run = UDCRunSubsystem::Get(this))
	{
		HState = Run->OnRunStateChanged.AddUObject(this, &ADCWorldDirector::OnRunStateChanged);
		HPhase = Run->OnPhaseChanged.AddUObject(this, &ADCWorldDirector::OnPhaseChanged);
		HStart = Run->OnMissionStarted.AddUObject(this, &ADCWorldDirector::OnMissionStarted);
		HDone = Run->OnMissionCompleted.AddUObject(this, &ADCWorldDirector::OnMissionCompleted);
		HRoom = Run->OnRoomChanged.AddUObject(this, &ADCWorldDirector::OnRoomChanged);
		HLevel = Run->OnCriminalLevelUp.AddUObject(this, &ADCWorldDirector::OnCriminalLevelUp);
		HHired = Run->OnCriminalHired.AddUObject(this, &ADCWorldDirector::OnCriminalHired);
		if (Run->HasActiveRun())
		{
			LastPhase = Run->GetPhase();
			ResetForNewRun();
		}
	}
	if (UDCUISubsystem* UI = UDCUISubsystem::Get(this))
	{
		HPanel = UI->OnPanelChanged.AddUObject(this, &ADCWorldDirector::OnPanelChanged);
	}
	UpdateBounds();
}

void ADCWorldDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDCRunSubsystem* Run = UDCRunSubsystem::Get(this))
	{
		Run->OnRunStateChanged.Remove(HState);
		Run->OnPhaseChanged.Remove(HPhase);
		Run->OnMissionStarted.Remove(HStart);
		Run->OnMissionCompleted.Remove(HDone);
		Run->OnRoomChanged.Remove(HRoom);
		Run->OnCriminalLevelUp.Remove(HLevel);
		Run->OnCriminalHired.Remove(HHired);
	}
	if (UDCUISubsystem* UI = UDCUISubsystem::Get(this)) { UI->OnPanelChanged.Remove(HPanel); }
	Super::EndPlay(Reason);
}

void ADCWorldDirector::GatherWorld()
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	Rooms.Reset(); Zones.Reset(); Doors.Reset(); Stations.Reset();
	for (TActorIterator<ADCRoomModule> It(W); It; ++It) { Rooms.Add(*It); }
	for (TActorIterator<ADCCriminalSpawnZone> It(W); It; ++It) { Zones.Add(*It); }
	for (TActorIterator<ADCGarageDoor> It(W); It; ++It) { Doors.Add(*It); }
	for (TActorIterator<ADCStation> It(W); It; ++It) { Stations.Add(*It); }
	bNavBuilt = false;
	SleepSpots.Reset();
	UE_LOG(LogDC, Log, TEXT("WorldDirector: %d rooms, %d spawn zones, %d doors, %d stations"), Rooms.Num(), Zones.Num(), Doors.Num(), Stations.Num());
}

ADCRoomModule* ADCWorldDirector::FindRoom(EDCRoomType Type) const
{
	for (const TWeakObjectPtr<ADCRoomModule>& R : Rooms) { if (R.IsValid() && R->RoomType == Type) { return R.Get(); } }
	return nullptr;
}

int32 ADCWorldDirector::RoomIndexAt(const FVector& P) const
{
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		const ADCRoomModule* R = Rooms[i].Get();
		if (!R) { continue; }
		const FBox B = R->GetWorldBounds();
		if (P.X >= B.Min.X - 1.f && P.X <= B.Max.X + 1.f && P.Y >= B.Min.Y - 1.f && P.Y <= B.Max.Y + 1.f) { return i; }
	}
	return INDEX_NONE;
}

ADCRoomModule* ADCWorldDirector::RoomAt(const FVector& P) const
{
	const int32 I = RoomIndexAt(P);
	return I == INDEX_NONE ? nullptr : Rooms[I].Get();
}

ADCCriminalPawn* ADCWorldDirector::FindPawn(int32 CriminalId) const
{
	for (ADCCriminalPawn* P : Pawns) { if (P && P->GetCriminalId() == CriminalId) { return P; } }
	return nullptr;
}

bool ADCWorldDirector::IsRoomUsable(const ADCRoomModule* R) const
{
	if (!R) { return false; }
	if (UDCDevTools::IsBuildAll()) { return true; }
	return R->IsUsable() && !RevealQueue.Contains(R->RoomType);
}

ADCGarageDoor* ADCWorldDirector::GetStreetDoor() const
{
	ADCGarageDoor* Any = nullptr;
	for (const TWeakObjectPtr<ADCGarageDoor>& D : Doors)
	{
		if (!D.IsValid()) { continue; }
		if (D->RoomType == EDCRoomType::Office) { return D.Get(); }
		if (!Any || (Any->RoomType == EDCRoomType::Garage && D->RoomType != EDCRoomType::Garage)) { Any = D.Get(); }
	}
	return Any;
}

ADCGarageDoor* ADCWorldDirector::GetMissionDoor() const
{
	if (IsRoomUsable(EDCRoomType::Garage))
	{
		for (const TWeakObjectPtr<ADCGarageDoor>& D : Doors) { if (D.IsValid() && D->RoomType == EDCRoomType::Garage) { return D.Get(); } }
	}
	return GetStreetDoor();
}

// ─── Room graph ──────────────────────────────────────────────────────────────

void ADCWorldDirector::BuildNav()
{
	bNavBuilt = true;
	Portals.Reset();
	UWorld* W = GetWorld();
	if (!W) { return; }
	FCollisionQueryParams Q(SCENE_QUERY_STAT(DCNav), false);

	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		for (int32 j = i + 1; j < Rooms.Num(); ++j)
		{
			const ADCRoomModule* RA = Rooms[i].Get();
			const ADCRoomModule* RB = Rooms[j].Get();
			if (!RA || !RB) { continue; }
			const FBox A = RA->GetWorldBounds(), B = RB->GetWorldBounds();
			// Shared wall: along Y at a fixed X, or along X at a fixed Y.
			for (int32 Axis = 0; Axis < 2; ++Axis)
			{
				const int32 Fix = Axis == 0 ? 0 : 1;   // fixed coordinate (0 = X, 1 = Y)
				const int32 Run = 1 - Fix;
				float Line = 0.f, Sign = 0.f;
				if (FMath::Abs(A.Max[Fix] - B.Min[Fix]) < 5.f) { Line = A.Max[Fix]; Sign = 1.f; }
				else if (FMath::Abs(A.Min[Fix] - B.Max[Fix]) < 5.f) { Line = A.Min[Fix]; Sign = -1.f; }
				else { continue; }
				const float Lo = FMath::Max(A.Min[Run], B.Min[Run]), Hi = FMath::Min(A.Max[Run], B.Max[Run]);
				if (Hi - Lo < 150.f) { continue; }
				FVector N = FVector::ZeroVector; N[Fix] = Sign;
				const float Z = RA->GetActorLocation().Z;
				constexpr float Step = 10.f;
				int32 BestStart = -1, BestLen = 0, CurStart = -1, CurLen = 0, Samples = 0, Hits = 0;
				for (float T = Lo + 30.f; T <= Hi - 30.f; T += Step, ++Samples)
				{
					FVector P(0.f, 0.f, Z + 30.f); P[Fix] = Line; P[Run] = T;
					const bool bHit = W->LineTraceTestByChannel(P - N * 45.f, P + N * 45.f, ECC_WorldStatic, Q);
					if (bHit) { ++Hits; CurLen = 0; CurStart = -1; continue; }
					if (CurStart < 0) { CurStart = Samples; }
					++CurLen;
					if (CurLen > BestLen) { BestLen = CurLen; BestStart = CurStart; }
				}
				if (BestLen * Step < 70.f) { continue; } // solid wall, no doorway
				const float Mid = Lo + 30.f + (BestStart + (BestLen - 1) * 0.5f) * Step;
				FVector P(0.f, 0.f, Z); P[Fix] = Line; P[Run] = Mid;
				FPortal Portal;
				Portal.A = i; Portal.B = j;
				Portal.InA = P - N * 60.f;
				Portal.InB = P + N * 60.f;
				Portal.InA.Z = RA->GetActorLocation().Z;
				Portal.InB.Z = RB->GetActorLocation().Z;
				Portals.Add(Portal);
				UE_LOG(LogDC, Log, TEXT("WorldDirector nav: %s <-> %s doorway %.0f cm at %s (wall hits %d/%d)"),
					DC::RoomName(RA->RoomType), DC::RoomName(RB->RoomType), BestLen * Step, *P.ToCompactString(), Hits, Samples);
			}
		}
	}
}

void ADCWorldDirector::PlanPath(const FVector& From, const FVector& To, TArray<FVector>& Out)
{
	if (!bNavBuilt) { BuildNav(); }
	const int32 S = RoomIndexAt(From), G = RoomIndexAt(To);
	if (S == INDEX_NONE || G == INDEX_NONE || S == G) { Out.Add(To); return; }
	const int32 N = Rooms.Num();
	TArray<float> Dist; Dist.Init(1e9f, N);
	TArray<int32> Prev; Prev.Init(INDEX_NONE, N);
	TArray<int32> Via; Via.Init(INDEX_NONE, N);
	TArray<bool> Done; Done.Init(false, N);
	Dist[S] = 0.f;
	for (int32 Iter = 0; Iter < N; ++Iter)
	{
		int32 U = INDEX_NONE;
		for (int32 k = 0; k < N; ++k) { if (!Done[k] && Dist[k] < 1e8f && (U == INDEX_NONE || Dist[k] < Dist[U])) { U = k; } }
		if (U == INDEX_NONE || U == G) { break; }
		Done[U] = true;
		for (int32 pi = 0; pi < Portals.Num(); ++pi)
		{
			const FPortal& Pt = Portals[pi];
			if (Pt.A != U && Pt.B != U) { continue; }
			const int32 V = Pt.A == U ? Pt.B : Pt.A;
			// Locked rooms are a last resort (walking past dust sheets beats walking through walls).
			const float Cost = (V == G || IsRoomUsable(Rooms[V].Get())) ? 1.f : 6.f;
			if (Dist[U] + Cost < Dist[V]) { Dist[V] = Dist[U] + Cost; Prev[V] = U; Via[V] = pi; }
		}
	}
	if (Prev[G] == INDEX_NONE) { Out.Add(To); return; } // disconnected: best effort
	TArray<int32> Chain;
	for (int32 V = G; V != S; V = Prev[V]) { Chain.Insert(V, 0); }
	int32 Cur = S;
	for (int32 V : Chain)
	{
		const FPortal& Pt = Portals[Via[V]];
		const bool bForward = Pt.A == Cur;
		Out.Add(bForward ? Pt.InA : Pt.InB);
		Out.Add(bForward ? Pt.InB : Pt.InA);
		Cur = V;
	}
	Out.Add(To);
}

// ─── Idle life ───────────────────────────────────────────────────────────────

void ADCWorldDirector::GetAnchors(const ADCRoomModule* Room, TArray<FAnchor>& Out)
{
	if (!Room) { return; }
	const FBox RB = Room->GetWorldBounds();
	const float FloorZ = Room->GetActorLocation().Z;
	const FBox Inner(FVector(RB.Min.X + 60.f, RB.Min.Y + 60.f, RB.Min.Z), FVector(RB.Max.X - 60.f, RB.Max.Y - 60.f, RB.Max.Z));
	for (const TWeakObjectPtr<ADCCriminalSpawnZone>& Z : Zones)
	{
		if (!Z.IsValid() || RoomAt(Z->GetActorLocation()) != Room) { continue; }
		for (int32 k = 0; k < 4; ++k)
		{
			FAnchor A;
			A.Point = Z->GetRandomPoint(Rng);
			A.Point.Z = FloorZ;
			Out.Add(A);
		}
	}
	for (const TWeakObjectPtr<ADCStation>& SW : Stations)
	{
		const ADCStation* S = SW.Get();
		if (!S || !S->IsStationEnabled() || RoomAt(S->GetActorLocation()) != Room) { continue; }
		// Stand in front of the click box face that looks towards the room centre.
		const FTransform T = S->GetActorTransform();
		const FVector E = S->ClickBox->GetScaledBoxExtent();
		const FVector D = T.InverseTransformVectorNoScale(RB.GetCenter() - S->GetActorLocation());
		FVector Local = FVector::ZeroVector;
		if (FMath::Abs(D.X) / FMath::Max(1.f, E.X) > FMath::Abs(D.Y) / FMath::Max(1.f, E.Y)) { Local.X = FMath::Sign(D.X) * (E.X + 48.f); }
		else { Local.Y = FMath::Sign(D.Y) * (E.Y + 48.f); }
		FVector P = T.TransformPositionNoScale(Local);
		P.X = FMath::Clamp(P.X, Inner.Min.X, Inner.Max.X);
		P.Y = FMath::Clamp(P.Y, Inner.Min.Y, Inner.Max.Y);
		P.Z = FloorZ;
		FAnchor A;
		A.Point = P + FVector(Rng.FRandRange(-15.f, 15.f), Rng.FRandRange(-15.f, 15.f), 0.f);
		A.Yaw = (S->GetActorLocation() - P).GetSafeNormal2D().Rotation().Yaw;
		A.bHasYaw = true;
		A.bStation = true;
		Out.Add(A);
	}
}

bool ADCWorldDirector::IsSpotTaken(const FVector& P, const ADCCriminalPawn* Except) const
{
	for (const ADCCriminalPawn* O : Pawns)
	{
		if (!O || O == Except || O->IsAway() || O->IsHidden()) { continue; }
		if (FVector::Dist2D(O->GetGoalLocation(), P) < 75.f || FVector::Dist2D(O->GetFeet(), P) < 60.f) { return true; }
	}
	return false;
}

bool ADCWorldDirector::IsSleeper(const ADCCriminalPawn* P) const
{
	// About one in four stays up all night.
	const uint32 H = ::GetTypeHash(P->GetCriminalId() * 2654435761u);
	return (H % 4u) != 1u;
}

void ADCWorldDirector::BuildSleepSpots()
{
	SleepSpots.Reset();
	SleepOwner.Reset();
	const ADCRoomModule* Lounge = FindRoom(EDCRoomType::Lounge);
	if (!Lounge) { return; }
	const ADCCriminalSpawnZone* Zone = nullptr;
	for (const TWeakObjectPtr<ADCCriminalSpawnZone>& Z : Zones)
	{
		if (Z.IsValid() && (Z->RoomType == EDCRoomType::Lounge || RoomAt(Z->GetActorLocation()) == Lounge)) { Zone = Z.Get(); break; }
	}
	if (!Zone) { return; }
	const FVector E = Zone->Zone->GetScaledBoxExtent();
	const FTransform T = Zone->GetActorTransform();
	// Rows of "beds" along the zone's long axis; the body lies along the short axis.
	const bool bLongX = E.X >= E.Y;
	const float Long = bLongX ? E.X : E.Y, Short = bLongX ? E.Y : E.X;
	const int32 Cols = FMath::Clamp(FMath::FloorToInt(Long * 2.f / 85.f), 1, 6);
	const int32 Rows = Short * 2.f > 260.f ? 2 : 1;
	for (int32 r = 0; r < Rows; ++r)
	{
		for (int32 c = 0; c < Cols; ++c)
		{
			const float U = -Long + (c + 0.5f) * (Long * 2.f / Cols);
			const float V = Rows == 1 ? 0.f : (r == 0 ? -Short * 0.5f : Short * 0.5f);
			const FVector Local = bLongX ? FVector(U, V, -E.Z) : FVector(V, U, -E.Z);
			FVector P = T.TransformPosition(Local);
			P.Z = Lounge->GetActorLocation().Z;
			SleepSpots.Add(P);
		}
	}
	SleepYaw = T.GetRotation().Rotator().Yaw + (bLongX ? 90.f : 0.f);
	SleepOwner.Init(INDEX_NONE, SleepSpots.Num());
}

int32 ADCWorldDirector::AcquireSleepSlot(ADCCriminalPawn* P)
{
	if (SleepSpots.Num() == 0) { BuildSleepSpots(); }
	for (int32 i = 0; i < SleepOwner.Num(); ++i) { if (SleepOwner[i] == P->GetCriminalId()) { return i; } }
	TArray<int32> Free;
	for (int32 i = 0; i < SleepOwner.Num(); ++i) { if (SleepOwner[i] == INDEX_NONE) { Free.Add(i); } }
	if (Free.Num() == 0) { return INDEX_NONE; }
	const int32 Slot = Free[Rng.RandHelper(Free.Num())];
	SleepOwner[Slot] = P->GetCriminalId();
	return Slot;
}

bool ADCWorldDirector::TrySendToBed(ADCCriminalPawn* P, bool bInstant)
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->IsNight() || !IsSleeper(P) || !IsRoomUsable(EDCRoomType::Lounge)) { return false; }
	const int32 Slot = AcquireSleepSlot(P);
	if (Slot == INDEX_NONE) { return false; }
	P->SetSleepSlot(Slot);
	const FVector Spot = SleepSpots[Slot];
	const float Yaw = SleepYaw + ((P->GetCriminalId() & 1) ? 180.f : 0.f);
	TArray<FVector> Route;
	if (bInstant) { P->PlaceAt(Spot); }
	else { PlanPath(P->GetFeet(), Spot, Route); }
	P->SetIdleGoal(Route, EDCPawnActivity::Sleep, Yaw, 0.f);
	return true;
}

FVector ADCWorldDirector::PickSpawnPoint(ADCCriminalPawn* P)
{
	// Hangouts (rooms with a spawn zone) first, then any usable room's station fronts.
	TArray<FAnchor> Anchors;
	for (const TWeakObjectPtr<ADCCriminalSpawnZone>& Z : Zones)
	{
		ADCRoomModule* R = Z.IsValid() ? RoomAt(Z->GetActorLocation()) : nullptr;
		if (IsRoomUsable(R)) { GetAnchors(R, Anchors); }
	}
	Anchors.RemoveAll([](const FAnchor& A) { return A.bStation; });
	if (Anchors.Num() == 0)
	{
		for (const TWeakObjectPtr<ADCRoomModule>& R : Rooms) { if (IsRoomUsable(R.Get())) { GetAnchors(R.Get(), Anchors); } }
	}
	if (Anchors.Num() == 0)
	{
		const ADCRoomModule* Office = FindRoom(EDCRoomType::Office);
		return Office ? Office->GetFocusPoint() * FVector(1, 1, 0) + FVector(0, 0, Office->GetActorLocation().Z) : FVector::ZeroVector;
	}
	for (int32 Try = 0; Try < 10; ++Try)
	{
		const FAnchor& A = Anchors[Rng.RandHelper(Anchors.Num())];
		if (!IsSpotTaken(A.Point, P)) { return A.Point; }
	}
	return Anchors[Rng.RandHelper(Anchors.Num())].Point;
}

void ADCWorldDirector::AssignIdleGoal(ADCCriminalPawn* P)
{
	if (!P || P->GetMode() != EDCPawnMode::Wander) { return; }
	if (TrySendToBed(P, false)) { return; }
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	const bool bNight = Run && Run->IsNight();
	const ADCRoomModule* Here = RoomAt(P->GetFeet());

	// Pick a room: stay put more often than not; night owls favour screens.
	TArray<TPair<ADCRoomModule*, float>> Cands;
	float Total = 0.f;
	for (const TWeakObjectPtr<ADCRoomModule>& RW : Rooms)
	{
		ADCRoomModule* R = RW.Get();
		if (!IsRoomUsable(R)) { continue; }
		float W = 1.f;
		if (R == Here) { W *= 2.5f; }
		if (R->RoomType == EDCRoomType::Lounge || R->RoomType == EDCRoomType::Office) { W *= 1.5f; }
		if (bNight && (R->RoomType == EDCRoomType::OpsRoom || R->RoomType == EDCRoomType::ServerRoom)) { W *= 2.f; }
		if (bNight && R->RoomType == EDCRoomType::Lounge) { W *= 0.3f; } // let the sleepers sleep
		if (P->IsInjured() && R->RoomType == EDCRoomType::Clinic) { W *= 4.f; }
		Cands.Add({ R, W });
		Total += W;
	}
	if (Cands.Num() == 0) { return; }
	float Pick = Rng.FRandRange(0.f, Total);
	ADCRoomModule* Target = Cands.Last().Key;
	for (const auto& C : Cands) { Pick -= C.Value; if (Pick <= 0.f) { Target = C.Key; break; } }

	TArray<FAnchor> Anchors;
	GetAnchors(Target, Anchors);
	if (Anchors.Num() == 0 && Here && Here != Target) { GetAnchors(Here, Anchors); }
	if (Anchors.Num() == 0) { return; }
	for (int32 Try = 0; Try < 6; ++Try)
	{
		const FAnchor& A = Anchors[Rng.RandHelper(Anchors.Num())];
		if (IsSpotTaken(A.Point, P)) { continue; }
		TArray<FVector> Route;
		PlanPath(P->GetFeet(), A.Point, Route);
		const float Hold = A.bStation ? Rng.FRandRange(6.f, 14.f) : Rng.FRandRange(2.5f, 7.f) * (bNight ? 1.8f : 1.f);
		P->SetIdleGoal(Route, A.bStation ? EDCPawnActivity::Work : EDCPawnActivity::Stand, A.bHasYaw ? A.Yaw : NAN, Hold);
		return;
	}
}

void ADCWorldDirector::UpdateClockLife()
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->HasActiveRun()) { return; }
	const bool bNight = Run->IsNight();
	// Release beds nobody is using any more.
	for (int32 i = 0; i < SleepOwner.Num(); ++i)
	{
		if (SleepOwner[i] == INDEX_NONE) { continue; }
		const ADCCriminalPawn* P = FindPawn(SleepOwner[i]);
		if (!P || P->GetSleepSlot() != i || !(P->IsSleeping() || P->IsHeadingToSleep())) { SleepOwner[i] = INDEX_NONE; }
	}
	if (bNight != bWasNight)
	{
		bWasNight = bNight;
		for (ADCCriminalPawn* P : Pawns)
		{
			if (!P || P->GetMode() != EDCPawnMode::Wander) { continue; }
			if (bNight && IsSleeper(P)) { P->InterruptIdle(Rng.FRandRange(0.f, 6.f)); }      // head to bed soon
			else if (!bNight && (P->IsSleeping() || P->IsHeadingToSleep())) { P->InterruptIdle(Rng.FRandRange(0.5f, 8.f)); } // staggered wake-up
		}
	}
}

// ─── Run events ──────────────────────────────────────────────────────────────

bool ADCWorldDirector::IsDetectingNewRun()
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->HasActiveRun()) { return false; }
	if (!bHadRun) { return true; }
	return Run->GetSeed() != LastSeed || Run->GetRunTime() < LastRunTime - 0.01;
}

void ADCWorldDirector::OnPhaseChanged(EDCGamePhase Phase)
{
	const EDCGamePhase Prev = LastPhase;
	LastPhase = Phase;
	// New run or Continue (loaded save): rebuild the whole world state from the run.
	if (Phase == EDCGamePhase::Playing && (Prev != EDCGamePhase::Playing || IsDetectingNewRun()))
	{
		ResetForNewRun();
		return;
	}
	SyncPawns();
}

void ADCWorldDirector::OnRunStateChanged()
{
	if (IsDetectingNewRun()) { ResetForNewRun(); return; }
	if (const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this)) { LastRunTime = Run->GetRunTime(); }
	SyncRooms();
	SyncPawns();
}

void ADCWorldDirector::OnMissionStarted(const FDCMission& Mission)
{
	ADCGarageDoor* Door = GetMissionDoor();
	float Delay = 0.2f;
	for (int32 Id : Mission.CrewIds)
	{
		if (ADCCriminalPawn* P = FindPawn(Id)) { P->Depart(Door, Delay, false); Delay += 0.7f; }
	}
}

void ADCWorldDirector::OnMissionCompleted(const FDCMissionReport& Report)
{
	ADCGarageDoor* Door = GetMissionDoor();
	float Delay = 0.5f;
	for (const FDCCrewMemberResult& M : Report.Members)
	{
		ADCCriminalPawn* P = FindPawn(M.CriminalId);
		if (!P) { continue; }
		if (M.bArrested) { P->MarkLeaveForever(); continue; } // never comes back (SyncPawns removes it)
		P->ReturnHome(Door, Delay);
		Delay += 0.8f;
	}
}

void ADCWorldDirector::OnRoomChanged(const FDCRoom& Room)
{
	if (UDCDevTools::IsBuildAll()) { return; }
	ADCRoomModule* M = FindRoom(Room.Type);
	if (!M) { return; }
	if (Room.State == EDCRoomState::Building)
	{
		// Scaffolding + work lights (and the build timer) for the whole build.
		if (!M->IsRevealPlaying() && !RevealQueue.Contains(Room.Type)) { M->ApplyState(EDCRoomState::Building, Room.Level); }
		return;
	}
	if (Room.State == EDCRoomState::Built && (M->GetState() != EDCRoomState::Built || Room.Level > M->GetLevel()))
	{
		if (M->GetLevel() >= 1 && !RevealQueue.Contains(Room.Type)) { M->PlayUpgradeReveal(Room.Level); }
		else { RevealQueue.AddUnique(Room.Type); }
		return;
	}
	if (!RevealQueue.Contains(Room.Type) && RevealingRoom.Get() != M) { M->ApplyState(Room.State, Room.Level); }
}

void ADCWorldDirector::OnCriminalLevelUp(int32 CriminalId, int32 NewLevel)
{
	if (ADCCriminalPawn* P = FindPawn(CriminalId)) { P->PlayLevelUp(NewLevel); }
}

void ADCWorldDirector::OnCriminalHired(const FDCCriminal& C)
{
	if (ADCCriminalPawn* Existing = FindPawn(C.Id))
	{
		Existing->SetAway();
		Existing->ReturnHome(GetStreetDoor(), 0.4f);
		return;
	}
	if (Pawns.Num() < MaxPawns) { SpawnPawn(C, true); }
}

void ADCWorldDirector::OnPanelChanged(EDCPanel Panel)
{
	LastPanel = Panel;
	if (Panel == EDCPanel::Missions || Panel == EDCPanel::MissionPlanner)
	{
		if (const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this))
		{
			for (const FDCMission& M : Run->GetMissions()) { SeenMissionIds.Add(M.Id); }
		}
	}
}

// ─── Sync ────────────────────────────────────────────────────────────────────

void ADCWorldDirector::ResetForNewRun()
{
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->HasActiveRun()) { return; }
	if (Rooms.Num() == 0) { GatherWorld(); }
	bHadRun = true;
	LastSeed = Run->GetSeed();
	LastRunTime = Run->GetRunTime();
	LastPhase = Run->GetPhase();
	bWasNight = Run->IsNight();

	for (ADCCriminalPawn* P : Pawns) { if (P) { P->Destroy(); } }
	Pawns.Reset();
	RevealQueue.Reset();
	RevealingRoom = nullptr;
	SeenMissionIds.Reset();
	SleepOwner.Init(INDEX_NONE, SleepSpots.Num());

	SyncRooms();
	UpdateBounds();
	if (PC.IsValid() && PC->GetCameraRig()) { PC->GetCameraRig()->ResetFocus(); }
	for (const FDCCriminal& C : Run->GetRoster())
	{
		if (Pawns.Num() >= MaxPawns) { break; }
		SpawnPawn(C, false);
	}
	UE_LOG(LogDC, Log, TEXT("WorldDirector: run synced (seed %d, t=%.0f, %02.0fh), %d pawns"), LastSeed, LastRunTime, Run->GetHourOfDay(), Pawns.Num());
}

void ADCWorldDirector::SyncRooms()
{
	if (UDCDevTools::IsBuildAll()) { return; }
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->HasActiveRun()) { return; }
	for (const TWeakObjectPtr<ADCRoomModule>& RW : Rooms)
	{
		ADCRoomModule* M = RW.Get();
		if (!M || M->IsRevealPlaying() || RevealQueue.Contains(M->RoomType)) { continue; }
		const FDCRoom R = Run->GetRoom(M->RoomType);
		if (R.State != M->GetState() || R.Level != M->GetLevel()) { M->ApplyState(R.State, R.Level); }
	}
	UpdateStations();
}

ADCCriminalPawn* ADCWorldDirector::SpawnPawn(const FDCCriminal& C, bool bWalkIn)
{
	UWorld* W = GetWorld();
	if (!W) { return nullptr; }
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADCCriminalPawn* P = W->SpawnActor<ADCCriminalPawn>(ADCCriminalPawn::StaticClass(), FTransform(FVector(0, 0, 100)), SP);
	if (!P) { return nullptr; }
	Pawns.Add(P);
	P->InitFromCriminal(C, this);
	if (bWalkIn)
	{
		P->SetAway();
		P->ReturnHome(GetStreetDoor(), 0.4f);
	}
	else if (C.Status == EDCCriminalStatus::OnMission)
	{
		P->SetAway(); // continued run: still out on the job
	}
	else
	{
		P->PlaceAt(PickSpawnPoint(P));
		TrySendToBed(P, true);
	}
	return P;
}

void ADCWorldDirector::SyncPawns()
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->HasActiveRun()) { return; }
	const TArray<FDCCriminal>& Roster = Run->GetRoster();

	// Gone from the roster: dismissed (walks out of the street door) or arrested (never comes back).
	for (int32 i = Pawns.Num() - 1; i >= 0; --i)
	{
		ADCCriminalPawn* P = Pawns[i];
		if (!P) { Pawns.RemoveAt(i); continue; }
		const bool bInRoster = Roster.ContainsByPredicate([P](const FDCCriminal& C) { return C.Id == P->GetCriminalId(); });
		if (bInRoster) { continue; }
		if (P->IsAway()) { P->Destroy(); Pawns.RemoveAt(i); continue; }
		if (P->GetMode() == EDCPawnMode::WalkOut) { P->MarkLeaveForever(); continue; }
		if (!P->IsLeavingForever()) { P->Depart(GetStreetDoor(), 0.f, true); }
	}

	for (const FDCCriminal& C : Roster)
	{
		ADCCriminalPawn* P = FindPawn(C.Id);
		if (!P)
		{
			if (Pawns.Num() < MaxPawns) { SpawnPawn(C, false); }
			continue;
		}
		P->UpdateFromCriminal(C);
		// Self-heal against missed events: on a job but still idling -> leave; back but still away -> come home.
		const bool bOnJob = C.Status == EDCCriminalStatus::OnMission;
		if (bOnJob && P->GetMode() == EDCPawnMode::Wander) { P->Depart(GetMissionDoor(), 0.2f, false); }
		else if (!bOnJob && P->IsAway() && !P->IsLeavingForever()) { P->ReturnHome(GetMissionDoor(), 0.5f); }
	}
}

void ADCWorldDirector::UpdateStations()
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	int32 NewPostings = 0, Out = 0, Back = 0;
	if (Run && Run->HasActiveRun())
	{
		for (const FDCMission& M : Run->GetMissions())
		{
			if (M.bInProgress) { ++Out; }
			else if (!SeenMissionIds.Contains(M.Id)) { ++NewPostings; }
		}
		Back = Run->GetUnseenReportCount();
	}
	for (const TWeakObjectPtr<ADCStation>& SW : Stations)
	{
		ADCStation* S = SW.Get();
		if (!S) { continue; }
		const ADCRoomModule* Own = RoomAt(S->GetActorLocation());
		const bool bEnabled = (!Own || IsRoomUsable(Own)) && IsRoomUsable(S->GetRequiredRoom());
		S->SetStationEnabled(bEnabled);
		if (S->StationType == EDCStationType::MissionBoard)
		{
			S->SetBadge(TEXT("bell-dot"), NewPostings > 0 ? FString::Printf(TEXT("%d new"), NewPostings) : FString(), FLinearColor(1.f, 0.7f, 0.2f));
		}
		else if (S->StationType == EDCStationType::GarageDoor)
		{
			if (Back > 0) { S->SetBadge(TEXT("inbox"), FString::Printf(TEXT("%d back"), Back), FLinearColor(0.3f, 0.9f, 0.5f)); }
			else { S->SetBadge(TEXT("car-front"), Out > 0 ? FString::Printf(TEXT("%d out"), Out) : FString(), FLinearColor(0.35f, 0.65f, 1.f)); }
		}
	}
}

void ADCWorldDirector::UpdateBuildTimers()
{
	if (UDCDevTools::IsBuildAll()) { return; }
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->HasActiveRun()) { return; }
	const double Now = Run->GetRunTime();
	for (const FDCRoom& R : Run->GetRooms())
	{
		if (R.State != EDCRoomState::Building) { continue; }
		ADCRoomModule* M = FindRoom(R.Type);
		if (!M || M->IsRevealPlaying()) { continue; }
		const double Len = FMath::Max(0.01, R.BuildEndsAt - R.BuildStartedAt);
		M->SetBuildProgress((float)((Now - R.BuildStartedAt) / Len), R.BuildEndsAt - Now, R.Level + 1);
	}
}

void ADCWorldDirector::UpdateBounds()
{
	ADCCameraRig* Rig = PC.IsValid() ? PC->GetCameraRig() : nullptr;
	if (!Rig) { return; }
	FBox B(ForceInit);
	for (const TWeakObjectPtr<ADCRoomModule>& R : Rooms)
	{
		if (R.IsValid() && (R->GetState() != EDCRoomState::Locked || R->IsRevealPlaying())) { B += R->GetWorldBounds(); }
	}
	if (!B.IsValid)
	{
		for (const TWeakObjectPtr<ADCRoomModule>& R : Rooms) { if (R.IsValid()) { B += R->GetWorldBounds(); } }
	}
	if (!B.IsValid) { return; }
	Rig->SetBounds(B);
	if (!bBoundsInitialized)
	{
		bBoundsInitialized = true;
		Rig->ResetFocus();
		Rig->SnapToTarget();
	}
}

void ADCWorldDirector::UpdateDoors(float Dt)
{
	for (const TWeakObjectPtr<ADCGarageDoor>& DW : Doors)
	{
		ADCGarageDoor* D = DW.Get();
		if (!D) { continue; }
		bool bNear = false;
		for (ADCCriminalPawn* P : Pawns)
		{
			if (P && P->GetActiveDoor() == D && P->IsNearDoor(D->bRollUp ? 520.f : 300.f)) { bNear = true; break; }
		}
		float& Idle = DoorIdleTime.FindOrAdd(D);
		if (bNear) { Idle = 0.f; D->Open(); }
		else
		{
			Idle += Dt;
			if (Idle > 0.9f) { D->Close(); }
		}
	}
}

void ADCWorldDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Rooms.Num() == 0) { GatherWorld(); if (Rooms.Num()) { UpdateBounds(); } }
	if (!bNavBuilt && Rooms.Num()) { BuildNav(); }

	// One full reveal at a time.
	ADCRoomModule* Revealing = RevealingRoom.Get();
	if (Revealing && !Revealing->IsRevealPlaying())
	{
		RevealingRoom = nullptr;
		Revealing = nullptr;
		UpdateBounds();
		UpdateStations();
	}
	if (!Revealing && RevealQueue.Num() > 0)
	{
		const EDCRoomType T = RevealQueue[0];
		RevealQueue.RemoveAt(0);
		if (ADCRoomModule* M = FindRoom(T))
		{
			const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
			const int32 Level = Run ? FMath::Max(1, Run->GetRoom(T).Level) : FMath::Max(1, M->GetLevel() + 1);
			M->PlayReveal(Level);
			RevealingRoom = M;
			UpdateBounds();
		}
	}

	for (int32 i = Pawns.Num() - 1; i >= 0; --i)
	{
		ADCCriminalPawn* P = Pawns[i];
		if (!P) { Pawns.RemoveAt(i); continue; }
		if (P->WantsDestroy()) { P->Destroy(); Pawns.RemoveAt(i); continue; }
	}
	UpdateDoors(DeltaSeconds);

	SlowTick += DeltaSeconds;
	if (SlowTick >= 0.25f)
	{
		SlowTick = 0.f;
		UpdateBuildTimers();
		UpdateStations();
		UpdateClockLife();
	}
	if (bWorldTest) { TickWorldTest(DeltaSeconds); }
}

// ─── -DCWorldTest ────────────────────────────────────────────────────────────

void ADCWorldDirector::TestShot(const FString& Name)
{
	const FString File = TestShotDir / (Name + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(File, true, false);
	UE_LOG(LogDC, Display, TEXT("[WorldTest] shot %s"), *File);
}

void ADCWorldDirector::LogPawns(const TCHAR* Tag) const
{
	static const TCHAR* Modes[] = { TEXT("Wander"), TEXT("WalkOut"), TEXT("Away"), TEXT("WalkIn") };
	static const TCHAR* Acts[] = { TEXT("Stand"), TEXT("Work"), TEXT("Sleep") };
	for (const ADCCriminalPawn* P : Pawns)
	{
		if (!P) { continue; }
		const ADCRoomModule* R = RoomAt(P->GetFeet());
		UE_LOG(LogDC, Display, TEXT("[WorldTest] %s pawn %d %s/%s%s%s hidden %d room %s loc %s"), Tag, P->GetCriminalId(), Modes[(int32)P->GetMode()], Acts[(int32)P->GetActivity()],
			P->IsSleeping() ? TEXT(" lying") : TEXT(""), P->IsReturnQueued() ? TEXT(" return-queued") : TEXT(""), P->IsHidden() ? 1 : 0,
			R ? DC::RoomName(R->RoomType) : TEXT("OUTSIDE"), *P->GetActorLocation().ToCompactString());
	}
}

void ADCWorldDirector::TickWorldTest(float Dt)
{
	TestTime += Dt;
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	ADCCameraRig* Rig = PC.IsValid() ? PC->GetCameraRig() : nullptr;
	if (!Run || !Rig || !Run->HasActiveRun()) { return; }
	float EV = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCWorldTestEV="), EV))
	{
		Rig->Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
		Rig->Camera->PostProcessSettings.AutoExposureBias = EV;
	}
	if (TestTime < StepAt) { return; }
	auto Next = [this](double Wait) { StepAt = TestTime + Wait; ++TestStep; };
	auto Focus = [&](EDCRoomType T, float Zoom) { if (ADCRoomModule* M = FindRoom(T)) { Rig->FocusOn(M->GetFocusPoint(), Zoom); } };
	auto SkipTo = [Run](float Hour) { const float DeltaH = FMath::Fmod(Hour - Run->GetHourOfDay() + 24.f, 24.f); Run->AdvanceTime(DeltaH * 60.0 / FMath::Max(0.01f, Run->GetGameMinutesPerSecond())); };
	auto Build = [this, Run](EDCRoomType T)
	{
		const bool bOk = Run->BuildRoom(T);
		const FDCRoom R = Run->GetRoom(T);
		UE_LOG(LogDC, Display, TEXT("[WorldTest] BuildRoom %s: %d (state %d level %d, %.0fs, cash $%d)"), DC::RoomName(T), bOk ? 1 : 0, (int32)R.State, R.Level, R.BuildEndsAt - R.BuildStartedAt, Run->GetCash());
		return bOk;
	};
	auto FinishBuild = [Run]() { for (const FDCRoom& R : Run->GetRooms()) { if (R.State == EDCRoomState::Building) { Run->AdvanceTime(R.BuildEndsAt - Run->GetRunTime() + 0.05); } } };

	switch (TestStep)
	{
	case 0: if (TestTime >= 6.0) { LogPawns(TEXT("start")); TestShot(TEXT("t_start")); Next(1.0); } break;
	// Build the Bunkroom: scaffolding + timer for the whole build, then the reveal.
	case 1: Build(EDCRoomType::Lounge); Run->AdvanceTime(12.0); Focus(EDCRoomType::Lounge, 0.45f); Next(2.5); break;
	case 2: TestShot(TEXT("t_building")); Next(0.5); break;
	case 3: FinishBuild(); Rig->ResetFocus(); Next(0.9); break;
	case 4: TestShot(TEXT("t_reveal_a")); Next(1.2); break;
	case 5: TestShot(TEXT("t_reveal_b")); Next(3.0); break;
	case 6: TestShot(TEXT("t_after_reveal")); LogPawns(TEXT("after_reveal")); Next(0.5); break;
	// Garage, so crews use the roller door.
	case 7: Build(EDCRoomType::Garage); FinishBuild(); Next(5.0); break;
	// Start every staffable mission: crews walk (staggered) to the garage and leave.
	case 8:
		{
			int32 Started = 0;
			double Shortest = 1e9;
			for (const FDCMission& M : Run->GetMissions())
			{
				if (M.bInProgress || Run->GetActiveMissionCount() >= Run->GetMaxActiveMissions()) { continue; }
				if (Run->StartMission(Run->SuggestPlan(M.Id))) { ++Started; }
			}
			for (const FDCMission& M : Run->GetMissions()) { if (M.bInProgress) { Shortest = FMath::Min(Shortest, M.EndsAt - Run->GetRunTime()); } }
			UE_LOG(LogDC, Display, TEXT("[WorldTest] started %d missions (active %d), shortest %.0fs"), Started, Run->GetActiveMissionCount(), Shortest);
			Focus(EDCRoomType::Garage, 0.3f);
			StepAt = TestTime + 1.2; ++TestStep;
			// The shortest finishes while its crew is still walking out: the return must queue.
			if (Shortest < 1e8) { Run->AdvanceTime(Shortest + 0.5); }
			LogPawns(TEXT("departing"));
		}
		break;
	case 9: TestShot(TEXT("t_departing")); Next(2.5); break;
	case 10: TestShot(TEXT("t_departing_b")); LogPawns(TEXT("departing_b")); Next(6.0); break;
	case 11: LogPawns(TEXT("away")); Run->AdvanceTime(1300.0); Next(2.0); break;
	case 12: TestShot(TEXT("t_returning")); LogPawns(TEXT("returning")); Next(5.0); break;
	case 13: TestShot(TEXT("t_returned")); LogPawns(TEXT("returned")); Next(0.5); break;
	// Office upgrade: quick reveal of the new level.
	case 14: Focus(EDCRoomType::Office, 0.35f); Build(EDCRoomType::Office); Run->AdvanceTime(5.0); Next(2.0); break;
	case 15: TestShot(TEXT("t_upgrading")); FinishBuild(); Next(0.5); break;
	case 16: TestShot(TEXT("t_upgrade_reveal")); Next(2.0); break;
	// Dismiss one, hire one (street door).
	case 17:
		{
			for (const FDCCriminal& C : Run->GetRoster()) { if (C.IsAvailable()) { UE_LOG(LogDC, Display, TEXT("[WorldTest] dismiss %d"), C.Id); Run->Dismiss(C.Id); break; } }
			if (Run->GetRecruits().Num() > 0) { FString E; const bool bOk = Run->Hire(Run->GetRecruits()[0].Id, &E); UE_LOG(LogDC, Display, TEXT("[WorldTest] Hire %d %s (cash $%d)"), bOk ? 1 : 0, *E, Run->GetCash()); }
			Focus(EDCRoomType::Office, 0.4f);
			Next(2.0);
		}
		break;
	case 18: TestShot(TEXT("t_fired_hired")); LogPawns(TEXT("fired_hired")); Next(4.0); break;
	case 19:
		for (ADCCriminalPawn* P : Pawns) { if (P && !P->IsHidden() && P->GetMode() == EDCPawnMode::Wander) { Run->OnCriminalLevelUp.Broadcast(P->GetCriminalId(), 5); Rig->FocusOn(P->GetActorLocation(), 0.85f); break; } }
		Next(0.6);
		break;
	case 20: TestShot(TEXT("t_levelup")); Next(0.5); break;
	// Night: most crew go to bed in the bunkroom, a couple stay up.
	case 21: SkipTo(22.5f); Rig->ResetFocus(); UE_LOG(LogDC, Display, TEXT("[WorldTest] hour %.1f night %d"), Run->GetHourOfDay(), Run->IsNight() ? 1 : 0); Next(14.0); break;
	case 22: TestShot(TEXT("t_night_wide")); Focus(EDCRoomType::Lounge, 0.55f); Next(2.0); break;
	case 23: TestShot(TEXT("t_night_bunks")); LogPawns(TEXT("night")); Next(0.5); break;
	case 24: SkipTo(8.f); Rig->ResetFocus(); Next(10.0); break;
	case 25: TestShot(TEXT("t_morning")); LogPawns(TEXT("morning")); Next(0.5); break;
	// Station hover + click (camera focus, then the panel opens); badge on the board.
	case 26:
		for (TActorIterator<ADCStation> It(GetWorld()); It; ++It)
		{
			if (It->StationType == EDCStationType::MissionBoard) { It->OnHoverBegin(); It->OnClicked(); break; }
		}
		Next(2.0);
		break;
	case 27: TestShot(TEXT("t_station_focus")); Next(0.5); break;
	case 28:
		for (TActorIterator<ADCStation> It(GetWorld()); It; ++It) { It->OnHoverEnd(); }
		if (UDCUISubsystem* UI = UDCUISubsystem::Get(this)) { UI->ClosePanel(); }
		Next(2.0);
		break;
	case 29: TestShot(TEXT("t_station_closed")); LogPawns(TEXT("end")); Next(1.0); break;
	case 30: UE_LOG(LogDC, Display, TEXT("[WorldTest] done")); if (GEngine) { GEngine->Exec(GetWorld(), TEXT("QUIT")); } ++TestStep; break;
	default: break;
	}
}
