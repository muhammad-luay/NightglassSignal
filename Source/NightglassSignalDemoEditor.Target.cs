// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

using UnrealBuildTool;

public class NightglassSignalDemoEditorTarget : TargetRules
{
    public NightglassSignalDemoEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        // Installed engine distributions cannot build a target with a unique
        // environment. Keep this target compatible with the provisioned
        // editor while retaining the normal project module boundary.
        BuildEnvironment = TargetBuildEnvironment.Shared;
        bOverrideBuildEnvironment = true;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_1;
        ExtraModuleNames.Add("NightglassSignalDemo");
    }
}
