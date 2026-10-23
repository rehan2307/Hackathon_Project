#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChaosControlTypes.h"
#include "ChaosVisuals.h"
#include "ChaosArenaManager.generated.h"

class ACameraActor;
class AChaosArenaGeometry;
class AChaosExit;
class AChaosObjective;
class AChaosSpinner;
class AChaosTrapBase;
class ADirectionalLight;
class AExponentialHeightFog;
class APostProcessVolume;
class ASkyLight;
class UMaterialInterface;

/** Art direction for one arena: palette, surface styles and lighting mood. */
struct FChaosArenaTheme
{
	FLinearColor Accent = FLinearColor(0.f, 0.7f, 1.f);
	FLinearColor AccentAlt = FLinearColor(0.6f, 0.8f, 1.f);
	FChaosSurfaceStyle Floor;
	FChaosSurfaceStyle Under;
	FChaosSurfaceStyle Wall;
	FChaosSurfaceStyle Trim;
	FLinearColor KeyColor = FLinearColor(0.85f, 0.9f, 1.f);
	float KeyIntensity = 3.f;
	FLinearColor RimColor = FLinearColor(0.f, 0.6f, 1.f);
	float RimIntensity = 1.f;
	FLinearColor FogColor = FLinearColor(0.01f, 0.02f, 0.035f);
	FLinearColor SkyTint = FLinearColor(0.35f, 0.45f, 0.6f);
	float SkyIntensity = 0.5f;
	float AccentLightIntensity = 40.f;
};

/**
 * Builds, resets and tears down the three arenas (PRD FR-06). Every arena shares the same
 * gameplay footprint and modular kit; layouts differ in floor gaps, walls, cores and mechanisms,
 * and each has its own theme (palette, materials, set dressing and lighting mood).
 * Building at runtime keeps every arena playable without hand-authored map assets.
 */
UCLASS(Config = Game)
class CHAOSCONTROL_API AChaosArenaManager : public AActor
{
	GENERATED_BODY()

public:
	AChaosArenaManager();

	void BuildArena(EChaosArena Arena);

	/** Restores cores, locks the exit and returns every mechanism to its inactive state. */
	void ResetArena();
	void StopAllTraps();

	EChaosArena GetCurrentArena() const { return CurrentArena; }
	bool HasBuiltArena() const { return bHasArena; }
	const TArray<TObjectPtr<AChaosObjective>>& GetObjectives() const { return Objectives; }
	const TArray<TObjectPtr<AChaosTrapBase>>& GetTraps() const { return Traps; }
	AChaosExit* GetExit() const { return Exit; }
	ACameraActor* GetArenaCamera() const { return ArenaCamera; }
	int32 GetRequiredCoreCount() const { return Objectives.Num(); }
	const FChaosArenaTheme& GetTheme() const { return Theme; }

	FTransform GetSpawnTransform(EChaosPlayerRole InRole) const;

	/** Nearest idle mechanism of the given type, or null if all of them are active. */
	AChaosTrapBase* FindTargetTrap(EChaosSabotageAbility Ability, const FVector& FromLocation) const;
	AChaosObjective* FindNearestUncollectedCore(const FVector& FromLocation, float MaxDistance) const;

	/** Exposure compensation (EV) applied by the arena post-process volume. */
	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Lighting")
	float ExposureCompensation = -1.f;

	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Lighting")
	float BloomIntensity = 0.55f;

	/** Degrees below the horizon of the shared overhead camera. */
	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Camera", meta = (ClampMin = "40", ClampMax = "85"))
	float CameraPitch = 64.f;

	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Camera", meta = (ClampMin = "40", ClampMax = "120"))
	float CameraFieldOfView = 70.f;

private:
	void ClearArena();
	void ApplyTheme(EChaosArena Arena);
	void SetupEnvironment();
	void ApplyAtmosphere();
	void PositionCamera();

	template <typename TActor>
	TActor* SpawnArenaActor(const FTransform& Transform);

	// ---- Modular kit ------------------------------------------------------------------------
	void AddShape(EChaosShape Shape, const FVector& Center, const FVector& Size, UMaterialInterface* Material, bool bCollision = false, const FRotator& Rotation = FRotator::ZeroRotator);
	void AddBox(const FVector& Min, const FVector& Max, UMaterialInterface* Material, bool bCollision = false);
	void AddFloor(float X0, float X1, float Y0, float Y1);
	void AddWall(float X0, float X1, float Y0, float Y1, float Height = 220.f, const FLinearColor* StripColor = nullptr);
	void AddChannel(float X0, float X1, float Y0, float Y1);
	void AddPerimeter();
	void AddFacilityBackdrop();
	void AddPit(float X0, float X1, float Y0, float Y1);
	void AddCore(const FVector2D& Location, float PedestalHeight = 0.f);
	void AddLaser(const FVector2D& Center, float Width, float Yaw = 0.f);
	void AddPlatform(const FVector2D& Center, const FVector2D& Size, const FVector& Axis, float Amplitude);
	void AddStunPad(const FVector2D& Center, const FVector2D& Size);
	void AddExit(const FVector2D& Center);
	void AddAccentLight(const FVector& Location, const FLinearColor& Color, float Intensity, float Radius);
	void AddScreen(const FVector& Center, float Width, float Height, const FLinearColor& Color);

	// Shared materials for the current theme.
	UMaterialInterface* FloorMaterial();
	UMaterialInterface* UnderMaterial();
	UMaterialInterface* WallMaterial();
	UMaterialInterface* TrimMaterial();
	UMaterialInterface* AccentGlow(float Intensity = 6.f);

	// ---- Arena layouts ----------------------------------------------------------------------
	void BuildNeonGrid();
	void BuildReactorRun();
	void BuildChaosCore();

	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedActors;

	UPROPERTY()
	TObjectPtr<AChaosArenaGeometry> Geometry;

	UPROPERTY()
	TArray<TObjectPtr<AChaosObjective>> Objectives;

	UPROPERTY()
	TArray<TObjectPtr<AChaosTrapBase>> Traps;

	UPROPERTY()
	TObjectPtr<AChaosExit> Exit;

	UPROPERTY()
	TObjectPtr<ACameraActor> ArenaCamera;

	UPROPERTY()
	TObjectPtr<ADirectionalLight> KeyLight;

	UPROPERTY()
	TObjectPtr<ADirectionalLight> RimLight;

	UPROPERTY()
	TObjectPtr<ASkyLight> AmbientLight;

	UPROPERTY()
	TObjectPtr<AExponentialHeightFog> HeightFog;

	UPROPERTY()
	TObjectPtr<APostProcessVolume> PostProcess;

	FChaosArenaTheme Theme;
	EChaosArena CurrentArena = EChaosArena::NeonGrid;
	FVector RunnerSpawn = FVector::ZeroVector;
	FVector SaboteurSpawn = FVector::ZeroVector;
	bool bHasArena = false;
	bool bEnvironmentReady = false;
};
