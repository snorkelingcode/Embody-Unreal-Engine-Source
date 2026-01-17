using UnrealBuildTool;

public class Embody : ModuleRules
{
    public Embody(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Core modules
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "Sockets",                  // For TCP networking
            "Networking",               // For networking support
            "PixelStreaming2",          // For WebRTC commands
            "AIModule",                 // For AI controllers
            "NavigationSystem",         // For NavMesh pathfinding
            "ProceduralMeshComponent",  // For procedural environment generation
            "RHI",                      // For VRS support check
            "RenderCore",               // For FSceneViewExtensionBase
            "Renderer"                  // For scene view extension system
        });

        // Private modules
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Slate",
            "SlateCore",
            "UMG",                       // For UI/Widget support
            "ApplicationCore"            // For FSlateApplication access
        });
    }
}