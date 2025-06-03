/*!
 * @file kboot.cpp
 * GOAL Boot.  Contains the "main" function to launch GOAL runtime
 * DONE!
 */

#include "kboot.h"

#include <chrono>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <fstream>

#include "common/common_types.h"
#include "common/log/log.h"
#include "common/util/Timer.h"

#include "game/common/game_common_types.h"
#include "game/kernel/common/klisten.h"
#include "game/kernel/common/kprint.h"
#include "game/kernel/common/kscheme.h"
#include "game/kernel/common/ksocket.h"
#include "game/kernel/jak1/klisten.h"
#include "game/kernel/jak1/kmachine.h"
#include "game/sce/libscf.h"

using namespace ee;


// mario globals? 
int marioId = -1;
uint8_t* marioTexture;
SM64MarioState g_mario_state = {0};
SM64MarioGeometryBuffers g_geom = {0};
//SM64MarioInputs g_mario_inputs = {0};
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


void KernelCheckAndDispatch() {
  u64 goal_stack = u64(g_ee_main_mem) + EE_MAIN_MEM_SIZE - 8;
  //sm64_global_terminate(); // this is just a placeholder function call to make sure it is imported and linked by compiler we can move/change it later.

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

  // Load the mario surfaces
  sm64_static_surfaces_load( surfaces, surfaces_count);
  
  // audio_thread_init();
  // sm64_set_sound_volume(0.5f);
  // sm64_play_sound_global(SOUND_MENU_STAR_SOUND);

  delete[] romBuffer;

  // Initialize Mario  and print his ID to the console 10 times
  marioId = sm64_mario_create(5000.000000, 1000.000000, 3000.000000);
  for (int i = 0; i < 10; ++i) {
    printf("marioId = %d\n", marioId);
  }

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
  int32_t frame_num =0;
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
if (scaled_stick_x > max_stick_x) max_stick_x = scaled_stick_x;
if (scaled_stick_x < min_stick_x) min_stick_x = scaled_stick_x;
if (scaled_stick_y > max_stick_y) max_stick_y = scaled_stick_y;
if (scaled_stick_y < min_stick_y) min_stick_y = scaled_stick_y;

if (frame_num % 2 == 0) {


    // Create a temp input struct with scaled values
    SM64MarioInputs inputs = g_mario_inputs;
    inputs.stickX = scaled_stick_x;
    inputs.stickY = scaled_stick_y * -1.0f;
    printf("MARIO STICK X: %f, STICK Y: %f \n STICK Y: %f \n",
           g_mario_state.position[0], g_mario_state.position[1], g_mario_state.position[2]);
          
    sm64_mario_tick(marioId, &inputs, &g_mario_state, &g_geom);
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