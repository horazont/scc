#version 330 core

out vec4 color;
in vec2 tc0;
in vec2 data_texcoord;

uniform sampler2D normalt;
uniform sampler2D grass;

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

vec3 sunlight(
    const in vec3 n_normal,
    const in vec3 n_eyedir,
    const in float nDotV,
    const in vec3 diffuse_colour,
    const in vec3 specular_colour,
    const in float roughness)
{
    const vec3 sundiffuse = vec3(1, 0.99, 0.95);
    const float sunpower = 3.f;

    vec3 v_lightdir = vec3(1, 1, 10);
    vec3 n_half = normalize(normalize(v_lightdir) + n_eyedir);

    vec3 n_lightdir = normalize(v_lightdir);
    /* n_half = normalize(v_lightdir + v_eyedir); */

    float nDotL = max(1e-5, dot(n_normal, n_lightdir));
    float nDotH = dot(n_normal, n_half);
    float vDotH = dot(n_eyedir, n_half);

    vec3 diffuse = diffuseF(diffuse_colour);
    vec3 specular =
            specularF(specular_colour, vDotH)
            * specularD(roughness, nDotH)
            * specularG(roughness, nDotV, nDotL);

    return (diffuse + specular) * (nDotL * sundiffuse * sunpower);
}

void main()
{
    vec3 eyedir = normalize(vec3(-1, -1, 1));
    vec3 normal = normalize(texture2D(normalt, data_texcoord).xyz);
    float nDotV = max(1e-5, dot(normal, eyedir));

    const float metallic = 0.1f;
    const float roughness = 0.8f;

    vec3 base_colour = texture2D(grass, tc0).rgb;
    vec3 diffuse_colour = base_colour * (1.f - metallic);
    vec3 specular_colour = mix(vec3(0.04f), base_colour, metallic);

    color = vec4(sunlight(normal, eyedir, nDotV, diffuse_colour, specular_colour, roughness), 1.0f);
}
