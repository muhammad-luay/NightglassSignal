// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "BMEnemyAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class ABMEnemyCharacter;
class AActor;

UCLASS()
class BLACKMERIDIANRUNTIME_API ABMEnemyAIController : public AAIController
{
    GENERATED_BODY()

public:
    ABMEnemyAIController();

    virtual void OnPossess(APawn* InPawn) override;
    virtual void Tick(float DeltaSeconds) override;

    /**
     * Movement uses a direct CharacterMovement fallback in the runtime
     * showcase. Expose its state so combat presentation can distinguish a
     * strafing/advancing enemy from a stationary one without guessing from a
     * possibly one-frame-old velocity.
     */
    UFUNCTION(BlueprintPure, Category = "Black Meridian|AI|Movement")
    bool IsFallbackMovementActive() const { return bFallbackMovementActive || bNavigationMovementActive; }

#if !UE_BUILD_SHIPPING
    /**
     * Development-only acceptance hook for the runtime-generated cover kit.
     * It calls the same cover query used by the decision loop without
     * changing the AI state or moving the controlled pawn.
     */
    bool ProbeCoverSelection(const FVector& ThreatLocation, FVector& OutCoverPoint) const;
#endif

protected:
    UFUNCTION()
    void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

private:
    void UpdateDecision(float DeltaSeconds);
    void SetInvestigationPoint(const FVector& Point);
    void MoveWithFallback(const FVector& Destination, float AcceptanceRadius, float DesiredSpeed);
    void ApplyFallbackMovement(float DeltaSeconds = 0.0f);
    void ClearFallbackMovement(const TCHAR* Reason = TEXT("unspecified"), bool bStopImmediately = true);
    bool IsRuntimeNavigationReady() const;
    void ResetCombatManeuver();
    TWeakObjectPtr<AActor> FindNearestLatticeTarget() const;
    FVector FindCoverPoint(const FVector& ThreatLocation) const;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UAIPerceptionComponent> AIPerception;

    UPROPERTY()
    TObjectPtr<UAISenseConfig_Sight> SightConfig;

    UPROPERTY()
    TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

    TWeakObjectPtr<AActor> CurrentTarget;
    TWeakObjectPtr<AActor> PendingTarget;
    FVector LastKnownTargetLocation = FVector::ZeroVector;
    FVector PendingTargetLocation = FVector::ZeroVector;
    FVector PatrolPoint = FVector::ZeroVector;
    FVector FallbackMoveDestination = FVector::ZeroVector;
    FVector FallbackMoveGoal = FVector::ZeroVector;
    FVector FallbackMoveDirection = FVector::ZeroVector;
    FVector FallbackDetourDestination = FVector::ZeroVector;
    FVector CombatManeuverDestination = FVector::ZeroVector;
    float DecisionTimer = 0.0f;
    float ReactionTimer = 0.0f;
    float FallbackMoveAcceptanceRadius = 0.0f;
    float FallbackMoveSpeed = 0.0f;
    float CombatManeuverTimeRemaining = 0.0f;
    TWeakObjectPtr<AActor> CombatManeuverTarget;
    bool bFallbackMovementActive = false;
    bool bNavigationMovementActive = false;
    bool bFallbackDetourActive = false;
    FVector NavigationMoveGoal = FVector::ZeroVector;
    bool bRuntimeNavigationEverReady = false;

#if !UE_BUILD_SHIPPING
    float DiagnosticLogTimer = 0.0f;
    float MovementDiagnosticLogTimer = 0.0f;
    bool bLoggedDirectMovementFallback = false;
    bool bLoggedNavigationWait = false;
#endif
};
