#include "ChaosAutoTestRunner.h"
#include "ChaosControl.h"
#include "ChaosAbilityComponent.h"
#include "ChaosArenaManager.h"
#include "ChaosExit.h"
#include "ChaosGameMode.h"
#include "ChaosLaserBarrier.h"
#include "ChaosMovingPlatform.h"
#include "ChaosObjective.h"
#include "ChaosPlayerCharacter.h"
#include "ChaosTrapBase.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"

namespace ChaosAutoTest
{
	constexpr float StepTimeout = 30.f;
}

AChaosAutoTestRunner::AChaosAutoTestRunner()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AChaosAutoTestRunner::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogChaosControl, Display, TEXT("CHAOS AUTOTEST: starting"));
	AddArenaMatchSteps(EChaosArena::NeonGrid, true);
	AddArenaMatchSteps(EChaosArena::ReactorRun, false);
	AddArenaMatchSteps(EChaosArena::ChaosCore, false);
	AddDrawMatchSteps(EChaosArena::ChaosCore);
	StepStartTime = GetWorld()->GetTimeSeconds();
}

void AChaosAutoTestRunner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bFinished)
	{
		return;
	}
	if (!Steps.IsValidIndex(StepIndex))
	{
		Finish();
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const float Elapsed = Now - StepStartTime;
	const FChaosAutoTestStep& Step = Steps[StepIndex];

	bool bDone = Step.Run(Elapsed);
	if (!bDone && Elapsed > ChaosAutoTest::StepTimeout)
	{
		Expect(false, FString::Printf(TEXT("Step timed out: %s"), *Step.Name));
		bDone = true;
	}

	if (bDone)
	{
		UE_LOG(LogChaosControl, Display, TEXT("CHAOS AUTOTEST: step %d/%d done - %s"), StepIndex + 1, Steps.Num(), *Step.Name);
		++StepIndex;
		StepStartTime = Now;
	}
}

// ---------------------------------------------------------------------------------------------
// Step builders
// ---------------------------------------------------------------------------------------------

void AChaosAutoTestRunner::AddStep(const FString& Name, TFunction<bool(float)> Run)
{
	Steps.Add({ Name, MoveTemp(Run) });
}

void AChaosAutoTestRunner::AddWait(float Seconds)
{
	AddStep(FString::Printf(TEXT("wait %.1fs"), Seconds), [Seconds](float Elapsed) { return Elapsed >= Seconds; });
}

void AChaosAutoTestRunner::AddWaitForPhase(EChaosMatchPhase Phase)
{
	AddStep(FString::Printf(TEXT("wait for phase %d"), static_cast<int32>(Phase)), [this, Phase](float)
	{
		return GetGM() && GetGM()->GetPhase() == Phase;
	});
}

void AChaosAutoTestRunner::AddArenaMatchSteps(EChaosArena Arena, bool bFullAbilityTest)
{
	const FString ArenaName = ChaosUtil::GetArenaName(Arena);

	AddStep(TEXT("start match on ") + ArenaName, [this, Arena](float)
	{
		GetGM()->StartMatchOnArena(Arena);
		Expect(GetGM()->GetPhase() == EChaosMatchPhase::RoundStarting, TEXT("AC-12 match enters RoundStarting"));
		Expect(GetGM()->GetSelectedArena() == Arena, TEXT("AC-03 requested arena is selected"));
		return true;
	});
	AddWaitForPhase(EChaosMatchPhase::RoundActive);

	AddStep(TEXT("round 1 initial state"), [this](float)
	{
		AChaosGameMode* GM = GetGM();
		AChaosArenaManager* ArenaManager = GM->GetArenaManager();
		Expect(GM->GetRunnerIndex() == 0, TEXT("AC-02 round 1: P1 is the Runner"));
		Expect(GetRunner() && GetRunner()->IsRunner(), TEXT("AC-02 runner avatar has Runner role"));
		Expect(GetSaboteur() && !GetSaboteur()->IsRunner(), TEXT("AC-02 saboteur avatar has Saboteur role"));
		Expect(GM->GetCoresRequired() == 3, TEXT("FR-02 arena has three energy cores"));
		Expect(!GM->IsExitUnlocked(), TEXT("FR-03 exit starts locked"));
		Expect(GM->GetRoundTimeRemaining() > GM->RoundDuration - 2.f, TEXT("FR-09 timer starts at round duration"));
		Expect(GM->GetScore(0) == 0 && GM->GetScore(1) == 0, TEXT("AC-07 scores start at zero"));

		int32 Counts[ChaosUtil::NumAbilities] = { 0, 0, 0 };
		for (AChaosTrapBase* Trap : ArenaManager->GetTraps())
		{
			++Counts[static_cast<int32>(Trap->GetAbilityType())];
		}
		Expect(Counts[0] > 0 && Counts[1] > 0 && Counts[2] > 0, TEXT("AC-03/AC-04 arena contains laser, platform and stun mechanisms"));
		return true;
	});

	// Locked exit: the real overlap path must reject the Runner.
	AddStep(TEXT("runner enters locked exit"), [this](float)
	{
		Teleport(GetRunner(), GetGM()->GetArenaManager()->GetExit()->GetActorLocation() + FVector(0.f, 0.f, 95.f));
		return true;
	});
	AddStep(TEXT("verify exit stayed locked"), [this](float Elapsed)
	{
		if (Elapsed < 0.3f)
		{
			return false;
		}
		Expect(GetGM()->GetPhase() == EChaosMatchPhase::RoundActive, TEXT("FR-03 locked exit does not end the round"));
		Teleport(GetRunner(), GetGM()->GetArenaManager()->GetSpawnTransform(EChaosPlayerRole::Runner).GetLocation());
		return true;
	});

	if (bFullAbilityTest)
	{
		AddStep(TEXT("sabotage: activation, cooldown and energy"), [this](float)
		{
			UChaosAbilityComponent* Abilities = GetSaboteur()->GetAbilityComponent();
			const float Max = Abilities->MaxEnergy;
			FString Feedback;

			Expect(Abilities->TryActivate(EChaosSabotageAbility::StunTrap, Feedback), TEXT("AC-04 stun trap activates: ") + Feedback);
			Expect(FMath::IsNearlyEqual(Abilities->GetEnergy(), Max - Abilities->StunTuning.EnergyCost), TEXT("FR-07 stun cost deducted"));
			Expect(!Abilities->TryActivate(EChaosSabotageAbility::StunTrap, Feedback), TEXT("AC-05 stun rejected while on cooldown"));
			Expect(FMath::IsNearlyEqual(Abilities->GetEnergy(), Max - Abilities->StunTuning.EnergyCost), TEXT("AC-05 rejected activation costs nothing"));

			Expect(Abilities->TryActivate(EChaosSabotageAbility::LaserBarrier, Feedback), TEXT("AC-04 laser activates: ") + Feedback);
			Expect(Abilities->TryActivate(EChaosSabotageAbility::MovingPlatform, Feedback), TEXT("AC-04 platform activates: ") + Feedback);

			AChaosLaserBarrier* Laser = Cast<AChaosLaserBarrier>(FindActiveTrap(EChaosSabotageAbility::LaserBarrier));
			Expect(Laser && Laser->IsBlocking(), TEXT("AC-04 active laser blocks movement"));

			// Stand the Runner on the active stun pad.
			if (AChaosTrapBase* StunPad = FindActiveTrap(EChaosSabotageAbility::StunTrap))
			{
				Teleport(GetRunner(), StunPad->GetActorLocation() + FVector(0.f, 0.f, 95.f));
			}
			else
			{
				Expect(false, TEXT("AC-04 an active stun pad exists"));
			}
			return true;
		});
		AddStep(TEXT("verify trap effects"), [this](float Elapsed)
		{
			if (Elapsed < 1.2f)
			{
				return false;
			}
			Expect(GetRunner()->IsStunned(), TEXT("AC-04 stun trap immobilises the Runner"));
			AChaosMovingPlatform* Platform = Cast<AChaosMovingPlatform>(FindActiveTrap(EChaosSabotageAbility::MovingPlatform));
			Expect(Platform && FMath::Abs(Platform->GetCurrentOffset()) > 10.f, TEXT("AC-04 active platform moves"));
			return true;
		});
		AddStep(TEXT("verify traps reset and energy limit"), [this](float Elapsed)
		{
			if (Elapsed < 8.f)
			{
				return false;
			}
			for (AChaosTrapBase* Trap : GetGM()->GetArenaManager()->GetTraps())
			{
				Expect(!Trap->IsTrapActive(), TEXT("AC-04 mechanism deactivated itself: ") + ChaosUtil::GetAbilityName(Trap->GetAbilityType()));
				if (const AChaosMovingPlatform* Platform = Cast<AChaosMovingPlatform>(Trap))
				{
					Expect(FMath::Abs(Platform->GetCurrentOffset()) < 1.f, TEXT("AC-04 platform returned home"));
				}
				if (const AChaosLaserBarrier* Laser = Cast<AChaosLaserBarrier>(Trap))
				{
					Expect(!Laser->IsBlocking(), TEXT("AC-04 laser no longer blocks"));
				}
			}
			Expect(!GetRunner()->IsStunned(), TEXT("AC-04 stun wore off"));

			UChaosAbilityComponent* Abilities = GetSaboteur()->GetAbilityComponent();
			FString Feedback;
			Expect(Abilities->GetCooldownRemaining(EChaosSabotageAbility::LaserBarrier) <= 0.f, TEXT("FR-04 laser cooldown expired"));
			Expect(!Abilities->TryActivate(EChaosSabotageAbility::LaserBarrier, Feedback), TEXT("AC-05 laser rejected with insufficient energy: ") + Feedback);
			return true;
		});
	}
	else
	{
		AddStep(TEXT("each mechanism type can be triggered"), [this](float)
		{
			UChaosAbilityComponent* Abilities = GetSaboteur()->GetAbilityComponent();
			FString Feedback;
			Expect(Abilities->TryActivate(EChaosSabotageAbility::LaserBarrier, Feedback), TEXT("AC-03 laser works in this arena"));
			Expect(Abilities->TryActivate(EChaosSabotageAbility::MovingPlatform, Feedback), TEXT("AC-03 platform works in this arena"));
			Expect(Abilities->TryActivate(EChaosSabotageAbility::StunTrap, Feedback), TEXT("AC-03 stun trap works in this arena"));
			return true;
		});
		AddWait(1.f);
	}

	// Collect each core through the real overlap trigger.
	for (int32 CoreIndex = 0; CoreIndex < 3; ++CoreIndex)
	{
		AddStep(FString::Printf(TEXT("runner touches core %d"), CoreIndex + 1), [this, CoreIndex](float)
		{
			const TArray<TObjectPtr<AChaosObjective>>& Cores = GetGM()->GetArenaManager()->GetObjectives();
			if (Expect(Cores.IsValidIndex(CoreIndex), TEXT("core exists")))
			{
				Teleport(GetRunner(), Cores[CoreIndex]->GetActorLocation());
			}
			return true;
		});
		AddStep(FString::Printf(TEXT("verify core %d collected"), CoreIndex + 1), [this, CoreIndex](float Elapsed)
		{
			if (Elapsed < 0.25f)
			{
				return false;
			}
			AChaosGameMode* GM = GetGM();
			const TArray<TObjectPtr<AChaosObjective>>& Cores = GM->GetArenaManager()->GetObjectives();
			Expect(Cores[CoreIndex]->IsCollected(), TEXT("AC-02 runner collected the core"));
			Expect(GM->GetCoresCollected() == CoreIndex + 1, TEXT("FR-02 objective counter updated"));
			Expect(GM->GetScore(0) == (CoreIndex + 1) * GM->Scoring.CorePoints, TEXT("AC-07 core points awarded once"));

			// Duplicate and wrong-role collection must not score.
			Expect(!GM->TryCollectCore(Cores[CoreIndex], GetRunner()), TEXT("AC-07 core cannot be collected twice"));
			if (Cores.IsValidIndex(CoreIndex + 1))
			{
				Expect(!GM->TryCollectCore(Cores[CoreIndex + 1], GetSaboteur()), TEXT("AC-02 saboteur cannot collect cores"));
			}
			Expect(GM->GetScore(0) == (CoreIndex + 1) * GM->Scoring.CorePoints, TEXT("AC-07 no duplicate award"));
			return true;
		});
	}

	AddStep(TEXT("runner enters unlocked exit"), [this](float)
	{
		Expect(GetGM()->IsExitUnlocked(), TEXT("FR-02 all cores unlock the exit"));
		Teleport(GetRunner(), GetGM()->GetArenaManager()->GetExit()->GetActorLocation() + FVector(0.f, 0.f, 95.f));
		return true;
	});
	AddStep(TEXT("verify runner victory"), [this](float Elapsed)
	{
		if (Elapsed < 0.3f)
		{
			return false;
		}
		AChaosGameMode* GM = GetGM();
		const FChaosScoringRules& Rules = GM->Scoring;
		Expect(GM->GetPhase() == EChaosMatchPhase::RoundComplete, TEXT("AC-08 round ends when the Runner escapes"));
		Expect(GM->GetLastRoundSummary().bRunnerWon, TEXT("AC-08 Runner recorded as winner"));
		Expect(GM->GetScore(0) == 3 * Rules.CorePoints + Rules.ExitPoints + Rules.RunnerWinBonus, TEXT("AC-07 runner total = cores + exit + win bonus"));
		Expect(GM->GetScore(1) == 0, TEXT("AC-07 saboteur scored nothing"));

		// Nothing may score or trigger after the round ends.
		FString Feedback;
		Expect(!GetSaboteur()->GetAbilityComponent()->TryActivate(EChaosSabotageAbility::LaserBarrier, Feedback), TEXT("NFR-02 no sabotage after round end"));
		Expect(!GM->TryRunnerExit(GetRunner()), TEXT("NFR-02 no second exit award"));
		SavedTime = GM->GetRoundTimeRemaining();
		return true;
	});
	AddStep(TEXT("verify timer stopped"), [this](float Elapsed)
	{
		if (Elapsed < 0.5f)
		{
			return false;
		}
		Expect(FMath::IsNearlyEqual(GetGM()->GetRoundTimeRemaining(), SavedTime), TEXT("FR-09 timer stops when the round ends"));
		GetGM()->ContinueAfterRound();
		return true;
	});

	AddStep(TEXT("round 2 reset and role swap"), [this](float)
	{
		AChaosGameMode* GM = GetGM();
		const FChaosScoringRules& Rules = GM->Scoring;
		Expect(GM->GetPhase() == EChaosMatchPhase::RoundStarting, TEXT("FR-09 automatic transition to next round"));
		Expect(GM->GetRunnerIndex() == 1, TEXT("AC-06 P2 is the Runner in round 2"));
		Expect(GetRunner()->GetPlayerIndex() == 1 && GetRunner()->IsRunner(), TEXT("AC-06 P2 avatar now has Runner role"));
		Expect(!GM->GetPlayerCharacter(0)->IsRunner(), TEXT("AC-06 P1 avatar now has Saboteur role"));
		Expect(GM->GetScore(0) == 3 * Rules.CorePoints + Rules.ExitPoints + Rules.RunnerWinBonus, TEXT("AC-06 scores preserved across rounds"));
		Expect(GM->GetCoresCollected() == 0, TEXT("AC-09 core counter reset"));
		for (AChaosObjective* Core : GM->GetArenaManager()->GetObjectives())
		{
			Expect(!Core->IsCollected(), TEXT("AC-09 cores restored"));
		}
		for (AChaosTrapBase* Trap : GM->GetArenaManager()->GetTraps())
		{
			Expect(!Trap->IsTrapActive(), TEXT("AC-09 mechanisms reset"));
		}
		Expect(!GM->IsExitUnlocked(), TEXT("AC-09 exit relocked"));
		UChaosAbilityComponent* Abilities = GetSaboteur()->GetAbilityComponent();
		Expect(FMath::IsNearlyEqual(Abilities->GetEnergy(), Abilities->MaxEnergy), TEXT("AC-09 sabotage energy refilled"));
		for (int32 AbilityIndex = 0; AbilityIndex < ChaosUtil::NumAbilities; ++AbilityIndex)
		{
			Expect(Abilities->GetCooldownRemaining(static_cast<EChaosSabotageAbility>(AbilityIndex)) <= 0.f, TEXT("AC-09 cooldowns cleared"));
		}
		Expect(FMath::IsNearlyEqual(GM->GetRoundTimeRemaining(), GM->RoundDuration), TEXT("AC-09 timer reset"));
		return true;
	});
	AddWaitForPhase(EChaosMatchPhase::RoundActive);

	AddStep(TEXT("force timer expiry"), [this](float)
	{
		GetGM()->ChaosForceTimeout();
		return true;
	});
	AddWaitForPhase(EChaosMatchPhase::RoundComplete);
	AddStep(TEXT("verify saboteur victory"), [this](float)
	{
		AChaosGameMode* GM = GetGM();
		const FChaosScoringRules& Rules = GM->Scoring;
		Expect(!GM->GetLastRoundSummary().bRunnerWon, TEXT("AC-08 timer expiry ends round for the Saboteur"));
		Expect(GM->GetScore(0) == 3 * Rules.CorePoints + Rules.ExitPoints + Rules.RunnerWinBonus + Rules.SaboteurDefensePoints, TEXT("AC-07 saboteur defense points to P1"));
		Expect(GM->GetScore(1) == 0, TEXT("AC-07 P2 unchanged"));
		GM->ContinueAfterRound();
		return true;
	});

	AddStep(TEXT("verify final results and replay"), [this](float)
	{
		AChaosGameMode* GM = GetGM();
		Expect(GM->GetPhase() == EChaosMatchPhase::MatchComplete, TEXT("AC-10 match completes after both rounds"));
		Expect(GM->GetMatchWinnerIndex() == 0, TEXT("AC-10 higher score wins"));

		GM->PlayAgain();
		Expect(GM->GetPhase() == EChaosMatchPhase::RoundStarting, TEXT("AC-11 play again starts a new match"));
		Expect(GM->GetScore(0) == 0 && GM->GetScore(1) == 0, TEXT("AC-11 new match clears scores"));
		Expect(GM->GetRoundNumber() == 1 && GM->GetRunnerIndex() == 0, TEXT("AC-11 new match restarts at round 1"));

		GM->ReturnToMainMenu();
		Expect(GM->GetPhase() == EChaosMatchPhase::MainMenu, TEXT("FR-11 return to main menu"));
		return true;
	});
}

void AChaosAutoTestRunner::AddDrawMatchSteps(EChaosArena Arena)
{
	AddStep(TEXT("start draw match"), [this, Arena](float)
	{
		GetGM()->StartMatchOnArena(Arena);
		return true;
	});
	for (int32 Round = 0; Round < 2; ++Round)
	{
		AddWaitForPhase(EChaosMatchPhase::RoundActive);
		AddStep(TEXT("draw match: time out"), [this](float)
		{
			GetGM()->ChaosForceTimeout();
			return true;
		});
		AddWaitForPhase(EChaosMatchPhase::RoundComplete);
		AddStep(TEXT("draw match: continue"), [this](float)
		{
			GetGM()->ContinueAfterRound();
			return true;
		});
	}
	AddStep(TEXT("verify draw"), [this](float)
	{
		AChaosGameMode* GM = GetGM();
		Expect(GM->GetPhase() == EChaosMatchPhase::MatchComplete, TEXT("AC-10 draw match completes"));
		Expect(GM->GetScore(0) == GM->GetScore(1), TEXT("AC-10 equal scores"));
		Expect(GM->GetMatchWinnerIndex() == INDEX_NONE, TEXT("AC-10 draw is reported"));
		GM->ReturnToMainMenu();
		return true;
	});
}

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

void AChaosAutoTestRunner::Finish()
{
	bFinished = true;
	const bool bPassed = Failures == 0;
	UE_LOG(LogChaosControl, Display, TEXT("CHAOS AUTOTEST RESULT: %s (%d checks, %d failures)"), bPassed ? TEXT("PASSED") : TEXT("FAILED"), Checks, Failures);
	FPlatformMisc::RequestExit(false);
}

bool AChaosAutoTestRunner::Expect(bool bCondition, const FString& Description)
{
	++Checks;
	if (bCondition)
	{
		UE_LOG(LogChaosControl, Display, TEXT("  [PASS] %s"), *Description);
	}
	else
	{
		++Failures;
		UE_LOG(LogChaosControl, Error, TEXT("  [FAIL] %s"), *Description);
	}
	return bCondition;
}

AChaosGameMode* AChaosAutoTestRunner::GetGM() const
{
	return GetWorld()->GetAuthGameMode<AChaosGameMode>();
}

AChaosPlayerCharacter* AChaosAutoTestRunner::GetRunner() const
{
	return GetGM()->GetPlayerCharacter(GetGM()->GetRunnerIndex());
}

AChaosPlayerCharacter* AChaosAutoTestRunner::GetSaboteur() const
{
	return GetGM()->GetPlayerCharacter(GetGM()->GetSaboteurIndex());
}

AChaosTrapBase* AChaosAutoTestRunner::FindActiveTrap(EChaosSabotageAbility Ability) const
{
	for (AChaosTrapBase* Trap : GetGM()->GetArenaManager()->GetTraps())
	{
		if (Trap && Trap->GetAbilityType() == Ability && Trap->IsTrapActive())
		{
			return Trap;
		}
	}
	return nullptr;
}

void AChaosAutoTestRunner::Teleport(AChaosPlayerCharacter* Avatar, const FVector& Location)
{
	if (Avatar)
	{
		Avatar->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		Avatar->GetCharacterMovement()->StopMovementImmediately();
	}
}
