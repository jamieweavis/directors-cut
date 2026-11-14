// Director's Cut — the run save (Agent A). A full copy of UDCRunSubsystem's run state; time is frozen while closed.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Core/DCTypes.h"
#include "DCSaveGame.generated.h"

UCLASS()
class DIRECTORSCUT_API UDCSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 2;   // v2: slots, finished runs, RunId (v1 still loads)
	static constexpr int32 MinVersion = 1;

	UPROPERTY() int32 Version = CurrentVersion;
	UPROPERTY() int32 Seed = 0;
	UPROPERTY() int32 RngState = 0;
	UPROPERTY() double RunTime = 0.0;
	UPROPERTY() int32 Cash = 0;
	UPROPERTY() int32 NextId = 1;
	UPROPERTY() float TimeScale = 1.f;

	UPROPERTY() TArray<FDCCriminal> Roster;
	UPROPERTY() TArray<FDCCriminal> Fallen;
	UPROPERTY() TArray<FDCCriminal> Recruits;
	UPROPERTY() TArray<FDCMission> Missions;
	UPROPERTY() TArray<FDCMissionReport> Reports;
	UPROPERTY() TArray<FName> Stash;
	UPROPERTY() TArray<FDCRoom> Rooms;
	UPROPERTY() TArray<FName> PerkOffer;
	UPROPERTY() TArray<FName> PerksTaken;

	UPROPERTY() double NextPostAt = 0.0;
	UPROPERTY() double RecruitRefreshAt = 0.0;
	UPROPERTY() double NextAutosaveAt = 0.0;
	UPROPERTY() int32 LastDawnIndex = 0;
	UPROPERTY() int32 MissionsSucceeded = 0;
	UPROPERTY() int32 MissionsAttempted = 0;
	UPROPERTY() int32 TotalTake = 0;
	UPROPERTY() int32 BestMissionTake = 0;
	UPROPERTY() FString BestMissionName;
	UPROPERTY() int32 RoomsBuilt = 0;
	UPROPERTY() int32 PendingPerkOffers = 0;
	UPROPERTY() bool bArrestPreventUsed = false;
	UPROPERTY() TMap<int32, double> ArrestedAt;

	// v2
	UPROPERTY() FGuid RunId;
	UPROPERTY() FDateTime SavedAt;          // UTC
	UPROPERTY() bool bFinished = false;     // game over or won: loads read-only into the end phase
	UPROPERTY() bool bWon = false;
	UPROPERTY() FString EndReason;
	UPROPERTY() FDCRunSummary Summary;      // as shown at the end (informational; GetRunSummary recomputes it)
};

/** Persistent leaderboard of finished runs (slot "DCLeaderboard"). */
UCLASS()
class DIRECTORSCUT_API UDCLeaderboardSave : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxRecords = 50;
	UPROPERTY() TArray<FDCRunRecord> Records;
	UPROPERTY() TArray<FGuid> RunIds;        // parallel to Records (dedupe)
};
