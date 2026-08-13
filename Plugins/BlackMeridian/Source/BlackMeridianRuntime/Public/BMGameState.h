// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BMCoreTypes.h"
#include "BMGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBMGameStateUpdated);

UCLASS()
class BLACKMERIDIANRUNTIME_API ABMGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    ABMGameState();

    /**
     * Returns whether combat ordnance is allowed for the current match state.
     * Campaign maps remain live without using the Signal Control match flag;
     * Signal Control requires an active, unfinished round.  Keeping this
     * policy here prevents player and AI throwables from drifting apart.
     */
    static bool IsCombatOrdnanceEnabled(const ABMGameState* State);

    UPROPERTY(ReplicatedUsing = OnRep_MissionPhase, BlueprintReadOnly, Category = "Mission")
    EBMMissionPhase MissionPhase = EBMMissionPhase::Intro;

    UPROPERTY(ReplicatedUsing = OnRep_Objective, BlueprintReadOnly, Category = "Mission")
    FBMObjectiveState CurrentObjective;

    UPROPERTY(ReplicatedUsing = OnRep_CampaignLevel, BlueprintReadOnly, Category = "Campaign")
    EBMCampaignLevel CampaignLevel = EBMCampaignLevel::SableQuay;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission")
    float MissionElapsedSeconds = 0.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission")
    int32 CheckpointIndex = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission")
    int32 HostileCount = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission")
    int32 TeamScore = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Signal Control")
    FBMSignalControlSnapshot SignalControl;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Signal Control|Lobby")
    TArray<FBMSignalLobbyMember> SignalLobbyMembers;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Signal Control|Lobby")
    int32 SignalLobbyHumanCount = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Signal Control|Lobby")
    int32 SignalLobbyBotCount = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Signal Control|Lobby")
    int32 SignalLobbyCapacity = 12;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Combat")
    TArray<FString> RecentKillFeed;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
    float MatchCountdown = 5.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
    bool bMatchLive = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
    bool bSignalControlMode = false;

    UPROPERTY(BlueprintAssignable, Category = "Mission")
    FBMGameStateUpdated OnGameStateUpdated;

    void SetMissionPhase(EBMMissionPhase NewPhase);
    void SetObjective(const FBMObjectiveState& NewObjective);

protected:
    UFUNCTION()
    void OnRep_MissionPhase();

    UFUNCTION()
    void OnRep_Objective();

    UFUNCTION()
    void OnRep_CampaignLevel();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
