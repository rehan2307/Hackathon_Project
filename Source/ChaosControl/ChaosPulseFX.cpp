#include "ChaosPulseFX.h"
#include "ChaosVisuals.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"

AChaosPulseFX::AChaosPulseFX()
{
	PrimaryActorTick.bCanEverTick = true;

	Shell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell"));
	RootComponent = Shell;
	Shell->SetMobility(EComponentMobility::Movable);
	Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shell->SetGenerateOverlapEvents(false);
	Shell->SetCanEverAffectNavigation(false);
	Shell->CastShadow = false;

	Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
	Flash->SetupAttachment(Shell);
	Flash->SetMobility(EComponentMobility::Movable);
	Flash->IntensityUnits = ELightUnits::Candelas;
	Flash->Intensity = 0.f;
	Flash->CastShadows = false;
	Flash->SetAbsolute(false, false, true);
}

void AChaosPulseFX::Start(const FLinearColor& Color, float Radius, float Duration, bool bRing, float LightIntensity)
{
	TotalDuration = FMath::Max(0.05f, Duration);
	MaxRadius = Radius;
	bIsRing = bRing;
	PeakLight = LightIntensity;
	PeakIntensity = bRing ? 6.f : 3.f;

	if (UChaosVisuals* Visuals = UChaosVisuals::Get(this))
	{
		Shell->SetStaticMesh(Visuals->GetMesh(bRing ? EChaosShape::Cylinder : EChaosShape::Sphere));
	}
	UChaosVisuals::ApplyEnergy(Shell, Color, PeakIntensity, bRing ? 0.f : 0.85f, 0.f, 0.f);

	Flash->SetLightColor(Color);
	Flash->SetAttenuationRadius(Radius * 3.f);
	Flash->SetIntensity(LightIntensity);
	Flash->SetVisibility(LightIntensity > 0.f);

	SetActorScale3D(FVector(0.01f));
}

void AChaosPulseFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Elapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(Elapsed / TotalDuration, 0.f, 1.f);
	if (Alpha >= 1.f)
	{
		Destroy();
		return;
	}

	// Fast ease-out growth, quadratic fade.
	const float Growth = 1.f - FMath::Square(1.f - Alpha);
	const float Fade = FMath::Square(1.f - Alpha);
	const float Diameter = FMath::Max(1.f, MaxRadius * 2.f * Growth) / 100.f;
	SetActorScale3D(bIsRing ? FVector(Diameter, Diameter, 0.04f) : FVector(Diameter));

	UChaosVisuals::SetScalar(Shell, TEXT("Intensity"), PeakIntensity * Fade);
	if (PeakLight > 0.f)
	{
		Flash->SetIntensity(PeakLight * Fade);
	}
}
