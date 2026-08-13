// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BMCoreTypes.h"
#include "BMCharacterAnimationInterface.h"
#include "BMCombatCharacter.generated.h"

class UBMHealthComponent;
class UBMCombatHistoryComponent;
class UBMSuppressionComponent;
class UBMCharacterAnimationDataAsset;
class UBMAudioDataAsset;
class UAnimInstance;
class UCapsuleComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBMCombatCharacterEliminated, AActor*, EliminatedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FBMCharacterAnimationStateChanged,
    EBMCharacterMovementState,
    MovementState,
    float,
    GroundSpeed,
    bool,
    bAiming);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBMCharacterAnimationEvent, FName, AnimationEvent);

UCLASS(Abstract, Blueprintable)
class BLACKMERIDIANRUNTIME_API ABMCombatCharacter : public ACharacter, public IBMCharacterAnimationInterface
{
    GENERATED_BODY()

public:
    ABMCombatCharacter(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UBMHealthComponent> HealthComponent;

    /** Authority-only short pose history used by server hitscan rewind. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Networking")
    TObjectPtr<UBMCombatHistoryComponent> CombatHistoryComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UBMSuppressionComponent> SuppressionComponent;

    /**
     * Smaller visibility-only hitbox used by the reticle and authoritative
     * weapon traces.  The movement capsule remains separate so an invisible
     * locomotion margin cannot turn a near miss beside the visible character
     * into a hit.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UCapsuleComponent> CombatAimHitbox;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Combat")
    UCapsuleComponent* GetCombatAimHitbox() const { return CombatAimHitbox; }

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Combat")
    EBMTeam Team = EBMTeam::Neutral;

    UPROPERTY(ReplicatedUsing = OnRep_Eliminated, BlueprintReadOnly, Category = "Combat")
    bool bEliminated = false;

    UPROPERTY(BlueprintAssignable, Category = "Combat")
    FBMCombatCharacterEliminated OnEliminated;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Animation")
    TObjectPtr<UBMCharacterAnimationDataAsset> AnimationData;

    /**
     * Optional authored third-person body. Set this on a Blueprint child of
     * the player/enemy class; the runtime cube is automatically hidden when
     * the mesh loads successfully.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Character")
    TSoftObjectPtr<USkeletalMesh> AuthoredCharacterMesh;

    /**
     * Optional authored AnimBP used when AnimationData is not supplied. This
     * keeps a source-only prototype able to show a real moving character while
     * still allowing a Blueprint/data asset to own the final presentation.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Animation")
    TSoftClassPtr<UAnimInstance> AuthoredAnimBlueprint;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Audio")
    TSoftObjectPtr<UBMAudioDataAsset> AudioProfile;

    UPROPERTY(BlueprintAssignable, Category = "Presentation|Animation")
    FBMCharacterAnimationStateChanged OnAnimationStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Presentation|Animation")
    FBMCharacterAnimationEvent OnAnimationEvent;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Combat")
    EBMTeam GetTeam() const { return Team; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Combat")
    bool IsEliminated() const { return bEliminated; }

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Combat")
    void SetTeam(EBMTeam NewTeam);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Combat")
    virtual void RestoreForCheckpoint(float InHealth);

    /**
     * Authority-owned hit presentation. The source direction is expressed
     * from the attacker toward this character so every client can reproduce
     * the same reaction without accepting a client hit result.
     */
    UFUNCTION(NetMulticast, Unreliable)
    void MulticastHitReaction(FVector_NetQuantizeNormal SourceDirection, EBMHitZone HitZone);

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Animation")
    EBMCharacterMovementState GetCharacterAnimationState() const { return AnimationState; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Animation")
    float GetAnimationGroundSpeed() const { return AnimationGroundSpeed; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Animation")
    bool IsAnimationAiming() const { return bAnimationAiming; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Animation")
    FRotator GetAnimationAimOffset() const { return AnimationAimOffset; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Presentation")
    bool IsUsingAuthoredCharacterMesh() const { return bUsingAuthoredCharacterMesh; }

    virtual void SetCharacterAnimationState_Implementation(
        EBMCharacterMovementState NewState,
        float GroundSpeed,
        bool bAiming) override;

    virtual void SetCharacterAimOffset_Implementation(FRotator AimRotation) override;

    virtual void PlayCharacterAnimation_Implementation(FName AnimationEvent) override;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION()
    virtual void HandleDeath(const FBMDamageContext& DamageContext);

    UFUNCTION()
    void HandleHealthChanged(float NewHealth, float DamageTaken);

    UFUNCTION()
    void ClearHitFlash();

    UFUNCTION()
    void ClearHitFeedbackLight();

    UFUNCTION()
    virtual void FinishDeathPresentation();

    UFUNCTION()
    void OnRep_Eliminated();

    UFUNCTION()
    void OnRep_DeathPresentation();

    /** Starts the same local death presentation on authority and clients. */
    void BeginDeathPresentation();
    void ConfigureDeathPresentationMotion();
    void ResetDeathPresentation();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation")
    TObjectPtr<UStaticMeshComponent> PlaceholderBody;

    /**
     * Short zone-aware pulse that keeps hit confirmation visible on authored
     * skeletal meshes as well as the source-only placeholder.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation|Combat")
    TObjectPtr<UPointLightComponent> HitFeedbackLight;

    void ConfigurePlaceholderBody(const FColor& Color);
    void SyncCombatAimHitboxToMovementCapsule();
    void SetAuthoredMeshBaseLocation(const FVector& NewLocation)
    {
        AuthoredMeshBaseLocation = NewLocation;
    }

    void SetAuthoredMeshBaseRotation(const FRotator& NewRotation)
    {
        AuthoredMeshBaseRotation = NewRotation;
    }

    const FRotator& GetAuthoredMeshBaseRotation() const
    {
        return AuthoredMeshBaseRotation;
    }

private:
    UPROPERTY(BlueprintReadOnly, Category = "Presentation|Animation", meta = (AllowPrivateAccess = "true"))
    EBMCharacterMovementState AnimationState = EBMCharacterMovementState::Idle;

    UPROPERTY(BlueprintReadOnly, Category = "Presentation|Animation", meta = (AllowPrivateAccess = "true"))
    float AnimationGroundSpeed = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Presentation|Animation", meta = (AllowPrivateAccess = "true"))
    bool bAnimationAiming = false;

    UPROPERTY(BlueprintReadOnly, Category = "Presentation|Animation", meta = (AllowPrivateAccess = "true"))
    FRotator AnimationAimOffset = FRotator::ZeroRotator;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> PlaceholderMaterial;

    FLinearColor PlaceholderBaseColor = FLinearColor::White;
    FVector PlaceholderBaseLocation = FVector::ZeroVector;
    FVector PlaceholderBaseScale = FVector::OneVector;
    FRotator PlaceholderBaseRotation = FRotator::ZeroRotator;
    FVector AuthoredMeshBaseLocation = FVector::ZeroVector;
    FVector AuthoredMeshBaseScale = FVector::OneVector;
    FRotator AuthoredMeshBaseRotation = FRotator::ZeroRotator;

    /** Replicated so a remote client can reproduce the same directional fall. */
    UPROPERTY(ReplicatedUsing = OnRep_DeathPresentation)
    FVector_NetQuantizeNormal DeathPresentationDirection = FVector::ForwardVector;

    UPROPERTY(Replicated)
    EBMHitZone DeathPresentationHitZone = EBMHitZone::Torso;

    FVector DeathPresentationLocalDirection = FVector::ForwardVector;
    FVector DeathPresentationTargetOffset = FVector(0.0f, 0.0f, -54.0f);
    float DeathPresentationPitch = 0.0f;
    float DeathPresentationYaw = 0.0f;
    float DeathPresentationRoll = 0.0f;
    float DeathPresentationDuration = 1.05f;
    FVector HitReactionLocalDirection = FVector::ForwardVector;
    float HitReactionScale = 1.0f;
    float HitReactionBlend = 0.0f;
    float DeathPresentationBlend = 0.0f;
    bool bDirectionalHitReactionActive = false;
    bool bDeathPresentationActive = false;
    FTimerHandle HitFlashTimerHandle;
    FTimerHandle HitFeedbackLightTimerHandle;
    FTimerHandle DeathPresentationTimerHandle;
    bool bUsingAuthoredCharacterMesh = false;

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
