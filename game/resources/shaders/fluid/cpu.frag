#version 330 core fragment

{% include ":/shaders/lib/matrix_block.glsl" %}
{% include ":/shaders/lib/universal_shader.frag" %}
{% include ":/shaders/lib/sunlight.frag" %}

out vec4 colour;

in vec3 world;
in vec3 normal;

const float depth_factor = 0.5f;

void main() {
    vec3 eyedir = normalize(mats.world_viewpoint - world);
    vec3 normal = normalize(normal);
    float nDotV = max(1e-5, dot(normal, eyedir));

    const float metallic = 0.8f;
    const float roughness = 0.1f;

    colour = vec4(lighting(normal, eyedir, vec3(0.1, 0.2, 1.0), metallic, roughness), 1.f);
}
