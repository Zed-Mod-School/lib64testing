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
#include "game/system/hid/devices/game_controller.h"
#include <libsm64/libsm64.h>
#include <libsm64/src/decomp/include/audio_defines.h>

#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include <cstring>


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

int marioId = -1;

uint8_t* marioTexture;


void save_surfaces_as_obj(const std::vector<SM64Surface>& surfaces, const std::string& filepath) {
  std::ofstream out(filepath);
  if (!out) {
    printf("[ERROR] Could not open output file: %s\n", filepath.c_str());
    return;
  }

  std::vector<std::array<int16_t, 3>> unique_vertices;
  std::unordered_map<std::string, int> vertex_index_map;

  auto vertex_key = [](int16_t x, int16_t y, int16_t z) {
    return std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(z);
  };

  // Write all unique vertices
  for (const auto& surf : surfaces) {
    for (int i = 0; i < 3; ++i) {
      int16_t x = surf.vertices[i][0];
      int16_t y = surf.vertices[i][1];
      int16_t z = surf.vertices[i][2];
      std::string key = vertex_key(x, y, z);
      if (vertex_index_map.find(key) == vertex_index_map.end()) {
        vertex_index_map[key] = unique_vertices.size() + 1;  // 1-based index
        unique_vertices.push_back({x, y, z});
        out << "v " << x << " " << y << " " << z << "\n";
      }
    }
  }

  // Write faces
  for (const auto& surf : surfaces) {
    std::string indices[3];
    for (int i = 0; i < 3; ++i) {
      int16_t x = surf.vertices[i][0];
      int16_t y = surf.vertices[i][1];
      int16_t z = surf.vertices[i][2];
      indices[i] = std::to_string(vertex_index_map[vertex_key(x, y, z)]);
    }
    out << "f " << indices[0] << " " << indices[1] << " " << indices[2] << "\n";
  }

  printf("[DEBUG] Exported %zu surfaces as OBJ to: %s\n", surfaces.size(), filepath.c_str());
}



void load_obj_to_surfaces(const std::string& filepath, std::vector<SM64Surface>& out_surfaces) {
  std::ifstream in(filepath);
  if (!in) {
    printf("[ERROR] Could not open OBJ file: %s\n", filepath.c_str());
    return;
  }

  std::vector<std::array<float, 3>> vertices;
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ss(line);
    std::string type;
    ss >> type;

    if (type == "v") {
      float x, y, z;
      ss >> x >> y >> z;
      vertices.push_back({x, y, z});
    } else if (type == "f") {
      int vi[3];
      for (int i = 0; i < 3; ++i) {
        std::string token;
        ss >> token;
        size_t pos = token.find('/');
        if (pos != std::string::npos) token = token.substr(0, pos);
        vi[i] = std::stoi(token) - 1;  // OBJ indices are 1-based
      }

      // Flip winding if necessary to make sure the Y normal is positive (upward)
      const auto& a = vertices[vi[0]];
      const auto& b = vertices[vi[1]];
      const auto& c = vertices[vi[2]];

      float ux = b[0] - a[0];
      float uy = b[1] - a[1];
      float uz = b[2] - a[2];

      float vx = c[0] - a[0];
      float vy = c[1] - a[1];
      float vz = c[2] - a[2];

      float normalY = ux * vz - uz * vx;  // Y component of cross product

      if (normalY < 0.0f) {
        std::swap(vi[1], vi[2]);  // flip winding before creating surface
      }

      SM64Surface surf;
      for (int i = 0; i < 3; ++i) {
        const auto& v = vertices[vi[i]];
        surf.vertices[i][0] = static_cast<int16_t>(v[0]);
        surf.vertices[i][1] = static_cast<int16_t>(v[1]);
        surf.vertices[i][2] = static_cast<int16_t>(v[2]);
      }

      out_surfaces.push_back(surf);
    }
  }

  printf("[DEBUG] Loaded %zu surfaces from OBJ\n", out_surfaces.size());
}


void setup_from_obj_file(const char* obj_path, SM64Surface* surfaces, int max_surfaces) {
  std::vector<SM64Surface> temp;
  load_obj_to_surfaces(obj_path, temp);
  
  if (temp.size() > static_cast<size_t>(max_surfaces)) {
    printf("[WARNING] OBJ has more surfaces than max (%d), truncating...\n", max_surfaces);
  }

  int count = std::min(static_cast<int>(temp.size()), max_surfaces);
  std::memcpy(surfaces, temp.data(), sizeof(SM64Surface) * count);

  for (int i = 0; i < count; ++i) {
    printf("[DEBUG] Triangle %d:\n", i);
    for (int j = 0; j < 3; ++j) {
      printf("  Vertex %d: (%d, %d, %d)\n",
             j, surfaces[i].vertices[j][0],
             surfaces[i].vertices[j][1],
             surfaces[i].vertices[j][2]);
    }
  }
}


void setup_flat_ground(SM64Surface* surfaces) {
  const int half_size = 5000;
  const int y_offset = -100;

  printf("[DEBUG] Setting up flat ground at y = %d with size %d\n", y_offset, half_size * 2);

  // Triangle 1 - now CCW
  surfaces[0].vertices[0][0] = -half_size;
  surfaces[0].vertices[0][1] = y_offset;
  surfaces[0].vertices[0][2] = -half_size;
  surfaces[0].vertices[1][0] = half_size;
  surfaces[0].vertices[1][1] = y_offset;
  surfaces[0].vertices[1][2] = half_size;
  surfaces[0].vertices[2][0] = half_size;
  surfaces[0].vertices[2][1] = y_offset;
  surfaces[0].vertices[2][2] = -half_size;

  // Triangle 2 - also CCW
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
      printf("  Vertex %d: (%d, %d, %d)\n", j, surfaces[i].vertices[j][0],
             surfaces[i].vertices[j][1], surfaces[i].vertices[j][2]);
    }
  }
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

  //audio_thread_init();
  //sm64_play_sound_global(SOUND_MENU_STAR_SOUND);

  delete[] romBuffer;

  // //SM64MarioGeometryBuffers geom = {};
  // SM64MarioInputs m_mario_inputs = {
  //   .camLookX = 0.0f,
  //   .camLookZ = 1.0f,   // ✅ required for orientation
  //   .stickX = 0,
  //   .stickY = 0,
  //   .buttonA = 0,
  //   .buttonB = 0,
  //   .buttonZ = 0
  // };
  


                                    printf("[DEBUG] marioId = %d\n", marioId);
                                    SM64MarioGeometryBuffers geom;
const int maxTris = SM64_GEO_MAX_TRIANGLES;

geom.position = new float[3 * 3 * maxTris];  // 3 coords per vertex, 3 verts per tri
geom.normal   = new float[3 * 3 * maxTris];
geom.color    = new float[3 * 3 * maxTris];
geom.uv       = new float[2 * 3 * maxTris];  // 2 UVs per vertex, 3 verts per tri
geom.numTrianglesUsed = 0;


  // SM64Surface surfaces[2];
  // setup_flat_ground(surfaces);

  // printf("[DEBUG] Loading static surfaces...\n");
  // sm64_static_surfaces_load(surfaces, 2);
  // printf("[DEBUG] Surfaces loaded.\n");

  SM64Surface surfaces[256];  // Up to 256 triangles
  setup_from_obj_file("fart.obj.obj", surfaces, 256);
  std::vector<SM64Surface> surface_vec(surfaces, surfaces + 256);
  save_surfaces_as_obj(surface_vec, "exported_debug.obj");
  sm64_static_surfaces_load(surfaces, 256);

  // Perform floor check at Mario spawn point
  SM64SurfaceCollisionData* floorTest = nullptr;
  float floorY = sm64_surface_find_floor(0.0f, 1.0f, 0.0f, &floorTest);

  printf("[DEBUG] Floor check at (0,1,0): floorY = %f, valid = %d\n", floorY, floorTest != nullptr);

  if (!floorTest) {
    printf("[ERROR] No valid floor found. Mario will likely fall through!\n");
  } else if (floorY < -10000 || floorY > 10000) {
    printf("[WARNING] FloorY = %f is suspicious. Geometry may be incorrect.\n", floorY);
  }

  printf("[DEBUG] Attempting to create Mario at (0,1,0)...\n");
  marioId = sm64_mario_create(125.0f, 125.0f, 125.0f);
  printf("[DEBUG] Mario ID returned: %d\n", marioId);

  // sm64_audio_init(romBuffer);
  //         sm64_set_sound_volume(0.5f);

  //         sm64_play_sound_global(SOUND_ARG_LOAD(7, 0, 0x1E, 0xFF, 8));
  g_mario_state = {};

  while (MasterExit == RuntimeExitStatus::RUNNING) {
    if (marioId == 0) {
      static float last_position[3] = {999999, 999999, 999999};  // Init to dummy impossible state


     //geom.triangleIndices = new uint16_t[3 * SM64_GEO_MAX_TRIANGLES];
                                  
    // printf("Inputs [%p] - Stick: (%.2f, %.2f)  CamLook: (%.2f, %.2f)  A:%d B:%d Z:%d\n",
    //   (void*)&m_mario_inputs,
    //   m_mario_inputs.stickX, m_mario_inputs.stickY,
    //   m_mario_inputs.camLookX, m_mario_inputs.camLookZ,
    //   m_mario_inputs.buttonA, m_mario_inputs.buttonB, m_mario_inputs.buttonZ);

       //m_mario_inputs.stickY = 32; // simulate forward stick
      sm64_mario_tick(marioId, &m_mario_inputs, &g_mario_state, &geom);


      bool changed = false;
      for (int i = 0; i < 3; ++i) {
        if (g_mario_state.position[i] != last_position[i]) {
          changed = true;
          break;
        }
      }
  
      if (changed) {
        printf("Mario position changed: X=%.2f Y=%.2f Z=%.2f\n",
               g_mario_state.position[0], g_mario_state.position[1], g_mario_state.position[2]);
  
        // Update last_position
        for (int i = 0; i < 3; ++i) {
          last_position[i] = g_mario_state.position[i];
        }
      }

    }

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
  //printf("[pc_get_mario_x] X = %.2f -> 0x%lx\n", x, out);
  return out;
}

uint64_t pc_get_mario_y() {
  float y = g_mario_state.position[1];
  uint64_t out = 0;
  std::memcpy(&out, &y, sizeof(float));
  //printf("[pc_get_mario_y] Y = %.2f -> 0x%lx\n", y, out);
  return out;
}

uint64_t pc_get_mario_z() {
  float z = g_mario_state.position[2];
  uint64_t out = 0;
  std::memcpy(&out, &z, sizeof(float));
 // printf("[pc_get_mario_z] Z = %.2f -> 0x%lx\n", z, out);
  return out;
}
