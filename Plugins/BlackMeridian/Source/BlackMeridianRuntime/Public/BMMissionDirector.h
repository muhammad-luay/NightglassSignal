// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BMCoreTypes.h"
#include "BMMissionDirector.generated.h"

class ABMEnemyCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBMMissionPhaseChanged, EBMMissionPhase, NewPhase);

UCLASS()
class BLACKMERIDIANRUNTIME_API ABMMissionDirector : public AActor
{
    GENERATED_BODY()

public:
    ABMMissionDirector();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Mission")
    void StartMission();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Campaign")
    void SetCampaignLevel(EBMCampaignLevel Level) { CampaignLevel = Level; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Campaign")
    EBMCampaignLevel GetCampaignLevel() const { return CampaignLevel; }

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Mission")
    void RestartFromCheckpoint();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Mission")
    void SaveCheckpoint();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Mission")
    bool LoadCheckpoint();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Mission")
    void NotifyInteraction(AActor* Interactor, FName InteractionId);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Mission")
    void NotifyEnemyEliminated(ABMEnemyCharacter* EnemyCharacter);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Mission")
    void NotifyPlayerEliminated();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Mission|Cinematic")
    void StartPlayableSequence(FName SequenceId);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Mission|Cinematic")
    void StopPlayableSequence();

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Mission|Cinematic")
    bool IsPlayableSequenceActive() const { return !ActivePlayableSequence.IsNone() && ActivePlayableCueIndex != INDEX_NONE; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Mission|Cinematic")
    bool IsPlayableSequenceLookLocked() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Mission|Cinematic")
    FBMPlayableCameraCue GetActivePlayableCameraCue() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Mission|Cinematic")
    float GetActivePlayableCameraCueProgress() const;

    UPROPERTY(BlueprintAssignable, Category = "Black Meridian|Mission")
    FBMMissionPhaseChanged OnMissionPhaseChanged;

    UPROPERTY(BlueprintReadOnly, Category = "Black Meridian|Mission")
    bool bOptionalIntelSecured = false;

    UPROPERTY(BlueprintReadOnly, Category = "Black Meridian|Mission")
    bool bSignalJammerPlanted = false;

    UPROPERTY(BlueprintReadOnly, Category = "Black Meridian|Campaign")
    EBMCampaignLevel CampaignLevel = EBMCampaignLevel::SableQuay;

    /** Blueprint-replaceable playable cues; authored Level Sequences can take over later. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Cinematics")
    TArray<FBMPlayableCameraCue> IntroCameraCues;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Cinematics")
    TArray<FBMPlayableCameraCue> SetPieceCameraCues;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Cinematics")
    TArray<FBMPlayableCameraCue> ExtractionCameraCues;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Cinematics")
    TArray<FBMPlayableCameraCue> EndingCameraCues;

protected:
    void SetPhase(EBMMissionPhase NewPhase, FName ObjectiveId, const FText& Title, bool bOptional = false);
    void UpdatePhaseScript(float DeltaSeconds);
    FVector GetObjectiveLocation(FName ObjectiveId) const;
    bool IsAnyPlayerNearObjective(float Radius) const;
    void UpdatePlayableSequence(float DeltaSeconds);
    const TArray<FBMPlayableCameraCue>* FindPlayableSequence(FName SequenceId) const;
    void NotifyPlayableCueStarted();
    void ShowSubtitleToSquad(const FText& Text, float Duration) const;
    void ShowMissionCardToSquad(const FText& Title, const FText& Body, float Duration) const;
    void ShowResultsAfterEnding();

private:
    EBMMissionPhase Phase = EBMMissionPhase::Intro;
    float PhaseElapsed = 0.0f;
    float MissionElapsed = 0.0f;
    int32 EliminationsInPhase = 0;
    bool bMissionStarted = false;
    bool bDeterministicProbe = false;
    EBMMissionPhase LoadedCheckpointPhase = EBMMissionPhase::Insertion;

#if !UE_BUILD_SHIPPING
    bool bInteractionProbeRelayCompleted = false;
    bool bInteractionProbeConsoleCompleted = false;
    bool bInteractionProbeExtractionCompleted = false;
#endif

    UPROPERTY(ReplicatedUsing = OnRep_PlayableSequence, BlueprintReadOnly, Category = "Presentation|Cinematics", meta = (AllowPrivateAccess = "true"))
    FName ActivePlayableSequence = NAME_None;

    UPROPERTY(ReplicatedUsing = OnRep_PlayableCueIndex, BlueprintReadOnly, Category = "Presentation|Cinematics", meta = (AllowPrivateAccess = "true"))
    int32 ActivePlayableCueIndex = INDEX_NONE;

    /** Server-local timing; clients advance their additive cue from the replicated index. */
    float ActivePlayableCueElapsed = 0.0f;

    /** Keeps the ending camera/subtitles visible before the results screen takes over. */
    FTimerHandle EndingResultsTimer;

    UFUNCTION()
    void OnRep_PlayableSequence();

    UFUNCTION()
    void OnRep_PlayableCueIndex();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
