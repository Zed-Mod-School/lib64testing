// main.cpp - libsm64 + Jak and Daxter style moving platform
// Platforms now visible again with old drawing style restored

#define _CRT_SECURE_NO_WARNINGS 1

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "../src/libsm64.h"

extern "C" {
#define SDL_MAIN_HANDLED
#include "level.h"
#include "context.h"
#include "renderer.h"
#include "gl33core/gl33core_renderer.h"
// #include "gl20/gl20_renderer.h"  // uncomment if using GL2.0
}

#include "audio.h"

// Define missing constants (from libsm64 common usage)
#ifndef SURFACE_DEFAULT
#define SURFACE_DEFAULT 0
#endif
#ifndef TERRAIN_STONE
#define TERRAIN_STONE 0
#endif

#define MAX_CUBES 64

// ─────────────────────────────────────────────
// Moving platform config (Jak-style sliding floor)
#define PLAT_MOVE_FRAMES  120
#define PLAT_MOVE_SPEED   20.0f

// Globals for the moving platform
static uint32_t           gPlatId    = 0;
static SM64ObjectTransform gPlatXf   = {};  // Authoritative transform (libsm64 uses this)
static int32_t            gPlatTimer = 0;
static float              gPlatDir   = 1.0f;

// Cube / spawned object tracking
struct Cube {
    float pos[3];
    float size;
    const char* name;
    uint32_t id;
};
static Cube spawnedCubes[MAX_CUBES];
static int numCubes = 0;

// ─────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────
uint8_t* utils_read_file_alloc(const char* path, size_t* outLength = nullptr)
{
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;

    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    rewind(f);

    uint8_t* buf = (uint8_t*)malloc(len);
    if (!buf) { fclose(f); return nullptr; }

    fread(buf, 1, len, f);
    fclose(f);

    if (outLength) *outLength = len;
    return buf;
}

static float read_axis(int16_t val)
{
    float f = (float)val / 32767.0f;
    if (fabsf(f) < 0.2f) return 0.0f;
    return f > 0 ? (f - 0.2f) / 0.8f : (f + 0.2f) / 0.8f;
}

// ─────────────────────────────────────────────
// Moving platform logic
// ─────────────────────────────────────────────
static void update_moving_platform()
{
    if (!gPlatId) return;

    float velocityX = PLAT_MOVE_SPEED * gPlatDir;

    gPlatXf.position[0] += velocityX;

    sm64_surface_object_move(gPlatId, &gPlatXf);

    // Sync visual position
    for (int i = 0; i < numCubes; i++) {
        if (spawnedCubes[i].id == gPlatId) {
            spawnedCubes[i].pos[0] = gPlatXf.position[0];
            spawnedCubes[i].pos[1] = gPlatXf.position[1];
            spawnedCubes[i].pos[2] = gPlatXf.position[2];
            break;
        }
    }

    gPlatTimer++;
    if (gPlatTimer >= PLAT_MOVE_FRAMES) {
        gPlatTimer = 0;
        gPlatDir = -gPlatDir;
    }
}

// ─────────────────────────────────────────────
// Spawn flat moving platform under Mario
// ─────────────────────────────────────────────
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
    float y  = 0.0f;

    ADD_TRI(0, x0,y,z1, x1,y,z1, x1,y,z0);
    ADD_TRI(1, x1,y,z0, x0,y,z0, x0,y,z1);

    #undef ADD_TRI

    uint32_t id = sm64_surface_object_create(&obj);
    free(obj.surfaces);

    if (!id) return 0;

    gPlatXf.position[0] = obj.transform.position[0];
    gPlatXf.position[1] = obj.transform.position[1];
    gPlatXf.position[2] = obj.transform.position[2];
    gPlatXf.eulerRotation[0] = obj.transform.eulerRotation[0];
    gPlatXf.eulerRotation[1] = obj.transform.eulerRotation[1];
    gPlatXf.eulerRotation[2] = obj.transform.eulerRotation[2];

    gPlatId    = id;
    gPlatTimer = 0;
    gPlatDir   = 1.0f;

    Cube& c = spawnedCubes[numCubes++];
    c.pos[0] = marioPos[0];
    c.pos[1] = marioPos[1];
    c.pos[2] = marioPos[2];
    c.size   = size;
    c.name   = "MovingPlat";
    c.id     = id;

    return id;
}

// ─────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────
int main(void)
{
    size_t romSize;
    uint8_t* rom = utils_read_file_alloc("baserom.us.z64", &romSize);
    if (!rom) {
        printf("Failed to read baserom.us.z64\n");
        return 1;
    }

    uint8_t* texture = (uint8_t*)malloc(4 * SM64_TEXTURE_WIDTH * SM64_TEXTURE_HEIGHT);

    sm64_global_init(rom, texture);
    sm64_audio_init(rom);
    sm64_static_surfaces_load(surfaces, surfaces_count);

    int32_t marioId = sm64_mario_create(0, 1000, 0);

    free(rom);

    RenderState renderState = {};
    vec3 cameraPos = {0, 200, 1000};
    float cameraRot = 0.0f;

    struct Renderer* renderer = &gl33core_renderer;

    context_init("libsm64 + Moving Platform", 1280, 720, 3, 3);
    renderer->init(&renderState, texture);

    struct SM64MarioInputs  marioInputs  = {};
    struct SM64MarioState   marioState   = {};
    struct SM64MarioGeometryBuffers marioGeometry = {};

    marioGeometry.position = (float*)malloc(9 * SM64_GEO_MAX_TRIANGLES * sizeof(float));
    marioGeometry.color    = (float*)malloc(9 * SM64_GEO_MAX_TRIANGLES * sizeof(float));
    marioGeometry.normal   = (float*)malloc(9 * SM64_GEO_MAX_TRIANGLES * sizeof(float));
    marioGeometry.uv       = (float*)malloc(6 * SM64_GEO_MAX_TRIANGLES * sizeof(float));

    float lastPos[3]{}, currPos[3]{};
    float lastGeoPos[9 * SM64_GEO_MAX_TRIANGLES]{};
    float currGeoPos[9 * SM64_GEO_MAX_TRIANGLES]{};

    float tick = 0.0f;
    uint32_t lastTicks = SDL_GetTicks();

    audio_init();
    sm64_play_music(0, 0x05 | 0x80, 0);

    bool prevSpawnKey = false;

    while (context_flip_frame_poll_events())
    {
        float dt = (SDL_GetTicks() - lastTicks) / 1000.0f;
        lastTicks = SDL_GetTicks();
        tick += dt;

        // Input
        SDL_GameController* ctrl = context_get_controller();
        float stickX = 0, stickY = 0, camX = 0;

        const Uint8* kb = SDL_GetKeyboardState(NULL);

        if (!ctrl) {
            if (kb[SDL_SCANCODE_W]) stickY = -1;
            if (kb[SDL_SCANCODE_S]) stickY =  1;
            if (kb[SDL_SCANCODE_A]) stickX = -1;
            if (kb[SDL_SCANCODE_D]) stickX =  1;
            camX = (kb[SDL_SCANCODE_RIGHT] ? 1 : 0) - (kb[SDL_SCANCODE_LEFT] ? 1 : 0);
            marioInputs.buttonA = kb[SDL_SCANCODE_X] || kb[SDL_SCANCODE_SPACE];
            marioInputs.buttonB = kb[SDL_SCANCODE_C];
            marioInputs.buttonZ = kb[SDL_SCANCODE_Z];
        } else {
            stickX = read_axis(SDL_GameControllerGetAxis(ctrl, SDL_CONTROLLER_AXIS_LEFTX));
            stickY = read_axis(SDL_GameControllerGetAxis(ctrl, SDL_CONTROLLER_AXIS_LEFTY));
            camX   = read_axis(SDL_GameControllerGetAxis(ctrl, SDL_CONTROLLER_AXIS_RIGHTX));
            marioInputs.buttonA = SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_A);
            marioInputs.buttonB = SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_X);
            marioInputs.buttonZ = SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        }

        // Spawn platform
        bool spawnKey = kb[SDL_SCANCODE_X] || (ctrl && SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_X));
        if (spawnKey && !prevSpawnKey) {
            float spawnPos[3] = {
                marioState.position[0],
                marioState.position[1] - 80.0f,   // better match Mario foot height
                marioState.position[2]
            };
            spawn_flat_platform_under_mario(spawnPos, 600.0f);
        }
        prevSpawnKey = spawnKey;

        // Camera
        cameraRot += camX * dt * 2.5f;
        cameraPos[0] = marioState.position[0] + 1200.0f * cosf(cameraRot);
        cameraPos[1] = marioState.position[1] + 400.0f;
        cameraPos[2] = marioState.position[2] + 1200.0f * sinf(cameraRot);

        marioInputs.camLookX = marioState.position[0] - cameraPos[0];
        marioInputs.camLookZ = marioState.position[2] - cameraPos[2];
        marioInputs.stickX = stickX;
        marioInputs.stickY = stickY;

        // Physics
        while (tick >= 1.f/30.f)
        {
            memcpy(lastPos, currPos, sizeof(currPos));
            memcpy(lastGeoPos, currGeoPos, sizeof(currGeoPos));

            update_moving_platform();


            sm64_mario_tick(marioId, &marioInputs, &marioState, &marioGeometry);

            memcpy(currPos, marioState.position, sizeof(currPos));
            memcpy(currGeoPos, marioGeometry.position, sizeof(currGeoPos));

            tick -= 1.f/30.f;
        }

        // Interpolate
        float alpha = tick / (1.f/30.f);
        for (int i = 0; i < 3; i++)
            marioState.position[i] = lastPos[i] + (currPos[i] - lastPos[i]) * alpha;

        for (int i = 0; i < marioGeometry.numTrianglesUsed * 9; i++)
            marioGeometry.position[i] = lastGeoPos[i] + (currGeoPos[i] - lastGeoPos[i]) * alpha;

        // ─── Render ───────────────────────────────────────
        renderer->draw(&renderState, cameraPos, &marioState, &marioGeometry);

        // Old main.cpp style drawing - restored
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_CULL_FACE);

        glColor4f(1.0f, 1.0f, 0.0f, 0.4f);  // semi-transparent yellow

        for (int i = 0; i < numCubes; ++i) {
            const Cube& cube = spawnedCubes[i];
            float s = cube.size / 2;

            float x = cube.pos[0];
            float y = cube.pos[1];
            float z = cube.pos[2];

            float v[8][3] = {
                {x-s, y-s, z-s}, {x+s, y-s, z-s},
                {x+s, y+s, z-s}, {x-s, y+s, z-s},
                {x-s, y-s, z+s}, {x+s, y-s, z+s},
                {x+s, y+s, z+s}, {x-s, y+s, z+s},
            };

            int faces[6][4] = {
                {0, 1, 2, 3}, // back
                {5, 4, 7, 6}, // front
                {4, 0, 3, 7}, // left
                {1, 5, 6, 2}, // right
                {3, 2, 6, 7}, // top
                {4, 5, 1, 0}  // bottom
            };

            glBegin(GL_QUADS);
            for (int f = 0; f < 6; ++f) {
                for (int j = 0; j < 4; ++j) {
                    glVertex3fv(v[faces[f][j]]);
                }
            }
            glEnd();
        }

        glPopMatrix();
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_CULL_FACE);
        glColor4f(1, 1, 1, 1);


        // Optional: old wireframe style on top (uncomment if you want both)

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_TEXTURE_2D);
        glLineWidth(2.0f);

        for (int i = 0; i < numCubes; ++i) {
            const Cube& cube = spawnedCubes[i];
            float s = cube.size / 2;

            float x = cube.pos[0];
            float y = cube.pos[1];
            float z = cube.pos[2];

            glBegin(GL_LINES);
            glColor4f(1.0f, 1.0f, 0.0f, 0.5f);

            float v[8][3] = {
                {x-s, y-s, z-s}, {x+s, y-s, z-s},
                {x+s, y+s, z-s}, {x-s, y+s, z-s},
                {x-s, y-s, z+s}, {x+s, y-s, z+s},
                {x+s, y+s, z+s}, {x-s, y+s, z+s},
            };

            int edges[12][2] = {
                {0,1},{1,2},{2,3},{3,0},
                {4,5},{5,6},{6,7},{7,4},
                {0,4},{1,5},{2,6},{3,7}
            };

            for (int e = 0; e < 12; ++e) {
                glVertex3fv(v[edges[e][0]]);
                glVertex3fv(v[edges[e][1]]);
            }

            glEnd();
        }

        glPopMatrix();
        glColor4f(1, 1, 1, 1);
        glEnable(GL_TEXTURE_2D);

    }

    sm64_stop_background_music(sm64_get_current_background_music());
    sm64_global_terminate();
    context_terminate();

    free(marioGeometry.position);
    free(marioGeometry.color);
    free(marioGeometry.normal);
    free(marioGeometry.uv);

    return 0;
}