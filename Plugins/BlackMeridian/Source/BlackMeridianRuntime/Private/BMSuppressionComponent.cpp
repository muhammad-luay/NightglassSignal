// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMSuppressionComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UBMSuppressionComponent::UBMSuppressionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UBMSuppressionComponent::AddSuppression(const float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0.0f)
    {
        return;
    }
    Suppression = FMath::Clamp(Suppression + Amount, 0.0f, MaxSuppression);
}

void UBMSuppressionComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        Suppression = FMath::Max(0.0f, Suppression - DecayPerSecond * DeltaTime);
    }
}

void UBMSuppressionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UBMSuppressionComponent, Suppression);
}
