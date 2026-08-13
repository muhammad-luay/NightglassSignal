// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMGameMode.h"

#include "BMGreyboxDirector.h"
#include "BMCampaignLevel.h"
#include "BMEnemyCharacter.h"
#include "BMEnemyAIController.h"
#include "BMFriendlySquadMember.h"
#include "BMArtDirectedProp.h"
#include "BMInteractableActor.h"
#include "BMInteractionInterface.h"
#include "BMMissionDirector.h"
#include "BMPlayerCharacter.h"
#include "BMPlayerController.h"
#include "BMPlayerState.h"
#include "BMPlayerProfileSaveGame.h"
#include "BMGameState.h"
#include "BMHealthComponent.h"
#include "BMHUD.h"
#include "BMCombatTargeting.h"
#include "BMCombatTuning.h"
#include "BMSuppressionComponent.h"
#include "BMThrowable.h"
#include "BMThrowableDataAsset.h"
#include "BMWeapon.h"
#include "BMWeaponDataAsset.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"

namespace
{
    FString GetProfileSaveSlotName()
    {
#if !UE_BUILD_SHIPPING
        FString ProbeSlot;
        if (FParse::Value(FCommandLine::Get(), TEXT("BM_ProfileSlot="), ProbeSlot) && !ProbeSlot.IsEmpty())
        {
            return ProbeSlot;
        }
#endif
        return TEXT("BlackMeridian_Profile");
    }

    EBMCampaignLevel ParseCampaignLevel(const FString& Options = FString())
    {
        // A results-screen "NEXT LEVEL" travel carries the selected level in
        // the Entry map URL.  URL options must take precedence over the
        // original process command line, otherwise a development launch such
        // as -BM_CampaignLevel=0 would force every subsequent level back to
        // Sable Quay after travel.
        const FString UrlLevel = UGameplayStatics::ParseOption(Options, TEXT("BM_CampaignLevel"));
        if (!UrlLevel.IsEmpty())
        {
            const int32 LevelIndex = FMath::Clamp(FCString::Atoi(*UrlLevel), 0, GetBMCampaignLevelCount() - 1);
            return static_cast<EBMCampaignLevel>(LevelIndex);
        }
#if !UE_BUILD_SHIPPING
        FString RequestedLevel;
        if (FParse::Value(FCommandLine::Get(), TEXT("BM_CampaignLevel="), RequestedLevel))
        {
            const int32 LevelIndex = FMath::Clamp(FCString::Atoi(*RequestedLevel), 0, GetBMCampaignLevelCount() - 1);
            return static_cast<EBMCampaignLevel>(LevelIndex);
        }
#endif
    return EBMCampaignLevel::SableQuay;
}
}

ABMGameMode::ABMGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    GameStateClass = ABMGameState::StaticClass();
    PlayerStateClass = ABMPlayerState::StaticClass();
    PlayerControllerClass = ABMPlayerController::StaticClass();
    DefaultPawnClass = ABMPlayerCharacter::StaticClass();
    HUDClass = ABMHUD::StaticClass();
    bStartPlayersAsSpectators = false;
    EnemyCharacterClass = ABMEnemyCharacter::StaticClass();
}

void ABMGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    CampaignLevel = ParseCampaignLevel(Options);
}

void ABMGameMode::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_BoundaryGuardProbe")))
    {
        RunBoundaryGuardProbe(DeltaSeconds);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatFeelProbe")))
    {
        RunCombatFeelProbe(DeltaSeconds);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_AimValidationProbe")))
    {
        RunAimValidationProbe(DeltaSeconds);
    }
    const bool bCombatShowcase = FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcase"));
    if (bCombatShowcase)
    {
        // Combat showcase sessions are development capture sessions and must
        // never be left behind a stale mission-menu pause. This GameMode tick
        // is explicitly enabled for that path and is allowed to run while
        // paused so it can release a late pause before AI/controller ticks.
        if (GetWorld() && GetWorld()->IsPaused())
        {
            if (APlayerController* LocalController = GetWorld()->GetFirstPlayerController())
            {
                LocalController->SetPause(false);
                UE_LOG(LogTemp, Display, TEXT("Black Meridian showcase: late pause released"));
            }
        }

        CombatShowcaseDiagnosticTimer -= FMath::Max(DeltaSeconds, 0.0f);
        if (CombatShowcaseDiagnosticTimer <= 0.0f)
        {
            CombatShowcaseDiagnosticTimer = 1.0f;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian showcase heartbeat: delta=%.3f world_paused=%s gamemode_tick=%s"),
                DeltaSeconds,
                GetWorld() && GetWorld()->IsPaused() ? TEXT("yes") : TEXT("no"),
                IsActorTickEnabled() ? TEXT("yes") : TEXT("no"));
        }
    }

    // Each development probe owns its own tick enablement in BeginPlay. Keep
    // the probe dispatch independent from the rendered combat showcase so a
    // standalone probe (for example -BM_ThrowableProbe) cannot arm and then
    // stall merely because the showcase flag was omitted.
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_ThrowableProbe")))
    {
        RunThrowableProbe();
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_ThrowablePresentationShowcase")))
    {
        RunThrowablePresentationShowcase();
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_ThrowableInputProbe")))
    {
        RunThrowableInputProbe();
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_ReloadInputProbe")))
    {
        RunReloadInputProbe();
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_FullCampaignProbe")))
    {
        RunFullCampaignProbe(DeltaSeconds);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_InteractionAcceptanceProbe")))
    {
        RunInteractionAcceptanceProbe();
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_DestructionPresentationShowcase")))
    {
        RunDestructionPresentationShowcase(DeltaSeconds);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_PickupProbe")))
    {
        RunPickupProbe();
    }
#endif
#if !UE_BUILD_SHIPPING
    PickupProbeElapsedSeconds += DeltaSeconds;
#else
    (void)DeltaSeconds;
#endif
}

#if !UE_BUILD_SHIPPING
void ABMGameMode::RunAimValidationProbe(const float DeltaSeconds)
{
    if (bAimValidationProbeFailed || !GetWorld())
    {
        return;
    }

    if (GetWorld()->IsPaused())
    {
        if (APlayerController* LocalController = GetWorld()->GetFirstPlayerController())
        {
            LocalController->SetPause(false);
        }
        return;
    }

    ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    ABMWeapon* Weapon = Player ? Player->GetCurrentWeapon() : nullptr;
    ABMGameState* State = GetGameState<ABMGameState>();
    if (!Player || !Weapon || !Weapon->GetWeaponData() || !Player->HasActorBegunPlay())
    {
        return;
    }

    const auto FailProbe = [this](const TCHAR* Reason)
    {
        if (bAimValidationProbeFailed)
        {
            return;
        }
        bAimValidationProbeFailed = true;
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Black Meridian aim validation probe: FAIL reason=%s stage=%d target=%s"),
            Reason ? Reason : TEXT("unknown"),
            AimValidationProbeStage,
            *GetNameSafe(AimValidationProbeTarget.Get()));
        if (AimValidationProbeTarget.IsValid())
        {
            AimValidationProbeTarget->Destroy();
            AimValidationProbeTarget.Reset();
        }
        SetActorTickEnabled(false);
        FPlatformMisc::RequestExit(false);
    };

    // The target is deliberately isolated from the normal mission wave. That
    // makes this a precise regression test for the player weapon path rather
    // than an ambiguous encounter result caused by an AI shot or an existing
    // explosion. The normal campaign remains untouched after the probe exits.
    if (AimValidationProbeStage == 0)
    {
        if (!ABMGameState::IsCombatOrdnanceEnabled(State))
        {
            return;
        }

        if (!Weapon->SetCombatProbeAmmo(5, 0))
        {
            FailProbe(TEXT("could_not_prepare_player_ammo"));
            return;
        }

        const FVector ViewOrigin = Player->GetPawnViewLocation();
        const FVector Forward = Player->GetFirstPersonCamera()
            ? Player->GetFirstPersonCamera()->GetForwardVector().GetSafeNormal()
            : Player->GetActorForwardVector().GetSafeNormal();
        if (Forward.IsNearlyZero())
        {
            FailProbe(TEXT("player_view_direction_missing"));
            return;
        }

        // Keep the target on the current forward lane and at the player's
        // floor height. The later direct aim rotation is calculated from the
        // target's actual post-spawn transform, so collision adjustment cannot
        // make the positive case accidentally miss.
        const FVector SpawnLocation = Player->GetActorLocation() + Forward.GetSafeNormal2D() * 680.0f;
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        TSubclassOf<ABMEnemyCharacter> SpawnClass = EnemyCharacterClass;
        if (!SpawnClass)
        {
            SpawnClass = ABMEnemyCharacter::StaticClass();
        }

        ABMEnemyCharacter* Target = GetWorld()->SpawnActor<ABMEnemyCharacter>(
            SpawnClass,
            FVector(SpawnLocation.X, SpawnLocation.Y, Player->GetActorLocation().Z),
            FRotator(0.0f, (Player->GetActorLocation() - SpawnLocation).Rotation().Yaw, 0.0f),
            SpawnParameters);
        if (!Target)
        {
            FailProbe(TEXT("target_spawn_failed"));
            return;
        }

        AimValidationProbeTarget = Target;
        Target->SetCombatStartDelay(60.0f);
        Target->SetAIState(EBMAIState::Suppressed);
        if (AController* Controller = Target->GetController())
        {
            Controller->StopMovement();
            Controller->UnPossess();
            Controller->Destroy();
        }
        if (UCharacterMovementComponent* Movement = Target->GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
            Movement->SetMovementMode(MOVE_None);
        }

        AimValidationProbeElapsedSeconds = 0.0f;
        AimValidationProbeStage = 1;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian aim validation probe: armed target=%s player=%s target_location=%s weapon=%s"),
            *GetNameSafe(Target),
            *GetNameSafe(Player),
            *Target->GetActorLocation().ToString(),
            *GetNameSafe(Weapon));
        return;
    }

    AimValidationProbeElapsedSeconds += FMath::Max(0.0f, DeltaSeconds);
    ABMEnemyCharacter* Target = AimValidationProbeTarget.Get();
    if (!Target || Target->IsEliminated() || !Target->HealthComponent)
    {
        FailProbe(TEXT("target_invalid_before_validation"));
        return;
    }

    // Keep the enemy stationary for the two rays. This does not alter the
    // production AI path; it only removes movement as a variable in the
    // deterministic probe.
    if (!AimValidationProbeTargetLocation.IsNearlyZero())
    {
        Target->SetActorLocation(AimValidationProbeTargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
    }
    if (UCharacterMovementComponent* Movement = Target->GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
        Movement->SetMovementMode(MOVE_None);
    }

    if (AimValidationProbeStage == 1)
    {
        if (AimValidationProbeElapsedSeconds < 0.20f)
        {
            return;
        }

        if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
            Movement->SetMovementMode(MOVE_None);
        }

        const FVector ViewOrigin = Player->GetFirstPersonCamera()
            ? Player->GetFirstPersonCamera()->GetComponentLocation()
            : Player->GetPawnViewLocation();
        const FVector DirectDirection = Player->GetFirstPersonCamera()
            ? Player->GetFirstPersonCamera()->GetForwardVector().GetSafeNormal()
            : Player->GetActorForwardVector().GetSafeNormal();
        if (DirectDirection.IsNearlyZero())
        {
            FailProbe(TEXT("direct_target_direction_missing"));
            return;
        }

        // Reposition the test target onto the exact camera ray after the
        // runtime floor and player camera have settled. Keep its capsule
        // center on that ray and verify the visibility query before firing;
        // this prevents a physical layout obstruction from masquerading as
        // an aim-validation failure.
        const FVector TargetPoint = ViewOrigin + DirectDirection * 520.0f;
        AimValidationProbeTargetLocation = TargetPoint - FVector(0.0f, 0.0f, 72.0f);
        Target->SetActorLocation(
            AimValidationProbeTargetLocation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
        if (UCapsuleComponent* AimHitbox = Target->GetCombatAimHitbox())
        {
            AimHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            AimHitbox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        }

        FHitResult DirectVisibilityHit;
        FCollisionQueryParams DirectVisibilityParams(
            FName(TEXT("BlackMeridianAimValidation")),
            true,
            Player);
        DirectVisibilityParams.AddIgnoredActor(Weapon);
        const bool bDirectVisibilityHit = GetWorld()->LineTraceSingleByChannel(
            DirectVisibilityHit,
            ViewOrigin,
            TargetPoint,
            ECC_Visibility,
            DirectVisibilityParams);
        if (!bDirectVisibilityHit || DirectVisibilityHit.GetActor() != Target)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Black Meridian aim validation probe: target_visibility_failed expected=%s hit=%s origin=%s target_point=%s"),
                *GetNameSafe(Target),
                *GetNameSafe(bDirectVisibilityHit ? DirectVisibilityHit.GetActor() : nullptr),
                *ViewOrigin.ToString(),
                *TargetPoint.ToString());
            FailProbe(TEXT("target_not_first_actor_under_verified_direct_ray"));
            return;
        }

        if (Target->HealthComponent)
        {
            Target->HealthComponent->bRegenerateHealth = false;
            Target->HealthComponent->SetHealthForCheckpoint(100.0f);
        }

        // The positive control is a damage-gate test, not an accuracy test.
        // Disable spread for this one deterministic probe so the new
        // single-pellet hip-fire behavior cannot make a mathematically exact
        // crosshair ray miss the one-centimetre aim hitbox by chance. The
        // spread contract is covered separately by the WeaponSpread test.
        Weapon->GetWeaponData()->Balance.HipSpreadDegrees = 0.0f;
        Weapon->GetWeaponData()->Balance.ADSSpreadDegrees = 0.0f;

        const FVector OffCrosshairDirection = DirectDirection.RotateAngleAxis(35.0f, FVector::UpVector).GetSafeNormal();
        AimValidationProbeHealthBefore = Target->HealthComponent->Health;
        AimValidationProbeMagazineBefore = Weapon->GetMagazineAmmo();
        Player->RunCombatProbeFire(OffCrosshairDirection);
        AimValidationProbeElapsedSeconds = 0.0f;
        AimValidationProbeStage = 2;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian aim validation probe: off_target_shot angle_deg=35.0 health_before=%.1f magazine_before=%d direction=%s"),
            AimValidationProbeHealthBefore,
            AimValidationProbeMagazineBefore,
            *OffCrosshairDirection.ToString());
        return;
    }

    if (AimValidationProbeStage == 2)
    {
        if (AimValidationProbeElapsedSeconds < 0.20f)
        {
            return;
        }

        const float HealthAfterOffTarget = Target->HealthComponent->Health;
        const int32 MagazineAfterOffTarget = Weapon->GetMagazineAmmo();
        const bool bOffTargetDidNotDamage = FMath::IsNearlyEqual(
            HealthAfterOffTarget,
            AimValidationProbeHealthBefore,
            0.1f);
        const bool bOffTargetDidNotConsumeRound = MagazineAfterOffTarget == AimValidationProbeMagazineBefore;
        if (!bOffTargetDidNotDamage || !bOffTargetDidNotConsumeRound)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Black Meridian aim validation probe: off_target_result blocked=%s ammo_consumed=%s health=%.1f->%.1f magazine=%d->%d"),
                bOffTargetDidNotDamage ? TEXT("yes") : TEXT("no"),
                bOffTargetDidNotConsumeRound ? TEXT("no") : TEXT("yes"),
                AimValidationProbeHealthBefore,
                HealthAfterOffTarget,
                AimValidationProbeMagazineBefore,
                MagazineAfterOffTarget);
            FailProbe(TEXT("off_target_shot_damaged_enemy_or_consumed_ammo"));
            return;
        }

        // Exercise the final health boundary independently of the weapon
        // trace. This intentionally invalid player-weapon context must be
        // rejected even if a future caller accidentally bypasses the earlier
        // crosshair gate.
        FBMDamageContext InvalidWeaponContext;
        InvalidWeaponContext.RawDamage = 40.0f;
        InvalidWeaponContext.FinalDamage = 40.0f;
        InvalidWeaponContext.HitZone = EBMHitZone::Torso;
        InvalidWeaponContext.InstigatorActor = Player;
        InvalidWeaponContext.bPlayerWeaponDamage = true;
        InvalidWeaponContext.bPlayerWeaponAimValidated = false;
        const float InvalidBoundaryDamage = Target->HealthComponent->ApplyDamage(
            InvalidWeaponContext.FinalDamage,
            InvalidWeaponContext);
        if (InvalidBoundaryDamage > KINDA_SMALL_NUMBER
            || !FMath::IsNearlyEqual(Target->HealthComponent->Health, AimValidationProbeHealthBefore, 0.1f))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Black Meridian aim validation probe: health_boundary_result blocked=no applied=%.1f health=%.1f"),
                InvalidBoundaryDamage,
                Target->HealthComponent->Health);
            FailProbe(TEXT("health_boundary_accepted_unvalidated_player_weapon_damage"));
            return;
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian aim validation probe: health_boundary_result blocked=yes applied=%.1f"),
            InvalidBoundaryDamage);

        // Also reject a player-to-enemy damage request that forgot to mark
        // itself as a weapon request. This is the bypass that a future direct
        // caller could otherwise use to skip the firearm validation bit
        // entirely. Melee and throwable callers carry their own explicit
        // source marker and remain valid separate gameplay actions.
        FBMDamageContext UnclassifiedPlayerContext;
        UnclassifiedPlayerContext.RawDamage = 40.0f;
        UnclassifiedPlayerContext.FinalDamage = 40.0f;
        UnclassifiedPlayerContext.HitZone = EBMHitZone::Torso;
        UnclassifiedPlayerContext.InstigatorActor = Player;
        const float UnclassifiedPlayerDamage = Target->HealthComponent->ApplyDamage(
            UnclassifiedPlayerContext.FinalDamage,
            UnclassifiedPlayerContext);
        if (UnclassifiedPlayerDamage > KINDA_SMALL_NUMBER
            || !FMath::IsNearlyEqual(Target->HealthComponent->Health, AimValidationProbeHealthBefore, 0.1f))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Black Meridian aim validation probe: unclassified_player_boundary_result blocked=no applied=%.1f health=%.1f"),
                UnclassifiedPlayerDamage,
                Target->HealthComponent->Health);
            FailProbe(TEXT("health_boundary_accepted_unclassified_player_enemy_damage"));
            return;
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian aim validation probe: unclassified_player_boundary_result blocked=yes applied=%.1f"),
            UnclassifiedPlayerDamage);

        const FVector ViewOrigin = Player->GetFirstPersonCamera()
            ? Player->GetFirstPersonCamera()->GetComponentLocation()
            : Player->GetPawnViewLocation();
        const FVector InitialAimPoint = Target->GetActorLocation() + FVector(0.0f, 0.0f, 48.0f);
        const FVector ApproachDirection = (InitialAimPoint - ViewOrigin).GetSafeNormal();
        const FVector CloseRangeAimPoint = ViewOrigin + ApproachDirection * 220.0f;
        AimValidationProbeTargetLocation = CloseRangeAimPoint - FVector(0.0f, 0.0f, 48.0f);
        Target->SetActorLocation(
            AimValidationProbeTargetLocation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
        const FVector DirectDirection = (Target->GetActorLocation() + FVector(0.0f, 0.0f, 48.0f) - ViewOrigin).GetSafeNormal();
        // These are deliberately outside the dedicated 24 cm aim surface at
        // a close 220 cm range. They guard against regressing to invisible
        // movement-capsule aim assist in the exact situation the player sees
        // most often: a reticle that is visibly beside the enemy. The narrow
        // interpolation sweep can still see the nearby target, but its strict
        // angular guard must reject the shot.
        const FVector CloseMissDirection = DirectDirection.RotateAngleAxis(7.0f, FVector::UpVector).GetSafeNormal();
        Player->RunCombatProbeFire(CloseMissDirection);
        AimValidationProbeElapsedSeconds = 0.0f;
        AimValidationProbeStage = 3;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian aim validation probe: close_miss_shot angle_deg=7.0 health_before=%.1f magazine_before=%d direction=%s"),
            HealthAfterOffTarget,
            MagazineAfterOffTarget,
            *CloseMissDirection.ToString());
        return;
    }

    if (AimValidationProbeStage == 3)
    {
        if (AimValidationProbeElapsedSeconds < 0.20f)
        {
            return;
        }

        const float HealthAfterHalfDegreeMiss = Target->HealthComponent->Health;
        const int32 MagazineAfterHalfDegreeMiss = Weapon->GetMagazineAmmo();
        const bool bHalfDegreeMissDidNotDamage = FMath::IsNearlyEqual(
            HealthAfterHalfDegreeMiss,
            AimValidationProbeHealthBefore,
            0.1f);
        const bool bHalfDegreeMissDidNotConsumeRound = MagazineAfterHalfDegreeMiss == AimValidationProbeMagazineBefore;
        if (!bHalfDegreeMissDidNotDamage || !bHalfDegreeMissDidNotConsumeRound)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Black Meridian aim validation probe: close_miss_result angle_deg=7.0 blocked=%s ammo_consumed=%s health=%.1f->%.1f magazine=%d->%d"),
                bHalfDegreeMissDidNotDamage ? TEXT("yes") : TEXT("no"),
                bHalfDegreeMissDidNotConsumeRound ? TEXT("no") : TEXT("yes"),
                AimValidationProbeHealthBefore,
                HealthAfterHalfDegreeMiss,
                AimValidationProbeMagazineBefore,
                MagazineAfterHalfDegreeMiss);
            FailProbe(TEXT("half_degree_near_miss_shot_damaged_enemy_or_consumed_ammo"));
            return;
        }

        const FVector ViewOrigin = Player->GetFirstPersonCamera()
            ? Player->GetFirstPersonCamera()->GetComponentLocation()
            : Player->GetPawnViewLocation();
        const FVector CloseMissPoint = Target->GetActorLocation() + FVector(0.0f, 0.0f, 48.0f);
        const FVector CloseMissDirection = (CloseMissPoint - ViewOrigin).GetSafeNormal();
        const FVector NearMissDirection = CloseMissDirection.RotateAngleAxis(6.5f, FVector::UpVector).GetSafeNormal();
        Player->RunCombatProbeFire(NearMissDirection);
        AimValidationProbeElapsedSeconds = 0.0f;
        AimValidationProbeStage = 4;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian aim validation probe: close_miss_shot angle_deg=6.5 health_before=%.1f magazine_before=%d direction=%s"),
            HealthAfterHalfDegreeMiss,
            MagazineAfterHalfDegreeMiss,
            *NearMissDirection.ToString());
        return;
    }

    if (AimValidationProbeStage == 4)
    {
        if (AimValidationProbeElapsedSeconds < 0.25f)
        {
            return;
        }

        const float HealthAfterQuarterDegreeMiss = Target->HealthComponent->Health;
        const int32 MagazineAfterQuarterDegreeMiss = Weapon->GetMagazineAmmo();
        const bool bQuarterDegreeMissDidNotDamage = FMath::IsNearlyEqual(
            HealthAfterQuarterDegreeMiss,
            AimValidationProbeHealthBefore,
            0.1f);
        const bool bQuarterDegreeMissDidNotConsumeRound = MagazineAfterQuarterDegreeMiss == AimValidationProbeMagazineBefore;
        if (!bQuarterDegreeMissDidNotDamage || !bQuarterDegreeMissDidNotConsumeRound)
        {
            UE_LOG(
                LogTemp,
                Error,
            TEXT("Black Meridian aim validation probe: close_miss_result angle_deg=6.5 blocked=%s ammo_consumed=%s health=%.1f->%.1f magazine=%d->%d"),
                bQuarterDegreeMissDidNotDamage ? TEXT("yes") : TEXT("no"),
                bQuarterDegreeMissDidNotConsumeRound ? TEXT("no") : TEXT("yes"),
                AimValidationProbeHealthBefore,
                HealthAfterQuarterDegreeMiss,
                AimValidationProbeMagazineBefore,
                MagazineAfterQuarterDegreeMiss);
            FailProbe(TEXT("quarter_degree_near_miss_shot_damaged_enemy_or_consumed_ammo"));
            return;
        }

        const FVector ViewOrigin = Player->GetFirstPersonCamera()
            ? Player->GetFirstPersonCamera()->GetComponentLocation()
            : Player->GetPawnViewLocation();
        const FVector TargetPoint = Target->GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
        const FVector DirectDirection = (TargetPoint - ViewOrigin).GetSafeNormal();
        Player->RunCombatProbeFire(DirectDirection);
        AimValidationProbeElapsedSeconds = 0.0f;
        AimValidationProbeStage = 5;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian aim validation probe: direct_target_shot health_before=%.1f magazine_before=%d direction=%s"),
            HealthAfterQuarterDegreeMiss,
            MagazineAfterQuarterDegreeMiss,
            *DirectDirection.ToString());
        return;
    }

    if (AimValidationProbeStage == 5)
    {
        if (AimValidationProbeElapsedSeconds < 0.25f)
        {
            return;
        }

        const float HealthAfterDirect = Target->HealthComponent->Health;
        const int32 MagazineAfterDirect = Weapon->GetMagazineAmmo();
        const bool bDirectShotDamaged = HealthAfterDirect < AimValidationProbeHealthBefore - 0.1f;
        const bool bDirectShotConsumedRound = MagazineAfterDirect == AimValidationProbeMagazineBefore - 1;
        const bool bPassed = bDirectShotDamaged && bDirectShotConsumedRound;
        if (!bPassed)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Black Meridian aim validation probe: FAIL direct_damage=%s direct_ammo_consumed=%s health=%.1f->%.1f magazine=%d->%d"),
                bDirectShotDamaged ? TEXT("yes") : TEXT("no"),
                bDirectShotConsumedRound ? TEXT("yes") : TEXT("no"),
                AimValidationProbeHealthBefore,
                HealthAfterDirect,
                AimValidationProbeMagazineBefore,
                MagazineAfterDirect);
            FailProbe(TEXT("direct_on_target_shot_did_not_damage_enemy"));
            return;
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian aim validation probe: direct_result=pass off_target_damage=no off_target_ammo=no close_miss_7.0_damage=no close_miss_7.0_ammo=no close_miss_6.5_damage=no close_miss_6.5_ammo=no direct_damage=yes health=%.1f->%.1f magazine=%d->%d"),
            AimValidationProbeHealthBefore,
            HealthAfterDirect,
            AimValidationProbeMagazineBefore,
            MagazineAfterDirect);

        // Move the same live target laterally while the player keeps the
        // current reticle direction. This exercises the production resolver
        // against a target transform that changed after the first shot, not
        // only against a stationary probe pawn. At 500 cm the 30 cm lateral
        // offset is just outside the mathematical line's 24 cm hitbox but is
        // inside the 8 cm swept tolerance. It is just beyond the old 2.5
        // degree guard and exercises the new distance-aware lateral bound.
        const FVector MovingViewOrigin = Player->GetFirstPersonCamera()
            ? Player->GetFirstPersonCamera()->GetComponentLocation()
            : Player->GetPawnViewLocation();
        const FVector MovingViewDirection = Player->GetAimDirection().GetSafeNormal();
        const FVector MovingRight = FVector::CrossProduct(
            FVector::UpVector,
            MovingViewDirection).GetSafeNormal();
        if (MovingViewDirection.IsNearlyZero() || MovingRight.IsNearlyZero())
        {
            FailProbe(TEXT("moving_target_view_direction_missing"));
            return;
        }

        // Try both lateral sides. The probe target is placed in the authored
        // campaign map, so one side can occasionally intersect a level prop
        // even though the other side is a clear visible near-miss. Select a
        // side only when the swept visibility query reaches this target first;
        // that keeps the regression about the narrow aim tolerance instead of
        // accidentally asserting that the resolver should shoot through cover.
        const FVector MovingOffsets[] = {
            MovingRight * 30.0f,
            MovingRight * -30.0f};
        bool bPlacedMovingTarget = false;
        for (const FVector& MovingOffset : MovingOffsets)
        {
            const FVector CandidateTargetPoint = MovingViewOrigin
                + MovingViewDirection * 500.0f
                + MovingOffset;
            const FVector CandidateActorLocation = CandidateTargetPoint - FVector(0.0f, 0.0f, 48.0f);
            Target->SetActorLocation(
                CandidateActorLocation,
                false,
                nullptr,
                ETeleportType::TeleportPhysics);

            FCollisionQueryParams MovingVisibilityParams(
                FName(TEXT("BlackMeridianAimValidationMovingTarget")),
                true,
                Player);
            MovingVisibilityParams.AddIgnoredActor(Weapon);
            TArray<FHitResult> MovingVisibilityHits;
            const bool bMovingVisibilityHit = GetWorld()->SweepMultiByChannel(
                MovingVisibilityHits,
                MovingViewOrigin,
                CandidateTargetPoint,
                FQuat::Identity,
                ECC_Visibility,
                FCollisionShape::MakeSphere(BMCombatTuning::CombatAimSweepRadius),
                MovingVisibilityParams);
            const FHitResult* FirstMovingVisibilityHit = nullptr;
            for (const FHitResult& Hit : MovingVisibilityHits)
            {
                if (Hit.bBlockingHit)
                {
                    FirstMovingVisibilityHit = &Hit;
                    break;
                }
            }
            if (bMovingVisibilityHit
                && FirstMovingVisibilityHit
                && FirstMovingVisibilityHit->GetActor() == Target
                && FirstMovingVisibilityHit->GetComponent() == Target->GetCombatAimHitbox())
            {
                AimValidationProbeTargetLocation = CandidateActorLocation;
                bPlacedMovingTarget = true;
                break;
            }
        }
        if (!bPlacedMovingTarget)
        {
            FailProbe(TEXT("moving_target_clear_lane_unavailable"));
            return;
        }

        const float MovingCrosshairRange = FMath::Max(
            100.0f,
            Weapon->GetWeaponData()->Balance.RangeMeters * 100.0f);
        FHitResult MovingCrosshairHit;
        ABMCombatCharacter* MovingCrosshairTarget = BMCombatTargeting::ResolveLiveHostileCrosshairTarget(
            GetWorld(),
            Player,
            MovingViewOrigin,
            MovingViewDirection,
            MovingCrosshairRange,
            Weapon,
            &MovingCrosshairHit);
        if (MovingCrosshairTarget != Target)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Black Meridian aim validation probe: moving_target_result resolved=no expected=%s resolved=%s hit_component=%s target_location=%s"),
                *GetNameSafe(Target),
                *GetNameSafe(MovingCrosshairTarget),
                *GetNameSafe(MovingCrosshairHit.GetComponent()),
                *Target->GetActorLocation().ToString());
            FailProbe(TEXT("moving_target_not_under_reticle"));
            return;
        }

        if (!Weapon->SetCombatProbeAmmo(1, 0))
        {
            FailProbe(TEXT("could_not_prepare_moving_target_ammo"));
            return;
        }
        AimValidationProbeHealthBefore = HealthAfterDirect;
        AimValidationProbeMagazineBefore = Weapon->GetMagazineAmmo();
        Player->RunCombatProbeFire(MovingViewDirection);
        AimValidationProbeElapsedSeconds = 0.0f;
        AimValidationProbeStage = 6;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian aim validation probe: moving_target_shot resolved=yes health_before=%.1f magazine_before=%d direction=%s target_location=%s"),
            AimValidationProbeHealthBefore,
            AimValidationProbeMagazineBefore,
            *MovingViewDirection.ToString(),
            *Target->GetActorLocation().ToString());
        return;
    }

    if (AimValidationProbeStage == 6)
    {
        if (AimValidationProbeElapsedSeconds < 0.35f)
        {
            return;
        }

        const float HealthAfterMovingShot = Target->HealthComponent->Health;
        const int32 MagazineAfterMovingShot = Weapon->GetMagazineAmmo();
        const bool bMovingShotDamaged = HealthAfterMovingShot < AimValidationProbeHealthBefore - 0.1f;
        const bool bMovingShotConsumedRound = MagazineAfterMovingShot == AimValidationProbeMagazineBefore - 1;
        if (!bMovingShotDamaged || !bMovingShotConsumedRound)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian aim validation probe: FAIL off_target_damage=no off_target_ammo=no close_miss_7.0_damage=no close_miss_7.0_ammo=no close_miss_6.5_damage=no close_miss_6.5_ammo=no direct_damage=yes moving_target_resolved=yes moving_damage=%s health=%.1f->%.1f magazine=%d->%d"),
                bMovingShotDamaged ? TEXT("yes") : TEXT("no"),
                AimValidationProbeHealthBefore,
                HealthAfterMovingShot,
                AimValidationProbeMagazineBefore,
                MagazineAfterMovingShot);
            FailProbe(TEXT("moving_target_shot_did_not_damage_enemy"));
            return;
        }

        // Aim at the upper torso/head band of the same visible target. The
        // old 72 cm aim capsule stopped below this point even though the
        // Manny silhouette was clearly visible, which made the reticle stay
        // teal and caused the authoritative shot to consume ammo without
        // reducing enemy health.
        const FVector UpperBodyViewOrigin = Player->GetFirstPersonCamera()
            ? Player->GetFirstPersonCamera()->GetComponentLocation()
            : Player->GetPawnViewLocation();
        const FVector UpperBodyAimPoint = Target->GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
        const FVector UpperBodyDirection = (UpperBodyAimPoint - UpperBodyViewOrigin).GetSafeNormal();
        if (UpperBodyDirection.IsNearlyZero())
        {
            FailProbe(TEXT("upper_body_target_direction_missing"));
            return;
        }

        FHitResult UpperBodyCrosshairHit;
        const float UpperBodyCrosshairRange = FMath::Max(
            100.0f,
            Weapon->GetWeaponData()->Balance.RangeMeters * 100.0f);
        ABMCombatCharacter* UpperBodyCrosshairTarget = BMCombatTargeting::ResolveLiveHostileCrosshairTarget(
            GetWorld(),
            Player,
            UpperBodyViewOrigin,
            UpperBodyDirection,
            UpperBodyCrosshairRange,
            Weapon,
            &UpperBodyCrosshairHit);
        if (UpperBodyCrosshairTarget != Target)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Black Meridian aim validation probe: upper_body_result resolved=no expected=%s resolved=%s hit_component=%s aim_point=%s target_location=%s"),
                *GetNameSafe(Target),
                *GetNameSafe(UpperBodyCrosshairTarget),
                *GetNameSafe(UpperBodyCrosshairHit.GetComponent()),
                *UpperBodyAimPoint.ToString(),
                *Target->GetActorLocation().ToString());
            FailProbe(TEXT("upper_body_not_under_reticle"));
            return;
        }

        if (Target->HealthComponent)
        {
            Target->HealthComponent->SetHealthForCheckpoint(100.0f);
        }
        if (!Weapon->SetCombatProbeAmmo(1, 0))
        {
            FailProbe(TEXT("could_not_prepare_upper_body_ammo"));
            return;
        }
        AimValidationProbeHealthBefore = Target->HealthComponent->Health;
        AimValidationProbeMagazineBefore = Weapon->GetMagazineAmmo();
        Player->RunCombatProbeFire(UpperBodyDirection);
        AimValidationProbeElapsedSeconds = 0.0f;
        AimValidationProbeStage = 7;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian aim validation probe: upper_body_shot resolved=yes aim_height_cm=80.0 health_before=%.1f magazine_before=%d direction=%s"),
            AimValidationProbeHealthBefore,
            AimValidationProbeMagazineBefore,
            *UpperBodyDirection.ToString());
        return;
    }

    if (AimValidationProbeStage == 7)
    {
        if (AimValidationProbeElapsedSeconds < 0.35f)
        {
            return;
        }

        const float HealthAfterUpperBodyShot = Target->HealthComponent->Health;
        const int32 MagazineAfterUpperBodyShot = Weapon->GetMagazineAmmo();
        const bool bUpperBodyShotDamaged = HealthAfterUpperBodyShot < AimValidationProbeHealthBefore - 0.1f;
        const bool bUpperBodyShotConsumedRound = MagazineAfterUpperBodyShot == AimValidationProbeMagazineBefore - 1;
        const bool bPassed = bUpperBodyShotDamaged && bUpperBodyShotConsumedRound;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian aim validation probe: %s off_target_damage=no off_target_ammo=no close_miss_7.0_damage=no close_miss_7.0_ammo=no close_miss_6.5_damage=no close_miss_6.5_ammo=no direct_damage=yes moving_target_resolved=yes moving_damage=yes upper_body_resolved=yes upper_body_damage=%s health=%.1f->%.1f magazine=%d->%d"),
            bPassed ? TEXT("PASS") : TEXT("FAIL"),
            bUpperBodyShotDamaged ? TEXT("yes") : TEXT("no"),
            AimValidationProbeHealthBefore,
            HealthAfterUpperBodyShot,
            AimValidationProbeMagazineBefore,
            MagazineAfterUpperBodyShot);
        if (!bPassed)
        {
            FailProbe(TEXT("upper_body_shot_did_not_damage_enemy"));
            return;
        }

        Target->Destroy();
        AimValidationProbeTarget.Reset();
        SetActorTickEnabled(false);
        FPlatformMisc::RequestExit(false);
    }
}

void ABMGameMode::RunFullCampaignProbe(const float DeltaSeconds)
{
    if (!GetWorld() || !MissionDirector || bFullCampaignProbeFailed)
    {
        return;
    }

    ABMGameState* State = GetGameState<ABMGameState>();
    ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (!State || !Player || !Player->GetController())
    {
        return;
    }

    // Auto-start still creates the ordinary mission menu before PostLogin
    // releases it. Keep this verifier from being held behind that pause if a
    // standalone launch delivers the callbacks in a different order.
    if (APlayerController* Controller = Cast<APlayerController>(Player->GetController()); Controller && Controller->IsPaused())
    {
        Controller->SetPause(false);
    }

    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
    const float WorldTime = GetWorld()->GetTimeSeconds();

    const auto FailProbe = [this](const TCHAR* Reason)
    {
        if (bFullCampaignProbeFailed)
        {
            return;
        }
        bFullCampaignProbeFailed = true;
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Black Meridian full campaign probe: FAIL level=%d reason=%s stage=%d segment=%s"),
            static_cast<int32>(CampaignLevel),
            Reason ? Reason : TEXT("unknown"),
            FullCampaignProbeStage,
            *FullCampaignProbeSegmentId.ToString());
        SetActorTickEnabled(false);
        FPlatformMisc::RequestExit(false);
    };

    if (Player->IsEliminated() || (Player->HealthComponent && Player->HealthComponent->Health <= 0.0f))
    {
        FailProbe(TEXT("player_eliminated"));
        return;
    }

    if (!bFullCampaignProbeStarted)
    {
        if (State->MissionPhase == EBMMissionPhase::Intro || !Player->GetCurrentWeapon())
        {
            return;
        }

        if (ABMWeapon* Weapon = Player->GetCurrentWeapon())
        {
            // The verifier keeps the ordinary weapon type and authoritative
            // fire path, but gives each isolated level enough reserve to
            // prove the full encounter rather than failing on a persistent
            // profile's leftover magazine.
            const int32 MagazineCapacity = Weapon->GetWeaponData()
                ? Weapon->GetWeaponData()->Balance.MagazineSize
                : Weapon->GetMagazineAmmo();
            if (!Weapon->SetCombatProbeAmmo(FMath::Max(1, MagazineCapacity), 1000))
            {
                FailProbe(TEXT("could_not_prepare_player_ammo"));
                return;
            }
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian full campaign probe: start level=%s phase=%d player=%s weapon=%s magazine=%d reserve=%d"),
                *Level.LevelId.ToString(),
                static_cast<int32>(State->MissionPhase),
                *Player->GetName(),
                *Weapon->GetName(),
                Weapon->GetMagazineAmmo(),
                Weapon->GetReserveAmmo());
        }

        bFullCampaignProbeStarted = true;
        FullCampaignProbeStage = 1;
    }

    const auto BeginTraversal = [this, Player](
        const FName SegmentId,
        const FVector& Target,
        const int32 NextStage)
    {
        FullCampaignProbeSegmentId = SegmentId;
        FullCampaignProbeSegmentTarget = Target;
        FullCampaignProbeSegmentStart = Player->GetActorLocation();
        FullCampaignProbeLastLocation = FullCampaignProbeSegmentStart;
        FullCampaignProbeSegmentDistance = 0.0f;
        FullCampaignProbeSegmentElapsed = 0.0f;
        FullCampaignProbeLastProgressLog = 0.0f;
        FullCampaignProbeTraversalNextStage = NextStage;
        bFullCampaignProbeTraversalActive = true;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian full campaign probe: traversal start segment=%s from=%s target=%s"),
            *SegmentId.ToString(),
            *FullCampaignProbeSegmentStart.ToString(),
            *Target.ToString());
    };

    const auto TickTraversal = [this, Player, DeltaSeconds, &FailProbe]() -> bool
    {
        if (!bFullCampaignProbeTraversalActive)
        {
            return true;
        }

        FullCampaignProbeSegmentElapsed += FMath::Max(0.0f, DeltaSeconds);
        const FVector CurrentLocation = Player->GetActorLocation();
        FullCampaignProbeSegmentDistance += FVector::Dist2D(CurrentLocation, FullCampaignProbeLastLocation);
        FullCampaignProbeLastLocation = CurrentLocation;

        FVector Offset = FullCampaignProbeSegmentTarget - CurrentLocation;
        Offset.Z = 0.0f;
        const float RemainingDistance = Offset.Size2D();
        // Intelligence is an optional proximity-gated phase in the live
        // mission. Stop the verifier before that gate (and before the
        // console's 360 cm trace limit) so the required console hold gets a
        // chance to start through the real interaction path.
        const float ArrivalRadius = FullCampaignProbeTraversalNextStage == 9 ? 340.0f : 260.0f;
        if (RemainingDistance <= ArrivalRadius)
        {
            if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
            {
                Movement->StopMovementImmediately();
            }

            const float Displacement = FVector::Dist2D(CurrentLocation, FullCampaignProbeSegmentStart);
            const float InitialDistance = FVector::Dist2D(FullCampaignProbeSegmentTarget, FullCampaignProbeSegmentStart);
            // Combat repositioning can leave the player inside a short final
            // approach. If the traversal starts inside the interaction radius,
            // the approach is already satisfied by the real player transform;
            // the subsequent boat hold and final-defense checks remain
            // mandatory. Outside the radius, keep requiring measured
            // CharacterMovement progress instead of accepting a zero-distance
            // probe result.
            const bool bInsideInteractionRadiusStart = InitialDistance <= ArrivalRadius;
            // The player must still make measurable progress when starting
            // outside the interaction radius, but the available approach can
            // be shorter than 100 cm.  Requiring a fixed 100 cm here made a
            // valid 389 cm Wreckline approach impossible: the 340 cm arrival
            // radius left only 49 cm of legal movement before completion.
            const float AvailableApproachDistance = FMath::Max(0.0f, InitialDistance - ArrivalRadius);
            const bool bShortApproach = !bInsideInteractionRadiusStart && AvailableApproachDistance < 100.0f;
            const float RequiredMovement = bInsideInteractionRadiusStart
                ? 0.0f
                : FMath::Min(100.0f, AvailableApproachDistance);
            const bool bMoved = bInsideInteractionRadiusStart
                || (Displacement >= RequiredMovement && FullCampaignProbeSegmentDistance >= RequiredMovement);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian full campaign probe: traversal complete segment=%s reached=%s displacement_cm=%.1f path_distance_cm=%.1f required_movement_cm=%.1f initial_distance_cm=%.1f arrival_radius_cm=%.1f inside_radius_start=%s short_approach=%s elapsed=%.2f final=%s"),
                *FullCampaignProbeSegmentId.ToString(),
                bMoved ? TEXT("yes") : TEXT("no"),
                Displacement,
                FullCampaignProbeSegmentDistance,
                RequiredMovement,
                InitialDistance,
                ArrivalRadius,
                bInsideInteractionRadiusStart ? TEXT("yes") : TEXT("no"),
                bShortApproach ? TEXT("yes") : TEXT("no"),
                FullCampaignProbeSegmentElapsed,
                *CurrentLocation.ToString());
            if (!bMoved)
            {
                FailProbe(TEXT("traversal_no_measured_displacement"));
                return false;
            }

            bFullCampaignProbeTraversalActive = false;
            FullCampaignProbeStage = FullCampaignProbeTraversalNextStage;
            return true;
        }

        if (FullCampaignProbeSegmentElapsed > 35.0f)
        {
            FailProbe(TEXT("traversal_timeout_or_blocked"));
            return false;
        }

        // Follow the runtime nav path when a structural cover piece sits on
        // the direct line to the next mission anchor. The probe still uses
        // the player's ordinary AddMovementInput path, but no longer treats
        // a legitimate wall/cover corner as an invalid campaign. A real
        // player can take the same readable side route manually.
        FVector SteeringTarget = FullCampaignProbeSegmentTarget;
        int32 NavigationPointCount = 0;
        if (UNavigationPath* NavigationPath = UNavigationSystemV1::FindPathToLocationSynchronously(
                this,
                CurrentLocation,
                FullCampaignProbeSegmentTarget,
                Player))
        {
            NavigationPointCount = NavigationPath->PathPoints.Num();
            for (int32 PointIndex = 1; PointIndex < NavigationPath->PathPoints.Num(); ++PointIndex)
            {
                if (FVector::Dist2D(CurrentLocation, NavigationPath->PathPoints[PointIndex]) > 90.0f
                    || PointIndex == NavigationPath->PathPoints.Num() - 1)
                {
                    SteeringTarget = NavigationPath->PathPoints[PointIndex];
                    break;
                }
            }
        }

        FVector SteeringOffset = SteeringTarget - CurrentLocation;
        SteeringOffset.Z = 0.0f;
        const FVector Direction = SteeringOffset.GetSafeNormal2D();
        if (Direction.IsNearlyZero())
        {
            FailProbe(TEXT("traversal_zero_direction"));
            return false;
        }

        if (AController* Controller = Player->GetController())
        {
            Controller->SetControlRotation(Direction.Rotation());
        }
        // This is the actual character movement input path. No actor
        // transform is written here; collision, acceleration, stairs, and
        // the perimeter guard remain authoritative.
        Player->AddMovementInput(Direction, 1.0f);

        if (FullCampaignProbeSegmentElapsed - FullCampaignProbeLastProgressLog >= 2.0f)
        {
            FullCampaignProbeLastProgressLog = FullCampaignProbeSegmentElapsed;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian full campaign probe: traversal progress segment=%s remaining_cm=%.1f displacement_cm=%.1f path_distance_cm=%.1f player=%s"),
                *FullCampaignProbeSegmentId.ToString(),
                RemainingDistance,
                FVector::Dist2D(CurrentLocation, FullCampaignProbeSegmentStart),
                FullCampaignProbeSegmentDistance,
                *CurrentLocation.ToString());
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian full campaign probe: navigation steering segment=%s points=%d steering_target=%s"),
                *FullCampaignProbeSegmentId.ToString(),
                NavigationPointCount,
                *SteeringTarget.ToString());
        }
        return true;
    };

    const auto FindInteraction = [this](const FName InteractionId) -> ABMInteractableActor*
    {
        for (TActorIterator<ABMInteractableActor> Iterator(GetWorld()); Iterator; ++Iterator)
        {
            ABMInteractableActor* Candidate = *Iterator;
            if (IsValid(Candidate) && Candidate->InteractionId == InteractionId)
            {
                return Candidate;
            }
        }
        return nullptr;
    };

    const auto BeginInteraction = [this, Player, WorldTime, &FindInteraction, &FailProbe](const FName InteractionId) -> bool
    {
        if (!bFullCampaignProbeInteractionStarted)
        {
            ABMInteractableActor* Target = FindInteraction(InteractionId);
            if (!Target)
            {
                FailProbe(TEXT("objective_actor_missing"));
                return false;
            }
            const float Distance = FVector::Dist2D(Player->GetActorLocation(), Target->GetActorLocation());
            // The normal interaction trace reaches 360 cm from the camera;
            // keep the console's verifier approach just inside that physical
            // limit while retaining the tighter 320 cm gate for the other
            // mission interactions.
            const float MaxInteractionDistance = InteractionId == TEXT("IntelConsole") ? 350.0f : 320.0f;
            if (Distance > MaxInteractionDistance)
            {
                FailProbe(TEXT("objective_not_reached_by_physical_traversal"));
                return false;
            }

            const bool bStarted = Player->RunInteractionProbeHold(Target);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian full campaign probe: objective hold start id=%s started=%s distance_cm=%.1f player=%s target=%s"),
                *InteractionId.ToString(),
                bStarted ? TEXT("yes") : TEXT("no"),
                Distance,
                *Player->GetActorLocation().ToString(),
                *Target->GetActorLocation().ToString());
            if (!bStarted)
            {
                FailProbe(TEXT("objective_target_trace_or_hold_start_failed"));
                return false;
            }

            FullCampaignProbeInteractionId = InteractionId;
            FullCampaignProbeInteraction = Target;
            FullCampaignProbeInteractionStartedAt = WorldTime;
            bFullCampaignProbeInteractionStarted = true;
            // The intelligence console can begin its physical hold at the
            // interaction trace limit (350 cm), while traversal completion
            // intentionally uses a tighter 340 cm arrival radius. Retire
            // that approach as soon as the real hold starts; otherwise the
            // next mission phase inherits the stale console target and keeps
            // steering the player into the completed leg.
            bFullCampaignProbeTraversalActive = false;
            if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
            {
                Movement->StopMovementImmediately();
            }
        }
        return true;
    };

    const auto ResetInteraction = [this]()
    {
        bFullCampaignProbeInteractionStarted = false;
        FullCampaignProbeInteraction.Reset();
        FullCampaignProbeInteractionId = NAME_None;
        FullCampaignProbeInteractionStartedAt = -1.0f;
    };

    const auto TickInteraction = [this, Player, State, WorldTime, &FailProbe, &ResetInteraction](
        const EBMMissionPhase ExpectedPhase,
        const EBMMissionPhase CompletedPhase,
        const int32 NextStage) -> bool
    {
        if (State->MissionPhase == CompletedPhase)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian full campaign probe: objective hold complete id=%s phase=%d elapsed=%.2f"),
                *FullCampaignProbeInteractionId.ToString(),
                static_cast<int32>(CompletedPhase),
                FullCampaignProbeInteractionStartedAt >= 0.0f ? WorldTime - FullCampaignProbeInteractionStartedAt : 0.0f);
            ResetInteraction();
            FullCampaignProbeStage = NextStage;
            return true;
        }

        if (State->MissionPhase != ExpectedPhase)
        {
            FailProbe(TEXT("objective_phase_changed_unexpectedly"));
            return false;
        }
        if (FullCampaignProbeInteractionStartedAt >= 0.0f && WorldTime - FullCampaignProbeInteractionStartedAt > 8.0f)
        {
            FailProbe(TEXT("objective_hold_timeout"));
            return false;
        }
        if (!Player->IsHoldingInteraction() && bFullCampaignProbeInteractionStarted)
        {
            FailProbe(TEXT("objective_hold_cancelled"));
            return false;
        }
        return true;
    };

    const auto TickCombat = [this, Player, State, WorldTime, DeltaSeconds, &FailProbe](
        const int32 ExpectedWave,
        const TCHAR* EncounterLabel,
        const int32 NextStage) -> bool
    {
        if (!bFullCampaignProbeCombatStarted)
        {
            bFullCampaignProbeCombatStarted = true;
            FullCampaignProbeCombatExpected = ExpectedWave;
            FullCampaignProbeCombatAcceptedShots = 0;
            FullCampaignProbeCombatDamageEvents = 0;
            FullCampaignProbeCombatWeaponCount = 0;
            FullCampaignProbeLastShotAt = -100.0f;
            FullCampaignProbeReloadStartedAt = -1.0f;
            FullCampaignProbeSegmentElapsed = 0.0f;
            FullCampaignProbeCombatTarget.Reset();
            FullCampaignProbeCombatNoDamageShots = 0;
            bFullCampaignProbeCombatRepositionActive = false;
            FullCampaignProbeCombatRepositionDirection = FVector::ZeroVector;
            FullCampaignProbeCombatLastProgressLog = -2.0f;
            FullCampaignProbeWaveActors.Reset();
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian full campaign probe: combat start encounter=%s expected_wave=%d phase=%d player=%s"),
                EncounterLabel ? EncounterLabel : TEXT("unknown"),
                ExpectedWave,
                static_cast<int32>(State->MissionPhase),
                *Player->GetActorLocation().ToString());
        }

        FullCampaignProbeSegmentElapsed += FMath::Max(0.0f, DeltaSeconds);
        // Keep a selected physical leg pressed between shot requests. A
        // single AddMovementInput call at the weapon fire cadence is too
        // sparse for CharacterMovement acceleration and can look like a
        // shuffle against cover even when the capsule leg is clear.
        if (bFullCampaignProbeCombatRepositionActive &&
            FullCampaignProbeCombatNoDamageShots >= 3 &&
            !FullCampaignProbeCombatRepositionDirection.IsNearlyZero())
        {
            Player->AddMovementInput(FullCampaignProbeCombatRepositionDirection, 1.0f, true);
        }
        for (TActorIterator<ABMEnemyCharacter> Iterator(GetWorld()); Iterator; ++Iterator)
        {
            ABMEnemyCharacter* Enemy = *Iterator;
            const bool bAlreadyTracked = FullCampaignProbeWaveActors.ContainsByPredicate(
                [Enemy](const TWeakObjectPtr<ABMEnemyCharacter>& Existing)
                {
                    return Existing.Get() == Enemy;
                });
            if (IsValid(Enemy) && !Enemy->IsEliminated() && !bAlreadyTracked)
            {
                FullCampaignProbeWaveActors.Add(Enemy);
            }
        }

        if (FullCampaignProbeWaveActors.Num() < ExpectedWave)
        {
            if (FullCampaignProbeSegmentElapsed > 12.0f)
            {
                FailProbe(TEXT("combat_wave_spawn_missing"));
                return false;
            }
            return true;
        }

        int32 LiveWaveCount = 0;
        int32 WeaponCount = 0;
        for (const TWeakObjectPtr<ABMEnemyCharacter>& EnemyPtr : FullCampaignProbeWaveActors)
        {
            if (ABMEnemyCharacter* Enemy = EnemyPtr.Get(); Enemy && !Enemy->IsEliminated())
            {
                ++LiveWaveCount;
                if (Enemy->GetCombatWeapon())
                {
                    ++WeaponCount;
                }
            }
        }
        FullCampaignProbeCombatWeaponCount = FMath::Max(FullCampaignProbeCombatWeaponCount, WeaponCount);
        if (WeaponCount < LiveWaveCount)
        {
            if (FullCampaignProbeSegmentElapsed > 12.0f)
            {
                FailProbe(TEXT("combat_wave_weapon_missing"));
                return false;
            }
            return true;
        }

        // Dense layouts can legitimately require several magazine cycles and
        // a real orbit around solid cover. Keep the failure guard finite, but
        // give the ordinary movement, reload, and damage paths enough time
        // to finish the largest generated encounter.
        if (FullCampaignProbeSegmentElapsed > 90.0f)
        {
            FailProbe(TEXT("combat_timeout_or_no_damage"));
            return false;
        }

        bool bAllEliminated = true;
        ABMEnemyCharacter* NearestTarget = nullptr;
        float NearestDistanceSquared = TNumericLimits<float>::Max();
        ABMEnemyCharacter* NearestVisibleTarget = nullptr;
        float NearestVisibleDistanceSquared = TNumericLimits<float>::Max();
        const FVector ProbeOrigin = Player->GetFirstPersonCamera()
            ? Player->GetFirstPersonCamera()->GetComponentLocation()
            : Player->GetPawnViewLocation();
        FCollisionQueryParams ProbeVisibilityParams(
            SCENE_QUERY_STAT(BlackMeridianFullCampaignProbeVisibility),
            true,
            Player);
        // Match the player HUD/weapon sightline contract. Runtime layout
        // proxies intentionally remain simple and block Pawn only; using
        // simple visibility here would make an open frame look like a solid
        // wall to the verifier and to the combat decision it drives.
        ProbeVisibilityParams.bTraceComplex = true;
        for (const TWeakObjectPtr<ABMEnemyCharacter>& EnemyPtr : FullCampaignProbeWaveActors)
        {
            ABMEnemyCharacter* Enemy = EnemyPtr.Get();
            if (!Enemy || Enemy->IsEliminated())
            {
                continue;
            }
            bAllEliminated = false;
            const float DistanceSquared = FVector::DistSquared2D(Player->GetActorLocation(), Enemy->GetActorLocation());
            if (DistanceSquared < NearestDistanceSquared)
            {
                NearestDistanceSquared = DistanceSquared;
                NearestTarget = Enemy;
            }

            // Physical layouts are allowed to put one enemy behind cover, so
            // nearest-by-distance is not a valid firing target by itself.
            // Select the nearest enemy whose body is actually visible from
            // the player's camera, while still using the ordinary weapon fire
            // path below. This keeps the verifier honest about collision and
            // sightlines without granting wall penetration or synthetic hits.
            const FVector AimPoint = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 58.0f);
            FHitResult VisibilityHit;
            if (GetWorld()->LineTraceSingleByChannel(
                    VisibilityHit,
                    ProbeOrigin,
                    AimPoint,
                    ECC_Visibility,
                    ProbeVisibilityParams)
                && VisibilityHit.GetActor() == Enemy
                && DistanceSquared < NearestVisibleDistanceSquared)
            {
                NearestVisibleDistanceSquared = DistanceSquared;
                NearestVisibleTarget = Enemy;
            }
        }

        // Keep the current live target sticky until it is eliminated. The
        // earlier nearest-visible selection could alternate between two
        // enemies whenever cover moved across a sightline; that reset the
        // no-damage counter on every shot and prevented the real capsule
        // reposition path from ever getting three consecutive samples to
        // clear a wall. Holding the target makes the verifier move around
        // physical cover instead of treating target churn as progress.
        if (ABMEnemyCharacter* TrackedTarget = FullCampaignProbeCombatTarget.Get();
            TrackedTarget && !TrackedTarget->IsEliminated())
        {
            NearestTarget = TrackedTarget;
            NearestDistanceSquared = FVector::DistSquared2D(
                Player->GetActorLocation(),
                TrackedTarget->GetActorLocation());
        }
        else if (NearestVisibleTarget)
        {
            NearestTarget = NearestVisibleTarget;
            NearestDistanceSquared = NearestVisibleDistanceSquared;
        }

        if (FullCampaignProbeSegmentElapsed - FullCampaignProbeCombatLastProgressLog >= 2.0f)
        {
            FullCampaignProbeCombatLastProgressLog = FullCampaignProbeSegmentElapsed;
            int32 LiveCount = 0;
            for (const TWeakObjectPtr<ABMEnemyCharacter>& EnemyPtr : FullCampaignProbeWaveActors)
            {
                if (const ABMEnemyCharacter* Enemy = EnemyPtr.Get(); Enemy && !Enemy->IsEliminated())
                {
                    ++LiveCount;
                }
            }
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian full campaign probe: combat progress encounter=%s live=%d spawned=%d weapons=%d magazine=%d reserve=%d reloading=%s elapsed=%.2f target=%s target_health=%.1f player_health=%.1f"),
                EncounterLabel ? EncounterLabel : TEXT("unknown"),
                LiveCount,
                FullCampaignProbeWaveActors.Num(),
                WeaponCount,
                Player->GetCurrentWeapon() ? Player->GetCurrentWeapon()->GetMagazineAmmo() : -1,
                Player->GetCurrentWeapon() ? Player->GetCurrentWeapon()->GetReserveAmmo() : -1,
                Player->GetCurrentWeapon() && Player->GetCurrentWeapon()->IsReloading() ? TEXT("yes") : TEXT("no"),
                FullCampaignProbeSegmentElapsed,
                NearestTarget ? *NearestTarget->GetName() : TEXT("none"),
                NearestTarget && NearestTarget->HealthComponent ? NearestTarget->HealthComponent->Health : -1.0f,
                Player->HealthComponent ? Player->HealthComponent->Health : -1.0f);
        }

        if (bAllEliminated)
        {
            const bool bCombatUsedPlayerFire = FullCampaignProbeCombatAcceptedShots > 0;
            const bool bCombatAppliedDamage = FullCampaignProbeCombatDamageEvents > 0;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian full campaign probe: combat complete encounter=%s spawned=%d weapons=%d player_shots=%d damage_events=%d eliminations=%d"),
                EncounterLabel ? EncounterLabel : TEXT("unknown"),
                FullCampaignProbeWaveActors.Num(),
                FullCampaignProbeCombatWeaponCount,
                FullCampaignProbeCombatAcceptedShots,
                FullCampaignProbeCombatDamageEvents,
                FullCampaignProbeWaveActors.Num());
            if (!bCombatUsedPlayerFire || !bCombatAppliedDamage)
            {
                FailProbe(TEXT("combat_completed_without_player_damage_path"));
                return false;
            }
            bFullCampaignProbeCombatStarted = false;
            FullCampaignProbeWaveActors.Reset();
            FullCampaignProbeStage = NextStage;
            return true;
        }

        ABMWeapon* Weapon = Player->GetCurrentWeapon();
        if (!Weapon || !Weapon->GetWeaponData())
        {
            FailProbe(TEXT("player_weapon_missing_during_combat"));
            return false;
        }
        if (Weapon->IsReloading())
        {
            if (FullCampaignProbeReloadStartedAt < 0.0f)
            {
                FullCampaignProbeReloadStartedAt = WorldTime;
            }
            else if (WorldTime - FullCampaignProbeReloadStartedAt > 8.0f)
            {
                FailProbe(TEXT("combat_reload_stuck"));
                return false;
            }
            return true;
        }
        if (FullCampaignProbeReloadStartedAt >= 0.0f)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian full campaign probe: reload complete elapsed=%.2f magazine=%d reserve=%d"),
                WorldTime - FullCampaignProbeReloadStartedAt,
                Weapon->GetMagazineAmmo(),
                Weapon->GetReserveAmmo());
            FullCampaignProbeReloadStartedAt = -1.0f;
        }
        if (Weapon->GetMagazineAmmo() <= 0)
        {
            if (Weapon->GetReserveAmmo() <= 0)
            {
                FailProbe(TEXT("player_ammo_exhausted"));
                return false;
            }
            Player->RunCombatProbeReload();
            FullCampaignProbeReloadStartedAt = WorldTime;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian full campaign probe: reload start magazine=%d reserve=%d"),
                Weapon->GetMagazineAmmo(),
                Weapon->GetReserveAmmo());
            return true;
        }

        const float FireInterval = 60.0f / FMath::Max(1.0f, Weapon->GetWeaponData()->Balance.FireRateRPM);
        if (!NearestTarget || WorldTime - FullCampaignProbeLastShotAt < FMath::Max(0.09f, FireInterval * 1.05f))
        {
            return true;
        }

        const float HealthBefore = NearestTarget->HealthComponent ? NearestTarget->HealthComponent->Health : 100.0f;
        const int32 MagazineBefore = Weapon->GetMagazineAmmo();
        const FVector Origin = Player->GetFirstPersonCamera()
            ? Player->GetFirstPersonCamera()->GetComponentLocation()
            : Player->GetPawnViewLocation();
        const FVector Direction = (NearestTarget->GetActorLocation() + FVector(0.0f, 0.0f, 58.0f) - Origin).GetSafeNormal();
        Player->RunCombatProbeFire(Direction);
        FullCampaignProbeLastShotAt = WorldTime;
        if (Weapon->GetMagazineAmmo() < MagazineBefore)
        {
            ++FullCampaignProbeCombatAcceptedShots;
        }
        const float HealthAfter = NearestTarget->HealthComponent ? NearestTarget->HealthComponent->Health : HealthBefore;
        const bool bDamageApplied = HealthAfter < HealthBefore || NearestTarget->IsEliminated();
        if (bDamageApplied)
        {
            ++FullCampaignProbeCombatDamageEvents;
            FullCampaignProbeCombatTarget = NearestTarget;
            FullCampaignProbeCombatNoDamageShots = 0;
            bFullCampaignProbeCombatRepositionActive = false;
            FullCampaignProbeCombatRepositionDirection = FVector::ZeroVector;
        }
        else if (FullCampaignProbeCombatTarget.Get() == NearestTarget)
        {
            ++FullCampaignProbeCombatNoDamageShots;
        }
        else
        {
            FullCampaignProbeCombatTarget = NearestTarget;
            FullCampaignProbeCombatNoDamageShots = 1;
            bFullCampaignProbeCombatRepositionActive = false;
            FullCampaignProbeCombatRepositionDirection = FVector::ZeroVector;
        }

        // A live enemy can legitimately be behind a physical prop after the
        // earlier targets fall. Reposition through the same movement/collision
        // path a player uses instead of firing forever at the blocked ray. A
        // direct approach is not sufficient here: the same cover that blocks
        // the shot can also block the pawn's capsule. Search a small set of
        // physically reachable orbit directions and hold the selected leg for
        // several shots so the pawn actually clears the obstruction instead
        // of oscillating against it.
        if (!bDamageApplied && FullCampaignProbeCombatNoDamageShots >= 3)
        {
            const FVector ToTarget = (NearestTarget->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
            const float TargetDistance = FVector::Dist2D(Player->GetActorLocation(), NearestTarget->GetActorLocation());
            if (ToTarget.IsNearlyZero())
            {
                FailProbe(TEXT("combat_reposition_zero_direction"));
                return false;
            }

            const FVector TargetAimPoint = NearestTarget->GetActorLocation() + FVector(0.0f, 0.0f, 58.0f);
            const auto HasTargetVisibilityFrom = [this, &ProbeVisibilityParams, NearestTarget, &TargetAimPoint](const FVector& CandidateOrigin)
            {
                FHitResult CandidateHit;
                const bool bTraceHit = GetWorld()->LineTraceSingleByChannel(
                           CandidateHit,
                           CandidateOrigin,
                           TargetAimPoint,
                           ECC_Visibility,
                           ProbeVisibilityParams);
                const bool bTargetVisible = bTraceHit && CandidateHit.GetActor() == NearestTarget;
                if (!bTargetVisible)
                {
                    UE_LOG(
                        LogTemp,
                        Display,
                        TEXT("Black Meridian full campaign probe: visibility blocked candidate=%s target=%s hit=%s actor=%s component=%s impact=%s normal=%s"),
                        *CandidateOrigin.ToString(),
                        *GetNameSafe(NearestTarget),
                        bTraceHit ? TEXT("yes") : TEXT("no"),
                        *GetNameSafe(CandidateHit.GetActor()),
                        *GetNameSafe(CandidateHit.GetComponent()),
                        *CandidateHit.ImpactPoint.ToString(),
                        *CandidateHit.ImpactNormal.ToString());
                }
                return bTargetVisible;
            };
            const auto HasClearMovementLeg = [this, Player](const FVector& Direction, const float Distance)
            {
                if (!GetWorld() || !Player || !Player->GetCapsuleComponent())
                {
                    return false;
                }

                // A line trace is not enough to validate a movement leg: a
                // rifle-sized ray can pass a prop while the player capsule
                // still collides with it. Use the same capsule shape that
                // CharacterMovement sweeps through the authored battlefield.
                FCollisionQueryParams MovementParams(
                    SCENE_QUERY_STAT(BlackMeridianFullCampaignProbeMovement),
                    false,
                    Player);
                for (const TWeakObjectPtr<ABMEnemyCharacter>& EnemyPtr : FullCampaignProbeWaveActors)
                {
                    if (ABMEnemyCharacter* Enemy = EnemyPtr.Get())
                    {
                        MovementParams.AddIgnoredActor(Enemy);
                    }
                }

                FHitResult MovementHit;
                const FVector Start = Player->GetActorLocation();
                const FVector End = Start + Direction * Distance;
                const FCollisionShape PlayerCapsule = FCollisionShape::MakeCapsule(
                    Player->GetCapsuleComponent()->GetScaledCapsuleRadius(),
                    Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
                const bool bBlocked = GetWorld()->SweepSingleByChannel(
                    MovementHit,
                    Start,
                    End,
                    FQuat::Identity,
                    ECC_Pawn,
                    PlayerCapsule,
                    MovementParams);
                if (bBlocked)
                {
                    UE_LOG(
                        LogTemp,
                        Display,
                        TEXT("Black Meridian full campaign probe: movement leg blocked actor=%s component=%s start=%s end=%s impact=%s normal=%s"),
                        *GetNameSafe(MovementHit.GetActor()),
                        *GetNameSafe(MovementHit.GetComponent()),
                        *Start.ToString(),
                        *End.ToString(),
                        *MovementHit.ImpactPoint.ToString(),
                        *MovementHit.ImpactNormal.ToString());
                }
                return !bBlocked;
            };

            const int32 RepositionLeg = (FullCampaignProbeCombatNoDamageShots - 3) / 6;
            const float PreferredSign = RepositionLeg % 2 == 0 ? 1.0f : -1.0f;
            const float OrbitAngle = TargetDistance <= 360.0f ? 62.0f : 55.0f;
            const FVector PreferredOrbit = ToTarget.RotateAngleAxis(PreferredSign * OrbitAngle, FVector::UpVector);
            const FVector AlternateOrbit = ToTarget.RotateAngleAxis(-PreferredSign * OrbitAngle, FVector::UpVector);
            const FVector PreferredStrafe = ToTarget.RotateAngleAxis(PreferredSign * 90.0f, FVector::UpVector);
            const FVector AlternateStrafe = ToTarget.RotateAngleAxis(-PreferredSign * 90.0f, FVector::UpVector);
            const FVector Retreat = -ToTarget;
            TArray<FVector> CandidateDirections = {
                PreferredOrbit,
                AlternateOrbit,
                PreferredStrafe,
                AlternateStrafe,
                Retreat};
            // Cover can make the preferred orbit tangent to a wall. Search
            // the full horizontal circle as well so a physical route around
            // the obstruction remains available in every generated layout.
            const float BaseYaw = ToTarget.Rotation().Yaw;
            for (int32 DirectionIndex = 0; DirectionIndex < 16; ++DirectionIndex)
            {
                const float CandidateYaw = BaseYaw + (360.0f / 16.0f) * static_cast<float>(DirectionIndex);
                CandidateDirections.Add(FRotator(0.0f, CandidateYaw, 0.0f).Vector().GetSafeNormal2D());
            }

            FVector RepositionDirection = PreferredOrbit;
            bool bFoundSightlineLeg = false;
            bool bFoundClearLeg = false;
            int32 BlockedLegCount = 0;
            for (const FVector& CandidateDirection : CandidateDirections)
            {
                const FVector CandidateOrigin = ProbeOrigin + CandidateDirection * 220.0f;
                if (!HasClearMovementLeg(CandidateDirection, 180.0f))
                {
                    ++BlockedLegCount;
                    continue;
                }
                if (!bFoundClearLeg)
                {
                    RepositionDirection = CandidateDirection;
                    bFoundClearLeg = true;
                }
                if (HasTargetVisibilityFrom(CandidateOrigin))
                {
                    RepositionDirection = CandidateDirection;
                    bFoundSightlineLeg = true;
                    break;
                }
            }
            const FVector PendingInputBefore = Player->GetPendingMovementInputVector();
            const bool bMoveInputIgnoredBefore = Player->IsMoveInputIgnored();
            if (AController* Controller = Player->GetController())
            {
                Controller->SetControlRotation(RepositionDirection.Rotation());
            }
            // This verifier is dispatched from the authoritative GameMode,
            // not from the interactive input component. Force only this
            // development input through any transient controller/menu ignore
            // counter; CharacterMovement still owns acceleration, floor
            // checks, capsule sweeps, and the resulting displacement.
            Player->AddMovementInput(RepositionDirection, 0.85f, true);
            bFullCampaignProbeCombatRepositionActive = true;
            FullCampaignProbeCombatRepositionDirection = RepositionDirection;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian full campaign probe: combat reposition encounter=%s target=%s no_damage_shots=%d distance_cm=%.1f direction=%s"),
                EncounterLabel ? EncounterLabel : TEXT("unknown"),
                *NearestTarget->GetName(),
                FullCampaignProbeCombatNoDamageShots,
                TargetDistance,
                *RepositionDirection.ToString());
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian full campaign probe: combat reposition state player=%s movement_mode=%d blocked_legs=%d clear_leg=%s sightline_leg=%s move_input_ignored=%s pending_before=%s pending_after=%s velocity=%s max_walk_speed=%.1f"),
                *Player->GetActorLocation().ToString(),
                Player->GetCharacterMovement() ? static_cast<int32>(Player->GetCharacterMovement()->MovementMode) : -1,
                BlockedLegCount,
                bFoundClearLeg ? TEXT("yes") : TEXT("no"),
                bFoundSightlineLeg ? TEXT("yes") : TEXT("no"),
                bMoveInputIgnoredBefore ? TEXT("yes") : TEXT("no"),
                *PendingInputBefore.ToString(),
                *Player->GetPendingMovementInputVector().ToString(),
                Player->GetCharacterMovement() ? *Player->GetCharacterMovement()->Velocity.ToString() : TEXT("none"),
                Player->GetCharacterMovement() ? Player->GetCharacterMovement()->MaxWalkSpeed : 0.0f);
            if (!bFoundClearLeg)
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian full campaign probe: combat reposition leg blocked target=%s leg=%d sightline=%s"),
                    *NearestTarget->GetName(),
                    RepositionLeg,
                    bFoundSightlineLeg ? TEXT("yes") : TEXT("no"));
            }
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian full campaign probe: player shot encounter=%s target=%s accepted=%s damage=%s target_health=%.1f->%.1f player_mag=%d->%d distance_cm=%.1f"),
            EncounterLabel ? EncounterLabel : TEXT("unknown"),
            *NearestTarget->GetName(),
            Weapon->GetMagazineAmmo() < MagazineBefore ? TEXT("yes") : TEXT("no"),
            bDamageApplied ? TEXT("yes") : TEXT("no"),
            HealthBefore,
            HealthAfter,
            MagazineBefore,
            Weapon->GetMagazineAmmo(),
            FMath::Sqrt(NearestDistanceSquared));

        return true;
    };

    switch (FullCampaignProbeStage)
    {
    case 1:
        if (!bFullCampaignProbeTraversalActive)
        {
            BeginTraversal(TEXT("insertion"), Level.InsertionLocation, 2);
        }
        TickTraversal();
        break;
    case 2:
        // The proximity gate can advance the mission into Checkpoint before
        // the movement verifier has crossed the final 260 cm of its approach.
        // Keep driving the real CharacterMovement path through that hand-off;
        // otherwise a physically valid but slightly obstructed layout leaves
        // the verifier parked outside the combat pocket while the live wave
        // fires at the player.
        if (State->MissionPhase == EBMMissionPhase::QuietApproach
            || State->MissionPhase == EBMMissionPhase::Checkpoint)
        {
            if (!bFullCampaignProbeTraversalActive)
            {
                BeginTraversal(TEXT("checkpoint_approach"), Level.CheckpointLocation, 3);
            }
            TickTraversal();
        }
        break;
    case 3:
        TickTraversal();
        if (!bFullCampaignProbeTraversalActive && FullCampaignProbeStage == 3)
        {
            // The traversal helper advances to stage 4 on arrival; this
            // guard is only here to make a zero-delta frame harmless.
            FullCampaignProbeStage = 4;
        }
        break;
    case 4:
        if (State->MissionPhase == EBMMissionPhase::Checkpoint)
        {
            FullCampaignProbeStage = 5;
        }
        break;
    case 5:
        if (State->MissionPhase == EBMMissionPhase::Checkpoint)
        {
            TickCombat(Level.CheckpointWaveSize, TEXT("checkpoint"), 6);
        }
        else if (State->MissionPhase == EBMMissionPhase::InteriorBreach)
        {
            FullCampaignProbeStage = 6;
        }
        break;
    case 6:
        if (State->MissionPhase == EBMMissionPhase::InteriorBreach)
        {
            if (!bFullCampaignProbeTraversalActive)
            {
                BeginTraversal(TEXT("relay_breach_approach"), Level.BreachLocation, 7);
            }
            TickTraversal();
        }
        break;
    case 7:
        if (State->MissionPhase == EBMMissionPhase::InteriorBreach)
        {
            if (!bFullCampaignProbeInteractionStarted)
            {
                BeginInteraction(TEXT("RelayBreach"));
            }
            if (bFullCampaignProbeInteractionStarted)
            {
                TickInteraction(EBMMissionPhase::InteriorBreach, EBMMissionPhase::Intelligence, 8);
            }
        }
        else if (State->MissionPhase == EBMMissionPhase::Intelligence)
        {
            ResetInteraction();
            FullCampaignProbeStage = 8;
        }
        break;
    case 8:
        if (State->MissionPhase == EBMMissionPhase::Intelligence)
        {
            // Intelligence advances as soon as the player enters its 360 cm
            // proximity gate. Begin the required console hold at a measured
            // 350 cm or closer before continuing the movement leg, so the
            // optional live-game hand-off cannot race past this verification.
            if (!bFullCampaignProbeInteractionStarted
                && FVector::Dist2D(Player->GetActorLocation(), Level.IntelLocation) <= 350.0f)
            {
                if (BeginInteraction(TEXT("IntelConsole")))
                {
                    FullCampaignProbeStage = 9;
                }
            }
            if (FullCampaignProbeStage == 8 && !bFullCampaignProbeTraversalActive)
            {
                BeginTraversal(TEXT("intel_approach"), Level.IntelLocation, 9);
            }
            if (FullCampaignProbeStage == 8)
            {
                TickTraversal();
            }
            else if (FullCampaignProbeStage == 9 && bFullCampaignProbeInteractionStarted)
            {
                TickInteraction(EBMMissionPhase::Intelligence, EBMMissionPhase::OpenCombat, 10);
            }
        }
        break;
    case 9:
        if (State->MissionPhase == EBMMissionPhase::Intelligence)
        {
            if (!bFullCampaignProbeInteractionStarted)
            {
                BeginInteraction(TEXT("IntelConsole"));
            }
            if (bFullCampaignProbeInteractionStarted)
            {
                TickInteraction(EBMMissionPhase::Intelligence, EBMMissionPhase::OpenCombat, 10);
            }
        }
        else if (State->MissionPhase == EBMMissionPhase::OpenCombat)
        {
            ResetInteraction();
            FullCampaignProbeStage = 10;
        }
        break;
    case 10:
        if (State->MissionPhase == EBMMissionPhase::OpenCombat)
        {
            if (!bFullCampaignProbeTraversalActive)
            {
                BeginTraversal(TEXT("open_combat_approach"), Level.OpenCombatLocation, 11);
            }
            TickTraversal();
        }
        break;
    case 11:
        if (State->MissionPhase == EBMMissionPhase::OpenCombat)
        {
            TickCombat(Level.OpenCombatWaveSize, TEXT("open_combat"), 12);
        }
        else if (State->MissionPhase == EBMMissionPhase::Extraction)
        {
            FullCampaignProbeStage = 12;
        }
        break;
    case 12:
        if (State->MissionPhase == EBMMissionPhase::Extraction)
        {
            if (!bFullCampaignProbeTraversalActive)
            {
                BeginTraversal(TEXT("extraction_approach"), Level.ExtractionLocation, 13);
            }
            TickTraversal();
        }
        break;
    case 13:
        if (State->MissionPhase == EBMMissionPhase::Extraction)
        {
            if (!bFullCampaignProbeInteractionStarted)
            {
                BeginInteraction(TEXT("ExtractionBoat"));
            }
            if (bFullCampaignProbeInteractionStarted)
            {
                TickInteraction(EBMMissionPhase::Extraction, EBMMissionPhase::FinalDefense, 14);
            }
        }
        else if (State->MissionPhase == EBMMissionPhase::FinalDefense)
        {
            ResetInteraction();
            FullCampaignProbeStage = 14;
        }
        break;
    case 14:
        if (State->MissionPhase == EBMMissionPhase::FinalDefense)
        {
            TickCombat(Level.FinalDefenseWaveSize, TEXT("final_defense"), 15);
        }
        else if (State->MissionPhase == EBMMissionPhase::Complete)
        {
            FullCampaignProbeStage = 15;
        }
        break;
    case 15:
        if (State->MissionPhase == EBMMissionPhase::Complete)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian full campaign probe: PASS level=%s traversal=complete combat=complete objectives=relay+console extraction=boat final_defense=complete phase=Complete"),
                *Level.LevelId.ToString());
            SetActorTickEnabled(false);
            FPlatformMisc::RequestExit(false);
        }
        break;
    default:
        FailProbe(TEXT("invalid_probe_stage"));
        break;
    }
}

void ABMGameMode::RunBoundaryGuardProbe(const float DeltaSeconds)
{
    if (bBoundaryGuardProbeFinished || !GetWorld())
    {
        return;
    }

    if (GetWorld()->IsPaused())
    {
        if (APlayerController* LocalController = GetWorld()->GetFirstPlayerController())
        {
            LocalController->SetPause(false);
        }
        return;
    }

    ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Player || !Player->HasActorBegunPlay())
    {
        return;
    }

    BoundaryGuardProbeElapsedSeconds += FMath::Max(DeltaSeconds, 0.0f);
    // Give CharacterMovement a short stable-floor window before adding input.
    // The probe therefore exercises the same movement/collision path as a
    // player, rather than judging the pawn during its initial spawn settle.
    if (BoundaryGuardProbeElapsedSeconds >= 1.0f && BoundaryGuardProbeElapsedSeconds < 8.0f)
    {
        Player->AddMovementInput(FVector(-1.0f, 0.0f, 0.0f), 1.0f, false);
    }

    if (BoundaryGuardProbeElapsedSeconds < 8.0f)
    {
        return;
    }

    const FVector Location = Player->GetActorLocation();
    const UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
    const bool bReachedPlayableEdge = Location.X <= -2000.0f;
    const bool bGrounded = Movement && Movement->IsMovingOnGround();
    const bool bAlive = !Player->IsEliminated();

    bBoundaryGuardProbeFinished = true;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian boundary guard probe: %s location=%s grounded=%s eliminated=%s"),
        bReachedPlayableEdge && bGrounded && bAlive ? TEXT("PASS") : TEXT("FAIL"),
        *Location.ToString(),
        bGrounded ? TEXT("yes") : TEXT("no"),
        bAlive ? TEXT("no") : TEXT("yes"));
    FPlatformMisc::RequestExit(false);
}

void ABMGameMode::RunCombatFeelProbe(const float DeltaSeconds)
{
    (void)DeltaSeconds;
    if (bCombatFeelProbeFinished || !GetWorld())
    {
        return;
    }

    if (GetWorld()->IsPaused())
    {
        if (APlayerController* LocalController = GetWorld()->GetFirstPlayerController())
        {
            LocalController->SetPause(false);
        }
        return;
    }

    ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    ABMWeapon* Weapon = Player ? Player->GetCurrentWeapon() : nullptr;
    if (!Player || !Weapon || !Weapon->GetWeaponData() || !Player->HasActorBegunPlay())
    {
        return;
    }

    const auto CleanupCombatFeelProbeTarget = [this]()
    {
        if (CombatFeelProbeTarget.IsValid())
        {
            CombatFeelProbeTarget->Destroy();
            CombatFeelProbeTarget.Reset();
        }
    };

    const auto FailCombatFeelProbe = [&](const TCHAR* Reason)
    {
        CleanupCombatFeelProbeTarget();
        bCombatFeelProbeFinished = true;
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Black Meridian combat feel probe: FAIL reason=%s"),
            Reason ? Reason : TEXT("unknown"));
        SetActorTickEnabled(false);
        FPlatformMisc::RequestExit(false);
    };

    const float Now = GetWorld()->GetTimeSeconds();
    if (CombatFeelProbeStage == 0)
    {
        if (!Weapon->SetCombatProbeAmmo(30, 120))
        {
            bCombatFeelProbeFinished = true;
            UE_LOG(LogTemp, Error, TEXT("Black Meridian combat feel probe: FAIL could_not_prepare_ammo"));
            FPlatformMisc::RequestExit(false);
            return;
        }

        // The feel probe measures the real player fire path, so its positive
        // shot must have a real live hostile under the same canonical
        // crosshair resolver used by the HUD and by server damage validation.
        // The old probe fired wherever the opening camera happened to face;
        // on Glasswake that is commonly a wall or the north boundary, making
        // the probe report a false combat failure before it ever exercised
        // fire-rate, recoil, or accuracy recovery.
        if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
            Movement->SetMovementMode(MOVE_None);
        }

        const FVector ViewOrigin = Player->GetFirstPersonCamera()
            ? Player->GetFirstPersonCamera()->GetComponentLocation()
            : Player->GetPawnViewLocation();
        const FVector BaseDirection = Player->GetAimDirection().GetSafeNormal();
        if (BaseDirection.IsNearlyZero())
        {
            FailCombatFeelProbe(TEXT("player_view_direction_missing"));
            return;
        }

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        TSubclassOf<ABMEnemyCharacter> SpawnClass = EnemyCharacterClass;
        if (!SpawnClass)
        {
            SpawnClass = ABMEnemyCharacter::StaticClass();
        }
        ABMEnemyCharacter* ProbeTarget = GetWorld()->SpawnActor<ABMEnemyCharacter>(
            SpawnClass,
            ViewOrigin + BaseDirection * 520.0f - FVector(0.0f, 0.0f, 72.0f),
            FRotator::ZeroRotator,
            SpawnParameters);
        if (!ProbeTarget)
        {
            FailCombatFeelProbe(TEXT("target_spawn_failed"));
            return;
        }

        CombatFeelProbeTarget = ProbeTarget;
        ProbeTarget->SetCombatStartDelay(60.0f);
        ProbeTarget->SetAIState(EBMAIState::Suppressed);
        if (AController* TargetController = ProbeTarget->GetController())
        {
            TargetController->StopMovement();
            TargetController->UnPossess();
            TargetController->Destroy();
        }
        if (UCharacterMovementComponent* TargetMovement = ProbeTarget->GetCharacterMovement())
        {
            TargetMovement->StopMovementImmediately();
            TargetMovement->SetMovementMode(MOVE_None);
        }
        if (UCapsuleComponent* AimHitbox = ProbeTarget->GetCombatAimHitbox())
        {
            AimHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            AimHitbox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
            AimHitbox->UpdateComponentToWorld();
        }

        CombatFeelProbeFireInterval = 60.0f / FMath::Max(1.0f, Weapon->GetWeaponData()->Balance.FireRateRPM);
        CombatFeelProbeStageStartedAt = Now;
        CombatFeelProbeStage = 1;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat feel probe: armed weapon=%s fire_interval=%.4f magazine=%d reserve=%d"),
            *GetNameSafe(Weapon),
            CombatFeelProbeFireInterval,
            Weapon->GetMagazineAmmo(),
            Weapon->GetReserveAmmo());
        return;
    }

    if (CombatFeelProbeStage == 1)
    {
        // Let the ordinary player tick establish the initial first-person
        // presentation before taking the no-shot baseline. This is the same
        // weapon actor that a player sees, attached to WeaponAnchor.
        if ((Now - CombatFeelProbeStageStartedAt) < 0.15f)
        {
            return;
        }

        ABMEnemyCharacter* ProbeTarget = CombatFeelProbeTarget.Get();
        if (!ProbeTarget || ProbeTarget->IsEliminated() || !ProbeTarget->HealthComponent)
        {
            FailCombatFeelProbe(TEXT("target_invalid_before_fire"));
            return;
        }

        const FVector ViewOrigin = Player->GetFirstPersonCamera()
            ? Player->GetFirstPersonCamera()->GetComponentLocation()
            : Player->GetPawnViewLocation();
        const FVector BaseDirection = Player->GetAimDirection().GetSafeNormal();
        if (BaseDirection.IsNearlyZero())
        {
            FailCombatFeelProbe(TEXT("player_view_direction_missing"));
            return;
        }

        // Try the current view and small yaw offsets so the deterministic
        // target can be placed in a clear lane even when a generated map prop
        // happens to occupy the opening camera's first ray. Every candidate
        // is accepted only if the shared resolver reaches this exact hostile
        // aim surface first; no probe-only aim assist is introduced.
        const FVector CandidateDirections[] = {
            BaseDirection,
            BaseDirection.RotateAngleAxis(15.0f, FVector::UpVector).GetSafeNormal(),
            BaseDirection.RotateAngleAxis(-15.0f, FVector::UpVector).GetSafeNormal(),
            BaseDirection.RotateAngleAxis(30.0f, FVector::UpVector).GetSafeNormal(),
            BaseDirection.RotateAngleAxis(-30.0f, FVector::UpVector).GetSafeNormal()};
        const float CandidateRanges[] = {360.0f, 520.0f, 700.0f};
        const float CrosshairRange = FMath::Max(
            100.0f,
            Weapon->GetWeaponData()->Balance.RangeMeters * 100.0f);
        bool bTargetPlaced = false;
        for (const FVector& CandidateDirection : CandidateDirections)
        {
            if (CandidateDirection.IsNearlyZero())
            {
                continue;
            }

            for (const float CandidateRange : CandidateRanges)
            {
                const FVector CandidatePoint = ViewOrigin + CandidateDirection * CandidateRange;
                const FVector CandidateLocation = CandidatePoint - FVector(0.0f, 0.0f, 72.0f);
                ProbeTarget->SetActorLocation(
                    CandidateLocation,
                    false,
                    nullptr,
                    ETeleportType::TeleportPhysics);
                if (UCharacterMovementComponent* TargetMovement = ProbeTarget->GetCharacterMovement())
                {
                    TargetMovement->StopMovementImmediately();
                    TargetMovement->SetMovementMode(MOVE_None);
                }
                if (ProbeTarget->GetCombatAimHitbox())
                {
                    ProbeTarget->GetCombatAimHitbox()->UpdateComponentToWorld();
                }

                FHitResult CrosshairHit;
                ABMCombatCharacter* ResolvedTarget = BMCombatTargeting::ResolveLiveHostileCrosshairTarget(
                    GetWorld(),
                    Player,
                    ViewOrigin,
                    CandidateDirection,
                    CrosshairRange,
                    Weapon,
                    &CrosshairHit);
                if (ResolvedTarget == ProbeTarget
                    && CrosshairHit.bBlockingHit
                    && CrosshairHit.GetActor() == ProbeTarget
                    && BMCombatTargeting::IsCombatAimHit(ProbeTarget, CrosshairHit))
                {
                    CombatFeelProbeDirection = CandidateDirection;
                    bTargetPlaced = true;
                    break;
                }
            }

            if (bTargetPlaced)
            {
                break;
            }
        }

        if (!bTargetPlaced)
        {
            FailCombatFeelProbe(TEXT("target_clear_lane_unavailable"));
            return;
        }

        ProbeTarget->HealthComponent->bRegenerateHealth = false;
        ProbeTarget->HealthComponent->SetHealthForCheckpoint(100.0f);

        CombatFeelProbeInitialPenalty = Weapon->GetAccuracyPenaltyDegrees();
        const USceneComponent* WeaponRoot = Weapon->GetRootComponent();
        CombatFeelProbeInitialWeaponLocation = WeaponRoot ? WeaponRoot->GetRelativeLocation() : FVector::ZeroVector;
        CombatFeelProbeInitialWeaponRotation = WeaponRoot ? WeaponRoot->GetRelativeRotation() : FRotator::ZeroRotator;

        const FVector Direction = CombatFeelProbeDirection;
        const int32 MagazineBefore = Weapon->GetMagazineAmmo();
        Player->RunCombatProbeFire(Direction);
        const int32 MagazineAfterFirst = Weapon->GetMagazineAmmo();
        bCombatFeelProbeFirstShotAccepted = MagazineAfterFirst == MagazineBefore - 1;
        CombatFeelProbePenaltyAfterFirstShot = Weapon->GetAccuracyPenaltyDegrees();

        const int32 MagazineBeforeImmediate = Weapon->GetMagazineAmmo();
        Player->RunCombatProbeFire(Direction);
        const int32 MagazineAfterImmediate = Weapon->GetMagazineAmmo();
        bCombatFeelProbeImmediateRejected = MagazineAfterImmediate == MagazineBeforeImmediate;
        // The trigger call can legitimately do setup work on a cold asset
        // path. Anchor interval assertions to the completed authoritative
        // request rather than to the frame that entered the request.
        CombatFeelProbeFirstShotAt = GetWorld()->GetTimeSeconds();
        CombatFeelProbeStage = 2;

        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat feel probe: first_shot accepted=%s magazine=%d->%d penalty=%.3f immediate_request rejected=%s magazine=%d->%d"),
            bCombatFeelProbeFirstShotAccepted ? TEXT("yes") : TEXT("no"),
            MagazineBefore,
            MagazineAfterFirst,
            CombatFeelProbePenaltyAfterFirstShot,
            bCombatFeelProbeImmediateRejected ? TEXT("yes") : TEXT("no"),
            MagazineBeforeImmediate,
            MagazineAfterImmediate);
        return;
    }

    if (CombatFeelProbeStage == 2)
    {
        if ((Now - CombatFeelProbeFirstShotAt) < CombatFeelProbeFireInterval * 0.5f)
        {
            return;
        }

        const FVector Direction = CombatFeelProbeDirection;
        const int32 MagazineBefore = Weapon->GetMagazineAmmo();
        Player->RunCombatProbeFire(Direction);
        const int32 MagazineAfter = Weapon->GetMagazineAmmo();
        bCombatFeelProbeHalfIntervalRejected = MagazineAfter == MagazineBefore;
        const float RequestCompletedAt = GetWorld()->GetTimeSeconds();
        CombatFeelProbeStage = 3;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat feel probe: half_interval_request rejected=%s elapsed=%.4f magazine=%d->%d"),
            bCombatFeelProbeHalfIntervalRejected ? TEXT("yes") : TEXT("no"),
            RequestCompletedAt - CombatFeelProbeFirstShotAt,
            MagazineBefore,
            MagazineAfter);
        return;
    }

    if (CombatFeelProbeStage == 3)
    {
        // Add a small margin so a frame landing exactly on the interval is
        // not treated as a failed post-interval request due to float rounding.
        if ((Now - CombatFeelProbeFirstShotAt) < CombatFeelProbeFireInterval + 0.03f)
        {
            return;
        }

        const FVector Direction = CombatFeelProbeDirection;
        const int32 MagazineBefore = Weapon->GetMagazineAmmo();
        Player->RunCombatProbeFire(Direction);
        const int32 MagazineAfter = Weapon->GetMagazineAmmo();
        bCombatFeelProbePostIntervalAccepted = MagazineAfter == MagazineBefore - 1;
        CombatFeelProbePenaltyAfterPostIntervalShot = Weapon->GetAccuracyPenaltyDegrees();
        const float RequestCompletedAt = GetWorld()->GetTimeSeconds();
        CombatFeelProbeStageStartedAt = RequestCompletedAt;
        CombatFeelProbeStage = 4;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat feel probe: post_interval_request accepted=%s elapsed=%.4f magazine=%d->%d penalty=%.3f"),
            bCombatFeelProbePostIntervalAccepted ? TEXT("yes") : TEXT("no"),
            RequestCompletedAt - CombatFeelProbeFirstShotAt,
            MagazineBefore,
            MagazineAfter,
            CombatFeelProbePenaltyAfterPostIntervalShot);
        return;
    }

    if (CombatFeelProbeStage == 4)
    {
        // Leave enough time for the real UpdatePresentation path to call the
        // fire-mode recovery clock and for the procedural sway phase to move
        // away from the baseline. This is intentionally numeric evidence, not
        // a substitute for a player-visible capture.
        if ((Now - CombatFeelProbeStageStartedAt) < 0.30f)
        {
            return;
        }

        CombatFeelProbePenaltyAfterRecovery = Weapon->GetAccuracyPenaltyDegrees();
        const USceneComponent* WeaponRoot = Weapon->GetRootComponent();
        const FVector CurrentLocation = WeaponRoot ? WeaponRoot->GetRelativeLocation() : FVector::ZeroVector;
        const FRotator CurrentRotation = WeaponRoot ? WeaponRoot->GetRelativeRotation() : FRotator::ZeroRotator;
        const float SwayLocationDelta = FVector::Dist(
            CombatFeelProbeInitialWeaponLocation,
            CurrentLocation);
        const float SwayRotationDelta =
            FMath::Abs(FMath::FindDeltaAngleDegrees(CombatFeelProbeInitialWeaponRotation.Pitch, CurrentRotation.Pitch))
            + FMath::Abs(FMath::FindDeltaAngleDegrees(CombatFeelProbeInitialWeaponRotation.Yaw, CurrentRotation.Yaw))
            + FMath::Abs(FMath::FindDeltaAngleDegrees(CombatFeelProbeInitialWeaponRotation.Roll, CurrentRotation.Roll));
        const bool bAccuracyIncreased = CombatFeelProbePenaltyAfterFirstShot > CombatFeelProbeInitialPenalty + 0.01f;
        const bool bAccuracyRecovered = CombatFeelProbePenaltyAfterRecovery + 0.01f < CombatFeelProbePenaltyAfterPostIntervalShot;
        const bool bSwayMoved = SwayLocationDelta > 0.02f || SwayRotationDelta > 0.02f;
        const bool bPassed = bCombatFeelProbeFirstShotAccepted
            && bCombatFeelProbeImmediateRejected
            && bCombatFeelProbeHalfIntervalRejected
            && bCombatFeelProbePostIntervalAccepted
            && bAccuracyIncreased
            && bAccuracyRecovered
            && bSwayMoved;

        bCombatFeelProbeFinished = true;
        CleanupCombatFeelProbeTarget();
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat feel probe: %s first_shot=%s immediate_rejected=%s half_interval_rejected=%s post_interval=%s accuracy_before=%.3f after_first=%.3f after_post=%.3f after_recovery=%.3f accuracy_increased=%s accuracy_recovered=%s sway_location_delta_cm=%.3f sway_rotation_delta_deg=%.3f sway_moved=%s"),
            bPassed ? TEXT("PASS") : TEXT("FAIL"),
            bCombatFeelProbeFirstShotAccepted ? TEXT("accepted") : TEXT("rejected"),
            bCombatFeelProbeImmediateRejected ? TEXT("yes") : TEXT("no"),
            bCombatFeelProbeHalfIntervalRejected ? TEXT("yes") : TEXT("no"),
            bCombatFeelProbePostIntervalAccepted ? TEXT("accepted") : TEXT("rejected"),
            CombatFeelProbeInitialPenalty,
            CombatFeelProbePenaltyAfterFirstShot,
            CombatFeelProbePenaltyAfterPostIntervalShot,
            CombatFeelProbePenaltyAfterRecovery,
            bAccuracyIncreased ? TEXT("yes") : TEXT("no"),
            bAccuracyRecovered ? TEXT("yes") : TEXT("no"),
            SwayLocationDelta,
            SwayRotationDelta,
            bSwayMoved ? TEXT("yes") : TEXT("no"));
        FPlatformMisc::RequestExit(false);
    }
}
#endif

void ABMGameMode::BeginPlay()
{
    Super::BeginPlay();

#if !UE_BUILD_SHIPPING
    const bool bBoundaryGuardProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_BoundaryGuardProbe"));
    const bool bCombatFeelProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_CombatFeelProbe"));
    const bool bAimValidationProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_AimValidationProbe"));
    const bool bMissionProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_MissionProbe"));
    const bool bMissionInteractionProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_InteractionProbe"));
    const bool bFullCampaignProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_FullCampaignProbe"));
    const bool bInteractionAcceptanceProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_InteractionAcceptanceProbe"));
    const bool bDestructionPresentationShowcase = FParse::Param(FCommandLine::Get(), TEXT("BM_DestructionPresentationShowcase"));
    float MissionProbeTimeScale = 1.0f;
    if (FParse::Value(FCommandLine::Get(), TEXT("BM_MissionTimeScale="), MissionProbeTimeScale))
    {
        MissionProbeTimeScale = FMath::Clamp(MissionProbeTimeScale, 1.0f, 100.0f);
    }
#else
    constexpr bool bCombatFeelProbe = false;
    constexpr bool bMissionProbe = false;
    constexpr bool bMissionInteractionProbe = false;
    constexpr bool bInteractionAcceptanceProbe = false;
    constexpr bool bDestructionPresentationShowcase = false;
#endif

    UE_LOG(LogTemp, Display, TEXT("Black Meridian runtime: BMGameMode BeginPlay (authority=%s, mission=%s)"), HasAuthority() ? TEXT("yes") : TEXT("no"), bMissionMode ? TEXT("yes") : TEXT("no"));

    if (HasAuthority())
    {
        LoadPlayerProfile();
        if (ABMGameState* State = GetGameState<ABMGameState>())
        {
            State->CampaignLevel = CampaignLevel;
            State->bMatchLive = bMissionMode;
            State->MatchCountdown = bMissionMode ? 0.0f : 5.0f;
            State->bSignalControlMode = !bMissionMode;
        }
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        GreyboxDirector = GetWorld()->SpawnActorDeferred<ABMGreyboxDirector>(ABMGreyboxDirector::StaticClass(), FTransform::Identity);
        if (GreyboxDirector)
        {
            // Seed the deferred director before BeginPlay so a command-line
            // level does not briefly spawn the Sable Quay pickup layout before
            // the selected campaign variant is applied.
            GreyboxDirector->CampaignLevel = CampaignLevel;
            // The accelerated phase probe normally omits mission actors so
            // headless automation cannot be interrupted by combat. The
            // interaction variant keeps the real door/console/boat actors
            // while retaining the probe's deterministic, wave-free pacing.
            GreyboxDirector->bSpawnMissionActors = bMissionMode
                && (!bMissionProbe || bMissionInteractionProbe)
                && !bBoundaryGuardProbe
                && !bCombatFeelProbe;
            GreyboxDirector->CombatEnemyClass = EnemyCharacterClass.Get();
            if (!GreyboxDirector->CombatEnemyClass)
            {
                GreyboxDirector->CombatEnemyClass = ABMEnemyCharacter::StaticClass();
            }
            GreyboxDirector->FinishSpawning(FTransform::Identity);
        }
        if (bMissionMode)
        {
            MissionDirector = GetWorld()->SpawnActor<ABMMissionDirector>(ABMMissionDirector::StaticClass(), FTransform::Identity, SpawnParameters);

            // Development-only runtime probe. It lets CI/headless Unreal runs
            // exercise the real mission phase clock without synthesizing input.
            // Normal players still enter through the menu and StartMissionInput.
            if (MissionDirector && FParse::Param(FCommandLine::Get(), TEXT("BM_AutoStartMission")))
            {
                UE_LOG(LogTemp, Display, TEXT("Black Meridian probe: auto-starting mission from command line"));
                RequestMissionStart(CampaignLevel);
            }

#if !UE_BUILD_SHIPPING
            if (bMissionProbe && MissionProbeTimeScale > 1.0f)
            {
                UGameplayStatics::SetGlobalTimeDilation(this, MissionProbeTimeScale);
                UE_LOG(LogTemp, Display, TEXT("Black Meridian probe: mission time scale %.1fx (combat actors disabled)"), MissionProbeTimeScale);
            }
#endif
        }

        UE_LOG(LogTemp, Display, TEXT("Black Meridian runtime: spawned greybox director=%s, mission director=%s"), GreyboxDirector ? TEXT("yes") : TEXT("no"), MissionDirector ? TEXT("yes") : TEXT("no"));

#if !UE_BUILD_SHIPPING
        if (bBoundaryGuardProbe)
        {
            // The probe tick runs while paused so a stale menu cannot prevent
            // it from releasing the pause and exercising real movement.
            SetActorTickEnabled(true);
            UE_LOG(LogTemp, Display, TEXT("Black Meridian boundary guard probe: armed"));
        }

        if (bMissionMode && bFullCampaignProbe)
        {
            // Keep the normal mission actors and mission clock. This path is
            // deliberately separate from BM_MissionProbe, whose deterministic
            // phase clock disables combat and bypasses proximity/interaction
            // gates.
            SetActorTickEnabled(true);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian full campaign probe: armed level=%d movement=AddMovementInput combat=player_weapon objectives=normal_hold extraction=boat"),
                static_cast<int32>(CampaignLevel));
        }

        if (bMissionMode && bCombatFeelProbe)
        {
            SetActorTickEnabled(true);
            UE_LOG(LogTemp, Display, TEXT("Black Meridian combat feel probe: armed (real player weapon path)"));
        }

        if (bMissionMode && bAimValidationProbe)
        {
            // This probe owns a server-side tick because it fires the same
            // player weapon path from a standalone game and must observe both
            // the off-target and on-target health results before exiting.
            SetActorTickEnabled(true);
            UE_LOG(LogTemp, Display, TEXT("Black Meridian aim validation probe: armed (off-target must not damage; direct target must damage)"));
        }

        if (bMissionMode && FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcase")))
        {
            // The showcase is world-owned rather than menu-owned. Keep the
            // authority tick available to clear any late local pause.
            SetActorTickEnabled(true);
            UE_LOG(LogTemp, Display, TEXT("Black Meridian showcase: GameMode diagnostic tick enabled"));
        }

        if (bMissionMode && FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcase")))
        {
            // The local editor game can create its player controller after
            // GameMode::BeginPlay and, depending on the launch path, may not
            // use the normal PostLogin ordering. Start a short retry from a
            // world-owned callback so the rendered acceptance path is stable.
            GetWorld()->GetTimerManager().SetTimer(
                CombatShowcaseTimerHandle,
                this,
                &ABMGameMode::TrySpawnCombatShowcase,
                0.25f,
                false);
            UE_LOG(LogTemp, Display, TEXT("Black Meridian showcase: deferred spawn requested from GameMode BeginPlay"));
        }

        if (bMissionMode && bMissionInteractionProbe)
        {
            GetWorld()->GetTimerManager().SetTimer(
                MissionInteractionProbeTimerHandle,
                this,
                &ABMGameMode::RunMissionInteractionProbe,
                0.25f,
                true);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian interaction probe: armed (relay -> console -> extraction)"));
        }

        if (bMissionMode && bInteractionAcceptanceProbe)
        {
            SetActorTickEnabled(true);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian interaction acceptance probe: armed (cover -> mounted -> destruction -> revive)"));
        }

        if (bMissionMode && bDestructionPresentationShowcase)
        {
            SetActorTickEnabled(true);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian destruction presentation showcase: armed"));
        }

        if (bMissionMode && FParse::Param(FCommandLine::Get(), TEXT("BM_PickupProbe")))
        {
            SetActorTickEnabled(true);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian pickup probe: armed (level=%d, proximity weapon grant)"),
                static_cast<int32>(CampaignLevel));
        }

        if (bMissionMode &&
            (FParse::Param(FCommandLine::Get(), TEXT("BM_PersistenceProbeWrite")) ||
             FParse::Param(FCommandLine::Get(), TEXT("BM_PersistenceProbeRead"))))
        {
            GetWorld()->GetTimerManager().SetTimer(
                PersistenceProbeTimerHandle,
                this,
                &ABMGameMode::RunPersistenceProbe,
                0.25f,
                true);
            UE_LOG(LogTemp, Display, TEXT("Black Meridian persistence probe: armed"));
        }

        if (FParse::Param(FCommandLine::Get(), TEXT("BM_ThrowableProbe")))
        {
            // The normal mission menu can pause the world before a
            // pauseable timer receives its first callback. The GameMode tick
            // is enabled for this development probe and is configured to run
            // while paused so it can release that menu pause deterministically.
            SetActorTickEnabled(true);
            UE_LOG(LogTemp, Display, TEXT("Black Meridian throwable probe: armed"));
        }

        if (FParse::Param(FCommandLine::Get(), TEXT("BM_ThrowablePresentationShowcase")))
        {
            SetActorTickEnabled(true);
            UE_LOG(LogTemp, Display, TEXT("Black Meridian throwable presentation showcase: armed"));
        }

        if (FParse::Param(FCommandLine::Get(), TEXT("BM_ThrowableInputProbe")))
        {
            // Feed a simulated G key through APlayerController::InputKey so
            // this probe exercises the same legacy action mapping and pawn
            // input binding as a physical keyboard press. Do not call
            // ThrowFrag directly: the purpose is to prove the input route.
            SetActorTickEnabled(true);
            UE_LOG(LogTemp, Display, TEXT("Black Meridian throwable input probe: armed (simulated G through PlayerController::InputKey)"));
        }

        if (FParse::Param(FCommandLine::Get(), TEXT("BM_ReloadInputProbe")))
        {
            // Exercise the normal R action mapping through PlayerController
            // rather than calling ReloadWeapon directly. This is the same
            // legacy input route used by a physical keyboard press.
            SetActorTickEnabled(true);
            bReloadInputProbeEmpty = FParse::Param(FCommandLine::Get(), TEXT("BM_ReloadInputProbeEmpty"));
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian reload input probe: armed (simulated R through PlayerController::InputKey empty=%s)"),
                bReloadInputProbeEmpty ? TEXT("yes") : TEXT("no"));
        }
#endif
    }
}

void ABMGameMode::LoadPlayerProfile()
{
    const FString SaveSlot = GetProfileSaveSlotName();
    PlayerProfile = Cast<UBMPlayerProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
    if (!PlayerProfile)
    {
        PlayerProfile = Cast<UBMPlayerProfileSaveGame>(UGameplayStatics::CreateSaveGameObject(UBMPlayerProfileSaveGame::StaticClass()));
        SavePlayerProfile();
    }
}

void ABMGameMode::SavePlayerProfile() const
{
    if (PlayerProfile)
    {
        const FString SaveSlot = GetProfileSaveSlotName();
        const bool bSaved = UGameplayStatics::SaveGameToSlot(PlayerProfile, SaveSlot, 0);
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian persistence: profile save slot=%s success=%s campaign=%d completed_levels=%d multiplayer=%d unlocked=%d"),
            *SaveSlot,
            bSaved ? TEXT("yes") : TEXT("no"),
            PlayerProfile->CampaignCompletionCount,
            PlayerProfile->CompletedCampaignLevels.Num(),
            PlayerProfile->MultiplayerMatches,
            PlayerProfile->UnlockedWeapons.Num());
    }
}

void ABMGameMode::RecordCampaignCompletion()
{
    if (!HasAuthority())
    {
        return;
    }
    if (!PlayerProfile)
    {
        LoadPlayerProfile();
    }
    if (PlayerProfile)
    {
        ++PlayerProfile->CampaignCompletionCount;
        PlayerProfile->CompletedCampaignLevels.AddUnique(CampaignLevel);
        PlayerProfile->UnlockedWeapons.AddUnique(EBMWeaponId::RookDMR);
        PlayerProfile->UnlockedWeapons.AddUnique(EBMWeaponId::ArcLauncher);
        SavePlayerProfile();
    }
}

void ABMGameMode::RecordMultiplayerMatch()
{
    if (!HasAuthority())
    {
        return;
    }
    if (!PlayerProfile)
    {
        LoadPlayerProfile();
    }
    if (PlayerProfile)
    {
        ++PlayerProfile->MultiplayerMatches;
        SavePlayerProfile();
    }
}

TArray<EBMWeaponId> ABMGameMode::GetProfileLoadout() const
{
    TArray<EBMWeaponId> Result;
    if (PlayerProfile)
    {
        for (const EBMWeaponId WeaponId : PlayerProfile->UnlockedWeapons)
        {
            if (WeaponId != EBMWeaponId::FragGrenade && WeaponId != EBMWeaponId::VeilGrenade)
            {
                Result.AddUnique(WeaponId);
            }
            if (Result.Num() >= 2)
            {
                break;
            }
        }
    }
    if (Result.Num() < 2)
    {
        Result.AddUnique(EBMWeaponId::ARX4);
        Result.AddUnique(EBMWeaponId::Kestrel9);
    }
    return Result;
}

void ABMGameMode::RegisterCombatLocation(const FVector& Location)
{
    (void)Location;
}

AActor* ABMGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    if (!RuntimePlayerStart.IsValid())
    {
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        RuntimePlayerStart = GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), FVector(0.0f, -950.0f, 120.0f), FRotator(0.0f, 0.0f, 0.0f), SpawnParameters);
    }
    return RuntimePlayerStart.Get();
}

void ABMGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (ABMPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<ABMPlayerState>() : nullptr)
    {
        // A standalone mission has one human player.  The normal alternating
        // team assignment counts that same PlayerState in PlayerArray, which
        // used to flip the just-defaulted player from Lattice to Vanta before
        // the showcase AI performed its target lookup.
        const bool bStandaloneMission = bMissionMode && GetNetMode() == NM_Standalone;
        int32 LatticePlayers = 0;
        if (!bStandaloneMission)
        {
            for (APlayerState* ExistingState : GameState->PlayerArray)
            {
                if (const ABMPlayerState* ExistingBMState = Cast<ABMPlayerState>(ExistingState); ExistingBMState && ExistingBMState->Team == EBMTeam::Lattice)
                {
                    ++LatticePlayers;
                }
            }
        }
        PlayerState->Team = bStandaloneMission
            ? EBMTeam::Lattice
            : (LatticePlayers % 2 == 0 ? EBMTeam::Lattice : EBMTeam::Vanta);
        ABMPlayerCharacter* PlayerCharacter = Cast<ABMPlayerCharacter>(NewPlayer->GetPawn());
        if (PlayerCharacter)
        {
            PlayerCharacter->SetTeam(PlayerState->Team);
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian mission: assigned player team=%d standalone=%s player=%s"),
            static_cast<int32>(PlayerState->Team),
            bStandaloneMission ? TEXT("yes") : TEXT("no"),
            *GetNameSafe(NewPlayer));
        if (bMissionMode)
        {
            if (ABMPlayerController* Controller = Cast<ABMPlayerController>(NewPlayer))
            {
                Controller->ShowMissionMenu();
                if (FParse::Param(FCommandLine::Get(), TEXT("BM_AutoStartMission")))
                {
                    // ShowMissionMenu pauses a normal local session. The
                    // development probe has already requested mission start,
                    // so release that menu pause after the player logs in.
                    NewPlayer->SetPause(false);
                    RequestMissionStart(CampaignLevel);
                }
#if !UE_BUILD_SHIPPING
                if (FParse::Param(FCommandLine::Get(), TEXT("BM_ThrowableProbe")))
                {
                    // The normal mission menu pauses a local session. The
                    // throwable probe is timer-driven, so release that pause
                    // before its first tick can resolve the possessed pawn.
                    NewPlayer->SetPause(false);
                    UE_LOG(LogTemp, Display, TEXT("Black Meridian throwable probe: controller unpaused"));
                }
#endif
            }
        }
    }

    // PostLogin can run before RestartPlayer has possessed the pawn. The
    // showcase is therefore requested here as well as from RestartPlayer;
    // TrySpawnCombatShowcase is idempotent and will resolve once a pawn and
    // the runtime director are both ready.
    TrySpawnCombatShowcase();
}

void ABMGameMode::RestartPlayer(AController* NewPlayer)
{
    Super::RestartPlayer(NewPlayer);
    if (ABMPlayerCharacter* Player = NewPlayer ? Cast<ABMPlayerCharacter>(NewPlayer->GetPawn()) : nullptr)
    {
        if (ABMPlayerState* PlayerState = NewPlayer->GetPlayerState<ABMPlayerState>())
        {
            // Standalone mission launches can reach RestartPlayer without the
            // network PostLogin assignment. Do not let the PlayerState's
            // Neutral default erase the player's Lattice identity; enemies
            // use that identity to acquire and return fire on the player.
            if (bMissionMode && PlayerState->Team == EBMTeam::Neutral)
            {
                PlayerState->Team = EBMTeam::Lattice;
                PlayerState->ForceNetUpdate();
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian mission: defaulted standalone player team to Lattice"));
            }
            Player->SetTeam(PlayerState->Team);
        }
    }
    TrySpawnCombatShowcase();
}

void ABMGameMode::TrySpawnCombatShowcase()
{
#if !UE_BUILD_SHIPPING
    if (bCombatShowcaseSpawned || !bMissionMode || !FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcase")))
    {
        return;
    }

    if (!GreyboxDirector || !GetWorld())
    {
        return;
    }

    ABMPlayerCharacter* ShowcasePlayer = nullptr;
    if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
    {
        ShowcasePlayer = Cast<ABMPlayerCharacter>(Controller->GetPawn());
    }

    if (!ShowcasePlayer)
    {
        // A possession callback can be one frame later on the editor game
        // path. Retry once without blocking the game thread.
        GetWorld()->GetTimerManager().SetTimer(
            CombatShowcaseTimerHandle,
            this,
            &ABMGameMode::TrySpawnCombatShowcase,
            0.10f,
            false);
        return;
    }

    // Keep the development encounter deterministic even when a standalone
    // launch did not pass through the normal PostLogin team assignment.  A
    // standalone mission is always the Lattice side of the encounter.
    const bool bStandaloneMission = bMissionMode && GetNetMode() == NM_Standalone;
    if ((bStandaloneMission && ShowcasePlayer->GetTeam() != EBMTeam::Lattice)
        || ShowcasePlayer->GetTeam() == EBMTeam::Neutral)
    {
        const EBMTeam PreviousTeam = ShowcasePlayer->GetTeam();
        ShowcasePlayer->SetTeam(EBMTeam::Lattice);
        if (ABMPlayerState* PlayerState = ShowcasePlayer->GetPlayerState<ABMPlayerState>())
        {
            PlayerState->Team = EBMTeam::Lattice;
            PlayerState->ForceNetUpdate();
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian showcase: corrected player team to Lattice standalone=%s previous=%d"),
            bStandaloneMission ? TEXT("yes") : TEXT("no"),
            static_cast<int32>(PreviousTeam));
    }

    bCombatShowcaseSpawned = true;
    GetWorld()->GetTimerManager().ClearTimer(CombatShowcaseTimerHandle);
#if !UE_BUILD_SHIPPING
    // The showcase is a development capture path, not the normal campaign
    // menu flow. Release the local pause explicitly so headless and rendered
    // verification sessions exercise real AI ticks immediately after the
    // encounter is spawned.
    if (ABMPlayerController* Controller = Cast<ABMPlayerController>(ShowcasePlayer->GetController()))
    {
        if (ABMHUD* HUD = Cast<ABMHUD>(Controller->GetHUD()))
        {
            HUD->SetScreen(EBMHUDScreen::InGame);
        }
        Controller->SetPause(false);
        Controller->RestoreInteractiveInput();
        // A delayed HUD/splash callback can run in the same startup window as
        // the showcase spawn. Reassert the capture once more on the next tick
        // so the rendered acceptance path starts with real player look input.
        GetWorld()->GetTimerManager().SetTimerForNextTick(
            Controller,
            &ABMPlayerController::RestoreInteractiveInput);
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian showcase: interactive input restored after spawn handoff world_paused=%s screen=%d"),
            GetWorld()->IsPaused() ? TEXT("yes") : TEXT("no"),
            Cast<ABMHUD>(Controller->GetHUD()) ? static_cast<int32>(Cast<ABMHUD>(Controller->GetHUD())->GetScreen()) : -1);
    }
#endif
    const FVector ViewForward = ShowcasePlayer->GetControlRotation().Vector().GetSafeNormal2D().IsNearlyZero()
        ? ShowcasePlayer->GetActorForwardVector().GetSafeNormal2D()
        : ShowcasePlayer->GetControlRotation().Vector().GetSafeNormal2D();
    GreyboxDirector->SpawnCombatShowcaseWave(ShowcasePlayer->GetActorLocation(), ViewForward);
#if !UE_BUILD_SHIPPING
    // The crosshair probe must exercise the same camera trace that a player
    // sees, rather than the showcase's authority-side BotFireAt ray. The
    // normal mission startup pitch looks slightly downward for route
    // readability; aim the opt-in probe at the close-facing hostile's upper
    // torso so the native reticle/target plate can be validated on the real
    // view, including the top of the visible combat silhouette.
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CrosshairProbe")))
    {
        if (ABMPlayerController* Controller = Cast<ABMPlayerController>(ShowcasePlayer->GetController()))
        {
            for (TActorIterator<ABMEnemyCharacter> Iterator(GetWorld()); Iterator; ++Iterator)
            {
                ABMEnemyCharacter* ProbeTarget = *Iterator;
                if (!IsValid(ProbeTarget) || ProbeTarget->IsEliminated())
                {
                    continue;
                }

                const FVector ViewOrigin = ShowcasePlayer->GetPawnViewLocation();
                // Aim through the visible upper torso rather than the center
                // of the target. This is deliberately above the old 72 cm
                // target top so the packaged probe catches regressions where
                // a visible enemy is not recognized by the reticle.
                const FVector TargetPoint = ProbeTarget->GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
                const FRotator TargetRotation = (TargetPoint - ViewOrigin).Rotation();
                Controller->SetControlRotation(TargetRotation);
                // The rendered probe is unattended.  A macOS cursor
                // recapture can deliver a later synthetic MouseY delta and
                // rotate the camera away from the already-verified target
                // before UMG refreshes the reticle.  Freeze only this
                // development probe so it validates the target trace rather
                // than window-focus behavior.
                Controller->SetIgnoreLookInput(true);
                Controller->SetIgnoreMoveInput(true);
                ShowcasePlayer->SetActorRotation(FRotator(0.0f, TargetRotation.Yaw, 0.0f));
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian showcase: crosshair probe aimed target=%s rotation=%s"),
                    *GetNameSafe(ProbeTarget),
                    *TargetRotation.ToString());
                break;
            }
        }
    }
#endif
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian showcase: spawned rendered combat wave player=%s location=%s forward=%s"),
        *GetNameSafe(ShowcasePlayer),
        *ShowcasePlayer->GetActorLocation().ToString(),
        *ViewForward.ToString());

    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcaseAutoFire")))
    {
        GetWorld()->GetTimerManager().SetTimer(
            CombatShowcaseFireTimerHandle,
            this,
            &ABMGameMode::RunCombatShowcaseAutoFire,
            0.16f,
            true,
            1.5f);
        UE_LOG(LogTemp, Display, TEXT("Black Meridian showcase: controlled auto-fire enabled"));
    }
#endif
}

#if !UE_BUILD_SHIPPING
void ABMGameMode::RequestCombatShowcaseReloadScreenshot(
    const TCHAR* ArgumentName,
    const TCHAR* DefaultFilename,
    const TCHAR* Stage)
{
    FString ScreenshotPath = FPaths::ProjectDir() / TEXT("artifacts") / DefaultFilename;
    FParse::Value(FCommandLine::Get(), ArgumentName, ScreenshotPath);
    FScreenshotRequest::RequestScreenshot(
        ScreenshotPath,
        false,
        false,
        false,
        FIntRect(),
        true);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian reload presentation showcase: screenshot requested stage=%s path=%s"),
        Stage,
        *ScreenshotPath);
}

void ABMGameMode::RunCombatShowcaseAutoFire()
{
    if (!GetWorld() || !FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcaseAutoFire")))
    {
        return;
    }

    ABMPlayerCharacter* ShowcasePlayer = nullptr;
    if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
    {
        ShowcasePlayer = Cast<ABMPlayerCharacter>(Controller->GetPawn());
    }
    if (!ShowcasePlayer || ShowcasePlayer->IsEliminated())
    {
        GetWorld()->GetTimerManager().ClearTimer(CombatShowcaseFireTimerHandle);
        UE_LOG(LogTemp, Warning, TEXT("Black Meridian showcase: controlled auto-fire stopped player_unavailable_or_eliminated"));
        return;
    }

    ABMWeapon* Weapon = ShowcasePlayer->GetCurrentWeapon();
    if (!Weapon)
    {
        return;
    }

    if (FParse::Param(FCommandLine::Get(), TEXT("BM_ReloadPresentationShowcase"))
        && bCombatShowcaseReloadIssued
        && CombatShowcaseReloadStartedAt >= 0.0f)
    {
        const float ReloadElapsed = GetWorld()->GetTimeSeconds() - CombatShowcaseReloadStartedAt;
        if (Weapon->IsReloading())
        {
            if (!bCombatShowcaseReloadMidScreenshotRequested && ReloadElapsed >= 0.12f)
            {
                RequestCombatShowcaseReloadScreenshot(
                    TEXT("BM_ReloadShowcaseMid="),
                    TEXT("reload-presentation-mid.png"),
                    TEXT("mid"));
                bCombatShowcaseReloadMidScreenshotRequested = true;
            }
            return;
        }

        if (!bCombatShowcaseReloadCompleteScreenshotRequested)
        {
            RequestCombatShowcaseReloadScreenshot(
                TEXT("BM_ReloadShowcaseComplete="),
                TEXT("reload-presentation-complete.png"),
                TEXT("complete"));
            bCombatShowcaseReloadCompleteScreenshotRequested = true;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian reload presentation showcase: PASS start=yes mid=%s complete=yes duration=%.3f"),
                bCombatShowcaseReloadMidScreenshotRequested ? TEXT("yes") : TEXT("no"),
                ReloadElapsed);
        }
    }

    ABMCombatCharacter* Target = nullptr;
    bool bAnyLiveEnemy = false;
    for (TActorIterator<ABMEnemyCharacter> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        ABMEnemyCharacter* Candidate = *Iterator;
        if (IsValid(Candidate) && !Candidate->IsEliminated())
        {
            bAnyLiveEnemy = true;
            break;
        }
    }

    // Auto-fire is a presentation helper, not an aim-assist system. Resolve
    // the target from the same camera ray that the player sees. Selecting the
    // nearest visible actor and then firing a separate ray to its centre lets
    // an off-screen enemy receive damage while the reticle is pointed
    // elsewhere.
    // Capture the same post-camera ray that drives the rendered reticle and
    // player fire path. Sampling the camera directly here could land on the
    // other side of a bob/pose update and make showcase-only shots disagree
    // with the red crosshair.
    ShowcasePlayer->UpdateLocalAimSnapshot();
    const FVector AimOrigin = ShowcasePlayer->HasLocalAimSnapshot()
        ? ShowcasePlayer->GetLocalAimOrigin()
        : (ShowcasePlayer->GetFirstPersonCamera()
            ? ShowcasePlayer->GetFirstPersonCamera()->GetComponentLocation()
            : ShowcasePlayer->GetPawnViewLocation());
    const FVector AimDirection = ShowcasePlayer->HasLocalAimSnapshot()
        ? ShowcasePlayer->GetLocalAimDirection()
        : (ShowcasePlayer->GetFirstPersonCamera()
            ? ShowcasePlayer->GetFirstPersonCamera()->GetForwardVector().GetSafeNormal()
            : ShowcasePlayer->GetActorForwardVector().GetSafeNormal());
    const float TraceRange = Weapon->GetWeaponData()
        ? FMath::Max(100.0f, Weapon->GetWeaponData()->Balance.RangeMeters * 100.0f)
        : 10000.0f;
    FHitResult CrosshairHit;
    Target = BMCombatTargeting::ResolveLiveHostileCrosshairTarget(
        GetWorld(),
        ShowcasePlayer,
        AimOrigin,
        AimDirection,
        TraceRange,
        Weapon,
        &CrosshairHit);
    const bool bCrosshairTraceHit = Target != nullptr;
#if !UE_BUILD_SHIPPING
    const FVector TargetDirection = Target
        ? (Target->GetActorLocation() + FVector(0.0f, 0.0f, 72.0f) - AimOrigin).GetSafeNormal()
        : FVector::ZeroVector;
    const float TargetAngleDegrees = Target
        ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(AimDirection, TargetDirection), -1.0f, 1.0f)))
        : 0.0f;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian showcase: crosshair trace origin=%s direction=%s hit=%s target=%s target_angle_deg=%.2f"),
        *AimOrigin.ToString(),
        *AimDirection.ToString(),
        *GetNameSafe(bCrosshairTraceHit ? CrosshairHit.GetActor() : nullptr),
        *GetNameSafe(Target),
        TargetAngleDegrees);
#endif

    if (!Target)
    {
        if (!bAnyLiveEnemy)
        {
            GetWorld()->GetTimerManager().ClearTimer(CombatShowcaseFireTimerHandle);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian showcase: controlled auto-fire complete shots=%d"),
                CombatShowcaseAutoFireShots);
        }
        else
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian showcase: shot skipped reason=crosshair_not_on_live_enemy live_enemy=yes"));
        }
        return;
    }

    if (Weapon->IsReloading())
    {
        return;
    }

    if (!bCombatShowcaseReloadIssued && CombatShowcaseAutoFireShots >= 8)
    {
        bCombatShowcaseReloadIssued = true;
        Weapon->Reload();
        if (FParse::Param(FCommandLine::Get(), TEXT("BM_ReloadPresentationShowcase")))
        {
            CombatShowcaseReloadStartedAt = GetWorld()->GetTimeSeconds();
            RequestCombatShowcaseReloadScreenshot(
                TEXT("BM_ReloadShowcaseStart="),
                TEXT("reload-presentation-start.png"),
                TEXT("start"));
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian showcase: controlled reload requested weapon=%s magazine=%d reserve=%d"),
            *GetNameSafe(Weapon),
            Weapon->GetMagazineAmmo(),
            Weapon->GetReserveAmmo());
        return;
    }

    if (Weapon->GetMagazineAmmo() <= 0)
    {
        Weapon->Reload();
        UE_LOG(LogTemp, Display, TEXT("Black Meridian showcase: controlled reload requested magazine_empty"));
        return;
    }

    const int32 MagazineBefore = Weapon->GetMagazineAmmo();
    ShowcasePlayer->BotFireAt(Target);
    const int32 MagazineAfter = Weapon->GetMagazineAmmo();
    if (MagazineAfter < MagazineBefore)
    {
        ++CombatShowcaseAutoFireShots;
    }
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian showcase: controlled shot target=%s accepted=%s magazine=%d"),
        *GetNameSafe(Target),
        MagazineAfter < MagazineBefore ? TEXT("yes") : TEXT("no"),
        MagazineAfter);
}
#endif

void ABMGameMode::RequestMissionStart(const EBMCampaignLevel Level)
{
    CampaignLevel = Level;
    const FBMCampaignLevelDefinition Definition = GetCampaignLevelDefinition(CampaignLevel);
    if (ABMGameState* State = GetGameState<ABMGameState>())
    {
        State->CampaignLevel = CampaignLevel;
        State->ForceNetUpdate();
    }
    if (GreyboxDirector)
    {
        GreyboxDirector->SetCampaignLevel(CampaignLevel);
    }
    if (MissionDirector)
    {
        MissionDirector->SetCampaignLevel(CampaignLevel);
    }
    if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
    {
        Player->SetActorLocationAndRotation(
            Definition.StartLocation,
            Definition.StartRotation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
        if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian campaign: selected level=%s start=%s weapon_pickup=%d"),
            *Definition.LevelId.ToString(),
            *Definition.StartLocation.ToString(),
            static_cast<int32>(Definition.PickupWeaponId));
    }
    if (MissionDirector)
    {
        MissionDirector->StartMission();
    }
}

void ABMGameMode::HandlePlayerEliminated(ABMPlayerCharacter* PlayerCharacter, const FBMDamageContext& DamageContext)
{
    AController* PlayerController = PlayerCharacter ? PlayerCharacter->GetController() : nullptr;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian combat: player eliminated pawn=%s controller=%s respawn=%s delay=%.1f"),
        *GetNameSafe(PlayerCharacter),
        *GetNameSafe(PlayerController),
        bRespawnPlayers ? TEXT("yes") : TEXT("no"),
        RespawnDelay);
    if (PlayerCharacter)
    {
        RegisterCombatLocation(PlayerCharacter->GetActorLocation());
    }
    if (ABMPlayerState* PlayerState = PlayerCharacter ? PlayerCharacter->GetPlayerState<ABMPlayerState>() : nullptr)
    {
        ++PlayerState->Deaths;
    }

    if (ABMPlayerCharacter* Killer = Cast<ABMPlayerCharacter>(DamageContext.InstigatorActor); Killer && Killer != PlayerCharacter)
    {
        if (ABMPlayerState* KillerState = Killer->GetPlayerState<ABMPlayerState>())
        {
            ++KillerState->Kills;
        }
        if (ABMPlayerController* KillerController = Cast<ABMPlayerController>(Killer->GetController()))
        {
            KillerController->NotifyElimination(DamageContext.bHeadshot);
        }

        if (ABMGameState* State = GetGameState<ABMGameState>())
        {
            const ABMPlayerState* KillerState = Killer->GetPlayerState<ABMPlayerState>();
            const ABMPlayerState* VictimState = PlayerCharacter ? PlayerCharacter->GetPlayerState<ABMPlayerState>() : nullptr;
            const FString KillerName = KillerState && !KillerState->GetPlayerName().IsEmpty() ? KillerState->GetPlayerName() : TEXT("BOT");
            const FString VictimName = VictimState && !VictimState->GetPlayerName().IsEmpty() ? VictimState->GetPlayerName() : TEXT("PLAYER");
            State->RecentKillFeed.Insert(FString::Printf(TEXT("%s  //  %s DOWN"), *KillerName, *VictimName), 0);
            if (State->RecentKillFeed.Num() > 5)
            {
                State->RecentKillFeed.SetNum(5);
            }
            State->ForceNetUpdate();
        }
    }
    if (MissionDirector)
    {
        MissionDirector->NotifyPlayerEliminated();
    }
    else if (bRespawnPlayers && PlayerController)
    {
        FTimerDelegate RespawnDelegate = FTimerDelegate::CreateUObject(this, &ABMGameMode::RespawnPlayer, PlayerController);
        FTimerHandle RespawnTimer;
        GetWorld()->GetTimerManager().SetTimer(RespawnTimer, RespawnDelegate, RespawnDelay, false);
        UE_LOG(LogTemp, Display, TEXT("Black Meridian combat: respawn timer armed controller=%s"), *GetNameSafe(PlayerController));
    }
    else if (bRespawnPlayers)
    {
        UE_LOG(LogTemp, Warning, TEXT("Black Meridian combat: respawn not armed because eliminated player has no controller"));
    }
}

void ABMGameMode::HandleEnemyEliminated(ABMEnemyCharacter* EnemyCharacter, const FBMDamageContext& DamageContext)
{
#if !UE_BUILD_SHIPPING
    const ABMPlayerCharacter* PlayerKiller = Cast<ABMPlayerCharacter>(DamageContext.InstigatorActor);
    const ABMFriendlySquadMember* FriendlyKiller = Cast<ABMFriendlySquadMember>(DamageContext.InstigatorActor);
    const TCHAR* KillSource = PlayerKiller
        ? TEXT("player")
        : (FriendlyKiller ? TEXT("friendly_support") : TEXT("other"));
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat: enemy death attribution source=%s enemy=%s instigator=%s weapon=%s aim_validated=%s melee=%s throwable=%s"),
            KillSource,
            *GetNameSafe(EnemyCharacter),
            *GetNameSafe(DamageContext.InstigatorActor),
            DamageContext.bPlayerWeaponDamage ? TEXT("yes") : TEXT("no"),
            DamageContext.bPlayerWeaponAimValidated ? TEXT("yes") : TEXT("no"),
            DamageContext.bPlayerMeleeDamage ? TEXT("yes") : TEXT("no"),
            DamageContext.bPlayerThrowableDamage ? TEXT("yes") : TEXT("no"));
#endif
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcaseAutoFire")))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian showcase: enemy death enemy=%s damage=%.1f hit_zone=%d headshot=%s instigator=%s"),
            *GetNameSafe(EnemyCharacter),
            DamageContext.FinalDamage,
            static_cast<int32>(DamageContext.HitZone),
            DamageContext.bHeadshot ? TEXT("yes") : TEXT("no"),
            *GetNameSafe(DamageContext.InstigatorActor));
    }
#endif
    if (ABMGameState* State = GetGameState<ABMGameState>())
    {
        State->HostileCount = FMath::Max(0, State->HostileCount - 1);
        ++State->TeamScore;
        const FString RoleLabel = EnemyCharacter ? EnemyCharacter->GetEnemyRoleLabel() : TEXT("VANTA");
        const ABMPlayerCharacter* PlayerKillerForFeed = Cast<ABMPlayerCharacter>(DamageContext.InstigatorActor);
        const ABMFriendlySquadMember* FriendlyKillerForFeed = Cast<ABMFriendlySquadMember>(DamageContext.InstigatorActor);
        FString KillFeedEntry;
        if (PlayerKillerForFeed)
        {
            KillFeedEntry = FString::Printf(TEXT("YOU // %s DOWN"), *RoleLabel);
        }
        else if (FriendlyKillerForFeed)
        {
            const FString Callsign = FriendlyKillerForFeed->Callsign.ToString();
            KillFeedEntry = FString::Printf(
                TEXT("%s SUPPORT // %s DOWN"),
                Callsign.IsEmpty() ? TEXT("SQUAD") : *Callsign,
                *RoleLabel);
        }
        else
        {
            KillFeedEntry = FString::Printf(TEXT("%s DOWN // SIGNAL CLEAN"), *RoleLabel);
        }
        State->RecentKillFeed.Insert(KillFeedEntry, 0);
        if (State->RecentKillFeed.Num() > 5)
        {
            State->RecentKillFeed.SetNum(5);
        }
        State->ForceNetUpdate();
    }
    if (DamageContext.InstigatorActor)
    {
        APawn* InstigatingPawn = Cast<APawn>(DamageContext.InstigatorActor);
        AController* InstigatingController = InstigatingPawn ? InstigatingPawn->GetController() : nullptr;
        if (ABMPlayerState* PlayerState = InstigatingController ? Cast<ABMPlayerState>(InstigatingController->PlayerState) : nullptr)
        {
            ++PlayerState->Kills;
        }
        if (ABMPlayerController* KillerController = Cast<ABMPlayerController>(InstigatingController))
        {
            KillerController->NotifyElimination(DamageContext.bHeadshot);
        }
    }
    if (MissionDirector)
    {
        MissionDirector->NotifyEnemyEliminated(EnemyCharacter);
    }
}

void ABMGameMode::HandleInteraction(AActor* Interactor, const FName InteractionId)
{
    if (MissionDirector)
    {
        MissionDirector->NotifyInteraction(Interactor, InteractionId);
    }
}

#if !UE_BUILD_SHIPPING
bool ABMGameMode::StartMissionInteractionProbeHold(
    const FName InteractionId,
    const FVector& ApproachLocation)
{
    ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (!Player || !GetWorld())
    {
        return false;
    }

    ABMInteractableActor* Target = nullptr;
    for (TActorIterator<ABMInteractableActor> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        if (Iterator->InteractionId == InteractionId)
        {
            Target = *Iterator;
            break;
        }
    }
    if (!Target)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Black Meridian interaction probe: actor missing id=%s"),
            *InteractionId.ToString());
        return false;
    }

    Player->SetActorLocation(ApproachLocation, false, nullptr, ETeleportType::TeleportPhysics);
    if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
    }
    const bool bStarted = Player->RunInteractionProbeHold(Target);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian interaction probe: hold start id=%s phase=%d started=%s player=%s target=%s duration=%.2f"),
        *InteractionId.ToString(),
        GetGameState<ABMGameState>() ? static_cast<int32>(GetGameState<ABMGameState>()->MissionPhase) : -1,
        bStarted ? TEXT("yes") : TEXT("no"),
        *Player->GetActorLocation().ToString(),
        *Target->GetActorLocation().ToString(),
        Target->HoldDuration);
    return bStarted;
}

void ABMGameMode::RunMissionInteractionProbe()
{
    if (!GetWorld() || !MissionDirector)
    {
        return;
    }

    ABMGameState* State = GetGameState<ABMGameState>();
    ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (!State || !Player || Player->IsHoldingInteraction())
    {
        return;
    }

    switch (MissionInteractionProbeStep)
    {
    case 0:
        if (State->MissionPhase == EBMMissionPhase::InteriorBreach &&
            StartMissionInteractionProbeHold(TEXT("RelayBreach"), FVector(780.0f, 530.0f, 85.0f)))
        {
            MissionInteractionProbeStep = 1;
        }
        break;
    case 1:
        if (State->MissionPhase == EBMMissionPhase::Intelligence &&
            StartMissionInteractionProbeHold(TEXT("IntelConsole"), FVector(850.0f, -240.0f, 85.0f)))
        {
            MissionInteractionProbeStep = 2;
        }
        break;
    case 2:
        if (State->MissionPhase == EBMMissionPhase::Extraction &&
            StartMissionInteractionProbeHold(TEXT("ExtractionBoat"), FVector(1500.0f, 320.0f, 85.0f)))
        {
            MissionInteractionProbeStep = 3;
        }
        break;
    case 3:
        if (State->MissionPhase == EBMMissionPhase::Complete)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian interaction probe: PASS relay=completed console=completed extraction=completed mission=complete"));
            GetWorld()->GetTimerManager().ClearTimer(MissionInteractionProbeTimerHandle);
            MissionInteractionProbeStep = 4;
        }
        break;
    default:
        break;
    }
}

void ABMGameMode::RunPickupProbe()
{
    if (!GetWorld() || !GreyboxDirector || !MissionDirector)
    {
        return;
    }

    ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (!Player)
    {
        return;
    }

    ABMInteractableActor* WeaponPickup = nullptr;
    for (TActorIterator<ABMInteractableActor> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        if (Iterator->InteractionAction == EBMInteractionAction::PickupWeapon)
        {
            WeaponPickup = *Iterator;
            break;
        }
    }

    const FBMCampaignLevelDefinition Definition = GetCampaignLevelDefinition(CampaignLevel);
    const auto FinishProbe = [this](const bool bSuccess, const FString& Reason)
    {
        if (bSuccess)
        {
            UE_LOG(LogTemp, Display, TEXT("Black Meridian pickup probe: %s"), *Reason);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Black Meridian pickup probe: %s"), *Reason);
        }
        SetActorTickEnabled(false);
        FPlatformMisc::RequestExit(false);
    };

    switch (PickupProbeStep)
    {
    case 0:
        if (!WeaponPickup)
        {
            if (PickupProbeElapsedSeconds > 8.0f)
            {
                FinishProbe(false, TEXT("FAIL weapon_pickup_missing"));
            }
            return;
        }

        {
            FVector ApproachLocation = WeaponPickup->GetActorLocation() + FVector(-120.0f, 0.0f, 35.0f);
            ApproachLocation.Z = Player->GetActorLocation().Z;
            Player->SetActorLocation(ApproachLocation, false, nullptr, ETeleportType::TeleportPhysics);
            if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
            {
                Movement->StopMovementImmediately();
            }
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian pickup probe: approach level=%s pickup=%s weapon=%d ammo=%d player=%s radius=%.0f"),
            *Definition.LevelId.ToString(),
            *WeaponPickup->GetActorLocation().ToString(),
            static_cast<int32>(WeaponPickup->PickupWeaponId),
            WeaponPickup->PickupAmmoAmount,
            *Player->GetActorLocation().ToString(),
            WeaponPickup->AutoPickupRadius);
        PickupProbeStep = 1;
        PickupProbeElapsedSeconds = 0.0f;
        return;

    case 1:
        if (IsValid(WeaponPickup))
        {
            if (PickupProbeElapsedSeconds > 5.0f)
            {
                FinishProbe(false, TEXT("FAIL weapon_not_auto_collected"));
            }
            return;
        }

        {
            const TArray<EBMWeaponId> Loadout = Player->GetLoadoutWeaponIds();
            const bool bWeaponGranted = Loadout.Contains(Definition.PickupWeaponId);
            const bool bWeaponEquipped = Player->GetCurrentWeapon() && Player->GetCurrentWeapon()->GetWeaponId() == Definition.PickupWeaponId;
            const int32 ReserveAmmo = Player->GetCurrentWeapon() ? Player->GetCurrentWeapon()->GetReserveAmmo() : 0;
            const int32 ExpectedBonusAmmo = 90 + static_cast<int32>(CampaignLevel) * 20;
            const bool bAmmoGranted = bWeaponEquipped && ReserveAmmo >= ExpectedBonusAmmo;
            if (!bWeaponGranted || !bWeaponEquipped || !bAmmoGranted)
            {
                FinishProbe(
                    false,
                    FString::Printf(
                        TEXT("FAIL grant weapon=%s equipped=%s ammo=%s loadout=%d reserve=%d expected_min=%d"),
                        bWeaponGranted ? TEXT("yes") : TEXT("no"),
                        bWeaponEquipped ? TEXT("yes") : TEXT("no"),
                        bAmmoGranted ? TEXT("yes") : TEXT("no"),
                        Loadout.Num(),
                        ReserveAmmo,
                        ExpectedBonusAmmo));
                return;
            }

            FinishProbe(
                true,
                FString::Printf(
                    TEXT("PASS level=%s pickup_destroyed=yes weapon=%d granted=yes equipped=yes ammo_granted=yes reserve=%d bonus_min=%d loadout=%d"),
                    *Definition.LevelId.ToString(),
                    static_cast<int32>(Definition.PickupWeaponId),
                    ReserveAmmo,
                    ExpectedBonusAmmo,
                    Loadout.Num()));
        }
        return;

    default:
        return;
    }
}

void ABMGameMode::RunInteractionAcceptanceProbe()
{
    if (!GetWorld() || !GreyboxDirector || !MissionDirector)
    {
        return;
    }

    if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
    {
        // The ordinary mission menu pauses a local session before the pawn is
        // ready. This probe is intentionally driven from GameMode::Tick so it
        // can release that presentation pause before using the real gameplay
        // interaction and AI paths.
        Controller->SetPause(false);
    }

    ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (!Player)
    {
        return;
    }

    auto FinishProbe = [this](const bool bPass, const TCHAR* Result)
    {
        if (bPass)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian interaction acceptance probe: %s"),
                Result);
        }
        else
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Black Meridian interaction acceptance probe: %s"),
                Result);
        }
        SetActorTickEnabled(false);
        FPlatformMisc::RequestExit(false);
    };

    switch (InteractionAcceptanceProbeStep)
    {
    case 0:
    {
        if (!bInteractionAcceptanceProbeWaveSpawned)
        {
            GreyboxDirector->SpawnCombatWave(1, Player->GetActorLocation() + FVector(760.0f, 0.0f, 0.0f));
            bInteractionAcceptanceProbeWaveSpawned = true;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian interaction acceptance probe: spawned cover-test enemy"));
            return;
        }

        ABMEnemyAIController* EnemyController = nullptr;
        for (TActorIterator<ABMEnemyCharacter> Iterator(GetWorld()); Iterator; ++Iterator)
        {
            if (ABMEnemyCharacter* Enemy = *Iterator)
            {
                EnemyController = Cast<ABMEnemyAIController>(Enemy->GetController());
                if (EnemyController)
                {
                    break;
                }
            }
        }
        if (!EnemyController)
        {
            return;
        }

        FVector CoverPoint = FVector::ZeroVector;
        if (!EnemyController->ProbeCoverSelection(Player->GetActorLocation(), CoverPoint))
        {
            FinishProbe(false, TEXT("FAIL cover_selection"));
            return;
        }

        ABMInteractableActor* MountedActor = nullptr;
        for (TActorIterator<ABMInteractableActor> Iterator(GetWorld()); Iterator; ++Iterator)
        {
            if (ABMInteractableActor* Candidate = *Iterator; Candidate->InteractionId == TEXT("MountedVantageGun"))
            {
                MountedActor = Candidate;
                break;
            }
        }
        if (!MountedActor)
        {
            FinishProbe(false, TEXT("FAIL mounted_actor_missing"));
            return;
        }

        const FVector MountedApproach(
            MountedActor->GetActorLocation().X - 220.0f,
            MountedActor->GetActorLocation().Y,
            Player->GetActorLocation().Z);
        Player->SetActorLocation(MountedApproach, false, nullptr, ETeleportType::TeleportPhysics);
        const bool bCanMount = IBMInteractionInterface::Execute_CanInteract(MountedActor, Player);
        if (!bCanMount)
        {
            FinishProbe(false, TEXT("FAIL mounted_can_interact"));
            return;
        }
        IBMInteractionInterface::Execute_Interact(MountedActor, Player);
        if (!Player->IsMountedWeapon())
        {
            FinishProbe(false, TEXT("FAIL mounted_enter"));
            return;
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian interaction acceptance probe: mounted enter=pass actor=%s"),
            *GetNameSafe(MountedActor));
        Player->EndMountedWeapon();
        if (Player->IsMountedWeapon())
        {
            FinishProbe(false, TEXT("FAIL mounted_exit"));
            return;
        }

        InteractionAcceptanceProbeStep = 1;
        break;
    }

    case 1:
    {
        ABMArtDirectedProp* RelayProp = nullptr;
        for (TActorIterator<ABMArtDirectedProp> Iterator(GetWorld()); Iterator; ++Iterator)
        {
            if (ABMArtDirectedProp* Candidate = *Iterator;
                Candidate->ActorHasTag(TEXT("Objective.RelayBreach")))
            {
                RelayProp = Candidate;
                break;
            }
        }
        if (!RelayProp || !RelayProp->IsDestructible())
        {
            FinishProbe(false, TEXT("FAIL destructible_relay_missing"));
            return;
        }
        GreyboxDirector->TriggerEnvironmentalEvent();
        if (!RelayProp->IsDestroyed())
        {
            FinishProbe(false, TEXT("FAIL destruction_event"));
            return;
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian interaction acceptance probe: destruction event=pass prop=%s"),
            *GetNameSafe(RelayProp));
        InteractionAcceptanceProbeStep = 2;
        break;
    }

    case 2:
    {
        ABMFriendlySquadMember* Friendly = InteractionAcceptanceProbeFriendly.Get();
        if (!Friendly)
        {
            for (TActorIterator<ABMFriendlySquadMember> Iterator(GetWorld()); Iterator; ++Iterator)
            {
                if (ABMFriendlySquadMember* Candidate = *Iterator;
                    IsValid(Candidate) && !Candidate->IsEliminated())
                {
                    Friendly = Candidate;
                    InteractionAcceptanceProbeFriendly = Candidate;
                    break;
                }
            }
        }
        if (!Friendly || !Friendly->HealthComponent)
        {
            FinishProbe(false, TEXT("FAIL revive_friendly_missing"));
            return;
        }

        if (!bInteractionAcceptanceProbeReviveSetup)
        {
            FBMDamageContext DamageContext;
            DamageContext.InstigatorActor = Player;
            Friendly->HealthComponent->ApplyDamage(1000.0f, DamageContext);
            bInteractionAcceptanceProbeReviveSetup = true;
            return;
        }

        ABMInteractableActor* RevivePrompt = InteractionAcceptanceProbeRevive.Get();
        if (!RevivePrompt)
        {
            for (TActorIterator<ABMInteractableActor> Iterator(GetWorld()); Iterator; ++Iterator)
            {
                if (ABMInteractableActor* Candidate = *Iterator;
                    Candidate->InteractionAction == EBMInteractionAction::ReviveAlly &&
                    Candidate->ReviveTarget == Friendly)
                {
                    RevivePrompt = Candidate;
                    InteractionAcceptanceProbeRevive = Candidate;
                    break;
                }
            }
        }
        if (!RevivePrompt)
        {
            return;
        }

        if (!Player->IsHoldingInteraction())
        {
            FVector Approach = RevivePrompt->GetActorLocation() - FVector(190.0f, 0.0f, 0.0f);
            Approach.Z = Player->GetActorLocation().Z;
            Player->SetActorLocation(Approach, false, nullptr, ETeleportType::TeleportPhysics);
            if (!Player->RunInteractionProbeHold(RevivePrompt))
            {
                FinishProbe(false, TEXT("FAIL revive_hold_start"));
                return;
            }
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian interaction acceptance probe: revive hold started target=%s duration=%.2f"),
                *GetNameSafe(Friendly),
                RevivePrompt->HoldDuration);
        }
        InteractionAcceptanceProbeStep = 3;
        break;
    }

    case 3:
    {
        ABMFriendlySquadMember* Friendly = InteractionAcceptanceProbeFriendly.Get();
        if (!Friendly || Player->IsHoldingInteraction())
        {
            return;
        }
        const bool bRevived = !Friendly->IsEliminated() && Friendly->HealthComponent && Friendly->HealthComponent->IsAlive();
        if (!bRevived)
        {
            FinishProbe(false, TEXT("FAIL revive_restore"));
            return;
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian interaction acceptance probe: revive restore=pass target=%s health=%.1f"),
            *GetNameSafe(Friendly),
            Friendly->HealthComponent->Health);
        FinishProbe(true, TEXT("PASS cover=selected mounted=enter_exit destruction=triggered revive=restored"));
        break;
    }

    default:
        break;
    }
}

void ABMGameMode::RunDestructionPresentationShowcase(const float DeltaSeconds)
{
    if (!GetWorld() || !GreyboxDirector)
    {
        return;
    }

    ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    ABMArtDirectedProp* RelayProp = nullptr;
    for (TActorIterator<ABMArtDirectedProp> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        if (ABMArtDirectedProp* Candidate = *Iterator;
            IsValid(Candidate) && Candidate->ActorHasTag(TEXT("Objective.RelayBreach")))
        {
            RelayProp = Candidate;
            break;
        }
    }

    if (!Player || !RelayProp)
    {
        return;
    }

    if (!bDestructionPresentationShowcaseStarted)
    {
        if (RelayProp->IsDestroyed())
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Black Meridian destruction presentation showcase: FAIL relay_already_destroyed"));
            SetActorTickEnabled(false);
            FPlatformMisc::RequestExit(false);
            return;
        }

        if (APlayerController* Controller = Cast<APlayerController>(Player->GetController()))
        {
            Controller->SetPause(false);
            if (ABMHUD* HUD = Cast<ABMHUD>(Controller->GetHUD()))
            {
                HUD->SetScreen(EBMHUDScreen::InGame);
                HUD->SetPresentationCapture(true);
            }
            if (ABMPlayerController* BMController = Cast<ABMPlayerController>(Controller))
            {
                BMController->RestoreInteractiveInput();
            }
        }

        const FVector TargetLocation = RelayProp->GetActorLocation() + FVector(0.0f, 0.0f, 130.0f);

        // The south-west approach is a valid player route, but it looks
        // through the relay-house barrier and produces a mostly black frame.
        // Use the south-east service lane for the presentation cut: it keeps
        // the cabinet, break aperture, and floor contact in one unobstructed
        // composition while remaining inside the playable map bounds.
        FVector ShowcaseLocation = RelayProp->GetActorLocation() + FVector(700.0f, -160.0f, 0.0f);
        ShowcaseLocation.Z = Player->GetActorLocation().Z;
        const FRotator LookRotation = (TargetLocation - ShowcaseLocation).Rotation();
        Player->SetActorLocationAndRotation(
            ShowcaseLocation,
            FRotator(0.0f, LookRotation.Yaw, 0.0f),
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
        if (APlayerController* Controller = Cast<APlayerController>(Player->GetController()))
        {
            Controller->SetControlRotation(LookRotation);
        }
        if (UCameraComponent* Camera = Player->GetFirstPersonCamera())
        {
            // A slightly tighter development capture keeps the relay's
            // antenna, cabinet face, and falling shards readable without
            // changing the shipped player FOV setting.
            Camera->SetFieldOfView(78.0f);
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian destruction presentation showcase: camera location=%s target=%s rotation=%s fov=78"),
            *ShowcaseLocation.ToString(),
            *TargetLocation.ToString(),
            *LookRotation.ToString());

        GreyboxDirector->TriggerEnvironmentalEvent();
        bDestructionPresentationShowcaseStarted = true;
        DestructionPresentationShowcaseElapsed = 0.0f;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian destruction presentation showcase: break triggered prop=%s location=%s destroyed=%s"),
            *GetNameSafe(RelayProp),
            *RelayProp->GetActorLocation().ToString(),
            RelayProp->IsDestroyed() ? TEXT("yes") : TEXT("no"));
        return;
    }

    DestructionPresentationShowcaseElapsed += FMath::Max(0.0f, DeltaSeconds);
    if (!bDestructionPresentationShowcaseScreenshotRequested && DestructionPresentationShowcaseElapsed >= 0.22f)
    {
        FString ScreenshotPath = FPaths::ProjectDir() / TEXT("artifacts/destructible-cover-burst-20260802.png");
        FParse::Value(FCommandLine::Get(), TEXT("BM_DestructionScreenshot="), ScreenshotPath);
        FScreenshotRequest::RequestScreenshot(
            ScreenshotPath,
            false,
            false,
            false,
            FIntRect(),
            true);
        bDestructionPresentationShowcaseScreenshotRequested = true;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian destruction presentation showcase: screenshot requested path=%s"),
            *ScreenshotPath);
    }
    if (!bDestructionPresentationShowcaseReset && DestructionPresentationShowcaseElapsed >= 1.65f)
    {
        RelayProp->ResetProp();
        bDestructionPresentationShowcaseReset = true;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian destruction presentation showcase: checkpoint reset prop=%s restored=%s"),
            *GetNameSafe(RelayProp),
            RelayProp->IsDestroyed() ? TEXT("no") : TEXT("yes"));
    }

    if (bDestructionPresentationShowcaseReset
        && !bDestructionPresentationShowcaseResetScreenshotRequested
        && DestructionPresentationShowcaseElapsed >= 1.82f)
    {
        FString ScreenshotPath = FPaths::ProjectDir() / TEXT("artifacts/destructible-cover-reset-20260802.png");
        FParse::Value(FCommandLine::Get(), TEXT("BM_DestructionResetScreenshot="), ScreenshotPath);
        FScreenshotRequest::RequestScreenshot(
            ScreenshotPath,
            false,
            false,
            false,
            FIntRect(),
            true);
        bDestructionPresentationShowcaseResetScreenshotRequested = true;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian destruction presentation showcase: reset screenshot requested path=%s"),
            *ScreenshotPath);
    }

    if (DestructionPresentationShowcaseElapsed >= 3.2f)
    {
        const bool bPass = bDestructionPresentationShowcaseReset && !RelayProp->IsDestroyed();
        if (bPass)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian destruction presentation showcase: PASS break=triggered reset=restored"));
        }
        else
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Black Meridian destruction presentation showcase: FAIL break=%s reset=%s"),
                bDestructionPresentationShowcaseStarted && RelayProp->IsDestructible() ? TEXT("triggered") : TEXT("missing"),
                bDestructionPresentationShowcaseReset && !RelayProp->IsDestroyed() ? TEXT("restored") : TEXT("not_restored"));
        }
        SetActorTickEnabled(false);
        FPlatformMisc::RequestExit(false);
    }
}

void ABMGameMode::RunPersistenceProbe()
{
    // Keep the development-only save/restart probe in this translation unit
    // so the timer armed from BeginPlay cannot resolve against a stale build
    // object after incremental editor rebuilds.
    if (!GetWorld() || !MissionDirector)
    {
        return;
    }

    const bool bWrite = FParse::Param(FCommandLine::Get(), TEXT("BM_PersistenceProbeWrite"));
    const bool bRead = FParse::Param(FCommandLine::Get(), TEXT("BM_PersistenceProbeRead"));
    if (!bWrite && !bRead)
    {
        GetWorld()->GetTimerManager().ClearTimer(PersistenceProbeTimerHandle);
        return;
    }

    ABMGameState* State = GetGameState<ABMGameState>();
    ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (!State || !Player)
    {
        return;
    }

    if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
    {
        Controller->SetPause(false);
    }

    if (bWrite)
    {
        if (!bPersistenceProbePrepared)
        {
            MissionDirector->StartMission();
            bPersistenceProbePrepared = true;
        }

        // Wait for the ordinary deterministic mission clock to reach the same
        // Intelligence checkpoint that a player reaches after the relay
        // breach. The probe only changes the saved values after that real
        // phase transition; it does not bypass SaveCheckpoint's gameplay path.
        if (State->MissionPhase != EBMMissionPhase::Intelligence)
        {
            return;
        }

        const TArray<EBMWeaponId> ProbeLoadout = {EBMWeaponId::ARX4, EBMWeaponId::RookDMR};
        Player->RestoreLoadoutFromIds(ProbeLoadout, EBMWeaponId::RookDMR);
        Player->SetActorLocationAndRotation(
            FVector(610.0f, -140.0f, 85.0f),
            FRotator(0.0f, 125.0f, 0.0f),
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
        if (Player->HealthComponent)
        {
            Player->HealthComponent->SetHealthForCheckpoint(63.0f);
        }
        Player->RestoreThrowableInventory(1, 0);
        State->CheckpointIndex = 7;
        MissionDirector->bOptionalIntelSecured = true;
        MissionDirector->bSignalJammerPlanted = true;
        MissionDirector->SaveCheckpoint();
        RecordCampaignCompletion();

        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian persistence probe: WRITE PASS phase=%d checkpoint=%d health=%.1f loadout=%d current=%d frag=%d veil=%d intel=%s jammer=%s"),
            static_cast<int32>(State->MissionPhase),
            State->CheckpointIndex,
            Player->HealthComponent ? Player->HealthComponent->Health : -1.0f,
            Player->GetLoadoutWeaponIds().Num(),
            static_cast<int32>(Player->GetCurrentWeaponId()),
            Player->GetFragCount(),
            Player->GetVeilCount(),
            MissionDirector->bOptionalIntelSecured ? TEXT("yes") : TEXT("no"),
            MissionDirector->bSignalJammerPlanted ? TEXT("yes") : TEXT("no"));
    }
    else
    {
        const bool bLoaded = MissionDirector->LoadCheckpoint();
        const FVector ExpectedLocation(610.0f, -140.0f, 85.0f);
        const TArray<EBMWeaponId> LoadedLoadout = Player->GetLoadoutWeaponIds();
        const bool bRawFieldsMatch = bLoaded &&
            State->CheckpointIndex == 7 &&
            MissionDirector->bOptionalIntelSecured &&
            MissionDirector->bSignalJammerPlanted &&
            Player->HealthComponent && FMath::IsNearlyEqual(Player->HealthComponent->Health, 63.0f, 0.1f) &&
            LoadedLoadout.Contains(EBMWeaponId::ARX4) &&
            LoadedLoadout.Contains(EBMWeaponId::RookDMR) &&
            Player->GetCurrentWeaponId() == EBMWeaponId::RookDMR &&
            Player->GetFragCount() == 1 &&
            Player->GetVeilCount() == 0 &&
            FVector::DistSquared(Player->GetActorLocation(), ExpectedLocation) < FMath::Square(2.0f);

        // Exercise the real restart path after inspecting the raw load. The
        // mission director restores the saved actor state, resets stale world
        // actors, and resumes the saved Intelligence checkpoint.
        MissionDirector->RestartFromCheckpoint();
        const bool bRestartedAtCheckpoint =
            State->MissionPhase == EBMMissionPhase::Intelligence &&
            FVector::DistSquared(Player->GetActorLocation(), ExpectedLocation) < FMath::Square(2.0f);
        const bool bPass = bRawFieldsMatch && bRestartedAtCheckpoint;

        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian persistence probe: READ %s raw=%s restart=%s phase=%d checkpoint=%d health=%.1f loadout=%d current=%d frag=%d veil=%d intel=%s jammer=%s"),
            bPass ? TEXT("PASS") : TEXT("FAIL"),
            bRawFieldsMatch ? TEXT("pass") : TEXT("fail"),
            bRestartedAtCheckpoint ? TEXT("pass") : TEXT("fail"),
            static_cast<int32>(State->MissionPhase),
            State->CheckpointIndex,
            Player->HealthComponent ? Player->HealthComponent->Health : -1.0f,
            Player->GetLoadoutWeaponIds().Num(),
            static_cast<int32>(Player->GetCurrentWeaponId()),
            Player->GetFragCount(),
            Player->GetVeilCount(),
            MissionDirector->bOptionalIntelSecured ? TEXT("yes") : TEXT("no"),
            MissionDirector->bSignalJammerPlanted ? TEXT("yes") : TEXT("no"));
    }

    GetWorld()->GetTimerManager().ClearTimer(PersistenceProbeTimerHandle);
    FPlatformMisc::RequestExit(false);
}

bool ABMGameMode::SpawnThrowableProbeGrenade(const EBMWeaponId ThrowableId)
{
    if (!GetWorld())
    {
        return false;
    }

    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    ABMPlayerCharacter* Thrower = Controller
        ? Cast<ABMPlayerCharacter>(Controller->GetPawn())
        : nullptr;
    ABMPlayerCharacter* Target = ThrowableProbeTarget.Get();
    if (!Thrower || !Target)
    {
        return false;
    }

    UBMThrowableDataAsset* Profile = UBMThrowableDataAsset::LoadContentProfile(this, ThrowableId);
    if (!Profile || Profile->ThrowableId != ThrowableId)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Black Meridian throwable probe: profile load failed id=%d"),
            static_cast<int32>(ThrowableId));
        return false;
    }

    // Keep the target inside the profile radius while leaving enough falloff
    // for the frag to prove damage without eliminating it before the Veil
    // pass. The zero throw direction keeps this probe focused on the fuse
    // and detonation contract rather than projectile travel.
    const FTransform SpawnTransform(
        FRotator::ZeroRotator,
        Target->GetActorLocation() - FVector(180.0f, 0.0f, 0.0f));
    ABMThrowable* Throwable = GetWorld()->SpawnActorDeferred<ABMThrowable>(
        ABMThrowable::StaticClass(),
        SpawnTransform,
        Thrower,
        Thrower,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Throwable)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Black Meridian throwable probe: spawn failed id=%d"),
            static_cast<int32>(ThrowableId));
        return false;
    }

    Throwable->ThrowableId = ThrowableId;
    Throwable->InitializeFromData(Profile);
    UGameplayStatics::FinishSpawningActor(Throwable, SpawnTransform);
    Throwable->ArmAndThrow(FVector::ZeroVector, 0.0f);

    ThrowableProbeActive = Throwable;
    ThrowableProbeExpectedFuse = Profile->FuseSeconds;
    ThrowableProbeDetonationStart = GetWorld()->GetTimeSeconds();
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian throwable probe: spawned id=%d profile=%s fuse_asset=%.2f damage=%.1f suppression=%.1f radius=%.1f"),
        static_cast<int32>(ThrowableId),
        *Profile->GetPathName(),
        Profile->FuseSeconds,
        Profile->Damage,
        Profile->SuppressionAmount,
        Profile->Radius);
    return true;
}

void ABMGameMode::RunThrowablePresentationShowcase()
{
    if (bThrowablePresentationShowcaseStarted || !GetWorld())
    {
        return;
    }

    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    ABMPlayerCharacter* Player = Controller
        ? Cast<ABMPlayerCharacter>(Controller->GetPawn())
        : nullptr;
    if (!Player || !Player->IsLocallyControlled())
    {
        return;
    }

    // The normal mission entry path leaves the local controller paused behind
    // the briefing/menu screen. This is a rendered development showcase, so
    // release that pause and put the HUD into the same gameplay state used by
    // the combat showcase before showing the first-person throwable.
    if (ABMPlayerController* PlayerController = Cast<ABMPlayerController>(Controller))
    {
        if (ABMHUD* HUD = Cast<ABMHUD>(PlayerController->GetHUD()))
        {
            HUD->SetScreen(EBMHUDScreen::InGame);
        }
        PlayerController->SetPause(false);
    }

    const EBMWeaponId ShowcaseId = FParse::Param(
        FCommandLine::Get(),
        TEXT("BM_ThrowablePresentationShowcaseVeil"))
        ? EBMWeaponId::VeilGrenade
        : EBMWeaponId::FragGrenade;
    Player->RunThrowablePresentationShowcase(ShowcaseId);
    bThrowablePresentationShowcaseStarted = true;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian throwable presentation showcase: started id=%d"),
        static_cast<int32>(ShowcaseId));
}

void ABMGameMode::RunThrowableInputProbe()
{
    if (!GetWorld())
    {
        return;
    }

    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    ABMPlayerCharacter* Player = Controller
        ? Cast<ABMPlayerCharacter>(Controller->GetPawn())
        : nullptr;
    if (!Controller || !Player || !Player->IsLocallyControlled() || !Player->GetFirstPersonCamera())
    {
        return;
    }

    ThrowableInputProbeElapsed += FMath::Max(GetWorld()->GetDeltaSeconds(), 0.0f);

    auto FinishProbe = [this](const bool bPass, const TCHAR* Result)
    {
        if (bPass)
        {
            UE_LOG(LogTemp, Display, TEXT("Black Meridian throwable input probe: %s"), Result);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Black Meridian throwable input probe: %s"), Result);
        }
        SetActorTickEnabled(false);
        FPlatformMisc::RequestExit(false);
    };

    switch (ThrowableInputProbeStep)
    {
    case 0:
    {
        // A normal mission starts behind a paused briefing screen. Release it
        // for this standalone probe so the pawn's input component is evaluated
        // exactly as it is after the player presses Enter to deploy.
        Controller->SetPause(false);
        if (ABMPlayerController* BMController = Cast<ABMPlayerController>(Controller))
        {
            if (ABMHUD* HUD = Cast<ABMHUD>(BMController->GetHUD()))
            {
                HUD->SetScreen(EBMHUDScreen::InGame);
            }
            BMController->RestoreInteractiveInput();
        }

        ThrowableInputProbeFragBefore = Player->GetFragCount();
        if (ThrowableInputProbeFragBefore <= 0)
        {
            FinishProbe(false, TEXT("FAIL player started without a Frag grenade"));
            return;
        }

        const bool bQueued = Controller->InputKey(
            FInputKeyEventArgs::CreateSimulated(EKeys::G, IE_Pressed, 1.0f));
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian throwable input probe: queued key=G event=Pressed accepted=%s frag_before=%d paused=%s"),
            bQueued ? TEXT("yes") : TEXT("no"),
            ThrowableInputProbeFragBefore,
            GetWorld()->IsPaused() ? TEXT("yes") : TEXT("no"));
        ThrowableInputProbeStep = 1;
        ThrowableInputProbeElapsed = 0.0f;
        break;
    }

    case 1:
    {
        if (Player->GetFragCount() < ThrowableInputProbeFragBefore)
        {
            for (TActorIterator<ABMThrowable> Iterator(GetWorld()); Iterator; ++Iterator)
            {
                ABMThrowable* Candidate = *Iterator;
                if (Candidate && Candidate->GetOwner() == Player && Candidate->ThrowableId == EBMWeaponId::FragGrenade)
                {
                    ThrowableInputProbeActive = Candidate;
                    break;
                }
            }

            if (ThrowableInputProbeActive.IsValid())
            {
                FinishProbe(
                    true,
                    TEXT("PASS PlayerController::InputKey(G) -> Frag action -> ThrowFrag -> server spawn; inventory decremented"));
            }
            else if (ThrowableInputProbeElapsed > 1.0f)
            {
                FinishProbe(false, TEXT("FAIL Frag count changed but no player-owned throwable spawned"));
            }
        }
        else if (ThrowableInputProbeElapsed > 2.0f)
        {
            FinishProbe(false, TEXT("FAIL simulated G did not reach the player Frag binding"));
        }
        break;
    }

    default:
        break;
    }
}

void ABMGameMode::RunReloadInputProbe()
{
    if (!GetWorld())
    {
        return;
    }

    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    ABMPlayerCharacter* Player = Controller
        ? Cast<ABMPlayerCharacter>(Controller->GetPawn())
        : nullptr;
    ABMWeapon* Weapon = Player ? Player->GetCurrentWeapon() : nullptr;
    if (!Controller || !Player || !Weapon || !Player->IsLocallyControlled())
    {
        return;
    }

    ReloadInputProbeElapsed += FMath::Max(GetWorld()->GetDeltaSeconds(), 0.0f);

    auto FinishProbe = [this](const bool bPass, const FString& Result)
    {
        if (bPass)
        {
            UE_LOG(LogTemp, Display, TEXT("Black Meridian reload input probe: %s"), *Result);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Black Meridian reload input probe: %s"), *Result);
        }
        SetActorTickEnabled(false);
        FPlatformMisc::RequestExit(false);
    };

    auto QueueReloadInput = [this, Controller, Weapon](const bool bEmptyReload)
    {
        ReloadInputProbeMagazineBefore = bEmptyReload ? 0 : FMath::Max(1, ReloadInputProbeMagazineCapacity - 1);
        ReloadInputProbeReserveBefore = 150;
        if (!Weapon->SetCombatProbeAmmo(ReloadInputProbeMagazineBefore, ReloadInputProbeReserveBefore))
        {
            return false;
        }

        ReloadInputProbeExpectedDuration = Weapon->GetWeaponData()
            ? (bEmptyReload
                ? Weapon->GetWeaponData()->Balance.EmptyReloadDuration
                : Weapon->GetWeaponData()->Balance.ReloadDuration)
            : 0.0f;
        ReloadInputProbeStartTime = GetWorld()->GetTimeSeconds();
        const bool bQueued = Controller->InputKey(
            FInputKeyEventArgs::CreateSimulated(EKeys::R, IE_Pressed, 1.0f));
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian reload input probe: queued key=R event=Pressed accepted=%s empty=%s magazine_before=%d reserve_before=%d expected_duration=%.2f"),
            bQueued ? TEXT("yes") : TEXT("no"),
            bEmptyReload ? TEXT("yes") : TEXT("no"),
            ReloadInputProbeMagazineBefore,
            ReloadInputProbeReserveBefore,
            ReloadInputProbeExpectedDuration);
        return bQueued;
    };

    switch (ReloadInputProbeStep)
    {
    case 0:
    {
        // The ordinary mission opens behind a paused briefing screen. Release
        // it before injecting R so the character input component sees the key
        // exactly as it does after the player presses Enter to deploy.
        Controller->SetPause(false);
        if (ABMPlayerController* BMController = Cast<ABMPlayerController>(Controller))
        {
            if (ABMHUD* HUD = Cast<ABMHUD>(BMController->GetHUD()))
            {
                HUD->SetScreen(EBMHUDScreen::InGame);
            }
            BMController->RestoreInteractiveInput();
        }

        ReloadInputProbeMagazineCapacity = Weapon->GetWeaponData()
            ? Weapon->GetWeaponData()->Balance.MagazineSize
            : 0;
        if (ReloadInputProbeMagazineCapacity <= 1 || !Weapon->GetWeaponData())
        {
            FinishProbe(false, TEXT("FAIL current weapon has no usable magazine profile"));
            return;
        }

        if (!QueueReloadInput(false))
        {
            FinishProbe(false, TEXT("FAIL PlayerController::InputKey rejected simulated R"));
            return;
        }
        ReloadInputProbeStep = 1;
        ReloadInputProbeElapsed = 0.0f;
        break;
    }

    case 1:
    {
        if (!Weapon->IsReloading() && Weapon->GetMagazineAmmo() > ReloadInputProbeMagazineBefore)
        {
            const float ActualDuration = GetWorld()->GetTimeSeconds() - ReloadInputProbeStartTime;
            const bool bTimingPass = ReloadInputProbeExpectedDuration <= 0.0f
                || FMath::Abs(ActualDuration - ReloadInputProbeExpectedDuration) <= 0.12f;
            if (!bTimingPass)
            {
                FinishProbe(
                    false,
                    FString::Printf(
                        TEXT("FAIL normal R reload completed with timing mismatch expected=%.2f actual=%.2f"),
                        ReloadInputProbeExpectedDuration,
                        ActualDuration));
                return;
            }

            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian reload input probe: normal R reload completed magazine=%d reserve=%d expected=%.2f actual=%.2f"),
                Weapon->GetMagazineAmmo(),
                Weapon->GetReserveAmmo(),
                ReloadInputProbeExpectedDuration,
                ActualDuration);

            if (!bReloadInputProbeEmpty)
            {
                FinishProbe(
                    true,
                    FString::Printf(
                        TEXT("PASS PlayerController::InputKey(R) -> Reload action -> ammo restored normal=%.2fs"),
                        ActualDuration));
                return;
            }

            ReloadInputProbeStep = 2;
            ReloadInputProbeElapsed = 0.0f;
        }
        else if (ReloadInputProbeElapsed > 2.0f)
        {
            FinishProbe(false, TEXT("FAIL simulated R did not complete the normal reload"));
        }
        break;
    }

    case 2:
        if (!QueueReloadInput(true))
        {
            FinishProbe(false, TEXT("FAIL PlayerController::InputKey rejected simulated R for empty reload"));
            return;
        }
        ReloadInputProbeStep = 3;
        ReloadInputProbeElapsed = 0.0f;
        break;

    case 3:
    {
        if (!Weapon->IsReloading()
            && Weapon->GetMagazineAmmo() == ReloadInputProbeMagazineCapacity
            && Weapon->GetReserveAmmo() < ReloadInputProbeReserveBefore)
        {
            const float ActualDuration = GetWorld()->GetTimeSeconds() - ReloadInputProbeStartTime;
            const bool bTimingPass = ReloadInputProbeExpectedDuration <= 0.0f
                || FMath::Abs(ActualDuration - ReloadInputProbeExpectedDuration) <= 0.12f;
            if (!bTimingPass)
            {
                FinishProbe(
                    false,
                    FString::Printf(
                        TEXT("FAIL empty R reload completed with timing mismatch expected=%.2f actual=%.2f"),
                        ReloadInputProbeExpectedDuration,
                        ActualDuration));
                return;
            }

            FinishProbe(
                true,
                FString::Printf(
                    TEXT("PASS PlayerController::InputKey(R) -> Reload action -> ammo restored empty=%.2fs normal_and_empty=yes"),
                    ActualDuration));
        }
        else if (ReloadInputProbeElapsed > 2.5f)
        {
            FinishProbe(false, TEXT("FAIL simulated R did not complete the empty reload"));
        }
        break;
    }

    default:
        break;
    }
}

void ABMGameMode::RunThrowableProbe()
{
    if (!GetWorld())
    {
        return;
    }

    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    if (Controller)
    {
        // Resolve and clear the pause before asking for the pawn. In the
        // normal mission flow ShowMissionMenu pauses the world, which would
        // otherwise prevent this timer from ever reaching the spawn step.
        Controller->SetPause(false);
    }
    ABMPlayerCharacter* Player = Controller
        ? Cast<ABMPlayerCharacter>(Controller->GetPawn())
        : nullptr;
    if (!Player)
    {
        return;
    }

    auto FinishProbe = [this](const bool bPass, const TCHAR* Result)
    {
        if (bPass)
        {
            UE_LOG(LogTemp, Display, TEXT("Black Meridian throwable probe: %s"), Result);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Black Meridian throwable probe: %s"), Result);
        }
        GetWorld()->GetTimerManager().ClearTimer(ThrowableProbeTimerHandle);
        if (ThrowableProbeTarget.IsValid())
        {
            ThrowableProbeTarget->Destroy();
        }
        if (ThrowableProbeActive.IsValid())
        {
            ThrowableProbeActive->Destroy();
        }
        FPlatformMisc::RequestExit(false);
    };

    ABMPlayerCharacter* Target = ThrowableProbeTarget.Get();
    switch (ThrowableProbeStep)
    {
    case 0:
        if (!Target)
        {
            FActorSpawnParameters SpawnParameters;
            SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Target = GetWorld()->SpawnActor<ABMPlayerCharacter>(
                ABMPlayerCharacter::StaticClass(),
                FVector(220.0f, 0.0f, 100.0f),
                FRotator::ZeroRotator,
                SpawnParameters);
            ThrowableProbeTarget = Target;
            if (Target)
            {
                Target->SetTeam(EBMTeam::Vanta);
                if (Target->GetCharacterMovement())
                {
                    Target->GetCharacterMovement()->StopMovementImmediately();
                    Target->GetCharacterMovement()->DisableMovement();
                }
                if (Target->HealthComponent)
                {
                    Target->HealthComponent->bRegenerateHealth = false;
                    Target->HealthComponent->SetHealthForCheckpoint(100.0f);
                }
                if (Target->SuppressionComponent)
                {
                    Target->SuppressionComponent->Suppression = 0.0f;
                }
            }
        }

        if (!Target || !Target->HealthComponent || !Target->SuppressionComponent)
        {
            FinishProbe(false, TEXT("FAIL target setup"));
            return;
        }

        ThrowableProbeFragHealthBefore = Target->HealthComponent->Health;
        if (SpawnThrowableProbeGrenade(EBMWeaponId::FragGrenade))
        {
            ThrowableProbeStep = 1;
        }
        else
        {
            FinishProbe(false, TEXT("FAIL frag spawn"));
        }
        break;

    case 1:
        if (!Target || !Target->HealthComponent)
        {
            FinishProbe(false, TEXT("FAIL frag target lost"));
            return;
        }

        if (Target->HealthComponent->Health < ThrowableProbeFragHealthBefore - 0.1f)
        {
            const float ActualFuse = GetWorld()->GetTimeSeconds() - ThrowableProbeDetonationStart;
            const bool bFuseTiming = ActualFuse >= ThrowableProbeExpectedFuse - 0.25f &&
                ActualFuse <= ThrowableProbeExpectedFuse + 0.75f;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian throwable probe: frag detonated profile_fuse=%.2f actual_fuse=%.2f health_before=%.1f health_after=%.1f timing=%s"),
                ThrowableProbeExpectedFuse,
                ActualFuse,
                ThrowableProbeFragHealthBefore,
                Target->HealthComponent->Health,
                bFuseTiming ? TEXT("pass") : TEXT("fail"));
            if (!bFuseTiming || Target->HealthComponent->Health <= 0.0f)
            {
                FinishProbe(false, TEXT("FAIL frag damage or fuse timing"));
                return;
            }

            Target->HealthComponent->SetHealthForCheckpoint(100.0f);
            Target->SuppressionComponent->Suppression = 0.0f;
            if (SpawnThrowableProbeGrenade(EBMWeaponId::VeilGrenade))
            {
                ThrowableProbeStep = 2;
            }
            else
            {
                FinishProbe(false, TEXT("FAIL Veil spawn"));
            }
        }
        else if (GetWorld()->GetTimeSeconds() - ThrowableProbeDetonationStart > ThrowableProbeExpectedFuse + 1.5f)
        {
            FinishProbe(false, TEXT("FAIL frag did not detonate"));
        }
        break;

    case 2:
        if (!Target || !Target->SuppressionComponent)
        {
            FinishProbe(false, TEXT("FAIL Veil target lost"));
            return;
        }

        if (Target->SuppressionComponent->Suppression > 0.1f)
        {
            const float ActualFuse = GetWorld()->GetTimeSeconds() - ThrowableProbeDetonationStart;
            const bool bFuseTiming = ActualFuse >= ThrowableProbeExpectedFuse - 0.25f &&
                ActualFuse <= ThrowableProbeExpectedFuse + 0.75f;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian throwable probe: Veil detonated profile_fuse=%.2f actual_fuse=%.2f suppression=%.1f timing=%s"),
                ThrowableProbeExpectedFuse,
                ActualFuse,
                Target->SuppressionComponent->Suppression,
                bFuseTiming ? TEXT("pass") : TEXT("fail"));
            FinishProbe(
                bFuseTiming,
                bFuseTiming ? TEXT("PASS frag_damage=pass veil_suppression=pass fuse_assets=pass") : TEXT("FAIL Veil fuse timing"));
        }
        else if (GetWorld()->GetTimeSeconds() - ThrowableProbeDetonationStart > ThrowableProbeExpectedFuse + 1.5f)
        {
            FinishProbe(false, TEXT("FAIL Veil did not apply suppression"));
        }
        break;

    default:
        break;
    }
}
#endif

void ABMGameMode::RespawnPlayer(AController* Controller)
{
    if (!HasAuthority() || !bRespawnPlayers || !Controller)
    {
        return;
    }
    UE_LOG(LogTemp, Display, TEXT("Black Meridian combat: respawning controller=%s old_pawn=%s"), *GetNameSafe(Controller), *GetNameSafe(Controller->GetPawn()));
    if (APawn* OldPawn = Controller->GetPawn())
    {
        Controller->UnPossess();
        OldPawn->Destroy();
    }
    RestartPlayer(Controller);
    UE_LOG(LogTemp, Display, TEXT("Black Meridian combat: respawn complete controller=%s new_pawn=%s"), *GetNameSafe(Controller), *GetNameSafe(Controller->GetPawn()));
    if (ABMPlayerController* BMController = Cast<ABMPlayerController>(Controller))
    {
        BMController->ShowRespawnedPlayer();
    }
}
