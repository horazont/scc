#version 330 core

in TerrainData {
    vec3 world;
    vec2 tc0;
    vec3 normal;
} terraindata;

uniform vec2 location;
const float radius = 5.f;

out vec4 color;

void main() {
    float dist = length(terraindata.world.xy - location) / radius;

    if (dist > 1.) {
        discard;
    }

    float shade = 1. - dist;

    color = vec4(shade, shade, shade, shade);
}
