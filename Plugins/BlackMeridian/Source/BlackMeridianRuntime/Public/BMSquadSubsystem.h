// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BMSquadSubsystem.generated.h"

class ABMEnemyCharacter;
class AActor;

UCLASS()
class BLACKMERIDIANRUNTIME_API UBMSquadSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    void RegisterEnemy(ABMEnemyCharacter* Enemy);
    void UnregisterEnemy(ABMEnemyCharacter* Enemy);
    void ReportEnemyDeath(AActor* Reporter, const FVector& Location);
    void ShareTarget(AActor* Reporter, AActor* Target, const FVector& LastKnownLocation);
    /**
     * Claims the next hostile-fire slot for a target.  AI still owns its
     * weapon cadence, but this squad-level 550 ms budget prevents five
     * independent decision timers from producing an unreadable burst on the
     * player in the same frame.
     */
    bool TryClaimFireSlot(AActor* Target, float MinimumInterval = 0.55f);

    /**
     * Applies one shared token-bucket budget to hostile damage aimed at a
     * player target. Muzzle, tracer, and hit feedback still happen when the
     * bucket is empty; only authoritative health subtraction is limited.
     */
    float ConsumeHostileDamageBudget(AActor* Target, float RequestedDamage, float DamageWindowCap);

    bool GetSharedTarget(AActor* Requester, TWeakObjectPtr<AActor>& OutTarget, FVector& OutLastKnownLocation) const;
    bool GetRecentEnemyDeathLocation(AActor* Requester, FVector& OutLocation) const;

private:
    UPROPERTY()
    TArray<TObjectPtr<ABMEnemyCharacter>> Enemies;

    TWeakObjectPtr<AActor> SharedTarget;
    FVector SharedTargetLocation = FVector::ZeroVector;
    double SharedTargetExpiry = 0.0;
    TMap<TWeakObjectPtr<AActor>, double> FireSlotExpiryByTarget;

    struct FHostileDamageBudget
    {
        double LastUpdateTime = 0.0;
        float AvailableDamage = 0.0f;
        float DamageWindowCap = 0.0f;
    };

    TMap<TWeakObjectPtr<AActor>, FHostileDamageBudget> HostileDamageBudgetByTarget;
    FVector RecentEnemyDeathLocation = FVector::ZeroVector;
    double RecentEnemyDeathExpiry = 0.0;
};
