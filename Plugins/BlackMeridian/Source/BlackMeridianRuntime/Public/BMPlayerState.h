// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BMCoreTypes.h"
#include "BMPlayerState.generated.h"

UCLASS()
class BLACKMERIDIANRUNTIME_API ABMPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    ABMPlayerState();

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Team")
    EBMTeam Team = EBMTeam::Neutral;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Score")
    int32 Kills = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Score")
    int32 Deaths = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Score")
    int32 ObjectiveScore = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Network")
    float SmoothedLatencyMs = 0.0f;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
