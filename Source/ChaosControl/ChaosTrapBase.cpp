#include "ChaosTrapBase.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

AChaosTrapBase::AChaosTrapBase()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Movable);
	RootComponent = SceneRoot;

	StateLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StateLight"));
	StateLight->SetupAttachment(SceneRoot);
	StateLight->SetMobility(EComponentMobility::Movable);
	StateLight->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	StateLight->IntensityUnits = ELightUnits::Candelas;
	StateLight->Intensity = 2.f;
	StateLight->AttenuationRadius = 450.f;
	StateLight->CastShadows = false;
}

float AChaosTrapBase::GetActiveTimeRemaining() const
{
	if (!bTrapActive)
	{
		return 0.f;
	}
	return FMath::Max(0.f, ActivationTime + ActiveDuration - GetWorld()->GetTimeSeconds());
}

bool AChaosTrapBase::ActivateTrap()
{
	if (bTrapActive)
	{
		return false;
	}

	bTrapActive = true;
	ActivationTime = GetWorld()->GetTimeSeconds();
	OnTrapActivated();
	GetWorldTimerManager().SetTimer(DeactivationTimer, this, &AChaosTrapBase::DeactivateTrap, ActiveDuration, false);
	return true;
}

void AChaosTrapBase::DeactivateTrap()
{
	GetWorldTimerManager().ClearTimer(DeactivationTimer);
	if (!bTrapActive)
	{
		return;
	}

	bTrapActive = false;
	OnTrapDeactivated();
}

void AChaosTrapBase::ResetTrap()
{
	DeactivateTrap();
}

FVector AChaosTrapBase::GetIndicatorLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, 150.f);
}

void AChaosTrapBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}

float AChaosTrapBase::GetTimeSinceActivation() const
{
	return GetWorld()->GetTimeSeconds() - ActivationTime;
}

void AChaosTrapBase::SetStateLight(const FLinearColor& Color, float Intensity)
{
	StateLight->SetLightColor(Color);
	StateLight->SetIntensity(Intensity);
}
