#include "ChaosGameMode.h"
#include "ChaosControl.h"
#include "ChaosAbilityComponent.h"
#include "ChaosArenaManager.h"
#include "ChaosAutoTestRunner.h"
#include "ChaosExit.h"
#include "ChaosHUD.h"
#include "ChaosObjective.h"
#include "ChaosPlayerCharacter.h"
#include "ChaosPlayerController.h"
#include "ChaosScreenshotRunner.h"
#include "ChaosVisuals.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

AChaosGameMode::AChaosGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Avatars are spawned and driven by the game mode, not auto-spawned per controller.
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AChaosPlayerController::StaticClass();
	HUDClass = AChaosHUD::StaticClass();
}

bool AChaosGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	return false;
}

void AChaosGameMode::BeginPlay()
{
	Super::BeginPlay();

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ArenaManager = GetWorld()->SpawnActor<AChaosArenaManager>(AChaosArenaManager::StaticClass(), FTransform::Identity, Params);
	if (!ArenaManager)
	{
		UE_LOG(LogChaosControl, Error, TEXT("Failed to spawn the arena manager"));
		return;
	}

	ArenaManager->BuildArena(SelectedArena);
	SpawnPlayers();
	Phase = EChaosMatchPhase::MainMenu;
	PlacePlayersForCurrentRound();

	if (FParse::Param(FCommandLine::Get(), TEXT("ChaosAutoTest")))
	{
		GetWorld()->SpawnActor<AChaosAutoTestRunner>(AChaosAutoTestRunner::StaticClass(), FTransform::Identity, Params);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("ChaosScreenshots")))
	{
		GetWorld()->SpawnActor<AChaosScreenshotRunner>(AChaosScreenshotRunner::StaticClass(), FTransform::Identity, Params);
	}
}

void AChaosGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Now = GetNow();
	Messages.RemoveAll([Now](const FChaosHudMessage& Message) { return Message.ExpireTime < Now; });

	switch (Phase)
	{
	case EChaosMatchPhase::RoundStarting:
		if (Now >= PhaseEndTime)
		{
			ActivateRound();
		}
		break;

	case EChaosMatchPhase::RoundActive:
		RoundTimeRemaining = FMath::Max(0.f, RoundTimeRemaining - DeltaSeconds);
		if (RoundTimeRemaining <= 0.f)
		{
			EndRound(false, TEXT("Time expired - the Saboteur held the line"));
		}
		break;

	case EChaosMatchPhase::RoundComplete:
		if (RoundResultAutoContinue > 0.f && Now >= PhaseEndTime)
		{
			ContinueAfterRound();
		}
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------------------------

bool AChaosGameMode::IsTransitionAllowed(EChaosMatchPhase From, EChaosMatchPhase To)
{
	switch (To)
	{
	case EChaosMatchPhase::MainMenu:
		return From != EChaosMatchPhase::MainMenu;
	case EChaosMatchPhase::ArenaSelect:
		return From == EChaosMatchPhase::MainMenu;
	case EChaosMatchPhase::RoundStarting:
		// New match from the menus, next round, play again, or restart from the pause menu.
		return true;
	case EChaosMatchPhase::RoundActive:
		return From == EChaosMatchPhase::RoundStarting;
	case EChaosMatchPhase::RoundComplete:
		return From == EChaosMatchPhase::RoundActive;
	case EChaosMatchPhase::MatchComplete:
		return From == EChaosMatchPhase::RoundComplete;
	}
	return false;
}

bool AChaosGameMode::TransitionTo(EChaosMatchPhase NewPhase)
{
	if (!IsTransitionAllowed(Phase, NewPhase))
	{
		UE_LOG(LogChaosControl, Warning, TEXT("Rejected phase transition %d -> %d"), static_cast<int32>(Phase), static_cast<int32>(NewPhase));
		return false;
	}

	UE_LOG(LogChaosControl, Log, TEXT("Phase %d -> %d (round %d)"), static_cast<int32>(Phase), static_cast<int32>(NewPhase), GetRoundNumber());
	Phase = NewPhase;
	return true;
}

// ---------------------------------------------------------------------------------------------
// Menu / flow
// ---------------------------------------------------------------------------------------------

void AChaosGameMode::OpenArenaSelect()
{
	TransitionTo(EChaosMatchPhase::ArenaSelect);
}

void AChaosGameMode::CloseArenaSelect()
{
	if (Phase == EChaosMatchPhase::ArenaSelect)
	{
		TransitionTo(EChaosMatchPhase::MainMenu);
	}
}

void AChaosGameMode::PreviewArena(EChaosArena Arena)
{
	if (Phase != EChaosMatchPhase::MainMenu && Phase != EChaosMatchPhase::ArenaSelect)
	{
		return;
	}

	SelectedArena = Arena;
	if (ArenaManager && (!ArenaManager->HasBuiltArena() || ArenaManager->GetCurrentArena() != Arena))
	{
		ArenaManager->BuildArena(Arena);
		PlacePlayersForCurrentRound();
	}
}

void AChaosGameMode::StartMatch()
{
	StartMatchOnArena(SelectedArena);
}

void AChaosGameMode::StartMatchOnArena(EChaosArena Arena)
{
	if (!ArenaManager)
	{
		return;
	}

	SetMenuPaused(false);
	SelectedArena = Arena;
	if (!ArenaManager->HasBuiltArena() || ArenaManager->GetCurrentArena() != Arena)
	{
		ArenaManager->BuildArena(Arena);
	}

	for (int32 Index = 0; Index < ChaosUtil::NumPlayers; ++Index)
	{
		Scores[Index] = 0;
	}
	CurrentRound = 0;
	LastRoundSummary = FChaosRoundSummary();

	UE_LOG(LogChaosControl, Log, TEXT("Match started on %s (%d rounds, %.0fs each)"), *ChaosUtil::GetArenaName(Arena), GetRoundsPerMatch(), RoundDuration);
	BeginRound();
}

void AChaosGameMode::ContinueAfterRound()
{
	if (Phase != EChaosMatchPhase::RoundComplete)
	{
		return;
	}

	if (IsFinalRound())
	{
		FinishMatch();
		return;
	}

	++CurrentRound;
	BeginRound();
}

void AChaosGameMode::PlayAgain()
{
	if (Phase == EChaosMatchPhase::MatchComplete)
	{
		StartMatchOnArena(SelectedArena);
	}
}

void AChaosGameMode::RestartMatch()
{
	StartMatchOnArena(SelectedArena);
}

void AChaosGameMode::ReturnToMainMenu()
{
	SetMenuPaused(false);
	if (Phase == EChaosMatchPhase::MainMenu)
	{
		return;
	}

	TransitionTo(EChaosMatchPhase::MainMenu);
	CurrentRound = 0;
	for (int32 Index = 0; Index < ChaosUtil::NumPlayers; ++Index)
	{
		Scores[Index] = 0;
	}
	Messages.Reset();
	if (ArenaManager)
	{
		ArenaManager->ResetArena();
	}
	PlacePlayersForCurrentRound();
}

void AChaosGameMode::SetMenuPaused(bool bPaused)
{
	if (bPaused && Phase != EChaosMatchPhase::RoundStarting && Phase != EChaosMatchPhase::RoundActive)
	{
		return;
	}
	if (bMenuPaused == bPaused)
	{
		return;
	}

	bMenuPaused = bPaused;
	UGameplayStatics::SetGamePaused(this, bPaused);
}

// ---------------------------------------------------------------------------------------------
// Round lifecycle
// ---------------------------------------------------------------------------------------------

void AChaosGameMode::BeginRound()
{
	if (!TransitionTo(EChaosMatchPhase::RoundStarting))
	{
		return;
	}

	CoresCollected = 0;
	RoundTimeRemaining = RoundDuration;
	for (int32 Index = 0; Index < ChaosUtil::NumPlayers; ++Index)
	{
		RoundPoints[Index] = 0;
	}
	Messages.Reset();

	if (ArenaManager)
	{
		ArenaManager->ResetArena();
	}
	PlacePlayersForCurrentRound();

	PhaseEndTime = GetNow() + RoundIntroDuration;
	if (RoundIntroDuration <= 0.f)
	{
		ActivateRound();
	}
}

void AChaosGameMode::ActivateRound()
{
	if (!TransitionTo(EChaosMatchPhase::RoundActive))
	{
		return;
	}
	PushMessage(TEXT("GO!"), ChaosUtil::SuccessColor, 1.5f);
}

void AChaosGameMode::EndRound(bool bRunnerWon, const FString& Reason)
{
	// Phase guard: a round can only end once, even if the exit and the timer fire on the same frame.
	if (Phase != EChaosMatchPhase::RoundActive || !TransitionTo(EChaosMatchPhase::RoundComplete))
	{
		return;
	}

	const int32 RunnerIndex = GetRunnerIndex();
	const int32 SaboteurIndex = GetSaboteurIndex();
	if (bRunnerWon)
	{
		AwardPoints(RunnerIndex, Scoring.ExitPoints + Scoring.RunnerWinBonus);
	}
	else
	{
		AwardPoints(SaboteurIndex, Scoring.SaboteurDefensePoints);
	}

	LastRoundSummary = FChaosRoundSummary();
	LastRoundSummary.RoundNumber = GetRoundNumber();
	LastRoundSummary.bRunnerWon = bRunnerWon;
	LastRoundSummary.WinnerIndex = bRunnerWon ? RunnerIndex : SaboteurIndex;
	LastRoundSummary.RunnerIndex = RunnerIndex;
	LastRoundSummary.CoresCollected = CoresCollected;
	LastRoundSummary.Reason = Reason;
	for (int32 Index = 0; Index < ChaosUtil::NumPlayers; ++Index)
	{
		LastRoundSummary.PointsAwarded[Index] = RoundPoints[Index];
	}

	if (ArenaManager)
	{
		ArenaManager->StopAllTraps();
	}
	FreezePlayers();
	PhaseEndTime = GetNow() + RoundResultAutoContinue;

	// Victory burst on the round winner.
	if (AChaosPlayerCharacter* Winner = GetPlayerCharacter(LastRoundSummary.WinnerIndex))
	{
		UChaosVisuals::SpawnPulse(this, Winner->GetActorLocation(), Winner->GetRoleColor(), 420.f, 1.1f, false, 900.f);
		UChaosVisuals::SpawnPulse(this, Winner->GetActorLocation() - FVector(0.f, 0.f, 85.f), Winner->GetRoleColor(), 600.f, 1.3f, true);
	}

	UE_LOG(LogChaosControl, Log, TEXT("Round %d over: %s wins (%s). Scores P1=%d P2=%d"),
		GetRoundNumber(), bRunnerWon ? TEXT("Runner") : TEXT("Saboteur"), *Reason, Scores[0], Scores[1]);
}

void AChaosGameMode::FinishMatch()
{
	if (!TransitionTo(EChaosMatchPhase::MatchComplete))
	{
		return;
	}
	if (ArenaManager)
	{
		ArenaManager->StopAllTraps();
	}
	FreezePlayers();

	const int32 Winner = GetMatchWinnerIndex();
	UE_LOG(LogChaosControl, Log, TEXT("Match complete: %s. Final P1=%d P2=%d"),
		Winner == INDEX_NONE ? TEXT("DRAW") : (Winner == 0 ? TEXT("Player 1 wins") : TEXT("Player 2 wins")), Scores[0], Scores[1]);
}

void AChaosGameMode::AwardPoints(int32 PlayerIndex, int32 Points)
{
	if (PlayerIndex < 0 || PlayerIndex >= ChaosUtil::NumPlayers)
	{
		return;
	}
	Scores[PlayerIndex] += Points;
	RoundPoints[PlayerIndex] += Points;
}

void AChaosGameMode::SpawnPlayers()
{
	for (int32 Index = 0; Index < ChaosUtil::NumPlayers; ++Index)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		const FTransform SpawnTransform = ArenaManager->GetSpawnTransform(GetRoleForPlayer(Index));
		AChaosPlayerCharacter* Avatar = GetWorld()->SpawnActor<AChaosPlayerCharacter>(AChaosPlayerCharacter::StaticClass(), SpawnTransform, Params);
		if (Avatar)
		{
			Avatar->InitializePlayer(Index);
			Players.Add(Avatar);
		}
		else
		{
			UE_LOG(LogChaosControl, Error, TEXT("Failed to spawn player %d"), Index + 1);
		}
	}
}

void AChaosGameMode::PlacePlayersForCurrentRound()
{
	if (!ArenaManager)
	{
		return;
	}

	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		if (AChaosPlayerCharacter* Avatar = Players[Index])
		{
			const EChaosPlayerRole NewRole = GetRoleForPlayer(Index);
			Avatar->SetRole(NewRole);
			Avatar->ResetForRound(ArenaManager->GetSpawnTransform(NewRole));
		}
	}
}

void AChaosGameMode::FreezePlayers()
{
	for (AChaosPlayerCharacter* Avatar : Players)
	{
		if (Avatar)
		{
			Avatar->FreezeMovement();
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Gameplay events
// ---------------------------------------------------------------------------------------------

bool AChaosGameMode::TryCollectCore(AChaosObjective* Core, AChaosPlayerCharacter* Collector)
{
	if (!IsRoundActive() || !Core || !Collector || !Collector->IsRunner() || Core->IsCollected())
	{
		return false;
	}

	Core->MarkCollected();
	++CoresCollected;
	AwardPoints(Collector->GetPlayerIndex(), Scoring.CorePoints);

	const int32 Required = GetCoresRequired();
	PushMessage(FString::Printf(TEXT("+%d  Energy core %d / %d"), Scoring.CorePoints, CoresCollected, Required), ChaosUtil::CoreColor);

	if (CoresCollected >= Required && ArenaManager && ArenaManager->GetExit())
	{
		AChaosExit* ExitGate = ArenaManager->GetExit();
		ExitGate->SetUnlocked(true);
		PushMessage(TEXT("All cores collected - the EXIT is open!"), ChaosUtil::SuccessColor, 3.f);

		// The Runner may already be standing in the gate.
		if (ExitGate->IsActorInside(Collector))
		{
			TryRunnerExit(Collector);
		}
	}
	return true;
}

bool AChaosGameMode::TryRunnerExit(AChaosPlayerCharacter* Visitor)
{
	if (!IsRoundActive() || !Visitor || !Visitor->IsRunner())
	{
		return false;
	}

	if (!IsExitUnlocked())
	{
		const float Now = GetNow();
		if (Now - LastLockedMessageTime > 1.5f)
		{
			LastLockedMessageTime = Now;
			PushMessage(FString::Printf(TEXT("Exit locked - collect %d more core(s)"), GetCoresRequired() - CoresCollected), ChaosUtil::DangerColor);
		}
		return false;
	}

	EndRound(true, TEXT("The Runner escaped with every energy core"));
	return true;
}

void AChaosGameMode::HandleInteract(AChaosPlayerCharacter* Actor)
{
	if (!IsRoundActive() || !Actor || !ArenaManager)
	{
		return;
	}

	const FString Label = FString::Printf(TEXT("P%d"), Actor->GetPlayerIndex() + 1);

	if (!Actor->IsRunner())
	{
		UChaosAbilityComponent* Abilities = Actor->GetAbilityComponent();
		Abilities->CycleSelection();
		const EChaosSabotageAbility Selected = Abilities->GetSelectedAbility();
		PushMessage(FString::Printf(TEXT("%s selected %s (%d energy)"), *Label, *ChaosUtil::GetAbilityName(Selected),
			FMath::RoundToInt(Abilities->GetTuning(Selected).EnergyCost)), ChaosUtil::SaboteurColor, 1.5f);
		return;
	}

	if (AChaosObjective* Core = ArenaManager->FindNearestUncollectedCore(Actor->GetActorLocation(), 220.f))
	{
		TryCollectCore(Core, Actor);
		return;
	}

	AChaosExit* ExitGate = ArenaManager->GetExit();
	if (ExitGate && FVector::Dist2D(ExitGate->GetActorLocation(), Actor->GetActorLocation()) < 300.f)
	{
		TryRunnerExit(Actor);
		return;
	}

	PushMessage(FString::Printf(TEXT("%s: nothing to interact with here"), *Label), ChaosUtil::NeutralColor, 1.2f);
}

void AChaosGameMode::HandleSabotage(AChaosPlayerCharacter* Actor)
{
	if (!IsRoundActive() || !Actor)
	{
		return;
	}

	const FString Label = FString::Printf(TEXT("P%d"), Actor->GetPlayerIndex() + 1);
	if (Actor->IsRunner())
	{
		PushMessage(FString::Printf(TEXT("%s: the Runner cannot sabotage"), *Label), ChaosUtil::NeutralColor, 1.2f);
		return;
	}

	FString Feedback;
	UChaosAbilityComponent* Abilities = Actor->GetAbilityComponent();
	const bool bActivated = Abilities->TryActivate(Abilities->GetSelectedAbility(), Feedback);
	PushMessage(FString::Printf(TEXT("%s: %s"), *Label, *Feedback), bActivated ? ChaosUtil::SaboteurColor : ChaosUtil::WarningColor, 2.f);
	if (!bActivated)
	{
		// Visible "fizzle" around the Saboteur when energy or cooldown blocks the request.
		UChaosVisuals::SpawnPulse(this, Actor->GetActorLocation() - FVector(0.f, 0.f, 80.f), ChaosUtil::DangerColor, 110.f, 0.35f, true);
	}
}

void AChaosGameMode::HandlePlayerFell(AChaosPlayerCharacter* Faller)
{
	if (!Faller || !ArenaManager)
	{
		return;
	}

	Faller->SetActorLocationAndRotation(ArenaManager->GetSpawnTransform(Faller->GetRole()).GetLocation(), FRotator::ZeroRotator,
		false, nullptr, ETeleportType::TeleportPhysics);
	Faller->FreezeMovement();

	if (Phase == EChaosMatchPhase::RoundActive)
	{
		PushMessage(FString::Printf(TEXT("P%d fell into the pit - back to the start!"), Faller->GetPlayerIndex() + 1), ChaosUtil::WarningColor);
	}
}

void AChaosGameMode::NotifyRunnerStunned(AChaosPlayerCharacter* Victim, float Duration)
{
	if (Victim)
	{
		PushMessage(FString::Printf(TEXT("P%d STUNNED for %.1fs!"), Victim->GetPlayerIndex() + 1, Duration), ChaosUtil::StunColor);
	}
}

void AChaosGameMode::PushMessage(const FString& Text, const FLinearColor& Color, float Duration)
{
	FChaosHudMessage Message;
	Message.Text = Text;
	Message.Color = Color;
	Message.ExpireTime = GetNow() + Duration;
	Messages.Add(Message);
	if (Messages.Num() > 5)
	{
		Messages.RemoveAt(0);
	}
	UE_LOG(LogChaosControl, Verbose, TEXT("HUD: %s"), *Text);
}

// ---------------------------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------------------------

AChaosPlayerCharacter* AChaosGameMode::GetPlayerCharacter(int32 PlayerIndex) const
{
	return Players.IsValidIndex(PlayerIndex) ? Players[PlayerIndex].Get() : nullptr;
}

EChaosPlayerRole AChaosGameMode::GetRoleForPlayer(int32 PlayerIndex) const
{
	return PlayerIndex == GetRunnerIndex() ? EChaosPlayerRole::Runner : EChaosPlayerRole::Saboteur;
}

int32 AChaosGameMode::GetScore(int32 PlayerIndex) const
{
	return (PlayerIndex >= 0 && PlayerIndex < ChaosUtil::NumPlayers) ? Scores[PlayerIndex] : 0;
}

float AChaosGameMode::GetPhaseTimeRemaining() const
{
	return FMath::Max(0.f, PhaseEndTime - GetNow());
}

int32 AChaosGameMode::GetCoresRequired() const
{
	return ArenaManager ? ArenaManager->GetRequiredCoreCount() : 0;
}

bool AChaosGameMode::IsExitUnlocked() const
{
	return ArenaManager && ArenaManager->GetExit() && ArenaManager->GetExit()->IsUnlocked();
}

int32 AChaosGameMode::GetMatchWinnerIndex() const
{
	if (Scores[0] == Scores[1])
	{
		return INDEX_NONE;
	}
	return Scores[0] > Scores[1] ? 0 : 1;
}

float AChaosGameMode::GetNow() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

// ---------------------------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------------------------

void AChaosGameMode::ChaosForceTimeout()
{
	if (Phase == EChaosMatchPhase::RoundActive)
	{
		RoundTimeRemaining = 0.01f;
	}
}

void AChaosGameMode::ChaosForceRunnerWin()
{
	AChaosPlayerCharacter* Runner = GetPlayerCharacter(GetRunnerIndex());
	if (!IsRoundActive() || !Runner || !ArenaManager)
	{
		return;
	}

	for (AChaosObjective* Core : ArenaManager->GetObjectives())
	{
		TryCollectCore(Core, Runner);
	}
	TryRunnerExit(Runner);
}
