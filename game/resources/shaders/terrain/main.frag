#version 330 core

{% include ":/shaders/lib/matrix_block.glsl" %}

out vec4 outcolor;

in TerrainData {
    vec3 world;
    vec2 tc0;
    vec3 normal;
    float sandiness;
} terraindata;

uniform vec3 lod_viewpoint;

uniform sampler2D grass;
uniform sampler2D rock;
uniform sampler2D blend;

{% include ":/shaders/lib/universal_shader.frag" %}
{% include ":/shaders/lib/sunlight.frag" %}

vec3 interp_colour(vec3 c1, vec3 c2, float t)
{
    return c1*(1-t) + c2*t;
}

float blend_with_texture(vec2 texcoord, float value)
{
    /*if (value <= 0.0 || value >= 3.0) {
        return value / 3.0;
    }*/

    float result;
    float blendtex = texture2D(blend, texcoord).r;
    if (value > 1.0f && value < 1.5f) {
        value = 1.0f;
    } else if (value >= 1.5f) {
        value -= 0.5f;
    }
    return clamp(blendtex + (value - 1.0f), 0, 1);
}

void main()
{
    vec3 eyedir = normalize(mats.world_viewpoint - terraindata.world);
    vec3 normal = normalize(terraindata.normal);

    const float metallic = 0.0f;
    const float roughness = 0.9f;


    float base_steepness = (1.f - abs(dot(normal, vec3(0, 0, 1))))*7.f;

    float steepness = blend_with_texture(terraindata.tc0/2.f, base_steepness);

    vec3 base_colour = interp_colour(texture2D(grass, terraindata.tc0).rgb,
                                     texture2D(rock, terraindata.tc0).rgb,
                                     steepness);

    vec3 color = lighting(normal, eyedir, base_colour, metallic, roughness);
    outcolor = vec4(vec3(terraindata.sandiness), 1.0f);
}
