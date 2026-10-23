#include "ChaosArenaManager.h"
#include "ChaosControl.h"
#include "ChaosArenaGeometry.h"
#include "ChaosExit.h"
#include "ChaosLaserBarrier.h"
#include "ChaosMovingPlatform.h"
#include "ChaosObjective.h"
#include "ChaosStunTrap.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PointLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// Shared arena footprint (cm). Screen-up is +X, screen-right is +Y.
namespace ChaosArenaLayout
{
	constexpr float MinX = -1100.f;
	constexpr float MaxX = 1100.f;
	constexpr float MinY = -1600.f;
	constexpr float MaxY = 1600.f;
	constexpr float FloorThickness = 40.f;
	constexpr float UnderDepth = 320.f;
	constexpr float WallHeight = 220.f;
	constexpr float PerimeterThickness = 60.f;
	constexpr float CatwalkWidth = 260.f;
	constexpr float LowerDeckZ = -760.f;
	constexpr float SpawnHeight = 95.f;

	// Pits are 550 wide: wider than a running jump (~450), so only the platform crosses them.
	constexpr float PitHalfWidth = 275.f;
	const FVector2D PlatformSize(546.f, 350.f);
	const FVector AlongY(0.f, 1.f, 0.f);
}

// Only this file uses the layout constants; no other translation unit declares these names.
using namespace ChaosArenaLayout;

AChaosArenaManager::AChaosArenaManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------

void AChaosArenaManager::BuildArena(EChaosArena Arena)
{
	ClearArena();
	SetupEnvironment();

	CurrentArena = Arena;
	ApplyTheme(Arena);

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Geometry = GetWorld()->SpawnActor<AChaosArenaGeometry>(AChaosArenaGeometry::StaticClass(), FTransform::Identity, Params);
	SpawnedActors.Add(Geometry);

	switch (Arena)
	{
	case EChaosArena::NeonGrid:   BuildNeonGrid();   break;
	case EChaosArena::ReactorRun: BuildReactorRun(); break;
	case EChaosArena::ChaosCore:  BuildChaosCore();  break;
	}

	bHasArena = true;
	ApplyAtmosphere();
	PositionCamera();
	ResetArena();

	UE_LOG(LogChaosControl, Log, TEXT("Built arena %s: %d cores, %d mechanisms, %d instanced pieces, %d actors"),
		*ChaosUtil::GetArenaName(Arena), Objectives.Num(), Traps.Num(), Geometry ? Geometry->GetPieceCount() : 0, SpawnedActors.Num());
}

void AChaosArenaManager::ResetArena()
{
	for (AChaosObjective* Core : Objectives)
	{
		if (IsValid(Core))
		{
			Core->ResetObjective();
		}
	}
	for (AChaosTrapBase* Trap : Traps)
	{
		if (IsValid(Trap))
		{
			Trap->ResetTrap();
		}
	}
	if (IsValid(Exit))
	{
		Exit->SetUnlocked(false);
	}
}

void AChaosArenaManager::StopAllTraps()
{
	for (AChaosTrapBase* Trap : Traps)
	{
		if (IsValid(Trap))
		{
			Trap->DeactivateTrap();
		}
	}
}

void AChaosArenaManager::ClearArena()
{
	for (AActor* Actor : SpawnedActors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	SpawnedActors.Reset();
	Objectives.Reset();
	Traps.Reset();
	Exit = nullptr;
	Geometry = nullptr;
	bHasArena = false;
}

// ---------------------------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------------------------

FTransform AChaosArenaManager::GetSpawnTransform(EChaosPlayerRole InRole) const
{
	const FVector Base = InRole == EChaosPlayerRole::Runner ? RunnerSpawn : SaboteurSpawn;
	return FTransform(FRotator::ZeroRotator, Base + FVector(0.f, 0.f, SpawnHeight));
}

AChaosTrapBase* AChaosArenaManager::FindTargetTrap(EChaosSabotageAbility Ability, const FVector& FromLocation) const
{
	AChaosTrapBase* Best = nullptr;
	double BestDistanceSq = TNumericLimits<double>::Max();
	for (AChaosTrapBase* Trap : Traps)
	{
		if (!IsValid(Trap) || Trap->GetAbilityType() != Ability || Trap->IsTrapActive())
		{
			continue;
		}
		const double DistanceSq = FVector::DistSquared2D(Trap->GetActorLocation(), FromLocation);
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Trap;
		}
	}
	return Best;
}

AChaosObjective* AChaosArenaManager::FindNearestUncollectedCore(const FVector& FromLocation, float MaxDistance) const
{
	AChaosObjective* Best = nullptr;
	double BestDistanceSq = FMath::Square(static_cast<double>(MaxDistance));
	for (AChaosObjective* Core : Objectives)
	{
		if (!IsValid(Core) || Core->IsCollected())
		{
			continue;
		}
		const double DistanceSq = FVector::DistSquared(Core->GetActorLocation(), FromLocation);
		if (DistanceSq <= BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Core;
		}
	}
	return Best;
}

// ---------------------------------------------------------------------------------------------
// Themes
// ---------------------------------------------------------------------------------------------

void AChaosArenaManager::ApplyTheme(EChaosArena Arena)
{
	Theme = FChaosArenaTheme();

	// Shared kit defaults: dark graphite composite, painted-metal walls, brushed-metal trims.
	Theme.Under.Color = FLinearColor(0.018f, 0.019f, 0.023f);
	Theme.Under.Metallic = 0.6f;
	Theme.Under.Roughness = 0.55f;
	Theme.Under.PanelSize = 100.f;
	Theme.Under.SeamDarken = 0.7f;

	Theme.Trim.Metallic = 0.9f;
	Theme.Trim.Roughness = 0.3f;
	Theme.Trim.PanelSize = 400.f;
	Theme.Trim.SeamDarken = 0.3f;
	Theme.Trim.Variation = 0.05f;
	Theme.Trim.NoiseAmount = 0.f;

	switch (Arena)
	{
	case EChaosArena::NeonGrid:
		// High-tech training facility: light composite floor with a faint cyan grid, clean walls.
		Theme.Accent = FLinearColor(0.f, 0.72f, 1.f);
		Theme.AccentAlt = FLinearColor(0.55f, 0.85f, 1.f);
		Theme.Floor.Color = FLinearColor(0.055f, 0.06f, 0.07f);
		Theme.Floor.Metallic = 0.25f;
		Theme.Floor.Roughness = 0.32f;
		Theme.Floor.PanelSize = 200.f;
		Theme.Floor.SeamDarken = 0.45f;
		Theme.Floor.Variation = 0.08f;
		Theme.Floor.SeamGlowColor = Theme.Accent;
		Theme.Floor.SeamGlow = 0.9f;
		Theme.Wall.Color = FLinearColor(0.1f, 0.105f, 0.115f);
		Theme.Wall.Metallic = 0.2f;
		Theme.Wall.Roughness = 0.5f;
		Theme.Wall.PanelSize = 110.f;
		Theme.Trim.Color = FLinearColor(0.26f, 0.27f, 0.3f);
		Theme.KeyColor = FLinearColor(0.85f, 0.92f, 1.f);
		Theme.KeyIntensity = 3.2f;
		Theme.RimColor = FLinearColor(0.2f, 0.7f, 1.f);
		Theme.RimIntensity = 1.2f;
		Theme.FogColor = FLinearColor(0.008f, 0.02f, 0.035f);
		Theme.SkyTint = FLinearColor(0.4f, 0.55f, 0.75f);
		Theme.SkyIntensity = 0.25f;
		Theme.AccentLightIntensity = 35.f;
		break;

	case EChaosArena::ReactorRun:
		// Industrial power facility: heavy brushed steel, amber hazard language, warm light.
		Theme.Accent = FLinearColor(1.f, 0.42f, 0.03f);
		Theme.AccentAlt = FLinearColor(1.f, 0.15f, 0.04f);
		Theme.Floor.Color = FLinearColor(0.045f, 0.042f, 0.04f);
		Theme.Floor.Metallic = 0.45f;
		Theme.Floor.Roughness = 0.42f;
		Theme.Floor.PanelSize = 150.f;
		Theme.Floor.SeamDarken = 0.65f;
		Theme.Floor.Variation = 0.14f;
		Theme.Floor.NoiseAmount = 0.f;
		Theme.Floor.SeamGlowColor = Theme.Accent;
		Theme.Floor.SeamGlow = 0.15f;
		Theme.Wall.Color = FLinearColor(0.085f, 0.075f, 0.065f);
		Theme.Wall.Metallic = 0.55f;
		Theme.Wall.Roughness = 0.55f;
		Theme.Wall.PanelSize = 120.f;
		Theme.Wall.NoiseAmount = 0.f;
		Theme.Trim.Color = FLinearColor(0.2f, 0.18f, 0.16f);
		Theme.KeyColor = FLinearColor(1.f, 0.88f, 0.72f);
		Theme.KeyIntensity = 2.8f;
		Theme.RimColor = FLinearColor(1.f, 0.45f, 0.1f);
		Theme.RimIntensity = 1.4f;
		Theme.FogColor = FLinearColor(0.03f, 0.016f, 0.008f);
		Theme.SkyTint = FLinearColor(0.7f, 0.5f, 0.35f);
		Theme.SkyIntensity = 0.2f;
		Theme.AccentLightIntensity = 45.f;
		break;

	case EChaosArena::ChaosCore:
		// Experimental facility: glossy graphite, magenta/violet energy, dramatic contrast.
		Theme.Accent = FLinearColor(1.f, 0.08f, 0.55f);
		Theme.AccentAlt = FLinearColor(0.45f, 0.12f, 1.f);
		Theme.Floor.Color = FLinearColor(0.03f, 0.028f, 0.038f);
		Theme.Floor.Metallic = 0.4f;
		Theme.Floor.Roughness = 0.24f;
		Theme.Floor.PanelSize = 250.f;
		Theme.Floor.SeamDarken = 0.55f;
		Theme.Floor.SeamGlowColor = Theme.AccentAlt;
		Theme.Floor.SeamGlow = 0.6f;
		Theme.Wall.Color = FLinearColor(0.07f, 0.06f, 0.085f);
		Theme.Wall.Metallic = 0.5f;
		Theme.Wall.Roughness = 0.35f;
		Theme.Wall.PanelSize = 125.f;
		Theme.Trim.Color = FLinearColor(0.16f, 0.14f, 0.19f);
		Theme.KeyColor = FLinearColor(0.8f, 0.78f, 1.f);
		Theme.KeyIntensity = 2.2f;
		Theme.RimColor = FLinearColor(1.f, 0.1f, 0.6f);
		Theme.RimIntensity = 1.6f;
		Theme.FogColor = FLinearColor(0.025f, 0.008f, 0.04f);
		Theme.SkyTint = FLinearColor(0.55f, 0.4f, 0.8f);
		Theme.SkyIntensity = 0.2f;
		Theme.AccentLightIntensity = 50.f;
		break;
	}
}

UMaterialInterface* AChaosArenaManager::FloorMaterial()
{
	return UChaosVisuals::Get(this)->Surface(Theme.Floor);
}

UMaterialInterface* AChaosArenaManager::UnderMaterial()
{
	return UChaosVisuals::Get(this)->Surface(Theme.Under);
}

UMaterialInterface* AChaosArenaManager::WallMaterial()
{
	return UChaosVisuals::Get(this)->Surface(Theme.Wall);
}

UMaterialInterface* AChaosArenaManager::TrimMaterial()
{
	return UChaosVisuals::Get(this)->Surface(Theme.Trim);
}

UMaterialInterface* AChaosArenaManager::AccentGlow(float Intensity)
{
	return UChaosVisuals::Get(this)->Emissive(Theme.Accent, Intensity);
}

// ---------------------------------------------------------------------------------------------
// Environment: lights, sky, fog, post-process, camera
// ---------------------------------------------------------------------------------------------

void AChaosArenaManager::SetupEnvironment()
{
	if (bEnvironmentReady)
	{
		return;
	}
	bEnvironmentReady = true;

	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	auto SpawnDirectional = [&](const FRotator& Rotation, bool bShadows, int32 Priority) -> ADirectionalLight*
	{
		FActorSpawnParameters Deferred = Params;
		Deferred.bDeferConstruction = true;
		const FTransform LightTransform(Rotation);
		ADirectionalLight* Light = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), LightTransform, Deferred);
		if (Light)
		{
			// Runtime-spawned lights must be movable: there is no baked lighting.
			Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
			Light->GetLightComponent()->CastShadows = bShadows;
			if (UDirectionalLightComponent* Directional = Cast<UDirectionalLightComponent>(Light->GetLightComponent()))
			{
				// Explicit priority: the key light drives forward-shaded translucency.
				Directional->ForwardShadingPriority = Priority;
			}
			Light->FinishSpawning(LightTransform);
		}
		return Light;
	};

	for (TActorIterator<ALight> It(World); It; ++It)
	{
		if (It->GetOwner() != this && It->GetLightComponent())
		{
			It->GetLightComponent()->SetVisibility(false);
		}
	}
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		if (It->GetOwner() != this && It->GetLightComponent())
		{
			It->GetLightComponent()->SetVisibility(false);
		}
	}

	KeyLight = SpawnDirectional(FRotator(-58.f, 30.f, 0.f), true, 1);

	FActorSpawnParameters DeferredSky = Params;
	DeferredSky.bDeferConstruction = true;
	AmbientLight = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FTransform::Identity, DeferredSky);
	if (AmbientLight)
	{
		USkyLightComponent* Sky = AmbientLight->GetLightComponent();
		Sky->SetMobility(EComponentMobility::Movable);
		Sky->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
		Sky->Cubemap = LoadObject<UTextureCube>(nullptr, TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"));
		Sky->bLowerHemisphereIsBlack = false;
		AmbientLight->FinishSpawning(FTransform::Identity);
	}

	HeightFog = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FTransform(FVector(0.f, 0.f, -400.f)), Params);

	PostProcess = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, Params);
	if (PostProcess)
	{
		PostProcess->bUnbound = true;
		FPostProcessSettings& Settings = PostProcess->Settings;
		Settings.bOverride_AutoExposureMethod = true;
		Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		Settings.AutoExposureApplyPhysicalCameraExposure = false;
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = ExposureCompensation;
		Settings.bOverride_BloomThreshold = true;
		Settings.BloomThreshold = 1.f;
		Settings.bOverride_BloomIntensity = true;
		Settings.BloomIntensity = BloomIntensity;
		Settings.bOverride_VignetteIntensity = true;
		Settings.VignetteIntensity = 0.45f;
		Settings.bOverride_AmbientOcclusionIntensity = true;
		Settings.AmbientOcclusionIntensity = 0.65f;
		Settings.bOverride_FilmGrainIntensity = true;
		Settings.FilmGrainIntensity = 0.f;
		Settings.bOverride_ColorContrast = true;
		Settings.ColorContrast = FVector4(1.08f, 1.08f, 1.08f, 1.f);
		Settings.bOverride_ColorSaturation = true;
		Settings.ColorSaturation = FVector4(1.06f, 1.06f, 1.06f, 1.f);
	}
}

void AChaosArenaManager::ApplyAtmosphere()
{
	if (KeyLight)
	{
		KeyLight->GetLightComponent()->SetIntensity(Theme.KeyIntensity);
		KeyLight->GetLightComponent()->SetLightColor(Theme.KeyColor);
	}
	if (RimLight)
	{
		RimLight->GetLightComponent()->SetIntensity(Theme.RimIntensity);
		RimLight->GetLightComponent()->SetLightColor(Theme.RimColor);
	}
	if (AmbientLight)
	{
		USkyLightComponent* Sky = AmbientLight->GetLightComponent();
		Sky->SetIntensity(Theme.SkyIntensity);
		Sky->SetLightColor(Theme.SkyTint);
		Sky->SetLowerHemisphereColor(Theme.FogColor);
	}
	if (HeightFog)
	{
		UExponentialHeightFogComponent* Fog = HeightFog->GetComponent();
		Fog->SetFogDensity(0.035f);
		Fog->SetFogHeightFalloff(0.12f);
		Fog->SetFogInscatteringColor(Theme.FogColor);
		Fog->SetStartDistance(2600.f);
		Fog->SetFogMaxOpacity(0.9f);
	}
}

void AChaosArenaManager::PositionCamera()
{
	if (!ArenaCamera)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ArenaCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
		if (!ArenaCamera)
		{
			return;
		}
		ArenaCamera->GetCameraComponent()->SetConstraintAspectRatio(false);
	}

	// Fit the whole arena into a fixed 16:9 overhead view. The far (top-of-screen) margin is
	// larger so the HUD cards never cover the exit or the far wall.
	const float FarMargin = 520.f;
	const float NearMargin = 40.f;
	const float SideMargin = 40.f;
	const float TanHalfH = FMath::Tan(FMath::DegreesToRadians(CameraFieldOfView * 0.5f));
	const float HalfV = FMath::Atan(TanHalfH / (16.f / 9.f));
	const float Pitch = FMath::DegreesToRadians(CameraPitch);
	const float FarAngle = Pitch - HalfV;  // ray through the top of the screen
	const float NearAngle = Pitch + HalfV; // ray through the bottom of the screen
	const float InvTanFar = FMath::Cos(FarAngle) / FMath::Sin(FarAngle);
	const float InvTanNear = FMath::Cos(NearAngle) / FMath::Sin(NearAngle);

	const float NearX = MinX - NearMargin;
	const float FarX = MaxX + FarMargin;
	const float DepthSpan = FarX - NearX;
	const float HalfWidth = (MaxY - MinY) * 0.5f + SideMargin;
	const float HeightForDepth = DepthSpan / (InvTanFar - InvTanNear);
	const float HeightForWidth = HalfWidth * FMath::Sin(NearAngle) / (FMath::Cos(HalfV) * TanHalfH);
	const float Height = FMath::Max(HeightForDepth, HeightForWidth);

	const float CenterX = (NearX + FarX) * 0.5f;
	const float CenterY = (MinY + MaxY) * 0.5f;
	const float CameraX = CenterX - 0.5f * Height * (InvTanFar + InvTanNear);

	ArenaCamera->SetActorLocationAndRotation(FVector(CameraX, CenterY, Height), FRotator(-CameraPitch, 0.f, 0.f));
	ArenaCamera->GetCameraComponent()->SetFieldOfView(CameraFieldOfView);
}

// ---------------------------------------------------------------------------------------------
// Modular kit
// ---------------------------------------------------------------------------------------------

template <typename TActor>
TActor* AChaosArenaManager::SpawnArenaActor(const FTransform& Transform)
{
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	TActor* Actor = GetWorld()->SpawnActor<TActor>(TActor::StaticClass(), Transform, Params);
	if (Actor)
	{
		SpawnedActors.Add(Actor);
	}
	return Actor;
}

void AChaosArenaManager::AddShape(EChaosShape Shape, const FVector& Center, const FVector& Size, UMaterialInterface* Material, bool bCollision, const FRotator& Rotation)
{
	if (Geometry)
	{
		Geometry->AddPiece(Shape, FTransform(Rotation, Center, Size / 100.0), Material, bCollision);
	}
}

void AChaosArenaManager::AddBox(const FVector& Min, const FVector& Max, UMaterialInterface* Material, bool bCollision)
{
	AddShape(EChaosShape::Cube, (Min + Max) * 0.5, Max - Min, Material, bCollision);
}

void AChaosArenaManager::AddFloor(float X0, float X1, float Y0, float Y1)
{
	// Walkable deck plus a deep substructure, so pits read as real drops with panelled walls.
	AddBox(FVector(X0, Y0, -FloorThickness), FVector(X1, Y1, 0.f), FloorMaterial(), true);
	AddBox(FVector(X0, Y0, -UnderDepth), FVector(X1, Y1, -FloorThickness), UnderMaterial());
}

void AChaosArenaManager::AddWall(float X0, float X1, float Y0, float Y1, float Height, const FLinearColor* StripColor)
{
	UChaosVisuals* Visuals = UChaosVisuals::Get(this);
	const FLinearColor Strip = StripColor ? *StripColor : Theme.Accent;

	// Structural body (gameplay collision), dark plinth, brushed cap and a glowing top inlay.
	AddBox(FVector(X0, Y0, 0.f), FVector(X1, Y1, Height), WallMaterial(), true);
	AddBox(FVector(X0 - 4.f, Y0 - 4.f, 0.f), FVector(X1 + 4.f, Y1 + 4.f, 20.f), UnderMaterial());
	AddBox(FVector(X0 - 5.f, Y0 - 5.f, Height), FVector(X1 + 5.f, Y1 + 5.f, Height + 10.f), TrimMaterial());

	const bool bAlongX = (X1 - X0) >= (Y1 - Y0);
	const float Thickness = bAlongX ? (Y1 - Y0) : (X1 - X0);
	const float Inset = FMath::Max(2.f, (Thickness - 10.f) * 0.5f);
	if (bAlongX)
	{
		AddBox(FVector(X0 + 6.f, Y0 + Inset, Height + 10.f), FVector(X1 - 6.f, Y1 - Inset, Height + 13.f), Visuals->Emissive(Strip, 3.5f));
	}
	else
	{
		AddBox(FVector(X0 + Inset, Y0 + 6.f, Height + 10.f), FVector(X1 - Inset, Y1 - 6.f, Height + 13.f), Visuals->Emissive(Strip, 3.5f));
	}

	// Vertical structural ribs on long walls.
	const float Length = bAlongX ? (X1 - X0) : (Y1 - Y0);
	if (Length < 400.f || Height < 120.f)
	{
		return;
	}
	const int32 RibCount = FMath::FloorToInt(Length / 260.f);
	for (int32 Rib = 1; Rib < RibCount; ++Rib)
	{
		const float T = bAlongX ? (X0 + Length * Rib / RibCount) : (Y0 + Length * Rib / RibCount);
		if (bAlongX)
		{
			AddBox(FVector(T - 9.f, Y0 - 5.f, 20.f), FVector(T + 9.f, Y1 + 5.f, Height), TrimMaterial());
		}
		else
		{
			AddBox(FVector(X0 - 5.f, T - 9.f, 20.f), FVector(X1 + 5.f, T + 9.f, Height), TrimMaterial());
		}
	}
}

void AChaosArenaManager::AddChannel(float X0, float X1, float Y0, float Y1)
{
	// Recessed route channel: dark groove with a travelling light band.
	UChaosVisuals* Visuals = UChaosVisuals::Get(this);
	AddBox(FVector(X0 - 6.f, Y0 - 6.f, 0.f), FVector(X1 + 6.f, Y1 + 6.f, 0.6f), UnderMaterial());
	AddBox(FVector(X0, Y0, 0.f), FVector(X1, Y1, 1.2f), Visuals->Emissive(Theme.Accent, 3.f, 0.f, 0.f, 0.75f));
}

void AChaosArenaManager::AddAccentLight(const FVector& Location, const FLinearColor& Color, float Intensity, float Radius)
{
	if (APointLight* Light = SpawnArenaActor<APointLight>(FTransform(Location)))
	{
		UPointLightComponent* Point = Cast<UPointLightComponent>(Light->GetLightComponent());
		Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		if (Point)
		{
			Point->SetIntensityUnits(ELightUnits::Candelas);
			Point->SetAttenuationRadius(Radius);
			Point->SetSourceRadius(40.f);
		}
		Light->GetLightComponent()->SetIntensity(Intensity);
		Light->GetLightComponent()->SetLightColor(Color);
		Light->GetLightComponent()->SetCastShadows(false);
	}
}

void AChaosArenaManager::AddScreen(const FVector& Center, float Width, float Height, const FLinearColor& Color)
{
	// Wall-mounted display on the north wall's inner face (which faces the camera).
	UChaosVisuals* Visuals = UChaosVisuals::Get(this);
	AddShape(EChaosShape::Cube, Center + FVector(2.f, 0.f, 0.f), FVector(6.f, Width + 16.f, Height + 16.f), TrimMaterial());
	AddShape(EChaosShape::Cube, Center + FVector(-2.f, 0.f, 0.f), FVector(4.f, Width, Height), Visuals->Emissive(Color, 3.5f, 0.f, 0.f, 0.5f));
}

void AChaosArenaManager::AddPerimeter()
{
	UChaosVisuals* Visuals = UChaosVisuals::Get(this);
	const float T = PerimeterThickness;

	AddWall(MaxX, MaxX + T, MinY - T, MaxY + T, WallHeight);      // north (faces the camera)
	AddWall(MinX - T, MinX, MinY - T, MaxY + T, 70.f);            // south (low, nearest the camera)
	AddWall(MinX, MaxX, MinY - T, MinY, WallHeight);              // west
	AddWall(MinX, MaxX, MaxY, MaxY + T, WallHeight);              // east

	// Outer catwalk ring with railings: the arena sits inside a larger facility.
	const float OX0 = MinX - T - CatwalkWidth;
	const float OX1 = MaxX + T + CatwalkWidth;
	const float OY0 = MinY - T - CatwalkWidth;
	const float OY1 = MaxY + T + CatwalkWidth;
	AddBox(FVector(OX0, OY0, -60.f), FVector(OX1, MinY - T, -20.f), UnderMaterial());
	AddBox(FVector(OX0, MaxY + T, -60.f), FVector(OX1, OY1, -20.f), UnderMaterial());
	AddBox(FVector(MaxX + T, MinY - T, -60.f), FVector(OX1, MaxY + T, -20.f), UnderMaterial());
	AddBox(FVector(OX0, MinY - T, -60.f), FVector(MinX - T, MaxY + T, -20.f), UnderMaterial());

	UMaterialInterface* EdgeGlow = Visuals->Emissive(Theme.Accent, 4.f);
	AddBox(FVector(OX0, OY0, -20.f), FVector(OX1, OY0 + 6.f, 70.f), TrimMaterial());
	AddBox(FVector(OX0, OY1 - 6.f, -20.f), FVector(OX1, OY1, 70.f), TrimMaterial());
	AddBox(FVector(OX1 - 6.f, OY0, -20.f), FVector(OX1, OY1, 70.f), TrimMaterial());
	AddBox(FVector(OX0, OY0, -26.f), FVector(OX1, OY0 + 8.f, -20.f), EdgeGlow);
	AddBox(FVector(OX0, OY1 - 8.f, -26.f), FVector(OX1, OY1, -20.f), EdgeGlow);
	AddBox(FVector(OX1 - 8.f, OY0, -26.f), FVector(OX1, OY1, -20.f), EdgeGlow);

	// Structural pylons along the outer edge, each with a vertical light strip.
	for (float X = OX0 + 200.f; X < OX1; X += 560.f)
	{
		for (const float Y : { OY0 + 50.f, OY1 - 50.f })
		{
			AddBox(FVector(X - 45.f, Y - 45.f, -60.f), FVector(X + 45.f, Y + 45.f, 380.f), WallMaterial());
			AddBox(FVector(X - 50.f, Y - 50.f, 380.f), FVector(X + 50.f, Y + 50.f, 400.f), TrimMaterial());
			const float Face = Y < 0.f ? Y + 46.f : Y - 49.f;
			AddBox(FVector(X - 8.f, Face, 40.f), FVector(X + 8.f, Face + 3.f, 360.f), Visuals->Emissive(Theme.Accent, 6.f));
		}
	}

	// Overhead gantry along the far (north) side frames the composition.
	AddBox(FVector(OX1 - 70.f, OY0, 380.f), FVector(OX1 - 10.f, OY1, 430.f), TrimMaterial());
	AddBox(FVector(OX1 - 74.f, OY0, 396.f), FVector(OX1 - 70.f, OY1, 404.f), EdgeGlow);
}

void AChaosArenaManager::AddFacilityBackdrop()
{
	// A lower facility level far below the arena: visible through the pits and past the edges,
	// it gives the scene depth instead of an empty black void.
	UChaosVisuals* Visuals = UChaosVisuals::Get(this);

	FChaosSurfaceStyle Deck = Theme.Under;
	Deck.PanelSize = 300.f;
	Deck.SeamGlowColor = Theme.Accent;
	Deck.SeamGlow = 0.7f;
	AddBox(FVector(-5000.f, -6000.f, LowerDeckZ - 40.f), FVector(5000.f, 6000.f, LowerDeckZ), Visuals->Surface(Deck));

	// Support columns rising from the lower deck to the catwalk ring.
	for (float X = MinX - 200.f; X <= MaxX + 200.f; X += 800.f)
	{
		for (const float Y : { MinY - 230.f, MaxY + 230.f })
		{
			AddBox(FVector(X - 60.f, Y - 60.f, LowerDeckZ), FVector(X + 60.f, Y + 60.f, -60.f), WallMaterial());
			AddBox(FVector(X - 64.f, Y - 64.f, -340.f), FVector(X + 64.f, Y + 64.f, -330.f), Visuals->Emissive(Theme.Accent, 5.f));
		}
	}

	// Distant machinery blocks on the lower deck, softened by fog.
	const FVector2D Machines[] = {
		FVector2D(-2200.f, -2600.f), FVector2D(-600.f, -2900.f), FVector2D(1200.f, -2600.f), FVector2D(2600.f, -2200.f),
		FVector2D(-2200.f, 2600.f), FVector2D(-600.f, 2900.f), FVector2D(1200.f, 2600.f), FVector2D(2600.f, 2200.f),
		FVector2D(2600.f, -800.f), FVector2D(2600.f, 800.f),
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Machines); ++Index)
	{
		const FVector2D& P = Machines[Index];
		const float Size = 300.f + 90.f * (Index % 3);
		const float Height = 500.f + 180.f * (Index % 4);
		AddBox(FVector(P.X - Size * 0.5f, P.Y - Size * 0.5f, LowerDeckZ), FVector(P.X + Size * 0.5f, P.Y + Size * 0.5f, LowerDeckZ + Height), WallMaterial());
		AddBox(FVector(P.X - Size * 0.5f - 6.f, P.Y - Size * 0.5f - 6.f, LowerDeckZ + Height * 0.7f),
			FVector(P.X + Size * 0.5f + 6.f, P.Y + Size * 0.5f + 6.f, LowerDeckZ + Height * 0.7f + 12.f), Visuals->Emissive(Theme.AccentAlt, 5.f));
	}
}

void AChaosArenaManager::AddPit(float X0, float X1, float Y0, float Y1)
{
	UChaosVisuals* Visuals = UChaosVisuals::Get(this);
	UMaterialInterface* Lip = Visuals->Hazard(FLinearColor(1.f, 0.5f, 0.04f), 0.5f);
	UMaterialInterface* Warning = Visuals->Emissive(ChaosUtil::DangerColor, 6.f, 4.f, 0.35f);

	// Hazard-striped lip around the drop, flush with the floor.
	const float W = 26.f;
	AddBox(FVector(X0 - W, Y0 - W, 0.f), FVector(X0, Y1 + W, 1.6f), Lip);
	AddBox(FVector(X1, Y0 - W, 0.f), FVector(X1 + W, Y1 + W, 1.6f), Lip);
	AddBox(FVector(X0, Y0 - W, 0.f), FVector(X1, Y0, 1.6f), Lip);
	AddBox(FVector(X0, Y1, 0.f), FVector(X1, Y1 + W, 1.6f), Lip);

	// Pulsing red warning light line just inside the drop.
	AddBox(FVector(X0, Y0, -70.f), FVector(X0 + 3.f, Y1, -60.f), Warning);
	AddBox(FVector(X1 - 3.f, Y0, -70.f), FVector(X1, Y1, -60.f), Warning);
	AddBox(FVector(X0, Y0, -70.f), FVector(X1, Y0 + 3.f, -60.f), Warning);
	AddBox(FVector(X0, Y1 - 3.f, -70.f), FVector(X1, Y1, -60.f), Warning);
}

void AChaosArenaManager::AddCore(const FVector2D& Location, float PedestalHeight)
{
	UChaosVisuals* Visuals = UChaosVisuals::Get(this);
	const FLinearColor Gold = ChaosUtil::CoreColor;

	if (PedestalHeight > 0.f)
	{
		// Two-tier objective pedestal; each step stays under the character's max step height.
		const float Step = PedestalHeight * 0.5f;
		AddShape(EChaosShape::Cylinder, FVector(Location.X, Location.Y, Step * 0.5f), FVector(320.f, 320.f, Step), WallMaterial(), true);
		AddShape(EChaosShape::Cylinder, FVector(Location.X, Location.Y, Step + Step * 0.5f), FVector(200.f, 200.f, Step), TrimMaterial(), true);
		AddShape(EChaosShape::Cylinder, FVector(Location.X, Location.Y, Step + 1.f), FVector(330.f, 330.f, 3.f), Visuals->Emissive(Gold, 2.f, 2.f, 0.3f));
		AddShape(EChaosShape::Cylinder, FVector(Location.X, Location.Y, Step + 1.5f), FVector(306.f, 306.f, 3.f), WallMaterial());
		AddShape(EChaosShape::Cylinder, FVector(Location.X, Location.Y, PedestalHeight + 1.f), FVector(200.f, 200.f, 2.f), Visuals->Emissive(Gold, 2.5f, 2.f, 0.3f));
		AddShape(EChaosShape::Cylinder, FVector(Location.X, Location.Y, PedestalHeight + 1.5f), FVector(176.f, 176.f, 2.f), UnderMaterial());
	}
	else
	{
		// Ground socket ring.
		AddShape(EChaosShape::Cylinder, FVector(Location.X, Location.Y, 1.f), FVector(240.f, 240.f, 2.f), UnderMaterial());
		AddShape(EChaosShape::Cylinder, FVector(Location.X, Location.Y, 1.5f), FVector(200.f, 200.f, 2.f), Visuals->Emissive(Gold, 2.5f, 2.f, 0.3f));
		AddShape(EChaosShape::Cylinder, FVector(Location.X, Location.Y, 2.f), FVector(176.f, 176.f, 2.f), UnderMaterial());
	}

	const FVector CoreLocation(Location.X, Location.Y, PedestalHeight + 70.f);
	if (AChaosObjective* Core = SpawnArenaActor<AChaosObjective>(FTransform(CoreLocation)))
	{
		Objectives.Add(Core);
	}
}

void AChaosArenaManager::AddLaser(const FVector2D& Center, float Width, float Yaw)
{
	const FTransform LaserTransform(FRotator(0.f, Yaw, 0.f), FVector(Center.X, Center.Y, 0.f));
	if (AChaosLaserBarrier* Laser = SpawnArenaActor<AChaosLaserBarrier>(LaserTransform))
	{
		Laser->ConfigureBarrier(Width, WallHeight - 20.f);
		Traps.Add(Laser);
	}
}

void AChaosArenaManager::AddPlatform(const FVector2D& Center, const FVector2D& Size, const FVector& Axis, float Amplitude)
{
	if (AChaosMovingPlatform* Platform = SpawnArenaActor<AChaosMovingPlatform>(FTransform(FVector(Center.X, Center.Y, 0.f))))
	{
		Platform->ConfigurePlatform(FVector(Size.X, Size.Y, FloorThickness), Axis, Amplitude);
		Traps.Add(Platform);
	}
}

void AChaosArenaManager::AddStunPad(const FVector2D& Center, const FVector2D& Size)
{
	if (AChaosStunTrap* Pad = SpawnArenaActor<AChaosStunTrap>(FTransform(FVector(Center.X, Center.Y, 0.f))))
	{
		Pad->ConfigurePad(Size);
		Traps.Add(Pad);
	}
}

void AChaosArenaManager::AddExit(const FVector2D& Center)
{
	Exit = SpawnArenaActor<AChaosExit>(FTransform(FVector(Center.X, Center.Y, 0.f)));
}

// ---------------------------------------------------------------------------------------------
// Arena layouts (gameplay geometry unchanged; set dressing per theme)
// ---------------------------------------------------------------------------------------------

void AChaosArenaManager::BuildNeonGrid()
{
	// Compact training arena: a dividing wall with two doors and one trench bridged by a platform.
	RunnerSpawn = FVector(-950.f, 0.f, 0.f);
	SaboteurSpawn = FVector(300.f, -1300.f, 0.f);
	UChaosVisuals* Visuals = UChaosVisuals::Get(this);

	AddPerimeter();
	AddFacilityBackdrop();

	// Floor, leaving a trench at X[50,600] x Y[200,MaxY].
	AddFloor(MinX, 50.f, MinY, MaxY);
	AddFloor(600.f, MaxX, MinY, MaxY);
	AddFloor(50.f, 600.f, MinY, 200.f);
	AddPit(50.f, 600.f, 200.f, MaxY);

	// Dividing wall with doors at Y[-1000,-700] and Y[400,700].
	AddWall(-350.f, -300.f, MinY, -1000.f);
	AddWall(-350.f, -300.f, -700.f, 400.f);
	AddWall(-350.f, -300.f, 700.f, MaxY);

	// Low training cover with hazard-striped tops.
	const FVector2D Cover[] = { FVector2D(-800.f, -400.f), FVector2D(-800.f, 1000.f), FVector2D(850.f, -900.f) };
	for (const FVector2D& C : Cover)
	{
		AddWall(C.X - 100.f, C.X + 100.f, C.Y - 100.f, C.Y + 100.f, 100.f);
	}

	// Cyan route channels: the spawn-to-exit spine and a guide loop.
	AddChannel(-1060.f, -350.f, -5.f, 5.f);
	AddChannel(-300.f, 800.f, -5.f, 5.f);
	AddChannel(-1060.f, -360.f, -1525.f, -1515.f);
	AddChannel(-290.f, 1060.f, -1525.f, -1515.f);
	AddChannel(-1060.f, -360.f, 1515.f, 1525.f);

	// North-wall displays facing the camera.
	for (const float Y : { -1100.f, 0.f, 1100.f })
	{
		AddScreen(FVector(MaxX - 4.f, Y, 140.f), 420.f, 110.f, Theme.Accent);
	}

	// Structured cool lighting along the long walls.
	for (const float X : { -700.f, 100.f, 800.f })
	{
		AddAccentLight(FVector(X, MinY + 120.f, 200.f), Theme.AccentAlt, Theme.AccentLightIntensity, 1100.f);
		AddAccentLight(FVector(X, MaxY - 120.f, 200.f), Theme.AccentAlt, Theme.AccentLightIntensity, 1100.f);
	}

	AddCore(FVector2D(-750.f, -1250.f));
	AddCore(FVector2D(300.f, -700.f), 80.f);
	AddCore(FVector2D(850.f, 1250.f));

	AddLaser(FVector2D(-325.f, -850.f), 300.f);
	AddLaser(FVector2D(-325.f, 550.f), 300.f);
	AddPlatform(FVector2D(325.f, 900.f), PlatformSize, AlongY, 450.f);
	AddStunPad(FVector2D(325.f, -250.f), FVector2D(300.f, 300.f));
	AddStunPad(FVector2D(750.f, -400.f), FVector2D(250.f, 250.f));

	AddExit(FVector2D(950.f, 0.f));
	(void)Visuals;
}

void AChaosArenaManager::BuildReactorRun()
{
	// A reactor block splits the arena into two lanes; each lane crosses a pit on a platform.
	RunnerSpawn = FVector(-950.f, 0.f, 0.f);
	SaboteurSpawn = FVector(450.f, 1000.f, 0.f);
	UChaosVisuals* Visuals = UChaosVisuals::Get(this);

	AddPerimeter();
	AddFacilityBackdrop();

	AddFloor(MinX, -PitHalfWidth, MinY, MaxY);
	AddFloor(PitHalfWidth, MaxX, MinY, MaxY);
	AddPit(-PitHalfWidth, PitHalfWidth, MinY, -400.f);
	AddPit(-PitHalfWidth, PitHalfWidth, 400.f, MaxY);

	// ---- Reactor core (collision is the original block; visuals are layered machinery) ----
	if (Geometry)
	{
		Geometry->AddPiece(EChaosShape::Cube, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 155.f), FVector(9.f, 8.f, 3.9f)), nullptr, true, false);
	}
	AddBox(FVector(-450.f, -400.f, -FloorThickness), FVector(450.f, 400.f, 60.f), WallMaterial());
	AddBox(FVector(-460.f, -410.f, 60.f), FVector(460.f, 410.f, 70.f), TrimMaterial());
	AddShape(EChaosShape::Cylinder, FVector(0.f, 0.f, 230.f), FVector(300.f, 300.f, 320.f), Visuals->Emissive(Theme.Accent, 9.f, 2.2f, 0.25f));
	AddShape(EChaosShape::Cylinder, FVector(0.f, 0.f, 230.f), FVector(520.f, 520.f, 330.f), Visuals->Energy(Theme.Accent, 1.4f, 0.85f, 0.5f, 0.15f));
	AddShape(EChaosShape::Cylinder, FVector(0.f, 0.f, 405.f), FVector(560.f, 560.f, 20.f), TrimMaterial());
	AddShape(EChaosShape::Cylinder, FVector(0.f, 0.f, 418.f), FVector(330.f, 330.f, 8.f), Visuals->Emissive(Theme.Accent, 8.f, 3.f, 0.4f));
	AddShape(EChaosShape::Cylinder, FVector(0.f, 0.f, 420.f), FVector(280.f, 280.f, 8.f), TrimMaterial());
	AddShape(EChaosShape::Cylinder, FVector(0.f, 0.f, 424.f), FVector(90.f, 90.f, 6.f), Visuals->Emissive(Theme.Accent, 10.f, 3.f, 0.4f));
	for (const FVector2D& Corner : { FVector2D(-380.f, -330.f), FVector2D(-380.f, 330.f), FVector2D(380.f, -330.f), FVector2D(380.f, 330.f) })
	{
		AddShape(EChaosShape::Cylinder, FVector(Corner.X, Corner.Y, 220.f), FVector(90.f, 90.f, 440.f), TrimMaterial());
		for (const float Z : { 140.f, 300.f })
		{
			AddShape(EChaosShape::Cylinder, FVector(Corner.X, Corner.Y, Z), FVector(96.f, 96.f, 12.f), Visuals->Emissive(Theme.Accent, 8.f));
		}
	}
	AddBox(FVector(-400.f, -350.f, 420.f), FVector(400.f, -310.f, 445.f), TrimMaterial());
	AddBox(FVector(-400.f, 310.f, 420.f), FVector(400.f, 350.f, 445.f), TrimMaterial());

	if (AChaosSpinner* Ring = SpawnArenaActor<AChaosSpinner>(FTransform(FVector(0.f, 0.f, 330.f))))
	{
		Ring->RotationRate = FRotator(0.f, 40.f, 0.f);
		UMaterialInterface* RingGlow = Visuals->Emissive(Theme.Accent, 8.f);
		for (int32 Segment = 0; Segment < 16; ++Segment)
		{
			const float Angle = Segment * 360.f / 16.f;
			const FVector Offset = FRotator(0.f, Angle, 0.f).RotateVector(FVector(285.f, 0.f, 0.f));
			Ring->AddPart(EChaosShape::Cube, FTransform(FRotator(0.f, Angle, 0.f), Offset, FVector(0.18f, 0.7f, 0.08f)), RingGlow);
		}
	}

	// Gate wall with a door into each lane, amber warning beacons on top.
	FLinearColor Amber = ChaosUtil::WarningColor;
	AddWall(-650.f, -600.f, MinY, -1150.f, WallHeight, &Amber);
	AddWall(-650.f, -600.f, -850.f, 850.f, WallHeight, &Amber);
	AddWall(-650.f, -600.f, 1150.f, MaxY, WallHeight, &Amber);
	for (const float Y : { -1150.f, -850.f, 850.f, 1150.f })
	{
		AddShape(EChaosShape::Sphere, FVector(-625.f, Y, WallHeight + 25.f), FVector(30.f), Visuals->Emissive(ChaosUtil::WarningColor, 14.f, 5.f, 0.6f));
	}

	// Coolant pillars and pipe runs along the long walls.
	AddWall(650.f, 800.f, -750.f, -600.f);
	AddWall(650.f, 800.f, 600.f, 750.f);
	for (const float Y : { MinY - 30.f, MaxY + 30.f })
	{
		for (const float Offset : { -16.f, 16.f })
		{
			AddShape(EChaosShape::Cylinder, FVector(0.f, Y + Offset, WallHeight + 34.f), FVector(26.f, 26.f, 2200.f), TrimMaterial(), false, FRotator(90.f, 0.f, 0.f));
		}
	}

	// Cooling towers rising from the lower level beside the arena.
	for (const FVector2D& Tower : { FVector2D(-400.f, -2250.f), FVector2D(700.f, -2250.f), FVector2D(-400.f, 2250.f), FVector2D(700.f, 2250.f) })
	{
		AddShape(EChaosShape::Cylinder, FVector(Tower.X, Tower.Y, LowerDeckZ + 420.f), FVector(460.f, 460.f, 840.f), WallMaterial());
		AddShape(EChaosShape::Cylinder, FVector(Tower.X, Tower.Y, LowerDeckZ + 846.f), FVector(400.f, 400.f, 12.f), Visuals->Emissive(Theme.Accent, 6.f, 1.5f, 0.3f));
		AddShape(EChaosShape::Cylinder, FVector(Tower.X, Tower.Y, LowerDeckZ + 600.f), FVector(470.f, 470.f, 16.f), Visuals->Emissive(Theme.Accent, 5.f));
	}

	// Lane flow channels from spawn to each gate door.
	AddChannel(-1060.f, -660.f, -1005.f, -995.f);
	AddChannel(-1060.f, -660.f, 995.f, 1005.f);
	AddChannel(300.f, 800.f, -5.f, 5.f);

	for (const FVector2D& LightSpot : { FVector2D(-850.f, -1000.f), FVector2D(-850.f, 1000.f), FVector2D(700.f, -1200.f), FVector2D(700.f, 1200.f) })
	{
		AddAccentLight(FVector(LightSpot.X, LightSpot.Y, 260.f), Theme.Accent, Theme.AccentLightIntensity, 1000.f);
	}
	AddAccentLight(FVector(0.f, 0.f, 520.f), Theme.Accent, 120.f, 1400.f);

	AddCore(FVector2D(-900.f, -1350.f), 80.f);
	AddCore(FVector2D(650.f, -1300.f));
	AddCore(FVector2D(650.f, 1300.f));

	AddLaser(FVector2D(-625.f, -1000.f), 300.f);
	AddLaser(FVector2D(-625.f, 1000.f), 300.f);
	AddPlatform(FVector2D(0.f, -1000.f), PlatformSize, AlongY, 400.f);
	AddPlatform(FVector2D(0.f, 1000.f), PlatformSize, AlongY, 400.f);
	AddStunPad(FVector2D(-450.f, -1000.f), FVector2D(250.f, 300.f));
	AddStunPad(FVector2D(-450.f, 1000.f), FVector2D(250.f, 300.f));
	AddStunPad(FVector2D(650.f, 0.f), FVector2D(250.f, 300.f));

	AddExit(FVector2D(950.f, 0.f));
}

void AChaosArenaManager::BuildChaosCore()
{
	// Narrow doors on two dividers, a central chasm with a platform, and narrow side passages.
	RunnerSpawn = FVector(-950.f, 0.f, 0.f);
	SaboteurSpawn = FVector(400.f, 0.f, 0.f);
	UChaosVisuals* Visuals = UChaosVisuals::Get(this);

	AddPerimeter();
	AddFacilityBackdrop();

	AddFloor(MinX, -PitHalfWidth, MinY, MaxY);
	AddFloor(PitHalfWidth, MaxX, MinY, MaxY);
	AddFloor(-PitHalfWidth, PitHalfWidth, MinY, -900.f);
	AddFloor(-PitHalfWidth, PitHalfWidth, 900.f, MaxY);
	AddPit(-PitHalfWidth, PitHalfWidth, -900.f, 900.f);

	// ---- The Chaos Core: a giant energy sphere suspended in the chasm ----
	AddShape(EChaosShape::Sphere, FVector(0.f, 0.f, -480.f), FVector(460.f), Visuals->Emissive(Theme.Accent, 12.f, 1.6f, 0.3f));
	AddShape(EChaosShape::Sphere, FVector(0.f, 0.f, -480.f), FVector(640.f), Visuals->Energy(Theme.AccentAlt, 2.2f, 0.9f, 0.4f, 0.2f));
	for (const float Pitch : { 0.f, 60.f, -60.f })
	{
		if (AChaosSpinner* Ring = SpawnArenaActor<AChaosSpinner>(FTransform(FRotator(Pitch, 0.f, 0.f), FVector(0.f, 0.f, -480.f))))
		{
			Ring->RotationRate = FRotator(0.f, Pitch == 0.f ? 35.f : -25.f, 0.f);
			UMaterialInterface* RingGlow = Visuals->Emissive(Pitch == 0.f ? Theme.Accent : Theme.AccentAlt, 9.f);
			for (int32 Segment = 0; Segment < 20; ++Segment)
			{
				const float Angle = Segment * 360.f / 20.f;
				const FVector Offset = FRotator(0.f, Angle, 0.f).RotateVector(FVector(420.f, 0.f, 0.f));
				Ring->AddPart(EChaosShape::Cube, FTransform(FRotator(0.f, Angle, 0.f), Offset, FVector(0.14f, 0.9f, 0.08f)), RingGlow);
			}
		}
	}
	AddAccentLight(FVector(0.f, 0.f, -150.f), Theme.Accent, 260.f, 1800.f);

	// Energy conduits running down the chasm walls.
	UMaterialInterface* Conduit = Visuals->Emissive(Theme.AccentAlt, 7.f, 0.f, 0.f, 0.8f);
	for (float Y = -750.f; Y <= 750.f; Y += 300.f)
	{
		AddBox(FVector(-PitHalfWidth, Y - 6.f, -UnderDepth), FVector(-PitHalfWidth + 3.f, Y + 6.f, -50.f), Conduit);
		AddBox(FVector(PitHalfWidth - 3.f, Y - 6.f, -UnderDepth), FVector(PitHalfWidth, Y + 6.f, -50.f), Conduit);
	}

	// Dividers trimmed in crimson.
	FLinearColor Crimson(1.f, 0.05f, 0.15f);
	AddWall(-600.f, -550.f, MinY, -1350.f, WallHeight, &Crimson);
	AddWall(-600.f, -550.f, -1100.f, -125.f, WallHeight, &Crimson);
	AddWall(-600.f, -550.f, 125.f, 1100.f, WallHeight, &Crimson);
	AddWall(-600.f, -550.f, 1350.f, MaxY, WallHeight, &Crimson);
	AddWall(550.f, 600.f, MinY, -700.f, WallHeight, &Crimson);
	AddWall(550.f, 600.f, -450.f, 450.f, WallHeight, &Crimson);
	AddWall(550.f, 600.f, 700.f, MaxY, WallHeight, &Crimson);

	// Pillars that narrow the passages, capped with energy emitters.
	const FVector4 Pillars[] = {
		FVector4(-75.f, 75.f, -1325.f, -1175.f), FVector4(-75.f, 75.f, 1275.f, 1425.f),
		FVector4(-950.f, -750.f, -700.f, -500.f), FVector4(-950.f, -750.f, 500.f, 700.f), FVector4(800.f, 950.f, 700.f, 850.f),
	};
	for (const FVector4& P : Pillars)
	{
		AddWall(P.X, P.Y, P.Z, P.W, 260.f, &Theme.AccentAlt);
		AddShape(EChaosShape::Sphere, FVector((P.X + P.Y) * 0.5f, (P.Z + P.W) * 0.5f, 300.f), FVector(50.f), Visuals->Energy(Theme.Accent, 4.f, 0.8f, 0.f, 0.3f));
	}

	AddChannel(-1060.f, -610.f, -5.f, 5.f);
	AddChannel(610.f, 800.f, -5.f, 5.f);

	for (const float Y : { -1100.f, 1100.f })
	{
		AddScreen(FVector(MaxX - 4.f, Y, 140.f), 380.f, 100.f, Theme.AccentAlt);
	}
	for (const FVector2D& LightSpot : { FVector2D(-850.f, -1250.f), FVector2D(-850.f, 1250.f), FVector2D(850.f, -1250.f), FVector2D(850.f, 1250.f) })
	{
		AddAccentLight(FVector(LightSpot.X, LightSpot.Y, 240.f), Theme.AccentAlt, Theme.AccentLightIntensity, 900.f);
	}

	AddCore(FVector2D(-850.f, 1350.f), 80.f);
	AddCore(FVector2D(0.f, -1475.f));
	AddCore(FVector2D(850.f, -1350.f), 80.f);

	AddLaser(FVector2D(-575.f, -1225.f), 250.f);
	AddLaser(FVector2D(-575.f, 0.f), 250.f);
	AddLaser(FVector2D(575.f, 575.f), 250.f);
	AddPlatform(FVector2D(0.f, 0.f), PlatformSize, AlongY, 500.f);
	AddStunPad(FVector2D(0.f, 1050.f), FVector2D(250.f, 250.f));
	AddStunPad(FVector2D(-800.f, -1000.f), FVector2D(250.f, 250.f));
	AddStunPad(FVector2D(750.f, -575.f), FVector2D(250.f, 250.f));

	AddExit(FVector2D(950.f, 0.f));
}
