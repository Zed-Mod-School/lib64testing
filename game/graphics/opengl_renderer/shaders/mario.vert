#version 410 core

// === Collision mesh inputs ===
layout (location = 0) in vec3 position_in;
layout (location = 1) in uint flags;
layout (location = 2) in vec3 normal_in;
layout (location = 3) in uint pat;

// === Mario inputs === (use different locations!)
layout (location = 4) in vec3 color_in;
layout (location = 5) in vec2 uv_in;


uniform vec4 hvdf_offset;
uniform mat4 camera;
uniform vec4 camera_position;
uniform float fog_constant;
uniform float fog_min;
uniform float fog_max;
uniform int wireframe;
uniform int wireframe_enabled;
uniform int mode;
uniform int version;

// === Outputs ===
out vec4 fragment_color;
out vec3 v_color;
out vec2 v_uv;

void main() {
  // === Camera transform ===
  vec4 transformed = -camera[3].xyzw;
  transformed += -camera[0] * position_in.x;
  transformed += -camera[1] * position_in.y;
  transformed += -camera[2] * position_in.z;

  float Q = fog_constant / transformed[3];
  transformed.xyz *= Q;
  transformed.xyz += hvdf_offset.xyz;
  transformed.xy -= 2048.;
  transformed.z /= 8388608.0;
  transformed.z -= 1.0;
  transformed.x /= 256.0;
  transformed.y /= -128.0;
  transformed.xyz *= transformed.w;

  gl_Position = transformed;

  // Apply any screen adjustments if needed
  gl_Position.y *= SCISSOR_ADJUST * HEIGHT_SCALE;

if (wireframe == 999) {
  v_color = color_in;
  v_uv = uv_in;
  return;  
}


  // === Regular collision rendering ===
  vec3 to_cam = camera_position.xyz - position_in;
  float dist_from_cam = length(to_cam);
  vec3 to_cam_n = to_cam / dist_from_cam;
  float cam_dot = abs(dot(to_cam_n, normal_in));

  fragment_color = vec4(vec3(0.75), 1.0);
  if (wireframe_enabled == 0) {
    fragment_color.rgb *= (pow(cam_dot, 2.0) * 0.35 + 0.65);
  } else {
    fragment_color.rgb *= (pow(cam_dot, 2.5) * 0.25 + 0.75);
  }

  // All pat logic is handled in the fragment shader
}
