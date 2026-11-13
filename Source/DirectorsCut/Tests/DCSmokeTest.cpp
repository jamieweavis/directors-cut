#include "Misc/AutomationTest.h"
#include "Core/DCTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSmokeTest, "DirectorsCut.Smoke.Pipeline", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDCSmokeTest::RunTest(const FString& Parameters)
{
	FDCStatBlock A; A.Muscle = 10;
	FDCStatBlock B; B.Muscle = 5; B.Tech = 3;
	const FDCStatBlock C = A + B;
	TestEqual(TEXT("Stat add muscle"), C.Get(EDCStat::Muscle), 15);
	TestEqual(TEXT("Stat add tech"), C.Get(EDCStat::Tech), 3);
	TestEqual(TEXT("Stamp text"), FString(DC::OutcomeStamp(EDCHeistOutcome::Clean)), FString(TEXT("IT'S A WRAP")));
	return true;
}

#endif
