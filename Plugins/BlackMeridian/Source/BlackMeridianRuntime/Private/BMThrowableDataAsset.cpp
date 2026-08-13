// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMThrowableDataAsset.h"

namespace
{
    const TCHAR* GetThrowableProfileAssetPath(const EBMWeaponId ThrowableId)
    {
        switch (ThrowableId)
        {
        case EBMWeaponId::VeilGrenade:
            return TEXT("/Game/Data/Throwables/DA_BM_Veil_Grenade.DA_BM_Veil_Grenade");
        case EBMWeaponId::FragGrenade:
            return TEXT("/Game/Data/Throwables/DA_BM_Frag_Grenade.DA_BM_Frag_Grenade");
        case EBMWeaponId::ARX4:
        case EBMWeaponId::Kestrel9:
        case EBMWeaponId::RookDMR:
        case EBMWeaponId::Breach12:
        case EBMWeaponId::SolacePistol:
        case EBMWeaponId::ArcLauncher:
        default:
            return nullptr;
        }
    }
}

UBMThrowableDataAsset* UBMThrowableDataAsset::CreateRuntimeProfile(
    UObject* Outer,
    const EBMWeaponId InThrowableId)
{
    UBMThrowableDataAsset* Profile = NewObject<UBMThrowableDataAsset>(Outer ? Outer : GetTransientPackage());
    Profile->ThrowableId = InThrowableId;
    Profile->Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
        InThrowableId == EBMWeaponId::VeilGrenade
            ? TEXT("/Game/Weapons/Meshes/SM_BM_Veil_Grenade.SM_BM_Veil_Grenade")
            : TEXT("/Game/Weapons/Meshes/SM_BM_Frag_Grenade.SM_BM_Frag_Grenade")));

    if (InThrowableId == EBMWeaponId::VeilGrenade)
    {
        Profile->DisplayName = NSLOCTEXT("BlackMeridian", "VeilGrenade", "Veil Tactical Grenade");
        Profile->FuseSeconds = 1.6f;
        Profile->Damage = 0.0f;
        Profile->Radius = 420.0f;
        Profile->SuppressionAmount = 8.0f;
        Profile->ThrowSpeed = 900.0f;
        Profile->GravityScale = 0.65f;
    }
    else
    {
        Profile->ThrowableId = EBMWeaponId::FragGrenade;
        Profile->DisplayName = NSLOCTEXT("BlackMeridian", "FragGrenade", "Frag Grenade");
        Profile->FuseSeconds = 2.4f;
        Profile->Damage = 110.0f;
        Profile->Radius = 360.0f;
        Profile->SuppressionAmount = 0.0f;
        Profile->ThrowSpeed = 950.0f;
        Profile->GravityScale = 0.7f;
    }
    return Profile;
}

UBMThrowableDataAsset* UBMThrowableDataAsset::LoadContentProfile(
    UObject* Outer,
    const EBMWeaponId InThrowableId)
{
    if (const TCHAR* AssetPath = GetThrowableProfileAssetPath(InThrowableId))
    {
        if (UBMThrowableDataAsset* ContentProfile = LoadObject<UBMThrowableDataAsset>(nullptr, AssetPath))
        {
            if (ContentProfile->ThrowableId == InThrowableId)
            {
#if !UE_BUILD_SHIPPING
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian throwable profile: loaded serialized asset=%s id=%d"),
                    AssetPath,
                    static_cast<int32>(InThrowableId));
#endif
                return ContentProfile;
            }
        }
    }

    return CreateRuntimeProfile(Outer ? Outer : GetTransientPackage(), InThrowableId);
}
