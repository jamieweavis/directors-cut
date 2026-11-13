// Director's Cut — UDCRunSubsystem. R0 STUB (lead), so UI, World and Audio can build against the v2 API.
// Believable fake data and working timers: the clock ticks, missions run and resolve with a plain roll, and rooms build.
// No JSON, no saves, no traits/perks logic. Agent A replaces this whole file (and adds Sim/*) with the real simulation.
#include "Core/DCRunSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	constexpr float StartHour = 7.f, DawnHour = 6.f, DuskHour = 20.f, GameMinPerSec = 1.f;
	const TCHAR* Firsts[] = { TEXT("Marco"), TEXT("Dana"), TEXT("Vic"), TEXT("Lena"), TEXT("Theo"), TEXT("Rosa"), TEXT("Kai"), TEXT("Nina") };
	const TCHAR* Nicks[] = { TEXT("Wrench"), TEXT("Static"), TEXT("Ghost"), TEXT("Silk"), TEXT("Brick"), TEXT("Tumbler"), TEXT("Spark"), TEXT("Lucky") };
	const TCHAR* Lasts[] = { TEXT("Reyes"), TEXT("Kowalski"), TEXT("Okafor"), TEXT("Brandt"), TEXT("Moreau"), TEXT("Sato"), TEXT("Quinn"), TEXT("Vance") };

	FName StubAbility(EDCCriminalType T, int32 K)
	{
		static const TCHAR* A[6][2] = {
			{ TEXT("getaway"), TEXT("boost") }, { TEXT("intimidate"), TEXT("brawler") }, { TEXT("loop_feed"), TEXT("bypass") },
			{ TEXT("shadow"), TEXT("climber") }, { TEXT("smooth_talk"), TEXT("insider") }, { TEXT("lockpick"), TEXT("drill") } };
		return A[(int32)T][K & 1];
	}
	const TMap<FName, FDCAbility>& StubAbilities()
	{
		static TMap<FName, FDCAbility> M;
		if (M.Num() == 0)
		{
			auto Add = [](const TCHAR* Id, const TCHAR* N, const TCHAR* Icon, EDCThreat T) { FDCAbility A; A.Id = Id; A.DisplayName = N; A.Icon = Icon; A.Counters = T; M.Add(A.Id, A); };
			Add(TEXT("getaway"), TEXT("Getaway Driver"), TEXT("car-front"), EDCThreat::Pursuit);
			Add(TEXT("boost"), TEXT("Hotwire"), TEXT("key-round"), EDCThreat::Locks);
			Add(TEXT("intimidate"), TEXT("Intimidate"), TEXT("hand"), EDCThreat::Witnesses);
			Add(TEXT("brawler"), TEXT("Brawler"), TEXT("swords"), EDCThreat::Guards);
			Add(TEXT("loop_feed"), TEXT("Loop the Feed"), TEXT("cctv"), EDCThreat::Cameras);
			Add(TEXT("bypass"), TEXT("Bypass"), TEXT("zap-off"), EDCThreat::Alarms);
			Add(TEXT("shadow"), TEXT("Shadow"), TEXT("moon"), EDCThreat::Patrols);
			Add(TEXT("climber"), TEXT("Climber"), TEXT("mountain"), EDCThreat::Guards);
			Add(TEXT("smooth_talk"), TEXT("Smooth Talk"), TEXT("message-circle"), EDCThreat::Witnesses);
			Add(TEXT("insider"), TEXT("Insider"), TEXT("id-card"), EDCThreat::Alarms);
			Add(TEXT("lockpick"), TEXT("Lockpick"), TEXT("key"), EDCThreat::Locks);
			Add(TEXT("drill"), TEXT("Drill"), TEXT("drill"), EDCThreat::Vault);
		}
		return M;
	}
	const TMap<FName, FDCTrait>& StubTraits()
	{
		static TMap<FName, FDCTrait> M;
		if (M.Num() == 0)
		{
			auto Add = [](const TCHAR* Id, const TCHAR* N, const TCHAR* D, const TCHAR* Icon, bool bNeg) { FDCTrait T; T.Id = Id; T.DisplayName = N; T.Description = D; T.Icon = Icon; T.bNegative = bNeg; M.Add(T.Id, T); };
			Add(TEXT("night_owl"), TEXT("Night Owl"), TEXT("+15% at night"), TEXT("moon"), false);
			Add(TEXT("lead_foot"), TEXT("Lead Foot"), TEXT("-25% duration"), TEXT("gauge"), false);
			Add(TEXT("greedy"), TEXT("Greedy"), TEXT("Wants a bigger cut"), TEXT("hand-coins"), true);
		}
		return M;
	}
}

UDCRunSubsystem* UDCRunSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = (WorldContext && GEngine) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UDCRunSubsystem>() : nullptr;
}

void UDCRunSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UDCRunSubsystem::Deinitialize() { bActive = false; Super::Deinitialize(); }
TStatId UDCRunSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UDCRunSubsystem, STATGROUP_Tickables); }
bool UDCRunSubsystem::IsTickable() const { return bActive && !bPaused && Phase == EDCGamePhase::Playing && !IsTemplate(); }

// ─── stub generation ─────────────────────────────────────────────────────────

static FDCCriminal MakeStubCriminal(FRandomStream& Rng, int32 Id, EDCCriminalType T)
{
	FDCCriminal C;
	C.Id = Id;
	C.FirstName = Firsts[Rng.RandHelper(8)]; C.Nickname = Nicks[Rng.RandHelper(8)]; C.LastName = Lasts[Rng.RandHelper(8)];
	C.Type = T;
	C.Quality = (EDCQuality)FMath::Min(2, Rng.RandHelper(10) / 5);
	C.Seed = Rng.GetUnsignedInt();
	C.Level = 1 + Rng.RandHelper(3);
	C.XPToNext = 100 * C.Level;
	C.Abilities.Add(StubAbility(T, Rng.RandHelper(2)));
	if (C.Quality == EDCQuality::Elite) { C.Abilities.AddUnique(StubAbility(T, 0)); C.Abilities.AddUnique(StubAbility(T, 1)); }
	TArray<FName> TraitIds; StubTraits().GetKeys(TraitIds);
	C.Traits.Add(TraitIds[Rng.RandHelper(TraitIds.Num())]);
	C.HireCost = 1500 + 500 * C.Level;
	return C;
}

static FDCMission MakeStubMission(FRandomStream& Rng, int32 Id, double Now)
{
	struct FT { const TCHAR* Name; const TCHAR* Line; const TCHAR* Icon; int32 Tier; int32 Size; float Dur; TArray<EDCThreat> Threats; int32 Cost; int32 Pay; float Arrest; bool bNight; };
	static const FT Ts[] = {
		{ TEXT("Corner Store"), TEXT("Till's full on Fridays."), TEXT("store"), 1, 1, 60.f, { EDCThreat::Witnesses }, 0, 1800, 0.05f, false },
		{ TEXT("Car Boost"), TEXT("Valet's on a smoke break."), TEXT("car"), 1, 1, 90.f, { EDCThreat::Locks, EDCThreat::Pursuit }, 200, 2600, 0.05f, true },
		{ TEXT("Phone Shop"), TEXT("New stock arrived today."), TEXT("smartphone"), 1, 2, 120.f, { EDCThreat::Cameras, EDCThreat::Alarms }, 400, 4200, 0.08f, true },
		{ TEXT("Warehouse Job"), TEXT("Container 14. Don't ask."), TEXT("container"), 2, 2, 300.f, { EDCThreat::Guards, EDCThreat::Locks, EDCThreat::Patrols }, 1200, 11000, 0.15f, true },
		{ TEXT("Jewellery Store"), TEXT("Diamonds in the back case."), TEXT("gem"), 3, 3, 600.f, { EDCThreat::Cameras, EDCThreat::Vault, EDCThreat::Witnesses, EDCThreat::Pursuit }, 3000, 30000, 0.25f, false },
		{ TEXT("Armoured Truck"), TEXT("Route 9, Thursday 4 a.m."), TEXT("truck"), 4, 4, 1200.f, { EDCThreat::Guards, EDCThreat::Guards, EDCThreat::Vault, EDCThreat::Pursuit, EDCThreat::Patrols }, 8000, 90000, 0.35f, true },
	};
	const FT& T = Ts[Rng.RandHelper(UE_ARRAY_COUNT(Ts))];
	FDCMission M;
	M.Id = Id; M.TemplateId = FName(T.Name); M.DisplayName = T.Name; M.Flavour = T.Line; M.Icon = T.Icon;
	M.Tier = T.Tier; M.Level = T.Tier * 2 - 1 + Rng.RandHelper(2); M.CrewSize = T.Size; M.Duration = T.Dur;
	M.Threats = T.Threats; M.Cost = T.Cost; M.Payout = T.Pay; M.XPReward = 40 * T.Tier; M.ArrestRisk = T.Arrest; M.bNight = T.bNight;
	M.bRare = Rng.FRand() < 0.1f;
	M.ExpiresAt = Now + 300.0 + Rng.RandHelper(300);
	return M;
}

// ─── lifecycle ───────────────────────────────────────────────────────────────

void UDCRunSubsystem::StartNewRun(int32 InSeed)
{
	Seed = InSeed != 0 ? InSeed : FMath::RandRange(1, 999999);
	Rng.Initialize(Seed);
	bActive = true; bPaused = false; RunTime = 0.0; Cash = 5000; NextId = 1;
	Roster.Reset(); Fallen.Reset(); Recruits.Reset(); Missions.Reset(); Reports.Reset(); Stash.Reset(); PerkOffer.Reset(); PerksTaken.Reset();
	for (int32 i = 0; i < 4; ++i) { Roster.Add(MakeStubCriminal(Rng, NextId++, (EDCCriminalType)(i % 6))); Roster.Last().HireCost = 0; }
	for (int32 i = 0; i < 3; ++i) { Recruits.Add(MakeStubCriminal(Rng, NextId++, (EDCCriminalType)Rng.RandHelper(6))); }
	for (int32 i = 0; i < 4; ++i) { Missions.Add(MakeStubMission(Rng, NextId++, RunTime)); }
	Missions[0].Tier = 1; Missions[0].CrewSize = 1; Missions[0].Duration = 60.f;
	Rooms.Reset();
	for (int32 i = 0; i < (int32)EDCRoomType::MAX; ++i)
	{
		FDCRoom R; R.Type = (EDCRoomType)i;
		R.State = i == 0 ? EDCRoomState::Built : EDCRoomState::Locked; R.Level = i == 0 ? 1 : 0;
		Rooms.Add(R);
	}
	Phase = EDCGamePhase::Playing;
	OnPhaseChanged.Broadcast(Phase);
	OnRunStateChanged.Broadcast();
}

bool UDCRunSubsystem::HasSave() const { return false; }
bool UDCRunSubsystem::ContinueRun() { return false; }
void UDCRunSubsystem::SaveRun() {}
void UDCRunSubsystem::ExitToTitle() { bActive = false; Phase = EDCGamePhase::Title; OnPhaseChanged.Broadcast(Phase); OnRunStateChanged.Broadcast(); }
void UDCRunSubsystem::AbandonRun() { ExitToTitle(); }
bool UDCRunSubsystem::HasActiveRun() const { return bActive; }
EDCGamePhase UDCRunSubsystem::GetPhase() const { return Phase; }
int32 UDCRunSubsystem::GetSeed() const { return Seed; }
void UDCRunSubsystem::SetPaused(bool b) { if (bPaused != b) { bPaused = b; OnPauseChanged.Broadcast(b); } }
bool UDCRunSubsystem::IsPaused() const { return bPaused; }
void UDCRunSubsystem::SetTimeScale(float S) { TimeScale = FMath::Max(0.f, S); }
float UDCRunSubsystem::GetTimeScale() const { return TimeScale; }

// ─── ticking ─────────────────────────────────────────────────────────────────

void UDCRunSubsystem::Tick(float DeltaTime) { AdvanceTime(DeltaTime * TimeScale); }

void UDCRunSubsystem::AdvanceTime(double Seconds)
{
	if (!bActive || Seconds <= 0.0) { return; }
	auto DawnIndex = [this]() { return FMath::FloorToInt((StartHour + RunTime * GameMinPerSec / 60.0 - DawnHour) / 24.0); };
	const int32 DawnBefore = DawnIndex();
	RunTime += Seconds;
	bool bChanged = false;

	for (int32 i = Missions.Num() - 1; i >= 0; --i)
	{
		FDCMission& M = Missions[i];
		if (M.bInProgress && RunTime >= M.EndsAt)
		{
			FDCMissionReport R;
			R.ReportId = NextId++; R.Mission = M; R.Roll = Rng.FRand(); R.CompletedAt = RunTime;
			R.Outcome = R.Roll < M.LockedChance ? EDCMissionOutcome::Success : (R.Roll > 0.9f ? EDCMissionOutcome::Busted : EDCMissionOutcome::Failed);
			R.bBonus = R.Outcome == EDCMissionOutcome::Success && M.LockedChance > 1.f && Rng.FRand() < M.LockedChance - 1.f;
			R.PayoutTotal = R.Outcome == EDCMissionOutcome::Success ? M.Payout * (R.bBonus ? 2 : 1) : 0;
			R.DirectorTake = FMath::RoundToInt(R.PayoutTotal * M.Cut);
			Cash += R.DirectorTake;
			for (int32 Id : M.CrewIds)
			{
				if (FDCCriminal* C = Roster.FindByPredicate([Id](const FDCCriminal& X) { return X.Id == Id; }))
				{
					FDCCrewMemberResult MR; MR.CriminalId = Id; MR.Name = C->GetDisplayName();
					MR.XPGained = R.Outcome == EDCMissionOutcome::Success ? M.XPReward : M.XPReward / 4;
					C->XP += MR.XPGained; C->MissionsRun++;
					C->Status = EDCCriminalStatus::Available; C->ActiveMissionId = INDEX_NONE;
					if (C->XP >= C->XPToNext) { C->XP -= C->XPToNext; C->Level++; C->XPToNext = 100 * C->Level; MR.bLevelledUp = true; MR.NewLevel = C->Level; OnCriminalLevelUp.Broadcast(Id, C->Level); }
					if (R.Outcome != EDCMissionOutcome::Success && Rng.FRand() < 0.4f)
					{
						MR.bInjured = true; MR.InjurySeconds = 120.f;
						C->Status = EDCCriminalStatus::Injured; C->InjuredUntil = RunTime + MR.InjurySeconds;
					}
					R.Members.Add(MR);
				}
			}
			Reports.Insert(R, 0);
			Missions.RemoveAt(i);
			OnMissionCompleted.Broadcast(R);
			bChanged = true;
		}
		else if (!M.bInProgress && M.ExpiresAt >= 0.0 && RunTime >= M.ExpiresAt)
		{
			Missions.RemoveAt(i);
			bChanged = true;
		}
	}
	for (FDCCriminal& C : Roster)
	{
		if (C.Status == EDCCriminalStatus::Injured && RunTime >= C.InjuredUntil) { C.Status = EDCCriminalStatus::Available; OnCriminalHealed.Broadcast(C.Id); bChanged = true; }
	}
	for (FDCRoom& R : Rooms)
	{
		if (R.State == EDCRoomState::Building && RunTime >= R.BuildEndsAt) { R.State = EDCRoomState::Built; R.Level++; OnRoomChanged.Broadcast(GetRoom(R.Type)); bChanged = true; }
	}
	if (Missions.FilterByPredicate([](const FDCMission& M) { return !M.bInProgress; }).Num() < 4 && FMath::Fmod(RunTime, 30.0) < Seconds)
	{
		Missions.Add(MakeStubMission(Rng, NextId++, RunTime));
		OnMissionAdded.Broadcast(Missions.Last());
		bChanged = true;
	}
	if (DawnIndex() > DawnBefore)
	{
		const int32 Due = GetUpkeepDue();
		Cash -= Due;
		OnPayday.Broadcast(GetDay(), Due);
		OnDayStarted.Broadcast(GetDay());
		bChanged = true;
	}
	if (bChanged) { OnRunStateChanged.Broadcast(); }
}

// ─── clock ───────────────────────────────────────────────────────────────────

double UDCRunSubsystem::GetRunTime() const { return RunTime; }
static double TotalHours(double RunTime) { return StartHour + RunTime * GameMinPerSec / 60.0; }
int32 UDCRunSubsystem::GetDay() const { return 1 + (int32)(TotalHours(RunTime) / 24.0); }
float UDCRunSubsystem::GetHourOfDay() const { return (float)FMath::Fmod(TotalHours(RunTime), 24.0); }
bool UDCRunSubsystem::IsNight() const { const float H = GetHourOfDay(); return H >= DuskHour || H < DawnHour; }
float UDCRunSubsystem::GetSunAlpha() const { const float H = GetHourOfDay(); return FMath::Clamp(FMath::Sin((H - DawnHour) / (DuskHour - DawnHour) * PI), 0.f, 1.f); }
double UDCRunSubsystem::GetSecondsUntilPayday() const
{
	const double H = GetHourOfDay();
	const double HoursLeft = H < DawnHour ? DawnHour - H : 24.0 - H + DawnHour;
	return HoursLeft * 60.0 / GameMinPerSec;
}
float UDCRunSubsystem::GetGameMinutesPerSecond() const { return GameMinPerSec; }

// ─── reads ───────────────────────────────────────────────────────────────────

const TArray<FDCCriminal>& UDCRunSubsystem::GetRoster() const { return Roster; }
const TArray<FDCCriminal>& UDCRunSubsystem::GetFallen() const { return Fallen; }
const TArray<FDCMission>& UDCRunSubsystem::GetMissions() const { return Missions; }
const TArray<FDCCriminal>& UDCRunSubsystem::GetRecruits() const { return Recruits; }
TArray<FDCGearItem> UDCRunSubsystem::GetGearCatalogue() const
{
	TArray<FDCGearItem> Out;
	auto Add = [&](const TCHAR* Id, const TCHAR* N, const TCHAR* Icon, EDCGearSlot S, int32 Price, float Power, EDCThreat Ctr)
	{ FDCGearItem G; G.Id = Id; G.DisplayName = N; G.Icon = Icon; G.Slot = S; G.Price = Price; G.Power = Power; G.Counters = Ctr; G.bAvailable = Cash >= Price; G.UnavailableReason = G.bAvailable ? FString() : TEXT("$"); Out.Add(G); };
	Add(TEXT("bolt_cutters"), TEXT("Bolt Cutters"), TEXT("scissors"), EDCGearSlot::Tool, 800, 0.f, EDCThreat::Locks);
	Add(TEXT("jammer"), TEXT("Signal Jammer"), TEXT("radio-tower"), EDCGearSlot::Gadget, 1500, 0.5f, EDCThreat::Alarms);
	Add(TEXT("hoodie"), TEXT("Dark Hoodie"), TEXT("shirt"), EDCGearSlot::Outfit, 600, 0.5f, EDCThreat::MAX);
	return Out;
}
const TArray<FName>& UDCRunSubsystem::GetStash() const { return Stash; }
const FDCCriminal* UDCRunSubsystem::FindCriminal(int32 Id) const
{
	if (const FDCCriminal* C = Roster.FindByPredicate([Id](const FDCCriminal& X) { return X.Id == Id; })) { return C; }
	if (const FDCCriminal* C = Recruits.FindByPredicate([Id](const FDCCriminal& X) { return X.Id == Id; })) { return C; }
	return Fallen.FindByPredicate([Id](const FDCCriminal& X) { return X.Id == Id; });
}
const FDCMission* UDCRunSubsystem::FindMission(int32 Id) const { return Missions.FindByPredicate([Id](const FDCMission& M) { return M.Id == Id; }); }
const FDCGearItem* UDCRunSubsystem::FindGear(FName) const { return nullptr; }
const FDCAbility* UDCRunSubsystem::FindAbility(FName Id) const { return StubAbilities().Find(Id); }
const FDCTrait* UDCRunSubsystem::FindTrait(FName Id) const { return StubTraits().Find(Id); }
const FDCPerk* UDCRunSubsystem::FindPerk(FName) const { return nullptr; }
float UDCRunSubsystem::GetEffectiveLevel(int32 Id) const { const FDCCriminal* C = FindCriminal(Id); return C ? (float)C->Level : 0.f; }
TArray<FName> UDCRunSubsystem::GetAllTraits(int32 Id) const { const FDCCriminal* C = FindCriminal(Id); return C ? C->Traits : TArray<FName>(); }
TArray<EDCThreat> UDCRunSubsystem::GetCounters(int32 Id) const
{
	TArray<EDCThreat> Out;
	if (const FDCCriminal* C = FindCriminal(Id)) { for (FName A : C->Abilities) { if (const FDCAbility* Ab = FindAbility(A)) { Out.Add(Ab->Counters); } } }
	return Out;
}
int32 UDCRunSubsystem::GetGearSlotCount() const { return IsRoomBuilt(EDCRoomType::Workshop) ? 3 : 2; }
int32 UDCRunSubsystem::GetCash() const { return Cash; }
int32 UDCRunSubsystem::GetMissionsSucceeded() const { return Reports.FilterByPredicate([](const FDCMissionReport& R) { return R.Outcome == EDCMissionOutcome::Success; }).Num(); }
int32 UDCRunSubsystem::GetUnlockedTier() const { return 4; }
int32 UDCRunSubsystem::GetRosterCap() const { return 4 + 2 * GetRoom(EDCRoomType::Lounge).Level; }
int32 UDCRunSubsystem::GetMaxActiveMissions() const { return 2 + GetRoom(EDCRoomType::Garage).Level; }
int32 UDCRunSubsystem::GetActiveMissionCount() const { return Missions.FilterByPredicate([](const FDCMission& M) { return M.bInProgress; }).Num(); }
int32 UDCRunSubsystem::GetUpkeepDue() const { return 250 * Roster.Num(); }
double UDCRunSubsystem::GetRecruitRefreshIn() const { return 600.0 - FMath::Fmod(RunTime, 600.0); }
int32 UDCRunSubsystem::GetRecruitRefreshCost() const { return 500; }
double UDCRunSubsystem::GetNextMissionIn() const { return 30.0 - FMath::Fmod(RunTime, 30.0); }
double UDCRunSubsystem::GetMissionTimeRemaining(int32 Id) const { const FDCMission* M = FindMission(Id); return (M && M->bInProgress) ? FMath::Max(0.0, M->EndsAt - RunTime) : 0.0; }
float UDCRunSubsystem::GetMissionProgress(int32 Id) const
{
	const FDCMission* M = FindMission(Id);
	return (M && M->bInProgress && M->EndsAt > M->StartedAt) ? (float)FMath::Clamp((RunTime - M->StartedAt) / (M->EndsAt - M->StartedAt), 0.0, 1.0) : 0.f;
}
FDCRunSummary UDCRunSubsystem::GetRunSummary() const
{
	FDCRunSummary S; S.Seed = Seed; S.DaysSurvived = GetDay(); S.PlayTime = RunTime; S.MissionsSucceeded = GetMissionsSucceeded(); S.MissionsAttempted = Reports.Num();
	for (const FDCCriminal& C : Roster) { S.Crew.Add(C.GetDisplayName()); }
	return S;
}

// ─── missions ────────────────────────────────────────────────────────────────

FDCSuccessBreakdown UDCRunSubsystem::PreviewPlan(const FDCCrewPlan& Plan) const
{
	FDCSuccessBreakdown B;
	const FDCMission* M = FindMission(Plan.MissionId);
	if (!M) { B.Block = EDCPlanBlock::Expired; return B; }
	TArray<int32> Used;
	for (EDCThreat T : M->Threats)
	{
		FDCThreatSlot S; S.Threat = T;
		for (int32 Id : Plan.CriminalIds)
		{
			if (const FDCCriminal* C = FindCriminal(Id))
			{
				for (FName A : C->Abilities)
				{
					const FDCAbility* Ab = FindAbility(A);
					if (Ab && Ab->Counters == T && !Used.Contains(Id * 100 + C->Abilities.IndexOfByKey(A))) { S.CounteredBy = Id; S.Source = A; Used.Add(Id * 100 + C->Abilities.IndexOfByKey(A)); break; }
				}
			}
			if (S.IsCountered()) { break; }
		}
		B.Threats.Add(S);
	}
	float Crew = 0.f;
	for (int32 Id : Plan.CriminalIds)
	{
		const FDCCriminal* C = FindCriminal(Id);
		const float F = C ? FMath::Clamp(1.f - 0.25f * FMath::Max(0, M->Level - C->Level), 0.1f, 1.f) : 0.f;
		B.MemberFactor.Add(Id, F); B.Willingness.Add(Id, 1.f); Crew += F;
	}
	const float Points = M->CrewSize + 1.5f * M->Threats.Num();
	B.CrewChance = Crew / Points;
	B.ThreatChance = 1.5f * B.Threats.FilterByPredicate([](const FDCThreatSlot& S) { return S.IsCountered(); }).Num() / Points;
	if (M->bNight && IsNight()) { FDCModifierHit H; H.Id = TEXT("night"); H.Icon = TEXT("moon"); H.Label = TEXT("Night"); H.ChanceDelta = 0.1f; B.Modifiers.Add(H); B.ModifierChance += 0.1f; }
	B.SuccessChance = FMath::Clamp(B.CrewChance + B.ThreatChance + B.ModifierChance, 0.f, 2.f);
	B.BonusChance = FMath::Max(0.f, B.SuccessChance - 1.f);
	B.ArrestChance = M->ArrestRisk; B.Duration = M->Duration; B.Cost = M->Cost; B.Payout = M->Payout;
	B.DirectorTake = FMath::RoundToInt(M->Payout * Plan.Cut);
	B.Block = Plan.CriminalIds.Num() < M->CrewSize ? EDCPlanBlock::SlotsEmpty
		: M->bInProgress ? EDCPlanBlock::Expired
		: Cash < M->Cost ? EDCPlanBlock::NoCash
		: GetActiveMissionCount() >= GetMaxActiveMissions() ? EDCPlanBlock::NoCapacity : EDCPlanBlock::None;
	for (int32 Id : Plan.CriminalIds) { const FDCCriminal* C = FindCriminal(Id); if (!C || !C->IsAvailable()) { B.Block = EDCPlanBlock::CrewUnavailable; } }
	B.bValid = B.Block == EDCPlanBlock::None;
	return B;
}

FDCCrewPlan UDCRunSubsystem::SuggestPlan(int32 MissionId, float Cut) const
{
	FDCCrewPlan P; P.MissionId = MissionId; P.Cut = Cut;
	if (const FDCMission* M = FindMission(MissionId))
	{
		for (const FDCCriminal& C : Roster) { if (P.CriminalIds.Num() < M->CrewSize && C.IsAvailable()) { P.CriminalIds.Add(C.Id); } }
	}
	return P;
}

bool UDCRunSubsystem::StartMission(const FDCCrewPlan& Plan, EDCPlanBlock* OutBlock)
{
	const FDCSuccessBreakdown B = PreviewPlan(Plan);
	if (OutBlock) { *OutBlock = B.Block; }
	if (!B.bValid) { return false; }
	FDCMission* M = Missions.FindByPredicate([&](const FDCMission& X) { return X.Id == Plan.MissionId; });
	Cash -= M->Cost;
	M->bInProgress = true; M->StartedAt = RunTime; M->EndsAt = RunTime + B.Duration; M->CrewIds = Plan.CriminalIds; M->Cut = Plan.Cut; M->LockedChance = B.SuccessChance;
	for (int32 Id : Plan.CriminalIds)
	{
		if (FDCCriminal* C = Roster.FindByPredicate([Id](const FDCCriminal& X) { return X.Id == Id; })) { C->Status = EDCCriminalStatus::OnMission; C->ActiveMissionId = M->Id; }
	}
	OnMissionStarted.Broadcast(*M);
	OnRunStateChanged.Broadcast();
	return true;
}

const TArray<FDCMissionReport>& UDCRunSubsystem::GetReports() const { return Reports; }
int32 UDCRunSubsystem::GetUnseenReportCount() const { return Reports.FilterByPredicate([](const FDCMissionReport& R) { return !R.bSeen; }).Num(); }
void UDCRunSubsystem::MarkReportSeen(int32 Id) { for (FDCMissionReport& R : Reports) { if (R.ReportId == Id) { R.bSeen = true; } } OnRunStateChanged.Broadcast(); }
void UDCRunSubsystem::MarkAllReportsSeen() { for (FDCMissionReport& R : Reports) { R.bSeen = true; } OnRunStateChanged.Broadcast(); }

// ─── economy ─────────────────────────────────────────────────────────────────

bool UDCRunSubsystem::BuyGear(FName, int32, FString* OutError) { if (OutError) { *OutError = TEXT("stub"); } return false; }
bool UDCRunSubsystem::EquipFromStash(FName, int32, FString* OutError) { if (OutError) { *OutError = TEXT("stub"); } return false; }
void UDCRunSubsystem::Unequip(int32, EDCGearSlot) {}
bool UDCRunSubsystem::SellFromStash(FName) { return false; }
bool UDCRunSubsystem::Hire(int32 RecruitId, FString* OutError)
{
	const int32 Idx = Recruits.IndexOfByPredicate([RecruitId](const FDCCriminal& C) { return C.Id == RecruitId; });
	if (Idx == INDEX_NONE || Cash < Recruits[Idx].HireCost || Roster.Num() >= GetRosterCap()) { if (OutError) { *OutError = TEXT("Can't hire"); } return false; }
	Cash -= Recruits[Idx].HireCost;
	Roster.Add(Recruits[Idx]); Roster.Last().HireCost = 0;
	Recruits.RemoveAt(Idx);
	OnCriminalHired.Broadcast(Roster.Last());
	OnRunStateChanged.Broadcast();
	return true;
}
void UDCRunSubsystem::Dismiss(int32 Id) { Roster.RemoveAll([Id](const FDCCriminal& C) { return C.Id == Id && C.IsAvailable(); }); OnRunStateChanged.Broadcast(); }
bool UDCRunSubsystem::RefreshRecruits() { return false; }

// ─── rooms ───────────────────────────────────────────────────────────────────

TArray<FDCRoom> UDCRunSubsystem::GetRooms() const
{
	TArray<FDCRoom> Out;
	for (const FDCRoom& R : Rooms) { Out.Add(GetRoom(R.Type)); }
	return Out;
}
FDCRoom UDCRunSubsystem::GetRoom(EDCRoomType Type) const
{
	FDCRoom R; R.Type = Type;
	if (const FDCRoom* S = Rooms.FindByPredicate([Type](const FDCRoom& X) { return X.Type == Type; })) { R = *S; }
	R.DisplayName = DC::RoomName(Type); R.Icon = DC::RoomIcon(Type);
	R.Description = TEXT("Stub room");
	R.LevelEffects = { TEXT("Level 1"), TEXT("Level 2"), TEXT("Level 3") };
	R.bMaxLevel = R.Level >= 3;
	R.NextCost = 2000 * (R.Level + 1); R.NextBuildTime = 30.f * (R.Level + 1);
	R.Block = R.bMaxLevel ? EDCPlanBlock::Locked : (Cash < R.NextCost ? EDCPlanBlock::NoCash : EDCPlanBlock::None);
	R.bCanAct = R.Block == EDCPlanBlock::None && R.State != EDCRoomState::Building && !IsAnyRoomBuilding();
	return R;
}
bool UDCRunSubsystem::IsRoomBuilt(EDCRoomType Type) const { return GetRoom(Type).Level >= 1; }
bool UDCRunSubsystem::BuildRoom(EDCRoomType Type, EDCPlanBlock* OutBlock)
{
	const FDCRoom V = GetRoom(Type);
	if (OutBlock) { *OutBlock = V.Block; }
	if (!V.bCanAct) { return false; }
	FDCRoom* R = Rooms.FindByPredicate([Type](const FDCRoom& X) { return X.Type == Type; });
	Cash -= V.NextCost;
	R->State = EDCRoomState::Building; R->BuildStartedAt = RunTime; R->BuildEndsAt = RunTime + V.NextBuildTime;
	OnRoomChanged.Broadcast(GetRoom(Type));
	OnRunStateChanged.Broadcast();
	return true;
}
bool UDCRunSubsystem::IsAnyRoomBuilding() const { return Rooms.ContainsByPredicate([](const FDCRoom& R) { return R.State == EDCRoomState::Building; }); }
int32 UDCRunSubsystem::GetPatchUpCost(int32) const { return -1; }
bool UDCRunSubsystem::PatchUp(int32) { return false; }
int32 UDCRunSubsystem::GetBailCost(int32) const { return -1; }
bool UDCRunSubsystem::BailOut(int32) { return false; }

// ─── perks ───────────────────────────────────────────────────────────────────

bool UDCRunSubsystem::HasPendingPerkOffer() const { return PerkOffer.Num() > 0; }
TArray<FDCPerk> UDCRunSubsystem::GetPerkOffer() const { return PerkOffer; }
bool UDCRunSubsystem::PickPerk(FName) { PerkOffer.Reset(); return true; }
TArray<FDCPerk> UDCRunSubsystem::GetPerksTaken() const { return PerksTaken; }
