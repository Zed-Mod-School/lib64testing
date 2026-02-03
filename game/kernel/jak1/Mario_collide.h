// Mario_collide.h
#pragma once

#include "Mario1.h"
#include <cstdint>
#include <cstring>
#include <algorithm>   // std::min, std::max

// ──────────────────────────────────────────────────────────────────────────────
// Constants & Globals (extern declarations)
// ──────────────────────────────────────────────────────────────────────────────

#define MAX_ACTORS 1024

extern SM64Surface     gSurfaceBuffer[MAX_ACTORS];
extern int             gSpawnedSurfaceCount;
extern int32_t         gTempVerts[3][3];
extern int             gTempVertIndex;

extern struct MarioActorCollisionData spawnedActors[MAX_ACTORS];
extern int             num_active_actors;

extern const float     zero_pos_array[3];

// Moving platform globals (if used elsewhere)
extern uint32_t           gPlatId;
extern SM64ObjectTransform gPlatXf;
extern int32_t            gPlatTimer;
extern float              gPlatDir;
extern float              gPlatYaw;
extern float              gPlatSpinSpeed;
extern bool               gPlatShouldSpin;

extern PlatformInfo       g_platform_info;
extern bool               g_platform_info_valid;

// ──────────────────────────────────────────────────────────────────────────────
// Data structures
// ──────────────────────────────────────────────────────────────────────────────

struct MarioActorCollisionData {
    float pos[3];
    float size;
    SM64SurfaceObject surfaceObj;
    const char* name;
    uint32_t id;
};

// ──────────────────────────────────────────────────────────────────────────────
// Template version – for fixed-size arrays (like psuedo_floor_surfaces)
// ──────────────────────────────────────────────────────────────────────────────

template <size_t N>
uint32_t spawn_surfaces_under_mario(
    const float*           marioPos,
    const SM64Surface      (&surfaces)[N],
    const char*            objectName,
    float                  y_offset = 0.0f)
{
    if (num_active_actors >= MAX_ACTORS)
        return 0;

    SM64SurfaceObject obj{};
    memset(&obj, 0, sizeof(obj));

    MarioActorCollisionData& currActor = spawnedActors[num_active_actors++];

    currActor.name = objectName;

    const float* pos = marioPos ? marioPos : zero_pos_array;
    obj.transform.position[0] = pos[0];
    obj.transform.position[1] = pos[1] + y_offset;
    obj.transform.position[2] = pos[2];

    obj.surfaceCount = static_cast<uint32_t>(N);

    obj.surfaces = (SM64Surface*)malloc(sizeof(SM64Surface) * N);
    if (!obj.surfaces) {
        --num_active_actors;
        return 0;
    }

    memcpy(obj.surfaces, surfaces, sizeof(SM64Surface) * N);

    // Bounding box
    float minc[3] = {1e9f, 1e9f, 1e9f};
    float maxc[3] = {-1e9f, -1e9f, -1e9f};

    for (size_t i = 0; i < N; ++i) {
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
                float v = static_cast<float>(surfaces[i].vertices[j][k]);
                minc[k] = std::min(minc[k], v);
                maxc[k] = std::max(maxc[k], v);
            }
        }
    }

    currActor.pos[0] = (maxc[0] + minc[0]) * 0.5f;
    currActor.pos[1] = (maxc[1] + minc[1]) * 0.5f;
    currActor.pos[2] = (maxc[2] + minc[2]) * 0.5f;

    currActor.size = std::max({maxc[0] - minc[0], maxc[1] - minc[1], maxc[2] - minc[2]});

    uint32_t id = sm64_surface_object_create(&obj);

    currActor.id = id;
    currActor.surfaceObj = obj;

    return id;
}

// ──────────────────────────────────────────────────────────────────────────────
// Pointer + count version – for dynamic buffers like gSurfaceBuffer
// ──────────────────────────────────────────────────────────────────────────────

uint32_t spawn_surfaces_under_mario(
    const float*           marioPos,
    const SM64Surface*     surfaces,
    int                    surfaceCount,
    const char*            objectName,
    float                  y_offset = 0.0f
);

// ──────────────────────────────────────────────────────────────────────────────
// Other functions
// ──────────────────────────────────────────────────────────────────────────────

void delete_surface_object_by_name(const char* name);
void pc_mario_spawn_updated_tris(const char* name);
void pc_add_tris_to_surface(uint32_t x_bits, uint32_t y_bits, uint32_t z_bits, uint32_t vert_index_bits);
void pc_spawn_mario_test_collide(uint32_t name_ptr);
void update_moving_platform();
void update_platform_info_from_goal(uint32_t platform_info_ptr);

inline bool is_moving_platform_active() { return gPlatId != 0; }

inline void reset_temp_surface_buffers() {
    gSpawnedSurfaceCount = 0;
    memset(gSurfaceBuffer, 0, sizeof(gSurfaceBuffer));
    memset(gTempVerts, 0, sizeof(gTempVerts));
}