// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMHUD.h"

#include "BMLobbyWidget.h"
#include "BMNativeUIWidget.h"
#include "BMGameState.h"
#include "BMGameUserSettings.h"
#include "BMCombatTargeting.h"
#include "BMCampaignLevel.h"
#include "BMHealthComponent.h"
#include "BMEnemyCharacter.h"
#include "BMPlayerCharacter.h"
#include "BMPlayerController.h"
#include "BMPlayerState.h"
#include "BMWeapon.h"
#include "BMWeaponDataAsset.h"
#include "CanvasItem.h"
#include "Camera/CameraComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

ABMHUD::ABMHUD()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ABMHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas)
    {
#if !UE_BUILD_SHIPPING
        if (DiagnosticsLogTimeRemaining <= 0.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("Black Meridian HUD: DrawHUD has no canvas owner=%s"), PlayerOwner ? *PlayerOwner->GetName() : TEXT("none"));
            DiagnosticsLogTimeRemaining = 2.0f;
        }
#endif
        return;
    }

    // Runtime-created UMG widgets are not guaranteed to receive Slate ticks
    // on every packaged/command-line path. Refresh the gameplay surface from
    // the HUD's authoritative draw loop so target lock, reticle color, ammo,
    // health, interaction, and kill feedback cannot become stale. This must
    // also happen during development presentation capture: the capture flag
    // suppresses the legacy canvas only and must not freeze the visible native
    // target/reticle state.
    if (NativeUIWidget && Screen != EBMHUDScreen::Lobby)
    {
        NativeUIWidget->RefreshFromHUD(this);
    }

    if (bPresentationCapture)
    {
        return;
    }

    const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
#if !UE_BUILD_SHIPPING
    DiagnosticsLogTimeRemaining -= DeltaSeconds;
    if (DiagnosticsLogTimeRemaining <= 0.0f)
    {
        const APawn* Pawn = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;
        const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
        const ABMPlayerCharacter* PlayerPawn = Cast<ABMPlayerCharacter>(Pawn);
        const FString PawnLocation = Pawn ? Pawn->GetActorLocation().ToString() : TEXT("none");
        const FString CameraRotation = PlayerPawn && PlayerPawn->GetFirstPersonCamera()
            ? PlayerPawn->GetFirstPersonCamera()->GetComponentRotation().ToString()
            : TEXT("none");
        const TCHAR* Grounded = PlayerPawn && PlayerPawn->GetCharacterMovement() && PlayerPawn->GetCharacterMovement()->IsMovingOnGround()
            ? TEXT("yes")
            : TEXT("no");
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian HUD: draw canvas=%.0fx%.0f screen=%d owner=%s pawn=%s player=%s state=%s match_live=%s location=%s grounded=%s camera=%s"),
            Canvas->ClipX,
            Canvas->ClipY,
            static_cast<int32>(Screen),
            PlayerOwner ? *PlayerOwner->GetName() : TEXT("none"),
            Pawn ? *Pawn->GetName() : TEXT("none"),
            PlayerPawn ? TEXT("bm_player") : TEXT("other"),
            State ? TEXT("present") : TEXT("none"),
            State && State->bMatchLive ? TEXT("yes") : TEXT("no"),
            *PawnLocation,
            Grounded,
            *CameraRotation);
        DiagnosticsLogTimeRemaining = 2.0f;
    }
#endif
    SubtitleTimeRemaining = FMath::Max(0.0f, SubtitleTimeRemaining - DeltaSeconds);
    MissionCardTimeRemaining = FMath::Max(0.0f, MissionCardTimeRemaining - DeltaSeconds);
    HitMarkerTimeRemaining = FMath::Max(0.0f, HitMarkerTimeRemaining - DeltaSeconds);
    DryFireTimeRemaining = FMath::Max(0.0f, DryFireTimeRemaining - DeltaSeconds);
    EliminationTimeRemaining = FMath::Max(0.0f, EliminationTimeRemaining - DeltaSeconds);
    DamageDirectionTimeRemaining = FMath::Max(0.0f, DamageDirectionTimeRemaining - DeltaSeconds);
    DamageFlashTimeRemaining = FMath::Max(0.0f, DamageFlashTimeRemaining - DeltaSeconds);

    // The authored UMG surface owns the presentation once Slate has laid out
    // its runtime-created root. Keep the canvas implementation alive until
    // that first valid geometry exists so a packaged launch never flashes a
    // blank menu or HUD while the widget is being constructed.
    const bool bNativeUIHasGeometry = NativeUIWidget
        && NativeUIWidget->IsInViewport()
        && NativeUIWidget->GetVisibility() == ESlateVisibility::Visible
        && NativeUIWidget->GetCachedGeometry().GetLocalSize().X > 1.0f
        && NativeUIWidget->GetCachedGeometry().GetLocalSize().Y > 1.0f;
    if (bNativeUIHasGeometry && Screen != EBMHUDScreen::Lobby)
    {
        return;
    }

    if (Screen == EBMHUDScreen::InGame || Screen == EBMHUDScreen::Scoreboard)
    {
        DrawCombatHUD();
    }

    switch (Screen)
    {
    case EBMHUDScreen::Splash:
        DrawMenuOverlay(TEXT("BLACK MERIDIAN"), TEXT("LATTICE RESPONSE UNIT\n\nA SIGNAL CAN BE A WARNING.\nOR A WEAPON."));
        break;
    case EBMHUDScreen::MainMenu:
        DrawMenuOverlay(
            TEXT("BLACK MERIDIAN"),
            TEXT("A FIRST-PERSON MILITARY THRILLER\n\nENTER   CAMPAIGN\nF4      MULTIPLAYER\nF1      SETTINGS\nK       CONTROLS  (F2: Fn+F2 on Mac)\nH       HOW TO PLAY\n\nWASD  MOVE     MOUSE  LOOK\nSHIFT SPRINT  RMB ADS  LMB FIRE"));
        break;
    case EBMHUDScreen::CampaignSelection:
    {
        const ABMPlayerController* Controller = PlayerOwner ? Cast<ABMPlayerController>(PlayerOwner) : nullptr;
        const EBMCampaignLevel SelectedLevel = Controller ? Controller->GetSelectedCampaignLevel() : EBMCampaignLevel::SableQuay;
        const FBMCampaignLevelDefinition Definition = GetCampaignLevelDefinition(SelectedLevel);
        const int32 LevelNumber = static_cast<int32>(Definition.Level) + 1;
        const int32 CampaignLevelCount = GetBMCampaignLevelCount();
        const FString LevelName = Definition.DisplayName.ToString();
        const FString Briefing = Definition.Briefing.ToString();
        DrawMenuOverlay(
            FString::Printf(TEXT("CAMPAIGN  //  LEVEL %d/%d"), LevelNumber, CampaignLevelCount),
            FString::Printf(
                TEXT("OPERATION GLASSWAKE\n%s\n\n%s\n\n%s\n\nSOLO MISSION // 20 DISTINCT OPERATIONS\nWASD MOVE   MOUSE LOOK   LMB FIRE   RMB AIM\nUP / DOWN OR LEFT / RIGHT  SELECT LEVEL\nWEAPONS + AMMO AUTO-COLLECT WHEN CLOSE\nENTER DEPLOY   H HOW TO PLAY   ESC BACK"),
                *LevelName,
                *Briefing,
                *GetBMCampaignLevelListText(SelectedLevel)));
        break;
    }
    case EBMHUDScreen::MultiplayerMenu:
        DrawMenuOverlay(TEXT("MULTIPLAYER"), TEXT("SIGNAL CONTROL\n2–12 PLAYERS // 6v6 TARGET\nTHREE ROTATING NODES\n\nENTER   OPEN LOBBY\nESC     BACK"));
        break;
    case EBMHUDScreen::Lobby:
    {
        const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
        const bool bLobbyWidgetHasGeometry = LobbyWidget
            && LobbyWidget->IsInViewport()
            && LobbyWidget->GetCachedGeometry().GetLocalSize().X > 1.0f
            && LobbyWidget->GetCachedGeometry().GetLocalSize().Y > 1.0f;
        if (State && State->bSignalControlMode && !bLobbyWidgetHasGeometry)
        {
            // Keep the proven canvas lobby visible until Slate has laid out
            // the native widget. This prevents a one-frame/zero-geometry UMG
            // failure from presenting a blank game window.
            DrawSignalLobby(State);
        }
        else if (!bLobbyWidgetHasGeometry)
        {
            DrawMenuOverlay(TEXT("SIGNAL CONTROL // LOBBY"), TEXT("ROSTER SYNCING...\n\nENTER   START LOCAL MATCH\nESC     BACK"));
        }
        break;
    }
    case EBMHUDScreen::Loading:
    {
        const ABMPlayerController* Controller = PlayerOwner ? Cast<ABMPlayerController>(PlayerOwner) : nullptr;
        const EBMCampaignLevel SelectedLevel = Controller ? Controller->GetSelectedCampaignLevel() : EBMCampaignLevel::SableQuay;
        const FBMCampaignLevelDefinition Definition = GetCampaignLevelDefinition(SelectedLevel);
        DrawMenuOverlay(TEXT("DEPLOYING"), FString::Printf(TEXT("%s\n\nSYNCING MISSION STATE..."), *Definition.DisplayName.ToString()));
        break;
    }
    case EBMHUDScreen::Settings:
        if (const UBMGameUserSettings* Settings = GEngine ? Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr)
        {
            const ABMPlayerController* Controller = PlayerOwner ? Cast<ABMPlayerController>(PlayerOwner) : nullptr;
            const int32 SelectedRow = Controller ? Controller->GetSettingsRow() : 0;
            const auto Row = [SelectedRow](const int32 Index, const FString& Label, const FString& Value)
            {
                return FString::Printf(TEXT("%s %-25s %s"), SelectedRow == Index ? TEXT(">") : TEXT(" "), *Label, *Value);
            };
            DrawMenuOverlay(
                TEXT("SETTINGS"),
                FString::Printf(TEXT("%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n\nUP / DOWN  SELECT\nLEFT / RIGHT  ADJUST\nESC           BACK"),
                    *Row(0, TEXT("FOV"), FString::Printf(TEXT("%d"), FMath::RoundToInt(Settings->FieldOfView))),
                    *Row(1, TEXT("HORIZONTAL SENSITIVITY"), FString::Printf(TEXT("%.2f"), Settings->HorizontalSensitivity)),
                    *Row(2, TEXT("VERTICAL SENSITIVITY"), FString::Printf(TEXT("%.2f"), Settings->VerticalSensitivity)),
                    *Row(3, TEXT("HIP-FIRE SENSITIVITY"), FString::Printf(TEXT("%.2f"), Settings->HipFireSensitivity)),
                    *Row(4, TEXT("ADS SENSITIVITY"), FString::Printf(TEXT("%.2f"), Settings->ADSSensitivity)),
                    *Row(5, TEXT("TOGGLE ADS"), Settings->bToggleADS ? TEXT("ON") : TEXT("OFF")),
                    *Row(6, TEXT("TOGGLE CROUCH"), Settings->bToggleCrouch ? TEXT("ON") : TEXT("OFF")),
                    *Row(7, TEXT("TOGGLE SPRINT"), Settings->bToggleSprint ? TEXT("ON") : TEXT("OFF")),
                    *Row(8, TEXT("CAMERA MOTION"), Settings->bReducedCameraMotion ? TEXT("REDUCED") : TEXT("FULL")),
                    *Row(9, TEXT("MOTION BLUR"), Settings->bReducedMotionBlur ? TEXT("REDUCED") : TEXT("FULL")),
                    *Row(10, TEXT("SUBTITLE SIZE"), Settings->SubtitleSize == 0 ? TEXT("SMALL") : (Settings->SubtitleSize == 1 ? TEXT("STANDARD") : TEXT("LARGE"))),
                    *Row(11, TEXT("COLOUR-BLIND PRESET"), FString::Printf(TEXT("%d"), Settings->ColorBlindPreset)),
                    *Row(12, TEXT("GORE REDUCTION"), Settings->bGoreReduction ? TEXT("ON") : TEXT("OFF")),
                    *Row(13, TEXT("GRAPHICS QUALITY"), Settings->GraphicsQualityPreset == 0 ? TEXT("LOW") : (Settings->GraphicsQualityPreset == 1 ? TEXT("MEDIUM") : (Settings->GraphicsQualityPreset == 2 ? TEXT("HIGH") : TEXT("EPIC")))),
                    *Row(14, TEXT("MASTER VOLUME"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->MasterVolume * 100.0f))),
                    *Row(15, TEXT("MUSIC VOLUME"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->MusicVolume * 100.0f))),
                    *Row(16, TEXT("EFFECTS VOLUME"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->EffectsVolume * 100.0f))),
                    *Row(17, TEXT("DIALOGUE VOLUME"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->DialogueVolume * 100.0f))),
                    *Row(18, TEXT("FRIENDLY FIRE"), Settings->bFriendlyFireEnabled ? TEXT("ON") : TEXT("OFF"))));
        }
        break;
    case EBMHUDScreen::Controls:
    {
        const ABMPlayerController* Controller = PlayerOwner ? Cast<ABMPlayerController>(PlayerOwner) : nullptr;
        const auto ControlRow = [Controller](const int32 Index, const FString& Text)
        {
            return FString::Printf(TEXT("%s%s"), Controller && Controller->GetControlsRow() == Index ? TEXT("> ") : TEXT("  "), *Text);
        };
        FString ControlsBody;
        if (Controller)
        {
            for (int32 Index = 0; Index < 13; ++Index)
            {
                if (Index > 0)
                {
                    ControlsBody += TEXT("\n");
                }
                ControlsBody += ControlRow(Index, Controller->GetControlBindingLabel(Index));
            }
        }
        else
        {
            ControlsBody = TEXT("OPEN CONTROLS IN A PLAYER SESSION");
        }
        ControlsBody += TEXT("\n\nCONTROLLER BUTTONS STAY AVAILABLE\nUP / DOWN  SELECT\nLEFT / RIGHT  REMAP KEY\nESC           BACK");
        DrawMenuOverlay(TEXT("CONTROLS"), ControlsBody);
        break;
    }
    case EBMHUDScreen::HowToPlay:
    {
        const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
        if (State && State->bSignalControlMode)
        {
            DrawMenuOverlay(
                TEXT("HOW TO PLAY // SIGNAL CONTROL"),
                TEXT("TWO TEAMS FIGHT OVER ONE ACTIVE SIGNAL NODE.\n\n1  FOLLOW THE BRIGHT NODE BEACON.\n2  STAND INSIDE ITS GLOW TO CAPTURE IT.\n3  HOLD THE NODE TO SCORE FOR YOUR TEAM.\n4  THE ACTIVE NODE ROTATES—MOVE WHEN IT DOES.\n5  ELIMINATE ENEMIES AND REJOIN YOUR TEAM.\n\nWASD MOVE   MOUSE LOOK   LMB FIRE   RMB AIM\nR RELOAD   TAB SCOREBOARD   ESC BACK"));
        }
        else
        {
            DrawMenuOverlay(
                TEXT("HOW TO PLAY // GLASSWAKE"),
                TEXT("GOAL  BREACH THE RELAY, SECURE INTEL, AND ESCAPE.\n\n1  FOLLOW THE TEAL BEACON TO START.\n2  REACH AMBER, THEN CLEAR THE RED VANTA WAVE.\n3  FOLLOW ORANGE AND HOLD E AT THE RELAY DOOR.\n4  TEAL CONSOLE INTEL IS OPTIONAL; THE ROUTE CONTINUES.\n5  FOLLOW YELLOW, HOLD E AT THE BOAT, THEN DEFEND THE PIER.\n\nWASD MOVE   MOUSE LOOK   LMB FIRE   RMB AIM\nG FRAG   X VEIL   R RELOAD   SHIFT SPRINT   E INTERACT   H CLOSE"));
        }
        break;
    }
    case EBMHUDScreen::Pause:
            DrawMenuOverlay(TEXT("PAUSED"), TEXT("ENTER   RESUME\nF1      SETTINGS\nK       CONTROLS  (F2: Fn+F2 on Mac)\nBACKSPACE  RESTART CHECKPOINT\nH       HOW TO PLAY\nESC     RESUME"));
        break;
    case EBMHUDScreen::Death:
        DrawMenuOverlay(
            TEXT("SQUAD LOST"),
            TEXT("PLAYER ELIMINATED // LOOK DISABLED\nWORLD PAUSED // RESTART TO CONTINUE\n\nENTER   RESTART FROM CHECKPOINT\nBACKSPACE  RESTART FROM CHECKPOINT\nESC     MAIN MENU"));
        break;
    case EBMHUDScreen::Results:
    {
        const ABMGameState* ResultState = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
        if (ResultState && ResultState->bSignalControlMode)
        {
            const TCHAR* WinnerLabel = ResultState->SignalControl.WinningTeam == EBMTeam::Lattice
                ? TEXT("LATTICE RESPONSE UNIT")
                : (ResultState->SignalControl.WinningTeam == EBMTeam::Vanta ? TEXT("VANTA CONTINUUM") : TEXT("NO WINNER"));
            DrawMenuOverlay(
                TEXT("SIGNAL CONTROL // MATCH COMPLETE"),
                FString::Printf(
                    TEXT("WINNER  %s\n\nLATTICE  %03d\nVANTA    %03d\n\nENTER   RETURN TO MENU\nESC     CLOSE"),
                    WinnerLabel,
                    ResultState->SignalControl.LatticeScore,
                    ResultState->SignalControl.VantaScore));
        }
        else
        {
            const bool bHasNextCampaignLevel = ResultState &&
                static_cast<int32>(ResultState->CampaignLevel) < GetBMCampaignLevelCount() - 1;
            if (bHasNextCampaignLevel)
            {
                const EBMCampaignLevel NextLevel = static_cast<EBMCampaignLevel>(static_cast<int32>(ResultState->CampaignLevel) + 1);
                const FBMCampaignLevelDefinition NextDefinition = GetCampaignLevelDefinition(NextLevel);
                DrawMenuOverlay(
                    TEXT("OPERATION COMPLETE"),
                    FString::Printf(
                        TEXT("THE ARCHIVE IS SECURED\n\nLEVEL %02d COMPLETE\nNEXT  LEVEL %02d // %s\n\nENTER   NEXT LEVEL\nESC     CAMPAIGN MENU"),
                        static_cast<int32>(ResultState->CampaignLevel) + 1,
                        static_cast<int32>(NextLevel) + 1,
                        *NextDefinition.DisplayName.ToString().ToUpper()));
            }
            else
            {
                DrawMenuOverlay(TEXT("OPERATION COMPLETE"), TEXT("THE ARCHIVE IS SECURED\n\nENTER   RETURN TO MENU\nESC     CLOSE"));
            }
        }
        break;
    }
    default:
        break;
    }

    // Briefing cards and subtitles belong to the live gameplay surface. They
    // must not leak over death, results, loading, or menu screens when a
    // phase transition and a player elimination happen in the same frame.
    const bool bGameplayPresentation = Screen == EBMHUDScreen::InGame
        || Screen == EBMHUDScreen::Scoreboard;
    if (bGameplayPresentation && SubtitleTimeRemaining > 0.0f && !Subtitle.IsEmpty())
    {
        DrawTextLine(Subtitle.ToString(), Canvas->ClipX * 0.18f, Canvas->ClipY * 0.78f, GetSubtitleScale(), FLinearColor::White);
    }
    if (bGameplayPresentation && MissionCardTimeRemaining > 0.0f && !MissionCardTitle.IsEmpty())
    {
        // Keep transient briefing copy below the persistent objective header
        // and to the left of the reticle.  The old 58%-wide centered card was
        // still covering the first contact even after the rest of the HUD had
        // correctly entered the combat state.
        const float Left = 42.0f;
        const float Top = FMath::Max(170.0f, Canvas->ClipY * 0.22f);
        const float CardWidth = FMath::Min(Canvas->ClipX * 0.46f, 650.0f);
        const int32 MaxCharsPerLine = FMath::Clamp(FMath::RoundToInt(CardWidth / 8.0f), 52, 78);
        TArray<FString> MissionCardLines;
        TArray<FString> RawMissionCardLines;
        MissionCardBody.ToString().ParseIntoArray(RawMissionCardLines, TEXT("\n"), false);
        for (const FString& RawLine : RawMissionCardLines)
        {
            FString Remaining = RawLine;
            Remaining.TrimStartAndEndInline();
            if (Remaining.IsEmpty())
            {
                continue;
            }

            while (Remaining.Len() > MaxCharsPerLine)
            {
                FString Window = Remaining.Left(MaxCharsPerLine);
                int32 BreakAt = INDEX_NONE;
                if (!Window.FindLastChar(TEXT(' '), BreakAt) || BreakAt < 1)
                {
                    BreakAt = MaxCharsPerLine;
                }
                MissionCardLines.Add(Remaining.Left(BreakAt));
                Remaining = Remaining.Mid(BreakAt);
                Remaining.TrimStartAndEndInline();
            }
            if (!Remaining.IsEmpty())
            {
                MissionCardLines.Add(Remaining);
            }
        }
        if (MissionCardLines.Num() == 0)
        {
            MissionCardLines.Add(TEXT("OBJECTIVE UPDATE"));
        }
        const float CardHeight = 72.0f + MissionCardLines.Num() * 24.0f;
        FCanvasTileItem CardPanel(
            FVector2D(Left - 18.0f, Top - 18.0f),
            FVector2D(CardWidth, FMath::Min(CardHeight, Canvas->ClipY * 0.30f)),
            FLinearColor(0.005f, 0.012f, 0.018f, 0.84f));
        CardPanel.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(CardPanel);
        const float CardAlpha = MissionCardTimeRemaining > 0.8f ? 1.0f : MissionCardTimeRemaining / 0.8f;
        DrawTextLine(MissionCardTitle.ToString(), Left, Top, 1.16f, FLinearColor(0.4f, 0.95f, 0.84f, CardAlpha));
        for (int32 Index = 0; Index < MissionCardLines.Num(); ++Index)
        {
            DrawTextLine(
                MissionCardLines[Index],
                Left,
                Top + 40.0f + Index * 24.0f,
                0.70f,
                FLinearColor(0.86f, 0.88f, 0.84f, CardAlpha));
        }

        // Keep the immediate destination above the briefing layer. The card
        // is useful context, but it must never hide the thing the player is
        // meant to walk toward during the first few seconds.
        if (Screen == EBMHUDScreen::InGame)
        {
            if (const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
                State && !State->bSignalControlMode)
            {
                if (const ABMPlayerCharacter* Player = PlayerOwner ? Cast<ABMPlayerCharacter>(PlayerOwner->GetPawn()) : nullptr)
                {
                    DrawObjectiveMarker(Player, State);
                }
            }
        }
    }
}

void ABMHUD::SetScreen(const EBMHUDScreen NewScreen)
{
    Screen = NewScreen;
    bScoreboardVisible = NewScreen == EBMHUDScreen::Scoreboard;

    if (!NativeUIWidget && PlayerOwner)
    {
        NativeUIWidget = CreateWidget<UBMNativeUIWidget>(PlayerOwner, UBMNativeUIWidget::StaticClass());
    }
    if (NativeUIWidget && PlayerOwner)
    {
        if (!NativeUIWidget->IsInViewport())
        {
            NativeUIWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
            NativeUIWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
            int32 ViewportWidth = 1440;
            int32 ViewportHeight = 900;
            PlayerOwner->GetViewportSize(ViewportWidth, ViewportHeight);
            if (ViewportWidth > 0 && ViewportHeight > 0)
            {
                NativeUIWidget->SetDesiredSizeInViewport(FVector2D(ViewportWidth, ViewportHeight));
                NativeUIWidget->SetPositionInViewport(FVector2D::ZeroVector, false);
            }
            NativeUIWidget->AddToViewport(110);
        }
        NativeUIWidget->SetVisibility(NewScreen == EBMHUDScreen::Lobby ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
        NativeUIWidget->RefreshFromHUD(this);
    }

    if (NewScreen == EBMHUDScreen::Lobby)
    {
        if (!LobbyWidget && PlayerOwner)
        {
            LobbyWidget = CreateWidget<UBMLobbyWidget>(PlayerOwner, UBMLobbyWidget::StaticClass());
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian UMG lobby: screen requested player_owner=%s widget=%s"),
            PlayerOwner ? *PlayerOwner->GetName() : TEXT("none"),
            LobbyWidget ? TEXT("created") : TEXT("missing"));
        if (LobbyWidget)
        {
            LobbyWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
            LobbyWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
            // Native UMG widgets have no Blueprint viewport layout to supply
            // a root size. Give the runtime-created widget the live viewport
            // bounds before adding it so its full-screen backdrop and the
            // centered lobby panel receive real Slate geometry.
            int32 ViewportWidth = 0;
            int32 ViewportHeight = 0;
            if (PlayerOwner)
            {
                PlayerOwner->GetViewportSize(ViewportWidth, ViewportHeight);
            }
            if (ViewportWidth > 0 && ViewportHeight > 0)
            {
                LobbyWidget->SetDesiredSizeInViewport(FVector2D(ViewportWidth, ViewportHeight));
                LobbyWidget->SetPositionInViewport(FVector2D::ZeroVector, false);
            }
            LobbyWidget->AddToViewport(100);
            LobbyWidget->SetVisibility(ESlateVisibility::Visible);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian UMG lobby: added to viewport visible=%s in_viewport=%s"),
                LobbyWidget->GetVisibility() == ESlateVisibility::Visible ? TEXT("yes") : TEXT("no"),
                LobbyWidget->IsInViewport() ? TEXT("yes") : TEXT("no"));
            if (const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr)
            {
                LobbyWidget->RefreshFromState(State);
            }
        }
    }
    else if (LobbyWidget)
    {
        LobbyWidget->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (NewScreen != EBMHUDScreen::InGame && NewScreen != EBMHUDScreen::Scoreboard)
    {
        // These are transient gameplay messages, not menu content. Clearing
        // them at the screen boundary prevents a stale opening card from
        // obscuring the actionable death/results/loading prompt.
        SubtitleTimeRemaining = 0.0f;
        MissionCardTimeRemaining = 0.0f;
        InteractionPrompt.Empty();
    }
}

void ABMHUD::ShowSubtitle(const FText& Text, const float Duration)
{
    Subtitle = Text;
    SubtitleTimeRemaining = Duration;
}

void ABMHUD::ShowMissionCard(const FText& Title, const FText& Body, const float Duration)
{
    MissionCardTitle = Title;
    MissionCardBody = Body;
    MissionCardTimeRemaining = FMath::Max(0.0f, Duration);
}

void ABMHUD::DismissMissionCard()
{
    MissionCardTimeRemaining = 0.0f;
    MissionCardTitle = FText::GetEmpty();
    MissionCardBody = FText::GetEmpty();
}

void ABMHUD::ShowInteractionPrompt(const FText& Prompt)
{
    InteractionPrompt = Prompt.ToString();
}

void ABMHUD::ShowHitMarker(const bool bHeadshot)
{
    DismissMissionCard();
    bHitMarkerHeadshot = bHeadshot;
    HitMarkerTimeRemaining = bHeadshot ? 0.3f : 0.2f;
}

void ABMHUD::ShowDryFire()
{
    DismissMissionCard();
    DryFireTimeRemaining = 0.28f;
}

void ABMHUD::ShowElimination(const bool bHeadshot, const int32 KillCount)
{
    DismissMissionCard();
    bEliminationHeadshot = bHeadshot;
    EliminationKillCount = FMath::Max(0, KillCount);
    EliminationTimeRemaining = 1.15f;
}

void ABMHUD::SetDamageDirection(const float RelativeYaw)
{
    DamageDirectionYaw = RelativeYaw;
    DamageDirectionTimeRemaining = 0.65f;
    // Damage direction is already communicated by the directional chevron.
    // Keep the vignette as a peripheral urgency cue so a few opening-wave
    // hits do not tint the whole combat read and hide the enemy silhouette.
    ShowDamageFlash(0.55f);
}

void ABMHUD::ShowDamageFlash(const float Intensity)
{
    DismissMissionCard();
    DamageFlashIntensity = FMath::Max(DamageFlashIntensity, FMath::Clamp(Intensity, 0.0f, 1.0f));
    DamageFlashTimeRemaining = FMath::Max(DamageFlashTimeRemaining, 0.34f);
}

void ABMHUD::SetScoreboardVisible(const bool bVisible)
{
    bScoreboardVisible = bVisible;
    Screen = bVisible ? EBMHUDScreen::Scoreboard : EBMHUDScreen::InGame;
}

void ABMHUD::DrawCrosshair()
{
    const float CenterX = Canvas->ClipX * 0.5f;
    const float CenterY = Canvas->ClipY * 0.5f;
    const ABMPlayerCharacter* Player = PlayerOwner ? Cast<ABMPlayerCharacter>(PlayerOwner->GetPawn()) : nullptr;
    const ABMWeapon* Weapon = Player ? Player->GetCurrentWeapon() : nullptr;
    const float Speed = Player ? Player->GetVelocity().Size2D() : 0.0f;
    const float MovementAlpha = FMath::Clamp(Speed / 620.0f, 0.0f, 1.0f);
    float SpreadAlpha = MovementAlpha * 0.42f;
    float ShotPulse = 0.0f;
    bool bAiming = false;
    if (Weapon && Weapon->GetWeaponData())
    {
        const float MaxPenalty = FMath::Max(0.1f, Weapon->GetWeaponData()->Balance.MaxAccuracyPenaltyDegrees);
        SpreadAlpha += FMath::Clamp(Weapon->GetAccuracyPenaltyDegrees() / MaxPenalty, 0.0f, 1.0f) * 0.58f;
        ShotPulse = Weapon->GetPresentationShotPulse();
        bAiming = Player->IsAiming();
    }
    SpreadAlpha = FMath::Clamp(SpreadAlpha, 0.0f, 1.0f);

    const float Gap = (bAiming ? 7.0f : 12.0f) + SpreadAlpha * 21.0f + ShotPulse * 5.0f;
    const float ArmLength = bAiming ? 8.0f : 12.0f;
    const float Thickness = bAiming ? 3.0f : 4.0f;
    const float ReticleAlpha = DryFireTimeRemaining > 0.0f
        ? FMath::Clamp(DryFireTimeRemaining / 0.28f, 0.0f, 1.0f)
        : 0.96f;

    // Reticle feedback must describe the first thing the player can actually
    // hit, not merely an enemy somewhere behind cover.  Consume the player's
    // post-camera snapshot so the reticle and the submitted shot use one
    // target decision for this frame.  The snapshot itself is produced by the
    // player after all camera presentation transforms have been applied.
    bool bTargetingLiveEnemy = false;
    if (Player && !Player->IsEliminated() && GetWorld())
    {
        const ABMCombatCharacter* CrosshairTarget = Player->HasLocalAimSnapshot()
            ? Player->GetLocalAimTarget()
            : nullptr;
        if (CrosshairTarget)
        {
            bTargetingLiveEnemy = BMCombatTargeting::IsLiveHostileTarget(Player, CrosshairTarget);
        }
        else if (!Player->HasLocalAimSnapshot() && Player->GetFirstPersonCamera())
        {
            const FVector TraceStart = Player->GetFirstPersonCamera()->GetComponentLocation();
            const FVector TraceDirection = Player->GetAimDirection();
            const float TraceRange = Weapon && Weapon->GetWeaponData()
                ? FMath::Max(100.0f, Weapon->GetWeaponData()->Balance.RangeMeters * 100.0f)
                : 10000.0f;
            bTargetingLiveEnemy = BMCombatTargeting::ResolveLiveHostileCrosshairTarget(
                GetWorld(),
                Player,
                TraceStart,
                TraceDirection,
                TraceRange,
                Weapon) != nullptr;
        }
    }

    const FLinearColor ReticleColor = DryFireTimeRemaining > 0.0f
        ? FLinearColor(1.0f, 0.22f, 0.12f, ReticleAlpha)
        : ApplyAccessibilityColor(bTargetingLiveEnemy
            ? FLinearColor(1.0f, 0.08f, 0.05f, ReticleAlpha)
            : FLinearColor(0.72f, 0.94f, 0.86f, ReticleAlpha));
    const auto DrawBar = [this](const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color)
    {
        FCanvasTileItem Bar(Position, Size, Color);
        Bar.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(Bar);
    };
    DrawBar(FVector2D(CenterX - Thickness * 0.5f, CenterY - Gap - ArmLength), FVector2D(Thickness, ArmLength), ReticleColor);
    DrawBar(FVector2D(CenterX - Thickness * 0.5f, CenterY + Gap), FVector2D(Thickness, ArmLength), ReticleColor);
    DrawBar(FVector2D(CenterX - Gap - ArmLength, CenterY - Thickness * 0.5f), FVector2D(ArmLength, Thickness), ReticleColor);
    DrawBar(FVector2D(CenterX + Gap, CenterY - Thickness * 0.5f), FVector2D(ArmLength, Thickness), ReticleColor);
    DrawBar(FVector2D(CenterX - 3.0f, CenterY - 3.0f), FVector2D(6.0f, 6.0f), ReticleColor);
    if (HitMarkerTimeRemaining > 0.0f)
    {
        const float Fade = FMath::Clamp(HitMarkerTimeRemaining / (bHitMarkerHeadshot ? 0.3f : 0.2f), 0.0f, 1.0f);
        const FLinearColor MarkerColor = bHitMarkerHeadshot
            ? FLinearColor(0.18f, 0.92f, 1.0f, Fade)
            : FLinearColor(1.0f, 0.65f, 0.2f, Fade);
        DrawTextLine(TEXT("×"), CenterX - 9.0f, CenterY - 15.0f, bHitMarkerHeadshot ? 1.45f : 1.3f, MarkerColor);
        if (bHitMarkerHeadshot)
        {
            DrawTextLine(TEXT("HEADSHOT"), CenterX - 44.0f, CenterY + 24.0f, 0.68f, MarkerColor);
        }
    }
    if (DamageDirectionTimeRemaining > 0.0f)
    {
        const FString Direction = FMath::Abs(DamageDirectionYaw) < 45.0f ? TEXT("▲") : (DamageDirectionYaw > 0.0f ? TEXT("◀") : TEXT("▶"));
        DrawTextLine(Direction, CenterX - 10.0f, 90.0f, 1.3f, FLinearColor(0.95f, 0.22f, 0.18f, DamageDirectionTimeRemaining));
    }
}

void ABMHUD::DrawDamageFeedback()
{
    if (!Canvas || DamageFlashTimeRemaining <= 0.0f || DamageFlashIntensity <= 0.0f)
    {
        return;
    }

    const float Fade = FMath::Clamp(DamageFlashTimeRemaining / 0.34f, 0.0f, 1.0f);
    const float Alpha = Fade * DamageFlashIntensity * 0.20f;
    const float Edge = FMath::Lerp(12.0f, 32.0f, Fade);
    const FLinearColor FeedbackColor(0.78f, 0.035f, 0.022f, Alpha);
    const auto DrawEdge = [this, &FeedbackColor](const FVector2D& Position, const FVector2D& Size)
    {
        FCanvasTileItem EdgeItem(Position, Size, FeedbackColor);
        EdgeItem.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(EdgeItem);
    };

    DrawEdge(FVector2D(0.0f, 0.0f), FVector2D(Canvas->ClipX, Edge));
    DrawEdge(FVector2D(0.0f, Canvas->ClipY - Edge), FVector2D(Canvas->ClipX, Edge));
    DrawEdge(FVector2D(0.0f, Edge), FVector2D(Edge, Canvas->ClipY - Edge * 2.0f));
    DrawEdge(FVector2D(Canvas->ClipX - Edge, Edge), FVector2D(Edge, Canvas->ClipY - Edge * 2.0f));
}

void ABMHUD::DrawThrowableThreat(const ABMPlayerCharacter* Player)
{
    if (!Canvas || !Player || !Player->HasThrowableThreat())
    {
        return;
    }

    const FVector ThreatLocation = Player->GetThrowableThreatLocation();
    const FVector ToThreat = ThreatLocation - Player->GetActorLocation();
    const float RelativeYaw = FMath::UnwindDegrees(ToThreat.Rotation().Yaw - Player->GetControlRotation().Yaw);
    const TCHAR* Direction = TEXT("RIGHT");
    if (FMath::Abs(RelativeYaw) <= 42.0f)
    {
        Direction = TEXT("FRONT");
    }
    else if (FMath::Abs(RelativeYaw) >= 138.0f)
    {
        Direction = TEXT("BEHIND");
    }
    else if (RelativeYaw > 0.0f)
    {
        Direction = TEXT("LEFT");
    }

    const float Pulse = 0.82f + 0.18f * (0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 9.0f));
    const FLinearColor WarningColor(1.0f, 0.16f, 0.035f, Pulse);
    const float CenterX = Canvas->ClipX * 0.5f;
    const float Top = 188.0f;
    FCanvasTileItem Panel(
        FVector2D(CenterX - 156.0f, Top - 8.0f),
        FVector2D(312.0f, 58.0f),
        FLinearColor(0.08f, 0.008f, 0.004f, 0.76f * Pulse));
    Panel.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(Panel);
    DrawTextLine(TEXT("INCOMING FRAG"), CenterX - 116.0f, Top, 0.94f, WarningColor);
    DrawTextLine(
        FString::Printf(TEXT("%s  //  %03dm"), Direction, FMath::RoundToInt(Player->GetThrowableThreatDistance())),
        CenterX - 82.0f,
        Top + 29.0f,
        0.66f,
        FLinearColor(1.0f, 0.62f, 0.32f, Pulse));
}

void ABMHUD::DrawCompass(const ABMPlayerCharacter* Player)
{
    if (!Canvas || !Player)
    {
        return;
    }

    static const TCHAR* DirectionLabels[] = {TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW")};
    constexpr int32 DirectionCount = UE_ARRAY_COUNT(DirectionLabels);
    const float CenterX = Canvas->ClipX * 0.5f;
    const float Top = 146.0f;
    const float Step = 52.0f;
    const int32 Heading = FMath::RoundToInt(FMath::UnwindDegrees(Player->GetControlRotation().Yaw));
    const int32 NormalizedHeading = (Heading + 360) % 360;
    const int32 CenterDirection = FMath::RoundToInt(static_cast<float>(NormalizedHeading) / 45.0f) % DirectionCount;

    FCanvasTileItem Panel(
        FVector2D(CenterX - Step * 4.8f, Top - 6.0f),
        FVector2D(Step * 9.6f, 32.0f),
        FLinearColor(0.005f, 0.012f, 0.018f, 0.68f));
    Panel.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(Panel);

    for (int32 RelativeIndex = -4; RelativeIndex <= 4; ++RelativeIndex)
    {
        const float X = CenterX + static_cast<float>(RelativeIndex) * Step;
        const int32 DirectionIndex = (CenterDirection + RelativeIndex + DirectionCount * 2) % DirectionCount;
        const bool bCenter = RelativeIndex == 0;
        DrawTextLine(
            DirectionLabels[DirectionIndex],
            X - (bCenter ? 10.0f : 14.0f),
            Top,
            bCenter ? 0.86f : 0.62f,
            bCenter ? FLinearColor(0.35f, 1.0f, 0.84f) : FLinearColor(0.62f, 0.7f, 0.68f));
    }

    DrawTextLine(
        FString::Printf(TEXT("HEADING %03d"), NormalizedHeading),
        CenterX - 42.0f,
        Top + 25.0f,
        0.52f,
        FLinearColor(0.48f, 0.82f, 0.76f));
}

void ABMHUD::DrawCombatHUD()
{
    DrawDamageFeedback();
    DrawCrosshair();

    if (EliminationTimeRemaining > 0.0f)
    {
        const float Fade = FMath::Clamp(EliminationTimeRemaining < 0.28f ? EliminationTimeRemaining / 0.28f : 1.0f, 0.0f, 1.0f);
        const FLinearColor EliminationColor(1.0f, 0.68f, 0.22f, Fade);
        const FString Label = bEliminationHeadshot ? TEXT("HEADSHOT  //  ELIMINATION") : TEXT("ELIMINATION");
        const float CenterX = Canvas->ClipX * 0.5f;
        const float PanelWidth = bEliminationHeadshot ? 300.0f : 250.0f;
        const float PanelTop = Canvas->ClipY - 286.0f;
        FCanvasTileItem Panel(
            FVector2D(CenterX - PanelWidth * 0.5f, PanelTop - 12.0f),
            FVector2D(PanelWidth, 54.0f),
            FLinearColor(0.02f, 0.028f, 0.034f, 0.78f * Fade));
        Panel.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(Panel);
        FCanvasTileItem Accent(
            FVector2D(CenterX - PanelWidth * 0.5f, PanelTop - 12.0f),
            FVector2D(4.0f, 54.0f),
            FLinearColor(1.0f, 0.42f, 0.08f, Fade));
        Accent.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(Accent);
        const float LabelWidth = bEliminationHeadshot ? 126.0f : 58.0f;
        DrawTextLine(Label, CenterX - LabelWidth, PanelTop - 1.0f, bEliminationHeadshot ? 0.82f : 0.78f, EliminationColor);
        DrawTextLine(
            FString::Printf(TEXT("KILL CONFIRMED  //  TOTAL %02d"), EliminationKillCount),
            CenterX - 92.0f,
            PanelTop + 24.0f,
            0.54f,
            FLinearColor(0.62f, 0.96f, 0.86f, Fade));
    }

    const ABMPlayerCharacter* Player = PlayerOwner ? Cast<ABMPlayerCharacter>(PlayerOwner->GetPawn()) : nullptr;
    const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
    if (!Player || !State)
    {
        return;
    }

    DrawThrowableThreat(Player);

    const float Health = Player->HealthComponent ? Player->HealthComponent->Health : 0.0f;
    const ABMWeapon* Weapon = Player->GetCurrentWeapon();
    const FString WeaponName = Weapon && Weapon->GetWeaponData() ? Weapon->GetWeaponData()->DisplayName.ToString() : TEXT("UNARMED");
    const FString Ammo = Weapon ? FString::Printf(TEXT("%02d / %03d"), Weapon->GetMagazineAmmo(), Weapon->GetReserveAmmo()) : TEXT("-- / ---");
    DrawTextLine(FString::Printf(TEXT("VITALS  %03d"), FMath::RoundToInt(Health)), 48.0f, Canvas->ClipY - 116.0f, 1.15f, FLinearColor(0.35f, 0.9f, 0.82f));
    DrawTextLine(WeaponName, Canvas->ClipX - 360.0f, Canvas->ClipY - 128.0f, 1.0f, FLinearColor::White);
    DrawTextLine(Ammo, Canvas->ClipX - 260.0f, Canvas->ClipY - 88.0f, 1.35f, FLinearColor::White);
    if (Weapon && Weapon->IsReloading())
    {
        const float ReloadProgress = Weapon->GetReloadProgress();
        // A percentage alone is difficult to read during a firefight. Keep
        // the feedback tied to the same procedural presentation phases that
        // move the magazine and charging handle, so the player knows whether
        // firing is still blocked by the magazine exchange or the chambering
        // beat. The thresholds are intentionally weapon-agnostic because the
        // progress is normalized against each profile's own reload duration.
        const TCHAR* ReloadPhase = ReloadProgress < 0.22f
            ? TEXT("MAG OUT")
            : (ReloadProgress < 0.68f ? TEXT("SEAT MAG") : TEXT("CHAMBER"));
        DrawTextLine(
            FString::Printf(
                TEXT("RELOADING  //  %s  //  %02d%%"),
                ReloadPhase,
                FMath::RoundToInt(ReloadProgress * 100.0f)),
            Canvas->ClipX - 260.0f,
            Canvas->ClipY - 142.0f,
            0.72f,
            FLinearColor(1.0f, 0.62f, 0.2f));
        const float ReloadBarWidth = 220.0f;
        const float ReloadBarHeight = 5.0f;
        const FVector2D ReloadBarPosition(Canvas->ClipX - 260.0f, Canvas->ClipY - 60.0f);
        FCanvasTileItem ReloadBarBackground(
            ReloadBarPosition,
            FVector2D(ReloadBarWidth, ReloadBarHeight),
            FLinearColor(0.01f, 0.02f, 0.025f, 0.88f));
        ReloadBarBackground.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(ReloadBarBackground);
        FCanvasTileItem ReloadBarFill(
            ReloadBarPosition,
            FVector2D(ReloadBarWidth * ReloadProgress, ReloadBarHeight),
            FLinearColor(1.0f, 0.56f, 0.18f, 0.96f));
        ReloadBarFill.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(ReloadBarFill);
    }
    else if (DryFireTimeRemaining > 0.0f)
    {
        DrawTextLine(TEXT("EMPTY  //  PRESS R"), Canvas->ClipX - 260.0f, Canvas->ClipY - 142.0f, 0.72f, FLinearColor(1.0f, 0.28f, 0.18f));
    }
    else if (Weapon && Weapon->GetMagazineAmmo() <= 5)
    {
        DrawTextLine(TEXT("LOW AMMO"), Canvas->ClipX - 260.0f, Canvas->ClipY - 142.0f, 0.72f, FLinearColor(1.0f, 0.32f, 0.2f));
    }
    const FString ContextLabel = State->bSignalControlMode ? TEXT("SIGNAL CONTROL") : GetMissionPhaseLabel();
    const FString ObjectiveLabel = State->bSignalControlMode
        ? FString::Printf(TEXT("NODE %d ACTIVE  //  ROTATES IN %02d"), State->SignalControl.ActiveNodeIndex + 1, FMath::CeilToInt(State->SignalControl.SecondsUntilRotation))
        : State->CurrentObjective.Title.ToString();
    // Keep the persistent objective readable without covering the route or
    // the first contact. The world beacon, compass, and flow strip carry the
    // extra navigation context now, so this panel only needs the active step.
    const float ObjectivePanelWidth = State->bSignalControlMode ? 600.0f : 560.0f;
    FCanvasTileItem ObjectivePanel(
        FVector2D(28.0f, 24.0f),
        FVector2D(FMath::Min(ObjectivePanelWidth, Canvas->ClipX * 0.44f), 96.0f),
        FLinearColor(0.005f, 0.012f, 0.018f, 0.72f));
    ObjectivePanel.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(ObjectivePanel);
    DrawTextLine(FString::Printf(TEXT("%s   //   %02d ENEMIES   //   CHECKPOINT %d"), *ContextLabel, State->HostileCount, State->CheckpointIndex), 48.0f, 38.0f, 0.66f, FLinearColor(0.62f, 0.9f, 0.84f));
    DrawTextLine(State->bSignalControlMode ? ObjectiveLabel : FString::Printf(TEXT("NEXT STEP  %s"), *ObjectiveLabel), 48.0f, 63.0f, 0.82f, State->CurrentObjective.bOptional ? FLinearColor(0.95f, 0.75f, 0.25f) : FLinearColor::White);
    const bool bMissionCombatPhase = !State->bSignalControlMode
        && (State->MissionPhase == EBMMissionPhase::Checkpoint
            || State->MissionPhase == EBMMissionPhase::OpenCombat
            || State->MissionPhase == EBMMissionPhase::FinalDefense);
    DrawTextLine(
        State->bSignalControlMode
            ? TEXT("FOLLOW THE BRIGHT NODE   //   TAB SCOREBOARD")
            : (bMissionCombatPhase ? TEXT("STAY IN THE COMBAT AREA   //   H HOW TO PLAY") : TEXT("FOLLOW THE BEACON   //   H HOW TO PLAY")),
        48.0f,
        87.0f,
        0.56f,
        FLinearColor(0.48f, 0.9f, 0.82f));
    if (!State->bSignalControlMode)
    {
        DrawMissionFlow(State);
        DrawObjectiveMarker(Player, State);
        DrawHostileMarkers(Player, State);
    }
    DrawCompass(Player);
    DrawTextLine(TEXT("SQUAD  MAKO  /  SABLE"), Canvas->ClipX - 250.0f, 42.0f, 0.72f, FLinearColor(0.35f, 0.75f, 0.72f));
    const ABMPlayerController* Controller = PlayerOwner ? Cast<ABMPlayerController>(PlayerOwner) : nullptr;
    const FString FragControl = Controller ? Controller->GetControlBindingLabel(9) : TEXT("FRAG  G");
    const FString TacticalControl = Controller ? Controller->GetControlBindingLabel(10) : TEXT("TACTICAL  X");
    DrawTextLine(
        FString::Printf(TEXT("%s    %s"), *FragControl, *TacticalControl),
        Canvas->ClipX - 365.0f,
        Canvas->ClipY - 72.0f,
        0.58f,
        FLinearColor(0.52f, 0.72f, 0.68f));
    const bool bAnyOrdnance = Player->GetFragCount() > 0 || Player->GetVeilCount() > 0;
    DrawTextLine(
        FString::Printf(
            TEXT("FRAG  %d/%d    VEIL  %d/%d"),
            Player->GetFragCount(),
            Player->GetMaxFragCount(),
            Player->GetVeilCount(),
            Player->GetMaxVeilCount()),
        Canvas->ClipX - 290.0f,
        Canvas->ClipY - 48.0f,
        0.76f,
        bAnyOrdnance ? FLinearColor(0.8f, 0.84f, 0.8f) : FLinearColor(1.0f, 0.28f, 0.18f));
    if (State->bSignalControlMode)
    {
        DrawTextLine(FString::Printf(TEXT("LATTICE %03d   VANTA %03d"), State->SignalControl.LatticeScore, State->SignalControl.VantaScore), Canvas->ClipX - 270.0f, 78.0f, 0.72f, FLinearColor(0.8f, 0.84f, 0.8f));
    }
    if (!State->bMatchLive && State->MatchCountdown > 0.0f && Screen == EBMHUDScreen::InGame)
    {
        DrawTextLine(FString::Printf(TEXT("ROUND STARTS IN  %02d"), FMath::CeilToInt(State->MatchCountdown)), Canvas->ClipX * 0.5f - 110.0f, Canvas->ClipY * 0.34f, 1.25f, FLinearColor(0.4f, 0.95f, 0.84f));
    }
    if (PlayerOwner->PlayerState)
    {
        DrawTextLine(FString::Printf(TEXT("PING %03d ms"), FMath::RoundToInt(PlayerOwner->PlayerState->GetPingInMilliseconds())), Canvas->ClipX - 150.0f, Canvas->ClipY - 18.0f, 0.65f, FLinearColor(0.65f, 0.7f, 0.68f));
        if (const ABMPlayerState* LocalPlayerState = Cast<ABMPlayerState>(PlayerOwner->PlayerState); LocalPlayerState && State->bSignalControlMode)
        {
            DrawTextLine(FString::Printf(TEXT("OBJECTIVE %03d"), LocalPlayerState->ObjectiveScore), Canvas->ClipX - 250.0f, Canvas->ClipY - 18.0f, 0.65f, FLinearColor(0.65f, 0.85f, 0.78f));
        }
    }
    for (int32 Index = 0; Index < State->RecentKillFeed.Num(); ++Index)
    {
        DrawTextLine(State->RecentKillFeed[Index], Canvas->ClipX - 330.0f, 132.0f + Index * 22.0f, 0.65f, FLinearColor(0.85f, 0.72f, 0.45f));
    }

    if (!InteractionPrompt.IsEmpty())
    {
        DrawTextLine(InteractionPrompt, Canvas->ClipX * 0.5f - 180.0f, Canvas->ClipY * 0.68f, 0.9f, FLinearColor(0.9f, 0.95f, 0.92f));
    }

    if (Player->IsHoldingInteraction())
    {
        const float Progress = Player->GetInteractionHoldProgress();
        const float BarWidth = FMath::Min(420.0f, Canvas->ClipX * 0.34f);
        const float BarHeight = 12.0f;
        const float BarLeft = Canvas->ClipX * 0.5f - BarWidth * 0.5f;
        const float BarTop = Canvas->ClipY * 0.72f;
        FCanvasTileItem BarBackground(FVector2D(BarLeft, BarTop), FVector2D(BarWidth, BarHeight), FLinearColor(0.01f, 0.02f, 0.025f, 0.88f));
        BarBackground.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(BarBackground);
        FCanvasTileItem BarFill(FVector2D(BarLeft, BarTop), FVector2D(BarWidth * Progress, BarHeight), FLinearColor(0.25f, 0.95f, 0.82f, 0.96f));
        BarFill.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(BarFill);
        DrawTextLine(
            FString::Printf(TEXT("HOLD E  %02d%%  //  RELEASE TO CANCEL"), FMath::RoundToInt(Progress * 100.0f)),
            BarLeft,
            BarTop - 30.0f,
            0.72f,
            FLinearColor(0.55f, 1.0f, 0.86f));
    }

    FString ActionLabel;
    bool bCombatAction = false;
    if (State->bSignalControlMode)
    {
        ActionLabel = TEXT("ACTION  CAPTURE THE ACTIVE NODE  //  STAY INSIDE THE GLOW TO SCORE");
    }
    else
    {
        switch (State->MissionPhase)
        {
        case EBMMissionPhase::Insertion:
            ActionLabel = TEXT("NEXT  WALK TO THE TEAL BEACON  //  WASD MOVE  //  NO ENEMIES YET");
            break;
        case EBMMissionPhase::QuietApproach:
            ActionLabel = TEXT("NEXT  WALK TO THE AMBER CHECKPOINT  //  STOP THERE  //  CONTACT STARTS THERE");
            break;
        case EBMMissionPhase::Checkpoint:
            bCombatAction = true;
            ActionLabel = FString::Printf(TEXT("COMBAT  SHOOT THE RED VANTA  //  STAY AT AMBER  //  LMB FIRE  //  %02d REMAINING"), State->HostileCount);
            break;
        case EBMMissionPhase::InteriorBreach:
            ActionLabel = TEXT("NEXT  WAVE CLEAR  //  FOLLOW ORANGE TO THE RELAY HOUSE  //  HOLD E AT THE DOOR");
            break;
        case EBMMissionPhase::Intelligence:
            ActionLabel = TEXT("OPTIONAL  WALK TO THE TEAL CONSOLE  //  HOLD E  //  THEN CONTINUE TO THE SEAWALL");
            break;
        case EBMMissionPhase::OpenCombat:
            bCombatAction = true;
            ActionLabel = FString::Printf(TEXT("COMBAT  SHOOT THE RED VANTA  //  STAY IN THE SEAWALL ARENA  //  %02d REMAINING"), State->HostileCount);
            break;
        case EBMMissionPhase::Extraction:
            ActionLabel = TEXT("NEXT  WALK TO THE YELLOW BOAT  //  HOLD E TO SIGNAL EXTRACTION");
            break;
        case EBMMissionPhase::FinalDefense:
            bCombatAction = true;
            ActionLabel = FString::Printf(TEXT("FINAL DEFENSE  SHOOT THE RED VANTA  //  STAY AT THE PIER  //  %02d REMAINING"), State->HostileCount);
            break;
        case EBMMissionPhase::Complete:
        {
            const bool bHasNextCampaignLevel = State &&
                static_cast<int32>(State->CampaignLevel) < GetBMCampaignLevelCount() - 1;
            ActionLabel = bHasNextCampaignLevel
                ? TEXT("ACTION  LEVEL COMPLETE  //  PRESS ENTER FOR NEXT LEVEL")
                : TEXT("ACTION  MISSION COMPLETE  //  PRESS ENTER TO RETURN TO MENU");
            break;
        }
        default:
            break;
        }
    }
    // The phase card is the immediate tutorial. A second copy of the same
    // instruction beneath it makes the first encounter look like a debug
    // screen, so let the persistent strip take over after the card fades.
    if (!ActionLabel.IsEmpty() && MissionCardTimeRemaining <= 0.0f)
    {
        if (bCombatAction)
        {
            FCanvasTileItem CombatPanel(
                FVector2D(36.0f, Canvas->ClipY - 248.0f),
                FVector2D(FMath::Min(Canvas->ClipX * 0.72f, 980.0f), 42.0f),
                FLinearColor(0.18f, 0.015f, 0.008f, 0.86f));
            CombatPanel.BlendMode = SE_BLEND_Translucent;
            Canvas->DrawItem(CombatPanel);
        }
        DrawTextLine(
            ActionLabel,
            52.0f,
            Canvas->ClipY - 238.0f,
            bCombatAction ? 0.92f : 0.76f,
            bCombatAction ? FLinearColor(1.0f, 0.48f, 0.22f) : FLinearColor(0.98f, 0.78f, 0.30f));
    }

    if (!State->bSignalControlMode)
    {
        DrawTextLine(
            TEXT("WASD MOVE   MOUSE LOOK   LMB FIRE   RMB AIM"),
            48.0f,
            Canvas->ClipY - 184.0f,
            0.74f,
            FLinearColor(0.48f, 0.9f, 0.82f));
        DrawTextLine(
            TEXT("G FRAG   X VEIL   R RELOAD   E INTERACT   SHIFT SPRINT   H HELP"),
            48.0f,
            Canvas->ClipY - 154.0f,
            0.72f,
            FLinearColor(0.48f, 0.9f, 0.82f));
    }

    if (bScoreboardVisible)
    {
        FString ScoreboardBody = FString::Printf(
            TEXT("LATTICE  %04d\nVANTA   %04d\nACTIVE NODE  %d\nROTATES IN  %02d\n"),
            State->SignalControl.LatticeScore,
            State->SignalControl.VantaScore,
            State->SignalControl.ActiveNodeIndex + 1,
            FMath::CeilToInt(State->SignalControl.SecondsUntilRotation));
        for (APlayerState* ExistingState : State->PlayerArray)
        {
            const ABMPlayerState* PlayerState = Cast<ABMPlayerState>(ExistingState);
            if (!PlayerState)
            {
                continue;
            }
            const TCHAR* TeamLabel = PlayerState->Team == EBMTeam::Lattice ? TEXT("L") : (PlayerState->Team == EBMTeam::Vanta ? TEXT("V") : TEXT("-"));
            const FString PlayerName = PlayerState->GetPlayerName().IsEmpty() ? TEXT("BOT") : PlayerState->GetPlayerName();
            ScoreboardBody += FString::Printf(
                TEXT("\n%s  %-12s  K %02d  D %02d  OBJ %03d  %03dms"),
                TeamLabel,
                *PlayerName.Left(12),
                PlayerState->Kills,
                PlayerState->Deaths,
                PlayerState->ObjectiveScore,
                FMath::RoundToInt(PlayerState->SmoothedLatencyMs > 0.0f ? PlayerState->SmoothedLatencyMs : PlayerState->GetPingInMilliseconds()));
        }
        ScoreboardBody += TEXT("\n\nTAB  CLOSE");
        DrawMenuOverlay(TEXT("SIGNAL CONTROL // SCOREBOARD"), ScoreboardBody);
    }
}

void ABMHUD::DrawMissionFlow(const ABMGameState* State)
{
    if (!Canvas || !State || State->bSignalControlMode)
    {
        return;
    }

    int32 ActiveStep = 1;
    switch (State->MissionPhase)
    {
    case EBMMissionPhase::Checkpoint:
    case EBMMissionPhase::OpenCombat:
        ActiveStep = 2;
        break;
    case EBMMissionPhase::InteriorBreach:
    case EBMMissionPhase::Intelligence:
        ActiveStep = 3;
        break;
    case EBMMissionPhase::Extraction:
        ActiveStep = 4;
        break;
    case EBMMissionPhase::FinalDefense:
        ActiveStep = 5;
        break;
    case EBMMissionPhase::Complete:
        ActiveStep = 5;
        break;
    default:
        break;
    }

    const float ObjectivePanelWidth = FMath::Min(560.0f, Canvas->ClipX * 0.44f);
    const float ObjectivePanelRight = 28.0f + ObjectivePanelWidth;
    const float PanelLeft = ObjectivePanelRight + 18.0f;
    const float PanelWidth = FMath::Max(300.0f, FMath::Min(760.0f, Canvas->ClipX - PanelLeft - 28.0f));
    const float PanelTop = 68.0f;
    FCanvasTileItem FlowPanel(
        FVector2D(PanelLeft, PanelTop),
        FVector2D(PanelWidth, 72.0f),
        FLinearColor(0.005f, 0.012f, 0.018f, 0.76f));
    FlowPanel.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(FlowPanel);

    DrawTextLine(TEXT("MISSION FLOW"), PanelLeft + 12.0f, PanelTop + 10.0f, 0.56f, FLinearColor(0.54f, 0.72f, 0.70f));

    static const TCHAR* StepLabels[] = {TEXT("MOVE"), TEXT("CLEAR"), TEXT("BREACH"), TEXT("EXTRACT"), TEXT("DEFEND")};
    const float StepLeft = PanelLeft + 108.0f;
    const float StepWidth = FMath::Max(58.0f, (PanelWidth - 118.0f) / 5.0f);
    for (int32 Index = 0; Index < 5; ++Index)
    {
        const int32 StepNumber = Index + 1;
        const FLinearColor StepColor = StepNumber == ActiveStep
            ? FLinearColor(1.0f, 0.72f, 0.24f)
            : (StepNumber < ActiveStep
                ? FLinearColor(0.34f, 0.92f, 0.78f)
                : FLinearColor(0.48f, 0.54f, 0.54f));
        const FString Label = FString::Printf(TEXT("%d %s"), StepNumber, StepLabels[Index]);
        DrawTextLine(Label, StepLeft + Index * StepWidth, PanelTop + 10.0f, StepNumber == ActiveStep ? 0.70f : 0.62f, StepColor);
    }

    // The route is intentionally expressed in both shape and words. The
    // generated greybox cannot rely on art-language alone, and a new player
    // should not have to remember what each beacon colour means.
    DrawTextLine(TEXT("COLOUR KEY"), PanelLeft + 12.0f, PanelTop + 42.0f, 0.54f, FLinearColor(0.54f, 0.72f, 0.70f));
    DrawTextLine(TEXT("TEAL  MOVE"), PanelLeft + 88.0f, PanelTop + 42.0f, 0.56f, FLinearColor(0.25f, 1.0f, 0.88f));
    DrawTextLine(TEXT("AMBER  FIGHT"), PanelLeft + 188.0f, PanelTop + 42.0f, 0.56f, FLinearColor(1.0f, 0.72f, 0.20f));
    DrawTextLine(TEXT("ORANGE  HOLD E"), PanelLeft + 306.0f, PanelTop + 42.0f, 0.56f, FLinearColor(1.0f, 0.42f, 0.12f));
    DrawTextLine(TEXT("YELLOW  EXTRACT"), PanelLeft + 450.0f, PanelTop + 42.0f, 0.56f, FLinearColor(1.0f, 0.92f, 0.30f));
}

void ABMHUD::DrawObjectiveMarker(const ABMPlayerCharacter* Player, const ABMGameState* State)
{
    if (!Canvas || !Player || !State || !State->CurrentObjective.bHasWorldLocation)
    {
        return;
    }

    // A checkpoint and the final pier are hold-and-clear spaces, not waypoints.
    // Showing a navigation marker while enemies are active can tell a player
    // to run away from the fight, especially after they overshoot the beacon.
    if (State->MissionPhase == EBMMissionPhase::Checkpoint
        || State->MissionPhase == EBMMissionPhase::OpenCombat
        || State->MissionPhase == EBMMissionPhase::FinalDefense)
    {
        // The phase card is the first, centered combat instruction. Do not
        // stack another panel beneath it while the card is still readable.
        if (MissionCardTimeRemaining > 0.0f)
        {
            return;
        }
        const bool bCheckpoint = State->MissionPhase == EBMMissionPhase::Checkpoint;
        const bool bSeawall = State->MissionPhase == EBMMissionPhase::OpenCombat;
        const FString AreaLabel = bCheckpoint
            ? TEXT("COMBAT AREA  //  AMBER CHECKPOINT")
            : (bSeawall ? TEXT("COMBAT AREA  //  SEAWALL ARENA") : TEXT("DEFEND AREA  //  EXTRACTION PIER"));
        const FString StayLabel = bCheckpoint
            ? TEXT("STAY HERE UNTIL THE RED VANTA ARE CLEAR")
            : (bSeawall ? TEXT("STAY IN THE ARENA UNTIL THE WAVE IS CLEAR") : TEXT("STAY HERE UNTIL THE HUD SAYS CLEAR"));
        FCanvasTileItem AreaPanel(
            FVector2D(Canvas->ClipX * 0.5f - 245.0f, 158.0f),
            FVector2D(490.0f, 54.0f),
            FLinearColor(0.20f, 0.018f, 0.008f, 0.72f));
        AreaPanel.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(AreaPanel);
        DrawTextLine(AreaLabel, Canvas->ClipX * 0.5f - 220.0f, 166.0f, 0.88f, FLinearColor(1.0f, 0.48f, 0.22f));
        DrawTextLine(StayLabel, Canvas->ClipX * 0.5f - 220.0f, 192.0f, 0.58f, FLinearColor(1.0f, 0.75f, 0.55f));
        return;
    }

    const FVector Target = State->CurrentObjective.WorldLocation;
    const FVector ToTarget = Target - Player->GetActorLocation();
    const int32 DistanceMeters = FMath::Max(0, FMath::RoundToInt(ToTarget.Size2D() / 100.0f));
    if (DistanceMeters <= 3)
    {
        DrawTextLine(TEXT("OBJECTIVE REACHED"), Canvas->ClipX * 0.5f - 90.0f, 116.0f, 0.85f, FLinearColor(0.45f, 1.0f, 0.75f));
        return;
    }

    FVector2D ScreenPosition;
    APlayerController* Controller = Cast<APlayerController>(PlayerOwner);
    const bool bProjected = Controller
        && Controller->ProjectWorldLocationToScreen(Target + FVector(0.0f, 0.0f, 125.0f), ScreenPosition, true)
        && ScreenPosition.X > 40.0f
        && ScreenPosition.X < Canvas->ClipX - 40.0f
        && ScreenPosition.Y > 110.0f
        && ScreenPosition.Y < Canvas->ClipY - 185.0f;

    if (bProjected)
    {
        DrawTextLine(
            FString::Printf(TEXT("◆  GO HERE  %03dm"), DistanceMeters),
            ScreenPosition.X - 110.0f,
            ScreenPosition.Y - 30.0f,
            0.94f,
            FLinearColor(0.45f, 1.0f, 0.78f));
        DrawTextLine(TEXT("FOLLOW THE COLOURED BEACON"), ScreenPosition.X - 112.0f, ScreenPosition.Y + 2.0f, 0.58f, FLinearColor(0.45f, 0.82f, 0.74f));
        return;
    }

    const FRotator ViewRotation = Player->GetControlRotation();
    const FRotator YawRotation(0.0f, ViewRotation.Yaw, 0.0f);
    const FVector ViewForward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    const FVector ViewRight = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
    const FVector FlatDirection = ToTarget.GetSafeNormal2D();
    const float ForwardDot = FVector::DotProduct(FlatDirection, ViewForward);
    const float RightDot = FVector::DotProduct(FlatDirection, ViewRight);

    FString Direction;
    if (ForwardDot > 0.72f)
    {
        Direction = TEXT("AHEAD");
    }
    else if (ForwardDot < -0.72f)
    {
        Direction = TEXT("BEHIND");
    }
    else if (RightDot > 0.0f)
    {
        Direction = ForwardDot > 0.0f ? TEXT("AHEAD RIGHT") : TEXT("RIGHT");
    }
    else
    {
        Direction = ForwardDot > 0.0f ? TEXT("AHEAD LEFT") : TEXT("LEFT");
    }
    DrawTextLine(
        FString::Printf(TEXT("◆  GO HERE  %03dm  //  %s"), DistanceMeters, *Direction),
        Canvas->ClipX * 0.5f - 145.0f,
        156.0f,
        0.94f,
        FLinearColor(0.45f, 1.0f, 0.78f));
    DrawTextLine(
        Direction == TEXT("AHEAD") ? TEXT("KEEP MOVING FORWARD") : TEXT("TURN UNTIL THE MARKER IS AHEAD"),
        Canvas->ClipX * 0.5f - 132.0f,
        188.0f,
        0.58f,
        FLinearColor(0.45f, 0.82f, 0.74f));
}

void ABMHUD::DrawHostileMarkers(const ABMPlayerCharacter* Player, const ABMGameState* State)
{
    if (!Canvas || !Player || !State || State->bSignalControlMode || State->HostileCount <= 0 || MissionCardTimeRemaining > 0.0f)
    {
        return;
    }

    const bool bCombatPhase = State->MissionPhase == EBMMissionPhase::Checkpoint
        || State->MissionPhase == EBMMissionPhase::OpenCombat
        || State->MissionPhase == EBMMissionPhase::FinalDefense;
    if (!bCombatPhase)
    {
        return;
    }

    APlayerController* Controller = Cast<APlayerController>(PlayerOwner);
    if (!Controller || !GetWorld())
    {
        return;
    }

    const FVector ViewLocation = Player->GetFirstPersonCamera()
        ? Player->GetFirstPersonCamera()->GetComponentLocation()
        : Player->GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
    const FRotator ViewRotation = Player->GetControlRotation();
    const FRotator YawRotation(0.0f, ViewRotation.Yaw, 0.0f);
    const FVector ViewForward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    const FVector ViewRight = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    const ABMEnemyCharacter* NearestEnemy = nullptr;
    float NearestDistanceSquared = TNumericLimits<float>::Max();
    int32 OnScreenMarkers = 0;
    for (TActorIterator<ABMEnemyCharacter> Iterator(GetWorld()); Iterator; ++Iterator)
    {
        const ABMEnemyCharacter* Enemy = *Iterator;
        if (!IsValid(Enemy) || Enemy->IsEliminated())
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared2D(ViewLocation, Enemy->GetActorLocation());
        if (DistanceSquared < NearestDistanceSquared)
        {
            NearestDistanceSquared = DistanceSquared;
            NearestEnemy = Enemy;
        }

        FVector2D ScreenPosition;
        const bool bProjected = Controller->ProjectWorldLocationToScreen(
            Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 116.0f),
            ScreenPosition,
            true);
        const bool bReadablePosition = bProjected
            && ScreenPosition.X > 64.0f
            && ScreenPosition.X < Canvas->ClipX - 64.0f
            && ScreenPosition.Y > 150.0f
            && ScreenPosition.Y < Canvas->ClipY - 210.0f;
        if (!bReadablePosition || OnScreenMarkers >= 5)
        {
            continue;
        }

        const int32 DistanceMeters = FMath::Max(1, FMath::RoundToInt(FMath::Sqrt(DistanceSquared) / 100.0f));
        const FLinearColor RoleColor = ApplyAccessibilityColor(Enemy->GetEnemyRoleColor());
        const FString MarkerLabel = FString::Printf(
            TEXT("%s %s  %02dm"),
            *Enemy->GetEnemyRoleMarker(),
            *Enemy->GetEnemyRoleLabel(),
            DistanceMeters);
        const float MarkerWidth = 48.0f + static_cast<float>(MarkerLabel.Len()) * 3.5f;
        DrawTextLine(
            MarkerLabel,
            ScreenPosition.X - MarkerWidth * 0.5f,
            ScreenPosition.Y - 24.0f,
            0.82f,
            FLinearColor(RoleColor.R, RoleColor.G, RoleColor.B, 0.96f));
        ++OnScreenMarkers;
    }

    if (!NearestEnemy || OnScreenMarkers > 0)
    {
        return;
    }

    const FVector ToEnemy = (NearestEnemy->GetActorLocation() - ViewLocation).GetSafeNormal2D();
    const float ForwardDot = FVector::DotProduct(ToEnemy, ViewForward);
    const float RightDot = FVector::DotProduct(ToEnemy, ViewRight);
    const FString Direction = ForwardDot > 0.72f
        ? TEXT("AHEAD")
        : (ForwardDot < -0.72f
            ? TEXT("BEHIND")
            : (RightDot > 0.0f ? TEXT("RIGHT") : TEXT("LEFT")));
    FCanvasTileItem ContactPanel(
        FVector2D(Canvas->ClipX * 0.5f - 250.0f, 228.0f),
        FVector2D(500.0f, 62.0f),
        FLinearColor(0.24f, 0.02f, 0.01f, 0.82f));
    ContactPanel.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(ContactPanel);
    DrawTextLine(
        FString::Printf(TEXT("CONTACT  //  TURN %s"), *Direction),
        Canvas->ClipX * 0.5f - 172.0f,
        236.0f,
        0.98f,
        FLinearColor(1.0f, 0.28f, 0.18f, 1.0f));
    DrawTextLine(
        TEXT("▲ ASSAULT  ■ RIFLEMAN  ✦ MARKSMAN  //  AIM THEN LMB FIRE"),
        Canvas->ClipX * 0.5f - 165.0f,
        266.0f,
        0.64f,
        FLinearColor(1.0f, 0.78f, 0.62f, 1.0f));
}

void ABMHUD::DrawMenuOverlay(const FString& Title, const FString& Body)
{
    const float Left = Canvas->ClipX * 0.12f;
    const float Top = Canvas->ClipY * 0.15f;
    TArray<FString> Lines;
    Body.ParseIntoArray(Lines, TEXT("\n"), false);
    // Keep every menu row inside the panel at small laptop/720p viewports.
    // The fixed 31 px rhythm used to push the lower controls/settings rows
    // below the window, making the available actions look missing.
    const float MaxPanelHeight = FMath::Min(Canvas->ClipY * 0.78f, Canvas->ClipY - Top + 28.0f);
    const float AvailableBodyHeight = FMath::Max(120.0f, MaxPanelHeight - 116.0f);
    const float BodyLineHeight = Lines.Num() > 0
        ? FMath::Clamp(AvailableBodyHeight / static_cast<float>(Lines.Num()), 20.0f, 31.0f)
        : 31.0f;
    const float BodyScale = FMath::Clamp(BodyLineHeight / 31.0f, 0.64f, 1.0f);
    const float PanelHeight = FMath::Min(116.0f + Lines.Num() * BodyLineHeight, MaxPanelHeight);
    FCanvasTileItem PanelItem(
        FVector2D(Left - 34.0f, Top - 28.0f),
        FVector2D(
            FMath::Min(Canvas->ClipX * 0.74f, 1120.0f),
            PanelHeight),
        FLinearColor(0.005f, 0.012f, 0.018f, 0.82f));
    PanelItem.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(PanelItem);
    FCanvasTileItem Accent(
        FVector2D(Left - 34.0f, Top - 28.0f),
        FVector2D(4.0f, PanelHeight),
        FLinearColor(0.18f, 0.82f, 0.76f, 0.92f));
    Accent.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(Accent);
    DrawTextLine(Title, Left, Top, 2.0f, FLinearColor(0.4f, 0.95f, 0.84f));
    for (int32 Index = 0; Index < Lines.Num(); ++Index)
    {
        DrawTextLine(
            Lines[Index],
            Left,
            Top + 72.0f + Index * BodyLineHeight,
            BodyScale,
            FLinearColor(0.86f, 0.88f, 0.84f));
    }
}

void ABMHUD::DrawSignalLobby(const ABMGameState* State)
{
    if (!Canvas || !State)
    {
        return;
    }

    const float Left = Canvas->ClipX * 0.10f;
    const float Top = Canvas->ClipY * 0.10f;
    const float PanelWidth = FMath::Min(Canvas->ClipX * 0.80f, 1180.0f);
    const float PanelHeight = FMath::Min(Canvas->ClipY * 0.80f, 620.0f);
    FCanvasTileItem Panel(
        FVector2D(Left - 34.0f, Top - 28.0f),
        FVector2D(PanelWidth, PanelHeight),
        FLinearColor(0.005f, 0.012f, 0.018f, 0.88f));
    Panel.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(Panel);

    DrawTextLine(TEXT("SIGNAL CONTROL // LOBBY"), Left, Top, 1.85f, FLinearColor(0.4f, 0.95f, 0.84f));
    DrawTextLine(
        FString::Printf(
            TEXT("ROSTER  %02d / %02d    HUMANS  %02d    BOTS  %02d"),
            State->SignalLobbyMembers.Num(),
            FMath::Max(2, State->SignalLobbyCapacity),
            State->SignalLobbyHumanCount,
            State->SignalLobbyBotCount),
        Left,
        Top + 48.0f,
        0.72f,
        FLinearColor(0.62f, 0.78f, 0.74f));

    const float ColumnGap = PanelWidth * 0.50f;
    const float Right = Left + ColumnGap;
    const float HeaderY = Top + 98.0f;
    const float RowStartY = HeaderY + 48.0f;
    const float RowStep = 38.0f;
    const FLinearColor LatticeColor(0.36f, 0.88f, 1.0f);
    const FLinearColor VantaColor(1.0f, 0.42f, 0.30f);
    int32 LatticeCount = 0;
    int32 VantaCount = 0;
    for (const FBMSignalLobbyMember& Member : State->SignalLobbyMembers)
    {
        if (Member.Team == EBMTeam::Lattice)
        {
            ++LatticeCount;
        }
        else if (Member.Team == EBMTeam::Vanta)
        {
            ++VantaCount;
        }
    }

    DrawTextLine(
        FString::Printf(TEXT("LATTICE RESPONSE UNIT  %02d"), LatticeCount),
        Left,
        HeaderY,
        0.86f,
        LatticeColor);
    DrawTextLine(
        FString::Printf(TEXT("VANTA CONTINUUM  %02d"), VantaCount),
        Right,
        HeaderY,
        0.86f,
        VantaColor);

    int32 LatticeRow = 0;
    int32 VantaRow = 0;
    for (const FBMSignalLobbyMember& Member : State->SignalLobbyMembers)
    {
        if (Member.Team != EBMTeam::Lattice && Member.Team != EBMTeam::Vanta)
        {
            continue;
        }

        const bool bLattice = Member.Team == EBMTeam::Lattice;
        int32* Row = bLattice ? &LatticeRow : &VantaRow;
        const float X = bLattice ? Left : Right;
        const FLinearColor TeamColor = bLattice ? LatticeColor : VantaColor;
        if (*Row < 6)
        {
            const FString Status = Member.bIsHost ? TEXT("HOST") : (Member.bIsBot ? TEXT("BOT") : TEXT("READY"));
            DrawTextLine(Status, X, RowStartY + *Row * RowStep, 0.58f, Member.bIsHost ? FLinearColor(1.0f, 0.82f, 0.34f) : TeamColor);
            DrawTextLine(Member.DisplayName.Left(20), X + 62.0f, RowStartY + *Row * RowStep, 0.78f, FLinearColor(0.86f, 0.88f, 0.84f));
        }
        ++(*Row);
    }

    if (LatticeRow == 0)
    {
        DrawTextLine(TEXT("NO OPERATORS ASSIGNED"), Left, RowStartY, 0.68f, FLinearColor(0.48f, 0.58f, 0.56f));
    }
    if (VantaRow == 0)
    {
        DrawTextLine(TEXT("NO OPERATORS ASSIGNED"), Right, RowStartY, 0.68f, FLinearColor(0.48f, 0.58f, 0.56f));
    }

    const float FooterY = Top + PanelHeight - 84.0f;
    if (State->bMatchLive)
    {
        DrawTextLine(TEXT("MATCH LIVE  //  DEPLOYING"), Left, FooterY, 0.88f, FLinearColor(0.35f, 1.0f, 0.84f));
    }
    else if (State->MatchCountdown > 0.0f)
    {
        DrawTextLine(
            FString::Printf(TEXT("MATCH STARTING  //  %02d"), FMath::CeilToInt(State->MatchCountdown)),
            Left,
            FooterY,
            0.88f,
            FLinearColor(1.0f, 0.78f, 0.32f));
    }
    else
    {
        DrawTextLine(TEXT("ENTER  START LOCAL MATCH"), Left, FooterY, 0.88f, FLinearColor(0.86f, 0.88f, 0.84f));
    }
    DrawTextLine(TEXT("ESC  BACK"), Right, FooterY, 0.88f, FLinearColor(0.62f, 0.7f, 0.68f));
}

void ABMHUD::DrawTextLine(const FString& Text, const float X, const float Y, const float Scale, const FLinearColor& Color) const
{
    if (!Canvas)
    {
        return;
    }
    FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), GEngine->GetMediumFont(), ApplyAccessibilityColor(Color));
    Item.Scale = FVector2D(Scale, Scale);
    Item.EnableShadow(FColor::Black);
    Canvas->DrawItem(Item);
}

FLinearColor ABMHUD::ApplyAccessibilityColor(const FLinearColor& Color) const
{
    const UBMGameUserSettings* Settings = GEngine ? Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
    if (!Settings)
    {
        return Color;
    }

    switch (Settings->ColorBlindPreset)
    {
    case 1: // red/green separation through a blue shift
        return FLinearColor(Color.R, Color.G * 0.55f + Color.B * 0.1f, FMath::Clamp(Color.B + Color.G * 0.35f, 0.0f, 1.0f), Color.A);
    case 2: // blue/yellow separation through a red shift
        return FLinearColor(FMath::Clamp(Color.R + Color.B * 0.3f, 0.0f, 1.0f), Color.G * 0.8f, Color.B * 0.55f, Color.A);
    case 3: // high-contrast monochrome fallback
    {
        const float Luminance = Color.R * 0.22f + Color.G * 0.7f + Color.B * 0.08f;
        return FLinearColor(Luminance, Luminance, Luminance, Color.A);
    }
    default:
        return Color;
    }
}

float ABMHUD::GetSubtitleScale() const
{
    const UBMGameUserSettings* Settings = GEngine ? Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
    if (!Settings)
    {
        return 1.15f;
    }
    return Settings->SubtitleSize == 0 ? 0.95f : (Settings->SubtitleSize == 1 ? 1.15f : 1.45f);
}

FString ABMHUD::GetMissionPhaseLabel() const
{
    if (const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr)
    {
        switch (State->MissionPhase)
        {
        case EBMMissionPhase::Intro: return TEXT("BRIEFING");
        case EBMMissionPhase::Insertion: return TEXT("INSERTION");
        case EBMMissionPhase::QuietApproach: return TEXT("QUIET APPROACH");
        case EBMMissionPhase::Checkpoint: return TEXT("CHECKPOINT");
        case EBMMissionPhase::InteriorBreach: return TEXT("INTERIOR BREACH");
        case EBMMissionPhase::Intelligence: return TEXT("INTELLIGENCE");
        case EBMMissionPhase::OpenCombat: return TEXT("OPEN COMBAT");
        case EBMMissionPhase::Extraction: return TEXT("EXTRACTION");
        case EBMMissionPhase::FinalDefense: return TEXT("FINAL DEFENSE");
        case EBMMissionPhase::Complete: return TEXT("COMPLETE");
        case EBMMissionPhase::Failed: return TEXT("FAILED");
        default: break;
        }
    }
    return TEXT("MISSION");
}
