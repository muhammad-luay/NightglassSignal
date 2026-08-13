// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BMCoreTypes.h"
#include "BMHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBMHealthChangedSignature, float, NewHealth, float, DamageTaken);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBMDeathSignature, const FBMDamageContext&, DamageContext);

UCLASS(ClassGroup = (BlackMeridian), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class BLACKMERIDIANRUNTIME_API UBMHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBMHealthComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Health")
    float Health = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
    float MaxHealth = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
    bool bRegenerateHealth = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (EditCondition = "bRegenerateHealth"))
    float RegenerationDelay = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (EditCondition = "bRegenerateHealth"))
    float RegenerationPerSecond = 18.0f;

    UPROPERTY(BlueprintAssignable, Category = "Health")
    FBMHealthChangedSignature OnHealthChanged;

    UPROPERTY(BlueprintAssignable, Category = "Health")
    FBMDeathSignature OnDeath;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Health")
    float ApplyDamage(float Damage, const FBMDamageContext& DamageContext);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Health")
    void RestoreHealth(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Health")
    void SetHealthForCheckpoint(float InHealth);

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Health")
    bool IsAlive() const { return Health > 0.0f; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Health")
    float GetHealthPercent() const { return MaxHealth > 0.0f ? Health / MaxHealth : 0.0f; }

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION()
    void OnRep_Health();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    float TimeSinceDamage = 0.0f;
    float LastReplicatedHealth = 100.0f;
    float CriticalRecoveryTimeRemaining = 0.0f;
    bool bCriticalRecoveryUsed = false;
};
