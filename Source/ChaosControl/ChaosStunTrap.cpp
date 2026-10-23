#include "ChaosStunTrap.h"
#include "ChaosGameMode.h"
#include "ChaosPlayerCharacter.h"
#include "ChaosVisuals.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

namespace ChaosStunLook
{
	const FLinearColor Standby(0.6f, 0.25f, 0.02f);
}

AChaosStunTrap::AChaosStunTrap()
{
	PrimaryActorTick.bCanEverTick = true;
	AbilityType = EChaosSabotageAbility::StunTrap;
	ActiveDuration = 3.5f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Frame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame"));
	ChaosUtil::SetupVisualMesh(Frame, SceneRoot, CubeMesh.Object, FVector::ZeroVector, FVector::OneVector);

	Pad = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pad"));
	ChaosUtil::SetupVisualMesh(Pad, SceneRoot, CubeMesh.Object, FVector::ZeroVector, FVector::OneVector);

	Coil = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Coil"));
	ChaosUtil::SetupVisualMesh(Coil, SceneRoot, CylinderMesh.Object, FVector::ZeroVector, FVector::OneVector);

	ChargeRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChargeRing"));
	ChaosUtil::SetupVisualMesh(ChargeRing, SceneRoot, CylinderMesh.Object, FVector::ZeroVector, FVector::OneVector);

	Dome = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Dome"));
	ChaosUtil::SetupVisualMesh(Dome, SceneRoot, SphereMesh.Object, FVector::ZeroVector, FVector::OneVector);

	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	Zone->SetupAttachment(SceneRoot);
	Zone->SetMobility(EComponentMobility::Movable);
	Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Zone->SetCollisionObjectType(ECC_WorldDynamic);
	Zone->SetCollisionResponseToAllChannels(ECR_Ignore);
	Zone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Zone->SetGenerateOverlapEvents(true);
	Zone->SetCanEverAffectNavigation(false);

	ConfigurePad(PadSize);
}

void AChaosStunTrap::ConfigurePad(const FVector2D& InSize)
{
	PadSize = InSize;
	const float Inner = FMath::Min(InSize.X, InSize.Y);

	// Hazard-striped frame with a dark plate and a circular coil in the middle.
	Frame->SetRelativeLocation(FVector(0.f, 0.f, 0.8f));
	Frame->SetRelativeScale3D(FVector(InSize.X / 100.f, InSize.Y / 100.f, 0.016f));
	Pad->SetRelativeLocation(FVector(0.f, 0.f, 1.2f));
	Pad->SetRelativeScale3D(FVector((InSize.X - 36.f) / 100.f, (InSize.Y - 36.f) / 100.f, 0.02f));
	Coil->SetRelativeLocation(FVector(0.f, 0.f, 2.6f));
	Coil->SetRelativeScale3D(FVector(Inner * 0.62f / 100.f, Inner * 0.62f / 100.f, 0.012f));
	ChargeRing->SetRelativeLocation(FVector(0.f, 0.f, 3.5f));
	Dome->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
	Dome->SetRelativeScale3D(FVector(InSize.X * 0.9f / 100.f, InSize.Y * 0.9f / 100.f, 1.3f));

	Zone->SetBoxExtent(FVector(InSize.X * 0.5f, InSize.Y * 0.5f, 80.f));
	Zone->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
}

void AChaosStunTrap::BeginPlay()
{
	Super::BeginPlay();

	UChaosVisuals::ApplyHazard(Frame, FLinearColor(1.f, 0.6f, 0.05f), 0.12f);

	FChaosSurfaceStyle Plate;
	Plate.Color = FLinearColor(0.03f, 0.03f, 0.035f);
	Plate.Metallic = 0.7f;
	Plate.Roughness = 0.4f;
	Plate.PanelSize = 50.f;
	UChaosVisuals::ApplySurface(Pad, Plate);

	UChaosVisuals::ApplyEmissive(Coil, ChaosStunLook::Standby, 2.f);
	UChaosVisuals::ApplyEnergy(ChargeRing, ChaosUtil::StunColor, 5.f, 0.f, 0.f, 0.f);
	UChaosVisuals::ApplyEnergy(Dome, ChaosUtil::StunColor, 1.8f, 0.85f, 0.6f, 0.35f);

	ApplyVisualState();
}

void AChaosStunTrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bTrapActive)
	{
		return;
	}

	ApplyVisualState();

	const bool bArmed = IsArmed();
	if (bArmed && !bWasArmed)
	{
		bWasArmed = true;
		UChaosVisuals::SpawnPulse(this, GetActorLocation() + FVector(0.f, 0.f, 10.f), ChaosUtil::StunColor, FMath::Max(PadSize.X, PadSize.Y) * 0.7f, 0.4f, true);
	}

	if (!bArmed || bHasStunned)
	{
		return;
	}

	AChaosGameMode* GM = GetWorld()->GetAuthGameMode<AChaosGameMode>();
	if (!GM || !GM->IsRoundActive())
	{
		return;
	}

	TArray<AActor*> Overlapping;
	Zone->GetOverlappingActors(Overlapping, AChaosPlayerCharacter::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		AChaosPlayerCharacter* Victim = Cast<AChaosPlayerCharacter>(Actor);
		if (Victim && Victim->IsRunner() && Victim->ApplyStun(StunDuration))
		{
			bHasStunned = true;
			GM->NotifyRunnerStunned(Victim, StunDuration);
			UChaosVisuals::SpawnPulse(this, Victim->GetActorLocation(), ChaosUtil::StunColor, 160.f, 0.5f, false, 500.f);
			break;
		}
	}
}

void AChaosStunTrap::ResetTrap()
{
	Super::ResetTrap();
	bHasStunned = false;
	bWasArmed = false;
	ApplyVisualState();
}

void AChaosStunTrap::OnTrapActivated()
{
	bHasStunned = false;
	bWasArmed = false;
	ApplyVisualState();
}

void AChaosStunTrap::OnTrapDeactivated()
{
	bWasArmed = false;
	ApplyVisualState();
}

bool AChaosStunTrap::IsArmed() const
{
	return bTrapActive && GetTimeSinceActivation() >= ArmDelay;
}

void AChaosStunTrap::ApplyVisualState()
{
	const float Inner = FMath::Min(PadSize.X, PadSize.Y);

	if (!bTrapActive)
	{
		// Standby: dim amber coil, no charge or dome.
		ChaosUtil::SetMeshColor(Coil, ChaosStunLook::Standby);
		UChaosVisuals::SetScalar(Coil, TEXT("Intensity"), 0.5f);
		ChargeRing->SetVisibility(false);
		Dome->SetVisibility(false);
		SetStateLight(ChaosStunLook::Standby, 2.f);
		return;
	}

	const float Elapsed = GetTimeSinceActivation();
	if (!IsArmed())
	{
		// Charging: fast-flashing coil and a ring sweeping outwards.
		const float Charge = ArmDelay > 0.f ? FMath::Clamp(Elapsed / ArmDelay, 0.f, 1.f) : 1.f;
		const bool bFlash = FMath::FloorToInt(Elapsed * 10.f) % 2 == 0;
		ChaosUtil::SetMeshColor(Coil, ChaosUtil::WarningColor);
		UChaosVisuals::SetScalar(Coil, TEXT("Intensity"), bFlash ? 16.f : 4.f);
		ChargeRing->SetVisibility(true);
		const float RingSize = FMath::Lerp(Inner * 0.2f, Inner * 0.95f, Charge) / 100.f;
		ChargeRing->SetRelativeScale3D(FVector(RingSize, RingSize, 0.01f));
		Dome->SetVisibility(false);
		SetStateLight(ChaosUtil::WarningColor, bFlash ? 40.f : 10.f);
		return;
	}

	// Armed: bright coil and a crackling dome; dims once it has stunned someone.
	ChaosUtil::SetMeshColor(Coil, ChaosUtil::StunColor);
	UChaosVisuals::SetScalar(Coil, TEXT("Intensity"), bHasStunned ? 5.f : 20.f);
	ChargeRing->SetVisibility(false);
	Dome->SetVisibility(!bHasStunned);
	SetStateLight(ChaosUtil::StunColor, bHasStunned ? 10.f : 55.f);
}
