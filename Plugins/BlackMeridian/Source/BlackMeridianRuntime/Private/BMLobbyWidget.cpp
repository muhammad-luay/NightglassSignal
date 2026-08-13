// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMLobbyWidget.h"

#include "BMGameState.h"
#include "BMPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
    const FLinearColor PanelColor(0.006f, 0.014f, 0.021f, 0.96f);
    const FLinearColor BorderColor(0.10f, 0.32f, 0.35f, 0.92f);
    const FLinearColor PrimaryTextColor(0.86f, 0.91f, 0.88f, 1.0f);
    const FLinearColor MutedTextColor(0.52f, 0.67f, 0.65f, 1.0f);
    const FLinearColor LatticeColor(0.36f, 0.88f, 1.0f, 1.0f);
    const FLinearColor VantaColor(1.0f, 0.42f, 0.30f, 1.0f);
    const FLinearColor ActionColor(0.16f, 0.62f, 0.58f, 1.0f);

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
        Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));
        Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
    }

    UCanvasPanelSlot* FillCanvasSlot(UCanvasPanel* Canvas, UWidget* Child)
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
}

TSharedRef<SWidget> UBMLobbyWidget::RebuildWidget()
{
    BuildWidgetTree();
    return Super::RebuildWidget();
}

void UBMLobbyWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(false);
    BuildWidgetTree();
    RefreshCachedState();
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian UMG lobby: constructed root=%s roster_summary=%s start_button=%s"),
        WidgetTree && WidgetTree->RootWidget ? TEXT("yes") : TEXT("no"),
        RosterSummaryText ? TEXT("yes") : TEXT("no"),
        StartButton ? TEXT("yes") : TEXT("no"));
}

void UBMLobbyWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshAccumulator += FMath::Max(0.0f, InDeltaTime);
    if (RefreshAccumulator < 0.25f)
    {
        return;
    }

    RefreshAccumulator = 0.0f;
    RefreshCachedState();
#if !UE_BUILD_SHIPPING
    const FVector2D WidgetSize = GetCachedGeometry().GetLocalSize();
    const FVector2D RootSize = WidgetTree && WidgetTree->RootWidget
        ? WidgetTree->RootWidget->GetCachedGeometry().GetLocalSize()
        : FVector2D::ZeroVector;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian UMG lobby: geometry widget=%.0fx%.0f root=%.0fx%.0f visibility=%d"),
        WidgetSize.X,
        WidgetSize.Y,
        RootSize.X,
        RootSize.Y,
        static_cast<int32>(GetVisibility()));
#endif
}

UTextBlock* UBMLobbyWidget::MakeTextBlock(
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
    ApplyTextStyle(TextBlock, FontSize, Color);
    return TextBlock;
}

void UBMLobbyWidget::BuildWidgetTree()
{
    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return;
    }

    UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    // A bare CanvasPanel reports a zero desired size when it is used as the
    // root of a runtime-created UserWidget. The viewport slot can therefore
    // be large while the actual root Slate geometry remains 0x0. A root
    // SizeBox supplies the full-screen desired size; the canvas then fills
    // that box and its anchored backdrop/panel can render normally.
    USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    RootSize->SetWidthOverride(1440.0f);
    RootSize->SetHeightOverride(900.0f);
    RootSize->AddChild(RootCanvas);
    WidgetTree->RootWidget = RootSize;

    UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Backdrop->SetBrushColor(FLinearColor(0.002f, 0.006f, 0.010f, 0.92f));
    FillCanvasSlot(RootCanvas, Backdrop);

    USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    PanelSize->SetWidthOverride(1120.0f);
    PanelSize->SetHeightOverride(650.0f);
    UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(PanelSize);
    PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    // A centered canvas slot must use explicit size/position. Right and
    // bottom in FMargin are width/height when the anchors are coincident;
    // treating them as absolute bounds can leave the native widget clipped
    // or with zero effective geometry on a game viewport.
    PanelSlot->SetPosition(FVector2D(0.0f, 0.0f));
    PanelSlot->SetSize(FVector2D(1120.0f, 650.0f));

    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Panel->SetBrushColor(PanelColor);
    Panel->SetPadding(FMargin(36.0f, 30.0f, 36.0f, 28.0f));
    PanelSize->AddChild(Panel);

    UVerticalBox* MainColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Panel->AddChild(MainColumn);

    UTextBlock* Title = MakeTextBlock(
        NSLOCTEXT("BlackMeridian", "SignalLobbyWidgetTitle", "SIGNAL CONTROL  //  LOBBY"),
        30,
        LatticeColor);
    MainColumn->AddChildToVerticalBox(Title);

    RosterSummaryText = MakeTextBlock(FText::GetEmpty(), 16, MutedTextColor);
    if (UVerticalBoxSlot* SummarySlot = MainColumn->AddChildToVerticalBox(RosterSummaryText))
    {
        SummarySlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 22.0f));
    }

    UHorizontalBox* RosterColumns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    MainColumn->AddChildToVerticalBox(RosterColumns);

    UVerticalBox* LatticeColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    UVerticalBox* VantaColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    if (UHorizontalBoxSlot* LatticeSlot = RosterColumns->AddChildToHorizontalBox(LatticeColumn))
    {
        LatticeSlot->SetSize(ESlateSizeRule::Fill);
        LatticeSlot->SetPadding(FMargin(0.0f, 0.0f, 24.0f, 0.0f));
    }
    if (UHorizontalBoxSlot* VantaSlot = RosterColumns->AddChildToHorizontalBox(VantaColumn))
    {
        VantaSlot->SetSize(ESlateSizeRule::Fill);
        VantaSlot->SetPadding(FMargin(24.0f, 0.0f, 0.0f, 0.0f));
    }

    UTextBlock* LatticeHeader = MakeTextBlock(
        NSLOCTEXT("BlackMeridian", "SignalLobbyWidgetLattice", "LATTICE RESPONSE UNIT"),
        20,
        LatticeColor);
    UTextBlock* VantaHeader = MakeTextBlock(
        NSLOCTEXT("BlackMeridian", "SignalLobbyWidgetVanta", "VANTA CONTINUUM"),
        20,
        VantaColor);
    LatticeColumn->AddChildToVerticalBox(LatticeHeader);
    VantaColumn->AddChildToVerticalBox(VantaHeader);

    LatticeRosterText = MakeTextBlock(FText::GetEmpty(), 17, PrimaryTextColor);
    VantaRosterText = MakeTextBlock(FText::GetEmpty(), 17, PrimaryTextColor);
    if (UVerticalBoxSlot* LatticeRosterSlot = LatticeColumn->AddChildToVerticalBox(LatticeRosterText))
    {
        LatticeRosterSlot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));
    }
    if (UVerticalBoxSlot* VantaRosterSlot = VantaColumn->AddChildToVerticalBox(VantaRosterText))
    {
        VantaRosterSlot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));
    }

    USpacer* RosterSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
    if (UVerticalBoxSlot* SpacerSlot = MainColumn->AddChildToVerticalBox(RosterSpacer))
    {
        SpacerSlot->SetSize(ESlateSizeRule::Fill);
    }

    LobbyStatusText = MakeTextBlock(FText::GetEmpty(), 15, MutedTextColor);
    MainColumn->AddChildToVerticalBox(LobbyStatusText);

    UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    if (UVerticalBoxSlot* FooterSlot = MainColumn->AddChildToVerticalBox(Footer))
    {
        FooterSlot->SetPadding(FMargin(0.0f, 18.0f, 0.0f, 0.0f));
    }

    UTextBlock* FooterHint = MakeTextBlock(
        NSLOCTEXT("BlackMeridian", "SignalLobbyWidgetFooter", "ENTER  START MATCH     ESC  BACK"),
        16,
        PrimaryTextColor);
    if (UHorizontalBoxSlot* HintSlot = Footer->AddChildToHorizontalBox(FooterHint))
    {
        HintSlot->SetSize(ESlateSizeRule::Fill);
        HintSlot->SetVerticalAlignment(VAlign_Center);
    }

    USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    ButtonSize->SetWidthOverride(220.0f);
    ButtonSize->SetHeightOverride(48.0f);
    StartButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    StartButton->SetBackgroundColor(ActionColor);
    StartButton->SetToolTipText(NSLOCTEXT("BlackMeridian", "SignalLobbyStartTooltip", "Start the Signal Control match"));
    UTextBlock* ButtonText = MakeTextBlock(
        NSLOCTEXT("BlackMeridian", "SignalLobbyStartButton", "START MATCH"),
        16,
        FLinearColor::White);
    StartButton->AddChild(ButtonText);
    ButtonSize->AddChild(StartButton);
    if (UHorizontalBoxSlot* ButtonSlot = Footer->AddChildToHorizontalBox(ButtonSize))
    {
        ButtonSlot->SetHorizontalAlignment(HAlign_Right);
    }
    StartButton->OnClicked.AddDynamic(this, &UBMLobbyWidget::HandleStartClicked);
}

void UBMLobbyWidget::RefreshCachedState()
{
    const UWorld* World = GetWorld();
    const ABMGameState* State = World ? World->GetGameState<ABMGameState>() : nullptr;
    RefreshFromState(State);
}

void UBMLobbyWidget::RefreshFromState(const ABMGameState* State)
{
    if (!State || !RosterSummaryText || !LatticeRosterText || !VantaRosterText || !LobbyStatusText)
    {
        return;
    }

    FString LatticeRoster;
    FString VantaRoster;
    int32 LatticeCount = 0;
    int32 VantaCount = 0;
    for (const FBMSignalLobbyMember& Member : State->SignalLobbyMembers)
    {
        const bool bLattice = Member.Team == EBMTeam::Lattice;
        const bool bVanta = Member.Team == EBMTeam::Vanta;
        if (!bLattice && !bVanta)
        {
            continue;
        }

        FString& Roster = bLattice ? LatticeRoster : VantaRoster;
        int32& Count = bLattice ? LatticeCount : VantaCount;
        const FString Status = Member.bIsHost ? TEXT("HOST") : (Member.bIsBot ? TEXT("BOT") : TEXT("READY"));
        if (!Roster.IsEmpty())
        {
            Roster += TEXT("\n");
        }
        Roster += FString::Printf(TEXT("%-6s  %s"), *Status, *Member.DisplayName.Left(24));
        ++Count;
    }

    if (LatticeRoster.IsEmpty())
    {
        LatticeRoster = TEXT("NO OPERATORS ASSIGNED");
    }
    if (VantaRoster.IsEmpty())
    {
        VantaRoster = TEXT("NO OPERATORS ASSIGNED");
    }

    RosterSummaryText->SetText(FText::FromString(FString::Printf(
        TEXT("ROSTER  %02d / %02d     HUMANS  %02d     BOTS  %02d"),
        State->SignalLobbyMembers.Num(),
        FMath::Max(2, State->SignalLobbyCapacity),
        State->SignalLobbyHumanCount,
        State->SignalLobbyBotCount)));
    LatticeRosterText->SetText(FText::FromString(LatticeRoster));
    VantaRosterText->SetText(FText::FromString(VantaRoster));
    LobbyStatusText->SetText(State->SignalControl.bMatchFinished
        ? NSLOCTEXT("BlackMeridian", "SignalLobbyWidgetFinished", "MATCH COMPLETE // RETURN TO MENU")
        : NSLOCTEXT("BlackMeridian", "SignalLobbyWidgetReady", "AWAITING HOST // START WHEN YOUR SQUAD IS READY"));

    if (StartButton)
    {
        StartButton->SetIsEnabled(!State->bMatchLive && !State->SignalControl.bMatchFinished);
    }
}

void UBMLobbyWidget::HandleStartClicked()
{
    if (ABMPlayerController* Controller = Cast<ABMPlayerController>(GetOwningPlayer()))
    {
        Controller->RequestSignalMatchStartFromUI();
    }
}
