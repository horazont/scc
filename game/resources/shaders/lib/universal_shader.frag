#version 330 core

vec3 diffuseF(const in vec3 colour)
{
    return colour / 3.14159265358979323846;
}

// Specular F fresnel - Schlick approximation
// [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
// [Lagarde 2012, "Spherical Gaussian approximation for Blinn-Phong, Phong and Fresnel"]
vec3 specularF(const in vec3 colour, const in float vDotH)
{
    float fc = pow(1.0 - vDotH, 5.0);
    return vec3(clamp(50.0 * colour.g, 0.0, 1.0) * fc) + ((1.0 - fc) * colour);
}

// Specular D normal distribution function (NDF) - GGX/Trowbridge-Reitz
// [Walter et al. 2007, "Microfacet models for refraction through rough surfaces"]
float specularD(const in float roughness, const in float nDotH)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = ((nDotH * nDotH) * (a2 - 1.0)) + 1.0;
    return a2 / (3.14159265358979323846 * (d * d));
}

// Specular G - Schlick
// [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
float specularG(const in float roughness,
                const in float nDotV,
                const in float nDotL)
{
    float k = (roughness * roughness) * 0.5;
    vec2 GVL = (vec2(nDotV, nDotL) * (1.0 - k)) + vec2(k);
    return 0.25 / (GVL.x * GVL.y);
}
