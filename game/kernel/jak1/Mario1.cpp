#include "Mario1.h"
#include "game/graphics/opengl_renderer/MarioRenderer.h"
//#include "game/graphics/opengl_renderer/MarioRenderer2.h"
#include "common/util/FileUtil.h"
#include "game/kernel/common/kscheme.h"
#include "kscheme.h"

#include "game/kernel/common/kmachine.h"

//int variables here

// Near the top of Mario1.cpp, with other globals

static PlatformInfo g_platform_info = {0};  // persisted global - zero-initialized

// Optional: flag to know if we ever received valid data
static bool g_platform_info_valid = false;


static uint8_t* g_mario_texture = nullptr;
int marioId = -1;
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

  //setup the mario rom path, this should be changed later on but for now it suffices for development reasons
  //make sure that the libsm64 also reads from this same path so we dont need to copy the same rom into multiple places
  std::string baseDir = file_util::get_file_path({"iso_data"});
  std::string romPathStr = baseDir + "/mario/test.rom";
  const char* romPath = romPathStr.c_str();
  static uint8_t* romBuffer = nullptr;

  // Open ROM file
  std::ifstream file(romPath, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    //Lets just print and then crash here to make it easy for end user to give good error message
    fprintf(stderr, "Failed to open ROM file: %s\n", romPath);
    std::abort();
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

  //TODO load in static surfaces here we use village1 because of debug mode.
  // For now, the level collide is a giant static surface loaded at init. These files/objects have not been "reviewed" view at own risk
  sm64_static_surfaces_load(village1_surfaces, village1_surfaces_count);

  // Create Mario and print his ID
  marioId = sm64_mario_create(-7541.8, 3688.475, 9237.5);


  //oh boy audio init time
  sm64_audio_init(romBuffer);
  if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    fprintf(stderr, "[libsm64][ERROR] SDL_Init failed: %s\n", SDL_GetError());
    return -1;
  }
  audio_init();
  //This is "test" music to make sure audio is working
  //sm64_play_music(0, 0x80 | SEQ_LEVEL_SNOW, 0);
  //sm64_play_music(0, 0x05 | 0x80, 0);
  // This stuff is needed to avoid a null pointer dereference for some reason
  const int maxTris = SM64_GEO_MAX_TRIANGLES;
  g_geom.position = new float[3 * 3 * maxTris];
  g_geom.normal = new float[3 * 3 * maxTris];
  g_geom.color = new float[3 * 3 * maxTris];
  g_geom.uv = new float[2 * 3 * maxTris];
  memset(g_geom.position, 0, sizeof(float) * 3 * 3 * maxTris);
  memset(g_geom.normal, 0, sizeof(float) * 3 * 3 * maxTris);
  memset(g_geom.color, 0, sizeof(float) * 3 * 3 * maxTris);
  memset(g_geom.uv, 0, sizeof(float) * 2 * 3 * maxTris);
  g_geom.numTrianglesUsed = 0;

  delete[] romBuffer;
  return marioId;
}

// Mario frame updating stuff now yippie, this is the "main" loop on the mario side.
#define M_PI       3.14159265358979323846   // pi
// Convert Jak-style quaternion (x y z w) to yaw angle in degrees (rotation around Y)
// Returns yaw in [-180, 180] range
float quaternion_to_yaw_degrees(float qx, float qy, float qz, float qw) {
    // Standard quaternion to yaw (only care about Y rotation)
    float siny_cosp = 2.0f * (qw * qy + qz * qx);
    float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
    float yaw_rad = atan2f(siny_cosp, cosy_cosp);

    // Convert to degrees and normalize to [-180, 180]
    float yaw_deg = yaw_rad * (180.0f / M_PI);
    if (yaw_deg > 180.0f)  yaw_deg -= 360.0f;
    if (yaw_deg < -180.0f) yaw_deg += 360.0f;

    return yaw_deg;
}


int frame_num = 0;
int global_mario_frame_count = 0;
void update_platform_info_from_goal(u32 platform_info_ptr) {
    if (!platform_info_ptr) {
        g_platform_info_valid = false;
        return;
    }

    auto info = Ptr<PlatformInfo>(platform_info_ptr).c();

    // Copy all raw bits
    g_platform_info.x_pos  = info->x_pos;
    g_platform_info.y_pos  = info->y_pos;
    g_platform_info.z_pos  = info->z_pos;
    g_platform_info.rot_x  = info->rot_x;
    g_platform_info.rot_y  = info->rot_y;
    g_platform_info.rot_z  = info->rot_z;
    g_platform_info.rot_w  = info->rot_w;   // ← NEW

    g_platform_info_valid = true;

    // Debug: unpack and show real values
    float px, py, pz, qx, qy, qz, qw;
    memcpy(&px, &info->x_pos, sizeof(u32));
    memcpy(&py, &info->y_pos, sizeof(u32));
    memcpy(&pz, &info->z_pos, sizeof(u32));
    memcpy(&qx, &info->rot_x, sizeof(u32));
    memcpy(&qy, &info->rot_y, sizeof(u32));
    memcpy(&qz, &info->rot_z, sizeof(u32));
    memcpy(&qw, &info->rot_w, sizeof(u32));

    px *= METERS_TO_UNITS;
    py *= METERS_TO_UNITS;
    pz *= METERS_TO_UNITS;

    float yaw_deg = quaternion_to_yaw_degrees(qx, qy, qz, qw);

    printf("[PLATFORM] Updated from GOAL → pos(%.2f, %.2f, %.2f) quat(%.4f, %.4f, %.4f, %.4f) → yaw %.1f°\n",
           px, py, pz, qx, qy, qz, qw, yaw_deg);
}

bool ready_to_update_plat() {
  //this is how we determine if we should run mario for this frame
  // note that this COMPLETELY bypasses mario thread so NOTHING WILL UPDATE
  //if jakstate == pushed triangle
  //return fales; lets skip this render frame for mario as jak is in periscope
auto sym = jak1::intern_from_c("*ready-to-update-plat*");

if (sym->value == offset_of_s7()) {
  return false;
}

  return true;
}
void tick_mario_frame() {
  // This function is called every frame and controls updating the mario engine.
  // Revist this later once we have *some* mario collide to test, probably can /64 where we set .stickX instead and remove all this junk
  float scaled_stick_x = g_mario_inputs.stickX / 64.0f;
  float scaled_stick_y = g_mario_inputs.stickY / 64.0f;

  if (frame_num % 2 == 0 && marioId != -1) {
    SM64MarioInputs inputs = g_mario_inputs;
    inputs.stickX = scaled_stick_x;
    inputs.stickY = -scaled_stick_y;
    jak1::call_goal_function_by_name("update-sm64-camera-from-goal");
    update_moving_platform();
    sm64_mario_tick(marioId, &inputs, &g_mario_state, &g_geom);
    update_mario_collide();
    frame_num = 0;
  }
  frame_num++;
  global_mario_frame_count++;
}

void update_mario_collide(){
//This function contains the logic to update marios collide every frame

//first this we always make sure there is a fake floor under mario
update_psuedo_floor_under_mario();

jak1::call_goal_function_by_name("update-mario-water-height-from-goal");
//update the level surfaces near mario from goal if they changed.
jak1::call_goal_function_by_name("update-mario-loaded-surfaces");

//This updates the level geometery near mario from surfaces we currently have loaded
maybe_reload_surfaces(g_mario_state.position);

// //now we update all the "normal actor" collide mesh by deleting them and respawning them in their new location
// //this calls a function in GOAL called GOAL_NAME and when it returns we do stuff
// update_mario_actor_collide_mesh();

// not my favorite, but the only way I can currently find to make this not crash when called from cpp
// This call doesnt actually "run" the goal function like the other calls but instead it spawns a process that runs it.
// so if you stop/avoid this call mid run time the function will still execute.
// crashes on cutscenes unless called directly from goal
// if (global_mario_frame_count % 30 == 0) {
//   jak1::call_goal_function_by_name("load-goal-actor-collide-to-sm64-2");
// }

// //This is the final step, and we update special actors here by properly moving their collide in the mario engine (platforms, etc)
// //this calls a function in GOAL called GOAL_NAME and when it returns we do stuff
// update_mario_spec_actor_collide();

}

bool run_and_render_mario() {
  //this is how we determine if we should run mario for this frame
  // note that this COMPLETELY bypasses mario thread so NOTHING WILL UPDATE
  //if jakstate == pushed triangle
  //return fales; lets skip this render frame for mario as jak is in periscope
auto sym = jak1::intern_from_c("*run-mario-code*");

if (sym->value == offset_of_s7()) {
  return false;
}

  return true;
}

bool has_blue_eco() {
  // Run only once every X frames
  if ((global_mario_frame_count % 4) != 0) {
    return false;
  }

  // this is how we determine if we should run mario for this frame
  // note that this COMPLETELY bypasses mario thread so NOTHING WILL UPDATE
  // if jakstate == pushed triangle
  // return false; lets skip this render frame for mario as jak is in periscope
  auto sym = jak1::intern_from_c("*has-blue-eco*");

  if (sym->value == offset_of_s7()) {
    return false;
  }

  return true;
}

// ─────────────────────────────────────────────
// Moving platform config (Jak-style sliding floor)
#define PLAT_MOVE_FRAMES  120
#define PLAT_MOVE_SPEED   20.0f

// Globals for the moving platform
static uint32_t           gPlatId    = 0;
static SM64ObjectTransform gPlatXf   = {};  // Authoritative transform (libsm64 uses this)
static int32_t            gPlatTimer = 0;
static float              gPlatDir   = 1.0f;
static float gPlatYaw = 0.0f;           // current visual yaw rotation (degrees)
static float gPlatSpinSpeed = 90.0f;    // degrees per second when spinning
static bool gPlatShouldSpin = false;    // true when we just reversed

// Mario functions we call in GOAL
uint64_t pc_get_mario_action() {
  g_mario_state.action;

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

void pc_set_mario_music_from_goal(u32 music_bits) {
  u32 seq;
  memcpy(&seq, &music_bits, sizeof(u32));

  if (seq == 0) {
    sm64_fadeout_background_music(0, 10);
    return;
  }

  // Player 0 = background music
  sm64_seq_player_play_sequence(0, (uint8_t)seq, 0);
    sm64_surface_object_delete(gPlatId);
}



void pc_set_mario_position_from_goal(u32 x_bits, u32 y_bits, u32 z_bits) {
  float x, y, z;
  memcpy(&x, &x_bits, sizeof(u32));
  memcpy(&y, &y_bits, sizeof(u32));
  memcpy(&z, &z_bits, sizeof(u32));

  x *= METERS_TO_UNITS;
  y *= METERS_TO_UNITS;
  z *= METERS_TO_UNITS;
  sm64_set_mario_position(marioId, x, y, z);
}

void pc_set_mario_water_level_from_goal(u32 level_bits) {
  float level;
  memcpy(&level, &level_bits, sizeof(u32));

  level *= METERS_TO_UNITS;

  sm64_set_mario_water_level(marioId,
  true ? //  hardcode this func to always accept true bc we check on the goal side and only call this func if close to water
  level
   : INT16_MIN
  );
}

void pc_change_mario_state(u32 act_bits) {
  int act;
  memcpy(&act, &act_bits, sizeof(u32));
  sm64_set_mario_action(marioId, act);
}


// // const struct SM64Surface beach_surfaces[] = {
// //     {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{-5709,1433,-5201}, {-5564,1604,-5050}, {-5695,1687,-5018}}},
// //     {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{-5572,1832,-4936}, {-5695,1687,-5018}, {-5564,1604,-5050}}},
// //     {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{-5695,1687,-5018}, {-5572,1832,-4936}, {-5676,1884,-4989}}},
// //     {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{-5648,1979,-5029}, {-5676,1884,-4989}, {-5572,1832,-4936}}},
// //     {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{-5676,1884,-4989}, {-5648,1979,-5029}, {-5798,1803,-5049}}}
// // };

// void pc_add_tris_to_surface(u32 x_bits, u32 y_bits, u32 z_bits, u32 increment_num) {
//   // This function is called from GOAL to add a triangle to the surface list for debug drawing
//   float x, y, z, increment_num;
//   memcpy(&x, &x_bits, sizeof(u32));
//   memcpy(&y, &y_bits, sizeof(u32));
//   memcpy(&z, &z_bits, sizeof(u32));
//   memcpy(&increment_num, &increment_num, sizeof(u32));
//   x *= METERS_TO_UNITS;
//   y *= METERS_TO_UNITS;
//   z *= METERS_TO_UNITS;
//   // we need to define a global SM64Surface array and add to it here, if the increment_num is 1.0 we copy the positions into the first triangle, 2.0 into second triangle, etc once we fill the third triangle, we add to the next index

// }

#define MAX_DEBUG_SURFACES 1024 * 3

static struct SM64Surface gDebugSurfaces[MAX_DEBUG_SURFACES];
static int gDebugSurfaceCount = 0;

static int32_t gTempVerts[3][3];


static int gTempVertIndex = 0;

// DEBUGGING: Dump the current debug surfaces to a C file to view in the test program
void pc_dump_debug_surfaces_to_file(void) {
  const char* filename = "debug_surfaces.c";

  FILE* f = fopen(filename, "w");
  if (!f) {
    perror("pc_dump_debug_surfaces_to_file fopen failed");
    return;
  }

  fprintf(f, "const struct SM64Surface debug_surfaces[] = {\n");

  for (int i = 0; i < gDebugSurfaceCount; i++) {
    SM64Surface* s = &gDebugSurfaces[i];

    fprintf(
      f,
      "  {SURFACE_DEFAULT, %d, TERRAIN_STONE, {{%d,%d,%d}, {%d,%d,%d}, {%d,%d,%d}}},\n",
      s->force,
      s->vertices[0][0], s->vertices[0][1], s->vertices[0][2],
      s->vertices[1][0], s->vertices[1][1], s->vertices[1][2],
      s->vertices[2][0], s->vertices[2][1], s->vertices[2][2]
    );
  }

  fprintf(f, "};\n");

  fclose(f);

  // Reset buffers
  gDebugSurfaceCount = 0;
  memset(gDebugSurfaces, 0, sizeof(gDebugSurfaces));
  memset(gTempVerts, 0, sizeof(gTempVerts));

  printf("[pc_dump_debug_surfaces] wrote %d surfaces to %s\n",
         gDebugSurfaceCount, filename);
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
    if (gDebugSurfaceCount >= MAX_DEBUG_SURFACES) {
      printf("  ERROR: surface buffer full (%d), aborting\n",
             gDebugSurfaceCount);
      return;
    }
    struct SM64Surface* surf = &gDebugSurfaces[gDebugSurfaceCount];
    surf->type = SURFACE_DEFAULT;
    surf->force = 0;
    surf->terrain = TERRAIN_STONE;
    memcpy(surf->vertices, gTempVerts, sizeof(gTempVerts));

    gDebugSurfaceCount++;
  }
 // printf("[pc_add_tris] EXIT\n");
}





void pc_burn_marios_butt() { // does not shoot mario up as much as we'd like. it's a start
  if (g_mario_state.action != ACT_BURNING_GROUND && g_mario_state.action != ACT_BURNING_FALL && g_mario_state.action != ACT_BURNING_JUMP)
                sm64_set_mario_action(marioId, ACT_BURNING_JUMP);
                pc_dump_debug_surfaces_to_file();
}



struct Cuben {
    float pos[3];
    float size;
    SM64SurfaceObject surfaceObj;
    const char* name;
    uint32_t id;
};

Cuben spawnedCubes[MAX_CUBES];
int numCubes = 0;

static const struct SM64Surface psuedo_floor_surfaces[] = {
    {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{-100, 0, 100}, {100, 0, 100}, {100, 0, -100}}},
    {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{100, 0, -100}, {-100, 0, -100}, {-100, 0, 100}}}
};


template <size_t N>
uint32_t spawn_surfaces_under_mario(
    const float* marioPos,
    const SM64Surface (&surfaces)[N], // Array passed by reference (size N is deduced)
    const char* objectName,
    float y_offset = 0.0f
) {
    if (numCubes >= MAX_CUBES) return 0;

    SM64SurfaceObject obj;
    memset(&obj, 0, sizeof(SM64SurfaceObject));

    Cuben& c = spawnedCubes[numCubes++];

    // Store the object name
    c.name = objectName; // <-- STORE NAME
    // Set object transform (position) - this is the object's origin
    obj.transform.position[0] = marioPos[0];
    obj.transform.position[1] = marioPos[1] + y_offset;
    obj.transform.position[2] = marioPos[2];

    // Use the deduced size N directly
    obj.surfaceCount = N;

    // Allocate memory for the surfaces and copy the data
    obj.surfaces = (SM64Surface*)malloc(sizeof(SM64Surface) * obj.surfaceCount);
    if (!obj.surfaces) {
        numCubes--;
        return 0;
    }

    // Copy the input array data
    memcpy(obj.surfaces, surfaces, sizeof(SM64Surface) * obj.surfaceCount);

    // Calculate a rough bounding box center and size for visualization/debugging (optional, but good practice)
    float min_coords[3] = {1e9f, 1e9f, 1e9f};
    float max_coords[3] = {-1e9f, -1e9f, -1e9f};

    for (size_t i = 0; i < N; ++i) {
        for (int j = 0; j < 3; ++j) { // Vertices
            for (int k = 0; k < 3; ++k) { // XYZ coordinates
                float v = (float)surfaces[i].vertices[j][k];
                if (v < min_coords[k]) min_coords[k] = v;
                if (v > max_coords[k]) max_coords[k] = v;
            }
        }
    }

    // Set Cube pos/size based on world coordinates of the array for drawing/tracking
    c.pos[0] = (max_coords[0] + min_coords[0]) / 2.0f;
    c.pos[1] = (max_coords[1] + min_coords[1]) / 2.0f;
    c.pos[2] = (max_coords[2] + min_coords[2]) / 2.0f;

    c.size = fmaxf(fmaxf(max_coords[0] - min_coords[0], max_coords[1] - min_coords[1]), max_coords[2] - min_coords[2]);

    uint32_t id = sm64_surface_object_create(&obj);


c.id = id;
c.surfaceObj = obj;
    return id;
}


void delete_surface_object_by_name(const char* name) {
    if (!name || numCubes == 0) return;

    int index_to_remove = -1;

    // 1. Find the object by name
    for (int i = 0; i < numCubes; ++i) {
        const Cuben& c = spawnedCubes[i];
        // Check if the object has a name and if it matches the requested name
        if (c.name != NULL && strcmp(c.name, name) == 0) {
            index_to_remove = i;
            break; // Found the object, stop searching
        }
    }

    if (index_to_remove != -1) {
        Cuben& c = spawnedCubes[index_to_remove];

        // 2. De-register the collision object using the stored ID
        sm64_surface_object_delete(c.id);

        // 3. Free the surface memory that was malloc'd in spawn_surfaces_under_mario
        if (c.surfaceObj.surfaces != NULL) {
            free(c.surfaceObj.surfaces);
            c.surfaceObj.surfaces = NULL;
        }

        // 4. Remove the object from the tracking array (Swap-and-Pop)

        // Decrement the total count
        numCubes--;

        // If the object being removed wasn't the last one,
        // copy the last Cube structure over the one we are removing.
        if (index_to_remove < numCubes) {
            spawnedCubes[index_to_remove] = spawnedCubes[numCubes];
        }

        // The object is now removed from the array and its collision is deleted.
    }
}

static int surface_spawn_count = 0;
void pc_mario_spawn_updated_tris(const char* name){
//this is called from goal after we finish updating gDebugSurfaces to push them into the mario engine

//first step is to spawn the surfaces

                const char* objectName;
                objectName = name; //money-32


                //spawn_surfaces_under_mario(g_mario_state.position, beach_surfaces, objectName);
                const float zero_pos_array[3] = {0.0f, 0.0f, 0.0f};
                spawn_surfaces_under_mario(zero_pos_array, gDebugSurfaces, objectName); // Offset crate above beach
                surface_spawn_count++; // Increment counter
                  // Reset buffers
  gDebugSurfaceCount = 0;
  memset(gDebugSurfaces, 0, sizeof(gDebugSurfaces));
  memset(gTempVerts, 0, sizeof(gTempVerts));

}

void update_psuedo_floor_under_mario(){
const char* floorName;
            floorName = "floor";
            delete_surface_object_by_name("floor");
            spawn_surfaces_under_mario(g_mario_state.position,psuedo_floor_surfaces, floorName, -300.0f);

}



uint32_t spawn_flat_platform_under_mario(const float* marioPos, float size = 600.0f)
{
    if (numCubes >= MAX_CUBES) return 0;
    SM64SurfaceObject obj = {};
    float half = size / 2.0f;
    obj.transform.position[0] = marioPos[0];
    obj.transform.position[1] = marioPos[1];
    obj.transform.position[2] = marioPos[2];
    obj.surfaceCount = 2;
    obj.surfaces = (SM64Surface*)malloc(sizeof(SM64Surface) * 2);

    #define ADD_TRI(i, ax,ay,az, bx,by,bz, cx,cy,cz) do { \
        obj.surfaces[i].vertices[0][0] = ax; obj.surfaces[i].vertices[0][1] = ay; obj.surfaces[i].vertices[0][2] = az; \
        obj.surfaces[i].vertices[1][0] = bx; obj.surfaces[i].vertices[1][1] = by; obj.surfaces[i].vertices[1][2] = bz; \
        obj.surfaces[i].vertices[2][0] = cx; obj.surfaces[i].vertices[2][1] = cy; obj.surfaces[i].vertices[2][2] = cz; \
        obj.surfaces[i].type = SURFACE_DEFAULT; \
        obj.surfaces[i].force = 0; \
        obj.surfaces[i].terrain = TERRAIN_STONE; \
    } while(0)

    float x0 = -half, x1 = half;
    float z0 = -half, z1 = half;
    float y = 0.0f;
    ADD_TRI(0, x0,y,z1, x1,y,z1, x1,y,z0);
    ADD_TRI(1, x1,y,z0, x0,y,z0, x0,y,z1);
    #undef ADD_TRI

    uint32_t id = sm64_surface_object_create(&obj);
    // ────────────── REMOVE THIS LINE ──────────────
    // free(obj.surfaces);   // ← DELETE or COMMENT OUT

    if (!id) return 0;

    gPlatXf.position[0] = obj.transform.position[0];
    gPlatXf.position[1] = obj.transform.position[1];
    gPlatXf.position[2] = obj.transform.position[2];
    gPlatXf.eulerRotation[0] = obj.transform.eulerRotation[0];
    gPlatXf.eulerRotation[1] = obj.transform.eulerRotation[1];
    gPlatXf.eulerRotation[2] = obj.transform.eulerRotation[2];

    gPlatId = id;
    gPlatTimer = 0;
    gPlatDir = 1.0f;

    Cuben& c = spawnedCubes[numCubes++];
    c.pos[0] = marioPos[0];
    c.pos[1] = marioPos[1];
    c.pos[2] = marioPos[2];
    c.size = size;
    c.name = "MovingPlat";
    c.id = id;
    c.surfaceObj = obj;           // now safe – surfaces still valid

    return id;
}





// ─────────────────────────────────────────────
// Moving platform logic
// ─────────────────────────────────────────────
void update_moving_platform() {
    if (!gPlatId) {
        // Fallback: spawn under Mario if platform doesn't exist yet
        float spawnPos[3] = {
            g_mario_state.position[0],
            g_mario_state.position[1] - 80.0f,  // foot height offset
            g_mario_state.position[2]
        };
        spawn_flat_platform_under_mario(spawnPos, 2000.0f);
        return;  // wait for next frame
    }

    if (!g_platform_info_valid) {
        // No valid GOAL data yet → skip this frame
        return;
    }

    // ─────────────────────────────────────────────
    // Unpack position from global (GOAL → libsm64 meters)
    // ─────────────────────────────────────────────
    float targetX, targetY, targetZ;
    memcpy(&targetX, &g_platform_info.x_pos, sizeof(u32));
    memcpy(&targetY, &g_platform_info.y_pos, sizeof(u32));
    memcpy(&targetZ, &g_platform_info.z_pos, sizeof(u32));

    targetX *= METERS_TO_UNITS;
    targetY *= METERS_TO_UNITS;
    targetZ *= METERS_TO_UNITS;

    // ─────────────────────────────────────────────
    // Unpack quaternion and convert to yaw
    // ─────────────────────────────────────────────
    float qx, qy, qz, qw;
    memcpy(&qx, &g_platform_info.rot_x, sizeof(u32));
    memcpy(&qy, &g_platform_info.rot_y, sizeof(u32));
    memcpy(&qz, &g_platform_info.rot_z, sizeof(u32));
    memcpy(&qw, &g_platform_info.rot_w, sizeof(u32));

    // Normalize quaternion if needed (Jak usually normalizes, but to be safe)
    float mag = sqrtf(qx*qx + qy*qy + qz*qz + qw*qw);
    if (mag > 0.0001f) {
        qx /= mag; qy /= mag; qz /= mag; qw /= mag;
    } else {
        qw = 1.0f;  // fallback to identity
    }

    float yaw_deg = quaternion_to_yaw_degrees(qx, qy, qz, qw);

    // Debug: show what we're applying
    printf("[PLATFORM] Applying → pos(%.2f, %.2f, %.2f) yaw=%.1f° (from quat %.4f,%.4f,%.4f,%.4f)\n",
           targetX, targetY, targetZ, yaw_deg, qx, qy, qz, qw);

    // ─────────────────────────────────────────────
    // Apply to libsm64 physics transform
    // ─────────────────────────────────────────────
    gPlatXf.position[0] = targetX;
    gPlatXf.position[1] = targetY;
    gPlatXf.position[2] = targetZ;

    gPlatXf.eulerRotation[0] = 0.0f;     // pitch = 0 (Jak platforms rarely pitch)
    gPlatXf.eulerRotation[1] = -yaw_deg;  // yaw from quaternion
    gPlatXf.eulerRotation[2] = 0.0f;     // roll = 0

    // Optional: your old spin/flip logic (disabled since GOAL now sends real rotation)
    // static float lastDir = 1.0f;
    // if (gPlatDir != lastDir) {
    //     gPlatShouldSpin = true;
    //     gPlatYaw = 0.0f;
    //     lastDir = gPlatDir;
    // }
    // if (gPlatShouldSpin) {
    //     gPlatYaw += gPlatSpinSpeed * (1.f / 30.f);
    //     if (gPlatYaw >= 180.0f) {
    //         gPlatYaw = 180.0f;
    //         gPlatShouldSpin = false;
    //     }
    // }
    // gPlatXf.eulerRotation[1] += gPlatYaw;

    // Push to libsm64 — Mario should now stick and rotate with the platform
    sm64_surface_object_move(gPlatId, &gPlatXf);

    // ─────────────────────────────────────────────
    // Sync visual Cube for rendering + name label
    // ─────────────────────────────────────────────
    for (int i = 0; i < numCubes; i++) {
        if (spawnedCubes[i].id == gPlatId) {
            Cuben& c = spawnedCubes[i];

            c.pos[0] = gPlatXf.position[0];
            c.pos[1] = gPlatXf.position[1];
            c.pos[2] = gPlatXf.position[2];

            c.surfaceObj.transform.position[0] = gPlatXf.position[0];
            c.surfaceObj.transform.position[1] = gPlatXf.position[1];
            c.surfaceObj.transform.position[2] = gPlatXf.position[2];

            c.surfaceObj.transform.eulerRotation[0] = gPlatXf.eulerRotation[0];
            c.surfaceObj.transform.eulerRotation[1] = gPlatXf.eulerRotation[1];
            c.surfaceObj.transform.eulerRotation[2] = gPlatXf.eulerRotation[2];

            break;
        }
    }

    // Optional internal timer (remove if GOAL fully controls direction)
    // gPlatTimer++;
    // if (gPlatTimer >= PLAT_MOVE_FRAMES) {
    //     gPlatTimer = 0;
    //     gPlatDir = -gPlatDir;
    // }
}
void destroy_moving_platform_under_mario(){
const char* floorName;
            floorName = "floor";
            delete_surface_object_by_name("MovingPlat");
            //spawn_flat_platform_under_mario(zero_pos_array, 200.0f);
            //MovingPlat

}

void pc_spawn_mario_test_collide(u32 name_ptr) {
    // Convert the u32 pointer to the C++ string (like in pc_filepath_exists)
    auto name_str = std::string(Ptr<String>(name_ptr).c()->data());
    const char* name = name_str.c_str();

    // first we delete the old surface object if it exists
    delete_surface_object_by_name(name);

    // then we spawn in the new one
    pc_mario_spawn_updated_tris(name);


// this is called after we finish uploading triangles to mario enginge and it spawns the object and resets the debug buffers
}


void pc_mario_says_so_long_gay_bowsa(uint32_t music_bits) {
    uint32_t seq;
    std::memcpy(&seq, &music_bits, sizeof(uint32_t));

    switch (seq) {
        case 0x01: sm64_play_sound_global(SOUND_MARIO_YAH_WAH_HOO); break;
        case 0x02: sm64_play_sound_global(SOUND_MARIO_HOOHOO); break;
        case 0x03: sm64_play_sound_global(SOUND_MARIO_YAHOO); break;
        case 0x04: sm64_play_sound_global(SOUND_MARIO_UH); break;
        case 0x05: sm64_play_sound_global(SOUND_MARIO_HRMM); break;
        case 0x06: sm64_play_sound_global(SOUND_MARIO_WAH2); break;
        case 0x07: sm64_play_sound_global(SOUND_MARIO_WHOA); break;
        case 0x08: sm64_play_sound_global(SOUND_MARIO_EEUH); break;
        case 0x09: sm64_play_sound_global(SOUND_MARIO_ATTACKED); break;
        case 0x0A: sm64_play_sound_global(SOUND_MARIO_OOOF); break;
        case 0x0B: sm64_play_sound_global(SOUND_MARIO_OOOF2); break;
        case 0x0C: sm64_play_sound_global(SOUND_MARIO_HERE_WE_GO); break;
        case 0x0D: sm64_play_sound_global(SOUND_MARIO_YAWNING); break;
        case 0x0E: sm64_play_sound_global(SOUND_MARIO_SNORING1); break;
        case 0x0F: sm64_play_sound_global(SOUND_MARIO_SNORING2); break;
        case 0x10: sm64_play_sound_global(SOUND_MARIO_WAAAOOOW); break;
        case 0x11: sm64_play_sound_global(SOUND_MARIO_HAHA); break;
        case 0x12: sm64_play_sound_global(SOUND_MARIO_HAHA_2); break;
        case 0x13: sm64_play_sound_global(SOUND_MARIO_UH2); break;
        case 0x14: sm64_play_sound_global(SOUND_MARIO_UH2_2); break;
        case 0x15: sm64_play_sound_global(SOUND_MARIO_ON_FIRE); break;
        case 0x16: sm64_play_sound_global(SOUND_MARIO_DYING); break;
        case 0x17: sm64_play_sound_global(SOUND_MARIO_PANTING_COLD); break;
        case 0x18: sm64_play_sound_global(SOUND_MARIO_PANTING); break;
        case 0x19: sm64_play_sound_global(SOUND_MARIO_COUGHING1); break;
        case 0x1A: sm64_play_sound_global(SOUND_MARIO_COUGHING2); break;
        case 0x1B: sm64_play_sound_global(SOUND_MARIO_COUGHING3); break;
        case 0x1C: sm64_play_sound_global(SOUND_MARIO_PUNCH_YAH); break;
        case 0x1D: sm64_play_sound_global(SOUND_MARIO_PUNCH_HOO); break;
        case 0x1E: sm64_play_sound_global(SOUND_MARIO_MAMA_MIA); break;
        case 0x1F: sm64_play_sound_global(SOUND_MARIO_OKEY_DOKEY); break;
        case 0x20: sm64_play_sound_global(SOUND_MARIO_GROUND_POUND_WAH); break;
        case 0x21: sm64_play_sound_global(SOUND_MARIO_DROWNING); break;
        case 0x22: sm64_play_sound_global(SOUND_MARIO_PUNCH_WAH); break;
        case 0x23: sm64_play_sound_global(SOUND_PEACH_DEAR_MARIO); break;
        case 0x24: sm64_play_sound_global(SOUND_MARIO_YAHOO_WAHA_YIPPEE); break;
        case 0x25: sm64_play_sound_global(SOUND_MARIO_DOH); break;
        case 0x26: sm64_play_sound_global(SOUND_MARIO_GAME_OVER); break;
        case 0x27: sm64_play_sound_global(SOUND_MARIO_HELLO); break;
        case 0x28: sm64_play_sound_global(SOUND_MARIO_PRESS_START_TO_PLAY); break;
        case 0x29: sm64_play_sound_global(SOUND_MARIO_TWIRL_BOUNCE); break;
        case 0x2A: sm64_play_sound_global(SOUND_MARIO_SNORING3); break;
        case 0x2B: sm64_play_sound_global(SOUND_MARIO_SO_LONGA_BOWSER); break;
        case 0x2C: sm64_play_sound_global(SOUND_MARIO_IMA_TIRED); break;
        case 0x2D: sm64_play_sound_global(SOUND_PEACH_MARIO); break;
        case 0x2E: sm64_play_sound_global(SOUND_PEACH_POWER_OF_THE_STARS); break;
        case 0x2F: sm64_play_sound_global(SOUND_PEACH_THANKS_TO_YOU); break;
        case 0x30: sm64_play_sound_global(SOUND_PEACH_THANK_YOU_MARIO); break;
        case 0x31: sm64_play_sound_global(SOUND_PEACH_SOMETHING_SPECIAL); break;
        case 0x32: sm64_play_sound_global(SOUND_PEACH_BAKE_A_CAKE); break;
        case 0x33: sm64_play_sound_global(SOUND_PEACH_FOR_MARIO); break;
        case 0x34: sm64_play_sound_global(SOUND_PEACH_MARIO2); break;
        case 0x35: sm64_play_sound_global(SOUND_GENERAL_ACTIVATE_CAP_SWITCH); break;
        case 0x36: sm64_play_sound_global(SOUND_GENERAL_FLAME_OUT); break;
        case 0x37: sm64_play_sound_global(SOUND_GENERAL_OPEN_WOOD_DOOR); break;
        case 0x38: sm64_play_sound_global(SOUND_GENERAL_CLOSE_WOOD_DOOR); break;
        case 0x39: sm64_play_sound_global(SOUND_GENERAL_OPEN_IRON_DOOR); break;
        case 0x3A: sm64_play_sound_global(SOUND_GENERAL_CLOSE_IRON_DOOR); break;
        case 0x3B: sm64_play_sound_global(SOUND_GENERAL_BUBBLES); break;
        case 0x3C: sm64_play_sound_global(SOUND_GENERAL_MOVING_WATER); break;
        case 0x3D: sm64_play_sound_global(SOUND_GENERAL_SWISH_WATER); break;
        case 0x3E: sm64_play_sound_global(SOUND_GENERAL_QUIET_BUBBLE); break;
        case 0x3F: sm64_play_sound_global(SOUND_GENERAL_VOLCANO_EXPLOSION); break;
        case 0x40: sm64_play_sound_global(SOUND_GENERAL_QUIET_BUBBLE2); break;
        case 0x41: sm64_play_sound_global(SOUND_GENERAL_CASTLE_TRAP_OPEN); break;
        case 0x42: sm64_play_sound_global(SOUND_GENERAL_WALL_EXPLOSION); break;
        case 0x43: sm64_play_sound_global(SOUND_GENERAL_COIN); break;
        case 0x44: sm64_play_sound_global(SOUND_GENERAL_COIN_WATER); break;
        case 0x45: sm64_play_sound_global(SOUND_GENERAL_SHORT_STAR); break;
        case 0x46: sm64_play_sound_global(SOUND_GENERAL_BIG_CLOCK); break;
        case 0x47: sm64_play_sound_global(SOUND_GENERAL_LOUD_POUND); break;
        case 0x48: sm64_play_sound_global(SOUND_GENERAL_LOUD_POUND2); break;
        case 0x49: sm64_play_sound_global(SOUND_GENERAL_SHORT_POUND1); break;
        case 0x4A: sm64_play_sound_global(SOUND_GENERAL_SHORT_POUND2); break;
        case 0x4B: sm64_play_sound_global(SOUND_GENERAL_SHORT_POUND3); break;
        case 0x4C: sm64_play_sound_global(SOUND_GENERAL_SHORT_POUND4); break;
        case 0x4D: sm64_play_sound_global(SOUND_GENERAL_SHORT_POUND5); break;
        case 0x4E: sm64_play_sound_global(SOUND_GENERAL_SHORT_POUND6); break;
        case 0x4F: sm64_play_sound_global(SOUND_GENERAL_OPEN_CHEST); break;
        case 0x50: sm64_play_sound_global(SOUND_GENERAL_CLAM_SHELL1); break;
        case 0x51: sm64_play_sound_global(SOUND_GENERAL_BOX_LANDING); break;
        case 0x52: sm64_play_sound_global(SOUND_GENERAL_BOX_LANDING_2); break;
        case 0x53: sm64_play_sound_global(SOUND_GENERAL_UNKNOWN1); break;
        case 0x54: sm64_play_sound_global(SOUND_GENERAL_UNKNOWN1_2); break;
        case 0x55: sm64_play_sound_global(SOUND_GENERAL_CLAM_SHELL2); break;
        case 0x56: sm64_play_sound_global(SOUND_GENERAL_CLAM_SHELL3); break;
        case 0x57: sm64_play_sound_global(SOUND_GENERAL_PAINTING_EJECT); break;
        case 0x58: sm64_play_sound_global(SOUND_GENERAL_PAINTING_EJECT); break;
        case 0x59: sm64_play_sound_global(SOUND_GENERAL_LEVEL_SELECT_CHANGE); break;
        case 0x5A: sm64_play_sound_global(SOUND_GENERAL_PLATFORM); break;
        case 0x5B: sm64_play_sound_global(SOUND_GENERAL_DONUT_PLATFORM_EXPLOSION); break;
        case 0x5C: sm64_play_sound_global(SOUND_GENERAL_BOWSER_BOMB_EXPLOSION); break;
        case 0x5D: sm64_play_sound_global(SOUND_GENERAL_COIN_SPURT); break;
        case 0x5E: sm64_play_sound_global(SOUND_GENERAL_COIN_SPURT_2); break;
        case 0x5F: sm64_play_sound_global(SOUND_GENERAL_COIN_SPURT_EU); break;
        case 0x60: sm64_play_sound_global(SOUND_GENERAL_EXPLOSION6); break;
        case 0x61: sm64_play_sound_global(SOUND_GENERAL_UNK32); break;
        case 0x62: sm64_play_sound_global(SOUND_GENERAL_BOAT_TILT1); break;
        case 0x63: sm64_play_sound_global(SOUND_GENERAL_BOAT_TILT2); break;
        case 0x64: sm64_play_sound_global(SOUND_GENERAL_COIN_DROP); break;
        case 0x65: sm64_play_sound_global(SOUND_GENERAL_UNKNOWN3_LOWPRIO); break;
        case 0x66: sm64_play_sound_global(SOUND_GENERAL_UNKNOWN3); break;
        case 0x67: sm64_play_sound_global(SOUND_GENERAL_UNKNOWN3_2); break;
        case 0x68: sm64_play_sound_global(SOUND_GENERAL_PENDULUM_SWING); break;
        case 0x69: sm64_play_sound_global(SOUND_GENERAL_CHAIN_CHOMP1); break;
        case 0x6A: sm64_play_sound_global(SOUND_GENERAL_CHAIN_CHOMP2); break;
        case 0x6B: sm64_play_sound_global(SOUND_GENERAL_DOOR_TURN_KEY); break;
        case 0x6C: sm64_play_sound_global(SOUND_GENERAL_MOVING_IN_SAND); break;
        case 0x6D: sm64_play_sound_global(SOUND_GENERAL_UNKNOWN4_LOWPRIO); break;
        case 0x6E: sm64_play_sound_global(SOUND_GENERAL_UNKNOWN4); break;
        case 0x6F: sm64_play_sound_global(SOUND_GENERAL_MOVING_PLATFORM_SWITCH); break;
        case 0x70: sm64_play_sound_global(SOUND_GENERAL_CAGE_OPEN); break;
        case 0x71: sm64_play_sound_global(SOUND_GENERAL_QUIET_POUND1_LOWPRIO); break;
        case 0x72: sm64_play_sound_global(SOUND_GENERAL_QUIET_POUND1); break;
        case 0x73: sm64_play_sound_global(SOUND_GENERAL_BREAK_BOX); break;
        case 0x74: sm64_play_sound_global(SOUND_GENERAL_DOOR_INSERT_KEY); break;
        case 0x75: sm64_play_sound_global(SOUND_GENERAL_QUIET_POUND2); break;
        case 0x76: sm64_play_sound_global(SOUND_GENERAL_BIG_POUND); break;
        case 0x77: sm64_play_sound_global(SOUND_GENERAL_UNK45); break;
        case 0x78: sm64_play_sound_global(SOUND_GENERAL_UNK46_LOWPRIO); break;
        case 0x79: sm64_play_sound_global(SOUND_GENERAL_UNK46); break;
        case 0x7A: sm64_play_sound_global(SOUND_GENERAL_CANNON_UP); break;
        case 0x7B: sm64_play_sound_global(SOUND_GENERAL_GRINDEL_ROLL); break;
        case 0x7C: sm64_play_sound_global(SOUND_GENERAL_EXPLOSION7); break;
        case 0x7D: sm64_play_sound_global(SOUND_GENERAL_SHAKE_COFFIN); break;
        case 0x7E: sm64_play_sound_global(SOUND_GENERAL_RACE_GUN_SHOT); break;
        case 0x7F: sm64_play_sound_global(SOUND_GENERAL_STAR_DOOR_OPEN); break;
        case 0x80: sm64_play_sound_global(SOUND_GENERAL_STAR_DOOR_CLOSE); break;
        case 0x81: sm64_play_sound_global(SOUND_GENERAL_POUND_ROCK); break;
        case 0x82: sm64_play_sound_global(SOUND_GENERAL_STAR_APPEARS); break;
        case 0x83: sm64_play_sound_global(SOUND_GENERAL_COLLECT_1UP); break;
        case 0x84: sm64_play_sound_global(SOUND_GENERAL_BUTTON_PRESS_LOWPRIO); break;
        case 0x85: sm64_play_sound_global(SOUND_GENERAL_BUTTON_PRESS); break;
        case 0x86: sm64_play_sound_global(SOUND_GENERAL_BUTTON_PRESS_2_LOWPRIO); break;
        case 0x87: sm64_play_sound_global(SOUND_GENERAL_BUTTON_PRESS_2); break;
        case 0x88: sm64_play_sound_global(SOUND_GENERAL_ELEVATOR_MOVE); break;
        case 0x89: sm64_play_sound_global(SOUND_GENERAL_ELEVATOR_MOVE_2); break;
        case 0x8A: sm64_play_sound_global(SOUND_GENERAL_SWISH_AIR); break;
        case 0x8B: sm64_play_sound_global(SOUND_GENERAL_SWISH_AIR_2); break;
        case 0x8C: sm64_play_sound_global(SOUND_GENERAL_HAUNTED_CHAIR); break;
        case 0x8D: sm64_play_sound_global(SOUND_GENERAL_SOFT_LANDING); break;
        case 0x8E: sm64_play_sound_global(SOUND_GENERAL_HAUNTED_CHAIR_MOVE); break;
        case 0x8F: sm64_play_sound_global(SOUND_GENERAL_BOWSER_PLATFORM); break;
        case 0x90: sm64_play_sound_global(SOUND_GENERAL_BOWSER_PLATFORM_2); break;
        case 0x91: sm64_play_sound_global(SOUND_GENERAL_HEART_SPIN); break;
        case 0x92: sm64_play_sound_global(SOUND_GENERAL_POUND_WOOD_POST); break;
        case 0x93: sm64_play_sound_global(SOUND_GENERAL_WATER_LEVEL_TRIG); break;
        case 0x94: sm64_play_sound_global(SOUND_GENERAL_SWITCH_DOOR_OPEN); break;
        case 0x95: sm64_play_sound_global(SOUND_GENERAL_RED_COIN); break;
        case 0x96: sm64_play_sound_global(SOUND_GENERAL_BIRDS_FLY_AWAY); break;
        case 0x97: sm64_play_sound_global(SOUND_GENERAL_METAL_POUND); break;
        case 0x98: sm64_play_sound_global(SOUND_GENERAL_BOING1); break;
        case 0x99: sm64_play_sound_global(SOUND_GENERAL_BOING2_LOWPRIO); break;
        case 0x9A: sm64_play_sound_global(SOUND_GENERAL_BOING2); break;
        case 0x9B: sm64_play_sound_global(SOUND_GENERAL_YOSHI_WALK); break;
        case 0x9C: sm64_play_sound_global(SOUND_GENERAL_ENEMY_ALERT1); break;
        case 0x9D: sm64_play_sound_global(SOUND_GENERAL_YOSHI_TALK); break;
        case 0x9E: sm64_play_sound_global(SOUND_GENERAL_SPLATTERING); break;
        case 0x9F: sm64_play_sound_global(SOUND_GENERAL_BOING3); break;
        case 0xA0: sm64_play_sound_global(SOUND_GENERAL_GRAND_STAR); break;
        case 0xA1: sm64_play_sound_global(SOUND_GENERAL_GRAND_STAR_JUMP); break;
        case 0xA2: sm64_play_sound_global(SOUND_GENERAL_BOAT_ROCK); break;
        case 0xA3: sm64_play_sound_global(SOUND_GENERAL_VANISH_SFX); break;
        case 0xA4: sm64_play_sound_global(SOUND_ENV_WATERFALL1); break;
        case 0xA5: sm64_play_sound_global(SOUND_ENV_WATERFALL2); break;
        case 0xA6: sm64_play_sound_global(SOUND_ENV_ELEVATOR1); break;
        case 0xA7: sm64_play_sound_global(SOUND_ENV_DRONING1); break;
        case 0xA8: sm64_play_sound_global(SOUND_ENV_DRONING2); break;
        case 0xA9: sm64_play_sound_global(SOUND_ENV_WIND1); break;
        case 0xAA: sm64_play_sound_global(SOUND_ENV_MOVING_SAND_SNOW); break;
        case 0xAB: sm64_play_sound_global(SOUND_ENV_UNK07); break;
        case 0xAC: sm64_play_sound_global(SOUND_ENV_ELEVATOR2); break;
        case 0xAD: sm64_play_sound_global(SOUND_ENV_WATER); break;
        case 0xAE: sm64_play_sound_global(SOUND_ENV_UNKNOWN2); break;
        case 0xAF: sm64_play_sound_global(SOUND_ENV_BOAT_ROCKING1); break;
        case 0xB0: sm64_play_sound_global(SOUND_ENV_ELEVATOR3); break;
        case 0xB1: sm64_play_sound_global(SOUND_ENV_ELEVATOR4); break;
        case 0xB2: sm64_play_sound_global(SOUND_ENV_ELEVATOR4_2); break;
        case 0xB3: sm64_play_sound_global(SOUND_ENV_MOVINGSAND); break;
        case 0xB4: sm64_play_sound_global(SOUND_ENV_MERRY_GO_ROUND_CREAKING); break;
        case 0xB5: sm64_play_sound_global(SOUND_ENV_WIND2); break;
        case 0xB6: sm64_play_sound_global(SOUND_ENV_UNK12); break;
        case 0xB7: sm64_play_sound_global(SOUND_ENV_SLIDING); break;
        case 0xB8: sm64_play_sound_global(SOUND_ENV_STAR); break;
        case 0xB9: sm64_play_sound_global(SOUND_ENV_UNKNOWN4); break;
        case 0xBA: sm64_play_sound_global(SOUND_ENV_WATER_DRAIN); break;
        case 0xBB: sm64_play_sound_global(SOUND_ENV_METAL_BOX_PUSH); break;
        case 0xBC: sm64_play_sound_global(SOUND_ENV_SINK_QUICKSAND); break;
        case 0xBD: sm64_play_sound_global(SOUND_OBJ_SUSHI_SHARK_WATER_SOUND); break;
        case 0xBE: sm64_play_sound_global(SOUND_OBJ_MRI_SHOOT); break;
        case 0xBF: sm64_play_sound_global(SOUND_OBJ_BABY_PENGUIN_WALK); break;
        case 0xC0: sm64_play_sound_global(SOUND_OBJ_BOWSER_WALK); break;
        case 0xC1: sm64_play_sound_global(SOUND_OBJ_BOWSER_TAIL_PICKUP); break;
        case 0xC2: sm64_play_sound_global(SOUND_OBJ_BOWSER_DEFEATED); break;
        case 0xC3: sm64_play_sound_global(SOUND_OBJ_BOWSER_SPINNING); break;
        case 0xC4: sm64_play_sound_global(SOUND_OBJ_BOWSER_INHALING); break;
        case 0xC5: sm64_play_sound_global(SOUND_OBJ_BIG_PENGUIN_WALK); break;
        case 0xC6: sm64_play_sound_global(SOUND_OBJ_BOO_BOUNCE_TOP); break;
        case 0xC7: sm64_play_sound_global(SOUND_OBJ_BOO_LAUGH_SHORT); break;
        case 0xC8: sm64_play_sound_global(SOUND_OBJ_THWOMP); break;
        case 0xC9: sm64_play_sound_global(SOUND_OBJ_CANNON1); break;
        case 0xCA: sm64_play_sound_global(SOUND_OBJ_CANNON2); break;
        case 0xCB: sm64_play_sound_global(SOUND_OBJ_CANNON3); break;
        case 0xCC: sm64_play_sound_global(SOUND_OBJ_JUMP_WALK_WATER); break;
        case 0xCD: sm64_play_sound_global(SOUND_OBJ_UNKNOWN2); break;
        case 0xCE: sm64_play_sound_global(SOUND_OBJ_MRI_DEATH); break;
        case 0xCF: sm64_play_sound_global(SOUND_OBJ_POUNDING1); break;
        case 0xD0: sm64_play_sound_global(SOUND_OBJ_POUNDING1_HIGHPRIO); break;
        case 0xD1: sm64_play_sound_global(SOUND_OBJ_WHOMP_LOWPRIO); break;
        case 0xD2: sm64_play_sound_global(SOUND_OBJ_KING_BOBOMB); break;
        case 0xD3: sm64_play_sound_global(SOUND_OBJ_BULLY_METAL); break;
        case 0xD4: sm64_play_sound_global(SOUND_OBJ_BULLY_EXPLODE); break;
        case 0xD5: sm64_play_sound_global(SOUND_OBJ_BULLY_EXPLODE_2); break;
        case 0xD6: sm64_play_sound_global(SOUND_OBJ_POUNDING_CANNON); break;
        case 0xD7: sm64_play_sound_global(SOUND_OBJ_BULLY_WALK); break;
        case 0xD8: sm64_play_sound_global(SOUND_OBJ_UNKNOWN3); break;
        case 0xD9: sm64_play_sound_global(SOUND_OBJ_UNKNOWN4); break;
        case 0xDA: sm64_play_sound_global(SOUND_OBJ_BABY_PENGUIN_DIVE); break;
        case 0xDB: sm64_play_sound_global(SOUND_OBJ_GOOMBA_WALK); break;
        case 0xDC: sm64_play_sound_global(SOUND_OBJ_UKIKI_CHATTER_LONG); break;
        case 0xDD: sm64_play_sound_global(SOUND_OBJ_MONTY_MOLE_ATTACK); break;
        case 0xDE: sm64_play_sound_global(SOUND_OBJ_EVIL_LAKITU_THROW); break;
        case 0xDF: sm64_play_sound_global(SOUND_OBJ_UNK23); break;
        case 0xE0: sm64_play_sound_global(SOUND_OBJ_DYING_ENEMY1); break;
        case 0xE1: sm64_play_sound_global(SOUND_OBJ_CANNON4); break;
        case 0xE2: sm64_play_sound_global(SOUND_OBJ_DYING_ENEMY2); break;
        case 0xE3: sm64_play_sound_global(SOUND_OBJ_BOBOMB_WALK); break;
        case 0xE4: sm64_play_sound_global(SOUND_OBJ_SOMETHING_LANDING); break;
        case 0xE5: sm64_play_sound_global(SOUND_OBJ_DIVING_IN_WATER); break;
        case 0xE6: sm64_play_sound_global(SOUND_OBJ_SNOW_SAND1); break;
        case 0xE7: sm64_play_sound_global(SOUND_OBJ_SNOW_SAND2); break;
        case 0xE8: sm64_play_sound_global(SOUND_OBJ_DEFAULT_DEATH); break;
        case 0xE9: sm64_play_sound_global(SOUND_OBJ_BIG_PENGUIN_YELL); break;
        case 0xEA: sm64_play_sound_global(SOUND_OBJ_WATER_BOMB_BOUNCING); break;
        case 0xEB: sm64_play_sound_global(SOUND_OBJ_GOOMBA_ALERT); break;
        case 0xEC: sm64_play_sound_global(SOUND_OBJ_WIGGLER_JUMP); break;
        case 0xED: sm64_play_sound_global(SOUND_OBJ_STOMPED); break;
        case 0xEE: sm64_play_sound_global(SOUND_OBJ_UNKNOWN6); break;
        case 0xEF: sm64_play_sound_global(SOUND_OBJ_DIVING_INTO_WATER); break;
        case 0xF0: sm64_play_sound_global(SOUND_OBJ_PIRANHA_PLANT_SHRINK); break;
        case 0xF1: sm64_play_sound_global(SOUND_OBJ_KOOPA_THE_QUICK_WALK); break;
        case 0xF2: sm64_play_sound_global(SOUND_OBJ_KOOPA_WALK); break;
        case 0xF3: sm64_play_sound_global(SOUND_OBJ_BULLY_WALKING); break;
        case 0xF4: sm64_play_sound_global(SOUND_OBJ_DORRIE); break;
        case 0xF5: sm64_play_sound_global(SOUND_OBJ_BOWSER_LAUGH); break;
        case 0xF6: sm64_play_sound_global(SOUND_OBJ_UKIKI_CHATTER_SHORT); break;
        case 0xF7: sm64_play_sound_global(SOUND_OBJ_UKIKI_CHATTER_IDLE); break;
        case 0xF8: sm64_play_sound_global(SOUND_OBJ_UKIKI_STEP_DEFAULT); break;
        case 0xF9: sm64_play_sound_global(SOUND_OBJ_UKIKI_STEP_LEAVES); break;
        case 0xFA: sm64_play_sound_global(SOUND_OBJ_KOOPA_TALK); break;
        case 0xFB: sm64_play_sound_global(SOUND_OBJ_KOOPA_DAMAGE); break;
        case 0xFC: sm64_play_sound_global(SOUND_OBJ_KLEPTO1); break;
        case 0xFD: sm64_play_sound_global(SOUND_OBJ_KLEPTO2); break;
        case 0xFE: sm64_play_sound_global(SOUND_OBJ_KING_BOBOMB_TALK); break;
        case 0xFF: sm64_play_sound_global(SOUND_OBJ_KING_BOBOMB_JUMP); break;
        case 0x100: sm64_play_sound_global(SOUND_OBJ_KING_WHOMP_DEATH); break;
        case 0x101: sm64_play_sound_global(SOUND_OBJ_BOO_LAUGH_LONG); break;
        case 0x102: sm64_play_sound_global(SOUND_OBJ_EEL); break;
        case 0x103: sm64_play_sound_global(SOUND_OBJ_EEL_2); break;
        case 0x104: sm64_play_sound_global(SOUND_OBJ_EYEROK_SHOW_EYE); break;
        case 0x105: sm64_play_sound_global(SOUND_OBJ_MR_BLIZZARD_ALERT); break;
        case 0x106: sm64_play_sound_global(SOUND_OBJ_SNUFIT_SHOOT); break;
        case 0x107: sm64_play_sound_global(SOUND_OBJ_SKEETER_WALK); break;
        case 0x108: sm64_play_sound_global(SOUND_OBJ_WALKING_WATER); break;
        case 0x109: sm64_play_sound_global(SOUND_OBJ_BIRD_CHIRP3); break;
        case 0x10A: sm64_play_sound_global(SOUND_OBJ_PIRANHA_PLANT_APPEAR); break;
        case 0x10B: sm64_play_sound_global(SOUND_OBJ_FLAME_BLOWN); break;
        case 0x10C: sm64_play_sound_global(SOUND_OBJ_MAD_PIANO_CHOMPING); break;
        case 0x10D: sm64_play_sound_global(SOUND_OBJ_BOBOMB_BUDDY_TALK); break;
        case 0x10E: sm64_play_sound_global(SOUND_OBJ_SPINY_UNK59); break;
        case 0x10F: sm64_play_sound_global(SOUND_OBJ_WIGGLER_HIGH_PITCH); break;
        case 0x110: sm64_play_sound_global(SOUND_OBJ_HEAVEHO_TOSSED); break;
        case 0x111: sm64_play_sound_global(SOUND_OBJ_WIGGLER_DEATH); break;
        case 0x112: sm64_play_sound_global(SOUND_OBJ_BOWSER_INTRO_LAUGH); break;
        case 0x113: sm64_play_sound_global(SOUND_OBJ_ENEMY_DEATH_HIGH); break;
        case 0x114: sm64_play_sound_global(SOUND_OBJ_ENEMY_DEATH_LOW); break;
        case 0x115: sm64_play_sound_global(SOUND_OBJ_SWOOP_DEATH); break;
        case 0x116: sm64_play_sound_global(SOUND_OBJ_KOOPA_FLYGUY_DEATH); break;
        case 0x117: sm64_play_sound_global(SOUND_OBJ_POKEY_DEATH); break;
        case 0x118: sm64_play_sound_global(SOUND_OBJ_SNOWMAN_BOUNCE); break;
        case 0x119: sm64_play_sound_global(SOUND_OBJ_SNOWMAN_EXPLODE); break;
        case 0x11A: sm64_play_sound_global(SOUND_OBJ_POUNDING_LOUD); break;
        case 0x11B: sm64_play_sound_global(SOUND_OBJ_MIPS_RABBIT); break;
        case 0x11C: sm64_play_sound_global(SOUND_OBJ_MIPS_RABBIT_WATER); break;
        case 0x11D: sm64_play_sound_global(SOUND_OBJ_EYEROK_EXPLODE); break;
        case 0x11E: sm64_play_sound_global(SOUND_OBJ_CHUCKYA_DEATH); break;
        case 0x11F: sm64_play_sound_global(SOUND_OBJ_WIGGLER_TALK); break;
        case 0x120: sm64_play_sound_global(SOUND_OBJ_WIGGLER_ATTACKED); break;
        case 0x121: sm64_play_sound_global(SOUND_OBJ_WIGGLER_LOW_PITCH); break;
        case 0x122: sm64_play_sound_global(SOUND_OBJ_SNUFIT_SKEETER_DEATH); break;
        case 0x123: sm64_play_sound_global(SOUND_OBJ_BUBBA_CHOMP); break;
        case 0x124: sm64_play_sound_global(SOUND_OBJ_ENEMY_DEFEAT_SHRINK); break;
        case 0x125: sm64_play_sound_global(SOUND_AIR_BOWSER_SPIT_FIRE); break;
        case 0x126: sm64_play_sound_global(SOUND_AIR_UNK01); break;
        case 0x127: sm64_play_sound_global(SOUND_AIR_LAKITU_FLY); break;
        case 0x128: sm64_play_sound_global(SOUND_AIR_LAKITU_FLY_HIGHPRIO); break;
        case 0x129: sm64_play_sound_global(SOUND_AIR_AMP_BUZZ); break;
        case 0x12A: sm64_play_sound_global(SOUND_AIR_BLOW_FIRE); break;
        case 0x12B: sm64_play_sound_global(SOUND_AIR_BLOW_WIND); break;
        case 0x12C: sm64_play_sound_global(SOUND_AIR_ROUGH_SLIDE); break;
        case 0x12D: sm64_play_sound_global(SOUND_AIR_HEAVEHO_MOVE); break;
        case 0x12E: sm64_play_sound_global(SOUND_AIR_UNK07); break;
        case 0x12F: sm64_play_sound_global(SOUND_AIR_BOBOMB_LIT_FUSE); break;
        case 0x130: sm64_play_sound_global(SOUND_AIR_HOWLING_WIND); break;
        case 0x131: sm64_play_sound_global(SOUND_AIR_CHUCKYA_MOVE); break;
        case 0x132: sm64_play_sound_global(SOUND_AIR_PEACH_TWINKLE); break;
        case 0x133: sm64_play_sound_global(SOUND_AIR_CASTLE_OUTDOORS_AMBIENT); break;
        case 0x134: sm64_play_sound_global(SOUND_MENU_CHANGE_SELECT); break;
        case 0x135: sm64_play_sound_global(SOUND_MENU_REVERSE_PAUSE); break;
        case 0x136: sm64_play_sound_global(SOUND_MENU_PAUSE); break;
        case 0x137: sm64_play_sound_global(SOUND_MENU_PAUSE_HIGHPRIO); break;
        case 0x138: sm64_play_sound_global(SOUND_MENU_PAUSE_2); break;
        case 0x139: sm64_play_sound_global(SOUND_MENU_MESSAGE_APPEAR); break;
        case 0x13A: sm64_play_sound_global(SOUND_MENU_MESSAGE_DISAPPEAR); break;
        case 0x13B: sm64_play_sound_global(SOUND_MENU_CAMERA_ZOOM_IN); break;
        case 0x13C: sm64_play_sound_global(SOUND_MENU_CAMERA_ZOOM_OUT); break;
        case 0x13D: sm64_play_sound_global(SOUND_MENU_PINCH_MARIO_FACE); break;
        case 0x13E: sm64_play_sound_global(SOUND_MENU_LET_GO_MARIO_FACE); break;
        case 0x13F: sm64_play_sound_global(SOUND_MENU_HAND_APPEAR); break;
        case 0x140: sm64_play_sound_global(SOUND_MENU_HAND_DISAPPEAR); break;
        case 0x141: sm64_play_sound_global(SOUND_MENU_UNK0C); break;
        case 0x142: sm64_play_sound_global(SOUND_MENU_POWER_METER); break;
        case 0x143: sm64_play_sound_global(SOUND_MENU_CAMERA_BUZZ); break;
        case 0x144: sm64_play_sound_global(SOUND_MENU_CAMERA_TURN); break;
        case 0x145: sm64_play_sound_global(SOUND_MENU_UNK10); break;
        case 0x146: sm64_play_sound_global(SOUND_MENU_CLICK_FILE_SELECT); break;
        case 0x147: sm64_play_sound_global(SOUND_MENU_MESSAGE_NEXT_PAGE); break;
        case 0x148: sm64_play_sound_global(SOUND_MENU_COIN_ITS_A_ME_MARIO); break;
        case 0x149: sm64_play_sound_global(SOUND_MENU_YOSHI_GAIN_LIVES); break;
        case 0x14A: sm64_play_sound_global(SOUND_MENU_ENTER_PIPE); break;
        case 0x14B: sm64_play_sound_global(SOUND_MENU_EXIT_PIPE); break;
        case 0x14C: sm64_play_sound_global(SOUND_MENU_BOWSER_LAUGH); break;
        case 0x14D: sm64_play_sound_global(SOUND_MENU_ENTER_HOLE); break;
        case 0x14E: sm64_play_sound_global(SOUND_MENU_CLICK_CHANGE_VIEW); break;
        case 0x14F: sm64_play_sound_global(SOUND_MENU_CAMERA_UNUSED1); break;
        case 0x150: sm64_play_sound_global(SOUND_MENU_CAMERA_UNUSED2); break;
        case 0x151: sm64_play_sound_global(SOUND_MENU_MARIO_CASTLE_WARP); break;
        case 0x152: sm64_play_sound_global(SOUND_MENU_STAR_SOUND); break;
        case 0x153: sm64_play_sound_global(SOUND_MENU_THANK_YOU_PLAYING_MY_GAME); break;
        case 0x154: sm64_play_sound_global(SOUND_MENU_READ_A_SIGN); break;
        case 0x155: sm64_play_sound_global(SOUND_MENU_EXIT_A_SIGN); break;
        case 0x156: sm64_play_sound_global(SOUND_MENU_MARIO_CASTLE_WARP2); break;
        case 0x157: sm64_play_sound_global(SOUND_MENU_STAR_SOUND_OKEY_DOKEY); break;
        case 0x158: sm64_play_sound_global(SOUND_MENU_STAR_SOUND_LETS_A_GO); break;
        case 0x159: sm64_play_sound_global(SOUND_MENU_COLLECT_RED_COIN); break;
        case 0x15A: sm64_play_sound_global(SOUND_MENU_COLLECT_SECRET); break;
        case 0x15B: sm64_play_sound_global(SOUND_GENERAL2_BOBOMB_EXPLOSION); break;
        case 0x15C: sm64_play_sound_global(SOUND_GENERAL2_PURPLE_SWITCH); break;
        case 0x15D: sm64_play_sound_global(SOUND_GENERAL2_ROTATING_BLOCK_CLICK); break;
        case 0x15E: sm64_play_sound_global(SOUND_GENERAL2_SPINDEL_ROLL); break;
        case 0x15F: sm64_play_sound_global(SOUND_GENERAL2_PYRAMID_TOP_SPIN); break;
        case 0x160: sm64_play_sound_global(SOUND_GENERAL2_PYRAMID_TOP_EXPLOSION); break;
        case 0x161: sm64_play_sound_global(SOUND_GENERAL2_BIRD_CHIRP2); break;
        case 0x162: sm64_play_sound_global(SOUND_GENERAL2_SWITCH_TICK_FAST); break;
        case 0x163: sm64_play_sound_global(SOUND_GENERAL2_SWITCH_TICK_SLOW); break;
        case 0x164: sm64_play_sound_global(SOUND_GENERAL2_STAR_APPEARS); break;
        case 0x165: sm64_play_sound_global(SOUND_GENERAL2_ROTATING_BLOCK_ALERT); break;
        case 0x166: sm64_play_sound_global(SOUND_GENERAL2_BOWSER_EXPLODE); break;
        case 0x167: sm64_play_sound_global(SOUND_GENERAL2_BOWSER_KEY); break;
        case 0x168: sm64_play_sound_global(SOUND_GENERAL2_1UP_APPEAR); break;
        case 0x169: sm64_play_sound_global(SOUND_GENERAL2_RIGHT_ANSWER); break;
        case 0x16A: sm64_play_sound_global(SOUND_OBJ2_BOWSER_ROAR); break;
        case 0x16B: sm64_play_sound_global(SOUND_OBJ2_PIRANHA_PLANT_BITE); break;
        case 0x16C: sm64_play_sound_global(SOUND_OBJ2_PIRANHA_PLANT_DYING); break;
        case 0x16D: sm64_play_sound_global(SOUND_OBJ2_BOWSER_PUZZLE_PIECE_MOVE); break;
        case 0x16E: sm64_play_sound_global(SOUND_OBJ2_BULLY_ATTACKED); break;
        case 0x16F: sm64_play_sound_global(SOUND_OBJ2_KING_BOBOMB_DAMAGE); break;
        case 0x170: sm64_play_sound_global(SOUND_OBJ2_SCUTTLEBUG_WALK); break;
        case 0x171: sm64_play_sound_global(SOUND_OBJ2_SCUTTLEBUG_ALERT); break;
        case 0x172: sm64_play_sound_global(SOUND_OBJ2_BABY_PENGUIN_YELL); break;
        case 0x173: sm64_play_sound_global(SOUND_OBJ2_SWOOP); break;
        case 0x174: sm64_play_sound_global(SOUND_OBJ2_BIRD_CHIRP1); break;
        case 0x175: sm64_play_sound_global(SOUND_OBJ2_LARGE_BULLY_ATTACKED); break;
        case 0x176: sm64_play_sound_global(SOUND_OBJ2_EYEROK_SOUND_SHORT); break;
        case 0x177: sm64_play_sound_global(SOUND_OBJ2_WHOMP_SOUND_SHORT); break;
        case 0x178: sm64_play_sound_global(SOUND_OBJ2_EYEROK_SOUND_LONG); break;
        case 0x179: sm64_play_sound_global(SOUND_OBJ2_BOWSER_TELEPORT); break;
        case 0x17A: sm64_play_sound_global(SOUND_OBJ2_MONTY_MOLE_APPEAR); break;
        case 0x17B: sm64_play_sound_global(SOUND_OBJ2_BOSS_DIALOG_GRUNT); break;
        case 0x17C: sm64_play_sound_global(SOUND_OBJ2_MRI_SPINNING); break;
        default:
            sm64_play_sound_global(seq); // Play raw ID if no match
            break;
    }
}



void pc_heal_mario() {
  sm64_set_mario_health(marioId, 256*8);
  sm64_set_mario_action(marioId, ACT_IDLE);
}

void pc_damage_mario() {
  // Damage Mario by reducing health
  int current_health = g_mario_state.health;
  int new_health = current_health - 256; // reduce by 1 heart or something
  if (new_health < 0) new_health = 0;
  sm64_set_mario_health(marioId, new_health);
  // Maybe set action to hurt
  sm64_set_mario_action(marioId, ACT_BACKWARD_GROUND_KB);
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
  // might not be necessary, might help zed's framerate
  maybe_reload_surfaces(g_mario_state.position);
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
