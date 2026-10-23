#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ChaosControlTypes.h"
#include "ChaosAbilityComponent.generated.h"

/**
 * Sabotage resource and cooldown tracking (PRD FR-04 / FR-07).
 * Activation triggers the idle mechanism of the requested type that is closest to the Saboteur.
 */
UCLASS(Config = Game, ClassGroup = (Chaos), meta = (BlueprintSpawnableComponent))
class CHAOSCONTROL_API UChaosAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UChaosAbilityComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Sabotage", meta = (ClampMin = "1"))
	float MaxEnergy = 100.f;

	/** Disabled (0) for the MVP as specified by FR-07. */
	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Sabotage", meta = (ClampMin = "0"))
	float EnergyRegenPerSecond = 0.f;

	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Sabotage")
	FChaosAbilityTuning LaserTuning;

	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Sabotage")
	FChaosAbilityTuning PlatformTuning;

	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Sabotage")
	FChaosAbilityTuning StunTuning;

	/** Refills energy, clears cooldowns and resets the selection. */
	void ResetForRound();

	void CycleSelection();
	EChaosSabotageAbility GetSelectedAbility() const { return SelectedAbility; }

	/** Validates the request and triggers a mechanism. OutFeedback always explains the result. */
	bool TryActivate(EChaosSabotageAbility Ability, FString& OutFeedback);

	float GetEnergy() const { return Energy; }
	float GetCooldownRemaining(EChaosSabotageAbility Ability) const;
	const FChaosAbilityTuning& GetTuning(EChaosSabotageAbility Ability) const;
	bool HasEnergyFor(EChaosSabotageAbility Ability) const;

private:
	float GetNow() const;

	float Energy = 100.f;
	EChaosSabotageAbility SelectedAbility = EChaosSabotageAbility::LaserBarrier;
	float CooldownEndTimes[ChaosUtil::NumAbilities] = { -1.f, -1.f, -1.f };
};
