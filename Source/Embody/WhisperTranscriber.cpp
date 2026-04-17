// WhisperTranscriber.cpp

#include "WhisperTranscriber.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

THIRD_PARTY_INCLUDES_START
#include "whisper.h"
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogWhisperTranscriber, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
//  Static shared context
// ─────────────────────────────────────────────────────────────────────────────

whisper_context* FWhisperTranscriber::SharedContext = nullptr;
int32 FWhisperTranscriber::SharedContextRefCount = 0;
FCriticalSection FWhisperTranscriber::SharedContextLock;

whisper_context* FWhisperTranscriber::AcquireContext()
{
	FScopeLock Lock(&SharedContextLock);

	if (!SharedContext)
	{
		FString ModelPath = FPaths::Combine(
			FPaths::ProjectContentDir(), TEXT("Models"), TEXT("ggml-tiny.en.bin"));

		UE_LOG(LogWhisperTranscriber, Log,
			TEXT("Loading whisper model: %s"), *ModelPath);

		struct whisper_context_params Params = whisper_context_default_params();
		UE_LOG(LogWhisperTranscriber, Log,
			TEXT("Whisper context params: use_gpu=%s, gpu_device=%d, flash_attn=%s"),
			Params.use_gpu ? TEXT("true") : TEXT("false"),
			Params.gpu_device,
			Params.flash_attn ? TEXT("true") : TEXT("false"));

		SharedContext = whisper_init_from_file_with_params(
			TCHAR_TO_UTF8(*ModelPath), Params);

		if (!SharedContext)
		{
			UE_LOG(LogWhisperTranscriber, Error,
				TEXT("Failed to load whisper model from: %s"), *ModelPath);
			return nullptr;
		}

		UE_LOG(LogWhisperTranscriber, Log, TEXT("Whisper model loaded successfully (GPU-accelerated if CUDA available)."));
	}

	SharedContextRefCount++;
	return SharedContext;
}

void FWhisperTranscriber::ReleaseContext()
{
	FScopeLock Lock(&SharedContextLock);
	SharedContextRefCount = FMath::Max(0, SharedContextRefCount - 1);
	// Keep model cached in memory — reloading takes ~300ms and causes audio stutter
	UE_LOG(LogWhisperTranscriber, Log,
		TEXT("Whisper context released (refcount=%d, model stays cached)."),
		SharedContextRefCount);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Construction / Destruction
// ─────────────────────────────────────────────────────────────────────────────

FWhisperTranscriber::FWhisperTranscriber(int32 InSampleRate, int32 InNumChannels, const FString& InInitialPrompt)
	: InputSampleRate(InSampleRate)
	, InputNumChannels(FMath::Max(InNumChannels, 1))
	, InitialPrompt(InInitialPrompt)
{
	// Convert initial prompt to UTF8 and keep it alive
	if (!InitialPrompt.IsEmpty())
	{
		auto Converted = StringCast<UTF8CHAR>(*InitialPrompt);
		int32 Len = Converted.Length();
		InitialPromptUTF8.SetNumUninitialized(Len + 1);
		FMemory::Memcpy(InitialPromptUTF8.GetData(), Converted.Get(), Len);
		InitialPromptUTF8[Len] = '\0';
	}
}

FWhisperTranscriber::~FWhisperTranscriber()
{
	Stop();

	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}

	if (Ctx)
	{
		ReleaseContext();
		Ctx = nullptr;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  FRunnable interface
// ─────────────────────────────────────────────────────────────────────────────

bool FWhisperTranscriber::Init()
{
	Ctx = AcquireContext();
	return Ctx != nullptr;
}

void FWhisperTranscriber::Stop()
{
	bShouldRun.Store(false);
}

void FWhisperTranscriber::StartThread()
{
	Thread = FRunnableThread::Create(
		this, TEXT("WhisperTranscriberThread"), 0, TPri_BelowNormal);
}

uint32 FWhisperTranscriber::Run()
{
	if (!Ctx)
	{
		UE_LOG(LogWhisperTranscriber, Error, TEXT("No whisper context — aborting."));
		return 1;
	}

	const int32 TargetSampleRate = WHISPER_SAMPLE_RATE; // 16000
	const int32 MinBufferSamples = FMath::FloorToInt32(MinBufferSeconds * TargetSampleRate);

	UE_LOG(LogWhisperTranscriber, Log,
		TEXT("Whisper worker started. ProcessInterval=%.2fs, MinBuffer=%.2fs, InitialPrompt=\"%s\""),
		ProcessIntervalSeconds, MinBufferSeconds,
		InitialPrompt.IsEmpty() ? TEXT("(none)") : *InitialPrompt);

	while (bShouldRun.Load())
	{
		// 1. Drain input queue into AudioBuffer
		{
			TArray<float> Chunk;
			while (PendingChunks.Dequeue(Chunk))
			{
				ResampleAndAppend(Chunk);
			}
		}

		const int32 TotalSamples = AudioBuffer.Num();
		const bool bIsEndOfAudio = bEndOfAudio.Load();

		// 2. Check if we should run inference
		bool bShouldProcess = false;

		if (TotalSamples >= MinBufferSamples && TotalSamples > LastProcessedBufferSize)
		{
			bShouldProcess = true;
		}
		// End-of-audio: process whatever remains
		if (bIsEndOfAudio && TotalSamples > LastProcessedBufferSize && TotalSamples > TargetSampleRate / 10)
		{
			bShouldProcess = true;
		}

		if (bShouldProcess)
		{
			LastProcessedBufferSize = TotalSamples;

			// 3. Set up whisper parameters (optimized for low-latency streaming)
			whisper_full_params WParams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
			WParams.print_progress = false;
			WParams.print_special = false;
			WParams.print_realtime = false;
			WParams.print_timestamps = false;
			WParams.language = "en";
			WParams.n_threads = FMath::Clamp(FPlatformMisc::NumberOfCoresIncludingHyperthreads() / 2, 2, 6);
			WParams.token_timestamps = true;
			WParams.thold_pt = 0.01f;

			// Streaming optimizations (from RuntimeSpeechRecognizer research)
			WParams.single_segment = false;    // allow multiple segments for longer audio
			WParams.max_tokens = 0;            // no limit — single_segment=false handles segmentation
			WParams.no_context = true;         // each pass re-processes full buffer, no stale context
			WParams.audio_ctx = 768;           // halve encoder computation (default 1500)
			WParams.suppress_blank = true;     // kill blank hallucinations
			WParams.temperature_inc = -1.0f;   // disable temperature fallback — faster

			// Initial prompt biasing (if known text provided, e.g. ElevenLabs TTS)
			if (InitialPromptUTF8.Num() > 0)
			{
				WParams.initial_prompt = InitialPromptUTF8.GetData();
			}

			// 4. Run whisper on the ENTIRE buffer (growing buffer approach)
			const double InferenceStart = FPlatformTime::Seconds();
			int Result = whisper_full(Ctx, WParams, AudioBuffer.GetData(), TotalSamples);
			const double InferenceMs = (FPlatformTime::Seconds() - InferenceStart) * 1000.0;

			if (Result == 0)
			{
				// 5. Extract ALL words — full replacement, no de-dup needed
				TArray<FTimestampedWord> Words;
				const int32 NumSegments = whisper_full_n_segments(Ctx);

				for (int32 Seg = 0; Seg < NumSegments; Seg++)
				{
					const int32 NumTokens = whisper_full_n_tokens(Ctx, Seg);

					for (int32 Tok = 0; Tok < NumTokens; Tok++)
					{
						whisper_token_data TokData = whisper_full_get_token_data(Ctx, Seg, Tok);

						if (TokData.id >= whisper_token_eot(Ctx))
						{
							continue;
						}

						const char* TokenText = whisper_full_get_token_text(Ctx, Seg, Tok);
						if (!TokenText || TokenText[0] == '\0')
						{
							continue;
						}

						bool bStartsNewWord = (TokenText[0] == ' ');
						FString WordStr = FString(UTF8_TO_TCHAR(TokenText)).TrimStartAndEnd();
						if (WordStr.IsEmpty())
						{
							continue;
						}

						FTimestampedWord NewWord;
						NewWord.Word = WordStr;
						NewWord.StartTime = TokData.t0 * 0.01f;
						NewWord.EndTime = TokData.t1 * 0.01f;
						NewWord.bWordStart = bStartsNewWord;

						Words.Add(MoveTemp(NewWord));
					}
				}

				// 6. Publish full result (replaces previous entirely)
				{
					FScopeLock Lock(&ResultLock);
					LatestResult = MoveTemp(Words);
					bNewResultAvailable = true;
				}

				UE_LOG(LogWhisperTranscriber, Log,
					TEXT("  [INFERENCE] %.1fms | %d words | %.1fs audio | prompt=%s"),
					InferenceMs, LatestResult.Num(),
					static_cast<float>(TotalSamples) / TargetSampleRate,
					InitialPromptUTF8.Num() > 0 ? TEXT("yes") : TEXT("no"));
			}
			else
			{
				UE_LOG(LogWhisperTranscriber, Warning,
					TEXT("whisper_full returned error: %d"), Result);
			}

			// If end-of-audio and we just processed, we're done
			if (bIsEndOfAudio)
			{
				break;
			}

			// Throttle: sleep between inference passes to avoid hammering GPU
			FPlatformProcess::Sleep(ProcessIntervalSeconds);
		}
		else if (bIsEndOfAudio)
		{
			break;
		}
		else
		{
			// No data yet — sleep until next check
			FPlatformProcess::Sleep(ProcessIntervalSeconds);
		}
	}

	// Log final transcription
	FString FullTranscription;
	{
		FScopeLock Lock(&ResultLock);
		for (int32 i = 0; i < LatestResult.Num(); i++)
		{
			if (LatestResult[i].bWordStart && i > 0) FullTranscription += TEXT(" ");
			FullTranscription += LatestResult[i].Word;
		}
	}

	UE_LOG(LogWhisperTranscriber, Log,
		TEXT("Whisper worker exiting. Final: \"%s\" (%.1fs audio)"),
		*FullTranscription,
		static_cast<float>(AudioBuffer.Num()) / TargetSampleRate);

	return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API (game thread)
// ─────────────────────────────────────────────────────────────────────────────

void FWhisperTranscriber::EnqueuePCM(const TArray<float>& PCMData)
{
	if (PCMData.Num() > 0)
	{
		PendingChunks.Enqueue(PCMData);
	}
}

bool FWhisperTranscriber::DequeueResult(TArray<FTimestampedWord>& OutWords)
{
	FScopeLock Lock(&ResultLock);
	if (!bNewResultAvailable)
	{
		return false;
	}
	OutWords = LatestResult;
	bNewResultAvailable = false;
	return true;
}

void FWhisperTranscriber::SignalEndOfAudio()
{
	bEndOfAudio.Store(true);
}

void FWhisperTranscriber::WaitForCompletion()
{
	bEndOfAudio.Store(true);

	if (Thread)
	{
		Thread->WaitForCompletion();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Resampling (InputSampleRate, InputNumChannels) -> 16 kHz mono
// ─────────────────────────────────────────────────────────────────────────────

void FWhisperTranscriber::ResampleAndAppend(const TArray<float>& InPCM)
{
	if (InPCM.Num() == 0)
	{
		return;
	}

	const int32 NumFrames = InPCM.Num() / InputNumChannels;
	if (NumFrames <= 0)
	{
		return;
	}

	// Step 1: Mix to mono
	TArray<float> Mono;
	Mono.SetNumUninitialized(NumFrames);

	if (InputNumChannels == 1)
	{
		FMemory::Memcpy(Mono.GetData(), InPCM.GetData(), NumFrames * sizeof(float));
	}
	else
	{
		for (int32 i = 0; i < NumFrames; i++)
		{
			float Sum = 0.f;
			for (int32 Ch = 0; Ch < InputNumChannels; Ch++)
			{
				Sum += InPCM[i * InputNumChannels + Ch];
			}
			Mono[i] = Sum / InputNumChannels;
		}
	}

	// Step 2: Resample to 16 kHz via linear interpolation
	const int32 TargetRate = WHISPER_SAMPLE_RATE; // 16000
	if (InputSampleRate == TargetRate)
	{
		AudioBuffer.Append(Mono);
		return;
	}

	const double Ratio = static_cast<double>(TargetRate) / InputSampleRate;
	const int32 OutputLen = FMath::FloorToInt32(NumFrames * Ratio);

	if (OutputLen <= 0)
	{
		return;
	}

	const int32 PrevSize = AudioBuffer.Num();
	AudioBuffer.AddUninitialized(OutputLen);
	float* OutPtr = AudioBuffer.GetData() + PrevSize;

	for (int32 i = 0; i < OutputLen; i++)
	{
		const double SrcIdx = i / Ratio;
		const int32 Idx0 = FMath::FloorToInt32(SrcIdx);
		const int32 Idx1 = FMath::Min(Idx0 + 1, NumFrames - 1);
		const float Frac = static_cast<float>(SrcIdx - Idx0);

		OutPtr[i] = Mono[Idx0] * (1.f - Frac) + Mono[Idx1] * Frac;
	}
}
