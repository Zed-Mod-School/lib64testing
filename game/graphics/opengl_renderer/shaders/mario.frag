#version 330 core
in vec2 frag_uv;
in vec4 frag_color;
out vec4 frag_output;

uniform sampler2D tex;

void main() {
    vec4 texColor = texture(tex, frag_uv);
    if (texColor.a < 0.01) {
        frag_output = vec4(1.0, 0.0, 0.0, 1.0); // fallback red
    } else {
        frag_output = texColor * frag_color;
    }
}
