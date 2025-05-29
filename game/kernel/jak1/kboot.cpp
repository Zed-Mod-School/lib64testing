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
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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
#include "libsm64/libsm64.h"
#include "libsm64/src/decomp/include/audio_defines.h"
#include "game/system/hid/devices/game_controller.h"

using namespace ee;
SM64MarioState g_mario_state;

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

 // mario globals? 
 int marioId = -1;
uint8_t* marioTexture;


//mario functions
void setup_flat_ground(SM64Surface* surfaces, size_t count){


  //surfaces.resize(2);  // ensure space for two triangles
  if (count < 2) {
  printf("[ERROR] Need at least 2 surfaces for flat ground\n");
  return;
}


  const int half_size = 5000;
  const int y_offset = -100;

  printf("[DEBUG] Setting up flat ground at y = %d with size %d\n", y_offset, half_size * 2);

  // Triangle 1 - CCW
  surfaces[0].vertices[0][0] = -half_size;
  surfaces[0].vertices[0][1] = y_offset;
  surfaces[0].vertices[0][2] = -half_size;
  surfaces[0].vertices[1][0] = half_size;
  surfaces[0].vertices[1][1] = y_offset;
  surfaces[0].vertices[1][2] = half_size;
  surfaces[0].vertices[2][0] = half_size;
  surfaces[0].vertices[2][1] = y_offset;
  surfaces[0].vertices[2][2] = -half_size;

  // Triangle 2 - CCW
  surfaces[1].vertices[0][0] = -half_size;
  surfaces[1].vertices[0][1] = y_offset;
  surfaces[1].vertices[0][2] = half_size;
  surfaces[1].vertices[1][0] = -half_size;
  surfaces[1].vertices[1][1] = y_offset;
  surfaces[1].vertices[1][2] = -half_size;
  surfaces[1].vertices[2][0] = half_size;
  surfaces[1].vertices[2][1] = y_offset;
  surfaces[1].vertices[2][2] = half_size;

  for (int i = 0; i < 2; i++) {
    printf("[DEBUG] Triangle %d:\n", i);
    for (int j = 0; j < 3; j++) {
      printf("  Vertex %d: (%d, %d, %d)\n",
             j, surfaces[i].vertices[j][0], surfaces[i].vertices[j][1], surfaces[i].vertices[j][2]);
    }
  }

  // Load static surfaces
  //sm64_static_surfaces_load(surfaces.data(),2);

  sm64_static_surfaces_load( surfaces, count );

  // // Spawn Mario at center
  // float spawnX = 0.0f;
  // float spawnZ = 0.0f;
  // float spawnY = y_offset + 1.0f;

  // SM64SurfaceCollisionData* floorTest = nullptr;
  // float floorY = sm64_surface_find_floor(spawnX, spawnY, spawnZ, &floorTest);

  // printf("[DEBUG] Floor check at (%.2f, %.2f, %.2f): floorY = %.2f, valid = %d\n",
  //        spawnX, spawnY, spawnZ, floorY, floorTest != nullptr);

  // if (!floorTest || !floorTest->isValid) {
  //   printf("[ERROR] No valid floor found. Mario will likely fall through!\n");
  // } else {
  //   printf("[DEBUG] Floor triangle:\n");
  //   printf("  V1: (%d, %d, %d)\n", floorTest->vertex1[0], floorTest->vertex1[1], floorTest->vertex1[2]);
  //   printf("  V2: (%d, %d, %d)\n", floorTest->vertex2[0], floorTest->vertex2[1], floorTest->vertex2[2]);
  //   printf("  V3: (%d, %d, %d)\n", floorTest->vertex3[0], floorTest->vertex3[1], floorTest->vertex3[2]);
  //   printf("[DEBUG] Floor normal:    (%.2f, %.2f, %.2f)\n",
  //          floorTest->normal.x, floorTest->normal.y, floorTest->normal.z);
  //   printf("[DEBUG] Floor height range: [%d, %d]\n", floorTest->lowerY, floorTest->upperY);
  //   printf("[DEBUG] Floor terrain type: %u\n", floorTest->terrain);

  //   spawnY = floorY + 1.0f;
  // }

  // printf("[INFO] Spawning Mario at: (%.2f, %.2f, %.2f)\n", spawnX, spawnY, spawnZ);
  // marioId = sm64_mario_create(spawnX, spawnY, spawnZ);
  // printf("[DEBUG] Mario ID returned: %d\n", marioId);
}

void KernelCheckAndDispatch() {
  u64 goal_stack = u64(g_ee_main_mem) + EE_MAIN_MEM_SIZE - 8;

  // Must call this once with a pointer to SM64 ROM data
  // Read the rom data (make sure it's an unmodified SM64 US ROM)
  std::ifstream file("sm64.us.z64", std::ios::ate | std::ios::binary);

  if (!file) {
    MessageBoxA(
        0,
        "Super Mario 64 US ROM not found!\nPlease provide a ROM with the filename \"sm64.us.z64\"",
        "sm64-san-andreas", 0);
    return;
  }

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
  sm64_global_init(romBuffer, marioTexture);
  sm64_audio_init(romBuffer);
  sm64_set_sound_volume(0.5f);

  // audio_thread_init();
  // sm64_play_sound_global(SOUND_MENU_STAR_SOUND);

  delete[] romBuffer;



  printf("[DEBUG] marioId = %d\n", marioId);
  SM64MarioGeometryBuffers geom;
  const int maxTris = SM64_GEO_MAX_TRIANGLES;

  geom.position = new float[3 * 3 * maxTris];  // 3 coords per vertex, 3 verts per tri
  geom.normal = new float[3 * 3 * maxTris];
  geom.color = new float[3 * 3 * maxTris];
  geom.uv = new float[2 * 3 * maxTris];  // 2 UVs per vertex, 3 verts per tri
  geom.numTrianglesUsed = 0;

   SM64Surface surfaces[2];
   setup_flat_ground(surfaces, 2);

  
  printf("[DEBUG] Loading static surfaces...\n");
  //sm64_static_surfaces_load(surfaces, 2);
  printf("[DEBUG] Surfaces loaded.\n");

  // Now spawn Mario AFTER surfaces + global init
float spawnX = 0.0f;
float spawnZ = 0.0f;
float spawnY = -99.0f;  // raised slightly above floor
marioId = sm64_mario_create(spawnX, spawnY, spawnZ);
printf("[DEBUG] Spawned Mario: ID = %d\n", marioId);
  //std::vector<SM64Surface> surfaces;
  //setup_flat_ground(surfaces);
  //load_obj_to_surfaces("fart.obj.obj", surfaces);

  // // Optionally force Y to -100
  // for (auto& surf : surfaces) {
  //   for (int j = 0; j < 3; ++j) {
  //     surf.vertices[j][1] = -100;
  //   }
  // }

  // sm64_audio_init(romBuffer);
  //         sm64_set_sound_volume(0.5f);

  //         sm64_play_sound_global(SOUND_ARG_LOAD(7, 0, 0x1E, 0xFF, 8));
  g_mario_state = {};
  static int counter = 0;
  static int last_mario_id = -233;

  while (MasterExit == RuntimeExitStatus::RUNNING) {
    // try to get a message from the listener, and process it if needed
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


    if (marioId < 0) {
  marioId = sm64_mario_create(0.0f, -99.0f, 0.0f);
  printf("[DEBUG] Respawned Mario: ID = %d\n", marioId);
}


    //Do the mario stuff down here
if (marioId >= 0) {
  static SM64MarioInputs last_inputs = {};
  static SM64MarioState last_state = {};

  // Check if inputs changed
  bool inputs_changed =
      m_mario_inputs.stickX != last_inputs.stickX ||
      m_mario_inputs.stickY != last_inputs.stickY ||
      m_mario_inputs.camLookX != last_inputs.camLookX ||
      m_mario_inputs.camLookZ != last_inputs.camLookZ ||
      m_mario_inputs.buttonA != last_inputs.buttonA ||
      m_mario_inputs.buttonB != last_inputs.buttonB ||
      m_mario_inputs.buttonZ != last_inputs.buttonZ;

  if (inputs_changed) {
    printf("[DEBUG] Mario Inputs Changed:\n");
    printf("  stickX: %d\n", m_mario_inputs.stickX);
    printf("  stickY: %d\n", m_mario_inputs.stickY);
    printf("  camLookX: %.2f\n", m_mario_inputs.camLookX);
    printf("  camLookZ: %.2f\n", m_mario_inputs.camLookZ);
    printf("  buttonA: %d, buttonB: %d, buttonZ: %d\n",
           m_mario_inputs.buttonA, m_mario_inputs.buttonB, m_mario_inputs.buttonZ);
    last_inputs = m_mario_inputs;
  }

  sm64_mario_tick(marioId, &m_mario_inputs, &g_mario_state, &geom);

  // Check if Mario state changed significantly
  bool state_changed =
      memcmp(g_mario_state.position, last_state.position, sizeof(float) * 3) != 0 ||
      memcmp(g_mario_state.velocity, last_state.velocity, sizeof(float) * 3) != 0 ||
      g_mario_state.faceAngle != last_state.faceAngle ||
      g_mario_state.health != last_state.health ||
      g_mario_state.action != last_state.action ||
      g_mario_state.flags != last_state.flags ||
      g_mario_state.particleFlags != last_state.particleFlags ||
      g_mario_state.invincTimer != last_state.invincTimer;

  if (state_changed) {
    printf("[DEBUG] Mario State Changed:\n");
    printf("  Position: (%.2f, %.2f, %.2f)\n",
           g_mario_state.position[0], g_mario_state.position[1], g_mario_state.position[2]);
    printf("  Velocity: (%.2f, %.2f, %.2f)\n",
           g_mario_state.velocity[0], g_mario_state.velocity[1], g_mario_state.velocity[2]);
    printf("  Face Angle: %.2f\n", g_mario_state.faceAngle);
    printf("  Health: %d\n", g_mario_state.health);
    printf("  Action: 0x%X\n", g_mario_state.action);
    printf("  Flags: 0x%X\n", g_mario_state.flags);
    printf("  Particle Flags: 0x%X\n", g_mario_state.particleFlags);
    printf("  Invincibility Timer: %d\n", g_mario_state.invincTimer);
    last_state = g_mario_state;
  }
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
