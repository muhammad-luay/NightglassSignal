// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BMCoreTypes.h"
#include "BMCharacterAnimationInterface.generated.h"

UINTERFACE(BlueprintType)
class BLACKMERIDIANRUNTIME_API UBMCharacterAnimationInterface : public UInterface
{
    GENERATED_BODY()
};

/** Presentation contract shared by first-person and third-person characters. */
class BLACKMERIDIANRUNTIME_API IBMCharacterAnimationInterface
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Black Meridian|Animation")
    void SetCharacterAnimationState(EBMCharacterMovementState NewState, float GroundSpeed, bool bAiming);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Black Meridian|Animation")
    void SetCharacterAimOffset(FRotator AimRotation);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Black Meridian|Animation")
    void PlayCharacterAnimation(FName AnimationEvent);
};
