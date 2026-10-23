#include "ChaosMovingPlatform.h"
#include "ChaosVisuals.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace ChaosPlatformLook
{
	const FLinearColor Idle(0.1f, 0.85f, 0.95f);
}

AChaosMovingPlatform::AChaosMovingPlatform()
{
	PrimaryActorTick.bCanEverTick = true;
	AbilityType = EChaosSabotageAbility::MovingPlatform;
	ActiveDuration = 6.f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Carriage = CreateDefaultSubobject<USceneComponent>(TEXT("Carriage"));
	Carriage->SetupAttachment(SceneRoot);
	Carriage->SetMobility(EComponentMobility::Movable);

	Platform = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Platform"));
	Platform->SetupAttachment(Carriage);
	Platform->SetStaticMesh(CubeMesh.Object);
	Platform->SetMobility(EComponentMobility::Movable);
	Platform->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Platform->SetGenerateOverlapEvents(false);
	Platform->SetCanEverAffectNavigation(false);

	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Edge = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Edge%d"), Index));
		ChaosUtil::SetupVisualMesh(Edge, Carriage, CubeMesh.Object, FVector::ZeroVector, FVector::OneVector);
		EdgeLights.Add(Edge);
	}

	Strut = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Strut"));
	ChaosUtil::SetupVisualMesh(Strut, Carriage, CylinderMesh.Object, FVector::ZeroVector, FVector::OneVector);

	Rail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rail"));
	ChaosUtil::SetupVisualMesh(Rail, SceneRoot, CubeMesh.Object, FVector::ZeroVector, FVector::OneVector);

	RailGlow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RailGlow"));
	ChaosUtil::SetupVisualMesh(RailGlow, SceneRoot, CubeMesh.Object, FVector::ZeroVector, FVector::OneVector);

	ConfigurePlatform(PlatformSize, MoveAxis, Amplitude);
}

void AChaosMovingPlatform::ConfigurePlatform(const FVector& InSize, const FVector& InAxis, float InAmplitude)
{
	PlatformSize = InSize;
	MoveAxis = InAxis.GetSafeNormal();
	if (MoveAxis.IsNearlyZero())
	{
		MoveAxis = FVector(0.f, 1.f, 0.f);
	}
	Amplitude = FMath::Max(0.f, InAmplitude);

	const float HalfX = PlatformSize.X * 0.5f;
	const float HalfY = PlatformSize.Y * 0.5f;
	Platform->SetRelativeLocation(FVector(0.f, 0.f, -PlatformSize.Z * 0.5f));
	Platform->SetRelativeScale3D(PlatformSize / 100.f);

	// Light strips inset along the deck edges.
	const float Inset = 12.f;
	EdgeLights[0]->SetRelativeLocation(FVector(0.f, -HalfY + Inset, 0.8f));
	EdgeLights[0]->SetRelativeScale3D(FVector((PlatformSize.X - 2.f * Inset) / 100.f, 0.08f, 0.015f));
	EdgeLights[1]->SetRelativeLocation(FVector(0.f, HalfY - Inset, 0.8f));
	EdgeLights[1]->SetRelativeScale3D(FVector((PlatformSize.X - 2.f * Inset) / 100.f, 0.08f, 0.015f));
	EdgeLights[2]->SetRelativeLocation(FVector(-HalfX + Inset, 0.f, 0.8f));
	EdgeLights[2]->SetRelativeScale3D(FVector(0.08f, (PlatformSize.Y - 2.f * Inset) / 100.f, 0.015f));
	EdgeLights[3]->SetRelativeLocation(FVector(HalfX - Inset, 0.f, 0.8f));
	EdgeLights[3]->SetRelativeScale3D(FVector(0.08f, (PlatformSize.Y - 2.f * Inset) / 100.f, 0.015f));

	// Hydraulic support running down to the rail.
	Strut->SetRelativeLocation(FVector(0.f, 0.f, -PlatformSize.Z - 70.f));
	Strut->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.4f));

	const bool bAlongY = FMath::Abs(MoveAxis.Y) >= FMath::Abs(MoveAxis.X);
	const float RailLength = 2.f * Amplitude + (bAlongY ? PlatformSize.Y : PlatformSize.X);
	Rail->SetRelativeLocation(FVector(0.f, 0.f, -PlatformSize.Z - 150.f));
	Rail->SetRelativeScale3D(bAlongY ? FVector(1.f, RailLength / 100.f, 0.3f) : FVector(RailLength / 100.f, 1.f, 0.3f));
	RailGlow->SetRelativeLocation(FVector(0.f, 0.f, -PlatformSize.Z - 134.f));
	RailGlow->SetRelativeScale3D(bAlongY ? FVector(0.12f, RailLength / 100.f, 0.03f) : FVector(RailLength / 100.f, 0.12f, 0.03f));

	CurrentOffset = 0.f;
	UpdatePlatformLocation();
}

void AChaosMovingPlatform::BeginPlay()
{
	Super::BeginPlay();

	FChaosSurfaceStyle Deck;
	Deck.Color = FLinearColor(0.2f, 0.21f, 0.23f);
	Deck.Metallic = 0.85f;
	Deck.Roughness = 0.38f;
	Deck.PanelSize = 90.f;
	Deck.SeamDarken = 0.75f;
	UChaosVisuals::ApplySurface(Platform, Deck);

	FChaosSurfaceStyle Mechanics;
	Mechanics.Color = FLinearColor(0.08f, 0.08f, 0.09f);
	Mechanics.Metallic = 0.9f;
	Mechanics.Roughness = 0.3f;
	UChaosVisuals::ApplySurface(Strut, Mechanics);
	UChaosVisuals::ApplySurface(Rail, Mechanics);

	for (UStaticMeshComponent* Edge : EdgeLights)
	{
		UChaosVisuals::ApplyEmissive(Edge, ChaosPlatformLook::Idle, 6.f);
	}
	UChaosVisuals::ApplyEmissive(RailGlow, ChaosPlatformLook::Idle, 3.f);

	ApplyVisualState();
}

void AChaosMovingPlatform::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bTrapActive)
	{
		const float Period = FMath::Max(0.5f, CyclePeriod);
		CurrentOffset = Amplitude * FMath::Sin(2.f * UE_PI * GetTimeSinceActivation() / Period);
	}
	else if (!FMath::IsNearlyZero(CurrentOffset))
	{
		CurrentOffset = FMath::FInterpConstantTo(CurrentOffset, 0.f, DeltaSeconds, ReturnSpeed);
	}
	else
	{
		return;
	}

	UpdatePlatformLocation();
}

void AChaosMovingPlatform::ResetTrap()
{
	Super::ResetTrap();
	CurrentOffset = 0.f;
	UpdatePlatformLocation();
}

FVector AChaosMovingPlatform::GetIndicatorLocation() const
{
	return Carriage->GetComponentLocation() + FVector(0.f, 0.f, 140.f);
}

void AChaosMovingPlatform::OnTrapActivated()
{
	ApplyVisualState();
	UChaosVisuals::SpawnPulse(this, Carriage->GetComponentLocation() + FVector(0.f, 0.f, 10.f), ChaosUtil::SaboteurColor, 340.f, 0.6f, true);
}

void AChaosMovingPlatform::OnTrapDeactivated()
{
	ApplyVisualState();
}

void AChaosMovingPlatform::UpdatePlatformLocation()
{
	Carriage->SetRelativeLocation(MoveAxis * CurrentOffset);
}

void AChaosMovingPlatform::ApplyVisualState()
{
	const FLinearColor EdgeColor = bTrapActive ? ChaosUtil::SaboteurColor : ChaosPlatformLook::Idle;
	for (UStaticMeshComponent* Edge : EdgeLights)
	{
		ChaosUtil::SetMeshColor(Edge, EdgeColor);
		UChaosVisuals::SetScalar(Edge, TEXT("Intensity"), bTrapActive ? 14.f : 6.f);
		UChaosVisuals::SetScalar(Edge, TEXT("PulseSpeed"), bTrapActive ? 10.f : 0.f);
		UChaosVisuals::SetScalar(Edge, TEXT("PulseAmount"), bTrapActive ? 0.45f : 0.f);
	}
	ChaosUtil::SetMeshColor(RailGlow, bTrapActive ? ChaosUtil::WarningColor : ChaosPlatformLook::Idle);
	SetStateLight(EdgeColor, bTrapActive ? 50.f : 4.f);
}
