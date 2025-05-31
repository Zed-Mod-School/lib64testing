#pragma once
#include "game/graphics/opengl_renderer/BucketRenderer.h"
#include "libsm64/libsm64.h"
#include "common/math/Vector.h"  


class MarioRenderer {
 public:
    MarioRenderer(GameVersion version);
  ~MarioRenderer();
  void update_geometry(const SM64MarioGeometryBuffers& geo);
  
  void render(SharedRenderState* render_state, const math::Matrix4f& camera_matrix);

 private:
  GLuint m_vao = 0;
  GLuint m_vbo = 0;
  
  int m_vertex_count = 0;
  Shader m_shader;
};
