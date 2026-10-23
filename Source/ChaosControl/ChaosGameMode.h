#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ChaosControlTypes.h"
#include "ChaosGameMode.generated.h"

class AChaosArenaManager;
class AChaosObjective;
class AChaosPlayerCharacter;

/**
 * Owns the match: phase state machine (FR-12), round lifecycle and timer (FR-09),
 * role assignment and switching (FR-08), win conditions (FR-03) and scoring (FR-10).
 */
UCLASS(Config = Game)
class CHAOSCONTROL_API AChaosGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AChaosGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;

	// ---- Tuning -----------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Match", meta = (ClampMin = "10"))
	float RoundDuration = 90.f;

	/** Two rounds by default so each player plays both roles. */
	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Match", meta = (ClampMin = "1"))
	int32 RoundsPerMatch = 2;

	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Match", meta = (ClampMin = "0"))
	float RoundIntroDuration = 3.f;

	/** Seconds before the result screen continues on its own. 0 waits for Continue. */
	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Match", meta = (ClampMin = "0"))
	float RoundResultAutoContinue = 12.f;

	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Scoring")
	FChaosScoringRules Scoring;

	// ---- Menu / flow ------------------------------------------------------------------------

	void OpenArenaSelect();
	void CloseArenaSelect();
	void PreviewArena(EChaosArena Arena);
	void StartMatch();
	void StartMatchOnArena(EChaosArena Arena);
	void ContinueAfterRound();
	void PlayAgain();
	void RestartMatch();
	void ReturnToMainMenu();
	void SetMenuPaused(bool bPaused);
	bool IsMenuPaused() const { return bMenuPaused; }

	// ---- Gameplay events --------------------------------------------------------------------

	bool TryCollectCore(AChaosObjective* Core, AChaosPlayerCharacter* Collector);
	bool TryRunnerExit(AChaosPlayerCharacter* Visitor);
	void HandleInteract(AChaosPlayerCharacter* Actor);
	void HandleSabotage(AChaosPlayerCharacter* Actor);
	void HandlePlayerFell(AChaosPlayerCharacter* Faller);
	void NotifyRunnerStunned(AChaosPlayerCharacter* Victim, float Duration);
	void PushMessage(const FString& Text, const FLinearColor& Color, float Duration = 2.5f);

	// ---- Queries ----------------------------------------------------------------------------

	EChaosMatchPhase GetPhase() const { return Phase; }
	bool IsRoundActive() const { return Phase == EChaosMatchPhase::RoundActive && !bMenuPaused; }
	AChaosPlayerCharacter* GetPlayerCharacter(int32 PlayerIndex) const;
	int32 GetRunnerIndex() const { return CurrentRound % ChaosUtil::NumPlayers; }
	int32 GetSaboteurIndex() const { return 1 - GetRunnerIndex(); }
	EChaosPlayerRole GetRoleForPlayer(int32 PlayerIndex) const;
	int32 GetScore(int32 PlayerIndex) const;
	int32 GetRoundNumber() const { return CurrentRound + 1; }
	int32 GetRoundsPerMatch() const { return FMath::Max(1, RoundsPerMatch); }
	bool IsFinalRound() const { return GetRoundNumber() >= GetRoundsPerMatch(); }
	float GetRoundTimeRemaining() const { return RoundTimeRemaining; }
	float GetPhaseTimeRemaining() const;
	int32 GetCoresCollected() const { return CoresCollected; }
	int32 GetCoresRequired() const;
	bool IsExitUnlocked() const;
	EChaosArena GetSelectedArena() const { return SelectedArena; }
	AChaosArenaManager* GetArenaManager() const { return ArenaManager; }
	const FChaosRoundSummary& GetLastRoundSummary() const { return LastRoundSummary; }
	/** Index of the player with the higher total score, or INDEX_NONE for a draw. */
	int32 GetMatchWinnerIndex() const;
	const TArray<FChaosHudMessage>& GetMessages() const { return Messages; }
	float GetNow() const;

	static bool IsTransitionAllowed(EChaosMatchPhase From, EChaosMatchPhase To);

	// ---- Debug (console) --------------------------------------------------------------------

	/** Console: ends the current round as if the timer had expired. */
	UFUNCTION(Exec)
	void ChaosForceTimeout();

	/** Console: collects every core and moves the Runner into the exit. */
	UFUNCTION(Exec)
	void ChaosForceRunnerWin();

private:
	bool TransitionTo(EChaosMatchPhase NewPhase);
	void SpawnPlayers();
	void PlacePlayersForCurrentRound();
	void BeginRound();
	void ActivateRound();
	void EndRound(bool bRunnerWon, const FString& Reason);
	void FinishMatch();
	void AwardPoints(int32 PlayerIndex, int32 Points);
	void FreezePlayers();

	UPROPERTY()
	TObjectPtr<AChaosArenaManager> ArenaManager;

	UPROPERTY()
	TArray<TObjectPtr<AChaosPlayerCharacter>> Players;

	EChaosMatchPhase Phase = EChaosMatchPhase::MainMenu;
	EChaosArena SelectedArena = EChaosArena::NeonGrid;
	int32 CurrentRound = 0;
	int32 Scores[ChaosUtil::NumPlayers] = { 0, 0 };
	int32 RoundPoints[ChaosUtil::NumPlayers] = { 0, 0 };
	int32 CoresCollected = 0;
	float RoundTimeRemaining = 0.f;
	float PhaseEndTime = 0.f;
	float LastLockedMessageTime = -100.f;
	bool bMenuPaused = false;
	FChaosRoundSummary LastRoundSummary;
	TArray<FChaosHudMessage> Messages;
};
