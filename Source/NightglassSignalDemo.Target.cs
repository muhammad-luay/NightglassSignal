// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

using UnrealBuildTool;

public class NightglassSignalDemoTarget : TargetRules
{
    public NightglassSignalDemoTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        bOverrideBuildEnvironment = true;
        // Keep the target rules consumable by the pinned UE5.1 toolchain. The
        // newer Mac compatibility engine accepts these shared settings too;
        // V7/Unreal5_8 would prevent the intended Windows build from getting
        // as far as the gameplay modules.
        DefaultBuildSettings = BuildSettingsVersion.V2;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_1;
        ExtraModuleNames.Add("NightglassSignalDemo");
    }
}
