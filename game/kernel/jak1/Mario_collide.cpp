#include "Mario1.h"
#include "Mario_collide.h"

#include <cstring>  // for strcmp, memcpy, memset
#include <cstdio>   // for printf

#define MAX_ACTORS 1024

static struct SM64Surface gSurfaceBuffer[MAX_ACTORS];
static int gSpawnedSurfaceCount = 0;
static int32_t gTempVerts[3][3];
static int gTempVertIndex = 0;

MarioActorCollisionData spawnedActors[MAX_ACTORS];
int num_active_actors = 0;

const float zero_pos_array[3] = {0.0f, 0.0f, 0.0f};

static PlatformInfo g_platform_info = {0};
static bool g_platform_info_valid = false;

// Globals for the moving platform
static uint32_t           gPlatId    = 0;
static SM64ObjectTransform gPlatXf   = {};
static int32_t            gPlatTimer = 0;
static float              gPlatDir   = 1.0f;
static float              gPlatYaw   = 0.0f;
static float              gPlatSpinSpeed = 90.0f;
static bool               gPlatShouldSpin = false;

void reset_temp_surface_buffers() {
    gSpawnedSurfaceCount = 0;
    memset(gSurfaceBuffer, 0, sizeof(gSurfaceBuffer));
    memset(gTempVerts, 0, sizeof(gTempVerts));
    gTempVertIndex = 0;
}

void delete_surface_object_by_name(const char* name) {
    if (!name || num_active_actors == 0) {
        return;
    }

    int index_to_remove = -1;
    for (int i = 0; i < num_active_actors; ++i) {
        const MarioActorCollisionData& current = spawnedActors[i];
        if (current.name && strcmp(current.name, name) == 0) {
            index_to_remove = i;
            break;
        }
    }

    if (index_to_remove != -1) {
        MarioActorCollisionData& actor = spawnedActors[index_to_remove];

        // Remove from libsm64
        sm64_surface_object_delete(actor.id);

        // Free allocated surfaces
        if (actor.surfaceObj.surfaces) {
            free(actor.surfaceObj.surfaces);
            actor.surfaceObj.surfaces = nullptr;
        }

        num_active_actors--;

        // Move last element into removed slot (if not already last)
        if (index_to_remove < num_active_actors) {
            spawnedActors[index_to_remove] = spawnedActors[num_active_actors];
        }
    }
}

void pc_mario_spawn_updated_tris(const char* name) {
    if (gSpawnedSurfaceCount <= 0) {
        printf("[pc_mario_spawn_updated_tris] No surfaces to spawn for '%s'\n", name ? name : "<null>");
        return;
    }

    spawn_surfaces_under_mario(
        zero_pos_array,
        gSurfaceBuffer,
        gSpawnedSurfaceCount,
        name,
        0.0f
    );

    // Clean up after spawn
    reset_temp_surface_buffers();
}

uint32_t pc_mario_spawn_updated_tris_with_id(const char* name) {
    if (gSpawnedSurfaceCount <= 0) {
        printf("[pc_mario_spawn_updated_tris_with_id] No surfaces in buffer for '%s'\n", name ? name : "<null>");
        return 0;
    }

    uint32_t id = spawn_surfaces_under_mario(
        zero_pos_array,
        gSurfaceBuffer,
        gSpawnedSurfaceCount,
        name,
        0.0f
    );

    if (id != 0) {
        printf("[PLATFORM] Spawned '%s' with ID %u\n", name ? name : "<null>", id);

        // Initialize transform
        gPlatXf.position[0] = zero_pos_array[0];
        gPlatXf.position[1] = zero_pos_array[1];
        gPlatXf.position[2] = zero_pos_array[2];
        gPlatXf.eulerRotation[0] = 0.0f;
        gPlatXf.eulerRotation[1] = 0.0f;
        gPlatXf.eulerRotation[2] = 0.0f;

        gPlatTimer = 0;
        gPlatDir = 1.0f;
        gPlatYaw = 0.0f;
        gPlatShouldSpin = false;

        sm64_surface_object_move(id, &gPlatXf);
    } else {
        printf("[PLATFORM] Failed to spawn '%s'\n", name ? name : "<null>");
    }

    reset_temp_surface_buffers();
    return id;
}

void pc_add_tris_to_surface(u32 x_bits, u32 y_bits, u32 z_bits, u32 vert_index_bits) {
    float x, y, z, vert_index_f;
    memcpy(&x, &x_bits, sizeof(float));
    memcpy(&y, &y_bits, sizeof(float));
    memcpy(&z, &z_bits, sizeof(float));
    memcpy(&vert_index_f, &vert_index_bits, sizeof(float));

    x *= METERS_TO_UNITS;
    y *= METERS_TO_UNITS;
    z *= METERS_TO_UNITS;

    int vert_index = static_cast<int>(vert_index_f + 0.5f);

    if (vert_index < 1 || vert_index > 3) {
        printf("ERROR: vert_index out of range (%d)\n", vert_index);
        return;
    }

    int idx = vert_index - 1;
    gTempVerts[idx][0] = static_cast<int32_t>(x);
    gTempVerts[idx][1] = static_cast<int32_t>(y);
    gTempVerts[idx][2] = static_cast<int32_t>(z);

    if (vert_index == 3) {
        if (gSpawnedSurfaceCount >= MAX_ACTORS) {
            printf("ERROR: surface buffer full (%d)\n", gSpawnedSurfaceCount);
            return;
        }

        SM64Surface* surf = &gSurfaceBuffer[gSpawnedSurfaceCount];
        surf->type = SURFACE_DEFAULT;
        surf->force = 0;
        surf->terrain = TERRAIN_STONE;
        memcpy(surf->vertices, gTempVerts, sizeof(gTempVerts));

        gSpawnedSurfaceCount++;
    }
}

void pc_spawn_mario_test_collide(u32 name_ptr) {
    const char* name = Ptr<String>(name_ptr).c()->data();

    if (!name) {
        printf("[pc_spawn_mario_test_collide] Null name pointer\n");
        return;
    }

    // Special case: persistent moving platform
    if (strcmp(name, "active-plat") == 0) {
        if (gPlatId == 0) {
            gPlatId = pc_mario_spawn_updated_tris_with_id(name);
        }
        // If already exists → do nothing (updated via update_moving_platform)
        return;
    }

    // Normal case: delete old + spawn new
    delete_surface_object_by_name(name);
    pc_mario_spawn_updated_tris(name);
}

void update_moving_platform() {
    if (gPlatId == 0 || !g_platform_info_valid) {
        return;
    }

    float targetX, targetY, targetZ;
    memcpy(&targetX, &g_platform_info.x_pos, sizeof(u32));
    memcpy(&targetY, &g_platform_info.y_pos, sizeof(u32));
    memcpy(&targetZ, &g_platform_info.z_pos, sizeof(u32));

    targetX *= METERS_TO_UNITS;
    targetY *= METERS_TO_UNITS;
    targetZ *= METERS_TO_UNITS;

    float rotX, rotY, rotZ;
    memcpy(&rotX, &g_platform_info.rot_x, sizeof(u32));
    memcpy(&rotY, &g_platform_info.rot_y, sizeof(u32));
    memcpy(&rotZ, &g_platform_info.rot_z, sizeof(u32));

    gPlatXf.position[0] = targetX;
    gPlatXf.position[1] = targetY;
    gPlatXf.position[2] = targetZ;

    gPlatXf.eulerRotation[0] = rotX;
    gPlatXf.eulerRotation[1] = rotY;
    gPlatXf.eulerRotation[2] = rotZ;

    sm64_surface_object_move(gPlatId, &gPlatXf);
}

uint64_t pc_get_platform_x() {
    float x = gPlatXf.position[0];
    uint64_t out = 0;
    memcpy(&out, &x, sizeof(float));
    return out;
}

uint64_t pc_get_platform_y() {
    float y = gPlatXf.position[1];
    uint64_t out = 0;
    memcpy(&out, &y, sizeof(float));
    return out;
}

uint64_t pc_get_platform_z() {
    float z = gPlatXf.position[2];
    uint64_t out = 0;
    memcpy(&out, &z, sizeof(float));
    return out;
}

void update_platform_info_from_goal(u32 platform_info_ptr) {
    if (!platform_info_ptr) {
        printf("[PLATFORM] Warning: null pointer from GOAL\n");
        g_platform_info_valid = false;
        return;
    }

    auto info = Ptr<PlatformInfo>(platform_info_ptr).c();

    g_platform_info = *info;  // copy whole struct
    g_platform_info_valid = true;

    // Optional debug print
    float tx = *reinterpret_cast<float*>(&info->x_pos) * METERS_TO_UNITS;
    float ty = *reinterpret_cast<float*>(&info->y_pos) * METERS_TO_UNITS;
    float tz = *reinterpret_cast<float*>(&info->z_pos) * METERS_TO_UNITS;
    float rx = *reinterpret_cast<float*>(&info->rot_x);
    float ry = *reinterpret_cast<float*>(&info->rot_y);
    float rz = *reinterpret_cast<float*>(&info->rot_z);

    printf("[PLATFORM] Updated: pos(%.2f, %.2f, %.2f) rot(%.1f, %.1f, %.1f)\n",
           tx, ty, tz, rx, ry, rz);
}

uint32_t spawn_surfaces_under_mario(
    const float*           marioPos,
    const SM64Surface*     surfaces,
    int                    surfaceCount,
    const char*            objectName,
    float                  y_offset)
{
    if (num_active_actors >= MAX_ACTORS || surfaceCount <= 0) {
        return 0;
    }

    SM64SurfaceObject obj{};
    memset(&obj, 0, sizeof(obj));

    MarioActorCollisionData& actor = spawnedActors[num_active_actors];
    actor.name = objectName;

    const float* pos = marioPos ? marioPos : zero_pos_array;
    obj.transform.position[0] = pos[0];
    obj.transform.position[1] = pos[1] + y_offset;
    obj.transform.position[2] = pos[2];

    obj.surfaceCount = static_cast<uint32_t>(surfaceCount);
    obj.surfaces = (SM64Surface*)malloc(sizeof(SM64Surface) * surfaceCount);

    if (!obj.surfaces) {
        return 0;
    }

    memcpy(obj.surfaces, surfaces, sizeof(SM64Surface) * surfaceCount);

    // Compute approximate center / size (for later use if needed)
    float minc[3] = {1e9f, 1e9f, 1e9f};
    float maxc[3] = {-1e9f, -1e9f, -1e9f};

    for (int i = 0; i < surfaceCount; ++i) {
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
                float v = static_cast<float>(surfaces[i].vertices[j][k]);
                minc[k] = std::min(minc[k], v);
                maxc[k] = std::max(maxc[k], v);
            }
        }
    }

    actor.pos[0] = (maxc[0] + minc[0]) * 0.5f;
    actor.pos[1] = (maxc[1] + minc[1]) * 0.5f;
    actor.pos[2] = (maxc[2] + minc[2]) * 0.5f;
    actor.size = std::max({maxc[0] - minc[0], maxc[1] - minc[1], maxc[2] - minc[2]});

    uint32_t id = sm64_surface_object_create(&obj);
    if (id == 0) {
        free(obj.surfaces);
        return 0;
    }

    actor.id = id;
    actor.surfaceObj = obj;

    num_active_actors++;
    return id;
}