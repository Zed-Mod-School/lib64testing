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
const float MARIO_SCALE_FACTOR = 4096.0f / 50.0f;
// Change the function signature to be a member of MarioRenderer
void MarioRenderer::draw_surface_object(const SM64SurfaceObject& obj,
                                        const float rgba[4],
                                        SharedRenderState* render_state,
                                        bool outline) {
  struct Vertex {
    float pos[3];
    float color[4];
  };

  std::vector<Vertex> triangles;
  std::vector<Vertex> lines;

  const float* base = obj.transform.position;

  for (uint32_t i = 0; i < obj.surfaceCount; ++i) {
    const auto& tri = obj.surfaces[i];

    float v[3][3];  // 3 verts, xyz
    for (int j = 0; j < 3; ++j) {
      for (int k = 0; k < 3; ++k) {
        // Re-apply the global Mario rendering scale factor to the vertices
        // after adding the object's base position.
        v[j][k] = ((float)tri.vertices[j][k] + base[k]) * MARIO_SCALE_FACTOR;
      }
    }

    for (int j = 0; j < 3; ++j) {
      triangles.push_back({{v[j][0], v[j][1], v[j][2]}, {rgba[0], rgba[1], rgba[2], rgba[3]}});
    }

    if (outline) {
      auto push_line = [&](int a, int b) {
        lines.push_back({{v[a][0], v[a][1], v[a][2]}, {0.0f, 0.0f, 0.0f, 1.0f}});
        lines.push_back({{v[b][0], v[b][1], v[b][2]}, {0.0f, 0.0f, 0.0f, 1.0f}});
      };
      push_line(0, 1);
      push_line(1, 2);
      push_line(2, 0);
    }
  }

  // Use the same shader as Mario for consistency in transformations
  render_state->shaders[ShaderId::MARIO].activate();
  auto shader = render_state->shaders[ShaderId::MARIO].id();

  // Set uniform values for the camera matrix etc. just like for Mario
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

  // ... (rest of GL state saving/restoring and drawing logic)
  // ... (ensure VAO/VBO setup is consistent with how Mario's data is bound if needed)

  // Bind VAO, generate if needed
  if (m_cube_vao == 0)
    glGenVertexArrays(1, &m_cube_vao);
  glBindVertexArray(m_cube_vao);

  // Draw triangles (filled)
  if (!triangles.empty()) {
    if (m_cube_vbo == 0)
      glGenBuffers(1, &m_cube_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, triangles.size() * sizeof(Vertex), triangles.data(),
                 GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(4);
    glDisableVertexAttribArray(5);  // no texture

    glDrawArrays(GL_TRIANGLES, 0, triangles.size());
  }

  // Draw outlines
  if (outline && !lines.empty()) {
    if (m_cube_line_vbo == 0)
      glGenBuffers(1, &m_cube_line_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_cube_line_vbo);
    glBufferData(GL_ARRAY_BUFFER, lines.size() * sizeof(Vertex), lines.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(4);
    glDisableVertexAttribArray(5);

    glLineWidth(2.5f);
    glDrawArrays(GL_LINES, 0, lines.size());
  }

  glBindVertexArray(0);  // Unbind VAO
}

uint32_t MarioRenderer::spawn_cube_under_mario(const float* marioPos, float size) {
  if (numCubes >= MAX_CUBES)
    return 0;

  SM64SurfaceObject obj;
  memset(&obj, 0, sizeof(SM64SurfaceObject));

  Cube& c = spawnedCubes[numCubes++];
  // Set the position of the cube (its center) relative to Mario's position
  // The 'size' here is in internal units, consistent with Mario's internal units
  c.pos[0] = marioPos[0];
  c.pos[1] = marioPos[1];  // Spawn directly below Mario
  c.pos[2] = marioPos[2] + size;
  c.size = size;

  // The transform.position of the SM64SurfaceObject should be the center of the object
  obj.transform.position[0] = c.pos[0];
  obj.transform.position[1] = c.pos[1];
  obj.transform.position[2] = c.pos[2];

  obj.surfaceCount = 12;
  obj.surfaces = (SM64Surface*)malloc(sizeof(SM64Surface) * obj.surfaceCount);

  float half = size / 2.0f;
  float x0 = -half, x1 = half;
  float y0 = -half, y1 = half;
  float z0 = -half, z1 = half;

#define ADD_TRI(i, ax, ay, az, bx, by, bz, cx, cy, cz) \
  do {                                                 \
    obj.surfaces[i].vertices[0][0] = ax;               \
    obj.surfaces[i].vertices[0][1] = ay;               \
    obj.surfaces[i].vertices[0][2] = az;               \
    obj.surfaces[i].vertices[1][0] = bx;               \
    obj.surfaces[i].vertices[1][1] = by;               \
    obj.surfaces[i].vertices[1][2] = bz;               \
    obj.surfaces[i].vertices[2][0] = cx;               \
    obj.surfaces[i].vertices[2][1] = cy;               \
    obj.surfaces[i].vertices[2][2] = cz;               \
    obj.surfaces[i].type = SURFACE_DEFAULT;            \
    obj.surfaces[i].force = 0;                         \
    obj.surfaces[i].terrain = TERRAIN_STONE;           \
  } while (0)

  // Vertices are relative to the object's origin (0,0,0)
  // Top
  ADD_TRI(0, x0, y1, z1, x1, y1, z1, x1, y1, z0);
  ADD_TRI(1, x1, y1, z0, x0, y1, z0, x0, y1, z1);

  // Bottom
  ADD_TRI(2, x1, y0, z1, x0, y0, z1, x0, y0, z0);
  ADD_TRI(3, x0, y0, z0, x1, y0, z0, x1, y0, z1);

  // Front (+Z)
  ADD_TRI(4, x0, y0, z1, x1, y0, z1, x1, y1, z1);
  ADD_TRI(5, x1, y1, z1, x0, y1, z1, x0, y0, z1);

  // Back (-Z)
  ADD_TRI(6, x1, y0, z0, x0, y0, z0, x0, y1, z0);
  ADD_TRI(7, x0, y1, z0, x1, y1, z0, x1, y0, z0);

  // Left (-X)
  ADD_TRI(8, x0, y0, z0, x0, y0, z1, x0, y1, z1);
  ADD_TRI(9, x0, y1, z1, x0, y0, z0, x0, y0, z0);

  // Right (+X)
  ADD_TRI(10, x1, y0, z1, x1, y0, z0, x1, y1, z0);
  ADD_TRI(11, x1, y1, z0, x1, y1, z1, x1, y0, z1);

#undef ADD_TRI

  uint32_t id = sm64_surface_object_create(&obj);
  c.surfaceObj = obj;

  return id;
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

  // Save GL state manually
  GLboolean cullEnabled, blendEnabled, tex2dEnabled, depthWriteEnabled;
  GLint depthFunc;
  glGetBooleanv(GL_CULL_FACE, &cullEnabled);
  glGetBooleanv(GL_BLEND, &blendEnabled);
  // glGetBooleanv(GL_TEXTURE_2D, &tex2dEnabled);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteEnabled);
  glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);

  // Set state for cube drawing
  // glDisable(GL_TEXTURE_2D);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);  // <--- IMPORTANT: Enable blending for transparency
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // <--- Set standard alpha blending function
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_GEQUAL);
  glDepthMask(GL_TRUE);  // Keep writing to depth buffer for now, can be adjusted if needed for
                         // complex transparency

  // Loop through spawnedCubes and call draw_surface_object
  for (int i = 0; i < numCubes; ++i) {
    const Cube& cube = spawnedCubes[i];
    float yellow[4] = {1.0f, 1.0f, 0.0f, 0.4f};  // Alpha is 0.4f, indicating transparency
    this->draw_surface_object(cube.surfaceObj, yellow, render_state, true);
  }

  // Restore previous state
  if (cullEnabled)
    glEnable(GL_CULL_FACE);
  else
    glDisable(GL_CULL_FACE);
  if (blendEnabled)
    glEnable(GL_BLEND);
  else
    glDisable(GL_BLEND);
  // if (tex2dEnabled) glEnable(GL_TEXTURE_2D); else glDisable(GL_TEXTURE_2D);
  glDepthFunc(depthFunc);
  glDepthMask(depthWriteEnabled);

  prof.add_draw_call();
  prof.add_tri(g_geom.numTrianglesUsed);
}