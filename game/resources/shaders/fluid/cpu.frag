#version 330 core fragment

{% include ":/shaders/lib/matrix_block.glsl" %}
{% include ":/shaders/lib/universal_shader.frag" %}
{% include ":/shaders/lib/sunlight.frag" %}

out vec4 colour;

in vec3 world;
in float depth;
in vec3 normal;

const float depth_factor = 0.5f;

void main() {
    vec3 eyedir = normalize(mats.world_viewpoint - world);
    vec3 normal = normalize(normal);
    float nDotV = max(1e-5, dot(normal, eyedir));

    const float roughness = 0.f;

    vec3 base_colour = vec3(0.1, 0.2, 1.0);

    vec3 diffuse_colour = base_colour.rgb;
    vec3 specular_colour = vec3(1, 1, 1);

    float alpha = min(1, 1 - pow(0.8, depth_factor*depth));

    vec3 result = sunlight(normal, eyedir, nDotV,
                           diffuse_colour*alpha, specular_colour, roughness,
                           mats.sun_colour.xyz, mats.sun_colour.w,
                           mats.sun_direction);
    result += sunlight(normal, eyedir, nDotV,
                       diffuse_colour*alpha, specular_colour, 1.f,
                       mats.sky_colour.xyz, mats.sky_colour.w,
                       reflect(-mats.sun_direction, vec3(0, 0, 1)));

    colour = vec4(result, alpha);
}
