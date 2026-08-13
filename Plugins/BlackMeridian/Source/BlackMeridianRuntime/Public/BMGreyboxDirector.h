// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BMArtDirectedProp.h"
#include "BMCoreTypes.h"
#include "BMGreyboxDirector.generated.h"

class AStaticMeshActor;
class APointLight;
class ADirectionalLight;
class ASkyLight;
class AExponentialHeightFog;
class APostProcessVolume;
class ABMEnemyCharacter;
class ABMFriendlySquadMember;
class ABMInteractableActor;
class ABMArtDirectedProp;
class ASkyAtmosphere;
class UAudioComponent;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class UNiagaraComponent;

UCLASS()
class BLACKMERIDIANRUNTIME_API ABMGreyboxDirector : public AActor
{
    GENERATED_BODY()

public:
    ABMGreyboxDirector();

    UFUNCTION(BlueprintPure, Category = "Black Meridian|World")
    bool IsRuntimeFloorReady() const;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Greybox")
    void SpawnCombatWave(int32 Count, const FVector& Center);

    /**
     * Development-only readable encounter layout. The player is the anchor;
     * enemies are placed in a forward arc so the rendered acceptance path
     * immediately shows bodies, weapons, muzzle flashes, and hit feedback.
     */
    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Greybox")
    void SpawnCombatShowcaseWave(const FVector& PlayerLocation, const FVector& ForwardDirection);

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Greybox")
    void SpawnFriendlySquad();

    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Greybox")
    void TriggerEnvironmentalEvent();

    /** Opens the physical relay barrier after the player completes the breach hold. */
    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Greybox")
    void OpenBreachBarrier();

    /**
     * Restores the server-owned campaign actors and environment to a checkpoint
     * baseline before the mission director resumes the saved phase.
     */
    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Greybox")
    void ResetForCheckpoint(EBMMissionPhase CheckpointPhase);

    /** Selects the runtime-generated campaign layout before mission start. */
    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Campaign")
    void SetCampaignLevel(EBMCampaignLevel Level);

    /** Refreshes the readable beacon emphasis after mission state replication. */
    UFUNCTION(BlueprintCallable, Category = "Black Meridian|Greybox")
    void RefreshObjectivePresentation();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox")
    bool bSpawnMissionActors = true;

    UPROPERTY(BlueprintReadOnly, Category = "Campaign")
    EBMCampaignLevel CampaignLevel = EBMCampaignLevel::SableQuay;

    /** Blueprint child with AuthoredCharacterMesh/AnimationData assigned. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Presentation")
    TSubclassOf<ABMEnemyCharacter> CombatEnemyClass;

    UFUNCTION(BlueprintPure, Category = "Black Meridian|Greybox")
    FVector GetExtractionLocation() const;

protected:
    void BuildRuntimeEnvironment();
    /** Hides the one-size-fits-all Glasswake shell before campaign geometry is built. */
    void HideAuthoredCampaignShell();
    /** Removes the previous level's structural geometry during level selection refresh. */
    void ClearCampaignLayout();
    /** Builds the selected level's distinct collision-bearing environment. */
    void SpawnCampaignLayout();
    /** Applies the runtime art-direction palette to static actors saved in an authored map shell. */
    void ApplyAuthoredShellPresentation();
    /** Adds deterministic, non-collision industrial dressing to saved map shells. */
    void SpawnAuthoredShellDressing();
    /** Adds the selected campaign's route-specific visual language. */
    void SpawnCampaignVariantDressing();
    /** Requests a dynamic Recast build after runtime collision is assembled. */
    void BootstrapRuntimeNavigation();
    /** Adds an analytic movement/nav proxy; visibility uses the runtime mesh geometry. */
    void CreateRuntimeCollisionProxy(AStaticMeshActor* Piece);
    /** Enables/disables both the runtime mesh and its analytic collision proxy. */
    void SetRuntimePieceCollision(AStaticMeshActor* Piece, bool bEnabled);
    /** Physically sweeps representative generated structures to verify pawn blocking. */
    void RunCollisionProbe();
    bool IsRuntimeNavigationReady(int32* OutActiveTiles = nullptr) const;
    /** Applies the selected level's terrain, sky, lighting, fog, audio, and weather family. */
    void ApplyEnvironmentTheme();
    /** Rebuilds the local procedural weather particle field for the selected level. */
    void ConfigureThemeWeather();
    void ClearThemeWeather();
    void TickThemeWeather(float DeltaSeconds);
    /** Repositions campaign landmarks when a menu-selected level starts. */
    void RefreshCampaignLevelPresentation();
    void SpawnMissionInteractables();
    void SpawnAuthoredProps();
    /** Adds the selected operation's physical cover/prop composition. */
    void SpawnCampaignPhysicalSet();
    UFUNCTION(NetMulticast, Reliable)
    void MulticastEnvironmentalEvent();
    UFUNCTION(NetMulticast, Reliable)
    void MulticastOpenBreachBarrier();
    UFUNCTION(NetMulticast, Reliable)
    void MulticastResetForCheckpoint();
    AStaticMeshActor* SpawnMesh(
        const TCHAR* MeshPath,
        const FVector& Location,
        const FRotator& Rotation,
        const FVector& Scale,
        const FColor& Color,
        bool bThinCover = false);
    AStaticMeshActor* SpawnCube(const FVector& Location, const FVector& Scale, const FColor& Color, bool bThinCover = false);
    ABMInteractableActor* SpawnInteraction(
        FName InteractionId,
        const FText& Prompt,
        EBMInteractionType InteractionType,
        EBMInteractionAction InteractionAction,
        const FVector& Location,
        const FVector& Scale,
        float HoldDuration = 0.75f);
    void SpawnLighting();
    void RestoreEnvironmentalState();
    ABMArtDirectedProp* SpawnArtDirectedProp(
        EBMArtDirectedPropVariant Variant,
        const FVector& Location,
        const FRotator& Rotation,
        const FVector& Scale,
        const FLinearColor& BaseColor,
        const FLinearColor& AccentColor,
        bool bDestructible = false);
    ABMArtDirectedProp* FindBreachRelayProp() const;

    UFUNCTION()
    void HandleGameStateUpdated();

    void SetActiveObjectiveBeacon(FName ObjectiveId);

private:
    /** True when this director is layered over a saved authored level shell. */
    bool bUseAuthoredLevelShell = false;

    /** The campaign shell is intentionally replaced by the selected structural layout. */
    bool bCampaignShellHidden = false;

    /** Invisible, tall perimeter colliders keep the player on the playable floor. */
    UPROPERTY(VisibleAnywhere, Category = "Greybox|Boundary")
    TObjectPtr<UBoxComponent> NorthBoundaryCollision;

    UPROPERTY(VisibleAnywhere, Category = "Greybox|Boundary")
    TObjectPtr<UBoxComponent> SouthBoundaryCollision;

    UPROPERTY(VisibleAnywhere, Category = "Greybox|Boundary")
    TObjectPtr<UBoxComponent> EastBoundaryCollision;

    UPROPERTY(VisibleAnywhere, Category = "Greybox|Boundary")
    TObjectPtr<UBoxComponent> WestBoundaryCollision;

    UPROPERTY(VisibleAnywhere, Category = "Greybox")
    TObjectPtr<UBoxComponent> FloorCollision;

    UPROPERTY()
    TArray<TObjectPtr<AStaticMeshActor>> GreyboxPieces;

    UPROPERTY()
    TObjectPtr<AStaticMeshActor> BreachBarrier;

    UPROPERTY()
    TArray<TObjectPtr<ABMInteractableActor>> MissionInteractables;

    UPROPERTY()
    TArray<TObjectPtr<AStaticMeshActor>> EnvironmentalEventPieces;

    /** Visual-only pieces belonging to the currently selected campaign. */
    UPROPERTY()
    TArray<TObjectPtr<AStaticMeshActor>> CampaignVariantPieces;

    /** Collision-bearing floor, walls, rooms, ramps, and landmarks for the selected campaign level. */
    UPROPERTY()
    TArray<TObjectPtr<AStaticMeshActor>> CampaignLayoutPieces;

    /** Server-spawned, replicated authored props; clients receive these actors. */
    UPROPERTY()
    TArray<TObjectPtr<ABMArtDirectedProp>> ArtDirectedProps;

    UPROPERTY()
    TObjectPtr<ABMArtDirectedProp> BreachRelayProp;

    /** Runtime light handles keyed by the objective state they represent. */
    UPROPERTY()
    TMap<FName, TObjectPtr<APointLight>> ObjectiveBeaconLights;

    /** Visual-only beacon pieces; only the current mission objective is shown. */
    TMap<FName, TArray<TObjectPtr<AStaticMeshActor>>> ObjectiveBeaconPieces;

    /** Runtime theme presentation actors. These are local visual state on every world instance. */
    UPROPERTY()
    TObjectPtr<ADirectionalLight> EnvironmentSun;

    UPROPERTY()
    TObjectPtr<APointLight> EnvironmentFillLight;

    UPROPERTY()
    TObjectPtr<ASkyLight> EnvironmentSkyLight;

    /** Runtime atmosphere provides an authored horizon for exterior operations. */
    UPROPERTY()
    TObjectPtr<ASkyAtmosphere> EnvironmentSkyAtmosphere;

    UPROPERTY()
    TObjectPtr<AExponentialHeightFog> EnvironmentFog;

    UPROPERTY()
    TObjectPtr<APostProcessVolume> EnvironmentPostProcess;

    UPROPERTY()
    TArray<TObjectPtr<APointLight>> EnvironmentPracticalLights;

    /** Brief local storm flash; never replicated as gameplay state. */
    UPROPERTY()
    TObjectPtr<APointLight> EnvironmentLightningLight;

    UPROPERTY()
    TObjectPtr<UAudioComponent> EnvironmentAmbientAudio;

    /** Niagara supplements the procedural field with authored dust/mist/spark accents where available. */
    UPROPERTY()
    TObjectPtr<UNiagaraComponent> EnvironmentNiagaraAccent;

    /** Lightweight instanced streak/flurry field used for rain, snow, dust, and interior mist. */
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> WeatherParticleSystem;

    TArray<FTransform> WeatherParticleTransforms;
    TArray<float> WeatherParticlePhases;
    EBMEnvironmentTheme ActiveEnvironmentTheme = EBMEnvironmentTheme::OpenDry;
    EBMWeatherType ActiveWeatherType = EBMWeatherType::None;
    float LightningCooldown = 0.0f;
    float LightningFlashRemaining = 0.0f;

    bool bRuntimeNavigationBuildRequested = false;
    bool bRuntimeNavigationReady = false;
    double RuntimeNavigationRequestPlatformTime = -1.0;
    bool bPendingCombatWave = false;
    int32 PendingCombatWaveCount = 0;
    FVector PendingCombatWaveCenter = FVector::ZeroVector;
    float RuntimeNavigationWaitLogTimer = 0.0f;

    bool bEnvironmentalEventTriggered = false;
};
