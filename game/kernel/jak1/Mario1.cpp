// Mario1.cpp
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
MarioManager* g_debug_mario_mgr = nullptr;

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



MarioManager* MarioManager::Get() {
  if (!g_mario_enabled) return nullptr;
  if (!sInstance) Initialize();
  return sInstance;
}

void MarioManager::Initialize() {
  if (sInstance || !g_mario_enabled) return;

  sInstance = new MarioManager();
  g_debug_mario_mgr = sInstance;
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

  s_shared_texture = new uint8_t[4 * SM64_TEXTURE_WIDTH * SM64_TEXTURE_HEIGHT];
  fprintf(stderr, "going in");
  sm64_global_init(romBuffer, s_shared_texture);
  fprintf(stderr, "goin out");

  sm64_static_surfaces_load(village1_surfaces, village1_surfaces_count);

  sm64_audio_init(romBuffer);
  if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    fprintf(stderr, "[libsm64][ERROR] SDL_Init failed: %s\n", SDL_GetError());
  }

  delete[] romBuffer;

  g_cylinder_center[0] = 0.0f;
  g_cylinder_center[1] = 0.0f;
  g_cylinder_center[2] = 0.0f;
  printf("[MIO0] Decoded bytes...\n");

  // if (sInstance) sInstance->AddTestActors();
}

int MarioManager::CreateMario(float x, float y, float z) {
  printf("[libsm64] CreateMario() called with pos (%.2f, %.2f, %.2f)\n", x, y, z);

  MarioManager* self = Get();
  if (!self) {
    fprintf(stderr, "[CreateMario] ERROR: Manager not available\n");
    return -1;
  }

  fprintf(stderr, "[CreateMario] self             = %p   (MarioManager* this)\n", (void*)self);
  fprintf(stderr, "[CreateMario] &self->m_marios = %p   (unordered_map* member)\n", (void*)&self->m_marios);
  fprintf(stderr, "[CreateMario] map size before insert = %zu\n", self->m_marios.size());

  int id = sm64_mario_create(x, y + 5.0f, z);

  if (id == -1) {
    fprintf(stderr, "[libsm64] Failed to create Mario at (%.1f, %.1f, %.1f)\n", x, y, z);
    return -1;
  }

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

  fprintf(stderr, "[CreateMario] About to insert ID %d into map\n", id);

  self->m_marios[id] = std::move(instance);

  fprintf(stderr, "[CreateMario] AFTER insert:\n");
  fprintf(stderr, "  MarioManager*     = %p\n", (void*)self);
  fprintf(stderr, "  &m_marios         = %p\n", (void*)&self->m_marios);
  fprintf(stderr, "  map size          = %zu\n", self->m_marios.size());
  fprintf(stderr, "  contains ID %d    = %s\n", 
          id, 
          self->m_marios.find(id) != self->m_marios.end() ? "YES" : "NO");

  if (self->m_marios.size() == 1) {
    g_cylinder_center[0] = x;
    g_cylinder_center[1] = y;
    g_cylinder_center[2] = z;
    fprintf(stderr, "[CreateMario] First Mario -> cylinder center set to (%.1f, %.1f, %.1f)\n", x, y, z);
  }

  fprintf(stderr, "[CreateMario] EXITING function -> final map size = %zu   map addr = %p\n",
          self->m_marios.size(), (void*)&self->m_marios);

  fprintf(stderr, "[libsm64] Created Mario %d at (%.1f, %.1f, %.1f)\n", id, x, y, z);
  return id;
}

void MarioManager::DestroyMario(int id) {
  MarioManager* self = Get();
  if (!self) {
    fprintf(stderr, "[DestroyMario] Manager not available, skipping id=%d\n", id);
    return;
  }

  auto it = self->m_marios.find(id);
  if (it == self->m_marios.end()) {
    fprintf(stderr, "[DestroyMario] ID %d not found — no-op\n", id);
    return;
  }

  sm64_mario_delete(id); // Assuming libsm64 has this; otherwise, just mark inactive

  auto& instance = *it->second;
  delete[] instance.geom.position;
  delete[] instance.geom.normal;
  delete[] instance.geom.color;
  delete[] instance.geom.uv;

  self->m_marios.erase(it);
  fprintf(stderr, "[libsm64] Destroyed Mario %d\n", id);
}

MarioInstance* MarioManager::GetMario(int id) {
  MarioManager* self = Get();
  if (!self) return nullptr;

  auto it = self->m_marios.find(id);
  return (it != self->m_marios.end()) ? it->second.get() : nullptr;
}

const MarioInstance* MarioManager::GetMario(int id) const {
  MarioManager* self = const_cast<MarioManager*>(this); // const_cast for Get() call
  if (!self) return nullptr;

  auto it = self->m_marios.find(id);
  return (it != self->m_marios.end()) ? it->second.get() : nullptr;
}

size_t MarioManager::GetActiveMarioCount() {
    MarioManager* mgr = Get();
    return mgr ? mgr->m_marios.size() : 0;
}

std::vector<int> MarioManager::GetActiveMarioIds() {
    std::vector<int> ids;

    MarioManager* mgr = Get();
    if (!mgr) {
        return ids;  // empty vector — safe, no crash
    }

    for (const auto& pair : mgr->m_marios) {
        if (pair.second->active) {
            ids.push_back(pair.first);
        }
    }

    return ids;
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
  static const SM64MarioGeometryBuffers zero = {};  // zero-initialized
  return zero;
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
  static const SM64MarioInputs zero = {};  // zero-initialized
  return zero;
}

void MarioManager::SetMarioCamera(int id, float x, float z) {
  auto& inputs = GetMarioInputs(id);
  
  inputs.camLookX = x;
  inputs.camLookZ = z;
}

void MarioManager::SetMarioMusic(int id, uint32_t music_bits) {
  // Stubbed
  (void)id;
  (void)music_bits;
}

void MarioManager::SetMarioPosition(int id, float x, float y, float z) {
  sm64_set_mario_position(id, x, y, z);
}

void MarioManager::SetMarioWaterLevel(int id, float level) {
 // sm64_mario_set_water_level(id, level);
}

void MarioManager::ChangeMarioState(int id, uint32_t act) {
  // Stubbed
  (void)id;
  (void)act;
}

void MarioManager::HealMario(int id) {
  sm64_set_mario_health(id, 256*8);
  sm64_set_mario_action(id, 0x0C400201); // ACT_IDLE
}

void MarioManager::DamageMario(int id) {
  auto* inst = GetMario(id);
  if (inst) {
    sm64_mario_take_damage(inst->id, 1, 0xFF, inst->state.position[0], inst->state.position[1], inst->state.position[2]);
  }
}

void MarioManager::Tick() {
  MarioManager* self = Get();
  if (!self) return;

  using clock = std::chrono::steady_clock;
  auto now = clock::now();

  if (g_last_tick_time.time_since_epoch().count() == 0) {
    g_last_tick_time = now;
    return;
  }

  double frame_dt = std::chrono::duration<double>(now - g_last_tick_time).count();
  g_last_tick_time = now;
  g_tick_accumulator += frame_dt;

  while (g_tick_accumulator >= MARIO_FIXED_DT) {
    for (auto& pair : self->m_marios) {
      if (pair.second->active) {
        jak1::call_goal_function_by_name("update-sm64-camera-from-goal");
        //self->CleanupDistantActorCollide();
        self->TickMario(pair.first);
      }
    }

    g_tick_accumulator -= MARIO_FIXED_DT;
  }
}

//const float M_PI = 3.14159265358979323846f;

void MarioManager::TickMario(int id) {
  auto* inst = GetMario(id);
  if (!inst || !inst->active) return;

  //jak1::call_goal_function_by_name("update-mario-water-height-from-goal");

  for (auto& [name, info] : m_actor_infos)
{
    if (info.is_spawned )
    {
        SM64ObjectTransform xf{};
        xf.position[0] = info.pos[0] * METERS_TO_UNITS;
        xf.position[1] = info.pos[1] * METERS_TO_UNITS;
        xf.position[2] = info.pos[2] * METERS_TO_UNITS;
        xf.eulerRotation[0] = info.euler_rot[0] * (M_PI / 180.0f); // Convert degrees to radians
        xf.eulerRotation[1] = info.euler_rot[1] * (M_PI / 180.0f); // Convert degrees to radians
        xf.eulerRotation[2] = info.euler_rot[2] * (M_PI / 180.0f); // Convert degrees to radians

        sm64_surface_object_move(info.sm64_id, &xf);
    }
}

  sm64_mario_tick(inst->id, &inst->inputs, &inst->state, &inst->geom);
}

void MarioManager::UpdateCollide(int id) {
  (void)id;
}

void MarioManager::UpdateCollideGlobal() {
  // Stub
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
  // Stub
}

void MarioManager::UpdatePseudoFloor(int id) {
  (void)id;
}

void MarioManager::MaybeReloadSurfaces(int id) {
  auto* inst = GetMario(id);
  if (!inst) return;

  float dx = inst->state.position[0] - g_cylinder_center[0];
  float dz = inst->state.position[2] - g_cylinder_center[2];
  float dist_sq = dx * dx + dz * dz;

  float reload_threshold_sq = (CYLINDER_RADIUS - CYLINDER_BUFFER) * (CYLINDER_RADIUS - CYLINDER_BUFFER);
  if (dist_sq > reload_threshold_sq) {
    g_cylinder_center[0] = inst->state.position[0];
    g_cylinder_center[1] = inst->state.position[1];
    g_cylinder_center[2] = inst->state.position[2];
  }
}

void MarioManager::MaybeReloadSurfacesGlobal() {
  // Stub
}

void MarioManager::Shutdown() {
  if (!sInstance) return;
  sm64_global_terminate();
  delete sInstance;
  sInstance = nullptr;
  g_debug_mario_mgr = nullptr;
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

// ─────────────────────────────────────────────────────────────────────────────
// pc_ wrapper functions (all now use pointer)
// ─────────────────────────────────────────────────────────────────────────────

uint64_t pc_get_mario_x(int id)       { float v = MarioManager::Get()->GetMarioX(id);       uint64_t b; memcpy(&b, &v, 4); return b; }
uint64_t pc_get_mario_y(int id)       { float v = MarioManager::Get()->GetMarioY(id);       uint64_t b; memcpy(&b, &v, 4); return b; }
uint64_t pc_get_mario_z(int id)       { float v = MarioManager::Get()->GetMarioZ(id);       uint64_t b; memcpy(&b, &v, 4); return b; }
uint64_t pc_get_mario_action(int id)  { return MarioManager::Get()->GetMarioAction(id); }

void pc_set_mario_camera(uint32_t x, uint32_t z) {
  float fx, fz;
  memcpy(&fx, &x, 4);
  memcpy(&fz, &z, 4);
  MarioManager::Get()->SetMarioCamera(0, fx, fz);
}

void pc_set_mario_music_from_goal(int id, uint32_t music_bits) {
  MarioManager::Get()->SetMarioMusic(id, music_bits);
}

void pc_set_mario_position_from_goal(int id, uint32_t x, uint32_t y, uint32_t z) {
  float fx, fy, fz;
  memcpy(&fx, &x, 4);
  memcpy(&fy, &y, 4);
  memcpy(&fz, &z, 4);
  fx *= METERS_TO_UNITS;
  fy *= METERS_TO_UNITS;
  fz *= METERS_TO_UNITS;
  MarioManager::Get()->SetMarioPosition(id, fx, fy, fz);
}

void pc_set_mario_water_level_from_goal(int id, uint32_t level_bits) {
  float f;
  memcpy(&f, &level_bits, 4);
  f *= METERS_TO_UNITS;
  MarioManager::Get()->SetMarioWaterLevel(id, f);
}

void pc_change_mario_state(int id, uint32_t act_bits) {
  MarioManager::Get()->ChangeMarioState(id, act_bits);
}

void update_platform_info_from_goal(u32 ptr) {
  MarioManager::Get()->UpdatePlatformInfo(ptr);
}

void update_moving_platform() {
  MarioManager::Get()->UpdateMovingPlatform();
}

void update_psuedo_floor_under_mario(int id) {
  MarioManager::Get()->UpdatePseudoFloor(id);
}

void pc_heal_mario(int id)   { MarioManager::Get()->HealMario(id); }
void pc_damage_mario(int id) { MarioManager::Get()->DamageMario(id); }

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
    {1,  {training_surfaces,   training_surfaces_count}},
    {2,  {village1_surfaces,   village1_surfaces_count}},
    {3,  {beach_surfaces,      beach_surfaces_count}},
    {4,  {jungle_surfaces,     jungle_surfaces_count}},
    {5,  {jungleb_surfaces,    jungleb_surfaces_count}},
    {6,  {misty_surfaces,      misty_surfaces_count}},
    {7,  {firecanyon_surfaces, firecanyon_surfaces_count}},
    {8,  {village2_surfaces,   village2_surfaces_count}},
    {9,  {sunken_surfaces,     sunken_surfaces_count}},
    {10, {sunkenb_surfaces,    sunkenb_surfaces_count}},
    {11, {swamp_surfaces,      swamp_surfaces_count}},
    {12, {rolling_surfaces,    rolling_surfaces_count}},
    {13, {ogre_surfaces,       ogre_surfaces_count}},
    {14, {village3_surfaces,   village3_surfaces_count}},
    {15, {snow_surfaces,       snow_surfaces_count}},
    {16, {maincave_surfaces,   maincave_surfaces_count}},
    {17, {darkcave_surfaces,   darkcave_surfaces_count}},
    {18, {robocave_surfaces,   robocave_surfaces_count}},
    {19, {lavatube_surfaces,   lavatube_surfaces_count}},
    {20, {citadel_surfaces,    citadel_surfaces_count}},
    {21, {finalboss_surfaces,  finalboss_surfaces_count}},
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
  MarioManager::Get()->MaybeReloadSurfacesGlobal();
}

// ─────────────────────────────────────────────────────────────────────────────
// Remaining stubs
// ─────────────────────────────────────────────────────────────────────────────

void pc_add_tris_to_surface(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {}
void pc_spawn_mario_test_collide(int id, uint32_t) { (void)id; }
void pc_mario_says_so_long_gay_bowsa(int id, uint32_t) { (void)id; }

bool run_and_render_mario(int id) {
  return MarioManager::Get()->GetMario(id) != nullptr;
}

bool has_blue_eco()          { return false; /* TODO */ }
bool should_render_mario(int id)   { return MarioManager::Get()->GetMario(id) != nullptr; }
