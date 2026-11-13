// Director's Cut — UDCRunSubsystem: the real simulation (Agent A).
// Public API in Core/DCRunSubsystem.h; logic helpers in Sim/DCSimImpl.*; data in Sim/DCSimData.*.
#include "Core/DCRunSubsystem.h"
#include "Sim/DCSimImpl.h"
#include "Sim/DCSimData.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "DirectorsCut.h"

using DCImpl = FDCSimImpl;

static void SetErr(FString* OutError, const FString& Msg) { if (OutError) { *OutError = Msg; } }

UDCRunSubsystem* UDCRunSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = (WorldContext && GEngine) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UDCRunSubsystem>() : nullptr;
}

void UDCRunSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Data = FDCSimData::Load(DataDirOverride);
}

void UDCRunSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

// ─── Run lifecycle ──────────────────────────────────────────────────────────

void UDCRunSubsystem::StartNewRun(int32 InSeed)
{
	Data = FDCSimData::Load(DataDirOverride); // re-read each run: JSON balance tweaks apply without a restart
	const FDCBalance& Bal = Data->Balance;

	Seed = InSeed;
	if (Seed == 0)
	{
		FRandomStream Seeder((int32)(FPlatformTime::Cycles64() ^ (uint64)FMath::Rand()));
		while (Seed == 0) { Seed = FMath::Abs((int32)(Seeder.GetUnsignedInt() & 0x7fffffff)) % 999999 + 1; }
	}
	Rng.Initialize(Seed);

	bActive = true;
	Cash = Bal.StartingCash;
	Night = 1;
	HeistsCompleted = HeistsAttempted = TotalTake = BestHeistTake = 0;
	BestHeistName.Reset();
	NextId = 1;
	RecruitRefreshIn = Bal.RecruitRefreshNights;
	PerkOffersGiven = 0;
	RetakesRemaining = 0;
	bFirstArrestPrevented = false;
	bWonRun = false;
	EndReason.Reset();
	Roster.Reset(); Fallen.Reset(); Recruits.Reset(); Jobs.Reset(); Plans.Reset();
	PerkOffer.Reset(); PerksTaken.Reset();
	JobRollSeeds.Reset(); ArrestedOnNight.Reset();
	RetakeSnapshots.Reset(); RetakeJobs.Reset(); RetakeBreakdowns.Reset();
	LastReport = FDCNightReport();

	Rooms.Reset();
	for (int32 i = 0; i < (int32)EDCRoomType::MAX; ++i)
	{
		FDCRoom R;
		R.Type = (EDCRoomType)i;
		R.State = R.Type == EDCRoomType::Office ? EDCRoomState::Built : EDCRoomState::Locked;
		R.Level = R.Type == EDCRoomType::Office ? 1 : 0;
		Rooms.Add(R);
	}
	LastUnlockedTier = DCImpl::UnlockedTier(*this);

	DCImpl::GenerateStartingRoster(*this);
	DCImpl::GenerateRecruits(*this);
	DCImpl::GenerateBoard(*this);
	LastReport.Events.Reset();

	UE_LOG(LogDC, Log, TEXT("[Sim] New run, seed %d: %d crew, %d jobs, $%d"), Seed, Roster.Num(), Jobs.Num(), Cash);
	DCImpl::SetPhase(*this, EDCGamePhase::Planning);
	DCImpl::Changed(*this);
}

bool UDCRunSubsystem::HasActiveRun() const { return bActive; }
EDCGamePhase UDCRunSubsystem::GetPhase() const { return Phase; }
int32 UDCRunSubsystem::GetSeed() const { return Seed; }

void UDCRunSubsystem::FinishDailies()
{
	if (!bActive || Phase != EDCGamePhase::Dailies) { return; }
	RetakesRemaining = 0;
	if (LastReport.bWon)
	{
		EndReason = Data->Strings.WinReason;
		DCImpl::SetPhase(*this, EDCGamePhase::Won);
		OnRunWon.Broadcast(GetRunSummary());
	}
	else if (LastReport.bGameOver)
	{
		DCImpl::SetPhase(*this, EDCGamePhase::GameOver);
		OnGameOver.Broadcast(GetRunSummary());
	}
	else
	{
		DCImpl::SetPhase(*this, EDCGamePhase::Planning);
		if (HasPendingPerkOffer()) { OnPerkOffered.Broadcast(PerkOffer); }
	}
	DCImpl::Changed(*this);
}

// ─── Read state ─────────────────────────────────────────────────────────────

const TArray<FDCCriminal>& UDCRunSubsystem::GetRoster() const { return Roster; }
const TArray<FDCCriminal>& UDCRunSubsystem::GetFallen() const { return Fallen; }
const TArray<FDCJob>& UDCRunSubsystem::GetJobs() const { return Jobs; }
const TArray<FDCCriminal>& UDCRunSubsystem::GetRecruits() const { return Recruits; }

TArray<FDCGearItem> UDCRunSubsystem::GetGearCatalogue() const
{
	TArray<FDCGearItem> Out;
	if (!Data.IsValid()) { return Out; }
	const FDCModStack M = DCImpl::BuildMods(*this);
	const int32 Tier = DCImpl::GearTier(M);
	const int32 Slots = DCImpl::GearSlots(*this, M);
	for (const FDCGearDef& G : Data->Gear)
	{
		FDCGearItem I = G.Item;
		I.Price = DCImpl::GearPrice(G, M);
		I.bAvailable = I.Tier <= Tier && (int32)I.Slot < Slots;
		Out.Add(I);
	}
	return Out;
}

const FDCCriminal* UDCRunSubsystem::FindCriminal(int32 Id) const { return Roster.FindByPredicate([Id](const FDCCriminal& C) { return C.Id == Id; }); }
FDCCriminal* UDCRunSubsystem::FindCriminalMutable(int32 Id) { return Roster.FindByPredicate([Id](const FDCCriminal& C) { return C.Id == Id; }); }
const FDCJob* UDCRunSubsystem::FindJob(int32 Id) const { return Jobs.FindByPredicate([Id](const FDCJob& J) { return J.Id == Id; }); }
const FDCGearItem* UDCRunSubsystem::FindGear(FName Id) const
{
	const FDCGearDef* G = Data.IsValid() ? Data->FindGear(Id) : nullptr;
	return G ? &G->Item : nullptr; // base price; GetGearCatalogue() has discounted prices
}

FDCStatBlock UDCRunSubsystem::GetEffectiveStats(int32 CriminalId) const
{
	const FDCCriminal* C = FindCriminal(CriminalId);
	if (!C) { C = Recruits.FindByPredicate([CriminalId](const FDCCriminal& X) { return X.Id == CriminalId; }); }
	return C && Data.IsValid() ? DCImpl::EffStats(*this, DCImpl::BuildMods(*this), *C) : FDCStatBlock();
}

int32 UDCRunSubsystem::GetEffectiveLuck(int32 CriminalId) const
{
	const FDCCriminal* C = FindCriminal(CriminalId);
	if (!C) { C = Recruits.FindByPredicate([CriminalId](const FDCCriminal& X) { return X.Id == CriminalId; }); }
	return C && Data.IsValid() ? DCImpl::EffLuck(*this, DCImpl::BuildMods(*this), *C) : 0;
}

int32 UDCRunSubsystem::GetCash() const { return Cash; }
int32 UDCRunSubsystem::GetNight() const { return Night; }
int32 UDCRunSubsystem::GetHeistsCompleted() const { return HeistsCompleted; }
int32 UDCRunSubsystem::GetUpkeepDue() const { return Data.IsValid() ? DCImpl::Upkeep(*this, DCImpl::BuildMods(*this)) : 0; }
int32 UDCRunSubsystem::GetRosterCap() const { return Data.IsValid() ? DCImpl::RosterCap(*this, DCImpl::BuildMods(*this)) : 4; }
int32 UDCRunSubsystem::GetMaxCrewsPerNight() const { return Data.IsValid() ? DCImpl::CrewsPerNight(*this, DCImpl::BuildMods(*this)) : 1; }
int32 UDCRunSubsystem::GetRecruitRefreshIn() const { return RecruitRefreshIn; }
int32 UDCRunSubsystem::GetUnlockedTier() const { return Data.IsValid() ? DCImpl::UnlockedTier(*this) : 1; }
const TArray<FDCCrewPlan>& UDCRunSubsystem::GetCommittedPlans() const { return Plans; }
const FDCNightReport& UDCRunSubsystem::GetLastNightReport() const { return LastReport; }

FDCRunSummary UDCRunSubsystem::GetRunSummary() const
{
	FDCRunSummary S;
	S.Seed = Seed;
	S.bWon = bWonRun;
	S.EndReason = EndReason;
	S.NightsSurvived = FMath::Max(0, Night - 1);
	S.HeistsCompleted = HeistsCompleted;
	S.HeistsAttempted = HeistsAttempted;
	S.TotalTake = TotalTake;
	S.BestHeistName = BestHeistName;
	S.BestHeistTake = BestHeistTake;
	for (const FDCRoom& R : Rooms) { if (R.State == EDCRoomState::Built && R.Type != EDCRoomType::Office) { S.RoomsBuilt++; } }
	for (const FDCCriminal& C : Roster) { S.Starring.Add(FString::Printf(TEXT("%s (%s)"), *C.GetDisplayName(), DC::TypeName(C.Type))); }
	for (const FDCCriminal& C : Fallen) { S.InMemoriam.Add(FString::Printf(TEXT("%s (%s)"), *C.GetDisplayName(), DC::TypeName(C.Type))); }
	for (const FDCPerk& P : PerksTaken) { S.PerksTaken.Add(P.DisplayName); }
	return S;
}

// ─── Planning ───────────────────────────────────────────────────────────────

FDCSuccessBreakdown UDCRunSubsystem::PreviewPlan(const FDCCrewPlan& Plan) const
{
	return DCImpl::Breakdown(*this, Plan, true);
}

bool UDCRunSubsystem::CommitPlan(const FDCCrewPlan& Plan, FString* OutError)
{
	const FDCSuccessBreakdown Bd = PreviewPlan(Plan);
	if (!Bd.bValid) { SetErr(OutError, Bd.InvalidReason); return false; }
	FDCCrewPlan P = Plan;
	P.Cut = FMath::Clamp(Plan.Cut, Data->Balance.CutMin, Data->Balance.CutMax);
	// Re-casting a committed job replaces its plan.
	Plans.RemoveAll([&P](const FDCCrewPlan& X) { return X.JobId == P.JobId; });
	Plans.Add(P);
	DCImpl::RefreshStatuses(*this);
	OnPlanCommitted.Broadcast(P);
	DCImpl::Changed(*this);
	return true;
}

void UDCRunSubsystem::CancelPlan(int32 JobId)
{
	if (Plans.RemoveAll([JobId](const FDCCrewPlan& P) { return P.JobId == JobId; }) == 0) { return; }
	DCImpl::RefreshStatuses(*this);
	OnPlanCancelled.Broadcast(JobId);
	DCImpl::Changed(*this);
}

// ─── Economy ────────────────────────────────────────────────────────────────

bool UDCRunSubsystem::BuyGear(FName ItemId, int32 CriminalId, FString* OutError)
{
	if (!bActive || Phase != EDCGamePhase::Planning) { SetErr(OutError, TEXT("The fence only deals during planning.")); return false; }
	const FDCGearDef* G = Data->FindGear(ItemId);
	FDCCriminal* C = FindCriminalMutable(CriminalId);
	if (!G) { SetErr(OutError, TEXT("Unknown item.")); return false; }
	if (!C) { SetErr(OutError, TEXT("Unknown crew member.")); return false; }
	const FDCModStack M = DCImpl::BuildMods(*this);
	if ((int32)G->Item.Slot >= DCImpl::GearSlots(*this, M)) { SetErr(OutError, FString::Printf(TEXT("The %s slot needs the Workshop."), DC::SlotName(G->Item.Slot))); return false; }
	if (G->Item.Tier > DCImpl::GearTier(M)) { SetErr(OutError, G->Item.Tier >= 3 ? TEXT("Needs a Workshop upgrade.") : TEXT("Needs the Workshop.")); return false; }
	const int32 Price = DCImpl::GearPrice(*G, M);
	if (Cash < Price) { SetErr(OutError, TEXT("Not enough cash.")); return false; }
	if (C->Equipped.FindRef(G->Item.Slot) == ItemId) { SetErr(OutError, TEXT("Already equipped.")); return false; }
	Cash -= Price;
	C->Equipped.Add(G->Item.Slot, ItemId);
	DCImpl::Changed(*this);
	return true;
}

void UDCRunSubsystem::Unequip(int32 CriminalId, EDCGearSlot Slot)
{
	FDCCriminal* C = FindCriminalMutable(CriminalId);
	if (C && C->Equipped.Remove(Slot) > 0) { DCImpl::Changed(*this); }
}

bool UDCRunSubsystem::Hire(int32 RecruitId, FString* OutError)
{
	if (!bActive || Phase != EDCGamePhase::Planning) { SetErr(OutError, TEXT("You can only hire during planning.")); return false; }
	const int32 Idx = Recruits.IndexOfByPredicate([RecruitId](const FDCCriminal& C) { return C.Id == RecruitId; });
	if (Idx == INDEX_NONE) { SetErr(OutError, TEXT("That recruit has moved on.")); return false; }
	if (Roster.Num() >= GetRosterCap()) { SetErr(OutError, TEXT("Roster full. Build or upgrade the Lounge.")); return false; }
	if (Cash < Recruits[Idx].HireCost) { SetErr(OutError, TEXT("Not enough cash.")); return false; }
	Cash -= Recruits[Idx].HireCost;
	FDCCriminal C = Recruits[Idx];
	C.Status = EDCCriminalStatus::Available;
	Recruits.RemoveAt(Idx);
	Roster.Add(C);
	OnCriminalHired.Broadcast(Roster.Last());
	DCImpl::Changed(*this);
	return true;
}

void UDCRunSubsystem::Dismiss(int32 CriminalId)
{
	if (!FindCriminal(CriminalId)) { return; }
	TArray<int32> Cancelled;
	for (const FDCCrewPlan& P : Plans) { if (P.CriminalIds.Contains(CriminalId)) { Cancelled.Add(P.JobId); } }
	Plans.RemoveAll([CriminalId](const FDCCrewPlan& P) { return P.CriminalIds.Contains(CriminalId); });
	Roster.RemoveAll([CriminalId](const FDCCriminal& C) { return C.Id == CriminalId; });
	DCImpl::RefreshStatuses(*this);
	for (int32 J : Cancelled) { OnPlanCancelled.Broadcast(J); }
	DCImpl::Changed(*this);
}

// ─── Rooms ──────────────────────────────────────────────────────────────────

TArray<FDCRoom> UDCRunSubsystem::GetRooms() const
{
	TArray<FDCRoom> Out;
	if (!Data.IsValid()) { return Rooms; }
	for (const FDCRoom& State : Rooms)
	{
		const FDCRoomDef& Def = Data->GetRoom(State.Type);
		FDCRoom R = State;
		R.DisplayName = Def.DisplayName;
		R.Description = Def.Description;
		R.LevelEffects.Reset();
		for (const FDCRoomLevelDef& L : Def.Levels) { R.LevelEffects.Add(L.EffectText); }
		R.RequiredHeists = Def.RequiredHeists;
		R.UpkeepPerNight = Def.UpkeepPerNight;
		const bool bBuilt = R.State == EDCRoomState::Built;
		R.NextCost = !bBuilt ? Def.BuildCost : (R.Level < 3 && Def.UpgradeCosts.IsValidIndex(R.Level - 1)) ? Def.UpgradeCosts[R.Level - 1] : 0;
		if (bBuilt && R.Level >= 3) { R.BlockedReason = TEXT("Max level"); }
		else if (!bBuilt && HeistsCompleted < Def.RequiredHeists) { R.BlockedReason = FString::Printf(TEXT("Needs %d heist%s"), Def.RequiredHeists, Def.RequiredHeists == 1 ? TEXT("") : TEXT("s")); }
		else if (Cash < R.NextCost) { R.BlockedReason = TEXT("Not enough cash"); }
		else if (Phase != EDCGamePhase::Planning) { R.BlockedReason = TEXT("Only during planning"); }
		else { R.BlockedReason.Reset(); }
		R.bCanAct = bActive && R.BlockedReason.IsEmpty();
		Out.Add(R);
	}
	return Out;
}

FDCRoom UDCRunSubsystem::GetRoom(EDCRoomType Type) const
{
	for (const FDCRoom& R : GetRooms()) { if (R.Type == Type) { return R; } }
	return FDCRoom();
}

bool UDCRunSubsystem::IsRoomBuilt(EDCRoomType Type) const
{
	return Rooms.ContainsByPredicate([Type](const FDCRoom& R) { return R.Type == Type && R.State == EDCRoomState::Built; });
}

bool UDCRunSubsystem::BuildRoom(EDCRoomType Type, FString* OutError)
{
	const FDCRoom R = GetRoom(Type);
	if (R.State == EDCRoomState::Built) { SetErr(OutError, TEXT("Already built.")); return false; }
	if (!R.bCanAct) { SetErr(OutError, R.BlockedReason.IsEmpty() ? FString(TEXT("Can't build that now.")) : R.BlockedReason); return false; }
	FDCRoom* X = Rooms.FindByPredicate([Type](const FDCRoom& Q) { return Q.Type == Type; });
	if (!X) { return false; }
	Cash -= R.NextCost;
	X->State = EDCRoomState::Built;
	X->Level = 1;
	DCImpl::TopUpBoard(*this);
	OnRoomChanged.Broadcast(GetRoom(Type));
	DCImpl::Changed(*this);
	return true;
}

bool UDCRunSubsystem::UpgradeRoom(EDCRoomType Type, FString* OutError)
{
	const FDCRoom R = GetRoom(Type);
	if (R.State != EDCRoomState::Built) { SetErr(OutError, TEXT("Build it first.")); return false; }
	if (!R.bCanAct) { SetErr(OutError, R.BlockedReason.IsEmpty() ? FString(TEXT("Can't upgrade that now.")) : R.BlockedReason); return false; }
	FDCRoom* X = Rooms.FindByPredicate([Type](const FDCRoom& Q) { return Q.Type == Type; });
	if (!X) { return false; }
	Cash -= R.NextCost;
	X->Level = FMath::Min(3, X->Level + 1);
	DCImpl::TopUpBoard(*this);
	OnRoomChanged.Broadcast(GetRoom(Type));
	DCImpl::Changed(*this);
	return true;
}

int32 UDCRunSubsystem::GetPatchUpCost(int32 CriminalId) const
{
	if (!bActive || !Data.IsValid()) { return -1; }
	const FDCCriminal* C = FindCriminal(CriminalId);
	if (!C || C->Status != EDCCriminalStatus::Injured || C->InjuredNights <= 0) { return -1; }
	const float PerNight = DCImpl::BuildMods(*this).Max(EDCModKind::PatchUp, 0.f);
	return PerNight > 0.f ? FMath::RoundToInt(PerNight * C->InjuredNights) : -1;
}

bool UDCRunSubsystem::PatchUp(int32 CriminalId, FString* OutError)
{
	if (Phase != EDCGamePhase::Planning) { SetErr(OutError, TEXT("Only during planning.")); return false; }
	const int32 Cost = GetPatchUpCost(CriminalId);
	if (Cost < 0) { SetErr(OutError, IsRoomBuilt(EDCRoomType::Clinic) ? TEXT("Nothing to patch up (needs Clinic L2).") : TEXT("Needs the Back-Alley Clinic.")); return false; }
	if (Cash < Cost) { SetErr(OutError, TEXT("Not enough cash.")); return false; }
	FDCCriminal* C = FindCriminalMutable(CriminalId);
	Cash -= Cost;
	C->InjuredNights = 0;
	C->Status = EDCCriminalStatus::Available;
	DCImpl::Changed(*this);
	return true;
}

int32 UDCRunSubsystem::GetBailCost(int32 CriminalId) const
{
	if (!bActive || !Data.IsValid()) { return -1; }
	const FDCCriminal* C = Fallen.FindByPredicate([CriminalId](const FDCCriminal& X) { return X.Id == CriminalId; });
	if (!C) { return -1; }
	const int32* When = ArrestedOnNight.Find(CriminalId);
	if (!When || Night - *When > 1) { return -1; } // only within 1 night of the arrest
	const FDCModStack M = DCImpl::BuildMods(*this);
	const float Mult = M.Max(EDCModKind::Bail, 0.f);
	if (Mult <= 0.f) { return -1; }
	const int32 Hire = C->HireCost > 0 ? C->HireCost : Data->GetType(C->Type).HireCostBase;
	return FMath::Max(100, DCImpl::RoundTo(Mult * Hire, 100));
}

bool UDCRunSubsystem::BailOut(int32 CriminalId, FString* OutError)
{
	if (Phase != EDCGamePhase::Planning) { SetErr(OutError, TEXT("Only during planning.")); return false; }
	const int32 Cost = GetBailCost(CriminalId);
	if (Cost < 0) { SetErr(OutError, IsRoomBuilt(EDCRoomType::LawyerOffice) ? TEXT("Too late to post bail.") : TEXT("Needs the Lawyer's Office.")); return false; }
	if (Cash < Cost) { SetErr(OutError, TEXT("Not enough cash.")); return false; }
	if (Roster.Num() >= GetRosterCap()) { SetErr(OutError, TEXT("Roster full.")); return false; }
	const int32 Idx = Fallen.IndexOfByPredicate([CriminalId](const FDCCriminal& X) { return X.Id == CriminalId; });
	FDCCriminal C = Fallen[Idx];
	Fallen.RemoveAt(Idx);
	ArrestedOnNight.Remove(CriminalId);
	C.Status = EDCCriminalStatus::Available;
	C.InjuredNights = 0;
	Cash -= Cost;
	Roster.Add(C);
	LastReport.Events.Add(DCImpl::Fill(Data->Strings.Event(TEXT("bailed")), C.GetShortName()));
	DCImpl::Changed(*this);
	return true;
}

// ─── Perks ──────────────────────────────────────────────────────────────────

bool UDCRunSubsystem::HasPendingPerkOffer() const { return PerkOffer.Num() > 0; }
TArray<FDCPerk> UDCRunSubsystem::GetPerkOffer() const { return PerkOffer; }
TArray<FDCPerk> UDCRunSubsystem::GetPerksTaken() const { return PerksTaken; }

bool UDCRunSubsystem::PickPerk(FName PerkId)
{
	if (!bActive) { return false; }
	const FDCPerk* P = PerkOffer.FindByPredicate([PerkId](const FDCPerk& X) { return X.Id == PerkId; });
	if (!P) { return false; }
	PerksTaken.Add(*P);
	PerkOffer.Reset();
	TArray<FString> Events;
	DCImpl::CheckProgression(*this, Events); // another offer may already be earned
	if (Phase == EDCGamePhase::Planning)
	{
		DCImpl::TopUpBoard(*this);
		if (HasPendingPerkOffer()) { OnPerkOffered.Broadcast(PerkOffer); }
	}
	DCImpl::Changed(*this);
	return true;
}

// ─── Night ──────────────────────────────────────────────────────────────────

TArray<FDCHeistResult> UDCRunSubsystem::EndNight()
{
	if (!bActive || Phase != EDCGamePhase::Planning) { return {}; }
	const FDCBalance& Bal = Data->Balance;

	LastReport = FDCNightReport();
	LastReport.Night = Night;
	LastReport.CashBefore = Cash;
	RetakeSnapshots.Reset(); RetakeJobs.Reset(); RetakeBreakdowns.Reset();

	// 1. Every breakdown is computed before anything mutates: identical to what PreviewPlan showed.
	TArray<FDCSuccessBreakdown> Bds;
	for (const FDCCrewPlan& P : Plans) { Bds.Add(DCImpl::Breakdown(*this, P, false)); }

	// 2. Resolve each heist with its own dice stream.
	TSet<int32> CastTonight;
	TMap<int32, int32> JobsPerCriminal;
	for (int32 i = 0; i < Plans.Num(); ++i)
	{
		const FDCCrewPlan& P = Plans[i];
		const FDCJob* JobPtr = FindJob(P.JobId);
		if (!JobPtr) { continue; }
		const FDCJob Job = *JobPtr;
		for (int32 Id : P.CriminalIds) { CastTonight.Add(Id); JobsPerCriminal.FindOrAdd(Id)++; }

		TArray<FDCCriminal> Snapshot;
		for (int32 Id : P.CriminalIds) { if (const FDCCriminal* C = FindCriminal(Id)) { Snapshot.Add(*C); } }

		FRandomStream Dice(JobRollSeeds.Contains(Job.Id) ? JobRollSeeds[Job.Id] : (int32)(Rng.GetUnsignedInt() & 0x7fffffff));
		FDCHeistResult R;
		R.ResultId = NextId++;
		DCImpl::ResolveHeist(*this, P, Bds[i], Job, Dice, R);
		HeistsAttempted++;
		if (R.Outcome == EDCHeistOutcome::Failed || R.Outcome == EDCHeistOutcome::Busted)
		{
			RetakeSnapshots.Add(R.ResultId, Snapshot);
			RetakeJobs.Add(R.ResultId, Job);
			RetakeBreakdowns.Add(R.ResultId, Bds[i]);
		}
		LastReport.TotalTake += R.DirectorTake;
		LastReport.Results.Add(R);
	}

	const FDCModStack M = DCImpl::BuildMods(*this);

	// 3. Overtime loyalty cost.
	const float OvertimeCost = M.Max(EDCModKind::Overtime, 0.f);
	for (const TPair<int32, int32>& E : JobsPerCriminal)
	{
		if (E.Value < 2) { continue; }
		if (FDCCriminal* C = FindCriminalMutable(E.Key))
		{
			C->Loyalty = FMath::Clamp(C->Loyalty - FMath::RoundToInt(OvertimeCost), Bal.LoyaltyMin, Bal.LoyaltyMax);
		}
	}

	// 4. Idle loyalty, healing.
	const int32 IdleLoy = FMath::RoundToInt(M.Sum(EDCModKind::IdleLoyalty));
	const int32 HealRate = FMath::Max(1, FMath::RoundToInt(M.Max(EDCModKind::InjuryHealRate, 1.f)));
	int32 IdleCount = 0;
	for (FDCCriminal& C : Roster)
	{
		if (CastTonight.Contains(C.Id))
		{
			continue; // injuries from tonight start healing tomorrow
		}
		if (C.Status == EDCCriminalStatus::Injured)
		{
			C.InjuredNights = FMath::Max(0, C.InjuredNights - HealRate);
			if (C.InjuredNights == 0)
			{
				C.Status = EDCCriminalStatus::Available;
				LastReport.Events.Add(DCImpl::Fill(Data->Strings.Event(TEXT("healed")), C.GetShortName()));
			}
		}
		else if (IdleLoy != 0)
		{
			C.Loyalty = FMath::Clamp(C.Loyalty + IdleLoy, Bal.LoyaltyMin, Bal.LoyaltyMax);
			++IdleCount;
		}
	}
	if (IdleCount > 0) { LastReport.Events.Add(DCImpl::Fill(Data->Strings.Event(TEXT("idleLoyalty")), FString(), FString(), 0, FString::FromInt(IdleCount))); }
	for (const FDCHeistResult& R : LastReport.Results)
	{
		for (const FDCCrewMemberResult& MR : R.Members)
		{
			if (MR.bArrested) { LastReport.Events.Add(DCImpl::Fill(Data->Strings.Event(TEXT("arrested")), MR.Name)); }
			if (MR.bArrestPrevented) { LastReport.Events.Add(DCImpl::Fill(Data->Strings.Event(TEXT("arrestPrevented")), MR.Name)); }
			if (MR.bInjured) { LastReport.Events.Add(DCImpl::Fill(Data->Strings.Event(TEXT("injured")), MR.Name)); }
			if (MR.bLevelledUp) { LastReport.Events.Add(FString::Printf(TEXT("%s (level %d)"), *DCImpl::Fill(Data->Strings.Event(TEXT("levelUp")), MR.Name), MR.NewLevel)); }
		}
	}

	// 5. Upkeep.
	Plans.Reset();
	DCImpl::RefreshStatuses(*this);
	LastReport.UpkeepPaid = DCImpl::Upkeep(*this, M);
	Cash -= LastReport.UpkeepPaid;
	LastReport.Events.Add(DCImpl::Fill(Data->Strings.Event(TEXT("upkeepPaid")), FString(), FString(), 0, DCImpl::Money(LastReport.UpkeepPaid)));

	// 6. Age the world.
	Night++;
	DCImpl::CheckProgression(*this, LastReport.Events);
	if (--RecruitRefreshIn <= 0)
	{
		DCImpl::GenerateRecruits(*this);
		RecruitRefreshIn = Bal.RecruitRefreshNights;
		LastReport.Events.Add(Data->Strings.RecruitsRefreshed);
	}
	DCImpl::GenerateBoard(*this);
	RetakesRemaining = DCImpl::RetakesPerNight(DCImpl::BuildMods(*this));
	for (FDCHeistResult& R : LastReport.Results)
	{
		R.bRetakeAvailable = RetakesRemaining > 0 && RetakeSnapshots.Contains(R.ResultId);
	}

	// 7. Win / game over.
	LastReport.CashAfter = Cash;
	LastReport.bWon = bWonRun;
	FString Reason;
	LastReport.bGameOver = !bWonRun && DCImpl::ComputeGameOver(*this, Reason);
	if (LastReport.bGameOver) { EndReason = Reason; }
	if (bWonRun) { EndReason = Data->Strings.WinReason; }

	DCImpl::SetPhase(*this, EDCGamePhase::Dailies);
	OnNightResolved.Broadcast(LastReport);
	DCImpl::Changed(*this);
	return LastReport.Results;
}

int32 UDCRunSubsystem::GetRetakesRemaining() const { return Phase == EDCGamePhase::Dailies ? RetakesRemaining : 0; }

bool UDCRunSubsystem::UseRetake(int32 HeistResultId, FDCHeistResult* OutNewResult)
{
	if (!bActive || Phase != EDCGamePhase::Dailies || RetakesRemaining <= 0) { return false; }
	const int32 Idx = LastReport.Results.IndexOfByPredicate([HeistResultId](const FDCHeistResult& R) { return R.ResultId == HeistResultId; });
	if (Idx == INDEX_NONE || !RetakeSnapshots.Contains(HeistResultId)) { return false; }
	const FDCHeistResult Old = LastReport.Results[Idx];
	if (Old.Outcome != EDCHeistOutcome::Failed && Old.Outcome != EDCHeistOutcome::Busted) { return false; }

	// Undo the failure: restore the crew as they were before the heist.
	for (const FDCCrewMemberResult& MR : Old.Members) { if (MR.bArrestPrevented) { bFirstArrestPrevented = false; } }
	for (const FDCCriminal& Snap : RetakeSnapshots[HeistResultId])
	{
		FDCCriminal C = Snap;
		C.Status = EDCCriminalStatus::Available;
		C.InjuredNights = 0;
		Fallen.RemoveAll([&C](const FDCCriminal& X) { return X.Id == C.Id; });
		ArrestedOnNight.Remove(C.Id);
		if (FDCCriminal* Existing = FindCriminalMutable(C.Id)) { *Existing = C; } else { Roster.Add(C); }
	}

	// Re-shoot with a fresh roll.
	FRandomStream Dice((int32)(Rng.GetUnsignedInt() & 0x7fffffff));
	FDCHeistResult R;
	R.ResultId = HeistResultId;
	const int32 CashBefore = Cash;
	DCImpl::ResolveHeist(*this, Old.Plan, RetakeBreakdowns[HeistResultId], RetakeJobs[HeistResultId], Dice, R);
	R.FirstRoll = Old.FirstRoll;
	R.bRetaken = true;
	RetakesRemaining--;

	LastReport.TotalTake += Cash - CashBefore;
	LastReport.Results[Idx] = R;
	if (R.Outcome == EDCHeistOutcome::Clean || R.Outcome == EDCHeistOutcome::Messy)
	{
		RetakeSnapshots.Remove(HeistResultId);
		LastReport.Events.Add(FString::Printf(TEXT("%s %s: %s"), *Data->Strings.Event(TEXT("retake")), *R.JobName, DC::OutcomeStamp(R.Outcome)));
	}
	for (FDCHeistResult& X : LastReport.Results) { X.bRetakeAvailable = RetakesRemaining > 0 && RetakeSnapshots.Contains(X.ResultId); }
	DCImpl::RefreshStatuses(*this);
	DCImpl::CheckProgression(*this, LastReport.Events);

	LastReport.CashAfter = Cash;
	LastReport.bWon = bWonRun;
	FString Reason;
	LastReport.bGameOver = !bWonRun && DCImpl::ComputeGameOver(*this, Reason);
	EndReason = bWonRun ? Data->Strings.WinReason : LastReport.bGameOver ? Reason : FString();

	if (OutNewResult) { *OutNewResult = R; }
	OnRetakeResolved.Broadcast(R);
	DCImpl::Changed(*this);
	return true;
}
