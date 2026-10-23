#include "ChaosArenaGeometry.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInterface.h"

AChaosArenaGeometry::AChaosArenaGeometry()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Static);
	RootComponent = SceneRoot;
}

void AChaosArenaGeometry::AddPiece(EChaosShape Shape, const FTransform& Transform, UMaterialInterface* Material, bool bCollision, bool bVisible)
{
	UChaosVisuals* Visuals = UChaosVisuals::Get(this);
	UStaticMesh* Mesh = Visuals ? Visuals->GetMesh(Shape) : nullptr;
	if (!Mesh)
	{
		return;
	}

	const FString Key = FString::Printf(TEXT("%d|%p|%d|%d"), static_cast<int32>(Shape), Material, bCollision ? 1 : 0, bVisible ? 1 : 0);
	TObjectPtr<UInstancedStaticMeshComponent>* Existing = Batches.Find(Key);
	UInstancedStaticMeshComponent* Batch = Existing ? Existing->Get() : nullptr;

	if (!Batch)
	{
		Batch = NewObject<UInstancedStaticMeshComponent>(this);
		Batch->SetMobility(EComponentMobility::Static);
		Batch->SetupAttachment(SceneRoot);
		Batch->SetStaticMesh(Mesh);
		if (Material)
		{
			Batch->SetMaterial(0, Material);
		}
		Batch->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		Batch->SetGenerateOverlapEvents(false);
		Batch->SetCanEverAffectNavigation(false);
		Batch->SetCastShadow(bCollision);
		Batch->SetVisibility(bVisible);
		Batch->SetHiddenInGame(!bVisible);
		Batch->RegisterComponent();
		AddInstanceComponent(Batch);
		Batches.Add(Key, Batch);
	}

	Batch->AddInstance(Transform, /*bWorldSpace*/ true);
	++PieceCount;
}

// ---------------------------------------------------------------------------------------------

AChaosSpinner::AChaosSpinner()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Movable);
	RootComponent = SceneRoot;

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(SceneRoot);
	Pivot->SetMobility(EComponentMobility::Movable);
}

UStaticMeshComponent* AChaosSpinner::AddPart(EChaosShape Shape, const FTransform& RelativeTransform, UMaterialInterface* Material)
{
	UChaosVisuals* Visuals = UChaosVisuals::Get(this);
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
	Part->SetMobility(EComponentMobility::Movable);
	Part->SetupAttachment(Pivot);
	Part->SetStaticMesh(Visuals ? Visuals->GetMesh(Shape) : nullptr);
	Part->SetRelativeTransform(RelativeTransform);
	if (Material)
	{
		Part->SetMaterial(0, Material);
	}
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	Part->SetCastShadow(false);
	Part->RegisterComponent();
	AddInstanceComponent(Part);
	return Part;
}

void AChaosSpinner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	Pivot->AddLocalRotation(RotationRate * DeltaSeconds);
	if (BobAmplitude > 0.f)
	{
		Pivot->SetRelativeLocation(FVector(0.f, 0.f, FMath::Sin(Age * BobSpeed) * BobAmplitude));
	}
}
