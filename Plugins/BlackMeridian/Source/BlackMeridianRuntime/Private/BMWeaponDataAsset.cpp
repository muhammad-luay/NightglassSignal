// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMWeaponDataAsset.h"

#include "BMProjectile.h"
#include "BMSurfaceRuleDataAsset.h"

namespace
{
    const TCHAR* GetWeaponProfileAssetPath(const EBMWeaponId WeaponId)
    {
        switch (WeaponId)
        {
        case EBMWeaponId::Kestrel9:
            return TEXT("/Game/Data/Weapons/DA_BM_Kestrel9_SMG.DA_BM_Kestrel9_SMG");
        case EBMWeaponId::RookDMR:
            return TEXT("/Game/Data/Weapons/DA_BM_Rook_DMR.DA_BM_Rook_DMR");
        case EBMWeaponId::Breach12:
            return TEXT("/Game/Data/Weapons/DA_BM_Breach12_Shotgun.DA_BM_Breach12_Shotgun");
        case EBMWeaponId::SolacePistol:
            return TEXT("/Game/Data/Weapons/DA_BM_Solace_Pistol.DA_BM_Solace_Pistol");
        case EBMWeaponId::ArcLauncher:
            return TEXT("/Game/Data/Weapons/DA_BM_Arc7_Launcher.DA_BM_Arc7_Launcher");
        case EBMWeaponId::ARX4:
            return TEXT("/Game/Data/Weapons/DA_BM_ARX4_Rifle.DA_BM_ARX4_Rifle");
        case EBMWeaponId::FragGrenade:
        case EBMWeaponId::VeilGrenade:
        default:
            return nullptr;
        }
    }
}

float UBMWeaponDataAsset::GetHitZoneMultiplier(const EBMHitZone HitZone) const
{
    switch (HitZone)
    {
    case EBMHitZone::Head:
        return HeadMultiplier;
    case EBMHitZone::Arm:
        return ArmMultiplier;
    case EBMHitZone::Leg:
        return LegMultiplier;
    case EBMHitZone::Torso:
    default:
        return TorsoMultiplier;
    }
}

UBMSurfaceRuleDataAsset* UBMWeaponDataAsset::FindSurfaceRule(const EBMSurfaceType SurfaceType) const
{
    const TObjectPtr<UBMSurfaceRuleDataAsset>* Rule = SurfaceRules.Find(SurfaceType);
    return Rule ? Rule->Get() : nullptr;
}

UBMWeaponDataAsset* UBMWeaponDataAsset::CreateRuntimeProfile(UObject* Outer, const EBMWeaponId InWeaponId)
{
    UBMWeaponDataAsset* Profile = NewObject<UBMWeaponDataAsset>(Outer);
    Profile->WeaponId = InWeaponId;
    Profile->Balance = FBMWeaponBalance{};

    UBMSurfaceRuleDataAsset* SoftCoverRule = NewObject<UBMSurfaceRuleDataAsset>(Profile);
    SoftCoverRule->SurfaceType = EBMSurfaceType::SoftCover;
    SoftCoverRule->bPenetrable = true;
    SoftCoverRule->DamageMultiplier = Profile->Balance.PenetrationMultiplier;
    SoftCoverRule->MaxPenetrations = 1;
    Profile->SurfaceRules.Add(EBMSurfaceType::SoftCover, SoftCoverRule);

    switch (InWeaponId)
    {
    case EBMWeaponId::Kestrel9:
        Profile->DisplayName = NSLOCTEXT("BlackMeridian", "Kestrel9", "Kestrel-9");
        Profile->FireMode = EBMWeaponFireMode::FullyAutomatic;
        Profile->Balance.BaseDamage = 20.0f;
        Profile->Balance.FireRateRPM = 850.0f;
        Profile->Balance.MagazineSize = 32;
        Profile->Balance.ReserveAmmo = 160;
        // Keep the SMG responsive in close pressure. Empty reloads still have
        // a readable extra beat without taking the player out of the fight.
        Profile->Balance.ReloadDuration = 0.40f;
        Profile->Balance.EmptyReloadDuration = 0.55f;
        Profile->Balance.RangeMeters = 55.0f;
        Profile->Balance.FalloffStartMeters = 18.0f;
        Profile->Balance.FalloffEndMeters = 55.0f;
        Profile->Balance.HipSpreadDegrees = 4.2f;
        Profile->Balance.ADSSpreadDegrees = 0.8f;
        Profile->Balance.AccuracyRecoveryDegreesPerSecond = 8.0f;
        Profile->Balance.ShotSpreadPenaltyDegrees = 0.45f;
        Profile->Balance.MaxAccuracyPenaltyDegrees = 5.0f;
        Profile->Balance.WeaponSwayAmount = 1.3f;
        Profile->Balance.WeaponSwayFrequency = 9.0f;
        Profile->Balance.ADSSwayMultiplier = 0.45f;
        Profile->Balance.RecoilPitch = 0.55f;
        Profile->Balance.RecoilYaw = 0.45f;
        break;
    case EBMWeaponId::RookDMR:
        Profile->DisplayName = NSLOCTEXT("BlackMeridian", "RookDMR", "Rook DMR");
        Profile->FireMode = EBMWeaponFireMode::SemiAutomatic;
        Profile->Balance.BaseDamage = 58.0f;
        Profile->Balance.FireRateRPM = 260.0f;
        Profile->Balance.MagazineSize = 12;
        Profile->Balance.ReserveAmmo = 72;
        Profile->Balance.ReloadDuration = 0.70f;
        Profile->Balance.EmptyReloadDuration = 0.90f;
        Profile->Balance.RangeMeters = 140.0f;
        Profile->Balance.FalloffStartMeters = 70.0f;
        Profile->Balance.FalloffEndMeters = 140.0f;
        Profile->Balance.HipSpreadDegrees = 5.0f;
        Profile->Balance.ADSSpreadDegrees = 0.25f;
        Profile->Balance.AccuracyRecoveryDegreesPerSecond = 4.5f;
        Profile->Balance.ShotSpreadPenaltyDegrees = 0.7f;
        Profile->Balance.MaxAccuracyPenaltyDegrees = 3.5f;
        Profile->Balance.WeaponSwayAmount = 1.6f;
        Profile->Balance.WeaponSwayFrequency = 5.0f;
        Profile->Balance.ADSSwayMultiplier = 0.25f;
        Profile->Balance.RecoilPitch = 1.8f;
        Profile->Balance.RecoilYaw = 0.35f;
        break;
    case EBMWeaponId::Breach12:
        Profile->DisplayName = NSLOCTEXT("BlackMeridian", "Breach12", "Breach-12");
        Profile->FireMode = EBMWeaponFireMode::SemiAutomatic;
        Profile->Balance.BaseDamage = 16.0f;
        Profile->Balance.FireRateRPM = 90.0f;
        Profile->Balance.MagazineSize = 8;
        Profile->Balance.ReserveAmmo = 48;
        Profile->Balance.ReloadDuration = 0.80f;
        Profile->Balance.EmptyReloadDuration = 1.05f;
        Profile->Balance.RangeMeters = 35.0f;
        Profile->Balance.FalloffStartMeters = 10.0f;
        Profile->Balance.FalloffEndMeters = 35.0f;
        Profile->Balance.HipSpreadDegrees = 7.0f;
        Profile->Balance.ADSSpreadDegrees = 4.0f;
        Profile->Balance.AccuracyRecoveryDegreesPerSecond = 3.5f;
        Profile->Balance.ShotSpreadPenaltyDegrees = 0.9f;
        Profile->Balance.MaxAccuracyPenaltyDegrees = 6.0f;
        Profile->Balance.WeaponSwayAmount = 1.9f;
        Profile->Balance.WeaponSwayFrequency = 4.0f;
        Profile->Balance.ADSSwayMultiplier = 0.6f;
        Profile->Balance.PelletCount = 8;
        Profile->Balance.RecoilPitch = 3.2f;
        Profile->Balance.RecoilYaw = 0.65f;
        break;
    case EBMWeaponId::SolacePistol:
        Profile->DisplayName = NSLOCTEXT("BlackMeridian", "SolacePistol", "Solace Pistol");
        Profile->FireMode = EBMWeaponFireMode::SemiAutomatic;
        Profile->Balance.BaseDamage = 34.0f;
        Profile->Balance.FireRateRPM = 420.0f;
        Profile->Balance.MagazineSize = 15;
        Profile->Balance.ReserveAmmo = 90;
        Profile->Balance.ReloadDuration = 0.35f;
        Profile->Balance.EmptyReloadDuration = 0.50f;
        Profile->Balance.RangeMeters = 60.0f;
        Profile->Balance.FalloffStartMeters = 22.0f;
        Profile->Balance.FalloffEndMeters = 60.0f;
        Profile->Balance.HipSpreadDegrees = 3.2f;
        Profile->Balance.ADSSpreadDegrees = 0.7f;
        Profile->Balance.AccuracyRecoveryDegreesPerSecond = 9.0f;
        Profile->Balance.ShotSpreadPenaltyDegrees = 0.25f;
        Profile->Balance.MaxAccuracyPenaltyDegrees = 3.0f;
        Profile->Balance.WeaponSwayAmount = 0.8f;
        Profile->Balance.WeaponSwayFrequency = 8.0f;
        Profile->Balance.ADSSwayMultiplier = 0.4f;
        Profile->Balance.RecoilPitch = 1.0f;
        Profile->Balance.RecoilYaw = 0.45f;
        break;
    case EBMWeaponId::ArcLauncher:
        Profile->DisplayName = NSLOCTEXT("BlackMeridian", "ArcLauncher", "Arc-7 Launcher");
        Profile->FireMode = EBMWeaponFireMode::SemiAutomatic;
        Profile->Balance.BaseDamage = 85.0f;
        Profile->Balance.FireRateRPM = 55.0f;
        Profile->Balance.MagazineSize = 1;
        Profile->Balance.ReserveAmmo = 12;
        Profile->Balance.ReloadDuration = 0.85f;
        Profile->Balance.EmptyReloadDuration = 0.90f;
        Profile->Balance.RangeMeters = 120.0f;
        Profile->Balance.FalloffStartMeters = 40.0f;
        Profile->Balance.FalloffEndMeters = 120.0f;
        Profile->Balance.HipSpreadDegrees = 1.0f;
        Profile->Balance.ADSSpreadDegrees = 0.5f;
        Profile->Balance.AccuracyRecoveryDegreesPerSecond = 3.0f;
        Profile->Balance.ShotSpreadPenaltyDegrees = 0.55f;
        Profile->Balance.MaxAccuracyPenaltyDegrees = 4.0f;
        Profile->Balance.WeaponSwayAmount = 1.6f;
        Profile->Balance.WeaponSwayFrequency = 4.0f;
        Profile->Balance.ADSSwayMultiplier = 0.3f;
        Profile->Balance.PelletCount = 1;
        Profile->Balance.RecoilPitch = 2.5f;
        Profile->Balance.RecoilYaw = 0.4f;
        Profile->bProjectileWeapon = true;
        Profile->ProjectileClass = ABMProjectile::StaticClass();
        Profile->ProjectileSpeed = 1000.0f;
        Profile->ProjectileGravityScale = 0.42f;
        Profile->ProjectileRadius = 240.0f;
        break;
    case EBMWeaponId::ARX4:
    default:
        Profile->DisplayName = NSLOCTEXT("BlackMeridian", "ARX4", "ARX-4");
        Profile->FireMode = EBMWeaponFireMode::FullyAutomatic;
        Profile->Balance.BaseDamage = 30.0f;
        Profile->Balance.FireRateRPM = 650.0f;
        Profile->Balance.MagazineSize = 30;
        Profile->Balance.ReserveAmmo = 150;
        // ARX-4 is the default loadout. Keep the reload inside a short
        // tactical decision window; an empty reload remains meaningfully
        // slower without making the player sit out of the encounter.
        Profile->Balance.ReloadDuration = 0.45f;
        Profile->Balance.EmptyReloadDuration = 0.65f;
        Profile->Balance.RangeMeters = 95.0f;
        Profile->Balance.FalloffStartMeters = 35.0f;
        Profile->Balance.FalloffEndMeters = 95.0f;
        Profile->Balance.HipSpreadDegrees = 3.0f;
        Profile->Balance.ADSSpreadDegrees = 0.55f;
        Profile->Balance.AccuracyRecoveryDegreesPerSecond = 6.5f;
        Profile->Balance.ShotSpreadPenaltyDegrees = 0.35f;
        Profile->Balance.MaxAccuracyPenaltyDegrees = 4.0f;
        Profile->Balance.WeaponSwayAmount = 1.0f;
        Profile->Balance.WeaponSwayFrequency = 7.0f;
        Profile->Balance.ADSSwayMultiplier = 0.35f;
        Profile->Balance.RecoilPitch = 0.85f;
        Profile->Balance.RecoilYaw = 0.3f;
        break;
    }

    return Profile;
}

UBMWeaponDataAsset* UBMWeaponDataAsset::LoadContentProfile(UObject* Outer, const EBMWeaponId InWeaponId)
{
    if (const TCHAR* AssetPath = GetWeaponProfileAssetPath(InWeaponId))
    {
        if (UBMWeaponDataAsset* ContentProfile = LoadObject<UBMWeaponDataAsset>(nullptr, AssetPath))
        {
            if (ContentProfile->WeaponId == InWeaponId)
            {
#if !UE_BUILD_SHIPPING
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian weapon profile: loaded serialized asset=%s id=%d"),
                    AssetPath,
                    static_cast<int32>(InWeaponId));
#endif
                return ContentProfile;
            }

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Black Meridian weapon profile id mismatch asset=%s expected=%d actual=%d; using fallback"),
                AssetPath,
                static_cast<int32>(InWeaponId),
                static_cast<int32>(ContentProfile->WeaponId));
        }
    }

    // A source-only or partially cooked checkout remains playable. The
    // serialized asset is authoritative whenever it exists; this profile is
    // strictly the missing-content bridge described by the project docs.
    return CreateRuntimeProfile(Outer ? Outer : GetTransientPackage(), InWeaponId);
}
