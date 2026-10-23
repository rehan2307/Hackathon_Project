#include "ChaosVisuals.h"
#include "ChaosPulseFX.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

UChaosVisuals::UChaosVisuals()
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SurfaceFinder(TEXT("/Game/Materials/M_ChaosSurface.M_ChaosSurface"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> EmissiveFinder(TEXT("/Game/Materials/M_ChaosEmissive.M_ChaosEmissive"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> EnergyFinder(TEXT("/Game/Materials/M_ChaosEnergy.M_ChaosEnergy"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HazardFinder(TEXT("/Game/Materials/M_ChaosHazard.M_ChaosHazard"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FallbackFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	SurfaceMaterial = SurfaceFinder.Object;
	EmissiveMaterial = EmissiveFinder.Object;
	EnergyMaterial = EnergyFinder.Object;
	HazardMaterial = HazardFinder.Object;
	FallbackMaterial = FallbackFinder.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	ShapeMeshes = { CubeFinder.Object, CylinderFinder.Object, SphereFinder.Object, ConeFinder.Object };
}

UChaosVisuals* UChaosVisuals::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UChaosVisuals>() : nullptr;
}

UStaticMesh* UChaosVisuals::GetMesh(EChaosShape Shape) const
{
	const int32 Index = static_cast<int32>(Shape);
	return ShapeMeshes.IsValidIndex(Index) ? ShapeMeshes[Index].Get() : nullptr;
}

// ---------------------------------------------------------------------------------------------
// Parameter setup
// ---------------------------------------------------------------------------------------------

void UChaosVisuals::ConfigureSurface(UMaterialInstanceDynamic* Instance, const FChaosSurfaceStyle& Style)
{
	Instance->SetVectorParameterValue(TEXT("Color"), Style.Color);
	Instance->SetScalarParameterValue(TEXT("Metallic"), Style.Metallic);
	Instance->SetScalarParameterValue(TEXT("Roughness"), Style.Roughness);
	Instance->SetScalarParameterValue(TEXT("PanelSize"), Style.PanelSize);
	Instance->SetScalarParameterValue(TEXT("SeamDarken"), Style.SeamDarken);
	Instance->SetScalarParameterValue(TEXT("Variation"), Style.Variation);
	Instance->SetScalarParameterValue(TEXT("NoiseAmount"), Style.NoiseAmount);
	Instance->SetScalarParameterValue(TEXT("NoiseScale"), Style.NoiseScale);
	Instance->SetVectorParameterValue(TEXT("SeamGlowColor"), Style.SeamGlowColor);
	Instance->SetScalarParameterValue(TEXT("SeamGlow"), Style.SeamGlow);
	Instance->SetVectorParameterValue(TEXT("Emissive"), Style.Emissive);
	Instance->SetScalarParameterValue(TEXT("EmissiveIntensity"), Style.EmissiveIntensity);
}

void UChaosVisuals::ConfigureEmissive(UMaterialInstanceDynamic* Instance, const FLinearColor& Color, float Intensity, float PulseSpeed, float PulseAmount, float BandMix)
{
	Instance->SetVectorParameterValue(TEXT("Color"), Color);
	Instance->SetScalarParameterValue(TEXT("Intensity"), Intensity);
	Instance->SetScalarParameterValue(TEXT("PulseSpeed"), PulseSpeed);
	Instance->SetScalarParameterValue(TEXT("PulseAmount"), PulseAmount);
	Instance->SetScalarParameterValue(TEXT("BandMix"), BandMix);
}

void UChaosVisuals::ConfigureEnergy(UMaterialInstanceDynamic* Instance, const FLinearColor& Color, float Intensity, float FresnelMix, float LineMix, float PulseAmount)
{
	Instance->SetVectorParameterValue(TEXT("Color"), Color);
	Instance->SetScalarParameterValue(TEXT("Intensity"), Intensity);
	Instance->SetScalarParameterValue(TEXT("FresnelMix"), FresnelMix);
	Instance->SetScalarParameterValue(TEXT("LineMix"), LineMix);
	Instance->SetScalarParameterValue(TEXT("PulseAmount"), PulseAmount);
}

void UChaosVisuals::ConfigureHazard(UMaterialInstanceDynamic* Instance, const FLinearColor& Color, float Glow)
{
	Instance->SetVectorParameterValue(TEXT("Color"), Color);
	Instance->SetScalarParameterValue(TEXT("Glow"), Glow);
}

// ---------------------------------------------------------------------------------------------
// Shared instances
// ---------------------------------------------------------------------------------------------

UMaterialInterface* UChaosVisuals::GetShared(const FString& Key, UMaterialInterface* Parent, TFunctionRef<void(UMaterialInstanceDynamic*)> Configure)
{
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = SharedInstances.Find(Key))
	{
		return *Found;
	}

	UMaterialInterface* Base = Parent ? Parent : FallbackMaterial.Get();
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, this);
	if (!Instance)
	{
		return Base;
	}
	Configure(Instance);
	SharedInstances.Add(Key, Instance);
	return Instance;
}

UMaterialInterface* UChaosVisuals::Surface(const FChaosSurfaceStyle& Style)
{
	const FString Key = FString::Printf(TEXT("S|%s|%.2f|%.2f|%.0f|%.2f|%.2f|%.2f|%.0f|%s|%.2f|%s|%.2f"),
		*Style.Color.ToString(), Style.Metallic, Style.Roughness, Style.PanelSize, Style.SeamDarken, Style.Variation, Style.NoiseAmount, Style.NoiseScale,
		*Style.SeamGlowColor.ToString(), Style.SeamGlow, *Style.Emissive.ToString(), Style.EmissiveIntensity);
	return GetShared(Key, SurfaceMaterial, [&Style](UMaterialInstanceDynamic* Instance) { ConfigureSurface(Instance, Style); });
}

UMaterialInterface* UChaosVisuals::Emissive(const FLinearColor& Color, float Intensity, float PulseSpeed, float PulseAmount, float BandMix)
{
	const FString Key = FString::Printf(TEXT("E|%s|%.2f|%.2f|%.2f|%.2f"), *Color.ToString(), Intensity, PulseSpeed, PulseAmount, BandMix);
	return GetShared(Key, EmissiveMaterial, [&](UMaterialInstanceDynamic* Instance) { ConfigureEmissive(Instance, Color, Intensity, PulseSpeed, PulseAmount, BandMix); });
}

UMaterialInterface* UChaosVisuals::Energy(const FLinearColor& Color, float Intensity, float FresnelMix, float LineMix, float PulseAmount)
{
	const FString Key = FString::Printf(TEXT("N|%s|%.2f|%.2f|%.2f|%.2f"), *Color.ToString(), Intensity, FresnelMix, LineMix, PulseAmount);
	return GetShared(Key, EnergyMaterial, [&](UMaterialInstanceDynamic* Instance) { ConfigureEnergy(Instance, Color, Intensity, FresnelMix, LineMix, PulseAmount); });
}

UMaterialInterface* UChaosVisuals::Hazard(const FLinearColor& Color, float Glow)
{
	const FString Key = FString::Printf(TEXT("H|%s|%.2f"), *Color.ToString(), Glow);
	return GetShared(Key, HazardMaterial, [&](UMaterialInstanceDynamic* Instance) { ConfigureHazard(Instance, Color, Glow); });
}

// ---------------------------------------------------------------------------------------------
// Per-component instances
// ---------------------------------------------------------------------------------------------

UMaterialInstanceDynamic* UChaosVisuals::MakeOwnInstance(UPrimitiveComponent* Component, UMaterialInterface* Parent)
{
	if (!Component)
	{
		return nullptr;
	}
	UChaosVisuals* Visuals = Get(Component);
	UMaterialInterface* Base = Parent ? Parent : (Visuals ? Visuals->FallbackMaterial.Get() : nullptr);
	if (!Base)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, Component);
	Component->SetMaterial(0, Instance);
	return Instance;
}

UMaterialInstanceDynamic* UChaosVisuals::ApplySurface(UPrimitiveComponent* Component, const FChaosSurfaceStyle& Style)
{
	UChaosVisuals* Visuals = Get(Component);
	UMaterialInstanceDynamic* Instance = MakeOwnInstance(Component, Visuals ? Visuals->SurfaceMaterial.Get() : nullptr);
	if (Instance)
	{
		ConfigureSurface(Instance, Style);
	}
	return Instance;
}

UMaterialInstanceDynamic* UChaosVisuals::ApplyEmissive(UPrimitiveComponent* Component, const FLinearColor& Color, float Intensity, float PulseSpeed, float PulseAmount)
{
	UChaosVisuals* Visuals = Get(Component);
	UMaterialInstanceDynamic* Instance = MakeOwnInstance(Component, Visuals ? Visuals->EmissiveMaterial.Get() : nullptr);
	if (Instance)
	{
		ConfigureEmissive(Instance, Color, Intensity, PulseSpeed, PulseAmount, 0.f);
	}
	if (Component)
	{
		Component->SetCastShadow(false);
	}
	return Instance;
}

UMaterialInstanceDynamic* UChaosVisuals::ApplyEnergy(UPrimitiveComponent* Component, const FLinearColor& Color, float Intensity, float FresnelMix, float LineMix, float PulseAmount)
{
	UChaosVisuals* Visuals = Get(Component);
	UMaterialInstanceDynamic* Instance = MakeOwnInstance(Component, Visuals ? Visuals->EnergyMaterial.Get() : nullptr);
	if (Instance)
	{
		ConfigureEnergy(Instance, Color, Intensity, FresnelMix, LineMix, PulseAmount);
	}
	if (Component)
	{
		Component->SetCastShadow(false);
	}
	return Instance;
}

UMaterialInstanceDynamic* UChaosVisuals::ApplyHazard(UPrimitiveComponent* Component, const FLinearColor& Color, float Glow)
{
	UChaosVisuals* Visuals = Get(Component);
	UMaterialInstanceDynamic* Instance = MakeOwnInstance(Component, Visuals ? Visuals->HazardMaterial.Get() : nullptr);
	if (Instance)
	{
		ConfigureHazard(Instance, Color, Glow);
	}
	return Instance;
}

void UChaosVisuals::SetScalar(UPrimitiveComponent* Component, FName Name, float Value)
{
	if (!Component)
	{
		return;
	}
	UMaterialInstanceDynamic* Instance = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0));
	if (Instance && Instance->GetOuter() == Component)
	{
		Instance->SetScalarParameterValue(Name, Value);
	}
}

void UChaosVisuals::SpawnPulse(const UObject* WorldContext, const FVector& Location, const FLinearColor& Color, float Radius, float Duration, bool bRing, float LightIntensity)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AChaosPulseFX* Pulse = World->SpawnActor<AChaosPulseFX>(AChaosPulseFX::StaticClass(), FTransform(Location), Params))
	{
		Pulse->Start(Color, Radius, Duration, bRing, LightIntensity);
	}
}
