#version 330 core vertex

{% include ":/shaders/lib/matrix_block.glsl" %}

in vec3 position;
in vec3 normal;

out vec3 world;
out vec3 fnormal;

void main()
{
    world = position;
    fnormal = normal;

    float dist = length(mats.world_viewpoint - position);

    vec4 final_position = mats.proj * mats.view * vec4(position, 1.f);

    final_position.z -= dist*0.000001;
    gl_Position = final_position;
}
