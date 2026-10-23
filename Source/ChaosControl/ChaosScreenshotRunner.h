#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChaosAutoTestRunner.h"
#include "ChaosScreenshotRunner.generated.h"

/**
 * Visual verification pass, spawned when the game is launched with -ChaosScreenshots.
 * Walks through every screen and arena (with mechanisms active), saves PNGs to
 * Saved/Screenshots/Chaos/ and quits. Used to review the presentation without a human tester.
 */
UCLASS(NotPlaceable)
class CHAOSCONTROL_API AChaosScreenshotRunner : public AActor
{
	GENERATED_BODY()

public:
	AChaosScreenshotRunner();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void AddStep(const FString& Name, TFunction<bool(float)> Run);
	void AddSettle(float Seconds);
	void AddShot(const FString& FileName);
	void AddArenaSteps(EChaosArena Arena, int32 ShotIndex);

	TArray<FChaosAutoTestStep> Steps;
	int32 StepIndex = 0;
	float StepStartTime = 0.f;
	bool bFinished = false;
	FString RunFolder;
};
