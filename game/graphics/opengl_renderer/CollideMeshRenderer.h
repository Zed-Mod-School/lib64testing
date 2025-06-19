#pragma once
#include "common/versions/versions.h"

#include "libsm64/libsm64.h"
extern SM64MarioGeometryBuffers g_geom;
#include "game/graphics/opengl_renderer/BucketRenderer.h"


struct PatColors {
  math::Vector4f pat_mode_colors[4];
  math::Vector4f pat_material_colors[32];
  math::Vector4f pat_event_colors[32];
};

class CollideMeshRenderer {
 public:
  CollideMeshRenderer(GameVersion version);
  void render(SharedRenderState* render_state, ScopedProfilerNode& prof);
  ~CollideMeshRenderer();

 private:
  void init_pat_colors(GameVersion version);

  GLuint m_vao;
  GLuint m_ubo;

  GLuint m_vao_textured = 0;
  GLuint m_vbo_textured = 0;
  GLuint m_vao_untextured = 0;
  GLuint m_vbo_untextured = 0;
  int num_textured_vertices = 0;
  int num_untextured_vertices = 0;
  GLuint m_mario_texture_id = 0;

  PatColors m_colors;
};
