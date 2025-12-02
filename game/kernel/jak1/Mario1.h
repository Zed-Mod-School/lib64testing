#include "libsm64.h"
//mariozed
#include "decomp/include/sm64_hacked.h"
#include "audio.h"
#include "SDL3/SDL.h"
#include "collide/level.h"
// Keep this mostly in the order they are used in the gameplay loop please
int load_and_init_mario();
void tick_mario_frame();

constexpr float METERS_TO_UNITS = 50.0f / 4096.0f;
// convert from float (used in GOAL) to SM64 units (meters 1.0) * METERS_TO_UNITS = pos in SM64
// the static level collide should be changes from {4926,1392,3379} to {4926 * JAK_SCALE, 1392 * JAK_SCALE, 3379 * JAK_SCALE} at some point to make world space comparasion easier