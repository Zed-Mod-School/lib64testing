#include "MarioRenderer.h"

#include <vector>

#include "game/graphics/gfx.h"
#include "game/kernel/jak1/Mario1.h"

MarioRenderer::MarioRenderer(GameVersion version) {
  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

MarioRenderer::~MarioRenderer() {
  glDeleteVertexArrays(1, &m_vao);
  glDeleteBuffers(1, &m_ubo);
}

void MarioRenderer::render(SharedRenderState* render_state, ScopedProfilerNode& prof) {
  render_state->shaders[ShaderId::MARIO].activate();

  glBindVertexArray(m_vao);
  auto shader = render_state->shaders[ShaderId::MARIO].id();
  GLuint block_index = glGetUniformBlockIndex(shader, "PatColors");
  if (block_index != GL_INVALID_INDEX) {
    glUniformBlockBinding(shader, block_index, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_ubo);
  }

  glUniformMatrix4fv(glGetUniformLocation(shader, "camera"), 1, GL_FALSE,
                     render_state->camera_matrix[0].data());
  glUniform4f(glGetUniformLocation(shader, "hvdf_offset"), render_state->camera_hvdf_off[0],
              render_state->camera_hvdf_off[1], render_state->camera_hvdf_off[2],
              render_state->camera_hvdf_off[3]);
  const auto& trans = render_state->camera_pos;
  glUniform4f(glGetUniformLocation(shader, "camera_position"), trans[0], trans[1], trans[2],
              trans[3]);
  glUniform1f(glGetUniformLocation(shader, "fog_constant"), render_state->camera_fog.x());
  glUniform1f(glGetUniformLocation(shader, "fog_min"), render_state->camera_fog.y());
  glUniform1f(glGetUniformLocation(shader, "fog_max"), render_state->camera_fog.z());
  glUniform1i(glGetUniformLocation(shader, "version"), (GLint)render_state->version);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_GEQUAL);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_TRUE);

  static GLuint mario_texture_id = 0;

  if (mario_texture_id == 0 && marioTexture) {
    glGenTextures(1, &mario_texture_id);
    glBindTexture(GL_TEXTURE_2D, mario_texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 704, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, marioTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  if (g_geom.numTrianglesUsed > 0 && g_geom.position) {
    struct MarioVertex {
      float pos[3];
      float color[3];
      float uv[2];
    };

    float scale = 4096.0f / 50.0f;
    std::vector<MarioVertex> untextured_verts;
    std::vector<MarioVertex> textured_verts;

    for (int i = 0; i < g_geom.numTrianglesUsed * 3; ++i) {
      float x = g_geom.position[i * 3 + 0] * scale;
      float y = g_geom.position[i * 3 + 1] * scale;
      float z = g_geom.position[i * 3 + 2] * scale;

      float u = g_geom.uv[i * 2 + 0];
      float v = g_geom.uv[i * 2 + 1];
      if (u > 1.0f) {
        u /= 65535.0f;
        v /= 65535.0f;
      }

      bool has_texture = !(g_geom.uv[i * 2 + 0] == 1 && g_geom.uv[i * 2 + 1] == 1);
      float r = has_texture ? 1.0f : (g_geom.color ? g_geom.color[i * 3 + 0] : 1.0f);
      float g = has_texture ? 1.0f : (g_geom.color ? g_geom.color[i * 3 + 1] : 1.0f);
      float b = has_texture ? 1.0f : (g_geom.color ? g_geom.color[i * 3 + 2] : 1.0f);

      MarioVertex vtx = {{x, y, z}, {r, g, b}, {u, v}};
      if (has_texture)
        textured_verts.push_back(vtx);
      else
        untextured_verts.push_back(vtx);
    }

    auto draw_mario_vbo = [&](const std::vector<MarioVertex>& verts, GLuint& vbo, bool textured) {
      if (vbo == 0)
        glGenBuffers(1, &vbo);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(MarioVertex), verts.data(),
                   GL_DYNAMIC_DRAW);

      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MarioVertex),
                            (void*)offsetof(MarioVertex, pos));
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(MarioVertex),
                            (void*)offsetof(MarioVertex, color));
      glEnableVertexAttribArray(4);
      glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(MarioVertex),
                            (void*)offsetof(MarioVertex, uv));
      glEnableVertexAttribArray(5);
      glDisableVertexAttribArray(3);

      if (textured) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mario_texture_id);
        glUniform1i(glGetUniformLocation(shader, "u_texture"), 0);
      } else {
        glBindTexture(GL_TEXTURE_2D, 0);
      }

      glUniform1i(glGetUniformLocation(shader, "wireframe"), 999);
      glDrawArrays(GL_TRIANGLES, 0, verts.size());
    };

    draw_mario_vbo(untextured_verts, m_vbo_untextured, false);
    draw_mario_vbo(textured_verts, m_vbo_textured, true);
  }

  prof.add_draw_call();
  prof.add_tri(g_geom.numTrianglesUsed);
}
