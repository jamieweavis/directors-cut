// Director's Cut — save slots + leaderboard automation tests: DirectorsCut.Sim.Slots.* (Agent A).
// Every test uses its own slot prefix (SaveSlotOverride), never the real "DCRun0..2" / "DCLeaderboard".
#include "Misc/AutomationTest.h"
#include "Tests/Sim/DCSimTestUtils.h"
#include "Sim/DCSaveGame.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using A = FDCSimTestAccess;
	constexpr EAutomationTestFlags DCSlotFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	void Wipe(const TCHAR* Prefix)
	{
		TArray<FString> Names = { Prefix, FString(Prefix) + TEXT("_Leaderboard") };
		for (int32 i = 0; i < UDCRunSubsystem::NumSaveSlots; ++i) { Names.Add(FString(Prefix) + FString::FromInt(i)); }
		for (const FString& N : Names) { if (UGameplayStatics::DoesSaveGameExist(N, 0)) { UGameplayStatics::DeleteGameInSlot(N, 0); } }
	}

	/** Ends the run with "No crew, no cash" on the next tick. */
	void ForceGameOver(UDCRunSubsystem& S)
	{
		A::ClearRun(S);
		A::Cash(S) = 10;
		S.AdvanceTime(1.0);
	}

	struct FWipeGuard
	{
		const TCHAR* P;
		explicit FWipeGuard(const TCHAR* In) : P(In) { Wipe(P); }
		~FWipeGuard() { Wipe(P); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSlotAutosave, "DirectorsCut.Sim.Slots.AutosaveToCurrentSlot", DCSlotFlags)
bool FDCSlotAutosave::RunTest(const FString&)
{
	const TCHAR* P = TEXT("DCRun_TSlotA");
	FWipeGuard W(P);
	TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim(P, true);
	DCSimTest::FEndGuard G(S.Get());
	TestEqual(TEXT("no current slot"), S->GetCurrentSlot(), -1);
	TestFalse(TEXT("no save"), S->HasSave());
	S->StartNewRunInSlot(1, 77);
	TestEqual(TEXT("current slot"), S->GetCurrentSlot(), 1);
	S->AdvanceTime(A::Data(*S).Balance.AutosaveSeconds + 5.0);
	TestFalse(TEXT("slot 0 untouched"), UGameplayStatics::DoesSaveGameExist(A::SlotName(*S, 0), 0));
	TestTrue(TEXT("slot 1 written"), UGameplayStatics::DoesSaveGameExist(A::SlotName(*S, 1), 0));
	TestFalse(TEXT("slot 2 untouched"), UGameplayStatics::DoesSaveGameExist(A::SlotName(*S, 2), 0));
	const TArray<FDCSaveSlotInfo> Slots = S->GetSaveSlots();
	TestEqual(TEXT("3 entries"), Slots.Num(), 3);
	TestTrue(TEXT("slot 1 used"), Slots[1].bUsed && !Slots[0].bUsed && !Slots[2].bUsed);
	TestTrue(TEXT("autosave time"), Slots[1].PlayTime > 0.0);
	TestEqual(TEXT("seed"), Slots[1].Seed, 77);
	TestFalse(TEXT("unfinished"), Slots[1].bFinished);
	TestEqual(TEXT("most recent"), S->GetMostRecentSlot(), 1);

	// Headers read from disk by a fresh instance match.
	TStrongObjectPtr<UDCRunSubsystem> L = DCSimTest::NewSim(P, true);
	DCSimTest::FEndGuard GL(L.Get());
	const TArray<FDCSaveSlotInfo> LS = L->GetSaveSlots();
	TestTrue(TEXT("disk header used"), LS[1].bUsed && !LS[0].bUsed);
	TestEqual(TEXT("disk header time"), LS[1].PlayTime, Slots[1].PlayTime);
	TestEqual(TEXT("disk header day"), LS[1].Day, Slots[1].Day);

	// Legacy StartNewRun: first empty slot.
	L->StartNewRun(5);
	TestEqual(TEXT("first empty slot"), L->GetCurrentSlot(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSlotCopyAndResume, "DirectorsCut.Sim.Slots.CopyAndResume", DCSlotFlags)
bool FDCSlotCopyAndResume::RunTest(const FString&)
{
	const TCHAR* P = TEXT("DCRun_TSlotB");
	FWipeGuard W(P);
	TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim(P, true);
	DCSimTest::FEndGuard G(S.Get());
	S->StartNewRunInSlot(0, 1234);
	DCSimTest::BotStep(*S);
	S->AdvanceTime(30.0);
	S->SaveRun();
	const FString Older = DCSimTest::DescribeState(*S);
	const double OlderT = S->GetRunTime();

	TestTrue(TEXT("save to slot 2"), S->SaveToSlot(2));
	TestEqual(TEXT("current is copy"), S->GetCurrentSlot(), 2);
	S->AdvanceTime(100.0);
	S->SaveRun();
	const FString Newer = DCSimTest::DescribeState(*S);
	const TArray<FDCSaveSlotInfo> Slots = S->GetSaveSlots();
	TestEqual(TEXT("slot 0 kept older"), Slots[0].PlayTime, OlderT);
	TestEqual(TEXT("slot 2 newer"), Slots[2].PlayTime, S->GetRunTime());
	TestEqual(TEXT("most recent = 2"), S->GetMostRecentSlot(), 2);

	TStrongObjectPtr<UDCRunSubsystem> L = DCSimTest::NewSim(P, true);
	DCSimTest::FEndGuard GL(L.Get());
	TestEqual(TEXT("continue picks newest"), L->ContinueRun() ? L->GetCurrentSlot() : -2, 2);
	TestEqual(TEXT("newer state"), DCSimTest::DescribeState(*L), Newer);
	TestTrue(TEXT("load older slot"), L->LoadSlot(0));
	TestEqual(TEXT("phase"), L->GetPhase(), EDCGamePhase::Playing);
	TestEqual(TEXT("current slot"), L->GetCurrentSlot(), 0);
	TestEqual(TEXT("older state"), DCSimTest::DescribeState(*L), Older);

	// Resuming the older slot follows the same future as the original did from that point.
	TStrongObjectPtr<UDCRunSubsystem> R = DCSimTest::NewSim(P, true);
	DCSimTest::FEndGuard GR(R.Get());
	TestTrue(TEXT("load older again"), R->LoadSlot(0));
	A::EnableSave(*L, false);
	A::EnableSave(*R, false);
	L->AdvanceTime(100.0);
	R->AdvanceTime(100.0);
	TestEqual(TEXT("deterministic resume"), DCSimTest::DescribeState(*L), DCSimTest::DescribeState(*R));
	TestFalse(TEXT("missing slot"), L->LoadSlot(1));
	TestFalse(TEXT("bad slot"), L->LoadSlot(7));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSlotFinished, "DirectorsCut.Sim.Slots.FinishedRun", DCSlotFlags)
bool FDCSlotFinished::RunTest(const FString&)
{
	const TCHAR* P = TEXT("DCRun_TSlotC");
	FWipeGuard W(P);
	TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim(P, true);
	DCSimTest::FEndGuard G(S.Get());
	S->ClearLeaderboard();
	S->StartNewRunInSlot(1, 6);
	S->AdvanceTime(40.0);
	ForceGameOver(*S);
	TestEqual(TEXT("game over"), S->GetPhase(), EDCGamePhase::GameOver);
	const FDCRunSummary EndSum = S->GetRunSummary();
	TArray<FDCSaveSlotInfo> Slots = S->GetSaveSlots();
	TestTrue(TEXT("slot kept"), Slots[1].bUsed);
	TestTrue(TEXT("slot finished"), Slots[1].bFinished && !Slots[1].bWon);
	TestEqual(TEXT("end reason"), Slots[1].EndReason, FString(TEXT("No crew, no cash")));
	TestFalse(TEXT("HasSave ignores finished"), S->HasSave());
	TestEqual(TEXT("most recent unfinished"), S->GetMostRecentSlot(true), -1);
	TestEqual(TEXT("most recent any"), S->GetMostRecentSlot(false), 1);
	TArray<FDCRunRecord> LB = S->GetLeaderboard();
	TestEqual(TEXT("one record"), LB.Num(), 1);
	if (LB.Num() == 1)
	{
		TestFalse(TEXT("record lost"), LB[0].bWon);
		TestEqual(TEXT("record time"), LB[0].PlayTime, EndSum.PlayTime);
		TestEqual(TEXT("record seed"), LB[0].Seed, 6);
	}

	// Reload the finished slot in a fresh instance: end phase, summary valid, clock frozen, no new record.
	TStrongObjectPtr<UDCRunSubsystem> L = DCSimTest::NewSim(P, true);
	DCSimTest::FEndGuard GL(L.Get());
	int32 OverCount = 0, PhaseCount = 0;
	EDCGamePhase LastPhase = EDCGamePhase::Title;
	L->OnGameOver.AddLambda([&](const FDCRunSummary&) { ++OverCount; });
	L->OnPhaseChanged.AddLambda([&](EDCGamePhase Ph) { ++PhaseCount; LastPhase = Ph; });
	TestFalse(TEXT("ContinueRun skips finished"), L->ContinueRun());
	TestTrue(TEXT("load finished"), L->LoadSlot(1));
	TestEqual(TEXT("end phase"), L->GetPhase(), EDCGamePhase::GameOver);
	TestEqual(TEXT("phase broadcast"), LastPhase, EDCGamePhase::GameOver);
	TestEqual(TEXT("one phase change"), PhaseCount, 1);
	TestEqual(TEXT("end screen delegate"), OverCount, 1);
	TestTrue(TEXT("has run"), L->HasActiveRun());
	TestFalse(TEXT("not tickable"), L->IsTickable());
	const FDCRunSummary LSum = L->GetRunSummary();
	TestEqual(TEXT("summary reason"), LSum.EndReason, EndSum.EndReason);
	TestEqual(TEXT("summary time"), LSum.PlayTime, EndSum.PlayTime);
	TestEqual(TEXT("summary days"), LSum.DaysSurvived, EndSum.DaysSurvived);
	const double T = L->GetRunTime();
	L->AdvanceTime(100.0);
	TestEqual(TEXT("clock frozen"), L->GetRunTime(), T);
	A::RecordFinished(*L); // even a forced re-record dedupes by RunId
	TestEqual(TEXT("no duplicate on reload"), L->GetLeaderboard().Num(), 1);

	// A finished run can be copied into another slot (end screen "save"); still one record.
	TestTrue(TEXT("save finished to slot 0"), L->SaveToSlot(0));
	TestEqual(TEXT("current = 0"), L->GetCurrentSlot(), 0);
	Slots = L->GetSaveSlots();
	TestTrue(TEXT("copy finished"), Slots[0].bUsed && Slots[0].bFinished);
	TStrongObjectPtr<UDCRunSubsystem> C = DCSimTest::NewSim(P, true);
	DCSimTest::FEndGuard GC(C.Get());
	TestTrue(TEXT("load copy"), C->LoadSlot(0));
	A::RecordFinished(*C);
	TestEqual(TEXT("copy not re-recorded"), C->GetLeaderboard().Num(), 1);

	// Delete.
	TestTrue(TEXT("delete"), C->DeleteSlot(0));
	TestEqual(TEXT("current cleared"), C->GetCurrentSlot(), -1);
	TestFalse(TEXT("deleted header"), C->GetSaveSlots()[0].bUsed);
	TestFalse(TEXT("deleted on disk"), UGameplayStatics::DoesSaveGameExist(A::SlotName(*C, 0), 0));
	TestTrue(TEXT("other slot kept"), C->GetSaveSlots()[1].bUsed);

	// Abandon deletes the current slot and records nothing.
	C->StartNewRunInSlot(2, 9);
	TestTrue(TEXT("slot 2 used"), C->GetSaveSlots()[2].bUsed);
	C->AbandonRun();
	TestFalse(TEXT("abandon deletes"), C->GetSaveSlots()[2].bUsed);
	TestEqual(TEXT("abandon records nothing"), C->GetLeaderboard().Num(), 1);
	TestEqual(TEXT("title"), C->GetPhase(), EDCGamePhase::Title);

	C->ClearLeaderboard();
	TestEqual(TEXT("cleared"), C->GetLeaderboard().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSlotLegacy, "DirectorsCut.Sim.Slots.LegacyMigration", DCSlotFlags)
bool FDCSlotLegacy::RunTest(const FString&)
{
	const TCHAR* P = TEXT("DCRun_TSlotD");
	const TCHAR* Src = TEXT("DCRun_TSlotDsrc");
	FWipeGuard W(P);
	FWipeGuard WS(Src);
	// Make a v1-style save under the legacy (unnumbered) name.
	FString Before;
	{
		TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim(Src, true);
		DCSimTest::FEndGuard G(S.Get());
		S->StartNewRunInSlot(0, 4321);
		S->AdvanceTime(20.0);
		S->SaveRun();
		Before = DCSimTest::DescribeState(*S);
		UDCSaveGame* SG = Cast<UDCSaveGame>(UGameplayStatics::LoadGameFromSlot(A::SlotName(*S, 0), 0));
		TestNotNull(TEXT("source save"), SG);
		if (!SG) { return false; }
		SG->Version = 1;
		SG->RunId.Invalidate();
		SG->SavedAt = FDateTime();
		TestTrue(TEXT("write legacy"), UGameplayStatics::SaveGameToSlot(SG, P, 0));
	}
	TStrongObjectPtr<UDCRunSubsystem> L = DCSimTest::NewSim(P, true);
	DCSimTest::FEndGuard GL(L.Get());
	const TArray<FDCSaveSlotInfo> Slots = L->GetSaveSlots();
	TestTrue(TEXT("migrated into slot 0"), Slots[0].bUsed && !Slots[0].bFinished);
	TestEqual(TEXT("seed"), Slots[0].Seed, 4321);
	TestFalse(TEXT("legacy removed"), UGameplayStatics::DoesSaveGameExist(P, 0));
	TestTrue(TEXT("HasSave"), L->HasSave());
	TestTrue(TEXT("continue"), L->ContinueRun());
	TestEqual(TEXT("slot"), L->GetCurrentSlot(), 0);
	TestEqual(TEXT("state"), DCSimTest::DescribeState(*L), Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSlotLeaderboardOrder, "DirectorsCut.Sim.Slots.LeaderboardOrder", DCSlotFlags)
bool FDCSlotLeaderboardOrder::RunTest(const FString&)
{
	const TCHAR* P = TEXT("DCRun_TSlotE");
	FWipeGuard W(P);
	TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim(P, true);
	DCSimTest::FEndGuard G(S.Get());
	UDCLeaderboardSave* LB = Cast<UDCLeaderboardSave>(UGameplayStatics::CreateSaveGameObject(UDCLeaderboardSave::StaticClass()));
	auto Add = [&](int32 Seed, bool bWon, double Time, int32 Days, int32 Take)
	{
		FDCRunRecord R; R.Seed = Seed; R.bWon = bWon; R.PlayTime = Time; R.DaysSurvived = Days; R.TotalTake = Take;
		LB->Records.Add(R); LB->RunIds.Add(FGuid::NewGuid());
	};
	Add(1, false, 500.0, 2, 900);
	Add(2, true, 9000.0, 7, 100);
	Add(3, false, 800.0, 5, 100);
	Add(4, true, 6000.0, 5, 50);
	Add(5, false, 700.0, 5, 400);
	TestTrue(TEXT("write board"), UGameplayStatics::SaveGameToSlot(LB, A::LeaderboardSlot(*S), 0));
	const TArray<FDCRunRecord> Out = S->GetLeaderboard();
	FString Order;
	for (const FDCRunRecord& R : Out) { Order += FString::FromInt(R.Seed); }
	TestEqual(TEXT("order"), Order, FString(TEXT("42531")));

	// A real finished run lands in its place.
	S->StartNewRunInSlot(0, 6);
	ForceGameOver(*S);
	const TArray<FDCRunRecord> Out2 = S->GetLeaderboard();
	TestEqual(TEXT("added"), Out2.Num(), 6);
	if (Out2.Num() == 6) { TestEqual(TEXT("day-1 loss last"), Out2.Last().Seed, 6); }
	return true;
}

#endif
