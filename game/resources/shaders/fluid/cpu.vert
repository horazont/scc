#version 330 core vertex

{% include ":/shaders/lib/matrix_block.glsl" %}

uniform vec3 lod_viewpoint;
uniform float chunk_size;
uniform float chunk_lod_scale;
uniform float scale_to_radius;

in vec3 position;
in vec4 fluiddata;
in vec4 normal_t;

out vec3 world;
out vec3 normal;
out float depth;

vec2 morph_vertex(vec2 vertex, float morph_k)
{
    vec2 frac_part = fract(vertex.xy / chunk_lod_scale * 0.5) * 2.0;
    return vertex.xy + frac_part * chunk_lod_scale * morph_k;
}

void main() {
    float size = chunk_size * scale_to_radius;

    float morph_k_value = clamp(
                ((length(vec3(position.xy, 0) - lod_viewpoint) - size) / size - 0.6) * 2.5, 0, 1);

    vec2 morphed = morph_vertex(position.xy, morph_k_value) + vec2(0.5, 0.5);

    normal = normal_t.xyz;

    vec3 world_pos = vec3(morphed.xy, position.z);
    world = world_pos;

    depth = fluiddata.y;

    gl_Position = mats.proj * mats.view * vec4(world_pos, 1.f);
}
