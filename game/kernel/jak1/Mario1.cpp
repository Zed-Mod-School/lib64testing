#include "Mario1.h"

#include <cstdint>

typedef uint32_t u32;
#include <fstream>
static uint8_t* g_mario_texture = nullptr;
int marioId = -1;
uint8_t* marioTexture;
SM64MarioState g_mario_state = {0};
SM64MarioGeometryBuffers g_geom = {0};
SM64MarioInputs g_mario_inputs = {.camLookX = 0.0f,
                                  .camLookZ = 1.0f,
                                  .stickX = 0.0f,
                                  .stickY = 0.0f,
                                  .buttonA = 0,
                                  .buttonB = 0,
                                  .buttonZ = 0};

int load_and_init_mario() {
  // Our entrypoint to setup mario, this is called once in kboot.cpp and makes sure all the mario
  // stuff is properly set up at the start
  const char* romPath = "sm64.us.z64";
  static uint8_t* romBuffer = nullptr;
  // Open ROM file
  std::ifstream file(romPath, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    fprintf(stderr, "Failed to open ROM file: %s\n", romPath);
    return -1;
  }

  // Load ROM into memory
  size_t romFileLength = file.tellg();
  romBuffer = new uint8_t[romFileLength + 1];
  file.seekg(0);
  file.read(reinterpret_cast<char*>(romBuffer), romFileLength);
  romBuffer[romFileLength] = 0;

  // Allocate texture buffer
  g_mario_texture = new uint8_t[4 * SM64_TEXTURE_WIDTH * SM64_TEXTURE_HEIGHT];

  // Initialize SM64
  sm64_global_init(romBuffer, g_mario_texture);
  // remove this calll later when we add dynamic collide loading back
  sm64_static_surfaces_load(village1_surfaces, village1_surfaces_count);
  // Create Mario and print his ID
  marioId = sm64_mario_create(-7541.8, 3688.475, 9237.5);
  sm64_audio_init(romBuffer);
  delete[] romBuffer;
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    fprintf(stderr, "[libsm64][ERROR] SDL_Init failed: %s\n", SDL_GetError());
    return -1;
  }
  audio_init();
  sm64_play_music(0, 0x05 | 0x80,
                  0);
  // for (int i = 0; i < 10; ++i) {
  //   printf("marioId = %d\n", marioId);
  // }

  // This stuff is needed to avoid a null pointer dereference for some reason
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

  // mario functions
  // g_pc_port_funcs.make_func_symbol_func("pc-get-mario-x", (void*)pc_get_mario_x);
  // g_pc_port_funcs.make_func_symbol_func("pc-get-mario-y", (void*)pc_get_mario_y);
  // g_pc_port_funcs.make_func_symbol_func("pc-get-mario-z", (void*)pc_get_mario_z);
  // g_pc_port_funcs.make_func_symbol_func("pc-set-mario-look-angles!", (void*)pc_set_mario_camera);
  // g_pc_port_funcs.make_func_symbol_func("teleport-mario-to-pos",
  // (void*)pc_set_mario_position_from_goal);
  // g_pc_port_funcs.make_func_symbol_func("pc-load-mario-collide!",
  // (void*)pc_call_load_combined_static_surfaces_from_game_idx);

  return marioId;
}

int frame_num = 0;
void tick_mario_frame() {
  // This function is called every frame and controls updating the mario engine.
  // Revist this later, probably can /64 where we set .stickX instead and remove all this junk
  float scaled_stick_x = g_mario_inputs.stickX / 64.0f;
  float scaled_stick_y = g_mario_inputs.stickY / 64.0f;

  if (frame_num % 2 == 0 && marioId != -1) {
    SM64MarioInputs inputs = g_mario_inputs;
    inputs.stickX = scaled_stick_x;
    inputs.stickY = -scaled_stick_y;

    sm64_mario_tick(marioId, &inputs, &g_mario_state, &g_geom);

    //  printf("[Mario Pos] X = %.2f, Y = %.2f, Z = %.2f\n",
    //      g_mario_state.position[0],
    //      g_mario_state.position[1],
    //      g_mario_state.position[2]);

    // maybe_reload_surfaces(g_mario_state.position); // Add back with dynamic collide update
    frame_num = 0;
  }

  frame_num++;
}

// Mario functions we call in GOAL
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

void pc_set_mario_camera(u32 x, u32 z) {
  g_mario_inputs.camLookX;

  memcpy(&g_mario_inputs.camLookX, &x, 4);

  g_mario_inputs.camLookZ;

  memcpy(&g_mario_inputs.camLookZ, &z, 4);
}

void pc_set_mario_position_from_goal(u32 x_bits, u32 y_bits, u32 z_bits) {
  float x, y, z;
  memcpy(&x, &x_bits, sizeof(u32));
  memcpy(&y, &y_bits, sizeof(u32));
  memcpy(&z, &z_bits, sizeof(u32));

  constexpr float METERS_TO_UNITS = 50.0f / 4096.0f;

  x *= METERS_TO_UNITS;
  y *= METERS_TO_UNITS;
  z *= METERS_TO_UNITS;

  sm64_set_mario_position(marioId, x, y, z);
}
