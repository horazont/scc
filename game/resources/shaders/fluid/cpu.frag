#version 330 core fragment

{% include ":/shaders/lib/matrix_block.glsl" %}
{% include ":/shaders/lib/inv_matrix_block.glsl" %}
{% include ":/shaders/lib/universal_shader.frag" %}
{% include ":/shaders/lib/sunlight.frag" %}
{% include ":/shaders/lib/unproject.glsl" %}
{% include ":/shaders/lib/fluidatten.frag" %}

out vec4 colour;

in vec3 world;
in vec3 data_tc;
in vec3 normal;

#ifdef REFRACTIVE
uniform sampler2DArray fluiddata;
uniform sampler2D scene_colour;
uniform sampler2D scene_depth;
#endif

const float depth_factor = 0.5f;
const float distortion_factor = 0.5f;

void main() {
    vec3 eyedir = normalize(mats.world_viewpoint - world);
    vec3 normal = normalize(normal);
    float nDotV = max(1e-5, dot(normal, eyedir));

    const float roughness = 0.f;

    vec3 base_colour = vec3(0.1, 0.2, 1.0);

    vec3 diffuse_colour = base_colour.rgb;
    vec3 specular_colour = vec3(1, 1, 1);

    //float alpha = min(1, 1 - pow(0.8, depth_factor*depth));

    float fresnel = max(0.f, 1.f - nDotV * 1.3f);

    vec3 reflective = sunlight(normal, eyedir, nDotV,
                           vec3(0, 0, 0), specular_colour, roughness,
                           mats.sun_colour.xyz, mats.sun_colour.w,
                           mats.sun_direction);
    reflective += sunlight(normal, eyedir, nDotV,
                       vec3(0, 0, 0), specular_colour, 1.f,
                       mats.sky_colour.xyz, mats.sky_colour.w,
                       reflect(-mats.sun_direction, vec3(0, 0, 1)));

#ifdef REFRACTIVE
    const float alpha = 1.f;
    vec4 fluiddata = textureLod(fluiddata, data_tc, 0);

    vec2 offs = distortion_factor*fluiddata.g*(mats.view * vec4(normal, 0.f)).xy;

    vec4 eye = mats.view * vec4(world, 1.f);

    float frag_depth = eye.z;

    vec4 scene_lookup_ndc = mats.proj * (eye+vec4(offs, 0., 0.));
    scene_lookup_ndc /= scene_lookup_ndc.w;

    vec2 scene_texcoord = scene_lookup_ndc.xy * 0.5 + 0.5;

    float scene_depth = unproject(vec3(gl_FragCoord.xy, texture2D(scene_depth, scene_texcoord).r)).z;

    vec3 refractive = texture2D(scene_colour, scene_texcoord).rgb;
    refractive *= fluidatten(frag_depth - scene_depth);
#else
    vec3 refractive = vec3(0, 0, 0);
    const float alpha = 0.f;
#endif

    colour = vec4(refractive+fresnel*reflective, alpha);
}
