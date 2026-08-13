// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMGreyboxDirector.h"

#include "BMArtDirectedProp.h"
#include "BMAudioDataAsset.h"
#include "BMCampaignLevel.h"
#include "BMEnemyCharacter.h"
#include "BMCombatTuning.h"
#include "BMFriendlySquadMember.h"
#include "BMInteractableActor.h"
#include "BMGameState.h"
#include "BMWeapon.h"
#include "BMSquadSubsystem.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DirectionalLight.h"
#include "Engine/CollisionProfile.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/PointLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Parse.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavMesh/RecastNavMesh.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "PhysicsEngine/BodySetup.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr const TCHAR* ProjectEnvironmentBlockPath = TEXT("/Game/Environment/Meshes/SM_BM_Env_Block.SM_BM_Env_Block");
    constexpr const TCHAR* ProjectEnvironmentPipePath = TEXT("/Game/Environment/Meshes/SM_BM_Env_Pipe.SM_BM_Env_Pipe");
    constexpr const TCHAR* ProjectEnvironmentOrbPath = TEXT("/Game/Environment/Meshes/SM_BM_Env_Orb.SM_BM_Env_Orb");
    constexpr const TCHAR* ProjectEnvironmentDeckPath = TEXT("/Game/Environment/Meshes/SM_BM_Env_Deck.SM_BM_Env_Deck");
    constexpr const TCHAR* ProjectEnvironmentTrussPath = TEXT("/Game/Environment/Meshes/SM_BM_Env_Truss.SM_BM_Env_Truss");
    constexpr const TCHAR* ProjectEnvironmentGratePath = TEXT("/Game/Environment/Meshes/SM_BM_Env_Grate.SM_BM_Env_Grate");
    constexpr const TCHAR* ProjectEnvironmentPanelPath = TEXT("/Game/Environment/Meshes/SM_BM_Env_Panel.SM_BM_Env_Panel");
    constexpr const TCHAR* ProjectEnvironmentCoverPath = TEXT("/Game/Environment/Meshes/SM_BM_Env_Cover.SM_BM_Env_Cover");
    constexpr const TCHAR* ProjectEnvironmentDoorPath = TEXT("/Game/Environment/Meshes/SM_BM_Env_Door.SM_BM_Env_Door");
    constexpr const TCHAR* ProjectEnvironmentWindowPath = TEXT("/Game/Environment/Meshes/SM_BM_Env_Window.SM_BM_Env_Window");
    constexpr const TCHAR* ProjectEnvironmentSignPath = TEXT("/Game/Environment/Meshes/SM_BM_Env_Sign.SM_BM_Env_Sign");
    constexpr const TCHAR* ProjectEnvironmentMachineryPath = TEXT("/Game/Environment/Meshes/SM_BM_Env_Machinery.SM_BM_Env_Machinery");
    constexpr const TCHAR* ProjectEnvironmentWreckagePath = TEXT("/Game/Environment/Meshes/SM_BM_Env_Wreckage.SM_BM_Env_Wreckage");

    UMaterialInterface* LoadPresentationMaterial()
    {
        if (UMaterialInterface* IndustrialMaterial = LoadObject<UMaterialInterface>(
                nullptr,
                TEXT("/Game/Environment/Materials/MI_BM_IndustrialSurface.MI_BM_IndustrialSurface")))
        {
            return IndustrialMaterial;
        }
        if (UMaterialInterface* BasicShapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
        {
            return BasicShapeMaterial;
        }
        return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
    }

    const TCHAR* ResolveEnvironmentMeshPath(const TCHAR* RequestedPath)
    {
        if (!RequestedPath)
        {
            return RequestedPath;
        }

        const FString Requested(RequestedPath);
        if (Requested.Equals(TEXT("/Engine/BasicShapes/Cube.Cube"), ESearchCase::IgnoreCase))
        {
            return ProjectEnvironmentBlockPath;
        }
        if (Requested.Equals(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), ESearchCase::IgnoreCase))
        {
            return ProjectEnvironmentPipePath;
        }
        if (Requested.Equals(TEXT("/Engine/BasicShapes/Sphere.Sphere"), ESearchCase::IgnoreCase))
        {
            return ProjectEnvironmentOrbPath;
        }
        return RequestedPath;
    }
}

ABMGreyboxDirector::ABMGreyboxDirector()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    SetNetUpdateFrequency(100.0f);
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    FloorCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("FloorCollision"));
    // Make the guaranteed runtime floor the actor root.  This keeps the
    // collision primitive registered independently of the generated mesh
    // actors and gives CharacterMovement a stable world surface immediately
    // after the director is spawned.
    RootComponent = FloorCollision;
    FloorCollision->SetRelativeLocation(FVector(650.0f, 0.0f, -55.0f));
    FloorCollision->SetBoxExtent(FVector(2750.0f, 1600.0f, 50.0f));
    FloorCollision->SetMobility(EComponentMobility::Movable);
    FloorCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    FloorCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    FloorCollision->SetCollisionObjectType(ECC_WorldStatic);
    FloorCollision->SetCollisionResponseToAllChannels(ECR_Block);
    FloorCollision->SetGenerateOverlapEvents(false);
    // The runtime floor is the guaranteed walkable surface beneath every
    // generated layout. Keep it in Recast's geometry so a packaged world
    // still has a valid nav seed even when a level's modular floor meshes are
    // movable and are registered after map load.
    FloorCollision->SetCanEverAffectNavigation(true);
    FloorCollision->SetHiddenInGame(true);

    // Weather is intentionally a local visual field rather than replicated
    // gameplay state. Every world instance builds the same deterministic
    // transforms from the replicated campaign level, keeping network cost
    // low while making rain/snow/mist visible to every player.
    WeatherParticleSystem = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WeatherParticleSystem"));
    WeatherParticleSystem->SetupAttachment(RootComponent);
    WeatherParticleSystem->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WeatherParticleSystem->SetGenerateOverlapEvents(false);
    WeatherParticleSystem->SetCanEverAffectNavigation(false);
    WeatherParticleSystem->SetCastShadow(false);
    WeatherParticleSystem->SetVisibility(false, true);

    EnvironmentNiagaraAccent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("EnvironmentNiagaraAccent"));
    EnvironmentNiagaraAccent->SetupAttachment(RootComponent);
    EnvironmentNiagaraAccent->SetAutoActivate(false);
    EnvironmentNiagaraAccent->SetVisibility(false, true);

    EnvironmentAmbientAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EnvironmentAmbientAudio"));
    EnvironmentAmbientAudio->SetupAttachment(RootComponent);
    EnvironmentAmbientAudio->bAutoActivate = false;

    // Keep the arena fail-safe: walking, sprinting, or jumping into an outer
    // edge must stop at the perimeter instead of turning into a mission loss.
    // These colliders are deliberately taller than the bounded jump arc and
    // overlap the floor so neither the authored shell nor runtime dressing can
    // leave an open corner.
    const auto ConfigureBoundary = [this](
        UBoxComponent* Boundary,
        const FVector& RelativeLocation,
        const FVector& Extent)
    {
        if (!Boundary)
        {
            return;
        }
        Boundary->SetupAttachment(RootComponent);
        Boundary->SetRelativeLocation(RelativeLocation);
        Boundary->SetBoxExtent(Extent);
        Boundary->SetMobility(EComponentMobility::Movable);
        Boundary->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Boundary->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
        Boundary->SetCollisionObjectType(ECC_WorldStatic);
        Boundary->SetCollisionResponseToAllChannels(ECR_Block);
        Boundary->SetGenerateOverlapEvents(false);
        Boundary->SetCanEverAffectNavigation(false);
        Boundary->SetHiddenInGame(true);
    };

    NorthBoundaryCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("NorthBoundaryCollision"));
    SouthBoundaryCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SouthBoundaryCollision"));
    EastBoundaryCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("EastBoundaryCollision"));
    WestBoundaryCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("WestBoundaryCollision"));

    constexpr float BoundaryWallHalfHeight = 600.0f;
    ConfigureBoundary(
        NorthBoundaryCollision,
        FVector(0.0f, 1650.0f, BoundaryWallHalfHeight),
        FVector(2800.0f, 50.0f, BoundaryWallHalfHeight));
    ConfigureBoundary(
        SouthBoundaryCollision,
        FVector(0.0f, -1650.0f, BoundaryWallHalfHeight),
        FVector(2800.0f, 50.0f, BoundaryWallHalfHeight));
    ConfigureBoundary(
        EastBoundaryCollision,
        FVector(2800.0f, 0.0f, BoundaryWallHalfHeight),
        FVector(50.0f, 1650.0f, BoundaryWallHalfHeight));
    ConfigureBoundary(
        WestBoundaryCollision,
        FVector(-2800.0f, 0.0f, BoundaryWallHalfHeight),
        FVector(50.0f, 1650.0f, BoundaryWallHalfHeight));
}

bool ABMGreyboxDirector::IsRuntimeFloorReady() const
{
    return FloorCollision && FloorCollision->IsRegistered() && FloorCollision->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
}

FVector ABMGreyboxDirector::GetExtractionLocation() const
{
    return GetCampaignLevelDefinition(CampaignLevel).ExtractionLocation;
}

void ABMGreyboxDirector::SetCampaignLevel(const EBMCampaignLevel Level)
{
    if (CampaignLevel == Level)
    {
        return;
    }

    CampaignLevel = Level;
    if (HasAuthority() && HasActorBegunPlay())
    {
        RefreshCampaignLevelPresentation();
        for (ABMInteractableActor* Interaction : MissionInteractables)
        {
            if (IsValid(Interaction))
            {
                Interaction->Destroy();
            }
        }
        MissionInteractables.Reset();
        SpawnMissionInteractables();
        RefreshObjectivePresentation();
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian campaign: greybox layout=%s"),
        *GetCampaignLevelDefinition(CampaignLevel).LevelId.ToString());
}

void ABMGreyboxDirector::RefreshCampaignLevelPresentation()
{
    if (!GetWorld())
    {
        return;
    }

    for (AStaticMeshActor* Piece : CampaignVariantPieces)
    {
        if (IsValid(Piece))
        {
            Piece->Destroy();
        }
    }
    CampaignVariantPieces.Reset();

    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
    const ABMGameState* State = GetWorld()->GetGameState<ABMGameState>();
    const bool bCampaignShellMode = bUseAuthoredLevelShell && (!State || !State->bSignalControlMode);
    if (bCampaignShellMode)
    {
        HideAuthoredCampaignShell();
        ClearCampaignLayout();
        SpawnCampaignLayout();
    }
    else if (bUseAuthoredLevelShell)
    {
        ApplyAuthoredShellPresentation();
    }
    if (!State || !State->bSignalControlMode)
    {
        SpawnCampaignVariantDressing();
    }

    // The compatibility map owns these props at runtime rather than in the
    // saved shell. Recreate them when the selector changes so the cover,
    // relay, and landmark actors follow the selected route as well.
    if (HasAuthority() && (!State || !State->bSignalControlMode))
    {
        for (ABMArtDirectedProp* Prop : ArtDirectedProps)
        {
            if (IsValid(Prop))
            {
                Prop->Destroy();
            }
        }
        ArtDirectedProps.Reset();
        BreachRelayProp = nullptr;
        SpawnAuthoredProps();
    }

    if (IsValid(BreachBarrier))
    {
        BreachBarrier->SetActorLocation(Level.BreachLocation + FVector(0.0f, 0.0f, -45.0f));
    }
    else if (!GetWorld()->GetGameState<ABMGameState>() || !GetWorld()->GetGameState<ABMGameState>()->bSignalControlMode)
    {
        BreachBarrier = SpawnCube(
            Level.BreachLocation + FVector(0.0f, 0.0f, -45.0f),
            FVector(1.8f, 0.35f, 2.4f),
            Level.EnvironmentPrimaryColor,
            true);
    }

    if (IsValid(BreachRelayProp))
    {
        BreachRelayProp->SetActorLocation(FVector(Level.BreachLocation.X, Level.BreachLocation.Y + 120.0f, 0.0f));
    }

    const FName ObjectiveIds[] = {
        TEXT("Insertion"),
        TEXT("QuietApproach"),
        TEXT("InteriorBreach"),
        TEXT("IntelConsole"),
        TEXT("OpenCombat"),
        TEXT("Extraction")};
    const FVector ObjectiveLocations[] = {
        FVector(Level.InsertionLocation.X, Level.InsertionLocation.Y, 0.0f),
        FVector(Level.CheckpointLocation.X, Level.CheckpointLocation.Y, 0.0f),
        FVector(Level.BreachLocation.X, Level.BreachLocation.Y, 0.0f),
        FVector(Level.IntelLocation.X, Level.IntelLocation.Y, 0.0f),
        FVector(Level.OpenCombatLocation.X, Level.OpenCombatLocation.Y, 0.0f),
        FVector(Level.ExtractionLocation.X, Level.ExtractionLocation.Y, 0.0f)};
    const FVector BeaconOffsets[] = {
        FVector(0.0f, 0.0f, 5.0f),
        FVector(0.0f, 0.0f, 105.0f),
        FVector(0.0f, 0.0f, 220.0f),
        FVector(0.0f, 0.0f, 270.0f),
        FVector(0.0f, 0.0f, 420.0f),
        FVector(0.0f, 0.0f, 350.0f)};

    for (int32 ObjectiveIndex = 0; ObjectiveIndex < UE_ARRAY_COUNT(ObjectiveIds); ++ObjectiveIndex)
    {
        if (TArray<TObjectPtr<AStaticMeshActor>>* Pieces = ObjectiveBeaconPieces.Find(ObjectiveIds[ObjectiveIndex]))
        {
            for (int32 PieceIndex = 0; PieceIndex < Pieces->Num() && PieceIndex < UE_ARRAY_COUNT(BeaconOffsets); ++PieceIndex)
            {
                if (IsValid((*Pieces)[PieceIndex]))
                {
                    (*Pieces)[PieceIndex]->SetActorLocation(ObjectiveLocations[ObjectiveIndex] + BeaconOffsets[PieceIndex]);
                }
            }
        }
        if (TObjectPtr<APointLight>* BeaconLight = ObjectiveBeaconLights.Find(ObjectiveIds[ObjectiveIndex]))
        {
            if (IsValid(BeaconLight->Get()))
            {
                BeaconLight->Get()->SetActorLocation(ObjectiveLocations[ObjectiveIndex] + FVector(0.0f, 0.0f, 105.0f));
            }
        }
    }

    // Theme presentation is local and must refresh on listen clients after
    // ABMGameState replicates the menu-selected campaign level.
    ApplyEnvironmentTheme();
    RefreshObjectivePresentation();
}

void ABMGreyboxDirector::BeginPlay()
{
    Super::BeginPlay();
    const FString MapName = GetWorld() ? GetWorld()->GetMapName() : FString();
    bUseAuthoredLevelShell = MapName.Contains(TEXT("Glasswake")) || MapName.Contains(TEXT("SignalControl"));
    BuildRuntimeEnvironment();
    SpawnLighting();

    if (ABMGameState* State = GetWorld()->GetGameState<ABMGameState>())
    {
        State->OnGameStateUpdated.AddDynamic(this, &ABMGreyboxDirector::HandleGameStateUpdated);
    }
    RefreshObjectivePresentation();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian runtime: floor collision registered=%s location=%s extent=%s"),
        FloorCollision && FloorCollision->IsRegistered() ? TEXT("yes") : TEXT("no"),
        FloorCollision ? *FloorCollision->GetComponentLocation().ToString() : TEXT("none"),
        FloorCollision ? *FloorCollision->GetScaledBoxExtent().ToString() : TEXT("none"));
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian runtime: perimeter collision north=%s south=%s east=%s west=%s action=block_fall"),
        NorthBoundaryCollision && NorthBoundaryCollision->IsRegistered() ? TEXT("ready") : TEXT("missing"),
        SouthBoundaryCollision && SouthBoundaryCollision->IsRegistered() ? TEXT("ready") : TEXT("missing"),
        EastBoundaryCollision && EastBoundaryCollision->IsRegistered() ? TEXT("ready") : TEXT("missing"),
        WestBoundaryCollision && WestBoundaryCollision->IsRegistered() ? TEXT("ready") : TEXT("missing"));

    if (HasAuthority() && bSpawnMissionActors)
    {
        // The role probe measures every hostile's locomotion and facing in
        // isolation.  Friendly support is intentionally present in the
        // rendered showcase, but its nearest-target fire can eliminate one
        // of the five diagnostic pawns before that pawn completes its
        // movement window.  Keep the normal playable encounter unchanged and
        // remove only that unrelated source of probe nondeterminism.
        const bool bCombatRoleProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe"));
        if (!bCombatRoleProbe)
        {
            SpawnFriendlySquad();
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Black Meridian combat role probe: friendly support suppressed for isolated hostile movement validation"));
        }
        SpawnMissionInteractables();
    }

    // Campaign collision is generated after the map is loaded. Request the
    // dynamic Recast pass only after those floors, walls, ramps, and cover
    // pieces exist, then let Tick hold combat waves until the data is usable.
    BootstrapRuntimeNavigation();

    UE_LOG(LogTemp, Display, TEXT("Black Meridian runtime: greybox ready (pieces=%d, mission_interactables=%d, mission_actors=%s)"), GreyboxPieces.Num(), MissionInteractables.Num(), bSpawnMissionActors ? TEXT("yes") : TEXT("no"));
}

void ABMGreyboxDirector::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    TickThemeWeather(DeltaSeconds);

    if (!bRuntimeNavigationReady)
    {
        int32 ActiveTiles = 0;
        if (IsRuntimeNavigationReady(&ActiveTiles))
        {
            bRuntimeNavigationReady = true;
            RuntimeNavigationWaitLogTimer = 0.0f;
            const double BuildElapsedMilliseconds = RuntimeNavigationRequestPlatformTime > 0.0
                ? (FPlatformTime::Seconds() - RuntimeNavigationRequestPlatformTime) * 1000.0
                : -1.0;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian navigation: ready level=%s active_tiles=%d runtime_generation=dynamic build_elapsed_ms=%.2f"),
                *GetCampaignLevelDefinition(CampaignLevel).LevelId.ToString(),
                ActiveTiles,
                BuildElapsedMilliseconds);

            if (bPendingCombatWave)
            {
                const int32 WaveCount = PendingCombatWaveCount;
                const FVector WaveCenter = PendingCombatWaveCenter;
                bPendingCombatWave = false;
                PendingCombatWaveCount = 0;
                PendingCombatWaveCenter = FVector::ZeroVector;
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian navigation: releasing queued combat wave count=%d center=%s"),
                    WaveCount,
                    *WaveCenter.ToString());
                SpawnCombatWave(WaveCount, WaveCenter);
            }
        }
        else
        {
            RuntimeNavigationWaitLogTimer += FMath::Max(0.0f, DeltaSeconds);
            if (RuntimeNavigationWaitLogTimer >= 2.0f)
            {
                RuntimeNavigationWaitLogTimer = 0.0f;
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian navigation: waiting for dynamic Recast level=%s"),
                    *GetCampaignLevelDefinition(CampaignLevel).LevelId.ToString());
            }
        }
    }
}

void ABMGreyboxDirector::BootstrapRuntimeNavigation()
{
    if (bRuntimeNavigationBuildRequested || !GetWorld())
    {
        return;
    }

    bRuntimeNavigationBuildRequested = true;
    RuntimeNavigationRequestPlatformTime = FPlatformTime::Seconds();
    SetActorTickEnabled(true);
    if (UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetNavigationSystem(GetWorld()))
    {
        // The campaign shell is replaced during BeginPlay, so the saved map
        // has no NavMeshBoundsVolume to describe the generated battlefield.
        // Ask NavigationSystem to derive its finite build region from the
        // nav-relevant runtime layout pieces. Without this, Build() succeeds
        // but correctly produces zero tiles, leaving the AI in its
        // compatibility movement path forever. The generated layout is the
        // only campaign geometry plus the guaranteed runtime floor are
        // marked nav-relevant; the hidden shell, perimeter guard, and weather
        // remain excluded.
        NavigationSystem->bWholeWorldNavigable = true;

        NavigationSystem->Build();
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian navigation: dynamic build requested level=%s collision_source=runtime_campaign_layout bounds=nav_relevant_runtime_layout"),
            *GetCampaignLevelDefinition(CampaignLevel).LevelId.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Black Meridian navigation: system unavailable; AI will use compatibility movement"));
    }
}

bool ABMGreyboxDirector::IsRuntimeNavigationReady(int32* OutActiveTiles) const
{
    if (OutActiveTiles)
    {
        *OutActiveTiles = 0;
    }
    if (!GetWorld())
    {
        return false;
    }

    UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetNavigationSystem(GetWorld());
    ARecastNavMesh* RecastNavMesh = NavigationSystem
        ? Cast<ARecastNavMesh>(NavigationSystem->GetDefaultNavDataInstance())
        : nullptr;
    if (!RecastNavMesh || !RecastNavMesh->IsRegistered())
    {
        return false;
    }

    const int32 ActiveTiles = RecastNavMesh->GetNumActiveTiles();
    if (OutActiveTiles)
    {
        *OutActiveTiles = ActiveTiles;
    }
    return ActiveTiles > 0
        && !UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(GetWorld());
}

void ABMGreyboxDirector::BuildRuntimeEnvironment()
{
    const FString MapName = GetWorld() ? GetWorld()->GetMapName() : FString();
    // The runtime scene has no baked Lightmass data. Keep the palette dark
    // and directional so the cyan/amber signal language reads as authored
    // world design rather than a collection of bright debug cubes.
    // These byte values are deliberately kept in the source as an art
    // director's palette. SpawnMesh converts them to linear material values
    // explicitly, so mid-tone industrial surfaces remain readable on the
    // runtime-lit Mac compatibility path instead of collapsing to black.
    const ABMGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
    const bool bSignalControlMode = GameState && GameState->bSignalControlMode;
    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
    const auto BlendPalette = [](const FColor& Base, const FColor& Theme, const float Alpha)
    {
        return FColor(
            static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(static_cast<float>(Base.R), static_cast<float>(Theme.R), Alpha)), 0, 255)),
            static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(static_cast<float>(Base.G), static_cast<float>(Theme.G), Alpha)), 0, 255)),
            static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(static_cast<float>(Base.B), static_cast<float>(Theme.B), Alpha)), 0, 255)),
            Base.A);
    };
    // Keep the palette restrained so objective colors remain readable. The
    // campaign collision shell is replaced below by SpawnCampaignLayout();
    // these values only establish the selected operation's material language.
    const FColor FloorColor = BlendPalette(FColor(38, 48, 60), Level.EnvironmentGroundColor, 0.56f);
    const FColor StructureColor = BlendPalette(FColor(60, 72, 84), Level.EnvironmentPrimaryColor, 0.32f);
    const FColor WallColor = BlendPalette(FColor(86, 96, 104), Level.EnvironmentPrimaryColor, 0.28f);
    const FColor CoverColor = BlendPalette(FColor(118, 120, 112), Level.EnvironmentPrimaryColor, 0.24f);
    const FColor TrimColor = BlendPalette(FColor(16, 88, 96), Level.EnvironmentAccentColor, 0.68f);
    const FColor AmberTrim = BlendPalette(FColor(180, 95, 20), Level.EnvironmentAccentColor, 0.62f);
    const FColor RedTrim = BlendPalette(FColor(170, 34, 38), Level.EnvironmentAccentColor, 0.42f);
    const FColor PanelColor = BlendPalette(FColor(72, 84, 92), Level.EnvironmentPrimaryColor, 0.34f);
    const FColor SafetyYellow = BlendPalette(FColor(194, 142, 32), Level.EnvironmentAccentColor, 0.38f);
    const FColor BlackTrim(20, 26, 30);

    if (bUseAuthoredLevelShell)
    {
        if (bSignalControlMode)
        {
            // Signal Control keeps its dedicated authored arena shell.
            ApplyAuthoredShellPresentation();
            SpawnAuthoredShellDressing();
        }
        else
        {
            // Campaign operations replace the shared Glasswake shell with
            // the selected operation's actual floor, walls, rooms, elevation,
            // and traversal pattern. Weather and grading are only one layer;
            // this is the structural layer players physically move through.
            HideAuthoredCampaignShell();
            SpawnCampaignLayout();
            BreachBarrier = SpawnCube(
                Level.BreachLocation + FVector(0.0f, 0.0f, -45.0f),
                FVector(1.8f, 0.35f, 2.4f),
                FColor(86, 96, 104),
                true);
            SpawnCampaignVariantDressing();
            SpawnAuthoredProps();
            RunCollisionProbe();
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian authored level shell: map=%s mission_support=%s campaign_shell_replaced=%s layout=%s"),
            *MapName,
            bSignalControlMode ? TEXT("no") : TEXT("yes"),
            bSignalControlMode ? TEXT("no") : TEXT("yes"),
            *Level.EnvironmentLayoutId.ToString());
        return;
    }

    // Cover the complete signal-control play space, including the sixteen
    // perimeter starts at x=-1200 and x=2800.
    SpawnMesh(ProjectEnvironmentDeckPath, FVector(650.0f, 0.0f, -55.0f), FRotator::ZeroRotator, FVector(55.0f, 16.0f, 1.0f), FloorColor);
    SpawnCube(FVector(-2100.0f, 0.0f, 245.0f), FVector(0.5f, 16.0f, 5.0f), StructureColor);
    SpawnCube(FVector(3400.0f, 0.0f, 245.0f), FVector(0.5f, 16.0f, 5.0f), StructureColor);
    SpawnCube(FVector(650.0f, -1550.0f, 245.0f), FVector(28.0f, 0.5f, 5.0f), StructureColor);
    SpawnCube(FVector(650.0f, 1550.0f, 245.0f), FVector(28.0f, 0.5f, 5.0f), StructureColor);

    // Low cover is deliberately aligned into readable lanes: every route to
    // a node has a safe first piece, a risky crossfire gap, and a second piece
    // that can be used to break sightlines.
    SpawnCube(FVector(100.0f, -620.0f, 45.0f), FVector(2.4f, 0.65f, 0.9f), CoverColor, true);
    SpawnCube(FVector(520.0f, -180.0f, 45.0f), FVector(1.2f, 0.65f, 0.9f), CoverColor, true);
    SpawnCube(FVector(960.0f, 360.0f, 45.0f), FVector(2.0f, 0.65f, 0.9f), CoverColor, true);
    SpawnCube(FVector(1420.0f, -260.0f, 45.0f), FVector(1.5f, 0.65f, 0.9f), CoverColor, true);
    SpawnCube(FVector(1740.0f, 540.0f, 45.0f), FVector(1.0f, 1.7f, 0.9f), CoverColor, true);
    SpawnCube(FVector(2200.0f, -520.0f, 45.0f), FVector(2.6f, 0.65f, 0.9f), CoverColor, true);

    // Hard cover and mission landmarks are grounded to the floor plane. The
    // previous greybox left several walls floating above the collision floor,
    // which made the scene read as a test layout rather than a place.
    SpawnCube(FVector(-700.0f, 540.0f, 60.0f), FVector(3.5f, 0.7f, 1.3f), WallColor, true);
    SpawnCube(FVector(-740.0f, -260.0f, 105.0f), FVector(0.55f, 2.8f, 2.2f), StructureColor);
    // Leave a real opening for the breach door. The previous single wall
    // occupied the same location as the RelayBreach interactable, so the
    // visibility trace hit the wall before it could ever reach the door.
    // These two collision-bearing sections preserve the relay-house silhouette
    // while allowing the player to approach, hold E, and pass through once
    // the door rotates open.
    SpawnCube(FVector(590.0f, 880.0f, 75.0f), FVector(0.7f, 0.45f, 1.6f), WallColor);
    SpawnCube(FVector(970.0f, 880.0f, 75.0f), FVector(0.7f, 0.45f, 1.6f), WallColor);
    SpawnCube(FVector(2100.0f, 760.0f, 75.0f), FVector(0.45f, 3.0f, 1.6f), WallColor);
    BreachBarrier = SpawnCube(FVector(1900.0f, 650.0f, 115.0f), FVector(1.8f, 0.35f, 2.4f), WallColor, true);

    if (bSignalControlMode)
    {
        // These lane markings and staging pads explain the competitive arena;
        // keeping them out of the solo route stops the two modes from reading
        // like the same unexplained collection of coloured platforms.
        for (const float X : {-1200.0f, -400.0f, 400.0f, 1200.0f, 2000.0f, 2800.0f})
        {
            SpawnCube(FVector(X, 0.0f, 2.0f), FVector(0.025f, 15.0f, 0.025f), TrimColor);
        }
        for (const float Y : {-1100.0f, -550.0f, 0.0f, 550.0f, 1100.0f})
        {
            SpawnCube(FVector(650.0f, Y, 2.5f), FVector(27.0f, 0.018f, 0.025f), StructureColor);
        }
        SpawnCube(FVector(250.0f, 650.0f, 4.0f), FVector(2.0f, 2.0f, 0.08f), TrimColor);
        SpawnCube(FVector(1600.0f, 850.0f, 4.0f), FVector(2.0f, 2.0f, 0.08f), AmberTrim);
    }

    // Repeating gantries establish scale and break the empty black horizon.
    // They also create long-range silhouettes and intermittent overhead cover.
    for (const float X : {-900.0f, 650.0f, 2200.0f})
    {
        SpawnCube(FVector(X, -1080.0f, 245.0f), FVector(0.55f, 0.55f, 5.0f), StructureColor);
        SpawnCube(FVector(X, 1080.0f, 245.0f), FVector(0.55f, 0.55f, 5.0f), StructureColor);
        SpawnCube(FVector(X, 0.0f, 500.0f), FVector(11.0f, 0.35f, 0.35f), StructureColor);
        SpawnCube(FVector(X, 0.0f, 468.0f), FVector(9.5f, 0.06f, 0.035f), TrimColor);
    }

    // A compact relay house at the eastern route gives the breach interaction
    // a recognizable destination and a stronger combat silhouette.
    SpawnCube(FVector(1750.0f, 820.0f, 24.0f), FVector(5.5f, 3.0f, 0.5f), StructureColor);
    SpawnCube(FVector(1750.0f, 820.0f, 175.0f), FVector(5.5f, 0.18f, 1.35f), WallColor);
    // The pier/boat sits just beyond the south wall. Split that wall around
    // a real opening so the yellow extraction route is traversable instead
    // of looking like an objective hidden behind collision.
    SpawnCube(FVector(1300.0f, 540.0f, 175.0f), FVector(1.0f, 0.18f, 1.35f), WallColor);
    SpawnCube(FVector(2050.0f, 540.0f, 175.0f), FVector(2.5f, 0.18f, 1.35f), WallColor);
    SpawnCube(FVector(1250.0f, 820.0f, 125.0f), FVector(0.18f, 2.6f, 1.2f), WallColor);
    SpawnCube(FVector(2250.0f, 820.0f, 125.0f), FVector(0.18f, 2.6f, 1.2f), WallColor);
    SpawnCube(FVector(1750.0f, 548.0f, 260.0f), FVector(3.8f, 0.04f, 0.08f), AmberTrim);

    // A small industrial kit gives the relay house and the node route a
    // recognizable authored language: round service pipes, antenna masts,
    // spherical warning lamps, and hard-edged modular trim. These are still
    // engine primitives so the source-only project remains portable, but the
    // scene no longer depends on a single cube silhouette for every prop.
    const auto AddServicePipe = [this](const FVector& Location, const FRotator& Rotation, const FVector& Scale, const FColor& Color)
    {
        SpawnMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), Location, Rotation, Scale, Color);
    };
    const auto AddWarningLamp = [this, &StructureColor](const FVector& Location, const FColor& Color)
    {
        SpawnMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"), Location, FRotator::ZeroRotator, FVector(0.32f), Color);
        SpawnCube(Location + FVector(0.0f, 0.0f, -24.0f), FVector(0.42f, 0.42f, 0.06f), StructureColor);
    };
    AddServicePipe(FVector(1750.0f, 820.0f, 390.0f), FRotator::ZeroRotator, FVector(0.16f, 0.16f, 2.0f), StructureColor);
    AddServicePipe(FVector(1750.0f, 820.0f, 540.0f), FRotator(0.0f, 0.0f, 90.0f), FVector(0.12f, 0.12f, 2.8f), TrimColor);
    AddServicePipe(FVector(1500.0f, 820.0f, 118.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.12f, 0.12f, 2.5f), StructureColor);
    AddServicePipe(FVector(2000.0f, 820.0f, 118.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.12f, 0.12f, 2.5f), StructureColor);
    AddWarningLamp(FVector(1750.0f, 548.0f, 290.0f), AmberTrim);
    AddWarningLamp(FVector(1750.0f, 820.0f, 560.0f), RedTrim);

    // A non-blocking dressing pass gives the compatibility map an authored
    // industrial rhythm without changing the tested movement lanes. The
    // collision-bearing floor, walls, cover, and objective actors above stay
    // authoritative; these braces, panels, rails, and cable trays are only
    // the visual layer that makes those spaces read as a real facility.
    const auto AddDecorMesh = [this](
        const TCHAR* MeshPath,
        const FVector& Location,
        const FRotator& Rotation,
        const FVector& Scale,
        const FColor& Color) -> AStaticMeshActor*
    {
        AStaticMeshActor* Piece = SpawnMesh(MeshPath, Location, Rotation, Scale, Color, false);
        if (Piece)
        {
            SetRuntimePieceCollision(Piece, false);
            if (UStaticMeshComponent* MeshComponent = Piece->GetStaticMeshComponent())
            {
                MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
        }
        return Piece;
    };
    const auto AddDecorCube = [&AddDecorMesh](
        const FVector& Location,
        const FRotator& Rotation,
        const FVector& Scale,
        const FColor& Color) -> AStaticMeshActor*
    {
        return AddDecorMesh(TEXT("/Engine/BasicShapes/Cube.Cube"), Location, Rotation, Scale, Color);
    };

    // Seawall service bays establish a horizon and give the arena a repeated
    // scale reference. The slim cyan uprights act as navigation landmarks
    // from the four competitive starts without becoming objective markers.
    for (const float X : {-1800.0f, -1200.0f, -600.0f, 0.0f, 600.0f, 1200.0f, 1800.0f, 2400.0f, 3000.0f})
    {
        AddDecorCube(FVector(X, -1320.0f, 100.0f), FRotator::ZeroRotator, FVector(3.2f, 0.22f, 1.0f), PanelColor);
        AddDecorCube(FVector(X, 1320.0f, 100.0f), FRotator::ZeroRotator, FVector(3.2f, 0.22f, 1.0f), PanelColor);
        AddDecorCube(FVector(X, -1302.0f, 224.0f), FRotator::ZeroRotator, FVector(0.11f, 0.10f, 0.92f), TrimColor);
        AddDecorCube(FVector(X, 1302.0f, 224.0f), FRotator::ZeroRotator, FVector(0.11f, 0.10f, 0.92f), TrimColor);
    }

    // Low catwalks, handrails, and overhead cable trays frame the long lanes.
    // They are deliberately offset from player eye height so they add depth
    // and silhouette without stealing sightlines through the combat routes.
    for (const float X : {-900.0f, 650.0f, 2200.0f})
    {
        AddDecorCube(FVector(X, -820.0f, 330.0f), FRotator::ZeroRotator, FVector(9.0f, 0.12f, 0.12f), StructureColor);
        AddDecorCube(FVector(X, 820.0f, 330.0f), FRotator::ZeroRotator, FVector(9.0f, 0.12f, 0.12f), StructureColor);
        AddDecorCube(FVector(X, -820.0f, 418.0f), FRotator::ZeroRotator, FVector(9.0f, 0.06f, 0.06f), TrimColor);
        AddDecorCube(FVector(X, 820.0f, 418.0f), FRotator::ZeroRotator, FVector(9.0f, 0.06f, 0.06f), TrimColor);
        AddDecorCube(FVector(X, 0.0f, 545.0f), FRotator::ZeroRotator, FVector(10.5f, 0.18f, 0.12f), PanelColor);
        AddDecorCube(FVector(X, 0.0f, 574.0f), FRotator::ZeroRotator, FVector(9.4f, 0.05f, 0.05f), TrimColor);
    }

    // Door portals and safety plates turn the two mission interaction points
    // into recognizable destinations even before final meshes are supplied.
    const auto AddDoorPortal = [&AddDecorCube, &BlackTrim](const FVector& Center, const FColor& Color)
    {
        AddDecorCube(Center + FVector(-190.0f, 0.0f, 170.0f), FRotator::ZeroRotator, FVector(0.12f, 0.14f, 1.70f), Color);
        AddDecorCube(Center + FVector(190.0f, 0.0f, 170.0f), FRotator::ZeroRotator, FVector(0.12f, 0.14f, 1.70f), Color);
        AddDecorCube(Center + FVector(0.0f, 0.0f, 338.0f), FRotator::ZeroRotator, FVector(1.98f, 0.14f, 0.12f), Color);
        AddDecorCube(Center + FVector(0.0f, -10.0f, 86.0f), FRotator::ZeroRotator, FVector(1.58f, 0.04f, 0.06f), BlackTrim);
    };
    AddDoorPortal(FVector(780.0f, 880.0f, 0.0f), AmberTrim);
    AddDoorPortal(FVector(1500.0f, 548.0f, 0.0f), SafetyYellow);

    // The relay house receives an elevated maintenance deck and vertical
    // antenna silhouette. Both are visual-only in this source-only pass; the
    // existing collision shell remains the gameplay boundary.
    AddDecorCube(FVector(1750.0f, 820.0f, 320.0f), FRotator::ZeroRotator, FVector(5.0f, 1.7f, 0.12f), PanelColor);
    AddDecorCube(FVector(1750.0f, 670.0f, 420.0f), FRotator::ZeroRotator, FVector(5.0f, 0.07f, 0.92f), TrimColor);
    AddDecorCube(FVector(1750.0f, 970.0f, 420.0f), FRotator::ZeroRotator, FVector(5.0f, 0.07f, 0.92f), TrimColor);
    AddDecorMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(1750.0f, 820.0f, 620.0f), FRotator::ZeroRotator, FVector(0.10f, 0.10f, 2.8f), StructureColor);
    AddDecorMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"), FVector(1750.0f, 820.0f, 900.0f), FRotator::ZeroRotator, FVector(0.20f), AmberTrim);

    // Alternating hazard plates provide a human-readable safety language at
    // the relay and along the breach approach without adding HUD clutter.
    for (int32 Index = 0; Index < 9; ++Index)
    {
        const FColor StripeColor = Index % 2 == 0 ? SafetyYellow : BlackTrim;
        AddDecorCube(
            FVector(1300.0f + (Index * 100.0f), 531.0f, 92.0f),
            FRotator(0.0f, 0.0f, -24.0f),
            FVector(0.38f, 0.045f, 0.07f),
            StripeColor);
    }

    // Cylindrical low cover breaks up the repeated rectangular barricades and
    // creates a readable alternate route through the central node.
    SpawnMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(1180.0f, 360.0f, 78.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.72f, 0.72f, 2.4f), CoverColor, true);
    SpawnMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(1300.0f, 360.0f, 78.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.72f, 0.72f, 2.4f), CoverColor, true);

    if (bSignalControlMode)
    {
        // Node pads, masts, and crossbars belong to Signal Control only. A
        // solo player should not see three competing objectives at spawn.
        const auto AddNodeInfrastructure = [this, &StructureColor](const FVector& Location, const FColor& Color)
        {
            SpawnCube(Location + FVector(0.0f, 0.0f, 8.0f), FVector(4.8f, 4.8f, 0.12f), StructureColor);
            SpawnCube(Location + FVector(0.0f, 0.0f, 28.0f), FVector(2.1f, 2.1f, 0.12f), Color);
            SpawnMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), Location + FVector(0.0f, 0.0f, 128.0f), FRotator::ZeroRotator, FVector(0.16f, 0.16f, 2.0f), StructureColor);
            SpawnCube(Location + FVector(0.0f, 0.0f, 246.0f), FVector(1.2f, 0.16f, 0.12f), Color);
            SpawnCube(Location + FVector(0.0f, 0.0f, 246.0f), FVector(0.16f, 1.2f, 0.12f), Color);
            SpawnMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"), Location + FVector(0.0f, 0.0f, 255.0f), FRotator::ZeroRotator, FVector(0.28f), Color);
        };
        AddNodeInfrastructure(FVector(250.0f, 650.0f, 0.0f), FColor(25, 185, 175));
        AddNodeInfrastructure(FVector(1500.0f, -500.0f, 0.0f), FColor(245, 170, 45));
        AddNodeInfrastructure(FVector(2600.0f, 650.0f, 0.0f), FColor(220, 55, 45));
    }

    // Reusable authored-style props add a recognizable human scale to the
    // route. They are server-spawned and replicated so the destructible relay
    // has one authoritative state instead of a client-only visual trick.
    if (!bSignalControlMode)
    {
        SpawnAuthoredProps();
    }
    else if (HasAuthority())
    {
        // Competitive routes also need human-scale silhouettes. Keep these
        // pieces outside the node pads and spawn lanes so the mode gains
        // readable cover without changing its tested start/rotation geometry.
        const FLinearColor CompetitiveMetal(0.24f, 0.30f, 0.34f, 1.0f);
        const FLinearColor CompetitiveTimber(0.32f, 0.23f, 0.14f, 1.0f);
        const FLinearColor CompetitiveCyan(0.04f, 0.62f, 0.64f, 1.0f);
        const FLinearColor CompetitiveAmber(0.82f, 0.40f, 0.08f, 1.0f);
        SpawnArtDirectedProp(EBMArtDirectedPropVariant::Crate, FVector(-620.0f, -610.0f, 0.0f), FRotator(0.0f, -18.0f, 0.0f), FVector(1.0f), CompetitiveTimber, CompetitiveAmber);
        SpawnArtDirectedProp(EBMArtDirectedPropVariant::Barricade, FVector(820.0f, 230.0f, 0.0f), FRotator(0.0f, 16.0f, 0.0f), FVector(1.0f), CompetitiveTimber, CompetitiveAmber, true);
        SpawnArtDirectedProp(EBMArtDirectedPropVariant::PipeRack, FVector(1980.0f, -240.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.92f), CompetitiveMetal, CompetitiveCyan);
        SpawnArtDirectedProp(EBMArtDirectedPropVariant::Crate, FVector(2240.0f, 920.0f, 0.0f), FRotator(0.0f, 10.0f, 0.0f), FVector(0.82f), CompetitiveTimber, CompetitiveCyan);
        SpawnArtDirectedProp(EBMArtDirectedPropVariant::SignalTower, FVector(-1050.0f, 1120.0f, 0.0f), FRotator::ZeroRotator, FVector(0.86f), CompetitiveMetal, CompetitiveCyan);
        SpawnArtDirectedProp(EBMArtDirectedPropVariant::SignalTower, FVector(2520.0f, -1120.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f), FVector(0.86f), CompetitiveMetal, CompetitiveAmber);
    }

    // High-contrast beacons make the greybox readable without a minimap. The
    // HUD calls out the active one; the other colors preview the mission route.
    const auto AddObjectiveBeacon = [this, bSignalControlMode](const FName ObjectiveId, const FVector& Location, const FColor& Color)
    {
        if (bSignalControlMode)
        {
            return;
        }

        TArray<TObjectPtr<AStaticMeshActor>>& Pieces = ObjectiveBeaconPieces.FindOrAdd(ObjectiveId);
        const auto AddBeaconPiece = [this, &Pieces](AStaticMeshActor* Piece)
        {
            if (!Piece)
            {
                return;
            }
            // Beacons are navigation language, not physical cover. Disable
            // the component query explicitly as well as the actor flag so a
            // marker cannot steal an interaction trace from a door, console,
            // or boat placed at the same objective.
            SetRuntimePieceCollision(Piece, false);
            if (UStaticMeshComponent* MeshComponent = Piece->GetStaticMeshComponent())
            {
                MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
            Piece->SetActorHiddenInGame(true);
            Pieces.Add(Piece);
        };
        // The active landmark must read from across the greybox. A wider pad,
        // tall mast, and high signal bars make it a destination rather than
        // another small coloured cube in the skyline.
        AddBeaconPiece(SpawnCube(Location + FVector(0.0f, 0.0f, 5.0f), FVector(3.0f, 3.0f, 0.08f), Color));
        AddBeaconPiece(SpawnCube(Location + FVector(0.0f, 0.0f, 105.0f), FVector(0.22f, 0.22f, 1.05f), Color));
        AddBeaconPiece(SpawnCube(Location + FVector(0.0f, 0.0f, 220.0f), FVector(0.62f, 0.62f, 0.12f), Color));
        AddBeaconPiece(SpawnCube(Location + FVector(0.0f, 0.0f, 270.0f), FVector(0.06f, 0.06f, 0.72f), Color));
        AddBeaconPiece(SpawnCube(Location + FVector(0.0f, 0.0f, 420.0f), FVector(0.10f, 0.10f, 1.05f), Color));
        AddBeaconPiece(SpawnMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"), Location + FVector(0.0f, 0.0f, 350.0f), FRotator::ZeroRotator, FVector(0.42f), Color));

        FActorSpawnParameters LightSpawnParameters;
        LightSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (APointLight* BeaconLight = GetWorld()->SpawnActor<APointLight>(
                APointLight::StaticClass(),
                Location + FVector(0.0f, 0.0f, 105.0f),
                FRotator::ZeroRotator,
                LightSpawnParameters))
        {
            BeaconLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
            // Keep future landmarks visible but quiet. The mission state
            // promotes only the current objective to a strong navigational
            // light, so the route reads without six equally loud beacons.
            BeaconLight->SetActorHiddenInGame(true);
            BeaconLight->GetLightComponent()->SetIntensity(480.0f);
            BeaconLight->GetLightComponent()->SetLightColor(FLinearColor(Color));
            BeaconLight->GetLightComponent()->SetCastShadows(false);
            if (ULocalLightComponent* LocalLight = Cast<ULocalLightComponent>(BeaconLight->GetLightComponent()))
            {
                LocalLight->SetAttenuationRadius(300.0f);
            }
            ObjectiveBeaconLights.Add(ObjectiveId, BeaconLight);
        }
    };
    AddObjectiveBeacon(TEXT("Insertion"), FVector(Level.InsertionLocation.X, Level.InsertionLocation.Y, 0.0f), FColor(35, 245, 225));
    AddObjectiveBeacon(TEXT("QuietApproach"), FVector(Level.CheckpointLocation.X, Level.CheckpointLocation.Y, 0.0f), FColor(255, 205, 55));
    AddObjectiveBeacon(TEXT("InteriorBreach"), FVector(Level.BreachLocation.X, Level.BreachLocation.Y, 0.0f), FColor(255, 145, 35));
    AddObjectiveBeacon(TEXT("IntelConsole"), FVector(Level.IntelLocation.X, Level.IntelLocation.Y, 0.0f), FColor(40, 235, 215));
    AddObjectiveBeacon(TEXT("OpenCombat"), FVector(Level.OpenCombatLocation.X, Level.OpenCombatLocation.Y, 0.0f), FColor(245, 70, 55));
    AddObjectiveBeacon(TEXT("Extraction"), FVector(Level.ExtractionLocation.X, Level.ExtractionLocation.Y, 0.0f), FColor(255, 225, 85));

    if (!bSignalControlMode)
    {
        SpawnCampaignVariantDressing();
    }
}

void ABMGreyboxDirector::HideAuthoredCampaignShell()
{
    if (!GetWorld() || bCampaignShellHidden)
    {
        return;
    }

    int32 HiddenActors = 0;
    // Glasswake is a replaceable shell. At this point BeginPlay has not
    // spawned runtime AStaticMeshActor pieces yet, so suppressing the saved
    // static meshes is reliable even when cooked actor labels are stripped.
    // The invisible director floor remains below each operation as a fail-safe.
    for (TActorIterator<AStaticMeshActor> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        AStaticMeshActor* Actor = *Iterator;
        if (!Actor || Actor->GetOwner() == this)
        {
            continue;
        }

        Actor->SetActorHiddenInGame(true);
        SetRuntimePieceCollision(Actor, false);
        if (UStaticMeshComponent* MeshComponent = Actor->GetStaticMeshComponent())
        {
            MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            MeshComponent->SetCanEverAffectNavigation(false);
        }
        ++HiddenActors;
    }

    bCampaignShellHidden = true;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian campaign: shared shell suppressed actors=%d reason=per_level_structural_layout"),
        HiddenActors);
}

void ABMGreyboxDirector::ClearCampaignLayout()
{
    for (AStaticMeshActor* Piece : CampaignLayoutPieces)
    {
        if (IsValid(Piece))
        {
            Piece->Destroy();
        }
    }
    CampaignLayoutPieces.Reset();
}

void ABMGreyboxDirector::SpawnCampaignLayout()
{
    if (!GetWorld())
    {
        return;
    }

    const ABMGameState* State = GetWorld()->GetGameState<ABMGameState>();
    if (State && State->bSignalControlMode)
    {
        return;
    }

    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
    const FColor GroundColor = Level.EnvironmentGroundColor;
    const FColor PrimaryColor = Level.EnvironmentPrimaryColor;
    const FColor AccentColor = Level.EnvironmentAccentColor;
    const FColor WeatherColor = Level.EnvironmentWeatherColor;
    const FName LayoutTag = FName(FString::Printf(TEXT("Environment.Layout.%s"), *Level.EnvironmentLayoutId.ToString()));
    int32 WreckageCount = 0;

    const auto AddLayoutMesh = [this, LayoutTag](
        const TCHAR* MeshPath,
        const FVector& Location,
        const FRotator& Rotation,
        const FVector& Scale,
        const FColor& Color) -> AStaticMeshActor*
    {
        AStaticMeshActor* Piece = SpawnMesh(MeshPath, Location, Rotation, Scale, Color, false);
        if (!Piece)
        {
            return nullptr;
        }

        // These are actual gameplay structures, unlike the old variant pass.
        Piece->Tags.AddUnique(TEXT("ArtDirected.CampaignLayout"));
        Piece->Tags.AddUnique(LayoutTag);
        Piece->SetActorEnableCollision(true);
        if (UStaticMeshComponent* MeshComponent = Piece->GetStaticMeshComponent())
        {
            MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
            MeshComponent->SetCanEverAffectNavigation(true);

            // The environment kit shares one authored material so it remains
            // cheap and portable on the Mac compatibility renderer. Give the
            // individual mesh families their own physical response here so a
            // wet deck, a painted panel, a steel pipe, and a timber-like cover
            // do not all read as the same matte block under the runtime key.
            if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(MeshComponent->GetMaterial(0)))
            {
                const FString MeshIdentity(MeshPath ? MeshPath : TEXT(""));
                float Metallic = 0.28f;
                float Roughness = 0.72f;
                if (MeshIdentity.Contains(TEXT("Deck"), ESearchCase::IgnoreCase))
                {
                    Metallic = 0.52f;
                    Roughness = 0.58f;
                    Piece->Tags.AddUnique(TEXT("ArtDirected.Surface.Deck"));
                }
                else if (MeshIdentity.Contains(TEXT("Pipe"), ESearchCase::IgnoreCase)
                    || MeshIdentity.Contains(TEXT("Truss"), ESearchCase::IgnoreCase)
                    || MeshIdentity.Contains(TEXT("Grate"), ESearchCase::IgnoreCase))
                {
                    Metallic = 0.68f;
                    Roughness = 0.44f;
                    Piece->Tags.AddUnique(TEXT("ArtDirected.Surface.Metal"));
                }
                else if (MeshIdentity.Contains(TEXT("Cover"), ESearchCase::IgnoreCase)
                    || MeshIdentity.Contains(TEXT("Wreckage"), ESearchCase::IgnoreCase))
                {
                    Metallic = 0.16f;
                    Roughness = 0.84f;
                    Piece->Tags.AddUnique(TEXT("ArtDirected.Surface.Cover"));
                }
                else if (MeshIdentity.Contains(TEXT("Panel"), ESearchCase::IgnoreCase)
                    || MeshIdentity.Contains(TEXT("Door"), ESearchCase::IgnoreCase)
                    || MeshIdentity.Contains(TEXT("Window"), ESearchCase::IgnoreCase)
                    || MeshIdentity.Contains(TEXT("Sign"), ESearchCase::IgnoreCase)
                    || MeshIdentity.Contains(TEXT("Machinery"), ESearchCase::IgnoreCase))
                {
                    Metallic = 0.46f;
                    Roughness = 0.56f;
                    Piece->Tags.AddUnique(TEXT("ArtDirected.Surface.Facade"));
                }
                else
                {
                    Piece->Tags.AddUnique(TEXT("ArtDirected.Surface.Structure"));
                }

                Material->SetScalarParameterValue(TEXT("Metallic"), Metallic);
                Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
            }
        }
        // The layout lambda reasserts gameplay collision after SpawnMesh has
        // initialized the presentation component. Re-apply the canonical
        // visibility/movement split here so these structures do not restore
        // the coarse proxy as a visibility blocker.
        SetRuntimePieceCollision(Piece, true);
        CampaignLayoutPieces.Add(Piece);
        return Piece;
    };

    const auto AddFloor = [&AddLayoutMesh](
        const FVector& Center,
        const FVector& FullSize,
        const FColor& Color,
        const float Z,
        const float Yaw)
        -> AStaticMeshActor*
    {
        return AddLayoutMesh(
            ProjectEnvironmentDeckPath,
            FVector(Center.X, Center.Y, Z),
            FRotator(0.0f, Yaw, 0.0f),
            FVector(
                FMath::Max(FullSize.X / 100.0f, 1.0f),
                FMath::Max(FullSize.Y / 100.0f, 1.0f),
                FMath::Max(FullSize.Z / 100.0f, 0.08f)),
            Color);
    };

    const auto AddSegmentFloor = [&AddFloor](
        const FVector& Start,
        const FVector& End,
        const float Width,
        const FColor& Color,
        const float Z)
    {
        const FVector Delta = End - Start;
        const float Length = FMath::Max(Delta.Size2D(), 200.0f);
        const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
        AddFloor((Start + End) * 0.5f, FVector(Length, Width, 100.0f), Color, Z, Yaw);
    };

    // A raised deck is only gameplay geometry when the player can actually
    // reach it. Build overlapping, collision-bearing steps so elevated
    // platforms have predictable walkable connections instead of becoming
    // decorative slabs that trap the pawn on the fail-safe floor.
    const auto AddStairs = [&AddFloor](
        const FVector& Start,
        const FVector& End,
        const float Width,
        const float StartSurfaceZ,
        const float EndSurfaceZ,
        const FColor& Color)
    {
        const FVector Delta = End - Start;
        const float Length = FMath::Max(Delta.Size2D(), 200.0f);
        const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
        const float Rise = EndSurfaceZ - StartSurfaceZ;
        const int32 StepCount = FMath::Clamp(FMath::CeilToInt(FMath::Abs(Rise) / 38.0f), 2, 14);
        const float Run = Length / static_cast<float>(StepCount);
        for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
        {
            const float Alpha = (static_cast<float>(StepIndex) + 0.5f) / static_cast<float>(StepCount);
            const FVector StepPoint = FMath::Lerp(Start, End, Alpha);
            const float SurfaceZ = FMath::Lerp(StartSurfaceZ, EndSurfaceZ, Alpha);
            AddFloor(
                FVector(StepPoint.X, StepPoint.Y, 0.0f),
                FVector(Run + 80.0f, Width, 100.0f),
                Color,
                SurfaceZ - 50.0f,
                Yaw);
        }
    };

    const auto AddWall = [&AddLayoutMesh](
        const FVector& Center,
        const FVector& FullSize,
        const FColor& Color,
        const float Yaw,
        const bool bThinCover)
    {
        AStaticMeshActor* Piece = AddLayoutMesh(
            ProjectEnvironmentBlockPath,
            Center,
            FRotator(0.0f, Yaw, 0.0f),
            FVector(
                FMath::Max(FullSize.X / 100.0f, 0.08f),
                FMath::Max(FullSize.Y / 100.0f, 0.08f),
                FMath::Max(FullSize.Z / 100.0f, 0.08f)),
            Color);
        if (Piece && bThinCover)
        {
            Piece->Tags.AddUnique(TEXT("CampaignLayout.ThinCover"));
        }
        return Piece;
    };

    const auto AddSegmentWall = [&AddWall](
        const FVector& Start,
        const FVector& End,
        const float Width,
        const float Height,
        const FColor& Color,
        const float BaseZ,
        const bool bThinCover)
    {
        const FVector Delta = End - Start;
        const float Length = FMath::Max(Delta.Size2D(), 200.0f);
        const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
        AddWall(
            (Start + End) * 0.5f + FVector(0.0f, 0.0f, BaseZ + Height * 0.5f),
            FVector(Length, Width, Height),
            Color,
            Yaw,
            bThinCover);
    };

    const auto AddCover = [&AddLayoutMesh](
        const FVector& Center,
        const FVector& FullSize,
        const FColor& Color,
        const float Yaw)
    {
        AStaticMeshActor* Piece = AddLayoutMesh(
            ProjectEnvironmentCoverPath,
            Center,
            FRotator(0.0f, Yaw, 0.0f),
            FVector(
                FMath::Max(FullSize.X / 100.0f, 0.08f),
                FMath::Max(FullSize.Y / 100.0f, 0.08f),
                FMath::Max(FullSize.Z / 100.0f, 0.08f)),
            Color);
        if (Piece)
        {
            Piece->Tags.AddUnique(TEXT("CampaignLayout.ThinCover"));
        }
        return Piece;
    };

    const auto AddPanel = [&AddLayoutMesh](
        const FVector& Center,
        const FVector& Scale,
        const FColor& Color,
        const float Yaw)
    {
        return AddLayoutMesh(
            ProjectEnvironmentPanelPath,
            Center,
            FRotator(0.0f, Yaw, 0.0f),
            Scale,
            Color);
    };

    const auto AddPipe = [&AddLayoutMesh](
        const FVector& Center,
        const FVector& Scale,
        const FColor& Color,
        const float Yaw)
    {
        return AddLayoutMesh(
            ProjectEnvironmentPipePath,
            Center,
            FRotator(0.0f, Yaw, 0.0f),
            Scale,
            Color);
    };

    const auto AddTruss = [&AddLayoutMesh](
        const FVector& Center,
        const FVector& Scale,
        const FColor& Color,
        const float Yaw)
    {
        return AddLayoutMesh(
            ProjectEnvironmentTrussPath,
            Center,
            FRotator(0.0f, Yaw, 0.0f),
            Scale,
            Color);
    };

    const auto AddGrate = [&AddLayoutMesh](
        const FVector& Center,
        const FVector& Scale,
        const FColor& Color,
        const float Yaw)
    {
        return AddLayoutMesh(
            ProjectEnvironmentGratePath,
            Center,
            FRotator(0.0f, Yaw, 0.0f),
            Scale,
            Color);
    };

    const auto AddDoor = [&AddLayoutMesh](
        const FVector& Center,
        const FVector& Scale,
        const FColor& Color,
        const float Yaw)
    {
        return AddLayoutMesh(
            ProjectEnvironmentDoorPath,
            Center,
            FRotator(0.0f, Yaw, 0.0f),
            Scale,
            Color);
    };

    const auto AddWindow = [&AddLayoutMesh](
        const FVector& Center,
        const FVector& Scale,
        const FColor& Color,
        const float Yaw)
    {
        return AddLayoutMesh(
            ProjectEnvironmentWindowPath,
            Center,
            FRotator(0.0f, Yaw, 0.0f),
            Scale,
            Color);
    };

    const auto AddMachinery = [&AddLayoutMesh](
        const FVector& Center,
        const FVector& Scale,
        const FColor& Color,
        const float Yaw)
    {
        return AddLayoutMesh(
            ProjectEnvironmentMachineryPath,
            Center,
            FRotator(0.0f, Yaw, 0.0f),
            Scale,
            Color);
    };

    const auto AddSign = [&AddLayoutMesh](
        const FVector& Center,
        const FVector& Scale,
        const FColor& Color,
        const float Yaw)
    {
        return AddLayoutMesh(
            ProjectEnvironmentSignPath,
            Center,
            FRotator(0.0f, Yaw, 0.0f),
            Scale,
            Color);
    };

    const auto AddWreckage = [&AddLayoutMesh, &WreckageCount](
        const FVector& Center,
        const FVector& Scale,
        const FColor& Color,
        const float Yaw)
    {
        AStaticMeshActor* Piece = AddLayoutMesh(
            ProjectEnvironmentWreckagePath,
            Center,
            FRotator(0.0f, Yaw, 0.0f),
            Scale,
            Color);
        if (Piece)
        {
            Piece->Tags.AddUnique(TEXT("CampaignLayout.StructuralWreckage"));
            ++WreckageCount;
        }
        return Piece;
    };

    // A route-only floor is enough for collision validation, but it leaves
    // open operations reading like floating platforms when the camera looks
    // past the next encounter. Give outdoor maps a real, grounded terrain bed
    // underneath the authored route. It uses the block kit rather than the
    // gridded deck so the ground reads as continuous earth/concrete while the
    // route decks remain visually distinct.
    const auto AddTerrainBed = [&AddLayoutMesh](
        const FVector& Center,
        const FVector& FullSize,
        const FColor& Color,
        const float Z,
        const float Yaw)
    {
        AStaticMeshActor* Piece = AddLayoutMesh(
            ProjectEnvironmentBlockPath,
            FVector(Center.X, Center.Y, Z),
            FRotator(0.0f, Yaw, 0.0f),
            FVector(
                FMath::Max(FullSize.X / 92.0f, 1.0f),
                FMath::Max(FullSize.Y / 92.0f, 1.0f),
                FMath::Max(FullSize.Z / 92.0f, 0.08f)),
            Color);
        if (Piece)
        {
            Piece->Tags.AddUnique(TEXT("CampaignLayout.TerrainBed"));
            Piece->Tags.AddUnique(TEXT("CampaignLayout.GroundedTerrain"));
        }
        return Piece;
    };

    // The recovery colliders remain intentionally invisible: they are the
    // last-resort safety net that stops a pawn falling out of the generated
    // battlefield.  Give the player a readable edge before that safety net is
    // reached.  These rails and shoreline caps are visual-only, sit just
    // inside the protected perimeter, and are rebuilt with every campaign
    // layout so an empty horizon cannot be mistaken for missing terrain.
    int32 BoundaryLandmarkCount = 0;
    const auto AddBoundaryLandmark = [this, &AddLayoutMesh, &BoundaryLandmarkCount](
        const TCHAR* MeshPath,
        const FVector& Location,
        const FRotator& Rotation,
        const FVector& Scale,
        const FColor& Color)
    {
        AStaticMeshActor* Piece = AddLayoutMesh(MeshPath, Location, Rotation, Scale, Color);
        if (!Piece)
        {
            return;
        }

        Piece->Tags.AddUnique(TEXT("CampaignLayout.BoundaryLandmark"));
        Piece->Tags.AddUnique(TEXT("CampaignLayout.VisualRecoveryEdge"));
        SetRuntimePieceCollision(Piece, false);
        if (UStaticMeshComponent* MeshComponent = Piece->GetStaticMeshComponent())
        {
            MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            MeshComponent->SetCanEverAffectNavigation(false);
        }
        ++BoundaryLandmarkCount;
    };

    constexpr float BoundaryMinX = -2040.0f;
    constexpr float BoundaryMaxX = 3340.0f;
    constexpr float BoundaryMinY = -1540.0f;
    constexpr float BoundaryMaxY = 1540.0f;
    const FColor BoundaryMetal = FColor(
        FMath::Clamp(Level.EnvironmentPrimaryColor.R + 18, 0, 255),
        FMath::Clamp(Level.EnvironmentPrimaryColor.G + 18, 0, 255),
        FMath::Clamp(Level.EnvironmentPrimaryColor.B + 18, 0, 255));
    const FColor BoundarySignal = Level.EnvironmentAccentColor;

    for (int32 PostIndex = 0; PostIndex < 9; ++PostIndex)
    {
        const float Alpha = static_cast<float>(PostIndex) / 8.0f;
        const float X = FMath::Lerp(BoundaryMinX, BoundaryMaxX, Alpha);
        AddBoundaryLandmark(
            ProjectEnvironmentPipePath,
            FVector(X, BoundaryMinY, 165.0f),
            FRotator::ZeroRotator,
            FVector(0.16f, 0.16f, 1.75f),
            BoundaryMetal);
        AddBoundaryLandmark(
            ProjectEnvironmentPipePath,
            FVector(X, BoundaryMaxY, 165.0f),
            FRotator::ZeroRotator,
            FVector(0.16f, 0.16f, 1.75f),
            BoundaryMetal);
    }
    for (int32 PostIndex = 1; PostIndex < 5; ++PostIndex)
    {
        const float Alpha = static_cast<float>(PostIndex) / 5.0f;
        const float Y = FMath::Lerp(BoundaryMinY, BoundaryMaxY, Alpha);
        AddBoundaryLandmark(
            ProjectEnvironmentPipePath,
            FVector(BoundaryMinX, Y, 165.0f),
            FRotator::ZeroRotator,
            FVector(0.16f, 0.16f, 1.75f),
            BoundaryMetal);
        AddBoundaryLandmark(
            ProjectEnvironmentPipePath,
            FVector(BoundaryMaxX, Y, 165.0f),
            FRotator::ZeroRotator,
            FVector(0.16f, 0.16f, 1.75f),
            BoundaryMetal);
    }

    AddBoundaryLandmark(
        ProjectEnvironmentTrussPath,
        FVector((BoundaryMinX + BoundaryMaxX) * 0.5f, BoundaryMinY, 120.0f),
        FRotator::ZeroRotator,
        FVector(28.0f, 0.10f, 0.10f),
        BoundarySignal);
    AddBoundaryLandmark(
        ProjectEnvironmentTrussPath,
        FVector((BoundaryMinX + BoundaryMaxX) * 0.5f, BoundaryMaxY, 120.0f),
        FRotator::ZeroRotator,
        FVector(28.0f, 0.10f, 0.10f),
        BoundarySignal);
    AddBoundaryLandmark(
        ProjectEnvironmentTrussPath,
        FVector(BoundaryMinX, (BoundaryMinY + BoundaryMaxY) * 0.5f, 120.0f),
        FRotator(0.0f, 90.0f, 0.0f),
        FVector(16.0f, 0.10f, 0.10f),
        BoundarySignal);
    AddBoundaryLandmark(
        ProjectEnvironmentTrussPath,
        FVector(BoundaryMaxX, (BoundaryMinY + BoundaryMaxY) * 0.5f, 120.0f),
        FRotator(0.0f, 90.0f, 0.0f),
        FVector(16.0f, 0.10f, 0.10f),
        BoundarySignal);

    // Coastal/ice operations get a low edge cap as well as the common rail;
    // the cap gives the shoreline or berth a physical silhouette in snow,
    // fog, and rain without changing the recovery collision contract.
    const bool bShorelineLevel = Level.Level == EBMCampaignLevel::SableQuay
        || Level.Level == EBMCampaignLevel::AshenDocks
        || Level.Level == EBMCampaignLevel::RedTide
        || Level.Level == EBMCampaignLevel::PaleHarbor
        || Level.Level == EBMCampaignLevel::LastLight;
    if (bShorelineLevel)
    {
        AddBoundaryLandmark(
            ProjectEnvironmentDeckPath,
            FVector((BoundaryMinX + BoundaryMaxX) * 0.5f, BoundaryMaxY - 34.0f, 8.0f),
            FRotator::ZeroRotator,
            FVector(28.0f, 0.42f, 0.06f),
            Level.EnvironmentGroundColor);
        AddBoundaryLandmark(
            ProjectEnvironmentPanelPath,
            FVector(BoundaryMaxX - 70.0f, BoundaryMaxY - 74.0f, 170.0f),
            FRotator(0.0f, 90.0f, 0.0f),
            FVector(0.72f, 0.12f, 1.45f),
            Level.EnvironmentPrimaryColor);
    }

    const auto AddRoom = [&AddFloor, &AddWall](
        const FVector& Center,
        const float Width,
        const float Depth,
        const float Height,
        const FColor& Color,
        const bool bRoof,
        const uint8 OpeningMask,
        const float OpeningWidth,
        const float OpeningCenterX,
        const float OpeningCenterY,
        const float NorthOpeningWidth,
        const float NorthOpeningCenterX)
    {
        // OpeningMask bits: south=1, east=2, north=4, west=8. Splitting the
        // wall mesh leaves an actual doorway for the route rather than a
        // visual door in front of an impenetrable collision box.
        const auto AddWallWithOpening = [&AddWall](
            const FVector& WallCenter,
            const FVector& FullSize,
            const FColor& WallColor,
            const bool bOpen,
            const bool bHorizontalWall,
            const float RequestedOpeningCenter,
            const float RequestedOpeningWidth)
        {
            if (!bOpen)
            {
                AddWall(WallCenter, FullSize, WallColor, 0.0f, false);
                return;
            }

            const float Span = bHorizontalWall ? FullSize.X : FullSize.Y;
            const float SafeOpeningWidth = FMath::Clamp(
                RequestedOpeningWidth,
                160.0f,
                FMath::Max(160.0f, Span - 80.0f));
            const float OpeningCenter = FMath::Clamp(
                RequestedOpeningCenter,
                -Span * 0.5f + SafeOpeningWidth * 0.5f,
                Span * 0.5f - SafeOpeningWidth * 0.5f);
            const float NegativeSegmentLength = FMath::Max(
                0.0f,
                OpeningCenter - SafeOpeningWidth * 0.5f + Span * 0.5f);
            const float PositiveSegmentLength = FMath::Max(
                0.0f,
                Span * 0.5f - (OpeningCenter + SafeOpeningWidth * 0.5f));
            const FVector Axis = bHorizontalWall ? FVector::ForwardVector : FVector::RightVector;
            if (NegativeSegmentLength > 20.0f)
            {
                const float SegmentCenter = -Span * 0.5f + NegativeSegmentLength * 0.5f;
                AddWall(
                    WallCenter + Axis * SegmentCenter,
                    FVector(
                        bHorizontalWall ? NegativeSegmentLength : FullSize.X,
                        bHorizontalWall ? FullSize.Y : NegativeSegmentLength,
                        FullSize.Z),
                    WallColor,
                    0.0f,
                    false);
            }
            if (PositiveSegmentLength > 20.0f)
            {
                const float SegmentCenter = Span * 0.5f - PositiveSegmentLength * 0.5f;
                AddWall(
                    WallCenter + Axis * SegmentCenter,
                    FVector(
                        bHorizontalWall ? PositiveSegmentLength : FullSize.X,
                        bHorizontalWall ? FullSize.Y : PositiveSegmentLength,
                        FullSize.Z),
                    WallColor,
                    0.0f,
                    false);
            }
        };

        AddFloor(Center, FVector(Width, Depth, 100.0f), Color, -55.0f, 0.0f);
        AddWallWithOpening(
            FVector(Center.X - Width * 0.5f, Center.Y, Height * 0.5f),
            FVector(40.0f, Depth, Height),
            Color,
            (OpeningMask & 8u) != 0,
            false,
            OpeningCenterY,
            OpeningWidth);
        AddWallWithOpening(
            FVector(Center.X + Width * 0.5f, Center.Y, Height * 0.5f),
            FVector(40.0f, Depth, Height),
            Color,
            (OpeningMask & 2u) != 0,
            false,
            OpeningCenterY,
            OpeningWidth);
        AddWallWithOpening(
            FVector(Center.X, Center.Y - Depth * 0.5f, Height * 0.5f),
            FVector(Width, 40.0f, Height),
            Color,
            (OpeningMask & 1u) != 0,
            true,
            OpeningCenterX,
            OpeningWidth);
        AddWallWithOpening(
            FVector(Center.X, Center.Y + Depth * 0.5f, Height * 0.5f),
            FVector(Width, 40.0f, Height),
            Color,
            (OpeningMask & 4u) != 0,
            true,
            NorthOpeningCenterX,
            NorthOpeningWidth);
        if (bRoof)
        {
            AddFloor(Center, FVector(Width, Depth, 40.0f), Color, Height, 0.0f);
        }
    };

    const FVector RouteAnchors[] = {
        Level.StartLocation,
        Level.InsertionLocation,
        Level.CheckpointLocation,
        Level.BreachLocation,
        Level.IntelLocation,
        Level.OpenCombatLocation,
        Level.ExtractionLocation};
    const auto AddRoute = [&AddSegmentFloor, &RouteAnchors](const float Width, const FColor& Color)
    {
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(RouteAnchors) - 1; ++Index)
        {
            AddSegmentFloor(RouteAnchors[Index], RouteAnchors[Index + 1], Width, Color, -55.0f);
        }
    };

    const FVector RouteDirection = (Level.ExtractionLocation - Level.StartLocation).GetSafeNormal2D();
    const FVector RouteSide(-RouteDirection.Y, RouteDirection.X, 0.0f);
    const float RouteYaw = FMath::RadiansToDegrees(FMath::Atan2(RouteDirection.Y, RouteDirection.X));
    const FVector Midpoint = FMath::Lerp(Level.CheckpointLocation, Level.OpenCombatLocation, 0.5f);
    const auto RoutePoint = [&RouteDirection, &RouteSide](
        const FVector& Anchor,
        const float Along,
        const float Across)
    {
        return Anchor + RouteDirection * Along + RouteSide * Across;
    };

    int32 SableHeroPieceCount = 0;
    const auto MarkSableHeroPiece = [&SableHeroPieceCount](AStaticMeshActor* Piece, const TCHAR* Role)
    {
        if (!Piece)
        {
            return;
        }

        Piece->Tags.AddUnique(TEXT("CampaignLayout.SableQuay.HeroShip"));
        Piece->Tags.AddUnique(FName(FString::Printf(TEXT("SableQuay.Hero.%s"), Role ? Role : TEXT("Piece"))));
        ++SableHeroPieceCount;
    };

    switch (Level.Level)
    {
    case EBMCampaignLevel::SableQuay:
    {
        // SableQuay is the opening operation, but it should already feel like
        // a place with a history: a relay warehouse sits off the main route,
        // a service lane wraps its southern wall, and a collapsed gantry
        // overlooks the final yard.  The route remains open in the middle so
        // the player can choose exposed speed, warehouse cover, or the raised
        // flank without moving any mission anchor.
        AddTerrainBed(
            FMath::Lerp(Level.StartLocation, Level.ExtractionLocation, 0.5f),
            FVector(5400.0f, 3500.0f, 80.0f),
            GroundColor,
            -50.0f,
            RouteYaw);

        // The far edge is a broken dry-quay embankment, not an unbounded void.
        // These collision-bearing wreck forms provide a readable horizon and
        // make the opening battlefield feel like a real abandoned facility.
        const FVector HorizonAnchor = Level.ExtractionLocation + RouteDirection * 900.0f;
        for (int32 RidgeIndex = -1; RidgeIndex <= 1; ++RidgeIndex)
        {
            AddWreckage(
                HorizonAnchor + RouteSide * (static_cast<float>(RidgeIndex) * 430.0f) + FVector(0.0f, 0.0f, 34.0f),
                FVector(
                    RidgeIndex == 0 ? 1.90f : 1.55f,
                    RidgeIndex == 0 ? 1.12f : 0.96f,
                    RidgeIndex == 0 ? 1.18f : 1.02f),
                WeatherColor,
                RouteYaw + static_cast<float>(RidgeIndex) * 17.0f);
        }
        AddRoute(620.0f, GroundColor);

        // Put the relay house on the actual breach approach.  The previous
        // flank placement left its east wall very close to the insertion
        // camera while the facade pieces were authored against the opposite
        // side of the room, producing one large unbroken wall and several
        // apparently detached panels.  This center keeps the south doorway
        // aligned with the existing breach trace and leaves the checkpoint
        // approach open for the first firefight.
        const FVector RelayHouseCenter =
            Level.BreachLocation + FVector(0.0f, 310.0f, 0.0f);
        AddRoom(
            RelayHouseCenter,
            760.0f,
            560.0f,
            360.0f,
            PrimaryColor,
            true,
            1u | 4u,
            420.0f,
            0.0f,
            0.0f,
            420.0f,
            0.0f);

        // These are real doorway/window/machinery meshes placed against the
        // room's split walls. They are not decals, and their collision keeps
        // the compound readable as an occupied structure when approached
        // from either route.
        AddDoor(
            RelayHouseCenter + FVector(0.0f, -292.0f, 108.0f),
            FVector(1.35f, 1.0f, 1.70f),
            AccentColor,
            0.0f);
        AddWindow(
            RelayHouseCenter + FVector(-382.0f, 72.0f, 278.0f),
            FVector(1.35f, 1.0f, 1.05f),
            WeatherColor,
            90.0f);
        AddWindow(
            RelayHouseCenter + FVector(382.0f, -88.0f, 278.0f),
            FVector(1.35f, 1.0f, 1.05f),
            AccentColor,
            90.0f);
        AddMachinery(
            RelayHouseCenter + FVector(210.0f, 122.0f, 105.0f),
            FVector(1.30f),
            WeatherColor,
            18.0f);
        AddSign(
            RelayHouseCenter + FVector(0.0f, -305.0f, 325.0f),
            FVector(1.25f, 1.0f, 0.82f),
            AccentColor,
            0.0f);
        AddGrate(
            RelayHouseCenter + FVector(0.0f, -255.0f, 8.0f),
            FVector(2.60f, 0.72f, 0.10f),
            WeatherColor,
            0.0f);

        // The relay house is the opening operation's hero landmark. The
        // collision shell above gives it the correct room and doorway bounds;
        // these additional facade modules give the same room a believable
        // construction language at combat distance instead of leaving one
        // uninterrupted block face in the camera. Keep a clear center bay for
        // the authored door and the breach interaction trace.
        const FColor RelayFacadeColor(
            FMath::Clamp(PrimaryColor.R + 34, 0, 255),
            FMath::Clamp(PrimaryColor.G + 36, 0, 255),
            FMath::Clamp(PrimaryColor.B + 34, 0, 255));
        // Keep the relay facade in the world-material palette. The bright
        // cyan accent is reserved for objective beacons and route signals;
        // using it on every facade trim made the compound read as debug
        // geometry in the packaged Metal capture.
        const FColor RelayTrimColor(58, 122, 118);
        for (const float SideOffset : {-250.0f, 250.0f})
        {
            AddPanel(
                RelayHouseCenter + FVector(SideOffset, -292.0f, 232.0f),
                FVector(2.05f, 0.12f, 1.55f),
                RelayFacadeColor,
                0.0f);
            AddWindow(
                RelayHouseCenter + FVector(SideOffset, -305.0f, 292.0f),
                FVector(1.05f, 1.0f, 0.82f),
                WeatherColor,
                0.0f);
        }
        AddPanel(
            RelayHouseCenter + FVector(0.0f, -292.0f, 334.0f),
            FVector(3.55f, 0.10f, 0.18f),
            RelayTrimColor,
            0.0f);
        AddSign(
            RelayHouseCenter + FVector(0.0f, -307.0f, 328.0f),
            FVector(1.45f, 1.0f, 0.68f),
            RelayTrimColor,
            0.0f);
        // The room already has a collision-bearing roof at Height=360. Do
        // not add a second high truss here: SM_BM_Env_Truss has an asymmetric
        // diagonal silhouette whose authored bounds extend beyond the visible
        // posts, producing floating fragments in the combat camera.
        for (const float SideOffset : {-300.0f, 300.0f})
        {
            AddPipe(
                RelayHouseCenter + FVector(SideOffset, -212.0f, 220.0f),
                FVector(0.18f, 0.18f, 4.05f),
                RelayTrimColor,
                0.0f);
        }
        AddMachinery(
            RelayHouseCenter + FVector(270.0f, 180.0f, 105.0f),
            FVector(0.92f),
            WeatherColor,
            90.0f);

        // A collision-bearing service lane provides a genuine alternate route
        // around the compound and terminates at the breach approach. It is
        // deliberately offset from the interaction points so the relay and
        // archive traces retain clear access.
        const FVector ServiceLaneStart = Level.CheckpointLocation + RouteSide * 500.0f;
        const FVector ServiceLaneEnd = Level.BreachLocation + RouteSide * 470.0f;
        AddSegmentFloor(ServiceLaneStart, ServiceLaneEnd, 300.0f, PrimaryColor, -55.0f);
        AddWall(
            FMath::Lerp(ServiceLaneStart, ServiceLaneEnd, 0.42f)
                + RouteSide * 185.0f
                + FVector(0.0f, 0.0f, 92.0f),
            FVector(520.0f, 80.0f, 184.0f),
            GroundColor,
            RouteYaw,
            true);
        AddWreckage(
            FMath::Lerp(ServiceLaneStart, ServiceLaneEnd, 0.70f)
                - RouteSide * 155.0f
                + FVector(0.0f, 0.0f, 34.0f),
            FVector(0.92f, 0.68f, 0.72f),
            WeatherColor,
            RouteYaw - 22.0f);

        // The final yard has a raised maintenance platform reached by broad
        // stairs. The platform is beside, rather than across, the extraction
        // anchor, preserving a ground-level retreat while adding vertical
        // cover and a meaningful sightline choice.
        // Keep the optional gantry on the outer flank.  At the former 520 cm
        // offset its stair approach crossed the ground-level extraction trace
        // at a shallow angle, so a valid capsule arrival could still put the
        // boat interaction ray through the stair collision proxy.  The wider
        // offset preserves the elevated route while leaving the extraction
        // apron and its sightline consistently clear.
        const FVector GantryPlatform = Level.ExtractionLocation + RouteSide * 720.0f;
        AddFloor(
            GantryPlatform,
            FVector(920.0f, 430.0f, 100.0f),
            WeatherColor,
            250.0f,
            RouteYaw);
        AddStairs(
            Level.OpenCombatLocation + RouteSide * 520.0f,
            GantryPlatform,
            250.0f,
            -5.0f,
            300.0f,
            AccentColor);
        for (const float Along : {-300.0f, 300.0f})
        {
            AddPipe(
                GantryPlatform + RouteDirection * Along + FVector(0.0f, 0.0f, 290.0f),
                FVector(0.30f, 0.30f, 5.6f),
                PrimaryColor,
                0.0f);
        }
        // Keep the raised platform's sightline open. Its floor, stairs, and
        // paired service posts provide the intended vertical landmark; the
        // unsupported truss previously read as a detached cyan strip above
        // the final yard on the Metal renderer.
        AddPanel(
            GantryPlatform - RouteSide * 190.0f + FVector(0.0f, 0.0f, 170.0f),
            FVector(2.10f, 0.16f, 1.70f),
            PrimaryColor,
            RouteYaw + 90.0f);

        // Broken pier plates and low cover turn the open combat pocket into
        // readable lanes while keeping the centerline available to the
        // mission's final-defense spawn ring.
        // Break the old single slab into two offset cover modules. The gap
        // creates a deliberate peek lane and removes the wall-sized black
        // silhouette that used to dominate the first combat capture while
        // retaining two collision-bearing pieces for the final-defense ring.
        const FVector CombatCoverCenter =
            Level.OpenCombatLocation + RouteSide * 430.0f + FVector(0.0f, 0.0f, 90.0f);
        for (const float AlongRoute : {-300.0f, 300.0f})
        {
            AddCover(
                CombatCoverCenter + RouteDirection * AlongRoute,
                FVector(220.0f, 130.0f, 140.0f),
                GroundColor,
                RouteYaw + 8.0f);
        }
        AddPanel(
            CombatCoverCenter + RouteSide * 115.0f + FVector(0.0f, 0.0f, 175.0f),
            FVector(1.35f, 0.18f, 1.65f),
            PrimaryColor,
            RouteYaw + 8.0f);
        AddMachinery(
            CombatCoverCenter - RouteSide * 118.0f + FVector(0.0f, 0.0f, 88.0f),
            FVector(0.72f),
            AccentColor,
            RouteYaw - 10.0f);
        AddWall(
            Level.OpenCombatLocation - RouteSide * 360.0f + FVector(0.0f, 0.0f, 78.0f),
            FVector(420.0f, 120.0f, 156.0f),
            WeatherColor,
            RouteYaw - 16.0f,
            true);
        // Keep the wreck as physical cover, but place it outside the final
        // approach lane.  Its former +190 cm offset put the collision proxy
        // directly across the capsule path from OpenCombat to Extraction;
        // that was invisible while the player capsule ignored WorldStatic,
        // then correctly became an unintentional hard stop when collision was
        // made authoritative.  The outer flank remains reachable cover and
        // leaves the centre lane genuinely traversable.
        AddWreckage(
            Level.OpenCombatLocation - RouteSide * 520.0f + RouteDirection * 180.0f + FVector(0.0f, 0.0f, 34.0f),
            FVector(1.12f, 0.82f, 0.86f),
            WeatherColor,
            RouteYaw + 28.0f);
        AddPanel(
            Level.ExtractionLocation + RouteSide * 260.0f + FVector(0.0f, 0.0f, 180.0f),
            FVector(2.4f, 0.20f, 2.0f),
            AccentColor,
            RouteYaw + 90.0f);

        // Hero quay landmark: a grounded patrol-boat/relay tender on the
        // outer flank.  The former Sable composition had a relay room and a
        // gantry, but both were built from the same upright block rhythm and
        // read as another runtime kit cluster in the camera.  This hull uses
        // a long deck, two raised side plates, an angled bow, stern bulkhead,
        // wheelhouse, mast, rail posts, and deck cargo.  Every structural
        // module is collision-bearing; the central mission lane stays clear
        // because the hull is placed 820 cm off the route axis.
        const FVector HeroShipCenter =
            Level.OpenCombatLocation - RouteSide * 820.0f + RouteDirection * 240.0f;
        const FColor HeroHull(78, 72, 62);
        const FColor HeroShadow(34, 45, 50);
        const FColor HeroTrim(214, 142, 48);
        const FColor HeroSignal(32, 182, 168);
        const float HeroShipYaw = RouteYaw;

        MarkSableHeroPiece(
            AddFloor(
                HeroShipCenter,
                FVector(1320.0f, 760.0f, 100.0f),
                HeroShadow,
                112.0f,
                HeroShipYaw),
            TEXT("Deck"));
        MarkSableHeroPiece(
            AddWall(
                HeroShipCenter + RouteSide * 308.0f + FVector(0.0f, 0.0f, 168.0f),
                FVector(1280.0f, 104.0f, 336.0f),
                HeroHull,
                HeroShipYaw,
                false),
            TEXT("PortHull"));
        MarkSableHeroPiece(
            AddWall(
                HeroShipCenter - RouteSide * 308.0f + FVector(0.0f, 0.0f, 168.0f),
                FVector(1280.0f, 104.0f, 336.0f),
                HeroHull,
                HeroShipYaw,
                false),
            TEXT("StarboardHull"));
        MarkSableHeroPiece(
            AddWall(
                HeroShipCenter - RouteDirection * 590.0f + FVector(0.0f, 0.0f, 160.0f),
                FVector(110.0f, 690.0f, 320.0f),
                HeroHull,
                HeroShipYaw,
                false),
            TEXT("Stern"));

        // Two angled plates converge on the bow, making the silhouette read
        // as a vessel from the insertion camera instead of a squared room.
        MarkSableHeroPiece(
            AddWall(
                HeroShipCenter + RouteDirection * 520.0f + RouteSide * 205.0f + FVector(0.0f, 0.0f, 148.0f),
                FVector(360.0f, 94.0f, 296.0f),
                HeroHull,
                HeroShipYaw + 34.0f,
                false),
            TEXT("BowPort"));
        MarkSableHeroPiece(
            AddWall(
                HeroShipCenter + RouteDirection * 520.0f - RouteSide * 205.0f + FVector(0.0f, 0.0f, 148.0f),
                FVector(360.0f, 94.0f, 296.0f),
                HeroHull,
                HeroShipYaw - 34.0f,
                false),
            TEXT("BowStarboard"));

        const FVector WheelhouseCenter =
            HeroShipCenter - RouteDirection * 150.0f + RouteSide * 20.0f;
        MarkSableHeroPiece(
            AddFloor(
                WheelhouseCenter,
                FVector(470.0f, 360.0f, 80.0f),
                HeroShadow,
                322.0f,
                HeroShipYaw),
            TEXT("WheelhouseDeck"));
        MarkSableHeroPiece(
            AddWall(
                WheelhouseCenter + RouteSide * 136.0f + FVector(0.0f, 0.0f, 430.0f),
                FVector(450.0f, 58.0f, 216.0f),
                HeroShadow,
                HeroShipYaw,
                false),
            TEXT("WheelhousePort"));
        MarkSableHeroPiece(
            AddWall(
                WheelhouseCenter - RouteSide * 136.0f + FVector(0.0f, 0.0f, 430.0f),
                FVector(450.0f, 58.0f, 216.0f),
                HeroShadow,
                HeroShipYaw,
                false),
            TEXT("WheelhouseStarboard"));
        MarkSableHeroPiece(
            AddPanel(
                WheelhouseCenter + RouteDirection * 228.0f + FVector(0.0f, 0.0f, 432.0f),
                FVector(1.65f, 0.14f, 0.86f),
                HeroSignal,
                HeroShipYaw),
            TEXT("WheelhouseWindow"));
        MarkSableHeroPiece(
            AddDoor(
                WheelhouseCenter - RouteDirection * 228.0f + FVector(0.0f, 0.0f, 418.0f),
                FVector(1.12f, 0.90f, 1.40f),
                HeroTrim,
                HeroShipYaw + 90.0f),
            TEXT("WheelhouseDoor"));
        MarkSableHeroPiece(
            AddFloor(
                WheelhouseCenter,
                FVector(520.0f, 410.0f, 70.0f),
                HeroHull,
                560.0f,
                HeroShipYaw),
            TEXT("WheelhouseRoof"));

        // A mast and small rail rhythm establish the vessel's scale.  They
        // are rooted in the deck/roof rather than suspended in the sky.
        MarkSableHeroPiece(
            AddPipe(
                WheelhouseCenter + RouteDirection * 30.0f + FVector(0.0f, 0.0f, 790.0f),
                FVector(0.26f, 0.26f, 5.0f),
                HeroSignal,
                0.0f),
            TEXT("Mast"));
        MarkSableHeroPiece(
            AddSign(
                WheelhouseCenter + RouteDirection * 222.0f + FVector(0.0f, 0.0f, 540.0f),
                FVector(1.30f, 0.90f, 0.56f),
                HeroTrim,
                HeroShipYaw),
            TEXT("BridgeMark"));
        for (const float Along : {-470.0f, 0.0f, 470.0f})
        {
            MarkSableHeroPiece(
                AddPipe(
                    HeroShipCenter + RouteDirection * Along + RouteSide * 365.0f + FVector(0.0f, 0.0f, 252.0f),
                    FVector(0.12f, 0.12f, 1.85f),
                    HeroTrim,
                    0.0f),
                TEXT("PortRail"));
            MarkSableHeroPiece(
                AddPipe(
                    HeroShipCenter + RouteDirection * Along - RouteSide * 365.0f + FVector(0.0f, 0.0f, 252.0f),
                    FVector(0.12f, 0.12f, 1.85f),
                    HeroTrim,
                    0.0f),
                TEXT("StarboardRail"));
        }
        MarkSableHeroPiece(
            AddGrate(
                HeroShipCenter + RouteDirection * 300.0f + RouteSide * 8.0f + FVector(0.0f, 0.0f, 166.0f),
                FVector(3.8f, 0.70f, 0.10f),
                HeroSignal,
                HeroShipYaw),
            TEXT("DeckGrate"));
        MarkSableHeroPiece(
            AddMachinery(
                HeroShipCenter - RouteDirection * 360.0f + RouteSide * 70.0f + FVector(0.0f, 0.0f, 225.0f),
                FVector(1.10f),
                HeroHull,
                HeroShipYaw - 8.0f),
            TEXT("DeckMachinery"));
        MarkSableHeroPiece(
            AddWreckage(
                HeroShipCenter + RouteDirection * 420.0f - RouteSide * 70.0f + FVector(0.0f, 0.0f, 190.0f),
                FVector(0.92f, 0.62f, 0.66f),
                HeroTrim,
                HeroShipYaw + 18.0f),
            TEXT("BowCargo"));

        // The inner-side stair is reachable from the open combat pocket and
        // gives the hero landmark a real elevated combat perch.  The stair
        // pieces deliberately remain beside the route, never across it.
        AddStairs(
            HeroShipCenter - RouteSide * 555.0f - RouteDirection * 250.0f,
            HeroShipCenter - RouteSide * 345.0f - RouteDirection * 250.0f,
            230.0f,
            -5.0f,
            162.0f,
            HeroTrim);

        // Three grounded cover islands stage the approach and provide
        // alternate peeks around the hull without sealing the mission lane.
        MarkSableHeroPiece(
            AddCover(
                HeroShipCenter - RouteDirection * 410.0f + RouteSide * 560.0f + FVector(0.0f, 0.0f, 94.0f),
                FVector(240.0f, 150.0f, 188.0f),
                GroundColor,
                HeroShipYaw - 12.0f),
            TEXT("InnerCover"));
        MarkSableHeroPiece(
            AddCover(
                HeroShipCenter + RouteDirection * 360.0f + RouteSide * 570.0f + FVector(0.0f, 0.0f, 88.0f),
                FVector(210.0f, 140.0f, 176.0f),
                HeroTrim,
                HeroShipYaw + 18.0f),
            TEXT("BowCover"));
        MarkSableHeroPiece(
            AddWreckage(
                HeroShipCenter + RouteDirection * 20.0f - RouteSide * 570.0f + FVector(0.0f, 0.0f, 46.0f),
                FVector(1.12f, 0.76f, 0.72f),
                HeroHull,
                HeroShipYaw - 26.0f),
            TEXT("OuterWreck"));
        break;
    }

    case EBMCampaignLevel::Floodline:
    {
        // Flooded drainage yard: a broad silt bed surrounds a raised
        // maintenance bridge, with segmented sluice walls and real side lanes
        // instead of two fixed-axis slabs that hid the floodgate route.
        AddTerrainBed(Midpoint, FVector(3600.0f, 2500.0f, 80.0f), GroundColor, -50.0f, RouteYaw);
        AddRoute(360.0f, GroundColor);
        // This section turns relative to the campaign-wide extraction vector.
        // Use its own breach-to-intel axis so the floodgate crossing follows
        // the actual drainage channel instead of cutting across the player's
        // capsule path from the checkpoint.  It is deliberately deck-height:
        // the side walls and cover provide the structural variation here,
        // while the primary objective lane remains continuously traversable.
        const FVector BreachToIntelDirection =
            (Level.IntelLocation - Level.BreachLocation).GetSafeNormal2D();
        AddSegmentFloor(
            Level.BreachLocation + BreachToIntelDirection * 220.0f,
            Level.IntelLocation - BreachToIntelDirection * 220.0f,
            760.0f,
            WeatherColor,
            -55.0f);
        AddStairs(
            Level.BreachLocation + BreachToIntelDirection * 60.0f,
            Level.BreachLocation + BreachToIntelDirection * 300.0f,
            260.0f,
            -5.0f,
            -5.0f,
            AccentColor);
        AddStairs(
            Level.IntelLocation - BreachToIntelDirection * 300.0f,
            Level.IntelLocation - BreachToIntelDirection * 60.0f,
            260.0f,
            -5.0f,
            -5.0f,
            AccentColor);
        // Keep the negative bank outside the opening-wave flank envelope. The
        // forward arc can legitimately place its sixth member around -670 cm
        // on the route-side axis; a -500 cm wall put that enemy behind a
        // continuous 1,050 cm barrier and left the player with no direct
        // capsule route to a valid sightline. It remains real cover, but the
        // widened central channel now gives both the player and that spawn a
        // grounded, flankable lane.
        for (const float ChannelOffset : {-820.0f, 500.0f})
        {
            AddSegmentWall(
                Level.CheckpointLocation + RouteSide * ChannelOffset,
                Level.BreachLocation + RouteSide * ChannelOffset,
                86.0f,
                250.0f,
                PrimaryColor,
                0.0f,
                false);
            AddSegmentWall(
                Level.IntelLocation + RouteSide * ChannelOffset,
                Level.OpenCombatLocation + RouteSide * ChannelOffset,
                86.0f,
                190.0f,
                PrimaryColor,
                0.0f,
                false);
        }
        // The open-combat approach is a broad flood basin, not a sealed
        // corridor.  The former +/-500 cm walls sat inside the player and
        // enemy flank envelope: they correctly blocked the capsule, but made
        // several valid reposition legs terminate against the same bank.
        // Keep the drainage walls as real collision-bearing structures while
        // moving them to the outer banks, leaving the 760 cm bridge and the
        // +/-330 cm encounter lanes genuinely flankable.
        constexpr float OpenCombatBankOffset = 820.0f;
        for (const float BankOffset : {-OpenCombatBankOffset, OpenCombatBankOffset})
        {
            AddSegmentWall(
                Level.IntelLocation + RouteSide * BankOffset,
                Level.OpenCombatLocation + RouteSide * BankOffset,
                86.0f,
                190.0f,
                PrimaryColor,
                0.0f,
                false);
        }
        // Low concrete islands create readable peek points on both sides of
        // the water channel while leaving the central bridge and objective
        // traces clear.
        AddCover(RoutePoint(Level.CheckpointLocation, 80.0f, -330.0f) + FVector(0.0f, 0.0f, 78.0f), FVector(250.0f, 120.0f, 156.0f), AccentColor, RouteYaw - 12.0f);
        AddCover(RoutePoint(Level.BreachLocation, -100.0f, 350.0f) + FVector(0.0f, 0.0f, 78.0f), FVector(220.0f, 120.0f, 156.0f), WeatherColor, RouteYaw + 15.0f);
        AddPanel(Level.BreachLocation + RouteSide * 170.0f + FVector(0.0f, 0.0f, 330.0f), FVector(2.4f, 0.16f, 1.8f), AccentColor, RouteYaw + 90.0f);
        AddPipe(Level.CheckpointLocation + RouteSide * 230.0f + FVector(0.0f, 0.0f, 270.0f), FVector(0.20f, 0.20f, 5.0f), AccentColor, RouteYaw + 90.0f);
        AddPipe(Level.OpenCombatLocation - RouteSide * 240.0f + FVector(0.0f, 0.0f, 270.0f), FVector(0.20f, 0.20f, 5.0f), WeatherColor, RouteYaw + 90.0f);
        break;
    }

    case EBMCampaignLevel::RelayHouse:
    {
        // Rooftop relay: disconnected roof plates with parapets and narrow
        // service bridges make this a true rooftop route.
        // Keep a lower service route under the roof plates so every mission
        // objective remains reachable even when the player chooses not to
        // climb the optional elevated flank.
        AddRoute(420.0f, GroundColor);
        AddFloor(Level.StartLocation, FVector(900.0f, 800.0f, 100.0f), GroundColor, -55.0f, 0.0f);
        const FVector RelayRoofPlate = Level.CheckpointLocation + RouteSide * 560.0f;
        const FVector RelayArchivePlate = Level.BreachLocation + RouteSide * 520.0f;
        AddFloor(RelayRoofPlate, FVector(1100.0f, 700.0f, 100.0f), PrimaryColor, 250.0f, 0.0f);
        AddFloor(RelayArchivePlate, FVector(900.0f, 900.0f, 100.0f), PrimaryColor, 390.0f, 0.0f);
        AddStairs(
            Level.CheckpointLocation + RouteSide * 220.0f,
            RelayRoofPlate,
            260.0f,
            -5.0f,
            300.0f,
            AccentColor);
        AddStairs(
            RelayRoofPlate,
            RelayArchivePlate,
            220.0f,
            300.0f,
            440.0f,
            AccentColor);
        AddStairs(
            RelayArchivePlate,
            Level.IntelLocation + RouteSide * 180.0f,
            220.0f,
            440.0f,
            -5.0f,
            AccentColor);
        AddPipe(RelayRoofPlate + FVector(-420.0f, -240.0f, 420.0f), FVector(0.26f, 0.26f, 5.4f), AccentColor, 0.0f);
        AddPipe(RelayRoofPlate + FVector(420.0f, 240.0f, 420.0f), FVector(0.26f, 0.26f, 5.4f), AccentColor, 0.0f);
        AddPipe(RelayArchivePlate + FVector(-320.0f, -300.0f, 560.0f), FVector(0.30f, 0.30f, 6.6f), WeatherColor, 0.0f);
        for (const float RailSide : {-300.0f, 300.0f})
        {
            AddSegmentWall(
                RelayRoofPlate - RouteDirection * 430.0f + RouteSide * RailSide,
                RelayRoofPlate + RouteDirection * 430.0f + RouteSide * RailSide,
                28.0f,
                86.0f,
                AccentColor,
                250.0f,
                true);
        }
        AddSegmentWall(
            RelayArchivePlate - RouteDirection * 330.0f + RouteSide * 390.0f,
            RelayArchivePlate + RouteDirection * 330.0f + RouteSide * 390.0f,
            28.0f,
            86.0f,
            WeatherColor,
            390.0f,
            true);
        // This checkpoint facade is real cover, but a single 1,000 cm slab
        // sealed the entire south approach.  The opening-wave formation can
        // legitimately place an enemy behind that facade, which made the
        // target resolver report no visibility even when the enemy was in the
        // reticle.  Keep the side parapets and add an overhead lintel so the
        // scene still reads as a constructed relay frontage, while leaving a
        // capsule-sized doorway for both the player and the opening wave.
        const float CheckpointFacadeY = Level.CheckpointLocation.Y - 320.0f;
        AddSegmentWall(
            FVector(Level.CheckpointLocation.X - 500.0f, CheckpointFacadeY, 0.0f),
            FVector(Level.CheckpointLocation.X - 260.0f, CheckpointFacadeY, 0.0f),
            40.0f,
            360.0f,
            PrimaryColor,
            0.0f,
            false);
        AddSegmentWall(
            FVector(Level.CheckpointLocation.X + 300.0f, CheckpointFacadeY, 0.0f),
            FVector(Level.CheckpointLocation.X + 500.0f, CheckpointFacadeY, 0.0f),
            40.0f,
            360.0f,
            PrimaryColor,
            0.0f,
            false);
        AddWall(
            Level.CheckpointLocation + FVector(0.0f, -320.0f, 400.0f),
            FVector(580.0f, 40.0f, 80.0f),
            PrimaryColor,
            0.0f,
            false);
        AddWall(Level.BreachLocation + FVector(0.0f, 420.0f, 260.0f), FVector(900.0f, 40.0f, 520.0f), PrimaryColor, 0.0f, false);
        break;
    }

    case EBMCampaignLevel::NightglassCore:
    {
        // Underground core: a segmented reactor chamber rather than one
        // uninterrupted box.  All four sides have controlled openings so a
        // player looking across the room sees a route, service gallery, or
        // branch instead of a blank wall.  The room still owns collision and
        // navigation; the supported frames below add the physical rhythm.
        // The extraction anchor sits immediately beyond the chamber's west
        // wall. Keep that wall physical, but make the north service opening
        // wide enough for the capsule's authored approach to enter the room
        // without sliding along a solid wall. The other three openings stay
        // narrow so this remains a room with meaningful cover and sightline
        // breaks rather than an empty shell.
        AddRoom(Midpoint, 2800.0f, 1400.0f, 540.0f, GroundColor, true, 1u | 2u | 4u | 8u, 1000.0f, 0.0f, 0.0f, 2200.0f, 0.0f);
        AddRoute(520.0f, GroundColor);
        AddRoom(
            Level.IntelLocation + FVector(0.0f, 500.0f, 0.0f),
            820.0f,
            620.0f,
            470.0f,
            PrimaryColor,
            true,
            1u,
            500.0f,
            0.0f,
            0.0f,
            500.0f,
            0.0f);

        // Four supported bulkheads divide the sightline into readable combat
        // beats.  The posts are grounded on the collision floor and the
        // beams span between them; none of the pieces hangs in the air.
        for (int32 FrameIndex = 0; FrameIndex < 4; ++FrameIndex)
        {
            const float Alpha = 0.16f + 0.21f * static_cast<float>(FrameIndex);
            const FVector FramePoint = FMath::Lerp(Level.InsertionLocation, Level.OpenCombatLocation, Alpha);
            const float FrameSideOffset = 440.0f;
            for (const float SideSign : {-1.0f, 1.0f})
            {
                AddPipe(
                    FramePoint + RouteSide * (SideSign * FrameSideOffset) + FVector(0.0f, 0.0f, 220.0f),
                    FVector(0.22f, 0.22f, 4.6f),
                    AccentColor,
                    RouteYaw + 90.0f);
                AddPanel(
                    FramePoint + RouteSide * (SideSign * (FrameSideOffset - 10.0f)) + FVector(0.0f, 0.0f, 252.0f),
                    FVector(1.35f, 0.10f, 1.02f),
                    SideSign < 0.0f ? PrimaryColor : GroundColor,
                    RouteYaw + 90.0f);
            }
            AddWall(
                FramePoint + FVector(0.0f, 0.0f, 484.0f),
                FVector(900.0f, 46.0f, 72.0f),
                PrimaryColor,
                RouteYaw + 90.0f,
                false);
            AddGrate(
                FramePoint + FVector(0.0f, 0.0f, 7.0f),
                FVector(2.45f, 0.68f, 0.08f),
                WeatherColor,
                RouteYaw);
        }

        // A raised service plinth gives the core a usable vertical shortcut.
        // The lower route remains untouched, while stairs make the optional
        // platform reachable and keep its collision/nav data honest.
        const FVector ServicePlinth = Level.IntelLocation - RouteSide * 290.0f;
        AddFloor(ServicePlinth, FVector(620.0f, 420.0f, 100.0f), PrimaryColor, 180.0f, RouteYaw);
        AddStairs(
            ServicePlinth - RouteDirection * 300.0f,
            ServicePlinth - RouteDirection * 120.0f,
            220.0f,
            -5.0f,
            180.0f,
            AccentColor);
        AddStairs(
            ServicePlinth + RouteDirection * 120.0f,
            ServicePlinth + RouteDirection * 300.0f,
            220.0f,
            180.0f,
            -5.0f,
            AccentColor);
        AddMachinery(ServicePlinth + RouteSide * 120.0f + FVector(0.0f, 0.0f, 82.0f), FVector(1.05f), WeatherColor, RouteYaw);
        AddDoor(ServicePlinth + RouteSide * 208.0f + FVector(0.0f, 0.0f, 108.0f), FVector(1.20f, 1.0f, 1.55f), AccentColor, RouteYaw);
        break;
    }

    case EBMCampaignLevel::AshenDocks:
        // Burned dry dock: parallel piers, open water gaps, crane legs, and a
        // narrow cross-dock at the breach.
        AddTerrainBed(Midpoint, FVector(3900.0f, 3000.0f, 80.0f), GroundColor, -50.0f, RouteYaw);
        AddRoute(430.0f, GroundColor);
        AddFloor(Level.StartLocation, FVector(1000.0f, 520.0f, 100.0f), GroundColor, -55.0f, 0.0f);
        AddFloor(Level.InsertionLocation, FVector(1300.0f, 420.0f, 100.0f), GroundColor, -55.0f, 0.0f);
        AddFloor(Level.IntelLocation, FVector(1050.0f, 560.0f, 100.0f), WeatherColor, -55.0f, RouteYaw);
        AddFloor(Level.OpenCombatLocation, FVector(1500.0f, 460.0f, 100.0f), PrimaryColor, -55.0f, 0.0f);
        AddFloor(Level.ExtractionLocation, FVector(1300.0f, 720.0f, 100.0f), GroundColor, -55.0f, RouteYaw);
        AddSegmentFloor(Level.CheckpointLocation, Level.BreachLocation, 260.0f, AccentColor, -55.0f);
        for (const float Offset : {-520.0f, 520.0f})
        {
            AddPipe(Midpoint + RouteSide * Offset + FVector(0.0f, 0.0f, 300.0f), FVector(0.34f, 0.34f, 6.0f), PrimaryColor, RouteYaw + 90.0f);
            // The north pier used to continue all the way to the open-combat
            // anchor. That made its collision proxy cross the authored
            // breach-to-intel approach, so a correctly colliding capsule
            // stopped at the pier instead of reaching the console. End that
            // pier at the breach and keep the opposite pier long to retain
            // the asymmetric dock sightline.
            const FVector PierEnd = Offset > 0.0f
                ? Level.BreachLocation + RouteSide * Offset
                : Level.OpenCombatLocation + RouteSide * Offset;
            AddSegmentWall(
                Level.CheckpointLocation + RouteSide * Offset,
                PierEnd,
                100.0f,
                150.0f,
                WeatherColor,
                0.0f,
                true);
        }
        // Cross-dock cover is split into staggered islands, leaving a clear
        // center lane for the final-defense receiving apron.
        AddCover(RoutePoint(Level.BreachLocation, -120.0f, -300.0f) + FVector(0.0f, 0.0f, 78.0f), FVector(240.0f, 120.0f, 156.0f), AccentColor, RouteYaw - 10.0f);
        AddCover(RoutePoint(Level.BreachLocation, 160.0f, 300.0f) + FVector(0.0f, 0.0f, 78.0f), FVector(220.0f, 120.0f, 156.0f), WeatherColor, RouteYaw + 14.0f);
        AddCover(RoutePoint(Level.OpenCombatLocation, -220.0f, -330.0f) + FVector(0.0f, 0.0f, 78.0f), FVector(260.0f, 120.0f, 156.0f), GroundColor, RouteYaw + 8.0f);
        AddWreckage(Level.ExtractionLocation + RouteSide * 430.0f + FVector(0.0f, 0.0f, 34.0f), FVector(1.18f, 0.86f, 0.90f), WeatherColor, RouteYaw - 18.0f);
        AddWreckage(Level.BreachLocation + RouteSide * 300.0f + FVector(0.0f, 0.0f, 30.0f), FVector(1.08f, 0.88f, 0.96f), WeatherColor, -24.0f);
        break;

    case EBMCampaignLevel::CinderWorks:
    {
        // Foundry: a grounded casting yard with an offset furnace bay, a
        // broken service wall, and a reachable side catwalk. The old pair of
        // room shells put a roof and a full wall directly into the approach
        // camera, making the operation read as a dark box with no cover.
        AddTerrainBed(
            FMath::Lerp(Level.StartLocation, Level.ExtractionLocation, 0.5f),
            FVector(3900.0f, 2800.0f, 80.0f),
            GroundColor,
            -50.0f,
            RouteYaw);
        AddRoute(460.0f, GroundColor);

        const FVector FurnaceBay = RoutePoint(Level.CheckpointLocation, 0.0f, 430.0f);
        AddFloor(FurnaceBay, FVector(920.0f, 760.0f, 100.0f), PrimaryColor, -55.0f, RouteYaw);
        AddSegmentWall(
            RoutePoint(Level.CheckpointLocation, -390.0f, 700.0f),
            RoutePoint(Level.CheckpointLocation, 390.0f, 700.0f),
            110.0f,
            300.0f,
            PrimaryColor,
            0.0f,
            false);
        AddSegmentWall(
            RoutePoint(Level.CheckpointLocation, -300.0f, -560.0f),
            RoutePoint(Level.CheckpointLocation, 120.0f, -560.0f),
            90.0f,
            220.0f,
            WeatherColor,
            0.0f,
            true);

        const FVector CastingShed = RoutePoint(Level.OpenCombatLocation, 80.0f, -620.0f);
        AddFloor(CastingShed, FVector(760.0f, 380.0f, 100.0f), WeatherColor, -55.0f, RouteYaw);
        AddSegmentWall(
            RoutePoint(Level.OpenCombatLocation, -300.0f, -620.0f),
            RoutePoint(Level.OpenCombatLocation, 340.0f, -620.0f),
            90.0f,
            320.0f,
            WeatherColor,
            0.0f,
            false);
        // Low staggered casting barriers create alternating peek lanes without
        // sealing the open-combat pocket or its objective trace.
        AddSegmentWall(
            RoutePoint(Level.OpenCombatLocation, -520.0f, 500.0f),
            RoutePoint(Level.OpenCombatLocation, -120.0f, 500.0f),
            110.0f,
            150.0f,
            AccentColor,
            0.0f,
            true);
        AddSegmentWall(
            RoutePoint(Level.OpenCombatLocation, 120.0f, -360.0f),
            RoutePoint(Level.OpenCombatLocation, 520.0f, -360.0f),
            110.0f,
            150.0f,
            PrimaryColor,
            0.0f,
            true);

        // The catwalk is now a side route rather than an overhead slab in the
        // player's primary sightline. Both stairheads meet grounded surfaces.
        const FVector CinderCatwalkStart = RoutePoint(Level.BreachLocation, 0.0f, 620.0f);
        const FVector CinderCatwalkEnd = RoutePoint(Level.IntelLocation, 0.0f, 620.0f);
        AddSegmentFloor(CinderCatwalkStart, CinderCatwalkEnd, 220.0f, WeatherColor, 220.0f);
        AddStairs(RoutePoint(Level.BreachLocation, 0.0f, 400.0f), CinderCatwalkStart, 180.0f, -5.0f, 260.0f, WeatherColor);
        AddStairs(CinderCatwalkEnd, RoutePoint(Level.IntelLocation, 0.0f, 400.0f), 180.0f, 260.0f, -5.0f, WeatherColor);
        for (const float RailOffset : {-92.0f, 92.0f})
        {
            AddSegmentWall(
                CinderCatwalkStart + RouteSide * RailOffset,
                CinderCatwalkEnd + RouteSide * RailOffset,
                28.0f,
                82.0f,
                AccentColor,
                220.0f,
                true);
        }

        for (int32 KilnIndex = 0; KilnIndex < 4; ++KilnIndex)
        {
            const float Along = -330.0f + KilnIndex * 220.0f;
            const FVector KilnPoint = RoutePoint(FurnaceBay, Along, -170.0f);
            AddPipe(KilnPoint + FVector(0.0f, 0.0f, 150.0f), FVector(1.05f, 1.05f, 1.45f), WeatherColor, RouteYaw);
            AddMachinery(RoutePoint(FurnaceBay, Along + 70.0f, 190.0f) + FVector(0.0f, 0.0f, 74.0f), FVector(0.78f), PrimaryColor, RouteYaw);
        }

        for (int32 CoverIndex = 0; CoverIndex < 3; ++CoverIndex)
        {
            const float Along = -220.0f + CoverIndex * 260.0f;
            AddWall(
                RoutePoint(Level.BreachLocation, Along, CoverIndex % 2 == 0 ? -280.0f : 280.0f) + FVector(0.0f, 0.0f, 74.0f),
                FVector(220.0f, 72.0f, 148.0f),
                CoverIndex % 2 == 0 ? AccentColor : PrimaryColor,
                RouteYaw + (CoverIndex % 2 == 0 ? -12.0f : 14.0f),
                true);
        }
        break;
    }

    case EBMCampaignLevel::SaltFoundry:
        // Salt evaporation basin: broad pale flats broken by rectangular
        // berms and raised salt pans rather than enclosed rooms.
        AddFloor(Midpoint, FVector(3400.0f, 1900.0f, 100.0f), GroundColor, -55.0f, 0.0f);
        for (int32 BasinIndex = 0; BasinIndex < 4; ++BasinIndex)
        {
            const float X = -650.0f + 820.0f * static_cast<float>(BasinIndex);
            AddFloor(FVector(X, 430.0f, 0.0f), FVector(560.0f, 420.0f, 40.0f), WeatherColor, 18.0f, 0.0f);
            AddWall(FVector(X, 650.0f, 55.0f), FVector(600.0f, 50.0f, 110.0f), PrimaryColor, 0.0f, true);
        }
        AddRoute(620.0f, GroundColor);
        break;

    case EBMCampaignLevel::BlackwaterYard:
        // Tank farm: circular tanks, containment lanes, pump corridors, and
        // a service bridge define the silhouette.
        AddTerrainBed(Midpoint, FVector(3600.0f, 2400.0f, 80.0f), GroundColor, -50.0f, RouteYaw);
        AddRoute(500.0f, GroundColor);
        AddFloor(Midpoint, FVector(3200.0f, 1800.0f, 100.0f), GroundColor, -55.0f, 0.0f);
        for (int32 TankIndex = 0; TankIndex < 6; ++TankIndex)
        {
            const float X = -700.0f + 820.0f * static_cast<float>(TankIndex % 3);
            const float Y = TankIndex < 3 ? -430.0f : 520.0f;
            // The top-right tank is still valid cover, but its original
            // center sat directly between the reachable service lane and the
            // IntelConsole. Move the complete tank/containment pair east so
            // the console approach has a real sightline without disabling
            // collision or removing the battlefield silhouette.
            const float TankCenterX = TankIndex == 5 ? X + 700.0f : X;
            const float TankCenterY = Y;
            // The tank mesh is centered on its actor origin.  Keep its base
            // on the yard deck instead of leaving every tank hovering above
            // the containment slab.
            AddPipe(FVector(TankCenterX, TankCenterY, 115.0f), FVector(2.6f, 2.6f, 2.2f), PrimaryColor, 0.0f);
            AddPipe(FVector(TankCenterX, TankCenterY + (TankCenterY < 0.0f ? 175.0f : -175.0f), 250.0f), FVector(0.18f, 0.18f, 2.7f), AccentColor, RouteYaw);
            const float ContainmentWallY = TankCenterY + (TankIndex == 5
                ? 170.0f
                : (TankCenterY < 0.0f ? 170.0f : -170.0f));
            if (TankIndex == 5)
            {
                // Keep a real service opening in this containment wall so its
                // collision remains meaningful cover without closing the
                // player's approach and visibility trace to the console.
                AddWall(
                    FVector(TankCenterX + 120.0f, ContainmentWallY, 65.0f),
                    FVector(300.0f, 45.0f, 130.0f),
                    AccentColor,
                    0.0f,
                    true);
            }
            else
            {
                AddWall(
                    FVector(X, ContainmentWallY, 65.0f),
                    FVector(540.0f, 45.0f, 130.0f),
                    AccentColor,
                    0.0f,
                    true);
            }
        }
        AddSegmentFloor(Level.BreachLocation, Level.IntelLocation, 340.0f, WeatherColor, 45.0f);
        AddStairs(
            Level.BreachLocation - RouteDirection * 220.0f,
            Level.BreachLocation - RouteDirection * 20.0f,
            240.0f,
            -5.0f,
            45.0f,
            AccentColor);
        AddStairs(
            Level.IntelLocation + RouteDirection * 20.0f,
            Level.IntelLocation + RouteDirection * 220.0f,
            240.0f,
            45.0f,
            -5.0f,
            AccentColor);
        for (const float LaneOffset : {-560.0f, 560.0f})
        {
            AddSegmentWall(
                Level.CheckpointLocation + RouteSide * LaneOffset,
                Level.BreachLocation + RouteSide * LaneOffset,
                72.0f,
                150.0f,
                PrimaryColor,
                0.0f,
                true);
            AddSegmentWall(
                Level.IntelLocation + RouteSide * LaneOffset,
                Level.OpenCombatLocation + RouteSide * LaneOffset,
                72.0f,
                150.0f,
                PrimaryColor,
                0.0f,
                true);
        }
        AddCover(RoutePoint(Level.CheckpointLocation, 120.0f, -360.0f) + FVector(0.0f, 0.0f, 78.0f), FVector(260.0f, 120.0f, 156.0f), AccentColor, RouteYaw + 12.0f);
        AddCover(RoutePoint(Level.OpenCombatLocation, -140.0f, 360.0f) + FVector(0.0f, 0.0f, 78.0f), FVector(260.0f, 120.0f, 156.0f), WeatherColor, RouteYaw - 14.0f);
        AddPanel(Level.IntelLocation + RouteSide * 260.0f + FVector(0.0f, 0.0f, 330.0f), FVector(2.8f, 0.16f, 1.8f), AccentColor, RouteYaw + 90.0f);
        break;

    case EBMCampaignLevel::GlassMarket:
    {
        // Shattered market: use broken storefronts rather than one room-sized
        // blocking shell. The two staggered shop rows leave real door gaps,
        // a central arcade, and side alleys around the mission anchors.
        AddTerrainBed(Midpoint, FVector(3400.0f, 2300.0f, 80.0f), GroundColor, -50.0f, RouteYaw);
        AddFloor(Midpoint, FVector(3200.0f, 1900.0f, 100.0f), GroundColor, -55.0f, RouteYaw);
        AddRoute(480.0f, GroundColor);

        const float SouthFacadeY = Midpoint.Y - 610.0f;
        const float NorthFacadeY = Midpoint.Y + 610.0f;
        const float FacadeXs[] = {-1100.0f, -500.0f, 100.0f, 700.0f, 1300.0f};
        for (int32 ShopIndex = 0; ShopIndex < UE_ARRAY_COUNT(FacadeXs); ++ShopIndex)
        {
            const float X = Midpoint.X + FacadeXs[ShopIndex];
            // Each 380 cm storefront is separated by a 220 cm opening. The
            // gaps are wide enough for a pawn, enemies, and weapon traces.
            AddSegmentWall(
                FVector(X - 190.0f, SouthFacadeY, 0.0f),
                FVector(X + 190.0f, SouthFacadeY, 0.0f),
                56.0f,
                330.0f,
                PrimaryColor,
                0.0f,
                false);
            if (ShopIndex == 4)
            {
                // The final north storefront faces the extraction apron. Keep
                // it physical, but cut a real gate aligned to the extraction
                // target so the player does not have to trace through a wall
                // to hold the boat interaction. Build the two side pieces
                // directly instead of using AddSegmentWall's minimum-length
                // clamp; this keeps the opening centered on the actual target
                // even when the target sits close to the storefront end.
                const float StorefrontMinX = X - 190.0f;
                const float StorefrontMaxX = X + 190.0f;
                const float GateWidth = 300.0f;
                const float GateCenterX = FMath::Clamp(
                    Level.ExtractionLocation.X,
                    StorefrontMinX + GateWidth * 0.5f,
                    StorefrontMaxX - GateWidth * 0.5f);
                const float GateMinX = GateCenterX - GateWidth * 0.5f;
                const float GateMaxX = GateCenterX + GateWidth * 0.5f;
                const float LeftLength = GateMinX - StorefrontMinX;
                const float RightLength = StorefrontMaxX - GateMaxX;
                if (LeftLength > 20.0f)
                {
                    AddWall(
                        FVector(StorefrontMinX + LeftLength * 0.5f, NorthFacadeY, 165.0f),
                        FVector(LeftLength, 56.0f, 330.0f),
                        WeatherColor,
                        0.0f,
                        false);
                }
                if (RightLength > 20.0f)
                {
                    AddWall(
                        FVector(GateMaxX + RightLength * 0.5f, NorthFacadeY, 165.0f),
                        FVector(RightLength, 56.0f, 330.0f),
                        WeatherColor,
                        0.0f,
                        false);
                }
            }
            else
            {
                AddSegmentWall(
                    FVector(X - 190.0f, NorthFacadeY, 0.0f),
                    FVector(X + 190.0f, NorthFacadeY, 0.0f),
                    56.0f,
                    330.0f,
                    WeatherColor,
                    0.0f,
                    false);
            }
            AddPanel(
                FVector(X, SouthFacadeY - 32.0f, 205.0f),
                FVector(3.25f, 0.18f, 3.05f),
                AccentColor,
                0.0f);
            AddPanel(
                FVector(X, NorthFacadeY + 32.0f, 205.0f),
                FVector(3.25f, 0.18f, 3.05f),
                WeatherColor,
                0.0f);
            AddSign(
                FVector(X, SouthFacadeY - 54.0f, 382.0f),
                FVector(0.95f, 0.10f, 0.34f),
                AccentColor,
                0.0f);
        }

        // Broken end walls retain the market footprint while opening both
        // ends into alternate approaches. Segments prevent a full-height slab
        // from pinning the player against the boundary.
        AddSegmentWall(
            FVector(Midpoint.X - 1500.0f, Midpoint.Y - 520.0f, 0.0f),
            FVector(Midpoint.X - 1500.0f, Midpoint.Y - 170.0f, 0.0f),
            56.0f,
            300.0f,
            PrimaryColor,
            0.0f,
            false);
        AddSegmentWall(
            FVector(Midpoint.X - 1500.0f, Midpoint.Y + 180.0f, 0.0f),
            FVector(Midpoint.X - 1500.0f, Midpoint.Y + 520.0f, 0.0f),
            56.0f,
            300.0f,
            PrimaryColor,
            0.0f,
            false);
        AddSegmentWall(
            FVector(Midpoint.X + 1500.0f, Midpoint.Y - 520.0f, 0.0f),
            FVector(Midpoint.X + 1500.0f, Midpoint.Y - 170.0f, 0.0f),
            56.0f,
            300.0f,
            WeatherColor,
            0.0f,
            false);
        AddSegmentWall(
            FVector(Midpoint.X + 1500.0f, Midpoint.Y + 180.0f, 0.0f),
            FVector(Midpoint.X + 1500.0f, Midpoint.Y + 520.0f, 0.0f),
            56.0f,
            300.0f,
            WeatherColor,
            0.0f,
            false);

        // Split the center island into two collision-bearing cover pieces.
        // Their gap preserves the breach/intel sightline and a direct peek
        // lane through the arcade.
        AddCover(
            FVector(Midpoint.X - 240.0f, Midpoint.Y, 78.0f),
            FVector(260.0f, 130.0f, 156.0f),
            AccentColor,
            RouteYaw + 10.0f);
        AddCover(
            FVector(Midpoint.X + 320.0f, Midpoint.Y + 80.0f, 78.0f),
            FVector(260.0f, 130.0f, 156.0f),
            WeatherColor,
            RouteYaw - 12.0f);

        // The north arcade has a supported, reachable awning rather than the
        // old overhead beam cluster. The ground route remains unobstructed.
        const FVector MarketAwning(Midpoint.X + 760.0f, Midpoint.Y + 420.0f, 0.0f);
        AddFloor(MarketAwning, FVector(820.0f, 360.0f, 100.0f), PrimaryColor, 270.0f, 0.0f);
        AddStairs(
            FVector(MarketAwning.X - 520.0f, MarketAwning.Y, 0.0f),
            FVector(MarketAwning.X - 250.0f, MarketAwning.Y, 0.0f),
            190.0f,
            -5.0f,
            320.0f,
            AccentColor);
        AddStairs(
            FVector(MarketAwning.X + 250.0f, MarketAwning.Y, 0.0f),
            FVector(MarketAwning.X + 520.0f, MarketAwning.Y, 0.0f),
            190.0f,
            320.0f,
            -5.0f,
            AccentColor);
        for (const float RailOffset : {-150.0f, 150.0f})
        {
            AddSegmentWall(
                FVector(MarketAwning.X - 330.0f, MarketAwning.Y + RailOffset, 0.0f),
                FVector(MarketAwning.X + 330.0f, MarketAwning.Y + RailOffset, 0.0f),
                28.0f,
                86.0f,
                AccentColor,
                270.0f,
                true);
        }
        break;
    }

    case EBMCampaignLevel::StormVault:
    {
        // High storm catwalks: staggered platforms, rail walls, lightning
        // pylons, and a long open drop beside the vault route. The extraction
        // objective is deliberately on a lower receiving apron; the raised
        // vault deck remains a usable optional flank instead of putting the
        // mission target underneath an unreachable collision slab.
        AddRoute(360.0f, GroundColor);
        AddFloor(Level.StartLocation, FVector(1000.0f, 800.0f, 100.0f), GroundColor, -55.0f, 0.0f);
        AddSegmentFloor(Level.InsertionLocation, Level.CheckpointLocation, 300.0f, PrimaryColor, 260.0f);
        AddSegmentFloor(Level.CheckpointLocation, Level.BreachLocation, 240.0f, AccentColor, 420.0f);
        AddSegmentFloor(Level.IntelLocation, Level.OpenCombatLocation, 300.0f, WeatherColor, 260.0f);
        AddFloor(Level.ExtractionLocation, FVector(980.0f, 620.0f, 100.0f), GroundColor, -55.0f, 0.0f);
        // The catwalk route is intentionally narrow between the objectives,
        // but the open-combat pocket needs a broad grounded receiving apron.
        // Without this lower surface, a valid nav route can place the player
        // just outside the edge of the narrow OpenCombat->Extraction deck;
        // once campaign collision is authoritative, that edge becomes a hard
        // stop and can hide the final enemy behind the floor proxy. Keep the
        // raised vault and stairs for optional verticality, while giving the
        // combat pocket a continuous physical floor underneath them.
        AddFloor(
            Level.OpenCombatLocation,
            FVector(1800.0f, 1400.0f, 100.0f),
            GroundColor,
            -55.0f,
            RouteYaw);
        // Keep the optional raised vault well outside the open-combat
        // formation envelope. The earlier central placement overlapped the
        // catwalk stagger spawn lanes, so an enemy could appear inside the
        // side-stair collision and remain unreachable behind its proxy.
        const FVector StormRaisedVaultDeck(-320.0f, -1170.0f, 0.0f);
        AddFloor(StormRaisedVaultDeck, FVector(1000.0f, 700.0f, 100.0f), PrimaryColor, 420.0f, 0.0f);
        AddStairs(Level.InsertionLocation, Level.CheckpointLocation, 300.0f, -5.0f, 310.0f, PrimaryColor);
        AddStairs(Level.CheckpointLocation, Level.BreachLocation, 240.0f, 310.0f, 470.0f, AccentColor);
        AddStairs(Level.IntelLocation, Level.OpenCombatLocation, 300.0f, -5.0f, 310.0f, WeatherColor);
        // The open-combat pocket is elevated above the receiving apron. Keep
        // its descending connection, but move that stair to the outer side
        // lane. A centered 310 cm stair is valid geometry for the catwalk,
        // yet it becomes a solid wall across the lower combat apron where
        // players and the open-combat wave use the ground route.
        // Keep the stair outside the catwalk encounter's +/-360 cm side
        // lanes. At 600 cm the step collision still overlapped the authored
        // wave formation and could hide the last rifleman behind a solid
        // stair slab even though the central ground route was open.
        constexpr float StormLowerRouteStairOffset = 900.0f;
        AddStairs(
            Level.OpenCombatLocation + RouteSide * StormLowerRouteStairOffset,
            Level.ExtractionLocation + RouteSide * StormLowerRouteStairOffset,
            320.0f,
            310.0f,
            -5.0f,
            GroundColor);
        // A side access stair reaches the raised vault without crossing the
        // lower extraction approach or the open-combat formation. Keep the
        // entire stair outside the final-defense spawn ring: the previous
        // near-side entrance sat inside that ring and put a real stair wall
        // between the player and otherwise visible enemies.
        AddStairs(
            Level.ExtractionLocation + RouteSide * 1200.0f,
            StormRaisedVaultDeck + RouteSide * 300.0f,
            240.0f,
            -5.0f,
            470.0f,
            PrimaryColor);
        for (const float Offset : {-230.0f, 230.0f})
        {
            AddWall(Midpoint + RouteSide * Offset + FVector(0.0f, 0.0f, 370.0f), FVector(1800.0f, 35.0f, 400.0f), PrimaryColor, 0.0f, true);
        }
        AddPipe(Level.BreachLocation + FVector(0.0f, 0.0f, 660.0f), FVector(0.30f, 0.30f, 7.8f), AccentColor, 0.0f);
        break;
    }

    case EBMCampaignLevel::RedTide:
    {
        // Chemical seawall: two long levees form a channel with filter towers
        // and one narrow crossing at the breach objective.
        AddRoute(340.0f, GroundColor);
        // The final defense is staged around the extraction boat, outside the
        // narrow channel. Give that receiving side its own grounded apron so
        // the level's seawall formation has a real combat surface for the
        // entire wave instead of projecting outer spawns onto the top of a
        // nearby vertical structure.
        AddFloor(Level.ExtractionLocation, FVector(1500.0f, 1500.0f, 100.0f), GroundColor, -55.0f, 0.0f);
        // Keep both levees outside the authored route and the zig-zag
        // approach to the Intel console.  At 460 cm the far-side levee sat
        // directly under the navigation probe's valid approach pocket: the
        // player could arrive on the wall's collision edge, but could not
        // turn through it toward Intel.  The wider 760 cm banks preserve the
        // seawall silhouette and cover while leaving a genuinely traversable
        // central channel and side recovery lanes.
        constexpr float RedTideBankOffset = 760.0f;
        AddSegmentWall(
            Level.CheckpointLocation + RouteSide * RedTideBankOffset,
            Level.BreachLocation + RouteSide * RedTideBankOffset,
            90.0f,
            250.0f,
            PrimaryColor,
            0.0f,
            false);
        AddSegmentWall(
            Level.IntelLocation + RouteSide * RedTideBankOffset,
            Level.OpenCombatLocation + RouteSide * RedTideBankOffset,
            90.0f,
            210.0f,
            PrimaryColor,
            0.0f,
            false);
        AddSegmentWall(
            Level.CheckpointLocation - RouteSide * RedTideBankOffset,
            Level.BreachLocation - RouteSide * RedTideBankOffset,
            90.0f,
            250.0f,
            PrimaryColor,
            0.0f,
            false);
        // The filter channel is a broad, ground-level chemical spillway.  A
        // raised runtime deck here created a second nav surface whose short
        // stair approximation could funnel the pawn onto a collision edge
        // during the Breach->Intel turn.  Keep the channel as real collision
        // geometry, but make its walk surface continuous with the shared
        // route so every capsule has a stable crossing and recovery lane.
        AddSegmentFloor(Level.BreachLocation, Level.IntelLocation, 820.0f, WeatherColor, -55.0f);
        for (int32 FilterIndex = 0; FilterIndex < 4; ++FilterIndex)
        {
            AddPipe(Level.CheckpointLocation + FVector(-420.0f + 280.0f * static_cast<float>(FilterIndex), 0.0f, 230.0f), FVector(0.75f, 0.75f, 2.4f), AccentColor, 0.0f);
        }
        break;
    }

    case EBMCampaignLevel::IronCathedral:
        // Turbine nave: a tall interior hall with six turbine pillars, side
        // aisles, a high roof, and a central processional floor.
        AddRoom(Midpoint, 3000.0f, 1700.0f, 980.0f, GroundColor, true, 1u | 4u, 1400.0f, -300.0f, 0.0f, 1400.0f, -300.0f);
        AddRoute(520.0f, PrimaryColor);
        for (int32 PillarIndex = 0; PillarIndex < 6; ++PillarIndex)
        {
            const float X = Midpoint.X - 1050.0f + 420.0f * static_cast<float>(PillarIndex);
            AddPipe(FVector(X, Midpoint.Y - 620.0f, 440.0f), FVector(0.75f, 0.75f, 8.8f), AccentColor, 0.0f);
            AddPipe(FVector(X, Midpoint.Y + 620.0f, 440.0f), FVector(0.75f, 0.75f, 8.8f), AccentColor, 0.0f);
        }
        // The nave needs low, grounded cover between the tall turbines.  Use
        // alternating console blocks and short shrine walls so the player can
        // break the long aisle sightline without turning the room into a maze.
        for (int32 CoverIndex = 0; CoverIndex < 5; ++CoverIndex)
        {
            const float Along = -440.0f + static_cast<float>(CoverIndex) * 260.0f;
            AddCover(
                RoutePoint(Level.BreachLocation, Along, CoverIndex % 2 == 0 ? -300.0f : 300.0f)
                    + FVector(0.0f, 0.0f, 78.0f),
                FVector(230.0f, 120.0f, 156.0f),
                CoverIndex % 2 == 0 ? GroundColor : AccentColor,
                RouteYaw + (CoverIndex % 2 == 0 ? -8.0f : 10.0f));
            AddMachinery(
                RoutePoint(Level.BreachLocation, Along + 65.0f, CoverIndex % 2 == 0 ? 420.0f : -420.0f)
                    + FVector(0.0f, 0.0f, 74.0f),
                FVector(0.74f),
                PrimaryColor,
                RouteYaw);
        }
        AddSegmentWall(
            Level.CheckpointLocation + RouteSide * 720.0f,
            Level.BreachLocation + RouteSide * 720.0f,
            70.0f,
            150.0f,
            AccentColor,
            0.0f,
            true);
        AddSegmentWall(
            Level.IntelLocation - RouteSide * 720.0f,
            Level.OpenCombatLocation - RouteSide * 720.0f,
            70.0f,
            150.0f,
            AccentColor,
            0.0f,
            true);
        AddPanel(Midpoint + FVector(0.0f, 0.0f, 720.0f), FVector(6.4f, 0.18f, 1.4f), AccentColor, RouteYaw);
        break;

    case EBMCampaignLevel::LanternBasin:
        // Forest basin: winding ground patches, a stream crossing, tree
        // trunks, and a broken canopy replace industrial walls.
        AddRoute(500.0f, GroundColor);
        AddFloor(Midpoint + RouteSide * 470.0f, FVector(900.0f, 360.0f, 80.0f), WeatherColor, -20.0f, 0.0f);
        for (int32 TreeIndex = 0; TreeIndex < 10; ++TreeIndex)
        {
            const float Alpha = 0.08f + 0.085f * static_cast<float>(TreeIndex);
            const float SideOffset = (TreeIndex % 2 == 0 ? -1.0f : 1.0f) * (430.0f + 70.0f * static_cast<float>(TreeIndex % 3));
            const FVector TreePoint = FMath::Lerp(Level.StartLocation, Level.OpenCombatLocation, Alpha) + RouteSide * SideOffset;
            AddPipe(TreePoint + FVector(0.0f, 0.0f, 210.0f), FVector(0.34f, 0.34f, 4.2f), PrimaryColor, 0.0f);
            AddPipe(TreePoint + FVector(0.0f, 0.0f, 470.0f), FVector(1.0f, 1.0f, 0.65f), WeatherColor, 0.0f);
        }
        break;

    case EBMCampaignLevel::QuarryNine:
    {
        // Desert signal canyon: broken, route-aligned faces create an actual
        // canyon without the two fixed-axis walls that previously filled the
        // camera with one giant overhead/foreground slab.
        AddTerrainBed(
            FMath::Lerp(Level.StartLocation, Level.ExtractionLocation, 0.5f),
            FVector(3800.0f, 2500.0f, 80.0f),
            GroundColor,
            -50.0f,
            RouteYaw);
        AddRoute(460.0f, GroundColor);
        AddFloor(Level.BreachLocation, FVector(900.0f, 620.0f, 100.0f), WeatherColor, -55.0f, RouteYaw);
        AddFloor(Level.OpenCombatLocation, FVector(1100.0f, 860.0f, 100.0f), GroundColor, -55.0f, RouteYaw);
        AddFloor(Level.ExtractionLocation, FVector(900.0f, 650.0f, 100.0f), AccentColor, -55.0f, RouteYaw);

        const auto AddQuarryFace = [&AddSegmentWall, &RouteSide](
            const FVector& Start,
            const FVector& End,
            const float Across,
            const float Height,
            const FColor& Color)
        {
            AddSegmentWall(
                Start + RouteSide * Across,
                End + RouteSide * Across,
                120.0f,
                Height,
                Color,
                0.0f,
                false);
        };

        // Each face is segmented on purpose. The gaps create cross-lane
        // sightlines and flank openings, while the unequal heights sell a
        // stepped quarry rather than a symmetric corridor.
        AddQuarryFace(Level.StartLocation, Level.CheckpointLocation, 610.0f, 320.0f, PrimaryColor);
        AddQuarryFace(Level.CheckpointLocation, Level.BreachLocation, 650.0f, 390.0f, PrimaryColor);
        AddQuarryFace(Level.IntelLocation, Level.OpenCombatLocation, 570.0f, 280.0f, PrimaryColor);
        // Keep the negative-side faces as real canyon walls, but outside the
        // opening encounter envelope.  At -610 cm the insertion-to-breach
        // face crossed the exact lane used by the forward opening wave: an
        // enemy could be pushed behind that wall while the player capsule
        // could not sweep around its full-length proxy.  The wider outer
        // shelf preserves the canyon silhouette and leaves a continuous
        // ground-level combat lane between the two faces.
        AddQuarryFace(Level.InsertionLocation, Level.BreachLocation, -900.0f, 250.0f, GroundColor);
        AddQuarryFace(Level.BreachLocation, Level.IntelLocation, -860.0f, 330.0f, GroundColor);
        AddQuarryFace(Level.OpenCombatLocation, Level.ExtractionLocation, -820.0f, 230.0f, GroundColor);

        // A quarry loading shelf creates optional verticality on the far side
        // of the open-combat pocket. Keep the complete branch outside the
        // authored +/-680 cm encounter radius: the old return stair ran from
        // the shelf back toward Extraction and became a long solid deck
        // through the wave's sightlines, trapping the last enemies behind
        // collision even though the player was on valid ground.
        constexpr float QuarryOptionalSideOffset = 1000.0f;
        const FVector QuarryShelf = RoutePoint(Level.OpenCombatLocation, 80.0f, QuarryOptionalSideOffset);
        const FVector QuarryShelfEntry = RoutePoint(Level.OpenCombatLocation, -260.0f, QuarryOptionalSideOffset);
        const FVector QuarryShelfLowerLanding = RoutePoint(Level.OpenCombatLocation, 420.0f, 1200.0f);
        AddFloor(QuarryShelf, FVector(760.0f, 500.0f, 100.0f), PrimaryColor, 230.0f, RouteYaw);
        AddStairs(QuarryShelfEntry, QuarryShelf, 220.0f, -5.0f, 270.0f, AccentColor);
        AddStairs(QuarryShelf + RouteDirection * 220.0f, QuarryShelfLowerLanding, 200.0f, 270.0f, -5.0f, WeatherColor);
        AddSegmentWall(
            QuarryShelf - RouteDirection * 280.0f + RouteSide * 190.0f,
            QuarryShelf + RouteDirection * 180.0f + RouteSide * 190.0f,
            70.0f,
            130.0f,
            AccentColor,
            230.0f,
            true);

        // Low drill barricades create readable close cover at the breach and
        // combat pocket while preserving the central descent.
        AddWall(
            RoutePoint(Level.BreachLocation, -130.0f, 300.0f) + FVector(0.0f, 0.0f, 74.0f),
            FVector(250.0f, 78.0f, 148.0f),
            AccentColor,
            RouteYaw + 14.0f,
            true);
        AddWall(
            RoutePoint(Level.BreachLocation, 110.0f, -300.0f) + FVector(0.0f, 0.0f, 74.0f),
            FVector(220.0f, 78.0f, 148.0f),
            WeatherColor,
            RouteYaw - 18.0f,
            true);
        AddPipe(RoutePoint(Level.CheckpointLocation, 120.0f, 430.0f) + FVector(0.0f, 0.0f, 165.0f), FVector(0.42f, 0.42f, 2.8f), AccentColor, RouteYaw);
        AddPipe(RoutePoint(Level.OpenCombatLocation, 180.0f, -470.0f) + FVector(0.0f, 0.0f, 150.0f), FVector(0.36f, 0.36f, 2.4f), WeatherColor, RouteYaw);
        break;
    }

    case EBMCampaignLevel::VantaSpire:
    {
        // Spire service ridge: a central tower, switchback service slabs,
        // tall antenna legs, and a high final platform dominate the skyline.
        AddRoute(420.0f, GroundColor);
        const FVector SpireMidPlatform = Level.CheckpointLocation + RouteSide * 520.0f;
        const FVector SpireFinalPlatform = Level.ExtractionLocation + RouteSide * 460.0f;
        AddFloor(SpireMidPlatform, FVector(850.0f, 850.0f, 100.0f), PrimaryColor, 280.0f, 0.0f);
        AddFloor(SpireFinalPlatform, FVector(900.0f, 800.0f, 100.0f), AccentColor, 430.0f, 0.0f);
        AddStairs(Level.CheckpointLocation + RouteSide * 210.0f, SpireMidPlatform, 260.0f, -5.0f, 330.0f, PrimaryColor);
        AddStairs(SpireMidPlatform, SpireFinalPlatform, 220.0f, 330.0f, 480.0f, AccentColor);
        AddStairs(SpireFinalPlatform, Level.ExtractionLocation + RouteSide * 140.0f, 240.0f, 480.0f, -5.0f, AccentColor);
        AddPipe(SpireMidPlatform + FVector(0.0f, 0.0f, 640.0f), FVector(2.1f, 2.1f, 7.5f), PrimaryColor, 0.0f);
        AddPipe(SpireMidPlatform + FVector(0.0f, 0.0f, 1170.0f), FVector(0.8f, 0.8f, 1.0f), AccentColor, 0.0f);
        const FVector VantaCatwalkStart = Level.BreachLocation + RouteSide * 260.0f;
        const FVector VantaCatwalkEnd = Level.IntelLocation + RouteSide * 260.0f;
        AddSegmentFloor(VantaCatwalkStart, VantaCatwalkEnd, 220.0f, WeatherColor, 260.0f);
        AddStairs(Level.BreachLocation, Level.BreachLocation + RouteSide * 260.0f, 220.0f, -5.0f, 310.0f, WeatherColor);
        AddStairs(Level.IntelLocation, Level.IntelLocation + RouteSide * 260.0f, 220.0f, -5.0f, 310.0f, WeatherColor);
        for (const float RailSide : {-115.0f, 115.0f})
        {
            AddSegmentWall(
                VantaCatwalkStart + RouteSide * RailSide,
                VantaCatwalkEnd + RouteSide * RailSide,
                24.0f,
                82.0f,
                AccentColor,
                260.0f,
                true);
        }
        for (const float PlatformSide : {-360.0f, 360.0f})
        {
            AddSegmentWall(
                SpireMidPlatform - RouteDirection * 330.0f + RouteSide * PlatformSide,
                SpireMidPlatform + RouteDirection * 330.0f + RouteSide * PlatformSide,
                28.0f,
                96.0f,
                AccentColor,
                280.0f,
                true);
            AddSegmentWall(
                SpireFinalPlatform - RouteDirection * 330.0f + RouteSide * PlatformSide,
                SpireFinalPlatform + RouteDirection * 330.0f + RouteSide * PlatformSide,
                28.0f,
                96.0f,
                AccentColor,
                430.0f,
                true);
        }
        break;
    }

    case EBMCampaignLevel::Wreckline:
        // Container wreckline: alternating hull blocks form a maze with
        // sightline breaks and two traversable alleys to extraction.
        AddRoute(420.0f, GroundColor);
        for (int32 HullIndex = 0; HullIndex < 8; ++HullIndex)
        {
            const float X = -950.0f + 520.0f * static_cast<float>(HullIndex);
            // Keep the open-combat receiving apron clear of collision hulls.
            // The old alternating row put the x=1130 and x=2170 hulls across
            // the combat pocket; a wave member could spawn inside that
            // container footprint and remain visible only through its muzzle.
            // Move those two hulls to the positive outer service lane and lift
            // the middle upper hull to the far edge. The final-defense wave
            // uses the negative receiving lane; leaving hull 6 at -1320 cm
            // put its real collision directly between the player and the
            // last target. The maze still has its alternating lanes, while
            // the combat pocket remains a real, traversable clearing rather
            // than an accidental dead end.
            float Y = HullIndex % 2 == 0 ? -480.0f : 470.0f;
            if (HullIndex == 4 || HullIndex == 6)
            {
                Y = 1320.0f;
            }
            else if (HullIndex == 5)
            {
                Y = 900.0f;
            }
            AddWall(FVector(X, Y, 150.0f), FVector(420.0f, 520.0f, 300.0f), PrimaryColor, HullIndex % 3 == 0 ? 8.0f : -5.0f, false);
            AddPanel(FVector(X, Y + (Y < 0.0f ? 270.0f : -270.0f), 230.0f), FVector(3.8f, 0.18f, 3.8f), WeatherColor, 0.0f);
        }
        // The receiving apron needs one readable flank obstacle, not a
        // collision wreck directly on the primary spawn/sightline lane.
        AddWreckage(Level.OpenCombatLocation + RouteSide * 720.0f + FVector(0.0f, 0.0f, 30.0f), FVector(1.20f, 0.90f, 1.0f), AccentColor, 18.0f);
        AddFloor(Level.OpenCombatLocation, FVector(1700.0f, 1200.0f, 100.0f), AccentColor, -55.0f, 0.0f);
        break;

    case EBMCampaignLevel::EmberStation:
        // Fuel station: forecourt, canopy, pump islands, and cylindrical
        // tanks create a recognizable service station. The extraction side is
        // a separate receiving forecourt rather than empty space beyond the
        // original service slab; it gives the final-defense formation a real
        // lower deck and keeps the tank roofs decorative, not spawn surfaces.
        AddRoute(500.0f, GroundColor);
        AddFloor(Midpoint, FVector(2800.0f, 1500.0f, 100.0f), GroundColor, -55.0f, 0.0f);
        AddFloor(Level.ExtractionLocation, FVector(1500.0f, 1100.0f, 100.0f), GroundColor, -55.0f, 0.0f);
        AddFloor(Level.CheckpointLocation, FVector(1050.0f, 760.0f, 100.0f), PrimaryColor, -45.0f, 0.0f);
        // The open-combat pocket sits beyond the midpoint apron. Give the
        // entire fuel-bay formation a grounded receiving deck so a wave
        // member cannot land on the top of a tall service piece when the
        // grounding trace misses the old apron edge.
        AddFloor(Level.OpenCombatLocation, FVector(1700.0f, 1100.0f, 100.0f), GroundColor, -55.0f, 0.0f);
        // The canopy is split into two service bays with a real central
        // opening.  The old single wall read as a blocking slab and erased
        // the fuel-stop approach from the combat camera.
        AddSegmentWall(
            Midpoint - RouteDirection * 880.0f + RouteSide * 430.0f,
            Midpoint - RouteDirection * 100.0f + RouteSide * 430.0f,
            72.0f,
            520.0f,
            PrimaryColor,
            0.0f,
            false);
        AddSegmentWall(
            Midpoint + RouteDirection * 100.0f + RouteSide * 430.0f,
            Midpoint + RouteDirection * 880.0f + RouteSide * 430.0f,
            72.0f,
            520.0f,
            PrimaryColor,
            0.0f,
            false);
        for (const float PumpOffset : {-360.0f, 0.0f, 360.0f})
        {
            AddPipe(
                RoutePoint(Level.BreachLocation, PumpOffset, 250.0f) + FVector(0.0f, 0.0f, 120.0f),
                FVector(1.18f, 1.18f, 1.05f),
                WeatherColor,
                RouteYaw);
            AddPanel(
                RoutePoint(Level.BreachLocation, PumpOffset + 70.0f, 250.0f) + FVector(0.0f, 0.0f, 260.0f),
                FVector(1.20f, 0.16f, 1.35f),
                AccentColor,
                RouteYaw + 90.0f);
        }
        for (int32 TankIndex = 0; TankIndex < 3; ++TankIndex)
        {
            AddPipe(RoutePoint(Level.BreachLocation, -360.0f + 360.0f * static_cast<float>(TankIndex), 340.0f) + FVector(0.0f, 0.0f, 115.0f), FVector(1.55f, 1.55f, 2.1f), WeatherColor, 0.0f);
        }
        AddPipe(Level.IntelLocation + FVector(0.0f, -280.0f, 180.0f), FVector(0.20f, 0.20f, 3.6f), AccentColor, 90.0f);
        AddCover(RoutePoint(Level.CheckpointLocation, 100.0f, -340.0f) + FVector(0.0f, 0.0f, 78.0f), FVector(250.0f, 120.0f, 156.0f), AccentColor, RouteYaw - 10.0f);
        // Keep the combat pocket's cover on the negative flank. The previous
        // positive-side placement sat directly between the player and the
        // receiving wave, so every eye-height visibility sample hit the cover
        // before it could reach an enemy in the fuel bay. It remains useful
        // close cover, but no longer seals the primary approach lane.
        AddCover(RoutePoint(Level.OpenCombatLocation, -120.0f, -380.0f) + FVector(0.0f, 0.0f, 60.0f), FVector(230.0f, 105.0f, 120.0f), WeatherColor, RouteYaw + 12.0f);
        AddSegmentWall(
            // The receiving wave is allowed to occupy the full encounter
            // radius, including the positive flank. Push this service barrier
            // beyond that radius so it still defines the station edge without
            // becoming a wall between the player and a valid enemy target.
            Level.OpenCombatLocation + RouteSide * 820.0f,
            Level.ExtractionLocation + RouteSide * 820.0f,
            80.0f,
            170.0f,
            AccentColor,
            0.0f,
            true);
        break;

    case EBMCampaignLevel::PaleHarbor:
    {
        // Ice harbor: separated berths, snow berms, crane uprights, and
        // narrow crossings create a broken frozen shoreline.
        AddTerrainBed(Midpoint, FVector(4000.0f, 3000.0f, 80.0f), GroundColor, -50.0f, RouteYaw);
        AddRoute(380.0f, GroundColor);
        AddFloor(Level.StartLocation, FVector(900.0f, 650.0f, 100.0f), GroundColor, -55.0f, 0.0f);
        const FVector IceCheckpointBerth = Level.CheckpointLocation + RouteSide * 520.0f;
        const FVector IceCombatBerth = Level.OpenCombatLocation + RouteSide * 480.0f;
        AddFloor(IceCheckpointBerth, FVector(1000.0f, 480.0f, 100.0f), WeatherColor, 220.0f, 0.0f);
        AddFloor(Level.OpenCombatLocation, FVector(1200.0f, 560.0f, 100.0f), GroundColor, -55.0f, 0.0f);
        AddFloor(IceCombatBerth, FVector(950.0f, 520.0f, 100.0f), WeatherColor, 360.0f, 0.0f);
        AddStairs(
            Level.CheckpointLocation + RouteSide * 220.0f,
            IceCheckpointBerth,
            240.0f,
            -5.0f,
            270.0f,
            AccentColor);
        AddStairs(
            IceCheckpointBerth,
            IceCombatBerth,
            220.0f,
            270.0f,
            410.0f,
            WeatherColor);
        AddStairs(
            IceCombatBerth,
            Level.OpenCombatLocation + RouteSide * 180.0f,
            240.0f,
            410.0f,
            -5.0f,
            WeatherColor);
        AddStairs(
            Level.IntelLocation,
            Level.IntelLocation + RouteSide * 220.0f,
            220.0f,
            -5.0f,
            270.0f,
            WeatherColor);
        // Final defense sits on the lower receiving berth, not in the open
        // center of the ice. These collision-bearing berms create two
        // readable peek/retreat pockets while leaving the extraction face
        // clear.
        AddWall(
            Level.ExtractionLocation + RouteSide * 330.0f + FVector(0.0f, 0.0f, 90.0f),
            FVector(560.0f, 90.0f, 180.0f),
            PrimaryColor,
            RouteYaw,
            true);
        AddWall(
            // Keep the second berth wall on the outer negative flank.  The
            // final-defense traversal arrives from that side of the harbor;
            // at -300 cm the wall sat across the only ground-level approach
            // to the boat and turned a valid extraction into a collision
            // dead-end.  The positive-side wall and the lower rail still
            // define the berth, while this offset leaves a real opening to
            // the objective and a usable retreat lane around the ice cover.
            Level.ExtractionLocation - RouteSide * 1280.0f + FVector(0.0f, 0.0f, 90.0f),
            FVector(440.0f, 90.0f, 180.0f),
            WeatherColor,
            RouteYaw,
            true);
        for (const float Offset : {-420.0f, 420.0f})
        {
            AddWall(Midpoint + RouteSide * Offset + FVector(0.0f, 0.0f, 105.0f), FVector(1200.0f, 170.0f, 210.0f), WeatherColor, 12.0f, true);
            AddPipe(Midpoint + RouteSide * Offset + FVector(0.0f, 0.0f, 520.0f), FVector(0.30f, 0.30f, 6.5f), PrimaryColor, 0.0f);
        }
        // Dark berth markers and a grounded ship-hull fragment restore depth
        // against the whiteout.  The lower rail follows the route and leaves
        // the elevated crossings, stairs, and extraction face open.
        AddSegmentWall(
            // This lower berth rail is a real collision wall, not dressing.
            // The -720 cm version overlapped the final-defense reposition
            // lane and caught the player capsule while it was moving around
            // the receiving wave. Keep the ice-berth edge and its sightline
            // break, but move it to the outer shelf so the combat pocket and
            // extraction apron retain a continuous ground route.
            Level.OpenCombatLocation - RouteSide * 1000.0f,
            Level.ExtractionLocation - RouteSide * 1000.0f,
            72.0f,
            170.0f,
            PrimaryColor,
            0.0f,
            true);
        AddWreckage(
            Level.ExtractionLocation + RouteSide * 500.0f + FVector(0.0f, 0.0f, 34.0f),
            FVector(1.45f, 0.92f, 1.02f),
            PrimaryColor,
            RouteYaw + 18.0f);
        AddPanel(
            IceCheckpointBerth + RouteSide * 180.0f + FVector(0.0f, 0.0f, 330.0f),
            FVector(2.8f, 0.16f, 1.65f),
            AccentColor,
            RouteYaw + 90.0f);
        AddGrate(
            Level.OpenCombatLocation + RouteSide * 220.0f + FVector(0.0f, 0.0f, 8.0f),
            FVector(2.8f, 0.74f, 0.08f),
            WeatherColor,
            RouteYaw);
        break;
    }

    case EBMCampaignLevel::MeridianGate:
        // Checkpoint gate: a road between two gate towers, inspection bays,
        // overhead lattice, and a hard final barrier define the operation.
        AddRoute(620.0f, GroundColor);
        AddWall(Level.CheckpointLocation + RouteSide * 380.0f + FVector(0.0f, 0.0f, 260.0f), FVector(650.0f, 260.0f, 520.0f), PrimaryColor, 0.0f, false);
        // Keep the negative-side tower as a real outer gate structure, but
        // move it beyond the negative checkpoint lane. At the original
        // offset its collision envelope sat directly in front of the
        // negative-side checkpoint formation: the enemies were visible just
        // beyond the wall yet every movement and aim trace still hit it.
        // The wider offset preserves the tower pair and central road while
        // leaving a usable sightline and bypass around the outer tower.
        AddWall(Level.CheckpointLocation - RouteSide * 820.0f + FVector(0.0f, 0.0f, 260.0f), FVector(650.0f, 260.0f, 520.0f), PrimaryColor, 0.0f, false);
        AddFloor(Level.BreachLocation, FVector(850.0f, 900.0f, 100.0f), AccentColor, -55.0f, 0.0f);
        AddPanel(Level.CheckpointLocation + FVector(0.0f, 0.0f, 590.0f), FVector(5.5f, 1.0f, 0.45f), AccentColor, 0.0f);
        AddPipe(Level.InsertionLocation + RouteSide * 300.0f + FVector(0.0f, 0.0f, 280.0f), FVector(0.38f, 0.38f, 5.6f), WeatherColor, 0.0f);
        break;

    case EBMCampaignLevel::LastLight:
        // Final ridge: open golden approach, cliff-edge slabs, final pier,
        // and a tall last-light signal stand apart from enclosed operations.
        AddTerrainBed(Midpoint, FVector(4000.0f, 3000.0f, 80.0f), GroundColor, -50.0f, RouteYaw);
        AddRoute(700.0f, GroundColor);
        AddWall(Level.OpenCombatLocation + RouteSide * 520.0f + FVector(0.0f, 0.0f, 115.0f), FVector(1300.0f, 140.0f, 230.0f), WeatherColor, -10.0f, true);
        AddWall(Level.ExtractionLocation - RouteSide * 500.0f + FVector(0.0f, 0.0f, 170.0f), FVector(900.0f, 180.0f, 340.0f), PrimaryColor, 14.0f, true);
        AddWreckage(Level.OpenCombatLocation - RouteSide * 340.0f + FVector(0.0f, 0.0f, 30.0f), FVector(1.12f, 0.88f, 1.0f), WeatherColor, 22.0f);
        AddFloor(Level.ExtractionLocation, FVector(1200.0f, 460.0f, 100.0f), AccentColor, -35.0f, 0.0f);
        AddSegmentWall(
            Level.OpenCombatLocation + RouteSide * 680.0f,
            Level.ExtractionLocation + RouteSide * 680.0f,
            78.0f,
            170.0f,
            PrimaryColor,
            0.0f,
            true);
        AddCover(
            RoutePoint(Level.OpenCombatLocation, 100.0f, -320.0f) + FVector(0.0f, 0.0f, 78.0f),
            FVector(260.0f, 120.0f, 156.0f),
            GroundColor,
            RouteYaw - 12.0f);
        AddCover(
            RoutePoint(Level.ExtractionLocation, -110.0f, 300.0f) + FVector(0.0f, 0.0f, 78.0f),
            FVector(230.0f, 120.0f, 156.0f),
            PrimaryColor,
            RouteYaw + 16.0f);
        AddTruss(
            Level.ExtractionLocation + FVector(0.0f, 0.0f, 520.0f),
            FVector(4.2f, 0.72f, 0.24f),
            AccentColor,
            RouteYaw);
        AddPipe(Level.ExtractionLocation + FVector(0.0f, 0.0f, 350.0f), FVector(0.42f, 0.42f, 7.0f), AccentColor, 0.0f);
        break;

    default:
        AddRoute(600.0f, GroundColor);
        break;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian campaign layout: level=%s layout=%s structural_pieces=%d wreckage_pieces=%d shell_replaced=yes"),
        *Level.LevelId.ToString(),
        *Level.EnvironmentLayoutId.ToString(),
        CampaignLayoutPieces.Num(),
        WreckageCount);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian campaign hero landmark: level=%s type=%s pieces=%d collision=authoritative alternate_route=stairs"),
        *Level.LevelId.ToString(),
        Level.Level == EBMCampaignLevel::SableQuay ? TEXT("grounded_patrol_boat_relay_tender") : TEXT("none"),
        SableHeroPieceCount);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian campaign boundary: level=%s visible_landmarks=%d recovery_colliders=4 action=block_fall"),
        *Level.LevelId.ToString(),
        BoundaryLandmarkCount);
}

// Campaign structural layout is authoritative gameplay space, not decorative dressing.
void ABMGreyboxDirector::ApplyAuthoredShellPresentation()
{
    if (!GetWorld())
    {
        return;
    }

    UMaterialInterface* BaseMaterial = LoadPresentationMaterial();
    if (!BaseMaterial)
    {
        UE_LOG(LogTemp, Warning, TEXT("Black Meridian authored shell: presentation material unavailable"));
        return;
    }

    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
    const FLinearColor ThemePrimary = FLinearColor::FromSRGBColor(Level.EnvironmentPrimaryColor);
    const FLinearColor ThemeGround = FLinearColor::FromSRGBColor(Level.EnvironmentGroundColor);
    const FLinearColor ThemeAccent = FLinearColor::FromSRGBColor(Level.EnvironmentAccentColor);
    const auto BlendTheme = [](const FLinearColor& Base, const FLinearColor& Theme, const float Alpha)
    {
        return FLinearColor(
            FMath::Lerp(Base.R, Theme.R, Alpha),
            FMath::Lerp(Base.G, Theme.G, Alpha),
            FMath::Lerp(Base.B, Theme.B, Alpha),
            1.0f);
    };

    int32 AppliedActors = 0;
    int32 AppliedMaterialSlots = 0;
    const bool bCampaignShell = !GetWorld()->GetMapName().Contains(TEXT("SignalControl"));

    for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
    {
        AStaticMeshActor* Actor = *It;
        UStaticMeshComponent* MeshComponent = Actor ? Actor->GetStaticMeshComponent() : nullptr;
        if (!Actor || !MeshComponent || !MeshComponent->GetStaticMesh())
        {
            continue;
        }

        // Labels are authored by generate_authored_maps.py. Include the object
        // name as a fallback because cooked builds can omit editor-only label
        // metadata for actors that were renamed by the level pipeline.
        FString ActorIdentity = Actor->GetName();
        // Persisted map tags survive cooking and carry the semantic role that
        // editor-only labels do not reliably expose in a standalone build.
        for (const FName& Tag : Actor->Tags)
        {
            ActorIdentity += TEXT(" ");
            ActorIdentity += Tag.ToString();
        }
        const auto Has = [&ActorIdentity](const TCHAR* Token)
        {
            return ActorIdentity.Contains(Token, ESearchCase::IgnoreCase);
        };

        FLinearColor Color(0.14f, 0.18f, 0.21f, 1.0f);
        float Metallic = 0.34f;
        float Roughness = 0.78f;
        bool bLandmark = false;
        bool bCover = false;

        // Vertical signals and route beacons carry the mission's cyan/amber/
        // red language. Resolve these before broad wall/floor terms so labels
        // such as SeawallBeacon do not get treated as ordinary wall steel.
        if (Has(TEXT("Beacon")) || Has(TEXT("Signal")) || Has(TEXT("Mast")) || Has(TEXT("Lamp")) || Has(TEXT("Node_")))
        {
            bLandmark = true;
            if (Has(TEXT("Node_3")) || Has(TEXT("Seawall")))
            {
                Color = BlendTheme(FLinearColor(0.78f, 0.075f, 0.045f, 1.0f), ThemeAccent, 0.48f);
            }
            else if (Has(TEXT("Node_2")) || Has(TEXT("Breach")) || Has(TEXT("Extraction")) || Has(TEXT("Relay")))
            {
                Color = BlendTheme(FLinearColor(0.92f, 0.34f, 0.055f, 1.0f), ThemeAccent, 0.56f);
            }
            else
            {
                Color = BlendTheme(FLinearColor(0.025f, 0.58f, 0.60f, 1.0f), ThemePrimary, 0.42f);
            }
            Metallic = 0.52f;
            Roughness = 0.38f;
        }
        else if (Has(TEXT("Cover")) || Has(TEXT("Barricade")) || Has(TEXT("Tank")))
        {
            bCover = true;
            // Weathered brown/olive cover separates gameplay silhouettes from
            // the cool steel shell while remaining subdued under night light.
            Color = BlendTheme(FLinearColor(0.30f, 0.25f, 0.15f, 1.0f), ThemePrimary, 0.38f);
            Metallic = 0.18f;
            Roughness = 0.86f;
        }
        else if (Has(TEXT("Floor")))
        {
            Color = BlendTheme(FLinearColor(0.075f, 0.105f, 0.14f, 1.0f), ThemeGround, 0.62f);
            // Terrain is a first-class part of the selected operation. Wet
            // floodgate steel, salt flats, forest soil, ice, and interior
            // decking therefore differ in response as well as hue.
            Metallic = Level.EnvironmentTerrainMetallic;
            Roughness = Level.EnvironmentTerrainRoughness;
        }
        else if (Has(TEXT("Wall")) || Has(TEXT("Boundary")) || Has(TEXT("Roof")) || Has(TEXT("Seawall")) || Has(TEXT("Pier")) || Has(TEXT("Checkpoint")))
        {
            Color = BlendTheme(FLinearColor(0.16f, 0.20f, 0.23f, 1.0f), ThemePrimary, 0.46f);
            Metallic = 0.42f;
            Roughness = 0.76f;
        }
        else if (Has(TEXT("Pad")))
        {
            Color = BlendTheme(FLinearColor(0.10f, 0.13f, 0.16f, 1.0f), ThemeAccent, 0.30f);
            Metallic = 0.48f;
            Roughness = 0.70f;
        }

        // This legacy presentation path is retained for the authored-shell
        // compatibility arena. Campaign operations use SpawnCampaignLayout()
        // for collision-bearing floors, walls, elevation, and cover; only the
        // non-campaign shell presentation reaches this branch now.
        if (bCampaignShell && !bLandmark && !bCover
            && (Has(TEXT("Wall")) || Has(TEXT("Boundary")) || Has(TEXT("Roof")) || Has(TEXT("Seawall")) || Has(TEXT("Pier")) || Has(TEXT("Checkpoint"))))
        {
            Actor->SetActorHiddenInGame(true);
        }

        const int32 MaterialCount = FMath::Max(1, MeshComponent->GetNumMaterials());
        bool bAppliedActor = false;
        for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
        {
            UMaterialInstanceDynamic* Material = MeshComponent->CreateAndSetMaterialInstanceDynamicFromMaterial(MaterialIndex, BaseMaterial);
            if (!Material)
            {
                continue;
            }

            // The authored shell is lit at runtime on the Mac compatibility
            // path.  Its source palette is intentionally restrained, but
            // feeding those values directly into BasicShapeMaterial makes
            // the shadow-facing walls collapse to black and hides cover
            // silhouettes during a real firefight.  Lift the readable value
            // range while preserving the authored hue and the bright beacon
            // accents.
            const FLinearColor ReadableColor(
                FMath::Clamp(FMath::Max(Color.R * 1.42f, 0.055f), 0.0f, 0.92f),
                FMath::Clamp(FMath::Max(Color.G * 1.42f, 0.055f), 0.0f, 0.92f),
                FMath::Clamp(FMath::Max(Color.B * 1.42f, 0.055f), 0.0f, 0.92f),
                Color.A);
            Material->SetVectorParameterValue(TEXT("Color"), ReadableColor);
            Material->SetVectorParameterValue(TEXT("BaseColor"), ReadableColor);
            Material->SetVectorParameterValue(
                TEXT("EmissiveColor"),
                ReadableColor * (bLandmark ? 0.18f : (bCover ? 0.035f : 0.022f)));
            Material->SetScalarParameterValue(TEXT("EmissiveStrength"), bLandmark ? 1.15f : 0.75f);
            Material->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
            Material->SetScalarParameterValue(TEXT("Metallic"), Metallic);
            Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
            bAppliedActor = true;
            ++AppliedMaterialSlots;
        }

        if (bAppliedActor)
        {
            Actor->Tags.AddUnique(TEXT("ArtDirected.AuthoredShell"));
            if (bLandmark)
            {
                Actor->Tags.AddUnique(TEXT("ArtDirected.Landmark"));
            }
            else if (bCover)
            {
                Actor->Tags.AddUnique(TEXT("ArtDirected.Cover"));
            }
            ++AppliedActors;
        }
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian authored shell: presentation applied actors=%d material_slots=%d"),
        AppliedActors,
        AppliedMaterialSlots);
}

void ABMGreyboxDirector::SpawnAuthoredShellDressing()
{
    if (!GetWorld())
    {
        return;
    }

    // The saved shell owns all gameplay collision. These pieces are a small,
    // deterministic visual kit only: seams, frames, cable trays, and safety
    // plates give the route scale and material rhythm without changing any
    // movement, cover, or interaction trace.
    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
    const auto BlendPalette = [](const FColor& Base, const FColor& Theme, const float Alpha)
    {
        return FColor(
            static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(static_cast<float>(Base.R), static_cast<float>(Theme.R), Alpha)), 0, 255)),
            static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(static_cast<float>(Base.G), static_cast<float>(Theme.G), Alpha)), 0, 255)),
            static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(static_cast<float>(Base.B), static_cast<float>(Theme.B), Alpha)), 0, 255)),
            Base.A);
    };
    const FColor PanelColor = BlendPalette(FColor(24, 34, 42), Level.EnvironmentPrimaryColor, 0.52f);
    const FColor EdgeColor = BlendPalette(FColor(12, 104, 112), Level.EnvironmentAccentColor, 0.70f);
    const FColor AmberColor = BlendPalette(FColor(190, 112, 24), Level.EnvironmentAccentColor, 0.66f);
    const FColor SafetyColor = BlendPalette(FColor(204, 160, 42), Level.EnvironmentAccentColor, 0.34f);
    const FColor BlackColor(14, 19, 24);

    const auto AddDecor = [this](
        const FVector& Location,
        const FRotator& Rotation,
        const FVector& Scale,
        const FColor& Color) -> AStaticMeshActor*
    {
        AStaticMeshActor* Piece = SpawnMesh(
            TEXT("/Engine/BasicShapes/Cube.Cube"),
            Location,
            Rotation,
            Scale,
            Color,
            false);
        if (Piece)
        {
            SetRuntimePieceCollision(Piece, false);
            Piece->Tags.AddUnique(TEXT("ArtDirected.Dressing"));
            if (UStaticMeshComponent* MeshComponent = Piece->GetStaticMeshComponent())
            {
                MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                MeshComponent->SetCanEverAffectNavigation(false);
            }
        }
        return Piece;
    };

    const auto AddDecorMesh = [this](
        const TCHAR* MeshPath,
        const FVector& Location,
        const FRotator& Rotation,
        const FVector& Scale,
        const FColor& Color) -> AStaticMeshActor*
    {
        AStaticMeshActor* Piece = SpawnMesh(MeshPath, Location, Rotation, Scale, Color, false);
        if (Piece)
        {
            SetRuntimePieceCollision(Piece, false);
            Piece->Tags.AddUnique(TEXT("ArtDirected.Dressing.MeshKit"));
            if (UStaticMeshComponent* MeshComponent = Piece->GetStaticMeshComponent())
            {
                MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                MeshComponent->SetCanEverAffectNavigation(false);
            }
        }
        return Piece;
    };

    const FString MapName = GetWorld()->GetMapName();
    const bool bSignalControlMap = MapName.Contains(TEXT("SignalControl"));

    // The saved shell supplies the route and collision, but its perimeter was
    // still reading as an empty horizon in a real combat camera. Build a
    // repeatable utility-bay facade from the project environment kit. Every
    // piece is visual-only: it adds architectural scale, panel rhythm, and a
    // recognizable industrial destination without changing navigation, cover,
    // spawn lanes, or interaction traces.
    const FColor FacadePanel(32, 43, 52);
    const FColor FacadeFrame(18, 73, 82);
    const FColor FacadeWarm(178, 86, 22);
    const FColor FacadeCool(18, 145, 151);
    const FColor FacadeWarning(204, 151, 34);

    int32 FacadeBayCount = 0;
    const auto AddFacadeBay = [this, &AddDecor, &AddDecorMesh, &FacadeBayCount, &FacadeFrame, &FacadeWarning](
        const float X,
        const float Y,
        const bool bFacesIntoArena,
        const FColor& PanelColor,
        const FColor& AccentColor)
    {
        const float FrontSign = bFacesIntoArena ? (Y > 0.0f ? -1.0f : 1.0f) : 1.0f;
        const float FrontY = Y + FrontSign * 22.0f;

        // Chamfered block panels read as a continuous maintenance wall from
        // gameplay distance, while the separate frames expose repetition and
        // scale like authored modular architecture.
        AddDecorMesh(
            ProjectEnvironmentPanelPath,
            FVector(X, Y, 232.0f),
            FRotator::ZeroRotator,
            FVector(2.55f, 0.18f, 2.18f),
            PanelColor);
        AddDecor(
            FVector(X - 268.0f, Y, 248.0f),
            FRotator::ZeroRotator,
            FVector(0.12f, 0.24f, 2.48f),
            FacadeFrame);
        AddDecor(
            FVector(X + 268.0f, Y, 248.0f),
            FRotator::ZeroRotator,
            FVector(0.12f, 0.24f, 2.48f),
            FacadeFrame);
        AddDecor(
            FVector(X, Y, 484.0f),
            FRotator::ZeroRotator,
            FVector(2.82f, 0.24f, 0.11f),
            FacadeFrame);

        // Front-facing service ribs create a deliberate highlight pattern and
        // make the wall legible even when the key light is behind the player.
        for (const float RibOffset : {-170.0f, -56.0f, 56.0f, 170.0f})
        {
            AddDecor(
                FVector(X + RibOffset, FrontY, 250.0f),
                FRotator::ZeroRotator,
                FVector(0.035f, 0.028f, 1.52f),
                FacadeFrame);
        }

        // A low warning stripe and two vertical signal bars establish a
        // readable authored language without becoming a HUD substitute.
        AddDecor(
            FVector(X, FrontY + FrontSign * 2.0f, 102.0f),
            FRotator::ZeroRotator,
            FVector(2.14f, 0.026f, 0.035f),
            FacadeWarning);
        AddDecor(
            FVector(X - 218.0f, FrontY + FrontSign * 3.0f, 300.0f),
            FRotator::ZeroRotator,
            FVector(0.045f, 0.03f, 0.42f),
            AccentColor);
        AddDecor(
            FVector(X + 218.0f, FrontY + FrontSign * 3.0f, 300.0f),
            FRotator::ZeroRotator,
            FVector(0.045f, 0.03f, 0.42f),
            AccentColor);

        // One raised service grille breaks up the large panel face and ties
        // the facade to the existing grate/truss vocabulary.
        AddDecorMesh(
            ProjectEnvironmentGratePath,
            FVector(X, FrontY + FrontSign * 5.0f, 402.0f),
            FRotator(90.0f, 0.0f, 0.0f),
            FVector(0.82f, 0.42f, 0.08f),
            FacadeFrame);

        // The facade now has authored human-scale openings. Doors, windows,
        // and route signs make the perimeter read as a lived-in facility
        // instead of a repeated wall primitive while remaining visual-only.
        const FRotator FacadeFacing = FRotator(0.0f, FrontSign < 0.0f ? 0.0f : 180.0f, 0.0f);
        AddDecorMesh(
            ProjectEnvironmentDoorPath,
            FVector(X, FrontY + FrontSign * 9.0f, 82.0f),
            FacadeFacing,
            FVector(1.55f, 1.0f, 1.65f),
            AccentColor);
        AddDecorMesh(
            ProjectEnvironmentWindowPath,
            FVector(X - 166.0f, FrontY + FrontSign * 8.0f, 300.0f),
            FacadeFacing,
            FVector(0.92f, 1.0f, 0.78f),
            FacadeFrame);
        AddDecorMesh(
            ProjectEnvironmentWindowPath,
            FVector(X + 166.0f, FrontY + FrontSign * 8.0f, 300.0f),
            FacadeFacing,
            FVector(0.92f, 1.0f, 0.78f),
            FacadeFrame);
        AddDecorMesh(
            ProjectEnvironmentSignPath,
            FVector(X, FrontY + FrontSign * 12.0f, 425.0f),
            FacadeFacing,
            FVector(0.92f, 1.0f, 0.58f),
            FacadeWarning);
        ++FacadeBayCount;
    };

    const TArray<float> FacadeXs = {-980.0f, -230.0f, 520.0f, 1270.0f, 2020.0f, 2770.0f};
    for (const float X : FacadeXs)
    {
        // Alternating accents keep the campaign route warm at the relay side
        // while Signal Control retains a cool competitive perimeter language.
        const FColor NorthAccent = bSignalControlMap ? FacadeCool : FacadeWarm;
        const FColor SouthAccent = bSignalControlMap ? FacadeWarm : FacadeCool;
        AddFacadeBay(X, 1435.0f, true, FacadePanel, NorthAccent);
        AddFacadeBay(X, -1435.0f, true, FacadePanel, SouthAccent);
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian authored shell: utility facade spawned bays=%d map=%s"),
        FacadeBayCount,
        *MapName);

    if (!bSignalControlMap)
    {
        // Glasswake's flagship landmark: a relay-house frontage anchored to
        // the selected operation's real breach route. This is intentionally a
        // small authored architectural composition, not another field of
        // generic cubes: doorway, observation windows, wayfinding sign,
        // relay machinery, overhead service bridge, and cable risers form a
        // destination the player can recognize during a firefight.
        const FVector RouteDirection = (Level.ExtractionLocation - Level.StartLocation).GetSafeNormal2D();
        const FVector RouteSide(-RouteDirection.Y, RouteDirection.X, 0.0f);
        const float RouteYaw = FMath::RadiansToDegrees(FMath::Atan2(RouteDirection.Y, RouteDirection.X));
        const FRotator RelayFacing(0.0f, RouteYaw - 90.0f, 0.0f);
        const FVector RelayDoorLocation = FVector(Level.BreachLocation.X, Level.BreachLocation.Y, 82.0f) + RouteSide * 180.0f;

        AddDecorMesh(
            ProjectEnvironmentDoorPath,
            RelayDoorLocation,
            RelayFacing,
            FVector(2.15f, 1.0f, 1.90f),
            Level.EnvironmentAccentColor);
        AddDecorMesh(
            ProjectEnvironmentWindowPath,
            RelayDoorLocation - RouteSide * 245.0f + FVector(0.0f, 0.0f, 175.0f),
            RelayFacing,
            FVector(1.35f, 1.0f, 1.0f),
            Level.EnvironmentSkyColor);
        AddDecorMesh(
            ProjectEnvironmentWindowPath,
            RelayDoorLocation + RouteSide * 245.0f + FVector(0.0f, 0.0f, 175.0f),
            RelayFacing,
            FVector(1.35f, 1.0f, 1.0f),
            Level.EnvironmentSkyColor);
        AddDecorMesh(
            ProjectEnvironmentSignPath,
            RelayDoorLocation + FVector(0.0f, 0.0f, 310.0f),
            RelayFacing,
            FVector(1.45f, 1.0f, 0.74f),
            Level.EnvironmentAccentColor);
        AddDecorMesh(
            ProjectEnvironmentMachineryPath,
            FVector(Level.IntelLocation.X, Level.IntelLocation.Y, 105.0f) + RouteSide * 160.0f,
            FRotator::ZeroRotator,
            FVector(1.22f),
            Level.EnvironmentPrimaryColor);
        AddDecorMesh(
            ProjectEnvironmentMachineryPath,
            FVector(Level.OpenCombatLocation.X, Level.OpenCombatLocation.Y, 92.0f) - RouteSide * 210.0f,
            FRotator(0.0f, RouteYaw, 0.0f),
            FVector(0.88f),
            Level.EnvironmentAccentColor);

        // Thin risers and bridge members read as service cabling in the
        // skyline; all are non-collision dressing and stay outside the tested
        // movement lanes.
        for (const float Offset : {-260.0f, 260.0f})
        {
            AddDecorMesh(
                ProjectEnvironmentPipePath,
                RelayDoorLocation + RouteSide * Offset + FVector(0.0f, 0.0f, 250.0f),
                FRotator::ZeroRotator,
                FVector(0.055f, 0.055f, 2.15f),
                Level.EnvironmentAccentColor);
        }
        AddDecorMesh(
            ProjectEnvironmentTrussPath,
            RelayDoorLocation + FVector(0.0f, 0.0f, 480.0f),
            RelayFacing,
            FVector(3.25f, 1.0f, 0.46f),
            Level.EnvironmentPrimaryColor);

        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian flagship architecture: relay frontage door/windows/sign/machinery/cables level=%s"),
            *Level.LevelId.ToString());
    }

    // Replace the remaining flat barricade silhouettes with the authored
    // low-cover module. These are presentation-only shells placed over the
    // already validated collision cover, so the combat lanes and traces stay
    // unchanged while the player sees a coherent family of impact shields.
    struct FCoverDressing
    {
        FVector Location;
        FRotator Rotation;
        FVector Scale;
        FColor Color;
    };
    const FCoverDressing CoverDressing[] = {
        {FVector(240.0f, -620.0f, 42.0f), FRotator(0.0f, -8.0f, 0.0f), FVector(1.35f, 0.62f, 0.58f), SafetyColor},
        {FVector(1040.0f, -260.0f, 42.0f), FRotator(0.0f, 12.0f, 0.0f), FVector(1.18f, 0.58f, 0.58f), AmberColor},
        {FVector(1740.0f, 540.0f, 42.0f), FRotator(0.0f, -5.0f, 0.0f), FVector(0.92f, 0.80f, 0.58f), EdgeColor}};
    for (const FCoverDressing& Cover : CoverDressing)
    {
        AddDecorMesh(ProjectEnvironmentCoverPath, Cover.Location, Cover.Rotation, Cover.Scale, Cover.Color);
    }

    // Floor expansion joints keep the large deck from reading as one flat
    // plane. The strips are deliberately shallow and remain below cover
    // silhouettes and objective actors.
    for (const float X : {-900.0f, -300.0f, 300.0f, 900.0f, 1500.0f, 2100.0f, 2700.0f})
    {
        AddDecor(FVector(X, 0.0f, 2.0f), FRotator::ZeroRotator, FVector(0.018f, 15.0f, 0.012f), PanelColor);
    }
    for (const float Y : {-1200.0f, -600.0f, 0.0f, 600.0f, 1200.0f})
    {
        AddDecor(FVector(650.0f, Y, 2.5f), FRotator::ZeroRotator, FVector(27.0f, 0.018f, 0.012f), PanelColor);
    }

    // Long edge rails and overhead trays establish a readable industrial
    // horizon while leaving the tested combat lanes open.
    for (const float X : {-900.0f, 650.0f, 2200.0f})
    {
        AddDecor(FVector(X, -1325.0f, 250.0f), FRotator::ZeroRotator, FVector(0.10f, 0.08f, 2.4f), EdgeColor);
        AddDecor(FVector(X, 1325.0f, 250.0f), FRotator::ZeroRotator, FVector(0.10f, 0.08f, 2.4f), EdgeColor);
        AddDecor(FVector(X, 0.0f, 510.0f), FRotator::ZeroRotator, FVector(9.5f, 0.08f, 0.08f), PanelColor);
        AddDecor(FVector(X, 0.0f, 520.0f), FRotator::ZeroRotator, FVector(8.8f, 0.025f, 0.025f), EdgeColor);
        // Replace the repeated flat overhead strips with a real authored
        // truss silhouette. It is visual-only, so the collision route and
        // spawn lanes remain exactly as validated.
        AddDecorMesh(
            ProjectEnvironmentTrussPath,
            FVector(X, 0.0f, 560.0f),
            FRotator::ZeroRotator,
            FVector(5.4f, 1.15f, 0.52f),
            PanelColor);
    }

    // A pair of raised grates and parallel service pipes make the relay
    // approach read as a maintained facility rather than a flat test floor.
    // They are dressing only; the authored shell remains the sole collision
    // authority for this map.
    AddDecorMesh(
        ProjectEnvironmentGratePath,
        FVector(780.0f, 660.0f, 7.0f),
        FRotator::ZeroRotator,
        FVector(0.92f, 0.86f, 0.10f),
        PanelColor);
    AddDecorMesh(
        ProjectEnvironmentGratePath,
        FVector(1500.0f, -420.0f, 7.0f),
        FRotator(0.0f, 90.0f, 0.0f),
        FVector(0.88f, 0.72f, 0.10f),
        PanelColor);
    for (const float Y : {-1260.0f, 1260.0f})
    {
        AddDecorMesh(
            ProjectEnvironmentPipePath,
            FVector(650.0f, Y, 92.0f),
            FRotator(0.0f, 90.0f, 0.0f),
            FVector(0.10f, 0.10f, 13.0f),
            EdgeColor);
        AddDecorMesh(
            ProjectEnvironmentPipePath,
            FVector(650.0f, Y, 138.0f),
            FRotator(0.0f, 90.0f, 0.0f),
            FVector(0.07f, 0.07f, 13.0f),
            PanelColor);
    }

    // Small orb lamps sit on the shell's service bays and give the player a
    // readable horizon rhythm without competing with the active objective.
    for (const FVector& Lamp : {
             FVector(-900.0f, -1320.0f, 332.0f),
             FVector(650.0f, 1320.0f, 332.0f),
             FVector(2200.0f, -1320.0f, 332.0f)})
    {
        AddDecorMesh(
            ProjectEnvironmentOrbPath,
            Lamp,
            FRotator::ZeroRotator,
            FVector(0.22f),
            EdgeColor);
    }

    // The relay approach gets a warm doorway frame and alternating hazard
    // plates; these are visual cues, not additional collision.
    AddDecor(FVector(590.0f, 868.0f, 170.0f), FRotator::ZeroRotator, FVector(0.08f, 0.06f, 1.7f), AmberColor);
    AddDecor(FVector(970.0f, 868.0f, 170.0f), FRotator::ZeroRotator, FVector(0.08f, 0.06f, 1.7f), AmberColor);
    AddDecor(FVector(780.0f, 868.0f, 335.0f), FRotator::ZeroRotator, FVector(1.98f, 0.06f, 0.08f), AmberColor);
    for (int32 Index = 0; Index < 10; ++Index)
    {
        AddDecor(
            FVector(1280.0f + Index * 92.0f, 531.0f, 5.0f),
            FRotator(0.0f, 0.0f, -24.0f),
            FVector(0.34f, 0.035f, 0.025f),
            Index % 2 == 0 ? SafetyColor : BlackColor);
    }

    // Signal Control receives node-neutral cyan edge marks; campaign maps
    // keep the route quieter and reserve strong colour for mission beacons.
    if (bSignalControlMap)
    {
        for (const FVector& Node : {FVector(250.0f, 650.0f, 5.0f), FVector(1500.0f, -500.0f, 5.0f), FVector(2600.0f, 650.0f, 5.0f)})
        {
            AddDecor(Node + FVector(0.0f, -210.0f, 0.0f), FRotator::ZeroRotator, FVector(1.8f, 0.025f, 0.025f), EdgeColor);
            AddDecor(Node + FVector(0.0f, 210.0f, 0.0f), FRotator::ZeroRotator, FVector(1.8f, 0.025f, 0.025f), EdgeColor);
            AddDecor(Node + FVector(-210.0f, 0.0f, 0.0f), FRotator::ZeroRotator, FVector(0.025f, 1.8f, 0.025f), EdgeColor);
            AddDecor(Node + FVector(210.0f, 0.0f, 0.0f), FRotator::ZeroRotator, FVector(0.025f, 1.8f, 0.025f), EdgeColor);
        }
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian authored shell: dressing spawned map=%s"),
        *MapName);
}

void ABMGreyboxDirector::SpawnCampaignVariantDressing()
{
    if (!GetWorld())
    {
        return;
    }

    if (const ABMGameState* State = GetWorld()->GetGameState<ABMGameState>(); State && State->bSignalControlMode)
    {
        return;
    }

    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
    const FColor PrimaryColor = Level.EnvironmentPrimaryColor;
    const FColor AccentColor = Level.EnvironmentAccentColor;
    const FColor DarkColor(
        FMath::Max(8, PrimaryColor.R / 2),
        FMath::Max(8, PrimaryColor.G / 2),
        FMath::Max(8, PrimaryColor.B / 2));
    const FName ThemeTag = FName(FString::Printf(TEXT("Environment.Theme.%s"), *Level.EnvironmentThemeId.ToString()));
    const FName TerrainTag = FName(FString::Printf(TEXT("Environment.Terrain.%s"), *Level.EnvironmentTerrainId.ToString()));
    const FName PropsTag = FName(FString::Printf(TEXT("Environment.Props.%s"), *Level.EnvironmentPropSetId.ToString()));
    const FName SkyTag = FName(FString::Printf(TEXT("Environment.Sky.%s"), *Level.EnvironmentSkyId.ToString()));

    int32 BlockingVariantPieceCount = 0;
    const auto AddDecor = [this, ThemeTag, TerrainTag, PropsTag, SkyTag, &BlockingVariantPieceCount](
        const TCHAR* MeshPath,
        const FVector& Location,
        const FRotator& Rotation,
        const FVector& Scale,
        const FColor& Color)
    {
        AStaticMeshActor* Piece = SpawnMesh(MeshPath, Location, Rotation, Scale, Color, false);
        if (!Piece)
        {
            return;
        }

        Piece->Tags.AddUnique(TEXT("ArtDirected.CampaignVariant"));
        Piece->Tags.AddUnique(ThemeTag);
        Piece->Tags.AddUnique(TerrainTag);
        Piece->Tags.AddUnique(PropsTag);
        Piece->Tags.AddUnique(SkyTag);
        CampaignVariantPieces.Add(Piece);

        // The variant pass is mostly atmospheric, but a grounded block,
        // facade, door, or window can still read as a real wall from the
        // player's camera. Keep the render and movement models consistent:
        // grounded wall-like modules receive the same runtime box proxy as
        // the authoritative campaign layout. Elevated panels, overhead
        // trusses, signs, pipes, and skyline pieces remain visual dressing so
        // they do not create surprise barriers on the route.
        const FString MeshIdentity(MeshPath ? MeshPath : TEXT(""));
        const bool bWallFamily =
            MeshIdentity.Contains(TEXT("Env_Block"), ESearchCase::IgnoreCase)
            || MeshIdentity.Contains(TEXT("Env_Panel"), ESearchCase::IgnoreCase)
            || MeshIdentity.Contains(TEXT("Env_Door"), ESearchCase::IgnoreCase)
            || MeshIdentity.Contains(TEXT("Env_Window"), ESearchCase::IgnoreCase);
        const FBox WorldBounds = Piece->GetComponentsBoundingBox(true);
        const FVector WorldSize = WorldBounds.GetSize();
        const bool bGroundedWall = bWallFamily
            && WorldBounds.Min.Z <= 34.0f
            && WorldBounds.Max.Z >= 118.0f
            && WorldSize.X >= 64.0f
            && WorldSize.Y >= 12.0f
            && WorldSize.Z >= 118.0f;
        if (bGroundedWall)
        {
            SetRuntimePieceCollision(Piece, true);
            Piece->Tags.AddUnique(TEXT("ArtDirected.CampaignVariant.BlockingWall"));
            ++BlockingVariantPieceCount;
        }
        else
        {
            SetRuntimePieceCollision(Piece, false);
        }
        if (UStaticMeshComponent* MeshComponent = Piece->GetStaticMeshComponent())
        {
            MeshComponent->SetCollisionEnabled(
                bGroundedWall
                    ? ECollisionEnabled::QueryAndPhysics
                    : ECollisionEnabled::NoCollision);
            MeshComponent->SetCanEverAffectNavigation(bGroundedWall);
        }
    };

    const FVector RouteDirection = (Level.ExtractionLocation - Level.StartLocation).GetSafeNormal2D();
    const FVector RouteAnchors[] = {
        Level.StartLocation,
        Level.InsertionLocation,
        Level.CheckpointLocation,
        Level.BreachLocation,
        Level.IntelLocation,
        Level.OpenCombatLocation,
        Level.ExtractionLocation};
    constexpr int32 RouteSegmentCount = UE_ARRAY_COUNT(RouteAnchors) - 1;
    const FColor GroundColor = Level.EnvironmentGroundColor;
    const FColor WeatherColor = Level.EnvironmentWeatherColor;
    const EBMEnvironmentTheme Theme = Level.EnvironmentTheme;
    const bool bHeroSableQuay = Level.Level == EBMCampaignLevel::SableQuay;

    // Build a macro setting after SpawnCampaignLayout(). This is the visual
    // separation layer: each family gets a different silhouette, overhead
    // volume, and landmark language instead of inheriting the same utility
    // bay with a new color grade. All pieces are non-collision because the
    // selected campaign layout owns movement, cover, and navigation collision.
    const FVector RouteSide(-RouteDirection.Y, RouteDirection.X, 0.0f);
    const float RouteYaw = FMath::RadiansToDegrees(FMath::Atan2(RouteDirection.Y, RouteDirection.X));
    const FVector MidRoute = FMath::Lerp(Level.InsertionLocation, Level.OpenCombatLocation, 0.52f);
    const FVector WarmColorPoint = FMath::Lerp(Level.CheckpointLocation, Level.BreachLocation, 0.50f);
    const FColor SkyColor = Level.EnvironmentSkyColor;
    const FVector HorizonPoint = Level.ExtractionLocation + RouteDirection * 1050.0f + FVector(0.0f, 0.0f, 420.0f);
    const auto AddMacro = [this, &AddDecor](
        const TCHAR* MeshPath,
        const FVector& Location,
        const FRotator& Rotation,
        const FVector& Scale,
        const FColor& Color)
    {
        AddDecor(MeshPath, Location, Rotation, Scale, Color);
    };

    switch (Theme)
    {
    case EBMEnvironmentTheme::OpenDry:
    {
        // Low rock shelves, a long exposed ridge, and a sparse signal mast
        // make this route read as open ground rather than an enclosed base.
        for (const float SideOffset : {-520.0f, 520.0f})
        {
            AddMacro(ProjectEnvironmentBlockPath, Level.CheckpointLocation + RouteSide * SideOffset + FVector(0.0f, 0.0f, 105.0f), FRotator(0.0f, RouteYaw + SideOffset * 0.02f, 0.0f), FVector(1.75f, 0.90f, 1.45f), GroundColor);
            AddMacro(ProjectEnvironmentBlockPath, Level.OpenCombatLocation + RouteSide * SideOffset * 0.86f + FVector(0.0f, 0.0f, 82.0f), FRotator(0.0f, RouteYaw - SideOffset * 0.015f, 0.0f), FVector(1.25f, 0.72f, 1.10f), WeatherColor);
        }
        AddMacro(ProjectEnvironmentDeckPath, MidRoute + FVector(0.0f, 0.0f, 5.0f), FRotator(0.0f, RouteYaw, 0.0f), FVector(7.5f, 0.30f, 0.045f), GroundColor);
        AddMacro(ProjectEnvironmentSignPath, Level.ExtractionLocation + RouteSide * 310.0f + FVector(0.0f, 0.0f, 215.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(1.15f, 1.0f, 1.0f), AccentColor);

        // The old open-dry backdrop was a single tall panel suspended above
        // the route. On the Metal renderer it read as a black floating wall
        // and broke the grounded scale of the quay. Replace it with a low,
        // irregular skyline made from the same authored wreck and block kit.
        // These pieces are visual-only; the campaign layout still owns the
        // perimeter and all gameplay collision.
        const FVector GroundedHorizon = Level.ExtractionLocation + RouteDirection * 1050.0f;
        const float HorizonOffsets[] = {-760.0f, -390.0f, 0.0f, 390.0f, 760.0f};
        const float HorizonYawOffsets[] = {-18.0f, 9.0f, -4.0f, 14.0f, -11.0f};
        const FVector HorizonScales[] = {
            FVector(1.80f, 0.82f, 0.78f),
            FVector(1.25f, 0.66f, 0.56f),
            FVector(2.10f, 0.92f, 0.92f),
            FVector(1.42f, 0.72f, 0.64f),
            FVector(1.72f, 0.80f, 0.74f)};
        for (int32 HorizonIndex = 0; HorizonIndex < UE_ARRAY_COUNT(HorizonOffsets); ++HorizonIndex)
        {
            AddMacro(
                ProjectEnvironmentWreckagePath,
                GroundedHorizon + RouteSide * HorizonOffsets[HorizonIndex] + FVector(0.0f, 0.0f, 82.0f),
                FRotator(0.0f, RouteYaw + HorizonYawOffsets[HorizonIndex], 0.0f),
                HorizonScales[HorizonIndex],
                HorizonIndex % 2 == 0 ? WeatherColor : GroundColor);
        }
        AddMacro(
            ProjectEnvironmentBlockPath,
            GroundedHorizon + RouteSide * 180.0f + FVector(0.0f, 0.0f, 74.0f),
            FRotator(0.0f, RouteYaw + 26.0f, 0.0f),
            FVector(2.40f, 0.72f, 0.58f),
            SkyColor);

        // Sable Quay is the first player-facing operation, so give it a
        // specific destination instead of relying on the generic ridge kit:
        // a three-bay relay service compound with a roof, doors, windows,
        // machinery, cable mast, sign, and a collapsed hull at its flank.
        // These pieces are visual-only; SpawnCampaignLayout() remains the
        // collision and navigation authority for the tested route.
        if (Level.Level == EBMCampaignLevel::SableQuay)
        {
            const FVector CompoundCenter = Level.OpenCombatLocation + RouteSide * 640.0f;
            const float CompoundYaw = RouteYaw + 90.0f;
            const FColor CompoundPanel(112, 120, 108);
            const FColor CompoundTrim(60, 112, 108);
            const FColor CompoundWarm(214, 146, 54);
            const FColor CompoundShadow(48, 58, 60);

            for (const float BayOffset : {-360.0f, 0.0f, 360.0f})
            {
                const FVector Bay = CompoundCenter + RouteDirection * BayOffset;
                AddMacro(
                    ProjectEnvironmentPanelPath,
                    Bay + FVector(0.0f, 0.0f, 232.0f),
                    FRotator(0.0f, CompoundYaw, 0.0f),
                    FVector(2.70f, 0.20f, 2.20f),
                    CompoundPanel);
                AddMacro(
                    ProjectEnvironmentWindowPath,
                    Bay - RouteSide * 28.0f + FVector(0.0f, 0.0f, 282.0f),
                    FRotator(0.0f, CompoundYaw, 0.0f),
                    FVector(1.05f, 0.90f, 0.82f),
                    SkyColor);
                AddMacro(
                    ProjectEnvironmentPipePath,
                    Bay + RouteSide * 122.0f + FVector(0.0f, 0.0f, 260.0f),
                    FRotator(0.0f, CompoundYaw, 0.0f),
                FVector(0.10f, 0.10f, 2.65f),
                CompoundTrim);

                // The roof line is carried by visible service posts rather
                // than hanging as a free-floating truss. These are
                // presentation-only supports; the low relay-house shell
                // remains the collision authority.
                for (const float Across : {-175.0f, 175.0f})
                {
                    AddMacro(
                        ProjectEnvironmentPipePath,
                        Bay + RouteSide * Across + FVector(0.0f, 0.0f, 205.0f),
                        FRotator::ZeroRotator,
                        FVector(0.20f, 0.20f, 4.0f),
                        CompoundTrim);
                }
            }

            AddMacro(
                ProjectEnvironmentDoorPath,
                CompoundCenter - RouteSide * 34.0f + FVector(0.0f, 0.0f, 112.0f),
                FRotator(0.0f, CompoundYaw, 0.0f),
                FVector(1.35f, 1.0f, 1.70f),
                CompoundWarm);
            AddMacro(
                ProjectEnvironmentMachineryPath,
                CompoundCenter + RouteDirection * 300.0f + RouteSide * 78.0f + FVector(0.0f, 0.0f, 104.0f),
                FRotator(0.0f, RouteYaw, 0.0f),
                FVector(1.35f),
                CompoundShadow);
            AddMacro(
                ProjectEnvironmentGratePath,
                CompoundCenter - RouteSide * 42.0f + FVector(0.0f, 0.0f, 9.0f),
                FRotator(0.0f, RouteYaw, 0.0f),
                FVector(2.70f, 0.82f, 0.11f),
                CompoundTrim);
            AddMacro(
                ProjectEnvironmentDeckPath,
                CompoundCenter + FVector(0.0f, 0.0f, 372.0f),
                FRotator(0.0f, RouteYaw, 0.0f),
                FVector(8.20f, 1.90f, 0.08f),
                CompoundShadow);
            // Keep the canopy grounded on the visible service posts. The
            // generic truss mesh has an asymmetric silhouette whose diagonal
            // members extend beyond its authored bounds on the Metal path;
            // above this compound it read as disconnected cyan fragments in
            // the combat camera. The lower deck and posts preserve the
            // industrial roof read without introducing unsupported geometry.
            AddMacro(
                ProjectEnvironmentSignPath,
                CompoundCenter - RouteSide * 56.0f + FVector(0.0f, 0.0f, 326.0f),
                FRotator(0.0f, CompoundYaw, 0.0f),
                FVector(1.35f, 1.0f, 0.90f),
                CompoundWarm);

            AddMacro(
                ProjectEnvironmentWreckagePath,
                CompoundCenter + RouteDirection * 470.0f - RouteSide * 120.0f + FVector(0.0f, 0.0f, 38.0f),
                FRotator(0.0f, RouteYaw - 18.0f, 0.0f),
                FVector(1.10f, 0.84f, 0.92f),
                CompoundShadow);
        }
        break;
    }
    case EBMEnvironmentTheme::CoastalStorm:
        // Floodgate setting: a raised service walk, seawall plates, drainage
        // risers, and a heavy overhead gantry define a wet vertical yard.
        AddMacro(ProjectEnvironmentDeckPath, MidRoute + RouteSide * 190.0f + FVector(0.0f, 0.0f, 12.0f), FRotator(0.0f, RouteYaw, 0.0f), FVector(8.5f, 0.42f, 0.08f), GroundColor);
        AddMacro(ProjectEnvironmentPanelPath, WarmColorPoint + RouteSide * 520.0f + FVector(0.0f, 0.0f, 260.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(4.4f, 0.22f, 2.4f), PrimaryColor);
        AddMacro(ProjectEnvironmentTrussPath, MidRoute + FVector(0.0f, 0.0f, 610.0f), FRotator(0.0f, RouteYaw, 0.0f), FVector(5.6f, 1.15f, 0.48f), WeatherColor);
        for (const float SideOffset : {-360.0f, 360.0f})
        {
            AddMacro(ProjectEnvironmentPipePath, WarmColorPoint + RouteSide * SideOffset + FVector(0.0f, 0.0f, 190.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(0.22f, 0.22f, 3.5f), AccentColor);
            AddMacro(ProjectEnvironmentGratePath, WarmColorPoint + RouteSide * SideOffset + FVector(0.0f, 0.0f, 10.0f), FRotator(0.0f, RouteYaw, 0.0f), FVector(1.70f, 0.72f, 0.12f), WeatherColor);
        }
        AddMacro(ProjectEnvironmentPanelPath, HorizonPoint, FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(11.0f, 0.14f, 3.6f), SkyColor);
        break;
    case EBMEnvironmentTheme::SnowIce:
        // Ice berth setting: faceted berms flank the route while a cold crane
        // line and raised white deck create a completely different horizon.
        AddMacro(ProjectEnvironmentDeckPath, MidRoute + FVector(0.0f, 0.0f, 6.0f), FRotator(0.0f, RouteYaw, 0.0f), FVector(8.0f, 0.34f, 0.06f), GroundColor);
        AddMacro(ProjectEnvironmentTrussPath, Level.BreachLocation + FVector(0.0f, 0.0f, 560.0f), FRotator(0.0f, RouteYaw, 0.0f), FVector(4.8f, 1.0f, 0.42f), WeatherColor);
        for (const float SideOffset : {-470.0f, 470.0f})
        {
            AddMacro(ProjectEnvironmentBlockPath, Level.CheckpointLocation + RouteSide * SideOffset + FVector(0.0f, 0.0f, 150.0f), FRotator(0.0f, RouteYaw + 25.0f, 0.0f), FVector(1.35f, 0.82f, 1.95f), WeatherColor);
            AddMacro(ProjectEnvironmentBlockPath, Level.OpenCombatLocation + RouteSide * SideOffset * 0.78f + FVector(0.0f, 0.0f, 115.0f), FRotator(0.0f, RouteYaw - 18.0f, 0.0f), FVector(1.05f, 0.68f, 1.50f), GroundColor);
            AddMacro(ProjectEnvironmentOrbPath, Level.OpenCombatLocation + RouteSide * SideOffset * 0.76f + FVector(0.0f, 0.0f, 370.0f), FRotator::ZeroRotator, FVector(0.26f), WeatherColor);
        }
        AddMacro(ProjectEnvironmentPanelPath, HorizonPoint, FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(12.0f, 0.12f, 4.5f), SkyColor);
        break;
    case EBMEnvironmentTheme::Desert:
        // Desert canyon setting: broad rock faces, a sand-colored route slab,
        // and sparse extraction towers replace the enclosed relay-yard read.
        AddMacro(ProjectEnvironmentDeckPath, MidRoute + FVector(0.0f, 0.0f, 4.0f), FRotator(0.0f, RouteYaw, 0.0f), FVector(9.0f, 0.46f, 0.045f), GroundColor);
        for (const float SideOffset : {-560.0f, 560.0f})
        {
            AddMacro(ProjectEnvironmentBlockPath, WarmColorPoint + RouteSide * SideOffset + FVector(0.0f, 0.0f, 220.0f), FRotator(0.0f, RouteYaw + SideOffset * 0.025f, 0.0f), FVector(1.95f, 0.92f, 2.25f), GroundColor);
            AddMacro(ProjectEnvironmentBlockPath, Level.ExtractionLocation + RouteSide * SideOffset * 0.75f + FVector(0.0f, 0.0f, 145.0f), FRotator(0.0f, RouteYaw - 14.0f, 0.0f), FVector(1.20f, 0.76f, 1.65f), WeatherColor);
        }
        AddMacro(ProjectEnvironmentPipePath, Level.InsertionLocation + RouteSide * 360.0f + FVector(0.0f, 0.0f, 250.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(0.24f, 0.24f, 4.0f), AccentColor);
        AddMacro(ProjectEnvironmentSignPath, Level.InsertionLocation + RouteSide * 360.0f + FVector(0.0f, 0.0f, 640.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(1.0f, 1.0f, 0.76f), WeatherColor);
        AddMacro(ProjectEnvironmentPanelPath, HorizonPoint, FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(12.0f, 0.10f, 4.2f), SkyColor);
        break;
    case EBMEnvironmentTheme::Forest:
        // Forest basin setting: repeated tree clusters and low fallen-log
        // forms create organic depth and a broken canopy around the route.
        for (int32 TreeIndex = 0; TreeIndex < 6; ++TreeIndex)
        {
            const float Alpha = 0.14f + 0.14f * static_cast<float>(TreeIndex);
            const float SideOffset = (TreeIndex % 2 == 0 ? -1.0f : 1.0f) * (360.0f + 75.0f * static_cast<float>(TreeIndex % 3));
            const FVector TreePoint = FMath::Lerp(Level.InsertionLocation, Level.OpenCombatLocation, Alpha) + RouteSide * SideOffset;
            AddMacro(ProjectEnvironmentPipePath, TreePoint + FVector(0.0f, 0.0f, 155.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(0.24f, 0.24f, 2.8f), GroundColor);
            AddMacro(ProjectEnvironmentOrbPath, TreePoint + FVector(0.0f, 0.0f, 400.0f), FRotator::ZeroRotator, FVector(0.72f), WeatherColor);
            AddMacro(ProjectEnvironmentOrbPath, TreePoint + RouteSide * 95.0f + FVector(45.0f, 0.0f, 330.0f), FRotator::ZeroRotator, FVector(0.42f), GroundColor);
            AddMacro(ProjectEnvironmentOrbPath, TreePoint - RouteSide * 82.0f + FVector(-50.0f, 0.0f, 350.0f), FRotator::ZeroRotator, FVector(0.38f), WeatherColor);
        }
        AddMacro(ProjectEnvironmentDeckPath, Level.CheckpointLocation + FVector(0.0f, 0.0f, 5.0f), FRotator(0.0f, RouteYaw, 0.0f), FVector(3.0f, 0.28f, 0.04f), GroundColor);
        AddMacro(ProjectEnvironmentPanelPath, HorizonPoint, FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(10.0f, 0.10f, 4.0f), SkyColor);
        break;
    case EBMEnvironmentTheme::Underground:
        // Underground setting: wall-mounted service panels, floor grates, and
        // grounded machinery establish a tunnel facility.  Keep the dressing
        // close to the supported collision frames so it reads as a built
        // reactor rather than detached purple geometry in the combat camera.
        for (int32 RibIndex = 0; RibIndex < 4; ++RibIndex)
        {
            const FVector RibPoint = FMath::Lerp(Level.InsertionLocation, Level.OpenCombatLocation, 0.18f + 0.20f * static_cast<float>(RibIndex));
            AddMacro(ProjectEnvironmentPanelPath, RibPoint - RouteSide * 438.0f + FVector(0.0f, 0.0f, 250.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(1.55f, 0.10f, 0.92f), PrimaryColor);
            AddMacro(ProjectEnvironmentPanelPath, RibPoint + RouteSide * 438.0f + FVector(0.0f, 0.0f, 250.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(1.55f, 0.10f, 0.92f), GroundColor);
            AddMacro(ProjectEnvironmentPipePath, RibPoint - RouteSide * 438.0f + FVector(0.0f, 0.0f, 150.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(0.14f, 0.14f, 2.25f), AccentColor);
            AddMacro(ProjectEnvironmentPipePath, RibPoint + RouteSide * 438.0f + FVector(0.0f, 0.0f, 150.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(0.14f, 0.14f, 2.25f), AccentColor);
            AddMacro(ProjectEnvironmentGratePath, RibPoint + FVector(0.0f, 0.0f, 7.0f), FRotator(0.0f, RouteYaw, 0.0f), FVector(2.65f, 0.88f, 0.08f), WeatherColor);
            AddMacro(ProjectEnvironmentOrbPath, RibPoint + FVector(0.0f, 0.0f, 402.0f), FRotator::ZeroRotator, FVector(0.22f), AccentColor);
        }
        AddMacro(ProjectEnvironmentMachineryPath, Level.IntelLocation + RouteSide * 330.0f + FVector(0.0f, 0.0f, 96.0f), FRotator(0.0f, RouteYaw, 0.0f), FVector(1.25f), PrimaryColor);
        AddMacro(ProjectEnvironmentMachineryPath, MidRoute - RouteSide * 330.0f + FVector(0.0f, 0.0f, 92.0f), FRotator(0.0f, RouteYaw + 180.0f, 0.0f), FVector(0.92f), GroundColor);
        AddMacro(ProjectEnvironmentDeckPath, MidRoute + FVector(0.0f, 0.0f, 7.0f), FRotator(0.0f, RouteYaw, 0.0f), FVector(8.0f, 0.42f, 0.06f), DarkColor);
        AddMacro(ProjectEnvironmentPanelPath, MidRoute - RouteSide * 438.0f + FVector(0.0f, 0.0f, 286.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(3.20f, 0.10f, 1.02f), GroundColor);
        AddMacro(ProjectEnvironmentPanelPath, MidRoute + RouteSide * 438.0f + FVector(0.0f, 0.0f, 286.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(3.20f, 0.10f, 1.02f), PrimaryColor);
        AddMacro(ProjectEnvironmentSignPath, Level.IntelLocation + RouteSide * 438.0f + FVector(0.0f, 0.0f, 422.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(0.80f, 1.0f, 0.52f), AccentColor);
        break;
    case EBMEnvironmentTheme::Interior:
        // Interior setting: rooms and sightline portals are expressed by
        // doors, windows, maintenance panels, and ceiling trusses rather than
        // open terrain or an exterior horizon.
        AddMacro(ProjectEnvironmentPanelPath, Level.CheckpointLocation - RouteSide * 420.0f + FVector(0.0f, 0.0f, 215.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(3.8f, 0.18f, 1.85f), PrimaryColor);
        AddMacro(ProjectEnvironmentDoorPath, Level.BreachLocation + RouteSide * 430.0f + FVector(0.0f, 0.0f, 105.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(1.45f, 1.0f, 1.75f), AccentColor);
        AddMacro(ProjectEnvironmentWindowPath, Level.IntelLocation + RouteSide * 430.0f + FVector(0.0f, 0.0f, 290.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(1.50f, 1.0f, 1.15f), WeatherColor);
        AddMacro(ProjectEnvironmentMachineryPath, WarmColorPoint + RouteSide * 260.0f + FVector(0.0f, 0.0f, 90.0f), FRotator(0.0f, RouteYaw, 0.0f), FVector(1.55f), PrimaryColor);
        AddMacro(ProjectEnvironmentTrussPath, MidRoute + FVector(0.0f, 0.0f, 600.0f), FRotator(0.0f, RouteYaw, 0.0f), FVector(5.5f, 0.90f, 0.40f), AccentColor);
        AddMacro(ProjectEnvironmentDeckPath, MidRoute + FVector(0.0f, 0.0f, 675.0f), FRotator::ZeroRotator, FVector(10.0f, 6.0f, 0.08f), DarkColor);
        AddMacro(ProjectEnvironmentPanelPath, MidRoute - RouteSide * 760.0f + FVector(0.0f, 0.0f, 300.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(7.5f, 0.12f, 2.8f), PrimaryColor);
        AddMacro(ProjectEnvironmentPanelPath, MidRoute + RouteSide * 760.0f + FVector(0.0f, 0.0f, 300.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(7.5f, 0.12f, 2.8f), PrimaryColor);
        break;
    default:
        break;
    }

    // Theme terrain is visual-only and deliberately sits on top of the
    // stable collision shell. It gives each route a recognisable ground
    // material language and skyline silhouette without invalidating the
    // interaction coordinates already verified for all twenty operations.
    for (int32 TerrainIndex = 0; TerrainIndex < 6; ++TerrainIndex)
    {
        const int32 SegmentIndex = TerrainIndex % RouteSegmentCount;
        const FVector SegmentStart = RouteAnchors[SegmentIndex];
        const FVector SegmentEnd = RouteAnchors[SegmentIndex + 1];
        const FVector SegmentDirection = (SegmentEnd - SegmentStart).GetSafeNormal2D();
        const FVector SegmentSide(-SegmentDirection.Y, SegmentDirection.X, 0.0f);
        FVector TerrainPoint = FMath::Lerp(SegmentStart, SegmentEnd, 0.24f + 0.10f * (TerrainIndex % 3));
        TerrainPoint += SegmentSide * FMath::Sin(static_cast<float>((Level.EnvironmentVariant + 3) * (TerrainIndex + 1))) * 120.0f;
        TerrainPoint.Z = 2.0f;
        const float TerrainYaw = FMath::RadiansToDegrees(FMath::Atan2(SegmentDirection.Y, SegmentDirection.X));

        switch (Theme)
        {
        case EBMEnvironmentTheme::CoastalStorm:
            AddDecor(ProjectEnvironmentDeckPath, TerrainPoint, FRotator(0.0f, TerrainYaw, 0.0f), FVector(2.45f, 0.88f, 0.055f), GroundColor);
            AddDecor(ProjectEnvironmentGratePath, TerrainPoint + SegmentSide * 95.0f + FVector(0.0f, 0.0f, 9.0f), FRotator(0.0f, TerrainYaw, 0.0f), FVector(0.95f, 0.55f, 0.10f), WeatherColor);
            AddDecor(ProjectEnvironmentPipePath, TerrainPoint + FVector(0.0f, 0.0f, 78.0f), FRotator(0.0f, TerrainYaw, 0.0f), FVector(0.08f, 0.08f, 1.9f), AccentColor);
            break;
        case EBMEnvironmentTheme::SnowIce:
            AddDecor(ProjectEnvironmentDeckPath, TerrainPoint, FRotator(0.0f, TerrainYaw, 0.0f), FVector(2.65f, 0.95f, 0.065f), GroundColor);
            AddDecor(ProjectEnvironmentBlockPath, TerrainPoint + SegmentSide * 120.0f + FVector(0.0f, 0.0f, 24.0f), FRotator(0.0f, TerrainYaw + 18.0f, 0.0f), FVector(0.56f, 0.42f, 0.50f), WeatherColor);
            AddDecor(ProjectEnvironmentBlockPath, TerrainPoint - SegmentSide * 105.0f + FVector(0.0f, 0.0f, 18.0f), FRotator(0.0f, TerrainYaw - 26.0f, 0.0f), FVector(0.38f, 0.28f, 0.34f), WeatherColor);
            AddDecor(ProjectEnvironmentOrbPath, TerrainPoint + FVector(0.0f, 0.0f, 270.0f), FRotator::ZeroRotator, FVector(0.20f), WeatherColor);
            break;
        case EBMEnvironmentTheme::Desert:
            AddDecor(ProjectEnvironmentDeckPath, TerrainPoint, FRotator(0.0f, TerrainYaw, 0.0f), FVector(2.90f, 1.05f, 0.05f), GroundColor);
            AddDecor(ProjectEnvironmentCoverPath, TerrainPoint + SegmentSide * 130.0f + FVector(0.0f, 0.0f, 42.0f), FRotator(0.0f, TerrainYaw + 12.0f, 0.0f), FVector(0.92f, 0.62f, 0.42f), WeatherColor);
            AddDecor(ProjectEnvironmentBlockPath, TerrainPoint - SegmentSide * 115.0f + FVector(0.0f, 0.0f, 42.0f), FRotator(0.0f, TerrainYaw - 18.0f, 0.0f), FVector(0.76f, 0.46f, 0.62f), GroundColor);
            break;
        case EBMEnvironmentTheme::Forest:
            AddDecor(ProjectEnvironmentDeckPath, TerrainPoint, FRotator(0.0f, TerrainYaw, 0.0f), FVector(2.35f, 0.90f, 0.05f), GroundColor);
            AddDecor(ProjectEnvironmentPipePath, TerrainPoint + SegmentSide * 105.0f + FVector(0.0f, 0.0f, 135.0f), FRotator(0.0f, TerrainYaw + 90.0f, 0.0f), FVector(0.18f, 0.18f, 2.5f), GroundColor);
            // Trunks plus offset canopy clusters give the forest family a
            // readable silhouette instead of another straight industrial
            // mast. They remain visual-only so the validated route stays
            // unchanged.
            AddDecor(ProjectEnvironmentOrbPath, TerrainPoint + SegmentSide * 105.0f + FVector(0.0f, 0.0f, 360.0f), FRotator::ZeroRotator, FVector(0.72f), WeatherColor);
            AddDecor(ProjectEnvironmentOrbPath, TerrainPoint + SegmentSide * 82.0f + FVector(85.0f, 0.0f, 305.0f), FRotator::ZeroRotator, FVector(0.48f), GroundColor);
            AddDecor(ProjectEnvironmentOrbPath, TerrainPoint + SegmentSide * 128.0f + FVector(-75.0f, 0.0f, 325.0f), FRotator::ZeroRotator, FVector(0.42f), WeatherColor);
            break;
        case EBMEnvironmentTheme::Underground:
            AddDecor(ProjectEnvironmentDeckPath, TerrainPoint, FRotator(0.0f, TerrainYaw, 0.0f), FVector(2.40f, 0.86f, 0.06f), GroundColor);
            AddDecor(ProjectEnvironmentGratePath, TerrainPoint + FVector(0.0f, 0.0f, 8.0f), FRotator(0.0f, TerrainYaw, 0.0f), FVector(1.55f, 0.62f, 0.08f), WeatherColor);
            AddDecor(ProjectEnvironmentCoverPath, TerrainPoint + SegmentSide * 126.0f + FVector(0.0f, 0.0f, 42.0f), FRotator(0.0f, TerrainYaw + 12.0f, 0.0f), FVector(0.80f, 0.48f, 0.48f), GroundColor);
            AddDecor(ProjectEnvironmentPanelPath, TerrainPoint + SegmentSide * 448.0f + FVector(0.0f, 0.0f, 258.0f), FRotator(0.0f, TerrainYaw + 90.0f, 0.0f), FVector(1.55f, 0.12f, 1.05f), PrimaryColor);
            AddDecor(ProjectEnvironmentPipePath, TerrainPoint + SegmentSide * 444.0f + FVector(0.0f, 0.0f, 152.0f), FRotator(0.0f, TerrainYaw + 90.0f, 0.0f), FVector(0.12f, 0.12f, 2.20f), AccentColor);
            break;
        case EBMEnvironmentTheme::Interior:
            AddDecor(ProjectEnvironmentDeckPath, TerrainPoint, FRotator(0.0f, TerrainYaw, 0.0f), FVector(2.25f, 0.82f, 0.055f), GroundColor);
            AddDecor(ProjectEnvironmentPanelPath, TerrainPoint + SegmentSide * 135.0f + FVector(0.0f, 0.0f, 150.0f), FRotator(0.0f, TerrainYaw + 90.0f, 0.0f), FVector(0.86f, 0.12f, 1.25f), WeatherColor);
            AddDecor(ProjectEnvironmentPipePath, TerrainPoint - SegmentSide * 115.0f + FVector(0.0f, 0.0f, 285.0f), FRotator(0.0f, TerrainYaw, 0.0f), FVector(0.10f, 0.10f, 2.1f), AccentColor);
            break;
        case EBMEnvironmentTheme::OpenDry:
        default:
            AddDecor(ProjectEnvironmentDeckPath, TerrainPoint, FRotator(0.0f, TerrainYaw, 0.0f), FVector(2.75f, 0.98f, 0.05f), GroundColor);
            AddDecor(ProjectEnvironmentGratePath, TerrainPoint + SegmentSide * 105.0f + FVector(0.0f, 0.0f, 8.0f), FRotator(0.0f, TerrainYaw, 0.0f), FVector(0.85f, 0.48f, 0.08f), AccentColor);
            break;
        }
    }

    const int32 StructureCount = 3 + (Level.EnvironmentVariant % 5);
    const int32 Pattern = Level.EnvironmentVariant % 5;

    // The selected campaign layout remains the collision authority, while this
    // route kit makes every campaign entry visibly different. The pieces are
    // anchored to that level's actual route, so moving to a new campaign
    // entry changes the skyline, rhythm, and color language together.
    // SableQuay has a dedicated compound and a collision-authored terrain
    // composition above. Do not layer the generic panel/truss pattern over
    // that hero space: the repeated unsupported trusses were the source of
    // the floating cyan fragments in the combat capture.
    if (!bHeroSableQuay)
    {
        for (int32 Index = 0; Index < StructureCount; ++Index)
        {
        const int32 SegmentIndex = Index % RouteSegmentCount;
        const FVector SegmentStart = RouteAnchors[SegmentIndex];
        const FVector SegmentEnd = RouteAnchors[SegmentIndex + 1];
        const FVector SegmentDirection = (SegmentEnd - SegmentStart).GetSafeNormal2D();
        const FVector SegmentSide(-SegmentDirection.Y, SegmentDirection.X, 0.0f);
        const float SegmentAlpha = 0.38f + 0.12f * static_cast<float>(Index / RouteSegmentCount);
        FVector Point = FMath::Lerp(SegmentStart, SegmentEnd, SegmentAlpha);
        const float LateralOffset = FMath::Sin(static_cast<float>((Level.EnvironmentVariant + 1) * (Index + 2))) * 150.0f;
        Point += SegmentSide * LateralOffset;
        Point.X = FMath::Clamp(Point.X, -1900.0f, 3150.0f);
        Point.Y = FMath::Clamp(Point.Y, -1400.0f, 1400.0f);
        Point.Z = 4.0f;

        const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(SegmentDirection.Y, SegmentDirection.X));
        if (Pattern == 0)
        {
            AddDecor(ProjectEnvironmentPanelPath, Point + FVector(0.0f, 0.0f, 155.0f), FRotator(0.0f, Yaw, 0.0f), FVector(1.8f, 0.16f, 1.35f), PrimaryColor);
            AddDecor(ProjectEnvironmentTrussPath, Point + FVector(0.0f, 0.0f, 345.0f), FRotator(0.0f, Yaw, 0.0f), FVector(2.5f, 0.65f, 0.30f), AccentColor);
        }
        else if (Pattern == 1)
        {
            AddDecor(ProjectEnvironmentPipePath, Point + FVector(0.0f, 0.0f, 120.0f), FRotator(0.0f, Yaw, 0.0f), FVector(0.12f, 0.12f, 2.8f), AccentColor);
            AddDecor(ProjectEnvironmentPipePath, Point + SegmentSide * 90.0f + FVector(0.0f, 0.0f, 75.0f), FRotator(0.0f, Yaw + 90.0f, 0.0f), FVector(0.08f, 0.08f, 2.0f), PrimaryColor);
        }
        else if (Pattern == 2)
        {
            AddDecor(ProjectEnvironmentGratePath, Point + FVector(0.0f, 0.0f, 7.0f), FRotator(0.0f, Yaw, 0.0f), FVector(1.15f, 0.72f, 0.10f), DarkColor);
            AddDecor(ProjectEnvironmentCoverPath, Point + SegmentSide * 115.0f + FVector(0.0f, 0.0f, 45.0f), FRotator(0.0f, Yaw, 0.0f), FVector(0.72f, 0.45f, 0.55f), AccentColor);
        }
        else if (Pattern == 3)
        {
            if (Theme == EBMEnvironmentTheme::Underground)
            {
                // The generic variant pattern used to place a truss in open
                // air at z=220. Underground variants instead get a wall
                // cabinet, a floor machine, and a flush service cable.
                AddDecor(ProjectEnvironmentPanelPath, Point + SegmentSide * 446.0f + FVector(0.0f, 0.0f, 244.0f), FRotator(0.0f, Yaw + 90.0f, 0.0f), FVector(1.35f, 0.12f, 0.92f), PrimaryColor);
                AddDecor(ProjectEnvironmentMachineryPath, Point - SegmentSide * 300.0f + FVector(0.0f, 0.0f, 86.0f), FRotator(0.0f, Yaw, 0.0f), FVector(0.72f), GroundColor);
                AddDecor(ProjectEnvironmentPipePath, Point + SegmentSide * 442.0f + FVector(0.0f, 0.0f, 142.0f), FRotator(0.0f, Yaw + 90.0f, 0.0f), FVector(0.10f, 0.10f, 2.05f), AccentColor);
            }
            else
            {
                AddDecor(ProjectEnvironmentTrussPath, Point + FVector(0.0f, 0.0f, 220.0f), FRotator(0.0f, Yaw, 0.0f), FVector(2.1f, 1.0f, 0.45f), PrimaryColor);
                AddDecor(ProjectEnvironmentOrbPath, Point + FVector(0.0f, 0.0f, 425.0f), FRotator::ZeroRotator, FVector(0.38f), AccentColor);
            }
        }
        else
        {
            AddDecor(ProjectEnvironmentPanelPath, Point + SegmentSide * 125.0f + FVector(0.0f, 0.0f, 90.0f), FRotator(0.0f, Yaw + 90.0f, 0.0f), FVector(0.75f, 0.12f, 1.05f), PrimaryColor);
            AddDecor(ProjectEnvironmentOrbPath, Point + FVector(0.0f, 0.0f, 250.0f), FRotator::ZeroRotator, FVector(0.24f), AccentColor);
            AddDecor(ProjectEnvironmentGratePath, Point + FVector(0.0f, 0.0f, 7.0f), FRotator(0.0f, Yaw, 0.0f), FVector(0.82f, 0.48f, 0.08f), DarkColor);
        }
        }
    }

    // The breach and extraction anchors receive a compact signature frame so
    // the objective beacons have a physical landmark that belongs to the
    // selected campaign route, not only a HUD label.
    AddDecor(
        ProjectEnvironmentPanelPath,
        Level.BreachLocation + FVector(0.0f, 0.0f, -35.0f),
        FRotator(0.0f, FMath::RadiansToDegrees(FMath::Atan2(RouteDirection.Y, RouteDirection.X)), 0.0f),
        FVector(1.35f, 0.10f, 1.10f),
        AccentColor);
    AddDecor(
        ProjectEnvironmentOrbPath,
        Level.ExtractionLocation + FVector(0.0f, 0.0f, 235.0f),
        FRotator::ZeroRotator,
        FVector(0.34f),
        AccentColor);

    // Each operation gets a small signature set in addition to the shared
    // family kit. These are deliberately non-collision dressing pieces: the
    // selected structural layout remains the movement/cover authority, while
    // the signature gives the player a memorable terrain and prop read.
    const FVector SignatureAnchor = Level.CheckpointLocation;
    const FVector SignatureSide(-RouteDirection.Y, RouteDirection.X, 0.0f);
    switch (Level.Level)
    {
    case EBMCampaignLevel::SableQuay:
        AddDecor(ProjectEnvironmentDeckPath, SignatureAnchor + FVector(0.0f, 310.0f, 5.0f), FRotator(0.0f, 8.0f, 0.0f), FVector(3.8f, 0.30f, 0.045f), GroundColor);
        AddDecor(ProjectEnvironmentBlockPath, SignatureAnchor + SignatureSide * 290.0f + FVector(0.0f, 0.0f, 42.0f), FRotator(0.0f, 18.0f, 0.0f), FVector(0.62f, 0.58f, 0.52f), WeatherColor);
        break;
    case EBMCampaignLevel::Floodline:
        AddDecor(ProjectEnvironmentGratePath, SignatureAnchor + FVector(0.0f, 300.0f, 12.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(2.4f, 0.72f, 0.12f), WeatherColor);
        AddDecor(ProjectEnvironmentPipePath, SignatureAnchor + FVector(0.0f, 300.0f, 170.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.18f, 0.18f, 3.4f), AccentColor);
        break;
    case EBMCampaignLevel::RelayHouse:
        AddDecor(ProjectEnvironmentPanelPath, SignatureAnchor + SignatureSide * 250.0f + FVector(0.0f, 0.0f, 185.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(2.2f, 0.16f, 1.65f), PrimaryColor);
        AddDecor(ProjectEnvironmentOrbPath, SignatureAnchor + SignatureSide * 250.0f + FVector(0.0f, 0.0f, 410.0f), FRotator::ZeroRotator, FVector(0.32f), AccentColor);
        break;
    case EBMCampaignLevel::NightglassCore:
        AddDecor(ProjectEnvironmentMachineryPath, SignatureAnchor + SignatureSide * 260.0f + FVector(0.0f, 0.0f, 96.0f), FRotator(0.0f, RouteYaw, 0.0f), FVector(1.05f), WeatherColor);
        AddDecor(ProjectEnvironmentPanelPath, SignatureAnchor + SignatureSide * 448.0f + FVector(0.0f, 0.0f, 294.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(2.55f, 0.12f, 1.25f), PrimaryColor);
        AddDecor(ProjectEnvironmentPipePath, SignatureAnchor + SignatureSide * 444.0f + FVector(0.0f, 0.0f, 148.0f), FRotator(0.0f, RouteYaw + 90.0f, 0.0f), FVector(0.14f, 0.14f, 2.15f), AccentColor);
        AddDecor(ProjectEnvironmentGratePath, SignatureAnchor + FVector(0.0f, 0.0f, 8.0f), FRotator(0.0f, RouteYaw, 0.0f), FVector(2.20f, 0.78f, 0.08f), GroundColor);
        break;
    case EBMCampaignLevel::AshenDocks:
        AddDecor(ProjectEnvironmentDeckPath, SignatureAnchor + FVector(0.0f, -300.0f, 7.0f), FRotator(0.0f, -12.0f, 0.0f), FVector(3.4f, 0.34f, 0.05f), GroundColor);
        AddDecor(ProjectEnvironmentOrbPath, SignatureAnchor + FVector(0.0f, -300.0f, 380.0f), FRotator::ZeroRotator, FVector(0.30f), AccentColor);
        break;
    case EBMCampaignLevel::CinderWorks:
        AddDecor(ProjectEnvironmentBlockPath, SignatureAnchor + SignatureSide * 300.0f + FVector(0.0f, 0.0f, 68.0f), FRotator(0.0f, -20.0f, 0.0f), FVector(0.95f, 0.58f, 0.82f), GroundColor);
        AddDecor(ProjectEnvironmentPipePath, SignatureAnchor + FVector(0.0f, 0.0f, 220.0f), FRotator(0.0f, 0.0f, 0.0f), FVector(0.20f, 0.20f, 2.7f), AccentColor);
        break;
    case EBMCampaignLevel::SaltFoundry:
        AddDecor(ProjectEnvironmentDeckPath, SignatureAnchor + FVector(0.0f, 330.0f, 5.0f), FRotator::ZeroRotator, FVector(4.2f, 0.26f, 0.04f), WeatherColor);
        AddDecor(ProjectEnvironmentGratePath, SignatureAnchor + SignatureSide * 275.0f + FVector(0.0f, 0.0f, 10.0f), FRotator::ZeroRotator, FVector(1.45f, 0.70f, 0.10f), GroundColor);
        break;
    case EBMCampaignLevel::BlackwaterYard:
        AddDecor(ProjectEnvironmentPipePath, SignatureAnchor + FVector(280.0f, 0.0f, 160.0f), FRotator(90.0f, 0.0f, 0.0f), FVector(0.22f, 0.22f, 3.0f), WeatherColor);
        AddDecor(ProjectEnvironmentOrbPath, SignatureAnchor + FVector(280.0f, 0.0f, 360.0f), FRotator::ZeroRotator, FVector(0.42f), AccentColor);
        break;
    case EBMCampaignLevel::GlassMarket:
        AddDecor(ProjectEnvironmentPanelPath, SignatureAnchor + FVector(-260.0f, 0.0f, 210.0f), FRotator(0.0f, 25.0f, 0.0f), FVector(1.4f, 0.12f, 1.90f), WeatherColor);
        AddDecor(ProjectEnvironmentPanelPath, SignatureAnchor + FVector(260.0f, 0.0f, 210.0f), FRotator(0.0f, -25.0f, 0.0f), FVector(1.4f, 0.12f, 1.90f), AccentColor);
        break;
    case EBMCampaignLevel::StormVault:
        AddDecor(ProjectEnvironmentTrussPath, SignatureAnchor + FVector(0.0f, 0.0f, 470.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(4.4f, 1.05f, 0.40f), WeatherColor);
        AddDecor(ProjectEnvironmentPipePath, SignatureAnchor + SignatureSide * 260.0f + FVector(0.0f, 0.0f, 260.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.09f, 0.09f, 3.6f), AccentColor);
        break;
    case EBMCampaignLevel::RedTide:
        AddDecor(ProjectEnvironmentGratePath, SignatureAnchor + FVector(0.0f, -310.0f, 11.0f), FRotator(0.0f, 12.0f, 0.0f), FVector(2.8f, 0.76f, 0.10f), AccentColor);
        AddDecor(ProjectEnvironmentBlockPath, SignatureAnchor + SignatureSide * 270.0f + FVector(0.0f, 0.0f, 54.0f), FRotator(0.0f, 28.0f, 0.0f), FVector(0.72f, 0.48f, 0.62f), WeatherColor);
        break;
    case EBMCampaignLevel::IronCathedral:
        AddDecor(ProjectEnvironmentTrussPath, SignatureAnchor + FVector(0.0f, 0.0f, 520.0f), FRotator(0.0f, 0.0f, 0.0f), FVector(2.2f, 2.0f, 0.58f), PrimaryColor);
        AddDecor(ProjectEnvironmentOrbPath, SignatureAnchor + FVector(0.0f, 0.0f, 690.0f), FRotator::ZeroRotator, FVector(0.36f), AccentColor);
        break;
    case EBMCampaignLevel::LanternBasin:
        AddDecor(ProjectEnvironmentPipePath, SignatureAnchor + SignatureSide * 230.0f + FVector(0.0f, 0.0f, 230.0f), FRotator::ZeroRotator, FVector(0.26f, 0.26f, 3.6f), GroundColor);
        AddDecor(ProjectEnvironmentOrbPath, SignatureAnchor + SignatureSide * 230.0f + FVector(0.0f, 0.0f, 610.0f), FRotator::ZeroRotator, FVector(0.78f), WeatherColor);
        break;
    case EBMCampaignLevel::QuarryNine:
        AddDecor(ProjectEnvironmentBlockPath, SignatureAnchor + FVector(-300.0f, 0.0f, 92.0f), FRotator(0.0f, 14.0f, 0.0f), FVector(1.20f, 0.72f, 1.05f), GroundColor);
        AddDecor(ProjectEnvironmentBlockPath, SignatureAnchor + FVector(300.0f, 0.0f, 62.0f), FRotator(0.0f, -18.0f, 0.0f), FVector(0.86f, 0.52f, 0.70f), WeatherColor);
        break;
    case EBMCampaignLevel::VantaSpire:
        AddDecor(ProjectEnvironmentPipePath, SignatureAnchor + FVector(0.0f, 0.0f, 300.0f), FRotator::ZeroRotator, FVector(0.16f, 0.16f, 4.5f), AccentColor);
        AddDecor(ProjectEnvironmentOrbPath, SignatureAnchor + FVector(0.0f, 0.0f, 780.0f), FRotator::ZeroRotator, FVector(0.46f), WeatherColor);
        break;
    case EBMCampaignLevel::Wreckline:
        AddDecor(ProjectEnvironmentPanelPath, SignatureAnchor + FVector(-300.0f, 0.0f, 175.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(1.9f, 0.20f, 1.55f), PrimaryColor);
        AddDecor(ProjectEnvironmentBlockPath, SignatureAnchor + FVector(300.0f, 0.0f, 80.0f), FRotator(0.0f, 8.0f, 0.0f), FVector(1.10f, 0.66f, 0.88f), GroundColor);
        break;
    case EBMCampaignLevel::EmberStation:
        AddDecor(ProjectEnvironmentPipePath, SignatureAnchor + FVector(-250.0f, 0.0f, 185.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.22f, 0.22f, 3.2f), AccentColor);
        AddDecor(ProjectEnvironmentOrbPath, SignatureAnchor + FVector(-250.0f, 0.0f, 475.0f), FRotator::ZeroRotator, FVector(0.36f), WeatherColor);
        break;
    case EBMCampaignLevel::PaleHarbor:
        AddDecor(ProjectEnvironmentDeckPath, SignatureAnchor + FVector(0.0f, 310.0f, 8.0f), FRotator(0.0f, -9.0f, 0.0f), FVector(3.6f, 0.38f, 0.06f), WeatherColor);
        AddDecor(ProjectEnvironmentBlockPath, SignatureAnchor + SignatureSide * 275.0f + FVector(0.0f, 0.0f, 76.0f), FRotator(0.0f, 20.0f, 0.0f), FVector(0.80f, 0.58f, 0.92f), WeatherColor);
        break;
    case EBMCampaignLevel::MeridianGate:
        AddDecor(ProjectEnvironmentPanelPath, SignatureAnchor + FVector(0.0f, 0.0f, 240.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(3.6f, 0.16f, 2.2f), PrimaryColor);
        AddDecor(ProjectEnvironmentGratePath, SignatureAnchor + FVector(0.0f, -300.0f, 10.0f), FRotator::ZeroRotator, FVector(2.2f, 0.70f, 0.10f), AccentColor);
        break;
    case EBMCampaignLevel::LastLight:
        AddDecor(ProjectEnvironmentDeckPath, SignatureAnchor + FVector(0.0f, -330.0f, 6.0f), FRotator(0.0f, 15.0f, 0.0f), FVector(4.0f, 0.28f, 0.05f), GroundColor);
        AddDecor(ProjectEnvironmentOrbPath, SignatureAnchor + FVector(0.0f, -330.0f, 340.0f), FRotator::ZeroRotator, FVector(0.38f), AccentColor);
        break;
    default:
        break;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian campaign: variant dressing level=%s terrain=%s props=%s variant=%d pattern=%d structures=%d"),
        *Level.LevelId.ToString(),
        *Level.EnvironmentTerrainId.ToString(),
        *Level.EnvironmentPropSetId.ToString(),
        Level.EnvironmentVariant,
        Pattern,
        StructureCount);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian campaign: variant collision level=%s grounded_wall_pieces=%d source=visible_wall_modules"),
        *Level.LevelId.ToString(),
        BlockingVariantPieceCount);
}

void ABMGreyboxDirector::SpawnAuthoredProps()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
    const FLinearColor ThemePrimary = FLinearColor::FromSRGBColor(Level.EnvironmentPrimaryColor);
    const FLinearColor ThemeAccent = FLinearColor::FromSRGBColor(Level.EnvironmentAccentColor);
    const auto BlendTheme = [](const FLinearColor& Base, const FLinearColor& Theme, const float Alpha)
    {
        return FLinearColor(
            FMath::Lerp(Base.R, Theme.R, Alpha),
            FMath::Lerp(Base.G, Theme.G, Alpha),
            FMath::Lerp(Base.B, Theme.B, Alpha),
            1.0f);
    };
    const FLinearColor DarkMetal = BlendTheme(FLinearColor(0.18f, 0.23f, 0.27f, 1.0f), ThemePrimary, 0.38f);
    // The relay is the mission's visual anchor. Keep its metal readable under
    // the night grade instead of letting it collapse into the surrounding
    // structural silhouettes.
    const FLinearColor WeatheredMetal = BlendTheme(FLinearColor(0.38f, 0.45f, 0.48f, 1.0f), ThemePrimary, 0.24f);
    const FLinearColor SignalCyan = BlendTheme(FLinearColor(0.03f, 0.56f, 0.58f, 1.0f), ThemeAccent, 0.60f);
    const FLinearColor SignalAmber = BlendTheme(FLinearColor(0.95f, 0.42f, 0.08f, 1.0f), ThemeAccent, 0.52f);
    const FLinearColor Timber = BlendTheme(FLinearColor(0.28f, 0.20f, 0.12f, 1.0f), ThemePrimary, 0.22f);
    const FLinearColor SafetyYellow = BlendTheme(FLinearColor(0.70f, 0.48f, 0.08f, 1.0f), ThemeAccent, 0.36f);
    const auto Grounded = [](const FVector& Location)
    {
        return FVector(Location.X, Location.Y, 0.0f);
    };
    const FVector RouteForward = (Level.ExtractionLocation - Level.StartLocation).GetSafeNormal2D().IsNearlyZero()
        ? FVector::ForwardVector
        : (Level.ExtractionLocation - Level.StartLocation).GetSafeNormal2D();
    const FVector RouteRight(-RouteForward.Y, RouteForward.X, 0.0f);
    const auto RoutePoint = [&RouteForward, &RouteRight](
        const FVector& Anchor,
        const float Along,
        const float Across)
    {
        return FVector(
            Anchor.X + RouteForward.X * Along + RouteRight.X * Across,
            Anchor.Y + RouteForward.Y * Along + RouteRight.Y * Across,
            0.0f);
    };

    SpawnArtDirectedProp(
        EBMArtDirectedPropVariant::Crate,
        Grounded(Level.StartLocation + FVector(120.0f, 100.0f, 0.0f)),
        FRotator(0.0f, -14.0f, 0.0f),
        FVector(1.0f),
        Timber,
        SafetyYellow);
    SpawnArtDirectedProp(
        EBMArtDirectedPropVariant::Barricade,
        RoutePoint(Level.CheckpointLocation, 120.0f, -390.0f),
        FRotator(0.0f, 18.0f, 0.0f),
        FVector(1.0f),
        Timber,
        SignalAmber,
        true);
    SpawnArtDirectedProp(
        EBMArtDirectedPropVariant::PipeRack,
        RoutePoint(Level.IntelLocation, 140.0f, -360.0f),
        FRotator::ZeroRotator,
        FVector(1.0f),
        DarkMetal,
        SignalCyan);

    BreachRelayProp = SpawnArtDirectedProp(
        EBMArtDirectedPropVariant::RelayCabinet,
        RoutePoint(Level.BreachLocation, 0.0f, 320.0f),
        FRotator::ZeroRotator,
        FVector(1.0f),
        WeatheredMetal,
        SignalAmber,
        true);
    if (BreachRelayProp)
    {
        BreachRelayProp->Tags.AddUnique(TEXT("Objective.RelayBreach"));
    }

    SpawnArtDirectedProp(
        EBMArtDirectedPropVariant::Crate,
        RoutePoint(Level.BreachLocation, 160.0f, -340.0f),
        FRotator(0.0f, 9.0f, 0.0f),
        FVector(0.82f),
        Timber,
        SignalAmber);

    // The first two combat pockets need authored silhouettes at the player's
    // eye line, not only a perimeter shell. These pieces are deliberately
    // offset from the objective faces so they add readable peek/retreat
    // choices without blocking the mission traces or the tested route.
    SpawnArtDirectedProp(
        EBMArtDirectedPropVariant::Crate,
        Grounded(Level.AmmoPickupLocation + FVector(100.0f, 80.0f, 0.0f)),
        FRotator(0.0f, -12.0f, 0.0f),
        FVector(0.88f),
        Timber,
        SafetyYellow);
    SpawnArtDirectedProp(
        EBMArtDirectedPropVariant::Barricade,
        RoutePoint(Level.CheckpointLocation, 280.0f, 390.0f),
        FRotator(0.0f, 22.0f, 0.0f),
        FVector(0.92f),
        Timber,
        SignalAmber,
        true);
    SpawnArtDirectedProp(
        EBMArtDirectedPropVariant::PipeRack,
        RoutePoint(Level.CheckpointLocation, -120.0f, 470.0f),
        FRotator(0.0f, 90.0f, 0.0f),
        FVector(0.82f),
        DarkMetal,
        SignalCyan);
    SpawnArtDirectedProp(
        EBMArtDirectedPropVariant::Crate,
        RoutePoint(Level.OpenCombatLocation, -260.0f, -360.0f),
        FRotator(0.0f, 8.0f, 0.0f),
        FVector(0.78f),
        Timber,
        SignalAmber);
    SpawnArtDirectedProp(
        EBMArtDirectedPropVariant::Barricade,
        RoutePoint(Level.OpenCombatLocation, 180.0f, 360.0f),
        FRotator(0.0f, -16.0f, 0.0f),
        FVector(0.96f),
        Timber,
        SafetyYellow,
        true);
    SpawnArtDirectedProp(
        EBMArtDirectedPropVariant::PipeRack,
        RoutePoint(Level.ExtractionLocation, 160.0f, -360.0f),
        FRotator::ZeroRotator,
        FVector(0.88f),
        DarkMetal,
        SignalCyan);
    SpawnArtDirectedProp(
        EBMArtDirectedPropVariant::SignalTower,
        Grounded(Level.StartLocation + FVector(-120.0f, -180.0f, 0.0f)),
        FRotator::ZeroRotator,
        FVector(0.90f),
        DarkMetal,
        SignalCyan);
    SpawnArtDirectedProp(
        EBMArtDirectedPropVariant::SignalTower,
        RoutePoint(Level.BreachLocation, -220.0f, 420.0f),
        FRotator(0.0f, 180.0f, 0.0f),
        FVector(0.84f),
        DarkMetal,
        SignalAmber);
    SpawnArtDirectedProp(
        EBMArtDirectedPropVariant::SignalTower,
        RoutePoint(Level.ExtractionLocation, 220.0f, 420.0f),
        FRotator::ZeroRotator,
        FVector(0.92f),
        DarkMetal,
        SignalAmber);

    SpawnCampaignPhysicalSet();
}

void ABMGreyboxDirector::SpawnCampaignPhysicalSet()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
    const FLinearColor ThemePrimary = FLinearColor::FromSRGBColor(Level.EnvironmentPrimaryColor);
    const FLinearColor ThemeGround = FLinearColor::FromSRGBColor(Level.EnvironmentGroundColor);
    const FLinearColor ThemeAccent = FLinearColor::FromSRGBColor(Level.EnvironmentAccentColor);
    const FLinearColor Wood = FLinearColor::LerpUsingHSV(
        FLinearColor(0.28f, 0.20f, 0.12f, 1.0f),
        ThemePrimary,
        0.24f);
    const FLinearColor Metal = FLinearColor::LerpUsingHSV(
        FLinearColor(0.18f, 0.23f, 0.27f, 1.0f),
        ThemePrimary,
        0.42f);
    const FLinearColor Accent = FLinearColor::LerpUsingHSV(
        FLinearColor(0.05f, 0.60f, 0.58f, 1.0f),
        ThemeAccent,
        0.56f);
    const FLinearColor Ground = FLinearColor::LerpUsingHSV(
        FLinearColor(0.30f, 0.26f, 0.20f, 1.0f),
        ThemeGround,
        0.42f);

    const FVector RouteForward = (Level.ExtractionLocation - Level.StartLocation).GetSafeNormal2D().IsNearlyZero()
        ? FVector::ForwardVector
        : (Level.ExtractionLocation - Level.StartLocation).GetSafeNormal2D();
    const FVector RouteRight(-RouteForward.Y, RouteForward.X, 0.0f);
    int32 VehicleCount = 0;
    int32 PillarCount = 0;
    int32 VegetationCount = 0;
    int32 FurnitureCount = 0;
    int32 LandmarkCount = 0;
    int32 HardCoverCount = 0;

    const auto RoutePoint = [&RouteForward, &RouteRight](
        const FVector& Anchor,
        const float Along,
        const float Across,
        const float SurfaceZ = 0.0f)
    {
        return FVector(
            Anchor.X + RouteForward.X * Along + RouteRight.X * Across,
            Anchor.Y + RouteForward.Y * Along + RouteRight.Y * Across,
            SurfaceZ);
    };

    const auto AddProp = [this, &VehicleCount, &PillarCount, &VegetationCount, &FurnitureCount, &LandmarkCount, &HardCoverCount](
        const EBMArtDirectedPropVariant Variant,
        const FVector& Location,
        const float Yaw,
        const FVector& Scale,
        const FLinearColor& BaseColor,
        const FLinearColor& AccentColor,
        const bool bDestructible = false)
    {
        // These props are cover and set dressing, but remain collision-bearing
        // through ABMArtDirectedProp. Keep their roots on the route surface so
        // a generated composition never creates a floating silhouette.
        SpawnArtDirectedProp(
            Variant,
            Location,
            FRotator(0.0f, Yaw, 0.0f),
            Scale,
            BaseColor,
            AccentColor,
            bDestructible);
        switch (Variant)
        {
        case EBMArtDirectedPropVariant::Vehicle:
            ++VehicleCount;
            // Vehicles are broad collision-bearing silhouettes, not merely
            // decoration. Count them as hard cover in the physical-set
            // evidence for sparse operations.
            ++HardCoverCount;
            break;
        case EBMArtDirectedPropVariant::Pillar:
            ++PillarCount;
            // The pillar kit blocks rounds and breaks sightlines, so it is a
            // hard-cover landmark even when it is not a crate or barricade.
            ++HardCoverCount;
            break;
        case EBMArtDirectedPropVariant::Vegetation:
            ++VegetationCount;
            break;
        case EBMArtDirectedPropVariant::Furniture:
            ++FurnitureCount;
            break;
        case EBMArtDirectedPropVariant::RelayBuilding:
        case EBMArtDirectedPropVariant::ShipHull:
        case EBMArtDirectedPropVariant::DockCrane:
            ++LandmarkCount;
            break;
        case EBMArtDirectedPropVariant::Barricade:
        case EBMArtDirectedPropVariant::Crate:
            ++HardCoverCount;
            break;
        default:
            break;
        }
    };

    const auto AddCoverPair = [&AddProp, &RoutePoint](
        const EBMArtDirectedPropVariant Variant,
        const FVector& Anchor,
        const float Along,
        const float Across,
        const float SurfaceZ,
        const float Yaw,
        const FVector& Scale,
        const FLinearColor& BaseColor,
        const FLinearColor& AccentColor)
    {
        AddProp(Variant, RoutePoint(Anchor, Along, Across, SurfaceZ), Yaw, Scale, BaseColor, AccentColor, true);
        AddProp(Variant, RoutePoint(Anchor, Along + 150.0f, -Across, SurfaceZ), Yaw + 18.0f, Scale * 0.92f, BaseColor, AccentColor, true);
    };

    // This switch is deliberately physical rather than a material/lighting
    // pass. Each entry adds a different set of collision-bearing silhouettes,
    // cover rhythms, and sightline breaks to the authored structural layout.
    // The mission anchors are never moved here; these pieces are placed beside
    // the route so the existing interaction traces and objective order remain
    // valid.
    switch (Level.Level)
    {
    case EBMCampaignLevel::SableQuay:
        // Quay approach: the relay building and grounded ship hull establish
        // two different flank identities, while the center remains exposed
        // enough for the first wave to be readable and contestable.
        AddProp(EBMArtDirectedPropVariant::RelayBuilding, RoutePoint(Level.CheckpointLocation, 40.0f, 720.0f), -12.0f, FVector(0.86f), Metal, Accent);
        AddProp(EBMArtDirectedPropVariant::ShipHull, RoutePoint(Level.OpenCombatLocation, -160.0f, -720.0f), 18.0f, FVector(0.88f), Metal, Accent, true);
        AddCoverPair(EBMArtDirectedPropVariant::Crate, Level.CheckpointLocation, 120.0f, 330.0f, 0.0f, 12.0f, FVector(1.08f), Wood, Accent);
        AddProp(EBMArtDirectedPropVariant::Vehicle, RoutePoint(Level.OpenCombatLocation, -180.0f, -480.0f), -8.0f, FVector(1.12f), Metal, Accent, true);
        // Keep the crane on the quay flank so it frames the route and offers
        // a meaningful side-cover landmark without sealing the checkpoint or
        // extraction approach.
        AddProp(EBMArtDirectedPropVariant::DockCrane, RoutePoint(Level.ExtractionLocation, -140.0f, -760.0f), 14.0f, FVector(1.05f), Metal, Accent);
        AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.ExtractionLocation, 120.0f, 420.0f), 24.0f, FVector(0.72f), Ground, Accent);
        break;

    case EBMCampaignLevel::Floodline:
        // Drainage channel: the player crosses between pump racks and a pair
        // of maintenance carts, with the channel itself left open for combat.
        AddProp(EBMArtDirectedPropVariant::PipeRack, RoutePoint(Level.CheckpointLocation, 80.0f, 360.0f), 90.0f, FVector(1.14f), Metal, Accent);
        AddProp(EBMArtDirectedPropVariant::PipeRack, RoutePoint(Level.BreachLocation, -90.0f, -360.0f), 90.0f, FVector(0.96f), Metal, Accent);
        AddCoverPair(EBMArtDirectedPropVariant::Vehicle, Level.IntelLocation, -130.0f, 420.0f, 35.0f, 90.0f, FVector(0.84f), Metal, Accent);
        break;

    case EBMCampaignLevel::RelayHouse:
        // Rooftop service route: furniture and tall columns make the roof
        // plates read as occupied service decks instead of empty platforms.
        AddProp(EBMArtDirectedPropVariant::Furniture, RoutePoint(Level.CheckpointLocation, 0.0f, 360.0f, 250.0f), 4.0f, FVector(0.90f), Wood, Accent, true);
        AddProp(EBMArtDirectedPropVariant::Furniture, RoutePoint(Level.BreachLocation, 40.0f, -380.0f, 390.0f), -16.0f, FVector(0.86f), Wood, Accent, true);
        AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.IntelLocation, 120.0f, 360.0f, 0.0f), 0.0f, FVector(0.65f), Metal, Accent);
        AddProp(EBMArtDirectedPropVariant::Barricade, RoutePoint(Level.BreachLocation, -160.0f, 210.0f, 390.0f), 22.0f, FVector(0.82f), Wood, Accent, true);
        break;

    case EBMCampaignLevel::NightglassCore:
        // Undercroft: tight columns, low console furniture, and offset hard
        // cover turn the long room into an alternating tunnel rhythm.
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const float Along = -360.0f + static_cast<float>(Index) * 300.0f;
            AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.BreachLocation, Along, Index % 2 == 0 ? -330.0f : 330.0f), 0.0f, FVector(0.54f), Metal, Accent);
        }
        AddProp(EBMArtDirectedPropVariant::Furniture, RoutePoint(Level.IntelLocation, 80.0f, 230.0f), 90.0f, FVector(0.88f), Metal, Accent, true);
        // Keep the cover on the flank of the combat pocket. The previous
        // across-route offset placed this barricade on the deterministic
        // probe's approach position for NightglassCore, trapping the player
        // behind hard cover and preventing a valid sightline to the wave.
        AddProp(EBMArtDirectedPropVariant::Barricade, RoutePoint(Level.OpenCombatLocation, -120.0f, 450.0f), 90.0f, FVector(0.86f), Metal, Accent, true);
        break;

    case EBMCampaignLevel::AshenDocks:
        // Burned dry dock: wrecked service vehicles and staggered barricades
        // create pier-to-pier crossfire rather than a flat open yard.
        AddProp(EBMArtDirectedPropVariant::Vehicle, RoutePoint(Level.InsertionLocation, 140.0f, 300.0f), 18.0f, FVector(1.08f), Metal, Accent, true);
        AddProp(EBMArtDirectedPropVariant::Vehicle, RoutePoint(Level.OpenCombatLocation, -180.0f, -360.0f), -22.0f, FVector(0.94f), Metal, Accent, true);
        // Keep both barricades on the southern service flank. The previous
        // mirrored pair put one directly on the breach-to-intel route and
        // turned valid cover collision into an invisible console gate.
        AddProp(EBMArtDirectedPropVariant::Barricade, RoutePoint(Level.BreachLocation, -80.0f, -470.0f), 8.0f, FVector(0.82f), Wood, Accent, true);
        AddProp(EBMArtDirectedPropVariant::Barricade, RoutePoint(Level.BreachLocation, 180.0f, -520.0f), -10.0f, FVector(0.76f), Wood, Accent, true);
        break;

    case EBMCampaignLevel::CinderWorks:
        // Foundry: furnace columns, a loader, and staggered barricades frame a
        // lower service lane while the side catwalk remains optional.
        for (int32 Index = 0; Index < 3; ++Index)
        {
            AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.CheckpointLocation, -120.0f + Index * 260.0f, 370.0f, 0.0f), 0.0f, FVector(0.66f), Metal, Accent);
        }
        AddProp(EBMArtDirectedPropVariant::PipeRack, RoutePoint(Level.BreachLocation, 0.0f, -360.0f), 90.0f, FVector(1.30f), Metal, Accent);
        AddProp(EBMArtDirectedPropVariant::Vehicle, RoutePoint(Level.OpenCombatLocation, 140.0f, 300.0f), 6.0f, FVector(0.86f), Metal, Accent, true);
        AddCoverPair(EBMArtDirectedPropVariant::Barricade, Level.BreachLocation, -80.0f, 260.0f, 0.0f, 10.0f, FVector(0.84f), Wood, Accent);
        AddCoverPair(EBMArtDirectedPropVariant::Crate, Level.OpenCombatLocation, 120.0f, -320.0f, 0.0f, -12.0f, FVector(0.80f), Wood, Accent);
        break;

    case EBMCampaignLevel::SaltFoundry:
        // Evaporation basin: low salt stacks and offset crate islands break
        // long sightlines without closing the broad basin crossings.
        AddCoverPair(EBMArtDirectedPropVariant::Crate, Level.CheckpointLocation, 160.0f, 520.0f, 18.0f, 0.0f, FVector(1.12f), Ground, Accent);
        AddCoverPair(EBMArtDirectedPropVariant::Barricade, Level.CheckpointLocation, -40.0f, 360.0f, 0.0f, 8.0f, FVector(0.88f), Wood, Accent);
        AddCoverPair(EBMArtDirectedPropVariant::Barricade, Level.OpenCombatLocation, -100.0f, 460.0f, 18.0f, 90.0f, FVector(0.90f), Wood, Accent);
        AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.ExtractionLocation, -100.0f, -430.0f), 0.0f, FVector(0.76f), Ground, Accent);
        break;

    case EBMCampaignLevel::BlackwaterYard:
        // Tank farm: pump vehicles and pipe racks create narrow containment
        // lanes between the larger authored tank silhouettes.
        AddProp(EBMArtDirectedPropVariant::Vehicle, RoutePoint(Level.CheckpointLocation, 0.0f, -520.0f), 90.0f, FVector(1.02f), Metal, Accent, true);
        AddProp(EBMArtDirectedPropVariant::Vehicle, RoutePoint(Level.OpenCombatLocation, -160.0f, 520.0f), -90.0f, FVector(0.92f), Metal, Accent, true);
        AddProp(EBMArtDirectedPropVariant::PipeRack, RoutePoint(Level.BreachLocation, 90.0f, 330.0f), 0.0f, FVector(1.10f), Metal, Accent);
        AddProp(EBMArtDirectedPropVariant::Barricade, RoutePoint(Level.IntelLocation, 120.0f, -320.0f), 18.0f, FVector(0.94f), Wood, Accent, true);
        break;

    case EBMCampaignLevel::GlassMarket:
        // Shattered market: stall furniture and opposing barricades create
        // the central arcade and two side peeking alleys.
        for (int32 Index = 0; Index < 3; ++Index)
        {
            AddProp(EBMArtDirectedPropVariant::Furniture, RoutePoint(Level.BreachLocation, -180.0f + Index * 260.0f, -420.0f), 0.0f, FVector(0.82f), Wood, Accent, true);
            AddProp(EBMArtDirectedPropVariant::Furniture, RoutePoint(Level.BreachLocation, -90.0f + Index * 260.0f, 420.0f), 180.0f, FVector(0.78f), Wood, Accent, true);
        }
        AddCoverPair(EBMArtDirectedPropVariant::Crate, Level.OpenCombatLocation, 120.0f, 300.0f, 0.0f, 14.0f, FVector(0.84f), Wood, Accent);
        break;

    case EBMCampaignLevel::StormVault:
        // High vault: elevated pillars and a side barricade sell the catwalk
        // scale; the lower route remains clear beneath them.
        AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.CheckpointLocation, 0.0f, -270.0f, 420.0f), 0.0f, FVector(0.68f), Metal, Accent);
        AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.BreachLocation, 80.0f, 270.0f, 420.0f), 0.0f, FVector(0.62f), Metal, Accent);
        // Keep the receiving-apron cover on the far side of the lower lane;
        // placing it on the approach centerline can trap the player after the
        // final wave has pushed them toward the extraction route.
        AddProp(EBMArtDirectedPropVariant::Barricade, RoutePoint(Level.ExtractionLocation, -120.0f, -420.0f, 0.0f), -10.0f, FVector(0.88f), Wood, Accent, true);
        AddProp(EBMArtDirectedPropVariant::SignalTower, RoutePoint(Level.BreachLocation, 0.0f, 0.0f, 420.0f), 0.0f, FVector(0.72f), Metal, Accent);
        break;

    case EBMCampaignLevel::RedTide:
        // Chemical seawall: filter columns and red-lit hard barricades form a
        // narrow crossing while the main channel stays navigable.
        AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.CheckpointLocation, 100.0f, -420.0f), 0.0f, FVector(0.70f), Metal, Accent);
        AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.CheckpointLocation, 100.0f, 420.0f), 0.0f, FVector(0.70f), Metal, Accent);
        AddCoverPair(EBMArtDirectedPropVariant::Barricade, Level.CheckpointLocation, -40.0f, 350.0f, 0.0f, 4.0f, FVector(0.88f), Wood, Accent);
        AddCoverPair(EBMArtDirectedPropVariant::Barricade, Level.BreachLocation, 0.0f, 280.0f, 28.0f, 0.0f, FVector(0.86f), Metal, Accent);
        AddProp(EBMArtDirectedPropVariant::PipeRack, RoutePoint(Level.IntelLocation, 80.0f, -360.0f), 90.0f, FVector(1.08f), Metal, Accent);
        break;

    case EBMCampaignLevel::IronCathedral:
        // Turbine nave: a processional aisle of columns and side consoles
        // creates a cathedral-like interior with deliberate cover gaps.
        for (int32 Index = 0; Index < 5; ++Index)
        {
            const float Along = -560.0f + Index * 280.0f;
            AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.BreachLocation, Along, Index % 2 == 0 ? -560.0f : 560.0f), 0.0f, FVector(0.74f), Metal, Accent);
        }
        AddProp(EBMArtDirectedPropVariant::Furniture, RoutePoint(Level.IntelLocation, 70.0f, 310.0f), 90.0f, FVector(0.96f), Metal, Accent, true);
        break;

    case EBMCampaignLevel::LanternBasin:
        // Forest basin: vegetation creates alternating soft cover and broken
        // sightlines, with a clear central lane toward the objective.
        for (int32 Index = 0; Index < 8; ++Index)
        {
            const float Along = -520.0f + Index * 190.0f;
            const float Across = (Index % 2 == 0 ? -1.0f : 1.0f) * (430.0f + (Index % 3) * 70.0f);
            AddProp(EBMArtDirectedPropVariant::Vegetation, RoutePoint(Level.InsertionLocation, Along, Across), Index * 19.0f, FVector(0.78f + (Index % 3) * 0.08f), Ground, Accent);
        }
        AddProp(EBMArtDirectedPropVariant::Barricade, RoutePoint(Level.OpenCombatLocation, -110.0f, 250.0f), 24.0f, FVector(0.78f), Wood, Accent, true);
        break;

    case EBMCampaignLevel::QuarryNine:
        // Canyon: rock-like pillars and offset crate shelves make the cut feel
        // stepped and enclosed while preserving the central descent.
        // The negative checkpoint pillar belongs on the same outer shelf as
        // the canyon face.  At -500 cm it occupied the opening target lane
        // and could hide a hostile behind a second collision-bearing prop.
        AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.CheckpointLocation, 0.0f, -820.0f), 0.0f, FVector(0.92f), Ground, Accent);
        AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.CheckpointLocation, 170.0f, 500.0f), 0.0f, FVector(0.78f), Ground, Accent);
        AddCoverPair(EBMArtDirectedPropVariant::Crate, Level.BreachLocation, -80.0f, 270.0f, 0.0f, 20.0f, FVector(1.00f), Ground, Accent);
        AddProp(EBMArtDirectedPropVariant::PipeRack, RoutePoint(Level.OpenCombatLocation, 80.0f, -420.0f), 90.0f, FVector(0.94f), Metal, Accent);
        break;

    case EBMCampaignLevel::VantaSpire:
        // Spire ridge: a grounded tower pair and an elevated barricade frame
        // the switchbacks without sealing their stair heads.
        AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.CheckpointLocation, 0.0f, 520.0f, 280.0f), 0.0f, FVector(0.86f), Metal, Accent);
        AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.ExtractionLocation, -100.0f, -460.0f, 430.0f), 0.0f, FVector(0.72f), Metal, Accent);
        AddProp(EBMArtDirectedPropVariant::SignalTower, RoutePoint(Level.ExtractionLocation, 0.0f, 0.0f, 430.0f), 0.0f, FVector(0.78f), Metal, Accent);
        AddProp(EBMArtDirectedPropVariant::Barricade, RoutePoint(Level.BreachLocation, 0.0f, 240.0f, 0.0f), 90.0f, FVector(0.84f), Wood, Accent, true);
        break;

    case EBMCampaignLevel::Wreckline:
        // Container wreckline: vehicles and crate stacks sit at alternating
        // alley mouths, leaving two readable routes through the hull maze.
        AddProp(EBMArtDirectedPropVariant::Vehicle, RoutePoint(Level.CheckpointLocation, 0.0f, -610.0f), 8.0f, FVector(1.12f), Metal, Accent, true);
        AddProp(EBMArtDirectedPropVariant::Vehicle, RoutePoint(Level.OpenCombatLocation, -120.0f, 610.0f), 188.0f, FVector(0.98f), Metal, Accent, true);
        AddCoverPair(EBMArtDirectedPropVariant::Crate, Level.BreachLocation, 100.0f, 330.0f, 0.0f, -12.0f, FVector(1.02f), Wood, Accent);
        AddProp(EBMArtDirectedPropVariant::Barricade, RoutePoint(Level.ExtractionLocation, -80.0f, -280.0f), 12.0f, FVector(0.90f), Wood, Accent, true);
        break;

    case EBMCampaignLevel::EmberStation:
        // Fuel station: service vehicles and pipe racks create pump islands,
        // with the broad forecourt retained as the combat approach.
        AddProp(EBMArtDirectedPropVariant::Vehicle, RoutePoint(Level.CheckpointLocation, 0.0f, -370.0f), 0.0f, FVector(1.04f), Metal, Accent, true);
        AddProp(EBMArtDirectedPropVariant::Vehicle, RoutePoint(Level.BreachLocation, 120.0f, 370.0f), 180.0f, FVector(0.90f), Metal, Accent, true);
        AddProp(EBMArtDirectedPropVariant::PipeRack, RoutePoint(Level.IntelLocation, 0.0f, -310.0f), 90.0f, FVector(1.16f), Metal, Accent);
        AddCoverPair(EBMArtDirectedPropVariant::Barricade, Level.OpenCombatLocation, -90.0f, 300.0f, 0.0f, 0.0f, FVector(0.82f), Wood, Accent);
        break;

    case EBMCampaignLevel::PaleHarbor:
        // Frozen berths: ice-crane pillars, a transport wreck, and snowbank
        // cover make the elevation changes legible from the lower route.
        AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.CheckpointLocation, 0.0f, -420.0f, 220.0f), 0.0f, FVector(0.72f), Metal, Accent);
        AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.OpenCombatLocation, 80.0f, 420.0f, 360.0f), 0.0f, FVector(0.66f), Metal, Accent);
        AddProp(EBMArtDirectedPropVariant::Vehicle, RoutePoint(Level.ExtractionLocation, -120.0f, -360.0f), -12.0f, FVector(0.96f), Metal, Accent, true);
        AddCoverPair(EBMArtDirectedPropVariant::Barricade, Level.ExtractionLocation, 80.0f, 320.0f, 0.0f, -8.0f, FVector(0.94f), Wood, Accent);
        AddCoverPair(EBMArtDirectedPropVariant::Crate, Level.ExtractionLocation, -180.0f, -300.0f, 0.0f, 16.0f, FVector(0.90f), Ground, Accent);
        AddCoverPair(EBMArtDirectedPropVariant::Crate, Level.IntelLocation, 0.0f, 250.0f, 0.0f, 20.0f, FVector(0.86f), Ground, Accent);
        break;

    case EBMCampaignLevel::MeridianGate:
        // Gate checkpoint: inspection furniture, a disabled carrier, and
        // offset barricades make the approach read as a controlled entry lane.
        AddProp(EBMArtDirectedPropVariant::Vehicle, RoutePoint(Level.CheckpointLocation, -180.0f, 620.0f), 90.0f, FVector(1.06f), Metal, Accent, true);
        AddProp(EBMArtDirectedPropVariant::Furniture, RoutePoint(Level.CheckpointLocation, 100.0f, -470.0f), 0.0f, FVector(0.88f), Metal, Accent, true);
        AddCoverPair(EBMArtDirectedPropVariant::Barricade, Level.BreachLocation, 0.0f, 340.0f, 0.0f, 90.0f, FVector(0.90f), Wood, Accent);
        AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.ExtractionLocation, 120.0f, -360.0f), 0.0f, FVector(0.74f), Metal, Accent);
        break;

    case EBMCampaignLevel::LastLight:
        // Final ridge: sparse wreckage, a final signal column, and deliberate
        // low cover keep the last defense exposed but defensible.
        AddProp(EBMArtDirectedPropVariant::Vehicle, RoutePoint(Level.OpenCombatLocation, -180.0f, 430.0f), 14.0f, FVector(1.04f), Metal, Accent, true);
        AddCoverPair(EBMArtDirectedPropVariant::Crate, Level.OpenCombatLocation, 120.0f, -300.0f, 0.0f, -8.0f, FVector(0.90f), Wood, Accent);
        AddProp(EBMArtDirectedPropVariant::Pillar, RoutePoint(Level.ExtractionLocation, 0.0f, 260.0f), 0.0f, FVector(0.82f), Ground, Accent);
        AddProp(EBMArtDirectedPropVariant::SignalTower, RoutePoint(Level.ExtractionLocation, 0.0f, 0.0f), 0.0f, FVector(0.86f), Metal, Accent);
        break;

    default:
        break;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian campaign: physical prop set level=%s terrain=%s props=%d landmarks=%d vehicles=%d pillars=%d vegetation=%d furniture=%d hard_cover=%d collision_cover=yes"),
        *Level.LevelId.ToString(),
        *Level.EnvironmentTerrainId.ToString(),
        ArtDirectedProps.Num(),
        LandmarkCount,
        VehicleCount,
        PillarCount,
        VegetationCount,
        FurnitureCount,
        HardCoverCount);
}

ABMArtDirectedProp* ABMGreyboxDirector::SpawnArtDirectedProp(
    const EBMArtDirectedPropVariant Variant,
    const FVector& Location,
    const FRotator& Rotation,
    const FVector& Scale,
    const FLinearColor& BaseColor,
    const FLinearColor& AccentColor,
    const bool bDestructible)
{
    if (!HasAuthority() || !GetWorld())
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ABMArtDirectedProp* Prop = GetWorld()->SpawnActor<ABMArtDirectedProp>(
        ABMArtDirectedProp::StaticClass(), Location, Rotation, SpawnParameters);
    if (!Prop)
    {
        return nullptr;
    }

    Prop->SetActorScale3D(Scale);
    Prop->ConfigureRuntime(Variant, BaseColor, AccentColor, bDestructible);
    ArtDirectedProps.Add(Prop);
    return Prop;
}

void ABMGreyboxDirector::RefreshObjectivePresentation()
{
    HandleGameStateUpdated();
}

void ABMGreyboxDirector::HandleGameStateUpdated()
{
    const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
    if (State && State->CampaignLevel != CampaignLevel)
    {
        CampaignLevel = State->CampaignLevel;
        RefreshCampaignLevelPresentation();
    }
    SetActiveObjectiveBeacon(State ? State->CurrentObjective.ObjectiveId : NAME_None);
}

void ABMGreyboxDirector::SetActiveObjectiveBeacon(const FName ObjectiveId)
{
    // Several phase names deliberately share one physical landmark.
    const FName CanonicalObjectiveId = ObjectiveId == TEXT("Checkpoint")
        ? FName(TEXT("QuietApproach"))
        : (ObjectiveId == TEXT("FinalDefense") ? FName(TEXT("Extraction")) : ObjectiveId);

    for (const TPair<FName, TObjectPtr<APointLight>>& Entry : ObjectiveBeaconLights)
    {
        APointLight* BeaconLight = Entry.Value.Get();
        if (!BeaconLight || !BeaconLight->GetLightComponent())
        {
            continue;
        }

        const bool bIsActive = Entry.Key == CanonicalObjectiveId;
        BeaconLight->SetActorHiddenInGame(!bIsActive);
        // A very high point-light value reads as a blown-out wall on the
        // compatibility renderer. The beacon should guide the eye without
        // hiding the cover and hostiles around it.
        BeaconLight->GetLightComponent()->SetIntensity(bIsActive ? 5200.0f : 0.0f);
        if (ULocalLightComponent* LocalLight = Cast<ULocalLightComponent>(BeaconLight->GetLightComponent()))
        {
            LocalLight->SetAttenuationRadius(bIsActive ? 820.0f : 300.0f);
        }

        if (const TArray<TObjectPtr<AStaticMeshActor>>* Pieces = ObjectiveBeaconPieces.Find(Entry.Key))
        {
            for (AStaticMeshActor* Piece : *Pieces)
            {
                if (Piece)
                {
                    Piece->SetActorHiddenInGame(!bIsActive);
                }
            }
        }
    }
}

void ABMGreyboxDirector::CreateRuntimeCollisionProxy(AStaticMeshActor* Piece)
{
    if (!Piece || !Piece->GetStaticMeshComponent())
    {
        return;
    }

    UStaticMeshComponent* MeshComponent = Piece->GetStaticMeshComponent();
    TArray<UBoxComponent*> ExistingProxies;
    Piece->GetComponents<UBoxComponent>(ExistingProxies);
    for (UBoxComponent* ExistingProxy : ExistingProxies)
    {
        if (ExistingProxy && ExistingProxy->ComponentTags.Contains(TEXT("BlackMeridian.RuntimeCollisionProxy")))
        {
            return;
        }
    }

    FVector LocalMin = FVector::ZeroVector;
    FVector LocalMax = FVector::ZeroVector;
    MeshComponent->GetLocalBounds(LocalMin, LocalMax);
    const FVector LocalCenter = (LocalMin + LocalMax) * 0.5f;
    const FVector LocalExtent = (LocalMax - LocalMin) * 0.5f;
    if (LocalExtent.IsNearlyZero())
    {
        return;
    }

    UBoxComponent* CollisionProxy = NewObject<UBoxComponent>(Piece, TEXT("RuntimeCollisionProxy"));
    if (!CollisionProxy)
    {
        return;
    }

    Piece->AddInstanceComponent(CollisionProxy);
    CollisionProxy->SetupAttachment(MeshComponent);
    CollisionProxy->SetRelativeLocation(LocalCenter);
    CollisionProxy->SetBoxExtent(FVector(
        FMath::Max(LocalExtent.X, 1.0f),
        FMath::Max(LocalExtent.Y, 1.0f),
        FMath::Max(LocalExtent.Z, 1.0f)));
    CollisionProxy->SetMobility(EComponentMobility::Movable);
    CollisionProxy->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionProxy->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    CollisionProxy->SetCollisionObjectType(ECC_WorldStatic);
    CollisionProxy->SetCollisionResponseToAllChannels(ECR_Block);
    // This analytic box exists for pawn movement and navigation. It must not
    // become the authoritative hitscan occluder: open trusses, grates,
    // wreckage frames, and facade modules can have large negative spaces that
    // an axis-aligned box would incorrectly fill from the player's view.
    CollisionProxy->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    CollisionProxy->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    CollisionProxy->SetGenerateOverlapEvents(false);
    CollisionProxy->SetCanEverAffectNavigation(true);
    CollisionProxy->ComponentTags.Add(TEXT("BlackMeridian.RuntimeCollisionProxy"));
    CollisionProxy->RegisterComponent();

    // Collision diagnostics need to identify the authored/runtime piece that
    // owns an anonymous StaticMeshActor id.  Keep this behind the probe flags
    // so normal matches do not pay for a large per-piece log at startup.
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CollisionProbe"))
        || FParse::Param(FCommandLine::Get(), TEXT("BM_FullCampaignProbe")))
    {
        const FString MeshAssetPath = MeshComponent->GetStaticMesh()
            ? MeshComponent->GetStaticMesh()->GetPathName()
            : FString(TEXT("<none>"));
        const UBodySetup* BodySetup = MeshComponent->GetStaticMesh()
            ? MeshComponent->GetStaticMesh()->GetBodySetup()
            : nullptr;
        const TCHAR* MeshTraceMode = BodySetup && BodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple
            ? TEXT("complex")
            : TEXT("simple");
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian collision map: actor=%s mesh=%s mesh_trace=%s proxy_visibility=ignore actor_location=%s actor_rotation=%s actor_scale=%s proxy_location=%s local_center=%s local_extent=%s world_extent=%s"),
            *GetNameSafe(Piece),
            *MeshAssetPath,
            MeshTraceMode,
            *Piece->GetActorLocation().ToString(),
            *Piece->GetActorRotation().ToString(),
            *Piece->GetActorScale3D().ToString(),
            *CollisionProxy->GetComponentLocation().ToString(),
            *LocalCenter.ToString(),
            *LocalExtent.ToString(),
            *CollisionProxy->GetScaledBoxExtent().ToString());
    }
}

void ABMGreyboxDirector::SetRuntimePieceCollision(AStaticMeshActor* Piece, const bool bEnabled)
{
    if (!Piece)
    {
        return;
    }

    Piece->SetActorEnableCollision(bEnabled);
    if (UStaticMeshComponent* MeshComponent = Piece->GetStaticMeshComponent())
    {
        // The mesh is the canonical visibility body. Runtime layout pieces
        // are generated from modular meshes whose render geometry can be
        // open or recessed; use complex-as-simple for the visibility query so
        // the reticle and authoritative weapon trace see the same gaps the
        // player sees. The analytic proxy below remains responsible for pawn
        // movement and navigation, avoiding a second coarse occluder.
        MeshComponent->SetCollisionEnabled(
            bEnabled
                ? ECollisionEnabled::QueryAndPhysics
                : ECollisionEnabled::NoCollision);
        MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
        if (bEnabled)
        {
            MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        }
        MeshComponent->SetCanEverAffectNavigation(false);
    }

    TArray<UBoxComponent*> CollisionProxies;
    Piece->GetComponents<UBoxComponent>(CollisionProxies);
    for (UBoxComponent* CollisionProxy : CollisionProxies)
    {
        if (!CollisionProxy || !CollisionProxy->ComponentTags.Contains(TEXT("BlackMeridian.RuntimeCollisionProxy")))
        {
            continue;
        }

        CollisionProxy->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        CollisionProxy->SetCanEverAffectNavigation(bEnabled);
    }
}

void ABMGreyboxDirector::RunCollisionProbe()
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("BM_CollisionProbe")) || !GetWorld())
    {
        return;
    }

    int32 StructuralPieces = 0;
    int32 CollisionProxies = 0;
    int32 SweptPieces = 0;
    int32 BlockedSweeps = 0;
    int32 UnblockedSweeps = 0;

    for (AStaticMeshActor* Piece : CampaignLayoutPieces)
    {
        if (!IsValid(Piece)
            || Piece->Tags.Contains(TEXT("CampaignLayout.BoundaryLandmark"))
            || Piece->Tags.Contains(TEXT("CampaignLayout.TerrainBed")))
        {
            continue;
        }

        ++StructuralPieces;
        UBoxComponent* CollisionProxy = nullptr;
        TArray<UBoxComponent*> PieceBoxes;
        Piece->GetComponents<UBoxComponent>(PieceBoxes);
        for (UBoxComponent* Box : PieceBoxes)
        {
            if (Box && Box->ComponentTags.Contains(TEXT("BlackMeridian.RuntimeCollisionProxy")))
            {
                CollisionProxy = Box;
                break;
            }
        }

        if (!CollisionProxy || CollisionProxy->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
        {
            ++UnblockedSweeps;
            continue;
        }
        ++CollisionProxies;

        const FVector Extent = CollisionProxy->GetScaledBoxExtent();
        // Test every human-scale collision-bearing shape, not only the old
        // tall-room-wall subset. Low dock berms, catwalk rails, salt-basin
        // walls, and forest trunks are still physical obstacles even when
        // one horizontal half-extent is narrower than 18 cm or the wall is
        // shorter than 184 cm. Floors remain out of this set because their
        // vertical half-extent is only a few centimetres; tiny sign/detail
        // pieces are likewise ignored so the diagnostic stays focused on
        // geometry a capsule could reasonably encounter.
        if (Extent.Z < 20.0f || FMath::Max(Extent.X, Extent.Y) < 12.0f)
        {
            continue;
        }

        ++SweptPieces;
        const bool bSweepAlongX = Extent.X <= Extent.Y;
        const FVector SweepAxis = bSweepAlongX
            ? CollisionProxy->GetForwardVector()
            : CollisionProxy->GetRightVector();
        const float TravelDistance = (bSweepAlongX ? Extent.X : Extent.Y) + 34.0f + 80.0f;
        const FVector SweepStart = CollisionProxy->GetComponentLocation() - SweepAxis * TravelDistance;
        const FVector SweepEnd = CollisionProxy->GetComponentLocation() + SweepAxis * TravelDistance;
        const FCollisionShape PawnShape = FCollisionShape::MakeCapsule(34.0f, 88.0f);
        FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BlackMeridianCollisionProbe), false, this);
        // Layout pieces intentionally overlap at route joins and stacked
        // cover. Ignore every non-target actor so this diagnostic proves the
        // selected wall owns a blocking body instead of passing because a
        // neighboring piece happened to stop the sweep first.
        for (TActorIterator<AActor> ActorIterator(GetWorld()); ActorIterator; ++ActorIterator)
        {
            AActor* OtherActor = *ActorIterator;
            if (OtherActor && OtherActor != Piece)
            {
                QueryParams.AddIgnoredActor(OtherActor);
                TArray<UPrimitiveComponent*> OtherPrimitives;
                OtherActor->GetComponents<UPrimitiveComponent>(OtherPrimitives);
                for (UPrimitiveComponent* OtherPrimitive : OtherPrimitives)
                {
                    if (OtherPrimitive)
                    {
                        QueryParams.AddIgnoredComponent(OtherPrimitive);
                    }
                }
            }
        }
        FHitResult Hit;
        const bool bSweepHit = GetWorld()->SweepSingleByChannel(
            Hit,
            SweepStart,
            SweepEnd,
            FQuat::Identity,
            ECC_Pawn,
            PawnShape,
            QueryParams)
            ;
        const bool bBlocked = bSweepHit && (Hit.GetActor() == Piece || Hit.GetComponent() == CollisionProxy);

        if (bBlocked)
        {
            ++BlockedSweeps;
        }
        else
        {
            ++UnblockedSweeps;
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Black Meridian collision probe: unblocked piece=%s location=%s extent=%s"),
                *GetNameSafe(Piece),
                *CollisionProxy->GetComponentLocation().ToString(),
                *Extent.ToString());
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Black Meridian collision probe: hit sweep=%s actor=%s component=%s actor_collision=%s component_collision=%s registered=%s"),
                bSweepHit ? TEXT("yes") : TEXT("no"),
                *GetNameSafe(Hit.GetActor()),
                *GetNameSafe(Hit.GetComponent()),
                Piece->GetActorEnableCollision() ? TEXT("yes") : TEXT("no"),
                CollisionProxy->GetCollisionEnabled() == ECollisionEnabled::NoCollision ? TEXT("no") : TEXT("yes"),
                CollisionProxy->IsRegistered() ? TEXT("yes") : TEXT("no"));
        }
    }

    const bool bPassed = StructuralPieces > 0
        && CollisionProxies == StructuralPieces
        && SweptPieces > 0
        && BlockedSweeps == SweptPieces
        && UnblockedSweeps == 0;
    if (bPassed)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian collision probe: PASS level=%s structural_pieces=%d collision_proxies=%d swept_pieces=%d blocked_sweeps=%d unblocked_sweeps=%d"),
            *GetCampaignLevelDefinition(CampaignLevel).LevelId.ToString(),
            StructuralPieces,
            CollisionProxies,
            SweptPieces,
            BlockedSweeps,
            UnblockedSweeps);
    }
    else
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Black Meridian collision probe: FAIL level=%s structural_pieces=%d collision_proxies=%d swept_pieces=%d blocked_sweeps=%d unblocked_sweeps=%d"),
            *GetCampaignLevelDefinition(CampaignLevel).LevelId.ToString(),
            StructuralPieces,
            CollisionProxies,
            SweptPieces,
            BlockedSweeps,
            UnblockedSweeps);
    }
}

AStaticMeshActor* ABMGreyboxDirector::SpawnMesh(
    const TCHAR* MeshPath,
    const FVector& Location,
    const FRotator& Rotation,
    const FVector& Scale,
    const FColor& Color,
    const bool bThinCover)
{
    if (!GetWorld() || !MeshPath)
    {
        return nullptr;
    }

    const TCHAR* ResolvedMeshPath = ResolveEnvironmentMeshPath(MeshPath);
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, ResolvedMeshPath);
    if (!Mesh && FString(ResolvedMeshPath).Equals(ProjectEnvironmentDeckPath, ESearchCase::IgnoreCase))
    {
        // Keep the no-content editor/runtime path usable before the generated
        // environment package has been materialized by the editor test.
        Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    }
    if (!Mesh && FString(ResolvedMeshPath).StartsWith(TEXT("/Game/Environment/Meshes/"), ESearchCase::IgnoreCase))
    {
        // New modular art assets are generated by the editor automation lane.
        // Preserve the source-only compatibility path if a developer launches
        // the runtime before that lane has materialized the optional asset.
        Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    }
    if (!Mesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("Black Meridian runtime: presentation mesh missing path=%s resolved=%s"), MeshPath, ResolvedMeshPath);
        return nullptr;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AStaticMeshActor* Piece = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParameters);
    if (!Piece)
    {
        return nullptr;
    }

    // Runtime layout components must be movable before their mesh is
    // assigned. This avoids Unreal treating the first assignment as a static
    // mobility update and preserves deterministic runtime collision/nav data.
    Piece->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Piece->GetStaticMeshComponent()->SetStaticMesh(Mesh);
    Piece->SetActorScale3D(Scale);
    // Runtime-spawned cubes do not inherit an authored map collision profile.
    // Make the floor and cover explicitly block characters even though the
    // components are movable for the no-Lightmass compatibility path.
    Piece->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Piece->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);
    Piece->GetStaticMeshComponent()->SetGenerateOverlapEvents(false);
    Piece->SetActorEnableCollision(true);
    Piece->GetStaticMeshComponent()->SetVisibility(true, true);
    CreateRuntimeCollisionProxy(Piece);
    SetRuntimePieceCollision(Piece, true);
    // This geometry is created during BeginPlay, so it cannot participate in
    // editor-baked static lighting. Keep it movable and use runtime lighting.
    if (bThinCover)
    {
        Piece->Tags.Add(TEXT("ThinCover"));
    }

    // Prefer the lit engine BasicShapeMaterial so floor, walls, and cover
    // retain readable depth. Keep the emissive material as a compatibility
    // fallback for stripped-down runtimes.
    UMaterialInterface* BaseMaterial = LoadPresentationMaterial();
    if (BaseMaterial)
    {
        UMaterialInstanceDynamic* Material = Piece->GetStaticMeshComponent()->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMaterial);
        if (Material)
        {
            // FColor is used above as a compact art-direction palette. Treat
            // its channels as the intended linear material values here; the
            // gamma-decoded FLinearColor(FColor) constructor makes these
            // already-dark industrial tones nearly black on the runtime
            // renderer.
            // The mission is a night operation, but the player still needs to
            // read cover, doors, and squad silhouettes on the compatibility
            // renderer instead of navigating a black screen.
            // Runtime-spawned pieces have no baked Lightmass contribution.
            // Keep the industrial palette dark, but leave enough reflected
            // value for cover and enemy silhouettes on the packaged Metal
            // renderer.  The previous 0.58 multiplier made the same scene
            // read as a black void when the Entry-map key light was not
            // available during a packaged launch.
            // Establish a readable shadow floor before the runtime light rig
            // is applied. Generated geometry has no Lightmass contribution,
            // so dark diffuse metal otherwise becomes a black cut-out on
            // shadow-facing walls and cover.
            // Runtime campaign geometry has no baked Lightmass contribution.
            // Lift the material floor enough for authored cover, doors, and
            // enemy silhouettes to remain readable in a real rendered match;
            // the directional key and theme grade still provide the contrast.
            constexpr float MaterialBrightness = 1.46f;
            constexpr float MinimumSurfaceValue = 0.075f;
            const FLinearColor PresentationColor(
                FMath::Clamp(FMath::Max((static_cast<float>(Color.R) / 255.0f) * MaterialBrightness, MinimumSurfaceValue), 0.0f, 0.92f),
                FMath::Clamp(FMath::Max((static_cast<float>(Color.G) / 255.0f) * MaterialBrightness, MinimumSurfaceValue), 0.0f, 0.92f),
                FMath::Clamp(FMath::Max((static_cast<float>(Color.B) / 255.0f) * MaterialBrightness, MinimumSurfaceValue), 0.0f, 0.92f),
                static_cast<float>(Color.A) / 255.0f);
            Material->SetVectorParameterValue(TEXT("Color"), PresentationColor);
            Material->SetVectorParameterValue(TEXT("BaseColor"), PresentationColor);
            // A restrained constant lift keeps shadow-facing surfaces legible;
            // saturated route accents get a little more signal so they read as
            // practical navigation landmarks instead of debug-colored cubes.
            const bool bAccentSurface =
                (Color.G > Color.R + 28 && Color.G >= Color.B * 0.88f)
                || (Color.B > Color.R + 30 && Color.B > Color.G + 12)
                || (Color.R > Color.G + 42 && Color.R > Color.B + 26);
            const float EmissiveScale = bAccentSurface ? 0.18f : 0.038f;
            Material->SetVectorParameterValue(TEXT("EmissiveColor"), PresentationColor * EmissiveScale);
            Material->SetScalarParameterValue(TEXT("EmissiveStrength"), bAccentSurface ? 1.15f : 0.75f);
            Material->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
            Material->SetScalarParameterValue(TEXT("Metallic"), bThinCover ? 0.28f : 0.12f);
            Material->SetScalarParameterValue(TEXT("Roughness"), bThinCover ? 0.78f : 0.86f);
        }
    }
    GreyboxPieces.Add(Piece);
    return Piece;
}

AStaticMeshActor* ABMGreyboxDirector::SpawnCube(
    const FVector& Location,
    const FVector& Scale,
    const FColor& Color,
    const bool bThinCover)
{
    return SpawnMesh(TEXT("/Engine/BasicShapes/Cube.Cube"), Location, FRotator::ZeroRotator, Scale, Color, bThinCover);
}

ABMInteractableActor* ABMGreyboxDirector::SpawnInteraction(
    const FName InteractionId,
    const FText& Prompt,
    const EBMInteractionType InteractionType,
    const EBMInteractionAction InteractionAction,
    const FVector& Location,
    const FVector& Scale,
    const float HoldDuration)
{
    if (!HasAuthority() || !GetWorld())
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ABMInteractableActor* Interaction = GetWorld()->SpawnActor<ABMInteractableActor>(
        ABMInteractableActor::StaticClass(), Location, FRotator::ZeroRotator, SpawnParameters);
    if (!Interaction)
    {
        return nullptr;
    }

    Interaction->InteractionId = InteractionId;
    Interaction->Prompt = Prompt;
    Interaction->InteractionType = InteractionType;
    Interaction->InteractionAction = InteractionAction;
    Interaction->HoldDuration = HoldDuration;
    Interaction->bAutoPickup = InteractionAction == EBMInteractionAction::PickupWeapon ||
        InteractionAction == EBMInteractionAction::PickupAmmo ||
        InteractionAction == EBMInteractionAction::PickupThrowable ||
        InteractionAction == EBMInteractionAction::PickupAttachment;
    if (Interaction->bAutoPickup)
    {
        Interaction->Prompt = InteractionAction == EBMInteractionAction::PickupThrowable
            ? NSLOCTEXT("BlackMeridian", "OrdnancePickupPrompt", "WALK NEAR  RESTOCK ORDNANCE")
            : (InteractionAction == EBMInteractionAction::PickupAttachment
                ? NSLOCTEXT("BlackMeridian", "AttachmentPickupPrompt", "WALK NEAR  TAKE ATTACHMENT")
                : NSLOCTEXT("BlackMeridian", "AutoPickupPrompt", "WALK NEAR  AUTO-COLLECT"));
    }
    Interaction->SetActorScale3D(Scale);
    const FLinearColor PresentationColor = InteractionId == TEXT("CheckpointOrdnance")
        ? FLinearColor(1.0f, 0.22f, 0.04f)
        : InteractionId == TEXT("RelayBreach")
        ? FLinearColor(1.0f, 0.38f, 0.1f)
        : (InteractionId == TEXT("IntelConsole")
            ? FLinearColor(0.1f, 0.9f, 0.8f)
            : (InteractionId == TEXT("ExtractionBoat")
                ? FLinearColor(1.0f, 0.8f, 0.15f)
                : FLinearColor(0.2f, 0.55f, 0.75f)));
    Interaction->SetPresentationColor(PresentationColor);
    if (InteractionId == TEXT("PlantSignalJammer"))
    {
        // The jammer is a ground marker, not a physical barricade. Keep its
        // visibility collision so the interaction trace remains precise, but
        // do not let the marker stop the pawn or contribute a nav obstacle in
        // the middle of the open-combat route.
        if (UStaticMeshComponent* InteractionMesh = Interaction->FindComponentByClass<UStaticMeshComponent>())
        {
            InteractionMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
            InteractionMesh->SetCanEverAffectNavigation(false);
        }
    }
    MissionInteractables.Add(Interaction);
    return Interaction;
}

void ABMGreyboxDirector::SpawnMissionInteractables()
{
    if (!HasAuthority())
    {
        return;
    }

    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);

    if (ABMInteractableActor* IntelConsole = SpawnInteraction(
            TEXT("IntelConsole"),
            NSLOCTEXT("BlackMeridian", "IntelPrompt", "HOLD E  DOWNLOAD SIGNAL ARCHIVE"),
            EBMInteractionType::Hold,
            EBMInteractionAction::MissionEvent,
            Level.IntelLocation,
            FVector(0.35f, 0.35f, 0.7f),
            1.5f))
    {
        IntelConsole->bDestroyAfterInteraction = false;
    }

    if (ABMInteractableActor* BreachDoor = SpawnInteraction(
            TEXT("RelayBreach"),
            NSLOCTEXT("BlackMeridian", "BreachPrompt", "HOLD E  BREACH RELAY HOUSE"),
            EBMInteractionType::Hold,
            EBMInteractionAction::BreachDoor,
            // Put the interaction face on the checkpoint-facing side of the
            // solid portal. The orange beacon remains the destination at the
            // portal center, while the player can see and trace the door
            // before entering its opening. A fixed south offset put some
            // rotated layouts just outside the interaction gate when the
            // collision shell correctly stopped the pawn at the portal.
            Level.BreachLocation
                + (Level.CheckpointLocation - Level.BreachLocation).GetSafeNormal2D() * 80.0f,
            FVector(0.8f, 0.15f, 1.4f),
            1.25f))
    {
        BreachDoor->bActsAsDoor = true;
    }

    if (ABMInteractableActor* MountedWeapon = SpawnInteraction(
            TEXT("MountedVantageGun"),
            NSLOCTEXT("BlackMeridian", "MountedWeaponPrompt", "PRESS E  USE VANTAGE GUN"),
            EBMInteractionType::Tap,
            EBMInteractionAction::MountedWeapon,
            Level.CheckpointLocation + FVector(0.0f, -180.0f, 55.0f),
            FVector(0.8f, 0.3f, 0.25f)))
    {
        MountedWeapon->bDestroyAfterInteraction = false;
    }

    if (ABMInteractableActor* Ladder = SpawnInteraction(
            TEXT("RelayLadder"),
            NSLOCTEXT("BlackMeridian", "LadderPrompt", "PRESS E  CLIMB LADDER"),
            EBMInteractionType::Tap,
            EBMInteractionAction::Ladder,
            // Keep the optional ladder on the flank of the breach face. A
            // ladder placed directly between the checkpoint approach and the
            // breach door occludes the door's interaction trace on several
            // structurally different campaign layouts.
            Level.BreachLocation
                + (FVector(-(Level.ExtractionLocation - Level.StartLocation).GetSafeNormal2D().Y,
                           (Level.ExtractionLocation - Level.StartLocation).GetSafeNormal2D().X,
                           0.0f) * 260.0f)
                + FVector(0.0f, 0.0f, 60.0f),
            FVector(0.18f, 0.7f, 2.0f)))
    {
        Ladder->bDestroyAfterInteraction = false;
    }

    if (ABMInteractableActor* Ammo = SpawnInteraction(
            TEXT("CheckpointAmmo"),
            NSLOCTEXT("BlackMeridian", "AmmoPrompt", "PRESS E  COLLECT AMMUNITION"),
            EBMInteractionType::Tap,
            EBMInteractionAction::PickupAmmo,
            Level.AmmoPickupLocation,
            FVector(0.3f, 0.3f, 0.2f)))
    {
        Ammo->PickupAmmoAmount = 60 + static_cast<int32>(CampaignLevel) * 15;
        Ammo->bDestroyAfterInteraction = true;
    }

    if (ABMInteractableActor* Ordnance = SpawnInteraction(
            TEXT("CheckpointOrdnance"),
            NSLOCTEXT("BlackMeridian", "OrdnancePrompt", "WALK NEAR  RESTOCK FRAG + VEIL"),
            EBMInteractionType::Tap,
            EBMInteractionAction::PickupThrowable,
            Level.ThrowablePickupLocation,
            FVector(1.0f, 1.0f, 1.0f)))
    {
        Ordnance->PickupFragAmount = 1;
        Ordnance->PickupVeilAmount = 1;
        Ordnance->bDestroyAfterInteraction = true;
        Ordnance->SetThrowablePickupPresentation(EBMWeaponId::FragGrenade);
        Ordnance->SetPresentationColor(FLinearColor(1.0f, 0.22f, 0.04f));
    }

    if (ABMInteractableActor* WeaponPickup = SpawnInteraction(
            TEXT("RookPickup"),
            NSLOCTEXT("BlackMeridian", "WeaponPrompt", "WALK NEAR  TAKE WEAPON + AMMO"),
            EBMInteractionType::Tap,
            EBMInteractionAction::PickupWeapon,
            Level.WeaponPickupLocation,
            FVector::OneVector))
    {
        WeaponPickup->PickupWeaponId = Level.PickupWeaponId;
        WeaponPickup->PickupAmmoAmount = 90 + static_cast<int32>(CampaignLevel) * 20;
        WeaponPickup->bDestroyAfterInteraction = true;
        WeaponPickup->SetWeaponPickupPresentation(Level.PickupWeaponId);
    }

    const FName AttachmentId = [&]()
    {
        switch (static_cast<int32>(CampaignLevel) % 5)
        {
        case 0:
            return FName(TEXT("Optic_Rangefinder"));
        case 1:
            return FName(TEXT("Muzzle_Suppressor"));
        case 2:
            return FName(TEXT("Grip_Stabilizer"));
        case 3:
            return FName(TEXT("Magazine_Extended"));
        default:
            return FName(TEXT("Stock_Reinforced"));
        }
    }();
    if (ABMInteractableActor* AttachmentPickup = SpawnInteraction(
            TEXT("CheckpointAttachment"),
            NSLOCTEXT("BlackMeridian", "AttachmentPrompt", "WALK NEAR  TAKE WEAPON ATTACHMENT"),
            EBMInteractionType::Tap,
            EBMInteractionAction::PickupAttachment,
            Level.CheckpointLocation + FVector(180.0f, -140.0f, -15.0f),
            FVector(0.72f, 0.72f, 0.72f)))
    {
        AttachmentPickup->PickupAttachmentId = AttachmentId;
        AttachmentPickup->bDestroyAfterInteraction = true;
        AttachmentPickup->SetAttachmentPickupPresentation(AttachmentId);
        AttachmentPickup->SetPresentationColor(FLinearColor(0.06f, 0.62f, 0.72f));
    }

    if (ABMInteractableActor* PlantSite = SpawnInteraction(
            TEXT("PlantSignalJammer"),
            NSLOCTEXT("BlackMeridian", "PlantPrompt", "HOLD E  PLANT SIGNAL JAMMER"),
            EBMInteractionType::Hold,
            EBMInteractionAction::PlantEquipment,
            Level.OpenCombatLocation + FVector(0.0f, 220.0f, -80.0f),
            FVector(0.35f, 0.35f, 0.15f),
            2.0f))
    {
        PlantSite->bDestroyAfterInteraction = true;
    }

    SpawnInteraction(
        TEXT("ExtractionBoat"),
        NSLOCTEXT("BlackMeridian", "ExtractionPrompt", "HOLD E  SIGNAL EXTRACTION BOAT"),
        EBMInteractionType::Hold,
        EBMInteractionAction::MissionEvent,
        Level.ExtractionLocation,
        FVector(0.7f, 0.7f, 0.18f),
        1.5f);
}

void ABMGreyboxDirector::SpawnLighting()
{
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    EnvironmentPracticalLights.Reset();

    // The Engine Entry map already carries a directional light. Reuse the
    // first one and disable extras so the player never sees Unreal's
    // multiple-directional-light warning over the mission HUD.
    ADirectionalLight* Sun = nullptr;
    for (TActorIterator<ADirectionalLight> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        if (!Sun)
        {
            Sun = *Iterator;
        }
        else if (ADirectionalLight* ExtraSun = *Iterator; ExtraSun->GetLightComponent())
        {
            ExtraSun->GetLightComponent()->SetVisibility(false);
            ExtraSun->GetLightComponent()->SetIntensity(0.0f);
            if (UDirectionalLightComponent* ExtraSunComponent = Cast<UDirectionalLightComponent>(ExtraSun->GetLightComponent()))
            {
                ExtraSunComponent->SetForwardShadingPriority(0);
            }
            ExtraSun->SetActorHiddenInGame(true);
        }
    }
    if (!Sun)
    {
        Sun = GetWorld()->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector::ZeroVector, FRotator(-45.0f, -35.0f, 0.0f), SpawnParameters);
    }
    if (Sun && Sun->GetLightComponent())
    {
        Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        // Authored map shells may carry a neutral editor light rotation. Set
        // the runtime moon angle explicitly so wall panels and cover receive
        // a readable key instead of leaving the horizon in a black band.
        Sun->SetActorRotation(FRotator(-38.0f, -32.0f, 0.0f));
        // Runtime geometry has no baked lightmap.  A restrained but positive
        // key is necessary on the packaged Metal path; 0.52 left the
        // shadow-facing side of the combat route unreadable when the Entry
        // map's original light was absent.
        // Keep the night key directional, but lift the shadow floor enough
        // that an advancing Vanta silhouette and the cover edge remain
        // readable on the Mac Metal path. The key is still substantially
        // cooler and dimmer than daylight; the extra energy is intentional
        // because this runtime scene has no baked Lightmass contribution.
        Sun->GetLightComponent()->SetIntensity(1.25f);
        Sun->GetLightComponent()->SetLightColor(FLinearColor(0.78f, 0.87f, 1.0f));
        if (UDirectionalLightComponent* SunComponent = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
        {
            SunComponent->SetForwardShadingPriority(1);
            // The atmosphere actor below consumes this as its primary sun.
            // Without the explicit link, the runtime sky remains an unlit
            // black backdrop even though the scene's geometry is illuminated.
            SunComponent->SetAtmosphereSunLight(true);
            SunComponent->SetAtmosphereSunLightIndex(0);
        }
        Sun->GetLightComponent()->SetCastShadows(true);
    }
    EnvironmentSun = Sun;

    // A low, shadowless opposite fill keeps the night palette readable on
    // the non-sun-facing sides of the runtime walls. It is intentionally
    // weaker and bluer than the key so cover still has a clear lit/shadow
    // break instead of becoming a flat daytime wash.
    if (APointLight* Fill = GetWorld()->SpawnActor<APointLight>(APointLight::StaticClass(), FVector(0.0f, 0.0f, 650.0f), FRotator::ZeroRotator, SpawnParameters))
    {
        EnvironmentFillLight = Fill;
        EnvironmentPracticalLights.Add(Fill);
        Fill->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Fill->GetLightComponent()->SetIntensity(12000.0f);
        Fill->GetLightComponent()->SetLightColor(FLinearColor(0.30f, 0.45f, 0.68f));
        Fill->GetLightComponent()->SetCastShadows(false);
        if (ULocalLightComponent* LocalLight = Cast<ULocalLightComponent>(Fill->GetLightComponent()))
        {
            LocalLight->SetAttenuationRadius(4200.0f);
        }
    }

    // Storm profiles get one cheap, shadowless world flash. It is kept dark
    // until the weather tick schedules a strike, so clear, snow, desert,
    // forest, underground, and interior levels pay no visible-light cost.
    if (APointLight* Lightning = GetWorld()->SpawnActor<APointLight>(
            APointLight::StaticClass(),
            FVector(650.0f, 0.0f, 1050.0f),
            FRotator::ZeroRotator,
            SpawnParameters))
    {
        EnvironmentLightningLight = Lightning;
        Lightning->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Lightning->GetLightComponent()->SetIntensity(0.0f);
        Lightning->GetLightComponent()->SetLightColor(FLinearColor(0.72f, 0.84f, 1.0f));
        Lightning->GetLightComponent()->SetCastShadows(false);
        Lightning->GetLightComponent()->SetVisibility(false);
        if (ULocalLightComponent* LocalLight = Cast<ULocalLightComponent>(Lightning->GetLightComponent()))
        {
            LocalLight->SetAttenuationRadius(4200.0f);
        }
    }

    if (ASkyLight* Sky = GetWorld()->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters))
    {
        EnvironmentSkyLight = Sky;
        Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Sky->GetLightComponent()->SetIntensity(1.10f);
        Sky->GetLightComponent()->SetLightColor(FLinearColor(0.38f, 0.52f, 0.72f));
        // A runtime sky capture can be effectively black on a source-only
        // night map. Give the lower hemisphere a restrained blue floor so
        // shadow-facing cover and character legs remain readable without
        // flattening the authored key/fill direction.
        Sky->GetLightComponent()->bLowerHemisphereIsBlack = false;
        Sky->GetLightComponent()->SetLowerHemisphereColor(FLinearColor(0.22f, 0.29f, 0.38f));
        Sky->GetLightComponent()->RecaptureSky();
    }

    // The Entry map has no authored sky material. A SkyAtmosphere supplies a
    // real horizon and aerial perspective for exterior operations, so the
    // player reads a place rather than a black void beyond the collision shell.
    // It is hidden for enclosed themes in ApplyEnvironmentTheme(), where the
    // ceiling/roof silhouette is the correct visual boundary.
    if (ASkyAtmosphere* Atmosphere = GetWorld()->SpawnActor<ASkyAtmosphere>(
            ASkyAtmosphere::StaticClass(),
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            SpawnParameters))
    {
        EnvironmentSkyAtmosphere = Atmosphere;
        if (USkyAtmosphereComponent* AtmosphereComponent = Atmosphere->GetComponent())
        {
            AtmosphereComponent->TransformMode = ESkyAtmosphereTransformMode::PlanetTopAtAbsoluteWorldOrigin;
            AtmosphereComponent->GroundAlbedo = FColor(86, 92, 104);
            AtmosphereComponent->SkyLuminanceFactor = FLinearColor(0.78f, 0.86f, 1.0f, 1.0f);
            AtmosphereComponent->SkyAndAerialPerspectiveLuminanceFactor = FLinearColor(0.72f, 0.82f, 1.0f, 1.0f);
            AtmosphereComponent->MieScatteringScale = 0.0028f;
            AtmosphereComponent->MieAnisotropy = 0.72f;
            AtmosphereComponent->SetVisibility(true, true);
        }
    }

    if (AExponentialHeightFog* Fog = GetWorld()->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FVector(650.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters))
    {
        EnvironmentFog = Fog;
        if (UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
        {
            // Fog should establish depth, not erase the first contact. A
            // lower density and later start distance keep distant beacons
            // atmospheric while leaving enemy silhouettes legible.
            FogComponent->SetFogDensity(0.0010f);
            FogComponent->SetFogHeightFalloff(0.22f);
            FogComponent->SetFogInscatteringColor(FLinearColor(0.018f, 0.035f, 0.065f));
            FogComponent->SetDirectionalInscatteringColor(FLinearColor(0.03f, 0.08f, 0.12f));
            FogComponent->SetDirectionalInscatteringExponent(4.0f);
            FogComponent->SetStartDistance(720.0f);
            FogComponent->SetFogMaxOpacity(0.12f);
        }
    }

    if (APostProcessVolume* LookVolume = GetWorld()->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters))
    {
        EnvironmentPostProcess = LookVolume;
        LookVolume->bUnbound = true;
        LookVolume->bEnabled = true;
        LookVolume->Priority = 2.0f;
        LookVolume->BlendWeight = 1.0f;
        LookVolume->Settings.bOverride_ColorSaturation = true;
        LookVolume->Settings.ColorSaturation = FVector4(1.04f, 1.02f, 0.98f, 1.0f);
        LookVolume->Settings.bOverride_ColorContrast = true;
        LookVolume->Settings.ColorContrast = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        LookVolume->Settings.bOverride_ColorGain = true;
        LookVolume->Settings.ColorGain = FVector4(1.04f, 1.04f, 1.06f, 1.0f);
        LookVolume->Settings.bOverride_ColorGamma = true;
        LookVolume->Settings.ColorGamma = FVector4(1.12f, 1.11f, 1.10f, 1.0f);
        LookVolume->Settings.bOverride_BloomIntensity = true;
        LookVolume->Settings.BloomIntensity = 0.08f;
        LookVolume->Settings.bOverride_AutoExposureBias = true;
        // Auto exposure is disabled project-wide for deterministic probes.
        // A small fixed compensation keeps the first-person combat lane
        // readable on the Metal compatibility renderer without washing out
        // the authored night grade or changing HUD/muzzle-flash values.
        LookVolume->Settings.AutoExposureBias = 0.22f;
        LookVolume->Settings.bOverride_VignetteIntensity = true;
        LookVolume->Settings.VignetteIntensity = 0.06f;
        // Combat readability wins over a cinematic blur on this runtime
        // compatibility path. The camera already has authored recoil,
        // damage kick, muzzle flash, tracers, and hit feedback; motion blur
        // stacks on those cues and turns the target line into an unreadable
        // smear during automatic fire.
        LookVolume->Settings.bOverride_MotionBlurAmount = true;
        LookVolume->Settings.MotionBlurAmount = 0.0f;
    }

    for (const FVector& Location : {FVector(-900.0f, -800.0f, 520.0f), FVector(0.0f, 0.0f, 500.0f), FVector(1400.0f, -500.0f, 420.0f), FVector(2400.0f, 600.0f, 420.0f)})
    {
        if (APointLight* Light = GetWorld()->SpawnActor<APointLight>(APointLight::StaticClass(), Location, FRotator::ZeroRotator, SpawnParameters))
        {
            EnvironmentPracticalLights.Add(Light);
            Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
            Light->GetLightComponent()->SetIntensity(1050.0f);
            Light->GetLightComponent()->SetLightColor(FLinearColor(0.38f, 0.52f, 0.72f));
            Light->GetLightComponent()->SetCastShadows(false);
            if (ULocalLightComponent* LocalLight = Cast<ULocalLightComponent>(Light->GetLightComponent()))
            {
                LocalLight->SetAttenuationRadius(3000.0f);
            }
        }
    }

    // Practical fill lights give the playable lanes a readable value range:
    // cool seawall fill, warm relay-house bounce, and a red warning wash at
    // the far route. They are intentionally shadowless and movable so the
    // runtime-generated scene remains cheap on the compatibility renderer.
    const auto AddPracticalFill = [this, &SpawnParameters](
        const FVector& Location,
        const FLinearColor& Color,
        const float Intensity,
        const float Radius) -> APointLight*
    {
        if (APointLight* Light = GetWorld()->SpawnActor<APointLight>(APointLight::StaticClass(), Location, FRotator::ZeroRotator, SpawnParameters))
        {
            EnvironmentPracticalLights.Add(Light);
            Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
            Light->GetLightComponent()->SetIntensity(Intensity);
            Light->GetLightComponent()->SetLightColor(Color);
            Light->GetLightComponent()->SetCastShadows(false);
            if (ULocalLightComponent* LocalLight = Cast<ULocalLightComponent>(Light->GetLightComponent()))
            {
                LocalLight->SetAttenuationRadius(Radius);
            }
            return Light;
        }
        return nullptr;
    };
    AddPracticalFill(FVector(-850.0f, -740.0f, 300.0f), FLinearColor(0.10f, 0.34f, 0.56f), 620.0f, 1150.0f);
    AddPracticalFill(FVector(760.0f, 620.0f, 330.0f), FLinearColor(0.78f, 0.36f, 0.12f), 480.0f, 950.0f);
    AddPracticalFill(FVector(1750.0f, 760.0f, 360.0f), FLinearColor(0.96f, 0.55f, 0.20f), 560.0f, 1050.0f);
    AddPracticalFill(FVector(2700.0f, -260.0f, 320.0f), FLinearColor(0.66f, 0.08f, 0.10f), 420.0f, 980.0f);

    // Keep the two playable combat pockets legible when the player turns away
    // from the global fill. These low-energy pools separate cool approach
    // steel from the amber/red engagement space without washing out the
    // night grade or the HUD contrast.
    AddPracticalFill(FVector(390.0f, -180.0f, 260.0f), FLinearColor(0.10f, 0.30f, 0.42f), 520.0f, 700.0f);
    AddPracticalFill(FVector(1450.0f, 40.0f, 280.0f), FLinearColor(0.52f, 0.14f, 0.07f), 560.0f, 820.0f);

    // The utility-bay facade is an actual visual destination now, so give its
    // lower panels a restrained practical wash. These shadowless pools keep
    // the new architecture visible on the Metal compatibility renderer while
    // preserving the darker combat lanes between bays.
    for (const FVector& FacadeLight : {
             FVector(-980.0f, 1320.0f, 330.0f),
             FVector(520.0f, 1320.0f, 330.0f),
             FVector(2020.0f, 1320.0f, 330.0f),
             FVector(-230.0f, -1320.0f, 330.0f),
             FVector(1270.0f, -1320.0f, 330.0f),
             FVector(2770.0f, -1320.0f, 330.0f)})
    {
        const bool bWarmBay = FacadeLight.Y < 0.0f && FacadeLight.X > 500.0f;
        AddPracticalFill(
            FacadeLight,
            bWarmBay ? FLinearColor(0.72f, 0.28f, 0.08f) : FLinearColor(0.08f, 0.34f, 0.46f),
            360.0f,
            560.0f);
    }

    // SableQuay's hero spaces need identifiable practical sources rather than
    // one undifferentiated world wash. These three lights are deliberately
    // shadowless and low-energy; they reveal the relay facade, the split
    // combat cover, and the raised extraction platform without changing any
    // collision or adding a second directional-light path. They remain hidden
    // on the other nineteen operations and are repositioned when the selected
    // campaign level is refreshed.
    const FLinearColor HeroLightColors[] = {
        FLinearColor(0.04f, 0.62f, 0.56f),
        FLinearColor(0.92f, 0.32f, 0.08f),
        FLinearColor(0.96f, 0.68f, 0.20f)};
    const float HeroLightIntensities[] = {420.0f, 300.0f, 380.0f};
    const float HeroLightRadii[] = {520.0f, 560.0f, 500.0f};
    for (int32 HeroIndex = 0; HeroIndex < UE_ARRAY_COUNT(HeroLightColors); ++HeroIndex)
    {
        if (APointLight* HeroLight = AddPracticalFill(
                FVector(0.0f, 0.0f, -1000.0f),
                HeroLightColors[HeroIndex],
                HeroLightIntensities[HeroIndex],
                HeroLightRadii[HeroIndex]))
        {
            HeroLight->Tags.Add(FName(*FString::Printf(TEXT("ArtDirected.SableHero.%d"), HeroIndex)));
            HeroLight->SetActorHiddenInGame(true);
            HeroLight->GetLightComponent()->SetVisibility(false);
        }
    }

    // NightglassCore has a different lighting language from the outdoor
    // campaign: cool service lamps mark the tunnel rhythm and a restrained
    // amber source marks the breach side. Keep these lights opt-in and
    // shadowless so the Mac compatibility renderer stays inexpensive while
    // characters, cover edges, and the new bulkheads remain readable.
    const FLinearColor CoreLightColors[] = {
        FLinearColor(0.06f, 0.52f, 0.50f),
        FLinearColor(0.08f, 0.34f, 0.56f),
        FLinearColor(0.90f, 0.28f, 0.08f),
        FLinearColor(0.48f, 0.16f, 0.64f),
        FLinearColor(0.08f, 0.58f, 0.48f)};
    const float CoreLightIntensities[] = {760.0f, 680.0f, 900.0f, 620.0f, 780.0f};
    for (int32 CoreLightIndex = 0; CoreLightIndex < UE_ARRAY_COUNT(CoreLightColors); ++CoreLightIndex)
    {
        if (APointLight* CoreLight = AddPracticalFill(
                FVector(0.0f, 0.0f, -1000.0f),
                CoreLightColors[CoreLightIndex],
                CoreLightIntensities[CoreLightIndex],
                760.0f))
        {
            CoreLight->Tags.Add(FName(*FString::Printf(TEXT("ArtDirected.NightglassCore.%d"), CoreLightIndex)));
            CoreLight->SetActorHiddenInGame(true);
            CoreLight->GetLightComponent()->SetVisibility(false);
        }
    }

    ApplyEnvironmentTheme();
}

void ABMGreyboxDirector::ApplyEnvironmentTheme()
{
    if (!GetWorld())
    {
        return;
    }

    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
    const FLinearColor SkyColor = FLinearColor::FromSRGBColor(Level.EnvironmentSkyColor);
    const FLinearColor FogColor = FLinearColor::FromSRGBColor(Level.EnvironmentFogColor);
    const FLinearColor GroundColor = FLinearColor::FromSRGBColor(Level.EnvironmentGroundColor);
    const FLinearColor WeatherColor = FLinearColor::FromSRGBColor(Level.EnvironmentWeatherColor);
    const FLinearColor AccentColor = FLinearColor::FromSRGBColor(Level.EnvironmentAccentColor);

    const bool bEnclosedTheme = Level.bInteriorEnvironment
        || Level.EnvironmentTheme == EBMEnvironmentTheme::Underground;
    if (EnvironmentSkyAtmosphere)
    {
        EnvironmentSkyAtmosphere->SetActorHiddenInGame(bEnclosedTheme);
        if (USkyAtmosphereComponent* AtmosphereComponent = EnvironmentSkyAtmosphere->GetComponent())
        {
            AtmosphereComponent->SetVisibility(!bEnclosedTheme, true);
            AtmosphereComponent->SkyLuminanceFactor = FLinearColor::LerpUsingHSV(
                FLinearColor(0.45f, 0.54f, 0.72f, 1.0f),
                SkyColor,
                0.42f);
            AtmosphereComponent->SkyAndAerialPerspectiveLuminanceFactor = FLinearColor::LerpUsingHSV(
                FLinearColor(0.40f, 0.50f, 0.70f, 1.0f),
                SkyColor,
                0.34f);
            AtmosphereComponent->GroundAlbedo = FColor(
                FMath::Clamp(FMath::RoundToInt(Level.EnvironmentGroundColor.R * 0.72f), 24, 190),
                FMath::Clamp(FMath::RoundToInt(Level.EnvironmentGroundColor.G * 0.72f), 24, 190),
                FMath::Clamp(FMath::RoundToInt(Level.EnvironmentGroundColor.B * 0.72f), 24, 210));
        }
    }

    FRotator SunRotation(-38.0f, -32.0f, 0.0f);
    FLinearColor SunColor(0.78f, 0.87f, 1.0f);
    FLinearColor FillColor = SkyColor;
    float PracticalLightScale = 1.0f;
    switch (Level.EnvironmentTheme)
    {
    case EBMEnvironmentTheme::CoastalStorm:
        SunRotation = FRotator(-52.0f, 35.0f, 0.0f);
        SunColor = FLinearColor(0.48f, 0.62f, 0.82f);
        FillColor = FLinearColor(0.18f, 0.32f, 0.55f);
        PracticalLightScale = 0.72f;
        break;
    case EBMEnvironmentTheme::SnowIce:
        SunRotation = FRotator(-34.0f, 118.0f, 0.0f);
        SunColor = FLinearColor(0.82f, 0.92f, 1.0f);
        FillColor = FLinearColor(0.48f, 0.66f, 0.90f);
        PracticalLightScale = 0.94f;
        break;
    case EBMEnvironmentTheme::Desert:
        SunRotation = FRotator(-22.0f, -128.0f, 0.0f);
        SunColor = FLinearColor(1.0f, 0.73f, 0.43f);
        FillColor = FLinearColor(0.62f, 0.34f, 0.18f);
        PracticalLightScale = 1.28f;
        break;
    case EBMEnvironmentTheme::Forest:
        SunRotation = FRotator(-31.0f, 12.0f, 0.0f);
        SunColor = FLinearColor(0.62f, 0.86f, 0.68f);
        FillColor = FLinearColor(0.16f, 0.36f, 0.27f);
        PracticalLightScale = 0.86f;
        break;
    case EBMEnvironmentTheme::Underground:
        SunRotation = FRotator(-72.0f, 0.0f, 0.0f);
        SunColor = FLinearColor(0.40f, 0.34f, 0.62f);
        FillColor = FLinearColor(0.24f, 0.20f, 0.46f);
        PracticalLightScale = 0.92f;
        break;
    case EBMEnvironmentTheme::Interior:
        SunRotation = FRotator(-58.0f, 82.0f, 0.0f);
        SunColor = FLinearColor(0.78f, 0.47f, 0.30f);
        FillColor = FLinearColor(0.38f, 0.20f, 0.16f);
        PracticalLightScale = 0.78f;
        break;
    case EBMEnvironmentTheme::OpenDry:
    default:
        SunRotation = FRotator(-24.0f, -52.0f, 0.0f);
        SunColor = FLinearColor(1.0f, 0.84f, 0.60f);
        FillColor = FLinearColor(0.28f, 0.42f, 0.58f);
        PracticalLightScale = 1.10f;
        break;
    }

    if (EnvironmentSun && EnvironmentSun->GetLightComponent())
    {
        SunRotation.Yaw += Level.EnvironmentSunYawOffset;
        EnvironmentSun->SetActorRotation(SunRotation);
        EnvironmentSun->GetLightComponent()->SetIntensity(Level.EnvironmentSunIntensity);
        EnvironmentSun->GetLightComponent()->SetLightColor(SunColor);
        EnvironmentSun->GetLightComponent()->SetVisibility(true);
    }

    if (EnvironmentFillLight && EnvironmentFillLight->GetLightComponent())
    {
        EnvironmentFillLight->GetLightComponent()->SetIntensity(12000.0f * PracticalLightScale);
        EnvironmentFillLight->GetLightComponent()->SetLightColor(FillColor);
    }

    // The light rig follows the selected route instead of keeping the same
    // four pools in the same world coordinates for every operation. This is
    // especially important once the visible macro setting replaces the
    // shared utility facade.
    const FVector ActiveRouteDirection = (Level.ExtractionLocation - Level.StartLocation).GetSafeNormal2D();
    const FVector ActiveRouteSide(-ActiveRouteDirection.Y, ActiveRouteDirection.X, 0.0f);
    const FVector ActiveRouteMid = FMath::Lerp(Level.StartLocation, Level.ExtractionLocation, 0.48f);
    if (EnvironmentFillLight)
    {
        const float FillHeight = Level.EnvironmentTheme == EBMEnvironmentTheme::Underground ? 360.0f : 650.0f;
        EnvironmentFillLight->SetActorLocation(ActiveRouteMid + FVector(0.0f, 0.0f, FillHeight));
    }
    const float RigSideOffset = Level.EnvironmentTheme == EBMEnvironmentTheme::Underground
        ? 440.0f
        : (Level.bInteriorEnvironment ? 360.0f : 560.0f);
    const float RigHeight = Level.EnvironmentTheme == EBMEnvironmentTheme::Underground
        ? 340.0f
        : (Level.bInteriorEnvironment ? 430.0f : 360.0f);
    const FVector RigLocations[] = {
        Level.StartLocation + ActiveRouteSide * RigSideOffset + FVector(0.0f, 0.0f, RigHeight),
        Level.CheckpointLocation - ActiveRouteSide * RigSideOffset + FVector(0.0f, 0.0f, RigHeight),
        Level.BreachLocation + ActiveRouteSide * (RigSideOffset * 0.72f) + FVector(0.0f, 0.0f, RigHeight + 70.0f),
        Level.IntelLocation - ActiveRouteSide * (RigSideOffset * 0.72f) + FVector(0.0f, 0.0f, RigHeight + 70.0f),
        Level.OpenCombatLocation + ActiveRouteSide * (RigSideOffset * 0.56f) + FVector(0.0f, 0.0f, RigHeight + 90.0f),
        Level.ExtractionLocation - ActiveRouteSide * (RigSideOffset * 0.56f) + FVector(0.0f, 0.0f, RigHeight + 90.0f)};
    for (int32 RigIndex = 0; RigIndex < UE_ARRAY_COUNT(RigLocations); ++RigIndex)
    {
        const int32 LightIndex = RigIndex + 1; // slot zero is EnvironmentFillLight
        if (EnvironmentPracticalLights.IsValidIndex(LightIndex) && IsValid(EnvironmentPracticalLights[LightIndex]))
        {
            EnvironmentPracticalLights[LightIndex]->SetActorLocation(RigLocations[RigIndex]);
        }
    }

    for (int32 Index = 0; Index < EnvironmentPracticalLights.Num(); ++Index)
    {
        APointLight* Light = EnvironmentPracticalLights[Index];
        if (!IsValid(Light) || Light == EnvironmentFillLight || !Light->GetLightComponent())
        {
            continue;
        }

        int32 NightglassCoreIndex = INDEX_NONE;
        for (int32 CandidateIndex = 0; CandidateIndex < 5; ++CandidateIndex)
        {
            if (Light->Tags.Contains(FName(*FString::Printf(TEXT("ArtDirected.NightglassCore.%d"), CandidateIndex))))
            {
                NightglassCoreIndex = CandidateIndex;
                break;
            }
        }
        if (NightglassCoreIndex != INDEX_NONE)
        {
            const bool bNightglassCore = Level.Level == EBMCampaignLevel::NightglassCore;
            Light->SetActorHiddenInGame(!bNightglassCore);
            Light->GetLightComponent()->SetVisibility(bNightglassCore);
            if (bNightglassCore)
            {
                const FVector CoreLightLocations[] = {
                    Level.InsertionLocation + ActiveRouteSide * 420.0f + FVector(0.0f, 0.0f, 360.0f),
                    Level.CheckpointLocation - ActiveRouteSide * 420.0f + FVector(0.0f, 0.0f, 360.0f),
                    Level.BreachLocation + ActiveRouteSide * 420.0f + FVector(0.0f, 0.0f, 360.0f),
                    Level.IntelLocation - ActiveRouteSide * 380.0f + FVector(0.0f, 0.0f, 340.0f),
                    Level.OpenCombatLocation + ActiveRouteSide * 380.0f + FVector(0.0f, 0.0f, 340.0f)};
                const FLinearColor CoreLightColors[] = {
                    FLinearColor(0.06f, 0.52f, 0.50f),
                    FLinearColor(0.08f, 0.34f, 0.56f),
                    FLinearColor(0.90f, 0.28f, 0.08f),
                    FLinearColor(0.48f, 0.16f, 0.64f),
                    FLinearColor(0.08f, 0.58f, 0.48f)};
                const float CoreLightIntensities[] = {760.0f, 680.0f, 900.0f, 620.0f, 780.0f};
                Light->SetActorLocation(CoreLightLocations[NightglassCoreIndex]);
                Light->GetLightComponent()->SetLightColor(CoreLightColors[NightglassCoreIndex]);
                Light->GetLightComponent()->SetIntensity(CoreLightIntensities[NightglassCoreIndex]);
            }
            continue;
        }

        int32 SableHeroIndex = INDEX_NONE;
        for (int32 CandidateIndex = 0; CandidateIndex < 3; ++CandidateIndex)
        {
            if (Light->Tags.Contains(FName(*FString::Printf(TEXT("ArtDirected.SableHero.%d"), CandidateIndex))))
            {
                SableHeroIndex = CandidateIndex;
                break;
            }
        }
        if (SableHeroIndex != INDEX_NONE)
        {
            const bool bSableQuay = Level.Level == EBMCampaignLevel::SableQuay;
            Light->SetActorHiddenInGame(!bSableQuay);
            Light->GetLightComponent()->SetVisibility(bSableQuay);
            if (bSableQuay)
            {
                const FVector RelayHouseCenter =
                    Level.CheckpointLocation + ActiveRouteSide * 900.0f + ActiveRouteDirection * 160.0f;
                const FVector CombatCoverCenter =
                    Level.OpenCombatLocation + ActiveRouteSide * 430.0f;
                const FVector GantryPlatform =
                    Level.ExtractionLocation + ActiveRouteSide * 520.0f;
                const FVector HeroLocations[] = {
                    RelayHouseCenter - ActiveRouteSide * 250.0f + FVector(0.0f, 0.0f, 300.0f),
                    CombatCoverCenter - ActiveRouteSide * 120.0f + FVector(0.0f, 0.0f, 280.0f),
                    GantryPlatform + FVector(0.0f, 0.0f, 320.0f)};
                const FLinearColor HeroColors[] = {
                    FLinearColor(0.04f, 0.62f, 0.56f),
                    FLinearColor(0.92f, 0.32f, 0.08f),
                    FLinearColor(0.96f, 0.68f, 0.20f)};
                const float HeroIntensities[] = {420.0f, 300.0f, 380.0f};
                if (HeroLocations[SableHeroIndex].IsNearlyZero() == false)
                {
                    Light->SetActorLocation(HeroLocations[SableHeroIndex]);
                }
                Light->GetLightComponent()->SetIntensity(HeroIntensities[SableHeroIndex]);
                Light->GetLightComponent()->SetLightColor(HeroColors[SableHeroIndex]);
            }
            continue;
        }

        const bool bAccentPool = Index % 3 == 0;
        const FLinearColor PoolColor = bAccentPool
            ? FLinearColor::LerpUsingHSV(FogColor, AccentColor, 0.38f)
            : FLinearColor::LerpUsingHSV(SkyColor, GroundColor, 0.28f);
        const float BaseIntensity = Index < 5 ? 1800.0f : 980.0f;
        Light->GetLightComponent()->SetIntensity(BaseIntensity * PracticalLightScale);
        Light->GetLightComponent()->SetLightColor(PoolColor);
    }

    if (EnvironmentSkyLight && EnvironmentSkyLight->GetLightComponent())
    {
        const float SkyHeight = Level.EnvironmentTheme == EBMEnvironmentTheme::Underground ? 360.0f : 540.0f;
        EnvironmentSkyLight->SetActorLocation(ActiveRouteMid + FVector(0.0f, 0.0f, SkyHeight));
        // Theme profiles still control the mood, but no operation may lose
        // the ambient floor required to read characters and cover on a
        // runtime-lit map with no baked Lightmass data.
        EnvironmentSkyLight->GetLightComponent()->SetIntensity(
            FMath::Max(
                Level.EnvironmentSkyIntensity * 1.55f,
                Level.EnvironmentTheme == EBMEnvironmentTheme::Underground ? 0.95f : 0.82f));
        EnvironmentSkyLight->GetLightComponent()->SetLightColor(SkyColor);
        EnvironmentSkyLight->GetLightComponent()->bLowerHemisphereIsBlack = false;
        const bool bUnderground = Level.EnvironmentTheme == EBMEnvironmentTheme::Underground;
        EnvironmentSkyLight->GetLightComponent()->SetLowerHemisphereColor(
            bUnderground
                ? FLinearColor(0.24f, 0.27f, 0.34f)
                : FLinearColor(
                    FMath::Max(GroundColor.R * 1.20f, 0.18f),
                    FMath::Max(GroundColor.G * 1.20f, 0.22f),
                    FMath::Max(GroundColor.B * 1.20f, 0.28f),
                    1.0f));
        EnvironmentSkyLight->GetLightComponent()->RecaptureSky();
    }

    if (EnvironmentFog && EnvironmentFog->GetComponent())
    {
        EnvironmentFog->SetActorLocation(ActiveRouteMid);
        UExponentialHeightFogComponent* FogComponent = EnvironmentFog->GetComponent();
        const bool bUnderground = Level.EnvironmentTheme == EBMEnvironmentTheme::Underground;
        FogComponent->SetFogDensity(Level.EnvironmentFogDensity * (bUnderground ? 0.52f : 0.68f));
        FogComponent->SetFogHeightFalloff(Level.EnvironmentFogHeightFalloff);
        FogComponent->SetFogInscatteringColor(FogColor);
        FogComponent->SetDirectionalInscatteringColor(WeatherColor * 0.32f);
        FogComponent->SetDirectionalInscatteringExponent(Level.EnvironmentTheme == EBMEnvironmentTheme::CoastalStorm ? 2.0f : 4.0f);
        FogComponent->SetStartDistance(FMath::Max(Level.EnvironmentFogStartDistance, bUnderground ? 520.0f : 420.0f));
        FogComponent->SetFogMaxOpacity(FMath::Min(Level.EnvironmentFogMaxOpacity, bUnderground ? 0.20f : 0.24f));
    }

    if (EnvironmentPostProcess)
    {
        const bool bWarm = Level.EnvironmentTheme == EBMEnvironmentTheme::Desert
            || Level.EnvironmentTheme == EBMEnvironmentTheme::Interior;
        const float CombatReadabilityExposure = Level.EnvironmentTheme == EBMEnvironmentTheme::Underground
            ? 0.68f
            : (Level.EnvironmentTheme == EBMEnvironmentTheme::CoastalStorm
                ? 0.34f
                : (Level.EnvironmentTheme == EBMEnvironmentTheme::Interior
                    ? 0.38f
                    : (Level.EnvironmentTheme == EBMEnvironmentTheme::Forest ? 0.26f : 0.20f)));
        EnvironmentPostProcess->Settings.ColorSaturation = bWarm
            ? FVector4(1.10f, 1.02f, 0.88f, 1.0f)
            : FVector4(1.02f, 1.04f, 1.08f, 1.0f);
        EnvironmentPostProcess->Settings.ColorContrast = Level.bInteriorEnvironment
            ? FVector4(1.03f, 1.02f, 1.04f, 1.0f)
            : FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        EnvironmentPostProcess->Settings.ColorGain = Level.bInteriorEnvironment
            ? FVector4(0.98f, 1.00f, 1.06f, 1.0f)
            : FVector4(1.02f, 1.03f, 1.06f, 1.0f);
        EnvironmentPostProcess->Settings.ColorGamma = Level.bInteriorEnvironment
            ? FVector4(1.14f, 1.12f, 1.12f, 1.0f)
            : FVector4(1.10f, 1.09f, 1.08f, 1.0f);
        EnvironmentPostProcess->Settings.BloomIntensity = Level.EnvironmentTheme == EBMEnvironmentTheme::SnowIce ? 0.14f : 0.08f;
        // The grade is theme-specific, but the bias is deliberately bounded:
        // enemy silhouettes, recoil flashes, and cover edges stay legible in
        // underground/storm operations without flattening the open spaces.
        EnvironmentPostProcess->Settings.AutoExposureBias = CombatReadabilityExposure;
    }

    if (EnvironmentLightningLight && EnvironmentLightningLight->GetLightComponent())
    {
        const bool bLightningActive = Level.bStormLightning && Level.WeatherType == EBMWeatherType::Rain;
        EnvironmentLightningLight->SetActorLocation(Level.OpenCombatLocation + FVector(0.0f, 0.0f, 950.0f));
        EnvironmentLightningLight->GetLightComponent()->SetLightColor(
            FLinearColor::LerpUsingHSV(WeatherColor, FLinearColor(0.82f, 0.92f, 1.0f), 0.55f));
        EnvironmentLightningLight->GetLightComponent()->SetIntensity(0.0f);
        EnvironmentLightningLight->GetLightComponent()->SetVisibility(false);
        LightningFlashRemaining = 0.0f;
        LightningCooldown = bLightningActive
            ? 2.6f + static_cast<float>((static_cast<int32>(CampaignLevel) * 7) % 23) * 0.18f
            : 0.0f;
    }

    if (EnvironmentAmbientAudio && GetNetMode() != NM_DedicatedServer && !FParse::Param(FCommandLine::Get(), TEXT("NoSound")))
    {
        USoundBase* AmbientSound = nullptr;
        if (UBMAudioDataAsset* AudioProfile = LoadObject<UBMAudioDataAsset>(
                nullptr,
                TEXT("/Game/Data/Audio/DA_BM_CombatAudio.DA_BM_CombatAudio")))
        {
            AmbientSound = AudioProfile->FindEnvironmentAmbience(Level.EnvironmentAmbienceId).LoadSynchronous();
        }
        if (!AmbientSound && !Level.EnvironmentAmbientSoundPath.IsEmpty())
        {
            AmbientSound = LoadObject<USoundBase>(nullptr, *Level.EnvironmentAmbientSoundPath);
        }
        if (AmbientSound)
        {
            if (USoundWave* Wave = Cast<USoundWave>(AmbientSound))
            {
                Wave->bLooping = true;
            }
            EnvironmentAmbientAudio->Stop();
            EnvironmentAmbientAudio->SetSound(AmbientSound);
            EnvironmentAmbientAudio->SetVolumeMultiplier(Level.EnvironmentAmbientVolume);
            EnvironmentAmbientAudio->Play();
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian environment audio: level=%s ambience=%s cue=%s playing=%s volume=%.2f"),
                *Level.LevelId.ToString(),
                *Level.EnvironmentAmbienceId.ToString(),
                *GetNameSafe(AmbientSound),
                EnvironmentAmbientAudio->IsPlaying() ? TEXT("yes") : TEXT("no"),
                Level.EnvironmentAmbientVolume);
        }
        else
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Black Meridian environment audio: level=%s ambience=%s cue=missing"),
                *Level.LevelId.ToString(),
                *Level.EnvironmentAmbienceId.ToString());
        }
    }

    ActiveEnvironmentTheme = Level.EnvironmentTheme;
    ActiveWeatherType = Level.WeatherType;
    ConfigureThemeWeather();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian environment theme: level=%s theme=%s weather=%d ground=%s sky=%s fog=%s sun=%.2f sound=%s"),
        *Level.LevelId.ToString(),
        *Level.EnvironmentThemeId.ToString(),
        static_cast<int32>(Level.WeatherType),
        *Level.EnvironmentGroundColor.ToString(),
        *Level.EnvironmentSkyColor.ToString(),
        *Level.EnvironmentFogColor.ToString(),
        Level.EnvironmentSunIntensity,
        Level.EnvironmentAmbientSoundPath.IsEmpty() ? TEXT("none") : *Level.EnvironmentAmbientSoundPath);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian environment profile: layout=%s terrain=%s props=%s sky=%s ambience=%s family=%d"),
        *Level.EnvironmentLayoutId.ToString(),
        *Level.EnvironmentTerrainId.ToString(),
        *Level.EnvironmentPropSetId.ToString(),
        *Level.EnvironmentSkyId.ToString(),
        *Level.EnvironmentAmbienceId.ToString(),
        static_cast<int32>(Level.EnvironmentTheme));
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian environment weather route: level=%s system=%s path=%s storm_lightning=%s particle_scale=%.2f"),
        *Level.LevelId.ToString(),
        *Level.EnvironmentWeatherSystemId.ToString(),
        Level.EnvironmentWeatherSystemPath.IsEmpty() ? TEXT("none") : *Level.EnvironmentWeatherSystemPath,
        Level.bStormLightning ? TEXT("yes") : TEXT("no"),
        Level.EnvironmentWeatherParticleScale);
}

void ABMGreyboxDirector::ClearThemeWeather()
{
    if (WeatherParticleSystem)
    {
        WeatherParticleSystem->ClearInstances();
        WeatherParticleSystem->SetVisibility(false, true);
    }
    WeatherParticleTransforms.Reset();
    WeatherParticlePhases.Reset();
    LightningCooldown = 0.0f;
    LightningFlashRemaining = 0.0f;
    if (EnvironmentLightningLight && EnvironmentLightningLight->GetLightComponent())
    {
        EnvironmentLightningLight->GetLightComponent()->SetIntensity(0.0f);
        EnvironmentLightningLight->GetLightComponent()->SetVisibility(false);
    }
    SetActorTickEnabled(false);

    if (EnvironmentNiagaraAccent)
    {
        EnvironmentNiagaraAccent->Deactivate();
        EnvironmentNiagaraAccent->SetVisibility(false, true);
    }
}

void ABMGreyboxDirector::ConfigureThemeWeather()
{
    if (!GetWorld() || !WeatherParticleSystem)
    {
        return;
    }

    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
    ClearThemeWeather();
    if (Level.WeatherType == EBMWeatherType::None || Level.EnvironmentWeatherIntensity <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const bool bSnow = Level.WeatherType == EBMWeatherType::Snow;
    UStaticMesh* ParticleMesh = LoadObject<UStaticMesh>(
        nullptr,
        bSnow
            ? TEXT("/Engine/BasicShapes/Sphere.Sphere")
            : TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!ParticleMesh)
    {
        return;
    }

    WeatherParticleSystem->SetStaticMesh(ParticleMesh);
    // The industrial surface instance is intentionally used by individual
    // environment actors, but it is not authored with the
    // InstancedStaticMeshes usage flag. Weather is an actual ISM field, so use
    // the engine's instancing-safe basic material here and keep the industrial
    // material as a fallback only when the engine asset is unavailable.
    UMaterialInterface* ParticleBaseMaterial = LoadObject<UMaterialInterface>(
        nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!ParticleBaseMaterial)
    {
        ParticleBaseMaterial = LoadPresentationMaterial();
    }
    if (ParticleBaseMaterial)
    {
        if (UMaterialInstanceDynamic* ParticleMaterial = WeatherParticleSystem->CreateAndSetMaterialInstanceDynamicFromMaterial(0, ParticleBaseMaterial))
        {
            const FLinearColor ParticleColor = FLinearColor::FromSRGBColor(Level.EnvironmentWeatherColor);
            ParticleMaterial->SetVectorParameterValue(TEXT("Color"), ParticleColor);
            ParticleMaterial->SetVectorParameterValue(TEXT("BaseColor"), ParticleColor);
            ParticleMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), ParticleColor * (bSnow ? 0.22f : 0.10f));
            ParticleMaterial->SetScalarParameterValue(TEXT("Metallic"), 0.0f);
            ParticleMaterial->SetScalarParameterValue(TEXT("Roughness"), bSnow ? 0.42f : 0.68f);
        }
    }

    const int32 BaseParticleCount = Level.WeatherType == EBMWeatherType::Rain
        ? 270
        : (Level.WeatherType == EBMWeatherType::Snow ? 185 : (Level.WeatherType == EBMWeatherType::Dust ? 145 : 80));
    const int32 ParticleCount = FMath::Clamp(
        FMath::RoundToInt(
            BaseParticleCount
            * FMath::Max(0.35f, Level.EnvironmentWeatherIntensity)
            * FMath::Max(0.35f, Level.EnvironmentWeatherParticleScale)),
        40,
        320);
    FRandomStream Stream(7201 + static_cast<int32>(CampaignLevel) * 97);
    WeatherParticleTransforms.Reserve(ParticleCount);
    WeatherParticlePhases.Reserve(ParticleCount);

    for (int32 Index = 0; Index < ParticleCount; ++Index)
    {
        const FVector Location(
            Stream.FRandRange(-2350.0f, 2350.0f),
            Stream.FRandRange(-1450.0f, 1450.0f),
            Stream.FRandRange(80.0f, 1180.0f));
        const float Yaw = Stream.FRandRange(0.0f, 360.0f);
        FRotator Rotation(0.0f, Yaw, 0.0f);
        FVector Scale(0.05f);
        if (Level.WeatherType == EBMWeatherType::Rain)
        {
            const float Lean = FMath::Clamp(Level.EnvironmentWeatherWindSpeed / 800.0f, 0.08f, 0.42f) * 45.0f;
            Rotation = FRotator(Lean, Yaw, 0.0f);
            Scale = FVector(0.018f, 0.018f, 1.10f);
        }
        else if (Level.WeatherType == EBMWeatherType::Dust)
        {
            Scale = FVector(Stream.FRandRange(0.025f, 0.07f), Stream.FRandRange(0.025f, 0.07f), Stream.FRandRange(0.20f, 0.65f));
        }
        else if (Level.WeatherType == EBMWeatherType::Mist)
        {
            Scale = FVector(Stream.FRandRange(0.08f, 0.18f));
        }
        else if (bSnow)
        {
            Scale = FVector(Stream.FRandRange(0.035f, 0.075f));
        }

        FTransform Transform(Rotation, Location, Scale);
        WeatherParticleTransforms.Add(Transform);
        WeatherParticlePhases.Add(Stream.FRandRange(0.0f, 6.28318f));
        WeatherParticleSystem->AddInstance(Transform, false);
    }

    WeatherParticleSystem->SetVisibility(true, true);
    WeatherParticleSystem->MarkRenderStateDirty();
    SetActorTickEnabled(true);

    // The generated Niagara systems are a replaceable authored accent layer;
    // the instanced field above remains the deterministic rain/snow fallback.
    const FString& NiagaraPath = Level.EnvironmentWeatherSystemPath;
    if (EnvironmentNiagaraAccent && !NiagaraPath.IsEmpty())
    {
        if (UNiagaraSystem* NiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, *NiagaraPath))
        {
            EnvironmentNiagaraAccent->SetAsset(NiagaraSystem);
            EnvironmentNiagaraAccent->SetRelativeLocation(FVector(0.0f, 0.0f, 420.0f));
            EnvironmentNiagaraAccent->SetRelativeScale3D(FVector(3.5f));
            EnvironmentNiagaraAccent->SetVisibility(true, true);
            EnvironmentNiagaraAccent->Activate(true);
        }
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian weather particles: level=%s type=%d count=%d intensity=%.2f fall_speed=%.1f wind=%.1f niagara=%s"),
        *Level.LevelId.ToString(),
        static_cast<int32>(Level.WeatherType),
        ParticleCount,
        Level.EnvironmentWeatherIntensity,
        Level.EnvironmentWeatherFallSpeed,
        Level.EnvironmentWeatherWindSpeed,
        NiagaraPath.IsEmpty() ? TEXT("none") : *NiagaraPath);
}

void ABMGreyboxDirector::TickThemeWeather(const float DeltaSeconds)
{
    if (!WeatherParticleSystem || WeatherParticleTransforms.IsEmpty() || ActiveWeatherType == EBMWeatherType::None)
    {
        return;
    }

    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
    const float SafeDelta = FMath::Clamp(DeltaSeconds, 0.0f, 0.05f);
    for (int32 Index = 0; Index < WeatherParticleTransforms.Num(); ++Index)
    {
        FTransform& Transform = WeatherParticleTransforms[Index];
        FVector Location = Transform.GetLocation();
        const float Phase = WeatherParticlePhases.IsValidIndex(Index) ? WeatherParticlePhases[Index] : 0.0f;
        const float Wind = Level.EnvironmentWeatherWindSpeed * SafeDelta;

        if (ActiveWeatherType == EBMWeatherType::Rain)
        {
            Location.Z -= Level.EnvironmentWeatherFallSpeed * SafeDelta;
            Location.X += Wind;
            Location.Y += Wind * 0.22f;
        }
        else if (ActiveWeatherType == EBMWeatherType::Snow)
        {
            Location.Z -= Level.EnvironmentWeatherFallSpeed * SafeDelta;
            Location.X += Wind * 0.35f + FMath::Sin(Phase + Location.Z * 0.004f) * 8.0f * SafeDelta;
            Location.Y += FMath::Cos(Phase + Location.X * 0.003f) * 6.0f * SafeDelta;
        }
        else if (ActiveWeatherType == EBMWeatherType::Dust)
        {
            Location.X += Wind;
            Location.Y += FMath::Sin(Phase + Location.X * 0.002f) * 12.0f * SafeDelta;
            Location.Z += FMath::Sin(Phase + Location.X * 0.001f) * 9.0f * SafeDelta;
        }
        else
        {
            Location.X += Wind * 0.20f;
            Location.Z += FMath::Sin(Phase + Location.X * 0.0015f) * 6.0f * SafeDelta;
        }

        if (Location.X > 2450.0f) Location.X -= 4900.0f;
        if (Location.X < -2450.0f) Location.X += 4900.0f;
        if (Location.Y > 1500.0f) Location.Y -= 3000.0f;
        if (Location.Y < -1500.0f) Location.Y += 3000.0f;
        if (Location.Z < 0.0f)
        {
            Location.Z = 1060.0f + FMath::Fmod(FMath::Abs(Phase * 113.0f + static_cast<float>(Index) * 31.0f), 180.0f);
        }

        Transform.SetLocation(Location);
        if (WeatherParticleSystem->IsValidInstance(Index))
        {
            WeatherParticleSystem->UpdateInstanceTransform(Index, Transform, false, false, true);
        }
        if (WeatherParticlePhases.IsValidIndex(Index))
        {
            WeatherParticlePhases[Index] += SafeDelta;
        }
    }

    if (Level.bStormLightning && ActiveWeatherType == EBMWeatherType::Rain && EnvironmentLightningLight && EnvironmentLightningLight->GetLightComponent())
    {
        if (LightningFlashRemaining > 0.0f)
        {
            LightningFlashRemaining -= SafeDelta;
            if (LightningFlashRemaining <= 0.0f)
            {
                EnvironmentLightningLight->GetLightComponent()->SetIntensity(0.0f);
                EnvironmentLightningLight->GetLightComponent()->SetVisibility(false);
            }
        }
        else
        {
            LightningCooldown -= SafeDelta;
            if (LightningCooldown <= 0.0f)
            {
                // A short, cool flash gives heavy rain a readable storm beat
                // without strobing the combat camera or changing gameplay.
                EnvironmentLightningLight->GetLightComponent()->SetIntensity(135000.0f);
                EnvironmentLightningLight->GetLightComponent()->SetVisibility(true);
                LightningFlashRemaining = 0.055f;
                LightningCooldown = 4.8f + static_cast<float>((static_cast<int32>(CampaignLevel) * 5) % 17) * 0.22f;
            }
        }
    }
    WeatherParticleSystem->MarkRenderStateDirty();
}

void ABMGreyboxDirector::ResetForCheckpoint(const EBMMissionPhase CheckpointPhase)
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    // Destroy every stale hostile, including dead actors still inside their
    // death lifespan, before rebuilding the wave for the saved phase.
    for (TActorIterator<ABMEnemyCharacter> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        ABMEnemyCharacter* Enemy = *Iterator;
        if (!IsValid(Enemy))
        {
            continue;
        }

        if (UBMSquadSubsystem* Squad = GetWorld()->GetSubsystem<UBMSquadSubsystem>())
        {
            Squad->UnregisterEnemy(Enemy);
        }
        Enemy->Destroy();
    }

    // Restore squad members in place. Their checkpoint override also removes
    // any revive prompt left by a failed attempt.
    for (TActorIterator<ABMFriendlySquadMember> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        if (ABMFriendlySquadMember* Friendly = *Iterator; IsValid(Friendly))
        {
            Friendly->RestoreForCheckpoint(100.0f);
        }
    }

    for (ABMInteractableActor* Interaction : MissionInteractables)
    {
        if (IsValid(Interaction))
        {
            Interaction->Destroy();
        }
    }
    MissionInteractables.Reset();

    if (ABMGameState* State = GetWorld()->GetGameState<ABMGameState>())
    {
        State->HostileCount = 0;
        State->TeamScore = 0;
        State->RecentKillFeed.Reset();
        State->ForceNetUpdate();
    }

    bEnvironmentalEventTriggered = false;
    SpawnMissionInteractables();
    MulticastResetForCheckpoint();

    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);

    // A saved campaign checkpoint currently resumes at Intelligence. These
    // branches also keep the reset API correct if additional checkpoint
    // phases are authored later.
    switch (CheckpointPhase)
    {
    case EBMMissionPhase::Checkpoint:
        SpawnCombatWave(Level.CheckpointWaveSize, Level.CheckpointLocation);
        break;
    case EBMMissionPhase::OpenCombat:
        SpawnCombatWave(Level.OpenCombatWaveSize, Level.OpenCombatLocation);
        TriggerEnvironmentalEvent();
        break;
    case EBMMissionPhase::FinalDefense:
        SpawnCombatWave(Level.FinalDefenseWaveSize, Level.ExtractionLocation);
        break;
    default:
        break;
    }
}

void ABMGreyboxDirector::RestoreEnvironmentalState()
{
    if (!GetWorld())
    {
        return;
    }

    for (AStaticMeshActor* EventPiece : EnvironmentalEventPieces)
    {
        if (IsValid(EventPiece))
        {
            EventPiece->Destroy();
        }
    }
    EnvironmentalEventPieces.Reset();

    if (ABMArtDirectedProp* RelayProp = FindBreachRelayProp())
    {
        RelayProp->ResetProp();
    }

    if (!IsValid(BreachBarrier))
    {
        const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
        BreachBarrier = SpawnCube(
            Level.BreachLocation + FVector(0.0f, 0.0f, -45.0f),
            FVector(1.8f, 0.35f, 2.4f),
            FColor(66, 74, 80),
            true);
    }
}

ABMArtDirectedProp* ABMGreyboxDirector::FindBreachRelayProp() const
{
    if (IsValid(BreachRelayProp))
    {
        return BreachRelayProp;
    }

    if (!GetWorld())
    {
        return nullptr;
    }

    for (TActorIterator<ABMArtDirectedProp> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        ABMArtDirectedProp* Prop = *Iterator;
        if (IsValid(Prop) && Prop->ActorHasTag(TEXT("Objective.RelayBreach")))
        {
            return Prop;
        }
    }
    return nullptr;
}

void ABMGreyboxDirector::SpawnCombatWave(const int32 Count, const FVector& Center)
{
    if (!HasAuthority())
    {
        return;
    }

#if !UE_BUILD_SHIPPING
    // The pacing probe advances every mission phase without a human aiming
    // or moving. Keep its world deterministic: it should validate the phase
    // clock, not spawn AI that can kill the probe pawn before Complete.
    const bool bCampaignEnemyProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_CampaignEnemyProbe"));
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_MissionProbe")) && !bCampaignEnemyProbe)
    {
        UE_LOG(LogTemp, Display, TEXT("Black Meridian probe: skipped combat wave count=%d center=%s"), Count, *Center.ToString());
        return;
    }
#endif

    if (!bRuntimeNavigationReady && !IsRuntimeNavigationReady())
    {
        bPendingCombatWave = true;
        PendingCombatWaveCount = Count;
        PendingCombatWaveCenter = Center;
        SetActorTickEnabled(true);
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian navigation: queued combat wave count=%d center=%s until Recast is ready"),
            Count,
            *Center.ToString());
        return;
    }
    bRuntimeNavigationReady = true;

    const int32 SafeCount = FMath::Clamp(Count, 0, 24);
    TSubclassOf<ABMEnemyCharacter> SpawnClass = CombatEnemyClass;
    if (!SpawnClass)
    {
        SpawnClass = ABMEnemyCharacter::StaticClass();
    }
#if !UE_BUILD_SHIPPING
    // The dedicated showcase keeps its own tighter placement rule; campaign
    // waves use a longer readable approach so the player can orient toward
    // the new objective before incoming fire begins.
    const float SpawnRadius = FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcase")) ? 420.0f : 900.0f;
#else
    constexpr float SpawnRadius = 900.0f;
#endif
    const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    const FVector PlayerLocation = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;
    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
    const bool bOpeningCheckpointWave = Count == Level.CheckpointWaveSize
        && FVector::DistSquared2D(Center, Level.CheckpointLocation) <= FMath::Square(5.0f);
    const bool bFinalDefenseWave = Count == Level.FinalDefenseWaveSize
        && FVector::DistSquared2D(Center, Level.ExtractionLocation) <= FMath::Square(5.0f);
    const bool bAshenFinalDefenseWave = bFinalDefenseWave
        && Level.Level == EBMCampaignLevel::AshenDocks;
    // Keep the normal ring safely outside the player, but let the first
    // encounter teach combat at a readable gameplay distance. The old
    // universal 850-unit clamp pushed the tutorial wave into the greybox
    // cover line, so the player could have five live hostiles and still see
    // an apparently empty arena.
    // Later waves used to combine a long formation lane with an 850 cm
    // player-distance clamp. On compact authored arenas that pushed the wave
    // beyond the encounter floor, behind a structural wall, or into a lane
    // the player could not reach. Keep the readable opening separation, but
    // keep every later wave inside a tactical distance that the physical
    // battlefield actually supports.
    const float MinimumPlayerDistance = bOpeningCheckpointWave ? 460.0f : 520.0f;

    // Runtime layouts deliberately vary their combat formations, but the
    // formation footprint cannot be allowed to outrun the physical battlefield.
    // Several authored pockets are narrower than the shared fallback floor;
    // spawning directly at an unvalidated XY coordinate left enemies at Z=85
    // with no supporting surface, so they fell through the arena and made the
    // later combat objective impossible to clear. Resolve every wave member
    // against collision before spawning it, preferring the surface closest to
    // the player's current deck and clamping failed samples to the guaranteed
    // director floor as a recoverable last resort.
    const ABMEnemyCharacter* EnemyDefaults = SpawnClass
        ? SpawnClass->GetDefaultObject<ABMEnemyCharacter>()
        : nullptr;
    const float EnemyCapsuleHalfHeight = EnemyDefaults && EnemyDefaults->GetCapsuleComponent()
        ? EnemyDefaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
        : 90.0f;
    const float PreferredSpawnRootZ = PlayerPawn ? PlayerLocation.Z : 85.0f;
    const FVector GuaranteedFloorCenter = FloorCollision
        ? FloorCollision->GetComponentLocation()
        : FVector(650.0f, 0.0f, -55.0f);
    const FVector GuaranteedFloorExtent = FloorCollision
        ? FloorCollision->GetScaledBoxExtent()
        : FVector(2750.0f, 1600.0f, 50.0f);
    const FVector GuaranteedFloorMargin(140.0f, 140.0f, 0.0f);

    const auto ClampToGuaranteedFloor = [
        GuaranteedFloorCenter,
        GuaranteedFloorExtent,
        GuaranteedFloorMargin](const FVector& Candidate)
    {
        return FVector(
            FMath::Clamp(
                Candidate.X,
                GuaranteedFloorCenter.X - GuaranteedFloorExtent.X + GuaranteedFloorMargin.X,
                GuaranteedFloorCenter.X + GuaranteedFloorExtent.X - GuaranteedFloorMargin.X),
            FMath::Clamp(
                Candidate.Y,
                GuaranteedFloorCenter.Y - GuaranteedFloorExtent.Y + GuaranteedFloorMargin.Y,
                GuaranteedFloorCenter.Y + GuaranteedFloorExtent.Y - GuaranteedFloorMargin.Y),
            Candidate.Z);
    };

    const auto FindGroundedSpawn = [this, EnemyCapsuleHalfHeight, PreferredSpawnRootZ](
        const FVector& Candidate,
        FVector& GroundedLocation)
    {
        if (!GetWorld())
        {
            return false;
        }

        const FVector TraceStart(Candidate.X, Candidate.Y, 2400.0f);
        const FVector TraceEnd(Candidate.X, Candidate.Y, -1200.0f);
        TArray<FHitResult> Hits;
        FCollisionQueryParams QueryParams(FName(TEXT("BlackMeridianCombatWaveGround")), false);
        if (!GetWorld()->LineTraceMultiByObjectType(
                Hits,
                TraceStart,
                TraceEnd,
                FCollisionObjectQueryParams(ECC_WorldStatic),
                QueryParams))
        {
            return false;
        }

        float BestScore = 1.0e9f;
        bool bFoundWalkableSurface = false;
        for (const FHitResult& Hit : Hits)
        {
            if (!Hit.bBlockingHit || Hit.ImpactNormal.Z < 0.45f)
            {
                continue;
            }

            const float CandidateRootZ = Hit.ImpactPoint.Z + EnemyCapsuleHalfHeight + 2.0f;
            // A trace can hit the top of a tank, roof, or decorative gantry
            // before it reaches the lower battlefield. Do not turn that hit
            // into a floating/unreachable enemy when the player is on the
            // lower deck. Elevated combat remains valid when the player is
            // elevated too, because the limit follows the current player
            // root height rather than assuming every map is flat.
            constexpr float MaximumUnrequestedElevation = 520.0f;
            if (CandidateRootZ > PreferredSpawnRootZ + MaximumUnrequestedElevation)
            {
                continue;
            }
            const float SurfaceScore = FMath::Abs(CandidateRootZ - PreferredSpawnRootZ);
            if (SurfaceScore >= BestScore)
            {
                continue;
            }

            BestScore = SurfaceScore;
            GroundedLocation = FVector(Candidate.X, Candidate.Y, CandidateRootZ);
            bFoundWalkableSurface = true;
        }
        return bFoundWalkableSurface;
    };

    // The opening checkpoint is the player's first combat lesson. A radial
    // ring is valid for a stress wave, but it is a poor teaching layout: the
    // player can arrive facing away from every enemy and read only an empty
    // arena while the hostiles advance from behind the camera. Anchor the
    // opening wave in a forward arc so bodies, weapons, and locomotion are
    // visible as soon as contact starts. Extra members use staggered arc rows
    // instead of falling back to a level formation that may be behind a wall.
    const FVector CombatAnchor = PlayerPawn ? PlayerLocation : Center;
    const APlayerController* PlayerController = PlayerPawn
        ? Cast<APlayerController>(PlayerPawn->GetController())
        : nullptr;
    const FVector AnchorForward = PlayerController
        ? PlayerController->GetControlRotation().Vector().GetSafeNormal2D()
        : (PlayerPawn
            ? PlayerPawn->GetActorForwardVector().GetSafeNormal2D()
            : (Center - PlayerLocation).GetSafeNormal2D());
    const FVector CombatForward = AnchorForward.IsNearlyZero()
        ? FVector::ForwardVector
        : AnchorForward;
    const FVector CombatRight(-CombatForward.Y, CombatForward.X, 0.0f);
    TArray<FVector> OpeningOffsets = {
        CombatForward * 520.0f,
        CombatForward * 660.0f - CombatRight * 260.0f,
        CombatForward * 660.0f + CombatRight * 260.0f,
        CombatForward * 820.0f - CombatRight * 470.0f,
        CombatForward * 820.0f + CombatRight * 470.0f};

    // Checkpoint waves grow across the 20-level campaign. Keep every member
    // in the player's readable forward approach; the old five-entry array
    // silently sent members 6+ through the level-specific formation, which
    // could put them on the far side of a solid channel wall.
    for (int32 OpeningIndex = OpeningOffsets.Num(); OpeningIndex < SafeCount; ++OpeningIndex)
    {
        const int32 ExtraIndex = OpeningIndex - OpeningOffsets.Num();
        const int32 ArcSlot = ExtraIndex % 4;
        const int32 ArcRow = ExtraIndex / 4;
        const float ArcAlpha = (static_cast<float>(ArcSlot) + 0.5f) / 4.0f;
        const float ArcAngle = FMath::Lerp(-58.0f, 58.0f, ArcAlpha);
        const float ArcRadius = 570.0f + static_cast<float>(ArcRow) * 170.0f;
        OpeningOffsets.Add(
            CombatForward.RotateAngleAxis(ArcAngle, FVector::UpVector) * ArcRadius);
    }

    const auto HasClearOpeningSightline = [this, PlayerPawn, PlayerLocation](const FVector& CandidateLocation)
    {
        if (!PlayerPawn || !GetWorld())
        {
            return true;
        }

        FCollisionQueryParams QueryParams(
            SCENE_QUERY_STAT(BlackMeridianOpeningWaveSightline),
            false,
            this);
        // The check is for structural cover, not the bodies already placed in
        // the same forward arc. Ignore all pawns so a previously spawned
        // member cannot make every later member fail its sightline test.
        for (TActorIterator<APawn> PawnIterator(GetWorld()); PawnIterator; ++PawnIterator)
        {
            if (*PawnIterator)
            {
                QueryParams.AddIgnoredActor(*PawnIterator);
            }
        }

        FHitResult Hit;
        const FVector TraceStart = PlayerLocation + FVector(0.0f, 0.0f, 72.0f);
        const FVector TraceEnd = CandidateLocation + FVector(0.0f, 0.0f, 58.0f);
        return !GetWorld()->LineTraceSingleByChannel(
            Hit,
            TraceStart,
            TraceEnd,
            ECC_Visibility,
            QueryParams);
    };

    // Physical layouts get encounter formations that use their lanes and
    // cover rhythm. The old universal ring could place a wave inside a tank,
    // behind a tunnel wall, or on the wrong side of a container lane even
    // though the level itself had changed. These offsets stay deterministic,
    // then the existing player-distance guard below keeps them readable.
    const FVector LayoutForward = (Level.ExtractionLocation - Level.StartLocation).GetSafeNormal2D().IsNearlyZero()
        ? CombatForward
        : (Level.ExtractionLocation - Level.StartLocation).GetSafeNormal2D();
    const FVector LayoutRight(-LayoutForward.Y, LayoutForward.X, 0.0f);
    FString EncounterFormationId(TEXT("open_radial_ring"));
    int32 EncounterFormationPattern = 0;
    switch (Level.Level)
    {
    case EBMCampaignLevel::SableQuay:
        EncounterFormationId = TEXT("open_ridge_arc");
        EncounterFormationPattern = 0;
        break;
    case EBMCampaignLevel::Floodline:
        EncounterFormationId = TEXT("drainage_channel_line");
        EncounterFormationPattern = 1;
        break;
    case EBMCampaignLevel::RelayHouse:
        EncounterFormationId = TEXT("rooftop_bridge_push");
        EncounterFormationPattern = 2;
        break;
    case EBMCampaignLevel::NightglassCore:
        EncounterFormationId = TEXT("tunnel_branch_columns");
        EncounterFormationPattern = 3;
        break;
    case EBMCampaignLevel::AshenDocks:
        EncounterFormationId = TEXT("dock_pier_crossfire");
        EncounterFormationPattern = 4;
        break;
    case EBMCampaignLevel::CinderWorks:
        EncounterFormationId = TEXT("foundry_room_push");
        EncounterFormationPattern = 5;
        break;
    case EBMCampaignLevel::SaltFoundry:
        EncounterFormationId = TEXT("salt_basin_ring");
        EncounterFormationPattern = 6;
        break;
    case EBMCampaignLevel::BlackwaterYard:
        EncounterFormationId = TEXT("tankfarm_ring");
        EncounterFormationPattern = 7;
        break;
    case EBMCampaignLevel::GlassMarket:
        EncounterFormationId = TEXT("market_lane_sweep");
        EncounterFormationPattern = 8;
        break;
    case EBMCampaignLevel::StormVault:
        EncounterFormationId = TEXT("catwalk_stagger");
        EncounterFormationPattern = 9;
        break;
    case EBMCampaignLevel::RedTide:
        EncounterFormationId = TEXT("seawall_channel");
        EncounterFormationPattern = 10;
        break;
    case EBMCampaignLevel::IronCathedral:
        EncounterFormationId = TEXT("cathedral_procession");
        EncounterFormationPattern = 11;
        break;
    case EBMCampaignLevel::LanternBasin:
        EncounterFormationId = TEXT("forest_flank");
        EncounterFormationPattern = 12;
        break;
    case EBMCampaignLevel::QuarryNine:
        EncounterFormationId = TEXT("canyon_crossfire");
        EncounterFormationPattern = 13;
        break;
    case EBMCampaignLevel::VantaSpire:
        EncounterFormationId = TEXT("spire_switchback");
        EncounterFormationPattern = 14;
        break;
    case EBMCampaignLevel::Wreckline:
        EncounterFormationId = TEXT("container_lane");
        EncounterFormationPattern = 15;
        break;
    case EBMCampaignLevel::EmberStation:
        EncounterFormationId = TEXT("fuel_bay_push");
        EncounterFormationPattern = 16;
        break;
    case EBMCampaignLevel::PaleHarbor:
        EncounterFormationId = TEXT("ice_berth_push");
        EncounterFormationPattern = 17;
        break;
    case EBMCampaignLevel::MeridianGate:
        EncounterFormationId = TEXT("gate_road_columns");
        EncounterFormationPattern = 18;
        break;
    case EBMCampaignLevel::LastLight:
        EncounterFormationId = TEXT("final_ridge_arc");
        EncounterFormationPattern = 19;
        break;
    default:
        break;
    }

    TArray<FVector> FormationOffsets;
    FormationOffsets.Reserve(SafeCount);
    const auto AddFormationOffset = [&FormationOffsets, &LayoutForward, &LayoutRight](
        const float Along,
        const float Across)
    {
        FormationOffsets.Add(LayoutForward * Along + LayoutRight * Across);
    };
    for (int32 Index = 0; Index < SafeCount; ++Index)
    {
        const int32 LaneIndex = Index % 4;
        const float Lane = static_cast<float>(LaneIndex) - 1.5f;
        const float PairSide = Index % 2 == 0 ? -1.0f : 1.0f;
        switch (EncounterFormationPattern)
        {
        case 0: // open ridge / final ridge: readable forward fan
        case 19:
        {
            const float FanAngle = -58.0f + 116.0f * (static_cast<float>(Index) + 0.5f) / static_cast<float>(FMath::Max(1, SafeCount));
            const float Radius = EncounterFormationPattern == 19 ? 980.0f : 820.0f + 90.0f * static_cast<float>(Index % 3);
            FormationOffsets.Add(LayoutForward.RotateAngleAxis(FanAngle, FVector::UpVector) * Radius);
            break;
        }
        case 1: // drainage channel: two disciplined lines down the channel
        case 10:
            AddFormationOffset(420.0f + 260.0f * static_cast<float>(Index / 3), PairSide * (EncounterFormationPattern == 10 ? 250.0f : 330.0f));
            break;
        case 2: // rooftop bridges: staggered approach from both roof edges
        case 9: // storm catwalk: alternating high-deck lanes
            AddFormationOffset(420.0f + 220.0f * static_cast<float>(Index % 5), PairSide * (EncounterFormationPattern == 9 ? 360.0f : 240.0f));
            break;
        case 3: // tunnel columns: tight alternating column positions
        case 11: // cathedral nave: processional aisle columns
            AddFormationOffset(-520.0f + 260.0f * static_cast<float>(Index % 6), PairSide * (EncounterFormationPattern == 3 ? 260.0f : 390.0f));
            break;
        case 4: // dock piers: broad crossfire from each pier
        case 13: // canyon: opposing elevated faces
            AddFormationOffset(
                bAshenFinalDefenseWave
                    ? -(360.0f + 250.0f * static_cast<float>(Index / 2))
                    : (360.0f + 250.0f * static_cast<float>(Index / 2)),
                PairSide * (EncounterFormationPattern == 13 ? 620.0f : 560.0f));
            break;
        case 5: // foundry: alternating furnace-room mouths
        case 16: // fuel bays: pump-island mouths
            AddFormationOffset(300.0f + 280.0f * static_cast<float>(Index % 5), Lane * (EncounterFormationPattern == 16 ? 300.0f : 430.0f));
            break;
        case 6: // salt basins: offset ring with a clear center
        case 7: // tank farm: ring around the service lane
        case 17: // ice berths: separated berth arc
        {
            const float Angle = (360.0f / static_cast<float>(FMath::Max(1, SafeCount))) * static_cast<float>(Index) + (EncounterFormationPattern * 13.0f);
            const float Radius = EncounterFormationPattern == 7
                ? 780.0f + 100.0f * static_cast<float>(Index % 2)
                : (EncounterFormationPattern == 17 ? 860.0f : 720.0f);
            FormationOffsets.Add(LayoutForward.RotateAngleAxis(Angle, FVector::UpVector) * Radius);
            break;
        }
        case 8: // market arcade: two parallel shopfront lanes
        case 15: // container maze: two long container alleys
        case 18: // gate road: checkpoint columns
            AddFormationOffset(360.0f + 250.0f * static_cast<float>(Index / 2), PairSide * (EncounterFormationPattern == 15 ? 650.0f : (EncounterFormationPattern == 18 ? 480.0f : 540.0f)));
            break;
        case 12: // forest: wide flank pairs around the route
            AddFormationOffset(300.0f + 220.0f * static_cast<float>(Index % 5), PairSide * (520.0f + 90.0f * static_cast<float>(Index % 3)));
            break;
        case 14: // spire: switchback lanes alternate side with distance
            AddFormationOffset(420.0f + 260.0f * static_cast<float>(Index % 5), PairSide * (620.0f + 120.0f * static_cast<float>(Index % 2)));
            break;
        default:
            break;
        }
    }

    // Each formation remains level-specific, but its authored lane must fit
    // inside the level's combat pocket. Without this envelope, patterns with
    // a long Along component placed later enemies kilometres down-route;
    // they could still own weapons and fire, but neither the player nor the
    // fallback movement could reach a valid sightline. Compress only the
    // horizontal offset, preserving the formation's direction and cadence.
    constexpr float MaxTacticalFormationRadius = 680.0f;
    for (FVector& FormationOffset : FormationOffsets)
    {
        FormationOffset.Z = 0.0f;
        const float FormationRadius = FormationOffset.Size2D();
        if (FormationRadius > MaxTacticalFormationRadius)
        {
            FormationOffset = FormationOffset.GetSafeNormal2D() * MaxTacticalFormationRadius;
        }
    }

    // A solid wall is useful cover only when the encounter can navigate
    // around it. Validate wave spawns against the same Recast graph used by
    // the enemy controller. If an authored formation lands on a disconnected
    // nav island, search nearby grounded candidates and keep the first one
    // with a complete path from the player. This preserves wall collision and
    // prevents a visual enemy from becoming an unreachable target.
    TArray<FVector> SelectedWaveLocations;
    SelectedWaveLocations.Reserve(SafeCount);
    const auto FindPlayableGroundedSpawn = [
        this,
        PlayerPawn,
        PlayerLocation,
        MinimumPlayerDistance,
        bOpeningCheckpointWave,
        &HasClearOpeningSightline,
        &FindGroundedSpawn,
        &ClampToGuaranteedFloor,
        &SelectedWaveLocations](const FVector& DesiredLocation, FVector& GroundedLocation)
    {
        TArray<FVector> Candidates;
        Candidates.Reserve(1 + 4 * 12 + 1);
        Candidates.Add(ClampToGuaranteedFloor(DesiredLocation));

        // Search a compact ring around the authored point first. This keeps
        // the encounter's visual shape while moving a blocked spawn out of a
        // wall or cover footprint.
        for (int32 RingIndex = 0; RingIndex < 4; ++RingIndex)
        {
            const float Radius = 140.0f + static_cast<float>(RingIndex) * 160.0f;
            const float AngleOffset = RingIndex % 2 == 0 ? 0.0f : 15.0f;
            for (int32 SectorIndex = 0; SectorIndex < 12; ++SectorIndex)
            {
                const float Angle = AngleOffset + (360.0f / 12.0f) * static_cast<float>(SectorIndex);
                Candidates.Add(ClampToGuaranteedFloor(
                    DesiredLocation + FRotator(0.0f, Angle, 0.0f).RotateVector(FVector(Radius, 0.0f, 0.0f))));
            }
        }

        // This is the guaranteed recovery pocket beneath the current player.
        // It is deliberately last so normal formations retain their authored
        // spacing, but it prevents an otherwise valid wave from being sealed
        // off by a newly-collidable structural shell.
        const FVector SafeFallbackDirection = (DesiredLocation - PlayerLocation).GetSafeNormal2D().IsNearlyZero()
            ? FVector::ForwardVector
            : (DesiredLocation - PlayerLocation).GetSafeNormal2D();
        Candidates.Add(ClampToGuaranteedFloor(
            PlayerLocation + SafeFallbackDirection * MinimumPlayerDistance));

        const auto IsSeparatedFromWave = [&SelectedWaveLocations](const FVector& Candidate)
        {
            for (const FVector& ExistingLocation : SelectedWaveLocations)
            {
                if (FVector::DistSquared2D(Candidate, ExistingLocation) < FMath::Square(120.0f))
                {
                    return false;
                }
            }
            return true;
        };

        for (const FVector& Candidate : Candidates)
        {
            FVector CandidateGroundedLocation = FVector::ZeroVector;
            if (!FindGroundedSpawn(Candidate, CandidateGroundedLocation)
                || (PlayerPawn
                    && FVector::DistSquared2D(CandidateGroundedLocation, PlayerLocation)
                        < FMath::Square(FMath::Max(120.0f, MinimumPlayerDistance - 40.0f)))
                || !IsSeparatedFromWave(CandidateGroundedLocation))
            {
                continue;
            }

            bool bReachable = !PlayerPawn;
            if (PlayerPawn)
            {
                if (UNavigationPath* NavigationPath = UNavigationSystemV1::FindPathToLocationSynchronously(
                        GetWorld(),
                        PlayerLocation,
                        CandidateGroundedLocation,
                        const_cast<APawn*>(PlayerPawn)))
                {
                    bReachable = NavigationPath->IsValid() && !NavigationPath->IsPartial();
                }
            }

            if (!bReachable)
            {
                continue;
            }

            if (bOpeningCheckpointWave
                && PlayerPawn
                && !HasClearOpeningSightline(CandidateGroundedLocation))
            {
                continue;
            }

            GroundedLocation = CandidateGroundedLocation;
            SelectedWaveLocations.Add(GroundedLocation);
            return true;
        }

        // Keep the original grounded fallback if Recast is still rebuilding.
        // It is preferable to spawn the enemy and let the controller retry
        // than to drop a required wave member from the mission entirely.
        for (const FVector& Candidate : Candidates)
        {
            FVector CandidateGroundedLocation = FVector::ZeroVector;
            if (FindGroundedSpawn(Candidate, CandidateGroundedLocation)
                && (!bOpeningCheckpointWave
                    || !PlayerPawn
                    || HasClearOpeningSightline(CandidateGroundedLocation))
                && IsSeparatedFromWave(CandidateGroundedLocation))
            {
                GroundedLocation = CandidateGroundedLocation;
                SelectedWaveLocations.Add(GroundedLocation);
                return true;
            }
        }
        return false;
    };

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian campaign encounter_layout=%s level=%s layout=%s pattern=%d wave_count=%d max_radius_cm=%.1f min_player_distance_cm=%.1f center=%s"),
        *EncounterFormationId,
        *Level.LevelId.ToString(),
        *Level.EnvironmentLayoutId.ToString(),
        EncounterFormationPattern,
        SafeCount,
        MaxTacticalFormationRadius,
        MinimumPlayerDistance,
        *Center.ToString());

    for (int32 Index = 0; Index < SafeCount; ++Index)
    {
        const float Angle = (360.0f / FMath::Max(1, SafeCount)) * Index;
        FVector Location = bOpeningCheckpointWave && OpeningOffsets.IsValidIndex(Index)
            ? CombatAnchor + OpeningOffsets[Index]
            : (FormationOffsets.IsValidIndex(Index)
                ? Center + FormationOffsets[Index]
                : Center + FRotator(0.0f, Angle, 0.0f).RotateVector(FVector(SpawnRadius, 0.0f, 0.0f)));
        if (PlayerPawn && FVector::DistSquared2D(Location, PlayerLocation) < FMath::Square(MinimumPlayerDistance))
        {
            FVector AwayFromPlayer = (Location - PlayerLocation).GetSafeNormal2D();
            if (AwayFromPlayer.IsNearlyZero())
            {
                AwayFromPlayer = (Center - PlayerLocation).GetSafeNormal2D();
            }
            if (AwayFromPlayer.IsNearlyZero())
            {
                AwayFromPlayer = FVector::ForwardVector;
            }
            Location = PlayerLocation + AwayFromPlayer * MinimumPlayerDistance;
        }

        const FVector RequestedSpawnLocation(
            Location.X,
            Location.Y,
            PlayerPawn ? PlayerLocation.Z : Location.Z);

        FVector SpawnLocation = RequestedSpawnLocation;
        FVector GroundedSpawnLocation = FVector::ZeroVector;
        const bool bGroundedSpawn = FindPlayableGroundedSpawn(
            RequestedSpawnLocation,
            GroundedSpawnLocation);
        if (bGroundedSpawn)
        {
            SpawnLocation = GroundedSpawnLocation;
        }

#if !UE_BUILD_SHIPPING
        if (bGroundedSpawn && FVector::DistSquared(SpawnLocation, RequestedSpawnLocation) > FMath::Square(4.0f))
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian campaign: grounded enemy spawn index=%d requested=%s resolved=%s"),
                Index,
                *RequestedSpawnLocation.ToString(),
                *SpawnLocation.ToString());
        }
        else if (!bGroundedSpawn)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Black Meridian campaign: enemy spawn ground fallback unavailable index=%d requested=%s"),
                Index,
                *RequestedSpawnLocation.ToString());
        }
#endif

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        const FRotator InitialFacing = PlayerPawn
            ? (PlayerLocation - SpawnLocation).Rotation()
            : FRotator::ZeroRotator;
        ABMEnemyCharacter* Enemy = GetWorld()->SpawnActor<ABMEnemyCharacter>(SpawnClass, SpawnLocation, InitialFacing, SpawnParameters);
        if (Enemy)
        {
            const EBMEnemyRole RolePalette[] = {
                EBMEnemyRole::Assault,
                EBMEnemyRole::Marksman,
                EBMEnemyRole::Breacher,
                EBMEnemyRole::Demolitionist,
                EBMEnemyRole::Rifleman};
            const int32 RoleCycle = Index % UE_ARRAY_COUNT(RolePalette);
            // Each operation has a different role cadence. The five enemy
            // classes remain available, but level 8 can open with a
            // breacher/assault push while later levels add launcher lanes.
            // This makes the encounter roster part of the level definition
            // instead of another globally repeated wave pattern.
            const EBMEnemyRole SpawnedRole = RolePalette[(Index + Level.EnvironmentVariant) % UE_ARRAY_COUNT(RolePalette)];
            // The first wave is the player's combat tutorial. Give the
            // player time to read the red Vanta labels, get behind cover, and
            // make the first aimed shots before the whole ring opens fire.
            Enemy->Aggression = bOpeningCheckpointWave
                ? (RoleCycle == 0 ? 0.72f : (RoleCycle == 2 ? 0.68f : 0.62f))
                : (RoleCycle == 0 ? 1.3f : (RoleCycle == 2 ? 1.15f : 1.0f));
            Enemy->DamageToPlayerMultiplier = bOpeningCheckpointWave
                ? BMCombatTuning::GetOpeningWaveDamageMultiplier(SpawnedRole)
                : (RoleCycle == 2 ? 0.64f : 0.68f);
            Enemy->DamageToPlayerMultiplier *= Level.HostileDamageScale;
            Enemy->SetReadableCombatPacing(bOpeningCheckpointWave);
            Enemy->AimErrorDegrees = bOpeningCheckpointWave
                ? BMCombatTuning::GetOpeningWaveAimErrorDegrees(SpawnedRole)
                : Enemy->AimErrorDegrees;
            Enemy->SetEnemyRole(SpawnedRole);
            Enemy->ReactionDelay = 0.35f;
            // Give the player a short readable hand-off, then let the wave
            // answer consistently. The previous five-second opening plus
            // stagger made hostile fire look absent during normal play.
            const float BaseCombatStartDelay = bOpeningCheckpointWave
                ? BMCombatTuning::OpeningWaveMinimumStartDelaySeconds
                : 1.25f;
            Enemy->SetCombatStartDelay(BaseCombatStartDelay + static_cast<float>(Index % 4) * 0.28f);
            if (ABMGameState* State = GetWorld()->GetGameState<ABMGameState>())
            {
                ++State->HostileCount;
            }
#if !UE_BUILD_SHIPPING
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian campaign: spawned enemy index=%d opening=%s location=%s facing=%s"),
                Index,
                bOpeningCheckpointWave ? TEXT("yes") : TEXT("no"),
                *Enemy->GetActorLocation().ToString(),
                *Enemy->GetActorRotation().ToString());
#endif
        }
    }
}

void ABMGreyboxDirector::SpawnCombatShowcaseWave(const FVector& PlayerLocation, const FVector& ForwardDirection)
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    const FVector Forward = ForwardDirection.GetSafeNormal2D().IsNearlyZero()
        ? FVector::ForwardVector
        : ForwardDirection.GetSafeNormal2D();
    const FVector Right(-Forward.Y, Forward.X, 0.0f);
    const bool bCloseFacingProbe = FParse::Param(
        FCommandLine::Get(),
        TEXT("BM_CombatCloseFacingProbe"));
    // The first three targets create a readable near/mid/far triangle; the
    // outer pair widen the encounter without placing anyone behind the
    // camera. Distances leave enough time for the player to react before the
    // reciprocal AI fire begins.
    const TArray<FVector> Offsets = bCloseFacingProbe
        ? TArray<FVector>{Forward * 360.0f}
        : TArray<FVector>{
            Forward * 650.0f,
            Forward * 820.0f - Right * 280.0f,
            Forward * 820.0f + Right * 280.0f,
            Forward * 1080.0f - Right * 520.0f,
            Forward * 1080.0f + Right * 520.0f};
    TSubclassOf<ABMEnemyCharacter> SpawnClass = CombatEnemyClass;
    if (!SpawnClass)
    {
        SpawnClass = ABMEnemyCharacter::StaticClass();
    }

    for (int32 Index = 0; Index < Offsets.Num(); ++Index)
    {
        const FVector Location = PlayerLocation + Offsets[Index];
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        const FVector SpawnLocation(Location.X, Location.Y, PlayerLocation.Z);
        ABMEnemyCharacter* Enemy = GetWorld()->SpawnActor<ABMEnemyCharacter>(
            SpawnClass,
            SpawnLocation,
            (PlayerLocation - Location).Rotation(),
            SpawnParameters);
        if (!Enemy)
        {
            UE_LOG(LogTemp, Warning, TEXT("Black Meridian showcase: failed to spawn enemy index=%d location=%s"), Index, *Location.ToString());
            continue;
        }

            const EBMEnemyRole ShowcaseRole = Index == 0
                ? EBMEnemyRole::Assault
                : (Index == 1
                    ? EBMEnemyRole::Breacher
                    : (Index == 3
                        ? EBMEnemyRole::Marksman
                        : (Index == 4 ? EBMEnemyRole::Demolitionist : EBMEnemyRole::Rifleman)));
            Enemy->Aggression = ShowcaseRole == EBMEnemyRole::Assault
                ? 1.15f
                : (ShowcaseRole == EBMEnemyRole::Breacher
                    ? 1.05f
                    : (ShowcaseRole == EBMEnemyRole::Demolitionist ? 0.90f : 0.95f));
            Enemy->DamageToPlayerMultiplier = BMCombatTuning::ShowcaseDamageMultiplier;
            Enemy->SetReadableCombatPacing(true);
            Enemy->SetEnemyRole(ShowcaseRole);
            Enemy->ReactionDelay = 0.35f;
            Enemy->AimErrorDegrees = BMCombatTuning::GetOpeningWaveAimErrorDegrees(ShowcaseRole);
            // The showcase is also the human-facing combat acceptance path.
            // Keep the first readable beat, then bring the whole line into the
            // fight quickly enough that return fire is unmistakable.
            Enemy->SetCombatStartDelay(
                bCloseFacingProbe
                    ? 30.0f
                    : 1.25f + static_cast<float>(Index) * 0.28f);
            if (ABMGameState* State = GetWorld()->GetGameState<ABMGameState>())
        {
            ++State->HostileCount;
        }
        const int32 RoleValue = static_cast<int32>(Enemy->GetEnemyRole());
        const FString EnemyLocation = Enemy->GetActorLocation().ToString();
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian showcase: enemy index=%d pawn=%s role=%s role_id=%d weapon=%d preferred=%.0f engage=%.0f speed=%.0f grenades=%s location=%s"),
            Index,
            *GetNameSafe(Enemy),
            *Enemy->GetEnemyRoleLabel(),
            RoleValue,
            static_cast<int32>(Enemy->GetCombatWeapon() ? Enemy->GetCombatWeapon()->GetWeaponId() : EBMWeaponId::ARX4),
            Enemy->GetPreferredEngagementRange(),
            Enemy->GetEngagementRange(),
            Enemy->GetRoleMoveSpeed(),
            Enemy->CanThrowGrenades() ? TEXT("yes") : TEXT("no"),
            *EnemyLocation);
    }
}

void ABMGreyboxDirector::SpawnFriendlySquad()
{
    const TArray<FVector> Locations = {FVector(-230.0f, -80.0f, 100.0f), FVector(-300.0f, 180.0f, 100.0f)};
    for (int32 Index = 0; Index < Locations.Num(); ++Index)
    {
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        if (ABMFriendlySquadMember* Friendly = GetWorld()->SpawnActor<ABMFriendlySquadMember>(ABMFriendlySquadMember::StaticClass(), Locations[Index], FRotator::ZeroRotator, SpawnParameters))
        {
            Friendly->Callsign = Index == 0 ? NSLOCTEXT("BlackMeridian", "MakoCallsign", "Mako") : NSLOCTEXT("BlackMeridian", "SableCallsign", "Sable");
            Friendly->FormationOffset = FVector(-180.0f, Index == 0 ? 120.0f : -120.0f, 0.0f);
        }
    }
}

void ABMGreyboxDirector::TriggerEnvironmentalEvent()
{
    if (HasAuthority())
    {
        MulticastEnvironmentalEvent();
    }
}

void ABMGreyboxDirector::OpenBreachBarrier()
{
    if (HasAuthority())
    {
        // This is deliberately separate from TriggerEnvironmentalEvent():
        // completing the relay hold opens the route, while the later intel
        // or seawall transition still owns the destruction/debris event.
        MulticastOpenBreachBarrier();
    }
}

void ABMGreyboxDirector::MulticastOpenBreachBarrier_Implementation()
{
    if (BreachBarrier)
    {
        BreachBarrier->SetActorEnableCollision(false);
        BreachBarrier->Destroy();
        BreachBarrier = nullptr;
        UE_LOG(LogTemp, Display, TEXT("Black Meridian campaign: relay breach barrier opened"));
    }
}

void ABMGreyboxDirector::MulticastEnvironmentalEvent_Implementation()
{
    if (bEnvironmentalEventTriggered)
    {
        return;
    }
    bEnvironmentalEventTriggered = true;

    if (ABMArtDirectedProp* RelayProp = FindBreachRelayProp())
    {
        RelayProp->TriggerDestruction();
    }

    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
    const FVector BreachLocation = Level.BreachLocation + FVector(0.0f, 0.0f, 100.0f);
    if (BreachBarrier)
    {
        BreachBarrier->Destroy();
        BreachBarrier = nullptr;
    }
    AStaticMeshActor* Debris = SpawnCube(
            BreachLocation + FVector(50.0f, 80.0f, -130.0f),
            FVector(0.45f, 0.25f, 0.35f),
            FColor(125, 48, 32),
            true);
    if (Debris)
    {
        EnvironmentalEventPieces.Add(Debris);
    }
    Debris = SpawnCube(
            BreachLocation + FVector(-55.0f, -70.0f, -130.0f),
            FVector(0.35f, 0.3f, 0.25f),
            FColor(90, 50, 40),
            true);
    if (Debris)
    {
        EnvironmentalEventPieces.Add(Debris);
    }
    // This is useful when diagnosing objective placement, but it is not part
    // of the authored event presentation. Keep it opt-in so ordinary gameplay
    // and rendered showcase captures do not expose a giant debug wireframe.
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_ShowDebugEvents")))
    {
        DrawDebugSphere(GetWorld(), BreachLocation, 260.0f, 24, FColor(255, 116, 40), false, 3.0f, 0, 4.0f);
    }
}

void ABMGreyboxDirector::MulticastResetForCheckpoint_Implementation()
{
    bEnvironmentalEventTriggered = false;
    RestoreEnvironmentalState();
}
