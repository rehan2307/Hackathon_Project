#include "ChaosPlayerCharacter.h"
#include "ChaosAbilityComponent.h"
#include "ChaosGameMode.h"
#include "ChaosVisuals.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AChaosPlayerCharacter::AChaosPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(40.f, 90.f);
	// Players pass through each other so a Saboteur can never body-block a corridor.
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 720.f, 0.f);
	Movement->AirControl = 0.4f;
	Movement->BrakingDecelerationWalking = 2400.f;
	Movement->bRunPhysicsWithNoController = true;

	// Neither avatar is possessed by the PlayerController (one keyboard drives both).
	// A plain AIController keeps each one "locally controlled" for character movement.
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	UStaticMesh* Cylinder = CylinderMesh.Object;
	UStaticMesh* Sphere = SphereMesh.Object;
	UStaticMesh* Cube = CubeMesh.Object;
	UStaticMesh* Cone = ConeMesh.Object;

	// Capsule spans Z -90..90 around the actor origin; +X is forward.
	Torso = AddPart(TEXT("Torso"), Cylinder, FVector(0.f, 0.f, 4.f), FVector(0.6f, 0.52f, 0.72f));
	Hips = AddPart(TEXT("Hips"), Sphere, FVector(0.f, 0.f, -36.f), FVector(0.55f, 0.48f, 0.32f));
	LegLeft = AddPart(TEXT("LegLeft"), Cylinder, FVector(0.f, -13.f, -66.f), FVector(0.2f, 0.2f, 0.48f));
	LegRight = AddPart(TEXT("LegRight"), Cylinder, FVector(0.f, 13.f, -66.f), FVector(0.2f, 0.2f, 0.48f));
	Shoulders = AddPart(TEXT("Shoulders"), Sphere, FVector(0.f, 0.f, 36.f), FVector(0.62f, 0.86f, 0.3f));
	Head = AddPart(TEXT("Head"), Sphere, FVector(2.f, 0.f, 66.f), FVector(0.42f, 0.4f, 0.44f));
	Visor = AddPart(TEXT("Visor"), Cube, FVector(18.f, 0.f, 68.f), FVector(0.1f, 0.32f, 0.08f));
	ChestLight = AddPart(TEXT("ChestLight"), Cube, FVector(30.f, 0.f, 10.f), FVector(0.03f, 0.2f, 0.3f));
	Pack = AddPart(TEXT("Pack"), Cube, FVector(-30.f, 0.f, 16.f), FVector(0.24f, 0.44f, 0.48f));
	PackLight = AddPart(TEXT("PackLight"), Cube, FVector(-43.f, 0.f, 16.f), FVector(0.02f, 0.3f, 0.06f));
	GroundRing = AddPart(TEXT("GroundRing"), Cylinder, FVector(0.f, 0.f, -88.f), FVector(1.5f, 1.5f, 0.012f));

	FinLeft = AddPart(TEXT("FinLeft"), Cone, FVector(-40.f, -20.f, 40.f), FVector(0.12f, 0.3f, 0.55f), FRotator(-120.f, 0.f, -12.f));
	FinRight = AddPart(TEXT("FinRight"), Cone, FVector(-40.f, 20.f, 40.f), FVector(0.12f, 0.3f, 0.55f), FRotator(-120.f, 0.f, 12.f));

	HornLeft = AddPart(TEXT("HornLeft"), Cone, FVector(-2.f, -15.f, 92.f), FVector(0.1f, 0.1f, 0.34f), FRotator(0.f, 0.f, -22.f));
	HornRight = AddPart(TEXT("HornRight"), Cone, FVector(-2.f, 15.f, 92.f), FVector(0.1f, 0.1f, 0.34f), FRotator(0.f, 0.f, 22.f));

	DronePivot = CreateDefaultSubobject<USceneComponent>(TEXT("DronePivot"));
	DronePivot->SetupAttachment(Capsule);
	DronePivot->SetMobility(EComponentMobility::Movable);
	DronePivot->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	Drone = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Drone"));
	ChaosUtil::SetupVisualMesh(Drone, DronePivot, Sphere, FVector(62.f, 0.f, 0.f), FVector(0.16f));

	RoleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RoleLight"));
	RoleLight->SetupAttachment(Capsule);
	RoleLight->SetMobility(EComponentMobility::Movable);
	RoleLight->SetRelativeLocation(FVector(0.f, 0.f, 20.f));
	RoleLight->IntensityUnits = ELightUnits::Candelas;
	RoleLight->Intensity = 18.f;
	RoleLight->AttenuationRadius = 320.f;
	RoleLight->CastShadows = false;

	AbilityComponent = CreateDefaultSubobject<UChaosAbilityComponent>(TEXT("AbilityComponent"));
}

UStaticMeshComponent* AChaosPlayerCharacter::AddPart(const TCHAR* Name, UStaticMesh* PartMesh, const FVector& Location, const FVector& Scale, const FRotator& Rotation)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	ChaosUtil::SetupVisualMesh(Part, GetCapsuleComponent(), PartMesh, Location, Scale);
	Part->SetRelativeRotation(Rotation);
	return Part;
}

void AChaosPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->JumpZVelocity = JumpVelocity;

	// Armour: dark painted composite; joints and legs in brushed metal.
	FChaosSurfaceStyle Armour;
	Armour.Color = FLinearColor(0.06f, 0.065f, 0.075f);
	Armour.Metallic = 0.4f;
	Armour.Roughness = 0.35f;
	Armour.PanelSize = 30.f;
	Armour.SeamDarken = 0.5f;
	for (UStaticMeshComponent* Part : { Torso.Get(), Shoulders.Get(), Pack.Get(), Head.Get() })
	{
		UChaosVisuals::ApplySurface(Part, Armour);
		Part->SetCastShadow(true);
	}
	FChaosSurfaceStyle Metal;
	Metal.Color = FLinearColor(0.28f, 0.29f, 0.31f);
	Metal.Metallic = 0.9f;
	Metal.Roughness = 0.3f;
	Metal.PanelSize = 400.f;
	for (UStaticMeshComponent* Part : { Hips.Get(), LegLeft.Get(), LegRight.Get(), FinLeft.Get(), FinRight.Get() })
	{
		UChaosVisuals::ApplySurface(Part, Metal);
		Part->SetCastShadow(true);
	}

	for (UStaticMeshComponent* Part : { Visor.Get(), ChestLight.Get(), PackLight.Get(), HornLeft.Get(), HornRight.Get(), Drone.Get() })
	{
		UChaosVisuals::ApplyEmissive(Part, ChaosUtil::RunnerColor, 10.f);
	}
	UChaosVisuals::ApplyEnergy(GroundRing, ChaosUtil::RunnerColor, 2.2f, 0.f, 0.f, 0.25f);

	bVisualsReady = true;
	RefreshVisuals();
}

void AChaosPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bStunnedNow = IsStunned();
	if (bStunnedNow != bShowingStun)
	{
		bShowingStun = bStunnedNow;
		RefreshVisuals();
	}
	if (bStunnedNow)
	{
		// Electrical flicker while stunned.
		const bool bFlash = FMath::FloorToInt(GetNow() * 12.f) % 2 == 0;
		UChaosVisuals::SetScalar(Visor, TEXT("Intensity"), bFlash ? 25.f : 2.f);
	}

	if (!IsRunner())
	{
		DronePivot->AddLocalRotation(FRotator(0.f, 160.f * DeltaSeconds, 0.f));
	}

	if (GetActorLocation().Z < FallResetZ)
	{
		if (AChaosGameMode* GM = GetWorld()->GetAuthGameMode<AChaosGameMode>())
		{
			GM->HandlePlayerFell(this);
		}
	}
}

void AChaosPlayerCharacter::InitializePlayer(int32 InPlayerIndex)
{
	PlayerIndex = InPlayerIndex;
	RefreshVisuals();
}

void AChaosPlayerCharacter::SetRole(EChaosPlayerRole NewRole)
{
	PlayerRole = NewRole;
	RefreshVisuals();
}

FLinearColor AChaosPlayerCharacter::GetRoleColor() const
{
	return IsRunner() ? ChaosUtil::RunnerColor : ChaosUtil::SaboteurColor;
}

void AChaosPlayerCharacter::ApplyMoveInput(const FVector2D& Input)
{
	if (IsStunned())
	{
		return;
	}

	// The arena camera looks along +X, so screen-up is +X and screen-right is +Y.
	const FVector Direction(Input.Y, Input.X, 0.f);
	if (!Direction.IsNearlyZero())
	{
		AddMovementInput(Direction.GetClampedToMaxSize(1.f));
	}
}

void AChaosPlayerCharacter::StartJumpInput()
{
	if (!IsStunned())
	{
		Jump();
	}
}

void AChaosPlayerCharacter::StopJumpInput()
{
	StopJumping();
}

bool AChaosPlayerCharacter::ApplyStun(float Duration)
{
	const float Now = GetNow();
	if (IsStunned() || Now < ImmunityEndTime)
	{
		return false;
	}

	StunEndTime = Now + Duration;
	ImmunityEndTime = StunEndTime + StunImmunityDuration;
	FreezeMovement();
	bShowingStun = true;
	RefreshVisuals();
	return true;
}

bool AChaosPlayerCharacter::IsStunned() const
{
	return GetNow() < StunEndTime;
}

float AChaosPlayerCharacter::GetStunTimeRemaining() const
{
	return FMath::Max(0.f, StunEndTime - GetNow());
}

void AChaosPlayerCharacter::ResetForRound(const FTransform& SpawnTransform)
{
	StunEndTime = -1.f;
	ImmunityEndTime = -1.f;
	bShowingStun = false;

	SetActorLocationAndRotation(SpawnTransform.GetLocation(), SpawnTransform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	FreezeMovement();

	if (AbilityComponent)
	{
		AbilityComponent->ResetForRound();
	}
	RefreshVisuals();
}

void AChaosPlayerCharacter::FreezeMovement()
{
	GetCharacterMovement()->StopMovementImmediately();
	ConsumeMovementInputVector();
	StopJumping();
}

void AChaosPlayerCharacter::RefreshVisuals()
{
	const bool bRunner = IsRunner();
	const bool bStunned = IsStunned();
	const FLinearColor RoleColor = GetRoleColor();
	const FLinearColor AccentColor = bStunned ? ChaosUtil::StunColor : RoleColor;

	// Role silhouette parts.
	FinLeft->SetVisibility(bRunner);
	FinRight->SetVisibility(bRunner);
	HornLeft->SetVisibility(!bRunner);
	HornRight->SetVisibility(!bRunner);
	Drone->SetVisibility(!bRunner);

	RoleLight->SetLightColor(AccentColor);
	RoleLight->SetIntensity(bStunned ? 40.f : 18.f);

	if (!bVisualsReady)
	{
		return;
	}

	for (UStaticMeshComponent* Part : { Visor.Get(), ChestLight.Get(), PackLight.Get(), HornLeft.Get(), HornRight.Get(), Drone.Get(), GroundRing.Get() })
	{
		ChaosUtil::SetMeshColor(Part, AccentColor);
	}
	UChaosVisuals::SetScalar(Visor, TEXT("Intensity"), 12.f);
	UChaosVisuals::SetScalar(GroundRing, TEXT("Intensity"), bStunned ? 4.f : 2.2f);

	// A subtle role tint on the armour plates.
	const FLinearColor ArmourTint = bRunner ? FLinearColor(0.05f, 0.075f, 0.1f) : FLinearColor(0.1f, 0.05f, 0.075f);
	for (UStaticMeshComponent* Part : { Torso.Get(), Shoulders.Get(), Pack.Get() })
	{
		ChaosUtil::SetMeshColor(Part, ArmourTint);
	}
}

float AChaosPlayerCharacter::GetNow() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}
