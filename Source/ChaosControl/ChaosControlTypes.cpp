#include "ChaosControlTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

FString ChaosUtil::GetRoleName(EChaosPlayerRole Role)
{
	return Role == EChaosPlayerRole::Runner ? FString(TEXT("RUNNER")) : FString(TEXT("SABOTEUR"));
}

FString ChaosUtil::GetArenaName(EChaosArena Arena)
{
	switch (Arena)
	{
	case EChaosArena::NeonGrid:   return TEXT("NEON GRID");
	case EChaosArena::ReactorRun: return TEXT("REACTOR RUN");
	case EChaosArena::ChaosCore:  return TEXT("CHAOS CORE");
	}
	return TEXT("UNKNOWN");
}

FString ChaosUtil::GetArenaDescription(EChaosArena Arena)
{
	switch (Arena)
	{
	case EChaosArena::NeonGrid:   return TEXT("Compact training arena - two doors, one trench bridge");
	case EChaosArena::ReactorRun: return TEXT("Reactor core with twin lanes and two moving bridges");
	case EChaosArena::ChaosCore:  return TEXT("Narrow doors, a central chasm and every trap combined");
	}
	return FString();
}

FString ChaosUtil::GetAbilityName(EChaosSabotageAbility Ability)
{
	switch (Ability)
	{
	case EChaosSabotageAbility::LaserBarrier:   return TEXT("Laser Barrier");
	case EChaosSabotageAbility::MovingPlatform: return TEXT("Moving Platform");
	case EChaosSabotageAbility::StunTrap:       return TEXT("Stun Trap");
	}
	return TEXT("Unknown");
}

FString ChaosUtil::FormatTime(float Seconds)
{
	const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
	return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
}

EChaosActivationCheck ChaosUtil::CheckAbilityActivation(bool bRoundActive, float Energy, float EnergyCost, float CooldownRemaining)
{
	if (!bRoundActive)
	{
		return EChaosActivationCheck::RoundInactive;
	}
	if (CooldownRemaining > 0.f)
	{
		return EChaosActivationCheck::OnCooldown;
	}
	if (Energy + KINDA_SMALL_NUMBER < EnergyCost)
	{
		return EChaosActivationCheck::InsufficientEnergy;
	}
	return EChaosActivationCheck::Ok;
}

void ChaosUtil::SetMeshColor(UPrimitiveComponent* Mesh, const FLinearColor& Color)
{
	if (!Mesh)
	{
		return;
	}

	// Only edit an instance this component owns; shared scenery instances must never be tinted.
	UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
	if (!Material || Material->GetOuter() != Mesh)
	{
		Material = UMaterialInstanceDynamic::Create(Mesh->GetMaterial(0), Mesh);
		Mesh->SetMaterial(0, Material);
	}
	if (Material)
	{
		// Every Chaos master material (and the engine's BasicShapeMaterial) has a "Color" parameter.
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void ChaosUtil::SetupVisualMesh(UStaticMeshComponent* Part, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale)
{
	if (!Part)
	{
		return;
	}

	Part->SetupAttachment(Parent);
	if (Mesh)
	{
		Part->SetStaticMesh(Mesh);
	}
	Part->SetMobility(EComponentMobility::Movable);
	Part->SetRelativeLocation(Location);
	Part->SetRelativeScale3D(Scale);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	Part->CastShadow = false;
}
