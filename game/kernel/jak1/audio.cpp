#include "audio.h"

#include <stdio.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <SDL3/SDL.h>
#ifdef _WIN32
#include <windows.h>  // For Sleep if you want it directly
#endif

extern "C" {
#include "libsm64/libsm64.h"
//#include "context.h"
}

static SDL_AudioStream* stream = nullptr;
static SDL_AudioDeviceID dev;
static std::thread gSoundThread;
static std::atomic<bool> gKeepRunning = true;

// Cross-platform time in milliseconds using std::chrono
long long timeInMilliseconds()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void audio_thread()
{
    long long currentTime = timeInMilliseconds();
    long long targetTime = 0;

    while (gKeepRunning.load())
    {
        int16_t audioBuffer[544 * 2 * 2];
        int queued = SDL_GetAudioStreamQueued(stream) / sizeof(int16_t) / 2;

        uint32_t numSamples = sm64_audio_tick(queued, 1100, audioBuffer);

        if (queued < 6000) {
            if (SDL_PutAudioStreamData(stream, audioBuffer, numSamples * sizeof(int16_t) * 2) < 0) {
                fprintf(stderr, "SDL_PutAudioStreamData error: %s\n", SDL_GetError());
            }
        }

        targetTime = currentTime + 33;

        while (timeInMilliseconds() < targetTime)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (!gKeepRunning.load()) return;
        }

        currentTime = timeInMilliseconds();
    }
}

void audio_init()
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
    {
        fprintf(stderr, "SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s\n", SDL_GetError());
        return;
    }

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 32000;
    want.format = SDL_AUDIO_S16LE;
    want.channels = 2;

	SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = 32000;
    spec.format = SDL_AUDIO_S16LE;
    spec.channels = 2;
	
    SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
	//SDL_AudioStream* SDL_OpenAudioDeviceStream(SDL_AudioDeviceID devid, const SDL_AudioSpec* spec, SDL_AudioFilterCallback filter, void* userdata);

    if (dev == 0)
    {
        fprintf(stderr, "SDL_OpenAudio error: %s\n", SDL_GetError());
        return;
    }

    SDL_ResumeAudioStreamDevice(stream);

    // Start audio thread (cross-platform using std::thread)
    gSoundThread = std::thread(audio_thread);
}

void audio_shutdown()
{
    gKeepRunning.store(false);
    if (gSoundThread.joinable())
        gSoundThread.join();

    SDL_CloseAudioDevice(dev);
}
