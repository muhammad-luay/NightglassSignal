// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "BMCoreTypes.h"
#include "BMHUD.h"
#include "Containers/Ticker.h"
#include "BMPlayerController.generated.h"

class UBMAudioDataAsset;
class UAudioComponent;
class USoundWaveProcedural;
class FSocket;

UCLASS()
class BLACKMERIDIANRUNTIME_API ABMPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ABMPlayerController();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void PlayerTick(float DeltaTime) override;
    virtual void SetupInputComponent() override;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void ShowSubtitle(const FText& Text, float Duration);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void ShowMissionCard(const FText& Title, const FText& Body, float Duration);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void ShowDeathScreen();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void ShowRespawnedPlayer();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void ShowResultsScreen();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void SetDamageDirection(float RelativeYaw);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void ShowDamageFeedback(float Intensity = 1.0f);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void SetInteractionPrompt(const FText& Prompt);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void ShowSignalLobby();

    /** Starts the lobby match through the same validated path as the Enter key. */
    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void RequestSignalMatchStartFromUI();

    /** Native UMG action wrappers; gameplay ownership remains in this controller. */
    void UIStartMission();
    void UIStartNextCampaignLevel();
    void UIOpenCampaign();
    void UIOpenMultiplayer();
    void UIOpenSettings();
    void UIOpenControls();
    void UIOpenHowToPlay();
    void UIBack();
    void UIRestartCheckpoint();
    void UIReturnToMenu();
    void UITogglePause();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void ShowMissionMenu();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void ShowLoadingScreen();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Audio")
    void PlayMusicState(FName StateId);

    /** Re-applies the local music state after replicated mission phase arrives. */
    void SyncMissionMusicToGameState();

    /** Optional authored ambient/music/dialogue routing for the source-only HUD. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Audio")
    TSoftObjectPtr<UBMAudioDataAsset> AudioProfile;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI|Settings")
    int32 GetSettingsRow() const { return SettingsRow; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI|Controls")
    int32 GetControlsRow() const { return ControlsRow; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI|Controls")
    FString GetControlBindingLabel(int32 Row) const;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Campaign")
    EBMCampaignLevel GetSelectedCampaignLevel() const { return SelectedCampaignLevel; }

    /** Development-only local test helpers; shipping builds compile them as no-ops. */
    UFUNCTION(Exec)
    void BM_CheatGiveAmmo();

    UFUNCTION(Exec)
    void BM_CheatRestore();

    void NotifyHitMarker(bool bHeadshot);
    void NotifyDryFire();
    void NotifyElimination(bool bHeadshot, int32 KillCount = -1);

    /** Reassert game-only mouse/look/movement input after a menu or focus handoff. */
    void RestoreInteractiveInput();

#if !UE_BUILD_SHIPPING
    /** Server entry point for the development-only network combat probe. */
    void StartCombatProbeFire(const FVector& Direction, const FVector& ProbeLocation, const FRotator& ProbeRotation);
    void StartCombatProbeReload();
    void StartCombatProbeReloadInterrupt();
    void ExecuteCombatProbeFire(FVector_NetQuantizeNormal Direction);
    void RetryCombatProbeFire();
    void ExecuteCombatProbeReload(bool bInterrupt);
#endif

protected:
    void StartMissionInput();
    void TogglePauseInput();
    void OpenSettingsInput();
    void OpenControlsInput();
    void OpenHowToPlayInput();
    void OpenCampaignInput();
    void OpenMultiplayerInput();
    void SettingsPreviousInput();
    void SettingsNextInput();
    void AdjustSettingsLeftInput();
    void AdjustSettingsRightInput();
    void RestartCheckpointInput();
    void ToggleScoreboardInput();
    void BackInput();

    void FinishSplash();
    void InitializeLocalPresentation();
    void FocusMissionStart();
    void ActivateInteractiveGameWindow();
    void HandleInitialApplicationReactivation();
    void CycleSettingsRow(int32 Direction);
    void CycleControlsRow(int32 Direction);
    void CycleCampaignLevel(int32 Direction);
    void AdjustSettings(int32 Direction);
    void AdjustControlBinding(int32 Direction);
    void ApplyControlBinding(FName ActionName, const FKey& NewKey);
    void ApplyLocalSettings(bool bApplyResolution = true);
    void StartAmbientAudio();
    float GetMusicVolumeMultiplier() const;
    void PlayDialogueCue();
    void RequestSignalControlTravel();
    void RequestMissionTravel();
    void RequestMissionTravelToLevel(EBMCampaignLevel RequestedLevel);
    bool TryGetNextCampaignLevel(EBMCampaignLevel& OutLevel) const;
    bool IsSignalControlWorld() const;

#if !UE_BUILD_SHIPPING
    /** Starts the opt-in loopback bridge used for remote Mac playtest input. */
    void InitializeRemoteInputBridge();
    void ShutdownRemoteInputBridge();
    void DisconnectRemoteInputClient();
    void PollRemoteInputBridge();
    bool TickRemoteInputBridge(float DeltaTime);
    void ProcessRemoteInputCommand(const FString& CommandLine);
    void SendRemoteInputResponse(const FString& Response);
    void ReleaseRemoteInputKeys();

    /** Records a bounded rendered frame-time sample for a repeatable local probe. */
    void TickPerformanceProbe(float DeltaTime);

    /** Emits a bounded music-component heartbeat for the audio-loop probe. */
    void TickAudioProbe(float DeltaTime);
#endif

    bool bHandledInitialApplicationReactivation = false;

    UFUNCTION(Server, Reliable)
    void ServerRequestMissionStart(EBMCampaignLevel RequestedLevel);

    UFUNCTION(Server, Reliable)
    void ServerRequestCheckpointRestart();

    UFUNCTION(Server, Reliable)
    void ServerRequestMatchStart();

    UFUNCTION(Server, Reliable)
    void ServerRequestSignalControlTravel();

    UFUNCTION(Server, Reliable)
    void ServerRequestMissionTravel();

    UFUNCTION(Server, Reliable)
    void ServerRequestNextCampaignLevel();

    UFUNCTION(Client, Reliable)
    void ClientShowSubtitle(const FText& Text, float Duration);

    UFUNCTION(Client, Reliable)
    void ClientShowMissionCard(const FText& Title, const FText& Body, float Duration);

    UFUNCTION(Client, Reliable)
    void ClientShowDeathScreen();

    UFUNCTION(Client, Reliable)
    void ClientShowRespawnedPlayer();

    UFUNCTION(Client, Reliable)
    void ClientShowResultsScreen();

    UFUNCTION(Client, Reliable)
    void ClientPlayMusicState(FName StateId);

    UFUNCTION(Client, Reliable)
    void ClientSetDamageDirection(float RelativeYaw);

    UFUNCTION(Client, Reliable)
    void ClientShowSignalLobby();

    UFUNCTION(Client, Reliable)
    void ClientShowMissionMenu();

    UFUNCTION(Client, Reliable)
    void ClientShowElimination(bool bHeadshot, int32 KillCount);

    UFUNCTION(Client, Reliable)
    void ClientRunCombatProbeFire(FVector_NetQuantizeNormal Direction, FVector_NetQuantize ProbeLocation, FRotator ProbeRotation);

    UFUNCTION(Client, Reliable)
    void ClientRunCombatProbeReload();

    UFUNCTION(Client, Reliable)
    void ClientRunCombatProbeReloadInterrupt();

    /** Development probe RPCs are declared for every target because UHT does
     * not allow UFUNCTION declarations inside build preprocessor blocks. */
    UFUNCTION(Server, Reliable)
    void ServerPrepareCombatProbeView(FRotator ProbeRotation);

    UFUNCTION(Client, Reliable)
    void ClientCombatProbeViewReady();

    ABMHUD* GetBlackMeridianHUD() const;

    int32 SettingsRow = 0;
    int32 ControlsRow = 0;
    EBMCampaignLevel SelectedCampaignLevel = EBMCampaignLevel::SableQuay;
    EBMHUDScreen SettingsReturnScreen = EBMHUDScreen::MainMenu;
    EBMHUDScreen ControlsReturnScreen = EBMHUDScreen::MainMenu;
    EBMHUDScreen HowToPlayReturnScreen = EBMHUDScreen::MainMenu;
    bool bSignalControlLobby = false;
    bool bAmbientAudioStarted = false;
    /** Persistent local battlefield bed; do not use fire-and-forget playback for looping ambience. */
    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> AmbientAudioComponent = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> MusicAudioComponent = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<USoundWaveProcedural> ProceduralMusicWave = nullptr;

    FName ActiveMusicState = NAME_None;
    FTimerHandle PresentationRetryTimer;
    FTimerHandle NativeFocusTimer;
#if !UE_BUILD_SHIPPING
    FSocket* RemoteInputListenSocket = nullptr;
    FSocket* RemoteInputClientSocket = nullptr;
    FTSTicker::FDelegateHandle RemoteInputTickerHandle;
    FString RemoteInputReceiveBuffer;
    TSet<FKey> RemoteInputPressedKeys;
    int32 RemoteInputPort = 0;

    FTimerHandle CombatProbeRetryTimer;
    FVector_NetQuantizeNormal PendingCombatProbeDirection = FVector::ForwardVector;
    FVector_NetQuantize PendingCombatProbeLocation = FVector::ZeroVector;
    FRotator PendingCombatProbeRotation = FRotator::ZeroRotator;
    int32 CombatProbeRetryCount = 0;
    bool bCombatProbeViewPending = false;
    bool bPerformanceProbeActive = false;
    bool bPerformanceProbeFinished = false;
    float PerformanceProbeElapsedSeconds = 0.0f;
    float PerformanceProbeDurationSeconds = 30.0f;
    TArray<float> PerformanceFrameTimesMilliseconds;
    float AudioProbeElapsedSeconds = 0.0f;
#endif
};
