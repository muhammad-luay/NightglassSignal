// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMWeaponAttachmentDataAsset.h"

UBMWeaponAttachmentDataAsset* UBMWeaponAttachmentDataAsset::CreateRuntimeProfile(
    UObject* Outer,
    const FName InAttachmentId)
{
    UBMWeaponAttachmentDataAsset* Profile = NewObject<UBMWeaponAttachmentDataAsset>(
        Outer ? Outer : GetTransientPackage());
    if (!Profile)
    {
        return nullptr;
    }

    Profile->AttachmentId = InAttachmentId;
    Profile->DisplayName = FText::FromName(InAttachmentId);

    // These profiles are deliberately small but materially different. They
    // are the same data-driven path used by authored assets once those assets
    // are supplied, so a pickup never needs a special-case combat formula.
    if (InAttachmentId == TEXT("Optic_Rangefinder"))
    {
        Profile->Slot = TEXT("Optic");
        Profile->DisplayName = NSLOCTEXT("BlackMeridian", "AttachmentOpticRangefinder", "Rangefinder Optic");
        Profile->ADSFOVOffset = -8.0f;
        Profile->RecoilMultiplier = 0.94f;
    }
    else if (InAttachmentId == TEXT("Muzzle_Suppressor"))
    {
        Profile->Slot = TEXT("Muzzle");
        Profile->DisplayName = NSLOCTEXT("BlackMeridian", "AttachmentMuzzleSuppressor", "Quietline Suppressor");
        Profile->RecoilMultiplier = 0.92f;
        Profile->bSuppressor = true;
    }
    else if (InAttachmentId == TEXT("Grip_Stabilizer"))
    {
        Profile->Slot = TEXT("Grip");
        Profile->DisplayName = NSLOCTEXT("BlackMeridian", "AttachmentGripStabilizer", "Stabilizer Grip");
        Profile->RecoilMultiplier = 0.72f;
    }
    else if (InAttachmentId == TEXT("Magazine_Extended"))
    {
        Profile->Slot = TEXT("Magazine");
        Profile->DisplayName = NSLOCTEXT("BlackMeridian", "AttachmentMagazineExtended", "Extended Magazine");
        Profile->DamageMultiplier = 1.03f;
    }
    else if (InAttachmentId == TEXT("Stock_Reinforced"))
    {
        Profile->Slot = TEXT("Stock");
        Profile->DisplayName = NSLOCTEXT("BlackMeridian", "AttachmentStockReinforced", "Reinforced Stock");
        Profile->RecoilMultiplier = 0.78f;
    }
    else
    {
        // Unknown ids remain valid for designer-authored content, but default
        // to a harmless optic slot until the data asset supplies its values.
        Profile->Slot = TEXT("Optic");
    }

    return Profile;
}
