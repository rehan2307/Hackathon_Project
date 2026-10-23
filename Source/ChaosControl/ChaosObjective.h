#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChaosObjective.generated.h"

class UPointLightComponent;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;

/** A collectible energy core (PRD FR-02). Collected once per round, restored by ResetObjective. */
UCLASS()
class CHAOSCONTROL_API AChaosObjective : public AActor
{
	GENERATED_BODY()

public:
	AChaosObjective();

	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

	bool IsCollected() const { return bCollected; }
	void MarkCollected();
	void ResetObjective();

	UPROPERTY(EditAnywhere, Category = "Chaos|Objective")
	float SpinSpeed = 120.f;

	UPROPERTY(EditAnywhere, Category = "Chaos|Objective")
	float BobHeight = 12.f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<USphereComponent> Trigger;

	/** Bobbing group: the core, its energy shell and the orbiting satellites. */
	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<USceneComponent> Floater;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> CoreMesh;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> ShellMesh;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<USceneComponent> OrbitPivot;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TArray<TObjectPtr<UStaticMeshComponent>> Satellites;

	/** Tall faint light column that marks the objective from anywhere in the arena. */
	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UStaticMeshComponent> Beacon;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UPointLightComponent> Glow;

private:
	bool bCollected = false;
	float AnimTime = 0.f;
};
