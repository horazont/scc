#version 330 core

{% include ":/shaders/lib/matrix_block.glsl" %}

in vec3 world;
in vec2 fdirection;

out vec4 colour;

void main()
{
    vec3 viewdir = normalize(world - mats.world_viewpoint);

    float relative_direction = dot(fdirection, vec2(viewdir));

    if (relative_direction < 0) {
        colour = vec4(1, 0, 0, 1);
    } else {
        colour = vec4(0, 0, 1, 1);
    }
}
