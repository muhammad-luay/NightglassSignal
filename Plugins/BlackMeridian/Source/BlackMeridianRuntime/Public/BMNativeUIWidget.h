// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BMHUD.h"
#include "BMNativeUIWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UCanvasPanelSlot;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UWidget;

/**
 * Runtime-authored UMG surface for the Black Meridian front-end and gameplay
 * presentation.  The widget deliberately reads the existing HUD/controller
 * state instead of owning mission or network logic, so it can be replaced by
 * authored Widget Blueprints without changing gameplay behavior.
 */
UCLASS()
class BLACKMERIDIANRUNTIME_API UBMNativeUIWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void RefreshFromHUD(const ABMHUD* HUD);

    /**
     * Moves the active front-end action between the visible runtime buttons.
     * The controller owns the input policy; this widget owns the focus
     * presentation and invokes the same actions as a mouse click.
     */
    void MoveMenuSelection(int32 Direction);
    bool ActivateMenuSelection();

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UFUNCTION()
    void HandlePrimaryClicked();

    UFUNCTION()
    void HandleSecondaryClicked();

    UFUNCTION()
    void HandleTertiaryClicked();

    UFUNCTION()
    void HandleQuaternaryClicked();

private:
    void BuildWidgetTree();
    void RefreshPresentation();
    void RefreshMenu(const ABMHUD* HUD);
    void RefreshGameplay(const ABMHUD* HUD);
    void RefreshTargeting(const ABMHUD* HUD);
    void RefreshObjectiveMarker(const ABMHUD* HUD, const ABMGameState* State, const ABMPlayerCharacter* Player);
    void RefreshScoreboard(const ABMHUD* HUD);
    void ConfigureMenuActions(EBMHUDScreen Screen);
    void InvokeMenuAction(int32 ActionIndex);
    void RefreshMenuSelectionPresentation();

    UTextBlock* MakeTextBlock(const FText& Text, int32 FontSize, const FLinearColor& Color) const;
    UButton* MakeActionButton(const FText& Label, UTextBlock*& OutLabel) const;
    void SetText(UTextBlock* TextBlock, const FText& Text) const;
    void SetVisible(UWidget* Widget, bool bVisible) const;

    UPROPERTY(Transient)
    TObjectPtr<USizeBox> RootSizeBox;

    UPROPERTY(Transient)
    TObjectPtr<UCanvasPanel> RootCanvas;

    UPROPERTY(Transient)
    TObjectPtr<UCanvasPanel> GameplayLayer;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> DamageFlashBorder;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> MenuPanel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> MenuKickerText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> MenuTitleText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> MenuBodyText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> MenuFooterText;

    UPROPERTY(Transient)
    TObjectPtr<UButton> PrimaryButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> SecondaryButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> TertiaryButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> QuaternaryButton;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> PrimaryButtonLabel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SecondaryButtonLabel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TertiaryButtonLabel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> QuaternaryButtonLabel;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> ObjectivePanel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ObjectiveContextText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ObjectiveTitleText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ObjectiveHintText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> MissionFlowText;

    /** Native persistent combat log; the legacy canvas is suppressed in packaged play. */
    UPROPERTY(Transient)
    TObjectPtr<UBorder> KillFeedPanel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> KillFeedHeaderText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> KillFeedText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> CompassText;

    /** Compact live-target readout; appears only for a clear-line hostile. */
    UPROPERTY(Transient)
    TObjectPtr<UBorder> TargetPanel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TargetContextText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TargetNameText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TargetHealthText;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> TargetHealthBar;

    /** Native replacement for the legacy canvas waypoint pointer. */
    UPROPERTY(Transient)
    TObjectPtr<UBorder> ObjectiveMarkerPanel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ObjectiveMarkerTitleText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ObjectiveMarkerDirectionText;

    UPROPERTY(Transient)
    TObjectPtr<UCanvasPanelSlot> ObjectiveMarkerSlot;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> HealthPanel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> HealthText;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> HealthBar;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> WeaponPanel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> WeaponNameText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> AmmoText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ReloadText;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> ReloadBar;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> OrdnanceText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ActionText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> InteractionText;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> InteractionBar;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> MissionCardPanel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> MissionCardTitleText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> MissionCardBodyText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SubtitleText;

    /** Native replacement for the legacy transient hit marker. */
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> HitMarkerSymbolText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> HitMarkerHeadshotText;

    /** Native replacement for the legacy elimination confirmation banner. */
    UPROPERTY(Transient)
    TObjectPtr<UBorder> EliminationPanel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> EliminationLabelText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> EliminationTotalText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ScoreboardText;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> ScoreboardPanel;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UBorder>> CrosshairBars;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UBorder>> CrosshairShadowBars;

    float RefreshAccumulator = 0.0f;
    EBMHUDScreen LastScreen = EBMHUDScreen::Splash;
    bool bHasPresentedScreen = false;
    int32 PrimaryAction = INDEX_NONE;
    int32 SecondaryAction = INDEX_NONE;
    int32 TertiaryAction = INDEX_NONE;
    int32 QuaternaryAction = INDEX_NONE;
    int32 FocusedMenuButton = INDEX_NONE;
};
