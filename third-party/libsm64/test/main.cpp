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
    const char* name;
    uint32_t id;
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

    // ? Carefully double-checked triangle order:
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

const struct SM64Surface beach_surfaces[] = {
    {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{-5709,1433,-5201}, {-5564,1604,-5050}, {-5695,1687,-5018}}},
    {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{-5572,1832,-4936}, {-5695,1687,-5018}, {-5564,1604,-5050}}},
    {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{-5695,1687,-5018}, {-5572,1832,-4936}, {-5676,1884,-4989}}},
    {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{-5648,1979,-5029}, {-5676,1884,-4989}, {-5572,1832,-4936}}},
    {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{-5676,1884,-4989}, {-5648,1979,-5029}, {-5798,1803,-5049}}}
};
// (create-sm64-collide-mesh-from-actor (process-by-ename "crate-32"))
// crate-32.Txt
// const struct SM64Surface crate_32_surfaces[] = {
//     {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{-5709,1433,-5201}, {-5564,1604,-5050}, {-5695,1687,-5018}}},
//     {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{-5572,1832,-4936}, {-5695,1687,-5018}, {-5564,1604,-5050}}},
//     {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{-5695,1687,-5018}, {-5572,1832,-4936}, {-5676,1884,-4989}}},
//     {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{-5648,1979,-5029}, {-5676,1884,-4989}, {-5572,1832,-4936}}},
//     {SURFACE_DEFAULT, 0, TERRAIN_STONE, {{-5676,1884,-4989}, {-5648,1979,-5029}, {-5798,1803,-5049}}}
// };
template <size_t N>

uint32_t spawn_surfaces_under_mario(
    const float* marioPos,
    const SM64Surface (&surfaces)[N], // Array passed by reference (size N is deduced)
    const char* objectName,
    float y_offset = 0.0f
) {
    if (numCubes >= MAX_CUBES) return 0;

    SM64SurfaceObject obj;
    memset(&obj, 0, sizeof(SM64SurfaceObject));

    Cube& c = spawnedCubes[numCubes++];

    // Store the object name
    c.name = objectName; // <-- STORE NAME
    // Set object transform (position) - this is the object's origin
    obj.transform.position[0] = marioPos[0];
    obj.transform.position[1] = marioPos[1] + y_offset;
    obj.transform.position[2] = marioPos[2];

    // Use the deduced size N directly
    obj.surfaceCount = N;

    // Allocate memory for the surfaces and copy the data
    obj.surfaces = (SM64Surface*)malloc(sizeof(SM64Surface) * obj.surfaceCount);
    if (!obj.surfaces) {
        numCubes--;
        return 0;
    }

    // Copy the input array data
    memcpy(obj.surfaces, surfaces, sizeof(SM64Surface) * obj.surfaceCount);

    // Calculate a rough bounding box center and size for visualization/debugging (optional, but good practice)
    float min_coords[3] = {1e9f, 1e9f, 1e9f};
    float max_coords[3] = {-1e9f, -1e9f, -1e9f};

    for (size_t i = 0; i < N; ++i) {
        for (int j = 0; j < 3; ++j) { // Vertices
            for (int k = 0; k < 3; ++k) { // XYZ coordinates
                float v = (float)surfaces[i].vertices[j][k];
                if (v < min_coords[k]) min_coords[k] = v;
                if (v > max_coords[k]) max_coords[k] = v;
            }
        }
    }

    // Set Cube pos/size based on world coordinates of the array for drawing/tracking
    c.pos[0] = (max_coords[0] + min_coords[0]) / 2.0f;
    c.pos[1] = (max_coords[1] + min_coords[1]) / 2.0f;
    c.pos[2] = (max_coords[2] + min_coords[2]) / 2.0f;

    c.size = fmaxf(fmaxf(max_coords[0] - min_coords[0], max_coords[1] - min_coords[1]), max_coords[2] - min_coords[2]);

    uint32_t id = sm64_surface_object_create(&obj);


c.id = id;
c.surfaceObj = obj;
    return id;
}


void delete_surface_object_by_name(const char* name) {
    if (!name || numCubes == 0) return;

    int index_to_remove = -1;

    // 1. Find the object by name
    for (int i = 0; i < numCubes; ++i) {
        const Cube& c = spawnedCubes[i];
        // Check if the object has a name and if it matches the requested name
        if (c.name != NULL && strcmp(c.name, name) == 0) {
            index_to_remove = i;
            break; // Found the object, stop searching
        }
    }

    if (index_to_remove != -1) {
        Cube& c = spawnedCubes[index_to_remove];

        // 2. De-register the collision object using the stored ID
        sm64_surface_object_delete(c.id);

        // 3. Free the surface memory that was malloc'd in spawn_surfaces_under_mario
        if (c.surfaceObj.surfaces != NULL) {
            free(c.surfaceObj.surfaces);
            c.surfaceObj.surfaces = NULL;
        }

        // 4. Remove the object from the tracking array (Swap-and-Pop)

        // Decrement the total count
        numCubes--;

        // If the object being removed wasn't the last one,
        // copy the last Cube structure over the one we are removing.
        if (index_to_remove < numCubes) {
            spawnedCubes[index_to_remove] = spawnedCubes[numCubes];
        }

        // The object is now removed from the array and its collision is deleted.
    }
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


// --- Text Drawing Functions ---

// Scale factor for the text size
#define TEXT_SCALE 50.0f
// Function to render a single stroke of a character
void draw_line_segment(float x1, float y1, float x2, float y2) {
    glVertex3f(x1 * TEXT_SCALE, y1 * TEXT_SCALE, 0.0f);
    glVertex3f(x2 * TEXT_SCALE, y2 * TEXT_SCALE, 0.0f);
}

// Function to draw a character 'c' relative to (0,0,0)
void draw_char(char c) {
    glPushMatrix();
    glTranslatef(-0.5f * TEXT_SCALE, -0.5f * TEXT_SCALE, 0.0f); // Center characters slightly

    // Define simple geometry for key characters as line segments (stroke font)
    switch (c) {
        case 'O':
        case '0':
            draw_line_segment(0.0f, 0.0f, 1.0f, 0.0f);
            draw_line_segment(1.0f, 0.0f, 1.0f, 1.0f);
            draw_line_segment(1.0f, 1.0f, 0.0f, 1.0f);
            draw_line_segment(0.0f, 1.0f, 0.0f, 0.0f);
            break;
        case 'N':
            draw_line_segment(0.0f, 0.0f, 0.0f, 1.0f);
            draw_line_segment(0.0f, 1.0f, 1.0f, 0.0f);
            draw_line_segment(1.0f, 0.0f, 1.0f, 1.0f);
            break;
        case 'E':
            draw_line_segment(1.0f, 0.0f, 0.0f, 0.0f);
            draw_line_segment(0.0f, 0.0f, 0.0f, 1.0f);
            draw_line_segment(0.0f, 1.0f, 1.0f, 1.0f);
            draw_line_segment(0.0f, 0.5f, 0.7f, 0.5f);
            break;
        case 'T':
            draw_line_segment(0.0f, 1.0f, 1.0f, 1.0f);
            draw_line_segment(0.5f, 1.0f, 0.5f, 0.0f);
            break;
        case 'W':
            draw_line_segment(0.0f, 0.0f, 0.33f, 1.0f);
            draw_line_segment(0.33f, 1.0f, 0.66f, 0.0f);
            draw_line_segment(0.66f, 0.0f, 1.0f, 1.0f);
            break;
        case 'S':
            draw_line_segment(1.0f, 1.0f, 0.0f, 1.0f);
            draw_line_segment(0.0f, 1.0f, 0.0f, 0.5f);
            draw_line_segment(0.0f, 0.5f, 1.0f, 0.5f);
            draw_line_segment(1.0f, 0.5f, 1.0f, 0.0f);
            draw_line_segment(1.0f, 0.0f, 0.0f, 0.0f);
            break;
        case 'I':
            draw_line_segment(0.5f, 0.0f, 0.5f, 1.0f);
            break;
        case 'V':
            draw_line_segment(0.0f, 1.0f, 0.5f, 0.0f);
            draw_line_segment(0.5f, 0.0f, 1.0f, 1.0f);
            break;
        case 'R':
            draw_line_segment(0.0f, 0.0f, 0.0f, 1.0f);
            draw_line_segment(0.0f, 1.0f, 1.0f, 1.0f);
            draw_line_segment(1.0f, 1.0f, 1.0f, 0.5f);
            draw_line_segment(1.0f, 0.5f, 0.0f, 0.5f);
            draw_line_segment(0.5f, 0.5f, 1.0f, 0.0f);
            break;
        // ... (Add more characters like F, U, etc. if needed for "four" and "five")
        default:
            // Placeholder for unsupported characters
            draw_line_segment(0.0f, 0.0f, 1.0f, 1.0f);
            draw_line_segment(0.0f, 1.0f, 1.0f, 0.0f);
            break;
    }
    glPopMatrix();
}

// Function to draw a 3D string (name) at a world position
void draw_3d_string(const char* str, float x, float y, float z) {
    if (!str) return;

    glPushMatrix();
    glTranslatef(x, y, z);
    glLineWidth(2.0f);
    glColor3f(1.0f, 1.0f, 1.0f); // White text

    // Calculate total width of the string to center it
    size_t len = strlen(str);
    float total_width = (float)len * TEXT_SCALE;
    glTranslatef(-total_width / 2.0f, 0.0f, 0.0f); // Center horizontally

    glBegin(GL_LINES);
    for (size_t i = 0; i < len; ++i) {
        char c = toupper(str[i]); // Convert to uppercase for simplicity
        draw_char(c);
        glTranslatef(TEXT_SCALE * 1.2f, 0.0f, 0.0f); // Move to the next character position
    }
    glEnd();

    glPopMatrix();
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
    static int surface_spawn_count = 0; // The missing declaration!
bool prevSquarePressed = false;

bool prevTrianglePressed = false;
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
            bool squarePressed = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X);
            bool trianglePressed = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_Y); // Assuming Y button for deletion  // SDL 2.x doesn't define BUTTON_SQUARE
            if (squarePressed && !prevSquarePressed) {
    //spawn_cube_under_mario(marioState.position);
    // spawn_surfaces_under_mario(marioState.position, beach_surfaces);

    const char* objectName;
                if (surface_spawn_count == 0) objectName = "one"; //money-32
                else if (surface_spawn_count == 1) objectName = "two";
                else if (surface_spawn_count == 2) objectName = "three";
                else if (surface_spawn_count == 3) objectName = "four";
                else objectName = "five"; // All subsequent ones are "five"

                spawn_surfaces_under_mario(marioState.position, beach_surfaces, objectName);
                surface_spawn_count++; // Increment counter
}
prevSquarePressed = squarePressed;

if (trianglePressed && !prevTrianglePressed) {
                // Example: Delete the object named "one"
                delete_surface_object_by_name("one");
                // Or you could cycle through names to delete:
                // delete_surface_object_by_name(get_next_name_to_delete());
            }
            prevTrianglePressed = trianglePressed;

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
    if (cube.name != NULL) {
        // Draw the name at the center of the cube, lifted by half its size + a buffer
        float text_y = cube.pos[1] + (cube.size / 2.0f) + TEXT_SCALE;
        draw_3d_string(cube.name, cube.pos[0], text_y, cube.pos[2]);
    }
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
