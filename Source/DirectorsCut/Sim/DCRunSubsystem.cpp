// Wave 0 STUB implementation of UDCRunSubsystem: believable fake data so UI/World can build against it.
// Agent A replaces this file with the real simulation.
#include "Core/DCRunSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "DirectorsCut.h"

UDCRunSubsystem* UDCRunSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UDCRunSubsystem>() : nullptr;
}

void UDCRunSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UDCRunSubsystem::Deinitialize() { Super::Deinitialize(); }

static const TCHAR* GFirst[] = { TEXT("Vinnie"), TEXT("Rosa"), TEXT("Sal"), TEXT("Dolores"), TEXT("Frankie"), TEXT("Mae"), TEXT("Lou"), TEXT("Ginny") };
static const TCHAR* GNick[]  = { TEXT("The Wrench"), TEXT("Two-Tone"), TEXT("Whisper"), TEXT("Lucky"), TEXT("Sparks"), TEXT("Velvet"), TEXT("Brakes"), TEXT("Pennies") };
static const TCHAR* GLast[]  = { TEXT("Caruso"), TEXT("Delgado"), TEXT("Moretti"), TEXT("Kowalski"), TEXT("Finch"), TEXT("O'Malley"), TEXT("Baptiste"), TEXT("Sterling") };

static EDCStat PrimaryStat(EDCCriminalType T)
{
	switch (T)
	{
	case EDCCriminalType::Wheelman:    return EDCStat::Driving;
	case EDCCriminalType::Muscle:      return EDCStat::Muscle;
	case EDCCriminalType::Hacker:      return EDCStat::Tech;
	case EDCCriminalType::Ghost:       return EDCStat::Stealth;
	case EDCCriminalType::Face:        return EDCStat::Charm;
	default:                           return EDCStat::Finesse;
	}
}

FDCCriminal UDCRunSubsystem::MakeStubCriminal(EDCCriminalType Type)
{
	FDCCriminal C;
	C.Id = NextId++;
	C.FirstName = GFirst[Rng.RandRange(0, 7)];
	C.Nickname = GNick[Rng.RandRange(0, 7)];
	C.LastName = GLast[Rng.RandRange(0, 7)];
	C.Type = Type;
	for (int32 s = 0; s < (int32)EDCStat::MAX; ++s) { C.BaseStats.Set((EDCStat)s, Rng.RandRange(10, 30)); }
	C.BaseStats.Set(PrimaryStat(Type), Rng.RandRange(45, 65));
	C.Luck = Rng.RandRange(0, 3);
	C.Loyalty = Rng.RandRange(35, 65);
	C.HireCost = 3000 + Rng.RandRange(0, 4) * 500;
	return C;
}

void UDCRunSubsystem::MakeStubJobs()
{
	struct FT { const TCHAR* Id; const TCHAR* Name; TArray<EDCCriminalType> Roles; int32 Min, Max, Pay; EDCStat A, B; };
	const TArray<FT> T = {
		{ TEXT("corner_store"), TEXT("Corner Store Stickup"), { EDCCriminalType::Muscle }, 1, 2, 5500, EDCStat::Muscle, EDCStat::Driving },
		{ TEXT("car_boost"), TEXT("Car Boost"), { EDCCriminalType::Wheelman }, 1, 2, 7000, EDCStat::Driving, EDCStat::Tech },
		{ TEXT("data_siphon"), TEXT("Data Siphon"), { EDCCriminalType::Hacker }, 1, 2, 9000, EDCStat::Tech, EDCStat::Stealth },
		{ TEXT("gallery_lift"), TEXT("Gallery Lift"), { EDCCriminalType::Ghost, EDCCriminalType::Face }, 2, 3, 18000, EDCStat::Stealth, EDCStat::Charm },
	};
	Jobs.Reset();
	for (const FT& X : T)
	{
		FDCJob J;
		J.Id = NextId++;
		J.TemplateId = X.Id;
		J.DisplayName = X.Name;
		J.Flavour = TEXT("A simple production. In and out before the second reel.");
		J.RequiredRoles = X.Roles;
		J.MinCrew = X.Min; J.MaxCrew = X.Max;
		J.StatWeights.Add(X.A, 0.6f); J.StatWeights.Add(X.B, 0.4f);
		J.Difficulty = 40.f + 10.f * X.Roles.Num();
		J.Payout = X.Pay;
		J.XPReward = 40 * X.Roles.Num();
		J.ArrestRisk = 0.2f;
		Jobs.Add(J);
	}
}

void UDCRunSubsystem::SetPhase(EDCGamePhase NewPhase)
{
	if (Phase != NewPhase) { Phase = NewPhase; OnPhaseChanged.Broadcast(Phase); }
}

void UDCRunSubsystem::StartNewRun(int32 InSeed)
{
	Seed = InSeed != 0 ? InSeed : FMath::Rand();
	Rng.Initialize(Seed);
	bActive = true;
	Cash = 25000; Night = 1; HeistsCompleted = 0; NextId = 1;
	Roster.Reset(); Fallen.Reset(); Recruits.Reset(); Plans.Reset(); PerkOffer.Reset(); PerksTaken.Reset();
	LastReport = FDCNightReport();
	for (EDCCriminalType T : { EDCCriminalType::Muscle, EDCCriminalType::Wheelman, EDCCriminalType::Hacker, EDCCriminalType::Ghost }) { Roster.Add(MakeStubCriminal(T)); }
	for (EDCCriminalType T : { EDCCriminalType::Face, EDCCriminalType::Safecracker, EDCCriminalType::Muscle }) { Recruits.Add(MakeStubCriminal(T)); }
	MakeStubJobs();

	Gear.Reset();
	auto AddGear = [this](const TCHAR* Id, const TCHAR* Name, EDCGearSlot Slot, int32 Price, EDCStat S, int32 Amt, int32 Tier)
	{
		FDCGearItem G; G.Id = Id; G.DisplayName = Name; G.Slot = Slot; G.Price = Price; G.StatBonus.Set(S, Amt); G.Tier = Tier;
		G.Description = FString::Printf(TEXT("+%d %s"), Amt, DC::StatName(S)); G.bAvailable = Tier == 1;
		Gear.Add(G);
	};
	AddGear(TEXT("crowbar"), TEXT("Crowbar"), EDCGearSlot::Tool, 800, EDCStat::Muscle, 8, 1);
	AddGear(TEXT("lockpicks"), TEXT("Lockpick Set"), EDCGearSlot::Tool, 1200, EDCStat::Finesse, 10, 1);
	AddGear(TEXT("laptop"), TEXT("Laptop Rig"), EDCGearSlot::Gadget, 2000, EDCStat::Tech, 12, 1);
	AddGear(TEXT("suit"), TEXT("Tailored Suit"), EDCGearSlot::Outfit, 1800, EDCStat::Charm, 12, 1);
	AddGear(TEXT("blackout"), TEXT("Blackout Suit"), EDCGearSlot::Outfit, 1600, EDCStat::Stealth, 10, 2);

	Rooms.Reset();
	const int32 Costs[] = { 0, 8000, 12000, 15000, 25000, 10000, 14000, 20000 };
	const int32 Req[] = { 0, 0, 0, 2, 5, 0, 0, 0 };
	for (int32 i = 0; i < (int32)EDCRoomType::MAX; ++i)
	{
		FDCRoom R;
		R.Type = (EDCRoomType)i;
		R.DisplayName = DC::RoomName(R.Type);
		R.Description = TEXT("Stub room description.");
		R.LevelEffects = { TEXT("Level 1 effect"), TEXT("Level 2 effect"), TEXT("Level 3 effect") };
		R.State = i == 0 ? EDCRoomState::Built : EDCRoomState::Locked;
		R.Level = i == 0 ? 1 : 0;
		R.NextCost = i == 0 ? 10000 : Costs[i];
		R.RequiredHeists = Req[i];
		R.UpkeepPerNight = 200;
		Rooms.Add(R);
	}
	SetPhase(EDCGamePhase::Planning);
	OnRunStateChanged.Broadcast();
}

bool UDCRunSubsystem::HasActiveRun() const { return bActive; }
EDCGamePhase UDCRunSubsystem::GetPhase() const { return Phase; }
int32 UDCRunSubsystem::GetSeed() const { return Seed; }

void UDCRunSubsystem::FinishDailies()
{
	if (Phase != EDCGamePhase::Dailies) { return; }
	if (LastReport.bGameOver) { SetPhase(EDCGamePhase::GameOver); OnGameOver.Broadcast(GetRunSummary()); return; }
	if (LastReport.bWon) { SetPhase(EDCGamePhase::Won); OnRunWon.Broadcast(GetRunSummary()); return; }
	SetPhase(EDCGamePhase::Planning);
	if (HasPendingPerkOffer()) { OnPerkOffered.Broadcast(PerkOffer); }
	OnRunStateChanged.Broadcast();
}

const TArray<FDCCriminal>& UDCRunSubsystem::GetRoster() const { return Roster; }
const TArray<FDCCriminal>& UDCRunSubsystem::GetFallen() const { return Fallen; }
const TArray<FDCJob>& UDCRunSubsystem::GetJobs() const { return Jobs; }
const TArray<FDCCriminal>& UDCRunSubsystem::GetRecruits() const { return Recruits; }
TArray<FDCGearItem> UDCRunSubsystem::GetGearCatalogue() const { return Gear; }

const FDCCriminal* UDCRunSubsystem::FindCriminal(int32 Id) const { return Roster.FindByPredicate([Id](const FDCCriminal& C) { return C.Id == Id; }); }
FDCCriminal* UDCRunSubsystem::FindCriminalMutable(int32 Id) { return Roster.FindByPredicate([Id](const FDCCriminal& C) { return C.Id == Id; }); }
const FDCJob* UDCRunSubsystem::FindJob(int32 Id) const { return Jobs.FindByPredicate([Id](const FDCJob& J) { return J.Id == Id; }); }
const FDCGearItem* UDCRunSubsystem::FindGear(FName Id) const { return Gear.FindByPredicate([Id](const FDCGearItem& G) { return G.Id == Id; }); }

FDCStatBlock UDCRunSubsystem::GetEffectiveStats(int32 CriminalId) const
{
	const FDCCriminal* C = FindCriminal(CriminalId);
	if (!C) { return FDCStatBlock(); }
	FDCStatBlock S = C->BaseStats;
	for (const TPair<EDCGearSlot, FName>& E : C->Equipped)
	{
		if (const FDCGearItem* G = FindGear(E.Value)) { S += G->StatBonus; }
	}
	return S;
}

int32 UDCRunSubsystem::GetEffectiveLuck(int32 CriminalId) const { const FDCCriminal* C = FindCriminal(CriminalId); return C ? C->Luck : 0; }
int32 UDCRunSubsystem::GetCash() const { return Cash; }
int32 UDCRunSubsystem::GetNight() const { return Night; }
int32 UDCRunSubsystem::GetHeistsCompleted() const { return HeistsCompleted; }
int32 UDCRunSubsystem::GetUpkeepDue() const { return 300 * Roster.Num() + 1000 * (HeistsCompleted / 4); }
int32 UDCRunSubsystem::GetRosterCap() const { return 4; }
int32 UDCRunSubsystem::GetMaxCrewsPerNight() const { return IsRoomBuilt(EDCRoomType::Garage) ? 2 : 1; }
int32 UDCRunSubsystem::GetRecruitRefreshIn() const { return 3 - ((Night - 1) % 3); }
int32 UDCRunSubsystem::GetUnlockedTier() const { return HeistsCompleted >= 12 ? 4 : HeistsCompleted >= 7 ? 3 : HeistsCompleted >= 3 ? 2 : 1; }
const TArray<FDCCrewPlan>& UDCRunSubsystem::GetCommittedPlans() const { return Plans; }
const FDCNightReport& UDCRunSubsystem::GetLastNightReport() const { return LastReport; }

FDCRunSummary UDCRunSubsystem::GetRunSummary() const
{
	FDCRunSummary S;
	S.Seed = Seed; S.NightsSurvived = Night - 1; S.HeistsCompleted = HeistsCompleted;
	S.EndReason = TEXT("Stub run");
	for (const FDCCriminal& C : Roster) { S.Starring.Add(C.GetDisplayName()); }
	for (const FDCCriminal& C : Fallen) { S.InMemoriam.Add(C.GetDisplayName()); }
	return S;
}

FDCSuccessBreakdown UDCRunSubsystem::PreviewPlan(const FDCCrewPlan& Plan) const
{
	FDCSuccessBreakdown B;
	const FDCJob* J = FindJob(Plan.JobId);
	if (!J) { B.InvalidReason = TEXT("No such job"); return B; }

	TArray<EDCCriminalType> Missing = J->RequiredRoles;
	float Score = 0.f;
	float WillSum = 0.f;
	for (int32 Id : Plan.CriminalIds)
	{
		const FDCCriminal* C = FindCriminal(Id);
		if (!C) { continue; }
		Missing.RemoveSingle(C->Type);
		const FDCStatBlock S = GetEffectiveStats(Id);
		for (const TPair<EDCStat, float>& W : J->StatWeights) { Score += W.Value * S.Get(W.Key) * (Score > 0 ? 0.25f : 1.f); }
		const float Fair = 0.30f + (C->Loyalty - 50) / 500.f;
		const float Will = FMath::Clamp(1.f - 2.f * FMath::Max(0.f, Plan.Cut - Fair), 0.2f, 1.f);
		B.Willingness.Add(Id, Will); B.FairCut.Add(Id, Fair);
		if (Will < 0.35f) { B.RefusingCriminalIds.Add(Id); }
		WillSum += Will;
	}
	B.MissingRoles = Missing;
	B.CrewScore = Score;
	B.Difficulty = J->Difficulty;
	B.Margin = (Score - J->Difficulty) / J->Difficulty;
	B.SkillChance = FMath::Clamp(0.55f + 0.75f * B.Margin, 0.05f, 0.95f);
	B.CrewWill = Plan.CriminalIds.Num() ? WillSum / Plan.CriminalIds.Num() : 1.f;
	B.WillFactor = 0.6f + 0.4f * B.CrewWill;
	B.SuccessChance = FMath::Clamp(B.SkillChance * B.WillFactor, 0.02f, 0.97f);
	B.CleanThreshold = B.SuccessChance * 0.6f;
	B.Payout = J->Payout;
	B.DirectorTake = FMath::RoundToInt(J->Payout * Plan.Cut);
	B.ArrestChanceOnFail = J->ArrestRisk;
	B.Lines.Add({ TEXT("Skill vs difficulty"), FString::Printf(TEXT("Crew %.0f vs %.0f"), Score, J->Difficulty), B.SkillChance, false, false });
	B.Lines.Add({ TEXT("Crew willingness"), FString::Printf(TEXT("Cut %d%%"), FMath::RoundToInt(Plan.Cut * 100)), B.WillFactor, true, B.WillFactor < 0.9f });

	if (Missing.Num()) { B.InvalidReason = FString::Printf(TEXT("Needs a %s"), DC::TypeName(Missing[0])); }
	else if (Plan.CriminalIds.Num() < J->MinCrew) { B.InvalidReason = TEXT("Crew too small"); }
	else if (Plan.CriminalIds.Num() > J->MaxCrew) { B.InvalidReason = TEXT("Crew too large"); }
	else if (B.RefusingCriminalIds.Num()) { B.InvalidReason = TEXT("Someone refuses at that cut"); }
	else { B.bValid = true; }
	return B;
}

bool UDCRunSubsystem::CommitPlan(const FDCCrewPlan& Plan, FString* OutError)
{
	const FDCSuccessBreakdown B = PreviewPlan(Plan);
	if (!B.bValid) { if (OutError) { *OutError = B.InvalidReason; } return false; }
	if (Plans.Num() >= GetMaxCrewsPerNight()) { if (OutError) { *OutError = TEXT("No more crews tonight"); } return false; }
	Plans.Add(Plan);
	for (int32 Id : Plan.CriminalIds) { if (FDCCriminal* C = FindCriminalMutable(Id)) { C->Status = EDCCriminalStatus::OnJob; } }
	for (FDCJob& J : Jobs) { if (J.Id == Plan.JobId) { J.bCommitted = true; } }
	OnPlanCommitted.Broadcast(Plan);
	OnRunStateChanged.Broadcast();
	return true;
}

void UDCRunSubsystem::CancelPlan(int32 JobId)
{
	for (int32 i = Plans.Num() - 1; i >= 0; --i)
	{
		if (Plans[i].JobId != JobId) { continue; }
		for (int32 Id : Plans[i].CriminalIds) { if (FDCCriminal* C = FindCriminalMutable(Id)) { C->Status = EDCCriminalStatus::Available; } }
		Plans.RemoveAt(i);
	}
	for (FDCJob& J : Jobs) { if (J.Id == JobId) { J.bCommitted = false; } }
	OnPlanCancelled.Broadcast(JobId);
	OnRunStateChanged.Broadcast();
}

bool UDCRunSubsystem::BuyGear(FName ItemId, int32 CriminalId, FString* OutError)
{
	const FDCGearItem* G = FindGear(ItemId);
	FDCCriminal* C = FindCriminalMutable(CriminalId);
	if (!G || !C) { if (OutError) { *OutError = TEXT("Invalid"); } return false; }
	if (Cash < G->Price) { if (OutError) { *OutError = TEXT("Not enough cash"); } return false; }
	Cash -= G->Price;
	C->Equipped.Add(G->Slot, G->Id);
	OnRunStateChanged.Broadcast();
	return true;
}

void UDCRunSubsystem::Unequip(int32 CriminalId, EDCGearSlot Slot)
{
	if (FDCCriminal* C = FindCriminalMutable(CriminalId)) { C->Equipped.Remove(Slot); OnRunStateChanged.Broadcast(); }
}

bool UDCRunSubsystem::Hire(int32 RecruitId, FString* OutError)
{
	const int32 Idx = Recruits.IndexOfByPredicate([RecruitId](const FDCCriminal& C) { return C.Id == RecruitId; });
	if (Idx == INDEX_NONE) { if (OutError) { *OutError = TEXT("Invalid"); } return false; }
	if (Cash < Recruits[Idx].HireCost) { if (OutError) { *OutError = TEXT("Not enough cash"); } return false; }
	if (Roster.Num() >= GetRosterCap()) { if (OutError) { *OutError = TEXT("Roster full"); } return false; }
	Cash -= Recruits[Idx].HireCost;
	Roster.Add(Recruits[Idx]);
	Recruits.RemoveAt(Idx);
	OnCriminalHired.Broadcast(Roster.Last());
	OnRunStateChanged.Broadcast();
	return true;
}

void UDCRunSubsystem::Dismiss(int32 CriminalId)
{
	Roster.RemoveAll([CriminalId](const FDCCriminal& C) { return C.Id == CriminalId; });
	OnRunStateChanged.Broadcast();
}

TArray<FDCRoom> UDCRunSubsystem::GetRooms() const
{
	TArray<FDCRoom> Out = Rooms;
	for (FDCRoom& R : Out)
	{
		R.bCanAct = R.Level < 3 && Cash >= R.NextCost && HeistsCompleted >= R.RequiredHeists;
		R.BlockedReason = R.Level >= 3 ? TEXT("Max level") : HeistsCompleted < R.RequiredHeists ? FString::Printf(TEXT("Needs %d heists"), R.RequiredHeists) : Cash < R.NextCost ? TEXT("Not enough cash") : TEXT("");
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
	if (R.State == EDCRoomState::Built || !R.bCanAct) { if (OutError) { *OutError = R.BlockedReason.IsEmpty() ? TEXT("Already built") : R.BlockedReason; } return false; }
	for (FDCRoom& X : Rooms)
	{
		if (X.Type != Type) { continue; }
		Cash -= X.NextCost; X.State = EDCRoomState::Built; X.Level = 1; X.NextCost = X.NextCost * 3 / 4;
		OnRoomChanged.Broadcast(X);
	}
	OnRunStateChanged.Broadcast();
	return true;
}

bool UDCRunSubsystem::UpgradeRoom(EDCRoomType Type, FString* OutError)
{
	const FDCRoom R = GetRoom(Type);
	if (R.State != EDCRoomState::Built || !R.bCanAct) { if (OutError) { *OutError = R.BlockedReason; } return false; }
	for (FDCRoom& X : Rooms)
	{
		if (X.Type != Type) { continue; }
		Cash -= X.NextCost; X.Level++;
		OnRoomChanged.Broadcast(X);
	}
	OnRunStateChanged.Broadcast();
	return true;
}

int32 UDCRunSubsystem::GetPatchUpCost(int32 CriminalId) const { return -1; }
bool UDCRunSubsystem::PatchUp(int32 CriminalId, FString* OutError) { if (OutError) { *OutError = TEXT("Stub"); } return false; }
int32 UDCRunSubsystem::GetBailCost(int32 CriminalId) const { return -1; }
bool UDCRunSubsystem::BailOut(int32 CriminalId, FString* OutError) { if (OutError) { *OutError = TEXT("Stub"); } return false; }

bool UDCRunSubsystem::HasPendingPerkOffer() const { return PerkOffer.Num() > 0; }
TArray<FDCPerk> UDCRunSubsystem::GetPerkOffer() const { return PerkOffer; }
bool UDCRunSubsystem::PickPerk(FName PerkId)
{
	const FDCPerk* P = PerkOffer.FindByPredicate([PerkId](const FDCPerk& X) { return X.Id == PerkId; });
	if (!P) { return false; }
	PerksTaken.Add(*P);
	PerkOffer.Reset();
	OnRunStateChanged.Broadcast();
	return true;
}
TArray<FDCPerk> UDCRunSubsystem::GetPerksTaken() const { return PerksTaken; }

TArray<FDCHeistResult> UDCRunSubsystem::EndNight()
{
	LastReport = FDCNightReport();
	LastReport.Night = Night;
	LastReport.CashBefore = Cash;
	for (const FDCCrewPlan& P : Plans)
	{
		const FDCSuccessBreakdown B = PreviewPlan(P);
		const FDCJob* J = FindJob(P.JobId);
		FDCHeistResult R;
		R.ResultId = NextId++;
		R.JobId = P.JobId; R.JobName = J ? J->DisplayName : TEXT("?"); R.Plan = P;
		R.SuccessChance = B.SuccessChance; R.CleanThreshold = B.CleanThreshold;
		R.Roll = R.FirstRoll = Rng.FRand();
		R.Outcome = R.Roll < B.CleanThreshold ? EDCHeistOutcome::Clean : R.Roll < B.SuccessChance ? EDCHeistOutcome::Messy : R.Roll > 0.9f + 0.1f * B.SuccessChance ? EDCHeistOutcome::Busted : EDCHeistOutcome::Failed;
		const bool bWin = R.Outcome == EDCHeistOutcome::Clean || R.Outcome == EDCHeistOutcome::Messy;
		R.PayoutTotal = bWin ? B.Payout : 0;
		R.DirectorTake = bWin ? B.DirectorTake : 0;
		R.FlavourLine = bWin ? TEXT("They got out clean, smelling of diamonds.") : TEXT("The getaway car wouldn't start.");
		for (int32 Id : P.CriminalIds)
		{
			FDCCrewMemberResult M; M.CriminalId = Id;
			if (FDCCriminal* C = FindCriminalMutable(Id)) { M.Name = C->GetShortName(); C->Status = EDCCriminalStatus::Available; M.XPGained = 30; M.NewLevel = C->Level; }
			R.Members.Add(M);
		}
		Cash += R.DirectorTake;
		LastReport.TotalTake += R.DirectorTake;
		if (bWin) { HeistsCompleted++; }
		LastReport.Results.Add(R);
	}
	Plans.Reset();
	LastReport.UpkeepPaid = GetUpkeepDue();
	Cash -= LastReport.UpkeepPaid;
	LastReport.CashAfter = Cash;
	LastReport.bGameOver = Cash < 0;
	Night++;
	MakeStubJobs();
	if (HeistsCompleted > 0 && HeistsCompleted % 3 == 0 && PerksTaken.Num() < HeistsCompleted / 3)
	{
		PerkOffer = {
			{ TEXT("method_acting"), TEXT("Method Acting"), TEXT("Faces get +10 Charm."), TEXT("Stay in character.") },
			{ TEXT("union_rates"), TEXT("Union Rates"), TEXT("Everyone's fair cut is +5%."), TEXT("Pay scale.") },
			{ TEXT("lucky_charm"), TEXT("Lucky Charm"), TEXT("+1 Luck for the whole roster."), TEXT("Knock on wood.") },
		};
		LastReport.bPerkOffered = true;
	}
	SetPhase(EDCGamePhase::Dailies);
	OnNightResolved.Broadcast(LastReport);
	OnRunStateChanged.Broadcast();
	return LastReport.Results;
}

int32 UDCRunSubsystem::GetRetakesRemaining() const { return 0; }
bool UDCRunSubsystem::UseRetake(int32 HeistResultId, FDCHeistResult* OutNewResult) { return false; }
