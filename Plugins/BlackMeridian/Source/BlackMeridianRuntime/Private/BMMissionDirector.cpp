// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMMissionDirector.h"

#include "BMCampaignLevel.h"
#include "BMGreyboxDirector.h"
#include "BMCheckpointSaveGame.h"
#include "BMEnemyCharacter.h"
#include "BMGameMode.h"
#include "BMGameState.h"
#include "BMHealthComponent.h"
#include "BMPlayerCharacter.h"
#include "BMPlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"

namespace
{
    FString GetCheckpointSaveSlotName()
    {
#if !UE_BUILD_SHIPPING
        FString ProbeSlot;
        if (FParse::Value(FCommandLine::Get(), TEXT("BM_PersistenceSlot="), ProbeSlot) && !ProbeSlot.IsEmpty())
        {
            return ProbeSlot;
        }
#endif
        return TEXT("BlackMeridian_Campaign");
    }

    FName GetMissionMusicState(const EBMMissionPhase MissionPhase)
    {
        switch (MissionPhase)
        {
        case EBMMissionPhase::Checkpoint:
        case EBMMissionPhase::OpenCombat:
        case EBMMissionPhase::FinalDefense:
            return TEXT("Combat");
        case EBMMissionPhase::Failed:
            return TEXT("Failure");
        case EBMMissionPhase::Complete:
            return TEXT("Complete");
        case EBMMissionPhase::Intro:
            return TEXT("Mission");
        case EBMMissionPhase::Insertion:
        case EBMMissionPhase::QuietApproach:
        case EBMMissionPhase::InteriorBreach:
        case EBMMissionPhase::Intelligence:
        case EBMMissionPhase::Extraction:
        default:
            return TEXT("Suspense");
        }
    }
}

ABMMissionDirector::ABMMissionDirector()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = true;

    FBMPlayableCameraCue IntroCue;
    IntroCue.CueId = TEXT("InsertionReveal");
    IntroCue.Duration = 3.5f;
    IntroCue.StartCameraOffset = FVector(-96.0f, -42.0f, 18.0f);
    IntroCue.StartCameraRotation = FRotator(-8.0f, -8.0f, 0.0f);
    IntroCue.CameraOffset = FVector(0.0f, -18.0f, 8.0f);
    IntroCue.CameraRotation = FRotator(-3.0f, 0.0f, 0.0f);
    IntroCue.Subtitle = NSLOCTEXT("BlackMeridian", "PlayableIntroCue", "Mako: Nightglass, wheels down. Keep the water between us and the towers.");
    IntroCameraCues.Add(IntroCue);

    FBMPlayableCameraCue IntroRelayCue;
    IntroRelayCue.CueId = TEXT("RelaySweep");
    IntroRelayCue.Duration = 2.25f;
    IntroRelayCue.StartCameraOffset = FVector(74.0f, 54.0f, 26.0f);
    IntroRelayCue.StartCameraRotation = FRotator(-7.0f, 16.0f, 0.0f);
    IntroRelayCue.CameraOffset = FVector(34.0f, 22.0f, 14.0f);
    IntroRelayCue.CameraRotation = FRotator(-2.0f, 7.0f, 0.0f);
    IntroRelayCue.Subtitle = NSLOCTEXT("BlackMeridian", "PlayableIntroRelayCue", "Sable: Relay lights are still moving. We cross before the next sweep.");
    IntroCameraCues.Add(IntroRelayCue);

    FBMPlayableCameraCue IntroStepOffCue;
    IntroStepOffCue.CueId = TEXT("StepOff");
    IntroStepOffCue.Duration = 1.75f;
    IntroStepOffCue.StartCameraOffset = FVector(30.0f, -14.0f, 8.0f);
    IntroStepOffCue.StartCameraRotation = FRotator(-4.0f, 3.0f, 0.0f);
    IntroStepOffCue.CameraOffset = FVector(8.0f, 0.0f, 3.0f);
    IntroStepOffCue.CameraRotation = FRotator(-1.0f, 0.0f, 0.0f);
    IntroStepOffCue.Subtitle = NSLOCTEXT("BlackMeridian", "PlayableIntroStepOffCue", "Mako: Move when the beacon turns teal. No noise until the checkpoint.");
    IntroCameraCues.Add(IntroStepOffCue);

    FBMPlayableCameraCue SetPieceFlashCue;
    SetPieceFlashCue.CueId = TEXT("SeawallFlash");
    SetPieceFlashCue.Duration = 1.2f;
    SetPieceFlashCue.StartCameraOffset = FVector(-54.0f, -18.0f, 14.0f);
    SetPieceFlashCue.StartCameraRotation = FRotator(-6.0f, -5.0f, 0.0f);
    SetPieceFlashCue.CameraOffset = FVector(-12.0f, 18.0f, 6.0f);
    SetPieceFlashCue.CameraRotation = FRotator(-2.0f, 3.0f, 0.0f);
    SetPieceFlashCue.Subtitle = NSLOCTEXT("BlackMeridian", "PlayableSetPieceFlashCue", "Vanta command: Light the seawall. Nothing leaves the district.");
    SetPieceCameraCues.Add(SetPieceFlashCue);

    FBMPlayableCameraCue SetPieceCollapseCue;
    SetPieceCollapseCue.CueId = TEXT("RelayCollapse");
    SetPieceCollapseCue.Duration = 1.8f;
    SetPieceCollapseCue.StartCameraOffset = FVector(8.0f, 22.0f, 6.0f);
    SetPieceCollapseCue.StartCameraRotation = FRotator(-2.0f, 6.0f, 0.0f);
    SetPieceCollapseCue.CameraOffset = FVector(42.0f, -20.0f, 16.0f);
    SetPieceCollapseCue.CameraRotation = FRotator(-7.0f, -8.0f, 0.0f);
    SetPieceCollapseCue.Subtitle = NSLOCTEXT("BlackMeridian", "PlayableSetPieceCollapseCue", "Sable: Relay house is coming down. Move through the seawall gap.");
    SetPieceCameraCues.Add(SetPieceCollapseCue);

    FBMPlayableCameraCue SetPieceReturnCue;
    SetPieceReturnCue.CueId = TEXT("SeawallReturn");
    SetPieceReturnCue.Duration = 1.0f;
    SetPieceReturnCue.StartCameraOffset = FVector(18.0f, -12.0f, 8.0f);
    SetPieceReturnCue.StartCameraRotation = FRotator(-4.0f, -3.0f, 0.0f);
    SetPieceReturnCue.CameraOffset = FVector::ZeroVector;
    SetPieceReturnCue.CameraRotation = FRotator::ZeroRotator;
    SetPieceReturnCue.Subtitle = NSLOCTEXT("BlackMeridian", "PlayableSetPieceReturnCue", "Sable: Seawall is live. Break the encirclement.");
    SetPieceCameraCues.Add(SetPieceReturnCue);

    FBMPlayableCameraCue ExtractionCue;
    ExtractionCue.CueId = TEXT("ExtractionApproach");
    ExtractionCue.Duration = 2.5f;
    ExtractionCue.StartCameraOffset = FVector(-28.0f, 28.0f, 12.0f);
    ExtractionCue.StartCameraRotation = FRotator(-5.0f, 10.0f, 0.0f);
    ExtractionCue.CameraOffset = FVector(0.0f, 12.0f, 4.0f);
    ExtractionCue.CameraRotation = FRotator(-1.5f, 5.0f, 0.0f);
    ExtractionCue.Subtitle = NSLOCTEXT("BlackMeridian", "PlayableExtractionCue", "Sable: Pier ahead. Keep the relay off our backs.");
    ExtractionCameraCues.Add(ExtractionCue);

    FBMPlayableCameraCue ExtractionBeaconCue;
    ExtractionBeaconCue.CueId = TEXT("ExtractionBeacon");
    ExtractionBeaconCue.Duration = 2.0f;
    ExtractionBeaconCue.StartCameraOffset = FVector(42.0f, -34.0f, 18.0f);
    ExtractionBeaconCue.StartCameraRotation = FRotator(-5.0f, -12.0f, 0.0f);
    ExtractionBeaconCue.CameraOffset = FVector(18.0f, -20.0f, 8.0f);
    ExtractionBeaconCue.CameraRotation = FRotator(-2.0f, -6.0f, 0.0f);
    ExtractionBeaconCue.Subtitle = NSLOCTEXT("BlackMeridian", "PlayableExtractionBeaconCue", "Mako: Beacon is live. Get to the boat before the seawall finds us.");
    ExtractionCameraCues.Add(ExtractionBeaconCue);

    FBMPlayableCameraCue ExtractionStepOffCue;
    ExtractionStepOffCue.CueId = TEXT("ExtractionStepOff");
    ExtractionStepOffCue.Duration = 1.75f;
    ExtractionStepOffCue.StartCameraOffset = FVector(24.0f, -8.0f, 8.0f);
    ExtractionStepOffCue.StartCameraRotation = FRotator(-3.0f, -2.0f, 0.0f);
    ExtractionStepOffCue.CameraOffset = FVector(6.0f, 0.0f, 2.0f);
    ExtractionStepOffCue.CameraRotation = FRotator::ZeroRotator;
    ExtractionStepOffCue.Subtitle = NSLOCTEXT("BlackMeridian", "PlayableExtractionStepOffCue", "Sable: Signal the boat, then hold the pier. They will come through the dark.");
    ExtractionCameraCues.Add(ExtractionStepOffCue);

    FBMPlayableCameraCue EndingCue;
    EndingCue.CueId = TEXT("ArchiveAftermath");
    EndingCue.Duration = 4.0f;
    EndingCue.StartCameraOffset = FVector(-34.0f, -12.0f, 18.0f);
    EndingCue.StartCameraRotation = FRotator(-9.0f, 6.0f, 0.0f);
    EndingCue.CameraOffset = FVector(0.0f, 0.0f, 12.0f);
    EndingCue.CameraRotation = FRotator(-5.0f, -8.0f, 0.0f);
    EndingCue.Subtitle = NSLOCTEXT("BlackMeridian", "PlayableEndingCue", "Mako: We have the archive. Now find out who taught the city to listen.");
    EndingCameraCues.Add(EndingCue);

    FBMPlayableCameraCue EndingSignalCue;
    EndingSignalCue.CueId = TEXT("SignalAftermath");
    EndingSignalCue.Duration = 2.75f;
    EndingSignalCue.StartCameraOffset = FVector(36.0f, -22.0f, 26.0f);
    EndingSignalCue.StartCameraRotation = FRotator(-11.0f, -2.0f, 0.0f);
    EndingSignalCue.CameraOffset = FVector(18.0f, 18.0f, 22.0f);
    EndingSignalCue.CameraRotation = FRotator(-8.0f, 8.0f, 0.0f);
    EndingSignalCue.Subtitle = NSLOCTEXT("BlackMeridian", "PlayableEndingSignalCue", "Sable: The trigger is older than Vanta. Someone built a network under the city.");
    EndingCameraCues.Add(EndingSignalCue);

    FBMPlayableCameraCue EndingResolveCue;
    EndingResolveCue.CueId = TEXT("Resolve");
    EndingResolveCue.Duration = 2.5f;
    EndingResolveCue.StartCameraOffset = FVector(18.0f, 12.0f, 12.0f);
    EndingResolveCue.StartCameraRotation = FRotator(-6.0f, 4.0f, 0.0f);
    EndingResolveCue.CameraOffset = FVector(4.0f, 0.0f, 6.0f);
    EndingResolveCue.CameraRotation = FRotator(-3.0f, 0.0f, 0.0f);
    EndingResolveCue.Subtitle = NSLOCTEXT("BlackMeridian", "PlayableEndingResolveCue", "Mako: Nightglass complete. The next relay is already listening.");
    EndingCameraCues.Add(EndingResolveCue);
}

void ABMMissionDirector::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    bDeterministicProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_MissionProbe"));
#endif
    if (ABMGameState* GameState = GetWorld()->GetGameState<ABMGameState>())
    {
        GameState->SetMissionPhase(EBMMissionPhase::Intro);
        FBMObjectiveState Objective;
        Objective.ObjectiveId = TEXT("Briefing");
        Objective.Title = NSLOCTEXT("BlackMeridian", "BriefingObjective", "AWAIT BRIEFING");
        Objective.State = EBMObjectiveState::Active;
        GameState->SetObjective(Objective);
    }
}

void ABMMissionDirector::StartMission()
{
    if (!HasAuthority() || bMissionStarted)
    {
        return;
    }

    bMissionStarted = true;
    MissionElapsed = 0.0f;
    PhaseElapsed = 0.0f;
    SetPhase(
        EBMMissionPhase::Insertion,
        TEXT("Insertion"),
        NSLOCTEXT("BlackMeridian", "InsertionObjective", "MOVE TO THE TEAL BEACON"));
    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
    FString OpeningBriefing = Level.Briefing.ToString();
    OpeningBriefing += TEXT("\n\nTEAL = MOVE  //  AMBER = FIGHT  //  ORANGE = BREACH\nYELLOW = EXTRACT  //  HOLD E ONLY FOR MISSION OBJECTIVES\nWEAPON AND AMMO PICKUPS AUTO-COLLECT WHEN YOU MOVE CLOSE.");
    ShowMissionCardToSquad(
        Level.DisplayName,
        FText::FromString(OpeningBriefing),
        8.0f);
    StartPlayableSequence(TEXT("Intro"));
}

void ABMMissionDirector::RestartFromCheckpoint()
{
    if (!HasAuthority())
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(EndingResultsTimer);

    const bool bLoaded = LoadCheckpoint();
    int32 LoadedCheckpointIndex = 0;
    if (ABMGameState* GameState = GetWorld()->GetGameState<ABMGameState>())
    {
        GameState->CheckpointIndex = FMath::Max(0, GameState->CheckpointIndex);
        LoadedCheckpointIndex = GameState->CheckpointIndex;
    }
    if (!bLoaded)
    {
        bOptionalIntelSecured = false;
        bSignalJammerPlanted = false;
        if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
        {
            Player->RestoreForCheckpoint(100.0f);
            Player->RestoreThrowableInventory(2, 1);
        }
    }
    EBMMissionPhase ResumePhase = EBMMissionPhase::Insertion;
    if (bLoaded && LoadedCheckpointIndex > 0)
    {
        // Checkpoint saves written before ResumePhase existed only recorded
        // index 1. Keep those saves playable by falling back to the old
        // intelligence hand-off, while new saves resume the actual encounter
        // that killed the player.
        switch (LoadedCheckpointPhase)
        {
        case EBMMissionPhase::Checkpoint:
        case EBMMissionPhase::Intelligence:
        case EBMMissionPhase::OpenCombat:
        case EBMMissionPhase::FinalDefense:
            ResumePhase = LoadedCheckpointPhase;
            break;
        default:
            ResumePhase = EBMMissionPhase::Intelligence;
            break;
        }
    }

    if (ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>())
    {
        if (ABMGreyboxDirector* Greybox = GameMode->GetGreyboxDirector())
        {
            Greybox->ResetForCheckpoint(ResumePhase);
        }
    }
    bMissionStarted = false;
    StartMission();
    if (ResumePhase != EBMMissionPhase::Insertion)
    {
        StopPlayableSequence();
        switch (ResumePhase)
        {
        case EBMMissionPhase::Checkpoint:
            SetPhase(EBMMissionPhase::Checkpoint, TEXT("Checkpoint"), NSLOCTEXT("BlackMeridian", "RestartCheckpointObjective", "CLEAR THE AMBER CHECKPOINT"));
            break;
        case EBMMissionPhase::OpenCombat:
            SetPhase(EBMMissionPhase::OpenCombat, TEXT("OpenCombat"), NSLOCTEXT("BlackMeridian", "RestartOpenCombatObjective", "CLEAR THE SEAWALL"));
            break;
        case EBMMissionPhase::FinalDefense:
            SetPhase(EBMMissionPhase::FinalDefense, TEXT("FinalDefense"), NSLOCTEXT("BlackMeridian", "RestartFinalDefenseObjective", "HOLD THE PIER UNTIL THE BOAT ARRIVES"));
            break;
        case EBMMissionPhase::Intelligence:
        default:
            SetPhase(EBMMissionPhase::Intelligence, TEXT("IntelConsole"), NSLOCTEXT("BlackMeridian", "RestartIntelObjective", "OPTIONAL: DOWNLOAD THE SIGNAL ARCHIVE"), true);
            break;
        }
    }
}

void ABMMissionDirector::SaveCheckpoint()
{
    if (!HasAuthority())
    {
        return;
    }

    UBMCheckpointSaveGame* SaveGame = Cast<UBMCheckpointSaveGame>(UGameplayStatics::CreateSaveGameObject(UBMCheckpointSaveGame::StaticClass()));
    if (!SaveGame)
    {
        return;
    }
    if (const ABMGameState* GameState = GetWorld()->GetGameState<ABMGameState>())
    {
        SaveGame->CheckpointIndex = GameState->CheckpointIndex;
        SaveGame->CampaignLevel = GameState->CampaignLevel;
    }
    else
    {
        SaveGame->CampaignLevel = CampaignLevel;
    }
    SaveGame->ResumePhase = Phase;
    SaveGame->bOptionalIntelSecured = bOptionalIntelSecured;
    SaveGame->bSignalJammerPlanted = bSignalJammerPlanted;
    if (const ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
    {
        SaveGame->PlayerHealth = Player->HealthComponent ? Player->HealthComponent->Health : 100.0f;
        SaveGame->Loadout = Player->GetLoadoutWeaponIds();
        SaveGame->CurrentWeapon = Player->GetCurrentWeaponId();
        SaveGame->FragCount = Player->GetFragCount();
        SaveGame->VeilCount = Player->GetVeilCount();
        SaveGame->PlayerLocation = Player->GetActorLocation();
        SaveGame->PlayerRotation = Player->GetActorRotation();
        SaveGame->bHasPlayerTransform = true;
    }
    const FString SaveSlot = GetCheckpointSaveSlotName();
    const bool bSaved = UGameplayStatics::SaveGameToSlot(SaveGame, SaveSlot, 0);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian persistence: checkpoint save slot=%s success=%s level=%d phase=%d index=%d"),
        *SaveSlot,
        bSaved ? TEXT("yes") : TEXT("no"),
        static_cast<int32>(SaveGame->CampaignLevel),
        static_cast<int32>(SaveGame->ResumePhase),
        SaveGame->CheckpointIndex);
}

bool ABMMissionDirector::LoadCheckpoint()
{
    if (!HasAuthority())
    {
        return false;
    }

    const FString SaveSlot = GetCheckpointSaveSlotName();
    UBMCheckpointSaveGame* SaveGame = Cast<UBMCheckpointSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
    if (!SaveGame)
    {
        UE_LOG(LogTemp, Warning, TEXT("Black Meridian persistence: checkpoint load missing slot=%s"), *SaveSlot);
        return false;
    }
    bOptionalIntelSecured = SaveGame->bOptionalIntelSecured;
    bSignalJammerPlanted = SaveGame->bSignalJammerPlanted;
    CampaignLevel = static_cast<EBMCampaignLevel>(FMath::Clamp(
        static_cast<int32>(SaveGame->CampaignLevel),
        static_cast<int32>(EBMCampaignLevel::SableQuay),
        GetBMCampaignLevelCount() - 1));
    if (ABMGameState* GameState = GetWorld()->GetGameState<ABMGameState>())
    {
        GameState->CheckpointIndex = SaveGame->CheckpointIndex;
        GameState->CampaignLevel = CampaignLevel;
        GameState->ForceNetUpdate();
    }
    if (ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>())
    {
        GameMode->CampaignLevel = CampaignLevel;
        if (ABMGreyboxDirector* Greybox = GameMode->GetGreyboxDirector())
        {
            Greybox->SetCampaignLevel(CampaignLevel);
        }
    }
    LoadedCheckpointPhase = SaveGame->ResumePhase;
    if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
    {
        if (SaveGame->bHasPlayerTransform)
        {
            Player->SetActorLocationAndRotation(SaveGame->PlayerLocation, SaveGame->PlayerRotation, false, nullptr, ETeleportType::TeleportPhysics);
        }
        Player->RestoreForCheckpoint(SaveGame->PlayerHealth);
        Player->RestoreLoadoutFromIds(SaveGame->Loadout, SaveGame->CurrentWeapon);
        Player->RestoreThrowableInventory(SaveGame->FragCount, SaveGame->VeilCount);
    }
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian persistence: checkpoint load slot=%s level=%d phase=%d index=%d transform=%s health=%.1f loadout=%d current=%d frag=%d veil=%d intel=%s jammer=%s"),
        *SaveSlot,
        static_cast<int32>(CampaignLevel),
        static_cast<int32>(SaveGame->ResumePhase),
        SaveGame->CheckpointIndex,
        SaveGame->bHasPlayerTransform ? TEXT("yes") : TEXT("no"),
        SaveGame->PlayerHealth,
        SaveGame->Loadout.Num(),
        static_cast<int32>(SaveGame->CurrentWeapon),
        SaveGame->FragCount,
        SaveGame->VeilCount,
        SaveGame->bOptionalIntelSecured ? TEXT("yes") : TEXT("no"),
        SaveGame->bSignalJammerPlanted ? TEXT("yes") : TEXT("no"));
    return true;
}

void ABMMissionDirector::NotifyInteraction(AActor* Interactor, const FName InteractionId)
{
    if (!HasAuthority() || !bMissionStarted)
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian mission: interaction id=%s phase=%d interactor=%s"),
        *InteractionId.ToString(),
        static_cast<int32>(Phase),
        *GetNameSafe(Interactor));

#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_InteractionProbe")))
    {
        if (InteractionId == TEXT("RelayBreach"))
        {
            bInteractionProbeRelayCompleted = true;
        }
        else if (InteractionId == TEXT("IntelConsole"))
        {
            bInteractionProbeConsoleCompleted = true;
        }
        else if (InteractionId == TEXT("ExtractionBoat"))
        {
            bInteractionProbeExtractionCompleted = true;
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian interaction probe: completed id=%s"),
            *InteractionId.ToString());
    }
#endif

    if (InteractionId == TEXT("IntelConsole") && Phase == EBMMissionPhase::Intelligence)
    {
        bOptionalIntelSecured = true;
        SetPhase(
            EBMMissionPhase::OpenCombat,
            TEXT("OpenCombat"),
            NSLOCTEXT("BlackMeridian", "OpenCombatObjective", "BREAK THE ENCIRCLEMENT"));
        if (ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>())
        {
            if (ABMGreyboxDirector* Greybox = GameMode->GetGreyboxDirector())
            {
                const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
                Greybox->SpawnCombatWave(Level.OpenCombatWaveSize, Level.OpenCombatLocation);
                Greybox->TriggerEnvironmentalEvent();
            }
        }
        ShowSubtitleToSquad(NSLOCTEXT("BlackMeridian", "IntelLine", "Sable: Archive is dirty. They were not guarding a server; they were building a trigger."), 4.0f);
    }
    else if (InteractionId == TEXT("RelayBreach") && Phase == EBMMissionPhase::InteriorBreach)
    {
        if (ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>())
        {
            if (ABMGreyboxDirector* Greybox = GameMode->GetGreyboxDirector())
            {
                Greybox->OpenBreachBarrier();
            }
        }
        // The archive is optional; the player should continue toward the
        // seawall beacon even if they do not stop at the console. Making the
        // seawall the phase destination prevents merely walking through the
        // relay house from silently starting the next firefight.
        SetPhase(EBMMissionPhase::Intelligence, TEXT("OpenCombat"), NSLOCTEXT("BlackMeridian", "IntelObjectiveAfterBreach", "REACH THE SEAWALL  //  ARCHIVE OPTIONAL"));
        ShowSubtitleToSquad(NSLOCTEXT("BlackMeridian", "BreachLine", "Sable: Door is open. Console is inside. Keep the relay dark."), 3.0f);
    }
    else if (InteractionId == TEXT("ExtractionBoat") && Phase == EBMMissionPhase::Extraction)
    {
        SetPhase(EBMMissionPhase::FinalDefense, TEXT("FinalDefense"), NSLOCTEXT("BlackMeridian", "FinalDefenseObjectiveAfterBoat", "HOLD THE PIER UNTIL THE BOAT ARRIVES"));
        if (ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>())
        {
            if (ABMGreyboxDirector* Greybox = GameMode->GetGreyboxDirector())
            {
                const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
                Greybox->SpawnCombatWave(Level.FinalDefenseWaveSize, Level.ExtractionLocation);
            }
        }
    }
    else if (InteractionId == TEXT("PlantSignalJammer") && Phase == EBMMissionPhase::OpenCombat)
    {
        bSignalJammerPlanted = true;
        ShowSubtitleToSquad(
            NSLOCTEXT("BlackMeridian", "JammerLine", "Sable: Jammer is live. Their reinforcements will arrive blind."),
            3.0f);
    }
}

void ABMMissionDirector::NotifyEnemyEliminated(ABMEnemyCharacter* EnemyCharacter)
{
    if (!HasAuthority() || !IsValid(EnemyCharacter))
    {
        return;
    }

    ++EliminationsInPhase;

    int32 HostileCount = INDEX_NONE;
    if (const ABMGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr)
    {
        HostileCount = GameState->HostileCount;
    }

    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian mission: enemy eliminated phase=%d eliminations=%d required=%d hostiles_remaining=%d enemy=%s"),
        static_cast<int32>(Phase),
        EliminationsInPhase,
        Phase == EBMMissionPhase::Checkpoint ? Level.CheckpointWaveSize : -1,
        HostileCount,
        *GetNameSafe(EnemyCharacter));

    // Do not make the player wait for the next mission tick after the final
    // opening-wave kill. The game mode has already decremented HostileCount
    // when it calls this method, so either signal is authoritative. The
    // count fallback also covers a wave where an enemy death was delivered by
    // a non-standard damage path but the world is demonstrably clear.
    if (Phase == EBMMissionPhase::Checkpoint &&
        (EliminationsInPhase >= Level.CheckpointWaveSize ||
         (EliminationsInPhase > 0 && HostileCount == 0)))
    {
        SetPhase(
            EBMMissionPhase::InteriorBreach,
            TEXT("InteriorBreach"),
            NSLOCTEXT("BlackMeridian", "BreachObjective", "REACH THE ORANGE RELAY HOUSE"));
    }
}

void ABMMissionDirector::NotifyPlayerEliminated()
{
    if (HasAuthority())
    {
        SetPhase(EBMMissionPhase::Failed, TEXT("Failed"), NSLOCTEXT("BlackMeridian", "MissionFailed", "SQUAD LOST — RESTART FROM CHECKPOINT"));
    }
}

const TArray<FBMPlayableCameraCue>* ABMMissionDirector::FindPlayableSequence(const FName SequenceId) const
{
    if (SequenceId == TEXT("Intro"))
    {
        return &IntroCameraCues;
    }
    if (SequenceId == TEXT("Extraction"))
    {
        return &ExtractionCameraCues;
    }
    if (SequenceId == TEXT("SetPiece"))
    {
        return &SetPieceCameraCues;
    }
    if (SequenceId == TEXT("Ending"))
    {
        return &EndingCameraCues;
    }
    return nullptr;
}

void ABMMissionDirector::StartPlayableSequence(const FName SequenceId)
{
    if (!HasAuthority())
    {
        return;
    }

    const TArray<FBMPlayableCameraCue>* Sequence = FindPlayableSequence(SequenceId);
    if (SequenceId.IsNone() || !Sequence || Sequence->Num() == 0)
    {
        StopPlayableSequence();
        return;
    }

    ActivePlayableSequence = SequenceId;
    ActivePlayableCueIndex = 0;
    ActivePlayableCueElapsed = 0.0f;
    NotifyPlayableCueStarted();
    float SequenceDuration = 0.0f;
    for (const FBMPlayableCameraCue& Cue : *Sequence)
    {
        SequenceDuration += FMath::Max(0.05f, Cue.Duration);
    }
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian cinematic: sequence started id=%s cues=%d duration=%.2fs control=preserved"),
        *SequenceId.ToString(),
        Sequence->Num(),
        SequenceDuration);
    ForceNetUpdate();
}

void ABMMissionDirector::StopPlayableSequence()
{
    if (!HasAuthority())
    {
        return;
    }

    if (IsPlayableSequenceActive())
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian cinematic: sequence stopped id=%s cue=%d"),
            *ActivePlayableSequence.ToString(),
            ActivePlayableCueIndex);
    }
    ActivePlayableSequence = NAME_None;
    ActivePlayableCueIndex = INDEX_NONE;
    ActivePlayableCueElapsed = 0.0f;
    ForceNetUpdate();
}

FBMPlayableCameraCue ABMMissionDirector::GetActivePlayableCameraCue() const
{
    if (const TArray<FBMPlayableCameraCue>* Sequence = FindPlayableSequence(ActivePlayableSequence);
        Sequence && Sequence->IsValidIndex(ActivePlayableCueIndex))
    {
        return (*Sequence)[ActivePlayableCueIndex];
    }
    return FBMPlayableCameraCue();
}

float ABMMissionDirector::GetActivePlayableCameraCueProgress() const
{
    if (!IsPlayableSequenceActive())
    {
        return 0.0f;
    }

    const FBMPlayableCameraCue Cue = GetActivePlayableCameraCue();
    return FMath::Clamp(
        ActivePlayableCueElapsed / FMath::Max(0.05f, Cue.Duration),
        0.0f,
        1.0f);
}

bool ABMMissionDirector::IsPlayableSequenceLookLocked() const
{
    return IsPlayableSequenceActive() && GetActivePlayableCameraCue().bLockLook;
}

void ABMMissionDirector::NotifyPlayableCueStarted()
{
    const FBMPlayableCameraCue Cue = GetActivePlayableCameraCue();
    if (!Cue.Subtitle.IsEmpty())
    {
        ShowSubtitleToSquad(Cue.Subtitle, FMath::Max(0.5f, Cue.Duration));
    }
}

void ABMMissionDirector::UpdatePlayableSequence(const float DeltaSeconds)
{
    if (!IsPlayableSequenceActive())
    {
        return;
    }

    const TArray<FBMPlayableCameraCue>* Sequence = FindPlayableSequence(ActivePlayableSequence);
    if (!Sequence || !Sequence->IsValidIndex(ActivePlayableCueIndex))
    {
        if (HasAuthority())
        {
            StopPlayableSequence();
        }
        return;
    }

    ActivePlayableCueElapsed += FMath::Max(0.0f, DeltaSeconds);
    if (!HasAuthority())
    {
        return;
    }

    while (Sequence->IsValidIndex(ActivePlayableCueIndex) &&
           ActivePlayableCueElapsed >= FMath::Max(0.05f, (*Sequence)[ActivePlayableCueIndex].Duration))
    {
        ActivePlayableCueElapsed -= FMath::Max(0.05f, (*Sequence)[ActivePlayableCueIndex].Duration);
        ++ActivePlayableCueIndex;
        if (Sequence->IsValidIndex(ActivePlayableCueIndex))
        {
            NotifyPlayableCueStarted();
            ForceNetUpdate();
        }
        else
        {
            StopPlayableSequence();
            return;
        }
    }
}

void ABMMissionDirector::SetPhase(
    const EBMMissionPhase NewPhase,
    const FName ObjectiveId,
    const FText& Title,
    const bool bOptional)
{
    const EBMMissionPhase PreviousPhase = Phase;
    const float PreviousPhaseElapsed = PhaseElapsed;
    Phase = NewPhase;
    PhaseElapsed = 0.0f;
    EliminationsInPhase = 0;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian mission: phase %d -> %d (mission_elapsed=%.2fs, previous_phase_elapsed=%.2fs, objective=%s)"),
        static_cast<int32>(PreviousPhase),
        static_cast<int32>(NewPhase),
        MissionElapsed,
        PreviousPhaseElapsed,
        *ObjectiveId.ToString());

    if (ABMGameState* GameState = GetWorld()->GetGameState<ABMGameState>())
    {
        GameState->SetMissionPhase(NewPhase);
        FBMObjectiveState Objective;
        Objective.ObjectiveId = ObjectiveId;
        Objective.Title = Title;
        Objective.State = bOptional ? EBMObjectiveState::Optional : EBMObjectiveState::Active;
        Objective.bOptional = bOptional;
        Objective.WorldLocation = GetObjectiveLocation(ObjectiveId);
        Objective.bHasWorldLocation = !Objective.WorldLocation.IsNearlyZero();
        GameState->SetObjective(Objective);
        GameState->CheckpointIndex = NewPhase == EBMMissionPhase::Checkpoint || NewPhase == EBMMissionPhase::Intelligence ? 1 : GameState->CheckpointIndex;
    }

    const FName MusicState = GetMissionMusicState(NewPhase);
    for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
    {
        if (ABMPlayerController* Controller = Cast<ABMPlayerController>(Iterator->Get()))
        {
            Controller->PlayMusicState(MusicState);
        }
    }

    // The orange relay is the campaign's second safe hand-off. Preserve the
    // checkpoint's meaning by giving the player a full-health reset before we
    // write the save that feeds the next encounter. Otherwise chip damage
    // from the amber checkpoint can turn the seawall spawn into an immediate
    // and unreadable mission failure.
    if (NewPhase == EBMMissionPhase::Intelligence)
    {
        if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
        {
            Player->RestoreForCheckpoint(100.0f);
            UE_LOG(LogTemp, Display, TEXT("Black Meridian mission: orange relay checkpoint restored player health"));
        }
    }
    if (ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>())
    {
        if (ABMGreyboxDirector* Greybox = GameMode->GetGreyboxDirector())
        {
            Greybox->RefreshObjectivePresentation();
        }
    }
    if (NewPhase == EBMMissionPhase::Checkpoint || NewPhase == EBMMissionPhase::Intelligence)
    {
        SaveCheckpoint();
    }

    // The persistent objective tells the player where to go. This short card
    // also tells a first-time player what action the new phase expects.
    // Insertion already has its longer opening card in StartMission.
    FText PhaseCardTitle;
    FText PhaseCardBody;
    switch (NewPhase)
    {
    case EBMMissionPhase::QuietApproach:
        PhaseCardTitle = NSLOCTEXT("BlackMeridian", "QuietApproachCardTitle", "ROUTE // AMBER CHECKPOINT");
        PhaseCardBody = NSLOCTEXT("BlackMeridian", "QuietApproachCardBody", "WALK TO THE AMBER BEACON\nSTOP THERE — CONTACT STARTS IN THIS AREA");
        break;
    case EBMMissionPhase::Checkpoint:
        PhaseCardTitle = NSLOCTEXT("BlackMeridian", "CheckpointCardTitle", "CONTACT // CLEAR THE CHECKPOINT");
        PhaseCardBody = NSLOCTEXT("BlackMeridian", "CheckpointCardBody", "STAY AT THE AMBER CHECKPOINT\nRED VANTA ARE THE TARGETS\nRMB AIM   //   LMB FIRE   //   R RELOAD");
        break;
    case EBMMissionPhase::InteriorBreach:
        PhaseCardTitle = NSLOCTEXT("BlackMeridian", "InteriorBreachCardTitle", "BREACH // ORANGE RELAY HOUSE");
        PhaseCardBody = NSLOCTEXT("BlackMeridian", "InteriorBreachCardBody", "FOLLOW THE ORANGE BEACON\nHOLD E UNTIL THE BREACH BAR COMPLETES");
        break;
    case EBMMissionPhase::Intelligence:
        PhaseCardTitle = NSLOCTEXT("BlackMeridian", "IntelligenceCardTitle", "OPTIONAL INTEL // TEAL CONSOLE");
        PhaseCardBody = NSLOCTEXT("BlackMeridian", "IntelligenceCardBody", "HOLD E FOR THE ARCHIVE (OPTIONAL)\nTHEN FOLLOW THE BEACON TO THE SEAWALL");
        break;
    case EBMMissionPhase::OpenCombat:
        PhaseCardTitle = NSLOCTEXT("BlackMeridian", "OpenCombatCardTitle", "SEAWALL // BREAK THE ENCIRCLEMENT");
        PhaseCardBody = NSLOCTEXT("BlackMeridian", "OpenCombatCardBody", "USE COVER   //   LMB FIRE   //   G FRAG   //   X VEIL\nCLEAR THE SEAWALL WAVE");
        break;
    case EBMMissionPhase::Extraction:
        PhaseCardTitle = NSLOCTEXT("BlackMeridian", "ExtractionCardTitle", "EXTRACT // YELLOW BOAT");
        PhaseCardBody = NSLOCTEXT("BlackMeridian", "ExtractionCardBody", "FOLLOW THE YELLOW BEACON\nHOLD E AT THE BOAT TO SIGNAL EXTRACTION");
        break;
    case EBMMissionPhase::FinalDefense:
        PhaseCardTitle = NSLOCTEXT("BlackMeridian", "FinalDefenseCardTitle", "HOLD // DEFEND THE PIER");
        PhaseCardBody = NSLOCTEXT("BlackMeridian", "FinalDefenseCardBody", "STAY AT THE PIER\nSHOOT HOSTILES UNTIL THE HUD SAYS CLEAR");
        break;
    default:
        break;
    }
    if (!PhaseCardTitle.IsEmpty() && !PhaseCardBody.IsEmpty())
    {
        const bool bCombatPhase = NewPhase == EBMMissionPhase::Checkpoint
            || NewPhase == EBMMissionPhase::OpenCombat
            || NewPhase == EBMMissionPhase::FinalDefense;
        // Combat markers and the first incoming volley must become readable
        // before the tutorial card disappears. The card remains long enough
        // to teach the action, but it no longer hides the enemy labels during
        // the entire three-second hostile start delay.
        const float PhaseCardDuration = NewPhase == EBMMissionPhase::InteriorBreach
            ? 8.0f
            : (bCombatPhase ? 2.5f : 4.0f);
        ShowMissionCardToSquad(PhaseCardTitle, PhaseCardBody, PhaseCardDuration);
    }

    if (NewPhase == EBMMissionPhase::OpenCombat)
    {
        StartPlayableSequence(TEXT("SetPiece"));
    }
    else if (NewPhase == EBMMissionPhase::Extraction)
    {
        StartPlayableSequence(TEXT("Extraction"));
    }
    else if (NewPhase == EBMMissionPhase::Complete)
    {
        StartPlayableSequence(TEXT("Ending"));
    }
    OnMissionPhaseChanged.Broadcast(NewPhase);
    if (NewPhase == EBMMissionPhase::Complete)
    {
#if !UE_BUILD_SHIPPING
        if (FParse::Param(FCommandLine::Get(), TEXT("BM_InteractionProbe")))
        {
            const bool bProbePassed = bInteractionProbeRelayCompleted &&
                bInteractionProbeConsoleCompleted &&
                bInteractionProbeExtractionCompleted;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian interaction probe: %s relay=%s console=%s extraction=%s mission=complete"),
                bProbePassed ? TEXT("PASS") : TEXT("FAIL"),
                bInteractionProbeRelayCompleted ? TEXT("completed") : TEXT("missing"),
                bInteractionProbeConsoleCompleted ? TEXT("completed") : TEXT("missing"),
                bInteractionProbeExtractionCompleted ? TEXT("completed") : TEXT("missing"));
        }
#endif
        if (ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>())
        {
            GameMode->RecordCampaignCompletion();
        }
        ShowMissionCardToSquad(
            NSLOCTEXT("BlackMeridian", "EndingCardTitle", "SIGNAL ARCHIVE SECURED"),
            bSignalJammerPlanted
                ? NSLOCTEXT("BlackMeridian", "EndingCardBodyJammed", "THE CITY WAS ONLY THE FIRST RELAY. TONIGHT, IT COULD NOT ANSWER.")
                : NSLOCTEXT("BlackMeridian", "EndingCardBody", "THE CITY WAS ONLY THE FIRST RELAY."),
            5.0f);

        float EndingDuration = 0.0f;
        for (const FBMPlayableCameraCue& Cue : EndingCameraCues)
        {
            EndingDuration += FMath::Max(0.05f, Cue.Duration);
        }
        GetWorldTimerManager().SetTimer(
            EndingResultsTimer,
            this,
            &ABMMissionDirector::ShowResultsAfterEnding,
            FMath::Max(0.5f, EndingDuration + 0.35f),
            false);
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian mission: ending presentation started cues=%d duration=%.2fs results_deferred=yes"),
            EndingCameraCues.Num(),
            EndingDuration);
    }
}

void ABMMissionDirector::ShowResultsAfterEnding()
{
    if (!HasAuthority() || Phase != EBMMissionPhase::Complete)
    {
        return;
    }

    StopPlayableSequence();
    for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
    {
        if (ABMPlayerController* Controller = Cast<ABMPlayerController>(Iterator->Get()))
        {
            Controller->ShowResultsScreen();
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Black Meridian mission: ending presentation complete results_shown=yes"));
}

FVector ABMMissionDirector::GetObjectiveLocation(const FName ObjectiveId) const
{
    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);
    if (ObjectiveId == TEXT("Insertion"))
    {
        return Level.InsertionLocation;
    }
    if (ObjectiveId == TEXT("QuietApproach") || ObjectiveId == TEXT("Checkpoint"))
    {
        return Level.CheckpointLocation;
    }
    if (ObjectiveId == TEXT("InteriorBreach"))
    {
        return Level.BreachLocation;
    }
    if (ObjectiveId == TEXT("IntelConsole"))
    {
        return Level.IntelLocation;
    }
    if (ObjectiveId == TEXT("OpenCombat"))
    {
        return Level.OpenCombatLocation;
    }
    if (ObjectiveId == TEXT("Extraction") || ObjectiveId == TEXT("FinalDefense"))
    {
        return Level.ExtractionLocation;
    }
    return FVector::ZeroVector;
}

bool ABMMissionDirector::IsAnyPlayerNearObjective(const float Radius) const
{
    const ABMGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
    if (!GameState || !GameState->CurrentObjective.bHasWorldLocation)
    {
        return false;
    }

    const float RadiusSquared = FMath::Square(FMath::Max(0.0f, Radius));
    for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
    {
        const APlayerController* Controller = Iterator->Get();
        const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
        if (Pawn && FVector::DistSquared2D(Pawn->GetActorLocation(), GameState->CurrentObjective.WorldLocation) <= RadiusSquared)
        {
            return true;
        }
    }
    return false;
}

void ABMMissionDirector::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdatePlayableSequence(DeltaSeconds);
    if (!HasAuthority() || !bMissionStarted || Phase == EBMMissionPhase::Complete || Phase == EBMMissionPhase::Failed)
    {
        return;
    }

    PhaseElapsed += DeltaSeconds;
    MissionElapsed += DeltaSeconds;
    if (ABMGameState* GameState = GetWorld()->GetGameState<ABMGameState>())
    {
        GameState->MissionElapsedSeconds = MissionElapsed;
    }
    UpdatePhaseScript(DeltaSeconds);
}

void ABMMissionDirector::UpdatePhaseScript(const float DeltaSeconds)
{
    ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>();
    ABMGreyboxDirector* Greybox = GameMode ? GameMode->GetGreyboxDirector() : nullptr;
    const FBMCampaignLevelDefinition Level = GetCampaignLevelDefinition(CampaignLevel);

    switch (Phase)
    {
    case EBMMissionPhase::Insertion:
        if (IsAnyPlayerNearObjective(280.0f) || (bDeterministicProbe && PhaseElapsed > 45.0f))
        {
            SetPhase(EBMMissionPhase::QuietApproach, TEXT("QuietApproach"), NSLOCTEXT("BlackMeridian", "QuietObjective", "REACH THE AMBER CHECKPOINT"));
        }
        break;
    case EBMMissionPhase::QuietApproach:
        // Give the route card and the player a short readable hand-off before
        // contact starts. Without this, a fast player can cross both beacons
        // in one continuous sprint and receive a combat wave before seeing
        // which area they are meant to hold.
        if ((PhaseElapsed > 2.0f && IsAnyPlayerNearObjective(320.0f)) || (bDeterministicProbe && PhaseElapsed > 65.0f))
        {
            SetPhase(EBMMissionPhase::Checkpoint, TEXT("Checkpoint"), NSLOCTEXT("BlackMeridian", "CheckpointObjective", "CLEAR THE AMBER CHECKPOINT"));
            if (Greybox)
            {
                Greybox->SpawnCombatWave(Level.CheckpointWaveSize, Level.CheckpointLocation);
            }
        }
        break;
    case EBMMissionPhase::Checkpoint:
        if (EliminationsInPhase >= Level.CheckpointWaveSize ||
            (EliminationsInPhase > 0 &&
             GetWorld()->GetGameState<ABMGameState>() &&
             GetWorld()->GetGameState<ABMGameState>()->HostileCount <= 0) ||
            (bDeterministicProbe && PhaseElapsed > 95.0f))
        {
            SetPhase(EBMMissionPhase::InteriorBreach, TEXT("InteriorBreach"), NSLOCTEXT("BlackMeridian", "BreachObjective", "REACH THE ORANGE RELAY HOUSE"));
        }
        break;
    case EBMMissionPhase::InteriorBreach:
        if (bDeterministicProbe && PhaseElapsed > 55.0f)
        {
            SetPhase(EBMMissionPhase::Intelligence, TEXT("IntelConsole"), NSLOCTEXT("BlackMeridian", "IntelObjective", "HOLD E AT THE TEAL CONSOLE (OPTIONAL)"), true);
        }
        break;
    case EBMMissionPhase::Intelligence:
#if !UE_BUILD_SHIPPING
        // The normal mission deliberately lets a player skip the optional
        // archive by walking to the seawall. The full campaign verifier has a
        // stricter contract: it must prove the console's real visibility trace
        // and hold before opening combat. Keep that guard development-only so
        // the shipped optional objective remains unchanged.
        if (FParse::Param(FCommandLine::Get(), TEXT("BM_FullCampaignProbe"))
            && !bOptionalIntelSecured)
        {
            break;
        }
#endif
        if (IsAnyPlayerNearObjective(360.0f) || (bDeterministicProbe && PhaseElapsed > 55.0f))
        {
            SetPhase(EBMMissionPhase::OpenCombat, TEXT("OpenCombat"), NSLOCTEXT("BlackMeridian", "CombatObjective", "CLEAR THE SEAWALL"));
            if (Greybox)
            {
                Greybox->SpawnCombatWave(Level.OpenCombatWaveSize, Level.OpenCombatLocation);
                Greybox->TriggerEnvironmentalEvent();
            }
            ShowSubtitleToSquad(NSLOCTEXT("BlackMeridian", "EscalationLine", "Vanta command: Light the seawall. Nothing leaves the district."), 3.0f);
        }
        break;
    case EBMMissionPhase::OpenCombat:
        if (EliminationsInPhase >= Level.OpenCombatWaveSize || (bDeterministicProbe && PhaseElapsed > 110.0f))
        {
            SetPhase(EBMMissionPhase::Extraction, TEXT("Extraction"), NSLOCTEXT("BlackMeridian", "ExtractionObjective", "REACH THE YELLOW EXTRACTION BOAT"));
        }
        break;
    case EBMMissionPhase::Extraction:
        if (bDeterministicProbe && PhaseElapsed > 70.0f)
        {
            SetPhase(EBMMissionPhase::FinalDefense, TEXT("FinalDefense"), NSLOCTEXT("BlackMeridian", "FinalDefenseObjective", "HOLD THE PIER UNTIL THE BOAT ARRIVES"));
            if (Greybox)
            {
                Greybox->SpawnCombatWave(Level.FinalDefenseWaveSize, Level.ExtractionLocation);
            }
        }
        break;
    case EBMMissionPhase::FinalDefense:
        if (EliminationsInPhase >= Level.FinalDefenseWaveSize || (bDeterministicProbe && PhaseElapsed > 120.0f))
        {
            SetPhase(EBMMissionPhase::Complete, TEXT("Complete"), NSLOCTEXT("BlackMeridian", "MissionComplete", "NIGHTGLASS COMPLETE — THE SIGNAL IS STILL LIVE"));
        }
        break;
    default:
        break;
    }
}

void ABMMissionDirector::ShowSubtitleToSquad(const FText& Text, const float Duration) const
{
    for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
    {
        if (ABMPlayerController* Controller = Cast<ABMPlayerController>(Iterator->Get()))
        {
            Controller->ShowSubtitle(Text, Duration);
        }
    }
}

void ABMMissionDirector::ShowMissionCardToSquad(const FText& Title, const FText& Body, const float Duration) const
{
    for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
    {
        if (ABMPlayerController* Controller = Cast<ABMPlayerController>(Iterator->Get()))
        {
            Controller->ShowMissionCard(Title, Body, Duration);
        }
    }
}

void ABMMissionDirector::OnRep_PlayableSequence()
{
    ActivePlayableCueElapsed = 0.0f;
    // Camera cue data is read by the owning player each frame. The replicated
    // sequence marker intentionally carries no authority-side presentation.
}

void ABMMissionDirector::OnRep_PlayableCueIndex()
{
    ActivePlayableCueElapsed = 0.0f;
}

void ABMMissionDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABMMissionDirector, ActivePlayableSequence);
    DOREPLIFETIME(ABMMissionDirector, ActivePlayableCueIndex);
}
