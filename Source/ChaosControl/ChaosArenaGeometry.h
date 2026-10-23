#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChaosVisuals.h"
#include "ChaosArenaGeometry.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;

/**
 * Holds every static piece of an arena (floors, walls, trims, decoration) as instanced meshes,
 * batched per mesh + material + collision. Hundreds of pieces cost only a few draw calls.
 */
UCLASS(NotPlaceable)
class CHAOSCONTROL_API AChaosArenaGeometry : public AActor
{
	GENERATED_BODY()

public:
	AChaosArenaGeometry();

	void AddPiece(EChaosShape Shape, const FTransform& Transform, UMaterialInterface* Material, bool bCollision, bool bVisible = true);
	int32 GetPieceCount() const { return PieceCount; }

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> Batches;

	int32 PieceCount = 0;
};

/** A slowly rotating group of decorative meshes (orbit rings, reactor machinery). */
UCLASS(NotPlaceable)
class CHAOSCONTROL_API AChaosSpinner : public AActor
{
	GENERATED_BODY()

public:
	AChaosSpinner();

	virtual void Tick(float DeltaSeconds) override;

	UStaticMeshComponent* AddPart(EChaosShape Shape, const FTransform& RelativeTransform, UMaterialInterface* Material);

	FRotator RotationRate = FRotator(0.f, 30.f, 0.f);
	float BobAmplitude = 0.f;
	float BobSpeed = 1.f;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<USceneComponent> Pivot;

	float Age = 0.f;
};
