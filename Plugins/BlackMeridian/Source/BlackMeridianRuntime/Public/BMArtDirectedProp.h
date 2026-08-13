// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BMArtDirectedProp.generated.h"

class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UPointLightComponent;

UENUM(BlueprintType)
enum class EBMArtDirectedPropVariant : uint8
{
    Crate,
    Barricade,
    RelayCabinet,
    PipeRack,
    /** Tall project-mesh landmark that establishes scale and carries a signal beacon. */
    SignalTower,
    /** A grounded industrial vehicle used as hard cover or a wreck silhouette. */
    Vehicle,
    /** A tall load-bearing column used to break sightlines and frame interiors. */
    Pillar,
    /** A trunk/canopy composition for forest and overgrown operations. */
    Vegetation,
    /** A table, console, bench, or market-stall silhouette for interior spaces. */
    Furniture,
    /** A low relay-side service building that frames a route without sealing it. */
    RelayBuilding,
    /** A grounded ship or cargo-hull silhouette used to shape a quay sightline. */
    ShipHull,
    /** A grounded loading-crane silhouette for quay and dock operations. */
    DockCrane
};

/**
 * Small reusable runtime-authored prop kit used by the compatibility map.
 *
 * The actor composes the project-owned environment kit with engine fallbacks.
 * That keeps the source-only project portable while giving cover and
 * landmarks consistent material identity, surface tags, and a replicated
 * destruction/reset state.
 */
// Runtime-authored cover and landmark prop used by both the editor and game targets.
UCLASS(Blueprintable)
class BLACKMERIDIANRUNTIME_API ABMArtDirectedProp : public AActor
{
    GENERATED_BODY()

public:
    ABMArtDirectedProp();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(ReplicatedUsing = OnRep_PropPresentation, EditAnywhere, BlueprintReadWrite, Category = "Prop")
    EBMArtDirectedPropVariant Variant = EBMArtDirectedPropVariant::Crate;

    UPROPERTY(ReplicatedUsing = OnRep_PropPresentation, EditAnywhere, BlueprintReadWrite, Category = "Prop|Material")
    FLinearColor BaseColor = FLinearColor(0.20f, 0.24f, 0.27f, 1.0f);

    UPROPERTY(ReplicatedUsing = OnRep_PropPresentation, EditAnywhere, BlueprintReadWrite, Category = "Prop|Material")
    FLinearColor AccentColor = FLinearColor(0.04f, 0.42f, 0.45f, 1.0f);

    UPROPERTY(ReplicatedUsing = OnRep_PropPresentation, EditAnywhere, BlueprintReadWrite, Category = "Prop|Destruction")
    bool bDestructible = false;

    UPROPERTY(ReplicatedUsing = OnRep_Destroyed, VisibleAnywhere, BlueprintReadOnly, Category = "Prop|Destruction")
    bool bDestroyed = false;

    /** Applies the variant and palette after a runtime spawn. */
    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Prop")
    void ConfigureRuntime(
        EBMArtDirectedPropVariant InVariant,
        const FLinearColor& InBaseColor,
        const FLinearColor& InAccentColor,
        bool bInDestructible);

    /** Marks the prop as destroyed and reveals its authored debris state. */
    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Prop")
    void TriggerDestruction();

    /** Applies an authoritative weapon impact to a destructible prop. */
    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Prop|Destruction")
    bool ApplyWeaponImpact(float Damage);

    /** Applies an authoritative impact and preserves its world presentation point. */
    bool ApplyWeaponImpactAtLocation(float Damage, const FVector& ImpactPoint, const FVector& ImpactNormal);

    /** Restores the intact prop state for a checkpoint restart. */
    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Prop")
    void ResetProp();

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Prop")
    bool IsDestroyed() const { return bDestroyed; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Prop|Destruction")
    bool IsDestructible() const { return bDestructible; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Prop|Presentation")
    bool HasSignalBeaconLight() const
    {
        return Variant == EBMArtDirectedPropVariant::SignalTower && SignalBeaconLight != nullptr;
    }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Prop|Presentation")
    float GetSignalBeaconIntensity() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prop|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prop|Components")
    TArray<TObjectPtr<UStaticMeshComponent>> MainParts;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prop|Components")
    TArray<TObjectPtr<UStaticMeshComponent>> DebrisParts;

    /** A short local pulse makes a cover break readable without requiring a Niagara asset. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prop|Presentation")
    TObjectPtr<UPointLightComponent> DestructionLight;

    /** Synchronized burst light for the instant the cover receives a lethal hit. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prop|Presentation")
    TObjectPtr<UPointLightComponent> ImpactLight;

    /** A low-cost route beacon gives signal towers a readable silhouette at night. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prop|Presentation")
    TObjectPtr<UPointLightComponent> SignalBeaconLight;

    UFUNCTION()
    void OnRep_PropPresentation();

    UFUNCTION()
    void OnRep_Destroyed();

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastImpactFeedback(FVector_NetQuantize ImpactPoint, FVector_NetQuantizeNormal ImpactNormal);

    void RebuildPresentation();
    void ApplyVisualState();
    void CacheDebrisTransforms();
    void PlayImpactFeedback(const FVector& ImpactPoint, const FVector& ImpactNormal);
    void PlayDestructionFeedback(const FVector& ImpactPoint, const FVector& ImpactNormal);
    void ClearImpactFeedback();
    void ConfigurePart(
        UStaticMeshComponent* Part,
        UStaticMesh* Mesh,
        const FVector& Location,
        const FRotator& Rotation,
        const FVector& Scale,
        const FLinearColor& Color,
        bool bEnableCollision);

    float RemainingDestructionHealth = 100.0f;
    float DestructionPresentationElapsed = 0.0f;
    bool bDestructionPresentationActive = false;
    FTimerHandle ImpactFeedbackTimerHandle;
    TArray<FVector> DebrisBaseLocations;
    TArray<FRotator> DebrisBaseRotations;
    TArray<FVector> DebrisBaseScales;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
