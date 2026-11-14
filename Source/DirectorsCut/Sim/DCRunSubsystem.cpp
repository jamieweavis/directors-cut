// Director's Cut — UDCRunSubsystem: the realtime crew & missions simulation (Agent A).
// Data: Content/Data/*.json (schema.md). Internals in Sim/DCSimImpl.*. Save: Sim/DCSaveGame.*.
#include "Core/DCRunSubsystem.h"
#include "Sim/DCSimImpl.h"
#include "Sim/DCSimData.h"
#include "Sim/DCSaveGame.h"
#include "DirectorsCut.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

using DCImpl = FDCSimImpl;

namespace
{
	const FName K_rosterCap(TEXT("rosterCap")), K_maxActive(TEXT("maxActive")), K_gearSlots(TEXT("gearSlots")), K_gearTier(TEXT("gearTier")),
		K_upkeepMult(TEXT("upkeepMult")), K_patchUp(TEXT("patchUp")), K_bail(TEXT("bail")), K_unlockTier(TEXT("unlockTier"));
	constexpr int32 BaseRosterCap = 4;
	constexpr int32 BaseMaxActive = 1;

	template <typename T>
	const T* FindById(const TArray<T>& A, int32 Id) { return A.FindByPredicate([Id](const T& X) { return X.Id == Id; }); }
}

UDCRunSubsystem* UDCRunSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = (WorldContext && GEngine) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UDCRunSubsystem>() : nullptr;
}

void UDCRunSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	EnsureData();
	float Scale = 1.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCTimeScale="), Scale)) { TimeScale = FMath::Max(0.f, Scale); }
}

void UDCRunSubsystem::Deinitialize()
{
	if (bActive && Phase == EDCGamePhase::Playing) { SaveRun(); }
	bActive = false;
	Super::Deinitialize();
}

TStatId UDCRunSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UDCRunSubsystem, STATGROUP_Tickables); }
bool UDCRunSubsystem::IsTickable() const { return bActive && !bPaused && Phase == EDCGamePhase::Playing && !IsTemplate(); }

// ─── private helpers ────────────────────────────────────────────────────────

void UDCRunSubsystem::EnsureData()
{
	if (!Data.IsValid() || (!DataDirOverride.IsEmpty() && Data->SourceDir != DataDirOverride))
	{
		Data = MakeShared<FDCSimData>();
		Data->Load(DataDirOverride);
	}
}

void UDCRunSubsystem::ResetRunState()
{
	bActive = false; bPaused = false; Phase = EDCGamePhase::Title;
	Seed = 0; RunTime = 0.0; Cash = 0; NextId = 1;
	Roster.Reset(); Fallen.Reset(); Recruits.Reset(); Missions.Reset(); Reports.Reset(); Stash.Reset(); Rooms.Reset();
	PerkOffer.Reset(); PerksTaken.Reset();
	NextPostAt = RecruitRefreshAt = NextAutosaveAt = 0.0;
	LastDawnIndex = 0; MissionsSucceeded = MissionsAttempted = TotalTake = BestMissionTake = RoomsBuilt = PendingPerkOffers = 0;
	BestMissionName.Reset(); EndReason.Reset();
	bArrestPreventUsed = false; ArrestedAt.Reset();
	bSaveDirty = false;
}

void UDCRunSubsystem::MarkChanged() { bStateDirty = true; }

void UDCRunSubsystem::FlushChanged()
{
	if (bStateDirty && AdvanceDepth == 0)
	{
		bStateDirty = false;
		OnRunStateChanged.Broadcast();
	}
}

FString UDCRunSubsystem::GetSlotName() const { return SaveSlotOverride.IsEmpty() ? FString(TEXT("DCRun")) : SaveSlotOverride; }

// ─── lifecycle ──────────────────────────────────────────────────────────────

void UDCRunSubsystem::StartNewRun(int32 InSeed)
{
	EnsureData();
	ResetRunState();
	const FDCBalance& B = Data->Balance;
	Seed = InSeed != 0 ? InSeed : FMath::RandRange(1, 999999);
	Rng.Initialize(Seed);
	bActive = true;
	Phase = EDCGamePhase::Playing;
	Cash = B.StartingCash;

	for (int32 i = 0; i < (int32)EDCRoomType::MAX; ++i)
	{
		FDCRoom R;
		R.Type = (EDCRoomType)i;
		R.State = R.Type == EDCRoomType::Office ? EDCRoomState::Built : EDCRoomState::Locked;
		R.Level = R.Type == EDCRoomType::Office ? 1 : 0;
		Rooms.Add(R);
	}

	// Starting board: the listed templates first, then random ones up to the board size.
	for (FName Id : B.StartingBoard)
	{
		if (DCImpl::BoardCount(*this) >= DCImpl::BoardSize(*this)) { break; }
		if (const FDCMissionTemplate* T = Data->FindTemplate(Id)) { Missions.Add(DCImpl::MakeMission(*this, *T)); }
	}
	while (DCImpl::BoardCount(*this) < DCImpl::BoardSize(*this))
	{
		const FDCMissionTemplate* T = DCImpl::PickTemplate(*this);
		if (!T) { break; }
		Missions.Add(DCImpl::MakeMission(*this, *T));
	}
	DCImpl::GenerateStartingRoster(*this);
	DCImpl::RegenerateRecruits(*this);

	NextPostAt = DCImpl::PostInterval(*this);
	LastDawnIndex = DCImpl::DawnIndexAt(*this, 0.0);
	NextAutosaveAt = B.AutosaveSeconds;
	bStateDirty = false;

	SaveRun();
	OnPhaseChanged.Broadcast(Phase);
	OnRunStateChanged.Broadcast();
}

bool UDCRunSubsystem::HasSave() const { return UGameplayStatics::DoesSaveGameExist(GetSlotName(), 0); }

void UDCRunSubsystem::SaveRun()
{
	if (Data.IsValid()) { NextAutosaveAt = RunTime + FMath::Max(1.f, Data->Balance.AutosaveSeconds); }
	bSaveDirty = false;
	if (!bActive || Phase != EDCGamePhase::Playing || !bSaveEnabled) { return; }
	UDCSaveGame* SG = Cast<UDCSaveGame>(UGameplayStatics::CreateSaveGameObject(UDCSaveGame::StaticClass()));
	if (!SG) { return; }
	SG->Version = UDCSaveGame::CurrentVersion;
	SG->Seed = Seed;
	SG->RngState = Rng.GetCurrentSeed();
	SG->RunTime = RunTime;
	SG->Cash = Cash;
	SG->NextId = NextId;
	SG->TimeScale = TimeScale;
	SG->Roster = Roster; SG->Fallen = Fallen; SG->Recruits = Recruits; SG->Missions = Missions; SG->Reports = Reports;
	SG->Stash = Stash; SG->Rooms = Rooms;
	for (const FDCPerk& P : PerkOffer) { SG->PerkOffer.Add(P.Id); }
	for (const FDCPerk& P : PerksTaken) { SG->PerksTaken.Add(P.Id); }
	SG->NextPostAt = NextPostAt; SG->RecruitRefreshAt = RecruitRefreshAt; SG->NextAutosaveAt = NextAutosaveAt;
	SG->LastDawnIndex = LastDawnIndex; SG->MissionsSucceeded = MissionsSucceeded; SG->MissionsAttempted = MissionsAttempted;
	SG->TotalTake = TotalTake; SG->BestMissionTake = BestMissionTake; SG->BestMissionName = BestMissionName; SG->RoomsBuilt = RoomsBuilt;
	SG->PendingPerkOffers = PendingPerkOffers; SG->bArrestPreventUsed = bArrestPreventUsed; SG->ArrestedAt = ArrestedAt;
	if (!UGameplayStatics::SaveGameToSlot(SG, GetSlotName(), 0)) { UE_LOG(LogDC, Warning, TEXT("[Sim] save to slot %s failed"), *GetSlotName()); }
}

bool UDCRunSubsystem::ContinueRun()
{
	if (!HasSave()) { return false; }
	UDCSaveGame* SG = Cast<UDCSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSlotName(), 0));
	if (!SG || SG->Version != UDCSaveGame::CurrentVersion || SG->Rooms.Num() == 0)
	{
		UE_LOG(LogDC, Warning, TEXT("[Sim] save %s missing or corrupt"), *GetSlotName());
		return false;
	}
	EnsureData();
	ResetRunState();
	Seed = SG->Seed;
	Rng.Initialize(SG->RngState);
	RunTime = SG->RunTime;
	Cash = SG->Cash;
	NextId = SG->NextId;
	Roster = SG->Roster; Fallen = SG->Fallen; Recruits = SG->Recruits; Missions = SG->Missions; Reports = SG->Reports;
	Stash = SG->Stash;
	for (const FDCRoom& Saved : SG->Rooms)
	{
		FDCRoom R;
		R.Type = Saved.Type; R.State = Saved.State; R.Level = Saved.Level; R.BuildStartedAt = Saved.BuildStartedAt; R.BuildEndsAt = Saved.BuildEndsAt;
		Rooms.Add(R);
	}
	for (FName Id : SG->PerkOffer) { if (const FDCPerkDef* P = Data->Perks.Find(Id)) { PerkOffer.Add(P->Perk); } }
	for (FName Id : SG->PerksTaken) { if (const FDCPerkDef* P = Data->Perks.Find(Id)) { PerksTaken.Add(P->Perk); } }
	NextPostAt = SG->NextPostAt; RecruitRefreshAt = SG->RecruitRefreshAt; NextAutosaveAt = SG->NextAutosaveAt;
	LastDawnIndex = SG->LastDawnIndex; MissionsSucceeded = SG->MissionsSucceeded; MissionsAttempted = SG->MissionsAttempted;
	TotalTake = SG->TotalTake; BestMissionTake = SG->BestMissionTake; BestMissionName = SG->BestMissionName; RoomsBuilt = SG->RoomsBuilt;
	PendingPerkOffers = SG->PendingPerkOffers; bArrestPreventUsed = SG->bArrestPreventUsed; ArrestedAt = SG->ArrestedAt;
	bActive = true;
	Phase = EDCGamePhase::Playing;
	bStateDirty = false;
	OnPhaseChanged.Broadcast(Phase);
	OnRunStateChanged.Broadcast();
	return true;
}

void UDCRunSubsystem::ExitToTitle()
{
	if (bActive && Phase == EDCGamePhase::Playing) { SaveRun(); }
	ResetRunState();
	OnPhaseChanged.Broadcast(Phase);
	OnRunStateChanged.Broadcast();
}

void UDCRunSubsystem::AbandonRun()
{
	UGameplayStatics::DeleteGameInSlot(GetSlotName(), 0);
	ResetRunState();
	OnPhaseChanged.Broadcast(Phase);
	OnRunStateChanged.Broadcast();
}

bool UDCRunSubsystem::HasActiveRun() const { return bActive; }
EDCGamePhase UDCRunSubsystem::GetPhase() const { return Phase; }
int32 UDCRunSubsystem::GetSeed() const { return Seed; }

void UDCRunSubsystem::SetPaused(bool bInPaused)
{
	if (bPaused == bInPaused) { return; }
	bPaused = bInPaused;
	if (bPaused && bActive && Phase == EDCGamePhase::Playing) { SaveRun(); }
	OnPauseChanged.Broadcast(bPaused);
}
bool UDCRunSubsystem::IsPaused() const { return bPaused; }
void UDCRunSubsystem::SetTimeScale(float Scale) { TimeScale = FMath::Max(0.f, Scale); }
float UDCRunSubsystem::GetTimeScale() const { return TimeScale; }

// ─── time ───────────────────────────────────────────────────────────────────

void UDCRunSubsystem::Tick(float DeltaTime) { AdvanceTime((double)DeltaTime * TimeScale); }

void UDCRunSubsystem::AdvanceTime(double Seconds)
{
	if (!bActive || Phase != EDCGamePhase::Playing || !Data.IsValid() || !(Seconds > 0.0)) { return; }
	++AdvanceDepth;
	const double Target = RunTime + Seconds;
	int32 Guard = 0;
	while (Phase == EDCGamePhase::Playing && RunTime < Target && ++Guard < 1000000)
	{
		const double Next = FMath::Clamp(DCImpl::NextEventTime(*this), RunTime, Target);
		RunTime = Next;
		DCImpl::ProcessDue(*this);
	}
	if (Phase == EDCGamePhase::Playing && RunTime < Target) { RunTime = Target; }
	--AdvanceDepth;
	if (Phase == EDCGamePhase::Playing && (bSaveDirty || RunTime >= NextAutosaveAt)) { SaveRun(); }
	FlushChanged();
}

// ─── clock ──────────────────────────────────────────────────────────────────

double UDCRunSubsystem::GetRunTime() const { return RunTime; }
int32 UDCRunSubsystem::GetDay() const { return FMath::FloorToInt(DCImpl::TotalHours(*this, RunTime) / 24.0) + 1; }
float UDCRunSubsystem::GetHourOfDay() const { return (float)FMath::Fmod(DCImpl::TotalHours(*this, RunTime), 24.0); }

bool UDCRunSubsystem::IsNight() const
{
	const FDCBalance& B = DCImpl::Bal(*this);
	const float H = GetHourOfDay();
	return B.DuskHour > B.DawnHour ? (H >= B.DuskHour || H < B.DawnHour) : (H >= B.DuskHour && H < B.DawnHour);
}

float UDCRunSubsystem::GetSunAlpha() const
{
	const FDCBalance& B = DCImpl::Bal(*this);
	const float H = GetHourOfDay();
	if (IsNight()) { return 0.f; }
	const float Noon = FMath::Clamp(12.f, B.DawnHour + 0.5f, B.DuskHour - 0.5f);
	const float T = H <= Noon ? (H - B.DawnHour) / (Noon - B.DawnHour) : (B.DuskHour - H) / (B.DuskHour - Noon);
	return FMath::Clamp(FMath::Sin(FMath::Clamp(T, 0.f, 1.f) * HALF_PI), 0.f, 1.f);
}

double UDCRunSubsystem::GetSecondsUntilPayday() const
{
	if (!bActive) { return 0.0; }
	return FMath::Max(0.0, DCImpl::DawnTime(*this, LastDawnIndex + 1) - RunTime);
}

float UDCRunSubsystem::GetGameMinutesPerSecond() const { return DCImpl::Bal(*this).GameMinutesPerSecond; }

// ─── reads ──────────────────────────────────────────────────────────────────

const TArray<FDCCriminal>& UDCRunSubsystem::GetRoster() const { return Roster; }
const TArray<FDCCriminal>& UDCRunSubsystem::GetFallen() const { return Fallen; }
const TArray<FDCMission>& UDCRunSubsystem::GetMissions() const { return Missions; }
const TArray<FDCCriminal>& UDCRunSubsystem::GetRecruits() const { return Recruits; }
const TArray<FName>& UDCRunSubsystem::GetStash() const { return Stash; }

TArray<FDCGearItem> UDCRunSubsystem::GetGearCatalogue() const
{
	TArray<FDCGearItem> Out;
	if (!Data.IsValid()) { return Out; }
	float MaxTier = 1.f;
	DCImpl::MaxGlobal(*this, K_gearTier, MaxTier);
	MaxTier = FMath::Max(1.f, MaxTier);

	// Which room level unlocks a gear tier / an extra slot (for the short "Workshop L3" reason).
	auto RoomFor = [this](FName Kind, float Need) -> FString
	{
		for (const FDCRoomDef& R : Data->Rooms)
		{
			for (int32 L = 0; L < R.Levels.Num(); ++L)
			{
				for (const FDCEffect& E : R.Levels[L])
				{
					if (E.Kind == Kind && E.Value >= Need) { return L == 0 ? R.DisplayName : FString::Printf(TEXT("%s L%d"), *R.DisplayName, L + 1); }
				}
			}
		}
		return TEXT("Locked");
	};
	const int32 Slots = GetGearSlotCount();
	for (FName Id : Data->GearOrder)
	{
		const FDCGearDef& G = Data->Gear[Id];
		if (G.bLoot) { continue; }
		FDCGearItem I = G.Item;
		I.Price = DCImpl::GearPrice(*this, G);
		I.bAvailable = true;
		if (I.Tier > MaxTier) { I.bAvailable = false; I.UnavailableReason = RoomFor(K_gearTier, (float)I.Tier); }
		else if ((int32)I.Slot >= Slots) { I.bAvailable = false; I.UnavailableReason = RoomFor(K_gearSlots, (float)((int32)I.Slot + 1 - Data->Balance.GearBaseSlots)); }
		else if (Cash < I.Price) { I.bAvailable = false; I.UnavailableReason = TEXT("$"); }
		Out.Add(I);
	}
	return Out;
}

const FDCCriminal* UDCRunSubsystem::FindCriminal(int32 Id) const
{
	if (const FDCCriminal* C = FindById(Roster, Id)) { return C; }
	if (const FDCCriminal* C = FindById(Recruits, Id)) { return C; }
	return FindById(Fallen, Id);
}
const FDCMission* UDCRunSubsystem::FindMission(int32 Id) const { return FindById(Missions, Id); }
const FDCGearItem* UDCRunSubsystem::FindGear(FName Id) const { const FDCGearDef* G = Data.IsValid() ? Data->Gear.Find(Id) : nullptr; return G ? &G->Item : nullptr; }
const FDCAbility* UDCRunSubsystem::FindAbility(FName Id) const { return Data.IsValid() ? Data->Abilities.Find(Id) : nullptr; }
const FDCTrait* UDCRunSubsystem::FindTrait(FName Id) const { const FDCTraitDef* T = Data.IsValid() ? Data->Traits.Find(Id) : nullptr; return T ? &T->Trait : nullptr; }
const FDCPerk* UDCRunSubsystem::FindPerk(FName Id) const { const FDCPerkDef* P = Data.IsValid() ? Data->Perks.Find(Id) : nullptr; return P ? &P->Perk : nullptr; }

float UDCRunSubsystem::GetEffectiveLevel(int32 Id) const
{
	const FDCCriminal* C = FindCriminal(Id);
	if (!C) { return 0.f; }
	float L = (float)C->Level;
	for (const TPair<EDCGearSlot, FName>& G : C->Equipped)
	{
		if (!DCImpl::SlotEnabled(*this, G.Key)) { continue; }
		if (const FDCGearItem* I = FindGear(G.Value)) { L += I->Power; }
	}
	return L;
}

TArray<FName> UDCRunSubsystem::GetAllTraits(int32 Id) const
{
	TArray<FName> Out;
	const FDCCriminal* C = FindCriminal(Id);
	if (!C) { return Out; }
	Out = C->Traits;
	for (int32 s = 0; s < (int32)EDCGearSlot::MAX; ++s)
	{
		const FName* G = C->Equipped.Find((EDCGearSlot)s);
		if (!G || !DCImpl::SlotEnabled(*this, (EDCGearSlot)s)) { continue; }
		const FDCGearItem* I = FindGear(*G);
		if (I && !I->TraitId.IsNone()) { Out.AddUnique(I->TraitId); }
	}
	return Out;
}

TArray<EDCThreat> UDCRunSubsystem::GetCounters(int32 Id) const
{
	TArray<EDCThreat> Out;
	const FDCCriminal* C = FindCriminal(Id);
	if (!C) { return Out; }
	for (FName A : C->Abilities) { if (const FDCAbility* Ab = FindAbility(A)) { Out.Add(Ab->Counters); } }
	for (int32 s = 0; s < (int32)EDCGearSlot::MAX; ++s)
	{
		const FName* G = C->Equipped.Find((EDCGearSlot)s);
		if (!G || !DCImpl::SlotEnabled(*this, (EDCGearSlot)s)) { continue; }
		const FDCGearItem* I = FindGear(*G);
		if (I && I->Counters != EDCThreat::MAX) { Out.Add(I->Counters); }
	}
	return Out;
}

int32 UDCRunSubsystem::GetGearSlotCount() const
{
	return FMath::Clamp(DCImpl::Bal(*this).GearBaseSlots + FMath::RoundToInt(DCImpl::SumGlobal(*this, K_gearSlots)), 1, (int32)EDCGearSlot::MAX);
}

int32 UDCRunSubsystem::GetCash() const { return Cash; }
int32 UDCRunSubsystem::GetMissionsSucceeded() const { return MissionsSucceeded; }

int32 UDCRunSubsystem::GetUnlockedTier() const
{
	const FDCBalance& B = DCImpl::Bal(*this);
	float RoomTier = 99.f;
	const bool bRoom = DCImpl::MinGlobal(*this, K_unlockTier, RoomTier);
	int32 Tier = 1;
	for (int32 t = 2; t <= 4; ++t)
	{
		const int32 Need = B.UnlockSuccesses.IsValidIndex(t - 1) ? B.UnlockSuccesses[t - 1] : 999;
		const bool bRoomOk = t < 3 || (bRoom && RoomTier <= t);
		if (MissionsSucceeded >= Need && bRoomOk) { Tier = t; } else { break; }
	}
	return Tier;
}

int32 UDCRunSubsystem::GetRosterCap() const { return BaseRosterCap + FMath::RoundToInt(DCImpl::SumGlobal(*this, K_rosterCap)); }
int32 UDCRunSubsystem::GetMaxActiveMissions() const { return BaseMaxActive + FMath::RoundToInt(DCImpl::SumGlobal(*this, K_maxActive)); }
int32 UDCRunSubsystem::GetActiveMissionCount() const { return Missions.FilterByPredicate([](const FDCMission& M) { return M.bInProgress; }).Num(); }
int32 UDCRunSubsystem::GetBoardSize() const { return DCImpl::BoardSize(*this); }

int32 UDCRunSubsystem::GetUpkeepDue() const
{
	if (!Data.IsValid()) { return 0; }
	int32 Sum = Roster.Num() * Data->Balance.UpkeepPerCriminal;
	for (const FDCRoom& R : Rooms)
	{
		const FDCRoomDef* D = Data->FindRoom(R.Type);
		if (D && R.Level >= 1 && D->UpkeepPerDay.IsValidIndex(R.Level - 1)) { Sum += D->UpkeepPerDay[R.Level - 1]; }
	}
	return FMath::RoundToInt(Sum * DCImpl::ProdGlobal(*this, K_upkeepMult));
}

double UDCRunSubsystem::GetRecruitRefreshIn() const { return FMath::Max(0.0, RecruitRefreshAt - RunTime); }
int32 UDCRunSubsystem::GetRecruitRefreshCost() const { return DCImpl::Bal(*this).RecruitRefreshCost; }
double UDCRunSubsystem::GetNextMissionIn() const { return FMath::Max(0.0, NextPostAt - RunTime); }

double UDCRunSubsystem::GetMissionTimeRemaining(int32 Id) const
{
	const FDCMission* M = FindMission(Id);
	return (M && M->bInProgress) ? FMath::Max(0.0, M->EndsAt - RunTime) : 0.0;
}

float UDCRunSubsystem::GetMissionProgress(int32 Id) const
{
	const FDCMission* M = FindMission(Id);
	return (M && M->bInProgress && M->EndsAt > M->StartedAt) ? (float)FMath::Clamp((RunTime - M->StartedAt) / (M->EndsAt - M->StartedAt), 0.0, 1.0) : 0.f;
}

FDCRunSummary UDCRunSubsystem::GetRunSummary() const
{
	FDCRunSummary S;
	S.Seed = Seed;
	S.bWon = Phase == EDCGamePhase::Won;
	S.EndReason = EndReason;
	S.DaysSurvived = GetDay();
	S.PlayTime = RunTime;
	S.MissionsSucceeded = MissionsSucceeded;
	S.MissionsAttempted = MissionsAttempted;
	S.TotalTake = TotalTake;
	S.BestMissionName = BestMissionName;
	S.BestMissionTake = BestMissionTake;
	S.RoomsBuilt = RoomsBuilt;
	for (const FDCCriminal& C : Roster) { S.Crew.Add(C.GetDisplayName()); }
	for (const FDCCriminal& C : Fallen) { S.Lost.Add(C.GetDisplayName()); }
	for (const FDCPerk& P : PerksTaken) { S.Perks.Add(P.Id); }
	return S;
}

// ─── missions ───────────────────────────────────────────────────────────────

FDCSuccessBreakdown UDCRunSubsystem::PreviewPlan(const FDCCrewPlan& Plan) const
{
	const FDCMission* M = FindMission(Plan.MissionId);
	if (!M || !Data.IsValid())
	{
		FDCSuccessBreakdown B;
		B.Block = EDCPlanBlock::Expired;
		return B;
	}
	FDCSuccessBreakdown B = DCImpl::Evaluate(*this, *M, Plan.CriminalIds, Plan.Cut, IsNight());

	EDCPlanBlock Block = EDCPlanBlock::None;
	if (!bActive || Phase != EDCGamePhase::Playing || M->bInProgress || (M->ExpiresAt >= 0.0 && RunTime >= M->ExpiresAt)) { Block = EDCPlanBlock::Expired; }
	else if (M->Tier > GetUnlockedTier() && !M->bFinal) { Block = EDCPlanBlock::Locked; }
	else if (Plan.CriminalIds.Num() < M->CrewSize) { Block = EDCPlanBlock::SlotsEmpty; }
	else
	{
		bool bBad = Plan.CriminalIds.Num() > M->CrewSize;
		TSet<int32> Seen;
		for (int32 Id : Plan.CriminalIds)
		{
			const FDCCriminal* C = FindById(Roster, Id);
			bool bDup = false;
			Seen.Add(Id, &bDup);
			if (!C || !C->IsAvailable() || bDup) { bBad = true; }
		}
		if (bBad) { Block = EDCPlanBlock::CrewUnavailable; }
		else if (B.RefusingIds.Num() > 0) { Block = EDCPlanBlock::Refusal; }
		else if (GetActiveMissionCount() >= GetMaxActiveMissions()) { Block = EDCPlanBlock::NoCapacity; }
		else if (Cash < B.Cost) { Block = EDCPlanBlock::NoCash; }
	}
	B.Block = Block;
	B.bValid = Block == EDCPlanBlock::None;
	return B;
}

FDCCrewPlan UDCRunSubsystem::SuggestPlan(int32 MissionId, float Cut) const
{
	FDCCrewPlan P;
	P.MissionId = MissionId;
	P.Cut = Cut;
	const FDCMission* M = FindMission(MissionId);
	if (!M || !Data.IsValid()) { return P; }
	const float RefuseAt = Data->Balance.RefusalThreshold;
	TArray<const FDCCriminal*> Pool;
	for (const FDCCriminal& C : Roster)
	{
		if (C.IsAvailable() && DCImpl::Willingness(*this, C, FMath::Clamp(Cut, Data->Balance.CutMin, Data->Balance.CutMax)) >= RefuseAt) { Pool.Add(&C); }
	}
	TArray<EDCThreat> Uncovered = M->Threats;
	while (P.CriminalIds.Num() < M->CrewSize && Pool.Num() > 0)
	{
		int32 BestIdx = INDEX_NONE;
		float BestScore = -1.f;
		for (int32 i = 0; i < Pool.Num(); ++i)
		{
			TArray<EDCThreat> Left = Uncovered;
			int32 Covers = 0;
			for (EDCThreat T : GetCounters(Pool[i]->Id)) { if (Left.RemoveSingle(T) > 0) { ++Covers; } }
			const float Score = Covers * 10.f + DCImpl::LevelFactor(*this, GetEffectiveLevel(Pool[i]->Id), M->Level);
			if (Score > BestScore) { BestScore = Score; BestIdx = i; }
		}
		const FDCCriminal* Pick = Pool[BestIdx];
		for (EDCThreat T : GetCounters(Pick->Id)) { Uncovered.RemoveSingle(T); }
		P.CriminalIds.Add(Pick->Id);
		Pool.RemoveAt(BestIdx);
	}
	return P;
}

bool UDCRunSubsystem::StartMission(const FDCCrewPlan& Plan, EDCPlanBlock* OutBlock)
{
	const FDCSuccessBreakdown B = PreviewPlan(Plan);
	if (OutBlock) { *OutBlock = B.Block; }
	if (!B.bValid) { return false; }
	FDCMission* M = Missions.FindByPredicate([&Plan](const FDCMission& X) { return X.Id == Plan.MissionId; });
	check(M);
	Cash -= B.Cost;
	M->bInProgress = true;
	M->StartedAt = RunTime;
	M->Duration = B.Duration;
	M->EndsAt = RunTime + B.Duration;
	M->CrewIds = Plan.CriminalIds;
	M->Cut = FMath::Clamp(Plan.Cut, Data->Balance.CutMin, Data->Balance.CutMax);
	M->LockedChance = B.SuccessChance;
	M->Cost = B.Cost;
	M->Payout = B.Payout;
	for (int32 Id : Plan.CriminalIds)
	{
		if (FDCCriminal* C = DCImpl::RosterMut(*this, Id))
		{
			C->Status = EDCCriminalStatus::OnMission;
			C->ActiveMissionId = M->Id;
			// Greedy penalty at the start, per member whose fair cut is exceeded.
			if (M->Cut > DCImpl::FairCut(*this, *C) + 0.001f)
			{
				C->Loyalty = FMath::Clamp(C->Loyalty + Data->Balance.LoyaltyGreedy, Data->Balance.LoyaltyMin, Data->Balance.LoyaltyMax);
			}
		}
	}
	MissionsAttempted++;
	const FDCMission Started = *M;
	MarkChanged();
	OnMissionStarted.Broadcast(Started);
	if (AdvanceDepth == 0) { SaveRun(); } else { bSaveDirty = true; }
	FlushChanged();
	return true;
}

const TArray<FDCMissionReport>& UDCRunSubsystem::GetReports() const { return Reports; }
int32 UDCRunSubsystem::GetUnseenReportCount() const { return Reports.FilterByPredicate([](const FDCMissionReport& R) { return !R.bSeen; }).Num(); }
void UDCRunSubsystem::MarkReportSeen(int32 Id)
{
	for (FDCMissionReport& R : Reports) { if (R.ReportId == Id && !R.bSeen) { R.bSeen = true; MarkChanged(); } }
	FlushChanged();
}
void UDCRunSubsystem::MarkAllReportsSeen()
{
	for (FDCMissionReport& R : Reports) { if (!R.bSeen) { R.bSeen = true; MarkChanged(); } }
	FlushChanged();
}

// ─── economy ────────────────────────────────────────────────────────────────

namespace
{
	bool Fail(FString* OutError, const TCHAR* Msg) { if (OutError) { *OutError = Msg; } return false; }
}

bool UDCRunSubsystem::BuyGear(FName ItemId, int32 CriminalId, FString* OutError)
{
	if (!bActive || Phase != EDCGamePhase::Playing) { return Fail(OutError, TEXT("No run")); }
	const TArray<FDCGearItem> Cat = GetGearCatalogue();
	const FDCGearItem* I = Cat.FindByPredicate([ItemId](const FDCGearItem& X) { return X.Id == ItemId; });
	if (!I) { return Fail(OutError, TEXT("Not for sale")); }
	if (!I->bAvailable) { if (OutError) { *OutError = I->UnavailableReason; } return false; }
	FDCCriminal* C = DCImpl::RosterMut(*this, CriminalId);
	if (!C) { return Fail(OutError, TEXT("No crew")); }
	if (C->Status == EDCCriminalStatus::OnMission) { return Fail(OutError, TEXT("On a job")); }
	Cash -= I->Price;
	if (const FName* Old = C->Equipped.Find(I->Slot)) { Stash.Add(*Old); }
	C->Equipped.Add(I->Slot, ItemId);
	MarkChanged();
	FlushChanged();
	return true;
}

bool UDCRunSubsystem::EquipFromStash(FName ItemId, int32 CriminalId, FString* OutError)
{
	if (!bActive || Phase != EDCGamePhase::Playing) { return Fail(OutError, TEXT("No run")); }
	const FDCGearItem* I = FindGear(ItemId);
	if (!I || !Stash.Contains(ItemId)) { return Fail(OutError, TEXT("Not in stash")); }
	FDCCriminal* C = DCImpl::RosterMut(*this, CriminalId);
	if (!C) { return Fail(OutError, TEXT("No crew")); }
	if (C->Status == EDCCriminalStatus::OnMission) { return Fail(OutError, TEXT("On a job")); }
	if (!DCImpl::SlotEnabled(*this, I->Slot)) { return Fail(OutError, TEXT("Workshop")); }
	Stash.RemoveSingle(ItemId);
	if (const FName* Old = C->Equipped.Find(I->Slot)) { Stash.Add(*Old); }
	C->Equipped.Add(I->Slot, ItemId);
	MarkChanged();
	FlushChanged();
	return true;
}

void UDCRunSubsystem::Unequip(int32 CriminalId, EDCGearSlot Slot)
{
	FDCCriminal* C = DCImpl::RosterMut(*this, CriminalId);
	if (!C || C->Status == EDCCriminalStatus::OnMission) { return; }
	FName Item;
	if (C->Equipped.RemoveAndCopyValue(Slot, Item)) { Stash.Add(Item); MarkChanged(); }
	FlushChanged();
}

bool UDCRunSubsystem::SellFromStash(FName ItemId)
{
	const FDCGearItem* I = FindGear(ItemId);
	if (!I || !bActive || Stash.RemoveSingle(ItemId) == 0) { return false; }
	Cash += FMath::RoundToInt(I->Price * Data->Balance.SellRefund);
	MarkChanged();
	FlushChanged();
	return true;
}

bool UDCRunSubsystem::Hire(int32 RecruitId, FString* OutError)
{
	if (!bActive || Phase != EDCGamePhase::Playing) { return Fail(OutError, TEXT("No run")); }
	const int32 Idx = Recruits.IndexOfByPredicate([RecruitId](const FDCCriminal& C) { return C.Id == RecruitId; });
	if (Idx == INDEX_NONE) { return Fail(OutError, TEXT("Gone")); }
	if (Roster.Num() >= GetRosterCap()) { return Fail(OutError, TEXT("Bunkroom")); }
	if (Cash < Recruits[Idx].HireCost) { return Fail(OutError, TEXT("$")); }
	Cash -= Recruits[Idx].HireCost;
	FDCCriminal C = Recruits[Idx];
	Recruits.RemoveAt(Idx);
	C.HireCost = 0;
	C.Status = EDCCriminalStatus::Available;
	Roster.Add(C);
	MarkChanged();
	OnCriminalHired.Broadcast(Roster.Last());
	if (AdvanceDepth == 0) { SaveRun(); }
	FlushChanged();
	return true;
}

void UDCRunSubsystem::Dismiss(int32 CriminalId)
{
	const int32 Idx = Roster.IndexOfByPredicate([CriminalId](const FDCCriminal& C) { return C.Id == CriminalId; });
	if (Idx == INDEX_NONE || Roster[Idx].Status == EDCCriminalStatus::OnMission) { return; }
	for (const TPair<EDCGearSlot, FName>& G : Roster[Idx].Equipped) { Stash.Add(G.Value); }
	Roster.RemoveAt(Idx);
	MarkChanged();
	FDCSimImpl::CheckBroke(*this);
	FlushChanged();
}

bool UDCRunSubsystem::RefreshRecruits()
{
	if (!bActive || Phase != EDCGamePhase::Playing || Cash < GetRecruitRefreshCost()) { return false; }
	Cash -= GetRecruitRefreshCost();
	DCImpl::RegenerateRecruits(*this);
	FlushChanged();
	return true;
}

// ─── rooms ──────────────────────────────────────────────────────────────────

TArray<FDCRoom> UDCRunSubsystem::GetRooms() const
{
	TArray<FDCRoom> Out;
	for (int32 i = 0; i < (int32)EDCRoomType::MAX; ++i) { Out.Add(GetRoom((EDCRoomType)i)); }
	return Out;
}

FDCRoom UDCRunSubsystem::GetRoom(EDCRoomType Type) const
{
	FDCRoom R;
	R.Type = Type;
	if (const FDCRoom* St = Rooms.FindByPredicate([Type](const FDCRoom& X) { return X.Type == Type; }))
	{
		R.State = St->State; R.Level = St->Level; R.BuildStartedAt = St->BuildStartedAt; R.BuildEndsAt = St->BuildEndsAt;
	}
	R.DisplayName = DC::RoomName(Type);
	R.Icon = DC::RoomIcon(Type);
	const FDCRoomDef* D = Data.IsValid() ? Data->FindRoom(Type) : nullptr;
	if (!D) { return R; }
	R.DisplayName = D->DisplayName;
	R.Description = D->Description;
	R.Icon = D->Icon;
	R.LevelEffects = D->LevelEffects;
	R.UpkeepPerDay = (R.Level >= 1 && D->UpkeepPerDay.IsValidIndex(R.Level - 1)) ? D->UpkeepPerDay[R.Level - 1] : 0;
	R.bMaxLevel = R.Level >= 3;
	if (!R.bMaxLevel)
	{
		R.NextCost = D->Cost[R.Level];
		R.NextBuildTime = D->BuildSec[R.Level];
		R.RequiredSuccesses = D->RequiredSuccesses[R.Level];
	}
	if (R.bMaxLevel || MissionsSucceeded < R.RequiredSuccesses) { R.Block = EDCPlanBlock::Locked; }
	else if (Cash < R.NextCost) { R.Block = EDCPlanBlock::NoCash; }
	else { R.Block = EDCPlanBlock::None; }
	R.bCanAct = bActive && Phase == EDCGamePhase::Playing && R.Block == EDCPlanBlock::None && !IsAnyRoomBuilding();
	return R;
}

bool UDCRunSubsystem::IsRoomBuilt(EDCRoomType Type) const
{
	const FDCRoom* St = Rooms.FindByPredicate([Type](const FDCRoom& X) { return X.Type == Type; });
	return St && St->Level >= 1;
}

bool UDCRunSubsystem::BuildRoom(EDCRoomType Type, EDCPlanBlock* OutBlock)
{
	const FDCRoom V = GetRoom(Type);
	if (OutBlock) { *OutBlock = (V.Block == EDCPlanBlock::None && IsAnyRoomBuilding()) ? EDCPlanBlock::NoCapacity : V.Block; }
	if (!V.bCanAct) { return false; }
	FDCRoom* R = Rooms.FindByPredicate([Type](const FDCRoom& X) { return X.Type == Type; });
	if (!R) { return false; }
	Cash -= V.NextCost;
	R->State = EDCRoomState::Building;
	R->BuildStartedAt = RunTime;
	R->BuildEndsAt = RunTime + V.NextBuildTime;
	MarkChanged();
	OnRoomChanged.Broadcast(GetRoom(Type));
	if (AdvanceDepth == 0) { SaveRun(); } else { bSaveDirty = true; }
	FlushChanged();
	return true;
}

bool UDCRunSubsystem::IsAnyRoomBuilding() const { return Rooms.ContainsByPredicate([](const FDCRoom& R) { return R.State == EDCRoomState::Building; }); }

int32 UDCRunSubsystem::GetPatchUpCost(int32 CriminalId) const
{
	const FDCCriminal* C = FindById(Roster, CriminalId);
	float Rate = 0.f;
	if (!C || C->Status != EDCCriminalStatus::Injured || !DCImpl::MinGlobal(*this, K_patchUp, Rate)) { return -1; }
	return FMath::Max(1, FMath::RoundToInt(Rate * FMath::Max(0.0, C->InjuredUntil - RunTime)));
}

bool UDCRunSubsystem::PatchUp(int32 CriminalId)
{
	const int32 Cost = GetPatchUpCost(CriminalId);
	if (Cost < 0 || Cash < Cost) { return false; }
	FDCCriminal* C = DCImpl::RosterMut(*this, CriminalId);
	Cash -= Cost;
	C->Status = EDCCriminalStatus::Available;
	C->InjuredUntil = 0.0;
	MarkChanged();
	OnCriminalHealed.Broadcast(CriminalId);
	FlushChanged();
	return true;
}

int32 UDCRunSubsystem::GetBailCost(int32 CriminalId) const
{
	const FDCCriminal* C = FindById(Fallen, CriminalId);
	const double* At = ArrestedAt.Find(CriminalId);
	float Rate = 0.f, Window = 0.f;
	if (!C || !At || !DCImpl::MinGlobal(*this, K_bail, Rate, &Window)) { return -1; }
	if (Window > 0.f && RunTime - *At > Window) { return -1; }
	if (Roster.Num() >= GetRosterCap()) { return -1; }
	return FMath::RoundToInt(Rate * C->Level);
}

bool UDCRunSubsystem::BailOut(int32 CriminalId)
{
	const int32 Cost = GetBailCost(CriminalId);
	if (Cost < 0 || Cash < Cost) { return false; }
	const int32 Idx = Fallen.IndexOfByPredicate([CriminalId](const FDCCriminal& C) { return C.Id == CriminalId; });
	Cash -= Cost;
	FDCCriminal C = Fallen[Idx];
	Fallen.RemoveAt(Idx);
	ArrestedAt.Remove(CriminalId);
	C.Status = EDCCriminalStatus::Available;
	Roster.Add(C);
	MarkChanged();
	if (AdvanceDepth == 0) { SaveRun(); }
	FlushChanged();
	return true;
}

// ─── perks ──────────────────────────────────────────────────────────────────

bool UDCRunSubsystem::HasPendingPerkOffer() const { return PerkOffer.Num() > 0; }
TArray<FDCPerk> UDCRunSubsystem::GetPerkOffer() const { return PerkOffer; }
TArray<FDCPerk> UDCRunSubsystem::GetPerksTaken() const { return PerksTaken; }

bool UDCRunSubsystem::PickPerk(FName PerkId)
{
	const FDCPerk* P = PerkOffer.FindByPredicate([PerkId](const FDCPerk& X) { return X.Id == PerkId; });
	if (!P) { return false; }
	PerksTaken.Add(*P);
	PerkOffer.Reset();
	MarkChanged();
	if (PendingPerkOffers > 0) { PendingPerkOffers--; DCImpl::OfferPerks(*this); }
	if (AdvanceDepth == 0) { SaveRun(); }
	FlushChanged();
	return true;
}

// ── v3 save slots + leaderboard: LEAD STUBS (Sim agent replaces) ─────────────────────────────────────────────────
TArray<FDCSaveSlotInfo> UDCRunSubsystem::GetSaveSlots() const
{
	TArray<FDCSaveSlotInfo> Out;
	for (int32 i = 0; i < NumSaveSlots; ++i) { FDCSaveSlotInfo I; I.Slot = i; I.bUsed = (i == 0 && HasSave()); Out.Add(I); }
	return Out;
}
int32 UDCRunSubsystem::GetCurrentSlot() const { return bActive ? 0 : -1; }
int32 UDCRunSubsystem::GetMostRecentSlot(bool) const { return HasSave() ? 0 : -1; }
void UDCRunSubsystem::StartNewRunInSlot(int32, int32 InSeed) { StartNewRun(InSeed); }
bool UDCRunSubsystem::LoadSlot(int32 Slot) { return Slot == 0 && ContinueRun(); }
bool UDCRunSubsystem::SaveToSlot(int32 Slot) { if (Slot != 0) { return false; } SaveRun(); return true; }
bool UDCRunSubsystem::DeleteSlot(int32 Slot) { if (Slot != 0) { return false; } UGameplayStatics::DeleteGameInSlot(GetSlotName(), 0); return true; }
TArray<FDCRunRecord> UDCRunSubsystem::GetLeaderboard() const { return {}; }
void UDCRunSubsystem::ClearLeaderboard() {}
