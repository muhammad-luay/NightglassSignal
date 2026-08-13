// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BMCoreTypes.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EBMTeam : uint8
{
    Neutral,
    Lattice,
    Vanta
};

UENUM(BlueprintType)
enum class EBMWeaponId : uint8
{
    ARX4,
    Kestrel9,
    RookDMR,
    Breach12,
    SolacePistol,
    ArcLauncher,
    FragGrenade,
    VeilGrenade
};

UENUM(BlueprintType)
enum class EBMWeaponFireMode : uint8
{
    SemiAutomatic,
    Burst,
    FullyAutomatic
};

UENUM(BlueprintType)
enum class EBMHitZone : uint8
{
    Head,
    Torso,
    Arm,
    Leg
};

UENUM(BlueprintType)
enum class EBMSurfaceType : uint8
{
    Concrete,
    Metal,
    Wood,
    Glass,
    SoftCover
};

UENUM(BlueprintType)
enum class EBMAIState : uint8
{
    Patrol,
    Investigate,
    Engage,
    Suppressed,
    Flank,
    Advance,
    Retreat,
    Downed
};

UENUM(BlueprintType)
enum class EBMCharacterMovementState : uint8
{
    Idle,
    Walk,
    Sprint,
    TacticalSprint,
    Crouch,
    Prone,
    Slide,
    Jump,
    Fall,
    Land,
    Ladder,
    Mounted,
    Downed,
    Dead
};

UENUM(BlueprintType)
enum class EBMEnemyRole : uint8
{
    Rifleman,
    Assault,
    Marksman,
    /** Close-range shotgun breacher that pressures cover and doorways. */
    Breacher,
    /** Area-denial launcher specialist that forces the player out of cover. */
    Demolitionist
};

UENUM(BlueprintType)
enum class EBMMissionPhase : uint8
{
    Intro,
    Insertion,
    QuietApproach,
    Checkpoint,
    InteriorBreach,
    Intelligence,
    OpenCombat,
    Extraction,
    FinalDefense,
    Complete,
    Failed
};

/**
 * Runtime campaign variants. The compatibility build uses one Entry map and
 * generates these layouts at runtime; every level still has its own route,
 * start point, objective anchors, environment treatment, and encounter
 * budget.
 */
UENUM(BlueprintType)
enum class EBMCampaignLevel : uint8
{
    SableQuay = 0,
    Floodline = 1,
    RelayHouse = 2,
    NightglassCore = 3,
    AshenDocks = 4,
    CinderWorks = 5,
    SaltFoundry = 6,
    BlackwaterYard = 7,
    GlassMarket = 8,
    StormVault = 9,
    RedTide = 10,
    IronCathedral = 11,
    LanternBasin = 12,
    QuarryNine = 13,
    VantaSpire = 14,
    Wreckline = 15,
    EmberStation = 16,
    PaleHarbor = 17,
    MeridianGate = 18,
    LastLight = 19
};

/** Visual family used to art-direct the runtime campaign shell. */
UENUM(BlueprintType)
enum class EBMEnvironmentTheme : uint8
{
    OpenDry,
    CoastalStorm,
    SnowIce,
    Desert,
    Forest,
    Underground,
    Interior
};

/** Lightweight precipitation/atmosphere treatment used by the runtime weather volume. */
UENUM(BlueprintType)
enum class EBMWeatherType : uint8
{
    None,
    Rain,
    Snow,
    Dust,
    Mist
};

UENUM(BlueprintType, meta = (ScriptName = "BMObjectiveStatus"))
enum class EBMObjectiveState : uint8
{
    Hidden,
    Active,
    Complete,
    Optional,
    Failed
};

UENUM(BlueprintType)
enum class EBMInteractionType : uint8
{
    Tap,
    Hold
};

UENUM(BlueprintType)
enum class EBMInteractionAction : uint8
{
    Generic,
    Ladder,
    OpenDoor,
    PickupWeapon,
    PickupAmmo,
    ActivateSwitch,
    PlantEquipment,
    BreachDoor,
    ReviveAlly,
    MountedWeapon,
    MissionEvent,
    /** Proximity-collected Frag/Veil ordnance resupply. Keep appended for serialized compatibility. */
    PickupThrowable,
    /** Proximity-collected weapon attachment. Keep appended for serialized compatibility. */
    PickupAttachment
};

USTRUCT(BlueprintType)
struct BLACKMERIDIANRUNTIME_API FBMWeaponBalance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float BaseDamage = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float FireRateRPM = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    int32 MagazineSize = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    int32 ReserveAmmo = 120;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    // Keep the compatibility/runtime fallback inside the same short tactical
    // pacing window as the authored weapon profiles. A missing or stale data
    // asset must not silently restore the old multi-second reload.
    float ReloadDuration = 0.45f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float EmptyReloadDuration = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float RangeMeters = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float FalloffStartMeters = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float FalloffEndMeters = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float HipSpreadDegrees = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float ADSSpreadDegrees = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0"))
    float AccuracyRecoveryDegreesPerSecond = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0"))
    float ShotSpreadPenaltyDegrees = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0"))
    float MaxAccuracyPenaltyDegrees = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Presentation", meta = (ClampMin = "0.0"))
    float WeaponSwayAmount = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Presentation", meta = (ClampMin = "0.0"))
    float WeaponSwayFrequency = 7.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Presentation", meta = (ClampMin = "0.0"))
    float ADSSwayMultiplier = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float RecoilPitch = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float RecoilYaw = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float PenetrationMultiplier = 0.55f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float SuppressionRadius = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    int32 PelletCount = 1;
};

USTRUCT(BlueprintType)
struct BLACKMERIDIANRUNTIME_API FBMDamageContext
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    float RawDamage = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    float FinalDamage = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    EBMHitZone HitZone = EBMHitZone::Torso;

    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    bool bHeadshot = false;

    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    bool bPenetrated = false;

    UPROPERTY(BlueprintReadOnly, Category = "Damage|Surface")
    EBMSurfaceType SurfaceType = EBMSurfaceType::Concrete;

    UPROPERTY(BlueprintReadOnly, Category = "Damage|Surface")
    int32 PenetrationCount = 0;

    /**
     * Marks damage that originated from a player-owned weapon. This is kept
     * separate from the validation result so the health boundary can fail
     * closed if a future weapon path forgets to complete its aim check.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Damage|Validation")
    bool bPlayerWeaponDamage = false;

    /**
     * Set only after the final player crosshair gate succeeds. A player
     * weapon context with this unset must never mutate character health.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Damage|Validation")
    bool bPlayerWeaponAimValidated = false;

    /**
     * Player melee and throwable damage are intentional non-firearm paths.
     * They are explicit so the health boundary can reject an unclassified
     * player-to-enemy damage call without disabling those two actions.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Damage|Validation")
    bool bPlayerMeleeDamage = false;

    UPROPERTY(BlueprintReadOnly, Category = "Damage|Validation")
    bool bPlayerThrowableDamage = false;

    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    TObjectPtr<AActor> InstigatorActor = nullptr;
};

USTRUCT(BlueprintType)
struct BLACKMERIDIANRUNTIME_API FBMObjectiveState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Objective")
    FName ObjectiveId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Objective")
    FText Title;

    UPROPERTY(BlueprintReadOnly, Category = "Objective")
    EBMObjectiveState State = EBMObjectiveState::Hidden;

    UPROPERTY(BlueprintReadOnly, Category = "Objective")
    bool bOptional = false;

    /** World-space destination for the active objective marker. */
    UPROPERTY(BlueprintReadOnly, Category = "Objective")
    FVector WorldLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Objective")
    bool bHasWorldLocation = false;
};

/**
 * Small, playable camera cue contract used until authored Level Sequences are
 * available. Cues are additive to the player camera and do not lock look input
 * unless a designer explicitly opts in.
 */
USTRUCT(BlueprintType)
struct BLACKMERIDIANRUNTIME_API FBMPlayableCameraCue
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
    FName CueId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic", meta = (ClampMin = "0.05"))
    float Duration = 2.0f;

    /** Additive camera transform at the beginning of the cue. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
    FVector StartCameraOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
    FRotator StartCameraRotation = FRotator::ZeroRotator;

    /** Additive camera transform at the end of the cue. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
    FVector CameraOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
    FRotator CameraRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
    bool bLockLook = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
    FText Subtitle;
};

USTRUCT(BlueprintType)
struct BLACKMERIDIANRUNTIME_API FBMSignalControlSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Signal Control")
    int32 LatticeScore = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Signal Control")
    int32 VantaScore = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Signal Control")
    int32 ActiveNodeIndex = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Signal Control")
    float SecondsUntilRotation = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Signal Control")
    bool bMatchFinished = false;

    /** Authority-selected winner, replicated with the final match snapshot. */
    UPROPERTY(BlueprintReadOnly, Category = "Signal Control")
    EBMTeam WinningTeam = EBMTeam::Neutral;
};

/**
 * Authority-built roster entry shown before a Signal Control round starts.
 * Keeping this as a small replicated value type means the lobby presentation
 * does not depend on client-side controller discovery or bot actor timing.
 */
USTRUCT(BlueprintType)
struct BLACKMERIDIANRUNTIME_API FBMSignalLobbyMember
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Signal Control|Lobby")
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Signal Control|Lobby")
    EBMTeam Team = EBMTeam::Neutral;

    UPROPERTY(BlueprintReadOnly, Category = "Signal Control|Lobby")
    bool bIsBot = false;

    UPROPERTY(BlueprintReadOnly, Category = "Signal Control|Lobby")
    bool bIsHost = false;
};
