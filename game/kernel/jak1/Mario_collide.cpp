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

    gSpawnedSurfaceCount++;
  }
 // printf("[pc_add_tris] EXIT\n");
}

void pc_spawn_mario_test_collide(u32 name_ptr) {
    auto name_str = std::string(Ptr<String>(name_ptr).c()->data());
    const char* name = name_str.c_str();

    //printf("[MarioSpawn] Called for '%s'\n", name);


    if (strcmp(name, "active-plat") == 0) {
      //name is active-plat check to see if it exists
      if (gPlatId == 0) {
        //platfor did not exist spawn it and update gPlatId
        //gPlatId = pc_mario_spawn_updated_tris_with_id(name);
        return;
      }




    }

        // Normal debug/test objects → delete + respawn + reset buffers
        //printf("[MarioSpawn] Normal object '%s' → delete + respawn + reset\n", name);
        delete_surface_object_by_name(name);
        pc_mario_spawn_updated_tris(name);

        // Only reset debug buffers for non-persistent objects
        gSpawnedSurfaceCount = 0;
        memset(gSurfaceBuffer, 0, sizeof(gSurfaceBuffer));
        memset(gTempVerts, 0, sizeof(gTempVerts));


   // printf("[MarioSpawn] Finished '%s'\n", name);
}

void update_moving_platform() {
  if (!gPlatId) {
    // Spawn if missing (fallback)
    float spawnPos[3] = {
        g_mario_state.position[0],
        g_mario_state.position[1] - 80.0f,
        g_mario_state.position[2]
    };
    //spawn_flat_platform_under_mario(spawnPos, 200.0f);
    return;  // wait until next frame for valid info
  }

  if (!g_platform_info_valid) {
    // No valid data yet → don't move/rotate this frame
    return;
  }

  // Unpack from global
  float targetX, targetY, targetZ;
  memcpy(&targetX, &g_platform_info.x_pos, sizeof(u32));
  memcpy(&targetY, &g_platform_info.y_pos, sizeof(u32));
  memcpy(&targetZ, &g_platform_info.z_pos, sizeof(u32));

  targetX *= METERS_TO_UNITS;
  targetY *= METERS_TO_UNITS;
  targetZ *= METERS_TO_UNITS;

  float baseRotX, baseRotY, baseRotZ;
  memcpy(&baseRotX, &g_platform_info.rot_x, sizeof(u32));
  memcpy(&baseRotY, &g_platform_info.rot_y, sizeof(u32));
  memcpy(&baseRotZ, &g_platform_info.rot_z, sizeof(u32));

  // Apply to physics transform
  gPlatXf.position[0] = targetX;
  gPlatXf.position[1] = targetY;
  gPlatXf.position[2] = targetZ;

  gPlatXf.eulerRotation[0] = baseRotX;
  gPlatXf.eulerRotation[1] = baseRotY;
  gPlatXf.eulerRotation[2] = baseRotZ;

  // ─── Your existing spin/flip logic ───
  // static float lastDir = 1.0f;
  // if (gPlatDir != lastDir) {
  //   gPlatShouldSpin = true;
  //   gPlatYaw = 0.0f;
  //   lastDir = gPlatDir;
  // }

  // if (gPlatShouldSpin) {
  //   gPlatYaw += gPlatSpinSpeed * (1.f / 30.f);
  //   if (gPlatYaw >= 180.0f) {
  //     gPlatYaw = 180.0f;
  //     gPlatShouldSpin = false;
  //   }
  // }

  // gPlatXf.eulerRotation[1] += gPlatYaw;

  // Apply to libsm64
  sm64_surface_object_move(gPlatId, &gPlatXf);

  // Internal timer (optional - remove if GOAL controls direction fully)
  // gPlatTimer++;
  // if (gPlatTimer >= PLAT_MOVE_FRAMES) {
  //   gPlatTimer = 0;
  //   gPlatDir = -gPlatDir;
  // }
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