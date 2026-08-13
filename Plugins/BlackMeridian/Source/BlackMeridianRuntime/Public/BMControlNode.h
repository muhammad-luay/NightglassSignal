// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BMCoreTypes.h"
#include "BMControlNode.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
class ABMCombatCharacter;

UCLASS(Blueprintable)
class BLACKMERIDIANRUNTIME_API ABMControlNode : public AActor
{
    GENERATED_BODY()

public:
    ABMControlNode();

    virtual void BeginPlay() override;

    UPROPERTY(ReplicatedUsing = OnRep_Active, EditAnywhere, BlueprintReadOnly, Category = "Signal Control")
    bool bActive = false;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Signal Control")
    int32 NodeIndex = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Signal Control")
    EBMTeam ControllingTeam = EBMTeam::Neutral;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Signal Control")
    void SetActive(bool bNewActive);

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Signal Control")
    EBMTeam GetControllingTeam() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Signal Control")
    bool ContainsCharacter(const ABMCombatCharacter* Character) const;

protected:
    UFUNCTION()
    void OnRep_Active();

    UFUNCTION()
    void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Signal Control")
    TObjectPtr<USphereComponent> CaptureVolume;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Signal Control")
    TObjectPtr<UStaticMeshComponent> MarkerMesh;

    UPROPERTY()
    TArray<TObjectPtr<AActor>> OverlappingActors;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
