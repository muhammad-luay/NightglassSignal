// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BMLobbyWidget.generated.h"

class ABMGameState;
class UButton;
class UTextBlock;

/**
 * Native UMG lobby presentation for Signal Control.
 *
 * The widget deliberately owns presentation only. Match start still routes
 * through ABMPlayerController and the authoritative game mode, so a future
 * authored Widget Blueprint can replace this class without changing lobby
 * state or network rules.
 */
UCLASS()
class BLACKMERIDIANRUNTIME_API UBMLobbyWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void RefreshFromState(const ABMGameState* State);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UFUNCTION()
    void HandleStartClicked();

    void BuildWidgetTree();
    void RefreshCachedState();
    UTextBlock* MakeTextBlock(const FText& Text, int32 FontSize, const FLinearColor& Color) const;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> RosterSummaryText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> LatticeRosterText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> VantaRosterText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> LobbyStatusText;

    UPROPERTY(Transient)
    TObjectPtr<UButton> StartButton;

    float RefreshAccumulator = 0.0f;
};
