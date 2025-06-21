#include <cstdint>

#include "audio.h"

#include "SDL3/SDL.h"
#include "collide/level.h"
#include "libsm64.h"

extern uint8_t* marioTexture;
extern int frame_num;
int load_and_init_mario();
void tick_mario_frame();

uint64_t pc_get_mario_x();
uint64_t pc_get_mario_y();
uint64_t pc_get_mario_z();
uint64_t pc_get_mario_action();

void pc_set_mario_camera(uint32_t x, uint32_t z);
void pc_set_mario_position_from_goal(uint32_t x_bits, uint32_t y_bits, uint32_t z_bits);

void load_combined_static_surfaces(const SM64Surface* surfaces1,
                                   int count1,
                                   const SM64Surface* surfaces2,
                                   int count2);
int load_surfaces_near(float x, float y, float z);
void pc_call_load_combined_static_surfaces_from_game_idx(uint32_t x_bits, uint32_t z_bits);
void maybe_reload_surfaces(const float* mario_pos);

bool point_in_triangle_2d(float px, float pz, const int32_t v[3][3]);
bool triangle_samples_in_cylinder(float center_x, float center_z, const int32_t v[3][3]);