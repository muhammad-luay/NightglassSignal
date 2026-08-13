// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "BMCoreTypes.h"
#include "BMCheckpointSaveGame.generated.h"

UCLASS()
class BLACKMERIDIANRUNTIME_API UBMCheckpointSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, Category = "Checkpoint|Campaign")
    EBMCampaignLevel CampaignLevel = EBMCampaignLevel::SableQuay;

    UPROPERTY(BlueprintReadWrite, Category = "Checkpoint")
    int32 CheckpointIndex = 0;

    /** The mission phase to show when a failed attempt is restarted. Older
     * saves leave this at Insertion and are handled by the mission director's
     * compatibility fallback. */
    UPROPERTY(BlueprintReadWrite, Category = "Checkpoint")
    EBMMissionPhase ResumePhase = EBMMissionPhase::Insertion;

    UPROPERTY(BlueprintReadWrite, Category = "Checkpoint")
    bool bOptionalIntelSecured = false;

    UPROPERTY(BlueprintReadWrite, Category = "Checkpoint")
    bool bSignalJammerPlanted = false;

    UPROPERTY(BlueprintReadWrite, Category = "Checkpoint")
    float PlayerHealth = 100.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Checkpoint")
    TArray<EBMWeaponId> Loadout;

    UPROPERTY(BlueprintReadWrite, Category = "Checkpoint")
    EBMWeaponId CurrentWeapon = EBMWeaponId::ARX4;

    UPROPERTY(BlueprintReadWrite, Category = "Checkpoint")
    FVector PlayerLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Checkpoint")
    FRotator PlayerRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "Checkpoint")
    bool bHasPlayerTransform = false;

    UPROPERTY(BlueprintReadWrite, Category = "Checkpoint|Equipment")
    int32 FragCount = 2;

    UPROPERTY(BlueprintReadWrite, Category = "Checkpoint|Equipment")
    int32 VeilCount = 1;
};
