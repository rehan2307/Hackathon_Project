#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ChaosControlTypes.h"
#include "ChaosPlayerCharacter.generated.h"

class UChaosAbilityComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * A local player avatar. Both avatars share one keyboard, so input is routed to them by
 * AChaosPlayerController; the same character plays the Runner or the Saboteur role.
 * The body is assembled from engine shapes with role-specific silhouette parts.
 */
UCLASS(Config = Game)
class CHAOSCONTROL_API AChaosPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AChaosPlayerCharacter();

	virtual void Tick(float DeltaSeconds) override;

	void InitializePlayer(int32 InPlayerIndex);
	int32 GetPlayerIndex() const { return PlayerIndex; }

	void SetRole(EChaosPlayerRole NewRole);
	EChaosPlayerRole GetRole() const { return PlayerRole; }
	bool IsRunner() const { return PlayerRole == EChaosPlayerRole::Runner; }
	FLinearColor GetRoleColor() const;

	UChaosAbilityComponent* GetAbilityComponent() const { return AbilityComponent; }

	/** Screen-space movement input: X = right, Y = up (away from the camera). */
	void ApplyMoveInput(const FVector2D& Input);
	void StartJumpInput();
	void StopJumpInput();

	/** Immobilises the character. Returns false while already stunned or still immune. */
	bool ApplyStun(float Duration);
	bool IsStunned() const;
	float GetStunTimeRemaining() const;

	/** Teleports to a spawn point and clears all per-round state (stun, abilities, velocity). */
	void ResetForRound(const FTransform& SpawnTransform);
	void FreezeMovement();

	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Movement", meta = (ClampMin = "100"))
	float WalkSpeed = 550.f;

	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Movement", meta = (ClampMin = "0"))
	float JumpVelocity = 400.f;

	/** Grace period after a stun ends during which the Runner cannot be stunned again. */
	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Movement", meta = (ClampMin = "0"))
	float StunImmunityDuration = 1.5f;

	/** Characters below this height fell into a pit and are returned to their spawn. */
	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Movement")
	float FallResetZ = -500.f;

protected:
	virtual void BeginPlay() override;

	UStaticMeshComponent* AddPart(const TCHAR* Name, UStaticMesh* PartMesh, const FVector& Location, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator);
	void RefreshVisuals();
	float GetNow() const;

	// Shared body.
	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<UStaticMeshComponent> Torso;

	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<UStaticMeshComponent> Hips;

	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<UStaticMeshComponent> LegLeft;

	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<UStaticMeshComponent> LegRight;

	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<UStaticMeshComponent> Shoulders;

	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<UStaticMeshComponent> Visor;

	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<UStaticMeshComponent> ChestLight;

	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<UStaticMeshComponent> Pack;

	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<UStaticMeshComponent> PackLight;

	/** Role-coloured ring on the floor: readable at a glance from the overhead camera. */
	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<UStaticMeshComponent> GroundRing;

	// Runner silhouette.
	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<UStaticMeshComponent> FinLeft;

	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<UStaticMeshComponent> FinRight;

	// Saboteur silhouette.
	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<UStaticMeshComponent> HornLeft;

	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<UStaticMeshComponent> HornRight;

	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<USceneComponent> DronePivot;

	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<UStaticMeshComponent> Drone;

	UPROPERTY(VisibleAnywhere, Category = "Chaos|Visuals")
	TObjectPtr<UPointLightComponent> RoleLight;

	UPROPERTY(VisibleAnywhere, Category = "Chaos")
	TObjectPtr<UChaosAbilityComponent> AbilityComponent;

private:
	int32 PlayerIndex = 0;
	EChaosPlayerRole PlayerRole = EChaosPlayerRole::Runner;
	float StunEndTime = -1.f;
	float ImmunityEndTime = -1.f;
	bool bShowingStun = false;
	bool bVisualsReady = false;
};
