#version 330 core

in TerrainData {
    vec3 world;
    vec2 tc0;
    vec3 normal;
} terraindata;

uniform vec2 center;
uniform float radius;

out vec4 color;

void main() {
    float dist = length(terraindata.world.xy - center) / radius;

    if (dist > 1.0) {
        discard;
    }

    color = vec4(dist, dist, dist, 1);
}
