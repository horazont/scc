#version 330 core

in TerrainData {
    vec3 world;
    vec2 tc0;
    vec3 normal;
} terraindata;

uniform vec2 location;
uniform float radius;

uniform sampler2D brush;

const float M_PI = 3.14159;

out vec4 colour;

vec3 cubehelix(float v, float s, float r, float h)
{
    float a = h*v*(1.f-v)/2.f;
    float phi = 2.*M_PI*(s/3. + r*v);

    float cos_phi = cos(phi);
    float sin_phi = sin(phi);

    float rf = clamp(v + a*(-0.14861*cos_phi + 1.78277*sin_phi), 0, 1);
    float gf = clamp(v + a*(-0.29227*cos_phi - 0.90649*sin_phi), 0, 1);
    float bf = clamp(v + a*(1.97294*cos_phi), 0, 1);

    return vec3(rf, gf, bf);
}

vec4 colourize_density(float density)
{
    vec3 colour = cubehelix(density, M_PI/12., -1.0, 1.0);
    float alpha = min(density, 0.4);
    return vec4(colour, alpha);
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
