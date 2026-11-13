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
	FParse::Value(Cmd, TEXT("DCAutoPlay="), AutoPlayNights);
	if (Shots.Num() || QuitAfter > 0.f || AutoPlayNights > 0)
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
	for (int32 N = 0; N < AutoPlayNights && Run->GetPhase() == EDCGamePhase::Planning; ++N)
	{
		// Greedy: for each job, crew = required roles filled by best available, cut 0.3.
		for (const FDCJob& J : Run->GetJobs())
		{
			if (Run->GetCommittedPlans().Num() >= Run->GetMaxCrewsPerNight()) { break; }
			FDCCrewPlan P; P.JobId = J.Id; P.Cut = 0.3f;
			for (EDCCriminalType Role : J.RequiredRoles)
			{
				for (const FDCCriminal& C : Run->GetRoster())
				{
					if (C.IsAvailable() && C.Type == Role && !P.CriminalIds.Contains(C.Id)) { P.CriminalIds.Add(C.Id); break; }
				}
			}
			FString Err;
			Run->CommitPlan(P, &Err);
		}
		const TArray<FDCHeistResult> Res = Run->EndNight();
		for (const FDCHeistResult& R : Res)
		{
			UE_LOG(LogDC, Display, TEXT("[AutoPlay] Night %d: %s -> %s (%.0f%%, roll %.2f) take $%d"), Run->GetNight() - 1, *R.JobName, DC::OutcomeStamp(R.Outcome), R.SuccessChance * 100.f, R.Roll, R.DirectorTake);
		}
		if (Run->HasPendingPerkOffer()) { const TArray<FDCPerk> O = Run->GetPerkOffer(); if (O.Num()) { Run->PickPerk(O[0].Id); } }
		Run->FinishDailies();
		UE_LOG(LogDC, Display, TEXT("[AutoPlay] Cash $%d, heists %d, roster %d, phase %d"), Run->GetCash(), Run->GetHeistsCompleted(), Run->GetRoster().Num(), (int32)Run->GetPhase());
	}
	if (UDCUISubsystem* UI = UDCUISubsystem::Get(GetGameWorld())) { UI->ClosePanel(); }
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
			const int64 V = E->GetValueByNameString(Shot.Focus);
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

	if (Step == 0 && AutoPlayNights > 0) { RunAutoPlay(); AutoPlayNights = 0; NextAt = Elapsed + 1.0; return true; }

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
