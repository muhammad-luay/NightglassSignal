// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BMCombatCharacter.h"
#include "BMPlayerCharacter.generated.h"

class UCameraComponent;
class UAnimInstance;
class UAnimSequence;
class UInputComponent;
class UPointLightComponent;
class USceneComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class ABMWeapon;
class ABMPlayerController;
class ABMMissionDirector;
class AActor;

UCLASS(Blueprintable)
class BLACKMERIDIANRUNTIME_API ABMPlayerCharacter : public ABMCombatCharacter
{
    GENERATED_BODY()

public:
    ABMPlayerCharacter(const FObjectInitializer& ObjectInitializer);

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void Landed(const FHitResult& Hit) override;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player")
    UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

    /** Canonical gameplay/crosshair ray; excludes presentation-only camera offsets. */
    FVector GetAimDirection() const;

    /**
     * Refreshes the local rendered aim sample after camera presentation has
     * been updated for the current frame.  The HUD and fire input consume
     * this same sample so a target cannot be red for one ray while the shot
     * is submitted on another.
     */
    void UpdateLocalAimSnapshot();
    bool HasLocalAimSnapshot() const { return bLocalAimSnapshotValid; }
    const ABMCombatCharacter* GetLocalAimTarget() const
    {
        return bLocalAimSnapshotValid ? LocalAimSnapshotTarget.Get() : nullptr;
    }
    FVector GetLocalAimOrigin() const { return LocalAimSnapshotOrigin; }
    FVector GetLocalAimDirection() const { return LocalAimSnapshotDirection; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player|Presentation")
    USkeletalMeshComponent* GetFirstPersonArms() const { return FirstPersonArms; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player")
    ABMWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player")
    bool IsAiming() const { return bAiming; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player")
    bool IsSprinting() const { return bSprinting; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player")
    bool IsTacticalSprinting() const { return bTacticalSprinting; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player")
    bool IsProne() const { return bProne; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player|Equipment")
    int32 GetFragCount() const { return FragCount; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player|Equipment")
    int32 GetVeilCount() const { return VeilCount; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player|Combat Feedback")
    bool HasThrowableThreat() const { return bThrowableThreatActive; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player|Combat Feedback")
    FVector GetThrowableThreatLocation() const { return ThrowableThreatLocation; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player|Combat Feedback")
    float GetThrowableThreatDistance() const { return ThrowableThreatDistance; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player|Equipment")
    bool IsThrowablePresentationActive() const { return ThrowablePresentationRemaining > 0.0f; }

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Player|Equipment")
    void RestoreThrowableInventory(int32 InFragCount, int32 InVeilCount);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Player|Equipment")
    bool AddThrowableInventory(int32 InFragCount, int32 InVeilCount);

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player|Equipment")
    int32 GetMaxFragCount() const { return MaxFragCount; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player|Equipment")
    int32 GetMaxVeilCount() const { return MaxVeilCount; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player|Movement")
    bool IsMountedWeapon() const { return bMountedWeapon; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player|Interaction")
    bool IsHoldingInteraction() const { return PendingInteraction.IsValid(); }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player|Interaction")
    float GetInteractionHoldProgress() const;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Player")
    void EquipWeaponById(EBMWeaponId WeaponId);

    /** Ignore the one-time mouse-capture delta that can arrive on mission launch. */
    void BeginMissionLookGuard(float Duration = 3.0f);

    /** Clear a held trigger whenever focus, menus, pause, or a pawn state interrupts gameplay. */
    void ResetFireInputState();

    /** Return a pawn that slipped through a collision gap to its last safe floor transform. */
    void RecoverFromBoundaryFall();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Player|Weapons")
    bool AddWeaponToLoadout(EBMWeaponId WeaponId);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Player|Weapons")
    bool AddReserveAmmo(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Player|Weapons")
    bool AddAmmoToWeapon(EBMWeaponId WeaponId, int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Player|Weapons|Attachments")
    bool EquipAttachmentById(FName AttachmentId);

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player|Weapons")
    TArray<EBMWeaponId> GetLoadoutWeaponIds() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Player|Weapons")
    EBMWeaponId GetCurrentWeaponId() const;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Player|Weapons")
    bool RestoreLoadoutFromIds(const TArray<EBMWeaponId>& WeaponIds, EBMWeaponId InCurrentWeaponId);

    virtual void RestoreForCheckpoint(float InHealth) override;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Player")
    void NotifyDamageDirection(const FVector& DamageOrigin);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Player|Movement")
    void BeginLadderClimb(AActor* LadderActor);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Player|Movement")
    void EndLadderClimb();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Player|Interaction")
    void BeginMountedWeapon(AActor* MountedActor);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Player|Interaction")
    void EndMountedWeapon();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Player|Bot")
    void BotFireAt(AActor* Target);

#if !UE_BUILD_SHIPPING
    /** Development-only transform sync used before the multiplayer combat probe fires. */
    void PrepareCombatProbeTransform(const FVector& Location, const FRotator& Rotation);
    /** Development-only rendered showcase for the first-person throwable hand presentation. */
    void RunThrowablePresentationShowcase(EBMWeaponId ThrowableId);
    /** Development-only client input seam used by the multiplayer combat probe. */
    void RunCombatProbeFire(const FVector& Direction);
    /** Development-only client input seam for the normal reload action. */
    void RunCombatProbeReload();
    /** Development-only client input seam for fire-to-cancel reload behavior. */
    void RunCombatProbeReloadInterrupt();

    /**
     * Development-only seam for the mission interaction probe. This deliberately
     * enters the same target acquisition and hold-timer path as the E key.
     */
    bool RunInteractionProbeHold(AActor* ExpectedTarget);
#endif

protected:
    virtual void HandleDeath(const FBMDamageContext& DamageContext) override;
    virtual void PlayCharacterAnimation_Implementation(FName AnimationEvent) override;

    UFUNCTION()
    void OnRep_CurrentWeapon();

    UFUNCTION()
    void HandleWeaponEvent(FName EventName);

    UFUNCTION()
    void HandleLocalHealthChanged(float NewHealth, float DamageTaken);

    UFUNCTION(Server, Reliable)
    void ServerEquipWeapon(EBMWeaponId WeaponId);

    UFUNCTION(Server, Reliable)
    void ServerAddWeaponToLoadout(EBMWeaponId WeaponId);

    UFUNCTION(Server, Reliable)
    void ServerEquipAttachmentById(FName AttachmentId);

    UFUNCTION(Server, Reliable)
    void ServerSetAimState(bool bNewAiming);

    UFUNCTION(Server, Reliable)
    void ServerSetSprintState(bool bNewSprinting, bool bNewTacticalSprinting);

    UFUNCTION(Server, Reliable)
    void ServerSetCrouchState(bool bNewCrouched);

    UFUNCTION(Server, Reliable)
    void ServerSetProneState(bool bNewProne);

    UFUNCTION(Server, Reliable)
    void ServerBeginLadderClimb(AActor* LadderActor);

    UFUNCTION(Server, Reliable)
    void ServerEndLadderClimb();

    UFUNCTION(Server, Reliable)
    void ServerBeginMountedWeapon(AActor* MountedActor);

    UFUNCTION(Server, Reliable)
    void ServerEndMountedWeapon();

    UFUNCTION(Server, Reliable)
    void ServerTryMantle();

    UFUNCTION(Server, Reliable)
    void ServerInteract(AActor* Target);

    UFUNCTION(Server, Reliable)
    void ServerMelee(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction);

    UFUNCTION(Server, Reliable)
    void ServerThrowThrowable(EBMWeaponId ThrowableId, FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction);

    void MoveForward(float Value);
    void MoveRight(float Value);
    void StopJumpInput();
    void Turn(float Value);
    void LookUp(float Value);
    void StartSprint();
    void StopSprint();
    void StartTacticalSprint();
    void StopTacticalSprint();
    void StartAim();
    void StopAim();
    void StartCrouch();
    void StopCrouch();
    void ToggleProne();
    void TryMantle();
    void TryMantleInternal();
    void StartSlide();
    void FireWeapon();
    bool ProcessPendingFire();
    void StartFire();
    void StartFireInternal(bool bBypassInputGuard);
    void StopFire();
    void ReloadWeapon();
    void SwitchWeapon();
    void MeleeWeapon();
    void ThrowFrag();
    void ThrowTactical();
    void Interact();
    void StopInteract();
    void CompleteHeldInteraction();
    void LeanLeft();
    void LeanRight();
    void ResetLean();
    void StopSprintInternal();
    void StopCrouchInternal();
    void UpdateCameraBob(float DeltaSeconds);
    void UpdateCameraPresentation(float DeltaSeconds);
    void ApplyPlayableCameraCue();
    void UpdateSuppressionPresentation();
    void UpdateAudioPresentation(float DeltaSeconds);
    void UpdateThrowableThreatPresentation(float DeltaSeconds);
    void UpdateWeaponPresentation(float DeltaSeconds);
    void ConfigureThrowablePresentation();
    void BeginThrowablePresentation(EBMWeaponId ThrowableId);
    void UpdateThrowablePresentation(float DeltaSeconds);
    void ApplyThrowablePresentationMaterial();
    void ConfigureFallbackArms();
    void UpdateFallbackArmsPresentation(const FVector& LocationOffset, const FRotator& RotationOffset, float ReloadAlpha);
    void UpdateFirstPersonAnimationPresentation(
        float DeltaSeconds,
        const FVector& LocationOffset,
        const FRotator& RotationOffset,
        float ReloadAlpha);
    void InitializeFirstPersonAnimationPresentation();
    void PlayFirstPersonAnimationEvent(FName AnimationEvent);
    void RestoreFirstPersonSingleNodeLocomotion();
    void ApplyFirstPersonAnimationEvent(FName AnimationEvent);
    void UpdateAnimationPresentation();
    void RecoverReplicatedWeaponState();
    void AttachCurrentWeapon();
    void SpawnLoadout();
    ABMWeapon* SpawnWeaponById(EBMWeaponId WeaponId);
    void PerformInteractionTrace();
    void ExecuteInteraction(AActor* Target);
    void UpdateInteractionPrompt();
    AActor* FindInteractionTarget() const;
    EBMSurfaceType ResolveMovementSurfaceType() const;
    void PlayFootstepCue();
    bool IsPlayableCameraLookLocked() const;
    bool IsSignalControlLocked() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<USceneComponent> WeaponAnchor;

    /**
     * Small local key light for the first-person weapon and hands. The
     * camera-mounted viewmodel is otherwise lit only by the world key, which
     * makes graphite materials disappear whenever the player is under cover.
     * It is presentation-only and is disabled on non-local pawns.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|First Person")
    TObjectPtr<UPointLightComponent> FirstPersonFillLight;

    /** Local-only grenade silhouette shown during the throw action. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|First Person")
    TObjectPtr<UStaticMeshComponent> ThrowablePresentationMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|First Person")
    TObjectPtr<USkeletalMeshComponent> FirstPersonArms;

    /** Runtime fallback sleeves keep the weapon grounded until authored arms are assigned. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|First Person")
    TObjectPtr<UStaticMeshComponent> FallbackLeftArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|First Person")
    TObjectPtr<UStaticMeshComponent> FallbackRightArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|First Person")
    TObjectPtr<UStaticMeshComponent> FallbackLeftHand;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|First Person")
    TObjectPtr<UStaticMeshComponent> FallbackRightHand;

    /** Layered details make the source-only first-person kit read as tactical gear instead of debug primitives. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|First Person")
    TObjectPtr<UStaticMeshComponent> FallbackLeftCuff;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|First Person")
    TObjectPtr<UStaticMeshComponent> FallbackRightCuff;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|First Person")
    TObjectPtr<UStaticMeshComponent> FallbackLeftGlovePanel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|First Person")
    TObjectPtr<UStaticMeshComponent> FallbackRightGlovePanel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|First Person")
    TObjectPtr<UStaticMeshComponent> FallbackLeftKnucklePlate;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|First Person")
    TObjectPtr<UStaticMeshComponent> FallbackRightKnucklePlate;

    /** Optional authored arms mesh; the runtime cube weapon remains the fallback. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|First Person")
    TSoftObjectPtr<USkeletalMesh> FirstPersonArmsMesh;

    /** Development AnimBP used to layer the additive rifle fire clip over the authored pose. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|First Person")
    TSoftClassPtr<UAnimInstance> FirstPersonAnimBlueprint;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon, BlueprintReadOnly, Category = "Weapons")
    TObjectPtr<ABMWeapon> CurrentWeapon;

    UPROPERTY(BlueprintReadOnly, Category = "Weapons")
    TArray<TObjectPtr<ABMWeapon>> Loadout;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
    bool bAiming = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
    bool bOnLadder = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
    bool bMountedWeapon = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Equipment")
    int32 FragCount = 2;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Equipment")
    int32 VeilCount = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    int32 MaxFragCount = 2;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    int32 MaxVeilCount = 1;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
    bool bSprinting = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
    bool bTacticalSprinting = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
    bool bProne = false;

    float DefaultWalkSpeed = 360.0f;
    float DefaultFOV = 90.0f;
    float StartupLookGuardRemaining = 0.0f;
    float CameraBobTime = 0.0f;
    float WeaponSwayTime = 0.0f;
    float ThrowablePresentationRemaining = 0.0f;
    float ThrowablePresentationElapsed = 0.0f;
    FRotator LastVisualRecoil = FRotator::ZeroRotator;
    FVector WeaponAnchorBaseLocation = FVector::ZeroVector;
    FRotator WeaponAnchorBaseRotation = FRotator::ZeroRotator;
    float WeaponAimAlpha = 0.0f;
    float WeaponLowerAlpha = 0.0f;
    float CameraStanceHeight = 72.0f;
    float TargetLeanAmount = 0.0f;
    float CameraLandingOffset = 0.0f;
    float CameraLandingRoll = 0.0f;
    float CameraDamageKick = 0.0f;
    float CameraDamageRoll = 0.0f;
    float FootstepDistance = 0.0f;
    float LowHealthAudioCooldown = 0.0f;
    float ThrowableThreatAudioCooldown = 0.0f;
    float LeanAmount = 0.0f;
    bool bWantsToFire = false;
    bool bFireInputArmed = true;
    bool bPendingFire = false;
    bool bAutomaticFireBlockedUntilRelease = false;
    bool bLocalAimSnapshotValid = false;
    FVector LocalAimSnapshotOrigin = FVector::ZeroVector;
    FVector LocalAimSnapshotDirection = FVector::ForwardVector;
    TWeakObjectPtr<ABMCombatCharacter> LocalAimSnapshotTarget;
    float FireInputGuardRemaining = 0.0f;
    bool bWasSuppressed = false;
    FVector FallbackLeftArmBaseLocation = FVector::ZeroVector;
    FVector FallbackRightArmBaseLocation = FVector::ZeroVector;
    FRotator FallbackLeftArmBaseRotation = FRotator::ZeroRotator;
    FRotator FallbackRightArmBaseRotation = FRotator::ZeroRotator;
    FVector FallbackLeftHandBaseLocation = FVector::ZeroVector;
    FVector FallbackRightHandBaseLocation = FVector::ZeroVector;
    FRotator FallbackLeftHandBaseRotation = FRotator::ZeroRotator;
    FRotator FallbackRightHandBaseRotation = FRotator::ZeroRotator;
    FVector FallbackLeftCuffBaseLocation = FVector::ZeroVector;
    FVector FallbackRightCuffBaseLocation = FVector::ZeroVector;
    FRotator FallbackLeftCuffBaseRotation = FRotator::ZeroRotator;
    FRotator FallbackRightCuffBaseRotation = FRotator::ZeroRotator;
    FVector FallbackLeftGlovePanelBaseLocation = FVector::ZeroVector;
    FVector FallbackRightGlovePanelBaseLocation = FVector::ZeroVector;
    FRotator FallbackLeftGlovePanelBaseRotation = FRotator::ZeroRotator;
    FRotator FallbackRightGlovePanelBaseRotation = FRotator::ZeroRotator;
    FVector FallbackLeftKnucklePlateBaseLocation = FVector::ZeroVector;
    FVector FallbackRightKnucklePlateBaseLocation = FVector::ZeroVector;
    FRotator FallbackLeftKnucklePlateBaseRotation = FRotator::ZeroRotator;
    FRotator FallbackRightKnucklePlateBaseRotation = FRotator::ZeroRotator;
    FVector FirstPersonArmsBaseLocation = FVector::ZeroVector;
    FRotator FirstPersonArmsBaseRotation = FRotator::ZeroRotator;
    float FirstPersonFirePulse = 0.0f;
    float FirstPersonReloadPulse = 0.0f;
    float FirstPersonReloadReturnPulse = 0.0f;
    float FirstPersonDryFirePulse = 0.0f;
    float FirstPersonDamagePulse = 0.0f;
    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> FirstPersonRifleIdleAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> FirstPersonRifleFireAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> FirstPersonRifleReloadAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> FirstPersonRifleDryFireAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> ActiveFirstPersonAnimation;

    UPROPERTY(Transient)
    TSubclassOf<UAnimInstance> FirstPersonAnimBlueprintClass;

    float FirstPersonEventRemaining = 0.0f;
    bool bFirstPersonAnimationBlueprintActive = false;
    bool bUsingDevelopmentFirstPersonAnimation = false;
    FTimerHandle SlideTimerHandle;
    FTimerHandle InteractionHoldTimerHandle;
    TWeakObjectPtr<AActor> PendingInteraction;
    float InteractionHoldElapsed = 0.0f;
    float InteractionHoldDuration = 0.0f;
    TWeakObjectPtr<AActor> ActiveLadder;
    TWeakObjectPtr<AActor> ActiveMountedWeapon;
    mutable TWeakObjectPtr<ABMMissionDirector> CachedMissionDirector;
    bool bHadPlayableCameraCue = false;
    bool bAwaitingRuntimeFloor = false;
    FVector LastSafeGroundLocation = FVector::ZeroVector;
    FRotator LastSafeGroundRotation = FRotator::ZeroRotator;
    bool bHasSafeGroundLocation = false;
    bool bClientWeaponStateRecovered = false;
    bool bThrowablePresentationConfigured = false;
    bool bWeaponHiddenForThrowable = false;
    bool bThrowableThreatActive = false;
    FVector ThrowableThreatLocation = FVector::ZeroVector;
    float ThrowableThreatDistance = 0.0f;
    EBMWeaponId ThrowablePresentationId = EBMWeaponId::FragGrenade;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
