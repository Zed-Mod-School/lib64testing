/*!
 * @file kboot.cpp
 * GOAL Boot.  Contains the "main" function to launch GOAL runtime
 * DONE!
 */

#include "kboot.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <math.h>

#include <SDL3/SDL.h>

#include "common/common_types.h"
#include "common/log/log.h"
#include "common/util/Timer.h"

#include "game/kernel/common/kmachine.h"

#include "game/common/game_common_types.h"
#include "game/kernel/common/klisten.h"
#include "game/kernel/common/kprint.h"
#include "game/kernel/common/kscheme.h"
#include "game/kernel/common/ksocket.h"
#include "game/kernel/jak1/klisten.h"
#include "game/kernel/jak1/kmachine.h"
#include "game/kernel/jak1/audio.h"
#include "game/sce/libscf.h"

using namespace ee;



void PlayTestWav() {
    printf("[libsm64][DEBUG] Starting WAV audio playback test (SDL3_OpenAudioDeviceStream-compliant).\n");

    const char* test_wav_path = "test.wav"; // Path to your test WAV file
    SDL_AudioSpec wav_spec;
    uint8_t *wav_buffer = NULL;
    uint32_t wav_length = 0;
    SDL_AudioStream *test_stream = NULL; // Moved stream declaration here

    // 1. Load the WAV file using the simpler SDL_LoadWAV (if available)
    if (SDL_LoadWAV(test_wav_path, &wav_spec, &wav_buffer, &wav_length) == -1) {
        fprintf(stderr, "[libsm64][ERROR] Failed to load test.wav '%s': %s\n", test_wav_path, SDL_GetError());
        return;
    }

    printf("[libsm64][DEBUG] Loaded test.wav: Format=%s, Channels=%d, Freq=%d, Length=%u bytes\n",
           SDL_GetAudioFormatName(wav_spec.format), wav_spec.channels, wav_spec.freq, wav_length);

    // 2. Open an audio stream directly associated with the default playback device
    test_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &wav_spec, NULL, NULL);
    if (!test_stream) {
        fprintf(stderr, "[libsm64][ERROR] Failed to open audio device stream: %s\n", SDL_GetError());
        SDL_free(wav_buffer);
        return;
    }
    printf("[libsm64][DEBUG] Successfully opened test audio device stream.\n");

    // 3. Put the WAV data into the stream
    if (SDL_PutAudioStreamData(test_stream, wav_buffer, wav_length) < 0) {
        fprintf(stderr, "[libsm64][ERROR] Failed to put WAV data into stream: %s\n", SDL_GetError());
        SDL_DestroyAudioStream(test_stream); // Correct cleanup here
        SDL_free(wav_buffer);
        return;
    }
    SDL_free(wav_buffer); // Data has been copied to the stream, can free original buffer

    printf("[libsm64][DEBUG] WAV data queued into stream.\n");

    // 4. Resume the stream's device to start playback
    if (SDL_ResumeAudioStreamDevice(test_stream) < 0) {
        fprintf(stderr, "[libsm64][ERROR] Failed to resume audio stream device: %s\n", SDL_GetError());
        SDL_DestroyAudioStream(test_stream); // Correct cleanup here
        return;
    }
    printf("[libsm64][DEBUG] Audio stream device resumed. Playing WAV...\n");

    // 5. Wait for playback to complete
    printf("[libsm64][DEBUG] Waiting for WAV playback to finish...\n");
    // We wait until the stream has no more data queued from us.
    while (SDL_GetAudioStreamQueued(test_stream) > 0) {
        SDL_Delay(50); // Small delay to avoid busy-waiting
    }

    // Add a slight delay to ensure the last buffered samples play out
    SDL_Delay(500);

    printf("[libsm64][DEBUG] Finished WAV audio playback test.\n");

    // 6. Clean up
    SDL_DestroyAudioStream(test_stream); // Correct cleanup here: Destroys the stream and closes the device
    printf("[libsm64][DEBUG] Test audio device stream closed.\n");
}


// mario globals?
int marioId = -1;
uint8_t* marioTexture;
SM64MarioState g_mario_state = {0};
SM64MarioGeometryBuffers g_geom = {0};
// SM64MarioInputs g_mario_inputs = {0};
SM64MarioInputs g_mario_inputs = {.camLookX = 0.0f,
                                  .camLookZ = 1.0f,
                                  .stickX = 0.0f,
                                  .stickY = 0.0f,
                                  .buttonA = 0,
                                  .buttonB = 0,
                                  .buttonZ = 0};

namespace jak1 {
VideoMode BootVideoMode;

void kboot_init_globals() {}

/*!
 * Launch the GOAL Kernel (EE).
 * DONE!
 * See InitParms for launch argument details.
 * @param argc : argument count
 * @param argv : argument list
 * @return 0 on success, otherwise failure.
 *
 * CHANGES:
 * Added InitParms call to handle command line arguments
 * Removed hard-coded debug mode disable
 * Renamed from `main` to `goal_main`
 * Add call to sceDeci2Reset when GOAL shuts down.
 */
s32 goal_main(int argc, const char* const* argv) {
  // Initialize global variables based on command line parameters
  // This call is not present in the retail version of the game
  // but the function is, and it likely goes here.
  InitParms(argc, argv);

  // Initialize CRC32 table for string hashing
  init_crc();

  // NTSC V1, NTSC v2, PAL CD Demo, PAL Retail
  // Set up game configurations
  masterConfig.aspect = (u16)sceScfGetAspect();
  masterConfig.language = (u16)sceScfGetLanguage();
  masterConfig.inactive_timeout = 0;  // demo thing
  masterConfig.timeout = 0;           // demo thing
  masterConfig.volume = 100;

  // Set up language configuration
  if (masterConfig.language == SCE_SPANISH_LANGUAGE) {
    masterConfig.language = (u16)Language::Spanish;
  } else if (masterConfig.language == SCE_FRENCH_LANGUAGE) {
    masterConfig.language = (u16)Language::French;
  } else if (masterConfig.language == SCE_GERMAN_LANGUAGE) {
    masterConfig.language = (u16)Language::German;
  } else if (masterConfig.language == SCE_ITALIAN_LANGUAGE) {
    masterConfig.language = (u16)Language::Italian;
  } else if (masterConfig.language == SCE_JAPANESE_LANGUAGE) {
    // Note: this case was added so it is easier to test Japanese fonts.
    masterConfig.language = (u16)Language::Japanese;
  } else {
    // pick english by default, if language is not supported.
    masterConfig.language = (u16)Language::English;
  }

  // Set up aspect ratio override in demo
  if (!strcmp(DebugBootMessage, "demo") || !strcmp(DebugBootMessage, "demo-shared")) {
    masterConfig.aspect = SCE_ASPECT_FULL;
  }

  // In retail game, disable debugging modes, and force on DiskBoot
  // MasterDebug = 0;
  // DiskBoot = 1;
  // DebugSegment = 0;

  // Launch GOAL!
  if (InitMachine() >= 0) {    // init kernel
    KernelCheckAndDispatch();  // run kernel
    ShutdownMachine();         // kernel died, we should too.
  } else {
    fprintf(stderr, "InitMachine failed\n");
    exit(1);
  }

  return 0;
}

/*!
 * Main loop to dispatch the GOAL kernel.
 */


#define CYLINDER_RADIUS 2000.0f
#define CYLINDER_RADIUS_SQ (CYLINDER_RADIUS * CYLINDER_RADIUS)
#define CYLINDER_BUFFER 1000.0f

static float g_cylinder_center[3] = {-7541.8f, 1688.475f, 9237.5f};  // initial Mario position

// Convert fixed-point vertex to float (or adjust based on actual vertex format if needed)
static inline float to_f32(int32_t val) {
  return static_cast<float>(val);
}

// Check if point is inside triangle in XZ plane
bool point_in_triangle_2d(float px, float pz, const int32_t v[3][3]) {
  float x0 = to_f32(v[0][0]), z0 = to_f32(v[0][2]);
  float x1 = to_f32(v[1][0]), z1 = to_f32(v[1][2]);
  float x2 = to_f32(v[2][0]), z2 = to_f32(v[2][2]);

  float dX = px - x2;
  float dZ = pz - z2;
  float dX21 = x2 - x1;
  float dZ12 = z1 - z2;
  float D = (z0 - z2) * dX21 + (x2 - x0) * dZ12;
  float s = ((z0 - z2) * dX + (x2 - x0) * dZ) / D;
  float t = ((z2 - z1) * dX + (x1 - x2) * dZ) / D;

  return s >= 0 && t >= 0 && (s + t) <= 1;
}

// Sample points inside triangle to check for cylinder overlap
bool triangle_samples_in_cylinder(float center_x, float center_z, const int32_t v[3][3]) {
  const float samples[7][2] = {
      {1.0f / 3, 1.0f / 3},
      {0.6f, 0.2f}, {0.2f, 0.6f},
      {0.2f, 0.2f}, {0.5f, 0.25f},
      {0.25f, 0.5f}, {0.4f, 0.4f},
  };

  for (int i = 0; i < 7; ++i) {
    float a = samples[i][0];
    float b = samples[i][1];
    float c = 1.0f - a - b;

    float px = a * to_f32(v[0][0]) + b * to_f32(v[1][0]) + c * to_f32(v[2][0]);
    float pz = a * to_f32(v[0][2]) + b * to_f32(v[1][2]) + c * to_f32(v[2][2]);

    float dx = px - center_x;
    float dz = pz - center_z;
    if (dx * dx + dz * dz <= CYLINDER_RADIUS_SQ)
      return true;
  }
  return false;
}

int load_surfaces_near(float x, float y, float z) {
  if (!g_combined_surfaces || g_combined_surfaces_count == 0)
    return 0;

  SM64Surface* filtered = (SM64Surface*)malloc(sizeof(SM64Surface) * g_combined_surfaces_count);
  if (!filtered) return 0;

  int count = 0;

  for (int i = 0; i < g_combined_surfaces_count; ++i) {
    const SM64Surface* s = &g_combined_surfaces[i];
    bool include = false;

    // 1. Any vertex in cylinder
    for (int v = 0; v < 3; ++v) {
      float dx = to_f32(s->vertices[v][0]) - x;
      float dz = to_f32(s->vertices[v][2]) - z;
      if (dx * dx + dz * dz <= CYLINDER_RADIUS_SQ) {
        include = true;
        break;
      }
    }

    // 2. Cylinder center inside triangle
    if (!include && point_in_triangle_2d(x, z, s->vertices)) {
      include = true;
    }

    // 3. Triangle samples intersect cylinder
    if (!include && triangle_samples_in_cylinder(x, z, s->vertices)) {
      include = true;
    }

    if (include) {
      filtered[count++] = *s;
    }
  }

  sm64_static_surfaces_load(filtered, count);
  free(filtered);

  printf("🔄 Reloaded %d static surfaces near (%.1f, %.1f, %.1f)\n", count, x, y, z);
  return count;
}

void maybe_reload_surfaces(const float* mario_pos) {
  float dx = mario_pos[0] - g_cylinder_center[0];
  float dz = mario_pos[2] - g_cylinder_center[2];
  float dist_sq = dx * dx + dz * dz;

  float reload_threshold_sq = (CYLINDER_RADIUS - CYLINDER_BUFFER) * (CYLINDER_RADIUS - CYLINDER_BUFFER);
  if (dist_sq > reload_threshold_sq) {
    g_cylinder_center[0] = mario_pos[0];
    g_cylinder_center[1] = mario_pos[1];
    g_cylinder_center[2] = mario_pos[2];

    load_surfaces_near(g_cylinder_center[0], g_cylinder_center[1], g_cylinder_center[2]);
  }
}

void KernelCheckAndDispatch() {

  u64 goal_stack = u64(g_ee_main_mem) + EE_MAIN_MEM_SIZE - 8;
  // sm64_global_terminate(); // this is just a placeholder function call to make sure it is
  // imported and linked by compiler we can move/change it later.

  // MARIO STUFF THAT ONLY RUNS ONCE
  // Read the rom data (make sure it's an unmodified SM64 US ROM)
  std::ifstream file("sm64.us.z64", std::ios::ate | std::ios::binary);

  // load ROM into memory
  uint8_t* romBuffer;
  size_t romFileLength = file.tellg();
  romBuffer = new uint8_t[romFileLength + 1];
  file.seekg(0);
  file.read((char*)romBuffer, romFileLength);
  romBuffer[romFileLength] = 0;

  // Mario texture is 704x64 RGBA
  marioTexture = new uint8_t[4 * SM64_TEXTURE_WIDTH * SM64_TEXTURE_HEIGHT];

  // load libsm64
  // sm64_global_terminate();
  sm64_global_init(romBuffer, marioTexture);

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    fprintf(stderr, "[libsm64][ERROR] SDL_Init failed: %s\n", SDL_GetError());
    return;
}
 // PlayTestWav();
  // Load the mario surfaces
  // sm64_static_surfaces_load( surfaces, surfaces_count);

  sm64_audio_init(romBuffer);

  // audio_thread_init();
  // sm64_set_sound_volume(0.5f);
  // sm64_play_sound_global(SOUND_MENU_STAR_SOUND);

  //delete[] romBuffer;
   audio_init();
   
    sm64_play_music(0, 0x05 | 0x80, 0); // from decomp/include/seq_ids.h: SEQ_LEVEL_WATER | SEQ_VARIATION
  // Initialize Mario  and print his ID to the console 10 times
  // marioId = sm64_mario_create(-7541.8, 1688.475, 9237.5);
  // for (int i = 0; i < 10; ++i) {
  //   printf("marioId = %d\n", marioId);
  // }

  const int maxTris = SM64_GEO_MAX_TRIANGLES;
  g_geom.position = new float[3 * 3 * maxTris];  // 3 coords per vertex, 3 verts per tri
  g_geom.normal = new float[3 * 3 * maxTris];
  g_geom.color = new float[3 * 3 * maxTris];
  g_geom.uv = new float[2 * 3 * maxTris];  // 2 UVs per vertex, 3 verts per tri
  memset(g_geom.position, 0, sizeof(float) * 3 * 3 * maxTris);
  memset(g_geom.normal, 0, sizeof(float) * 3 * 3 * maxTris);
  memset(g_geom.color, 0, sizeof(float) * 3 * 3 * maxTris);
  memset(g_geom.uv, 0, sizeof(float) * 2 * 3 * maxTris);
  g_geom.numTrianglesUsed = 0;
  int32_t frame_num = 0;
  // END MARIO STUFF THAT ONLY RUNS ONCE

  float max_stick_x = -1.0f, min_stick_x = 1.0f;
  float max_stick_y = -1.0f, min_stick_y = 1.0f;

  while (MasterExit == RuntimeExitStatus::RUNNING) {
    // try to get a message from the listener, and process it if needed
    // Only store raw values in g_mario_inputs
    // Then scale into local variables
    float scaled_stick_x = g_mario_inputs.stickX / 64.0f;
    float scaled_stick_y = g_mario_inputs.stickY / 64.0f;

    // Track min/max of the scaled values
    if (scaled_stick_x > max_stick_x)
      max_stick_x = scaled_stick_x;
    if (scaled_stick_x < min_stick_x)
      min_stick_x = scaled_stick_x;
    if (scaled_stick_y > max_stick_y)
      max_stick_y = scaled_stick_y;
    if (scaled_stick_y < min_stick_y)
      min_stick_y = scaled_stick_y;

    if (frame_num % 2 == 0 && marioId != -1) {
      // printf("MARIO STICK X: %f (min: %f, max: %f), STICK Y: %f (min: %f, max: %f)\n",
      //        scaled_stick_x, min_stick_x, max_stick_x, scaled_stick_y, min_stick_y, max_stick_y);

      // Create a temp input struct with scaled values
      SM64MarioInputs inputs = g_mario_inputs;
      inputs.stickX = scaled_stick_x;
      inputs.stickY = -scaled_stick_y;

      sm64_mario_tick(marioId, &inputs, &g_mario_state, &g_geom);
      maybe_reload_surfaces(g_mario_state.position);
      frame_num = 0;
    }

    frame_num++;
    // END MARIO STUFF THAT RUNS EVERY FRAME
    Ptr<char> new_message = WaitForMessageAndAck();
    if (new_message.offset) {
      ProcessListenerMessage(new_message);
    }

    // remember the old listener function
    auto old_listener = ListenerFunction->value;
    // dispatch the kernel
    //(**kernel_dispatcher)();

    Timer kernel_dispatch_timer;
    if (MasterUseKernel) {
      // use the GOAL kernel.
      call_goal_on_stack(Ptr<Function>(kernel_dispatcher->value), goal_stack, s7.offset,
                         g_ee_main_mem);
    } else {
      // use a hack to just run the listener function if there's no GOAL kernel.
      if (ListenerFunction->value != s7.offset) {
        auto result = call_goal_on_stack(Ptr<Function>(ListenerFunction->value), goal_stack,
                                         s7.offset, g_ee_main_mem);
#ifdef __linux__
        cprintf("%ld\n", result);
#else
        cprintf("%lld\n", result);
#endif
        ListenerFunction->value = s7.offset;
      }
    }

    auto time_ms = kernel_dispatch_timer.getMs();
    if (time_ms > 50) {
      lg::print("Kernel dispatch time: {:.3f} ms\n", time_ms);
    }

    ClearPending();

    // if the listener function changed, it means the kernel ran it, so we should notify compiler.
    if (MasterDebug && ListenerFunction->value != old_listener) {
      SendAck();
    }

    if (time_ms < 4) {
      std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }
  }
}

/*!
 * Stop running the GOAL Kernel.
 * DONE, EXACT
 */
void KernelShutdown() {
  MasterExit = RuntimeExitStatus::EXIT;  // GOAL Kernel Dispatch loop will stop now.
}
}  // namespace jak1

uint64_t pc_get_mario_x() {
  float x = g_mario_state.position[0];
  uint64_t out = 0;
  std::memcpy(&out, &x, sizeof(float));
  // printf("[pc_get_mario_x] X = %.2f -> 0x%lx\n", x, out);
  return out;
}

uint64_t pc_get_mario_y() {
  float y = g_mario_state.position[1];
  uint64_t out = 0;
  std::memcpy(&out, &y, sizeof(float));
  // printf("[pc_get_mario_y] Y = %.2f -> 0x%lx\n", y, out);
  return out;
}

uint64_t pc_get_mario_z() {
  float z = g_mario_state.position[2];
  uint64_t out = 0;
  std::memcpy(&out, &z, sizeof(float));
  // printf("[pc_get_mario_z] Z = %.2f -> 0x%lx\n", z, out);
  return out;
}

