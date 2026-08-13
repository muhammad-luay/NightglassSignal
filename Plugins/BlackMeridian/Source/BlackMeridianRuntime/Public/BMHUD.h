// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BMHUD.generated.h"

class ABMGameState;
class ABMEnemyCharacter;
class ABMPlayerCharacter;
class UBMLobbyWidget;
class UBMNativeUIWidget;

UENUM(BlueprintType)
enum class EBMHUDScreen : uint8
{
    Splash,
    MainMenu,
    CampaignSelection,
    MultiplayerMenu,
    Lobby,
    Settings,
    Controls,
    HowToPlay,
    Pause,
    Loading,
    InGame,
    Scoreboard,
    Death,
    Results
};

UCLASS()
class BLACKMERIDIANRUNTIME_API ABMHUD : public AHUD
{
    GENERATED_BODY()

public:
    ABMHUD();

    virtual void DrawHUD() override;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void SetScreen(EBMHUDScreen NewScreen);

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI")
    EBMHUDScreen GetScreen() const { return Screen; }

    /** Runtime-created front-end surface used for controller menu focus. */
    UBMNativeUIWidget* GetNativeUIWidget() const { return NativeUIWidget; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI|Transient")
    bool HasActiveMissionCard() const { return MissionCardTimeRemaining > 0.0f && !MissionCardTitle.IsEmpty(); }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI|Transient")
    bool HasActiveSubtitle() const { return SubtitleTimeRemaining > 0.0f && !Subtitle.IsEmpty(); }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI|Transient")
    bool HasInteractionPrompt() const { return !InteractionPrompt.IsEmpty(); }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI|Transient")
    FString GetInteractionPromptText() const { return InteractionPrompt; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI|Transient")
    FText GetSubtitleText() const { return Subtitle; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI|Transient")
    FText GetMissionCardTitleText() const { return MissionCardTitle; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI|Transient")
    FText GetMissionCardBodyText() const { return MissionCardBody; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI|Transient")
    float GetDamageFlashTimeRemaining() const { return DamageFlashTimeRemaining; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI|Transient")
    float GetDamageFlashIntensity() const { return DamageFlashIntensity; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI|Transient")
    float GetHitMarkerTimeRemaining() const { return HitMarkerTimeRemaining; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI|Transient")
    bool GetHitMarkerHeadshot() const { return bHitMarkerHeadshot; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI|Transient")
    float GetEliminationTimeRemaining() const { return EliminationTimeRemaining; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI|Transient")
    bool GetEliminationHeadshot() const { return bEliminationHeadshot; }

    UFUNCTION(BlueprintPure, Category = "Black Meridian|UI|Transient")
    int32 GetEliminationKillCount() const { return EliminationKillCount; }

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void ShowSubtitle(const FText& Text, float Duration);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void ShowMissionCard(const FText& Title, const FText& Body, float Duration);

    /** Removes transient briefing copy as soon as the player needs an unobstructed combat view. */
    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void DismissMissionCard();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void ShowInteractionPrompt(const FText& Prompt);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void ShowHitMarker(bool bHeadshot);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void ShowDryFire();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void ShowElimination(bool bHeadshot, int32 KillCount);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void SetDamageDirection(float RelativeYaw);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void ShowDamageFlash(float Intensity = 1.0f);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|UI")
    void SetScoreboardVisible(bool bVisible);

    /** Hides the gameplay canvas for development-only world presentation captures. */
    void SetPresentationCapture(bool bEnabled) { bPresentationCapture = bEnabled; }

protected:
    void DrawCrosshair();
    void DrawCompass(const ABMPlayerCharacter* Player);
    void DrawCombatHUD();
    void DrawDamageFeedback();
    void DrawThrowableThreat(const ABMPlayerCharacter* Player);
    void DrawMissionFlow(const ABMGameState* State);
    void DrawObjectiveMarker(const ABMPlayerCharacter* Player, const ABMGameState* State);
    void DrawHostileMarkers(const ABMPlayerCharacter* Player, const ABMGameState* State);
    void DrawSignalLobby(const ABMGameState* State);
    void DrawMenuOverlay(const FString& Title, const FString& Body);
    void DrawTextLine(const FString& Text, float X, float Y, float Scale, const FLinearColor& Color) const;
    FLinearColor ApplyAccessibilityColor(const FLinearColor& Color) const;
    float GetSubtitleScale() const;
    FString GetMissionPhaseLabel() const;

private:
    EBMHUDScreen Screen = EBMHUDScreen::MainMenu;
    FString InteractionPrompt;
    FText Subtitle;
    FText MissionCardTitle;
    FText MissionCardBody;
    float SubtitleTimeRemaining = 0.0f;
    float MissionCardTimeRemaining = 0.0f;
    float HitMarkerTimeRemaining = 0.0f;
    float DryFireTimeRemaining = 0.0f;
    float EliminationTimeRemaining = 0.0f;
    int32 EliminationKillCount = 0;
    float DamageDirectionTimeRemaining = 0.0f;
    float DamageDirectionYaw = 0.0f;
    float DamageFlashTimeRemaining = 0.0f;
    float DamageFlashIntensity = 0.0f;
    bool bHitMarkerHeadshot = false;
    bool bEliminationHeadshot = false;
    bool bScoreboardVisible = false;
    bool bPresentationCapture = false;

    UPROPERTY(Transient)
    TObjectPtr<UBMLobbyWidget> LobbyWidget;

    UPROPERTY(Transient)
    TObjectPtr<UBMNativeUIWidget> NativeUIWidget;

#if !UE_BUILD_SHIPPING
    float DiagnosticsLogTimeRemaining = 0.0f;
#endif
};
