#include "ChaosExit.h"
#include "ChaosControlTypes.h"
#include "ChaosGameMode.h"
#include "ChaosPlayerCharacter.h"
#include "ChaosVisuals.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AChaosExit::AChaosExit()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Movable);
	RootComponent = SceneRoot;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(SceneRoot);
	Trigger->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Trigger->InitBoxExtent(FVector(130.f, 130.f, 120.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cube = CubeMesh.Object;

	Pad = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pad"));
	ChaosUtil::SetupVisualMesh(Pad, SceneRoot, Cube, FVector(0.f, 0.f, 1.5f), FVector(3.2f, 3.2f, 0.03f));

	PadGlow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadGlow"));
	ChaosUtil::SetupVisualMesh(PadGlow, SceneRoot, CylinderMesh.Object, FVector(0.f, 0.f, 3.2f), FVector(2.5f, 2.5f, 0.01f));

	PostLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PostLeft"));
	ChaosUtil::SetupVisualMesh(PostLeft, SceneRoot, Cube, FVector(0.f, -165.f, 140.f), FVector(0.5f, 0.4f, 2.8f));

	PostRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PostRight"));
	ChaosUtil::SetupVisualMesh(PostRight, SceneRoot, Cube, FVector(0.f, 165.f, 140.f), FVector(0.5f, 0.4f, 2.8f));

	StripLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StripLeft"));
	ChaosUtil::SetupVisualMesh(StripLeft, SceneRoot, Cube, FVector(-26.f, -165.f, 140.f), FVector(0.03f, 0.1f, 2.4f));

	StripRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StripRight"));
	ChaosUtil::SetupVisualMesh(StripRight, SceneRoot, Cube, FVector(-26.f, 165.f, 140.f), FVector(0.03f, 0.1f, 2.4f));

	Lintel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lintel"));
	ChaosUtil::SetupVisualMesh(Lintel, SceneRoot, Cube, FVector(0.f, 0.f, 295.f), FVector(0.55f, 3.9f, 0.35f));

	LintelGlow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LintelGlow"));
	ChaosUtil::SetupVisualMesh(LintelGlow, SceneRoot, Cube, FVector(-29.f, 0.f, 295.f), FVector(0.03f, 3.0f, 0.12f));

	Seal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Seal"));
	ChaosUtil::SetupVisualMesh(Seal, SceneRoot, Cube, FVector(0.f, 0.f, 140.f), FVector(0.04f, 2.9f, 2.7f));

	Beacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));
	ChaosUtil::SetupVisualMesh(Beacon, SceneRoot, CylinderMesh.Object, FVector(0.f, 0.f, 200.f), FVector(1.5f, 1.5f, 4.f));

	GateLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GateLight"));
	GateLight->SetupAttachment(SceneRoot);
	GateLight->SetMobility(EComponentMobility::Movable);
	GateLight->SetRelativeLocation(FVector(-80.f, 0.f, 200.f));
	GateLight->IntensityUnits = ELightUnits::Candelas;
	GateLight->Intensity = 25.f;
	GateLight->AttenuationRadius = 700.f;
	GateLight->CastShadows = false;
}

void AChaosExit::BeginPlay()
{
	Super::BeginPlay();

	FChaosSurfaceStyle Frame;
	Frame.Color = FLinearColor(0.32f, 0.33f, 0.36f);
	Frame.Metallic = 0.9f;
	Frame.Roughness = 0.3f;
	Frame.PanelSize = 60.f;
	UChaosVisuals::ApplySurface(PostLeft, Frame);
	UChaosVisuals::ApplySurface(PostRight, Frame);
	UChaosVisuals::ApplySurface(Lintel, Frame);

	FChaosSurfaceStyle Floor;
	Floor.Color = FLinearColor(0.03f, 0.032f, 0.036f);
	Floor.PanelSize = 80.f;
	UChaosVisuals::ApplySurface(Pad, Floor);

	UChaosVisuals::ApplyEmissive(PadGlow, ChaosUtil::DangerColor, 3.f, 2.f, 0.3f);
	UChaosVisuals::ApplyEmissive(StripLeft, ChaosUtil::DangerColor, 8.f);
	UChaosVisuals::ApplyEmissive(StripRight, ChaosUtil::DangerColor, 8.f);
	UChaosVisuals::ApplyEmissive(LintelGlow, ChaosUtil::DangerColor, 8.f);
	UChaosVisuals::ApplyEnergy(Seal, ChaosUtil::DangerColor, 1.8f, 0.f, 0.6f, 0.25f);
	UChaosVisuals::ApplyEnergy(Beacon, ChaosUtil::SuccessColor, 0.5f, 0.6f, 0.6f, 0.2f);

	ApplyVisualState();
}

void AChaosExit::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (AChaosPlayerCharacter* Visitor = Cast<AChaosPlayerCharacter>(OtherActor))
	{
		if (AChaosGameMode* GM = GetWorld()->GetAuthGameMode<AChaosGameMode>())
		{
			GM->TryRunnerExit(Visitor);
		}
	}
}

void AChaosExit::SetUnlocked(bool bInUnlocked)
{
	const bool bJustOpened = bInUnlocked && !bUnlocked;
	bUnlocked = bInUnlocked;
	ApplyVisualState();

	if (bJustOpened && HasActorBegunPlay())
	{
		UChaosVisuals::SpawnPulse(this, GetActorLocation() + FVector(0.f, 0.f, 140.f), ChaosUtil::SuccessColor, 320.f, 0.8f, false, 800.f);
	}
}

bool AChaosExit::IsActorInside(const AActor* Actor) const
{
	return Actor && Trigger->IsOverlappingActor(Actor);
}

void AChaosExit::ApplyVisualState()
{
	const FLinearColor StateColor = bUnlocked ? ChaosUtil::SuccessColor : ChaosUtil::DangerColor;

	ChaosUtil::SetMeshColor(PadGlow, StateColor);
	ChaosUtil::SetMeshColor(StripLeft, StateColor);
	ChaosUtil::SetMeshColor(StripRight, StateColor);
	ChaosUtil::SetMeshColor(LintelGlow, StateColor);
	UChaosVisuals::SetScalar(PadGlow, TEXT("Intensity"), bUnlocked ? 6.f : 2.f);
	Seal->SetVisibility(!bUnlocked);
	Beacon->SetVisibility(bUnlocked);

	GateLight->SetLightColor(StateColor);
	GateLight->SetIntensity(bUnlocked ? 90.f : 20.f);
}
