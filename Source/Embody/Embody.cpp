// Copyright Epic Games, Inc. All Rights Reserved.

#include "Embody.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FEmbodyModule, Embody, "Embody");

void FEmbodyModule::StartupModule()
{
#if PLATFORM_WINDOWS
	// Load whisper.cpp DLLs from ThirdParty directory.
	// Must load ggml dependencies first (order matters).
	FString WhisperDir = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("Source/ThirdParty/Lib/Win64/whisper/x64"));

	FPlatformProcess::PushDllDirectory(*WhisperDir);

	const TCHAR* Dlls[] = {
		TEXT("ggml-base.dll"),
		TEXT("ggml-cpu.dll"),
		TEXT("ggml-cuda.dll"),
		TEXT("ggml.dll"),
		TEXT("whisper.dll"),
	};

	for (const TCHAR* Dll : Dlls)
	{
		void* Handle = FPlatformProcess::GetDllHandle(
			*FPaths::Combine(WhisperDir, Dll));
		if (Handle)
		{
			DllHandles.Add(Handle);
			UE_LOG(LogTemp, Log, TEXT("Loaded %s"), Dll);
		}
		else
		{
			// ggml-cuda.dll is optional — falls back to CPU if no NVIDIA GPU
			if (FCString::Strcmp(Dll, TEXT("ggml-cuda.dll")) == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("Could not load %s — whisper will use CPU only"), Dll);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to load %s from %s"), Dll, *WhisperDir);
			}
		}
	}

	FPlatformProcess::PopDllDirectory(*WhisperDir);
#endif
}

void FEmbodyModule::ShutdownModule()
{
	for (void* Handle : DllHandles)
	{
		if (Handle)
		{
			FPlatformProcess::FreeDllHandle(Handle);
		}
	}
	DllHandles.Empty();
}
