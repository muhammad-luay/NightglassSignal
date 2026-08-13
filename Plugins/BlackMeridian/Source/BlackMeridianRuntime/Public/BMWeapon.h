// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BMCoreTypes.h"
#include "BMWeaponDataAsset.h"
#include "BMWeaponAttachmentDataAsset.h"
#include "BMWeaponInterfaces.h"
#include "BMWeapon.generated.h"

class UBMFireModeComponent;
class UBMRecoilComponent;
class UBMAmmunitionComponent;
class UBMWeaponAttachmentDataAsset;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UProceduralMeshComponent;
class UNiagaraSystem;
class USoundBase;
class UStaticMeshComponent;
struct FRandomStream;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBMWeaponEvent, FName, EventName);

UCLASS(BlueprintType, Blueprintable)
class BLACKMERIDIANRUNTIME_API ABMWeapon : public AActor, public IBMWeaponAnimationInterface, public IBMWeaponAudioInterface
{
    GENERATED_BODY()

public:
    ABMWeapon();

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Weapon")
    void InitializeFromData(UBMWeaponDataAsset* InWeaponData);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Weapon")
    void Fire(const FVector& ClientOrigin, const FVector& ClientDirection, bool bAiming);

    /** Shared authority-side view-cone predicate used by player fire validation. */
    static bool IsViewDirectionWithinCone(
        const FVector& ViewDirection,
        const FVector& ShotDirection,
        float MaxAngleDegrees = 10.0f);

    /**
     * Apply the weapon's cone spread to one shot direction. This is shared by
     * single-pellet and multi-pellet weapons so a rifle or pistol cannot be
     * accidentally made perfectly accurate just because PelletCount is one.
     */
    static FVector GetSpreadDirection(
        const FVector& Direction,
        float SpreadRadians,
        FRandomStream& RandomStream);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Weapon")
    void Reload();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Weapon")
    void CancelReload();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Weapon")
    void StopFiring();

    /** Clear one-frame fallback FX when a combatant dies or a screen pauses. */
    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Weapon|Presentation")
    void ClearTransientPresentation();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Weapon|Attachments")
    bool EquipAttachment(UBMWeaponAttachmentDataAsset* Attachment);

    /** Equip a built-in or authored attachment by stable id on the authority. */
    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Weapon|Attachments")
    bool EquipAttachmentById(FName AttachmentId);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Weapon|Attachments")
    bool ClearAttachment(FName Slot);

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon|Attachments")
    float GetAttachmentDamageMultiplier() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon|Attachments")
    float GetAttachmentADSFOVOffset() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon|Attachments")
    float GetAttachmentRecoilMultiplier() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon|Attachments")
    FName GetEquippedAttachmentId(FName Slot) const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon|Attachments")
    TArray<FName> GetEquippedAttachmentIds() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon")
    UBMWeaponDataAsset* GetWeaponData() const { return WeaponData; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon")
    EBMWeaponId GetWeaponId() const { return ReplicatedWeaponId; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon")
    int32 GetMagazineAmmo() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon")
    int32 GetReserveAmmo() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon")
    bool IsReloading() const;

    /** Client-visible fraction of the current reload timer, for HUD feedback. */
    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon")
    float GetReloadProgress() const;

#if !UE_BUILD_SHIPPING
    /** Development-only probe seam for forcing a known magazine state. */
    bool SetCombatProbeAmmo(int32 InMagazineAmmo, int32 InReserveAmmo);
#endif

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon|Audio")
    bool IsSuppressed() const;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Weapon|Ammunition")
    void AddReserveAmmo(int32 Amount);

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon")
    FVector GetMuzzleLocation() const;

    /** World-space hand anchor resolved from the authored weapon socket. */
    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon|Presentation")
    FVector GetGripLocation() const;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Weapon|Presentation")
    void UpdatePresentation(float DeltaSeconds);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Weapon|Presentation")
    FRotator ConsumeVisualRecoil(float DeltaSeconds);

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon|Presentation")
    float GetPresentationReloadAlpha() const { return PresentationReloadAlpha; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon|Presentation")
    float GetPresentationShotPulse() const { return PresentationShotPulse; }

    /** True briefly after this owning client has predicted a visual shot. */
    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon|Presentation")
    bool WasFireRecentlyPredicted() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Weapon|Accuracy")
    float GetAccuracyPenaltyDegrees() const;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Weapon|Presentation")
    void SetFirstPersonPresentation(const FVector& LocationOffset, const FRotator& RotationOffset);

    /**
     * Select the visibility/material treatment for a replicated third-person
     * weapon. Player weapons remain owner-only; AI weapons must be readable to
     * every client that can see the enemy.
     */
    void SetThirdPersonPresentation(bool bEnabled);

    UPROPERTY(BlueprintAssignable, Category = "Black Meridian|Weapon")
    FBMWeaponEvent OnWeaponEvent;

    virtual void PlayWeaponAnimation_Implementation(FName AnimationEvent) override;
    virtual void PlayWeaponAudio_Implementation(FName AudioEvent, bool bSuppressed) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
    TObjectPtr<UBMFireModeComponent> FireModeComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
    TObjectPtr<UBMRecoilComponent> RecoilComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
    TObjectPtr<UBMAmmunitionComponent> AmmunitionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UStaticMeshComponent> WeaponMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UStaticMeshComponent> BarrelMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UStaticMeshComponent> MagazineMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UStaticMeshComponent> GripMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UStaticMeshComponent> OpticMesh;

    /** Additional fallback pieces keep the source-only weapon readable until
     * an authored weapon mesh is assigned. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UStaticMeshComponent> StockMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UStaticMeshComponent> HandguardMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UStaticMeshComponent> MuzzleDeviceMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UStaticMeshComponent> RailMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UStaticMeshComponent> FrontSightMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UStaticMeshComponent> TriggerGuardMesh;

    /** Small fallback bolt/charging-handle cue keeps fire and reload actions readable. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UStaticMeshComponent> ChargingHandleMesh;

    /** Additional slot-specific meshes are hidden until an attachment is equipped. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Attachments")
    TObjectPtr<UStaticMeshComponent> EquippedOpticMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Attachments")
    TObjectPtr<UStaticMeshComponent> EquippedMuzzleMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Attachments")
    TObjectPtr<UStaticMeshComponent> EquippedMagazineMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Attachments")
    TObjectPtr<UStaticMeshComponent> EquippedGripMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Attachments")
    TObjectPtr<UStaticMeshComponent> EquippedStockMesh;

    /**
     * Beveled first-person ARX-4 body. The project-owned placeholder meshes
     * remain the third-person contract; this runtime mesh keeps the local
     * viewmodel connected and readable without a separate art dependency.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UProceduralMeshComponent> FirstPersonGeometryMesh;

    /** Separate beveled magazine so the first-person reload animation can
     * move the visible magazine without moving the whole rifle body. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UProceduralMeshComponent> FirstPersonMagazineGeometryMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UPointLightComponent> FireLight;

    /** Runtime fallback effects keep the combat loop readable without authored Niagara assets. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UStaticMeshComponent> MuzzleFlashMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UStaticMeshComponent> TracerMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UStaticMeshComponent> ImpactFlashMesh;

    /** Two short emissive shards turn a single impact point into a readable
     * directional burst while authored Niagara is still optional. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UStaticMeshComponent> ImpactShardMeshA;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UStaticMeshComponent> ImpactShardMeshB;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
    TObjectPtr<UPointLightComponent> ImpactLight;

protected:
    UFUNCTION(Server, Reliable)
    void ServerFire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction, bool bAiming);

    UFUNCTION(Server, Reliable)
    void ServerReload();

    /** Clears an optimistic local reload if authority rejected the request. */
    UFUNCTION(Client, Reliable)
    void ClientReloadRejected();

    UFUNCTION(Server, Reliable)
    void ServerStopFiring();

    UFUNCTION(Server, Reliable)
    void ServerEquipAttachmentById(FName AttachmentId);

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastFireEffects(
        FVector_NetQuantize Start,
        FVector_NetQuantize End,
        bool bHit,
        bool bHeadshot,
        bool bSurfaceImpact,
        bool bSuppressed,
        EBMSurfaceType ImpactSurface);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastWeaponEvent(FName EventName);

    bool PerformAuthoritativeShot(
        const FVector& Origin,
        const FVector& Direction,
        bool bAiming,
        bool bStrictPlayerCrosshair,
        AActor* PlayerCrosshairTarget,
        bool bPlayerCrosshairAgreesWithServerView);
    bool ApplyHitDamage(
        const FHitResult& Hit,
        const FVector& ShotDirection,
        float DamageScale,
        bool bPenetrated,
        EBMSurfaceType SurfaceType,
        int32 PenetrationCount,
        bool& bHeadshot,
        float& HostileDamageRemaining,
        bool bStrictPlayerCrosshair,
        AActor* PlayerCrosshairTarget,
        bool bPlayerCrosshairAgreesWithServerView,
        bool bValidatedCenterRay);
    EBMHitZone ResolveHitZone(const FHitResult& Hit) const;
    EBMSurfaceType ResolveSurfaceType(const FHitResult& Hit) const;
    float CalculateDistanceFalloff(float DistanceCentimeters) const;
    bool IsValidClientShot(const FVector& Origin, const FVector& Direction) const;

    void FinishReload();
    void BeginReloadPresentation(bool bEmptyReload);

    UFUNCTION(Server, Reliable)
    void ServerCancelReload();

    UFUNCTION()
    void DisableFireLight();

    UFUNCTION()
    void DisableMuzzleFlash();

    UFUNCTION()
    void DisableTracer();

    UFUNCTION()
    void DisableImpactFlash();

    void ApplyPresentationMaterials();
    void ApplyAttachmentPresentation();
    void RebuildAttachmentMapFromReplication();
    void RebuildReplicatedAttachmentState();
    void ApplyFallbackWeaponVariant();
    void BuildFirstPersonARX4Geometry();
    void BuildFirstPersonMagazineGeometry();
    /** Load fire-path assets during setup so the first shot is not a hitch. */
    void PrewarmPresentationAssets();
    void ApplyFallbackFirePresentation(
        const FVector& Start,
        const FVector& End,
        bool bHit,
        bool bHeadshot,
        bool bSurfaceImpact,
        bool bSuppressed,
        EBMSurfaceType ImpactSurface);
    /** Play the shot sound at the muzzle; used by both prediction and confirmation. */
    void PlayFireSoundAtLocation(const FVector& Location, bool bSuppressed);
    void PlayPredictedFirePresentation(const FVector& ClientOrigin, const FVector& ClientDirection, bool bAiming);

private:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Data", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBMWeaponDataAsset> WeaponData;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Weapon|Attachments", meta = (AllowPrivateAccess = "true"))
    TMap<FName, TObjectPtr<UBMWeaponAttachmentDataAsset>> EquippedAttachments;

    UPROPERTY(ReplicatedUsing = OnRep_AttachmentState, BlueprintReadOnly, Category = "Weapon|Attachments", meta = (AllowPrivateAccess = "true"))
    TArray<FBMWeaponAttachmentState> ReplicatedAttachmentState;

    UPROPERTY(ReplicatedUsing = OnRep_WeaponId, BlueprintReadOnly, Category = "Weapon|Data", meta = (AllowPrivateAccess = "true"))
    EBMWeaponId ReplicatedWeaponId = EBMWeaponId::ARX4;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon|Audio", meta = (AllowPrivateAccess = "true"))
    bool bReplicatedSuppressed = false;

    UFUNCTION()
    void OnRep_WeaponId();

    UFUNCTION()
    void OnRep_AttachmentState();

    FTimerHandle ReloadTimerHandle;
    FTimerHandle FireLightTimerHandle;
    FTimerHandle MuzzleFlashTimerHandle;
    FTimerHandle TracerTimerHandle;
    FTimerHandle ImpactFlashTimerHandle;
    FVector LastShotEnd = FVector::ZeroVector;
    FVector PresentationBaseLocation = FVector::ZeroVector;
    FRotator PresentationBaseRotation = FRotator::ZeroRotator;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> MuzzleFlashMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> TracerMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> ImpactFlashMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> ImpactShardMaterialA;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> ImpactShardMaterialB;

    /** Strong transient references keep prewarmed presentation assets resident. */
    UPROPERTY(Transient)
    TObjectPtr<UBMAudioDataAsset> PrewarmedAudioProfile;

    UPROPERTY(Transient)
    TObjectPtr<UBMVFXDataAsset> PrewarmedVFXProfile;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USoundBase>> PrewarmedAudioAssets;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UNiagaraSystem>> PrewarmedVFXAssets;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> PrewarmedImpactDecal;

    /** The runtime barrel uses a cylinder whose local Z axis is the muzzle axis. */
    bool bFallbackBarrelUsesUpAxis = false;

    /** The muzzle device is the visual origin when the fallback weapon is in use. */
    bool bFallbackMuzzleDeviceUsesUpAxis = false;

    float PresentationReloadAlpha = 0.0f;
    float ReloadPresentationDuration = 0.0f;
    float ReloadPresentationElapsed = 0.0f;
    float PresentationShotPulse = 0.0f;
    float LastPredictedFireTime = -BIG_NUMBER;
    float LastDryFireFeedbackTime = -BIG_NUMBER;
    bool bPresentationWasReloading = false;
    bool bPresentationReloadReturning = false;
    FVector PresentationMagazineBaseLocation = FVector::ZeroVector;
    FRotator PresentationMagazineBaseRotation = FRotator::ZeroRotator;
    FVector PresentationFirstPersonMagazineBaseLocation = FVector::ZeroVector;
    FRotator PresentationFirstPersonMagazineBaseRotation = FRotator::ZeroRotator;
    FVector PresentationChargingHandleBaseLocation = FVector::ZeroVector;
    FRotator PresentationChargingHandleBaseRotation = FRotator::ZeroRotator;
    bool bThirdPersonPresentation = false;
    bool bUsingProjectWeaponMesh = false;
    bool bFirstPersonGeometryBuilt = false;
    bool bFirstPersonMagazineGeometryBuilt = false;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
