#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aColor;

uniform mat4 camera;

out vec3 vColor;
out vec2 vUV;

void main() {
    gl_Position = camera * vec4(aPos, 1.0);
    vColor = aColor;
    vUV = aUV;
}
