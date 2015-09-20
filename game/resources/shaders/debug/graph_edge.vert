#version 330 core

{% include ":/shaders/lib/matrix_block.glsl" %}

in vec3 position;
in vec2 direction;

out vec3 world;
out vec2 fdirection;

void main()
{
    world = position;
    fdirection = direction;
    gl_Position = mats.proj * mats.view * vec4(position, 1.f);
}
