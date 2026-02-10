#include "Mario1.h"
#include "Mario_collide.h"



#define MAX_ACTORS 1024

static struct SM64Surface gSurfaceBuffer[MAX_ACTORS];
static int gSpawnedSurfaceCount = 0;
static int32_t gTempVerts[3][3];
static int gTempVertIndex = 0;

MarioActorCollisionData spawnedActors[MAX_ACTORS];
int num_active_actors = 0;

const float zero_pos_array[3] = {0.0f, 0.0f, 0.0f};

static PlatformInfo g_platform_info = {0};  // p
static bool g_platform_info_valid = false;

// Globals for the moving platform
static uint32_t           gPlatId    = 0;
static SM64ObjectTransform gPlatXf   = {};  // Authoritative transform (libsm64 uses this)
static int32_t            gPlatTimer = 0;
static float              gPlatDir   = 1.0f;
static float gPlatYaw = 0.0f;           // current visual yaw rotation (degrees)
static float gPlatSpinSpeed = 90.0f;    // degrees per second when spinning
static bool gPlatShouldSpin = false;    // true when we just reversed

///───────────────────────────────────────────── START Mario Collide Actor Management ────────



void delete_surface_object_by_name(const char* name) {
  if (!name || num_active_actors == 0)
    return;

  int index_to_remove = -1;

  for (int i = 0; i < num_active_actors; ++i) {
    const MarioActorCollisionData& currentActor = spawnedActors[i];
    if (currentActor.name != NULL && strcmp(currentActor.name, name) == 0) {
      index_to_remove = i;
      break;
    }
  }

  if (index_to_remove != -1) {
    MarioActorCollisionData& actorToDelete = spawnedActors[index_to_remove];

    // 2. De-register the collision object using the stored ID
    sm64_surface_object_delete(actorToDelete.id);

    // 3. Free the surface memory that was malloc'd in spawn_surfaces_under_mario
    if (actorToDelete.surfaceObj.surfaces != NULL) {
      free(actorToDelete.surfaceObj.surfaces);
      actorToDelete.surfaceObj.surfaces = NULL;
    }

    // Decrement the total count
    num_active_actors--;

    // If the object being removed wasn't the last one,
    // copy the last MarioActorCollisionData structure over the one we are removing.
    if (index_to_remove < num_active_actors) {
      spawnedActors[index_to_remove] = spawnedActors[num_active_actors];
    }

    // The object is now removed from the array and its collision is deleted.
  }
}

// static int surface_spawn_count = 0;
void pc_mario_spawn_updated_tris(const char* name) {
  // this is called from goal after we finish updating gSurfaceBuffer to push them into the mario
  // engine first step is to spawn the surfaces
  const char* objectName;
  objectName = name;  // money-32
  // We should store the ID here and return it if needed
spawn_surfaces_under_mario(
    zero_pos_array,
    gSurfaceBuffer,
    gSpawnedSurfaceCount,   // ← this was missing
    objectName
);
  // surface_spawn_count++; // Increment counter
  // Reset buffers
   gSpawnedSurfaceCount = 0;
  memset(gSurfaceBuffer, 0, sizeof(gSurfaceBuffer));
  memset(gTempVerts, 0, sizeof(gTempVerts));
}

void pc_add_tris_to_surface(u32 x_bits, u32 y_bits, u32 z_bits, u32 vert_index_bits) {
  float x, y, z, vert_index_f;

  // Decode floats
  memcpy(&x, &x_bits, sizeof(float));
  memcpy(&y, &y_bits, sizeof(float));
  memcpy(&z, &z_bits, sizeof(float));
  memcpy(&vert_index_f, &vert_index_bits, sizeof(float));

  // Unit conversion
  x *= METERS_TO_UNITS;
  y *= METERS_TO_UNITS;
  z *= METERS_TO_UNITS;

  int vert_index = (int)(vert_index_f + 0.5f);

  // Validate index
  if (vert_index < 1 || vert_index > 3) {
    printf("  ERROR: vert_index out of range (must be 1–3), aborting\n");
    return;
  }

  int idx = vert_index - 1;
  // Store vertex
gTempVerts[idx][0] = (int32_t)x;
gTempVerts[idx][1] = (int32_t)y;
gTempVerts[idx][2] = (int32_t)z;


  // Commit triangle if this is the last vertex, we trust GOAL to send the 3rd vertex last lol
  if (vert_index == 3) {
    if (gSpawnedSurfaceCount >= MAX_ACTORS) {
      printf("  ERROR: surface buffer full (%d), aborting\n",
             gSpawnedSurfaceCount);
      return;
    }
    struct SM64Surface* surf = &gSurfaceBuffer[gSpawnedSurfaceCount];
    surf->type = SURFACE_DEFAULT;
    surf->force = 0;
    surf->terrain = TERRAIN_STONE;
    memcpy(surf->vertices, gTempVerts, sizeof(gTempVerts));
    std::string actor_name = Ptr<String>(name_bits).c()->data();
    gSpawnedSurfaceCount++;
  }
 // printf("[pc_add_tris] EXIT\n");
}

// Add this helper function somewhere in Mario_collide.cpp (near the top or with other spawn functions)
uint32_t pc_mario_spawn_updated_tris_with_id(const char* name) {
    if (gSpawnedSurfaceCount <= 0) {
        printf("[PLATFORM] WARNING: Tried to spawn '%s' but no surfaces in buffer!\n", name);
        return 0;
    }

    // Use Mario's current position as fallback spawn point (or zero_pos_array if you prefer)
    const float* spawnPos = zero_pos_array;

    uint32_t id = spawn_surfaces_under_mario(
        spawnPos,
        gSurfaceBuffer,
        gSpawnedSurfaceCount,
        name,
        0.0f  // y_offset - adjust if platform needs to be lower/higher relative to Mario feet
    );

    if (id != 0) {
        printf("[PLATFORM] Successfully spawned moving platform '%s' with ID %u\n", name, id);

        // Initialize authoritative transform from current Mario pos (or buffer center if preferred)
        gPlatXf.position[0] = spawnPos[0];
        gPlatXf.position[1] = spawnPos[1];
        gPlatXf.position[2] = spawnPos[2];
        gPlatXf.eulerRotation[0] = 0.0f;
        gPlatXf.eulerRotation[1] = 0.0f;
        gPlatXf.eulerRotation[2] = 0.0f;

        // Reset movement state
        gPlatTimer = 0;
        gPlatDir = 1.0f;
        gPlatYaw = 0.0f;
        gPlatShouldSpin = false;

        // Initial sync to libsm64
        sm64_surface_object_move(id, &gPlatXf);
    } else {
        printf("[PLATFORM] ERROR: Failed to spawn moving platform '%s'\n", name);
    }

    // Always reset buffers after successful spawn attempt
    reset_temp_surface_buffers();

    return id;
}


void pc_spawn_mario_test_collide(u32 name_ptr) {
    auto name_str = std::string(Ptr<String>(name_ptr).c()->data());
    
    std::string name = Ptr<String>(name_ptr).c()->data();

    // Special handling for persistent moving platform
    if (strcmp(name, "active-plat") == 0) {
        if (gPlatId == 0) {
            // Platform doesn't exist → spawn it using current buffer tris
            gPlatId = pc_mario_spawn_updated_tris_with_id(name);
        }
        // If it already exists, do nothing - we keep it alive and let update_moving_platform() control it
        return;
    }

    // All other objects: standard delete + respawn behavior (non-persistent debug objects)
    delete_surface_object_by_name(name);
    pc_mario_spawn_updated_tris(name);

    // Reset temp buffers only for non-platform objects
    reset_temp_surface_buffers();
}

void update_moving_platform() {
    if (gPlatId == 0) {
        // No platform active - nothing to update
        // Remove the fallback spawn here; spawning is now fully controlled by GOAL via pc_spawn_mario_test_collide
        return;
    }

    if (!g_platform_info_valid) {
        // No fresh data from GOAL yet → hold current position/rotation this frame
        return;
    }

    // Unpack position from GOAL fixed-point floats
    float targetX, targetY, targetZ;
    memcpy(&targetX, &g_platform_info.x_pos, sizeof(u32));
    memcpy(&targetY, &g_platform_info.y_pos, sizeof(u32));
    memcpy(&targetZ, &g_platform_info.z_pos, sizeof(u32));

    targetX *= METERS_TO_UNITS;
    targetY *= METERS_TO_UNITS;
    targetZ *= METERS_TO_UNITS;

    // Unpack base rotation (assuming these are already in degrees)
    float rotX, rotY, rotZ;
    memcpy(&rotX, &g_platform_info.rot_x, sizeof(u32));
    memcpy(&rotY, &g_platform_info.rot_y, sizeof(u32));
    memcpy(&rotZ, &g_platform_info.rot_z, sizeof(u32));

    // Apply to authoritative transform
    gPlatXf.position[0] = targetX;
    gPlatXf.position[1] = targetY;
    gPlatXf.position[2] = targetZ;

    gPlatXf.eulerRotation[0] = rotX;
    gPlatXf.eulerRotation[1] = rotY;
    gPlatXf.eulerRotation[2] = rotZ;

    // Optional spin-on-reverse logic (uncomment if you still want visual flip)
    /*
    static float lastDir = 1.0f;
    if (gPlatDir != lastDir) {
        gPlatShouldSpin = true;
        gPlatYaw = 0.0f;
        lastDir = gPlatDir;
    }
    if (gPlatShouldSpin) {
        gPlatYaw += gPlatSpinSpeed * (1.0f / 30.0f);
        if (gPlatYaw >= 180.0f) {
            gPlatYaw = 180.0f;
            gPlatShouldSpin = false;
        }
    }
    gPlatXf.eulerRotation[1] += gPlatYaw;
    */

    // Push transform to libsm64
    sm64_surface_object_move(gPlatId, &gPlatXf);

    // Optional internal timer if you want automatic back-and-forth (remove if fully GOAL-controlled)
    /*
    gPlatTimer++;
    if (gPlatTimer >= PLAT_MOVE_FRAMES) {
        gPlatTimer = 0;
        gPlatDir = -gPlatDir;
    }
    */
}


uint64_t pc_get_platform_x() {
  float x = gPlatXf.position[0];
  uint64_t out = 0;
  std::memcpy(&out, &x, sizeof(float));
  // printf("[pc_get_platform_x] X = %.2f -> 0x%lx\n", x, out);
  return out;
}

uint64_t pc_get_platform_y() {
  float y = gPlatXf.position[1];
  uint64_t out = 0;
  std::memcpy(&out, &y, sizeof(float));
  // printf("[pc_get_platform_y] Y = %.2f -> 0x%lx\n", y, out);
  return out;
}

uint64_t pc_get_platform_z() {
  float z = gPlatXf.position[2];
  uint64_t out = 0;
  std::memcpy(&out, &z, sizeof(float));
  // printf("[pc_get_platform_z] Z = %.2f -> 0x%lx\n", z, out);
  return out;
}

void update_platform_info_from_goal(u32 platform_info_ptr) {
  if (!platform_info_ptr) {
    printf("[PLATFORM] Warning: null pointer from GOAL\n");
    g_platform_info_valid = false;
    return;
  }

  auto info = Ptr<PlatformInfo>(platform_info_ptr).c();

  // Copy the raw bits into our persisted global
  g_platform_info.x_pos  = info->x_pos;
  g_platform_info.y_pos  = info->y_pos;
  g_platform_info.z_pos  = info->z_pos;
  g_platform_info.rot_x  = info->rot_x;
  g_platform_info.rot_y  = info->rot_y;
  g_platform_info.rot_z  = info->rot_z;

  g_platform_info_valid = true;

  // Debug print (remove later if not needed)
  float tx, ty, tz, rx, ry, rz;
  memcpy(&tx, &info->x_pos, sizeof(u32));
  memcpy(&ty, &info->y_pos, sizeof(u32));
  memcpy(&tz, &info->z_pos, sizeof(u32));
  memcpy(&rx, &info->rot_x, sizeof(u32));
  memcpy(&ry, &info->rot_y, sizeof(u32));
  memcpy(&rz, &info->rot_z, sizeof(u32));

  tx *= METERS_TO_UNITS;
  ty *= METERS_TO_UNITS;
  tz *= METERS_TO_UNITS;

  printf("[PLATFORM] Updated global from GOAL: pos(%.2f, %.2f, %.2f) rot(%.1f, %.1f, %.1f)\n",
         tx, ty, tz, rx, ry, rz);
}

// Pointer + count version – used for gSurfaceBuffer and other dynamic arrays
uint32_t spawn_surfaces_under_mario(
    const float*           marioPos,
    const SM64Surface*     surfaces,
    int                    surfaceCount,
    const char*            objectName,
    float                  y_offset)
{
    if (num_active_actors >= MAX_ACTORS || surfaceCount <= 0)
        return 0;

    SM64SurfaceObject obj{};
    memset(&obj, 0, sizeof(obj));

    MarioActorCollisionData& currActor = spawnedActors[num_active_actors++];

    currActor.name = objectName;

    const float* pos = marioPos ? marioPos : zero_pos_array;
    obj.transform.position[0] = pos[0];
    obj.transform.position[1] = pos[1] + y_offset;
    obj.transform.position[2] = pos[2];

    obj.surfaceCount = static_cast<uint32_t>(surfaceCount);

    obj.surfaces = (SM64Surface*)malloc(sizeof(SM64Surface) * surfaceCount);
    if (!obj.surfaces) {
        --num_active_actors;
        return 0;
    }

    memcpy(obj.surfaces, surfaces, sizeof(SM64Surface) * surfaceCount);

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

    currActor.pos[0] = (maxc[0] + minc[0]) * 0.5f;
    currActor.pos[1] = (maxc[1] + minc[1]) * 0.5f;
    currActor.pos[2] = (maxc[2] + minc[2]) * 0.5f;

    currActor.size = std::max({maxc[0] - minc[0], maxc[1] - minc[1], maxc[2] - minc[2]});

    uint32_t id = sm64_surface_object_create(&obj);

    currActor.id = id;
    currActor.surfaceObj = obj;

    return id;
}