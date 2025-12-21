#version 410 core

// === Inputs from vertex shader ===
in vec3 v_color;
in vec2 v_uv;

// === Uniforms ===
uniform sampler2D u_texture;
uniform int wireframe;

// === Output ===
out vec4 fragColor;

void main() {
  if (wireframe == -1) discard;

  if (wireframe == 999) {
    vec4 tex_color = texture(u_texture, v_uv);

    // 1. ALPHA TEST: Discard invisible pixels so they don't block the depth buffer
    if (tex_color.a < 0.1) {
      discard;
    }

    // 2. BLENDING: Multiply texture by vertex color.
    // This makes the black mustache texture stay black,
    // and the white skin textures stay skin-colored.
    fragColor = tex_color * vec4(v_color, 1.0);
    return;
  }

  fragColor = vec4(1.0, 0.0, 1.0, 1.0);
}