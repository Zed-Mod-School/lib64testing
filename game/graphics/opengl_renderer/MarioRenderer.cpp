#include "MarioRenderer.h"
#include <vector>
#include "game/graphics/gfx.h"
#include "game/kernel/jak1/Mario1.h"

std::vector<SM64SurfaceObject> g_active_debug_objects;
const float MARIO_SCALE_FACTOR = 4096.0f / 50.0f;

MarioRenderer::MarioRenderer(GameVersion version) {
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_ubo);


    m_cube_vao = 0;
    m_cube_vbo = 0;
    m_cube_line_vbo = 0;
    m_texture = 0;
}

MarioRenderer::~MarioRenderer() {
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_ubo);
    if (m_cube_vao) glDeleteVertexArrays(1, &m_cube_vao);
    if (m_cube_vbo) glDeleteBuffers(1, &m_cube_vbo);
    if (m_cube_line_vbo) glDeleteBuffers(1, &m_cube_line_vbo);
    if (m_vbo_textured) glDeleteBuffers(1, &m_vbo_textured);
    if (m_vbo_untextured) glDeleteBuffers(1, &m_vbo_untextured);
}

//Debugging stuff
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
        float v[3][3];

        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
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
            push_line(0, 1); push_line(1, 2); push_line(2, 0);
        }
    }

    auto shader = render_state->shaders[ShaderId::MARIO].id();
    glUseProgram(shader);

    if (m_cube_vao == 0) glGenVertexArrays(1, &m_cube_vao);
    glBindVertexArray(m_cube_vao);

    if (!triangles.empty()) {
        if (m_cube_vbo == 0) glGenBuffers(1, &m_cube_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_cube_vbo);
        glBufferData(GL_ARRAY_BUFFER, triangles.size() * sizeof(Vertex), triangles.data(), GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
        glDisableVertexAttribArray(5);

        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)triangles.size());
    }

    if (outline && !lines.empty()) {
        if (m_cube_line_vbo == 0) glGenBuffers(1, &m_cube_line_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_cube_line_vbo);
        glBufferData(GL_ARRAY_BUFFER, lines.size() * sizeof(Vertex), lines.data(), GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));

        glLineWidth(2.5f);
        glDrawArrays(GL_LINES, 0, (GLsizei)lines.size());
    }
}

void MarioRenderer::render(SharedRenderState* render_state, ScopedProfilerNode& prof) {
    auto& mario_shader = render_state->shaders[ShaderId::MARIO];
    mario_shader.activate();
    auto shader_id = mario_shader.id();
    glUniform1i(glGetUniformLocation(shader_id, "wireframe"), 999);
    // Set Uniforms
    glUniformMatrix4fv(glGetUniformLocation(shader_id, "camera"), 1, GL_FALSE, render_state->camera_matrix[0].data());
    glUniform4fv(glGetUniformLocation(shader_id, "hvdf_offset"), 1, render_state->camera_hvdf_off.data());
    glUniform4fv(glGetUniformLocation(shader_id, "camera_position"), 1, render_state->camera_pos.data());
    glUniform1f(glGetUniformLocation(shader_id, "fog_constant"), render_state->camera_fog.x());
    glUniform1f(glGetUniformLocation(shader_id, "fog_min"), render_state->camera_fog.y());
    glUniform1f(glGetUniformLocation(shader_id, "fog_max"), render_state->camera_fog.z());
    glUniform1i(glGetUniformLocation(shader_id, "version"), (GLint)render_state->version);

    // Set Pipeline State
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);

    if (g_geom.numTrianglesUsed > 0 && g_geom.position && g_geom.uv) {
        struct MarioVertex { float pos[3]; float color[3]; float uv[2]; };
        std::vector<MarioVertex> untextured_verts;
        std::vector<MarioVertex> textured_verts;

        untextured_verts.reserve(g_geom.numTrianglesUsed * 3);
        textured_verts.reserve(g_geom.numTrianglesUsed * 3);

        for (int i = 0; i < g_geom.numTrianglesUsed * 3; ++i) {
            float u = g_geom.uv[i * 2 + 0];
            float v = g_geom.uv[i * 2 + 1];

            if (u > 1.0f || u < -1.0f) {
                u /= 65535.0f;
                v /= 65535.0f;
            }

            bool has_texture = !(g_geom.uv[i * 2 + 0] == 1.0f && g_geom.uv[i * 2 + 1] == 1.0f);

            MarioVertex vtx = {
                { g_geom.position[i * 3 + 0] * MARIO_SCALE_FACTOR,
                  g_geom.position[i * 3 + 1] * MARIO_SCALE_FACTOR,
                  g_geom.position[i * 3 + 2] * MARIO_SCALE_FACTOR },
                { g_geom.color ? g_geom.color[i * 3 + 0] : 1.0f, // Scale check: 1.0 or 255.0?
                  g_geom.color ? g_geom.color[i * 3 + 1] : 1.0f,
                  g_geom.color ? g_geom.color[i * 3 + 2] : 1.0f },
                { u, v }
            };

            if (has_texture) {
                textured_verts.push_back(vtx);
            } else {
                untextured_verts.push_back(vtx);
            }
        }

        glBindVertexArray(m_vao);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(4);
        glEnableVertexAttribArray(5);

        // --- Draw Untextured ---
        if (!untextured_verts.empty()) {
            if (m_vbo_untextured == 0) glGenBuffers(1, &m_vbo_untextured);
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo_untextured);
            glBufferData(GL_ARRAY_BUFFER, untextured_verts.size() * sizeof(MarioVertex), untextured_verts.data(), GL_DYNAMIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MarioVertex), (void*)offsetof(MarioVertex, pos));
            glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(MarioVertex), (void*)offsetof(MarioVertex, color));
            glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(MarioVertex), (void*)offsetof(MarioVertex, uv));

            glBindTexture(GL_TEXTURE_2D, 0);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)untextured_verts.size());
        }

        // --- Draw Textured ---
        if (!textured_verts.empty()) {
            if (m_vbo_textured == 0) glGenBuffers(1, &m_vbo_textured);
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo_textured);
            glBufferData(GL_ARRAY_BUFFER, textured_verts.size() * sizeof(MarioVertex), textured_verts.data(), GL_DYNAMIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MarioVertex), (void*)offsetof(MarioVertex, pos));
            glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(MarioVertex), (void*)offsetof(MarioVertex, color));
            glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(MarioVertex), (void*)offsetof(MarioVertex, uv));

            // Use the member m_texture which should be loaded with Mario's texture
            glBindTexture(GL_TEXTURE_2D, m_texture);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)textured_verts.size());
        }
    }

    // Render Cubes Debugging
    for (int i = 0; i < numCubes; ++i) {
        float yellow[4] = {1.0f, 1.0f, 0.0f, 0.4f};
        draw_surface_object(spawnedCubes[i].surfaceObj, yellow, render_state, true);
    }

    prof.add_draw_call();
    prof.add_tri(g_geom.numTrianglesUsed);
}