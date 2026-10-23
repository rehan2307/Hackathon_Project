#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChaosControlTypes.h"
#include "ChaosAutoTestRunner.generated.h"

class AChaosGameMode;
class AChaosPlayerCharacter;
class AChaosTrapBase;

/** One step of the scripted match. Run returns true when the step is complete. */
struct FChaosAutoTestStep
{
	FString Name;
	TFunction<bool(float /*StepElapsed*/)> Run;
};

/**
 * In-game acceptance test, spawned when the game is launched with -ChaosAutoTest.
 * It plays full matches on every arena through the real gameplay code (physical overlaps,
 * traps, timers, scoring, role switching, replay and draw) and logs
 * "CHAOS AUTOTEST RESULT: PASSED" or "FAILED", then quits.
 */
UCLASS()
class CHAOSCONTROL_API AChaosAutoTestRunner : public AActor
{
	GENERATED_BODY()

public:
	AChaosAutoTestRunner();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void AddStep(const FString& Name, TFunction<bool(float)> Run);
	void AddWait(float Seconds);
	void AddWaitForPhase(EChaosMatchPhase Phase);
	void AddArenaMatchSteps(EChaosArena Arena, bool bFullAbilityTest);
	void AddDrawMatchSteps(EChaosArena Arena);
	void Finish();

	bool Expect(bool bCondition, const FString& Description);
	AChaosGameMode* GetGM() const;
	AChaosPlayerCharacter* GetRunner() const;
	AChaosPlayerCharacter* GetSaboteur() const;
	AChaosTrapBase* FindActiveTrap(EChaosSabotageAbility Ability) const;
	static void Teleport(AChaosPlayerCharacter* Avatar, const FVector& Location);

	TArray<FChaosAutoTestStep> Steps;
	int32 StepIndex = 0;
	float StepStartTime = 0.f;
	int32 Checks = 0;
	int32 Failures = 0;
	bool bFinished = false;
	float SavedTime = 0.f;
};
