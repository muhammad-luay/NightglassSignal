// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMProjectile.h"

#include "BMCombatCharacter.h"
#include "BMCombatTuning.h"
#include "BMEnemyCharacter.h"
#include "BMPlayerCharacter.h"
#include "BMAudioDataAsset.h"
#include "BMGameMode.h"
#include "BMGameUserSettings.h"
#include "BMHealthComponent.h"
#include "BMSquadSubsystem.h"
#include "BMVFXDataAsset.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    /**
     * Project-owned Niagara systems are the normal player-facing baseline.
     * Engine content stays probe-only, while cooked project effects remain on
     * the regular projectile path so launcher fire and detonation have a
     * visible result without relying on debug geometry.
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
}

ABMProjectile::ABMProjectile()
{
    bReplicates = true;
    SetReplicateMovement(true);
    InitialLifeSpan = 8.0f;
    PrimaryActorTick.bCanEverTick = false;

    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    SetRootComponent(Collision);
    Collision->InitSphereRadius(8.0f);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Collision->SetCollisionObjectType(ECC_WorldDynamic);
    Collision->SetCollisionResponseToAllChannels(ECR_Block);
    // Projectiles must still detonate on pawns, but they are not solid cover.
    // Use an overlap for the pawn channel and keep blocking responses for
    // world geometry, so CharacterMovement cannot become pinned by a round.
    Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Collision->SetGenerateOverlapEvents(true);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(Collision);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetRelativeScale3D(FVector(0.08f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        Mesh->SetStaticMesh(SphereMesh.Object);
    }

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->UpdatedComponent = Collision;
    ProjectileMovement->InitialSpeed = 1200.0f;
    ProjectileMovement->MaxSpeed = 1200.0f;
    ProjectileMovement->ProjectileGravityScale = 0.35f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce = false;
}

void ABMProjectile::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner())
    {
        Collision->IgnoreActorWhenMoving(GetOwner(), true);
    }

    // CharacterMovement performs its own capsule sweep. Without this
    // reciprocal ignore, an incoming round can become a WorldDynamic body
    // that pins the player against the floor for several frames, which reads
    // like a failed wall collision and prevents a valid route around cover.
    // This does not change projectile hit resolution: the projectile's
    // UpdatedComponent still collides with the pawn and HandleHit detonates.
    if (GetWorld())
    {
        for (TActorIterator<ABMCombatCharacter> Iterator(GetWorld()); Iterator; ++Iterator)
        {
            ABMCombatCharacter* Character = *Iterator;
            if (Character && Character != GetOwner() && Character->GetCapsuleComponent())
            {
                Character->GetCapsuleComponent()->IgnoreActorWhenMoving(this, true);
            }
        }
    }
    Collision->OnComponentHit.AddDynamic(this, &ABMProjectile::HandleHit);
    Collision->OnComponentBeginOverlap.AddDynamic(this, &ABMProjectile::HandleBeginOverlap);
}

void ABMProjectile::InitializeProjectile(
    const FVector& Direction,
    const float Speed,
    const float InDamage,
    const float InRadius,
    const float InGravityScale)
{
    if (!HasAuthority())
    {
        return;
    }

    Damage = FMath::Max(0.0f, InDamage);
    Radius = FMath::Max(0.0f, InRadius);
    ProjectileMovement->InitialSpeed = FMath::Max(1.0f, Speed);
    ProjectileMovement->MaxSpeed = ProjectileMovement->InitialSpeed;
    ProjectileMovement->ProjectileGravityScale = FMath::Max(0.0f, InGravityScale);
    ProjectileMovement->Velocity = Direction.GetSafeNormal() * ProjectileMovement->InitialSpeed;
}

void ABMProjectile::SetPlayerCrosshairDamageConstraint(
    const bool bEnabled,
    AActor* CrosshairTarget)
{
    if (!HasAuthority())
    {
        return;
    }

    bPlayerCrosshairDamageConstrained = bEnabled;
    PlayerCrosshairTarget = CrosshairTarget;
}

void ABMProjectile::HandleHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    const FVector NormalImpulse,
    const FHitResult& Hit)
{
    if (!HasAuthority() || bDetonated)
    {
        return;
    }

    const FVector DetonationLocation = Hit.ImpactPoint.IsNearlyZero()
        ? GetActorLocation()
        : FVector(Hit.ImpactPoint);
    Detonate(DetonationLocation);
}

void ABMProjectile::HandleBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    const int32 OtherBodyIndex,
    const bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!HasAuthority() || bDetonated || !OtherActor || OtherActor == GetOwner())
    {
        return;
    }

    // Only combat pawns should trigger the non-blocking pawn hit path. World
    // surfaces continue through HandleHit so the projectile remains a real
    // collision-bearing round rather than a visual tracer.
    if (!Cast<ABMCombatCharacter>(OtherActor))
    {
        return;
    }

    const FVector DetonationLocation = SweepResult.ImpactPoint.IsNearlyZero()
        ? GetActorLocation()
        : FVector(SweepResult.ImpactPoint);
    Detonate(DetonationLocation);
}

void ABMProjectile::Detonate(const FVector& Location)
{
    if (bDetonated)
    {
        return;
    }
    bDetonated = true;

    if (ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>())
    {
        GameMode->RegisterCombatLocation(Location);
    }

    const float SafeRadius = FMath::Max(1.0f, Radius);
    DrawDebugSphere(GetWorld(), Location, SafeRadius, 20, FColor::Orange, false, 0.45f, 0, 1.5f);

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BlackMeridianProjectile), false, this);
    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
    GetWorld()->OverlapMultiByObjectType(
        Overlaps,
        Location,
        FQuat::Identity,
        ObjectParams,
        FCollisionShape::MakeSphere(SafeRadius),
        QueryParams);

    const ABMCombatCharacter* Thrower = Cast<ABMCombatCharacter>(GetOwner());
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Actor = Overlap.GetActor();
        UBMHealthComponent* Health = Actor ? Actor->FindComponentByClass<UBMHealthComponent>() : nullptr;
        if (!Actor || !Health)
        {
            continue;
        }

        // A player-fired projectile may still detonate against world geometry
        // and show its blast, but it cannot turn the launcher's large splash
        // radius into aim assist. Only the first actor resolved by the
        // authoritative/client crosshair validation may receive character
        // damage. A null target intentionally means that an off-target shot
        // damages no character at all.
        if (bPlayerCrosshairDamageConstrained && Actor != PlayerCrosshairTarget.Get())
        {
#if !UE_BUILD_SHIPPING
            if (FParse::Param(FCommandLine::Get(), TEXT("BM_AimValidationProbe")))
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian combat validation: projectile damage blocked off_crosshair actor=%s target=%s"),
                    *GetNameSafe(Actor),
                    *GetNameSafe(PlayerCrosshairTarget.Get()));
            }
#endif
            continue;
        }

        const ABMCombatCharacter* Target = Cast<ABMCombatCharacter>(Actor);
        if (Thrower && Target && Thrower->GetTeam() == Target->GetTeam())
        {
            const UBMGameUserSettings* Settings = GEngine ? Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
            if (!Settings || !Settings->bFriendlyFireEnabled)
            {
                continue;
            }
        }

        const float DistanceScale = 1.0f - FMath::Clamp(FVector::Dist(Location, Actor->GetActorLocation()) / SafeRadius, 0.0f, 1.0f) * 0.7f;
        const ABMEnemyCharacter* EnemyThrower = Cast<ABMEnemyCharacter>(Thrower);
        const bool bHostilePlayerHit = EnemyThrower && Cast<ABMPlayerCharacter>(Actor);
        const float HostileDamageMultiplier = bHostilePlayerHit
            ? EnemyThrower->GetDamageToPlayerMultiplier()
            : 1.0f;
        FBMDamageContext DamageContext;
        DamageContext.RawDamage = Damage;
        DamageContext.FinalDamage = Damage * DistanceScale * HostileDamageMultiplier;
        if (bHostilePlayerHit)
        {
            DamageContext.FinalDamage = FMath::Min(
                DamageContext.FinalDamage,
                BMCombatTuning::HostileExplosionDamageCap);
            if (UBMSquadSubsystem* Squad = GetWorld()->GetSubsystem<UBMSquadSubsystem>())
            {
                DamageContext.FinalDamage = Squad->ConsumeHostileDamageBudget(
                    Actor,
                    DamageContext.FinalDamage,
                    EnemyThrower->UsesReadableCombatPacing()
                        ? BMCombatTuning::OpeningWaveDamageWindowCap
                        : BMCombatTuning::HostileDamageWindowCap);
            }
        }
        if (DamageContext.FinalDamage <= KINDA_SMALL_NUMBER)
        {
            continue;
        }
        DamageContext.HitZone = EBMHitZone::Torso;
        DamageContext.InstigatorActor = GetOwner();
        // Keep player-fired projectile character damage covered by the same
        // health-boundary contract. The actor-equality gate above is the
        // projectile equivalent of the hitscan final crosshair gate; splash
        // presentation remains unrestricted while off-target character
        // damage is rejected.
        const bool bPlayerWeaponDamage = bPlayerCrosshairDamageConstrained
            && Cast<ABMPlayerCharacter>(Thrower) != nullptr;
        DamageContext.bPlayerWeaponDamage = bPlayerWeaponDamage;
        DamageContext.bPlayerWeaponAimValidated = bPlayerWeaponDamage
            && Actor == PlayerCrosshairTarget.Get();
        Health->ApplyDamage(DamageContext.FinalDamage, DamageContext);
        if (ABMCombatCharacter* CombatTarget = Cast<ABMCombatCharacter>(Actor))
        {
            FVector SourceDirection = (Actor->GetActorLocation() - Location).GetSafeNormal();
            if (SourceDirection.IsNearlyZero())
            {
                SourceDirection = GetActorForwardVector();
            }
            CombatTarget->MulticastHitReaction(SourceDirection, DamageContext.HitZone);
        }
    }

    MulticastDetonation(Location);
    Destroy();
}

void ABMProjectile::MulticastDetonation_Implementation(const FVector_NetQuantize Location)
{
    if (GetNetMode() == NM_DedicatedServer || !GetWorld())
    {
        return;
    }

    if (UBMAudioDataAsset* Audio = AudioProfile.LoadSynchronous())
    {
        if (USoundBase* Explosion = Audio->Explosion.LoadSynchronous())
        {
            UGameplayStatics::PlaySoundAtLocation(this, Explosion, Location);
        }
    }
    if (UBMVFXDataAsset* VFX = VFXProfile.LoadSynchronous())
    {
        const bool bVFXProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_VFXProbe"));
        if (UNiagaraSystem* Explosion = VFX->Explosion.LoadSynchronous();
            ShouldSpawnConfiguredNiagara(Explosion))
        {
            UNiagaraComponent* ExplosionComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                this,
                Explosion,
                Location,
                FRotator::ZeroRotator);
            if (ExplosionComponent)
            {
                ExplosionComponent->SetWorldScale3D(FVector(0.28f));
            }
            const bool bSpawned = ExplosionComponent != nullptr;
            if (bVFXProbe)
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian VFX: event=projectile_explosion system=%s spawned=%s"),
                    *Explosion->GetPathName(),
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
                SmokeComponent->SetWorldScale3D(FVector(0.24f));
            }
            const bool bSpawned = SmokeComponent != nullptr;
            if (bVFXProbe)
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian VFX: event=projectile_smoke system=%s spawned=%s"),
                    *Smoke->GetPathName(),
                    bSpawned ? TEXT("yes") : TEXT("no"));
            }
        }
    }
}

void ABMProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABMProjectile, Damage);
    DOREPLIFETIME(ABMProjectile, Radius);
}
