// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/InputSettings.h"

namespace BMInputRemap
{
    /** Result of validating and constructing a keyboard/mouse remap. */
    enum class ERemapResult : uint8
    {
        Applied,
        InvalidRequest,
        NonRemappableAction,
        ReservedConflict,
        AmbiguousConflict,
        MissingSwapSource
    };

    /** The actions exposed by the compact in-game controls screen. */
    BLACKMERIDIANRUNTIME_API const TArray<FName>& GetRemappableActions();

    /**
     * Builds a complete action-map result without mutating UInputSettings.
     *
     * Keyboard/mouse mappings can be swapped only between actions in the
     * remappable list. Gamepad mappings are copied unchanged, and a key used
     * by a non-remappable action is rejected instead of being stolen.
     */
    BLACKMERIDIANRUNTIME_API ERemapResult BuildKeyboardRemap(
        const TArray<FInputActionKeyMapping>& ExistingMappings,
        FName ActionName,
        const FKey& NewKey,
        TArray<FInputActionKeyMapping>& OutMappings,
        FString& OutReason);

    BLACKMERIDIANRUNTIME_API const TCHAR* LexToString(ERemapResult Result);
}
