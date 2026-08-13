// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMSignalControlGameMode.h"

#include "BMControlNode.h"
#include "BMCombatCharacter.h"
#include "BMGameState.h"
#include "BMHealthComponent.h"
#include "BMPlayerController.h"
#include "BMPlayerCharacter.h"
#include "BMPlayerState.h"
#include "BMSignalBotController.h"
#include "BMWeapon.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerStart.h"
#include "CollisionQueryParams.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ABMSignalControlGameMode::ABMSignalControlGameMode()
{
    bMissionMode = false;
    bRespawnPlayers = true;
    PrimaryActorTick.bCanEverTick = true;
    // ABMGameMode keeps its tick disabled until a mission-side probe opts in.
    // Signal Control owns the authoritative countdown, scoring, rotation, and
    // combat-probe state machine, so it must always begin with its tick live.
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ABMSignalControlGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (!HasAuthority())
    {
        return;
    }

    // Be explicit here as well as in the constructor.  This protects listen
    // servers when an inherited GameMode tick policy or a paused lobby has
    // disabled the actor before the signal state is initialized.
    SetActorTickEnabled(true);

    const bool bCountdownAlreadyRequested = bMatchStarted && CountdownTimer > 0.0f;
    if (ABMGameState* State = GetGameState<ABMGameState>())
    {
        State->bSignalControlMode = true;
        State->bMatchLive = false;
        State->MatchCountdown = bCountdownAlreadyRequested ? CountdownTimer : 0.0f;
        State->SignalControl.bMatchFinished = false;
        State->SignalControl.WinningTeam = EBMTeam::Neutral;
    }

#if !UE_BUILD_SHIPPING
    const bool bResultsProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_SignalControlResultsProbe"));
    bSignalControlProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_SignalControlProbe")) || bResultsProbe;
    bDeathPresentationProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_DeathPresentationProbe"));
    bReloadProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_ReloadProbe"));
    bReloadProbeEmpty = FParse::Param(FCommandLine::Get(), TEXT("BM_ReloadProbeEmpty"));
    bReloadProbeNoInterrupt = FParse::Param(FCommandLine::Get(), TEXT("BM_ReloadProbeNoInterrupt")) || bReloadProbeEmpty;
    bReloadProbe = bReloadProbe || bReloadProbeEmpty;
    bCombatFallbackProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_CombatFallbackProbe"));
    bCombatProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe"))
        || bDeathPresentationProbe
        || bReloadProbe
        || bCombatFallbackProbe;
    if (bSignalControlProbe)
    {
        // Keep the probe short while preserving the ordinary countdown,
        // node-rotation, overlap, and score paths.
        NodeRotationSeconds = 3.0f;
    }
    if (bResultsProbe)
    {
        // The result probe uses the ordinary scoring path with a small limit;
        // it never changes normal game balance or shipping behavior.
        SignalScoreLimit = 2;
    }
#endif

    const TArray<FVector> NodeLocations = {FVector(250.0f, 650.0f, 65.0f), FVector(1500.0f, -500.0f, 65.0f), FVector(2600.0f, 650.0f, 65.0f)};
    for (int32 Index = 0; Index < NodeLocations.Num(); ++Index)
    {
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (ABMControlNode* Node = GetWorld()->SpawnActor<ABMControlNode>(ABMControlNode::StaticClass(), NodeLocations[Index], FRotator::ZeroRotator, SpawnParameters))
        {
            Node->NodeIndex = Index;
            Node->SetActive(Index == 0);
            Nodes.Add(Node);
        }
    }
    // Keep more starts than the 12-player match cap. Bot fill runs after the
    // engine's initial human RestartPlayer call, so a late joiner needs an
    // unoccupied slot before MaintainBotFill can trim a bot. The two
    // perimeter banks also keep first spawns away from the rotating nodes.
    const TArray<FVector> StartLocations = {
        FVector(-1200.0f, -1400.0f, 120.0f), FVector(-1200.0f, -1000.0f, 120.0f),
        FVector(-1200.0f, -600.0f, 120.0f), FVector(-1200.0f, -200.0f, 120.0f),
        FVector(-1200.0f, 200.0f, 120.0f), FVector(-1200.0f, 600.0f, 120.0f),
        FVector(-1200.0f, 1000.0f, 120.0f), FVector(-1200.0f, 1400.0f, 120.0f),
        FVector(2800.0f, -1400.0f, 120.0f), FVector(2800.0f, -1000.0f, 120.0f),
        FVector(2800.0f, -600.0f, 120.0f), FVector(2800.0f, -200.0f, 120.0f),
        FVector(2800.0f, 200.0f, 120.0f), FVector(2800.0f, 600.0f, 120.0f),
        FVector(2800.0f, 1000.0f, 120.0f), FVector(2800.0f, 1400.0f, 120.0f)};
    const FVector ArenaCenter(650.0f, 0.0f, 0.0f);
    for (const FVector& Location : StartLocations)
    {
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        const FRotator StartRotation(0.0f, (ArenaCenter - Location).Rotation().Yaw, 0.0f);
        if (APlayerStart* Start = GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Location, StartRotation, SpawnParameters))
        {
            SignalStarts.Add(Start);
        }
    }
    RotationTimer = NodeRotationSeconds;
    ScoreTimer = 1.0f;
    if (!bMatchStarted)
    {
        CountdownTimer = 0.0f;
    }

#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_AutoStartSignalMatch")) || bSignalControlProbe)
    {
        UE_LOG(LogTemp, Display, TEXT("Black Meridian signal probe: auto-start requested"));
        RequestMatchStart();

        // Development probes are launched straight into the arena.  A
        // listen-server controller can still carry the normal lobby pause
        // from PostLogin, so release that pause after the authoritative
        // countdown has been installed.
        if (bCombatProbe || bSignalControlProbe)
        {
            if (APlayerController* LocalController = GetWorld()->GetFirstPlayerController())
            {
                LocalController->SetPause(false);
            }
        }
    }
#endif

#if !UE_BUILD_SHIPPING
    if (bSignalControlProbe && !bCombatProbe && Nodes.IsValidIndex(0))
    {
        // Put one authoritative bot inside the active capture volume so the
        // probe observes the real overlap and scoring path without depending
        // on AI pathing or a rendered client.
        UE_LOG(LogTemp, Display, TEXT("Black Meridian signal probe: scoring bot staged team=%d node=%d"), static_cast<int32>(EBMTeam::Lattice), 0);
        SpawnReplacementBot(EBMTeam::Lattice, Nodes[0]->GetActorLocation() + FVector(0.0f, 0.0f, 96.0f));
    }
    else
#endif
    {
        // Bot fill is deferred until startup has created the arena starts and
        // the server knows how many human slots are actually occupied. This
        // keeps a listen session at the 12-player ceiling without creating
        // bots at the origin during map startup.
    }

    MaintainBotFill();

    // PostLogin can run while the map is still starting up. Bots created in
    // that window may have missed the initial profile-backed weapon spawn;
    // repair them now that ABMGameMode::BeginPlay has loaded the profile.
    EnsureBotWeapons();
    RefreshSignalLobbySnapshot();
}

void ABMSignalControlGameMode::RestartPlayer(AController* NewPlayer)
{
    Super::RestartPlayer(NewPlayer);

    if (ABMSignalBotController* BotController = Cast<ABMSignalBotController>(NewPlayer))
    {
        if (ABMPlayerCharacter* Bot = Cast<ABMPlayerCharacter>(BotController->GetPawn()))
        {
            Bot->SetTeam(BotController->GetBotTeam());
        }
    }
}

void ABMSignalControlGameMode::RequestMatchStart()
{
    if (!HasAuthority() || bMatchStarted)
    {
        return;
    }

    bMatchStarted = true;
    CountdownTimer = 5.0f;
    UE_LOG(LogTemp, Display, TEXT("Black Meridian signal: authoritative countdown started (seconds=%.1f)"), CountdownTimer);
    if (ABMGameState* State = GetGameState<ABMGameState>())
    {
        State->bMatchLive = false;
        State->MatchCountdown = CountdownTimer;
        State->ForceNetUpdate();
    }
}

AActor* ABMSignalControlGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    if (SignalStarts.Num() == 0)
    {
        return Super::ChoosePlayerStart_Implementation(Player);
    }
    EBMTeam Team = EBMTeam::Neutral;
    if (const ABMPlayerState* PlayerState = Player ? Player->GetPlayerState<ABMPlayerState>() : nullptr)
    {
        Team = PlayerState->Team;
    }
    if (Team == EBMTeam::Neutral)
    {
        Team = NextStartIndex % 2 == 0 ? EBMTeam::Lattice : EBMTeam::Vanta;
    }
    ++NextStartIndex;
    return ChooseSafestStart(Team);
}

void ABMSignalControlGameMode::RegisterCombatLocation(const FVector& Location)
{
    if (!HasAuthority() || Location.IsNearlyZero() || !GetWorld())
    {
        return;
    }

    PruneRecentCombatLocations();
    FBMSignalCombatLocation RecentLocation;
    RecentLocation.Location = Location;
    RecentLocation.RecordedAt = GetWorld()->GetTimeSeconds();
    RecentCombatLocations.Add(RecentLocation);
    if (RecentCombatLocations.Num() > 18)
    {
        RecentCombatLocations.RemoveAt(0);
    }
}

void ABMSignalControlGameMode::PruneRecentCombatLocations()
{
    if (!GetWorld())
    {
        return;
    }
    const float Now = GetWorld()->GetTimeSeconds();
    RecentCombatLocations.RemoveAll([Now](const FBMSignalCombatLocation& Entry)
    {
        return Now - Entry.RecordedAt > 18.0f;
    });
}

APlayerStart* ABMSignalControlGameMode::ChooseSafestStart(const EBMTeam Team)
{
    PruneRecentCombatLocations();
    APlayerStart* BestStart = nullptr;
    float BestScore = -MAX_FLT;
    for (APlayerStart* Start : SignalStarts)
    {
        if (!Start)
        {
            continue;
        }
        const float Score = ScoreStartLocation(Start, Team);
        if (!BestStart || Score > BestScore)
        {
            BestStart = Start;
            BestScore = Score;
        }
    }
    return BestStart;
}

float ABMSignalControlGameMode::ScoreStartLocation(const APlayerStart* Start, const EBMTeam Team) const
{
    if (!Start || !GetWorld())
    {
        return -MAX_FLT;
    }

    const FVector CandidateLocation = Start->GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
    float Score = 0.0f;
    for (const FBMSignalCombatLocation& Entry : RecentCombatLocations)
    {
        const float Distance = FVector::Dist2D(CandidateLocation, Entry.Location);
        if (Distance < 1100.0f)
        {
            Score -= (1100.0f - Distance) * 3.0f;
        }
    }

    for (TActorIterator<ABMCombatCharacter> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        const ABMCombatCharacter* Enemy = *Iterator;
        if (!Enemy || Enemy->IsEliminated())
        {
            continue;
        }

        const FVector EnemyViewLocation = Enemy->GetPawnViewLocation();
        const float Distance = FVector::Dist2D(CandidateLocation, EnemyViewLocation);
        if (Enemy->GetTeam() == Team || Enemy->GetTeam() == EBMTeam::Neutral)
        {
            if (Distance < 380.0f)
            {
                Score -= (380.0f - Distance) * 4.0f;
            }
            continue;
        }

        if (Distance < 1400.0f)
        {
            Score -= (1400.0f - Distance) * 1.5f;
            FHitResult VisibilityHit;
            FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BlackMeridianSpawnVisibility), true, Enemy);
            const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
                VisibilityHit,
                EnemyViewLocation,
                CandidateLocation,
                ECC_Visibility,
                QueryParams);
            if (!bBlocked)
            {
                Score -= 10000.0f;
            }
        }
    }

    return Score;
}

void ABMSignalControlGameMode::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || Nodes.Num() == 0)
    {
        return;
    }

    BotWeaponAuditTimer -= DeltaSeconds;
    if (BotWeaponAuditTimer <= 0.0f)
    {
        BotWeaponAuditTimer = 0.5f;
        EnsureBotWeapons();
    }

#if !UE_BUILD_SHIPPING
    // Keep the probe deterministic even if a late local presentation callback
    // reapplies the normal lobby pause.  This tick is configured to run while
    // paused, so it can clear that pause and allow the real countdown/combat
    // path below to proceed.
    if (bCombatProbe && GetWorld()->IsPaused())
    {
        if (APlayerController* LocalController = GetWorld()->GetFirstPlayerController())
        {
            LocalController->SetPause(false);
            if (!GetWorld()->IsPaused())
            {
                UE_LOG(LogTemp, Display, TEXT("Black Meridian combat probe: listen host pause released"));
            }
        }
    }
#endif

#if !UE_BUILD_SHIPPING
    if (bCombatProbe)
    {
        TickCombatProbe(DeltaSeconds);
    }
#endif

    if (!bMatchStarted)
    {
        if (ABMGameState* State = GetGameState<ABMGameState>())
        {
            State->bMatchLive = false;
            State->MatchCountdown = 0.0f;
        }
        return;
    }

    if (CountdownTimer > 0.0f)
    {
        CountdownTimer = FMath::Max(0.0f, CountdownTimer - DeltaSeconds);
        if (ABMGameState* State = GetGameState<ABMGameState>())
        {
            const bool bWasLive = State->bMatchLive;
            State->MatchCountdown = CountdownTimer;
            State->bMatchLive = CountdownTimer <= 0.0f;
            if (!bWasLive && State->bMatchLive)
            {
                UE_LOG(LogTemp, Display, TEXT("Black Meridian signal: match live"));
            }
        }
        return;
    }

    if (const ABMGameState* State = GetGameState<ABMGameState>(); State && State->SignalControl.bMatchFinished)
    {
        return;
    }

    EnsureLivePlayerPawns();

    RotationTimer -= DeltaSeconds;
    ScoreTimer -= DeltaSeconds;
    if (RotationTimer <= 0.0f)
    {
        RotateActiveNode();
    }

    if (ScoreTimer <= 0.0f)
    {
        ScoreTimer = 1.0f;
        const EBMTeam ControllingTeam = Nodes[ActiveNodeIndex]->GetControllingTeam();
        Nodes[ActiveNodeIndex]->ControllingTeam = ControllingTeam;
        Nodes[ActiveNodeIndex]->ForceNetUpdate();
        if (ABMGameState* State = GetGameState<ABMGameState>())
        {
            if (ControllingTeam == EBMTeam::Lattice)
            {
                ++State->SignalControl.LatticeScore;
            }
            else if (ControllingTeam == EBMTeam::Vanta)
            {
                ++State->SignalControl.VantaScore;
            }

            if (ControllingTeam != EBMTeam::Neutral &&
                ((State->SignalControl.LatticeScore + State->SignalControl.VantaScore) % 5 == 0))
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian signal: node=%d controlling_team=%d score_lattice=%d score_vanta=%d"),
                    ActiveNodeIndex,
                    static_cast<int32>(ControllingTeam),
                    State->SignalControl.LatticeScore,
                    State->SignalControl.VantaScore);
            }
#if !UE_BUILD_SHIPPING
            if (bSignalControlProbe && ControllingTeam != EBMTeam::Neutral && !bProbeScoreReported)
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian signal probe: server scoring verified node=%d controlling_team=%d score_lattice=%d score_vanta=%d"),
                    ActiveNodeIndex,
                    static_cast<int32>(ControllingTeam),
                    State->SignalControl.LatticeScore,
                    State->SignalControl.VantaScore);
                bProbeScoreReported = true;
            }
#endif

            if (ControllingTeam != EBMTeam::Neutral)
            {
                for (APlayerState* ExistingState : State->PlayerArray)
                {
                    ABMPlayerState* PlayerState = Cast<ABMPlayerState>(ExistingState);
                    ABMCombatCharacter* Character = PlayerState ? Cast<ABMCombatCharacter>(PlayerState->GetPawn()) : nullptr;
                    if (PlayerState && Character && PlayerState->Team == ControllingTeam && Nodes[ActiveNodeIndex]->ContainsCharacter(Character))
                    {
                        ++PlayerState->ObjectiveScore;
                    }
                }
            }
            State->SignalControl.ActiveNodeIndex = ActiveNodeIndex;
            State->SignalControl.SecondsUntilRotation = RotationTimer;
            if (State->SignalControl.LatticeScore >= SignalScoreLimit)
            {
                FinishMatch(EBMTeam::Lattice);
            }
            else if (State->SignalControl.VantaScore >= SignalScoreLimit)
            {
                FinishMatch(EBMTeam::Vanta);
            }
            State->ForceNetUpdate();
        }
    }
}

void ABMSignalControlGameMode::EnsureLivePlayerPawns()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    const ABMGameState* State = GetGameState<ABMGameState>();
    if (!State || !State->bMatchLive || State->SignalControl.bMatchFinished)
    {
        return;
    }

    const float Now = GetWorld()->GetTimeSeconds();
    for (auto CooldownIterator = PawnRecoveryCooldowns.CreateIterator(); CooldownIterator; ++CooldownIterator)
    {
        if (!CooldownIterator.Key().IsValid() || CooldownIterator.Value() <= Now)
        {
            CooldownIterator.RemoveCurrent();
        }
    }

    for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
    {
        APlayerController* PlayerController = Iterator->Get();
        if (!PlayerController || PlayerController->IsPendingKillPending())
        {
            continue;
        }

        const ABMPlayerState* PlayerState = PlayerController->GetPlayerState<ABMPlayerState>();
        if (!PlayerState || PlayerState->IsInactive())
        {
            // AGameMode moves a disconnected player's state to the inactive
            // array during net cleanup. It can remain in the controller
            // iterator for a short period; never respawn that detached slot.
            continue;
        }

        // The listen-server host has no UNetConnection by design. A remote
        // controller without one is either still being torn down or has not
        // completed connection setup, so defer recovery until the authority
        // owns an active connection instead of retrying every tick.
        if (!PlayerController->IsLocalController() && PlayerController->GetNetConnection() == nullptr)
        {
            continue;
        }

        if (const float* NextRecoveryAttempt = PawnRecoveryCooldowns.Find(PlayerController); NextRecoveryAttempt && *NextRecoveryAttempt > Now)
        {
            continue;
        }

        if (APawn* Pawn = PlayerController->GetPawn())
        {
            // The perimeter colliders normally make this unreachable. Keep
            // the last-resort path alive for a transient cooked-collision
            // gap, but return the existing pawn instead of destroying it or
            // starting a respawn/death flow.
            if (Pawn->GetActorLocation().Z < -300.0f)
            {
                if (ABMPlayerCharacter* PlayerPawn = Cast<ABMPlayerCharacter>(Pawn))
                {
                    PlayerPawn->RecoverFromBoundaryFall();
                    PawnRecoveryCooldowns.Remove(PlayerController);
                    continue;
                }

                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT("Black Meridian signal: non-player pawn fell out of arena; recovering controller=%s location=%s"),
                    *GetNameSafe(PlayerController),
                    *Pawn->GetActorLocation().ToString());
                PlayerController->UnPossess();
                Pawn->Destroy();
            }
            else
            {
                PawnRecoveryCooldowns.Remove(PlayerController);
                continue;
            }
        }

        PawnRecoveryCooldowns.Add(PlayerController, Now + 0.75f);
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Black Meridian signal: live player had no pawn; recovering controller=%s"),
            *GetNameSafe(PlayerController));
        RestartPlayer(PlayerController);
        if (ABMPlayerController* BMController = Cast<ABMPlayerController>(PlayerController))
        {
            BMController->ShowRespawnedPlayer();
        }
    }
}

void ABMSignalControlGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (ABMPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<ABMPlayerState>() : nullptr)
    {
        int32 LatticeCount = 0;
        int32 VantaCount = 0;
        for (APlayerState* ExistingState : GameState->PlayerArray)
        {
            if (ExistingState == PlayerState)
            {
                continue;
            }
            if (const ABMPlayerState* ExistingBMState = Cast<ABMPlayerState>(ExistingState); ExistingBMState && ExistingBMState->Team == EBMTeam::Lattice)
            {
                ++LatticeCount;
            }
            else if (const ABMPlayerState* VantaState = Cast<ABMPlayerState>(ExistingState); VantaState && VantaState->Team == EBMTeam::Vanta)
            {
                ++VantaCount;
            }
        }
        PlayerState->Team = LatticeCount <= VantaCount ? EBMTeam::Lattice : EBMTeam::Vanta;
        UE_LOG(LogTemp, Display, TEXT("Black Meridian signal: player login team=%d"), static_cast<int32>(PlayerState->Team));
        if (ABMPlayerCharacter* PlayerCharacter = Cast<ABMPlayerCharacter>(NewPlayer->GetPawn()))
        {
            PlayerCharacter->SetTeam(PlayerState->Team);
        }
        if (ABMPlayerController* Controller = Cast<ABMPlayerController>(NewPlayer))
        {
            Controller->ShowSignalLobby();
#if !UE_BUILD_SHIPPING
            if (FParse::Param(FCommandLine::Get(), TEXT("BM_AutoStartSignalMatch")))
            {
                NewPlayer->SetPause(false);
                RequestMatchStart();
            }
#endif
        }
        if (HasActorBegunPlay())
        {
            MaintainBotFill();
        }
        RefreshSignalLobbySnapshot();
    }
}

void ABMSignalControlGameMode::Logout(AController* Exiting)
{
    if (APlayerController* ExitingPlayerController = Cast<APlayerController>(Exiting))
    {
        PawnRecoveryCooldowns.Remove(ExitingPlayerController);
    }

    const ABMPlayerState* ExitingState = Exiting ? Exiting->GetPlayerState<ABMPlayerState>() : nullptr;
    const ABMGameState* CurrentState = GetGameState<ABMGameState>();
    const bool bReplaceDisconnectedPlayer =
        bMatchStarted &&
        (!CurrentState || !CurrentState->SignalControl.bMatchFinished) &&
        ExitingState &&
        ExitingState->Team != EBMTeam::Neutral &&
        !Cast<ABMSignalBotController>(Exiting);

    if (ABMGameState* State = GetGameState<ABMGameState>())
    {
        State->RecentKillFeed.Insert(
            bReplaceDisconnectedPlayer
                ? TEXT("PLAYER DISCONNECTED // BOT TAKEOVER")
                : TEXT("PLAYER DISCONNECTED // MATCH CONTINUES"),
            0);
        if (State->RecentKillFeed.Num() > 5)
        {
            State->RecentKillFeed.SetNum(5);
        }
        State->ForceNetUpdate();
    }
    if (bReplaceDisconnectedPlayer)
    {
#if !UE_BUILD_SHIPPING
        if (bCombatProbe)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian combat probe: disconnect observed team=%d bot_takeover=yes"),
                static_cast<int32>(ExitingState->Team));
        }
#endif
        SpawnReplacementBot(ExitingState->Team);
    }
#if !UE_BUILD_SHIPPING
    else if (bCombatProbe && ExitingState)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat probe: disconnect observed team=%d bot_takeover=no"),
            static_cast<int32>(ExitingState->Team));
    }
#endif
    Super::Logout(Exiting);
    MaintainBotFill();
    RefreshSignalLobbySnapshot();
}

void ABMSignalControlGameMode::RefreshSignalLobbySnapshot()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    ABMGameState* State = GetGameState<ABMGameState>();
    if (!State)
    {
        return;
    }

    constexpr int32 MaxSignalLobbyPlayers = 12;
    TArray<FBMSignalLobbyMember> Members;
    int32 HumanCount = 0;
    int32 LobbyBotCount = 0;
    int32 LatticeBotNumber = 0;
    int32 VantaBotNumber = 0;

    for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
    {
        APlayerController* PlayerController = Iterator->Get();
        if (!PlayerController || PlayerController->IsPendingKillPending())
        {
            continue;
        }

        const ABMPlayerState* PlayerState = PlayerController->GetPlayerState<ABMPlayerState>();
        if (!PlayerState || PlayerState->IsInactive())
        {
            continue;
        }

        // On the authority, a remote controller without a connection is still
        // in teardown/startup and should not appear as a playable slot.
        if (!PlayerController->IsLocalController() && PlayerController->GetNetConnection() == nullptr)
        {
            continue;
        }

        FBMSignalLobbyMember Member;
        Member.DisplayName = PlayerState->GetPlayerName();
        if (Member.DisplayName.IsEmpty())
        {
            Member.DisplayName = FString::Printf(TEXT("OPERATOR %02d"), HumanCount + 1);
        }
        Member.Team = PlayerState->Team;
        Member.bIsBot = false;
        Member.bIsHost = PlayerController->GetNetConnection() == nullptr;
        Members.Add(MoveTemp(Member));
        ++HumanCount;
    }

    for (TActorIterator<ABMSignalBotController> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        const ABMSignalBotController* BotController = *Iterator;
        if (!BotController || BotController->GetBotTeam() == EBMTeam::Neutral)
        {
            continue;
        }

        FBMSignalLobbyMember Member;
        Member.Team = BotController->GetBotTeam();
        Member.bIsBot = true;
        Member.bIsHost = false;
        const int32 BotNumber = Member.Team == EBMTeam::Lattice
            ? ++LatticeBotNumber
            : ++VantaBotNumber;
        Member.DisplayName = FString::Printf(
            TEXT("%s BOT %02d"),
            Member.Team == EBMTeam::Lattice ? TEXT("LATTICE") : TEXT("VANTA"),
            BotNumber);
        Members.Add(MoveTemp(Member));
        ++LobbyBotCount;
    }

    Members.Sort([](const FBMSignalLobbyMember& A, const FBMSignalLobbyMember& B)
    {
        const auto TeamOrder = [](const EBMTeam Team)
        {
            return Team == EBMTeam::Lattice ? 0 : (Team == EBMTeam::Vanta ? 1 : 2);
        };
        const int32 ATeam = TeamOrder(A.Team);
        const int32 BTeam = TeamOrder(B.Team);
        if (ATeam != BTeam)
        {
            return ATeam < BTeam;
        }
        if (A.bIsBot != B.bIsBot)
        {
            return !A.bIsBot;
        }
        return A.DisplayName < B.DisplayName;
    });

    if (Members.Num() > MaxSignalLobbyPlayers)
    {
        Members.SetNum(MaxSignalLobbyPlayers);
    }

    State->SignalLobbyMembers = MoveTemp(Members);
    State->SignalLobbyHumanCount = HumanCount;
    State->SignalLobbyBotCount = LobbyBotCount;
    State->SignalLobbyCapacity = MaxSignalLobbyPlayers;
    State->ForceNetUpdate();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian signal lobby: roster refreshed members=%d humans=%d bots=%d capacity=%d"),
        State->SignalLobbyMembers.Num(),
        State->SignalLobbyHumanCount,
        State->SignalLobbyBotCount,
        State->SignalLobbyCapacity);
}

#if !UE_BUILD_SHIPPING
void ABMSignalControlGameMode::TickCombatProbe(const float DeltaSeconds)
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    const ABMGameState* State = GetGameState<ABMGameState>();
    if (!State || !State->bSignalControlMode || !State->bMatchLive || State->SignalControl.bMatchFinished)
    {
        return;
    }

    if (!bCombatProbeConfigured)
    {
        TArray<ABMPlayerCharacter*> RemotePlayers;
        for (APlayerState* ExistingState : State->PlayerArray)
        {
            const ABMPlayerState* BMState = Cast<ABMPlayerState>(ExistingState);
            ABMPlayerCharacter* Character = BMState ? Cast<ABMPlayerCharacter>(BMState->GetPawn()) : nullptr;
            if (!Character || Character->IsEliminated())
            {
                continue;
            }
            // The listen host owns a local player as well as the remote
            // clients. Keep this probe explicitly client-to-client so its
            // fire request and disconnect are observable in client logs.
            AController* CharacterController = Character->GetController();
            if (!CharacterController || CharacterController->IsLocalController() || Cast<ABMSignalBotController>(CharacterController))
            {
                continue;
            }
            RemotePlayers.Add(Character);
        }

        ABMPlayerCharacter* Shooter = nullptr;
        ABMPlayerCharacter* Target = nullptr;
        for (ABMPlayerCharacter* CandidateShooter : RemotePlayers)
        {
            if (!CandidateShooter || CandidateShooter->GetTeam() != EBMTeam::Lattice)
            {
                continue;
            }
            for (ABMPlayerCharacter* CandidateTarget : RemotePlayers)
            {
                if (CandidateTarget && CandidateTarget != CandidateShooter && CandidateTarget->GetTeam() == EBMTeam::Vanta)
                {
                    Shooter = CandidateShooter;
                    Target = CandidateTarget;
                    break;
                }
            }
            if (Shooter && Target)
            {
                break;
            }
        }

        if (Shooter && Target && Shooter->GetCurrentWeapon())
        {
            StageCombatProbePlayers(Shooter, Target);
            return;
        }
    }

    if (bCombatProbeConfigured && !bCombatProbeFireDispatched)
    {
        CombatProbeFireDelay = FMath::Max(0.0f, CombatProbeFireDelay - DeltaSeconds);
        if (CombatProbeFireDelay <= 0.0f)
        {
            DispatchCombatProbeFire();
        }
    }

    if (bReloadProbe && bCombatProbeDamageReported && !bCombatProbeReloadCompleted && !bCombatProbeReloadFailed)
    {
        ABMPlayerCharacter* Shooter = CombatProbeShooter.Get();
        ABMWeapon* Weapon = Shooter ? Shooter->GetCurrentWeapon() : nullptr;
        if (!Shooter || !Weapon)
        {
            bCombatProbeReloadFailed = true;
            UE_LOG(LogTemp, Error, TEXT("Black Meridian combat probe: reload probe failed reason=shooter_or_weapon_missing"));
        }
        else if (!bCombatProbeReloadDispatched)
        {
            if (bReloadProbeEmpty && !bCombatProbeReloadPrepared)
            {
                bCombatProbeReloadFailed = true;
                UE_LOG(LogTemp, Error, TEXT("Black Meridian combat probe: reload probe failed reason=empty_ammo_not_prepared"));
            }
            else
            {
                CombatProbeReloadDelay = FMath::Max(0.0f, CombatProbeReloadDelay - DeltaSeconds);
                if (CombatProbeReloadDelay <= 0.0f)
                {
                    DispatchCombatProbeReload(false);
                }
            }
        }
        else if (!bCombatProbeReloadStarted)
        {
            if (Weapon->IsReloading())
            {
                bCombatProbeReloadStarted = true;
                CombatProbeReloadPartialAmmo = Weapon->GetMagazineAmmo();
                CombatProbeReloadPartialReserve = Weapon->GetReserveAmmo();
                CombatProbeReloadStartTime = GetWorld()->GetTimeSeconds();
                CombatProbeReloadExpectedDuration = Weapon->GetWeaponData()
                    ? (bReloadProbeEmpty
                        ? Weapon->GetWeaponData()->Balance.EmptyReloadDuration
                        : Weapon->GetWeaponData()->Balance.ReloadDuration)
                    : 0.0f;
                CombatProbeReloadDelay = bReloadProbeNoInterrupt ? 0.0f : 0.25f;
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian combat probe: server reload started owner=%s magazine=%d reserve=%d empty=%s expected_duration=%.2f interruption=%s"),
                    *GetNameSafe(Shooter),
                    CombatProbeReloadPartialAmmo,
                    CombatProbeReloadPartialReserve,
                    bReloadProbeEmpty ? TEXT("yes") : TEXT("no"),
                    CombatProbeReloadExpectedDuration,
                    bReloadProbeNoInterrupt ? TEXT("no") : TEXT("yes"));
            }
        }
        else if (!bReloadProbeNoInterrupt && !bCombatProbeReloadInterruptDispatched)
        {
            CombatProbeReloadDelay = FMath::Max(0.0f, CombatProbeReloadDelay - DeltaSeconds);
            if (CombatProbeReloadDelay <= 0.0f)
            {
                DispatchCombatProbeReloadInterrupt();
            }
        }
        else if (!bReloadProbeNoInterrupt && bCombatProbeReloadInterruptDispatched && !bCombatProbeReloadCancelled)
        {
            if (!Weapon->IsReloading())
            {
                if (Weapon->GetMagazineAmmo() != CombatProbeReloadPartialAmmo
                    || Weapon->GetReserveAmmo() != CombatProbeReloadPartialReserve)
                {
                    bCombatProbeReloadFailed = true;
                    UE_LOG(
                        LogTemp,
                        Error,
                        TEXT("Black Meridian combat probe: reload probe failed reason=interrupt_raced_with_completion magazine=%d expected=%d reserve=%d expected_reserve=%d"),
                        Weapon->GetMagazineAmmo(),
                        CombatProbeReloadPartialAmmo,
                        Weapon->GetReserveAmmo(),
                        CombatProbeReloadPartialReserve);
                }
                else
                {
                    bCombatProbeReloadCancelled = true;
                    CombatProbeReloadDelay = 0.30f;
                    UE_LOG(
                        LogTemp,
                        Display,
                        TEXT("Black Meridian combat probe: server reload cancelled verified owner=%s magazine=%d reserve=%d"),
                        *GetNameSafe(Shooter),
                        Weapon->GetMagazineAmmo(),
                        Weapon->GetReserveAmmo());
                }
            }
        }
        else if (!bReloadProbeNoInterrupt && bCombatProbeReloadCancelled && !bCombatProbeReloadSecondDispatched)
        {
            CombatProbeReloadDelay = FMath::Max(0.0f, CombatProbeReloadDelay - DeltaSeconds);
            if (CombatProbeReloadDelay <= 0.0f)
            {
                DispatchCombatProbeReload(true);
            }
        }
        else if (bReloadProbeNoInterrupt && !Weapon->IsReloading())
        {
            const float ActualDuration = CombatProbeReloadStartTime >= 0.0f
                ? GetWorld()->GetTimeSeconds() - CombatProbeReloadStartTime
                : -1.0f;
            const bool bAmmoRestored = Weapon->GetMagazineAmmo() > CombatProbeReloadPartialAmmo
                && Weapon->GetReserveAmmo() < CombatProbeReloadPartialReserve;
            const bool bTimingPass = CombatProbeReloadExpectedDuration <= 0.0f
                || (ActualDuration >= 0.0f && FMath::Abs(ActualDuration - CombatProbeReloadExpectedDuration) <= 0.12f);
            if (bAmmoRestored && bTimingPass)
            {
                bCombatProbeReloadCompleted = true;
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian combat probe: reload probe passed interruption=no completion=yes empty=%s magazine=%d reserve=%d expected_duration=%.2f actual_duration=%.2f timing=%s"),
                    bReloadProbeEmpty ? TEXT("yes") : TEXT("no"),
                    Weapon->GetMagazineAmmo(),
                    Weapon->GetReserveAmmo(),
                    CombatProbeReloadExpectedDuration,
                    ActualDuration,
                    bTimingPass ? TEXT("pass") : TEXT("fail"));
            }
            else if (!bAmmoRestored)
            {
                bCombatProbeReloadFailed = true;
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("Black Meridian combat probe: reload probe failed reason=completion_state_invalid empty=%s magazine=%d partial=%d reserve=%d partial_reserve=%d"),
                    bReloadProbeEmpty ? TEXT("yes") : TEXT("no"),
                    Weapon->GetMagazineAmmo(),
                    CombatProbeReloadPartialAmmo,
                    Weapon->GetReserveAmmo(),
                    CombatProbeReloadPartialReserve);
            }
            else
            {
                bCombatProbeReloadFailed = true;
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("Black Meridian combat probe: reload probe failed reason=timing_mismatch empty=%s expected_duration=%.2f actual_duration=%.2f"),
                    bReloadProbeEmpty ? TEXT("yes") : TEXT("no"),
                    CombatProbeReloadExpectedDuration,
                    ActualDuration);
            }
        }
        else if (!bReloadProbeNoInterrupt && bCombatProbeReloadSecondDispatched && !bCombatProbeReloadSecondStarted)
        {
            if (Weapon->IsReloading())
            {
                bCombatProbeReloadSecondStarted = true;
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian combat probe: server reload restart started owner=%s magazine=%d reserve=%d"),
                    *GetNameSafe(Shooter),
                    Weapon->GetMagazineAmmo(),
                    Weapon->GetReserveAmmo());
            }
        }
        else if (!bReloadProbeNoInterrupt && bCombatProbeReloadSecondStarted && !Weapon->IsReloading())
        {
            if (Weapon->GetMagazineAmmo() > CombatProbeReloadPartialAmmo
                && Weapon->GetReserveAmmo() < CombatProbeReloadPartialReserve)
            {
                bCombatProbeReloadCompleted = true;
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian combat probe: reload probe passed interruption=yes completion=yes magazine=%d reserve=%d"),
                    Weapon->GetMagazineAmmo(),
                    Weapon->GetReserveAmmo());
            }
            else
            {
                bCombatProbeReloadFailed = true;
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("Black Meridian combat probe: reload probe failed reason=completion_state_invalid magazine=%d partial=%d reserve=%d partial_reserve=%d"),
                    Weapon->GetMagazineAmmo(),
                    CombatProbeReloadPartialAmmo,
                    Weapon->GetReserveAmmo(),
                    CombatProbeReloadPartialReserve);
            }
        }
    }

    if (bDeathPresentationProbe && bCombatProbeDamageReported && !bCombatProbeDeathReported)
    {
        ABMPlayerCharacter* Target = CombatProbeTarget.Get();
        UBMHealthComponent* TargetHealth = Target ? Target->FindComponentByClass<UBMHealthComponent>() : nullptr;
        if (Target && TargetHealth && !Target->IsEliminated() && TargetHealth->Health > 0.0f)
        {
            FBMDamageContext DeathContext;
            DeathContext.RawDamage = TargetHealth->Health;
            DeathContext.FinalDamage = TargetHealth->Health;
            DeathContext.HitZone = EBMHitZone::Head;
            DeathContext.bHeadshot = true;
            DeathContext.InstigatorActor = CombatProbeShooter.Get();
            TargetHealth->ApplyDamage(DeathContext.FinalDamage, DeathContext);
            bCombatProbeDeathReported = true;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian death presentation probe: server eliminated target=%s remote_controller=%s"),
                *GetNameSafe(Target),
                Target->GetController() && !Target->GetController()->IsLocalController() ? TEXT("yes") : TEXT("no"));
        }
    }
}

void ABMSignalControlGameMode::RegisterCombatProbeDamage(
    AActor* Shooter,
    AActor* Target,
    const float HealthBefore,
    const float HealthAfter)
{
    if (!bCombatProbe
        || !bCombatProbeFireDispatched
        || bCombatProbeDamageReported
        || Shooter != CombatProbeShooter.Get()
        || Target != CombatProbeTarget.Get()
        || !FMath::IsFinite(HealthBefore)
        || !FMath::IsFinite(HealthAfter)
        || HealthAfter >= HealthBefore)
    {
        return;
    }

    ABMPlayerCharacter* ProbeShooter = CombatProbeShooter.Get();
    ABMWeapon* Weapon = ProbeShooter ? ProbeShooter->GetCurrentWeapon() : nullptr;
    if (!ProbeShooter || !Weapon)
    {
        return;
    }

    const int32 AmmoAfterDamage = Weapon->GetMagazineAmmo();
    if (bReloadProbeEmpty)
    {
        bCombatProbeReloadPrepared = Weapon->SetCombatProbeAmmo(0, Weapon->GetReserveAmmo());
    }
    bCombatProbeDamageReported = true;
    if (bReloadProbe)
    {
        CombatProbeReloadDelay = 0.35f;
    }
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian combat probe: server damage verified shooter=%s target=%s shooter_ammo=%d target_health_before=%.1f target_health_after=%.1f reload_empty=%s prepared=%s"),
        *GetNameSafe(Shooter),
        *GetNameSafe(Target),
        AmmoAfterDamage,
        HealthBefore,
        HealthAfter,
        bReloadProbeEmpty ? TEXT("yes") : TEXT("no"),
        bCombatProbeReloadPrepared ? TEXT("yes") : TEXT("no"));
}

void ABMSignalControlGameMode::StageCombatProbePlayers(ABMPlayerCharacter* Shooter, ABMPlayerCharacter* Target)
{
    if (!Shooter || !Target)
    {
        return;
    }

    const FVector ShooterLocation(-350.0f, 0.0f, 120.0f);
    const FVector OnRayTargetLocation(350.0f, 0.0f, 120.0f);
    // The fallback regression intentionally keeps the target away from the
    // submitted ray while history samples are being captured. The authoritative
    // fire seam moves the same live pawn onto the ray only when the request is
    // received, so the requested historical pose remains off-ray while the
    // current server pose is a valid red-reticle target.
    const FVector TargetLocation = bCombatFallbackProbe
        ? OnRayTargetLocation + FVector(0.0f, 600.0f, 0.0f)
        : OnRayTargetLocation;

    // Restore first: RestoreForCheckpoint() puts CharacterMovement back into
    // walking mode.  Staging before that call let the floor solver move both
    // pawns to Z=93.9, leaving the old +72 cm probe ray tangent to the top of
    // the narrow aim capsule instead of passing through its center.
    Shooter->RestoreForCheckpoint(100.0f);
    Target->RestoreForCheckpoint(100.0f);
    Shooter->SetActorLocation(ShooterLocation, false, nullptr, ETeleportType::TeleportPhysics);
    Shooter->SetActorRotation(FRotator::ZeroRotator);
    Target->SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
    Target->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
    if (Target->GetCombatAimHitbox())
    {
        Target->GetCombatAimHitbox()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Target->GetCombatAimHitbox()->UpdateComponentToWorld();
    }
    if (Shooter->GetCharacterMovement())
    {
        Shooter->GetCharacterMovement()->StopMovementImmediately();
        Shooter->GetCharacterMovement()->GravityScale = 0.0f;
        Shooter->GetCharacterMovement()->SetMovementMode(MOVE_None);
    }
    if (Target->GetCharacterMovement())
    {
        Target->GetCharacterMovement()->StopMovementImmediately();
        Target->GetCharacterMovement()->GravityScale = 0.0f;
        Target->GetCharacterMovement()->SetMovementMode(MOVE_None);
    }
    // The combat probe stages a real remote player pawn inside the live Signal
    // match. Other AI pawns can legitimately acquire that pawn during the
    // 1.25-second network setup window, which used to let unrelated bot fire
    // eliminate the target before the probe's own rewound shot arrived. Give
    // this development-only target a bounded health buffer so the probe
    // measures player-target resolution/damage rather than encounter timing.
    if (Target->HealthComponent)
    {
        Target->HealthComponent->bRegenerateHealth = false;
        Target->HealthComponent->MaxHealth = 1000.0f;
        Target->HealthComponent->Health = Target->HealthComponent->MaxHealth;
    }

    // Keep the probe's baseline deterministic even if a client-side loadout
    // reconciliation completed between player selection and staging. The
    // combat path itself supports every profile; this probe deliberately
    // measures the ARX-4's 30-round baseline.
    Shooter->EquipWeaponById(EBMWeaponId::ARX4);
    Shooter->ForceNetUpdate();
    Target->ForceNetUpdate();

    CombatProbeShooter = Shooter;
    CombatProbeTarget = Target;
    CombatProbeInitialAmmo = Shooter->GetCurrentWeapon()->GetMagazineAmmo();
    CombatProbeFireDelay = 1.25f;
    bCombatProbeConfigured = true;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian combat probe: staged shooter=%s team=%d target=%s team=%d initial_ammo=%d shooter_location=%s target_location=%s"),
        *Shooter->GetName(),
        static_cast<int32>(Shooter->GetTeam()),
        *Target->GetName(),
        static_cast<int32>(Target->GetTeam()),
        CombatProbeInitialAmmo,
        *Shooter->GetActorLocation().ToString(),
        *Target->GetActorLocation().ToString());
}

void ABMSignalControlGameMode::DispatchCombatProbeFire()
{
    ABMPlayerCharacter* Shooter = CombatProbeShooter.Get();
    ABMPlayerCharacter* Target = CombatProbeTarget.Get();
    if (!Shooter || !Target || !Shooter->GetCurrentWeapon())
    {
        return;
    }

    const FVector Origin = Shooter->GetPawnViewLocation();
    // The fallback probe's target is intentionally still off-ray at dispatch
    // time. Aim at the position the live pawn will occupy when the actual
    // server fire request is received; otherwise the historical off-ray pose
    // would be the correct target and the fallback branch would not run.
    const FVector TargetPoint = bCombatFallbackProbe
        ? FVector(350.0f, 0.0f, 120.0f)
        : (Target->GetCombatAimHitbox()
            ? Target->GetCombatAimHitbox()->GetComponentLocation()
            : Target->GetActorLocation());
    const FVector Direction = (TargetPoint - Origin).GetSafeNormal();
    // A replicated client loadout can reconcile after staging and select its
    // serialized current weapon again. Measure the actual weapon state at the
    // instant of the probe request so the acceptance check remains valid for
    // either magazine profile.
    CombatProbeInitialAmmo = Shooter->GetCurrentWeapon()->GetMagazineAmmo();
    if (ABMPlayerController* Controller = Cast<ABMPlayerController>(Shooter->GetController()))
    {
        // A remote client can have a queued movement/view update overwrite the
        // staged pawn's old control rotation between staging and dispatch.
        // Set the server-side probe view to the exact ray before invoking the
        // ordinary client fire path, otherwise the normal anti-cheat cone
        // correctly rejects a valid probe shot as facing backwards.
        const FRotator ProbeRotation = Direction.Rotation();
        Controller->SetControlRotation(ProbeRotation);
        Controller->StartCombatProbeFire(Direction, Shooter->GetActorLocation(), ProbeRotation);
        bCombatProbeFireDispatched = true;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat probe: server dispatched client fire shooter=%s target=%s origin=%s direction=%s"),
            *Shooter->GetName(),
            *Target->GetName(),
            *Origin.ToString(),
            *Direction.ToString());
    }
}

void ABMSignalControlGameMode::PrepareCombatFallbackCurrentPose(AActor* Shooter)
{
    if (!bCombatFallbackProbe
        || bCombatFallbackPoseActivated
        || Shooter != CombatProbeShooter.Get())
    {
        return;
    }

    ABMPlayerCharacter* Target = CombatProbeTarget.Get();
    if (!Target)
    {
        return;
    }

    const FVector CurrentPoseTargetLocation(350.0f, 0.0f, 120.0f);
    Target->SetActorLocation(CurrentPoseTargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
    Target->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
    if (Target->GetCharacterMovement())
    {
        Target->GetCharacterMovement()->StopMovementImmediately();
        Target->GetCharacterMovement()->DisableMovement();
    }
    if (Target->GetCombatAimHitbox())
    {
        Target->GetCombatAimHitbox()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Target->GetCombatAimHitbox()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Target->GetCombatAimHitbox()->UpdateComponentToWorld();
    }
    Target->ForceNetUpdate();
    bCombatFallbackPoseActivated = true;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian combat fallback probe: current_pose_primed target=%s historical_pose_off_ray=yes location=%s"),
        *GetNameSafe(Target),
        *Target->GetActorLocation().ToString());
}

void ABMSignalControlGameMode::DispatchCombatProbeReload(const bool bSecondAttempt)
{
    ABMPlayerCharacter* Shooter = CombatProbeShooter.Get();
    if (!Shooter)
    {
        return;
    }

    if (ABMPlayerController* Controller = Cast<ABMPlayerController>(Shooter->GetController()))
    {
        Controller->StartCombatProbeReload();
        if (bSecondAttempt)
        {
            bCombatProbeReloadSecondDispatched = true;
        }
        else
        {
            bCombatProbeReloadDispatched = true;
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat probe: server dispatched client reload attempt=%s shooter=%s"),
            bSecondAttempt ? TEXT("second") : TEXT("first"),
            *GetNameSafe(Shooter));
    }
}

void ABMSignalControlGameMode::DispatchCombatProbeReloadInterrupt()
{
    ABMPlayerCharacter* Shooter = CombatProbeShooter.Get();
    if (!Shooter)
    {
        return;
    }

    if (ABMPlayerController* Controller = Cast<ABMPlayerController>(Shooter->GetController()))
    {
        Controller->StartCombatProbeReloadInterrupt();
        bCombatProbeReloadInterruptDispatched = true;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat probe: server dispatched client reload interrupt shooter=%s"),
            *GetNameSafe(Shooter));
    }
}
#endif

void ABMSignalControlGameMode::SpawnReplacementBot(const EBMTeam Team, const FVector& RequestedLocation)
{
    if (!HasAuthority() || !GetWorld() || Team == EBMTeam::Neutral)
    {
        return;
    }

    const APlayerStart* SafeStart = ChooseSafestStart(Team);
    const FVector SpawnLocation = RequestedLocation.IsNearlyZero()
        ? (SafeStart ? SafeStart->GetActorLocation() : FVector::ZeroVector)
        : RequestedLocation;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian signal: spawning bot team=%d requested=%s location=%s"),
        static_cast<int32>(Team),
        *RequestedLocation.ToString(),
        *SpawnLocation.ToString());
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    ABMPlayerCharacter* Bot = GetWorld()->SpawnActor<ABMPlayerCharacter>(
        ABMPlayerCharacter::StaticClass(), SpawnLocation, FRotator::ZeroRotator, SpawnParameters);
    ABMSignalBotController* Controller = GetWorld()->SpawnActor<ABMSignalBotController>(
        ABMSignalBotController::StaticClass(), SpawnLocation, FRotator::ZeroRotator, SpawnParameters);
    if (!Bot || !Controller)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Black Meridian signal: bot spawn failed bot=%s controller=%s"),
            Bot ? TEXT("yes") : TEXT("no"),
            Controller ? TEXT("yes") : TEXT("no"));
        if (Bot)
        {
            Bot->Destroy();
        }
        if (Controller)
        {
            Controller->Destroy();
        }
        return;
    }

    Bot->SetTeam(Team);
    Controller->SetBotTeam(Team);
    Controller->Possess(Bot);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian signal: bot ready name=%s controller=%s team=%d weapon=%s location=%s"),
        *Bot->GetName(),
        *Controller->GetName(),
        static_cast<int32>(Bot->GetTeam()),
        Bot->GetCurrentWeapon() ? TEXT("yes") : TEXT("no"),
        *Bot->GetActorLocation().ToString());
}

void ABMSignalControlGameMode::EnsureBotWeapons()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    const TArray<EBMWeaponId> WeaponIds = GetProfileLoadout();
    if (WeaponIds.Num() == 0)
    {
        return;
    }

    for (TActorIterator<ABMSignalBotController> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        ABMSignalBotController* BotController = *Iterator;
        ABMPlayerCharacter* Bot = BotController ? Cast<ABMPlayerCharacter>(BotController->GetPawn()) : nullptr;
        if (!Bot || Bot->IsEliminated() || Bot->GetCurrentWeapon())
        {
            continue;
        }

        if (Bot->RestoreLoadoutFromIds(WeaponIds, WeaponIds[0]))
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian signal: recovered bot weapon controller=%s weapon=%d loadout=%d"),
                *GetNameSafe(BotController),
                static_cast<int32>(Bot->GetCurrentWeaponId()),
                Bot->GetLoadoutWeaponIds().Num());
        }
    }
}

void ABMSignalControlGameMode::MaintainBotFill()
{
    if (!HasAuthority() || !GetWorld() || !bFillWithBots)
    {
        return;
    }

    constexpr int32 MaxMatchPlayers = 12;
    int32 HumanPlayerCount = 0;
    int32 LatticeCount = 0;
    int32 VantaCount = 0;
    for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
    {
        APlayerController* PlayerController = Iterator->Get();
        if (!PlayerController || PlayerController->IsPendingKillPending())
        {
            continue;
        }

        const ABMPlayerState* PlayerState = PlayerController->GetPlayerState<ABMPlayerState>();
        if (!PlayerState || PlayerState->IsInactive())
        {
            continue;
        }

        if (!PlayerController->IsLocalController() && PlayerController->GetNetConnection() == nullptr)
        {
            continue;
        }

        ++HumanPlayerCount;
        if (PlayerState)
        {
            if (PlayerState->Team == EBMTeam::Lattice)
            {
                ++LatticeCount;
            }
            else if (PlayerState->Team == EBMTeam::Vanta)
            {
                ++VantaCount;
            }
        }
    }

    TArray<ABMSignalBotController*> Bots;
    for (TActorIterator<ABMSignalBotController> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        if (ABMSignalBotController* Bot = *Iterator)
        {
            Bots.Add(Bot);
            if (Bot->GetBotTeam() == EBMTeam::Lattice)
            {
                ++LatticeCount;
            }
            else if (Bot->GetBotTeam() == EBMTeam::Vanta)
            {
                ++VantaCount;
            }
        }
    }

    const int32 DesiredBotCount = FMath::Clamp(
        MaxMatchPlayers - HumanPlayerCount,
        0,
        FMath::Max(0, BotCount));
    int32 CurrentBotCount = Bots.Num();
    for (int32 Index = Bots.Num() - 1; Index >= DesiredBotCount; --Index)
    {
        ABMSignalBotController* BotController = Bots[Index];
        if (!BotController)
        {
            continue;
        }
        if (APawn* BotPawn = BotController->GetPawn())
        {
            BotPawn->Destroy();
        }
        if (BotController->GetBotTeam() == EBMTeam::Lattice)
        {
            --LatticeCount;
        }
        else if (BotController->GetBotTeam() == EBMTeam::Vanta)
        {
            --VantaCount;
        }
        BotController->Destroy();
        --CurrentBotCount;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian signal: trimmed bot for human slot controller=%s"),
            *GetNameSafe(BotController));
    }

    while (CurrentBotCount < DesiredBotCount)
    {
        const EBMTeam Team = LatticeCount <= VantaCount ? EBMTeam::Lattice : EBMTeam::Vanta;
        SpawnReplacementBot(Team);
        ++CurrentBotCount;
        if (Team == EBMTeam::Lattice)
        {
            ++LatticeCount;
        }
        else
        {
            ++VantaCount;
        }
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian signal: bot fill humans=%d bots=%d target=%d teams=%d/%d"),
        HumanPlayerCount,
        CurrentBotCount,
        DesiredBotCount,
        LatticeCount,
        VantaCount);

    RefreshSignalLobbySnapshot();
}

void ABMSignalControlGameMode::RotateActiveNode()
{
    Nodes[ActiveNodeIndex]->SetActive(false);
    ActiveNodeIndex = (ActiveNodeIndex + 1) % Nodes.Num();
    Nodes[ActiveNodeIndex]->SetActive(true);
    RotationTimer = NodeRotationSeconds;
    UE_LOG(LogTemp, Display, TEXT("Black Meridian signal: active node rotated to %d"), ActiveNodeIndex);
}

void ABMSignalControlGameMode::FinishMatch(const EBMTeam WinningTeam)
{
    if (ABMGameState* State = GetGameState<ABMGameState>())
    {
        State->SignalControl.bMatchFinished = true;
        State->SignalControl.WinningTeam = WinningTeam;
        State->bMatchLive = false;
        State->SignalControl.SecondsUntilRotation = 0.0f;
        State->TeamScore = WinningTeam == EBMTeam::Lattice ? State->SignalControl.LatticeScore : State->SignalControl.VantaScore;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian signal: match finished winner=%d lattice_score=%d vanta_score=%d"),
            static_cast<int32>(WinningTeam),
            State->SignalControl.LatticeScore,
            State->SignalControl.VantaScore);
    }
    RecordMultiplayerMatch();
    for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
    {
        if (ABMPlayerController* Controller = Cast<ABMPlayerController>(Iterator->Get()))
        {
            Controller->ShowResultsScreen();
        }
    }
}
