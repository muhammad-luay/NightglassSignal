// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BMCoreTypes.h"
#include "BMAudioDataAsset.generated.h"

class USoundBase;

/** Audio routing contract; missing cues fall back to silence in the source-only build. */
UCLASS(BlueprintType)
class BLACKMERIDIANRUNTIME_API UBMAudioDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gunfire")
    TSoftObjectPtr<USoundBase> IndoorGunshot;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gunfire")
    TSoftObjectPtr<USoundBase> OutdoorGunshot;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gunfire")
    TSoftObjectPtr<USoundBase> SuppressedGunshot;

    /** Optional empty-chamber cue; the runtime pulse remains the fallback. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gunfire")
    TSoftObjectPtr<USoundBase> DryFire;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impacts")
    TSoftObjectPtr<USoundBase> BulletImpact;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impacts")
    TMap<EBMSurfaceType, TSoftObjectPtr<USoundBase>> SurfaceImpacts;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
    TSoftObjectPtr<USoundBase> Footstep;

    /** Optional surface-specific footsteps; Footstep remains the fallback. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
    TMap<EBMSurfaceType, TSoftObjectPtr<USoundBase>> SurfaceFootsteps;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    TSoftObjectPtr<USoundBase> Reload;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    TSoftObjectPtr<USoundBase> Equipment;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    TSoftObjectPtr<USoundBase> Explosion;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dialogue")
    TSoftObjectPtr<USoundBase> PositionalDialogue;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World")
    TSoftObjectPtr<USoundBase> AmbientBattlefield;

    /** Per-operation environmental beds; AmbientBattlefield remains the fallback. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World|Environment")
    TMap<FName, TSoftObjectPtr<USoundBase>> EnvironmentAmbiences;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Music")
    TSoftObjectPtr<USoundBase> MusicState;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Music")
    TMap<FName, TSoftObjectPtr<USoundBase>> MusicStates;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
    TSoftObjectPtr<USoundBase> LowHealth;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
    TSoftObjectPtr<USoundBase> Suppression;

    /** Optional incoming-grenade warning; the runtime pulse remains the fallback. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
    TSoftObjectPtr<USoundBase> ThrowableWarning;

    /** Optional local confirmation cues; generated project cues remain the fallback. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
    TSoftObjectPtr<USoundBase> HitConfirm;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
    TSoftObjectPtr<USoundBase> Elimination;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Audio")
    TSoftObjectPtr<USoundBase> FindMusicState(FName StateId) const
    {
        if (const TSoftObjectPtr<USoundBase>* StateSound = MusicStates.Find(StateId))
        {
            return *StateSound;
        }
        return MusicState;
    }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Audio")
    TSoftObjectPtr<USoundBase> FindFootstep(EBMSurfaceType SurfaceType) const
    {
        if (const TSoftObjectPtr<USoundBase>* SurfaceSound = SurfaceFootsteps.Find(SurfaceType))
        {
            return *SurfaceSound;
        }
        return Footstep;
    }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Audio")
    TSoftObjectPtr<USoundBase> FindEnvironmentAmbience(FName EnvironmentId) const
    {
        if (const TSoftObjectPtr<USoundBase>* EnvironmentSound = EnvironmentAmbiences.Find(EnvironmentId))
        {
            return *EnvironmentSound;
        }
        return AmbientBattlefield;
    }
};
