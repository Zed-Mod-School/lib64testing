#include <cstdint>

#include "collide/level.h"
#include "libsm64.h"

extern int frame_num;
int load_and_init_mario();
void tick_mario_frame();

uint64_t pc_get_mario_x();
uint64_t pc_get_mario_y();
uint64_t pc_get_mario_z();

void pc_set_mario_camera(uint32_t x, uint32_t z);
void pc_set_mario_position_from_goal(uint32_t x_bits, uint32_t y_bits, uint32_t z_bits);