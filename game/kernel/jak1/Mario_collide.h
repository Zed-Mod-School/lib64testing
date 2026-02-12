#pragma once
#include "libsm64.h"

static const struct SM64Surface psuedo_floor_surfaces[] = {
    {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{-100, 0, 100}, {100, 0, 100}, {100, 0, -100}}},
    {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{100, 0, -100}, {-100, 0, -100}, {-100, 0, 100}}}
};