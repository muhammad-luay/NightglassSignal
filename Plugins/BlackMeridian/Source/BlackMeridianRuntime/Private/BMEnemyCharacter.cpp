// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMEnemyCharacter.h"

#include "BMCharacterAnimationDataAsset.h"
#include "BMGameMode.h"
#include "BMGameState.h"
#include "BMHealthComponent.h"
#include "BMEnemyAIController.h"
#include "BMPlayerCharacter.h"
#include "BMWeapon.h"
#include "BMWeaponDataAsset.h"
#include "BMSuppressionComponent.h"
#include "BMThrowable.h"
#include "BMSquadSubsystem.h"
#include "BMCombatTuning.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "HAL/FileManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/SoftObjectPath.h"
#include "UnrealClient.h"
#include "Net/UnrealNetwork.h"

namespace
{
    FColor GetRoleDisplayColor(const EBMEnemyRole Role)
    {
        switch (Role)
        {
        case EBMEnemyRole::Assault:
            return FColor(238, 92, 28);
        case EBMEnemyRole::Marksman:
            return FColor(42, 160, 210);
        case EBMEnemyRole::Breacher:
            return FColor(220, 170, 38);
        case EBMEnemyRole::Demolitionist:
            return FColor(168, 76, 224);
        case EBMEnemyRole::Rifleman:
        default:
            return FColor(190, 38, 48);
        }
    }

    FLinearColor GetRoleLinearColor(const EBMEnemyRole Role)
    {
        const FColor DisplayColor = GetRoleDisplayColor(Role);
        return FLinearColor(
            static_cast<float>(DisplayColor.R) / 255.0f,
            static_cast<float>(DisplayColor.G) / 255.0f,
            static_cast<float>(DisplayColor.B) / 255.0f,
            1.0f);
    }

    const TCHAR* GetRoleLabel(const EBMEnemyRole Role)
    {
        switch (Role)
        {
        case EBMEnemyRole::Assault:
            return TEXT("ASSAULT");
        case EBMEnemyRole::Marksman:
            return TEXT("MARKSMAN");
        case EBMEnemyRole::Breacher:
            return TEXT("BREACHER");
        case EBMEnemyRole::Demolitionist:
            return TEXT("DEMOLITIONIST");
        case EBMEnemyRole::Rifleman:
        default:
            return TEXT("RIFLEMAN");
        }
    }

    const TCHAR* GetRoleMarker(const EBMEnemyRole Role)
    {
        switch (Role)
        {
        case EBMEnemyRole::Assault:
            return TEXT("▲");
        case EBMEnemyRole::Marksman:
            return TEXT("✦");
        case EBMEnemyRole::Breacher:
            return TEXT("◆");
        case EBMEnemyRole::Demolitionist:
            return TEXT("✹");
        case EBMEnemyRole::Rifleman:
        default:
            return TEXT("■");
        }
    }

    void ConfigureModularPart(
        UStaticMeshComponent* Part,
        UStaticMesh* Mesh,
        const FVector& Location,
        const FVector& Scale,
        const FRotator& Rotation = FRotator::ZeroRotator)
    {
        if (!Part)
        {
            return;
        }

        Part->SetStaticMesh(Mesh);
        Part->SetRelativeLocation(Location);
        Part->SetRelativeRotation(Rotation);
        Part->SetRelativeScale3D(Scale);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetGenerateOverlapEvents(false);
        Part->SetCastShadow(true);
        Part->SetVisibility(false, false);
    }
}

ABMEnemyCharacter::ABMEnemyCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    Team = EBMTeam::Vanta;
    PrimaryActorTick.bCanEverTick = true;
    // CharacterMovement consumes pending input in the pre-physics group. Run
    // the enemy actor after that update so UpdateMovementFacing sees the
    // velocity that actually moved the capsule this frame.
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = ABMEnemyAIController::StaticClass();
    // Combat target-facing owns the rendered body heading while the movement
    // input remains an independent collision-safe route. This makes a moving
    // enemy visibly engage the player instead of showing a shoulder/back while
    // its weapon aims correctly.
    bUseControllerRotationYaw = false;
    // Enemy transforms are part of the player-visible combat presentation.
    // Keep enough server updates for remote clients to interpolate a readable
    // walk instead of receiving sparse position/rotation steps from a local
    // server, while still allowing the normal lower bound under load.
    NetUpdateFrequency = 60.0f;
    MinNetUpdateFrequency = 30.0f;
    NetPriority = 2.0f;
    // Keep the default conservative as well as the role-specific values
    // below.  This prevents a navigation-enabled map from briefly using a
    // sprint-like speed before ApplyRolePresentation runs.
    GetCharacterMovement()->MaxWalkSpeed = 96.0f;
    // Hostile bodies should not form a physical wall in the direct-input
    // showcase fallback. Damage and weapon traces still use the capsule, but
    // overlapping pawns lets a crowded approach keep flowing while
    // navigation/avoidance is unavailable.
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    GetCharacterMovement()->PrimaryComponentTick.TickGroup = TG_PrePhysics;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
    GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
    GetCharacterMovement()->RotationRate = FRotator(
        0.0f,
        BMCombatTuning::EnemyMovementRotationRateDegreesPerSecond,
        0.0f);
    // The runtime showcase uses direct CharacterMovement input when no baked
    // NavMesh is available.  Softer acceleration/braking keeps a target hand-
    // off from looking like a sudden lunge or a skating stop.
    GetCharacterMovement()->MaxAcceleration = 600.0f;
    GetCharacterMovement()->BrakingDecelerationWalking = 650.0f;
    HealthComponent->MaxHealth = 100.0f;
    HealthComponent->Health = 100.0f;
    HealthComponent->bRegenerateHealth = false;
    PlaceholderBody->SetRelativeScale3D(FVector(0.38f, 0.38f, 0.9f));

    ModularRigRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ModularRigRoot"));
    ModularRigRoot->SetupAttachment(GetRootComponent());

    ModularTorso = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ModularTorso"));
    ModularTorso->SetupAttachment(ModularRigRoot);
    ModularPelvis = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ModularPelvis"));
    ModularPelvis->SetupAttachment(ModularRigRoot);
    ModularHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ModularHead"));
    ModularHead->SetupAttachment(ModularRigRoot);
    ModularVisor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ModularVisor"));
    ModularVisor->SetupAttachment(ModularRigRoot);
    ModularBackpack = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ModularBackpack"));
    ModularBackpack->SetupAttachment(ModularRigRoot);
    ModularLeftArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ModularLeftArm"));
    ModularLeftArm->SetupAttachment(ModularRigRoot);
    ModularRightArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ModularRightArm"));
    ModularRightArm->SetupAttachment(ModularRigRoot);
    ModularLeftLeg = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ModularLeftLeg"));
    ModularLeftLeg->SetupAttachment(ModularRigRoot);
    ModularRightLeg = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ModularRightLeg"));
    ModularRightLeg->SetupAttachment(ModularRigRoot);
    ModularLeftFoot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ModularLeftFoot"));
    ModularLeftFoot->SetupAttachment(ModularRigRoot);
    ModularRightFoot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ModularRightFoot"));
    ModularRightFoot->SetupAttachment(ModularRigRoot);
    ModularChestPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ModularChestPlate"));
    ModularChestPlate->SetupAttachment(ModularRigRoot);

    // These are deliberately attached to Manny bones instead of the capsule:
    // role identity must stay attached while the body walks, aims, reacts to a
    // hit, and plays the replicated death presentation. They remain hidden on
    // the procedural fallback, which already has its own readable armor kit.
    RoleHelmet = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoleHelmet"));
    RoleHelmet->SetupAttachment(GetMesh());
    RoleChestRig = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoleChestRig"));
    RoleChestRig->SetupAttachment(GetMesh());
    RoleBackpack = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoleBackpack"));
    RoleBackpack->SetupAttachment(GetMesh());
    RoleLeftShoulder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoleLeftShoulder"));
    RoleLeftShoulder->SetupAttachment(GetMesh());
    RoleRightShoulder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoleRightShoulder"));
    RoleRightShoulder->SetupAttachment(GetMesh());
    RoleAntenna = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoleAntenna"));
    RoleAntenna->SetupAttachment(GetMesh());

    // Keep socket-driven locations so the kit follows the animated body, but
    // do not inherit socket rotation. spine_03 and the upper-arm bones roll
    // during rifle locomotion, which made a correctly target-facing mannequin
    // visually read as a side/back-facing enemy. UpdateRoleAccessoryFacing
    // supplies one stable upright world yaw after animation evaluation.
    RoleHelmet->SetAbsolute(false, true, false);
    RoleChestRig->SetAbsolute(false, true, false);
    RoleBackpack->SetAbsolute(false, true, false);
    RoleLeftShoulder->SetAbsolute(false, true, false);
    RoleRightShoulder->SetAbsolute(false, true, false);
    RoleAntenna->SetAbsolute(false, true, false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* Cube = CubeMesh.Succeeded() ? CubeMesh.Object : nullptr;
    UStaticMesh* Sphere = SphereMesh.Succeeded() ? SphereMesh.Object : nullptr;
    UStaticMesh* Cylinder = CylinderMesh.Succeeded() ? CylinderMesh.Object : nullptr;

    // Role gear uses the same project-owned modular kit as the environment.
    // The old engine primitives were functionally correct, but made every
    // enemy read as a mannequin with debug cubes strapped to it. Keep the
    // engine meshes as a hard fallback so a source-only/editor launch still
    // has a complete silhouette when the optional generated kit is absent.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> EnvironmentBlock(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Block.SM_BM_Env_Block"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> EnvironmentOrb(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Orb.SM_BM_Env_Orb"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> EnvironmentPipe(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Pipe.SM_BM_Env_Pipe"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> EnvironmentPanel(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Panel.SM_BM_Env_Panel"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> EnvironmentCover(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Cover.SM_BM_Env_Cover"));
    UStaticMesh* ProjectBlock = EnvironmentBlock.Succeeded() ? EnvironmentBlock.Object.Get() : Cube;
    UStaticMesh* ProjectOrb = EnvironmentOrb.Succeeded() ? EnvironmentOrb.Object.Get() : Sphere;
    UStaticMesh* ProjectPipe = EnvironmentPipe.Succeeded() ? EnvironmentPipe.Object.Get() : Cylinder;
    UStaticMesh* ProjectPanel = EnvironmentPanel.Succeeded() ? EnvironmentPanel.Object.Get() : Cube;
    UStaticMesh* ProjectCover = EnvironmentCover.Succeeded() ? EnvironmentCover.Object.Get() : Cube;

    // The rig is intentionally assembled from simple project-safe primitives
    // rather than relying on engine sample content that will not be cooked
    // into a standalone package. The proportions read as a soldier at play
    // distance while every piece still follows the capsule and weapon.
    ConfigureModularPart(ModularTorso, Cube, FVector(0.0f, 0.0f, 0.0f), FVector(0.42f, 0.27f, 0.62f));
    ConfigureModularPart(ModularPelvis, Cube, FVector(-1.0f, 0.0f, -37.0f), FVector(0.32f, 0.25f, 0.20f));
    ConfigureModularPart(ModularHead, Sphere, FVector(1.0f, 0.0f, 51.0f), FVector(0.19f, 0.19f, 0.21f));
    ConfigureModularPart(ModularVisor, Cube, FVector(12.0f, 0.0f, 51.0f), FVector(0.075f, 0.145f, 0.055f));
    ConfigureModularPart(ModularBackpack, Cube, FVector(-16.0f, 0.0f, 1.0f), FVector(0.16f, 0.21f, 0.38f));
    ConfigureModularPart(ModularLeftArm, Cube, FVector(1.0f, -28.0f, 1.0f), FVector(0.16f, 0.115f, 0.34f), FRotator(-18.0f, 0.0f, 0.0f));
    ConfigureModularPart(ModularRightArm, Cube, FVector(1.0f, 28.0f, 1.0f), FVector(0.16f, 0.115f, 0.34f), FRotator(-18.0f, 0.0f, 0.0f));
    ConfigureModularPart(ModularLeftLeg, Cube, FVector(0.0f, -11.0f, -70.0f), FVector(0.15f, 0.135f, 0.39f));
    ConfigureModularPart(ModularRightLeg, Cube, FVector(0.0f, 11.0f, -70.0f), FVector(0.15f, 0.135f, 0.39f));
    ConfigureModularPart(ModularLeftFoot, Cube, FVector(13.0f, -11.0f, -108.0f), FVector(0.22f, 0.15f, 0.09f));
    ConfigureModularPart(ModularRightFoot, Cube, FVector(13.0f, 11.0f, -108.0f), FVector(0.22f, 0.15f, 0.09f));
    ConfigureModularPart(ModularChestPlate, Cube, FVector(22.0f, 0.0f, 2.0f), FVector(0.055f, 0.20f, 0.30f));

    const auto ConfigureRolePart = [](UStaticMeshComponent* Part, UStaticMesh* Mesh)
    {
        if (!Part)
        {
            return;
        }
        Part->SetStaticMesh(Mesh);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetGenerateOverlapEvents(false);
        Part->SetCanEverAffectNavigation(false);
        Part->SetCastShadow(true);
        Part->SetVisibility(false, false);
    };
    ConfigureRolePart(RoleHelmet, ProjectOrb);
    ConfigureRolePart(RoleChestRig, ProjectPanel);
    ConfigureRolePart(RoleBackpack, ProjectBlock);
    ConfigureRolePart(RoleLeftShoulder, ProjectCover);
    ConfigureRolePart(RoleRightShoulder, ProjectCover);
    ConfigureRolePart(RoleAntenna, ProjectPipe);

    // Prefer the project-cooked mannequin so packaged builds and editor runs
    // share one stable body and socket layout. The ConstructorHelpers lookup
    // is intentional: a soft path alone is not always enough for a platform
    // cook to retain a runtime-only character mesh, which previously caused
    // packaged enemies to silently switch to the MoverExamples fallback.
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> ProjectMannyMesh(
        TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    if (ProjectMannyMesh.Succeeded())
    {
        AuthoredCharacterMesh = ProjectMannyMesh.Object;
    }
    else
    {
        AuthoredCharacterMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
            TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
    }

    // Keep the serialized animation contract on the native enemy class so a
    // packaged wave does not depend on a Blueprint child being opened and
    // manually assigned. BeginPlay repeats this load as a recovery path for
    // editor sessions where the asset was created after the class was loaded.
    static ConstructorHelpers::FObjectFinder<UBMCharacterAnimationDataAsset> ProjectMannyAnimationData(
        TEXT("/Game/Data/Animations/DA_BM_Manny_Rifle_Animation.DA_BM_Manny_Rifle_Animation"));
    if (ProjectMannyAnimationData.Succeeded())
    {
        AnimationData = ProjectMannyAnimationData.Object;
    }

    // Keep a native hard class reference as a cook/runtime safety net. The
    // serialized data asset remains the source of truth, but a TSubclassOf
    // can report null until its generated class package is loaded; additive
    // fire and hit clips require this class's DefaultSlot in a packaged game.
    static ConstructorHelpers::FClassFinder<UAnimInstance> ProjectMannyAnimationBlueprint(
        TEXT("/Game/Characters/Mannequins/Animations/ABP_BM_Manny_Rifle"));
    if (ProjectMannyAnimationBlueprint.Succeeded())
    {
        DevelopmentAnimBlueprintClass = ProjectMannyAnimationBlueprint.Class;
    }

    // The project-owned rifle sequences run through AnimationSingleNode in
    // the packaged path. Keeping this null prevents an installed engine
    // sample AnimBP from changing the character presentation between builds.
    AuthoredAnimBlueprint = nullptr;
}

void ABMEnemyCharacter::BeginPlay()
{
#if !UE_BUILD_SHIPPING
    const bool bForceFallbackPresentation = FParse::Param(
        FCommandLine::Get(),
        TEXT("BM_ForceFallbackEnemyPresentation"));
    if (bForceFallbackPresentation)
    {
        // This development-only switch makes the cooked fallback path
        // testable even when the normal package contains the authored Manny
        // mesh. It never changes the normal game presentation or shipping
        // builds.
        AuthoredCharacterMesh = nullptr;
        AuthoredAnimBlueprint = nullptr;
    }
#endif
    if (!AnimationData)
    {
        AnimationData = LoadObject<UBMCharacterAnimationDataAsset>(
            nullptr,
            TEXT("/Game/Data/Animations/DA_BM_Manny_Rifle_Animation.DA_BM_Manny_Rifle_Animation"));
    }
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if (bForceFallbackPresentation && GetMesh())
    {
        GetMesh()->SetVisibility(false, true);
        GetMesh()->SetSkeletalMesh(nullptr);
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian presentation: forced modular fallback pawn=%s reason=death_box_probe"),
            *GetNameSafe(this));
    }
#endif
    AttachRoleAccessoriesToAuthoredMesh();
    CombatAimYaw = GetActorRotation().Yaw;
    SmoothedCombatAimYaw = CombatAimYaw;
    bCombatAimYawInitialized = true;
    SmoothedCombatVisualYaw = GetActorRotation().Yaw;
    bCombatVisualYawInitialized = true;
    LastFacingLocation = GetActorLocation();
    bHasFacingLocation = true;
#if !UE_BUILD_SHIPPING
    LastMovementDirectionForProbe = FVector::ZeroVector;
    MovementProbeLogTimer = 0.0f;
#endif
    InitializeModularPresentation();
    ApplyRolePresentation();
    AlignAuthoredMeshToActorFacing();
    InitializeDevelopmentLocomotion();
    if (SuppressionComponent)
    {
        SuppressionComponent->SuppressionThreshold = FMath::Max(0.1f, SuppressionThreshold);
    }
    if (HasAuthority())
    {
        GetWorld()->GetSubsystem<UBMSquadSubsystem>()->RegisterEnemy(this);
        EnsureCombatWeapon();
    }
}

void ABMEnemyCharacter::AttachRoleAccessoriesToAuthoredMesh()
{
    if (!IsUsingAuthoredCharacterMesh() || !GetMesh())
    {
        return;
    }

    struct FAccessorySocket
    {
        UStaticMeshComponent* Component = nullptr;
        FName Socket = NAME_None;
    };

    const TArray<FAccessorySocket> Accessories = {
        {RoleHelmet, TEXT("head")},
        {RoleChestRig, TEXT("spine_03")},
        {RoleBackpack, TEXT("spine_03")},
        {RoleLeftShoulder, TEXT("upperarm_l")},
        {RoleRightShoulder, TEXT("upperarm_r")},
        {RoleAntenna, TEXT("spine_03")}};

    int32 BoundAccessories = 0;
    for (const FAccessorySocket& Accessory : Accessories)
    {
        if (!Accessory.Component || !GetMesh()->DoesSocketExist(Accessory.Socket))
        {
            continue;
        }

        // The constructor intentionally attaches to the mesh without a socket
        // because the mesh is loaded in ABMCombatCharacter::BeginPlay. Bind
        // after that load so the accessory never asks a socket-less CDO for a
        // bone transform and never races the character presentation setup.
        Accessory.Component->AttachToComponent(
            GetMesh(),
            FAttachmentTransformRules::KeepRelativeTransform,
            Accessory.Socket);
        ++BoundAccessories;
    }

    if (BoundAccessories > 0)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian presentation: role accessory sockets bound pawn=%s count=%d"),
            *GetNameSafe(this),
            BoundAccessories);
    }
}

void ABMEnemyCharacter::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // HandleDeath runs on the authority, while a simulated proxy learns about
    // elimination through OnRep_Eliminated.  Keep the death presentation
    // deterministic on both paths: otherwise a remote client can render the
    // box-built fallback (or its separately replicated rifle) for one or more
    // frames after the actor has already been marked dead.
    if (bEliminated)
    {
        if (CombatWeapon)
        {
            CombatWeapon->SetActorHiddenInGame(true);
        }
        if (!IsUsingAuthoredCharacterMesh())
        {
            if (ModularRigRoot)
            {
                ModularRigRoot->SetVisibility(false, true);
            }
            if (PlaceholderBody)
            {
                PlaceholderBody->SetVisibility(false, true);
            }
        }
    }
    else
    {
        if (CombatWeapon)
        {
            CombatWeapon->SetActorHiddenInGame(false);
        }
        if (!IsUsingAuthoredCharacterMesh() && bModularPresentationReady && ModularRigRoot)
        {
            ModularRigRoot->SetVisibility(true, true);
        }
    }

    FireCooldown = FMath::Max(0.0f, FireCooldown - DeltaSeconds);
    CombatStartDelayRemaining = FMath::Max(0.0f, CombatStartDelayRemaining - DeltaSeconds);
    GrenadeCooldown = FMath::Max(0.0f, GrenadeCooldown - DeltaSeconds);

    // CharacterMovement consumes the controller's pending input after the
    // controller has selected a route. The body-facing pass runs after that
    // movement, so the visible soldier can follow the route while the weapon
    // and upper-body aim continue tracking a live combat target independently.
    UpdateMovementFacing(DeltaSeconds);

    // CombatAimYaw is authoritative, but it is replicated at the actor's
    // network update cadence. Never apply the replicated value directly to a
    // visible mesh or weapon on a client: a player moving across the screen
    // would turn the target-facing presentation into a sequence of small
    // snaps. The authority follows its live value; simulated proxies ease to
    // the newest replicated heading.
    if (!bCombatAimYawInitialized)
    {
        SmoothedCombatAimYaw = CombatAimYaw;
        bCombatAimYawInitialized = true;
    }
    else if (HasAuthority())
    {
        SmoothedCombatAimYaw = CombatAimYaw;
    }
    else
    {
        SmoothedCombatAimYaw = FMath::FixedTurn(
            SmoothedCombatAimYaw,
            CombatAimYaw,
            1440.0f * FMath::Max(DeltaSeconds, 0.0f));
    }

    if (HasAuthority())
    {
        SmoothedCombatAimPitch = CombatAimPitch;
    }
    else
    {
        SmoothedCombatAimPitch = FMath::FInterpTo(
            SmoothedCombatAimPitch,
            CombatAimPitch,
            FMath::Max(DeltaSeconds, 0.0f),
            18.0f);
    }

#if !UE_BUILD_SHIPPING
    // This sample is intentionally emitted from the character's post-physics
    // tick, after UpdateMovementFacing has consumed the capsule displacement
    // and finalized actor yaw. Logging from the AI controller is one tick too
    // early and can pair a new displacement with the previous body rotation.
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe")))
    {
        MovementProbeLogTimer -= FMath::Max(DeltaSeconds, 0.0f);
        if (MovementProbeLogTimer <= 0.0f)
        {
            MovementProbeLogTimer = 1.0f;
            const bool bCombatTargetActive = bHasCombatFacingTarget && CombatFacingTarget.IsValid();
            if (!LastMovementDirectionForProbe.IsNearlyZero() || bCombatTargetActive)
            {
                const float MovementYaw = LastMovementDirectionForProbe.Rotation().Yaw;
                const bool bMovementActive = GetVelocity().Size2D() > 20.0f;
                const float BodyYaw = GetActorRotation().Yaw;
                const float BodyToMovementYaw = FMath::FindDeltaAngleDegrees(MovementYaw, BodyYaw);
                // The capsule/legs follow the route, while the rendered
                // soldier remains target-facing even during a forward
                // pressure step. This is the readable third-person combat
                // contract: locomotion can be slightly lateral, but a hostile
                // does not look away from the player while firing.
                const bool bTargetFacing = bCombatTargetActive;
                const float TargetYaw = bTargetFacing ? CombatAimYaw : BodyYaw;
                const float BodyToTargetYaw = bTargetFacing
                    ? FMath::FindDeltaAngleDegrees(TargetYaw, BodyYaw)
                    : 0.0f;
                // Measure the same rendered front that UpdateCombatVisualFacing
                // uses. The old probe hard-coded a Manny mesh axis, so it could
                // report zero error while the player was looking at the
                // backpack after a cooked mesh/import change.
                const FVector VisualForward = GetVisibleVisualForward();
                const float VisualYaw = FRotator::NormalizeAxis(VisualForward.Rotation().Yaw);
                const float VisualToMovementYaw = bMovementActive
                    ? FMath::FindDeltaAngleDegrees(MovementYaw, VisualYaw)
                    : 0.0f;
                const float VisualToTargetYaw = bTargetFacing
                    ? FMath::FindDeltaAngleDegrees(TargetYaw, VisualYaw)
                    : 0.0f;
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian AI movement: pawn=%s location=%s body_yaw=%.1f movement_yaw=%.1f movement_speed=%.1f movement_active=%s target_yaw=%.1f aim_yaw=%.1f visual_yaw=%.1f body_to_movement_yaw=%.1f visual_to_movement_yaw=%.1f body_to_target_yaw=%.1f visual_to_target_yaw=%.1f target_facing=%s"),
                    *GetName(),
                    *GetActorLocation().ToString(),
                    BodyYaw,
                    MovementYaw,
                    GetVelocity().Size2D(),
                    bMovementActive ? TEXT("yes") : TEXT("no"),
                    TargetYaw,
                    CombatAimYaw,
                    VisualYaw,
                    BodyToMovementYaw,
                    VisualToMovementYaw,
                    BodyToTargetYaw,
                    VisualToTargetYaw,
                    bTargetFacing ? TEXT("yes") : TEXT("no"));
            }
        }
    }
#endif

    if (CombatWeapon)
    {
        if (!bCombatWeaponPresentationInitialized)
        {
            CombatWeaponBaseLocation = CombatWeapon->GetRootComponent()
                ? CombatWeapon->GetRootComponent()->GetRelativeLocation()
                : FVector::ZeroVector;
            CombatWeaponBaseRotation = CombatWeapon->GetRootComponent()
                ? CombatWeapon->GetRootComponent()->GetRelativeRotation()
                : FRotator::ZeroRotator;
            bCombatWeaponPresentationInitialized = true;
        }

        CombatWeaponKick = FMath::FInterpTo(CombatWeaponKick, 0.0f, DeltaSeconds, 22.0f);
        CombatWeaponReloadAlpha = FMath::FInterpTo(
            CombatWeaponReloadAlpha,
            CombatWeapon->IsReloading() ? 1.0f : 0.0f,
            DeltaSeconds,
            10.0f);
        const float ReloadCurve = FMath::Sin(PI * FMath::Clamp(CombatWeaponReloadAlpha, 0.0f, 1.0f));
        CombatWeapon->SetActorRelativeLocation(
            CombatWeaponBaseLocation + FVector(-CombatWeaponKick * 8.0f, 0.0f, -ReloadCurve * 12.0f));
        CombatWeapon->SetActorRelativeRotation(
            CombatWeaponBaseRotation + FRotator(-CombatWeaponKick * 9.0f, 0.0f, ReloadCurve * 4.0f));

        // The hand socket supplies the weapon's world position. Its yaw is
        // intentionally independent from the capsule/body yaw so an enemy
        // can aim at a target while advancing or strafing.
        ApplyCombatWeaponAim();
    }

    if (!bEliminated)
    {
        const float GroundSpeed = GetVelocity().Size2D();
        EBMCharacterMovementState MovementState = EBMCharacterMovementState::Idle;
        if (GetCharacterMovement() && GetCharacterMovement()->IsFalling())
        {
            MovementState = GetVelocity().Z > 20.0f
                ? EBMCharacterMovementState::Jump
                : EBMCharacterMovementState::Fall;
        }
        else if (GroundSpeed > 420.0f)
        {
            MovementState = EBMCharacterMovementState::Sprint;
        }
        else if (GroundSpeed > 20.0f)
        {
            MovementState = EBMCharacterMovementState::Walk;
        }

        const bool bAiming = AIState == EBMAIState::Engage
            || AIState == EBMAIState::Advance
            || AIState == EBMAIState::Suppressed;
        if (MovementState != LastPublishedAnimationState
            || FMath::Abs(GroundSpeed - LastPublishedAnimationSpeed) > 8.0f
            || bAiming != bLastPublishedAnimationAiming)
        {
            SetCharacterAnimationState_Implementation(MovementState, GroundSpeed, bAiming);
            LastPublishedAnimationState = MovementState;
            LastPublishedAnimationSpeed = GroundSpeed;
            bLastPublishedAnimationAiming = bAiming;
        }

    }

    UpdateModularLocomotion(DeltaSeconds);
    UpdateDevelopmentLocomotion(DeltaSeconds);

    // Locomotion/event playback can restore the skeletal component transform
    // when a walk, fire, hit, or reload sequence changes. Apply the final
    // combat-facing transform after that pass so the visible body cannot snap
    // back to its import heading for one frame.
    UpdateCombatVisualFacing(DeltaSeconds);
    UpdateRoleAccessoryFacing();
#if !UE_BUILD_SHIPPING
    UpdateCombatAxisProbe();
#endif
    if (!bEliminated)
    {
        // The mesh may carry a smoothed target-facing yaw while the capsule
        // follows a route. Feed the animation graph only the residual between
        // the final visible body heading and the weapon heading; otherwise a
        // full mesh turn plus the AnimBP aim offset twists the upper body a
        // second time and reads as a snap.
        const float VisualBodyYaw = GetActorRotation().Yaw + CombatVisualFacingYawOffset;
        const float AimYawOffset = bHasCombatFacingTarget || bReplicatedCombatFacing
            ? FMath::FindDeltaAngleDegrees(VisualBodyYaw, SmoothedCombatAimYaw)
            : 0.0f;
        SetCharacterAimOffset_Implementation(FRotator(0.0f, AimYawOffset, 0.0f));
    }
    ApplyModularPresentationTransform();

    if (SuppressionComponent && SuppressionComponent->IsSuppressed() && !bEliminated)
    {
        SetAIState(EBMAIState::Suppressed);
    }
}

void ABMEnemyCharacter::UpdateMovementFacing(const float DeltaSeconds)
{
    // The server owns the capsule's authoritative heading. Remote clients
    // receive that movement rotation and only apply the replicated visual
    // aim offset below; locally rotating a simulated proxy from its
    // interpolated displacement would fight CharacterMovement smoothing and
    // reintroduce network-only jitter.
    if (bEliminated || !HasAuthority() || DeltaSeconds <= 0.0f)
    {
        return;
    }

    const FVector CurrentLocation = GetActorLocation();
    const FVector Displacement = bHasFacingLocation
        ? (CurrentLocation - LastFacingLocation)
        : FVector::ZeroVector;
    LastFacingLocation = CurrentLocation;
    bHasFacingLocation = true;

    FVector DesiredFacingDirection = FVector::ZeroVector;
    bool bFacingCombatTarget = false;
    if (bHasCombatFacingTarget)
    {
        if (AActor* Target = CombatFacingTarget.Get(); IsValid(Target))
        {
            CombatFacingLocation = Target->GetActorLocation();
        }
        else
        {
            ClearCombatFacingTarget();
        }

        if (bHasCombatFacingTarget)
        {
            FVector ToTarget = CombatFacingLocation - CurrentLocation;
            ToTarget.Z = 0.0f;
            DesiredFacingDirection = ToTarget.GetSafeNormal2D();
            bFacingCombatTarget = !DesiredFacingDirection.IsNearlyZero();
            const FVector AimTarget = CombatFacingLocation + FVector(0.0f, 0.0f, 72.0f);
            const FVector ToAimTarget = AimTarget - CurrentLocation;
            if (!ToAimTarget.IsNearlyZero())
            {
                CombatAimPitch = ToAimTarget.Rotation().Pitch;
            }
            if (bFacingCombatTarget && HasAuthority())
            {
                // Keep one bounded combat heading filter. The capsule and
                // rendered mesh consume this same heading below; separate
                // filters for target, actor, and mesh yaw made the body lag
                // the weapon and read as a twitch during pursuit.
                const float TargetYaw = DesiredFacingDirection.Rotation().Yaw;
                const float TargetYawDelta = FMath::FindDeltaAngleDegrees(
                    CombatAimYaw,
                    TargetYaw);
                if (FMath::Abs(TargetYawDelta)
                    > BMCombatTuning::EnemyCombatTargetFacingDeadbandDegrees)
                {
                    CombatAimYaw = FMath::FixedTurn(
                        CombatAimYaw,
                        TargetYaw,
                        BMCombatTuning::EnemyCombatTargetFacingRateDegreesPerSecond * DeltaSeconds);
                }
            }
        }
    }

    // Prefer the direction CharacterMovement was actually asked to follow over
    // a collision-resolved velocity slide. A capsule grazing cover can report
    // a one-frame lateral velocity; turning the body to that slide and then
    // back to the route is the visible jitter reported during combat.
    const FVector GroundVelocity = GetVelocity();
    const float GroundSpeed = GroundVelocity.Size2D();
    const bool bMovementActive = GroundSpeed > 20.0f;
    const FVector RequestedDirection = GetLastMovementInputVector().GetSafeNormal2D();
    const FVector DisplacementDirection = Displacement.GetSafeNormal2D();
    // Use the direction the capsule actually travelled when it is available.
    // LastMovementInputVector can remain valid for one frame after a waypoint
    // hand-off, while a collision can resolve the capsule into a different
    // direction. Feeding that stale request into body yaw was the source of
    // the visible left/right twitch at movement transitions.
    const FVector RawTravelDirection = !DisplacementDirection.IsNearlyZero()
        ? DisplacementDirection
        : (!GroundVelocity.IsNearlyZero()
            ? GroundVelocity.GetSafeNormal2D()
            : RequestedDirection);
    if (bMovementActive && !RawTravelDirection.IsNearlyZero())
    {
        if (SmoothedTravelDirection.IsNearlyZero())
        {
            SmoothedTravelDirection = RawTravelDirection;
        }
        else
        {
            const float SmoothedYaw = FMath::FixedTurn(
                SmoothedTravelDirection.Rotation().Yaw,
                RawTravelDirection.Rotation().Yaw,
                BMCombatTuning::EnemyMovementSteeringRateDegreesPerSecond * DeltaSeconds);
            SmoothedTravelDirection = FRotator(0.0f, SmoothedYaw, 0.0f).Vector().GetSafeNormal2D();
        }
    }
    const FVector TravelDirection = bMovementActive
        ? SmoothedTravelDirection
        : FVector::ZeroVector;
#if !UE_BUILD_SHIPPING
    LastMovementDirectionForProbe = TravelDirection;
#endif
    // A hostile target owns the complete body heading, including while the
    // enemy is advancing. Previously a moving enemy replaced this with its
    // route heading and relied on the mesh/weapon offset to compensate. That
    // left the authoritative capsule and the visible Manny looking down the
    // lane while the muzzle fired at the player. The fallback movement route
    // is already constrained to a readable forward cone, so the enemy can
    // keep walking toward the player without presenting its back or shoulder.
    if (bFacingCombatTarget)
    {
        DesiredFacingDirection = FRotator(0.0f, CombatAimYaw, 0.0f).Vector().GetSafeNormal2D();
    }
    else if (bMovementActive)
    {
        DesiredFacingDirection = TravelDirection;
    }
    if (DesiredFacingDirection.IsNearlyZero())
    {
        return;
    }

    const float DesiredYaw = DesiredFacingDirection.Rotation().Yaw;
    const FRotator CurrentRotation = GetActorRotation();
    const float NewYaw = FMath::FixedTurn(
        CurrentRotation.Yaw,
        DesiredYaw,
        BMCombatTuning::EnemyMovementRotationRateDegreesPerSecond * DeltaSeconds);
    if (FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, NewYaw)) <= 0.05f)
    {
        return;
    }

    // Turn at a bounded rate. Snapping to each one-frame displacement made a
    // collision slide or a small target correction read as visible jitter.
    SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
}

FVector ABMEnemyCharacter::GetVisibleVisualForward() const
{
    if (IsUsingAuthoredCharacterMesh() && GetMesh())
    {
        if (bAuthoredVisualForwardCalibrated)
        {
            return GetMesh()->GetComponentTransform()
                .TransformVectorNoScale(AuthoredVisualForwardLocal)
                .GetSafeNormal2D();
        }

        // The project Manny's rendered front is its local +Y basis. Do not
        // infer this from RoleChestRig/RoleBackpack: both accessories are
        // attached to spine_03, so their socket-space offsets include the
        // animated spine basis and can produce a plausible but wrong heading.
        return GetMesh()->GetComponentTransform()
            .TransformVectorNoScale(FVector::RightVector)
            .GetSafeNormal2D();
    }

    if (ModularChestPlate && ModularBackpack)
    {
        const FVector ChestToBack = ModularChestPlate->GetComponentLocation()
            - ModularBackpack->GetComponentLocation();
        if (!ChestToBack.IsNearlyZero())
        {
            return ChestToBack.GetSafeNormal2D();
        }
    }

    if (GetMesh())
    {
        return GetMesh()->GetForwardVector().GetSafeNormal2D();
    }
    return GetActorForwardVector().GetSafeNormal2D();
}

void ABMEnemyCharacter::AlignAuthoredMeshToActorFacing()
{
    if (!IsUsingAuthoredCharacterMesh() || !GetMesh())
    {
        return;
    }

    // The project Manny asset has a stable rendered front on local +Y. The
    // previous chest/backpack calibration was self-referential: both pieces
    // are attached to spine_03, so their animated socket basis could report a
    // zero-error vector while the rendered mannequin still presented its
    // backpack to the player. Keep this basis independent from the role
    // accessories and from the combat target.
    const FTransform MeshTransformBefore = GetMesh()->GetComponentTransform();
    const FVector LocalVisualForward = FVector::RightVector;
    const FVector VisualForwardBefore = MeshTransformBefore
        .TransformVectorNoScale(LocalVisualForward)
        .GetSafeNormal2D();
    const float ActorYaw = GetActorRotation().Yaw;
    const float VisualYawBefore = VisualForwardBefore.IsNearlyZero()
        ? ActorYaw
        : VisualForwardBefore.Rotation().Yaw;
    const float VisualForwardCorrection = FMath::FindDeltaAngleDegrees(
        VisualYawBefore,
        ActorYaw);

    FRotator CorrectedRotation = GetMesh()->GetRelativeRotation();
    CorrectedRotation.Yaw = FRotator::NormalizeAxis(
        CorrectedRotation.Yaw + VisualForwardCorrection);
    GetMesh()->SetRelativeRotation(CorrectedRotation);
    SetAuthoredMeshBaseRotation(CorrectedRotation);
    AuthoredVisualForwardLocal = LocalVisualForward;
    bAuthoredVisualForwardCalibrated = true;

    const float VisualYawAfter = GetVisibleVisualForward().Rotation().Yaw;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian presentation: enemy mesh facing calibrated pawn=%s visible_yaw_before=%.1f correction=%.1f visible_yaw_after=%.1f base_rel_yaw=%.1f source=project_manny_local_plus_y"),
        *GetNameSafe(this),
        VisualYawBefore,
        VisualForwardCorrection,
        VisualYawAfter,
        CorrectedRotation.Yaw);
}

void ABMEnemyCharacter::UpdateCombatVisualFacing(const float DeltaSeconds)
{
    if (bEliminated || DeltaSeconds <= 0.0f)
    {
        return;
    }

    const bool bCombatTargetActive = bReplicatedCombatFacing
        || (bHasCombatFacingTarget && CombatFacingTarget.IsValid());
    // The live hostile target owns the rendered combat heading, including
    // while the enemy is advancing. The capsule can follow a collision-safe
    // navigation route independently, while the rifle-ready mesh and weapon
    // keep looking at the player. Previously this was gated by
    // !bMovementActive, so the exact moment an enemy started coming toward the
    // player its visible body snapped back to route-facing and presented a
    // shoulder/back even though the muzzle continued to aim correctly.
    const bool bTargetFacing = bCombatTargetActive;
    // CombatVisualFacingYawOffset is expressed relative to the actor's
    // conventional +X heading. AuthoredMeshBaseRotation already contains the
    // Manny import correction; including that base correction in this delta
    // would rotate the visible body another quarter-turn away from the target.
    const float ActorWorldYaw = GetActorRotation().Yaw;
    const float DesiredWorldYaw = bTargetFacing
        ? SmoothedCombatAimYaw
        : ActorWorldYaw;
    if (!bCombatVisualYawInitialized)
    {
        SmoothedCombatVisualYaw = DesiredWorldYaw;
        bCombatVisualYawInitialized = true;
    }
    else
    {
        SmoothedCombatVisualYaw = FMath::FixedTurn(
            SmoothedCombatVisualYaw,
            DesiredWorldYaw,
            900.0f * DeltaSeconds);
    }
    const float NewVisualYawOffset = FMath::FindDeltaAngleDegrees(
        ActorWorldYaw,
        SmoothedCombatVisualYaw);

    if (IsUsingAuthoredCharacterMesh() && GetMesh())
    {
        // Write the desired local yaw from the cached authored base instead
        // of accumulating AddLocalRotation deltas. Hit/death presentation and
        // single-node animation can legitimately rewrite the component
        // transform; absolute re-anchoring removes that reset/accumulate race
        // and keeps the rendered soldier facing the same heading as the
        // capsule/weapon.
        FRotator DesiredMeshRotation = GetMesh()->GetRelativeRotation();
        DesiredMeshRotation.Yaw = GetAuthoredMeshBaseRotation().Yaw + NewVisualYawOffset;
        GetMesh()->SetRelativeRotation(DesiredMeshRotation);
        LastExpectedMeshRelativeYaw = DesiredMeshRotation.Yaw;
        bCombatVisualFacingInitialized = true;
    }

    CombatVisualFacingYawOffset = NewVisualYawOffset;
}

void ABMEnemyCharacter::UpdateRoleAccessoryFacing()
{
    if (!IsUsingAuthoredCharacterMesh() || !GetMesh())
    {
        return;
    }

    const FVector VisualForward = GetVisibleVisualForward();
    if (VisualForward.IsNearlyZero())
    {
        return;
    }

    // The kit meshes use local +X as their front/back thickness axis, matching
    // Manny's visible front. Give the kit the same world heading
    // as the rendered mannequin and deliberately remove animated bone pitch /
    // roll. Their positions still come from head/spine/arm sockets, so the
    // armor follows the body without advertising a false facing direction.
    const FRotator StableAccessoryRotation(
        0.0f,
        VisualForward.Rotation().Yaw,
        0.0f);
    const TArray<UStaticMeshComponent*> Accessories = {
        RoleHelmet,
        RoleChestRig,
        RoleBackpack,
        RoleLeftShoulder,
        RoleRightShoulder,
        RoleAntenna};
    for (UStaticMeshComponent* Accessory : Accessories)
    {
        if (Accessory && Accessory->IsVisible())
        {
            Accessory->SetWorldRotation(StableAccessoryRotation);
        }
    }
}

#if !UE_BUILD_SHIPPING
void ABMEnemyCharacter::UpdateCombatAxisProbe()
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("BM_CombatAxisProbe"))
        || !GetWorld()
        || !GetMesh()
        || bEliminated)
    {
        return;
    }

    const FTransform MeshTransform = GetMesh()->GetComponentTransform();
    const FVector MeshOrigin = MeshTransform.GetLocation() + FVector(0.0f, 0.0f, 24.0f);
    const FVector ActorOrigin = GetActorLocation() + FVector(0.0f, 0.0f, 112.0f);
    const float AxisLength = 120.0f;

    FVector TargetDirection = FVector::ZeroVector;
    if (bHasCombatFacingTarget)
    {
        TargetDirection = CombatFacingLocation - GetActorLocation();
        TargetDirection.Z = 0.0f;
        TargetDirection.Normalize();
    }

    FVector ChestToBackDirection = FVector::ZeroVector;
    if (RoleChestRig && RoleBackpack)
    {
        ChestToBackDirection = (
            RoleChestRig->GetComponentLocation() - RoleBackpack->GetComponentLocation())
            .GetSafeNormal2D();
    }
    const FVector MeshPlusX = MeshTransform.TransformVectorNoScale(FVector::ForwardVector).GetSafeNormal2D();
    const FVector MeshPlusY = MeshTransform.TransformVectorNoScale(FVector::RightVector).GetSafeNormal2D();
    const FVector RightHandLocation = GetMesh()->GetSocketLocation(TEXT("hand_r"));
    const FVector LeftHandLocation = GetMesh()->GetSocketLocation(TEXT("hand_l"));
    const FVector WeaponGripLocation = CombatWeapon ? CombatWeapon->GetGripLocation() : FVector::ZeroVector;
    const FVector WeaponMuzzleLocation = CombatWeapon ? CombatWeapon->GetMuzzleLocation() : FVector::ZeroVector;
    const FVector WeaponSupportLocation = CombatWeapon
        ? FMath::Lerp(WeaponGripLocation, WeaponMuzzleLocation, 0.38f)
        : FVector::ZeroVector;

    if (!bCombatAxisProbeScreenshotRequested)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian axis probe: pawn=%s actor_yaw=%.1f target_yaw=%.1f chest_to_back_yaw=%.1f mesh_plus_x_yaw=%.1f mesh_plus_y_yaw=%.1f visible_yaw=%.1f"),
            *GetNameSafe(this),
            GetActorRotation().Yaw,
            TargetDirection.IsNearlyZero() ? 0.0f : TargetDirection.Rotation().Yaw,
            ChestToBackDirection.IsNearlyZero() ? 0.0f : ChestToBackDirection.Rotation().Yaw,
            MeshPlusX.IsNearlyZero() ? 0.0f : MeshPlusX.Rotation().Yaw,
            MeshPlusY.IsNearlyZero() ? 0.0f : MeshPlusY.Rotation().Yaw,
            GetVisibleVisualForward().IsNearlyZero() ? 0.0f : GetVisibleVisualForward().Rotation().Yaw);
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian weapon hold probe: pawn=%s hand_r=%s hand_l=%s grip=%s support=%s muzzle=%s left_to_support_cm=%.1f"),
            *GetNameSafe(this),
            *RightHandLocation.ToString(),
            *LeftHandLocation.ToString(),
            *WeaponGripLocation.ToString(),
            *WeaponSupportLocation.ToString(),
            *WeaponMuzzleLocation.ToString(),
            FVector::Dist(LeftHandLocation, WeaponSupportLocation));
    }

    const auto DrawAxis = [this, &MeshTransform, &MeshOrigin, AxisLength](
        const TCHAR* Label,
        const FVector& LocalDirection,
        const FColor& Color)
    {
        const FVector WorldDirection = MeshTransform.TransformVectorNoScale(LocalDirection).GetSafeNormal2D();
        if (WorldDirection.IsNearlyZero())
        {
            return;
        }
        const FVector End = MeshOrigin + WorldDirection * AxisLength;
        DrawDebugDirectionalArrow(
            GetWorld(),
            MeshOrigin,
            End,
            18.0f,
            Color,
            false,
            0.12f,
            0,
            3.0f);
        DrawDebugString(GetWorld(), End, Label, nullptr, Color, 0.12f, false);
    };

    DrawAxis(TEXT("MESH +X"), FVector::ForwardVector, FColor::Green);
    DrawAxis(TEXT("MESH -X"), -FVector::ForwardVector, FColor::Red);
    DrawAxis(TEXT("MESH +Y"), FVector::RightVector, FColor::Blue);
    DrawAxis(TEXT("MESH -Y"), -FVector::RightVector, FColor::Yellow);

    const FVector ActorForward = GetActorForwardVector().GetSafeNormal2D();
    DrawDebugDirectionalArrow(
        GetWorld(),
        ActorOrigin,
        ActorOrigin + ActorForward * AxisLength,
        18.0f,
        FColor::White,
        false,
        0.12f,
        0,
        4.0f);
    DrawDebugString(
        GetWorld(),
        ActorOrigin + ActorForward * AxisLength,
        TEXT("ACTOR FORWARD"),
        nullptr,
        FColor::White,
        0.12f,
        false);

    if (!TargetDirection.IsNearlyZero())
    {
        DrawDebugDirectionalArrow(
            GetWorld(),
            ActorOrigin,
            ActorOrigin + TargetDirection * AxisLength,
            18.0f,
            FColor::Magenta,
            false,
            0.12f,
            0,
            4.0f);
        DrawDebugString(
            GetWorld(),
            ActorOrigin + TargetDirection * AxisLength,
            TEXT("TARGET"),
            nullptr,
            FColor::Magenta,
            0.12f,
            false);
    }

    if (RoleChestRig)
    {
        const FVector ChestLocation = RoleChestRig->GetComponentLocation();
        DrawDebugSphere(GetWorld(), ChestLocation, 12.0f, 12, FColor::Cyan, false, 0.12f, 0, 2.5f);
        DrawDebugString(GetWorld(), ChestLocation + FVector(0.0f, 0.0f, 18.0f), TEXT("CHEST PART"), nullptr, FColor::Cyan, 0.12f, false);
    }
    if (RoleBackpack)
    {
        const FVector BackpackLocation = RoleBackpack->GetComponentLocation();
        DrawDebugSphere(GetWorld(), BackpackLocation, 12.0f, 12, FColor::Orange, false, 0.12f, 0, 2.5f);
        DrawDebugString(GetWorld(), BackpackLocation + FVector(0.0f, 0.0f, 18.0f), TEXT("BACKPACK PART"), nullptr, FColor::Orange, 0.12f, false);
    }

    DrawDebugSphere(GetWorld(), RightHandLocation, 7.0f, 10, FColor::Green, false, 0.12f, 0, 2.0f);
    DrawDebugString(GetWorld(), RightHandLocation + FVector(0.0f, 0.0f, 12.0f), TEXT("RIGHT HAND"), nullptr, FColor::Green, 0.12f, false);
    DrawDebugSphere(GetWorld(), LeftHandLocation, 7.0f, 10, FColor::Blue, false, 0.12f, 0, 2.0f);
    DrawDebugString(GetWorld(), LeftHandLocation + FVector(0.0f, 0.0f, 12.0f), TEXT("LEFT HAND"), nullptr, FColor::Blue, 0.12f, false);
    if (CombatWeapon)
    {
        DrawDebugSphere(GetWorld(), WeaponGripLocation, 6.0f, 10, FColor::White, false, 0.12f, 0, 2.0f);
        DrawDebugSphere(GetWorld(), WeaponSupportLocation, 6.0f, 10, FColor::Yellow, false, 0.12f, 0, 2.0f);
        DrawDebugLine(GetWorld(), LeftHandLocation, WeaponSupportLocation, FColor::Yellow, false, 0.12f, 0, 2.0f);
        DrawDebugString(GetWorld(), WeaponSupportLocation + FVector(0.0f, 0.0f, 12.0f), TEXT("FOREGRIP"), nullptr, FColor::Yellow, 0.12f, false);
    }

    if (!bCombatAxisProbeScreenshotRequested)
    {
        const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("Screenshots/BMCombatAxisProbe.png");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);
        FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false, false, FIntRect(), true);
        bCombatAxisProbeScreenshotRequested = true;
    }
}
#endif

void ABMEnemyCharacter::InitializeModularPresentation()
{
    if (IsUsingAuthoredCharacterMesh())
    {
        bModularPresentationReady = false;
        if (ModularRigRoot)
        {
            ModularRigRoot->SetVisibility(false, true);
        }
        return;
    }

    if (!ModularRigRoot || !ModularTorso || !ModularTorso->GetStaticMesh())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Black Meridian presentation: modular enemy fallback unavailable pawn=%s"),
            *GetNameSafe(this));
        return;
    }

    // PlaceholderBody remains the source of hit/death offsets, but the
    // distorted cube itself is no longer rendered when the modular fallback
    // is available. Keeping the rig at unit scale prevents the placeholder's
    // collision/debug proportions from squashing the soldier pieces.
    if (PlaceholderBody)
    {
        PlaceholderBody->SetVisibility(false, true);
    }
    ModularRigRoot->SetRelativeScale3D(FVector::OneVector);
    ModularRigRoot->SetVisibility(true, true);

    ModularLeftLegBaseLocation = ModularLeftLeg->GetRelativeLocation();
    ModularRightLegBaseLocation = ModularRightLeg->GetRelativeLocation();
    ModularLeftLegBaseRotation = ModularLeftLeg->GetRelativeRotation();
    ModularRightLegBaseRotation = ModularRightLeg->GetRelativeRotation();
    ModularLeftArmBaseLocation = ModularLeftArm->GetRelativeLocation();
    ModularRightArmBaseLocation = ModularRightArm->GetRelativeLocation();
    ModularLeftArmBaseRotation = ModularLeftArm->GetRelativeRotation();
    ModularRightArmBaseRotation = ModularRightArm->GetRelativeRotation();
    ModularTorsoBaseLocation = ModularTorso->GetRelativeLocation();
    ModularLocomotionPhase = 0.0f;
    bModularPresentationReady = true;

    ConfigureModularMaterials();
    ApplyModularPresentationTransform();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian presentation: modular enemy fallback active pawn=%s role=%s"),
        *GetNameSafe(this),
        GetRoleLabel(EnemyRole));
}

void ABMEnemyCharacter::ApplyModularPresentationTransform()
{
    if (!bModularPresentationReady || !ModularRigRoot)
    {
        return;
    }

    // The modular pieces are authored around the character actor's capsule
    // center, while PlaceholderBody is intentionally lifted to the visual
    // torso center.  Following PlaceholderBody directly therefore raises the
    // fallback rig and leaves both feet visibly above the ground when a cooked
    // build cannot resolve the skeletal Manny asset.  Keep hit/death offsets
    // from the placeholder, but remove the fixed visual lift so the feet sit at
    // the capsule ground plane.
    constexpr float ModularRigGroundingOffsetZ = -68.0f;
    if (PlaceholderBody)
    {
        ModularRigRoot->SetRelativeLocation(
            PlaceholderBody->GetRelativeLocation() + FVector(0.0f, 0.0f, ModularRigGroundingOffsetZ));
        ModularRigRoot->SetRelativeRotation(
            PlaceholderBody->GetRelativeRotation()
            + FRotator(0.0f, CombatVisualFacingYawOffset, 0.0f));
    }
    else
    {
        ModularRigRoot->SetRelativeLocation(FVector(0.0f, 0.0f, ModularRigGroundingOffsetZ));
        ModularRigRoot->SetRelativeRotation(FRotator(0.0f, CombatVisualFacingYawOffset, 0.0f));
    }

    // PlaceholderBody's scale is intentionally animated during hit/death
    // presentation. Do not inherit it: the modular pieces should keep their
    // authored proportions while the root still follows its offset/rotation.
    ModularRigRoot->SetRelativeScale3D(FVector::OneVector);
}

void ABMEnemyCharacter::ConfigureModularMaterials()
{
    if (!bModularPresentationReady)
    {
        return;
    }

    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
        nullptr,
        TEXT("/Game/Environment/Materials/MI_BM_IndustrialSurface.MI_BM_IndustrialSurface"));
    if (!BaseMaterial)
    {
        BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr,
            TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    }
    if (!BaseMaterial)
    {
        BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr,
            TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
    }
    if (!BaseMaterial)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Black Meridian presentation: modular materials unavailable pawn=%s"),
            *GetNameSafe(this));
        return;
    }

    const FLinearColor RoleColor = GetRoleLinearColor(EnemyRole);
    const FLinearColor ArmorColor(
        FMath::Clamp(0.018f + RoleColor.R * 0.58f, 0.0f, 1.0f),
        FMath::Clamp(0.022f + RoleColor.G * 0.58f, 0.0f, 1.0f),
        FMath::Clamp(0.030f + RoleColor.B * 0.58f, 0.0f, 1.0f),
        1.0f);
    const FLinearColor DarkArmor(0.012f, 0.018f, 0.028f, 1.0f);
    const FLinearColor VisorColor(
        FMath::Clamp(RoleColor.R * 0.34f + 0.02f, 0.0f, 1.0f),
        FMath::Clamp(RoleColor.G * 0.70f + 0.10f, 0.0f, 1.0f),
        FMath::Clamp(RoleColor.B * 0.80f + 0.16f, 0.0f, 1.0f),
        1.0f);
    const FLinearColor AccentColor(
        FMath::Clamp(RoleColor.R * 0.86f + 0.08f, 0.0f, 1.0f),
        FMath::Clamp(RoleColor.G * 0.86f + 0.08f, 0.0f, 1.0f),
        FMath::Clamp(RoleColor.B * 0.86f + 0.08f, 0.0f, 1.0f),
        1.0f);

    ModularMaterials.Reset();
    auto ApplyMaterial = [this, BaseMaterial](UStaticMeshComponent* Part, const FLinearColor& Color, const float EmissiveStrength)
    {
        if (!Part)
        {
            return;
        }

        UMaterialInstanceDynamic* DynamicMaterial = Part->CreateDynamicMaterialInstance(0, BaseMaterial);
        if (!DynamicMaterial)
        {
            return;
        }

        DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
        DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
        DynamicMaterial->SetVectorParameterValue(
            TEXT("EmissiveColor"),
            FLinearColor(Color.R * EmissiveStrength, Color.G * EmissiveStrength, Color.B * EmissiveStrength, 1.0f));
        DynamicMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.58f);
        Part->SetVisibility(true, false);
        ModularMaterials.Add(DynamicMaterial);
    };

    ApplyMaterial(ModularTorso, ArmorColor, 0.08f);
    ApplyMaterial(ModularPelvis, DarkArmor, 0.02f);
    ApplyMaterial(ModularHead, DarkArmor, 0.02f);
    ApplyMaterial(ModularVisor, VisorColor, 0.52f);
    ApplyMaterial(ModularBackpack, DarkArmor, 0.02f);
    ApplyMaterial(ModularLeftArm, ArmorColor, 0.05f);
    ApplyMaterial(ModularRightArm, ArmorColor, 0.05f);
    ApplyMaterial(ModularLeftLeg, DarkArmor, 0.02f);
    ApplyMaterial(ModularRightLeg, DarkArmor, 0.02f);
    ApplyMaterial(ModularLeftFoot, DarkArmor, 0.02f);
    ApplyMaterial(ModularRightFoot, DarkArmor, 0.02f);
    ApplyMaterial(ModularChestPlate, AccentColor, 0.30f);
}

void ABMEnemyCharacter::ConfigureRoleAccessories()
{
    const TArray<UStaticMeshComponent*> Accessories = {
        RoleHelmet,
        RoleChestRig,
        RoleBackpack,
        RoleLeftShoulder,
        RoleRightShoulder,
        RoleAntenna};

    for (UStaticMeshComponent* Accessory : Accessories)
    {
        if (Accessory)
        {
            Accessory->SetVisibility(false, false);
            Accessory->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }

    // A packaged build may legitimately use the procedural rig when a body
    // asset is unavailable. Do not stack two different armor silhouettes in
    // that case; the modular fallback remains the compatibility presentation.
    if (!IsUsingAuthoredCharacterMesh() || !GetMesh())
    {
        return;
    }

    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
        nullptr,
        TEXT("/Game/Environment/Materials/MI_BM_IndustrialSurface.MI_BM_IndustrialSurface"));
    if (!BaseMaterial)
    {
        BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr,
            TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    }
    if (!BaseMaterial)
    {
        BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr,
            TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
    }
    if (!BaseMaterial)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Black Meridian presentation: role accessory material unavailable pawn=%s"),
            *GetNameSafe(this));
        return;
    }

    const FLinearColor RoleColor = GetRoleLinearColor(EnemyRole);
    const FLinearColor ArmorColor(0.025f, 0.040f, 0.055f, 1.0f);
    const FLinearColor RoleMetal(
        FMath::Clamp(0.055f + RoleColor.R * 0.42f, 0.0f, 1.0f),
        FMath::Clamp(0.075f + RoleColor.G * 0.42f, 0.0f, 1.0f),
        FMath::Clamp(0.095f + RoleColor.B * 0.42f, 0.0f, 1.0f),
        1.0f);
    const FLinearColor RoleGlow(
        FMath::Clamp(RoleColor.R * 0.80f + 0.10f, 0.0f, 1.0f),
        FMath::Clamp(RoleColor.G * 0.80f + 0.10f, 0.0f, 1.0f),
        FMath::Clamp(RoleColor.B * 0.80f + 0.10f, 0.0f, 1.0f),
        1.0f);

    const bool bAssault = EnemyRole == EBMEnemyRole::Assault;
    const bool bMarksman = EnemyRole == EBMEnemyRole::Marksman;
    const bool bBreacher = EnemyRole == EBMEnemyRole::Breacher;
    const bool bDemolitionist = EnemyRole == EBMEnemyRole::Demolitionist;

    const auto ApplyMaterial = [BaseMaterial](
        UStaticMeshComponent* Part,
        const FLinearColor& Color,
        const float EmissiveStrength)
    {
        if (!Part)
        {
            return;
        }

        UMaterialInstanceDynamic* Material = Part->CreateDynamicMaterialInstance(0, BaseMaterial);
        if (!Material)
        {
            return;
        }

        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
        Material->SetVectorParameterValue(
            TEXT("EmissiveColor"),
            FLinearColor(
                Color.R * EmissiveStrength,
                Color.G * EmissiveStrength,
                Color.B * EmissiveStrength,
                1.0f));
        Material->SetScalarParameterValue(TEXT("Metallic"), 0.48f);
        Material->SetScalarParameterValue(TEXT("Roughness"), 0.52f);
        Part->SetVisibility(true, false);
    };

    // All roles get a restrained shared kit, then the silhouette expands at
    // the points that communicate the role from a combat distance.
    RoleHelmet->SetRelativeLocation(FVector(0.0f, 0.0f, 1.5f));
    RoleHelmet->SetRelativeRotation(FRotator::ZeroRotator);
    RoleHelmet->SetRelativeScale3D(FVector(0.235f, 0.205f, bBreacher || bDemolitionist ? 0.145f : 0.115f));
    ApplyMaterial(RoleHelmet, ArmorColor, 0.02f);

    RoleChestRig->SetRelativeLocation(FVector(12.0f, 0.0f, 0.0f));
    RoleChestRig->SetRelativeRotation(FRotator::ZeroRotator);
    RoleChestRig->SetRelativeScale3D(FVector(0.055f, bBreacher || bDemolitionist ? 0.31f : 0.265f, bMarksman ? 0.19f : (bDemolitionist ? 0.285f : 0.225f)));
    ApplyMaterial(RoleChestRig, RoleMetal, 0.08f);

    RoleBackpack->SetRelativeLocation(FVector(-13.0f, 0.0f, -1.0f));
    RoleBackpack->SetRelativeRotation(FRotator::ZeroRotator);
    RoleBackpack->SetRelativeScale3D(FVector(0.16f, bAssault ? 0.22f : (bDemolitionist ? 0.34f : 0.27f), bBreacher || bDemolitionist ? 0.34f : 0.27f));
    ApplyMaterial(RoleBackpack, ArmorColor, 0.02f);

    const FVector ShoulderScale = bBreacher || bDemolitionist
        ? FVector(0.19f, 0.235f, 0.125f)
        : (bAssault ? FVector(0.18f, 0.205f, 0.11f) : FVector(0.15f, 0.17f, 0.095f));
    RoleLeftShoulder->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
    RoleLeftShoulder->SetRelativeRotation(FRotator::ZeroRotator);
    RoleLeftShoulder->SetRelativeScale3D(ShoulderScale);
    ApplyMaterial(RoleLeftShoulder, RoleMetal, 0.04f);
    RoleRightShoulder->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
    RoleRightShoulder->SetRelativeRotation(FRotator::ZeroRotator);
    RoleRightShoulder->SetRelativeScale3D(ShoulderScale);
    ApplyMaterial(RoleRightShoulder, RoleMetal, 0.04f);

    // Marksmen carry a deliberately tall comms/optic silhouette. The other
    // roles stay lower and broader so the player can classify them by shape
    // before reading the HUD label or role color.
    RoleAntenna->SetRelativeLocation(FVector(-16.0f, 0.0f, bMarksman ? 24.0f : (bDemolitionist ? 18.0f : 14.0f)));
    RoleAntenna->SetRelativeRotation(FRotator::ZeroRotator);
    RoleAntenna->SetRelativeScale3D(FVector(0.032f, 0.032f, bMarksman ? 0.34f : (bDemolitionist ? 0.22f : 0.16f)));
    ApplyMaterial(RoleAntenna, RoleGlow, bMarksman ? 0.72f : (bDemolitionist ? 0.52f : 0.30f));
    if (RoleAntenna)
    {
        RoleAntenna->SetVisibility(true, false);
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian presentation: role accessory kit active pawn=%s role=%s helmet=yes chest=yes shoulders=yes antenna=%s"),
        *GetNameSafe(this),
        GetRoleLabel(EnemyRole),
        bMarksman ? TEXT("tall") : (bDemolitionist ? TEXT("mid") : TEXT("low")));
}

void ABMEnemyCharacter::UpdateModularLocomotion(const float DeltaSeconds)
{
    if (!bModularPresentationReady)
    {
        return;
    }

    if (bEliminated)
    {
        bModularLocomotionMoving = false;
        ModularLeftLeg->SetRelativeLocation(ModularLeftLegBaseLocation);
        ModularRightLeg->SetRelativeLocation(ModularRightLegBaseLocation);
        ModularLeftLeg->SetRelativeRotation(ModularLeftLegBaseRotation);
        ModularRightLeg->SetRelativeRotation(ModularRightLegBaseRotation);
        ModularLeftArm->SetRelativeLocation(ModularLeftArmBaseLocation);
        ModularRightArm->SetRelativeLocation(ModularRightArmBaseLocation);
        ModularLeftArm->SetRelativeRotation(ModularLeftArmBaseRotation);
        ModularRightArm->SetRelativeRotation(ModularRightArmBaseRotation);
        ModularTorso->SetRelativeLocation(ModularTorsoBaseLocation);
        return;
    }

    const float GroundSpeed = GetVelocity().Size2D();
    const float LocomotionAlpha = FMath::Clamp(GroundSpeed / 360.0f, 0.0f, 1.0f);
    // CharacterMovement can briefly report a small residual speed while a
    // combat waypoint is being accepted. Use hysteresis so the procedural
    // fallback does not alternate walk/idle poses at the stop boundary.
    const float MovingThreshold = bModularLocomotionMoving ? 10.0f : 28.0f;
    bModularLocomotionMoving = GroundSpeed > MovingThreshold;
    const bool bMoving = bModularLocomotionMoving;
    const float Frequency = bMoving
        ? FMath::Lerp(3.4f, 6.8f, LocomotionAlpha)
        : 1.7f;
    ModularLocomotionPhase = FMath::Fmod(ModularLocomotionPhase + DeltaSeconds * Frequency, 2.0f * PI);

    const float SwingDegrees = bMoving
        ? FMath::Lerp(5.0f, 18.0f, LocomotionAlpha)
        : 2.0f;
    const float Swing = FMath::Sin(ModularLocomotionPhase) * SwingDegrees;
    const float CounterSwing = -Swing;
    const float Breathing = FMath::Sin(ModularLocomotionPhase * 0.5f) * (bMoving ? 0.7f : 1.6f);

    ModularLeftLeg->SetRelativeRotation(ModularLeftLegBaseRotation + FRotator(Swing, 0.0f, 0.0f));
    ModularRightLeg->SetRelativeRotation(ModularRightLegBaseRotation + FRotator(CounterSwing, 0.0f, 0.0f));
    ModularLeftArm->SetRelativeRotation(ModularLeftArmBaseRotation + FRotator(CounterSwing * 0.65f, 0.0f, 0.0f));
    ModularRightArm->SetRelativeRotation(ModularRightArmBaseRotation + FRotator(Swing * 0.65f, 0.0f, 0.0f));
    ModularTorso->SetRelativeLocation(ModularTorsoBaseLocation + FVector(0.0f, 0.0f, Breathing));
}

void ABMEnemyCharacter::InitializeDevelopmentLocomotion()
{
    if (!IsUsingAuthoredCharacterMesh()
        || !GetMesh()
        || AuthoredCharacterMesh.IsNull())
    {
        return;
    }

    // This fallback is deliberately restricted to assets owned by this
    // project or the installed development sample. A Blueprint that supplies
    // AnimationData continues to own its own animation mode.
    const FString MeshPath = AuthoredCharacterMesh.ToSoftObjectPath().ToString();
    const FString AnimBlueprintPath = AuthoredAnimBlueprint.ToSoftObjectPath().ToString();
    const bool bProjectOwnedMesh = MeshPath.StartsWith(TEXT("/Game/Characters/Mannequins/"));
    const bool bMoverExamplesMesh = MeshPath.StartsWith(TEXT("/MoverExamples/"));
    const bool bMoverExamplesAnimation = AnimBlueprintPath.StartsWith(TEXT("/MoverExamples/"));
    if (!bProjectOwnedMesh && (!bMoverExamplesMesh || !bMoverExamplesAnimation))
    {
        return;
    }

    const auto LoadAnimationDataSequence = [this](const FName SequenceName) -> UAnimSequence*
    {
        if (!AnimationData)
        {
            return nullptr;
        }
        const TSoftObjectPtr<UAnimSequence> SequenceReference =
            AnimationData->FindLocomotionSequence(SequenceName);
        return SequenceReference.IsNull() ? nullptr : SequenceReference.LoadSynchronous();
    };
    const auto LoadAnimationDataEvent = [this](const FName EventName) -> UAnimSequence*
    {
        if (!AnimationData)
        {
            return nullptr;
        }
        const TSoftObjectPtr<UAnimSequence> SequenceReference =
            AnimationData->FindEventSequence(EventName);
        return SequenceReference.IsNull() ? nullptr : SequenceReference.LoadSynchronous();
    };

    bool bLoadedSerializedLocomotion = false;
    if (AnimationData)
    {
        // The data asset is the authored source of truth. Keep the old
        // hard-coded paths below as a compatibility bridge for an editor that
        // has not run the content automation yet.
        DevelopmentRifleReadyAnimation = LoadAnimationDataSequence(TEXT("Idle"));
        DevelopmentRifleWalkAnimation = LoadAnimationDataSequence(TEXT("Walk"));
        DevelopmentRifleRunAnimation = LoadAnimationDataSequence(TEXT("Sprint"));
        DevelopmentRifleFallAnimation = LoadAnimationDataSequence(TEXT("Fall"));
        DevelopmentRifleFireAnimation = LoadAnimationDataEvent(TEXT("Fire"));
        DevelopmentRifleReloadAnimation = LoadAnimationDataEvent(TEXT("ReloadStarted"));
        DevelopmentRifleDryFireAnimation = LoadAnimationDataEvent(TEXT("DryFire"));
        DevelopmentRifleHitReactionAnimation = LoadAnimationDataEvent(TEXT("HitReact"));
        DevelopmentRifleHeadHitReactionAnimation = LoadAnimationDataEvent(TEXT("HeadHitReact"));
        DevelopmentRifleDeathAnimation = LoadAnimationDataEvent(TEXT("Death"));

        bLoadedSerializedLocomotion = DevelopmentRifleReadyAnimation
            && DevelopmentRifleWalkAnimation
            && DevelopmentRifleRunAnimation
            && DevelopmentRifleFallAnimation;
    }

    if (!bLoadedSerializedLocomotion && bProjectOwnedMesh)
    {
        // These sequences are part of the project Content tree and therefore
        // survive cooking/packaging. They also share the same mannequin
        // skeleton as SK_Mannequin, so the playable package gets actual
        // locomotion rather than a pose-only mesh.
        DevelopmentIdleAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS.MF_Rifle_Idle_ADS"));
        DevelopmentWalkAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd.MF_Rifle_Walk_Fwd"));
        DevelopmentRunAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Fwd.MF_Rifle_Jog_Fwd"));
        DevelopmentFallAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/Game/Characters/Mannequins/Anims/Rifle/Jump/MM_Rifle_Jump_Fall_Loop.MM_Rifle_Jump_Fall_Loop"));
    }
    else if (!bLoadedSerializedLocomotion)
    {
        DevelopmentIdleAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/MoverExamples/Characters/Mannequins/Animations/Manny/MM_Idle.MM_Idle"));
        DevelopmentWalkAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/MoverExamples/Characters/Mannequins/Animations/Manny/MM_Walk_InPlace.MM_Walk_InPlace"));
        DevelopmentRunAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/MoverExamples/Characters/Mannequins/Animations/Manny/MM_Run_Fwd.MM_Run_Fwd"));
        DevelopmentFallAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/MoverExamples/Characters/Mannequins/Animations/Manny/MM_Fall_Loop.MM_Fall_Loop"));
    }

    // MoverExamples ships the animated Manny body but not a weapon-ready
    // pose. The project carries the compatible rifle clips under /Game so
    // the compatibility path can keep both hands on the visible weapon.
    if (!DevelopmentRifleReadyAnimation)
    {
        DevelopmentRifleReadyAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS.MF_Rifle_Idle_ADS"));
    }
    if (!DevelopmentRifleWalkAnimation)
    {
        DevelopmentRifleWalkAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd.MF_Rifle_Walk_Fwd"));
    }
    if (!DevelopmentRifleRunAnimation)
    {
        DevelopmentRifleRunAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Fwd.MF_Rifle_Jog_Fwd"));
    }
    if (!DevelopmentRifleFallAnimation)
    {
        DevelopmentRifleFallAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/Game/Characters/Mannequins/Anims/Rifle/Jump/MM_Rifle_Jump_Fall_Loop.MM_Rifle_Jump_Fall_Loop"));
    }
    if (!DevelopmentRifleFireAnimation)
    {
        DevelopmentRifleFireAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Fire.MM_Rifle_Fire"));
    }
    if (!DevelopmentRifleReloadAnimation)
    {
        DevelopmentRifleReloadAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload.MM_Rifle_Reload"));
    }
    if (!DevelopmentRifleDryFireAnimation)
    {
        DevelopmentRifleDryFireAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_DryFire.MM_Rifle_DryFire"));
    }
    if (!DevelopmentRifleHitReactionAnimation)
    {
        DevelopmentRifleHitReactionAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01.MM_HitReact_Front_Lgt_01"));
    }
    if (!DevelopmentRifleHeadHitReactionAnimation)
    {
        DevelopmentRifleHeadHitReactionAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Hvy_01.MM_HitReact_Front_Hvy_01"));
    }
    if (!DevelopmentRifleDeathAnimation)
    {
        DevelopmentRifleDeathAnimation = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_01.MM_Death_Front_01"));
    }

    // The serialized data asset uses the rifle-ready clips for all four
    // locomotion states. Mirror them into the generic fields used by the
    // compatibility state machine so the same update path serves both data
    // driven and legacy source-only characters.
    DevelopmentIdleAnimation = DevelopmentRifleReadyAnimation;
    DevelopmentWalkAnimation = DevelopmentRifleWalkAnimation;
    DevelopmentRunAnimation = DevelopmentRifleRunAnimation;
    DevelopmentFallAnimation = DevelopmentRifleFallAnimation;

    bUsingDevelopmentLocomotionFallback = DevelopmentIdleAnimation
        && DevelopmentWalkAnimation
        && DevelopmentRunAnimation
        && DevelopmentFallAnimation;
    bUsingDevelopmentRifleLocomotion = DevelopmentRifleReadyAnimation
        && DevelopmentRifleWalkAnimation
        && DevelopmentRifleRunAnimation
        && DevelopmentRifleFallAnimation;
    if (!bUsingDevelopmentLocomotionFallback)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Black Meridian presentation: Manny locomotion fallback unavailable pawn=%s"),
            *GetNameSafe(this));
        return;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian presentation: project mannequin rifle locomotion %s pawn=%s mesh=%s source=%s ready=%s walk=%s run=%s fall=%s"),
        bUsingDevelopmentRifleLocomotion ? TEXT("active") : TEXT("unavailable"),
        *GetNameSafe(this),
        bProjectOwnedMesh ? TEXT("project") : TEXT("mover_examples"),
        bLoadedSerializedLocomotion ? TEXT("animation_data") : TEXT("compatibility_paths"),
        *GetNameSafe(DevelopmentRifleReadyAnimation),
        *GetNameSafe(DevelopmentRifleWalkAnimation),
        *GetNameSafe(DevelopmentRifleRunAnimation),
        *GetNameSafe(DevelopmentRifleFallAnimation));
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian presentation: project mannequin rifle events %s pawn=%s fire=%s additive=%s reload=%s additive=%s dry_fire=%s additive=%s hit=%s head_hit=%s death=%s"),
        DevelopmentRifleFireAnimation && DevelopmentRifleReloadAnimation
            && DevelopmentRifleHitReactionAnimation
            && DevelopmentRifleDeathAnimation
            ? TEXT("active")
            : TEXT("partial"),
        *GetNameSafe(this),
        *GetNameSafe(DevelopmentRifleFireAnimation),
        DevelopmentRifleFireAnimation && DevelopmentRifleFireAnimation->IsValidAdditive() ? TEXT("yes") : TEXT("no"),
        *GetNameSafe(DevelopmentRifleReloadAnimation),
        DevelopmentRifleReloadAnimation && DevelopmentRifleReloadAnimation->IsValidAdditive() ? TEXT("yes") : TEXT("no"),
        *GetNameSafe(DevelopmentRifleDryFireAnimation),
        DevelopmentRifleDryFireAnimation && DevelopmentRifleDryFireAnimation->IsValidAdditive() ? TEXT("yes") : TEXT("no"),
        *GetNameSafe(DevelopmentRifleHitReactionAnimation),
        *GetNameSafe(DevelopmentRifleHeadHitReactionAnimation),
        *GetNameSafe(DevelopmentRifleDeathAnimation));

    // AnimationData is the project-owned source of truth. The separate
    // AuthoredAnimBlueprint property remains a compatibility override for
    // Blueprint children, but the packaged enemy must not lose its
    // DefaultSlot just because that optional override is empty.
    if (AnimationData && AnimationData->AnimBlueprintClass.Get())
    {
        DevelopmentAnimBlueprintClass = AnimationData->AnimBlueprintClass.Get();
    }
    else if (!DevelopmentAnimBlueprintClass && !AuthoredAnimBlueprint.IsNull())
    {
        DevelopmentAnimBlueprintClass = AuthoredAnimBlueprint.LoadSynchronous();
    }
    if (!DevelopmentAnimBlueprintClass && bProjectOwnedMesh)
    {
        DevelopmentAnimBlueprintClass = LoadClass<UAnimInstance>(
            nullptr,
            TEXT("/Game/Characters/Mannequins/Animations/ABP_BM_Manny_Rifle.ABP_BM_Manny_Rifle_C"));
    }
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian presentation: project AnimBP route pawn=%s class=%s active=%s"),
        *GetNameSafe(this),
        *GetNameSafe(DevelopmentAnimBlueprintClass),
        DevelopmentAnimBlueprintClass ? TEXT("yes") : TEXT("no"));
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    GetMesh()->PlayAnimation(DevelopmentIdleAnimation, true);
    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        // CharacterMovement owns the capsule route. Do not let an imported
        // root-motion track add a second, animation-driven displacement.
        AnimInstance->SetRootMotionMode(ERootMotionMode::NoRootMotionExtraction);
    }
    GetMesh()->SetPlayRate(1.0f);
    ActiveDevelopmentAnimation = DevelopmentIdleAnimation;
    bDevelopmentMeshAligned = false;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian presentation: project mannequin locomotion active pawn=%s mesh=%s"),
        *GetNameSafe(this),
        bProjectOwnedMesh ? TEXT("project") : TEXT("mover_examples"));
}

void ABMEnemyCharacter::AlignDevelopmentMeshToCapsule()
{
    if (bDevelopmentMeshAligned
        || !bUsingDevelopmentLocomotionFallback
        || !GetMesh()
        || !GetCapsuleComponent())
    {
        return;
    }

    const int32 LeftFootBone = GetMesh()->GetBoneIndex(TEXT("foot_l"));
    const int32 RightFootBone = GetMesh()->GetBoneIndex(TEXT("foot_r"));
    if (LeftFootBone == INDEX_NONE || RightFootBone == INDEX_NONE)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Black Meridian presentation: Manny foot alignment unavailable pawn=%s left=%d right=%d"),
            *GetNameSafe(this),
            LeftFootBone,
            RightFootBone);
        bDevelopmentMeshAligned = true;
        return;
    }

    const float LeftFootZ = GetMesh()->GetBoneLocation(TEXT("foot_l"), EBoneSpaces::WorldSpace).Z;
    const float RightFootZ = GetMesh()->GetBoneLocation(TEXT("foot_r"), EBoneSpaces::WorldSpace).Z;
    const float LowestFootZ = FMath::Min(LeftFootZ, RightFootZ);
    const float CapsuleGroundZ = GetCapsuleComponent()->GetComponentLocation().Z
        - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    constexpr float FootClearance = 2.0f;
    const float AlignmentDeltaWorldZ = (CapsuleGroundZ + FootClearance) - LowestFootZ;
    const float ParentScaleZ = GetRootComponent()
        ? FMath::Max(KINDA_SMALL_NUMBER, GetRootComponent()->GetComponentScale().Z)
        : 1.0f;

    const FVector BeforeLocation = GetMesh()->GetRelativeLocation();
    FVector AlignedLocation = BeforeLocation;
    AlignedLocation.Z += AlignmentDeltaWorldZ / ParentScaleZ;
    GetMesh()->SetRelativeLocation(AlignedLocation);
    SetAuthoredMeshBaseLocation(AlignedLocation);
    bDevelopmentMeshAligned = true;

    const float AlignedLeftFootZ = GetMesh()->GetBoneLocation(TEXT("foot_l"), EBoneSpaces::WorldSpace).Z;
    const float AlignedRightFootZ = GetMesh()->GetBoneLocation(TEXT("foot_r"), EBoneSpaces::WorldSpace).Z;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian presentation: Manny mesh aligned pawn=%s capsule_ground_z=%.3f before_foot_z=(%.3f,%.3f) after_foot_z=(%.3f,%.3f) mesh_rel_z=%.3f delta=%.3f"),
        *GetNameSafe(this),
        CapsuleGroundZ,
        LeftFootZ,
        RightFootZ,
        AlignedLeftFootZ,
        AlignedRightFootZ,
        AlignedLocation.Z,
        AlignmentDeltaWorldZ);
}

void ABMEnemyCharacter::UpdateDevelopmentLocomotion(const float DeltaSeconds)
{
    if (!bUsingDevelopmentLocomotionFallback || !GetMesh())
    {
        return;
    }

    AlignDevelopmentMeshToCapsule();

    if (bEliminated)
    {
        bDevelopmentLocomotionMoving = false;
        if (DevelopmentEventRemaining > 0.0f)
        {
            DevelopmentEventRemaining = FMath::Max(
                0.0f,
                DevelopmentEventRemaining - FMath::Max(0.0f, DeltaSeconds));
            return;
        }

        // Keep the authored death pose on screen until the shared death
        // presentation timer removes the body. Stopping it on the first tick
        // after elimination was the reason the data-driven death clip never
        // became visible in a packaged probe.
        if (DevelopmentRifleDeathAnimation
            && ActiveDevelopmentEventAnimation == DevelopmentRifleDeathAnimation)
        {
            return;
        }

        ActiveDevelopmentEventAnimation = nullptr;
        RestoreDevelopmentSingleNodeLocomotion();
        if (ActiveDevelopmentAnimation)
        {
            GetMesh()->Stop();
            ActiveDevelopmentAnimation = nullptr;
        }
        return;
    }

    if (DevelopmentEventRemaining > 0.0f)
    {
        DevelopmentEventRemaining = FMath::Max(0.0f, DevelopmentEventRemaining - FMath::Max(0.0f, DeltaSeconds));
        if (DevelopmentEventRemaining > 0.0f)
        {
            return;
        }
        ActiveDevelopmentEventAnimation = nullptr;
    }

    if (bDevelopmentAnimationBlueprintActive)
    {
        RestoreDevelopmentSingleNodeLocomotion();
    }

    const UCharacterMovementComponent* Movement = GetCharacterMovement();
    const bool bFalling = Movement && Movement->IsFalling();
    const float GroundSpeed = GetVelocity().Size2D();
    // CharacterMovement speed changes in small steps while acceleration,
    // collision resolution, and a new tactical destination settle. Driving
    // the animation play rate directly from that raw value makes the rifle
    // walk cycle surge and brake every frame, which reads as foot jitter even
    // when the capsule path is smooth. Keep a presentation-only speed filter;
    // gameplay movement and the animation state thresholds still use the
    // authoritative velocity below.
    DevelopmentLocomotionSpeed = FMath::FInterpTo(
        DevelopmentLocomotionSpeed,
        GroundSpeed,
        FMath::Max(0.0f, DeltaSeconds),
        9.0f);
    // Keep the locomotion state stable while CharacterMovement brakes. A
    // short direct-input route can legitimately have one quiet frame while
    // the next waypoint is selected; switching to the ready clip in that
    // frame visibly snaps the feet even though the enemy is still advancing.
    const ABMEnemyAIController* AIController = Cast<ABMEnemyAIController>(GetController());
    const bool bRouteInputActive = AIController && AIController->IsFallbackMovementActive();
    if (GroundSpeed > 10.0f)
    {
        // A direct-input route can lose a frame while its tactical goal is
        // retargeted. Keep the walk state alive through that short brake
        // window so the clip does not restart at frame zero.
        DevelopmentLocomotionStopGraceRemaining = 0.30f;
    }
    else
    {
        DevelopmentLocomotionStopGraceRemaining = FMath::Max(
            0.0f,
            DevelopmentLocomotionStopGraceRemaining - FMath::Max(0.0f, DeltaSeconds));
    }
    const float MovingThreshold = bDevelopmentLocomotionMoving ? 10.0f : 28.0f;
    bDevelopmentLocomotionMoving = GroundSpeed > MovingThreshold
        || (bDevelopmentLocomotionMoving && DevelopmentLocomotionStopGraceRemaining > 0.0f)
        || (bRouteInputActive && GroundSpeed > 4.0f);

    UAnimSequence* DesiredAnimation = bUsingDevelopmentRifleLocomotion
        ? DevelopmentRifleReadyAnimation
        : DevelopmentIdleAnimation;
    float DesiredPlayRate = 1.0f;
    if (bFalling)
    {
        DesiredAnimation = bUsingDevelopmentRifleLocomotion
            ? DevelopmentRifleFallAnimation
            : DevelopmentFallAnimation;
    }
    else if (bDevelopmentLocomotionMoving)
    {
        const bool bRunning = GroundSpeed > 320.0f;
        if (bUsingDevelopmentRifleLocomotion)
        {
            DesiredAnimation = bRunning
                ? DevelopmentRifleRunAnimation
                : DevelopmentRifleWalkAnimation;
        }
        else
        {
            DesiredAnimation = bRunning ? DevelopmentRunAnimation : DevelopmentWalkAnimation;
        }
        DesiredPlayRate = bRunning
            ? FMath::Clamp(DevelopmentLocomotionSpeed / 430.0f, 0.72f, 1.25f)
            : FMath::Clamp(DevelopmentLocomotionSpeed / 220.0f, 0.72f, 1.35f);
    }

    if (DesiredAnimation != ActiveDevelopmentAnimation)
    {
        // Single-node PlayAnimation starts the new sequence at frame zero.
        // That reset was visible whenever CharacterMovement crossed the
        // walk/idle threshold or a short combat brake ended. Carry the phase
        // into the replacement locomotion clip so the feet continue through
        // the transition instead of snapping and restarting the stride.
        float PreviousPhase = 0.0f;
        if (ActiveDevelopmentAnimation)
        {
            const float PreviousLength = FMath::Max(
                KINDA_SMALL_NUMBER,
                ActiveDevelopmentAnimation->GetPlayLength());
            PreviousPhase = FMath::Fmod(
                FMath::Max(0.0f, GetMesh()->GetPosition()),
                PreviousLength) / PreviousLength;
        }

        GetMesh()->PlayAnimation(DesiredAnimation, true);
        ActiveDevelopmentAnimation = DesiredAnimation;
        if (DesiredAnimation)
        {
            const float NewLength = DesiredAnimation->GetPlayLength();
            if (NewLength > KINDA_SMALL_NUMBER)
            {
                GetMesh()->SetPosition(PreviousPhase * NewLength, false);
            }
        }
    }
    GetMesh()->SetPlayRate(DesiredPlayRate);
}

void ABMEnemyCharacter::PlayDevelopmentAnimationEvent(const FName AnimationEvent)
{
    if (!bUsingDevelopmentLocomotionFallback || !GetMesh())
    {
        return;
    }

    // The authoritative reload timer sends a completion/cancel event. Clear
    // the one-shot immediately so the rifle-ready locomotion clip resumes on
    // the same frame that the weapon becomes usable again.
    if (AnimationEvent == TEXT("ReloadCompleted") || AnimationEvent == TEXT("ReloadCancelled"))
    {
        RestoreDevelopmentSingleNodeLocomotion();
        DevelopmentEventRemaining = 0.0f;
        ActiveDevelopmentEventAnimation = nullptr;
        return;
    }

    UAnimSequence* EventAnimation = nullptr;
    float EventDuration = 0.0f;
    if (AnimationEvent == TEXT("Fire") || AnimationEvent == TEXT("FirePredicted"))
    {
        EventAnimation = DevelopmentRifleFireAnimation;
        // Keep automatic-fire enemies responsive while still playing the
        // complete authored clip at a slightly faster rate when needed.
        EventDuration = EventAnimation
            ? FMath::Clamp(EventAnimation->GetPlayLength(), 0.12f, 0.30f)
            : 0.0f;
    }
    else if (AnimationEvent == TEXT("Hit") || AnimationEvent == TEXT("HitReact"))
    {
        EventAnimation = DevelopmentRifleHitReactionAnimation;
        EventDuration = EventAnimation
            ? FMath::Clamp(EventAnimation->GetPlayLength(), 0.16f, 0.55f)
            : 0.0f;
    }
    else if (AnimationEvent == TEXT("HeadshotHit") || AnimationEvent == TEXT("HeadHitReact"))
    {
        EventAnimation = DevelopmentRifleHeadHitReactionAnimation
            ? DevelopmentRifleHeadHitReactionAnimation
            : DevelopmentRifleHitReactionAnimation;
        EventDuration = EventAnimation
            ? FMath::Clamp(EventAnimation->GetPlayLength(), 0.18f, 0.62f)
            : 0.0f;
    }
    else if (AnimationEvent == TEXT("ReloadStarted"))
    {
        EventAnimation = DevelopmentRifleReloadAnimation;
        const float ReloadDuration = CombatWeapon && CombatWeapon->GetWeaponData()
            ? (CombatWeapon->GetMagazineAmmo() <= 0
                ? CombatWeapon->GetWeaponData()->Balance.EmptyReloadDuration
                : CombatWeapon->GetWeaponData()->Balance.ReloadDuration)
            : 2.0f;
        EventDuration = EventAnimation ? FMath::Max(0.35f, ReloadDuration) : 0.0f;
    }
    else if (AnimationEvent == TEXT("DryFire"))
    {
        EventAnimation = DevelopmentRifleDryFireAnimation;
        EventDuration = EventAnimation
            ? FMath::Clamp(EventAnimation->GetPlayLength(), 0.16f, 0.32f)
            : 0.0f;
    }
    else if (AnimationEvent == TEXT("Death"))
    {
        EventAnimation = DevelopmentRifleDeathAnimation;
        EventDuration = EventAnimation
            ? FMath::Clamp(EventAnimation->GetPlayLength(), 0.65f, 2.0f)
            : 0.0f;
    }

    if (!EventAnimation || EventDuration <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    // The project rifle AnimBP supplies a stable idle weapon pose, not a
    // CharacterMovement-driven locomotion graph. A non-additive hit clip
    // would pause the walk clip for the reaction, then restart it when the
    // clip ends. That is the visible stride hitch this presentation path is
    // meant to avoid. Additive hit clips are different: they can layer over
    // the active rifle locomotion without taking ownership of the base pose,
    // so moving enemies should keep those reactions when the optional AnimBP
    // is available.
    const ABMEnemyAIController* AIController = Cast<ABMEnemyAIController>(GetController());
    const bool bContinuousLocomotionActive = GetVelocity().Size2D() > 20.0f
        || bDevelopmentLocomotionMoving
        || (AIController && AIController->IsFallbackMovementActive());
    const bool bMovementDisruptiveReaction = AnimationEvent == TEXT("Hit")
        || AnimationEvent == TEXT("HitReact")
        || AnimationEvent == TEXT("HeadshotHit")
        || AnimationEvent == TEXT("HeadHitReact");
    const bool bCanLayerMovingReaction = bMovementDisruptiveReaction
        && EventAnimation->IsValidAdditive()
        && DevelopmentAnimBlueprintClass != nullptr;
    const bool bLayeredOverMovingLocomotion = bContinuousLocomotionActive
        && bCanLayerMovingReaction;
    if (bContinuousLocomotionActive
        && bMovementDisruptiveReaction
        && !bCanLayerMovingReaction)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian presentation: hit animation kept on locomotion pawn=%s event=%s additive=%s anim_blueprint=%s reason=avoid_non_additive_moving_anim_swap"),
            *GetNameSafe(this),
            *AnimationEvent.ToString(),
            EventAnimation->IsValidAdditive() ? TEXT("yes") : TEXT("no"),
            DevelopmentAnimBlueprintClass ? TEXT("yes") : TEXT("no"));
        return;
    }

    const float SequenceLength = FMath::Max(KINDA_SMALL_NUMBER, EventAnimation->GetPlayLength());
    const float PlayRate = SequenceLength / EventDuration;
    const bool bAdditiveAnimation = EventAnimation->IsValidAdditive();
    if (bAdditiveAnimation)
    {
        // Fire is emitted at weapon cadence while an enemy is walking. The
        // project AnimBP's source pose is an idle rifle clip, so switching
        // from AnimationSingleNode locomotion to that blueprint for every
        // shot freezes the stride, then restarts it when the montage ends.
        // Keep the continuous rifle locomotion and let the weapon kick, muzzle
        // flash, audio, and hit reaction provide the short fire presentation.
        // This is intentionally limited to high-frequency fire events; the
        // lower-frequency hit/reload/death paths retain their authored clips.
        if (AnimationEvent == TEXT("Fire") || AnimationEvent == TEXT("FirePredicted"))
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian presentation: fire animation kept on locomotion pawn=%s asset=%s reason=avoid_idle_anim_blueprint_swap"),
                *GetNameSafe(this),
                *GetNameSafe(EventAnimation));
            return;
        }

        // AnimationSingleNode rejects additive clips in cooked builds. Manny's
        // ABP contains DefaultSlot, so temporarily restore it and play the
        // clip as a dynamic montage layered over the blueprint pose.
        static const FName AdditiveEventSlot(TEXT("DefaultSlot"));
        if (!DevelopmentAnimBlueprintClass)
        {
            // The project-owned rifle fire clip is additive. A packaged
            // source-only build intentionally does not depend on the optional
            // Manny AnimBP, so do not silently drop the authored fire motion
            // when that graph is unavailable. The dry-fire clip uses the same
            // rifle skeleton as a non-additive single-node sequence and is a
            // close, readable trigger/bolt gesture for the short fire window;
            // procedural weapon kick and muzzle effects still layer on top.
            if ((AnimationEvent == TEXT("Fire")
                    || AnimationEvent == TEXT("FirePredicted")
                    || AnimationEvent == TEXT("Hit")
                    || AnimationEvent == TEXT("HeadshotHit"))
                && DevelopmentRifleDryFireAnimation
                && !DevelopmentRifleDryFireAnimation->IsValidAdditive())
            {
                const float FallbackSequenceLength = FMath::Max(
                    KINDA_SMALL_NUMBER,
                    DevelopmentRifleDryFireAnimation->GetPlayLength());
                const float FallbackPlayRate = FallbackSequenceLength / EventDuration;
                RestoreDevelopmentSingleNodeLocomotion();
                ActiveDevelopmentEventAnimation = DevelopmentRifleDryFireAnimation;
                DevelopmentEventRemaining = EventDuration;
                ActiveDevelopmentAnimation = DevelopmentRifleDryFireAnimation;
                GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
                GetMesh()->PlayAnimation(DevelopmentRifleDryFireAnimation, false);
                GetMesh()->SetPlayRate(FallbackPlayRate);
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian presentation: authored combat animation event pawn=%s event=%s asset=%s duration=%.2f play_rate=%.2f mode=single_node fallback_from=%s reason=no_anim_blueprint"),
                    *GetNameSafe(this),
                    *AnimationEvent.ToString(),
                    *GetNameSafe(DevelopmentRifleDryFireAnimation),
                    EventDuration,
                    FallbackPlayRate,
                    *GetNameSafe(EventAnimation));
                return;
            }

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Black Meridian presentation: additive animation fallback pawn=%s event=%s asset=%s reason=no_anim_blueprint"),
                *GetNameSafe(this),
                *AnimationEvent.ToString(),
                *GetNameSafe(EventAnimation));
            return;
        }

        if (!bDevelopmentAnimationBlueprintActive)
        {
            // Preserve the current locomotion pose before temporarily handing
            // the mesh to the rifle AnimBP. Restoring a walk clip at frame 0
            // after every hit/fire montage is a visual foot hitch even when
            // the capsule path itself is smooth.
            DevelopmentLocomotionResumeAnimation = ActiveDevelopmentAnimation;
            DevelopmentLocomotionResumePosition = GetMesh()->GetPosition();
            GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
            GetMesh()->SetAnimInstanceClass(DevelopmentAnimBlueprintClass);
            // Mark the mode transition before asking for the instance so a
            // failed AnimBP initialization still takes the safe single-node
            // path below instead of leaving the mesh in an empty blueprint
            // mode.
            bDevelopmentAnimationBlueprintActive = true;
        }

        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->SetRootMotionMode(ERootMotionMode::NoRootMotionExtraction);
            if (bDevelopmentAnimationBlueprintActive)
            {
                AnimInstance->StopSlotAnimation(0.03f, AdditiveEventSlot);
            }

            if (UAnimMontage* DynamicMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
                    EventAnimation,
                    AdditiveEventSlot,
                    0.04f,
                    0.08f,
                    PlayRate,
                    1,
                    -1.0f,
                    0.0f))
            {
                ActiveDevelopmentEventAnimation = EventAnimation;
                DevelopmentEventRemaining = EventDuration;
                ActiveDevelopmentAnimation = EventAnimation;

                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian presentation: authored combat animation event pawn=%s event=%s asset=%s duration=%.2f play_rate=%.2f mode=montage additive=yes slot=%s montage=%s"),
                    *GetNameSafe(this),
                    *AnimationEvent.ToString(),
                    *GetNameSafe(EventAnimation),
                    EventDuration,
                    PlayRate,
                    *AdditiveEventSlot.ToString(),
                    *GetNameSafe(DynamicMontage));
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian presentation: additive reaction layered pawn=%s event=%s locomotion_layered=%s"),
                    *GetNameSafe(this),
                    *AnimationEvent.ToString(),
                    bLayeredOverMovingLocomotion ? TEXT("yes") : TEXT("no"));
                return;
            }
        }

        // Do not call PlayAnimation on an additive asset. Fire still has the
        // procedural weapon kick, and other additive events safely leave the
        // current locomotion pose intact if the slot is unavailable.
        RestoreDevelopmentSingleNodeLocomotion();
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Black Meridian presentation: additive animation fallback pawn=%s event=%s asset=%s reason=montage_unavailable mode=procedural_fallback"),
            *GetNameSafe(this),
            *AnimationEvent.ToString(),
            *GetNameSafe(EventAnimation));
        return;
    }

    RestoreDevelopmentSingleNodeLocomotion();
    ActiveDevelopmentEventAnimation = EventAnimation;
    DevelopmentEventRemaining = EventDuration;
    ActiveDevelopmentAnimation = EventAnimation;
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    GetMesh()->PlayAnimation(EventAnimation, false);
    GetMesh()->SetPlayRate(PlayRate);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian presentation: authored combat animation event pawn=%s event=%s asset=%s duration=%.2f play_rate=%.2f mode=single_node additive=no"),
        *GetNameSafe(this),
        *AnimationEvent.ToString(),
        *GetNameSafe(EventAnimation),
        EventDuration,
        PlayRate);
}

void ABMEnemyCharacter::RestoreDevelopmentSingleNodeLocomotion()
{
    if (!bDevelopmentAnimationBlueprintActive || !GetMesh())
    {
        return;
    }

    static const FName AdditiveEventSlot(TEXT("DefaultSlot"));
    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        AnimInstance->StopSlotAnimation(0.06f, AdditiveEventSlot);
    }

    UAnimSequence* ResumeAnimation = DevelopmentLocomotionResumeAnimation;
    const float ResumePosition = DevelopmentLocomotionResumePosition;
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    bDevelopmentAnimationBlueprintActive = false;
    GetMesh()->SetPlayRate(1.0f);
    if (ResumeAnimation)
    {
        GetMesh()->PlayAnimation(ResumeAnimation, true);
        const float ResumeLength = FMath::Max(KINDA_SMALL_NUMBER, ResumeAnimation->GetPlayLength());
        GetMesh()->SetPosition(FMath::Fmod(FMath::Max(0.0f, ResumePosition), ResumeLength), false);
        if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
        {
            AnimInstance->SetRootMotionMode(ERootMotionMode::NoRootMotionExtraction);
        }
        ActiveDevelopmentAnimation = ResumeAnimation;
    }
    else
    {
        ActiveDevelopmentAnimation = nullptr;
    }
    DevelopmentLocomotionResumeAnimation = nullptr;
    DevelopmentLocomotionResumePosition = 0.0f;
}

void ABMEnemyCharacter::TryFireAt(AActor* Target)
{
#if !UE_BUILD_SHIPPING
    // Keep the AI target, movement, animation, and facing paths alive for a
    // clean rendered movement check. Hostile fire is suppressed only for an
    // explicitly passive capture; the showcase label itself remains a real
    // playable combat encounter.
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_PassiveCombatCapture")))
    {
        return;
    }

    // The reload-presentation capture intentionally keeps the AI movement,
    // weapon mounts, and player-side firing path alive while removing only
    // hostile damage from this diagnostic encounter. Without this guard the
    // showcase player can be eliminated before the eight-shot reload trigger
    // is reached, making the rendered reload check a test of spawn timing
    // rather than a test of reload presentation.
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_ReloadPresentationShowcase")))
    {
        return;
    }

    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe")) && Target && !bEliminated)
    {
        const ABMCombatCharacter* ProbeTarget = Cast<ABMCombatCharacter>(Target);
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian role fire attempt: pawn=%s target=%s authority=%s enemy_team=%d target_team=%d fire_cooldown=%.2f start_delay=%.2f target_eliminated=%s"),
            *GetNameSafe(this),
            *GetNameSafe(Target),
            HasAuthority() ? TEXT("yes") : TEXT("no"),
            static_cast<int32>(Team),
            ProbeTarget ? static_cast<int32>(ProbeTarget->GetTeam()) : -1,
            FireCooldown,
            CombatStartDelayRemaining,
            ProbeTarget && ProbeTarget->IsEliminated() ? TEXT("yes") : TEXT("no"));
    }
#endif
    if (!HasAuthority() || !Target || bEliminated || FireCooldown > 0.0f || CombatStartDelayRemaining > 0.0f)
    {
        return;
    }

    ABMCombatCharacter* TargetCharacter = Cast<ABMCombatCharacter>(Target);
    if (!TargetCharacter || TargetCharacter->GetTeam() == Team || TargetCharacter->IsEliminated())
    {
        BurstShotsRemaining = 0;
        BurstTarget.Reset();
        return;
    }

    if (BurstTarget.Get() != Target)
    {
        BurstTarget = Target;
        BurstShotsRemaining = 0;
    }

    EnsureCombatWeapon();
    if (!CombatWeapon)
    {
        return;
    }

    if (CombatWeapon->IsReloading())
    {
        BurstShotsRemaining = 0;
        return;
    }

    if (CombatWeapon->GetMagazineAmmo() <= 0)
    {
        CombatWeapon->Reload();
        BurstShotsRemaining = 0;
        FireCooldown = CombatWeapon->GetWeaponData()
            ? FMath::Max(
                0.45f,
                CombatWeapon->GetMagazineAmmo() <= 0
                    ? CombatWeapon->GetWeaponData()->Balance.EmptyReloadDuration
                    : CombatWeapon->GetWeaponData()->Balance.ReloadDuration)
            : 2.0f;
        return;
    }

    const FVector TargetLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
    const FVector AimDirection = (TargetLocation - CombatWeapon->GetMuzzleLocation()).GetSafeNormal();
    if (AimDirection.IsNearlyZero())
    {
        return;
    }

    // Keep the visible body and rifle on the same full 3D target line as the
    // shot. The old path only applied a smoothed yaw, so a bot could damage
    // the player while the rendered muzzle was visibly off target on ramps or
    // elevated cover.
    CombatAimYaw = AimDirection.Rotation().Yaw;
    CombatAimPitch = AimDirection.Rotation().Pitch;
    SmoothedCombatAimYaw = CombatAimYaw;
    SmoothedCombatAimPitch = CombatAimPitch;
    ApplyCombatWeaponAim();
    const FVector FireOrigin = CombatWeapon->GetMuzzleLocation();
    FVector FireDirection = (TargetLocation - FireOrigin).GetSafeNormal();
    if (FireDirection.IsNearlyZero())
    {
        return;
    }

    // Do not fire through cover or at a stale target that is no longer on the
    // muzzle line. The weapon trace remains authoritative, but this preflight
    // keeps the AI's visible aim and its decision to shoot consistent.
    FHitResult LineOfSightHit;
    FCollisionQueryParams LineOfSightParams(SCENE_QUERY_STAT(BlackMeridianEnemyAim), true, this);
    // Enemy fire must respect the same rendered cover geometry as the
    // player's sightline. Runtime pawn proxies are deliberately ignored by
    // ECC_Visibility because their analytic boxes can fill visible gaps.
    LineOfSightParams.bTraceComplex = true;
    LineOfSightParams.AddIgnoredActor(CombatWeapon);
    if (!GetWorld()->LineTraceSingleByChannel(
            LineOfSightHit,
            FireOrigin,
            TargetLocation,
            ECC_Visibility,
            LineOfSightParams)
        || LineOfSightHit.GetActor() != Target)
    {
        return;
    }

    // AI controllers make decisions independently. Claim the shared hostile
    // fire slot so the player receives a readable stream of incoming fire
    // rather than a frame-perfect five-gun burst when several enemies acquire the
    // same target together.
    if (UBMSquadSubsystem* Squad = GetWorld()->GetSubsystem<UBMSquadSubsystem>();
        Squad && !Squad->TryClaimFireSlot(Target, BMCombatTuning::SquadFireSlotIntervalSeconds))
    {
        return;
    }

    // AI should acquire and track a target reliably, but it should not land a
    // perfect center-mass hit on every tick. The error is deliberately
    // applied before ABMWeapon's data-driven spread so attachments, stance,
    // suppression, and weapon balance still contribute to the final shot.
    const float ClampedAimError = FMath::Clamp(AimErrorDegrees, 0.0f, 12.0f);
    if (ClampedAimError > KINDA_SMALL_NUMBER)
    {
        FireDirection = FMath::VRandCone(
            FireDirection,
            FMath::DegreesToRadians(ClampedAimError));
    }
    const float TargetDistance = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());
    CombatWeapon->Fire(FireOrigin, FireDirection, ShouldAimAtDistance(TargetDistance));

    // Keep the weapon cadence readable without making every enemy a perfect
    // automatic turret. Automatic roles use short bursts; breachers and
    // marksmen stay deliberate. Burst state is local to this target, so a
    // target hand-off cannot inherit a half-finished volley.
    const int32 BurstSize = EnemyRole == EBMEnemyRole::Assault
        ? 3
        : (EnemyRole == EBMEnemyRole::Rifleman ? 3 : 1);
    const float RoleCooldown = EnemyRole == EBMEnemyRole::Marksman
        ? 0.68f
        : (EnemyRole == EBMEnemyRole::Assault
            ? 0.24f
            : (EnemyRole == EBMEnemyRole::Demolitionist
                ? 1.15f
                : (EnemyRole == EBMEnemyRole::Breacher ? 0.52f : 0.36f)));
    const float AggressionScale = FMath::Max(0.75f, 0.82f + 0.18f * FMath::Clamp(Aggression, 0.0f, 1.5f));
    const float WeaponFireInterval = CombatWeapon->GetWeaponData()
        ? 60.0f / FMath::Max(1.0f, CombatWeapon->GetWeaponData()->Balance.FireRateRPM)
        : 0.2f;
    if (BurstShotsRemaining <= 0)
    {
        BurstShotsRemaining = BurstSize;
    }
    --BurstShotsRemaining;

    if (BurstShotsRemaining > 0)
    {
        const float BurstSpacing = EnemyRole == EBMEnemyRole::Assault ? 0.20f : 0.24f;
        FireCooldown = FMath::Max(WeaponFireInterval + 0.02f, BurstSpacing);
    }
    else
    {
        FireCooldown = FMath::Max(
            WeaponFireInterval + 0.03f,
            RoleCooldown / AggressionScale);
    }
    SetAIState(EBMAIState::Engage);

#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe")))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian role fire: pawn=%s role=%s distance=%.0f aiming=%s burst_remaining=%d cooldown=%.2f"),
            *GetNameSafe(this),
            GetRoleLabel(EnemyRole),
            TargetDistance,
            ShouldAimAtDistance(TargetDistance) ? TEXT("yes") : TEXT("no"),
            BurstShotsRemaining,
            FireCooldown);
    }
#endif
}

void ABMEnemyCharacter::SetCombatStartDelay(const float DelaySeconds)
{
    CombatStartDelayRemaining = FMath::Max(CombatStartDelayRemaining, DelaySeconds);
}

EBMWeaponId ABMEnemyCharacter::GetWeaponIdForRole() const
{
    switch (EnemyRole)
    {
    case EBMEnemyRole::Assault:
        return EBMWeaponId::Kestrel9;
    case EBMEnemyRole::Marksman:
        return EBMWeaponId::RookDMR;
    case EBMEnemyRole::Breacher:
        return EBMWeaponId::Breach12;
    case EBMEnemyRole::Demolitionist:
        return EBMWeaponId::ArcLauncher;
    case EBMEnemyRole::Rifleman:
    default:
        return EBMWeaponId::ARX4;
    }
}

void ABMEnemyCharacter::SetEnemyRole(const EBMEnemyRole NewRole)
{
    if (!HasAuthority())
    {
        return;
    }

    EnemyRole = NewRole;
    GrenadesRemaining = EnemyRole == EBMEnemyRole::Marksman
        ? 0
        : (EnemyRole == EBMEnemyRole::Assault || EnemyRole == EBMEnemyRole::Breacher ? 2 : 1);
    ApplyRolePresentation();
    EnsureCombatWeapon();
    ForceNetUpdate();
}

void ABMEnemyCharacter::OnRep_EnemyRole()
{
    ApplyRolePresentation();
}

FString ABMEnemyCharacter::GetEnemyRoleLabel() const
{
    return FString(GetRoleLabel(EnemyRole));
}

FString ABMEnemyCharacter::GetEnemyRoleMarker() const
{
    return FString(GetRoleMarker(EnemyRole));
}

FLinearColor ABMEnemyCharacter::GetEnemyRoleColor() const
{
    return GetRoleLinearColor(EnemyRole);
}

float ABMEnemyCharacter::GetPreferredEngagementRange() const
{
    switch (EnemyRole)
    {
    case EBMEnemyRole::Assault:
        return 420.0f;
    case EBMEnemyRole::Marksman:
        return 1000.0f;
    case EBMEnemyRole::Breacher:
        return 320.0f;
    case EBMEnemyRole::Demolitionist:
        return 820.0f;
    case EBMEnemyRole::Rifleman:
    default:
        return 650.0f;
    }
}

float ABMEnemyCharacter::GetEngagementRange() const
{
    switch (EnemyRole)
    {
    case EBMEnemyRole::Assault:
        return 650.0f;
    case EBMEnemyRole::Marksman:
        return 1350.0f;
    case EBMEnemyRole::Breacher:
        return 560.0f;
    case EBMEnemyRole::Demolitionist:
        return 1180.0f;
    case EBMEnemyRole::Rifleman:
    default:
        return 900.0f;
    }
}

float ABMEnemyCharacter::GetRoleMoveSpeed() const
{
    switch (EnemyRole)
    {
    case EBMEnemyRole::Assault:
        return BMCombatTuning::EnemyAssaultMoveSpeed;
    case EBMEnemyRole::Marksman:
        return BMCombatTuning::EnemyMarksmanMoveSpeed;
    case EBMEnemyRole::Breacher:
        return BMCombatTuning::EnemyBreacherMoveSpeed;
    case EBMEnemyRole::Demolitionist:
        return BMCombatTuning::EnemyDemolitionistMoveSpeed;
    case EBMEnemyRole::Rifleman:
    default:
        return BMCombatTuning::EnemyRiflemanMoveSpeed;
    }
}

bool ABMEnemyCharacter::CanThrowGrenades() const
{
    return EnemyRole != EBMEnemyRole::Marksman && GrenadesRemaining > 0;
}

bool ABMEnemyCharacter::ShouldThrowGrenadeAtDistance(const float Distance) const
{
    if (!CanThrowGrenades())
    {
        return false;
    }

    switch (EnemyRole)
    {
    case EBMEnemyRole::Assault:
        return Distance >= 460.0f && Distance <= 900.0f;
    case EBMEnemyRole::Breacher:
        return Distance >= 380.0f && Distance <= 760.0f;
    case EBMEnemyRole::Demolitionist:
        return Distance >= 560.0f && Distance <= 980.0f;
    case EBMEnemyRole::Rifleman:
    default:
        return Distance >= 680.0f && Distance <= 1100.0f;
    }
}

bool ABMEnemyCharacter::ShouldAimAtDistance(const float Distance) const
{
    switch (EnemyRole)
    {
    case EBMEnemyRole::Marksman:
    case EBMEnemyRole::Demolitionist:
        return true;
    case EBMEnemyRole::Breacher:
        return Distance >= 420.0f;
    case EBMEnemyRole::Rifleman:
        return Distance >= 500.0f;
    case EBMEnemyRole::Assault:
    default:
        return Distance >= 700.0f;
    }
}

void ABMEnemyCharacter::ApplyRolePresentation()
{
    const FColor DisplayColor = GetRoleDisplayColor(EnemyRole);
    ConfigurePlaceholderBody(DisplayColor);

    if (IsUsingAuthoredCharacterMesh() && GetMesh())
    {
        // Keep the real mannequin readable as a role-coded opposing force.
        // The material parameter names are shared by the development sample
        // and by the final soldier material contract.
        const FLinearColor RoleColor = GetRoleLinearColor(EnemyRole);
        const FVector RoleColorVector(RoleColor.R, RoleColor.G, RoleColor.B);
        GetMesh()->SetVectorParameterValueOnMaterials(TEXT("BaseColor"), RoleColorVector);
        GetMesh()->SetVectorParameterValueOnMaterials(TEXT("Color"), RoleColorVector);
        GetMesh()->SetVectorParameterValueOnMaterials(
            TEXT("EmissiveColor"),
            FVector(RoleColor.R * 0.05f, RoleColor.G * 0.05f, RoleColor.B * 0.05f));
    }

    ConfigureModularMaterials();
    ConfigureRoleAccessories();

    if (GetCharacterMovement())
    {
        // Re-assert the movement-facing contract here as well as in the
        // constructor so a Blueprint child cannot silently restore controller
        // yaw or automatic movement rotation after native defaults are applied.
        GetCharacterMovement()->PrimaryComponentTick.TickGroup = TG_PrePhysics;
        GetCharacterMovement()->bOrientRotationToMovement = false;
        GetCharacterMovement()->bUseControllerDesiredRotation = false;
        GetCharacterMovement()->RotationRate = FRotator(
            0.0f,
            BMCombatTuning::EnemyMovementRotationRateDegreesPerSecond,
            0.0f);
        // Use the same far-approach cap as the AI controller until its first
        // decision tick. This keeps navigation-enabled maps from briefly
        // using the faster raw role speed during spawn/target acquisition.
        GetCharacterMovement()->MaxWalkSpeed = BMCombatTuning::GetEnemyApproachSpeed(
            GetRoleMoveSpeed(),
            1.0f);
    }
}

void ABMEnemyCharacter::EnsureCombatWeapon()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    const EBMWeaponId DesiredWeaponId = GetWeaponIdForRole();
    if (!CombatWeapon)
    {
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Owner = this;
        SpawnParameters.Instigator = this;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        CombatWeapon = GetWorld()->SpawnActor<ABMWeapon>(ABMWeapon::StaticClass(), GetActorTransform(), SpawnParameters);
        if (!CombatWeapon)
        {
            return;
        }

        CombatWeapon->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        CombatWeapon->SetActorRelativeLocation(FVector(12.0f, 0.0f, 86.0f));
        CombatWeapon->SetActorRelativeRotation(FRotator::ZeroRotator);
        if (IsUsingAuthoredCharacterMesh() && GetMesh() && GetMesh()->GetBoneIndex(TEXT("hand_r")) != INDEX_NONE)
        {
            CombatWeapon->AttachToComponent(
                GetMesh(),
                FAttachmentTransformRules::SnapToTargetNotIncludingScale,
                TEXT("hand_r"));
            // The third-person weapon profile is authored around the top of
            // its pistol grip. Snap that origin directly to hand_r; the
            // character tick applies the enemy-facing aim rotation while the
            // hand socket continues to provide the palm position.
            CombatWeapon->SetActorRelativeLocation(FVector::ZeroVector);
            CombatWeapon->SetActorRelativeRotation(FRotator::ZeroRotator);
            // Scale the whole hand-mounted actor so the authored weapon and
            // its support silhouette stay aligned with the same Grip/Muzzle
            // socket contract while remaining readable at combat distance.
            CombatWeapon->SetActorRelativeScale3D(FVector(1.20f));
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian presentation: enemy weapon attached pawn=%s socket=hand_r"),
                *GetNameSafe(this));
        }
        CombatWeapon->SetActorHiddenInGame(false);
        CombatWeapon->SetActorEnableCollision(false);
        CombatWeapon->SetThirdPersonPresentation(true);
    }

    CombatWeapon->OnWeaponEvent.AddUniqueDynamic(this, &ABMEnemyCharacter::HandleWeaponEvent);

    if (CombatWeapon->GetWeaponId() != DesiredWeaponId)
    {
        CombatWeapon->InitializeFromData(UBMWeaponDataAsset::LoadContentProfile(CombatWeapon, DesiredWeaponId));
    }

    // The socket supplies the initial parent transform, but Manny's hand_r
    // socket is authored with a palm-oriented basis rather than a firearm
    // forward basis. Resolve the weapon's independent aim and grip position
    // immediately after the mesh/profile is assigned so the first rendered
    // frame cannot show a vertical or floating muzzle before Tick() applies
    // the same correction.
    if (IsUsingAuthoredCharacterMesh() && GetMesh())
    {
        ApplyCombatWeaponAim();
    }

    if (!bCombatWeaponPresentationInitialized)
    {
        CombatWeaponBaseLocation = CombatWeapon->GetRootComponent()
            ? CombatWeapon->GetRootComponent()->GetRelativeLocation()
            : FVector::ZeroVector;
        CombatWeaponBaseRotation = CombatWeapon->GetRootComponent()
            ? CombatWeapon->GetRootComponent()->GetRelativeRotation()
            : FRotator::ZeroRotator;
        bCombatWeaponPresentationInitialized = true;
    }
    if (IsUsingAuthoredCharacterMesh() && GetMesh())
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian presentation: weapon mount pawn=%s hand=%s grip=%s muzzle=%s"),
            *GetNameSafe(this),
            *GetMesh()->GetSocketLocation(TEXT("hand_r")).ToString(),
            *CombatWeapon->GetGripLocation().ToString(),
            *CombatWeapon->GetMuzzleLocation().ToString());
    }
    CombatWeapon->ForceNetUpdate();
}

void ABMEnemyCharacter::ApplyCombatWeaponAim()
{
    if (!CombatWeapon)
    {
        return;
    }

    FVector AimDirection = FRotator(0.0f, SmoothedCombatAimYaw, 0.0f).Vector();
    if (bHasCombatFacingTarget || bReplicatedCombatFacing)
    {
        AimDirection = FRotator(SmoothedCombatAimPitch, SmoothedCombatAimYaw, 0.0f).Vector();
    }

    FRotator AimRotation = AimDirection.Rotation();
    AimRotation.Pitch -= CombatWeaponKick * 9.0f;
    AimRotation.Roll += FMath::Sin(PI * FMath::Clamp(CombatWeaponReloadAlpha, 0.0f, 1.0f)) * 4.0f;
    CombatWeapon->SetActorRotation(AimRotation);

    if (IsUsingAuthoredCharacterMesh() && GetMesh())
    {
        // SetActorRotation changes the world-space direction of the grip's
        // local offset. Fit the rigid rifle between both animated hand
        // sockets: keep the right-hand grip close to hand_r, then move only
        // half of the support-hand's perpendicular error into the weapon.
        // This preserves a believable two-handed hold while avoiding the old
        // 30 cm readability offset that made the rifle float beside the arms.
        // A small balanced offset also keeps enough of the receiver visible
        // when a bot aims directly toward the camera.
        const FVector GripOffset = CombatWeapon->GetGripLocation()
            - CombatWeapon->GetActorLocation();
        const FVector WeaponDirection = (
            CombatWeapon->GetMuzzleLocation()
            - CombatWeapon->GetGripLocation()).GetSafeNormal();
        FVector TwoHandPresentationOffset = FVector::ZeroVector;
        if (!WeaponDirection.IsNearlyZero()
            && GetMesh()->DoesSocketExist(TEXT("hand_l")))
        {
            const FVector RightHandLocation = GetMesh()->GetSocketLocation(TEXT("hand_r"));
            const FVector LeftHandLocation = GetMesh()->GetSocketLocation(TEXT("hand_l"));
            const FVector SupportHandError = FVector::VectorPlaneProject(
                LeftHandLocation - RightHandLocation,
                WeaponDirection);
            TwoHandPresentationOffset = SupportHandError * 0.5f;
            const float MaxOffset = 14.0f;
            if (TwoHandPresentationOffset.Size() > MaxOffset)
            {
                TwoHandPresentationOffset = TwoHandPresentationOffset.GetSafeNormal() * MaxOffset;
            }
        }
        CombatWeapon->SetActorLocation(
            GetMesh()->GetSocketLocation(TEXT("hand_r"))
            - GripOffset
            + TwoHandPresentationOffset);
    }
}

void ABMEnemyCharacter::OnRep_CombatWeapon()
{
    if (CombatWeapon)
    {
        CombatWeapon->SetThirdPersonPresentation(true);
        CombatWeapon->OnWeaponEvent.AddUniqueDynamic(this, &ABMEnemyCharacter::HandleWeaponEvent);
        CombatWeaponBaseLocation = CombatWeapon->GetRootComponent()
            ? CombatWeapon->GetRootComponent()->GetRelativeLocation()
            : FVector::ZeroVector;
        CombatWeaponBaseRotation = CombatWeapon->GetRootComponent()
            ? CombatWeapon->GetRootComponent()->GetRelativeRotation()
            : FRotator::ZeroRotator;
        bCombatWeaponPresentationInitialized = true;
    }
}

void ABMEnemyCharacter::OnRep_CombatAimYaw()
{
    // The first replicated value establishes the local presentation without
    // a startup turn. Subsequent values are eased in Tick(), so this callback
    // intentionally does not write a component transform itself.
    if (!bCombatAimYawInitialized)
    {
        SmoothedCombatAimYaw = CombatAimYaw;
        bCombatAimYawInitialized = true;
    }
}

void ABMEnemyCharacter::HandleWeaponEvent(const FName EventName)
{
    // AI weapons do not have a client-predicted path, so every event can be
    // forwarded to an authored montage when the character data asset supplies
    // one. The event remains useful to Blueprint presentations even when the
    // development Manny AnimBP has no weapon montage assigned.
    if (EventName == TEXT("Fire")
        || EventName == TEXT("FirePredicted")
        || EventName == TEXT("Hit")
        || EventName == TEXT("HeadshotHit"))
    {
        CombatWeaponKick = FMath::Max(CombatWeaponKick, 1.0f);
    }
    PlayCharacterAnimation_Implementation(EventName);
}

void ABMEnemyCharacter::PlayCharacterAnimation_Implementation(const FName AnimationEvent)
{
    Super::PlayCharacterAnimation_Implementation(AnimationEvent);
    PlayDevelopmentAnimationEvent(AnimationEvent);
}

void ABMEnemyCharacter::SetCombatFacingTarget(AActor* Target)
{
    if (!IsValid(Target) || Target == this)
    {
        ClearCombatFacingTarget();
        return;
    }

    const bool bWasAlreadyFacingTarget = bHasCombatFacingTarget
        && CombatFacingTarget.Get() == Target
        && bReplicatedCombatFacing;
    CombatFacingTarget = Target;
    CombatFacingLocation = Target->GetActorLocation();
    bHasCombatFacingTarget = true;
    bReplicatedCombatFacing = true;
    if (!bWasAlreadyFacingTarget)
    {
        ForceNetUpdate();
    }
}

void ABMEnemyCharacter::ClearCombatFacingTarget()
{
    const bool bWasFacingTarget = bHasCombatFacingTarget || bReplicatedCombatFacing;
    CombatFacingTarget.Reset();
    CombatFacingLocation = FVector::ZeroVector;
    bHasCombatFacingTarget = false;
    bReplicatedCombatFacing = false;
    if (bWasFacingTarget)
    {
        ForceNetUpdate();
    }
}

void ABMEnemyCharacter::SetAIState(const EBMAIState NewState)
{
    AIState = NewState;
}

void ABMEnemyCharacter::TryThrowGrenade(AActor* Target)
{
#if !UE_BUILD_SHIPPING
    // The passive combat capture is for readable movement/facing review;
    // grenade flashes and smoke would obscure the body being inspected.
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_PassiveCombatCapture")))
    {
        return;
    }
#endif

    const float Distance = Target ? FVector::Dist2D(GetActorLocation(), Target->GetActorLocation()) : 0.0f;
    const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
    const ABMCombatCharacter* TargetCharacter = Cast<ABMCombatCharacter>(Target);
    if (!HasAuthority()
        || !Target
        || !TargetCharacter
        || TargetCharacter->GetTeam() == Team
        || TargetCharacter->IsEliminated()
        || !ABMGameState::IsCombatOrdnanceEnabled(State)
        || CombatStartDelayRemaining > 0.0f
        || GrenadeCooldown > 0.0f
        || !ShouldThrowGrenadeAtDistance(Distance))
    {
        return;
    }

    const FVector Origin = GetActorLocation() + FVector(0.0f, 0.0f, 100.0f);
    const FVector Direction = (Target->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f) - Origin).GetSafeNormal();
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.Instigator = this;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    if (ABMThrowable* Grenade = GetWorld()->SpawnActor<ABMThrowable>(ABMThrowable::StaticClass(), Origin, Direction.Rotation(), SpawnParameters))
    {
        Grenade->ThrowableId = EBMWeaponId::FragGrenade;
        Grenade->ArmAndThrow(Direction, EnemyRole == EBMEnemyRole::Assault ? 760.0f : 680.0f);
        --GrenadesRemaining;
        GrenadeCooldown = EnemyRole == EBMEnemyRole::Assault ? 9.0f : 13.0f;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian throwable: enemy spawned role=%s pawn=%s target=%s remaining=%d origin=%s direction=%s"),
            GetRoleLabel(EnemyRole),
            *GetNameSafe(this),
            *GetNameSafe(Target),
            GrenadesRemaining,
            *Origin.ToString(),
            *Direction.ToString());
    }
}

void ABMEnemyCharacter::HandleDeath(const FBMDamageContext& DamageContext)
{
    if (CombatWeapon)
    {
        CombatWeapon->ClearTransientPresentation();
        // The weapon is an independent replicated actor attached to the hand;
        // hiding only the skeletal mesh leaves a floating blocky rifle behind
        // during or after the death pose.
        CombatWeapon->SetActorHiddenInGame(true);
    }
    Super::HandleDeath(DamageContext);

    // A cooked client can legitimately miss the optional Manny asset and use
    // the project-owned modular fallback for the live enemy silhouette. That
    // fallback is intentionally assembled from box-shaped parts; showing it
    // during the fall makes a dead enemy turn into a set of moving boxes. A
    // fallback enemy should disappear cleanly after the lethal hit rather
    // than expose its construction primitives as a corpse.
    if (!IsUsingAuthoredCharacterMesh())
    {
        if (ModularRigRoot)
        {
            ModularRigRoot->SetVisibility(false, true);
        }
        if (PlaceholderBody)
        {
            PlaceholderBody->SetVisibility(false, true);
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian presentation: fallback death geometry hidden pawn=%s reason=avoid_moving_boxes"),
            *GetNameSafe(this));
    }
    if (HasAuthority())
    {
        if (UBMSquadSubsystem* Squad = GetWorld()->GetSubsystem<UBMSquadSubsystem>())
        {
            Squad->ReportEnemyDeath(this, GetActorLocation());
            Squad->UnregisterEnemy(this);
        }
        if (ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>())
        {
            GameMode->HandleEnemyEliminated(this, DamageContext);
        }
        SetLifeSpan(2.0f);
    }
}

void ABMEnemyCharacter::FinishDeathPresentation()
{
    Super::FinishDeathPresentation();

    // The procedural fallback is made from collision-sized boxes. It is a
    // useful live silhouette, but it must not remain visible after the death
    // pose or it reads as moving debug boxes instead of a finished corpse.
    if (ModularRigRoot)
    {
        ModularRigRoot->SetVisibility(false, true);
    }

    const TArray<UStaticMeshComponent*> Accessories = {
        RoleHelmet,
        RoleChestRig,
        RoleBackpack,
        RoleLeftShoulder,
        RoleRightShoulder,
        RoleAntenna};
    for (UStaticMeshComponent* Accessory : Accessories)
    {
        if (Accessory)
        {
            Accessory->SetVisibility(false, true);
        }
    }

    if (CombatWeapon)
    {
        CombatWeapon->ClearTransientPresentation();
        CombatWeapon->SetActorHiddenInGame(true);
    }
}

void ABMEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABMEnemyCharacter, EnemyRole);
    DOREPLIFETIME(ABMEnemyCharacter, CombatWeapon);
    DOREPLIFETIME(ABMEnemyCharacter, CombatAimYaw);
    DOREPLIFETIME(ABMEnemyCharacter, CombatAimPitch);
    DOREPLIFETIME(ABMEnemyCharacter, bReplicatedCombatFacing);
}
