// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMCampaignLevel.h"

namespace
{
    struct FEnvironmentIdentitySpec
    {
        const TCHAR* ThemeId;
        const TCHAR* TerrainId;
        const TCHAR* PropSetId;
        const TCHAR* SkyId;
        const TCHAR* AmbienceId;
        const TCHAR* AmbientSoundPath;
    };

    const FEnvironmentIdentitySpec EnvironmentIdentitySpecs[] = {
        {TEXT("SABLE_SUNBREAK"), TEXT("OPEN_DRY_RIDGE"), TEXT("DRY_RELAY_YARD"), TEXT("SKY_CLEAR_BLUE"), TEXT("SableQuay"), TEXT("/Game/Audio/Generated/BM_Ambient_SableQuay.BM_Ambient_SableQuay")},
        {TEXT("FLOODLINE_FLOODGATES"), TEXT("FLOODED_DRAINAGE_YARD"), TEXT("FLOODGATE_STORMWORKS"), TEXT("SKY_STORM_FRONT"), TEXT("Floodline"), TEXT("/Game/Audio/Generated/BM_Ambient_Floodline.BM_Ambient_Floodline")},
        {TEXT("RELAYHOUSE_ROOFTOP"), TEXT("ROOFTOP_RELAY"), TEXT("ROOFTOP_SERVICE"), TEXT("SKY_OVERCAST_CITY"), TEXT("RelayHouse"), TEXT("/Game/Audio/Generated/BM_Ambient_RelayHouse.BM_Ambient_RelayHouse")},
        {TEXT("NIGHTGLASS_UNDERCROFT"), TEXT("UNDERGROUND_CORE"), TEXT("CORE_TUNNELS"), TEXT("SKY_NONE_COREGLOW"), TEXT("NightglassCore"), TEXT("/Game/Audio/Generated/BM_Ambient_NightglassCore.BM_Ambient_NightglassCore")},
        {TEXT("ASHEN_DOCKS"), TEXT("CINDERED_DOCKS"), TEXT("BURNING_GANTRIES"), TEXT("SKY_SMOKE_RED"), TEXT("AshenDocks"), TEXT("/Game/Audio/Generated/BM_Ambient_AshenDocks.BM_Ambient_AshenDocks")},
        {TEXT("CINDER_FOUNDRY"), TEXT("FOUNDRY_KILNS"), TEXT("HOT_CASTING_LINE"), TEXT("SKY_FURNACE_GLOW"), TEXT("CinderWorks"), TEXT("/Game/Audio/Generated/BM_Ambient_CinderWorks.BM_Ambient_CinderWorks")},
        {TEXT("SALT_FLATS"), TEXT("SALT_EVAPORATION_BASIN"), TEXT("SALT_DEPOT"), TEXT("SKY_WHITE_HAZE"), TEXT("SaltFoundry"), TEXT("/Game/Audio/Generated/BM_Ambient_SaltFoundry.BM_Ambient_SaltFoundry")},
        {TEXT("BLACKWATER_TANKFARM"), TEXT("OIL_TANK_FARM"), TEXT("PUMP_STATION"), TEXT("SKY_OIL_MIST"), TEXT("BlackwaterYard"), TEXT("/Game/Audio/Generated/BM_Ambient_BlackwaterYard.BM_Ambient_BlackwaterYard")},
        {TEXT("GLASS_ARCADES"), TEXT("SHATTERED_MARKET"), TEXT("SIGNAL_MIRRORS"), TEXT("SKY_MAGENTA_DUSK"), TEXT("GlassMarket"), TEXT("/Game/Audio/Generated/BM_Ambient_GlassMarket.BM_Ambient_GlassMarket")},
        {TEXT("STORM_VAULT"), TEXT("HIGH_STORM_CATWALKS"), TEXT("LIGHTNING_GRID"), TEXT("SKY_ELECTRIC_STORM"), TEXT("StormVault"), TEXT("/Game/Audio/Generated/BM_Ambient_StormVault.BM_Ambient_StormVault")},
        {TEXT("RED_TIDE_BREAKWATER"), TEXT("CHEMICAL_SEAWALL"), TEXT("FILTER_STATIONS"), TEXT("SKY_RED_TIDE"), TEXT("RedTide"), TEXT("/Game/Audio/Generated/BM_Ambient_RedTide.BM_Ambient_RedTide")},
        {TEXT("IRON_CATHEDRAL"), TEXT("TURBINE_AISLES"), TEXT("IRON_RELAY_SHRINE"), TEXT("SKY_TURBINE_VAULT"), TEXT("IronCathedral"), TEXT("/Game/Audio/Generated/BM_Ambient_IronCathedral.BM_Ambient_IronCathedral")},
        {TEXT("LANTERN_FOREST"), TEXT("DAMP_FOREST_BASIN"), TEXT("LANTERN_CHAIN"), TEXT("SKY_FOREST_MIST"), TEXT("LanternBasin"), TEXT("/Game/Audio/Generated/BM_Ambient_LanternBasin.BM_Ambient_LanternBasin")},
        {TEXT("QUARRY_CANYON"), TEXT("DESERT_SIGNAL_CANYON"), TEXT("EXCAVATOR_RIGS"), TEXT("SKY_DUST_CANYON"), TEXT("QuarryNine"), TEXT("/Game/Audio/Generated/BM_Ambient_QuarryNine.BM_Ambient_QuarryNine")},
        {TEXT("VANTA_SPIRE"), TEXT("SPIRE_SERVICE_RIDGE"), TEXT("SPIRE_RADAR_TOWERS"), TEXT("SKY_VIOLET_DAWN"), TEXT("VantaSpire"), TEXT("/Game/Audio/Generated/BM_Ambient_VantaSpire.BM_Ambient_VantaSpire")},
        {TEXT("WRECKLINE_HULLS"), TEXT("CONTAINER_WRECKLINE"), TEXT("DEAD_RADAR_HULKS"), TEXT("SKY_SALT_SQUALL"), TEXT("Wreckline"), TEXT("/Game/Audio/Generated/BM_Ambient_Wreckline.BM_Ambient_Wreckline")},
        {TEXT("EMBER_FUEL_STATION"), TEXT("FUEL_SERVICE_RACKS"), TEXT("IGNITION_TANKS"), TEXT("SKY_HEAT_SHIMMER"), TEXT("EmberStation"), TEXT("/Game/Audio/Generated/BM_Ambient_EmberStation.BM_Ambient_EmberStation")},
        {TEXT("PALE_ICE_HARBOR"), TEXT("ICE_BERTHS"), TEXT("FROZEN_CRANES"), TEXT("SKY_POLAR_WHITEOUT"), TEXT("PaleHarbor"), TEXT("/Game/Audio/Generated/BM_Ambient_PaleHarbor.BM_Ambient_PaleHarbor")},
        {TEXT("MERIDIAN_GATE"), TEXT("GATE_CHECKPOINT"), TEXT("LATTICE_CONVOY"), TEXT("SKY_COLD_CITY"), TEXT("MeridianGate"), TEXT("/Game/Audio/Generated/BM_Ambient_MeridianGate.BM_Ambient_MeridianGate")},
        {TEXT("LAST_LIGHT_RIDGE"), TEXT("LAST_LIGHT_RIDGE"), TEXT("LAST_LIGHT_PIER"), TEXT("SKY_GOLDEN_LASTLIGHT"), TEXT("LastLight"), TEXT("/Game/Audio/Generated/BM_Ambient_LastLight.BM_Ambient_LastLight")}};

    const FEnvironmentIdentitySpec& GetEnvironmentIdentity(const EBMCampaignLevel Level)
    {
        const int32 Index = FMath::Clamp(static_cast<int32>(Level), 0, UE_ARRAY_COUNT(EnvironmentIdentitySpecs) - 1);
        return EnvironmentIdentitySpecs[Index];
    }

    EBMEnvironmentTheme ResolveEnvironmentTheme(const EBMCampaignLevel Level)
    {
        // Reuse the seven art families intentionally, but distribute them so
        // the 20-operation campaign changes weather and spatial language
        // throughout the run instead of presenting one long industrial night.
        static const EBMEnvironmentTheme Themes[] = {
            EBMEnvironmentTheme::OpenDry,
            EBMEnvironmentTheme::CoastalStorm,
            EBMEnvironmentTheme::Interior,
            EBMEnvironmentTheme::Underground,
            EBMEnvironmentTheme::Desert,
            EBMEnvironmentTheme::Underground,
            EBMEnvironmentTheme::Desert,
            EBMEnvironmentTheme::CoastalStorm,
            EBMEnvironmentTheme::OpenDry,
            EBMEnvironmentTheme::CoastalStorm,
            EBMEnvironmentTheme::CoastalStorm,
            EBMEnvironmentTheme::Interior,
            EBMEnvironmentTheme::Forest,
            EBMEnvironmentTheme::Desert,
            EBMEnvironmentTheme::OpenDry,
            EBMEnvironmentTheme::CoastalStorm,
            EBMEnvironmentTheme::Desert,
            EBMEnvironmentTheme::SnowIce,
            EBMEnvironmentTheme::Forest,
            EBMEnvironmentTheme::SnowIce};
        const int32 Index = FMath::Clamp(static_cast<int32>(Level), 0, UE_ARRAY_COUNT(Themes) - 1);
        return Themes[Index];
    }

    void ApplyEnvironmentTheme(FBMCampaignLevelDefinition& Definition)
    {
        Definition.EnvironmentTheme = ResolveEnvironmentTheme(Definition.Level);
        switch (Definition.EnvironmentTheme)
        {
        case EBMEnvironmentTheme::CoastalStorm:
            Definition.EnvironmentThemeId = TEXT("COASTAL_STORM");
            Definition.WeatherType = EBMWeatherType::Rain;
            Definition.EnvironmentGroundColor = FColor(26, 48, 58);
            Definition.EnvironmentSkyColor = FColor(72, 92, 118);
            Definition.EnvironmentFogColor = FColor(35, 55, 76);
            Definition.EnvironmentWeatherColor = FColor(150, 190, 225);
            Definition.EnvironmentTerrainMetallic = 0.62f;
            Definition.EnvironmentTerrainRoughness = 0.48f;
            Definition.EnvironmentSunIntensity = 0.58f;
            Definition.EnvironmentSkyIntensity = 0.72f;
            Definition.EnvironmentSunYawOffset = 18.0f;
            Definition.EnvironmentFogDensity = 0.0048f;
            Definition.EnvironmentFogHeightFalloff = 0.28f;
            Definition.EnvironmentFogStartDistance = 240.0f;
            Definition.EnvironmentFogMaxOpacity = 0.42f;
            Definition.EnvironmentWeatherIntensity = 1.0f;
            Definition.EnvironmentWeatherParticleScale = 1.25f;
            Definition.EnvironmentWeatherFallSpeed = 1550.0f;
            Definition.EnvironmentWeatherWindSpeed = 260.0f;
            Definition.EnvironmentAmbientVolume = 0.12f;
            Definition.bStormLightning = true;
            Definition.EnvironmentAmbientSoundPath = TEXT("/Game/Audio/Generated/BM_Music_Suspense.BM_Music_Suspense");
            break;
        case EBMEnvironmentTheme::SnowIce:
            Definition.EnvironmentThemeId = TEXT("SNOW_ICE");
            Definition.WeatherType = EBMWeatherType::Snow;
            Definition.EnvironmentGroundColor = FColor(164, 181, 194);
            Definition.EnvironmentSkyColor = FColor(122, 158, 194);
            Definition.EnvironmentFogColor = FColor(138, 165, 186);
            Definition.EnvironmentWeatherColor = FColor(232, 242, 255);
            Definition.EnvironmentTerrainMetallic = 0.08f;
            Definition.EnvironmentTerrainRoughness = 0.36f;
            Definition.EnvironmentSunIntensity = 0.78f;
            Definition.EnvironmentSkyIntensity = 0.92f;
            Definition.EnvironmentSunYawOffset = -14.0f;
            Definition.EnvironmentFogDensity = 0.0028f;
            Definition.EnvironmentFogHeightFalloff = 0.18f;
            Definition.EnvironmentFogStartDistance = 420.0f;
            Definition.EnvironmentFogMaxOpacity = 0.34f;
            Definition.EnvironmentWeatherIntensity = 0.82f;
            Definition.EnvironmentWeatherParticleScale = 1.20f;
            Definition.EnvironmentWeatherFallSpeed = 125.0f;
            Definition.EnvironmentWeatherWindSpeed = 90.0f;
            Definition.EnvironmentAmbientVolume = 0.09f;
            Definition.EnvironmentAmbientSoundPath = TEXT("/Game/Audio/Generated/BM_Music_Mission.BM_Music_Mission");
            break;
        case EBMEnvironmentTheme::Desert:
            Definition.EnvironmentThemeId = TEXT("DESERT");
            Definition.WeatherType = EBMWeatherType::Dust;
            Definition.EnvironmentGroundColor = FColor(143, 108, 62);
            Definition.EnvironmentSkyColor = FColor(192, 151, 106);
            Definition.EnvironmentFogColor = FColor(145, 110, 78);
            Definition.EnvironmentWeatherColor = FColor(220, 174, 105);
            Definition.EnvironmentTerrainMetallic = 0.12f;
            Definition.EnvironmentTerrainRoughness = 0.93f;
            Definition.EnvironmentSunIntensity = 0.98f;
            Definition.EnvironmentSkyIntensity = 0.72f;
            Definition.EnvironmentSunYawOffset = -26.0f;
            Definition.EnvironmentFogDensity = 0.0011f;
            Definition.EnvironmentFogHeightFalloff = 0.11f;
            Definition.EnvironmentFogStartDistance = 620.0f;
            Definition.EnvironmentFogMaxOpacity = 0.20f;
            Definition.EnvironmentWeatherIntensity = 0.46f;
            Definition.EnvironmentWeatherParticleScale = 0.90f;
            Definition.EnvironmentWeatherFallSpeed = 70.0f;
            Definition.EnvironmentWeatherWindSpeed = 180.0f;
            Definition.EnvironmentAmbientVolume = 0.08f;
            Definition.EnvironmentAmbientSoundPath = TEXT("/Game/Audio/Generated/BM_Music_Combat.BM_Music_Combat");
            break;
        case EBMEnvironmentTheme::Forest:
            Definition.EnvironmentThemeId = TEXT("FOREST");
            Definition.WeatherType = EBMWeatherType::Mist;
            Definition.EnvironmentGroundColor = FColor(44, 74, 53);
            Definition.EnvironmentSkyColor = FColor(50, 83, 86);
            Definition.EnvironmentFogColor = FColor(48, 82, 73);
            Definition.EnvironmentWeatherColor = FColor(155, 215, 180);
            Definition.EnvironmentTerrainMetallic = 0.08f;
            Definition.EnvironmentTerrainRoughness = 0.88f;
            Definition.EnvironmentSunIntensity = 0.68f;
            Definition.EnvironmentSkyIntensity = 0.82f;
            Definition.EnvironmentSunYawOffset = 32.0f;
            Definition.EnvironmentFogDensity = 0.0032f;
            Definition.EnvironmentFogHeightFalloff = 0.36f;
            Definition.EnvironmentFogStartDistance = 300.0f;
            Definition.EnvironmentFogMaxOpacity = 0.36f;
            Definition.EnvironmentWeatherIntensity = 0.40f;
            Definition.EnvironmentWeatherParticleScale = 0.82f;
            Definition.EnvironmentWeatherFallSpeed = 35.0f;
            Definition.EnvironmentWeatherWindSpeed = 45.0f;
            Definition.EnvironmentAmbientVolume = 0.10f;
            Definition.EnvironmentAmbientSoundPath = TEXT("/Game/Audio/Generated/BM_Music_Mission.BM_Music_Mission");
            break;
        case EBMEnvironmentTheme::Underground:
            Definition.EnvironmentThemeId = TEXT("UNDERGROUND");
            Definition.WeatherType = EBMWeatherType::Mist;
            // NightglassCore is an occupied reactor facility, not a purple
            // void. Slate/teal surfaces keep the core identity cold while
            // leaving the violet accent for signs, objectives, and enemy
            // silhouettes.
            Definition.EnvironmentGroundColor = FColor(36, 48, 54);
            Definition.EnvironmentSkyColor = FColor(18, 28, 36);
            Definition.EnvironmentFogColor = FColor(25, 39, 47);
            Definition.EnvironmentWeatherColor = FColor(92, 186, 176);
            Definition.EnvironmentTerrainMetallic = 0.55f;
            Definition.EnvironmentTerrainRoughness = 0.62f;
            // Underground keeps the violet suspense grade, but the runtime
            // Metal path has no baked bounce light. Lift the key and ambient
            // floor enough to read enemies, cover edges, and service panels.
            Definition.EnvironmentSunIntensity = 0.42f;
            Definition.EnvironmentSkyIntensity = 0.68f;
            Definition.EnvironmentSunYawOffset = 180.0f;
            Definition.EnvironmentFogDensity = 0.0038f;
            Definition.EnvironmentFogHeightFalloff = 0.42f;
            Definition.EnvironmentFogStartDistance = 300.0f;
            Definition.EnvironmentFogMaxOpacity = 0.30f;
            Definition.EnvironmentWeatherIntensity = 0.30f;
            Definition.EnvironmentWeatherParticleScale = 0.70f;
            Definition.EnvironmentWeatherFallSpeed = 18.0f;
            Definition.EnvironmentWeatherWindSpeed = 20.0f;
            Definition.EnvironmentAmbientVolume = 0.13f;
            Definition.bInteriorEnvironment = true;
            Definition.EnvironmentAmbientSoundPath = TEXT("/Game/Audio/Generated/BM_Music_Suspense.BM_Music_Suspense");
            break;
        case EBMEnvironmentTheme::Interior:
            Definition.EnvironmentThemeId = TEXT("INTERIOR");
            Definition.WeatherType = EBMWeatherType::Mist;
            Definition.EnvironmentGroundColor = FColor(58, 51, 49);
            Definition.EnvironmentSkyColor = FColor(44, 43, 52);
            Definition.EnvironmentFogColor = FColor(55, 45, 52);
            Definition.EnvironmentWeatherColor = FColor(245, 145, 72);
            Definition.EnvironmentTerrainMetallic = 0.42f;
            Definition.EnvironmentTerrainRoughness = 0.76f;
            Definition.EnvironmentSunIntensity = 0.28f;
            Definition.EnvironmentSkyIntensity = 0.48f;
            Definition.EnvironmentSunYawOffset = 94.0f;
            Definition.EnvironmentFogDensity = 0.0044f;
            Definition.EnvironmentFogHeightFalloff = 0.30f;
            Definition.EnvironmentFogStartDistance = 170.0f;
            Definition.EnvironmentFogMaxOpacity = 0.38f;
            Definition.EnvironmentWeatherIntensity = 0.24f;
            Definition.EnvironmentWeatherParticleScale = 0.60f;
            Definition.EnvironmentWeatherFallSpeed = 24.0f;
            Definition.EnvironmentWeatherWindSpeed = 12.0f;
            Definition.EnvironmentAmbientVolume = 0.11f;
            Definition.bInteriorEnvironment = true;
            Definition.EnvironmentAmbientSoundPath = TEXT("/Game/Audio/Generated/BM_Music_Combat.BM_Music_Combat");
            break;
        case EBMEnvironmentTheme::OpenDry:
        default:
            Definition.EnvironmentThemeId = TEXT("OPEN_DRY");
            Definition.WeatherType = EBMWeatherType::None;
            Definition.EnvironmentGroundColor = FColor(91, 95, 82);
            Definition.EnvironmentSkyColor = FColor(86, 126, 168);
            Definition.EnvironmentFogColor = FColor(52, 69, 82);
            Definition.EnvironmentWeatherColor = FColor(205, 190, 150);
            Definition.EnvironmentTerrainMetallic = 0.28f;
            Definition.EnvironmentTerrainRoughness = 0.86f;
            // The open operation is the player's first visual read of the
            // campaign. Keep it warm and readable on the runtime-lit Metal
            // path; the other families retain their own darker mood profiles.
            Definition.EnvironmentSunIntensity = 1.18f;
            Definition.EnvironmentSkyIntensity = 1.04f;
            Definition.EnvironmentSunYawOffset = 0.0f;
            Definition.EnvironmentFogDensity = 0.0008f;
            Definition.EnvironmentFogHeightFalloff = 0.12f;
            Definition.EnvironmentFogStartDistance = 880.0f;
            Definition.EnvironmentFogMaxOpacity = 0.10f;
            Definition.EnvironmentWeatherIntensity = 0.0f;
            Definition.EnvironmentWeatherParticleScale = 0.0f;
            Definition.EnvironmentWeatherFallSpeed = 0.0f;
            Definition.EnvironmentWeatherWindSpeed = 0.0f;
            Definition.EnvironmentAmbientVolume = 0.06f;
            Definition.EnvironmentAmbientSoundPath = TEXT("/Game/Audio/Generated/BM_Music_Mission.BM_Music_Mission");
            break;
        }

        // Keep every level's existing palette identity while giving it the
        // terrain/sky/fog base of its named environment family.
        const auto BlendColor = [](const FColor& A, const FColor& B, const float Alpha)
        {
            return FColor(
                static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(static_cast<float>(A.R), static_cast<float>(B.R), Alpha)), 0, 255)),
                static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(static_cast<float>(A.G), static_cast<float>(B.G), Alpha)), 0, 255)),
                static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(static_cast<float>(A.B), static_cast<float>(B.B), Alpha)), 0, 255)),
                255);
        };
        Definition.EnvironmentGroundColor = BlendColor(Definition.EnvironmentGroundColor, Definition.EnvironmentPrimaryColor, 0.22f);
        Definition.EnvironmentSkyColor = BlendColor(Definition.EnvironmentSkyColor, Definition.EnvironmentAccentColor, 0.12f);
        Definition.EnvironmentFogColor = BlendColor(Definition.EnvironmentFogColor, Definition.EnvironmentPrimaryColor, 0.18f);
        Definition.EnvironmentWeatherColor = BlendColor(Definition.EnvironmentWeatherColor, Definition.EnvironmentAccentColor, 0.20f);

        // The family controls the broad physical language (storm, snow, dry,
        // forest, underground, or interior). The profile below gives every
        // operation its own named terrain, prop, sky, and sound identity so
        // selecting a new level never silently reuses the previous scene.
        const FEnvironmentIdentitySpec& Identity = GetEnvironmentIdentity(Definition.Level);
        Definition.EnvironmentThemeId = FName(Identity.ThemeId);
        Definition.EnvironmentTerrainId = FName(Identity.TerrainId);
        Definition.EnvironmentLayoutId = FName(FString::Printf(TEXT("LAYOUT_%s"), *Definition.EnvironmentTerrainId.ToString()));
        Definition.EnvironmentPropSetId = FName(Identity.PropSetId);
        Definition.EnvironmentSkyId = FName(Identity.SkyId);
        Definition.EnvironmentAmbienceId = FName(Identity.AmbienceId);
        Definition.EnvironmentAmbientSoundPath = Identity.AmbientSoundPath;

        // Weather is part of the level profile, not an implicit family lookup.
        // That keeps the authored Niagara route visible in logs, cook rules,
        // and future designer data when a level needs a bespoke storm or
        // whiteout variant.
        Definition.EnvironmentWeatherSystemId = TEXT("NONE");
        Definition.EnvironmentWeatherSystemPath.Empty();
        switch (Definition.WeatherType)
        {
        case EBMWeatherType::Rain:
            Definition.EnvironmentWeatherSystemId = TEXT("NS_BM_Rain");
            Definition.EnvironmentWeatherSystemPath = TEXT("/Game/VFX/Generated/NS_BM_Rain.NS_BM_Rain");
            break;
        case EBMWeatherType::Snow:
            Definition.EnvironmentWeatherSystemId = TEXT("NS_BM_Snow");
            Definition.EnvironmentWeatherSystemPath = TEXT("/Game/VFX/Generated/NS_BM_Snow.NS_BM_Snow");
            break;
        case EBMWeatherType::Dust:
            Definition.EnvironmentWeatherSystemId = TEXT("NS_BM_Dust");
            Definition.EnvironmentWeatherSystemPath = TEXT("/Game/VFX/Generated/NS_BM_Dust.NS_BM_Dust");
            break;
        case EBMWeatherType::Mist:
            Definition.EnvironmentWeatherSystemId = TEXT("NS_BM_Smoke");
            Definition.EnvironmentWeatherSystemPath = TEXT("/Game/VFX/Generated/NS_BM_Smoke.NS_BM_Smoke");
            break;
        case EBMWeatherType::None:
        default:
            break;
        }

        // Small deterministic per-operation grade changes make adjacent
        // levels in the same family read as separate places. Structural
        // collision is built by ABMGreyboxDirector from EnvironmentLayoutId;
        // these values only control the lighting grade.
        static constexpr float SunMultipliers[] = {
            1.00f, 0.86f, 0.72f, 0.58f, 0.78f,
            0.92f, 1.06f, 0.74f, 0.84f, 0.68f,
            0.82f, 0.62f, 0.88f, 1.14f, 0.96f,
            0.80f, 1.10f, 0.76f, 0.90f, 1.02f};
        static constexpr float SkyMultipliers[] = {
            1.00f, 0.78f, 0.66f, 0.42f, 0.58f,
            0.52f, 1.12f, 0.64f, 0.72f, 0.48f,
            0.62f, 0.50f, 0.82f, 1.18f, 0.74f,
            0.70f, 1.08f, 0.68f, 0.86f, 1.10f};
        static constexpr float FogMultipliers[] = {
            0.84f, 1.32f, 1.08f, 1.38f, 1.22f,
            1.16f, 0.70f, 1.28f, 0.92f, 1.44f,
            1.18f, 1.34f, 1.26f, 0.62f, 0.78f,
            1.14f, 0.74f, 1.46f, 0.98f, 0.66f};
        const int32 ProfileIndex = FMath::Clamp(static_cast<int32>(Definition.Level), 0, UE_ARRAY_COUNT(SunMultipliers) - 1);
        Definition.EnvironmentSunIntensity *= SunMultipliers[ProfileIndex];
        Definition.EnvironmentSkyIntensity *= SkyMultipliers[ProfileIndex];
        Definition.EnvironmentFogDensity *= FogMultipliers[ProfileIndex];
        Definition.EnvironmentSunYawOffset += (static_cast<float>((ProfileIndex % 5) - 2) * 7.0f);
        Definition.EnvironmentWeatherParticleScale *= 0.90f + (static_cast<float>((ProfileIndex * 3) % 5) * 0.06f);
    }

    struct FCampaignLevelSpec
    {
        const TCHAR* LevelId;
        const TCHAR* DisplayName;
        const TCHAR* Briefing;
        FVector StartLocation;
        FRotator StartRotation;
        FVector InsertionLocation;
        FVector CheckpointLocation;
        FVector BreachLocation;
        FVector IntelLocation;
        FVector OpenCombatLocation;
        FVector ExtractionLocation;
        FVector WeaponPickupLocation;
        FVector AmmoPickupLocation;
        EBMWeaponId PickupWeaponId;
        int32 CheckpointWaveSize;
        int32 OpenCombatWaveSize;
        int32 FinalDefenseWaveSize;
        FColor EnvironmentPrimaryColor;
        FColor EnvironmentAccentColor;
    };

    FBMCampaignLevelDefinition MakeLevel(
        const EBMCampaignLevel Level,
        const FCampaignLevelSpec& Spec)
    {
        FBMCampaignLevelDefinition Definition;
        Definition.Level = Level;
        Definition.LevelId = FName(Spec.LevelId);
        Definition.DisplayName = FText::FromString(FString::Printf(
            TEXT("LEVEL %02d  //  %s"),
            static_cast<int32>(Level) + 1,
            Spec.DisplayName));
        Definition.Briefing = FText::FromString(Spec.Briefing);
        Definition.StartLocation = Spec.StartLocation;
        Definition.StartRotation = Spec.StartRotation;
        Definition.InsertionLocation = Spec.InsertionLocation;
        Definition.CheckpointLocation = Spec.CheckpointLocation;
        Definition.BreachLocation = Spec.BreachLocation;
        Definition.IntelLocation = Spec.IntelLocation;
        Definition.OpenCombatLocation = Spec.OpenCombatLocation;
        Definition.ExtractionLocation = Spec.ExtractionLocation;
        Definition.WeaponPickupLocation = Spec.WeaponPickupLocation;
        Definition.AmmoPickupLocation = Spec.AmmoPickupLocation;
        // Ordnance is always on the approach to the second combat pocket.
        // Keeping this relative makes the pickup move with every route.
        Definition.ThrowablePickupLocation = Spec.OpenCombatLocation + FVector(-260.0f, -220.0f, -15.0f);
        Definition.PickupWeaponId = Spec.PickupWeaponId;
        Definition.CheckpointWaveSize = Spec.CheckpointWaveSize;
        Definition.OpenCombatWaveSize = Spec.OpenCombatWaveSize;
        Definition.FinalDefenseWaveSize = Spec.FinalDefenseWaveSize;
        Definition.EnvironmentVariant = static_cast<int32>(Level);
        Definition.EnvironmentPrimaryColor = Spec.EnvironmentPrimaryColor;
        Definition.EnvironmentAccentColor = Spec.EnvironmentAccentColor;
        ApplyEnvironmentTheme(Definition);
        // These operations deliberately expose the player to unusually long
        // or elevated sightlines. Tune their pressure with the authored map
        // rather than weakening the shared weapon damage contract globally.
        switch (Level)
        {
        case EBMCampaignLevel::SaltFoundry:
            Definition.HostileDamageScale = 0.78f;
            break;
        case EBMCampaignLevel::StormVault:
            Definition.HostileDamageScale = 0.88f;
            break;
        case EBMCampaignLevel::RedTide:
            Definition.HostileDamageScale = 0.78f;
            break;
        case EBMCampaignLevel::PaleHarbor:
            Definition.HostileDamageScale = 0.72f;
            break;
        default:
            Definition.HostileDamageScale = 1.0f;
            break;
        }
        return Definition;
    }

    const FCampaignLevelSpec CampaignLevelSpecs[] = {
        {
            TEXT("SableQuay"), TEXT("SABLE QUAY"),
            TEXT("A PRIVATE MILITARY NETWORK HAS SEIZED THE COASTAL RELAY.\n\nMOVE TO THE CHECKPOINT, BREACH THE HOUSE, SECURE THE ARCHIVE,\nTHEN EXTRACT BEFORE THE PIER LIGHTS UP."),
            FVector(0.0f, -950.0f, 120.0f), FRotator::ZeroRotator,
            FVector(250.0f, -500.0f, 100.0f), FVector(400.0f, 0.0f, 100.0f),
            FVector(780.0f, 850.0f, 160.0f), FVector(850.0f, 80.0f, 70.0f),
            FVector(1500.0f, 0.0f, 100.0f), FVector(1500.0f, 650.0f, 110.0f),
            FVector(1600.0f, 850.0f, 85.0f), FVector(520.0f, -180.0f, 95.0f),
            EBMWeaponId::SolacePistol, 5, 8, 10,
            FColor(38, 48, 60), FColor(25, 185, 175)
        },
        {
            TEXT("Floodline"), TEXT("FLOODLINE"),
            TEXT("THE LOWER FLOODGATES ARE LIVE.\n\nMOVE THROUGH THE DRAINAGE YARD, TAKE THE BREACH CHARGE,\nAND BREAK THE SECOND RELAY BEFORE THE TIDE CUTS THE ROUTE."),
            FVector(-1500.0f, -900.0f, 120.0f), FRotator(0.0f, 8.0f, 0.0f),
            FVector(-1100.0f, -700.0f, 100.0f), FVector(-700.0f, -300.0f, 100.0f),
            FVector(-250.0f, 650.0f, 160.0f), FVector(200.0f, 350.0f, 70.0f),
            FVector(900.0f, 250.0f, 100.0f), FVector(1300.0f, -500.0f, 110.0f),
            FVector(-360.0f, 520.0f, 85.0f), FVector(-700.0f, -260.0f, 95.0f),
            EBMWeaponId::Breach12, 6, 9, 11,
            FColor(34, 55, 72), FColor(45, 160, 220)
        },
        {
            TEXT("RelayHouse"), TEXT("RELAY HOUSE"),
            TEXT("THE RELAY HOUSE IS OCCUPIED.\n\nCROSS THE ROOFTOP SERVICE LANE, SECURE THE MARKSMAN RIFLE,\nAND PUSH THE ARCHIVE ROOM BEFORE THE HOUSE GOES DARK."),
            FVector(-1050.0f, 820.0f, 120.0f), FRotator(0.0f, -72.0f, 0.0f),
            FVector(-700.0f, 600.0f, 100.0f), FVector(-100.0f, 430.0f, 100.0f),
            FVector(650.0f, 850.0f, 160.0f), FVector(1050.0f, 500.0f, 70.0f),
            FVector(1750.0f, 250.0f, 100.0f), FVector(2100.0f, -500.0f, 110.0f),
            FVector(900.0f, 700.0f, 85.0f), FVector(-100.0f, 520.0f, 95.0f),
            EBMWeaponId::RookDMR, 7, 10, 12,
            FColor(58, 48, 45), FColor(255, 140, 35)
        },
        {
            TEXT("NightglassCore"), TEXT("NIGHTGLASS CORE"),
            TEXT("THE SIGNAL HAS A HEART.\n\nTAKE THE ARC-7 FROM THE SERVICE CACHE, CUT THROUGH THE CORE,\nAND HOLD THE SOUTHERN PIER LONG ENOUGH TO ERASE THE ARCHIVE."),
            FVector(1050.0f, -900.0f, 120.0f), FRotator(0.0f, 165.0f, 0.0f),
            FVector(1300.0f, -500.0f, 100.0f), FVector(1800.0f, -100.0f, 100.0f),
            FVector(2050.0f, 700.0f, 160.0f), FVector(1250.0f, 900.0f, 70.0f),
            FVector(500.0f, 650.0f, 100.0f), FVector(-200.0f, 400.0f, 110.0f),
            FVector(1950.0f, 420.0f, 85.0f), FVector(1800.0f, -40.0f, 95.0f),
            EBMWeaponId::ArcLauncher, 8, 12, 14,
            FColor(42, 38, 70), FColor(165, 90, 230)
        },
        {
            TEXT("AshenDocks"), TEXT("ASHEN DOCKS"),
            TEXT("THE OLD DRY DOCKS ARE BURNING AGAIN.\n\nCROSS THE CINDERED GANTRIES, SECURE THE SIDEARM CACHE,\nAND CUT THE FIRE-CONTROL RELAY BEFORE THE HARBOR COLLAPSES."),
            FVector(-1850.0f, -1150.0f, 120.0f), FRotator(0.0f, 18.0f, 0.0f),
            FVector(-1450.0f, -850.0f, 100.0f), FVector(-1100.0f, -300.0f, 100.0f),
            FVector(-650.0f, 650.0f, 160.0f), FVector(-820.0f, 1040.0f, 70.0f),
            FVector(-50.0f, 920.0f, 100.0f), FVector(550.0f, 1260.0f, 110.0f),
            FVector(0.0f, 1100.0f, 85.0f), FVector(-900.0f, -80.0f, 95.0f),
            EBMWeaponId::SolacePistol, 8, 12, 14,
            FColor(62, 42, 38), FColor(220, 75, 48)
        },
        {
            TEXT("CinderWorks"), TEXT("CINDER WORKS"),
            TEXT("THE FOUNDRY IS STILL HOT AND VANTA IS USING THE FURNACES.\n\nMOVE BETWEEN THE KILNS, TAKE THE SHOTGUN,\nAND SHUT DOWN THE MAIN CASTING LINE."),
            FVector(-1750.0f, 1050.0f, 120.0f), FRotator(0.0f, -38.0f, 0.0f),
            FVector(-1400.0f, 700.0f, 100.0f), FVector(-950.0f, 250.0f, 100.0f),
            FVector(-300.0f, -500.0f, 160.0f), FVector(-600.0f, -900.0f, 70.0f),
            FVector(250.0f, -850.0f, 100.0f), FVector(900.0f, -1080.0f, 110.0f),
            FVector(420.0f, -650.0f, 85.0f), FVector(-800.0f, 80.0f, 95.0f),
            EBMWeaponId::Breach12, 9, 13, 15,
            FColor(72, 48, 30), FColor(255, 120, 35)
        },
        {
            TEXT("SaltFoundry"), TEXT("SALT FOUNDRY"),
            TEXT("THE SALT REFINERY HAS BECOME A VANTA SUPPLY DEPOT.\n\nFOLLOW THE WHITE DUST TRAIL, TAKE THE RIFLE,\nAND HOLD THE EVAPORATION BASIN UNTIL THE SHIPS TURN."),
            FVector(-1250.0f, -1000.0f, 120.0f), FRotator(0.0f, 12.0f, 0.0f),
            FVector(-900.0f, -650.0f, 100.0f), FVector(-450.0f, -150.0f, 100.0f),
            FVector(0.0f, 550.0f, 160.0f), FVector(-50.0f, 1100.0f, 70.0f),
            FVector(700.0f, 900.0f, 100.0f), FVector(1350.0f, 1150.0f, 110.0f),
            FVector(500.0f, 720.0f, 85.0f), FVector(-460.0f, -260.0f, 95.0f),
            EBMWeaponId::RookDMR, 9, 13, 15,
            FColor(42, 66, 55), FColor(110, 210, 120)
        },
        {
            TEXT("BlackwaterYard"), TEXT("BLACKWATER YARD"),
            TEXT("THE BLACKWATER PUMPS ARE MOVING SOMETHING WORSE THAN OIL.\n\nCROSS THE TANK FARM, RECOVER THE SMG,\nAND BREAK THE PUMP CONTROL NODE BEFORE IT REACHES THE SEA."),
            FVector(1000.0f, -1200.0f, 120.0f), FRotator(0.0f, 112.0f, 0.0f),
            FVector(750.0f, -800.0f, 100.0f), FVector(450.0f, -350.0f, 100.0f),
            FVector(250.0f, 250.0f, 160.0f), FVector(700.0f, 550.0f, 150.0f),
            FVector(1350.0f, 300.0f, 100.0f), FVector(1900.0f, -250.0f, 110.0f),
            FVector(1200.0f, 500.0f, 85.0f), FVector(400.0f, -420.0f, 95.0f),
            EBMWeaponId::Kestrel9, 10, 14, 16,
            FColor(24, 48, 58), FColor(45, 190, 210)
        },
        {
            TEXT("GlassMarket"), TEXT("GLASS MARKET"),
            TEXT("THE MARKET DISTRICT IS EMPTY EXCEPT FOR THE SIGNAL MIRRORS.\n\nMOVE THROUGH THE SHATTERED ARCADES, TAKE THE ARC-7,\nAND KILL THE MIRROR ARRAY BEFORE IT BLINDS THE COAST."),
            FVector(-650.0f, -1250.0f, 120.0f), FRotator(0.0f, 24.0f, 0.0f),
            FVector(-350.0f, -900.0f, 100.0f), FVector(100.0f, -650.0f, 100.0f),
            FVector(700.0f, -350.0f, 160.0f), FVector(1100.0f, -800.0f, 70.0f),
            FVector(1450.0f, -100.0f, 100.0f), FVector(2200.0f, 250.0f, 110.0f),
            FVector(1250.0f, -420.0f, 85.0f), FVector(150.0f, -550.0f, 95.0f),
            EBMWeaponId::ArcLauncher, 10, 14, 16,
            FColor(68, 50, 76), FColor(220, 90, 190)
        },
        {
            TEXT("StormVault"), TEXT("STORM VAULT"),
            TEXT("THE STORM VAULT IS LOCKED TO THE LIGHTNING GRID.\n\nRUN THE HIGH CATWALKS, TAKE THE MARKSMAN RIFLE,\nAND GROUND THE VAULT BEFORE THE NEXT STRIKE."),
            FVector(2750.0f, 1150.0f, 120.0f), FRotator(0.0f, -150.0f, 0.0f),
            FVector(2350.0f, 850.0f, 100.0f), FVector(1900.0f, 450.0f, 100.0f),
            FVector(1500.0f, -100.0f, 160.0f), FVector(2000.0f, -350.0f, 70.0f),
            FVector(1300.0f, -650.0f, 100.0f), FVector(500.0f, -300.0f, 110.0f),
            FVector(1650.0f, -300.0f, 85.0f), FVector(1850.0f, 280.0f, 95.0f),
            EBMWeaponId::RookDMR, 11, 15, 17,
            FColor(30, 48, 74), FColor(85, 145, 255)
        },
        {
            TEXT("RedTide"), TEXT("RED TIDE"),
            TEXT("THE RED TIDE IS A CHEMICAL FRONT, NOT A WEATHER WARNING.\n\nCROSS THE SEA WALL, SECURE THE SHOTGUN,\nAND HOLD THE BREAKWATER WHILE THE FILTERS RESET."),
            FVector(2500.0f, -1100.0f, 120.0f), FRotator(0.0f, 168.0f, 0.0f),
            FVector(2200.0f, -750.0f, 100.0f), FVector(1700.0f, -300.0f, 100.0f),
            FVector(1200.0f, 250.0f, 160.0f), FVector(1600.0f, 700.0f, 70.0f),
            FVector(1000.0f, 1100.0f, 100.0f), FVector(300.0f, 1200.0f, 110.0f),
            FVector(1400.0f, 580.0f, 85.0f), FVector(1750.0f, -180.0f, 95.0f),
            EBMWeaponId::Breach12, 11, 15, 17,
            FColor(70, 36, 38), FColor(240, 65, 62)
        },
        {
            TEXT("IronCathedral"), TEXT("IRON CATHEDRAL"),
            TEXT("THE IRON CATHEDRAL IS A POWER STATION BUILT LIKE A SHRINE.\n\nPASS THE TURBINE AISLES, TAKE THE RIFLE,\nAND SILENCE THE CHOIR OF RELAY DISCHARGES."),
            FVector(50.0f, 1200.0f, 120.0f), FRotator(0.0f, -65.0f, 0.0f),
            FVector(400.0f, 850.0f, 100.0f), FVector(800.0f, 500.0f, 100.0f),
            FVector(1200.0f, 50.0f, 160.0f), FVector(800.0f, -250.0f, 70.0f),
            FVector(350.0f, -700.0f, 100.0f), FVector(-300.0f, -1100.0f, 110.0f),
            FVector(1050.0f, 0.0f, 85.0f), FVector(760.0f, 360.0f, 95.0f),
            EBMWeaponId::ARX4, 12, 16, 18,
            FColor(48, 42, 52), FColor(190, 180, 235)
        },
        {
            TEXT("LanternBasin"), TEXT("LANTERN BASIN"),
            TEXT("THE BASIN LIGHTS ARE GUIDING A CONVOY THROUGH THE DARK.\n\nFOLLOW THE LANTERN CHAIN, TAKE THE SMG,\nAND TURN THE BASIN'S FALSE SIGNAL AGAINST VANTA."),
            FVector(-1900.0f, 450.0f, 120.0f), FRotator(0.0f, -8.0f, 0.0f),
            FVector(-1500.0f, 350.0f, 100.0f), FVector(-1050.0f, 50.0f, 100.0f),
            FVector(-550.0f, -400.0f, 160.0f), FVector(-100.0f, -150.0f, 70.0f),
            FVector(400.0f, -800.0f, 100.0f), FVector(950.0f, -500.0f, 110.0f),
            FVector(-420.0f, -250.0f, 85.0f), FVector(-980.0f, -120.0f, 95.0f),
            EBMWeaponId::Kestrel9, 12, 16, 18,
            FColor(64, 48, 32), FColor(255, 190, 70)
        },
        {
            TEXT("QuarryNine"), TEXT("QUARRY NINE"),
            TEXT("QUARRY NINE HAS BEEN CUT INTO A PERFECT SIGNAL CANYON.\n\nDESCEND THROUGH THE CUT, TAKE THE ARC-7,\nAND COLLAPSE THE VANTA EXCAVATOR NETWORK."),
            FVector(2600.0f, 700.0f, 120.0f), FRotator(0.0f, 145.0f, 0.0f),
            FVector(2200.0f, 500.0f, 100.0f), FVector(1750.0f, 100.0f, 100.0f),
            FVector(1250.0f, -450.0f, 160.0f), FVector(800.0f, -850.0f, 70.0f),
            FVector(300.0f, -300.0f, 100.0f), FVector(-250.0f, 300.0f, 110.0f),
            FVector(980.0f, -600.0f, 85.0f), FVector(1680.0f, -180.0f, 95.0f),
            EBMWeaponId::ArcLauncher, 13, 17, 19,
            FColor(50, 52, 55), FColor(170, 185, 195)
        },
        {
            TEXT("VantaSpire"), TEXT("VANTA SPIRE"),
            TEXT("THE SPIRE IS VANTA'S EYESIGHT OVER THE WHOLE COAST.\n\nCLIMB THE SERVICE LANE, TAKE THE DMR,\nAND BREAK THE RELAY AT THE TOP BEFORE DAWN."),
            FVector(-1500.0f, 1300.0f, 120.0f), FRotator(0.0f, -42.0f, 0.0f),
            FVector(-1100.0f, 1000.0f, 100.0f), FVector(-650.0f, 700.0f, 100.0f),
            FVector(-200.0f, 250.0f, 160.0f), FVector(250.0f, 700.0f, 70.0f),
            FVector(750.0f, 300.0f, 100.0f), FVector(1250.0f, -250.0f, 110.0f),
            FVector(0.0f, 520.0f, 85.0f), FVector(-600.0f, 520.0f, 95.0f),
            EBMWeaponId::RookDMR, 13, 17, 19,
            FColor(48, 32, 68), FColor(150, 80, 255)
        },
        {
            TEXT("Wreckline"), TEXT("WRECKLINE"),
            TEXT("THE WRECKLINE IS A MAZE OF CONTAINER HULLS AND DEAD RADAR.\n\nTHREAD THE GAPS, TAKE THE SHOTGUN,\nAND SEND THE LAST HULL'S BEACON INTO THE DEEP."),
            FVector(1900.0f, 1300.0f, 120.0f), FRotator(0.0f, 132.0f, 0.0f),
            FVector(1500.0f, 1050.0f, 100.0f), FVector(1050.0f, 700.0f, 100.0f),
            FVector(600.0f, 250.0f, 160.0f), FVector(1150.0f, -50.0f, 70.0f),
            FVector(1650.0f, -500.0f, 100.0f), FVector(2300.0f, -850.0f, 110.0f),
            FVector(980.0f, 180.0f, 85.0f), FVector(1000.0f, 520.0f, 95.0f),
            EBMWeaponId::Breach12, 14, 18, 20,
            FColor(35, 53, 60), FColor(80, 200, 180)
        },
        {
            TEXT("EmberStation"), TEXT("EMBER STATION"),
            TEXT("EMBER STATION IS THE LAST FUEL STOP BEFORE THE OPEN SEA.\n\nMOVE THROUGH THE SERVICE RACKS, TAKE THE SMG,\nAND STOP THE IGNITION SEQUENCE AT THE WESTERN TANK."),
            FVector(-2050.0f, -500.0f, 120.0f), FRotator(0.0f, 5.0f, 0.0f),
            FVector(-1700.0f, -250.0f, 100.0f), FVector(-1250.0f, 150.0f, 100.0f),
            FVector(-800.0f, 650.0f, 160.0f), FVector(-300.0f, 250.0f, 70.0f),
            FVector(-100.0f, 950.0f, 100.0f), FVector(500.0f, 1250.0f, 110.0f),
            FVector(-650.0f, 480.0f, 85.0f), FVector(-1200.0f, -100.0f, 95.0f),
            EBMWeaponId::Kestrel9, 14, 18, 20,
            FColor(68, 42, 26), FColor(255, 100, 30)
        },
        {
            TEXT("PaleHarbor"), TEXT("PALE HARBOR"),
            TEXT("PALE HARBOR HAS LOST POWER, BUT THE CRANES KEEP MOVING.\n\nCROSS THE QUIET BERTHS, TAKE THE ARC-7,\nAND CUT THE AUTOMATED HARBOR MASTER OFF THE GRID."),
            FVector(-1950.0f, 1100.0f, 120.0f), FRotator(0.0f, -35.0f, 0.0f),
            FVector(-1600.0f, 800.0f, 100.0f), FVector(-1200.0f, 400.0f, 100.0f),
            FVector(-700.0f, -50.0f, 160.0f), FVector(-100.0f, -500.0f, 70.0f),
            FVector(500.0f, -250.0f, 100.0f), FVector(1050.0f, 350.0f, 110.0f),
            FVector(-400.0f, -300.0f, 85.0f), FVector(-1150.0f, 240.0f, 95.0f),
            EBMWeaponId::ArcLauncher, 15, 19, 21,
            FColor(44, 64, 72), FColor(120, 210, 225)
        },
        {
            TEXT("MeridianGate"), TEXT("MERIDIAN GATE"),
            TEXT("THE MERIDIAN GATE IS THE ONLY ROUTE INTO THE INNER DISTRICT.\n\nBREAK THE OUTER CHECKPOINTS, TAKE THE RIFLE,\nAND KEEP THE GATE OPEN FOR THE LATTICE CONVOY."),
            FVector(1250.0f, 1250.0f, 120.0f), FRotator(0.0f, 138.0f, 0.0f),
            FVector(950.0f, 900.0f, 100.0f), FVector(550.0f, 550.0f, 100.0f),
            FVector(100.0f, 150.0f, 160.0f), FVector(-300.0f, 700.0f, 70.0f),
            FVector(-750.0f, 300.0f, 100.0f), FVector(-1250.0f, -250.0f, 110.0f),
            FVector(0.0f, 350.0f, 85.0f), FVector(600.0f, 350.0f, 95.0f),
            EBMWeaponId::RookDMR, 15, 19, 21,
            FColor(42, 48, 62), FColor(120, 160, 255)
        },
        {
            TEXT("LastLight"), TEXT("LAST LIGHT"),
            TEXT("THIS IS THE LAST LIGHT STILL ANSWERING THE SIGNAL.\n\nCROSS THE EASTERN RIDGE, TAKE THE SIDEARM CACHE,\nAND HOLD THE WESTERN PIER UNTIL THE MERIDIAN GOES DARK."),
            FVector(3000.0f, -250.0f, 120.0f), FRotator(0.0f, 180.0f, 0.0f),
            FVector(2600.0f, 0.0f, 100.0f), FVector(2200.0f, 400.0f, 100.0f),
            FVector(1700.0f, 800.0f, 160.0f), FVector(1200.0f, 450.0f, 70.0f),
            FVector(650.0f, 800.0f, 100.0f), FVector(-50.0f, 1150.0f, 110.0f),
            FVector(1500.0f, 650.0f, 85.0f), FVector(2100.0f, 250.0f, 95.0f),
            EBMWeaponId::SolacePistol, 16, 20, 22,
            FColor(78, 78, 62), FColor(255, 220, 95)
        }
    };
}

FBMCampaignLevelDefinition GetCampaignLevelDefinition(const EBMCampaignLevel Level)
{
    const int32 Index = FMath::Clamp(
        static_cast<int32>(Level),
        0,
        UE_ARRAY_COUNT(CampaignLevelSpecs) - 1);
    return MakeLevel(static_cast<EBMCampaignLevel>(Index), CampaignLevelSpecs[Index]);
}

FString GetBMCampaignLevelListText(const EBMCampaignLevel SelectedLevel)
{
    const int32 SelectedIndex = FMath::Clamp(static_cast<int32>(SelectedLevel), 0, GetBMCampaignLevelCount() - 1);
    FString Result;
    for (int32 Index = 0; Index < GetBMCampaignLevelCount(); ++Index)
    {
        if (Index > 0)
        {
            Result += Index % 5 == 0 ? TEXT("\n") : TEXT("     ");
        }

        const FBMCampaignLevelDefinition Definition = GetCampaignLevelDefinition(static_cast<EBMCampaignLevel>(Index));
        const FString Prefix = Index == SelectedIndex ? TEXT("[") : TEXT(" ");
        const FString Suffix = Index == SelectedIndex ? TEXT("]") : TEXT(" ");
        Result += FString::Printf(
            TEXT("%s%02d %-16s%s"),
            *Prefix,
            Index + 1,
            *Definition.LevelId.ToString().ToUpper(),
            *Suffix);
    }
    return Result;
}
