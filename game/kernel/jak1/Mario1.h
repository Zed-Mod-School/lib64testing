#include "libsm64.h"
//mariozed
#include "game/kernel/common/kscheme.h"
#include "decomp/include/sm64_hacked.h"
#include "decomp/include/seq_ids.h"
#include "audio.h"
#include "SDL3/SDL.h"
#include "collide/level.h"
// Keep this mostly in the order they are used in the gameplay loop please
int load_and_init_mario();
void tick_mario_frame();
void update_mario_collide();

constexpr float METERS_TO_UNITS = 50.0f / 4096.0f;
// convert from float (used in GOAL) to SM64 units (meters 1.0) * METERS_TO_UNITS = pos in SM64
// the static level collide should be changes from {4926,1392,3379} to {4926 * JAK_SCALE, 1392 * JAK_SCALE, 3379 * JAK_SCALE} at some point to make world space comparasion easier

#include <iostream>
#include <fstream> // Required for std::ofstream
#include <cstdint>
#include <cstdlib>

#include <vector>
extern std::vector<SM64SurfaceObject> g_active_debug_objects;  // DECLARATION
struct NamedSurface {
    SM64Surface surface;
    std::string actor_name;   // for debugging / logging only
};

uint64_t pc_get_mario_x();
uint64_t pc_get_mario_y();
uint64_t pc_get_mario_z();
uint64_t pc_get_mario_action();

void pc_set_mario_camera(uint32_t x, uint32_t z);
void pc_set_mario_music_from_goal(uint32_t music_bits);
void pc_set_mario_position_from_goal(uint32_t x_bits, uint32_t y_bits, uint32_t z_bits);
void pc_set_mario_water_level_from_goal(uint32_t level_bits);
void pc_change_mario_state(uint32_t act_bits);
void pc_add_tris_to_surface(u32 info_ptr);

void update_platform_info_from_goal(u32 platform_info_ptr);
void update_moving_platform();
void pc_spawn_mario_test_collide(uint32_t name_ptr);
void update_psuedo_floor_under_mario();
void pc_mario_says_so_long_gay_bowsa(uint32_t music_bits);
void pc_heal_mario();
void pc_damage_mario();

void load_combined_static_surfaces(const SM64Surface* surfaces1,
                                   int count1,
                                   const SM64Surface* surfaces2,
                                   int count2);
int load_surfaces_near(float x, float y, float z);
void pc_call_load_combined_static_surfaces_from_game_idx(uint32_t x_bits, uint32_t z_bits);
void maybe_reload_surfaces(const float* mario_pos);

bool point_in_triangle_2d(float px, float pz, const int32_t v[3][3]);
bool triangle_samples_in_cylinder(float center_x, float center_z, const int32_t v[3][3]);
bool run_and_render_mario();
bool has_blue_eco();

struct PlatformInfo {
  u32 x_pos;
  u32 y_pos;
  u32 z_pos;
  u32 rot_x;
  u32 rot_y;
  u32 rot_z;
  u32 rot_w;
};

