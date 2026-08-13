// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

using UnrealBuildTool;

public class NightglassSignalDemo : ModuleRules
{
    public NightglassSignalDemo(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        // ScreenCaptureKit is optional on the macOS 13 remote test host; keep
        // the framework out of the Windows target link environment entirely.
        if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicWeakFrameworks.Add("ScreenCaptureKit");
        }
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "BlackMeridianRuntime"
        });
    }
}
