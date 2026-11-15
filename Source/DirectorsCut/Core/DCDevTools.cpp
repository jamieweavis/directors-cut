#include "Core/DCDevTools.h"
#include "Core/DCRunSubsystem.h"
#include "Core/DCUISubsystem.h"
#include "World/DCPlayerController.h"
#include "World/DCCameraRig.h"
#include "World/DCRoomModule.h"
#include "World/DCCriminalPawn.h"
#include "World/DCWorldDirector.h"
#include "Core/DCSiteLayout.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "DirectorsCut.h"

bool UDCDevTools::IsBuildAll() { return FParse::Param(FCommandLine::Get(), TEXT("DCBuildAll")); }
int32 UDCDevTools::GetCmdSeed() { int32 S = 0; FParse::Value(FCommandLine::Get(), TEXT("DCSeed="), S); return S; }

void UDCDevTools::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const TCHAR* Cmd = FCommandLine::Get();
	FString ShotsStr;
	if (FParse::Value(Cmd, TEXT("DCShots="), ShotsStr, false))
	{
		TArray<FString> Parts; ShotsStr.ParseIntoArray(Parts, TEXT(","));
		for (const FString& P : Parts)
		{
			TArray<FString> F; P.ParseIntoArray(F, TEXT(":"));
			FShot S; S.Name = F.IsValidIndex(0) ? F[0] : TEXT("shot"); S.Focus = F.IsValidIndex(1) ? F[1] : TEXT("wide"); S.Panel = F.IsValidIndex(2) ? F[2] : TEXT("");
			Shots.Add(S);
		}
	}
	ShotDir = FPaths::ProjectSavedDir() / TEXT("DCShots");
	FParse::Value(Cmd, TEXT("DCShotDir="), ShotDir);
	FParse::Value(Cmd, TEXT("DCShotDelay="), Delay);
	FParse::Value(Cmd, TEXT("DCQuitAfter="), QuitAfter);
	FParse::Value(Cmd, TEXT("DCAutoPlay="), AutoPlaySeconds);
	bClockFlags = FCString::Strifind(Cmd, TEXT("DCHour=")) || FCString::Strifind(Cmd, TEXT("DCTimeScale="));
	bContinue = FParse::Param(Cmd, TEXT("DCContinue"));
	FParse::Value(Cmd, TEXT("DCHireCrew="), HireCrew);
	bPerkOffer = FParse::Param(Cmd, TEXT("DCPerkOffer"));
	FParse::Value(Cmd, TEXT("DCEndRun="), EndRun);
	if (Shots.Num() || QuitAfter > 0.f || AutoPlaySeconds > 0 || bClockFlags || bContinue || HireCrew > 0 || bPerkOffer || !EndRun.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*ShotDir, true);
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDCDevTools::Tick));
		NextAt = Delay;
	}
}

void UDCDevTools::Deinitialize()
{
	if (TickHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(TickHandle); }
	Super::Deinitialize();
}

UWorld* UDCDevTools::GetGameWorld() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetWorld() : nullptr;
}

void UDCDevTools::RunAutoPlay()
{
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(GetGameWorld());
	if (!Run) { return; }
	if (!Run->HasActiveRun()) { Run->StartNewRun(GetCmdSeed()); }
	// Bot: every 5 run-seconds, start every mission it can fully staff (SuggestPlan), pick the first perk, and spend what is
	// left over a safety reserve (two paydays) on crew and rooms.
	const auto BotStep = [Run](bool bWindDown)
	{
		for (const FDCMission& M : Run->GetMissions())
		{
			// Winding down for the shots: one small job at a time, so most of the crew is home.
			if (bWindDown && (Run->GetActiveMissionCount() > 0 || M.CrewSize > 2)) { continue; }
			// The Big Score is left on the board: winning would end the run before the shots (-DCEndRun does that on request).
			if (M.bInProgress || M.bFinal || Run->GetActiveMissionCount() >= Run->GetMaxActiveMissions()) { continue; }
			const FDCCrewPlan P = Run->SuggestPlan(M.Id, 0.3f);
			const FDCSuccessBreakdown B = Run->PreviewPlan(P);
			if (B.bValid && B.SuccessChance >= 0.5f) { Run->StartMission(P); }
		}
		if (Run->HasPendingPerkOffer()) { const TArray<FDCPerk> O = Run->GetPerkOffer(); if (O.Num()) { Run->PickPerk(O[0].Id); } }
		const int32 Crew = Run->GetRoster().Num();
		const int32 Reserve = Crew < 2 ? 0 : Run->GetUpkeepDue() * 2 + 500;
		if (Crew < Run->GetRosterCap())
		{
			// Hire like a player: whoever counters the most threats on the board that the crew can't handle yet, then by level.
			TArray<EDCThreat> Open;
			for (const FDCMission& M : Run->GetMissions()) { if (!M.bInProgress) { Open.Append(M.Threats); } }
			for (const FDCCriminal& C : Run->GetRoster()) { for (EDCThreat T : Run->GetCounters(C.Id)) { Open.Remove(T); } }
			const FDCCriminal* Best = nullptr;
			int32 BestScore = -1;
			for (const FDCCriminal& C : Run->GetRecruits())
			{
				if (C.HireCost > Run->GetCash() - Reserve) { continue; }
				int32 Score = C.Level;
				for (FName Id : C.Abilities) { if (const FDCAbility* A = Run->FindAbility(Id)) { Score += 10 * Open.FilterByPredicate([A](EDCThreat T) { return T == A->Counters; }).Num(); } }
				if (Score > BestScore) { BestScore = Score; Best = &C; }
			}
			// Past the first few, only take people who add something the crew lacks.
			if (Best && (Crew < 3 || BestScore >= 10)) { Run->Hire(Best->Id); }
		}
		for (const FDCRoom& R : Run->GetRooms())
		{
			if (R.bCanAct && R.NextCost <= Run->GetCash() - Reserve && Crew >= 2) { Run->BuildRoom(R.Type); }
		}
		Run->AdvanceTime(5.0);
	};
	for (double T = 0.0; T < AutoPlaySeconds && Run->GetPhase() == EDCGamePhase::Playing; T += 5.0) { BotStep(false); }
	// -DCHour with -DCAutoPlay: keep playing until the clock reads that hour (at most one more day).
	float Hour = -1.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCHour="), Hour) && Hour >= 0.f)
	{
		const float StepHours = 5.f * Run->GetGameMinutesPerSecond() / 60.f;
		const float WindDownHours = 8.f;
		float HoursLeft = FMath::Fmod(Hour - Run->GetHourOfDay() + 24.f, 24.f);
		if (HoursLeft < WindDownHours) { HoursLeft += 24.f; } // always a full wind-down
		const int32 Steps = FMath::RoundToInt(HoursLeft / FMath::Max(0.001f, StepHours));
		const int32 WindDownSteps = FMath::RoundToInt(WindDownHours / FMath::Max(0.001f, StepHours));
		for (int32 i = 0; i < Steps && Run->GetPhase() == EDCGamePhase::Playing; ++i) { BotStep(i >= Steps - WindDownSteps); }
	}
	while (Run->HasPendingPerkOffer()) { const TArray<FDCPerk> O = Run->GetPerkOffer(); if (!O.Num() || !Run->PickPerk(O[0].Id)) { break; } }
	for (const FDCMissionReport& R : Run->GetReports())
	{
		UE_LOG(LogDC, Display, TEXT("[AutoPlay] %s -> %s (%.0f%%, roll %.2f) take $%d"), *R.Mission.DisplayName, DC::OutcomeName(R.Outcome), R.Mission.LockedChance * 100.f, R.Roll, R.DirectorTake);
	}
	int32 Built = 0;
	for (const FDCRoom& R : Run->GetRooms()) { Built += R.Level >= 1 ? 1 : 0; }
	UE_LOG(LogDC, Display, TEXT("[AutoPlay] t=%.0fs day %d %.1fh cash $%d, successes %d, roster %d, rooms %d, phase %d"), Run->GetRunTime(), Run->GetDay(), Run->GetHourOfDay(), Run->GetCash(), Run->GetMissionsSucceeded(), Run->GetRoster().Num(), Built, (int32)Run->GetPhase());
	// The world saw hours of play in one frame: rebuild it from the run (as loading a save does) instead of queueing every reveal.
	if (UWorld* W = GetGameWorld()) { for (TActorIterator<ADCWorldDirector> It(W); It; ++It) { It->DevResync(); } }
	if (UDCUISubsystem* UI = UDCUISubsystem::Get(GetGameWorld())) { UI->ClosePanel(); }
}

void UDCDevTools::ApplyEndRun()
{
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(GetGameWorld());
	if (!Run) { return; }
	if (!Run->HasActiveRun()) { Run->StartNewRun(GetCmdSeed()); }
	Run->DevEndRun(EndRun.Equals(TEXT("win"), ESearchCase::IgnoreCase));
}

void UDCDevTools::ApplyContinue()
{
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(GetGameWorld());
	if (!Run) { return; }
	const bool bHad = Run->HasSave();
	const bool bOk = bHad && Run->ContinueRun();
	UE_LOG(LogDC, Display, TEXT("[DevTools] -DCContinue: save %d, loaded %d (t=%.0fs day %d %.1fh)"), bHad ? 1 : 0, bOk ? 1 : 0, Run->GetRunTime(), Run->GetDay(), Run->GetHourOfDay());
	if (UDCUISubsystem* UI = UDCUISubsystem::Get(GetGameWorld())) { if (bOk) { UI->ClosePanel(); } }
}

void UDCDevTools::ApplyHireCrew()
{
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(GetGameWorld());
	if (!Run) { return; }
	if (!Run->HasActiveRun()) { Run->StartNewRun(GetCmdSeed()); }
	int32 Hired = 0;
	for (int32 k = 0; k < HireCrew; ++k)
	{
		const TArray<FDCCriminal>& Recruits = Run->GetRecruits();
		if (Recruits.Num() == 0 || !Run->Hire(Recruits[0].Id)) { break; }
		++Hired;
	}
	UE_LOG(LogDC, Display, TEXT("[DevTools] -DCHireCrew: hired %d / %d (roster %d, cash $%d)"), Hired, HireCrew, Run->GetRoster().Num(), Run->GetCash());
}

void UDCDevTools::ApplyClockFlags()
{
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(GetGameWorld());
	if (!Run) { return; }
	if (!Run->HasActiveRun()) { Run->StartNewRun(GetCmdSeed()); }
	float Scale = 1.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCTimeScale="), Scale)) { Run->SetTimeScale(Scale); }
	float Hour = -1.f;
	if (AutoPlaySeconds <= 0.f && FParse::Value(FCommandLine::Get(), TEXT("DCHour="), Hour) && Hour >= 0.f) // with -DCAutoPlay the bot plays up to that hour
	{
		const float DeltaH = FMath::Fmod(Hour - Run->GetHourOfDay() + 24.f, 24.f);
		Run->AdvanceTime(DeltaH * 60.0 / FMath::Max(0.01f, Run->GetGameMinutesPerSecond()));
	}
}

void UDCDevTools::PrepareShot(const FShot& Shot)
{
	UWorld* W = GetGameWorld();
	if (!W) { return; }
	ADCPlayerController* PC = Cast<ADCPlayerController>(W->GetFirstPlayerController());
	ADCCameraRig* Rig = PC ? PC->GetCameraRig() : nullptr;
	if (Rig)
	{
		// "focus@Z": Z overrides the zoom (0 = the overview distance .. 1 = closest).
		FString Focus = Shot.Focus, ZoomStr;
		float ZoomOverride = -1.f;
		if (Shot.Focus.Split(TEXT("@"), &Focus, &ZoomStr)) { ZoomOverride = FMath::Clamp(FCString::Atof(*ZoomStr), 0.f, 1.f); }
		const auto Is = [&Focus](const TCHAR* Name) { return Focus.Equals(Name, ESearchCase::IgnoreCase); };
		const auto Go = [Rig, ZoomOverride](const FVector& Point, float Zoom) { Rig->FocusOn(Point, ZoomOverride >= 0.f ? ZoomOverride : Zoom); };
		if (Is(TEXT("wide"))) { if (ZoomOverride >= 0.f) { Rig->FocusOn(Rig->GetOverviewPivot(), ZoomOverride); } else { Rig->ResetFocus(); } }
		else if (Is(TEXT("yard")))
		{
			// The parked cars, the driveway and the front of the house.
			const FVector2D C = DCSite::YardRect().GetCenter();
			Go(FVector(C.X, C.Y - 150.f, 0.f), 0.3f);
		}
		else if (Is(TEXT("house")))
		{
			FBox B(ForceInit);
			for (TActorIterator<ADCRoomModule> It(W); It; ++It) { if (It->GetState() != EDCRoomState::Locked || IsBuildAll()) { B += It->GetWorldBounds(); } }
			Go(B.IsValid ? B.GetCenter() * FVector(1, 1, 0) : FVector::ZeroVector, 0.25f);
		}
		else if (Is(TEXT("far"))) { Rig->ResetFocus(); Rig->Zoom(-30.f); }
		else if (Is(TEXT("crew")))
		{
			// Close-up on a crew member (a sleeping one if any).
			ADCCriminalPawn* Best = nullptr;
			for (TActorIterator<ADCCriminalPawn> It(W); It; ++It)
			{
				if (It->IsHidden()) { continue; }
				if (!Best || (It->IsSleeping() && !Best->IsSleeping())) { Best = *It; }
			}
			if (Best) { Go(Best->GetActorLocation(), 1.f); }
		}
		else
		{
			const UEnum* E = StaticEnum<EDCRoomType>();
			FString Name = Focus;
			if (Is(TEXT("Bunkroom"))) { Name = TEXT("Lounge"); }
			if (Is(TEXT("Lawyer"))) { Name = TEXT("LawyerOffice"); }
			if (Is(TEXT("Ops"))) { Name = TEXT("OpsRoom"); }
			const int64 V = E->GetValueByNameString(Name);
			if (V == INDEX_NONE) { UE_LOG(LogDC, Warning, TEXT("[DCShots] unknown focus '%s'"), *Shot.Focus); }
			for (TActorIterator<ADCRoomModule> It(W); It; ++It)
			{
				if ((int64)It->RoomType == V) { Go(It->GetFocusPoint(), 0.7f); }
			}
		}
	}
	if (UDCUISubsystem* UI = UDCUISubsystem::Get(W))
	{
		if (!Shot.Panel.IsEmpty())
		{
			const int64 P = StaticEnum<EDCPanel>()->GetValueByNameString(Shot.Panel);
			const UDCRunSubsystem* RunS = UDCRunSubsystem::Get(W);
			const FDCMission* First = nullptr;
			if (RunS) { for (const FDCMission& M : RunS->GetMissions()) { if (!M.bInProgress) { First = &M; break; } } }
			if (P == (int64)EDCPanel::MissionPlanner && First) { UI->OpenMissionPlanner(First->Id); }
			else if (P != INDEX_NONE) { UI->OpenPanel((EDCPanel)P); }
		}
		else { UI->ClosePanel(); }
	}
}

bool UDCDevTools::Tick(float DeltaTime)
{
	Elapsed += DeltaTime;
	// Run-setup flags apply as soon as the game world is up (independent of -DCShotDelay).
	const bool bWorldUp = GetGameWorld() && GetGameWorld()->HasBegunPlay();
	if (bContinue && bWorldUp) { ApplyContinue(); bContinue = false; }
	if (bClockFlags && bWorldUp && Elapsed > 0.5) { ApplyClockFlags(); bClockFlags = false; }
	if (HireCrew > 0 && bWorldUp && Elapsed > 1.0) { ApplyHireCrew(); HireCrew = 0; }
	if (bPerkOffer && bWorldUp && Elapsed > 1.0)
	{
		if (UDCRunSubsystem* Run = UDCRunSubsystem::Get(GetGameWorld())) { if (Run->HasActiveRun()) { Run->DevOfferPerks(); } }
		bPerkOffer = false;
	}
	if (Elapsed < NextAt) { return true; }
	// Give the rebuilt world a few seconds of real time (crew walk to their spots, lights settle) before the first shot.
	if (Step == 0 && AutoPlaySeconds > 0) { RunAutoPlay(); AutoPlaySeconds = 0; NextAt = Elapsed + (Shots.Num() ? 4.0 : 1.0); return true; }
	if (!EndRun.IsEmpty()) { ApplyEndRun(); EndRun.Reset(); NextAt = Elapsed + 1.0; return true; }

	if (Shots.IsValidIndex(Step))
	{
		if (!bPreparedShot)
		{
			PrepareShot(Shots[Step]);
			bPreparedShot = true;
			NextAt = Elapsed + 2.5; // let camera ease & UI animate
			return true;
		}
		const FString File = ShotDir / (Shots[Step].Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(File, true, false);
		UE_LOG(LogDC, Display, TEXT("[DCShots] %s"), *File);
		bPreparedShot = false;
		++Step;
		NextAt = Elapsed + 0.75;
		return true;
	}
	if (Shots.Num() > 0 || (QuitAfter > 0.f && Elapsed >= QuitAfter))
	{
		UE_LOG(LogDC, Display, TEXT("[DCShots] done, quitting"));
		if (GEngine) { GEngine->Exec(GetGameWorld(), TEXT("QUIT")); }
		return false;
	}
	return true;
}
