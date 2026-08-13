// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BMCombatCharacter.h"
#include "BMFriendlySquadMember.generated.h"

class ABMEnemyCharacter;
class ABMInteractableActor;
class ABMPlayerCharacter;

UCLASS(Blueprintable)
class BLACKMERIDIANRUNTIME_API ABMFriendlySquadMember : public ABMCombatCharacter
{
    GENERATED_BODY()

public:
    ABMFriendlySquadMember(const FObjectInitializer& ObjectInitializer);

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void RestoreForCheckpoint(float InHealth) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
    FText Callsign;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
    FVector FormationOffset = FVector(-180.0f, 100.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Combat", meta = (ClampMin = "0.0"))
    // Squad fire should create pressure and support the player without
    // silently clearing the first tutorial wave before the player understands
    // that shooting is the main action.
    float SupportDamagePerShot = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Combat", meta = (ClampMin = "0.0"))
    float EngagementRange = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Combat", meta = (ClampMin = "0.1"))
    float EngagementFireInterval = 1.1f;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Squad")
    void AssistPlayer();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Squad")
    void EngageVisibleEnemy();

protected:
    virtual void HandleDeath(const FBMDamageContext& DamageContext) override;

private:
    void AssistDownedSquadmate();
    FVector GetObjectiveAnchor() const;
    FVector FindCoverLocation(const FVector& ThreatLocation) const;

    TWeakObjectPtr<ABMPlayerCharacter> PlayerTarget;
    TWeakObjectPtr<ABMEnemyCharacter> CurrentEnemyTarget;
    TWeakObjectPtr<ABMInteractableActor> RevivePrompt;
    float AssistCooldown = 0.0f;
    float FireCooldown = 0.0f;
};
