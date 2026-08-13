// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BMCoreTypes.h"
#include "BMThrowableDataAsset.h"
#include "BMThrowable.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UBMAudioDataAsset;
class UBMVFXDataAsset;
class UMaterialInstanceDynamic;
class UPointLightComponent;

UCLASS(Blueprintable)
class BLACKMERIDIANRUNTIME_API ABMThrowable : public AActor
{
    GENERATED_BODY()

public:
    ABMThrowable();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(ReplicatedUsing = OnRep_ThrowableId, EditAnywhere, BlueprintReadOnly, Category = "Throwable")
    EBMWeaponId ThrowableId = EBMWeaponId::FragGrenade;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable")
    float FuseSeconds = 2.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable")
    float Damage = 110.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable")
    float Radius = 360.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable|Tactical", meta = (ClampMin = "0.0"))
    float SuppressionAmount = 6.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Presentation")
    TSoftObjectPtr<UBMAudioDataAsset> AudioProfile;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Presentation")
    TSoftObjectPtr<UBMVFXDataAsset> VFXProfile;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Throwable")
    void InitializeFromData(UBMThrowableDataAsset* InThrowableData);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Throwable")
    void ArmAndThrow(const FVector& Direction, float Speed = 950.0f);

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Throwable|Presentation")
    bool IsDetonationPresentationActive() const { return bDetonationPresentationActive; }

protected:
    void Detonate();

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastDetonation(FVector_NetQuantize Location, bool bTactical);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throwable")
    TObjectPtr<USphereComponent> Collision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throwable")
    TObjectPtr<UStaticMeshComponent> Mesh;

    /** Runtime fallback burst keeps detonation readable until authored Niagara is assigned. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throwable|Presentation")
    TObjectPtr<UStaticMeshComponent> DetonationFlashMesh;

    /** Expanding ground ring gives the detonation a readable gameplay footprint. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throwable|Presentation")
    TObjectPtr<UStaticMeshComponent> DetonationRingMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throwable|Presentation")
    TObjectPtr<UPointLightComponent> DetonationLight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throwable")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

private:
    void ApplyPresentationMaterial();
    void StartFallbackDetonationPresentation(bool bTactical);

    UPROPERTY(Transient)
    TObjectPtr<UBMThrowableDataAsset> ThrowableData;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> PresentationMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> DetonationMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> DetonationRingMaterial;

    bool bUsingProjectMesh = false;
    bool bDetonationPresentationActive = false;
    bool bDetonationTactical = false;
    float DetonationPresentationElapsed = 0.0f;

    UFUNCTION()
    void OnRep_ThrowableId();

    FTimerHandle FuseTimer;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
