#include "Misc/AutomationTest.h"
#include "Core/DCTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSmokeTest, "DirectorsCut.Smoke.Pipeline", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDCSmokeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Clock m:ss"), DC::Clock(65.0), FString(TEXT("1:05")));
	TestEqual(TEXT("Clock h:mm:ss"), DC::Clock(3725.0), FString(TEXT("1:02:05")));
	TestEqual(TEXT("Outcome"), FString(DC::OutcomeName(EDCMissionOutcome::Busted)), FString(TEXT("BUSTED")));
	TestEqual(TEXT("Threat icon"), DC::ThreatIcon(EDCThreat::Cameras), FName(TEXT("cctv")));
	return true;
}

#endif
