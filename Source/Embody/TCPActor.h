#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Delegates/DelegateCombinations.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Async/Async.h"
#include "TCPActor.generated.h"

class IPixelStreaming2InputHandler;
class FServerRunnable;

// Blueprint event: fires when a string is received from Pixel Streaming frontend or TCP
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStringReceived, const FString&, ReceivedString);

UCLASS()
class EMBODY_API ATCPActor : public AActor
{
	GENERATED_BODY()

public:
	ATCPActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Registers to receive Pixel Streaming data channel messages
	void RegisterPixelStreamingHandler();

	// Starts the TCP server on port 7777
	void StartTcpServer();

	// Processes the string received from WebRTC or TCP
	void HandleReceivedString(const FString& ReceivedString);

public:
	// Exposed to Blueprints: fired when a command string arrives
	UPROPERTY(BlueprintAssignable, Category = "TCP")
	FOnStringReceived OnStringReceived;

	// TCP server port (default 7777)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TCP")
	int32 TCPPort = 7777;

private:
	// WebRTC: Input handler for receiving Pixel Streaming commands
	TSharedPtr<IPixelStreaming2InputHandler> InputHandler;

	// TCP: Server socket and thread management
	FSocket* ServerSocket;
	FServerRunnable* ServerRunnable;
	FRunnableThread* ServerThread;
};


// Standalone Runnable class for TCP server
class FServerRunnable : public FRunnable
{
public:
	FServerRunnable(FSocket* InServerSocket)
		: ServerSocket(InServerSocket), bShouldRun(true) {
	}

	virtual ~FServerRunnable() override = default;

	virtual bool Init() override { return true; }
	virtual uint32 Run() override;
	virtual void Stop() override { bShouldRun = false; }

	bool IsRunning() const { return bShouldRun; }
	void SetShouldRun(bool bRun) { bShouldRun = bRun; }

	DECLARE_EVENT_OneParam(FServerRunnable, FOnStringReceivedEvent, const FString&)
	FOnStringReceivedEvent& OnStringReceived() { return StringReceivedEvent; }

private:
	void ListenForConnections();

	FSocket* ServerSocket;
	bool bShouldRun;
	FOnStringReceivedEvent StringReceivedEvent;
};