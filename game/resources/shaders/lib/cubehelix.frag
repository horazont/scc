#version 330 core

const float M_PI = 3.14159;

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
