#pragma once

#include "common/versions/versions.h"

#include "game/graphics/opengl_renderer/BucketRenderer.h"
#include "game/graphics/opengl_renderer/Profiler.h"
#include "libsm64/libsm64.h"

class MarioRenderer {
 public:
  MarioRenderer();
  ~MarioRenderer();

  void render(SharedRenderState* render_state,
              ScopedProfilerNode& prof,
              const SM64MarioGeometryBuffers& geometry,
              const uint8_t* marioTexture,
              const float* modelMatrix);

 private:
  GLuint m_vao;
  GLuint m_vbo;
  GLuint m_texture;
};
