// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "BMWeapon.h"

#include "BMAudioDataAsset.h"
#include "BMArtDirectedProp.h"
#include "BMHealthComponent.h"
#include "BMGameState.h"
#include "BMGameMode.h"
#include "BMProjectile.h"
#include "BMPlayerCharacter.h"
#include "BMMissionDirector.h"
#include "BMSignalBotController.h"
#include "BMEnemyCharacter.h"
#include "BMSuppressionComponent.h"
#include "BMCombatCharacter.h"
#include "BMCombatTargeting.h"
#include "BMCombatHistoryComponent.h"
#include "BMCombatTuning.h"
#include "BMSignalControlGameMode.h"
#include "BMSquadSubsystem.h"
#include "BMGameUserSettings.h"
#include "BMWeaponComponents.h"
#include "BMWeaponDataAsset.h"
#include "BMWeaponAttachmentDataAsset.h"
#include "BMVFXDataAsset.h"
#include "BMSurfaceRuleDataAsset.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ProceduralMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Controller.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RandomStream.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AISense_Hearing.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/SoftObjectPath.h"

namespace
{
    // The submitted client ray is the gameplay/crosshair ray. The controller
    // rotation is only a bounded packet-integrity check; it
    // must not choose a second target when the rendered camera is a frame
    // ahead of replicated controller rotation. The submitted camera ray is
    // still resolved through the narrow dedicated crosshair hitbox below.
    // Keep this tolerance broad enough for camera smoothing and network
    // quantization without turning it into aim assist: the 2.5-degree/24 cm
    // shared resolver remains the actual player damage gate. That resolver
    // uses the dedicated hitbox plus a bounded, distance-aware sweep rather
    // than a single fixed angle at every range.
    constexpr float PlayerShotViewConeDegrees = 25.0f;

    /**
     * Return the server-side controller baseline for a submitted player ray.
     * The server does not own the remote player's interpolated first-person
     * camera component, so reconstructing playable cue interpolation here
     * creates a second, sometimes stale aim source. The submitted direction
     * remains the rendered crosshair ray and the dedicated hostile resolver
     * remains the damage gate; this baseline only rejects implausible packets.
     */
    FVector GetPlayerRenderedViewDirection(
        UWorld* World,
        const APawn* OwnerPawn,
        const APlayerController* PlayerController)
    {
        if (!PlayerController)
        {
            return FVector::ZeroVector;
        }

        (void)World;
        (void)OwnerPawn;
        return PlayerController->GetControlRotation().Vector().GetSafeNormal();
    }

    /**
     * Project-owned Niagara systems are part of the normal combat baseline.
     * The generated systems are deliberately small, cooked project assets
     * rather than engine content; keeping them in the ordinary path means a
     * player sees the same muzzle, shell, impact, and blood event that the
     * weapon data asset describes. Engine-only systems remain probe-only so a
     * stripped checkout cannot accidentally pull editor content into play.
     */
    bool ShouldSpawnConfiguredNiagara(const UNiagaraSystem* System)
    {
        if (!System)
        {
            return false;
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("BM_VFXProbe")))
        {
            return true;
        }
        const FString SystemPath = System->GetPathName();
        // The generated Niagara files are deterministic editor templates,
        // not final authored effects. Their imported burst geometry can
        // expand to screen-covering white ribbons on Metal when several
        // weapons fire together. Keep them available to the explicit VFX
        // asset probe, but use the weapon's project-owned fallback meshes and
        // lights during normal play until bespoke systems replace them.
        if (SystemPath.StartsWith(TEXT("/Game/VFX/Generated/"), ESearchCase::IgnoreCase))
        {
            return false;
        }
        return SystemPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase);
    }

    enum class ERuntimePulse : uint8
    {
        Shot,
        Impact,
        Reload,
        Hit,
        DryFire
    };

    /** Project-owned PCM cues are the first fallback after a designer audio
     * profile. The procedural pulse remains the final source-only fallback. */
    USoundBase* LoadGeneratedCombatCue(const TCHAR* CueName)
    {
        if (!CueName || !*CueName)
        {
            return nullptr;
        }
        const FString AssetPath = FString::Printf(
            TEXT("/Game/Audio/Generated/%s.%s"),
            CueName,
            CueName);
        return LoadObject<USoundBase>(nullptr, *AssetPath);
    }

    UMaterialInterface* LoadPresentationMaterial()
    {
        if (UMaterialInterface* BasicShapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
        {
            return BasicShapeMaterial;
        }
        return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
    }

    UMaterialInterface* LoadFXMaterial()
    {
        // The fallback FX are ordinary cube/sphere components.  An
        // emissive-only material makes those primitives read as opaque
        // screen-covering cards when a muzzle or impact is close to the
        // first-person camera.  Use the same lit presentation material as
        // the weapon and environment first; the point lights below provide
        // the short-lived flash without blowing out the whole viewport.
        if (UMaterialInterface* PresentationMaterial = LoadPresentationMaterial())
        {
            return PresentationMaterial;
        }
        if (UMaterialInterface* EmissiveMaterial = LoadObject<UMaterialInterface>(
                nullptr,
                TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial")))
        {
            return EmissiveMaterial;
        }
        return LoadPresentationMaterial();
    }

    UStaticMesh* LoadProjectWeaponMesh(const EBMWeaponId WeaponId)
    {
        const TCHAR* AssetPath = nullptr;
        switch (WeaponId)
        {
        case EBMWeaponId::Kestrel9:
            AssetPath = TEXT("/Game/Weapons/Meshes/SM_BM_Kestrel9_SMG.SM_BM_Kestrel9_SMG");
            break;
        case EBMWeaponId::RookDMR:
            AssetPath = TEXT("/Game/Weapons/Meshes/SM_BM_Rook_DMR.SM_BM_Rook_DMR");
            break;
        case EBMWeaponId::Breach12:
            AssetPath = TEXT("/Game/Weapons/Meshes/SM_BM_Breach12_Shotgun.SM_BM_Breach12_Shotgun");
            break;
        case EBMWeaponId::SolacePistol:
            AssetPath = TEXT("/Game/Weapons/Meshes/SM_BM_Solace_Pistol.SM_BM_Solace_Pistol");
            break;
        case EBMWeaponId::ArcLauncher:
            AssetPath = TEXT("/Game/Weapons/Meshes/SM_BM_Arc7_Launcher.SM_BM_Arc7_Launcher");
            break;
        case EBMWeaponId::ARX4:
            AssetPath = TEXT("/Game/Weapons/Meshes/SM_BM_ARX4_Rifle.SM_BM_ARX4_Rifle");
            break;
        case EBMWeaponId::FragGrenade:
        case EBMWeaponId::VeilGrenade:
        default:
            break;
        }

        return AssetPath ? LoadObject<UStaticMesh>(nullptr, AssetPath) : nullptr;
    }

    /**
     * Small flat-shaded mesh builder used only for the local ARX-4 viewmodel.
     * BasicShapes cubes are useful for greybox collision and remote silhouettes,
     * but their unbroken corners and disconnected pieces read as floating blocks
     * in a first-person camera. These helpers make a few inexpensive beveled and
     * octagonal parts without introducing a content-only art dependency.
     */
    struct FRuntimeWeaponMeshBuilder
    {
        TArray<FVector> Vertices;
        TArray<int32> Triangles;
        TArray<FVector> Normals;
        TArray<FVector2D> UV0;

        void AddQuad(
            const FVector& A,
            const FVector& B,
            const FVector& C,
            const FVector& D,
            const FVector& Normal)
        {
            const int32 BaseIndex = Vertices.Num();
            Vertices.Append({A, B, C, D});
            Triangles.Append({
                BaseIndex + 0,
                BaseIndex + 1,
                BaseIndex + 2,
                BaseIndex + 0,
                BaseIndex + 2,
                BaseIndex + 3});
            Normals.Append({Normal, Normal, Normal, Normal});
            UV0.Append({
                FVector2D(0.0f, 0.0f),
                FVector2D(1.0f, 0.0f),
                FVector2D(1.0f, 1.0f),
                FVector2D(0.0f, 1.0f)});
        }

        void AddTriangle(
            const FVector& A,
            const FVector& B,
            const FVector& C,
            const FVector& Normal)
        {
            const int32 BaseIndex = Vertices.Num();
            Vertices.Append({A, B, C});
            Triangles.Append({BaseIndex + 0, BaseIndex + 1, BaseIndex + 2});
            Normals.Append({Normal, Normal, Normal});
            UV0.Append({
                FVector2D(0.0f, 0.0f),
                FVector2D(1.0f, 0.0f),
                FVector2D(0.5f, 1.0f)});
        }

        static FVector RotatePoint(const FVector& LocalPoint, const FVector& Center, const FRotator& Rotation)
        {
            return Center + Rotation.Quaternion().RotateVector(LocalPoint);
        }

        static TArray<FVector2D> ChamferedProfile(const float HalfY, const float HalfZ, const float Bevel)
        {
            const float SafeBevel = FMath::Clamp(Bevel, 0.0f, FMath::Min(HalfY, HalfZ) * 0.8f);
            return {
                FVector2D(-HalfY + SafeBevel, -HalfZ),
                FVector2D(HalfY - SafeBevel, -HalfZ),
                FVector2D(HalfY, -HalfZ + SafeBevel),
                FVector2D(HalfY, HalfZ - SafeBevel),
                FVector2D(HalfY - SafeBevel, HalfZ),
                FVector2D(-HalfY + SafeBevel, HalfZ),
                FVector2D(-HalfY, HalfZ - SafeBevel),
                FVector2D(-HalfY, -HalfZ + SafeBevel)};
        }

        void AddChamferedBox(
            const FVector& Center,
            const FVector& HalfExtent,
            const float Bevel,
            const FRotator& Rotation = FRotator::ZeroRotator)
        {
            const TArray<FVector2D> Profile = ChamferedProfile(HalfExtent.Y, HalfExtent.Z, Bevel);
            TArray<FVector> MinusX;
            TArray<FVector> PlusX;
            MinusX.Reserve(Profile.Num());
            PlusX.Reserve(Profile.Num());
            for (const FVector2D& Point : Profile)
            {
                MinusX.Add(RotatePoint(FVector(-HalfExtent.X, Point.X, Point.Y), Center, Rotation));
                PlusX.Add(RotatePoint(FVector(HalfExtent.X, Point.X, Point.Y), Center, Rotation));
            }

            for (int32 Index = 0; Index < Profile.Num(); ++Index)
            {
                const int32 NextIndex = (Index + 1) % Profile.Num();
                const FVector SideNormal = FVector::CrossProduct(
                    PlusX[Index] - MinusX[Index],
                    MinusX[NextIndex] - MinusX[Index]).GetSafeNormal();
                AddQuad(
                    MinusX[Index],
                    MinusX[NextIndex],
                    PlusX[NextIndex],
                    PlusX[Index],
                    SideNormal);
            }

            const FVector MinusCenter = RotatePoint(FVector::ZeroVector, Center, Rotation)
                + Rotation.Quaternion().RotateVector(FVector(-HalfExtent.X, 0.0f, 0.0f));
            const FVector PlusCenter = RotatePoint(FVector::ZeroVector, Center, Rotation)
                + Rotation.Quaternion().RotateVector(FVector(HalfExtent.X, 0.0f, 0.0f));
            for (int32 Index = 0; Index < Profile.Num(); ++Index)
            {
                const int32 NextIndex = (Index + 1) % Profile.Num();
                AddTriangle(MinusCenter, MinusX[NextIndex], MinusX[Index], -Rotation.Quaternion().GetForwardVector());
                AddTriangle(PlusCenter, PlusX[Index], PlusX[NextIndex], Rotation.Quaternion().GetForwardVector());
            }
        }

        void AddTaperedChamferedBox(
            const FVector& Center,
            const FVector& RearHalfExtent,
            const FVector& FrontHalfExtent,
            const float Bevel,
            const FRotator& Rotation = FRotator::ZeroRotator)
        {
            const TArray<FVector2D> RearProfile = ChamferedProfile(
                RearHalfExtent.Y,
                RearHalfExtent.Z,
                Bevel);
            const TArray<FVector2D> FrontProfile = ChamferedProfile(
                FrontHalfExtent.Y,
                FrontHalfExtent.Z,
                Bevel);
            TArray<FVector> Rear;
            TArray<FVector> Front;
            Rear.Reserve(RearProfile.Num());
            Front.Reserve(FrontProfile.Num());
            for (const FVector2D& Point : RearProfile)
            {
                Rear.Add(RotatePoint(
                    FVector(-RearHalfExtent.X, Point.X, Point.Y),
                    Center,
                    Rotation));
            }
            for (const FVector2D& Point : FrontProfile)
            {
                Front.Add(RotatePoint(
                    FVector(FrontHalfExtent.X, Point.X, Point.Y),
                    Center,
                    Rotation));
            }

            for (int32 Index = 0; Index < RearProfile.Num(); ++Index)
            {
                const int32 NextIndex = (Index + 1) % RearProfile.Num();
                const FVector SideNormal = FVector::CrossProduct(
                    Front[Index] - Rear[Index],
                    Rear[NextIndex] - Rear[Index]).GetSafeNormal();
                AddQuad(
                    Rear[Index],
                    Rear[NextIndex],
                    Front[NextIndex],
                    Front[Index],
                    SideNormal);
            }

            const FVector RearCenter = RotatePoint(
                FVector(-RearHalfExtent.X, 0.0f, 0.0f),
                Center,
                Rotation);
            const FVector FrontCenter = RotatePoint(
                FVector(FrontHalfExtent.X, 0.0f, 0.0f),
                Center,
                Rotation);
            const FVector Forward = Rotation.Quaternion().GetForwardVector();
            for (int32 Index = 0; Index < RearProfile.Num(); ++Index)
            {
                const int32 NextIndex = (Index + 1) % RearProfile.Num();
                AddTriangle(RearCenter, Rear[NextIndex], Rear[Index], -Forward);
                AddTriangle(FrontCenter, Front[Index], Front[NextIndex], Forward);
            }
        }

        void AddCylindricalTube(
            const FVector& Center,
            const float HalfLength,
            const float RadiusY,
            const float RadiusZ,
            const int32 SegmentCount = 12,
            const FRotator& Rotation = FRotator::ZeroRotator)
        {
            const int32 SafeSegmentCount = FMath::Max(6, SegmentCount);
            TArray<FVector> MinusX;
            TArray<FVector> PlusX;
            MinusX.Reserve(SafeSegmentCount);
            PlusX.Reserve(SafeSegmentCount);
            for (int32 Segment = 0; Segment < SafeSegmentCount; ++Segment)
            {
                const float Angle = -PI / static_cast<float>(SafeSegmentCount)
                    + (2.0f * PI * static_cast<float>(Segment) / static_cast<float>(SafeSegmentCount));
                const FVector CrossSection(
                    0.0f,
                    FMath::Cos(Angle) * RadiusY,
                    FMath::Sin(Angle) * RadiusZ);
                MinusX.Add(RotatePoint(
                    FVector(-HalfLength, CrossSection.Y, CrossSection.Z),
                    Center,
                    Rotation));
                PlusX.Add(RotatePoint(
                    FVector(HalfLength, CrossSection.Y, CrossSection.Z),
                    Center,
                    Rotation));
            }

            for (int32 Segment = 0; Segment < SafeSegmentCount; ++Segment)
            {
                const int32 NextSegment = (Segment + 1) % SafeSegmentCount;
                const FVector SideNormal = FVector::CrossProduct(
                    PlusX[Segment] - MinusX[Segment],
                    MinusX[NextSegment] - MinusX[Segment]).GetSafeNormal();
                AddQuad(
                    MinusX[Segment],
                    MinusX[NextSegment],
                    PlusX[NextSegment],
                    PlusX[Segment],
                    SideNormal);
            }

            const FVector Forward = Rotation.Quaternion().GetForwardVector();
            const FVector MinusCenter = Center - Forward * HalfLength;
            const FVector PlusCenter = Center + Forward * HalfLength;
            for (int32 Segment = 0; Segment < SafeSegmentCount; ++Segment)
            {
                const int32 NextSegment = (Segment + 1) % SafeSegmentCount;
                AddTriangle(MinusCenter, MinusX[NextSegment], MinusX[Segment], -Forward);
                AddTriangle(PlusCenter, PlusX[Segment], PlusX[NextSegment], Forward);
            }
        }

        void AddOctagonalTube(
            const FVector& Center,
            const float HalfLength,
            const float RadiusY,
            const float RadiusZ,
            const FRotator& Rotation = FRotator::ZeroRotator)
        {
            constexpr int32 SegmentCount = 8;
            TArray<FVector> MinusX;
            TArray<FVector> PlusX;
            MinusX.Reserve(SegmentCount);
            PlusX.Reserve(SegmentCount);
            for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
            {
                const float Angle = -PI * 0.125f + (2.0f * PI * static_cast<float>(Segment) / static_cast<float>(SegmentCount));
                const FVector CrossSection(0.0f, FMath::Cos(Angle) * RadiusY, FMath::Sin(Angle) * RadiusZ);
                MinusX.Add(RotatePoint(FVector(-HalfLength, CrossSection.Y, CrossSection.Z), Center, Rotation));
                PlusX.Add(RotatePoint(FVector(HalfLength, CrossSection.Y, CrossSection.Z), Center, Rotation));
            }

            for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
            {
                const int32 NextSegment = (Segment + 1) % SegmentCount;
                const FVector SideNormal = FVector::CrossProduct(
                    PlusX[Segment] - MinusX[Segment],
                    MinusX[NextSegment] - MinusX[Segment]).GetSafeNormal();
                AddQuad(
                    MinusX[Segment],
                    MinusX[NextSegment],
                    PlusX[NextSegment],
                    PlusX[Segment],
                    SideNormal);
            }

            const FVector Forward = Rotation.Quaternion().GetForwardVector();
            const FVector MinusCenter = Center - Forward * HalfLength;
            const FVector PlusCenter = Center + Forward * HalfLength;
            for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
            {
                const int32 NextSegment = (Segment + 1) % SegmentCount;
                AddTriangle(MinusCenter, MinusX[NextSegment], MinusX[Segment], -Forward);
                AddTriangle(PlusCenter, PlusX[Segment], PlusX[NextSegment], Forward);
            }
        }
    };

    /**
     * Minimal procedural fallback audio for source-only builds. Authored
     * SoundWave assets take precedence; these short pulses prevent a silent
     * combat loop while the Content/Audio assets are being produced.
     */
    void PlayRuntimePulse(
        const UObject* WorldContextObject,
        const FVector& Location,
        const ERuntimePulse Pulse,
        const float Volume,
        const float Pitch)
    {
        // Network/automation probes deliberately run with -NoSound. Avoid
        // generating PCM for a device that cannot consume it; the rendered
        // player path still receives the fallback pulse.
        if (!WorldContextObject || FParse::Param(FCommandLine::Get(), TEXT("NoSound")))
        {
            return;
        }

        const int32 SampleRate = 22050;
        const float Duration = Pulse == ERuntimePulse::Shot
            ? 0.095f
            : (Pulse == ERuntimePulse::Impact
                ? 0.055f
                : (Pulse == ERuntimePulse::Reload ? 0.18f : (Pulse == ERuntimePulse::DryFire ? 0.045f : 0.075f)));
        const int32 SampleCount = FMath::Max(1, FMath::RoundToInt(Duration * SampleRate));
        TArray<int16> Samples;
        Samples.SetNumUninitialized(SampleCount);
        uint32 NoiseState = 0xA341316Cu ^ static_cast<uint32>(Pulse) * 0x9E3779B9u;

        for (int32 Index = 0; Index < SampleCount; ++Index)
        {
            const float Time = static_cast<float>(Index) / static_cast<float>(SampleRate);
            const float NormalizedTime = Time / Duration;
            const float Envelope = FMath::Pow(FMath::Clamp(1.0f - NormalizedTime, 0.0f, 1.0f), Pulse == ERuntimePulse::Reload ? 2.0f : 1.35f);
            NoiseState = NoiseState * 1664525u + 1013904223u;
            const float Noise = (static_cast<float>((NoiseState >> 9) & 0x7FFFu) / 16384.0f) - 1.0f;
            float Signal = 0.0f;

            switch (Pulse)
            {
            case ERuntimePulse::Shot:
                Signal = Noise * 0.48f
                    + FMath::Sin(Time * 2.0f * PI * 92.0f) * 0.38f
                    + FMath::Sin(Time * 2.0f * PI * 410.0f) * 0.16f;
                break;
            case ERuntimePulse::Impact:
                Signal = FMath::Sin(Time * 2.0f * PI * 920.0f) * 0.58f
                    + Noise * 0.18f;
                break;
            case ERuntimePulse::Reload:
                Signal = FMath::Sin(Time * 2.0f * PI * (210.0f + 180.0f * NormalizedTime)) * 0.42f
                    + Noise * 0.08f;
                break;
            case ERuntimePulse::Hit:
                Signal = FMath::Sin(Time * 2.0f * PI * 1380.0f) * 0.55f
                    + FMath::Sin(Time * 2.0f * PI * 620.0f) * 0.18f;
                break;
            case ERuntimePulse::DryFire:
                Signal = FMath::Sin(Time * 2.0f * PI * 245.0f) * 0.34f
                    + Noise * 0.18f
                    + FMath::Sin(Time * 2.0f * PI * 1220.0f) * 0.12f;
                break;
            }

            Samples[Index] = static_cast<int16>(FMath::Clamp(Signal * Envelope * Volume, -1.0f, 1.0f) * 32767.0f);
        }

        USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(GetTransientPackage());
        if (!Wave)
        {
            return;
        }
        Wave->NumChannels = 1;
        Wave->SetSampleRate(SampleRate);
        Wave->Duration = Duration;
        Wave->SoundGroup = SOUNDGROUP_Effects;
        Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
        UGameplayStatics::PlaySoundAtLocation(WorldContextObject, Wave, Location, FRotator::ZeroRotator, 1.0f, Pitch);
    }
}

ABMWeapon::ABMWeapon()
{
    bReplicates = true;
    SetReplicateMovement(false);
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
    WeaponMesh->SetupAttachment(Root);
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WeaponMesh->SetOnlyOwnerSee(true);
    // A readable low-poly fallback silhouette: receiver, barrel, magazine,
    // grip, and optic. Authored weapon meshes can replace these components.
    WeaponMesh->SetRelativeScale3D(FVector(0.23f, 0.075f, 0.06f));
    WeaponMesh->SetRelativeLocation(FVector(30.0f, 8.0f, -13.0f));
    PresentationBaseLocation = WeaponMesh->GetRelativeLocation();
    PresentationBaseRotation = WeaponMesh->GetRelativeRotation();

    BarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrelMesh"));
    BarrelMesh->SetupAttachment(Root);
    BarrelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BarrelMesh->SetOnlyOwnerSee(true);
    BarrelMesh->SetRelativeScale3D(FVector(0.32f, 0.026f, 0.026f));
    BarrelMesh->SetRelativeLocation(FVector(51.0f, 8.0f, -11.0f));
    // BasicShapes/Cylinder is authored along local Z. Pitch it onto the
    // weapon's local X axis; yaw would leave the barrel pointing vertically.
    BarrelMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

    MagazineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MagazineMesh"));
    MagazineMesh->SetupAttachment(Root);
    MagazineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MagazineMesh->SetOnlyOwnerSee(true);
    MagazineMesh->SetRelativeScale3D(FVector(0.055f, 0.045f, 0.13f));
    MagazineMesh->SetRelativeLocation(FVector(27.0f, 8.0f, -24.0f));

    GripMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GripMesh"));
    GripMesh->SetupAttachment(Root);
    GripMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GripMesh->SetOnlyOwnerSee(true);
    GripMesh->SetRelativeScale3D(FVector(0.06f, 0.045f, 0.14f));
    GripMesh->SetRelativeLocation(FVector(18.0f, 8.0f, -22.0f));

    OpticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OpticMesh"));
    OpticMesh->SetupAttachment(Root);
    OpticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OpticMesh->SetOnlyOwnerSee(true);
    OpticMesh->SetRelativeScale3D(FVector(0.075f, 0.032f, 0.035f));
    OpticMesh->SetRelativeLocation(FVector(30.0f, 8.0f, -6.0f));

    StockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StockMesh"));
    StockMesh->SetupAttachment(Root);
    StockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    StockMesh->SetOnlyOwnerSee(true);
    StockMesh->SetRelativeScale3D(FVector(0.16f, 0.095f, 0.075f));
    StockMesh->SetRelativeLocation(FVector(6.0f, 8.0f, -12.0f));
    StockMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, -4.0f));

    HandguardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandguardMesh"));
    HandguardMesh->SetupAttachment(Root);
    HandguardMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HandguardMesh->SetOnlyOwnerSee(true);
    HandguardMesh->SetRelativeScale3D(FVector(0.14f, 0.085f, 0.055f));
    HandguardMesh->SetRelativeLocation(FVector(46.0f, 8.0f, -11.5f));

    MuzzleDeviceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MuzzleDeviceMesh"));
    MuzzleDeviceMesh->SetupAttachment(Root);
    MuzzleDeviceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MuzzleDeviceMesh->SetOnlyOwnerSee(true);
    MuzzleDeviceMesh->SetRelativeScale3D(FVector(0.045f, 0.045f, 0.075f));
    MuzzleDeviceMesh->SetRelativeLocation(FVector(69.0f, 8.0f, -11.0f));
    MuzzleDeviceMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

    RailMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RailMesh"));
    RailMesh->SetupAttachment(Root);
    RailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RailMesh->SetOnlyOwnerSee(true);
    RailMesh->SetRelativeScale3D(FVector(0.12f, 0.018f, 0.012f));
    RailMesh->SetRelativeLocation(FVector(35.0f, 8.0f, -4.6f));

    FrontSightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrontSightMesh"));
    FrontSightMesh->SetupAttachment(Root);
    FrontSightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FrontSightMesh->SetOnlyOwnerSee(true);
    FrontSightMesh->SetRelativeScale3D(FVector(0.02f, 0.018f, 0.045f));
    FrontSightMesh->SetRelativeLocation(FVector(58.0f, 8.0f, -4.0f));

    TriggerGuardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TriggerGuardMesh"));
    TriggerGuardMesh->SetupAttachment(Root);
    TriggerGuardMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TriggerGuardMesh->SetOnlyOwnerSee(true);
    TriggerGuardMesh->SetRelativeScale3D(FVector(0.06f, 0.018f, 0.015f));
    TriggerGuardMesh->SetRelativeLocation(FVector(20.0f, 8.0f, -14.0f));

    ChargingHandleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChargingHandleMesh"));
    ChargingHandleMesh->SetupAttachment(Root);
    ChargingHandleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ChargingHandleMesh->SetOnlyOwnerSee(true);
    ChargingHandleMesh->SetRelativeScale3D(FVector(0.05f, 0.014f, 0.012f));
    ChargingHandleMesh->SetRelativeLocation(FVector(39.0f, 8.0f, -3.7f));

    EquippedOpticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EquippedOpticMesh"));
    EquippedOpticMesh->SetupAttachment(Root);
    EquippedOpticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EquippedOpticMesh->SetOnlyOwnerSee(true);
    EquippedOpticMesh->SetVisibility(false);

    EquippedMuzzleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EquippedMuzzleMesh"));
    EquippedMuzzleMesh->SetupAttachment(Root);
    EquippedMuzzleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EquippedMuzzleMesh->SetOnlyOwnerSee(true);
    EquippedMuzzleMesh->SetVisibility(false);

    EquippedMagazineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EquippedMagazineMesh"));
    EquippedMagazineMesh->SetupAttachment(Root);
    EquippedMagazineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EquippedMagazineMesh->SetOnlyOwnerSee(true);
    EquippedMagazineMesh->SetVisibility(false);

    EquippedGripMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EquippedGripMesh"));
    EquippedGripMesh->SetupAttachment(Root);
    EquippedGripMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EquippedGripMesh->SetOnlyOwnerSee(true);
    EquippedGripMesh->SetVisibility(false);

    EquippedStockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EquippedStockMesh"));
    EquippedStockMesh->SetupAttachment(Root);
    EquippedStockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EquippedStockMesh->SetOnlyOwnerSee(true);
    EquippedStockMesh->SetVisibility(false);

    FirstPersonGeometryMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FirstPersonGeometryMesh"));
    FirstPersonGeometryMesh->SetupAttachment(Root);
    FirstPersonGeometryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FirstPersonGeometryMesh->SetOnlyOwnerSee(true);
    FirstPersonGeometryMesh->SetCastShadow(true);
    FirstPersonGeometryMesh->SetVisibility(false);

    FirstPersonMagazineGeometryMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FirstPersonMagazineGeometryMesh"));
    FirstPersonMagazineGeometryMesh->SetupAttachment(Root);
    FirstPersonMagazineGeometryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FirstPersonMagazineGeometryMesh->SetOnlyOwnerSee(true);
    FirstPersonMagazineGeometryMesh->SetCastShadow(true);
    FirstPersonMagazineGeometryMesh->SetVisibility(false);

    FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
    FireLight->SetupAttachment(Root);
    FireLight->SetVisibility(false);
    FireLight->SetIntensity(0.0f);
    FireLight->SetAttenuationRadius(240.0f);
    FireLight->SetCastShadows(false);

    MuzzleFlashMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MuzzleFlashMesh"));
    MuzzleFlashMesh->SetupAttachment(Root);
    MuzzleFlashMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MuzzleFlashMesh->SetCastShadow(false);
    MuzzleFlashMesh->SetVisibility(false);

    TracerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TracerMesh"));
    TracerMesh->SetupAttachment(Root);
    TracerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TracerMesh->SetCastShadow(false);
    TracerMesh->SetVisibility(false);

    ImpactFlashMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ImpactFlashMesh"));
    ImpactFlashMesh->SetupAttachment(Root);
    ImpactFlashMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ImpactFlashMesh->SetCastShadow(false);
    ImpactFlashMesh->SetVisibility(false);

    ImpactShardMeshA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ImpactShardMeshA"));
    ImpactShardMeshA->SetupAttachment(Root);
    ImpactShardMeshA->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ImpactShardMeshA->SetCastShadow(false);
    ImpactShardMeshA->SetVisibility(false);

    ImpactShardMeshB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ImpactShardMeshB"));
    ImpactShardMeshB->SetupAttachment(Root);
    ImpactShardMeshB->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ImpactShardMeshB->SetCastShadow(false);
    ImpactShardMeshB->SetVisibility(false);

    ImpactLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ImpactLight"));
    ImpactLight->SetupAttachment(Root);
    ImpactLight->SetVisibility(false);
    ImpactLight->SetIntensity(0.0f);
    ImpactLight->SetAttenuationRadius(160.0f);
    ImpactLight->SetCastShadows(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (CubeMesh.Succeeded())
    {
        WeaponMesh->SetStaticMesh(CubeMesh.Object);
        MagazineMesh->SetStaticMesh(CubeMesh.Object);
        GripMesh->SetStaticMesh(CubeMesh.Object);
        OpticMesh->SetStaticMesh(CubeMesh.Object);
        StockMesh->SetStaticMesh(CubeMesh.Object);
        HandguardMesh->SetStaticMesh(CubeMesh.Object);
        RailMesh->SetStaticMesh(CubeMesh.Object);
        FrontSightMesh->SetStaticMesh(CubeMesh.Object);
        TriggerGuardMesh->SetStaticMesh(CubeMesh.Object);
        ChargingHandleMesh->SetStaticMesh(CubeMesh.Object);
        EquippedOpticMesh->SetStaticMesh(CubeMesh.Object);
        EquippedMagazineMesh->SetStaticMesh(CubeMesh.Object);
        EquippedGripMesh->SetStaticMesh(CubeMesh.Object);
        EquippedStockMesh->SetStaticMesh(CubeMesh.Object);
        MuzzleFlashMesh->SetStaticMesh(CubeMesh.Object);
        TracerMesh->SetStaticMesh(CubeMesh.Object);
        ImpactFlashMesh->SetStaticMesh(CubeMesh.Object);
        ImpactShardMeshA->SetStaticMesh(CubeMesh.Object);
        ImpactShardMeshB->SetStaticMesh(CubeMesh.Object);
    }
    if (SphereMesh.Succeeded())
    {
        // A rounded core reads as a burst of light at encounter distance;
        // the two cube shards below provide the directional shape.
        ImpactFlashMesh->SetStaticMesh(SphereMesh.Object);
    }
    if (CylinderMesh.Succeeded())
    {
        BarrelMesh->SetStaticMesh(CylinderMesh.Object);
        BarrelMesh->SetRelativeScale3D(FVector(0.032f, 0.032f, 0.32f));
        bFallbackBarrelUsesUpAxis = true;
        MuzzleDeviceMesh->SetStaticMesh(CylinderMesh.Object);
        bFallbackMuzzleDeviceUsesUpAxis = true;
        EquippedMuzzleMesh->SetStaticMesh(CylinderMesh.Object);
    }

    FireModeComponent = CreateDefaultSubobject<UBMFireModeComponent>(TEXT("FireMode"));
    RecoilComponent = CreateDefaultSubobject<UBMRecoilComponent>(TEXT("Recoil"));
    AmmunitionComponent = CreateDefaultSubobject<UBMAmmunitionComponent>(TEXT("Ammunition"));
}

void ABMWeapon::BeginPlay()
{
    Super::BeginPlay();

    // First-person player weapons stay owner-only, while AI weapons are a
    // replicated third-person presentation. The old unconditional flag made
    // computer adversaries damage the player without ever visibly holding or
    // firing a weapon.
    const ABMPlayerCharacter* OwningCharacter = Cast<ABMPlayerCharacter>(GetOwner());
    const bool bFirstPersonOwner = OwningCharacter && OwningCharacter->IsLocallyControlled();
    SetThirdPersonPresentation(!bFirstPersonOwner);

    if (WeaponMesh)
    {
        PresentationBaseLocation = WeaponMesh->GetRelativeLocation();
        PresentationBaseRotation = WeaponMesh->GetRelativeRotation();
    }

    PresentationReloadAlpha = 0.0f;

    if (!WeaponData)
    {
        WeaponData = UBMWeaponDataAsset::LoadContentProfile(this, ReplicatedWeaponId);
    }

    PrewarmPresentationAssets();

    ApplyFallbackWeaponVariant();
    ApplyPresentationMaterials();
    if (AmmunitionComponent && WeaponData)
    {
        // Capacity is data-driven and safe to configure locally. Do not call
        // Initialize on clients because replicated magazine/reserve values
        // must remain authoritative and must not be reset during BeginPlay.
        AmmunitionComponent->ConfigureMagazineCapacity(WeaponData->Balance.MagazineSize);
    }

    PresentationShotPulse = 0.0f;
    bPresentationWasReloading = false;
    bPresentationReloadReturning = false;

    if (HasAuthority() && WeaponData)
    {
        bReplicatedSuppressed = WeaponData->bSuppressed;
    }

    if (HasAuthority() && WeaponData)
    {
        AmmunitionComponent->Initialize(WeaponData->Balance.MagazineSize, WeaponData->Balance.ReserveAmmo);
    }
}

void ABMWeapon::SetThirdPersonPresentation(const bool bEnabled)
{
    const bool bPresentationChanged = bThirdPersonPresentation != bEnabled;
    bThirdPersonPresentation = bEnabled;

    const TArray<UPrimitiveComponent*> PresentationMeshes = {
        WeaponMesh.Get(),
        BarrelMesh.Get(),
        MagazineMesh.Get(),
        GripMesh.Get(),
        OpticMesh.Get(),
        StockMesh.Get(),
        HandguardMesh.Get(),
        MuzzleDeviceMesh.Get(),
        RailMesh.Get(),
        FrontSightMesh.Get(),
        TriggerGuardMesh.Get(),
        ChargingHandleMesh.Get(),
        EquippedOpticMesh.Get(),
        EquippedMuzzleMesh.Get(),
        EquippedMagazineMesh.Get(),
        EquippedGripMesh.Get(),
        EquippedStockMesh.Get(),
        FirstPersonGeometryMesh.Get(),
        FirstPersonMagazineGeometryMesh.Get()};
    for (UPrimitiveComponent* PresentationMesh : PresentationMeshes)
    {
        if (PresentationMesh)
        {
            // Set both flags explicitly. The weapon class starts with the
            // first-person default, and replicated AI actors may reach
            // BeginPlay before their owner is resolved on a remote client.
            PresentationMesh->SetOnlyOwnerSee(!bEnabled);
            PresentationMesh->SetOwnerNoSee(false);
            PresentationMesh->SetVisibility(true, true);
            PresentationMesh->SetCastShadow(bEnabled);
        }
    }

    // AI weapons are authored around the hand socket, so scaling the complete
    // actor keeps the receiver, grip, barrel, and authored Muzzle socket in
    // the same proportion. The previous 1.12 profile was easy to lose against
    // the Manny torso at encounter distance.
    SetActorRelativeScale3D(bEnabled ? FVector(1.20f) : FVector::OneVector);

    // Possession can happen after the weapon actor's BeginPlay. Rebuild the
    // geometry when the owner changes from the provisional third-person mode
    // to the local first-person mode; changing visibility alone would leave
    // the authored receiver in place with the first-person camera.
    if (bPresentationChanged)
    {
        ApplyFallbackWeaponVariant();
    }
    ApplyPresentationMaterials();
}

void ABMWeapon::InitializeFromData(UBMWeaponDataAsset* InWeaponData)
{
    if (!InWeaponData)
    {
        return;
    }

    WeaponData = InWeaponData;
    ReplicatedWeaponId = InWeaponData->WeaponId;
    bReplicatedSuppressed = InWeaponData->bSuppressed;
    PrewarmPresentationAssets();
    ApplyFallbackWeaponVariant();
    ApplyPresentationMaterials();
    if (AmmunitionComponent)
    {
        AmmunitionComponent->ConfigureMagazineCapacity(WeaponData->Balance.MagazineSize);
    }
    if (HasAuthority())
    {
        AmmunitionComponent->Initialize(WeaponData->Balance.MagazineSize, WeaponData->Balance.ReserveAmmo);
    }
    ForceNetUpdate();
}

void ABMWeapon::PrewarmPresentationAssets()
{
    if (!WeaponData)
    {
        return;
    }

    const double PrewarmStartSeconds = FPlatformTime::Seconds();
    PrewarmedAudioAssets.Reset();
    PrewarmedVFXAssets.Reset();
    PrewarmedAudioProfile = WeaponData->AudioProfile.LoadSynchronous();
    PrewarmedVFXProfile = WeaponData->VFXProfile.LoadSynchronous();
    PrewarmedImpactDecal = nullptr;

    auto HoldSound = [this](USoundBase* Sound)
    {
        if (Sound && !PrewarmedAudioAssets.Contains(Sound))
        {
            PrewarmedAudioAssets.Add(Sound);
        }
    };
    auto HoldVFX = [this](UNiagaraSystem* System)
    {
        if (System && !PrewarmedVFXAssets.Contains(System))
        {
            PrewarmedVFXAssets.Add(System);
        }
    };

    HoldSound(WeaponData->FireSound.LoadSynchronous());
    HoldSound(WeaponData->SuppressedFireSound.LoadSynchronous());
    HoldSound(WeaponData->DryFireSound.LoadSynchronous());

    if (PrewarmedAudioProfile)
    {
        HoldSound(PrewarmedAudioProfile->IndoorGunshot.LoadSynchronous());
        HoldSound(PrewarmedAudioProfile->OutdoorGunshot.LoadSynchronous());
        HoldSound(PrewarmedAudioProfile->SuppressedGunshot.LoadSynchronous());
        HoldSound(PrewarmedAudioProfile->DryFire.LoadSynchronous());
        HoldSound(PrewarmedAudioProfile->BulletImpact.LoadSynchronous());
        HoldSound(PrewarmedAudioProfile->Reload.LoadSynchronous());
        for (const TPair<EBMSurfaceType, TSoftObjectPtr<USoundBase>>& SurfaceCue : PrewarmedAudioProfile->SurfaceImpacts)
        {
            HoldSound(SurfaceCue.Value.LoadSynchronous());
        }
    }

    if (PrewarmedVFXProfile)
    {
        HoldVFX(PrewarmedVFXProfile->MuzzleFlash.LoadSynchronous());
        HoldVFX(PrewarmedVFXProfile->BulletImpact.LoadSynchronous());
        HoldVFX(PrewarmedVFXProfile->BloodImpact.LoadSynchronous());
        HoldVFX(PrewarmedVFXProfile->Explosion.LoadSynchronous());
        HoldVFX(PrewarmedVFXProfile->Dust.LoadSynchronous());
        HoldVFX(PrewarmedVFXProfile->Smoke.LoadSynchronous());
        HoldVFX(PrewarmedVFXProfile->Sparks.LoadSynchronous());
        HoldVFX(PrewarmedVFXProfile->ShellEjection.LoadSynchronous());
        HoldVFX(PrewarmedVFXProfile->Suppression.LoadSynchronous());
        PrewarmedImpactDecal = PrewarmedVFXProfile->ImpactDecal.LoadSynchronous();
    }

    // Keep the generated source-only cues resident as well. They are used by
    // the fallback path when a designer profile omits a cue and are cheap to
    // load during setup compared with stalling on the first trigger pull.
    HoldSound(LoadGeneratedCombatCue(TEXT("BM_ARX4_Fire")));
    HoldSound(LoadGeneratedCombatCue(TEXT("BM_Suppressed_Fire")));
    HoldSound(LoadGeneratedCombatCue(TEXT("BM_Impact_Concrete")));
    HoldSound(LoadGeneratedCombatCue(TEXT("BM_Impact_Metal")));
    HoldSound(LoadGeneratedCombatCue(TEXT("BM_DryFire")));
    HoldSound(LoadGeneratedCombatCue(TEXT("BM_Reload")));

#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe")))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat probe: presentation assets prewarmed weapon=%s audio=%d vfx=%d elapsed_ms=%.2f"),
            *GetNameSafe(this),
            PrewarmedAudioAssets.Num(),
            PrewarmedVFXAssets.Num(),
            (FPlatformTime::Seconds() - PrewarmStartSeconds) * 1000.0);
    }
#endif
}

void ABMWeapon::Fire(const FVector& ClientOrigin, const FVector& ClientDirection, const bool bAiming)
{
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe")) && !HasAuthority())
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat probe: client weapon fire owner=%s magazine_before=%d reserve=%d"),
            *GetNameSafe(GetOwner()),
            GetMagazineAmmo(),
            GetReserveAmmo());
    }
#endif

    if (!WeaponData || IsReloading())
    {
        // A held trigger must not cancel a reload on every tick. Reload is a
        // committed player action; firing becomes available again as soon as
        // the authoritative timer restores the magazine. Weapon switching or
        // an explicit cancel path can still interrupt it.
        return;
    }

    if (HasAuthority())
    {
        ServerFire_Implementation(ClientOrigin, ClientDirection.GetSafeNormal(), bAiming);
    }
    else
    {
        // Predict presentation only when the same local resolver that drives
        // the reticle sees a live hostile. A rejected off-target click must
        // not look like a successful shot while the server correctly refuses
        // its ammo and damage.
        bool bCanPredictPresentation = true;
        const APawn* OwnerPawn = Cast<APawn>(GetOwner());
        const bool bSignalBotPawn = OwnerPawn
            && Cast<ABMSignalBotController>(OwnerPawn->GetController()) != nullptr;
        const ABMPlayerCharacter* PlayerOwner = Cast<ABMPlayerCharacter>(OwnerPawn);
        const bool bHumanPlayerPawn = PlayerOwner && !bSignalBotPawn;
        if (bHumanPlayerPawn && GetWorld())
        {
            const ABMCombatCharacter* CrosshairTarget = PlayerOwner->HasLocalAimSnapshot()
                ? PlayerOwner->GetLocalAimTarget()
                : nullptr;
            if (!PlayerOwner->HasLocalAimSnapshot())
            {
                const float CrosshairRange = FMath::Max(
                    100.0f,
                    WeaponData->Balance.RangeMeters * 100.0f);
                CrosshairTarget = BMCombatTargeting::ResolveLiveHostileCrosshairTarget(
                    GetWorld(),
                    PlayerOwner,
                    ClientOrigin,
                    ClientDirection,
                    CrosshairRange,
                    this);
            }
            bCanPredictPresentation = CrosshairTarget != nullptr;
#if !UE_BUILD_SHIPPING
            if (!bCanPredictPresentation
                && (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe"))
                    || FParse::Param(FCommandLine::Get(), TEXT("BM_AimDebug"))))
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian combat presentation: predicted fire suppressed reason=local_crosshair_not_on_live_enemy"));
            }
#endif
        }
        if (bCanPredictPresentation)
        {
            PlayPredictedFirePresentation(ClientOrigin, ClientDirection.GetSafeNormal(), bAiming);
        }
        ServerFire(ClientOrigin, ClientDirection.GetSafeNormal(), bAiming);
    }
}

void ABMWeapon::PlayPredictedFirePresentation(
    const FVector& ClientOrigin,
    const FVector& ClientDirection,
    const bool bAiming)
{
    if (!WeaponData || !FireModeComponent || !RecoilComponent || !GetWorld() || GetMagazineAmmo() <= 0)
    {
        return;
    }

    const float WorldTime = GetWorld()->GetTimeSeconds();
    const float FireInterval = 60.0f / FMath::Max(1.0f, WeaponData->Balance.FireRateRPM);
    if ((WorldTime - LastPredictedFireTime) < FireInterval)
    {
        return;
    }

    LastPredictedFireTime = WorldTime;
    PresentationShotPulse = 1.0f;
    RecoilComponent->AddImpulse(
        WeaponData->Balance.RecoilPitch * GetAttachmentRecoilMultiplier(),
        FMath::FRandRange(-WeaponData->Balance.RecoilYaw, WeaponData->Balance.RecoilYaw) * GetAttachmentRecoilMultiplier());
    FireModeComponent->RegisterShot(
        WorldTime,
        WeaponData->Balance.ShotSpreadPenaltyDegrees,
        WeaponData->Balance.AccuracyRecoveryDegreesPerSecond,
        WeaponData->Balance.MaxAccuracyPenaltyDegrees);

    const FVector SafeDirection = ClientDirection.GetSafeNormal();
    const FVector Start = GetMuzzleLocation().IsNearlyZero() ? ClientOrigin : GetMuzzleLocation();
    const FVector End = Start + SafeDirection * WeaponData->Balance.RangeMeters * 100.0f;
    ApplyFallbackFirePresentation(Start, End, false, false, false, IsSuppressed(), EBMSurfaceType::Concrete);
    // Recoil and muzzle presentation are predicted already; play the shot
    // locally as well so a remote client does not wait for the server
    // multicast before the weapon feels responsive. The confirmed multicast
    // suppresses only this duplicate shot sound, while keeping impact audio
    // and hit markers authoritative.
    PlayFireSoundAtLocation(Start, IsSuppressed());
    OnWeaponEvent.Broadcast(TEXT("FirePredicted"));
    (void)bAiming;
}

void ABMWeapon::ServerFire_Implementation(
    const FVector_NetQuantize Origin,
    const FVector_NetQuantizeNormal Direction,
    const bool bAiming)
{
#if !UE_BUILD_SHIPPING
    const bool bCombatProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe"));
    const bool bCombatShowcase = FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcaseAutoFire"))
        || FParse::Param(FCommandLine::Get(), TEXT("BM_CombatRoleProbe"));
    const int32 MagazineBefore = GetMagazineAmmo();
#else
    constexpr bool bCombatProbe = false;
    constexpr bool bCombatShowcase = false;
    constexpr int32 MagazineBefore = 0;
#endif

    if (!WeaponData || !IsValidClientShot(Origin, Direction))
    {
#if !UE_BUILD_SHIPPING
        if (bCombatProbe || bCombatShowcase || FParse::Param(FCommandLine::Get(), TEXT("BM_AimDebug")))
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian combat trace: server rejected shot owner=%s"),
                *GetNameSafe(GetOwner()));
        }
#endif
        return;
    }

    // The player submits the rendered/crosshair ray. The server validates
    // that ray against the controller view below, but the controller ray is
    // never a second aim source: a one-frame presentation or replication
    // difference must not make the red reticle disagree with damage.
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    const bool bSignalBotPawn = OwnerPawn != nullptr
        && Cast<ABMSignalBotController>(OwnerPawn->GetController()) != nullptr;
    const bool bHumanPlayerPawn = Cast<ABMPlayerCharacter>(OwnerPawn) != nullptr && !bSignalBotPawn;
    const ABMPlayerCharacter* PlayerOwner = bHumanPlayerPawn
        ? Cast<ABMPlayerCharacter>(GetOwner())
        : nullptr;
    const APlayerController* PlayerController = PlayerOwner
        ? Cast<APlayerController>(PlayerOwner->GetController())
        : (OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr);
    // Do not infer the damage policy from APawn::IsPlayerControlled(). On a
    // listen server or during standalone possession hand-off, that flag can
    // briefly be false even though this is the real player character. The
    // concrete gameplay class is the authoritative ownership boundary: every
    // human ABMPlayerCharacter shot must be crosshair-gated, while
    // ABMPlayerCharacter bodies owned by ABMSignalBotController retain their
    // normal explicit AI-ray behavior.
    const bool bStrictPlayerCrosshair = bHumanPlayerPawn;
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatFallbackProbe")))
    {
        if (ABMSignalControlGameMode* SignalGameMode = GetWorld()
            ? GetWorld()->GetAuthGameMode<ABMSignalControlGameMode>()
            : nullptr)
        {
            // Keep the fallback target off-ray while history captures it, then
            // put the live target onto the exact submitted ray immediately
            // before the normal current-pose resolver runs.
            SignalGameMode->PrepareCombatFallbackCurrentPose(GetOwner());
        }
    }
#endif
    // Use the submitted camera origin after the bounded pawn-distance check in
    // IsValidClientShot. Replacing it with the server camera was causing a
    // moving remote pawn's rendered ray and authoritative ray to diverge by a
    // frame. The submitted origin is still rejected when it is not close to
    // the owned pawn, so it cannot become an arbitrary free camera.
    FVector AuthoritativeOrigin = Origin;
    FVector AuthoritativeDirection = Direction.GetSafeNormal();
    AActor* PlayerCrosshairTarget = nullptr;
    // A player shot is never allowed to damage a character until the
    // canonical submitted/rendered ray resolves a non-null live hostile and
    // remains inside the server controller's narrow integrity cone. nullptr
    // is never valid: the later multi-hit weapon trace can still encounter a
    // character behind a component that the shared crosshair resolver did not
    // resolve.
    bool bPlayerCrosshairAgreesWithServerView = !bStrictPlayerCrosshair;
    if (PlayerController && GetWorld())
    {
        const FVector ServerViewDirection = GetPlayerRenderedViewDirection(
            GetWorld(),
            OwnerPawn,
            PlayerController);
        // This is deliberately only packet-integrity validation. The actual
        // target is resolved later, inside the same historical-pose scope as
        // the weapon trace. Resolving it here would compare a current target
        // against a rewound hitbox and intermittently fail the exact final
        // component gate for moving enemies.
        const bool bClientRayWithinServerViewCone = IsViewDirectionWithinCone(
            ServerViewDirection,
            AuthoritativeDirection,
            PlayerShotViewConeDegrees);
        bPlayerCrosshairAgreesWithServerView = bClientRayWithinServerViewCone;
#if !UE_BUILD_SHIPPING
        if (bCombatProbe || bCombatShowcase || FParse::Param(FCommandLine::Get(), TEXT("BM_AimDebug")))
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian combat validation: strict_player=%s target_resolution=deferred cone_ok=%s aim_validated=%s canonical_direction=%s submitted_direction=%s server_direction=%s"),
                bStrictPlayerCrosshair ? TEXT("yes") : TEXT("no"),
                bClientRayWithinServerViewCone ? TEXT("yes") : TEXT("no"),
                bPlayerCrosshairAgreesWithServerView ? TEXT("yes") : TEXT("no"),
                *AuthoritativeDirection.ToString(),
                *Direction.ToString(),
                *ServerViewDirection.ToString());
        }
#endif
    }

    if (AmmunitionComponent && AmmunitionComponent->IsReloading())
    {
        CancelReload();
        return;
    }

    if (const ABMGameState* State = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
        State && State->bSignalControlMode && !State->bMatchLive)
    {
        return;
    }

    const float FireInterval = 60.0f / FMath::Max(1.0f, WeaponData->Balance.FireRateRPM);
    if (!FireModeComponent->CanFire(GetWorld()->GetTimeSeconds(), FireInterval, WeaponData->FireMode))
    {
        return;
    }

    // PerformAuthoritativeShot resolves the same current/historical hostile
    // hit that protects damage. It consumes ammo only after that resolution
    // succeeds, so a moving target cannot create an accepted round with no
    // corresponding health change.
    const bool bShotAccepted = PerformAuthoritativeShot(
        AuthoritativeOrigin,
        AuthoritativeDirection,
        bAiming,
        bStrictPlayerCrosshair,
        PlayerCrosshairTarget,
        bPlayerCrosshairAgreesWithServerView);
#if !UE_BUILD_SHIPPING
    if (bShotAccepted && (bCombatProbe || bCombatShowcase || FParse::Param(FCommandLine::Get(), TEXT("BM_AimDebug"))))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat trace: server shot accepted owner=%s magazine_before=%d magazine_after=%d"),
            *GetNameSafe(GetOwner()),
            MagazineBefore,
            GetMagazineAmmo());
    }
#endif
}

bool ABMWeapon::PerformAuthoritativeShot(
    const FVector& Origin,
    const FVector& Direction,
    const bool bAiming,
    const bool bStrictPlayerCrosshair,
    AActor* PlayerCrosshairTarget,
    const bool bPlayerCrosshairAgreesWithServerView)
{
    // Keep the final damage path fail-closed even if another caller later
    // supplies an incorrect policy flag. A player-class owner can never fall
    // back to the AI ray path, and null/invalid crosshair agreement is never
    // treated as a valid target.
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    const bool bSignalBotPawn = OwnerPawn != nullptr
        && Cast<ABMSignalBotController>(OwnerPawn->GetController()) != nullptr;
    const bool bPlayerClassOwner = Cast<ABMPlayerCharacter>(GetOwner()) != nullptr && !bSignalBotPawn;
    const bool bEffectiveStrictPlayerCrosshair = !bSignalBotPawn
        && (bPlayerClassOwner || bStrictPlayerCrosshair);
    bool bEffectiveCrosshairAgreement = !bEffectiveStrictPlayerCrosshair
        || (bPlayerCrosshairAgreesWithServerView && Cast<ABMCombatCharacter>(PlayerCrosshairTarget) != nullptr);

    const float WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    const auto ConsumeAuthoritativeRound = [this]() -> bool
    {
        if (!AmmunitionComponent || AmmunitionComponent->ConsumeRound())
        {
            return AmmunitionComponent != nullptr;
        }

        const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        if (Now - LastDryFireFeedbackTime >= 0.16f)
        {
            LastDryFireFeedbackTime = Now;
            MulticastWeaponEvent(TEXT("DryFire"));
        }
        return false;
    };
    const float AccuracyPenalty = FireModeComponent
        ? FireModeComponent->GetAccuracyPenalty(WorldTime, WeaponData->Balance.AccuracyRecoveryDegreesPerSecond)
        : 0.0f;
    const float BaseSpreadDegrees = bAiming ? WeaponData->Balance.ADSSpreadDegrees : WeaponData->Balance.HipSpreadDegrees;
    const float SpreadRadians = FMath::DegreesToRadians(FMath::Max(0.0f, BaseSpreadDegrees + AccuracyPenalty));
    const bool bPlayerHitscanShot = bEffectiveStrictPlayerCrosshair && !WeaponData->bProjectileWeapon;

    // Projectile damage is resolved at impact time, so it does not use the
    // hitscan rewind scope below. Still resolve its player target from the
    // submitted ray before constraining the projectile's damage radius.
    if (bEffectiveStrictPlayerCrosshair && WeaponData->bProjectileWeapon && GetWorld())
    {
        FHitResult ProjectileCrosshairHit;
        PlayerCrosshairTarget = BMCombatTargeting::ResolveLiveHostileCrosshairTarget(
            GetWorld(),
            Cast<ABMCombatCharacter>(GetOwner()),
            Origin,
            Direction,
            WeaponData->Balance.RangeMeters * 100.0f,
            this,
            &ProjectileCrosshairHit);
        bEffectiveCrosshairAgreement =
            bPlayerCrosshairAgreesWithServerView
            && PlayerCrosshairTarget != nullptr;
        if (!bEffectiveCrosshairAgreement
            || !PlayerCrosshairTarget
            || !ProjectileCrosshairHit.bBlockingHit
            || ProjectileCrosshairHit.GetActor() != PlayerCrosshairTarget
            || !BMCombatTargeting::IsCombatAimHit(
                Cast<ABMCombatCharacter>(PlayerCrosshairTarget),
                ProjectileCrosshairHit))
        {
            return false;
        }
    }

    if (WeaponData->bProjectileWeapon && !ConsumeAuthoritativeRound())
    {
        return false;
    }

    bool bUseValidatedPlayerCenterRay =
        bEffectiveStrictPlayerCrosshair
        && bEffectiveCrosshairAgreement
        && Cast<ABMCombatCharacter>(PlayerCrosshairTarget) != nullptr;
    // One stream per authoritative trigger pull keeps the pellet directions
    // independent while ensuring every pellet count uses the same spread
    // contract. The server remains the only source of gameplay randomness.
    FRandomStream ShotSpreadRandomStream(FMath::Rand());

    if (WeaponData->bProjectileWeapon)
    {
        const FVector ProjectileDirection = bUseValidatedPlayerCenterRay
            ? Direction
            : GetSpreadDirection(Direction, SpreadRadians, ShotSpreadRandomStream);
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Owner = GetOwner();
        SpawnParameters.Instigator = Cast<APawn>(GetOwner());
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        UClass* ProjectileClass = WeaponData->ProjectileClass ? WeaponData->ProjectileClass.Get() : ABMProjectile::StaticClass();
        if (ABMProjectile* Projectile = GetWorld()->SpawnActor<ABMProjectile>(ProjectileClass, Origin, ProjectileDirection.Rotation(), SpawnParameters))
        {
            Projectile->InitializeProjectile(
                ProjectileDirection,
                WeaponData->ProjectileSpeed,
                WeaponData->Balance.BaseDamage * GetAttachmentDamageMultiplier(),
                WeaponData->ProjectileRadius,
                WeaponData->ProjectileGravityScale);
            // Preserve explosive visuals and world impact for a player shot,
            // but do not let the projectile's area radius grant character
            // damage to anything that was merely near the intended target.
            // AI-owned weapons keep their normal unconstrained blast.
            Projectile->SetPlayerCrosshairDamageConstraint(
                bEffectiveStrictPlayerCrosshair,
                bEffectiveCrosshairAgreement ? PlayerCrosshairTarget : nullptr);
            Projectile->AudioProfile = WeaponData->AudioProfile;
            Projectile->VFXProfile = WeaponData->VFXProfile;
            RecoilComponent->AddImpulse(
                WeaponData->Balance.RecoilPitch * GetAttachmentRecoilMultiplier(),
                FMath::FRandRange(-WeaponData->Balance.RecoilYaw, WeaponData->Balance.RecoilYaw) * GetAttachmentRecoilMultiplier());
            FireModeComponent->RegisterShot(
                WorldTime,
                WeaponData->Balance.ShotSpreadPenaltyDegrees,
                WeaponData->Balance.AccuracyRecoveryDegreesPerSecond,
                WeaponData->Balance.MaxAccuracyPenaltyDegrees);
            LastShotEnd = Origin + ProjectileDirection * WeaponData->Balance.RangeMeters * 100.0f;
            if (ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>())
            {
                GameMode->RegisterCombatLocation(Origin);
            }
            const bool bSuppressed = IsSuppressed();
            UAISense_Hearing::ReportNoiseEvent(
                GetWorld(),
                Origin,
                bSuppressed ? 0.25f : 1.0f,
                GetOwner(),
                WeaponData->Balance.SuppressionRadius,
                TEXT("ProjectileLaunch"));
            MulticastFireEffects(GetMuzzleLocation(), LastShotEnd, false, false, false, bSuppressed, EBMSurfaceType::Concrete);
            OnWeaponEvent.Broadcast(TEXT("Fire"));
            return true;
        }
        return false;
    }

    const int32 PelletCount = FMath::Max(1, WeaponData->Balance.PelletCount);
    const float MaxDistance = WeaponData->Balance.RangeMeters * 100.0f;

    float ShooterPingMilliseconds = 0.0f;
    if (const APawn* ShooterPawn = Cast<APawn>(GetOwner()))
    {
        if (const APlayerState* ShooterPlayerState = ShooterPawn->GetPlayerState())
        {
            ShooterPingMilliseconds = FMath::Max(0.0f, ShooterPlayerState->GetPingInMilliseconds());
        }
    }
    const float RewindSeconds = FMath::Clamp(
        ShooterPingMilliseconds * 0.0005f,
        0.0f,
        UBMCombatHistoryComponent::MaxRewindSeconds);

    // The HUD resolves the target from the current client-visible pose. Save
    // the server's current-pose answer before rewinding so a moving target
    // cannot become a false negative merely because the bounded history does
    // not contain the exact pose represented by this submitted ray. This is
    // only a fallback candidate; the same ray is resolved again after the
    // rewind scope is restored and the actor identity must still match.
    AActor* CurrentServerCrosshairTarget = nullptr;
    FHitResult CurrentServerCrosshairHit;
    FHitResult ValidatedPlayerCrosshairHit;
    if (bEffectiveStrictPlayerCrosshair && GetWorld())
    {
        CurrentServerCrosshairTarget = BMCombatTargeting::ResolveLiveHostileCrosshairTarget(
            GetWorld(),
            Cast<ABMCombatCharacter>(GetOwner()),
            Origin,
            Direction,
            MaxDistance,
            this,
            &CurrentServerCrosshairHit);
    }

    FBMCombatHistoryRewindScope RewindScope(
        RewindSeconds > KINDA_SMALL_NUMBER ? GetWorld() : nullptr,
        GetOwner(),
        WorldTime - RewindSeconds);
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe"))
        || FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcaseAutoFire")))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat rewind: shooter=%s ping_ms=%.1f rewind_ms=%.1f targets=%d"),
            *GetNameSafe(GetOwner()),
            ShooterPingMilliseconds,
            RewindSeconds * 1000.0f,
            RewindScope.GetRewoundCharacterCount());
    }
#endif

    // Resolve the player's target after rewind, using the exact same origin
    // and direction that the weapon trace will use. This is the critical
    // moving-target/network invariant: the final component gate must compare
    // against the hitbox at the shot's historical pose, not the target's
    // current pose from before the rewind scope was entered.
    bool bUsedCurrentPoseFallback = false;
    if (bEffectiveStrictPlayerCrosshair && GetWorld())
    {
        FHitResult RewoundCrosshairHit;
        PlayerCrosshairTarget = BMCombatTargeting::ResolveLiveHostileCrosshairTarget(
            GetWorld(),
            Cast<ABMCombatCharacter>(GetOwner()),
            Origin,
            Direction,
            MaxDistance,
            this,
            &RewoundCrosshairHit);
        bEffectiveCrosshairAgreement =
            bPlayerCrosshairAgreesWithServerView
            && PlayerCrosshairTarget != nullptr;
        if (bEffectiveCrosshairAgreement)
        {
            ValidatedPlayerCrosshairHit = RewoundCrosshairHit;
        }

        // A target can move out of the ray between the current server pose
        // and the half-RTT historical pose. If that happens, restore every
        // rewound component and use the exact current-pose hit captured before
        // entering the rewind scope. Re-running the resolver here created a
        // second race window and could consume a round after the final query
        // had already become a miss.
        if (PlayerCrosshairTarget != CurrentServerCrosshairTarget && CurrentServerCrosshairTarget)
        {
            RewindScope.RestoreCurrentPoses();
            PlayerCrosshairTarget = CurrentServerCrosshairTarget;
            ValidatedPlayerCrosshairHit = CurrentServerCrosshairHit;
            bUsedCurrentPoseFallback = true;
            bEffectiveCrosshairAgreement =
                bPlayerCrosshairAgreesWithServerView
                && PlayerCrosshairTarget != nullptr;
        }

        bUseValidatedPlayerCenterRay =
            bEffectiveCrosshairAgreement
            && Cast<ABMCombatCharacter>(PlayerCrosshairTarget) != nullptr;
#if !UE_BUILD_SHIPPING
        if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe"))
            || FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcaseAutoFire"))
            || FParse::Param(FCommandLine::Get(), TEXT("BM_AimDebug")))
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian combat validation: rewound_target=%s rewound_component=%s current_server_target=%s pose=%s cone_agreement=%s aim_validated=%s rewind_ms=%.1f"),
                *GetNameSafe(PlayerCrosshairTarget),
                *GetNameSafe(RewoundCrosshairHit.GetComponent()),
                *GetNameSafe(CurrentServerCrosshairTarget),
                bUsedCurrentPoseFallback ? TEXT("current_fallback") : TEXT("historical"),
                bPlayerCrosshairAgreesWithServerView ? TEXT("yes") : TEXT("no"),
                bEffectiveCrosshairAgreement ? TEXT("yes") : TEXT("no"),
                RewindSeconds * 1000.0f);
            if (!PlayerCrosshairTarget)
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian combat validation: player damage blocked aim_validation reason=rewound_ray_no_live_hostile current_server_target=%s current_fallback=no"),
                    *GetNameSafe(CurrentServerCrosshairTarget));
            }
            else if (bUsedCurrentPoseFallback)
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian combat validation: current_pose_fallback target=%s component=%s"),
                    *GetNameSafe(PlayerCrosshairTarget),
                    *GetNameSafe(CurrentServerCrosshairHit.GetComponent()));
            }
        }
#endif
    }

    // This is the atomic aim/ammo boundary. The resolved hit is either the
    // historical hitbox result or the exact current-pose result captured
    // before rewind. If neither exists, reject before touching ammo or fire
    // presentation.
    if (bEffectiveStrictPlayerCrosshair)
    {
        const ABMCombatCharacter* ValidatedTarget = Cast<ABMCombatCharacter>(PlayerCrosshairTarget);
        const bool bHasValidatedCrosshairHit =
            bEffectiveCrosshairAgreement
            && ValidatedTarget != nullptr
            && BMCombatTargeting::IsLiveHostileTarget(
                Cast<ABMCombatCharacter>(GetOwner()),
                ValidatedTarget)
            && ValidatedPlayerCrosshairHit.bBlockingHit
            && ValidatedPlayerCrosshairHit.GetActor() == PlayerCrosshairTarget
            && BMCombatTargeting::IsCombatAimHit(ValidatedTarget, ValidatedPlayerCrosshairHit);
        if (!bHasValidatedCrosshairHit)
        {
#if !UE_BUILD_SHIPPING
            if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe"))
                || FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcaseAutoFire"))
                || FParse::Param(FCommandLine::Get(), TEXT("BM_AimDebug")))
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian combat validation: player shot rejected before ammo reason=no_atomic_crosshair_hit target=%s current_target=%s"),
                    *GetNameSafe(PlayerCrosshairTarget),
                    *GetNameSafe(CurrentServerCrosshairTarget));
            }
#endif
            return false;
        }
    }

    // A human hitscan shot must have a round available, but it cannot consume
    // that round until the validated center hit has actually changed health.
    // This closes the last aim/ammo mismatch: a target can die or otherwise
    // become non-damageable between the resolver and ApplyHitDamage.
    if (bPlayerHitscanShot
        && (!AmmunitionComponent
            || AmmunitionComponent->IsReloading()
            || AmmunitionComponent->GetMagazineAmmo() <= 0))
    {
        ConsumeAuthoritativeRound();
        return false;
    }

    if (!bPlayerHitscanShot && !ConsumeAuthoritativeRound())
    {
        return false;
    }

    FHitResult FirstHit;
    bool bAnyHit = false;
    bool bAnyDamageApplied = false;
    bool bAnyHeadshot = false;
    bool bAnySurfaceImpact = false;
    bool bLoggedOffCrosshairBlock = false;
    EBMSurfaceType ImpactSurface = EBMSurfaceType::Concrete;
    float HostileDamageRemaining = Cast<ABMEnemyCharacter>(GetOwner())
        ? BMCombatTuning::HostileDamagePerShotCap
        : TNumericLimits<float>::Max();

    for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
    {
        const FVector ShotDirection = bUseValidatedPlayerCenterRay && PelletIndex == 0
            ? Direction
            : GetSpreadDirection(Direction, SpreadRadians, ShotSpreadRandomStream);
        const FVector End = Origin + ShotDirection * MaxDistance;
        TArray<FHitResult> Hits;
        FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BlackMeridianWeaponTrace), true, GetOwner());
        // Use the authored/rendered mesh for all authoritative visibility
        // pellets. Pawn/navigation collision stays on the runtime proxy, but
        // a simple visibility body can turn a recessed or open prop into a
        // false bullet stop.
        QueryParams.bTraceComplex = true;
        QueryParams.AddIgnoredActor(this);
        QueryParams.bReturnPhysicalMaterial = true;

        bool bHit = false;
        // The validated center pellet uses the exact hit returned by the
        // shared reticle query. This matters when that query used its narrow
        // swept tolerance: re-running a mathematical line here would recreate
        // the very reticle-red/bullet-no-damage mismatch this contract is
        // meant to prevent. The resolved hit has already passed visibility,
        // hostility, and the distance-aware aim gate.
        if (bUseValidatedPlayerCenterRay
            && PelletIndex == 0
            && ValidatedPlayerCrosshairHit.bBlockingHit
            && ValidatedPlayerCrosshairHit.GetActor() == PlayerCrosshairTarget
            && BMCombatTargeting::IsCombatAimHit(
                Cast<ABMCombatCharacter>(PlayerCrosshairTarget),
                ValidatedPlayerCrosshairHit))
        {
            Hits.Add(ValidatedPlayerCrosshairHit);
            bHit = true;
        }
        else
        {
            bHit = GetWorld()->LineTraceMultiByChannel(Hits, Origin, End, ECC_Visibility, QueryParams);
        }
#if !UE_BUILD_SHIPPING
        if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe"))
            || FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcaseAutoFire"))
            || FParse::Param(FCommandLine::Get(), TEXT("BM_AimDebug")))
        {
            FString HitNames;
            for (const FHitResult& Hit : Hits)
            {
                if (!HitNames.IsEmpty())
                {
                    HitNames += TEXT(",");
                }
                HitNames += GetNameSafe(Hit.GetActor());
            }
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian combat trace: server trace origin=%s end=%s hit=%s impact=%s"),
                *Origin.ToString(),
                *End.ToString(),
                *HitNames,
                Hits.Num() > 0 ? *Hits[0].ImpactPoint.ToString() : TEXT("None"));
        }
#endif
        if (!bHit)
        {
            LastShotEnd = End;
            continue;
        }

        if (!bAnySurfaceImpact)
        {
            ImpactSurface = ResolveSurfaceType(Hits[0]);
            bAnySurfaceImpact = true;
        }

        float DamageScale = 1.0f;
        bool bPenetrated = false;
        int32 PenetrationCount = 0;
        for (const FHitResult& Hit : Hits)
        {
            if (ABMArtDirectedProp* DestructibleProp = Cast<ABMArtDirectedProp>(Hit.GetActor());
                DestructibleProp && DestructibleProp->IsDestructible() && !DestructibleProp->IsDestroyed())
            {
                // Destructible cover is an authoritative obstacle: a shot
                // can weaken or remove it, but it cannot continue through the
                // prop in the same trace. The existing surface-impact path
                // still supplies the muzzle, impact, and audio feedback.
                DestructibleProp->ApplyWeaponImpactAtLocation(
                    WeaponData->Balance.BaseDamage * GetAttachmentDamageMultiplier() * DamageScale,
                    Hit.ImpactPoint,
                    Hit.ImpactNormal);
                break;
            }

            if (Hit.GetActor() && Hit.GetActor()->FindComponentByClass<UBMHealthComponent>())
            {
                // Player hitscan is deliberately crosshair-gated. Spread may
                // still produce readable bullet impacts, but it cannot turn
                // a shot aimed beside an enemy into character damage. Bots
                // and server-owned showcase actors keep their normal target
                // ray because they do not have a player crosshair.
                const ABMCombatCharacter* CrosshairTargetCharacter = Cast<ABMCombatCharacter>(PlayerCrosshairTarget);
                const bool bValidatedCenterRay = bUseValidatedPlayerCenterRay && PelletIndex == 0;
                const bool bHitDedicatedCrosshairComponent =
                    bEffectiveCrosshairAgreement
                    && BMCombatTargeting::IsCombatAimHit(CrosshairTargetCharacter, Hit);
                const bool bHitValidatedCrosshairActor =
                    bValidatedCenterRay
                    && bEffectiveCrosshairAgreement
                    && CrosshairTargetCharacter
                    && Hit.GetActor() == PlayerCrosshairTarget;
                if (bEffectiveStrictPlayerCrosshair &&
                    !bHitDedicatedCrosshairComponent
                    && !bHitValidatedCrosshairActor)
                {
#if !UE_BUILD_SHIPPING
                    if (!bLoggedOffCrosshairBlock)
                    {
                        UE_LOG(
                            LogTemp,
                            Display,
                            TEXT("Black Meridian combat validation: player damage blocked off_crosshair hit=%s hit_component=%s crosshair_target=%s view_agreement=%s strict_player=%s"),
                            *GetNameSafe(Hit.GetActor()),
                            *GetNameSafe(Hit.GetComponent()),
                            *GetNameSafe(PlayerCrosshairTarget),
                            bEffectiveCrosshairAgreement ? TEXT("yes") : TEXT("no"),
                            bEffectiveStrictPlayerCrosshair ? TEXT("yes") : TEXT("no"));
                        bLoggedOffCrosshairBlock = true;
                    }
#endif
                    break;
                }

                const float Distance = FVector::Distance(Origin, Hit.ImpactPoint);
                const float Falloff = CalculateDistanceFalloff(Distance);
                const bool bDamageApplied = ApplyHitDamage(
                    Hit,
                    ShotDirection,
                    DamageScale * Falloff,
                    bPenetrated,
                    ResolveSurfaceType(Hit),
                    PenetrationCount,
                    bAnyHeadshot,
                    HostileDamageRemaining,
                    bEffectiveStrictPlayerCrosshair,
                    PlayerCrosshairTarget,
                    bEffectiveCrosshairAgreement,
                    bValidatedCenterRay);
                bAnyDamageApplied = bAnyDamageApplied || bDamageApplied;
                if (!bAnyHit)
                {
                    FirstHit = Hit;
                }
                bAnyHit = true;
                break;
            }

            const EBMSurfaceType SurfaceType = ResolveSurfaceType(Hit);
            const UBMSurfaceRuleDataAsset* SurfaceRule = WeaponData->FindSurfaceRule(SurfaceType);
            if (SurfaceRule && SurfaceRule->AllowsPenetration(PenetrationCount))
            {
                DamageScale *= SurfaceRule->DamageMultiplier;
                bPenetrated = true;
                ++PenetrationCount;
                continue;
            }

            // Greybox geometry predates physical-material assignment. Keep this
            // explicit fallback until authored materials are available.
            if (Hit.GetActor() && Hit.GetActor()->ActorHasTag(TEXT("ThinCover")) && PenetrationCount == 0)
            {
                DamageScale *= WeaponData->Balance.PenetrationMultiplier;
                bPenetrated = true;
                ++PenetrationCount;
                continue;
            }

            break;
        }
        LastShotEnd = bAnyHit ? FirstHit.ImpactPoint : End;
    }

    if (bPlayerHitscanShot)
    {
        if (!bAnyDamageApplied)
        {
#if !UE_BUILD_SHIPPING
            if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe"))
                || FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcaseAutoFire"))
                || FParse::Param(FCommandLine::Get(), TEXT("BM_AimDebug")))
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian combat validation: player shot rejected after trace reason=no_damage_applied target=%s"),
                    *GetNameSafe(PlayerCrosshairTarget));
            }
#endif
            return false;
        }

        // Ammo is committed only after authoritative health mutation reports
        // a positive result. The preflight above makes this consume expected
        // to succeed; keeping the failure path preserves dry-fire behavior if
        // a future caller changes ammunition state during the trace.
        if (!ConsumeAuthoritativeRound())
        {
#if !UE_BUILD_SHIPPING
            if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe"))
                || FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcaseAutoFire"))
                || FParse::Param(FCommandLine::Get(), TEXT("BM_AimDebug")))
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian combat validation: player shot rejected after trace reason=ammo_commit_failed target=%s"),
                    *GetNameSafe(PlayerCrosshairTarget));
            }
#endif
            return false;
        }
    }

    RecoilComponent->AddImpulse(
        WeaponData->Balance.RecoilPitch * GetAttachmentRecoilMultiplier(),
        FMath::FRandRange(-WeaponData->Balance.RecoilYaw, WeaponData->Balance.RecoilYaw) * GetAttachmentRecoilMultiplier());
    FireModeComponent->RegisterShot(
        WorldTime,
        WeaponData->Balance.ShotSpreadPenaltyDegrees,
        WeaponData->Balance.AccuracyRecoveryDegreesPerSecond,
        WeaponData->Balance.MaxAccuracyPenaltyDegrees);
    const bool bSuppressed = IsSuppressed();
    UAISense_Hearing::ReportNoiseEvent(
        GetWorld(),
        Origin,
        bSuppressed ? 0.25f : 1.0f,
        GetOwner(),
        WeaponData->Balance.SuppressionRadius,
        TEXT("Gunshot"));
    if (ABMGameMode* GameMode = GetWorld()->GetAuthGameMode<ABMGameMode>())
    {
        GameMode->RegisterCombatLocation(bAnyHit ? LastShotEnd : Origin);
    }
    MulticastFireEffects(
        GetMuzzleLocation(),
        LastShotEnd,
        bAnyDamageApplied,
        bAnyHeadshot,
        bAnySurfaceImpact,
        bSuppressed,
        ImpactSurface);
    OnWeaponEvent.Broadcast(
        bAnyHeadshot && bAnyDamageApplied
            ? TEXT("HeadshotHit")
            : (bAnyDamageApplied ? TEXT("Hit") : TEXT("Fire")));
    return true;
}

FVector ABMWeapon::GetSpreadDirection(
    const FVector& Direction,
    const float SpreadRadians,
    FRandomStream& RandomStream)
{
    const FVector SafeDirection = Direction.GetSafeNormal();
    if (SafeDirection.IsNearlyZero() || SpreadRadians <= KINDA_SMALL_NUMBER)
    {
        return SafeDirection;
    }

    return RandomStream.VRandCone(SafeDirection, FMath::Max(0.0f, SpreadRadians)).GetSafeNormal();
}

bool ABMWeapon::ApplyHitDamage(
    const FHitResult& Hit,
    const FVector& ShotDirection,
    const float DamageScale,
    const bool bPenetrated,
    const EBMSurfaceType SurfaceType,
    const int32 PenetrationCount,
    bool& bHeadshot,
    float& HostileDamageRemaining,
    const bool bStrictPlayerCrosshair,
    AActor* PlayerCrosshairTarget,
    const bool bPlayerCrosshairAgreesWithServerView,
    const bool bValidatedCenterRay)
{
    // Keep the player aim contract at the last function before health is
    // mutated. The earlier trace gate is intentionally duplicated here so a
    // future caller, spread/pellet refactor, or alternate weapon path cannot
    // accidentally turn an actor with a health component into aim assist.
    // Player damage is valid only when the live hostile enemy was under the
    // canonical crosshair ray, that ray passed the server view-cone check,
    // and this exact hit either struck its dedicated aim component or was the
    // validated center-ray component on that same resolved actor. The latter
    // keeps a cooked component-order difference from making a red reticle shot
    // consume ammo without applying its damage.
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    const bool bSignalBotPawn = OwnerPawn
        && Cast<ABMSignalBotController>(OwnerPawn->GetController()) != nullptr;
    const bool bHumanPlayerWeaponOwner = Cast<ABMPlayerCharacter>(GetOwner()) != nullptr && !bSignalBotPawn;
    bool bFinalPlayerAimGate = false;
    if (bHumanPlayerWeaponOwner)
    {
        const ABMCombatCharacter* CrosshairTarget = Cast<ABMCombatCharacter>(PlayerCrosshairTarget);
        bFinalPlayerAimGate =
            bStrictPlayerCrosshair
            && bPlayerCrosshairAgreesWithServerView
            && CrosshairTarget != nullptr
            && Hit.bBlockingHit
            && Hit.GetActor() == PlayerCrosshairTarget
            && (BMCombatTargeting::IsCombatAimHit(CrosshairTarget, Hit)
                || bValidatedCenterRay);
        if (!bFinalPlayerAimGate)
        {
#if !UE_BUILD_SHIPPING
            if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe"))
                || FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcaseAutoFire"))
                || FParse::Param(FCommandLine::Get(), TEXT("BM_AimDebug")))
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Black Meridian combat validation: player damage blocked final_gate hit=%s hit_component=%s crosshair_target=%s agreement=%s dedicated_component=%s"),
                    *GetNameSafe(Hit.GetActor()),
                    *GetNameSafe(Hit.GetComponent()),
                    *GetNameSafe(PlayerCrosshairTarget),
                    bPlayerCrosshairAgreesWithServerView ? TEXT("yes") : TEXT("no"),
                    BMCombatTargeting::IsCombatAimHit(CrosshairTarget, Hit) ? TEXT("yes") : TEXT("no"));
            }
#endif
            return false;
        }
    }

    UBMHealthComponent* Health = Hit.GetActor() ? Hit.GetActor()->FindComponentByClass<UBMHealthComponent>() : nullptr;
    if (!Health || !WeaponData)
    {
        return false;
    }

    if (const ABMCombatCharacter* Shooter = Cast<ABMCombatCharacter>(GetOwner()))
    {
        if (const ABMCombatCharacter* Target = Cast<ABMCombatCharacter>(Hit.GetActor()); Target && Target->GetTeam() == Shooter->GetTeam())
        {
            const UBMGameUserSettings* Settings = GEngine ? Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
            if (!Settings || !Settings->bFriendlyFireEnabled)
            {
                return false;
            }
        }
    }

    const EBMHitZone HitZone = ResolveHitZone(Hit);
    FBMDamageContext DamageContext;
    DamageContext.RawDamage = WeaponData->Balance.BaseDamage;
    const float AttachmentDamageMultiplier = GetAttachmentDamageMultiplier();
    const ABMEnemyCharacter* EnemyShooter = Cast<ABMEnemyCharacter>(GetOwner());
    float ShooterDamageMultiplier = 1.0f;
    if (EnemyShooter && Cast<ABMPlayerCharacter>(Hit.GetActor()))
    {
        ShooterDamageMultiplier = EnemyShooter->GetDamageToPlayerMultiplier();
    }
    DamageContext.FinalDamage = DamageContext.RawDamage
        * AttachmentDamageMultiplier
        * WeaponData->GetHitZoneMultiplier(HitZone)
        * DamageScale
        * ShooterDamageMultiplier;
    const bool bHostilePlayerHit = EnemyShooter && Cast<ABMPlayerCharacter>(Hit.GetActor());
    if (bHostilePlayerHit)
    {
        if (HostileDamageRemaining <= KINDA_SMALL_NUMBER)
        {
            return false;
        }
        DamageContext.FinalDamage = FMath::Min(DamageContext.FinalDamage, HostileDamageRemaining);

        if (UBMSquadSubsystem* Squad = GetWorld() ? GetWorld()->GetSubsystem<UBMSquadSubsystem>() : nullptr)
        {
            DamageContext.FinalDamage = Squad->ConsumeHostileDamageBudget(
                Hit.GetActor(),
                DamageContext.FinalDamage,
                EnemyShooter->UsesReadableCombatPacing()
                    ? BMCombatTuning::OpeningWaveDamageWindowCap
                    : BMCombatTuning::HostileDamageWindowCap);
        }
        if (DamageContext.FinalDamage <= KINDA_SMALL_NUMBER)
        {
            return false;
        }
    }
    DamageContext.HitZone = HitZone;
    DamageContext.bHeadshot = HitZone == EBMHitZone::Head;
    DamageContext.bPenetrated = bPenetrated;
    DamageContext.SurfaceType = SurfaceType;
    DamageContext.PenetrationCount = PenetrationCount;
    DamageContext.InstigatorActor = GetOwner();
    // This assignment is intentionally after the final gate above. The
    // health component independently rejects a player-weapon context whose
    // validation bit was not earned by that gate.
    const bool bPlayerWeaponDamage = bHumanPlayerWeaponOwner;
    DamageContext.bPlayerWeaponDamage = bPlayerWeaponDamage;
    DamageContext.bPlayerWeaponAimValidated = bPlayerWeaponDamage && bFinalPlayerAimGate;
    const float HealthBefore = Health->Health;
    const float AppliedDamage = Health->ApplyDamage(DamageContext.FinalDamage, DamageContext);
    const bool bDamageApplied = AppliedDamage > KINDA_SMALL_NUMBER;
    if (bHostilePlayerHit)
    {
        HostileDamageRemaining = FMath::Max(0.0f, HostileDamageRemaining - AppliedDamage);
    }
    bHeadshot = bHeadshot || (bDamageApplied && DamageContext.bHeadshot);
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe"))
        || FParse::Param(FCommandLine::Get(), TEXT("BM_CombatShowcaseAutoFire"))
        || FParse::Param(FCommandLine::Get(), TEXT("BM_AimDebug")))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat trace: server damage applied shooter=%s target=%s damage=%.1f target_health_before=%.1f target_health_after=%.1f"),
            *GetNameSafe(GetOwner()),
            *GetNameSafe(Hit.GetActor()),
            AppliedDamage,
            HealthBefore,
            Health->Health);
    }
#endif

    if (!bDamageApplied)
    {
        return false;
    }

#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe")))
    {
        if (ABMSignalControlGameMode* SignalGameMode = GetWorld()
            ? GetWorld()->GetAuthGameMode<ABMSignalControlGameMode>()
            : nullptr)
        {
            SignalGameMode->RegisterCombatProbeDamage(
                GetOwner(),
                Hit.GetActor(),
                HealthBefore,
                Health->Health);
        }
    }
#endif

    if (ABMCombatCharacter* CombatTarget = Cast<ABMCombatCharacter>(Hit.GetActor()))
    {
        if (CombatTarget->SuppressionComponent)
        {
            CombatTarget->SuppressionComponent->AddSuppression(0.9f);
        }
        CombatTarget->MulticastHitReaction(ShotDirection, HitZone);
    }

    // The server remains the source of truth for damage, but the owning
    // player still needs an immediate directional response when a replicated
    // shot lands. This feeds the existing damage-direction HUD cue without
    // trusting any client-supplied hit result.
    if (ABMPlayerCharacter* PlayerTarget = Cast<ABMPlayerCharacter>(Hit.GetActor()))
    {
        PlayerTarget->NotifyDamageDirection(GetActorLocation());
    }

    if (Hit.GetActor())
    {
#if !UE_BUILD_SHIPPING
        DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 8.0f, DamageContext.bHeadshot ? FColor::Red : FColor::Yellow, false, 0.4f);
#endif
    }
    return true;
}

EBMHitZone ABMWeapon::ResolveHitZone(const FHitResult& Hit) const
{
    const AActor* HitActor = Hit.GetActor();
    if (!HitActor)
    {
        return EBMHitZone::Torso;
    }

    const float RelativeZ = Hit.ImpactPoint.Z - HitActor->GetActorLocation().Z;
    const FString Bone = Hit.BoneName.ToString().ToLower();
    if (Bone.Contains(TEXT("arm")) || Bone.Contains(TEXT("hand")))
    {
        return EBMHitZone::Arm;
    }
    const UCapsuleComponent* Capsule = HitActor->FindComponentByClass<UCapsuleComponent>();
    const float Height = Capsule ? FMath::Max(1.0f, Capsule->GetScaledCapsuleHalfHeight() * 2.0f) : 180.0f;
    const float NormalizedHeight = RelativeZ / Height;
    if (NormalizedHeight > 0.78f)
    {
        return EBMHitZone::Head;
    }
    if (NormalizedHeight < 0.30f)
    {
        return EBMHitZone::Leg;
    }
    return EBMHitZone::Torso;
}

EBMSurfaceType ABMWeapon::ResolveSurfaceType(const FHitResult& Hit) const
{
    if (Hit.PhysMaterial.IsValid())
    {
        switch (UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get()))
        {
        case SurfaceType1:
            return EBMSurfaceType::Concrete;
        case SurfaceType2:
            return EBMSurfaceType::Metal;
        case SurfaceType3:
            return EBMSurfaceType::Wood;
        case SurfaceType4:
            return EBMSurfaceType::Glass;
        case SurfaceType5:
            return EBMSurfaceType::SoftCover;
        default:
            break;
        }
    }

    const AActor* HitActor = Hit.GetActor();
    if (HitActor)
    {
        if (HitActor->ActorHasTag(TEXT("Surface.Metal")))
        {
            return EBMSurfaceType::Metal;
        }
        if (HitActor->ActorHasTag(TEXT("Surface.Wood")))
        {
            return EBMSurfaceType::Wood;
        }
        if (HitActor->ActorHasTag(TEXT("Surface.Glass")))
        {
            return EBMSurfaceType::Glass;
        }
        if (HitActor->ActorHasTag(TEXT("ThinCover")) || HitActor->ActorHasTag(TEXT("Surface.SoftCover")))
        {
            return EBMSurfaceType::SoftCover;
        }
    }
    return EBMSurfaceType::Concrete;
}

float ABMWeapon::CalculateDistanceFalloff(const float DistanceCentimeters) const
{
    if (!WeaponData)
    {
        return 1.0f;
    }

    const float DistanceMeters = DistanceCentimeters / 100.0f;
    if (DistanceMeters <= WeaponData->Balance.FalloffStartMeters)
    {
        return 1.0f;
    }
    return FMath::GetMappedRangeValueClamped(
        FVector2D(WeaponData->Balance.FalloffStartMeters, WeaponData->Balance.FalloffEndMeters),
        FVector2D(1.0f, 0.55f),
        DistanceMeters);
}

bool ABMWeapon::IsValidClientShot(const FVector& Origin, const FVector& Direction) const
{
    if (!GetOwner() || !Direction.IsNormalized())
    {
        return false;
    }

    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn)
    {
        return false;
    }

    const FVector OwnerLocation = OwnerPawn->GetActorLocation();
    if (FVector::DistSquared(OwnerLocation, Origin) > FMath::Square(180.0f))
    {
        return false;
    }

    // AI and server-owned showcase pawns submit an explicit authoritative ray
    // and do not have a player controller. Player-owned requests must also be
    // consistent with the controller's replicated view, otherwise a modified
    // client could keep a valid muzzle origin while shooting around cover or
    // behind itself. The tolerance absorbs ordinary network quantization and
    // the small camera/pawn rotation difference during movement.
    // Use the concrete player pawn class here too, but exclude Signal Control
    // bots. Bots deliberately reuse ABMPlayerCharacter for their body and
    // weapon presentation while submitting an explicit AI ray without an
    // APlayerController. Only a human player pawn must pass the controller
    // ownership check and later strict crosshair validation.
    const bool bSignalBotPawn = Cast<ABMSignalBotController>(OwnerPawn->GetController()) != nullptr;
    const bool bHumanPlayerPawn = Cast<ABMPlayerCharacter>(OwnerPawn) != nullptr && !bSignalBotPawn;
    const APlayerController* PlayerController = Cast<APlayerController>(OwnerPawn->GetController());
    if (bHumanPlayerPawn && !PlayerController)
    {
#if !UE_BUILD_SHIPPING
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Black Meridian combat validation: rejected player shot owner=%s reason=player_controller_unavailable"),
            *GetNameSafe(GetOwner()));
#endif
        return false;
    }
    if (!PlayerController)
    {
        return true;
    }

    const FVector ViewDirection = GetPlayerRenderedViewDirection(
        GetWorld(),
        OwnerPawn,
        PlayerController);
    // The server view is only a packet-integrity check; it is not aim assist.
    // Keep enough room for quantization, one-frame input timing, and a fast
    // local turn against a still-replicating controller view. The submitted
    // ray is still resolved through the narrow dedicated hitbox before any
    // player damage can be applied.
    constexpr float MaxAcceptedViewAngleDegrees = PlayerShotViewConeDegrees;
    const float ViewAlignment = FVector::DotProduct(ViewDirection.GetSafeNormal(), Direction.GetSafeNormal());
#if !UE_BUILD_SHIPPING
    // The explicit reload probe stages a remote pawn and immediately drives
    // the ordinary client fire path. On a listen server the probe transform
    // can arrive before the replicated controller rotation, even though the
    // submitted origin and ray are valid. Keep the normal player cone guard
    // intact; only this opt-in development probe is allowed through so the
    // reload timer can be exercised instead of being masked by setup timing.
    const bool bReloadProbeStagingWindow = FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe"))
        && FParse::Param(FCommandLine::Get(), TEXT("BM_ReloadProbe"));
    if (bReloadProbeStagingWindow)
    {
        return true;
    }
#endif
    const float MinimumViewAlignment = FMath::Cos(FMath::DegreesToRadians(MaxAcceptedViewAngleDegrees));
    if (!IsViewDirectionWithinCone(ViewDirection, Direction, MaxAcceptedViewAngleDegrees))
    {
#if !UE_BUILD_SHIPPING
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Black Meridian combat validation: rejected player shot owner=%s view_alignment=%.3f required=%.3f"),
            *GetNameSafe(GetOwner()),
            ViewAlignment,
            MinimumViewAlignment);
#endif
        return false;
    }

    return true;
}

bool ABMWeapon::IsViewDirectionWithinCone(
    const FVector& ViewDirection,
    const FVector& ShotDirection,
    const float MaxAngleDegrees)
{
    const FVector SafeViewDirection = ViewDirection.GetSafeNormal();
    const FVector SafeShotDirection = ShotDirection.GetSafeNormal();
    if (SafeViewDirection.IsNearlyZero() || SafeShotDirection.IsNearlyZero() || MaxAngleDegrees < 0.0f)
    {
        return false;
    }

    const float MinimumAlignment = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(MaxAngleDegrees, 0.0f, 180.0f)));
    return FVector::DotProduct(SafeViewDirection, SafeShotDirection) >= MinimumAlignment;
}

void ABMWeapon::Reload()
{
    if (!WeaponData || !AmmunitionComponent)
    {
        return;
    }

    if (HasAuthority())
    {
        ServerReload_Implementation();
    }
    else
    {
        if (!AmmunitionComponent->BeginReload())
        {
            return;
        }
        BeginReloadPresentation(AmmunitionComponent->IsEmpty());
        ServerReload();
    }
}

void ABMWeapon::StopFiring()
{
    if (FireModeComponent)
    {
        FireModeComponent->ResetBurst();
    }
    if (!HasAuthority())
    {
        ServerStopFiring();
    }
}

void ABMWeapon::ClearTransientPresentation()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(FireLightTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(MuzzleFlashTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(TracerTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(ImpactFlashTimerHandle);
    }
    DisableFireLight();
    DisableMuzzleFlash();
    DisableTracer();
    DisableImpactFlash();
}

void ABMWeapon::ServerStopFiring_Implementation()
{
    if (FireModeComponent)
    {
        FireModeComponent->ResetBurst();
    }
}

void ABMWeapon::ServerReload_Implementation()
{
    if (!WeaponData || !AmmunitionComponent || !GetWorld())
    {
        ClientReloadRejected();
        return;
    }

    if (AmmunitionComponent->IsReloading() || !AmmunitionComponent->BeginReload())
    {
        ClientReloadRejected();
        return;
    }

    const bool bEmptyReload = AmmunitionComponent->IsEmpty();
    const float Duration = bEmptyReload ? WeaponData->Balance.EmptyReloadDuration : WeaponData->Balance.ReloadDuration;
    BeginReloadPresentation(bEmptyReload);
    GetWorld()->GetTimerManager().SetTimer(
        ReloadTimerHandle,
        FTimerDelegate::CreateUObject(this, &ABMWeapon::FinishReload),
        Duration,
        false);
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_ReloadProbe")))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat probe: reload server timer started owner=%s duration=%.2f empty=%s magazine=%d reserve=%d"),
            *GetNameSafe(GetOwner()),
            Duration,
            bEmptyReload ? TEXT("yes") : TEXT("no"),
            GetMagazineAmmo(),
            GetReserveAmmo());
    }
#endif
    MulticastWeaponEvent(TEXT("ReloadStarted"));
}

#if !UE_BUILD_SHIPPING
bool ABMWeapon::SetCombatProbeAmmo(const int32 InMagazineAmmo, const int32 InReserveAmmo)
{
    if (!HasAuthority() || !AmmunitionComponent || !WeaponData)
    {
        return false;
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ReloadTimerHandle);
    }

    // Ammo alone is not a deterministic reset: fire-rate gating, accuracy,
    // pending recoil, and predicted-presentation cooldowns all survive a
    // magazine edit unless they are explicitly cleared as well.
    if (FireModeComponent)
    {
        FireModeComponent->ResetCombatState();
    }
    if (RecoilComponent)
    {
        RecoilComponent->ResetCombatState();
    }
    LastPredictedFireTime = -BIG_NUMBER;
    LastDryFireFeedbackTime = -BIG_NUMBER;
    PresentationShotPulse = 0.0f;
    PrewarmPresentationAssets();

    const int32 MagazineCapacity = FMath::Max(0, WeaponData->Balance.MagazineSize);
    AmmunitionComponent->bReloading = false;
    AmmunitionComponent->MagazineAmmo = FMath::Clamp(InMagazineAmmo, 0, MagazineCapacity);
    AmmunitionComponent->ReserveAmmo = FMath::Max(0, InReserveAmmo);
    AmmunitionComponent->OnAmmunitionChanged.Broadcast();
    ForceNetUpdate();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Black Meridian combat probe: ammo prepared owner=%s magazine=%d reserve=%d capacity=%d"),
        *GetNameSafe(GetOwner()),
        GetMagazineAmmo(),
        GetReserveAmmo(),
        MagazineCapacity);
    return true;
}
#endif

void ABMWeapon::CancelReload()
{
    if (!AmmunitionComponent->IsReloading())
    {
        return;
    }
    if (HasAuthority())
    {
        GetWorld()->GetTimerManager().ClearTimer(ReloadTimerHandle);
        AmmunitionComponent->CancelReload();
#if !UE_BUILD_SHIPPING
        if (FParse::Param(FCommandLine::Get(), TEXT("BM_ReloadProbe")))
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian combat probe: reload server timer cancelled owner=%s magazine=%d reserve=%d"),
                *GetNameSafe(GetOwner()),
                GetMagazineAmmo(),
                GetReserveAmmo());
        }
#endif
        MulticastWeaponEvent(TEXT("ReloadCancelled"));
    }
    else
    {
        AmmunitionComponent->CancelReload();
        ReloadPresentationElapsed = ReloadPresentationDuration;
        ServerCancelReload();
    }
}

void ABMWeapon::FinishReload()
{
    if (!HasAuthority() || !AmmunitionComponent || !AmmunitionComponent->IsReloading())
    {
        return;
    }
    ReloadPresentationElapsed = ReloadPresentationDuration;
    AmmunitionComponent->CompleteReload();
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_ReloadProbe")))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat probe: reload server timer completed owner=%s magazine=%d reserve=%d"),
            *GetNameSafe(GetOwner()),
            GetMagazineAmmo(),
            GetReserveAmmo());
    }
#endif
    MulticastWeaponEvent(TEXT("ReloadCompleted"));
}

void ABMWeapon::ClientReloadRejected_Implementation()
{
    if (!AmmunitionComponent || !AmmunitionComponent->IsReloading())
    {
        return;
    }

    AmmunitionComponent->CancelReload();
    ReloadPresentationElapsed = ReloadPresentationDuration;
    PlayWeaponAnimation_Implementation(TEXT("ReloadCancelled"));
    PlayWeaponAudio_Implementation(TEXT("ReloadCancelled"), IsSuppressed());
}

void ABMWeapon::ServerCancelReload_Implementation()
{
    CancelReload();
}

void ABMWeapon::MulticastWeaponEvent_Implementation(const FName EventName)
{
    PlayWeaponAnimation_Implementation(EventName);
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_ReloadProbe"))
        && (EventName == TEXT("ReloadStarted") || EventName == TEXT("ReloadCompleted") || EventName == TEXT("ReloadCancelled")))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat probe: reload presentation event=%s owner=%s authority=%s magazine=%d reserve=%d reloading=%s"),
            *EventName.ToString(),
            *GetNameSafe(GetOwner()),
            HasAuthority() ? TEXT("yes") : TEXT("no"),
            GetMagazineAmmo(),
            GetReserveAmmo(),
            IsReloading() ? TEXT("yes") : TEXT("no"));
    }
#endif
    if (EventName == TEXT("ReloadStarted") || EventName == TEXT("ReloadCompleted") || EventName == TEXT("ReloadCancelled") || EventName == TEXT("DryFire"))
    {
        PlayWeaponAudio_Implementation(EventName, IsSuppressed());
    }
}

bool ABMWeapon::EquipAttachment(UBMWeaponAttachmentDataAsset* Attachment)
{
    if (!Attachment || !WeaponData)
    {
        return false;
    }

    if (!HasAuthority())
    {
        if (!Attachment->AttachmentId.IsNone())
        {
            ServerEquipAttachmentById(Attachment->AttachmentId);
        }
        return false;
    }

    // "Barrel" was used by an early data-asset draft. Treat it as the
    // canonical muzzle slot so old authored profiles remain usable.
    const FName CanonicalSlot = Attachment->Slot == TEXT("Barrel")
        ? FName(TEXT("Muzzle"))
        : Attachment->Slot;
    const bool bSupported = WeaponData->SupportedAttachmentSlots.Contains(CanonicalSlot)
        || CanonicalSlot == TEXT("Muzzle");
    if (!bSupported)
    {
        return false;
    }

    if (Attachment->AttachmentId.IsNone())
    {
        Attachment->AttachmentId = Attachment->GetFName();
    }
    Attachment->Slot = CanonicalSlot;
    EquippedAttachments.Add(CanonicalSlot, Attachment);
    RebuildReplicatedAttachmentState();
    bReplicatedSuppressed = WeaponData->bSuppressed;
    for (const FBMWeaponAttachmentState& State : ReplicatedAttachmentState)
    {
        bReplicatedSuppressed = bReplicatedSuppressed || State.bSuppressor;
    }
    ApplyPresentationMaterials();
    ForceNetUpdate();

#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_CombatProbe"))
        || FParse::Param(FCommandLine::Get(), TEXT("BM_AttachmentProbe")))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian attachment: equipped id=%s slot=%s weapon=%s damage=%.2f ads_fov=%.1f recoil=%.2f suppressor=%s"),
            *Attachment->AttachmentId.ToString(),
            *CanonicalSlot.ToString(),
            *GetNameSafe(this),
            Attachment->DamageMultiplier,
            Attachment->ADSFOVOffset,
            Attachment->RecoilMultiplier,
            Attachment->bSuppressor ? TEXT("yes") : TEXT("no"));
    }
#endif
    return true;
}

bool ABMWeapon::EquipAttachmentById(const FName AttachmentId)
{
    if (AttachmentId.IsNone())
    {
        return false;
    }
    if (!HasAuthority())
    {
        ServerEquipAttachmentById(AttachmentId);
        return false;
    }

    UBMWeaponAttachmentDataAsset* Attachment = UBMWeaponAttachmentDataAsset::CreateRuntimeProfile(this, AttachmentId);
    return EquipAttachment(Attachment);
}

bool ABMWeapon::ClearAttachment(const FName Slot)
{
    if (!HasAuthority())
    {
        return false;
    }

    const FName CanonicalSlot = Slot == TEXT("Barrel") ? FName(TEXT("Muzzle")) : Slot;
    const int32 Removed = EquippedAttachments.Remove(CanonicalSlot);
    if (Removed <= 0)
    {
        return false;
    }

    RebuildReplicatedAttachmentState();
    bReplicatedSuppressed = WeaponData && WeaponData->bSuppressed;
    ApplyPresentationMaterials();
    ForceNetUpdate();
    return true;
}

float ABMWeapon::GetAttachmentDamageMultiplier() const
{
    float Multiplier = 1.0f;
    for (const FBMWeaponAttachmentState& State : ReplicatedAttachmentState)
    {
        Multiplier *= FMath::Clamp(State.DamageMultiplier, 0.25f, 4.0f);
    }
    return FMath::Clamp(Multiplier, 0.25f, 4.0f);
}

float ABMWeapon::GetAttachmentADSFOVOffset() const
{
    float Offset = 0.0f;
    for (const FBMWeaponAttachmentState& State : ReplicatedAttachmentState)
    {
        Offset += FMath::Clamp(State.ADSFOVOffset, -20.0f, 20.0f);
    }
    return FMath::Clamp(Offset, -24.0f, 24.0f);
}

float ABMWeapon::GetAttachmentRecoilMultiplier() const
{
    float Multiplier = 1.0f;
    for (const FBMWeaponAttachmentState& State : ReplicatedAttachmentState)
    {
        Multiplier *= FMath::Clamp(State.RecoilMultiplier, 0.25f, 3.0f);
    }
    return FMath::Clamp(Multiplier, 0.25f, 3.0f);
}

FName ABMWeapon::GetEquippedAttachmentId(const FName Slot) const
{
    const FName CanonicalSlot = Slot == TEXT("Barrel") ? FName(TEXT("Muzzle")) : Slot;
    for (const FBMWeaponAttachmentState& State : ReplicatedAttachmentState)
    {
        if (State.Slot == CanonicalSlot)
        {
            return State.AttachmentId;
        }
    }
    return NAME_None;
}

TArray<FName> ABMWeapon::GetEquippedAttachmentIds() const
{
    TArray<FName> Result;
    Result.Reserve(ReplicatedAttachmentState.Num());
    for (const FBMWeaponAttachmentState& State : ReplicatedAttachmentState)
    {
        if (!State.AttachmentId.IsNone())
        {
            Result.Add(State.AttachmentId);
        }
    }
    return Result;
}

void ABMWeapon::ServerEquipAttachmentById_Implementation(const FName AttachmentId)
{
    EquipAttachmentById(AttachmentId);
}

void ABMWeapon::RebuildReplicatedAttachmentState()
{
    ReplicatedAttachmentState.Reset();

    TArray<FName> Slots;
    EquippedAttachments.GetKeys(Slots);
    Slots.Sort([](const FName& Left, const FName& Right)
    {
        return Left.ToString() < Right.ToString();
    });
    for (const FName Slot : Slots)
    {
        const UBMWeaponAttachmentDataAsset* Attachment = EquippedAttachments.FindRef(Slot);
        if (!Attachment)
        {
            continue;
        }

        FBMWeaponAttachmentState& State = ReplicatedAttachmentState.AddDefaulted_GetRef();
        State.Slot = Slot;
        State.AttachmentId = Attachment->AttachmentId.IsNone() ? Attachment->GetFName() : Attachment->AttachmentId;
        State.DamageMultiplier = Attachment->DamageMultiplier;
        State.ADSFOVOffset = Attachment->ADSFOVOffset;
        State.RecoilMultiplier = Attachment->RecoilMultiplier;
        State.bSuppressor = Attachment->bSuppressor;
        State.MeshPath = Attachment->Mesh.IsNull()
            ? FString()
            : Attachment->Mesh.ToSoftObjectPath().ToString();
    }
}

void ABMWeapon::RebuildAttachmentMapFromReplication()
{
    EquippedAttachments.Reset();
    for (const FBMWeaponAttachmentState& State : ReplicatedAttachmentState)
    {
        if (State.Slot.IsNone())
        {
            continue;
        }

        UBMWeaponAttachmentDataAsset* Attachment = NewObject<UBMWeaponAttachmentDataAsset>(this);
        Attachment->AttachmentId = State.AttachmentId;
        Attachment->Slot = State.Slot;
        Attachment->DamageMultiplier = State.DamageMultiplier;
        Attachment->ADSFOVOffset = State.ADSFOVOffset;
        Attachment->RecoilMultiplier = State.RecoilMultiplier;
        Attachment->bSuppressor = State.bSuppressor;
        if (!State.MeshPath.IsEmpty())
        {
            Attachment->Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(State.MeshPath));
        }
        EquippedAttachments.Add(State.Slot, Attachment);
    }
}

void ABMWeapon::OnRep_AttachmentState()
{
    RebuildAttachmentMapFromReplication();
    if (WeaponData)
    {
        bReplicatedSuppressed = WeaponData->bSuppressed;
        for (const FBMWeaponAttachmentState& State : ReplicatedAttachmentState)
        {
            bReplicatedSuppressed = bReplicatedSuppressed || State.bSuppressor;
        }
    }
    ApplyPresentationMaterials();
}

void ABMWeapon::MulticastFireEffects_Implementation(
    const FVector_NetQuantize Start,
    const FVector_NetQuantize End,
    const bool bHit,
    const bool bHeadshot,
    const bool bSurfaceImpact,
    const bool bSuppressed,
    const EBMSurfaceType ImpactSurface)
{
    const bool bLocallyPredicted = !HasAuthority() && WasFireRecentlyPredicted();
    // The pulse is consumed by the local presentation tick. It is deliberately
    // separate from authoritative damage so the fallback rig can show a sharp
    // bolt action without changing hit validation or replication semantics.
    PresentationShotPulse = 1.0f;
    ApplyFallbackFirePresentation(Start, End, bHit, bHeadshot, bSurfaceImpact, bSuppressed, ImpactSurface);

    if (!HasAuthority() && !bLocallyPredicted && RecoilComponent && WeaponData)
    {
        RecoilComponent->AddImpulse(
            WeaponData->Balance.RecoilPitch * GetAttachmentRecoilMultiplier(),
            FMath::FRandRange(-WeaponData->Balance.RecoilYaw, WeaponData->Balance.RecoilYaw) * GetAttachmentRecoilMultiplier());
        if (FireModeComponent && GetWorld())
        {
            FireModeComponent->RegisterShot(
                GetWorld()->GetTimeSeconds(),
                WeaponData->Balance.ShotSpreadPenaltyDegrees,
                WeaponData->Balance.AccuracyRecoveryDegreesPerSecond,
                WeaponData->Balance.MaxAccuracyPenaltyDegrees);
        }
    }
    if (GetNetMode() != NM_DedicatedServer && GetWorld())
    {
        if (FireLight)
        {
            FireLight->SetWorldLocation(Start);
            FireLight->SetLightColor(bSuppressed ? FLinearColor(0.25f, 0.65f, 1.0f) : FLinearColor(1.0f, 0.55f, 0.16f));
            FireLight->SetIntensity(bSuppressed ? 900.0f : 2600.0f);
            FireLight->SetVisibility(true);
            GetWorld()->GetTimerManager().ClearTimer(FireLightTimerHandle);
            GetWorld()->GetTimerManager().SetTimer(FireLightTimerHandle, this, &ABMWeapon::DisableFireLight, 0.045f, false);
        }

        UBMAudioDataAsset* AudioProfile = WeaponData ? WeaponData->AudioProfile.LoadSynchronous() : nullptr;
        if (!bLocallyPredicted)
        {
            PlayFireSoundAtLocation(Start, bSuppressed);
        }

        bool bImpactSoundPlayed = false;
        if (bSurfaceImpact && AudioProfile)
        {
            if (const TSoftObjectPtr<USoundBase>* SurfaceImpact = AudioProfile->SurfaceImpacts.Find(ImpactSurface))
            {
                if (USoundBase* ImpactSound = SurfaceImpact->LoadSynchronous())
                {
                    UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, End);
                    bImpactSoundPlayed = true;
                }
            }
            if (!bImpactSoundPlayed)
            {
                if (USoundBase* ImpactSound = AudioProfile->BulletImpact.LoadSynchronous())
                {
                    UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, End);
                    bImpactSoundPlayed = true;
                }
            }
        }
        if (bSurfaceImpact && !bImpactSoundPlayed)
        {
            const bool bMetallicSurface = ImpactSurface == EBMSurfaceType::Metal || ImpactSurface == EBMSurfaceType::Glass;
            if (USoundBase* GeneratedImpact = LoadGeneratedCombatCue(bMetallicSurface ? TEXT("BM_Impact_Metal") : TEXT("BM_Impact_Concrete")))
            {
                UGameplayStatics::PlaySoundAtLocation(
                    this,
                    GeneratedImpact,
                    End,
                    FRotator::ZeroRotator,
                    bHit ? 0.56f : 0.34f,
                    bMetallicSurface ? 1.12f : 0.90f);
                bImpactSoundPlayed = true;
            }
            else
            {
                PlayRuntimePulse(
                    this,
                    End,
                    ERuntimePulse::Impact,
                    bHit ? 0.48f : 0.26f,
                    bMetallicSurface ? 1.2f : 0.82f);
            }
        }

        if (WeaponData)
        {
            if (UBMVFXDataAsset* VFXProfile = WeaponData->VFXProfile.LoadSynchronous())
            {
                const bool bVFXProbe = FParse::Param(FCommandLine::Get(), TEXT("BM_VFXProbe"));
                if (UNiagaraSystem* MuzzleFlash = VFXProfile->MuzzleFlash.LoadSynchronous();
                    ShouldSpawnConfiguredNiagara(MuzzleFlash))
                {
                    UNiagaraComponent* MuzzleComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                        this,
                        MuzzleFlash,
                        Start,
                        GetActorRotation());
                    if (MuzzleComponent)
                    {
                        // DirectionalBurst is intentionally duplicated from
                        // the engine template for portability, but its
                        // default world scale is enormous in a cooked game.
                        // Five simultaneous emitters used to turn one burst
                        // into white screen-covering spikes. Keep the
                        // authored system as a restrained accent; the
                        // project-owned fallback muzzle mesh remains the
                        // primary readable cue at weapon scale.
                        MuzzleComponent->SetWorldScale3D(FVector(0.035f));
                    }
                    const bool bSpawned = MuzzleComponent != nullptr;
                    if (bVFXProbe)
                    {
                        UE_LOG(
                            LogTemp,
                            Display,
                            TEXT("Black Meridian VFX: event=muzzle_flash system=%s spawned=%s"),
                            *MuzzleFlash->GetPathName(),
                            bSpawned ? TEXT("yes") : TEXT("no"));
                    }
                }

                if (UNiagaraSystem* ShellEjection = VFXProfile->ShellEjection.LoadSynchronous();
                    ShouldSpawnConfiguredNiagara(ShellEjection) && WeaponMesh)
                {
                    UNiagaraComponent* ShellComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
                        ShellEjection,
                        WeaponMesh,
                        TEXT("ShellEject"),
                        FVector::ZeroVector,
                        FRotator::ZeroRotator,
                        EAttachLocation::KeepRelativeOffset,
                        true);
                    if (ShellComponent)
                    {
                        ShellComponent->SetRelativeScale3D(FVector(0.035f));
                    }
                    const bool bSpawned = ShellComponent != nullptr;
                    if (bVFXProbe)
                    {
                        UE_LOG(
                            LogTemp,
                            Display,
                            TEXT("Black Meridian VFX: event=shell_ejection system=%s spawned=%s"),
                            *ShellEjection->GetPathName(),
                            bSpawned ? TEXT("yes") : TEXT("no"));
                    }
                }

                if (bSurfaceImpact)
                {
                    if (UNiagaraSystem* Impact = VFXProfile->BulletImpact.LoadSynchronous();
                        ShouldSpawnConfiguredNiagara(Impact))
                    {
                        UNiagaraComponent* ImpactComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                            this,
                            Impact,
                            End,
                            (End - Start).Rotation());
                        if (ImpactComponent)
                        {
                            ImpactComponent->SetWorldScale3D(FVector(0.035f));
                        }
                        const bool bSpawned = ImpactComponent != nullptr;
                        if (bVFXProbe)
                        {
                            UE_LOG(
                                LogTemp,
                                Display,
                                TEXT("Black Meridian VFX: event=bullet_impact system=%s spawned=%s"),
                                *Impact->GetPathName(),
                                bSpawned ? TEXT("yes") : TEXT("no"));
                        }
                    }

                        UNiagaraSystem* SecondaryImpact = nullptr;
                        if (ImpactSurface == EBMSurfaceType::Metal || ImpactSurface == EBMSurfaceType::Glass)
                        {
                            SecondaryImpact = VFXProfile->Sparks.LoadSynchronous();
                        }
                        else if (ImpactSurface == EBMSurfaceType::SoftCover)
                        {
                            SecondaryImpact = VFXProfile->Dust.LoadSynchronous();
                        }
                        else
                        {
                            SecondaryImpact = VFXProfile->Smoke.LoadSynchronous();
                        }
                        if (ShouldSpawnConfiguredNiagara(SecondaryImpact))
                        {
                            UNiagaraComponent* SecondaryComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                                this,
                                SecondaryImpact,
                                End,
                                (End - Start).Rotation());
                            if (SecondaryComponent)
                            {
                                SecondaryComponent->SetWorldScale3D(FVector(0.035f));
                            }
                            const bool bSpawned = SecondaryComponent != nullptr;
                            if (bVFXProbe)
                            {
                                UE_LOG(
                                    LogTemp,
                                    Display,
                                    TEXT("Black Meridian VFX: event=surface_secondary system=%s spawned=%s"),
                                    *SecondaryImpact->GetPathName(),
                                    bSpawned ? TEXT("yes") : TEXT("no"));
                            }
                        }

                        const UBMGameUserSettings* Settings = GEngine ? Cast<UBMGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
                        if (!Settings || !Settings->bGoreReduction)
                        {
                            if (UNiagaraSystem* BloodImpact = VFXProfile->BloodImpact.LoadSynchronous();
                                ShouldSpawnConfiguredNiagara(BloodImpact))
                            {
                                UNiagaraComponent* BloodComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                                    this,
                                    BloodImpact,
                                    End,
                                    (End - Start).Rotation());
                                if (BloodComponent)
                                {
                                    BloodComponent->SetWorldScale3D(FVector(0.030f));
                                }
                                const bool bSpawned = BloodComponent != nullptr;
                                if (bVFXProbe)
                                {
                                    UE_LOG(
                                        LogTemp,
                                        Display,
                                        TEXT("Black Meridian VFX: event=blood_impact system=%s spawned=%s"),
                                        *BloodImpact->GetPathName(),
                                        bSpawned ? TEXT("yes") : TEXT("no"));
                                }
                            }
                        }
                        if (UMaterialInterface* ImpactDecal = VFXProfile->ImpactDecal.LoadSynchronous())
                        {
                            UGameplayStatics::SpawnDecalAtLocation(
                                this,
                                ImpactDecal,
                                FVector(6.0f),
                                End,
                                (End - Start).Rotation(),
                                8.0f);
                        }
                    }
                }
            }
    }
#if !UE_BUILD_SHIPPING
    // Trace lines are useful for an explicit weapon-debug capture, but they
    // make ordinary development play look like an editor diagnostic overlay.
    // Keep the opt-in path available without exposing it to players.
    if (GetWorld() && FParse::Param(FCommandLine::Get(), TEXT("BM_DebugTraces")))
    {
        DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Orange : FColor::White, false, 0.08f, 0, 0.7f);
    }
#endif
    if (!HasAuthority())
    {
        OnWeaponEvent.Broadcast(bHeadshot ? TEXT("HeadshotHit") : (bHit ? TEXT("Hit") : TEXT("Fire")));
    }
}

void ABMWeapon::OnRep_WeaponId()
{
    if (!WeaponData || WeaponData->WeaponId != ReplicatedWeaponId)
    {
        WeaponData = UBMWeaponDataAsset::LoadContentProfile(this, ReplicatedWeaponId);
    }
    if (AmmunitionComponent && WeaponData)
    {
        AmmunitionComponent->ConfigureMagazineCapacity(WeaponData->Balance.MagazineSize);
    }
    ApplyFallbackWeaponVariant();
    ApplyPresentationMaterials();
}

void ABMWeapon::PlayWeaponAnimation_Implementation(const FName AnimationEvent)
{
    OnWeaponEvent.Broadcast(AnimationEvent);
}

void ABMWeapon::PlayWeaponAudio_Implementation(const FName AudioEvent, const bool bSuppressed)
{
    if (GetNetMode() != NM_DedicatedServer && GetWorld())
    {
        const bool bReloadEvent = AudioEvent == TEXT("ReloadStarted") || AudioEvent == TEXT("ReloadCompleted") || AudioEvent == TEXT("ReloadCancelled");
        const bool bDryFireEvent = AudioEvent == TEXT("DryFire");
        UBMAudioDataAsset* AudioProfile = WeaponData ? WeaponData->AudioProfile.LoadSynchronous() : nullptr;
        USoundBase* Sound = nullptr;
        if (AudioProfile)
        {
            if (bReloadEvent)
            {
                Sound = AudioProfile->Reload.LoadSynchronous();
            }
            else if (bDryFireEvent)
            {
                Sound = AudioProfile->DryFire.LoadSynchronous();
            }
            else
            {
                Sound = bSuppressed ? AudioProfile->SuppressedGunshot.LoadSynchronous() : AudioProfile->OutdoorGunshot.LoadSynchronous();
            }
        }
        if (!Sound && WeaponData && !bReloadEvent && !bDryFireEvent)
        {
            Sound = bSuppressed ? WeaponData->SuppressedFireSound.LoadSynchronous() : WeaponData->FireSound.LoadSynchronous();
        }
        if (!Sound)
        {
            if (bReloadEvent)
            {
                Sound = LoadGeneratedCombatCue(TEXT("BM_Reload"));
            }
            else if (bDryFireEvent)
            {
                Sound = LoadGeneratedCombatCue(TEXT("BM_DryFire"));
            }
            else
            {
                Sound = LoadGeneratedCombatCue(bSuppressed ? TEXT("BM_Suppressed_Fire") : TEXT("BM_ARX4_Fire"));
            }
        }
        if (Sound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
        }
        else if (bReloadEvent)
        {
            PlayRuntimePulse(this, GetActorLocation(), ERuntimePulse::Reload, 0.34f, AudioEvent == TEXT("ReloadCompleted") ? 1.08f : 0.82f);
        }
        else if (bDryFireEvent)
        {
            PlayRuntimePulse(this, GetActorLocation(), ERuntimePulse::DryFire, 0.42f, 0.92f);
        }
    }
    // Animation and audio deliberately share the same semantic event names,
    // but they have separate delivery paths. MulticastWeaponEvent already
    // broadcasts the event through PlayWeaponAnimation_Implementation before
    // calling this function. Rebroadcasting it here made reload and dry-fire
    // callbacks run twice, restarting the first-person sequence and doubling
    // HUD feedback. Audio must remain presentation-only.
}

void ABMWeapon::PlayFireSoundAtLocation(const FVector& Location, const bool bSuppressed)
{
    if (GetNetMode() == NM_DedicatedServer || !GetWorld() || FParse::Param(FCommandLine::Get(), TEXT("NoSound")))
    {
        return;
    }

    UBMAudioDataAsset* AudioProfile = WeaponData ? WeaponData->AudioProfile.LoadSynchronous() : nullptr;
    FCollisionQueryParams CeilingQuery(SCENE_QUERY_STAT(BlackMeridianIndoorAudio), true, this);
    const bool bIndoors = GetWorld()->LineTraceTestByChannel(
        Location,
        Location + FVector(0.0f, 0.0f, 1200.0f),
        ECC_Visibility,
        CeilingQuery);

    USoundBase* FireSound = nullptr;
    if (AudioProfile)
    {
        if (bSuppressed)
        {
            FireSound = AudioProfile->SuppressedGunshot.LoadSynchronous();
        }
        if (!FireSound)
        {
            FireSound = (bIndoors ? AudioProfile->IndoorGunshot : AudioProfile->OutdoorGunshot).LoadSynchronous();
        }
        if (!FireSound)
        {
            FireSound = AudioProfile->OutdoorGunshot.LoadSynchronous();
        }
    }
    if (!FireSound && WeaponData)
    {
        FireSound = (bSuppressed ? WeaponData->SuppressedFireSound : WeaponData->FireSound).LoadSynchronous();
        if (!FireSound && bSuppressed)
        {
            FireSound = WeaponData->FireSound.LoadSynchronous();
        }
    }

        if (FireSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, FireSound, Location);
        }
        else
        {
            FireSound = LoadGeneratedCombatCue(bSuppressed ? TEXT("BM_Suppressed_Fire") : TEXT("BM_ARX4_Fire"));
            if (FireSound)
            {
                UGameplayStatics::PlaySoundAtLocation(this, FireSound, Location, FRotator::ZeroRotator, bSuppressed ? 0.62f : 0.86f, bSuppressed ? 1.10f : 0.96f);
            }
            else
            {
                PlayRuntimePulse(this, Location, ERuntimePulse::Shot, bSuppressed ? 0.28f : 0.72f, bSuppressed ? 1.25f : 0.95f);
            }
        }
}

int32 ABMWeapon::GetMagazineAmmo() const
{
    return AmmunitionComponent ? AmmunitionComponent->GetMagazineAmmo() : 0;
}

int32 ABMWeapon::GetReserveAmmo() const
{
    return AmmunitionComponent ? AmmunitionComponent->GetReserveAmmo() : 0;
}

bool ABMWeapon::IsReloading() const
{
    return AmmunitionComponent && AmmunitionComponent->IsReloading();
}

float ABMWeapon::GetReloadProgress() const
{
    if (!IsReloading() || ReloadPresentationDuration <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }
    return FMath::Clamp(ReloadPresentationElapsed / ReloadPresentationDuration, 0.0f, 1.0f);
}

bool ABMWeapon::WasFireRecentlyPredicted() const
{
    return GetWorld() && (GetWorld()->GetTimeSeconds() - LastPredictedFireTime) <= 0.22f;
}

bool ABMWeapon::IsSuppressed() const
{
    if (bReplicatedSuppressed || (WeaponData && WeaponData->bSuppressed))
    {
        return true;
    }

    const UBMWeaponAttachmentDataAsset* MuzzleAttachment = EquippedAttachments.FindRef(TEXT("Muzzle"));
    return MuzzleAttachment && MuzzleAttachment->bSuppressor;
}

void ABMWeapon::AddReserveAmmo(const int32 Amount)
{
    if (HasAuthority() && AmmunitionComponent)
    {
        AmmunitionComponent->AddReserveAmmo(Amount);
    }
}

void ABMWeapon::DisableFireLight()
{
    if (FireLight)
    {
        FireLight->SetIntensity(0.0f);
        FireLight->SetVisibility(false);
    }
}

void ABMWeapon::DisableMuzzleFlash()
{
    if (MuzzleFlashMesh)
    {
        MuzzleFlashMesh->SetVisibility(false);
    }
}

void ABMWeapon::DisableTracer()
{
    if (TracerMesh)
    {
        TracerMesh->SetVisibility(false);
    }
}

void ABMWeapon::DisableImpactFlash()
{
    if (ImpactFlashMesh)
    {
        ImpactFlashMesh->SetVisibility(false);
    }
    if (ImpactShardMeshA)
    {
        ImpactShardMeshA->SetVisibility(false);
    }
    if (ImpactShardMeshB)
    {
        ImpactShardMeshB->SetVisibility(false);
    }
    if (ImpactLight)
    {
        ImpactLight->SetIntensity(0.0f);
        ImpactLight->SetVisibility(false);
    }
}

void ABMWeapon::ApplyFallbackFirePresentation(
    const FVector& Start,
    const FVector& End,
    const bool bHit,
    const bool bHeadshot,
    const bool bSurfaceImpact,
    const bool bSuppressed,
    const EBMSurfaceType ImpactSurface)
{
    if (GetNetMode() == NM_DedicatedServer || !GetWorld())
    {
        return;
    }

    const FVector ShotVector = End - Start;
    const float ShotDistance = FMath::Max(1.0f, ShotVector.Size());
    const FVector ShotDirection = ShotVector / ShotDistance;

    // World-space fallback FX are also used by remote weapons.  When an
    // enemy's impact lands on the local pawn, the impact sphere/light can be
    // only a few centimetres from the first-person camera and become a
    // screen-sized white/yellow card.  Keep the feedback, but attenuate the
    // near-camera version so the weapon, hands, and target remain readable.
    const UCameraComponent* LocalCamera = nullptr;
    if (const ABMPlayerCharacter* LocalPlayer = Cast<ABMPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
    {
        LocalCamera = LocalPlayer->GetFirstPersonCamera();
    }
    const FVector LocalViewLocation = LocalCamera
        ? LocalCamera->GetComponentLocation()
        : FVector::ZeroVector;
    const bool bNearLocalViewStart = LocalCamera
        && FVector::DistSquared(Start, LocalViewLocation) < FMath::Square(115.0f);
    const bool bNearLocalViewEnd = LocalCamera
        && FVector::DistSquared(End, LocalViewLocation) < FMath::Square(115.0f);
    const float NearViewMuzzleScale = bNearLocalViewStart ? 0.48f : 1.0f;
    const float NearViewImpactScale = bNearLocalViewEnd ? 0.42f : 1.0f;
    const float NearViewLightScale = bNearLocalViewEnd ? 0.28f : 1.0f;
    const bool bThirdPersonTracer = bThirdPersonPresentation;

    if (MuzzleFlashMesh)
    {
        MuzzleFlashMesh->SetWorldLocation(Start + ShotDirection * 10.0f);
        MuzzleFlashMesh->SetWorldRotation(ShotDirection.Rotation());
        MuzzleFlashMesh->SetWorldScale3D(bSuppressed
            ? FVector(0.055f, 0.018f, 0.018f) * NearViewMuzzleScale
            : FVector(0.11f, 0.032f, 0.032f) * NearViewMuzzleScale);
        if (MuzzleFlashMaterial)
        {
            MuzzleFlashMaterial->SetVectorParameterValue(
                TEXT("Color"),
                bSuppressed ? FLinearColor(0.06f, 0.34f, 0.72f) : FLinearColor(0.76f, 0.18f, 0.018f));
        }
        MuzzleFlashMesh->SetVisibility(true);
        GetWorld()->GetTimerManager().ClearTimer(MuzzleFlashTimerHandle);
        GetWorld()->GetTimerManager().SetTimer(
            MuzzleFlashTimerHandle,
            this,
            &ABMWeapon::DisableMuzzleFlash,
            bSuppressed ? 0.035f : 0.055f,
            false);
    }

    if (TracerMesh)
    {
        // A full-length cube from muzzle to hit point reads as a laser beam,
        // especially against the dark Metal capture.  Real firearm tracers
        // are short luminous segments moving through the bullet path.  Keep
        // the procedural fallback bounded and place it partway downrange so
        // it remains visible without erasing the target, cover, or muzzle
        // silhouette.  The segment is clamped to the actual shot distance
        // for close hits.
        // Enemy weapons are rendered in third person and can fire together
        // from several lanes. A long, bright segment from each weapon stacks
        // into a screen-covering orange grid in a dark arena. Keep hostile
        // tracers readable, but bias them toward a short downrange segment;
        // the local first-person weapon retains the stronger confirmation.
        const float TracerCenterDistance = FMath::Clamp(
            ShotDistance * (bThirdPersonTracer ? 0.22f : 0.42f),
            bThirdPersonTracer ? 72.0f : 32.0f,
            bThirdPersonTracer ? 220.0f : 520.0f);
        const float RequestedTracerLength = FMath::Clamp(
            ShotDistance * (bThirdPersonTracer ? 0.035f : 0.24f),
            bThirdPersonTracer ? 6.0f : 18.0f,
            bThirdPersonTracer ? 28.0f : 160.0f);
        const float TracerLength = FMath::Min(RequestedTracerLength, ShotDistance);
        const float HalfTracerLength = TracerLength * 0.5f;
        const float TracerStartDistance = FMath::Clamp(
            TracerCenterDistance - HalfTracerLength,
            0.0f,
            FMath::Max(0.0f, ShotDistance - TracerLength));
        const FVector TracerStart = Start + ShotDirection * TracerStartDistance;
        const FVector TracerCenter = TracerStart + ShotDirection * (TracerLength * 0.5f);
        TracerMesh->SetWorldLocation(TracerCenter);
        TracerMesh->SetWorldRotation(ShotDirection.Rotation());
        TracerMesh->SetWorldScale3D(FVector(
            TracerLength / 100.0f,
            bSuppressed
                ? (bThirdPersonTracer ? 0.00065f : 0.0025f)
                : (bThirdPersonTracer ? 0.00085f : 0.0032f),
            bSuppressed
                ? (bThirdPersonTracer ? 0.00065f : 0.0025f)
                : (bThirdPersonTracer ? 0.00085f : 0.0032f)));
        if (TracerMaterial)
        {
            TracerMaterial->SetVectorParameterValue(
                TEXT("Color"),
                bSuppressed
                    ? FLinearColor(0.05f, 0.22f, 0.70f)
                    : (bThirdPersonTracer
                        ? FLinearColor(0.34f, 0.055f, 0.008f)
                        : FLinearColor(0.72f, 0.22f, 0.025f)));
        }
        TracerMesh->SetVisibility(true);
        GetWorld()->GetTimerManager().ClearTimer(TracerTimerHandle);
        GetWorld()->GetTimerManager().SetTimer(
            TracerTimerHandle,
            this,
            &ABMWeapon::DisableTracer,
            bThirdPersonTracer ? 0.022f : 0.035f,
            false);
    }

    if (bSurfaceImpact && ImpactFlashMesh)
    {
        const FLinearColor ImpactColor = bHeadshot
            ? FLinearColor(0.10f, 0.60f, 0.72f)
            : (bHit
                ? FLinearColor(0.76f, 0.025f, 0.008f)
            : ((ImpactSurface == EBMSurfaceType::Metal || ImpactSurface == EBMSurfaceType::Glass)
                ? FLinearColor(0.68f, 0.40f, 0.06f)
                : FLinearColor(0.38f, 0.52f, 0.62f)));
        ImpactFlashMesh->SetWorldLocation(End);
        ImpactFlashMesh->SetWorldRotation((-ShotDirection).Rotation());
        // Keep the impact cue close to the hit surface.  The fallback sphere
        // uses an emissive material, so the previous 9.5 cm body-hit core and
        // 1700-unit light read as a large orange ball in the dark showcase
        // instead of a brief, directional hit confirmation.
        ImpactFlashMesh->SetWorldScale3D(
            (bHeadshot ? FVector(0.082f) : (bHit ? FVector(0.058f) : FVector(0.042f))) * NearViewImpactScale);
        if (ImpactFlashMaterial)
        {
            ImpactFlashMaterial->SetVectorParameterValue(TEXT("Color"), ImpactColor);
        }
        ImpactFlashMesh->SetVisibility(true);
        if (ImpactLight)
        {
            ImpactLight->SetWorldLocation(End);
            ImpactLight->SetLightColor(ImpactColor);
            ImpactLight->SetIntensity(
                (bHeadshot ? 420.0f : (bHit ? 260.0f : 120.0f)) * NearViewLightScale);
            ImpactLight->SetAttenuationRadius(bHeadshot ? 92.0f : (bHit ? 68.0f : 52.0f));
            ImpactLight->SetVisibility(true);
        }
        // Layer two short emissive shards around the core. The burst is
        // deliberately built from the same project-safe primitive kit as the
        // weapon, so it remains visible in a cooked package without relying
        // on an optional Niagara system. Surface colour still communicates
        // the material response: hot sparks for metal/glass, red confirmation
        // for a body hit, and cool dust-light for concrete/soft cover.
        const FVector ImpactNormal = (-ShotDirection).GetSafeNormal();
        FVector Tangent = FVector::CrossProduct(ImpactNormal, FVector::UpVector).GetSafeNormal();
        if (Tangent.IsNearlyZero())
        {
            Tangent = FVector::CrossProduct(ImpactNormal, FVector::RightVector).GetSafeNormal();
        }
        if (Tangent.IsNearlyZero())
        {
            Tangent = FVector::ForwardVector;
        }
        const FVector Bitangent = FVector::CrossProduct(ImpactNormal, Tangent).GetSafeNormal();
        const float BurstOffset = bHeadshot ? 5.0f : (bHit ? 3.5f : 2.5f);
        const float NormalOffset = 1.5f;
        if (ImpactShardMeshA)
        {
            ImpactShardMeshA->SetWorldLocation(End + ImpactNormal * NormalOffset + Tangent * BurstOffset);
            ImpactShardMeshA->SetWorldRotation((Tangent + ImpactNormal * 0.28f).Rotation());
            ImpactShardMeshA->SetWorldScale3D(
                (bHeadshot ? FVector(0.064f, 0.010f, 0.010f) : FVector(0.046f, 0.008f, 0.008f))
                * NearViewImpactScale);
            if (ImpactShardMaterialA)
            {
                ImpactShardMaterialA->SetVectorParameterValue(TEXT("Color"), ImpactColor * 0.88f);
                ImpactShardMaterialA->SetVectorParameterValue(TEXT("BaseColor"), ImpactColor * 0.88f);
            }
            ImpactShardMeshA->SetVisibility(true);
        }
        if (ImpactShardMeshB)
        {
            ImpactShardMeshB->SetWorldLocation(End + ImpactNormal * NormalOffset - Bitangent * BurstOffset * 0.8f);
            ImpactShardMeshB->SetWorldRotation((Bitangent + ImpactNormal * 0.22f).Rotation());
            ImpactShardMeshB->SetWorldScale3D(
                (bHeadshot ? FVector(0.050f, 0.008f, 0.008f) : FVector(0.036f, 0.006f, 0.006f))
                * NearViewImpactScale);
            if (ImpactShardMaterialB)
            {
                ImpactShardMaterialB->SetVectorParameterValue(TEXT("Color"), ImpactColor * 0.78f);
                ImpactShardMaterialB->SetVectorParameterValue(TEXT("BaseColor"), ImpactColor * 0.78f);
            }
            ImpactShardMeshB->SetVisibility(true);
        }
        GetWorld()->GetTimerManager().ClearTimer(ImpactFlashTimerHandle);
        GetWorld()->GetTimerManager().SetTimer(
            ImpactFlashTimerHandle,
            this,
            &ABMWeapon::DisableImpactFlash,
            bNearLocalViewEnd ? 0.055f : (bHeadshot ? 0.115f : 0.085f),
            false);
    }
    else
    {
        if (ImpactShardMeshA)
        {
            ImpactShardMeshA->SetVisibility(false);
        }
        if (ImpactShardMeshB)
        {
            ImpactShardMeshB->SetVisibility(false);
        }
    }
}

void ABMWeapon::ApplyFallbackWeaponVariant()
{
    if (!WeaponMesh || !BarrelMesh || !MagazineMesh || !GripMesh || !OpticMesh ||
        !StockMesh || !HandguardMesh || !MuzzleDeviceMesh || !RailMesh ||
        !FrontSightMesh || !TriggerGuardMesh || !ChargingHandleMesh)
    {
        return;
    }

    // Revert to the engine cube before applying a role-specific project mesh.
    // This keeps loadout swaps and first-person fallback presentation safe if
    // an actor changes between a project asset and the source-only kit.
    if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(
            nullptr,
            TEXT("/Engine/BasicShapes/Cube.Cube")))
    {
        WeaponMesh->SetStaticMesh(CubeMesh);
    }
    bUsingProjectWeaponMesh = false;

    // Reset the complete primitive kit before applying an identity-specific
    // layout. This also makes a replicated weapon-id change safe if a loadout
    // is rebuilt without spawning a new actor.
    WeaponMesh->SetRelativeLocation(FVector(30.0f, 8.0f, -13.0f));
    WeaponMesh->SetRelativeScale3D(FVector(0.23f, 0.075f, 0.06f));
    BarrelMesh->SetRelativeLocation(FVector(51.0f, 8.0f, -11.0f));
    BarrelMesh->SetRelativeScale3D(FVector(0.032f, 0.032f, 0.32f));
    BarrelMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
    MagazineMesh->SetRelativeLocation(FVector(27.0f, 8.0f, -24.0f));
    MagazineMesh->SetRelativeScale3D(FVector(0.055f, 0.045f, 0.13f));
    MagazineMesh->SetRelativeRotation(FRotator(-7.0f, 0.0f, 0.0f));
    GripMesh->SetRelativeLocation(FVector(18.0f, 8.0f, -22.0f));
    GripMesh->SetRelativeScale3D(FVector(0.06f, 0.045f, 0.14f));
    GripMesh->SetRelativeRotation(FRotator(-8.0f, 0.0f, 0.0f));
    OpticMesh->SetRelativeLocation(FVector(30.0f, 8.0f, -6.0f));
    OpticMesh->SetRelativeScale3D(FVector(0.075f, 0.032f, 0.035f));
    OpticMesh->SetRelativeRotation(FRotator::ZeroRotator);
    StockMesh->SetRelativeLocation(FVector(6.0f, 8.0f, -12.0f));
    StockMesh->SetRelativeScale3D(FVector(0.16f, 0.095f, 0.075f));
    StockMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, -2.0f));
    HandguardMesh->SetRelativeLocation(FVector(46.0f, 8.0f, -11.5f));
    HandguardMesh->SetRelativeScale3D(FVector(0.14f, 0.085f, 0.055f));
    HandguardMesh->SetRelativeRotation(FRotator::ZeroRotator);
    MuzzleDeviceMesh->SetRelativeLocation(FVector(69.0f, 8.0f, -11.0f));
    MuzzleDeviceMesh->SetRelativeScale3D(FVector(0.045f, 0.045f, 0.075f));
    MuzzleDeviceMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
    RailMesh->SetRelativeLocation(FVector(35.0f, 8.0f, -4.6f));
    RailMesh->SetRelativeScale3D(FVector(0.12f, 0.018f, 0.012f));
    RailMesh->SetRelativeRotation(FRotator::ZeroRotator);
    FrontSightMesh->SetRelativeLocation(FVector(58.0f, 8.0f, -4.0f));
    FrontSightMesh->SetRelativeScale3D(FVector(0.02f, 0.018f, 0.045f));
    FrontSightMesh->SetRelativeRotation(FRotator::ZeroRotator);
    TriggerGuardMesh->SetRelativeLocation(FVector(20.0f, 8.0f, -14.0f));
    TriggerGuardMesh->SetRelativeScale3D(FVector(0.06f, 0.018f, 0.015f));
    TriggerGuardMesh->SetRelativeRotation(FRotator::ZeroRotator);
    ChargingHandleMesh->SetRelativeLocation(FVector(39.0f, 8.0f, -3.7f));
    ChargingHandleMesh->SetRelativeScale3D(FVector(0.05f, 0.014f, 0.012f));
    ChargingHandleMesh->SetRelativeRotation(FRotator::ZeroRotator);

    const auto SetVisible = [](UStaticMeshComponent* Mesh, const bool bVisible)
    {
        if (Mesh)
        {
            Mesh->SetVisibility(bVisible, true);
        }
    };
    SetVisible(WeaponMesh, true);
    SetVisible(BarrelMesh, true);
    SetVisible(MagazineMesh, true);
    SetVisible(GripMesh, true);
    SetVisible(OpticMesh, true);
    SetVisible(StockMesh, true);
    SetVisible(HandguardMesh, true);
    SetVisible(MuzzleDeviceMesh, true);
    SetVisible(RailMesh, true);
    SetVisible(FrontSightMesh, true);
    SetVisible(TriggerGuardMesh, true);
    SetVisible(ChargingHandleMesh, true);
    if (FirstPersonGeometryMesh)
    {
        FirstPersonGeometryMesh->SetVisibility(false, true);
    }
    if (FirstPersonMagazineGeometryMesh)
    {
        FirstPersonMagazineGeometryMesh->SetVisibility(false, true);
    }

    switch (ReplicatedWeaponId)
    {
    case EBMWeaponId::Kestrel9:
        // Compact SMG: short gas system, tall magazine, compact optic.
        WeaponMesh->SetRelativeLocation(FVector(28.0f, 8.0f, -13.0f));
        WeaponMesh->SetRelativeScale3D(FVector(0.20f, 0.075f, 0.055f));
        BarrelMesh->SetRelativeLocation(FVector(48.0f, 8.0f, -11.0f));
        BarrelMesh->SetRelativeScale3D(FVector(0.027f, 0.027f, 0.22f));
        MuzzleDeviceMesh->SetRelativeLocation(FVector(60.0f, 8.0f, -11.0f));
        MuzzleDeviceMesh->SetRelativeScale3D(FVector(0.04f, 0.04f, 0.06f));
        HandguardMesh->SetRelativeLocation(FVector(42.0f, 8.0f, -11.5f));
        HandguardMesh->SetRelativeScale3D(FVector(0.11f, 0.08f, 0.05f));
        MagazineMesh->SetRelativeScale3D(FVector(0.05f, 0.045f, 0.16f));
        OpticMesh->SetRelativeScale3D(FVector(0.06f, 0.028f, 0.03f));
        FrontSightMesh->SetRelativeLocation(FVector(52.0f, 8.0f, -4.0f));
        break;

    case EBMWeaponId::RookDMR:
        // Long-range DMR: extended barrel, heavier stock, and high optic.
        WeaponMesh->SetRelativeLocation(FVector(32.0f, 8.0f, -12.0f));
        WeaponMesh->SetRelativeScale3D(FVector(0.27f, 0.08f, 0.065f));
        BarrelMesh->SetRelativeLocation(FVector(61.0f, 8.0f, -10.5f));
        BarrelMesh->SetRelativeScale3D(FVector(0.038f, 0.038f, 0.50f));
        MuzzleDeviceMesh->SetRelativeLocation(FVector(87.0f, 8.0f, -10.5f));
        MuzzleDeviceMesh->SetRelativeScale3D(FVector(0.05f, 0.05f, 0.08f));
        HandguardMesh->SetRelativeLocation(FVector(56.0f, 8.0f, -11.0f));
        HandguardMesh->SetRelativeScale3D(FVector(0.18f, 0.09f, 0.06f));
        StockMesh->SetRelativeScale3D(FVector(0.20f, 0.10f, 0.08f));
        MagazineMesh->SetRelativeScale3D(FVector(0.05f, 0.04f, 0.085f));
        OpticMesh->SetRelativeLocation(FVector(38.0f, 8.0f, -3.5f));
        OpticMesh->SetRelativeScale3D(FVector(0.105f, 0.04f, 0.05f));
        FrontSightMesh->SetRelativeLocation(FVector(75.0f, 8.0f, -3.5f));
        break;

    case EBMWeaponId::Breach12:
        // Shotgun: broad receiver and barrel, tubular magazine, no optic.
        WeaponMesh->SetRelativeLocation(FVector(34.0f, 8.0f, -12.0f));
        WeaponMesh->SetRelativeScale3D(FVector(0.25f, 0.11f, 0.09f));
        BarrelMesh->SetRelativeLocation(FVector(66.0f, 8.0f, -9.0f));
        BarrelMesh->SetRelativeScale3D(FVector(0.055f, 0.055f, 0.34f));
        MuzzleDeviceMesh->SetRelativeLocation(FVector(84.0f, 8.0f, -9.0f));
        MuzzleDeviceMesh->SetRelativeScale3D(FVector(0.07f, 0.07f, 0.08f));
        HandguardMesh->SetRelativeLocation(FVector(61.0f, 8.0f, -9.0f));
        HandguardMesh->SetRelativeScale3D(FVector(0.20f, 0.105f, 0.065f));
        MagazineMesh->SetRelativeLocation(FVector(55.0f, 8.0f, -19.0f));
        MagazineMesh->SetRelativeScale3D(FVector(0.035f, 0.035f, 0.22f));
        SetVisible(OpticMesh, false);
        SetVisible(RailMesh, false);
        SetVisible(FrontSightMesh, false);
        SetVisible(ChargingHandleMesh, false);
        break;

    case EBMWeaponId::SolacePistol:
        // Sidearm: compact slide and grip; remove rifle furniture.
        WeaponMesh->SetRelativeLocation(FVector(27.0f, 8.0f, -14.0f));
        WeaponMesh->SetRelativeScale3D(FVector(0.18f, 0.065f, 0.07f));
        BarrelMesh->SetRelativeLocation(FVector(44.0f, 8.0f, -13.0f));
        BarrelMesh->SetRelativeScale3D(FVector(0.035f, 0.035f, 0.14f));
        MuzzleDeviceMesh->SetRelativeLocation(FVector(52.0f, 8.0f, -13.0f));
        MuzzleDeviceMesh->SetRelativeScale3D(FVector(0.04f, 0.04f, 0.045f));
        MagazineMesh->SetRelativeLocation(FVector(24.0f, 8.0f, -25.0f));
        MagazineMesh->SetRelativeScale3D(FVector(0.045f, 0.04f, 0.095f));
        GripMesh->SetRelativeLocation(FVector(19.0f, 8.0f, -23.0f));
        GripMesh->SetRelativeScale3D(FVector(0.045f, 0.04f, 0.11f));
        OpticMesh->SetRelativeLocation(FVector(29.0f, 8.0f, -7.0f));
        OpticMesh->SetRelativeScale3D(FVector(0.045f, 0.025f, 0.025f));
        SetVisible(StockMesh, false);
        SetVisible(HandguardMesh, false);
        SetVisible(RailMesh, false);
        SetVisible(FrontSightMesh, false);
        SetVisible(ChargingHandleMesh, false);
        break;

    case EBMWeaponId::ArcLauncher:
        // Launcher: oversized tube and single-shot breech with a large optic.
        WeaponMesh->SetRelativeLocation(FVector(36.0f, 8.0f, -11.0f));
        WeaponMesh->SetRelativeScale3D(FVector(0.28f, 0.11f, 0.11f));
        BarrelMesh->SetRelativeLocation(FVector(70.0f, 8.0f, -8.0f));
        BarrelMesh->SetRelativeScale3D(FVector(0.09f, 0.09f, 0.36f));
        MuzzleDeviceMesh->SetRelativeLocation(FVector(91.0f, 8.0f, -8.0f));
        MuzzleDeviceMesh->SetRelativeScale3D(FVector(0.10f, 0.10f, 0.08f));
        HandguardMesh->SetRelativeLocation(FVector(66.0f, 8.0f, -8.0f));
        HandguardMesh->SetRelativeScale3D(FVector(0.20f, 0.11f, 0.09f));
        OpticMesh->SetRelativeLocation(FVector(42.0f, 8.0f, -1.5f));
        OpticMesh->SetRelativeScale3D(FVector(0.13f, 0.045f, 0.06f));
        SetVisible(MagazineMesh, false);
        SetVisible(TriggerGuardMesh, false);
        SetVisible(ChargingHandleMesh, false);
        break;

    case EBMWeaponId::ARX4:
        // Standard rifle: keep the first-person receiver slim and let the
        // angled magazine/grip, separate handguard, exposed barrel, and
        // charging hardware define the silhouette. These proportions are
        // deliberately camera-facing; third-person uses the authored mesh
        // and its socket contract below.
        WeaponMesh->SetRelativeLocation(FVector(30.0f, 8.0f, -12.0f));
        WeaponMesh->SetRelativeScale3D(FVector(0.22f, 0.065f, 0.052f));
        BarrelMesh->SetRelativeLocation(FVector(56.0f, 8.0f, -10.2f));
        BarrelMesh->SetRelativeScale3D(FVector(0.023f, 0.023f, 0.38f));
        MagazineMesh->SetRelativeLocation(FVector(27.0f, 8.0f, -22.5f));
        MagazineMesh->SetRelativeScale3D(FVector(0.042f, 0.034f, 0.115f));
        MagazineMesh->SetRelativeRotation(FRotator(-10.0f, 0.0f, 0.0f));
        GripMesh->SetRelativeLocation(FVector(18.5f, 8.0f, -20.5f));
        GripMesh->SetRelativeScale3D(FVector(0.047f, 0.036f, 0.12f));
        GripMesh->SetRelativeRotation(FRotator(-11.0f, 0.0f, 0.0f));
        OpticMesh->SetRelativeLocation(FVector(30.5f, 8.0f, -6.7f));
        OpticMesh->SetRelativeScale3D(FVector(0.058f, 0.025f, 0.026f));
        StockMesh->SetRelativeLocation(FVector(7.0f, 8.0f, -11.5f));
        StockMesh->SetRelativeScale3D(FVector(0.135f, 0.070f, 0.060f));
        StockMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, -2.0f));
        HandguardMesh->SetRelativeLocation(FVector(46.0f, 8.0f, -10.5f));
        HandguardMesh->SetRelativeScale3D(FVector(0.115f, 0.067f, 0.045f));
        MuzzleDeviceMesh->SetRelativeLocation(FVector(78.0f, 8.0f, -10.2f));
        MuzzleDeviceMesh->SetRelativeScale3D(FVector(0.038f, 0.038f, 0.060f));
        RailMesh->SetRelativeLocation(FVector(35.0f, 8.0f, -8.65f));
        RailMesh->SetRelativeScale3D(FVector(0.105f, 0.012f, 0.008f));
        FrontSightMesh->SetRelativeLocation(FVector(60.0f, 8.0f, -6.2f));
        FrontSightMesh->SetRelativeScale3D(FVector(0.014f, 0.013f, 0.028f));
        TriggerGuardMesh->SetRelativeLocation(FVector(20.0f, 8.0f, -13.0f));
        TriggerGuardMesh->SetRelativeScale3D(FVector(0.050f, 0.012f, 0.012f));
        ChargingHandleMesh->SetRelativeLocation(FVector(39.5f, 11.1f, -8.25f));
        ChargingHandleMesh->SetRelativeScale3D(FVector(0.042f, 0.010f, 0.010f));
        break;

    case EBMWeaponId::FragGrenade:
    case EBMWeaponId::VeilGrenade:
    default:
        break;
    }

    if (!bThirdPersonPresentation && ReplicatedWeaponId == EBMWeaponId::ARX4)
    {
        BuildFirstPersonARX4Geometry();
        BuildFirstPersonMagazineGeometry();
        SetVisible(WeaponMesh, false);
        SetVisible(BarrelMesh, false);
        SetVisible(MagazineMesh, false);
        SetVisible(GripMesh, false);
        SetVisible(OpticMesh, false);
        SetVisible(StockMesh, false);
        SetVisible(HandguardMesh, false);
        SetVisible(MuzzleDeviceMesh, false);
        SetVisible(RailMesh, false);
        SetVisible(FrontSightMesh, false);
        SetVisible(TriggerGuardMesh, false);
        SetVisible(ChargingHandleMesh, false);
        if (FirstPersonGeometryMesh)
        {
            FirstPersonGeometryMesh->SetVisibility(true, true);
        }
        if (FirstPersonMagazineGeometryMesh)
        {
            FirstPersonMagazineGeometryMesh->SetRelativeLocation(FVector(27.0f, 8.0f, -22.5f));
            FirstPersonMagazineGeometryMesh->SetRelativeRotation(FRotator(-10.0f, 0.0f, 0.0f));
            FirstPersonMagazineGeometryMesh->SetVisibility(true, true);
        }
    }

    if (bThirdPersonPresentation)
    {
        // Third-person weapons use a separate, hand-mounted profile. The
        // actor origin is the top of the pistol grip (the hand_r bone), so the
        // receiver, stock, magazine, barrel, and muzzle form one connected
        // silhouette instead of floating beside the mannequin. First-person
        // transforms above remain unchanged for the player's weapon.
        WeaponMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -8.0f));
        WeaponMesh->SetRelativeScale3D(FVector(0.26f, 0.085f, 0.07f));
        BarrelMesh->SetRelativeLocation(FVector(37.0f, 0.0f, -7.0f));
        BarrelMesh->SetRelativeScale3D(FVector(0.034f, 0.034f, 0.33f));
        BarrelMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
        MagazineMesh->SetRelativeLocation(FVector(1.0f, 0.0f, -19.0f));
        MagazineMesh->SetRelativeScale3D(FVector(0.06f, 0.05f, 0.14f));
        MagazineMesh->SetRelativeRotation(FRotator::ZeroRotator);
        GripMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -14.0f));
        GripMesh->SetRelativeScale3D(FVector(0.065f, 0.05f, 0.13f));
        GripMesh->SetRelativeRotation(FRotator::ZeroRotator);
        OpticMesh->SetRelativeLocation(FVector(4.0f, 0.0f, 0.0f));
        OpticMesh->SetRelativeScale3D(FVector(0.075f, 0.04f, 0.04f));
        OpticMesh->SetRelativeRotation(FRotator::ZeroRotator);
        StockMesh->SetRelativeLocation(FVector(-17.0f, 0.0f, -8.0f));
        StockMesh->SetRelativeScale3D(FVector(0.18f, 0.105f, 0.08f));
        StockMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, -4.0f));
        HandguardMesh->SetRelativeLocation(FVector(24.0f, 0.0f, -7.0f));
        HandguardMesh->SetRelativeScale3D(FVector(0.16f, 0.095f, 0.06f));
        HandguardMesh->SetRelativeRotation(FRotator::ZeroRotator);
        MuzzleDeviceMesh->SetRelativeLocation(FVector(55.0f, 0.0f, -7.0f));
        MuzzleDeviceMesh->SetRelativeScale3D(FVector(0.05f, 0.05f, 0.085f));
        MuzzleDeviceMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
        RailMesh->SetRelativeLocation(FVector(7.0f, 0.0f, -2.8f));
        RailMesh->SetRelativeScale3D(FVector(0.13f, 0.025f, 0.016f));
        RailMesh->SetRelativeRotation(FRotator::ZeroRotator);
        FrontSightMesh->SetRelativeLocation(FVector(47.0f, 0.0f, -1.7f));
        FrontSightMesh->SetRelativeScale3D(FVector(0.025f, 0.022f, 0.05f));
        FrontSightMesh->SetRelativeRotation(FRotator::ZeroRotator);
        TriggerGuardMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -10.5f));
        TriggerGuardMesh->SetRelativeScale3D(FVector(0.065f, 0.022f, 0.018f));
        TriggerGuardMesh->SetRelativeRotation(FRotator::ZeroRotator);
        ChargingHandleMesh->SetRelativeLocation(FVector(12.0f, 0.0f, -2.5f));
        ChargingHandleMesh->SetRelativeScale3D(FVector(0.05f, 0.018f, 0.014f));
        ChargingHandleMesh->SetRelativeRotation(FRotator::ZeroRotator);

        switch (ReplicatedWeaponId)
        {
        case EBMWeaponId::Kestrel9:
            WeaponMesh->SetRelativeScale3D(FVector(0.23f, 0.085f, 0.065f));
            BarrelMesh->SetRelativeLocation(FVector(27.0f, 0.0f, -7.0f));
            BarrelMesh->SetRelativeScale3D(FVector(0.029f, 0.029f, 0.20f));
            MuzzleDeviceMesh->SetRelativeLocation(FVector(43.0f, 0.0f, -7.0f));
            MuzzleDeviceMesh->SetRelativeScale3D(FVector(0.042f, 0.042f, 0.065f));
            HandguardMesh->SetRelativeLocation(FVector(20.0f, 0.0f, -7.0f));
            HandguardMesh->SetRelativeScale3D(FVector(0.13f, 0.09f, 0.055f));
            StockMesh->SetRelativeLocation(FVector(-16.0f, 0.0f, -8.0f));
            StockMesh->SetRelativeScale3D(FVector(0.17f, 0.10f, 0.07f));
            MagazineMesh->SetRelativeScale3D(FVector(0.055f, 0.05f, 0.16f));
            OpticMesh->SetRelativeScale3D(FVector(0.06f, 0.035f, 0.035f));
            FrontSightMesh->SetRelativeLocation(FVector(36.0f, 0.0f, -1.7f));
            break;

        case EBMWeaponId::RookDMR:
            WeaponMesh->SetRelativeScale3D(FVector(0.29f, 0.09f, 0.075f));
            BarrelMesh->SetRelativeLocation(FVector(52.0f, 0.0f, -7.0f));
            BarrelMesh->SetRelativeScale3D(FVector(0.041f, 0.041f, 0.49f));
            MuzzleDeviceMesh->SetRelativeLocation(FVector(80.0f, 0.0f, -7.0f));
            MuzzleDeviceMesh->SetRelativeScale3D(FVector(0.055f, 0.055f, 0.095f));
            HandguardMesh->SetRelativeLocation(FVector(35.0f, 0.0f, -7.0f));
            HandguardMesh->SetRelativeScale3D(FVector(0.20f, 0.10f, 0.065f));
            StockMesh->SetRelativeLocation(FVector(-19.0f, 0.0f, -8.0f));
            StockMesh->SetRelativeScale3D(FVector(0.21f, 0.11f, 0.085f));
            MagazineMesh->SetRelativeLocation(FVector(1.0f, 0.0f, -18.0f));
            MagazineMesh->SetRelativeScale3D(FVector(0.055f, 0.045f, 0.09f));
            OpticMesh->SetRelativeLocation(FVector(5.0f, 0.0f, 0.0f));
            OpticMesh->SetRelativeScale3D(FVector(0.11f, 0.045f, 0.05f));
            FrontSightMesh->SetRelativeLocation(FVector(68.0f, 0.0f, -1.7f));
            break;

        case EBMWeaponId::Breach12:
            WeaponMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -7.0f));
            WeaponMesh->SetRelativeScale3D(FVector(0.28f, 0.12f, 0.10f));
            BarrelMesh->SetRelativeLocation(FVector(48.0f, 0.0f, -5.0f));
            BarrelMesh->SetRelativeScale3D(FVector(0.058f, 0.058f, 0.30f));
            MuzzleDeviceMesh->SetRelativeLocation(FVector(70.0f, 0.0f, -5.0f));
            MuzzleDeviceMesh->SetRelativeScale3D(FVector(0.075f, 0.075f, 0.09f));
            HandguardMesh->SetRelativeLocation(FVector(38.0f, 0.0f, -5.0f));
            HandguardMesh->SetRelativeScale3D(FVector(0.22f, 0.115f, 0.075f));
            MagazineMesh->SetRelativeLocation(FVector(30.0f, 0.0f, -12.0f));
            MagazineMesh->SetRelativeScale3D(FVector(0.035f, 0.04f, 0.20f));
            GripMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -14.0f));
            GripMesh->SetRelativeScale3D(FVector(0.065f, 0.05f, 0.13f));
            break;

        case EBMWeaponId::SolacePistol:
            WeaponMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -6.0f));
            WeaponMesh->SetRelativeScale3D(FVector(0.21f, 0.08f, 0.075f));
            BarrelMesh->SetRelativeLocation(FVector(22.0f, 0.0f, -6.0f));
            BarrelMesh->SetRelativeScale3D(FVector(0.038f, 0.038f, 0.14f));
            MuzzleDeviceMesh->SetRelativeLocation(FVector(32.0f, 0.0f, -6.0f));
            MuzzleDeviceMesh->SetRelativeScale3D(FVector(0.045f, 0.045f, 0.055f));
            MagazineMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -18.0f));
            MagazineMesh->SetRelativeScale3D(FVector(0.05f, 0.045f, 0.10f));
            GripMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -14.0f));
            GripMesh->SetRelativeScale3D(FVector(0.055f, 0.045f, 0.12f));
            OpticMesh->SetRelativeLocation(FVector(3.0f, 0.0f, 0.0f));
            OpticMesh->SetRelativeScale3D(FVector(0.05f, 0.03f, 0.03f));
            break;

        case EBMWeaponId::ArcLauncher:
            WeaponMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -5.0f));
            WeaponMesh->SetRelativeScale3D(FVector(0.32f, 0.12f, 0.12f));
            BarrelMesh->SetRelativeLocation(FVector(52.0f, 0.0f, -3.0f));
            BarrelMesh->SetRelativeScale3D(FVector(0.095f, 0.095f, 0.36f));
            MuzzleDeviceMesh->SetRelativeLocation(FVector(76.0f, 0.0f, -3.0f));
            MuzzleDeviceMesh->SetRelativeScale3D(FVector(0.11f, 0.11f, 0.095f));
            HandguardMesh->SetRelativeLocation(FVector(40.0f, 0.0f, -3.0f));
            HandguardMesh->SetRelativeScale3D(FVector(0.22f, 0.12f, 0.10f));
            StockMesh->SetRelativeLocation(FVector(-20.0f, 0.0f, -4.0f));
            StockMesh->SetRelativeScale3D(FVector(0.22f, 0.12f, 0.10f));
            OpticMesh->SetRelativeLocation(FVector(7.0f, 0.0f, 4.0f));
            OpticMesh->SetRelativeScale3D(FVector(0.13f, 0.05f, 0.06f));
            break;

        case EBMWeaponId::ARX4:
        case EBMWeaponId::FragGrenade:
        case EBMWeaponId::VeilGrenade:
        default:
            break;
        }
    }

    // Keep the authored mesh on the replicated third-person path, where its
    // Grip/Muzzle sockets are the shared attachment and firing contract. The
    // local first-person weapon deliberately uses the assembled presentation
    // kit below: it exposes the barrel, magazine, grip, stock, handguard,
    // trigger guard, charging handle, and sights at a useful camera scale
    // instead of reducing the rifle to a single blocky receiver.
    if (bThirdPersonPresentation)
    {
        UStaticMesh* ProjectWeaponMesh = nullptr;
        if (WeaponData && !WeaponData->ThirdPersonMesh.IsNull())
        {
            ProjectWeaponMesh = WeaponData->ThirdPersonMesh.LoadSynchronous();
        }
        if (!ProjectWeaponMesh)
        {
            ProjectWeaponMesh = LoadProjectWeaponMesh(ReplicatedWeaponId);
        }

        if (ProjectWeaponMesh)
        {
            // These meshes are authored around the top of the pistol grip,
            // matching the hand_r socket. They are the authoritative visible
            // firearm silhouette; the supporting barrel/stock geometry below
            // stays enabled in third person so the rifle remains readable in
            // dark scenes and at encounter distance without replacing the
            // project asset with a box fallback.
            WeaponMesh->SetStaticMesh(ProjectWeaponMesh);
            // Reassert the third-person visibility contract after replacing
            // the default cube. A replicated enemy can reach this path while
            // its owner is still unresolved, and SetOnlyOwnerSee from the
            // first-person default would otherwise hide a correctly mounted
            // authored weapon from observers.
            WeaponMesh->SetOnlyOwnerSee(false);
            WeaponMesh->SetOwnerNoSee(false);
            WeaponMesh->SetVisibility(true, true);
            const FVector ProjectWeaponLocation = bThirdPersonPresentation
                ? FVector::ZeroVector
                : FVector(18.0f, 8.0f, -10.0f);
            const float ProjectWeaponScale = bThirdPersonPresentation ? 1.34f : 0.80f;
            WeaponMesh->SetRelativeLocation(ProjectWeaponLocation);
            WeaponMesh->SetRelativeRotation(FRotator::ZeroRotator);
            WeaponMesh->SetRelativeScale3D(FVector(ProjectWeaponScale));

            // Keep the authored grip socket explicit in the runtime contract.
            // The mesh origin is authored at the same point, so this also
            // gives future art a named attachment reference without changing
            // the enemy character code.
            const FName GripSocketName = WeaponData && !WeaponData->GripSocketName.IsNone()
                ? WeaponData->GripSocketName
                : FName(TEXT("Grip"));
            if (ProjectWeaponMesh->FindSocket(GripSocketName))
            {
                UE_LOG(
                    LogTemp,
                    Verbose,
                    TEXT("Black Meridian weapon asset: grip socket resolved asset=%s socket=%s"),
                    *ProjectWeaponMesh->GetPathName(),
                    *GripSocketName.ToString());
            }

            if (bThirdPersonPresentation)
            {
                const auto SetThirdPersonSupportVisible = [](UStaticMeshComponent* Mesh)
                {
                    if (Mesh)
                    {
                        Mesh->SetOnlyOwnerSee(false);
                        Mesh->SetOwnerNoSee(false);
                        Mesh->SetVisibility(true, true);
                    }
                };

                // The mesh above is the real weapon. These low-poly support
                // pieces expose the barrel, handguard, magazine, grip, and
                // stock edges that otherwise disappear against a dark Manny
                // body. Their transforms are the same third-person profile
                // used when no replacement mesh is available.
                SetThirdPersonSupportVisible(BarrelMesh);
                SetThirdPersonSupportVisible(MagazineMesh);
                SetThirdPersonSupportVisible(GripMesh);
                SetThirdPersonSupportVisible(OpticMesh);
                SetThirdPersonSupportVisible(StockMesh);
                SetThirdPersonSupportVisible(HandguardMesh);
                SetThirdPersonSupportVisible(MuzzleDeviceMesh);
                SetThirdPersonSupportVisible(RailMesh);
                SetThirdPersonSupportVisible(FrontSightMesh);
                SetThirdPersonSupportVisible(TriggerGuardMesh);
                SetThirdPersonSupportVisible(ChargingHandleMesh);
            }

            // The hidden cylinder remains a stable muzzle locator for the
            // authoritative trace and replicated flash/tracer effects when an
            // older asset has no Muzzle socket. Current project meshes use
            // the asset socket in GetMuzzleLocation(), so this locator is only
            // a compatibility fallback.
            float MuzzleX = 67.0f;
            switch (ReplicatedWeaponId)
            {
            case EBMWeaponId::Kestrel9:
                MuzzleX = 60.0f;
                break;
            case EBMWeaponId::RookDMR:
                MuzzleX = 101.0f;
                break;
            case EBMWeaponId::Breach12:
                MuzzleX = 90.0f;
                break;
            case EBMWeaponId::SolacePistol:
                MuzzleX = 48.0f;
                break;
            case EBMWeaponId::ArcLauncher:
                MuzzleX = 96.0f;
                break;
            case EBMWeaponId::ARX4:
            case EBMWeaponId::FragGrenade:
            case EBMWeaponId::VeilGrenade:
            default:
                break;
            }
            MuzzleDeviceMesh->SetRelativeLocation(FVector(MuzzleX, 0.0f, 0.0f));
            MuzzleDeviceMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
            // Keep the authored receiver and the project-safe functional
            // pieces visible together in third person. The support geometry
            // makes the muzzle, magazine, stock, and sight line legible when
            // the rifle is aimed directly toward the camera.
            bUsingProjectWeaponMesh = true;

#if !UE_BUILD_SHIPPING
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Black Meridian weapon asset: loaded=%s id=%d presentation=%s muzzle_x=%.0f"),
                *ProjectWeaponMesh->GetPathName(),
                static_cast<int32>(ReplicatedWeaponId),
                bThirdPersonPresentation ? TEXT("third_person") : TEXT("first_person"),
                MuzzleX);
#endif
        }
    }

    // Cache the post-variant transforms. Presentation animation must layer on
    // top of each weapon identity rather than slowly accumulating offsets.
    PresentationBaseLocation = WeaponMesh->GetRelativeLocation();
    PresentationBaseRotation = WeaponMesh->GetRelativeRotation();
    PresentationMagazineBaseLocation = MagazineMesh->GetRelativeLocation();
    PresentationMagazineBaseRotation = MagazineMesh->GetRelativeRotation();
    PresentationFirstPersonMagazineBaseLocation = FirstPersonMagazineGeometryMesh
        ? FirstPersonMagazineGeometryMesh->GetRelativeLocation()
        : FVector::ZeroVector;
    PresentationFirstPersonMagazineBaseRotation = FirstPersonMagazineGeometryMesh
        ? FirstPersonMagazineGeometryMesh->GetRelativeRotation()
        : FRotator::ZeroRotator;
    PresentationChargingHandleBaseLocation = ChargingHandleMesh->GetRelativeLocation();
    PresentationChargingHandleBaseRotation = ChargingHandleMesh->GetRelativeRotation();
}

void ABMWeapon::BuildFirstPersonARX4Geometry()
{
    if (!FirstPersonGeometryMesh || bFirstPersonGeometryBuilt)
    {
        return;
    }

    FirstPersonGeometryMesh->ClearAllMeshSections();

    const auto CreateSection = [this](const int32 SectionIndex, FRuntimeWeaponMeshBuilder& Builder)
    {
        if (Builder.Vertices.Num() == 0 || Builder.Triangles.Num() == 0)
        {
            return;
        }

        const TArray<FLinearColor> EmptyColors;
        const TArray<FProcMeshTangent> EmptyTangents;
        FirstPersonGeometryMesh->CreateMeshSection_LinearColor(
            SectionIndex,
            Builder.Vertices,
            Builder.Triangles,
            Builder.Normals,
            Builder.UV0,
            EmptyColors,
            EmptyTangents,
            false);
    };

    // The viewmodel is intentionally assembled as one connected low-poly
    // firearm. The old version left the optic, rail, front sight, barrel, and
    // charging handle as BasicShapes cubes; that made the rifle read like a
    // greybox prop even after the receiver had been beveled.
    FRuntimeWeaponMeshBuilder Receiver;
    Receiver.AddTaperedChamferedBox(
        FVector(30.0f, 8.0f, -12.0f),
        FVector(17.5f, 3.45f, 2.85f),
        FVector(16.0f, 3.10f, 2.55f),
        0.65f);
    // Upper receiver and magazine well break up the single slab silhouette.
    Receiver.AddChamferedBox(
        FVector(34.0f, 8.0f, -9.65f),
        FVector(8.5f, 3.0f, 0.70f),
        0.24f);
    Receiver.AddChamferedBox(
        FVector(24.5f, 8.0f, -14.35f),
        FVector(4.8f, 2.85f, 1.05f),
        0.30f,
        FRotator(0.0f, 0.0f, -3.0f));
    CreateSection(0, Receiver);

    FRuntimeWeaponMeshBuilder Stock;
    Stock.AddTaperedChamferedBox(
        FVector(5.0f, 8.0f, -11.7f),
        FVector(11.0f, 3.35f, 2.65f),
        FVector(12.5f, 3.05f, 2.35f),
        0.58f,
        FRotator(0.0f, 0.0f, -2.0f));
    // A raised cheek piece and buffer-tube shoulder keep the rear furniture
    // from looking like a second rectangular receiver.
    Stock.AddChamferedBox(
        FVector(1.0f, 8.0f, -8.75f),
        FVector(5.0f, 2.45f, 0.60f),
        0.24f,
        FRotator(0.0f, 0.0f, -2.0f));
    CreateSection(1, Stock);

    FRuntimeWeaponMeshBuilder Handguard;
    Handguard.AddOctagonalTube(
        FVector(47.0f, 8.0f, -10.6f),
        13.0f,
        3.55f,
        2.95f);
    // Subtle side rails and cooling ribs give the handguard a functional
    // surface pattern without turning it into a row of floating cubes.
    for (int32 RibIndex = 0; RibIndex < 5; ++RibIndex)
    {
        const float RibX = 38.5f + static_cast<float>(RibIndex) * 4.0f;
        Handguard.AddChamferedBox(
            FVector(RibX, 4.62f, -10.6f),
            FVector(0.32f, 0.16f, 1.75f),
            0.08f);
    }
    CreateSection(2, Handguard);

    FRuntimeWeaponMeshBuilder Grip;
    Grip.AddTaperedChamferedBox(
        FVector(18.5f, 8.0f, -20.5f),
        FVector(2.45f, 1.95f, 9.8f),
        FVector(2.15f, 1.70f, 8.8f),
        0.44f,
        FRotator(-11.0f, 0.0f, 0.0f));
    Grip.AddChamferedBox(
        FVector(18.6f, 8.0f, -29.1f),
        FVector(2.35f, 1.85f, 0.80f),
        0.24f,
        FRotator(-11.0f, 0.0f, 0.0f));
    CreateSection(3, Grip);

    FRuntimeWeaponMeshBuilder Buttpad;
    Buttpad.AddChamferedBox(
        FVector(-6.25f, 8.0f, -11.8f),
        FVector(1.25f, 3.5f, 2.95f),
        0.48f,
        FRotator(0.0f, 0.0f, -2.0f));
    CreateSection(4, Buttpad);

    FRuntimeWeaponMeshBuilder EjectionPort;
    EjectionPort.AddChamferedBox(
        FVector(35.5f, 11.28f, -11.55f),
        FVector(5.0f, 0.14f, 0.82f),
        0.10f);
    EjectionPort.AddChamferedBox(
        FVector(36.5f, 11.48f, -10.38f),
        FVector(3.0f, 0.10f, 0.18f),
        0.06f,
        FRotator(0.0f, 0.0f, -4.0f));
    EjectionPort.AddChamferedBox(
        FVector(29.5f, 11.40f, -12.75f),
        FVector(0.65f, 0.16f, 0.72f),
        0.12f);
    CreateSection(5, EjectionPort);

    // A complete trigger guard, trigger shoe, and magazine catch make the
    // lower receiver read as a working rifle rather than a solid block.
    FRuntimeWeaponMeshBuilder TriggerGuard;
    TriggerGuard.AddChamferedBox(FVector(22.8f, 8.0f, -14.25f), FVector(1.0f, 0.78f, 0.95f), 0.18f);
    TriggerGuard.AddChamferedBox(FVector(17.2f, 8.0f, -14.25f), FVector(1.0f, 0.78f, 0.95f), 0.18f);
    TriggerGuard.AddChamferedBox(FVector(20.0f, 8.0f, -15.75f), FVector(3.0f, 0.78f, 0.52f), 0.18f);
    TriggerGuard.AddChamferedBox(FVector(20.2f, 11.22f, -14.20f), FVector(0.42f, 0.24f, 0.90f), 0.14f);
    CreateSection(6, TriggerGuard);

    FRuntimeWeaponMeshBuilder Barrel;
    Barrel.AddCylindricalTube(
        FVector(68.0f, 8.0f, -10.55f),
        13.0f,
        1.05f,
        1.05f,
        12);
    Barrel.AddCylindricalTube(
        FVector(56.5f, 8.0f, -10.55f),
        1.8f,
        1.45f,
        1.35f,
        10);
    CreateSection(7, Barrel);

    FRuntimeWeaponMeshBuilder Optic;
    Optic.AddChamferedBox(
        FVector(30.5f, 8.0f, -7.45f),
        FVector(4.3f, 1.75f, 0.42f),
        0.16f);
    Optic.AddTaperedChamferedBox(
        FVector(31.0f, 8.0f, -5.65f),
        FVector(3.6f, 1.50f, 1.35f),
        FVector(2.75f, 1.25f, 1.05f),
        0.30f);
    CreateSection(8, Optic);

    FRuntimeWeaponMeshBuilder OpticLens;
    OpticLens.AddCylindricalTube(
        FVector(34.0f, 8.0f, -5.65f),
        0.20f,
        0.92f,
        0.72f,
        10);
    CreateSection(9, OpticLens);

    FRuntimeWeaponMeshBuilder Rail;
    Rail.AddChamferedBox(
        FVector(37.0f, 8.0f, -8.35f),
        FVector(13.5f, 1.75f, 0.34f),
        0.12f);
    for (int32 SlotIndex = 0; SlotIndex < 8; ++SlotIndex)
    {
        Rail.AddChamferedBox(
            FVector(25.0f + static_cast<float>(SlotIndex) * 3.0f, 8.0f, -7.90f),
            FVector(0.38f, 1.90f, 0.22f),
            0.08f);
    }
    // A short handguard rail continues the top line toward the front sight.
    Rail.AddChamferedBox(
        FVector(51.0f, 8.0f, -7.55f),
        FVector(8.0f, 1.75f, 0.28f),
        0.10f);
    CreateSection(10, Rail);

    FRuntimeWeaponMeshBuilder FrontSight;
    FrontSight.AddChamferedBox(
        FVector(59.0f, 8.0f, -7.25f),
        FVector(1.75f, 1.40f, 0.34f),
        0.12f);
    FrontSight.AddChamferedBox(
        FVector(59.0f, 6.85f, -5.65f),
        FVector(0.52f, 0.34f, 1.25f),
        0.12f);
    FrontSight.AddChamferedBox(
        FVector(59.0f, 9.15f, -5.65f),
        FVector(0.52f, 0.34f, 1.25f),
        0.12f);
    FrontSight.AddChamferedBox(
        FVector(59.0f, 8.0f, -4.35f),
        FVector(0.85f, 1.40f, 0.30f),
        0.12f);
    CreateSection(11, FrontSight);

    FRuntimeWeaponMeshBuilder MuzzleDevice;
    MuzzleDevice.AddCylindricalTube(
        FVector(80.5f, 8.0f, -10.55f),
        2.5f,
        1.45f,
        1.35f,
        12);
    MuzzleDevice.AddChamferedBox(
        FVector(78.3f, 8.0f, -10.55f),
        FVector(0.38f, 1.70f, 1.55f),
        0.16f);
    MuzzleDevice.AddChamferedBox(
        FVector(82.7f, 8.0f, -10.55f),
        FVector(0.38f, 1.70f, 1.55f),
        0.16f);
    CreateSection(12, MuzzleDevice);

    FRuntimeWeaponMeshBuilder ChargingHandle;
    ChargingHandle.AddChamferedBox(
        FVector(39.5f, 11.45f, -8.25f),
        FVector(2.35f, 0.34f, 0.38f),
        0.14f);
    ChargingHandle.AddChamferedBox(
        FVector(42.0f, 11.60f, -8.25f),
        FVector(0.55f, 0.50f, 0.62f),
        0.16f);
    CreateSection(13, ChargingHandle);

    FRuntimeWeaponMeshBuilder Controls;
    Controls.AddChamferedBox(
        FVector(22.2f, 11.40f, -12.85f),
        FVector(0.42f, 0.22f, 0.72f),
        0.12f,
        FRotator(0.0f, 0.0f, -18.0f));
    Controls.AddChamferedBox(
        FVector(18.7f, 11.35f, -12.45f),
        FVector(0.75f, 0.20f, 0.20f),
        0.08f);
    Controls.AddChamferedBox(
        FVector(26.4f, 11.38f, -14.0f),
        FVector(0.62f, 0.20f, 0.30f),
        0.10f);
    CreateSection(14, Controls);

    bFirstPersonGeometryBuilt = true;
}

void ABMWeapon::BuildFirstPersonMagazineGeometry()
{
    if (!FirstPersonMagazineGeometryMesh || bFirstPersonMagazineGeometryBuilt)
    {
        return;
    }

    FirstPersonMagazineGeometryMesh->ClearAllMeshSections();

    const auto CreateSection = [this](const int32 SectionIndex, FRuntimeWeaponMeshBuilder& Builder)
    {
        if (Builder.Vertices.Num() == 0 || Builder.Triangles.Num() == 0)
        {
            return;
        }

        const TArray<FLinearColor> EmptyColors;
        const TArray<FProcMeshTangent> EmptyTangents;
        FirstPersonMagazineGeometryMesh->CreateMeshSection_LinearColor(
            SectionIndex,
            Builder.Vertices,
            Builder.Triangles,
            Builder.Normals,
            Builder.UV0,
            EmptyColors,
            EmptyTangents,
            false);
    };

    // Build the magazine around its own component origin so the reload layer
    // can drop and rotate it around the same pivot as the old static piece.
    FRuntimeWeaponMeshBuilder MagazineBody;
    MagazineBody.AddChamferedBox(
        FVector::ZeroVector,
        FVector(2.25f, 1.85f, 10.8f),
        0.42f);
    CreateSection(0, MagazineBody);

    FRuntimeWeaponMeshBuilder MagazineDetails;
    MagazineDetails.AddChamferedBox(
        FVector(0.0f, 0.0f, 10.9f),
        FVector(2.45f, 1.98f, 0.70f),
        0.25f);
    MagazineDetails.AddChamferedBox(
        FVector(0.0f, 0.0f, -10.9f),
        FVector(2.45f, 1.98f, 0.70f),
        0.25f);
    MagazineDetails.AddChamferedBox(
        FVector(0.0f, -1.92f, 0.0f),
        FVector(1.45f, 0.12f, 7.8f),
        0.10f);
    CreateSection(1, MagazineDetails);

    FirstPersonMagazineGeometryMesh->SetRelativeLocation(FVector(27.0f, 8.0f, -22.5f));
    FirstPersonMagazineGeometryMesh->SetRelativeRotation(FRotator(-10.0f, 0.0f, 0.0f));
    bFirstPersonMagazineGeometryBuilt = true;
}

void ABMWeapon::ApplyPresentationMaterials()
{
    UMaterialInterface* BaseMaterial = LoadPresentationMaterial();
    if (!BaseMaterial)
    {
        return;
    }

    FLinearColor AccentColor(0.08f, 0.46f, 0.34f);
    switch (ReplicatedWeaponId)
    {
    case EBMWeaponId::Kestrel9:
        AccentColor = FLinearColor(0.08f, 0.38f, 0.68f);
        break;
    case EBMWeaponId::RookDMR:
        AccentColor = FLinearColor(0.56f, 0.34f, 0.08f);
        break;
    case EBMWeaponId::Breach12:
        AccentColor = FLinearColor(0.72f, 0.2f, 0.06f);
        break;
    case EBMWeaponId::SolacePistol:
        AccentColor = FLinearColor(0.26f, 0.2f, 0.7f);
        break;
    case EBMWeaponId::ArcLauncher:
        AccentColor = FLinearColor(0.42f, 0.12f, 0.75f);
        break;
    case EBMWeaponId::ARX4:
    case EBMWeaponId::FragGrenade:
    case EBMWeaponId::VeilGrenade:
    default:
        break;
    }

    // The source-only Vanta weapons need to read as weapons at the same
    // distance as the dark Manny silhouettes. Keep the receiver grounded,
    // but add a restrained rust-red faction treatment to the third-person
    // kit. The local weapon uses a neutral graphite/polymer palette so its
    // silhouette and functional parts carry the realism, not a bright HUD
    // accent or an emissive block.
    const ABMCombatCharacter* CombatOwner = Cast<ABMCombatCharacter>(GetOwner());
    const bool bVantaThirdPerson = bThirdPersonPresentation
        && CombatOwner
        && CombatOwner->GetTeam() == EBMTeam::Vanta;
    const FLinearColor FactionAccent = bVantaThirdPerson
        ? FLinearColor(0.92f, 0.12f, 0.035f)
        : AccentColor;

    const auto ApplyPiece = [BaseMaterial](UStaticMeshComponent* Mesh, const FLinearColor& Color, const float Metallic, const float Roughness)
    {
        if (!Mesh)
        {
            return;
        }

        const int32 MaterialSlotCount = FMath::Max(1, Mesh->GetNumMaterials());
        for (int32 MaterialSlot = 0; MaterialSlot < MaterialSlotCount; ++MaterialSlot)
        {
            if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(MaterialSlot, BaseMaterial))
            {
                Material->SetVectorParameterValue(TEXT("Color"), Color);
                Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
                Material->SetScalarParameterValue(TEXT("Metallic"), Metallic);
                Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
            }
        }
    };

    const bool bReadableFirstPerson = !bThirdPersonPresentation && bUsingProjectWeaponMesh;
    const bool bProceduralFirstPerson = !bThirdPersonPresentation && !bUsingProjectWeaponMesh;
    const FLinearColor ReceiverColor = bThirdPersonPresentation
        ? (bVantaThirdPerson
            // The weapon should separate from the dark Manny body without
            // becoming an emissive/toy-colored prop. A cool graphite receiver
            // gives the edges enough value contrast in the normal night scene.
            ? FLinearColor(0.24f, 0.28f, 0.30f)
            : FLinearColor(0.26f, 0.30f, 0.32f))
        : (bReadableFirstPerson
            ? FLinearColor(0.19f, 0.26f, 0.30f)
            : (bProceduralFirstPerson ? FLinearColor(0.085f, 0.105f, 0.125f) : FLinearColor(0.075f, 0.095f, 0.11f)));
    const FLinearColor BarrelColor = bThirdPersonPresentation
        ? FLinearColor(0.14f, 0.17f, 0.19f)
        : (bReadableFirstPerson
            ? FLinearColor(0.065f, 0.09f, 0.105f)
            : (bProceduralFirstPerson ? FLinearColor(0.070f, 0.080f, 0.092f) : FLinearColor(0.018f, 0.024f, 0.03f)));
    const FLinearColor MagazineColor = bThirdPersonPresentation
        ? FLinearColor(0.25f, 0.28f, 0.30f)
        : (bReadableFirstPerson
            ? FLinearColor(0.10f, 0.14f, 0.16f)
            : (bProceduralFirstPerson ? FLinearColor(0.060f, 0.072f, 0.085f) : FLinearColor(0.04f, 0.055f, 0.06f)));
    const FLinearColor GripColor = bThirdPersonPresentation
        ? FLinearColor(0.10f, 0.12f, 0.135f)
        : (bReadableFirstPerson
            ? FLinearColor(0.075f, 0.10f, 0.115f)
            : (bProceduralFirstPerson ? FLinearColor(0.032f, 0.040f, 0.048f) : FLinearColor(0.025f, 0.032f, 0.038f)));
    const FLinearColor StockColor = bThirdPersonPresentation
        ? FLinearColor(0.19f, 0.22f, 0.24f)
        : (bProceduralFirstPerson ? FLinearColor(0.050f, 0.062f, 0.075f) : FLinearColor(0.035f, 0.045f, 0.052f));
    const FLinearColor HandguardColor = bThirdPersonPresentation
        ? FLinearColor(0.23f, 0.26f, 0.28f)
        : (bProceduralFirstPerson ? FLinearColor(0.075f, 0.090f, 0.105f) : FLinearColor(0.055f, 0.068f, 0.078f));
    ApplyPiece(WeaponMesh, ReceiverColor, 0.62f, 0.56f);
    ApplyPiece(BarrelMesh, BarrelColor, 0.78f, 0.42f);
    ApplyPiece(MagazineMesh, MagazineColor, 0.5f, 0.68f);
    ApplyPiece(GripMesh, GripColor, 0.2f, 0.86f);
    ApplyPiece(
        OpticMesh,
        bThirdPersonPresentation
            ? FactionAccent
            : (bProceduralFirstPerson ? FLinearColor(0.055f, 0.06f, 0.065f) : FLinearColor(0.24f, 0.16f, 0.05f)),
        0.24f,
        0.48f);
    ApplyPiece(
        StockMesh,
        StockColor,
        0.42f,
        0.68f);
    ApplyPiece(
        HandguardMesh,
        HandguardColor,
        0.58f,
        0.54f);
    ApplyPiece(
        MuzzleDeviceMesh,
        bThirdPersonPresentation
            ? FLinearColor(0.035f, 0.045f, 0.052f)
            : (bProceduralFirstPerson ? FLinearColor(0.055f, 0.062f, 0.068f) : FLinearColor(0.012f, 0.016f, 0.02f)),
        0.82f,
        0.36f);
    ApplyPiece(
        RailMesh,
        bThirdPersonPresentation
            ? FactionAccent
            : (bProceduralFirstPerson ? FLinearColor(0.065f, 0.072f, 0.078f) : FLinearColor(0.012f, 0.016f, 0.02f)),
        0.75f,
        0.50f);
    ApplyPiece(
        FrontSightMesh,
        bThirdPersonPresentation
            ? FactionAccent * 0.7f
            : (bProceduralFirstPerson ? FLinearColor(0.16f, 0.17f, 0.18f) : FLinearColor(0.056f, 0.322f, 0.238f)),
        0.32f,
        0.46f);
    ApplyPiece(
        TriggerGuardMesh,
        bThirdPersonPresentation
            ? FLinearColor(0.035f, 0.045f, 0.052f)
            : (bProceduralFirstPerson ? FLinearColor(0.040f, 0.047f, 0.054f) : FLinearColor(0.012f, 0.016f, 0.02f)),
        0.35f,
        0.78f);
    ApplyPiece(
        ChargingHandleMesh,
        bThirdPersonPresentation
            ? FLinearColor(0.055f, 0.07f, 0.08f)
            : (bProceduralFirstPerson ? FLinearColor(0.10f, 0.11f, 0.12f) : FLinearColor(0.018f, 0.024f, 0.03f)),
        0.72f,
        0.42f);

    const auto ApplyPieceSlot = [BaseMaterial](UProceduralMeshComponent* Mesh, const int32 MaterialSlot, const FLinearColor& Color, const float Metallic, const float Roughness)
    {
        if (UMaterialInstanceDynamic* Material = Mesh
            ? Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(MaterialSlot, BaseMaterial)
            : nullptr)
        {
            Material->SetVectorParameterValue(TEXT("Color"), Color);
            Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
            Material->SetScalarParameterValue(TEXT("Metallic"), Metallic);
            Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
        }
    };

    // The local body uses separate graphite, polymer, steel, optic, and
    // control sections. Their restrained value differences create edge
    // highlights without bringing back the old emissive/toy treatment.
    ApplyPieceSlot(FirstPersonGeometryMesh, 0, ReceiverColor, 0.62f, 0.56f);
    ApplyPieceSlot(
        FirstPersonGeometryMesh,
        1,
        bProceduralFirstPerson ? StockColor : StockColor,
        0.34f,
        0.72f);
    ApplyPieceSlot(
        FirstPersonGeometryMesh,
        2,
        bProceduralFirstPerson ? HandguardColor : HandguardColor,
        0.52f,
        0.60f);
    ApplyPieceSlot(FirstPersonGeometryMesh, 3, GripColor, 0.18f, 0.88f);
    ApplyPieceSlot(
        FirstPersonGeometryMesh,
        4,
        bProceduralFirstPerson ? FLinearColor(0.018f, 0.022f, 0.026f) : StockColor,
        0.12f,
        0.92f);
    ApplyPieceSlot(FirstPersonGeometryMesh, 5, BarrelColor, 0.72f, 0.48f);
    ApplyPieceSlot(FirstPersonGeometryMesh, 6, GripColor, 0.18f, 0.88f);
    ApplyPieceSlot(FirstPersonGeometryMesh, 7, BarrelColor, 0.76f, 0.44f);
    ApplyPieceSlot(
        FirstPersonGeometryMesh,
        8,
        bProceduralFirstPerson ? FLinearColor(0.055f, 0.066f, 0.078f) : StockColor,
        0.54f,
        0.58f);
    ApplyPieceSlot(
        FirstPersonGeometryMesh,
        9,
        bProceduralFirstPerson ? FLinearColor(0.030f, 0.055f, 0.060f) : BarrelColor,
        0.12f,
        0.30f);
    ApplyPieceSlot(
        FirstPersonGeometryMesh,
        10,
        bProceduralFirstPerson ? FLinearColor(0.095f, 0.11f, 0.125f) : BarrelColor,
        0.78f,
        0.42f);
    ApplyPieceSlot(
        FirstPersonGeometryMesh,
        11,
        bProceduralFirstPerson ? FLinearColor(0.080f, 0.095f, 0.11f) : BarrelColor,
        0.74f,
        0.46f);
    ApplyPieceSlot(
        FirstPersonGeometryMesh,
        12,
        bProceduralFirstPerson ? FLinearColor(0.090f, 0.105f, 0.12f) : BarrelColor,
        0.80f,
        0.38f);
    ApplyPieceSlot(
        FirstPersonGeometryMesh,
        13,
        bProceduralFirstPerson ? FLinearColor(0.115f, 0.13f, 0.145f) : BarrelColor,
        0.78f,
        0.36f);
    ApplyPieceSlot(
        FirstPersonGeometryMesh,
        14,
        bProceduralFirstPerson ? FLinearColor(0.105f, 0.12f, 0.135f) : BarrelColor,
        0.70f,
        0.42f);

    ApplyPieceSlot(FirstPersonMagazineGeometryMesh, 0, MagazineColor, 0.46f, 0.70f);
    ApplyPieceSlot(
        FirstPersonMagazineGeometryMesh,
        1,
        bProceduralFirstPerson ? FLinearColor(0.085f, 0.10f, 0.115f) : BarrelColor,
        0.40f,
        0.62f);

    UMaterialInterface* FXMaterial = LoadFXMaterial();
    const auto ApplyFX = [FXMaterial](UStaticMeshComponent* Mesh, const FLinearColor& Color, TObjectPtr<UMaterialInstanceDynamic>& MaterialSlot)
    {
        if (!FXMaterial || !Mesh)
        {
            return;
        }
        MaterialSlot = Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, FXMaterial);
        if (MaterialSlot)
        {
            MaterialSlot->SetVectorParameterValue(TEXT("Color"), Color);
            MaterialSlot->SetVectorParameterValue(TEXT("BaseColor"), Color);
        }
    };
    ApplyFX(MuzzleFlashMesh, FLinearColor(1.0f, 0.3f, 0.04f), MuzzleFlashMaterial);
    ApplyFX(TracerMesh, FLinearColor(1.0f, 0.36f, 0.04f), TracerMaterial);
    ApplyFX(ImpactFlashMesh, FLinearColor(1.0f, 0.04f, 0.015f), ImpactFlashMaterial);
    ApplyFX(ImpactShardMeshA, FLinearColor(1.0f, 0.16f, 0.025f), ImpactShardMaterialA);
    ApplyFX(ImpactShardMeshB, FLinearColor(1.0f, 0.52f, 0.08f), ImpactShardMaterialB);

    if (BaseMaterial && bThirdPersonPresentation)
    {
        const auto ApplyReadableAccent = [BaseMaterial](UStaticMeshComponent* Mesh, const FLinearColor& Color)
        {
            if (!Mesh)
            {
                return;
            }
            if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMaterial))
            {
                const FLinearColor ReadableAccent = Color * 0.82f;
                Material->SetVectorParameterValue(TEXT("Color"), ReadableAccent);
                Material->SetVectorParameterValue(TEXT("BaseColor"), ReadableAccent);
                Material->SetScalarParameterValue(TEXT("Metallic"), 0.48f);
                Material->SetScalarParameterValue(TEXT("Roughness"), 0.52f);
            }
        };
        // Keep only a small, matte faction mark on the sight line. The body of
        // the rifle remains graphite so the held weapon reads as equipment,
        // not as a glowing red debug bar.
        const FLinearColor HostileAccent = bVantaThirdPerson
            ? FLinearColor(0.58f, 0.055f, 0.015f)
            : AccentColor;
        ApplyReadableAccent(OpticMesh, AccentColor);
        ApplyReadableAccent(RailMesh, HostileAccent);
        ApplyReadableAccent(FrontSightMesh, HostileAccent);
    }

    ApplyAttachmentPresentation();
}

void ABMWeapon::ApplyAttachmentPresentation()
{
    const TArray<UStaticMeshComponent*> AttachmentMeshes = {
        EquippedOpticMesh.Get(),
        EquippedMuzzleMesh.Get(),
        EquippedMagazineMesh.Get(),
        EquippedGripMesh.Get(),
        EquippedStockMesh.Get()};
    for (UStaticMeshComponent* Mesh : AttachmentMeshes)
    {
        if (Mesh)
        {
            Mesh->SetVisibility(false, true);
            Mesh->SetOnlyOwnerSee(!bThirdPersonPresentation);
            Mesh->SetOwnerNoSee(false);
        }
    }

    if (ReplicatedAttachmentState.Num() == 0)
    {
        return;
    }

    UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UMaterialInterface* BaseMaterial = LoadPresentationMaterial();

    const auto ResolveMesh = [CubeMesh, CylinderMesh](const FBMWeaponAttachmentState& State) -> UStaticMesh*
    {
        if (!State.MeshPath.IsEmpty())
        {
            if (UStaticMesh* AuthoredMesh = LoadObject<UStaticMesh>(nullptr, *State.MeshPath))
            {
                return AuthoredMesh;
            }
        }
        return State.Slot == TEXT("Muzzle") ? CylinderMesh : CubeMesh;
    };
    const auto ResolveComponent = [this](const FName Slot) -> UStaticMeshComponent*
    {
        if (Slot == TEXT("Optic"))
        {
            return EquippedOpticMesh;
        }
        if (Slot == TEXT("Muzzle"))
        {
            return EquippedMuzzleMesh;
        }
        if (Slot == TEXT("Magazine"))
        {
            return EquippedMagazineMesh;
        }
        if (Slot == TEXT("Grip"))
        {
            return EquippedGripMesh;
        }
        if (Slot == TEXT("Stock"))
        {
            return EquippedStockMesh;
        }
        return nullptr;
    };
    const auto ApplyAttachmentMaterial = [BaseMaterial](UStaticMeshComponent* Mesh, const FLinearColor& Color)
    {
        if (!BaseMaterial || !Mesh)
        {
            return;
        }
        if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMaterial))
        {
            Material->SetVectorParameterValue(TEXT("Color"), Color);
            Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
            Material->SetScalarParameterValue(TEXT("Metallic"), 0.58f);
            Material->SetScalarParameterValue(TEXT("Roughness"), 0.42f);
        }
    };

    for (const FBMWeaponAttachmentState& State : ReplicatedAttachmentState)
    {
        UStaticMeshComponent* Mesh = ResolveComponent(State.Slot);
        if (!Mesh)
        {
            continue;
        }

        UStaticMesh* StaticMesh = ResolveMesh(State);
        if (!StaticMesh)
        {
            continue;
        }
        Mesh->SetStaticMesh(StaticMesh);

        FVector Location = FVector::ZeroVector;
        FVector Scale = FVector(0.06f, 0.03f, 0.03f);
        FRotator Rotation = FRotator::ZeroRotator;
        FLinearColor Color(0.22f, 0.52f, 0.62f);

        if (bThirdPersonPresentation)
        {
            if (State.Slot == TEXT("Optic"))
            {
                Location = FVector(5.0f, 0.0f, 3.0f);
                Scale = FVector(0.085f, 0.045f, 0.050f);
                Color = FLinearColor(0.08f, 0.46f, 0.58f);
            }
            else if (State.Slot == TEXT("Muzzle"))
            {
                Location = FVector(55.0f, 0.0f, -7.0f);
                Rotation = FRotator(90.0f, 0.0f, 0.0f);
                Scale = State.bSuppressor
                    ? FVector(0.075f, 0.075f, 0.18f)
                    : FVector(0.060f, 0.060f, 0.105f);
                Color = State.bSuppressor
                    ? FLinearColor(0.06f, 0.22f, 0.30f)
                    : FLinearColor(0.18f, 0.20f, 0.22f);
            }
            else if (State.Slot == TEXT("Magazine"))
            {
                Location = FVector(1.0f, 0.0f, -20.0f);
                Rotation = FRotator(0.0f, 0.0f, -4.0f);
                Scale = FVector(0.065f, 0.052f, 0.17f);
                Color = FLinearColor(0.38f, 0.28f, 0.08f);
            }
            else if (State.Slot == TEXT("Grip"))
            {
                Location = FVector(0.0f, 0.0f, -15.0f);
                Rotation = FRotator(0.0f, 0.0f, -5.0f);
                Scale = FVector(0.072f, 0.055f, 0.15f);
                Color = FLinearColor(0.08f, 0.42f, 0.28f);
            }
            else if (State.Slot == TEXT("Stock"))
            {
                Location = FVector(-18.0f, 0.0f, -8.0f);
                Rotation = FRotator(0.0f, 0.0f, -4.0f);
                Scale = FVector(0.205f, 0.115f, 0.09f);
                Color = FLinearColor(0.34f, 0.16f, 0.48f);
            }
        }
        else
        {
            if (State.Slot == TEXT("Optic"))
            {
                Location = FVector(32.0f, 8.0f, -1.0f);
                Scale = FVector(0.070f, 0.030f, 0.040f);
                Color = FLinearColor(0.06f, 0.52f, 0.64f);
            }
            else if (State.Slot == TEXT("Muzzle"))
            {
                Location = FVector(78.0f, 8.0f, -10.2f);
                Rotation = FRotator(90.0f, 0.0f, 0.0f);
                Scale = State.bSuppressor
                    ? FVector(0.055f, 0.055f, 0.18f)
                    : FVector(0.045f, 0.045f, 0.10f);
                Color = State.bSuppressor
                    ? FLinearColor(0.04f, 0.28f, 0.42f)
                    : FLinearColor(0.18f, 0.21f, 0.23f);
            }
            else if (State.Slot == TEXT("Magazine"))
            {
                Location = FVector(29.0f, 8.0f, -18.5f);
                Rotation = FRotator(-10.0f, 0.0f, 0.0f);
                Scale = FVector(0.050f, 0.040f, 0.15f);
                Color = FLinearColor(0.42f, 0.28f, 0.06f);
            }
            else if (State.Slot == TEXT("Grip"))
            {
                Location = FVector(19.0f, 8.0f, -21.0f);
                Rotation = FRotator(-11.0f, 0.0f, 0.0f);
                Scale = FVector(0.055f, 0.040f, 0.15f);
                Color = FLinearColor(0.05f, 0.40f, 0.26f);
            }
            else if (State.Slot == TEXT("Stock"))
            {
                Location = FVector(-1.0f, 8.0f, -11.5f);
                Rotation = FRotator(0.0f, 0.0f, -2.0f);
                Scale = FVector(0.16f, 0.08f, 0.07f);
                Color = FLinearColor(0.31f, 0.14f, 0.44f);
            }
        }

        Mesh->SetRelativeLocation(Location);
        Mesh->SetRelativeRotation(Rotation);
        Mesh->SetRelativeScale3D(Scale);
        ApplyAttachmentMaterial(Mesh, Color);
        Mesh->SetVisibility(true, true);
    }
}

FVector ABMWeapon::GetMuzzleLocation() const
{
    // Project weapon meshes carry the muzzle contract in asset space. Resolve
    // this before the hidden fallback locator so the visible barrel and the
    // authoritative trace/flash/tracer remain co-located when final art
    // changes its proportions.
    const FName MuzzleSocketName = WeaponData && !WeaponData->MuzzleSocketName.IsNone()
        ? WeaponData->MuzzleSocketName
        : FName(TEXT("Muzzle"));
    if (bUsingProjectWeaponMesh
        && WeaponMesh
        && WeaponMesh->GetStaticMesh()
        && WeaponMesh->DoesSocketExist(MuzzleSocketName))
    {
        return WeaponMesh->GetSocketLocation(MuzzleSocketName);
    }

    if (MuzzleDeviceMesh && MuzzleDeviceMesh->GetStaticMesh())
    {
        const FVector MuzzleAxis = bFallbackMuzzleDeviceUsesUpAxis
            // The cylinder is pitched +90 degrees so its local +Z points
            // toward the stock; negate it to reach the visible muzzle end.
            ? -MuzzleDeviceMesh->GetUpVector()
            : MuzzleDeviceMesh->GetForwardVector();
        return MuzzleDeviceMesh->GetComponentLocation() + MuzzleAxis * 6.0f;
    }
    if (BarrelMesh)
    {
        // The fallback barrel is a 32 cm cylinder aligned along local X; put
        // the visual muzzle just beyond its front face while the authoritative
        // trace still originates at the camera for predictable aim/validation.
        const FVector MuzzleAxis = bFallbackBarrelUsesUpAxis ? -BarrelMesh->GetUpVector() : BarrelMesh->GetForwardVector();
        return BarrelMesh->GetComponentLocation() + MuzzleAxis * 16.0f;
    }
    return GetActorLocation() + GetActorForwardVector() * 45.0f;
}

FVector ABMWeapon::GetGripLocation() const
{
    const FName GripSocketName = WeaponData && !WeaponData->GripSocketName.IsNone()
        ? WeaponData->GripSocketName
        : FName(TEXT("Grip"));

    if (WeaponMesh
        && WeaponMesh->GetStaticMesh()
        && WeaponMesh->DoesSocketExist(GripSocketName))
    {
        return WeaponMesh->GetSocketLocation(GripSocketName);
    }

    // The procedural first-person kit has a separate grip component. Keep it
    // as the compatibility path for source-only/runtime fallback weapons;
    // authored project meshes always resolve through the socket above.
    if (GripMesh)
    {
        return GripMesh->GetComponentLocation();
    }

    return GetActorLocation();
}

void ABMWeapon::UpdatePresentation(const float DeltaSeconds)
{
    if (!WeaponData || !FireModeComponent || !GetWorld())
    {
        return;
    }

    FireModeComponent->GetAccuracyPenalty(
        GetWorld()->GetTimeSeconds(),
        WeaponData->Balance.AccuracyRecoveryDegreesPerSecond);

    const bool bReloadingNow = IsReloading();
    if (bReloadingNow)
    {
        if (ReloadPresentationDuration <= KINDA_SMALL_NUMBER)
        {
            BeginReloadPresentation(AmmunitionComponent && AmmunitionComponent->IsEmpty());
        }
        ReloadPresentationElapsed = FMath::Min(
            ReloadPresentationDuration,
            ReloadPresentationElapsed + FMath::Max(0.0f, DeltaSeconds));
    }
    else if (bPresentationWasReloading)
    {
        ReloadPresentationElapsed = ReloadPresentationDuration;
    }
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("BM_ReloadProbe")) && bReloadingNow != bPresentationWasReloading)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian combat probe: reload presentation state owner=%s authority=%s state=%s alpha=%.2f"),
            *GetNameSafe(GetOwner()),
            HasAuthority() ? TEXT("yes") : TEXT("no"),
            bReloadingNow ? TEXT("reloading") : TEXT("ready"),
            PresentationReloadAlpha);
    }
#endif
    if (bReloadingNow && !bPresentationWasReloading)
    {
        bPresentationReloadReturning = false;
    }
    else if (!bReloadingNow && bPresentationWasReloading)
    {
        // Keep the magazine in the dropped position while the authority
        // completes the final insertion phase, then return it smoothly.
        bPresentationReloadReturning = true;
    }
    bPresentationWasReloading = bReloadingNow;

    const float TargetReloadAlpha = bReloadingNow ? 1.0f : 0.0f;
    const float ReloadPresentationSpeed = bPresentationReloadReturning ? 28.0f : 18.0f;
    PresentationReloadAlpha = FMath::FInterpTo(
        PresentationReloadAlpha,
        TargetReloadAlpha,
        DeltaSeconds,
        ReloadPresentationSpeed);
    PresentationShotPulse = FMath::FInterpTo(PresentationShotPulse, 0.0f, DeltaSeconds, 24.0f);
    if (bPresentationReloadReturning && PresentationReloadAlpha <= 0.015f)
    {
        bPresentationReloadReturning = false;
    }
}

void ABMWeapon::BeginReloadPresentation(const bool bEmptyReload)
{
    if (!WeaponData)
    {
        ReloadPresentationDuration = 0.0f;
        ReloadPresentationElapsed = 0.0f;
        return;
    }

    ReloadPresentationDuration = FMath::Max(
        0.05f,
        bEmptyReload ? WeaponData->Balance.EmptyReloadDuration : WeaponData->Balance.ReloadDuration);
    ReloadPresentationElapsed = 0.0f;
}

FRotator ABMWeapon::ConsumeVisualRecoil(const float DeltaSeconds)
{
    return RecoilComponent ? RecoilComponent->ConsumeVisualKick(DeltaSeconds, 11.0f) : FRotator::ZeroRotator;
}

float ABMWeapon::GetAccuracyPenaltyDegrees() const
{
    return FireModeComponent ? FireModeComponent->GetCurrentAccuracyPenalty() : 0.0f;
}

void ABMWeapon::SetFirstPersonPresentation(const FVector& LocationOffset, const FRotator& RotationOffset)
{
    if (!RootComponent)
    {
        return;
    }

    // Move the actor root so receiver, barrel, magazine, grip, optic, and
    // fallback effects stay rigidly together. The old implementation moved
    // only the receiver, which made recoil and sway visibly break the weapon
    // silhouette apart.
    const float ReloadCurve = FMath::Sin(PI * FMath::Clamp(PresentationReloadAlpha, 0.0f, 1.0f));
    const FVector ReloadLocation(-10.0f * ReloadCurve, 0.0f, -24.0f * ReloadCurve);
    const FRotator ReloadRotation(-14.0f * ReloadCurve, 0.0f, 5.0f * ReloadCurve);
    RootComponent->SetRelativeLocation(LocationOffset + ReloadLocation);
    RootComponent->SetRelativeRotation(RotationOffset + ReloadRotation);

    const auto SmoothStep01 = [](float Value)
    {
        Value = FMath::Clamp(Value, 0.0f, 1.0f);
        return Value * Value * (3.0f - 2.0f * Value);
    };

    // Treat reload as a short sequence even in the source-only rig: drop the
    // magazine, hold it clear while the weapon is low, then seat it during the
    // completion blend. Authored reload montages can replace this layer.
    const float ReloadProgress = bPresentationReloadReturning
        ? (1.0f - FMath::Clamp(PresentationReloadAlpha, 0.0f, 1.0f))
        : FMath::Clamp(PresentationReloadAlpha, 0.0f, 1.0f);
    const float MagazineDrop = bPresentationReloadReturning
        ? (1.0f - SmoothStep01((ReloadProgress - 0.42f) / 0.42f))
        : SmoothStep01((ReloadProgress - 0.05f) / 0.4f);
    if (MagazineMesh)
    {
        MagazineMesh->SetRelativeLocation(
            PresentationMagazineBaseLocation + FVector(-4.0f, 1.5f, -18.0f) * MagazineDrop);
        MagazineMesh->SetRelativeRotation(
            PresentationMagazineBaseRotation + FRotator(10.0f, 0.0f, -7.0f) * MagazineDrop);
    }

    if (FirstPersonMagazineGeometryMesh)
    {
        FirstPersonMagazineGeometryMesh->SetRelativeLocation(
            PresentationFirstPersonMagazineBaseLocation + FVector(-4.0f, 1.5f, -18.0f) * MagazineDrop);
        FirstPersonMagazineGeometryMesh->SetRelativeRotation(
            PresentationFirstPersonMagazineBaseRotation + FRotator(10.0f, 0.0f, -7.0f) * MagazineDrop);
    }

    if (ChargingHandleMesh)
    {
        const float ReloadHandleOffset = ReloadCurve * 4.0f;
        const float ShotHandleOffset = FMath::Clamp(PresentationShotPulse, 0.0f, 1.0f) * 9.0f;
        ChargingHandleMesh->SetRelativeLocation(
            PresentationChargingHandleBaseLocation + FVector(-ReloadHandleOffset - ShotHandleOffset, 0.0f, 0.0f));
        ChargingHandleMesh->SetRelativeRotation(
            PresentationChargingHandleBaseRotation + FRotator(0.0f, 0.0f, ReloadCurve * 5.0f));
    }
}

void ABMWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABMWeapon, ReplicatedWeaponId);
    DOREPLIFETIME(ABMWeapon, bReplicatedSuppressed);
    DOREPLIFETIME(ABMWeapon, ReplicatedAttachmentState);
}
