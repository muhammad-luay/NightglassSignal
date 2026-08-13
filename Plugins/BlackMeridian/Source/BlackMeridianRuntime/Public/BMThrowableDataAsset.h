// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BMCoreTypes.h"
#include "BMThrowableDataAsset.generated.h"

class UBMAudioDataAsset;
class UBMVFXDataAsset;
class UStaticMesh;

/** Designer-owned tuning and presentation contract for a throwable. */
UCLASS(BlueprintType)
class BLACKMERIDIANRUNTIME_API UBMThrowableDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    EBMWeaponId ThrowableId = EBMWeaponId::FragGrenade;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0.05"))
    float FuseSeconds = 2.4f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0.0"))
    float Damage = 110.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "1.0"))
    float Radius = 360.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance|Tactical", meta = (ClampMin = "0.0"))
    float SuppressionAmount = 6.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "1.0"))
    float ThrowSpeed = 950.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
    float GravityScale = 0.7f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
    TSoftObjectPtr<UStaticMesh> Mesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
    TSoftObjectPtr<UBMAudioDataAsset> AudioProfile;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
    TSoftObjectPtr<UBMVFXDataAsset> VFXProfile;

    /** Creates the source-only profile used when the content package is absent. */
    static UBMThrowableDataAsset* CreateRuntimeProfile(UObject* Outer, EBMWeaponId InThrowableId);

    /** Loads the serialized profile and falls back only when content is absent. */
    static UBMThrowableDataAsset* LoadContentProfile(UObject* Outer, EBMWeaponId InThrowableId);
};
