// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMNativeUIWidget.h"

#include "BMCampaignLevel.h"
#include "BMCombatCharacter.h"
#include "BMCombatTargeting.h"
#include "BMEnemyCharacter.h"
#include "BMGameState.h"
#include "BMGameUserSettings.h"
#include "BMHealthComponent.h"
#include "BMPlayerCharacter.h"
#include "BMPlayerController.h"
#include "BMPlayerState.h"
#include "BMWeapon.h"
#include "BMWeaponDataAsset.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
    const FLinearColor BackgroundColor(0.003f, 0.009f, 0.014f, 0.94f);
    const FLinearColor PanelColor(0.006f, 0.016f, 0.023f, 0.90f);
    const FLinearColor PanelSoftColor(0.005f, 0.014f, 0.020f, 0.72f);
    const FLinearColor BorderColor(0.12f, 0.40f, 0.40f, 0.92f);
    const FLinearColor PrimaryTextColor(0.88f, 0.92f, 0.89f, 1.0f);
    const FLinearColor MutedTextColor(0.52f, 0.69f, 0.67f, 1.0f);
    const FLinearColor TealColor(0.30f, 0.96f, 0.82f, 1.0f);
    const FLinearColor AmberColor(1.0f, 0.70f, 0.22f, 1.0f);
    const FLinearColor DangerColor(1.0f, 0.28f, 0.18f, 1.0f);
    const FLinearColor VantaColor(1.0f, 0.42f, 0.30f, 1.0f);

    void ApplyTextStyle(UTextBlock* Text, const int32 FontSize, const FLinearColor& Color)
    {
        if (!Text)
        {
            return;
        }

        FSlateFontInfo Font = Text->GetFont();
        Font.Size = FontSize;
        Text->SetFont(Font);
        Text->SetColorAndOpacity(Color);
        Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.76f));
        Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
    }

    UCanvasPanelSlot* FillCanvas(UCanvasPanel* Canvas, UWidget* Child)
    {
        if (!Canvas || !Child)
        {
            return nullptr;
        }

        UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Child);
        if (Slot)
        {
            Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
            Slot->SetOffsets(FMargin(0.0f));
        }
        return Slot;
    }

    UCanvasPanelSlot* PlaceCanvas(
        UCanvasPanel* Canvas,
        UWidget* Child,
        const FAnchors& Anchors,
        const FVector2D& Alignment,
        const FVector2D& Position,
        const FVector2D& Size)
    {
        if (!Canvas || !Child)
        {
            return nullptr;
        }

        UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Child);
        if (Slot)
        {
            Slot->SetAnchors(Anchors);
            Slot->SetAlignment(Alignment);
            Slot->SetPosition(Position);
            Slot->SetSize(Size);
        }
        return Slot;
    }

    UVerticalBoxSlot* AddTextToColumn(UVerticalBox* Column, UTextBlock* Text, const FMargin& Padding = FMargin(0.0f))
    {
        if (!Column || !Text)
        {
            return nullptr;
        }

        UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Text);
        if (Slot)
        {
            Slot->SetPadding(Padding);
        }
        return Slot;
    }

    UVerticalBoxSlot* AddWidgetToColumn(UVerticalBox* Column, UWidget* Widget, const FMargin& Padding = FMargin(0.0f))
    {
        if (!Column || !Widget)
        {
            return nullptr;
        }

        UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Widget);
        if (Slot)
        {
            Slot->SetPadding(Padding);
        }
        return Slot;
    }

    FString MissionPhaseLabel(const EBMMissionPhase Phase)
    {
        switch (Phase)
        {
        case EBMMissionPhase::Intro: return TEXT("INTRO");
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
        default: return TEXT("MISSION");
        }
    }

    FString CampaignLevelList(const EBMCampaignLevel SelectedLevel)
    {
        return GetBMCampaignLevelListText(SelectedLevel);
    }
}

TSharedRef<SWidget> UBMNativeUIWidget::RebuildWidget()
{
    BuildWidgetTree();
    return Super::RebuildWidget();
}

void UBMNativeUIWidget::NativeConstruct()
{
    Super::NativeConstruct();
    // Menus are also navigable from a controller.  The player controller
    // still owns the action bindings, while this widget supplies the focus
    // state and click-equivalent activation for the runtime-created buttons.
    SetIsFocusable(true);
    BuildWidgetTree();
    RefreshPresentation();
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian UMG surface: constructed root=%s menu=%s gameplay=%s"),
        WidgetTree && WidgetTree->RootWidget ? TEXT("yes") : TEXT("no"),
        MenuPanel ? TEXT("yes") : TEXT("no"),
        GameplayLayer ? TEXT("yes") : TEXT("no"));
}

void UBMNativeUIWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshAccumulator += FMath::Max(0.0f, InDeltaTime);
    if (RefreshAccumulator < 0.08f)
    {
        // Keep the combat target/reticle path frame-rate responsive even
        // though the full gameplay surface is intentionally throttled. A
        // target can cross the screen centre between two full HUD refreshes.
        const ABMPlayerController* Controller = Cast<ABMPlayerController>(GetOwningPlayer());
        const ABMHUD* HUD = Controller ? Cast<ABMHUD>(Controller->GetHUD()) : nullptr;
        RefreshTargeting(HUD);
        return;
    }

    RefreshAccumulator = 0.0f;
    RefreshPresentation();
}

UTextBlock* UBMNativeUIWidget::MakeTextBlock(
    const FText& Text,
    const int32 FontSize,
    const FLinearColor& Color) const
{
    if (!WidgetTree)
    {
        return nullptr;
    }

    UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    if (!TextBlock)
    {
        return nullptr;
    }

    TextBlock->SetText(Text);
    TextBlock->SetAutoWrapText(true);
    ApplyTextStyle(TextBlock, FontSize, Color);
    return TextBlock;
}

UButton* UBMNativeUIWidget::MakeActionButton(const FText& Label, UTextBlock*& OutLabel) const
{
    OutLabel = nullptr;
    if (!WidgetTree)
    {
        return nullptr;
    }

    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    if (!Button)
    {
        return nullptr;
    }

    Button->SetBackgroundColor(FLinearColor(0.08f, 0.32f, 0.30f, 0.96f));
    Button->SetColorAndOpacity(FLinearColor::White);
    OutLabel = MakeTextBlock(Label, 15, FLinearColor::White);
    if (OutLabel)
    {
        OutLabel->SetJustification(ETextJustify::Center);
        Button->AddChild(OutLabel);
    }
    return Button;
}

void UBMNativeUIWidget::SetText(UTextBlock* TextBlock, const FText& Text) const
{
    if (TextBlock)
    {
        TextBlock->SetText(Text);
    }
}

void UBMNativeUIWidget::SetVisible(UWidget* Widget, const bool bVisible) const
{
    if (Widget)
    {
        Widget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UBMNativeUIWidget::BuildWidgetTree()
{
    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return;
    }

    RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    RootSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    RootSizeBox->SetWidthOverride(1440.0f);
    RootSizeBox->SetHeightOverride(900.0f);
    RootSizeBox->AddChild(RootCanvas);
    WidgetTree->RootWidget = RootSizeBox;

    UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    // Keep the world visible behind the gameplay HUD. Menu surfaces supply
    // their own opaque panel and are shown only while the world is paused.
    Backdrop->SetBrushColor(FLinearColor(BackgroundColor.R, BackgroundColor.G, BackgroundColor.B, 0.0f));
    FillCanvas(RootCanvas, Backdrop);

    GameplayLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    FillCanvas(RootCanvas, GameplayLayer);

    DamageFlashBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    DamageFlashBorder->SetBrushColor(FLinearColor(0.95f, 0.04f, 0.02f, 0.0f));
    FillCanvas(GameplayLayer, DamageFlashBorder);

    ObjectivePanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    ObjectivePanel->SetBrushColor(PanelSoftColor);
    // The gameplay layer is a sightline aid, not a briefing card. Keep the
    // persistent objective readable while leaving the upper-left quadrant
    // open for silhouettes, cover edges, and muzzle/impact feedback.
    ObjectivePanel->SetPadding(FMargin(14.0f, 10.0f, 14.0f, 9.0f));
    PlaceCanvas(GameplayLayer, ObjectivePanel, FAnchors(0.0f, 0.0f, 0.0f, 0.0f), FVector2D::ZeroVector, FVector2D(28.0f, 20.0f), FVector2D(560.0f, 96.0f));
    UVerticalBox* ObjectiveColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    ObjectivePanel->AddChild(ObjectiveColumn);
    ObjectiveContextText = MakeTextBlock(FText::GetEmpty(), 11, MutedTextColor);
    ObjectiveTitleText = MakeTextBlock(FText::GetEmpty(), 16, PrimaryTextColor);
    ObjectiveHintText = MakeTextBlock(FText::GetEmpty(), 11, TealColor);
    AddTextToColumn(ObjectiveColumn, ObjectiveContextText);
    AddTextToColumn(ObjectiveColumn, ObjectiveTitleText, FMargin(0.0f, 4.0f, 0.0f, 0.0f));
    AddTextToColumn(ObjectiveColumn, ObjectiveHintText, FMargin(0.0f, 4.0f, 0.0f, 0.0f));

    UBorder* FlowPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    FlowPanel->SetBrushColor(PanelSoftColor);
    FlowPanel->SetPadding(FMargin(12.0f, 8.0f));
    PlaceCanvas(GameplayLayer, FlowPanel, FAnchors(1.0f, 0.0f, 1.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(-28.0f, 20.0f), FVector2D(470.0f, 66.0f));
    MissionFlowText = MakeTextBlock(FText::GetEmpty(), 11, PrimaryTextColor);
    FlowPanel->AddChild(MissionFlowText);

    // The persistent combat log already records confirmed eliminations. This
    // transient toast is intentionally compact and low in the frame: it gives
    // immediate kill feedback without covering the target, reticle, or route.
    KillFeedPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    KillFeedPanel->SetBrushColor(FLinearColor(0.004f, 0.012f, 0.018f, 0.78f));
    KillFeedPanel->SetPadding(FMargin(12.0f, 8.0f));
    PlaceCanvas(
        GameplayLayer,
        KillFeedPanel,
        FAnchors(1.0f, 0.0f, 1.0f, 0.0f),
        FVector2D(1.0f, 0.0f),
        FVector2D(-28.0f, 174.0f),
        FVector2D(340.0f, 92.0f));
    UVerticalBox* KillFeedColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    KillFeedPanel->AddChild(KillFeedColumn);
    KillFeedHeaderText = MakeTextBlock(FText::FromString(TEXT("COMBAT LOG")), 9, MutedTextColor);
    KillFeedText = MakeTextBlock(FText::GetEmpty(), 10, AmberColor);
    AddTextToColumn(KillFeedColumn, KillFeedHeaderText);
    AddTextToColumn(KillFeedColumn, KillFeedText, FMargin(0.0f, 5.0f, 0.0f, 0.0f));
    SetVisible(KillFeedPanel, false);

    CompassText = MakeTextBlock(FText::GetEmpty(), 11, TealColor);
    CompassText->SetJustification(ETextJustify::Center);
    PlaceCanvas(GameplayLayer, CompassText, FAnchors(0.5f, 0.0f, 0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 26.0f), FVector2D(440.0f, 58.0f));

    TargetPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    TargetPanel->SetBrushColor(FLinearColor(0.03f, 0.008f, 0.006f, 0.84f));
    TargetPanel->SetPadding(FMargin(10.0f, 7.0f));
    // Keep target information in the right-hand information rail rather than
    // in the objective-marker band. The marker is allowed to move across the
    // upper middle of the screen as the player turns; pinning the target card
    // to that same band made the two panels overlap exactly when a hostile was
    // under the reticle, which is the moment the player needs both pieces of
    // information most.
    PlaceCanvas(
        GameplayLayer,
        TargetPanel,
        FAnchors(1.0f, 0.0f, 1.0f, 0.0f),
        FVector2D(1.0f, 0.0f),
        FVector2D(-28.0f, 96.0f),
        FVector2D(276.0f, 70.0f));
    UVerticalBox* TargetColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    TargetPanel->AddChild(TargetColumn);
    TargetContextText = MakeTextBlock(FText::GetEmpty(), 9, MutedTextColor);
    TargetNameText = MakeTextBlock(FText::GetEmpty(), 13, DangerColor);
    TargetHealthText = MakeTextBlock(FText::GetEmpty(), 9, PrimaryTextColor);
    AddTextToColumn(TargetColumn, TargetContextText);
    AddTextToColumn(TargetColumn, TargetNameText, FMargin(0.0f, 2.0f, 0.0f, 0.0f));
    AddTextToColumn(TargetColumn, TargetHealthText, FMargin(0.0f, 3.0f, 0.0f, 0.0f));
    TargetHealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    TargetHealthBar->SetFillColorAndOpacity(DangerColor);
    AddWidgetToColumn(TargetColumn, TargetHealthBar, FMargin(0.0f, 3.0f, 0.0f, 0.0f));
    SetVisible(TargetPanel, false);

    // The legacy canvas HUD is intentionally suppressed after this native
    // surface has valid geometry. Keep the route pointer on the same native
    // layer so the packaged game cannot lose the only directional cue when
    // the player clears the opening wave.
    ObjectiveMarkerPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    ObjectiveMarkerPanel->SetBrushColor(FLinearColor(0.12f, 0.035f, 0.006f, 0.76f));
    ObjectiveMarkerPanel->SetPadding(FMargin(12.0f, 8.0f));
    ObjectiveMarkerSlot = PlaceCanvas(
        GameplayLayer,
        ObjectiveMarkerPanel,
        FAnchors(0.0f, 0.0f, 0.0f, 0.0f),
        FVector2D(0.5f, 0.5f),
        FVector2D(720.0f, 122.0f),
        FVector2D(400.0f, 58.0f));
    UVerticalBox* ObjectiveMarkerColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    ObjectiveMarkerPanel->AddChild(ObjectiveMarkerColumn);
    ObjectiveMarkerTitleText = MakeTextBlock(FText::GetEmpty(), 13, FLinearColor(1.0f, 0.58f, 0.18f));
    ObjectiveMarkerDirectionText = MakeTextBlock(FText::GetEmpty(), 10, PrimaryTextColor);
    ObjectiveMarkerTitleText->SetJustification(ETextJustify::Center);
    ObjectiveMarkerDirectionText->SetJustification(ETextJustify::Center);
    AddTextToColumn(ObjectiveMarkerColumn, ObjectiveMarkerTitleText);
    AddTextToColumn(ObjectiveMarkerColumn, ObjectiveMarkerDirectionText, FMargin(0.0f, 4.0f, 0.0f, 0.0f));
    SetVisible(ObjectiveMarkerPanel, false);

    HealthPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    HealthPanel->SetBrushColor(PanelSoftColor);
    HealthPanel->SetPadding(FMargin(12.0f, 10.0f));
    PlaceCanvas(GameplayLayer, HealthPanel, FAnchors(0.0f, 1.0f, 0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(28.0f, -118.0f), FVector2D(290.0f, 84.0f));
    UVerticalBox* HealthColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    HealthPanel->AddChild(HealthColumn);
    HealthText = MakeTextBlock(FText::GetEmpty(), 16, TealColor);
    AddTextToColumn(HealthColumn, HealthText);
    HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    HealthBar->SetFillColorAndOpacity(TealColor);
    AddWidgetToColumn(HealthColumn, HealthBar, FMargin(0.0f, 6.0f, 0.0f, 0.0f));
    OrdnanceText = MakeTextBlock(FText::GetEmpty(), 10, MutedTextColor);
    AddTextToColumn(HealthColumn, OrdnanceText, FMargin(0.0f, 6.0f, 0.0f, 0.0f));

    WeaponPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    WeaponPanel->SetBrushColor(PanelSoftColor);
    WeaponPanel->SetPadding(FMargin(12.0f, 10.0f));
    PlaceCanvas(GameplayLayer, WeaponPanel, FAnchors(1.0f, 1.0f, 1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(-318.0f, -138.0f), FVector2D(290.0f, 104.0f));
    UVerticalBox* WeaponColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    WeaponPanel->AddChild(WeaponColumn);
    WeaponNameText = MakeTextBlock(FText::GetEmpty(), 15, PrimaryTextColor);
    AmmoText = MakeTextBlock(FText::GetEmpty(), 22, PrimaryTextColor);
    ReloadText = MakeTextBlock(FText::GetEmpty(), 10, AmberColor);
    AddTextToColumn(WeaponColumn, WeaponNameText);
    AddTextToColumn(WeaponColumn, AmmoText, FMargin(0.0f, 2.0f, 0.0f, 0.0f));
    AddTextToColumn(WeaponColumn, ReloadText, FMargin(0.0f, 2.0f, 0.0f, 0.0f));
    ReloadBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    ReloadBar->SetFillColorAndOpacity(AmberColor);
    AddWidgetToColumn(WeaponColumn, ReloadBar, FMargin(0.0f, 4.0f, 0.0f, 0.0f));

    ActionText = MakeTextBlock(FText::GetEmpty(), 14, AmberColor);
    ActionText->SetJustification(ETextJustify::Center);
    PlaceCanvas(GameplayLayer, ActionText, FAnchors(0.5f, 1.0f, 0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -178.0f), FVector2D(820.0f, 40.0f));

    InteractionText = MakeTextBlock(FText::GetEmpty(), 13, PrimaryTextColor);
    InteractionText->SetJustification(ETextJustify::Center);
    PlaceCanvas(GameplayLayer, InteractionText, FAnchors(0.5f, 0.68f, 0.5f, 0.68f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(600.0f, 36.0f));
    InteractionBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    InteractionBar->SetFillColorAndOpacity(TealColor);
    PlaceCanvas(GameplayLayer, InteractionBar, FAnchors(0.5f, 0.72f, 0.5f, 0.72f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(320.0f, 8.0f));

    MissionCardPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    // The card is a short-lived route hand-off, not a full-screen briefing.
    // Keep it under the objective header and left of the reticle so a late
    // combat event never hides the first target while the card fades.
    MissionCardPanel->SetBrushColor(FLinearColor(0.006f, 0.018f, 0.023f, 0.86f));
    MissionCardPanel->SetPadding(FMargin(12.0f, 10.0f));
    PlaceCanvas(GameplayLayer, MissionCardPanel, FAnchors(0.0f, 0.0f, 0.0f, 0.0f), FVector2D::ZeroVector, FVector2D(28.0f, 130.0f), FVector2D(480.0f, 150.0f));
    UVerticalBox* MissionCardColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    MissionCardPanel->AddChild(MissionCardColumn);
    MissionCardTitleText = MakeTextBlock(FText::GetEmpty(), 15, TealColor);
    MissionCardBodyText = MakeTextBlock(FText::GetEmpty(), 11, PrimaryTextColor);
    AddTextToColumn(MissionCardColumn, MissionCardTitleText);
    AddTextToColumn(MissionCardColumn, MissionCardBodyText, FMargin(0.0f, 7.0f, 0.0f, 0.0f));

    SubtitleText = MakeTextBlock(FText::GetEmpty(), 12, PrimaryTextColor);
    SubtitleText->SetJustification(ETextJustify::Center);
    PlaceCanvas(GameplayLayer, SubtitleText, FAnchors(0.5f, 1.0f, 0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -78.0f), FVector2D(820.0f, 34.0f));

    // Native reticle bars keep the gameplay surface authored in UMG while
    // avoiding a dependency on a project font or texture asset.  The dark
    // backing bars are deliberately larger so the reticle remains readable
    // over bright cover, enemy silhouettes, and muzzle flashes.
    const TArray<FVector2D> ReticlePositions = {
        FVector2D(0.0f, -26.0f), FVector2D(0.0f, 26.0f),
        FVector2D(-26.0f, 0.0f), FVector2D(26.0f, 0.0f),
        FVector2D(0.0f, 0.0f)};
    const TArray<FVector2D> ReticleSizes = {
        FVector2D(4.0f, 24.0f), FVector2D(4.0f, 24.0f),
        FVector2D(24.0f, 4.0f), FVector2D(24.0f, 4.0f),
        FVector2D(6.0f, 6.0f)};
    for (int32 Index = 0; Index < ReticlePositions.Num(); ++Index)
    {
        UBorder* ShadowBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        ShadowBar->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.86f));
        const FVector2D ShadowPadding(2.0f, 2.0f);
        PlaceCanvas(
            GameplayLayer,
            ShadowBar,
            FAnchors(0.5f, 0.5f, 0.5f, 0.5f),
            FVector2D(0.5f, 0.5f),
            ReticlePositions[Index],
            ReticleSizes[Index] + ShadowPadding * 2.0f);
        CrosshairShadowBars.Add(ShadowBar);

        UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Bar->SetBrushColor(TealColor);
        PlaceCanvas(GameplayLayer, Bar, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(0.5f, 0.5f), ReticlePositions[Index], ReticleSizes[Index]);
        CrosshairBars.Add(Bar);
    }

    // Combat feedback is authored on the native gameplay layer because the
    // legacy canvas is suppressed once this widget has valid geometry. Keep
    // both surfaces visually consistent: the marker sits on the reticle and
    // the elimination confirmation uses the same short fade as the fallback.
    HitMarkerSymbolText = MakeTextBlock(FText::FromString(TEXT("×")), 42, AmberColor);
    HitMarkerSymbolText->SetJustification(ETextJustify::Center);
    PlaceCanvas(
        GameplayLayer,
        HitMarkerSymbolText,
        FAnchors(0.5f, 0.5f, 0.5f, 0.5f),
        FVector2D(0.5f, 0.5f),
        FVector2D(0.0f, -18.0f),
        FVector2D(96.0f, 66.0f));
    HitMarkerHeadshotText = MakeTextBlock(FText::FromString(TEXT("HEADSHOT")), 13, FLinearColor(0.18f, 0.92f, 1.0f));
    HitMarkerHeadshotText->SetJustification(ETextJustify::Center);
    PlaceCanvas(
        GameplayLayer,
        HitMarkerHeadshotText,
        FAnchors(0.5f, 0.5f, 0.5f, 0.5f),
        FVector2D(0.5f, 0.5f),
        FVector2D(0.0f, 30.0f),
        FVector2D(220.0f, 28.0f));
    SetVisible(HitMarkerSymbolText, false);
    SetVisible(HitMarkerHeadshotText, false);

    EliminationPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    EliminationPanel->SetBrushColor(FLinearColor(0.02f, 0.028f, 0.034f, 0.84f));
    EliminationPanel->SetPadding(FMargin(10.0f, 5.0f));
    PlaceCanvas(
        GameplayLayer,
        EliminationPanel,
        FAnchors(0.5f, 1.0f, 0.5f, 1.0f),
        FVector2D(0.5f, 1.0f),
        FVector2D(0.0f, -214.0f),
        FVector2D(300.0f, 46.0f));
    UVerticalBox* EliminationColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    EliminationPanel->AddChild(EliminationColumn);
    EliminationLabelText = MakeTextBlock(FText::GetEmpty(), 13, AmberColor);
    EliminationLabelText->SetJustification(ETextJustify::Center);
    EliminationTotalText = MakeTextBlock(FText::GetEmpty(), 9, TealColor);
    EliminationTotalText->SetJustification(ETextJustify::Center);
    AddTextToColumn(EliminationColumn, EliminationLabelText);
    AddTextToColumn(EliminationColumn, EliminationTotalText, FMargin(0.0f, 2.0f, 0.0f, 0.0f));
    SetVisible(EliminationPanel, false);

    ScoreboardPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    ScoreboardPanel->SetBrushColor(FLinearColor(0.004f, 0.012f, 0.018f, 0.96f));
    ScoreboardPanel->SetPadding(FMargin(28.0f, 24.0f));
    PlaceCanvas(GameplayLayer, ScoreboardPanel, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0.0f, 0.0f), FVector2D(760.0f, 620.0f));
    ScoreboardText = MakeTextBlock(FText::GetEmpty(), 16, PrimaryTextColor);
    ScoreboardPanel->AddChild(ScoreboardText);

    MenuPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    MenuPanel->SetBrushColor(PanelColor);
    MenuPanel->SetPadding(FMargin(42.0f, 34.0f, 42.0f, 28.0f));
    PlaceCanvas(RootCanvas, MenuPanel, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0.0f, 0.0f), FVector2D(1120.0f, 700.0f));
    UVerticalBox* MenuColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    MenuPanel->AddChild(MenuColumn);
    MenuKickerText = MakeTextBlock(FText::GetEmpty(), 13, MutedTextColor);
    MenuTitleText = MakeTextBlock(FText::GetEmpty(), 34, TealColor);
    MenuBodyText = MakeTextBlock(FText::GetEmpty(), 17, PrimaryTextColor);
    MenuFooterText = MakeTextBlock(FText::GetEmpty(), 13, MutedTextColor);
    AddTextToColumn(MenuColumn, MenuKickerText);
    AddTextToColumn(MenuColumn, MenuTitleText, FMargin(0.0f, 8.0f, 0.0f, 0.0f));
    AddTextToColumn(MenuColumn, MenuBodyText, FMargin(0.0f, 24.0f, 0.0f, 0.0f));
    USpacer* MenuSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
    if (UVerticalBoxSlot* SpacerSlot = MenuColumn->AddChildToVerticalBox(MenuSpacer))
    {
        SpacerSlot->SetSize(ESlateSizeRule::Fill);
    }
    AddTextToColumn(MenuColumn, MenuFooterText, FMargin(0.0f, 16.0f, 0.0f, 0.0f));

    UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    if (UVerticalBoxSlot* ButtonSlot = MenuColumn->AddChildToVerticalBox(ButtonRow))
    {
        ButtonSlot->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 0.0f));
    }

    UTextBlock* IgnoredLabel = nullptr;
    const auto AddButton = [this, ButtonRow](const FText& Label, TObjectPtr<UButton>& OutButton, TObjectPtr<UTextBlock>& OutLabel)
    {
        UTextBlock* ButtonLabel = nullptr;
        UButton* Button = MakeActionButton(Label, ButtonLabel);
        OutButton = Button;
        OutLabel = ButtonLabel;
        if (!Button || !ButtonRow || !WidgetTree)
        {
            return;
        }
        USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        ButtonSize->SetWidthOverride(222.0f);
        ButtonSize->SetHeightOverride(50.0f);
        ButtonSize->AddChild(Button);
        if (UHorizontalBoxSlot* Slot = ButtonRow->AddChildToHorizontalBox(ButtonSize))
        {
            Slot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
            Slot->SetHorizontalAlignment(HAlign_Fill);
        }
    };
    AddButton(NSLOCTEXT("BlackMeridian", "NativeUIPrimary", "PRIMARY"), PrimaryButton, PrimaryButtonLabel);
    AddButton(NSLOCTEXT("BlackMeridian", "NativeUISecondary", "SECONDARY"), SecondaryButton, SecondaryButtonLabel);
    AddButton(NSLOCTEXT("BlackMeridian", "NativeUITertiary", "TERTIARY"), TertiaryButton, TertiaryButtonLabel);
    AddButton(NSLOCTEXT("BlackMeridian", "NativeUIQuaternary", "HELP"), QuaternaryButton, QuaternaryButtonLabel);
    if (PrimaryButton)
    {
        PrimaryButton->OnClicked.AddDynamic(this, &UBMNativeUIWidget::HandlePrimaryClicked);
    }
    if (SecondaryButton)
    {
        SecondaryButton->OnClicked.AddDynamic(this, &UBMNativeUIWidget::HandleSecondaryClicked);
    }
    if (TertiaryButton)
    {
        TertiaryButton->OnClicked.AddDynamic(this, &UBMNativeUIWidget::HandleTertiaryClicked);
    }
    if (QuaternaryButton)
    {
        QuaternaryButton->OnClicked.AddDynamic(this, &UBMNativeUIWidget::HandleQuaternaryClicked);
    }
}

void UBMNativeUIWidget::RefreshPresentation()
{
    ABMPlayerController* Controller = Cast<ABMPlayerController>(GetOwningPlayer());
    ABMHUD* HUD = Controller ? Cast<ABMHUD>(Controller->GetHUD()) : nullptr;
    if (!HUD)
    {
        return;
    }

    const EBMHUDScreen Screen = HUD->GetScreen();
    const bool bGameplay = Screen == EBMHUDScreen::InGame || Screen == EBMHUDScreen::Scoreboard;
    const bool bMenu = !bGameplay && Screen != EBMHUDScreen::Lobby;

    // This path is also called directly by ABMHUD::DrawHUD, so native target
    // state stays synchronized with the same frame as the canvas fallback.
    RefreshTargeting(HUD);

    SetVisible(MenuPanel, bMenu);
    SetVisible(GameplayLayer, bGameplay);
    if (Screen != LastScreen || !bHasPresentedScreen)
    {
        LastScreen = Screen;
        bHasPresentedScreen = true;
        ConfigureMenuActions(Screen);
    }

    if (bMenu)
    {
        RefreshMenu(HUD);
    }
    if (bGameplay)
    {
        RefreshGameplay(HUD);
        RefreshScoreboard(HUD);
    }
}

void UBMNativeUIWidget::RefreshMenu(const ABMHUD* HUD)
{
    if (!HUD)
    {
        return;
    }

    const ABMPlayerController* Controller = Cast<ABMPlayerController>(GetOwningPlayer());
    const EBMHUDScreen Screen = HUD->GetScreen();
    FString Kicker;
    FString Title;
    FString Body;
    FString Footer;

    switch (Screen)
    {
    case EBMHUDScreen::Splash:
        Kicker = TEXT("LATTICE RESPONSE UNIT  //  FIELD ARCHIVE");
        Title = TEXT("BLACK MERIDIAN");
        Body = TEXT("A SIGNAL CAN BE A WARNING.\nOR A WEAPON.\n\nOPERATION GLASSWAKE");
        Footer = TEXT("INITIALIZING MISSION SYSTEMS...");
        break;
    case EBMHUDScreen::MainMenu:
        Kicker = TEXT("LATTICE RESPONSE UNIT  //  SINGLE-PLAYER + MULTIPLAYER");
        Title = TEXT("BLACK MERIDIAN");
        Body = TEXT("A FIRST-PERSON MILITARY THRILLER\n\nDeploy alone through Operation Glasswake, or enter Signal Control for a 2–12 operator node-control match.\n\nWASD MOVE   MOUSE LOOK   LMB FIRE   RMB AIM");
        Footer = TEXT("D-PAD  SELECT     A  CONFIRM     B  BACK     ENTER  CAMPAIGN     F4  SIGNAL CONTROL     F1  SETTINGS     H  HOW TO PLAY");
        break;
    case EBMHUDScreen::CampaignSelection:
    {
        const EBMCampaignLevel Selected = Controller ? Controller->GetSelectedCampaignLevel() : EBMCampaignLevel::SableQuay;
        const FBMCampaignLevelDefinition Definition = GetCampaignLevelDefinition(Selected);
        Kicker = FString::Printf(TEXT("OPERATION GLASSWAKE  //  LEVEL %d / %d"), static_cast<int32>(Selected) + 1, GetBMCampaignLevelCount());
        Title = Definition.DisplayName.ToString().ToUpper();
        Body = FString::Printf(
            TEXT("%s\n\n%s\n\n%s\n\nPICKUP  %s + AMMO  //  HOSTILES  %d / %d / %d"),
            *Definition.LevelId.ToString().ToUpper(),
            *Definition.Briefing.ToString(),
            *CampaignLevelList(Selected),
            *StaticEnum<EBMWeaponId>()->GetDisplayNameTextByValue(static_cast<int64>(Definition.PickupWeaponId)).ToString(),
            Definition.CheckpointWaveSize,
            Definition.OpenCombatWaveSize,
            Definition.FinalDefenseWaveSize);
        Footer = TEXT("UP / DOWN  SELECT LEVEL     A / ENTER  DEPLOY     B / ESC  BACK     H  BRIEFING");
        break;
    }
    case EBMHUDScreen::MultiplayerMenu:
        Kicker = TEXT("NETWORK OPERATIONS  //  LOCAL NULL SUBSYSTEM");
        Title = TEXT("SIGNAL CONTROL");
        Body = TEXT("TWO TEAMS FIGHT OVER ONE ACTIVE SIGNAL NODE.\n\n2–12 PLAYERS  //  6v6 TARGET\nTHREE ROTATING NODES  //  TEAM SCORE\n\nCAPTURE THE BRIGHT NODE, HOLD THE GLOW, AND MOVE WHEN THE SIGNAL ROTATES.");
        Footer = TEXT("A / ENTER  OPEN LOBBY     B / ESC  BACK");
        break;
    case EBMHUDScreen::Settings:
    {
        const UBMGameUserSettings* Settings = GEngine ? Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
        const int32 SelectedRow = Controller ? Controller->GetSettingsRow() : 0;
        const auto Row = [SelectedRow](const int32 Index, const FString& Label, const FString& Value)
        {
            return FString::Printf(TEXT("%s %-24s %s"), SelectedRow == Index ? TEXT("▶") : TEXT(" "), *Label, *Value);
        };
        Kicker = TEXT("SYSTEM CONFIGURATION  //  LOCAL PROFILE");
        Title = TEXT("SETTINGS");
        Body = Settings
            ? FString::Printf(
                TEXT("%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s"),
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
                *Row(18, TEXT("FRIENDLY FIRE"), Settings->bFriendlyFireEnabled ? TEXT("ON") : TEXT("OFF")))
            : TEXT("PROFILE UNAVAILABLE");
        Footer = TEXT("UP / DOWN  SELECT     LEFT / RIGHT  ADJUST     ESC  BACK");
        break;
    }
    case EBMHUDScreen::Controls:
        Kicker = TEXT("INPUT CONFIGURATION  //  CONFLICT-SAFE REMAPPING");
        Title = TEXT("CONTROLS");
        Body = TEXT("MOVE                 W A S D\nLOOK                 MOUSE\nSPRINT               SHIFT\nCROUCH               C\nPRONE                Z\nJUMP / MANTLE        SPACE\nFIRE                 LEFT MOUSE\nAIM                  RIGHT MOUSE\nRELOAD               R\nFRAG                 G\nTACTICAL             X\nINTERACT             E\nSCOREBOARD           TAB");
        Footer = TEXT("UP / DOWN  SELECT     LEFT / RIGHT  REMAP     ESC  BACK");
        break;
    case EBMHUDScreen::HowToPlay:
        Kicker = TEXT("FIELD MANUAL  //  READ BEFORE DEPLOYMENT");
        Title = TEXT("HOW TO PLAY");
        Body = TEXT("GLASSWAKE\n1  FOLLOW THE TEAL BEACON TO INSERT.\n2  REACH AMBER, THEN CLEAR THE RED VANTA WAVE.\n3  FOLLOW ORANGE AND HOLD E AT THE RELAY DOOR.\n4  TEAL CONSOLE INTEL IS OPTIONAL.\n5  FOLLOW YELLOW, HOLD E AT THE BOAT, THEN DEFEND THE PIER.\n\nWEAPONS AND AMMO AUTO-COLLECT WHEN YOU MOVE CLOSE.\nR RELOAD   G FRAG   X VEIL   E INTERACT   SHIFT SPRINT\n\nSIGNAL CONTROL\nCAPTURE THE ACTIVE NODE, HOLD IT TO SCORE, AND MOVE WHEN IT ROTATES.");
        Footer = TEXT("A / H  CLOSE     B / ESC  BACK");
        break;
    case EBMHUDScreen::Pause:
        Kicker = TEXT("OPERATION GLASSWAKE  //  WORLD PAUSED");
        Title = TEXT("PAUSED");
        Body = TEXT("YOUR SQUAD IS HOLDING POSITION.\n\nResume to continue the current objective. Settings and controls remain available without losing the checkpoint.");
        Footer = TEXT("D-PAD  SELECT     A / ESC  RESUME     B  BACK     F1  SETTINGS     K / F2  CONTROLS     BACKSPACE  RESTART");
        break;
    case EBMHUDScreen::Loading:
        Kicker = TEXT("FIELD SYSTEMS  //  DEPLOYMENT");
        Title = TEXT("DEPLOYING");
        Body = TEXT("SYNCING MISSION STATE...\n\nLoading world geometry, squad state, weapon profiles, and objective route.");
        Footer = TEXT("PLEASE WAIT");
        break;
    case EBMHUDScreen::Death:
        Kicker = TEXT("LATTICE RESPONSE UNIT  //  RECOVERABLE FAILURE");
        Title = TEXT("SQUAD LOST");
        Body = TEXT("PLAYER ELIMINATED.\n\nThe campaign checkpoint is preserved. Restart to re-enter from the last safe state; the playable perimeter is guarded against boundary falls.");
        Footer = TEXT("A / ENTER / BACKSPACE  RESTART CHECKPOINT     B / ESC  RETURN TO CAMPAIGN");
        break;
    case EBMHUDScreen::Results:
    {
        const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
        Kicker = State && State->bSignalControlMode ? TEXT("SIGNAL CONTROL  //  MATCH COMPLETE") : TEXT("OPERATION GLASSWAKE  //  ARCHIVE SECURED");
        Title = State && State->bSignalControlMode ? TEXT("MATCH COMPLETE") : TEXT("OPERATION COMPLETE");
        if (State && State->bSignalControlMode)
        {
            const TCHAR* Winner = State->SignalControl.WinningTeam == EBMTeam::Lattice ? TEXT("LATTICE RESPONSE UNIT") : (State->SignalControl.WinningTeam == EBMTeam::Vanta ? TEXT("VANTA CONTINUUM") : TEXT("NO WINNER"));
            Body = FString::Printf(TEXT("WINNER  %s\n\nLATTICE  %03d\nVANTA    %03d\n\nThe signal node is no longer contested."), Winner, State->SignalControl.LatticeScore, State->SignalControl.VantaScore);
        }
        else
        {
            const bool bHasNextCampaignLevel = State &&
                static_cast<int32>(State->CampaignLevel) < GetBMCampaignLevelCount() - 1;
            if (bHasNextCampaignLevel)
            {
                const EBMCampaignLevel NextLevel = static_cast<EBMCampaignLevel>(static_cast<int32>(State->CampaignLevel) + 1);
                const FBMCampaignLevelDefinition NextDefinition = GetCampaignLevelDefinition(NextLevel);
                Body = FString::Printf(
                    TEXT("THE ARCHIVE IS SECURED.\n\nLEVEL %02d IS COMPLETE.\nNEXT: LEVEL %02d // %s\n\nContinue to the next level?\nNEXT LEVEL is selected by default."),
                    static_cast<int32>(State->CampaignLevel) + 1,
                    static_cast<int32>(NextLevel) + 1,
                    *NextDefinition.DisplayName.ToString().ToUpper());
                Footer = TEXT("A / ENTER  NEXT LEVEL     B / ESC  CAMPAIGN MENU");
            }
            else
            {
                Body = TEXT("THE ARCHIVE IS SECURED.\n\nThe full Glasswake route is complete.\nReturn to the campaign menu to choose another operation.");
                Footer = TEXT("A / ENTER  RETURN TO MENU     B / ESC  CLOSE");
            }
        }
        break;
    }
    default:
        break;
    }

    SetText(MenuKickerText, FText::FromString(Kicker));
    SetText(MenuTitleText, FText::FromString(Title));
    SetText(MenuBodyText, FText::FromString(Body));
    SetText(MenuFooterText, FText::FromString(Footer));
}

void UBMNativeUIWidget::RefreshGameplay(const ABMHUD* HUD)
{
    if (!HUD || !GetWorld())
    {
        return;
    }

    const ABMGameState* State = GetWorld()->GetGameState<ABMGameState>();
    const ABMPlayerCharacter* Player = nullptr;
    if (const ABMPlayerController* Controller = Cast<ABMPlayerController>(GetOwningPlayer()))
    {
        Player = Cast<ABMPlayerCharacter>(Controller->GetPawn());
    }
    if (!State || !Player)
    {
        SetVisible(ObjectivePanel, false);
        SetVisible(HealthPanel, false);
        SetVisible(WeaponPanel, false);
        SetVisible(ObjectiveMarkerPanel, false);
        SetVisible(KillFeedPanel, false);
        SetVisible(TargetPanel, false);
        SetVisible(HitMarkerSymbolText, false);
        SetVisible(HitMarkerHeadshotText, false);
        SetVisible(EliminationPanel, false);
        return;
    }

    SetVisible(ObjectivePanel, true);
    SetVisible(HealthPanel, true);
    SetVisible(WeaponPanel, true);

    const bool bCombatPhase = !State->bSignalControlMode
        && (State->MissionPhase == EBMMissionPhase::Checkpoint
            || State->MissionPhase == EBMMissionPhase::OpenCombat
            || State->MissionPhase == EBMMissionPhase::FinalDefense);

    // Showcase and late-spawned opening contacts can briefly contain live
    // enemies before the mission phase label catches up. The player should
    // never be told to await briefing while an armed hostile is already in
    // the sightline, so this changes presentation only; mission authority is
    // still owned by the director.
    const bool bLiveCombatContact = !State->bSignalControlMode
        && State->HostileCount > 0
        && (bCombatPhase
            || State->MissionPhase == EBMMissionPhase::Intro
            || State->MissionPhase == EBMMissionPhase::Insertion
            || State->MissionPhase == EBMMissionPhase::QuietApproach);

    FString Context = State->bSignalControlMode ? TEXT("SIGNAL CONTROL") : MissionPhaseLabel(State->MissionPhase);
    FString Objective = State->bSignalControlMode
        ? FString::Printf(TEXT("NODE %d ACTIVE  //  ROTATES IN %02d"), State->SignalControl.ActiveNodeIndex + 1, FMath::CeilToInt(State->SignalControl.SecondsUntilRotation))
        : FString::Printf(TEXT("NEXT STEP  %s"), *State->CurrentObjective.Title.ToString());
    FString ObjectiveHint = State->bSignalControlMode
        ? TEXT("FOLLOW NODE   //   TAB SCOREBOARD")
        : (State->MissionPhase == EBMMissionPhase::InteriorBreach
            ? TEXT("CLEAR  //  MOVE TO ORANGE RELAY HOUSE  //  HOLD E")
            : ((State->MissionPhase == EBMMissionPhase::Checkpoint || State->MissionPhase == EBMMissionPhase::OpenCombat || State->MissionPhase == EBMMissionPhase::FinalDefense)
                ? TEXT("HOLD COMBAT AREA   //   H HELP")
                : TEXT("FOLLOW BEACON   //   H HELP")));
    if (bLiveCombatContact && !bCombatPhase)
    {
        Context = TEXT("CONTACT");
        Objective = FString::Printf(TEXT("CLEAR THE CONTACT  //  %02d HOSTILES"), State->HostileCount);
        ObjectiveHint = TEXT("FIRE AND MOVE   //   RED RETICLE = LIVE TARGET");
    }
    SetText(ObjectiveContextText, FText::FromString(FString::Printf(TEXT("%s   //   %02d HOSTILES   //   CP %d"), *Context, State->HostileCount, State->CheckpointIndex)));
    SetText(ObjectiveTitleText, FText::FromString(Objective));
    SetText(ObjectiveHintText, FText::FromString(ObjectiveHint));

    int32 ActiveStep = 1;
    if (bLiveCombatContact || State->MissionPhase == EBMMissionPhase::Checkpoint || State->MissionPhase == EBMMissionPhase::OpenCombat)
    {
        ActiveStep = 2;
    }
    else if (State->MissionPhase == EBMMissionPhase::InteriorBreach || State->MissionPhase == EBMMissionPhase::Intelligence)
    {
        ActiveStep = 3;
    }
    else if (State->MissionPhase == EBMMissionPhase::Extraction)
    {
        ActiveStep = 4;
    }
    else if (State->MissionPhase == EBMMissionPhase::FinalDefense || State->MissionPhase == EBMMissionPhase::Complete)
    {
        ActiveStep = 5;
    }
    SetText(MissionFlowText, FText::FromString(State->bSignalControlMode
        ? FString::Printf(TEXT("NODE %d ACTIVE   L %03d   V %03d\nMOVE WITH SIGNAL   //   TAB SCOREBOARD"), State->SignalControl.ActiveNodeIndex + 1, State->SignalControl.LatticeScore, State->SignalControl.VantaScore)
        : FString::Printf(TEXT("FLOW  1 MOVE   %s2 CLEAR   %s3 BREACH   %s4 EXTRACT   %s5 DEFEND\nACTIVE: %s"), ActiveStep == 1 ? TEXT("> ") : TEXT("  "), ActiveStep == 2 ? TEXT("> ") : TEXT("  "), ActiveStep == 3 ? TEXT("> ") : TEXT("  "), ActiveStep >= 4 ? TEXT("> ") : TEXT("  "), ActiveStep == 1 ? TEXT("TEAL MOVE") : (ActiveStep == 2 ? TEXT("AMBER FIGHT") : (ActiveStep == 3 ? TEXT("ORANGE HOLD E") : TEXT("YELLOW EXTRACT"))))));

    FString KillFeedCopy;
    for (int32 Index = 0; Index < FMath::Min(3, State->RecentKillFeed.Num()); ++Index)
    {
        if (Index > 0)
        {
            KillFeedCopy += TEXT("\n");
        }
        KillFeedCopy += State->RecentKillFeed[Index];
    }
    SetText(KillFeedText, FText::FromString(KillFeedCopy));
    SetVisible(KillFeedPanel, !KillFeedCopy.IsEmpty());

    const int32 Heading = (FMath::RoundToInt(FMath::UnwindDegrees(Player->GetControlRotation().Yaw)) + 360) % 360;
    static const TCHAR* CompassDirections[] = {TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW")};
    const int32 CenterDirection = FMath::RoundToInt(static_cast<float>(Heading) / 45.0f) % UE_ARRAY_COUNT(CompassDirections);
    FString Compass;
    for (int32 Relative = -3; Relative <= 3; ++Relative)
    {
        if (!Compass.IsEmpty())
        {
            Compass += TEXT("   ");
        }
        Compass += CompassDirections[(CenterDirection + Relative + UE_ARRAY_COUNT(CompassDirections) * 2) % UE_ARRAY_COUNT(CompassDirections)];
    }
    SetText(CompassText, FText::FromString(FString::Printf(TEXT("%s\nHEADING %03d"), *Compass, Heading)));

    const float Health = Player->HealthComponent ? Player->HealthComponent->Health : 0.0f;
    const float HealthPercent = Player->HealthComponent ? Player->HealthComponent->GetHealthPercent() : 0.0f;
    SetText(HealthText, FText::FromString(FString::Printf(TEXT("VITALS  %03d"), FMath::RoundToInt(Health))));
    HealthBar->SetPercent(FMath::Clamp(HealthPercent, 0.0f, 1.0f));
    HealthBar->SetFillColorAndOpacity(HealthPercent <= 0.30f ? DangerColor : TealColor);
    SetText(OrdnanceText, FText::FromString(FString::Printf(TEXT("FRAG  %d/%d     VEIL  %d/%d"), Player->GetFragCount(), Player->GetMaxFragCount(), Player->GetVeilCount(), Player->GetMaxVeilCount())));

    const ABMWeapon* Weapon = Player->GetCurrentWeapon();
    const FString WeaponName = Weapon && Weapon->GetWeaponData() ? Weapon->GetWeaponData()->DisplayName.ToString() : TEXT("UNARMED");
    SetText(WeaponNameText, FText::FromString(WeaponName));
    SetText(AmmoText, FText::FromString(Weapon ? FString::Printf(TEXT("%02d  /  %03d"), Weapon->GetMagazineAmmo(), Weapon->GetReserveAmmo()) : TEXT("--  /  ---")));
    const bool bReloading = Weapon && Weapon->IsReloading();
    const float ReloadProgress = bReloading && Weapon ? Weapon->GetReloadProgress() : 0.0f;
    ReloadBar->SetPercent(ReloadProgress);
    SetVisible(ReloadBar, bReloading);
    const TCHAR* ReloadPhase = ReloadProgress < 0.22f
        ? TEXT("MAG OUT")
        : (ReloadProgress < 0.68f ? TEXT("SEAT MAG") : TEXT("CHAMBER"));
    SetText(ReloadText, FText::FromString(bReloading
        ? FString::Printf(TEXT("RELOADING  //  %s  //  %02d%%"), ReloadPhase, FMath::RoundToInt(ReloadProgress * 100.0f))
        : (Weapon && Weapon->GetMagazineAmmo() <= 5 ? TEXT("LOW AMMO  //  PRESS R") : TEXT("READY"))));
    ReloadText->SetColorAndOpacity(bReloading || (Weapon && Weapon->GetMagazineAmmo() <= 5) ? AmberColor : MutedTextColor);

    FString Action;
    if (State->bSignalControlMode)
    {
        Action = TEXT("CAPTURE THE ACTIVE NODE  //  STAY INSIDE THE GLOW TO SCORE");
    }
    else if (bLiveCombatContact && !bCombatPhase)
    {
        Action = FString::Printf(TEXT("CONTACT  //  FIRE AND MOVE  //  %02d HOSTILES REMAIN"), State->HostileCount);
    }
    else
    {
        switch (State->MissionPhase)
        {
        case EBMMissionPhase::Insertion: Action = TEXT("WALK TO THE TEAL BEACON  //  WASD MOVE"); break;
        case EBMMissionPhase::QuietApproach: Action = TEXT("WALK TO THE AMBER CHECKPOINT  //  CONTACT STARTS THERE"); break;
        case EBMMissionPhase::Checkpoint: Action = FString::Printf(TEXT("COMBAT  //  SHOOT THE RED VANTA  //  %02d REMAINING"), State->HostileCount); break;
        case EBMMissionPhase::InteriorBreach: Action = TEXT("CLEAR  //  MOVE TO ORANGE RELAY HOUSE  //  HOLD E AT THE DOOR"); break;
        case EBMMissionPhase::Intelligence: Action = TEXT("OPTIONAL INTEL  //  HOLD E AT THE TEAL CONSOLE"); break;
        case EBMMissionPhase::OpenCombat: Action = FString::Printf(TEXT("COMBAT  //  STAY IN THE SEAWALL ARENA  //  %02d REMAINING"), State->HostileCount); break;
        case EBMMissionPhase::Extraction: Action = TEXT("WALK TO THE YELLOW BOAT  //  HOLD E TO SIGNAL EXTRACTION"); break;
        case EBMMissionPhase::FinalDefense: Action = FString::Printf(TEXT("FINAL DEFENSE  //  HOLD THE PIER  //  %02d REMAINING"), State->HostileCount); break;
        case EBMMissionPhase::Complete: Action = TEXT("MISSION COMPLETE  //  PRESS ENTER TO RETURN TO MENU"); break;
        default: break;
        }
    }
    SetText(ActionText, FText::FromString(Action));
    ActionText->SetColorAndOpacity(State->MissionPhase == EBMMissionPhase::Checkpoint || State->MissionPhase == EBMMissionPhase::OpenCombat || State->MissionPhase == EBMMissionPhase::FinalDefense ? VantaColor : AmberColor);

    RefreshObjectiveMarker(HUD, State, Player);

    const FString InteractionPrompt = HUD->GetInteractionPromptText();
    const bool bHoldingInteraction = Player->IsHoldingInteraction();
    const bool bInteractionAvailable = !InteractionPrompt.IsEmpty() || bHoldingInteraction;

    // Interaction copy has priority over the persistent action strip. Without
    // this arbitration, "PRESS E" and the current combat objective occupy the
    // same lower-center band during the exact moment the player needs to read
    // the prompt and keep the reticle on the target.
    SetVisible(ActionText, !bInteractionAvailable);
    SetVisible(InteractionText, bInteractionAvailable);
    SetText(InteractionText, FText::FromString(bHoldingInteraction
        ? FString::Printf(TEXT("%s  //  HOLD E  %02d%%"), *InteractionPrompt, FMath::RoundToInt(Player->GetInteractionHoldProgress() * 100.0f))
        : InteractionPrompt));
    SetVisible(InteractionBar, bHoldingInteraction);
    InteractionBar->SetPercent(bHoldingInteraction ? Player->GetInteractionHoldProgress() : 0.0f);

    const bool bMissionCard = HUD->HasActiveMissionCard();
    // The objective header and action strip already describe the live fight.
    // Suppress a stale route hand-off card once contact begins so it cannot
    // cover enemies or compete with the reticle during combat.
    const bool bCombatPresentation = bCombatPhase || bLiveCombatContact;
    const bool bShowMissionCard = bMissionCard && !bCombatPresentation;
    SetVisible(MissionCardPanel, bShowMissionCard);
    if (bShowMissionCard)
    {
        SetText(MissionCardTitleText, HUD->GetMissionCardTitleText());
        SetText(MissionCardBodyText, HUD->GetMissionCardBodyText());
    }
    const bool bSubtitle = HUD->HasActiveSubtitle();
    // Do not stack dialogue on top of a route card or an active interaction.
    // The subtitle remains available in the clear gameplay band after those
    // higher-priority surfaces have yielded.
    const bool bShowSubtitle = bSubtitle && !bShowMissionCard && !bHoldingInteraction;
    SetVisible(SubtitleText, bShowSubtitle);
    if (bShowSubtitle)
    {
        SetText(SubtitleText, HUD->GetSubtitleText());
    }

    const float DamageFlashAlpha = HUD->GetDamageFlashTimeRemaining() > 0.0f
        ? FMath::Clamp(HUD->GetDamageFlashTimeRemaining() / 0.34f, 0.0f, 1.0f) * HUD->GetDamageFlashIntensity() * 0.34f
        : 0.0f;
    DamageFlashBorder->SetBrushColor(FLinearColor(0.95f, 0.04f, 0.02f, DamageFlashAlpha));
    SetVisible(DamageFlashBorder, DamageFlashAlpha > 0.0f);

    const float HitMarkerTimeRemaining = HUD->GetHitMarkerTimeRemaining();
    const bool bHitMarkerHeadshot = HUD->GetHitMarkerHeadshot();
    const float HitMarkerDuration = bHitMarkerHeadshot ? 0.3f : 0.2f;
    const float HitMarkerFade = HitMarkerDuration > 0.0f
        ? FMath::Clamp(HitMarkerTimeRemaining / HitMarkerDuration, 0.0f, 1.0f)
        : 0.0f;
    const bool bShowHitMarker = HitMarkerTimeRemaining > 0.0f && HitMarkerFade > 0.0f;
    HitMarkerSymbolText->SetColorAndOpacity(bHitMarkerHeadshot
        ? FLinearColor(0.18f, 0.92f, 1.0f)
        : AmberColor);
    HitMarkerSymbolText->SetRenderOpacity(HitMarkerFade);
    HitMarkerHeadshotText->SetRenderOpacity(HitMarkerFade);
    SetVisible(HitMarkerSymbolText, bShowHitMarker);
    SetVisible(HitMarkerHeadshotText, bShowHitMarker && bHitMarkerHeadshot);

    const float EliminationTimeRemaining = HUD->GetEliminationTimeRemaining();
    const float EliminationFade = FMath::Clamp(
        EliminationTimeRemaining < 0.28f ? EliminationTimeRemaining / 0.28f : 1.0f,
        0.0f,
        1.0f);
    const bool bShowElimination = EliminationTimeRemaining > 0.0f && EliminationFade > 0.0f;
    EliminationPanel->SetRenderOpacity(EliminationFade);
    SetText(
        EliminationLabelText,
        FText::FromString(HUD->GetEliminationHeadshot()
            ? TEXT("HEADSHOT  //  ELIMINATION")
            : TEXT("ELIMINATION")));
    SetText(
        EliminationTotalText,
        FText::FromString(FString::Printf(
            TEXT("KILL CONFIRMED  //  TOTAL %02d"),
            HUD->GetEliminationKillCount())));
    SetVisible(EliminationPanel, bShowElimination);

}

void UBMNativeUIWidget::RefreshTargeting(const ABMHUD* HUD)
{
    if (!HUD || !GetWorld())
    {
        return;
    }

    const bool bGameplay = HUD->GetScreen() == EBMHUDScreen::InGame
        || HUD->GetScreen() == EBMHUDScreen::Scoreboard;
    const ABMPlayerController* Controller = Cast<ABMPlayerController>(GetOwningPlayer());
    const ABMPlayerCharacter* Player = Controller ? Cast<ABMPlayerCharacter>(Controller->GetPawn()) : nullptr;
    const ABMWeapon* Weapon = Player ? Player->GetCurrentWeapon() : nullptr;

    bool bTargetingLiveEnemy = false;
    const ABMCombatCharacter* CrosshairTarget = nullptr;
    if (bGameplay && Player && !Player->IsEliminated())
    {
        // The player snapshot is generated after camera bob, stance, recoil,
        // and cinematic cue transforms. It is the same target decision used
        // by the player fire request for this frame.
        CrosshairTarget = Player->HasLocalAimSnapshot()
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
            CrosshairTarget = BMCombatTargeting::ResolveLiveHostileCrosshairTarget(
                GetWorld(),
                Player,
                TraceStart,
                TraceDirection,
                TraceRange,
                Weapon);
            bTargetingLiveEnemy = CrosshairTarget != nullptr;
        }
    }

    const ABMEnemyCharacter* CrosshairEnemy = Cast<ABMEnemyCharacter>(CrosshairTarget);
    const UBMHealthComponent* CrosshairTargetHealth = CrosshairTarget
        ? CrosshairTarget->HealthComponent
        : nullptr;
    const bool bShowTargetPanel = bTargetingLiveEnemy && CrosshairTarget && CrosshairTargetHealth;
    SetVisible(TargetPanel, bShowTargetPanel);
    if (bShowTargetPanel && Player)
    {
        const float TargetDistanceMeters = FVector::Dist(Player->GetPawnViewLocation(), CrosshairTarget->GetActorLocation()) / 100.0f;
        const float TargetHealthPercent = FMath::Clamp(CrosshairTargetHealth->GetHealthPercent(), 0.0f, 1.0f);
        const FLinearColor RoleColor = CrosshairEnemy
            ? CrosshairEnemy->GetEnemyRoleColor()
            : VantaColor;
        const FString RoleMarker = CrosshairEnemy
            ? CrosshairEnemy->GetEnemyRoleMarker()
            : TEXT("SIG");
        const FString RoleLabel = CrosshairEnemy
            ? CrosshairEnemy->GetEnemyRoleLabel()
            : TEXT("SIGNAL BOT");
        TargetContextText->SetColorAndOpacity(RoleColor);
        TargetNameText->SetColorAndOpacity(DangerColor);
        TargetHealthText->SetColorAndOpacity(PrimaryTextColor);
        TargetHealthBar->SetFillColorAndOpacity(RoleColor);
        SetText(TargetContextText, FText::FromString(FString::Printf(TEXT("TARGET LOCK  //  %03dm"), FMath::Max(0, FMath::RoundToInt(TargetDistanceMeters)))));
        SetText(TargetNameText, FText::FromString(FString::Printf(TEXT("%s  %s"), *RoleMarker, *RoleLabel)));
        SetText(TargetHealthText, FText::FromString(FString::Printf(TEXT("HOSTILE VITALS  %03d  //  %02d%%"), FMath::Max(0, FMath::RoundToInt(CrosshairTargetHealth->Health)), FMath::RoundToInt(TargetHealthPercent * 100.0f))));
        TargetHealthBar->SetPercent(TargetHealthPercent);
    }

#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CrosshairProbe")))
    {
        static bool bLastCrosshairProbeTarget = false;
        if (bLastCrosshairProbeTarget != bTargetingLiveEnemy)
        {
            bLastCrosshairProbeTarget = bTargetingLiveEnemy;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian UI: native crosshair target live_enemy=%s color=%s"),
                bTargetingLiveEnemy ? TEXT("yes") : TEXT("no"),
                bTargetingLiveEnemy ? TEXT("danger_red") : TEXT("teal"));
        }
    }
#endif

    const bool bTargetingDanger = Weapon && Weapon->GetMagazineAmmo() <= 0;
    for (UBorder* Bar : CrosshairBars)
    {
        if (Bar)
        {
            Bar->SetBrushColor(bTargetingDanger || bTargetingLiveEnemy ? DangerColor : TealColor);
        }
    }
}

void UBMNativeUIWidget::RefreshObjectiveMarker(
    const ABMHUD* HUD,
    const ABMGameState* State,
    const ABMPlayerCharacter* Player)
{
    if (!ObjectiveMarkerPanel || !ObjectiveMarkerTitleText || !ObjectiveMarkerDirectionText || !ObjectiveMarkerSlot
        || !HUD || !State || !Player || HUD->GetScreen() != EBMHUDScreen::InGame
        || State->bSignalControlMode || !State->CurrentObjective.bHasWorldLocation
        || State->MissionPhase == EBMMissionPhase::Checkpoint
        || State->MissionPhase == EBMMissionPhase::OpenCombat
        || State->MissionPhase == EBMMissionPhase::FinalDefense
        || State->MissionPhase == EBMMissionPhase::Complete
        || State->MissionPhase == EBMMissionPhase::Failed
        || HUD->HasInteractionPrompt())
    {
        SetVisible(ObjectiveMarkerPanel, false);
        return;
    }

    ABMPlayerController* Controller = Cast<ABMPlayerController>(GetOwningPlayer());
    if (!Controller)
    {
        SetVisible(ObjectiveMarkerPanel, false);
        return;
    }

    const FVector ToTarget = State->CurrentObjective.WorldLocation - Player->GetActorLocation();
    const int32 DistanceMeters = FMath::Max(0, FMath::RoundToInt(ToTarget.Size2D() / 100.0f));
    if (DistanceMeters <= 3)
    {
        SetVisible(ObjectiveMarkerPanel, false);
        return;
    }

    int32 ViewportWidth = 0;
    int32 ViewportHeight = 0;
    Controller->GetViewportSize(ViewportWidth, ViewportHeight);
    const FVector2D LocalSize = GetCachedGeometry().GetLocalSize();
    if (ViewportWidth <= 0 || ViewportHeight <= 0 || LocalSize.X <= 1.0f || LocalSize.Y <= 1.0f)
    {
        SetVisible(ObjectiveMarkerPanel, false);
        return;
    }

    FVector2D ScreenPosition;
    const bool bProjected = Controller->ProjectWorldLocationToScreen(
        State->CurrentObjective.WorldLocation + FVector(0.0f, 0.0f, 125.0f),
        ScreenPosition,
        true);
    const bool bOnScreen = bProjected
        && ScreenPosition.X > 80.0f
        && ScreenPosition.X < static_cast<float>(ViewportWidth) - 80.0f
        && ScreenPosition.Y > 170.0f
        && ScreenPosition.Y < static_cast<float>(ViewportHeight) - 170.0f;

    const FRotator ViewRotation(0.0f, Player->GetControlRotation().Yaw, 0.0f);
    const FVector ViewForward = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::X);
    const FVector ViewRight = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y);
    const FVector FlatDirection = ToTarget.GetSafeNormal2D();
    const float ForwardDot = FVector::DotProduct(FlatDirection, ViewForward);
    const float RightDot = FVector::DotProduct(FlatDirection, ViewRight);

    FString ObjectiveLabel = TEXT("OBJECTIVE");
    if (State->MissionPhase == EBMMissionPhase::InteriorBreach)
    {
        ObjectiveLabel = TEXT("ORANGE RELAY HOUSE");
    }
    else if (State->MissionPhase == EBMMissionPhase::Intelligence)
    {
        ObjectiveLabel = TEXT("TEAL CONSOLE");
    }
    else if (State->MissionPhase == EBMMissionPhase::Extraction)
    {
        ObjectiveLabel = TEXT("YELLOW BOAT");
    }

    FString Pointer;
    FString Direction;
    if (bOnScreen)
    {
        Pointer = TEXT("V");
        Direction = State->MissionPhase == EBMMissionPhase::InteriorBreach
            ? TEXT("MOVE TO ORANGE RELAY HOUSE  //  HOLD E")
            : TEXT("FOLLOW THIS ACTIVE OBJECTIVE");
    }
    else if (ForwardDot > 0.72f)
    {
        Pointer = TEXT("^^");
        Direction = TEXT("AHEAD  //  KEEP MOVING");
    }
    else if (ForwardDot < -0.72f)
    {
        Pointer = TEXT("<>");
        Direction = TEXT("BEHIND  //  TURN AROUND");
    }
    else if (RightDot > 0.0f)
    {
        Pointer = TEXT(">>");
        Direction = TEXT("RIGHT  //  TURN UNTIL AHEAD");
    }
    else
    {
        Pointer = TEXT("<<");
        Direction = TEXT("LEFT  //  TURN UNTIL AHEAD");
    }

    SetText(
        ObjectiveMarkerTitleText,
        FText::FromString(FString::Printf(TEXT("%s  %s  //  %03dm"), *Pointer, *ObjectiveLabel, DistanceMeters)));
    SetText(
        ObjectiveMarkerDirectionText,
        FText::FromString(State->MissionPhase == EBMMissionPhase::InteriorBreach
            ? FString::Printf(TEXT("%s  //  FOLLOW ORANGE TO THE DOOR"), *Direction)
            : Direction));

    const float ScaleX = LocalSize.X / static_cast<float>(ViewportWidth);
    const float ScaleY = LocalSize.Y / static_cast<float>(ViewportHeight);
    FVector2D MarkerPosition = bOnScreen
        ? FVector2D(ScreenPosition.X * ScaleX, (ScreenPosition.Y - 130.0f) * ScaleY)
        : FVector2D(LocalSize.X * 0.5f, 122.0f * ScaleY);
    MarkerPosition.X = FMath::Clamp(MarkerPosition.X, 205.0f * ScaleX, LocalSize.X - 205.0f * ScaleX);
    MarkerPosition.Y = FMath::Clamp(MarkerPosition.Y, 92.0f * ScaleY, LocalSize.Y - 190.0f * ScaleY);
    ObjectiveMarkerSlot->SetPosition(MarkerPosition);
    SetVisible(ObjectiveMarkerPanel, true);
}

void UBMNativeUIWidget::RefreshScoreboard(const ABMHUD* HUD)
{
    if (!HUD || !GetWorld())
    {
        return;
    }

    const bool bVisible = HUD->GetScreen() == EBMHUDScreen::Scoreboard;
    SetVisible(ScoreboardPanel, bVisible);
    if (!bVisible)
    {
        return;
    }

    const ABMGameState* State = GetWorld()->GetGameState<ABMGameState>();
    if (!State)
    {
        return;
    }

    FString Body = FString::Printf(
        TEXT("SIGNAL CONTROL  //  SCOREBOARD\n\nLATTICE  %04d\nVANTA    %04d\nACTIVE NODE  %d\nROTATES IN  %02d\n"),
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
        Body += FString::Printf(
            TEXT("\n%s  %-12s  K %02d  D %02d  OBJ %03d  %03dms"),
            TeamLabel,
            *PlayerName.Left(12),
            PlayerState->Kills,
            PlayerState->Deaths,
            PlayerState->ObjectiveScore,
            FMath::RoundToInt(PlayerState->SmoothedLatencyMs > 0.0f ? PlayerState->SmoothedLatencyMs : PlayerState->GetPingInMilliseconds()));
    }
    Body += TEXT("\n\nTAB  CLOSE");
    SetText(ScoreboardText, FText::FromString(Body));
}

void UBMNativeUIWidget::ConfigureMenuActions(const EBMHUDScreen Screen)
{
    PrimaryAction = INDEX_NONE;
    SecondaryAction = INDEX_NONE;
    TertiaryAction = INDEX_NONE;
    QuaternaryAction = INDEX_NONE;
    FocusedMenuButton = INDEX_NONE;

    switch (Screen)
    {
    case EBMHUDScreen::MainMenu:
        PrimaryAction = 1;
        SecondaryAction = 2;
        TertiaryAction = 3;
        QuaternaryAction = 5;
        break;
    case EBMHUDScreen::CampaignSelection:
        PrimaryAction = 0;
        SecondaryAction = 6;
        QuaternaryAction = 5;
        break;
    case EBMHUDScreen::MultiplayerMenu:
        PrimaryAction = 0;
        SecondaryAction = 6;
        break;
    case EBMHUDScreen::Settings:
    case EBMHUDScreen::Controls:
    case EBMHUDScreen::HowToPlay:
        PrimaryAction = 6;
        break;
    case EBMHUDScreen::Pause:
        PrimaryAction = 9;
        SecondaryAction = 3;
        TertiaryAction = 4;
        QuaternaryAction = 5;
        break;
    case EBMHUDScreen::Death:
        PrimaryAction = 7;
        SecondaryAction = 8;
        break;
    case EBMHUDScreen::Results:
    {
        const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
        const bool bHasNextCampaignLevel = State && !State->bSignalControlMode &&
            static_cast<int32>(State->CampaignLevel) < GetBMCampaignLevelCount() - 1;
        PrimaryAction = bHasNextCampaignLevel ? 10 : 8;
        // Completing a campaign level is an explicit decision point. Keep
        // the next operation focused by default, while exposing the menu as
        // the visible alternative instead of requiring Escape to discover it.
        if (bHasNextCampaignLevel)
        {
            SecondaryAction = 8;
        }
        break;
    }
    default:
        break;
    }

    const TArray<TPair<UButton*, int32>> Buttons = {
        TPair<UButton*, int32>(PrimaryButton, PrimaryAction),
        TPair<UButton*, int32>(SecondaryButton, SecondaryAction),
        TPair<UButton*, int32>(TertiaryButton, TertiaryAction),
        TPair<UButton*, int32>(QuaternaryButton, QuaternaryAction)};
    for (const TPair<UButton*, int32>& Entry : Buttons)
    {
        SetVisible(Entry.Key, Entry.Value != INDEX_NONE);
    }

    const auto LabelForAction = [](const int32 Action) -> FText
    {
        switch (Action)
        {
        case 0: return NSLOCTEXT("BlackMeridian", "NativeUIDeploy", "DEPLOY");
        case 1: return NSLOCTEXT("BlackMeridian", "NativeUICampaign", "CAMPAIGN");
        case 2: return NSLOCTEXT("BlackMeridian", "NativeUISignal", "SIGNAL CONTROL");
        case 3: return NSLOCTEXT("BlackMeridian", "NativeUISettings", "SETTINGS");
        case 4: return NSLOCTEXT("BlackMeridian", "NativeUIControls", "CONTROLS");
        case 5: return NSLOCTEXT("BlackMeridian", "NativeUIHowToPlay", "HOW TO PLAY");
        case 6: return NSLOCTEXT("BlackMeridian", "NativeUIBack", "BACK");
        case 7: return NSLOCTEXT("BlackMeridian", "NativeUIRestart", "RESTART CHECKPOINT");
        case 8: return NSLOCTEXT("BlackMeridian", "NativeUIReturn", "RETURN TO MENU");
        case 9: return NSLOCTEXT("BlackMeridian", "NativeUIResume", "RESUME");
        case 10: return NSLOCTEXT("BlackMeridian", "NativeUINextLevel", "NEXT LEVEL");
        default: return FText::GetEmpty();
        }
    };
    SetText(PrimaryButtonLabel, LabelForAction(PrimaryAction));
    SetText(SecondaryButtonLabel, LabelForAction(SecondaryAction));
    SetText(TertiaryButtonLabel, LabelForAction(TertiaryAction));
    SetText(QuaternaryButtonLabel, LabelForAction(QuaternaryAction));
    RefreshMenuSelectionPresentation();
}

void UBMNativeUIWidget::RefreshMenuSelectionPresentation()
{
    const TArray<TPair<UButton*, int32>> Buttons = {
        TPair<UButton*, int32>(PrimaryButton, PrimaryAction),
        TPair<UButton*, int32>(SecondaryButton, SecondaryAction),
        TPair<UButton*, int32>(TertiaryButton, TertiaryAction),
        TPair<UButton*, int32>(QuaternaryButton, QuaternaryAction)};

    if (FocusedMenuButton == INDEX_NONE)
    {
        for (int32 Index = 0; Index < Buttons.Num(); ++Index)
        {
            if (Buttons[Index].Key && Buttons[Index].Value != INDEX_NONE)
            {
                FocusedMenuButton = Index;
                break;
            }
        }
    }

    for (int32 Index = 0; Index < Buttons.Num(); ++Index)
    {
        UButton* Button = Buttons[Index].Key;
        if (!Button || Buttons[Index].Value == INDEX_NONE)
        {
            continue;
        }

        const bool bFocused = Index == FocusedMenuButton;
        Button->SetBackgroundColor(bFocused
            ? FLinearColor(0.14f, 0.58f, 0.48f, 1.0f)
            : FLinearColor(0.08f, 0.32f, 0.30f, 0.96f));

        UTextBlock* Label = nullptr;
        switch (Index)
        {
        case 0: Label = PrimaryButtonLabel; break;
        case 1: Label = SecondaryButtonLabel; break;
        case 2: Label = TertiaryButtonLabel; break;
        case 3: Label = QuaternaryButtonLabel; break;
        default: break;
        }
        if (Label)
        {
            Label->SetColorAndOpacity(bFocused ? FLinearColor::White : MutedTextColor);
        }
    }
}

void UBMNativeUIWidget::MoveMenuSelection(const int32 Direction)
{
    if (Direction == 0)
    {
        return;
    }

    const TArray<int32> ValidButtons = {
        PrimaryAction != INDEX_NONE ? 0 : INDEX_NONE,
        SecondaryAction != INDEX_NONE ? 1 : INDEX_NONE,
        TertiaryAction != INDEX_NONE ? 2 : INDEX_NONE,
        QuaternaryAction != INDEX_NONE ? 3 : INDEX_NONE};

    TArray<int32> VisibleButtons;
    for (const int32 ButtonIndex : ValidButtons)
    {
        if (ButtonIndex != INDEX_NONE)
        {
            VisibleButtons.Add(ButtonIndex);
        }
    }
    if (VisibleButtons.Num() == 0)
    {
        FocusedMenuButton = INDEX_NONE;
        return;
    }

    int32 CurrentPosition = VisibleButtons.IndexOfByKey(FocusedMenuButton);
    if (CurrentPosition == INDEX_NONE)
    {
        CurrentPosition = 0;
    }
    CurrentPosition = (CurrentPosition + (Direction > 0 ? 1 : -1) + VisibleButtons.Num()) % VisibleButtons.Num();
    FocusedMenuButton = VisibleButtons[CurrentPosition];
    RefreshMenuSelectionPresentation();
}

bool UBMNativeUIWidget::ActivateMenuSelection()
{
    const TArray<int32> Actions = {PrimaryAction, SecondaryAction, TertiaryAction, QuaternaryAction};
    if (!Actions.IsValidIndex(FocusedMenuButton) || Actions[FocusedMenuButton] == INDEX_NONE)
    {
        return false;
    }

    InvokeMenuAction(Actions[FocusedMenuButton]);
    return true;
}

void UBMNativeUIWidget::InvokeMenuAction(const int32 ActionIndex)
{
    ABMPlayerController* Controller = Cast<ABMPlayerController>(GetOwningPlayer());
    if (!Controller || ActionIndex == INDEX_NONE)
    {
        return;
    }

    switch (ActionIndex)
    {
    case 0: Controller->UIStartMission(); break;
    case 1: Controller->UIOpenCampaign(); break;
    case 2: Controller->UIOpenMultiplayer(); break;
    case 3: Controller->UIOpenSettings(); break;
    case 4: Controller->UIOpenControls(); break;
    case 5: Controller->UIOpenHowToPlay(); break;
    case 6: Controller->UIBack(); break;
    case 7: Controller->UIRestartCheckpoint(); break;
    case 8: Controller->UIReturnToMenu(); break;
    case 9: Controller->UITogglePause(); break;
    case 10: Controller->UIStartNextCampaignLevel(); break;
    default: break;
    }
}

void UBMNativeUIWidget::HandlePrimaryClicked()
{
    InvokeMenuAction(PrimaryAction);
}

void UBMNativeUIWidget::HandleSecondaryClicked()
{
    InvokeMenuAction(SecondaryAction);
}

void UBMNativeUIWidget::HandleTertiaryClicked()
{
    InvokeMenuAction(TertiaryAction);
}

void UBMNativeUIWidget::HandleQuaternaryClicked()
{
    InvokeMenuAction(QuaternaryAction);
}

void UBMNativeUIWidget::RefreshFromHUD(const ABMHUD* HUD)
{
    if (HUD)
    {
        RefreshPresentation();
    }
}
