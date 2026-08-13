// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMGameState.h"

#include "BMPlayerController.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ABMGameState::ABMGameState()
{
    bReplicates = true;
}

bool ABMGameState::IsCombatOrdnanceEnabled(const ABMGameState* State)
{
    // A missing state is possible during early actor initialization. Preserve
    // the existing campaign behavior and allow the throw path to proceed;
    // Signal Control is the only mode that uses the match-live gate.
    return !State
        || !State->bSignalControlMode
        || (State->bMatchLive && !State->SignalControl.bMatchFinished);
}

void ABMGameState::SetMissionPhase(const EBMMissionPhase NewPhase)
{
    MissionPhase = NewPhase;
    OnGameStateUpdated.Broadcast();
    ForceNetUpdate();
}

void ABMGameState::SetObjective(const FBMObjectiveState& NewObjective)
{
    CurrentObjective = NewObjective;
    OnGameStateUpdated.Broadcast();
    ForceNetUpdate();
}

void ABMGameState::OnRep_MissionPhase()
{
    OnGameStateUpdated.Broadcast();
    // A listen-server/client can receive ShowMissionMenu before this state
    // has replicated. Re-apply the phase-driven local music here so that the
    // late menu hand-off cannot leave the opening mission on neutral music.
    if (UWorld* World = GetWorld())
    {
        if (ABMPlayerController* Controller = Cast<ABMPlayerController>(World->GetFirstPlayerController()))
        {
            Controller->SyncMissionMusicToGameState();
        }
    }
}

void ABMGameState::OnRep_Objective()
{
    OnGameStateUpdated.Broadcast();
}

void ABMGameState::OnRep_CampaignLevel()
{
    // The greybox director consumes this shared event to rebuild local
    // terrain/weather/lighting presentation on clients after a menu-selected
    // campaign level arrives over the network.
    OnGameStateUpdated.Broadcast();
}

void ABMGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABMGameState, MissionPhase);
    DOREPLIFETIME(ABMGameState, CurrentObjective);
    DOREPLIFETIME(ABMGameState, CampaignLevel);
    DOREPLIFETIME(ABMGameState, MissionElapsedSeconds);
    DOREPLIFETIME(ABMGameState, CheckpointIndex);
    DOREPLIFETIME(ABMGameState, HostileCount);
    DOREPLIFETIME(ABMGameState, TeamScore);
    DOREPLIFETIME(ABMGameState, SignalControl);
    DOREPLIFETIME(ABMGameState, SignalLobbyMembers);
    DOREPLIFETIME(ABMGameState, SignalLobbyHumanCount);
    DOREPLIFETIME(ABMGameState, SignalLobbyBotCount);
    DOREPLIFETIME(ABMGameState, SignalLobbyCapacity);
    DOREPLIFETIME(ABMGameState, RecentKillFeed);
    DOREPLIFETIME(ABMGameState, MatchCountdown);
    DOREPLIFETIME(ABMGameState, bMatchLive);
    DOREPLIFETIME(ABMGameState, bSignalControlMode);
}
