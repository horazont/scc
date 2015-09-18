#version 330 core
in TerrainData {
    vec3 world;
    vec2 tc0;
    vec3 normal;
    float sandiness;
} terraindata;

uniform vec2 location;
uniform float radius;

uniform sampler2D brush;

out vec4 colour;

{% include ":/shaders/lib/cubehelix.frag" %}

vec4 colourize_density(float density)
{
    vec3 colour = cubehelix(density, M_PI/12., -1.0, 1.0);
    float alpha = min(density, 0.4);
    return vec4(colour * alpha * 1.1, alpha);
}

void main() {
    float u = (terraindata.world.x - location.x) / (2*radius) + 0.5;
    float v = (terraindata.world.y - location.y) / (2*radius) + 0.5;

    if (u < 0 || u > 1 || v < 0 || v > 1) {
        discard;
    }

    float density = texture2D(brush, vec2(u, v)).r;

    colour = colourize_density(density);
}
