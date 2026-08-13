// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ABMCombatCharacter;
class AActor;
class UWorld;
struct FHitResult;

/**
 * The player reticle, player damage, and friendly support fire must agree on
 * the same target.  Keeping the query here prevents one of those paths from
 * quietly falling back to "any actor with health" or to a different collision
 * query than the one the player sees.
 */
namespace BMCombatTargeting
{
    bool IsLiveHostileTarget(const ABMCombatCharacter* Viewer, const AActor* Candidate);

    /**
     * Returns true only when the trace hit the target's dedicated
     * visibility-only combat hitbox. Actor identity alone is insufficient for
     * resolving a target: a movement capsule or another component on the same
     * pawn must never become an implicit aim-assist surface.
     */
    bool IsCombatAimHit(const ABMCombatCharacter* Target, const FHitResult& Hit);

    ABMCombatCharacter* ResolveLiveHostileCrosshairTarget(
        UWorld* World,
        const ABMCombatCharacter* Viewer,
        const FVector& Origin,
        const FVector& Direction,
        float Range,
        const AActor* AdditionalIgnoredActor,
        FHitResult* OutHit = nullptr);
}
