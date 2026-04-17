using UnrealBuildTool;
using System.IO;

public class EmbodyEditor : ModuleRules
{
    public EmbodyEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Compiler-only path for AnimNode_ProceduralBoneDriver.h (runtime header
        // included by the editor AnimGraphNode). Not passed to UHT to avoid the
        // UE5.7 include-root prefix-match bug between "Embody" and "EmbodyEditor".
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "Embody"));

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "AnimGraph",
            "AnimGraphRuntime",
            "BlueprintGraph",
            "KismetCompiler",
            "Embody"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "Slate",
            "SlateCore",
            "EditorFramework"
        });
    }
}
