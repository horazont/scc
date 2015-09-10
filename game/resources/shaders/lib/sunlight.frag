#version 330 core fragment

vec3 sunlight(
        const in vec3 n_normal,
        const in vec3 n_eyedir,
        const in float nDotV,
        const in vec3 diffuse_colour,
        const in vec3 specular_colour,
        const in float roughness,
        const in vec3 light_colour,
        const in float light_power,
        const in vec3 n_lightdir)
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

    return (diffuse + specular) * (nDotL * light_colour * light_power);
}


vec4 transparent_dirlight(
        const in vec3 n_normal,
        const in vec3 n_eyedir,
        const in float nDotV,
        const in vec3 diffuse_colour,
        const in float diffuse_alpha,
        const in vec3 specular_colour,
        const in float roughness,
        const in vec3 light_colour,
        const in float light_power,
        const in vec3 n_lightdir)
{
    vec3 n_half = normalize(n_lightdir + n_eyedir);

    float nDotL = max(1e-5, dot(n_normal, n_lightdir));
    float nDotH = dot(n_normal, n_half);
    float vDotH = dot(n_eyedir, n_half);

    vec3 factor = nDotL * light_colour * light_power;

    vec3 diffuse = diffuseF(diffuse_colour) * factor;
    vec3 specular =
            specularF(specular_colour, vDotH)
            * specularD(roughness, nDotH)
            * specularG(roughness, nDotV, nDotL) * factor;

    return vec4(diffuse*diffuse_alpha, diffuse_alpha) + vec4(specular, 0.f);
}


vec3 lighting(
        const in vec3 n_normal,
        const in vec3 n_eyedir,
        const in vec3 base_colour,
        const in float metallic,
        const in float roughness)
{
    float nDotV = dot(n_normal, n_eyedir);

    vec3 diffuse_colour = base_colour * (1.f - metallic);
    vec3 specular_colour = mix(vec3(0.04f), base_colour, metallic);

    vec3 result = sunlight(n_normal, n_eyedir, nDotV,
                           diffuse_colour, specular_colour, roughness,
                           mats.sun_colour.xyz, mats.sun_colour.w,
                           mats.sun_direction);
    result += sunlight(n_normal, n_eyedir, nDotV,
                       diffuse_colour, specular_colour, 1.f,
                       mats.sky_colour.xyz, mats.sky_colour.w,
                       reflect(-mats.sun_direction, vec3(0, 0, 1)));

    return result;
}


vec4 transparent_lighting(
        const in vec3 n_normal,
        const in vec3 n_eyedir,
        const in vec4 base_colour,
        const in float metallic,
        const in float roughness)
{
    float nDotV = dot(n_normal, n_eyedir);

    vec3 diffuse_colour = base_colour.rgb * (1.f - metallic);
    vec3 specular_colour = mix(vec3(0.04f), base_colour.xyz, metallic);

    vec4 result = transparent_dirlight(n_normal, n_eyedir, nDotV,
                                       diffuse_colour, base_colour.w, specular_colour, roughness,
                                       mats.sun_colour.xyz, mats.sun_colour.w,
                                       mats.sun_direction);
    result += transparent_dirlight(n_normal, n_eyedir, nDotV,
                                   diffuse_colour, base_colour.w, specular_colour, 1.f,
                                   mats.sky_colour.xyz, mats.sky_colour.w,
                                   reflect(-mats.sun_direction, vec3(0, 0, 1)));

    return result;
}
