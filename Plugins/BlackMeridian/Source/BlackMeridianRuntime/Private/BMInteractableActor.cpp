// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMInteractableActor.h"

#include "BMCombatCharacter.h"
#include "BMGameState.h"
#include "BMGameMode.h"
#include "BMPlayerCharacter.h"
#include "BMPlayerController.h"
#include "BMWeapon.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    UMaterialInterface* LoadPresentationMaterial()
    {
        if (UMaterialInterface* BasicShapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
        {
            return BasicShapeMaterial;
        }
        return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
    }
}

ABMInteractableActor::ABMInteractableActor()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.10f;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        Mesh->SetStaticMesh(CubeMesh.Object);
    }
    Prompt = NSLOCTEXT("BlackMeridian", "DefaultInteractPrompt", "HOLD E  INTERACT");
}

void ABMInteractableActor::SetWeaponPickupPresentation(const EBMWeaponId WeaponId)
{
    if (!Mesh)
    {
        return;
    }

    const FVector PickupWorldLocation = GetActorLocation();
    const FRotator PickupWorldRotation = GetActorRotation();

    const TCHAR* MeshPath = nullptr;
    switch (WeaponId)
    {
    case EBMWeaponId::Kestrel9:
        MeshPath = TEXT("/Game/Weapons/Meshes/SM_BM_Kestrel9_SMG.SM_BM_Kestrel9_SMG");
        break;
    case EBMWeaponId::RookDMR:
        MeshPath = TEXT("/Game/Weapons/Meshes/SM_BM_Rook_DMR.SM_BM_Rook_DMR");
        break;
    case EBMWeaponId::Breach12:
        MeshPath = TEXT("/Game/Weapons/Meshes/SM_BM_Breach12_Shotgun.SM_BM_Breach12_Shotgun");
        break;
    case EBMWeaponId::SolacePistol:
        MeshPath = TEXT("/Game/Weapons/Meshes/SM_BM_Solace_Pistol.SM_BM_Solace_Pistol");
        break;
    case EBMWeaponId::ArcLauncher:
        MeshPath = TEXT("/Game/Weapons/Meshes/SM_BM_Arc7_Launcher.SM_BM_Arc7_Launcher");
        break;
    case EBMWeaponId::ARX4:
    default:
        MeshPath = TEXT("/Game/Weapons/Meshes/SM_BM_ARX4_Rifle.SM_BM_ARX4_Rifle");
        break;
    }

    if (UStaticMesh* WeaponMesh = LoadObject<UStaticMesh>(nullptr, MeshPath))
    {
        Mesh->SetStaticMesh(WeaponMesh);
        Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 30.0f));
        Mesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
        Mesh->SetRelativeScale3D(FVector(0.82f));
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SetActorScale3D(FVector::OneVector);
        SetActorLocationAndRotation(
            PickupWorldLocation,
            PickupWorldRotation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
    }
}

void ABMInteractableActor::SetThrowablePickupPresentation(const EBMWeaponId ThrowableId)
{
    if (!Mesh)
    {
        return;
    }

    const FVector PickupWorldLocation = GetActorLocation();
    const FRotator PickupWorldRotation = GetActorRotation();
    const TCHAR* MeshPath = ThrowableId == EBMWeaponId::VeilGrenade
        ? TEXT("/Game/Weapons/Meshes/SM_BM_Veil_Grenade.SM_BM_Veil_Grenade")
        : TEXT("/Game/Weapons/Meshes/SM_BM_Frag_Grenade.SM_BM_Frag_Grenade");

    if (UStaticMesh* ThrowableMesh = LoadObject<UStaticMesh>(nullptr, MeshPath))
    {
        Mesh->SetStaticMesh(ThrowableMesh);
        Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 28.0f));
        Mesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
        Mesh->SetRelativeScale3D(FVector(1.45f));
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SetActorScale3D(FVector::OneVector);
        SetActorLocationAndRotation(
            PickupWorldLocation,
            PickupWorldRotation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
    }
}

void ABMInteractableActor::SetAttachmentPickupPresentation(const FName AttachmentId)
{
    if (!Mesh)
    {
        return;
    }

    UStaticMesh* PickupMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    const bool bMuzzleAttachment = AttachmentId == TEXT("Muzzle_Suppressor");
    if (bMuzzleAttachment)
    {
        if (UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
        {
            PickupMesh = CylinderMesh;
        }
    }
    if (!PickupMesh)
    {
        return;
    }

    const FVector PickupWorldLocation = GetActorLocation();
    const FRotator PickupWorldRotation = GetActorRotation();
    Mesh->SetStaticMesh(PickupMesh);
    Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 28.0f));
    Mesh->SetRelativeRotation(bMuzzleAttachment ? FRotator(90.0f, 0.0f, 0.0f) : FRotator::ZeroRotator);
    Mesh->SetRelativeScale3D(bMuzzleAttachment ? FVector(0.30f, 0.30f, 0.90f) : FVector(0.52f, 0.52f, 0.52f));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetActorScale3D(FVector::OneVector);
    SetActorLocationAndRotation(
        PickupWorldLocation,
        PickupWorldRotation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
}

void ABMInteractableActor::SetPresentationColor(const FLinearColor& Color)
{
    if (!Mesh)
    {
        return;
    }

    UMaterialInterface* BaseMaterial = LoadPresentationMaterial();
    if (BaseMaterial)
    {
        if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMaterial))
        {
            Material->SetVectorParameterValue(TEXT("Color"), Color);
            Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
            Material->SetScalarParameterValue(TEXT("Roughness"), 0.82f);
        }
    }
}

bool ABMInteractableActor::IsAvailableForMissionPhase(const EBMMissionPhase MissionPhase) const
{
    if (InteractionId == TEXT("RelayBreach"))
    {
        return MissionPhase == EBMMissionPhase::InteriorBreach;
    }
    if (InteractionId == TEXT("IntelConsole"))
    {
        return MissionPhase == EBMMissionPhase::Intelligence;
    }
    if (InteractionId == TEXT("ExtractionBoat"))
    {
        return MissionPhase == EBMMissionPhase::Extraction;
    }
    if (InteractionId == TEXT("PlantSignalJammer"))
    {
        return MissionPhase == EBMMissionPhase::OpenCombat;
    }
    return true;
}

bool ABMInteractableActor::CanInteract_Implementation(AActor* Interactor) const
{
    if (!IsValid(Interactor) || IsActorBeingDestroyed() || FVector::DistSquared(Interactor->GetActorLocation(), GetActorLocation()) > FMath::Square(360.0f))
    {
        return false;
    }

    if (InteractionAction == EBMInteractionAction::ReviveAlly)
    {
        const ABMCombatCharacter* Target = Cast<ABMCombatCharacter>(ReviveTarget);
        return Target && Target->IsEliminated();
    }

    if (const ABMGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr)
    {
        if (!IsAvailableForMissionPhase(GameState->MissionPhase))
        {
            return false;
        }
    }

    return true;
}

void ABMInteractableActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    (void)DeltaSeconds;

    if (!HasAuthority() || !bAutoPickup || bAutoPickupInProgress ||
        (InteractionAction != EBMInteractionAction::PickupWeapon &&
         InteractionAction != EBMInteractionAction::PickupAmmo &&
         InteractionAction != EBMInteractionAction::PickupThrowable &&
         InteractionAction != EBMInteractionAction::PickupAttachment) ||
        IsActorBeingDestroyed() || !GetWorld())
    {
        return;
    }

    for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
    {
        APlayerController* Controller = Iterator->Get();
        ABMPlayerCharacter* Player = Controller ? Cast<ABMPlayerCharacter>(Controller->GetPawn()) : nullptr;
        if (!Player || Player->IsEliminated() || !CanInteract_Implementation(Player) ||
            FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) > FMath::Square(AutoPickupRadius))
        {
            continue;
        }

        bAutoPickupInProgress = true;
        PerformInteraction(Player);
        bAutoPickupInProgress = false;
        break;
    }
}

void ABMInteractableActor::Interact_Implementation(AActor* Interactor)
{
    if (!HasAuthority())
    {
        return;
    }
    PerformInteraction(Interactor);
}

void ABMInteractableActor::PerformInteraction(AActor* Interactor)
{
    bool bInteractionCompleted = true;
    if (InteractionAction == EBMInteractionAction::Ladder)
    {
        if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(Interactor))
        {
            Player->BeginLadderClimb(this);
        }
        else
        {
            bInteractionCompleted = false;
        }
    }
    else if (InteractionAction == EBMInteractionAction::ReviveAlly)
    {
        if (ABMCombatCharacter* Target = Cast<ABMCombatCharacter>(ReviveTarget))
        {
            if (Target->IsEliminated())
            {
                Target->RestoreForCheckpoint(35.0f);
            }
            else
            {
                bInteractionCompleted = false;
            }
        }
        else
        {
            bInteractionCompleted = false;
        }
    }
    else if (InteractionAction == EBMInteractionAction::MountedWeapon)
    {
        if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(Interactor))
        {
            Player->BeginMountedWeapon(this);
        }
        else
        {
            bInteractionCompleted = false;
        }
    }
    else if (InteractionAction == EBMInteractionAction::PickupWeapon)
    {
        if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(Interactor))
        {
            const bool bWeaponGranted = Player->AddWeaponToLoadout(PickupWeaponId);
            const bool bAmmoGranted = bWeaponGranted &&
                (PickupAmmoAmount <= 0 || Player->AddAmmoToWeapon(PickupWeaponId, PickupAmmoAmount));
            bInteractionCompleted = bWeaponGranted && bAmmoGranted;
            if (bWeaponGranted)
            {
                Player->EquipWeaponById(PickupWeaponId);
            }
        }
        else
        {
            bInteractionCompleted = false;
        }
    }
    else if (InteractionAction == EBMInteractionAction::PickupAmmo)
    {
        if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(Interactor))
        {
            bInteractionCompleted = Player->AddReserveAmmo(PickupAmmoAmount);
        }
        else
        {
            bInteractionCompleted = false;
        }
    }
    else if (InteractionAction == EBMInteractionAction::PickupThrowable)
    {
        if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(Interactor))
        {
            bInteractionCompleted = Player->AddThrowableInventory(PickupFragAmount, PickupVeilAmount);
        }
        else
        {
            bInteractionCompleted = false;
        }
    }
    else if (InteractionAction == EBMInteractionAction::PickupAttachment)
    {
        if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(Interactor))
        {
            bInteractionCompleted = Player->EquipAttachmentById(PickupAttachmentId);
        }
        else
        {
            bInteractionCompleted = false;
        }
    }

    if (bActsAsDoor || InteractionAction == EBMInteractionAction::OpenDoor || InteractionAction == EBMInteractionAction::BreachDoor)
    {
        SetActorRotation(GetActorRotation() + FRotator(0.0f, 90.0f, 0.0f));
    }
    if (bInteractionCompleted)
    {
        if (ABMPlayerCharacter* Player = Cast<ABMPlayerCharacter>(Interactor))
        {
            if (ABMPlayerController* Controller = Cast<ABMPlayerController>(Player->GetController()))
            {
                FText PickupMessage = FText::GetEmpty();
                if (InteractionAction == EBMInteractionAction::PickupWeapon)
                {
                    PickupMessage = FText::FromString(FString::Printf(TEXT("WEAPON ACQUIRED  //  AMMO +%d"), PickupAmmoAmount));
                }
                else if (InteractionAction == EBMInteractionAction::PickupAmmo)
                {
                    PickupMessage = FText::FromString(FString::Printf(TEXT("AMMUNITION ACQUIRED  //  +%d"), PickupAmmoAmount));
                }
                else if (InteractionAction == EBMInteractionAction::PickupThrowable)
                {
                    PickupMessage = FText::FromString(FString::Printf(
                        TEXT("ORDNANCE ACQUIRED  //  FRAG +%d  VEIL +%d"),
                        PickupFragAmount,
                        PickupVeilAmount));
                }
                else if (InteractionAction == EBMInteractionAction::PickupAttachment)
                {
                    PickupMessage = FText::FromString(FString::Printf(
                        TEXT("ATTACHMENT ACQUIRED  //  %s"),
                        *PickupAttachmentId.ToString().Replace(TEXT("_"), TEXT(" "))));
                }
                if (!PickupMessage.IsEmpty())
                {
                    Controller->ShowSubtitle(PickupMessage, 2.0f);
                }
            }
        }
        if (ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>())
        {
            GameMode->HandleInteraction(Interactor, InteractionId);
        }
    }

    if (bInteractionCompleted && bDestroyAfterInteraction)
    {
        Destroy();
    }
}

void ABMInteractableActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABMInteractableActor, InteractionId);
    DOREPLIFETIME(ABMInteractableActor, Prompt);
    DOREPLIFETIME(ABMInteractableActor, InteractionType);
    DOREPLIFETIME(ABMInteractableActor, InteractionAction);
    DOREPLIFETIME(ABMInteractableActor, HoldDuration);
    DOREPLIFETIME(ABMInteractableActor, PickupWeaponId);
    DOREPLIFETIME(ABMInteractableActor, PickupAmmoAmount);
    DOREPLIFETIME(ABMInteractableActor, PickupFragAmount);
    DOREPLIFETIME(ABMInteractableActor, PickupVeilAmount);
    DOREPLIFETIME(ABMInteractableActor, PickupAttachmentId);
    DOREPLIFETIME(ABMInteractableActor, bAutoPickup);
    DOREPLIFETIME(ABMInteractableActor, AutoPickupRadius);
    DOREPLIFETIME(ABMInteractableActor, ReviveTarget);
    DOREPLIFETIME(ABMInteractableActor, bDestroyAfterInteraction);
    DOREPLIFETIME(ABMInteractableActor, bActsAsDoor);
}
