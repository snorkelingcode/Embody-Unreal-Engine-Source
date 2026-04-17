using UnrealBuildTool;
using System.IO;

public class Embody : ModuleRules
{
    public Embody(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // whisper.cpp third-party library (speech-to-text)
        string WhisperThirdParty = Path.Combine(ModuleDirectory, "..", "ThirdParty");
        string WhisperIncDir = Path.Combine(WhisperThirdParty, "Include", "whisper");
        PublicIncludePaths.Add(WhisperIncDir);
        PublicDefinitions.Add("WHISPER_SHARED");  // dllimport on Win64, visibility on Linux
        PublicDefinitions.Add("GGML_SHARED");     // dllimport for ggml symbols

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string WhisperLibDir = Path.Combine(WhisperThirdParty, "Lib", "Win64", "whisper", "x64");
            PublicAdditionalLibraries.Add(Path.Combine(WhisperLibDir, "whisper.lib"));
            // whisper.dll + ggml dependency DLLs
            string[] WhisperDlls = { "whisper.dll", "ggml.dll", "ggml-base.dll", "ggml-cpu.dll", "ggml-cuda.dll" };
            foreach (string Dll in WhisperDlls)
            {
                RuntimeDependencies.Add(Path.Combine(WhisperLibDir, Dll));
                PublicDelayLoadDLLs.Add(Dll);
            }
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            string WhisperLibDir = Path.Combine(WhisperThirdParty, "Lib", "Linux", "whisper", "x64");
            PublicAdditionalLibraries.Add(Path.Combine(WhisperLibDir, "libwhisper.so"));
            // whisper.so + ggml dependency SOs
            string[] WhisperSos = { "libwhisper.so", "libggml.so", "libggml-base.so", "libggml-cpu.so", "libggml-cuda.so" };
            foreach (string So in WhisperSos)
            {
                RuntimeDependencies.Add(Path.Combine(WhisperLibDir, So));
            }
        }

        // Add include path for PixelStreaming2 internal headers (for FVideoProducerRenderTarget)
        string PixelStreaming2Path = Path.Combine(EngineDirectory, "Plugins", "Media", "PixelStreaming2", "Source", "PixelStreaming2", "Internal");
        PublicIncludePaths.Add(PixelStreaming2Path);

        // Add include path for PixelStreaming2Input public headers (for IPixelStreaming2InputHandler)
        string PixelStreaming2InputPath = Path.Combine(EngineDirectory, "Plugins", "Media", "PixelStreaming2", "Source", "PixelStreaming2Input", "Public");
        PublicIncludePaths.Add(PixelStreaming2InputPath);

        // Core modules
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "HTTP",                     // For HTTP requests
            "Json",                     // For JSON parsing/serialization
            "JsonUtilities",            // For JSON utility functions
            "Sockets",                  // For TCP networking
            "Networking",               // For networking support
            "PixelStreaming2",          // For WebRTC commands and streaming
            "PixelStreaming2Core",      // For IPixelStreaming2Streamer interface
            "PixelStreaming2Input",     // For IPixelStreaming2InputHandler
            "AIModule",                 // For AI controllers
            "NavigationSystem",         // For NavMesh pathfinding
            "CinematicCamera",          // For UCineCameraComponent (post-process commands)
            "ProceduralMeshComponent",  // For procedural environment generation
            "RHI",                      // For VRS support check
            "RenderCore",               // For FSceneViewExtensionBase
            "Renderer",                 // For scene view extension system
            "ZenBlink",                 // For UZenBlinkComponent (emotion commands)
            "AnimGraphRuntime"          // For custom anim graph nodes (Procedural Bone Driver)
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