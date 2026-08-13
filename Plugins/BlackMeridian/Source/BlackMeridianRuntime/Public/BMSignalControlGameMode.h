// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BMGameMode.h"
#include "BMSignalControlGameMode.generated.h"

class ABMControlNode;
class APlayerStart;
class AController;
class APlayerController;
class AActor;
class ABMPlayerCharacter;

struct FBMSignalCombatLocation
{
    FVector Location = FVector::ZeroVector;
    float RecordedAt = 0.0f;
};

UCLASS()
class BLACKMERIDIANRUNTIME_API ABMSignalControlGameMode : public ABMGameMode
{
    GENERATED_BODY()

public:
    ABMSignalControlGameMode();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void RestartPlayer(AController* NewPlayer) override;
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
    virtual void Logout(AController* Exiting) override;
    virtual void RegisterCombatLocation(const FVector& Location) override;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Signal Control")
    void RequestMatchStart();

#if !UE_BUILD_SHIPPING
    /** Records only damage dealt by the staged probe shooter to its staged target. */
    void RegisterCombatProbeDamage(AActor* Shooter, AActor* Target, float HealthBefore, float HealthAfter);

    /** Moves the fallback probe target onto the submitted ray at shot time. */
    void PrepareCombatFallbackCurrentPose(AActor* Shooter);
#endif

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Control")
    int32 SignalScoreLimit = 200;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Control")
    float NodeRotationSeconds = 30.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Control|Bots")
    bool bFillWithBots = true;

    /** Maximum bot fill; human players consume the remaining 12 match slots. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Control|Bots", meta = (ClampMin = "0", ClampMax = "11"))
    int32 BotCount = 11;

private:
    void RotateActiveNode();
    void FinishMatch(EBMTeam WinningTeam);
    void PruneRecentCombatLocations();
    void MaintainBotFill();
    void EnsureBotWeapons();
    void EnsureLivePlayerPawns();
    void RefreshSignalLobbySnapshot();
    APlayerStart* ChooseSafestStart(EBMTeam Team);
    float ScoreStartLocation(const APlayerStart* Start, EBMTeam Team) const;
    void SpawnReplacementBot(EBMTeam Team, const FVector& RequestedLocation = FVector::ZeroVector);

#if !UE_BUILD_SHIPPING
    void TickCombatProbe(float DeltaSeconds);
    void StageCombatProbePlayers(ABMPlayerCharacter* Shooter, ABMPlayerCharacter* Target);
    void DispatchCombatProbeFire();
    void DispatchCombatProbeReload(bool bSecondAttempt);
    void DispatchCombatProbeReloadInterrupt();
#endif

    UPROPERTY()
    TArray<TObjectPtr<ABMControlNode>> Nodes;

    UPROPERTY()
    TArray<TObjectPtr<APlayerStart>> SignalStarts;

    int32 NextStartIndex = 0;

    int32 ActiveNodeIndex = 0;
    float RotationTimer = 30.0f;
    float ScoreTimer = 0.0f;
    float BotWeaponAuditTimer = 0.0f;
    float CountdownTimer = 5.0f;
    bool bMatchStarted = false;
    bool bSignalControlProbe = false;
    bool bProbeScoreReported = false;
    TArray<FBMSignalCombatLocation> RecentCombatLocations;
    TMap<TWeakObjectPtr<APlayerController>, float> PawnRecoveryCooldowns;

#if !UE_BUILD_SHIPPING
    bool bCombatProbe = false;
    /** Forces a live-pose-only target transition so rewind fallback is exercised. */
    bool bCombatFallbackProbe = false;
    bool bCombatFallbackPoseActivated = false;
    bool bReloadProbe = false;
    bool bReloadProbeEmpty = false;
    bool bReloadProbeNoInterrupt = false;
    bool bCombatProbeConfigured = false;
    bool bCombatProbeFireDispatched = false;
    bool bCombatProbeDamageReported = false;
    bool bDeathPresentationProbe = false;
    bool bCombatProbeDeathReported = false;
    bool bCombatProbeReloadDispatched = false;
    bool bCombatProbeReloadPrepared = false;
    bool bCombatProbeReloadStarted = false;
    bool bCombatProbeReloadInterruptDispatched = false;
    bool bCombatProbeReloadCancelled = false;
    bool bCombatProbeReloadSecondDispatched = false;
    bool bCombatProbeReloadSecondStarted = false;
    bool bCombatProbeReloadCompleted = false;
    bool bCombatProbeReloadFailed = false;
    float CombatProbeFireDelay = 0.0f;
    float CombatProbeReloadDelay = 0.0f;
    float CombatProbeReloadStartTime = -1.0f;
    float CombatProbeReloadExpectedDuration = 0.0f;
    int32 CombatProbeInitialAmmo = 0;
    int32 CombatProbeReloadPartialAmmo = 0;
    int32 CombatProbeReloadPartialReserve = 0;
    TWeakObjectPtr<ABMPlayerCharacter> CombatProbeShooter;
    TWeakObjectPtr<ABMPlayerCharacter> CombatProbeTarget;
#endif
};
