// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BMCombatHistoryComponent.generated.h"

class ABMCombatCharacter;
class UWorld;

/**
 * A server-captured combat pose.  The capsule dimensions are stored with the
 * transform so a crouching target is not rewound into a standing hitbox.
 */
struct FBMCombatHistoryPose
{
    float ServerTime = 0.0f;
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    float CapsuleRadius = 34.0f;
    float CapsuleHalfHeight = 88.0f;
    float AimHitboxRadius = 24.0f;
    float AimHitboxHalfHeight = 88.0f;
};

/**
 * Short, authority-only history used by server hitscan validation.  This is
 * intentionally bounded: it improves ordinary listen-server play without
 * allowing an unbounded client-requested rewind.
 */
UCLASS(ClassGroup = (BlackMeridian), meta = (BlueprintSpawnableComponent))
class BLACKMERIDIANRUNTIME_API UBMCombatHistoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBMCombatHistoryComponent();

    /** Maximum one-way rewind granted to a player-owned hitscan shot. */
    static constexpr float MaxRewindSeconds = 0.25f;

    /** Capture after the owning character has completed its movement tick. */
    void CaptureCurrentPose(float ServerTime);

    /**
     * Interpolate a pose from the bounded server history. Returns false when
     * the requested time is outside the available samples.
     */
    bool GetHistoricalPose(float ServerTime, FBMCombatHistoryPose& OutPose) const;

    int32 GetSampleCount() const { return History.Num(); }

protected:
    virtual void BeginPlay() override;

private:
    static constexpr float CaptureIntervalSeconds = 0.05f;
    static constexpr float HistoryWindowSeconds = 1.0f;
    static constexpr int32 MaxHistorySamples = 32;

    TArray<FBMCombatHistoryPose> History;
    float LastCaptureTime = -BIG_NUMBER;
};

/**
 * Temporarily places all eligible combatants at their historical poses while
 * one authoritative hitscan trace runs. The destructor always restores the
 * current pose, including stance and collision state.
 */
class BLACKMERIDIANRUNTIME_API FBMCombatHistoryRewindScope final
{
public:
    FBMCombatHistoryRewindScope(UWorld* World, const AActor* IgnoredActor, float TargetServerTime);
    ~FBMCombatHistoryRewindScope();

    FBMCombatHistoryRewindScope(const FBMCombatHistoryRewindScope&) = delete;
    FBMCombatHistoryRewindScope& operator=(const FBMCombatHistoryRewindScope&) = delete;

    /**
     * Restore live poses before the scope leaves its lexical lifetime. The
     * destructor remains the safety net and is idempotent with this call.
     */
    void RestoreCurrentPoses();

    int32 GetRewoundCharacterCount() const { return RestoreStates.Num(); }

private:
    struct FRestoreState
    {
        TWeakObjectPtr<ABMCombatCharacter> Character;
        FTransform CurrentTransform = FTransform::Identity;
        float CurrentCapsuleRadius = 34.0f;
        float CurrentCapsuleHalfHeight = 88.0f;
        ECollisionEnabled::Type CurrentCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
        float CurrentAimHitboxRadius = 24.0f;
        float CurrentAimHitboxHalfHeight = 88.0f;
        ECollisionEnabled::Type CurrentAimHitboxCollisionEnabled = ECollisionEnabled::QueryOnly;
    };

    TArray<FRestoreState> RestoreStates;
    bool bCurrentPosesRestored = false;
};
