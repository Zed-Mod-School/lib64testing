#pragma once

/*!
 * @file kboot.h
 * GOAL Boot.  Contains the "main" function to launch GOAL runtime.
 */

#include "common/common_types.h"
// mario stuff
#include "libsm64/level.h"  // level.h is needed for SM64MarioSurface it has the surface defintions and data
#include "libsm64/libsm64.h"

extern int marioId;
uint64_t pc_get_mario_x();
uint64_t pc_get_mario_y();
uint64_t pc_get_mario_z();

void load_combined_static_surfaces(const struct SM64Surface* surfaces1,

                                   int count1,

                                   const struct SM64Surface* surfaces2,

                                   int count2);
#include "game/kernel/common/kboot.h"

namespace jak1 {

// Video Mode that's set based on display refresh rate on boot
extern VideoMode BootVideoMode;

/*!
 * Initialize global variables for kboot
 */
void kboot_init_globals();

/*!
 * Launch the GOAL Kernel (EE).
 * See InitParms for launch argument details.
 * @param argc : argument count
 * @param argv : argument list
 * @return 0 on success, otherwise failure.
 */
s32 goal_main(int argc, const char* const* argv);

/*!
 * Run the GOAL Kernel.
 */
void KernelCheckAndDispatch();

/*!
 * Stop running the GOAL Kernel.
 */
void KernelShutdown();

}  // namespace jak1
