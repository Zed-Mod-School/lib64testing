#pragma once

#include "libsm64.h"
 #include "decomp/include/surface_terrains.h"
#include "all_surfaces.h"
//#include "collide/beach_surfaces.c"
// #include "collide/citadel_surfaces.c"
// #include "collide/darkcave_surfaces.c"
// #include "collide/demo_surfaces.c"
// #include "collide/finalboss_surfaces.c"
// #include "collide/firecanyon_surfaces.c"
// #include "collide/intro_surfaces.c"
//#include "collide/jungle_surfaces.c"
// #include "collide/jungleb_surfaces.c"
// #include "collide/lavatube_surfaces.c"
// #include "collide/maincave_surfaces.c"
// #include "collide/misty_surfaces.c"
// #include "collide/ogre_surfaces.c"
// #include "collide/robocave_surfaces.c"
// #include "collide/rolling_surfaces.c"
// #include "collide/snow_surfaces.c"
// #include "collide/sunken_surfaces.c"
// #include "collide/sunkenb_surfaces.c"
// #include "collide/swamp_surfaces.c"h
// #include "collide/title_surfaces.c"
// #include "collide/training_surfaces.c"
// #include "collide/village1_surfaces.c"
// #include "collide/village2_surfaces.c"
// #include "collide/village3_surfaces.c"



//poop

const struct SM64Surface surfaces[] = {
    {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{5016,1188,3516}, {4936,1264,3518}, {4916,1206,3481}}},
    {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{4908,1293,3435}, {4916,1206,3481}, {4936,1264,3518}}},
    {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{1738,1498,-7485}, {1707,1321,-7591}, {1716,1508,-7501}}},
};
const size_t surfaces_count = sizeof(surfaces) / sizeof(surfaces[0]);
