#include "ChaosAbilityComponent.h"
#include "ChaosArenaManager.h"
#include "ChaosGameMode.h"
#include "ChaosPlayerCharacter.h"
#include "ChaosTrapBase.h"
#include "Engine/World.h"

UChaosAbilityComponent::UChaosAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	LaserTuning.EnergyCost = 35.f;
	LaserTuning.Cooldown = 8.f;
	PlatformTuning.EnergyCost = 25.f;
	PlatformTuning.Cooldown = 6.f;
	StunTuning.EnergyCost = 40.f;
	StunTuning.Cooldown = 10.f;
}

void UChaosAbilityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (EnergyRegenPerSecond <= 0.f)
	{
		return;
	}

	const AChaosGameMode* GM = GetWorld()->GetAuthGameMode<AChaosGameMode>();
	const AChaosPlayerCharacter* OwnerCharacter = Cast<AChaosPlayerCharacter>(GetOwner());
	if (GM && GM->IsRoundActive() && OwnerCharacter && !OwnerCharacter->IsRunner())
	{
		Energy = FMath::Min(MaxEnergy, Energy + EnergyRegenPerSecond * DeltaTime);
	}
}

void UChaosAbilityComponent::ResetForRound()
{
	Energy = MaxEnergy;
	for (float& EndTime : CooldownEndTimes)
	{
		EndTime = -1.f;
	}
	SelectedAbility = EChaosSabotageAbility::LaserBarrier;
}

void UChaosAbilityComponent::CycleSelection()
{
	const int32 Next = (static_cast<int32>(SelectedAbility) + 1) % ChaosUtil::NumAbilities;
	SelectedAbility = static_cast<EChaosSabotageAbility>(Next);
}

bool UChaosAbilityComponent::TryActivate(EChaosSabotageAbility Ability, FString& OutFeedback)
{
	AChaosGameMode* GM = GetWorld()->GetAuthGameMode<AChaosGameMode>();
	const FString AbilityName = ChaosUtil::GetAbilityName(Ability);
	const FChaosAbilityTuning& Tuning = GetTuning(Ability);
	const float CooldownRemaining = GetCooldownRemaining(Ability);

	switch (ChaosUtil::CheckAbilityActivation(GM && GM->IsRoundActive(), Energy, Tuning.EnergyCost, CooldownRemaining))
	{
	case EChaosActivationCheck::RoundInactive:
		OutFeedback = TEXT("Sabotage is only possible while a round is active");
		return false;
	case EChaosActivationCheck::OnCooldown:
		OutFeedback = FString::Printf(TEXT("%s is on cooldown (%.1fs)"), *AbilityName, CooldownRemaining);
		return false;
	case EChaosActivationCheck::InsufficientEnergy:
		OutFeedback = FString::Printf(TEXT("Not enough energy for %s (%d / %d)"), *AbilityName, FMath::RoundToInt(Energy), FMath::RoundToInt(Tuning.EnergyCost));
		return false;
	case EChaosActivationCheck::Ok:
		break;
	}

	AChaosArenaManager* Arena = GM->GetArenaManager();
	AChaosTrapBase* Trap = Arena ? Arena->FindTargetTrap(Ability, GetOwner()->GetActorLocation()) : nullptr;
	if (!Trap || !Trap->ActivateTrap())
	{
		OutFeedback = FString::Printf(TEXT("No idle %s mechanism available"), *AbilityName);
		return false;
	}

	Energy -= Tuning.EnergyCost;
	CooldownEndTimes[static_cast<int32>(Ability)] = GetNow() + Tuning.Cooldown;
	OutFeedback = FString::Printf(TEXT("%s triggered! (-%d energy)"), *AbilityName, FMath::RoundToInt(Tuning.EnergyCost));
	return true;
}

float UChaosAbilityComponent::GetCooldownRemaining(EChaosSabotageAbility Ability) const
{
	return FMath::Max(0.f, CooldownEndTimes[static_cast<int32>(Ability)] - GetNow());
}

const FChaosAbilityTuning& UChaosAbilityComponent::GetTuning(EChaosSabotageAbility Ability) const
{
	switch (Ability)
	{
	case EChaosSabotageAbility::MovingPlatform: return PlatformTuning;
	case EChaosSabotageAbility::StunTrap:       return StunTuning;
	case EChaosSabotageAbility::LaserBarrier:
	default:                                    return LaserTuning;
	}
}

bool UChaosAbilityComponent::HasEnergyFor(EChaosSabotageAbility Ability) const
{
	return Energy + KINDA_SMALL_NUMBER >= GetTuning(Ability).EnergyCost;
}

float UChaosAbilityComponent::GetNow() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}
