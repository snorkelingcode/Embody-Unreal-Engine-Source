// StreamingHelpers.h
// Simple helpers for multi-player streaming setup
//
// Copyright (c) 2025 Embody AI

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "InputMappingContext.h"
#include "StreamingHelpers.generated.h"

// Store per-player streaming state
// Note: RenderTarget and VideoProducer use TStrongObjectPtr to prevent GC
// while the render thread delegate is active. This fixes the crash in
// FVideoProducerRenderTarget::OnEndFrameRenderThread() when accessing GC'd objects.
struct FPlayerStreamingState
{
    int32 PlayerIndex = -1;
    TWeakObjectPtr<APawn> Pawn;
    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<USceneCaptureComponent2D> SceneCapture;
    TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget;  // Strong ref to prevent GC crash
    TStrongObjectPtr<UObject> VideoProducer;                // Strong ref to prevent GC crash
    TWeakObjectPtr<UActorComponent> StreamerComponent;      // Keep ref to streamer component
    FString StreamerId;
};

UCLASS()
class EMBODY_API UStreamingHelpers : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Setup streaming for a pawn with its own PlayerController and unique stream.
     *
     * Call this from your character Blueprint after setting the PlayerIndex variable.
     * This will:
     * 1. Create a new PlayerController for this index (if index > 0)
     * 2. Have that controller possess this pawn
     * 3. Configure the PixelStreaming2StreamerComponent with a unique StreamerId
     * 4. Start streaming
     *
     * @param Pawn - The pawn to set up (usually Self from your character BP)
     * @param PlayerIndex - 0 for main player, 1+ for additional players
     * @param OutPlayerController - The PlayerController now controlling this pawn
     * @param OutStreamerId - The StreamerId assigned to this player (e.g., "Streamer0", "Streamer1")
     * @return true if setup succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "Streaming", meta = (DisplayName = "Setup Player Streaming"))
    static bool SetupPlayerStreaming(APawn* Pawn, int32 PlayerIndex, APlayerController*& OutPlayerController, FString& OutStreamerId);

    /**
     * Setup streaming for a pawn, automatically determining PlayerIndex from tags.
     *
     * Looks for tags like "Metahuman0", "Metahuman1", etc. and extracts the index.
     * If no matching tag is found, defaults to PlayerIndex 0.
     *
     * @param Pawn - The pawn to set up
     * @param OutPlayerController - The PlayerController now controlling this pawn
     * @param OutPlayerIndex - The PlayerIndex that was determined from the tag
     * @return true if setup succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "Streaming", meta = (DisplayName = "Setup Player Streaming Auto"))
    static bool SetupPlayerStreamingAuto(APawn* Pawn, APlayerController*& OutPlayerController, int32& OutPlayerIndex);

    /**
     * Parse the player index from a tag like "Metahuman1" -> 1
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Streaming")
    static int32 ParsePlayerIndexFromTag(const FString& Tag);

    /**
     * Get the port number for a given player index.
     * Player 0 = 8080, Player 1 = 8082, Player 2 = 8083, etc.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Streaming")
    static int32 GetPortForPlayer(int32 PlayerIndex);

    /**
     * Get the PlayerController associated with a StreamerId.
     * Used for routing input from a specific streamer to its controller.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Streaming")
    static APlayerController* GetControllerForStreamer(const FString& StreamerId);

    /**
     * Get the PlayerIndex associated with a StreamerId.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Streaming")
    static int32 GetPlayerIndexForStreamer(const FString& StreamerId);

    /**
     * Get all active streaming states.
     */
    static TMap<FString, FPlayerStreamingState>& GetStreamingStates();

    /**
     * Add an Input Mapping Context to a PlayerController's Enhanced Input Subsystem.
     * This properly handles getting the subsystem from the LocalPlayer associated with the controller.
     *
     * @param Controller - The PlayerController to set up input for
     * @param MappingContext - The Input Mapping Context to add (e.g., CC_IMC)
     * @param Priority - Priority for this mapping context (default 0)
     * @return true if the mapping context was successfully added
     */
    UFUNCTION(BlueprintCallable, Category = "Streaming|Input")
    static bool AddInputMappingContextToPlayer(APlayerController* Controller, UInputMappingContext* MappingContext, int32 Priority = 0);

    /**
     * Inject raw mouse input for a specific player.
     * This injects MouseX and MouseY axis input into the player's input system,
     * which then flows through their Input Mapping Context (e.g., CC_IMC) naturally.
     * No specific Input Action needs to be specified - it uses whatever your IMC is configured with.
     *
     * @param PlayerIndex - The player index to inject input for (0, 1, 2, etc.)
     * @param DeltaX - Mouse X delta
     * @param DeltaY - Mouse Y delta
     * @return true if injection succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "Streaming|Input")
    static bool InjectRawMouseInputForPlayer(int32 PlayerIndex, float DeltaX, float DeltaY);

    /**
     * Inject a raw key input for a specific player.
     * This injects a key press/release into the player's input system,
     * which then flows through their Input Mapping Context naturally.
     *
     * @param PlayerIndex - The player index to inject input for
     * @param Key - The key to inject (e.g., EKeys::W, EKeys::SpaceBar)
     * @param bPressed - True for key down, false for key up
     * @return true if injection succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "Streaming|Input")
    static bool InjectRawKeyInputForPlayer(int32 PlayerIndex, FKey Key, bool bPressed);

    /**
     * Register custom input handlers for a streamer to route input to the correct PlayerController.
     * This sets up custom handlers that intercept mouse/keyboard input from PixelStreaming
     * and route it through the correct player's Enhanced Input system.
     *
     * Call this ONCE after SetupPlayerStreaming for each additional player (PlayerIndex > 0).
     * Player 0 uses the default input routing and doesn't need this.
     *
     * @param StreamerId - The streamer ID (e.g., "Streamer1") to register handlers for
     * @return true if handlers were successfully registered
     */
    UFUNCTION(BlueprintCallable, Category = "Streaming|Input")
    static bool RegisterInputHandlersForStreamer(const FString& StreamerId);

    /**
     * Set the look sensitivity multiplier for Streamer1+ input handlers.
     *
     * This adjusts how fast the camera rotates in response to mouse movement.
     * Use this to match Streamer1's look speed to Streamer0's.
     *
     * To calibrate:
     * 1. Add logging to your Character's Look() function to see what values Streamer0 receives
     * 2. Compare to the "Streamer1 Look" log output
     * 3. Adjust this multiplier until they match
     *
     * @param Sensitivity - Multiplier applied to pixel delta (default 1.0)
     */
    UFUNCTION(BlueprintCallable, Category = "Streaming|Input")
    static void SetLookSensitivity(float Sensitivity);

    /**
     * Get the current look sensitivity multiplier.
    */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Streaming|Input")
    static float GetLookSensitivity();

    /**
     * Log look input values for debugging/calibration.
     * Call this from BP_Lucy's IA_Look event to see what values Streamer0 receives.
     * Compare the output with "Streamer1 Look:" logs to calibrate sensitivity.
     *
     * @param StreamerName - Identifier for the log (e.g., "Streamer0")
     * @param LookX - The X (Yaw) component of the look input
     * @param LookY - The Y (Pitch) component of the look input
     */
    UFUNCTION(BlueprintCallable, Category = "Streaming|Debug")
    static void LogLookInput(const FString& StreamerName, float LookX, float LookY);

    /**
     * Cleanup streaming resources for a SINGLE player (by index).
     * Call this when removing a MetaHuman at RUNTIME (e.g., via SPWNDEL_ command).
     *
     * In Blueprint EndPlay, check EndPlayReason:
     * - If "Destroyed" → call this function (runtime deletion)
     * - If "End Play in Editor" or "Quit" → call CleanupAllPlayerStreaming instead
     *
     * @param PlayerIndex - The player index to cleanup (1, 2, etc.)
     */
    UFUNCTION(BlueprintCallable, Category = "Streaming|Cleanup", meta = (DisplayName = "Cleanup Single Player Streaming"))
    static void CleanupSinglePlayerStreaming(int32 PlayerIndex);

    /**
     * Cleanup ALL streaming resources and release orphaned objects.
     * Call this during PIE SHUTDOWN (EndPlayReason = EndPlayInEditor or Quit).
     *
     * In Blueprint EndPlay, check EndPlayReason:
     * - If "End Play in Editor" or "Quit" → call this function
     * - If "Destroyed" → call CleanupSinglePlayerStreaming instead
     *
     * This releases all references so the PIE World can be garbage collected.
     */
    UFUNCTION(BlueprintCallable, Category = "Streaming|Cleanup", meta = (DisplayName = "Cleanup All Player Streaming (Shutdown)"))
    static void CleanupAllPlayerStreaming();

    /**
     * Set the render target resolution for spawned player streams.
     * Call BEFORE spawning additional MetaHumans.
     * Default is 1920x1080.
     *
     * @param Width - Render target width in pixels
     * @param Height - Render target height in pixels
     */
    UFUNCTION(BlueprintCallable, Category = "Streaming|Quality")
    static void SetStreamingResolution(int32 Width, int32 Height);

    /**
     * Set the render target quality preset for spawned player streams.
     * Call BEFORE spawning additional MetaHumans.
     *
     * @param bHighQuality - true for high quality (TAA, full post-process), false for performance mode
     */
    UFUNCTION(BlueprintCallable, Category = "Streaming|Quality")
    static void SetStreamingQualityMode(bool bHighQuality);
};
