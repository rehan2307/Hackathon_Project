#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ChaosVisuals.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class UStaticMesh;

UENUM()
enum class EChaosShape : uint8
{
	Cube,
	Cylinder,
	Sphere,
	Cone
};

/** Parameters of the M_ChaosSurface master material (PBR panels with seams and wear). */
struct FChaosSurfaceStyle
{
	FLinearColor Color = FLinearColor(0.05f, 0.055f, 0.065f);
	float Metallic = 0.5f;
	float Roughness = 0.45f;
	float PanelSize = 200.f;
	float SeamDarken = 0.6f;
	float Variation = 0.12f;
	float NoiseAmount = 0.f;
	float NoiseScale = 1400.f;
	FLinearColor SeamGlowColor = FLinearColor::Black;
	float SeamGlow = 0.f;
	FLinearColor Emissive = FLinearColor::Black;
	float EmissiveIntensity = 0.f;
};

/**
 * Shared look-up for the generated master materials (Content/Materials) and the engine basic
 * shapes. Static scenery uses cached shared instances; gameplay objects get their own dynamic
 * instances so their state colours can change. Falls back to the engine's BasicShapeMaterial
 * (which also has a "Color" parameter) if the generated materials are missing.
 */
UCLASS()
class CHAOSCONTROL_API UChaosVisuals : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UChaosVisuals();

	static UChaosVisuals* Get(const UObject* WorldContext);

	UStaticMesh* GetMesh(EChaosShape Shape) const;

	// ---- Shared instances for static scenery ----------------------------------------------
	UMaterialInterface* Surface(const FChaosSurfaceStyle& Style);
	UMaterialInterface* Emissive(const FLinearColor& Color, float Intensity, float PulseSpeed = 0.f, float PulseAmount = 0.f, float BandMix = 0.f);
	UMaterialInterface* Energy(const FLinearColor& Color, float Intensity, float FresnelMix = 0.5f, float LineMix = 0.35f, float PulseAmount = 0.2f);
	UMaterialInterface* Hazard(const FLinearColor& Color, float Glow = 0.4f);

	// ---- Per-component instances for objects that change state -----------------------------
	static UMaterialInstanceDynamic* ApplySurface(UPrimitiveComponent* Component, const FChaosSurfaceStyle& Style);
	static UMaterialInstanceDynamic* ApplyEmissive(UPrimitiveComponent* Component, const FLinearColor& Color, float Intensity, float PulseSpeed = 0.f, float PulseAmount = 0.f);
	static UMaterialInstanceDynamic* ApplyEnergy(UPrimitiveComponent* Component, const FLinearColor& Color, float Intensity, float FresnelMix = 0.5f, float LineMix = 0.35f, float PulseAmount = 0.2f);
	static UMaterialInstanceDynamic* ApplyHazard(UPrimitiveComponent* Component, const FLinearColor& Color, float Glow = 0.4f);

	/** Updates a parameter on a component's own dynamic instance (no-op if it has none). */
	static void SetScalar(UPrimitiveComponent* Component, FName Name, float Value);

	/** Spawns a short expanding glow (collection, activation, stun, victory feedback). */
	static void SpawnPulse(const UObject* WorldContext, const FVector& Location, const FLinearColor& Color, float Radius, float Duration = 0.6f, bool bRing = false, float LightIntensity = 0.f);

private:
	UMaterialInterface* GetShared(const FString& Key, UMaterialInterface* Parent, TFunctionRef<void(UMaterialInstanceDynamic*)> Configure);
	static UMaterialInstanceDynamic* MakeOwnInstance(UPrimitiveComponent* Component, UMaterialInterface* Parent);
	static void ConfigureSurface(UMaterialInstanceDynamic* Instance, const FChaosSurfaceStyle& Style);
	static void ConfigureEmissive(UMaterialInstanceDynamic* Instance, const FLinearColor& Color, float Intensity, float PulseSpeed, float PulseAmount, float BandMix);
	static void ConfigureEnergy(UMaterialInstanceDynamic* Instance, const FLinearColor& Color, float Intensity, float FresnelMix, float LineMix, float PulseAmount);
	static void ConfigureHazard(UMaterialInstanceDynamic* Instance, const FLinearColor& Color, float Glow);

	UPROPERTY()
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> EmissiveMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> EnergyMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> HazardMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FallbackMaterial;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> ShapeMeshes;

	UPROPERTY()
	TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> SharedInstances;
};
