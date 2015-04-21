#version 330 core

out vec4 outcolor;

in TerrainData {
    vec3 world;
    vec2 tc0;
    vec3 normal;
} terraindata;

uniform vec3 lod_viewpoint;

uniform sampler2D grass;
uniform sampler2D rock;
uniform sampler2D blend;

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
        const in float roughness,
        const in vec3 n_lightdir,
        const in vec3 light_diffuse,
        const in float light_power)
{
    /* const vec3 sundiffuse = vec3(1, 0.99, 0.95);
    const float sunpower = 3.f;

    vec3 v_lightdir = vec3(1, 1, 10); */
    vec3 n_half = normalize(n_lightdir + n_eyedir);

    /* n_half = normalize(v_lightdir + v_eyedir); */

    float nDotL = max(1e-5, dot(n_normal, n_lightdir));
    float nDotH = dot(n_normal, n_half);
    float vDotH = dot(n_eyedir, n_half);

    vec3 diffuse = diffuseF(diffuse_colour);
    vec3 specular =
            specularF(specular_colour, vDotH)
            * specularD(roughness, nDotH)
            * specularG(roughness, nDotV, nDotL);

    return (diffuse + specular) * (nDotL * light_diffuse * light_power);
}

vec3 interp_colour(vec3 c1, vec3 c2, float t)
{
    return c1*(1-t) + c2*t;
}

float blend_with_texture(vec2 texcoord, float value)
{
    /*if (value <= 0.0 || value >= 3.0) {
        return value / 3.0;
    }*/

    float result;
    float blendtex = texture2D(blend, texcoord).r;
    if (value > 1.0f && value < 1.5f) {
        value = 1.0f;
    } else if (value >= 1.5f) {
        value -= 0.5f;
    }
    return clamp(blendtex + (value - 1.0f), 0, 1);
}

void main()
{
    vec3 eyedir = normalize(lod_viewpoint - terraindata.world);
    vec3 normal = normalize(terraindata.normal);
    float nDotV = max(1e-5, dot(normal, eyedir));

    const float metallic = 0.1f;
    const float roughness = 0.8f;


    float base_steepness = (1.f - abs(dot(normal, vec3(0, 0, 1))))*7.f;

    float steepness = blend_with_texture(terraindata.tc0/2.f, base_steepness);

    vec3 base_colour = interp_colour(texture2D(grass, terraindata.tc0).rgb,
                                     texture2D(rock, terraindata.tc0).rgb,
                                     steepness);
    vec3 diffuse_colour = base_colour * (1.f - metallic);
    vec3 specular_colour = mix(vec3(0.04f), base_colour, metallic);

    const vec3 sundiffuse = vec3(1, 0.99, 0.95);
    const float sunpower = 2.3f;
    const vec3 sundir = normalize(vec3(1, 1, 4));
    const vec3 skydiffuse = vec3(0.95, 0.99, 1);
    const float skypower = 1.0f;
    const vec3 skydir = normalize(vec3(-1, -1, 4));

    vec3 color = sunlight(normal, eyedir, nDotV, diffuse_colour, specular_colour, roughness, sundir, sundiffuse, sunpower);
    color += sunlight(normal, eyedir, nDotV, diffuse_colour, specular_colour, roughness, skydir, skydiffuse, skypower);

    outcolor = vec4(color, 1.0f);
}
