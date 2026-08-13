// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BMWeaponInterfaces.generated.h"

UINTERFACE(BlueprintType)
class BLACKMERIDIANRUNTIME_API UBMWeaponAnimationInterface : public UInterface
{
    GENERATED_BODY()
};

class BLACKMERIDIANRUNTIME_API IBMWeaponAnimationInterface
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Black Meridian|Weapon|Animation")
    void PlayWeaponAnimation(FName AnimationEvent);
};

UINTERFACE(BlueprintType)
class BLACKMERIDIANRUNTIME_API UBMWeaponAudioInterface : public UInterface
{
    GENERATED_BODY()
};

class BLACKMERIDIANRUNTIME_API IBMWeaponAudioInterface
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Black Meridian|Weapon|Audio")
    void PlayWeaponAudio(FName AudioEvent, bool bSuppressed);
};
