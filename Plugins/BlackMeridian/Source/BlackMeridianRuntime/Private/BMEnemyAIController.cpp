// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMEnemyAIController.h"

#include "BMCombatCharacter.h"
#include "BMEnemyCharacter.h"
#include "BMHealthComponent.h"
#include "BMPlayerCharacter.h"
#include "BMSuppressionComponent.h"
#include "BMSquadSubsystem.h"
#include "BMCombatTuning.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CollisionShape.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "AI/Navigation/NavigationTypes.h"
#include "NavMesh/RecastNavMesh.h"

ABMEnemyAIController::ABMEnemyAIController()
{
    PrimaryActorTick.bCanEverTick = true;
    // Feed CharacterMovement before its pre-physics tick consumes input. The
    // default controller tick group can drift across engine versions, which
    // leaves one-frame input gaps and makes a direct fallback route stutter.
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight"));
    HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing"));
    SightConfig->SightRadius = 1800.0f;
    SightConfig->LoseSightRadius = 2200.0f;
    SightConfig->PeripheralVisionAngleDegrees = 95.0f;
    SightConfig->SetMaxAge(4.0f);
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
    AIPerception->ConfigureSense(*SightConfig);
    HearingConfig->HearingRange = 1800.0f;
    HearingConfig->SetMaxAge(5.0f);
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
    AIPerception->ConfigureSense(*HearingConfig);
    AIPerception->SetDominantSense(SightConfig->GetSenseImplementation());
}

void ABMEnemyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    PatrolPoint = InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;
    ClearFallbackMovement(TEXT("possess"));
    FallbackMoveGoal = FVector::ZeroVector;
    NavigationMoveGoal = FVector::ZeroVector;
    bNavigationMovementActive = false;
    bRuntimeNavigationEverReady = false;
    ResetCombatManeuver();
#if !UE_BUILD_SHIPPING
    // The combat showcase is also used as a diagnostic harness. Keep the
    // controller alive long enough to report a late pause/tick transition;
    // normal gameplay still pauses AI through the ordinary world pause path.
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe")))
    {
        PrimaryActorTick.SetTickFunctionEnable(true);
        PrimaryActorTick.bTickEvenWhenPaused = true;
    }
    bLoggedDirectMovementFallback = false;
    bLoggedNavigationWait = false;
    DiagnosticLogTimer = 0.0f;
    MovementDiagnosticLogTimer = 0.0f;
#endif
    if (ACharacter* Character = Cast<ACharacter>(InPawn))
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            // CharacterMovement consumes pending input in pre-physics. Make
            // the controller's input-producing tick an explicit prerequisite
            // so the scheduler cannot run movement first on a crowded frame.
            Movement->PrimaryComponentTick.TickGroup = TG_PrePhysics;
            Movement->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
        }
    }
    if (AIPerception)
    {
        AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &ABMEnemyAIController::OnTargetPerceptionUpdated);
    }
}

void ABMEnemyAIController::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    const bool bNavigationReadyThisFrame = IsRuntimeNavigationReady();
    if (bNavigationReadyThisFrame)
    {
        bRuntimeNavigationEverReady = true;
#if !UE_BUILD_SHIPPING
        bLoggedNavigationWait = false;
#endif
    }

    DecisionTimer -= DeltaSeconds;
    if (DecisionTimer <= 0.0f)
    {
        // The controller still makes movement/target decisions at a bounded
        // rate, but 250 ms made a weapon burst look like isolated single
        // shots. 120 ms gives the enemy burst state enough opportunities to
        // present muzzle flashes and hit pressure without turning the AI into
        // a per-frame weapon loop.
        DecisionTimer = 0.12f;
#if !UE_BUILD_SHIPPING
        if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe")))
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian AI decision: controller=%s pawn=%s target=%s pending=%s"),
                *GetNameSafe(this),
                *GetNameSafe(GetPawn()),
                *GetNameSafe(CurrentTarget.Get()),
                *GetNameSafe(PendingTarget.Get()));
        }
#endif
        UpdateDecision(DeltaSeconds);
    }

    CombatManeuverTimeRemaining = FMath::Max(
        0.0f,
        CombatManeuverTimeRemaining - FMath::Max(DeltaSeconds, 0.0f));

#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe")))
    {
        DiagnosticLogTimer -= FMath::Max(DeltaSeconds, 0.0f);
        if (DiagnosticLogTimer <= 0.0f)
        {
            DiagnosticLogTimer = 1.0f;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian AI heartbeat: controller=%s pawn=%s delta=%.3f world_paused=%s controller_tick=%s pawn_tick=%s target=%s pending=%s reaction=%.2f velocity=%s speed=%.1f movement_mode=%d fallback=%s"),
                *GetNameSafe(this),
                *GetNameSafe(GetPawn()),
                DeltaSeconds,
                GetWorld() && GetWorld()->IsPaused() ? TEXT("yes") : TEXT("no"),
                IsActorTickEnabled() ? TEXT("yes") : TEXT("no"),
                GetPawn() && GetPawn()->IsActorTickEnabled() ? TEXT("yes") : TEXT("no"),
                *GetNameSafe(CurrentTarget.Get()),
                *GetNameSafe(PendingTarget.Get()),
                ReactionTimer,
                GetPawn() ? *GetPawn()->GetVelocity().ToString() : TEXT("None"),
                GetPawn() ? GetPawn()->GetVelocity().Size2D() : 0.0f,
                Cast<ACharacter>(GetPawn()) && Cast<ACharacter>(GetPawn())->GetCharacterMovement()
                    ? static_cast<int32>(Cast<ACharacter>(GetPawn())->GetCharacterMovement()->MovementMode)
                    : -1,
                bFallbackMovementActive ? TEXT("yes") : TEXT("no"));
        }
    }

#endif

    // Decisions are selected before input is submitted. This removes the
    // one-frame no-input gap that used to occur when a waypoint was accepted
    // in ApplyFallbackMovement and the next route was created later in the
    // same tick.
    ApplyFallbackMovement(DeltaSeconds);
}

#if !UE_BUILD_SHIPPING
bool ABMEnemyAIController::ProbeCoverSelection(
    const FVector& ThreatLocation,
    FVector& OutCoverPoint) const
{
    OutCoverPoint = FindCoverPoint(ThreatLocation);
    const bool bSelected = !OutCoverPoint.IsNearlyZero();
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian cover probe: pawn=%s threat=%s cover=%s selected=%s"),
        *GetNameSafe(GetPawn()),
        *ThreatLocation.ToString(),
        *OutCoverPoint.ToString(),
        bSelected ? TEXT("yes") : TEXT("no"));
    return bSelected;
}
#endif

void ABMEnemyAIController::OnTargetPerceptionUpdated(AActor* Actor, const FAIStimulus Stimulus)
{
    if (!Actor || !Stimulus.WasSuccessfullySensed())
    {
        if (CurrentTarget.Get() == Actor)
        {
            LastKnownTargetLocation = Stimulus.StimulusLocation;
            CurrentTarget.Reset();
            if (ABMEnemyCharacter* Enemy = Cast<ABMEnemyCharacter>(GetPawn()))
            {
                Enemy->SetAIState(EBMAIState::Investigate);
            }
        }
        return;
    }

    if (Cast<ABMCombatCharacter>(Actor) && Cast<ABMCombatCharacter>(Actor)->GetTeam() == EBMTeam::Lattice)
    {
        const bool bPlayerTarget = Cast<ABMPlayerCharacter>(Actor) != nullptr;

        // Friendly squad members are valid investigation/combat actors, but
        // they must not steal a live player target. Perception reports all
        // Lattice actors through the same callback; without this priority
        // rule a nearby friendly could replace the player in PendingTarget,
        // after which TryFireAt correctly rejects the same-team target and
        // the enemy appears to stop returning fire.
        const bool bCurrentTargetIsPlayer = Cast<ABMPlayerCharacter>(CurrentTarget.Get()) != nullptr;
        const bool bPendingTargetIsPlayer = Cast<ABMPlayerCharacter>(PendingTarget.Get()) != nullptr;
        if (!bPlayerTarget
            && (bCurrentTargetIsPlayer
                || bPendingTargetIsPlayer
                // A friendly stimulus can arrive every perception update.
                // If a player is already in the bounded acquisition radius,
                // ignore that stimulus entirely so it cannot keep resetting
                // the reaction timer before the player target is promoted.
                || FindNearestLatticeTarget().IsValid()))
        {
            return;
        }

        if (ABMEnemyCharacter* Enemy = Cast<ABMEnemyCharacter>(GetPawn()))
        {
            PendingTargetLocation = Stimulus.StimulusLocation.IsNearlyZero() ? Actor->GetActorLocation() : Stimulus.StimulusLocation;
            if (CurrentTarget.Get() != Actor && PendingTarget.Get() != Actor)
            {
                PendingTarget = Actor;
                ReactionTimer = FMath::Max(0.0f, Enemy->ReactionDelay);
            }
            if (ReactionTimer <= 0.0f)
            {
                CurrentTarget = PendingTarget;
                LastKnownTargetLocation = PendingTargetLocation;
                PendingTarget.Reset();
                Enemy->SetAIState(EBMAIState::Engage);
                if (UBMSquadSubsystem* Squad = GetWorld()->GetSubsystem<UBMSquadSubsystem>())
                {
                    Squad->ShareTarget(GetPawn(), Actor, LastKnownTargetLocation);
                }
            }
        }
    }
}

void ABMEnemyAIController::UpdateDecision(const float DeltaSeconds)
{
    ABMEnemyCharacter* Enemy = Cast<ABMEnemyCharacter>(GetPawn());
    if (!Enemy || Enemy->IsEliminated())
    {
        ResetCombatManeuver();
        return;
    }

    // Every fallback route must use the same readable tactical walk as the
    // target-approach route. Keeping independent 105-135 cm/s literals here
    // made cover, patrol, and investigation movement visibly outrun a role's
    // shared cap, especially for marksmen.
    const float RoleMoveSpeed = Enemy->GetRoleMoveSpeed();
    const float TacticalMoveSpeed = BMCombatTuning::GetEnemyApproachSpeed(RoleMoveSpeed, 1.0f);
    const float CloseTacticalMoveSpeed = BMCombatTuning::GetEnemyApproachSpeed(RoleMoveSpeed, 0.0f);

    // Delay the first shot, not the enemy's body. Letting enemies advance to
    // cover during the warning makes the encounter readable and prevents a
    // stationary mannequin line from looking like a broken AI system. The
    // combat character independently rejects fire and grenade requests while
    // this timer is active.
    if (Enemy->IsCombatStartDelayed())
    {
        Enemy->SetAIState(EBMAIState::Patrol);
    }

    // Promote a bounded player target before applying a reaction delay to a
    // friendly stimulus. Without this early promotion, two nearby friendlies
    // can alternate perception callbacks and keep ReactionTimer above zero
    // indefinitely, leaving that enemy investigating instead of firing.
    if (!Cast<ABMPlayerCharacter>(PendingTarget.Get()))
    {
        if (TWeakObjectPtr<AActor> PriorityPlayer = FindNearestLatticeTarget(); PriorityPlayer.IsValid())
        {
            CurrentTarget = PriorityPlayer;
            LastKnownTargetLocation = PriorityPlayer->GetActorLocation();
            PendingTarget.Reset();
            ReactionTimer = 0.0f;
        }
    }

    if (ReactionTimer > 0.0f)
    {
        ReactionTimer = FMath::Max(0.0f, ReactionTimer - DeltaSeconds);
        Enemy->SetAIState(EBMAIState::Investigate);
        if (ReactionTimer > 0.0f)
        {
            MoveWithFallback(PendingTargetLocation, 150.0f, TacticalMoveSpeed);
            return;
        }
    }

    if (PendingTarget.IsValid())
    {
        const bool bCurrentTargetIsPlayer = Cast<ABMPlayerCharacter>(CurrentTarget.Get()) != nullptr;
        const bool bPendingTargetIsPlayer = Cast<ABMPlayerCharacter>(PendingTarget.Get()) != nullptr;
        if (bCurrentTargetIsPlayer && !bPendingTargetIsPlayer)
        {
            // Keep the player as the active combat target. The pending
            // friendly may still be useful to the squad subsystem, but it is
            // not allowed to interrupt return fire on the player.
            PendingTarget.Reset();
        }
        else
        {
            CurrentTarget = PendingTarget;
            LastKnownTargetLocation = PendingTargetLocation;
            PendingTarget.Reset();
            if (UBMSquadSubsystem* Squad = GetWorld()->GetSubsystem<UBMSquadSubsystem>())
            {
                Squad->ShareTarget(GetPawn(), CurrentTarget.Get(), LastKnownTargetLocation);
            }
        }
    }

    // A player target has higher priority than a friendly squad member shared
    // through perception or the squad subsystem. This is deliberately scoped
    // to the nearest player in range so multiplayer still chooses a concrete
    // target instead of becoming omniscient.
    if (TWeakObjectPtr<AActor> PriorityPlayer = FindNearestLatticeTarget(); PriorityPlayer.IsValid())
    {
        if (!Cast<ABMPlayerCharacter>(CurrentTarget.Get()))
        {
            CurrentTarget = PriorityPlayer;
            LastKnownTargetLocation = PriorityPlayer->GetActorLocation();
            PendingTarget.Reset();
            ReactionTimer = 0.0f;
        }
    }

    // Runtime campaign layouts build their collision and Recast data during
    // play. MoveWithFallback prefers path-following once that data is ready,
    // then retains the deterministic CharacterMovement route as a recoverable
    // compatibility path for source-only or editor sessions without nav data.
    if (!CurrentTarget.IsValid())
    {
        if (TWeakObjectPtr<AActor> DirectTarget = FindNearestLatticeTarget(); DirectTarget.IsValid())
        {
            CurrentTarget = DirectTarget;
            LastKnownTargetLocation = DirectTarget->GetActorLocation();
            Enemy->SetAIState(EBMAIState::Engage);
        }
    }

    const ABMCombatCharacter* CurrentFacingTarget = Cast<ABMCombatCharacter>(CurrentTarget.Get());
    if (IsValid(CurrentTarget.Get())
        && CurrentFacingTarget
        && CurrentFacingTarget->GetTeam() != Enemy->GetTeam())
    {
        Enemy->SetCombatFacingTarget(CurrentTarget.Get());
    }
    else
    {
        Enemy->ClearCombatFacingTarget();
    }

    if (Enemy->HealthComponent && Enemy->HealthComponent->GetHealthPercent() < 0.2f)
    {
        ResetCombatManeuver();
        // Retreating has no backward rifle locomotion clip. Release the
        // combat target marker so the weapon can stop tracking while the body
        // turns into its retreat route. The movement-facing presentation also
        // follows velocity, so the authored walk never slides backward.
        Enemy->ClearCombatFacingTarget();
        Enemy->SetAIState(EBMAIState::Retreat);
        MoveWithFallback(PatrolPoint, 150.0f, TacticalMoveSpeed);
        return;
    }

    if (Enemy->SuppressionComponent && Enemy->SuppressionComponent->IsSuppressed())
    {
        ResetCombatManeuver();
        Enemy->SetAIState(EBMAIState::Suppressed);
        ClearFallbackMovement(TEXT("suppressed"));
        StopMovement();
        return;
    }

    if (!CurrentTarget.IsValid())
    {
        ResetCombatManeuver();
        FVector RecentDeathLocation;
        if (UBMSquadSubsystem* Squad = GetWorld()->GetSubsystem<UBMSquadSubsystem>(); Squad && Squad->GetRecentEnemyDeathLocation(this, RecentDeathLocation))
        {
            SetInvestigationPoint(RecentDeathLocation);
            MoveWithFallback(RecentDeathLocation, 120.0f, TacticalMoveSpeed);
            Enemy->SetAIState(EBMAIState::Investigate);
            return;
        }

        TWeakObjectPtr<AActor> SharedTarget;
        FVector SharedLocation;
        if (UBMSquadSubsystem* Squad = GetWorld()->GetSubsystem<UBMSquadSubsystem>(); Squad && Squad->GetSharedTarget(this, SharedTarget, SharedLocation))
        {
            if (Cast<ABMPlayerCharacter>(SharedTarget.Get()) == nullptr
                && FindNearestLatticeTarget().IsValid())
            {
                // A player in range is a stronger combat target than a
                // friendly member broadcast by the squad subsystem.
                return;
            }
            PendingTarget = SharedTarget;
            PendingTargetLocation = SharedLocation;
            ReactionTimer = FMath::Max(0.0f, Enemy->ReactionDelay);
            Enemy->SetAIState(EBMAIState::Investigate);
        }
    }

    if (CurrentTarget.IsValid())
    {
        const ABMCombatCharacter* ActiveCombatTarget = Cast<ABMCombatCharacter>(CurrentTarget.Get());
        if (ActiveCombatTarget && ActiveCombatTarget->GetTeam() != Enemy->GetTeam())
        {
            Enemy->SetCombatFacingTarget(CurrentTarget.Get());
        }
        else
        {
            Enemy->ClearCombatFacingTarget();
        }
        if (CombatManeuverTarget.Get() != CurrentTarget.Get())
        {
            ResetCombatManeuver();
            CombatManeuverTarget = CurrentTarget;
        }
        const float Distance = FVector::Dist(GetPawn()->GetActorLocation(), CurrentTarget->GetActorLocation());
        const float PreferredRange = Enemy->GetPreferredEngagementRange();
        const float EngagementRange = Enemy->GetEngagementRange();
        // Keep the last approach readable. Enemies ease down as they enter
        // the final closing band instead of rushing the player at their role
        // cap. This is deliberately shared by the generated-map fallback and
        // navigation-enabled maps so the encounter has the same pacing on
        // both paths.
        const float ClosingBand = FMath::Max(120.0f, EngagementRange - PreferredRange);
        const float ClosingAlpha = FMath::Clamp(
            (Distance - PreferredRange) / ClosingBand,
            0.0f,
            1.0f);
        // Keep enemy approach in a deliberate combat walk. The previous
        // 0.18..0.36 scale was below a readable locomotion pace and looked
        // like foot shuffling during a first-person playtest. The shared
        // tuning now leaves time to read silhouettes, reload, and choose cover
        // without freezing the attack line.
        const float ApproachSpeed = BMCombatTuning::GetEnemyApproachSpeed(RoleMoveSpeed, ClosingAlpha);
        const ABMCombatCharacter* TargetCharacter = Cast<ABMCombatCharacter>(CurrentTarget.Get());
        const bool bTargetSuppressed = TargetCharacter && TargetCharacter->SuppressionComponent && TargetCharacter->SuppressionComponent->IsSuppressed();
        if (bTargetSuppressed)
        {
            if (Distance > PreferredRange)
            {
                MoveWithFallback(CurrentTarget->GetActorLocation(), PreferredRange, ApproachSpeed);
            }
            else
            {
                ClearFallbackMovement(TEXT("suppressed_range_hold"));
                StopMovement();
            }
            Enemy->TryFireAt(CurrentTarget.Get());
            Enemy->SetAIState(EBMAIState::Advance);
            return;
        }

        // Combat pursuit owns one stable route all the way into the role's
        // preferred range. The previous branch built a new 42 cm pressure
        // waypoint after every acceptance, which repeatedly cleared and
        // re-accelerated CharacterMovement and read as foot jitter.
        if (Distance > PreferredRange + BMCombatTuning::EnemyCombatPursuitHysteresis)
        {
            ResetCombatManeuver();
            MoveWithFallback(CurrentTarget->GetActorLocation(), PreferredRange, ApproachSpeed);
            const bool bClosePressureRole = Enemy->GetEnemyRole() == EBMEnemyRole::Assault
                || Enemy->GetEnemyRole() == EBMEnemyRole::Breacher;
            Enemy->SetAIState(bClosePressureRole ? EBMAIState::Advance : EBMAIState::Flank);
            Enemy->TryThrowGrenade(CurrentTarget.Get());
            // Keep the weapon active while the enemy closes. Waiting for the
            // preferred range made advancing enemies look unarmed even though
            // the combat weapon and AI target were already valid.
            Enemy->TryFireAt(CurrentTarget.Get());
        }
        else
        {
            // Hold the role range with hysteresis. A target must move beyond
            // the extra band before pursuit restarts, so small player motion
            // cannot cause a stop/start loop at the acceptance boundary.
            ResetCombatManeuver();
            ClearFallbackMovement(TEXT("combat_range_hold"), false);
            Enemy->TryFireAt(CurrentTarget.Get());
            Enemy->SetAIState(EBMAIState::Engage);
        }
        return;
    }

    if (!LastKnownTargetLocation.IsNearlyZero())
    {
        ResetCombatManeuver();
        Enemy->ClearCombatFacingTarget();
        MoveWithFallback(LastKnownTargetLocation, 120.0f, TacticalMoveSpeed);
        Enemy->SetAIState(EBMAIState::Investigate);
        return;
    }

    if (FVector::DistSquared(GetPawn()->GetActorLocation(), PatrolPoint) < FMath::Square(120.0f))
    {
        PatrolPoint += FVector(FMath::FRandRange(-450.0f, 450.0f), FMath::FRandRange(-450.0f, 450.0f), 0.0f);
    }
    ResetCombatManeuver();
    Enemy->ClearCombatFacingTarget();
    MoveWithFallback(PatrolPoint, 120.0f, TacticalMoveSpeed);
    Enemy->SetAIState(EBMAIState::Patrol);
}

void ABMEnemyAIController::ResetCombatManeuver()
{
    CombatManeuverDestination = FVector::ZeroVector;
    CombatManeuverTimeRemaining = 0.0f;
    CombatManeuverTarget.Reset();
}

void ABMEnemyAIController::SetInvestigationPoint(const FVector& Point)
{
    LastKnownTargetLocation = Point;
}

bool ABMEnemyAIController::IsRuntimeNavigationReady() const
{
    if (!GetWorld())
    {
        return false;
    }

    UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetNavigationSystem(GetWorld());
    ARecastNavMesh* RecastNavMesh = NavigationSystem
        ? Cast<ARecastNavMesh>(NavigationSystem->GetDefaultNavDataInstance())
        : nullptr;
    if (!RecastNavMesh || !RecastNavMesh->IsRegistered() || RecastNavMesh->GetNumActiveTiles() <= 0)
    {
        return false;
    }

    // Do not hand a pawn to path following while the dynamic layout is still
    // dirty. A path created during that window can be invalidated immediately
    // as the generated wall/cover geometry enters the Recast octree.
    return !UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(GetWorld());
}

void ABMEnemyAIController::MoveWithFallback(
    const FVector& Destination,
    const float AcceptanceRadius,
    const float DesiredSpeed)
{
    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn)
    {
        return;
    }

    // Do not retarget the capsule on every 120 ms decision. A moving player
    // changes the raw target location every frame; feeding each small change
    // straight into AddMovementInput made the desired velocity wobble and
    // caused the visible foot jitter. Combat pursuit needs a shorter, bounded
    // hysteresis than patrol/cover routes, otherwise the enemy follows a stale
    // point and then makes a large corrective turn when the 260 cm threshold
    // is finally crossed. The route heading is still eased in
    // ApplyFallbackMovement, so this update remains continuous.
    const ABMCombatCharacter* CombatTarget = Cast<ABMCombatCharacter>(CurrentTarget.Get());
    const ABMEnemyCharacter* Enemy = Cast<ABMEnemyCharacter>(ControlledPawn);
    const bool bHostileCombatPursuit = CombatTarget
        && Enemy
        && CombatTarget->GetTeam() != Enemy->GetTeam();
    const float DestinationRetargetDistance = bHostileCombatPursuit
        ? 120.0f
        : (AcceptanceRadius <= 60.0f ? 90.0f : 260.0f);
    const bool bMovementActive = bFallbackMovementActive || bNavigationMovementActive;
    const bool bDestinationChanged = !bMovementActive
        || FVector::DistSquared2D(FallbackMoveGoal, Destination)
            > FMath::Square(DestinationRetargetDistance);
    if (bDestinationChanged)
    {
        // Keep the requested goal separate from the route point consumed by
        // CharacterMovement. The raw player position changes in network-sized
        // steps; easing the route point toward it stops those steps becoming
        // left/right velocity pulses while still pursuing the live target.
        if (!bFallbackMovementActive)
        {
            FallbackMoveDestination = Destination;
        }
        FallbackMoveGoal = Destination;
        bFallbackDetourActive = false;
        FallbackDetourDestination = FVector::ZeroVector;
        // MoveToLocation owns path replacement when navigation is available.
        // The direct-input compatibility route below also keeps its current
        // braking state through a retarget so neither path produces a visible
        // one-frame stop/start hitch.
    }

    const FVector CurrentLocation = ControlledPawn->GetActorLocation();
    const FVector Goal = FallbackMoveGoal.IsNearlyZero()
        ? FallbackMoveDestination
        : FallbackMoveGoal;
    FVector Offset = Goal - CurrentLocation;
    Offset.Z = 0.0f;
    if (Offset.SizeSquared2D() <= FMath::Square(AcceptanceRadius))
    {
        ClearFallbackMovement(TEXT("move_acceptance"), false);
        return;
    }

    FallbackMoveAcceptanceRadius = FMath::Max(1.0f, AcceptanceRadius);
    FallbackMoveSpeed = FMath::Max(1.0f, DesiredSpeed);

    const bool bNavigationReady = IsRuntimeNavigationReady();
    const bool bNavigationSystemAvailable = GetWorld()
        && UNavigationSystemV1::GetNavigationSystem(GetWorld()) != nullptr;
    if (bNavigationReady)
    {
        bRuntimeNavigationEverReady = true;
    }
    else if (bRuntimeNavigationEverReady || bNavigationSystemAvailable)
    {
        // A dynamic Recast update can briefly unregister or lock the mesh
        // while runtime cover changes are committed. Preserve the route
        // hand-off and retry the same goal after the mesh returns instead of
        // changing locomotion authority to direct input. Holding also applies
        // before the first valid tile whenever a navigation system exists;
        // direct input is now reserved for genuinely nav-less editor/source
        // sessions, so a transient Recast build can never reintroduce the
        // backward/sliding compatibility route.
        if (bNavigationMovementActive)
        {
            StopMovement();
            bNavigationMovementActive = false;
            NavigationMoveGoal = FVector::ZeroVector;
        }
        bFallbackMovementActive = false;
#if !UE_BUILD_SHIPPING
        if (!bLoggedNavigationWait)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian AI movement: navigation temporarily unavailable; holding route pawn=%s"),
                *GetNameSafe(ControlledPawn));
            bLoggedNavigationWait = true;
        }
#endif
        return;
    }

    if (bNavigationReady)
    {
        if (bNavigationMovementActive
            && GetPathFollowingComponent()
            && GetPathFollowingComponent()->GetStatus() == EPathFollowingStatus::Idle)
        {
            // A failed or completed path can become idle between decision
            // ticks. Reissue it once rather than silently leaving the pawn
            // stationary or switching to a side-stepping input route.
            bNavigationMovementActive = false;
        }

        if (!bNavigationMovementActive || bDestinationChanged)
        {
            FVector NavigationDestination = Destination;
            bool bProjectedDestination = false;
            if (UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetNavigationSystem(GetWorld()))
            {
                FNavLocation ProjectedDestination;
                bProjectedDestination = NavigationSystem->ProjectPointToNavigation(
                    Destination,
                    ProjectedDestination,
                    FVector(1200.0f, 1200.0f, 500.0f));
                if (bProjectedDestination)
                {
                    NavigationDestination = ProjectedDestination.Location;
                }
            }

            if (!bProjectedDestination)
            {
                // The requested target can be an off-mesh actor location or
                // the far side of newly registered cover. Wait for a
                // projectable point rather than switching to direct input
                // around collision.
#if !UE_BUILD_SHIPPING
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian AI movement: navigation destination pending projection pawn=%s destination=%s"),
                    *GetNameSafe(ControlledPawn),
                    *Destination.ToString());
#endif
                return;
            }

            const EPathFollowingRequestResult::Type RequestResult = MoveToLocation(
                NavigationDestination,
                FallbackMoveAcceptanceRadius,
                true,
                true,
                true,
                false,
                nullptr,
                true);
            if (RequestResult == EPathFollowingRequestResult::RequestSuccessful)
            {
                bNavigationMovementActive = true;
                bFallbackMovementActive = false;
                NavigationMoveGoal = NavigationDestination;
                if (ACharacter* Character = Cast<ACharacter>(ControlledPawn))
                {
                    if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
                    {
                        Movement->MaxWalkSpeed = FMath::Max(1.0f, DesiredSpeed);
                    }
                }
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian AI movement: navigation path active pawn=%s destination=%s acceptance=%.1f speed=%.1f"),
                    *GetNameSafe(ControlledPawn),
                    *NavigationDestination.ToString(),
                    FallbackMoveAcceptanceRadius,
                    FallbackMoveSpeed);
                return;
            }
            if (RequestResult == EPathFollowingRequestResult::AlreadyAtGoal)
            {
                ClearFallbackMovement(TEXT("navigation_already_at_goal"), false);
                return;
            }

            bNavigationMovementActive = false;
            NavigationMoveGoal = FVector::ZeroVector;
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Black Meridian AI movement: navigation request failed; retrying projected route pawn=%s destination=%s"),
                *GetNameSafe(ControlledPawn),
                *NavigationDestination.ToString());
            return;
        }
        else
        {
            // The path follower is already driving CharacterMovement. Do not
            // submit AddMovementInput as a second movement authority.
            return;
        }
    }
    else if (bNavigationMovementActive)
    {
        // Navigation can be invalidated while a generated layout is being
        // refreshed. Abort the stale route before recovering through the
        // direct CharacterMovement path.
        StopMovement();
        bNavigationMovementActive = false;
        NavigationMoveGoal = FVector::ZeroVector;
    }

    bFallbackMovementActive = true;

#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe"))
        && (bDestinationChanged || !bLoggedDirectMovementFallback))
    {
        const ACharacter* Character = Cast<ACharacter>(ControlledPawn);
        const UCharacterMovementComponent* Movement = Character
            ? Character->GetCharacterMovement()
            : nullptr;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian AI movement request: pawn=%s current=%s destination=%s distance=%.1f acceptance=%.1f desired_speed=%.1f max_speed=%.1f max_accel=%.1f mode=%d destination_changed=%s"),
            *GetNameSafe(ControlledPawn),
            *ControlledPawn->GetActorLocation().ToString(),
            *Destination.ToString(),
            FVector::Dist2D(ControlledPawn->GetActorLocation(), Destination),
            FallbackMoveAcceptanceRadius,
            FallbackMoveSpeed,
            Movement ? Movement->MaxWalkSpeed : 0.0f,
            Movement ? Movement->GetMaxAcceleration() : 0.0f,
            Movement ? static_cast<int32>(Movement->MovementMode) : -1,
            bDestinationChanged ? TEXT("yes") : TEXT("no"));
    }
#endif

#if !UE_BUILD_SHIPPING
    if (!bLoggedDirectMovementFallback)
    {
        const FString PawnName = ControlledPawn->GetName();
        const FString DestinationString = Destination.ToString();
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian AI movement: direct fallback active pawn=%s destination=%s navigation_ready=%s"),
            *PawnName,
            *DestinationString,
            bNavigationReady ? TEXT("yes_request_failed") : TEXT("no"));
        bLoggedDirectMovementFallback = true;
    }
#endif

}

void ABMEnemyAIController::ApplyFallbackMovement(const float DeltaSeconds)
{
    if (!bFallbackMovementActive)
    {
        return;
    }

    APawn* ControlledPawn = GetPawn();
    ABMEnemyCharacter* Enemy = Cast<ABMEnemyCharacter>(ControlledPawn);
    if (!ControlledPawn || !Enemy || Enemy->IsEliminated())
    {
        ClearFallbackMovement(TEXT("invalid_or_eliminated"));
        return;
    }

    // The actor's post-physics tick owns final body facing. The controller
    // only supplies locomotion input here, before CharacterMovement resolves
    // acceleration and collision.
    static_cast<void>(DeltaSeconds);

    const FVector CurrentLocation = ControlledPawn->GetActorLocation();
    const FVector Goal = FallbackMoveGoal.IsNearlyZero()
        ? FallbackMoveDestination
        : FallbackMoveGoal;
    FVector GoalOffset = Goal - CurrentLocation;
    GoalOffset.Z = 0.0f;
    if (GoalOffset.SizeSquared2D() <= FMath::Square(FallbackMoveAcceptanceRadius))
    {
        ClearFallbackMovement(TEXT("apply_goal_acceptance"), false);
        DecisionTimer = 0.0f;
        return;
    }
    if (DeltaSeconds > KINDA_SMALL_NUMBER && !FallbackMoveGoal.IsNearlyZero())
    {
        // A constant-rate filter gives a bounded, deterministic pursuit lag
        // without the asymptotic tail of VInterpTo. It also avoids changing
        // the route heading simply because the target sent a new position
        // update this frame.
        FallbackMoveDestination = FMath::VInterpConstantTo(
            FallbackMoveDestination,
            FallbackMoveGoal,
            DeltaSeconds,
            900.0f);
    }
    FVector Offset = FallbackMoveDestination - CurrentLocation;
    Offset.Z = 0.0f;
    if (Offset.SizeSquared2D() <= FMath::Square(FallbackMoveAcceptanceRadius))
    {
        ClearFallbackMovement(TEXT("apply_acceptance"), false);
        // Re-evaluate the tactical state in this same controller tick. The
        // old 120 ms decision cadence left a visible brake/no-input gap at
        // every short combat waypoint even though the next point was already
        // knowable from the current target.
        DecisionTimer = 0.0f;
        return;
    }

    FVector Direction = Offset.GetSafeNormal2D();
    if (Direction.IsNearlyZero())
    {
        ClearFallbackMovement(TEXT("zero_direction"));
        return;
    }

    // The generated showcase deliberately remains usable without a baked
    // NavMesh. A straight AddMovementInput route is enough in open space, but
    // it can pin a capsule against the first collision-bearing cover piece
    // when the tactical destination is on the far side of that piece.
    //
    // Combat enemies use a forward-only rifle locomotion set. A lateral route
    // while the body and weapon face the player looks like sliding and was the
    // source of the reported jitter. Keep combat routes inside the target-
    // facing cone; if cover would require a wider strafe, hold at the cover
    // until a readable forward route is available. Patrol/investigation still
    // use the wider detour behavior because they are not presenting a target.
    const ABMCombatCharacter* LiveCombatTarget = Cast<ABMCombatCharacter>(CurrentTarget.Get());
    const bool bHasHostileCombatTarget = LiveCombatTarget
        && LiveCombatTarget->GetTeam() != Enemy->GetTeam();
    FVector CombatTargetDirection = FVector::ZeroVector;
    if (bHasHostileCombatTarget)
    {
        CombatTargetDirection = (
            LiveCombatTarget->GetActorLocation() - CurrentLocation).GetSafeNormal2D();
    }
    const auto IsReadableCombatMovementDirection = [bHasHostileCombatTarget, &CombatTargetDirection](
        const FVector& CandidateDirection)
    {
        if (!bHasHostileCombatTarget || CandidateDirection.IsNearlyZero() || CombatTargetDirection.IsNearlyZero())
        {
            return true;
        }

        const float ForwardDot = FVector::DotProduct(
            CandidateDirection.GetSafeNormal2D(),
            CombatTargetDirection);
        const float TargetFacingDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(
            CombatTargetDirection.Rotation().Yaw,
            CandidateDirection.Rotation().Yaw));
        return ForwardDot >= BMCombatTuning::EnemyCombatMovementMinimumForwardDot
            && TargetFacingDelta <= BMCombatTuning::EnemyMovementFacingToleranceDegrees;
    };

    bool bHoldingForUnreadableCombatDetour = false;
    const float DetourAcceptanceRadius = 55.0f;
    if (bFallbackDetourActive)
    {
        FVector DetourOffset = FallbackDetourDestination - CurrentLocation;
        DetourOffset.Z = 0.0f;
        if (DetourOffset.SizeSquared2D() <= FMath::Square(DetourAcceptanceRadius))
        {
            bFallbackDetourActive = false;
            FallbackDetourDestination = FVector::ZeroVector;
        }
        else
        {
            Direction = DetourOffset.GetSafeNormal2D();
        }
    }

    // A previously selected detour can become stale as the player moves. Do
    // not keep following a route that has already left the body-facing cone;
    // re-evaluate it from the live target direction instead. This check must
    // happen after the detour direction is loaded above, otherwise the test
    // would validate the old direct-to-goal heading instead of the visible
    // route the capsule is about to take.
    if (bFallbackDetourActive && !IsReadableCombatMovementDirection(Direction))
    {
        bFallbackDetourActive = false;
        FallbackDetourDestination = FVector::ZeroVector;
        Direction = CombatTargetDirection;
    }
    if (!bFallbackDetourActive && !IsReadableCombatMovementDirection(Direction))
    {
        Direction = CombatTargetDirection;
    }

    if (!bFallbackDetourActive && GetWorld())
    {
        const FVector SweepStart = CurrentLocation + FVector(0.0f, 0.0f, 20.0f);
        const FVector SweepEnd = SweepStart + Direction * 260.0f;
        const FCollisionShape SweepShape = FCollisionShape::MakeSphere(42.0f);
        const auto IsStaticPathClear = [this, ControlledPawn, &SweepShape](
            const FVector& Start,
            const FVector& End)
        {
            FHitResult Hit;
            FCollisionQueryParams QueryParams(
                SCENE_QUERY_STAT(BMEnemyFallbackDetour),
                false,
                ControlledPawn);
            QueryParams.AddIgnoredActor(ControlledPawn);
            return !GetWorld()->SweepSingleByObjectType(
                Hit,
                Start,
                End,
                FQuat::Identity,
                FCollisionObjectQueryParams(ECC_WorldStatic),
                SweepShape,
                QueryParams);
        };

        FHitResult BlockingHit;
        FCollisionQueryParams QueryParams(
            SCENE_QUERY_STAT(BMEnemyFallbackObstacleProbe),
            false,
            ControlledPawn);
        QueryParams.AddIgnoredActor(ControlledPawn);
        const bool bBlockedAhead = GetWorld()->SweepSingleByObjectType(
            BlockingHit,
            SweepStart,
            SweepEnd,
            FQuat::Identity,
            FCollisionObjectQueryParams(ECC_WorldStatic),
            SweepShape,
            QueryParams);
        if (bBlockedAhead)
        {
            const FVector Lateral(-Direction.Y, Direction.X, 0.0f);
            constexpr float DetourLateralDistance = 340.0f;
            constexpr float DetourForwardDistance = 180.0f;
            bool bFoundDetour = false;
            int32 SelectedSide = 0;
            FVector SelectedWaypoint = FVector::ZeroVector;

            for (const int32 Side : {1, -1})
            {
                const FVector Waypoint = CurrentLocation
                    + Lateral * (DetourLateralDistance * static_cast<float>(Side))
                    + Direction * DetourForwardDistance;
                const FVector FlatWaypoint(Waypoint.X, Waypoint.Y, CurrentLocation.Z);
                const FVector CandidateDirection = (FlatWaypoint - CurrentLocation).GetSafeNormal2D();
                if (!IsReadableCombatMovementDirection(CandidateDirection))
                {
                    continue;
                }
                const bool bFirstLegClear = IsStaticPathClear(CurrentLocation, FlatWaypoint);
                const bool bSecondLegClear = bFirstLegClear
                    && IsStaticPathClear(FlatWaypoint, FallbackMoveDestination);
                if (bSecondLegClear || (!bFoundDetour && bFirstLegClear))
                {
                    bFoundDetour = true;
                    SelectedSide = Side;
                    SelectedWaypoint = FlatWaypoint;
                    if (bSecondLegClear)
                    {
                        break;
                    }
                }
            }

            if (bFoundDetour)
            {
                FallbackDetourDestination = SelectedWaypoint;
                bFallbackDetourActive = true;
                Direction = (FallbackDetourDestination - CurrentLocation).GetSafeNormal2D();
#if !UE_BUILD_SHIPPING
                if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe")))
                {
                    UE_LOG(
                        LogTemp,
                        Display,
                        TEXT("Black Meridian AI movement: detour pawn=%s obstacle=%s waypoint=%s side=%d"),
                        *GetNameSafe(ControlledPawn),
                        *GetNameSafe(BlockingHit.GetActor()),
                        *FallbackDetourDestination.ToString(),
                        SelectedSide);
                }
#endif
            }
            else if (bHasHostileCombatTarget)
            {
                // There is no forward-facing detour around this obstacle.
                // Suppress this frame's input so CharacterMovement brakes
                // cleanly instead of sliding around the corner sideways.
                bHoldingForUnreadableCombatDetour = true;
#if !UE_BUILD_SHIPPING
                if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe")))
                {
                    UE_LOG(
                        LogTemp,
                        Display,
                        TEXT("Black Meridian AI movement: combat route held at obstacle pawn=%s target=%s route_direction=%s target_direction=%s"),
                        *GetNameSafe(ControlledPawn),
                        *GetNameSafe(LiveCombatTarget),
                        *Direction.ToString(),
                        *CombatTargetDirection.ToString());
                }
#endif
            }
        }
    }

    if (bHoldingForUnreadableCombatDetour)
    {
        // Do not clear the active route: the next decision can retry the same
        // obstacle with a new target position. With no new input, the
        // CharacterMovement braking settings provide a smooth stop.
        return;
    }

    if (ACharacter* Character = Cast<ACharacter>(ControlledPawn))
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            // Let CharacterMovement steer through a route hand-off instead of
            // hard-zeroing velocity. Smooth the speed cap as well as the
            // direction; the closing-band calculation changes at decision
            // rate and an immediate cap change reads as a foot-speed hitch.
            Movement->MaxWalkSpeed = FMath::FInterpTo(
                Movement->MaxWalkSpeed,
                FallbackMoveSpeed,
                FMath::Max(DeltaSeconds, 0.0f),
                8.0f);
        }
    }

    // CharacterMovement consumes this input on its next pre-physics update.
    // Smooth the route heading as an angle, rather than interpolating two
    // vectors and normalizing the result. Vector interpolation crosses zero
    // when a maneuver reverses direction; normalizing that near-zero vector
    // can produce a one-frame heading flip, which is the visible foot/capsule
    // jitter reported during combat strafes.
    const FVector DesiredDirection = Direction;
    if (DeltaSeconds > KINDA_SMALL_NUMBER && !FallbackMoveDirection.IsNearlyZero())
    {
        const float CurrentHeading = FallbackMoveDirection.Rotation().Yaw;
        const float DesiredHeading = DesiredDirection.Rotation().Yaw;
        const float SmoothedHeading = FMath::FixedTurn(
            CurrentHeading,
            DesiredHeading,
            BMCombatTuning::EnemyMovementSteeringRateDegreesPerSecond * DeltaSeconds);
        FallbackMoveDirection = FRotator(0.0f, SmoothedHeading, 0.0f).Vector().GetSafeNormal2D();
    }
    else
    {
        FallbackMoveDirection = DesiredDirection;
    }
    Direction = FallbackMoveDirection.IsNearlyZero() ? DesiredDirection : FallbackMoveDirection;
    ControlledPawn->AddMovementInput(Direction, 1.0f);

#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe")))
    {
        MovementDiagnosticLogTimer -= FMath::Max(DeltaSeconds, 0.0f);
        if (MovementDiagnosticLogTimer <= 0.0f)
        {
            MovementDiagnosticLogTimer = 0.5f;
            const ACharacter* Character = Cast<ACharacter>(ControlledPawn);
            const UCharacterMovementComponent* Movement = Character
                ? Character->GetCharacterMovement()
                : nullptr;
            const FVector PendingInput = ControlledPawn->GetPendingMovementInputVector();
            const FVector LastInput = ControlledPawn->GetLastMovementInputVector();
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian AI movement apply: pawn=%s current=%s destination=%s distance=%.1f direction=%s desired_speed=%.1f velocity=%s current_accel=%s pending_input=%s last_input=%s max_speed=%.1f max_accel=%.1f mode=%d walking=%s floor_walkable=%s detour=%s"),
                *GetNameSafe(ControlledPawn),
                *ControlledPawn->GetActorLocation().ToString(),
                *FallbackMoveDestination.ToString(),
                FVector::Dist2D(ControlledPawn->GetActorLocation(), FallbackMoveDestination),
                *Direction.ToString(),
                FallbackMoveSpeed,
                *ControlledPawn->GetVelocity().ToString(),
                Movement ? *Movement->GetCurrentAcceleration().ToString() : TEXT("None"),
                *PendingInput.ToString(),
                *LastInput.ToString(),
                Movement ? Movement->MaxWalkSpeed : 0.0f,
                Movement ? Movement->GetMaxAcceleration() : 0.0f,
                Movement ? static_cast<int32>(Movement->MovementMode) : -1,
                Movement && Movement->IsMovingOnGround() ? TEXT("yes") : TEXT("no"),
                Movement && Movement->CurrentFloor.IsWalkableFloor() ? TEXT("yes") : TEXT("no"),
                bFallbackDetourActive ? TEXT("yes") : TEXT("no"));
        }
    }
#endif

    // Body rotation is finalized by ABMEnemyCharacter::UpdateMovementFacing
    // in the post-physics actor tick, after collision and acceleration have
    // produced the velocity that the player actually sees. Combat movement
    // has already been constrained to the same forward-facing cone as the
    // visible body, so the forward-only rifle walk cannot present a sideways
    // slide while aiming at the player.
}

void ABMEnemyAIController::ClearFallbackMovement(const TCHAR* Reason, const bool bStopImmediately)
{
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe"))
        && (bFallbackMovementActive || bNavigationMovementActive))
    {
        const APawn* ControlledPawn = GetPawn();
        const ACharacter* Character = Cast<ACharacter>(ControlledPawn);
        const UCharacterMovementComponent* Movement = Character
            ? Character->GetCharacterMovement()
            : nullptr;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian AI movement clear: pawn=%s reason=%s current=%s destination=%s distance=%.1f velocity=%s max_speed=%.1f mode=%d"),
            *GetNameSafe(ControlledPawn),
            Reason ? Reason : TEXT("unspecified"),
            ControlledPawn ? *ControlledPawn->GetActorLocation().ToString() : TEXT("None"),
            *FallbackMoveDestination.ToString(),
            ControlledPawn ? FVector::Dist2D(ControlledPawn->GetActorLocation(), FallbackMoveDestination) : 0.0f,
            ControlledPawn ? *ControlledPawn->GetVelocity().ToString() : TEXT("None"),
            Movement ? Movement->MaxWalkSpeed : 0.0f,
            Movement ? static_cast<int32>(Movement->MovementMode) : -1);
    }
#endif
    if (bNavigationMovementActive)
    {
        StopMovement();
    }
    if (APawn* ControlledPawn = GetPawn())
    {
        // A normal waypoint hand-off deliberately keeps the already supplied
        // input pulse until CharacterMovement consumes it.  Consuming it here
        // created a one-frame zero-input/braking hitch before the next
        // decision selected the following route.  Emergency clears still
        // remove the pulse before stopping immediately below.
        if (bStopImmediately)
        {
            ControlledPawn->ConsumeMovementInputVector();
        }
        if (ACharacter* Character = Cast<ACharacter>(ControlledPawn))
        {
            if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
            {
                if (bStopImmediately)
                {
                    // Emergency states such as suppression/elimination need
                    // an immediate halt. A normal waypoint hand-off passes
                    // false and keeps CharacterMovement's braking, so the
                    // feet do not snap from walk to zero between destinations.
                    Movement->StopMovementImmediately();
                }
            }
        }
    }
    bFallbackMovementActive = false;
    bNavigationMovementActive = false;
    NavigationMoveGoal = FVector::ZeroVector;
    FallbackMoveDestination = FVector::ZeroVector;
    FallbackMoveGoal = FVector::ZeroVector;
    // Keep the last route heading through an ordinary waypoint hand-off so a
    // new tactical destination is steered into instead of snapping from zero
    // on the first frame. Emergency clears still reset it below.
    if (bStopImmediately)
    {
        FallbackMoveDirection = FVector::ZeroVector;
    }
    FallbackDetourDestination = FVector::ZeroVector;
    FallbackMoveAcceptanceRadius = 0.0f;
    FallbackMoveSpeed = 0.0f;
    bFallbackDetourActive = false;
}

TWeakObjectPtr<AActor> ABMEnemyAIController::FindNearestLatticeTarget() const
{
    if (!GetWorld() || !GetPawn())
    {
        return nullptr;
    }

    TWeakObjectPtr<AActor> NearestTarget;
    float NearestDistanceSquared = FMath::Square(2600.0f);
    for (TActorIterator<ABMPlayerCharacter> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        ABMPlayerCharacter* Candidate = *Iterator;
        if (!IsValid(Candidate) || Candidate->IsEliminated() || Candidate->GetTeam() != EBMTeam::Lattice)
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared2D(GetPawn()->GetActorLocation(), Candidate->GetActorLocation());
        if (DistanceSquared < NearestDistanceSquared)
        {
            NearestDistanceSquared = DistanceSquared;
            NearestTarget = Candidate;
        }
    }
    return NearestTarget;
}

FVector ABMEnemyAIController::FindCoverPoint(const FVector& ThreatLocation) const
{
    if (!GetWorld() || !GetPawn())
    {
        return FVector::ZeroVector;
    }

    FVector BestPoint = FVector::ZeroVector;
    float BestDistance = MAX_FLT;
    for (TActorIterator<AStaticMeshActor> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        AStaticMeshActor* Cover = *Iterator;
        if (!Cover || !Cover->ActorHasTag(TEXT("ThinCover")))
        {
            continue;
        }
        const FVector Candidate = Cover->GetActorLocation() + (Cover->GetActorLocation() - ThreatLocation).GetSafeNormal2D() * 120.0f;
        const float Distance = FVector::DistSquared(GetPawn()->GetActorLocation(), Candidate);
        if (Distance < BestDistance && Distance > FMath::Square(120.0f))
        {
            BestPoint = Candidate;
            BestDistance = Distance;
        }
    }
    return BestPoint;
}
