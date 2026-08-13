// Copyright (c) 2026 Black Meridian Team. All Rights Reserved.

using UnrealBuildTool;

public class BlackMeridianTests : ModuleRules
{
    public BlackMeridianTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "AIModule",
            "BlackMeridianRuntime",
            "AssetRegistry",
            "MeshDescription",
            "MeshConversion",
            "StaticMeshDescription",
            "Niagara"
        });

        // The automation module is loaded by both editor and game targets,
        // but the AnimGraph/Blueprint authoring code is compiled only when
        // the editor target is being built. Keeping these dependencies out of
        // the game target prevents UnrealEd from leaking into a packaged
        // client through the editor-only graph modules.
        if (Target.Type == TargetType.Editor)
        {
            PrivateDependencyModuleNames.AddRange(new[]
            {
                "AnimGraph",
                "BlueprintGraph",
                "Kismet",
                "KismetCompiler"
            });
            PrivateDependencyModuleNames.Add("UnrealEd");
            PrivateDependencyModuleNames.Add("AssetTools");
            PrivateDependencyModuleNames.Add("AudioEditor");
            PrivateDependencyModuleNames.Add("MeshDescriptionOperations");
        }
    }
}
