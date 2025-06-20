#include "Mario1.h"

#include <fstream>

static uint8_t* g_mario_texture = nullptr;
int marioId = -1;
uint8_t* marioTexture;

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
  int marioId = sm64_mario_create(-7541.8, 1688.475, 9237.5);
  for (int i = 0; i < 10; ++i) {
    printf("marioId = %d\n", marioId);
  }

  return marioId;
}