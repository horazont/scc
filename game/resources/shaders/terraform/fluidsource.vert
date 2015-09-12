#version 330 core vertex

{% include ":/shaders/lib/matrix_block.glsl" %}

uniform vec2 pos;
uniform float height;
uniform float radius;

in vec3 position;
in vec3 normal;

out vec3 world;
out vec3 fnormal;

void main()
{
    vec3 world_pos = position * vec3(radius, radius, height + 0.1) + vec3(pos.xy, 0.);

    world = world_pos;
    fnormal = normal;

    gl_Position = mats.proj * mats.view * vec4(world_pos, 1.f);
}
