#version 330 core vertex

{% include ":/shaders/lib/matrix_block.glsl" %}

uniform sampler2DArray normalt;
uniform sampler2DArray fluiddata;
uniform vec3 lod_viewpoint;
uniform vec2 base;
uniform float chunk_size;
uniform float chunk_lod_scale;
uniform float chunk_lod;
uniform float scale_to_radius;
uniform float layer;

in vec2 position;

#ifndef MINIMAL
out vec3 world;
out vec3 normal;
out vec3 tangent;
out vec3 data_tc;
#endif

vec2 morph_vertex(vec2 vertex, float morph_k)
{
    vec2 frac_part = fract(vertex.xy / chunk_lod_scale * 0.5) * 2.0;
    return vertex.xy + frac_part * chunk_lod_scale * morph_k;
}

void main() {
    float size = chunk_size * scale_to_radius;
    // position is already translated into world space
    vec2 model_vertex = position;
    float morph_k_value = clamp(
                ((length(vec3(position, 0) - lod_viewpoint) - size) / size - 0.6) * 2.5, 0, 1);

    vec2 morphed = morph_vertex(position, morph_k_value) + vec2(0.5, 0.5);

    vec2 lookup_coord = (morphed - vec2(0.5, 0.5) + vec2(0.5, 0.5) * chunk_lod_scale - base) / (chunk_size+chunk_lod_scale);

    vec4 fluid_data = textureLod(fluiddata, vec3(lookup_coord, layer), 0);
    vec3 world_pos = vec3(morphed, fluid_data.x);

#ifndef MINIMAL
    vec4 normalt = textureLod(normalt, vec3(lookup_coord, layer), 0);

    normal = normalt.xyz;
    tangent = vec3(1, 0, normalt.w);
    data_tc = vec3(lookup_coord, layer);

    world = world_pos;
#endif

    gl_Position = mats.proj * mats.view * vec4(world_pos, 1.f);
}
