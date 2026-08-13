// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMCombatCharacter.h"

#include "BMCharacterAnimationDataAsset.h"
#include "BMCombatHistoryComponent.h"
#include "BMCombatTuning.h"
#include "BMHealthComponent.h"
#include "BMSuppressionComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/SoftObjectPath.h"

namespace
{
    constexpr TCHAR ProjectMannyAnimationDataPath[] =
        TEXT("/Game/Data/Animations/DA_BM_Manny_Rifle_Animation.DA_BM_Manny_Rifle_Animation");
    constexpr TCHAR ProjectMannyAnimationBlueprintClassPath[] =
        TEXT("/Game/Characters/Mannequins/Animations/ABP_BM_Manny_Rifle.ABP_BM_Manny_Rifle_C");

    void ConfigureCombatCapsuleCollision(UCapsuleComponent* Capsule)
    {
        if (!Capsule)
        {
            return;
        }

        // CharacterMovement resolves its capsule against the other
        // component's response and the capsule's own response. Runtime
        // campaign walls are WorldStatic, so do not leave this half of the
        // movement contract to an engine or Blueprint collision preset.
        Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Capsule->SetCollisionObjectType(ECC_Pawn);
        Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
        Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
        Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        // Projectile bodies use a pawn overlap to detonate without becoming
        // solid obstacles for CharacterMovement. Keep the root capsule able
        // to receive that overlap; world-static walls remain blocking.
        Capsule->SetGenerateOverlapEvents(true);
    }

    UMaterialInterface* LoadPresentationMaterial()
    {
        if (UMaterialInterface* BasicShapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
        {
            return BasicShapeMaterial;
        }
        return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
    }

    UClass* ResolveAnimationBlueprintClass(const UBMCharacterAnimationDataAsset* AnimationData)
    {
        if (!AnimationData)
        {
            return nullptr;
        }

        // A TSubclassOf can be serialized into the data asset while its class
        // object is still unloaded. Get() is therefore intentionally only the
        // first lookup. The explicit project path keeps the packaged client
        // from silently losing DefaultSlot and falling back to a dry-fire
        // sequence when the generated class is not resident yet.
        if (UClass* AnimClass = AnimationData->AnimBlueprintClass.Get())
        {
            return AnimClass;
        }
        if (AnimationData->GetPathName() != ProjectMannyAnimationDataPath)
        {
            return nullptr;
        }
        return LoadClass<UAnimInstance>(nullptr, ProjectMannyAnimationBlueprintClassPath);
    }
}

ABMCombatCharacter::ABMCombatCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bReplicates = true;
    SetReplicateMovement(true);
    // Derived combat characters already tick for movement, camera, or AI
    // presentation. Keep a base tick as well so authored meshes receive the
    // same deterministic hit/death easing even when no montage asset exists.
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);
    ConfigureCombatCapsuleCollision(GetCapsuleComponent());

    // The root capsule owns movement/world collision.  It is intentionally
    // not the weapon target: its extra radius is useful for locomotion but
    // makes a visible near miss look like a hit when used by a crosshair ray.
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    CombatAimHitbox = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CombatAimHitbox"));
    CombatAimHitbox->SetupAttachment(GetRootComponent());
    // Keep the visibility-only target deliberately narrower than the
    // locomotion capsule. This is a damage/reticle surface, not a
    // general-purpose pawn collision shape. Preserve the full movement
    // capsule height so a visible upper torso or head cannot fall outside the
    // reticle, while the shared targeting resolver applies a horizontal angle
    // guard for close near misses.
    CombatAimHitbox->InitCapsuleSize(BMCombatTuning::CombatAimHitboxRadius, 88.0f);
    CombatAimHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CombatAimHitbox->SetCollisionObjectType(ECC_Pawn);
    CombatAimHitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
    CombatAimHitbox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    CombatAimHitbox->SetGenerateOverlapEvents(false);
    CombatAimHitbox->SetHiddenInGame(true);
    GetCharacterMovement()->MaxWalkSpeed = 360.0f;

    HealthComponent = CreateDefaultSubobject<UBMHealthComponent>(TEXT("Health"));
    CombatHistoryComponent = CreateDefaultSubobject<UBMCombatHistoryComponent>(TEXT("CombatHistory"));
    SuppressionComponent = CreateDefaultSubobject<UBMSuppressionComponent>(TEXT("Suppression"));
    PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
    PlaceholderBody->SetupAttachment(GetRootComponent());
    PlaceholderBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PlaceholderBody->SetMobility(EComponentMobility::Movable);
    PlaceholderBody->SetRelativeLocation(FVector(0.0f, 0.0f, 88.0f));
    PlaceholderBody->SetRelativeScale3D(FVector(0.34f, 0.34f, 0.88f));

    HitFeedbackLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("HitFeedbackLight"));
    HitFeedbackLight->SetupAttachment(GetRootComponent());
    HitFeedbackLight->SetRelativeLocation(FVector(0.0f, 0.0f, 96.0f));
    HitFeedbackLight->SetMobility(EComponentMobility::Movable);
    HitFeedbackLight->SetCastShadows(false);
    HitFeedbackLight->SetAttenuationRadius(190.0f);
    HitFeedbackLight->SetIntensity(0.0f);
    HitFeedbackLight->SetVisibility(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        PlaceholderBody->SetStaticMesh(CubeMesh.Object);
    }
}

void ABMCombatCharacter::BeginPlay()
{
    Super::BeginPlay();
    // Blueprint children and cooked profile data can reapply a collision
    // preset after the native constructor. Reassert the movement contract at
    // runtime so the playable campaign cannot depend on that preset.
    ConfigureCombatCapsuleCollision(GetCapsuleComponent());
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    if (CombatAimHitbox)
    {
        CombatAimHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        CombatAimHitbox->SetCollisionObjectType(ECC_Pawn);
        CombatAimHitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
        CombatAimHitbox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        CombatAimHitbox->SetGenerateOverlapEvents(false);
        CombatAimHitbox->SetHiddenInGame(true);
    }
    SyncCombatAimHitboxToMovementCapsule();
    if (PlaceholderBody)
    {
        PlaceholderBaseLocation = PlaceholderBody->GetRelativeLocation();
        PlaceholderBaseScale = PlaceholderBody->GetRelativeScale3D();
        PlaceholderBaseRotation = PlaceholderBody->GetRelativeRotation();
    }
    HealthComponent->OnDeath.AddDynamic(this, &ABMCombatCharacter::HandleDeath);
    HealthComponent->OnHealthChanged.AddDynamic(this, &ABMCombatCharacter::HandleHealthChanged);

    if (GetMesh() && !AuthoredCharacterMesh.IsNull())
    {
        USkeletalMesh* LoadedCharacterMesh = AuthoredCharacterMesh.LoadSynchronous();
        FSoftObjectPath LoadedCharacterPath = AuthoredCharacterMesh.ToSoftObjectPath();

        // The project-local SK_Mannequin copy is retained as an import slot,
        // but some UE5.8 cooked builds resolve it as the mannequin Skeleton
        // rather than a renderable USkeletalMesh. Prefer the licensed
        // MoverExamples Manny when the configured path cannot be loaded, then
        // keep the local slot as a final compatibility fallback for an
        // installed UE5.1 editor that has no MoverExamples plugin.
        if (!LoadedCharacterMesh)
        {
            const TArray<FSoftObjectPath> CharacterMeshFallbacks = {
                FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")),
                FSoftObjectPath(TEXT("/MoverExamples/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")),
                FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"))};
            for (const FSoftObjectPath& FallbackPath : CharacterMeshFallbacks)
            {
                if (FallbackPath == LoadedCharacterPath)
                {
                    continue;
                }
                if (USkeletalMesh* FallbackMesh = Cast<USkeletalMesh>(FallbackPath.TryLoad()))
                {
                    LoadedCharacterMesh = FallbackMesh;
                    LoadedCharacterPath = FallbackPath;
                    AuthoredCharacterMesh = TSoftObjectPtr<USkeletalMesh>(FallbackPath);
                    break;
                }
            }
        }

        if (LoadedCharacterMesh)
        {
            GetMesh()->SetSkeletalMesh(LoadedCharacterMesh);
            GetMesh()->SetVisibility(true, true);
            GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            AuthoredMeshBaseLocation = GetMesh()->GetRelativeLocation();
            AuthoredMeshBaseScale = GetMesh()->GetRelativeScale3D();
            AuthoredMeshBaseRotation = GetMesh()->GetRelativeRotation();
            bUsingAuthoredCharacterMesh = true;
            if (PlaceholderBody)
            {
                PlaceholderBody->SetVisibility(false, true);
            }
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian presentation: authored character mesh loaded pawn=%s asset=%s"),
                *GetNameSafe(this),
                *LoadedCharacterPath.ToString());
        }
        else
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Black Meridian presentation: authored character mesh failed to load pawn=%s asset=%s; keeping placeholder"),
                *GetNameSafe(this),
                *AuthoredCharacterMesh.ToSoftObjectPath().ToString());
        }
    }

    if (AnimationData && GetMesh())
    {
        if (UClass* AnimClass = ResolveAnimationBlueprintClass(AnimationData))
        {
            GetMesh()->SetAnimInstanceClass(AnimClass);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian presentation: project AnimBP loaded pawn=%s class=%s source=data_or_project_path"),
                *GetNameSafe(this),
                *GetNameSafe(AnimClass));
        }
        else
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Black Meridian presentation: project AnimBP unavailable pawn=%s data=%s"),
                *GetNameSafe(this),
                *AnimationData->GetPathName());
        }
    }
    else if (GetMesh() && !AuthoredAnimBlueprint.IsNull())
    {
        if (UClass* AnimClass = AuthoredAnimBlueprint.LoadSynchronous())
        {
            GetMesh()->SetAnimInstanceClass(AnimClass);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian presentation: authored AnimBP loaded pawn=%s asset=%s"),
                *GetNameSafe(this),
                *AuthoredAnimBlueprint.ToSoftObjectPath().ToString());
        }
        else
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Black Meridian presentation: authored AnimBP failed to load pawn=%s asset=%s"),
                *GetNameSafe(this),
                *AuthoredAnimBlueprint.ToSoftObjectPath().ToString());
        }
    }
}

void ABMCombatCharacter::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bEliminated)
    {
        SyncCombatAimHitboxToMovementCapsule();
    }

    if (HasAuthority() && CombatHistoryComponent && GetWorld())
    {
        // Capture after CharacterMovement has advanced the authoritative pose
        // so a rewind represents the state other clients could have seen.
        CombatHistoryComponent->CaptureCurrentPose(GetWorld()->GetTimeSeconds());
    }

    if (!bEliminated && HitReactionBlend > KINDA_SMALL_NUMBER)
    {
        HitReactionBlend = FMath::FInterpTo(HitReactionBlend, 0.0f, DeltaSeconds, 14.0f);
        const float ReactionAlpha = FMath::Clamp(HitReactionBlend, 0.0f, 1.0f);
        const float ReactionScale = HitReactionScale * ReactionAlpha;
        const FVector ReactionOffset(
            HitReactionLocalDirection.X * 2.5f * ReactionScale,
            HitReactionLocalDirection.Y * 2.0f * ReactionScale,
            HitReactionLocalDirection.Z * 1.2f * ReactionScale);
        const FRotator ReactionRotation(
            -HitReactionLocalDirection.X * 3.5f * ReactionScale,
            0.0f,
            HitReactionLocalDirection.Y * 4.5f * ReactionScale);

        if (PlaceholderBody && !bUsingAuthoredCharacterMesh)
        {
            PlaceholderBody->SetRelativeLocation(PlaceholderBaseLocation + ReactionOffset);
            PlaceholderBody->SetRelativeRotation(PlaceholderBaseRotation + ReactionRotation);
            PlaceholderBody->SetRelativeScale3D(PlaceholderBaseScale * FVector(
                1.0f + 0.006f * ReactionAlpha,
                1.0f + 0.006f * ReactionAlpha,
                1.0f - 0.008f * ReactionAlpha));
        }
        else if (bUsingAuthoredCharacterMesh && GetMesh())
        {
            // Enemy target-facing presentation owns authored-mesh yaw. Hit
            // reaction must only add its local pitch/roll; restoring the
            // cached import yaw here briefly turned a combatant away from
            // the player every time the reaction blend updated.
            FRotator ReactionBaseRotation = AuthoredMeshBaseRotation;
            ReactionBaseRotation.Yaw = GetMesh()->GetRelativeRotation().Yaw;
            GetMesh()->SetRelativeLocation(AuthoredMeshBaseLocation + ReactionOffset);
            GetMesh()->SetRelativeRotation(ReactionBaseRotation + ReactionRotation);
            GetMesh()->SetRelativeScale3D(AuthoredMeshBaseScale * FVector(
                1.0f + 0.002f * ReactionAlpha,
                1.0f + 0.002f * ReactionAlpha,
                1.0f - 0.003f * ReactionAlpha));
        }
    }

    if (bEliminated && bDeathPresentationActive)
    {
        DeathPresentationBlend = FMath::FInterpTo(DeathPresentationBlend, 1.0f, DeltaSeconds, 8.0f);
        const float FallAlpha = FMath::InterpEaseOut(0.0f, 1.0f, FMath::Clamp(DeathPresentationBlend, 0.0f, 1.0f), 2.0f);
        const FVector FallOffset = DeathPresentationTargetOffset * FallAlpha;
        const FRotator FallRotation(
            DeathPresentationPitch * FallAlpha,
            DeathPresentationYaw * FallAlpha,
            DeathPresentationRoll * FallAlpha);
        if (PlaceholderBody && !bUsingAuthoredCharacterMesh)
        {
            PlaceholderBody->SetRelativeLocation(PlaceholderBaseLocation + FallOffset);
            PlaceholderBody->SetRelativeRotation(PlaceholderBaseRotation + FallRotation);
            PlaceholderBody->SetRelativeScale3D(PlaceholderBaseScale * FVector(
                1.0f + 0.02f * FallAlpha,
                1.0f + 0.02f * FallAlpha,
                1.0f - 0.04f * FallAlpha));
        }
        else if (bUsingAuthoredCharacterMesh && GetMesh())
        {
            FRotator FallBaseRotation = AuthoredMeshBaseRotation;
            FallBaseRotation.Yaw = GetMesh()->GetRelativeRotation().Yaw;
            GetMesh()->SetRelativeLocation(AuthoredMeshBaseLocation + FallOffset);
            GetMesh()->SetRelativeRotation(FallBaseRotation + FallRotation);
            GetMesh()->SetRelativeScale3D(AuthoredMeshBaseScale);
        }
    }
}

void ABMCombatCharacter::SetTeam(const EBMTeam NewTeam)
{
    if (HasAuthority())
    {
        Team = NewTeam;
        ConfigurePlaceholderBody(NewTeam == EBMTeam::Lattice ? FColor(30, 90, 125) : FColor(145, 28, 42));
        if (PlaceholderBody && !bUsingAuthoredCharacterMesh && !IsLocallyControlled())
        {
            PlaceholderBody->SetVisibility(true, true);
        }
        ForceNetUpdate();
    }
}

void ABMCombatCharacter::SyncCombatAimHitboxToMovementCapsule()
{
    if (!CombatAimHitbox || !GetCapsuleComponent() || bEliminated)
    {
        return;
    }

    // Preserve the full stance height while keeping the target width
    // substantially narrower than the locomotion capsule. The visible body
    // can be targeted without sub-centimetre precision at normal range, while
    // the targeting resolver keeps its interpolation sweep bounded by the
    // distance-aware horizontal tolerance. An exact ray that reaches this
    // dedicated surface is accepted as a precise body hit.
    const float MovementHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    const float AimRadius = BMCombatTuning::CombatAimHitboxRadius;
    const float AimHalfHeight = FMath::Max(
        AimRadius,
        MovementHalfHeight - BMCombatTuning::CombatAimHitboxVerticalInset);
    if (!FMath::IsNearlyEqual(CombatAimHitbox->GetUnscaledCapsuleRadius(), AimRadius, 0.1f)
        || !FMath::IsNearlyEqual(CombatAimHitbox->GetUnscaledCapsuleHalfHeight(), AimHalfHeight, 0.1f))
    {
        CombatAimHitbox->SetCapsuleSize(AimRadius, AimHalfHeight, false);
    }
}

void ABMCombatCharacter::RestoreForCheckpoint(const float InHealth)
{
    if (!HasAuthority())
    {
        return;
    }

    bEliminated = false;
    ResetDeathPresentation();
    if (HealthComponent)
    {
        HealthComponent->SetHealthForCheckpoint(InHealth);
    }
    SetCharacterAnimationState_Implementation(EBMCharacterMovementState::Idle, 0.0f, false);
    ForceNetUpdate();
}

void ABMCombatCharacter::MulticastHitReaction_Implementation(
    const FVector_NetQuantizeNormal SourceDirection,
    const EBMHitZone HitZone)
{
    if (bEliminated)
    {
        return;
    }

    const FVector LocalSourceDirection = GetActorTransform()
        .InverseTransformVectorNoScale(FVector(SourceDirection))
        .GetSafeNormal2D();
    const bool bHeadshot = HitZone == EBMHitZone::Head;
    const float ReactionScale = bHeadshot
        ? 1.35f
        : (HitZone == EBMHitZone::Arm ? 0.82f : (HitZone == EBMHitZone::Leg ? 0.92f : 1.0f));
    bDirectionalHitReactionActive = true;
    HitReactionLocalDirection = LocalSourceDirection;
    HitReactionScale = ReactionScale;
    HitReactionBlend = 1.0f;

    // Authored Manny materials do not necessarily expose the placeholder's
    // Color/BaseColor parameters. A tiny movable light makes the hit readable
    // on the actual skeletal body too, including headshots and leg shots.
    if (HitFeedbackLight && GetNetMode() != NM_DedicatedServer)
    {
        const float LightHeight = HitZone == EBMHitZone::Head
            ? 154.0f
            : (HitZone == EBMHitZone::Leg ? 46.0f : 96.0f);
        const FLinearColor LightColor = bHeadshot
            ? FLinearColor(0.08f, 0.72f, 1.0f)
            : FLinearColor(1.0f, 0.035f, 0.015f);
        HitFeedbackLight->SetRelativeLocation(FVector(0.0f, 0.0f, LightHeight));
        HitFeedbackLight->SetLightColor(LightColor);
        // Keep the hit cue readable without bleaching the mannequin, nearby
        // cover, and the weapon view. The generated Niagara impact remains
        // the high-frequency cue; this light is only the short body-local
        // colour pulse that makes a hit readable on authored materials.
        HitFeedbackLight->SetAttenuationRadius(bHeadshot ? 150.0f : 120.0f);
        HitFeedbackLight->SetIntensity(bHeadshot ? 1900.0f : 1200.0f);
        HitFeedbackLight->SetVisibility(true);
        GetWorldTimerManager().ClearTimer(HitFeedbackLightTimerHandle);
        GetWorldTimerManager().SetTimer(
            HitFeedbackLightTimerHandle,
            this,
            &ABMCombatCharacter::ClearHitFeedbackLight,
            bHeadshot ? 0.18f : 0.13f,
            false);
    }

    // The event is also consumed by an authored AnimBP/montage when one is
    // assigned. Call the virtual implementation without qualifying it so an
    // enemy/player subclass can route the same hit through its authored
    // sequence fallback as well as the base montage path. Qualifying this
    // call would silently bypass ABMEnemyCharacter's runtime animation path.
    PlayCharacterAnimation_Implementation(bHeadshot ? TEXT("HeadHitReact") : TEXT("HitReact"));
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_AnimationDispatchProbe")))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian presentation: hit feedback route pawn=%s hit_zone=%d light=%s animation_event=%s authored_mesh=%s"),
            *GetNameSafe(this),
            static_cast<int32>(HitZone),
            HitFeedbackLight && GetNetMode() != NM_DedicatedServer ? TEXT("yes") : TEXT("no"),
            *(bHeadshot ? FString(TEXT("HeadHitReact")) : FString(TEXT("HitReact"))),
            bUsingAuthoredCharacterMesh ? TEXT("yes") : TEXT("no"));
    }
#endif
    if (PlaceholderMaterial)
    {
        const FLinearColor FlashColor = bHeadshot
            ? FLinearColor(0.16f, 0.82f, 1.0f, 1.0f)
            : FLinearColor(1.0f, 0.04f, 0.015f, 1.0f);
        PlaceholderMaterial->SetVectorParameterValue(TEXT("Color"), FlashColor);
        PlaceholderMaterial->SetVectorParameterValue(TEXT("BaseColor"), FlashColor);
    }
    GetWorldTimerManager().ClearTimer(HitFlashTimerHandle);
    GetWorldTimerManager().SetTimer(
        HitFlashTimerHandle,
        this,
        &ABMCombatCharacter::ClearHitFlash,
        bHeadshot ? 0.28f : 0.22f,
        false);
}

void ABMCombatCharacter::HandleDeath(const FBMDamageContext& DamageContext)
{
    if (bEliminated)
    {
        return;
    }

#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe")))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian role death: target=%s target_class=%s target_team=%d instigator=%s instigator_class=%s hit_zone=%d raw=%.1f final=%.1f headshot=%s"),
            *GetNameSafe(this),
            *GetNameSafe(GetClass()),
            static_cast<int32>(GetTeam()),
            *GetNameSafe(DamageContext.InstigatorActor),
            DamageContext.InstigatorActor ? *GetNameSafe(DamageContext.InstigatorActor->GetClass()) : TEXT("None"),
            static_cast<int32>(DamageContext.HitZone),
            DamageContext.RawDamage,
            DamageContext.FinalDamage,
            DamageContext.bHeadshot ? TEXT("yes") : TEXT("no"));
    }
#endif

    FVector IncomingDirection = FVector::ForwardVector;
    if (const AActor* InstigatorActor = DamageContext.InstigatorActor)
    {
        IncomingDirection = (GetActorLocation() - InstigatorActor->GetActorLocation()).GetSafeNormal2D();
    }
    if (IncomingDirection.IsNearlyZero())
    {
        IncomingDirection = GetActorForwardVector().GetSafeNormal2D();
    }
    DeathPresentationDirection = IncomingDirection;
    DeathPresentationHitZone = DamageContext.HitZone;
    bEliminated = true;
    BeginDeathPresentation();
    OnEliminated.Broadcast(this);
    ForceNetUpdate();
}

void ABMCombatCharacter::OnRep_Eliminated()
{
    if (bEliminated)
    {
        BeginDeathPresentation();
    }
    else
    {
        ResetDeathPresentation();
    }
}

void ABMCombatCharacter::OnRep_DeathPresentation()
{
    if (bEliminated)
    {
        // Direction and elimination can arrive in either order. Recomputing
        // the targets here lets a late direction update correct the already
        // started local presentation without replaying the death event.
        ConfigureDeathPresentationMotion();
    }
}

void ABMCombatCharacter::ConfigureDeathPresentationMotion()
{
    FVector IncomingDirection = FVector(DeathPresentationDirection).GetSafeNormal2D();
    if (IncomingDirection.IsNearlyZero())
    {
        IncomingDirection = GetActorForwardVector().GetSafeNormal2D();
    }
    DeathPresentationLocalDirection = GetActorTransform()
        .InverseTransformVectorNoScale(IncomingDirection)
        .GetSafeNormal2D();
    if (DeathPresentationLocalDirection.IsNearlyZero())
    {
        DeathPresentationLocalDirection = FVector::ForwardVector;
    }

    const float ZoneScale = DeathPresentationHitZone == EBMHitZone::Head
        ? 1.16f
        : (DeathPresentationHitZone == EBMHitZone::Leg ? 0.84f : 1.0f);
    DeathPresentationPitch = FMath::Clamp(DeathPresentationLocalDirection.X * 48.0f * ZoneScale, -58.0f, 58.0f);
    DeathPresentationYaw = FMath::Clamp(DeathPresentationLocalDirection.Y * 18.0f * ZoneScale, -24.0f, 24.0f);
    DeathPresentationRoll = FMath::Clamp(-DeathPresentationLocalDirection.Y * 76.0f * ZoneScale, -92.0f, 92.0f);
    DeathPresentationTargetOffset = FVector(
        DeathPresentationLocalDirection.X * 30.0f * ZoneScale,
        DeathPresentationLocalDirection.Y * 24.0f * ZoneScale,
        -54.0f * ZoneScale);
    DeathPresentationDuration = DeathPresentationHitZone == EBMHitZone::Head
        ? 1.22f
        : (DeathPresentationHitZone == EBMHitZone::Leg ? 1.0f : 1.08f);
}

void ABMCombatCharacter::BeginDeathPresentation()
{
    if (bDeathPresentationActive)
    {
        ConfigureDeathPresentationMotion();
        return;
    }

    ConfigureDeathPresentationMotion();
    GetWorldTimerManager().ClearTimer(HitFlashTimerHandle);
    GetWorldTimerManager().ClearTimer(HitFeedbackLightTimerHandle);
    ClearHitFeedbackLight();
    HitReactionBlend = 0.0f;
    bDeathPresentationActive = true;
    DeathPresentationBlend = 0.0f;
    SetCharacterAnimationState_Implementation(EBMCharacterMovementState::Dead, 0.0f, false);
    // Keep this virtual: enemy subclasses own the project-mannequin sequence
    // fallback, while the base still broadcasts the event and plays any
    // configured montage. An explicitly qualified call would reduce a lethal
    // hit to the procedural knockdown even when the Death asset is loaded.
    PlayCharacterAnimation_Implementation(TEXT("Death"));
    if (AnimationData)
    {
        const TSoftObjectPtr<UAnimSequence> DeathSequenceReference =
            AnimationData->FindEventSequence(TEXT("Death"));
        if (UAnimSequence* DeathSequence = DeathSequenceReference.LoadSynchronous())
        {
            // Leave the authored death pose visible for the full clip when a
            // character profile supplies one; the old directional knockdown
            // duration remains the lower bound for profiles without a clip.
            DeathPresentationDuration = FMath::Clamp(
                FMath::Max(DeathPresentationDuration, DeathSequence->GetPlayLength()),
                0.65f,
                2.5f);
        }
    }
    // A dead pawn is still replicated for the short death-presentation
    // window, but it must not keep receiving movement/smoothing updates. If
    // the last route velocity is allowed to drain after elimination, any
    // fallback presentation attached to the root appears to slide across the
    // floor as a collection of moving boxes. Freeze the movement component and
    // the actor transform on every authority/client copy before the death
    // animation begins.
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    GetCharacterMovement()->SetComponentTickEnabled(false);
    SetReplicateMovement(false);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (CombatAimHitbox)
    {
        CombatAimHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    if (PlaceholderBody && !bUsingAuthoredCharacterMesh)
    {
        // Keep the fallback silhouette visible long enough to communicate a
        // hit-confirmed fall. An authored Death montage can layer on top of
        // this or replace it entirely when a skeletal presentation is set.
        PlaceholderBody->SetRelativeLocation(PlaceholderBaseLocation);
        PlaceholderBody->SetRelativeRotation(PlaceholderBaseRotation);
        PlaceholderBody->SetRelativeScale3D(PlaceholderBaseScale);
        // The enemy subclass suppresses this primitive fallback on death. It
        // remains explicitly hidden here as a safe default for any combat
        // character that has no authored mesh, so a corpse can never regress
        // into the moving cube silhouette.
        PlaceholderBody->SetVisibility(false, true);
    }
    if (bUsingAuthoredCharacterMesh && GetMesh())
    {
        FRotator RestoreRotation = AuthoredMeshBaseRotation;
        RestoreRotation.Yaw = GetMesh()->GetRelativeRotation().Yaw;
        GetMesh()->SetRelativeLocation(AuthoredMeshBaseLocation);
        GetMesh()->SetRelativeRotation(RestoreRotation);
        GetMesh()->SetRelativeScale3D(AuthoredMeshBaseScale);
        GetMesh()->SetVisibility(true, true);
    }
    GetWorldTimerManager().SetTimer(
        DeathPresentationTimerHandle,
        this,
        &ABMCombatCharacter::FinishDeathPresentation,
        DeathPresentationDuration,
        false);

#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcaseAutoFire"))
        || FParse::Param(FCommandLine::Get(), TEXT("BM_DeathPresentationProbe")))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian presentation: death knockdown pawn=%s authority=%s hit_zone=%d incoming=%s"),
            *GetNameSafe(this),
            HasAuthority() ? TEXT("yes") : TEXT("no"),
            static_cast<int32>(DeathPresentationHitZone),
            *FVector(DeathPresentationDirection).ToString());
    }
#endif
}

void ABMCombatCharacter::ResetDeathPresentation()
{
    GetWorldTimerManager().ClearTimer(HitFlashTimerHandle);
    GetWorldTimerManager().ClearTimer(HitFeedbackLightTimerHandle);
    GetWorldTimerManager().ClearTimer(DeathPresentationTimerHandle);
    bDirectionalHitReactionActive = false;
    HitReactionBlend = 0.0f;
    bDeathPresentationActive = false;
    DeathPresentationBlend = 0.0f;
    SetReplicateMovement(true);
    GetCharacterMovement()->SetComponentTickEnabled(true);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    if (CombatAimHitbox)
    {
        CombatAimHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    if (PlaceholderBody)
    {
        PlaceholderBody->SetVisibility(!bUsingAuthoredCharacterMesh, true);
        PlaceholderBody->SetRelativeLocation(PlaceholderBaseLocation);
        PlaceholderBody->SetRelativeRotation(PlaceholderBaseRotation);
        PlaceholderBody->SetRelativeScale3D(PlaceholderBaseScale);
    }
    if (bUsingAuthoredCharacterMesh && GetMesh())
    {
        FRotator RestoreRotation = AuthoredMeshBaseRotation;
        RestoreRotation.Yaw = GetMesh()->GetRelativeRotation().Yaw;
        GetMesh()->SetRelativeLocation(AuthoredMeshBaseLocation);
        GetMesh()->SetRelativeRotation(RestoreRotation);
        GetMesh()->SetRelativeScale3D(AuthoredMeshBaseScale);
        GetMesh()->SetVisibility(true, true);
    }
    ClearHitFeedbackLight();
}

void ABMCombatCharacter::HandleHealthChanged(const float /*NewHealth*/, const float DamageTaken)
{
    if (DamageTaken <= 0.0f || bEliminated)
    {
        return;
    }

    if (bDirectionalHitReactionActive)
    {
        return;
    }

    // Direction and hit-zone presentation is emitted by the authority-owned
    // MulticastHitReaction path. Keep this replicated-health callback as a
    // material-only fallback for damage sources that have no source vector;
    // it must not overwrite the deterministic directional body reaction.
    if (PlaceholderMaterial)
    {
        PlaceholderMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.04f, 0.015f, 1.0f));
        PlaceholderMaterial->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(1.0f, 0.04f, 0.015f, 1.0f));
        GetWorldTimerManager().ClearTimer(HitFlashTimerHandle);
        GetWorldTimerManager().SetTimer(HitFlashTimerHandle, this, &ABMCombatCharacter::ClearHitFlash, 0.09f, false);
    }
}

void ABMCombatCharacter::ClearHitFlash()
{
    bDirectionalHitReactionActive = false;
    HitReactionBlend = 0.0f;
    if (!bEliminated && PlaceholderBody)
    {
        PlaceholderBody->SetRelativeLocation(PlaceholderBaseLocation);
        PlaceholderBody->SetRelativeRotation(PlaceholderBaseRotation);
        PlaceholderBody->SetRelativeScale3D(PlaceholderBaseScale);
    }
    if (!bEliminated && bUsingAuthoredCharacterMesh && GetMesh())
    {
        FRotator RestoreRotation = AuthoredMeshBaseRotation;
        RestoreRotation.Yaw = GetMesh()->GetRelativeRotation().Yaw;
        GetMesh()->SetRelativeLocation(AuthoredMeshBaseLocation);
        GetMesh()->SetRelativeRotation(RestoreRotation);
        GetMesh()->SetRelativeScale3D(AuthoredMeshBaseScale);
    }
    if (PlaceholderMaterial)
    {
        PlaceholderMaterial->SetVectorParameterValue(TEXT("Color"), PlaceholderBaseColor);
        PlaceholderMaterial->SetVectorParameterValue(TEXT("BaseColor"), PlaceholderBaseColor);
    }
}

void ABMCombatCharacter::ClearHitFeedbackLight()
{
    if (HitFeedbackLight)
    {
        HitFeedbackLight->SetIntensity(0.0f);
        HitFeedbackLight->SetVisibility(false);
    }
}

void ABMCombatCharacter::FinishDeathPresentation()
{
    bDeathPresentationActive = false;
    if (PlaceholderBody)
    {
        PlaceholderBody->SetVisibility(false, true);
    }
    if (GetMesh())
    {
        GetMesh()->SetVisibility(false, true);
    }
}

void ABMCombatCharacter::SetCharacterAnimationState_Implementation(
    const EBMCharacterMovementState NewState,
    const float GroundSpeed,
    const bool bAiming)
{
    AnimationState = NewState;
    AnimationGroundSpeed = FMath::Max(0.0f, GroundSpeed);
    bAnimationAiming = bAiming;
    OnAnimationStateChanged.Broadcast(AnimationState, AnimationGroundSpeed, bAnimationAiming);
}

void ABMCombatCharacter::SetCharacterAimOffset_Implementation(const FRotator AimRotation)
{
    AnimationAimOffset = AimRotation;
}

void ABMCombatCharacter::PlayCharacterAnimation_Implementation(const FName AnimationEvent)
{
    if (AnimationData && GetMesh())
    {
        const TSoftObjectPtr<UAnimMontage> MontageReference = AnimationData->FindEventMontage(AnimationEvent);
        if (!MontageReference.IsNull())
        {
            if (UAnimMontage* Montage = MontageReference.LoadSynchronous())
            {
                if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
                {
                    AnimInstance->Montage_Play(Montage);
                }
            }
        }
    }
    OnAnimationEvent.Broadcast(AnimationEvent);
}

void ABMCombatCharacter::ConfigurePlaceholderBody(const FColor& Color)
{
    if (!PlaceholderBody)
    {
        return;
    }

    UMaterialInterface* BaseMaterial = LoadPresentationMaterial();
    if (BaseMaterial)
    {
        // The fallback palette is authored as compact display-space colors,
        // just like the runtime greybox. Feeding FColor directly into
        // FLinearColor applies an additional sRGB conversion and makes the
        // Vanta red silhouettes disappear against the night scene.
        // The fallback body is still used on stripped-down cooked clients.
        // Keep it readable in the same shadow floor as the authored Manny
        // route; the role color remains restrained and non-emissive.
        constexpr float PlaceholderBrightness = 0.72f;
        PlaceholderBaseColor = FLinearColor(
            (static_cast<float>(Color.R) / 255.0f) * PlaceholderBrightness,
            (static_cast<float>(Color.G) / 255.0f) * PlaceholderBrightness,
            (static_cast<float>(Color.B) / 255.0f) * PlaceholderBrightness,
            static_cast<float>(Color.A) / 255.0f);
        PlaceholderMaterial = PlaceholderBody->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMaterial);
        if (PlaceholderMaterial)
        {
            PlaceholderMaterial->SetVectorParameterValue(TEXT("Color"), PlaceholderBaseColor);
            PlaceholderMaterial->SetVectorParameterValue(TEXT("BaseColor"), PlaceholderBaseColor);
            PlaceholderMaterial->SetScalarParameterValue(TEXT("Metallic"), 0.08f);
            PlaceholderMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.82f);
        }
    }
}

void ABMCombatCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABMCombatCharacter, Team);
    DOREPLIFETIME(ABMCombatCharacter, bEliminated);
    DOREPLIFETIME(ABMCombatCharacter, DeathPresentationDirection);
    DOREPLIFETIME(ABMCombatCharacter, DeathPresentationHitZone);
}
