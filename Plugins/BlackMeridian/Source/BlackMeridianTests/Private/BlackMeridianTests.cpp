// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"
#include "GameFramework/InputSettings.h"
#include "InputCoreTypes.h"

#include "BMCheckpointSaveGame.h"
#include "BMCharacterAnimationDataAsset.h"
#include "BMCampaignLevel.h"
#include "BMCombatTuning.h"
#include "BMCoreTypes.h"
#include "BMAudioDataAsset.h"
#include "BMArtDirectedProp.h"
#include "BMEnemyAIController.h"
#include "BMEnemyCharacter.h"
#include "BMGameUserSettings.h"
#include "BMGameState.h"
#include "BMHUD.h"
#include "BMInputRemap.h"
#include "BMInteractableActor.h"
#include "BMMissionDirector.h"
#include "BMPlayerProfileSaveGame.h"
#include "BMProjectile.h"
#include "BMThrowableDataAsset.h"
#include "BMVFXDataAsset.h"
#include "BMWeapon.h"
#include "BMWeaponComponents.h"
#include "BMWeaponDataAsset.h"
#include "BMGreyboxDirector.h"
#include "Perception/AIPerceptionComponent.h"
#include "Components/BoxComponent.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimSequence.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/Texture2D.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshOperations.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "StaticMeshAttributes.h"
#include "Engine/Texture.h"
#include "UObject/SavePackage.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Factories/SoundFactory.h"
#include "HAL/FileManager.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#endif

#if WITH_EDITOR
namespace
{
    enum class EBlackMeridianWeaponMeshKind : uint8
    {
        ARX4,
        Kestrel9,
        RookDMR,
        Breach12,
        SolacePistol,
        ArcLauncher
    };

    struct FBlackMeridianWeaponMeshSpec
    {
        const TCHAR* AssetName;
        EBlackMeridianWeaponMeshKind Kind;
        EBMWeaponId WeaponId;
    };

    enum class EBlackMeridianThrowableMeshKind : uint8
    {
        Frag,
        Veil
    };

    struct FBlackMeridianThrowableMeshSpec
    {
        const TCHAR* AssetName;
        EBlackMeridianThrowableMeshKind Kind;
        EBMWeaponId ThrowableId;
    };

    enum class EBlackMeridianEnvironmentMeshKind : uint8
    {
        Block,
        Pipe,
        Orb,
        Deck,
        Truss,
        Grate,
        Panel,
        Cover,
        Door,
        Window,
        Sign,
        Machinery,
        Wreckage
    };

    struct FBlackMeridianEnvironmentMeshSpec
    {
        const TCHAR* AssetName;
        EBlackMeridianEnvironmentMeshKind Kind;
    };

    float GetWeaponMuzzleX(const EBlackMeridianWeaponMeshKind Kind)
    {
        switch (Kind)
        {
        case EBlackMeridianWeaponMeshKind::Kestrel9:
            return 60.0f;
        case EBlackMeridianWeaponMeshKind::RookDMR:
            return 101.0f;
        case EBlackMeridianWeaponMeshKind::Breach12:
            return 90.0f;
        case EBlackMeridianWeaponMeshKind::SolacePistol:
            return 48.0f;
        case EBlackMeridianWeaponMeshKind::ArcLauncher:
            return 96.0f;
        case EBlackMeridianWeaponMeshKind::ARX4:
        default:
            return 67.0f;
        }
    }

    bool EnsureWeaponSocket(
        UStaticMesh* Mesh,
        const FName SocketName,
        const FVector& RelativeLocation)
    {
        if (!Mesh)
        {
            return false;
        }

        UStaticMeshSocket* Socket = Mesh->FindSocket(SocketName);
        bool bCreated = false;
        if (!Socket)
        {
            Socket = NewObject<UStaticMeshSocket>(Mesh, NAME_None, RF_Public | RF_Standalone);
            if (!Socket)
            {
                return false;
            }
            Socket->SocketName = SocketName;
            Mesh->AddSocket(Socket);
            bCreated = true;
        }

        const bool bChanged = Socket->RelativeLocation != RelativeLocation
            || !Socket->RelativeRotation.IsNearlyZero()
            || Socket->RelativeScale != FVector::OneVector;
        Socket->RelativeLocation = RelativeLocation;
        Socket->RelativeRotation = FRotator::ZeroRotator;
        Socket->RelativeScale = FVector::OneVector;
        return bCreated || bChanged;
    }

    bool EnsureWeaponSockets(
        UStaticMesh* Mesh,
        const FBlackMeridianWeaponMeshSpec& Spec)
    {
        if (!Mesh)
        {
            return false;
        }

        const bool bHadGripSocket = Mesh->FindSocket(TEXT("Grip")) != nullptr;
        const bool bHadMuzzleSocket = Mesh->FindSocket(TEXT("Muzzle")) != nullptr;
        const bool bGripChanged = EnsureWeaponSocket(Mesh, TEXT("Grip"), FVector::ZeroVector);
        const bool bMuzzleChanged = EnsureWeaponSocket(
            Mesh,
            TEXT("Muzzle"),
            FVector(GetWeaponMuzzleX(Spec.Kind), 0.0f, 0.0f));
        return bGripChanged || bMuzzleChanged || !bHadGripSocket || !bHadMuzzleSocket;
    }

    void AddWeaponBox(
        FMeshDescriptionBuilder& Builder,
        const FPolygonGroupID PolygonGroup,
        const FVector& Min,
        const FVector& Max)
    {
        const FVector Points[8] = {
            FVector(Min.X, Min.Y, Min.Z),
            FVector(Max.X, Min.Y, Min.Z),
            FVector(Max.X, Max.Y, Min.Z),
            FVector(Min.X, Max.Y, Min.Z),
            FVector(Min.X, Min.Y, Max.Z),
            FVector(Max.X, Min.Y, Max.Z),
            FVector(Max.X, Max.Y, Max.Z),
            FVector(Min.X, Max.Y, Max.Z)};

        const int32 Faces[6][4] = {
            {0, 3, 2, 1}, // -Z
            {4, 5, 6, 7}, // +Z
            {0, 1, 5, 4}, // -Y
            {3, 2, 6, 7}, // +Y
            {0, 4, 7, 3}, // -X
            {1, 2, 6, 5}}; // +X

        for (const int32 (&Face)[4] : Faces)
        {
            const FVertexID A = Builder.AppendVertex(Points[Face[0]]);
            const FVertexID B = Builder.AppendVertex(Points[Face[1]]);
            const FVertexID C = Builder.AppendVertex(Points[Face[2]]);
            const FVertexID D = Builder.AppendVertex(Points[Face[3]]);
            Builder.AppendTriangle(A, B, C, PolygonGroup);
            Builder.AppendTriangle(A, C, D, PolygonGroup);
        }
    }

    void AddEnvironmentChamferedPrismZ(
        FMeshDescriptionBuilder& Builder,
        const FPolygonGroupID PolygonGroup,
        const float HalfX,
        const float HalfY,
        const float HalfZ,
        const float Bevel)
    {
        // Eight-sided cross-section gives the modular block a real edge break
        // while keeping its bounds compatible with the engine cube replacement.
        const FVector2D CrossSection[8] = {
            FVector2D(-HalfX + Bevel, -HalfY),
            FVector2D(HalfX - Bevel, -HalfY),
            FVector2D(HalfX, -HalfY + Bevel),
            FVector2D(HalfX, HalfY - Bevel),
            FVector2D(HalfX - Bevel, HalfY),
            FVector2D(-HalfX + Bevel, HalfY),
            FVector2D(-HalfX, HalfY - Bevel),
            FVector2D(-HalfX, -HalfY + Bevel)};

        TArray<FVertexID> BottomVertices;
        TArray<FVertexID> TopVertices;
        BottomVertices.Reserve(UE_ARRAY_COUNT(CrossSection));
        TopVertices.Reserve(UE_ARRAY_COUNT(CrossSection));
        for (const FVector2D& Point : CrossSection)
        {
            BottomVertices.Add(Builder.AppendVertex(FVector(Point.X, Point.Y, -HalfZ)));
            TopVertices.Add(Builder.AppendVertex(FVector(Point.X, Point.Y, HalfZ)));
        }

        for (int32 Segment = 0; Segment < UE_ARRAY_COUNT(CrossSection); ++Segment)
        {
            const int32 Next = (Segment + 1) % UE_ARRAY_COUNT(CrossSection);
            Builder.AppendTriangle(BottomVertices[Segment], BottomVertices[Next], TopVertices[Next], PolygonGroup);
            Builder.AppendTriangle(BottomVertices[Segment], TopVertices[Next], TopVertices[Segment], PolygonGroup);
        }

        const FVertexID BottomCenter = Builder.AppendVertex(FVector(0.0f, 0.0f, -HalfZ));
        const FVertexID TopCenter = Builder.AppendVertex(FVector(0.0f, 0.0f, HalfZ));
        for (int32 Segment = 0; Segment < UE_ARRAY_COUNT(CrossSection); ++Segment)
        {
            const int32 Next = (Segment + 1) % UE_ARRAY_COUNT(CrossSection);
            Builder.AppendTriangle(BottomCenter, BottomVertices[Next], BottomVertices[Segment], PolygonGroup);
            Builder.AppendTriangle(TopCenter, TopVertices[Segment], TopVertices[Next], PolygonGroup);
        }
    }

    void AddEnvironmentCylinderZ(
        FMeshDescriptionBuilder& Builder,
        const FPolygonGroupID PolygonGroup,
        const float StartZ,
        const float EndZ,
        const float Radius,
        const int32 SegmentCount = 12)
    {
        TArray<FVertexID> StartVertices;
        TArray<FVertexID> EndVertices;
        StartVertices.Reserve(SegmentCount);
        EndVertices.Reserve(SegmentCount);
        for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
        {
            const float Angle = 2.0f * PI * static_cast<float>(Segment) / static_cast<float>(SegmentCount);
            const FVector RingOffset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
            StartVertices.Add(Builder.AppendVertex(FVector(0.0f, 0.0f, StartZ) + RingOffset));
            EndVertices.Add(Builder.AppendVertex(FVector(0.0f, 0.0f, EndZ) + RingOffset));
        }

        for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
        {
            const int32 Next = (Segment + 1) % SegmentCount;
            Builder.AppendTriangle(StartVertices[Segment], StartVertices[Next], EndVertices[Next], PolygonGroup);
            Builder.AppendTriangle(StartVertices[Segment], EndVertices[Next], EndVertices[Segment], PolygonGroup);
        }

        const FVertexID StartCenter = Builder.AppendVertex(FVector::UpVector * StartZ);
        const FVertexID EndCenter = Builder.AppendVertex(FVector::UpVector * EndZ);
        for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
        {
            const int32 Next = (Segment + 1) % SegmentCount;
            Builder.AppendTriangle(StartCenter, StartVertices[Next], StartVertices[Segment], PolygonGroup);
            Builder.AppendTriangle(EndCenter, EndVertices[Segment], EndVertices[Next], PolygonGroup);
        }
    }

    void AddEnvironmentOrb(
        FMeshDescriptionBuilder& Builder,
        const FPolygonGroupID PolygonGroup,
        const float Radius,
        const int32 RingCount = 6,
        const int32 SegmentCount = 12)
    {
        TArray<TArray<FVertexID>> Rings;
        Rings.Reserve(RingCount - 1);
        for (int32 Ring = 1; Ring < RingCount; ++Ring)
        {
            const float Latitude = -0.5f * PI + PI * static_cast<float>(Ring) / static_cast<float>(RingCount);
            const float RingRadius = Radius * FMath::Cos(Latitude);
            const float Z = Radius * FMath::Sin(Latitude);
            TArray<FVertexID>& Vertices = Rings.AddDefaulted_GetRef();
            Vertices.Reserve(SegmentCount);
            for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
            {
                const float Angle = 2.0f * PI * static_cast<float>(Segment) / static_cast<float>(SegmentCount);
                Vertices.Add(Builder.AppendVertex(FVector(
                    FMath::Cos(Angle) * RingRadius,
                    FMath::Sin(Angle) * RingRadius,
                    Z)));
            }
        }

        const FVertexID Bottom = Builder.AppendVertex(FVector(0.0f, 0.0f, -Radius));
        const FVertexID Top = Builder.AppendVertex(FVector(0.0f, 0.0f, Radius));
        for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
        {
            const int32 Next = (Segment + 1) % SegmentCount;
            Builder.AppendTriangle(Bottom, Rings[0][Next], Rings[0][Segment], PolygonGroup);
            Builder.AppendTriangle(Rings.Last()[Segment], Rings.Last()[Next], Top, PolygonGroup);
        }

        for (int32 Ring = 0; Ring + 1 < Rings.Num(); ++Ring)
        {
            for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
            {
                const int32 Next = (Segment + 1) % SegmentCount;
                Builder.AppendTriangle(Rings[Ring][Segment], Rings[Ring][Next], Rings[Ring + 1][Next], PolygonGroup);
                Builder.AppendTriangle(Rings[Ring][Segment], Rings[Ring + 1][Next], Rings[Ring + 1][Segment], PolygonGroup);
            }
        }
    }

    void AddEnvironmentPanel(
        FMeshDescriptionBuilder& Builder,
        const FPolygonGroupID PolygonGroup)
    {
        // A shallow, chamfered wall module with raised service ribs on both
        // faces.  The panel is intentionally dimension-compatible with the
        // old cube wall, but its edge breaks and inset bands catch runtime
        // light as a real modular facility surface.
        AddEnvironmentChamferedPrismZ(Builder, PolygonGroup, 50.0f, 8.0f, 50.0f, 6.0f);
        for (const float Z : {-34.0f, 0.0f, 34.0f})
        {
            AddWeaponBox(
                Builder,
                PolygonGroup,
                FVector(-42.0f, -12.0f, Z - 2.0f),
                FVector(42.0f, -8.0f, Z + 2.0f));
            AddWeaponBox(
                Builder,
                PolygonGroup,
                FVector(-42.0f, 8.0f, Z - 2.0f),
                FVector(42.0f, 12.0f, Z + 2.0f));
        }
        AddWeaponBox(
            Builder,
            PolygonGroup,
            FVector(-4.0f, -12.0f, -34.0f),
            FVector(4.0f, -8.0f, 34.0f));
        AddWeaponBox(
            Builder,
            PolygonGroup,
            FVector(-4.0f, 8.0f, -34.0f),
            FVector(4.0f, 12.0f, 34.0f));
    }

    void AddEnvironmentCover(
        FMeshDescriptionBuilder& Builder,
        const FPolygonGroupID PolygonGroup)
    {
        // Low, chamfered cover with an impact lip and two embedded feet. It
        // gives barricades a recognizable silhouette while retaining a
        // simple convex collision profile for the runtime compatibility path.
        AddEnvironmentChamferedPrismZ(Builder, PolygonGroup, 58.0f, 18.0f, 34.0f, 6.0f);
        AddWeaponBox(
            Builder,
            PolygonGroup,
            FVector(-52.0f, -23.0f, 22.0f),
            FVector(52.0f, -18.0f, 31.0f));
        AddWeaponBox(
            Builder,
            PolygonGroup,
            FVector(-52.0f, 18.0f, 22.0f),
            FVector(52.0f, 23.0f, 31.0f));
        AddWeaponBox(
            Builder,
            PolygonGroup,
            FVector(-46.0f, -22.0f, -38.0f),
            FVector(-34.0f, 22.0f, -31.0f));
        AddWeaponBox(
            Builder,
            PolygonGroup,
            FVector(34.0f, -22.0f, -38.0f),
            FVector(46.0f, 22.0f, -31.0f));
    }

    void AddEnvironmentDoor(
        FMeshDescriptionBuilder& Builder,
        const FPolygonGroupID PolygonGroup)
    {
        // A compact service door with a real frame, recessed leaf, handle
        // rail, and threshold. It is intentionally one low-cost mesh so a
        // facade can repeat it without spawning a dozen primitive actors.
        AddWeaponBox(Builder, PolygonGroup, FVector(-56.0f, -9.0f, -62.0f), FVector(-44.0f, 9.0f, 62.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(44.0f, -9.0f, -62.0f), FVector(56.0f, 9.0f, 62.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-56.0f, -9.0f, 50.0f), FVector(56.0f, 9.0f, 62.0f));
        AddEnvironmentChamferedPrismZ(Builder, PolygonGroup, 39.0f, 5.0f, 49.0f, 4.0f);
        AddWeaponBox(Builder, PolygonGroup, FVector(28.0f, -8.0f, -8.0f), FVector(34.0f, 8.0f, 20.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-42.0f, -11.0f, -70.0f), FVector(42.0f, 11.0f, -62.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-31.0f, -8.0f, 28.0f), FVector(31.0f, -5.0f, 35.0f));
    }

    void AddEnvironmentWindow(
        FMeshDescriptionBuilder& Builder,
        const FPolygonGroupID PolygonGroup)
    {
        // A thick observation window: raised perimeter, four mullions, and a
        // recessed pane give the facade a readable interior/exterior rhythm.
        AddWeaponBox(Builder, PolygonGroup, FVector(-58.0f, -8.0f, -43.0f), FVector(-48.0f, 8.0f, 43.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(48.0f, -8.0f, -43.0f), FVector(58.0f, 8.0f, 43.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-48.0f, -8.0f, 33.0f), FVector(48.0f, 8.0f, 43.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-48.0f, -8.0f, -43.0f), FVector(48.0f, 8.0f, -33.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-5.0f, -8.0f, -33.0f), FVector(5.0f, 8.0f, 33.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-48.0f, -8.0f, -5.0f), FVector(48.0f, 8.0f, 5.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-39.0f, -4.0f, -26.0f), FVector(39.0f, 4.0f, 26.0f));
    }

    void AddEnvironmentSign(
        FMeshDescriptionBuilder& Builder,
        const FPolygonGroupID PolygonGroup)
    {
        // Directional facility signage: a shallow board, inset face, twin
        // support arms, and an elevated beacon cap.
        AddEnvironmentChamferedPrismZ(Builder, PolygonGroup, 76.0f, 7.0f, 34.0f, 5.0f);
        AddWeaponBox(Builder, PolygonGroup, FVector(-62.0f, -11.0f, -20.0f), FVector(62.0f, -7.0f, 20.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-58.0f, 7.0f, -22.0f), FVector(-49.0f, 15.0f, 72.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(49.0f, 7.0f, -22.0f), FVector(58.0f, 15.0f, 72.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-12.0f, -12.0f, 28.0f), FVector(12.0f, 12.0f, 52.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-88.0f, -3.0f, -5.0f), FVector(-80.0f, 3.0f, 5.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(80.0f, -3.0f, -5.0f), FVector(88.0f, 3.0f, 5.0f));
    }

    void AddEnvironmentMachinery(
        FMeshDescriptionBuilder& Builder,
        const FPolygonGroupID PolygonGroup)
    {
        // A low-poly relay machine built as one optimized silhouette: tank,
        // service console, cable collars, and a top signal lamp.
        AddEnvironmentChamferedPrismZ(Builder, PolygonGroup, 54.0f, 42.0f, 54.0f, 8.0f);
        AddEnvironmentCylinderZ(Builder, PolygonGroup, 42.0f, 142.0f, 38.0f, 12);
        AddEnvironmentCylinderZ(Builder, PolygonGroup, 136.0f, 148.0f, 47.0f, 12);
        AddWeaponBox(Builder, PolygonGroup, FVector(-62.0f, -48.0f, 30.0f), FVector(62.0f, -34.0f, 82.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-48.0f, -55.0f, 44.0f), FVector(48.0f, -49.0f, 66.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-7.0f, -62.0f, 44.0f), FVector(7.0f, -56.0f, 82.0f));
        AddEnvironmentCylinderZ(Builder, PolygonGroup, 148.0f, 186.0f, 9.0f, 10);
        AddWeaponBox(Builder, PolygonGroup, FVector(-18.0f, -18.0f, 168.0f), FVector(18.0f, 18.0f, 204.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-62.0f, 26.0f, 20.0f), FVector(-50.0f, 42.0f, 106.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(50.0f, 26.0f, 20.0f), FVector(62.0f, 42.0f, 106.0f));
    }

    void AddEnvironmentWreckage(
        FMeshDescriptionBuilder& Builder,
        const FPolygonGroupID PolygonGroup)
    {
        // A single optimized collapsed-hull silhouette for exposed combat
        // lanes. It is intentionally asymmetric: a low armored keel, split
        // side rails, a broken cab, and four exposed roof ribs read as a
        // disabled vehicle or radar chassis without spawning a dozen cubes.
        // The simple collision body generated from this mesh remains useful
        // as cover while the open upper frame preserves sightlines.
        AddEnvironmentChamferedPrismZ(Builder, PolygonGroup, 150.0f, 78.0f, 24.0f, 14.0f);
        AddWeaponBox(Builder, PolygonGroup, FVector(-128.0f, -56.0f, -36.0f), FVector(118.0f, 56.0f, -24.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-126.0f, -72.0f, 12.0f), FVector(116.0f, -56.0f, 29.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-126.0f, 56.0f, 12.0f), FVector(116.0f, 72.0f, 29.0f));

        // Collapsed cab and a sheared rear plate break the otherwise regular
        // industrial language and give the asset a recognizable wreck pose.
        AddEnvironmentChamferedPrismZ(Builder, PolygonGroup, 52.0f, 54.0f, 42.0f, 10.0f);
        AddWeaponBox(Builder, PolygonGroup, FVector(68.0f, -52.0f, 20.0f), FVector(136.0f, 52.0f, 70.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-132.0f, -44.0f, 24.0f), FVector(-112.0f, 44.0f, 116.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(110.0f, -42.0f, 58.0f), FVector(132.0f, 42.0f, 96.0f));

        // Exposed transverse ribs form readable negative space and catch
        // practical lights, while the two raised side braces imply a torn
        // frame rather than a clean primitive block.
        for (const float X : {-82.0f, -24.0f, 34.0f, 92.0f})
        {
            AddWeaponBox(Builder, PolygonGroup, FVector(X - 6.0f, -54.0f, 24.0f), FVector(X + 6.0f, 54.0f, 34.0f));
        }
        AddWeaponBox(Builder, PolygonGroup, FVector(-88.0f, -56.0f, 26.0f), FVector(-72.0f, -42.0f, 88.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(42.0f, 42.0f, 26.0f), FVector(58.0f, 56.0f, 76.0f));
    }

    void BuildEnvironmentMeshDescription(
        FMeshDescriptionBuilder& Builder,
        const FPolygonGroupID PolygonGroup,
        const EBlackMeridianEnvironmentMeshKind Kind)
    {
        switch (Kind)
        {
        case EBlackMeridianEnvironmentMeshKind::Block:
            AddEnvironmentChamferedPrismZ(Builder, PolygonGroup, 46.0f, 46.0f, 46.0f, 8.0f);
            // Raised service ribs catch the runtime light and give walls and
            // cover a repeatable industrial language at every scale.
            AddWeaponBox(Builder, PolygonGroup, FVector(-36.0f, -36.0f, 46.0f), FVector(36.0f, -32.0f, 50.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(-36.0f, 32.0f, 46.0f), FVector(36.0f, 36.0f, 50.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(-36.0f, -32.0f, 46.0f), FVector(-32.0f, 32.0f, 50.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(32.0f, -32.0f, 46.0f), FVector(36.0f, 32.0f, 50.0f));
            break;

        case EBlackMeridianEnvironmentMeshKind::Pipe:
            AddEnvironmentCylinderZ(Builder, PolygonGroup, -42.0f, 42.0f, 42.0f);
            AddEnvironmentCylinderZ(Builder, PolygonGroup, -50.0f, -40.0f, 49.0f);
            AddEnvironmentCylinderZ(Builder, PolygonGroup, 40.0f, 50.0f, 49.0f);
            AddWeaponBox(Builder, PolygonGroup, FVector(-3.0f, 40.0f, -36.0f), FVector(3.0f, 45.0f, 36.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(-3.0f, -45.0f, -36.0f), FVector(3.0f, -40.0f, 36.0f));
            break;

        case EBlackMeridianEnvironmentMeshKind::Orb:
            AddEnvironmentOrb(Builder, PolygonGroup, 48.0f);
            AddEnvironmentCylinderZ(Builder, PolygonGroup, -3.0f, 3.0f, 50.0f);
            break;

        case EBlackMeridianEnvironmentMeshKind::Deck:
            AddWeaponBox(Builder, PolygonGroup, FVector(-50.0f, -50.0f, -50.0f), FVector(50.0f, 50.0f, 50.0f));
            for (const float Offset : {-35.0f, 0.0f, 35.0f})
            {
                AddWeaponBox(Builder, PolygonGroup, FVector(Offset - 2.0f, -46.0f, 50.0f), FVector(Offset + 2.0f, 46.0f, 53.0f));
                AddWeaponBox(Builder, PolygonGroup, FVector(-46.0f, Offset - 2.0f, 50.0f), FVector(46.0f, Offset + 2.0f, 53.0f));
            }
            break;

        case EBlackMeridianEnvironmentMeshKind::Truss:
            // A compact service gantry: paired rails, end posts, and a
            // cross-member. It is intentionally modular so authored maps can
            // repeat it as an overhead silhouette without a heavy asset.
            AddWeaponBox(Builder, PolygonGroup, FVector(-96.0f, -6.0f, -50.0f), FVector(96.0f, 6.0f, -42.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(-96.0f, -6.0f, 42.0f), FVector(96.0f, 6.0f, 50.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(-90.0f, -6.0f, -42.0f), FVector(-82.0f, 6.0f, 42.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(82.0f, -6.0f, -42.0f), FVector(90.0f, 6.0f, 42.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(-6.0f, -42.0f, -6.0f), FVector(6.0f, 42.0f, 6.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(-88.0f, -13.0f, -5.0f), FVector(-74.0f, 13.0f, 5.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(74.0f, -13.0f, -5.0f), FVector(88.0f, 13.0f, 5.0f));
            break;

        case EBlackMeridianEnvironmentMeshKind::Grate:
            // Thin raised deck with a perimeter frame and regular slats. The
            // collision is real, so the asset can serve as a small platform
            // or bridge rather than being a purely decorative decal.
            AddWeaponBox(Builder, PolygonGroup, FVector(-104.0f, -64.0f, -6.0f), FVector(104.0f, -56.0f, 2.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(-104.0f, 56.0f, -6.0f), FVector(104.0f, 64.0f, 2.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(-104.0f, -56.0f, -6.0f), FVector(-96.0f, 56.0f, 2.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(96.0f, -56.0f, -6.0f), FVector(104.0f, 56.0f, 2.0f));
            for (const float Offset : {-80.0f, -60.0f, -40.0f, -20.0f, 0.0f, 20.0f, 40.0f, 60.0f, 80.0f})
            {
                AddWeaponBox(Builder, PolygonGroup, FVector(Offset - 2.0f, -52.0f, 2.0f), FVector(Offset + 2.0f, 52.0f, 5.0f));
            }
            AddWeaponBox(Builder, PolygonGroup, FVector(-96.0f, -4.0f, 2.0f), FVector(96.0f, 4.0f, 5.0f));
            break;

        case EBlackMeridianEnvironmentMeshKind::Panel:
            AddEnvironmentPanel(Builder, PolygonGroup);
            break;

        case EBlackMeridianEnvironmentMeshKind::Cover:
            AddEnvironmentCover(Builder, PolygonGroup);
            break;

        case EBlackMeridianEnvironmentMeshKind::Door:
            AddEnvironmentDoor(Builder, PolygonGroup);
            break;

        case EBlackMeridianEnvironmentMeshKind::Window:
            AddEnvironmentWindow(Builder, PolygonGroup);
            break;

        case EBlackMeridianEnvironmentMeshKind::Sign:
            AddEnvironmentSign(Builder, PolygonGroup);
            break;

        case EBlackMeridianEnvironmentMeshKind::Machinery:
            AddEnvironmentMachinery(Builder, PolygonGroup);
            break;

        case EBlackMeridianEnvironmentMeshKind::Wreckage:
            AddEnvironmentWreckage(Builder, PolygonGroup);
            break;
        }
    }

    void ApplyEnvironmentUVs(
        FMeshDescriptionBuilder& Builder,
        FMeshDescription& MeshDescription)
    {
        // The runtime material is mostly parameter-driven today, but authored
        // UVs keep the generated assets valid for future grunge/normal maps
        // and prevent MikkTSpace from receiving an all-zero tangent basis.
        // A stable tri-planar-style projection is sufficient for this compact
        // modular kit and avoids storing a second hand-authored UV asset.
        Builder.SetNumUVLayers(1);
        for (const FVertexInstanceID InstanceID : MeshDescription.VertexInstances().GetElementIDs())
        {
            const FVector3f Position = MeshDescription.GetVertexPosition(
                MeshDescription.GetVertexInstanceVertex(InstanceID));
            const FVector2D UV(
                Position.X * 0.011f + Position.Z * 0.0037f + 0.5f,
                Position.Y * 0.011f + Position.Z * 0.0061f + 0.5f);
            Builder.SetInstanceUV(InstanceID, UV, 0);
        }
    }

    void AddWeaponCylinderX(
        FMeshDescriptionBuilder& Builder,
        const FPolygonGroupID PolygonGroup,
        const float StartX,
        const float EndX,
        const float Radius,
        const int32 SegmentCount = 8)
    {
        TArray<FVertexID> StartVertices;
        TArray<FVertexID> EndVertices;
        StartVertices.Reserve(SegmentCount);
        EndVertices.Reserve(SegmentCount);
        for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
        {
            const float Angle = 2.0f * PI * static_cast<float>(Segment) / static_cast<float>(SegmentCount);
            const FVector RingOffset(0.0f, FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);
            StartVertices.Add(Builder.AppendVertex(FVector(StartX, 0.0f, 0.0f) + RingOffset));
            EndVertices.Add(Builder.AppendVertex(FVector(EndX, 0.0f, 0.0f) + RingOffset));
        }

        for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
        {
            const int32 Next = (Segment + 1) % SegmentCount;
            Builder.AppendTriangle(StartVertices[Segment], StartVertices[Next], EndVertices[Next], PolygonGroup);
            Builder.AppendTriangle(StartVertices[Segment], EndVertices[Next], EndVertices[Segment], PolygonGroup);
        }

        const FVertexID StartCenter = Builder.AppendVertex(FVector(StartX, 0.0f, 0.0f));
        const FVertexID EndCenter = Builder.AppendVertex(FVector(EndX, 0.0f, 0.0f));
        for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
        {
            const int32 Next = (Segment + 1) % SegmentCount;
            Builder.AppendTriangle(StartCenter, StartVertices[Next], StartVertices[Segment], PolygonGroup);
            Builder.AppendTriangle(EndCenter, EndVertices[Segment], EndVertices[Next], PolygonGroup);
        }
    }

    void AddThrowableCylinderZ(
        FMeshDescriptionBuilder& Builder,
        const FPolygonGroupID PolygonGroup,
        const float StartZ,
        const float EndZ,
        const float Radius,
        const int32 SegmentCount = 10)
    {
        TArray<FVertexID> StartVertices;
        TArray<FVertexID> EndVertices;
        StartVertices.Reserve(SegmentCount);
        EndVertices.Reserve(SegmentCount);
        for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
        {
            const float Angle = 2.0f * PI * static_cast<float>(Segment) / static_cast<float>(SegmentCount);
            const FVector RingOffset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
            StartVertices.Add(Builder.AppendVertex(FVector(0.0f, 0.0f, StartZ) + RingOffset));
            EndVertices.Add(Builder.AppendVertex(FVector(0.0f, 0.0f, EndZ) + RingOffset));
        }

        for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
        {
            const int32 Next = (Segment + 1) % SegmentCount;
            Builder.AppendTriangle(StartVertices[Segment], EndVertices[Segment], EndVertices[Next], PolygonGroup);
            Builder.AppendTriangle(StartVertices[Segment], EndVertices[Next], StartVertices[Next], PolygonGroup);
        }

        const FVertexID StartCenter = Builder.AppendVertex(FVector::UpVector * StartZ);
        const FVertexID EndCenter = Builder.AppendVertex(FVector::UpVector * EndZ);
        for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
        {
            const int32 Next = (Segment + 1) % SegmentCount;
            Builder.AppendTriangle(StartCenter, StartVertices[Next], StartVertices[Segment], PolygonGroup);
            Builder.AppendTriangle(EndCenter, EndVertices[Segment], EndVertices[Next], PolygonGroup);
        }
    }

    void BuildThrowableMeshDescription(
        FMeshDescriptionBuilder& Builder,
        const FPolygonGroupID PolygonGroup,
        const FBlackMeridianThrowableMeshSpec& Spec)
    {
        // These silhouettes are deliberately compact and readable in first
        // person and during a short airborne throw. Production grenade art
        // can replace the assets without changing the data or throw code.
        AddThrowableCylinderZ(Builder, PolygonGroup, -9.0f, 9.0f, 8.0f);
        AddThrowableCylinderZ(Builder, PolygonGroup, -11.0f, -8.5f, 9.5f, 10);
        AddThrowableCylinderZ(Builder, PolygonGroup, 8.5f, 11.0f, 9.5f, 10);

        if (Spec.Kind == EBlackMeridianThrowableMeshKind::Frag)
        {
            // Fragmentation ribs and a raised fuze cap give the frag a
            // distinct industrial outline without relying on textures.
            AddWeaponBox(Builder, PolygonGroup, FVector(-8.5f, -1.2f, -7.0f), FVector(8.5f, 1.2f, 7.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(-1.2f, -8.5f, -7.0f), FVector(1.2f, 8.5f, 7.0f));
            AddThrowableCylinderZ(Builder, PolygonGroup, 11.0f, 15.5f, 3.2f, 8);
            AddWeaponBox(Builder, PolygonGroup, FVector(-4.0f, -2.0f, 14.0f), FVector(4.0f, 2.0f, 16.0f));
        }
        else
        {
            // Veil is a smooth canister with a luminous collar and a short
            // antenna-like cap so players can distinguish it from the frag.
            AddThrowableCylinderZ(Builder, PolygonGroup, -3.0f, 3.0f, 9.2f, 10);
            AddThrowableCylinderZ(Builder, PolygonGroup, 11.0f, 15.0f, 4.0f, 8);
            AddWeaponBox(Builder, PolygonGroup, FVector(-5.5f, -5.5f, -1.5f), FVector(5.5f, 5.5f, 1.5f));
        }
    }

    UStaticMesh* CreateThrowableMeshAsset(const FBlackMeridianThrowableMeshSpec& Spec)
    {
        const FString PackageName = FString::Printf(TEXT("/Game/Weapons/Meshes/%s"), Spec.AssetName);
        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            PackageName,
            FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFilename), true);

        if (UStaticMesh* Existing = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackageName, Spec.AssetName)))
        {
            return Existing;
        }

        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            return nullptr;
        }

        UStaticMesh* Mesh = NewObject<UStaticMesh>(Package, FName(Spec.AssetName), RF_Public | RF_Standalone);
        if (!Mesh)
        {
            return nullptr;
        }

        FMeshDescription MeshDescription;
        FStaticMeshAttributes Attributes(MeshDescription);
        Attributes.Register();
        FMeshDescriptionBuilder Builder;
        Builder.SetMeshDescription(&MeshDescription);
        Builder.SetAllEdgesHardness(true);
        const FPolygonGroupID PolygonGroup = Builder.AppendPolygonGroup(FName(TEXT("Throwable")));
        BuildThrowableMeshDescription(Builder, PolygonGroup, Spec);

        TArray<FStaticMaterial> Materials;
        FStaticMaterial& Material = Materials.AddDefaulted_GetRef();
        Material.MaterialInterface = LoadObject<UMaterialInterface>(
            nullptr,
            TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        Material.MaterialSlotName = FName(TEXT("Throwable"));
        Material.ImportedMaterialSlotName = FName(TEXT("Throwable"));
        Mesh->SetStaticMaterials(Materials);

        UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
        BuildParams.bMarkPackageDirty = true;
        BuildParams.bUseHashAsGuid = true;
        BuildParams.bBuildSimpleCollision = true;
        BuildParams.bCommitMeshDescription = true;
        BuildParams.bFastBuild = false;
        if (!Mesh->BuildFromMeshDescriptions({&MeshDescription}, BuildParams))
        {
            return nullptr;
        }

        UStaticMeshSocket* GripSocket = NewObject<UStaticMeshSocket>(Mesh, NAME_None, RF_Public | RF_Standalone);
        if (GripSocket)
        {
            GripSocket->SocketName = TEXT("Grip");
            GripSocket->RelativeLocation = FVector::ZeroVector;
            Mesh->AddSocket(GripSocket);
        }

        FAssetRegistryModule::AssetCreated(Mesh);
        Package->MarkPackageDirty();
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Package, Mesh, *PackageFilename, SaveArgs))
        {
            return nullptr;
        }
        return Mesh;
    }

    bool SaveGeneratedAsset(UObject* Asset, const FString& PackageFilename)
    {
        if (!Asset || !Asset->GetOutermost())
        {
            return false;
        }

        Asset->GetOutermost()->MarkPackageDirty();
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(
            Asset->GetOutermost(),
            Asset,
            *PackageFilename,
            SaveArgs);
    }

    UTexture2D* CreateIndustrialGrungeTexture()
    {
        const FString PackageName = TEXT("/Game/Environment/Textures/T_BM_IndustrialGrunge");
        const FString ObjectPath = TEXT("/Game/Environment/Textures/T_BM_IndustrialGrunge.T_BM_IndustrialGrunge");
        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            PackageName,
            FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFilename), true);

        UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
        UPackage* Package = Texture ? Texture->GetOutermost() : CreatePackage(*PackageName);
        if (!Package)
        {
            return nullptr;
        }

        if (!Texture)
        {
            Texture = NewObject<UTexture2D>(
                Package,
                FName(TEXT("T_BM_IndustrialGrunge")),
                RF_Public | RF_Standalone);
        }
        if (!Texture)
        {
            return nullptr;
        }

        // A tiny authored source texture gives the modular kit real surface
        // identity without introducing a large texture dependency. The two
        // scales of deterministic breakup produce broad stained bands and
        // fine paint/oxidation variation on the generated UVs.
        constexpr int32 TextureSize = 128;
        TArray<uint8> Pixels;
        Pixels.SetNumUninitialized(TextureSize * TextureSize * 4);
        for (int32 Y = 0; Y < TextureSize; ++Y)
        {
            for (int32 X = 0; X < TextureSize; ++X)
            {
                const float U = static_cast<float>(X) / static_cast<float>(TextureSize);
                const float V = static_cast<float>(Y) / static_cast<float>(TextureSize);
                const float Broad = 0.5f + 0.5f * FMath::Sin(U * 19.0f + FMath::Sin(V * 7.0f) * 2.1f);
                const float Fine = 0.5f + 0.5f * FMath::Sin(U * 83.0f + V * 61.0f + FMath::Sin(U * 17.0f) * 4.0f);
                const float Streak = 0.5f + 0.5f * FMath::Sin((U * 3.0f + V * 21.0f) * PI);
                const uint8 Value = static_cast<uint8>(FMath::Clamp(
                    FMath::RoundToInt((0.46f + Broad * 0.24f + Fine * 0.14f + Streak * 0.16f) * 255.0f),
                    0,
                    255));
                const int32 PixelIndex = (Y * TextureSize + X) * 4;
                Pixels[PixelIndex + 0] = Value;
                Pixels[PixelIndex + 1] = Value;
                Pixels[PixelIndex + 2] = Value;
                Pixels[PixelIndex + 3] = 255;
            }
        }

        Texture->PreEditChange(nullptr);
        Texture->Source.Init(TextureSize, TextureSize, 1, 1, TSF_BGRA8, Pixels.GetData());
        Texture->SRGB = false;
        Texture->CompressionSettings = TC_Grayscale;
        Texture->MipGenSettings = TMGS_FromTextureGroup;
        Texture->AddressX = TA_Wrap;
        Texture->AddressY = TA_Wrap;
        Texture->PostEditChange();
        Texture->UpdateResource();
        if (!LoadObject<UTexture2D>(nullptr, *ObjectPath))
        {
            FAssetRegistryModule::AssetCreated(Texture);
        }

        return SaveGeneratedAsset(Texture, PackageFilename) ? Texture : nullptr;
    }

    UMaterialInterface* CreateIndustrialSurfaceMaster()
    {
        const FString PackageName = TEXT("/Game/Environment/Materials/M_BM_IndustrialSurface");
        const FString ObjectPath = TEXT("/Game/Environment/Materials/M_BM_IndustrialSurface.M_BM_IndustrialSurface");
        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            PackageName,
            FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFilename), true);

        UTexture2D* GrungeTexture = CreateIndustrialGrungeTexture();
        if (!GrungeTexture)
        {
            return nullptr;
        }

        UMaterial* Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
        UPackage* Package = Material ? Material->GetOutermost() : CreatePackage(*PackageName);
        if (!Package)
        {
            return nullptr;
        }
        if (!Material)
        {
            Material = NewObject<UMaterial>(
                Package,
                FName(TEXT("M_BM_IndustrialSurface")),
                RF_Public | RF_Standalone);
        }
        if (!Material || !Material->GetEditorOnlyData())
        {
            return nullptr;
        }

        UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
        EditorData->ExpressionCollection.Empty();

        const auto AddExpression = [Material](UMaterialExpression* Expression) -> UMaterialExpression*
        {
            if (Expression && Material->GetEditorOnlyData())
            {
                Material->GetEditorOnlyData()->ExpressionCollection.AddExpression(Expression);
            }
            return Expression;
        };

        UMaterialExpressionTextureSampleParameter2D* Grunge = Cast<UMaterialExpressionTextureSampleParameter2D>(AddExpression(
            NewObject<UMaterialExpressionTextureSampleParameter2D>(Material)));
        UMaterialExpressionVectorParameter* BaseColor = Cast<UMaterialExpressionVectorParameter>(AddExpression(
            NewObject<UMaterialExpressionVectorParameter>(Material)));
        UMaterialExpressionScalarParameter* Metallic = Cast<UMaterialExpressionScalarParameter>(AddExpression(
            NewObject<UMaterialExpressionScalarParameter>(Material)));
        UMaterialExpressionScalarParameter* Roughness = Cast<UMaterialExpressionScalarParameter>(AddExpression(
            NewObject<UMaterialExpressionScalarParameter>(Material)));
        UMaterialExpressionVectorParameter* EmissiveColor = Cast<UMaterialExpressionVectorParameter>(AddExpression(
            NewObject<UMaterialExpressionVectorParameter>(Material)));
        UMaterialExpressionScalarParameter* EmissiveStrength = Cast<UMaterialExpressionScalarParameter>(AddExpression(
            NewObject<UMaterialExpressionScalarParameter>(Material)));
        UMaterialExpressionLinearInterpolate* SurfaceVariation = Cast<UMaterialExpressionLinearInterpolate>(AddExpression(
            NewObject<UMaterialExpressionLinearInterpolate>(Material)));
        UMaterialExpressionMultiply* BaseColorMultiply = Cast<UMaterialExpressionMultiply>(AddExpression(
            NewObject<UMaterialExpressionMultiply>(Material)));
        UMaterialExpressionMultiply* EmissiveMultiply = Cast<UMaterialExpressionMultiply>(AddExpression(
            NewObject<UMaterialExpressionMultiply>(Material)));

        if (!Grunge || !BaseColor || !Metallic || !Roughness || !EmissiveColor || !EmissiveStrength
            || !SurfaceVariation || !BaseColorMultiply || !EmissiveMultiply)
        {
            return nullptr;
        }

        Grunge->ParameterName = FName(TEXT("IndustrialGrunge"));
        Grunge->Texture = GrungeTexture;
        Grunge->SamplerType = SAMPLERTYPE_LinearGrayscale;
        Grunge->MaterialExpressionEditorX = -760;
        Grunge->MaterialExpressionEditorY = 20;

        BaseColor->ParameterName = FName(TEXT("BaseColor"));
        BaseColor->DefaultValue = FLinearColor::White;
        BaseColor->MaterialExpressionEditorX = -760;
        BaseColor->MaterialExpressionEditorY = -180;

        Metallic->ParameterName = FName(TEXT("Metallic"));
        Metallic->DefaultValue = 0.20f;
        Metallic->MaterialExpressionEditorX = -760;
        Metallic->MaterialExpressionEditorY = 240;

        Roughness->ParameterName = FName(TEXT("Roughness"));
        Roughness->DefaultValue = 0.78f;
        Roughness->MaterialExpressionEditorX = -760;
        Roughness->MaterialExpressionEditorY = 360;

        EmissiveColor->ParameterName = FName(TEXT("EmissiveColor"));
        EmissiveColor->DefaultValue = FLinearColor::Black;
        EmissiveColor->MaterialExpressionEditorX = -760;
        EmissiveColor->MaterialExpressionEditorY = 520;

        EmissiveStrength->ParameterName = FName(TEXT("EmissiveStrength"));
        EmissiveStrength->DefaultValue = 0.75f;
        EmissiveStrength->MaterialExpressionEditorX = -760;
        EmissiveStrength->MaterialExpressionEditorY = 650;

        SurfaceVariation->ConstA = 0.70f;
        SurfaceVariation->ConstB = 1.22f;
        SurfaceVariation->Alpha.Expression = Grunge;
        SurfaceVariation->MaterialExpressionEditorX = -300;
        SurfaceVariation->MaterialExpressionEditorY = 40;

        BaseColorMultiply->A.Expression = BaseColor;
        BaseColorMultiply->B.Expression = SurfaceVariation;
        BaseColorMultiply->MaterialExpressionEditorX = 20;
        BaseColorMultiply->MaterialExpressionEditorY = -120;

        EmissiveMultiply->A.Expression = EmissiveColor;
        EmissiveMultiply->B.Expression = EmissiveStrength;
        EmissiveMultiply->MaterialExpressionEditorX = 20;
        EmissiveMultiply->MaterialExpressionEditorY = 520;

        EditorData->BaseColor.Expression = BaseColorMultiply;
        EditorData->Metallic.Expression = Metallic;
        EditorData->Roughness.Expression = Roughness;
        EditorData->EmissiveColor.Expression = EmissiveMultiply;
        Material->BlendMode = BLEND_Opaque;
        Material->PostEditChange();
        Material->ForceRecompileForRendering();
        if (!LoadObject<UMaterial>(nullptr, *ObjectPath))
        {
            FAssetRegistryModule::AssetCreated(Material);
        }

        return SaveGeneratedAsset(Material, PackageFilename) ? Material : nullptr;
    }

    UMaterialInterface* CreateEnvironmentMaterialAsset()
    {
        const FString PackageName = TEXT("/Game/Environment/Materials/MI_BM_IndustrialSurface");
        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            PackageName,
            FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFilename), true);

        UMaterialInterface* Parent = CreateIndustrialSurfaceMaster();
        if (!Parent)
        {
            Parent = LoadObject<UMaterialInterface>(
                nullptr,
                TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        }
        if (!Parent)
        {
            return nullptr;
        }

        if (UMaterialInstanceConstant* Existing = LoadObject<UMaterialInstanceConstant>(
                nullptr,
                TEXT("/Game/Environment/Materials/MI_BM_IndustrialSurface.MI_BM_IndustrialSurface")))
        {
            Existing->SetParentEditorOnly(Parent, false);
            Existing->ClearParameterValuesEditorOnly();
            Existing->PostEditChange();
            SaveGeneratedAsset(Existing, PackageFilename);
            return Existing;
        }

        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            return nullptr;
        }

        UMaterialInstanceConstant* Material = NewObject<UMaterialInstanceConstant>(
            Package,
            FName(TEXT("MI_BM_IndustrialSurface")),
            RF_Public | RF_Standalone);
        if (!Material)
        {
            return nullptr;
        }

        Material->SetParentEditorOnly(Parent, false);
        Material->PostEditChange();
        FAssetRegistryModule::AssetCreated(Material);
        if (!SaveGeneratedAsset(Material, PackageFilename))
        {
            return nullptr;
        }
        return Material;
    }

    USoundBase* EnsureGeneratedSoundAsset(const TCHAR* CueName)
    {
        if (!CueName || !*CueName)
        {
            return nullptr;
        }

        const FString ObjectPath = FString::Printf(
            TEXT("/Game/Audio/Generated/%s.%s"),
            CueName,
            CueName);
        // Avoid emitting a false missing-asset warning on the first automation
        // run.  The sound is intentionally imported below when the generated
        // WAV has not been materialized yet.
        if (USoundBase* Existing = FindObject<USoundBase>(nullptr, *ObjectPath))
        {
            return Existing;
        }

        const FString SourceFilename = FPaths::ProjectContentDir() / TEXT("Audio/Generated") / FString(CueName) + TEXT(".wav");
        if (!IFileManager::Get().FileExists(*SourceFilename))
        {
            return nullptr;
        }

        USoundFactory* Factory = NewObject<USoundFactory>();
        if (!Factory)
        {
            return nullptr;
        }
        TArray<FString> SourceFiles;
        SourceFiles.Add(SourceFilename);
        const TArray<UObject*> Imported = FAssetToolsModule::GetModule().Get().ImportAssets(
            SourceFiles,
            TEXT("/Game/Audio/Generated"),
            Factory,
            false);
        for (UObject* ImportedObject : Imported)
        {
            if (USoundBase* ImportedSound = Cast<USoundBase>(ImportedObject))
            {
                if (USoundWave* ImportedWave = Cast<USoundWave>(ImportedSound))
                {
                    if (FCString::Strncmp(CueName, TEXT("BM_Music_"), 9) == 0
                        || FCString::Strncmp(CueName, TEXT("BM_Ambient_"), 11) == 0)
                    {
                        ImportedWave->bLooping = true;
                    }
                }

                UPackage* SoundPackage = ImportedSound->GetOutermost();
                if (SoundPackage)
                {
                    SoundPackage->MarkPackageDirty();
                    const FString SoundPackageFilename = FPackageName::LongPackageNameToFilename(
                        SoundPackage->GetName(),
                        FPackageName::GetAssetPackageExtension());
                    IFileManager::Get().MakeDirectory(*FPaths::GetPath(SoundPackageFilename), true);

                    FSavePackageArgs SaveArgs;
                    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
                    SaveArgs.SaveFlags = SAVE_NoError;
                    if (!UPackage::SavePackage(
                            SoundPackage,
                            ImportedSound,
                            *SoundPackageFilename,
                            SaveArgs))
                    {
                        return nullptr;
                    }
                }
                return ImportedSound;
            }
        }
        return LoadObject<USoundBase>(nullptr, *ObjectPath);
    }

    UAnimBlueprint* CreateCharacterAnimationBlueprint()
    {
        const FString PackageName = TEXT("/Game/Characters/Mannequins/Animations/ABP_BM_Manny_Rifle");
        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            PackageName,
            FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFilename), true);
        const FString ObjectPath = TEXT("/Game/Characters/Mannequins/Animations/ABP_BM_Manny_Rifle.ABP_BM_Manny_Rifle");

        UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, *ObjectPath);
        if (!Blueprint)
        {
            UPackage* Package = CreatePackage(*PackageName);
            if (!Package)
            {
                return nullptr;
            }

            UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
            Factory->BlueprintType = BPTYPE_Normal;
            Factory->ParentClass = UAnimInstance::StaticClass();
            Factory->TargetSkeleton = LoadObject<USkeleton>(
                nullptr,
                TEXT("/Game/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"));
            Factory->PreviewSkeletalMesh = LoadObject<USkeletalMesh>(
                nullptr,
                TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));

            Blueprint = Cast<UAnimBlueprint>(Factory->FactoryCreateNew(
                UAnimBlueprint::StaticClass(),
                Package,
                FName(TEXT("ABP_BM_Manny_Rifle")),
                RF_Public | RF_Standalone,
                nullptr,
                GWarn));
            if (!Blueprint)
            {
                return nullptr;
            }
            FAssetRegistryModule::AssetCreated(Blueprint);
        }

        UAnimationGraph* AnimGraph = nullptr;
        for (UEdGraph* CandidateGraph : Blueprint->FunctionGraphs)
        {
            if (CandidateGraph && CandidateGraph->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
            {
                AnimGraph = Cast<UAnimationGraph>(CandidateGraph);
                break;
            }
        }
        if (!AnimGraph)
        {
            return nullptr;
        }

        UAnimGraphNode_Root* RootNode = nullptr;
        for (UEdGraphNode* Node : AnimGraph->Nodes)
        {
            if (UAnimGraphNode_Root* CandidateRoot = Cast<UAnimGraphNode_Root>(Node))
            {
                RootNode = CandidateRoot;
                break;
            }
        }
        if (!RootNode)
        {
            return nullptr;
        }

        // Rebuild this tiny graph on reruns so an interrupted editor session
        // cannot leave behind a half-connected slot node.
        AnimGraph->Modify();
        TArray<UEdGraphNode*> ExistingNodes = AnimGraph->Nodes;
        for (UEdGraphNode* Node : ExistingNodes)
        {
            if (Node && Node != RootNode)
            {
                AnimGraph->RemoveNode(Node);
            }
        }

        UAnimSequence* IdleSequence = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS.MF_Rifle_Idle_ADS"));
        if (!IdleSequence)
        {
            return nullptr;
        }

        FGraphNodeCreator<UAnimGraphNode_SequencePlayer> SequenceNodeCreator(*AnimGraph);
        UAnimGraphNode_SequencePlayer* SequenceNode = SequenceNodeCreator.CreateNode();
        SequenceNode->Node.SetSequence(IdleSequence);
        SequenceNode->Node.SetLoopAnimation(true);
        SequenceNode->Node.SetPlayRate(1.0f);
        SequenceNode->NodePosX = -420;
        SequenceNode->NodePosY = 0;
        SequenceNodeCreator.Finalize();

        FGraphNodeCreator<UAnimGraphNode_Slot> SlotNodeCreator(*AnimGraph);
        UAnimGraphNode_Slot* SlotNode = SlotNodeCreator.CreateNode();
        SlotNode->Node.SlotName = FName(TEXT("DefaultSlot"));
        SlotNode->Node.bAlwaysUpdateSourcePose = true;
        SlotNode->NodePosX = -80;
        SlotNode->NodePosY = 0;
        SlotNodeCreator.Finalize();

        UEdGraphPin* SequenceOutput = SequenceNode->FindPin(TEXT("Pose"), EGPD_Output);
        UEdGraphPin* SlotInput = SlotNode->FindPin(TEXT("Source"), EGPD_Input);
        UEdGraphPin* SlotOutput = SlotNode->FindPin(TEXT("Pose"), EGPD_Output);
        UEdGraphPin* RootInput = RootNode->FindPin(TEXT("Result"), EGPD_Input);
        const UEdGraphSchema* Schema = AnimGraph->GetSchema();
        const bool bLinkedSequence = Schema
            && SequenceOutput
            && SlotInput
            && Schema->TryCreateConnection(SequenceOutput, SlotInput);
        const bool bLinkedRoot = Schema
            && SlotOutput
            && RootInput
            && Schema->TryCreateConnection(SlotOutput, RootInput);
        if (!bLinkedSequence || !bLinkedRoot)
        {
            return nullptr;
        }

        AnimGraph->NotifyGraphChanged();
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        if (!Blueprint->GeneratedClass)
        {
            return nullptr;
        }

        Blueprint->MarkPackageDirty();
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *PackageFilename, SaveArgs))
        {
            return nullptr;
        }
        return Blueprint;
    }

    UBMCharacterAnimationDataAsset* CreateCharacterAnimationAsset()
    {
        UAnimBlueprint* AnimationBlueprint = CreateCharacterAnimationBlueprint();
        const FString PackageName = TEXT("/Game/Data/Animations/DA_BM_Manny_Rifle_Animation");
        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            PackageName,
            FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFilename), true);
        const FString ObjectPath = TEXT("/Game/Data/Animations/DA_BM_Manny_Rifle_Animation.DA_BM_Manny_Rifle_Animation");

        UBMCharacterAnimationDataAsset* Asset = LoadObject<UBMCharacterAnimationDataAsset>(nullptr, *ObjectPath);
        if (!Asset)
        {
            UPackage* Package = CreatePackage(*PackageName);
            if (!Package)
            {
                return nullptr;
            }
            Asset = NewObject<UBMCharacterAnimationDataAsset>(
                Package,
                FName(TEXT("DA_BM_Manny_Rifle_Animation")),
                RF_Public | RF_Standalone);
            if (!Asset)
            {
                return nullptr;
            }
            FAssetRegistryModule::AssetCreated(Asset);
        }

        const auto SequencePath = [](const TCHAR* Path)
        {
            return TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(Path));
        };

        Asset->LocomotionSequences.Reset();
        Asset->LocomotionSequences.Add(
            TEXT("Idle"),
            SequencePath(TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS.MF_Rifle_Idle_ADS")));
        Asset->LocomotionSequences.Add(
            TEXT("Walk"),
            SequencePath(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd.MF_Rifle_Walk_Fwd")));
        Asset->LocomotionSequences.Add(
            TEXT("Sprint"),
            SequencePath(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Fwd.MF_Rifle_Jog_Fwd")));
        Asset->LocomotionSequences.Add(
            TEXT("Jump"),
            SequencePath(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Jump/MM_Rifle_Jump_Fall_Loop.MM_Rifle_Jump_Fall_Loop")));
        Asset->LocomotionSequences.Add(
            TEXT("Fall"),
            SequencePath(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Jump/MM_Rifle_Jump_Fall_Loop.MM_Rifle_Jump_Fall_Loop")));

        Asset->EventSequences.Reset();
        Asset->EventSequences.Add(
            TEXT("Fire"),
            SequencePath(TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Fire.MM_Rifle_Fire")));
        Asset->EventSequences.Add(
            TEXT("ReloadStarted"),
            SequencePath(TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload.MM_Rifle_Reload")));
        Asset->EventSequences.Add(
            TEXT("DryFire"),
            SequencePath(TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_DryFire.MM_Rifle_DryFire")));
        Asset->EventSequences.Add(
            TEXT("HitReact"),
            SequencePath(TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01.MM_HitReact_Front_Lgt_01")));
        Asset->EventSequences.Add(
            TEXT("HeadHitReact"),
            SequencePath(TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Hvy_01.MM_HitReact_Front_Hvy_01")));
        Asset->EventSequences.Add(
            TEXT("Death"),
            SequencePath(TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_01.MM_Death_Front_01")));

        // The data asset owns the project-local generated class. This keeps
        // runtime characters independent from an engine sample AnimBP and
        // lets additive fire/hit montages play through DefaultSlot in a
        // cooked build.
        Asset->AnimBlueprintClass = AnimationBlueprint
            ? AnimationBlueprint->GeneratedClass
            : nullptr;

        Asset->MarkPackageDirty();
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Asset->GetOutermost(), Asset, *PackageFilename, SaveArgs))
        {
            return nullptr;
        }
        return Asset;
    }

    UBMAudioDataAsset* CreateCombatAudioAsset()
    {
        const FString PackageName = TEXT("/Game/Data/Audio/DA_BM_CombatAudio");
        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            PackageName,
            FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFilename), true);
        const FString ObjectPath = TEXT("/Game/Data/Audio/DA_BM_CombatAudio.DA_BM_CombatAudio");

        UBMAudioDataAsset* Asset = LoadObject<UBMAudioDataAsset>(nullptr, *ObjectPath);
        if (!Asset)
        {
            UPackage* Package = CreatePackage(*PackageName);
            if (!Package)
            {
                return nullptr;
            }
            Asset = NewObject<UBMAudioDataAsset>(
                Package,
                FName(TEXT("DA_BM_CombatAudio")),
                RF_Public | RF_Standalone);
            if (!Asset)
            {
                return nullptr;
            }
            FAssetRegistryModule::AssetCreated(Asset);
        }

        const TCHAR* CueNames[] = {
            TEXT("BM_ARX4_Fire"),
            TEXT("BM_Suppressed_Fire"),
            TEXT("BM_DryFire"),
            TEXT("BM_Impact_Concrete"),
            TEXT("BM_Impact_Metal"),
            TEXT("BM_Reload"),
            TEXT("BM_Explosion"),
            TEXT("BM_Hit_Confirm"),
            TEXT("BM_Elimination"),
            TEXT("BM_Footstep_Concrete"),
            TEXT("BM_Footstep_Metal"),
            TEXT("BM_Footstep_SoftCover"),
            TEXT("BM_LowHealth"),
            TEXT("BM_Suppression"),
            TEXT("BM_ThrowableWarning"),
            TEXT("BM_Music_Mission"),
            TEXT("BM_Music_Suspense"),
            TEXT("BM_Music_Combat"),
            TEXT("BM_Music_Complete"),
            TEXT("BM_Ambient_SableQuay"),
            TEXT("BM_Ambient_Floodline"),
            TEXT("BM_Ambient_RelayHouse"),
            TEXT("BM_Ambient_NightglassCore"),
            TEXT("BM_Ambient_AshenDocks"),
            TEXT("BM_Ambient_CinderWorks"),
            TEXT("BM_Ambient_SaltFoundry"),
            TEXT("BM_Ambient_BlackwaterYard"),
            TEXT("BM_Ambient_GlassMarket"),
            TEXT("BM_Ambient_StormVault"),
            TEXT("BM_Ambient_RedTide"),
            TEXT("BM_Ambient_IronCathedral"),
            TEXT("BM_Ambient_LanternBasin"),
            TEXT("BM_Ambient_QuarryNine"),
            TEXT("BM_Ambient_VantaSpire"),
            TEXT("BM_Ambient_Wreckline"),
            TEXT("BM_Ambient_EmberStation"),
            TEXT("BM_Ambient_PaleHarbor"),
            TEXT("BM_Ambient_MeridianGate"),
            TEXT("BM_Ambient_LastLight")};
        for (const TCHAR* CueName : CueNames)
        {
            EnsureGeneratedSoundAsset(CueName);
        }

        const auto CuePath = [](const TCHAR* CueName)
        {
            return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(
                TEXT("/Game/Audio/Generated/%s.%s"),
                CueName,
                CueName)));
        };

        Asset->IndoorGunshot = CuePath(TEXT("BM_ARX4_Fire"));
        Asset->OutdoorGunshot = CuePath(TEXT("BM_ARX4_Fire"));
        Asset->SuppressedGunshot = CuePath(TEXT("BM_Suppressed_Fire"));
        Asset->DryFire = CuePath(TEXT("BM_DryFire"));
        Asset->BulletImpact = CuePath(TEXT("BM_Impact_Concrete"));
        Asset->SurfaceImpacts.Add(EBMSurfaceType::Concrete, CuePath(TEXT("BM_Impact_Concrete")));
        Asset->SurfaceImpacts.Add(EBMSurfaceType::Metal, CuePath(TEXT("BM_Impact_Metal")));
        Asset->SurfaceImpacts.Add(EBMSurfaceType::Glass, CuePath(TEXT("BM_Impact_Metal")));
        Asset->SurfaceImpacts.Add(EBMSurfaceType::Wood, CuePath(TEXT("BM_Impact_Concrete")));
        Asset->SurfaceImpacts.Add(EBMSurfaceType::SoftCover, CuePath(TEXT("BM_Impact_Concrete")));
        Asset->Footstep = CuePath(TEXT("BM_Footstep_Concrete"));
        Asset->SurfaceFootsteps.Add(EBMSurfaceType::Concrete, CuePath(TEXT("BM_Footstep_Concrete")));
        Asset->SurfaceFootsteps.Add(EBMSurfaceType::Metal, CuePath(TEXT("BM_Footstep_Metal")));
        Asset->SurfaceFootsteps.Add(EBMSurfaceType::SoftCover, CuePath(TEXT("BM_Footstep_SoftCover")));
        Asset->Reload = CuePath(TEXT("BM_Reload"));
        Asset->Explosion = CuePath(TEXT("BM_Explosion"));
        Asset->LowHealth = CuePath(TEXT("BM_LowHealth"));
        Asset->Suppression = CuePath(TEXT("BM_Suppression"));
        Asset->ThrowableWarning = CuePath(TEXT("BM_ThrowableWarning"));
        Asset->MusicState = CuePath(TEXT("BM_Music_Mission"));
        Asset->MusicStates.Add(TEXT("Mission"), CuePath(TEXT("BM_Music_Mission")));
        Asset->MusicStates.Add(TEXT("Suspense"), CuePath(TEXT("BM_Music_Suspense")));
        Asset->MusicStates.Add(TEXT("Combat"), CuePath(TEXT("BM_Music_Combat")));
        Asset->MusicStates.Add(TEXT("Complete"), CuePath(TEXT("BM_Music_Complete")));
        Asset->AmbientBattlefield = CuePath(TEXT("BM_Ambient_SableQuay"));
        const TCHAR* EnvironmentIds[] = {
            TEXT("SableQuay"),
            TEXT("Floodline"),
            TEXT("RelayHouse"),
            TEXT("NightglassCore"),
            TEXT("AshenDocks"),
            TEXT("CinderWorks"),
            TEXT("SaltFoundry"),
            TEXT("BlackwaterYard"),
            TEXT("GlassMarket"),
            TEXT("StormVault"),
            TEXT("RedTide"),
            TEXT("IronCathedral"),
            TEXT("LanternBasin"),
            TEXT("QuarryNine"),
            TEXT("VantaSpire"),
            TEXT("Wreckline"),
            TEXT("EmberStation"),
            TEXT("PaleHarbor"),
            TEXT("MeridianGate"),
            TEXT("LastLight")};
        for (const TCHAR* EnvironmentId : EnvironmentIds)
        {
            const FString CueName = FString::Printf(TEXT("BM_Ambient_%s"), EnvironmentId);
            Asset->EnvironmentAmbiences.Add(FName(EnvironmentId), CuePath(*CueName));
        }
        Asset->MarkPackageDirty();

        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Asset->GetOutermost(), Asset, *PackageFilename, SaveArgs))
        {
            return nullptr;
        }
        return Asset;
    }

    UNiagaraSystem* DuplicateProjectNiagaraSystem(
        const TCHAR* SourceObjectPath,
        const TCHAR* AssetName)
    {
        if (!SourceObjectPath || !*SourceObjectPath || !AssetName || !*AssetName)
        {
            return nullptr;
        }

        const FString PackageName = FString::Printf(TEXT("/Game/VFX/Generated/%s"), AssetName);
        const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, AssetName);
        if (UNiagaraSystem* Existing = LoadObject<UNiagaraSystem>(nullptr, *ObjectPath))
        {
            return Existing;
        }

        UNiagaraSystem* Source = LoadObject<UNiagaraSystem>(nullptr, SourceObjectPath);
        if (!Source)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Black Meridian VFX: Niagara template failed to load source=%s"),
                SourceObjectPath);
            return nullptr;
        }

        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            PackageName,
            FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFilename), true);

        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            return nullptr;
        }

        UNiagaraSystem* Asset = DuplicateObject<UNiagaraSystem>(Source, Package, FName(AssetName));
        if (!Asset)
        {
            return nullptr;
        }

        Asset->SetFlags(RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Asset);
        Package->MarkPackageDirty();

        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Package, Asset, *PackageFilename, SaveArgs))
        {
            return nullptr;
        }
        return Asset;
    }

    UBMVFXDataAsset* CreateCombatVFXAsset()
    {
        const FString PackageName = TEXT("/Game/Data/VFX/DA_BM_CombatVFX");
        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            PackageName,
            FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFilename), true);
        const FString ObjectPath = TEXT("/Game/Data/VFX/DA_BM_CombatVFX.DA_BM_CombatVFX");

        UBMVFXDataAsset* Asset = LoadObject<UBMVFXDataAsset>(nullptr, *ObjectPath);
        if (!Asset)
        {
            UPackage* Package = CreatePackage(*PackageName);
            if (!Package)
            {
                return nullptr;
            }
            Asset = NewObject<UBMVFXDataAsset>(
                Package,
                FName(TEXT("DA_BM_CombatVFX")),
                RF_Public | RF_Standalone);
            if (!Asset)
            {
                return nullptr;
            }
            FAssetRegistryModule::AssetCreated(Asset);
        }

        const auto NiagaraPath = [](UNiagaraSystem* System)
        {
            return TSoftObjectPtr<UNiagaraSystem>(
                System ? FSoftObjectPath(System->GetPathName()) : FSoftObjectPath());
        };

        // These are UE Niagara's deterministic burst templates, duplicated
        // into project-owned packages so the combat profile has real cooked
        // systems rather than relying on a runtime debug primitive. They are
        // intentionally replaceable by final authored VFX without changing
        // weapon, projectile, or throwable gameplay code.
        UNiagaraSystem* MuzzleFlash = DuplicateProjectNiagaraSystem(
            TEXT("/Niagara/DefaultAssets/Templates/Systems/DirectionalBurst.DirectionalBurst"),
            TEXT("NS_BM_MuzzleFlash"));
        UNiagaraSystem* BulletImpact = DuplicateProjectNiagaraSystem(
            TEXT("/Niagara/DefaultAssets/Templates/Systems/RadialBurst.RadialBurst"),
            TEXT("NS_BM_BulletImpact"));
        UNiagaraSystem* BloodImpact = DuplicateProjectNiagaraSystem(
            TEXT("/Niagara/DefaultAssets/Templates/Systems/RadialBurst.RadialBurst"),
            TEXT("NS_BM_BloodImpact"));
        UNiagaraSystem* Explosion = DuplicateProjectNiagaraSystem(
            TEXT("/Niagara/DefaultAssets/Templates/Systems/SimpleExplosion.SimpleExplosion"),
            TEXT("NS_BM_Explosion"));
        UNiagaraSystem* Dust = DuplicateProjectNiagaraSystem(
            TEXT("/Niagara/DefaultAssets/Templates/Systems/RadialBurst.RadialBurst"),
            TEXT("NS_BM_Dust"));
        UNiagaraSystem* Smoke = DuplicateProjectNiagaraSystem(
            TEXT("/Niagara/DefaultAssets/Templates/Systems/SimpleExplosion.SimpleExplosion"),
            TEXT("NS_BM_Smoke"));
        UNiagaraSystem* Sparks = DuplicateProjectNiagaraSystem(
            TEXT("/Niagara/DefaultAssets/Templates/Systems/DirectionalBurst.DirectionalBurst"),
            TEXT("NS_BM_Sparks"));
        UNiagaraSystem* ShellEjection = DuplicateProjectNiagaraSystem(
            TEXT("/Niagara/DefaultAssets/Templates/Systems/DirectionalBurst.DirectionalBurst"),
            TEXT("NS_BM_ShellEjection"));
        UNiagaraSystem* Suppression = DuplicateProjectNiagaraSystem(
            TEXT("/Niagara/DefaultAssets/Templates/Systems/RadialBurst.RadialBurst"),
            TEXT("NS_BM_Suppression"));
        // Environment weather gets dedicated project-owned systems. The
        // template systems are intentionally conservative compatibility
        // accents; the director's deterministic instanced field supplies the
        // continuous fall/flurry simulation and these assets remain the
        // authored Niagara layer that can be replaced later without changing
        // gameplay code.
        UNiagaraSystem* Rain = DuplicateProjectNiagaraSystem(
            TEXT("/Niagara/DefaultAssets/Templates/Systems/DirectionalBurst.DirectionalBurst"),
            TEXT("NS_BM_Rain"));
        UNiagaraSystem* Snow = DuplicateProjectNiagaraSystem(
            TEXT("/Niagara/DefaultAssets/Templates/Systems/RadialBurst.RadialBurst"),
            TEXT("NS_BM_Snow"));
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Black Meridian environment VFX assets: rain=%s snow=%s"),
            Rain ? TEXT("ready") : TEXT("missing"),
            Snow ? TEXT("ready") : TEXT("missing"));

        Asset->MuzzleFlash = NiagaraPath(MuzzleFlash);
        Asset->BulletImpact = NiagaraPath(BulletImpact);
        Asset->BloodImpact = NiagaraPath(BloodImpact);
        Asset->Explosion = NiagaraPath(Explosion);
        Asset->Dust = NiagaraPath(Dust);
        Asset->Smoke = NiagaraPath(Smoke);
        Asset->Sparks = NiagaraPath(Sparks);
        Asset->ShellEjection = NiagaraPath(ShellEjection);
        Asset->Suppression = NiagaraPath(Suppression);
        Asset->MarkPackageDirty();

        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Asset->GetOutermost(), Asset, *PackageFilename, SaveArgs))
        {
            return nullptr;
        }
        return Asset;
    }

    UStaticMesh* CreateEnvironmentMeshAsset(
        const FBlackMeridianEnvironmentMeshSpec& Spec,
        UMaterialInterface* EnvironmentMaterial)
    {
        const FString PackageName = FString::Printf(TEXT("/Game/Environment/Meshes/%s"), Spec.AssetName);
        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            PackageName,
            FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFilename), true);

        UStaticMesh* Mesh = LoadObject<UStaticMesh>(
            nullptr,
            *FString::Printf(TEXT("%s.%s"), *PackageName, Spec.AssetName));
        UPackage* Package = Mesh ? Mesh->GetOutermost() : CreatePackage(*PackageName);
        if (!Package)
        {
            return nullptr;
        }

        const bool bNewAsset = Mesh == nullptr;
        if (!Mesh)
        {
            Mesh = NewObject<UStaticMesh>(Package, FName(Spec.AssetName), RF_Public | RF_Standalone);
        }
        if (!Mesh)
        {
            return nullptr;
        }

        FMeshDescription MeshDescription;
        FStaticMeshAttributes Attributes(MeshDescription);
        Attributes.Register();
        FMeshDescriptionBuilder Builder;
        Builder.SetMeshDescription(&MeshDescription);
        Builder.SetNumUVLayers(1);
        Builder.SetAllEdgesHardness(true);
        const FPolygonGroupID PolygonGroup = Builder.AppendPolygonGroup(FName(TEXT("Environment")));
        BuildEnvironmentMeshDescription(Builder, PolygonGroup, Spec.Kind);
        ApplyEnvironmentUVs(Builder, MeshDescription);

        // The generated kit is authored from raw mesh descriptions rather
        // than imported source files, so the editor cannot rely on an import
        // build setting to populate vertex normals/tangents.  Without this
        // explicit pass, UE reports degenerate tangent bases and the cooked
        // Metal path renders several faces as black silhouettes.  Compute the
        // triangle data first, then the vertex-instance NTBs with hard-edge
        // aware options before saving the asset.
        FStaticMeshOperations::ComputeTriangleTangentsAndNormals(
            MeshDescription,
            0.0f,
            Spec.AssetName);
        FStaticMeshOperations::ComputeTangentsAndNormals(
            MeshDescription,
            EComputeNTBsFlags::Normals
                | EComputeNTBsFlags::Tangents
                | EComputeNTBsFlags::UseMikkTSpace
                | EComputeNTBsFlags::IgnoreDegenerateTriangles);

        TArray<FStaticMaterial> Materials;
        FStaticMaterial& Material = Materials.AddDefaulted_GetRef();
        Material.MaterialInterface = EnvironmentMaterial
            ? EnvironmentMaterial
            : LoadObject<UMaterialInterface>(
                nullptr,
                TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        Material.MaterialSlotName = FName(TEXT("Environment"));
        Material.ImportedMaterialSlotName = FName(TEXT("Environment"));
        Mesh->SetStaticMaterials(Materials);

        UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
        BuildParams.bMarkPackageDirty = true;
        BuildParams.bUseHashAsGuid = true;
        BuildParams.bBuildSimpleCollision = true;
        BuildParams.bCommitMeshDescription = true;
        BuildParams.bFastBuild = false;
        if (!Mesh->BuildFromMeshDescriptions({&MeshDescription}, BuildParams))
        {
            return nullptr;
        }

        if (bNewAsset)
        {
            FAssetRegistryModule::AssetCreated(Mesh);
        }
        Package->MarkPackageDirty();
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Package, Mesh, *PackageFilename, SaveArgs))
        {
            return nullptr;
        }
        return Mesh;
    }

    void BuildWeaponMeshDescription(
        FMeshDescriptionBuilder& Builder,
        const FPolygonGroupID PolygonGroup,
        const FBlackMeridianWeaponMeshSpec& Spec)
    {
        if (Spec.Kind == EBlackMeridianWeaponMeshKind::RookDMR)
        {
            // Rook DMR: long receiver, heavy stock, extended handguard and
            // a large optic. The mesh origin is the top of the pistol grip.
            AddWeaponBox(Builder, PolygonGroup, FVector(-5.0f, -6.0f, -8.0f), FVector(32.0f, 6.0f, 3.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(3.0f, -5.0f, 3.0f), FVector(31.0f, 5.0f, 7.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(-29.0f, -5.5f, -8.0f), FVector(-4.0f, 5.5f, 1.5f));
            AddWeaponBox(Builder, PolygonGroup, FVector(7.0f, -4.0f, -21.0f), FVector(17.0f, 4.0f, -6.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(0.0f, -4.0f, -22.0f), FVector(12.0f, 4.0f, -8.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(29.0f, -5.0f, -6.0f), FVector(61.0f, 5.0f, 1.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(8.0f, -3.0f, 7.0f), FVector(21.0f, 3.0f, 12.0f));
            AddWeaponCylinderX(Builder, PolygonGroup, 58.0f, 94.0f, 2.0f);
            AddWeaponCylinderX(Builder, PolygonGroup, 93.0f, 101.0f, 3.4f);
            return;
        }

        if (Spec.Kind == EBlackMeridianWeaponMeshKind::Kestrel9)
        {
            // Kestrel SMG: short receiver, tall magazine and compact stock.
            AddWeaponBox(Builder, PolygonGroup, FVector(-4.0f, -6.0f, -8.0f), FVector(24.0f, 6.0f, 3.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(2.0f, -5.0f, 3.0f), FVector(23.0f, 5.0f, 6.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(-21.0f, -5.0f, -7.0f), FVector(-3.0f, 5.0f, 1.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(6.0f, -4.0f, -20.0f), FVector(15.0f, 4.0f, -6.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(1.0f, -4.0f, -23.0f), FVector(12.0f, 4.0f, -8.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(22.0f, -5.0f, -6.0f), FVector(42.0f, 5.0f, 1.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(7.0f, -3.0f, 6.0f), FVector(17.0f, 3.0f, 10.0f));
            AddWeaponCylinderX(Builder, PolygonGroup, 40.0f, 54.0f, 2.0f);
            AddWeaponCylinderX(Builder, PolygonGroup, 53.0f, 60.0f, 3.0f);
            return;
        }

        if (Spec.Kind == EBlackMeridianWeaponMeshKind::Breach12)
        {
            // Breach-12 shotgun: broad receiver, tubular magazine, and a
            // deliberately heavy front profile that reads at combat distance.
            AddWeaponBox(Builder, PolygonGroup, FVector(-5.0f, -7.0f, -9.0f), FVector(30.0f, 7.0f, 4.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(3.0f, -5.5f, 4.0f), FVector(25.0f, 5.5f, 8.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(-29.0f, -6.0f, -8.0f), FVector(-4.0f, 6.0f, 2.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(5.0f, -4.5f, -23.0f), FVector(16.0f, 4.5f, -7.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(26.0f, -6.0f, -6.0f), FVector(58.0f, 6.0f, 1.0f));
            AddWeaponCylinderX(Builder, PolygonGroup, 25.0f, 56.0f, 1.7f);
            AddWeaponCylinderX(Builder, PolygonGroup, 53.0f, 84.0f, 2.8f);
            AddWeaponCylinderX(Builder, PolygonGroup, 82.0f, 90.0f, 3.8f);
            return;
        }

        if (Spec.Kind == EBlackMeridianWeaponMeshKind::SolacePistol)
        {
            // Solace pistol: compact slide, visible frame, and a distinct
            // angled grip so the sidearm remains readable in first or third
            // person without relying on the fallback primitive kit.
            AddWeaponBox(Builder, PolygonGroup, FVector(-3.0f, -5.5f, -5.0f), FVector(28.0f, 5.5f, 3.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(2.0f, -5.0f, -10.0f), FVector(24.0f, 5.0f, -4.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(5.0f, -4.5f, -24.0f), FVector(16.0f, 4.5f, -8.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(6.0f, -3.0f, 3.0f), FVector(12.0f, 3.0f, 6.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(13.0f, -3.0f, -15.0f), FVector(19.0f, 3.0f, -10.0f));
            AddWeaponCylinderX(Builder, PolygonGroup, 25.0f, 43.0f, 1.8f);
            AddWeaponCylinderX(Builder, PolygonGroup, 42.0f, 48.0f, 2.4f);
            return;
        }

        if (Spec.Kind == EBlackMeridianWeaponMeshKind::ArcLauncher)
        {
            // Arc-7 launcher: oversized tube, breech block, stock, and optic.
            AddWeaponBox(Builder, PolygonGroup, FVector(-22.0f, -8.0f, -8.0f), FVector(26.0f, 8.0f, 8.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(-39.0f, -7.0f, -9.0f), FVector(-21.0f, 7.0f, 5.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(1.0f, -5.0f, -23.0f), FVector(14.0f, 5.0f, -7.0f));
            AddWeaponBox(Builder, PolygonGroup, FVector(5.0f, -4.0f, 8.0f), FVector(20.0f, 4.0f, 14.0f));
            AddWeaponCylinderX(Builder, PolygonGroup, 20.0f, 86.0f, 6.0f);
            AddWeaponCylinderX(Builder, PolygonGroup, 84.0f, 96.0f, 7.4f);
            return;
        }

        // ARX-4 rifle: the standard Vanta weapon silhouette.
        AddWeaponBox(Builder, PolygonGroup, FVector(-4.0f, -6.0f, -8.0f), FVector(28.0f, 6.0f, 3.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(3.0f, -5.0f, 3.0f), FVector(27.0f, 5.0f, 7.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(-25.0f, -5.0f, -7.0f), FVector(-3.0f, 5.0f, 1.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(7.0f, -4.0f, -20.0f), FVector(16.0f, 4.0f, -6.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(0.0f, -4.0f, -22.0f), FVector(11.0f, 4.0f, -8.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(26.0f, -5.0f, -6.0f), FVector(47.0f, 5.0f, 1.0f));
        AddWeaponBox(Builder, PolygonGroup, FVector(8.0f, -3.0f, 7.0f), FVector(18.0f, 3.0f, 11.0f));
        AddWeaponCylinderX(Builder, PolygonGroup, 45.0f, 61.0f, 2.0f);
        AddWeaponCylinderX(Builder, PolygonGroup, 60.0f, 67.0f, 3.0f);
    }

    UStaticMesh* CreateWeaponMeshAsset(const FBlackMeridianWeaponMeshSpec& Spec)
    {
        const FString PackageName = FString::Printf(TEXT("/Game/Weapons/Meshes/%s"), Spec.AssetName);
        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            PackageName,
            FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFilename), true);

        if (UStaticMesh* Existing = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackageName, Spec.AssetName)))
        {
            // Existing packages may have been generated by an earlier test
            // pass. Migrate them in place so the mesh, rather than a runtime
            // guess, owns the hand and muzzle contract.
            if (EnsureWeaponSockets(Existing, Spec))
            {
                Existing->MarkPackageDirty();
                FSavePackageArgs SaveArgs;
                SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
                SaveArgs.SaveFlags = SAVE_NoError;
                UPackage::SavePackage(
                    Existing->GetOutermost(),
                    Existing,
                    *PackageFilename,
                    SaveArgs);
            }
            return Existing;
        }

        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            return nullptr;
        }
        UStaticMesh* Mesh = NewObject<UStaticMesh>(Package, FName(Spec.AssetName), RF_Public | RF_Standalone);
        if (!Mesh)
        {
            return nullptr;
        }

        FMeshDescription MeshDescription;
        FStaticMeshAttributes Attributes(MeshDescription);
        Attributes.Register();
        FMeshDescriptionBuilder Builder;
        Builder.SetMeshDescription(&MeshDescription);
        Builder.SetAllEdgesHardness(true);
        const FPolygonGroupID PolygonGroup = Builder.AppendPolygonGroup(FName(TEXT("Weapon")));
        BuildWeaponMeshDescription(Builder, PolygonGroup, Spec);

        TArray<FStaticMaterial> Materials;
        FStaticMaterial& Material = Materials.AddDefaulted_GetRef();
        Material.MaterialInterface = LoadObject<UMaterialInterface>(
            nullptr,
            TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        Material.MaterialSlotName = FName(TEXT("Weapon"));
        Material.ImportedMaterialSlotName = FName(TEXT("Weapon"));
        Mesh->SetStaticMaterials(Materials);

        UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
        BuildParams.bMarkPackageDirty = true;
        BuildParams.bUseHashAsGuid = true;
        BuildParams.bBuildSimpleCollision = true;
        BuildParams.bCommitMeshDescription = true;
        BuildParams.bFastBuild = false;
        if (!Mesh->BuildFromMeshDescriptions({&MeshDescription}, BuildParams))
        {
            return nullptr;
        }

        EnsureWeaponSockets(Mesh, Spec);

        FAssetRegistryModule::AssetCreated(Mesh);
        Package->MarkPackageDirty();
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Package, Mesh, *PackageFilename, SaveArgs))
        {
            return nullptr;
        }
        return Mesh;
    }

    UBMWeaponDataAsset* CreateWeaponDataAsset(
        const FBlackMeridianWeaponMeshSpec& Spec,
        UStaticMesh* PresentationMesh)
    {
        if (!PresentationMesh)
        {
            return nullptr;
        }

        FString DataAssetName(Spec.AssetName);
        DataAssetName.ReplaceInline(TEXT("SM_BM_"), TEXT("DA_BM_"));
        const FString PackageName = FString::Printf(TEXT("/Game/Data/Weapons/%s"), *DataAssetName);
        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            PackageName,
            FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFilename), true);
        const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, *DataAssetName);
        const FString MeshObjectPath = PresentationMesh->GetPathName();

        UBMWeaponDataAsset* Asset = LoadObject<UBMWeaponDataAsset>(nullptr, *ObjectPath);
        if (!Asset)
        {
            UPackage* Package = CreatePackage(*PackageName);
            if (!Package)
            {
                return nullptr;
            }

            Asset = UBMWeaponDataAsset::CreateRuntimeProfile(Package, Spec.WeaponId);
            if (!Asset || !Asset->Rename(*DataAssetName, Package, REN_DontCreateRedirectors))
            {
                return nullptr;
            }
            Asset->SetFlags(RF_Public | RF_Standalone);
            FAssetRegistryModule::AssetCreated(Asset);
        }

        // The serialized profile owns the presentation seam. Runtime
        // fallback profiles remain available when this package is not cooked.
        Asset->WeaponId = Spec.WeaponId;
        // Keep generated content profiles in step with the source balance
        // defaults when an older package already exists. Without this small
        // migration, the runtime would keep the pre-pacing-pass reload values
        // even after the profiles were regenerated by automation.
        if (const UBMWeaponDataAsset* CurrentDefaults = UBMWeaponDataAsset::CreateRuntimeProfile(
                GetTransientPackage(),
                Spec.WeaponId))
        {
            Asset->Balance.ReloadDuration = CurrentDefaults->Balance.ReloadDuration;
            Asset->Balance.EmptyReloadDuration = CurrentDefaults->Balance.EmptyReloadDuration;
        }
        Asset->ThirdPersonMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshObjectPath));
        Asset->FirstPersonMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshObjectPath));
        Asset->GripSocketName = TEXT("Grip");
        Asset->MuzzleSocketName = TEXT("Muzzle");
        if (UBMAudioDataAsset* CombatAudio = CreateCombatAudioAsset())
        {
            Asset->AudioProfile = TSoftObjectPtr<UBMAudioDataAsset>(FSoftObjectPath(CombatAudio->GetPathName()));
            Asset->FireSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/Generated/BM_ARX4_Fire.BM_ARX4_Fire")));
            Asset->SuppressedFireSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/Generated/BM_Suppressed_Fire.BM_Suppressed_Fire")));
            Asset->DryFireSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/Generated/BM_DryFire.BM_DryFire")));
        }
        if (UBMVFXDataAsset* CombatVFX = CreateCombatVFXAsset())
        {
            Asset->VFXProfile = TSoftObjectPtr<UBMVFXDataAsset>(FSoftObjectPath(CombatVFX->GetPathName()));
        }
        Asset->MarkPackageDirty();

        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Asset->GetOutermost(), Asset, *PackageFilename, SaveArgs))
        {
            return nullptr;
        }
        return Asset;
    }

    UBMThrowableDataAsset* CreateThrowableDataAsset(const EBMWeaponId ThrowableId)
    {
        const bool bTactical = ThrowableId == EBMWeaponId::VeilGrenade;
        const FString DataAssetName = bTactical
            ? TEXT("DA_BM_Veil_Grenade")
            : TEXT("DA_BM_Frag_Grenade");
        const FBlackMeridianThrowableMeshSpec MeshSpec = bTactical
            ? FBlackMeridianThrowableMeshSpec{TEXT("SM_BM_Veil_Grenade"), EBlackMeridianThrowableMeshKind::Veil, EBMWeaponId::VeilGrenade}
            : FBlackMeridianThrowableMeshSpec{TEXT("SM_BM_Frag_Grenade"), EBlackMeridianThrowableMeshKind::Frag, EBMWeaponId::FragGrenade};
        UStaticMesh* PresentationMesh = CreateThrowableMeshAsset(MeshSpec);
        if (!PresentationMesh)
        {
            return nullptr;
        }
        const FString PackageName = FString::Printf(TEXT("/Game/Data/Throwables/%s"), *DataAssetName);
        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            PackageName,
            FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFilename), true);
        const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, *DataAssetName);

        UBMThrowableDataAsset* Asset = LoadObject<UBMThrowableDataAsset>(nullptr, *ObjectPath);
        if (!Asset)
        {
            UPackage* Package = CreatePackage(*PackageName);
            if (!Package)
            {
                return nullptr;
            }

            Asset = UBMThrowableDataAsset::CreateRuntimeProfile(Package, ThrowableId);
            if (!Asset || !Asset->Rename(*DataAssetName, Package, REN_DontCreateRedirectors))
            {
                return nullptr;
            }
            Asset->SetFlags(RF_Public | RF_Standalone);
            FAssetRegistryModule::AssetCreated(Asset);
        }

        Asset->ThrowableId = ThrowableId;
        Asset->Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(PresentationMesh->GetPathName()));
        if (UBMVFXDataAsset* CombatVFX = CreateCombatVFXAsset())
        {
            Asset->VFXProfile = TSoftObjectPtr<UBMVFXDataAsset>(FSoftObjectPath(CombatVFX->GetPathName()));
        }
        Asset->MarkPackageDirty();
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Asset->GetOutermost(), Asset, *PackageFilename, SaveArgs))
        {
            return nullptr;
        }
        return Asset;
    }
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianWeaponProfileTest,
    "BlackMeridian.Combat.WeaponProfiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianWeaponProfileTest::RunTest(const FString& Parameters)
{
    UBMWeaponDataAsset* ARX4 = UBMWeaponDataAsset::CreateRuntimeProfile(GetTransientPackage(), EBMWeaponId::ARX4);
    UBMWeaponDataAsset* Rook = UBMWeaponDataAsset::CreateRuntimeProfile(GetTransientPackage(), EBMWeaponId::RookDMR);
    UBMWeaponDataAsset* Arc = UBMWeaponDataAsset::CreateRuntimeProfile(GetTransientPackage(), EBMWeaponId::ArcLauncher);
    TestNotNull(TEXT("ARX-4 fallback profile exists"), ARX4);
    TestNotNull(TEXT("Rook fallback profile exists"), Rook);
    TestNotNull(TEXT("Arc-7 projectile profile exists"), Arc);
    if (ARX4 && Rook && Arc)
    {
        TestTrue(TEXT("DMR damage exceeds assault rifle damage"), Rook->Balance.BaseDamage > ARX4->Balance.BaseDamage);
        TestEqual(TEXT("ARX-4 headshot multiplier"), ARX4->GetHitZoneMultiplier(EBMHitZone::Head), 2.0f);
        TestTrue(TEXT("ARX-4 has attachment slots"), ARX4->SupportedAttachmentSlots.Contains(TEXT("Optic")));
        TestTrue(TEXT("Arc-7 uses the projectile path"), Arc->bProjectileWeapon && Arc->ProjectileClass != nullptr);
        TestNotNull(TEXT("ARX-4 has a soft-cover surface rule"), ARX4->FindSurfaceRule(EBMSurfaceType::SoftCover));
        TestFalse(TEXT("runtime fallback profiles are unsuppressed by default"), ARX4->bSuppressed);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianReloadProfileTimingTest,
    "BlackMeridian.Combat.ReloadProfileTiming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianReloadProfileTimingTest::RunTest(const FString& Parameters)
{
    struct FExpectedReloadProfile
    {
        EBMWeaponId WeaponId;
        const TCHAR* Label;
        float NormalSeconds;
        float EmptySeconds;
    };

    const FExpectedReloadProfile ExpectedProfiles[] = {
        {EBMWeaponId::ARX4, TEXT("ARX-4"), 0.45f, 0.65f},
        {EBMWeaponId::Kestrel9, TEXT("Kestrel-9"), 0.40f, 0.55f},
        {EBMWeaponId::RookDMR, TEXT("Rook DMR"), 0.70f, 0.90f},
        {EBMWeaponId::Breach12, TEXT("Breach-12"), 0.80f, 1.05f},
        {EBMWeaponId::SolacePistol, TEXT("Solace pistol"), 0.35f, 0.50f},
        {EBMWeaponId::ArcLauncher, TEXT("Arc-7 launcher"), 0.85f, 0.90f}};

    for (const FExpectedReloadProfile& Expected : ExpectedProfiles)
    {
        UBMWeaponDataAsset* Profile = UBMWeaponDataAsset::CreateRuntimeProfile(
            GetTransientPackage(),
            Expected.WeaponId);
        TestNotNull(
            FString::Printf(TEXT("reload profile exists: %s"), Expected.Label),
            Profile);
        if (!Profile)
        {
            continue;
        }

        TestTrue(
            FString::Printf(TEXT("normal reload timing is current: %s"), Expected.Label),
            FMath::IsNearlyEqual(Profile->Balance.ReloadDuration, Expected.NormalSeconds, 0.01f));
        TestTrue(
            FString::Printf(TEXT("empty reload timing is current: %s"), Expected.Label),
            FMath::IsNearlyEqual(Profile->Balance.EmptyReloadDuration, Expected.EmptySeconds, 0.01f));
        TestTrue(
            FString::Printf(TEXT("empty reload remains longer than normal reload: %s"), Expected.Label),
            Profile->Balance.EmptyReloadDuration >= Profile->Balance.ReloadDuration);
        TestTrue(
            FString::Printf(TEXT("normal reload stays below one second: %s"), Expected.Label),
            Profile->Balance.ReloadDuration < 1.0f);
        TestTrue(
            FString::Printf(TEXT("empty reload stays below tactical ceiling: %s"), Expected.Label),
            Profile->Balance.EmptyReloadDuration <= 1.10f);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianWeaponSpreadTest,
    "BlackMeridian.Combat.WeaponSpread",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianWeaponSpreadTest::RunTest(const FString& Parameters)
{
    UBMWeaponDataAsset* ARX4 = UBMWeaponDataAsset::CreateRuntimeProfile(
        GetTransientPackage(),
        EBMWeaponId::ARX4);
    TestNotNull(TEXT("ARX-4 spread profile exists"), ARX4);
    if (!ARX4)
    {
        return true;
    }

    const FVector Forward = FVector::ForwardVector;
    FRandomStream HipRandom(1337);
    const FVector HipDirection = ABMWeapon::GetSpreadDirection(
        Forward,
        FMath::DegreesToRadians(ARX4->Balance.HipSpreadDegrees),
        HipRandom);
    FRandomStream ZeroSpreadRandom(1337);
    const FVector ZeroSpreadDirection = ABMWeapon::GetSpreadDirection(
        Forward,
        0.0f,
        ZeroSpreadRandom);

    TestTrue(
        TEXT("single-pellet hip fire is not locked to the center ray"),
        !HipDirection.Equals(Forward, 0.000001f));
    TestTrue(
        TEXT("zero spread preserves the requested direction"),
        ZeroSpreadDirection.Equals(Forward, 0.000001f));
    TestTrue(
        TEXT("hip-fire spread is wider than ADS spread"),
        ARX4->Balance.HipSpreadDegrees > ARX4->Balance.ADSSpreadDegrees);

    const float HipAngle = FMath::RadiansToDegrees(
        FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, HipDirection), -1.0f, 1.0f)));
    TestTrue(
        TEXT("single-pellet hip-fire direction stays inside its configured cone"),
        HipAngle <= ARX4->Balance.HipSpreadDegrees + 0.001f);
    return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianWeaponAssetTest,
    "BlackMeridian.Combat.WeaponAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianWeaponAssetTest::RunTest(const FString& Parameters)
{
    const FBlackMeridianWeaponMeshSpec Specs[] = {
        {TEXT("SM_BM_ARX4_Rifle"), EBlackMeridianWeaponMeshKind::ARX4, EBMWeaponId::ARX4},
        {TEXT("SM_BM_Kestrel9_SMG"), EBlackMeridianWeaponMeshKind::Kestrel9, EBMWeaponId::Kestrel9},
        {TEXT("SM_BM_Rook_DMR"), EBlackMeridianWeaponMeshKind::RookDMR, EBMWeaponId::RookDMR},
        {TEXT("SM_BM_Breach12_Shotgun"), EBlackMeridianWeaponMeshKind::Breach12, EBMWeaponId::Breach12},
        {TEXT("SM_BM_Solace_Pistol"), EBlackMeridianWeaponMeshKind::SolacePistol, EBMWeaponId::SolacePistol},
        {TEXT("SM_BM_Arc7_Launcher"), EBlackMeridianWeaponMeshKind::ArcLauncher, EBMWeaponId::ArcLauncher}};

    for (const FBlackMeridianWeaponMeshSpec& Spec : Specs)
    {
        UStaticMesh* Mesh = CreateWeaponMeshAsset(Spec);
        TestNotNull(FString::Printf(TEXT("project weapon asset exists: %s"), Spec.AssetName), Mesh);
        if (Mesh)
        {
            TestTrue(
                FString::Printf(TEXT("weapon asset has renderable bounds: %s"), Spec.AssetName),
                Mesh->GetBounds().BoxExtent.SizeSquared() > 1.0f);
            TestNotNull(
                FString::Printf(TEXT("weapon asset has authored grip socket: %s"), Spec.AssetName),
                Mesh->FindSocket(TEXT("Grip")));
            TestNotNull(
                FString::Printf(TEXT("weapon asset has authored muzzle socket: %s"), Spec.AssetName),
                Mesh->FindSocket(TEXT("Muzzle")));

            UBMWeaponDataAsset* DataAsset = CreateWeaponDataAsset(Spec, Mesh);
            TestNotNull(
                FString::Printf(TEXT("serialized weapon data asset exists: %s"), Spec.AssetName),
                DataAsset);
            if (DataAsset)
            {
                TestEqual(
                    FString::Printf(TEXT("serialized profile keeps weapon id: %s"), Spec.AssetName),
                    static_cast<uint8>(DataAsset->WeaponId),
                    static_cast<uint8>(Spec.WeaponId));
                TestTrue(
                    FString::Printf(TEXT("serialized profile references presentation mesh: %s"), Spec.AssetName),
                    DataAsset->ThirdPersonMesh.ToSoftObjectPath().ToString() == Mesh->GetPathName());
            }

            // Exercise the same runtime ID-to-asset mapping used by an
            // attached enemy, rather than only checking that a package exists.
            ABMWeapon* RuntimeWeapon = NewObject<ABMWeapon>(GetTransientPackage());
            TestNotNull(
                FString::Printf(TEXT("runtime weapon object exists: %s"), Spec.AssetName),
                RuntimeWeapon);
            if (RuntimeWeapon)
            {
                RuntimeWeapon->SetThirdPersonPresentation(true);
                UBMWeaponDataAsset* LoadedProfile = UBMWeaponDataAsset::LoadContentProfile(RuntimeWeapon, Spec.WeaponId);
                TestEqual(
                    FString::Printf(TEXT("runtime resolves serialized profile: %s"), Spec.AssetName),
                    LoadedProfile,
                    DataAsset);
                RuntimeWeapon->InitializeFromData(LoadedProfile);
                TestNotNull(
                    FString::Printf(TEXT("runtime weapon has presentation mesh: %s"), Spec.AssetName),
                    RuntimeWeapon->WeaponMesh.Get());
                if (RuntimeWeapon->WeaponMesh)
                {
                    TestEqual(
                        FString::Printf(TEXT("runtime weapon resolves project mesh: %s"), Spec.AssetName),
                        RuntimeWeapon->WeaponMesh->GetStaticMesh().Get(),
                        Mesh);
                    TestTrue(
                        FString::Printf(TEXT("runtime weapon exposes authored muzzle: %s"), Spec.AssetName),
                        RuntimeWeapon->GetMuzzleLocation().SizeSquared() > 1.0f);
                    if (Mesh->FindSocket(TEXT("Grip")) && RuntimeWeapon->WeaponMesh->DoesSocketExist(TEXT("Grip")))
                    {
                        TestTrue(
                            FString::Printf(TEXT("runtime grip resolves from authored socket: %s"), Spec.AssetName),
                            FVector::DistSquared(
                                RuntimeWeapon->GetGripLocation(),
                                RuntimeWeapon->WeaponMesh->GetSocketLocation(TEXT("Grip"))) < 0.01f);
                    }
                    if (Mesh->FindSocket(TEXT("Muzzle")) && RuntimeWeapon->WeaponMesh->DoesSocketExist(TEXT("Muzzle")))
                    {
                        TestTrue(
                            FString::Printf(TEXT("runtime muzzle resolves from authored socket: %s"), Spec.AssetName),
                            FVector::DistSquared(
                                RuntimeWeapon->GetMuzzleLocation(),
                                RuntimeWeapon->WeaponMesh->GetSocketLocation(TEXT("Muzzle"))) < 0.01f);
                    }
                }
            }
        }
    }
    return true;
}
#endif

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianCombatAudioAssetTest,
    "BlackMeridian.Presentation.CombatAudioAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianCombatAudioAssetTest::RunTest(const FString& Parameters)
{
    UBMAudioDataAsset* Asset = CreateCombatAudioAsset();
    TestNotNull(TEXT("serialized combat audio profile exists"), Asset);
    if (!Asset)
    {
        return false;
    }

    const auto TestCue = [this, Asset](const TCHAR* Label, const TSoftObjectPtr<USoundBase>& Cue)
    {
        TestFalse(FString::Printf(TEXT("combat cue is assigned: %s"), Label), Cue.IsNull());
        TestNotNull(FString::Printf(TEXT("combat cue loads: %s"), Label), Cue.LoadSynchronous());
    };
    TestCue(TEXT("outdoor gunshot"), Asset->OutdoorGunshot);
    TestCue(TEXT("suppressed gunshot"), Asset->SuppressedGunshot);
    TestCue(TEXT("reload"), Asset->Reload);
    TestCue(TEXT("concrete impact"), Asset->BulletImpact);
    TestCue(TEXT("music suspense"), Asset->FindMusicState(TEXT("Suspense")));
    TestCue(TEXT("music combat"), Asset->FindMusicState(TEXT("Combat")));
    TestCue(TEXT("metal footstep"), Asset->FindFootstep(EBMSurfaceType::Metal));
    return true;
}
#endif

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianCombatVFXAssetTest,
    "BlackMeridian.Presentation.CombatVFXAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianCombatVFXAssetTest::RunTest(const FString& Parameters)
{
    UBMVFXDataAsset* Asset = CreateCombatVFXAsset();
    TestNotNull(TEXT("serialized combat VFX profile exists"), Asset);
    if (!Asset)
    {
        return false;
    }

    struct FVFXSlot
    {
        const TCHAR* Label;
        const TSoftObjectPtr<UNiagaraSystem>& Reference;
    };
    const FVFXSlot Slots[] = {
        {TEXT("muzzle flash"), Asset->MuzzleFlash},
        {TEXT("bullet impact"), Asset->BulletImpact},
        {TEXT("blood impact"), Asset->BloodImpact},
        {TEXT("explosion"), Asset->Explosion},
        {TEXT("dust"), Asset->Dust},
        {TEXT("smoke"), Asset->Smoke},
        {TEXT("sparks"), Asset->Sparks},
        {TEXT("shell ejection"), Asset->ShellEjection},
        {TEXT("suppression"), Asset->Suppression}};

    for (const FVFXSlot& Slot : Slots)
    {
        TestFalse(FString::Printf(TEXT("VFX slot is assigned: %s"), Slot.Label), Slot.Reference.IsNull());
        TestTrue(
            FString::Printf(TEXT("VFX slot is project-owned: %s"), Slot.Label),
            Slot.Reference.ToSoftObjectPath().ToString().StartsWith(TEXT("/Game/VFX/Generated/")));
        UNiagaraSystem* System = Slot.Reference.LoadSynchronous();
        TestNotNull(FString::Printf(TEXT("VFX slot loads: %s"), Slot.Label), System);
        if (System)
        {
            TestTrue(
                FString::Printf(TEXT("VFX slot has emitter handles: %s"), Slot.Label),
                System->GetEmitterHandles().Num() > 0);
            TestTrue(
                FString::Printf(TEXT("VFX slot is valid: %s"), Slot.Label),
                System->IsValid());
        }
    }

    UBMWeaponDataAsset* Weapon = LoadObject<UBMWeaponDataAsset>(
        nullptr,
        TEXT("/Game/Data/Weapons/DA_BM_ARX4_Rifle.DA_BM_ARX4_Rifle"));
    TestNotNull(TEXT("ARX-4 profile exists for VFX wiring"), Weapon);
    if (Weapon)
    {
        TestEqual(
            TEXT("ARX-4 profile references combat VFX"),
            Weapon->VFXProfile.ToSoftObjectPath(),
            FSoftObjectPath(Asset->GetPathName()));
    }

    UBMThrowableDataAsset* Frag = LoadObject<UBMThrowableDataAsset>(
        nullptr,
        TEXT("/Game/Data/Throwables/DA_BM_Frag_Grenade.DA_BM_Frag_Grenade"));
    TestNotNull(TEXT("frag profile exists for VFX wiring"), Frag);
    if (Frag)
    {
        TestEqual(
            TEXT("frag profile references combat VFX"),
            Frag->VFXProfile.ToSoftObjectPath(),
            FSoftObjectPath(Asset->GetPathName()));
    }
    return true;
}
#endif

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianCharacterPresentationAssetTest,
    "BlackMeridian.Presentation.CharacterAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianCharacterPresentationAssetTest::RunTest(const FString& Parameters)
{
    USkeletalMesh* MannyMesh = LoadObject<USkeletalMesh>(
        nullptr,
        TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    TestNotNull(TEXT("project-cooked Manny skeletal mesh exists"), MannyMesh);
    if (MannyMesh)
    {
        TestTrue(TEXT("Manny has a hand_r weapon anchor bone"), MannyMesh->GetRefSkeleton().FindBoneIndex(FName(TEXT("hand_r"))) != INDEX_NONE);
        TestTrue(TEXT("Manny has a visible reference skeleton"), MannyMesh->GetBounds().SphereRadius > 1.0f);
    }

    const TCHAR* RifleAnimationPaths[] = {
        TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS.MF_Rifle_Idle_ADS"),
        TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd.MF_Rifle_Walk_Fwd"),
        TEXT("/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Fwd.MF_Rifle_Jog_Fwd"),
        TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Fire.MM_Rifle_Fire"),
        TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload.MM_Rifle_Reload")};
    for (const TCHAR* AnimationPath : RifleAnimationPaths)
    {
        UAnimSequence* Animation = LoadObject<UAnimSequence>(nullptr, AnimationPath);
        TestNotNull(FString::Printf(TEXT("project rifle animation exists: %s"), AnimationPath), Animation);
        if (Animation)
        {
            TestTrue(
                FString::Printf(TEXT("project rifle animation has duration: %s"), AnimationPath),
                Animation->GetPlayLength() > 0.05f);
        }
    }
    return true;
}
#endif

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianCharacterAnimationDataAssetTest,
    "BlackMeridian.Presentation.CharacterAnimationData",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianCharacterAnimationDataAssetTest::RunTest(const FString& Parameters)
{
    UBMCharacterAnimationDataAsset* Asset = CreateCharacterAnimationAsset();
    TestNotNull(TEXT("serialized Manny rifle animation data asset exists"), Asset);
    if (!Asset)
    {
        return false;
    }

    TestNotNull(
        TEXT("project-owned Manny rifle AnimBP generated class exists"),
        Asset->AnimBlueprintClass.Get());

    const FName LocomotionNames[] = {
        TEXT("Idle"),
        TEXT("Walk"),
        TEXT("Sprint"),
        TEXT("Jump"),
        TEXT("Fall")};
    for (const FName StateName : LocomotionNames)
    {
        const TSoftObjectPtr<UAnimSequence> Sequence = Asset->FindLocomotionSequence(StateName);
        TestFalse(
            FString::Printf(TEXT("locomotion sequence is assigned: %s"), *StateName.ToString()),
            Sequence.IsNull());
        TestNotNull(
            FString::Printf(TEXT("locomotion sequence loads: %s"), *StateName.ToString()),
            Sequence.LoadSynchronous());
    }

    const FName EventNames[] = {
        TEXT("Fire"),
        TEXT("ReloadStarted"),
        TEXT("DryFire"),
        TEXT("HitReact"),
        TEXT("HeadHitReact"),
        TEXT("Death")};
    for (const FName EventName : EventNames)
    {
        const TSoftObjectPtr<UAnimSequence> Sequence = Asset->FindEventSequence(EventName);
        TestFalse(
            FString::Printf(TEXT("event sequence is assigned: %s"), *EventName.ToString()),
            Sequence.IsNull());
        TestNotNull(
            FString::Printf(TEXT("event sequence loads: %s"), *EventName.ToString()),
            Sequence.LoadSynchronous());
    }
    return true;
}
#endif

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianThrowableAssetTest,
    "BlackMeridian.Combat.ThrowableAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianThrowableAssetTest::RunTest(const FString& Parameters)
{
    const EBMWeaponId ThrowableIds[] = {EBMWeaponId::FragGrenade, EBMWeaponId::VeilGrenade};
    for (const EBMWeaponId ThrowableId : ThrowableIds)
    {
        UBMThrowableDataAsset* DataAsset = CreateThrowableDataAsset(ThrowableId);
        TestNotNull(TEXT("serialized throwable data asset exists"), DataAsset);
        if (!DataAsset)
        {
            continue;
        }

        UBMThrowableDataAsset* LoadedProfile = UBMThrowableDataAsset::LoadContentProfile(
            GetTransientPackage(),
            ThrowableId);
        TestEqual(TEXT("runtime resolves serialized throwable profile"), LoadedProfile, DataAsset);
        TestTrue(TEXT("throwable has a bounded fuse"), DataAsset->FuseSeconds > 0.0f);
        TestTrue(TEXT("throwable has a presentation mesh reference"), !DataAsset->Mesh.IsNull());
        UStaticMesh* PresentationMesh = DataAsset->Mesh.LoadSynchronous();
        TestNotNull(TEXT("throwable presentation mesh loads"), PresentationMesh);
        if (PresentationMesh)
        {
            TestTrue(TEXT("throwable presentation mesh has renderable bounds"), PresentationMesh->GetBounds().BoxExtent.SizeSquared() > 1.0f);
            TestNotNull(TEXT("throwable presentation mesh has authored grip socket"), PresentationMesh->FindSocket(TEXT("Grip")));
        }
        if (ThrowableId == EBMWeaponId::FragGrenade)
        {
            TestTrue(TEXT("frag has authoritative damage"), DataAsset->Damage > 0.0f);
        }
        else
        {
            TestTrue(TEXT("Veil carries suppression instead of damage"), DataAsset->SuppressionAmount > 0.0f);
        }
    }
    return true;
}
#endif

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianEnvironmentAssetTest,
    "BlackMeridian.World.EnvironmentAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianEnvironmentAssetTest::RunTest(const FString& Parameters)
{
    UMaterialInterface* EnvironmentMaterial = CreateEnvironmentMaterialAsset();
    TestNotNull(TEXT("project environment material exists"), EnvironmentMaterial);
    if (!EnvironmentMaterial)
    {
        return false;
    }

    const FBlackMeridianEnvironmentMeshSpec Specs[] = {
        {TEXT("SM_BM_Env_Block"), EBlackMeridianEnvironmentMeshKind::Block},
        {TEXT("SM_BM_Env_Pipe"), EBlackMeridianEnvironmentMeshKind::Pipe},
        {TEXT("SM_BM_Env_Orb"), EBlackMeridianEnvironmentMeshKind::Orb},
        {TEXT("SM_BM_Env_Deck"), EBlackMeridianEnvironmentMeshKind::Deck},
        {TEXT("SM_BM_Env_Truss"), EBlackMeridianEnvironmentMeshKind::Truss},
        {TEXT("SM_BM_Env_Grate"), EBlackMeridianEnvironmentMeshKind::Grate},
        {TEXT("SM_BM_Env_Panel"), EBlackMeridianEnvironmentMeshKind::Panel},
        {TEXT("SM_BM_Env_Cover"), EBlackMeridianEnvironmentMeshKind::Cover},
        {TEXT("SM_BM_Env_Door"), EBlackMeridianEnvironmentMeshKind::Door},
        {TEXT("SM_BM_Env_Window"), EBlackMeridianEnvironmentMeshKind::Window},
        {TEXT("SM_BM_Env_Sign"), EBlackMeridianEnvironmentMeshKind::Sign},
        {TEXT("SM_BM_Env_Machinery"), EBlackMeridianEnvironmentMeshKind::Machinery},
        {TEXT("SM_BM_Env_Wreckage"), EBlackMeridianEnvironmentMeshKind::Wreckage}};

    for (const FBlackMeridianEnvironmentMeshSpec& Spec : Specs)
    {
        UStaticMesh* Mesh = CreateEnvironmentMeshAsset(Spec, EnvironmentMaterial);
        TestNotNull(FString::Printf(TEXT("project environment asset exists: %s"), Spec.AssetName), Mesh);
        if (!Mesh)
        {
            continue;
        }

        TestTrue(
            FString::Printf(TEXT("environment asset has renderable bounds: %s"), Spec.AssetName),
            Mesh->GetBounds().BoxExtent.SizeSquared() > 1.0f);
        TestTrue(
            FString::Printf(TEXT("environment asset has a render LOD: %s"), Spec.AssetName),
            Mesh->GetNumLODs() > 0);
        const TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
        TestTrue(
            FString::Printf(TEXT("environment asset has a material slot: %s"), Spec.AssetName),
            Materials.Num() > 0);
        if (Materials.Num() > 0)
        {
            TestEqual(
                FString::Printf(TEXT("environment asset uses the project material: %s"), Spec.AssetName),
                Materials[0].MaterialInterface.Get(),
                EnvironmentMaterial);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianEnvironmentThemeAssetTest,
    "BlackMeridian.World.EnvironmentThemeAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianEnvironmentThemeAssetTest::RunTest(const FString& Parameters)
{
    UBMAudioDataAsset* Audio = CreateCombatAudioAsset();
    TestNotNull(TEXT("campaign environment audio profile exists"), Audio);
    if (!Audio)
    {
        return false;
    }

    const UNiagaraSystem* Rain = LoadObject<UNiagaraSystem>(
        nullptr,
        TEXT("/Game/VFX/Generated/NS_BM_Rain.NS_BM_Rain"));
    const UNiagaraSystem* Snow = LoadObject<UNiagaraSystem>(
        nullptr,
        TEXT("/Game/VFX/Generated/NS_BM_Snow.NS_BM_Snow"));
    TestNotNull(TEXT("dedicated rain Niagara system loads"), Rain);
    TestNotNull(TEXT("dedicated snow Niagara system loads"), Snow);

    for (int32 Index = static_cast<int32>(EBMCampaignLevel::SableQuay);
         Index <= static_cast<int32>(EBMCampaignLevel::LastLight);
         ++Index)
    {
        const FBMCampaignLevelDefinition Definition = GetCampaignLevelDefinition(static_cast<EBMCampaignLevel>(Index));
        const TSoftObjectPtr<USoundBase> EnvironmentSound = Audio->FindEnvironmentAmbience(Definition.EnvironmentAmbienceId);
        TestFalse(
            FString::Printf(TEXT("environment ambience is routed: %s"), *Definition.EnvironmentThemeId.ToString()),
            EnvironmentSound.IsNull());
        TestNotNull(
            FString::Printf(TEXT("environment ambience loads: %s"), *Definition.EnvironmentThemeId.ToString()),
            EnvironmentSound.LoadSynchronous());
        TestTrue(
            FString::Printf(TEXT("terrain material response is bounded: %s"), *Definition.EnvironmentThemeId.ToString()),
            Definition.EnvironmentTerrainMetallic >= 0.0f
                && Definition.EnvironmentTerrainMetallic <= 1.0f
                && Definition.EnvironmentTerrainRoughness >= 0.0f
                && Definition.EnvironmentTerrainRoughness <= 1.0f);
        if (Definition.WeatherType == EBMWeatherType::Rain)
        {
            TestEqual(
                FString::Printf(TEXT("rain profile selects dedicated system: %s"), *Definition.EnvironmentThemeId.ToString()),
                Definition.EnvironmentWeatherSystemId,
                FName(TEXT("NS_BM_Rain")));
            TestEqual(
                FString::Printf(TEXT("rain profile selects dedicated path: %s"), *Definition.EnvironmentThemeId.ToString()),
                Definition.EnvironmentWeatherSystemPath,
                FString(TEXT("/Game/VFX/Generated/NS_BM_Rain.NS_BM_Rain")));
        }
        if (Definition.WeatherType == EBMWeatherType::Snow)
        {
            TestEqual(
                FString::Printf(TEXT("snow profile selects dedicated system: %s"), *Definition.EnvironmentThemeId.ToString()),
                Definition.EnvironmentWeatherSystemId,
                FName(TEXT("NS_BM_Snow")));
            TestEqual(
                FString::Printf(TEXT("snow profile selects dedicated path: %s"), *Definition.EnvironmentThemeId.ToString()),
                Definition.EnvironmentWeatherSystemPath,
                FString(TEXT("/Game/VFX/Generated/NS_BM_Snow.NS_BM_Snow")));
        }
    }
    return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianAmmunitionTest,
    "BlackMeridian.Combat.Ammunition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianAmmunitionTest::RunTest(const FString& Parameters)
{
    UBMAmmunitionComponent* Ammo = NewObject<UBMAmmunitionComponent>(GetTransientPackage());
    Ammo->Initialize(3, 5);
    TestEqual(TEXT("initial magazine"), Ammo->GetMagazineAmmo(), 3);
    TestEqual(TEXT("initial reserve"), Ammo->GetReserveAmmo(), 5);
    TestTrue(TEXT("round consumed"), Ammo->ConsumeRound());
    TestEqual(TEXT("magazine after shot"), Ammo->GetMagazineAmmo(), 2);
    TestTrue(TEXT("reload starts"), Ammo->BeginReload());
    Ammo->CompleteReload();
    TestEqual(TEXT("reload fills magazine"), Ammo->GetMagazineAmmo(), 3);
    TestEqual(TEXT("reload consumes reserve"), Ammo->GetReserveAmmo(), 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianDestructiblePropTest,
    "BlackMeridian.World.DestructibleProp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianDestructiblePropTest::RunTest(const FString& Parameters)
{
    ABMArtDirectedProp* Prop = NewObject<ABMArtDirectedProp>(GetTransientPackage());
    TestNotNull(TEXT("destructible prop object exists"), Prop);
    if (!Prop)
    {
        return false;
    }

    Prop->ConfigureRuntime(
        EBMArtDirectedPropVariant::Barricade,
        FLinearColor(0.28f, 0.20f, 0.12f, 1.0f),
        FLinearColor(0.78f, 0.32f, 0.08f, 1.0f),
        true);
    TestTrue(TEXT("prop accepts the destructible configuration"), Prop->IsDestructible());
    TestFalse(TEXT("prop starts intact"), Prop->IsDestroyed());
    TestFalse(TEXT("partial weapon damage leaves cover intact"), Prop->ApplyWeaponImpact(30.0f));
    TestFalse(TEXT("partial weapon damage does not destroy cover"), Prop->IsDestroyed());
    TestTrue(TEXT("lethal weapon damage destroys cover"), Prop->ApplyWeaponImpact(70.0f));
    TestTrue(TEXT("destroyed state is observable"), Prop->IsDestroyed());
    TestFalse(TEXT("destroyed cover cannot be damaged twice"), Prop->ApplyWeaponImpact(100.0f));
    Prop->ResetProp();
    TestFalse(TEXT("checkpoint reset restores intact cover"), Prop->IsDestroyed());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianSignalTowerPresentationTest,
    "BlackMeridian.World.SignalTowerPresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianSignalTowerPresentationTest::RunTest(const FString& Parameters)
{
    ABMArtDirectedProp* Tower = NewObject<ABMArtDirectedProp>(GetTransientPackage());
    TestNotNull(TEXT("signal tower object exists"), Tower);
    if (!Tower)
    {
        return false;
    }

    Tower->ConfigureRuntime(
        EBMArtDirectedPropVariant::SignalTower,
        FLinearColor(0.18f, 0.23f, 0.27f, 1.0f),
        FLinearColor(0.03f, 0.56f, 0.58f, 1.0f),
        false);

    TestTrue(TEXT("signal tower has a beacon light"), Tower->HasSignalBeaconLight());
    TestTrue(TEXT("signal tower beacon is lit"), Tower->GetSignalBeaconIntensity() > 0.0f);
    TestFalse(TEXT("signal tower is not destructible by default"), Tower->IsDestructible());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianDamageTypeTest,
    "BlackMeridian.Combat.DamageContext",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianDamageTypeTest::RunTest(const FString& Parameters)
{
    FBMDamageContext Context;
    Context.RawDamage = 30.0f;
    Context.HitZone = EBMHitZone::Head;
    Context.bHeadshot = true;
    TestTrue(TEXT("headshot context remains explicit"), Context.bHeadshot && Context.HitZone == EBMHitZone::Head);
    TestEqual(TEXT("default mission phase"), static_cast<uint8>(EBMMissionPhase::Intro), static_cast<uint8>(0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianReloadInterruptionTest,
    "BlackMeridian.Combat.ReloadInterruption",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianReloadInterruptionTest::RunTest(const FString& Parameters)
{
    UBMAmmunitionComponent* Ammo = NewObject<UBMAmmunitionComponent>(GetTransientPackage());
    Ammo->Initialize(3, 5);
    TestTrue(TEXT("round is removed before interruption"), Ammo->ConsumeRound());
    TestTrue(TEXT("reload starts before interruption"), Ammo->BeginReload());
    Ammo->CancelReload();
    TestFalse(TEXT("interrupted reload leaves the component idle"), Ammo->IsReloading());
    TestEqual(TEXT("interrupted reload does not restore the magazine"), Ammo->GetMagazineAmmo(), 2);
    TestEqual(TEXT("interrupted reload does not consume reserve"), Ammo->GetReserveAmmo(), 5);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianAccuracyRecoveryTest,
    "BlackMeridian.Combat.AccuracyRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianAccuracyRecoveryTest::RunTest(const FString& Parameters)
{
    UBMFireModeComponent* FireMode = NewObject<UBMFireModeComponent>(GetTransientPackage());
    TestNotNull(TEXT("fire mode component exists"), FireMode);
    if (!FireMode)
    {
        return false;
    }

    FireMode->RegisterShot(0.0f, 1.0f, 1.0f, 3.0f);
    TestTrue(TEXT("shot adds spread penalty"), FMath::IsNearlyEqual(FireMode->GetCurrentAccuracyPenalty(), 1.0f));
    TestTrue(TEXT("penalty recovers over time"), FMath::IsNearlyEqual(FireMode->GetAccuracyPenalty(0.5f, 1.0f), 0.5f));
    TestTrue(TEXT("penalty does not go below zero"), FMath::IsNearlyZero(FireMode->GetAccuracyPenalty(2.0f, 1.0f)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianFireRateTest,
    "BlackMeridian.Combat.FireRate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianFireRateTest::RunTest(const FString& Parameters)
{
    UBMFireModeComponent* FireMode = NewObject<UBMFireModeComponent>(GetTransientPackage());
    TestTrue(TEXT("first shot is accepted"), FireMode->CanFire(0.0f, 1.0f, EBMWeaponFireMode::SemiAutomatic));
    TestFalse(TEXT("shot inside fire interval is rejected"), FireMode->CanFire(0.5f, 1.0f, EBMWeaponFireMode::SemiAutomatic));
    TestTrue(TEXT("shot after fire interval is accepted"), FireMode->CanFire(1.01f, 1.0f, EBMWeaponFireMode::SemiAutomatic));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianHostileDamagePacingTest,
    "BlackMeridian.Combat.HostileDamagePacing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianHostileDamagePacingTest::RunTest(const FString& Parameters)
{
    TestTrue(
        TEXT("squad fire slot leaves a readable incoming-fire gap"),
        BMCombatTuning::SquadFireSlotIntervalSeconds >= 0.30f);
    TestTrue(
        TEXT("opening wave gives the player a reaction window"),
        BMCombatTuning::OpeningWaveMinimumStartDelaySeconds >= 1.50f);
    TestTrue(
        TEXT("opening wave keeps marksman aim readable"),
        BMCombatTuning::OpeningWaveMarksmanAimErrorDegrees >= 2.50f);
    TestTrue(
        TEXT("opening wave reduces every role's torso damage"),
        BMCombatTuning::GetOpeningWaveDamageMultiplier(EBMEnemyRole::Rifleman) <= 0.35f
            && BMCombatTuning::GetOpeningWaveDamageMultiplier(EBMEnemyRole::Assault) <= 0.35f
            && BMCombatTuning::GetOpeningWaveDamageMultiplier(EBMEnemyRole::Marksman) <= 0.35f
            && BMCombatTuning::GetOpeningWaveDamageMultiplier(EBMEnemyRole::Breacher) <= 0.35f
            && BMCombatTuning::GetOpeningWaveDamageMultiplier(EBMEnemyRole::Demolitionist) <= 0.35f);

    const TArray<TPair<EBMEnemyRole, EBMWeaponId>> OpeningLoadout = {
        {EBMEnemyRole::Rifleman, EBMWeaponId::ARX4},
        {EBMEnemyRole::Assault, EBMWeaponId::Kestrel9},
        {EBMEnemyRole::Marksman, EBMWeaponId::RookDMR},
        {EBMEnemyRole::Breacher, EBMWeaponId::Breach12},
        {EBMEnemyRole::Demolitionist, EBMWeaponId::ArcLauncher}};
    float MaximumOpeningTorsoHit = 0.0f;
    for (const TPair<EBMEnemyRole, EBMWeaponId>& Entry : OpeningLoadout)
    {
        UBMWeaponDataAsset* Profile = UBMWeaponDataAsset::CreateRuntimeProfile(
            GetTransientPackage(),
            Entry.Value);
        TestNotNull(TEXT("opening role has a weapon profile"), Profile);
        if (!Profile)
        {
            continue;
        }

        const float PelletCount = static_cast<float>(FMath::Max(1, Profile->Balance.PelletCount));
        const float TorsoHit = Profile->Balance.BaseDamage
            * PelletCount
            * Profile->GetHitZoneMultiplier(EBMHitZone::Torso)
            * BMCombatTuning::GetOpeningWaveDamageMultiplier(Entry.Key);
        MaximumOpeningTorsoHit = FMath::Max(MaximumOpeningTorsoHit, TorsoHit);
    }
    TestTrue(
        TEXT("a single opening torso volley stays below the instant-kill threshold"),
        MaximumOpeningTorsoHit <= 40.0f);
    TestTrue(
        TEXT("hostile weapon actions have a non-lethal per-shot budget"),
        BMCombatTuning::HostileDamagePerShotCap < 100.0f);
    TestTrue(
        TEXT("hostile explosions have a non-lethal per-detonation budget"),
        BMCombatTuning::HostileExplosionDamageCap < 100.0f);
    TestTrue(
        TEXT("all hostile sources share a one-second damage budget"),
        BMCombatTuning::HostileDamageWindowSeconds >= 1.0f
            && BMCombatTuning::HostileDamageWindowCap <= 24.0f);
    TestTrue(
        TEXT("the shared budget cannot erase full health in one second"),
        BMCombatTuning::HostileDamageWindowCap < 100.0f);

    const float CriticalHealth = BMCombatTuning::PlayerCriticalHealthThreshold - 1.0f;
    const float CriticalShotDamage = BMCombatTuning::ResolvePlayerHostileDamage(
        CriticalHealth,
        12.0f,
        false);
    TestTrue(
        TEXT("critical-band hostile damage is reduced instead of deleting the player"),
        CriticalShotDamage < 24.0f && CriticalHealth - CriticalShotDamage > 0.0f);

    const float RecoveryShotDamage = BMCombatTuning::ResolvePlayerHostileDamage(
        BMCombatTuning::PlayerCriticalRecoveryMinimumHealth + 1.0f,
        24.0f,
        true);
    TestTrue(
        TEXT("critical recovery keeps a minimum reaction-health floor"),
        RecoveryShotDamage <= 1.0f);
    TestTrue(
        TEXT("critical recovery has a bounded duration and health cap"),
        BMCombatTuning::PlayerCriticalRecoveryDurationSeconds >= 0.75f
            && BMCombatTuning::PlayerCriticalRecoveryDurationSeconds <= 2.0f
            && BMCombatTuning::PlayerCriticalRecoveryHealthCap < 60.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianViewConeValidationTest,
    "BlackMeridian.Combat.ViewConeValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianViewConeValidationTest::RunTest(const FString& Parameters)
{
    TestTrue(
        TEXT("aligned player ray is accepted"),
        ABMWeapon::IsViewDirectionWithinCone(FVector::ForwardVector, FVector::ForwardVector));
    TestTrue(
        TEXT("ray at the configured tolerance edge is accepted"),
        ABMWeapon::IsViewDirectionWithinCone(
            FVector::ForwardVector,
            FRotator(0.0f, 9.0f, 0.0f).Vector()));
    TestFalse(
        TEXT("ray outside the tightened player view cone is rejected"),
        ABMWeapon::IsViewDirectionWithinCone(
            FVector::ForwardVector,
            FRotator(0.0f, 11.0f, 0.0f).Vector()));
    TestFalse(
        TEXT("ray outside the player view cone is rejected"),
        ABMWeapon::IsViewDirectionWithinCone(
            FVector::ForwardVector,
            FRotator(0.0f, 90.0f, 0.0f).Vector()));
    TestFalse(
        TEXT("behind-player ray is rejected"),
        ABMWeapon::IsViewDirectionWithinCone(FVector::ForwardVector, -FVector::ForwardVector));
    TestFalse(
        TEXT("zero-length ray is rejected"),
        ABMWeapon::IsViewDirectionWithinCone(FVector::ForwardVector, FVector::ZeroVector));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianInteractionContractTest,
    "BlackMeridian.Interaction.ActionContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianInteractionContractTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("ladder interaction action is distinct"), EBMInteractionAction::Ladder != EBMInteractionAction::Generic);
    TestTrue(TEXT("revive interaction action is distinct"), EBMInteractionAction::ReviveAlly != EBMInteractionAction::Generic);
    TestTrue(TEXT("mounted weapon interaction action is distinct"), EBMInteractionAction::MountedWeapon != EBMInteractionAction::Generic);
    TestTrue(TEXT("mission event interaction action is distinct"), EBMInteractionAction::MissionEvent != EBMInteractionAction::Generic);
    TestTrue(TEXT("throwable pickup interaction action is distinct"), EBMInteractionAction::PickupThrowable != EBMInteractionAction::Generic);
    TestEqual(TEXT("tap interaction remains the default first value"), static_cast<uint8>(EBMInteractionType::Tap), static_cast<uint8>(0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianPersistenceContractTest,
    "BlackMeridian.SaveSystem.CheckpointAndProfile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianPersistenceContractTest::RunTest(const FString& Parameters)
{
    UBMCheckpointSaveGame* Checkpoint = NewObject<UBMCheckpointSaveGame>(GetTransientPackage());
    UBMPlayerProfileSaveGame* Profile = NewObject<UBMPlayerProfileSaveGame>(GetTransientPackage());
    TestNotNull(TEXT("checkpoint object exists"), Checkpoint);
    TestNotNull(TEXT("profile object exists"), Profile);
    if (!Checkpoint || !Profile)
    {
        return false;
    }

    Checkpoint->Loadout = {EBMWeaponId::ARX4, EBMWeaponId::RookDMR};
    Checkpoint->CurrentWeapon = EBMWeaponId::RookDMR;
    Checkpoint->CampaignLevel = EBMCampaignLevel::NightglassCore;
    Checkpoint->bHasPlayerTransform = true;
    Checkpoint->bSignalJammerPlanted = true;
    Checkpoint->FragCount = 1;
    Checkpoint->VeilCount = 0;
    TestEqual(TEXT("checkpoint preserves loadout count"), Checkpoint->Loadout.Num(), 2);
    TestTrue(TEXT("checkpoint preserves active weapon"), Checkpoint->CurrentWeapon == EBMWeaponId::RookDMR);
    TestTrue(TEXT("checkpoint preserves campaign level"), Checkpoint->CampaignLevel == EBMCampaignLevel::NightglassCore);
    TestTrue(TEXT("checkpoint preserves jammer choice"), Checkpoint->bSignalJammerPlanted);
    TestEqual(TEXT("checkpoint preserves frag inventory"), Checkpoint->FragCount, 1);
    TestEqual(TEXT("checkpoint preserves Veil inventory"), Checkpoint->VeilCount, 0);
    TestTrue(TEXT("profile starts with a usable weapon"), Profile->UnlockedWeapons.Contains(EBMWeaponId::ARX4));
    Profile->CompletedCampaignLevels.AddUnique(EBMCampaignLevel::NightglassCore);
    TestTrue(TEXT("profile preserves completed campaign level"), Profile->CompletedCampaignLevels.Contains(EBMCampaignLevel::NightglassCore));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianCampaignLevelContractTest,
    "BlackMeridian.Campaign.LevelDefinitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianCampaignLevelContractTest::RunTest(const FString& Parameters)
{
    TSet<FName> LevelIds;
    TSet<EBMWeaponId> PickupWeapons;
    TSet<int32> EnvironmentVariants;
    TSet<FString> RouteSignatures;
    TSet<uint32> PaletteSignatures;
    TSet<uint8> EnvironmentThemes;
    TSet<uint8> WeatherTypes;
    TSet<FName> EnvironmentProfileIds;
    TSet<FName> EnvironmentTerrainIds;
    TSet<FName> EnvironmentLayoutIds;
    TSet<FName> EnvironmentPropSetIds;
    TSet<FName> EnvironmentSkyIds;
    TSet<FName> EnvironmentAmbienceIds;
    int32 PreviousCheckpointWave = 0;
    int32 PreviousOpenCombatWave = 0;
    int32 PreviousFinalDefenseWave = 0;

    for (int32 Index = static_cast<int32>(EBMCampaignLevel::SableQuay);
         Index <= static_cast<int32>(EBMCampaignLevel::LastLight);
         ++Index)
    {
        const EBMCampaignLevel Level = static_cast<EBMCampaignLevel>(Index);
        const FBMCampaignLevelDefinition Definition = GetCampaignLevelDefinition(Level);
        TestEqual(TEXT("definition preserves enum id"), static_cast<int32>(Definition.Level), Index);
        TestTrue(TEXT("level id is unique"), !LevelIds.Contains(Definition.LevelId));
        LevelIds.Add(Definition.LevelId);
        TestFalse(TEXT("start location is authored"), Definition.StartLocation.IsNearlyZero());
        TestFalse(TEXT("objective route is authored"), Definition.InsertionLocation.Equals(Definition.ExtractionLocation));
        const FString RouteSignature = FString::Printf(
            TEXT("%s|%s|%s|%s|%s|%s|%s"),
            *Definition.StartLocation.ToString(),
            *Definition.InsertionLocation.ToString(),
            *Definition.CheckpointLocation.ToString(),
            *Definition.BreachLocation.ToString(),
            *Definition.IntelLocation.ToString(),
            *Definition.OpenCombatLocation.ToString(),
            *Definition.ExtractionLocation.ToString());
        TestTrue(TEXT("route signature is unique"), !RouteSignatures.Contains(RouteSignature));
        RouteSignatures.Add(RouteSignature);
        TestTrue(TEXT("weapon pickup is a firearm"), Definition.PickupWeaponId != EBMWeaponId::FragGrenade && Definition.PickupWeaponId != EBMWeaponId::VeilGrenade);
        PickupWeapons.Add(Definition.PickupWeaponId);
        const uint32 PaletteSignature = static_cast<uint32>(Definition.EnvironmentPrimaryColor.R)
            | (static_cast<uint32>(Definition.EnvironmentPrimaryColor.G) << 8)
            | (static_cast<uint32>(Definition.EnvironmentPrimaryColor.B) << 16)
            | (static_cast<uint32>(Definition.EnvironmentAccentColor.R) << 24);
        TestTrue(TEXT("palette signature is unique"), !PaletteSignatures.Contains(PaletteSignature));
        PaletteSignatures.Add(PaletteSignature);
        TestTrue(TEXT("environment theme id is authored"), !Definition.EnvironmentThemeId.IsNone());
        TestTrue(TEXT("environment theme profile is unique"), !EnvironmentProfileIds.Contains(Definition.EnvironmentThemeId));
        EnvironmentProfileIds.Add(Definition.EnvironmentThemeId);
        TestTrue(TEXT("environment terrain id is authored"), !Definition.EnvironmentTerrainId.IsNone());
        TestTrue(TEXT("environment terrain profile is unique"), !EnvironmentTerrainIds.Contains(Definition.EnvironmentTerrainId));
        EnvironmentTerrainIds.Add(Definition.EnvironmentTerrainId);
        TestTrue(TEXT("environment layout id is authored"), !Definition.EnvironmentLayoutId.IsNone());
        TestTrue(TEXT("environment layout is unique"), !EnvironmentLayoutIds.Contains(Definition.EnvironmentLayoutId));
        EnvironmentLayoutIds.Add(Definition.EnvironmentLayoutId);
        TestTrue(TEXT("environment prop set id is authored"), !Definition.EnvironmentPropSetId.IsNone());
        TestTrue(TEXT("environment prop set is unique"), !EnvironmentPropSetIds.Contains(Definition.EnvironmentPropSetId));
        EnvironmentPropSetIds.Add(Definition.EnvironmentPropSetId);
        TestTrue(TEXT("environment sky id is authored"), !Definition.EnvironmentSkyId.IsNone());
        TestTrue(TEXT("environment sky profile is unique"), !EnvironmentSkyIds.Contains(Definition.EnvironmentSkyId));
        EnvironmentSkyIds.Add(Definition.EnvironmentSkyId);
        TestTrue(TEXT("environment ambience id is authored"), !Definition.EnvironmentAmbienceId.IsNone());
        TestTrue(TEXT("environment ambience route is unique"), !EnvironmentAmbienceIds.Contains(Definition.EnvironmentAmbienceId));
        EnvironmentAmbienceIds.Add(Definition.EnvironmentAmbienceId);
        TestTrue(TEXT("environment ambience source is authored"), !Definition.EnvironmentAmbientSoundPath.IsEmpty());
        TestTrue(TEXT("environment ground palette is non-black"), Definition.EnvironmentGroundColor.R > 0 || Definition.EnvironmentGroundColor.G > 0 || Definition.EnvironmentGroundColor.B > 0);
        TestTrue(TEXT("environment sky palette is non-black"), Definition.EnvironmentSkyColor.R > 0 || Definition.EnvironmentSkyColor.G > 0 || Definition.EnvironmentSkyColor.B > 0);
        EnvironmentThemes.Add(static_cast<uint8>(Definition.EnvironmentTheme));
        WeatherTypes.Add(static_cast<uint8>(Definition.WeatherType));
        TestTrue(TEXT("environment variant is unique"), !EnvironmentVariants.Contains(Definition.EnvironmentVariant));
        TestEqual(TEXT("environment variant follows level index"), Definition.EnvironmentVariant, Index);
        EnvironmentVariants.Add(Definition.EnvironmentVariant);
        TestTrue(TEXT("encounter budgets are positive"), Definition.CheckpointWaveSize > 0 && Definition.OpenCombatWaveSize > 0 && Definition.FinalDefenseWaveSize > 0);
        TestTrue(TEXT("encounter budgets do not decrease by level"), Definition.CheckpointWaveSize >= PreviousCheckpointWave && Definition.OpenCombatWaveSize >= PreviousOpenCombatWave && Definition.FinalDefenseWaveSize >= PreviousFinalDefenseWave);
        PreviousCheckpointWave = Definition.CheckpointWaveSize;
        PreviousOpenCombatWave = Definition.OpenCombatWaveSize;
        PreviousFinalDefenseWave = Definition.FinalDefenseWaveSize;
    }

    TestEqual(TEXT("campaign exposes twenty runtime levels"), LevelIds.Num(), 20);
    TestEqual(TEXT("campaign exposes twenty environment variants"), EnvironmentVariants.Num(), 20);
    TestEqual(TEXT("campaign exposes twenty unique routes"), RouteSignatures.Num(), 20);
    TestEqual(TEXT("campaign exposes twenty unique palettes"), PaletteSignatures.Num(), 20);
    TestEqual(TEXT("campaign exposes twenty environment profiles"), EnvironmentProfileIds.Num(), 20);
    TestEqual(TEXT("campaign exposes twenty terrain profiles"), EnvironmentTerrainIds.Num(), 20);
    TestEqual(TEXT("campaign exposes twenty structural layouts"), EnvironmentLayoutIds.Num(), 20);
    TestEqual(TEXT("campaign exposes twenty prop sets"), EnvironmentPropSetIds.Num(), 20);
    TestEqual(TEXT("campaign exposes twenty sky profiles"), EnvironmentSkyIds.Num(), 20);
    TestEqual(TEXT("campaign exposes twenty ambience routes"), EnvironmentAmbienceIds.Num(), 20);
    TestEqual(TEXT("campaign covers seven environment families"), EnvironmentThemes.Num(), 7);
    TestEqual(TEXT("campaign covers clear, rain, snow, dust, and mist weather"), WeatherTypes.Num(), 5);
    TestEqual(TEXT("campaign covers the six firearm pickups"), PickupWeapons.Num(), 6);
    TestTrue(TEXT("campaign selector lists level twenty"), GetBMCampaignLevelListText(EBMCampaignLevel::LastLight).Contains(TEXT("20 LASTLIGHT")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianProximityPickupContractTest,
    "BlackMeridian.Interaction.ProximityPickup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianProximityPickupContractTest::RunTest(const FString& Parameters)
{
    ABMInteractableActor* Pickup = NewObject<ABMInteractableActor>(GetTransientPackage());
    TestNotNull(TEXT("proximity pickup object exists"), Pickup);
    if (!Pickup)
    {
        return false;
    }

    Pickup->InteractionAction = EBMInteractionAction::PickupWeapon;
    Pickup->InteractionType = EBMInteractionType::Tap;
    Pickup->bAutoPickup = true;
    Pickup->AutoPickupRadius = 180.0f;
    Pickup->PickupWeaponId = EBMWeaponId::Breach12;
    Pickup->PickupAmmoAmount = 110;
    Pickup->bDestroyAfterInteraction = true;

    TestTrue(TEXT("weapon pickup is proximity enabled"), Pickup->bAutoPickup);
    TestTrue(TEXT("proximity radius is positive"), Pickup->AutoPickupRadius > 0.0f);
    TestTrue(TEXT("weapon pickup carries a firearm id"), Pickup->PickupWeaponId == EBMWeaponId::Breach12);
    TestTrue(TEXT("weapon pickup carries bonus ammunition"), Pickup->PickupAmmoAmount > 0);
    TestTrue(TEXT("successful pickup is consumable"), Pickup->bDestroyAfterInteraction);

    Pickup->InteractionAction = EBMInteractionAction::PickupThrowable;
    Pickup->PickupFragAmount = 1;
    Pickup->PickupVeilAmount = 1;
    TestTrue(TEXT("throwable pickup carries Frag resupply"), Pickup->PickupFragAmount > 0);
    TestTrue(TEXT("throwable pickup carries Veil resupply"), Pickup->PickupVeilAmount > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianSettingsContractTest,
    "BlackMeridian.UI.SettingsPersistence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianSettingsContractTest::RunTest(const FString& Parameters)
{
    UBMGameUserSettings* Settings = NewObject<UBMGameUserSettings>(GetTransientPackage());
    TestNotNull(TEXT("game settings object exists"), Settings);
    if (!Settings)
    {
        return false;
    }

    Settings->SetToDefaults();
    TestTrue(TEXT("default FOV is readable"), FMath::IsNearlyEqual(Settings->FieldOfView, 90.0f));
    TestTrue(TEXT("ADS sensitivity is independent from hip-fire"), Settings->ADSSensitivity < Settings->HipFireSensitivity);
    TestFalse(TEXT("reduced camera motion is opt-in"), Settings->bReducedCameraMotion);
    TestTrue(TEXT("audio channels have persisted defaults"), Settings->MasterVolume > 0.0f && Settings->EffectsVolume > 0.0f);
    TestEqual(TEXT("default graphics preset is High"), Settings->GraphicsQualityPreset, 2);
    TestTrue(TEXT("graphics preset range is scalable"), Settings->GraphicsQualityPreset >= 0 && Settings->GraphicsQualityPreset <= 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianInputContractTest,
    "BlackMeridian.Input.ActionBindings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianInputContractTest::RunTest(const FString& Parameters)
{
    const UInputSettings* InputSettings = GetDefault<UInputSettings>();
    TestNotNull(TEXT("legacy input settings exist"), InputSettings);
    if (!InputSettings)
    {
        return false;
    }

    const TArray<FInputActionKeyMapping> ActionMappings = InputSettings->GetActionMappings();
    const TArray<FInputAxisKeyMapping> AxisMappings = InputSettings->GetAxisMappings();
    const auto HasAction = [&ActionMappings](const FName ActionName)
    {
        return ActionMappings.ContainsByPredicate([ActionName](const FInputActionKeyMapping& Mapping)
        {
            return Mapping.ActionName == ActionName;
        });
    };
    const auto HasGamepadAction = [&ActionMappings](const FName ActionName)
    {
        return ActionMappings.ContainsByPredicate([ActionName](const FInputActionKeyMapping& Mapping)
        {
            return Mapping.ActionName == ActionName && Mapping.Key.IsGamepadKey();
        });
    };
    const auto HasKeyboardBinding = [&ActionMappings](const FName ActionName, const FKey& Key)
    {
        return ActionMappings.ContainsByPredicate([ActionName, &Key](const FInputActionKeyMapping& Mapping)
        {
            return Mapping.ActionName == ActionName && Mapping.Key == Key;
        });
    };
    const auto HasAxis = [&AxisMappings](const FName AxisName)
    {
        return AxisMappings.ContainsByPredicate([AxisName](const FInputAxisKeyMapping& Mapping)
        {
            return Mapping.AxisName == AxisName;
        });
    };

    for (const FName ActionName : {
             FName(TEXT("Fire")), FName(TEXT("Aim")), FName(TEXT("Reload")), FName(TEXT("Melee")),
             FName(TEXT("Frag")), FName(TEXT("Tactical")), FName(TEXT("Interact")), FName(TEXT("Sprint")),
             FName(TEXT("Crouch")), FName(TEXT("Prone")), FName(TEXT("Jump")), FName(TEXT("SwitchWeapon")),
             FName(TEXT("OpenControls")), FName(TEXT("TogglePause")), FName(TEXT("Scoreboard")),
             FName(TEXT("StartMission")), FName(TEXT("Back")), FName(TEXT("SettingsPrevious")),
             FName(TEXT("SettingsNext")), FName(TEXT("SettingsLeft")), FName(TEXT("SettingsRight"))})
    {
        TestTrue(*FString::Printf(TEXT("action mapping exists: %s"), *ActionName.ToString()), HasAction(ActionName));
    }
    for (const FName ActionName : {
             FName(TEXT("Fire")), FName(TEXT("Aim")), FName(TEXT("Interact")), FName(TEXT("Sprint")),
             FName(TEXT("Jump"))})
    {
        TestTrue(*FString::Printf(TEXT("controller mapping exists: %s"), *ActionName.ToString()), HasGamepadAction(ActionName));
    }
    for (const FName AxisName : {FName(TEXT("MoveForward")), FName(TEXT("MoveRight")), FName(TEXT("Turn")), FName(TEXT("LookUp"))})
    {
        TestTrue(*FString::Printf(TEXT("axis mapping exists: %s"), *AxisName.ToString()), HasAxis(AxisName));
    }
    TestTrue(TEXT("weapon switch keeps the 1 alias"), HasKeyboardBinding(TEXT("SwitchWeapon"), EKeys::One));
    TestTrue(TEXT("weapon switch owns the 2 alias"), HasKeyboardBinding(TEXT("SwitchWeapon"), EKeys::Two));
    TestTrue(TEXT("controls stays on F2"), HasKeyboardBinding(TEXT("OpenControls"), EKeys::F2));
    TestFalse(TEXT("controls does not steal weapon slot 2"), HasKeyboardBinding(TEXT("OpenControls"), EKeys::Two));
    TestTrue(TEXT("menu confirm has a controller A binding"), HasKeyboardBinding(TEXT("StartMission"), EKeys::Gamepad_FaceButton_Bottom));
    TestTrue(TEXT("menu back has a controller B binding"), HasKeyboardBinding(TEXT("Back"), EKeys::Gamepad_FaceButton_Right));
    TestTrue(TEXT("menu navigation has a controller up binding"), HasKeyboardBinding(TEXT("SettingsPrevious"), EKeys::Gamepad_DPad_Up));
    TestTrue(TEXT("menu navigation has a controller down binding"), HasKeyboardBinding(TEXT("SettingsNext"), EKeys::Gamepad_DPad_Down));
    TestTrue(TEXT("menu navigation has a controller left binding"), HasKeyboardBinding(TEXT("SettingsLeft"), EKeys::Gamepad_DPad_Left));
    TestTrue(TEXT("menu navigation has a controller right binding"), HasKeyboardBinding(TEXT("SettingsRight"), EKeys::Gamepad_DPad_Right));
    TestTrue(TEXT("movement state includes slide"), EBMCharacterMovementState::Slide != EBMCharacterMovementState::Walk);
    TestTrue(TEXT("movement state includes ladder"), EBMCharacterMovementState::Ladder != EBMCharacterMovementState::Walk);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianAIContractTest,
    "BlackMeridian.AI.Perception",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianAIContractTest::RunTest(const FString& Parameters)
{
    ABMEnemyAIController* Controller = NewObject<ABMEnemyAIController>(GetTransientPackage());
    TestNotNull(TEXT("enemy AI controller exists"), Controller);
    if (!Controller)
    {
        return false;
    }

    TestNotNull(TEXT("enemy AI owns a perception component"), Controller->FindComponentByClass<UAIPerceptionComponent>());
    TestTrue(TEXT("enemy roles include rifleman"), EBMEnemyRole::Rifleman != EBMEnemyRole::Marksman);
    TestTrue(TEXT("enemy roles include assault"), EBMEnemyRole::Assault != EBMEnemyRole::Marksman);
    TestTrue(TEXT("enemy roles include breacher"), EBMEnemyRole::Breacher != EBMEnemyRole::Marksman);
    TestTrue(TEXT("breacher is distinct from assault"), EBMEnemyRole::Breacher != EBMEnemyRole::Assault);
    TestTrue(TEXT("enemy roles include demolitionist"), EBMEnemyRole::Demolitionist != EBMEnemyRole::Marksman);
    TestTrue(TEXT("demolitionist is distinct from breacher"), EBMEnemyRole::Demolitionist != EBMEnemyRole::Breacher);
    TestTrue(TEXT("AI state includes investigation"), EBMAIState::Investigate != EBMAIState::Patrol);
    TestTrue(TEXT("AI state includes flanking"), EBMAIState::Flank != EBMAIState::Engage);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianInputRemapConflictTest,
    "BlackMeridian.Input.ConflictSafeRemapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianInputRemapConflictTest::RunTest(const FString& Parameters)
{
    const TArray<FInputActionKeyMapping> ExistingMappings = {
        FInputActionKeyMapping(TEXT("Fire"), EKeys::LeftMouseButton),
        FInputActionKeyMapping(TEXT("Fire"), EKeys::Gamepad_RightTrigger),
        FInputActionKeyMapping(TEXT("Sprint"), EKeys::LeftShift),
        FInputActionKeyMapping(TEXT("Sprint"), EKeys::Gamepad_LeftThumbstick),
        FInputActionKeyMapping(TEXT("Crouch"), EKeys::C),
        FInputActionKeyMapping(TEXT("Crouch"), EKeys::Gamepad_FaceButton_Right),
        FInputActionKeyMapping(TEXT("OpenControls"), EKeys::F2),
        FInputActionKeyMapping(TEXT("OpenControls"), EKeys::Gamepad_FaceButton_Bottom)};

    const auto HasMapping = [](const TArray<FInputActionKeyMapping>& Mappings, const FName ActionName, const FKey& Key)
    {
        return Mappings.ContainsByPredicate([ActionName, &Key](const FInputActionKeyMapping& Mapping)
        {
            return Mapping.ActionName == ActionName && Mapping.Key == Key;
        });
    };

    TArray<FInputActionKeyMapping> UpdatedMappings;
    FString Reason;
    BMInputRemap::ERemapResult Result = BMInputRemap::BuildKeyboardRemap(
        ExistingMappings,
        TEXT("Fire"),
        EKeys::F2,
        UpdatedMappings,
        Reason);
    TestEqual(
        TEXT("reserved keyboard action rejects the remap"),
        static_cast<uint8>(Result),
        static_cast<uint8>(BMInputRemap::ERemapResult::ReservedConflict));
    TestTrue(TEXT("reserved rejection explains the owner"), Reason.Contains(TEXT("OpenControls")));
    TestTrue(TEXT("reserved rejection leaves the source map unchanged"), UpdatedMappings == ExistingMappings);

    Result = BMInputRemap::BuildKeyboardRemap(
        ExistingMappings,
        TEXT("Sprint"),
        EKeys::C,
        UpdatedMappings,
        Reason);
    TestEqual(
        TEXT("remappable keyboard actions swap"),
        static_cast<uint8>(Result),
        static_cast<uint8>(BMInputRemap::ERemapResult::Applied));
    TestTrue(TEXT("swap gives the requested key to Sprint"), HasMapping(UpdatedMappings, TEXT("Sprint"), EKeys::C));
    TestTrue(TEXT("swap gives Sprint's old key to Crouch"), HasMapping(UpdatedMappings, TEXT("Crouch"), EKeys::LeftShift));
    TestTrue(TEXT("swap preserves Sprint's controller binding"), HasMapping(UpdatedMappings, TEXT("Sprint"), EKeys::Gamepad_LeftThumbstick));
    TestTrue(TEXT("swap preserves Crouch's controller binding"), HasMapping(UpdatedMappings, TEXT("Crouch"), EKeys::Gamepad_FaceButton_Right));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianAIMovementPacingTest,
    "BlackMeridian.AI.MovementPacing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianAIMovementPacingTest::RunTest(const FString& Parameters)
{
    ABMEnemyCharacter* Enemy = NewObject<ABMEnemyCharacter>(GetTransientPackage());
    TestNotNull(TEXT("enemy movement contract object exists"), Enemy);
    if (!Enemy)
    {
        return false;
    }

    Enemy->EnemyRole = EBMEnemyRole::Assault;
    TestEqual(TEXT("assault tactical speed uses shared tuning"), Enemy->GetRoleMoveSpeed(), BMCombatTuning::EnemyAssaultMoveSpeed);
    Enemy->EnemyRole = EBMEnemyRole::Marksman;
    TestEqual(TEXT("marksman tactical speed uses shared tuning"), Enemy->GetRoleMoveSpeed(), BMCombatTuning::EnemyMarksmanMoveSpeed);
    Enemy->EnemyRole = EBMEnemyRole::Breacher;
    TestEqual(TEXT("breacher tactical speed uses shared tuning"), Enemy->GetRoleMoveSpeed(), BMCombatTuning::EnemyBreacherMoveSpeed);
    Enemy->EnemyRole = EBMEnemyRole::Demolitionist;
    TestEqual(TEXT("demolitionist tactical speed uses shared tuning"), Enemy->GetRoleMoveSpeed(), BMCombatTuning::EnemyDemolitionistMoveSpeed);
    TestEqual(TEXT("demolitionist holds a long launcher range"), Enemy->GetPreferredEngagementRange(), 820.0f);
    TestEqual(TEXT("demolitionist disengages beyond launcher range"), Enemy->GetEngagementRange(), 1180.0f);
    TestTrue(TEXT("demolitionist always aims the Arc-7"), Enemy->ShouldAimAtDistance(300.0f));
    TestTrue(TEXT("demolitionist grenade window is mid-range"), Enemy->ShouldThrowGrenadeAtDistance(700.0f));
    TestFalse(TEXT("demolitionist does not throw grenades at point blank"), Enemy->ShouldThrowGrenadeAtDistance(400.0f));
    TestFalse(TEXT("demolitionist does not throw grenades past its window"), Enemy->ShouldThrowGrenadeAtDistance(1100.0f));
    Enemy->EnemyRole = EBMEnemyRole::Rifleman;
    TestEqual(TEXT("rifleman tactical speed uses shared tuning"), Enemy->GetRoleMoveSpeed(), BMCombatTuning::EnemyRiflemanMoveSpeed);

    const float FarApproach = BMCombatTuning::GetEnemyApproachSpeed(Enemy->GetRoleMoveSpeed(), 1.0f);
    const float NearApproach = BMCombatTuning::GetEnemyApproachSpeed(Enemy->GetRoleMoveSpeed(), 0.0f);
    TestTrue(TEXT("far approach remains below player walk speed"), FarApproach < 360.0f);
    TestTrue(TEXT("far approach remains a readable combat walk"), FarApproach <= 220.0f);
    TestTrue(TEXT("far approach is visibly advancing"), FarApproach >= 100.0f);
    TestTrue(TEXT("near approach clears the shuffle threshold"), NearApproach >= 85.0f);
    TestTrue(TEXT("approach speed increases while closing"), FarApproach > NearApproach);
    TestTrue(
        TEXT("combat maneuver has a bounded forward pressure step"),
        BMCombatTuning::EnemyCombatManeuverMaxAdvance >= 120.0f);
    TestTrue(
        TEXT("combat maneuver holds in a readable acceptance band"),
        BMCombatTuning::EnemyCombatManeuverAcceptanceRadius >= 50.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianHUDScreenBoundaryTest,
    "BlackMeridian.UI.ScreenBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianHUDScreenBoundaryTest::RunTest(const FString& Parameters)
{
    ABMHUD* HUD = NewObject<ABMHUD>(GetTransientPackage());
    TestNotNull(TEXT("HUD object exists"), HUD);
    if (!HUD)
    {
        return false;
    }

    HUD->SetScreen(EBMHUDScreen::InGame);
    HUD->ShowMissionCard(FText::FromString(TEXT("BRIEFING")), FText::FromString(TEXT("MOVE")), 8.0f);
    HUD->ShowSubtitle(FText::FromString(TEXT("Move.")), 4.0f);
    HUD->ShowInteractionPrompt(FText::FromString(TEXT("HOLD E")));
    TestTrue(TEXT("mission card is active during gameplay"), HUD->HasActiveMissionCard());
    TestTrue(TEXT("subtitle is active during gameplay"), HUD->HasActiveSubtitle());
    TestTrue(TEXT("interaction prompt is active during gameplay"), HUD->HasInteractionPrompt());
    HUD->SetScreen(EBMHUDScreen::Death);
    TestEqual(TEXT("death screen is selected"), static_cast<uint8>(HUD->GetScreen()), static_cast<uint8>(EBMHUDScreen::Death));
    TestFalse(TEXT("death screen clears the mission card"), HUD->HasActiveMissionCard());
    TestFalse(TEXT("death screen clears the subtitle"), HUD->HasActiveSubtitle());
    TestFalse(TEXT("death screen clears the interaction prompt"), HUD->HasInteractionPrompt());
    HUD->SetScreen(EBMHUDScreen::InGame);
    TestEqual(TEXT("gameplay screen can be restored after death"), static_cast<uint8>(HUD->GetScreen()), static_cast<uint8>(EBMHUDScreen::InGame));

    HUD->ShowMissionCard(FText::FromString(TEXT("BRIEFING")), FText::FromString(TEXT("MOVE")), 8.0f);
    HUD->ShowHitMarker(false);
    TestFalse(TEXT("first hit dismisses the mission card"), HUD->HasActiveMissionCard());

    HUD->ShowMissionCard(FText::FromString(TEXT("BRIEFING")), FText::FromString(TEXT("MOVE")), 8.0f);
    HUD->ShowDamageFlash(0.5f);
    TestFalse(TEXT("incoming damage dismisses the mission card"), HUD->HasActiveMissionCard());

    HUD->ShowMissionCard(FText::FromString(TEXT("BRIEFING")), FText::FromString(TEXT("MOVE")), 8.0f);
    HUD->ShowElimination(false, 1);
    TestFalse(TEXT("elimination dismisses the mission card"), HUD->HasActiveMissionCard());

    HUD->ShowMissionCard(FText::FromString(TEXT("BRIEFING")), FText::FromString(TEXT("MOVE")), 8.0f);
    HUD->ShowDryFire();
    TestFalse(TEXT("first fire attempt dismisses the mission card"), HUD->HasActiveMissionCard());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianPlayableBoundaryTest,
    "BlackMeridian.World.PlayableBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianPlayableBoundaryTest::RunTest(const FString& Parameters)
{
    ABMGreyboxDirector* Director = NewObject<ABMGreyboxDirector>(GetTransientPackage());
    TestNotNull(TEXT("greybox director exists"), Director);
    if (!Director)
    {
        return false;
    }

    TArray<UBoxComponent*> BoxComponents;
    Director->GetComponents<UBoxComponent>(BoxComponents);

    int32 BoundaryCount = 0;
    for (UBoxComponent* Box : BoxComponents)
    {
        if (!Box || !Box->GetName().Contains(TEXT("Boundary")))
        {
            continue;
        }

        ++BoundaryCount;
        TestEqual(
            FString::Printf(TEXT("%s blocks physics"), *Box->GetName()),
            Box->GetCollisionEnabled(),
            ECollisionEnabled::QueryAndPhysics);
        TestTrue(
            FString::Printf(TEXT("%s is taller than the bounded jump"), *Box->GetName()),
            Box->GetUnscaledBoxExtent().Z >= 500.0f);
        TestFalse(
            FString::Printf(TEXT("%s is hidden in game"), *Box->GetName()),
            Box->IsVisible());
    }

    TestEqual(TEXT("all four perimeter walls are present"), BoundaryCount, 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianMissionInteractionGateTest,
    "BlackMeridian.Mission.InteractionPhaseGates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianMissionInteractionGateTest::RunTest(const FString& Parameters)
{
    ABMInteractableActor* Breach = NewObject<ABMInteractableActor>(GetTransientPackage());
    ABMInteractableActor* Intel = NewObject<ABMInteractableActor>(GetTransientPackage());
    ABMInteractableActor* Extraction = NewObject<ABMInteractableActor>(GetTransientPackage());
    TestNotNull(TEXT("breach interaction object exists"), Breach);
    TestNotNull(TEXT("intel interaction object exists"), Intel);
    TestNotNull(TEXT("extraction interaction object exists"), Extraction);
    if (!Breach || !Intel || !Extraction)
    {
        return false;
    }

    Breach->InteractionId = TEXT("RelayBreach");
    Intel->InteractionId = TEXT("IntelConsole");
    Extraction->InteractionId = TEXT("ExtractionBoat");
    TestFalse(TEXT("breach is unavailable during insertion"), Breach->IsAvailableForMissionPhase(EBMMissionPhase::Insertion));
    TestTrue(TEXT("breach is available in its phase"), Breach->IsAvailableForMissionPhase(EBMMissionPhase::InteriorBreach));
    TestFalse(TEXT("intel is unavailable before the breach"), Intel->IsAvailableForMissionPhase(EBMMissionPhase::InteriorBreach));
    TestTrue(TEXT("intel is available in the intelligence phase"), Intel->IsAvailableForMissionPhase(EBMMissionPhase::Intelligence));
    TestFalse(TEXT("extraction is unavailable during open combat"), Extraction->IsAvailableForMissionPhase(EBMMissionPhase::OpenCombat));
    TestTrue(TEXT("extraction is available in the extraction phase"), Extraction->IsAvailableForMissionPhase(EBMMissionPhase::Extraction));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianSignalControlStateContractTest,
    "BlackMeridian.Multiplayer.SignalControlState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianSignalControlStateContractTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("campaign ordnance remains enabled without a match gate"), ABMGameState::IsCombatOrdnanceEnabled(nullptr));

    ABMGameState* State = NewObject<ABMGameState>(GetTransientPackage());
    TestNotNull(TEXT("game state policy object exists"), State);
    if (!State)
    {
        return false;
    }

    TestTrue(TEXT("campaign ordnance remains enabled"), ABMGameState::IsCombatOrdnanceEnabled(State));
    State->bSignalControlMode = true;
    State->bMatchLive = false;
    TestFalse(TEXT("Signal Control blocks ordnance before the round is live"), ABMGameState::IsCombatOrdnanceEnabled(State));
    State->bMatchLive = true;
    TestTrue(TEXT("Signal Control enables ordnance during the live round"), ABMGameState::IsCombatOrdnanceEnabled(State));
    State->SignalControl.bMatchFinished = true;
    TestFalse(TEXT("finished Signal Control round blocks ordnance"), ABMGameState::IsCombatOrdnanceEnabled(State));

    FBMSignalControlSnapshot Snapshot;
    TestFalse(TEXT("new Signal Control snapshot is not finished"), Snapshot.bMatchFinished);
    TestEqual(TEXT("new Signal Control snapshot starts on node zero"), Snapshot.ActiveNodeIndex, 0);
    TestEqual(TEXT("new Signal Control snapshot starts at zero score"), Snapshot.LatticeScore, 0);
    TestEqual(TEXT("new Signal Control snapshot starts at zero opposing score"), Snapshot.VantaScore, 0);
    TestEqual(TEXT("new Signal Control snapshot has no winner"), static_cast<uint8>(Snapshot.WinningTeam), static_cast<uint8>(EBMTeam::Neutral));

    TestEqual(TEXT("Signal Control lobby defaults to a twelve-slot capacity"), State->SignalLobbyCapacity, 12);
    TestTrue(TEXT("Signal Control lobby starts with an empty roster"), State->SignalLobbyMembers.IsEmpty());
    FBMSignalLobbyMember LobbyMember;
    LobbyMember.DisplayName = TEXT("OPERATOR");
    LobbyMember.Team = EBMTeam::Lattice;
    LobbyMember.bIsHost = true;
    State->SignalLobbyMembers.Add(LobbyMember);
    State->SignalLobbyHumanCount = 1;
    TestEqual(TEXT("lobby roster preserves the member display name"), State->SignalLobbyMembers[0].DisplayName, FString(TEXT("OPERATOR")));
    TestEqual(TEXT("lobby roster preserves the member team"), static_cast<uint8>(State->SignalLobbyMembers[0].Team), static_cast<uint8>(EBMTeam::Lattice));
    TestTrue(TEXT("lobby roster preserves host status"), State->SignalLobbyMembers[0].bIsHost);
    TestFalse(TEXT("human lobby members are not bots"), State->SignalLobbyMembers[0].bIsBot);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianPresentationContractTest,
    "BlackMeridian.Presentation.AudioAndPlayableCues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianPresentationContractTest::RunTest(const FString& Parameters)
{
    UBMAudioDataAsset* Audio = NewObject<UBMAudioDataAsset>(GetTransientPackage());
    ABMMissionDirector* Mission = NewObject<ABMMissionDirector>(GetTransientPackage());
    TestNotNull(TEXT("audio routing asset exists"), Audio);
    TestNotNull(TEXT("mission cue director exists"), Mission);
    if (!Audio || !Mission)
    {
        return false;
    }

    Audio->Footstep = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/Footstep")));
    TestTrue(TEXT("surface-aware footsteps retain the generic fallback"), Audio->FindFootstep(EBMSurfaceType::Concrete) == Audio->Footstep);
    Audio->MusicState = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/MusicFallback")));
    Audio->MusicStates.Add(TEXT("SignalControl"), TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/SignalControl"))));
    TestTrue(TEXT("named music state resolves its dedicated cue"), Audio->FindMusicState(TEXT("SignalControl")) != Audio->MusicState);
    TestTrue(TEXT("unknown music state retains the generic fallback"), Audio->FindMusicState(TEXT("Mission")) == Audio->MusicState);
    TestTrue(TEXT("intro playable cue is authored in the source fallback"), Mission->IntroCameraCues.Num() > 0);
    if (Mission->IntroCameraCues.Num() > 0)
    {
        TestTrue(TEXT("intro cue keeps look control by default"), !Mission->IntroCameraCues[0].bLockLook);
        TestTrue(TEXT("intro cue has a bounded duration"), Mission->IntroCameraCues[0].Duration > 0.0f);
        TestFalse(TEXT("intro cue begins with a distinct camera pose"), Mission->IntroCameraCues[0].StartCameraOffset.IsNearlyZero());
    }
    TestTrue(TEXT("seawall set-piece has multiple playable beats"), Mission->SetPieceCameraCues.Num() >= 2);
    TestTrue(TEXT("seawall set-piece preserves look control"), Mission->SetPieceCameraCues.Num() == 0 || !Mission->SetPieceCameraCues[0].bLockLook);
    TestTrue(TEXT("seawall set-piece has a return beat"), Mission->SetPieceCameraCues.Num() == 0 || Mission->SetPieceCameraCues.Last().CueId == TEXT("SeawallReturn"));
    TestTrue(TEXT("ending has a playable presentation"), Mission->EndingCameraCues.Num() >= 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlackMeridianDeathRecoveryBindingTest,
    "BlackMeridian.UI.DeathRecoveryBinding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackMeridianDeathRecoveryBindingTest::RunTest(const FString& Parameters)
{
    const UInputSettings* InputSettings = GetDefault<UInputSettings>();
    TestNotNull(TEXT("project input settings exist"), InputSettings);
    if (!InputSettings)
    {
        return false;
    }

    bool bHasSafeRestartBinding = false;
    bool bHasEditorReservedF5Binding = false;
    for (const FInputActionKeyMapping& Mapping : InputSettings->GetActionMappings())
    {
        if (Mapping.ActionName != TEXT("RestartCheckpoint"))
        {
            continue;
        }

        bHasSafeRestartBinding = bHasSafeRestartBinding || Mapping.Key == EKeys::BackSpace;
        bHasEditorReservedF5Binding = bHasEditorReservedF5Binding || Mapping.Key == EKeys::F5;
    }

    TestTrue(TEXT("death recovery has a non-editor BackSpace binding"), bHasSafeRestartBinding);
    TestFalse(TEXT("death recovery no longer uses editor-reserved F5"), bHasEditorReservedF5Binding);
    return true;
}
