/*!
 * @file kboot.cpp
 * GOAL Boot.  Contains the "main" function to launch GOAL runtime
 * DONE!
 */

#include "kboot.h"
#include "game/system/hid/input_manager.h"

extern std::unique_ptr<InputManager> g_input_manager;
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
#include "game/graphics/opengl_renderer/MarioRenderer.h"

static std::unique_ptr<MarioRenderer> g_mario_renderer;


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

void KernelCheckAndDispatch() {
  u64 goal_stack = u64(g_ee_main_mem) + EE_MAIN_MEM_SIZE - 8;

  // Must call this once with a pointer to SM64 ROM data
  // Read the rom data (make sure it's an unmodified SM64 US ROM)
  std::ifstream file("sm64.us.z64", std::ios::ate | std::ios::binary);

  // if (!file) {
  //   MessageBoxA(
  //       0,
  //       "Super Mario 64 US ROM not found!\nPlease provide a ROM with the filename \"sm64.us.z64\"",
  //       "sm64-san-andreas", 0);
  //   return;
  // }

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
  sm64_global_terminate();
  sm64_global_init(romBuffer, marioTexture);


  sm64_static_surfaces_load( surfaces, surfaces_count);
  sm64_set_sound_volume(0.5f);

  // audio_thread_init();
  // sm64_play_sound_global(SOUND_MENU_STAR_SOUND);
  
  delete[] romBuffer;

  SM64MarioGeometryBuffers geom;
  const int maxTris = SM64_GEO_MAX_TRIANGLES;

  geom.position = new float[3 * 3 * maxTris];  // 3 coords per vertex, 3 verts per tri
  geom.normal = new float[3 * 3 * maxTris];
  geom.color = new float[3 * 3 * maxTris];
  geom.uv = new float[2 * 3 * maxTris];  // 2 UVs per vertex, 3 verts per tri
  geom.numTrianglesUsed = 0;
  
  marioId = sm64_mario_create(5000.000000, 1000.000000, 3000.000000);
  
  g_mario_renderer = std::make_unique<MarioRenderer>(GameVersion::Jak1);


  for (int i = 0; i < 10; ++i) {
    printf("marioId = %d\n", marioId);
  }
 

  g_mario_state = {};
  while (MasterExit == RuntimeExitStatus::RUNNING) {
    // try to get a message from the listener, and process it if needed

// auto pad_data_opt = g_input_manager->get_current_data(0);
// if (pad_data_opt.has_value()) {
//   const auto& pad = pad_data_opt.value();

//   // Get right analog stick as floats in range [-1, 1]
//   auto [rx, ry] = pad->analog_right();
//   m_mario_inputs.stickX = ((int)rx - 127) / 127.0f;
//   m_mario_inputs.stickY = ((int)ry - 127) / 127.0f;

//   // Button booleans
//   m_mario_inputs.buttonA = pad->cross().first ? 1 : 0;
//   m_mario_inputs.buttonB = pad->circle().first ? 1 : 0;
//   m_mario_inputs.buttonZ = pad->r1().first ? 1 : 0;
// } else {
//   // fallback to no input if controller isn't active
//   m_mario_inputs.stickX = 0.0f;
//   m_mario_inputs.stickY = 0.0f;
//   m_mario_inputs.buttonA = 0;
//   m_mario_inputs.buttonB = 0;
//   m_mario_inputs.buttonZ = 0;
// }

// // Optional: keep Mario facing camera
// m_mario_inputs.camLookX = 0.0f;
// m_mario_inputs.camLookZ = 1.0f;

// printf("[Mario State] Pos: (%.2f, %.2f, %.2f), Vel: (%.2f, %.2f, %.2f), Action: 0x%X\n",
//        g_mario_state.position[0],
//        g_mario_state.position[1],
//        g_mario_state.position[2],
//        g_mario_state.velocity[0],
//        g_mario_state.velocity[1],
//        g_mario_state.velocity[2],
//        g_mario_state.action);
       
// Step 1: Update Mario's mesh
sm64_mario_tick(marioId, &m_mario_inputs, &g_mario_state, &geom);
g_mario_renderer->update_geometry(geom);

// Step 2: Camera matrix using your math::Matrix4f
math::Matrix4f view = math::Matrix4f::identity();
math::Matrix4f proj = math::Matrix4f::identity();

// Basic hardcoded projection (90° FOV, 4:3 aspect, near=100, far=10000)
proj(0, 0) = 1.0f / (4.0f / 3.0f);  // aspect
proj(1, 1) = 1.0f;
proj(2, 2) = -1.0002f;
proj(2, 3) = -1.0f;
proj(3, 2) = -200.02f;  // -(2*far*near)/(far-near)
proj(3, 3) = 0.0f;

// Simple view matrix from behind and above Mario
math::Vector3f mario_pos(
  g_mario_state.position[0],
  g_mario_state.position[1],
  g_mario_state.position[2]
);

math::Vector3f eye = mario_pos + math::Vector3f(0, 300, 500);
math::Vector3f up = math::Vector3f(0, 1, 0);
math::Vector3f forward = (mario_pos - eye).normalized();
math::Vector3f right = up.cross(forward).normalized();
up = forward.cross(right);  // fix up

view(0, 0) = right.x(); view(1, 0) = right.y(); view(2, 0) = right.z();
view(0, 1) = up.x();    view(1, 1) = up.y();    view(2, 1) = up.z();
view(0, 2) = -forward.x(); view(1, 2) = -forward.y(); view(2, 2) = -forward.z();

view(3, 0) = -right.dot(eye);
view(3, 1) = -up.dot(eye);
view(3, 2) = forward.dot(eye);  // reversed forward

// Combine view and projection
math::Matrix4f cam_matrix = proj * view;

// Step 3: Render
g_mario_renderer->render(nullptr, cam_matrix);

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