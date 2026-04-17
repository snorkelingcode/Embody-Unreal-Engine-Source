// StreamingHelpers.cpp
// Copyright (c) 2025 Embody AI

#include "StreamingHelpers.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderingThread.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Components/ActorComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Camera/CameraComponent.h"
#include "IPixelStreaming2Module.h"
#include "IPixelStreaming2Streamer.h"
#include "IPixelStreaming2InputHandler.h"
#include "TimerManager.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogStreamingHelpers, Log, All);

// Track which pawns have already been set up to avoid duplicate calls
static TSet<TWeakObjectPtr<APawn>> SetupPawns;

// Store streaming states for each player
static TMap<FString, FPlayerStreamingState> StreamingStates;

// Track mouse button state per player (for click-to-look behavior)
static TMap<int32, bool> PlayerRightMouseHeld;
static TMap<int32, bool> PlayerLeftMouseHeld;

// Look sensitivity multiplier - tune this to match Streamer0's sensitivity
// If look is too fast, decrease this. If too slow, increase.
// Browser sends pixel deltas as int16. Streamer0 receives similar pixel deltas from Enhanced Input.
// Start with 1.0 and adjust based on actual measurement.
// To measure: Move mouse same distance on both streams, compare logged values, calculate ratio.
static float LookSensitivityMultiplier = 1.0f;

// Render target settings for spawned player streams
static int32 StreamingResolutionWidth = 1920;
static int32 StreamingResolutionHeight = 1080;
static bool bHighQualityStreaming = false;  // Default to performance mode to match main viewport

TMap<FString, FPlayerStreamingState>& UStreamingHelpers::GetStreamingStates()
{
    return StreamingStates;
}

APlayerController* UStreamingHelpers::GetControllerForStreamer(const FString& StreamerId)
{
    if (FPlayerStreamingState* State = StreamingStates.Find(StreamerId))
    {
        return State->Controller.Get();
    }
    return nullptr;
}

int32 UStreamingHelpers::GetPlayerIndexForStreamer(const FString& StreamerId)
{
    if (FPlayerStreamingState* State = StreamingStates.Find(StreamerId))
    {
        return State->PlayerIndex;
    }
    return -1;
}

// Forward declare the streamer component class - we'll find it by name to avoid hard dependency
static UClass* GetStreamerComponentClass()
{
    static UClass* CachedClass = nullptr;
    if (!CachedClass)
    {
        CachedClass = FindObject<UClass>(nullptr, TEXT("/Script/PixelStreaming2.PixelStreaming2StreamerComponent"));
    }
    return CachedClass;
}

// Get the VideoProducerRenderTarget class
static UClass* GetVideoProducerRenderTargetClass()
{
    static UClass* CachedClass = nullptr;
    if (!CachedClass)
    {
        CachedClass = FindObject<UClass>(nullptr, TEXT("/Script/PixelStreaming2.PixelStreaming2VideoProducerRenderTarget"));
    }
    return CachedClass;
}

// Create a SceneCapture setup for a pawn (used for PlayerIndex > 0)
// Configured to match Pixel Streaming's main viewport quality
static USceneCaptureComponent2D* CreateSceneCaptureForPawn(APawn* Pawn, int32 PlayerIndex, UTextureRenderTarget2D*& OutRenderTarget)
{
    OutRenderTarget = nullptr;

    // Create RenderTarget matching main viewport format (PF_B8G8R8A8)
    // This ensures consistent quality and performance across all streams
    OutRenderTarget = NewObject<UTextureRenderTarget2D>(Pawn, *FString::Printf(TEXT("StreamingRenderTarget_%d"), PlayerIndex));
    OutRenderTarget->RenderTargetFormat = RTF_RGBA8;  // Match main viewport format
    OutRenderTarget->InitCustomFormat(StreamingResolutionWidth, StreamingResolutionHeight, PF_B8G8R8A8, false);  // false = sRGB gamma
    OutRenderTarget->bAutoGenerateMips = false;
    OutRenderTarget->UpdateResourceImmediate(true);

    // Create SceneCaptureComponent2D
    USceneCaptureComponent2D* SceneCapture = NewObject<USceneCaptureComponent2D>(Pawn, *FString::Printf(TEXT("StreamingSceneCapture_%d"), PlayerIndex));
    SceneCapture->RegisterComponent();
    Pawn->AddInstanceComponent(SceneCapture);

    // Configure the scene capture to match main viewport quality
    SceneCapture->TextureTarget = OutRenderTarget;
    SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalToneCurveHDR;  // Full post-process chain
    SceneCapture->bCaptureEveryFrame = true;
    SceneCapture->bCaptureOnMovement = false;
    SceneCapture->bAlwaysPersistRenderingState = true;  // Maintains quality across frames

    // Match main viewport quality settings - don't add extra effects
    // Use default ShowFlags which inherit from the main viewport
    if (bHighQualityStreaming)
    {
        // High quality: Enable TAA and full post-process
        SceneCapture->ShowFlags.SetTemporalAA(true);
        SceneCapture->ShowFlags.SetAntiAliasing(true);
        SceneCapture->ShowFlags.SetMotionBlur(false);  // Disable motion blur for cleaner streaming
        SceneCapture->ShowFlags.SetBloom(true);
        SceneCapture->ShowFlags.SetToneCurve(true);
        SceneCapture->ShowFlags.SetTonemapper(true);
        SceneCapture->ShowFlags.SetPostProcessing(true);

        UE_LOG(LogStreamingHelpers, Log, TEXT("CreateSceneCaptureForPawn: High quality mode (TAA, post-process)"));
    }
    else
    {
        // Performance mode - minimal post-process, matches main viewport baseline
        SceneCapture->ShowFlags.SetTemporalAA(true);
        SceneCapture->ShowFlags.SetAntiAliasing(true);
        SceneCapture->ShowFlags.SetMotionBlur(false);
        SceneCapture->ShowFlags.SetBloom(true);
        SceneCapture->ShowFlags.SetToneCurve(true);
        SceneCapture->ShowFlags.SetTonemapper(true);
        SceneCapture->ShowFlags.SetPostProcessing(true);

        // Disable expensive effects for performance
        SceneCapture->ShowFlags.SetAmbientOcclusion(false);
        SceneCapture->ShowFlags.SetScreenSpaceReflections(false);
        SceneCapture->ShowFlags.SetEyeAdaptation(false);

        UE_LOG(LogStreamingHelpers, Log, TEXT("CreateSceneCaptureForPawn: Performance mode (minimal post-process)"));
    }

    // Find and attach to the pawn's camera
    UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>();
    if (Camera)
    {
        SceneCapture->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        SceneCapture->FOVAngle = Camera->FieldOfView;

        // Copy camera's post-process settings if available
        if (Camera->PostProcessSettings.bOverride_ColorGamma)
        {
            SceneCapture->PostProcessSettings.bOverride_ColorGamma = true;
            SceneCapture->PostProcessSettings.ColorGamma = Camera->PostProcessSettings.ColorGamma;
        }

        UE_LOG(LogStreamingHelpers, Log, TEXT("CreateSceneCaptureForPawn: Attached SceneCapture to camera with FOV %.1f"), Camera->FieldOfView);
    }
    else
    {
        // Attach to root if no camera found
        SceneCapture->AttachToComponent(Pawn->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        UE_LOG(LogStreamingHelpers, Warning, TEXT("CreateSceneCaptureForPawn: No camera found, attached to root component"));
    }

    UE_LOG(LogStreamingHelpers, Log, TEXT("CreateSceneCaptureForPawn: Created SceneCapture (%dx%d) and RenderTarget for PlayerIndex %d"),
        StreamingResolutionWidth, StreamingResolutionHeight, PlayerIndex);
    return SceneCapture;
}

bool UStreamingHelpers::SetupPlayerStreaming(APawn* Pawn, int32 PlayerIndex, APlayerController*& OutPlayerController, FString& OutStreamerId)
{
    OutPlayerController = nullptr;
    OutStreamerId = FString::Printf(TEXT("Streamer%d"), PlayerIndex);

    if (!Pawn)
    {
        UE_LOG(LogStreamingHelpers, Error, TEXT("SetupPlayerStreaming: Pawn is null"));
        return false;
    }

    // Check if this pawn was already set up
    TWeakObjectPtr<APawn> WeakPawn(Pawn);
    if (SetupPawns.Contains(WeakPawn))
    {
        UE_LOG(LogStreamingHelpers, Warning, TEXT("SetupPlayerStreaming: Pawn %s already set up, skipping"), *Pawn->GetName());
        // Still return the controller if we can find it
        if (PlayerIndex == 0)
        {
            OutPlayerController = UGameplayStatics::GetPlayerController(Pawn->GetWorld(), 0);
        }
        else if (UGameInstance* GI = Pawn->GetWorld()->GetGameInstance())
        {
            if (ULocalPlayer* LP = GI->GetLocalPlayerByIndex(PlayerIndex))
            {
                OutPlayerController = LP->GetPlayerController(Pawn->GetWorld());
            }
        }
        return OutPlayerController != nullptr;
    }

    UWorld* World = Pawn->GetWorld();
    if (!World)
    {
        UE_LOG(LogStreamingHelpers, Error, TEXT("SetupPlayerStreaming: Could not get world"));
        return false;
    }

    UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: Setting up PlayerIndex %d for pawn %s"), PlayerIndex, *Pawn->GetName());

    // 1. Find the PixelStreaming2StreamerComponent on the pawn
    UClass* StreamerCompClass = GetStreamerComponentClass();
    UActorComponent* StreamerComp = nullptr;

    if (StreamerCompClass)
    {
        StreamerComp = Pawn->GetComponentByClass(StreamerCompClass);
    }

    if (!StreamerComp)
    {
        UE_LOG(LogStreamingHelpers, Warning, TEXT("SetupPlayerStreaming: No PixelStreaming2StreamerComponent found on %s"), *Pawn->GetName());

        // List all components for debugging
        TArray<UActorComponent*> AllComponents;
        Pawn->GetComponents(AllComponents);
        UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: Pawn has %d components:"), AllComponents.Num());
        for (UActorComponent* Comp : AllComponents)
        {
            UE_LOG(LogStreamingHelpers, Log, TEXT("  - %s (%s)"), *Comp->GetName(), *Comp->GetClass()->GetName());
        }
    }
    else
    {
        // 2. Set the StreamerId property to match the player index
        FString NewStreamerId = FString::Printf(TEXT("Streamer%d"), PlayerIndex);

        // Use reflection to set the StreamerId property
        FProperty* StreamerIdProp = StreamerCompClass->FindPropertyByName(TEXT("StreamerId"));
        if (StreamerIdProp)
        {
            FString* StreamerIdPtr = StreamerIdProp->ContainerPtrToValuePtr<FString>(StreamerComp);
            if (StreamerIdPtr)
            {
                FString OldStreamerId = *StreamerIdPtr;
                *StreamerIdPtr = NewStreamerId;
                UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: Changed StreamerId from '%s' to '%s'"), *OldStreamerId, *NewStreamerId);

                // Verify it was set
                FString VerifyStreamerId = *StreamerIdPtr;
                UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: Verified StreamerId is now '%s'"), *VerifyStreamerId);
            }
        }
        else
        {
            UE_LOG(LogStreamingHelpers, Error, TEXT("SetupPlayerStreaming: Could not find StreamerId property!"));
        }

        // ALWAYS set the SignallingServerURL to ensure it's configured
        FProperty* UrlProp = StreamerCompClass->FindPropertyByName(TEXT("SignallingServerURL"));
        if (UrlProp)
        {
            FString* UrlPtr = UrlProp->ContainerPtrToValuePtr<FString>(StreamerComp);
            if (UrlPtr)
            {
                *UrlPtr = TEXT("ws://127.0.0.1:8888");
                UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: Set SignallingServerURL to ws://127.0.0.1:8888"));
            }
        }

        // Make sure UsePixelStreamingURL is false so it uses our URL
        FProperty* UseUrlProp = StreamerCompClass->FindPropertyByName(TEXT("UsePixelStreamingURL"));
        if (UseUrlProp)
        {
            bool* UseUrlPtr = UseUrlProp->ContainerPtrToValuePtr<bool>(StreamerComp);
            if (UseUrlPtr)
            {
                *UseUrlPtr = false;
            }
        }

        // For PlayerIndex > 0, set up SceneCapture-based video producer
        // PlayerIndex 0 uses the main viewport (default behavior)
        // IMPORTANT: We need to store strong references to prevent GC crash
        FString TempStreamerId = FString::Printf(TEXT("Streamer%d"), PlayerIndex);
        FPlayerStreamingState& State = StreamingStates.FindOrAdd(TempStreamerId);
        State.StreamerComponent = StreamerComp;

        if (PlayerIndex > 0)
        {
            UClass* VideoProducerClass = GetVideoProducerRenderTargetClass();
            if (VideoProducerClass)
            {
                // Create RenderTarget and SceneCapture
                UTextureRenderTarget2D* RenderTarget = nullptr;
                USceneCaptureComponent2D* SceneCapture = CreateSceneCaptureForPawn(Pawn, PlayerIndex, RenderTarget);

                if (RenderTarget && SceneCapture)
                {
                    // Store strong references IMMEDIATELY to prevent GC
                    // This is critical - the render thread delegate will crash if these are GC'd
                    State.RenderTarget.Reset(RenderTarget);
                    State.SceneCapture = SceneCapture;

                    // Create VideoProducerRenderTarget
                    UObject* VideoProducer = NewObject<UObject>(StreamerComp, VideoProducerClass, *FString::Printf(TEXT("VideoProducer_%d"), PlayerIndex));

                    // Set the Target property on the VideoProducer
                    FProperty* TargetProp = VideoProducerClass->FindPropertyByName(TEXT("Target"));
                    if (TargetProp)
                    {
                        UTextureRenderTarget2D** TargetPtr = TargetProp->ContainerPtrToValuePtr<UTextureRenderTarget2D*>(VideoProducer);
                        if (TargetPtr)
                        {
                            *TargetPtr = RenderTarget;
                            UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: Set VideoProducer Target to RenderTarget"));
                        }
                    }

                    // Set the VideoProducer property on the StreamerComponent
                    FProperty* VideoProducerProp = StreamerCompClass->FindPropertyByName(TEXT("VideoProducer"));
                    if (VideoProducerProp)
                    {
                        UObject** VideoProducerPtr = VideoProducerProp->ContainerPtrToValuePtr<UObject*>(StreamerComp);
                        if (VideoProducerPtr)
                        {
                            *VideoProducerPtr = VideoProducer;
                            UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: Set StreamerComponent VideoProducer for PlayerIndex %d"), PlayerIndex);
                        }
                    }

                    // Store strong reference to VideoProducer to prevent GC
                    State.VideoProducer.Reset(VideoProducer);
                    UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: Stored strong references to RenderTarget, SceneCapture, and VideoProducer for PlayerIndex %d"), PlayerIndex);
                }
            }
            else
            {
                UE_LOG(LogStreamingHelpers, Warning, TEXT("SetupPlayerStreaming: Could not find VideoProducerRenderTarget class"));
            }
        }
    }

    // 3. Get or create PlayerController for this index
    APlayerController* Controller = nullptr;

    if (PlayerIndex == 0)
    {
        // Player 0 uses the existing primary controller
        Controller = UGameplayStatics::GetPlayerController(World, 0);
        UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: Using existing PlayerController 0"));
    }
    else
    {
        // Create a new LocalPlayer and PlayerController for this index
        UGameInstance* GameInstance = World->GetGameInstance();
        if (!GameInstance)
        {
            UE_LOG(LogStreamingHelpers, Error, TEXT("SetupPlayerStreaming: No GameInstance"));
            return false;
        }

        // Check if we already have a LocalPlayer for this index
        ULocalPlayer* LocalPlayer = GameInstance->GetLocalPlayerByIndex(PlayerIndex);

        if (!LocalPlayer)
        {
            FString Error;
            LocalPlayer = GameInstance->CreateLocalPlayer(PlayerIndex, Error, true);

            if (!LocalPlayer)
            {
                UE_LOG(LogStreamingHelpers, Error, TEXT("SetupPlayerStreaming: Failed to create LocalPlayer %d: %s"), PlayerIndex, *Error);
                return false;
            }
            UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: Created LocalPlayer %d"), PlayerIndex);
        }

        Controller = LocalPlayer->GetPlayerController(World);

        if (Controller)
        {
            // Disable split screen - we don't want multiple viewports on screen
            UGameViewportClient* ViewportClient = World->GetGameViewport();
            if (ViewportClient)
            {
                ViewportClient->SetForceDisableSplitscreen(true);
            }
            UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: Got PlayerController %d: %s"), PlayerIndex, *Controller->GetName());
        }
    }

    if (!Controller)
    {
        UE_LOG(LogStreamingHelpers, Error, TEXT("SetupPlayerStreaming: Failed to get PlayerController for index %d"), PlayerIndex);
        return false;
    }

    OutPlayerController = Controller;

    // 4. Possess the pawn with this controller
    if (Controller->GetPawn() != Pawn)
    {
        Controller->UnPossess();
        Controller->Possess(Pawn);
        UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: PlayerController %d now possessing %s"), PlayerIndex, *Pawn->GetName());
    }

    // 5. Start streaming on the component (if found)
    if (StreamerComp)
    {
        // Check if already streaming
        UFunction* IsStreamingFunc = StreamerCompClass->FindFunctionByName(TEXT("IsStreaming"));
        bool bIsAlreadyStreaming = false;
        if (IsStreamingFunc)
        {
            StreamerComp->ProcessEvent(IsStreamingFunc, &bIsAlreadyStreaming);
        }

        if (bIsAlreadyStreaming)
        {
            UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: Component already streaming, skipping StartStreaming"));
        }
        else
        {
            // Call StartStreaming() via reflection
            UFunction* StartStreamingFunc = StreamerCompClass->FindFunctionByName(TEXT("StartStreaming"));
            if (StartStreamingFunc)
            {
                UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: About to call StartStreaming() - StreamerId should be 'Streamer%d'"), PlayerIndex);
                StreamerComp->ProcessEvent(StartStreamingFunc, nullptr);
                UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: Called StartStreaming() on component"));

                // Try to get the actual streamer ID that was used
                UFunction* GetIdFunc = StreamerCompClass->FindFunctionByName(TEXT("GetId"));
                if (GetIdFunc)
                {
                    FString ActualStreamerId;
                    StreamerComp->ProcessEvent(GetIdFunc, &ActualStreamerId);
                    UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: Component's GetId() returned '%s'"), *ActualStreamerId);
                }

                // For PlayerIndex > 0, enable input on this controller
                if (PlayerIndex > 0 && Controller)
                {
                    Controller->SetIgnoreMoveInput(false);
                    Controller->SetIgnoreLookInput(false);
                    UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: Enabled input on PlayerController %d"), PlayerIndex);
                }
            }
        }
    }

    // Mark this pawn as set up
    SetupPawns.Add(WeakPawn);

    // Store the streaming state for input routing
    FString StreamerId = FString::Printf(TEXT("Streamer%d"), PlayerIndex);
    FPlayerStreamingState& State = StreamingStates.FindOrAdd(StreamerId);
    State.PlayerIndex = PlayerIndex;
    State.Pawn = Pawn;
    State.Controller = Controller;
    State.StreamerId = StreamerId;
    // SceneCapture and RenderTarget are set earlier for PlayerIndex > 0

    UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: Stored streaming state for %s -> PlayerController %d"), *StreamerId, PlayerIndex);
    UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreaming: SUCCESS - PlayerIndex %d, Port %d"), PlayerIndex, GetPortForPlayer(PlayerIndex));
    return true;
}

int32 UStreamingHelpers::GetPortForPlayer(int32 PlayerIndex)
{
    if (PlayerIndex == 0)
    {
        return 8080;
    }
    // Skip 8081 (blocked by IIS on some systems)
    return 8082 + (PlayerIndex - 1);
}

int32 UStreamingHelpers::ParsePlayerIndexFromTag(const FString& Tag)
{
    // Extract number from end of tag like "Metahuman1" -> 1
    // Find where the trailing digits start
    int32 DigitStart = Tag.Len();
    for (int32 i = Tag.Len() - 1; i >= 0; i--)
    {
        if (FChar::IsDigit(Tag[i]))
        {
            DigitStart = i;
        }
        else
        {
            break;
        }
    }

    if (DigitStart >= Tag.Len())
    {
        return 0; // No digits found
    }

    FString NumberStr = Tag.Mid(DigitStart);
    return FCString::Atoi(*NumberStr);
}

bool UStreamingHelpers::AddInputMappingContextToPlayer(APlayerController* Controller, UInputMappingContext* MappingContext, int32 Priority)
{
    if (!Controller)
    {
        UE_LOG(LogStreamingHelpers, Error, TEXT("AddInputMappingContextToPlayer: Controller is null"));
        return false;
    }

    if (!MappingContext)
    {
        UE_LOG(LogStreamingHelpers, Error, TEXT("AddInputMappingContextToPlayer: MappingContext is null"));
        return false;
    }

    // Get the LocalPlayer from the controller
    ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
    if (!LocalPlayer)
    {
        UE_LOG(LogStreamingHelpers, Error, TEXT("AddInputMappingContextToPlayer: Controller has no LocalPlayer"));
        return false;
    }

    UE_LOG(LogStreamingHelpers, Log, TEXT("AddInputMappingContextToPlayer: Found LocalPlayer for controller %s"), *Controller->GetName());

    // Get the Enhanced Input Local Player Subsystem
    UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!Subsystem)
    {
        UE_LOG(LogStreamingHelpers, Error, TEXT("AddInputMappingContextToPlayer: Could not get EnhancedInputLocalPlayerSubsystem for LocalPlayer"));
        return false;
    }

    UE_LOG(LogStreamingHelpers, Log, TEXT("AddInputMappingContextToPlayer: Got EnhancedInputLocalPlayerSubsystem"));

    // Check if already added
    if (Subsystem->HasMappingContext(MappingContext))
    {
        UE_LOG(LogStreamingHelpers, Log, TEXT("AddInputMappingContextToPlayer: MappingContext '%s' already added"), *MappingContext->GetName());
        return true;
    }

    // Add the mapping context
    Subsystem->AddMappingContext(MappingContext, Priority);
    UE_LOG(LogStreamingHelpers, Log, TEXT("AddInputMappingContextToPlayer: Added MappingContext '%s' with priority %d"), *MappingContext->GetName(), Priority);

    return true;
}

bool UStreamingHelpers::SetupPlayerStreamingAuto(APawn* Pawn, APlayerController*& OutPlayerController, int32& OutPlayerIndex)
{
    OutPlayerController = nullptr;
    OutPlayerIndex = 0;

    if (!Pawn)
    {
        UE_LOG(LogStreamingHelpers, Error, TEXT("SetupPlayerStreamingAuto: Pawn is null"));
        return false;
    }

    // Look for Metahuman tags to determine PlayerIndex
    for (const FName& Tag : Pawn->Tags)
    {
        FString TagStr = Tag.ToString();
        if (TagStr.StartsWith(TEXT("Metahuman")))
        {
            OutPlayerIndex = ParsePlayerIndexFromTag(TagStr);
            UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreamingAuto: Found tag '%s', PlayerIndex = %d"), *TagStr, OutPlayerIndex);
            break;
        }
    }

    // Also check components for tags (in case tag is on a component)
    if (OutPlayerIndex == 0)
    {
        TArray<UActorComponent*> Components;
        Pawn->GetComponents(Components);
        for (UActorComponent* Comp : Components)
        {
            for (const FName& Tag : Comp->ComponentTags)
            {
                FString TagStr = Tag.ToString();
                if (TagStr.StartsWith(TEXT("Metahuman")))
                {
                    OutPlayerIndex = ParsePlayerIndexFromTag(TagStr);
                    UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreamingAuto: Found component tag '%s', PlayerIndex = %d"), *TagStr, OutPlayerIndex);
                    break;
                }
            }
            if (OutPlayerIndex != 0) break;
        }
    }

    // Check the pawn's name as a fallback (spawned pawns might have names like "BP_Lucy_Metahuman1_C_0")
    if (OutPlayerIndex == 0)
    {
        FString PawnName = Pawn->GetName();
        int32 MetahumanIdx = PawnName.Find(TEXT("Metahuman"));
        if (MetahumanIdx != INDEX_NONE)
        {
            FString Suffix = PawnName.Mid(MetahumanIdx);
            OutPlayerIndex = ParsePlayerIndexFromTag(Suffix);
            UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreamingAuto: Parsed from pawn name '%s', PlayerIndex = %d"), *PawnName, OutPlayerIndex);
        }
    }

    UE_LOG(LogStreamingHelpers, Log, TEXT("SetupPlayerStreamingAuto: Using PlayerIndex %d for pawn %s"), OutPlayerIndex, *Pawn->GetName());

    FString StreamerId;
    return SetupPlayerStreaming(Pawn, OutPlayerIndex, OutPlayerController, StreamerId);
}

bool UStreamingHelpers::InjectRawMouseInputForPlayer(int32 PlayerIndex, float DeltaX, float DeltaY)
{
    // Find the controller for this player index
    FString StreamerId = FString::Printf(TEXT("Streamer%d"), PlayerIndex);
    APlayerController* Controller = GetControllerForStreamer(StreamerId);

    if (!Controller)
    {
        UE_LOG(LogStreamingHelpers, Warning, TEXT("InjectRawMouseInputForPlayer: No controller found for PlayerIndex %d"), PlayerIndex);
        return false;
    }

    // Inject mouse axis input using FInputKeyEventArgs - flows through Enhanced Input system
    if (DeltaX != 0.0f)
    {
        FInputKeyEventArgs MouseXArgs = FInputKeyEventArgs::CreateSimulated(EKeys::MouseX, IE_Axis, DeltaX);
        Controller->InputKey(MouseXArgs);
    }

    if (DeltaY != 0.0f)
    {
        FInputKeyEventArgs MouseYArgs = FInputKeyEventArgs::CreateSimulated(EKeys::MouseY, IE_Axis, DeltaY);
        Controller->InputKey(MouseYArgs);
    }

    return true;
}

bool UStreamingHelpers::InjectRawKeyInputForPlayer(int32 PlayerIndex, FKey Key, bool bPressed)
{
    // Find the controller for this player index
    FString StreamerId = FString::Printf(TEXT("Streamer%d"), PlayerIndex);
    APlayerController* Controller = GetControllerForStreamer(StreamerId);

    if (!Controller)
    {
        UE_LOG(LogStreamingHelpers, Warning, TEXT("InjectRawKeyInputForPlayer: No controller found for PlayerIndex %d"), PlayerIndex);
        return false;
    }

    // Inject key input using FInputKeyEventArgs - flows through Enhanced Input system
    FInputKeyEventArgs KeyArgs = FInputKeyEventArgs::CreateSimulated(
        Key,
        bPressed ? IE_Pressed : IE_Released,
        bPressed ? 1.0f : 0.0f
    );
    Controller->InputKey(KeyArgs);

    return true;
}

bool UStreamingHelpers::RegisterInputHandlersForStreamer(const FString& StreamerId)
{
    UE_LOG(LogStreamingHelpers, Warning, TEXT("RegisterInputHandlersForStreamer: Called for '%s'"), *StreamerId);

    // Get the player index for this streamer
    int32 PlayerIndex = GetPlayerIndexForStreamer(StreamerId);

    // Skip PlayerIndex 0 - the default handlers work correctly for it via Enhanced Input
    // We only need custom handlers for PlayerIndex 1+ where we bypass FSlateApplication
    if (PlayerIndex == 0)
    {
        UE_LOG(LogStreamingHelpers, Log, TEXT("RegisterInputHandlersForStreamer: Skipping '%s' (PlayerIndex 0) - default handlers work correctly"), *StreamerId);
        return true;
    }

    if (PlayerIndex < 0)
    {
        UE_LOG(LogStreamingHelpers, Error, TEXT("RegisterInputHandlersForStreamer: No player index found for streamer '%s'"), *StreamerId);
        return false;
    }

    // Find the streamer by ID
    TSharedPtr<IPixelStreaming2Streamer> TargetStreamer;
    IPixelStreaming2Module::Get().ForEachStreamer([&StreamerId, &TargetStreamer](TSharedPtr<IPixelStreaming2Streamer> Streamer) {
        if (Streamer->GetId() == StreamerId)
        {
            TargetStreamer = Streamer;
        }
    });

    if (!TargetStreamer)
    {
        UE_LOG(LogStreamingHelpers, Error, TEXT("RegisterInputHandlersForStreamer: Streamer '%s' NOT FOUND in module!"), *StreamerId);
        return false;
    }

    UE_LOG(LogStreamingHelpers, Warning, TEXT("RegisterInputHandlersForStreamer: Found streamer '%s'"), *StreamerId);

    TSharedPtr<IPixelStreaming2InputHandler> InputHandler = TargetStreamer->GetInputHandler().Pin();
    if (!InputHandler)
    {
        UE_LOG(LogStreamingHelpers, Error, TEXT("RegisterInputHandlersForStreamer: No input handler for streamer '%s'"), *StreamerId);
        return false;
    }

    UE_LOG(LogStreamingHelpers, Warning, TEXT("RegisterInputHandlersForStreamer: Registering handlers for '%s' (PlayerIndex %d)"), *StreamerId, PlayerIndex);

    // Register custom MouseMove handler
    InputHandler->RegisterMessageHandler(TEXT("MouseMove"),
        [PlayerIndex, StreamerId](FString SourceId, FMemoryReader Ar) {
            // Get viewport dimensions for denormalization (same as engine does)
            UGameViewportClient* ViewportClient = GEngine->GameViewport;
            FVector2D ViewportSize(1920.0f, 1080.0f); // Default to common resolution
            if (ViewportClient)
            {
                ViewportClient->GetViewportSize(ViewportSize);
            }
            const float int16_MAX = 32767.0f;

            // Parse mouse move data: uint16 PosX, uint16 PosY, int16 DeltaX, int16 DeltaY
            uint16 PosX, PosY;
            int16 RawDeltaX, RawDeltaY;
            Ar << PosX << PosY << RawDeltaX << RawDeltaY;

            // Only apply look input when right mouse button is held (matching typical click-to-look behavior)
            bool bRightMouseHeld = PlayerRightMouseHeld.FindRef(PlayerIndex);
            if (bRightMouseHeld)
            {
                FString LocalStreamerId = FString::Printf(TEXT("Streamer%d"), PlayerIndex);
                APlayerController* Controller = GetControllerForStreamer(LocalStreamerId);
                if (Controller)
                {
                    // Scale raw int16 deltas to match Streamer0's Enhanced Input sensitivity
                    // Streamer0 min step: 0.14, Streamer1 raw min: 56
                    // 56 / 400 = 0.14 (matches Streamer0)
                    float DeltaX = static_cast<float>(RawDeltaX) / 400.0f;
                    float DeltaY = static_cast<float>(RawDeltaY) / 400.0f;

                    UE_LOG(LogStreamingHelpers, Log, TEXT("Streamer1 Look: Raw(%d,%d) Final(%.2f,%.2f)"),
                        RawDeltaX, RawDeltaY, DeltaX, DeltaY);

                    // Apply directly to pawn
                    if (APawn* Pawn = Controller->GetPawn())
                    {
                        Pawn->AddControllerYawInput(DeltaX);
                        Pawn->AddControllerPitchInput(DeltaY);
                    }
                }
            }
        }
    );

    // Register custom MouseDown handler
    InputHandler->RegisterMessageHandler(TEXT("MouseDown"),
        [PlayerIndex, StreamerId](FString SourceId, FMemoryReader Ar) {
            uint8 Button;
            uint16 PosX, PosY;
            Ar << Button << PosX << PosY;

            UE_LOG(LogStreamingHelpers, Warning, TEXT("MouseDown: Button=%d for %s"), Button, *StreamerId);

            FKey MouseKey;
            switch (Button)
            {
                case 0:
                    MouseKey = EKeys::LeftMouseButton;
                    PlayerLeftMouseHeld.Add(PlayerIndex, true);
                    break;
                case 1:
                    MouseKey = EKeys::MiddleMouseButton;
                    break;
                case 2:
                    MouseKey = EKeys::RightMouseButton;
                    PlayerRightMouseHeld.Add(PlayerIndex, true);
                    break;
                default: return;
            }

            InjectRawKeyInputForPlayer(PlayerIndex, MouseKey, true);
        }
    );

    // Register custom MouseUp handler
    InputHandler->RegisterMessageHandler(TEXT("MouseUp"),
        [PlayerIndex, StreamerId](FString SourceId, FMemoryReader Ar) {
            uint8 Button;
            uint16 PosX, PosY;
            Ar << Button << PosX << PosY;

            UE_LOG(LogStreamingHelpers, Warning, TEXT("MouseUp: Button=%d for %s"), Button, *StreamerId);

            FKey MouseKey;
            switch (Button)
            {
                case 0:
                    MouseKey = EKeys::LeftMouseButton;
                    PlayerLeftMouseHeld.Add(PlayerIndex, false);
                    break;
                case 1:
                    MouseKey = EKeys::MiddleMouseButton;
                    break;
                case 2:
                    MouseKey = EKeys::RightMouseButton;
                    PlayerRightMouseHeld.Add(PlayerIndex, false);
                    break;
                default: return;
            }

            InjectRawKeyInputForPlayer(PlayerIndex, MouseKey, false);
        }
    );

    // Register custom MouseWheel handler
    InputHandler->RegisterMessageHandler(TEXT("MouseWheel"),
        [PlayerIndex, StreamerId](FString SourceId, FMemoryReader Ar) {
            UE_LOG(LogStreamingHelpers, Warning, TEXT("MouseWheel handler called for %s"), *StreamerId);

            int16 WheelDelta;
            uint16 PosX, PosY;
            Ar << WheelDelta << PosX << PosY;

            UE_LOG(LogStreamingHelpers, Warning, TEXT("MouseWheel: Delta=%d"), WheelDelta);

            // Get the controller for this player
            FString LocalStreamerId = FString::Printf(TEXT("Streamer%d"), PlayerIndex);
            APlayerController* Controller = GetControllerForStreamer(LocalStreamerId);
            if (Controller)
            {
                // Convert wheel delta to float (same scaling as default handler: 1/120)
                const float SpinFactor = 1.0f / 120.0f;
                float WheelValue = static_cast<float>(WheelDelta) * SpinFactor;

                // Inject as MouseWheelAxis using FInputKeyEventArgs
                FInputKeyEventArgs WheelArgs = FInputKeyEventArgs::CreateSimulated(EKeys::MouseWheelAxis, IE_Axis, WheelValue);
                Controller->InputKey(WheelArgs);
            }
        }
    );

    // Register custom KeyDown handler
    InputHandler->RegisterMessageHandler(TEXT("KeyDown"),
        [PlayerIndex, StreamerId](FString SourceId, FMemoryReader Ar) {
            uint8 KeyCode;
            uint8 IsRepeat;
            Ar << KeyCode << IsRepeat;

            UE_LOG(LogStreamingHelpers, Warning, TEXT("KeyDown handler called for %s: KeyCode=%d IsRepeat=%d"), *StreamerId, KeyCode, IsRepeat);

            FKey Key = FInputKeyManager::Get().GetKeyFromCodes(KeyCode, KeyCode);
            if (Key.IsValid() && IsRepeat == 0)
            {
                InjectRawKeyInputForPlayer(PlayerIndex, Key, true);
            }
        }
    );

    // Register custom KeyUp handler
    InputHandler->RegisterMessageHandler(TEXT("KeyUp"),
        [PlayerIndex, StreamerId](FString SourceId, FMemoryReader Ar) {
            uint8 KeyCode;
            Ar << KeyCode;

            UE_LOG(LogStreamingHelpers, Warning, TEXT("KeyUp handler called for %s: KeyCode=%d"), *StreamerId, KeyCode);

            FKey Key = FInputKeyManager::Get().GetKeyFromCodes(KeyCode, KeyCode);
            if (Key.IsValid())
            {
                InjectRawKeyInputForPlayer(PlayerIndex, Key, false);
            }
        }
    );

    UE_LOG(LogStreamingHelpers, Warning, TEXT("RegisterInputHandlersForStreamer: SUCCESS - All handlers registered for '%s'"), *StreamerId);

    return true;
}

void UStreamingHelpers::SetLookSensitivity(float Sensitivity)
{
    LookSensitivityMultiplier = FMath::Max(0.001f, Sensitivity);
    UE_LOG(LogStreamingHelpers, Log, TEXT("SetLookSensitivity: Set to %.4f"), LookSensitivityMultiplier);
}

float UStreamingHelpers::GetLookSensitivity()
{
    return LookSensitivityMultiplier;
}

void UStreamingHelpers::LogLookInput(const FString& StreamerName, float LookX, float LookY)
{
    UE_LOG(LogStreamingHelpers, Log, TEXT("%s Look: Final(%.4f,%.4f)"),
        *StreamerName, LookX, LookY);
}

// VideoProducers that have been disconnected but kept alive to prevent render thread crash
// These will be cleaned up only during engine shutdown
static TArray<TStrongObjectPtr<UObject>> OrphanedVideoProducers;
static TArray<TStrongObjectPtr<UTextureRenderTarget2D>> OrphanedRenderTargets;

// Internal helper to cleanup a single streamer
static void CleanupStreamerByIdInternal(const FString& StreamerId)
{
    UE_LOG(LogStreamingHelpers, Log, TEXT("CleanupStreamer: Cleaning up '%s'"), *StreamerId);

    FPlayerStreamingState* State = StreamingStates.Find(StreamerId);
    if (!State)
    {
        UE_LOG(LogStreamingHelpers, Warning, TEXT("CleanupStreamer: No streaming state found for '%s'"), *StreamerId);
        return;
    }

    // 1. Stop streaming on the component FIRST
    if (State->StreamerComponent.IsValid())
    {
        UActorComponent* StreamerComp = State->StreamerComponent.Get();
        UClass* StreamerCompClass = GetStreamerComponentClass();

        if (StreamerCompClass)
        {
            // Call StopStreaming()
            UFunction* StopStreamingFunc = StreamerCompClass->FindFunctionByName(TEXT("StopStreaming"));
            if (StopStreamingFunc)
            {
                StreamerComp->ProcessEvent(StopStreamingFunc, nullptr);
                UE_LOG(LogStreamingHelpers, Log, TEXT("CleanupStreamer: Called StopStreaming() on component"));
            }

            // Clear the VideoProducer reference on the streamer component
            FProperty* VideoProducerProp = StreamerCompClass->FindPropertyByName(TEXT("VideoProducer"));
            if (VideoProducerProp)
            {
                UObject** VideoProducerPtr = VideoProducerProp->ContainerPtrToValuePtr<UObject*>(StreamerComp);
                if (VideoProducerPtr && *VideoProducerPtr)
                {
                    *VideoProducerPtr = nullptr;
                    UE_LOG(LogStreamingHelpers, Log, TEXT("CleanupStreamer: Cleared VideoProducer on StreamerComponent"));
                }
            }
        }
    }

    // 2. Deactivate SceneCaptureComponent (stop capturing frames)
    if (State->SceneCapture.IsValid())
    {
        USceneCaptureComponent2D* SceneCapture = State->SceneCapture.Get();
        SceneCapture->bCaptureEveryFrame = false;
        SceneCapture->bCaptureOnMovement = false;
        SceneCapture->Deactivate();
        SceneCapture->TextureTarget = nullptr;
        SceneCapture->DestroyComponent();
        UE_LOG(LogStreamingHelpers, Log, TEXT("CleanupStreamer: Deactivated and destroyed SceneCaptureComponent"));
    }
    State->SceneCapture.Reset();

    // 3. Disconnect VideoProducer from RenderTarget but KEEP IT ALIVE
    // The VideoProducer has a render thread delegate that will crash if we destroy it
    // Instead, we disconnect it and keep it orphaned until engine shutdown
    if (State->VideoProducer.IsValid())
    {
        UObject* VideoProducer = State->VideoProducer.Get();
        UClass* VideoProducerClass = GetVideoProducerRenderTargetClass();

        if (VideoProducerClass)
        {
            // Clear the Target to stop it from capturing
            FProperty* TargetProp = VideoProducerClass->FindPropertyByName(TEXT("Target"));
            if (TargetProp)
            {
                UTextureRenderTarget2D** TargetPtr = TargetProp->ContainerPtrToValuePtr<UTextureRenderTarget2D*>(VideoProducer);
                if (TargetPtr)
                {
                    *TargetPtr = nullptr;
                    UE_LOG(LogStreamingHelpers, Log, TEXT("CleanupStreamer: Cleared Target on VideoProducer (disconnected)"));
                }
            }
        }

        // CRITICAL: Move to orphaned list instead of releasing
        // This keeps the VideoProducer alive so its render thread delegate doesn't crash
        OrphanedVideoProducers.Add(MoveTemp(State->VideoProducer));
        UE_LOG(LogStreamingHelpers, Log, TEXT("CleanupStreamer: VideoProducer orphaned (kept alive to prevent crash)"));
    }
    State->VideoProducer.Reset();

    // 4. Orphan RenderTarget too (VideoProducer may still reference it briefly)
    if (State->RenderTarget.IsValid())
    {
        OrphanedRenderTargets.Add(MoveTemp(State->RenderTarget));
        UE_LOG(LogStreamingHelpers, Log, TEXT("CleanupStreamer: RenderTarget orphaned"));
    }
    State->RenderTarget.Reset();

    // 5. Clear pawn from setup tracking
    if (State->Pawn.IsValid())
    {
        SetupPawns.Remove(State->Pawn);
        UE_LOG(LogStreamingHelpers, Log, TEXT("CleanupStreamer: Removed pawn from setup tracking"));
    }

    // 6. Clear mouse button tracking for this player
    PlayerRightMouseHeld.Remove(State->PlayerIndex);
    PlayerLeftMouseHeld.Remove(State->PlayerIndex);

    // 7. Remove from streaming states map
    StreamingStates.Remove(StreamerId);

    UE_LOG(LogStreamingHelpers, Log, TEXT("CleanupStreamer: Cleanup complete for '%s'"), *StreamerId);
}

void UStreamingHelpers::CleanupSinglePlayerStreaming(int32 PlayerIndex)
{
    FString StreamerId = FString::Printf(TEXT("Streamer%d"), PlayerIndex);
    UE_LOG(LogStreamingHelpers, Log, TEXT("CleanupSinglePlayerStreaming: Cleaning up PlayerIndex %d (%s)"), PlayerIndex, *StreamerId);

    CleanupStreamerByIdInternal(StreamerId);

    UE_LOG(LogStreamingHelpers, Log, TEXT("CleanupSinglePlayerStreaming: Done. %d VideoProducers and %d RenderTargets orphaned."),
        OrphanedVideoProducers.Num(), OrphanedRenderTargets.Num());
}

void UStreamingHelpers::CleanupAllPlayerStreaming()
{
    UE_LOG(LogStreamingHelpers, Log, TEXT("CleanupAllPlayerStreaming: Full shutdown - cleaning all states and releasing orphaned objects"));

    // Get all streamer IDs first (can't modify map while iterating)
    TArray<FString> StreamerIds;
    StreamingStates.GetKeys(StreamerIds);

    for (const FString& StreamerId : StreamerIds)
    {
        CleanupStreamerByIdInternal(StreamerId);
    }

    // Clear all tracking data
    SetupPawns.Empty();
    StreamingStates.Empty();
    PlayerRightMouseHeld.Empty();
    PlayerLeftMouseHeld.Empty();

    // During PIE shutdown, we MUST release orphaned objects or the World can't be GC'd
    // The render thread will be stopping soon anyway, so the risk of crash is lower
    // Flush render commands first to minimize the window for race conditions
    FlushRenderingCommands();

    // Release orphaned VideoProducers
    int32 NumVideoProducers = OrphanedVideoProducers.Num();
    for (auto& VideoProducer : OrphanedVideoProducers)
    {
        VideoProducer.Reset();
    }
    OrphanedVideoProducers.Empty();

    // Flush again after releasing VideoProducers
    FlushRenderingCommands();

    // Release orphaned RenderTargets
    int32 NumRenderTargets = OrphanedRenderTargets.Num();
    for (auto& RenderTarget : OrphanedRenderTargets)
    {
        if (RenderTarget.IsValid())
        {
            RenderTarget->ReleaseResource();
        }
        RenderTarget.Reset();
    }
    OrphanedRenderTargets.Empty();

    // Final flush
    FlushRenderingCommands();

    UE_LOG(LogStreamingHelpers, Log, TEXT("CleanupAllPlayerStreaming: Released %d VideoProducers and %d RenderTargets. Shutdown complete."),
        NumVideoProducers, NumRenderTargets);
}

void UStreamingHelpers::SetStreamingResolution(int32 Width, int32 Height)
{
    StreamingResolutionWidth = FMath::Clamp(Width, 640, 3840);
    StreamingResolutionHeight = FMath::Clamp(Height, 480, 2160);
    UE_LOG(LogStreamingHelpers, Log, TEXT("SetStreamingResolution: Set to %dx%d"), StreamingResolutionWidth, StreamingResolutionHeight);
}

void UStreamingHelpers::SetStreamingQualityMode(bool bHighQuality)
{
    bHighQualityStreaming = bHighQuality;
    UE_LOG(LogStreamingHelpers, Log, TEXT("SetStreamingQualityMode: %s"), bHighQuality ? TEXT("High Quality") : TEXT("Performance"));
}
