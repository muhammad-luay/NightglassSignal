// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMControlNode.h"

#include "BMCombatCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ABMControlNode::ABMControlNode()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = false;
    CaptureVolume = CreateDefaultSubobject<USphereComponent>(TEXT("CaptureVolume"));
    SetRootComponent(CaptureVolume);
    CaptureVolume->InitSphereRadius(430.0f);
    CaptureVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CaptureVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    CaptureVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    CaptureVolume->SetGenerateOverlapEvents(true);

    MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
    MarkerMesh->SetupAttachment(CaptureVolume);
    MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MarkerMesh->SetRelativeScale3D(FVector(1.8f, 1.8f, 0.08f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        MarkerMesh->SetStaticMesh(CubeMesh.Object);
    }
}

void ABMControlNode::BeginPlay()
{
    Super::BeginPlay();
    CaptureVolume->OnComponentBeginOverlap.AddDynamic(this, &ABMControlNode::HandleBeginOverlap);
    CaptureVolume->OnComponentEndOverlap.AddDynamic(this, &ABMControlNode::HandleEndOverlap);
}

void ABMControlNode::SetActive(const bool bNewActive)
{
    bActive = bNewActive;
    MarkerMesh->SetVisibility(bActive, true);
    ForceNetUpdate();
}

EBMTeam ABMControlNode::GetControllingTeam() const
{
    int32 LatticeCount = 0;
    int32 VantaCount = 0;
    TArray<AActor*> CandidateActors;
    for (AActor* Actor : OverlappingActors)
    {
        CandidateActors.AddUnique(Actor);
    }

    // Keep the authoritative capture decision correct when a character is
    // spawned or teleported into the volume between overlap delegate ticks.
    TArray<AActor*> CurrentOverlaps;
    GetOverlappingActors(CurrentOverlaps, ABMCombatCharacter::StaticClass());
    for (AActor* Actor : CurrentOverlaps)
    {
        CandidateActors.AddUnique(Actor);
    }

    for (AActor* Actor : CandidateActors)
    {
        const ABMCombatCharacter* Character = Cast<ABMCombatCharacter>(Actor);
        if (!Character || Character->IsEliminated())
        {
            continue;
        }
        if (Character->GetTeam() == EBMTeam::Lattice)
        {
            ++LatticeCount;
        }
        else if (Character->GetTeam() == EBMTeam::Vanta)
        {
            ++VantaCount;
        }
    }
    if (LatticeCount == VantaCount)
    {
        return EBMTeam::Neutral;
    }
    return LatticeCount > VantaCount ? EBMTeam::Lattice : EBMTeam::Vanta;
}

bool ABMControlNode::ContainsCharacter(const ABMCombatCharacter* Character) const
{
    if (!Character || Character->IsEliminated())
    {
        return false;
    }

    // A pawn can be spawned or teleported into a node after the overlap
    // delegate has fired. Query the component as well as the cached set so
    // bot takeover and respawn placement immediately use the real volume.
    if (OverlappingActors.Contains(Character))
    {
        return true;
    }

    TArray<AActor*> CurrentOverlaps;
    GetOverlappingActors(CurrentOverlaps, ABMCombatCharacter::StaticClass());
    return CurrentOverlaps.Contains(const_cast<ABMCombatCharacter*>(Character));
}

void ABMControlNode::OnRep_Active()
{
    MarkerMesh->SetVisibility(bActive, true);
}

void ABMControlNode::HandleBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    const int32 OtherBodyIndex,
    const bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor->IsA<ABMCombatCharacter>() && !OverlappingActors.Contains(OtherActor))
    {
        OverlappingActors.Add(OtherActor);
    }
}

void ABMControlNode::HandleEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    const int32 OtherBodyIndex)
{
    OverlappingActors.Remove(OtherActor);
}

void ABMControlNode::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABMControlNode, bActive);
    DOREPLIFETIME(ABMControlNode, NodeIndex);
    DOREPLIFETIME(ABMControlNode, ControllingTeam);
}
