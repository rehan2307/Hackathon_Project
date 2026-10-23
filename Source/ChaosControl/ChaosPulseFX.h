#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChaosPulseFX.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/** A self-destroying expanding glow used for gameplay feedback. Spawn through UChaosVisuals::SpawnPulse. */
UCLASS(NotPlaceable)
class CHAOSCONTROL_API AChaosPulseFX : public AActor
{
	GENERATED_BODY()

public:
	AChaosPulseFX();

	virtual void Tick(float DeltaSeconds) override;

	void Start(const FLinearColor& Color, float Radius, float Duration, bool bRing, float LightIntensity);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> Shell;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UPointLightComponent> Flash;

private:
	float Elapsed = 0.f;
	float TotalDuration = 0.6f;
	float MaxRadius = 100.f;
	float PeakIntensity = 4.f;
	float PeakLight = 0.f;
	bool bIsRing = false;
};
