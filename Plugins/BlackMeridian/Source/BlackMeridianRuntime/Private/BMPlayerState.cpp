// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMPlayerState.h"

#include "Net/UnrealNetwork.h"

ABMPlayerState::ABMPlayerState()
{
    bReplicates = true;
}

void ABMPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABMPlayerState, Team);
    DOREPLIFETIME(ABMPlayerState, Kills);
    DOREPLIFETIME(ABMPlayerState, Deaths);
    DOREPLIFETIME(ABMPlayerState, ObjectiveScore);
    DOREPLIFETIME(ABMPlayerState, SmoothedLatencyMs);
}
