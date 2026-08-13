// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMWeaponComponents.h"

#include "GameFramework/Actor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"

bool UBMFireModeComponent::CanFire(const float WorldTime, const float FireInterval, const EBMWeaponFireMode FireMode)
{
    if ((WorldTime - LastFireTime) < FireInterval)
    {
        return false;
    }

    if (FireMode == EBMWeaponFireMode::Burst && BurstShots >= 3)
    {
        return false;
    }

    LastFireTime = WorldTime;
    if (FireMode == EBMWeaponFireMode::Burst)
    {
        ++BurstShots;
    }
    else if (FireMode == EBMWeaponFireMode::SemiAutomatic)
    {
        BurstShots = 0;
    }
    return true;
}

void UBMFireModeComponent::ResetBurst()
{
    BurstShots = 0;
}

void UBMFireModeComponent::ResetCombatState()
{
    LastFireTime = -BIG_NUMBER;
    BurstShots = 0;
    AccuracyPenalty = 0.0f;
    LastAccuracyUpdateTime = 0.0f;
    bAccuracyClockInitialized = false;
}

float UBMFireModeComponent::GetAccuracyPenalty(const float WorldTime, const float RecoveryRate)
{
    if (!bAccuracyClockInitialized)
    {
        LastAccuracyUpdateTime = WorldTime;
        bAccuracyClockInitialized = true;
        return AccuracyPenalty;
    }

    const float DeltaTime = FMath::Max(0.0f, WorldTime - LastAccuracyUpdateTime);
    AccuracyPenalty = FMath::Max(0.0f, AccuracyPenalty - DeltaTime * FMath::Max(0.0f, RecoveryRate));
    LastAccuracyUpdateTime = WorldTime;
    return AccuracyPenalty;
}

void UBMFireModeComponent::RegisterShot(
    const float WorldTime,
    const float ShotPenalty,
    const float RecoveryRate,
    const float MaxPenalty)
{
    GetAccuracyPenalty(WorldTime, RecoveryRate);
    AccuracyPenalty = FMath::Clamp(AccuracyPenalty + FMath::Max(0.0f, ShotPenalty), 0.0f, FMath::Max(0.0f, MaxPenalty));
}

void UBMRecoilComponent::AddImpulse(const float Pitch, const float Yaw)
{
    PendingKick.Pitch += Pitch;
    PendingKick.Yaw += Yaw;
}

void UBMRecoilComponent::ResetCombatState()
{
    PendingKick = FRotator::ZeroRotator;
}

FRotator UBMRecoilComponent::ConsumeVisualKick(const float DeltaSeconds, const float RecoverySpeed)
{
    const FRotator Kick = PendingKick;
    PendingKick = FMath::RInterpTo(PendingKick, FRotator::ZeroRotator, DeltaSeconds, RecoverySpeed);
    return Kick;
}

UBMAmmunitionComponent::UBMAmmunitionComponent()
{
    SetIsReplicatedByDefault(true);
}

void UBMAmmunitionComponent::ConfigureMagazineCapacity(const int32 InMagazineCapacity)
{
    MagazineCapacity = FMath::Max(0, InMagazineCapacity);
}

void UBMAmmunitionComponent::Initialize(const int32 InMagazineCapacity, const int32 StartingReserve)
{
    ConfigureMagazineCapacity(InMagazineCapacity);
    MagazineAmmo = MagazineCapacity;
    ReserveAmmo = FMath::Max(0, StartingReserve);
    bReloading = false;
    OnAmmunitionChanged.Broadcast();
}

bool UBMAmmunitionComponent::ConsumeRound()
{
    if (bReloading || MagazineAmmo <= 0)
    {
        return false;
    }
    --MagazineAmmo;
    OnAmmunitionChanged.Broadcast();
    return true;
}

void UBMAmmunitionComponent::AddReserveAmmo(const int32 Amount)
{
    if (Amount <= 0)
    {
        return;
    }
    ReserveAmmo += Amount;
    OnAmmunitionChanged.Broadcast();
}

bool UBMAmmunitionComponent::BeginReload()
{
    if (bReloading || MagazineAmmo >= MagazineCapacity || ReserveAmmo <= 0)
    {
        return false;
    }
    bReloading = true;
    OnAmmunitionChanged.Broadcast();
    return true;
}

void UBMAmmunitionComponent::CompleteReload()
{
    if (!bReloading)
    {
        return;
    }
    const int32 Needed = MagazineCapacity - MagazineAmmo;
    const int32 Loaded = FMath::Min(Needed, ReserveAmmo);
    MagazineAmmo += Loaded;
    ReserveAmmo -= Loaded;
    bReloading = false;
    OnAmmunitionChanged.Broadcast();
}

void UBMAmmunitionComponent::CancelReload()
{
    bReloading = false;
    OnAmmunitionChanged.Broadcast();
}

void UBMAmmunitionComponent::OnRep_Ammo()
{
    OnAmmunitionChanged.Broadcast();
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe"))
        || FParse::Param(FCommandLine::Get(), TEXT("BM_ReloadProbe")))
    {
        const AActor* Weapon = GetOwner();
        const AActor* WeaponOwner = Weapon ? Weapon->GetOwner() : nullptr;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat probe: client ammo replicated owner=%s magazine=%d reserve=%d"),
            *GetNameSafe(WeaponOwner),
            MagazineAmmo,
            ReserveAmmo);
    }
#endif
}

void UBMAmmunitionComponent::OnRep_ReloadState()
{
    // Reload state is presentation-critical even when magazine and reserve
    // values have not changed yet. Notify HUD and animation listeners as soon
    // as authority accepts or completes the timer.
    OnAmmunitionChanged.Broadcast();
}

void UBMAmmunitionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UBMAmmunitionComponent, MagazineAmmo);
    DOREPLIFETIME(UBMAmmunitionComponent, ReserveAmmo);
    DOREPLIFETIME(UBMAmmunitionComponent, bReloading);
}
