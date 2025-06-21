#include <cstdint>

#include "libsm64.h"
#include "collide/level.h"

extern int frame_num;
int load_and_init_mario();
void tick_mario_frame();

uint64_t pc_get_mario_x();
uint64_t pc_get_mario_y();
uint64_t pc_get_mario_z();