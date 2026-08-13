// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BMCoreTypes.h"
#include "BMInteractionInterface.generated.h"

class AActor;

UINTERFACE(BlueprintType)
class BLACKMERIDIANRUNTIME_API UBMInteractionInterface : public UInterface
{
    GENERATED_BODY()
};

class BLACKMERIDIANRUNTIME_API IBMInteractionInterface
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Black Meridian|Interaction")
    bool CanInteract(AActor* Interactor) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Black Meridian|Interaction")
    void Interact(AActor* Interactor);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Black Meridian|Interaction")
    FText GetInteractionPrompt() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Black Meridian|Interaction")
    EBMInteractionType GetInteractionType() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Black Meridian|Interaction")
    EBMInteractionAction GetInteractionAction() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Black Meridian|Interaction")
    float GetInteractionHoldDuration() const;
};
