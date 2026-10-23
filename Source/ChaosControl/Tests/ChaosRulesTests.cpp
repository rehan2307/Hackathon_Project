#include "Misc/AutomationTest.h"
#include "ChaosControlTypes.h"
#include "ChaosGameMode.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChaosAbilityActivationTest, "ChaosControl.Rules.AbilityActivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FChaosAbilityActivationTest::RunTest(const FString& Parameters)
{
	using ChaosUtil::CheckAbilityActivation;

	TestTrue(TEXT("Ready with enough energy"), CheckAbilityActivation(true, 100.f, 35.f, 0.f) == EChaosActivationCheck::Ok);
	TestTrue(TEXT("Exact energy is enough"), CheckAbilityActivation(true, 40.f, 40.f, 0.f) == EChaosActivationCheck::Ok);
	TestTrue(TEXT("Insufficient energy rejected"), CheckAbilityActivation(true, 30.f, 35.f, 0.f) == EChaosActivationCheck::InsufficientEnergy);
	TestTrue(TEXT("Cooldown rejected"), CheckAbilityActivation(true, 100.f, 35.f, 2.5f) == EChaosActivationCheck::OnCooldown);
	TestTrue(TEXT("Inactive round rejected"), CheckAbilityActivation(false, 100.f, 35.f, 0.f) == EChaosActivationCheck::RoundInactive);
	TestTrue(TEXT("Round state checked first"), CheckAbilityActivation(false, 0.f, 35.f, 5.f) == EChaosActivationCheck::RoundInactive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChaosPhaseTransitionTest, "ChaosControl.Rules.PhaseTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FChaosPhaseTransitionTest::RunTest(const FString& Parameters)
{
	using EPhase = EChaosMatchPhase;
	auto Allowed = [](EPhase From, EPhase To) { return AChaosGameMode::IsTransitionAllowed(From, To); };

	TestTrue(TEXT("Menu -> arena select"), Allowed(EPhase::MainMenu, EPhase::ArenaSelect));
	TestTrue(TEXT("Menu -> round starting"), Allowed(EPhase::MainMenu, EPhase::RoundStarting));
	TestTrue(TEXT("Starting -> active"), Allowed(EPhase::RoundStarting, EPhase::RoundActive));
	TestTrue(TEXT("Active -> complete"), Allowed(EPhase::RoundActive, EPhase::RoundComplete));
	TestTrue(TEXT("Complete -> next round"), Allowed(EPhase::RoundComplete, EPhase::RoundStarting));
	TestTrue(TEXT("Complete -> match complete"), Allowed(EPhase::RoundComplete, EPhase::MatchComplete));
	TestTrue(TEXT("Match complete -> play again"), Allowed(EPhase::MatchComplete, EPhase::RoundStarting));
	TestTrue(TEXT("Match complete -> menu"), Allowed(EPhase::MatchComplete, EPhase::MainMenu));

	TestFalse(TEXT("Round cannot complete twice"), Allowed(EPhase::RoundComplete, EPhase::RoundComplete));
	TestFalse(TEXT("Menu cannot jump to active"), Allowed(EPhase::MainMenu, EPhase::RoundActive));
	TestFalse(TEXT("Active cannot skip to match complete"), Allowed(EPhase::RoundActive, EPhase::MatchComplete));
	TestFalse(TEXT("Starting cannot complete"), Allowed(EPhase::RoundStarting, EPhase::RoundComplete));
	TestFalse(TEXT("Arena select only from menu"), Allowed(EPhase::RoundActive, EPhase::ArenaSelect));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChaosFormatTimeTest, "ChaosControl.Rules.FormatTime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FChaosFormatTimeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("90s"), ChaosUtil::FormatTime(90.f), FString(TEXT("1:30")));
	TestEqual(TEXT("Rounds up"), ChaosUtil::FormatTime(0.2f), FString(TEXT("0:01")));
	TestEqual(TEXT("Never negative"), ChaosUtil::FormatTime(-3.f), FString(TEXT("0:00")));
	return true;
}

#endif
