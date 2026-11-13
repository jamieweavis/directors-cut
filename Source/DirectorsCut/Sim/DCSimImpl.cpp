// Director's Cut — Sim internals: modifiers, generation, resolver, levelling (Agent A).
#include "Sim/DCSimImpl.h"
#include "DirectorsCut.h"

// ═════════════════════════════════════════════════════════════════════════════
// Modifier stack
// ═════════════════════════════════════════════════════════════════════════════

bool FDCModStack::Has(EDCModKind K) const
{
	return Mods.ContainsByPredicate([K](const FDCActiveMod& A) { return A.Mod.Kind == K; });
}
float FDCModStack::Max(EDCModKind K, float Base) const
{
	float V = Base;
	for (const FDCActiveMod& A : Mods) { if (A.Mod.Kind == K) { V = FMath::Max(V, A.Mod.Value); } }
	return V;
}
float FDCModStack::Sum(EDCModKind K) const
{
	float V = 0.f;
	for (const FDCActiveMod& A : Mods) { if (A.Mod.Kind == K) { V += A.Mod.Value; } }
	return V;
}
float FDCModStack::Product(EDCModKind K) const
{
	float V = 1.f;
	for (const FDCActiveMod& A : Mods) { if (A.Mod.Kind == K && A.Mod.Condition.IsEmpty()) { V *= A.Mod.Value; } }
	return V;
}

void FDCSimImpl::SetPhase(S_t& S, EDCGamePhase P)
{
	if (S.Phase != P) { S.Phase = P; S.OnPhaseChanged.Broadcast(P); }
}

FDCModStack FDCSimImpl::BuildMods(const S_t& S)
{
	FDCModStack M;
	if (!S.Data.IsValid()) { return M; }
	for (const FDCRoom& R : S.Rooms)
	{
		if (R.State != EDCRoomState::Built || R.Level <= 0) { continue; }
		const FDCRoomDef& Def = D(S).GetRoom(R.Type);
		const int32 L = FMath::Clamp(R.Level, 1, Def.Levels.Num()) - 1;
		if (!Def.Levels.IsValidIndex(L)) { continue; }
		for (const FDCModifier& Mod : Def.Levels[L].Modifiers) { M.Mods.Add({ Mod, Def.DisplayName, false }); }
	}
	for (const FDCPerk& P : S.PerksTaken)
	{
		if (const FDCPerkDef* Def = D(S).FindPerk(P.Id))
		{
			for (const FDCModifier& Mod : Def->Modifiers) { M.Mods.Add({ Mod, Def->Perk.DisplayName, true }); }
		}
	}
	return M;
}

int32 FDCSimImpl::RosterCap(const S_t& S, const FDCModStack& M) { return FMath::RoundToInt(M.Max(EDCModKind::RosterCap, (float)B(S).BaseRosterCap)); }
int32 FDCSimImpl::CrewsPerNight(const S_t& S, const FDCModStack& M) { return FMath::Max(1, FMath::RoundToInt(M.Max(EDCModKind::CrewsPerNight, (float)B(S).BaseCrewsPerNight))); }
int32 FDCSimImpl::GearSlots(const S_t& S, const FDCModStack& M) { return FMath::Clamp(FMath::RoundToInt(M.Max(EDCModKind::GearSlots, (float)B(S).BaseGearSlots)), 0, (int32)EDCGearSlot::MAX); }
int32 FDCSimImpl::GearTier(const FDCModStack& M) { return FMath::RoundToInt(M.Max(EDCModKind::GearTier, 1.f)); }
int32 FDCSimImpl::RetakesPerNight(const FDCModStack& M) { return FMath::Max(0, FMath::RoundToInt(M.Sum(EDCModKind::Retakes))); }

int32 FDCSimImpl::BoardSize(const S_t& S, const FDCModStack& M)
{
	// Rooms set (max), perks add.
	float RoomV = (float)B(S).BaseJobBoard;
	float PerkV = 0.f;
	for (const FDCActiveMod& A : M.Mods)
	{
		if (A.Mod.Kind != EDCModKind::JobBoardSize) { continue; }
		if (A.bFromPerk) { PerkV += A.Mod.Value; } else { RoomV = FMath::Max(RoomV, A.Mod.Value); }
	}
	int32 Size = FMath::RoundToInt(RoomV + PerkV);
	if (S.Night <= 1) { Size = FMath::Max(Size, B(S).StartingBoard); } // night 1 shows startingBoard jobs
	return FMath::Clamp(Size, 1, 12);
}

int32 FDCSimImpl::GearPrice(const FDCGearDef& G, const FDCModStack& M)
{
	float Discount = 0.f; // additive across sources, capped at 50%
	for (const FDCActiveMod& A : M.Mods)
	{
		if (A.Mod.Kind != EDCModKind::GearDiscount) { continue; }
		if (A.Mod.bHasType && (!G.bHasAffinity || G.Affinity != A.Mod.Type)) { continue; }
		Discount += A.Mod.Value;
	}
	return RoundTo(G.Item.Price * (1.f - FMath::Clamp(Discount, 0.f, 0.5f)), 10);
}

float FDCSimImpl::PayoutMult(const FDCJob& J, const FDCModStack& M)
{
	float Mult = 1.f;
	for (const FDCActiveMod& A : M.Mods)
	{
		if (A.Mod.Kind != EDCModKind::PayoutMult) { continue; }
		const FString& C = A.Mod.Condition;
		bool bApplies = C.IsEmpty();
		if (C.Equals(TEXT("TierAtLeast3"), ESearchCase::IgnoreCase)) { bApplies = J.Tier >= 3; }
		else if (C.Equals(TEXT("HackerJob"), ESearchCase::IgnoreCase)) { bApplies = J.RequiredRoles.Contains(EDCCriminalType::Hacker); }
		else if (!C.IsEmpty()) { UE_LOG(LogDC, Warning, TEXT("[Sim] PayoutMult: unknown condition '%s'"), *C); }
		if (bApplies) { Mult *= A.Mod.Value; }
	}
	return FMath::Max(0.f, Mult);
}

int32 FDCSimImpl::UnlockedTier(const S_t& S)
{
	const TArray<int32>& T = B(S).TierUnlockHeists;
	int32 Tier = 1;
	for (int32 i = 0; i < T.Num(); ++i) { if (S.HeistsCompleted >= T[i]) { Tier = i + 1; } }
	return FMath::Max(1, Tier);
}

bool FDCSimImpl::IsFinalCutUnlocked(const S_t& S, const FDCModStack& M)
{
	return M.Has(EDCModKind::UnlockFinalCut) && S.HeistsCompleted >= B(S).FinalCutRequiredHeists;
}

int32 FDCSimImpl::Upkeep(const S_t& S, const FDCModStack& M)
{
	const FDCBalance& Bal = B(S);
	float U = (float)Bal.UpkeepPerCriminal * S.Roster.Num() + (float)Bal.UpkeepPerHeistBlock * (S.HeistsCompleted / Bal.UpkeepHeistBlock);
	for (const FDCRoom& R : S.Rooms)
	{
		if (R.State == EDCRoomState::Built) { U += D(S).GetRoom(R.Type).UpkeepPerNight; }
	}
	return FMath::Max(0, FMath::RoundToInt(U * M.Product(EDCModKind::UpkeepMult)));
}

// ═════════════════════════════════════════════════════════════════════════════
// Stats
// ═════════════════════════════════════════════════════════════════════════════

FDCStatBlock FDCSimImpl::EffStats(const S_t& S, const FDCModStack& M, const FDCCriminal& C)
{
	FDCStatBlock Out = C.BaseStats;
	const int32 Slots = GearSlots(S, M);
	for (const TPair<EDCGearSlot, FName>& E : C.Equipped)
	{
		if ((int32)E.Key >= Slots || E.Value.IsNone()) { continue; }
		if (const FDCGearDef* G = D(S).FindGear(E.Value)) { Out += G->Item.StatBonus; }
	}
	for (const FDCActiveMod& A : M.Mods)
	{
		if (A.Mod.Kind != EDCModKind::StatBonus || !A.Mod.bHasStat) { continue; }
		if (A.Mod.bHasType && A.Mod.Type != C.Type) { continue; }
		Out.GetMutable(A.Mod.Stat) += FMath::RoundToInt(A.Mod.Value);
	}
	for (int32 i = 0; i < (int32)EDCStat::MAX; ++i) { Out.Set((EDCStat)i, FMath::Max(1, Out.Get((EDCStat)i))); }
	return Out;
}

int32 FDCSimImpl::EffLuck(const S_t& S, const FDCModStack& M, const FDCCriminal& C)
{
	int32 L = C.Luck;
	const int32 Slots = GearSlots(S, M);
	for (const TPair<EDCGearSlot, FName>& E : C.Equipped)
	{
		if ((int32)E.Key >= Slots) { continue; }
		if (const FDCGearDef* G = D(S).FindGear(E.Value)) { L += G->Item.LuckBonus; }
	}
	for (const FDCActiveMod& A : M.Mods)
	{
		if (A.Mod.Kind == EDCModKind::LuckBonus && (!A.Mod.bHasType || A.Mod.Type == C.Type)) { L += FMath::RoundToInt(A.Mod.Value); }
	}
	return FMath::Clamp(L, 0, 10);
}

// ═════════════════════════════════════════════════════════════════════════════
// Generation
// ═════════════════════════════════════════════════════════════════════════════

static const FString& PickStr(const TArray<FString>& A, FRandomStream& R)
{
	return A[R.RandRange(0, A.Num() - 1)];
}

FDCCriminal FDCSimImpl::MakeCriminal(S_t& S, EDCCriminalType Type, int32 Tier, int32 BonusLevels)
{
	const FDCSimData& Dt = D(S);
	const FDCTypeDef& T = Dt.GetType(Type);
	FRandomStream& R = S.Rng;

	FDCCriminal C;
	C.Id = S.NextId++;
	C.Type = Type;

	// Unique-ish names.
	for (int32 Try = 0; Try < 12; ++Try)
	{
		C.FirstName = PickStr(Dt.Names.First, R);
		C.LastName = PickStr(Dt.Names.Last, R);
		const TArray<FString>* TypeNicks = Dt.Names.TypeNicknames.Find(Type);
		const bool bTypeNick = TypeNicks && TypeNicks->Num() > 0 && R.FRand() < 0.5f;
		C.Nickname = bTypeNick ? PickStr(*TypeNicks, R) : PickStr(Dt.Names.Nicknames, R);
		auto Clash = [&C](const TArray<FDCCriminal>& L)
		{
			return L.ContainsByPredicate([&C](const FDCCriminal& X) { return X.FirstName == C.FirstName || X.Nickname == C.Nickname; });
		};
		if (!Clash(S.Roster) && !Clash(S.Recruits) && !Clash(S.Fallen)) { break; }
	}

	const int32 StatBonus = FMath::Max(0, Tier - 1) * B(S).RecruitStatBonusPerTier;
	for (int32 i = 0; i < (int32)EDCStat::MAX; ++i)
	{
		const EDCStat St = (EDCStat)i;
		const FIntPoint Rg = St == T.Primary ? T.PrimaryRange : St == T.Secondary ? T.SecondaryRange : T.OtherRange;
		C.BaseStats.Set(St, FMath::Clamp(R.RandRange(Rg.X, Rg.Y) + StatBonus, 1, 100));
	}
	C.Luck = FMath::Clamp(R.RandRange(T.LuckRange.X, T.LuckRange.Y), 0, 10);
	C.Loyalty = FMath::Clamp(R.RandRange(T.LoyaltyRange.X, T.LoyaltyRange.Y), B(S).LoyaltyMin, B(S).LoyaltyMax);
	C.Level = 1;
	C.XP = 0;
	for (int32 i = 0; i < BonusLevels && C.Level < B(S).MaxLevel; ++i) { ApplyLevelUp(S, C); }
	C.XPToNext = DCSimFormula::XPToNext(C.Level, B(S).LevelCurveBase, B(S).LevelCurveExp);

	const float Cost = T.HireCostBase * FMath::Pow(B(S).RecruitCostTierMult, (float)FMath::Max(0, Tier - 1));
	C.HireCost = FMath::Max(100, RoundTo(Cost, 100));
	return C;
}

bool FDCSimImpl::IsDoable(const FDCJobDef& Def, const TArray<EDCCriminalType>& Pool)
{
	if (Pool.Num() < Def.MinCrew) { return false; }
	TArray<EDCCriminalType> Left = Pool;
	for (EDCCriminalType Role : Def.RequiredRoles)
	{
		if (Left.RemoveSingle(Role) == 0) { return false; }
	}
	return true;
}

TArray<const FDCJobDef*> FDCSimImpl::EligibleTemplates(const S_t& S, const FDCModStack& M)
{
	const int32 Tier = UnlockedTier(S);
	const bool bHeavy = M.Has(EDCModKind::UnlockHeavyJobs);
	TArray<const FDCJobDef*> Out;
	for (const FDCJobDef& J : D(S).Jobs)
	{
		if (!J.bFinalCut && J.Tier <= Tier && (!J.bHeavy || bHeavy)) { Out.Add(&J); }
	}
	return Out;
}

void FDCSimImpl::GenerateStartingRoster(S_t& S)
{
	const FDCBalance& Bal = B(S);
	const int32 N = FMath::Max(1, Bal.StartingRoster);
	const int32 NumTypes = (int32)EDCCriminalType::MAX;

	// Tier-1 templates the first board can draw from.
	TArray<const FDCJobDef*> T1;
	for (const FDCJobDef& J : D(S).Jobs) { if (!J.bFinalCut && !J.bHeavy && J.Tier <= 1) { T1.Add(&J); } }
	const int32 NeedDoable = FMath::Min(2, T1.Num());

	TArray<EDCCriminalType> Types;
	for (int32 Attempt = 0; Attempt < 200; ++Attempt)
	{
		TArray<int32> Order;
		for (int32 i = 0; i < NumTypes; ++i) { Order.Add(i); }
		for (int32 i = NumTypes - 1; i > 0; --i) { Order.Swap(i, S.Rng.RandRange(0, i)); }
		Types.Reset();
		const int32 Distinct = FMath::Min(N, 3);
		for (int32 i = 0; i < Distinct; ++i) { Types.Add((EDCCriminalType)Order[i]); }
		while (Types.Num() < N) { Types.Add((EDCCriminalType)S.Rng.RandRange(0, NumTypes - 1)); }

		int32 Doable = 0;
		for (const FDCJobDef* J : T1) { if (IsDoable(*J, Types)) { ++Doable; } }
		if (Doable >= NeedDoable) { break; }
	}
	// Fallback (pathological data): make sure the first tier-1 templates' roles are present.
	{
		int32 Doable = 0;
		for (const FDCJobDef* J : T1) { if (IsDoable(*J, Types)) { ++Doable; } }
		if (Doable < NeedDoable)
		{
			Types.Reset();
			for (int32 k = 0; k < NeedDoable; ++k) { for (EDCCriminalType R : T1[k]->RequiredRoles) { Types.Add(R); } }
			for (int32 i = 0; Types.Num() < N; ++i) { Types.AddUnique((EDCCriminalType)(i % NumTypes)); }
		}
	}
	for (EDCCriminalType T : Types) { S.Roster.Add(MakeCriminal(S, T, 1, 0)); }
}

void FDCSimImpl::GenerateRecruits(S_t& S)
{
	const FDCModStack M = BuildMods(S);
	const int32 Tier = UnlockedTier(S);
	const int32 Bonus = FMath::RoundToInt(M.Sum(EDCModKind::RecruitLevelBonus));
	S.Recruits.Reset();
	// Bias toward types the roster lacks so the board stays useful.
	TArray<EDCCriminalType> Missing;
	for (int32 i = 0; i < (int32)EDCCriminalType::MAX; ++i)
	{
		const EDCCriminalType T = (EDCCriminalType)i;
		if (!S.Roster.ContainsByPredicate([T](const FDCCriminal& C) { return C.Type == T; })) { Missing.Add(T); }
	}
	for (int32 i = 0; i < B(S).RecruitBoardSize; ++i)
	{
		EDCCriminalType T = (EDCCriminalType)S.Rng.RandRange(0, (int32)EDCCriminalType::MAX - 1);
		if (i == 0 && Missing.Num() > 0) { T = Missing[S.Rng.RandRange(0, Missing.Num() - 1)]; }
		S.Recruits.Add(MakeCriminal(S, T, Tier, Bonus));
	}
}

FDCJob FDCSimImpl::MakeJob(S_t& S, const FDCJobDef& Def, int32 BoardTier)
{
	const FDCBalance& Bal = B(S);
	FDCJob J;
	J.Id = S.NextId++;
	J.TemplateId = Def.Id;
	J.DisplayName = Def.DisplayName;
	J.Flavour = Def.Flavour.Num() ? PickStr(Def.Flavour, S.Rng) : FString();
	J.Tier = FMath::Max(Def.Tier, BoardTier);
	J.RequiredRoles = Def.RequiredRoles;
	J.BonusRoles = Def.BonusRoles;
	J.MinCrew = Def.MinCrew;
	J.MaxCrew = Def.MaxCrew;
	J.StatWeights = Def.StatWeights;
	J.Difficulty = DCSimFormula::Difficulty(Def.BaseDifficulty, S.HeistsCompleted, J.Tier, Bal.DifficultyPerHeist, Bal.DifficultyPerTier);
	const int32 Base = S.Rng.RandRange(Def.PayoutRange.X, Def.PayoutRange.Y);
	J.Payout = RoundTo(Base * FMath::Pow(Bal.TierPayoutMult, (float)(J.Tier - 1)), 100);
	J.XPReward = FMath::RoundToInt(Def.XPReward * (1.f + 0.5f * (J.Tier - Def.Tier)));
	J.ArrestRisk = Def.ArrestRisk; // per-5-heists increment is added in the resolver
	J.bHeavy = Def.bHeavy;
	J.bFinalCut = Def.bFinalCut;
	J.bCommitted = false;
	S.JobRollSeeds.Add(J.Id, (int32)(S.Rng.GetUnsignedInt() & 0x7fffffff));
	return J;
}

void FDCSimImpl::GenerateBoard(S_t& S)
{
	const FDCModStack M = BuildMods(S);
	const bool bHadFinal = S.Jobs.ContainsByPredicate([](const FDCJob& J) { return J.bFinalCut; });
	S.Jobs.Reset();
	S.JobRollSeeds.Reset();

	const TArray<const FDCJobDef*> Elig = EligibleTemplates(S, M);
	const int32 Size = BoardSize(S, M);
	const int32 Tier = UnlockedTier(S);

	TArray<EDCCriminalType> Pool;
	for (const FDCCriminal& C : S.Roster) { Pool.Add(C.Type); }

	TArray<const FDCJobDef*> Picks;
	if (Elig.Num() > 0)
	{
		TArray<const FDCJobDef*> Shuffled = Elig;
		for (int32 i = Shuffled.Num() - 1; i > 0; --i) { Shuffled.Swap(i, S.Rng.RandRange(0, i)); }
		for (int32 i = 0; i < Size; ++i)
		{
			Picks.Add(i < Shuffled.Num() ? Shuffled[i] : Elig[S.Rng.RandRange(0, Elig.Num() - 1)]);
		}

		// Playability guarantee: >= 2 jobs doable with the current roster (when possible).
		TArray<const FDCJobDef*> DoableElig;
		for (const FDCJobDef* J : Elig) { if (IsDoable(*J, Pool)) { DoableElig.Add(J); } }
		const int32 Need = FMath::Min3(2, DoableElig.Num(), Picks.Num());
		auto CountDoable = [&]() { int32 N = 0; for (const FDCJobDef* J : Picks) { if (IsDoable(*J, Pool)) { ++N; } } return N; };
		for (int32 Guard = 0; CountDoable() < Need && Guard < 16; ++Guard)
		{
			const int32 Slot = Picks.IndexOfByPredicate([&Pool](const FDCJobDef* J) { return !IsDoable(*J, Pool); });
			if (Slot == INDEX_NONE) { break; }
			TArray<const FDCJobDef*> Fresh = DoableElig.FilterByPredicate([&Picks](const FDCJobDef* J) { return !Picks.Contains(J); });
			const TArray<const FDCJobDef*>& From = Fresh.Num() ? Fresh : DoableElig;
			Picks[Slot] = From[S.Rng.RandRange(0, From.Num() - 1)];
		}
	}
	for (const FDCJobDef* Def : Picks)
	{
		S.Jobs.Add(MakeJob(S, *Def, S.Rng.RandRange(Def->Tier, FMath::Max(Def->Tier, Tier))));
	}
	if (IsFinalCutUnlocked(S, M))
	{
		for (const FDCJobDef& Def : D(S).Jobs)
		{
			if (!Def.bFinalCut) { continue; }
			S.Jobs.Add(MakeJob(S, Def, Def.Tier));
			if (!bHadFinal) { S.LastReport.Events.Add(Fill(D(S).Strings.Event(TEXT("finalCutUnlocked")), Def.DisplayName, Def.DisplayName)); }
			break;
		}
	}
}

void FDCSimImpl::TopUpBoard(S_t& S)
{
	const FDCModStack M = BuildMods(S);
	const TArray<const FDCJobDef*> Elig = EligibleTemplates(S, M);
	const int32 Size = BoardSize(S, M);
	const int32 Tier = UnlockedTier(S);
	int32 Regular = S.Jobs.FilterByPredicate([](const FDCJob& J) { return !J.bFinalCut; }).Num();
	while (Regular < Size && Elig.Num() > 0)
	{
		TArray<const FDCJobDef*> Fresh = Elig.FilterByPredicate([&S](const FDCJobDef* D)
		{
			return !S.Jobs.ContainsByPredicate([D](const FDCJob& J) { return J.TemplateId == D->Id; });
		});
		const TArray<const FDCJobDef*>& From = Fresh.Num() ? Fresh : Elig;
		const FDCJobDef* Def = From[S.Rng.RandRange(0, From.Num() - 1)];
		// Keep the Final Cut last on the board.
		const int32 FinalIdx = S.Jobs.IndexOfByPredicate([](const FDCJob& J) { return J.bFinalCut; });
		const FDCJob NewJob = MakeJob(S, *Def, S.Rng.RandRange(Def->Tier, FMath::Max(Def->Tier, Tier)));
		if (FinalIdx == INDEX_NONE) { S.Jobs.Add(NewJob); } else { S.Jobs.Insert(NewJob, FinalIdx); }
		++Regular;
	}
	if (IsFinalCutUnlocked(S, M) && !S.Jobs.ContainsByPredicate([](const FDCJob& J) { return J.bFinalCut; }))
	{
		for (const FDCJobDef& Def : D(S).Jobs)
		{
			if (!Def.bFinalCut) { continue; }
			S.Jobs.Add(MakeJob(S, Def, Def.Tier));
			S.LastReport.Events.Add(Fill(D(S).Strings.Event(TEXT("finalCutUnlocked")), Def.DisplayName, Def.DisplayName));
			break;
		}
	}
}

// ═════════════════════════════════════════════════════════════════════════════
// Resolver (PLAN §2.7)
// ═════════════════════════════════════════════════════════════════════════════

static FString Pct(float V) { return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V * 100.f)); }
static FString SignedPct(float V) { return FString::Printf(TEXT("%s%d%%"), V >= 0.f ? TEXT("+") : TEXT(""), FMath::RoundToInt(V * 100.f)); }

FDCSuccessBreakdown FDCSimImpl::Breakdown(const S_t& S, const FDCCrewPlan& Plan, bool bCheckAvailability)
{
	FDCSuccessBreakdown Bd;
	if (!S.bActive || !S.Data.IsValid()) { Bd.InvalidReason = TEXT("No run in progress."); return Bd; }
	const FDCJob* Job = S.FindJob(Plan.JobId);
	if (!Job) { Bd.InvalidReason = TEXT("Pick a production first."); return Bd; }

	const FDCBalance& Bal = B(S);
	const FDCModStack M = BuildMods(S);
	const float Cut = FMath::Clamp(Plan.Cut, Bal.CutMin, Bal.CutMax);
	TArray<FString> Errors; // first one wins

	// ── Crew ──
	TArray<const FDCCriminal*> Crew;
	for (int32 Id : Plan.CriminalIds)
	{
		const FDCCriminal* C = S.FindCriminal(Id);
		if (!C) { Errors.Add(TEXT("Unknown crew member.")); continue; }
		if (Crew.Contains(C)) { Errors.Add(FString::Printf(TEXT("%s is cast twice."), *C->GetShortName())); continue; }
		Crew.Add(C);
	}

	if (bCheckAvailability)
	{
		if (S.Phase != EDCGamePhase::Planning) { Errors.Add(TEXT("You can only cast during planning.")); }
		const bool bOvertime = M.Has(EDCModKind::Overtime);
		for (const FDCCriminal* C : Crew)
		{
			if (C->Status == EDCCriminalStatus::Injured) { Errors.Add(FString::Printf(TEXT("%s is injured (%d night%s)."), *C->GetShortName(), C->InjuredNights, C->InjuredNights == 1 ? TEXT("") : TEXT("s"))); continue; }
			int32 OtherPlans = 0;
			for (const FDCCrewPlan& P : S.Plans) { if (P.JobId != Plan.JobId && P.CriminalIds.Contains(C->Id)) { ++OtherPlans; } }
			if (OtherPlans >= (bOvertime ? 2 : 1)) { Errors.Add(FString::Printf(TEXT("%s is already cast tonight."), *C->GetShortName())); }
		}
	}

	// ── Roles ──
	TArray<const FDCCriminal*> Unused = Crew;
	for (EDCCriminalType Role : Job->RequiredRoles)
	{
		const int32 Idx = Unused.IndexOfByPredicate([Role](const FDCCriminal* C) { return C->Type == Role; });
		if (Idx == INDEX_NONE) { Bd.MissingRoles.Add(Role); } else { Unused.RemoveAt(Idx); }
	}
	if (Bd.MissingRoles.Num())
	{
		TArray<FString> Names;
		for (EDCCriminalType R : Bd.MissingRoles) { Names.Add(DC::TypeName(R)); }
		Errors.Add(FString::Printf(TEXT("Needs a %s."), *FString::Join(Names, TEXT(", "))));
	}
	if (Crew.Num() < Job->MinCrew) { Errors.Add(FString::Printf(TEXT("Crew too small (min %d)."), Job->MinCrew)); }
	if (Crew.Num() > Job->MaxCrew) { Errors.Add(FString::Printf(TEXT("Crew too large (max %d)."), Job->MaxCrew)); }
	if (bCheckAvailability)
	{
		const bool bAlready = S.Plans.ContainsByPredicate([&Plan](const FDCCrewPlan& P) { return P.JobId == Plan.JobId; });
		const int32 Max = CrewsPerNight(S, M);
		if (!bAlready && S.Plans.Num() >= Max)
		{
			Errors.Add(Max <= 1 ? TEXT("Only one crew per night. Build the Garage to run more.") : FString::Printf(TEXT("Only %d crews per night."), Max));
		}
	}

	int32 RoleFitCount = 0;
	for (const FDCCriminal* C : Unused) { if (Job->BonusRoles.Contains(C->Type)) { ++RoleFitCount; } }
	Bd.RoleFitBonus = FMath::Min(Bal.RoleFitPer * RoleFitCount, Bal.RoleFitCap);

	// ── Crew stats ──
	TArray<FDCStatBlock> Eff;
	int32 LuckSum = 0;
	float GearArrestMult = 1.f;
	const int32 Slots = GearSlots(S, M);
	for (const FDCCriminal* C : Crew)
	{
		Eff.Add(EffStats(S, M, *C));
		LuckSum += EffLuck(S, M, *C);
		for (const TPair<EDCGearSlot, FName>& E : C->Equipped)
		{
			if ((int32)E.Key >= Slots) { continue; }
			if (const FDCGearDef* G = D(S).FindGear(E.Value)) { GearArrestMult *= G->Item.ArrestRiskMultiplier; }
		}
	}
	for (int32 i = 0; i < (int32)EDCStat::MAX; ++i)
	{
		const EDCStat St = (EDCStat)i;
		float Best = 0.f, Sum = 0.f;
		for (const FDCStatBlock& E : Eff) { const float V = (float)E.Get(St); Best = FMath::Max(Best, V); Sum += V; }
		Bd.CrewStats.Add(St, Eff.Num() ? Best + Bal.SupportWeight * (Sum - Best) : 0.f);
	}
	for (const TPair<EDCStat, float>& W : Job->StatWeights) { Bd.CrewScore += W.Value * Bd.CrewStats.FindRef(W.Key); }

	Bd.Difficulty = FMath::Max(1.f, Job->Difficulty);
	Bd.Margin = (Bd.CrewScore - Bd.Difficulty) / Bd.Difficulty;
	Bd.SkillChance = DCSimFormula::SkillChance(Bd.Margin, Bal.SkillBase, Bal.SkillSlope, Bal.SkillMin, Bal.SkillMax);

	// ── Willingness ──
	const float FairBonus = M.Sum(EDCModKind::FairCutBonus);
	const float WillFloor = M.Max(EDCModKind::MinWillingness, 0.f);
	float WillSum = 0.f;
	float MinFair = TNumericLimits<float>::Max();
	TArray<FDCBreakdownLine> WillLines;
	for (const FDCCriminal* C : Crew)
	{
		const float Fair = DCSimFormula::FairCut(C->Loyalty, Bal.FairCutBase, Bal.FairCutLoyaltyDiv, FairBonus);
		const float Will = DCSimFormula::Willingness(Cut, Fair, Bal.WillSlope, Bal.WillMin, WillFloor);
		Bd.FairCut.Add(C->Id, Fair);
		Bd.Willingness.Add(C->Id, Will);
		MinFair = FMath::Min(MinFair, Fair);
		WillSum += Will;
		const bool bRefuses = Will < Bal.RefusalThreshold;
		if (bRefuses) { Bd.RefusingCriminalIds.Add(C->Id); }
		if (Cut > Fair + KINDA_SMALL_NUMBER)
		{
			FDCBreakdownLine L;
			L.Label = FString::Printf(TEXT("Willingness: %s"), *C->GetShortName());
			L.Detail = FString::Printf(TEXT("Fair cut %s, your cut %s -> %s willing%s"), *Pct(Fair), *Pct(Cut), *Pct(Will), bRefuses ? TEXT(" (REFUSES)") : TEXT(""));
			L.Value = Will;
			L.bIsMultiplier = true;
			L.bIsWarning = bRefuses || Will < 0.75f;
			WillLines.Add(L);
		}
	}
	if (Bd.RefusingCriminalIds.Num())
	{
		TArray<FString> Names;
		for (int32 Id : Bd.RefusingCriminalIds) { if (const FDCCriminal* C = S.FindCriminal(Id)) { Names.Add(C->GetShortName()); } }
		Errors.Add(FString::Printf(TEXT("%s refuse%s to work for a %s cut."), *FString::Join(Names, TEXT(" & ")), Names.Num() == 1 ? TEXT("s") : TEXT(""), *Pct(Cut)));
	}
	Bd.CrewWill = Crew.Num() ? WillSum / Crew.Num() : 1.f;
	Bd.WillFactor = Bal.WillFactorBase + Bal.WillFactorScale * Bd.CrewWill;
	Bd.GenerosityBonus = (Crew.Num() && Cut < MinFair - KINDA_SMALL_NUMBER) ? Bal.GenerosityBonus : 0.f;

	// ── Room / perk chance modifiers ──
	TArray<FDCBreakdownLine> ModLines;
	for (const FDCActiveMod& A : M.Mods)
	{
		if (A.Mod.Kind != EDCModKind::ChanceBonus) { continue; }
		const FString& Cnd = A.Mod.Condition;
		float V = 0.f;
		FString Why;
		if (Cnd.IsEmpty()) { V = A.Mod.Value; Why = TEXT("always"); }
		else if (Cnd.Equals(TEXT("HasWheelman"), ESearchCase::IgnoreCase))
		{
			if (Crew.ContainsByPredicate([](const FDCCriminal* C) { return C->Type == EDCCriminalType::Wheelman; })) { V = A.Mod.Value; Why = TEXT("Wheelman on the crew"); }
		}
		else if (Cnd.Equals(TEXT("AllDifferentTypes"), ESearchCase::IgnoreCase))
		{
			TSet<EDCCriminalType> Types;
			for (const FDCCriminal* C : Crew) { Types.Add(C->Type); }
			if (Crew.Num() >= 2 && Types.Num() == Crew.Num()) { V = A.Mod.Value; Why = TEXT("every member a different type"); }
		}
		else if (Cnd.Equals(TEXT("PerExtraCrew"), ESearchCase::IgnoreCase))
		{
			const int32 Extra = FMath::Max(0, Crew.Num() - Job->MinCrew);
			if (Extra > 0) { V = A.Mod.Value * Extra; Why = FString::Printf(TEXT("%d beyond the minimum"), Extra); }
		}
		else if (Cnd.Equals(TEXT("TierAtLeast3"), ESearchCase::IgnoreCase))
		{
			if (Job->Tier >= 3) { V = A.Mod.Value; Why = TEXT("tier 3+ production"); }
		}
		else { UE_LOG(LogDC, Warning, TEXT("[Sim] ChanceBonus: unknown condition '%s'"), *Cnd); }
		if (V != 0.f)
		{
			Bd.ModifierBonus += V;
			ModLines.Add({ A.Source, FString::Printf(TEXT("%s (%s)"), *SignedPct(V), *Why), V, false, V < 0.f });
		}
	}

	const float Raw = Bd.SkillChance + Bd.RoleFitBonus + Bd.GenerosityBonus + Bd.ModifierBonus;
	Bd.SuccessChance = FMath::Clamp(Raw * Bd.WillFactor, Bal.SuccessMin, Bal.SuccessMax);
	Bd.CleanThreshold = Bd.SuccessChance * Bal.CleanBand;
	Bd.LuckyBreakChance = FMath::Clamp(Bal.LuckyBreakPerLuck * LuckSum, 0.f, 1.f);
	{
		const float RiskBase = Job->ArrestRisk * FMath::Max(0.f, 1.f - Bd.Margin) + Bal.ArrestRiskPer5Heists * (S.HeistsCompleted / 5);
		Bd.ArrestChanceOnFail = FMath::Clamp(RiskBase * M.Product(EDCModKind::ArrestMult) * GearArrestMult, 0.f, 0.95f);
	}
	Bd.Payout = RoundTo(Job->Payout * PayoutMult(*Job, M), 100);
	Bd.DirectorTake = FMath::RoundToInt(Bd.Payout * Cut);

	// ── Lines, in order ──
	Bd.Lines.Add({ TEXT("Crew vs difficulty"), FString::Printf(TEXT("Crew %.0f vs Difficulty %.0f (%s margin)"), Bd.CrewScore, Bd.Difficulty, *SignedPct(Bd.Margin)), Bd.Margin, false, Bd.Margin < 0.f });
	Bd.Lines.Add({ TEXT("Skill chance"), FString::Printf(TEXT("%.0f%% + %.2f x margin, clamped %s-%s"), Bal.SkillBase * 100.f, Bal.SkillSlope, *Pct(Bal.SkillMin), *Pct(Bal.SkillMax)), Bd.SkillChance, false, false });
	Bd.Lines.Add({ TEXT("Role fit"), RoleFitCount ? FString::Printf(TEXT("%d extra specialist%s (+%s each, cap %s)"), RoleFitCount, RoleFitCount == 1 ? TEXT("") : TEXT("s"), *Pct(Bal.RoleFitPer), *Pct(Bal.RoleFitCap)) : FString(TEXT("No extra specialists")), Bd.RoleFitBonus, false, false });
	Bd.Lines.Add({ TEXT("Generosity"), Bd.GenerosityBonus > 0.f ? FString::Printf(TEXT("Cut %s is below everyone's fair cut"), *Pct(Cut)) : FString::Printf(TEXT("Cut %s is not below every fair cut"), *Pct(Cut)), Bd.GenerosityBonus, false, false });
	Bd.Lines.Append(ModLines);
	Bd.Lines.Append(WillLines);
	Bd.Lines.Add({ TEXT("Crew willingness"), FString::Printf(TEXT("%.2f + %.2f x mean willingness %s"), Bal.WillFactorBase, Bal.WillFactorScale, *Pct(Bd.CrewWill)), Bd.WillFactor, true, Bd.WillFactor < 0.9f });
	Bd.Lines.Add({ TEXT("Success chance"), FString::Printf(TEXT("(%s) x %.2f, clamped %s-%s"), *Pct(Raw), Bd.WillFactor, *Pct(Bal.SuccessMin), *Pct(Bal.SuccessMax)), Bd.SuccessChance, false, Bd.SuccessChance < 0.4f });
	Bd.Lines.Add({ TEXT("Lucky break"), FString::Printf(TEXT("%s x crew luck %d: re-roll on failure"), *Pct(Bal.LuckyBreakPerLuck), LuckSum), Bd.LuckyBreakChance, false, false });
	if (M.Has(EDCModKind::Intel))
	{
		Bd.Lines.Add({ TEXT("Intel: arrest risk"), FString::Printf(TEXT("Per member on failure %s (x%.1f if busted). Clean below %s."), *Pct(Bd.ArrestChanceOnFail), Bal.BustedArrestMult, *Pct(Bd.CleanThreshold)), Bd.ArrestChanceOnFail, false, Bd.ArrestChanceOnFail > 0.25f });
	}
	if (M.Has(EDCModKind::Dailies))
	{
		if (const int32* RollSeed = S.JobRollSeeds.Find(Job->Id))
		{
			FRandomStream Peek(*RollSeed);
			const float Roll = Peek.FRand();
			const EDCHeistOutcome O = OutcomeForRoll(Roll, Bd.SuccessChance, Bd.CleanThreshold, Bal.BustedBand);
			Bd.Lines.Add({ TEXT("Dailies"), FString::Printf(TEXT("The dice will land on %.2f -> %s"), Roll, DC::OutcomeStamp(O)), Roll, false, O == EDCHeistOutcome::Failed || O == EDCHeistOutcome::Busted });
		}
	}

	if (Errors.Num()) { Bd.InvalidReason = Errors[0]; Bd.bValid = false; }
	else { Bd.bValid = true; }
	return Bd;
}

EDCHeistOutcome FDCSimImpl::OutcomeForRoll(float Roll, float Success, float Clean, float BustedBand)
{
	if (Roll < Clean) { return EDCHeistOutcome::Clean; }
	if (Roll < Success) { return EDCHeistOutcome::Messy; }
	// Busted: a failed roll in the top BustedBand of the roll range.
	return Roll > 1.f - BustedBand ? EDCHeistOutcome::Busted : EDCHeistOutcome::Failed;
}

void FDCSimImpl::ResolveHeist(S_t& S, const FDCCrewPlan& Plan, const FDCSuccessBreakdown& Bd, const FDCJob& Job, FRandomStream& R, FDCHeistResult& Out)
{
	const FDCBalance& Bal = B(S);
	const FDCModStack M = BuildMods(S);
	const float Cut = FMath::Clamp(Plan.Cut, Bal.CutMin, Bal.CutMax);

	Out.JobId = Job.Id;
	Out.JobName = Job.DisplayName;
	Out.Plan = Plan;
	Out.SuccessChance = Bd.SuccessChance;
	Out.CleanThreshold = Bd.CleanThreshold;
	Out.bFinalCut = Job.bFinalCut;

	float Roll = R.FRand();
	Out.FirstRoll = Roll;
	Out.bLuckyBreak = false;
	if (Roll >= Bd.SuccessChance && R.FRand() < Bd.LuckyBreakChance)
	{
		Out.bLuckyBreak = true;
		Roll = R.FRand();
	}
	Out.Roll = Roll;
	Out.Outcome = OutcomeForRoll(Roll, Bd.SuccessChance, Bd.CleanThreshold, Bal.BustedBand);
	const bool bSuccess = Out.Outcome == EDCHeistOutcome::Clean || Out.Outcome == EDCHeistOutcome::Messy;
	const bool bBusted = Out.Outcome == EDCHeistOutcome::Busted;

	Out.PayoutTotal = bSuccess ? Bd.Payout : 0;
	Out.DirectorTake = bSuccess ? Bd.DirectorTake : 0;
	if (bSuccess)
	{
		S.Cash += Out.DirectorTake;
		S.TotalTake += Out.DirectorTake;
		S.HeistsCompleted++;
		if (Out.DirectorTake > S.BestHeistTake) { S.BestHeistTake = Out.DirectorTake; S.BestHeistName = Job.DisplayName; }
		if (Job.bFinalCut) { S.bWonRun = true; }
	}

	const float InjuryMult = M.Product(EDCModKind::InjuryMult);
	float InjuryChance = 0.f;
	FIntPoint InjuryNights(1, 1);
	if (Out.Outcome == EDCHeistOutcome::Messy) { InjuryChance = Bal.MessyInjuryChance; InjuryNights = Bal.MessyInjuryNights; }
	else if (!bSuccess) { InjuryChance = Bal.FailedInjuryChance; InjuryNights = Bal.FailedInjuryNights; }
	InjuryChance = FMath::Clamp(InjuryChance * InjuryMult, 0.f, 1.f);
	const float ArrestChance = bSuccess ? 0.f : FMath::Clamp(Bd.ArrestChanceOnFail * (bBusted ? Bal.BustedArrestMult : 1.f), 0.f, 0.95f);
	const float XPMult = Out.Outcome == EDCHeistOutcome::Clean ? Bal.XPMultClean : Out.Outcome == EDCHeistOutcome::Messy ? Bal.XPMultMessy : bBusted ? Bal.XPMultBusted : Bal.XPMultFailed;
	const bool bPrevent = M.Has(EDCModKind::PreventFirstArrest);

	TArray<FString> Names;
	Out.Members.Reset();
	for (int32 Id : Plan.CriminalIds)
	{
		// Fixed draw order per member keeps runs reproducible.
		const float ArrestRoll = R.FRand();
		const float InjuryRoll = R.FRand();
		const int32 Nights = R.RandRange(InjuryNights.X, FMath::Max(InjuryNights.X, InjuryNights.Y));

		FDCCriminal* C = S.FindCriminalMutable(Id);
		if (!C) { continue; } // arrested earlier tonight (Overtime)
		FDCCrewMemberResult MR;
		MR.CriminalId = Id;
		MR.Name = C->GetShortName();
		Names.Add(MR.Name);

		if (ArrestRoll < ArrestChance)
		{
			if (bPrevent && !S.bFirstArrestPrevented) { S.bFirstArrestPrevented = true; MR.bArrestPrevented = true; }
			else { MR.bArrested = true; }
		}
		if (!MR.bArrested && InjuryRoll < InjuryChance) { MR.bInjured = true; MR.InjuryNights = FMath::Max(1, Nights); }

		const float Fair = Bd.FairCut.Contains(Id) ? Bd.FairCut[Id] : Bal.FairCutBase;
		int32 Loy = 0;
		if (Cut > Fair + KINDA_SMALL_NUMBER) { Loy += Bal.LoyaltyGreedy; }
		else if (bSuccess) { Loy += Bal.LoyaltySuccessFair; }
		if (MR.bInjured) { Loy += Bal.LoyaltyInjury; }
		C->Loyalty = FMath::Clamp(C->Loyalty + Loy, Bal.LoyaltyMin, Bal.LoyaltyMax);
		MR.LoyaltyDelta = Loy;

		C->HeistsPulled++;
		const int32 LevelBefore = C->Level;
		AddXP(S, *C, FMath::RoundToInt(Job.XPReward * XPMult), &MR);
		MR.NewLevel = C->Level;
		MR.bLevelledUp = C->Level > LevelBefore;

		if (MR.bArrested)
		{
			FDCCriminal Gone = *C;
			Gone.Equipped.Empty(); // gear is lost on arrest
			Gone.Status = EDCCriminalStatus::Arrested;
			Gone.InjuredNights = 0;
			S.Roster.RemoveAll([Id](const FDCCriminal& X) { return X.Id == Id; });
			S.Fallen.Add(Gone);
			S.ArrestedOnNight.Add(Id, S.Night);
		}
		else if (MR.bInjured)
		{
			C->Status = EDCCriminalStatus::Injured;
			C->InjuredNights = FMath::Max(C->InjuredNights, MR.InjuryNights);
		}
		else if (C->Status != EDCCriminalStatus::Injured)
		{
			C->Status = EDCCriminalStatus::Available;
		}
		Out.Members.Add(MR);
	}

	const FString Who = Names.Num() ? Names[R.RandRange(0, Names.Num() - 1)] : FString(TEXT("The crew"));
	const TArray<FString>* Lines = D(S).Strings.OutcomeLines.Find(Out.Outcome);
	FString Line = Lines && Lines->Num() ? PickLine(*Lines, R) : FString();
	if (Out.bLuckyBreak && bSuccess && D(S).Strings.LuckyBreak.Num())
	{
		Line = PickLine(D(S).Strings.LuckyBreak, R) + TEXT(" ") + Line;
	}
	Out.FlavourLine = Fill(Line, Who, Job.DisplayName, Job.Tier, Money(Out.DirectorTake));
}

// ═════════════════════════════════════════════════════════════════════════════
// Levelling (PLAN §2.8)
// ═════════════════════════════════════════════════════════════════════════════

void FDCSimImpl::ApplyLevelUp(S_t& S, FDCCriminal& C)
{
	const FDCBalance& Bal = B(S);
	const FDCTypeDef& T = D(S).GetType(C.Type);
	C.Level++;
	for (int32 i = 0; i < (int32)EDCStat::MAX; ++i)
	{
		const EDCStat St = (EDCStat)i;
		const int32 Gain = St == T.Primary ? Bal.LevelUpPrimary : St == T.Secondary ? Bal.LevelUpSecondary : Bal.LevelUpOther;
		C.BaseStats.Set(St, FMath::Clamp(C.BaseStats.Get(St) + Gain, 1, 100));
	}
	if (C.Level % Bal.LuckEveryNLevels == 0) { C.Luck = FMath::Min(10, C.Luck + 1); }
	C.XPToNext = DCSimFormula::XPToNext(C.Level, Bal.LevelCurveBase, Bal.LevelCurveExp);
}

void FDCSimImpl::AddXP(S_t& S, FDCCriminal& C, int32 XP, FDCCrewMemberResult* Out)
{
	const FDCBalance& Bal = B(S);
	if (Out) { Out->XPGained = XP; }
	if (C.Level >= Bal.MaxLevel) { C.XP = 0; return; }
	C.XP += FMath::Max(0, XP);
	C.XPToNext = DCSimFormula::XPToNext(C.Level, Bal.LevelCurveBase, Bal.LevelCurveExp);
	if (C.XP >= C.XPToNext)
	{
		C.XP = 0; // XP resets on level up
		ApplyLevelUp(S, C);
		S.OnCriminalLevelUp.Broadcast(C.Id, C.Level);
	}
	if (C.Level >= Bal.MaxLevel) { C.XP = 0; }
}

// ═════════════════════════════════════════════════════════════════════════════
// Progression
// ═════════════════════════════════════════════════════════════════════════════

void FDCSimImpl::CheckProgression(S_t& S, TArray<FString>& Events)
{
	const int32 Tier = UnlockedTier(S);
	while (S.LastUnlockedTier < Tier)
	{
		S.LastUnlockedTier++;
		Events.Add(Fill(D(S).Strings.TierUnlocked, FString(), FString(), S.LastUnlockedTier));
	}

	// Perk draft.
	const int32 Earned = S.HeistsCompleted / B(S).PerkEveryNHeists;
	if (S.PerkOffer.Num() == 0 && Earned > S.PerkOffersGiven)
	{
		TArray<const FDCPerkDef*> Pool;
		for (const FDCPerkDef& P : D(S).Perks)
		{
			if (!S.PerksTaken.ContainsByPredicate([&P](const FDCPerk& T) { return T.Id == P.Perk.Id; })) { Pool.Add(&P); }
		}
		if (Pool.Num() > 0)
		{
			for (int32 i = Pool.Num() - 1; i > 0; --i) { Pool.Swap(i, S.Rng.RandRange(0, i)); }
			for (int32 i = 0; i < FMath::Min(B(S).PerkOfferSize, Pool.Num()); ++i) { S.PerkOffer.Add(Pool[i]->Perk); }
			S.PerkOffersGiven++;
			S.LastReport.bPerkOffered = true;
			Events.Add(D(S).Strings.Event(TEXT("perkOffer")));
		}
		else
		{
			S.PerkOffersGiven = Earned; // nothing left to offer
		}
	}
}

bool FDCSimImpl::ComputeGameOver(const S_t& S, FString& OutReason)
{
	if (S.Cash < 0) { OutReason = D(S).Strings.GameOverBroke; return true; }
	if (S.Roster.Num() == 0)
	{
		int32 Cheapest = MAX_int32;
		for (const FDCCriminal& R : S.Recruits) { Cheapest = FMath::Min(Cheapest, R.HireCost); }
		if (S.Recruits.Num() == 0 || S.Cash < Cheapest) { OutReason = D(S).Strings.GameOverNoCrew; return true; }
	}
	return false;
}

void FDCSimImpl::RefreshStatuses(S_t& S)
{
	for (FDCCriminal& C : S.Roster)
	{
		if (C.Status == EDCCriminalStatus::Injured && C.InjuredNights > 0) { continue; }
		const bool bCast = S.Plans.ContainsByPredicate([&C](const FDCCrewPlan& P) { return P.CriminalIds.Contains(C.Id); });
		C.Status = bCast ? EDCCriminalStatus::OnJob : EDCCriminalStatus::Available;
	}
	for (FDCJob& J : S.Jobs)
	{
		J.bCommitted = S.Plans.ContainsByPredicate([&J](const FDCCrewPlan& P) { return P.JobId == J.Id; });
	}
}

FString FDCSimImpl::Money(int32 Amount)
{
	FString Digits = FString::FromInt(FMath::Abs(Amount));
	for (int32 i = Digits.Len() - 3; i > 0; i -= 3) { Digits.InsertAt(i, TEXT(',')); }
	return FString::Printf(TEXT("%s$%s"), Amount < 0 ? TEXT("-") : TEXT(""), *Digits);
}

FString FDCSimImpl::Fill(const FString& Tmpl, const FString& Name, const FString& Job, int32 Tier, const FString& Amount)
{
	FString Out = Tmpl;
	Out.ReplaceInline(TEXT("{name}"), *Name);
	Out.ReplaceInline(TEXT("{job}"), *Job);
	Out.ReplaceInline(TEXT("{tier}"), *FString::FromInt(Tier));
	Out.ReplaceInline(TEXT("{amount}"), *Amount);
	return Out;
}

FString FDCSimImpl::PickLine(const TArray<FString>& Lines, FRandomStream& R)
{
	return Lines.Num() ? Lines[R.RandRange(0, Lines.Num() - 1)] : FString();
}
