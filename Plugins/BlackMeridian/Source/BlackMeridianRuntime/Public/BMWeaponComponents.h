// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BMCoreTypes.h"
#include "BMWeaponComponents.generated.h"

UCLASS(ClassGroup = (BlackMeridian), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class BLACKMERIDIANRUNTIME_API UBMFireModeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    bool CanFire(float WorldTime, float FireInterval, EBMWeaponFireMode FireMode);
    void ResetBurst();
    /** Reset timing/accuracy state when a deterministic combat scenario is prepared. */
    void ResetCombatState();
    float GetAccuracyPenalty(float WorldTime, float RecoveryRate);
    void RegisterShot(float WorldTime, float ShotPenalty, float RecoveryRate, float MaxPenalty);

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon|Accuracy")
    float GetCurrentAccuracyPenalty() const { return AccuracyPenalty; }

private:
    float LastFireTime = -BIG_NUMBER;
    int32 BurstShots = 0;
    float AccuracyPenalty = 0.0f;
    float LastAccuracyUpdateTime = 0.0f;
    bool bAccuracyClockInitialized = false;
};

UCLASS(ClassGroup = (BlackMeridian), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class BLACKMERIDIANRUNTIME_API UBMRecoilComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    void AddImpulse(float Pitch, float Yaw);
    /** Clear any pending visual kick when a weapon is reset or respawned. */
    void ResetCombatState();
    FRotator ConsumeVisualKick(float DeltaSeconds, float RecoverySpeed = 8.0f);

private:
    FRotator PendingKick = FRotator::ZeroRotator;
};

UCLASS(ClassGroup = (BlackMeridian), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class BLACKMERIDIANRUNTIME_API UBMAmmunitionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBMAmmunitionComponent();

    /** Configure the local capacity without touching replicated ammo values. */
    void ConfigureMagazineCapacity(int32 InMagazineCapacity);
    void Initialize(int32 MagazineCapacity, int32 StartingReserve);
    bool ConsumeRound();
    void AddReserveAmmo(int32 Amount);
    bool BeginReload();
    void CompleteReload();
    void CancelReload();

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Ammunition")
    int32 GetMagazineAmmo() const { return MagazineAmmo; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Ammunition")
    int32 GetReserveAmmo() const { return ReserveAmmo; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Ammunition")
    bool IsReloading() const { return bReloading; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Ammunition")
    bool IsEmpty() const { return MagazineAmmo <= 0; }

    UPROPERTY(ReplicatedUsing = OnRep_Ammo, BlueprintReadOnly, Category = "Ammunition")
    int32 MagazineAmmo = 0;

    UPROPERTY(ReplicatedUsing = OnRep_Ammo, BlueprintReadOnly, Category = "Ammunition")
    int32 ReserveAmmo = 0;

    UPROPERTY(ReplicatedUsing = OnRep_ReloadState, BlueprintReadOnly, Category = "Ammunition")
    bool bReloading = false;

    DECLARE_MULTICAST_DELEGATE(FOnAmmunitionChanged);
    FOnAmmunitionChanged OnAmmunitionChanged;

protected:
    UFUNCTION()
    void OnRep_Ammo();

    UFUNCTION()
    void OnRep_ReloadState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    int32 MagazineCapacity = 0;
};
