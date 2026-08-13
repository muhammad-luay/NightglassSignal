// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMGameUserSettings.h"

void UBMGameUserSettings::SetToDefaults()
{
    Super::SetToDefaults();
    HorizontalSensitivity = 1.0f;
    VerticalSensitivity = 1.0f;
    HipFireSensitivity = 1.0f;
    ADSSensitivity = 0.75f;
    bToggleADS = false;
    bToggleCrouch = false;
    bToggleSprint = false;
    FieldOfView = 90.0f;
    bReducedCameraMotion = false;
    bReducedMotionBlur = false;
    SubtitleSize = 1;
    ColorBlindPreset = 0;
    bGoreReduction = false;
    GraphicsQualityPreset = 2;
    bFriendlyFireEnabled = false;
    MasterVolume = 1.0f;
    MusicVolume = 0.8f;
    EffectsVolume = 1.0f;
    DialogueVolume = 1.0f;
    AudioMix.Reset();
    MasterSoundClass.Reset();
    MusicSoundClass.Reset();
    EffectsSoundClass.Reset();
    DialogueSoundClass.Reset();
}
