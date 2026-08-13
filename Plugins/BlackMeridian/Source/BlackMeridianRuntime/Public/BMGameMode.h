// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BMCoreTypes.h"
#include "BMGameMode.generated.h"

class ABMEnemyCharacter;
class ABMFriendlySquadMember;
class ABMInteractableActor;
class ABMPlayerCharacter;
class ABMGreyboxDirector;
class ABMMissionDirector;
class ABMThrowable;
class ABMWeapon;
class APlayerStart;
class UBMPlayerProfileSaveGame;

UCLASS(Blueprintable)
class BLACKMERIDIANRUNTIME_API ABMGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ABMGameMode();

    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void RestartPlayer(AController* NewPlayer) override;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Mission")
    void RequestMissionStart(EBMCampaignLevel Level = EBMCampaignLevel::SableQuay);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Mission")
    void HandlePlayerEliminated(ABMPlayerCharacter* PlayerCharacter, const FBMDamageContext& DamageContext);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Mission")
    void HandleEnemyEliminated(ABMEnemyCharacter* EnemyCharacter, const FBMDamageContext& DamageContext);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Interaction")
    void HandleInteraction(AActor* Interactor, FName InteractionId);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Multiplayer")
    void RespawnPlayer(AController* Controller);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Profile")
    void RecordCampaignCompletion();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Profile")
    void RecordMultiplayerMatch();

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Profile")
    TArray<EBMWeaponId> GetProfileLoadout() const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Campaign")
    EBMCampaignLevel GetCampaignLevel() const { return CampaignLevel; }

    virtual void RegisterCombatLocation(const FVector& Location);

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Mission")
    ABMMissionDirector* GetMissionDirector() const { return MissionDirector; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Greybox")
    ABMGreyboxDirector* GetGreyboxDirector() const { return GreyboxDirector; }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
    bool bMissionMode = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multiplayer")
    bool bRespawnPlayers = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multiplayer")
    float RespawnDelay = 5.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Campaign")
    EBMCampaignLevel CampaignLevel = EBMCampaignLevel::SableQuay;

    /** Set this to a Blueprint child when authored enemy presentation exists. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Combat")
    TSubclassOf<ABMEnemyCharacter> EnemyCharacterClass;

protected:
    UPROPERTY()
    TObjectPtr<ABMGreyboxDirector> GreyboxDirector;

    UPROPERTY()
    TObjectPtr<ABMMissionDirector> MissionDirector;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
    int32 ScoreLimit = 1000;

    UPROPERTY()
    TObjectPtr<UBMPlayerProfileSaveGame> PlayerProfile;

private:
    void LoadPlayerProfile();
    void SavePlayerProfile() const;
    void TrySpawnCombatShowcase();
#if !UE_BUILD_SHIPPING
    void RunBoundaryGuardProbe(float DeltaSeconds);
    void RunCombatFeelProbe(float DeltaSeconds);
    void RunAimValidationProbe(float DeltaSeconds);
    void RunCombatShowcaseAutoFire();
    void RequestCombatShowcaseReloadScreenshot(const TCHAR* ArgumentName, const TCHAR* DefaultFilename, const TCHAR* Stage);
    void RunMissionInteractionProbe();
    bool StartMissionInteractionProbeHold(FName InteractionId, const FVector& ApproachLocation);
    void RunInteractionAcceptanceProbe();
    void RunDestructionPresentationShowcase(float DeltaSeconds);
    void RunPickupProbe();
    void RunPersistenceProbe();
    bool SpawnThrowableProbeGrenade(EBMWeaponId ThrowableId);
    void RunThrowableProbe();
    void RunThrowablePresentationShowcase();
    void RunThrowableInputProbe();
    void RunReloadInputProbe();
    void RunFullCampaignProbe(float DeltaSeconds);
#endif

    TWeakObjectPtr<APlayerStart> RuntimePlayerStart;
    FTimerHandle CombatShowcaseTimerHandle;
#if !UE_BUILD_SHIPPING
    float BoundaryGuardProbeElapsedSeconds = 0.0f;
    bool bBoundaryGuardProbeFinished = false;
    float CombatFeelProbeStageStartedAt = 0.0f;
    float CombatFeelProbeFirstShotAt = 0.0f;
    float CombatFeelProbeFireInterval = 0.0f;
    float CombatFeelProbeInitialPenalty = 0.0f;
    float CombatFeelProbePenaltyAfterFirstShot = 0.0f;
    float CombatFeelProbePenaltyAfterPostIntervalShot = 0.0f;
    float CombatFeelProbePenaltyAfterRecovery = 0.0f;
    FVector CombatFeelProbeInitialWeaponLocation = FVector::ZeroVector;
    FRotator CombatFeelProbeInitialWeaponRotation = FRotator::ZeroRotator;
    TWeakObjectPtr<ABMEnemyCharacter> CombatFeelProbeTarget;
    FVector CombatFeelProbeDirection = FVector::ZeroVector;
    int32 CombatFeelProbeStage = 0;
    bool bCombatFeelProbeFinished = false;
    bool bCombatFeelProbeFirstShotAccepted = false;
    bool bCombatFeelProbeImmediateRejected = false;
    bool bCombatFeelProbeHalfIntervalRejected = false;
    bool bCombatFeelProbePostIntervalAccepted = false;
    int32 AimValidationProbeStage = 0;
    float AimValidationProbeElapsedSeconds = 0.0f;
    float AimValidationProbeHealthBefore = 0.0f;
    int32 AimValidationProbeMagazineBefore = 0;
    bool bAimValidationProbeFailed = false;
    TWeakObjectPtr<ABMEnemyCharacter> AimValidationProbeTarget;
    FVector AimValidationProbeTargetLocation = FVector::ZeroVector;
    FTimerHandle CombatShowcaseFireTimerHandle;
    int32 CombatShowcaseAutoFireShots = 0;
    bool bCombatShowcaseReloadIssued = false;
    bool bCombatShowcaseReloadMidScreenshotRequested = false;
    bool bCombatShowcaseReloadCompleteScreenshotRequested = false;
    float CombatShowcaseReloadStartedAt = -1.0f;
    float CombatShowcaseDiagnosticTimer = 0.0f;
    FTimerHandle MissionInteractionProbeTimerHandle;
    int32 MissionInteractionProbeStep = 0;
    int32 InteractionAcceptanceProbeStep = 0;
    float DestructionPresentationShowcaseElapsed = 0.0f;
    bool bDestructionPresentationShowcaseStarted = false;
    bool bDestructionPresentationShowcaseReset = false;
    bool bDestructionPresentationShowcaseScreenshotRequested = false;
    bool bDestructionPresentationShowcaseResetScreenshotRequested = false;
    int32 PickupProbeStep = 0;
    float PickupProbeElapsedSeconds = 0.0f;
    bool bInteractionAcceptanceProbeWaveSpawned = false;
    bool bInteractionAcceptanceProbeReviveSetup = false;
    TWeakObjectPtr<ABMFriendlySquadMember> InteractionAcceptanceProbeFriendly;
    TWeakObjectPtr<ABMInteractableActor> InteractionAcceptanceProbeRevive;
    FTimerHandle PersistenceProbeTimerHandle;
    bool bPersistenceProbePrepared = false;
    FTimerHandle ThrowableProbeTimerHandle;
    int32 ThrowableProbeStep = 0;
    TWeakObjectPtr<ABMPlayerCharacter> ThrowableProbeTarget;
    TWeakObjectPtr<ABMThrowable> ThrowableProbeActive;
    float ThrowableProbeDetonationStart = 0.0f;
    float ThrowableProbeExpectedFuse = 0.0f;
    float ThrowableProbeFragHealthBefore = 0.0f;
    bool bThrowablePresentationShowcaseStarted = false;
    int32 ThrowableInputProbeStep = 0;
    int32 ThrowableInputProbeFragBefore = 0;
    float ThrowableInputProbeElapsed = 0.0f;
    TWeakObjectPtr<ABMThrowable> ThrowableInputProbeActive;
    int32 ReloadInputProbeStep = 0;
    int32 ReloadInputProbeMagazineBefore = 0;
    int32 ReloadInputProbeReserveBefore = 0;
    int32 ReloadInputProbeMagazineCapacity = 0;
    float ReloadInputProbeElapsed = 0.0f;
    float ReloadInputProbeStartTime = -1.0f;
    float ReloadInputProbeExpectedDuration = 0.0f;
    bool bReloadInputProbeEmpty = false;
    bool bFullCampaignProbeStarted = false;
    bool bFullCampaignProbeFailed = false;
    int32 FullCampaignProbeStage = 0;
    int32 FullCampaignProbeTraversalNextStage = 0;
    FName FullCampaignProbeSegmentId;
    FVector FullCampaignProbeSegmentTarget = FVector::ZeroVector;
    FVector FullCampaignProbeSegmentStart = FVector::ZeroVector;
    FVector FullCampaignProbeLastLocation = FVector::ZeroVector;
    float FullCampaignProbeSegmentDistance = 0.0f;
    float FullCampaignProbeSegmentElapsed = 0.0f;
    float FullCampaignProbeLastProgressLog = 0.0f;
    bool bFullCampaignProbeTraversalActive = false;
    FName FullCampaignProbeInteractionId;
    TWeakObjectPtr<ABMInteractableActor> FullCampaignProbeInteraction;
    float FullCampaignProbeInteractionStartedAt = -1.0f;
    bool bFullCampaignProbeInteractionStarted = false;
    bool bFullCampaignProbeCombatStarted = false;
    int32 FullCampaignProbeCombatNextStage = 0;
    int32 FullCampaignProbeCombatExpected = 0;
    int32 FullCampaignProbeCombatAcceptedShots = 0;
    int32 FullCampaignProbeCombatDamageEvents = 0;
    int32 FullCampaignProbeCombatWeaponCount = 0;
    float FullCampaignProbeLastShotAt = -100.0f;
    float FullCampaignProbeReloadStartedAt = -1.0f;
    TArray<TWeakObjectPtr<ABMEnemyCharacter>> FullCampaignProbeWaveActors;
    TWeakObjectPtr<ABMEnemyCharacter> FullCampaignProbeCombatTarget;
    int32 FullCampaignProbeCombatNoDamageShots = 0;
    bool bFullCampaignProbeCombatRepositionActive = false;
    FVector FullCampaignProbeCombatRepositionDirection = FVector::ZeroVector;
    float FullCampaignProbeCombatLastProgressLog = -2.0f;
#endif
    bool bCombatShowcaseSpawned = false;
};
