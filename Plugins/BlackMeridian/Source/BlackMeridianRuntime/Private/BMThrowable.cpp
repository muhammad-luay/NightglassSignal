// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMThrowable.h"

#include "BMAudioDataAsset.h"
#include "BMHealthComponent.h"
#include "BMCombatCharacter.h"
#include "BMCombatTuning.h"
#include "BMSquadSubsystem.h"
#include "BMEnemyCharacter.h"
#include "BMPlayerCharacter.h"
#include "BMGameUserSettings.h"
#include "BMSuppressionComponent.h"
#include "BMVFXDataAsset.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    /**
     * Project-owned Niagara systems are the normal player-facing baseline.
     * Keep engine content probe-only, but do not suppress the cooked project
     * grenade effects: a thrown Frag or Veil must visibly detonate in the
     * same ordinary path that applies its authoritative gameplay result.
     */
    bool ShouldSpawnConfiguredNiagara(const UNiagaraSystem* System)
    {
        if (!System)
        {
            return false;
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("BM_VFXProbe")))
        {
            return true;
        }
        const FString SystemPath = System->GetPathName();
        if (SystemPath.StartsWith(TEXT("/Game/VFX/Generated/"), ESearchCase::IgnoreCase))
        {
            return false;
        }
        return SystemPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase);
    }

    /**
     * Keeps grenade feedback audible in a source-only checkout. Authored
     * explosion/suppression cues still win; this is only the playable fallback
     * when the optional audio profile is empty.
     */
    void PlayRuntimeThrowablePulse(
        const UObject* WorldContextObject,
        const FVector& Location,
        const bool bTactical)
    {
        if (!WorldContextObject
            || FParse::Param(FCommandLine::Get(), TEXT("NoSound")))
        {
            return;
        }

        constexpr int32 SampleRate = 22050;
        const float Duration = bTactical ? 0.48f : 0.72f;
        const int32 SampleCount = FMath::Max(1, FMath::RoundToInt(Duration * SampleRate));
        TArray<int16> Samples;
        Samples.SetNumUninitialized(SampleCount);
        uint32 NoiseState = bTactical ? 0x8EBC6AF1u : 0xC2B2AE35u;

        for (int32 Index = 0; Index < SampleCount; ++Index)
        {
            const float Time = static_cast<float>(Index) / static_cast<float>(SampleRate);
            const float NormalizedTime = Time / Duration;
            const float Attack = FMath::Clamp(Time / (bTactical ? 0.012f : 0.004f), 0.0f, 1.0f);
            const float Envelope = Attack * FMath::Pow(
                FMath::Clamp(1.0f - NormalizedTime, 0.0f, 1.0f),
                bTactical ? 1.35f : 1.7f);
            NoiseState = NoiseState * 1664525u + 1013904223u;
            const float Noise = (static_cast<float>((NoiseState >> 9) & 0x7FFFu) / 16384.0f) - 1.0f;

            float Signal = 0.0f;
            if (bTactical)
            {
                const float Pulse = FMath::Sin(Time * 2.0f * PI * 156.0f) * 0.36f;
                const float Harmonic = FMath::Sin(Time * 2.0f * PI * 392.0f + 0.4f) * 0.16f;
                Signal = Pulse + Harmonic + Noise * 0.18f;
            }
            else
            {
                const float Thump = FMath::Sin(Time * 2.0f * PI * 54.0f) * 0.58f
                    + FMath::Sin(Time * 2.0f * PI * 88.0f + 0.35f) * 0.26f;
                const float Crack = FMath::Sin(Time * 2.0f * PI * 1420.0f) * 0.12f;
                Signal = Thump + Noise * 0.72f + Crack;
            }

            Samples[Index] = static_cast<int16>(FMath::Clamp(Signal * Envelope, -1.0f, 1.0f) * 32767.0f);
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
        UGameplayStatics::PlaySoundAtLocation(
            WorldContextObject,
            Wave,
            Location,
            FRotator::ZeroRotator,
            bTactical ? 0.5f : 0.86f,
            bTactical ? 1.0f : 0.82f);
    }
}

ABMThrowable::ABMThrowable()
{
    bReplicates = true;
    // A grenade is a network-visible projectile, not just an authoritative
    // damage timer. Explicit movement replication keeps its throw arc and
    // bounce visible to remote clients before the multicast detonation.
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    SetRootComponent(Collision);
    Collision->InitSphereRadius(12.0f);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    // Grenades must bounce off the authored/runtime world, but they are not
    // solid actors. Blocking Pawn made a thrown grenade pin an enemy capsule
    // against it and made a crowded firefight look like broken locomotion.
    // Detonation still finds pawns through the explicit overlap query below.
    Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(Collision);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetRelativeScale3D(FVector(0.12f));

    DetonationFlashMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DetonationFlashMesh"));
    DetonationFlashMesh->SetupAttachment(Collision);
    DetonationFlashMesh->SetOnlyOwnerSee(false);
    DetonationFlashMesh->SetCastShadow(false);
    DetonationFlashMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DetonationFlashMesh->SetVisibility(false, true);
    DetonationFlashMesh->SetRelativeScale3D(FVector(0.18f));

    DetonationRingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DetonationRingMesh"));
    DetonationRingMesh->SetupAttachment(Collision);
    DetonationRingMesh->SetOnlyOwnerSee(false);
    DetonationRingMesh->SetCastShadow(false);
    DetonationRingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DetonationRingMesh->SetVisibility(false, true);
    DetonationRingMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -10.0f));
    DetonationRingMesh->SetRelativeScale3D(FVector(0.18f, 0.18f, 0.035f));

    DetonationLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("DetonationLight"));
    DetonationLight->SetupAttachment(Collision);
    DetonationLight->SetMobility(EComponentMobility::Movable);
    DetonationLight->SetCastShadows(false);
    DetonationLight->SetIntensity(0.0f);
    DetonationLight->SetAttenuationRadius(420.0f);
    DetonationLight->SetVisibility(false, true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (SphereMesh.Succeeded())
    {
        Mesh->SetStaticMesh(SphereMesh.Object);
        DetonationFlashMesh->SetStaticMesh(SphereMesh.Object);
    }
    if (CylinderMesh.Succeeded())
    {
        DetonationRingMesh->SetStaticMesh(CylinderMesh.Object);
    }

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->UpdatedComponent = Collision;
    ProjectileMovement->InitialSpeed = 950.0f;
    ProjectileMovement->MaxSpeed = 950.0f;
    ProjectileMovement->ProjectileGravityScale = 0.7f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce = true;
}

void ABMThrowable::BeginPlay()
{
    Super::BeginPlay();

    if (!ThrowableData || ThrowableData->ThrowableId != ThrowableId)
    {
        InitializeFromData(UBMThrowableDataAsset::LoadContentProfile(this, ThrowableId));
    }

    if (HasAuthority())
    {
        GetWorldTimerManager().SetTimer(FuseTimer, this, &ABMThrowable::Detonate, FuseSeconds, false);
    }
}

void ABMThrowable::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bDetonationPresentationActive)
    {
        return;
    }

    DetonationPresentationElapsed += FMath::Max(0.0f, DeltaSeconds);
    const float Alpha = 1.0f - FMath::Clamp(DetonationPresentationElapsed / 0.46f, 0.0f, 1.0f);
    const float Scale = FMath::Lerp(0.25f, 2.8f, 1.0f - Alpha);
    if (DetonationFlashMesh)
    {
        DetonationFlashMesh->SetRelativeScale3D(FVector(Scale));
    }
    if (DetonationRingMesh)
    {
        const float RingScale = FMath::Lerp(0.18f, 3.8f, 1.0f - Alpha);
        DetonationRingMesh->SetRelativeScale3D(FVector(
            RingScale,
            RingScale,
            bDetonationTactical ? 0.028f : 0.055f));
    }
    if (DetonationLight)
    {
        DetonationLight->SetIntensity(1450.0f * Alpha);
    }
    if (DetonationMaterial)
    {
        DetonationMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), 8.0f * Alpha);
    }

    if (DetonationPresentationElapsed >= 0.46f)
    {
        Destroy();
    }
}

void ABMThrowable::InitializeFromData(UBMThrowableDataAsset* InThrowableData)
{
    if (!InThrowableData)
    {
        return;
    }

    ThrowableData = InThrowableData;
    ThrowableId = InThrowableData->ThrowableId;
    FuseSeconds = FMath::Max(0.05f, InThrowableData->FuseSeconds);
    Damage = FMath::Max(0.0f, InThrowableData->Damage);
    Radius = FMath::Max(1.0f, InThrowableData->Radius);
    SuppressionAmount = FMath::Max(0.0f, InThrowableData->SuppressionAmount);
    AudioProfile = InThrowableData->AudioProfile;
    VFXProfile = InThrowableData->VFXProfile;

    if (Mesh && !InThrowableData->Mesh.IsNull())
    {
        if (UStaticMesh* ThrowableMesh = InThrowableData->Mesh.LoadSynchronous())
        {
            Mesh->SetStaticMesh(ThrowableMesh);
            Mesh->SetRelativeScale3D(FVector(0.92f));
            bUsingProjectMesh = ThrowableMesh->GetPathName().StartsWith(TEXT("/Game/Weapons/Meshes/SM_BM_"));
        }
    }

    ApplyPresentationMaterial();

    if (ProjectileMovement)
    {
        ProjectileMovement->ProjectileGravityScale = FMath::Max(0.0f, InThrowableData->GravityScale);
        ProjectileMovement->InitialSpeed = FMath::Max(1.0f, InThrowableData->ThrowSpeed);
        ProjectileMovement->MaxSpeed = ProjectileMovement->InitialSpeed;
    }

    // SpawnActor calls BeginPlay before callers can set a replicated ID when
    // they use the non-deferred API. Reset the authority fuse if the data is
    // applied after BeginPlay so a tactical grenade never inherits frag timing.
    if (HasAuthority() && GetWorld() && FuseTimer.IsValid())
    {
        GetWorldTimerManager().ClearTimer(FuseTimer);
        GetWorldTimerManager().SetTimer(FuseTimer, this, &ABMThrowable::Detonate, FuseSeconds, false);
    }
}

void ABMThrowable::ArmAndThrow(const FVector& Direction, const float Speed)
{
    if (HasAuthority())
    {
        const float ThrowSpeed = Speed > 0.0f
            ? Speed
            : (ThrowableData ? ThrowableData->ThrowSpeed : 950.0f);
        ProjectileMovement->Velocity = Direction.GetSafeNormal() * ThrowSpeed;
    }
}

void ABMThrowable::OnRep_ThrowableId()
{
    InitializeFromData(UBMThrowableDataAsset::LoadContentProfile(this, ThrowableId));
}

void ABMThrowable::Detonate()
{
    if (!HasAuthority())
    {
        return;
    }

    const float SafeRadius = FMath::Max(1.0f, Radius);
    DrawDebugSphere(GetWorld(), GetActorLocation(), SafeRadius, 24, ThrowableId == EBMWeaponId::FragGrenade ? FColor::Orange : FColor::Cyan, false, 0.8f, 0, 2.0f);
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BlackMeridianThrowable), false, this);
    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
    GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(SafeRadius), QueryParams);

    // A character can contribute several Pawn collision components (capsule,
    // mesh and authored aim hitboxes).  OverlapMultiByObjectType reports each
    // component independently, but a grenade is one blast and must damage a
    // character only once.  Collapse the component results to actor results
    // before applying either explosive damage or tactical suppression.
    TArray<AActor*> OverlapActors;
    OverlapActors.Reserve(Overlaps.Num());
    TSet<AActor*> SeenActors;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        if (AActor* Actor = Overlap.GetActor(); Actor && !SeenActors.Contains(Actor))
        {
            SeenActors.Add(Actor);
            OverlapActors.Add(Actor);
        }
    }

#if !UE_BUILD_SHIPPING
    const bool bRoleProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe"));
    if (bRoleProbe)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian role grenade detonate: grenade=%s thrower=%s thrower_class=%s overlaps=%d unique_actors=%d location=%s radius=%.1f"),
            *GetNameSafe(this),
            *GetNameSafe(GetOwner()),
            GetOwner() ? *GetNameSafe(GetOwner()->GetClass()) : TEXT("None"),
            Overlaps.Num(),
            OverlapActors.Num(),
            *GetActorLocation().ToString(),
            SafeRadius);
    }
#endif

    if (ThrowableId == EBMWeaponId::FragGrenade)
    {
        for (AActor* Actor : OverlapActors)
        {
            if (Actor)
            {
                const ABMCombatCharacter* Thrower = Cast<ABMCombatCharacter>(GetOwner());
                const ABMCombatCharacter* Target = Cast<ABMCombatCharacter>(Actor);
                if (Thrower && Target && Thrower->GetTeam() == Target->GetTeam())
                {
                    const UBMGameUserSettings* Settings = GEngine ? Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
#if !UE_BUILD_SHIPPING
                    if (bRoleProbe)
                    {
                        UE_LOG(
                            LogTemp,
                            Display,
                            TEXT("Black Meridian role grenade overlap: grenade=%s target=%s target_team=%d thrower_team=%d friendly_fire=%s action=%s"),
                            *GetNameSafe(this),
                            *GetNameSafe(Target),
                            static_cast<int32>(Target->GetTeam()),
                            static_cast<int32>(Thrower->GetTeam()),
                            Settings && Settings->bFriendlyFireEnabled ? TEXT("yes") : TEXT("no_or_unavailable"),
                            Settings && Settings->bFriendlyFireEnabled ? TEXT("damage") : TEXT("skip"));
                    }
#endif
                    if (!Settings || !Settings->bFriendlyFireEnabled)
                    {
                        continue;
                    }
                }
                if (UBMHealthComponent* Health = Actor->FindComponentByClass<UBMHealthComponent>())
                {
                    const float DistanceScale = 1.0f - FMath::Clamp(FVector::Dist(GetActorLocation(), Actor->GetActorLocation()) / SafeRadius, 0.0f, 1.0f) * 0.7f;
                    const ABMEnemyCharacter* EnemyThrower = Cast<ABMEnemyCharacter>(GetOwner());
                    const bool bHostilePlayerHit = EnemyThrower && Cast<ABMPlayerCharacter>(Actor);
                    const float HostileDamageMultiplier = bHostilePlayerHit
                        ? EnemyThrower->GetDamageToPlayerMultiplier()
                        : 1.0f;
                    FBMDamageContext Context;
                    Context.RawDamage = Damage;
                    Context.FinalDamage = Damage * DistanceScale * HostileDamageMultiplier;
                    if (bHostilePlayerHit)
                    {
                        Context.FinalDamage = FMath::Min(
                            Context.FinalDamage,
                            BMCombatTuning::HostileExplosionDamageCap);
                        if (UBMSquadSubsystem* Squad = GetWorld()->GetSubsystem<UBMSquadSubsystem>())
                        {
                            Context.FinalDamage = Squad->ConsumeHostileDamageBudget(
                                Actor,
                                Context.FinalDamage,
                                EnemyThrower->UsesReadableCombatPacing()
                                    ? BMCombatTuning::OpeningWaveDamageWindowCap
                                    : BMCombatTuning::HostileDamageWindowCap);
                        }
                    }
                    if (Context.FinalDamage <= KINDA_SMALL_NUMBER)
                    {
                        continue;
                    }
                    Context.HitZone = EBMHitZone::Torso;
                    Context.InstigatorActor = GetOwner();
                    Context.bPlayerThrowableDamage = Cast<ABMPlayerCharacter>(GetOwner()) != nullptr;
                    Health->ApplyDamage(Context.FinalDamage, Context);
                    if (ABMCombatCharacter* CombatTarget = Cast<ABMCombatCharacter>(Actor))
                    {
                        FVector SourceDirection = (Actor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
                        if (SourceDirection.IsNearlyZero())
                        {
                            SourceDirection = GetActorForwardVector();
                        }
                        CombatTarget->MulticastHitReaction(SourceDirection, Context.HitZone);
                    }
                }
            }
        }
    }
    else if (ThrowableId == EBMWeaponId::VeilGrenade)
    {
        const ABMCombatCharacter* Thrower = Cast<ABMCombatCharacter>(GetOwner());
        for (AActor* Actor : OverlapActors)
        {
            ABMCombatCharacter* Target = Cast<ABMCombatCharacter>(Actor);
            if (!Target || Target->IsEliminated() || (Thrower && Target->GetTeam() == Thrower->GetTeam()))
            {
                continue;
            }

            if (Target->SuppressionComponent)
            {
                Target->SuppressionComponent->AddSuppression(SuppressionAmount);
            }
        }
    }
    MulticastDetonation(GetActorLocation(), ThrowableId == EBMWeaponId::VeilGrenade);
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (ProjectileMovement)
    {
        ProjectileMovement->StopMovementImmediately();
    }
    if (GetNetMode() == NM_DedicatedServer || !bDetonationPresentationActive)
    {
        Destroy();
    }
}

void ABMThrowable::MulticastDetonation_Implementation(
    const FVector_NetQuantize Location,
    const bool bTactical)
{
    if (GetNetMode() == NM_DedicatedServer || !GetWorld())
    {
        return;
    }

    bool bPlayedAudio = false;
    if (UBMAudioDataAsset* Audio = AudioProfile.LoadSynchronous())
    {
        USoundBase* Sound = bTactical
            ? Audio->Suppression.LoadSynchronous()
            : Audio->Explosion.LoadSynchronous();
        if (Sound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, Sound, Location);
            bPlayedAudio = true;
        }
    }
    if (!bPlayedAudio)
    {
        PlayRuntimeThrowablePulse(this, Location, bTactical);
    }
    if (UBMVFXDataAsset* VFX = VFXProfile.LoadSynchronous())
    {
        const bool bVFXProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_VFXProbe"));
        UNiagaraSystem* Effect = bTactical
            ? VFX->Suppression.LoadSynchronous()
            : VFX->Explosion.LoadSynchronous();
        if (ShouldSpawnConfiguredNiagara(Effect))
        {
            UNiagaraComponent* EffectComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                this,
                Effect,
                Location,
                FRotator::ZeroRotator);
            if (EffectComponent)
            {
                EffectComponent->SetWorldScale3D(FVector(bTactical ? 0.34f : 0.28f));
            }
            const bool bSpawned = EffectComponent != nullptr;
            if (bVFXProbe)
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian VFX: event=%s system=%s spawned=%s"),
                    bTactical ? TEXT("throwable_suppression") : TEXT("throwable_explosion"),
                    *Effect->GetPathName(),
                    bSpawned ? TEXT("yes") : TEXT("no"));
            }
        }
        if (!bTactical)
        {
            if (UNiagaraSystem* Dust = VFX->Dust.LoadSynchronous();
                ShouldSpawnConfiguredNiagara(Dust))
            {
                UNiagaraComponent* DustComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                    this,
                    Dust,
                    Location,
                    FRotator::ZeroRotator);
                if (DustComponent)
                {
                    DustComponent->SetWorldScale3D(FVector(0.24f));
                }
                const bool bSpawned = DustComponent != nullptr;
                if (bVFXProbe)
                {
                    UE_LOG(
                        LogTemp,
                        Display,
                        TEXT("Black Meridian VFX: event=throwable_dust system=%s spawned=%s"),
                        *Dust->GetPathName(),
                        bSpawned ? TEXT("yes") : TEXT("no"));
                }
            }
            if (UNiagaraSystem* Smoke = VFX->Smoke.LoadSynchronous();
                ShouldSpawnConfiguredNiagara(Smoke))
            {
                UNiagaraComponent* SmokeComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                    this,
                    Smoke,
                    Location,
                    FRotator::ZeroRotator);
                if (SmokeComponent)
                {
                    SmokeComponent->SetWorldScale3D(FVector(0.22f));
                }
                const bool bSpawned = SmokeComponent != nullptr;
                if (bVFXProbe)
                {
                    UE_LOG(
                        LogTemp,
                        Display,
                        TEXT("Black Meridian VFX: event=throwable_smoke system=%s spawned=%s"),
                        *Smoke->GetPathName(),
                        bSpawned ? TEXT("yes") : TEXT("no"));
                }
            }
        }
    }

    // Keep the player-facing event readable on low-feature or headless paths;
    // configured Niagara is the primary presentation and this fallback is
    // additive rather than a replacement for the authored effect.
    StartFallbackDetonationPresentation(bTactical);
}

void ABMThrowable::ApplyPresentationMaterial()
{
    if (!Mesh)
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

    PresentationMaterial = Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMaterial);
    if (!PresentationMaterial)
    {
        return;
    }

    const bool bTactical = ThrowableId == EBMWeaponId::VeilGrenade;
    const FLinearColor BaseColor = bTactical
        ? FLinearColor(0.015f, 0.26f, 0.55f, 1.0f)
        : FLinearColor(0.52f, 0.055f, 0.012f, 1.0f);
    const FLinearColor EmissiveColor = bTactical
        ? FLinearColor(0.01f, 0.12f, 0.36f, 1.0f)
        : FLinearColor(0.18f, 0.012f, 0.002f, 1.0f);
    PresentationMaterial->SetVectorParameterValue(TEXT("Color"), BaseColor);
    PresentationMaterial->SetVectorParameterValue(TEXT("BaseColor"), BaseColor);
    PresentationMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveColor);
    PresentationMaterial->SetScalarParameterValue(TEXT("Metallic"), 0.58f);
    PresentationMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.34f);
    Mesh->SetVisibility(true, true);
}

void ABMThrowable::StartFallbackDetonationPresentation(const bool bTactical)
{
    if (GetNetMode() == NM_DedicatedServer || !DetonationFlashMesh || !GetWorld())
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

    if (BaseMaterial)
    {
        DetonationMaterial = DetonationFlashMesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMaterial);
        if (DetonationMaterial)
        {
            const FLinearColor BurstColor = bTactical
                ? FLinearColor(0.02f, 0.40f, 1.0f, 1.0f)
                : FLinearColor(1.0f, 0.11f, 0.015f, 1.0f);
            DetonationMaterial->SetVectorParameterValue(TEXT("Color"), BurstColor);
            DetonationMaterial->SetVectorParameterValue(TEXT("BaseColor"), BurstColor);
            DetonationMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), BurstColor);
            DetonationMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), 8.0f);
        }

        if (DetonationRingMesh)
        {
            DetonationRingMaterial = DetonationRingMesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMaterial);
            if (DetonationRingMaterial)
            {
                const FLinearColor RingColor = bTactical
                    ? FLinearColor(0.01f, 0.30f, 0.95f, 1.0f)
                    : FLinearColor(1.0f, 0.18f, 0.015f, 1.0f);
                DetonationRingMaterial->SetVectorParameterValue(TEXT("Color"), RingColor);
                DetonationRingMaterial->SetVectorParameterValue(TEXT("BaseColor"), RingColor);
                DetonationRingMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), RingColor);
                DetonationRingMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), 6.0f);
            }
        }
    }

    bDetonationTactical = bTactical;
    DetonationLight->SetLightColor(
        bTactical
            ? FLinearColor(0.02f, 0.35f, 1.0f)
            : FLinearColor(1.0f, 0.08f, 0.01f));
    DetonationFlashMesh->SetRelativeScale3D(FVector(0.25f));
    DetonationFlashMesh->SetVisibility(true, true);
    if (DetonationRingMesh)
    {
        DetonationRingMesh->SetRelativeScale3D(FVector(0.18f, 0.18f, bTactical ? 0.028f : 0.055f));
        DetonationRingMesh->SetVisibility(true, true);
    }
    DetonationLight->SetVisibility(true, true);
    DetonationPresentationElapsed = 0.0f;
    bDetonationPresentationActive = true;
    SetActorTickEnabled(true);
}

void ABMThrowable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABMThrowable, ThrowableId);
}
