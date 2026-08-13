// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BMSuppressionComponent.generated.h"

UCLASS(ClassGroup = (BlackMeridian), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class BLACKMERIDIANRUNTIME_API UBMSuppressionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBMSuppressionComponent();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Suppression")
    void AddSuppression(float Amount);

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Suppression")
    bool IsSuppressed() const { return Suppression >= SuppressionThreshold; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Suppression")
    float GetSuppressionPercent() const { return MaxSuppression > 0.0f ? Suppression / MaxSuppression : 0.0f; }

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Suppression")
    float Suppression = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suppression")
    float MaxSuppression = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suppression")
    float SuppressionThreshold = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suppression")
    float DecayPerSecond = 1.25f;

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
