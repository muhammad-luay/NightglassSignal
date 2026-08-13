// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMFriendlySquadMember.h"

#include "BMPlayerCharacter.h"
#include "BMPlayerController.h"
#include "BMCombatTargeting.h"
#include "BMEnemyCharacter.h"
#include "BMGameState.h"
#include "BMHealthComponent.h"
#include "BMInteractableActor.h"
#include "BMSuppressionComponent.h"
#include "CollisionQueryParams.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Perception/AISense_Hearing.h"

ABMFriendlySquadMember::ABMFriendlySquadMember(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    Team = EBMTeam::Lattice;
    PrimaryActorTick.bCanEverTick = true;
    Callsign = NSLOCTEXT("BlackMeridian", "FriendlyCallsign", "Mako");
    HealthComponent->bRegenerateHealth = false;
    GetCharacterMovement()->MaxWalkSpeed = 330.0f;
    AutoPossessAI = EAutoPossessAI::Disabled;
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ABMFriendlySquadMember::BeginPlay()
{
    Super::BeginPlay();
    ConfigurePlaceholderBody(FColor(30, 90, 125));
    PlayerTarget = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
}

void ABMFriendlySquadMember::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING
    // Full-campaign verification is concerned with the player's physical
    // traversal, player weapon damage, mission holds, and extraction. The
    // friendly squad's cover-selection scan is intentionally excluded from
    // that harness because it walks every runtime prop on every support tick
    // and can starve the game thread on the densest generated layouts. Normal
    // campaign launches do not carry this flag and retain squad support.
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_FullCampaignProbe")))
    {
        return;
    }
#endif
    AssistCooldown = FMath::Max(0.0f, AssistCooldown - DeltaSeconds);
    FireCooldown = FMath::Max(0.0f, FireCooldown - DeltaSeconds);
    if (!PlayerTarget.IsValid())
    {
        PlayerTarget = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    }
    if (!HasAuthority() || bEliminated || !PlayerTarget.IsValid())
    {
        return;
    }

    AssistDownedSquadmate();

    FVector Desired = PlayerTarget->GetActorLocation() + PlayerTarget->GetActorRotation().RotateVector(FormationOffset);
    const FVector ObjectiveAnchor = GetObjectiveAnchor();
    if (!ObjectiveAnchor.IsNearlyZero() && FVector::DistSquared2D(GetActorLocation(), ObjectiveAnchor) > FMath::Square(260.0f))
    {
        Desired = ObjectiveAnchor + PlayerTarget->GetActorRotation().RotateVector(FormationOffset * 0.35f);
    }

    if (FVector::DistSquared2D(GetActorLocation(), PlayerTarget->GetActorLocation()) < FMath::Square(130.0f))
    {
        Desired = PlayerTarget->GetActorLocation() - PlayerTarget->GetActorForwardVector() * 170.0f + FVector(0.0f, FormationOffset.Y, 0.0f);
    }
    const FVector Offset = Desired - GetActorLocation();
    if (Offset.SizeSquared2D() > FMath::Square(180.0f))
    {
        AddMovementInput(Offset.GetSafeNormal2D(), FMath::Clamp(Offset.Size2D() / 500.0f, 0.0f, 1.0f));
    }

    if (AssistCooldown <= 0.0f && PlayerTarget->HealthComponent && PlayerTarget->HealthComponent->GetHealthPercent() < 0.35f)
    {
        AssistPlayer();
    }
    EngageVisibleEnemy();
}

void ABMFriendlySquadMember::AssistPlayer()
{
    if (!HasAuthority() || !PlayerTarget.IsValid() || AssistCooldown > 0.0f)
    {
        return;
    }

    // Squad support is an emergency nudge rather than a second health bar.
    // Keep it small and infrequent so sustained hostile fire can still finish
    // an exposed player.
    PlayerTarget->HealthComponent->RestoreHealth(2.0f);
    AssistCooldown = 45.0f;
    if (ABMPlayerController* Controller = Cast<ABMPlayerController>(PlayerTarget->GetController()))
    {
        Controller->ShowSubtitle(FText::Format(NSLOCTEXT("BlackMeridian", "AssistLine", "{0}: Patch up. Keep moving."), Callsign), 2.5f);
    }
}

void ABMFriendlySquadMember::EngageVisibleEnemy()
{
    if (!HasAuthority() || bEliminated || FireCooldown > 0.0f || !GetWorld())
    {
        return;
    }

#if !UE_BUILD_SHIPPING
    // Passive combat captures are intended to isolate enemy locomotion and
    // target-facing presentation. Friendly support fire would still apply
    // damage and replay hit-reaction montages on the advancing enemies,
    // making a clean movement recording look like route jitter. Normal
    // gameplay has no passive flag and keeps the squad's support fire.
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_PassiveCombatCapture")))
    {
        return;
    }

    // The aim-validation probe owns a stationary enemy and measures only the
    // player's weapon path. Friendly support is a separate damage source and
    // would make a near-miss health sample nondeterministic if its 1.1 second
    // cadence happens to land between the probe's staged shots. Keep this
    // development-only harness isolated; ordinary campaign gameplay retains
    // squad support fire.
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_AimValidationProbe")))
    {
        return;
    }
#endif

    ABMEnemyCharacter* BestTarget = nullptr;
    float BestDistanceSquared = FMath::Square(EngagementRange);
    for (TActorIterator<ABMEnemyCharacter> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        ABMEnemyCharacter* Candidate = *Iterator;
        if (!Candidate || Candidate->IsEliminated())
        {
            continue;
        }
        const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
        if (DistanceSquared < BestDistanceSquared)
        {
            BestTarget = Candidate;
            BestDistanceSquared = DistanceSquared;
        }
    }

    if (!BestTarget)
    {
        CurrentEnemyTarget.Reset();
        return;
    }

    // Squad fire is support for the player's current engagement, not a hidden
    // second hitscan system that can clear enemies outside the reticle. The
    // same live-hostile resolver used by the player's weapon and HUD must
    // resolve this exact enemy before a friendly can spend a support shot.
    // Missing player view data is not permission to fall back to the nearest
    // enemy; support fire fails closed during possession or camera teardown.
    if (!PlayerTarget.IsValid() || !PlayerTarget->GetFirstPersonCamera())
    {
        CurrentEnemyTarget.Reset();
#if !UE_BUILD_SHIPPING
        if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe"))
            || FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe")))
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian support fire blocked: player_view_unavailable selected_enemy=%s"),
                *GetNameSafe(BestTarget));
        }
#endif
        return;
    }

    if (PlayerTarget.IsValid() && PlayerTarget->GetFirstPersonCamera())
    {
        const FVector CrosshairOrigin = PlayerTarget->GetFirstPersonCamera()->GetComponentLocation();
        const FVector CrosshairDirection = PlayerTarget->GetAimDirection();
        const float CrosshairRange = FMath::Max(100.0f, EngagementRange);
        const ABMCombatCharacter* CrosshairTarget = BMCombatTargeting::ResolveLiveHostileCrosshairTarget(
            GetWorld(),
            PlayerTarget.Get(),
            CrosshairOrigin,
            CrosshairDirection,
            CrosshairRange,
            this);
        if (CrosshairTarget != BestTarget)
        {
#if !UE_BUILD_SHIPPING
            if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe"))
                || FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe")))
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian support fire blocked: player_crosshair=%s selected_enemy=%s"),
                    *GetNameSafe(CrosshairTarget),
                    *GetNameSafe(BestTarget));
            }
#endif
            return;
        }
    }

    CurrentEnemyTarget = BestTarget;
    const FVector CoverPoint = FindCoverLocation(BestTarget->GetActorLocation());
    if (!CoverPoint.IsNearlyZero() && FVector::DistSquared2D(GetActorLocation(), CoverPoint) > FMath::Square(65.0f))
    {
        AddMovementInput((CoverPoint - GetActorLocation()).GetSafeNormal2D(), 0.55f);
    }

    const FVector Origin = GetActorLocation() + FVector(0.0f, 0.0f, 105.0f);
    const FVector TargetLocation = BestTarget->GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BlackMeridianFriendlyShot), false, this);
    QueryParams.bTraceComplex = true;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, TargetLocation, ECC_Visibility, QueryParams) || Hit.GetActor() != BestTarget)
    {
        return;
    }

    FBMDamageContext DamageContext;
    DamageContext.RawDamage = SupportDamagePerShot;
    DamageContext.FinalDamage = DamageContext.RawDamage;
    DamageContext.HitZone = EBMHitZone::Torso;
    DamageContext.InstigatorActor = this;
    if (UBMHealthComponent* Health = BestTarget->HealthComponent)
    {
        Health->ApplyDamage(DamageContext.FinalDamage, DamageContext);
        BestTarget->MulticastHitReaction((BestTarget->GetActorLocation() - Origin).GetSafeNormal(), DamageContext.HitZone);
        if (BestTarget->SuppressionComponent)
        {
            BestTarget->SuppressionComponent->AddSuppression(0.45f);
        }
    }
    UAISense_Hearing::ReportNoiseEvent(GetWorld(), Origin, 0.7f, this, 900.0f, TEXT("FriendlyRifle"));
    FireCooldown = EngagementFireInterval;
}

void ABMFriendlySquadMember::HandleDeath(const FBMDamageContext& DamageContext)
{
    Super::HandleDeath(DamageContext);
    if (HasAuthority())
    {
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (ABMInteractableActor* SpawnedRevivePrompt = GetWorld()->SpawnActor<ABMInteractableActor>(
                ABMInteractableActor::StaticClass(),
                GetActorLocation(),
                GetActorRotation(),
                SpawnParameters))
        {
            const FString CallsignString = Callsign.ToString();
            SpawnedRevivePrompt->InteractionId = CallsignString.IsEmpty()
                ? FName(TEXT("ReviveAlly"))
                : FName(*FString::Printf(TEXT("Revive_%s"), *CallsignString));
            SpawnedRevivePrompt->Prompt = FText::Format(
                NSLOCTEXT("BlackMeridian", "RevivePrompt", "HOLD E  REVIVE {0}"),
                Callsign);
            SpawnedRevivePrompt->InteractionType = EBMInteractionType::Hold;
            SpawnedRevivePrompt->InteractionAction = EBMInteractionAction::ReviveAlly;
            SpawnedRevivePrompt->ReviveTarget = this;
            SpawnedRevivePrompt->bDestroyAfterInteraction = true;
            SpawnedRevivePrompt->SetLifeSpan(25.0f);
            this->RevivePrompt = SpawnedRevivePrompt;
        }
    }
}

void ABMFriendlySquadMember::RestoreForCheckpoint(const float InHealth)
{
    Super::RestoreForCheckpoint(InHealth);
    SetLifeSpan(0.0f);
    if (RevivePrompt.IsValid())
    {
        RevivePrompt->Destroy();
        RevivePrompt.Reset();
    }
}

void ABMFriendlySquadMember::AssistDownedSquadmate()
{
    if (!HasAuthority() || AssistCooldown > 0.0f || !GetWorld())
    {
        return;
    }

    for (TActorIterator<ABMFriendlySquadMember> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        ABMFriendlySquadMember* Ally = *Iterator;
        if (!Ally || Ally == this || !Ally->IsEliminated() ||
            FVector::DistSquared(GetActorLocation(), Ally->GetActorLocation()) > FMath::Square(240.0f))
        {
            continue;
        }

        Ally->RestoreForCheckpoint(45.0f);
        AssistCooldown = 12.0f;
        if (ABMPlayerController* Controller = Cast<ABMPlayerController>(PlayerTarget.IsValid() ? PlayerTarget->GetController() : nullptr))
        {
            Controller->ShowSubtitle(FText::Format(
                NSLOCTEXT("BlackMeridian", "ReviveSquadLine", "{0}: Sable is back on her feet."),
                Callsign), 2.5f);
        }
        return;
    }
}

FVector ABMFriendlySquadMember::GetObjectiveAnchor() const
{
    if (!GetWorld())
    {
        return FVector::ZeroVector;
    }

    const ABMGameState* State = GetWorld()->GetGameState<ABMGameState>();
    if (!State || State->bSignalControlMode)
    {
        return FVector::ZeroVector;
    }

    switch (State->MissionPhase)
    {
    case EBMMissionPhase::Checkpoint:
        return FVector(400.0f, 0.0f, 100.0f);
    case EBMMissionPhase::InteriorBreach:
        return FVector(780.0f, 880.0f, 150.0f);
    case EBMMissionPhase::Intelligence:
        return FVector(850.0f, 80.0f, 80.0f);
    case EBMMissionPhase::OpenCombat:
        return FVector(1500.0f, 0.0f, 100.0f);
    case EBMMissionPhase::Extraction:
    case EBMMissionPhase::FinalDefense:
        return FVector(1500.0f, 650.0f, 110.0f);
    default:
        return FVector::ZeroVector;
    }
}

FVector ABMFriendlySquadMember::FindCoverLocation(const FVector& ThreatLocation) const
{
    if (!GetWorld())
    {
        return FVector::ZeroVector;
    }

    FVector BestPoint = FVector::ZeroVector;
    float BestDistanceSquared = MAX_FLT;
    for (TActorIterator<AStaticMeshActor> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        AStaticMeshActor* Cover = *Iterator;
        if (!Cover || !Cover->ActorHasTag(TEXT("ThinCover")))
        {
            continue;
        }

        const FVector AwayFromThreat = (Cover->GetActorLocation() - ThreatLocation).GetSafeNormal2D();
        const FVector Candidate = Cover->GetActorLocation() + AwayFromThreat * 120.0f;
        const float DistanceSquared = FVector::DistSquared2D(GetActorLocation(), Candidate);
        if (DistanceSquared >= BestDistanceSquared || DistanceSquared > FMath::Square(900.0f))
        {
            continue;
        }

        FHitResult CoverHit;
        FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BlackMeridianFriendlyCover), true, this);
        const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
            CoverHit,
            Candidate + FVector(0.0f, 0.0f, 70.0f),
            ThreatLocation + FVector(0.0f, 0.0f, 70.0f),
            ECC_Visibility,
            QueryParams);
        if (bBlocked && CoverHit.GetActor() == Cover)
        {
            BestPoint = Candidate;
            BestDistanceSquared = DistanceSquared;
        }
    }
    return BestPoint;
}
