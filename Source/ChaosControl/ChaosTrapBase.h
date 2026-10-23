#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChaosControlTypes.h"
#include "ChaosTrapBase.generated.h"

class UPointLightComponent;
class USceneComponent;

/**
 * Shared behaviour for every environmental mechanism: validated activation, a timed
 * active window, automatic deactivation and a forced reset between rounds.
 */
UCLASS(Abstract, Config = Game)
class CHAOSCONTROL_API AChaosTrapBase : public AActor
{
	GENERATED_BODY()

public:
	AChaosTrapBase();

	/** Seconds the mechanism stays active before it resets itself. */
	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Trap", meta = (ClampMin = "0.1"))
	float ActiveDuration = 4.f;

	EChaosSabotageAbility GetAbilityType() const { return AbilityType; }
	bool IsTrapActive() const { return bTrapActive; }
	float GetActiveTimeRemaining() const;

	/** Returns false if the mechanism is already active. */
	bool ActivateTrap();
	void DeactivateTrap();

	/** Forces the inactive, home state. Called between rounds. */
	virtual void ResetTrap();

	/** World position the HUD uses for labels above this mechanism. */
	virtual FVector GetIndicatorLocation() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void OnTrapActivated() {}
	virtual void OnTrapDeactivated() {}

	float GetTimeSinceActivation() const;
	void SetStateLight(const FLinearColor& Color, float Intensity);

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UPointLightComponent> StateLight;

	EChaosSabotageAbility AbilityType = EChaosSabotageAbility::LaserBarrier;
	bool bTrapActive = false;
	float ActivationTime = 0.f;
	FTimerHandle DeactivationTimer;
};
