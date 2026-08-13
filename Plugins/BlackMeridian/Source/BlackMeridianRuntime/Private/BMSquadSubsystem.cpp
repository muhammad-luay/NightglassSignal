// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMSquadSubsystem.h"

#include "BMCombatTuning.h"
#include "BMEnemyCharacter.h"
#include "Engine/World.h"

void UBMSquadSubsystem::RegisterEnemy(ABMEnemyCharacter* Enemy)
{
    if (Enemy)
    {
        Enemies.AddUnique(Enemy);
    }
}

void UBMSquadSubsystem::UnregisterEnemy(ABMEnemyCharacter* Enemy)
{
    Enemies.Remove(Enemy);
}

void UBMSquadSubsystem::ReportEnemyDeath(AActor* Reporter, const FVector& Location)
{
    if (!Reporter || !GetWorld() || Location.IsNearlyZero())
    {
        return;
    }

    RecentEnemyDeathLocation = Location;
    RecentEnemyDeathExpiry = GetWorld()->GetTimeSeconds() + 3.5;
}

void UBMSquadSubsystem::ShareTarget(AActor* Reporter, AActor* Target, const FVector& LastKnownLocation)
{
    if (!Reporter)
    {
        return;
    }
    SharedTarget = Target;
    SharedTargetLocation = LastKnownLocation;
    SharedTargetExpiry = GetWorld() ? GetWorld()->GetTimeSeconds() + 4.0 : 0.0;
}

bool UBMSquadSubsystem::TryClaimFireSlot(AActor* Target, const float MinimumInterval)
{
    if (!GetWorld() || !IsValid(Target))
    {
        return false;
    }

    const double Now = GetWorld()->GetTimeSeconds();
    // Keep the map bounded as campaign waves are destroyed and respawned.
    for (auto It = FireSlotExpiryByTarget.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || It.Value() < Now - 2.0)
        {
            It.RemoveCurrent();
        }
    }

    const TWeakObjectPtr<AActor> TargetKey(Target);
    double& NextAllowedFireTime = FireSlotExpiryByTarget.FindOrAdd(TargetKey);
    if (Now < NextAllowedFireTime)
    {
        return false;
    }

    // Never let a caller accidentally collapse the player-facing budget back
    // into a frame-perfect volley. Individual weapons still control their own
    // cadence after they win this shared slot.
    NextAllowedFireTime = Now + FMath::Clamp(static_cast<double>(MinimumInterval), 0.30, 1.25);
    return true;
}

float UBMSquadSubsystem::ConsumeHostileDamageBudget(
    AActor* Target,
    const float RequestedDamage,
    const float DamageWindowCap)
{
    if (!GetWorld() || !IsValid(Target) || RequestedDamage <= KINDA_SMALL_NUMBER)
    {
        return RequestedDamage;
    }

    const double Now = GetWorld()->GetTimeSeconds();
    // Keep the budget map bounded as campaign waves are destroyed and
    // respawned. A short-lived target entry is safe to forget entirely.
    for (auto It = HostileDamageBudgetByTarget.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid()
            || (Now - It.Value().LastUpdateTime) > (BMCombatTuning::HostileDamageWindowSeconds * 3.0f))
        {
            It.RemoveCurrent();
        }
    }

    const TWeakObjectPtr<AActor> TargetKey(Target);
    FHostileDamageBudget& Budget = HostileDamageBudgetByTarget.FindOrAdd(TargetKey);
    const float RequestedWindowCap = FMath::Clamp(
        DamageWindowCap,
        1.0f,
        BMCombatTuning::HostileDamageWindowCap);
    if (Budget.LastUpdateTime <= 0.0)
    {
        Budget.LastUpdateTime = Now;
        Budget.DamageWindowCap = RequestedWindowCap;
        Budget.AvailableDamage = Budget.DamageWindowCap;
    }
    else
    {
        // A target can move from the readable opening profile into a later,
        // higher-pressure wave without restarting the world. Keep the highest
        // requested cap seen for that target, but do not retroactively refill
        // the bucket when the cap widens; only elapsed time replenishes
        // available damage.
        Budget.DamageWindowCap = FMath::Max(Budget.DamageWindowCap, RequestedWindowCap);
        const float Elapsed = FMath::Max(0.0f, static_cast<float>(Now - Budget.LastUpdateTime));
        const float Replenished = Elapsed
            * (Budget.DamageWindowCap / BMCombatTuning::HostileDamageWindowSeconds);
        Budget.AvailableDamage = FMath::Min(
            Budget.DamageWindowCap,
            Budget.AvailableDamage + Replenished);
        Budget.LastUpdateTime = Now;
    }

    const float GrantedDamage = FMath::Min(RequestedDamage, Budget.AvailableDamage);
    Budget.AvailableDamage = FMath::Max(0.0f, Budget.AvailableDamage - GrantedDamage);
    return GrantedDamage;
}

bool UBMSquadSubsystem::GetSharedTarget(
    AActor* Requester,
    TWeakObjectPtr<AActor>& OutTarget,
    FVector& OutLastKnownLocation) const
{
    if (!GetWorld() || !Requester || GetWorld()->GetTimeSeconds() > SharedTargetExpiry)
    {
        return false;
    }

    OutTarget = SharedTarget;
    OutLastKnownLocation = SharedTargetLocation;
    return OutTarget.IsValid() || !OutLastKnownLocation.IsNearlyZero();
}

bool UBMSquadSubsystem::GetRecentEnemyDeathLocation(AActor* Requester, FVector& OutLocation) const
{
    if (!GetWorld() || !Requester || GetWorld()->GetTimeSeconds() > RecentEnemyDeathExpiry || RecentEnemyDeathLocation.IsNearlyZero())
    {
        return false;
    }

    OutLocation = RecentEnemyDeathLocation;
    return true;
}
