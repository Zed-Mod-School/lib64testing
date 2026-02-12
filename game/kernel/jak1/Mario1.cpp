// Mario1.cpp
#include "Mario1.h"
#include "Mario_collide.h"
#include "Mario_collide2.h"
#include "game/graphics/opengl_renderer/MarioRenderer.h"
#include "common/util/FileUtil.h"
#include "game/kernel/common/kmachine.h"
#include "game/kernel/common/kscheme.h"
#include "load_surfaces.h"  // contains all your level surface arrays


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
// Globals (cylinder + combined surfaces)
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



MarioManager::MarioManager() {
    const int maxTris = SM64_GEO_MAX_TRIANGLES;
    m_geom.position = new float[3 * 3 * maxTris];
    m_geom.normal   = new float[3 * 3 * maxTris];
    m_geom.color    = new float[3 * 3 * maxTris];
    m_geom.uv       = new float[2 * 3 * maxTris];

    memset(m_geom.position, 0, sizeof(float) * 3 * 3 * maxTris);
    memset(m_geom.normal,   0, sizeof(float) * 3 * 3 * maxTris);
    memset(m_geom.color,    0, sizeof(float) * 3 * 3 * maxTris);
    memset(m_geom.uv,       0, sizeof(float) * 2 * 3 * maxTris);
    m_geom.numTrianglesUsed = 0;
}

MarioManager::~MarioManager() {
    delete[] m_texture;
    delete[] m_geom.position;
    delete[] m_geom.normal;
    delete[] m_geom.color;
    delete[] m_geom.uv;

    if (g_combined_surfaces) {
        delete[] g_combined_surfaces;
        g_combined_surfaces = nullptr;
        g_combined_surfaces_count = 0;
    }
}

MarioManager& MarioManager::Get() {
    if (!sInstance) {
        Initialize();
    }
    return *sInstance;
}

void MarioManager::Initialize() {
    if (sInstance) return;
    sInstance = new MarioManager();
    fprintf(stderr, "STARTING NEW MANAGER");
    std::string baseDir = file_util::get_file_path({"iso_data"});
    std::string romPathStr = baseDir + "/mario/test.rom";
    const char* romPath = romPathStr.c_str();

    uint8_t* romBuffer = nullptr;
    std::ifstream file(romPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        fprintf(stderr, "Failed to open ROM file: %s\n", romPath);
        std::abort();
    }

    size_t romFileLength = file.tellg();
    romBuffer = new uint8_t[romFileLength + 1];
    file.seekg(0);
    file.read(reinterpret_cast<char*>(romBuffer), romFileLength);
    romBuffer[romFileLength] = 0;

    sInstance->m_texture = new uint8_t[4 * SM64_TEXTURE_WIDTH * SM64_TEXTURE_HEIGHT];

    sm64_global_init(romBuffer, sInstance->m_texture);

    // Debug: load village1
    sm64_static_surfaces_load(village1_surfaces, village1_surfaces_count);

    sInstance->m_id = sm64_mario_create(-7541.8f, 3688.475f, 9237.5f);

    sm64_audio_init(romBuffer);
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "[libsm64][ERROR] SDL_Init failed: %s\n", SDL_GetError());
    }
    audio_init();

    delete[] romBuffer;

    // Set initial cylinder center
    g_cylinder_center[0] = sInstance->GetX();
    g_cylinder_center[1] = sInstance->GetY();
    g_cylinder_center[2] = sInstance->GetZ();
}

void MarioManager::Shutdown() {
    if (!sInstance) return;
    sm64_global_terminate();
    delete sInstance;
    sInstance = nullptr;
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

    // Assuming m_inputs already contains the raw -64 to +64 values from GOAL
    float scaled_stick_x = (float)self.m_inputs.stickX / 64.0f;
    float scaled_stick_y = (float)self.m_inputs.stickY / 64.0f;

    while (g_tick_accumulator >= MARIO_FIXED_DT) {
        jak1::call_goal_function_by_name("update-sm64-camera-from-goal");
        
        SM64MarioInputs inputs = self.m_inputs;
        inputs.stickX = scaled_stick_x;
        inputs.stickY = -scaled_stick_y; // Invert Y for SM64 standard

        sm64_mario_tick(self.m_id, &inputs, &self.m_state, &self.m_geom);

        self.UpdateCollide();
        self.MaybeReloadSurfaces();

        g_tick_accumulator -= MARIO_FIXED_DT;
    }
}


void MarioManager::UpdateCollide() {
    // Dynamic object updates (stub)
    // for (auto& obj : g_active_debug_objects) {
    //     sm64_surface_object_update(obj.id, ...);
    // }

    // Main collision step is done inside sm64_mario_tick()
    // No separate sm64_mario_update_collide() exists in libsm64
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

    // Example: convert u32 bits → float
    float x = *reinterpret_cast<const float*>(&m_platform_info.x_pos);
    float y = *reinterpret_cast<const float*>(&m_platform_info.y_pos);
    float z = *reinterpret_cast<const float*>(&m_platform_info.z_pos);

    // TODO: apply to SM64 moving platform object
    // (you'll need to create persistent SM64SurfaceObject instances)
}

void MarioManager::UpdatePseudoFloor() {
    // Optional: flat floor under Mario for safety
    // sm64_static_surfaces_load(psuedo_floor_surfaces, 2);
}

void MarioManager::MaybeReloadSurfaces() {
    float dx = m_state.position[0] - g_cylinder_center[0];
    float dz = m_state.position[2] - g_cylinder_center[2];
    float dist_sq = dx * dx + dz * dz;

    float threshold_sq = (CYLINDER_RADIUS - CYLINDER_BUFFER) * (CYLINDER_RADIUS - CYLINDER_BUFFER);

    if (dist_sq > threshold_sq) {
        g_cylinder_center[0] = m_state.position[0];
        g_cylinder_center[1] = m_state.position[1];
        g_cylinder_center[2] = m_state.position[2];

        load_surfaces_near(g_cylinder_center[0], g_cylinder_center[1], g_cylinder_center[2]);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Getters / Setters
// ─────────────────────────────────────────────────────────────────────────────



// (The other getters are already inline in the header)

void MarioManager::SetCamera(float x, float z) {
    m_inputs.camLookX = x;
    m_inputs.camLookZ = z;
}

void MarioManager::SetPosition(float x, float y, float z) {
    m_state.position[0] = x;
    m_state.position[1] = y;
    m_state.position[2] = z;
}

void MarioManager::SetMusic(uint32_t music_bits) {
    sm64_play_music(0, music_bits, 0);
}

void MarioManager::SetWaterLevel(float level) {
    //sm64_set_water_level(static_cast<int32_t>(level));
}

void MarioManager::ChangeState(uint32_t act) {
    sm64_set_mario_action(m_id, act);
}

void MarioManager::Heal() {
    sm64_set_mario_health(m_id, 0x880);  // full health
}

void MarioManager::Damage() {
    // Correct call: needs position too (damage knockback origin)
    sm64_mario_take_damage(m_id, 1, 0xFF, m_state.position[0], m_state.position[1], m_state.position[2]);
}

// ─────────────────────────────────────────────────────────────────────────────
// Surface loading helpers
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

    printf("Reloaded %d surfaces near (%.1f, %.1f, %.1f)\n", count, x, y, z);
    return count;
}

// ─────────────────────────────────────────────────────────────────────────────
// pc_ wrapper functions
// ─────────────────────────────────────────────────────────────────────────────

uint64_t pc_get_mario_x()       { float v = MarioManager::Get().GetX();       uint64_t b; memcpy(&b, &v, 4); return b; }
uint64_t pc_get_mario_y()       { float v = MarioManager::Get().GetY();       uint64_t b; memcpy(&b, &v, 4); return b; }
uint64_t pc_get_mario_z()       { float v = MarioManager::Get().GetZ();       uint64_t b; memcpy(&b, &v, 4); return b; }
uint64_t pc_get_mario_action()  { return MarioManager::Get().GetAction(); }

void pc_set_mario_camera(uint32_t x, uint32_t z) {
    float fx, fz;
    memcpy(&fx, &x, 4);
    memcpy(&fz, &z, 4);
    MarioManager::Get().SetCamera(fx, fz);
}

void pc_set_mario_music_from_goal(uint32_t music_bits) {
    MarioManager::Get().SetMusic(music_bits);
}

void pc_set_mario_position_from_goal(uint32_t x, uint32_t y, uint32_t z) {
    float fx, fy, fz;
    memcpy(&fx, &x, 4);
    memcpy(&fy, &y, 4);
    memcpy(&fz, &z, 4);
    MarioManager::Get().SetPosition(fx, fy, fz);
}

void pc_set_mario_water_level_from_goal(uint32_t level_bits) {
    float f;
    memcpy(&f, &level_bits, 4);
    MarioManager::Get().SetWaterLevel(f);
}

void pc_change_mario_state(uint32_t act_bits) {
    MarioManager::Get().ChangeState(act_bits);
}

void update_platform_info_from_goal(u32 ptr) {
    MarioManager::Get().UpdatePlatformInfo(ptr);
}

void update_moving_platform() {
    MarioManager::Get().UpdateMovingPlatform();
}

void update_psuedo_floor_under_mario() {
    MarioManager::Get().UpdatePseudoFloor();
}

void pc_heal_mario()   { MarioManager::Get().Heal(); }
void pc_damage_mario() { MarioManager::Get().Damage(); }

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
    MarioManager::Get().MaybeReloadSurfaces();
}

// ─────────────────────────────────────────────────────────────────────────────
// Remaining stubs
// ─────────────────────────────────────────────────────────────────────────────

void pc_add_tris_to_surface(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {}
void pc_spawn_mario_test_collide(uint32_t) {}
void pc_mario_says_so_long_gay_bowsa(uint32_t) {}

bool run_and_render_mario() {
    return MarioManager::Get().GetId() != -1;
}

bool has_blue_eco()          { return false; /* TODO */ }
bool should_render_mario()   { return true;  /* TODO */ }
