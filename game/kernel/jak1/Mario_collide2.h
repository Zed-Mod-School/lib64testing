// // Mario_collide2.h
// #pragma once

// #include "libsm64.h"
// #include "Mario1.h"           // for g_mario_state, METERS_TO_UNITS, PlatformInfo
// #include <string>
// #include <unordered_map>
// #include <vector>
// #include <cstdint>

// // Global control flag (can be set from GOAL via *run-collide*)
// extern bool g_run_mario_collide;

// // Per-actor tracked data
// struct ActorCollisionData {
//     std::string           name;

//     float                 root_trans[3] = {0,0,0};   // position in SM64 meters
//     float                 rotation[3]   = {0,0,0};   // yaw/pitch/roll (usually just yaw)

//     SM64Surface*          surfaces      = nullptr;   // owned memory
//     uint32_t              num_tris      = 0;

//     bool                  is_platform   = false;

//     uint32_t              sm64_id       = 0;         // 0 = not spawned
//     bool                  is_spawned    = false;

//     uint32_t              mesh_hash     = 0;         // for change detection
// };

// extern std::unordered_map<std::string, ActorCollisionData> g_active_actors;

// // Main per-frame collision update
// void update_mario_actor_collide_system();

// // Pseudo-floor under Mario
// void update_psuedo_floor_under_mario();

// // Moving platform sync from GOAL
// void update_platform_info_from_goal(u32 platform_info_ptr);
// void update_moving_platform();

// // Actor API called from GOAL
// void pc_begin_actor_collide(const char* name, bool is_platform);
// void pc_add_triangle_to_current_actor(int32_t v0[3], int32_t v1[3], int32_t v2[3]);
// void pc_finish_actor_collide(const char* name, float x, float y, float z, float rot_y);
// void pc_remove_actor_collide(const char* name);

// // Platform getters (used from kmachine.cpp)
// uint64_t pc_get_platform_x();
// uint64_t pc_get_platform_y();
// uint64_t pc_get_platform_z();

// // Helpers
// uint32_t simple_mesh_hash(const SM64Surface* surfaces, uint32_t count);

// #define MAX_ACTORS 1024

// extern SM64Surface     gSurfaceBuffer[MAX_ACTORS];
// extern int             gSpawnedSurfaceCount;
// extern int32_t         gTempVerts[3][3];
// extern int             gTempVertIndex;

// extern struct MarioActorCollisionData spawnedActors[MAX_ACTORS];
// extern int             num_active_actors;

// extern const float     zero_pos_array[3];

// // Moving platform globals (if used elsewhere)
// extern uint32_t           gPlatId;
// extern SM64ObjectTransform gPlatXf;
// extern int32_t            gPlatTimer;
// extern float              gPlatDir;
// extern float              gPlatYaw;
// extern float              gPlatSpinSpeed;
// extern bool               gPlatShouldSpin;

// extern PlatformInfo       g_platform_info;
// extern bool               g_platform_info_valid;