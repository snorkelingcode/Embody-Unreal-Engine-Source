// MultiInstanceManager.h
// Manages multi-instance pixel streaming with per-player MetaHuman assignment
//
// Copyright (c) 2025 Embody AI

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameFramework/PlayerController.h"
#include "MultiInstanceManager.generated.h"

class IPixelStreaming2Streamer;
class IPixelStreaming2InputHandler;

/**
 * Represents a single streaming instance with its associated player, MetaHuman, and streamer
 */
USTRUCT(BlueprintType)
struct FStreamingInstance
{
    GENERATED_BODY()

    /** Instance index (0, 1, 2...) */
    UPROPERTY(BlueprintReadOnly, Category = "Streaming")
    int32 InstanceIndex = 0;

    /** Unique streamer ID (Streamer0, Streamer1, etc.) */
    UPROPERTY(BlueprintReadOnly, Category = "Streaming")
    FString StreamerId;

    /** The PlayerController assigned to this instance */
    UPROPERTY(BlueprintReadOnly, Category = "Streaming")
    TWeakObjectPtr<APlayerController> PlayerController;

    /** The MetaHuman pawn assigned to this instance */
    UPROPERTY(BlueprintReadOnly, Category = "Streaming")
    TWeakObjectPtr<APawn> MetaHumanPawn;

    /** Frontend port this instance maps to (informational only - routing handled by signaling server) */
    UPROPERTY(BlueprintReadOnly, Category = "Streaming")
    int32 FrontendPort = 0;

    /** Whether this instance is currently connected */
    UPROPERTY(BlueprintReadOnly, Category = "Streaming")
    bool bIsConnected = false;

    /** Whether the streamer is registered with the module */
    UPROPERTY(BlueprintReadOnly, Category = "Streaming")
    bool bIsStreamerRegistered = false;
};

/**
 * Delegate fired when instance state changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInstanceStateChanged, int32, InstanceIndex, bool, bIsConnected);

/**
 * Delegate fired when a new instance is created
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInstanceCreated, const FStreamingInstance&, Instance);

/**
 * Manages multiple pixel streaming instances, each with its own PlayerController and MetaHuman
 */
UCLASS(BlueprintType, Blueprintable)
class EMBODY_API UMultiInstanceManager : public UObject
{
    GENERATED_BODY()

public:
    UMultiInstanceManager();

    // ============================================
    // INITIALIZATION
    // ============================================

    /**
     * Initialize the multi-instance manager
     * @param InWorld - World context
     * @param BaseFrontendPort - Starting frontend port for mapping (8080, 8081, etc. - informational)
     * @param MaxInstances - Maximum number of instances to support
     * Note: All streamers share the same signaling server. Differentiation is via StreamerId.
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    void Initialize(UWorld* InWorld, int32 BaseFrontendPort = 8080, int32 MaxInstances = 32);

    /**
     * Shutdown and cleanup all instances
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    void Shutdown();

    // ============================================
    // INSTANCE MANAGEMENT
    // ============================================

    /**
     * Create a new streaming instance
     * @param MetaHumanClass - Class of MetaHuman to spawn (optional - can be assigned later)
     * @param SpawnTransform - Where to spawn the MetaHuman
     * @return Index of the created instance, or -1 on failure
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    int32 CreateInstance(TSubclassOf<APawn> MetaHumanClass = nullptr, FTransform SpawnTransform = FTransform());

    /**
     * Remove a streaming instance
     * @param InstanceIndex - Index of instance to remove
     * @return True if successfully removed
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    bool RemoveInstance(int32 InstanceIndex);

    /**
     * Assign an existing MetaHuman pawn to an instance
     * @param InstanceIndex - Index of instance
     * @param MetaHumanPawn - The pawn to assign
     * @return True if successfully assigned
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    bool AssignMetaHuman(int32 InstanceIndex, APawn* MetaHumanPawn);

    /**
     * Assign a PlayerController to an instance
     * @param InstanceIndex - Index of instance
     * @param Controller - The controller to assign
     * @return True if successfully assigned
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    bool AssignPlayerController(int32 InstanceIndex, APlayerController* Controller);

    /**
     * Have the PlayerController possess the MetaHuman for this instance
     * @param InstanceIndex - Index of instance
     * @return True if possession succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    bool PossessMetaHuman(int32 InstanceIndex);

    // ============================================
    // STREAMER MANAGEMENT
    // ============================================

    /**
     * Register a streamer for an instance with the Pixel Streaming module
     * @param InstanceIndex - Index of instance
     * @return True if streamer was registered
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    bool RegisterStreamer(int32 InstanceIndex);

    /**
     * Unregister a streamer from the Pixel Streaming module
     * @param InstanceIndex - Index of instance
     * @return True if streamer was unregistered
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    bool UnregisterStreamer(int32 InstanceIndex);

    /**
     * Set up a command handler for an instance's data channel
     * @param InstanceIndex - Index of instance
     * @param CommandName - Name of the command to handle
     * @return True if handler was set up
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    bool SetupCommandHandler(int32 InstanceIndex, const FString& CommandName);

    // ============================================
    // QUERIES
    // ============================================

    /**
     * Get all active instances
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    TArray<FStreamingInstance> GetAllInstances() const;

    /**
     * Get a specific instance by index
     * @param InstanceIndex - Index of instance
     * @param OutInstance - Output instance data
     * @return True if instance exists
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    bool GetInstance(int32 InstanceIndex, FStreamingInstance& OutInstance) const;

    /**
     * Get instance by streamer ID
     * @param StreamerId - The streamer ID (Streamer0, Streamer1, etc.)
     * @param OutInstance - Output instance data
     * @return True if instance exists
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    bool GetInstanceByStreamerId(const FString& StreamerId, FStreamingInstance& OutInstance) const;

    /**
     * Get the number of active instances
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    int32 GetInstanceCount() const;

    /**
     * Get the number of connected instances
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    int32 GetConnectedInstanceCount() const;

    /**
     * Check if an instance exists
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    bool IsValidInstance(int32 InstanceIndex) const;

    // ============================================
    // BULK OPERATIONS
    // ============================================

    /**
     * Create multiple instances at once
     * @param Count - Number of instances to create
     * @param MetaHumanClass - Class to spawn for each
     * @param SpawnLocations - Array of spawn transforms (must match Count)
     * @return Number of instances successfully created
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    int32 CreateMultipleInstances(int32 Count, TSubclassOf<APawn> MetaHumanClass, const TArray<FTransform>& SpawnLocations);

    /**
     * Register all unregistered streamers
     * @return Number of streamers registered
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    int32 RegisterAllStreamers();

    /**
     * Possess all MetaHumans with their assigned controllers
     * @return Number of successful possessions
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance")
    int32 PossessAllMetaHumans();

    // ============================================
    // EVENTS
    // ============================================

    /** Fired when an instance's connection state changes */
    UPROPERTY(BlueprintAssignable, Category = "Multi-Instance")
    FOnInstanceStateChanged OnInstanceStateChanged;

    /** Fired when a new instance is created */
    UPROPERTY(BlueprintAssignable, Category = "Multi-Instance")
    FOnInstanceCreated OnInstanceCreated;

    // ============================================
    // STATIC HELPERS
    // ============================================

    /**
     * Get the singleton instance (creates one if needed)
     * @param WorldContext - World context object
     * @return The multi-instance manager singleton
     */
    UFUNCTION(BlueprintCallable, Category = "Multi-Instance", meta = (WorldContext = "WorldContext"))
    static UMultiInstanceManager* Get(UObject* WorldContext);

    /**
     * Generate a streamer ID from an index
     */
    UFUNCTION(BlueprintPure, Category = "Multi-Instance")
    static FString MakeStreamerId(int32 Index);

protected:
    /** World reference */
    UPROPERTY()
    TWeakObjectPtr<UWorld> World;

    /** All streaming instances */
    UPROPERTY()
    TMap<int32, FStreamingInstance> Instances;

    /** Streamers for each instance (not UPROPERTY since they're shared pointers) */
    TMap<int32, TSharedPtr<IPixelStreaming2Streamer>> Streamers;

    /** Input handlers for each instance */
    TMap<int32, TSharedPtr<IPixelStreaming2InputHandler>> InputHandlers;

    /** Base frontend port (informational - all streamers share signaling server) */
    int32 BaseFrontendPort;

    /** Maximum allowed instances */
    int32 MaxInstanceCount;

    /** Next available instance index */
    int32 NextInstanceIndex;

    /** Whether the manager has been initialized */
    bool bIsInitialized;

private:
    /** Internal helper to create streamer ID */
    FString GenerateStreamerId(int32 Index) const;

    /** Internal helper to calculate frontend port for instance (informational) */
    int32 CalculateFrontendPort(int32 Index) const;

    /** Log helper */
    void LogInfo(const FString& Message) const;
    void LogWarning(const FString& Message) const;
    void LogError(const FString& Message) const;
};
