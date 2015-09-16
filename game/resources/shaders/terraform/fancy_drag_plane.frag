#version 330 core fragment

{% include ":/shaders/lib/matrix_block.glsl" %}
{% include ":/shaders/lib/inv_matrix_block.glsl" %}

uniform vec2 viewport;
uniform sampler2D scene_colour;
uniform sampler2D scene_depth;

const float width = 2.f;

{% include ":/shaders/lib/unproject.glsl" %}

in vec3 world;

out vec4 colour;

void main()
{
    vec4 eye = mats.view * vec4(world, 1.f);

    float frag_depth = eye.z;

    vec4 scene_lookup_ndc = mats.proj * eye;
    scene_lookup_ndc /= scene_lookup_ndc.w;

    vec2 scene_texcoord = scene_lookup_ndc.xy * 0.5 + 0.5;

    float scene_depth = unproject(vec3(gl_FragCoord.xy, texture2D(scene_depth, scene_texcoord).r)).z;

    float alpha = min(0.8, 1.f - min(0.8, abs(frag_depth - scene_depth) / width));

    colour = vec4(alpha, alpha, alpha, 0.4);
}
