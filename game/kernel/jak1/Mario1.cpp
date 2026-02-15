// Mario1.cpp
#include "Mario1.h"

// Refactored Mario1.cpp
#include "Mario1.h"
// #include "Mario_collide.h"
// #include "Mario_collide2.h"
// #include "game/graphics/opengl_renderer/MarioRenderer.h"
#include "common/util/FileUtil.h"
#include "game/kernel/common/kmachine.h"
#include "game/kernel/common/kscheme.h"
//#include "load_surfaces.h"  // contains all your level surface arrays

#include "kscheme.h"

#include <cstring>   // memcpy, memset
#include <cmath>     // roundf
#include <cstdio>    // printf
#include <chrono>

static std::chrono::steady_clock::time_point g_last_tick_time;
static double g_tick_accumulator = 0.0;

// change this to slow/speed Mario
constexpr double MARIO_FIXED_DT = 1.0 / 30.0; // 30hz mario

// ─────────────────────────────────────────────────────────────────────────────
// Globals (cylinder + combined surfaces) - Shared across all Marios
// ─────────────────────────────────────────────────────────────────────────────

static float g_cylinder_center[3] = {0.0f, 0.0f, 0.0f};
constexpr float CYLINDER_RADIUS = 8000.0f;
constexpr float CYLINDER_BUFFER = 2000.0f;
constexpr float CYLINDER_RADIUS_SQ = CYLINDER_RADIUS * CYLINDER_RADIUS;

static SM64Surface* g_combined_surfaces = nullptr;
static int g_combined_surfaces_count = 0;

// For dynamic actor surfaces (stubbed for now)
std::vector<SM64SurfaceObject> g_active_debug_objects;

// ─────────────────────────────────────────────────────────────────────────────
// MarioManager Implementation
// ─────────────────────────────────────────────────────────────────────────────

MarioManager* MarioManager::sInstance = nullptr;
uint8_t* MarioManager::s_shared_texture = nullptr;

MarioManager::MarioManager() {
  // Shared texture allocation moved to Initialize
}

MarioManager::~MarioManager() {
  // Cleanup per-Mario in loop
  for (auto& pair : m_marios) {
    auto& instance = *pair.second;
    delete[] instance.geom.position;
    delete[] instance.geom.normal;
    delete[] instance.geom.color;
    delete[] instance.geom.uv;
  }
  m_marios.clear();

  delete[] s_shared_texture;
  s_shared_texture = nullptr;

  if (g_combined_surfaces) {
    delete[] g_combined_surfaces;
    g_combined_surfaces = nullptr;
    g_combined_surfaces_count = 0;
  }
}
bool g_mario_enabled = false;
MarioManager& MarioManager::Get() {
  // ─────────────────────────────────────────────────────────────
  // Early-out: Mario is disabled until the global flag is true
  // ─────────────────────────────────────────────────────────────
  if (!g_mario_enabled) {
    // Return a static dummy object that does absolutely nothing.a
    // All calls to MarioManager::Get().xxx will be no-ops until enabled.
    static MarioManager dummy;
    return dummy;
  }

  // Normal path — initialize on first real use
  if (!sInstance) {
    Initialize();
  }
  return *sInstance;
}

void MarioManager::Initialize() {
  if (sInstance) return;
  sInstance = new MarioManager();
  fprintf(stderr, "[libsm64] STARTING NEW MANAGER FOR MULTIPLE MARIOS");
  std::string baseDir = file_util::get_file_path({"iso_data"});
  std::string romPathStr = baseDir + "/mario/test.rom";
  const char* romPath = romPathStr.c_str();

  uint8_t* romBuffer = nullptr;
  std::ifstream file(romPath, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    fprintf(stderr, "[libsm64] Failed to open ROM file: %s\n", romPath);
    std::abort();
  }

  size_t romFileLength = file.tellg();
  romBuffer = new uint8_t[romFileLength + 1];
  file.seekg(0);
  file.read(reinterpret_cast<char*>(romBuffer), romFileLength);

  
  romBuffer[romFileLength] = 0;

  // Shared texture
  
  s_shared_texture = new uint8_t[4 * SM64_TEXTURE_WIDTH * SM64_TEXTURE_HEIGHT];
  fprintf(stderr, "going in");
  sm64_global_init(romBuffer, s_shared_texture);
  fprintf(stderr, "goin out");

  // Debug: load village1 (shared)
  sm64_static_surfaces_load(village1_surfaces, village1_surfaces_count);

  sm64_audio_init(romBuffer);
  if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    fprintf(stderr, "[libsm64][ERROR] SDL_Init failed: %s\n", SDL_GetError());
  }
  //audio_init();

  delete[] romBuffer;

  // Set initial cylinder center (can be updated per-Mario later if needed)
  g_cylinder_center[0] = 0.0f;  // Default; will be set on first Mario creation
  g_cylinder_center[1] = 0.0f;
  g_cylinder_center[2] = 0.0f;
  printf("[MIO0] Decoded  bytes...\n");
}

int MarioManager::CreateMario(float x, float y, float z) {
    printf("[libsm64] CreateMario() called with pos (%.2f, %.2f, %.2f)\n", x, y, z);

    auto& self = Get();


    int id = sm64_mario_create(x, y + 5.0f , z);

    if (id == -1) {
        fprintf(stderr, "[libsm64] Failed to create Mario at (%.1f, %.1f, %.1f)\n", x, y, z);
        return -1;
    }

    // Success print
    printf("[libsm64] Successfully created Mario! ID = %d at (%.2f, %.2f, %.2f)\n", 
           id, x, y, z);

  auto instance = std::make_unique<MarioInstance>();
  instance->id = id;
  instance->active = true;

  const int maxTris = SM64_GEO_MAX_TRIANGLES;
  instance->geom.position = new float[3 * 3 * maxTris];
  instance->geom.normal   = new float[3 * 3 * maxTris];
  instance->geom.color    = new float[3 * 3 * maxTris];
  instance->geom.uv       = new float[2 * 3 * maxTris];

  memset(instance->geom.position, 0, sizeof(float) * 3 * 3 * maxTris);
  memset(instance->geom.normal,   0, sizeof(float) * 3 * 3 * maxTris);
  memset(instance->geom.color,    0, sizeof(float) * 3 * 3 * maxTris);
  memset(instance->geom.uv,       0, sizeof(float) * 2 * 3 * maxTris);
  instance->geom.numTrianglesUsed = 0;

  self.m_marios[id] = std::move(instance);

  // Update cylinder center to this Mario's position if first
  if (self.m_marios.size() == 1) {
    g_cylinder_center[0] = x;
    g_cylinder_center[1] = y;
    g_cylinder_center[2] = z;
  }

  fprintf(stderr, "[libsm64] Created Mario %d at (%.1f, %.1f, %.1f)\n", id, x, y, z);
  return id;
}

void MarioManager::DestroyMario(int id) {
  auto& self = Get();
  auto it = self.m_marios.find(id);
  if (it != self.m_marios.end()) {
    sm64_mario_delete(id);  // Assuming libsm64 has this; otherwise, just mark inactive
    auto& instance = *it->second;
    delete[] instance.geom.position;
    delete[] instance.geom.normal;
    delete[] instance.geom.color;
    delete[] instance.geom.uv;
    self.m_marios.erase(it);
    fprintf(stderr, "[libsm64] Destroyed Mario %d\n", id);
  }
}

MarioInstance* MarioManager::GetMario(int id) {
  auto it = m_marios.find(id);
  return (it != m_marios.end()) ? it->second.get() : nullptr;
}

const MarioInstance* MarioManager::GetMario(int id) const {
  auto it = m_marios.find(id);
  return (it != m_marios.end()) ? it->second.get() : nullptr;
}

float MarioManager::GetMarioX(int id) const {
  const auto* inst = GetMario(id);
  return inst ? inst->state.position[0] : 0.0f;
}

float MarioManager::GetMarioY(int id) const {
  const auto* inst = GetMario(id);
  return inst ? inst->state.position[1] : 0.0f;
}

float MarioManager::GetMarioZ(int id) const {
  const auto* inst = GetMario(id);
  return inst ? inst->state.position[2] : 0.0f;
}

uint32_t MarioManager::GetMarioAction(int id) const {
  const auto* inst = GetMario(id);
  return inst ? inst->state.action : 0;
}

SM64MarioGeometryBuffers& MarioManager::GetMarioGeom(int id) {
  auto* inst = GetMario(id);
  if (inst) return inst->geom;
  static SM64MarioGeometryBuffers dummy;  // Fallback
  return dummy;
}

const SM64MarioGeometryBuffers& MarioManager::GetMarioGeom(int id) const {
  const auto* inst = GetMario(id);
  if (inst) return inst->geom;
  //static const SM64MarioGeometryBuffers dummy;  // Fallback
  //return dummy;
}

SM64MarioInputs& MarioManager::GetMarioInputs(int id) {
  auto* inst = GetMario(id);
  if (inst) return inst->inputs;
  static SM64MarioInputs dummy;  // Fallback
  return dummy;
}

const SM64MarioInputs& MarioManager::GetMarioInputs(int id) const {
  const auto* inst = GetMario(id);
  if (inst) return inst->inputs;
  //static const SM64MarioInputs dummy;  // Fallback
  //return dummy;
}

void MarioManager::SetMarioCamera(int id, float x, float z) {
  auto& inputs = GetMarioInputs(id);
  inputs.camLookX = x;
  inputs.camLookZ = z;
}

void MarioManager::SetMarioMusic(int id, uint32_t music_bits) {
  // Assuming music is global or per-Mario; stub for now
  // sm64_set_music(music_bits);  // If libsm64 supports
  (void)id;  // Unused if global
}

void MarioManager::SetMarioPosition(int id, float x, float y, float z) {
  auto& state = GetMario(id)->state;
  state.position[0] = x;
  state.position[1] = y;
  state.position[2] = z;
  //sm64_mario_set_position(id, x, y, z);  // If libsm64 has setter
}

void MarioManager::SetMarioWaterLevel(int id, float level) {
  sm64_set_mario_water_level(id,
                             true ?  //  hardcode this func to always accept true bc we check on the
                                     //  goal side and only call this func if close to water
                                 level
                                  : INT16_MIN);
}

void MarioManager::ChangeMarioState(int id, uint32_t act) {
  auto& state = GetMario(id)->state;
  state.action = act;
}

void MarioManager::HealMario(int id) {
  // Stub: heal logic
  (void)id;
}

void MarioManager::DamageMario(int id) {
  auto* inst = GetMario(id);
  if (inst) {
    sm64_mario_take_damage(inst->id, 1, 0xFF, inst->state.position[0], inst->state.position[1], inst->state.position[2]);
  }
}

void MarioManager::Tick() {
  auto& self = Get();
  using clock = std::chrono::steady_clock;
  auto now = clock::now();

  if (g_last_tick_time.time_since_epoch().count() == 0) {
    g_last_tick_time = now;
    return;
  }

  double frame_dt = std::chrono::duration<double>(now - g_last_tick_time).count();
  g_last_tick_time = now;
  g_tick_accumulator += frame_dt; 

  // Assuming inputs already contain raw values from GOAL (per-Mario)

  while (g_tick_accumulator >= MARIO_FIXED_DT) {
    //jak1::call_goal_function_by_name("update-sm64-camera-from-goal");  // Global? Adjust if per-Mario

    // Tick all active Marios
    for (auto& pair : self.m_marios) {
      if (pair.second->active) {
        jak1::call_goal_function_by_name("update-sm64-camera-from-goal");
        self.TickMario(pair.first);
      }
    }

    // self.UpdateCollideGlobal();  // Shared dynamic updates
    // self.MaybeReloadSurfacesGlobal();

    g_tick_accumulator -= MARIO_FIXED_DT;
  }
}

void MarioManager::TickMario(int id) {
  auto* inst = GetMario(id);
  if (!inst || !inst->active) return;

  jak1::call_goal_function_by_name("update-mario-water-height-from-goal");  // Per-Mario? Stub

  sm64_mario_tick(inst->id, &inst->inputs, &inst->state, &inst->geom);

//   UpdateCollide(id);
//   MaybeReloadSurfaces(id);
}

void MarioManager::UpdateCollide(int id) {
  // Dynamic object updates (stub per-Mario)
  // Main collision is in sm64_mario_tick
  (void)id;
}

void MarioManager::UpdateCollideGlobal() {
  // Shared dynamic updates (e.g., active_debug_objects)
  // for (auto& obj : g_active_debug_objects) {
  //   sm64_surface_object_update(obj.id, ...);
  // }
}

void MarioManager::UpdatePlatformInfo(u32 platform_info_ptr) {
  if (platform_info_ptr == 0) {
    m_platform_info_valid = false;
    return;
  }

  const PlatformInfo* info = reinterpret_cast<const PlatformInfo*>(platform_info_ptr);
  m_platform_info = *info;
  m_platform_info_valid = true;
}

void MarioManager::UpdateMovingPlatform() {
  if (!m_platform_info_valid) return;

  // Example: convert u32 bits → float (shared for now)
  float x = *reinterpret_cast<const float*>(&m_platform_info.x_pos);
  float y = *reinterpret_cast<const float*>(&m_platform_info.y_pos);
  float z = *reinterpret_cast<const float*>(&m_platform_info.z_pos);

  // TODO: apply to shared SM64 moving platform objects
}

void MarioManager::UpdatePseudoFloor(int id) {
  // Optional: flat floor under this Mario for safety
  // sm64_static_surfaces_load(psuedo_floor_surfaces, 2);
  (void)id;
}

void MarioManager::MaybeReloadSurfaces(int id) {
  const auto* inst = GetMario(id);
  if (!inst) return;

  float dx = inst->state.position[0] - g_cylinder_center[0];
  float dz = inst->state.position[2] - g_cylinder_center[2];
  float dist_sq = dx * dx + dz * dz;

  float reload_threshold_sq = (CYLINDER_RADIUS - CYLINDER_BUFFER) * (CYLINDER_RADIUS - CYLINDER_BUFFER);
  if (dist_sq > reload_threshold_sq) {
    // Update global center or per-Mario? For simplicity, update global to this Mario
    g_cylinder_center[0] = inst->state.position[0];
    g_cylinder_center[1] = inst->state.position[1];
    g_cylinder_center[2] = inst->state.position[2];

    //load_surfaces_near(g_cylinder_center[0], g_cylinder_center[1], g_cylinder_center[2]);
  }
}

void MarioManager::MaybeReloadSurfacesGlobal() {
  // If multiple Marios, could average positions or use a union cylinder
  // For now, stub - per-Mario handles it
}

void MarioManager::Shutdown() {
  if (!sInstance) return;
  sm64_global_terminate();
  delete sInstance;
  sInstance = nullptr;
}

std::vector<int> MarioManager::GetActiveMarioIds() const {
  std::vector<int> ids;
  for (const auto& pair : m_marios) {
    if (pair.second->active) ids.push_back(pair.first);
  }
  return ids;
}

// ─────────────────────────────────────────────────────────────────────────────
// Surface loading helpers (shared)
// ─────────────────────────────────────────────────────────────────────────────

void load_combined_static_surfaces(const SM64Surface* s1, int c1, const SM64Surface* s2, int c2) {
  int total = c1 + c2;
  if (total == 0) return;

  if (g_combined_surfaces) {
    delete[] g_combined_surfaces;
  }

  g_combined_surfaces = new SM64Surface[total];
  g_combined_surfaces_count = total;

  if (s1 && c1 > 0) memcpy(g_combined_surfaces, s1, sizeof(SM64Surface) * c1);
  if (s2 && c2 > 0) memcpy(g_combined_surfaces + c1, s2, sizeof(SM64Surface) * c2);

  sm64_static_surfaces_load(g_combined_surfaces, total);
}

int load_surfaces_near(float x, float y, float z) {
  if (g_combined_surfaces_count == 0) return 0;

  SM64Surface* filtered = new SM64Surface[g_combined_surfaces_count];
  int count = 0;

  for (int i = 0; i < g_combined_surfaces_count; ++i) {
    const SM64Surface& s = g_combined_surfaces[i];
    bool include = false;

    for (int v = 0; v < 3; ++v) {
      float dx = static_cast<float>(s.vertices[v][0]) - x;
      float dz = static_cast<float>(s.vertices[v][2]) - z;
      if (dx * dx + dz * dz <= CYLINDER_RADIUS_SQ) {
        include = true;
        break;
      }
    }

    if (include) filtered[count++] = s;
  }

  sm64_static_surfaces_load(filtered, count);
  delete[] filtered;

  printf("[libsm64] Reloaded %d surfaces near (%.1f, %.1f, %.1f)\n", count, x, y, z);
  return count;
}

// ─────────────────────────────────────────────────────────────────────────────
// pc_ wrapper functions (updated for ID)
// ─────────────────────────────────────────────────────────────────────────────

uint64_t pc_get_mario_x(int id)       { float v = MarioManager::Get().GetMarioX(id);       uint64_t b; memcpy(&b, &v, 4); return b; }
uint64_t pc_get_mario_y(int id)       { float v = MarioManager::Get().GetMarioY(id);       uint64_t b; memcpy(&b, &v, 4); return b; }
uint64_t pc_get_mario_z(int id)       { float v = MarioManager::Get().GetMarioZ(id);       uint64_t b; memcpy(&b, &v, 4); return b; }
uint64_t pc_get_mario_action(int id)  { return MarioManager::Get().GetMarioAction(id); }

void pc_set_mario_camera(uint32_t x, uint32_t z) {
    float fx, fz;
    memcpy(&fx, &x, 4);
    memcpy(&fz, &z, 4);
    MarioManager::Get().SetMarioCamera(0, fx, fz);
}

void pc_set_mario_music_from_goal(int id, uint32_t music_bits) {
  MarioManager::Get().SetMarioMusic(id, music_bits);
}

void pc_set_mario_position_from_goal(int id, uint32_t x, uint32_t y, uint32_t z) {
  float fx, fy, fz;
  memcpy(&fx, &x, 4);
  memcpy(&fy, &y, 4);
  memcpy(&fz, &z, 4);
  fx *= METERS_TO_UNITS;
  fy *= METERS_TO_UNITS;
  fz *= METERS_TO_UNITS;
  MarioManager::Get().SetMarioPosition(id, fx, fy, fz);
}

void pc_set_mario_water_level_from_goal(int id, uint32_t level_bits) {
  float f;
  memcpy(&f, &level_bits, 4);
  f *= METERS_TO_UNITS;
  MarioManager::Get().SetMarioWaterLevel(id, f);
}

void pc_change_mario_state(int id, uint32_t act_bits) {
  MarioManager::Get().ChangeMarioState(id, act_bits);
}

void update_platform_info_from_goal(u32 ptr) {
  MarioManager::Get().UpdatePlatformInfo(ptr);
}

void update_moving_platform() {
  MarioManager::Get().UpdateMovingPlatform();
}

void update_psuedo_floor_under_mario(int id) {
  MarioManager::Get().UpdatePseudoFloor(id);
}

void pc_heal_mario(int id)   { MarioManager::Get().HealMario(id); }
void pc_damage_mario(int id) { MarioManager::Get().DamageMario(id); }

void pc_call_load_combined_static_surfaces_from_game_idx(uint32_t x_bits, uint32_t z_bits) {
  float x, z;
  memcpy(&x, &x_bits, 4);
  memcpy(&z, &z_bits, 4);

  int x_key = static_cast<int>(roundf(x));
  int z_key = static_cast<int>(roundf(z));

  struct SurfaceEntry {
    const SM64Surface* surfaces;
    int count;
  };

  static const std::unordered_map<int, SurfaceEntry> surface_map = {
    // {1,  {training_surfaces,   training_surfaces_count}},
    // {2,  {village1_surfaces,   village1_surfaces_count}},
    // {3,  {beach_surfaces,      beach_surfaces_count}},
    // {4,  {jungle_surfaces,     jungle_surfaces_count}},
    // {5,  {jungleb_surfaces,    jungleb_surfaces_count}},
    // {6,  {misty_surfaces,      misty_surfaces_count}},
    // {7,  {firecanyon_surfaces, firecanyon_surfaces_count}},
    // {8,  {village2_surfaces,   village2_surfaces_count}},
    // {9,  {sunken_surfaces,     sunken_surfaces_count}},
    // {10, {sunkenb_surfaces,    sunkenb_surfaces_count}},
    // {11, {swamp_surfaces,      swamp_surfaces_count}},
    // {12, {rolling_surfaces,    rolling_surfaces_count}},
    // {13, {ogre_surfaces,       ogre_surfaces_count}},
    // {14, {village3_surfaces,   village3_surfaces_count}},
    // {15, {snow_surfaces,       snow_surfaces_count}},
    // {16, {maincave_surfaces,   maincave_surfaces_count}},
    // {17, {darkcave_surfaces,   darkcave_surfaces_count}},
    // {18, {robocave_surfaces,   robocave_surfaces_count}},
    // {19, {lavatube_surfaces,   lavatube_surfaces_count}},
    // {20, {citadel_surfaces,    citadel_surfaces_count}},
    // {21, {finalboss_surfaces,  finalboss_surfaces_count}},
  };

  const SM64Surface* s1 = nullptr; int c1 = 0;
  const SM64Surface* s2 = nullptr; int c2 = 0;

  if (auto it = surface_map.find(x_key); it != surface_map.end()) {
    s1 = it->second.surfaces;
    c1 = it->second.count;
  }
  if (auto it = surface_map.find(z_key); it != surface_map.end()) {
    s2 = it->second.surfaces;
    c2 = it->second.count;
  }

  load_combined_static_surfaces(s1, c1, s2, c2);
  MarioManager::Get().MaybeReloadSurfacesGlobal();
}

// ─────────────────────────────────────────────────────────────────────────────
// Remaining stubs (updated for multi-Mario)
// ─────────────────────────────────────────────────────────────────────────────

void pc_add_tris_to_surface(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {}
void pc_spawn_mario_test_collide(int id, uint32_t) { (void)id; }
void pc_mario_says_so_long_gay_bowsa(int id, uint32_t) { (void)id; }

bool run_and_render_mario(int id) {
  return MarioManager::Get().GetMario(id) != nullptr;
}

bool has_blue_eco()          { return false; /* TODO */ }
bool should_render_mario(int id)   { return MarioManager::Get().GetMario(id) != nullptr; }
