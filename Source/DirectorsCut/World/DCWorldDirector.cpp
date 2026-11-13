#include "World/DCWorldDirector.h"
#include "World/DCPlayerController.h"
#include "World/DCCameraRig.h"
#include "World/DCRoomModule.h"
#include "World/DCCriminalPawn.h"
#include "World/DCCriminalSpawnZone.h"
#include "World/DCGarageDoor.h"
#include "World/DCStation.h"
#include "Core/DCRunSubsystem.h"
#include "Core/DCDevTools.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Core/DCUISubsystem.h"
#include "Camera/CameraComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Engine/Engine.h"
#include "DirectorsCut.h"

ADCWorldDirector::ADCWorldDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
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
		HNight = Run->OnMissionStarted.AddUObject(this, &ADCWorldDirector::OnMissionStarted);
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
	UpdateBounds();
}

void ADCWorldDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDCRunSubsystem* Run = UDCRunSubsystem::Get(this))
	{
		Run->OnRunStateChanged.Remove(HState);
		Run->OnPhaseChanged.Remove(HPhase);
		Run->OnMissionStarted.Remove(HNight);
		Run->OnMissionCompleted.Remove(HDone);
		Run->OnRoomChanged.Remove(HRoom);
		Run->OnCriminalLevelUp.Remove(HLevel);
		Run->OnCriminalHired.Remove(HHired);
	}
	Super::EndPlay(Reason);
}

void ADCWorldDirector::GatherWorld()
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	Rooms.Reset(); Zones.Reset(); Doors.Reset();
	for (TActorIterator<ADCRoomModule> It(W); It; ++It) { Rooms.Add(*It); }
	for (TActorIterator<ADCCriminalSpawnZone> It(W); It; ++It) { Zones.Add(*It); }
	for (TActorIterator<ADCGarageDoor> It(W); It; ++It) { Doors.Add(*It); }
	UE_LOG(LogDC, Log, TEXT("WorldDirector: %d rooms, %d spawn zones, %d doors"), Rooms.Num(), Zones.Num(), Doors.Num());
}

ADCRoomModule* ADCWorldDirector::FindRoom(EDCRoomType Type) const
{
	for (const TWeakObjectPtr<ADCRoomModule>& R : Rooms) { if (R.IsValid() && R->RoomType == Type) { return R.Get(); } }
	return nullptr;
}

ADCCriminalPawn* ADCWorldDirector::FindPawn(int32 CriminalId) const
{
	for (ADCCriminalPawn* P : Pawns) { if (P && P->GetCriminalId() == CriminalId) { return P; } }
	return nullptr;
}

static bool IsRoomVisiblyBuilt(const ADCRoomModule* R)
{
	return R && (R->GetState() == EDCRoomState::Built || R->IsRevealPlaying());
}

ADCGarageDoor* ADCWorldDirector::GetActiveDoor() const
{
	const bool bGarage = IsRoomVisiblyBuilt(FindRoom(EDCRoomType::Garage));
	ADCGarageDoor* Office = nullptr;
	ADCGarageDoor* Any = nullptr;
	for (const TWeakObjectPtr<ADCGarageDoor>& D : Doors)
	{
		if (!D.IsValid()) { continue; }
		if (bGarage && D->RoomType == EDCRoomType::Garage) { return D.Get(); }
		if (!Office && D->RoomType == EDCRoomType::Office) { Office = D.Get(); }
		if (!Any) { Any = D.Get(); }
	}
	return Office ? Office : Any;
}

ADCCriminalSpawnZone* ADCWorldDirector::GetActiveZone() const
{
	const bool bLounge = IsRoomVisiblyBuilt(FindRoom(EDCRoomType::Lounge)) && !FindRoom(EDCRoomType::Lounge)->IsRevealPlaying();
	ADCCriminalSpawnZone* Office = nullptr;
	ADCCriminalSpawnZone* Any = nullptr;
	for (const TWeakObjectPtr<ADCCriminalSpawnZone>& Z : Zones)
	{
		if (!Z.IsValid()) { continue; }
		if (bLounge && Z->RoomType == EDCRoomType::Lounge) { return Z.Get(); }
		if (!Office && Z->RoomType == EDCRoomType::Office) { Office = Z.Get(); }
		if (!Any) { Any = Z.Get(); }
	}
	return Office ? Office : Any;
}

// ─── Run events ──────────────────────────────────────────────────────────────

bool ADCWorldDirector::IsDetectingNewRun()
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->HasActiveRun()) { return false; }
	if (!bHadRun) { return true; }
	return Run->GetSeed() != LastSeed || Run->GetRunTime() < LastRunTime;
}

void ADCWorldDirector::OnPhaseChanged(EDCGamePhase Phase)
{
	const EDCGamePhase Prev = LastPhase;
	LastPhase = Phase;
	if (Phase == EDCGamePhase::Playing && (Prev == EDCGamePhase::Title || Prev == EDCGamePhase::GameOver || Prev == EDCGamePhase::Won || IsDetectingNewRun()))
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
	SyncRooms(false);
	SyncPawns();
}

void ADCWorldDirector::OnMissionStarted(const FDCMission& Mission)
{
	// R0: crew walk out to the garage when a mission starts (Agent E: cars, staggering, polish).
	ADCGarageDoor* Door = GetActiveDoor();
	float Delay = 0.3f;
	for (int32 Id : Mission.CrewIds)
	{
		if (ADCCriminalPawn* P = FindPawn(Id)) { P->Depart(Door, Delay, false); Delay += 0.55f; }
	}
}

void ADCWorldDirector::OnMissionCompleted(const FDCMissionReport& Report)
{
	ADCGarageDoor* Door = GetActiveDoor();
	float Delay = 0.4f;
	for (const FDCCrewMemberResult& M : Report.Members)
	{
		ADCCriminalPawn* P = FindPawn(M.CriminalId);
		if (!P) { continue; }
		if (M.bArrested) { P->MarkLeaveForever(); continue; }
		P->ReturnHome(Door, Delay);
		Delay += 0.7f;
	}
}

void ADCWorldDirector::OnRoomChanged(const FDCRoom& Room)
{
	if (UDCDevTools::IsBuildAll()) { return; }
	ADCRoomModule* M = FindRoom(Room.Type);
	if (!M) { return; }
	const bool bUpgrade = Room.State == EDCRoomState::Built && (M->GetState() != EDCRoomState::Built || Room.Level > M->GetLevel());
	if (bUpgrade)
	{
		RevealQueue.AddUnique(Room.Type);
	}
	else if (!RevealQueue.Contains(Room.Type) && RevealingRoom.Get() != M && Room.State != EDCRoomState::Building)
	{
		M->ApplyState(Room.State, Room.Level);
	}
}

void ADCWorldDirector::OnCriminalLevelUp(int32 CriminalId, int32 NewLevel)
{
	if (ADCCriminalPawn* P = FindPawn(CriminalId)) { P->PlayLevelUp(NewLevel); }
}

void ADCWorldDirector::OnCriminalHired(const FDCCriminal& C)
{
	if (ADCCriminalPawn* Existing = FindPawn(C.Id))
	{
		// Already spawned by an earlier state sync: send it round to the door so it walks in.
		Existing->SetAway();
		Existing->ReturnHome(GetActiveDoor(), 0.4f);
		return;
	}
	if (Pawns.Num() < MaxPawns) { SpawnPawn(C, true); }
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

	for (ADCCriminalPawn* P : Pawns) { if (P) { P->Destroy(); } }
	Pawns.Reset();
	PendingHires.Reset();
	RevealQueue.Reset();
	RevealingRoom = nullptr;

	SyncRooms(false);
	UpdateBounds();
	if (PC.IsValid() && PC->GetCameraRig()) { PC->GetCameraRig()->ResetFocus(); }
	for (const FDCCriminal& C : Run->GetRoster())
	{
		if (Pawns.Num() >= MaxPawns) { break; }
		SpawnPawn(C, false);
	}
	UE_LOG(LogDC, Log, TEXT("WorldDirector: new run (seed %d), %d pawns"), LastSeed, Pawns.Num());
}

void ADCWorldDirector::SyncRooms(bool bAllowReveal)
{
	if (UDCDevTools::IsBuildAll()) { return; }
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->HasActiveRun()) { return; }
	for (const TWeakObjectPtr<ADCRoomModule>& RW : Rooms)
	{
		ADCRoomModule* M = RW.Get();
		if (!M || M->IsRevealPlaying() || RevealQueue.Contains(M->RoomType)) { continue; }
		const FDCRoom R = Run->GetRoom(M->RoomType);
		if (R.State == EDCRoomState::Building) { continue; }
		if (R.State != M->GetState() || (R.State == EDCRoomState::Built && R.Level != M->GetLevel()))
		{
			const bool bReveal = bAllowReveal && R.State == EDCRoomState::Built && (M->GetState() != EDCRoomState::Built || R.Level > M->GetLevel());
			if (bReveal) { RevealQueue.AddUnique(M->RoomType); }
			else { M->ApplyState(R.State, R.Level); }
		}
	}
}

ADCCriminalPawn* ADCWorldDirector::SpawnPawn(const FDCCriminal& C, bool bWalkIn)
{
	UWorld* W = GetWorld();
	if (!W) { return nullptr; }
	ADCCriminalSpawnZone* Zone = GetActiveZone();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector At = Zone ? Zone->GetActorLocation() + FVector(0, 0, 100) : FVector(0, 0, 100);
	ADCCriminalPawn* P = W->SpawnActor<ADCCriminalPawn>(ADCCriminalPawn::StaticClass(), FTransform(At), SP);
	if (!P) { return nullptr; }
	P->InitFromCriminal(C);
	P->SetHomeZone(Zone);
	P->PlaceInZone();
	if (bWalkIn)
	{
		P->SetAway();
		P->ReturnHome(GetActiveDoor(), 0.4f);
	}
	Pawns.Add(P);
	return P;
}

void ADCWorldDirector::SyncPawns()
{
	const UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	if (!Run || !Run->HasActiveRun()) { return; }
	const TArray<FDCCriminal>& Roster = Run->GetRoster();
	ADCGarageDoor* Door = GetActiveDoor();

	// Gone from the roster: fired (walks out now) or arrested (never comes back).
	for (int32 i = Pawns.Num() - 1; i >= 0; --i)
	{
		ADCCriminalPawn* P = Pawns[i];
		if (!P) { Pawns.RemoveAt(i); continue; }
		const bool bInRoster = Roster.ContainsByPredicate([P](const FDCCriminal& C) { return C.Id == P->GetCriminalId(); });
		if (bInRoster) { continue; }
		if (P->IsAway()) { P->Destroy(); Pawns.RemoveAt(i); continue; }
		if (P->GetMode() == EDCPawnMode::WalkOut) { P->MarkLeaveForever(); continue; }
		if (!P->IsLeavingForever()) { P->Depart(Door, 0.f, true); }
	}

	for (const FDCCriminal& C : Roster)
	{
		if (ADCCriminalPawn* P = FindPawn(C.Id)) { P->UpdateFromCriminal(C); }
		else if (Pawns.Num() < MaxPawns) { SpawnPawn(C, false); }
	}

}

void ADCWorldDirector::UpdateBounds()
{
	ADCCameraRig* Rig = PC.IsValid() ? PC->GetCameraRig() : nullptr;
	if (!Rig) { return; }
	FBox B(ForceInit);
	for (const TWeakObjectPtr<ADCRoomModule>& R : Rooms)
	{
		if (IsRoomVisiblyBuilt(R.Get()) || (R.IsValid() && R->GetState() == EDCRoomState::Building)) { B += R->GetWorldBounds(); }
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
			if (P && P->GetActiveDoor() == D && P->IsNearDoor(420.f)) { bNear = true; break; }
		}
		float& Idle = DoorIdleTime.FindOrAdd(D);
		if (bNear) { Idle = 0.f; D->Open(); }
		else
		{
			Idle += Dt;
			if (Idle > 0.7f) { D->Close(); }
		}
	}
}

void ADCWorldDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Rooms.Num() == 0) { GatherWorld(); if (Rooms.Num()) { UpdateBounds(); } }

	// One reveal at a time.
	ADCRoomModule* Revealing = RevealingRoom.Get();
	if (Revealing && !Revealing->IsRevealPlaying())
	{
		RevealingRoom = nullptr;
		Revealing = nullptr;
		UpdateBounds();
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

	// Pawns follow the best hangout (Lounge once built).
	ADCCriminalSpawnZone* Zone = GetActiveZone();
	for (int32 i = Pawns.Num() - 1; i >= 0; --i)
	{
		ADCCriminalPawn* P = Pawns[i];
		if (!P) { Pawns.RemoveAt(i); continue; }
		if (P->WantsDestroy()) { P->Destroy(); Pawns.RemoveAt(i); continue; }
		P->SetHomeZone(Zone);
	}
	UpdateDoors(DeltaSeconds);
	if (bWorldTest) { TickWorldTest(DeltaSeconds); }
}

void ADCWorldDirector::TestShot(const FString& Name)
{
	const FString File = TestShotDir / (Name + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(File, true, false);
	UE_LOG(LogDC, Display, TEXT("[WorldTest] shot %s"), *File);
}

void ADCWorldDirector::TickWorldTest(float Dt)
{
	TestTime += Dt;
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(this);
	ADCCameraRig* Rig = PC.IsValid() ? PC->GetCameraRig() : nullptr;
	if (!Run || !Rig) { return; }
	float EV = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCWorldTestEV="), EV))
	{
		Rig->Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
		Rig->Camera->PostProcessSettings.AutoExposureBias = EV;
	}
	auto At = [this](double T) { return TestTime >= T; };
	auto LogPawns = [this](const TCHAR* Tag)
	{
		for (ADCCriminalPawn* P : Pawns)
		{
			if (!P) { continue; }
			UE_LOG(LogDC, Display, TEXT("[WorldTest] %s pawn %d mode %d hidden %d loc %s vel %.0f"), Tag, P->GetCriminalId(), (int32)P->GetMode(), P->IsHidden() ? 1 : 0, *P->GetActorLocation().ToCompactString(), P->GetVelocity().Size2D());
		}
	};
	auto FocusFirstPawn = [&](float Zoom)
	{
		for (ADCCriminalPawn* P : Pawns) { if (P && !P->IsHidden()) { Rig->FocusOn(P->GetActorLocation(), Zoom); return; } }
	};
	switch (TestStep)
	{
	case 0: if (At(6.0)) { LogPawns(TEXT("start")); FocusFirstPawn(1.f); ++TestStep; } break;
	case 1: if (At(8.0)) { TestShot(TEXT("t_pawn_close")); ++TestStep; } break;
	case 2: if (At(9.0)) { Rig->ResetFocus(); const bool bOk = Run->BuildRoom(EDCRoomType::Lounge); Run->AdvanceTime(60.0); UE_LOG(LogDC, Display, TEXT("[WorldTest] BuildRoom Lounge: %d"), bOk ? 1 : 0); ++TestStep; } break;
	case 3: if (At(9.7)) { TestShot(TEXT("t_reveal_a")); ++TestStep; } break;
	case 4: if (At(10.6)) { TestShot(TEXT("t_reveal_b")); ++TestStep; } break;
	case 5: if (At(13.5)) { TestShot(TEXT("t_after_reveal")); LogPawns(TEXT("after_reveal")); ++TestStep; } break;
	case 6: if (At(15.0))
		{
			for (const FDCMission& M : Run->GetMissions())
			{
				if (!M.bInProgress && Run->GetActiveMissionCount() < Run->GetMaxActiveMissions()) { Run->StartMission(Run->SuggestPlan(M.Id)); }
			}
			UE_LOG(LogDC, Display, TEXT("[WorldTest] active missions %d"), Run->GetActiveMissionCount());
			++TestStep;
		}
		break;
	case 7: if (At(16.5)) { TestShot(TEXT("t_departing")); LogPawns(TEXT("departing")); ++TestStep; } break;
	case 8: if (At(21.0)) { LogPawns(TEXT("away")); Run->AdvanceTime(1200.0); ++TestStep; } break;
	case 9: if (At(23.0)) { TestShot(TEXT("t_returning")); LogPawns(TEXT("returning")); ++TestStep; } break;
	case 10: if (At(25.0))
		{
			if (Run->GetRoster().Num() > 0) { Run->Dismiss(Run->GetRoster()[0].Id); }
			++TestStep;
		}
		break;
	case 11: if (At(26.5)) { LogPawns(TEXT("fired")); TestShot(TEXT("t_fired")); ++TestStep; } break;
	case 12: if (At(27.0))
		{
			if (Run->GetRecruits().Num() > 0) { FString E; const bool bOk = Run->Hire(Run->GetRecruits()[0].Id, &E); UE_LOG(LogDC, Display, TEXT("[WorldTest] Hire %d: %s"), bOk ? 1 : 0, *E); }
			++TestStep;
		}
		break;
	case 13: if (At(29.0)) { TestShot(TEXT("t_hired")); LogPawns(TEXT("hired")); ++TestStep; } break;
	case 14: if (At(33.0))
		{
			for (ADCCriminalPawn* P : Pawns) { if (P && !P->IsHidden()) { Run->OnCriminalLevelUp.Broadcast(P->GetCriminalId(), 4); Rig->FocusOn(P->GetActorLocation(), 0.9f); break; } }
			++TestStep;
		}
		break;
	case 15: if (At(33.5)) { TestShot(TEXT("t_levelup")); ++TestStep; } break;
	case 16: if (At(36.0))
		{
			// Station hover + click (focus, then panel opens).
			for (TActorIterator<ADCStation> It(GetWorld()); It; ++It)
			{
				if (It->StationType == EDCStationType::MissionBoard) { It->OnHoverBegin(); It->OnClicked(); break; }
			}
			++TestStep;
		}
		break;
	case 17: if (At(38.0)) { TestShot(TEXT("t_station_focus")); ++TestStep; } break;
	case 18: if (At(38.5))
		{
			for (TActorIterator<ADCStation> It(GetWorld()); It; ++It) { It->OnHoverEnd(); }
			if (UDCUISubsystem* UI = UDCUISubsystem::Get(this)) { UI->ClosePanel(); }
			++TestStep;
		}
		break;
	case 19: if (At(40.5)) { TestShot(TEXT("t_station_closed")); LogPawns(TEXT("end")); ++TestStep; } break;
	case 20: if (At(41.5)) { UE_LOG(LogDC, Display, TEXT("[WorldTest] done")); if (GEngine) { GEngine->Exec(GetWorld(), TEXT("QUIT")); } ++TestStep; } break;
	default: break;
	}
}
