#include "MarioRenderer.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "third-party/glad/include/glad/glad.h"

namespace {
void debug_draw_mario_geometry(const SM64MarioGeometryBuffers& geometry) {
  if (!geometry.position || geometry.numTrianglesUsed == 0)
    return;

  struct Vertex {
    float pos[3];
    float color[3];
  };

  float scale = 4096.0f / 50.0f;
  std::vector<Vertex> mario_vertices;
  for (int i = 0; i < geometry.numTrianglesUsed * 3; ++i) {
    float x = geometry.position[i * 3 + 0] * scale;
    float y = geometry.position[i * 3 + 1] * scale;
    float z = geometry.position[i * 3 + 2] * scale;

    float r = 1.0f, g = 1.0f, b = 1.0f;
    if (geometry.color) {
      r = geometry.color[i * 4 + 0];
      g = geometry.color[i * 4 + 1];
      b = geometry.color[i * 4 + 2];
    }

    mario_vertices.push_back({{x, y, z}, {r, g, b}});
  }

  static GLuint mario_vbo = 0;
  static GLuint mario_vao = 0;
  if (mario_vbo == 0)
    glGenBuffers(1, &mario_vbo);
  if (mario_vao == 0)
    glGenVertexArrays(1, &mario_vao);

  glBindVertexArray(mario_vao);
  glBindBuffer(GL_ARRAY_BUFFER, mario_vbo);
  glBufferData(GL_ARRAY_BUFFER, mario_vertices.size() * sizeof(Vertex), mario_vertices.data(),
               GL_DYNAMIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
  glEnableVertexAttribArray(1);

  glDisableVertexAttribArray(2);
  glDisableVertexAttribArray(3);

  glDrawArrays(GL_TRIANGLES, 0, mario_vertices.size());
}
}  // namespace

MarioRenderer::MarioRenderer() {
  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_vbo);
  glGenTextures(1, &m_texture);
}

MarioRenderer::~MarioRenderer() {
  glDeleteVertexArrays(1, &m_vao);
  glDeleteBuffers(1, &m_vbo);
  glDeleteTextures(1, &m_texture);
}

void MarioRenderer::render(SharedRenderState* render_state,
                           ScopedProfilerNode& prof,
                           const SM64MarioGeometryBuffers& geometry,
                           const uint8_t* marioTexture,
                           const float* modelMatrix) {
  if (geometry.numTrianglesUsed == 0 || !geometry.position) {
    printf("[MarioRenderer] No geometry to render.\n");
    debug_draw_mario_geometry(geometry);
    return;
  }

  // glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "MarioRenderer::render");

  GLuint active_shader = 0;
  glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<GLint*>(&active_shader));

  if (active_shader == 0) {
    printf("[MarioRenderer] Warning: no active shader bound.\n");

    return;
  }

  glBindVertexArray(m_vao);
  glObjectLabel(GL_VERTEX_ARRAY, m_vao, -1, "Mario VAO");

  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glObjectLabel(GL_BUFFER, m_vbo, -1, "Mario VBO");

  std::vector<float> interleaved;
  interleaved.reserve(geometry.numTrianglesUsed * 3 * 9);

  for (int i = 0; i < geometry.numTrianglesUsed * 3; ++i) {
    interleaved.push_back(geometry.position[i * 3 + 0]);
    interleaved.push_back(geometry.position[i * 3 + 1]);
    interleaved.push_back(geometry.position[i * 3 + 2]);

    interleaved.push_back(geometry.uv[i * 2 + 0]);
    interleaved.push_back(geometry.uv[i * 2 + 1]);

    interleaved.push_back(1.0f);
    interleaved.push_back(1.0f);
    interleaved.push_back(1.0f);
    interleaved.push_back(1.0f);
  }

  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * interleaved.size(), interleaved.data(),
               GL_DYNAMIC_DRAW);

  glBindTexture(GL_TEXTURE_2D, m_texture);
  glObjectLabel(GL_TEXTURE, m_texture, -1, "Mario Texture");

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SM64_TEXTURE_WIDTH, SM64_TEXTURE_HEIGHT, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, marioTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  const int stride = sizeof(float) * 9;
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)(0));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 3));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 5));
  glEnableVertexAttribArray(2);

  float scale = 4096.0f / 50.0f;
  const float scaled_model[16] = {scale, 0, 0, 0, 0, scale, 0, 0, 0, 0, scale, 0, 0, 0, 0, 1};
  const float* model_to_use = modelMatrix ? modelMatrix : scaled_model;

  GLint modelLoc = glGetUniformLocation(active_shader, "model");
  GLint cameraLoc = glGetUniformLocation(active_shader, "camera");
  if (modelLoc != -1)
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model_to_use);
  if (cameraLoc != -1)
    glUniformMatrix4fv(cameraLoc, 1, GL_FALSE, render_state->camera_matrix[0].data());

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glDrawArrays(GL_TRIANGLES, 0, geometry.numTrianglesUsed * 3);

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);

  glFinish();

  prof.add_draw_call();
  prof.add_tri(geometry.numTrianglesUsed);
}
