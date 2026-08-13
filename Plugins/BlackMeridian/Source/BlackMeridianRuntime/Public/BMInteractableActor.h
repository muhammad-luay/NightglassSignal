// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BMCoreTypes.h"
#include "BMInteractionInterface.h"
#include "BMInteractableActor.generated.h"

class UStaticMeshComponent;

UCLASS(Blueprintable)
class BLACKMERIDIANRUNTIME_API ABMInteractableActor : public AActor, public IBMInteractionInterface
{
    GENERATED_BODY()

public:
    ABMInteractableActor();

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    FName InteractionId = TEXT("Interactable");

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    FText Prompt;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    EBMInteractionType InteractionType = EBMInteractionType::Hold;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    EBMInteractionAction InteractionAction = EBMInteractionAction::Generic;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.1", EditCondition = "InteractionType == EBMInteractionType::Hold"))
    float HoldDuration = 0.75f;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Interaction|Pickup", meta = (EditCondition = "InteractionAction == EBMInteractionAction::PickupWeapon"))
    EBMWeaponId PickupWeaponId = EBMWeaponId::ARX4;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Interaction|Pickup", meta = (ClampMin = "1", EditCondition = "InteractionAction == EBMInteractionAction::PickupAmmo"))
    int32 PickupAmmoAmount = 30;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Interaction|Pickup", meta = (ClampMin = "0", EditCondition = "InteractionAction == EBMInteractionAction::PickupThrowable"))
    int32 PickupFragAmount = 1;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Interaction|Pickup", meta = (ClampMin = "0", EditCondition = "InteractionAction == EBMInteractionAction::PickupThrowable"))
    int32 PickupVeilAmount = 0;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Interaction|Pickup", meta = (EditCondition = "InteractionAction == EBMInteractionAction::PickupAttachment"))
    FName PickupAttachmentId = NAME_None;

    /** Weapon and ammo pickups are collected by proximity rather than a key press. */
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Interaction|Pickup")
    bool bAutoPickup = false;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Interaction|Pickup", meta = (ClampMin = "40.0", EditCondition = "bAutoPickup"))
    float AutoPickupRadius = 180.0f;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Interaction|Revive", meta = (EditCondition = "InteractionAction == EBMInteractionAction::ReviveAlly"))
    TObjectPtr<AActor> ReviveTarget;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    bool bDestroyAfterInteraction = false;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    bool bActsAsDoor = false;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Interaction")
    bool IsAvailableForMissionPhase(EBMMissionPhase MissionPhase) const;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Interaction|Pickup")
    void SetWeaponPickupPresentation(EBMWeaponId WeaponId);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Interaction|Pickup")
    void SetThrowablePickupPresentation(EBMWeaponId ThrowableId);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Interaction|Pickup")
    void SetAttachmentPickupPresentation(FName AttachmentId);

    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionPrompt_Implementation() const override { return Prompt; }
    virtual EBMInteractionType GetInteractionType_Implementation() const override { return InteractionType; }
    virtual EBMInteractionAction GetInteractionAction_Implementation() const override { return InteractionAction; }
    virtual float GetInteractionHoldDuration_Implementation() const override { return FMath::Max(0.1f, HoldDuration); }

    virtual void Tick(float DeltaSeconds) override;

    void SetPresentationColor(const FLinearColor& Color);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation")
    TObjectPtr<UStaticMeshComponent> Mesh;

    void PerformInteraction(AActor* Interactor);

    bool bAutoPickupInProgress = false;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
