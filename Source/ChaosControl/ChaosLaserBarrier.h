#pragma once

#include "CoreMinimal.h"
#include "ChaosTrapBase.h"
#include "ChaosLaserBarrier.generated.h"

class UStaticMeshComponent;

/** Mechanism A: a beam that temporarily blocks a doorway, forcing an alternative route. */
UCLASS()
class CHAOSCONTROL_API AChaosLaserBarrier : public AChaosTrapBase
{
	GENERATED_BODY()

public:
	AChaosLaserBarrier();

	virtual void Tick(float DeltaSeconds) override;

	/** Width is measured along the actor's local Y axis. */
	void ConfigureBarrier(float InWidth, float InHeight);
	bool IsBlocking() const;

	virtual FVector GetIndicatorLocation() const override;

	/** Seconds before switching off during which the beam flickers as a warning. */
	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Trap", meta = (ClampMin = "0"))
	float ShutdownWarning = 0.8f;

protected:
	virtual void BeginPlay() override;
	virtual void OnTrapActivated() override;
	virtual void OnTrapDeactivated() override;

	void ApplyVisualState();
	void SetBeamVisible(bool bVisible);

	/** Invisible blocking volume; the visible beam is drawn by the curtain and strings. */
	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> Beam;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> Curtain;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TArray<TObjectPtr<UStaticMeshComponent>> Strings;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> EmitterA;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> EmitterB;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> CapA;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> CapB;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> FloorMarker;

	float BarrierWidth = 300.f;
	float BarrierHeight = 200.f;
	bool bBeamShown = false;
};
