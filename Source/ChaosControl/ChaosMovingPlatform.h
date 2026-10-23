#pragma once

#include "CoreMinimal.h"
#include "ChaosTrapBase.h"
#include "ChaosMovingPlatform.generated.h"

class UStaticMeshComponent;

/**
 * Mechanism B: a bridge across a pit. While idle it sits still; when the Saboteur activates it,
 * it oscillates along its rail so the Runner has to time the crossing. It glides home afterwards.
 */
UCLASS()
class CHAOSCONTROL_API AChaosMovingPlatform : public AChaosTrapBase
{
	GENERATED_BODY()

public:
	AChaosMovingPlatform();

	virtual void Tick(float DeltaSeconds) override;

	/** Size is the full platform extent; its top surface sits at the actor's Z. */
	void ConfigurePlatform(const FVector& InSize, const FVector& InAxis, float InAmplitude);
	float GetCurrentOffset() const { return CurrentOffset; }

	virtual void ResetTrap() override;
	virtual FVector GetIndicatorLocation() const override;

	/** Seconds for one full back-and-forth swing while active. */
	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Trap", meta = (ClampMin = "0.5"))
	float CyclePeriod = 3.f;

	/** Speed (cm/s) at which the platform glides back to its home position. */
	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Trap", meta = (ClampMin = "50"))
	float ReturnSpeed = 500.f;

protected:
	virtual void BeginPlay() override;
	virtual void OnTrapActivated() override;
	virtual void OnTrapDeactivated() override;

	void UpdatePlatformLocation();
	void ApplyVisualState();

	/** Moving part: everything attached to it travels with the deck. */
	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<USceneComponent> Carriage;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> Platform;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TArray<TObjectPtr<UStaticMeshComponent>> EdgeLights;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> Strut;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> Rail;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> RailGlow;

	FVector PlatformSize = FVector(500.f, 350.f, 40.f);
	FVector MoveAxis = FVector(0.f, 1.f, 0.f);
	float Amplitude = 400.f;
	float CurrentOffset = 0.f;
};
