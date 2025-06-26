#define _CRT_SECURE_NO_WARNINGS 1 // for fopen

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>

#include "../src/libsm64.h"
#ifndef SURFACE_DEFAULT
#define SURFACE_DEFAULT 0
#endif

#ifndef TERRAIN_STONE
#define TERRAIN_STONE 0
#endif

extern "C" {
#define SDL_MAIN_HANDLED
#include "level.h"
#include "context.h"
#include "renderer.h"
#include "gl33core/gl33core_renderer.h"
#include "gl20/gl20_renderer.h"
}

#include "audio.h"

#define MAX_CUBES 64

struct Cube {
    float pos[3];
    float size;
    SM64SurfaceObject surfaceObj;
};

Cube spawnedCubes[MAX_CUBES];
int numCubes = 0;

uint8_t *utils_read_file_alloc( const char *path, size_t *fileLength )
{
    FILE *f = fopen( path, "rb" );

    if( !f ) return NULL;

    fseek( f, 0, SEEK_END );
    size_t length = (size_t)ftell( f );
    rewind( f );
    uint8_t *buffer = (uint8_t*)malloc( length + 1 );
    fread( buffer, 1, length, f );
    buffer[length] = 0;
    fclose( f );

    if( fileLength ) *fileLength = length;

    return buffer;
}

static float read_axis( int16_t val )
{
    float result = (float)val / 32767.0f;

    if( result < 0.2f && result > -0.2f )
        return 0.0f;

    return result > 0.0f ? (result - 0.2f) / 0.8f : (result + 0.2f) / 0.8f;
}

float lerp(float a, float b, float amount)
{
    return a + (b - a) * amount;
}

uint32_t spawn_cube_under_mario(const float* marioPos, float size = 1000.0f) {
    if (numCubes >= MAX_CUBES) return 0;

    SM64SurfaceObject obj;
    memset(&obj, 0, sizeof(SM64SurfaceObject));

    Cube& c = spawnedCubes[numCubes++];
    c.pos[0] = marioPos[0];
    c.pos[1] = marioPos[1];
    c.pos[2] = marioPos[2] - size / 2.0f;
    c.size = size;

    float half = size / 2.0f;
    obj.transform.position[0] = marioPos[0];
    obj.transform.position[1] = marioPos[1];
    obj.transform.position[2] = marioPos[2] - half;

    obj.surfaceCount = 12;
    obj.surfaces = (SM64Surface*)malloc(sizeof(SM64Surface) * obj.surfaceCount);

    float x0 = -half, x1 = half;
    float y0 = -half, y1 = half;
    float z0 = -half, z1 = half;

    // This macro is correct
    #define ADD_TRI(i, ax, ay, az, bx, by, bz, cx, cy, cz) do { \
        obj.surfaces[i].vertices[0][0] = ax; obj.surfaces[i].vertices[0][1] = ay; obj.surfaces[i].vertices[0][2] = az; \
        obj.surfaces[i].vertices[1][0] = bx; obj.surfaces[i].vertices[1][1] = by; obj.surfaces[i].vertices[1][2] = bz; \
        obj.surfaces[i].vertices[2][0] = cx; obj.surfaces[i].vertices[2][1] = cy; obj.surfaces[i].vertices[2][2] = cz; \
        obj.surfaces[i].type = SURFACE_DEFAULT; \
        obj.surfaces[i].force = 0; \
        obj.surfaces[i].terrain = TERRAIN_STONE; \
    } while(0)

    // ✅ Carefully double-checked triangle order:
    // Top
    ADD_TRI(0, x0, y1, z1,  x1, y1, z1,  x1, y1, z0);
    ADD_TRI(1, x1, y1, z0,  x0, y1, z0,  x0, y1, z1);

    // Bottom
    ADD_TRI(2, x1, y0, z1,  x0, y0, z1,  x0, y0, z0);
    ADD_TRI(3, x0, y0, z0,  x1, y0, z0,  x1, y0, z1);

    // Front
    ADD_TRI(4, x0, y0, z1,  x1, y0, z1,  x1, y1, z1);
    ADD_TRI(5, x1, y1, z1,  x0, y1, z1,  x0, y0, z1);

    // Back
    ADD_TRI(6, x1, y0, z0,  x0, y0, z0,  x0, y1, z0);
    ADD_TRI(7, x0, y1, z0,  x1, y1, z0,  x1, y0, z0);

    // Left
    ADD_TRI(8, x0, y0, z0,  x0, y0, z1,  x0, y1, z1);
    ADD_TRI(9, x0, y1, z1,  x0, y1, z0,  x0, y0, z0);

    // Right
    ADD_TRI(10, x1, y0, z1,  x1, y0, z0,  x1, y1, z0);
    ADD_TRI(11, x1, y1, z0,  x1, y1, z1,  x1, y0, z1);

    #undef ADD_TRI

    uint32_t id = sm64_surface_object_create(&obj);
    c.surfaceObj = obj;
    free(obj.surfaces);

    return id;
}



void draw_surface_object(const SM64SurfaceObject& obj, const float rgba[4], bool outline = true) {
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glColor4f(rgba[0], rgba[1], rgba[2], rgba[3]);

    // Draw filled triangles
    glBegin(GL_TRIANGLES);
    for (uint32_t i = 0; i < obj.surfaceCount; ++i) {
float base[3] = {
    obj.transform.position[0],
    obj.transform.position[1],
    obj.transform.position[2]
};

for (uint32_t j = 0; j < 3; ++j) {
    float v[3] = {
        (float)obj.surfaces[i].vertices[j][0],
        (float)obj.surfaces[i].vertices[j][1],
        (float)obj.surfaces[i].vertices[j][2]
    };
    glVertex3f(v[0] + base[0], v[1] + base[1], v[2] + base[2]);
}

    }
    glEnd();

    // Optionally draw outlines
if (outline) {
    glLineWidth(2.5f);
    glColor4f(0, 0, 0, 1.0f); // black
    glBegin(GL_LINES);
    for (int i = 0; i < obj.surfaceCount; ++i) {
        float base[3] = {
            obj.transform.position[0],
            obj.transform.position[1],
            obj.transform.position[2]
        };

        float v0[3] = {
            (float)obj.surfaces[i].vertices[0][0],
            (float)obj.surfaces[i].vertices[0][1],
            (float)obj.surfaces[i].vertices[0][2]
        };
        float v1[3] = {
            (float)obj.surfaces[i].vertices[1][0],
            (float)obj.surfaces[i].vertices[1][1],
            (float)obj.surfaces[i].vertices[1][2]
        };
        float v2[3] = {
            (float)obj.surfaces[i].vertices[2][0],
            (float)obj.surfaces[i].vertices[2][1],
            (float)obj.surfaces[i].vertices[2][2]
        };

        glVertex3f(v0[0] + base[0], v0[1] + base[1], v0[2] + base[2]);
        glVertex3f(v1[0] + base[0], v1[1] + base[1], v1[2] + base[2]);

        glVertex3f(v1[0] + base[0], v1[1] + base[1], v1[2] + base[2]);
        glVertex3f(v2[0] + base[0], v2[1] + base[1], v2[2] + base[2]);

        glVertex3f(v2[0] + base[0], v2[1] + base[1], v2[2] + base[2]);
        glVertex3f(v0[0] + base[0], v0[1] + base[1], v0[2] + base[2]);
    }
    glEnd();
}


    glPopMatrix();
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
    glColor4f(1, 1, 1, 1);
}



int main( void )
{
    size_t romSize;

    uint8_t *rom = utils_read_file_alloc( "baserom.us.z64", &romSize );

    if( rom == NULL )
    {
        printf("\nFailed to read ROM file \"baserom.us.z64\"\n\n");
        return 1;
    }

    uint8_t *texture = (uint8_t*)malloc( 4 * SM64_TEXTURE_WIDTH * SM64_TEXTURE_HEIGHT );

    sm64_global_terminate();
    sm64_global_init( rom, texture );
    sm64_audio_init(rom);
    sm64_static_surfaces_load( surfaces, surfaces_count );
    int32_t marioId = sm64_mario_create( 0, 1000, 0 );

    free( rom );

    RenderState renderState;
    renderState.mario.index = NULL;
    vec3 cameraPos = { 0, 0, 0 };
    float cameraRot = 0.0f;

    struct Renderer *renderer;

    int major, minor;
#ifdef GL33_CORE
    major = 3; minor = 3;
    renderer = &gl33core_renderer;
#else
    major = 2; minor = 0;
    renderer = &gl20_renderer;
#endif

    context_init( "libsm64", 800, 600, major, minor );
    renderer->init( &renderState, texture );

    struct SM64MarioInputs marioInputs;
    struct SM64MarioState marioState;
    struct SM64MarioGeometryBuffers marioGeometry;

    // interpolation
    float lastPos[3], currPos[3];
    float lastGeoPos[9 * SM64_GEO_MAX_TRIANGLES], currGeoPos[9 * SM64_GEO_MAX_TRIANGLES];

    marioGeometry.position = (float*)malloc( sizeof(float) * 9 * SM64_GEO_MAX_TRIANGLES );
    marioGeometry.color    = (float*)malloc( sizeof(float) * 9 * SM64_GEO_MAX_TRIANGLES );
    marioGeometry.normal   = (float*)malloc( sizeof(float) * 9 * SM64_GEO_MAX_TRIANGLES );
    marioGeometry.uv       = (float*)malloc( sizeof(float) * 6 * SM64_GEO_MAX_TRIANGLES );
    marioGeometry.numTrianglesUsed = 0;

    float tick = 0;
    uint32_t lastTicks = SDL_GetTicks();

    audio_init();

    sm64_play_music(0, 0x05 | 0x80, 0); // from decomp/include/seq_ids.h: SEQ_LEVEL_WATER | SEQ_VARIATION
bool prevSquarePressed = false;
    do
    {
        float dt = (SDL_GetTicks() - lastTicks) / 1000.f;
        lastTicks = SDL_GetTicks();
        tick += dt;

        SDL_GameController *controller = context_get_controller();
        float x_axis, y_axis, x0_axis;

        if (!controller) // keyboard
        {
            const Uint8* state = SDL_GetKeyboardState(NULL);

            float dir;
            float spd = 0;
            if (state[SDL_SCANCODE_UP] && state[SDL_SCANCODE_RIGHT])
            {
                dir = -M_PI * 0.25f;
                spd = 1;
            }
            else if (state[SDL_SCANCODE_UP] && state[SDL_SCANCODE_LEFT])
            {
                dir = -M_PI * 0.75f;
                spd = 1;
            }
            else if (state[SDL_SCANCODE_DOWN] && state[SDL_SCANCODE_RIGHT])
            {
                dir = M_PI * 0.25f;
                spd = 1;
            }
            else if (state[SDL_SCANCODE_DOWN] && state[SDL_SCANCODE_LEFT])
            {
                dir = M_PI * 0.75f;
                spd = 1;
            }
            else if (state[SDL_SCANCODE_UP])
            {
                dir = -M_PI * 0.5f;
                spd = 1;
            }
            else if (state[SDL_SCANCODE_DOWN])
            {
                dir = M_PI * 0.5f;
                spd = 1;
            }
            else if (state[SDL_SCANCODE_LEFT])
            {
                dir = M_PI;
                spd = 1;
            }
            else if (state[SDL_SCANCODE_RIGHT])
            {
                dir = 0;
                spd = 1;
            }

            x_axis = cosf(dir) * spd;
            y_axis = sinf(dir) * spd;
            x0_axis = state[SDL_SCANCODE_LSHIFT] ? 1 : state[SDL_SCANCODE_RSHIFT] ? -1 : 0;

            marioInputs.buttonA = state[SDL_SCANCODE_X];
            marioInputs.buttonB = state[SDL_SCANCODE_C];
            marioInputs.buttonZ = state[SDL_SCANCODE_Z];
        }
        else
        {
            x_axis = read_axis( SDL_GameControllerGetAxis( controller, SDL_CONTROLLER_AXIS_LEFTX ));
            y_axis = read_axis( SDL_GameControllerGetAxis( controller, SDL_CONTROLLER_AXIS_LEFTY ));
            x0_axis = read_axis( SDL_GameControllerGetAxis( controller, SDL_CONTROLLER_AXIS_RIGHTX ));

            marioInputs.buttonA = SDL_GameControllerGetButton( controller, SDL_CONTROLLER_BUTTON_A );
            marioInputs.buttonB = SDL_GameControllerGetButton( controller, SDL_CONTROLLER_BUTTON_X );
            bool squarePressed = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X);  // SDL 2.x doesn't define BUTTON_SQUARE
            if (squarePressed && !prevSquarePressed) {
    spawn_cube_under_mario(marioState.position);
}
prevSquarePressed = squarePressed;


            marioInputs.buttonZ = SDL_GameControllerGetButton( controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER );
        }

        cameraRot += x0_axis * dt * 2;
        cameraPos[0] = marioState.position[0] + 1000.0f * cosf( cameraRot );
        cameraPos[1] = marioState.position[1] + 200.0f;
        cameraPos[2] = marioState.position[2] + 1000.0f * sinf( cameraRot );

        marioInputs.camLookX = marioState.position[0] - cameraPos[0];
        marioInputs.camLookZ = marioState.position[2] - cameraPos[2];
        marioInputs.stickX = x_axis;
        marioInputs.stickY = y_axis;

        while (tick >= 1.f/30)
        {
            memcpy(lastPos, currPos, sizeof(currPos));
            memcpy(lastGeoPos, currGeoPos, sizeof(currGeoPos));

            tick -= 1.f/30;
            sm64_mario_tick( marioId, &marioInputs, &marioState, &marioGeometry );

            memcpy(currPos, marioState.position, sizeof(currPos));
            memcpy(currGeoPos, marioGeometry.position, sizeof(currGeoPos));
        }

        for (int i=0; i<3; i++) marioState.position[i] = lerp(lastPos[i], currPos[i], tick / (1.f/30));
        for (int i=0; i<marioGeometry.numTrianglesUsed*9; i++) marioGeometry.position[i] = lerp(lastGeoPos[i], currGeoPos[i], tick / (1.f/30));

        renderer->draw( &renderState, cameraPos, &marioState, &marioGeometry );

        //new new
glMatrixMode(GL_MODELVIEW);
glPushMatrix();

glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
glDisable(GL_TEXTURE_2D);
glDisable(GL_CULL_FACE); // optional
glColor4f(1.0f, 1.0f, 0.0f, 0.4f);  // semi-transparent yellow

for (int i = 0; i < numCubes; ++i) {
    const Cube& cube = spawnedCubes[i];
    float yellow[] = {1.0f, 1.0f, 0.0f, 0.4f};
    draw_surface_object(cube.surfaceObj, yellow, true);
}

// for (int i = 0; i < numCubes; ++i) {
//     const Cube& cube = spawnedCubes[i];
//     float s = cube.size / 2;

//     float x = cube.pos[0];
//     float y = cube.pos[1];
//     float z = cube.pos[2];

//     float v[8][3] = {
//         {x-s, y-s, z-s}, {x+s, y-s, z-s},
//         {x+s, y+s, z-s}, {x-s, y+s, z-s},
//         {x-s, y-s, z+s}, {x+s, y-s, z+s},
//         {x+s, y+s, z+s}, {x-s, y+s, z+s},
//     };

//     int faces[6][4] = {
//         {0, 1, 2, 3}, // back
//         {5, 4, 7, 6}, // front
//         {4, 0, 3, 7}, // left
//         {1, 5, 6, 2}, // right
//         {3, 2, 6, 7}, // top
//         {4, 5, 1, 0}  // bottom
//     };

//     glBegin(GL_QUADS);
//     for (int f = 0; f < 6; ++f) {
//         for (int j = 0; j < 4; ++j) {
//             glVertex3fv(v[faces[f][j]]);
//         }
//     }
//     glEnd();
// }

glPopMatrix();
glEnable(GL_TEXTURE_2D);
glEnable(GL_CULL_FACE);
glColor4f(1, 1, 1, 1);  // reset


//outlines

//end outlines
        //end new new
//         //new
//         glMatrixMode(GL_MODELVIEW);
// glPushMatrix();

// glEnable(GL_BLEND);
// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
// glDisable(GL_TEXTURE_2D);
// glLineWidth(2.0f);

// for (int i = 0; i < numCubes; ++i) {
//     const Cube& cube = spawnedCubes[i];
//     float s = cube.size / 2;

//     float x = cube.pos[0];
//     float y = cube.pos[1];
//     float z = cube.pos[2];

//     glBegin(GL_LINES);
//     glColor4f(1.0f, 1.0f, 0.0f, 0.5f);  // yellowish transparent

//     // 8 corners of cube
//     float v[8][3] = {
//         {x-s, y-s, z-s}, {x+s, y-s, z-s},
//         {x+s, y+s, z-s}, {x-s, y+s, z-s},
//         {x-s, y-s, z+s}, {x+s, y-s, z+s},
//         {x+s, y+s, z+s}, {x-s, y+s, z+s},
//     };

//     // 12 edges
//     int edges[12][2] = {
//         {0,1},{1,2},{2,3},{3,0},
//         {4,5},{5,6},{6,7},{7,4},
//         {0,4},{1,5},{2,6},{3,7}
//     };

//     for (int e = 0; e < 12; ++e) {
//         glVertex3fv(v[edges[e][0]]);
//         glVertex3fv(v[edges[e][1]]);
//     }

//     glEnd();
// }

// glPopMatrix();
// glColor4f(1, 1, 1, 1);
// glEnable(GL_TEXTURE_2D);
//         //endnew
    }
    while( context_flip_frame_poll_events() );

    sm64_stop_background_music(sm64_get_current_background_music());
    sm64_global_terminate();
    context_terminate();

    return 0;
}
