#version 330 core fragment

{% include ":/shaders/lib/matrix_block.glsl" %}

uniform sampler2D heightmap;

const float width = 2.f;

in vec3 world;

out vec4 colour;

void main()
{
    vec2 terrain_texcoord = world.xy / terrain_size;

    float terrain_height = -100;
    if (terrain_texcoord.x < 0 || terrain_texcoord.x > 1 ||
            terrain_texcoord.y < 0 || terrain_texcoord.y > 1)
    {
        discard;
    }

    terrain_height = texture2D(heightmap, terrain_texcoord).r;
    float alpha;
    if (terrain_height > world.z) {
        alpha = exp((world.z - terrain_height) * 10.f);
    } else {
        alpha = exp(terrain_height - world.z) * width;
    }

    colour = vec4(alpha, alpha, alpha, 0);
}
