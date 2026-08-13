// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

using UnrealBuildTool;

public class NightglassSignalDemoServerTarget : TargetRules
{
    public NightglassSignalDemoServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        bOverrideBuildEnvironment = true;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_1;
        ExtraModuleNames.Add("NightglassSignalDemo");
    }
}
