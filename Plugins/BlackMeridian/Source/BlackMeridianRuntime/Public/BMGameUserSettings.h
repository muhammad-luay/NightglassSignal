// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "BMGameUserSettings.generated.h"

class USoundClass;
class USoundMix;

UCLASS(config = Game, BlueprintType)
class BLACKMERIDIANRUNTIME_API UBMGameUserSettings : public UGameUserSettings
{
    GENERATED_BODY()

public:
    virtual void SetToDefaults() override;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Controls")
    float HorizontalSensitivity = 1.0f;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Controls")
    float VerticalSensitivity = 1.0f;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Controls")
    float HipFireSensitivity = 1.0f;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Controls")
    float ADSSensitivity = 0.75f;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Controls")
    bool bToggleADS = false;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Controls")
    bool bToggleCrouch = false;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Controls")
    bool bToggleSprint = false;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
    float FieldOfView = 90.0f;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
    bool bReducedCameraMotion = false;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
    bool bReducedMotionBlur = false;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
    int32 SubtitleSize = 1;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
    int32 ColorBlindPreset = 0;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
    bool bGoreReduction = false;

    /** 0=Low, 1=Medium, 2=High, 3=Epic. Applied through UGameUserSettings scalability levels. */
    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Graphics", meta = (ClampMin = "0", ClampMax = "3"))
    int32 GraphicsQualityPreset = 2;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
    bool bFriendlyFireEnabled = false;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
    float MasterVolume = 1.0f;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
    float MusicVolume = 0.8f;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
    float EffectsVolume = 1.0f;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
    float DialogueVolume = 1.0f;

    /** Optional authored routing assets. Empty in the source-only greybox. */
    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
    TSoftObjectPtr<USoundMix> AudioMix;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
    TSoftObjectPtr<USoundClass> MasterSoundClass;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
    TSoftObjectPtr<USoundClass> MusicSoundClass;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
    TSoftObjectPtr<USoundClass> EffectsSoundClass;

    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
    TSoftObjectPtr<USoundClass> DialogueSoundClass;
};
