// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMSignalBotController.h"

#include "BMCombatCharacter.h"
#include "BMControlNode.h"
#include "BMGameState.h"
#include "BMPlayerCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
    constexpr float SignalBotDecisionIntervalSeconds = 0.12f;
    constexpr float SignalBotStopDistance = 760.0f;
    constexpr float SignalBotResumeDistance = 860.0f;
    constexpr float SignalBotFireDistance = 900.0f;
    constexpr float SignalBotDestinationRetargetDistance = 180.0f;
    constexpr float SignalBotTargetSwitchMargin = 220.0f;
    constexpr float SignalBotFacingRateDegreesPerSecond = 900.0f;
    constexpr float SignalBotSteeringRateDegreesPerSecond = 280.0f;
}

ABMSignalBotController::ABMSignalBotController()
{
    PrimaryActorTick.bCanEverTick = true;
    // CharacterMovement consumes pending input in pre-physics. Keep the bot's
    // decision and input submission ahead of that consume step so a route is
    // continuous instead of arriving as alternating input/no-input frames.
    PrimaryActorTick.TickGroup = TG_PrePhysics;
}

void ABMSignalBotController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    DecisionTimer = 0.0f;
    ActiveTarget.Reset();
    MovementDestination = InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;
    MovementDirection = FVector::ZeroVector;
    MovementAcceptanceRadius = 260.0f;
    bHasMovementDestination = false;
    SmoothedFacingYaw = InPawn ? InPawn->GetActorRotation().Yaw : 0.0f;
    bHasSmoothedFacingYaw = InPawn != nullptr;

    if (ABMPlayerCharacter* Bot = Cast<ABMPlayerCharacter>(InPawn))
    {
        // This controller owns bot body rotation. ACharacter's automatic
        // movement rotation must not run after this tick and overwrite the
        // target-facing heading with the route heading on the next physics
        // step. That controller-vs-movement fight was the source of the
        // visible yaw jitter and the occasional back-facing bot.
        Bot->bUseControllerRotationPitch = false;
        Bot->bUseControllerRotationYaw = false;
        Bot->bUseControllerRotationRoll = false;
        if (UCharacterMovementComponent* Movement = Bot->GetCharacterMovement())
        {
            Movement->PrimaryComponentTick.TickGroup = TG_PrePhysics;
            Movement->bOrientRotationToMovement = false;
            Movement->bUseControllerDesiredRotation = false;
            // CharacterMovement consumes pending input in pre-physics. Make
            // this controller's input-producing tick an explicit prerequisite
            // so the scheduler cannot run movement first on a crowded frame.
            Movement->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
        }
    }
#if !UE_BUILD_SHIPPING
    DiagnosticLogTimer = 0.0f;
#endif
}

void ABMSignalBotController::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    ABMPlayerCharacter* Bot = Cast<ABMPlayerCharacter>(GetPawn());
    if (!Bot || Bot->IsEliminated())
    {
        return;
    }

    if (const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
        State && State->bSignalControlMode && (!State->bMatchLive || State->SignalControl.bMatchFinished))
    {
        ActiveTarget.Reset();
        ClearMovementDestination(true);
        return;
    }

    // Target selection and weapon actions are rate-limited, but locomotion
    // and body facing are frame-driven. The old controller returned for
    // 0.35 seconds after each decision, leaving only a short movement pulse
    // and making the remote mannequin appear to stutter or glide.
    DecisionTimer -= FMath::Max(DeltaSeconds, 0.0f);
    if (DecisionTimer <= 0.0f)
    {
        DecisionTimer = SignalBotDecisionIntervalSeconds;
        UpdateDecision();
    }

    ApplyMovement(DeltaSeconds);
    UpdateFacing(DeltaSeconds);

#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_SignalControlProbe")))
    {
        DiagnosticLogTimer -= FMath::Max(DeltaSeconds, 0.0f);
        if (DiagnosticLogTimer <= 0.0f)
        {
            DiagnosticLogTimer = 0.6f;
            float TargetYaw = Bot->GetActorRotation().Yaw;
            float BodyToTargetYaw = 0.0f;
            if (AActor* Target = ActiveTarget.Get(); IsValid(Target))
            {
                FVector ToTarget = Target->GetActorLocation() - Bot->GetActorLocation();
                ToTarget.Z = 0.0f;
                if (!ToTarget.IsNearlyZero())
                {
                    TargetYaw = ToTarget.Rotation().Yaw;
                    BodyToTargetYaw = FMath::FindDeltaAngleDegrees(
                        Bot->GetActorRotation().Yaw,
                        TargetYaw);
                }
            }
            const float RouteYaw = MovementDirection.IsNearlyZero()
                ? Bot->GetActorRotation().Yaw
                : MovementDirection.Rotation().Yaw;
            const float BodyToRouteYaw = FMath::FindDeltaAngleDegrees(
                Bot->GetActorRotation().Yaw,
                RouteYaw);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian signal bot: pawn=%s target=%s location=%s velocity=%s body_yaw=%.1f target_yaw=%.1f body_to_target_yaw=%.1f route_yaw=%.1f body_to_route_yaw=%.1f face_yaw=%.1f moving=%s destination=%s"),
                *GetNameSafe(Bot),
                *GetNameSafe(ActiveTarget.Get()),
                *Bot->GetActorLocation().ToString(),
                *Bot->GetVelocity().ToString(),
                Bot->GetActorRotation().Yaw,
                TargetYaw,
                BodyToTargetYaw,
                RouteYaw,
                BodyToRouteYaw,
                Bot->GetController() ? Bot->GetController()->GetControlRotation().Yaw : Bot->GetActorRotation().Yaw,
                bHasMovementDestination ? TEXT("yes") : TEXT("no"),
                *MovementDestination.ToString());
        }
    }
#endif
}

void ABMSignalBotController::UpdateDecision()
{
    ABMPlayerCharacter* Bot = Cast<ABMPlayerCharacter>(GetPawn());
    if (!Bot || Bot->IsEliminated())
    {
        return;
    }

    // Keep a live target until another opponent is materially closer. Picking
    // the nearest actor afresh every 120 ms makes two crossing bots exchange
    // destinations, headings, and fire targets in quick succession. That
    // looks like jitter even when each individual turn is rate-limited.
    TWeakObjectPtr<AActor> NearestTarget = FindNearestOpposingTarget();
    AActor* CurrentTarget = ActiveTarget.Get();
    const ABMCombatCharacter* CurrentCombatTarget = Cast<ABMCombatCharacter>(CurrentTarget);
    const bool bCurrentTargetValid = IsValid(CurrentTarget)
        && CurrentTarget != Bot
        && CurrentCombatTarget
        && !CurrentCombatTarget->IsEliminated()
        && CurrentCombatTarget->GetTeam() != Bot->GetTeam()
        && CurrentCombatTarget->GetTeam() != EBMTeam::Neutral;
    if (!bCurrentTargetValid)
    {
        ActiveTarget = NearestTarget;
    }
    else if (NearestTarget.IsValid() && NearestTarget.Get() != CurrentTarget)
    {
        const float CurrentDistance = FVector::Dist2D(Bot->GetActorLocation(), CurrentTarget->GetActorLocation());
        const float NearestDistance = FVector::Dist2D(Bot->GetActorLocation(), NearestTarget->GetActorLocation());
        if (NearestDistance + SignalBotTargetSwitchMargin < CurrentDistance)
        {
            ActiveTarget = NearestTarget;
        }
    }

    if (AActor* Target = ActiveTarget.Get())
    {
        const float Distance = FVector::Dist2D(Bot->GetActorLocation(), Target->GetActorLocation());
        const bool bShouldClose = bHasMovementDestination
            ? Distance > SignalBotStopDistance
            : Distance > SignalBotResumeDistance;
        if (bShouldClose)
        {
            SetMovementDestination(Target->GetActorLocation(), 180.0f);
        }
        else
        {
            // Preserve the last route heading through a range hold. If the
            // target moves out again, the new route eases from that heading
            // instead of starting from zero and producing a lurch.
            ClearMovementDestination(false);
        }

        if (Distance <= SignalBotFireDistance)
        {
            Bot->BotFireAt(Target);
        }
        return;
    }

    ActiveTarget.Reset();
    for (TActorIterator<ABMControlNode> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        if (ABMControlNode* Node = *Iterator; Node && Node->bActive)
        {
            const float Distance = FVector::Dist2D(Bot->GetActorLocation(), Node->GetActorLocation());
            if (Distance > 260.0f)
            {
                SetMovementDestination(Node->GetActorLocation(), 260.0f);
            }
            else
            {
                ClearMovementDestination(false);
            }
            break;
        }
    }
}

void ABMSignalBotController::ApplyMovement(const float DeltaSeconds)
{
    ABMPlayerCharacter* Bot = Cast<ABMPlayerCharacter>(GetPawn());
    if (!Bot || Bot->IsEliminated() || !bHasMovementDestination)
    {
        return;
    }

    FVector Offset = MovementDestination - Bot->GetActorLocation();
    Offset.Z = 0.0f;
    if (Offset.SizeSquared2D() <= FMath::Square(MovementAcceptanceRadius))
    {
        ClearMovementDestination();
        return;
    }

    const FVector DesiredDirection = Offset.GetSafeNormal2D();
    if (DesiredDirection.IsNearlyZero())
    {
        ClearMovementDestination();
        return;
    }

    if (DeltaSeconds > KINDA_SMALL_NUMBER && !MovementDirection.IsNearlyZero())
    {
        const float CurrentHeading = MovementDirection.Rotation().Yaw;
        const float DesiredHeading = DesiredDirection.Rotation().Yaw;
        const float SmoothedHeading = FMath::FixedTurn(
            CurrentHeading,
            DesiredHeading,
            SignalBotSteeringRateDegreesPerSecond * DeltaSeconds);
        MovementDirection = FRotator(0.0f, SmoothedHeading, 0.0f).Vector().GetSafeNormal2D();
    }
    else
    {
        MovementDirection = DesiredDirection;
    }

    Bot->AddMovementInput(MovementDirection, 1.0f);
}

void ABMSignalBotController::UpdateFacing(const float DeltaSeconds)
{
    ABMPlayerCharacter* Bot = Cast<ABMPlayerCharacter>(GetPawn());
    if (!Bot || Bot->IsEliminated() || DeltaSeconds <= 0.0f)
    {
        return;
    }

    FVector FacingDirection = FVector::ZeroVector;
    if (AActor* Target = ActiveTarget.Get(); IsValid(Target))
    {
        FacingDirection = Target->GetActorLocation() - Bot->GetActorLocation();
        FacingDirection.Z = 0.0f;
    }
    else if (!MovementDirection.IsNearlyZero())
    {
        FacingDirection = MovementDirection;
    }
    else if (bHasMovementDestination)
    {
        FacingDirection = MovementDestination - Bot->GetActorLocation();
        FacingDirection.Z = 0.0f;
    }

    FacingDirection = FacingDirection.GetSafeNormal2D();
    if (FacingDirection.IsNearlyZero())
    {
        return;
    }

    const float DesiredYaw = FacingDirection.Rotation().Yaw;
    if (!bHasSmoothedFacingYaw)
    {
        SmoothedFacingYaw = Bot->GetActorRotation().Yaw;
        bHasSmoothedFacingYaw = true;
    }
    // Keep an independent, continuous body-heading state. The controller
    // rotation is still the instantaneous aim heading, but the actor body
    // only follows the bounded state; this prevents target replication steps
    // and route corrections from snapping the visible mannequin.
    SmoothedFacingYaw = FMath::FixedTurn(
        SmoothedFacingYaw,
        DesiredYaw,
        SignalBotFacingRateDegreesPerSecond * DeltaSeconds);
    SetControlRotation(FRotator(0.0f, DesiredYaw, 0.0f));

    const float NewYaw = SmoothedFacingYaw;
    if (FMath::Abs(FMath::FindDeltaAngleDegrees(Bot->GetActorRotation().Yaw, NewYaw)) > 0.05f)
    {
        Bot->SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
    }
}

void ABMSignalBotController::SetMovementDestination(
    const FVector& Destination,
    const float AcceptanceRadius)
{
    const FVector FlatDestination(Destination.X, Destination.Y, MovementDestination.Z);
    if (!bHasMovementDestination
        || FVector::DistSquared2D(MovementDestination, FlatDestination)
            > FMath::Square(SignalBotDestinationRetargetDistance))
    {
        MovementDestination = FlatDestination;
    }
    MovementAcceptanceRadius = FMath::Max(1.0f, AcceptanceRadius);
    bHasMovementDestination = true;
}

void ABMSignalBotController::ClearMovementDestination(const bool bResetDirection)
{
    bHasMovementDestination = false;
    if (bResetDirection)
    {
        MovementDirection = FVector::ZeroVector;
    }
}

TWeakObjectPtr<AActor> ABMSignalBotController::FindNearestOpposingTarget() const
{
    const ABMCombatCharacter* Bot = Cast<ABMCombatCharacter>(GetPawn());
    if (!Bot || !GetWorld())
    {
        return TWeakObjectPtr<AActor>();
    }

    TWeakObjectPtr<AActor> BestTarget;
    float BestDistanceSquared = MAX_FLT;
    for (TActorIterator<ABMCombatCharacter> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        ABMCombatCharacter* Candidate = *Iterator;
        if (!Candidate || Candidate == Bot || Candidate->IsEliminated() || Candidate->GetTeam() == Bot->GetTeam() || Candidate->GetTeam() == EBMTeam::Neutral)
        {
            continue;
        }
        const float DistanceSquared = FVector::DistSquared(Bot->GetActorLocation(), Candidate->GetActorLocation());
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestTarget = Candidate;
        }
    }
    return BestTarget;
}
