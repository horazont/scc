#version 330 core fragment

// capacity ranges from 0 to 1 inclusively
uniform float capacity;
uniform vec4 add_colour;

{% include ":/shaders/lib/matrix_block.glsl" %}
{% include ":/shaders/lib/universal_shader.frag" %}
{% include ":/shaders/lib/sunlight.frag" %}

in vec3 world;
in vec3 fnormal;

out vec4 colour;

void main()
{
    vec3 n_normal = normalize(fnormal);
    vec3 n_eyedir = normalize(mats.world_viewpoint - world);


    float factor = abs(1 - dot(n_eyedir, n_normal));
    float alpha = (capacity * 0.9 + 0.05) * factor;

    vec4 base_colour = vec4(0.2, 0.8, 1.0, factor);

    colour = add_colour + vec4(transparent_lighting(n_normal, n_eyedir, base_colour, 0.f, 1.f), base_colour.a);
}
