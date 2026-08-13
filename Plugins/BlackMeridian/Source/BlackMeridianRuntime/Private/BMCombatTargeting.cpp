// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMCombatTargeting.h"

#include "BMCombatCharacter.h"
#include "BMCombatTuning.h"
#include "BMHealthComponent.h"
#include "BMInteractableActor.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PhysicsEngine/BodySetup.h"

namespace BMCombatTargeting
{
    namespace
    {
        bool IsNonBlockingCombatMarker(const AActor* Candidate)
        {
            const ABMInteractableActor* Interaction = Cast<ABMInteractableActor>(Candidate);
            if (!Interaction)
            {
                return false;
            }

            // Mission markers, pickups, consoles, revive prompts, and other
            // interaction proxies need Visibility collision so the E trace
            // remains precise, but they are not battlefield cover. Letting a
            // marker win the combat trace makes a clearly visible enemy turn
            // the reticle teal and rejects the shot. Doors remain real sight
            // blockers because they explicitly opt into physical behavior.
            return !Interaction->bActsAsDoor
                && Interaction->InteractionAction != EBMInteractionAction::OpenDoor
                && Interaction->InteractionAction != EBMInteractionAction::BreachDoor;
        }

        bool IsWithinHorizontalAimTolerance(
            const ABMCombatCharacter* Target,
            const FVector& Origin,
            const FVector& Direction,
            const float MaxAngleDegrees)
        {
            if (!Target || !Target->GetCombatAimHitbox())
            {
                return false;
            }

            const FVector FlatDirection = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
            const FVector FlatToTarget = FVector(
                Target->GetCombatAimHitbox()->GetComponentLocation().X - Origin.X,
                Target->GetCombatAimHitbox()->GetComponentLocation().Y - Origin.Y,
                0.0f);
            if (FlatDirection.IsNearlyZero() || FlatToTarget.IsNearlyZero())
            {
                return true;
            }

            const float ForwardDistance = FVector::DotProduct(FlatDirection, FlatToTarget);
            if (ForwardDistance <= KINDA_SMALL_NUMBER)
            {
                return false;
            }

            const FVector LateralOffset = FlatToTarget - FlatDirection * ForwardDistance;
            const float HitboxRadius = Target->GetCombatAimHitbox()->GetScaledCapsuleRadius();
            const float GeometricLateralLimit = HitboxRadius + BMCombatTuning::CombatAimSweepRadius;
            const float AngularLateralLimit = ForwardDistance * FMath::Tan(
                FMath::DegreesToRadians(MaxAngleDegrees));
            const float MaxLateralOffset = FMath::Min(
                GeometricLateralLimit,
                FMath::Max(0.0f, AngularLateralLimit));
            return LateralOffset.SizeSquared() <= FMath::Square(MaxLateralOffset);
        }

#if !UE_BUILD_SHIPPING
        void LogResolverMiss(
            const FHitResult* BlockingHit,
            const TCHAR* Reason,
            const ABMCombatCharacter* CandidateTarget,
            const ABMCombatCharacter* Viewer,
            const FVector& Origin,
            const FVector& Direction,
            const bool bExactTraceHit,
            const bool bUsedSweptTrace)
        {
            if (!FParse::Param(FCommandLine::Get(), TEXT("BM_AimDebug")))
            {
                return;
            }

            const ABMCombatCharacter* ObservedTarget = CandidateTarget;
            if (!ObservedTarget && BlockingHit)
            {
                ObservedTarget = Cast<ABMCombatCharacter>(BlockingHit->GetActor());
            }
            const UCapsuleComponent* AimHitbox = ObservedTarget
                ? ObservedTarget->GetCombatAimHitbox()
                : nullptr;
            const FVector FlatDirection = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
            const FVector FlatToTarget = AimHitbox
                ? FVector(
                    AimHitbox->GetComponentLocation().X - Origin.X,
                    AimHitbox->GetComponentLocation().Y - Origin.Y,
                    0.0f).GetSafeNormal()
                : FVector::ZeroVector;
            const float HorizontalAngleDegrees = (!FlatDirection.IsNearlyZero() && !FlatToTarget.IsNearlyZero())
                ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
                    FVector::DotProduct(FlatDirection, FlatToTarget),
                    -1.0f,
                    1.0f)))
                : -1.0f;
            const FString TraceMode = bUsedSweptTrace
                ? TEXT("swept")
                : (bExactTraceHit ? TEXT("exact") : TEXT("none"));

            // Interaction meshes are intentionally present on the Visibility
            // channel so the normal E trace can select a small face.  When one
            // of them is allowed to reach this miss logger, capture the
            // complete runtime classification once per actor.  This keeps a
            // rendered marker/door regression distinguishable from real cover
            // without filling the packaged log once per frame.
            if (BlockingHit)
            {
                if (const ABMInteractableActor* Interaction = Cast<ABMInteractableActor>(BlockingHit->GetActor()))
                {
                    static TWeakObjectPtr<AActor> LastLoggedInteraction;
                    if (LastLoggedInteraction.Get() != Interaction)
                    {
                        LastLoggedInteraction = const_cast<ABMInteractableActor*>(Interaction);
                        const UStaticMeshComponent* InteractionMesh =
                            Interaction->FindComponentByClass<UStaticMeshComponent>();
                        FString TagList;
                        for (const FName& Tag : Interaction->Tags)
                        {
                            if (!TagList.IsEmpty())
                            {
                                TagList += TEXT(",");
                            }
                            TagList += Tag.ToString();
                        }
                        UE_LOG(
                            LogTemp,
                            Display,
                            TEXT("Black Meridian aim blocker metadata actor=%s interaction_id=%s action=%d acts_as_door=%s location=%s rotation=%s scale=%s tags=%s collision=%d visibility_response=%d mesh=%s"),
                            *GetNameSafe(Interaction),
                            *Interaction->InteractionId.ToString(),
                            static_cast<int32>(Interaction->InteractionAction),
                            Interaction->bActsAsDoor ? TEXT("yes") : TEXT("no"),
                            *Interaction->GetActorLocation().ToString(),
                            *Interaction->GetActorRotation().ToString(),
                            *Interaction->GetActorScale3D().ToString(),
                            TagList.IsEmpty() ? TEXT("None") : *TagList,
                            InteractionMesh ? static_cast<int32>(InteractionMesh->GetCollisionEnabled()) : -1,
                            InteractionMesh ? static_cast<int32>(InteractionMesh->GetCollisionResponseToChannel(ECC_Visibility)) : -1,
                            InteractionMesh && InteractionMesh->GetStaticMesh()
                                ? *InteractionMesh->GetStaticMesh()->GetPathName()
                                : TEXT("None"));
                    }
                }

                // Runtime campaign geometry is intentionally anonymous in a
                // cooked build (StaticMeshActor_214748xxxx).  A miss report
                // without its physical classification cannot tell an actual
                // wall/cover from a decorative mesh that accidentally blocks
                // Visibility.  Log each blocker actor once while the debug
                // flag is enabled so a rendered repro can be fixed at the
                // source layout rather than by weakening the target gate.
                if (AActor* BlockingActor = BlockingHit->GetActor())
                {
                    static TWeakObjectPtr<AActor> LastLoggedStaticBlocker;
                    if (LastLoggedStaticBlocker.Get() != BlockingActor
                        && !Cast<ABMInteractableActor>(BlockingActor))
                    {
                        LastLoggedStaticBlocker = BlockingActor;
                        const UPrimitiveComponent* BlockingComponent = BlockingHit->GetComponent();
                        const UStaticMeshComponent* BlockingMesh = Cast<UStaticMeshComponent>(BlockingComponent);
                        const UStaticMesh* StaticMesh = BlockingMesh ? BlockingMesh->GetStaticMesh() : nullptr;
                        const UBodySetup* BodySetup = StaticMesh ? StaticMesh->GetBodySetup() : nullptr;
                        FString ActorTags;
                        for (const FName& Tag : BlockingActor->Tags)
                        {
                            if (!ActorTags.IsEmpty())
                            {
                                ActorTags += TEXT(",");
                            }
                            ActorTags += Tag.ToString();
                        }
                        FString ComponentTags;
                        if (BlockingComponent)
                        {
                            for (const FName& Tag : BlockingComponent->ComponentTags)
                            {
                                if (!ComponentTags.IsEmpty())
                                {
                                    ComponentTags += TEXT(",");
                                }
                                ComponentTags += Tag.ToString();
                            }
                        }
                        const FBoxSphereBounds Bounds = BlockingComponent
                            ? BlockingComponent->Bounds
                            : FBoxSphereBounds(BlockingActor->GetActorLocation(), FVector::ZeroVector, 0.0f);
                        UE_LOG(
                            LogTemp,
                            Display,
                            TEXT("Black Meridian aim blocker metadata actor=%s class=%s component=%s mesh=%s mesh_trace=%d actor_collision=%s component_collision=%d object_type=%d visibility_response=%d pawn_response=%d profile=%s actor_location=%s actor_rotation=%s actor_scale=%s bounds_origin=%s bounds_extent=%s actor_tags=%s component_tags=%s impact=%s distance=%.2f"),
                            *GetNameSafe(BlockingActor),
                            *GetNameSafe(BlockingActor->GetClass()),
                            *GetNameSafe(BlockingComponent),
                            StaticMesh ? *StaticMesh->GetPathName() : TEXT("None"),
                            BodySetup ? static_cast<int32>(BodySetup->CollisionTraceFlag) : -1,
                            BlockingActor->GetActorEnableCollision() ? TEXT("yes") : TEXT("no"),
                            BlockingComponent ? static_cast<int32>(BlockingComponent->GetCollisionEnabled()) : -1,
                            BlockingComponent ? static_cast<int32>(BlockingComponent->GetCollisionObjectType()) : -1,
                            BlockingComponent ? static_cast<int32>(BlockingComponent->GetCollisionResponseToChannel(ECC_Visibility)) : -1,
                            BlockingComponent ? static_cast<int32>(BlockingComponent->GetCollisionResponseToChannel(ECC_Pawn)) : -1,
                            BlockingComponent ? *BlockingComponent->GetCollisionProfileName().ToString() : TEXT("None"),
                            *BlockingActor->GetActorLocation().ToString(),
                            *BlockingActor->GetActorRotation().ToString(),
                            *BlockingActor->GetActorScale3D().ToString(),
                            *Bounds.Origin.ToString(),
                            *Bounds.BoxExtent.ToString(),
                            ActorTags.IsEmpty() ? TEXT("None") : *ActorTags,
                            ComponentTags.IsEmpty() ? TEXT("None") : *ComponentTags,
                            *BlockingHit->ImpactPoint.ToString(),
                            BlockingHit->Distance);
                    }
                }
            }

            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian aim resolver: result=miss reason=%s trace_mode=%s exact_trace_hit=%s viewer=%s blocking_actor=%s blocking_component=%s candidate=%s origin=%s direction=%s actor_location=%s hitbox_location=%s target_velocity=%s horizontal_angle_deg=%.3f hitbox_radius=%.2f hitbox_half_height=%.2f hitbox_collision=%d hitbox_registered=%s hitbox_physics=%s"),
                Reason,
                *TraceMode,
                bExactTraceHit ? TEXT("yes") : TEXT("no"),
                *GetNameSafe(Viewer),
                BlockingHit ? *GetNameSafe(BlockingHit->GetActor()) : TEXT("None"),
                BlockingHit ? *GetNameSafe(BlockingHit->GetComponent()) : TEXT("None"),
                *GetNameSafe(CandidateTarget),
                *Origin.ToString(),
                *Direction.ToString(),
                ObservedTarget ? *ObservedTarget->GetActorLocation().ToString() : TEXT("None"),
                AimHitbox ? *AimHitbox->GetComponentLocation().ToString() : TEXT("None"),
                ObservedTarget ? *ObservedTarget->GetVelocity().ToString() : TEXT("None"),
                HorizontalAngleDegrees,
                AimHitbox ? AimHitbox->GetScaledCapsuleRadius() : 0.0f,
                AimHitbox ? AimHitbox->GetScaledCapsuleHalfHeight() : 0.0f,
                AimHitbox ? static_cast<int32>(AimHitbox->GetCollisionEnabled()) : -1,
                AimHitbox && AimHitbox->IsRegistered() ? TEXT("yes") : TEXT("no"),
                AimHitbox && AimHitbox->IsPhysicsStateCreated() ? TEXT("yes") : TEXT("no"));
        }

        void LogResolverHit(
            const FHitResult& Hit,
            const ABMCombatCharacter* Target,
            const ABMCombatCharacter* Viewer,
            const FVector& Origin,
            const FVector& Direction,
            const bool bExactTraceHit,
            const bool bUsedSweptTrace)
        {
            if (!FParse::Param(FCommandLine::Get(), TEXT("BM_AimDebug")))
            {
                return;
            }

            const UCapsuleComponent* AimHitbox = Target ? Target->GetCombatAimHitbox() : nullptr;
            const FVector FlatDirection = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
            const FVector FlatToTarget = AimHitbox
                ? FVector(
                    AimHitbox->GetComponentLocation().X - Origin.X,
                    AimHitbox->GetComponentLocation().Y - Origin.Y,
                    0.0f).GetSafeNormal()
                : FVector::ZeroVector;
            const float HorizontalAngleDegrees = (!FlatDirection.IsNearlyZero() && !FlatToTarget.IsNearlyZero())
                ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
                    FVector::DotProduct(FlatDirection, FlatToTarget),
                    -1.0f,
                    1.0f)))
                : -1.0f;
            const FString TraceMode = bUsedSweptTrace
                ? TEXT("swept")
                : (bExactTraceHit ? TEXT("exact") : TEXT("none"));

            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian aim resolver: result=hit target=%s component=%s impact=%s trace_mode=%s exact_trace_hit=%s viewer=%s origin=%s direction=%s actor_location=%s hitbox_location=%s target_velocity=%s horizontal_angle_deg=%.3f hitbox_radius=%.2f hitbox_half_height=%.2f hitbox_collision=%d hitbox_registered=%s hitbox_physics=%s"),
                *GetNameSafe(Target),
                *GetNameSafe(Hit.GetComponent()),
                *Hit.ImpactPoint.ToString(),
                *TraceMode,
                bExactTraceHit ? TEXT("yes") : TEXT("no"),
                *GetNameSafe(Viewer),
                *Origin.ToString(),
                *Direction.ToString(),
                Target ? *Target->GetActorLocation().ToString() : TEXT("None"),
                AimHitbox ? *AimHitbox->GetComponentLocation().ToString() : TEXT("None"),
                Target ? *Target->GetVelocity().ToString() : TEXT("None"),
                HorizontalAngleDegrees,
                AimHitbox ? AimHitbox->GetScaledCapsuleRadius() : 0.0f,
                AimHitbox ? AimHitbox->GetScaledCapsuleHalfHeight() : 0.0f,
                AimHitbox ? static_cast<int32>(AimHitbox->GetCollisionEnabled()) : -1,
                AimHitbox && AimHitbox->IsRegistered() ? TEXT("yes") : TEXT("no"),
                AimHitbox && AimHitbox->IsPhysicsStateCreated() ? TEXT("yes") : TEXT("no"));
        }
#else
        void LogResolverMiss(
            const FHitResult*,
            const TCHAR*,
            const ABMCombatCharacter*,
            const ABMCombatCharacter*,
            const FVector&,
            const FVector&,
            bool,
            bool)
        {
        }

        void LogResolverHit(
            const FHitResult&,
            const ABMCombatCharacter*,
            const ABMCombatCharacter*,
            const FVector&,
            const FVector&,
            bool,
            bool)
        {
        }
#endif
    }

    bool IsLiveHostileTarget(const ABMCombatCharacter* Viewer, const AActor* Candidate)
    {
        const ABMCombatCharacter* CandidateCharacter = Cast<ABMCombatCharacter>(Candidate);
        const UBMHealthComponent* TargetHealth = CandidateCharacter ? CandidateCharacter->HealthComponent : nullptr;
        return Viewer
            && CandidateCharacter
            && Viewer->GetTeam() != EBMTeam::Neutral
            && CandidateCharacter->GetTeam() != EBMTeam::Neutral
            && CandidateCharacter->GetTeam() != Viewer->GetTeam()
            && !CandidateCharacter->IsEliminated()
            && TargetHealth
            && TargetHealth->IsAlive();
    }

    bool IsCombatAimHit(const ABMCombatCharacter* Target, const FHitResult& Hit)
    {
        return Target
            && Hit.GetActor() == Target
            && Target->GetCombatAimHitbox()
            && Hit.GetComponent() == Target->GetCombatAimHitbox();
    }

    ABMCombatCharacter* ResolveLiveHostileCrosshairTarget(
        UWorld* World,
        const ABMCombatCharacter* Viewer,
        const FVector& Origin,
        const FVector& Direction,
        const float Range,
        const AActor* AdditionalIgnoredActor,
        FHitResult* OutHit)
    {
        if (OutHit)
        {
            *OutHit = FHitResult();
        }

        const FVector SafeDirection = Direction.GetSafeNormal();
        if (!World || !Viewer || SafeDirection.IsNearlyZero() || Range <= 0.0f)
        {
            return nullptr;
        }

        FCollisionQueryParams QueryParams(
            SCENE_QUERY_STAT(BlackMeridianLiveHostileCrosshair),
            true,
            const_cast<ABMCombatCharacter*>(Viewer));
        // Visibility must follow the rendered mesh's real gaps and recesses.
        // Keep the runtime pawn/navigation proxies on simple collision, but
        // opt this canonical sightline query into the mesh's complex body.
        QueryParams.bTraceComplex = true;
        if (AdditionalIgnoredActor)
        {
            QueryParams.AddIgnoredActor(AdditionalIgnoredActor);
        }

        const FVector TraceEnd = Origin + SafeDirection * FMath::Max(100.0f, Range);
        TArray<FHitResult> ExactHits;
        const bool bExactTraceHit = World->LineTraceMultiByChannel(
            ExactHits,
            Origin,
            TraceEnd,
            ECC_Visibility,
            QueryParams);

        // The mathematical center ray is the cover/visibility contract. The
        // later narrow sweep exists only to absorb a small rendered-pose
        // interpolation error; it must not turn a prop that is merely beside
        // the screen-center ray into an invisible wall. Preserve the exact
        // result separately so the sweep can be treated as candidate
        // acquisition rather than a second, wider obstruction query.
        TArray<FHitResult> Hits = ExactHits;

        // The rendered character can move a few centimetres between the
        // frame that produced the reticle and the collision query. A strict
        // mathematical line then misses the visible torso even though the
        // player has not meaningfully moved the crosshair. It can also touch
        // a same-actor collision component before reaching the dedicated aim
        // surface. Use a very narrow swept ray whenever the exact line did
        // not already reach a dedicated live hostile surface. This also
        // handles a target that is slightly off the exact line while a prop
        // sits farther along that same lane. The sweep remains authoritative
        // for ordering: cover or another non-hostile hit returned before the
        // hostile still remains a hard boundary. The dedicated hitbox is
        // already the precision contract for an exact line hit; the angular
        // guard below is only needed for the interpolation sweep so the
        // sweep cannot become broad aim assist at close range.
        bool bTraceHit = bExactTraceHit;
        bool bUsedSweptTrace = false;
        bool bShouldTrySweptTrace = !bExactTraceHit;
        bool bHasExactNonHostileBlocker = false;
        float ExactNonHostileBlockerDistance = TNumericLimits<float>::Max();
        const ABMCombatCharacter* ExactHostileComponentOwner = nullptr;
        if (bExactTraceHit)
        {
            bShouldTrySweptTrace = true;
            for (const FHitResult& Hit : ExactHits)
            {
                if (!Hit.bBlockingHit)
                {
                    continue;
                }

                if (IsNonBlockingCombatMarker(Hit.GetActor()))
                {
                    continue;
                }

                const ABMCombatCharacter* FirstTarget = Cast<ABMCombatCharacter>(Hit.GetActor());
                bShouldTrySweptTrace = !(IsLiveHostileTarget(Viewer, FirstTarget)
                    && IsCombatAimHit(FirstTarget, Hit));
                if (IsLiveHostileTarget(Viewer, FirstTarget))
                {
                    ExactHostileComponentOwner = FirstTarget;
                }

                // The center-ray result remains the visibility contract, but
                // its distance matters. A far boundary, floor, or landmark
                // after a slightly offset hostile is not cover in front of
                // that hostile and must not suppress the small interpolation
                // sweep. The candidate is compared against this boundary
                // below, so real cover still wins when it is closer.
                if (!FirstTarget || !IsLiveHostileTarget(Viewer, FirstTarget))
                {
                    bHasExactNonHostileBlocker = true;
                    ExactNonHostileBlockerDistance = Hit.Distance > KINDA_SMALL_NUMBER
                        ? Hit.Distance
                        : FVector::Dist(Origin, Hit.ImpactPoint);
                }
                break;
            }
        }

        if (bShouldTrySweptTrace)
        {
            TArray<FHitResult> SweptHits;
            bTraceHit = World->SweepMultiByChannel(
                SweptHits,
                Origin,
                TraceEnd,
                FQuat::Identity,
                ECC_Visibility,
                FCollisionShape::MakeSphere(BMCombatTuning::CombatAimSweepRadius),
                QueryParams);
            if (bTraceHit)
            {
                Hits = MoveTemp(SweptHits);
                bUsedSweptTrace = true;
            }
        }

        if (!bTraceHit)
        {
            LogResolverMiss(
                nullptr,
                TEXT("no_visibility_hit"),
                nullptr,
                Viewer,
                Origin,
                SafeDirection,
                bExactTraceHit,
                bUsedSweptTrace);
            return nullptr;
        }

        ABMCombatCharacter* FirstHostileComponentOwner = nullptr;
        const FHitResult* FirstBlockingHit = nullptr;
        for (const FHitResult& Hit : Hits)
        {
            if (!Hit.bBlockingHit)
            {
                continue;
            }

            if (IsNonBlockingCombatMarker(Hit.GetActor()))
            {
                continue;
            }

            if (!FirstBlockingHit)
            {
                FirstBlockingHit = &Hit;
            }

            ABMCombatCharacter* Target = Cast<ABMCombatCharacter>(Hit.GetActor());
            if (!IsLiveHostileTarget(Viewer, Target))
            {
                // For the exact trace this is the first screen-center
                // blocker, so cover, props, and any other blocking actor
                // remain a hard visibility boundary. For the interpolation
                // sweep, however, this hit is allowed to sit beside the
                // center ray: the exact trace already proved that the center
                // ray is clear, and this wider query is only looking for a
                // small target-pose discrepancy. A different hostile still
                // remains an ordering boundary below.
                if (bUsedSweptTrace)
                {
                    continue;
                }
                LogResolverMiss(
                    &Hit,
                    TEXT("blocked_by_non_hostile"),
                    nullptr,
                    Viewer,
                    Origin,
                    SafeDirection,
                    bExactTraceHit,
                    bUsedSweptTrace);
                return nullptr;
            }

            if (bUsedSweptTrace
                && bHasExactNonHostileBlocker
                && Hit.Distance >= ExactNonHostileBlockerDistance - KINDA_SMALL_NUMBER)
            {
                // The exact center ray hit cover before this swept target.
                // A sweep-only prop beside the center ray is intentionally
                // ignored, but a target at or beyond the center-ray blocker
                // is not visible and cannot be selected or damaged.
                LogResolverMiss(
                    &Hit,
                    TEXT("blocked_by_center_ray_before_target"),
                    Target,
                    Viewer,
                    Origin,
                    SafeDirection,
                    bExactTraceHit,
                    bUsedSweptTrace);
                return nullptr;
            }

            // Some cooked/rewound character setups can report a visible
            // component on the hostile before the dedicated aim surface. It
            // is safe to skip only components on this same first hostile: the
            // resolver still requires the dedicated surface before it returns,
            // and a different hostile remains an occluding actor.
            if (FirstHostileComponentOwner && FirstHostileComponentOwner != Target)
            {
                LogResolverMiss(
                    &Hit,
                    TEXT("occluded_by_second_hostile"),
                    FirstHostileComponentOwner,
                    Viewer,
                    Origin,
                    SafeDirection,
                    bExactTraceHit,
                    bUsedSweptTrace);
                return nullptr;
            }
            FirstHostileComponentOwner = Target;

            if (!IsCombatAimHit(Target, Hit))
            {
                continue;
            }

            if (bUsedSweptTrace)
            {
                const bool bExactRayAlreadyTouchedSameHostile = ExactHostileComponentOwner == Target;
                const float MaxHorizontalAngleDegrees = bExactRayAlreadyTouchedSameHostile
                    ? BMCombatTuning::CombatAimRenderPoseContinuityMaxHorizontalAngleDegrees
                    : BMCombatTuning::CombatAimMaxHorizontalAngleDegrees;
                if (!IsWithinHorizontalAimTolerance(
                        Target,
                        Origin,
                        SafeDirection,
                        MaxHorizontalAngleDegrees))
                {
                    LogResolverMiss(
                        &Hit,
                        bExactRayAlreadyTouchedSameHostile
                            ? TEXT("outside_render_pose_continuity_tolerance")
                            : TEXT("outside_horizontal_aim_tolerance"),
                        Target,
                        Viewer,
                        Origin,
                        SafeDirection,
                        bExactTraceHit,
                        bUsedSweptTrace);
                    return nullptr;
                }
            }

            if (OutHit)
            {
                *OutHit = Hit;
            }
            LogResolverHit(
                Hit,
                Target,
                Viewer,
                Origin,
                SafeDirection,
                bExactTraceHit,
                bUsedSweptTrace);
            return Target;
        }

        LogResolverMiss(
            FirstBlockingHit,
            TEXT("hostile_without_dedicated_aim_surface"),
            FirstHostileComponentOwner,
            Viewer,
            Origin,
            SafeDirection,
            bExactTraceHit,
            bUsedSweptTrace);
        return nullptr;
    }
}
