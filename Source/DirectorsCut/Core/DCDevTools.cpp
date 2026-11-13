#include "Core/DCDevTools.h"
#include "Core/DCRunSubsystem.h"
#include "Core/DCUISubsystem.h"
#include "World/DCPlayerController.h"
#include "World/DCCameraRig.h"
#include "World/DCRoomModule.h"
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
	if (Shots.Num() || QuitAfter > 0.f || AutoPlaySeconds > 0 || bClockFlags || bContinue)
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
	// Bot: every 5 run-seconds, start every mission it can fully staff (SuggestPlan), pick the first perk, hire if broke on crew.
	for (double T = 0.0; T < AutoPlaySeconds && Run->GetPhase() == EDCGamePhase::Playing; T += 5.0)
	{
		for (const FDCMission& M : Run->GetMissions())
		{
			if (M.bInProgress || Run->GetActiveMissionCount() >= Run->GetMaxActiveMissions()) { continue; }
			const FDCCrewPlan P = Run->SuggestPlan(M.Id, 0.3f);
			const FDCSuccessBreakdown B = Run->PreviewPlan(P);
			if (B.bValid && B.SuccessChance >= 0.5f) { Run->StartMission(P); }
		}
		if (Run->HasPendingPerkOffer()) { const TArray<FDCPerk> O = Run->GetPerkOffer(); if (O.Num()) { Run->PickPerk(O[0].Id); } }
		Run->AdvanceTime(5.0);
	}
	for (const FDCMissionReport& R : Run->GetReports())
	{
		UE_LOG(LogDC, Display, TEXT("[AutoPlay] %s -> %s (%.0f%%, roll %.2f) take $%d"), *R.Mission.DisplayName, DC::OutcomeName(R.Outcome), R.Mission.LockedChance * 100.f, R.Roll, R.DirectorTake);
	}
	UE_LOG(LogDC, Display, TEXT("[AutoPlay] t=%.0fs day %d %.1fh cash $%d, successes %d, roster %d, phase %d"), Run->GetRunTime(), Run->GetDay(), Run->GetHourOfDay(), Run->GetCash(), Run->GetMissionsSucceeded(), Run->GetRoster().Num(), (int32)Run->GetPhase());
	if (UDCUISubsystem* UI = UDCUISubsystem::Get(GetGameWorld())) { UI->ClosePanel(); }
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

void UDCDevTools::ApplyClockFlags()
{
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(GetGameWorld());
	if (!Run) { return; }
	if (!Run->HasActiveRun()) { Run->StartNewRun(GetCmdSeed()); }
	float Scale = 1.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCTimeScale="), Scale)) { Run->SetTimeScale(Scale); }
	float Hour = -1.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCHour="), Hour) && Hour >= 0.f)
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
		if (Shot.Focus.Equals(TEXT("wide"), ESearchCase::IgnoreCase)) { Rig->ResetFocus(); }
		else
		{
			const UEnum* E = StaticEnum<EDCRoomType>();
			FString Name = Shot.Focus;
			if (Name.Equals(TEXT("Bunkroom"), ESearchCase::IgnoreCase)) { Name = TEXT("Lounge"); }
			if (Name.Equals(TEXT("Lawyer"), ESearchCase::IgnoreCase)) { Name = TEXT("LawyerOffice"); }
			if (Name.Equals(TEXT("Ops"), ESearchCase::IgnoreCase)) { Name = TEXT("OpsRoom"); }
			const int64 V = E->GetValueByNameString(Name);
			if (V == INDEX_NONE) { UE_LOG(LogDC, Warning, TEXT("[DCShots] unknown focus '%s'"), *Shot.Focus); }
			for (TActorIterator<ADCRoomModule> It(W); It; ++It)
			{
				if ((int64)It->RoomType == V) { Rig->FocusOn(It->GetFocusPoint(), 0.7f); }
			}
		}
	}
	if (UDCUISubsystem* UI = UDCUISubsystem::Get(W))
	{
		if (!Shot.Panel.IsEmpty())
		{
			const int64 P = StaticEnum<EDCPanel>()->GetValueByNameString(Shot.Panel);
			if (P != INDEX_NONE) { UI->OpenPanel((EDCPanel)P); }
		}
		else { UI->ClosePanel(); }
	}
}

bool UDCDevTools::Tick(float DeltaTime)
{
	Elapsed += DeltaTime;
	if (Elapsed < NextAt) { return true; }

	if (bContinue && GetGameWorld() && GetGameWorld()->HasBegunPlay()) { ApplyContinue(); bContinue = false; }
	if (bClockFlags && Elapsed > 0.5) { ApplyClockFlags(); bClockFlags = false; }
	if (Step == 0 && AutoPlaySeconds > 0) { RunAutoPlay(); AutoPlaySeconds = 0; NextAt = Elapsed + 1.0; return true; }

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
