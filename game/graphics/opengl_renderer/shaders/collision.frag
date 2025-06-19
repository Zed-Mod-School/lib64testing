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
  // Force inclusion of wireframe even if not visibly used
  if (wireframe == -1) discard;

if (wireframe == 999) {
  vec4 tex_color = texture(u_texture, v_uv);

  // If texture is black or transparent, fallback to vertex color
  bool use_vertex_color = tex_color.a < 0.1 ||
                          (tex_color.r < 0.01 && tex_color.g < 0.01 && tex_color.b < 0.01);

  fragColor = use_vertex_color ? vec4(v_color, 1.0) : tex_color * vec4(v_color, 1.0);
  return;
}

  fragColor = vec4(1.0, 0.0, 1.0, 1.0);  // fallback pink
}
