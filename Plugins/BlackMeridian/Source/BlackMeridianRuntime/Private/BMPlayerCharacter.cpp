// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMPlayerCharacter.h"

#include "BMGameMode.h"
#include "BMGreyboxDirector.h"
#include "BMAudioDataAsset.h"
#include "BMCharacterAnimationDataAsset.h"
#include "BMCombatTargeting.h"
#include "BMGameUserSettings.h"
#include "BMGameState.h"
#include "BMHealthComponent.h"
#include "BMInteractionInterface.h"
#include "BMMissionDirector.h"
#include "BMPlayerController.h"
#include "BMSignalBotController.h"
#include "BMSuppressionComponent.h"
#include "BMThrowable.h"
#include "BMThrowableDataAsset.h"
#include "BMWeapon.h"
#include "BMWeaponDataAsset.h"
#include "CollisionQueryParams.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWaveProcedural.h"

namespace
{
    USoundBase* LoadGeneratedPlayerCue(const TCHAR* CueName)
    {
        if (!CueName || !*CueName)
        {
            return nullptr;
        }
        const FString AssetPath = FString::Printf(
            TEXT("/Game/Audio/Generated/%s.%s"),
            CueName,
            CueName);
        return LoadObject<USoundBase>(nullptr, *AssetPath);
    }

    bool PlayGeneratedPlayerCue(
        const UObject* WorldContextObject,
        const FVector& Location,
        const TCHAR* CueName,
        const float Volume,
        const float Pitch)
    {
        if (!WorldContextObject || FParse::Param(FCommandLine::Get(), TEXT("NoSound")))
        {
            return false;
        }
        if (USoundBase* Cue = LoadGeneratedPlayerCue(CueName))
        {
            UGameplayStatics::PlaySoundAtLocation(
                WorldContextObject,
                Cue,
                Location,
                FRotator::ZeroRotator,
                Volume,
                Pitch);
            return true;
        }
        return false;
    }

    enum class ERuntimePlayerPulse : uint8
    {
        Footstep,
        Suppression,
        LowHealth,
        Land,
        ThrowableWarning
    };

    void PlayRuntimePlayerPulse(
        const UObject* WorldContextObject,
        const FVector& Location,
        const ERuntimePlayerPulse Pulse,
        const EBMSurfaceType Surface,
        const float Volume,
        const float Pitch)
    {
        if (!WorldContextObject || FParse::Param(FCommandLine::Get(), TEXT("NoSound")))
        {
            return;
        }

        constexpr int32 SampleRate = 22050;
        const float Duration = Pulse == ERuntimePlayerPulse::Footstep
            ? 0.075f
            : (Pulse == ERuntimePlayerPulse::Land
                ? 0.12f
                : (Pulse == ERuntimePlayerPulse::ThrowableWarning
                    ? 0.13f
                    : (Pulse == ERuntimePlayerPulse::Suppression ? 0.24f : 0.34f)));
        const int32 SampleCount = FMath::Max(1, FMath::RoundToInt(Duration * SampleRate));
        TArray<int16> Samples;
        Samples.SetNumUninitialized(SampleCount);
        uint32 NoiseState = 0x9E3779B9u ^ static_cast<uint32>(Pulse) * 0xA341316Cu ^ static_cast<uint32>(Surface) * 0xC8013EA4u;

        float BaseFrequency = 120.0f;
        switch (Surface)
        {
        case EBMSurfaceType::Metal: BaseFrequency = 185.0f; break;
        case EBMSurfaceType::Wood: BaseFrequency = 104.0f; break;
        case EBMSurfaceType::Glass: BaseFrequency = 420.0f; break;
        case EBMSurfaceType::SoftCover: BaseFrequency = 82.0f; break;
        case EBMSurfaceType::Concrete:
        default: break;
        }
        if (Pulse == ERuntimePlayerPulse::Suppression)
        {
            BaseFrequency = 64.0f;
        }
        else if (Pulse == ERuntimePlayerPulse::LowHealth)
        {
            BaseFrequency = 112.0f;
        }
        else if (Pulse == ERuntimePlayerPulse::Land)
        {
            BaseFrequency = 76.0f;
        }
        else if (Pulse == ERuntimePlayerPulse::ThrowableWarning)
        {
            BaseFrequency = 720.0f;
        }

        for (int32 Index = 0; Index < SampleCount; ++Index)
        {
            const float Time = static_cast<float>(Index) / static_cast<float>(SampleRate);
            const float NormalizedTime = Time / Duration;
            const float Envelope = FMath::Pow(FMath::Clamp(1.0f - NormalizedTime, 0.0f, 1.0f), Pulse == ERuntimePlayerPulse::Footstep ? 1.75f : 1.2f);
            NoiseState = NoiseState * 1664525u + 1013904223u;
            const float Noise = (static_cast<float>((NoiseState >> 9) & 0x7FFFu) / 16384.0f) - 1.0f;
            const float Transient = FMath::Sin(Time * 2.0f * PI * BaseFrequency) * 0.42f;
            const float Body = FMath::Sin(Time * 2.0f * PI * (BaseFrequency * 2.03f)) * 0.14f;
            const float Signal = Pulse == ERuntimePlayerPulse::Footstep
                ? (Transient + Noise * 0.22f + Body)
                : (Transient * (Pulse == ERuntimePlayerPulse::ThrowableWarning ? 0.82f : 0.65f)
                    + Body
                    + Noise * (Pulse == ERuntimePlayerPulse::LowHealth ? 0.06f : 0.18f));
            Samples[Index] = static_cast<int16>(FMath::Clamp(Signal * Envelope * Volume, -1.0f, 1.0f) * 32767.0f);
        }

        USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(GetTransientPackage());
        if (!Wave)
        {
            return;
        }
        Wave->NumChannels = 1;
        Wave->SetSampleRate(SampleRate);
        Wave->Duration = Duration;
        Wave->SoundGroup = SOUNDGROUP_Effects;
        Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
        UGameplayStatics::PlaySoundAtLocation(WorldContextObject, Wave, Location, FRotator::ZeroRotator, 1.0f, Pitch);
    }
}

ABMPlayerCharacter::ABMPlayerCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    Team = EBMTeam::Lattice;
    PrimaryActorTick.bCanEverTick = true;

    // Sustained hostile fire must be able to end the player's life. Healing
    // comes from deliberate squad support and the single critical-recovery
    // beat, not from an always-on health bar refill.
    if (HealthComponent)
    {
        HealthComponent->bRegenerateHealth = false;
    }

    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetRootComponent());
    FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 72.0f));
    FirstPersonCamera->bUsePawnControlRotation = true;
    FirstPersonCamera->FieldOfView = 90.0f;

    WeaponAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponAnchor"));
    WeaponAnchor->SetupAttachment(FirstPersonCamera);
    // Keep the assembled rifle high enough to read above the lower-right HUD
    // while retaining the familiar offset from the camera's sight line.
    WeaponAnchor->SetRelativeLocation(FVector(22.0f, 8.0f, -10.0f));

    FirstPersonFillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FirstPersonFillLight"));
    FirstPersonFillLight->SetupAttachment(WeaponAnchor);
    FirstPersonFillLight->SetRelativeLocation(FVector(12.0f, -17.0f, 9.0f));
    FirstPersonFillLight->SetMobility(EComponentMobility::Movable);
    FirstPersonFillLight->SetLightColor(FLinearColor(0.48f, 0.72f, 0.82f));
    FirstPersonFillLight->SetIntensity(360.0f);
    FirstPersonFillLight->SetAttenuationRadius(145.0f);
    FirstPersonFillLight->SetCastShadows(false);
    FirstPersonFillLight->SetVisibility(false);

    ThrowablePresentationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThrowablePresentationMesh"));
    ThrowablePresentationMesh->SetupAttachment(WeaponAnchor);
    ThrowablePresentationMesh->SetOnlyOwnerSee(true);
    ThrowablePresentationMesh->SetCastShadow(false);
    ThrowablePresentationMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ThrowablePresentationMesh->SetRelativeLocation(FVector(18.0f, -18.0f, -16.0f));
    ThrowablePresentationMesh->SetRelativeScale3D(FVector(0.88f));
    ThrowablePresentationMesh->SetVisibility(false);

    FirstPersonArms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonArms"));
    FirstPersonArms->SetupAttachment(FirstPersonCamera);
    FirstPersonArms->SetOnlyOwnerSee(true);
    FirstPersonArms->SetCastShadow(false);
    FirstPersonArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FirstPersonArms->SetVisibility(false);

    FallbackLeftArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackLeftArm"));
    FallbackLeftArm->SetupAttachment(WeaponAnchor);
    FallbackLeftArm->SetOnlyOwnerSee(true);
    FallbackLeftArm->SetCastShadow(false);
    FallbackLeftArm->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FallbackLeftArm->SetRelativeLocation(FVector(5.0f, -9.0f, -27.0f));
    FallbackLeftArm->SetRelativeRotation(FRotator(0.0f, 12.0f, -18.0f));
    FallbackLeftArm->SetRelativeScale3D(FVector(0.18f, 0.075f, 0.075f));
    FallbackLeftArm->SetVisibility(false);

    FallbackRightArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackRightArm"));
    FallbackRightArm->SetupAttachment(WeaponAnchor);
    FallbackRightArm->SetOnlyOwnerSee(true);
    FallbackRightArm->SetCastShadow(false);
    FallbackRightArm->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FallbackRightArm->SetRelativeLocation(FVector(11.0f, 19.0f, -26.0f));
    FallbackRightArm->SetRelativeRotation(FRotator(0.0f, 10.0f, 18.0f));
    FallbackRightArm->SetRelativeScale3D(FVector(0.2f, 0.08f, 0.08f));
    FallbackRightArm->SetVisibility(false);

    FallbackLeftHand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackLeftHand"));
    FallbackLeftHand->SetupAttachment(WeaponAnchor);
    FallbackLeftHand->SetOnlyOwnerSee(true);
    FallbackLeftHand->SetCastShadow(false);
    FallbackLeftHand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FallbackLeftHand->SetRelativeLocation(FVector(39.0f, 8.0f, -19.0f));
    FallbackLeftHand->SetRelativeScale3D(FVector(0.075f, 0.06f, 0.07f));
    FallbackLeftHand->SetVisibility(false);

    FallbackRightHand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackRightHand"));
    FallbackRightHand->SetupAttachment(WeaponAnchor);
    FallbackRightHand->SetOnlyOwnerSee(true);
    FallbackRightHand->SetCastShadow(false);
    FallbackRightHand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FallbackRightHand->SetRelativeLocation(FVector(19.0f, 8.0f, -20.0f));
    FallbackRightHand->SetRelativeScale3D(FVector(0.08f, 0.065f, 0.072f));
    FallbackRightHand->SetVisibility(false);

    FallbackLeftCuff = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackLeftCuff"));
    FallbackLeftCuff->SetupAttachment(WeaponAnchor);
    FallbackLeftCuff->SetOnlyOwnerSee(true);
    FallbackLeftCuff->SetCastShadow(false);
    FallbackLeftCuff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FallbackLeftCuff->SetVisibility(false);

    FallbackRightCuff = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackRightCuff"));
    FallbackRightCuff->SetupAttachment(WeaponAnchor);
    FallbackRightCuff->SetOnlyOwnerSee(true);
    FallbackRightCuff->SetCastShadow(false);
    FallbackRightCuff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FallbackRightCuff->SetVisibility(false);

    FallbackLeftGlovePanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackLeftGlovePanel"));
    FallbackLeftGlovePanel->SetupAttachment(WeaponAnchor);
    FallbackLeftGlovePanel->SetOnlyOwnerSee(true);
    FallbackLeftGlovePanel->SetCastShadow(false);
    FallbackLeftGlovePanel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FallbackLeftGlovePanel->SetVisibility(false);

    FallbackRightGlovePanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackRightGlovePanel"));
    FallbackRightGlovePanel->SetupAttachment(WeaponAnchor);
    FallbackRightGlovePanel->SetOnlyOwnerSee(true);
    FallbackRightGlovePanel->SetCastShadow(false);
    FallbackRightGlovePanel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FallbackRightGlovePanel->SetVisibility(false);

    FallbackLeftKnucklePlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackLeftKnucklePlate"));
    FallbackLeftKnucklePlate->SetupAttachment(WeaponAnchor);
    FallbackLeftKnucklePlate->SetOnlyOwnerSee(true);
    FallbackLeftKnucklePlate->SetCastShadow(false);
    FallbackLeftKnucklePlate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FallbackLeftKnucklePlate->SetVisibility(false);

    FallbackRightKnucklePlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackRightKnucklePlate"));
    FallbackRightKnucklePlate->SetupAttachment(WeaponAnchor);
    FallbackRightKnucklePlate->SetOnlyOwnerSee(true);
    FallbackRightKnucklePlate->SetCastShadow(false);
    FallbackRightKnucklePlate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FallbackRightKnucklePlate->SetVisibility(false);

    FallbackLeftArmBaseLocation = FallbackLeftArm->GetRelativeLocation();
    FallbackRightArmBaseLocation = FallbackRightArm->GetRelativeLocation();
    FallbackLeftArmBaseRotation = FallbackLeftArm->GetRelativeRotation();
    FallbackRightArmBaseRotation = FallbackRightArm->GetRelativeRotation();
    FallbackLeftHandBaseLocation = FallbackLeftHand->GetRelativeLocation();
    FallbackRightHandBaseLocation = FallbackRightHand->GetRelativeLocation();
    FallbackLeftHandBaseRotation = FallbackLeftHand->GetRelativeRotation();
    FallbackRightHandBaseRotation = FallbackRightHand->GetRelativeRotation();

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CubeMesh.Succeeded())
    {
        FallbackLeftArm->SetStaticMesh(CubeMesh.Object);
        FallbackRightArm->SetStaticMesh(CubeMesh.Object);
    }
    if (SphereMesh.Succeeded())
    {
        FallbackLeftHand->SetStaticMesh(SphereMesh.Object);
        FallbackRightHand->SetStaticMesh(SphereMesh.Object);
        ThrowablePresentationMesh->SetStaticMesh(SphereMesh.Object);
    }
    if (CylinderMesh.Succeeded())
    {
        // BasicShape Cylinder is authored on local Z. ConfigureFallbackArms
        // aligns these tapered-looking sleeves between the elbow and grip.
        FallbackLeftArm->SetStaticMesh(CylinderMesh.Object);
        FallbackRightArm->SetStaticMesh(CylinderMesh.Object);
        FallbackLeftCuff->SetStaticMesh(CylinderMesh.Object);
        FallbackRightCuff->SetStaticMesh(CylinderMesh.Object);
    }
    if (CubeMesh.Succeeded())
    {
        FallbackLeftGlovePanel->SetStaticMesh(CubeMesh.Object);
        FallbackRightGlovePanel->SetStaticMesh(CubeMesh.Object);
        FallbackLeftKnucklePlate->SetStaticMesh(CubeMesh.Object);
        FallbackRightKnucklePlate->SetStaticMesh(CubeMesh.Object);
    }

    // Do not assign the full-body Manny development mesh to the owner-only
    // first-person slot. A dedicated arms mesh can still be supplied by a
    // Blueprint child; otherwise the procedural sleeves below are the safe
    // packaged fallback and cannot occlude the camera.
    FirstPersonAnimBlueprint = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(
        TEXT("/MoverExamples/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C")));

    GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
    GetCharacterMovement()->MaxWalkSpeedCrouched = 170.0f;
}

void ABMPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    // The perimeter colliders are the normal protection. Keep the spawn
    // transform as an emergency safety point so a transient physics/cook
    // problem can return the pawn to the floor without damaging the player
    // or ending the mission.
    LastSafeGroundLocation = GetActorLocation();
    LastSafeGroundRotation = GetActorRotation();
    bHasSafeGroundLocation = true;

    if (HealthComponent)
    {
        HealthComponent->OnHealthChanged.AddUniqueDynamic(this, &ABMPlayerCharacter::HandleLocalHealthChanged);
    }

    // Runtime geometry is spawned by the authoritative game mode and then
    // replicated. Hold client pawns at their spawn transform until the
    // replicated floor exists; otherwise CharacterMovement can fall through
    // the still-loading source-only arena before the first input arrives.
    bAwaitingRuntimeFloor = !HasAuthority();
    if (bAwaitingRuntimeFloor && GetCharacterMovement())
    {
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->GravityScale = 0.0f;
        GetCharacterMovement()->SetMovementMode(MOVE_Flying);
    }

    DefaultWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
    DefaultFOV = FirstPersonCamera->FieldOfView;
    WeaponAnchorBaseLocation = WeaponAnchor ? WeaponAnchor->GetRelativeLocation() : FVector(22.0f, 10.0f, -14.0f);
    WeaponAnchorBaseRotation = WeaponAnchor ? WeaponAnchor->GetRelativeRotation() : FRotator::ZeroRotator;
    CameraStanceHeight = FirstPersonCamera ? FirstPersonCamera->GetRelativeLocation().Z : 72.0f;
    if (IsLocallyControlled())
    {
        // A fresh Signal Control session should look into the arena, not at
        // the empty Entry-map horizon.  PlayerStart rotations cover normal
        // respawns; this local correction also handles the first pawn created
        // before the mode has finished selecting its start.
        if (const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
            State && Controller)
        {
            if (State->bMatchLive && !State->bSignalControlMode)
            {
                BeginMissionLookGuard();
            }
            const FVector LookTarget = State->bSignalControlMode
                ? FVector(650.0f, 0.0f, 72.0f)
                : FVector(250.0f, -500.0f, 72.0f);
            const FRotator LookRotation = (LookTarget - GetActorLocation()).Rotation();
            const FRotator StartRotation(State->bSignalControlMode ? 0.0f : -4.0f, LookRotation.Yaw, 0.0f);
            Controller->SetControlRotation(StartRotation);
            SetActorRotation(FRotator(0.0f, LookRotation.Yaw, 0.0f));
        }

        // Hide only the local body; AI and remote players use the same
        // placeholder mesh as their visible third-person representation.
        PlaceholderBody->SetVisibility(false, true);
        if (FirstPersonFillLight)
        {
            FirstPersonFillLight->SetVisibility(true);
        }
        if (const UBMGameUserSettings* Settings = Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()))
        {
            DefaultFOV = Settings->FieldOfView;
            FirstPersonCamera->FieldOfView = DefaultFOV;
        }

        if (FirstPersonArms && !FirstPersonArmsMesh.IsNull())
        {
            const FString ArmsMeshPath = FirstPersonArmsMesh.ToSoftObjectPath().ToString();
            const bool bKnownFullBodyMesh = ArmsMeshPath.EndsWith(
                TEXT("/SKM_Manny_Simple.SKM_Manny_Simple"));
            if (bKnownFullBodyMesh)
            {
                // This asset is authored for third-person characters. Using
                // it in the camera produces a giant torso/head silhouette;
                // leave the component hidden so ConfigureFallbackArms() can
                // enable the unobstructed procedural presentation instead.
                FirstPersonArms->SetSkeletalMesh(nullptr);
                FirstPersonArms->SetVisibility(false, true);
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT("Black Meridian presentation: skipped full-body first-person mesh pawn=%s mesh=%s fallback=procedural"),
                    *GetNameSafe(this),
                    *ArmsMeshPath);
            }
            else if (USkeletalMesh* LoadedArmsMesh = FirstPersonArmsMesh.LoadSynchronous())
            {
                FirstPersonArms->SetSkeletalMesh(LoadedArmsMesh);
                FirstPersonArms->SetVisibility(true);
            }
        }
        if (FirstPersonArms && AnimationData)
        {
            if (UClass* AnimClass = AnimationData->AnimBlueprintClass.Get())
            {
                FirstPersonArms->SetAnimationMode(EAnimationMode::AnimationBlueprint);
                FirstPersonArms->SetAnimInstanceClass(AnimClass);
            }
        }
        if (FirstPersonArms)
        {
            FirstPersonArmsBaseLocation = FirstPersonArms->GetRelativeLocation();
            FirstPersonArmsBaseRotation = FirstPersonArms->GetRelativeRotation();
            if (!AnimationData && FirstPersonArms->IsVisible())
            {
                InitializeFirstPersonAnimationPresentation();
            }
        }
        ConfigureFallbackArms();
        ConfigureThrowablePresentation();
    }
    else
    {
        PlaceholderBody->SetVisibility(true, true);
        if (FirstPersonFillLight)
        {
            FirstPersonFillLight->SetVisibility(false);
        }
        ConfigurePlaceholderBody(Team == EBMTeam::Lattice ? FColor(30, 90, 125) : FColor(145, 28, 42));
    }

    if (HasAuthority())
    {
        SpawnLoadout();
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian combat: player pawn ready name=%s authority=%s location=%s floor_mode=%s"),
        *GetName(),
        HasAuthority() ? TEXT("yes") : TEXT("no"),
        *GetActorLocation().ToString(),
        GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround() ? TEXT("grounded") : TEXT("airborne"));
}

void ABMPlayerCharacter::Landed(const FHitResult& Hit)
{
    const float VerticalImpactSpeed = GetCharacterMovement()
        ? FMath::Abs(GetCharacterMovement()->Velocity.Z)
        : 0.0f;
    Super::Landed(Hit);

    if (!IsLocallyControlled())
    {
        return;
    }

    const float ImpactAlpha = FMath::Clamp(VerticalImpactSpeed / 720.0f, 0.0f, 1.0f);
    CameraLandingOffset = FMath::Min(CameraLandingOffset, -FMath::Lerp(1.5f, 8.5f, ImpactAlpha));
    CameraLandingRoll = FMath::Clamp(
        CameraLandingRoll + (FMath::RandBool() ? 1.0f : -1.0f) * FMath::Lerp(0.35f, 2.2f, ImpactAlpha),
        -3.5f,
        3.5f);
    if (!AudioProfile.LoadSynchronous())
    {
        if (!PlayGeneratedPlayerCue(this, GetActorLocation(), TEXT("BM_Impact_Concrete"), 0.32f, 0.82f))
        {
            PlayRuntimePlayerPulse(this, GetActorLocation(), ERuntimePlayerPulse::Land, ResolveMovementSurfaceType(), 0.26f, 0.82f);
        }
    }
    PlayCharacterAnimation_Implementation(TEXT("Land"));
}

void ABMPlayerCharacter::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bAwaitingRuntimeFloor)
    {
        bool bFloorReady = false;
        if (GetWorld())
        {
            for (TActorIterator<ABMGreyboxDirector> Iterator(GetWorld()); Iterator; ++Iterator)
            {
                if (Iterator->IsRuntimeFloorReady())
                {
                    bFloorReady = true;
                    break;
                }
            }
        }

        if (bFloorReady && GetCharacterMovement())
        {
            bAwaitingRuntimeFloor = false;
            GetCharacterMovement()->GravityScale = 1.0f;
            GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            UE_LOG(LogTemp, Display, TEXT("Black Meridian combat: runtime floor ready for pawn=%s"), *GetName());
        }
        else
        {
            if (GetCharacterMovement())
            {
                GetCharacterMovement()->StopMovementImmediately();
            }
            return;
        }
    }

    // Perimeter collision prevents normal traversal from leaving the arena.
    // If a cooked collision gap or a transient physics issue still sends the
    // pawn below the floor, return it to the last grounded transform instead
    // of applying damage or entering the mission-death path.
    if (HasAuthority() && IsPlayerControlled() && !bEliminated)
    {
        const FVector CurrentLocation = GetActorLocation();
        if (GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround() && CurrentLocation.Z > -100.0f)
        {
            LastSafeGroundLocation = CurrentLocation;
            LastSafeGroundRotation = GetActorRotation();
            bHasSafeGroundLocation = true;
        }

        constexpr float PlayerOutOfBoundsRecoveryZ = -300.0f;
        if (bHasSafeGroundLocation && CurrentLocation.Z < PlayerOutOfBoundsRecoveryZ)
        {
            RecoverFromBoundaryFall();
        }
    }

    StartupLookGuardRemaining = FMath::Max(0.0f, StartupLookGuardRemaining - DeltaSeconds);
    FireInputGuardRemaining = FMath::Max(0.0f, FireInputGuardRemaining - DeltaSeconds);
    if (PendingInteraction.IsValid())
    {
        InteractionHoldElapsed = FMath::Min(
            InteractionHoldDuration,
            InteractionHoldElapsed + FMath::Max(0.0f, DeltaSeconds));
    }
    if (!HasAuthority())
    {
        RecoverReplicatedWeaponState();
    }

    // Update every camera transform that contributes to the rendered center
    // ray before sampling held fire.  The HUD is drawn after actor ticks, so
    // firing first made automatic shots use the previous frame's bob/stance
    // origin while the reticle used the newly updated camera.  That was most
    // visible at close range and around cover: the enemy could be red on the
    // reticle, but the authoritative shot started a few centimetres away and
    // failed the same visibility query.  Keeping the camera sample ahead of
    // FireWeapon makes the rendered reticle and submitted gameplay ray share
    // one frame's transform.
    UpdateCameraBob(DeltaSeconds);
    UpdateCameraPresentation(DeltaSeconds);
    ApplyPlayableCameraCue();
    UpdateLocalAimSnapshot();

    // Process a queued press only after every camera transform contributing to
    // the rendered centre ray has been updated.  The HUD is drawn after actor
    // ticks, so an immediate semi-auto shot could otherwise use the previous
    // bob/stance/cue transform while the reticle used this frame's transform.
    // Keep the queued press alive through StopFire(): a quick click is still a
    // real single shot even when press and release arrive in one frame.
    const bool bProcessedPendingFire = ProcessPendingFire();
    if (!bProcessedPendingFire
        && bWantsToFire
        && bFireInputArmed
        && !bAutomaticFireBlockedUntilRelease
        && CurrentWeapon
        && CurrentWeapon->GetWeaponData()
        && CurrentWeapon->GetWeaponData()->FireMode != EBMWeaponFireMode::SemiAutomatic)
    {
        FireWeapon();
    }
    UpdateInteractionPrompt();
    UpdateAnimationPresentation();
    UpdateSuppressionPresentation();
    UpdateAudioPresentation(DeltaSeconds);
    if (IsLocallyControlled())
    {
        UpdateThrowableThreatPresentation(DeltaSeconds);
        UpdateThrowablePresentation(DeltaSeconds);
        UpdateWeaponPresentation(DeltaSeconds);
    }
}

void ABMPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ABMPlayerCharacter::MoveForward);
    PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ABMPlayerCharacter::MoveRight);
    PlayerInputComponent->BindAxis(TEXT("Turn"), this, &ABMPlayerCharacter::Turn);
    PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &ABMPlayerCharacter::LookUp);
    PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &ABMPlayerCharacter::StartFire);
    PlayerInputComponent->BindAction(TEXT("Fire"), IE_Released, this, &ABMPlayerCharacter::StopFire);
    PlayerInputComponent->BindAction(TEXT("Aim"), IE_Pressed, this, &ABMPlayerCharacter::StartAim);
    PlayerInputComponent->BindAction(TEXT("Aim"), IE_Released, this, &ABMPlayerCharacter::StopAim);
    PlayerInputComponent->BindAction(TEXT("Reload"), IE_Pressed, this, &ABMPlayerCharacter::ReloadWeapon);
    PlayerInputComponent->BindAction(TEXT("SwitchWeapon"), IE_Pressed, this, &ABMPlayerCharacter::SwitchWeapon);
    PlayerInputComponent->BindAction(TEXT("Melee"), IE_Pressed, this, &ABMPlayerCharacter::MeleeWeapon);
    PlayerInputComponent->BindAction(TEXT("Frag"), IE_Pressed, this, &ABMPlayerCharacter::ThrowFrag);
    PlayerInputComponent->BindAction(TEXT("Tactical"), IE_Pressed, this, &ABMPlayerCharacter::ThrowTactical);
    PlayerInputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &ABMPlayerCharacter::Interact);
    PlayerInputComponent->BindAction(TEXT("Interact"), IE_Released, this, &ABMPlayerCharacter::StopInteract);
    PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Pressed, this, &ABMPlayerCharacter::StartSprint);
    PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Released, this, &ABMPlayerCharacter::StopSprint);
    PlayerInputComponent->BindAction(TEXT("TacticalSprint"), IE_Pressed, this, &ABMPlayerCharacter::StartTacticalSprint);
    PlayerInputComponent->BindAction(TEXT("TacticalSprint"), IE_Released, this, &ABMPlayerCharacter::StopTacticalSprint);
    PlayerInputComponent->BindAction(TEXT("Crouch"), IE_Pressed, this, &ABMPlayerCharacter::StartCrouch);
    PlayerInputComponent->BindAction(TEXT("Crouch"), IE_Released, this, &ABMPlayerCharacter::StopCrouch);
    PlayerInputComponent->BindAction(TEXT("Prone"), IE_Pressed, this, &ABMPlayerCharacter::ToggleProne);
    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ABMPlayerCharacter::TryMantle);
    // TryMantle uses a discrete launch rather than relying on ACharacter's
    // held-jump state. Still clear the inherited flag on release so an older
    // input event or a remapped binding can never leave the pawn auto-jumping.
    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ABMPlayerCharacter::StopJumpInput);
    PlayerInputComponent->BindAction(TEXT("LeanLeft"), IE_Pressed, this, &ABMPlayerCharacter::LeanLeft);
    PlayerInputComponent->BindAction(TEXT("LeanRight"), IE_Pressed, this, &ABMPlayerCharacter::LeanRight);
    PlayerInputComponent->BindAction(TEXT("LeanReset"), IE_Pressed, this, &ABMPlayerCharacter::ResetLean);
}

void ABMPlayerCharacter::MoveForward(const float Value)
{
    if (bAwaitingRuntimeFloor || bMountedWeapon || IsSignalControlLocked())
    {
        return;
    }

    if (bOnLadder)
    {
        AddMovementInput(FVector::UpVector, Value);
        return;
    }

    if (Controller && FMath::Abs(Value) > KINDA_SMALL_NUMBER)
    {
        const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
        AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Value);
    }
}

void ABMPlayerCharacter::MoveRight(const float Value)
{
    if (bAwaitingRuntimeFloor || bMountedWeapon || IsSignalControlLocked())
    {
        return;
    }

    if (Controller && FMath::Abs(Value) > KINDA_SMALL_NUMBER)
    {
        const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
        AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Value);
    }
}

void ABMPlayerCharacter::StopJumpInput()
{
    StopJumping();
}

void ABMPlayerCharacter::Turn(const float Value)
{
    if (StartupLookGuardRemaining > 0.0f || IsPlayableCameraLookLocked())
    {
        return;
    }

    // macOS can report a captured-cursor recapture as a large but finite
    // MouseX delta. Ignore only the implausible one-frame jump; ordinary
    // high-DPI turns remain responsive below this bound.
    constexpr float MaxYawInputPerFrame = 24.0f;
    if (!FMath::IsFinite(Value) || FMath::Abs(Value) > MaxYawInputPerFrame)
    {
        return;
    }

    // A stale look-ignore counter can survive a paused death/menu transition.
    // Clear it when real gameplay look input arrives; normal cinematic locks
    // have already returned through IsPlayableCameraLookLocked() above.
    if (ABMPlayerController* BMController = Cast<ABMPlayerController>(Controller);
        BMController && BMController->IsLookInputIgnored() && !BMController->IsPaused())
    {
        BMController->ResetIgnoreLookInput();
    }

    float Sensitivity = 1.0f;
    if (const UBMGameUserSettings* Settings = Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()))
    {
        Sensitivity = Settings->HorizontalSensitivity * (bAiming ? Settings->ADSSensitivity : Settings->HipFireSensitivity);
    }
    AddControllerYawInput(Value * Sensitivity);
}

void ABMPlayerCharacter::LookUp(const float Value)
{
    if (StartupLookGuardRemaining > 0.0f || IsPlayableCameraLookLocked())
    {
        return;
    }

    // See Turn(): the same focus/capture jump can arrive on MouseY and pitch
    // the camera straight into the floor before the player moves. Pitch has a
    // tighter bound because a single vertical recapture delta is otherwise
    // enough to make every projectile, including grenades, leave at the
    // player's feet.
    constexpr float MaxPitchInputPerFrame = 12.0f;
    if (!FMath::IsFinite(Value) || FMath::Abs(Value) > MaxPitchInputPerFrame)
    {
        return;
    }

    // Keep pitch recovery symmetric with yaw recovery. A stale ignore counter
    // must not make the camera appear frozen after the player resumes.
    if (ABMPlayerController* BMController = Cast<ABMPlayerController>(Controller);
        BMController && BMController->IsLookInputIgnored() && !BMController->IsPaused())
    {
        BMController->ResetIgnoreLookInput();
    }

    float Sensitivity = 1.0f;
    if (const UBMGameUserSettings* Settings = Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()))
    {
        Sensitivity = Settings->VerticalSensitivity * (bAiming ? Settings->ADSSensitivity : Settings->HipFireSensitivity);
    }
    const float CurrentPitch = FMath::UnwindDegrees(Controller ? Controller->GetControlRotation().Pitch : 0.0f);
    const float RequestedPitchDelta = Value * Sensitivity;
    const float TargetPitch = FMath::Clamp(CurrentPitch + RequestedPitchDelta, -82.0f, 82.0f);
    AddControllerPitchInput(TargetPitch - CurrentPitch);
}

void ABMPlayerCharacter::BeginMissionLookGuard(const float Duration)
{
    StartupLookGuardRemaining = FMath::Max(StartupLookGuardRemaining, FMath::Max(0.0f, Duration));
}

void ABMPlayerCharacter::RecoverFromBoundaryFall()
{
    if (!HasAuthority() || !IsPlayerControlled() || bEliminated || !bHasSafeGroundLocation || GetActorLocation().Z >= -300.0f)
    {
        return;
    }

    const FVector CurrentLocation = GetActorLocation();
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
        Movement->SetMovementMode(MOVE_Walking);
    }
    SetActorLocationAndRotation(
        LastSafeGroundLocation,
        LastSafeGroundRotation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Black Meridian combat: boundary guard returned pawn=%s from=%s to=%s action=keep_alive"),
        *GetNameSafe(this),
        *CurrentLocation.ToString(),
        *LastSafeGroundLocation.ToString());
}

void ABMPlayerCharacter::StartSprint()
{
    if (bMountedWeapon)
    {
        return;
    }

    if (const UBMGameUserSettings* Settings = Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()); Settings && Settings->bToggleSprint && bSprinting)
    {
        StopSprint();
        return;
    }
    bSprinting = true;
    bAiming = false;
    GetCharacterMovement()->MaxWalkSpeed = 620.0f;
    if (!HasAuthority())
    {
        ServerSetSprintState(true, false);
    }
}

void ABMPlayerCharacter::StopSprint()
{
    if (const UBMGameUserSettings* Settings = Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()); Settings && Settings->bToggleSprint && !bTacticalSprinting)
    {
        return;
    }

    StopSprintInternal();
}

void ABMPlayerCharacter::StopSprintInternal()
{
    bSprinting = false;
    bTacticalSprinting = false;
    GetCharacterMovement()->MaxWalkSpeed = bProne ? 100.0f : (IsCrouched() ? GetCharacterMovement()->MaxWalkSpeedCrouched : DefaultWalkSpeed);
    if (!HasAuthority())
    {
        ServerSetSprintState(false, false);
    }
}

void ABMPlayerCharacter::StartTacticalSprint()
{
    if (bMountedWeapon)
    {
        return;
    }

    bTacticalSprinting = true;
    bSprinting = true;
    bAiming = false;
    GetCharacterMovement()->MaxWalkSpeed = 760.0f;
    if (!HasAuthority())
    {
        ServerSetSprintState(true, true);
    }
}

void ABMPlayerCharacter::StopTacticalSprint()
{
    StopSprintInternal();
}

void ABMPlayerCharacter::StartAim()
{
    if (const UBMGameUserSettings* Settings = Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()); Settings && Settings->bToggleADS && bAiming)
    {
        bAiming = false;
        GetCharacterMovement()->MaxWalkSpeed = bSprinting ? 620.0f : DefaultWalkSpeed;
        if (!HasAuthority())
        {
            ServerSetAimState(false);
        }
        return;
    }
    bAiming = true;
    bSprinting = false;
    bTacticalSprinting = false;
    GetCharacterMovement()->MaxWalkSpeed = 285.0f;
    if (!HasAuthority())
    {
        ServerSetAimState(true);
    }
}

void ABMPlayerCharacter::StopAim()
{
    if (const UBMGameUserSettings* Settings = Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()); Settings && Settings->bToggleADS)
    {
        return;
    }
    bAiming = false;
    GetCharacterMovement()->MaxWalkSpeed = bSprinting ? 620.0f : DefaultWalkSpeed;
    if (!HasAuthority())
    {
        ServerSetAimState(false);
    }
}

void ABMPlayerCharacter::StartCrouch()
{
    if (bMountedWeapon)
    {
        return;
    }

    if (const UBMGameUserSettings* Settings = Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()); Settings && Settings->bToggleCrouch && IsCrouched())
    {
        StopCrouchInternal();
        return;
    }
    if (bSprinting)
    {
        StartSlide();
        return;
    }
    if (!bProne)
    {
        Crouch();
        if (!HasAuthority())
        {
            ServerSetCrouchState(true);
        }
    }
}

void ABMPlayerCharacter::StopCrouch()
{
    if (const UBMGameUserSettings* Settings = Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()); Settings && Settings->bToggleCrouch)
    {
        return;
    }

    StopCrouchInternal();
}

void ABMPlayerCharacter::StopCrouchInternal()
{
    if (!bProne)
    {
        UnCrouch();
        GetCharacterMovement()->MaxWalkSpeed = bSprinting ? 620.0f : DefaultWalkSpeed;
        if (!HasAuthority())
        {
            ServerSetCrouchState(false);
        }
    }
}

void ABMPlayerCharacter::ToggleProne()
{
    if (bMountedWeapon)
    {
        return;
    }

    bProne = !bProne;
    if (bProne)
    {
        Crouch();
        GetCharacterMovement()->MaxWalkSpeed = 100.0f;
    }
    else
    {
        UnCrouch();
        GetCharacterMovement()->MaxWalkSpeed = DefaultWalkSpeed;
    }
    if (!HasAuthority())
    {
        ServerSetProneState(bProne);
    }
}

void ABMPlayerCharacter::TryMantle()
{
    if (bMountedWeapon || IsSignalControlLocked())
    {
        return;
    }

    if (!HasAuthority())
    {
        ServerTryMantle();
        return;
    }

    TryMantleInternal();
}

void ABMPlayerCharacter::TryMantleInternal()
{
    if (!HasAuthority() || bMountedWeapon || IsSignalControlLocked())
    {
        return;
    }

    if (bOnLadder)
    {
        EndLadderClimb();
        return;
    }

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    // A client can briefly be in MOVE_Flying while the runtime floor is
    // being installed, and ladder/mounted transitions also use non-grounded
    // modes. Do not pass those states to Jump(): with zero gravity that turns
    // a normal space press into an unbounded climb.
    if (bAwaitingRuntimeFloor || bProne || !Movement || !Movement->IsMovingOnGround())
    {
        return;
    }

    const FVector Start = FirstPersonCamera->GetComponentLocation();
    const FVector Forward = FirstPersonCamera->GetForwardVector();
    FHitResult ObstacleHit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BlackMeridianMantle), false, this);
    if (GetWorld()->LineTraceSingleByChannel(ObstacleHit, Start, Start + Forward * 100.0f, ECC_Visibility, Params))
    {
        const FVector MantleLocation = ObstacleHit.ImpactPoint + Forward * 35.0f + FVector(0.0f, 0.0f, 105.0f);
        FHitResult LandingHit;
        if (!GetWorld()->LineTraceSingleByChannel(LandingHit, MantleLocation + FVector(0.0f, 0.0f, 80.0f), MantleLocation - FVector(0.0f, 0.0f, 100.0f), ECC_Visibility, Params))
        {
            StopJumping();
            SetActorLocation(MantleLocation, true);
            return;
        }
    }

    // Keep the hop deterministic and bounded. ACharacter::Jump() leaves
    // bPressedJump set until StopJumping(), and this action intentionally has
    // no hold-to-jump behavior. A direct launch gives one ballistic impulse,
    // while restoring gravity protects level transitions from stale ladder or
    // runtime-floor state.
    constexpr float PlayerJumpVelocity = 420.0f;
    StopJumping();
    Movement->GravityScale = 1.0f;
    LaunchCharacter(FVector(0.0f, 0.0f, PlayerJumpVelocity), false, true);
}

void ABMPlayerCharacter::ServerTryMantle_Implementation()
{
    TryMantleInternal();
}

void ABMPlayerCharacter::StartSlide()
{
    if (!bSprinting || !GetCharacterMovement()->IsMovingOnGround())
    {
        return;
    }

    Crouch();
    if (!HasAuthority())
    {
        ServerSetCrouchState(true);
    }
    PlayCharacterAnimation_Implementation(TEXT("Slide"));
    LaunchCharacter(GetVelocity().GetSafeNormal2D() * 300.0f, true, false);
    GetWorld()->GetTimerManager().SetTimer(SlideTimerHandle, this, &ABMPlayerCharacter::StopCrouchInternal, 0.65f, false);
}

void ABMPlayerCharacter::FireWeapon()
{
#if !UE_BUILD_SHIPPING
    // Only an explicitly passive capture may suppress firing. Keep the
    // showcase label itself playable: it is also used to inspect the real
    // combat presentation, and an accidental visual-only flag must never
    // make a player build appear unable to shoot.
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_PassiveCombatCapture")))
    {
        bWantsToFire = false;
        return;
    }
#endif

    if (IsSignalControlLocked() || !CurrentWeapon || (bSprinting && !bMountedWeapon))
    {
        return;
    }
    if (CurrentWeapon->IsReloading())
    {
        // Do not turn a held trigger into a per-frame reload cancel. The
        // reload completes, then the next fire tick can consume a round.
        return;
    }

    // Fire from the same post-camera sample that the HUD rendered.  Refresh
    // here as well for immediate development probes and other non-Tick input
    // seams; ordinary input already captured this sample just above.
    UpdateLocalAimSnapshot();
    const FVector Origin = HasLocalAimSnapshot()
        ? GetLocalAimOrigin()
        : FirstPersonCamera->GetComponentLocation();
    const FVector Direction = HasLocalAimSnapshot()
        ? GetLocalAimDirection()
        : GetAimDirection();
    CurrentWeapon->Fire(Origin, Direction, bAiming);
}

#if !UE_BUILD_SHIPPING
void ABMPlayerCharacter::PrepareCombatProbeTransform(const FVector& Location, const FRotator& Rotation)
{
    if (!IsLocallyControlled())
    {
        return;
    }

    if (GetCharacterMovement())
    {
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->GravityScale = 0.0f;
        GetCharacterMovement()->SetMovementMode(MOVE_None);
    }
    SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    if (Controller)
    {
        Controller->SetControlRotation(Rotation);
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian combat probe: client transform synchronized pawn=%s location=%s rotation=%s"),
        *GetName(),
        *GetActorLocation().ToString(),
        *GetActorRotation().ToString());
}

void ABMPlayerCharacter::RunThrowablePresentationShowcase(const EBMWeaponId InThrowableId)
{
    if (!IsLocallyControlled())
    {
        return;
    }

    BeginThrowablePresentation(InThrowableId);
    // Use the ordinary player action as part of the showcase. This keeps the
    // rendered capture honest: the mesh-in-hand, server spawn, projectile
    // motion, inventory decrement, fuse, and detonation all use the same path
    // as a real Frag/Veil input.
    if (InThrowableId == EBMWeaponId::VeilGrenade)
    {
        ThrowTactical();
    }
    else
    {
        ThrowFrag();
    }
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian throwable presentation showcase: id=%d mesh=%s active=%s real_throw=yes"),
        static_cast<int32>(InThrowableId),
        ThrowablePresentationMesh && ThrowablePresentationMesh->GetStaticMesh()
            ? *ThrowablePresentationMesh->GetStaticMesh()->GetPathName()
            : TEXT("none"),
        IsThrowablePresentationActive() ? TEXT("yes") : TEXT("no"));
}

void ABMPlayerCharacter::RunCombatProbeFire(const FVector& Direction)
{
    if (!IsLocallyControlled() || !FirstPersonCamera || !CurrentWeapon)
    {
        UE_LOG(LogTemp, Display, TEXT("Black Meridian combat probe: client fire unavailable pawn=%s weapon=%s"), *GetName(), CurrentWeapon ? TEXT("yes") : TEXT("no"));
        return;
    }

    const FRotator AimRotation = Direction.GetSafeNormal().Rotation();
    FRotator CameraCueRotation = FRotator::ZeroRotator;
    if (GetWorld())
    {
        if (!CachedMissionDirector.IsValid())
        {
            TActorIterator<ABMMissionDirector> Iterator(GetWorld());
            if (Iterator)
            {
                CachedMissionDirector = *Iterator;
            }
        }
        if (CachedMissionDirector.IsValid() && CachedMissionDirector->IsPlayableSequenceActive())
        {
            const FBMPlayableCameraCue Cue = CachedMissionDirector->GetActivePlayableCameraCue();
            const float CueProgress = CachedMissionDirector->GetActivePlayableCameraCueProgress();
            const float EasedProgress = FMath::InterpEaseInOut(
                0.0f,
                1.0f,
                CueProgress,
                2.0f);
            CameraCueRotation = FRotator(
                FMath::Lerp(Cue.StartCameraRotation.Pitch, Cue.CameraRotation.Pitch, EasedProgress),
                FMath::Lerp(Cue.StartCameraRotation.Yaw, Cue.CameraRotation.Yaw, EasedProgress),
                FMath::Lerp(Cue.StartCameraRotation.Roll, Cue.CameraRotation.Roll, EasedProgress));
        }
    }

    // The development seam supplies a rendered direction directly. During a
    // playable cue the authority helper adds the cue to the controller view,
    // so stage the inverse base rotation and preserve the same cue as the
    // camera's relative rotation. This keeps the synthetic request on the
    // exact ray it claims to render and prevents the cue-aware authority
    // guard from rejecting the probe before it can test damage.
    FRotator ControlRotation = AimRotation - CameraCueRotation;
    ControlRotation.Roll = 0.0f;
    if (Controller)
    {
        Controller->SetControlRotation(ControlRotation);
    }
    const bool bProbeCameraUsedPawnControlRotation = FirstPersonCamera
        ? FirstPersonCamera->bUsePawnControlRotation
        : false;
    if (FirstPersonCamera)
    {
        // bUsePawnControlRotation and the playable cue are both applied by
        // the normal camera update. A relative cue assignment here can leave
        // the component on the previous controller yaw until the next tick,
        // so the simulated click would submit a different ray from the one
        // it was asked to test. Set the rendered world rotation explicitly
        // for this immediate input-path request; the next ordinary camera
        // tick restores the production cue interpolation.
        FirstPersonCamera->bUsePawnControlRotation = false;
        FirstPersonCamera->SetWorldRotation(AimRotation);
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian combat probe: client fire request pawn=%s magazine_before=%d reserve=%d aim=%s"),
        *GetName(),
        CurrentWeapon->GetMagazineAmmo(),
        CurrentWeapon->GetReserveAmmo(),
        *GetAimDirection().ToString());

    // Exercise the same input path as a player click. Stop immediately so an
    // automatic weapon contributes one deterministic request to the probe.
    StartFireInternal(true);
    StopFire();
    // The probe supplies the rendered camera rotation directly above and
    // intentionally releases in the same frame. Process its queued click
    // before restoring normal camera ownership so it tests the exact ray it
    // claims to render, while ordinary input is processed from Tick() above.
    ProcessPendingFire();
    if (FirstPersonCamera)
    {
        FirstPersonCamera->bUsePawnControlRotation = bProbeCameraUsedPawnControlRotation;
    }
}

void ABMPlayerCharacter::RunCombatProbeReload()
{
    if (!IsLocallyControlled() || !CurrentWeapon)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat probe: client reload unavailable pawn=%s weapon=%s"),
            *GetName(),
            CurrentWeapon ? TEXT("yes") : TEXT("no"));
        return;
    }

    const int32 MagazineBefore = CurrentWeapon->GetMagazineAmmo();
    const int32 ReserveBefore = CurrentWeapon->GetReserveAmmo();
    ReloadWeapon();
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian combat probe: client reload request pawn=%s magazine_before=%d reserve_before=%d accepted=%s magazine_after=%d reserve_after=%d reloading=%s"),
        *GetName(),
        MagazineBefore,
        ReserveBefore,
        CurrentWeapon->IsReloading() ? TEXT("yes") : TEXT("no"),
        CurrentWeapon->GetMagazineAmmo(),
        CurrentWeapon->GetReserveAmmo(),
        CurrentWeapon->IsReloading() ? TEXT("yes") : TEXT("no"));
}

void ABMPlayerCharacter::RunCombatProbeReloadInterrupt()
{
    if (!IsLocallyControlled() || !CurrentWeapon)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat probe: client reload interrupt unavailable pawn=%s weapon=%s"),
            *GetName(),
            CurrentWeapon ? TEXT("yes") : TEXT("no"));
        return;
    }

    const bool bWasReloading = CurrentWeapon->IsReloading();
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian combat probe: client reload interrupt request pawn=%s reloading_before=%s"),
        *GetName(),
        bWasReloading ? TEXT("yes") : TEXT("no"));

    // Use the same fire action that a player uses. FireWeapon() owns the
    // gameplay rule that a shot input cancels an active reload, so this probe
    // does not bypass the real interruption path.
    if (bWasReloading)
    {
        StartFireInternal(true);
        StopFire();
        ProcessPendingFire();
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian combat probe: client reload interrupt result pawn=%s cancelled=%s reloading_after=%s magazine=%d reserve=%d"),
        *GetName(),
        bWasReloading && !CurrentWeapon->IsReloading() ? TEXT("yes") : TEXT("no"),
        CurrentWeapon->IsReloading() ? TEXT("yes") : TEXT("no"),
        CurrentWeapon->GetMagazineAmmo(),
        CurrentWeapon->GetReserveAmmo());
}
#endif

#if !UE_BUILD_SHIPPING
bool ABMPlayerCharacter::RunInteractionProbeHold(AActor* ExpectedTarget)
{
    if (!HasAuthority() || !ExpectedTarget)
    {
        return false;
    }

    // The probe has already placed the pawn at a readable approach point. Set
    // both control and actor rotation so the normal first-person camera trace
    // sees the expected mission actor exactly as it would after mouse-look.
    const FRotator LookRotation = (ExpectedTarget->GetActorLocation() - GetPawnViewLocation()).Rotation();
    if (AController* PlayerController = GetController())
    {
        PlayerController->SetControlRotation(LookRotation);
    }
    SetActorRotation(FRotator(0.0f, LookRotation.Yaw, 0.0f));
    // The normal camera tick mirrors controller rotation on the following
    // frame. The development verifier acquires and holds on this same frame,
    // so update the camera now to keep the interaction query on the same
    // target a real player just looked at.
    if (FirstPersonCamera)
    {
        FirstPersonCamera->SetWorldRotation(LookRotation);
    }

    FHitResult ProbeTraceHit;
    const FVector ProbeTraceStart = GetPawnViewLocation();
    const FVector ProbeTraceEnd = ProbeTraceStart + GetAimDirection() * 360.0f;
    FCollisionQueryParams ProbeTraceParams(SCENE_QUERY_STAT(BlackMeridianInteractionProbe), false, this);
    const bool bProbeTraceHit = GetWorld()->LineTraceSingleByChannel(
        ProbeTraceHit,
        ProbeTraceStart,
        ProbeTraceEnd,
        ECC_Visibility,
        ProbeTraceParams);
    AActor* ResolvedTarget = FindInteractionTarget();
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian interaction probe: target expected=%s resolved=%s trace_hit=%s trace_component=%s trace_location=%s trace_start=%s trace_end=%s distance=%.1f"),
        *GetNameSafe(ExpectedTarget),
        *GetNameSafe(ResolvedTarget),
        bProbeTraceHit ? *GetNameSafe(ProbeTraceHit.GetActor()) : TEXT("none"),
        bProbeTraceHit ? *GetNameSafe(ProbeTraceHit.GetComponent()) : TEXT("none"),
        bProbeTraceHit ? *ProbeTraceHit.GetActor()->GetActorLocation().ToString() : TEXT("none"),
        *ProbeTraceStart.ToString(),
        *ProbeTraceEnd.ToString(),
        FVector::Dist(GetActorLocation(), ExpectedTarget->GetActorLocation()));

    if (ResolvedTarget != ExpectedTarget)
    {
        return false;
    }

    // Interact() is the ordinary bound action. It starts the real hold timer;
    // CompleteHeldInteraction later performs the authoritative interaction.
    Interact();
    return IsHoldingInteraction();
}
#endif

void ABMPlayerCharacter::ResetFireInputState()
{
    bWantsToFire = false;
    bFireInputArmed = false;
    bPendingFire = false;
    bAutomaticFireBlockedUntilRelease = false;
    // A focus/menu transition can deliver a delayed press after the reset.
    // Ignore that stale event briefly, then accept the next genuine press.
    constexpr float FireInputGuardDuration = 0.25f;
    FireInputGuardRemaining = FMath::Max(FireInputGuardRemaining, FireInputGuardDuration);
    if (CurrentWeapon)
    {
        CurrentWeapon->StopFiring();
    }
}

void ABMPlayerCharacter::StartFire()
{
    StartFireInternal(false);
}

void ABMPlayerCharacter::StartFireInternal(const bool bBypassInputGuard)
{
    if (!bBypassInputGuard && FireInputGuardRemaining > 0.0f)
    {
        return;
    }

    bFireInputArmed = true;
    bWantsToFire = true;
    bPendingFire = true;

    // The automatic-fire target latch is evaluated in ProcessPendingFire(),
    // after the camera has been updated for this frame. Evaluating it here
    // made a held trigger use a pre-bob/pre-cue ray even though the HUD turned
    // red immediately afterward.
    bAutomaticFireBlockedUntilRelease = false;
}

bool ABMPlayerCharacter::ProcessPendingFire()
{
    if (!bPendingFire)
    {
        return false;
    }

    bPendingFire = false;

    if (CurrentWeapon
        && CurrentWeapon->GetWeaponData()
        && CurrentWeapon->GetWeaponData()->FireMode != EBMWeaponFireMode::SemiAutomatic
        && FirstPersonCamera
        && GetWorld())
    {
        // An automatic weapon must not turn an off-target trigger press into
        // a delayed target-acquisition system. Without this latch, the first
        // shot can miss into the world and a held trigger will keep firing
        // until an enemy later walks through the reticle. Resolve it only
        // after the camera update so the latch and the red HUD reticle share
        // the same rendered ray.
        UpdateLocalAimSnapshot();
        const ABMCombatCharacter* CrosshairTarget = HasLocalAimSnapshot()
            ? GetLocalAimTarget()
            : nullptr;
        bAutomaticFireBlockedUntilRelease = CrosshairTarget == nullptr;
#if !UE_BUILD_SHIPPING
        if (bAutomaticFireBlockedUntilRelease
            && FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcase")))
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian combat input: automatic fire latched until release reason=crosshair_not_on_live_enemy"));
        }
#endif
    }

    if (!bAutomaticFireBlockedUntilRelease)
    {
        FireWeapon();
    }
    return true;
}

void ABMPlayerCharacter::StopFire()
{
    bWantsToFire = false;
    bAutomaticFireBlockedUntilRelease = false;
    if (CurrentWeapon)
    {
        CurrentWeapon->StopFiring();
    }
}

void ABMPlayerCharacter::ReloadWeapon()
{
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_ReloadInputProbe")))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian reload input probe: Reload action binding invoked pawn=%s weapon=%s"),
            *GetNameSafe(this),
            CurrentWeapon ? TEXT("yes") : TEXT("no"));
    }
#endif
    if (CurrentWeapon)
    {
        CurrentWeapon->Reload();
    }
}

void ABMPlayerCharacter::SwitchWeapon()
{
    if (CurrentWeapon && CurrentWeapon->IsReloading())
    {
        CurrentWeapon->CancelReload();
    }

    if (Loadout.Num() < 2)
    {
        if (!HasAuthority() && CurrentWeapon)
        {
            EquipWeaponById(CurrentWeapon->GetWeaponId() == EBMWeaponId::ARX4 ? EBMWeaponId::Kestrel9 : EBMWeaponId::ARX4);
        }
        return;
    }

    const int32 CurrentIndex = Loadout.IndexOfByKey(CurrentWeapon);
    const int32 NextIndex = CurrentIndex == INDEX_NONE ? 0 : (CurrentIndex + 1) % Loadout.Num();
    EquipWeaponById(Loadout[NextIndex]->GetWeaponId());
}

void ABMPlayerCharacter::MeleeWeapon()
{
    const FVector Origin = FirstPersonCamera->GetComponentLocation();
    const FVector Direction = GetAimDirection();
    if (HasAuthority())
    {
        ServerMelee_Implementation(Origin, Direction);
    }
    else
    {
        ServerMelee(Origin, Direction);
    }
}

void ABMPlayerCharacter::ServerMelee_Implementation(
    const FVector_NetQuantize Origin,
    const FVector_NetQuantizeNormal Direction)
{
    if (IsSignalControlLocked() || !Direction.IsNormalized() || FVector::DistSquared(Origin, GetActorLocation()) > FMath::Square(180.0f))
    {
        return;
    }

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BlackMeridianMelee), false, this);
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 170.0f, ECC_Visibility, Params))
    {
        return;
    }

    if (UBMHealthComponent* Health = Hit.GetActor() ? Hit.GetActor()->FindComponentByClass<UBMHealthComponent>() : nullptr)
    {
        if (const ABMCombatCharacter* Target = Cast<ABMCombatCharacter>(Hit.GetActor()); Target && Target->GetTeam() == GetTeam())
        {
            const UBMGameUserSettings* Settings = GEngine ? Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
            if (!Settings || !Settings->bFriendlyFireEnabled)
            {
                return;
            }
        }

        FBMDamageContext Context;
        Context.RawDamage = 70.0f;
        Context.FinalDamage = 70.0f;
        Context.HitZone = EBMHitZone::Torso;
        Context.InstigatorActor = this;
        Context.bPlayerMeleeDamage = true;
        Health->ApplyDamage(70.0f, Context);
        if (ABMCombatCharacter* CombatTarget = Cast<ABMCombatCharacter>(Hit.GetActor()))
        {
            CombatTarget->MulticastHitReaction(Direction, Context.HitZone);
        }
    }
}

void ABMPlayerCharacter::ThrowFrag()
{
    if (bEliminated || bMountedWeapon || IsSignalControlLocked())
    {
        if (ABMPlayerController* BMController = Cast<ABMPlayerController>(GetController()))
        {
            const TCHAR* BlockedMessage = bEliminated
                ? TEXT("SQUAD LOST  //  PRESS ENTER TO RESTART")
                : (bMountedWeapon ? TEXT("EXIT MOUNT  //  PRESS E TO THROW") : TEXT("ROUND NOT LIVE  //  DEPLOY FIRST"));
            BMController->ShowSubtitle(FText::FromString(BlockedMessage), 1.15f);
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian throwable: player frag rejected state eliminated=%s mounted=%s locked=%s"),
            bEliminated ? TEXT("yes") : TEXT("no"),
            bMountedWeapon ? TEXT("yes") : TEXT("no"),
            IsSignalControlLocked() ? TEXT("yes") : TEXT("no"));
        return;
    }

    if (FragCount <= 0)
    {
        if (ABMPlayerController* BMController = Cast<ABMPlayerController>(GetController()))
        {
            BMController->ShowSubtitle(FText::FromString(TEXT("NO FRAG GRENADES  //  X USE VEIL")), 0.9f);
        }
        UE_LOG(LogTemp, Display, TEXT("Black Meridian throwable: player frag rejected inventory_empty"));
        return;
    }

    if (!FirstPersonCamera)
    {
        UE_LOG(LogTemp, Warning, TEXT("Black Meridian throwable: player frag rejected camera_unavailable"));
        return;
    }

    const int32 PredictedRemaining = FMath::Max(0, FragCount - 1);
    BeginThrowablePresentation(EBMWeaponId::FragGrenade);
    const FVector Origin = FirstPersonCamera->GetComponentLocation() + GetAimDirection() * 60.0f;
    if (HasAuthority())
    {
        ServerThrowThrowable_Implementation(EBMWeaponId::FragGrenade, Origin, GetAimDirection());
    }
    else
    {
        ServerThrowThrowable(EBMWeaponId::FragGrenade, Origin, GetAimDirection());
    }

    if (ABMPlayerController* BMController = Cast<ABMPlayerController>(GetController()))
    {
        BMController->ShowSubtitle(
            FText::FromString(FString::Printf(TEXT("FRAG OUT  //  %d REMAINING"), PredictedRemaining)),
            0.9f);
    }
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian throwable: player frag requested remaining_local=%d predicted_remaining=%d"),
        FragCount,
        PredictedRemaining);
}

void ABMPlayerCharacter::ThrowTactical()
{
    if (bEliminated || bMountedWeapon || IsSignalControlLocked())
    {
        if (ABMPlayerController* BMController = Cast<ABMPlayerController>(GetController()))
        {
            const TCHAR* BlockedMessage = bEliminated
                ? TEXT("SQUAD LOST  //  PRESS ENTER TO RESTART")
                : (bMountedWeapon ? TEXT("EXIT MOUNT  //  PRESS E TO THROW") : TEXT("ROUND NOT LIVE  //  DEPLOY FIRST"));
            BMController->ShowSubtitle(FText::FromString(BlockedMessage), 1.15f);
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian throwable: player veil rejected state eliminated=%s mounted=%s locked=%s"),
            bEliminated ? TEXT("yes") : TEXT("no"),
            bMountedWeapon ? TEXT("yes") : TEXT("no"),
            IsSignalControlLocked() ? TEXT("yes") : TEXT("no"));
        return;
    }

    if (VeilCount <= 0)
    {
        if (ABMPlayerController* BMController = Cast<ABMPlayerController>(GetController()))
        {
            BMController->ShowSubtitle(FText::FromString(TEXT("NO VEIL GRENADES  //  G USE FRAG")), 0.9f);
        }
        UE_LOG(LogTemp, Display, TEXT("Black Meridian throwable: player veil rejected inventory_empty"));
        return;
    }

    if (!FirstPersonCamera)
    {
        UE_LOG(LogTemp, Warning, TEXT("Black Meridian throwable: player veil rejected camera_unavailable"));
        return;
    }

    const int32 PredictedRemaining = FMath::Max(0, VeilCount - 1);
    BeginThrowablePresentation(EBMWeaponId::VeilGrenade);
    const FVector Origin = FirstPersonCamera->GetComponentLocation() + GetAimDirection() * 60.0f;
    if (HasAuthority())
    {
        ServerThrowThrowable_Implementation(EBMWeaponId::VeilGrenade, Origin, GetAimDirection());
    }
    else
    {
        ServerThrowThrowable(EBMWeaponId::VeilGrenade, Origin, GetAimDirection());
    }

    if (ABMPlayerController* BMController = Cast<ABMPlayerController>(GetController()))
    {
        BMController->ShowSubtitle(
            FText::FromString(FString::Printf(TEXT("VEIL OUT  //  %d REMAINING"), PredictedRemaining)),
            0.9f);
    }
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian throwable: player veil requested remaining_local=%d predicted_remaining=%d"),
        VeilCount,
        PredictedRemaining);
}

void ABMPlayerCharacter::ServerThrowThrowable_Implementation(
    const EBMWeaponId ThrowableId,
    const FVector_NetQuantize Origin,
    const FVector_NetQuantizeNormal Direction)
{
    if (IsSignalControlLocked() ||
        (ThrowableId != EBMWeaponId::FragGrenade && ThrowableId != EBMWeaponId::VeilGrenade) ||
        !Direction.IsNormalized() ||
        FVector::DistSquared(Origin, GetActorLocation()) > FMath::Square(220.0f))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Black Meridian throwable: server rejected id=%d locked=%s normalized=%s origin_distance=%.1f"),
            static_cast<int32>(ThrowableId),
            IsSignalControlLocked() ? TEXT("yes") : TEXT("no"),
            Direction.IsNormalized() ? TEXT("yes") : TEXT("no"),
            FVector::Dist(Origin, GetActorLocation()));
        return;
    }

    int32& RemainingCount = ThrowableId == EBMWeaponId::FragGrenade ? FragCount : VeilCount;
    if (RemainingCount <= 0)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian throwable: server rejected id=%d inventory_empty"),
            static_cast<int32>(ThrowableId));
        return;
    }

    const FTransform SpawnTransform(Direction.Rotation(), Origin);
    if (ABMThrowable* Throwable = GetWorld()->SpawnActorDeferred<ABMThrowable>(
            ABMThrowable::StaticClass(),
            SpawnTransform,
            this,
            this,
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
    {
        --RemainingCount;
        Throwable->ThrowableId = ThrowableId;
        Throwable->InitializeFromData(UBMThrowableDataAsset::LoadContentProfile(Throwable, ThrowableId));
        UGameplayStatics::FinishSpawningActor(Throwable, SpawnTransform);
        Throwable->ArmAndThrow(Direction, 0.0f);
        ForceNetUpdate();
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian throwable: server spawned id=%d remaining=%d origin=%s direction=%s"),
            static_cast<int32>(ThrowableId),
            RemainingCount,
            *Origin.ToString(),
            *Direction.ToString());
    }
    else
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Black Meridian throwable: server spawn failed id=%d"),
            static_cast<int32>(ThrowableId));
    }
}

void ABMPlayerCharacter::Interact()
{
    if (bOnLadder)
    {
        EndLadderClimb();
        return;
    }

    if (bMountedWeapon)
    {
        EndMountedWeapon();
        return;
    }

    AActor* Target = FindInteractionTarget();
    if (!Target)
    {
        return;
    }

    if (IBMInteractionInterface::Execute_GetInteractionType(Target) == EBMInteractionType::Hold)
    {
        PendingInteraction = Target;
        InteractionHoldElapsed = 0.0f;
        InteractionHoldDuration = FMath::Max(0.1f, IBMInteractionInterface::Execute_GetInteractionHoldDuration(Target));
        GetWorld()->GetTimerManager().SetTimer(
            InteractionHoldTimerHandle,
            this,
            &ABMPlayerCharacter::CompleteHeldInteraction,
            InteractionHoldDuration,
            false);
        return;
    }

    ExecuteInteraction(Target);
}

void ABMPlayerCharacter::StopInteract()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(InteractionHoldTimerHandle);
    }
    PendingInteraction.Reset();
    InteractionHoldElapsed = 0.0f;
    InteractionHoldDuration = 0.0f;
}

void ABMPlayerCharacter::CompleteHeldInteraction()
{
    AActor* Target = PendingInteraction.Get();
    PendingInteraction.Reset();
    InteractionHoldElapsed = 0.0f;
    InteractionHoldDuration = 0.0f;
    if (Target && IBMInteractionInterface::Execute_CanInteract(Target, this))
    {
        ExecuteInteraction(Target);
    }
}

float ABMPlayerCharacter::GetInteractionHoldProgress() const
{
    if (!PendingInteraction.IsValid() || InteractionHoldDuration <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }
    return FMath::Clamp(InteractionHoldElapsed / InteractionHoldDuration, 0.0f, 1.0f);
}

void ABMPlayerCharacter::ExecuteInteraction(AActor* Target)
{
    if (!Target || !Target->GetClass()->ImplementsInterface(UBMInteractionInterface::StaticClass()) ||
        !IBMInteractionInterface::Execute_CanInteract(Target, this))
    {
        return;
    }

    if (HasAuthority())
    {
        IBMInteractionInterface::Execute_Interact(Target, this);
    }
    else
    {
        ServerInteract(Target);
    }
}

void ABMPlayerCharacter::ServerInteract_Implementation(AActor* Target)
{
    if (IsSignalControlLocked() || !Target || !Target->GetClass()->ImplementsInterface(UBMInteractionInterface::StaticClass()) ||
        !IBMInteractionInterface::Execute_CanInteract(Target, this))
    {
        return;
    }

    const FVector Start = GetPawnViewLocation();
    const FVector End = Target->GetActorLocation();
    FHitResult VisibilityHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BlackMeridianServerInteraction), true, this);
    if (!GetWorld() || !GetWorld()->LineTraceSingleByChannel(VisibilityHit, Start, End, ECC_Visibility, QueryParams) ||
        VisibilityHit.GetActor() != Target)
    {
        return;
    }
    IBMInteractionInterface::Execute_Interact(Target, this);
}

void ABMPlayerCharacter::BeginLadderClimb(AActor* LadderActor)
{
    if (!LadderActor || !LadderActor->GetClass()->ImplementsInterface(UBMInteractionInterface::StaticClass()) ||
        IBMInteractionInterface::Execute_GetInteractionAction(LadderActor) != EBMInteractionAction::Ladder)
    {
        return;
    }

    if (!HasAuthority())
    {
        ServerBeginLadderClimb(LadderActor);
        return;
    }

    if (IsSignalControlLocked() || FVector::DistSquared(GetActorLocation(), LadderActor->GetActorLocation()) > FMath::Square(420.0f))
    {
        return;
    }

    ActiveLadder = LadderActor;
    bOnLadder = true;
    bSprinting = false;
    bTacticalSprinting = false;
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->GravityScale = 0.0f;
    GetCharacterMovement()->SetMovementMode(MOVE_Flying);
    PlayCharacterAnimation_Implementation(TEXT("Ladder"));
    SetActorRotation(FRotator(0.0f, LadderActor->GetActorRotation().Yaw, 0.0f));
    ForceNetUpdate();
}

void ABMPlayerCharacter::EndLadderClimb()
{
    if (!HasAuthority())
    {
        ServerEndLadderClimb();
        return;
    }

    bOnLadder = false;
    ActiveLadder.Reset();
    GetCharacterMovement()->GravityScale = 1.0f;
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    PlayCharacterAnimation_Implementation(TEXT("Land"));
    ForceNetUpdate();
}

void ABMPlayerCharacter::ServerBeginLadderClimb_Implementation(AActor* LadderActor)
{
    BeginLadderClimb(LadderActor);
}

void ABMPlayerCharacter::ServerEndLadderClimb_Implementation()
{
    EndLadderClimb();
}

void ABMPlayerCharacter::BeginMountedWeapon(AActor* MountedActor)
{
    if (!MountedActor || !MountedActor->GetClass()->ImplementsInterface(UBMInteractionInterface::StaticClass()) ||
        IBMInteractionInterface::Execute_GetInteractionAction(MountedActor) != EBMInteractionAction::MountedWeapon)
    {
        return;
    }

    if (!HasAuthority())
    {
        ServerBeginMountedWeapon(MountedActor);
        return;
    }

    if (IsSignalControlLocked() || bMountedWeapon || !IBMInteractionInterface::Execute_CanInteract(MountedActor, this) ||
        FVector::DistSquared(GetActorLocation(), MountedActor->GetActorLocation()) > FMath::Square(420.0f))
    {
        return;
    }

    ActiveMountedWeapon = MountedActor;
    bMountedWeapon = true;
    bAiming = true;
    bSprinting = false;
    bTacticalSprinting = false;
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    PlayCharacterAnimation_Implementation(TEXT("Mounted"));
    ForceNetUpdate();
}

void ABMPlayerCharacter::EndMountedWeapon()
{
    if (!HasAuthority())
    {
        ServerEndMountedWeapon();
        return;
    }

    bMountedWeapon = false;
    bAiming = false;
    ActiveMountedWeapon.Reset();
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    GetCharacterMovement()->MaxWalkSpeed = DefaultWalkSpeed;
    PlayCharacterAnimation_Implementation(TEXT("Land"));
    ForceNetUpdate();
}

void ABMPlayerCharacter::ServerBeginMountedWeapon_Implementation(AActor* MountedActor)
{
    BeginMountedWeapon(MountedActor);
}

void ABMPlayerCharacter::ServerEndMountedWeapon_Implementation()
{
    EndMountedWeapon();
}

void ABMPlayerCharacter::LeanLeft()
{
    TargetLeanAmount = -1.0f;
}

void ABMPlayerCharacter::LeanRight()
{
    TargetLeanAmount = 1.0f;
}

void ABMPlayerCharacter::ResetLean()
{
    TargetLeanAmount = 0.0f;
}

void ABMPlayerCharacter::UpdateCameraBob(const float DeltaSeconds)
{
    if (!FirstPersonCamera || !IsLocallyControlled())
    {
        return;
    }

    const float Speed = GetVelocity().Size2D();
    const bool bGrounded = GetCharacterMovement()->IsMovingOnGround();
    const float TargetStanceHeight = bProne ? 40.0f : (IsCrouched() ? 54.0f : 72.0f);
    CameraStanceHeight = FMath::FInterpTo(CameraStanceHeight, TargetStanceHeight, DeltaSeconds, bProne ? 10.0f : 16.0f);
    CameraLandingOffset = FMath::FInterpTo(CameraLandingOffset, 0.0f, DeltaSeconds, 11.0f);
    CameraLandingRoll = FMath::FInterpTo(CameraLandingRoll, 0.0f, DeltaSeconds, 13.0f);
    CameraDamageKick = FMath::FInterpTo(CameraDamageKick, 0.0f, DeltaSeconds, 12.0f);
    CameraDamageRoll = FMath::FInterpTo(CameraDamageRoll, 0.0f, DeltaSeconds, 14.0f);

    if (Speed > 20.0f && bGrounded)
    {
        CameraBobTime += DeltaSeconds * FMath::GetMappedRangeValueClamped(FVector2D(100.0f, 760.0f), FVector2D(5.0f, 10.0f), Speed);
        float BobAmplitude = 1.4f;
        if (const UBMGameUserSettings* Settings = Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()); Settings && Settings->bReducedCameraMotion)
        {
            BobAmplitude = 0.25f;
        }
        const float SpeedAlpha = FMath::Clamp(Speed / 760.0f, 0.0f, 1.0f);
        const float LateralBob = FMath::Lerp(0.35f, 1.15f, SpeedAlpha) * (bProne ? 0.35f : (IsCrouched() ? 0.65f : 1.0f));
        const float BobFrequency = bSprinting ? 1.12f : (IsCrouched() ? 0.88f : 1.0f);
        const FVector TargetLocation(
            FMath::Sin(CameraBobTime * 0.5f) * LateralBob,
            FMath::Cos(CameraBobTime) * LateralBob,
            CameraStanceHeight + FMath::Sin(CameraBobTime * 2.0f * BobFrequency) * BobAmplitude + CameraLandingOffset);
        FirstPersonCamera->SetRelativeLocation(FMath::VInterpTo(
            FirstPersonCamera->GetRelativeLocation(),
            TargetLocation,
            DeltaSeconds,
            18.0f));
    }
    else
    {
        FirstPersonCamera->SetRelativeLocation(FMath::VInterpTo(
            FirstPersonCamera->GetRelativeLocation(),
            FVector(0.0f, 0.0f, CameraStanceHeight + CameraLandingOffset),
            DeltaSeconds,
            12.0f));
    }
}

void ABMPlayerCharacter::UpdateCameraPresentation(const float DeltaSeconds)
{
    if (!FirstPersonCamera || !IsLocallyControlled())
    {
        return;
    }

    // A short FOV blend makes ADS and sprint transitions feel deliberate and
    // avoids the hard snap that made the source-only camera feel like a test
    // harness. The target values remain intentionally conservative so the
    // weapon and HUD stay readable on the Mac compatibility renderer.
    const float AttachmentADSFOVOffset = CurrentWeapon ? CurrentWeapon->GetAttachmentADSFOVOffset() : 0.0f;
    const float TargetFOV = bAiming
        ? FMath::Clamp(72.0f + AttachmentADSFOVOffset, 55.0f, 82.0f)
        : (bTacticalSprinting ? 99.0f : (bSprinting ? 95.0f : DefaultFOV));
    const float InterpSpeed = bAiming ? 13.0f : 9.0f;
    FirstPersonCamera->SetFieldOfView(FMath::FInterpTo(
        FirstPersonCamera->FieldOfView,
        TargetFOV,
        DeltaSeconds,
        InterpSpeed));

    LeanAmount = FMath::FInterpTo(LeanAmount, TargetLeanAmount, DeltaSeconds, 14.0f);
    // Keep camera roll as presentation only. Pitching the rendered camera on
    // damage changes the screen ray without changing controller rotation,
    // which is precisely the intermittent red-reticle/no-damage mismatch the
    // player sees. Weapon recoil and hit feedback still communicate impact.
    const FRotator TargetLeanRotation(0.0f, 0.0f, LeanAmount * 6.0f + CameraLandingRoll + CameraDamageRoll);
    FirstPersonCamera->SetRelativeRotation(FMath::RInterpTo(
        FirstPersonCamera->GetRelativeRotation(),
        TargetLeanRotation,
        DeltaSeconds,
        16.0f));
}

bool ABMPlayerCharacter::IsPlayableCameraLookLocked() const
{
    if (!CachedMissionDirector.IsValid() && GetWorld())
    {
        TActorIterator<ABMMissionDirector> Iterator(GetWorld());
        if (Iterator)
        {
            CachedMissionDirector = *Iterator;
        }
    }
    return CachedMissionDirector.IsValid() && CachedMissionDirector->IsPlayableSequenceLookLocked();
}

void ABMPlayerCharacter::ApplyPlayableCameraCue()
{
    if (!IsLocallyControlled() || !FirstPersonCamera || !GetWorld())
    {
        return;
    }

    if (!CachedMissionDirector.IsValid())
    {
        TActorIterator<ABMMissionDirector> Iterator(GetWorld());
        if (Iterator)
        {
            CachedMissionDirector = *Iterator;
        }
    }

    const bool bSequenceActive = CachedMissionDirector.IsValid() && CachedMissionDirector->IsPlayableSequenceActive();
    if (!bSequenceActive && !bHadPlayableCameraCue)
    {
        return;
    }

    const FVector BaseLocation(0.0f, 0.0f, CameraStanceHeight + CameraLandingOffset);
    const FRotator LeanRotation(0.0f, 0.0f, LeanAmount * 6.0f);
    FVector TargetLocation = BaseLocation;
    FRotator TargetRotation = LeanRotation;
    if (bSequenceActive)
    {
        const FBMPlayableCameraCue Cue = CachedMissionDirector->GetActivePlayableCameraCue();
        const float CueProgress = CachedMissionDirector->GetActivePlayableCameraCueProgress();
        const float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, CueProgress, 2.0f);
        TargetLocation += FMath::Lerp(Cue.StartCameraOffset, Cue.CameraOffset, EasedProgress);
        TargetRotation = FRotator(
            FMath::Lerp(Cue.StartCameraRotation.Pitch, Cue.CameraRotation.Pitch, EasedProgress),
            FMath::Lerp(Cue.StartCameraRotation.Yaw, Cue.CameraRotation.Yaw, EasedProgress),
            FMath::Lerp(Cue.StartCameraRotation.Roll, Cue.CameraRotation.Roll, EasedProgress)) + LeanRotation;
        bHadPlayableCameraCue = true;
    }
    else
    {
        bHadPlayableCameraCue = false;
    }

    FirstPersonCamera->SetRelativeLocation(FMath::VInterpTo(
        FirstPersonCamera->GetRelativeLocation(),
        TargetLocation,
        GetWorld()->GetDeltaSeconds(),
        bSequenceActive ? 6.0f : 10.0f));
    FirstPersonCamera->SetRelativeRotation(FMath::RInterpTo(
        FirstPersonCamera->GetRelativeRotation(),
        TargetRotation,
        GetWorld()->GetDeltaSeconds(),
        bSequenceActive ? 6.0f : 10.0f));
}

void ABMPlayerCharacter::UpdateSuppressionPresentation()
{
    if (!FirstPersonCamera || !IsLocallyControlled())
    {
        return;
    }

    const float SuppressionAlpha = SuppressionComponent
        ? FMath::Clamp(SuppressionComponent->GetSuppressionPercent(), 0.0f, 1.0f)
        : 0.0f;
    FirstPersonCamera->PostProcessSettings.bOverride_VignetteIntensity = SuppressionAlpha > KINDA_SMALL_NUMBER;
    FirstPersonCamera->PostProcessSettings.VignetteIntensity = FMath::Lerp(0.0f, 0.62f, SuppressionAlpha);
    FirstPersonCamera->PostProcessSettings.bOverride_ColorSaturation = SuppressionAlpha > KINDA_SMALL_NUMBER;
    FirstPersonCamera->PostProcessSettings.ColorSaturation = FVector4(
        FMath::Lerp(1.0f, 0.72f, SuppressionAlpha),
        FMath::Lerp(1.0f, 0.82f, SuppressionAlpha),
        FMath::Lerp(1.0f, 0.9f, SuppressionAlpha),
        1.0f);
    FirstPersonCamera->PostProcessBlendWeight = SuppressionAlpha;
}

void ABMPlayerCharacter::UpdateAudioPresentation(const float DeltaSeconds)
{
    if (!IsLocallyControlled() || !GetWorld())
    {
        return;
    }

    LowHealthAudioCooldown = FMath::Max(0.0f, LowHealthAudioCooldown - DeltaSeconds);
    const bool bSuppressed = SuppressionComponent && SuppressionComponent->IsSuppressed();
    if (bSuppressed && !bWasSuppressed)
    {
        if (UBMAudioDataAsset* Audio = AudioProfile.LoadSynchronous())
        {
            if (USoundBase* Sound = Audio->Suppression.LoadSynchronous())
            {
                UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
            }
            else if (!PlayGeneratedPlayerCue(this, GetActorLocation(), TEXT("BM_Suppression"), 0.46f, 0.78f))
            {
                PlayRuntimePlayerPulse(this, GetActorLocation(), ERuntimePlayerPulse::Suppression, EBMSurfaceType::Concrete, 0.28f, 0.72f);
            }
        }
        else
        {
            if (!PlayGeneratedPlayerCue(this, GetActorLocation(), TEXT("BM_Suppression"), 0.46f, 0.78f))
            {
                PlayRuntimePlayerPulse(this, GetActorLocation(), ERuntimePlayerPulse::Suppression, EBMSurfaceType::Concrete, 0.28f, 0.72f);
            }
        }
    }
    bWasSuppressed = bSuppressed;

    const float HealthPercent = HealthComponent ? HealthComponent->GetHealthPercent() : 1.0f;
    if (HealthPercent >= 0.4f)
    {
        LowHealthAudioCooldown = 0.0f;
    }
    else if (HealthPercent <= 0.3f && LowHealthAudioCooldown <= 0.0f)
    {
        bool bPlayedLowHealthSound = false;
        if (UBMAudioDataAsset* Audio = AudioProfile.LoadSynchronous())
        {
            if (USoundBase* Sound = Audio->LowHealth.LoadSynchronous())
            {
                UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
                bPlayedLowHealthSound = true;
            }
        }
        if (!bPlayedLowHealthSound)
        {
            if (!PlayGeneratedPlayerCue(this, GetActorLocation(), TEXT("BM_LowHealth"), 0.42f, 0.90f))
            {
                PlayRuntimePlayerPulse(this, GetActorLocation(), ERuntimePlayerPulse::LowHealth, EBMSurfaceType::Concrete, 0.25f, 0.88f);
            }
        }
        LowHealthAudioCooldown = 6.0f;
    }

    const float Speed = GetVelocity().Size2D();
    if (!GetCharacterMovement()->IsMovingOnGround() || Speed < 25.0f || bMountedWeapon)
    {
        FootstepDistance = 0.0f;
        return;
    }

    FootstepDistance += Speed * DeltaSeconds;
    const float StrideDistance = bProne ? 105.0f : (IsCrouched() ? 130.0f : (bSprinting ? 215.0f : 170.0f));
    if (FootstepDistance >= StrideDistance)
    {
        FootstepDistance = FMath::Fmod(FootstepDistance, StrideDistance);
        PlayFootstepCue();
    }
}

void ABMPlayerCharacter::UpdateThrowableThreatPresentation(const float DeltaSeconds)
{
    ThrowableThreatAudioCooldown = FMath::Max(0.0f, ThrowableThreatAudioCooldown - DeltaSeconds);

    if (!IsLocallyControlled() || bEliminated || !GetWorld())
    {
        bThrowableThreatActive = false;
        ThrowableThreatLocation = FVector::ZeroVector;
        ThrowableThreatDistance = 0.0f;
        return;
    }

    ABMThrowable* NearestThreat = nullptr;
    float NearestDistance = TNumericLimits<float>::Max();
    for (TActorIterator<ABMThrowable> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        ABMThrowable* Throwable = *Iterator;
        if (!Throwable
            || Throwable->ThrowableId != EBMWeaponId::FragGrenade
            || Throwable->IsDetonationPresentationActive())
        {
            continue;
        }

        const ABMCombatCharacter* Thrower = Cast<ABMCombatCharacter>(Throwable->GetOwner());
        if (!Thrower
            || Thrower->IsEliminated()
            || Thrower->GetTeam() == Team
            || Thrower->GetTeam() == EBMTeam::Neutral)
        {
            continue;
        }

        const float Distance = FVector::Dist(GetActorLocation(), Throwable->GetActorLocation());
        // Warn before the frag reaches its damage radius, while keeping the
        // cue local enough that a distant AI throw does not become noise.
        const float WarningRadius = FMath::Max(520.0f, Throwable->Radius + 260.0f);
        if (Distance <= WarningRadius && Distance < NearestDistance)
        {
            NearestThreat = Throwable;
            NearestDistance = Distance;
        }
    }

    const bool bNewThreatActive = NearestThreat != nullptr;
    if (bNewThreatActive != bThrowableThreatActive)
    {
#if !UE_BUILD_SHIPPING
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat: throwable threat state=%s distance=%.1f owner=%s"),
            bNewThreatActive ? TEXT("active") : TEXT("clear"),
            bNewThreatActive ? NearestDistance : 0.0f,
            bNewThreatActive ? *GetNameSafe(NearestThreat->GetOwner()) : TEXT("none"));
#endif
    }

    bThrowableThreatActive = bNewThreatActive;
    ThrowableThreatLocation = bNewThreatActive ? NearestThreat->GetActorLocation() : FVector::ZeroVector;
    ThrowableThreatDistance = bNewThreatActive ? NearestDistance : 0.0f;

    if (!bNewThreatActive || ThrowableThreatAudioCooldown > 0.0f)
    {
        return;
    }

    bool bPlayedAuthoredWarning = false;
    if (UBMAudioDataAsset* Audio = AudioProfile.LoadSynchronous())
    {
        if (USoundBase* Sound = Audio->ThrowableWarning.LoadSynchronous())
        {
            UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
            bPlayedAuthoredWarning = true;
        }
    }
    if (!bPlayedAuthoredWarning)
    {
        if (!PlayGeneratedPlayerCue(this, GetActorLocation(), TEXT("BM_ThrowableWarning"), 0.46f, 1.08f))
        {
            PlayRuntimePlayerPulse(
                this,
                GetActorLocation(),
                ERuntimePlayerPulse::ThrowableWarning,
                EBMSurfaceType::Concrete,
                0.28f,
                1.12f);
        }
    }

    const float ThreatProximity = FMath::Clamp(
        1.0f - (NearestDistance / FMath::Max(1.0f, NearestThreat->Radius + 260.0f)),
        0.0f,
        1.0f);
    ThrowableThreatAudioCooldown = FMath::Lerp(0.72f, 0.30f, ThreatProximity);
}

EBMSurfaceType ABMPlayerCharacter::ResolveMovementSurfaceType() const
{
    if (!GetWorld())
    {
        return EBMSurfaceType::Concrete;
    }

    const FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 24.0f);
    const FVector End = GetActorLocation() - FVector(0.0f, 0.0f, 120.0f);
    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BlackMeridianFootstepSurface), true, this);
    QueryParams.bReturnPhysicalMaterial = true;
    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams))
    {
        if (Hit.PhysMaterial.IsValid())
        {
            switch (UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get()))
            {
            case SurfaceType1:
                return EBMSurfaceType::Concrete;
            case SurfaceType2:
                return EBMSurfaceType::Metal;
            case SurfaceType3:
                return EBMSurfaceType::Wood;
            case SurfaceType4:
                return EBMSurfaceType::Glass;
            case SurfaceType5:
                return EBMSurfaceType::SoftCover;
            default:
                break;
            }
        }

        if (const AActor* SurfaceActor = Hit.GetActor())
        {
            if (SurfaceActor->ActorHasTag(TEXT("Surface.Metal")))
            {
                return EBMSurfaceType::Metal;
            }
            if (SurfaceActor->ActorHasTag(TEXT("Surface.Wood")))
            {
                return EBMSurfaceType::Wood;
            }
            if (SurfaceActor->ActorHasTag(TEXT("Surface.Glass")))
            {
                return EBMSurfaceType::Glass;
            }
            if (SurfaceActor->ActorHasTag(TEXT("ThinCover")) || SurfaceActor->ActorHasTag(TEXT("Surface.SoftCover")))
            {
                return EBMSurfaceType::SoftCover;
            }
        }
    }
    return EBMSurfaceType::Concrete;
}

void ABMPlayerCharacter::PlayFootstepCue()
{
    const EBMSurfaceType Surface = ResolveMovementSurfaceType();
    if (UBMAudioDataAsset* Audio = AudioProfile.LoadSynchronous())
    {
        if (USoundBase* Sound = Audio->FindFootstep(Surface).LoadSynchronous())
        {
            UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
            return;
        }
    }
    const TCHAR* GeneratedFootstep = Surface == EBMSurfaceType::Metal
        ? TEXT("BM_Footstep_Metal")
        : (Surface == EBMSurfaceType::SoftCover ? TEXT("BM_Footstep_SoftCover") : TEXT("BM_Footstep_Concrete"));
    if (!PlayGeneratedPlayerCue(
            this,
            GetActorLocation(),
            GeneratedFootstep,
            bSprinting ? 0.42f : 0.32f,
            bSprinting ? 0.86f : 0.76f))
    {
        PlayRuntimePlayerPulse(this, GetActorLocation(), ERuntimePlayerPulse::Footstep, Surface, bSprinting ? 0.32f : 0.24f, bSprinting ? 0.86f : 0.76f);
    }
}

void ABMPlayerCharacter::UpdateAnimationPresentation()
{
    const float GroundSpeed = GetVelocity().Size2D();
    EBMCharacterMovementState NewState = EBMCharacterMovementState::Idle;

    if (bEliminated)
    {
        NewState = EBMCharacterMovementState::Dead;
    }
    else if (bMountedWeapon)
    {
        NewState = EBMCharacterMovementState::Mounted;
    }
    else if (bOnLadder)
    {
        NewState = EBMCharacterMovementState::Ladder;
    }
    else if (GetCharacterMovement()->IsFalling())
    {
        NewState = GroundSpeed > 20.0f ? EBMCharacterMovementState::Jump : EBMCharacterMovementState::Fall;
    }
    else if (bTacticalSprinting)
    {
        NewState = EBMCharacterMovementState::TacticalSprint;
    }
    else if (bSprinting)
    {
        NewState = EBMCharacterMovementState::Sprint;
    }
    else if (bProne)
    {
        NewState = EBMCharacterMovementState::Prone;
    }
    else if (IsCrouched())
    {
        NewState = EBMCharacterMovementState::Crouch;
    }
    else if (GroundSpeed > 20.0f)
    {
        NewState = EBMCharacterMovementState::Walk;
    }

    SetCharacterAnimationState_Implementation(NewState, GroundSpeed, bAiming);
    if (Controller)
    {
        FRotator AimOffset = Controller->GetControlRotation() - GetActorRotation();
        AimOffset.Normalize();
        SetCharacterAimOffset_Implementation(AimOffset);
    }
}

void ABMPlayerCharacter::UpdateWeaponPresentation(const float DeltaSeconds)
{
    if (!CurrentWeapon || !CurrentWeapon->GetWeaponData())
    {
        LastVisualRecoil = FRotator::ZeroRotator;
        return;
    }

    CurrentWeapon->UpdatePresentation(DeltaSeconds);

    if (WeaponAnchor)
    {
        // Keep the weapon trace camera-authoritative, but let the local rig
        // move smoothly between hip fire, ADS, and sprint. This is the visual
        // layer that makes the input feel immediate without changing server
        // hit validation or projectile origin rules.
        const float TargetAimAlpha = bAiming ? 1.0f : 0.0f;
        const float TargetLowerAlpha = bTacticalSprinting ? 1.0f : (bSprinting ? 0.72f : 0.0f);
        WeaponAimAlpha = FMath::FInterpTo(WeaponAimAlpha, TargetAimAlpha, DeltaSeconds, bAiming ? 14.0f : 11.0f);
        WeaponLowerAlpha = FMath::FInterpTo(WeaponLowerAlpha, TargetLowerAlpha, DeltaSeconds, 10.0f);

        const FVector ADSLocationOffset(-2.0f, -9.5f, 7.0f);
        const FRotator ADSRotationOffset(-0.8f, 0.0f, 0.0f);
        const FVector SprintLocationOffset(-4.0f, 6.0f, -9.0f);
        const FRotator SprintRotationOffset(-4.0f, 0.0f, -5.0f);
        const FVector AimLocation = WeaponAnchorBaseLocation + ADSLocationOffset * WeaponAimAlpha;
        const FRotator AimRotation = WeaponAnchorBaseRotation + ADSRotationOffset * WeaponAimAlpha;
        const FVector TargetLocation = FMath::Lerp(AimLocation, WeaponAnchorBaseLocation + SprintLocationOffset, WeaponLowerAlpha);
        const FRotator TargetRotation = FMath::Lerp(AimRotation, WeaponAnchorBaseRotation + SprintRotationOffset, WeaponLowerAlpha);
        WeaponAnchor->SetRelativeLocation(FMath::VInterpTo(
            WeaponAnchor->GetRelativeLocation(),
            TargetLocation,
            DeltaSeconds,
            13.0f));
        WeaponAnchor->SetRelativeRotation(FMath::RInterpTo(
            WeaponAnchor->GetRelativeRotation(),
            TargetRotation,
            DeltaSeconds,
            13.0f));
    }

    const FBMWeaponBalance& Balance = CurrentWeapon->GetWeaponData()->Balance;
    const UBMGameUserSettings* Settings = GEngine ? Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
    const float MotionScale = Settings && Settings->bReducedCameraMotion ? 0.25f : 1.0f;
    const float AimScale = bAiming ? Balance.ADSSwayMultiplier : 1.0f;
    const float MovementScale = FMath::Clamp(GetVelocity().Size2D() / 620.0f, 0.0f, 1.0f);
    const float SwayScale = Balance.WeaponSwayAmount * AimScale * MotionScale * (0.35f + MovementScale * 0.65f);
    WeaponSwayTime += DeltaSeconds * FMath::Max(0.0f, Balance.WeaponSwayFrequency);

    const FVector LocationOffset(
        FMath::Sin(WeaponSwayTime * 0.85f) * SwayScale * 0.55f,
        FMath::Cos(WeaponSwayTime) * SwayScale * 0.7f,
        FMath::Sin(WeaponSwayTime * 0.5f) * SwayScale * 0.3f);
    const FRotator RecoilOffset = CurrentWeapon->ConsumeVisualRecoil(DeltaSeconds);
    const FRotator RecoilDelta = RecoilOffset - LastVisualRecoil;
    if (IsLocallyControlled() && Controller)
    {
        // Apply only the change in the spring value to the view. This gives
        // a sharp kick and a natural return without accumulating the entire
        // spring offset into control rotation every frame.
        AddControllerPitchInput(RecoilDelta.Pitch * MotionScale);
        AddControllerYawInput(RecoilDelta.Yaw * MotionScale * 0.35f);
    }
    LastVisualRecoil = RecoilOffset;

    FRotator RotationOffset = RecoilOffset;
    RotationOffset.Pitch = -RotationOffset.Pitch;
    RotationOffset.Yaw += FMath::Sin(WeaponSwayTime * 0.85f) * SwayScale * 0.45f;
    RotationOffset.Roll += FMath::Cos(WeaponSwayTime) * SwayScale * 0.35f;
    CurrentWeapon->SetFirstPersonPresentation(LocationOffset, RotationOffset);
    UpdateFallbackArmsPresentation(LocationOffset, RotationOffset, CurrentWeapon->GetPresentationReloadAlpha());
    UpdateFirstPersonAnimationPresentation(
        DeltaSeconds,
        LocationOffset,
        RotationOffset,
        CurrentWeapon->GetPresentationReloadAlpha());
}

void ABMPlayerCharacter::ConfigureThrowablePresentation()
{
    if (!ThrowablePresentationMesh || bThrowablePresentationConfigured)
    {
        return;
    }

    const UBMThrowableDataAsset* Profile = UBMThrowableDataAsset::LoadContentProfile(
        this,
        EBMWeaponId::FragGrenade);
    if (Profile && !Profile->Mesh.IsNull())
    {
        if (UStaticMesh* ThrowableMesh = Profile->Mesh.LoadSynchronous())
        {
            ThrowablePresentationMesh->SetStaticMesh(ThrowableMesh);
            ThrowablePresentationMesh->SetRelativeScale3D(FVector(0.88f));
        }
    }

    ThrowablePresentationMesh->SetVisibility(false, true);
    bThrowablePresentationConfigured = true;
    ApplyThrowablePresentationMaterial();
}

void ABMPlayerCharacter::BeginThrowablePresentation(const EBMWeaponId InThrowableId)
{
    if (!IsLocallyControlled() || !ThrowablePresentationMesh)
    {
        return;
    }

    ConfigureThrowablePresentation();
    ThrowablePresentationId = InThrowableId;
    if (const UBMThrowableDataAsset* Profile = UBMThrowableDataAsset::LoadContentProfile(this, InThrowableId))
    {
        if (!Profile->Mesh.IsNull())
        {
            if (UStaticMesh* ThrowableMesh = Profile->Mesh.LoadSynchronous())
            {
                ThrowablePresentationMesh->SetStaticMesh(ThrowableMesh);
            }
        }
    }

    ApplyThrowablePresentationMaterial();
    ThrowablePresentationElapsed = 0.0f;
    ThrowablePresentationRemaining = FParse::Param(
        FCommandLine::Get(),
        TEXT("BM_ThrowablePresentationShowcase"))
        ? 5.0f
        : 0.54f;
    ThrowablePresentationMesh->SetVisibility(true, true);

    if (CurrentWeapon && !bWeaponHiddenForThrowable)
    {
        CurrentWeapon->SetActorHiddenInGame(true);
        bWeaponHiddenForThrowable = true;
    }

    PlayCharacterAnimation_Implementation(
        InThrowableId == EBMWeaponId::VeilGrenade ? TEXT("ThrowTactical") : TEXT("ThrowFrag"));
}

void ABMPlayerCharacter::UpdateThrowablePresentation(const float DeltaSeconds)
{
    if (!ThrowablePresentationMesh)
    {
        return;
    }

    if (!bThrowablePresentationConfigured)
    {
        ConfigureThrowablePresentation();
    }

    if (ThrowablePresentationRemaining <= 0.0f)
    {
        if (ThrowablePresentationMesh->IsVisible())
        {
            ThrowablePresentationMesh->SetVisibility(false, true);
        }
        if (bWeaponHiddenForThrowable)
        {
            if (CurrentWeapon)
            {
                CurrentWeapon->SetActorHiddenInGame(false);
            }
            bWeaponHiddenForThrowable = false;
        }
        return;
    }

    ThrowablePresentationElapsed += FMath::Max(0.0f, DeltaSeconds);
    ThrowablePresentationRemaining = FMath::Max(
        0.0f,
        ThrowablePresentationRemaining - FMath::Max(0.0f, DeltaSeconds));

    const float LinearAlpha = FMath::Clamp(ThrowablePresentationElapsed / 0.54f, 0.0f, 1.0f);
    const float EaseAlpha = LinearAlpha * LinearAlpha * (3.0f - 2.0f * LinearAlpha);
    const FVector HandStart(16.0f, -20.0f, -21.0f);
    const FVector HandRelease(31.0f, -12.0f, -3.0f);
    const FVector Location = FMath::Lerp(HandStart, HandRelease, EaseAlpha)
        + FVector(0.0f, 0.0f, FMath::Sin(LinearAlpha * PI) * 4.0f);
    const FRotator Rotation(
        FMath::Lerp(-18.0f, 22.0f, EaseAlpha),
        FMath::Lerp(-12.0f, 10.0f, EaseAlpha),
        FMath::Lerp(24.0f, -40.0f, EaseAlpha) + LinearAlpha * 220.0f);
    ThrowablePresentationMesh->SetRelativeLocation(Location);
    ThrowablePresentationMesh->SetRelativeRotation(Rotation);

    if (ThrowablePresentationRemaining <= 0.0f)
    {
        ThrowablePresentationMesh->SetVisibility(false, true);
        if (CurrentWeapon)
        {
            CurrentWeapon->SetActorHiddenInGame(false);
        }
        bWeaponHiddenForThrowable = false;
    }
}

void ABMPlayerCharacter::ApplyThrowablePresentationMaterial()
{
    if (!ThrowablePresentationMesh)
    {
        return;
    }

    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
        nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!BaseMaterial)
    {
        BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr,
            TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
    }
    if (!BaseMaterial)
    {
        return;
    }

    if (UMaterialInstanceDynamic* Material = ThrowablePresentationMesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMaterial))
    {
        const bool bTactical = ThrowablePresentationId == EBMWeaponId::VeilGrenade;
        const FLinearColor BaseColor = bTactical
            ? FLinearColor(0.015f, 0.26f, 0.55f, 1.0f)
            : FLinearColor(0.52f, 0.055f, 0.012f, 1.0f);
        const FLinearColor EmissiveColor = bTactical
            ? FLinearColor(0.01f, 0.12f, 0.36f, 1.0f)
            : FLinearColor(0.18f, 0.012f, 0.002f, 1.0f);
        Material->SetVectorParameterValue(TEXT("Color"), BaseColor);
        Material->SetVectorParameterValue(TEXT("BaseColor"), BaseColor);
        Material->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveColor);
        Material->SetScalarParameterValue(TEXT("Metallic"), 0.58f);
        Material->SetScalarParameterValue(TEXT("Roughness"), 0.34f);
    }
}

void ABMPlayerCharacter::RecoverReplicatedWeaponState()
{
    if (HasAuthority() || !GetWorld())
    {
        return;
    }

    TArray<ABMWeapon*> OwnedWeapons;
    for (TActorIterator<ABMWeapon> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        ABMWeapon* Weapon = *Iterator;
        if (IsValid(Weapon) && Weapon->GetOwner() == this)
        {
            OwnedWeapons.Add(Weapon);
        }
    }

    if (OwnedWeapons.Num() == 0)
    {
        return;
    }

    OwnedWeapons.Sort([](const ABMWeapon& Left, const ABMWeapon& Right)
    {
        return static_cast<uint8>(Left.GetWeaponId()) < static_cast<uint8>(Right.GetWeaponId());
    });

    bool bChanged = false;
    for (ABMWeapon* Weapon : OwnedWeapons)
    {
        if (!Loadout.Contains(Weapon))
        {
            Loadout.Add(Weapon);
            Weapon->OnWeaponEvent.AddUniqueDynamic(this, &ABMPlayerCharacter::HandleWeaponEvent);
            bChanged = true;
        }
    }

    if (!CurrentWeapon || !OwnedWeapons.Contains(CurrentWeapon))
    {
        CurrentWeapon = OwnedWeapons[0];
        bChanged = true;
    }

    if (bChanged)
    {
        AttachCurrentWeapon();
        if (!bClientWeaponStateRecovered)
        {
            bClientWeaponStateRecovered = true;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian combat: reconciled replicated client loadout pawn=%s weapons=%d current=%s"),
                *GetName(),
                Loadout.Num(),
                *GetNameSafe(CurrentWeapon));
        }
    }
}

void ABMPlayerCharacter::ConfigureFallbackArms()
{
    if (!FallbackLeftArm || !FallbackRightArm)
    {
        return;
    }

    const bool bUsingAuthoredArms = FirstPersonArms && FirstPersonArms->IsVisible();
    const TArray<UStaticMeshComponent*> FallbackComponents = {
        FallbackLeftArm.Get(),
        FallbackRightArm.Get(),
        FallbackLeftHand.Get(),
        FallbackRightHand.Get(),
        FallbackLeftCuff.Get(),
        FallbackRightCuff.Get(),
        FallbackLeftGlovePanel.Get(),
        FallbackRightGlovePanel.Get(),
        FallbackLeftKnucklePlate.Get(),
        FallbackRightKnucklePlate.Get()};
    for (UStaticMeshComponent* Component : FallbackComponents)
    {
        if (Component)
        {
            Component->SetVisibility(!bUsingAuthoredArms, true);
        }
    }

    // The project does not yet ship a dedicated arms-only skeletal asset.
    // Build a deliberate first-person silhouette from the same low-poly kit
    // used by the project weapon: cylinders form the forearms, spheres form
    // the gloves, and small plates/cuffs establish material and scale cues.
    // The endpoints are chosen against the weapon's authored grip and
    // handguard locations so the hands remain attached during recoil and
    // reload instead of hovering as four unrelated primitives.
    const auto AlignSegment = [](UStaticMeshComponent* Component, const FVector& Start, const FVector& End, const float Radius)
    {
        if (!Component)
        {
            return;
        }
        const FVector Delta = End - Start;
        const float Length = FMath::Max(1.0f, Delta.Size());
        Component->SetRelativeLocation((Start + End) * 0.5f);
        Component->SetRelativeRotation(FRotationMatrix::MakeFromZ(Delta / Length).Rotator());
        Component->SetRelativeScale3D(FVector(Radius, Radius, Length / 100.0f));
    };
    const FVector LeftElbow(0.0f, -21.0f, -42.0f);
    const FVector LeftGrip(40.0f, 8.0f, -19.0f);
    const FVector RightElbow(-7.0f, 27.0f, -40.0f);
    const FVector RightGrip(19.0f, 8.0f, -20.0f);
    const FVector LeftDirection = (LeftGrip - LeftElbow).GetSafeNormal();
    const FVector RightDirection = (RightGrip - RightElbow).GetSafeNormal();
    const FRotator LeftArmRotation = FRotationMatrix::MakeFromZ(LeftDirection).Rotator();
    const FRotator RightArmRotation = FRotationMatrix::MakeFromZ(RightDirection).Rotator();

    AlignSegment(FallbackLeftArm, LeftElbow, LeftGrip, 0.115f);
    AlignSegment(FallbackRightArm, RightElbow, RightGrip, 0.125f);
    FallbackLeftHand->SetRelativeLocation(LeftGrip);
    FallbackLeftHand->SetRelativeRotation(LeftArmRotation);
    FallbackLeftHand->SetRelativeScale3D(FVector(0.125f, 0.102f, 0.092f));
    FallbackRightHand->SetRelativeLocation(RightGrip);
    FallbackRightHand->SetRelativeRotation(RightArmRotation);
    FallbackRightHand->SetRelativeScale3D(FVector(0.132f, 0.108f, 0.096f));

    if (FallbackLeftCuff)
    {
        FallbackLeftCuff->SetRelativeLocation(LeftGrip - LeftDirection * 3.0f);
        FallbackLeftCuff->SetRelativeRotation(LeftArmRotation);
        FallbackLeftCuff->SetRelativeScale3D(FVector(0.145f, 0.145f, 0.045f));
    }
    if (FallbackRightCuff)
    {
        FallbackRightCuff->SetRelativeLocation(RightGrip - RightDirection * 3.0f);
        FallbackRightCuff->SetRelativeRotation(RightArmRotation);
        FallbackRightCuff->SetRelativeScale3D(FVector(0.15f, 0.15f, 0.045f));
    }

    const auto ConfigureGlovePanel = [](UStaticMeshComponent* Component, const FVector& Grip, const FRotator& Rotation, const float YawOffset)
    {
        if (!Component)
        {
            return;
        }
        Component->SetRelativeLocation(Grip + FVector(-1.0f, 0.0f, 5.0f));
        Component->SetRelativeRotation(Rotation + FRotator(0.0f, YawOffset, 0.0f));
        Component->SetRelativeScale3D(FVector(0.062f, 0.086f, 0.022f));
    };
    ConfigureGlovePanel(FallbackLeftGlovePanel, LeftGrip, LeftArmRotation, -12.0f);
    ConfigureGlovePanel(FallbackRightGlovePanel, RightGrip, RightArmRotation, 10.0f);
    ConfigureGlovePanel(FallbackLeftKnucklePlate, LeftGrip + FVector(2.0f, 0.0f, 1.5f), LeftArmRotation, -12.0f);
    ConfigureGlovePanel(FallbackRightKnucklePlate, RightGrip + FVector(2.0f, 0.0f, 1.5f), RightArmRotation, 10.0f);

    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!BaseMaterial)
    {
        BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
    }
    if (!BaseMaterial)
    {
        return;
    }

    const auto ApplySleeveMaterial = [BaseMaterial](UStaticMeshComponent* Arm, const FLinearColor& Color, const float Metallic, const float Roughness)
    {
        if (UMaterialInstanceDynamic* Material = Arm ? Arm->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMaterial) : nullptr)
        {
            Material->SetVectorParameterValue(TEXT("Color"), Color);
            Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
            Material->SetVectorParameterValue(TEXT("EmissiveColor"), Color * 0.018f);
            Material->SetScalarParameterValue(TEXT("Metallic"), Metallic);
            Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
        }
    };
    // Keep the tactical blue sleeves and warm gloves above the shadow floor;
    // graphite cuffs and desaturated plates separate the silhouette from the
    // rifle without turning the hands into another bright HUD element.
    ApplySleeveMaterial(FallbackLeftArm, FLinearColor(0.16f, 0.23f, 0.28f), 0.25f, 0.78f);
    ApplySleeveMaterial(FallbackRightArm, FLinearColor(0.19f, 0.27f, 0.32f), 0.25f, 0.78f);
    ApplySleeveMaterial(FallbackLeftHand, FLinearColor(0.36f, 0.19f, 0.105f), 0.12f, 0.88f);
    ApplySleeveMaterial(FallbackRightHand, FLinearColor(0.42f, 0.23f, 0.13f), 0.12f, 0.88f);
    ApplySleeveMaterial(FallbackLeftCuff, FLinearColor(0.055f, 0.075f, 0.09f), 0.48f, 0.56f);
    ApplySleeveMaterial(FallbackRightCuff, FLinearColor(0.065f, 0.085f, 0.10f), 0.48f, 0.56f);
    ApplySleeveMaterial(FallbackLeftGlovePanel, FLinearColor(0.10f, 0.12f, 0.13f), 0.36f, 0.62f);
    ApplySleeveMaterial(FallbackRightGlovePanel, FLinearColor(0.12f, 0.14f, 0.15f), 0.36f, 0.62f);
    ApplySleeveMaterial(FallbackLeftKnucklePlate, FLinearColor(0.26f, 0.29f, 0.29f), 0.54f, 0.46f);
    ApplySleeveMaterial(FallbackRightKnucklePlate, FLinearColor(0.29f, 0.31f, 0.31f), 0.54f, 0.46f);

    FallbackLeftArmBaseLocation = FallbackLeftArm->GetRelativeLocation();
    FallbackRightArmBaseLocation = FallbackRightArm->GetRelativeLocation();
    FallbackLeftArmBaseRotation = FallbackLeftArm->GetRelativeRotation();
    FallbackRightArmBaseRotation = FallbackRightArm->GetRelativeRotation();
    FallbackLeftHandBaseLocation = FallbackLeftHand->GetRelativeLocation();
    FallbackRightHandBaseLocation = FallbackRightHand->GetRelativeLocation();
    FallbackLeftHandBaseRotation = FallbackLeftHand->GetRelativeRotation();
    FallbackRightHandBaseRotation = FallbackRightHand->GetRelativeRotation();
    FallbackLeftCuffBaseLocation = FallbackLeftCuff ? FallbackLeftCuff->GetRelativeLocation() : FVector::ZeroVector;
    FallbackRightCuffBaseLocation = FallbackRightCuff ? FallbackRightCuff->GetRelativeLocation() : FVector::ZeroVector;
    FallbackLeftCuffBaseRotation = FallbackLeftCuff ? FallbackLeftCuff->GetRelativeRotation() : FRotator::ZeroRotator;
    FallbackRightCuffBaseRotation = FallbackRightCuff ? FallbackRightCuff->GetRelativeRotation() : FRotator::ZeroRotator;
    FallbackLeftGlovePanelBaseLocation = FallbackLeftGlovePanel ? FallbackLeftGlovePanel->GetRelativeLocation() : FVector::ZeroVector;
    FallbackRightGlovePanelBaseLocation = FallbackRightGlovePanel ? FallbackRightGlovePanel->GetRelativeLocation() : FVector::ZeroVector;
    FallbackLeftGlovePanelBaseRotation = FallbackLeftGlovePanel ? FallbackLeftGlovePanel->GetRelativeRotation() : FRotator::ZeroRotator;
    FallbackRightGlovePanelBaseRotation = FallbackRightGlovePanel ? FallbackRightGlovePanel->GetRelativeRotation() : FRotator::ZeroRotator;
    FallbackLeftKnucklePlateBaseLocation = FallbackLeftKnucklePlate ? FallbackLeftKnucklePlate->GetRelativeLocation() : FVector::ZeroVector;
    FallbackRightKnucklePlateBaseLocation = FallbackRightKnucklePlate ? FallbackRightKnucklePlate->GetRelativeLocation() : FVector::ZeroVector;
    FallbackLeftKnucklePlateBaseRotation = FallbackLeftKnucklePlate ? FallbackLeftKnucklePlate->GetRelativeRotation() : FRotator::ZeroRotator;
    FallbackRightKnucklePlateBaseRotation = FallbackRightKnucklePlate ? FallbackRightKnucklePlate->GetRelativeRotation() : FRotator::ZeroRotator;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian presentation: first-person fallback kit active pawn=%s authored_arms=%s components=%d route=tactical_sleeve_glove"),
        *GetNameSafe(this),
        bUsingAuthoredArms ? TEXT("yes") : TEXT("no"),
        FallbackComponents.Num());
}

void ABMPlayerCharacter::UpdateFallbackArmsPresentation(
    const FVector& LocationOffset,
    const FRotator& RotationOffset,
    const float ReloadAlpha)
{
    if (!FallbackLeftArm || !FallbackRightArm || !FallbackLeftArm->IsVisible())
    {
        return;
    }

    const float ReloadCurve = FMath::Sin(PI * FMath::Clamp(ReloadAlpha, 0.0f, 1.0f));
    const FVector SharedOffset = LocationOffset * 0.8f + FVector(-7.0f * ReloadCurve, 0.0f, -16.0f * ReloadCurve);
    const FRotator SharedRotation(
        RotationOffset.Pitch * 0.42f,
        RotationOffset.Yaw * 0.42f,
        RotationOffset.Roll * 0.42f);
    const FVector HandOffset = LocationOffset * 0.95f + FVector(-8.0f * ReloadCurve, 0.0f, -11.0f * ReloadCurve);
    const FRotator HandRotation(
        RotationOffset.Pitch * 0.58f,
        RotationOffset.Yaw * 0.58f,
        RotationOffset.Roll * 0.58f);
    FallbackLeftArm->SetRelativeLocation(FallbackLeftArmBaseLocation + SharedOffset);
    FallbackRightArm->SetRelativeLocation(FallbackRightArmBaseLocation + SharedOffset);
    FallbackLeftArm->SetRelativeRotation(FallbackLeftArmBaseRotation + SharedRotation);
    FallbackRightArm->SetRelativeRotation(FallbackRightArmBaseRotation + SharedRotation);
    FallbackLeftHand->SetRelativeLocation(FallbackLeftHandBaseLocation + HandOffset);
    FallbackRightHand->SetRelativeLocation(FallbackRightHandBaseLocation + HandOffset);
    FallbackLeftHand->SetRelativeRotation(FallbackLeftHandBaseRotation + HandRotation);
    FallbackRightHand->SetRelativeRotation(FallbackRightHandBaseRotation + HandRotation);
    if (FallbackLeftCuff)
    {
        FallbackLeftCuff->SetRelativeLocation(FallbackLeftCuffBaseLocation + SharedOffset);
        FallbackLeftCuff->SetRelativeRotation(FallbackLeftCuffBaseRotation + SharedRotation);
    }
    if (FallbackRightCuff)
    {
        FallbackRightCuff->SetRelativeLocation(FallbackRightCuffBaseLocation + SharedOffset);
        FallbackRightCuff->SetRelativeRotation(FallbackRightCuffBaseRotation + SharedRotation);
    }
    if (FallbackLeftGlovePanel)
    {
        FallbackLeftGlovePanel->SetRelativeLocation(FallbackLeftGlovePanelBaseLocation + HandOffset);
        FallbackLeftGlovePanel->SetRelativeRotation(FallbackLeftGlovePanelBaseRotation + HandRotation);
    }
    if (FallbackRightGlovePanel)
    {
        FallbackRightGlovePanel->SetRelativeLocation(FallbackRightGlovePanelBaseLocation + HandOffset);
        FallbackRightGlovePanel->SetRelativeRotation(FallbackRightGlovePanelBaseRotation + HandRotation);
    }
    if (FallbackLeftKnucklePlate)
    {
        FallbackLeftKnucklePlate->SetRelativeLocation(FallbackLeftKnucklePlateBaseLocation + HandOffset);
        FallbackLeftKnucklePlate->SetRelativeRotation(FallbackLeftKnucklePlateBaseRotation + HandRotation);
    }
    if (FallbackRightKnucklePlate)
    {
        FallbackRightKnucklePlate->SetRelativeLocation(FallbackRightKnucklePlateBaseLocation + HandOffset);
        FallbackRightKnucklePlate->SetRelativeRotation(FallbackRightKnucklePlateBaseRotation + HandRotation);
    }
}

void ABMPlayerCharacter::AttachCurrentWeapon()
{
    LastVisualRecoil = FRotator::ZeroRotator;
    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors, true);
    for (AActor* AttachedActor : AttachedActors)
    {
        if (ABMWeapon* AttachedWeapon = Cast<ABMWeapon>(AttachedActor))
        {
            AttachedWeapon->SetActorHiddenInGame(true);
        }
    }
    if (CurrentWeapon && Loadout.Num() == 0)
    {
        CurrentWeapon->SetThirdPersonPresentation(!IsLocallyControlled());
        CurrentWeapon->AttachToComponent(WeaponAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        CurrentWeapon->SetActorHiddenInGame(false);
        return;
    }
    for (ABMWeapon* Weapon : Loadout)
    {
        if (Weapon)
        {
            // AI pawns use the same weapon actor as the local player. Keep
            // their weapon readable to other clients while retaining the
            // owner-only first-person presentation for the human player.
            Weapon->SetThirdPersonPresentation(!IsLocallyControlled());
            Weapon->AttachToComponent(WeaponAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
            Weapon->SetActorHiddenInGame(Weapon != CurrentWeapon);
        }
    }
}

void ABMPlayerCharacter::SpawnLoadout()
{
    const ABMGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABMGameMode>() : nullptr;
    const TArray<EBMWeaponId> WeaponIds = GameMode ? GameMode->GetProfileLoadout() : TArray<EBMWeaponId>{EBMWeaponId::ARX4, EBMWeaponId::Kestrel9};
    for (const EBMWeaponId WeaponId : WeaponIds)
    {
        ABMWeapon* Weapon = SpawnWeaponById(WeaponId);
        if (!Weapon)
        {
            continue;
        }
        Loadout.Add(Weapon);
    }

    if (Loadout.Num() > 0)
    {
        CurrentWeapon = Loadout[0];
        AttachCurrentWeapon();
    }
}

ABMWeapon* ABMPlayerCharacter::SpawnWeaponById(const EBMWeaponId WeaponId)
{
    if (!HasAuthority() || !GetWorld())
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.Instigator = this;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ABMWeapon* Weapon = GetWorld()->SpawnActor<ABMWeapon>(ABMWeapon::StaticClass(), GetActorTransform(), SpawnParameters);
    if (!Weapon)
    {
        return nullptr;
    }
    Weapon->InitializeFromData(UBMWeaponDataAsset::LoadContentProfile(Weapon, WeaponId));
    Weapon->OnWeaponEvent.AddUniqueDynamic(this, &ABMPlayerCharacter::HandleWeaponEvent);
    return Weapon;
}

bool ABMPlayerCharacter::AddWeaponToLoadout(const EBMWeaponId WeaponId)
{
    if (WeaponId == EBMWeaponId::FragGrenade || WeaponId == EBMWeaponId::VeilGrenade)
    {
        return false;
    }

    if (!HasAuthority())
    {
        ServerAddWeaponToLoadout(WeaponId);
        return false;
    }

    for (ABMWeapon* Weapon : Loadout)
    {
        if (Weapon && Weapon->GetWeaponId() == WeaponId)
        {
            CurrentWeapon = Weapon;
            AttachCurrentWeapon();
            return true;
        }
    }

    ABMWeapon* NewWeapon = SpawnWeaponById(WeaponId);
    if (!NewWeapon)
    {
        return false;
    }
    Loadout.Add(NewWeapon);
    if (!CurrentWeapon)
    {
        CurrentWeapon = NewWeapon;
    }
    AttachCurrentWeapon();
    ForceNetUpdate();
    return true;
}

bool ABMPlayerCharacter::AddReserveAmmo(const int32 Amount)
{
    if (!HasAuthority() || Amount <= 0 || !CurrentWeapon)
    {
        return false;
    }
    CurrentWeapon->AddReserveAmmo(Amount);
    return true;
}

bool ABMPlayerCharacter::AddAmmoToWeapon(const EBMWeaponId WeaponId, const int32 Amount)
{
    if (!HasAuthority() || Amount <= 0)
    {
        return false;
    }

    for (ABMWeapon* Weapon : Loadout)
    {
        if (Weapon && Weapon->GetWeaponId() == WeaponId)
        {
            Weapon->AddReserveAmmo(Amount);
            return true;
        }
    }
    return false;
}

bool ABMPlayerCharacter::EquipAttachmentById(const FName AttachmentId)
{
    if (AttachmentId.IsNone())
    {
        return false;
    }
    if (!HasAuthority())
    {
        ServerEquipAttachmentById(AttachmentId);
        return false;
    }
    return CurrentWeapon && CurrentWeapon->EquipAttachmentById(AttachmentId);
}

void ABMPlayerCharacter::RestoreThrowableInventory(const int32 InFragCount, const int32 InVeilCount)
{
    if (!HasAuthority())
    {
        return;
    }

    FragCount = FMath::Max(0, InFragCount);
    VeilCount = FMath::Max(0, InVeilCount);
    ForceNetUpdate();
}

bool ABMPlayerCharacter::AddThrowableInventory(const int32 InFragCount, const int32 InVeilCount)
{
    if (!HasAuthority() || (InFragCount <= 0 && InVeilCount <= 0))
    {
        return false;
    }

    const int32 PreviousFragCount = FragCount;
    const int32 PreviousVeilCount = VeilCount;
    FragCount = FMath::Clamp(
        FragCount + FMath::Max(0, InFragCount),
        0,
        FMath::Max(0, MaxFragCount));
    VeilCount = FMath::Clamp(
        VeilCount + FMath::Max(0, InVeilCount),
        0,
        FMath::Max(0, MaxVeilCount));

    const bool bChanged = FragCount != PreviousFragCount || VeilCount != PreviousVeilCount;
    if (bChanged)
    {
        ForceNetUpdate();
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian throwable: ordnance resupplied frag=%d->%d veil=%d->%d"),
            PreviousFragCount,
            FragCount,
            PreviousVeilCount,
            VeilCount);
    }
    return bChanged;
}

TArray<EBMWeaponId> ABMPlayerCharacter::GetLoadoutWeaponIds() const
{
    TArray<EBMWeaponId> WeaponIds;
    for (const ABMWeapon* Weapon : Loadout)
    {
        if (Weapon)
        {
            WeaponIds.Add(Weapon->GetWeaponId());
        }
    }
    return WeaponIds;
}

EBMWeaponId ABMPlayerCharacter::GetCurrentWeaponId() const
{
    return CurrentWeapon ? CurrentWeapon->GetWeaponId() : EBMWeaponId::ARX4;
}

bool ABMPlayerCharacter::RestoreLoadoutFromIds(const TArray<EBMWeaponId>& WeaponIds, const EBMWeaponId InCurrentWeaponId)
{
    if (!HasAuthority())
    {
        return false;
    }

    for (ABMWeapon* Weapon : Loadout)
    {
        if (Weapon)
        {
            Weapon->Destroy();
        }
    }
    Loadout.Reset();
    CurrentWeapon = nullptr;

    for (const EBMWeaponId WeaponId : WeaponIds)
    {
        if (WeaponId == EBMWeaponId::FragGrenade || WeaponId == EBMWeaponId::VeilGrenade)
        {
            continue;
        }
        if (Loadout.ContainsByPredicate([WeaponId](const TObjectPtr<ABMWeapon>& ExistingWeapon)
        {
            return ExistingWeapon && ExistingWeapon->GetWeaponId() == WeaponId;
        }))
        {
            continue;
        }

        if (ABMWeapon* Weapon = SpawnWeaponById(WeaponId))
        {
            Loadout.Add(Weapon);
        }
    }

    if (Loadout.Num() == 0)
    {
        SpawnLoadout();
    }

    for (ABMWeapon* Weapon : Loadout)
    {
        if (Weapon && Weapon->GetWeaponId() == InCurrentWeaponId)
        {
            CurrentWeapon = Weapon;
            break;
        }
    }
    if (!CurrentWeapon && Loadout.Num() > 0)
    {
        CurrentWeapon = Loadout[0];
    }
    AttachCurrentWeapon();
    ForceNetUpdate();
    return Loadout.Num() > 0;
}

void ABMPlayerCharacter::RestoreForCheckpoint(const float InHealth)
{
    Super::RestoreForCheckpoint(InHealth);
    if (bMountedWeapon)
    {
        EndMountedWeapon();
    }
    bOnLadder = false;
    ActiveLadder.Reset();
    GetCharacterMovement()->GravityScale = 1.0f;
    PlaceholderBody->SetVisibility(false, true);
}

void ABMPlayerCharacter::EquipWeaponById(const EBMWeaponId WeaponId)
{
    if (!HasAuthority())
    {
        ServerEquipWeapon(WeaponId);
        return;
    }

    for (ABMWeapon* Weapon : Loadout)
    {
        if (Weapon && Weapon->GetWeaponId() == WeaponId)
        {
            CurrentWeapon = Weapon;
            AttachCurrentWeapon();
            ForceNetUpdate();
            return;
        }
    }
}

void ABMPlayerCharacter::ServerEquipWeapon_Implementation(const EBMWeaponId WeaponId)
{
    EquipWeaponById(WeaponId);
}

void ABMPlayerCharacter::ServerAddWeaponToLoadout_Implementation(const EBMWeaponId WeaponId)
{
    AddWeaponToLoadout(WeaponId);
}

void ABMPlayerCharacter::ServerEquipAttachmentById_Implementation(const FName AttachmentId)
{
    EquipAttachmentById(AttachmentId);
}

void ABMPlayerCharacter::ServerSetAimState_Implementation(const bool bNewAiming)
{
    if (bEliminated || bMountedWeapon)
    {
        return;
    }

    bAiming = bNewAiming;
    if (bAiming)
    {
        bSprinting = false;
        bTacticalSprinting = false;
        GetCharacterMovement()->MaxWalkSpeed = 285.0f;
    }
    else
    {
        GetCharacterMovement()->MaxWalkSpeed = bTacticalSprinting
            ? 760.0f
            : (bSprinting ? 620.0f : (bProne ? 100.0f : (IsCrouched() ? GetCharacterMovement()->MaxWalkSpeedCrouched : DefaultWalkSpeed)));
    }
    ForceNetUpdate();
}

void ABMPlayerCharacter::ServerSetSprintState_Implementation(
    const bool bNewSprinting,
    const bool bNewTacticalSprinting)
{
    if (bEliminated || bMountedWeapon)
    {
        return;
    }

    bSprinting = bNewSprinting;
    bTacticalSprinting = bNewSprinting && bNewTacticalSprinting;
    if (bSprinting)
    {
        bAiming = false;
    }
    GetCharacterMovement()->MaxWalkSpeed = bTacticalSprinting
        ? 760.0f
        : (bSprinting ? 620.0f : (bProne ? 100.0f : (IsCrouched() ? GetCharacterMovement()->MaxWalkSpeedCrouched : DefaultWalkSpeed)));
    ForceNetUpdate();
}

void ABMPlayerCharacter::ServerSetCrouchState_Implementation(const bool bNewCrouched)
{
    if (bEliminated || bMountedWeapon || bProne)
    {
        return;
    }

    if (bNewCrouched)
    {
        bSprinting = false;
        bTacticalSprinting = false;
        bAiming = false;
        Crouch();
        GetCharacterMovement()->MaxWalkSpeed = GetCharacterMovement()->MaxWalkSpeedCrouched;
    }
    else
    {
        UnCrouch();
        GetCharacterMovement()->MaxWalkSpeed = bSprinting ? 620.0f : DefaultWalkSpeed;
    }
    ForceNetUpdate();
}

void ABMPlayerCharacter::ServerSetProneState_Implementation(const bool bNewProne)
{
    if (bEliminated || bMountedWeapon)
    {
        return;
    }

    bProne = bNewProne;
    if (bProne)
    {
        bSprinting = false;
        bTacticalSprinting = false;
        bAiming = false;
        Crouch();
        GetCharacterMovement()->MaxWalkSpeed = 100.0f;
    }
    else
    {
        UnCrouch();
        GetCharacterMovement()->MaxWalkSpeed = DefaultWalkSpeed;
    }
    ForceNetUpdate();
}

void ABMPlayerCharacter::OnRep_CurrentWeapon()
{
    if (CurrentWeapon)
    {
        CurrentWeapon->OnWeaponEvent.AddUniqueDynamic(this, &ABMPlayerCharacter::HandleWeaponEvent);
    }
    AttachCurrentWeapon();
}

void ABMPlayerCharacter::HandleWeaponEvent(const FName EventName)
{
    if (EventName == TEXT("FirePredicted"))
    {
        // The owning client gets the fire montage immediately; the later
        // authoritative multicast supplies the confirmed hit/impact state.
        ApplyFirstPersonAnimationEvent(TEXT("Fire"));
        PlayCharacterAnimation_Implementation(TEXT("Fire"));
    }
    else if (!(EventName == TEXT("Fire") && CurrentWeapon && CurrentWeapon->WasFireRecentlyPredicted()))
    {
        // Do not restart a miss animation when the authoritative response
        // arrives after the predicted first-person fire event.
        ApplyFirstPersonAnimationEvent(EventName);
        PlayCharacterAnimation_Implementation(EventName);
    }
    if (EventName == TEXT("Hit") || EventName == TEXT("HeadshotHit"))
    {
        if (ABMPlayerController* BMController = Cast<ABMPlayerController>(GetController()))
        {
            BMController->NotifyHitMarker(EventName == TEXT("HeadshotHit"));
        }
    }
    else if (EventName == TEXT("DryFire"))
    {
        if (ABMPlayerController* BMController = Cast<ABMPlayerController>(GetController()))
        {
            BMController->NotifyDryFire();
        }
    }
}

void ABMPlayerCharacter::HandleLocalHealthChanged(const float /*NewHealth*/, const float DamageTaken)
{
    if (!IsLocallyControlled() || DamageTaken <= 0.0f)
    {
        return;
    }

    const float Strength = FMath::Clamp(DamageTaken / 42.0f, 0.28f, 1.0f);
    CameraDamageKick = FMath::Clamp(CameraDamageKick + FMath::Lerp(0.8f, 2.6f, Strength), 0.0f, 6.0f);
    CameraDamageRoll = FMath::Clamp(
        CameraDamageRoll + (FMath::RandBool() ? 1.0f : -1.0f) * FMath::Lerp(0.55f, 2.0f, Strength),
        -5.0f,
        5.0f);
    if (ABMPlayerController* BMController = Cast<ABMPlayerController>(GetController()))
    {
        BMController->ShowDamageFeedback(Strength);
    }
    FirstPersonDamagePulse = FMath::Max(FirstPersonDamagePulse, Strength);
}

void ABMPlayerCharacter::InitializeFirstPersonAnimationPresentation()
{
    if (!FirstPersonArms || !FirstPersonArms->IsVisible() || !FirstPersonArms->GetSkeletalMeshAsset())
    {
        return;
    }

    // These clips are the same project-owned rifle set used by the authored
    // enemy body. Loading them here gives the local player a real pose for
    // fire/reload/dry-fire while preserving the existing procedural camera,
    // weapon, and recoil layers.
    FirstPersonRifleIdleAnimation = LoadObject<UAnimSequence>(
        nullptr,
        TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS.MF_Rifle_Idle_ADS"));
    FirstPersonRifleFireAnimation = LoadObject<UAnimSequence>(
        nullptr,
        TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Fire.MM_Rifle_Fire"));
    FirstPersonRifleReloadAnimation = LoadObject<UAnimSequence>(
        nullptr,
        TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload.MM_Rifle_Reload"));
    FirstPersonRifleDryFireAnimation = LoadObject<UAnimSequence>(
        nullptr,
        TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_DryFire.MM_Rifle_DryFire"));
    FirstPersonAnimBlueprintClass = FirstPersonAnimBlueprint.IsNull()
        ? nullptr
        : FirstPersonAnimBlueprint.LoadSynchronous();
    if (!FirstPersonAnimBlueprintClass)
    {
        // A class soft reference can be evaluated before an optional engine
        // plugin finishes mounting its content in an installed client. Retry
        // through the object path so the local player gets the same AnimBP
        // that the enemy path already resolves in packaged builds.
        FirstPersonAnimBlueprintClass = LoadClass<UAnimInstance>(
            nullptr,
            TEXT("/MoverExamples/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C"));
    }

    bUsingDevelopmentFirstPersonAnimation = FirstPersonRifleIdleAnimation
        && FirstPersonRifleFireAnimation
        && FirstPersonRifleReloadAnimation
        && FirstPersonRifleDryFireAnimation;
    if (!bUsingDevelopmentFirstPersonAnimation)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Black Meridian presentation: first-person rifle animation unavailable pawn=%s mesh=%s idle=%s fire=%s reload=%s dry_fire=%s"),
            *GetNameSafe(this),
            *GetNameSafe(FirstPersonArms->GetSkeletalMeshAsset()),
            *GetNameSafe(FirstPersonRifleIdleAnimation),
            *GetNameSafe(FirstPersonRifleFireAnimation),
            *GetNameSafe(FirstPersonRifleReloadAnimation),
            *GetNameSafe(FirstPersonRifleDryFireAnimation));
        return;
    }

    // Start in a stable two-handed rifle pose. Additive fire is layered over
    // this pose only for its short event window; non-additive reload and dry
    // fire temporarily replace it and then return here.
    FirstPersonArms->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    FirstPersonArms->PlayAnimation(FirstPersonRifleIdleAnimation, true);
    FirstPersonArms->SetPlayRate(1.0f);
    ActiveFirstPersonAnimation = FirstPersonRifleIdleAnimation;
    FirstPersonEventRemaining = 0.0f;
    bFirstPersonAnimationBlueprintActive = false;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian presentation: first-person rifle animation active pawn=%s mesh=%s idle=%s fire=%s additive=%s reload=%s dry_fire=%s anim_bp=%s"),
        *GetNameSafe(this),
        *GetNameSafe(FirstPersonArms->GetSkeletalMeshAsset()),
        *GetNameSafe(FirstPersonRifleIdleAnimation),
        *GetNameSafe(FirstPersonRifleFireAnimation),
        FirstPersonRifleFireAnimation->IsValidAdditive() ? TEXT("yes") : TEXT("no"),
        *GetNameSafe(FirstPersonRifleReloadAnimation),
        *GetNameSafe(FirstPersonRifleDryFireAnimation),
        *GetNameSafe(FirstPersonAnimBlueprintClass));
}

void ABMPlayerCharacter::RestoreFirstPersonSingleNodeLocomotion()
{
    if (!FirstPersonArms || !bUsingDevelopmentFirstPersonAnimation)
    {
        FirstPersonEventRemaining = 0.0f;
        ActiveFirstPersonAnimation = nullptr;
        return;
    }

    static const FName AdditiveEventSlot(TEXT("DefaultSlot"));
    if (bFirstPersonAnimationBlueprintActive)
    {
        if (UAnimInstance* AnimInstance = FirstPersonArms->GetAnimInstance())
        {
            AnimInstance->StopSlotAnimation(0.06f, AdditiveEventSlot);
        }
    }

    FirstPersonArms->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    bFirstPersonAnimationBlueprintActive = false;
    FirstPersonEventRemaining = 0.0f;
    FirstPersonArms->SetPlayRate(1.0f);
    if (FirstPersonRifleIdleAnimation)
    {
        FirstPersonArms->PlayAnimation(FirstPersonRifleIdleAnimation, true);
        ActiveFirstPersonAnimation = FirstPersonRifleIdleAnimation;
    }
    else
    {
        ActiveFirstPersonAnimation = nullptr;
    }
}

void ABMPlayerCharacter::PlayFirstPersonAnimationEvent(const FName AnimationEvent)
{
    if (!IsLocallyControlled()
        || !FirstPersonArms
        || !FirstPersonArms->IsVisible()
        || !bUsingDevelopmentFirstPersonAnimation)
    {
        return;
    }

    if (AnimationEvent == TEXT("ReloadCompleted") || AnimationEvent == TEXT("ReloadCancelled"))
    {
        RestoreFirstPersonSingleNodeLocomotion();
        return;
    }

    UAnimSequence* EventAnimation = nullptr;
    float EventDuration = 0.0f;
    if (AnimationEvent == TEXT("Fire")
        || AnimationEvent == TEXT("FirePredicted")
        || AnimationEvent == TEXT("Hit")
        || AnimationEvent == TEXT("HeadshotHit"))
    {
        EventAnimation = FirstPersonRifleFireAnimation;
        EventDuration = EventAnimation
            ? FMath::Clamp(EventAnimation->GetPlayLength(), 0.12f, 0.30f)
            : 0.0f;
    }
    else if (AnimationEvent == TEXT("ReloadStarted"))
    {
        EventAnimation = FirstPersonRifleReloadAnimation;
        const float ReloadDuration = CurrentWeapon && CurrentWeapon->GetWeaponData()
            ? (CurrentWeapon->GetMagazineAmmo() <= 0
                ? CurrentWeapon->GetWeaponData()->Balance.EmptyReloadDuration
                : CurrentWeapon->GetWeaponData()->Balance.ReloadDuration)
            : 2.0f;
        EventDuration = EventAnimation ? FMath::Max(0.35f, ReloadDuration) : 0.0f;
    }
    else if (AnimationEvent == TEXT("DryFire"))
    {
        EventAnimation = FirstPersonRifleDryFireAnimation;
        EventDuration = EventAnimation
            ? FMath::Clamp(EventAnimation->GetPlayLength(), 0.16f, 0.32f)
            : 0.0f;
    }

    if (!EventAnimation || EventDuration <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const float SequenceLength = FMath::Max(KINDA_SMALL_NUMBER, EventAnimation->GetPlayLength());
    const float PlayRate = SequenceLength / EventDuration;
    const bool bAdditiveAnimation = EventAnimation->IsValidAdditive();
    if (bAdditiveAnimation)
    {
        static const FName AdditiveEventSlot(TEXT("DefaultSlot"));
        if (!FirstPersonAnimBlueprintClass)
        {
            if (FirstPersonRifleDryFireAnimation && !FirstPersonRifleDryFireAnimation->IsValidAdditive())
            {
                // The authored fire clip is additive and needs an AnimBP slot.
                // Keep the package visually authored even on an installation
                // where the optional MoverExamples AnimBP is unavailable by
                // using the compatible non-additive trigger clip; procedural
                // recoil still supplies the weapon kick on top.
                const float FallbackSequenceLength = FMath::Max(
                    KINDA_SMALL_NUMBER,
                    FirstPersonRifleDryFireAnimation->GetPlayLength());
                const float FallbackPlayRate = FallbackSequenceLength / EventDuration;
                RestoreFirstPersonSingleNodeLocomotion();
                FirstPersonArms->SetAnimationMode(EAnimationMode::AnimationSingleNode);
                FirstPersonArms->PlayAnimation(FirstPersonRifleDryFireAnimation, false);
                FirstPersonArms->SetPlayRate(FallbackPlayRate);
                ActiveFirstPersonAnimation = FirstPersonRifleDryFireAnimation;
                FirstPersonEventRemaining = EventDuration;
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian presentation: first-person authored animation event=%s asset=%s duration=%.2f play_rate=%.2f mode=single_node fallback_from=%s"),
                    *AnimationEvent.ToString(),
                    *GetNameSafe(FirstPersonRifleDryFireAnimation),
                    EventDuration,
                    FallbackPlayRate,
                    *GetNameSafe(EventAnimation));
                return;
            }
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Black Meridian presentation: first-person animation fallback event=%s asset=%s reason=no_anim_blueprint"),
                *AnimationEvent.ToString(),
                *GetNameSafe(EventAnimation));
            return;
        }

        if (!bFirstPersonAnimationBlueprintActive)
        {
            FirstPersonArms->SetAnimationMode(EAnimationMode::AnimationBlueprint);
            FirstPersonArms->SetAnimInstanceClass(FirstPersonAnimBlueprintClass);
            bFirstPersonAnimationBlueprintActive = true;
        }

        if (UAnimInstance* AnimInstance = FirstPersonArms->GetAnimInstance())
        {
            AnimInstance->StopSlotAnimation(0.03f, AdditiveEventSlot);
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
                ActiveFirstPersonAnimation = EventAnimation;
                FirstPersonEventRemaining = EventDuration;
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian presentation: first-person authored animation event=%s asset=%s duration=%.2f play_rate=%.2f mode=montage additive=yes slot=%s montage=%s"),
                    *AnimationEvent.ToString(),
                    *GetNameSafe(EventAnimation),
                    EventDuration,
                    PlayRate,
                    *AdditiveEventSlot.ToString(),
                    *GetNameSafe(DynamicMontage));
                return;
            }
        }

        RestoreFirstPersonSingleNodeLocomotion();
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Black Meridian presentation: first-person animation fallback event=%s asset=%s reason=montage_unavailable"),
            *AnimationEvent.ToString(),
            *GetNameSafe(EventAnimation));
        return;
    }

    RestoreFirstPersonSingleNodeLocomotion();
    FirstPersonArms->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    FirstPersonArms->PlayAnimation(EventAnimation, false);
    FirstPersonArms->SetPlayRate(PlayRate);
    ActiveFirstPersonAnimation = EventAnimation;
    FirstPersonEventRemaining = EventDuration;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian presentation: first-person authored animation event=%s asset=%s duration=%.2f play_rate=%.2f mode=single_node additive=no"),
        *AnimationEvent.ToString(),
        *GetNameSafe(EventAnimation),
        EventDuration,
        PlayRate);
}

void ABMPlayerCharacter::PlayCharacterAnimation_Implementation(const FName AnimationEvent)
{
    Super::PlayCharacterAnimation_Implementation(AnimationEvent);

    if (!AnimationData)
    {
        PlayFirstPersonAnimationEvent(AnimationEvent);
        return;
    }

    if (!FirstPersonArms || !AnimationData)
    {
        return;
    }

    const TSoftObjectPtr<UAnimMontage> MontageReference = AnimationData->FindEventMontage(AnimationEvent);
    if (!MontageReference.IsNull())
    {
        if (UAnimMontage* Montage = MontageReference.LoadSynchronous())
        {
            if (UAnimInstance* AnimInstance = FirstPersonArms->GetAnimInstance())
            {
                AnimInstance->Montage_Play(Montage);
            }
        }
    }
}

void ABMPlayerCharacter::ApplyFirstPersonAnimationEvent(const FName AnimationEvent)
{
    if (AnimationEvent == TEXT("Fire") || AnimationEvent == TEXT("FirePredicted"))
    {
        FirstPersonFirePulse = FMath::Max(FirstPersonFirePulse, 1.0f);
    }
    else if (AnimationEvent == TEXT("ReloadStarted"))
    {
        FirstPersonReloadPulse = FMath::Max(FirstPersonReloadPulse, 1.0f);
    }
    else if (AnimationEvent == TEXT("ReloadCompleted") || AnimationEvent == TEXT("ReloadCancelled"))
    {
        FirstPersonReloadReturnPulse = FMath::Max(FirstPersonReloadReturnPulse, 1.0f);
    }
    else if (AnimationEvent == TEXT("DryFire"))
    {
        FirstPersonDryFirePulse = FMath::Max(FirstPersonDryFirePulse, 1.0f);
    }

#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe"))
        || FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcaseAutoFire")))
    {
        const TCHAR* AuthoredRoute = AnimationData
            ? TEXT("montage")
            : (bUsingDevelopmentFirstPersonAnimation ? TEXT("sequence") : TEXT("none"));
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian presentation: first-person animation event=%s authored_route=%s procedural_recoil=yes"),
            *AnimationEvent.ToString(),
            AuthoredRoute);
    }
#endif
}

void ABMPlayerCharacter::UpdateFirstPersonAnimationPresentation(
    const float DeltaSeconds,
    const FVector& LocationOffset,
    const FRotator& RotationOffset,
    const float ReloadAlpha)
{
    if (!IsLocallyControlled())
    {
        return;
    }

    const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
    if (FirstPersonEventRemaining > 0.0f)
    {
        FirstPersonEventRemaining = FMath::Max(0.0f, FirstPersonEventRemaining - SafeDeltaSeconds);
        if (FirstPersonEventRemaining <= 0.0f)
        {
            RestoreFirstPersonSingleNodeLocomotion();
        }
    }
    FirstPersonFirePulse = FMath::FInterpTo(FirstPersonFirePulse, 0.0f, SafeDeltaSeconds, 25.0f);
    FirstPersonReloadPulse = FMath::FInterpTo(FirstPersonReloadPulse, 0.0f, SafeDeltaSeconds, 7.0f);
    FirstPersonReloadReturnPulse = FMath::FInterpTo(FirstPersonReloadReturnPulse, 0.0f, SafeDeltaSeconds, 18.0f);
    FirstPersonDryFirePulse = FMath::FInterpTo(FirstPersonDryFirePulse, 0.0f, SafeDeltaSeconds, 19.0f);
    FirstPersonDamagePulse = FMath::FInterpTo(FirstPersonDamagePulse, 0.0f, SafeDeltaSeconds, 11.0f);

    const float ReloadCurve = FMath::Sin(PI * FMath::Clamp(ReloadAlpha, 0.0f, 1.0f));
    const float FireCurve = FMath::Clamp(FirstPersonFirePulse, 0.0f, 1.0f);
    const float ReloadStartCurve = FMath::Clamp(FirstPersonReloadPulse, 0.0f, 1.0f);
    const float ReloadReturnCurve = FMath::Clamp(FirstPersonReloadReturnPulse, 0.0f, 1.0f);
    const float DryFireCurve = FMath::Clamp(FirstPersonDryFirePulse, 0.0f, 1.0f);
    const float DamageCurve = FMath::Clamp(FirstPersonDamagePulse, 0.0f, 1.0f);

    // The authored arms component receives the same weapon sway/recoil as the
    // first-person weapon, plus small event offsets. This keeps a future
    // montage-driven rig coherent with the procedural weapon even when its
    // AnimBP does not own the weapon root.
    FVector EventLocationOffset = FVector::ZeroVector;
    EventLocationOffset += FVector(-2.8f, 0.0f, 1.6f) * FireCurve;
    EventLocationOffset += FVector(-4.0f, 0.0f, -5.0f) * ReloadCurve;
    EventLocationOffset += FVector(-1.0f, 0.0f, -2.0f) * ReloadStartCurve;
    EventLocationOffset += FVector(0.0f, 0.0f, 2.8f) * ReloadReturnCurve;
    EventLocationOffset += FVector(-1.7f, 0.0f, -0.8f) * DryFireCurve;
    EventLocationOffset += FVector(-1.2f, 0.0f, 0.6f) * DamageCurve;

    FRotator EventRotationOffset = FRotator::ZeroRotator;
    EventRotationOffset += FRotator(-7.0f, 1.5f, -2.0f) * FireCurve;
    EventRotationOffset += FRotator(11.0f, 0.0f, -7.0f) * ReloadCurve;
    EventRotationOffset += FRotator(4.0f, 0.0f, -3.0f) * ReloadStartCurve;
    EventRotationOffset += FRotator(-8.0f, 0.0f, 3.0f) * ReloadReturnCurve;
    EventRotationOffset += FRotator(0.0f, 0.0f, 8.0f) * DryFireCurve;
    EventRotationOffset += FRotator(2.5f, -2.0f, 2.0f) * DamageCurve;

    if (FirstPersonArms && FirstPersonArms->IsVisible())
    {
        FirstPersonArms->SetRelativeLocation(FirstPersonArmsBaseLocation + LocationOffset + EventLocationOffset);
        FirstPersonArms->SetRelativeRotation(FirstPersonArmsBaseRotation + RotationOffset + EventRotationOffset);
    }

    if (FallbackLeftArm && FallbackLeftArm->IsVisible())
    {
        // UpdateFallbackArmsPresentation() establishes the base pose for the
        // current frame. Apply the event layer from that same base instead of
        // adding to the component's current transform; adding here every tick
        // makes recoil/reload pulses accumulate and eventually pulls the hands
        // away from the weapon.
        const float FallbackReloadCurve = FMath::Sin(PI * FMath::Clamp(ReloadAlpha, 0.0f, 1.0f));
        const FVector SharedFallbackOffset = LocationOffset * 0.8f
            + FVector(-7.0f * FallbackReloadCurve, 0.0f, -16.0f * FallbackReloadCurve);
        const FVector HandFallbackOffset = LocationOffset * 0.95f
            + FVector(-8.0f * FallbackReloadCurve, 0.0f, -11.0f * FallbackReloadCurve);
        const FRotator SharedFallbackRotation(
            RotationOffset.Pitch * 0.42f,
            RotationOffset.Yaw * 0.42f,
            RotationOffset.Roll * 0.42f);
        const FRotator HandFallbackRotation(
            RotationOffset.Pitch * 0.58f,
            RotationOffset.Yaw * 0.58f,
            RotationOffset.Roll * 0.58f);
        const FVector ArmOffset = EventLocationOffset * 0.82f;
        const FRotator ArmRotation = EventRotationOffset * 0.82f;
        const FVector HandOffset = EventLocationOffset * 0.98f;
        const FRotator HandRotation = EventRotationOffset * 0.98f;
        FallbackLeftArm->SetRelativeLocation(FallbackLeftArmBaseLocation + SharedFallbackOffset + ArmOffset);
        FallbackRightArm->SetRelativeLocation(FallbackRightArmBaseLocation + SharedFallbackOffset + ArmOffset);
        FallbackLeftArm->SetRelativeRotation(FallbackLeftArmBaseRotation + SharedFallbackRotation + ArmRotation);
        FallbackRightArm->SetRelativeRotation(FallbackRightArmBaseRotation + SharedFallbackRotation + ArmRotation);
        FallbackLeftHand->SetRelativeLocation(FallbackLeftHandBaseLocation + HandFallbackOffset + HandOffset);
        FallbackRightHand->SetRelativeLocation(FallbackRightHandBaseLocation + HandFallbackOffset + HandOffset);
        FallbackLeftHand->SetRelativeRotation(FallbackLeftHandBaseRotation + HandFallbackRotation + HandRotation);
        FallbackRightHand->SetRelativeRotation(FallbackRightHandBaseRotation + HandFallbackRotation + HandRotation);
        if (FallbackLeftCuff)
        {
            FallbackLeftCuff->SetRelativeLocation(FallbackLeftCuffBaseLocation + SharedFallbackOffset + ArmOffset);
            FallbackLeftCuff->SetRelativeRotation(FallbackLeftCuffBaseRotation + SharedFallbackRotation + ArmRotation);
        }
        if (FallbackRightCuff)
        {
            FallbackRightCuff->SetRelativeLocation(FallbackRightCuffBaseLocation + SharedFallbackOffset + ArmOffset);
            FallbackRightCuff->SetRelativeRotation(FallbackRightCuffBaseRotation + SharedFallbackRotation + ArmRotation);
        }
        if (FallbackLeftGlovePanel)
        {
            FallbackLeftGlovePanel->SetRelativeLocation(FallbackLeftGlovePanelBaseLocation + HandFallbackOffset + HandOffset);
            FallbackLeftGlovePanel->SetRelativeRotation(FallbackLeftGlovePanelBaseRotation + HandFallbackRotation + HandRotation);
        }
        if (FallbackRightGlovePanel)
        {
            FallbackRightGlovePanel->SetRelativeLocation(FallbackRightGlovePanelBaseLocation + HandFallbackOffset + HandOffset);
            FallbackRightGlovePanel->SetRelativeRotation(FallbackRightGlovePanelBaseRotation + HandFallbackRotation + HandRotation);
        }
        if (FallbackLeftKnucklePlate)
        {
            FallbackLeftKnucklePlate->SetRelativeLocation(FallbackLeftKnucklePlateBaseLocation + HandFallbackOffset + HandOffset);
            FallbackLeftKnucklePlate->SetRelativeRotation(FallbackLeftKnucklePlateBaseRotation + HandFallbackRotation + HandRotation);
        }
        if (FallbackRightKnucklePlate)
        {
            FallbackRightKnucklePlate->SetRelativeLocation(FallbackRightKnucklePlateBaseLocation + HandFallbackOffset + HandOffset);
            FallbackRightKnucklePlate->SetRelativeRotation(FallbackRightKnucklePlateBaseRotation + HandFallbackRotation + HandRotation);
        }
    }
}

FVector ABMPlayerCharacter::GetAimDirection() const
{
    // The screen centre is defined by the rendered first-person camera, so it
    // must also be the ray used by the reticle and by the weapon request. The
    // camera carries small presentation offsets (bob, lean, damage kick, and
    // cinematic hand-off); using only the controller rotation here made an
    // enemy appear under the reticle while the authoritative shot travelled
    // several degrees beside it and hit cover instead. The server still
    // validates the submitted ray against the controller view cone and the
    // dedicated hostile aim surface, so this is camera alignment, not aim
    // assist.
    if (FirstPersonCamera && IsLocallyControlled())
    {
        const FVector CameraDirection = FirstPersonCamera->GetForwardVector().GetSafeNormal();
        if (!CameraDirection.IsNearlyZero())
        {
            return CameraDirection;
        }
    }

    const FRotator ControlRotation = Controller
        ? Controller->GetControlRotation()
        : GetActorRotation();
    FRotator SafeRotation = ControlRotation;
    SafeRotation.Pitch = FMath::Clamp(FMath::UnwindDegrees(SafeRotation.Pitch), -82.0f, 82.0f);
    SafeRotation.Roll = 0.0f;
    return SafeRotation.Vector().GetSafeNormal();
}

void ABMPlayerCharacter::UpdateLocalAimSnapshot()
{
    bLocalAimSnapshotValid = false;
    LocalAimSnapshotTarget.Reset();

    if (!IsLocallyControlled() || IsEliminated() || !FirstPersonCamera || !GetWorld())
    {
        return;
    }

    const FVector SnapshotOrigin = FirstPersonCamera->GetComponentLocation();
    const FVector SnapshotDirection = GetAimDirection();
    if (SnapshotDirection.IsNearlyZero())
    {
        return;
    }

    LocalAimSnapshotOrigin = SnapshotOrigin;
    LocalAimSnapshotDirection = SnapshotDirection.GetSafeNormal();
    bLocalAimSnapshotValid = true;

    const float TraceRange = CurrentWeapon && CurrentWeapon->GetWeaponData()
        ? FMath::Max(100.0f, CurrentWeapon->GetWeaponData()->Balance.RangeMeters * 100.0f)
        : 10000.0f;
    LocalAimSnapshotTarget = BMCombatTargeting::ResolveLiveHostileCrosshairTarget(
        GetWorld(),
        this,
        LocalAimSnapshotOrigin,
        LocalAimSnapshotDirection,
        TraceRange,
        CurrentWeapon);
}

bool ABMPlayerCharacter::IsSignalControlLocked() const
{
    const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
    return !ABMGameState::IsCombatOrdnanceEnabled(State);
}

void ABMPlayerCharacter::PerformInteractionTrace()
{
    if (AActor* Target = FindInteractionTarget())
    {
        ExecuteInteraction(Target);
    }
}

AActor* ABMPlayerCharacter::FindInteractionTarget() const
{
    if (!GetWorld() || !FirstPersonCamera)
    {
        return nullptr;
    }

    const FVector Start = FirstPersonCamera->GetComponentLocation();
    // The interaction actors are deliberately small authored-proxy faces.
    // Keep the forward ray for precise aiming, but use the same 360 cm reach
    // as CanInteract so a player who arrives at the objective marker does not
    // have to put a thin door face on the exact center pixel before E works.
    const FVector End = Start + GetAimDirection() * 360.0f;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BlackMeridianInteraction), false, this);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) && Hit.GetActor() &&
        Hit.GetActor()->GetClass()->ImplementsInterface(UBMInteractionInterface::StaticClass()) &&
        IBMInteractionInterface::Execute_CanInteract(Hit.GetActor(), const_cast<ABMPlayerCharacter*>(this)))
    {
        return Hit.GetActor();
    }

    // If the player is beside the marker rather than looking directly at it,
    // select the nearest visible interactable in reach. This preserves the
    // wall/visibility rule while making the objective usable in a rendered
    // greybox where the breach face is narrower than a final art asset.
    AActor* NearestVisibleTarget = nullptr;
    float NearestDistanceSquared = TNumericLimits<float>::Max();
    for (TActorIterator<AActor> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        AActor* Candidate = *Iterator;
        if (!IsValid(Candidate) || Candidate == this ||
            !Candidate->GetClass()->ImplementsInterface(UBMInteractionInterface::StaticClass()) ||
            !IBMInteractionInterface::Execute_CanInteract(Candidate, const_cast<ABMPlayerCharacter*>(this)))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
        if (DistanceSquared >= NearestDistanceSquared || DistanceSquared > FMath::Square(360.0f))
        {
            continue;
        }

        FHitResult VisibilityHit;
        if (!GetWorld()->LineTraceSingleByChannel(
                VisibilityHit,
                GetPawnViewLocation(),
                Candidate->GetActorLocation(),
                ECC_Visibility,
                Params) ||
            VisibilityHit.GetActor() != Candidate)
        {
            continue;
        }

        NearestVisibleTarget = Candidate;
        NearestDistanceSquared = DistanceSquared;
    }
    if (NearestVisibleTarget)
    {
        return NearestVisibleTarget;
    }
    return nullptr;
}

void ABMPlayerCharacter::UpdateInteractionPrompt()
{
    ABMPlayerController* BMController = Cast<ABMPlayerController>(GetController());
    if (!BMController || !FirstPersonCamera)
    {
        return;
    }

    if (AActor* Target = FindInteractionTarget())
    {
        BMController->SetInteractionPrompt(IBMInteractionInterface::Execute_GetInteractionPrompt(Target));
    }
    else
    {
        BMController->SetInteractionPrompt(FText::GetEmpty());
    }
}

void ABMPlayerCharacter::NotifyDamageDirection(const FVector& DamageOrigin)
{
    if (ABMPlayerController* BMController = Cast<ABMPlayerController>(GetController()))
    {
        const float RelativeYaw = (DamageOrigin - GetActorLocation()).Rotation().Yaw - GetActorRotation().Yaw;
        BMController->SetDamageDirection(RelativeYaw);
    }
}

void ABMPlayerCharacter::BotFireAt(AActor* Target)
{
    if (!HasAuthority() || !Target || !CurrentWeapon || !FirstPersonCamera)
    {
        return;
    }

#if !UE_BUILD_SHIPPING
    const bool bShowcaseAutoFire = FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcaseAutoFire"));
#else
    constexpr bool bShowcaseAutoFire = false;
#endif

    // BotFireAt is also the explicit development showcase firing seam, but it
    // is a public player-pawn method and can be reached by controller/AI code.
    // A human-owned pawn must never fall through this seam and fire merely
    // because an actor selected it as a target. Signal Control bot pawns are
    // identified by their dedicated controller; the rendered showcase is the
    // only human-controller exception, and it must opt in with its exact flag.
    const bool bSignalBotPawn = Cast<ABMSignalBotController>(GetController()) != nullptr;
    if (!bShowcaseAutoFire && !bSignalBotPawn)
    {
#if !UE_BUILD_SHIPPING
        if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcase")))
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian showcase: bot fire blocked owner=%s reason=human_pawn_without_explicit_auto_fire"),
                *GetNameSafe(this));
        }
#endif
        return;
    }

    if (bShowcaseAutoFire)
    {
        UpdateLocalAimSnapshot();
    }
    const FVector Origin = bShowcaseAutoFire && HasLocalAimSnapshot()
        ? GetLocalAimOrigin()
        : FirstPersonCamera->GetComponentLocation();
    const FVector TargetLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
    const FVector TargetDirection = (TargetLocation - Origin).GetSafeNormal();
    // The normal bot path is allowed to aim at its assigned target. The
    // rendered combat showcase is different: it has a real player camera and
    // must never turn a target lookup into aim assist. Its shot direction is
    // the current camera ray, and the target must be the first actor hit by
    // that ray before the weapon is allowed to consume a round.
    const FVector Direction = bShowcaseAutoFire && HasLocalAimSnapshot()
        ? GetLocalAimDirection()
        : (bShowcaseAutoFire ? GetAimDirection() : TargetDirection);
#if !UE_BUILD_SHIPPING
    if (bShowcaseAutoFire)
    {
        const float TraceRange = CurrentWeapon->GetWeaponData()
            ? FMath::Max(100.0f, CurrentWeapon->GetWeaponData()->Balance.RangeMeters * 100.0f)
            : 10000.0f;
        FHitResult CrosshairHit;
        // Keep the development showcase on the exact same target resolver as
        // the HUD and the authoritative weapon path. The old single-line
        // trace here could hit a collision proxy while the shared resolver's
        // narrow sweep correctly found the visible enemy, producing a red
        // reticle with no shot/damage. Do not reintroduce a second notion of
        // "under the crosshair" for this otherwise playable input path.
        ABMCombatCharacter* ResolvedTarget = BMCombatTargeting::ResolveLiveHostileCrosshairTarget(
            GetWorld(),
            this,
            Origin,
            Direction,
            TraceRange,
            CurrentWeapon,
            &CrosshairHit);
        const bool bUnderCrosshair = ResolvedTarget == Target;
        const float TargetAngleDegrees = FMath::RadiansToDegrees(
            FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction, TargetDirection), -1.0f, 1.0f)));
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian showcase: aim validation shooter=%s target=%s origin=%s camera_direction=%s target_direction=%s target_angle_deg=%.2f crosshair_hit=%s crosshair_component=%s under_crosshair=%s resolver=shared"),
            *GetNameSafe(this),
            *GetNameSafe(Target),
            *Origin.ToString(),
            *Direction.ToString(),
            *TargetDirection.ToString(),
            TargetAngleDegrees,
            *GetNameSafe(ResolvedTarget),
            *GetNameSafe(CrosshairHit.GetComponent()),
            bUnderCrosshair ? TEXT("yes") : TEXT("no"));
        if (!bUnderCrosshair)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian showcase: shot skipped reason=target_not_under_crosshair target=%s"),
                *GetNameSafe(Target));
            return;
        }
    }
#endif
    // Signal Control bots update their body heading continuously in
    // ABMSignalBotController. Do not snap the capsule to the fire ray at the
    // slower weapon-decision cadence; that turns every shot into a visible
    // rotation hitch and makes a moving bot face away between shots. The
    // controller rotation still keeps the aim offset aligned. The showcase
    // path intentionally skips this assignment so its camera remains the
    // source of truth for the submitted ray and the server cone check.
    if (!bShowcaseAutoFire)
    {
        if (AController* BotController = GetController())
        {
            BotController->SetControlRotation(Direction.Rotation());
        }
    }
    CurrentWeapon->Fire(Origin, Direction, false);
}

void ABMPlayerCharacter::HandleDeath(const FBMDamageContext& DamageContext)
{
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian combat: player death handler pawn=%s authority=%s controller=%s health=%.1f"),
        *GetName(),
        HasAuthority() ? TEXT("yes") : TEXT("no"),
        *GetNameSafe(GetController()),
        HealthComponent ? HealthComponent->Health : 0.0f);
    if (CurrentWeapon)
    {
        CurrentWeapon->ClearTransientPresentation();
    }
    Super::HandleDeath(DamageContext);
    if (ABMPlayerController* BMController = Cast<ABMPlayerController>(GetController()))
    {
        BMController->ShowDeathScreen();
    }
    if (HasAuthority())
    {
        if (ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>())
        {
            GameMode->HandlePlayerEliminated(this, DamageContext);
        }
    }
}

void ABMPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABMPlayerCharacter, CurrentWeapon);
    DOREPLIFETIME(ABMPlayerCharacter, bAiming);
    DOREPLIFETIME(ABMPlayerCharacter, bOnLadder);
    DOREPLIFETIME(ABMPlayerCharacter, bMountedWeapon);
    DOREPLIFETIME(ABMPlayerCharacter, FragCount);
    DOREPLIFETIME(ABMPlayerCharacter, VeilCount);
    DOREPLIFETIME(ABMPlayerCharacter, bSprinting);
    DOREPLIFETIME(ABMPlayerCharacter, bTacticalSprinting);
    DOREPLIFETIME(ABMPlayerCharacter, bProne);
}
