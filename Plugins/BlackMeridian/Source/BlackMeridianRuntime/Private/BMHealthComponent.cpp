// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMHealthComponent.h"

#include "BMCombatTuning.h"
#include "BMEnemyCharacter.h"
#include "BMPlayerCharacter.h"
#include "BMSignalBotController.h"
#include "GameFramework/Actor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"

namespace
{
    bool IsHostilePlayerDamage(const UBMHealthComponent* HealthComponent, const FBMDamageContext& DamageContext)
    {
        return HealthComponent
            && Cast<ABMPlayerCharacter>(HealthComponent->GetOwner())
            && Cast<ABMEnemyCharacter>(DamageContext.InstigatorActor);
    }
}

UBMHealthComponent::UBMHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UBMHealthComponent::BeginPlay()
{
    Super::BeginPlay();
    Health = FMath::Clamp(Health, 0.0f, MaxHealth);
    LastReplicatedHealth = Health;
    CriticalRecoveryTimeRemaining = 0.0f;
    bCriticalRecoveryUsed = false;
}

float UBMHealthComponent::ApplyDamage(const float Damage, const FBMDamageContext& DamageContext)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Damage <= 0.0f || !IsAlive())
    {
        return 0.0f;
    }

    const ABMPlayerCharacter* PlayerInstigator = Cast<ABMPlayerCharacter>(DamageContext.InstigatorActor);
    const bool bSignalBotInstigator = PlayerInstigator
        && Cast<ABMSignalBotController>(PlayerInstigator->GetController()) != nullptr;
    const bool bPlayerInstigator = PlayerInstigator != nullptr && !bSignalBotInstigator;
    const bool bEnemyTarget = Cast<ABMEnemyCharacter>(GetOwner()) != nullptr;
    const bool bUnclassifiedPlayerEnemyDamage = bPlayerInstigator
        && bEnemyTarget
        && !DamageContext.bPlayerWeaponDamage
        && !DamageContext.bPlayerMeleeDamage
        && !DamageContext.bPlayerThrowableDamage;

    // The weapon trace is not the only caller that can eventually reach this
    // component. Keep the final mutation boundary fail-closed so a player
    // weapon context cannot damage a character unless the same request has
    // explicitly completed authoritative crosshair validation.
    if (bUnclassifiedPlayerEnemyDamage
        || (DamageContext.bPlayerWeaponDamage && !DamageContext.bPlayerWeaponAimValidated))
    {
#if !UE_BUILD_SHIPPING
        if (FParse::Param(FCommandLine::Get(), TEXT("BM_AimValidationProbe"))
            || FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe")))
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian combat validation: health boundary blocked player damage target=%s instigator=%s requested=%.1f weapon=%s aim_validated=%s melee=%s throwable=%s"),
                *GetNameSafe(GetOwner()),
                *GetNameSafe(DamageContext.InstigatorActor),
                Damage,
                DamageContext.bPlayerWeaponDamage ? TEXT("yes") : TEXT("no"),
                DamageContext.bPlayerWeaponAimValidated ? TEXT("yes") : TEXT("no"),
                DamageContext.bPlayerMeleeDamage ? TEXT("yes") : TEXT("no"),
                DamageContext.bPlayerThrowableDamage ? TEXT("yes") : TEXT("no"));
        }
#endif
        return 0.0f;
    }

    const bool bHostilePlayerDamage = IsHostilePlayerDamage(this, DamageContext);
    const bool bCriticalRecoveryActive = CriticalRecoveryTimeRemaining > 0.0f;
    float EffectiveDamage = bHostilePlayerDamage
        ? BMCombatTuning::ResolvePlayerHostileDamage(Health, Damage, bCriticalRecoveryActive)
        : Damage;

    // The opening bucket and per-shot cap make this branch rare, but keep the
    // recovery contract correct for a future hostile weapon or grenade profile
    // that can still cross the entire remaining health bar in one action.
    const bool bWouldBeLethal = bHostilePlayerDamage && Health - EffectiveDamage <= 0.0f;
    if (bWouldBeLethal && !bCriticalRecoveryUsed)
    {
        EffectiveDamage = FMath::Max(
            0.0f,
            Health - BMCombatTuning::PlayerCriticalRecoveryMinimumHealth);
    }

    const float PreviousHealth = Health;
    Health = FMath::Clamp(Health - EffectiveDamage, 0.0f, MaxHealth);
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe")))
    {
        const ABMCombatCharacter* OwnerCharacter = Cast<ABMCombatCharacter>(GetOwner());
        const ABMCombatCharacter* InstigatorCharacter = Cast<ABMCombatCharacter>(DamageContext.InstigatorActor);
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian role damage: target=%s target_class=%s target_team=%d instigator=%s instigator_class=%s instigator_team=%d raw=%.1f requested=%.1f applied=%.1f health_before=%.1f health_after=%.1f hit_zone=%d headshot=%s"),
            *GetNameSafe(GetOwner()),
            *GetNameSafe(GetOwner()->GetClass()),
            OwnerCharacter ? static_cast<int32>(OwnerCharacter->GetTeam()) : -1,
            *GetNameSafe(DamageContext.InstigatorActor),
            DamageContext.InstigatorActor ? *GetNameSafe(DamageContext.InstigatorActor->GetClass()) : TEXT("None"),
            InstigatorCharacter ? static_cast<int32>(InstigatorCharacter->GetTeam()) : -1,
            DamageContext.RawDamage,
            Damage,
            EffectiveDamage,
            PreviousHealth,
            Health,
            static_cast<int32>(DamageContext.HitZone),
            DamageContext.bHeadshot ? TEXT("yes") : TEXT("no"));
    }
#endif
    if (bHostilePlayerDamage
        && !bCriticalRecoveryUsed
        && (bWouldBeLethal || Health <= BMCombatTuning::PlayerCriticalHealthThreshold))
    {
        bCriticalRecoveryUsed = true;
        CriticalRecoveryTimeRemaining = BMCombatTuning::PlayerCriticalRecoveryDurationSeconds;
        // A hit during the recovery beat is still reported to the HUD, but it
        // must not keep postponing the recovery tick every frame.
        TimeSinceDamage = RegenerationDelay;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat: player critical recovery started health=%.1f duration=%.2f damage=%.1f"),
            Health,
            CriticalRecoveryTimeRemaining,
            PreviousHealth - Health);
    }
    LastReplicatedHealth = Health;
    if (!(bHostilePlayerDamage && CriticalRecoveryTimeRemaining > 0.0f))
    {
        TimeSinceDamage = 0.0f;
    }
    const float AppliedDamage = PreviousHealth - Health;
    OnHealthChanged.Broadcast(Health, AppliedDamage);

    if (Health <= 0.0f)
    {
        OnDeath.Broadcast(DamageContext);
    }

    return AppliedDamage;
}

void UBMHealthComponent::RestoreHealth(const float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0.0f || !IsAlive())
    {
        return;
    }

    Health = FMath::Clamp(Health + Amount, 0.0f, MaxHealth);
    LastReplicatedHealth = Health;
    OnHealthChanged.Broadcast(Health, 0.0f);
    if (Health >= MaxHealth - KINDA_SMALL_NUMBER)
    {
        bCriticalRecoveryUsed = false;
    }
}

void UBMHealthComponent::SetHealthForCheckpoint(const float InHealth)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    Health = FMath::Clamp(InHealth, 1.0f, MaxHealth);
    LastReplicatedHealth = Health;
    TimeSinceDamage = 0.0f;
    CriticalRecoveryTimeRemaining = 0.0f;
    bCriticalRecoveryUsed = false;
    OnHealthChanged.Broadcast(Health, 0.0f);
}

void UBMHealthComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (GetOwner() && GetOwner()->HasAuthority() && CriticalRecoveryTimeRemaining > 0.0f)
    {
        CriticalRecoveryTimeRemaining = FMath::Max(
            0.0f,
            CriticalRecoveryTimeRemaining - FMath::Max(0.0f, DeltaTime));
        const float RecoveryCap = FMath::Min(
            MaxHealth,
            BMCombatTuning::PlayerCriticalRecoveryHealthCap);
        if (IsAlive() && Health < RecoveryCap)
        {
            Health = FMath::Min(
                RecoveryCap,
                Health + BMCombatTuning::PlayerCriticalRecoveryPerSecond * FMath::Max(0.0f, DeltaTime));
            LastReplicatedHealth = Health;
            OnHealthChanged.Broadcast(Health, 0.0f);
        }
        // Keep the ordinary regeneration path open after the critical pulse;
        // a player who reaches cover can continue recovering normally.
        TimeSinceDamage = FMath::Max(TimeSinceDamage, RegenerationDelay);
    }

    if (GetOwner()
        && GetOwner()->HasAuthority()
        && bRegenerateHealth
        && CriticalRecoveryTimeRemaining <= 0.0f
        && IsAlive()
        && Health < MaxHealth)
    {
        TimeSinceDamage += DeltaTime;
        if (TimeSinceDamage >= RegenerationDelay)
        {
            RestoreHealth(RegenerationPerSecond * DeltaTime);
        }
    }
}

void UBMHealthComponent::OnRep_Health()
{
    const float DamageTaken = FMath::Max(0.0f, LastReplicatedHealth - Health);
    LastReplicatedHealth = Health;
    OnHealthChanged.Broadcast(Health, DamageTaken);
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe")))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat probe: client health replicated owner=%s health=%.1f"),
            *GetNameSafe(GetOwner()),
            Health);
    }
#endif
}

void UBMHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UBMHealthComponent, Health);
}
