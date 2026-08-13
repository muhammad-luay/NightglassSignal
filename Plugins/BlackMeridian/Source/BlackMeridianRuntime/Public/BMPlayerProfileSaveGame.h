// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "BMCoreTypes.h"
#include "BMPlayerProfileSaveGame.generated.h"

UCLASS()
class BLACKMERIDIANRUNTIME_API UBMPlayerProfileSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    FString Callsign = TEXT("NIGHTGLASS");

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    int32 CampaignCompletionCount = 0;

    /** Campaign variants completed at least once; independent of the checkpoint slot. */
    UPROPERTY(BlueprintReadWrite, Category = "Profile|Campaign")
    TArray<EBMCampaignLevel> CompletedCampaignLevels;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    int32 MultiplayerMatches = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    TArray<EBMWeaponId> UnlockedWeapons = {EBMWeaponId::ARX4, EBMWeaponId::Kestrel9, EBMWeaponId::SolacePistol};

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    TArray<FName> UnlockedAttachments;
};
