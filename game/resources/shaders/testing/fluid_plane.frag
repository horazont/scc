#version 330 core

layout(std140) uniform MatrixBlock {
   layout(row_major) mat4 proj;
   layout(row_major) mat4 view;
   layout(row_major) mat4 model;
   layout(row_major) mat3 normal;
} mats;

layout(std140) uniform InvMatrixBlock {
   layout(row_major) mat4 proj;
   layout(row_major) mat4 view;
} inv_mats;

uniform vec3 viewpoint;

in FluidFData {
    vec3 world;
    vec3 normal;
    vec3 tangent;
    vec2 tc0;
    vec4 fluiddata;
} fluid;

out vec4 colour;

uniform sampler2D fluidmap;
uniform sampler2D waves;
uniform sampler2D scene;
uniform sampler2D scene_depth;

uniform vec2 viewport;

uniform float t;

const vec2 winddir = vec2(1, 0);

const float wave_tc_factor = 64.f;
const float fluiddata_tc_factor = 1080.f/2.f;
const float flow_factor = 8.f;

const float max_flow = 10.f;

const float distortion_factor = 10.f;
const float depth_factor = 0.5f;
const vec3 depth_attenuation = vec3(0.7, 0.8, 0.9);


/*vec2 sample_normal(
        const vec2 wavecoord,
        const vec2 tilecoord,
        const vec2 tileoffset,
        const vec2 timeoffset)
{
    vec2 flowdir  = texture2D(fluidmap, floor(tilecoord+tileoffset)/fluiddata_tc_factor).ba * flow_factor;
    return normalize(flowdir) / 2.0 + 0.5;
}*/

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

vec4 unproject(vec3 window_space)
{
    vec3 ndc = vec3(2*window_space.xy / viewport.xy - 1, 0);
    ndc.z = (2*window_space.z - gl_DepthRange.far - gl_DepthRange.near)
            / (gl_DepthRange.far - gl_DepthRange.near);

    float clip_w = mats.proj[3][2] / (ndc.z - mats.proj[2][2]/mats.proj[2][3]);
    vec4 clip = vec4(ndc * clip_w, clip_w);

    return inv_mats.proj * clip;
}

vec3 sample_normal(
        const vec2 wavecoord,
        const vec2 tilecoord,
        const vec2 tileoffset,
        const vec2 timeoffset)
{
    vec2 flowdir  = texture2D(fluidmap, floor(tilecoord+tileoffset)/fluiddata_tc_factor).ba * flow_factor;
    float flowintensity = length(flowdir);
    // // invert the length of flowdir
    // flowdir /= flowintensity*flowintensity;

    /*float flowintensity = min(length(flowdir) * flow_factor, 1.f);*/

    if (flowintensity < 1.f) {
        /*flowdir = flowdir + winddir * (1-flowintensity);*/
        flowdir /= flowintensity;
    } else if (flowintensity > max_flow) {
        flowdir /= flowintensity;
        flowdir *= max_flow;
    }

    /*if (flowintensity > 1e-6) {
         flowdir = normalize(flowdir);
    } else {
        flowdir = vec2(0, 0);
        flowintensity = 0.;
    }

    flowdir = flowdir * flowintensity + winddir * (1.-flowintensity);
    flowdir = normalize(flowdir);*/

    mat2 rotmat = mat2(flowdir.x, -flowdir.y, flowdir.y, flowdir.x);

    return vec3(texture2D(waves, rotmat*wavecoord-timeoffset*max(1.f, flowintensity)).rg, min(1.f, flowintensity));
}

void main()
{
    vec2 toffset = vec2(t*0.08, 0);
    vec2 wavecoord = fluid.tc0 * wave_tc_factor;

    if (fluid.fluiddata.g < 1e-4) {
        discard;
    }

    vec2 flow_tex_coord = fluid.tc0*fluiddata_tc_factor;
    /*vec2 ff = abs(2*(fract(flow_tex_coord)) - 1) - 0.5;*/
    vec2 ff = 1-abs(2*fract(flow_tex_coord)-1);
    /*ff = 0.5-4*ff*ff*ff;
    vec2 ffscale = sqrt(ff*ff + (1-ff)*(1-ff));*/

    vec3 normal_tile_A = sample_normal(wavecoord, flow_tex_coord, vec2(0, 0), toffset);
    vec3 normal_tile_B = sample_normal(wavecoord, flow_tex_coord, vec2(0.5, 0), toffset);

    vec3 normal_AB = ff.x * normal_tile_A + (1-ff.x) * normal_tile_B;

    vec3 normal_tile_C = sample_normal(wavecoord, flow_tex_coord, vec2(0, 0.5), toffset);
    vec3 normal_tile_D = sample_normal(wavecoord, flow_tex_coord, vec2(0.5, 0.5), toffset);

    vec3 normal_CD = ff.x * normal_tile_C + (1-ff.x) * normal_tile_D;

    vec3 partial_normal_data = ff.y * normal_AB + (1-ff.y) * normal_CD;

    vec2 partial_normal = partial_normal_data.xy;
    // scale with intensity
    partial_normal *= partial_normal_data.z;

    /*partial_normal = (partial_normal - 0.5) / (ffscale.y * ffscale.x);*/

    /*partial_normal = (partial_normal - 0.5) * 2.f;

    vec3 final_normal = vec3(partial_normal, sqrt(1-partial_normal.x*partial_normal.x - partial_normal.y*partial_normal.y));*/

    /*colour = vec4(fluiddata.rg / 5.f, 0.f, 1.f);*/


    /*colour = vec4(floor(flow_tex_coord+vec2(-0.5f, -0.5f))/fluiddata_tc_factor*1080.f/10.f, 0.f, 1.f);*/
    /*colour = vec4(normalize(fluid.normal) nDotV/ 2. + 0.5, 1.f);*/

    vec3 vert_tangent = normalize(fluid.tangent);
    vec3 vert_normal = normalize(fluid.normal);
    mat3 normal_matrix = mat3(vert_tangent, -cross(vert_normal, vert_tangent), vert_normal);

    partial_normal *= min(fluid.fluiddata.g, 1.f);
    partial_normal *= 0.3;
    /*partial_normal = vec2(0, 0);*/

    vec3 eyedir = normalize(viewpoint - fluid.world);
    vec3 normal = normalize(normal_matrix * vec3(partial_normal, sqrt(1-partial_normal.x*partial_normal.x-partial_normal.y*partial_normal.y)));
    float nDotV = max(1e-5, dot(normal, eyedir));

    const float metallic = 1.f;
    const float roughness = 0.2f;

    vec3 base_colour = vec3(1., 1., 1.);
    vec3 diffuse_colour = base_colour * (1.f - metallic);
    vec3 specular_colour = mix(vec3(0.04f), base_colour, metallic);

    const vec3 sundiffuse = vec3(1, 0.99, 0.95);
    const float sunpower = 2.3f;
    const vec3 sundir = normalize(vec3(1, 1, 4));
    const vec3 skydiffuse = vec3(0.95, 0.99, 1);
    const float skypower = 1.0f;
    const vec3 skydir = normalize(vec3(-1, -1, 4));

    vec3 reflective = sunlight(normal, eyedir, nDotV, diffuse_colour, specular_colour, roughness, sundir, sundiffuse, sunpower);
    reflective += sunlight(normal, eyedir, nDotV, diffuse_colour, specular_colour, 1.f, skydir, skydiffuse, skypower);

    float fresnel = max(0.f, 1.f - nDotV * 1.3f);

    vec2 offs = 10.f*fluid.fluiddata.g*normal.xy / viewport.xy;
    vec2 scene_texcoord = gl_FragCoord.xy / viewport.xy + offs;

    float scene_depth = unproject(vec3(gl_FragCoord.xy, texture2D(scene_depth, scene_texcoord).r)).z;
    float frag_depth = unproject(gl_FragCoord.xyz).z;

    /*scene_texcoord /= gl_FragCoord.z;*/

    vec3 refractive = texture2D(scene, scene_texcoord).rgb;
    float depth = depth_factor*(frag_depth - scene_depth);
    refractive *= pow(depth_attenuation, vec3(depth));

    colour = vec4(mix(refractive, reflective, fresnel), 1.f);
    /*colour = vec4(vec3(frag_depth), 1.f);*/
    /*colour = vec4(vec3(frag_depth - scene_depth), 1.f);*/
}
