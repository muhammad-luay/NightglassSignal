// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BMCoreTypes.h"
#include "BMSurfaceRuleDataAsset.generated.h"

UCLASS(BlueprintType)
class BLACKMERIDIANRUNTIME_API UBMSurfaceRuleDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface")
    EBMSurfaceType SurfaceType = EBMSurfaceType::Concrete;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface")
    bool bPenetrable = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface")
    float DamageMultiplier = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface")
    int32 MaxPenetrations = 0;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Surface")
    bool AllowsPenetration(int32 CurrentPenetrationCount) const
    {
        return bPenetrable && CurrentPenetrationCount < FMath::Max(0, MaxPenetrations);
    }
};
