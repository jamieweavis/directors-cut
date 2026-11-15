// Director's Cut — simulation internals: effects, generation, success formula, mission resolution, timers (Agent A).
#include "Sim/DCSimImpl.h"
#include "DirectorsCut.h"
#include "Core/DCAssetPaths.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FName K_chance(TEXT("chance")), K_chanceNight(TEXT("chanceNight")), K_chanceDay(TEXT("chanceDay")), K_chanceShort(TEXT("chanceShort")),
		K_chanceLong(TEXT("chanceLong")), K_chanceSolo(TEXT("chanceSolo")), K_chanceThreat(TEXT("chanceThreat")), K_chanceWithType(TEXT("chanceWithType")),
		K_chanceTierMin(TEXT("chanceTierMin")), K_chanceIfType(TEXT("chanceIfType")), K_durationMult(TEXT("durationMult")), K_payoutMult(TEXT("payoutMult")),
		K_costMult(TEXT("costMult")), K_xpMult(TEXT("xpMult")), K_injuryChanceMult(TEXT("injuryChanceMult")), K_injuryTimeMult(TEXT("injuryTimeMult")),
		K_arrestMult(TEXT("arrestMult")), K_fairCutAdd(TEXT("fairCutAdd")), K_willMin(TEXT("willMin")), K_loyaltyIdlePerDay(TEXT("loyaltyIdlePerDay")),
		K_boardSize(TEXT("boardSize")), K_postIntervalMult(TEXT("postIntervalMult")), K_gearPriceMult(TEXT("gearPriceMult")),
		K_recruitLevelAdd(TEXT("recruitLevelAdd")), K_recruitQualityAdd(TEXT("recruitQualityAdd")), K_recruitBoardAdd(TEXT("recruitBoardAdd")), K_preventFirstArrest(TEXT("preventFirstArrest")),
		K_unlockFinal(TEXT("unlockFinal")), K_lootChanceAdd(TEXT("lootChanceAdd")), K_chanceStreak(TEXT("chanceStreak")), K_chanceRookie(TEXT("chanceRookie")),
		K_failRefund(TEXT("failRefund")), K_dawnInterest(TEXT("dawnInterest")), K_creditLine(TEXT("creditLine"));

	bool TypeMatches(const FString& Param, EDCCriminalType T) { return Param.Equals(DC::TypeName(T), ESearchCase::IgnoreCase); }
	int32 Round50(float V) { return FMath::RoundToInt(V / 50.f) * 50; }
	int32 RandIn(FRandomStream& R, FIntPoint P) { return R.RandRange(FMath::Min(P.X, P.Y), FMath::Max(P.X, P.Y)); }
	double RandIn(FRandomStream& R, FVector2D P) { return R.FRandRange(FMath::Min(P.X, P.Y), FMath::Max(P.X, P.Y)); }

	template <typename T>
	void Shuffle(FRandomStream& R, TArray<T>& A)
	{
		for (int32 i = A.Num() - 1; i > 0; --i) { A.Swap(i, R.RandRange(0, i)); }
	}

	/** Mission-affecting effect kinds (shown as modifier hits). */
	bool IsMissionKind(FName K)
	{
		return K == K_chance || K == K_chanceNight || K == K_chanceDay || K == K_chanceShort || K == K_chanceLong || K == K_chanceSolo
			|| K == K_chanceThreat || K == K_chanceWithType || K == K_chanceTierMin || K == K_chanceIfType || K == K_durationMult
			|| K == K_payoutMult || K == K_costMult || K == K_xpMult || K == K_injuryChanceMult || K == K_arrestMult || K == K_fairCutAdd;
	}
}

// ─── data / effects ─────────────────────────────────────────────────────────

const FDCBalance& FDCSimImpl::Bal(const S& Sim)
{
	static const FDCBalance Default;
	return Sim.Data.IsValid() ? Sim.Data->Balance : Default;
}

void FDCSimImpl::ForEachGlobalEffect(const S& Sim, TFunctionRef<void(const FDCEffect&, FName, FName, const FString&)> F)
{
	if (!Sim.Data.IsValid()) { return; }
	for (const FDCPerk& P : Sim.PerksTaken)
	{
		if (const FDCPerkDef* D = Sim.Data->Perks.Find(P.Id))
		{
			for (const FDCEffect& E : D->Effects) { F(E, P.Id, D->Perk.Icon, D->Perk.DisplayName); }
		}
	}
	for (const FDCRoom& R : Sim.Rooms)
	{
		if (R.Level < 1) { continue; }
		const FDCRoomDef* D = Sim.Data->FindRoom(R.Type);
		if (!D || !D->Levels.IsValidIndex(R.Level - 1)) { continue; }
		const FName Id(*FString::Printf(TEXT("room.%s"), *StaticEnum<EDCRoomType>()->GetNameStringByValue((int64)R.Type)));
		for (const FDCEffect& E : D->Levels[R.Level - 1]) { F(E, Id, D->Icon, D->DisplayName); }
	}
}

float FDCSimImpl::SumGlobal(const S& Sim, FName Kind)
{
	float V = 0.f;
	ForEachGlobalEffect(Sim, [&](const FDCEffect& E, FName, FName, const FString&) { if (E.Kind == Kind) { V += E.Value; } });
	return V;
}

float FDCSimImpl::ProdGlobal(const S& Sim, FName Kind)
{
	float V = 1.f;
	ForEachGlobalEffect(Sim, [&](const FDCEffect& E, FName, FName, const FString&) { if (E.Kind == Kind && E.Param.IsEmpty()) { V *= E.Value; } });
	return V;
}

bool FDCSimImpl::MinGlobal(const S& Sim, FName Kind, float& OutValue, float* OutThreshold)
{
	bool bFound = false;
	ForEachGlobalEffect(Sim, [&](const FDCEffect& E, FName, FName, const FString&)
	{
		if (E.Kind == Kind && (!bFound || E.Value < OutValue)) { OutValue = E.Value; if (OutThreshold) { *OutThreshold = E.Threshold; } bFound = true; }
	});
	return bFound;
}

bool FDCSimImpl::MaxGlobal(const S& Sim, FName Kind, float& OutValue)
{
	bool bFound = false;
	ForEachGlobalEffect(Sim, [&](const FDCEffect& E, FName, FName, const FString&)
	{
		if (E.Kind == Kind && (!bFound || E.Value > OutValue)) { OutValue = E.Value; bFound = true; }
	});
	return bFound;
}

float FDCSimImpl::TraitProd(const S& Sim, int32 CriminalId, FName Kind)
{
	float V = 1.f;
	if (!Sim.Data.IsValid()) { return V; }
	for (FName T : Sim.GetAllTraits(CriminalId))
	{
		if (const FDCTraitDef* D = Sim.Data->Traits.Find(T)) { for (const FDCEffect& E : D->Effects) { if (E.Kind == Kind) { V *= E.Value; } } }
	}
	return V;
}

float FDCSimImpl::TraitSum(const S& Sim, int32 CriminalId, FName Kind)
{
	float V = 0.f;
	if (!Sim.Data.IsValid()) { return V; }
	for (FName T : Sim.GetAllTraits(CriminalId))
	{
		if (const FDCTraitDef* D = Sim.Data->Traits.Find(T)) { for (const FDCEffect& E : D->Effects) { if (E.Kind == Kind) { V += E.Value; } } }
	}
	return V;
}

// ─── crew helpers ───────────────────────────────────────────────────────────

FDCCriminal* FDCSimImpl::RosterMut(S& Sim, int32 Id) { return Sim.Roster.FindByPredicate([Id](const FDCCriminal& C) { return C.Id == Id; }); }

bool FDCSimImpl::SlotEnabled(const S& Sim, EDCGearSlot Slot) { return (int32)Slot < Sim.GetGearSlotCount(); }

float FDCSimImpl::FairCut(const S& Sim, const FDCCriminal& C)
{
	const FDCBalance& B = Bal(Sim);
	return B.FairCutBase + (C.Loyalty - 50) / B.FairCutLoyaltyDiv + TraitSum(Sim, C.Id, K_fairCutAdd) + SumGlobal(Sim, K_fairCutAdd);
}

float FDCSimImpl::Willingness(const S& Sim, const FDCCriminal& C, float Cut)
{
	const FDCBalance& B = Bal(Sim);
	float W = FMath::Clamp(1.f - B.WillSlope * FMath::Max(0.f, Cut - FairCut(Sim, C)), B.WillMin, 1.f);
	float PerkMin = 0.f;
	if (MaxGlobal(Sim, K_willMin, PerkMin)) { W = FMath::Max(W, FMath::Min(1.f, PerkMin)); }
	return W;
}

float FDCSimImpl::LevelFactor(const S& Sim, float Eff, int32 MLevel)
{
	const FDCBalance& B = Bal(Sim);
	if (Eff >= MLevel) { return FMath::Min(B.LFMaxFactor, B.LFAtOrAbove + B.LFPerLevelAbove * (Eff - MLevel)); }
	const int32 Under = FMath::Min(3, FMath::CeilToInt(MLevel - Eff));
	return B.LFBelow[FMath::Clamp(Under - 1, 0, B.LFBelow.Num() - 1)];
}

int32 FDCSimImpl::XPToNext(const S& Sim, int32 Level)
{
	const FDCBalance& B = Bal(Sim);
	return FMath::Max(1, FMath::RoundToInt(B.XPBase * FMath::Pow((float)Level, B.XPExp)));
}

int32 FDCSimImpl::GearPrice(const S& Sim, const FDCGearDef& G)
{
	return FMath::RoundToInt(G.Item.Price * ProdGlobal(Sim, K_gearPriceMult) / 10.f) * 10;
}

// ─── clock ──────────────────────────────────────────────────────────────────

double FDCSimImpl::TotalHours(const S& Sim, double T) { const FDCBalance& B = Bal(Sim); return B.StartHour + T * B.GameMinutesPerSecond / 60.0; }
int32 FDCSimImpl::DawnIndexAt(const S& Sim, double T) { return FMath::FloorToInt((TotalHours(Sim, T) - Bal(Sim).DawnHour) / 24.0); }
double FDCSimImpl::DawnTime(const S& Sim, int32 I)
{
	const FDCBalance& B = Bal(Sim);
	return (B.DawnHour + 24.0 * I - B.StartHour) * 60.0 / B.GameMinutesPerSecond;
}

// ─── generation ─────────────────────────────────────────────────────────────

FDCCriminal FDCSimImpl::MakeCriminal(S& Sim, EDCCriminalType Type, EDCQuality Quality, int32 Level, FName ForcedAbility)
{
	const FDCSimData& D = *Sim.Data;
	const FDCBalance& B = D.Balance;
	FRandomStream& R = Sim.Rng;
	FDCCriminal C;
	C.Id = Sim.NextId++;
	C.Type = Type;
	C.Quality = Quality;
	// Prefer names nobody in the roster, recruit pool or memorial is using; fall back to the full list.
	TSet<FString> UsedFirst, UsedLast, UsedNick;
	for (const TArray<FDCCriminal>* List : { &Sim.Roster, &Sim.Recruits, &Sim.Fallen })
	{
		for (const FDCCriminal& O : *List) { UsedFirst.Add(O.FirstName); UsedLast.Add(O.LastName); UsedNick.Add(O.Nickname); }
	}
	auto PickUnused = [&R](const TArray<FString>& All, const TSet<FString>& Used) -> FString
	{
		TArray<FString> Free;
		for (const FString& N : All) { if (!Used.Contains(N)) { Free.Add(N); } }
		const TArray<FString>& Pool = Free.Num() > 0 ? Free : All;
		return Pool[R.RandRange(0, Pool.Num() - 1)];
	};
	// One draw from every first name; the list it came from sets the gender (names for anyone go by the seed), so
	// gender costs no extra RNG draws.
	TArray<FString> AllFirst = D.MaleFirstNames;
	AllFirst.Append(D.FemaleFirstNames);
	AllFirst.Append(D.FirstNames);
	C.FirstName = PickUnused(AllFirst, UsedFirst);
	C.LastName = PickUnused(D.LastNames, UsedLast);
	C.Nickname = PickUnused(D.Nicknames.FindChecked(Type), UsedNick);
	C.Seed = R.RandRange(0, MAX_int32 - 1);
	C.bFemale = D.FemaleFirstNames.Contains(C.FirstName) || (!D.MaleFirstNames.Contains(C.FirstName) && (C.Seed & 1));
	// A random body nobody in the roster or recruit pool is wearing (least-worn if every one is taken). Chosen
	// from the seed, so it costs no extra RNG draws.
	{
		const int32 NumBodies = C.bFemale ? UE_ARRAY_COUNT(DCPaths::CrewBodiesFemale) : UE_ARRAY_COUNT(DCPaths::CrewBodies);
		TArray<int32> Worn;
		Worn.SetNumZeroed(NumBodies);
		for (const TArray<FDCCriminal>* List : { &Sim.Roster, &Sim.Recruits })
		{
			for (const FDCCriminal& O : *List)
			{
				const int32 OB = O.Body != INDEX_NONE ? O.Body : FMath::Abs(O.Id) % NumBodies;
				if (O.bFemale == C.bFemale && Worn.IsValidIndex(OB)) { ++Worn[OB]; }
			}
		}
		const int32 Least = FMath::Min(Worn);
		TArray<int32> Free;
		for (int32 b = 0; b < NumBodies; ++b) { if (Worn[b] == Least) { Free.Add(b); } }
		C.Body = Free[(C.Seed >> 1) % Free.Num()];
	}
	C.Level = FMath::Clamp(Level, 1, B.MaxLevel);
	C.XP = 0;
	C.XPToNext = XPToNext(Sim, C.Level);
	C.Loyalty = B.LoyaltyStart;

	const int32 Q = (int32)Quality;
	if (const FDCTypeDef* TD = D.FindType(Type))
	{
		TArray<FName> Pool = TD->Abilities;
		Shuffle(R, Pool);
		if (!ForcedAbility.IsNone()) { Pool.Remove(ForcedAbility); C.Abilities.Add(ForcedAbility); }
		const int32 NumAb = FMath::Max(1, B.QualityAbilities[Q]);
		for (int32 i = 0; i < Pool.Num() && C.Abilities.Num() < NumAb; ++i) { C.Abilities.Add(Pool[i]); }

		TArray<FName> Pos, Neg;
		for (FName T : TD->TraitPool)
		{
			const FDCTraitDef* TDef = D.Traits.Find(T);
			if (TDef) { (TDef->Trait.bNegative ? Neg : Pos).Add(T); }
		}
		Shuffle(R, Pos);
		Shuffle(R, Neg);
		// quality.traits positive traits, plus (by chance) one negative trait on top.
		for (int32 i = 0; i < B.QualityTraits[Q] && Pos.Num() > 0; ++i) { C.Traits.Add(Pos.Pop()); }
		if (R.FRand() < B.NegativeTraitChance[Q] && Neg.Num() > 0) { C.Traits.Add(Neg.Pop()); }
	}
	C.HireCost = Round50((B.RecruitCostBase + B.RecruitCostPerLevel * (C.Level - 1)) * B.QualityCostMult[Q]);
	return C;
}

FDCMission FDCSimImpl::MakeMission(S& Sim, const FDCMissionTemplate& T)
{
	const FDCSimData& D = *Sim.Data;
	const FDCBalance& B = D.Balance;
	FRandomStream& R = Sim.Rng;
	FDCMission M;
	M.Id = Sim.NextId++;
	M.TemplateId = T.Id;
	M.DisplayName = T.DisplayName;
	M.Flavour = T.Flavour.Num() ? T.Flavour[R.RandRange(0, T.Flavour.Num() - 1)] : FString();
	M.Icon = T.Icon;
	M.Tier = T.Tier;
	const FIntPoint LR = B.LevelRange.IsValidIndex(T.Tier - 1) ? B.LevelRange[T.Tier - 1] : FIntPoint(1, 1);
	M.Level = RandIn(R, LR);
	M.CrewSize = FMath::Max(1, RandIn(R, T.CrewSize));
	M.Duration = FMath::Max(5.f, FMath::RoundToFloat((float)RandIn(R, T.DurationSec) / 5.f) * 5.f);
	M.Threats = T.FixedThreats;
	TArray<EDCThreat> Pool = T.PoolThreats;
	Shuffle(R, Pool);
	const int32 NExtra = FMath::Min(RandIn(R, T.Extra), Pool.Num());
	for (int32 i = 0; i < NExtra; ++i) { M.Threats.Add(Pool[i]); }
	M.Cost = Round50((float)RandIn(R, T.Cost));
	M.Payout = Round50((float)RandIn(R, T.Payout));
	M.XPReward = T.XPReward;
	M.ArrestRisk = T.ArrestRisk;
	M.bNight = R.FRand() < T.NightChance;
	M.bFinal = T.bFinal;
	M.bRare = !T.bFinal && R.FRand() < B.RareChance;
	if (M.bRare)
	{
		TArray<FName> Loot;
		for (FName G : D.GearOrder) { if (D.Gear[G].bLoot) { Loot.Add(G); } }
		if (Loot.Num()) { M.LootGearId = Loot[R.RandRange(0, Loot.Num() - 1)]; }
		// Rare jobs are the big scores of their tier: richer, but tougher and busier.
		M.Payout = Round50(M.Payout * B.RarePayoutMult);
		M.Level = FMath::Clamp(M.Level + B.RareLevelAdd, 1, B.MaxLevel);
		TArray<EDCThreat> More;
		for (EDCThreat Th : T.PoolThreats) { if (!M.Threats.Contains(Th)) { More.AddUnique(Th); } }
		if (More.Num() < B.RareExtraThreats)
		{
			for (int32 t = 0; t < (int32)EDCThreat::MAX; ++t) { if (!M.Threats.Contains((EDCThreat)t)) { More.AddUnique((EDCThreat)t); } }
		}
		Shuffle(R, More);
		for (int32 i = 0; i < B.RareExtraThreats && i < More.Num(); ++i) { M.Threats.Add(More[i]); }
	}
	M.ExpiresAt = T.bFinal ? -1.0 : Sim.RunTime + FMath::RoundToDouble(RandIn(R, B.ExpireSecRange));
	return M;
}

const FDCMissionTemplate* FDCSimImpl::PickTemplate(S& Sim)
{
	const FDCSimData& D = *Sim.Data;
	const FDCBalance& B = D.Balance;
	const int32 Unlocked = Sim.GetUnlockedTier();
	const TArray<float>& W = B.TierWeights[FMath::Clamp(Unlocked - 1, 0, B.TierWeights.Num() - 1)];
	auto TierHasTemplates = [&D](int32 Tier) { return D.Missions.ContainsByPredicate([Tier](const FDCMissionTemplate& T) { return T.Tier == Tier && !T.bFinal && T.Weight > 0.f; }); };
	float Total = 0.f;
	for (int32 t = 1; t <= Unlocked; ++t) { if (W.IsValidIndex(t - 1) && TierHasTemplates(t)) { Total += W[t - 1]; } }
	int32 Tier = 1;
	if (Total > 0.f)
	{
		float Pick = Sim.Rng.FRand() * Total;
		for (int32 t = 1; t <= Unlocked; ++t)
		{
			if (!W.IsValidIndex(t - 1) || !TierHasTemplates(t)) { continue; }
			Tier = t;
			Pick -= W[t - 1];
			if (Pick < 0.f) { break; }
		}
	}
	while (Tier > 1 && !TierHasTemplates(Tier)) { --Tier; }
	TArray<const FDCMissionTemplate*> Cands;
	float TW = 0.f;
	for (const FDCMissionTemplate& T : D.Missions) { if (T.Tier == Tier && !T.bFinal && T.Weight > 0.f) { Cands.Add(&T); TW += T.Weight; } }
	if (Cands.Num() == 0) { return nullptr; }
	float Pick = Sim.Rng.FRand() * TW;
	for (const FDCMissionTemplate* T : Cands) { Pick -= T->Weight; if (Pick < 0.f) { return T; } }
	return Cands.Last();
}

void FDCSimImpl::PostMission(S& Sim, const FDCMissionTemplate& T)
{
	const FDCMission& M = Sim.Missions.Add_GetRef(MakeMission(Sim, T));
	Sim.MarkChanged();
	Sim.OnMissionAdded.Broadcast(M);
}

void FDCSimImpl::TryPostFinal(S& Sim)
{
	const FDCBalance& B = Bal(Sim);
	if (Sim.MissionsSucceeded < B.FinalRequiredSuccesses) { return; }
	float Dummy = 0.f;
	if (!MaxGlobal(Sim, K_unlockFinal, Dummy)) { return; }
	if (Sim.Missions.ContainsByPredicate([](const FDCMission& M) { return M.bFinal; })) { return; }
	const FDCMissionTemplate* T = Sim.Data->FindTemplate(B.FinalTemplateId);
	if (!T) { T = Sim.Data->Missions.FindByPredicate([](const FDCMissionTemplate& X) { return X.bFinal; }); }
	if (T) { PostMission(Sim, *T); }
}

void FDCSimImpl::RegenerateRecruits(S& Sim)
{
	const FDCBalance& B = Bal(Sim);
	FRandomStream& R = Sim.Rng;
	Sim.Recruits.Reset();
	const int32 Tier = Sim.GetUnlockedTier();
	const TArray<float>& Odds = B.QualityOdds[FMath::Clamp(Tier - 1, 0, B.QualityOdds.Num() - 1)];
	const float QAdd = SumGlobal(Sim, K_recruitQualityAdd);
	const int32 LAdd = FMath::RoundToInt(SumGlobal(Sim, K_recruitLevelAdd));
	const FIntPoint LR = B.LevelRange.IsValidIndex(Tier - 1) ? B.LevelRange[Tier - 1] : FIntPoint(1, 1);
	const int32 Size = FMath::Max(1, B.RecruitBoardSize + FMath::RoundToInt(SumGlobal(Sim, K_recruitBoardAdd)));
	// Deal roles from a shuffled bag so a board never doubles up on a role until every role is shown.
	TArray<EDCCriminalType> Bag;
	for (int32 i = 0; i < Size; ++i)
	{
		if (Bag.Num() == 0)
		{
			for (int32 t = 0; t < (int32)EDCCriminalType::MAX; ++t) { Bag.Add((EDCCriminalType)t); }
			Shuffle(R, Bag);
		}
		const EDCCriminalType Type = Bag.Pop();
		float Sum = 0.f;
		for (float O : Odds) { Sum += O; }
		float Pick = R.FRand() * FMath::Max(Sum, KINDA_SMALL_NUMBER);
		int32 Q = 0;
		for (int32 q = 0; q < Odds.Num() && q < 3; ++q) { Q = q; Pick -= Odds[q]; if (Pick < 0.f) { break; } }
		int32 Steps = FMath::FloorToInt(QAdd);
		if (R.FRand() < QAdd - Steps) { ++Steps; }
		Q = FMath::Clamp(Q + Steps, 0, 2);
		const int32 Level = RandIn(R, LR) + LAdd;
		Sim.Recruits.Add(MakeCriminal(Sim, Type, (EDCQuality)Q, Level));
	}
	// A crew of fewer than two can always afford to fill out: keep enough cheap Street hires listed.
	int32 Street = Sim.Recruits.FilterByPredicate([](const FDCCriminal& C) { return C.Quality == EDCQuality::Street; }).Num();
	for (int32 i = 0; i < Sim.Recruits.Num() && Street < 2 - Sim.Roster.Num(); ++i)
	{
		FDCCriminal& C = Sim.Recruits[i];
		if (C.Quality == EDCQuality::Street) { continue; }
		C = MakeCriminal(Sim, C.Type, EDCQuality::Street, C.Level);
		++Street;
	}
	Sim.RecruitRefreshAt = Sim.RunTime + B.RecruitRefreshSec;
	Sim.MarkChanged();
}

void FDCSimImpl::GenerateStartingRoster(S& Sim)
{
	const FDCSimData& D = *Sim.Data;
	const FDCBalance& B = D.Balance;
	FRandomStream& R = Sim.Rng;
	TArray<EDCCriminalType> Free;
	for (int32 i = 0; i < (int32)EDCCriminalType::MAX; ++i) { Free.Add((EDCCriminalType)i); }
	Shuffle(R, Free);
	const int32 N = FMath::Clamp(B.StartingRoster, 0, (int32)EDCCriminalType::MAX);
	if (N == 0) { return; }

	// Cover the starting board: one specialist per posting (first fixed threat), levelled to the mission.
	TArray<FDCMission> Board = Sim.Missions;
	for (int32 mi = 0; mi < Board.Num() && Sim.Roster.Num() < N; ++mi)
	{
		const FDCMission& M = Board[mi];
		if (M.Threats.Num() == 0) { continue; }
		bool bDone = false;
		for (EDCThreat Need : M.Threats)
		{
			for (int32 fi = 0; fi < Free.Num() && !bDone; ++fi)
			{
				const FDCTypeDef* TD = D.FindType(Free[fi]);
				if (!TD) { continue; }
				for (FName A : TD->Abilities)
				{
					const FDCAbility* Ab = D.Abilities.Find(A);
					if (Ab && Ab->Counters == Need)
					{
						const int32 Lvl = FMath::Max(RandIn(R, B.StartingRosterLevels), M.Level);
						FDCCriminal C = MakeCriminal(Sim, Free[fi], EDCQuality::Street, Lvl, A);
						C.HireCost = 0;
						Sim.Roster.Add(C);
						Free.RemoveAt(fi);
						bDone = true;
						break;
					}
				}
			}
			if (bDone) { break; }
		}
	}
	while (Sim.Roster.Num() < N && Free.Num() > 0)
	{
		FDCCriminal C = MakeCriminal(Sim, Free.Pop(), EDCQuality::Street, RandIn(R, B.StartingRosterLevels));
		C.HireCost = 0;
		Sim.Roster.Add(C);
	}
}

int32 FDCSimImpl::BoardCount(const S& Sim) { return Sim.Missions.FilterByPredicate([](const FDCMission& M) { return !M.bInProgress && !M.bFinal; }).Num(); }
int32 FDCSimImpl::BoardSize(const S& Sim) { return FMath::Max(1, Bal(Sim).BoardBaseSize + FMath::RoundToInt(SumGlobal(Sim, K_boardSize))); }
double FDCSimImpl::PostInterval(const S& Sim) { return FMath::Max(1.0, (double)Bal(Sim).PostIntervalSec * ProdGlobal(Sim, K_postIntervalMult)); }

// ─── success formula ────────────────────────────────────────────────────────

FDCSuccessBreakdown FDCSimImpl::Evaluate(const S& Sim, const FDCMission& M, const TArray<int32>& CrewIds, float InCut, bool bNight)
{
	const FDCBalance& B = Bal(Sim);
	FDCSuccessBreakdown Bd;
	const float Cut = FMath::Clamp(InCut, B.CutMin, B.CutMax);

	TArray<const FDCCriminal*> Crew;
	for (int32 Id : CrewIds)
	{
		const FDCCriminal* C = Sim.Roster.FindByPredicate([Id](const FDCCriminal& X) { return X.Id == Id; });
		if (C && !Crew.Contains(C)) { Crew.Add(C); }
	}

	// Counters: each crew counter instance (ability, then gear) can neutralise one threat; prefer the lowest-level holder.
	struct FCounter { int32 CrimId; float Eff; int32 Order; EDCThreat Threat; FName Source; bool bUsed; };
	TArray<FCounter> Counters;
	for (int32 ci = 0; ci < Crew.Num(); ++ci)
	{
		const FDCCriminal& C = *Crew[ci];
		const float Eff = Sim.GetEffectiveLevel(C.Id);
		for (FName A : C.Abilities)
		{
			if (const FDCAbility* Ab = Sim.FindAbility(A)) { Counters.Add({ C.Id, Eff, ci, Ab->Counters, A, false }); }
		}
		for (int32 s = 0; s < (int32)EDCGearSlot::MAX; ++s)
		{
			const FName* G = C.Equipped.Find((EDCGearSlot)s);
			if (!G || !SlotEnabled(Sim, (EDCGearSlot)s)) { continue; }
			const FDCGearItem* GI = Sim.FindGear(*G);
			if (GI && GI->Counters != EDCThreat::MAX) { Counters.Add({ C.Id, Eff, ci, GI->Counters, *G, false }); }
		}
	}
	int32 Countered = 0;
	for (EDCThreat T : M.Threats)
	{
		FDCThreatSlot Slot;
		Slot.Threat = T;
		int32 Best = INDEX_NONE;
		for (int32 k = 0; k < Counters.Num(); ++k)
		{
			const FCounter& Ct = Counters[k];
			if (Ct.bUsed || Ct.Threat != T) { continue; }
			if (Best == INDEX_NONE || Ct.Eff < Counters[Best].Eff || (Ct.Eff == Counters[Best].Eff && Ct.Order < Counters[Best].Order)) { Best = k; }
		}
		if (Best != INDEX_NONE)
		{
			Counters[Best].bUsed = true;
			Slot.CounteredBy = Counters[Best].CrimId;
			Slot.Source = Counters[Best].Source;
			++Countered;
		}
		Bd.Threats.Add(Slot);
	}

	const float Points = FMath::Max(0.001f, M.CrewSize + B.ThreatPoints * M.Threats.Num());
	float LFSum = 0.f;
	for (const FDCCriminal* C : Crew)
	{
		const float LF = LevelFactor(Sim, Sim.GetEffectiveLevel(C->Id), M.Level);
		Bd.MemberFactor.Add(C->Id, LF);
		LFSum += LF;
	}
	// baseMult is the execution risk: a perfect plan alone falls short of 100%; modifiers close the gap.
	Bd.CrewChance = B.BaseMult * LFSum / Points;
	Bd.ThreatChance = B.BaseMult * B.ThreatPoints * Countered / Points;

	// Duration first (chanceShort/chanceLong look at the final duration).
	float DurMult = 1.f;
	for (const FDCCriminal* C : Crew) { DurMult *= TraitProd(Sim, C->Id, K_durationMult); }
	ForEachGlobalEffect(Sim, [&](const FDCEffect& E, FName, FName, const FString&) { if (E.Kind == K_durationMult) { DurMult *= E.Value; } });
	DurMult = FMath::Max(DurMult, B.MinDurationMult);
	Bd.Duration = M.Duration * DurMult;

	auto AnyType = [&Crew](const FString& Param, int32 ExceptId)
	{
		return Crew.ContainsByPredicate([&](const FDCCriminal* X) { return X->Id != ExceptId && TypeMatches(Param, X->Type); });
	};
	auto ChanceApplies = [&](const FDCEffect& E, int32 HolderId) -> bool
	{
		if (E.Kind == K_chance) { return true; }
		if (E.Kind == K_chanceNight) { return bNight; }
		if (E.Kind == K_chanceDay) { return !bNight; }
		if (E.Kind == K_chanceShort) { return Bd.Duration <= E.Threshold; }
		if (E.Kind == K_chanceLong) { return Bd.Duration >= E.Threshold; }
		if (E.Kind == K_chanceSolo) { return M.CrewSize == 1; }
		if (E.Kind == K_chanceThreat) { EDCThreat T; return FDCSimData::ParseThreat(E.Param, T) && M.Threats.Contains(T); }
		if (E.Kind == K_chanceWithType) { return AnyType(E.Param, HolderId); }
		if (E.Kind == K_chanceTierMin) { return M.Tier >= E.Threshold; }
		if (E.Kind == K_chanceIfType) { return AnyType(E.Param, INDEX_NONE); }
		return false;
	};
	auto IsChanceKind = [](FName K)
	{
		return K == K_chance || K == K_chanceNight || K == K_chanceDay || K == K_chanceShort || K == K_chanceLong || K == K_chanceSolo
			|| K == K_chanceThreat || K == K_chanceWithType || K == K_chanceTierMin || K == K_chanceIfType;
	};

	float CostMult = 1.f, PayMult = 1.f;
	auto AddHit = [&Bd](const FDCModifierHit& H) { Bd.Modifiers.Add(H); Bd.ModifierChance += H.ChanceDelta; };

	// Traits on crew (innate + gear).
	for (const FDCCriminal* C : Crew)
	{
		for (FName TId : Sim.GetAllTraits(C->Id))
		{
			const FDCTraitDef* TD = Sim.Data.IsValid() ? Sim.Data->Traits.Find(TId) : nullptr;
			if (!TD) { continue; }
			FDCModifierHit H;
			H.Id = TId; H.Icon = TD->Trait.Icon; H.Label = TD->Trait.DisplayName; H.CriminalId = C->Id; H.bNegative = TD->Trait.bNegative;
			bool bApplied = false;
			for (const FDCEffect& E : TD->Effects)
			{
				if (IsChanceKind(E.Kind)) { if (ChanceApplies(E, C->Id)) { H.ChanceDelta += E.Value; bApplied = true; } }
				else if (E.Kind == K_durationMult) { H.DurationMult *= E.Value; bApplied = true; }
				else if (E.Kind == K_payoutMult) { if (E.Param.IsEmpty() || AnyType(E.Param, INDEX_NONE)) { H.PayoutMult *= E.Value; PayMult *= E.Value; bApplied = true; } }
				else if (E.Kind == K_costMult) { CostMult *= E.Value; bApplied = true; }
				else if (IsMissionKind(E.Kind)) { bApplied = true; }
			}
			if (bApplied) { AddHit(H); }
		}
	}

	// Perks and rooms (global), grouped per source.
	TArray<FDCModifierHit> Globals;
	ForEachGlobalEffect(Sim, [&](const FDCEffect& E, FName Src, FName Icon, const FString& Label)
	{
		float dChance = 0.f, dDur = 1.f, dPay = 1.f;
		bool bApplied = false;
		if (E.Kind == K_chanceStreak)
		{
			const float Cap = E.Threshold > 0.f ? E.Threshold : TNumericLimits<float>::Max();
			dChance = FMath::Min(Cap, E.Value * Sim.SuccessStreak);
			bApplied = Sim.SuccessStreak > 0;
		}
		else if (E.Kind == K_chanceRookie)
		{
			const int32 Rookies = Crew.FilterByPredicate([](const FDCCriminal* X) { return X->MissionsRun == 0; }).Num();
			dChance = E.Value * Rookies;
			bApplied = Rookies > 0;
		}
		else if (IsChanceKind(E.Kind)) { if (ChanceApplies(E, INDEX_NONE)) { dChance = E.Value; bApplied = true; } }
		else if (E.Kind == K_durationMult) { dDur = E.Value; bApplied = true; }
		else if (E.Kind == K_payoutMult) { if (E.Param.IsEmpty() || AnyType(E.Param, INDEX_NONE)) { dPay = E.Value; PayMult *= E.Value; bApplied = true; } }
		else if (E.Kind == K_costMult) { CostMult *= E.Value; bApplied = true; }
		if (!bApplied) { return; }
		FDCModifierHit* H = Globals.FindByPredicate([Src](const FDCModifierHit& X) { return X.Id == Src; });
		if (!H) { H = &Globals.AddDefaulted_GetRef(); H->Id = Src; H->Icon = Icon; H->Label = Label; }
		H->ChanceDelta += dChance; H->DurationMult *= dDur; H->PayoutMult *= dPay;
	});
	for (FDCModifierHit& H : Globals) { H.bNegative = H.ChanceDelta < 0.f || H.DurationMult > 1.f || H.PayoutMult < 1.f; AddHit(H); }

	if (M.bNight && bNight && B.NightMissionBonus != 0.f)
	{
		FDCModifierHit H;
		H.Id = TEXT("night"); H.Icon = TEXT("moon"); H.Label = TEXT("Night job"); H.ChanceDelta = B.NightMissionBonus;
		AddHit(H);
	}

	// Willingness.
	float WSum = 0.f;
	for (const FDCCriminal* C : Crew)
	{
		const float W = Willingness(Sim, *C, Cut);
		Bd.Willingness.Add(C->Id, W);
		WSum += W;
		if (W < B.RefusalThreshold) { Bd.RefusingIds.Add(C->Id); }
	}
	Bd.WillFactor = B.WillFactorBase + B.WillFactorScale * (Crew.Num() ? WSum / Crew.Num() : 1.f);

	Bd.SuccessChance = FMath::Clamp((Bd.CrewChance + Bd.ThreatChance + Bd.ModifierChance) * Bd.WillFactor, 0.f, B.MaxChance);
	Bd.BonusChance = FMath::Max(0.f, Bd.SuccessChance - 1.f);
	Bd.ArrestChance = FMath::Clamp(M.ArrestRisk * ProdGlobal(Sim, K_arrestMult), 0.f, 1.f);
	Bd.Cost = FMath::Max(0, FMath::RoundToInt(M.Cost * CostMult));
	Bd.Payout = FMath::Max(0, FMath::RoundToInt(M.Payout * PayMult));
	Bd.DirectorTake = FMath::RoundToInt(Bd.Payout * Cut);
	return Bd;
}

// ─── mission resolution ─────────────────────────────────────────────────────

void FDCSimImpl::CompleteMission(S& Sim, int32 MissionId)
{
	const int32 Idx = Sim.Missions.IndexOfByPredicate([MissionId](const FDCMission& M) { return M.Id == MissionId; });
	if (Idx == INDEX_NONE) { return; }
	const FDCSimData& D = *Sim.Data;
	const FDCBalance& B = D.Balance;
	FRandomStream& R = Sim.Rng;
	const FDCMission M = Sim.Missions[Idx];
	Sim.Missions.RemoveAt(Idx);

	FDCMissionReport Rep;
	Rep.ReportId = Sim.NextId++;
	Rep.Mission = M;
	Rep.CompletedAt = Sim.RunTime;
	Rep.Roll = R.FRand();
	const float S1 = FMath::Min(M.LockedChance, 1.f);
	if (Rep.Roll < S1)
	{
		Rep.Outcome = EDCMissionOutcome::Success;
		Rep.bBonus = M.LockedChance > 1.f && R.FRand() < M.LockedChance - 1.f;
	}
	else
	{
		Rep.Outcome = Rep.Roll >= 1.f - (1.f - S1) * B.BustedBand ? EDCMissionOutcome::Busted : EDCMissionOutcome::Failed;
	}
	const bool bSuccess = Rep.Outcome == EDCMissionOutcome::Success;
	if (bSuccess)
	{
		Rep.PayoutTotal = FMath::RoundToInt(M.Payout * (Rep.bBonus ? B.BonusPayoutMult : 1.f));
		Rep.DirectorTake = FMath::RoundToInt(Rep.PayoutTotal * M.Cut);
		Sim.Cash += Rep.DirectorTake;
		Sim.TotalTake += Rep.DirectorTake;
		if (Rep.DirectorTake > Sim.BestMissionTake) { Sim.BestMissionTake = Rep.DirectorTake; Sim.BestMissionName = M.DisplayName; }
		Sim.SuccessStreak++;
	}
	else
	{
		Sim.SuccessStreak = 0;
		const float RefundFrac = FMath::Clamp(SumGlobal(Sim, K_failRefund), 0.f, 1.f);
		if (RefundFrac > 0.f && M.Cost > 0)
		{
			Rep.Refund = FMath::RoundToInt(M.Cost * RefundFrac);
			Sim.Cash += Rep.Refund;
		}
	}

	const float GXp = ProdGlobal(Sim, K_xpMult);
	const float GInjChance = ProdGlobal(Sim, K_injuryChanceMult);
	const float GInjTime = ProdGlobal(Sim, K_injuryTimeMult);
	const float GArrest = ProdGlobal(Sim, K_arrestMult);
	float PreventDummy = 0.f;
	const bool bHasPrevent = MaxGlobal(Sim, K_preventFirstArrest, PreventDummy) && PreventDummy > 0.f;

	TArray<TPair<int32, int32>> LevelUps;
	TArray<int32> Arrested, Quit;
	for (int32 Id : M.CrewIds)
	{
		FDCCriminal* C = RosterMut(Sim, Id);
		if (!C) { continue; }
		FDCCrewMemberResult MR;
		MR.CriminalId = Id;
		MR.Name = C->GetDisplayName();
		C->MissionsRun++;
		C->Status = EDCCriminalStatus::Available;
		C->ActiveMissionId = INDEX_NONE;

		// XP
		const float XpMult = GXp * TraitProd(Sim, Id, K_xpMult);
		MR.XPGained = FMath::RoundToInt(M.XPReward * (bSuccess ? 1.f : B.FailXpMult) * XpMult);
		if (C->Level < B.MaxLevel)
		{
			C->XP += MR.XPGained;
			while (C->Level < B.MaxLevel && C->XP >= C->XPToNext)
			{
				C->XP -= C->XPToNext;
				C->Level++;
				C->XPToNext = XPToNext(Sim, C->Level);
				MR.bLevelledUp = true;
			}
			if (C->Level >= B.MaxLevel) { C->XP = 0; }
		}
		MR.NewLevel = C->Level;
		if (MR.bLevelledUp) { LevelUps.Add(TPair<int32, int32>(Id, C->Level)); }

		// Loyalty: a fair cut on a success builds loyalty (the greedy penalty was applied at mission start).
		int32 dLoy = 0;
		if (bSuccess && M.Cut <= FairCut(Sim, *C) + 0.001f) { dLoy += B.LoyaltySuccessFair; }

		// Injury
		if (!bSuccess)
		{
			const float Chance = (Rep.Outcome == EDCMissionOutcome::Busted ? B.InjuryBustedChance : B.InjuryFailChance) * GInjChance * TraitProd(Sim, Id, K_injuryChanceMult);
			if (R.FRand() < Chance)
			{
				MR.bInjured = true;
				MR.InjurySeconds = FMath::RoundToFloat((float)RandIn(R, B.InjurySecondsRange) * GInjTime);
				C->Status = EDCCriminalStatus::Injured;
				C->InjuredUntil = Sim.RunTime + MR.InjurySeconds;
				dLoy += B.LoyaltyInjury;
			}
		}
		// Arrest
		if (Rep.Outcome == EDCMissionOutcome::Busted)
		{
			const float Chance = M.ArrestRisk * GArrest * TraitProd(Sim, Id, K_arrestMult);
			if (R.FRand() < Chance)
			{
				if (bHasPrevent && !Sim.bArrestPreventUsed) { Sim.bArrestPreventUsed = true; MR.bArrestPrevented = true; }
				else { MR.bArrested = true; Arrested.Add(Id); }
			}
		}
		const int32 OldLoy = C->Loyalty;
		C->Loyalty = FMath::Clamp(C->Loyalty + dLoy, B.LoyaltyMin, B.LoyaltyMax);
		MR.LoyaltyDelta = C->Loyalty - OldLoy;
		MR.LoyaltyAfter = C->Loyalty;
		MR.Type = C->Type;
		MR.Quality = C->Quality;
		if (!MR.bArrested && C->Loyalty < B.LoyaltyQuitBelow) { MR.bQuit = true; Quit.Add(Id); }
		Rep.Members.Add(MR);
	}
	for (int32 Id : Arrested)
	{
		const int32 RI = Sim.Roster.IndexOfByPredicate([Id](const FDCCriminal& C) { return C.Id == Id; });
		if (RI == INDEX_NONE) { continue; }
		FDCCriminal C = Sim.Roster[RI];
		Sim.Roster.RemoveAt(RI);
		C.Status = EDCCriminalStatus::Arrested;
		C.ActiveMissionId = INDEX_NONE;
		C.InjuredUntil = 0.0;
		Sim.Fallen.Add(C);
		Sim.ArrestedAt.Add(Id, Sim.RunTime);
	}
	for (int32 Id : Quit)
	{
		const int32 RI = Sim.Roster.IndexOfByPredicate([Id](const FDCCriminal& C) { return C.Id == Id; });
		if (RI == INDEX_NONE) { continue; }
		for (const TPair<EDCGearSlot, FName>& G : Sim.Roster[RI].Equipped) { Sim.Stash.Add(G.Value); }
		Sim.Roster.RemoveAt(RI);
	}

	if (bSuccess)
	{
		Sim.MissionsSucceeded++;
		const float LootAdd = SumGlobal(Sim, K_lootChanceAdd);
		if (!M.LootGearId.IsNone() && R.FRand() < B.RareLootChance + LootAdd) { Rep.LootGearId = M.LootGearId; }
		else if (R.FRand() < B.LootChance + LootAdd)
		{
			TArray<FName> Pool;
			for (FName G : D.GearOrder) { const FDCGearDef& GD = D.Gear[G]; if (GD.bLoot || GD.Item.Tier <= M.Tier) { Pool.Add(G); } }
			if (Pool.Num()) { Rep.LootGearId = Pool[R.RandRange(0, Pool.Num() - 1)]; }
		}
		if (!Rep.LootGearId.IsNone()) { Sim.Stash.Add(Rep.LootGearId); }
	}

	Sim.Reports.Insert(Rep, 0);
	if (Sim.Reports.Num() > 60) { Sim.Reports.SetNum(60); }
	Sim.MarkChanged();
	Sim.bSaveDirty = true;

	for (const TPair<int32, int32>& L : LevelUps) { Sim.OnCriminalLevelUp.Broadcast(L.Key, L.Value); }
	Sim.OnMissionCompleted.Broadcast(Sim.Reports[0]);

	if (bSuccess && Sim.MissionsSucceeded % B.PerkEveryN == 0) { OfferPerks(Sim); }
	if (bSuccess && M.bFinal) { EndRun(Sim, true, TEXT("The Big Score")); return; }
	TryPostFinal(Sim);
}

void FDCSimImpl::OfferPerks(S& Sim)
{
	if (Sim.PerkOffer.Num() > 0) { Sim.PendingPerkOffers++; return; }
	TArray<FName> Pool;
	for (FName P : Sim.Data->PerkOrder)
	{
		if (!Sim.PerksDiscarded.Contains(P) && !Sim.PerksTaken.ContainsByPredicate([P](const FDCPerk& X) { return X.Id == P; })) { Pool.Add(P); }
	}
	if (Pool.Num() == 0) { return; }
	Shuffle(Sim.Rng, Pool);
	for (int32 i = 0; i < Pool.Num() && i < Bal(Sim).PerkOfferSize; ++i) { Sim.PerkOffer.Add(Sim.Data->Perks[Pool[i]].Perk); }
	Sim.MarkChanged();
	Sim.OnPerkOffered.Broadcast(Sim.PerkOffer);
}

// ─── time ───────────────────────────────────────────────────────────────────

double FDCSimImpl::NextEventTime(const S& Sim)
{
	double T = DawnTime(Sim, Sim.LastDawnIndex + 1);
	T = FMath::Min(T, Sim.NextPostAt);
	T = FMath::Min(T, Sim.RecruitRefreshAt);
	for (const FDCMission& M : Sim.Missions)
	{
		if (M.bInProgress) { T = FMath::Min(T, M.EndsAt); }
		else if (M.ExpiresAt >= 0.0) { T = FMath::Min(T, M.ExpiresAt); }
	}
	for (const FDCRoom& R : Sim.Rooms) { if (R.State == EDCRoomState::Building) { T = FMath::Min(T, R.BuildEndsAt); } }
	for (const FDCCriminal& C : Sim.Roster) { if (C.Status == EDCCriminalStatus::Injured) { T = FMath::Min(T, C.InjuredUntil); } }
	return T;
}

void FDCSimImpl::ProcessDue(S& Sim)
{
	const double Now = Sim.RunTime;
	auto Playing = [&Sim]() { return Sim.Phase == EDCGamePhase::Playing; };

	// 1. missions finishing, in EndsAt order
	for (;;)
	{
		const FDCMission* Due = nullptr;
		for (const FDCMission& M : Sim.Missions)
		{
			if (M.bInProgress && M.EndsAt <= Now && (!Due || M.EndsAt < Due->EndsAt || (M.EndsAt == Due->EndsAt && M.Id < Due->Id))) { Due = &M; }
		}
		if (!Due) { break; }
		CompleteMission(Sim, Due->Id);
		if (!Playing()) { return; }
	}
	// 2. room builds
	for (FDCRoom& R : Sim.Rooms)
	{
		if (R.State == EDCRoomState::Building && R.BuildEndsAt <= Now)
		{
			R.State = EDCRoomState::Built;
			R.Level = FMath::Min(3, R.Level + 1);
			Sim.RoomsBuilt++;
			Sim.MarkChanged();
			Sim.bSaveDirty = true;
			Sim.OnRoomChanged.Broadcast(Sim.GetRoom(R.Type));
		}
	}
	// 3. injuries heal
	for (FDCCriminal& C : Sim.Roster)
	{
		if (C.Status == EDCCriminalStatus::Injured && C.InjuredUntil <= Now)
		{
			C.Status = EDCCriminalStatus::Available;
			C.InjuredUntil = 0.0;
			Sim.MarkChanged();
			Sim.OnCriminalHealed.Broadcast(C.Id);
		}
	}
	// 4. postings expire
	const int32 Removed = Sim.Missions.RemoveAll([Now](const FDCMission& M) { return !M.bInProgress && M.ExpiresAt >= 0.0 && M.ExpiresAt <= Now; });
	if (Removed > 0) { Sim.MarkChanged(); }
	// 5. dawn: payday, then a new day
	while (Playing() && DawnTime(Sim, Sim.LastDawnIndex + 1) <= Now)
	{
		Sim.LastDawnIndex++;
		Payday(Sim);
	}
	if (!Playing()) { return; }
	// 6. The Big Score
	TryPostFinal(Sim);
	// 7. new postings
	if (BoardCount(Sim) < BoardSize(Sim))
	{
		if (Sim.NextPostAt <= Now)
		{
			if (const FDCMissionTemplate* T = PickTemplate(Sim)) { PostMission(Sim, *T); }
			Sim.NextPostAt = Now + PostInterval(Sim);
		}
	}
	else { Sim.NextPostAt = FMath::Max(Sim.NextPostAt, Now + PostInterval(Sim)); }
	// 8. recruits
	if (Sim.RecruitRefreshAt <= Now) { RegenerateRecruits(Sim); }
	// 9. broke with nobody left?
	CheckBroke(Sim);
}

void FDCSimImpl::Payday(S& Sim)
{
	const FDCBalance& B = Bal(Sim);
	const int32 Day = Sim.GetDay();
	const int32 Due = Sim.GetUpkeepDue();
	// Interest on the cash held overnight (before upkeep), one capped sum per perk. Payday reports the net.
	int32 Interest = 0;
	if (Sim.Cash > 0)
	{
		ForEachGlobalEffect(Sim, [&](const FDCEffect& E, FName, FName, const FString&)
		{
			if (E.Kind != K_dawnInterest) { return; }
			const int32 V = FMath::RoundToInt(Sim.Cash * E.Value);
			Interest += E.Threshold > 0.f ? FMath::Min(V, FMath::RoundToInt(E.Threshold)) : V;
		});
		Interest = FMath::Max(0, Interest);
		Sim.Cash += Interest;
	}
	Sim.Cash -= Due;
	Sim.MarkChanged();
	Sim.bSaveDirty = true;
	Sim.OnPayday.Broadcast(Day, Due - Interest);
	const int32 Idle = FMath::RoundToInt(SumGlobal(Sim, K_loyaltyIdlePerDay));
	if (Idle != 0)
	{
		for (FDCCriminal& C : Sim.Roster)
		{
			if (C.Status != EDCCriminalStatus::Injured) { C.Loyalty = FMath::Clamp(C.Loyalty + Idle, B.LoyaltyMin, B.LoyaltyMax); }
		}
	}
	if (Sim.Cash < B.CashFloor - FMath::RoundToInt(SumGlobal(Sim, K_creditLine))) { EndRun(Sim, false, TEXT("Broke at payday")); return; }
	Sim.OnDayStarted.Broadcast(Day);
}

void FDCSimImpl::CheckBroke(S& Sim)
{
	if (Sim.Phase != EDCGamePhase::Playing || Sim.Roster.Num() > 0 || Sim.GetActiveMissionCount() > 0) { return; }
	const FDCBalance& B = Bal(Sim);
	int32 Cheapest = Round50(B.RecruitCostBase * B.QualityCostMult[0]);
	for (const FDCCriminal& R : Sim.Recruits) { Cheapest = FMath::Min(Cheapest, R.HireCost); }
	if (Sim.Cash < Cheapest) { EndRun(Sim, false, TEXT("No crew, no cash")); }
}

void FDCSimImpl::EndRun(S& Sim, bool bWon, const FString& Reason)
{
	Sim.Phase = bWon ? EDCGamePhase::Won : EDCGamePhase::GameOver;
	Sim.EndReason = Reason;
	const FDCRunSummary Sum = Sim.GetRunSummary();
	Sim.RecordFinishedRun(); // keeps the slot (marked finished) + one leaderboard record
	Sim.bSaveDirty = false;
	Sim.MarkChanged();
	UE_LOG(LogDC, Display, TEXT("[Sim] run ended: %s (%s) day %d, %d successes, cash %d"), bWon ? TEXT("WON") : TEXT("GAME OVER"), *Reason, Sum.DaysSurvived, Sum.MissionsSucceeded, Sim.Cash);
	Sim.OnPhaseChanged.Broadcast(Sim.Phase);
	if (bWon) { Sim.OnRunWon.Broadcast(Sum); } else { Sim.OnGameOver.Broadcast(Sum); }
}
