#version 330 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

uniform mat4 model;
uniform mat4 camera;

out vec2 frag_uv;
out vec4 frag_color;

void main() {
    frag_uv = in_uv;
    frag_color = in_color;
    gl_Position = camera * model * vec4(in_position, 1.0);
}
