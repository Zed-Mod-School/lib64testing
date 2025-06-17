#include "sm64_audio_thread.h"

#include <SDL3/SDL.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdint>
#include <cstdio>

extern "C" {
#include "libsm64/libsm64.h"
}



// Audio thread control
static std::atomic<bool> keepRunning{false};
static std::thread audioThread;

static SDL_AudioDeviceID device = 0;
static SDL_AudioStream* stream = nullptr;

static constexpr int SAMPLE_RATE = 32000;
static constexpr int CHANNELS = 2;
static constexpr int SAMPLES_PER_TICK = 1024;
void AudioThreadFunc() {
    printf("[libsm64][AUDIO] Audio thread started.\n");

    constexpr size_t buffer_size = SAMPLES_PER_TICK * CHANNELS;
    int16_t buffer[buffer_size] = {0};

    while (keepRunning) {
        uint32_t num_samples = sm64_audio_tick(0, SAMPLES_PER_TICK, buffer);
        printf("[libsm64][AUDIO] sm64_audio_tick -> %u samples\n", num_samples);

        if (SDL_PutAudioStreamData(stream, buffer, num_samples * CHANNELS * sizeof(int16_t)) < 0) {
            fprintf(stderr, "[libsm64][AUDIO][ERROR] SDL_PutAudioStreamData failed: %s\n", SDL_GetError());
        }

        SDL_FlushAudioStream(stream);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
    }

    printf("[libsm64][AUDIO] Audio thread exiting.\n");
}




void StartSM64AudioThread() {
    if (keepRunning) return;

    SDL_AudioSpec want = {
        .format = SDL_AUDIO_S16,
        .channels = CHANNELS,
        .freq = SAMPLE_RATE,
    };

    printf("[libsm64][AUDIO] Opening default audio device...\n");
    device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want);
    if (!device) {
        fprintf(stderr, "[libsm64][AUDIO][FATAL] Failed to open audio device: %s\n", SDL_GetError());
        return;
    }

    SDL_AudioSpec actual = want;
    stream = SDL_CreateAudioStream(&actual, &actual);
    if (!stream) {
        fprintf(stderr, "[libsm64][AUDIO][FATAL] Failed to create audio stream: %s\n", SDL_GetError());
        SDL_CloseAudioDevice(device);
        device = 0;
        return;
    }

    if (SDL_ResumeAudioDevice(device) < 0) {
        fprintf(stderr, "[libsm64][AUDIO][FATAL] Failed to resume device: %s\n", SDL_GetError());
        SDL_DestroyAudioStream(stream);
        SDL_CloseAudioDevice(device);
        stream = nullptr;
        device = 0;
        return;
    }

    keepRunning = true;
    audioThread = std::thread(AudioThreadFunc);
}

void StopSM64AudioThread() {
    keepRunning = false;
    if (audioThread.joinable())
        audioThread.join();

    if (stream) {
        SDL_DestroyAudioStream(stream);
        stream = nullptr;
    }

    if (device) {
        SDL_CloseAudioDevice(device);
        device = 0;
    }

    printf("[libsm64][AUDIO] Cleaned up audio system.\n");
}
