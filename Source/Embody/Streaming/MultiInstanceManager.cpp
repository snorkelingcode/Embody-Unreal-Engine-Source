// MultiInstanceManager.cpp
// Implementation of multi-instance pixel streaming management
//
// Copyright (c) 2025 Embody AI

#include "MultiInstanceManager.h"
#include "Modules/ModuleManager.h"
#include "IPixelStreaming2Module.h"
#include "IPixelStreaming2Streamer.h"
#include "IPixelStreaming2InputHandler.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"

// Log category
DEFINE_LOG_CATEGORY_STATIC(LogMultiInstance, Log, All);

// ============================================
// STATIC SINGLETON
// ============================================

static TWeakObjectPtr<UMultiInstanceManager> GMultiInstanceManager;

UMultiInstanceManager* UMultiInstanceManager::Get(UObject* WorldContext)
{
    if (GMultiInstanceManager.IsValid())
    {
        return GMultiInstanceManager.Get();
    }

    if (!WorldContext)
    {
        UE_LOG(LogMultiInstance, Warning, TEXT("Get() called with null WorldContext"));
        return nullptr;
    }

    UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        return nullptr;
    }

    UMultiInstanceManager* Manager = NewObject<UMultiInstanceManager>(World);
    Manager->AddToRoot(); // Prevent garbage collection
    GMultiInstanceManager = Manager;

    return Manager;
}

// ============================================
// CONSTRUCTOR
// ============================================

UMultiInstanceManager::UMultiInstanceManager()
    : BaseFrontendPort(8080)
    , MaxInstanceCount(32)
    , NextInstanceIndex(0)
    , bIsInitialized(false)
{
}

// ============================================
// INITIALIZATION
// ============================================

void UMultiInstanceManager::Initialize(UWorld* InWorld, int32 InBaseFrontendPort, int32 MaxInstances)
{
    if (bIsInitialized)
    {
        LogWarning(TEXT("Already initialized"));
        return;
    }

    World = InWorld;
    BaseFrontendPort = InBaseFrontendPort;
    MaxInstanceCount = FMath::Clamp(MaxInstances, 1, 64);
    NextInstanceIndex = 0;
    bIsInitialized = true;

    LogInfo(FString::Printf(TEXT("Initialized: BaseFrontendPort=%d, MaxInstances=%d"), BaseFrontendPort, MaxInstances));
    LogInfo(TEXT("Note: All streamers share the same signaling server. StreamerId differentiates connections."));
}

void UMultiInstanceManager::Shutdown()
{
    if (!bIsInitialized)
    {
        return;
    }

    LogInfo(TEXT("Shutting down..."));

    // Unregister all streamers
    for (auto& Pair : Instances)
    {
        UnregisterStreamer(Pair.Key);
    }

    // Clear all data
    InputHandlers.Empty();
    Streamers.Empty();
    Instances.Empty();

    bIsInitialized = false;

    // Remove from root to allow GC
    RemoveFromRoot();
    GMultiInstanceManager.Reset();

    LogInfo(TEXT("Shutdown complete"));
}

// ============================================
// INSTANCE MANAGEMENT
// ============================================

int32 UMultiInstanceManager::CreateInstance(TSubclassOf<APawn> MetaHumanClass, FTransform SpawnTransform)
{
    if (!bIsInitialized)
    {
        LogError(TEXT("CreateInstance: Not initialized"));
        return -1;
    }

    if (Instances.Num() >= MaxInstanceCount)
    {
        LogError(FString::Printf(TEXT("CreateInstance: Maximum instances (%d) reached"), MaxInstanceCount));
        return -1;
    }

    int32 Index = NextInstanceIndex++;

    // Create the instance data
    FStreamingInstance NewInstance;
    NewInstance.InstanceIndex = Index;
    NewInstance.StreamerId = GenerateStreamerId(Index);
    NewInstance.FrontendPort = CalculateFrontendPort(Index);
    NewInstance.bIsConnected = false;
    NewInstance.bIsStreamerRegistered = false;

    // Spawn MetaHuman if class provided
    if (MetaHumanClass && World.IsValid())
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        APawn* SpawnedPawn = World->SpawnActor<APawn>(MetaHumanClass, SpawnTransform, SpawnParams);
        if (SpawnedPawn)
        {
            NewInstance.MetaHumanPawn = SpawnedPawn;
            LogInfo(FString::Printf(TEXT("Instance %d: Spawned MetaHuman at %s"),
                Index, *SpawnTransform.GetLocation().ToString()));
        }
        else
        {
            LogWarning(FString::Printf(TEXT("Instance %d: Failed to spawn MetaHuman"), Index));
        }
    }

    Instances.Add(Index, NewInstance);

    LogInfo(FString::Printf(TEXT("Created instance %d: StreamerId=%s, FrontendPort=%d"),
        Index, *NewInstance.StreamerId, NewInstance.FrontendPort));

    OnInstanceCreated.Broadcast(NewInstance);

    return Index;
}

bool UMultiInstanceManager::RemoveInstance(int32 InstanceIndex)
{
    if (!IsValidInstance(InstanceIndex))
    {
        LogWarning(FString::Printf(TEXT("RemoveInstance: Invalid index %d"), InstanceIndex));
        return false;
    }

    // Unregister streamer first
    UnregisterStreamer(InstanceIndex);

    // Get instance before removing
    FStreamingInstance* Instance = Instances.Find(InstanceIndex);
    if (Instance)
    {
        // Destroy spawned MetaHuman if we own it
        if (Instance->MetaHumanPawn.IsValid())
        {
            Instance->MetaHumanPawn->Destroy();
        }
    }

    // Remove from maps
    InputHandlers.Remove(InstanceIndex);
    Streamers.Remove(InstanceIndex);
    Instances.Remove(InstanceIndex);

    LogInfo(FString::Printf(TEXT("Removed instance %d"), InstanceIndex));
    return true;
}

bool UMultiInstanceManager::AssignMetaHuman(int32 InstanceIndex, APawn* MetaHumanPawn)
{
    FStreamingInstance* Instance = Instances.Find(InstanceIndex);
    if (!Instance)
    {
        LogWarning(FString::Printf(TEXT("AssignMetaHuman: Invalid index %d"), InstanceIndex));
        return false;
    }

    Instance->MetaHumanPawn = MetaHumanPawn;
    LogInfo(FString::Printf(TEXT("Instance %d: Assigned MetaHuman %s"),
        InstanceIndex, MetaHumanPawn ? *MetaHumanPawn->GetName() : TEXT("(null)")));

    return true;
}

bool UMultiInstanceManager::AssignPlayerController(int32 InstanceIndex, APlayerController* Controller)
{
    FStreamingInstance* Instance = Instances.Find(InstanceIndex);
    if (!Instance)
    {
        LogWarning(FString::Printf(TEXT("AssignPlayerController: Invalid index %d"), InstanceIndex));
        return false;
    }

    Instance->PlayerController = Controller;
    LogInfo(FString::Printf(TEXT("Instance %d: Assigned PlayerController %s"),
        InstanceIndex, Controller ? *Controller->GetName() : TEXT("(null)")));

    return true;
}

bool UMultiInstanceManager::PossessMetaHuman(int32 InstanceIndex)
{
    FStreamingInstance* Instance = Instances.Find(InstanceIndex);
    if (!Instance)
    {
        LogWarning(FString::Printf(TEXT("PossessMetaHuman: Invalid index %d"), InstanceIndex));
        return false;
    }

    if (!Instance->PlayerController.IsValid())
    {
        LogWarning(FString::Printf(TEXT("Instance %d: No PlayerController assigned"), InstanceIndex));
        return false;
    }

    if (!Instance->MetaHumanPawn.IsValid())
    {
        LogWarning(FString::Printf(TEXT("Instance %d: No MetaHuman assigned"), InstanceIndex));
        return false;
    }

    Instance->PlayerController->Possess(Instance->MetaHumanPawn.Get());
    LogInfo(FString::Printf(TEXT("Instance %d: PlayerController possessed MetaHuman"), InstanceIndex));

    return true;
}

// ============================================
// STREAMER MANAGEMENT
// ============================================

bool UMultiInstanceManager::RegisterStreamer(int32 InstanceIndex)
{
    FStreamingInstance* Instance = Instances.Find(InstanceIndex);
    if (!Instance)
    {
        LogWarning(FString::Printf(TEXT("RegisterStreamer: Invalid index %d"), InstanceIndex));
        return false;
    }

    if (Instance->bIsStreamerRegistered)
    {
        LogWarning(FString::Printf(TEXT("Instance %d: Streamer already registered"), InstanceIndex));
        return true;
    }

    if (!FModuleManager::Get().IsModuleLoaded("PixelStreaming2"))
    {
        LogError(TEXT("PixelStreaming2 module not loaded"));
        return false;
    }

    IPixelStreaming2Module& PS2Module = IPixelStreaming2Module::Get();

    // Check if streamer already exists
    TSharedPtr<IPixelStreaming2Streamer> ExistingStreamer = PS2Module.FindStreamer(Instance->StreamerId);
    if (ExistingStreamer.IsValid())
    {
        Streamers.Add(InstanceIndex, ExistingStreamer);
        Instance->bIsStreamerRegistered = true;
        LogInfo(FString::Printf(TEXT("Instance %d: Using existing streamer %s"),
            InstanceIndex, *Instance->StreamerId));
        return true;
    }

    // Create new streamer
    TSharedPtr<IPixelStreaming2Streamer> NewStreamer = PS2Module.CreateStreamer(Instance->StreamerId);
    if (!NewStreamer.IsValid())
    {
        LogError(FString::Printf(TEXT("Instance %d: Failed to create streamer"), InstanceIndex));
        return false;
    }

    Streamers.Add(InstanceIndex, NewStreamer);
    Instance->bIsStreamerRegistered = true;

    LogInfo(FString::Printf(TEXT("Instance %d: Registered streamer %s"),
        InstanceIndex, *Instance->StreamerId));

    return true;
}

bool UMultiInstanceManager::UnregisterStreamer(int32 InstanceIndex)
{
    FStreamingInstance* Instance = Instances.Find(InstanceIndex);
    if (!Instance)
    {
        return false;
    }

    if (!Instance->bIsStreamerRegistered)
    {
        return true;
    }

    // Clear input handler
    InputHandlers.Remove(InstanceIndex);

    // Remove streamer reference
    TSharedPtr<IPixelStreaming2Streamer>* StreamerPtr = Streamers.Find(InstanceIndex);
    if (StreamerPtr && StreamerPtr->IsValid())
    {
        if (FModuleManager::Get().IsModuleLoaded("PixelStreaming2"))
        {
            IPixelStreaming2Module& PS2Module = IPixelStreaming2Module::Get();
            PS2Module.DeleteStreamer(Instance->StreamerId);
        }
    }

    Streamers.Remove(InstanceIndex);
    Instance->bIsStreamerRegistered = false;

    LogInfo(FString::Printf(TEXT("Instance %d: Unregistered streamer %s"),
        InstanceIndex, *Instance->StreamerId));

    return true;
}

bool UMultiInstanceManager::SetupCommandHandler(int32 InstanceIndex, const FString& CommandName)
{
    FStreamingInstance* Instance = Instances.Find(InstanceIndex);
    if (!Instance)
    {
        LogWarning(FString::Printf(TEXT("SetupCommandHandler: Invalid index %d"), InstanceIndex));
        return false;
    }

    TSharedPtr<IPixelStreaming2Streamer>* StreamerPtr = Streamers.Find(InstanceIndex);
    if (!StreamerPtr || !StreamerPtr->IsValid())
    {
        LogWarning(FString::Printf(TEXT("Instance %d: No streamer registered"), InstanceIndex));
        return false;
    }

    TSharedPtr<IPixelStreaming2InputHandler> Handler = (*StreamerPtr)->GetInputHandler().Pin();
    if (!Handler.IsValid())
    {
        LogWarning(FString::Printf(TEXT("Instance %d: Could not get input handler"), InstanceIndex));
        return false;
    }

    InputHandlers.Add(InstanceIndex, Handler);

    // Set up command handler
    int32 CapturedIndex = InstanceIndex;
    Handler->SetCommandHandler(*CommandName,
        [this, CapturedIndex](FString SourceId, FString Descriptor, FString CommandString)
        {
            AsyncTask(ENamedThreads::GameThread, [this, CapturedIndex, CommandString]()
            {
                LogInfo(FString::Printf(TEXT("Instance %d received command: %s"),
                    CapturedIndex, *CommandString));
                // Could broadcast an event here for Blueprint handling
            });
        }
    );

    LogInfo(FString::Printf(TEXT("Instance %d: Set up command handler for '%s'"),
        InstanceIndex, *CommandName));

    return true;
}

// ============================================
// QUERIES
// ============================================

TArray<FStreamingInstance> UMultiInstanceManager::GetAllInstances() const
{
    TArray<FStreamingInstance> Result;
    for (const auto& Pair : Instances)
    {
        Result.Add(Pair.Value);
    }
    return Result;
}

bool UMultiInstanceManager::GetInstance(int32 InstanceIndex, FStreamingInstance& OutInstance) const
{
    const FStreamingInstance* Instance = Instances.Find(InstanceIndex);
    if (Instance)
    {
        OutInstance = *Instance;
        return true;
    }
    return false;
}

bool UMultiInstanceManager::GetInstanceByStreamerId(const FString& StreamerId, FStreamingInstance& OutInstance) const
{
    for (const auto& Pair : Instances)
    {
        if (Pair.Value.StreamerId == StreamerId)
        {
            OutInstance = Pair.Value;
            return true;
        }
    }
    return false;
}

int32 UMultiInstanceManager::GetInstanceCount() const
{
    return Instances.Num();
}

int32 UMultiInstanceManager::GetConnectedInstanceCount() const
{
    int32 Count = 0;
    for (const auto& Pair : Instances)
    {
        if (Pair.Value.bIsConnected)
        {
            Count++;
        }
    }
    return Count;
}

bool UMultiInstanceManager::IsValidInstance(int32 InstanceIndex) const
{
    return Instances.Contains(InstanceIndex);
}

// ============================================
// BULK OPERATIONS
// ============================================

int32 UMultiInstanceManager::CreateMultipleInstances(int32 Count, TSubclassOf<APawn> MetaHumanClass, const TArray<FTransform>& SpawnLocations)
{
    if (SpawnLocations.Num() < Count)
    {
        LogWarning(FString::Printf(TEXT("CreateMultipleInstances: Not enough spawn locations (%d) for %d instances"),
            SpawnLocations.Num(), Count));
        Count = SpawnLocations.Num();
    }

    int32 SuccessCount = 0;
    for (int32 i = 0; i < Count; i++)
    {
        int32 Index = CreateInstance(MetaHumanClass, SpawnLocations[i]);
        if (Index >= 0)
        {
            SuccessCount++;
        }
    }

    LogInfo(FString::Printf(TEXT("Created %d/%d instances"), SuccessCount, Count));
    return SuccessCount;
}

int32 UMultiInstanceManager::RegisterAllStreamers()
{
    int32 Count = 0;
    for (auto& Pair : Instances)
    {
        if (!Pair.Value.bIsStreamerRegistered)
        {
            if (RegisterStreamer(Pair.Key))
            {
                Count++;
            }
        }
    }
    LogInfo(FString::Printf(TEXT("Registered %d streamers"), Count));
    return Count;
}

int32 UMultiInstanceManager::PossessAllMetaHumans()
{
    int32 Count = 0;
    for (auto& Pair : Instances)
    {
        if (PossessMetaHuman(Pair.Key))
        {
            Count++;
        }
    }
    LogInfo(FString::Printf(TEXT("Possessed %d MetaHumans"), Count));
    return Count;
}

// ============================================
// STATIC HELPERS
// ============================================

FString UMultiInstanceManager::MakeStreamerId(int32 Index)
{
    return FString::Printf(TEXT("Streamer%d"), Index);
}

// ============================================
// INTERNAL HELPERS
// ============================================

FString UMultiInstanceManager::GenerateStreamerId(int32 Index) const
{
    return MakeStreamerId(Index);
}

int32 UMultiInstanceManager::CalculateFrontendPort(int32 Index) const
{
    return BaseFrontendPort + Index;
}

void UMultiInstanceManager::LogInfo(const FString& Message) const
{
    UE_LOG(LogMultiInstance, Log, TEXT("[MultiInstance] %s"), *Message);
}

void UMultiInstanceManager::LogWarning(const FString& Message) const
{
    UE_LOG(LogMultiInstance, Warning, TEXT("[MultiInstance] %s"), *Message);
}

void UMultiInstanceManager::LogError(const FString& Message) const
{
    UE_LOG(LogMultiInstance, Error, TEXT("[MultiInstance] %s"), *Message);
}
