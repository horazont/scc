#version 330 core

{% include ":/shaders/lib/matrix_block.glsl" %}

in vec4 position_t;

out float t;

void main() {
    t = position_t.w;
    vec3 position = position_t.xyz;
    gl_Position = mats.proj * mats.view * vec4(position, 1.f);
}
