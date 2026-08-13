// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BMWeaponAttachmentDataAsset.generated.h"

class UStaticMesh;

/**
 * Compact attachment state replicated with the weapon. UObject data assets are
 * convenient on the authority, but the asset pointer itself is not a reliable
 * multiplayer contract for a runtime-created or source-only attachment. The
 * stable id and modifier values keep every client visually and mechanically in
 * sync, while MeshPath remains optional for authored content.
 */
USTRUCT(BlueprintType)
struct BLACKMERIDIANRUNTIME_API FBMWeaponAttachmentState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Attachment")
    FName Slot = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Attachment")
    FName AttachmentId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Modifiers")
    float DamageMultiplier = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Modifiers")
    float ADSFOVOffset = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Modifiers")
    float RecoilMultiplier = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Modifiers")
    bool bSuppressor = false;

    UPROPERTY(BlueprintReadOnly, Category = "Presentation")
    FString MeshPath;
};

UCLASS(BlueprintType)
class BLACKMERIDIANRUNTIME_API UBMWeaponAttachmentDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attachment")
    FName AttachmentId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attachment")
    FName Slot = TEXT("Optic");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attachment")
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Modifiers")
    float DamageMultiplier = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Modifiers")
    float ADSFOVOffset = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Modifiers")
    float RecoilMultiplier = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Modifiers")
    bool bSuppressor = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
    TSoftObjectPtr<UStaticMesh> Mesh;

    /** Source-only/built-in attachment profiles used by pickups and probes. */
    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Weapons|Attachments")
    static UBMWeaponAttachmentDataAsset* CreateRuntimeProfile(UObject* Outer, FName InAttachmentId);
};
