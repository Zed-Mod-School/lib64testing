// MarioRenderer.h
#pragma once
#include "common/versions/versions.h"

#include "libsm64.h"
extern SM64MarioGeometryBuffers g_geom;
#include "game/graphics/opengl_renderer/BucketRenderer.h"

#define MAX_CUBES 64

struct Cube {
  float pos[3];
  float size;
  SM64SurfaceObject surfaceObj;
};

class MarioRenderer {
 public:
  MarioRenderer(GameVersion version);
  static uint32_t spawn_cube_under_mario(const float* marioPos,
                                         float size = 1000.0f);  // Adjust default size
  // Add SharedRenderState* render_state to the signature
  void draw_surface_object(const SM64SurfaceObject& obj,
                           const float rgba[4],
                           SharedRenderState* render_state,
                           bool outline = true);

  void render(SharedRenderState* render_state, ScopedProfilerNode& prof);
  ~MarioRenderer();
  GLuint m_cube_vao = 0;
  GLuint m_cube_vbo = 0;
  GLuint m_cube_line_vbo = 0;

 private:
  void init_pat_colors(GameVersion version);

  GLuint m_vao;
  GLuint m_ubo;
  inline static Cube spawnedCubes[MAX_CUBES];
  inline static int numCubes = 0;

  GLuint m_vao_textured = 0;
  GLuint m_vbo_textured = 0;
  GLuint m_vao_untextured = 0;
  GLuint m_vbo_untextured = 0;

  GLuint m_texture;
  int m_num_mario_verts;
  int m_num_mario_textured_verts;
  int m_num_mario_untextured_verts;

  uint8_t* marioTexture = nullptr;
};