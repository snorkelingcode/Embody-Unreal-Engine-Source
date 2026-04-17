// WhisperTranscriber.h
// FRunnable background worker that feeds PCM audio to whisper.cpp
// and produces timestamped words for the subtitle system.
//
// Uses a growing-buffer approach: re-processes the entire audio buffer
// each inference pass. GPU handles re-processing quickly (~30-50ms),
// and each pass produces better results as more audio arrives.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Containers/Queue.h"

// Forward-declare the whisper C types
struct whisper_context;

/** A single word with its timing within the audio stream. */
struct FTimestampedWord
{
	FString Word;
	float StartTime; // seconds from audio start
	float EndTime;   // seconds from audio start
	bool bWordStart = true; // true if token had leading space (new word boundary)
};

/**
 * Background thread that accumulates PCM audio, resamples to 16 kHz mono,
 * and runs whisper.cpp inference on the growing buffer to produce transcribed words.
 *
 * Growing-buffer approach: each inference pass re-processes the entire buffer.
 * This eliminates sliding-window de-duplication and gives progressively better
 * results as more context accumulates. GPU makes re-processing cheap.
 */
class FWhisperTranscriber : public FRunnable
{
public:
	/**
	 * @param InSampleRate   Sample rate of incoming PCM (e.g. 44100)
	 * @param InNumChannels  Channel count of incoming PCM (1 or 2)
	 * @param InInitialPrompt  Optional text hint to bias recognition (e.g. known TTS text)
	 */
	FWhisperTranscriber(int32 InSampleRate, int32 InNumChannels, const FString& InInitialPrompt = FString());
	virtual ~FWhisperTranscriber() override;

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;

	/** Start the background thread. Call after construction. */
	void StartThread();

	/** Enqueue a chunk of PCM float data (game thread). */
	void EnqueuePCM(const TArray<float>& PCMData);

	/** Drain the latest full transcription from the worker (game thread).
	 *  Returns true if a new result was available. Unlike the old per-word
	 *  queue, this returns the ENTIRE transcription each time (growing buffer). */
	bool DequeueResult(TArray<FTimestampedWord>& OutWords);

	/** Signal that no more audio will arrive. The worker will
	 *  process any remaining buffered audio and then exit. */
	void SignalEndOfAudio();

	/** Signal end-of-audio and block until the worker thread finishes.
	 *  Call from game thread. Safe to call multiple times. */
	void WaitForCompletion();

private:
	// ── Model management (shared across all instances) ──
	static whisper_context* SharedContext;
	static int32 SharedContextRefCount;
	static FCriticalSection SharedContextLock;

	static whisper_context* AcquireContext();
	static void ReleaseContext();

	// ── Resampling ──
	void ResampleAndAppend(const TArray<float>& InPCM);

	int32 InputSampleRate;
	int32 InputNumChannels;

	// ── Thread state ──
	FRunnableThread* Thread = nullptr;
	TAtomic<bool> bShouldRun{true};
	TAtomic<bool> bEndOfAudio{false};

	// ── Input queue (game thread -> worker) ──
	TQueue<TArray<float>> PendingChunks; // lock-free SPSC queue

	// ── Accumulated 16 kHz mono buffer (worker thread only) ──
	TArray<float> AudioBuffer;

	// ── Processing state (worker thread only) ──
	float ProcessIntervalSeconds = 0.3f;  // re-run inference every 300ms
	float MinBufferSeconds = 0.5f;        // don't process until we have this much audio
	int32 LastProcessedBufferSize = 0;    // skip inference if no new audio since last pass

	// ── Initial prompt for biasing recognition ──
	FString InitialPrompt;
	TArray<char> InitialPromptUTF8; // kept alive for whisper_full_params pointer

	// ── Result (worker -> game thread) ──
	// Full replacement: each inference replaces the previous result entirely
	FCriticalSection ResultLock;
	TArray<FTimestampedWord> LatestResult;
	bool bNewResultAvailable = false;

	// ── Whisper context for this instance ──
	whisper_context* Ctx = nullptr;
};
