#include "Mario1.h"

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

  // Create Mario and print his ID
  marioId = sm64_mario_create(-7541.8, 1688.475, 9237.5);
  for (int i = 0; i < 10; ++i) {
    printf("marioId = %d\n", marioId);
  }

  return marioId;
}

static int frame_num = 0;
void tick_mario_frame() {
  // Only store raw values in g_mario_inputs
  // Then scale into local variables
  float scaled_stick_x = g_mario_inputs.stickX / 64.0f;
  float scaled_stick_y = g_mario_inputs.stickY / 64.0f;

  if (frame_num % 2 == 0 && marioId != -1) {
    SM64MarioInputs inputs = g_mario_inputs;
    inputs.stickX = scaled_stick_x;
    inputs.stickY = -scaled_stick_y;

    sm64_mario_tick(marioId, &inputs, &g_mario_state, &g_geom);
    //maybe_reload_surfaces(g_mario_state.position);  // Ensure this is declared somewhere
    sm64_static_surfaces_load(village1_surfaces, village1_surfaces_count);
    marioId = sm64_mario_create(-7541.8, 1688.475, 9237.5);
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