#include "Mario1.h"

#include <cstdint>

typedef uint32_t u32;
#include <fstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// #include "game/graphics/opengl_renderer/MarioRenderer.h"
#include "game/graphics/opengl_renderer/MarioRenderer.h"

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
  const char* romPath = "baserom.us.z64";
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
  //  sm64_play_music(0, 0x05 | 0x80, 0);
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
// Static variable to store the previous state of the punch button
static bool prev_punch_button_state = false;
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

    bool current_punch_button_state = g_mario_inputs.buttonB;
    // TODO make this only run on press instead of hold, if the punch button is pushed
    if (current_punch_button_state && !prev_punch_button_state) {
      if (marioId != -1) {  // Still check marioId before spawning a cube
        MarioRenderer::spawn_cube_under_mario(g_mario_state.position);
      }
    }

    prev_punch_button_state = current_punch_button_state;
    //  printf("[Mario Pos] X = %.2f, Y = %.2f, Z = %.2f\n",
    //      g_mario_state.position[0],
    //      g_mario_state.position[1],
    //      g_mario_state.position[2]);

    maybe_reload_surfaces(g_mario_state.position);  // Add back with dynamic collide update
    frame_num = 0;
  }

  frame_num++;
}

// Mario functions we call in GOAL

// Mario functions we call in GOAL
uint64_t pc_get_mario_action() {
  return static_cast<uint64_t>(g_mario_state.action);
}

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

// Gross collide stuff just stop reading
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
      {1.0f / 3, 1.0f / 3}, {0.6f, 0.2f},  {0.2f, 0.6f}, {0.2f, 0.2f},
      {0.5f, 0.25f},        {0.25f, 0.5f}, {0.4f, 0.4f},
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

SM64Surface* g_combined_surfaces = nullptr;
int g_combined_surfaces_count = 0;

void load_combined_static_surfaces(const SM64Surface* surfaces1,
                                   int count1,
                                   const SM64Surface* surfaces2,
                                   int count2) {
  int total_count = count1 + count2;

  if (g_combined_surfaces) {
    free(g_combined_surfaces);
    g_combined_surfaces = nullptr;
    g_combined_surfaces_count = 0;
  }

  g_combined_surfaces = (SM64Surface*)malloc(sizeof(SM64Surface) * total_count);

  if (!g_combined_surfaces) {
    fprintf(stderr, "Failed to allocate memory for combined surfaces\n");
    return;
  }

  int offset = 0;

  if (surfaces1 && count1 > 0) {
    memcpy(g_combined_surfaces, surfaces1, sizeof(SM64Surface) * count1);
    offset += count1;
  }

  if (surfaces2 && count2 > 0) {
    memcpy(g_combined_surfaces + offset, surfaces2, sizeof(SM64Surface) * count2);
  }

  g_combined_surfaces_count = total_count;

  sm64_static_surfaces_load(g_combined_surfaces, g_combined_surfaces_count);

  if (marioId == -1) {
    marioId = sm64_mario_create(-7541.8, 1688.475, 9237.5);
    for (int i = 0; i < 10; ++i) {
      printf("marioId = %d\n", marioId);
    }
  }
}

float triangle_area_2d(const int32_t v[3][3]) {
  float x0 = to_f32(v[0][0]), z0 = to_f32(v[0][2]);
  float x1 = to_f32(v[1][0]), z1 = to_f32(v[1][2]);
  float x2 = to_f32(v[2][0]), z2 = to_f32(v[2][2]);
  return 0.5f * fabsf((x1 - x0) * (z2 - z0) - (x2 - x0) * (z1 - z0));
}

bool triangle_intersects_cylinder_recursive(float cx,
                                            float cz,
                                            const int32_t v[3][3],
                                            int depth = 0) {
  if (depth > 3) {
    return triangle_samples_in_cylinder(cx, cz, v);
  }

  float area = triangle_area_2d(v);
  const float cylinder_area = M_PI * CYLINDER_RADIUS_SQ;

  if (area < cylinder_area * 4) {
    return triangle_samples_in_cylinder(cx, cz, v);
  }

  // Compute midpoints
  int32_t m01[3], m12[3], m20[3];
  for (int i = 0; i < 3; ++i) {
    m01[i] = (v[0][i] + v[1][i]) / 2;
    m12[i] = (v[1][i] + v[2][i]) / 2;
    m20[i] = (v[2][i] + v[0][i]) / 2;
  }

  // Subdivide into 4 triangles and test recursively
  int32_t sub1[3][3] = {
      {v[0][0], v[0][1], v[0][2]}, {m01[0], m01[1], m01[2]}, {m20[0], m20[1], m20[2]}};
  int32_t sub2[3][3] = {
      {m01[0], m01[1], m01[2]}, {v[1][0], v[1][1], v[1][2]}, {m12[0], m12[1], m12[2]}};
  int32_t sub3[3][3] = {
      {m20[0], m20[1], m20[2]}, {m12[0], m12[1], m12[2]}, {v[2][0], v[2][1], v[2][2]}};
  int32_t sub4[3][3] = {
      {m01[0], m01[1], m01[2]}, {m12[0], m12[1], m12[2]}, {m20[0], m20[1], m20[2]}};

  return triangle_intersects_cylinder_recursive(cx, cz, sub1, depth + 1) ||
         triangle_intersects_cylinder_recursive(cx, cz, sub2, depth + 1) ||
         triangle_intersects_cylinder_recursive(cx, cz, sub3, depth + 1) ||
         triangle_intersects_cylinder_recursive(cx, cz, sub4, depth + 1);
}

int load_surfaces_near(float x, float y, float z) {
  if (!g_combined_surfaces || g_combined_surfaces_count == 0)
    return 0;

  SM64Surface* filtered = (SM64Surface*)malloc(sizeof(SM64Surface) * g_combined_surfaces_count);
  if (!filtered)
    return 0;

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
    if (!include && triangle_intersects_cylinder_recursive(x, z, s->vertices)) {
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

struct SurfaceEntry {
  const SM64Surface* surfaces;
  int count;
};

#include <unordered_map>
void pc_call_load_combined_static_surfaces_from_game_idx(u32 x_bits, u32 z_bits) {
  float x, z;
  memcpy(&x, &x_bits, sizeof(float));
  memcpy(&z, &z_bits, sizeof(float));

  printf("[DEBUG] Called with x = %.2f, z = %.2f\n", x, z);

  // Round to int for matching
  int x_key = static_cast<int>(roundf(x));
  int z_key = static_cast<int>(roundf(z));

  static const std::unordered_map<int, SurfaceEntry> surface_map = {
      {1, {training_surfaces, training_surfaces_count}},
      {2, {village1_surfaces, village1_surfaces_count}},
      {3, {beach_surfaces, beach_surfaces_count}},
      {4, {jungle_surfaces, jungle_surfaces_count}},
      {5, {jungleb_surfaces, jungleb_surfaces_count}},
      {6, {misty_surfaces, misty_surfaces_count}},
      {7, {firecanyon_surfaces, firecanyon_surfaces_count}},
      {8, {village2_surfaces, village2_surfaces_count}},
      {9, {sunken_surfaces, sunken_surfaces_count}},
      {10, {sunkenb_surfaces, sunkenb_surfaces_count}},
      {11, {swamp_surfaces, swamp_surfaces_count}},
      {12, {rolling_surfaces, rolling_surfaces_count}},
      {13, {ogre_surfaces, ogre_surfaces_count}},
      {14, {village3_surfaces, village3_surfaces_count}},
      {15, {snow_surfaces, snow_surfaces_count}},
      {16, {maincave_surfaces, maincave_surfaces_count}},
      {17, {darkcave_surfaces, darkcave_surfaces_count}},
      {18, {robocave_surfaces, robocave_surfaces_count}},
      {19, {lavatube_surfaces, lavatube_surfaces_count}},
      {20, {citadel_surfaces, citadel_surfaces_count}},
      {21, {finalboss_surfaces, finalboss_surfaces_count}},
  };
  const SM64Surface* surfaces1 = nullptr;
  const SM64Surface* surfaces2 = nullptr;
  int count1 = 0;
  int count2 = 0;

  auto it_x = surface_map.find(x_key);
  if (it_x != surface_map.end()) {
    surfaces1 = it_x->second.surfaces;
    count1 = it_x->second.count;
    printf("[DEBUG] x matched map key %d\n", x_key);
  }

  auto it_z = surface_map.find(z_key);
  if (it_z != surface_map.end()) {
    surfaces2 = it_z->second.surfaces;
    count2 = it_z->second.count;
    printf("[DEBUG] z matched map key %d\n", z_key);
  }

  load_combined_static_surfaces(surfaces1, count1, surfaces2, count2);
}
void maybe_reload_surfaces(const float* mario_pos) {
  float dx = mario_pos[0] - g_cylinder_center[0];
  float dz = mario_pos[2] - g_cylinder_center[2];
  float dist_sq = dx * dx + dz * dz;

  float reload_threshold_sq =
      (CYLINDER_RADIUS - CYLINDER_BUFFER) * (CYLINDER_RADIUS - CYLINDER_BUFFER);
  if (dist_sq > reload_threshold_sq) {
    g_cylinder_center[0] = mario_pos[0];
    g_cylinder_center[1] = mario_pos[1];
    g_cylinder_center[2] = mario_pos[2];

    load_surfaces_near(g_cylinder_center[0], g_cylinder_center[1], g_cylinder_center[2]);
  }
}