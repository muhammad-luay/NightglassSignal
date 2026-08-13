// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMInputRemap.h"

namespace BMInputRemap
{
    namespace
    {
        bool IsKeyboardOrMouseMapping(const FInputActionKeyMapping& Mapping)
        {
            return Mapping.Key.IsValid() && !Mapping.Key.IsGamepadKey();
        }
    }

    const TArray<FName>& GetRemappableActions()
    {
        static const TArray<FName> Actions = {
            TEXT("Fire"),
            TEXT("Aim"),
            TEXT("Reload"),
            TEXT("Interact"),
            TEXT("Sprint"),
            TEXT("Crouch"),
            TEXT("Prone"),
            TEXT("Jump"),
            TEXT("Melee"),
            TEXT("Frag"),
            TEXT("Tactical"),
            TEXT("TogglePause"),
            TEXT("Scoreboard")};
        return Actions;
    }

    ERemapResult BuildKeyboardRemap(
        const TArray<FInputActionKeyMapping>& ExistingMappings,
        const FName ActionName,
        const FKey& NewKey,
        TArray<FInputActionKeyMapping>& OutMappings,
        FString& OutReason)
    {
        OutMappings = ExistingMappings;
        OutReason.Reset();

        if (ActionName.IsNone() || !NewKey.IsValid() || NewKey.IsGamepadKey())
        {
            OutReason = TEXT("keyboard/mouse remap request is invalid");
            return ERemapResult::InvalidRequest;
        }

        if (!GetRemappableActions().Contains(ActionName))
        {
            OutReason = FString::Printf(TEXT("action %s is reserved"), *ActionName.ToString());
            return ERemapResult::NonRemappableAction;
        }

        bool bHasCurrentKeyboardMapping = false;
        FInputActionKeyMapping CurrentMapping;
        TArray<FName> ConflictingActions;

        for (const FInputActionKeyMapping& Mapping : ExistingMappings)
        {
            if (!IsKeyboardOrMouseMapping(Mapping))
            {
                continue;
            }

            if (Mapping.ActionName == ActionName)
            {
                if (!bHasCurrentKeyboardMapping)
                {
                    CurrentMapping = Mapping;
                    bHasCurrentKeyboardMapping = true;
                }
                continue;
            }

            if (Mapping.Key != NewKey)
            {
                continue;
            }

            if (!GetRemappableActions().Contains(Mapping.ActionName))
            {
                OutReason = FString::Printf(
                    TEXT("key %s is reserved by %s"),
                    *NewKey.GetDisplayName().ToString(),
                    *Mapping.ActionName.ToString());
                return ERemapResult::ReservedConflict;
            }

            ConflictingActions.AddUnique(Mapping.ActionName);
        }

        if (ConflictingActions.Num() > 1)
        {
            OutReason = FString::Printf(
                TEXT("key %s is already shared by multiple remappable actions"),
                *NewKey.GetDisplayName().ToString());
            return ERemapResult::AmbiguousConflict;
        }

        if (ConflictingActions.Num() == 1 && !bHasCurrentKeyboardMapping)
        {
            OutReason = FString::Printf(
                TEXT("cannot swap key %s because %s has no keyboard source key"),
                *NewKey.GetDisplayName().ToString(),
                *ActionName.ToString());
            return ERemapResult::MissingSwapSource;
        }

        const FName ConflictingAction = ConflictingActions.Num() == 1
            ? ConflictingActions[0]
            : NAME_None;

        OutMappings.RemoveAll([ActionName](const FInputActionKeyMapping& Mapping)
        {
            return Mapping.ActionName == ActionName && IsKeyboardOrMouseMapping(Mapping);
        });

        if (!ConflictingAction.IsNone())
        {
            OutMappings.RemoveAll([ConflictingAction](const FInputActionKeyMapping& Mapping)
            {
                return Mapping.ActionName == ConflictingAction && IsKeyboardOrMouseMapping(Mapping);
            });

            // Retain any modifier flags from the source mapping while moving
            // that source key to the other remappable action.
            CurrentMapping.ActionName = ConflictingAction;
            OutMappings.Add(CurrentMapping);
        }

        OutMappings.Add(FInputActionKeyMapping(ActionName, NewKey));
        return ERemapResult::Applied;
    }

    const TCHAR* LexToString(const ERemapResult Result)
    {
        switch (Result)
        {
        case ERemapResult::Applied:
            return TEXT("Applied");
        case ERemapResult::InvalidRequest:
            return TEXT("InvalidRequest");
        case ERemapResult::NonRemappableAction:
            return TEXT("NonRemappableAction");
        case ERemapResult::ReservedConflict:
            return TEXT("ReservedConflict");
        case ERemapResult::AmbiguousConflict:
            return TEXT("AmbiguousConflict");
        case ERemapResult::MissingSwapSource:
            return TEXT("MissingSwapSource");
        default:
            return TEXT("Unknown");
        }
    }
}
