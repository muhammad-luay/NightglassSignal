// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

using UnrealBuildTool;

public class BlackMeridianRuntime : ModuleRules
{
    public BlackMeridianRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "NetCore",
            "Networking",
            "Sockets",
            "UMG",
            "AIModule",
            "GameplayTasks",
            "Niagara",
            "ProceduralMeshComponent"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "ApplicationCore",
            "NavigationSystem",
            "Slate",
            "SlateCore",
            "PhysicsCore"
        });
    }
}
