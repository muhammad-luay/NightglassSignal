// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BMCoreTypes.h"
#include "BMSignalBotController.generated.h"

class AActor;

UCLASS()
class BLACKMERIDIANRUNTIME_API ABMSignalBotController : public AAIController
{
    GENERATED_BODY()

public:
    ABMSignalBotController();

    virtual void OnPossess(APawn* InPawn) override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Signal Control|Bot")
    void SetBotTeam(EBMTeam NewTeam) { BotTeam = NewTeam; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Signal Control|Bot")
    EBMTeam GetBotTeam() const { return BotTeam; }

private:
    TWeakObjectPtr<AActor> FindNearestOpposingTarget() const;
    void UpdateDecision();
    void ApplyMovement(float DeltaSeconds);
    void UpdateFacing(float DeltaSeconds);
    void SetMovementDestination(const FVector& Destination, float AcceptanceRadius);
    void ClearMovementDestination(bool bResetDirection = false);

    float DecisionTimer = 0.0f;
    EBMTeam BotTeam = EBMTeam::Neutral;
    TWeakObjectPtr<AActor> ActiveTarget;
    FVector MovementDestination = FVector::ZeroVector;
    FVector MovementDirection = FVector::ZeroVector;
    float SmoothedFacingYaw = 0.0f;
    float MovementAcceptanceRadius = 260.0f;
    bool bHasMovementDestination = false;
    bool bHasSmoothedFacingYaw = false;

#if !UE_BUILD_SHIPPING
    float DiagnosticLogTimer = 0.0f;
#endif
};
