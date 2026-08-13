// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BMCoreTypes.h"

/** Runtime-authored level definition used by the source-only campaign. */
struct BLACKMERIDIANRUNTIME_API FBMCampaignLevelDefinition
{
    EBMCampaignLevel Level = EBMCampaignLevel::SableQuay;
    FName LevelId = TEXT("SableQuay");
    FText DisplayName;
    FText Briefing;
    FVector StartLocation = FVector(0.0f, -950.0f, 120.0f);
    FRotator StartRotation = FRotator::ZeroRotator;
    FVector InsertionLocation = FVector(250.0f, -500.0f, 100.0f);
    FVector CheckpointLocation = FVector(400.0f, 0.0f, 100.0f);
    FVector BreachLocation = FVector(780.0f, 850.0f, 160.0f);
    FVector IntelLocation = FVector(850.0f, 80.0f, 70.0f);
    FVector OpenCombatLocation = FVector(1500.0f, 0.0f, 100.0f);
    FVector ExtractionLocation = FVector(1500.0f, 650.0f, 110.0f);
    FVector WeaponPickupLocation = FVector(1600.0f, 850.0f, 85.0f);
    FVector AmmoPickupLocation = FVector(520.0f, -180.0f, 95.0f);
    FVector ThrowablePickupLocation = FVector(1240.0f, -220.0f, 85.0f);
    EBMWeaponId PickupWeaponId = EBMWeaponId::RookDMR;
    int32 CheckpointWaveSize = 5;
    int32 OpenCombatWaveSize = 8;
    int32 FinalDefenseWaveSize = 10;
    /** Map-authored hostile pressure adjustment for long sightlines and hazards. */
    float HostileDamageScale = 1.0f;
    /** Selects a deterministic visual treatment for the shared runtime map. */
    int32 EnvironmentVariant = 0;
    FColor EnvironmentPrimaryColor = FColor(38, 48, 60);
    FColor EnvironmentAccentColor = FColor(25, 185, 175);
    /** Broad art family used to select the level's terrain, atmosphere, and weather language. */
    EBMEnvironmentTheme EnvironmentTheme = EBMEnvironmentTheme::OpenDry;
    EBMWeatherType WeatherType = EBMWeatherType::None;
    /** Unique art-direction profile for this campaign entry. */
    FName EnvironmentThemeId = TEXT("SABLE_SUNBREAK");
    /** Ground/terrain kit identifier used by runtime dressing and QA logs. */
    FName EnvironmentTerrainId = TEXT("OPEN_DRY_TERRAIN");
    /** Structural layout identifier; unlike weather, this changes collision and traversal. */
    FName EnvironmentLayoutId = TEXT("LAYOUT_OPEN_DRY_RIDGE");
    /** Prop language identifier used by runtime dressing and QA logs. */
    FName EnvironmentPropSetId = TEXT("DRY_RELAY_YARD");
    /** Sky and horizon treatment identifier used by runtime lighting. */
    FName EnvironmentSkyId = TEXT("CLEAR_BLUE_HORIZON");
    /** Sound routing key resolved through the campaign audio profile. */
    FName EnvironmentAmbienceId = TEXT("SableQuay");
    /** Authored weather VFX route selected by this level profile. */
    FName EnvironmentWeatherSystemId = TEXT("NONE");
    FString EnvironmentWeatherSystemPath;
    FColor EnvironmentGroundColor = FColor(86, 92, 88);
    FColor EnvironmentSkyColor = FColor(80, 112, 148);
    FColor EnvironmentFogColor = FColor(36, 52, 68);
    FColor EnvironmentWeatherColor = FColor(180, 195, 210);
    float EnvironmentTerrainMetallic = 0.46f;
    float EnvironmentTerrainRoughness = 0.72f;
    // Runtime-generated geometry has no baked Lightmass contribution, but the
    // compatibility renderer still needs a bounded lighting budget.  Theme
    // profiles may tune these values, while ApplyEnvironmentTheme keeps them
    // inside a readable range for every operation.
    float EnvironmentSunIntensity = 1.25f;
    float EnvironmentSkyIntensity = 0.55f;
    float EnvironmentSunYawOffset = 0.0f;
    float EnvironmentFogDensity = 0.0016f;
    float EnvironmentFogHeightFalloff = 0.22f;
    float EnvironmentFogStartDistance = 560.0f;
    float EnvironmentFogMaxOpacity = 0.17f;
    float EnvironmentWeatherIntensity = 0.0f;
    float EnvironmentWeatherParticleScale = 1.0f;
    float EnvironmentWeatherFallSpeed = 0.0f;
    float EnvironmentWeatherWindSpeed = 0.0f;
    float EnvironmentAmbientVolume = 0.08f;
    bool bInteriorEnvironment = false;
    bool bStormLightning = false;
    FString EnvironmentAmbientSoundPath;
};

/** Number of runtime campaign variants exposed by the level selector. */
inline int32 GetBMCampaignLevelCount()
{
    return static_cast<int32>(EBMCampaignLevel::LastLight) + 1;
}

BLACKMERIDIANRUNTIME_API FBMCampaignLevelDefinition GetCampaignLevelDefinition(EBMCampaignLevel Level);

/** Compact four-row selector text shared by the native UMG and legacy HUD. */
BLACKMERIDIANRUNTIME_API FString GetBMCampaignLevelListText(EBMCampaignLevel SelectedLevel);
