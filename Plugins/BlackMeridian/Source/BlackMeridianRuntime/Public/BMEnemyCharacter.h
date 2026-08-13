// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BMCombatCharacter.h"
#include "BMEnemyCharacter.generated.h"

class ABMEnemyAIController;
class ABMPlayerCharacter;
class ABMWeapon;
class AActor;
class UAnimSequence;
class USceneComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS(Blueprintable)
class BLACKMERIDIANRUNTIME_API ABMEnemyCharacter : public ABMCombatCharacter
{
    GENERATED_BODY()

public:
    ABMEnemyCharacter(const FObjectInitializer& ObjectInitializer);

    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    EBMAIState AIState = EBMAIState::Patrol;

    UPROPERTY(ReplicatedUsing = OnRep_EnemyRole, EditAnywhere, BlueprintReadOnly, Category = "AI")
    EBMEnemyRole EnemyRole = EBMEnemyRole::Rifleman;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float Aggression = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float ReactionDelay = 0.25f;

    /**
     * Additional cone applied to an AI shot before the weapon's own spread.
     * This keeps hostile fire readable and role-specific instead of making
     * every enemy a perfect hitscan turret.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.0", ClampMax = "12.0"))
    float AimErrorDegrees = 2.8f;

    /**
     * Hostile AI rounds are intentionally less lethal than player rounds so
     * the player has time to read muzzle flashes, react to the damage cue,
     * reach cover, and keep playing the encounter.  Headshots and hit-zone
     * multipliers still apply before this role-owned tuning scalar.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.18", ClampMax = "1.0"))
    float DamageToPlayerMultiplier = 0.68f;

    /**
     * Uses the readable first-wave hostile-damage bucket. This is a server-
     * side encounter flag so the opening can show several armed enemies
     * without turning their combined fire into an immediate failure.
     */
    void SetReadableCombatPacing(bool bEnabled) { bReadableCombatPacing = bEnabled; }
    bool UsesReadableCombatPacing() const { return bReadableCombatPacing; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float SuppressionThreshold = 3.0f;

    /** Authority-side grenade charges keep enemy ordnance readable and fair. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Equipment", meta = (ClampMin = "0"))
    int32 GrenadesRemaining = 1;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|AI")
    void TryFireAt(AActor* Target);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|AI")
    void TryThrowGrenade(AActor* Target);

    /** Delays hostile fire after a mission wave spawns so the encounter has a
     * readable hand-off instead of an unavoidable first volley. */
    void SetCombatStartDelay(float DelaySeconds);

    UFUNCTION(BlueprintPure, Category = "Black Meridian|AI")
    bool IsCombatStartDelayed() const { return CombatStartDelayRemaining > 0.0f; }

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|AI")
    void SetAIState(EBMAIState NewState);

    /**
     * Gives the presentation layer the live combat target. The weapon and
     * upper-body aim and rendered body track this target while the capsule
     * follows its collision-safe travel route, including during movement.
     */
    void SetCombatFacingTarget(AActor* Target);
    void ClearCombatFacingTarget();

    /** True while the authority has assigned a live weapon/aim target. */
    bool IsCombatFacingTargetActive() const
    {
        return bHasCombatFacingTarget || bReplicatedCombatFacing;
    }

    /** Assigns the role on the authority and refreshes its weapon and
     * presentation. Runtime waves should use this instead of writing the
     * replicated property directly. */
    UFUNCTION(BlueprintCallable, Category = "Black Meridian|AI")
    void SetEnemyRole(EBMEnemyRole NewRole);

    UFUNCTION(BlueprintPure, Category = "Black Meridian|AI")
    EBMEnemyRole GetEnemyRole() const { return EnemyRole; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|AI|Presentation")
    FString GetEnemyRoleLabel() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|AI|Presentation")
    FString GetEnemyRoleMarker() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|AI|Presentation")
    FLinearColor GetEnemyRoleColor() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|AI|Tuning")
    float GetPreferredEngagementRange() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|AI|Tuning")
    float GetEngagementRange() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|AI|Tuning")
    float GetRoleMoveSpeed() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|AI|Tuning")
    bool CanThrowGrenades() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|AI|Tuning")
    bool ShouldThrowGrenadeAtDistance(float Distance) const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|AI|Tuning")
    bool ShouldAimAtDistance(float Distance) const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|AI|Tuning")
    float GetDamageToPlayerMultiplier() const { return FMath::Clamp(DamageToPlayerMultiplier, 0.18f, 1.0f); }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|AI")
    float GetFireCooldown() const { return FireCooldown; }

    /** The weapon used by this AI is replicated so remote clients see the same
     * authored-style silhouette and muzzle presentation as the server. */
    UFUNCTION(BlueprintPure, Category = "Black Meridian|AI|Presentation")
    ABMWeapon* GetCombatWeapon() const { return CombatWeapon; }

#if !UE_BUILD_SHIPPING
    /** Development-only movement/facing probe value. */
    float GetCombatAimYawForProbe() const { return CombatAimYaw; }

    /** Draws the authored mesh basis and independent front/back markers. */
    void UpdateCombatAxisProbe();
    bool bCombatAxisProbeScreenshotRequested = false;
#endif

protected:
    virtual void HandleDeath(const FBMDamageContext& DamageContext) override;
    virtual void FinishDeathPresentation() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PlayCharacterAnimation_Implementation(FName AnimationEvent) override;

    UFUNCTION()
    void OnRep_CombatWeapon();

    UFUNCTION()
    void OnRep_CombatAimYaw();

    UFUNCTION()
    void OnRep_EnemyRole();

    UFUNCTION()
    void HandleWeaponEvent(FName EventName);

private:
    void EnsureCombatWeapon();
    void ApplyCombatWeaponAim();
    void ApplyRolePresentation();
    void AttachRoleAccessoriesToAuthoredMesh();
    void InitializeModularPresentation();
    void ApplyModularPresentationTransform();
    void UpdateMovementFacing(float DeltaSeconds);
    void AlignAuthoredMeshToActorFacing();
    FVector GetVisibleVisualForward() const;
    void UpdateCombatVisualFacing(float DeltaSeconds);
    void UpdateRoleAccessoryFacing();
    void UpdateModularLocomotion(float DeltaSeconds);
    void ConfigureModularMaterials();
    /** Applies the small project-owned role kit to the animated character. */
    void ConfigureRoleAccessories();
    void InitializeDevelopmentLocomotion();
    void AlignDevelopmentMeshToCapsule();
    void UpdateDevelopmentLocomotion(float DeltaSeconds);
    void PlayDevelopmentAnimationEvent(FName AnimationEvent);
    void RestoreDevelopmentSingleNodeLocomotion();
    EBMWeaponId GetWeaponIdForRole() const;

    UPROPERTY(ReplicatedUsing = OnRep_CombatWeapon, BlueprintReadOnly, Category = "AI|Presentation", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<ABMWeapon> CombatWeapon;

    /** Replicated weapon/upper-body aim yaw for target tracking. */
    UPROPERTY(ReplicatedUsing = OnRep_CombatAimYaw, BlueprintReadOnly, Category = "AI|Presentation", meta = (AllowPrivateAccess = "true"))
    float CombatAimYaw = 0.0f;

    /** Replicated vertical weapon aim keeps the visible rifle on the same
     * target line as the authoritative shot, including ramps and ledges. */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "AI|Presentation", meta = (AllowPrivateAccess = "true"))
    float CombatAimPitch = 0.0f;

    /** Replicated state bit lets remote clients apply the stationary
     * target-facing mesh offset even though the server-only target pointer is
     * not replicated. */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "AI|Presentation", meta = (AllowPrivateAccess = "true"))
    bool bReplicatedCombatFacing = false;

    float FireCooldown = 0.0f;
    float CombatStartDelayRemaining = 0.0f;
    float GrenadeCooldown = 8.0f;
    bool bReadableCombatPacing = false;
    int32 BurstShotsRemaining = 0;
    TWeakObjectPtr<AActor> BurstTarget;
    EBMCharacterMovementState LastPublishedAnimationState = EBMCharacterMovementState::Idle;
    float LastPublishedAnimationSpeed = -1.0f;
    bool bLastPublishedAnimationAiming = false;
    FVector CombatWeaponBaseLocation = FVector::ZeroVector;
    FRotator CombatWeaponBaseRotation = FRotator::ZeroRotator;
    float CombatWeaponKick = 0.0f;
    float CombatWeaponReloadAlpha = 0.0f;
    bool bCombatWeaponPresentationInitialized = false;

    /** Last post-movement capsule location used to derive body travel yaw. */
    FVector LastFacingLocation = FVector::ZeroVector;
    bool bHasFacingLocation = false;
    TWeakObjectPtr<AActor> CombatFacingTarget;
    FVector CombatFacingLocation = FVector::ZeroVector;
    bool bHasCombatFacingTarget = false;
    /** Smoothed visual yaw offset relative to the movement capsule. */
    float CombatVisualFacingYawOffset = 0.0f;
    /** World-space visual yaw filter. The capsule may follow a route while
     * the rendered soldier tracks its combat target without snapping. */
    float SmoothedCombatVisualYaw = 0.0f;
    bool bCombatVisualYawInitialized = false;
    /** Client-side target yaw smoothing prevents replicated aim updates from
     * stepping the rendered body at the network update rate. */
    float SmoothedCombatAimYaw = 0.0f;
    float SmoothedCombatAimPitch = 0.0f;
    bool bCombatAimYawInitialized = false;
    /** Stable route heading used by the body-facing pass. */
    FVector SmoothedTravelDirection = FVector::ZeroVector;
    /**
     * Calibrated from the project Manny local +X visual basis. This remains
     * independent from animated spine-attached role accessories, so the
     * rendered facing probe cannot validate a false chest/backpack heading.
     */
    FVector AuthoredVisualForwardLocal = FVector(1.0f, 0.0f, 0.0f);
    bool bAuthoredVisualForwardCalibrated = false;
    float LastExpectedMeshRelativeYaw = 0.0f;
    bool bCombatVisualFacingInitialized = false;

#if !UE_BUILD_SHIPPING
    /** Post-physics movement direction used by the exact-package acceptance probe. */
    FVector LastMovementDirectionForProbe = FVector::ZeroVector;
    float MovementProbeLogTimer = 0.0f;
#endif

    /**
     * Project-owned procedural soldier fallback. The packaged build cannot
     * assume the editor's MoverExamples body is cooked, so keep a grounded
     * multi-part silhouette available even when no skeletal mesh is present.
     * All pieces follow PlaceholderBody's hit/death transform so the fallback
     * cannot leave an upright body floating over a fallen hitbox.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Fallback", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> ModularRigRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Fallback", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ModularTorso;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Fallback", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ModularPelvis;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Fallback", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ModularHead;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Fallback", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ModularVisor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Fallback", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ModularBackpack;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Fallback", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ModularLeftArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Fallback", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ModularRightArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Fallback", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ModularLeftLeg;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Fallback", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ModularRightLeg;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Fallback", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ModularLeftFoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Fallback", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ModularRightFoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Fallback", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ModularChestPlate;

    /**
     * Role silhouette kit for the project Manny body. These parts are attached
     * to skeletal bones rather than the actor root, so they follow locomotion,
     * aim, hit reactions, and the replicated death fall on every client.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Role", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> RoleHelmet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Role", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> RoleChestRig;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Role", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> RoleBackpack;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Role", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> RoleLeftShoulder;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Role", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> RoleRightShoulder;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Role", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> RoleAntenna;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> ModularMaterials;

    FVector ModularLeftLegBaseLocation = FVector::ZeroVector;
    FVector ModularRightLegBaseLocation = FVector::ZeroVector;
    FRotator ModularLeftLegBaseRotation = FRotator::ZeroRotator;
    FRotator ModularRightLegBaseRotation = FRotator::ZeroRotator;
    FVector ModularLeftArmBaseLocation = FVector::ZeroVector;
    FVector ModularRightArmBaseLocation = FVector::ZeroVector;
    FRotator ModularLeftArmBaseRotation = FRotator::ZeroRotator;
    FRotator ModularRightArmBaseRotation = FRotator::ZeroRotator;
    FVector ModularTorsoBaseLocation = FVector::ZeroVector;
    bool bModularPresentationReady = false;
    bool bModularLocomotionMoving = false;
    float ModularLocomotionPhase = 0.0f;

    // The engine's development Manny AnimBP is authored for the Mover sample
    // stack, while this enemy intentionally uses CharacterMovementComponent.
    // Keep the sample body, but drive its locomotion explicitly until a
    // project-owned soldier AnimBP is assigned through AnimationData.
    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> DevelopmentIdleAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> DevelopmentWalkAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> DevelopmentRunAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> DevelopmentFallAnimation;

    /**
     * Rifle-ready fallback set used by the installed Manny development body.
     * These are project-owned copies of the engine's mannequin rifle clips so
     * the procedural combat weapon can follow an actual two-handed pose.
     */
    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> DevelopmentRifleReadyAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> DevelopmentRifleWalkAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> DevelopmentRifleRunAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> DevelopmentRifleFallAnimation;

    /** Authored one-shot rifle clips used by the development Manny path. */
    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> DevelopmentRifleFireAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> DevelopmentRifleReloadAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> DevelopmentRifleDryFireAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> DevelopmentRifleHitReactionAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> DevelopmentRifleHeadHitReactionAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> DevelopmentRifleDeathAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> ActiveDevelopmentAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> ActiveDevelopmentEventAnimation;

    /**
     * Additive clips need an AnimBlueprint slot in cooked builds. The normal
     * development locomotion remains single-node; this class is restored only
     * for the short duration of an additive combat event.
     */
    UPROPERTY(Transient)
    TSubclassOf<UAnimInstance> DevelopmentAnimBlueprintClass;

    float DevelopmentEventRemaining = 0.0f;

    bool bUsingDevelopmentLocomotionFallback = false;
    bool bUsingDevelopmentRifleLocomotion = false;
    bool bDevelopmentMeshAligned = false;
    bool bDevelopmentAnimationBlueprintActive = false;
    bool bDevelopmentLocomotionMoving = false;
    /** Keeps the walk clip alive across a one-frame route hand-off. */
    float DevelopmentLocomotionStopGraceRemaining = 0.0f;
    /** Smoothed presentation speed used to keep rifle footfalls stable. */
    float DevelopmentLocomotionSpeed = 0.0f;
    /** Resume the same walk-cycle phase after a temporary additive montage. */
    TObjectPtr<UAnimSequence> DevelopmentLocomotionResumeAnimation;
    float DevelopmentLocomotionResumePosition = 0.0f;

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
