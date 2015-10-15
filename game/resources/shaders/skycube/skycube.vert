#version 330 core

{% include ":/shaders/lib/matrix_block.glsl" %}

in vec3 position;

out vec3 coord;

void main()
{
    coord = position;
    gl_Position = mats.proj * vec4(mat3(mats.view) * position * 100.f, 1.f);
}
