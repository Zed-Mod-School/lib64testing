#include "CollideMeshRenderer.h"

#include <vector>

#include "game/graphics/gfx.h"
#include "game/graphics/opengl_renderer/background/background_common.h"

const static std::vector<float> material_colors_jak1 = {
    1.0f,  0.7f,  1.0f,   // 0, stone
    0.1f,  2.0f,  2.0f,   // 1, ice
    0.75f, 0.25f, 0.1f,   // 2, quicksand
    0.1f,  0.25f, 0.75f,  // 3, waterbottom
    0.5f,  0.15f, 0.1f,   // 4, tar
    2.0f,  1.5f,  0.5f,   // 5, sand
    1.5f,  0.75f, 0.1f,   // 6, wood
    0.1f,  1.35f, 0.1f,   // 7, grass
    1.7f,  1.3f,  0.1f,   // 8, pcmetal
    1.8f,  1.8f,  1.8f,   // 9, snow
    1.5f,  0.2f,  1.0f,   // 10, deepsnow
    1.2f,  0.5f,  0.3f,   // 11, hotcoals
    1.4f,  0.1f,  0.1f,   // 12, lava
    0.8f,  0.3f,  0.1f,   // 13, crwood
    1.0f,  0.4f,  1.0f,   // 14, gravel
    1.5f,  0.5f,  0.15f,  // 15, dirt
    0.7f,  0.7f,  1.0f,   // 16, metal
    0.1f,  0.1f,  1.2f,   // 17, straw
    0.75f, 1.75f, 0.75f,  // 18, tube
    0.4f,  0.1f,  0.8f,   // 19, swamp
    0.1f,  0.4f,  0.8f,   // 20, stopproj
    1.9f,  0.1f,  1.9f,   // 21, rotate
    1.0f,  1.0f,  1.0f,   // 22, neutral
};

const static std::vector<float> event_colors_jak1 = {
    1.0f, 1.0f, 1.0f,  // 0, none
    0.2f, 1.0f, 1.0f,  // 1, deadly
    0.1f, 1.0f, 0.1f,  // 2, endlessfall
    1.0f, 1.0f, 0.1f,  // 3, burn
    0.1f, 0.1f, 1.0f,  // 4, deadlyup
    1.0f, 0.1f, 0.5f,  // 5, burnup
    1.0f, 0.1f, 0.1f,  // 6, melt
};

const static std::vector<float> mode_colors_jak1 = {
    1.25f, 0.1f, 0.1f,  // 0, ground
    0.1f,  0.1f, 1.0f,  // 1, wall
    1.0f,  0.1f, 1.0f,  // 2, obstacle
};

const static std::vector<float> material_colors_jak2 = {
    1.0f,  0.1f,  1.0f,   // 0, unknown
    0.1f,  2.0f,  2.0f,   // 1, ice
    0.75f, 0.25f, 0.1f,   // 2, quicksand
    0.1f,  0.25f, 0.75f,  // 3, waterbottom
    0.5f,  0.15f, 0.1f,   // 4, tar
    2.0f,  1.5f,  0.5f,   // 5, sand
    1.5f,  0.75f, 0.1f,   // 6, wood
    0.1f,  1.35f, 0.1f,   // 7, grass
    1.7f,  1.3f,  0.1f,   // 8, pcmetal
    1.8f,  1.8f,  1.8f,   // 9, snow
    1.5f,  0.2f,  1.0f,   // 10, deepsnow
    1.2f,  0.5f,  0.3f,   // 11, hotcoals
    1.4f,  0.1f,  0.1f,   // 12, lava
    0.8f,  0.3f,  0.1f,   // 13, crwood
    1.0f,  0.4f,  1.0f,   // 14, gravel
    1.5f,  0.5f,  0.15f,  // 15, dirt
    0.7f,  0.7f,  1.0f,   // 16, metal
    0.1f,  0.1f,  1.2f,   // 17, straw
    0.75f, 1.75f, 0.75f,  // 18, tube
    0.4f,  0.1f,  0.8f,   // 19, swamp
    0.1f,  0.4f,  0.8f,   // 20, stopproj
    1.9f,  0.1f,  1.9f,   // 21, rotate
    1.0f,  1.0f,  1.0f,   // 22, neutral
    1.0f,  0.7f,  1.0f,   // 23, stone
    0.8f,  1.2f,  1.2f,   // 24, crmetal
    0.7f,  0.0f,  0.0f,   // 25, carpet
    0.1f,  0.9f,  0.1f,   // 26, grmetal
    1.4f,  0.7f,  0.1f,   // 27, shmetal
    0.5f,  0.5f,  0.0f,   // 28, hdwood
};

const static std::vector<float> event_colors_jak2 = {
    1.0f, 1.0f, 1.0f,  // 0, none
    0.5f, 1.0f, 1.0f,  // 1, deadly
    0.1f, 1.0f, 0.1f,  // 2, endlessfall
    1.0f, 1.0f, 0.1f,  // 3, burn
    0.1f, 0.1f, 1.0f,  // 4, deadlyup
    1.0f, 0.1f, 0.5f,  // 5, burnup
    1.0f, 0.1f, 0.1f,  // 6, melt
    0.1f, 0.7f, 0.7f,  // 7, slide
    1.0f, 0.2f, 1.0f,  // 8, lip
    0.5f, 0.2f, 1.0f,  // 9, lipramp
    0.1f, 0.5f, 1.0f,  // 10, shock
    0.5f, 0.6f, 1.0f,  // 11, shockup
    0.5f, 0.6f, 0.5f,  // 12, hide
    0.5f, 1.0f, 0.5f,  // 13, rail
    0.7f, 0.7f, 0.7f,  // 14, slippery
};

const static std::vector<float> mode_colors_jak2 = {
    1.25f, 0.1f, 0.1f,  // 0, ground
    0.1f,  0.1f, 1.0f,  // 1, wall
    1.0f,  0.1f, 1.0f,  // 2, obstacle
    1.0f,  1.0f, 0.1f,  // 3, halfpipe
};

CollideMeshRenderer::CollideMeshRenderer(GameVersion version) {
  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_ubo);

  init_pat_colors(version);

  glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(m_colors), &m_colors, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

CollideMeshRenderer::~CollideMeshRenderer() {
  glDeleteVertexArrays(1, &m_vao);
  glDeleteBuffers(1, &m_ubo);
}

void CollideMeshRenderer::init_pat_colors(GameVersion version) {
  for (int i = 0; i < 4; ++i) {
    m_colors.pat_mode_colors[i].x() = -1.f;
    m_colors.pat_mode_colors[i].y() = -1.f;
    m_colors.pat_mode_colors[i].z() = -1.f;
  }
  for (int i = 0; i < 32; ++i) {
    m_colors.pat_material_colors[i].x() = -1.f;
    m_colors.pat_material_colors[i].y() = -1.f;
    m_colors.pat_material_colors[i].z() = -1.f;
  }
  for (int i = 0; i < 32; ++i) {
    m_colors.pat_event_colors[i].x() = -1.f;
    m_colors.pat_event_colors[i].y() = -1.f;
    m_colors.pat_event_colors[i].z() = -1.f;
  }

  const std::vector<float>* material_colors = nullptr;
  const std::vector<float>* event_colors = nullptr;
  const std::vector<float>* mode_colors = nullptr;

  switch (version) {
    case GameVersion::Jak1:
      material_colors = &material_colors_jak1;
      event_colors = &event_colors_jak1;
      mode_colors = &mode_colors_jak1;
      break;
    case GameVersion::Jak2:
    case GameVersion::Jak3:
      material_colors = &material_colors_jak2;
      event_colors = &event_colors_jak2;
      mode_colors = &mode_colors_jak2;
      break;
  }

  if (material_colors) {
    for (size_t i = 0; i < material_colors->size(); ++i) {
      m_colors.pat_material_colors[i / 3].data()[i % 3] = material_colors->at(i);
    }
  }
  if (event_colors) {
    for (size_t i = 0; i < event_colors->size(); ++i) {
      m_colors.pat_event_colors[i / 3].data()[i % 3] = event_colors->at(i);
    }
  }
  if (mode_colors) {
    for (size_t i = 0; i < mode_colors->size(); ++i) {
      m_colors.pat_mode_colors[i / 3].data()[i % 3] = mode_colors->at(i);
    }
  }
}

void CollideMeshRenderer::render(SharedRenderState* render_state, ScopedProfilerNode& prof) {
  if (!render_state->has_pc_data) {
    return;
  }

  auto levels = render_state->loader->get_in_use_levels();
  if (levels.empty()) {
    return;
  }
  render_state->shaders[ShaderId::COLLISION].activate();

  glBindVertexArray(m_vao);
  auto shader = render_state->shaders[ShaderId::COLLISION].id();
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
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // ?
  glDepthMask(GL_TRUE);

  for (auto lev : levels) {
    glBindBuffer(GL_ARRAY_BUFFER, lev->collide_vertices);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(0,                                      // location 0 in the shader
                          3,                                      // 3 values per vert
                          GL_FLOAT,                               // floats
                          GL_FALSE,                               // normalized
                          sizeof(tfrag3::CollisionMesh::Vertex),  // stride
                          0                                       // offset (0)
    );
    glVertexAttribIPointer(1,                                      // location 1 in the shader
                           1,                                      // 3 values per vert
                           GL_UNSIGNED_INT,                        // u32
                           sizeof(tfrag3::CollisionMesh::Vertex),  // stride
                           (void*)offsetof(tfrag3::CollisionMesh::Vertex, flags)  // offset
    );
    glVertexAttribPointer(2,                                      // location 2 in the shader
                          3,                                      // 3 values per vert
                          GL_SHORT,                               // floats
                          GL_TRUE,                                // normalized
                          sizeof(tfrag3::CollisionMesh::Vertex),  // stride
                          (void*)offsetof(tfrag3::CollisionMesh::Vertex, nx)  // offset (0)
    );
    glVertexAttribIPointer(3,                                      // location 3 in the shader
                           1,                                      // 3 values per vert
                           GL_UNSIGNED_INT,                        // u32
                           sizeof(tfrag3::CollisionMesh::Vertex),  // stride
                           (void*)offsetof(tfrag3::CollisionMesh::Vertex, pat)  // offset (0)
    );
    glUniform1i(glGetUniformLocation(shader, "wireframe"), 0);
    glUniform1i(glGetUniformLocation(shader, "wireframe_enabled"),
                Gfx::g_global_settings.collision_wireframe);
    glUniform1uiv(glGetUniformLocation(shader, "collision_mode_mask"),
                  Gfx::g_global_settings.collision_mode_mask.size(),
                  Gfx::g_global_settings.collision_mode_mask.data());
    glUniform1uiv(glGetUniformLocation(shader, "collision_event_mask"),
                  Gfx::g_global_settings.collision_event_mask.size(),
                  Gfx::g_global_settings.collision_event_mask.data());
    glUniform1uiv(glGetUniformLocation(shader, "collision_material_mask"),
                  Gfx::g_global_settings.collision_material_mask.size(),
                  Gfx::g_global_settings.collision_material_mask.data());
    glUniform1ui(glGetUniformLocation(shader, "collision_skip_mask"),
                 Gfx::g_global_settings.collision_skip_mask);
    glUniform1ui(glGetUniformLocation(shader, "collision_skip_hide_mask"),
                 Gfx::g_global_settings.collision_skip_hide_mask);
    glUniform1ui(glGetUniformLocation(shader, "collision_skip_nomask_allowed"),
                 Gfx::g_global_settings.collision_skip_nomask_allowed);
    glUniform1i(glGetUniformLocation(shader, "mode"), Gfx::g_global_settings.collision_mode);
    // glDrawArrays(GL_TRIANGLES, 0, lev->level->collision.vertices.size());

    if (Gfx::g_global_settings.collision_wireframe) {
      glUniform1i(glGetUniformLocation(shader, "wireframe"), 1);
      glDisable(GL_BLEND);
      glDepthMask(GL_FALSE);
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      // glDrawArrays(GL_TRIANGLES, 0, lev->level->collision.vertices.size());
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      glEnable(GL_BLEND);
      glDepthMask(GL_TRUE);
    }
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Draw Mario");

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
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(MarioVertex), verts.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MarioVertex), (void*)offsetof(MarioVertex, pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(MarioVertex), (void*)offsetof(MarioVertex, color));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(MarioVertex), (void*)offsetof(MarioVertex, uv));
    glEnableVertexAttribArray(5);
    glDisableVertexAttribArray(3);  // Not used

    if (textured) {
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



    glPopDebugGroup();

    prof.add_draw_call();
    prof.add_tri(lev->level->collision.vertices.size() / 3);
  }
}
