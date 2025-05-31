#include "MarioRenderer.h"
#include <vector>
#include <glad/glad.h>    // ✅ Use this instead


#include "common/log/log.h"
#include "common/math/Vector.h"  // Your matrix + vector library
#include "game/graphics/opengl_renderer/Shader.h"

extern GLuint get_shader_program(const Shader& s);


MarioRenderer::MarioRenderer(GameVersion version)
    : m_shader("mario", version) {  // Looks for mario.vert / mario.frag
  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_vbo);
  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

  const int stride = sizeof(float) * 11;

  glEnableVertexAttribArray(0); // pos
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

  glEnableVertexAttribArray(1); // norm
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 3));

  glEnableVertexAttribArray(2); // uv
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 6));

  glEnableVertexAttribArray(3); // color
  glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 8));

  glBindVertexArray(0);
}

MarioRenderer::~MarioRenderer() {
  glDeleteBuffers(1, &m_vbo);
  glDeleteVertexArrays(1, &m_vao);
}

void MarioRenderer::update_geometry(const SM64MarioGeometryBuffers& geo) {
  std::vector<float> vertex_data;
  for (int i = 0; i < geo.numTrianglesUsed * 3; i++) {
    vertex_data.insert(vertex_data.end(), {
      geo.position[i * 3 + 0],
      geo.position[i * 3 + 1],
      geo.position[i * 3 + 2],
      geo.normal[i * 3 + 0],
      geo.normal[i * 3 + 1],
      geo.normal[i * 3 + 2],
      geo.uv[i * 2 + 0],
      geo.uv[i * 2 + 1],
      geo.color[i * 3 + 0] / 255.0f,
      geo.color[i * 3 + 1] / 255.0f,
      geo.color[i * 3 + 2] / 255.0f
    });
  }
  m_vertex_count = geo.numTrianglesUsed * 3;
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, vertex_data.size() * sizeof(float), vertex_data.data(), GL_DYNAMIC_DRAW);
}

void MarioRenderer::render(SharedRenderState* render_state, const math::Matrix4f& camera_matrix) {
  m_shader.activate();
  glBindVertexArray(m_vao);
  GLuint program = get_shader_program(m_shader);
GLint cam_loc = glGetUniformLocation(program, "camera");
if (cam_loc != -1) {
  glUniformMatrix4fv(cam_loc, 1, GL_FALSE, camera_matrix.data());
}


  glDrawArrays(GL_TRIANGLES, 0, m_vertex_count);
}

