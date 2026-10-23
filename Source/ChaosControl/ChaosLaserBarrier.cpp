#include "ChaosLaserBarrier.h"
#include "ChaosVisuals.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace ChaosLaserLook
{
	constexpr int32 StringCount = 4;
	const FLinearColor Standby(1.f, 0.45f, 0.05f);
}

AChaosLaserBarrier::AChaosLaserBarrier()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	AbilityType = EChaosSabotageAbility::LaserBarrier;
	ActiveDuration = 4.f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Beam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));
	Beam->SetupAttachment(SceneRoot);
	Beam->SetStaticMesh(CubeMesh.Object);
	Beam->SetMobility(EComponentMobility::Movable);
	Beam->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->SetGenerateOverlapEvents(false);
	Beam->SetCanEverAffectNavigation(false);
	Beam->SetHiddenInGame(true);
	Beam->CastShadow = false;

	Curtain = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Curtain"));
	ChaosUtil::SetupVisualMesh(Curtain, SceneRoot, CubeMesh.Object, FVector::ZeroVector, FVector::OneVector);

	for (int32 Index = 0; Index < ChaosLaserLook::StringCount; ++Index)
	{
		UStaticMeshComponent* LaserString = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("String%d"), Index));
		ChaosUtil::SetupVisualMesh(LaserString, SceneRoot, CylinderMesh.Object, FVector::ZeroVector, FVector::OneVector);
		LaserString->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
		Strings.Add(LaserString);
	}

	EmitterA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EmitterA"));
	ChaosUtil::SetupVisualMesh(EmitterA, SceneRoot, CubeMesh.Object, FVector::ZeroVector, FVector::OneVector);

	EmitterB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EmitterB"));
	ChaosUtil::SetupVisualMesh(EmitterB, SceneRoot, CubeMesh.Object, FVector::ZeroVector, FVector::OneVector);

	CapA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CapA"));
	ChaosUtil::SetupVisualMesh(CapA, SceneRoot, CubeMesh.Object, FVector::ZeroVector, FVector::OneVector);

	CapB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CapB"));
	ChaosUtil::SetupVisualMesh(CapB, SceneRoot, CubeMesh.Object, FVector::ZeroVector, FVector::OneVector);

	FloorMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorMarker"));
	ChaosUtil::SetupVisualMesh(FloorMarker, SceneRoot, CubeMesh.Object, FVector::ZeroVector, FVector::OneVector);

	ConfigureBarrier(BarrierWidth, BarrierHeight);
}

void AChaosLaserBarrier::ConfigureBarrier(float InWidth, float InHeight)
{
	BarrierWidth = FMath::Max(50.f, InWidth);
	BarrierHeight = FMath::Max(50.f, InHeight);

	const float HalfWidth = BarrierWidth * 0.5f;
	Beam->SetRelativeLocation(FVector(0.f, 0.f, BarrierHeight * 0.5f));
	Beam->SetRelativeScale3D(FVector(0.16f, BarrierWidth / 100.f, BarrierHeight / 100.f));

	Curtain->SetRelativeLocation(FVector(0.f, 0.f, BarrierHeight * 0.5f));
	Curtain->SetRelativeScale3D(FVector(0.03f, BarrierWidth / 100.f, BarrierHeight / 100.f));

	for (int32 Index = 0; Index < Strings.Num(); ++Index)
	{
		const float Z = BarrierHeight * (Index + 0.6f) / (Strings.Num() + 0.2f);
		Strings[Index]->SetRelativeLocation(FVector(0.f, 0.f, Z));
		Strings[Index]->SetRelativeScale3D(FVector(0.05f, 0.05f, BarrierWidth / 100.f));
	}

	const FVector EmitterScale(0.34f, 0.34f, (BarrierHeight + 20.f) / 100.f);
	EmitterA->SetRelativeLocation(FVector(0.f, -HalfWidth - 17.f, BarrierHeight * 0.5f + 10.f));
	EmitterA->SetRelativeScale3D(EmitterScale);
	EmitterB->SetRelativeLocation(FVector(0.f, HalfWidth + 17.f, BarrierHeight * 0.5f + 10.f));
	EmitterB->SetRelativeScale3D(EmitterScale);

	const FVector CapScale(0.4f, 0.4f, 0.14f);
	CapA->SetRelativeLocation(FVector(0.f, -HalfWidth - 17.f, BarrierHeight + 27.f));
	CapA->SetRelativeScale3D(CapScale);
	CapB->SetRelativeLocation(FVector(0.f, HalfWidth + 17.f, BarrierHeight + 27.f));
	CapB->SetRelativeScale3D(CapScale);

	FloorMarker->SetRelativeLocation(FVector(0.f, 0.f, 1.f));
	FloorMarker->SetRelativeScale3D(FVector(0.5f, BarrierWidth / 100.f, 0.02f));

	StateLight->SetRelativeLocation(FVector(-60.f, 0.f, BarrierHeight * 0.6f));
}

bool AChaosLaserBarrier::IsBlocking() const
{
	return Beam->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
}

FVector AChaosLaserBarrier::GetIndicatorLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, BarrierHeight + 70.f);
}

void AChaosLaserBarrier::BeginPlay()
{
	Super::BeginPlay();

	FChaosSurfaceStyle Housing;
	Housing.Color = FLinearColor(0.12f, 0.12f, 0.135f);
	Housing.Metallic = 0.85f;
	Housing.Roughness = 0.35f;
	Housing.PanelSize = 40.f;
	UChaosVisuals::ApplySurface(EmitterA, Housing);
	UChaosVisuals::ApplySurface(EmitterB, Housing);

	UChaosVisuals::ApplyEnergy(Curtain, ChaosUtil::DangerColor, 1.4f, 0.f, 0.65f, 0.3f);
	for (UStaticMeshComponent* LaserString : Strings)
	{
		UChaosVisuals::ApplyEmissive(LaserString, FLinearColor(1.f, 0.08f, 0.05f), 22.f, 18.f, 0.15f);
	}
	UChaosVisuals::ApplyEmissive(CapA, ChaosLaserLook::Standby, 3.f);
	UChaosVisuals::ApplyEmissive(CapB, ChaosLaserLook::Standby, 3.f);
	UChaosVisuals::ApplyHazard(FloorMarker, ChaosLaserLook::Standby, 0.25f);

	ApplyVisualState();
}

void AChaosLaserBarrier::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bTrapActive)
	{
		return;
	}

	// Flicker shortly before shutting off so the Runner can time the crossing.
	const float Remaining = GetActiveTimeRemaining();
	const bool bShow = Remaining > ShutdownWarning || FMath::FloorToInt(Remaining * 14.f) % 2 == 0;
	SetBeamVisible(bShow);
}

void AChaosLaserBarrier::OnTrapActivated()
{
	SetActorTickEnabled(true);
	ApplyVisualState();
	UChaosVisuals::SpawnPulse(this, GetActorLocation() + FVector(0.f, 0.f, BarrierHeight * 0.5f), ChaosUtil::DangerColor, BarrierWidth * 0.55f, 0.45f, false, 250.f);
}

void AChaosLaserBarrier::OnTrapDeactivated()
{
	SetActorTickEnabled(false);
	ApplyVisualState();
}

void AChaosLaserBarrier::SetBeamVisible(bool bVisible)
{
	if (bBeamShown == bVisible)
	{
		return;
	}
	bBeamShown = bVisible;
	Curtain->SetVisibility(bVisible);
	for (UStaticMeshComponent* LaserString : Strings)
	{
		LaserString->SetVisibility(bVisible);
	}
}

void AChaosLaserBarrier::ApplyVisualState()
{
	const bool bOn = bTrapActive;

	// Gameplay blocking is independent of the flicker.
	Beam->SetCollisionEnabled(bOn ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	bBeamShown = !bOn;
	SetBeamVisible(bOn);

	const FLinearColor CapColor = bOn ? ChaosUtil::DangerColor : ChaosLaserLook::Standby;
	ChaosUtil::SetMeshColor(CapA, CapColor);
	ChaosUtil::SetMeshColor(CapB, CapColor);
	UChaosVisuals::SetScalar(CapA, TEXT("Intensity"), bOn ? 18.f : 3.f);
	UChaosVisuals::SetScalar(CapB, TEXT("Intensity"), bOn ? 18.f : 3.f);
	ChaosUtil::SetMeshColor(FloorMarker, bOn ? ChaosUtil::DangerColor : ChaosLaserLook::Standby);
	UChaosVisuals::SetScalar(FloorMarker, TEXT("Glow"), bOn ? 1.5f : 0.25f);

	SetStateLight(bOn ? ChaosUtil::DangerColor : ChaosLaserLook::Standby, bOn ? 70.f : 2.f);
}
