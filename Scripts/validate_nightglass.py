#!/usr/bin/env python3
"""Engine-independent contract checks for the Black Meridian subproject."""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    project = json.loads((ROOT / "NightglassSignal.uproject").read_text())
    assert project["EngineAssociation"] == "5.1"
    assert project["DisableEnginePluginsByDefault"] is True
    assert any(module["Name"] == "NightglassSignalDemo" for module in project["Modules"])
    assert any(plugin["Name"] == "BlackMeridian" and plugin["Enabled"] for plugin in project["Plugins"])
    assert any(plugin["Name"] == "EnhancedInput" and plugin["Enabled"] for plugin in project["Plugins"])
    assert any(plugin["Name"] == "Niagara" and plugin["Enabled"] for plugin in project["Plugins"])
    assert any(plugin["Name"] == "OnlineSubsystemNull" and plugin["Enabled"] for plugin in project["Plugins"])
    assert any(plugin["Name"] == "AnimationData" and plugin["Enabled"] for plugin in project["Plugins"])
    assert any(plugin["Name"] == "ControlRig" and plugin["Enabled"] for plugin in project["Plugins"])
    assert any(plugin["Name"] == "RigVM" and plugin["Enabled"] for plugin in project["Plugins"])
    assert any(plugin["Name"] == "ControlRigSpline" and plugin["Enabled"] for plugin in project["Plugins"])

    required = [
        "Config/DefaultEngine.ini",
        "Config/DefaultInput.ini",
        "Plugins/BlackMeridian/BlackMeridian.uplugin",
        "Plugins/BlackMeridian/Source/BlackMeridianRuntime/BlackMeridianRuntime.Build.cs",
        "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Private/BlackMeridianRuntime.cpp",
        "Plugins/BlackMeridian/Source/BlackMeridianTests/Private/BlackMeridianTestsModule.cpp",
        "Source/NightglassSignalDemo/NightglassSignalDemo.Build.cs",
        "Source/NightglassSignalDemo.Target.cs",
        "Source/NightglassSignalDemoEditor.Target.cs",
        "Source/NightglassSignalDemoServer.Target.cs",
        "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Public/BMPlayerCharacter.h",
        "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Public/BMCombatTargeting.h",
        "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Public/BMCombatTuning.h",
        "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Public/BMCharacterAnimationDataAsset.h",
        "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Public/BMCharacterAnimationInterface.h",
        "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Public/BMWeaponDataAsset.h",
        "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Public/BMProjectile.h",
        "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Public/BMThrowableDataAsset.h",
        "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Public/BMInteractionInterface.h",
        "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Public/BMCheckpointSaveGame.h",
        "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Public/BMSurfaceRuleDataAsset.h",
        "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Public/BMCombatHistoryComponent.h",
        "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Public/BMInputRemap.h",
        "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Public/BMSignalControlGameMode.h",
        "Scripts/run_mission_probe.sh",
        "Scripts/run_interaction_probe.sh",
        "Scripts/run_interaction_acceptance_probe.sh",
        "Scripts/run_persistence_probe.sh",
        "Scripts/run_signal_control_probe.sh",
        "Scripts/run_signal_results_probe.sh",
        "Scripts/verify_staged_maps.sh",
        "Scripts/run_combat_probe.sh",
        "Scripts/run_animation_dispatch_probe.sh",
        "Scripts/run_crosshair_probe.sh",
        "Scripts/run_combat_feel_probe.sh",
        "Scripts/run_throwable_probe.sh",
        "Scripts/run_boundary_guard_probe.sh",
        "Scripts/run_collision_probe.sh",
        "Scripts/run_combat_role_probe.sh",
        "Scripts/run_audio_loop_probe.sh",
        "Scripts/generate_environment_audio.sh",
        "Scripts/run_automation.sh",
        "Scripts/build_windows.bat",
        "Scripts/verify_windows_package.ps1",
        "Docs/Game-Design-Document.md",
        "Docs/Technical-Architecture.md",
        "Docs/QA-Checklist.md",
        "Docs/Known-Issues.md",
        "Docs/Asset-Attribution.md",
        "Content/Levels/Glasswake.umap",
        "Content/Levels/SignalControl.umap",
        "Content/Environment/README.md",
        "Content/Environment/Materials/MI_BM_IndustrialSurface.uasset",
        "Content/Environment/Meshes/SM_BM_Env_Block.uasset",
        "Content/Environment/Meshes/SM_BM_Env_Pipe.uasset",
        "Content/Environment/Meshes/SM_BM_Env_Orb.uasset",
        "Content/Environment/Meshes/SM_BM_Env_Deck.uasset",
        "Content/Environment/Meshes/SM_BM_Env_Truss.uasset",
        "Content/Environment/Meshes/SM_BM_Env_Grate.uasset",
        "Content/Environment/Meshes/SM_BM_Env_Panel.uasset",
        "Content/Environment/Meshes/SM_BM_Env_Cover.uasset",
        "Content/Environment/Meshes/SM_BM_Env_Door.uasset",
        "Content/Environment/Meshes/SM_BM_Env_Window.uasset",
        "Content/Environment/Meshes/SM_BM_Env_Sign.uasset",
        "Content/Environment/Meshes/SM_BM_Env_Machinery.uasset",
        "Content/Environment/Meshes/SM_BM_Env_Wreckage.uasset",
        "Content/VFX/Generated/NS_BM_Rain.uasset",
        "Content/VFX/Generated/NS_BM_Snow.uasset",
        "Content/Characters/Mannequins/Meshes/SKM_Manny_Simple.uasset",
        "Content/Characters/Mannequins/Materials/M_Mannequin.uasset",
        "Content/Characters/Mannequins/Materials/Manny/MI_Manny_01_New.uasset",
        "Content/Characters/Mannequins/Materials/Manny/MI_Manny_02_New.uasset",
        "Content/Characters/Mannequins/Rigs/PA_Mannequin.uasset",
    ]
    missing = [path for path in required if not (ROOT / path).is_file()]
    if missing:
        raise AssertionError(f"missing required project artifacts: {missing}")

    source_files = list((ROOT / "Plugins/BlackMeridian/Source/BlackMeridianRuntime").rglob("*.h"))
    source_files += list((ROOT / "Plugins/BlackMeridian/Source/BlackMeridianRuntime").rglob("*.cpp"))
    assert len(source_files) >= 25
    all_source = "\n".join(path.read_text(errors="replace") for path in source_files)
    combat_character_source = (ROOT / "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Private/BMCombatCharacter.cpp").read_text()
    enemy_character_source = (ROOT / "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Private/BMEnemyCharacter.cpp").read_text()
    player_character_source = (ROOT / "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Private/BMPlayerCharacter.cpp").read_text()
    # Hit/death events must remain virtual at the base call site. A qualified
    # base call compiles cleanly but skips the enemy's authored sequence path.
    assert "Super::PlayCharacterAnimation_Implementation" not in combat_character_source
    assert "PlayCharacterAnimation_Implementation(bHeadshot ? TEXT(\"HeadHitReact\") : TEXT(\"HitReact\"));" in combat_character_source
    assert "PlayCharacterAnimation_Implementation(TEXT(\"Death\"));" in combat_character_source
    assert "PlayDevelopmentAnimationEvent(AnimationEvent);" in enemy_character_source
    assert "ABP_BM_Manny_Rifle.ABP_BM_Manny_Rifle_C" in enemy_character_source
    assert "project AnimBP route" in enemy_character_source
    movement_facing_start = enemy_character_source.index("void ABMEnemyCharacter::UpdateMovementFacing")
    movement_facing_end = enemy_character_source.index("FVector ABMEnemyCharacter::GetVisibleVisualForward", movement_facing_start)
    movement_facing_source = enemy_character_source[movement_facing_start:movement_facing_end]
    # A moving hostile must keep its complete body heading on the live combat
    # target. The route direction is only the no-target fallback; otherwise a
    # rifle can fire toward the player while Manny visibly advances sideways
    # or with his back turned.
    assert "if (bFacingCombatTarget)" in movement_facing_source
    assert "DesiredFacingDirection = FRotator(0.0f, CombatAimYaw, 0.0f).Vector().GetSafeNormal2D();" in movement_facing_source
    assert movement_facing_source.index("if (bFacingCombatTarget)") < movement_facing_source.index("else if (bMovementActive)")
    assert "DesiredFacingDirection = TravelDirection;" in movement_facing_source
    assert "project AnimBP loaded" in combat_character_source
    animation_probe_script = (ROOT / "Scripts/run_animation_dispatch_probe.sh").read_text()
    crosshair_probe_script = (ROOT / "Scripts/run_crosshair_probe.sh").read_text()
    assert "BM_AnimationDispatchProbe" in all_source
    assert "event=Hit" in animation_probe_script
    assert "mode=montage additive=yes" in animation_probe_script
    assert "hit feedback route" in animation_probe_script
    assert "event=Death" in animation_probe_script
    assert "death knockdown" in animation_probe_script
    assert "BM_CrosshairProbe" in all_source
    assert "native crosshair target" in crosshair_probe_script
    assert "danger_red" in crosshair_probe_script
    for forbidden in ("Call of Duty", "Modern Warfare", "Activision"):
        assert forbidden not in all_source
    assert "ServerFire" in all_source
    assert "LoadContentProfile" in all_source
    assert "BMThrowableDataAsset" in all_source
    assert "SpawnActorDeferred" in all_source
    assert "FinishSpawningActor" in all_source
    assert "SignalControl" in all_source
    assert "WinningTeam" in all_source
    assert "MaintainBotFill" in all_source
    assert "MaxMatchPlayers = 12" in all_source
    assert "bot fill humans=" in all_source
    assert "EnsureBotWeapons" in all_source
    assert "HasActorBegunPlay" in all_source
    assert "IsLocallyControlled" in all_source
    assert "SetThirdPersonPresentation(!IsLocallyControlled())" in all_source
    assert "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple" in all_source
    assert "EBMMissionPhase" in all_source
    assert "RegisterShot" in all_source
    assert "RunCombatFeelProbe" in all_source
    assert "BM_CombatFeelProbe" in all_source
    assert "ServerInteract" in all_source
    assert "RegisterCombatLocation" in all_source
    assert "UBMCombatHistoryComponent" in all_source
    assert "MaxRewindSeconds" in all_source
    assert "FBMCombatHistoryRewindScope" in all_source
    # Moving-target/network damage must not fail solely because the historical
    # pose misses a ray that was valid on the current server pose. The current
    # hit is captured before rewind and carried through the fallback, then the
    # round is consumed only after an atomic validated hit exists.
    assert "RestoreCurrentPoses" in all_source
    assert "CurrentServerCrosshairTarget" in all_source
    assert "ValidatedPlayerCrosshairHit = CurrentServerCrosshairHit" in all_source
    assert "player shot rejected before ammo" in all_source
    assert "current_pose_fallback" in all_source
    assert "BM_CombatFallbackProbe" in all_source
    assert "current_pose_primed" in all_source
    assert "AIM_FALLBACK_PROBE" in (ROOT / "Scripts/run_combat_probe.sh").read_text()
    assert "EBMEnemyRole::Breacher" in all_source
    assert "BREACHER" in all_source
    assert "Breach12" in all_source
    assert "EBMCharacterMovementState" in all_source
    assert "SetCharacterAnimationState" in all_source
    assert "FirstPersonArms" in all_source
    assert "PlayCharacterAnimation" in all_source
    assert "ReviveTarget" in all_source
    assert "RestoreForCheckpoint" in all_source
    assert "BeginMountedWeapon" in all_source
    assert "SuppressionAmount" in all_source
    assert "ResolveSurfaceType" in all_source
    assert "FindSurfaceRule" in all_source
    assert "AllowsPenetration" in all_source
    assert "PhysicalSurfaces" in (ROOT / "Config/DefaultEngine.ini").read_text()
    input_config = (ROOT / "Config/DefaultInput.ini").read_text()
    assert 'ActionName="RestartCheckpoint"' in input_config and "Key=BackSpace" in input_config
    assert "ActionName=\"RestartCheckpoint\",bShift=False,bCtrl=False,bAlt=False,bCmd=False,Key=F5" not in input_config
    open_controls_bindings = [
        line for line in input_config.splitlines() if 'ActionName="OpenControls"' in line
    ]
    switch_weapon_bindings = [
        line for line in input_config.splitlines() if 'ActionName="SwitchWeapon"' in line
    ]
    assert any("Key=F2" in line for line in open_controls_bindings)
    assert any("Key=K" in line for line in open_controls_bindings)
    assert all("Key=Two" not in line for line in open_controls_bindings)
    assert any("Key=One" in line for line in switch_weapon_bindings)
    assert any("Key=Two" in line for line in switch_weapon_bindings)
    assert "BindKey(EKeys::F2" in all_source
    assert "CurrentScreen == EBMHUDScreen::Controls" in all_source
    assert "Gamepad_FaceButton_Bottom" in input_config
    assert "Gamepad_Special_Right" in input_config
    assert "Gamepad_DPad_Left" in input_config and "Gamepad_DPad_Right" in input_config
    assert "IsViewDirectionWithinCone" in all_source
    assert "ResolveLiveHostileCrosshairTarget" in all_source
    assert "IsCombatAimHit" in all_source
    assert "IsNonBlockingCombatMarker" in all_source
    assert "interaction proxies need Visibility collision" in all_source
    assert "Hit.GetComponent() == Target->GetCombatAimHitbox()" in all_source
    assert "CombatAimHitbox->InitCapsuleSize(BMCombatTuning::CombatAimHitboxRadius, 88.0f)" in combat_character_source
    assert "const float AimRadius = BMCombatTuning::CombatAimHitboxRadius;" in combat_character_source
    assert "CombatAimHitboxRadius = 24.0f" in all_source
    assert "CombatAimMaxHorizontalAngleDegrees = 5.0f" in all_source
    assert "CombatAimHitboxVerticalInset = 0.0f" in all_source
    assert "upper_body_resolved=yes" in all_source
    assert "upper_body_shot_did_not_damage_enemy" in all_source
    assert "bHitDedicatedCrosshairComponent" in all_source
    assert "OutPose.AimHitboxRadius = FMath::Lerp" in all_source
    assert "OutPose.AimHitboxHalfHeight = FMath::Lerp" in all_source
    tick_start = player_character_source.index("void ABMPlayerCharacter::Tick")
    tick_end = player_character_source.index("void ABMPlayerCharacter::SetupPlayerInputComponent", tick_start)
    tick_source = player_character_source[tick_start:tick_end]
    assert tick_source.index("UpdateCameraBob(DeltaSeconds);") < tick_source.index("FireWeapon();")
    assert "ResetFireInputState" in all_source
    assert "bFireInputArmed" in all_source
    assert "StartFireInternal(true)" in all_source
    # BotFireAt is exposed on the player pawn for the Signal Control AI and
    # the development combat showcase. A target lookup must never make a
    # human-controlled pawn fire unless the explicit showcase auto-fire flag
    # is present; keep this guard source-tested because the packaged repro
    # previously showed kills attributed to BMPlayerCharacter with no input.
    assert '#include "BMSignalBotController.h"' in player_character_source
    assert "const bool bSignalBotPawn = Cast<ABMSignalBotController>(GetController()) != nullptr;" in player_character_source
    assert "if (!bShowcaseAutoFire && !bSignalBotPawn)" in player_character_source
    assert "reason=human_pawn_without_explicit_auto_fire" in player_character_source
    assert "CurrentWeapon->Fire(Origin, Direction, false);" in player_character_source
    # The near-miss regression cases remain outside the dedicated aim surface
    # even though exact dedicated-hitbox rays no longer get a redundant
    # center-angle rejection. Keep this source-level guard aligned with the
    # runtime probe so a later change cannot silently restore broad
    # movement-capsule aim assist.
    assert "close_miss_shot angle_deg=7.0" in all_source
    assert "close_miss_shot angle_deg=6.5" in all_source
    assert "moving_target_shot resolved=yes" in all_source
    assert "moving_target_not_under_reticle" in all_source
    assert "MaxAcceptedViewAngleDegrees" in all_source
    weapon_source = (ROOT / "Plugins/BlackMeridian/Source/BlackMeridianRuntime/Private/BMWeapon.cpp").read_text()
    assert "FVector AuthoritativeOrigin = Origin;" in weapon_source
    assert "target_resolution=deferred" in weapon_source
    assert "RewoundCrosshairHit" in weapon_source
    assert "player damage blocked final_gate" in weapon_source
    assert "Hit.GetActor() == PlayerCrosshairTarget" in weapon_source
    assert "bStrictPlayerCrosshair" in weapon_source
    # Signal bots reuse ABMPlayerCharacter for their body and weapon, but
    # their ABMSignalBotController-owned shots must remain on the explicit AI
    # ray path instead of being rejected as controller-less human shots.
    assert '#include "BMSignalBotController.h"' in weapon_source
    assert "const bool bSignalBotPawn" in weapon_source
    assert "const bool bHumanPlayerPawn" in weapon_source
    assert "if (bHumanPlayerPawn && !PlayerController)" in weapon_source
    assert "const bool bPlayerClassOwner = Cast<ABMPlayerCharacter>(GetOwner()) != nullptr && !bSignalBotPawn;" in weapon_source
    assert "const bool bHumanPlayerWeaponOwner = Cast<ABMPlayerCharacter>(GetOwner()) != nullptr && !bSignalBotPawn;" in weapon_source
    assert "const bool bEffectiveStrictPlayerCrosshair = !bSignalBotPawn" in weapon_source
    assert "Target->HealthComponent->MaxHealth = 1000.0f" in all_source
    assert "bPlayerWeaponDamage" in all_source
    assert "bPlayerWeaponAimValidated" in all_source
    assert "bSignalBotInstigator" in all_source
    assert (
        "health boundary blocked player damage" in all_source
        or "health boundary blocked unvalidated player weapon damage" in all_source
    )
    assert "health_boundary_result blocked=yes" in all_source
    performance_probe = (ROOT / "Scripts/run_performance_probe.sh").read_text(errors="replace")
    assert performance_probe.startswith("#!/usr/bin/env bash")
    assert "-installed" in performance_probe
    assert "-BM_PerformanceProbe" in performance_probe
    assert "-BM_PerformanceProbeSeconds=" in performance_probe
    assert "-BM_PerformanceProbeExit" in performance_probe
    assert "-NoMutex" in performance_probe
    audio_loop_probe = (ROOT / "Scripts/run_audio_loop_probe.sh").read_text(errors="replace")
    assert audio_loop_probe.startswith("#!/usr/bin/env bash")
    assert "-BM_AudioProbe" in audio_loop_probe
    assert "music heartbeat" in all_source
    assert "playing=no" in audio_loop_probe
    assert "AudioProfile" in all_source
    assert "SurfaceFootsteps" in all_source
    assert "FindFootstep" in all_source
    assert "UpdateAudioPresentation" in all_source
    assert "LowHealth" in all_source
    assert "AmbientBattlefield" in all_source
    assert "EnvironmentAmbiences" in all_source
    assert "FindEnvironmentAmbience" in all_source
    assert "EnvironmentTerrainId" in all_source
    assert "EnvironmentLayoutId" in all_source
    assert "EnvironmentPropSetId" in all_source
    assert "EnvironmentSkyId" in all_source
    assert "EnvironmentAmbienceId" in all_source
    assert "EnvironmentWeatherSystemId" in all_source
    assert "EnvironmentWeatherSystemPath" in all_source
    assert "EnvironmentTerrainMetallic" in all_source
    assert "EnvironmentTerrainRoughness" in all_source
    assert "bStormLightning" in all_source
    assert "LightningFlashRemaining" in all_source
    assert "ProjectEnvironmentDoorPath" in all_source
    assert "ProjectEnvironmentWindowPath" in all_source
    assert "ProjectEnvironmentSignPath" in all_source
    assert "ProjectEnvironmentMachineryPath" in all_source
    assert "SpawnCampaignLayout" in all_source
    assert "HideAuthoredCampaignShell" in all_source
    assert "CampaignLayoutPieces" in all_source
    assert "CreateRuntimeCollisionProxy" in all_source
    assert "Black Meridian collision probe: PASS" in all_source
    assert "BM_CollisionProbe" in all_source
    assert "NS_BM_Rain" in all_source
    assert "NS_BM_Snow" in all_source
    assert "NS_BM_Dust" in all_source
    assert "NS_BM_Smoke" in all_source
    assert "rain, snow" in all_source
    assert "MusicStates" in all_source
    assert "FindMusicState" in all_source
    assert "PlayMusicState" in all_source
    assert "DrawCompass" in all_source
    assert "GetBMCampaignLevelCount" in all_source
    assert "GetBMCampaignLevelListText" in all_source
    assert "20 DISTINCT OPERATIONS" in all_source
    assert "LastLight" in all_source
    assert "bWeaponGranted" in all_source
    assert "bAmmoGranted" in all_source
    assert "SyncMissionMusicToGameState" in all_source
    assert "const bool bSuspense" in all_source
    assert "Suspense" in (ROOT / "Content/Audio/README.md").read_text()
    assert "PositionalDialogue" in all_source
    assert "VFXProfile" in all_source
    assert "PlaySoundAtLocation" in all_source
    assert "SpawnSystemAtLocation" in all_source
    assert "SurfaceImpacts" in all_source
    assert "ShellEjection" in all_source
    assert "ImpactDecal" in all_source
    assert "BloodImpact" in all_source
    assert "MulticastDetonation" in all_source
    assert "IsDetonationPresentationActive" in all_source
    assert "UpdateThrowableThreatPresentation" in all_source
    assert "HasThrowableThreat" in all_source
    assert "INCOMING FRAG" in all_source
    assert "ThrowableWarning" in all_source
    assert "TargetCharacter->IsEliminated()" in all_source
    assert "Black Meridian throwable: enemy spawned" in all_source
    assert "IsSuppressed()" in all_source
    assert "bSuppressor" in all_source
    assert "bReplicatedSuppressed" in all_source
    assert "SuppressedGunshot" in all_source
    assert "UpdateSuppressionPresentation" in all_source
    assert "PostProcessBlendWeight" in all_source
    assert "FinishReload" in all_source
    assert "ServerCancelReload" in all_source
    assert "ReloadCancelled" in all_source
    assert "ObjectiveScore" in all_source
    assert "ContainsCharacter" in all_source
    assert "AddSuppression(SuppressionAmount)" in all_source
    assert "DOREPLIFETIME(ABMInteractableActor, ReviveTarget)" in all_source
    assert "RestoreThrowableInventory" in all_source
    assert "CompletedCampaignLevels" in all_source
    assert "ServerTryMantle" in all_source
    assert "ServerSetAimState" in all_source
    assert "ServerSetSprintState" in all_source
    assert "ServerSetCrouchState" in all_source
    assert "ServerSetProneState" in all_source
    assert "StopSprintInternal" in all_source
    assert "StopCrouchInternal" in all_source
    assert "DOREPLIFETIME(ABMPlayerCharacter, bSprinting)" in all_source
    assert "DOREPLIFETIME(ABMPlayerCharacter, bTacticalSprinting)" in all_source
    assert "DOREPLIFETIME(ABMPlayerCharacter, bProne)" in all_source
    assert "TryMantleInternal" in all_source
    for target_file in (
        ROOT / "Source/NightglassSignalDemo.Target.cs",
        ROOT / "Source/NightglassSignalDemoEditor.Target.cs",
        ROOT / "Source/NightglassSignalDemoServer.Target.cs",
    ):
        target_text = target_file.read_text(errors="replace")
        assert "BuildSettingsVersion.V2" in target_text
        assert "EngineIncludeOrderVersion.Unreal5_1" in target_text
    build_cs = (ROOT / "Source/NightglassSignalDemo/NightglassSignalDemo.Build.cs").read_text(errors="replace")
    assert "Target.Platform == UnrealTargetPlatform.Mac" in build_cs
    assert "IsSignalControlLocked" in all_source
    assert "RequestMatchStart" in all_source
    assert "ServerRequestMatchStart" in all_source
    assert "SpawnReplacementBot" in all_source
    assert "BOT TAKEOVER" in all_source
    assert "RequestSignalControlTravel" in all_source
    assert "RequestMissionTravel" in all_source
    assert "ServerTravel" in all_source
    assert "OpenLevel" in all_source
    assert "ShowMissionMenu" in all_source
    assert "Continue to the next level?" in all_source
    assert "NEXT LEVEL is selected by default." in all_source
    assert "SecondaryAction = 8" in all_source
    assert "campaign completion prompt" in all_source
    assert "BMGameMode?BM_CampaignLevel=%d" in all_source
    assert "BMGameMode&BM_CampaignLevel=%d" not in all_source
    assert "skipped late splash initialization" in all_source
    assert "ignored stale splash completion" in all_source
    assert "SettingsPrevious" in all_source
    assert "AdjustSettings" in all_source
    assert "bSignalControlMode" in all_source
    assert "ApplyAccessibilityColor" in all_source
    assert "GetSubtitleScale" in all_source
    assert "GetControlBindingLabel" in all_source
    assert "ApplyControlBinding" in all_source
    assert "BuildKeyboardRemap" in all_source
    assert "ReservedConflict" in all_source
    assert "GetRemappableActions" in all_source
    assert "IsGamepadKey" in all_source
    assert "SaveKeyMappings" in all_source
    assert "ForceRebuildingKeyMaps" in all_source
    assert "BM_CheatGiveAmmo" in all_source
    assert "BM_CheatRestore" in all_source
    assert "BM_AutoStartMission" in all_source
    assert "BM_MissionProbe" in all_source
    assert "BM_InteractionAcceptanceProbe" in all_source
    assert "ProbeCoverSelection" in all_source
    assert "interaction acceptance probe" in all_source
    assert "BM_MissionTimeScale" in all_source
    assert "BM_PersistenceProbeWrite" in all_source
    assert "BM_PersistenceProbeRead" in all_source
    assert "GetCheckpointSaveSlotName" in all_source
    assert "GetProfileSaveSlotName" in all_source
    assert "persistence probe: WRITE PASS" in all_source
    assert "persistence probe: READ" in all_source
    assert "BM_AutoStartSignalMatch" in all_source
    assert "BM_SignalControlProbe" in all_source
    assert "BM_CombatProbe" in all_source
    assert "BM_ThrowableProbe" in all_source
    assert "BM_BoundaryGuardProbe" in all_source
    assert "RunBoundaryGuardProbe" in all_source
    assert "NorthBoundaryCollision" in all_source
    assert "RecoverFromBoundaryFall" in all_source
    assert "PASS frag_damage=pass veil_suppression=pass fuse_assets=pass" in all_source
    assert "BM_CombatShowcase" in all_source
    assert "authoritative countdown started" in all_source
    assert "lobby ready authority=" in all_source
    assert "player login team=" in all_source
    assert "active node rotated" in all_source
    assert "server scoring verified" in all_source
    assert "match finished winner=" in all_source
    assert "results screen signal_control=yes" in all_source
    assert "GetInteractionHoldProgress" in all_source
    assert "NEXT STEP" in all_source
    assert "COLOUR KEY" in all_source
    assert "WALK TO THE TEAL BEACON" in all_source
    assert "R RELOAD   E INTERACT   SHIFT SPRINT   H HELP" in all_source
    assert "RELEASE TO CANCEL" in all_source
    assert "RunCombatProbeFire" in all_source
    assert "RunCombatProbeReload" in all_source
    tests_source = (ROOT / "Plugins/BlackMeridian/Source/BlackMeridianTests/Private/BlackMeridianTests.cpp").read_text(errors="replace")
    assert "ThrowableAssets" in tests_source
    assert "EnvironmentAssets" in tests_source
    assert "EnvironmentThemeAssets" in tests_source
    assert "BM_Ambient_SableQuay" in tests_source
    assert "SM_BM_Env_Block" in tests_source
    assert "SM_BM_Env_Pipe" in tests_source
    assert "SM_BM_Env_Orb" in tests_source
    assert "SM_BM_Env_Deck" in tests_source
    assert "SM_BM_Env_Truss" in tests_source
    assert "SM_BM_Env_Grate" in tests_source
    assert "SM_BM_Env_Panel" in tests_source
    assert "SM_BM_Env_Cover" in tests_source
    assert "MI_BM_IndustrialSurface" in tests_source
    assert "+DirectoriesToAlwaysCook=(Path=\"/Game/Environment\")" in (ROOT / "Config/DefaultGame.ini").read_text()
    assert "+DirectoriesToAlwaysCook=(Path=\"/Game/Characters/Mannequins\")" in (ROOT / "Config/DefaultGame.ini").read_text()
    assert "+DirectoriesToAlwaysCook=(Path=\"/Game/Data/Weapons\")" in (ROOT / "Config/DefaultGame.ini").read_text()
    assert "+DirectoriesToAlwaysCook=(Path=\"/Game/Data/Throwables\")" in (ROOT / "Config/DefaultGame.ini").read_text()
    assert "+DirectoriesToAlwaysCook=(Path=\"/Game/Levels\")" in (ROOT / "Config/DefaultGame.ini").read_text()
    assert "+MapsToCook=(FilePath=\"/Game/Levels/Glasswake\")" in (ROOT / "Config/DefaultGame.ini").read_text()
    assert "+MapsToCook=(FilePath=\"/Game/Levels/SignalControl\")" in (ROOT / "Config/DefaultGame.ini").read_text()
    assert "RunCombatProbeReloadInterrupt" in all_source
    assert "ClientRunCombatProbeReload" in all_source
    assert "BM_ReloadProbe" in all_source
    assert "ConfigureMagazineCapacity" in all_source
    assert "reload probe passed" in all_source
    assert "reload presentation state" in all_source
    assert "server shot accepted" in all_source
    assert "server damage applied" in all_source
    assert "server damage verified" in all_source
    assert "client ammo replicated" in all_source
    assert "client health replicated" in all_source
    assert "disconnect observed" in all_source
    assert "Black Meridian mission: phase" in all_source
    assert "#if !UE_BUILD_SHIPPING" in all_source
    assert "SetSoundMixClassOverride" in all_source
    assert "AudioMix" in all_source
    assert "RecentKillFeed.Insert" in all_source
    assert "SpawnMissionInteractables" in all_source
    assert "ResetForCheckpoint" in all_source
    assert "MulticastResetForCheckpoint" in all_source
    assert "MissionInteractables" in all_source
    assert "RestoreEnvironmentalState" in all_source
    assert "RecentKillFeed.Reset()" in all_source
    assert "Squad->UnregisterEnemy(Enemy)" in all_source
    assert "IsAvailableForMissionPhase" in all_source
    assert "FBMPlayableCameraCue" in all_source
    assert "StartPlayableSequence" in all_source
    assert "GetActivePlayableCameraCue" in all_source
    for interaction_id in (
        "IntelConsole",
        "RelayBreach",
        "MountedVantageGun",
        "RelayLadder",
        "CheckpointAmmo",
        "CheckpointOrdnance",
        "RookPickup",
        "PlantSignalJammer",
        "ExtractionBoat",
    ):
        assert interaction_id in all_source, f"missing runtime interaction prop: {interaction_id}"
    assert "BreachBarrier->Destroy()" in all_source
    assert "UE_BUILD_SHIPPING" in all_source
    assert "GetRecentEnemyDeathLocation" in all_source
    assert "ReplicatedUsing = OnRep_Eliminated" in all_source
    assert "DeathPresentationDirection" in all_source
    assert "OnRep_DeathPresentation" in all_source
    assert "death knockdown" in all_source
    assert "BM_DeathPresentationProbe" in all_source
    assert "death presentation probe passed" in (ROOT / "Scripts/run_combat_probe.sh").read_text()
    assert "combat (probe|trace): server shot accepted" in (ROOT / "Scripts/run_combat_probe.sh").read_text()
    assert "client (fire dispatch|weapon fire)" in (ROOT / "Scripts/run_combat_probe.sh").read_text()
    assert "RELOAD_PROBE" in (ROOT / "Scripts/run_combat_probe.sh").read_text()
    assert "reload probe passed interruption=" in (ROOT / "Scripts/run_combat_probe.sh").read_text()
    assert "has_rewind_probe_passed" in (ROOT / "Scripts/run_combat_probe.sh").read_text()
    assert "Cast<ABMSignalBotController>(CharacterController)" in all_source
    assert "EBMAIState::Advance" in all_source
    assert "FindCoverLocation" in all_source
    assert "GetObjectiveAnchor" in all_source
    assert "AssistDownedSquadmate" in all_source
    assert "PlantSignalJammer" in all_source
    assert "bSignalJammerPlanted" in all_source
    assert '"GameFramework/ProjectileMovementComponent.h"' in all_source
    assert '"Components/ProjectileMovementComponent.h"' not in all_source
    assert all_source.count("UpdatedComponent = Collision") >= 2
    assert "IMPLEMENT_MODULE(FDefaultModuleImpl, BlackMeridianRuntime)" in all_source
    print(f"Black Meridian contract passed: {len(source_files)} runtime source files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
