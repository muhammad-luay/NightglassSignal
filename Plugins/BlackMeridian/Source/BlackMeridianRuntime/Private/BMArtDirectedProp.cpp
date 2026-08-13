// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMArtDirectedProp.h"

#include "BMAudioDataAsset.h"
#include "BMVFXDataAsset.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    UStaticMesh* LoadEnvironmentMesh(const TCHAR* ProjectPath, const TCHAR* FallbackPath)
    {
        if (UStaticMesh* ProjectMesh = LoadObject<UStaticMesh>(nullptr, ProjectPath))
        {
            return ProjectMesh;
        }
        return LoadObject<UStaticMesh>(nullptr, FallbackPath);
    }

    UMaterialInterface* LoadPresentationMaterial()
    {
        if (UMaterialInterface* IndustrialMaterial = LoadObject<UMaterialInterface>(
                nullptr,
                TEXT("/Game/Environment/Materials/MI_BM_IndustrialSurface.MI_BM_IndustrialSurface")))
        {
            return IndustrialMaterial;
        }
        if (UMaterialInterface* BasicShapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
        {
            return BasicShapeMaterial;
        }
        return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
    }

    void RemoveSurfaceTags(AActor* Actor)
    {
        if (!Actor)
        {
            return;
        }

        Actor->Tags.Remove(TEXT("Surface.Metal"));
        Actor->Tags.Remove(TEXT("Surface.Wood"));
        Actor->Tags.Remove(TEXT("Surface.SoftCover"));
        Actor->Tags.Remove(TEXT("ThinCover"));
    }
}

ABMArtDirectedProp::ABMArtDirectedProp()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    SetNetUpdateFrequency(20.0f);
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    UStaticMesh* CubeMesh = LoadEnvironmentMesh(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Block.SM_BM_Env_Block"),
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    for (int32 Index = 0; Index < 8; ++Index)
    {
        const FName ComponentName(*FString::Printf(TEXT("MainPart_%02d"), Index));
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(ComponentName);
        Part->SetupAttachment(SceneRoot);
        Part->SetStaticMesh(CubeMesh);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
        Part->SetGenerateOverlapEvents(false);
        Part->SetCanEverAffectNavigation(false);
        Part->SetMobility(EComponentMobility::Movable);
        MainParts.Add(Part);
    }

    for (int32 Index = 0; Index < 5; ++Index)
    {
        const FName ComponentName(*FString::Printf(TEXT("DebrisPart_%02d"), Index));
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(ComponentName);
        Part->SetupAttachment(SceneRoot);
        Part->SetStaticMesh(CubeMesh);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
        Part->SetGenerateOverlapEvents(false);
        Part->SetCanEverAffectNavigation(false);
        Part->SetMobility(EComponentMobility::Movable);
        DebrisParts.Add(Part);
    }

    DestructionLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("DestructionLight"));
    DestructionLight->SetupAttachment(SceneRoot);
    DestructionLight->SetMobility(EComponentMobility::Movable);
    DestructionLight->SetCastShadows(false);
    DestructionLight->SetAttenuationRadius(260.0f);
    DestructionLight->SetIntensity(0.0f);
    DestructionLight->SetVisibility(false, true);

    ImpactLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ImpactLight"));
    ImpactLight->SetupAttachment(SceneRoot);
    ImpactLight->SetMobility(EComponentMobility::Movable);
    ImpactLight->SetCastShadows(false);
    ImpactLight->SetAttenuationRadius(230.0f);
    ImpactLight->SetIntensity(0.0f);
    ImpactLight->SetVisibility(false, true);

    SignalBeaconLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("SignalBeaconLight"));
    SignalBeaconLight->SetupAttachment(SceneRoot);
    SignalBeaconLight->SetRelativeLocation(FVector(0.0f, 0.0f, 570.0f));
    SignalBeaconLight->SetMobility(EComponentMobility::Movable);
    SignalBeaconLight->SetCastShadows(false);
    SignalBeaconLight->SetAttenuationRadius(460.0f);
    SignalBeaconLight->SetIntensity(0.0f);
    SignalBeaconLight->SetVisibility(false, true);
}

void ABMArtDirectedProp::BeginPlay()
{
    Super::BeginPlay();
    RebuildPresentation();
}

void ABMArtDirectedProp::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildPresentation();
}

void ABMArtDirectedProp::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bDestructionPresentationActive)
    {
        if (Variant == EBMArtDirectedPropVariant::SignalTower
            && !bDestroyed
            && SignalBeaconLight)
        {
            // A restrained pulse gives the route landmark life without
            // turning it into a navigation billboard. All towers share the
            // same phase so replicated clients see the same rhythm.
            const float Pulse = 0.80f
                + 0.20f * (0.5f + 0.5f * FMath::Sin(GetWorld() ? GetWorld()->GetTimeSeconds() * 4.2f : 0.0f));
            SignalBeaconLight->SetLightColor(AccentColor);
            SignalBeaconLight->SetIntensity(1350.0f * Pulse);
            SignalBeaconLight->SetVisibility(true, true);
        }
        return;
    }

    DestructionPresentationElapsed += FMath::Max(0.0f, DeltaSeconds);
    constexpr float PresentationDuration = 0.82f;
    const float Alpha = FMath::Clamp(
        DestructionPresentationElapsed / PresentationDuration,
        0.0f,
        1.0f);
    const float EaseAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);

    for (int32 Index = 0; Index < DebrisParts.Num(); ++Index)
    {
        UStaticMeshComponent* Debris = DebrisParts[Index];
        if (!Debris || !DebrisBaseLocations.IsValidIndex(Index))
        {
            continue;
        }

        // Each shard gets a deterministic impulse derived from its slot. The
        // result reads as a designed break-apart event and remains identical
        // on every replicated client without a physics simulation.
        const float Side = Index % 2 == 0 ? 1.0f : -1.0f;
        const float Forward = Index % 3 == 0 ? 1.0f : (Index % 3 == 1 ? -0.65f : 0.35f);
        const FVector Impulse(
            Side * (34.0f + Index * 7.0f),
            Forward * (26.0f + Index * 5.0f),
            72.0f + Index * 11.0f);
        const FVector Fall(0.0f, 0.0f, -118.0f * EaseAlpha * EaseAlpha);
        Debris->SetRelativeLocation(DebrisBaseLocations[Index] + Impulse * EaseAlpha + Fall);
        Debris->SetRelativeRotation(
            DebrisBaseRotations[Index]
            + FRotator(
                Side * (42.0f + Index * 17.0f) * EaseAlpha,
                Forward * (28.0f + Index * 13.0f) * EaseAlpha,
                Side * (72.0f + Index * 21.0f) * EaseAlpha));
        Debris->SetRelativeScale3D(DebrisBaseScales[Index] * (1.0f - 0.16f * EaseAlpha));
    }

    if (DestructionLight)
    {
        DestructionLight->SetIntensity(2600.0f * (1.0f - Alpha));
        DestructionLight->SetVisibility(Alpha < 1.0f, true);
    }

    if (Alpha >= 1.0f)
    {
        for (UStaticMeshComponent* Debris : DebrisParts)
        {
            if (Debris)
            {
                Debris->SetVisibility(false, true);
                Debris->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
        }
        if (DestructionLight)
        {
            DestructionLight->SetIntensity(0.0f);
            DestructionLight->SetVisibility(false, true);
        }
        bDestructionPresentationActive = false;
        SetActorTickEnabled(false);
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian prop: destruction presentation complete prop=%s debris=%d"),
            *GetNameSafe(this),
            DebrisParts.Num());
    }
}

void ABMArtDirectedProp::ConfigureRuntime(
    const EBMArtDirectedPropVariant InVariant,
    const FLinearColor& InBaseColor,
    const FLinearColor& InAccentColor,
    const bool bInDestructible)
{
    Variant = InVariant;
    BaseColor = InBaseColor;
    AccentColor = InAccentColor;
    bDestructible = bInDestructible;
    RemainingDestructionHealth = 100.0f;
    bDestroyed = false;
    bDestructionPresentationActive = false;
    DestructionPresentationElapsed = 0.0f;
    SetActorTickEnabled(InVariant == EBMArtDirectedPropVariant::SignalTower);
    RebuildPresentation();

    if (HasAuthority())
    {
        ForceNetUpdate();
    }
}

void ABMArtDirectedProp::TriggerDestruction()
{
    if (!bDestructible || bDestroyed)
    {
        return;
    }

    // The director calls this on authority and replicated clients replay the
    // same presentation from OnRep_Destroyed. Updating the local state here
    // makes the authoritative listen-server view immediate while the
    // replicated value remains authoritative for cover validity.
    bDestroyed = true;
    bDestructionPresentationActive = true;
    DestructionPresentationElapsed = 0.0f;
    SetActorTickEnabled(true);
    if (DestructionLight)
    {
        DestructionLight->SetWorldLocation(GetActorLocation() + FVector(0.0f, 0.0f, 86.0f));
        DestructionLight->SetLightColor(AccentColor);
        DestructionLight->SetIntensity(4200.0f);
        DestructionLight->SetAttenuationRadius(360.0f);
        DestructionLight->SetVisibility(true, true);
    }
    ClearImpactFeedback();
    ApplyVisualState();
    PlayDestructionFeedback(GetActorLocation() + FVector(0.0f, 0.0f, 86.0f), FVector::UpVector);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian prop: destruction started prop=%s variant=%d debris=%d"),
        *GetNameSafe(this),
        static_cast<int32>(Variant),
        DebrisParts.Num());
    if (HasAuthority())
    {
        ForceNetUpdate();
    }
}

bool ABMArtDirectedProp::ApplyWeaponImpact(const float Damage)
{
    return ApplyWeaponImpactAtLocation(
        Damage,
        GetActorLocation() + FVector(0.0f, 0.0f, 86.0f),
        FVector::UpVector);
}

bool ABMArtDirectedProp::ApplyWeaponImpactAtLocation(
    const float Damage,
    const FVector& ImpactPoint,
    const FVector& ImpactNormal)
{
    // Prop destruction is an authority-owned world mutation. Clients may
    // receive the replicated bDestroyed state, but cannot remove cover by
    // submitting a local visual-only hit.
    if (!HasAuthority() || !bDestructible || bDestroyed || Damage <= 0.0f)
    {
        return false;
    }

    const float PreviousHealth = RemainingDestructionHealth;
    RemainingDestructionHealth = FMath::Max(0.0f, RemainingDestructionHealth - Damage);
    const FVector SafeImpactPoint = ImpactPoint.IsNearlyZero()
        ? GetActorLocation() + FVector(0.0f, 0.0f, 86.0f)
        : ImpactPoint;
    const FVector SafeImpactNormal = ImpactNormal.GetSafeNormal().IsNearlyZero()
        ? FVector::UpVector
        : ImpactNormal.GetSafeNormal();
    // The server owns the cover health, but every client needs the same
    // short-lived impact burst at the actual hit point. This keeps rounds
    // readable against physical cover without replicating transient VFX
    // state or making clients author destruction.
    MulticastImpactFeedback(SafeImpactPoint, SafeImpactNormal);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian prop: weapon impact prop=%s damage=%.1f health=%.1f->%.1f"),
        *GetNameSafe(this),
        Damage,
        PreviousHealth,
        RemainingDestructionHealth);
    if (RemainingDestructionHealth <= 0.0f)
    {
        TriggerDestruction();
        return true;
    }

    ForceNetUpdate();
    return false;
}

void ABMArtDirectedProp::ResetProp()
{
    ClearImpactFeedback();
    bDestroyed = false;
    RemainingDestructionHealth = 100.0f;
    bDestructionPresentationActive = false;
    DestructionPresentationElapsed = 0.0f;
    SetActorTickEnabled(Variant == EBMArtDirectedPropVariant::SignalTower);
    if (DestructionLight)
    {
        DestructionLight->SetIntensity(0.0f);
        DestructionLight->SetVisibility(false, true);
    }
    if (SignalBeaconLight)
    {
        SignalBeaconLight->SetLightColor(AccentColor);
        SignalBeaconLight->SetIntensity(Variant == EBMArtDirectedPropVariant::SignalTower ? 1350.0f : 0.0f);
        SignalBeaconLight->SetVisibility(Variant == EBMArtDirectedPropVariant::SignalTower, true);
    }
    for (int32 Index = 0; Index < DebrisParts.Num(); ++Index)
    {
        if (DebrisParts[Index] && DebrisBaseLocations.IsValidIndex(Index))
        {
            DebrisParts[Index]->SetRelativeLocation(DebrisBaseLocations[Index]);
            DebrisParts[Index]->SetRelativeRotation(DebrisBaseRotations[Index]);
            DebrisParts[Index]->SetRelativeScale3D(DebrisBaseScales[Index]);
        }
    }
    ApplyVisualState();
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian prop: reset prop=%s restored=%s"),
        *GetNameSafe(this),
        bDestroyed ? TEXT("no") : TEXT("yes"));
    if (HasAuthority())
    {
        ForceNetUpdate();
    }
}

void ABMArtDirectedProp::OnRep_PropPresentation()
{
    RebuildPresentation();
}

void ABMArtDirectedProp::OnRep_Destroyed()
{
    if (bDestroyed)
    {
        ClearImpactFeedback();
        bDestructionPresentationActive = true;
        DestructionPresentationElapsed = 0.0f;
        SetActorTickEnabled(true);
        if (DestructionLight)
        {
            DestructionLight->SetWorldLocation(GetActorLocation() + FVector(0.0f, 0.0f, 86.0f));
            DestructionLight->SetLightColor(AccentColor);
            DestructionLight->SetIntensity(4200.0f);
            DestructionLight->SetAttenuationRadius(360.0f);
            DestructionLight->SetVisibility(true, true);
        }
        PlayDestructionFeedback(GetActorLocation() + FVector(0.0f, 0.0f, 86.0f), FVector::UpVector);
    }
    else
    {
        SetActorTickEnabled(Variant == EBMArtDirectedPropVariant::SignalTower);
    }
    ApplyVisualState();
}

void ABMArtDirectedProp::MulticastImpactFeedback_Implementation(
    const FVector_NetQuantize ImpactPoint,
    const FVector_NetQuantizeNormal ImpactNormal)
{
    PlayImpactFeedback(FVector(ImpactPoint), FVector(ImpactNormal));
}

void ABMArtDirectedProp::PlayImpactFeedback(
    const FVector& ImpactPoint,
    const FVector& ImpactNormal)
{
    if (GetNetMode() == NM_DedicatedServer || !GetWorld())
    {
        return;
    }

    const bool bMetallic = Variant != EBMArtDirectedPropVariant::Crate
        && Variant != EBMArtDirectedPropVariant::Barricade
        && Variant != EBMArtDirectedPropVariant::Furniture
        && Variant != EBMArtDirectedPropVariant::Vegetation;
    const FVector SafeNormal = ImpactNormal.GetSafeNormal().IsNearlyZero()
        ? FVector::UpVector
        : ImpactNormal.GetSafeNormal();
    const FRotator ImpactRotation = SafeNormal.Rotation();

    UBMVFXDataAsset* VFXProfile = LoadObject<UBMVFXDataAsset>(
        nullptr,
        TEXT("/Game/Data/VFX/DA_BM_CombatVFX.DA_BM_CombatVFX"));
    UNiagaraSystem* ImpactSystem = VFXProfile
        ? (bMetallic ? VFXProfile->Sparks.LoadSynchronous() : VFXProfile->Dust.LoadSynchronous())
        : nullptr;
    if (!ImpactSystem)
    {
        ImpactSystem = LoadObject<UNiagaraSystem>(
            nullptr,
            bMetallic
                ? TEXT("/Game/VFX/Generated/NS_BM_Sparks.NS_BM_Sparks")
                : TEXT("/Game/VFX/Generated/NS_BM_Dust.NS_BM_Dust"));
    }
    if (ImpactSystem)
    {
        if (UNiagaraComponent* ImpactComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                GetWorld(),
                ImpactSystem,
                ImpactPoint,
                ImpactRotation,
                bMetallic ? FVector(0.42f) : FVector(0.30f),
                true,
                true,
                ENCPoolMethod::AutoRelease,
                true))
        {
            ImpactComponent->SetVariableLinearColor(TEXT("User.AccentColor"), AccentColor);
        }
    }

    if (ImpactLight)
    {
        const FLinearColor ImpactColor = bMetallic
            ? FLinearColor(1.0f, 0.42f, 0.05f, 1.0f)
            : FLinearColor(1.0f, 0.14f, 0.025f, 1.0f);
        ImpactLight->SetWorldLocation(ImpactPoint + SafeNormal * 3.0f);
        ImpactLight->SetLightColor(ImpactColor);
        ImpactLight->SetAttenuationRadius(bMetallic ? 170.0f : 135.0f);
        ImpactLight->SetIntensity(bMetallic ? 1050.0f : 760.0f);
        ImpactLight->SetVisibility(true, true);
        GetWorldTimerManager().ClearTimer(ImpactFeedbackTimerHandle);
        GetWorldTimerManager().SetTimer(
            ImpactFeedbackTimerHandle,
            this,
            &ABMArtDirectedProp::ClearImpactFeedback,
            bMetallic ? 0.12f : 0.10f,
            false);
    }

    USoundBase* ImpactSound = nullptr;
    if (UBMAudioDataAsset* AudioProfile = LoadObject<UBMAudioDataAsset>(
            nullptr,
            TEXT("/Game/Data/Audio/DA_BM_CombatAudio.DA_BM_CombatAudio")))
    {
        ImpactSound = AudioProfile->BulletImpact.LoadSynchronous();
    }
    if (!ImpactSound)
    {
        ImpactSound = LoadObject<USoundBase>(
            nullptr,
            bMetallic
                ? TEXT("/Game/Audio/Generated/BM_Impact_Metal.BM_Impact_Metal")
                : TEXT("/Game/Audio/Generated/BM_Impact_Concrete.BM_Impact_Concrete"));
    }
    if (ImpactSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, ImpactPoint, bMetallic ? 0.46f : 0.38f);
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian VFX: event=prop_impact prop=%s surface=%s system=%s sound=%s point=%s"),
        *GetNameSafe(this),
        bMetallic ? TEXT("metal") : TEXT("soft_cover"),
        *GetNameSafe(ImpactSystem),
        *GetNameSafe(ImpactSound),
        *ImpactPoint.ToString());
}

void ABMArtDirectedProp::PlayDestructionFeedback(
    const FVector& ImpactPoint,
    const FVector& ImpactNormal)
{
    if (GetNetMode() == NM_DedicatedServer || !GetWorld())
    {
        return;
    }

    const FVector SafeNormal = ImpactNormal.GetSafeNormal().IsNearlyZero()
        ? FVector::UpVector
        : ImpactNormal.GetSafeNormal();
    const FRotator ImpactRotation = SafeNormal.Rotation();
    UBMVFXDataAsset* VFXProfile = LoadObject<UBMVFXDataAsset>(
        nullptr,
        TEXT("/Game/Data/VFX/DA_BM_CombatVFX.DA_BM_CombatVFX"));
    UNiagaraSystem* Explosion = VFXProfile ? VFXProfile->Explosion.LoadSynchronous() : nullptr;
    UNiagaraSystem* Smoke = VFXProfile ? VFXProfile->Smoke.LoadSynchronous() : nullptr;
    if (!Explosion)
    {
        Explosion = LoadObject<UNiagaraSystem>(
            nullptr,
            TEXT("/Game/VFX/Generated/NS_BM_Explosion.NS_BM_Explosion"));
    }
    if (!Smoke)
    {
        Smoke = LoadObject<UNiagaraSystem>(
            nullptr,
            TEXT("/Game/VFX/Generated/NS_BM_Smoke.NS_BM_Smoke"));
    }
    if (Explosion)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(),
            Explosion,
            ImpactPoint,
            ImpactRotation,
            FVector(0.78f),
            true,
            true,
            ENCPoolMethod::AutoRelease,
            true);
    }
    if (Smoke)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(),
            Smoke,
            ImpactPoint + SafeNormal * 18.0f,
            ImpactRotation,
            FVector(0.62f),
            true,
            true,
            ENCPoolMethod::AutoRelease,
            true);
    }

    USoundBase* ExplosionSound = nullptr;
    if (UBMAudioDataAsset* AudioProfile = LoadObject<UBMAudioDataAsset>(
            nullptr,
            TEXT("/Game/Data/Audio/DA_BM_CombatAudio.DA_BM_CombatAudio")))
    {
        ExplosionSound = AudioProfile->Explosion.LoadSynchronous();
    }
    if (!ExplosionSound)
    {
        ExplosionSound = LoadObject<USoundBase>(
            nullptr,
            TEXT("/Game/Audio/Generated/BM_Explosion.BM_Explosion"));
    }
    if (ExplosionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, ImpactPoint, 0.72f);
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian VFX: event=prop_destruction prop=%s explosion=%s smoke=%s sound=%s point=%s"),
        *GetNameSafe(this),
        *GetNameSafe(Explosion),
        *GetNameSafe(Smoke),
        *GetNameSafe(ExplosionSound),
        *ImpactPoint.ToString());
}

void ABMArtDirectedProp::ClearImpactFeedback()
{
    if (ImpactLight)
    {
        ImpactLight->SetIntensity(0.0f);
        ImpactLight->SetVisibility(false, true);
    }
}

float ABMArtDirectedProp::GetSignalBeaconIntensity() const
{
    return SignalBeaconLight ? SignalBeaconLight->Intensity : 0.0f;
}

void ABMArtDirectedProp::CacheDebrisTransforms()
{
    DebrisBaseLocations.Reset();
    DebrisBaseRotations.Reset();
    DebrisBaseScales.Reset();
    DebrisBaseLocations.Reserve(DebrisParts.Num());
    DebrisBaseRotations.Reserve(DebrisParts.Num());
    DebrisBaseScales.Reserve(DebrisParts.Num());

    for (UStaticMeshComponent* Debris : DebrisParts)
    {
        DebrisBaseLocations.Add(Debris ? Debris->GetRelativeLocation() : FVector::ZeroVector);
        DebrisBaseRotations.Add(Debris ? Debris->GetRelativeRotation() : FRotator::ZeroRotator);
        DebrisBaseScales.Add(Debris ? Debris->GetRelativeScale3D() : FVector::OneVector);
    }
}

void ABMArtDirectedProp::ConfigurePart(
    UStaticMeshComponent* Part,
    UStaticMesh* Mesh,
    const FVector& Location,
    const FRotator& Rotation,
    const FVector& Scale,
    const FLinearColor& Color,
    const bool bEnableCollision)
{
    if (!Part)
    {
        return;
    }

    if (Mesh)
    {
        Part->SetStaticMesh(Mesh);
    }
    Part->SetRelativeLocation(Location);
    Part->SetRelativeRotation(Rotation);
    Part->SetRelativeScale3D(Scale);
    Part->SetVisibility(true, true);
    Part->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    Part->SetCollisionResponseToAllChannels(ECR_Block);
    Part->SetGenerateOverlapEvents(false);
    Part->SetCanEverAffectNavigation(false);
    Part->SetMobility(EComponentMobility::Movable);

    if (UMaterialInterface* BaseMaterial = LoadPresentationMaterial())
    {
        if (UMaterialInstanceDynamic* Material = Part->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMaterial))
        {
            // Props are runtime-lit and need a small value lift of their own;
            // otherwise a readable modular prop disappears as soon as it
            // turns onto the shadow side of a wall. Keep this below the
            // accent-light range so cover retains depth.
            const FLinearColor ReadableColor(
                FMath::Clamp(FMath::Max(Color.R * 1.16f, 0.06f), 0.0f, 0.92f),
                FMath::Clamp(FMath::Max(Color.G * 1.16f, 0.06f), 0.0f, 0.92f),
                FMath::Clamp(FMath::Max(Color.B * 1.16f, 0.06f), 0.0f, 0.92f),
                Color.A);
            const bool bAccentSurface =
                (Color.G > Color.R + 0.12f && Color.G >= Color.B * 0.88f)
                || (Color.B > Color.R + 0.14f && Color.B > Color.G + 0.05f)
                || (Color.R > Color.G + 0.18f && Color.R > Color.B + 0.10f);
            Material->SetVectorParameterValue(TEXT("Color"), ReadableColor);
            Material->SetVectorParameterValue(TEXT("BaseColor"), ReadableColor);
            Material->SetVectorParameterValue(
                TEXT("EmissiveColor"),
                ReadableColor * (bAccentSurface ? 0.16f : 0.025f));
            Material->SetScalarParameterValue(TEXT("EmissiveStrength"), bAccentSurface ? 1.10f : 0.70f);
            Material->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
            const bool bWoodSurface = Variant == EBMArtDirectedPropVariant::Crate
                || Variant == EBMArtDirectedPropVariant::Barricade;
            Material->SetScalarParameterValue(TEXT("Metallic"), bWoodSurface ? 0.18f : 0.68f);
            Material->SetScalarParameterValue(TEXT("Roughness"), bWoodSurface ? 0.86f : 0.56f);
        }
    }
}

void ABMArtDirectedProp::RebuildPresentation()
{
    UStaticMesh* CubeMesh = LoadEnvironmentMesh(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Block.SM_BM_Env_Block"),
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* CylinderMesh = LoadEnvironmentMesh(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Pipe.SM_BM_Env_Pipe"),
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* SphereMesh = LoadEnvironmentMesh(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Orb.SM_BM_Env_Orb"),
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UStaticMesh* DeckMesh = LoadEnvironmentMesh(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Deck.SM_BM_Env_Deck"),
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* TrussMesh = LoadEnvironmentMesh(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Truss.SM_BM_Env_Truss"),
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* GrateMesh = LoadEnvironmentMesh(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Grate.SM_BM_Env_Grate"),
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* PanelMesh = LoadEnvironmentMesh(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Panel.SM_BM_Env_Panel"),
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* CoverMesh = LoadEnvironmentMesh(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Cover.SM_BM_Env_Cover"),
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* DoorMesh = LoadEnvironmentMesh(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Door.SM_BM_Env_Door"),
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* WindowMesh = LoadEnvironmentMesh(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Window.SM_BM_Env_Window"),
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* SignMesh = LoadEnvironmentMesh(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Sign.SM_BM_Env_Sign"),
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* WreckageMesh = LoadEnvironmentMesh(
        TEXT("/Game/Environment/Meshes/SM_BM_Env_Wreckage.SM_BM_Env_Wreckage"),
        TEXT("/Engine/BasicShapes/Cube.Cube"));

    for (UStaticMeshComponent* Part : MainParts)
    {
        if (Part)
        {
            Part->SetVisibility(false, true);
            Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }
    for (UStaticMeshComponent* Part : DebrisParts)
    {
        if (Part)
        {
            Part->SetVisibility(false, true);
            Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }

    RemoveSurfaceTags(this);
    const FLinearColor MetalColor = BaseColor;
    const FLinearColor WoodColor = BaseColor * FLinearColor(1.20f, 0.88f, 0.62f, 1.0f);
    const FLinearColor DebrisColor = BaseColor * FLinearColor(0.72f, 0.62f, 0.52f, 1.0f);

    auto Main = [this](
        const int32 Index,
        UStaticMesh* Mesh,
        const FVector& Location,
        const FRotator& Rotation,
        const FVector& Scale,
        const FLinearColor& Color)
    {
        if (MainParts.IsValidIndex(Index))
        {
            ConfigurePart(MainParts[Index], Mesh, Location, Rotation, Scale, Color, true);
        }
    };
    auto Debris = [this](
        const int32 Index,
        UStaticMesh* Mesh,
        const FVector& Location,
        const FRotator& Rotation,
        const FVector& Scale,
        const FLinearColor& Color)
    {
        if (DebrisParts.IsValidIndex(Index))
        {
            ConfigurePart(DebrisParts[Index], Mesh, Location, Rotation, Scale, Color, true);
        }
    };

    switch (Variant)
    {
    case EBMArtDirectedPropVariant::Crate:
        Tags.AddUnique(TEXT("Surface.Wood"));
        Tags.AddUnique(TEXT("Surface.SoftCover"));
        Tags.AddUnique(TEXT("ThinCover"));
        Main(0, CubeMesh, FVector(0.0f, 0.0f, 58.0f), FRotator::ZeroRotator, FVector(0.9f, 0.68f, 0.58f), WoodColor);
        Main(1, CubeMesh, FVector(0.0f, 0.0f, 117.0f), FRotator::ZeroRotator, FVector(0.94f, 0.055f, 0.055f), AccentColor);
        Main(2, CubeMesh, FVector(0.0f, 0.0f, 117.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.055f, 0.94f, 0.055f), AccentColor);
        Main(3, CubeMesh, FVector(-70.0f, 0.0f, 60.0f), FRotator::ZeroRotator, FVector(0.055f, 0.72f, 0.70f), AccentColor);
        Main(4, CubeMesh, FVector(70.0f, 0.0f, 60.0f), FRotator::ZeroRotator, FVector(0.055f, 0.72f, 0.70f), AccentColor);
        Debris(0, CubeMesh, FVector(34.0f, -18.0f, 28.0f), FRotator(0.0f, 18.0f, 22.0f), FVector(0.55f, 0.16f, 0.12f), DebrisColor);
        Debris(1, CubeMesh, FVector(-42.0f, 22.0f, 42.0f), FRotator(0.0f, -28.0f, -17.0f), FVector(0.44f, 0.14f, 0.12f), DebrisColor);
        Debris(2, CubeMesh, FVector(0.0f, 0.0f, 18.0f), FRotator(0.0f, 0.0f, 16.0f), FVector(0.12f, 0.62f, 0.10f), AccentColor);
        Debris(3, SphereMesh, FVector(48.0f, 30.0f, 24.0f), FRotator::ZeroRotator, FVector(0.18f), AccentColor);
        break;

    case EBMArtDirectedPropVariant::Barricade:
        Tags.AddUnique(TEXT("Surface.Wood"));
        Tags.AddUnique(TEXT("Surface.SoftCover"));
        Tags.AddUnique(TEXT("ThinCover"));
        Main(0, CoverMesh, FVector(0.0f, 0.0f, 58.0f), FRotator(0.0f, 0.0f, -9.0f), FVector(1.75f, 0.18f, 0.42f), WoodColor);
        Main(1, CoverMesh, FVector(0.0f, 0.0f, 118.0f), FRotator(0.0f, 0.0f, 8.0f), FVector(1.62f, 0.16f, 0.34f), WoodColor);
        Main(2, CubeMesh, FVector(-125.0f, 0.0f, 72.0f), FRotator::ZeroRotator, FVector(0.14f, 0.20f, 0.72f), MetalColor);
        Main(3, CubeMesh, FVector(125.0f, 0.0f, 72.0f), FRotator::ZeroRotator, FVector(0.14f, 0.20f, 0.72f), MetalColor);
        Main(4, CubeMesh, FVector(-125.0f, 0.0f, 18.0f), FRotator::ZeroRotator, FVector(0.28f, 0.34f, 0.12f), AccentColor);
        Main(5, CubeMesh, FVector(125.0f, 0.0f, 18.0f), FRotator::ZeroRotator, FVector(0.28f, 0.34f, 0.12f), AccentColor);
        Debris(0, CubeMesh, FVector(-78.0f, -12.0f, 30.0f), FRotator(0.0f, 0.0f, -28.0f), FVector(0.72f, 0.14f, 0.12f), DebrisColor);
        Debris(1, CubeMesh, FVector(72.0f, 18.0f, 42.0f), FRotator(0.0f, 0.0f, 33.0f), FVector(0.62f, 0.12f, 0.10f), DebrisColor);
        Debris(2, CylinderMesh, FVector(0.0f, 22.0f, 24.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.10f, 0.10f, 0.72f), AccentColor);
        Debris(3, CubeMesh, FVector(0.0f, -22.0f, 16.0f), FRotator::ZeroRotator, FVector(0.24f, 0.20f, 0.10f), AccentColor);
        break;

    case EBMArtDirectedPropVariant::RelayCabinet:
        Tags.AddUnique(TEXT("Surface.Metal"));
        Main(0, CubeMesh, FVector(0.0f, 0.0f, 142.0f), FRotator::ZeroRotator, FVector(0.78f, 0.52f, 1.42f), MetalColor);
        Main(1, PanelMesh, FVector(0.0f, -54.0f, 146.0f), FRotator::ZeroRotator, FVector(0.58f, 0.045f, 0.92f), AccentColor);
        Main(2, CubeMesh, FVector(0.0f, -59.0f, 265.0f), FRotator::ZeroRotator, FVector(0.64f, 0.035f, 0.055f), AccentColor);
        Main(3, CubeMesh, FVector(-70.0f, 0.0f, 146.0f), FRotator::ZeroRotator, FVector(0.05f, 0.58f, 1.50f), AccentColor);
        Main(4, CylinderMesh, FVector(72.0f, 20.0f, 178.0f), FRotator::ZeroRotator, FVector(0.08f, 0.08f, 1.30f), AccentColor);
        Main(5, CylinderMesh, FVector(0.0f, 0.0f, 350.0f), FRotator::ZeroRotator, FVector(0.07f, 0.07f, 0.90f), MetalColor);
        Main(6, SphereMesh, FVector(0.0f, 0.0f, 450.0f), FRotator::ZeroRotator, FVector(0.16f), AccentColor);
        Main(7, SphereMesh, FVector(0.0f, -62.0f, 190.0f), FRotator::ZeroRotator, FVector(0.075f), FLinearColor(1.0f, 0.22f, 0.05f, 1.0f));
        Debris(0, CubeMesh, FVector(48.0f, -18.0f, 62.0f), FRotator(0.0f, 15.0f, 24.0f), FVector(0.44f, 0.28f, 0.18f), DebrisColor);
        Debris(1, CubeMesh, FVector(-44.0f, 20.0f, 42.0f), FRotator(0.0f, -22.0f, -31.0f), FVector(0.36f, 0.20f, 0.16f), DebrisColor);
        Debris(2, CylinderMesh, FVector(20.0f, 10.0f, 30.0f), FRotator(0.0f, 90.0f, 20.0f), FVector(0.08f, 0.08f, 0.50f), AccentColor);
        Debris(3, SphereMesh, FVector(54.0f, -40.0f, 24.0f), FRotator::ZeroRotator, FVector(0.14f), AccentColor);
        break;

    case EBMArtDirectedPropVariant::PipeRack:
        Tags.AddUnique(TEXT("Surface.Metal"));
        Main(0, CylinderMesh, FVector(-125.0f, 0.0f, 120.0f), FRotator::ZeroRotator, FVector(0.12f, 0.12f, 1.20f), MetalColor);
        Main(1, CylinderMesh, FVector(125.0f, 0.0f, 120.0f), FRotator::ZeroRotator, FVector(0.12f, 0.12f, 1.20f), MetalColor);
        Main(2, CylinderMesh, FVector(0.0f, 0.0f, 65.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.12f, 0.12f, 1.36f), AccentColor);
        Main(3, CylinderMesh, FVector(0.0f, 0.0f, 185.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.12f, 0.12f, 1.36f), MetalColor);
        Main(4, CylinderMesh, FVector(0.0f, 52.0f, 125.0f), FRotator(0.0f, 0.0f, 90.0f), FVector(0.08f, 0.08f, 0.66f), AccentColor);
        Main(5, CubeMesh, FVector(0.0f, 0.0f, 22.0f), FRotator::ZeroRotator, FVector(1.55f, 0.48f, 0.10f), MetalColor);
        Main(6, SphereMesh, FVector(-125.0f, 0.0f, 250.0f), FRotator::ZeroRotator, FVector(0.12f), AccentColor);
        Main(7, SphereMesh, FVector(125.0f, 0.0f, 250.0f), FRotator::ZeroRotator, FVector(0.12f), AccentColor);
        Debris(0, CylinderMesh, FVector(-62.0f, 20.0f, 34.0f), FRotator(0.0f, 90.0f, 24.0f), FVector(0.09f, 0.09f, 0.64f), DebrisColor);
        Debris(1, CylinderMesh, FVector(58.0f, -20.0f, 52.0f), FRotator(0.0f, 90.0f, -31.0f), FVector(0.08f, 0.08f, 0.52f), AccentColor);
        Debris(2, CubeMesh, FVector(0.0f, 0.0f, 18.0f), FRotator(0.0f, 15.0f, 0.0f), FVector(0.58f, 0.24f, 0.10f), MetalColor);
        Debris(3, SphereMesh, FVector(88.0f, 20.0f, 24.0f), FRotator::ZeroRotator, FVector(0.12f), AccentColor);
        break;

    case EBMArtDirectedPropVariant::SignalTower:
        // The tower is a landmark rather than cover: its project-owned deck,
        // grate, truss, pipe, and orb silhouettes give the map a repeatable
        // vertical language and make the signal route legible at a distance.
        Tags.AddUnique(TEXT("Surface.Metal"));
        Tags.AddUnique(TEXT("ArtDirected.Landmark"));
        Main(0, DeckMesh, FVector(0.0f, 0.0f, 6.0f), FRotator::ZeroRotator, FVector(1.18f, 1.05f, 0.10f), MetalColor);
        Main(1, CubeMesh, FVector(0.0f, 0.0f, 96.0f), FRotator::ZeroRotator, FVector(0.46f, 0.46f, 1.05f), MetalColor);
        Main(2, GrateMesh, FVector(0.0f, 0.0f, 204.0f), FRotator::ZeroRotator, FVector(0.76f, 0.74f, 0.16f), MetalColor);
        Main(3, TrussMesh, FVector(0.0f, 0.0f, 278.0f), FRotator::ZeroRotator, FVector(1.10f, 0.72f, 0.52f), MetalColor);
        Main(4, CylinderMesh, FVector(0.0f, 0.0f, 402.0f), FRotator::ZeroRotator, FVector(0.09f, 0.09f, 1.55f), AccentColor);
        Main(5, CylinderMesh, FVector(0.0f, 0.0f, 512.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.055f, 0.055f, 0.62f), MetalColor);
        Main(6, SphereMesh, FVector(0.0f, 0.0f, 570.0f), FRotator::ZeroRotator, FVector(0.25f), AccentColor);
        Main(7, SphereMesh, FVector(0.0f, -38.0f, 386.0f), FRotator::ZeroRotator, FVector(0.075f), FLinearColor(1.0f, 0.20f, 0.04f, 1.0f));
        break;

    case EBMArtDirectedPropVariant::Vehicle:
        // Vehicles are intentionally assembled from the same project-owned
        // modular kit as the rest of the runtime scene. They provide a broad
        // hard-cover silhouette and a recognisable scale cue without adding a
        // new cooked dependency to the compatibility build.
        Tags.AddUnique(TEXT("Surface.Metal"));
        Tags.AddUnique(TEXT("HardCover"));
        Main(0, DeckMesh, FVector(0.0f, 0.0f, 62.0f), FRotator::ZeroRotator, FVector(1.85f, 0.82f, 0.30f), MetalColor);
        Main(1, CubeMesh, FVector(0.0f, 0.0f, 142.0f), FRotator::ZeroRotator, FVector(1.30f, 0.64f, 0.44f), MetalColor);
        Main(2, PanelMesh, FVector(0.0f, -66.0f, 148.0f), FRotator::ZeroRotator, FVector(0.92f, 0.06f, 0.34f), AccentColor);
        Main(3, CylinderMesh, FVector(-128.0f, -70.0f, 38.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.36f, 0.16f, 0.30f), MetalColor);
        Main(4, CylinderMesh, FVector(128.0f, -70.0f, 38.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.36f, 0.16f, 0.30f), MetalColor);
        Main(5, CylinderMesh, FVector(-128.0f, 70.0f, 38.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.36f, 0.16f, 0.30f), MetalColor);
        Main(6, CylinderMesh, FVector(128.0f, 70.0f, 38.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.36f, 0.16f, 0.30f), MetalColor);
        Main(7, TrussMesh, FVector(0.0f, 0.0f, 235.0f), FRotator::ZeroRotator, FVector(1.18f, 0.52f, 0.24f), AccentColor);
        Debris(0, CubeMesh, FVector(-84.0f, 0.0f, 205.0f), FRotator(0.0f, 18.0f, 11.0f), FVector(0.36f, 0.10f, 0.12f), DebrisColor);
        Debris(1, CubeMesh, FVector(86.0f, 12.0f, 92.0f), FRotator(0.0f, -24.0f, -18.0f), FVector(0.28f, 0.12f, 0.10f), AccentColor);
        Debris(2, CylinderMesh, FVector(0.0f, 82.0f, 176.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.08f, 0.08f, 0.72f), DebrisColor);
        break;

    case EBMArtDirectedPropVariant::Pillar:
        Tags.AddUnique(TEXT("Surface.Metal"));
        Tags.AddUnique(TEXT("HardCover"));
        Main(0, DeckMesh, FVector(0.0f, 0.0f, 8.0f), FRotator::ZeroRotator, FVector(0.92f, 0.92f, 0.12f), MetalColor);
        Main(1, CylinderMesh, FVector(0.0f, 0.0f, 270.0f), FRotator::ZeroRotator, FVector(0.48f, 0.48f, 2.70f), MetalColor);
        Main(2, PanelMesh, FVector(-52.0f, 0.0f, 270.0f), FRotator::ZeroRotator, FVector(0.05f, 0.56f, 2.20f), AccentColor);
        Main(3, PanelMesh, FVector(52.0f, 0.0f, 270.0f), FRotator::ZeroRotator, FVector(0.05f, 0.56f, 2.20f), AccentColor);
        Main(4, TrussMesh, FVector(0.0f, 0.0f, 548.0f), FRotator::ZeroRotator, FVector(0.82f, 0.82f, 0.22f), MetalColor);
        Main(5, CylinderMesh, FVector(0.0f, 0.0f, 640.0f), FRotator::ZeroRotator, FVector(0.09f, 0.09f, 0.90f), AccentColor);
        Main(6, SphereMesh, FVector(0.0f, 0.0f, 735.0f), FRotator::ZeroRotator, FVector(0.22f), AccentColor);
        Main(7, GrateMesh, FVector(0.0f, 0.0f, 18.0f), FRotator::ZeroRotator, FVector(0.70f, 0.70f, 0.10f), MetalColor);
        Debris(0, CubeMesh, FVector(-45.0f, 25.0f, 80.0f), FRotator(0.0f, 18.0f, 23.0f), FVector(0.22f, 0.10f, 0.12f), DebrisColor);
        Debris(1, CubeMesh, FVector(42.0f, -22.0f, 120.0f), FRotator(0.0f, -30.0f, -15.0f), FVector(0.16f, 0.12f, 0.10f), AccentColor);
        break;

    case EBMArtDirectedPropVariant::Vegetation:
        Tags.AddUnique(TEXT("Surface.SoftCover"));
        Main(0, DeckMesh, FVector(0.0f, 0.0f, 5.0f), FRotator::ZeroRotator, FVector(1.10f, 1.10f, 0.08f), BaseColor);
        Main(1, CylinderMesh, FVector(0.0f, 0.0f, 128.0f), FRotator(0.0f, 12.0f, 0.0f), FVector(0.28f, 0.28f, 1.40f), WoodColor);
        Main(2, CylinderMesh, FVector(-78.0f, 18.0f, 82.0f), FRotator(0.0f, -24.0f, 0.0f), FVector(0.12f, 0.12f, 0.88f), WoodColor);
        Main(3, SphereMesh, FVector(0.0f, 0.0f, 360.0f), FRotator::ZeroRotator, FVector(1.32f, 1.05f, 1.18f), BaseColor);
        Main(4, SphereMesh, FVector(-112.0f, 10.0f, 270.0f), FRotator::ZeroRotator, FVector(0.72f), AccentColor);
        Main(5, SphereMesh, FVector(98.0f, -28.0f, 300.0f), FRotator::ZeroRotator, FVector(0.82f), BaseColor);
        Main(6, PanelMesh, FVector(0.0f, 0.0f, 215.0f), FRotator(0.0f, 35.0f, 0.0f), FVector(0.06f, 0.32f, 1.12f), AccentColor);
        Main(7, SphereMesh, FVector(38.0f, 46.0f, 420.0f), FRotator::ZeroRotator, FVector(0.42f), AccentColor);
        Debris(0, CubeMesh, FVector(-92.0f, -32.0f, 18.0f), FRotator(0.0f, 20.0f, 0.0f), FVector(0.38f, 0.08f, 0.08f), DebrisColor);
        Debris(1, CubeMesh, FVector(76.0f, 28.0f, 26.0f), FRotator(0.0f, -18.0f, 0.0f), FVector(0.28f, 0.07f, 0.07f), DebrisColor);
        break;

    case EBMArtDirectedPropVariant::Furniture:
        Tags.AddUnique(TEXT("Surface.Wood"));
        Tags.AddUnique(TEXT("Surface.SoftCover"));
        Main(0, DeckMesh, FVector(0.0f, 0.0f, 8.0f), FRotator::ZeroRotator, FVector(1.20f, 0.76f, 0.10f), WoodColor);
        Main(1, CubeMesh, FVector(0.0f, 0.0f, 112.0f), FRotator::ZeroRotator, FVector(1.05f, 0.62f, 0.12f), WoodColor);
        Main(2, CubeMesh, FVector(-78.0f, -38.0f, 58.0f), FRotator::ZeroRotator, FVector(0.10f, 0.10f, 0.58f), MetalColor);
        Main(3, CubeMesh, FVector(78.0f, -38.0f, 58.0f), FRotator::ZeroRotator, FVector(0.10f, 0.10f, 0.58f), MetalColor);
        Main(4, CubeMesh, FVector(-78.0f, 38.0f, 58.0f), FRotator::ZeroRotator, FVector(0.10f, 0.10f, 0.58f), MetalColor);
        Main(5, CubeMesh, FVector(78.0f, 38.0f, 58.0f), FRotator::ZeroRotator, FVector(0.10f, 0.10f, 0.58f), MetalColor);
        Main(6, PanelMesh, FVector(0.0f, -58.0f, 180.0f), FRotator::ZeroRotator, FVector(0.72f, 0.06f, 0.56f), AccentColor);
        Main(7, SphereMesh, FVector(0.0f, -68.0f, 242.0f), FRotator::ZeroRotator, FVector(0.12f), AccentColor);
        Debris(0, CubeMesh, FVector(-48.0f, 0.0f, 140.0f), FRotator(0.0f, 16.0f, 18.0f), FVector(0.20f, 0.10f, 0.10f), DebrisColor);
        Debris(1, CubeMesh, FVector(54.0f, 16.0f, 36.0f), FRotator(0.0f, -24.0f, -12.0f), FVector(0.18f, 0.08f, 0.08f), AccentColor);
        break;

    case EBMArtDirectedPropVariant::RelayBuilding:
        // A compact service building gives the quay a readable destination
        // silhouette and creates a side-room sightline.  The front remains
        // an open door rather than a solid wall so the adjacent route stays
        // traversable and the objective beacon remains visible from outside.
        Tags.AddUnique(TEXT("Surface.Metal"));
        Tags.AddUnique(TEXT("ArtDirected.Landmark"));
        Main(0, DeckMesh, FVector(0.0f, 0.0f, 8.0f), FRotator::ZeroRotator, FVector(2.65f, 1.72f, 0.12f), MetalColor);
        Main(1, CubeMesh, FVector(0.0f, 152.0f, 170.0f), FRotator::ZeroRotator, FVector(2.60f, 0.14f, 1.62f), MetalColor);
        Main(2, CubeMesh, FVector(-248.0f, 0.0f, 170.0f), FRotator::ZeroRotator, FVector(0.14f, 1.62f, 1.62f), MetalColor);
        Main(3, CubeMesh, FVector(248.0f, 0.0f, 170.0f), FRotator::ZeroRotator, FVector(0.14f, 1.62f, 1.62f), MetalColor);
        Main(4, DoorMesh, FVector(0.0f, -166.0f, 148.0f), FRotator::ZeroRotator, FVector(0.72f, 0.12f, 1.38f), AccentColor);
        Main(5, WindowMesh, FVector(-154.0f, -166.0f, 188.0f), FRotator::ZeroRotator, FVector(0.46f, 0.08f, 0.40f), AccentColor);
        Main(6, WindowMesh, FVector(154.0f, -166.0f, 188.0f), FRotator::ZeroRotator, FVector(0.46f, 0.08f, 0.40f), AccentColor);
        Main(7, SignMesh, FVector(0.0f, -178.0f, 318.0f), FRotator::ZeroRotator, FVector(1.08f, 0.08f, 0.26f), AccentColor);
        Debris(0, TrussMesh, FVector(-150.0f, 110.0f, 338.0f), FRotator::ZeroRotator, FVector(0.92f, 0.18f, 0.18f), MetalColor);
        Debris(1, PanelMesh, FVector(0.0f, -154.0f, 330.0f), FRotator::ZeroRotator, FVector(1.78f, 0.05f, 0.08f), AccentColor);
        Debris(2, CylinderMesh, FVector(-208.0f, 0.0f, 352.0f), FRotator::ZeroRotator, FVector(0.08f, 0.08f, 0.40f), AccentColor);
        Debris(3, CylinderMesh, FVector(208.0f, 0.0f, 352.0f), FRotator::ZeroRotator, FVector(0.08f, 0.08f, 0.40f), AccentColor);
        break;

    case EBMArtDirectedPropVariant::ShipHull:
        // The hull is deliberately low and broad: it provides hard cover and
        // a second approach lane while preserving a readable over-the-top
        // sightline for the first combat wave.  Wreckage and deck modules
        // give it a different physical profile from the vehicle kit.
        Tags.AddUnique(TEXT("Surface.Metal"));
        Tags.AddUnique(TEXT("HardCover"));
        Tags.AddUnique(TEXT("ArtDirected.Landmark"));
        Main(0, WreckageMesh, FVector(0.0f, 0.0f, 92.0f), FRotator(0.0f, 2.0f, 0.0f), FVector(3.35f, 1.18f, 0.78f), MetalColor);
        Main(1, DeckMesh, FVector(0.0f, 0.0f, 24.0f), FRotator::ZeroRotator, FVector(3.72f, 1.30f, 0.14f), MetalColor);
        Main(2, PanelMesh, FVector(0.0f, -116.0f, 142.0f), FRotator::ZeroRotator, FVector(2.62f, 0.08f, 0.34f), AccentColor);
        Main(3, PanelMesh, FVector(0.0f, 116.0f, 142.0f), FRotator::ZeroRotator, FVector(2.62f, 0.08f, 0.34f), MetalColor);
        Main(4, TrussMesh, FVector(-260.0f, 0.0f, 220.0f), FRotator::ZeroRotator, FVector(0.56f, 0.72f, 0.36f), MetalColor);
        Main(5, TrussMesh, FVector(260.0f, 0.0f, 220.0f), FRotator::ZeroRotator, FVector(0.56f, 0.72f, 0.36f), MetalColor);
        Main(6, SignMesh, FVector(0.0f, -130.0f, 258.0f), FRotator::ZeroRotator, FVector(0.92f, 0.06f, 0.20f), AccentColor);
        Main(7, GrateMesh, FVector(0.0f, 0.0f, 170.0f), FRotator::ZeroRotator, FVector(2.28f, 0.74f, 0.10f), MetalColor);
        Debris(0, CylinderMesh, FVector(-310.0f, -92.0f, 78.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.10f, 0.10f, 0.58f), AccentColor);
        Debris(1, CylinderMesh, FVector(312.0f, 92.0f, 74.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.10f, 0.10f, 0.58f), MetalColor);
        Debris(2, PanelMesh, FVector(-82.0f, 0.0f, 212.0f), FRotator(0.0f, 12.0f, 0.0f), FVector(0.46f, 0.06f, 0.22f), DebrisColor);
        Debris(3, SphereMesh, FVector(270.0f, -110.0f, 154.0f), FRotator::ZeroRotator, FVector(0.14f), AccentColor);
        break;

    case EBMArtDirectedPropVariant::DockCrane:
        // A loading-crane silhouette gives the open-dry operation a grounded
        // maritime landmark and a readable scale reference.  It is assembled
        // from the same authored deck, pipe, truss, panel, grate, and orb kit
        // so it remains cheap to cook and consistent with the rest of the
        // environment language while still reading as a distinct machine.
        Tags.AddUnique(TEXT("Surface.Metal"));
        Tags.AddUnique(TEXT("HardCover"));
        Main(0, DeckMesh, FVector(0.0f, 0.0f, 8.0f), FRotator::ZeroRotator, FVector(1.70f, 1.05f, 0.12f), MetalColor);
        Main(1, GrateMesh, FVector(0.0f, 0.0f, 18.0f), FRotator::ZeroRotator, FVector(1.20f, 0.78f, 0.10f), MetalColor);
        Main(2, CylinderMesh, FVector(-112.0f, -64.0f, 230.0f), FRotator::ZeroRotator, FVector(0.22f, 0.22f, 2.30f), MetalColor);
        Main(3, CylinderMesh, FVector(112.0f, -64.0f, 230.0f), FRotator::ZeroRotator, FVector(0.22f, 0.22f, 2.30f), MetalColor);
        Main(4, CylinderMesh, FVector(-112.0f, 64.0f, 230.0f), FRotator::ZeroRotator, FVector(0.18f, 0.18f, 2.30f), AccentColor);
        Main(5, CylinderMesh, FVector(112.0f, 64.0f, 230.0f), FRotator::ZeroRotator, FVector(0.18f, 0.18f, 2.30f), AccentColor);
        Main(6, TrussMesh, FVector(110.0f, 0.0f, 478.0f), FRotator::ZeroRotator, FVector(2.30f, 0.42f, 0.24f), MetalColor);
        Main(7, PanelMesh, FVector(-128.0f, 0.0f, 410.0f), FRotator::ZeroRotator, FVector(0.42f, 0.30f, 0.72f), AccentColor);
        Debris(0, CylinderMesh, FVector(260.0f, 0.0f, 330.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.06f, 0.06f, 2.20f), AccentColor);
        Debris(1, CylinderMesh, FVector(260.0f, 0.0f, 158.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.035f, 0.035f, 0.82f), MetalColor);
        Debris(2, SphereMesh, FVector(252.0f, 0.0f, 78.0f), FRotator::ZeroRotator, FVector(0.14f), FLinearColor(1.0f, 0.22f, 0.05f, 1.0f));
        Debris(3, CubeMesh, FVector(58.0f, 0.0f, 474.0f), FRotator(0.0f, 18.0f, 0.0f), FVector(0.28f, 0.22f, 0.20f), AccentColor);
        break;
    }

    ApplyVisualState();
    CacheDebrisTransforms();
}

void ABMArtDirectedProp::ApplyVisualState()
{
    const bool bShowDestroyedState = bDestructible && bDestroyed;

    if (SignalBeaconLight)
    {
        const bool bShowSignalBeacon =
            Variant == EBMArtDirectedPropVariant::SignalTower && !bShowDestroyedState;

        SignalBeaconLight->SetLightColor(AccentColor);
        SignalBeaconLight->SetIntensity(bShowSignalBeacon ? 1350.0f : 0.0f);
        SignalBeaconLight->SetVisibility(bShowSignalBeacon, true);
    }

    for (UStaticMeshComponent* Part : MainParts)
    {
        if (Part)
        {
            Part->SetVisibility(!bShowDestroyedState, true);
            Part->SetCollisionEnabled(bShowDestroyedState ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
        }
    }

    for (UStaticMeshComponent* Part : DebrisParts)
    {
        if (Part)
        {
            Part->SetVisibility(bShowDestroyedState, true);
            Part->SetCollisionEnabled(
                bShowDestroyedState && !bDestructionPresentationActive
                    ? ECollisionEnabled::QueryAndPhysics
                    : ECollisionEnabled::NoCollision);
        }
    }
}

void ABMArtDirectedProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABMArtDirectedProp, Variant);
    DOREPLIFETIME(ABMArtDirectedProp, BaseColor);
    DOREPLIFETIME(ABMArtDirectedProp, AccentColor);
    DOREPLIFETIME(ABMArtDirectedProp, bDestructible);
    DOREPLIFETIME(ABMArtDirectedProp, bDestroyed);
}
