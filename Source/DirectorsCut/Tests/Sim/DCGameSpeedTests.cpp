// Director's Cut — game speed tests: DirectorsCut.Sim.GameSpeed (1x / 2x / 4x, player pause, reset on new run / load).
#include "Misc/AutomationTest.h"
#include "Tests/Sim/DCSimTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimGameSpeed, "DirectorsCut.Sim.GameSpeed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDCSimGameSpeed::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim();
	DCSimTest::FEndGuard G(S.Get());
	S->StartNewRun(11);
	FDCSimTestAccess::ClearRun(*S);

	TestEqual(TEXT("starts at 1x"), S->GetGameSpeed(), 0);
	TestFalse(TEXT("starts running"), S->IsSpeedPaused());
	TestEqual(TEXT("multipliers"), UDCRunSubsystem::GameSpeedMultiplier(0) + UDCRunSubsystem::GameSpeedMultiplier(1) * 10.f + UDCRunSubsystem::GameSpeedMultiplier(2) * 100.f, 421.f);

	int32 Changes = 0;
	S->OnGameSpeedChanged.AddLambda([&Changes](int32, bool) { ++Changes; });

	double T0 = S->GetRunTime();
	S->Tick(1.f);
	TestEqual(TEXT("1x: one run second per second"), S->GetRunTime() - T0, 1.0, 1e-6);

	S->SetGameSpeed(1);
	T0 = S->GetRunTime();
	S->Tick(1.f);
	TestEqual(TEXT("2x"), S->GetRunTime() - T0, 2.0, 1e-6);

	S->SetGameSpeed(2);
	T0 = S->GetRunTime();
	S->Tick(1.f);
	TestEqual(TEXT("4x"), S->GetRunTime() - T0, 4.0, 1e-6);
	TestEqual(TEXT("effective scale 4x"), S->GetEffectiveTimeScale(), 4.f);

	S->ToggleSpeedPaused();
	TestTrue(TEXT("paused"), S->IsSpeedPaused());
	TestFalse(TEXT("not tickable while paused"), S->IsTickable());
	TestEqual(TEXT("effective scale 0"), S->GetEffectiveTimeScale(), 0.f);
	TestEqual(TEXT("speed kept while paused"), S->GetGameSpeed(), 2);

	// The Esc menu opening / closing must not resume a player pause.
	S->SetPaused(true);
	S->SetPaused(false);
	TestTrue(TEXT("menu close keeps player pause"), S->IsSpeedPaused());

	S->ToggleSpeedPaused();
	TestFalse(TEXT("resumed"), S->IsSpeedPaused());
	TestEqual(TEXT("resumes at previous speed"), S->GetGameSpeed(), 2);

	S->SetSpeedPaused(true);
	S->SetGameSpeed(1);
	TestFalse(TEXT("picking a speed resumes"), S->IsSpeedPaused());
	TestEqual(TEXT("picked 2x"), S->GetGameSpeed(), 1);
	TestEqual(TEXT("change events"), Changes, 6);

	// Not saved: a new run starts at 1x, running.
	S->SetGameSpeed(2);
	S->SetSpeedPaused(true);
	S->StartNewRun(12);
	TestEqual(TEXT("new run 1x"), S->GetGameSpeed(), 0);
	TestFalse(TEXT("new run running"), S->IsSpeedPaused());

	// AdvanceTime (tests / bots) ignores the game speed.
	S->SetGameSpeed(2);
	T0 = S->GetRunTime();
	S->AdvanceTime(3.0);
	TestEqual(TEXT("AdvanceTime unscaled"), S->GetRunTime() - T0, 3.0, 1e-6);
	return true;
}

#endif
