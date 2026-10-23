#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "ChaosControlTypes.generated.h"

class UPrimitiveComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EChaosPlayerRole : uint8
{
	Runner,
	Saboteur
};

UENUM(BlueprintType)
enum class EChaosMatchPhase : uint8
{
	MainMenu,
	ArenaSelect,
	RoundStarting,
	RoundActive,
	RoundComplete,
	MatchComplete
};

UENUM(BlueprintType)
enum class EChaosSabotageAbility : uint8
{
	LaserBarrier,
	MovingPlatform,
	StunTrap
};

UENUM(BlueprintType)
enum class EChaosArena : uint8
{
	NeonGrid,
	ReactorRun,
	ChaosCore
};

/** Result of validating a sabotage activation request. */
enum class EChaosActivationCheck : uint8
{
	Ok,
	RoundInactive,
	OnCooldown,
	InsufficientEnergy
};

/** Energy cost and cooldown of one sabotage ability. */
USTRUCT(BlueprintType)
struct FChaosAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos", meta = (ClampMin = "0"))
	float EnergyCost = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos", meta = (ClampMin = "0"))
	float Cooldown = 8.f;
};

/** Point values for every scoring event (PRD FR-10). */
USTRUCT(BlueprintType)
struct FChaosScoringRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos")
	int32 CorePoints = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos")
	int32 ExitPoints = 300;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos")
	int32 SaboteurDefensePoints = 300;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos")
	int32 RunnerWinBonus = 200;
};

/** Keyboard keys driving one local player, whatever role they currently hold. */
USTRUCT(BlueprintType)
struct FChaosKeyBindings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos")
	FKey Up;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos")
	FKey Down;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos")
	FKey Left;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos")
	FKey Right;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos")
	FKey Jump;

	/** Runner: interact with a nearby core or the exit. Saboteur: cycle the selected mechanism. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos")
	FKey Interact;

	/** Saboteur: trigger the selected mechanism nearest to the Saboteur. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chaos")
	FKey Sabotage;
};

/** Outcome of the most recent round, shown on the round result screen. */
struct FChaosRoundSummary
{
	int32 RoundNumber = 0;
	bool bRunnerWon = false;
	int32 WinnerIndex = INDEX_NONE;
	int32 RunnerIndex = 0;
	int32 CoresCollected = 0;
	FString Reason;
	int32 PointsAwarded[2] = { 0, 0 };
};

/** A short feedback line drawn by the HUD. */
struct FChaosHudMessage
{
	FString Text;
	FLinearColor Color = FLinearColor::White;
	float ExpireTime = 0.f;
};

namespace ChaosUtil
{
	constexpr int32 NumPlayers = 2;
	constexpr int32 NumAbilities = 3;
	constexpr int32 NumArenas = 3;

	inline const FLinearColor RunnerColor(0.0f, 0.85f, 1.0f);
	inline const FLinearColor SaboteurColor(1.0f, 0.1f, 0.65f);
	inline const FLinearColor CoreColor(1.0f, 0.8f, 0.1f);
	inline const FLinearColor StunColor(1.0f, 0.95f, 0.2f);
	inline const FLinearColor DangerColor(1.0f, 0.12f, 0.08f);
	inline const FLinearColor SuccessColor(0.2f, 1.0f, 0.35f);
	inline const FLinearColor WarningColor(1.0f, 0.55f, 0.1f);
	inline const FLinearColor NeutralColor(0.78f, 0.8f, 0.88f);
	inline const FLinearColor FloorColor(0.1f, 0.11f, 0.14f);
	inline const FLinearColor WallColor(0.2f, 0.21f, 0.26f);

	CHAOSCONTROL_API FString GetRoleName(EChaosPlayerRole Role);
	CHAOSCONTROL_API FString GetArenaName(EChaosArena Arena);
	CHAOSCONTROL_API FString GetArenaDescription(EChaosArena Arena);
	CHAOSCONTROL_API FString GetAbilityName(EChaosSabotageAbility Ability);
	CHAOSCONTROL_API FString FormatTime(float Seconds);

	/** Pure validation rule shared by the ability component and the automation tests. */
	CHAOSCONTROL_API EChaosActivationCheck CheckAbilityActivation(bool bRoundActive, float Energy, float EnergyCost, float CooldownRemaining);

	/** Tints a basic-shape mesh through a dynamic instance of its material. */
	CHAOSCONTROL_API void SetMeshColor(UPrimitiveComponent* Mesh, const FLinearColor& Color);

	/** Constructor helper: attaches a non-colliding decorative mesh part. */
	CHAOSCONTROL_API void SetupVisualMesh(UStaticMeshComponent* Part, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale);
}
