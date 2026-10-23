#include "ChaosObjective.h"
#include "ChaosControlTypes.h"
#include "ChaosGameMode.h"
#include "ChaosPlayerCharacter.h"
#include "ChaosVisuals.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AChaosObjective::AChaosObjective()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Movable);
	RootComponent = SceneRoot;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(SceneRoot);
	Trigger->InitSphereRadius(90.f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Floater = CreateDefaultSubobject<USceneComponent>(TEXT("Floater"));
	Floater->SetupAttachment(SceneRoot);
	Floater->SetMobility(EComponentMobility::Movable);

	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	ChaosUtil::SetupVisualMesh(CoreMesh, Floater, SphereMesh.Object, FVector::ZeroVector, FVector(0.42f));

	ShellMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShellMesh"));
	ChaosUtil::SetupVisualMesh(ShellMesh, Floater, SphereMesh.Object, FVector::ZeroVector, FVector(0.78f));

	OrbitPivot = CreateDefaultSubobject<USceneComponent>(TEXT("OrbitPivot"));
	OrbitPivot->SetupAttachment(Floater);
	OrbitPivot->SetMobility(EComponentMobility::Movable);
	OrbitPivot->SetRelativeRotation(FRotator(20.f, 0.f, 15.f));

	for (int32 Index = 0; Index < 3; ++Index)
	{
		UStaticMeshComponent* Satellite = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Satellite%d"), Index));
		const FVector Offset = FRotator(0.f, Index * 120.f, 0.f).RotateVector(FVector(58.f, 0.f, 0.f));
		ChaosUtil::SetupVisualMesh(Satellite, OrbitPivot, SphereMesh.Object, Offset, FVector(0.1f));
		Satellites.Add(Satellite);
	}

	Beacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));
	ChaosUtil::SetupVisualMesh(Beacon, SceneRoot, CylinderMesh.Object, FVector(0.f, 0.f, 170.f), FVector(0.16f, 0.16f, 2.6f));

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(SceneRoot);
	Glow->SetMobility(EComponentMobility::Movable);
	Glow->IntensityUnits = ELightUnits::Candelas;
	Glow->Intensity = 30.f;
	Glow->AttenuationRadius = 450.f;
	Glow->CastShadows = false;
	Glow->LightColor = ChaosUtil::CoreColor.ToFColor(true);
}

void AChaosObjective::BeginPlay()
{
	Super::BeginPlay();

	const FLinearColor Gold = ChaosUtil::CoreColor;
	UChaosVisuals::ApplyEmissive(CoreMesh, Gold, 9.f, 3.f, 0.25f);
	UChaosVisuals::ApplyEnergy(ShellMesh, FLinearColor(1.f, 0.6f, 0.05f), 1.6f, 0.92f, 0.3f, 0.2f);
	for (UStaticMeshComponent* Satellite : Satellites)
	{
		UChaosVisuals::ApplyEmissive(Satellite, FLinearColor(1.f, 0.95f, 0.7f), 12.f);
	}
	UChaosVisuals::ApplyEnergy(Beacon, Gold, 0.5f, 0.f, 0.7f, 0.15f);

	// Desynchronise idle animation between cores.
	AnimTime = FMath::FRandRange(0.f, 10.f);
}

void AChaosObjective::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bCollected)
	{
		return;
	}

	AnimTime += DeltaSeconds;
	Floater->SetRelativeLocation(FVector(0.f, 0.f, FMath::Sin(AnimTime * 2.2f) * BobHeight));
	CoreMesh->SetRelativeRotation(FRotator(25.f, AnimTime * SpinSpeed, 0.f));
	OrbitPivot->AddLocalRotation(FRotator(0.f, SpinSpeed * 1.6f * DeltaSeconds, 0.f));
	Glow->SetIntensity(30.f + 8.f * FMath::Sin(AnimTime * 3.f));
}

void AChaosObjective::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (AChaosPlayerCharacter* Collector = Cast<AChaosPlayerCharacter>(OtherActor))
	{
		if (AChaosGameMode* GM = GetWorld()->GetAuthGameMode<AChaosGameMode>())
		{
			GM->TryCollectCore(this, Collector);
		}
	}
}

void AChaosObjective::MarkCollected()
{
	bCollected = true;
	SetActorHiddenInGame(true);
	Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Glow->SetVisibility(false);

	// Collection burst: expanding golden shell, a floor shock ring and a light flash.
	UChaosVisuals::SpawnPulse(this, GetActorLocation(), ChaosUtil::CoreColor, 220.f, 0.7f, false, 600.f);
	UChaosVisuals::SpawnPulse(this, GetActorLocation() - FVector(0.f, 0.f, 60.f), FLinearColor(1.f, 0.6f, 0.05f), 320.f, 0.9f, true);
}

void AChaosObjective::ResetObjective()
{
	bCollected = false;
	SetActorHiddenInGame(false);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Glow->SetVisibility(true);
}
