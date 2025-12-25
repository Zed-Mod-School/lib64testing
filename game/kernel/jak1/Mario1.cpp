#include "Mario1.h"
#include "game/graphics/opengl_renderer/MarioRenderer.h"
//#include "game/graphics/opengl_renderer/MarioRenderer2.h"
#include "common/util/FileUtil.h"
#include "game/kernel/common/kscheme.h"

//int variables here
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
  //sm64_play_music(0, 0x05 | 0x80, 0);
  // for (int i = 0; i < 10; ++i) {
  //   printf("marioId = %d\n", marioId);
  // }


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

  // mario functions that I wish we could somehow register from this file instead of kmachine
  // g_pc_port_funcs.make_func_symbol_func("pc-get-mario-x", (void*)pc_get_mario_x);
  // g_pc_port_funcs.make_func_symbol_func("pc-get-mario-y", (void*)pc_get_mario_y);
  // g_pc_port_funcs.make_func_symbol_func("pc-get-mario-z", (void*)pc_get_mario_z);
  // g_pc_port_funcs.make_func_symbol_func("pc-set-mario-look-angles!", (void*)pc_set_mario_camera);
  // g_pc_port_funcs.make_func_symbol_func("teleport-mario-to-pos",
  // (void*)pc_set_mario_position_from_goal);
  // g_pc_port_funcs.make_func_symbol_func("pc-load-mario-collide!",
  // (void*)pc_call_load_combined_static_surfaces_from_game_idx);
  delete[] romBuffer;
  return marioId;
}

// Mario frame updating stuff now yippie, this is the "main" loop on the mario side.

int frame_num = 0;


void tick_mario_frame() {
  // This function is called every frame and controls updating the mario engine.
  // Revist this later once we have *some* mario collide to test, probably can /64 where we set .stickX instead and remove all this junk
  float scaled_stick_x = g_mario_inputs.stickX / 64.0f;
  float scaled_stick_y = g_mario_inputs.stickY / 64.0f;

  if (frame_num % 2 == 0 && marioId != -1) {
    SM64MarioInputs inputs = g_mario_inputs;
    inputs.stickX = scaled_stick_x;
    inputs.stickY = -scaled_stick_y;

    sm64_mario_tick(marioId, &inputs, &g_mario_state, &g_geom);
    //  printf("[Mario Pos] X = %.2f, Y = %.2f, Z = %.2f\n",
    //      g_mario_state.position[0],
    //      g_mario_state.position[1],
    //      g_mario_state.position[2]);
    update_psuedo_floor_under_mario();
    //TODO this is the function resposible for updating mario collide dynamically
    maybe_reload_surfaces(g_mario_state.position);  // Add back with dynamic collide update
    frame_num = 0;
  }

  frame_num++;
}

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

  // printf("[pc_add_tris] ENTER\n");
  // printf("  raw bits: x=0x%08X y=0x%08X z=0x%08X vi=0x%08X\n",
  //        x_bits, y_bits, z_bits, vert_index_bits);

  // Decode floats
  memcpy(&x, &x_bits, sizeof(float));
  memcpy(&y, &y_bits, sizeof(float));
  memcpy(&z, &z_bits, sizeof(float));
  memcpy(&vert_index_f, &vert_index_bits, sizeof(float));

  //printf("  decoded floats: x=%f y=%f z=%f vert_index_f=%f\n",
        // x, y, z, vert_index_f);

  // Unit conversion
  x *= METERS_TO_UNITS;
  y *= METERS_TO_UNITS;
  z *= METERS_TO_UNITS;

  //printf("  scaled to units: x=%f y=%f z=%f\n", x, y, z);

  // Convert float selector → int
  int vert_index = (int)(vert_index_f + 0.5f);
  //printf("  vert_index rounded = %d\n", vert_index);

  // Validate index
  if (vert_index < 1 || vert_index > 3) {
    printf("  ERROR: vert_index out of range (must be 1–3), aborting\n");
    return;
  }

  int idx = vert_index - 1;
//  printf("  writing vertex slot %d\n", idx);

  // Store vertex
gTempVerts[idx][0] = (int32_t)x;
gTempVerts[idx][1] = (int32_t)y;
gTempVerts[idx][2] = (int32_t)z;


//  printf("  stored gTempVerts[%d] = (%d, %d, %d)\n",
        //  idx,
        //  gTempVerts[idx][0],
        //  gTempVerts[idx][1],
        //  gTempVerts[idx][2]);

  // Commit triangle
  if (vert_index == 3) {
   // printf("  vert_index == 3 → committing triangle\n");

    if (gDebugSurfaceCount >= MAX_DEBUG_SURFACES) {
      printf("  ERROR: surface buffer full (%d), aborting\n",
             gDebugSurfaceCount);
      return;
    }

    struct SM64Surface* surf = &gDebugSurfaces[gDebugSurfaceCount];

   // printf("  writing surface index %d\n", gDebugSurfaceCount);

    surf->type = SURFACE_DEFAULT;
    surf->force = 0;
    surf->terrain = TERRAIN_STONE;

    memcpy(surf->vertices, gTempVerts, sizeof(gTempVerts));

    // printf("  surface vertices:\n");
    // printf("    v0 = (%d, %d, %d)\n",
    //        surf->vertices[0][0],
    //        surf->vertices[0][1],
    //        surf->vertices[0][2]);
    // printf("    v1 = (%d, %d, %d)\n",
    //        surf->vertices[1][0],
    //        surf->vertices[1][1],
    //        surf->vertices[1][2]);
    // printf("    v2 = (%d, %d, %d)\n",
    //        surf->vertices[2][0],
    //        surf->vertices[2][1],
    //        surf->vertices[2][2]);

    gDebugSurfaceCount++;

    // printf("  triangle committed, new surface count = %d\n",
    //        gDebugSurfaceCount);
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


                //spawn_surfaces_under_mario(marioState.position, beach_surfaces, objectName);
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

void pc_spawn_mario_test_collide(u32 name_ptr) {
    // Convert the u32 pointer to the C++ string (like in pc_filepath_exists)
    auto name_str = std::string(Ptr<String>(name_ptr).c()->data());
    const char* name = name_str.c_str();

    // first we delete the old surface object if it exists
    delete_surface_object_by_name(name);

    // then we spawn in the new one
    pc_mario_spawn_updated_tris(name);


// this is called after we finish uploading triangles to mario enginge and it spawns the object and resets the debug buffers

// sm64_set_mario_water_level(marioId,
//   //(ped->m_nPhysicalFlags.bTouchingWater) ? // add valid function call to check if jak is in/close to water
//   //ped->m_pPlayerData->m_fWaterHeight/MARIO_SCALE // call a c++ function that returns target's water height
//   // : INT16_MIN
//   );
}

void pc_mario_says_so_long_gay_bowsa() {
//This function is called by GOAL to play the iconic line, but it should eventually be refactored to take in a sound ID from GOAL and play any sound/line
sm64_play_sound_global(SOUND_MARIO_SO_LONGA_BOWSER);
}

void pc_heal_mario() {
  sm64_set_mario_health(marioId, 256*8);
  sm64_set_mario_action(marioId, ACT_IDLE);
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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
