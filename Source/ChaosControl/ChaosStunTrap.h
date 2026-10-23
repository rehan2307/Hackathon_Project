#pragma once

#include "CoreMinimal.h"
#include "ChaosTrapBase.h"
#include "ChaosStunTrap.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * Mechanism C: a floor pad that charges for ArmDelay seconds (counterplay window) and then
 * immobilises the first Runner standing on it. Each activation can stun at most once.
 */
UCLASS()
class CHAOSCONTROL_API AChaosStunTrap : public AChaosTrapBase
{
	GENERATED_BODY()

public:
	AChaosStunTrap();

	virtual void Tick(float DeltaSeconds) override;

	void ConfigurePad(const FVector2D& InSize);
	virtual void ResetTrap() override;

	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Trap", meta = (ClampMin = "0.1"))
	float StunDuration = 2.f;

	/** Warning time between activation and the pad becoming dangerous. */
	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Trap", meta = (ClampMin = "0"))
	float ArmDelay = 0.6f;

protected:
	virtual void BeginPlay() override;
	virtual void OnTrapActivated() override;
	virtual void OnTrapDeactivated() override;

	bool IsArmed() const;
	void ApplyVisualState();

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> Frame;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> Pad;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> Coil;

	/** Grows across the pad while it charges. */
	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> ChargeRing;

	/** Crackling dome shown while the pad is dangerous. */
	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> Dome;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UBoxComponent> Zone;

	FVector2D PadSize = FVector2D(300.f, 300.f);
	bool bHasStunned = false;
	bool bWasArmed = false;
};
