// This code could be cleaned up later, but for now it works so there isnt a reason to touch it.
#include "audio.h"

#include <atomic>
#include <chrono>
#include <stdio.h>
#include <thread>

#include "SDL3/SDL.h"
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
#include "libsm64.h"
}
#define USE_SDL3

static SDL_AudioStream* stream = nullptr;
static SDL_AudioDeviceID dev;
static std::thread gSoundThread;
static std::atomic<bool> gKeepRunning = true;

long long timeInMilliseconds() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void write_wav_header(FILE* file,
                      int sampleRate,
                      int numChannels,
                      int bitsPerSample,
                      int dataSize) {
  int byteRate = sampleRate * numChannels * bitsPerSample / 8;
  int blockAlign = numChannels * bitsPerSample / 8;
  int chunkSize = 36 + dataSize;

  fwrite("RIFF", 1, 4, file);
  fwrite(&chunkSize, 4, 1, file);  // file size - 8
  fwrite("WAVE", 1, 4, file);
  fwrite("fmt ", 1, 4, file);

  int subchunk1Size = 16;
  short audioFormat = 1;  // PCM
  fwrite(&subchunk1Size, 4, 1, file);
  fwrite(&audioFormat, 2, 1, file);
  fwrite(&numChannels, 2, 1, file);
  fwrite(&sampleRate, 4, 1, file);
  fwrite(&byteRate, 4, 1, file);
  fwrite(&blockAlign, 2, 1, file);
  fwrite(&bitsPerSample, 2, 1, file);

  fwrite("data", 1, 4, file);
  fwrite(&dataSize, 4, 1, file);
}

void patch_wav_header(FILE* file, int dataSize) {
  int chunkSize = 36 + dataSize;
  fseek(file, 4, SEEK_SET);
  fwrite(&chunkSize, 4, 1, file);
  fseek(file, 40, SEEK_SET);
  fwrite(&dataSize, 4, 1, file);
  fflush(file);
}

void audio_thread() {
  long long currentTime = timeInMilliseconds();
  long long startTime = currentTime;
  long long lastPrintTime = currentTime;
  long long targetTime = 0;
  int tickCount = 0;

  FILE* wavFile = fopen("output.wav", "wb");
  int sampleRate = 32000;
  int channels = 2;
  int bitsPerSample = 16;
  int totalBytesWritten = 0;

  // Write dummy header to reserve space
  write_wav_header(wavFile, sampleRate, channels, bitsPerSample, 0);

  while (gKeepRunning.load()) {
    int16_t audioBuffer[544 * 2 * 2];
    int queued = SDL_GetAudioStreamQueued(stream) / sizeof(int16_t) / 2;

    uint32_t numSamples = sm64_audio_tick(queued, 1100, audioBuffer);
    // Time tracking
    static long long lastTickTime = 0;
    long long currentTime = timeInMilliseconds();
    long long delta = (lastTickTime > 0) ? (currentTime - lastTickTime) : 0;
    lastTickTime = currentTime;

    // Sample range analysis
    int16_t min = 32767, max = -32768;
    for (int i = 0; i < numSamples * 2; ++i) {
      if (audioBuffer[i] < min)
        min = audioBuffer[i];
      if (audioBuffer[i] > max)
        max = audioBuffer[i];
    }

    // Queued size
    int queuedBytes;
#ifdef USE_SDL3
    queuedBytes = SDL_GetAudioStreamQueued(stream);
#else
    queuedBytes = SDL_GetQueuedAudioSize(dev);
#endif

    // Bytes we plan to write
    int bytesToWrite = numSamples * 2 * sizeof(int16_t);  // stereo 16-bit

    // Log the unified debug line
    // printf(
    //     "[TICK] t=%lldms | Δt=%lld | ticked=%u samples | queued=%d bytes | min=%d max=%d | "
    //     "write=%d bytes\n",
    //     currentTime, delta, numSamples, queuedBytes, min, max, bytesToWrite);

    tickCount++;

    if (timeInMilliseconds() - startTime <= 20000) {
      size_t written = fwrite(audioBuffer, sizeof(int16_t), numSamples * 2, wavFile);
      totalBytesWritten += static_cast<int>(written * sizeof(int16_t));
    }

    if (queued < 6000) {
#ifdef USE_SDL3
      int queuedBytes = SDL_GetAudioStreamQueued(stream);
      SDL_PutAudioStreamData(stream, audioBuffer, numSamples * 2 * 4);
      //   printf("Value: %u\n", numSamples * 2 * 4);
      //   printf("Value: %u\n", bytesToWrite);

#else
      int queuedBytes = SDL_GetQueuedAudioSize(dev);
      SDL_QueueAudio(dev, audioBuffer, bytesToWrite);
#endif
    }

    currentTime = timeInMilliseconds();

    targetTime = currentTime + 33;
    while (timeInMilliseconds() < targetTime) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      if (!gKeepRunning.load())
        return;
    }
  }
}

void audio_init() {
  printf("[AUDIO] Initializing audio subsystem...\n");
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
    fprintf(stderr, "[AUDIO][ERROR] SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s\n",
            SDL_GetError());
    return;
  }

  SDL_AudioSpec spec;
  SDL_zero(spec);
  spec.freq = 32000;
  spec.format = SDL_AUDIO_S16LE;
  spec.channels = 2;

  printf("[AUDIO] Opening audio stream...\n");
  stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
  if (!stream) {
    fprintf(stderr, "[AUDIO][ERROR] SDL_OpenAudioDeviceStream failed: %s\n", SDL_GetError());
    return;
  }

  dev = SDL_GetAudioStreamDevice(stream);
  printf("[AUDIO] Opened audio stream with device ID %u\n", dev);

  printf("[AUDIO] Resuming audio stream...\n");
  if (SDL_ResumeAudioStreamDevice(stream) < 0) {
    fprintf(stderr, "[AUDIO][ERROR] SDL_ResumeAudioStreamDevice failed: %s\n", SDL_GetError());
    return;
  }

  printf("[AUDIO] Launching audio thread...\n");
  gSoundThread = std::thread(audio_thread);
}

void audio_shutdown() {
  printf("[AUDIO] Shutting down audio system...\n");
  gKeepRunning.store(false);
  if (gSoundThread.joinable()) {
    gSoundThread.join();
    printf("[AUDIO] Audio thread joined successfully.\n");
  }

  if (stream) {
    SDL_CloseAudioDevice(dev);
    stream = nullptr;
    printf("[AUDIO] Closed audio device.\n");
  }

  printf("[AUDIO] Audio shutdown complete.\n");
}
