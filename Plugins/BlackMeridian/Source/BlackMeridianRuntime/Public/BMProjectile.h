// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BMProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
class UBMAudioDataAsset;
class UBMVFXDataAsset;

/** Server-authoritative projectile seam for launchers and future guided ordnance. */
UCLASS(Blueprintable)
class BLACKMERIDIANRUNTIME_API ABMProjectile : public AActor
{
    GENERATED_BODY()

public:
    ABMProjectile();

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Projectile")
    void InitializeProjectile(const FVector& Direction, float Speed, float InDamage, float InRadius, float InGravityScale);

    /**
     * Player-fired explosive rounds still use their normal blast presentation,
     * but character damage is constrained to the actor that was actually
     * under the player's crosshair.  Bots leave this constraint disabled so
     * their ordinary target/area-damage behavior is unchanged.
     */
    void SetPlayerCrosshairDamageConstraint(bool bEnabled, AActor* CrosshairTarget);

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Projectile")
    float Damage = 85.0f;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Projectile")
    float Radius = 220.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Presentation")
    TSoftObjectPtr<UBMAudioDataAsset> AudioProfile;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Presentation")
    TSoftObjectPtr<UBMVFXDataAsset> VFXProfile;

protected:
    UFUNCTION()
    void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

    UFUNCTION()
    void HandleBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    void Detonate(const FVector& Location);

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastDetonation(FVector_NetQuantize Location);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
    TObjectPtr<USphereComponent> Collision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

private:
    bool bDetonated = false;
    bool bPlayerCrosshairDamageConstrained = false;
    TWeakObjectPtr<AActor> PlayerCrosshairTarget;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
