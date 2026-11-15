// Director's Cut — JSON data loader for schema v2 (Agent A).
#include "Sim/DCSimData.h"
#include "DirectorsCut.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"

namespace
{
	using FObj = TSharedPtr<FJsonObject>;
	using FArr = TArray<TSharedPtr<FJsonValue>>;

	struct FCtx
	{
		FDCSimData& D;
		FString File;
		void Warn(const FString& Msg) { ++D.Warnings; UE_LOG(LogDC, Warning, TEXT("[Data] %s: %s"), *File, *Msg); }
	};

	template <typename E>
	bool ParseEnumByName(const FString& S, E& Out)
	{
		const UEnum* En = StaticEnum<E>();
		const int64 V = En ? En->GetValueByNameString(S) : INDEX_NONE;
		if (V == INDEX_NONE || V >= (int64)E::MAX) { return false; }
		Out = (E)V;
		return true;
	}

	bool Has(const FObj& O, const TCHAR* K) { return O.IsValid() && O->HasField(K); }

	float Num(FCtx& C, const FObj& O, const TCHAR* K, float Def, bool bWarn = true)
	{
		double V = 0.0;
		if (O.IsValid() && O->TryGetNumberField(K, V)) { return (float)V; }
		if (bWarn) { C.Warn(FString::Printf(TEXT("missing '%s', default %g"), K, Def)); }
		return Def;
	}
	int32 Int(FCtx& C, const FObj& O, const TCHAR* K, int32 Def, bool bWarn = true) { return FMath::RoundToInt(Num(C, O, K, (float)Def, bWarn)); }
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
		const FObj* V = nullptr;
		return (O.IsValid() && O->TryGetObjectField(K, V) && V) ? *V : FObj();
	}
	const FArr* Arr(const FObj& O, const TCHAR* K)
	{
		const FArr* V = nullptr;
		return (O.IsValid() && O->TryGetArrayField(K, V)) ? V : nullptr;
	}
	TArray<FString> StrArr(const FArr* A)
	{
		TArray<FString> Out;
		if (A) { for (const TSharedPtr<FJsonValue>& V : *A) { FString S; if (V.IsValid() && V->TryGetString(S)) { Out.Add(S); } } }
		return Out;
	}
	TArray<FString> StrArr(const FObj& O, const TCHAR* K) { return StrArr(Arr(O, K)); }
	TArray<float> NumArr(const FArr* A)
	{
		TArray<float> Out;
		if (A) { for (const TSharedPtr<FJsonValue>& V : *A) { double N = 0; if (V.IsValid() && V->TryGetNumber(N)) { Out.Add((float)N); } } }
		return Out;
	}
	TArray<float> NumArr(const FObj& O, const TCHAR* K) { return NumArr(Arr(O, K)); }
	TArray<int32> IntArr(const FObj& O, const TCHAR* K)
	{
		TArray<int32> Out;
		for (float F : NumArr(O, K)) { Out.Add(FMath::RoundToInt(F)); }
		return Out;
	}
	TArray<TArray<float>> NumArr2(const FObj& O, const TCHAR* K)
	{
		TArray<TArray<float>> Out;
		if (const FArr* A = Arr(O, K))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const FArr* Inner = nullptr;
				if (V.IsValid() && V->TryGetArray(Inner)) { Out.Add(NumArr(Inner)); }
			}
		}
		return Out;
	}
	FVector2D Range(FCtx& C, const FObj& O, const TCHAR* K, FVector2D Def)
	{
		const TArray<float> A = NumArr(O, K);
		if (A.Num() >= 2) { return FVector2D(A[0], A[1]); }
		if (A.Num() == 1) { return FVector2D(A[0], A[0]); }
		if (O.IsValid()) { double N = 0; if (O->TryGetNumberField(K, N)) { return FVector2D(N, N); } }
		C.Warn(FString::Printf(TEXT("missing range '%s'"), K));
		return Def;
	}
	FIntPoint IRange(FCtx& C, const FObj& O, const TCHAR* K, FIntPoint Def)
	{
		const FVector2D V = Range(C, O, K, FVector2D(Def.X, Def.Y));
		return FIntPoint(FMath::RoundToInt(V.X), FMath::RoundToInt(V.Y));
	}

	FObj ReadJson(FCtx& C, const FString& Path)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path)) { C.Warn(TEXT("file not found")); return FObj(); }
		FObj Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { C.Warn(TEXT("JSON parse error")); return FObj(); }
		return Root;
	}

	TArray<FDCEffect> ParseEffects(FCtx& C, const FArr* A)
	{
		static const TSet<FName> Known = {
			"chance", "chanceNight", "chanceDay", "chanceShort", "chanceLong", "chanceSolo", "chanceThreat", "chanceWithType",
			"chanceTierMin", "chanceIfType", "durationMult", "payoutMult", "costMult", "xpMult", "injuryChanceMult", "injuryTimeMult",
			"arrestMult", "fairCutAdd", "willMin", "loyaltyIdlePerDay", "rosterCap", "maxActive", "boardSize", "postIntervalMult",
			"gearSlots", "gearTier", "gearPriceMult", "upkeepMult", "recruitLevelAdd", "recruitQualityAdd", "recruitBoardAdd", "preventFirstArrest",
			"patchUp", "bail", "unlockTier", "unlockFinal", "lootChanceAdd" };
		TArray<FDCEffect> Out;
		if (!A) { return Out; }
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const FObj* EO = nullptr;
			if (!V.IsValid() || !V->TryGetObject(EO) || !EO) { continue; }
			FDCEffect E;
			E.Kind = FName(*Str(*EO, TEXT("kind")));
			if (!Known.Contains(E.Kind)) { C.Warn(FString::Printf(TEXT("unknown effect kind '%s' (ignored)"), *E.Kind.ToString())); continue; }
			E.Value = Num(C, *EO, TEXT("value"), 0.f);
			E.Param = Str(*EO, TEXT("param"));
			E.Threshold = Num(C, *EO, TEXT("threshold"), 0.f, false);
			Out.Add(E);
		}
		return Out;
	}

	FString PickFile(const FString& Dir, const TCHAR* Name)
	{
		const FString P = FPaths::Combine(Dir, Name);
		if (FPaths::FileExists(P)) { return P; }
		const FString F = FPaths::Combine(FDCSimData::FixturesDir(), Name);
		if (FPaths::FileExists(F))
		{
			UE_LOG(LogDC, Warning, TEXT("[Data] %s missing in %s, using fixture"), Name, *Dir);
			return F;
		}
		return P;
	}
}

bool FDCSimData::ParseThreat(const FString& S, EDCThreat& Out) { return ParseEnumByName(S, Out); }
bool FDCSimData::ParseType(const FString& S, EDCCriminalType& Out) { return ParseEnumByName(S, Out); }
bool FDCSimData::ParseSlot(const FString& S, EDCGearSlot& Out) { return ParseEnumByName(S, Out); }
bool FDCSimData::ParseRoom(const FString& S, EDCRoomType& Out) { return ParseEnumByName(S, Out); }

FString FDCSimData::FixturesDir() { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::GameSourceDir(), TEXT("DirectorsCut/Tests/Fixtures"))); }
FString FDCSimData::ContentDataDir() { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"))); }

bool FDCSimData::IsV2Dir(const FString& Dir)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(Dir, TEXT("missions.json")))) { return false; }
	FObj Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { return false; }
	const FArr* A = nullptr;
	return Root->TryGetArrayField(TEXT("missions"), A) && A && A->Num() > 0;
}

const FDCTypeDef* FDCSimData::FindType(EDCCriminalType T) const { return Types.FindByPredicate([T](const FDCTypeDef& X) { return X.Type == T; }); }
const FDCMissionTemplate* FDCSimData::FindTemplate(FName Id) const { return Missions.FindByPredicate([Id](const FDCMissionTemplate& X) { return X.Id == Id; }); }
const FDCRoomDef* FDCSimData::FindRoom(EDCRoomType T) const { return Rooms.FindByPredicate([T](const FDCRoomDef& X) { return X.Type == T; }); }

bool FDCSimData::Load(const FString& DirOverride)
{
	*this = FDCSimData();
	FString Dir = DirOverride;
	if (Dir.IsEmpty())
	{
		Dir = ContentDataDir();
		if (!IsV2Dir(Dir))
		{
			UE_LOG(LogDC, Display, TEXT("[Data] %s is missing or v1; using fixtures"), *Dir);
			Dir = FixturesDir();
		}
	}
	SourceDir = Dir;
	FCtx C{ *this, TEXT("") };

	// ── balance.json ──
	{
		C.File = TEXT("balance.json");
		const FObj R = ReadJson(C, PickFile(Dir, TEXT("balance.json")));
		FDCBalance& B = Balance;
		B.StartingCash = Int(C, R, TEXT("startingCash"), B.StartingCash);
		B.StartingRoster = Int(C, R, TEXT("startingRoster"), B.StartingRoster);
		B.StartingRosterLevels = IRange(C, R, TEXT("startingRosterLevels"), B.StartingRosterLevels);
		const FObj Clk = Obj(R, TEXT("clock"));
		B.StartHour = Num(C, Clk, TEXT("startHour"), B.StartHour);
		B.DawnHour = Num(C, Clk, TEXT("dawnHour"), B.DawnHour);
		B.DuskHour = Num(C, Clk, TEXT("duskHour"), B.DuskHour);
		B.GameMinutesPerSecond = FMath::Max(0.001f, Num(C, Clk, TEXT("gameMinutesPerSecond"), B.GameMinutesPerSecond));
		B.AutosaveSeconds = Num(C, R, TEXT("autosaveSeconds"), B.AutosaveSeconds);

		const FObj Bo = Obj(R, TEXT("board"));
		B.BoardBaseSize = Int(C, Bo, TEXT("baseSize"), B.BoardBaseSize);
		B.PostIntervalSec = FMath::Max(1.f, Num(C, Bo, TEXT("postIntervalSec"), B.PostIntervalSec));
		B.ExpireSecRange = Range(C, Bo, TEXT("expireSecRange"), B.ExpireSecRange);
		B.TierWeights = NumArr2(Bo, TEXT("tierWeights"));
		if (B.TierWeights.Num() == 0) { C.Warn(TEXT("missing board.tierWeights")); B.TierWeights = { { 1, 0, 0, 0 }, { 0.5f, 0.5f, 0, 0 }, { 0.3f, 0.4f, 0.3f, 0 }, { 0.2f, 0.3f, 0.3f, 0.2f } }; }
		B.RareChance = Num(C, Bo, TEXT("rareChance"), B.RareChance);
		B.RarePayoutMult = FMath::Max(0.f, Num(C, Bo, TEXT("rarePayoutMult"), B.RarePayoutMult, false));
		B.RareLevelAdd = Int(C, Bo, TEXT("rareLevelAdd"), B.RareLevelAdd, false);
		B.RareExtraThreats = FMath::Max(0, Int(C, Bo, TEXT("rareExtraThreats"), B.RareExtraThreats, false));
		for (const FString& S : StrArr(Bo, TEXT("startingBoard"))) { B.StartingBoard.Add(FName(*S)); }

		const FObj Ti = Obj(R, TEXT("tiers"));
		if (Has(Ti, TEXT("unlockSuccesses"))) { B.UnlockSuccesses = IntArr(Ti, TEXT("unlockSuccesses")); } else { C.Warn(TEXT("missing tiers.unlockSuccesses")); }
		const TArray<TArray<float>> LR = NumArr2(Ti, TEXT("levelRange"));
		if (LR.Num() > 0) { B.LevelRange.Reset(); for (const TArray<float>& P : LR) { B.LevelRange.Add(FIntPoint(P.Num() > 0 ? (int32)P[0] : 1, P.Num() > 1 ? (int32)P[1] : (P.Num() > 0 ? (int32)P[0] : 1))); } }
		else { C.Warn(TEXT("missing tiers.levelRange")); }

		const FObj Su = Obj(R, TEXT("success"));
		B.ThreatPoints = Num(C, Su, TEXT("threatPoints"), B.ThreatPoints);
		const FObj LF = Obj(Su, TEXT("levelFactor"));
		B.LFAtOrAbove = Num(C, LF, TEXT("atOrAbove"), B.LFAtOrAbove);
		B.LFPerLevelAbove = Num(C, LF, TEXT("perLevelAbove"), B.LFPerLevelAbove);
		B.LFMaxFactor = Num(C, LF, TEXT("maxFactor"), B.LFMaxFactor);
		if (Has(LF, TEXT("below"))) { B.LFBelow = NumArr(LF, TEXT("below")); } else { C.Warn(TEXT("missing levelFactor.below")); }
		while (B.LFBelow.Num() < 3) { B.LFBelow.Add(0.1f); }
		B.MaxChance = Num(C, Su, TEXT("maxChance"), B.MaxChance);
		B.BaseMult = Num(C, Su, TEXT("baseMult"), B.BaseMult, false);

		const FObj Cu = Obj(R, TEXT("cut"));
		B.CutMin = Num(C, Cu, TEXT("min"), B.CutMin);
		B.CutMax = Num(C, Cu, TEXT("max"), B.CutMax);
		B.CutStep = Num(C, Cu, TEXT("step"), B.CutStep);
		B.CutDefault = Num(C, Cu, TEXT("default"), B.CutDefault);
		B.FairCutBase = Num(C, Cu, TEXT("fairCutBase"), B.FairCutBase);
		B.FairCutLoyaltyDiv = FMath::Max(1.f, Num(C, Cu, TEXT("fairCutLoyaltyDiv"), B.FairCutLoyaltyDiv));
		B.WillSlope = Num(C, Cu, TEXT("willSlope"), B.WillSlope);
		B.WillMin = Num(C, Cu, TEXT("willMin"), B.WillMin);
		B.RefusalThreshold = Num(C, Cu, TEXT("refusalThreshold"), B.RefusalThreshold);
		B.WillFactorBase = Num(C, Cu, TEXT("willFactorBase"), B.WillFactorBase);
		B.WillFactorScale = Num(C, Cu, TEXT("willFactorScale"), B.WillFactorScale);

		const FObj Ou = Obj(R, TEXT("outcome"));
		B.BustedBand = Num(C, Ou, TEXT("bustedBand"), B.BustedBand);
		B.BonusPayoutMult = Num(C, Ou, TEXT("bonusPayoutMult"), B.BonusPayoutMult);
		B.FailXpMult = Num(C, Ou, TEXT("failXpMult"), B.FailXpMult);

		const FObj In = Obj(R, TEXT("injury"));
		B.InjuryFailChance = Num(C, In, TEXT("failChance"), B.InjuryFailChance);
		B.InjuryBustedChance = Num(C, In, TEXT("bustedChance"), B.InjuryBustedChance);
		B.InjurySecondsRange = Range(C, In, TEXT("secondsRange"), B.InjurySecondsRange);

		const FObj Lv = Obj(R, TEXT("levels"));
		B.MaxLevel = Int(C, Lv, TEXT("max"), B.MaxLevel);
		B.XPBase = Num(C, Lv, TEXT("xpBase"), B.XPBase);
		B.XPExp = Num(C, Lv, TEXT("xpExp"), B.XPExp);

		const FObj Lo = Obj(R, TEXT("loyalty"));
		B.LoyaltyStart = Int(C, Lo, TEXT("start"), B.LoyaltyStart);
		B.LoyaltySuccessFair = Int(C, Lo, TEXT("successFair"), B.LoyaltySuccessFair);
		B.LoyaltyGreedy = Int(C, Lo, TEXT("greedy"), B.LoyaltyGreedy);
		B.LoyaltyInjury = Int(C, Lo, TEXT("injury"), B.LoyaltyInjury);
		B.LoyaltyMin = Int(C, Lo, TEXT("min"), B.LoyaltyMin);
		B.LoyaltyMax = Int(C, Lo, TEXT("max"), B.LoyaltyMax);
		B.LoyaltyQuitBelow = Int(C, Lo, TEXT("quitBelow"), B.LoyaltyQuitBelow);

		B.UpkeepPerCriminal = Int(C, Obj(R, TEXT("upkeep")), TEXT("perCriminalPerDay"), B.UpkeepPerCriminal);

		const FObj Re = Obj(R, TEXT("recruits"));
		B.RecruitBoardSize = Int(C, Re, TEXT("boardSize"), B.RecruitBoardSize);
		B.RecruitRefreshSec = FMath::Max(1.f, Num(C, Re, TEXT("refreshSec"), B.RecruitRefreshSec));
		B.RecruitRefreshCost = Int(C, Re, TEXT("refreshCost"), B.RecruitRefreshCost);
		B.RecruitCostBase = Int(C, Re, TEXT("costBase"), B.RecruitCostBase);
		B.RecruitCostPerLevel = Int(C, Re, TEXT("costPerLevel"), B.RecruitCostPerLevel);
		if (Has(Re, TEXT("qualityCostMult"))) { B.QualityCostMult = NumArr(Re, TEXT("qualityCostMult")); } else { C.Warn(TEXT("missing recruits.qualityCostMult")); }
		while (B.QualityCostMult.Num() < 3) { B.QualityCostMult.Add(1.f); }
		B.QualityOdds = NumArr2(Re, TEXT("qualityOdds"));
		if (B.QualityOdds.Num() == 0) { C.Warn(TEXT("missing recruits.qualityOdds")); B.QualityOdds = { { 0.8f, 0.18f, 0.02f } }; }

		const FObj Qu = Obj(R, TEXT("quality"));
		if (Has(Qu, TEXT("abilities"))) { B.QualityAbilities = IntArr(Qu, TEXT("abilities")); } else { C.Warn(TEXT("missing quality.abilities")); }
		if (Has(Qu, TEXT("traits"))) { B.QualityTraits = IntArr(Qu, TEXT("traits")); } else { C.Warn(TEXT("missing quality.traits")); }
		if (Has(Qu, TEXT("negativeTraitChance"))) { B.NegativeTraitChance = NumArr(Qu, TEXT("negativeTraitChance")); } else { C.Warn(TEXT("missing quality.negativeTraitChance")); }
		while (B.QualityAbilities.Num() < 3) { B.QualityAbilities.Add(1); }
		while (B.QualityTraits.Num() < 3) { B.QualityTraits.Add(1); }
		while (B.NegativeTraitChance.Num() < 3) { B.NegativeTraitChance.Add(0.f); }

		const FObj Ge = Obj(R, TEXT("gear"));
		B.GearBaseSlots = Int(C, Ge, TEXT("baseSlots"), B.GearBaseSlots);
		B.SellRefund = Num(C, Ge, TEXT("sellRefund"), B.SellRefund);
		B.LootChance = Num(C, Ge, TEXT("lootChance"), B.LootChance);
		B.RareLootChance = Num(C, Ge, TEXT("rareLootChance"), B.RareLootChance);
		B.NightMissionBonus = Num(C, Obj(R, TEXT("night")), TEXT("missionBonus"), B.NightMissionBonus);
		B.MinDurationMult = Num(C, R, TEXT("minDurationMult"), B.MinDurationMult);
		const FObj Pe = Obj(R, TEXT("perks"));
		B.PerkEveryN = FMath::Max(1, Int(C, Pe, TEXT("everyNSuccesses"), B.PerkEveryN));
		B.PerkOfferSize = Int(C, Pe, TEXT("offerSize"), B.PerkOfferSize);
		const FObj Fi = Obj(R, TEXT("final"));
		B.FinalTemplateId = FName(*Str(Fi, TEXT("templateId"), B.FinalTemplateId.ToString()));
		B.FinalRequiredSuccesses = Int(C, Fi, TEXT("requiredSuccesses"), B.FinalRequiredSuccesses);
		B.CashFloor = Int(C, Obj(R, TEXT("gameOver")), TEXT("cashFloor"), B.CashFloor);
	}

	// ── abilities.json ──
	{
		C.File = TEXT("abilities.json");
		const FObj R = ReadJson(C, PickFile(Dir, TEXT("abilities.json")));
		if (const FArr* A = Arr(R, TEXT("abilities")))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const FObj* O = nullptr;
				if (!V.IsValid() || !V->TryGetObject(O)) { continue; }
				FDCAbility Ab;
				Ab.Id = FName(*Str(*O, TEXT("id")));
				Ab.DisplayName = Str(*O, TEXT("displayName"), Ab.Id.ToString());
				Ab.Icon = FName(*Str(*O, TEXT("icon"), TEXT("circle-help")));
				if (!ParseThreat(Str(*O, TEXT("counters")), Ab.Counters)) { C.Warn(FString::Printf(TEXT("%s: bad counters"), *Ab.Id.ToString())); }
				Abilities.Add(Ab.Id, Ab);
			}
		}
	}

	// ── traits.json ──
	{
		C.File = TEXT("traits.json");
		const FObj R = ReadJson(C, PickFile(Dir, TEXT("traits.json")));
		if (const FArr* A = Arr(R, TEXT("traits")))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const FObj* O = nullptr;
				if (!V.IsValid() || !V->TryGetObject(O)) { continue; }
				FDCTraitDef T;
				T.Trait.Id = FName(*Str(*O, TEXT("id")));
				T.Trait.DisplayName = Str(*O, TEXT("displayName"), T.Trait.Id.ToString());
				T.Trait.Description = Str(*O, TEXT("description"));
				T.Trait.Icon = FName(*Str(*O, TEXT("icon"), TEXT("circle-help")));
				T.Trait.bNegative = Bool(*O, TEXT("negative"), false);
				T.Effects = ParseEffects(C, Arr(*O, TEXT("effects")));
				Traits.Add(T.Trait.Id, T);
			}
		}
	}

	// ── perks.json ──
	{
		C.File = TEXT("perks.json");
		const FObj R = ReadJson(C, PickFile(Dir, TEXT("perks.json")));
		if (const FArr* A = Arr(R, TEXT("perks")))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const FObj* O = nullptr;
				if (!V.IsValid() || !V->TryGetObject(O)) { continue; }
				FDCPerkDef P;
				P.Perk.Id = FName(*Str(*O, TEXT("id")));
				P.Perk.DisplayName = Str(*O, TEXT("displayName"), P.Perk.Id.ToString());
				P.Perk.Description = Str(*O, TEXT("description"));
				P.Perk.Icon = FName(*Str(*O, TEXT("icon"), TEXT("sparkles")));
				P.Effects = ParseEffects(C, Arr(*O, TEXT("effects")));
				PerkOrder.Add(P.Perk.Id);
				Perks.Add(P.Perk.Id, P);
			}
		}
	}

	// ── gear.json ──
	{
		C.File = TEXT("gear.json");
		const FObj R = ReadJson(C, PickFile(Dir, TEXT("gear.json")));
		if (const FArr* A = Arr(R, TEXT("gear")))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const FObj* O = nullptr;
				if (!V.IsValid() || !V->TryGetObject(O)) { continue; }
				FDCGearDef G;
				FDCGearItem& I = G.Item;
				I.Id = FName(*Str(*O, TEXT("id")));
				I.DisplayName = Str(*O, TEXT("displayName"), I.Id.ToString());
				I.Description = Str(*O, TEXT("description"));
				I.Icon = FName(*Str(*O, TEXT("icon"), TEXT("package")));
				if (!ParseSlot(Str(*O, TEXT("slot")), I.Slot)) { C.Warn(FString::Printf(TEXT("%s: bad slot"), *I.Id.ToString())); }
				I.Price = Int(C, *O, TEXT("price"), 1000);
				I.Power = Num(C, *O, TEXT("power"), 0.f, false);
				I.Counters = EDCThreat::MAX;
				const FString Ctr = Str(*O, TEXT("counters"));
				if (!Ctr.IsEmpty() && !ParseThreat(Ctr, I.Counters)) { C.Warn(FString::Printf(TEXT("%s: bad counters"), *I.Id.ToString())); I.Counters = EDCThreat::MAX; }
				const FString Tr = Str(*O, TEXT("trait"));
				I.TraitId = Tr.IsEmpty() ? NAME_None : FName(*Tr);
				I.Tier = Int(C, *O, TEXT("tier"), 1);
				G.bLoot = Bool(*O, TEXT("loot"), false);
				GearOrder.Add(I.Id);
				Gear.Add(I.Id, G);
			}
		}
	}

	// ── criminal_types.json ──
	{
		C.File = TEXT("criminal_types.json");
		const FObj R = ReadJson(C, PickFile(Dir, TEXT("criminal_types.json")));
		if (const FArr* A = Arr(R, TEXT("types")))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const FObj* O = nullptr;
				if (!V.IsValid() || !V->TryGetObject(O)) { continue; }
				FDCTypeDef T;
				if (!ParseType(Str(*O, TEXT("type")), T.Type)) { C.Warn(TEXT("bad type")); continue; }
				for (const FString& S : StrArr(*O, TEXT("abilities"))) { if (Abilities.Contains(FName(*S))) { T.Abilities.Add(FName(*S)); } else { C.Warn(FString::Printf(TEXT("unknown ability %s"), *S)); } }
				for (const FString& S : StrArr(*O, TEXT("traitPool"))) { if (Traits.Contains(FName(*S))) { T.TraitPool.Add(FName(*S)); } else { C.Warn(FString::Printf(TEXT("unknown trait %s"), *S)); } }
				Types.Add(T);
			}
		}
		for (int32 i = 0; i < (int32)EDCCriminalType::MAX; ++i)
		{
			if (!FindType((EDCCriminalType)i)) { C.Warn(FString::Printf(TEXT("type %s missing"), DC::TypeName((EDCCriminalType)i))); FDCTypeDef T; T.Type = (EDCCriminalType)i; Types.Add(T); }
		}
	}

	// ── names.json ──
	{
		C.File = TEXT("names.json");
		const FObj R = ReadJson(C, PickFile(Dir, TEXT("names.json")));
		FirstNames = StrArr(R, TEXT("first"));
		MaleFirstNames = StrArr(R, TEXT("firstMale"));
		FemaleFirstNames = StrArr(R, TEXT("firstFemale"));
		LastNames = StrArr(R, TEXT("last"));
		if (FirstNames.Num() + MaleFirstNames.Num() + FemaleFirstNames.Num() == 0) { C.Warn(TEXT("no first names")); FirstNames = { TEXT("Alex") }; }
		if (LastNames.Num() == 0) { C.Warn(TEXT("no last names")); LastNames = { TEXT("Doe") }; }
		const FObj N = Obj(R, TEXT("nicknames"));
		for (int32 i = 0; i < (int32)EDCCriminalType::MAX; ++i)
		{
			TArray<FString> L = StrArr(N, DC::TypeName((EDCCriminalType)i));
			if (L.Num() == 0) { L.Add(DC::TypeName((EDCCriminalType)i)); }
			Nicknames.Add((EDCCriminalType)i, L);
		}
	}

	// ── missions.json ──
	{
		C.File = TEXT("missions.json");
		const FObj R = ReadJson(C, PickFile(Dir, TEXT("missions.json")));
		if (const FArr* A = Arr(R, TEXT("missions")))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const FObj* O = nullptr;
				if (!V.IsValid() || !V->TryGetObject(O)) { continue; }
				FDCMissionTemplate M;
				M.Id = FName(*Str(*O, TEXT("id")));
				M.DisplayName = Str(*O, TEXT("displayName"), M.Id.ToString());
				M.Icon = FName(*Str(*O, TEXT("icon"), TEXT("target")));
				M.Flavour = StrArr(*O, TEXT("flavour"));
				if (M.Flavour.Num() == 0) { const FString F = Str(*O, TEXT("flavour")); if (!F.IsEmpty()) { M.Flavour.Add(F); } }
				M.Tier = FMath::Clamp(Int(C, *O, TEXT("tier"), 1), 1, 4);
				M.CrewSize = IRange(C, *O, TEXT("crewSize"), M.CrewSize);
				M.DurationSec = Range(C, *O, TEXT("durationSec"), M.DurationSec);
				const FObj Th = Obj(*O, TEXT("threats"));
				for (const FString& S : StrArr(Th, TEXT("fixed"))) { EDCThreat T; if (ParseThreat(S, T)) { M.FixedThreats.Add(T); } else { C.Warn(FString::Printf(TEXT("%s: bad threat %s"), *M.Id.ToString(), *S)); } }
				for (const FString& S : StrArr(Th, TEXT("pool"))) { EDCThreat T; if (ParseThreat(S, T)) { M.PoolThreats.Add(T); } else { C.Warn(FString::Printf(TEXT("%s: bad threat %s"), *M.Id.ToString(), *S)); } }
				M.Extra = Has(Th, TEXT("extra")) ? IRange(C, Th, TEXT("extra"), M.Extra) : FIntPoint(0, 0);
				M.Cost = IRange(C, *O, TEXT("cost"), M.Cost);
				M.Payout = IRange(C, *O, TEXT("payout"), M.Payout);
				M.XPReward = Int(C, *O, TEXT("xpReward"), M.XPReward);
				M.ArrestRisk = Num(C, *O, TEXT("arrestRisk"), M.ArrestRisk);
				M.NightChance = Num(C, *O, TEXT("nightChance"), M.NightChance);
				M.Weight = Num(C, *O, TEXT("weight"), M.Weight);
				M.bFinal = Bool(*O, TEXT("final"), false) || M.Id == Balance.FinalTemplateId;
				Missions.Add(M);
			}
		}
	}

	// ── rooms.json ──
	{
		C.File = TEXT("rooms.json");
		const FObj R = ReadJson(C, PickFile(Dir, TEXT("rooms.json")));
		if (const FArr* A = Arr(R, TEXT("rooms")))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const FObj* O = nullptr;
				if (!V.IsValid() || !V->TryGetObject(O)) { continue; }
				FDCRoomDef D;
				if (!ParseRoom(Str(*O, TEXT("type")), D.Type)) { C.Warn(TEXT("bad room type")); continue; }
				D.DisplayName = Str(*O, TEXT("displayName"), DC::RoomName(D.Type));
				D.Description = Str(*O, TEXT("description"));
				D.Icon = FName(*Str(*O, TEXT("icon"), DC::RoomIcon(D.Type).ToString()));
				D.Cost = IntArr(*O, TEXT("cost"));
				D.BuildSec = NumArr(*O, TEXT("buildSec"));
				D.RequiredSuccesses = IntArr(*O, TEXT("requiredSuccesses"));
				D.UpkeepPerDay = IntArr(*O, TEXT("upkeepPerDay"));
				D.LevelEffects = StrArr(*O, TEXT("levelEffects"));
				if (const FArr* L = Arr(*O, TEXT("levels")))
				{
					for (const TSharedPtr<FJsonValue>& LV : *L)
					{
						const FObj* LO = nullptr;
						D.Levels.Add((LV.IsValid() && LV->TryGetObject(LO)) ? ParseEffects(C, Arr(*LO, TEXT("effects"))) : TArray<FDCEffect>());
					}
				}
				while (D.Cost.Num() < 3) { D.Cost.Add(D.Cost.Num() ? D.Cost.Last() * 2 : 5000); }
				while (D.BuildSec.Num() < 3) { D.BuildSec.Add(D.BuildSec.Num() ? D.BuildSec.Last() : 60.f); }
				while (D.RequiredSuccesses.Num() < 3) { D.RequiredSuccesses.Add(D.RequiredSuccesses.Num() ? D.RequiredSuccesses.Last() : 0); }
				while (D.UpkeepPerDay.Num() < 3) { D.UpkeepPerDay.Add(D.UpkeepPerDay.Num() ? D.UpkeepPerDay.Last() : 0); }
				while (D.LevelEffects.Num() < 3) { D.LevelEffects.Add(FString()); }
				while (D.Levels.Num() < 3) { D.Levels.Add(D.Levels.Num() ? D.Levels.Last() : TArray<FDCEffect>()); }
				Rooms.Add(D);
			}
		}
		for (int32 i = 0; i < (int32)EDCRoomType::MAX; ++i)
		{
			if (!FindRoom((EDCRoomType)i))
			{
				C.Warn(FString::Printf(TEXT("room %s missing"), DC::RoomName((EDCRoomType)i)));
				FDCRoomDef D; D.Type = (EDCRoomType)i; D.DisplayName = DC::RoomName(D.Type); D.Icon = DC::RoomIcon(D.Type);
				D.Cost = { 5000, 10000, 20000 }; D.BuildSec = { 60, 120, 180 }; D.RequiredSuccesses = { 0, 0, 0 }; D.UpkeepPerDay = { 0, 0, 0 };
				D.LevelEffects = { FString(), FString(), FString() }; D.Levels.SetNum(3);
				Rooms.Add(D);
			}
		}
		Rooms.Sort([](const FDCRoomDef& A, const FDCRoomDef& B) { return (int32)A.Type < (int32)B.Type; });
	}

	const bool bOk = Missions.Num() > 0 && Abilities.Num() > 0;
	UE_LOG(LogDC, Display, TEXT("[Data] loaded from %s: %d missions, %d abilities, %d traits, %d gear, %d perks, %d rooms, %d warnings"),
		*SourceDir, Missions.Num(), Abilities.Num(), Traits.Num(), Gear.Num(), Perks.Num(), Rooms.Num(), Warnings);
	return bOk;
}
