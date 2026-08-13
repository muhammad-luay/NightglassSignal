// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMCombatHistoryComponent.h"

#include "BMCombatCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UBMCombatHistoryComponent::UBMCombatHistoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false);
}

void UBMCombatHistoryComponent::BeginPlay()
{
    Super::BeginPlay();
    History.Reset();
    LastCaptureTime = -BIG_NUMBER;
}

void UBMCombatHistoryComponent::CaptureCurrentPose(const float ServerTime)
{
    ABMCombatCharacter* Character = Cast<ABMCombatCharacter>(GetOwner());
    if (!Character || !Character->HasAuthority() || Character->IsEliminated() || !FMath::IsFinite(ServerTime))
    {
        return;
    }

    if (ServerTime - LastCaptureTime < CaptureIntervalSeconds)
    {
        return;
    }

    FBMCombatHistoryPose Pose;
    Pose.ServerTime = ServerTime;
    Pose.Location = Character->GetActorLocation();
    Pose.Rotation = Character->GetActorRotation();
    if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
    {
        Pose.CapsuleRadius = Capsule->GetScaledCapsuleRadius();
        Pose.CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    }
    if (const UCapsuleComponent* AimHitbox = Character->GetCombatAimHitbox())
    {
        Pose.AimHitboxRadius = AimHitbox->GetScaledCapsuleRadius();
        Pose.AimHitboxHalfHeight = AimHitbox->GetScaledCapsuleHalfHeight();
    }

    if (!History.IsEmpty() && FMath::IsNearlyEqual(History.Last().ServerTime, ServerTime, KINDA_SMALL_NUMBER))
    {
        History.Last() = Pose;
    }
    else
    {
        History.Add(Pose);
    }

    LastCaptureTime = ServerTime;
    while (History.Num() > MaxHistorySamples || (!History.IsEmpty() && ServerTime - History[0].ServerTime > HistoryWindowSeconds))
    {
        History.RemoveAt(0, 1, EAllowShrinking::No);
    }
}

bool UBMCombatHistoryComponent::GetHistoricalPose(
    const float ServerTime,
    FBMCombatHistoryPose& OutPose) const
{
    if (History.IsEmpty() || !FMath::IsFinite(ServerTime))
    {
        return false;
    }

    const float NewestSampleDelta = ServerTime - History.Last().ServerTime;
    if (ServerTime < History[0].ServerTime ||
        NewestSampleDelta > CaptureIntervalSeconds + KINDA_SMALL_NUMBER)
    {
        return false;
    }

    // A shot can arrive just after the most recent fixed-rate capture. Treat
    // that small future gap as the current pose instead of dropping every
    // low-latency rewind because the requested half-RTT falls between ticks.
    // Larger gaps are still rejected so a missing/stalled history cannot turn
    // into an unbounded present-time hitbox.
    if (NewestSampleDelta >= 0.0f)
    {
        OutPose = History.Last();
        return true;
    }

    if (History.Num() == 1 || ServerTime <= History[0].ServerTime)
    {
        OutPose = History[0];
        return true;
    }

    for (int32 Index = 1; Index < History.Num(); ++Index)
    {
        const FBMCombatHistoryPose& Upper = History[Index];
        if (ServerTime > Upper.ServerTime)
        {
            continue;
        }

        const FBMCombatHistoryPose& Lower = History[Index - 1];
        const float SampleDuration = FMath::Max(KINDA_SMALL_NUMBER, Upper.ServerTime - Lower.ServerTime);
        const float Alpha = FMath::Clamp((ServerTime - Lower.ServerTime) / SampleDuration, 0.0f, 1.0f);
        OutPose.ServerTime = FMath::Lerp(Lower.ServerTime, Upper.ServerTime, Alpha);
        OutPose.Location = FMath::Lerp(Lower.Location, Upper.Location, Alpha);
        OutPose.Rotation = FQuat::Slerp(Lower.Rotation.Quaternion(), Upper.Rotation.Quaternion(), Alpha).Rotator();
        OutPose.CapsuleRadius = FMath::Lerp(Lower.CapsuleRadius, Upper.CapsuleRadius, Alpha);
        OutPose.CapsuleHalfHeight = FMath::Lerp(Lower.CapsuleHalfHeight, Upper.CapsuleHalfHeight, Alpha);
        OutPose.AimHitboxRadius = FMath::Lerp(Lower.AimHitboxRadius, Upper.AimHitboxRadius, Alpha);
        OutPose.AimHitboxHalfHeight = FMath::Lerp(Lower.AimHitboxHalfHeight, Upper.AimHitboxHalfHeight, Alpha);
        return true;
    }

    OutPose = History.Last();
    return true;
}

FBMCombatHistoryRewindScope::FBMCombatHistoryRewindScope(
    UWorld* World,
    const AActor* IgnoredActor,
    const float TargetServerTime)
{
    if (!World || !World->GetAuthGameMode() || !FMath::IsFinite(TargetServerTime))
    {
        return;
    }

    for (TActorIterator<ABMCombatCharacter> Iterator(World); Iterator; ++Iterator)
    {
        ABMCombatCharacter* Character = *Iterator;
        if (!IsValid(Character) || Character == IgnoredActor || Character->IsEliminated() || !Character->CombatHistoryComponent)
        {
            continue;
        }

        FBMCombatHistoryPose HistoricalPose;
        if (!Character->CombatHistoryComponent->GetHistoricalPose(TargetServerTime, HistoricalPose))
        {
            continue;
        }

        UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
        if (!Capsule)
        {
            continue;
        }
        UCapsuleComponent* AimHitbox = Character->GetCombatAimHitbox();

        FRestoreState& RestoreState = RestoreStates.AddDefaulted_GetRef();
        RestoreState.Character = Character;
        RestoreState.CurrentTransform = Character->GetActorTransform();
        RestoreState.CurrentCapsuleRadius = Capsule->GetScaledCapsuleRadius();
        RestoreState.CurrentCapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
        RestoreState.CurrentCollisionEnabled = Capsule->GetCollisionEnabled();
        if (AimHitbox)
        {
            RestoreState.CurrentAimHitboxRadius = AimHitbox->GetScaledCapsuleRadius();
            RestoreState.CurrentAimHitboxHalfHeight = AimHitbox->GetScaledCapsuleHalfHeight();
            RestoreState.CurrentAimHitboxCollisionEnabled = AimHitbox->GetCollisionEnabled();
        }

        Capsule->SetCapsuleRadius(HistoricalPose.CapsuleRadius, false);
        Capsule->SetCapsuleHalfHeight(HistoricalPose.CapsuleHalfHeight, false);
        if (AimHitbox)
        {
            AimHitbox->SetCapsuleRadius(HistoricalPose.AimHitboxRadius, false);
            AimHitbox->SetCapsuleHalfHeight(HistoricalPose.AimHitboxHalfHeight, false);
            AimHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        }
        Character->SetActorLocationAndRotation(
            HistoricalPose.Location,
            HistoricalPose.Rotation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
        // The rewind is immediately followed by a visibility trace.  Force
        // attached hitboxes to publish their teleported transform now rather
        // than relying on the next component tick to refresh the query scene.
        Capsule->UpdateComponentToWorld();
        if (AimHitbox)
        {
            AimHitbox->UpdateComponentToWorld();
        }
    }
}

void FBMCombatHistoryRewindScope::RestoreCurrentPoses()
{
    if (bCurrentPosesRestored)
    {
        return;
    }

    bCurrentPosesRestored = true;
    for (int32 Index = RestoreStates.Num() - 1; Index >= 0; --Index)
    {
        FRestoreState& RestoreState = RestoreStates[Index];
        ABMCombatCharacter* Character = RestoreState.Character.Get();
        if (!IsValid(Character))
        {
            continue;
        }

        Character->SetActorLocationAndRotation(
            RestoreState.CurrentTransform.GetLocation(),
            RestoreState.CurrentTransform.Rotator(),
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
        if (UCapsuleComponent* RestoredCapsule = Character->GetCapsuleComponent())
        {
            RestoredCapsule->UpdateComponentToWorld();
        }
        if (UCapsuleComponent* RestoredAimHitbox = Character->GetCombatAimHitbox())
        {
            RestoredAimHitbox->UpdateComponentToWorld();
        }

        if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
        {
            // A target can die during the trace. Preserve the death path's
            // disabled collision in that case; otherwise restore the exact
            // live stance that existed before the rewind.
            if (!Character->IsEliminated())
            {
                Capsule->SetCapsuleRadius(RestoreState.CurrentCapsuleRadius, false);
                Capsule->SetCapsuleHalfHeight(RestoreState.CurrentCapsuleHalfHeight, false);
                Capsule->SetCollisionEnabled(RestoreState.CurrentCollisionEnabled);
                if (UCapsuleComponent* AimHitbox = Character->GetCombatAimHitbox())
                {
                    AimHitbox->SetCapsuleRadius(RestoreState.CurrentAimHitboxRadius, false);
                    AimHitbox->SetCapsuleHalfHeight(RestoreState.CurrentAimHitboxHalfHeight, false);
                    AimHitbox->SetCollisionEnabled(RestoreState.CurrentAimHitboxCollisionEnabled);
                    AimHitbox->UpdateComponentToWorld();
                }
            }
        }
    }
}

FBMCombatHistoryRewindScope::~FBMCombatHistoryRewindScope()
{
    RestoreCurrentPoses();
}
