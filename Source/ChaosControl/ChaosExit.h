#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChaosExit.generated.h"

class UBoxComponent;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;

/** The escape gate (PRD FR-03). Locked until every core is collected; the Runner wins by entering it. */
UCLASS()
class CHAOSCONTROL_API AChaosExit : public AActor
{
	GENERATED_BODY()

public:
	AChaosExit();

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

	void SetUnlocked(bool bInUnlocked);
	bool IsUnlocked() const { return bUnlocked; }
	bool IsActorInside(const AActor* Actor) const;

protected:
	virtual void BeginPlay() override;
	void ApplyVisualState();

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> Pad;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> PadGlow;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> PostLeft;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> PostRight;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> StripLeft;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> StripRight;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> Lintel;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> LintelGlow;

	/** Energy seal across the gate while it is locked. */
	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> Seal;

	/** Light column shown once the gate opens. */
	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> Beacon;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UPointLightComponent> GateLight;

private:
	bool bUnlocked = false;
};
