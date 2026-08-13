// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BMVFXDataAsset.generated.h"

class UNiagaraSystem;
class UMaterialInterface;

UCLASS(BlueprintType)
class BLACKMERIDIANRUNTIME_API UBMVFXDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    TSoftObjectPtr<UNiagaraSystem> MuzzleFlash;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    TSoftObjectPtr<UNiagaraSystem> BulletImpact;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    TSoftObjectPtr<UNiagaraSystem> BloodImpact;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    TSoftObjectPtr<UMaterialInterface> ImpactDecal;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    TSoftObjectPtr<UNiagaraSystem> Explosion;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World")
    TSoftObjectPtr<UNiagaraSystem> Dust;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World")
    TSoftObjectPtr<UNiagaraSystem> Smoke;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World")
    TSoftObjectPtr<UNiagaraSystem> Sparks;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World")
    TSoftObjectPtr<UNiagaraSystem> ShellEjection;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
    TSoftObjectPtr<UNiagaraSystem> Suppression;
};
