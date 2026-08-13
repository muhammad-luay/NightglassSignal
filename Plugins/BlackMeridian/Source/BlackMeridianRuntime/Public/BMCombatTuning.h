// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "BMCoreTypes.h"

/**
 * Shared combat pacing values for the readable first encounter.  Keeping the
 * values in one source contract prevents the AI, wave director, and tests
 * from drifting apart while the placeholder weapons are still being tuned.
 */
namespace BMCombatTuning
{
    /**
     * Visibility-only character target surface used by the reticle and player
     * weapon validation. It remains narrower than the 34 cm locomotion capsule,
     * but gives the rendered torso/shoulders a usable target surface instead of
     * requiring the reticle to sit on a thin center strip. The vertical surface
     * follows the movement capsule so the visible upper torso and head do not
     * fall outside the target. The resolver applies the horizontal angle limit
     * to the small interpolation sweep below so the sweep cannot turn a close,
     * visible near miss into a hit. An exact visibility ray that reaches this
     * dedicated surface is already a precise target confirmation. Swept
     * contacts use a distance-aware lateral bound: the hitbox and sweep
     * radii provide the physical ceiling, while the angular ceiling keeps
     * short-range near misses outside the target.
     */
    inline constexpr float CombatAimHitboxRadius = 24.0f;
    // A small swept radius absorbs animation/network interpolation error at
    // the rendered reticle without turning the target into broad aim assist.
    // The resolver applies the sweep geometry and
    // CombatAimMaxHorizontalAngleDegrees only when this sweep is used; an
    // exact dedicated-hitbox ray does not need a second center-angle test
    // that can reject a legitimate edge-of-body shot.
    inline constexpr float CombatAimSweepRadius = 8.0f;
    // This is a hard short-range ceiling for the interpolation sweep. The
    // accepted angle is also limited by (hitbox radius + sweep radius) at the
    // target's actual distance, so the sweep gets tighter rather than wider
    // at long range. The 6.5 and 7 degree close-range near-miss probes remain
    // outside this bound.
    inline constexpr float CombatAimMaxHorizontalAngleDegrees = 5.0f;
    // When the exact screen-center ray already touched the same hostile's
    // rendered body, the dedicated hitbox can still be reached by the narrow
    // sweep after a one-frame animation/render-pose update. Allow only this
    // bounded continuity case to extend the angular guard; sweep-only actors
    // keep the same hard short-range ceiling.
    inline constexpr float CombatAimRenderPoseContinuityMaxHorizontalAngleDegrees = 5.0f;
    inline constexpr float CombatAimHitboxVerticalInset = 0.0f;

    inline constexpr float SquadFireSlotIntervalSeconds = 0.55f;
    inline constexpr float OpeningWaveMinimumStartDelaySeconds = 1.75f;
    inline constexpr float OpeningWaveDamageMultiplier = 0.28f;
    inline constexpr float OpeningWaveBreacherDamageMultiplier = 0.22f;
    inline constexpr float ShowcaseDamageMultiplier = 0.28f;

    /**
     * A single hostile weapon action should create pressure, not erase the
     * player before the damage-direction cue can be read.  This is a total
     * per-shot budget, so it also covers multi-pellet shotguns and headshots.
     */
    inline constexpr float HostileDamagePerShotCap = 22.0f;

    /** Frag grenades remain dangerous, but a point-blank enemy throw is not a
     * full-health one-shot.  The enemy damage multiplier is applied first. */
    inline constexpr float HostileExplosionDamageCap = 38.0f;

    /**
     * Shared target-side budget for all hostile damage sources. This is a
     * token bucket rather than a per-weapon limit, so several enemies cannot
     * combine otherwise-valid shots into a one-second full-health deletion.
     */
    inline constexpr float HostileDamageWindowSeconds = 1.0f;
    // Later waves may contain eight or more armed actors. Keep their shared
    // target-side pressure readable without making the player effectively
    // immortal: sustained exposed fire should normally kill in roughly five to
    // six seconds, while cover and movement remain the intended counterplay.
    inline constexpr float HostileDamageWindowCap = 18.0f;

    /**
     * A player who reaches the critical band gets one short, bounded recovery
     * window. Hostile fire remains visible and still subtracts health, but a
     * single missed cover decision cannot turn the next hit into an opaque
     * instant death before the low-health cue can be acted on.
     */
    inline constexpr float PlayerCriticalHealthThreshold = 15.0f;
    // Keep only a short last-chance readability beat at low health. This is not
    // a second health bar and must not cancel sustained hostile fire.
    inline constexpr float PlayerCriticalDamageScale = 0.75f;
    inline constexpr float PlayerCriticalRecoveryMinimumHealth = 2.0f;
    inline constexpr float PlayerCriticalRecoveryDurationSeconds = 0.75f;
    inline constexpr float PlayerCriticalRecoveryHealthCap = 8.0f;
    inline constexpr float PlayerCriticalRecoveryPerSecond = 2.0f;

    inline float ResolvePlayerHostileDamage(
        const float CurrentHealth,
        const float RequestedDamage,
        const bool bCriticalRecoveryActive)
    {
        if (RequestedDamage <= KINDA_SMALL_NUMBER)
        {
            return 0.0f;
        }

        float Damage = RequestedDamage;
        if (bCriticalRecoveryActive || CurrentHealth <= PlayerCriticalHealthThreshold)
        {
            Damage *= PlayerCriticalDamageScale;
        }
        if (bCriticalRecoveryActive)
        {
            Damage = FMath::Min(
                Damage,
                FMath::Max(0.0f, CurrentHealth - PlayerCriticalRecoveryMinimumHealth));
        }
        return FMath::Max(0.0f, Damage);
    }

    /**
     * The first encounter and the rendered combat showcase deliberately use
     * a softer shared bucket. This is separate from the per-shot multiplier:
     * several visible enemies must still return fire, but their combined
     * opening pressure must leave time for the player to read, aim, and reach
     * cover before health regeneration has even started.
     */
    // Keep the first combat lesson readable without allowing an unattended
    // opening lane to turn into a long invulnerability test. The player gets
    // several seconds to identify the armed silhouettes and reach cover.
    inline constexpr float OpeningWaveDamageWindowCap = 12.0f;

    inline constexpr float OpeningWaveAssaultAimErrorDegrees = 5.5f;
    inline constexpr float OpeningWaveMarksmanAimErrorDegrees = 3.0f;
    inline constexpr float OpeningWaveBreacherAimErrorDegrees = 6.5f;
    inline constexpr float OpeningWaveRiflemanAimErrorDegrees = 4.5f;
    inline constexpr float OpeningWaveDemolitionistAimErrorDegrees = 8.0f;

    /**
     * Tactical movement must give the player time to read silhouettes, aim,
     * reload, and choose cover, but it must still look like a soldier
     * advancing rather than feet shuffling in place. These are combat-walk
     * caps in cm/s; the player walk remains substantially faster.
     */
    inline constexpr float EnemyAssaultMoveSpeed = 190.0f;
    inline constexpr float EnemyMarksmanMoveSpeed = 125.0f;
    inline constexpr float EnemyBreacherMoveSpeed = 215.0f;
    inline constexpr float EnemyDemolitionistMoveSpeed = 145.0f;
    inline constexpr float EnemyRiflemanMoveSpeed = 170.0f;
    inline constexpr float EnemyApproachMinSpeedScale = 0.55f;
    inline constexpr float EnemyApproachMaxSpeedScale = 0.85f;
    inline constexpr float EnemyMinimumApproachSpeed = 85.0f;

    /**
     * Once an enemy reaches its role range it should continue to make a
     * short, readable forward pressure step. The current rifle locomotion
     * set is forward-only, so route input remains independent while the body
     * and weapon keep presenting the live combat target through an engagement
     * band. A later strafe blend space can replace this without changing the
     * target/facing contract.
     */
    inline constexpr float EnemyCombatManeuverDurationSeconds = 1.55f;
    inline constexpr float EnemyMarksmanCombatManeuverDurationSeconds = 4.50f;
    // Retained for authored tactical routes that still use a finite waypoint.
    // Combat pursuit now uses one continuous target route instead of chaining
    // short pressure points, so this no longer controls the normal approach.
    inline constexpr float EnemyCombatManeuverAcceptanceRadius = 56.0f;
    inline constexpr float EnemyCombatManeuverRangeOffset = 55.0f;
    inline constexpr float EnemyCombatManeuverMaxAdvance = 220.0f;
    // Once an enemy has reached its role range, do not restart pursuit for
    // every small player movement. It must move again only after the target
    // has created a meaningful closing error.
    // Keep the range hold large enough to prevent a moving target from
    // restarting pursuit on millimetre-level position noise, but small enough
    // that long-range roles visibly complete their approach. 120 cm left the
    // marksman and outer rifleman with only a short 90-114 cm burst in the
    // packaged showcase, which read as a stalled/jittery route.
    inline constexpr float EnemyCombatPursuitHysteresis = 48.0f;
    // The current rifle locomotion set has no backward or strafe clips. A
    // target-relative pressure point may therefore only be accepted in the
    // forward hemisphere; otherwise it holds and reacquires a forward point.
    inline constexpr float EnemyCombatMovementMinimumForwardDot = 0.45f;

    /**
     * Enemy bodies should turn into a route instead of snapping to it. This
     * is deliberately faster than the tactical walk so a target hand-off
     * remains readable without leaving the soldier moving sideways for a
     * visible beat.
     */
    inline constexpr float EnemyMovementRotationRateDegreesPerSecond = 540.0f;
    inline constexpr float EnemyMovementSteeringRateDegreesPerSecond = 300.0f;
    inline constexpr float EnemyMovementFacingToleranceDegrees = 30.0f;
    /**
     * One bounded target-heading filter owns the combat turn. The capsule and
     * rendered mesh consume that same heading instead of each easing toward a
     * slightly different copy of the player's position.
     */
    inline constexpr float EnemyCombatTargetFacingRateDegreesPerSecond = 720.0f;
    // Ignore sub-degree target-position noise. The player capsule can move by
    // a few millimetres while standing on a replicated floor; feeding that
    // noise into a visible enemy's full-body yaw makes the shoulders twitch.
    inline constexpr float EnemyCombatTargetFacingDeadbandDegrees = 1.0f;

    inline float GetEnemyApproachSpeed(const float RoleMoveSpeed, const float ClosingAlpha)
    {
        return FMath::Max(
            EnemyMinimumApproachSpeed,
            RoleMoveSpeed * FMath::Lerp(
                EnemyApproachMinSpeedScale,
                EnemyApproachMaxSpeedScale,
                FMath::Clamp(ClosingAlpha, 0.0f, 1.0f)));
    }

    inline float GetOpeningWaveDamageMultiplier(const EBMEnemyRole Role)
    {
        return Role == EBMEnemyRole::Breacher
            ? OpeningWaveBreacherDamageMultiplier
            : OpeningWaveDamageMultiplier;
    }

    inline float GetOpeningWaveAimErrorDegrees(const EBMEnemyRole Role)
    {
        switch (Role)
        {
        case EBMEnemyRole::Assault:
            return OpeningWaveAssaultAimErrorDegrees;
        case EBMEnemyRole::Marksman:
            return OpeningWaveMarksmanAimErrorDegrees;
        case EBMEnemyRole::Breacher:
            return OpeningWaveBreacherAimErrorDegrees;
        case EBMEnemyRole::Demolitionist:
            return OpeningWaveDemolitionistAimErrorDegrees;
        case EBMEnemyRole::Rifleman:
        default:
            return OpeningWaveRiflemanAimErrorDegrees;
        }
    }
}
