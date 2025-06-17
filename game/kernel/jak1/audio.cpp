#include "audio.h"

#include <stdio.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <SDL3/SDL.h>
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
#include "libsm64/libsm64.h"
}

static SDL_AudioStream* stream = nullptr;
static SDL_AudioDeviceID dev;
static std::thread gSoundThread;
static std::atomic<bool> gKeepRunning = true;

long long timeInMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void audio_thread() {
    printf("[AUDIO] Audio thread started.\n");
    long long currentTime = timeInMilliseconds();
    long long targetTime = 0;

    while (gKeepRunning.load()) {
        int16_t audioBuffer[544 * 2 * 2];
        int queued = SDL_GetAudioStreamQueued(stream) / sizeof(int16_t) / 2;
        printf("[AUDIO] Queued samples: %d\n", queued);

        uint32_t numSamples = sm64_audio_tick(queued, 1100, audioBuffer);
        printf("[AUDIO] sm64_audio_tick returned %u samples\n", numSamples);

        if (queued < 6000) {
            int result = SDL_PutAudioStreamData(stream, audioBuffer, numSamples * sizeof(int16_t) * 2);
            if (result < 0) {
                fprintf(stderr, "[AUDIO][ERROR] SDL_PutAudioStreamData error: %s\n", SDL_GetError());
            } else {
                printf("[AUDIO] Queued %u samples to stream.\n", numSamples);
            }
        }

        targetTime = currentTime + 33;
        while (timeInMilliseconds() < targetTime) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (!gKeepRunning.load()) return;
        }

        currentTime = timeInMilliseconds();
    }

    printf("[AUDIO] Audio thread ending.\n");
}

void audio_init() {
    printf("[AUDIO] Initializing audio subsystem...\n");
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "[AUDIO][ERROR] SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s\n", SDL_GetError());
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
