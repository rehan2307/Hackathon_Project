#include "ChaosScreenshotRunner.h"
#include "ChaosControl.h"
#include "ChaosAbilityComponent.h"
#include "ChaosArenaManager.h"
#include "ChaosGameMode.h"
#include "ChaosObjective.h"
#include "ChaosPlayerCharacter.h"
#include "ChaosPlayerController.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

AChaosScreenshotRunner::AChaosScreenshotRunner()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AChaosScreenshotRunner::BeginPlay()
{
	Super::BeginPlay();

	AChaosGameMode* GM = GetWorld()->GetAuthGameMode<AChaosGameMode>();
	if (GM)
	{
		// Long enough to hold each screen for its screenshot.
		GM->RoundIntroDuration = 4.f;
		GM->RoundResultAutoContinue = 0.f;
	}

	// Wait for shaders and Lumen to settle before the first capture.
	AddStep(TEXT("wait for shaders"), [](float Elapsed)
	{
#if WITH_EDITOR
		if (GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0)
		{
			return false;
		}
#endif
		return Elapsed > 3.f;
	});
	AddSettle(3.f);
	AddShot(TEXT("01_MainMenu"));

	AddStep(TEXT("open arena select"), [this](float)
	{
		GetWorld()->GetAuthGameMode<AChaosGameMode>()->OpenArenaSelect();
		return true;
	});
	AddSettle(1.5f);
	AddShot(TEXT("02_ArenaSelect"));
	AddStep(TEXT("close arena select"), [this](float)
	{
		GetWorld()->GetAuthGameMode<AChaosGameMode>()->CloseArenaSelect();
		return true;
	});

	AddArenaSteps(EChaosArena::NeonGrid, 3);
	AddArenaSteps(EChaosArena::ReactorRun, 6);
	AddArenaSteps(EChaosArena::ChaosCore, 9);

	// Final results: finish the current (Chaos Core) match.
	AddStep(TEXT("finish match"), [this](float)
	{
		AChaosGameMode* Mode = GetWorld()->GetAuthGameMode<AChaosGameMode>();
		Mode->ContinueAfterRound();
		return Mode->GetPhase() == EChaosMatchPhase::RoundStarting;
	});
	AddStep(TEXT("wait round 2"), [this](float)
	{
		return GetWorld()->GetAuthGameMode<AChaosGameMode>()->GetPhase() == EChaosMatchPhase::RoundActive;
	});
	AddStep(TEXT("time out round 2"), [this](float)
	{
		GetWorld()->GetAuthGameMode<AChaosGameMode>()->ChaosForceTimeout();
		return true;
	});
	AddStep(TEXT("wait round 2 end"), [this](float)
	{
		AChaosGameMode* Mode = GetWorld()->GetAuthGameMode<AChaosGameMode>();
		if (Mode->GetPhase() != EChaosMatchPhase::RoundComplete)
		{
			return false;
		}
		Mode->ContinueAfterRound();
		return true;
	});
	AddSettle(1.5f);
	AddShot(TEXT("12_FinalResults"));

	StepStartTime = GetWorld()->GetRealTimeSeconds();
}

void AChaosScreenshotRunner::AddArenaSteps(EChaosArena Arena, int32 ShotIndex)
{
	const FString Name = ChaosUtil::GetArenaName(Arena).Replace(TEXT(" "), TEXT(""));

	AddStep(TEXT("start ") + Name, [this, Arena](float)
	{
		GetWorld()->GetAuthGameMode<AChaosGameMode>()->StartMatchOnArena(Arena);
		return true;
	});
	AddSettle(2.0f);
	AddShot(FString::Printf(TEXT("%02d_%s_Intro"), ShotIndex, *Name));

	AddStep(TEXT("wait active"), [this](float)
	{
		return GetWorld()->GetAuthGameMode<AChaosGameMode>()->GetPhase() == EChaosMatchPhase::RoundActive;
	});
	AddStep(TEXT("stage gameplay"), [this](float)
	{
		AChaosGameMode* Mode = GetWorld()->GetAuthGameMode<AChaosGameMode>();
		AChaosPlayerCharacter* Saboteur = Mode->GetPlayerCharacter(Mode->GetSaboteurIndex());
		AChaosPlayerCharacter* Runner = Mode->GetPlayerCharacter(Mode->GetRunnerIndex());
		FString Feedback;
		for (int32 Ability = 0; Ability < ChaosUtil::NumAbilities; ++Ability)
		{
			Saboteur->GetAbilityComponent()->TryActivate(static_cast<EChaosSabotageAbility>(Ability), Feedback);
		}
		// Collect one core so the HUD shows progress, then move the Runner into open floor.
		const TArray<TObjectPtr<AChaosObjective>>& Cores = Mode->GetArenaManager()->GetObjectives();
		if (Cores.Num() > 0)
		{
			Mode->TryCollectCore(Cores[0], Runner);
		}
		Runner->SetActorLocation(FVector(-700.f, 250.f, 95.f), false, nullptr, ETeleportType::TeleportPhysics);
		return true;
	});
	AddSettle(1.0f);
	AddShot(FString::Printf(TEXT("%02d_%s_Gameplay"), ShotIndex + 1, *Name));

	AddStep(TEXT("runner wins"), [this](float)
	{
		GetWorld()->GetAuthGameMode<AChaosGameMode>()->ChaosForceRunnerWin();
		return true;
	});
	AddSettle(1.2f);
	AddShot(FString::Printf(TEXT("%02d_%s_RoundResult"), ShotIndex + 2, *Name));
}

void AChaosScreenshotRunner::AddStep(const FString& Name, TFunction<bool(float)> Run)
{
	Steps.Add({ Name, MoveTemp(Run) });
}

void AChaosScreenshotRunner::AddSettle(float Seconds)
{
	AddStep(FString::Printf(TEXT("settle %.1fs"), Seconds), [Seconds](float Elapsed) { return Elapsed >= Seconds; });
}

void AChaosScreenshotRunner::AddShot(const FString& FileName)
{
	AddStep(TEXT("shot ") + FileName, [this, FileName](float)
	{
		// One folder per run: the screenshot writer never overwrites existing files.
		if (RunFolder.IsEmpty())
		{
			RunFolder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/Chaos") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		}
		const FString Path = RunFolder / (FileName + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogChaosControl, Display, TEXT("CHAOS SCREENSHOT: %s"), *Path);
		return true;
	});
	AddSettle(0.5f);
}

void AChaosScreenshotRunner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bFinished)
	{
		return;
	}
	if (!Steps.IsValidIndex(StepIndex))
	{
		bFinished = true;
		UE_LOG(LogChaosControl, Display, TEXT("CHAOS SCREENSHOTS: complete"));
		FPlatformMisc::RequestExit(false);
		return;
	}

	const float Now = GetWorld()->GetRealTimeSeconds();
	if (Steps[StepIndex].Run(Now - StepStartTime) || Now - StepStartTime > 120.f)
	{
		++StepIndex;
		StepStartTime = Now;
	}
}
