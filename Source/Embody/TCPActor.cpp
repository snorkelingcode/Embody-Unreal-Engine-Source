#include "TCPActor.h"
#include "ProceduralAnimComponent.h"
#include "Modules/ModuleManager.h"
#include "IPixelStreaming2Module.h"
#include "IPixelStreaming2Streamer.h"
#include "IPixelStreaming2InputHandler.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"

ATCPActor::ATCPActor()
{
	PrimaryActorTick.bCanEverTick = false;
	ServerSocket = nullptr;
	ServerRunnable = nullptr;
	ServerThread = nullptr;
}

void ATCPActor::BeginPlay()
{
    Super::BeginPlay();

    StartTcpServer();

    FTimerHandle DelayHandle;
    GetWorld()->GetTimerManager().SetTimer(DelayHandle, [this]()
    {
        IPixelStreaming2Module& PS2 = FModuleManager::LoadModuleChecked<IPixelStreaming2Module>("PixelStreaming2");
        if (PS2.GetStreamerIds().Num() > 0)
        {
            RegisterPixelStreamingHandler();
        }
        else
        {
            GetWorld()->GetTimerManager().SetTimerForNextTick([this]() { BeginPlay(); });
        }
    }, 2.0f, false);
}


void ATCPActor::StartTcpServer()
{
	if (ServerSocket != nullptr)
	{
		return;
	}

	ServerSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("TcpListener"), false);
	TSharedRef<FInternetAddr> ListenAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();

	bool bIsValid;
	ListenAddress->SetIp(TEXT("127.0.0.1"), bIsValid);
	ListenAddress->SetPort(TCPPort);

	if (!bIsValid || !ServerSocket->Bind(*ListenAddress) || !ServerSocket->Listen(1))
	{
		return;
	}

	ServerRunnable = new FServerRunnable(ServerSocket);
	ServerRunnable->OnStringReceived().AddUObject(this, &ATCPActor::HandleReceivedString);
	ServerThread = FRunnableThread::Create(ServerRunnable, TEXT("TcpServerThread"));
}

void ATCPActor::RegisterPixelStreamingHandler()
{
	if (!FModuleManager::Get().IsModuleLoaded("PixelStreaming2"))
	{
		FTimerHandle RetryHandle;
		GetWorld()->GetTimerManager().SetTimer(RetryHandle, [this]()
		{
			RegisterPixelStreamingHandler();
		}, 1.0f, false);
		return;
	}

	IPixelStreaming2Module& PS2Module = IPixelStreaming2Module::Get();
	TArray<FString> StreamerIds = PS2Module.GetStreamerIds();

	if (StreamerIds.Num() == 0)
	{
		FTimerHandle RetryHandle;
		GetWorld()->GetTimerManager().SetTimer(RetryHandle, [this]()
		{
			RegisterPixelStreamingHandler();
		}, 1.0f, false);
		return;
	}

	FString StreamerId = StreamerIds[0];
	TSharedPtr<IPixelStreaming2Streamer> Streamer = PS2Module.FindStreamer(StreamerId);

	if (!Streamer.IsValid())
	{
		return;
	}

	InputHandler = Streamer->GetInputHandler().Pin();

	if (InputHandler.IsValid())
	{
		InputHandler->SetCommandHandler(TEXT("command"),
			[this](FString SourceId, FString Descriptor, FString CommandString)
			{
				AsyncTask(ENamedThreads::GameThread, [this, CommandString]()
					{
						HandleReceivedString(CommandString);
					});
			}
		);
	}
}

void ATCPActor::HandleReceivedString(const FString& ReceivedString)
{
	// Auto-process MAN_ commands in C++ before broadcasting
	if (ReceivedString.StartsWith(TEXT("MAN_")))
	{
		ProcessMannequinCommand(ReceivedString);
	}

	OnStringReceived.Broadcast(ReceivedString);
}

bool ATCPActor::ProcessMannequinCommand(const FString& Command)
{
	if (!Command.StartsWith(TEXT("MAN_")))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MAN] No world available"));
		return false;
	}

	// Handle MAN_RESET — clear all bone overrides on all characters
	if (Command.Equals(TEXT("MAN_RESET"), ESearchCase::IgnoreCase))
	{
		TArray<AActor*> AllActors;
		UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
		for (AActor* Actor : AllActors)
		{
			if (!Actor) continue;
			UProceduralAnimComponent* AnimComp = Actor->FindComponentByClass<UProceduralAnimComponent>();
			if (AnimComp)
			{
				AnimComp->ClearAllBoneRotations();
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[MAN] Reset all bone overrides"));
		return true;
	}

	bool bApplied = false;

	// Strategy 1: Find actors with the Metahuman tag that have the component
	TArray<AActor*> TaggedActors;
	UGameplayStatics::GetAllActorsWithTag(World, FName(TEXT("Metahuman")), TaggedActors);

	for (AActor* Actor : TaggedActors)
	{
		if (!Actor) continue;

		UProceduralAnimComponent* AnimComp = Actor->FindComponentByClass<UProceduralAnimComponent>();
		if (!AnimComp)
		{
			static TSet<FString> LoggedMissing;
			if (!LoggedMissing.Contains(Actor->GetName()))
			{
				UE_LOG(LogTemp, Warning, TEXT("[MAN] Actor '%s' has Metahuman tag but no ProceduralAnimComponent"), *Actor->GetName());
				LoggedMissing.Add(Actor->GetName());
			}
			continue;
		}

		if (AnimComp->ApplyMannequinCommand(Command))
		{
			bApplied = true;
			static bool bLoggedTagged = false;
			if (!bLoggedTagged)
			{
				UE_LOG(LogTemp, Log, TEXT("[MAN] TCPActor writing to ProceduralAnimComponent %p on '%s'. Bones now: %d"), AnimComp, *Actor->GetName(), AnimComp->GetAllBoneRotations().Num());
				bLoggedTagged = true;
			}
		}
	}

	// Strategy 2: If no tagged actors had the component, find ALL actors with the component
	if (!bApplied)
	{
		TArray<AActor*> AllActors;
		UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

		for (AActor* Actor : AllActors)
		{
			if (!Actor) continue;

			UProceduralAnimComponent* AnimComp = Actor->FindComponentByClass<UProceduralAnimComponent>();
			if (AnimComp)
			{
				if (AnimComp->ApplyMannequinCommand(Command))
				{
					bApplied = true;
					// Log once per actor we find this way
					static TSet<FString> LoggedFallbacks;
					if (!LoggedFallbacks.Contains(Actor->GetName()))
					{
						UE_LOG(LogTemp, Log, TEXT("[MAN] Found ProceduralAnimComponent on '%s' (no Metahuman tag, using fallback search)"), *Actor->GetName());
						LoggedFallbacks.Add(Actor->GetName());
					}
				}
			}
		}
	}

	if (!bApplied)
	{
		static bool bLoggedOnce = false;
		if (!bLoggedOnce)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MAN] No actors with ProceduralAnimComponent found. Add the component to your character blueprint."));
			bLoggedOnce = true;
		}
	}

	return bApplied;
}

void ATCPActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Stop TCP listener
	if (ServerRunnable)
	{
		ServerRunnable->SetShouldRun(false);
	}

	if (ServerThread)
	{
		ServerThread->WaitForCompletion();
		delete ServerThread;
		ServerThread = nullptr;
	}

	if (ServerRunnable)
	{
		delete ServerRunnable;
		ServerRunnable = nullptr;
	}

	if (ServerSocket)
	{
		ServerSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ServerSocket);
		ServerSocket = nullptr;
	}

	// Stop WebRTC listener
	if (InputHandler.IsValid())
	{
		InputHandler.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

// FServerRunnable implementation
uint32 FServerRunnable::Run()
{
	ListenForConnections();
	return 0;
}

void FServerRunnable::ListenForConnections()
{
	while (bShouldRun)
	{
		bool bHasPendingConnection = false;

		if (ServerSocket->WaitForPendingConnection(bHasPendingConnection, FTimespan::FromSeconds(1.0)))
		{
			if (bHasPendingConnection)
			{
				TSharedRef<FInternetAddr> ClientAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
				FSocket* ClientSocket = ServerSocket->Accept(*ClientAddress, TEXT("Accepted Connection"));

				if (ClientSocket)
				{
					TArray<uint8> DataBuffer;
					uint8 TempBuffer[1024];
					int32 BytesRead;

					while (ClientSocket->Recv(TempBuffer, sizeof(TempBuffer), BytesRead))
					{
						DataBuffer.Append(TempBuffer, BytesRead);
					}

					if (DataBuffer.Num() > 0)
					{
						DataBuffer.Add(0);
					}

					FString ReceivedString = FString(UTF8_TO_TCHAR(DataBuffer.GetData())).TrimStartAndEnd();

					AsyncTask(ENamedThreads::GameThread, [this, ReceivedString]()
						{
							StringReceivedEvent.Broadcast(ReceivedString);
						});

					ClientSocket->Close();
					ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
				}
			}
		}
	}
}
