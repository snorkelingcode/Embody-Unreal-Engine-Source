#include "TCPActor.h"
#include "Modules/ModuleManager.h"
#include "IPixelStreaming2Module.h"
#include "IPixelStreaming2Streamer.h"
#include "IPixelStreaming2InputHandler.h"
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
	OnStringReceived.Broadcast(ReceivedString);
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
