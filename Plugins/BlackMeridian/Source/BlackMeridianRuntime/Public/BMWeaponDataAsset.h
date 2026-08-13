// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BMCoreTypes.h"
#include "BMWeaponDataAsset.generated.h"

class USoundBase;
class ABMProjectile;
class UBMAudioDataAsset;
class UBMVFXDataAsset;
class UBMSurfaceRuleDataAsset;
class UStaticMesh;

/** Designer-owned weapon tuning. Runtime weapon actors never own balance constants. */
UCLASS(BlueprintType)
class BLACKMERIDIANRUNTIME_API UBMWeaponDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    EBMWeaponId WeaponId = EBMWeaponId::ARX4;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    EBMWeaponFireMode FireMode = EBMWeaponFireMode::FullyAutomatic;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    bool bSuppressed = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance")
    FBMWeaponBalance Balance;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
    bool bProjectileWeapon = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (EditCondition = "bProjectileWeapon"))
    TSubclassOf<ABMProjectile> ProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (EditCondition = "bProjectileWeapon", ClampMin = "0.0"))
    float ProjectileSpeed = 1200.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (EditCondition = "bProjectileWeapon", ClampMin = "0.0"))
    float ProjectileGravityScale = 0.35f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (EditCondition = "bProjectileWeapon", ClampMin = "0.0"))
    float ProjectileRadius = 220.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance")
    float HeadMultiplier = 2.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance")
    float TorsoMultiplier = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance")
    float ArmMultiplier = 0.75f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance")
    float LegMultiplier = 0.65f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attachments")
    TArray<FName> SupportedAttachmentSlots = {TEXT("Optic"), TEXT("Muzzle"), TEXT("Magazine"), TEXT("Grip"), TEXT("Stock")};

    /** Replaceable project-owned or production third-person weapon mesh. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Mesh")
    TSoftObjectPtr<UStaticMesh> ThirdPersonMesh;

    /** Optional first-person mesh; the procedural kit remains the fallback. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Mesh")
    TSoftObjectPtr<UStaticMesh> FirstPersonMesh;

    /** Socket contract owned by the weapon asset rather than the character. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Mesh")
    FName GripSocketName = TEXT("Grip");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Mesh")
    FName MuzzleSocketName = TEXT("Muzzle");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
    TSoftObjectPtr<USoundBase> FireSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
    TSoftObjectPtr<USoundBase> SuppressedFireSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
    TSoftObjectPtr<USoundBase> DryFireSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Audio")
    TSoftObjectPtr<UBMAudioDataAsset> AudioProfile;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|VFX")
    TSoftObjectPtr<UBMVFXDataAsset> VFXProfile;

    /** Physical-material rules are optional; the runtime tag fallback remains available for greybox. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface")
    TMap<EBMSurfaceType, TObjectPtr<UBMSurfaceRuleDataAsset>> SurfaceRules;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapons")
    float GetHitZoneMultiplier(EBMHitZone HitZone) const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapons|Surface")
    UBMSurfaceRuleDataAsset* FindSurfaceRule(EBMSurfaceType SurfaceType) const;

    /** Creates a placeholder profile when Content/Data assets have not been authored yet. */
    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Weapons")
    static UBMWeaponDataAsset* CreateRuntimeProfile(UObject* Outer, EBMWeaponId InWeaponId);

    /** Loads the serialized profile and falls back only when content is absent. */
    static UBMWeaponDataAsset* LoadContentProfile(UObject* Outer, EBMWeaponId InWeaponId);
};
