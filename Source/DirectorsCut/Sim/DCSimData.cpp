// Director's Cut — JSON data loader (Agent A).
#include "Sim/DCSimData.h"
#include "DirectorsCut.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace DCSim
{
	template <typename E>
	static bool ParseEnum(const FString& S, E& Out, int32 Max, const TCHAR* (*NameFn)(E))
	{
		for (int32 i = 0; i < Max; ++i)
		{
			if (S.Equals(NameFn((E)i), ESearchCase::IgnoreCase)) { Out = (E)i; return true; }
		}
		return false;
	}
	static const TCHAR* RoomKey(EDCRoomType R)
	{
		static const TCHAR* N[] = { TEXT("Office"), TEXT("Lounge"), TEXT("Workshop"), TEXT("Garage"), TEXT("ScreeningRoom"), TEXT("Clinic"), TEXT("ServerCloset"), TEXT("LawyerOffice") };
		return N[(int32)R];
	}

	bool ParseType(const FString& S, EDCCriminalType& Out) { return ParseEnum<EDCCriminalType>(S, Out, (int32)EDCCriminalType::MAX, &DC::TypeName); }
	bool ParseStat(const FString& S, EDCStat& Out) { return ParseEnum<EDCStat>(S, Out, (int32)EDCStat::MAX, &DC::StatName); }
	bool ParseSlot(const FString& S, EDCGearSlot& Out) { return ParseEnum<EDCGearSlot>(S, Out, (int32)EDCGearSlot::MAX, &DC::SlotName); }
	bool ParseRoom(const FString& S, EDCRoomType& Out) { return ParseEnum<EDCRoomType>(S, Out, (int32)EDCRoomType::MAX, &RoomKey); }

	EDCModKind ParseModKind(const FString& S)
	{
		static const TCHAR* N[] = {
			TEXT("RosterCap"), TEXT("CrewsPerNight"), TEXT("JobBoardSize"), TEXT("IdleLoyalty"), TEXT("GearSlots"), TEXT("GearTier"), TEXT("GearDiscount"),
			TEXT("StatBonus"), TEXT("LuckBonus"), TEXT("ChanceBonus"), TEXT("PayoutMult"), TEXT("UpkeepMult"), TEXT("ArrestMult"), TEXT("InjuryMult"), TEXT("InjuryHealRate"),
			TEXT("FairCutBonus"), TEXT("MinWillingness"), TEXT("RecruitLevelBonus"), TEXT("PreventFirstArrest"), TEXT("Retakes"), TEXT("UnlockHeavyJobs"),
			TEXT("UnlockFinalCut"), TEXT("Intel"), TEXT("PatchUp"), TEXT("Bail"), TEXT("Dailies"), TEXT("Overtime") };
		static_assert(UE_ARRAY_COUNT(N) == (int32)EDCModKind::Unknown, "mod kind table");
		for (int32 i = 0; i < UE_ARRAY_COUNT(N); ++i)
		{
			if (S.Equals(N[i], ESearchCase::IgnoreCase)) { return (EDCModKind)i; }
		}
		return EDCModKind::Unknown;
	}
}

// ─── JSON helpers ───────────────────────────────────────────────────────────
namespace
{
	using FObj = TSharedPtr<FJsonObject>;

	float Num(const FObj& O, const TCHAR* K, float Def)
	{
		double V = 0.0;
		return (O.IsValid() && O->TryGetNumberField(K, V)) ? (float)V : Def;
	}
	int32 Int(const FObj& O, const TCHAR* K, int32 Def)
	{
		double V = 0.0;
		return (O.IsValid() && O->TryGetNumberField(K, V)) ? FMath::RoundToInt((float)V) : Def;
	}
	bool Bool(const FObj& O, const TCHAR* K, bool Def)
	{
		bool V = false;
		return (O.IsValid() && O->TryGetBoolField(K, V)) ? V : Def;
	}
	FString Str(const FObj& O, const TCHAR* K, const FString& Def = FString())
	{
		FString V;
		return (O.IsValid() && O->TryGetStringField(K, V)) ? V : Def;
	}
	FObj Obj(const FObj& O, const TCHAR* K)
	{
		const TSharedPtr<FJsonObject>* V = nullptr;
		return (O.IsValid() && O->TryGetObjectField(K, V) && V) ? *V : FObj();
	}
	const TArray<TSharedPtr<FJsonValue>>* Arr(const FObj& O, const TCHAR* K)
	{
		const TArray<TSharedPtr<FJsonValue>>* V = nullptr;
		return (O.IsValid() && O->TryGetArrayField(K, V)) ? V : nullptr;
	}
	TArray<FString> StrArr(const FObj& O, const TCHAR* K)
	{
		TArray<FString> Out;
		if (const auto* A = Arr(O, K))
		{
			for (const TSharedPtr<FJsonValue>& V : *A) { FString S; if (V.IsValid() && V->TryGetString(S)) { Out.Add(S); } }
		}
		return Out;
	}
	FIntPoint Range(const FObj& O, const TCHAR* K, FIntPoint Def)
	{
		if (const auto* A = Arr(O, K))
		{
			if (A->Num() >= 2)
			{
				double X = Def.X, Y = Def.Y;
				(*A)[0]->TryGetNumber(X); (*A)[1]->TryGetNumber(Y);
				FIntPoint R(FMath::RoundToInt((float)X), FMath::RoundToInt((float)Y));
				if (R.Y < R.X) { Swap(R.X, R.Y); }
				return R;
			}
		}
		return Def;
	}
	TArray<int32> IntArr(const FObj& O, const TCHAR* K, const TArray<int32>& Def)
	{
		const auto* A = Arr(O, K);
		if (!A) { return Def; }
		TArray<int32> Out;
		for (const TSharedPtr<FJsonValue>& V : *A) { double D = 0; if (V.IsValid() && V->TryGetNumber(D)) { Out.Add(FMath::RoundToInt((float)D)); } }
		return Out.Num() ? Out : Def;
	}
	TArray<EDCCriminalType> TypeArr(const FObj& O, const TCHAR* K, TArray<FString>& Warnings, const FString& Ctx)
	{
		TArray<EDCCriminalType> Out;
		for (const FString& S : StrArr(O, K))
		{
			EDCCriminalType T;
			if (DCSim::ParseType(S, T)) { Out.Add(T); }
			else { Warnings.Add(FString::Printf(TEXT("%s: unknown criminal type '%s'"), *Ctx, *S)); }
		}
		return Out;
	}

	FObj LoadJsonFile(const FString& DataDir, const TCHAR* File, TArray<FString>& Warnings)
	{
		FString Path = FPaths::Combine(DataDir, File);
		if (!FPaths::FileExists(Path))
		{
			const FString Fixture = FPaths::Combine(FDCSimData::FixturesDir(), File);
			if (FPaths::FileExists(Fixture))
			{
				UE_LOG(LogDC, Warning, TEXT("[Sim] %s missing; using test fixture %s"), *Path, *Fixture);
				Path = Fixture;
			}
			else
			{
				Warnings.Add(FString::Printf(TEXT("Data file %s not found (no fixture either); using built-in defaults"), File));
				return FObj();
			}
		}
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			Warnings.Add(FString::Printf(TEXT("Could not read %s"), *Path));
			return FObj();
		}
		FObj Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			Warnings.Add(FString::Printf(TEXT("JSON parse error in %s: %s"), *Path, *Reader->GetErrorMessage()));
			return FObj();
		}
		return Root;
	}

	FDCModifier ParseModifier(const FObj& O, TArray<FString>& Warnings, const FString& Ctx)
	{
		FDCModifier M;
		M.KindName = Str(O, TEXT("kind"));
		M.Kind = DCSim::ParseModKind(M.KindName);
		if (M.Kind == EDCModKind::Unknown) { Warnings.Add(FString::Printf(TEXT("%s: unknown modifier kind '%s' (ignored)"), *Ctx, *M.KindName)); }
		M.Value = Num(O, TEXT("value"), 0.f);
		const FString StatS = Str(O, TEXT("stat"));
		if (!StatS.IsEmpty())
		{
			M.bHasStat = DCSim::ParseStat(StatS, M.Stat);
			if (!M.bHasStat) { Warnings.Add(FString::Printf(TEXT("%s: unknown stat '%s'"), *Ctx, *StatS)); }
		}
		const FString TypeS = Str(O, TEXT("type"));
		if (!TypeS.IsEmpty())
		{
			M.bHasType = DCSim::ParseType(TypeS, M.Type);
			if (!M.bHasType) { Warnings.Add(FString::Printf(TEXT("%s: unknown type '%s'"), *Ctx, *TypeS)); }
		}
		M.Condition = Str(O, TEXT("condition"));
		return M;
	}
	TArray<FDCModifier> ParseModifiers(const FObj& O, TArray<FString>& Warnings, const FString& Ctx)
	{
		TArray<FDCModifier> Out;
		if (const auto* A = Arr(O, TEXT("modifiers")))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TSharedPtr<FJsonObject>* MO = nullptr;
				if (V.IsValid() && V->TryGetObject(MO) && MO)
				{
					FDCModifier M = ParseModifier(*MO, Warnings, Ctx);
					if (M.Kind != EDCModKind::Unknown) { Out.Add(M); }
				}
			}
		}
		return Out;
	}

	FDCTypeDef DefaultType(EDCCriminalType T)
	{
		FDCTypeDef D;
		D.Type = T;
		D.DisplayName = DC::TypeName(T);
		switch (T)
		{
		case EDCCriminalType::Wheelman:    D.Primary = EDCStat::Driving; D.Secondary = EDCStat::Muscle; break;
		case EDCCriminalType::Muscle:      D.Primary = EDCStat::Muscle;  D.Secondary = EDCStat::Driving; break;
		case EDCCriminalType::Hacker:      D.Primary = EDCStat::Tech;    D.Secondary = EDCStat::Stealth; break;
		case EDCCriminalType::Ghost:       D.Primary = EDCStat::Stealth; D.Secondary = EDCStat::Finesse; break;
		case EDCCriminalType::Face:        D.Primary = EDCStat::Charm;   D.Secondary = EDCStat::Tech; break;
		default:                           D.Primary = EDCStat::Finesse; D.Secondary = EDCStat::Tech; break;
		}
		return D;
	}

	FDCRoomDef DefaultRoom(EDCRoomType T)
	{
		FDCRoomDef R;
		R.Type = T;
		R.DisplayName = DC::RoomName(T);
		R.BuildCost = T == EDCRoomType::Office ? 0 : 10000;
		R.UpgradeCosts = { 8000, 12000 };
		R.UpkeepPerNight = T == EDCRoomType::Office ? 0 : 200;
		R.Levels.SetNum(3);
		return R;
	}
}

// ─── FDCSimData ─────────────────────────────────────────────────────────────

FString FDCSimData::DefaultDataDir() { return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data")); }
FString FDCSimData::FixturesDir() { return FPaths::Combine(FPaths::GameSourceDir(), TEXT("DirectorsCut/Tests/Fixtures")); }

const FDCTypeDef& FDCSimData::GetType(EDCCriminalType T) const
{
	for (const FDCTypeDef& D : Types) { if (D.Type == T) { return D; } }
	check(Types.Num() > 0);
	return Types[0];
}
const FDCRoomDef& FDCSimData::GetRoom(EDCRoomType T) const
{
	for (const FDCRoomDef& D : Rooms) { if (D.Type == T) { return D; } }
	check(Rooms.Num() > 0);
	return Rooms[0];
}
const FDCJobDef* FDCSimData::FindJob(FName Id) const { return Jobs.FindByPredicate([Id](const FDCJobDef& J) { return J.Id == Id; }); }
const FDCGearDef* FDCSimData::FindGear(FName Id) const { return Gear.FindByPredicate([Id](const FDCGearDef& G) { return G.Item.Id == Id; }); }
const FDCPerkDef* FDCSimData::FindPerk(FName Id) const { return Perks.FindByPredicate([Id](const FDCPerkDef& P) { return P.Perk.Id == Id; }); }

TSharedRef<FDCSimData> FDCSimData::Load(const FString& DataDirOverride)
{
	TSharedRef<FDCSimData> D = MakeShared<FDCSimData>();
	TArray<FString>& W = D->LoadWarnings;
	const FString Dir = DataDirOverride.IsEmpty() ? DefaultDataDir() : DataDirOverride;

	// ── balance.json ──
	{
		const FObj O = LoadJsonFile(Dir, TEXT("balance.json"), W);
		FDCBalance& B = D->Balance;
		B.StartingCash = Int(O, TEXT("startingCash"), B.StartingCash);
		B.StartingRoster = Int(O, TEXT("startingRoster"), B.StartingRoster);
		B.StartingBoard = Int(O, TEXT("startingBoard"), B.StartingBoard);
		B.RecruitBoardSize = Int(O, TEXT("recruitBoardSize"), B.RecruitBoardSize);
		B.RecruitRefreshNights = FMath::Max(1, Int(O, TEXT("recruitRefreshNights"), B.RecruitRefreshNights));
		B.CutMin = Num(O, TEXT("cutMin"), B.CutMin);
		B.CutMax = Num(O, TEXT("cutMax"), B.CutMax);
		B.CutStep = Num(O, TEXT("cutStep"), B.CutStep);
		B.DefaultCut = Num(O, TEXT("defaultCut"), B.DefaultCut);
		B.SupportWeight = Num(O, TEXT("supportWeight"), B.SupportWeight);
		B.DifficultyPerHeist = Num(O, TEXT("difficultyPerHeist"), B.DifficultyPerHeist);
		B.DifficultyPerTier = Num(O, TEXT("difficultyPerTier"), B.DifficultyPerTier);
		B.SkillBase = Num(O, TEXT("skillBase"), B.SkillBase);
		B.SkillSlope = Num(O, TEXT("skillSlope"), B.SkillSlope);
		B.SkillMin = Num(O, TEXT("skillMin"), B.SkillMin);
		B.SkillMax = Num(O, TEXT("skillMax"), B.SkillMax);
		B.RoleFitPer = Num(O, TEXT("roleFitPer"), B.RoleFitPer);
		B.RoleFitCap = Num(O, TEXT("roleFitCap"), B.RoleFitCap);
		B.FairCutBase = Num(O, TEXT("fairCutBase"), B.FairCutBase);
		B.FairCutLoyaltyDiv = FMath::Max(1.f, Num(O, TEXT("fairCutLoyaltyDiv"), B.FairCutLoyaltyDiv));
		B.WillSlope = Num(O, TEXT("willSlope"), B.WillSlope);
		B.WillMin = Num(O, TEXT("willMin"), B.WillMin);
		B.RefusalThreshold = Num(O, TEXT("refusalThreshold"), B.RefusalThreshold);
		B.WillFactorBase = Num(O, TEXT("willFactorBase"), B.WillFactorBase);
		B.WillFactorScale = Num(O, TEXT("willFactorScale"), B.WillFactorScale);
		B.GenerosityBonus = Num(O, TEXT("generosityBonus"), B.GenerosityBonus);
		B.SuccessMin = Num(O, TEXT("successMin"), B.SuccessMin);
		B.SuccessMax = Num(O, TEXT("successMax"), B.SuccessMax);
		B.LuckyBreakPerLuck = Num(O, TEXT("luckyBreakPerLuck"), B.LuckyBreakPerLuck);
		B.CleanBand = Num(O, TEXT("cleanBand"), B.CleanBand);
		B.BustedBand = Num(O, TEXT("bustedBand"), B.BustedBand);
		B.MessyInjuryChance = Num(O, TEXT("messyInjuryChance"), B.MessyInjuryChance);
		B.MessyInjuryNights = Range(O, TEXT("messyInjuryNights"), B.MessyInjuryNights);
		B.FailedInjuryChance = Num(O, TEXT("failedInjuryChance"), B.FailedInjuryChance);
		B.FailedInjuryNights = Range(O, TEXT("failedInjuryNights"), B.FailedInjuryNights);
		B.BustedArrestMult = Num(O, TEXT("bustedArrestMult"), B.BustedArrestMult);
		const FObj XP = Obj(O, TEXT("xpOutcomeMult"));
		B.XPMultClean = Num(XP, TEXT("Clean"), B.XPMultClean);
		B.XPMultMessy = Num(XP, TEXT("Messy"), B.XPMultMessy);
		B.XPMultFailed = Num(XP, TEXT("Failed"), B.XPMultFailed);
		B.XPMultBusted = Num(XP, TEXT("Busted"), B.XPMultBusted);
		B.LevelCurveBase = Num(O, TEXT("levelCurveBase"), B.LevelCurveBase);
		B.LevelCurveExp = Num(O, TEXT("levelCurveExp"), B.LevelCurveExp);
		B.MaxLevel = FMath::Max(1, Int(O, TEXT("maxLevel"), B.MaxLevel));
		const FObj LU = Obj(O, TEXT("levelUp"));
		B.LevelUpPrimary = Int(LU, TEXT("primary"), B.LevelUpPrimary);
		B.LevelUpSecondary = Int(LU, TEXT("secondary"), B.LevelUpSecondary);
		B.LevelUpOther = Int(LU, TEXT("other"), B.LevelUpOther);
		B.LuckEveryNLevels = FMath::Max(1, Int(LU, TEXT("luckEveryNLevels"), B.LuckEveryNLevels));
		const FObj LO = Obj(O, TEXT("loyalty"));
		B.LoyaltySuccessFair = Int(LO, TEXT("successFair"), B.LoyaltySuccessFair);
		B.LoyaltyGreedy = Int(LO, TEXT("greedy"), B.LoyaltyGreedy);
		B.LoyaltyInjury = Int(LO, TEXT("injury"), B.LoyaltyInjury);
		B.LoyaltyMin = Int(LO, TEXT("min"), B.LoyaltyMin);
		B.LoyaltyMax = Int(LO, TEXT("max"), B.LoyaltyMax);
		B.TierUnlockHeists = IntArr(O, TEXT("tierUnlockHeists"), B.TierUnlockHeists);
		B.TierPayoutMult = Num(O, TEXT("tierPayoutMult"), B.TierPayoutMult);
		B.UpkeepPerCriminal = Int(O, TEXT("upkeepPerCriminal"), B.UpkeepPerCriminal);
		B.UpkeepPerHeistBlock = Int(O, TEXT("upkeepPerHeistBlock"), B.UpkeepPerHeistBlock);
		B.UpkeepHeistBlock = FMath::Max(1, Int(O, TEXT("upkeepHeistBlock"), B.UpkeepHeistBlock));
		B.ArrestRiskPer5Heists = Num(O, TEXT("arrestRiskPer5Heists"), B.ArrestRiskPer5Heists);
		B.RecruitCostTierMult = Num(O, TEXT("recruitCostTierMult"), B.RecruitCostTierMult);
		B.RecruitStatBonusPerTier = Int(O, TEXT("recruitStatBonusPerTier"), B.RecruitStatBonusPerTier);
		B.PerkEveryNHeists = FMath::Max(1, Int(O, TEXT("perkEveryNHeists"), B.PerkEveryNHeists));
		B.PerkOfferSize = FMath::Max(1, Int(O, TEXT("perkOfferSize"), B.PerkOfferSize));
		B.BaseRosterCap = Int(O, TEXT("baseRosterCap"), B.BaseRosterCap);
		B.BaseCrewsPerNight = Int(O, TEXT("baseCrewsPerNight"), B.BaseCrewsPerNight);
		B.BaseGearSlots = Int(O, TEXT("baseGearSlots"), B.BaseGearSlots);
		B.BaseJobBoard = Int(O, TEXT("baseJobBoard"), B.BaseJobBoard);
		B.FinalCutRequiredHeists = Int(Obj(O, TEXT("finalCut")), TEXT("requiredHeists"), B.FinalCutRequiredHeists);
	}

	// ── criminal_types.json ──
	{
		const FObj O = LoadJsonFile(Dir, TEXT("criminal_types.json"), W);
		if (const auto* A = Arr(O, TEXT("types")))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TSharedPtr<FJsonObject>* TO = nullptr;
				if (!V.IsValid() || !V->TryGetObject(TO) || !TO) { continue; }
				const FObj& T = *TO;
				EDCCriminalType Type;
				if (!DCSim::ParseType(Str(T, TEXT("type")), Type)) { W.Add(FString::Printf(TEXT("criminal_types: unknown type '%s'"), *Str(T, TEXT("type")))); continue; }
				FDCTypeDef Def = DefaultType(Type);
				Def.DisplayName = Str(T, TEXT("displayName"), Def.DisplayName);
				Def.Fantasy = Str(T, TEXT("fantasy"));
				DCSim::ParseStat(Str(T, TEXT("primary")), Def.Primary);
				DCSim::ParseStat(Str(T, TEXT("secondary")), Def.Secondary);
				Def.PrimaryRange = Range(T, TEXT("primaryRange"), Def.PrimaryRange);
				Def.SecondaryRange = Range(T, TEXT("secondaryRange"), Def.SecondaryRange);
				Def.OtherRange = Range(T, TEXT("otherRange"), Def.OtherRange);
				Def.LuckRange = Range(T, TEXT("luckRange"), Def.LuckRange);
				Def.LoyaltyRange = Range(T, TEXT("loyaltyRange"), Def.LoyaltyRange);
				Def.HireCostBase = Int(T, TEXT("hireCostBase"), Def.HireCostBase);
				Def.Props = Str(T, TEXT("props"));
				D->Types.RemoveAll([Type](const FDCTypeDef& X) { return X.Type == Type; });
				D->Types.Add(Def);
			}
		}
		for (int32 i = 0; i < (int32)EDCCriminalType::MAX; ++i)
		{
			if (!D->Types.ContainsByPredicate([i](const FDCTypeDef& X) { return (int32)X.Type == i; }))
			{
				W.Add(FString::Printf(TEXT("criminal_types: '%s' missing; using built-in default"), DC::TypeName((EDCCriminalType)i)));
				D->Types.Add(DefaultType((EDCCriminalType)i));
			}
		}
		D->Types.Sort([](const FDCTypeDef& A, const FDCTypeDef& B) { return A.Type < B.Type; });
	}

	// ── names.json ──
	{
		const FObj O = LoadJsonFile(Dir, TEXT("names.json"), W);
		D->Names.First = StrArr(O, TEXT("first"));
		D->Names.Last = StrArr(O, TEXT("last"));
		D->Names.Nicknames = StrArr(O, TEXT("nicknames"));
		if (const FObj TN = Obj(O, TEXT("typeNicknames")))
		{
			for (const auto& Pair : TN->Values)
			{
				EDCCriminalType T;
				if (!DCSim::ParseType(FString(*Pair.Key), T)) { continue; }
				TArray<FString> L;
				const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
				if (Pair.Value.IsValid() && Pair.Value->TryGetArray(A) && A)
				{
					for (const TSharedPtr<FJsonValue>& V : *A) { FString S; if (V->TryGetString(S)) { L.Add(S); } }
				}
				if (L.Num()) { D->Names.TypeNicknames.Add(T, L); }
			}
		}
		if (D->Names.First.Num() == 0) { D->Names.First = { TEXT("Vinnie"), TEXT("Rosa"), TEXT("Sal"), TEXT("Dolores") }; W.Add(TEXT("names: no first names")); }
		if (D->Names.Last.Num() == 0) { D->Names.Last = { TEXT("Caruso"), TEXT("Delgado"), TEXT("Moretti"), TEXT("Finch") }; W.Add(TEXT("names: no last names")); }
		if (D->Names.Nicknames.Num() == 0) { D->Names.Nicknames = { TEXT("The Wrench"), TEXT("Two-Tone"), TEXT("Whisper"), TEXT("Lucky") }; W.Add(TEXT("names: no nicknames")); }
	}

	// ── jobs.json ──
	{
		const FObj O = LoadJsonFile(Dir, TEXT("jobs.json"), W);
		if (const auto* A = Arr(O, TEXT("jobs")))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TSharedPtr<FJsonObject>* JO = nullptr;
				if (!V.IsValid() || !V->TryGetObject(JO) || !JO) { continue; }
				const FObj& J = *JO;
				FDCJobDef Def;
				Def.Id = FName(*Str(J, TEXT("id"), TEXT("job")));
				const FString Ctx = FString::Printf(TEXT("jobs/%s"), *Def.Id.ToString());
				Def.DisplayName = Str(J, TEXT("displayName"), Def.Id.ToString());
				Def.Tier = FMath::Max(1, Int(J, TEXT("tier"), 1));
				Def.RequiredRoles = TypeArr(J, TEXT("requiredRoles"), W, Ctx);
				Def.BonusRoles = TypeArr(J, TEXT("bonusRoles"), W, Ctx);
				Def.MinCrew = FMath::Max(1, Int(J, TEXT("minCrew"), FMath::Max(1, Def.RequiredRoles.Num())));
				Def.MinCrew = FMath::Max(Def.MinCrew, Def.RequiredRoles.Num());
				Def.MaxCrew = FMath::Max(Def.MinCrew, Int(J, TEXT("maxCrew"), Def.MinCrew + 1));
				float WSum = 0.f;
				if (const FObj SW = Obj(J, TEXT("statWeights")))
				{
					for (const auto& Pair : SW->Values)
					{
						EDCStat S; double Wt = 0;
						if (DCSim::ParseStat(FString(*Pair.Key), S) && Pair.Value.IsValid() && Pair.Value->TryGetNumber(Wt) && Wt > 0) { Def.StatWeights.Add(S, (float)Wt); WSum += (float)Wt; }
					}
				}
				if (Def.StatWeights.Num() == 0) { Def.StatWeights.Add(EDCStat::Muscle, 1.f); WSum = 1.f; W.Add(Ctx + TEXT(": no statWeights")); }
				if (FMath::Abs(WSum - 1.f) > 0.011f)
				{
					W.Add(FString::Printf(TEXT("%s: statWeights sum %.3f, normalising"), *Ctx, WSum));
					for (auto& Pair : Def.StatWeights) { Pair.Value /= WSum; }
				}
				Def.BaseDifficulty = FMath::Max(1.f, Num(J, TEXT("baseDifficulty"), Def.BaseDifficulty));
				Def.PayoutRange = Range(J, TEXT("payoutRange"), Def.PayoutRange);
				Def.XPReward = Int(J, TEXT("xpReward"), Def.XPReward);
				Def.ArrestRisk = FMath::Clamp(Num(J, TEXT("arrestRisk"), Def.ArrestRisk), 0.f, 1.f);
				Def.bHeavy = Bool(J, TEXT("heavy"), false);
				Def.bFinalCut = Bool(J, TEXT("finalCut"), false);
				Def.Flavour = StrArr(J, TEXT("flavour"));
				D->Jobs.Add(Def);
			}
		}
		if (D->Jobs.Num() == 0) { W.Add(TEXT("jobs: no jobs loaded")); }
	}

	// ── gear.json ──
	{
		const FObj O = LoadJsonFile(Dir, TEXT("gear.json"), W);
		if (const auto* A = Arr(O, TEXT("gear")))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TSharedPtr<FJsonObject>* GO = nullptr;
				if (!V.IsValid() || !V->TryGetObject(GO) || !GO) { continue; }
				const FObj& G = *GO;
				FDCGearDef Def;
				Def.Item.Id = FName(*Str(G, TEXT("id"), TEXT("gear")));
				Def.Item.DisplayName = Str(G, TEXT("displayName"), Def.Item.Id.ToString());
				Def.Item.Description = Str(G, TEXT("description"));
				if (!DCSim::ParseSlot(Str(G, TEXT("slot"), TEXT("Tool")), Def.Item.Slot)) { W.Add(FString::Printf(TEXT("gear/%s: bad slot"), *Def.Item.Id.ToString())); }
				Def.Item.Price = FMath::Max(0, Int(G, TEXT("price"), 1000));
				Def.Item.Tier = FMath::Max(1, Int(G, TEXT("tier"), 1));
				if (const FObj S = Obj(G, TEXT("stats")))
				{
					for (const auto& Pair : S->Values)
					{
						EDCStat St; double Amt = 0;
						if (DCSim::ParseStat(FString(*Pair.Key), St) && Pair.Value.IsValid() && Pair.Value->TryGetNumber(Amt)) { Def.Item.StatBonus.Set(St, FMath::RoundToInt((float)Amt)); }
					}
				}
				Def.Item.LuckBonus = Int(G, TEXT("luck"), 0);
				Def.Item.ArrestRiskMultiplier = FMath::Max(0.f, Num(G, TEXT("arrestRiskMult"), 1.f));
				const FString Aff = Str(G, TEXT("typeAffinity"));
				Def.bHasAffinity = !Aff.IsEmpty() && DCSim::ParseType(Aff, Def.Affinity);
				D->Gear.Add(Def);
			}
		}
		if (D->Gear.Num() == 0) { W.Add(TEXT("gear: no gear loaded")); }
	}

	// ── rooms.json ──
	{
		const FObj O = LoadJsonFile(Dir, TEXT("rooms.json"), W);
		if (const auto* A = Arr(O, TEXT("rooms")))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TSharedPtr<FJsonObject>* RO = nullptr;
				if (!V.IsValid() || !V->TryGetObject(RO) || !RO) { continue; }
				const FObj& R = *RO;
				EDCRoomType Type;
				if (!DCSim::ParseRoom(Str(R, TEXT("type")), Type)) { W.Add(FString::Printf(TEXT("rooms: unknown room '%s'"), *Str(R, TEXT("type")))); continue; }
				FDCRoomDef Def = DefaultRoom(Type);
				const FString Ctx = FString::Printf(TEXT("rooms/%s"), *Str(R, TEXT("type")));
				Def.DisplayName = Str(R, TEXT("displayName"), Def.DisplayName);
				Def.Priority = Str(R, TEXT("priority"));
				Def.Description = Str(R, TEXT("description"));
				Def.BuildCost = Int(R, TEXT("buildCost"), Def.BuildCost);
				Def.UpgradeCosts = IntArr(R, TEXT("upgradeCosts"), Def.UpgradeCosts);
				while (Def.UpgradeCosts.Num() < 2) { Def.UpgradeCosts.Add(Def.UpgradeCosts.Num() ? Def.UpgradeCosts.Last() : 10000); }
				Def.RequiredHeists = Int(R, TEXT("requiredHeists"), 0);
				Def.UpkeepPerNight = Int(R, TEXT("upkeepPerNight"), Def.UpkeepPerNight);
				Def.Levels.Reset();
				if (const auto* LA = Arr(R, TEXT("levels")))
				{
					for (const TSharedPtr<FJsonValue>& LV : *LA)
					{
						const TSharedPtr<FJsonObject>* LO = nullptr;
						if (!LV.IsValid() || !LV->TryGetObject(LO) || !LO) { continue; }
						FDCRoomLevelDef L;
						L.EffectText = Str(*LO, TEXT("effectText"));
						L.Flavour = Str(*LO, TEXT("flavour"));
						L.Modifiers = ParseModifiers(*LO, W, FString::Printf(TEXT("%s/L%d"), *Ctx, Def.Levels.Num() + 1));
						Def.Levels.Add(L);
					}
				}
				while (Def.Levels.Num() < 3) { Def.Levels.Add(Def.Levels.Num() ? Def.Levels.Last() : FDCRoomLevelDef()); }
				D->Rooms.RemoveAll([Type](const FDCRoomDef& X) { return X.Type == Type; });
				D->Rooms.Add(Def);
			}
		}
		for (int32 i = 0; i < (int32)EDCRoomType::MAX; ++i)
		{
			if (!D->Rooms.ContainsByPredicate([i](const FDCRoomDef& X) { return (int32)X.Type == i; }))
			{
				W.Add(FString::Printf(TEXT("rooms: '%s' missing; using built-in default"), DC::RoomName((EDCRoomType)i)));
				D->Rooms.Add(DefaultRoom((EDCRoomType)i));
			}
		}
		D->Rooms.Sort([](const FDCRoomDef& A, const FDCRoomDef& B) { return A.Type < B.Type; });
	}

	// ── perks.json ──
	{
		const FObj O = LoadJsonFile(Dir, TEXT("perks.json"), W);
		if (const auto* A = Arr(O, TEXT("perks")))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TSharedPtr<FJsonObject>* PO = nullptr;
				if (!V.IsValid() || !V->TryGetObject(PO) || !PO) { continue; }
				const FObj& P = *PO;
				FDCPerkDef Def;
				Def.Perk.Id = FName(*Str(P, TEXT("id"), TEXT("perk")));
				Def.Perk.DisplayName = Str(P, TEXT("displayName"), Def.Perk.Id.ToString());
				Def.Perk.Description = Str(P, TEXT("description"));
				Def.Perk.Flavour = Str(P, TEXT("flavour"));
				Def.Modifiers = ParseModifiers(P, W, FString::Printf(TEXT("perks/%s"), *Def.Perk.Id.ToString()));
				D->Perks.Add(Def);
			}
		}
	}

	// ── strings.json ──
	{
		const FObj O = LoadJsonFile(Dir, TEXT("strings.json"), W);
		FDCStringsDef& S = D->Strings;
		static const TPair<EDCHeistOutcome, const TCHAR*> Outs[] = {
			{ EDCHeistOutcome::Clean, TEXT("Clean") }, { EDCHeistOutcome::Messy, TEXT("Messy") },
			{ EDCHeistOutcome::Failed, TEXT("Failed") }, { EDCHeistOutcome::Busted, TEXT("Busted") } };
		const FObj Stamps = Obj(O, TEXT("stamps"));
		const FObj Lines = Obj(O, TEXT("outcomeLines"));
		for (const auto& P : Outs)
		{
			S.Stamps.Add(P.Key, Str(Stamps, P.Value, DC::OutcomeStamp(P.Key)));
			TArray<FString> L = StrArr(Lines, P.Value);
			if (L.Num() == 0)
			{
				switch (P.Key)
				{
				case EDCHeistOutcome::Clean: L.Add(TEXT("{name} got out clean.")); break;
				case EDCHeistOutcome::Messy: L.Add(TEXT("{name} made it out, but it wasn't pretty.")); break;
				case EDCHeistOutcome::Failed: L.Add(TEXT("{name} came back empty-handed.")); break;
				default: L.Add(TEXT("Sirens. {name} never stood a chance.")); break;
				}
			}
			S.OutcomeLines.Add(P.Key, L);
		}
		S.LuckyBreak = StrArr(O, TEXT("luckyBreak"));
		if (S.LuckyBreak.Num() == 0) { S.LuckyBreak.Add(TEXT("Lucky break! {name} found the back door unlocked.")); }
		const FObj GO = Obj(O, TEXT("gameOverReasons"));
		S.GameOverBroke = Str(GO, TEXT("Broke"), S.GameOverBroke);
		S.GameOverNoCrew = Str(GO, TEXT("NoCrew"), S.GameOverNoCrew);
		S.WinReason = Str(O, TEXT("winReason"), S.WinReason);
		const FObj NE = Obj(O, TEXT("nightEvents"));
		S.TierUnlocked = Str(NE, TEXT("tierUnlocked"), S.TierUnlocked);
		S.RecruitsRefreshed = Str(NE, TEXT("recruitsRefreshed"), S.RecruitsRefreshed);
		static const TPair<const TCHAR*, const TCHAR*> EventDefaults[] = {
			{ TEXT("tierUnlocked"), TEXT("Tier {tier} productions unlocked!") },
			{ TEXT("recruitsRefreshed"), TEXT("New faces at the desk.") },
			{ TEXT("upkeepPaid"), TEXT("Paid {amount} in upkeep.") },
			{ TEXT("levelUp"), TEXT("{name} has grown as an artist.") },
			{ TEXT("injured"), TEXT("{name} is in the makeup chair for a while.") },
			{ TEXT("healed"), TEXT("{name} is back on their feet.") },
			{ TEXT("arrested"), TEXT("{name} has been written out of the show.") },
			{ TEXT("arrestPrevented"), TEXT("A stunt double took the fall for {name}.") },
			{ TEXT("bailed"), TEXT("{name} walked out of the precinct with a lawyer.") },
			{ TEXT("retake"), TEXT("Retake! Back to one, everybody.") },
			{ TEXT("roomBuilt"), TEXT("Lights up on {name}!") },
			{ TEXT("perkOffer"), TEXT("The studio is impressed. Pick a perk.") },
			{ TEXT("finalCutUnlocked"), TEXT("The Premiere is on the board. This is the big one.") },
			{ TEXT("idleLoyalty"), TEXT("The Lounge: {amount} idle crew feel more loyal.") },
		};
		for (const auto& E : EventDefaults) { S.NightEvents.Add(E.Key, E.Value); }
		if (NE.IsValid())
		{
			for (const auto& Pair : NE->Values) { FString V; if (Pair.Value.IsValid() && Pair.Value->TryGetString(V)) { S.NightEvents.Add(FString(*Pair.Key), V); } }
		}
	}

	for (const FString& Msg : W) { UE_LOG(LogDC, Warning, TEXT("[Sim] Data: %s"), *Msg); }
	UE_LOG(LogDC, Log, TEXT("[Sim] Data loaded from %s: %d types, %d jobs, %d gear, %d rooms, %d perks (%d warnings)"),
		*Dir, D->Types.Num(), D->Jobs.Num(), D->Gear.Num(), D->Rooms.Num(), D->Perks.Num(), W.Num());
	return D;
}
