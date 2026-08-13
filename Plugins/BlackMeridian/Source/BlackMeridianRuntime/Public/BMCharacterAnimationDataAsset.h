// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/BlendSpace.h"
#include "Engine/DataAsset.h"
#include "BMCharacterAnimationDataAsset.generated.h"

class UAnimMontage;
class UAnimSequence;
class UAnimInstance;
class UBlendSpace;

/**
 * Optional authored animation package. Gameplay only emits state/event names;
 * a Blueprint animation graph can consume the same contract when assets exist.
 */
UCLASS(BlueprintType)
class BLACKMERIDIANRUNTIME_API UBMCharacterAnimationDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Graph")
    TSubclassOf<UAnimInstance> AnimBlueprintClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Graph")
    TSoftObjectPtr<UBlendSpace> LocomotionBlendSpace;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Graph")
    TSoftObjectPtr<UBlendSpace> AimOffsetBlendSpace;

    /**
     * Serialized locomotion clips used by source-only characters that do not
     * have an authored AnimBP yet. The keys are semantic state names such as
     * Idle, Walk, Sprint, Jump, and Fall.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Locomotion")
    TMap<FName, TSoftObjectPtr<UAnimSequence>> LocomotionSequences;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Montages")
    TMap<FName, TSoftObjectPtr<UAnimMontage>> EventMontages;

    /**
     * Authored one-shot clips for characters whose presentation is driven by
     * explicit sequence playback. Event names share the gameplay contract:
     * Fire, ReloadStarted, HitReact, HeadHitReact, and Death are examples.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Sequences")
    TMap<FName, TSoftObjectPtr<UAnimSequence>> EventSequences;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Animation")
    TSoftObjectPtr<UAnimSequence> FindLocomotionSequence(FName StateName) const
    {
        const TSoftObjectPtr<UAnimSequence>* Sequence = LocomotionSequences.Find(StateName);
        return Sequence ? *Sequence : TSoftObjectPtr<UAnimSequence>();
    }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Animation")
    TSoftObjectPtr<UAnimMontage> FindEventMontage(FName AnimationEvent) const
    {
        const TSoftObjectPtr<UAnimMontage>* Montage = EventMontages.Find(AnimationEvent);
        return Montage ? *Montage : TSoftObjectPtr<UAnimMontage>();
    }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Animation")
    TSoftObjectPtr<UAnimSequence> FindEventSequence(FName AnimationEvent) const
    {
        const TSoftObjectPtr<UAnimSequence>* Sequence = EventSequences.Find(AnimationEvent);
        return Sequence ? *Sequence : TSoftObjectPtr<UAnimSequence>();
    }
};
