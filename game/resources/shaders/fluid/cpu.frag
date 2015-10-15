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
in vec3 tangent;

uniform sampler2DArray fluiddata;
uniform float t;

#ifdef REFRACTIVE
uniform sampler2D scene_colour;
uniform sampler2D scene_depth;

const float distortion_factor = 0.5f;
#endif

#ifdef TILED_FLOW
uniform sampler2D wave_normals;

const vec2 winddir = vec2(1, 0);

const float wave_tc_factor = 64.f/1920.f;
const float fluiddata_tc_factor = 1080.f/2.f;
const float flow_factor = 8.f;

const float max_flow = 10.f;
#endif

const float depth_factor = 0.5f;

#ifdef TILED_FLOW

vec3 sample_normal(
        const vec2 wavecoord,
        const vec3 tilecoord,
        const vec2 tileoffset,
        const vec2 timeoffset)
{
    vec2 flowdir = textureLod(fluiddata, vec3(floor(tilecoord.xy+tileoffset)/(vec2(GRID_SIZE+1)*2.f), tilecoord.z), 0).ba * flow_factor;
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

    return vec3(texture(wave_normals, rotmat*wavecoord-timeoffset*max(1.f, flowintensity)).rg, min(1.f, flowintensity));
}
#endif

void main() {
    vec3 eyedir = normalize(mats.world_viewpoint - world);
    vec4 fluiddata = textureLod(fluiddata, data_tc, 0);

#ifdef TILED_FLOW
    vec2 toffset = vec2(t*0.08, 0);
    /*vec2 ff = abs(2*(fract(flow_tex_coord)) - 1) - 0.5;*/
    vec3 flow_tex_coord = vec3(data_tc.xy*(GRID_SIZE+1)*2.f, data_tc.z);
    vec2 wavecoord = world.xy * wave_tc_factor;
    vec2 ff = 1-abs(2*fract(flow_tex_coord.xy)-1);
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

    vec3 vert_tangent = normalize(tangent);
    vec3 vert_normal = normalize(normal);
    mat3 normal_matrix = mat3(vert_tangent, -cross(vert_normal, vert_tangent), vert_normal);

    partial_normal *= min(fluiddata.g, 1.f);
    partial_normal *= 0.3;
    /*partial_normal = vec2(0, 0);*/

    vec3 normal = normalize(normal_matrix * vec3(partial_normal, sqrt(1-partial_normal.x*partial_normal.x-partial_normal.y*partial_normal.y)));
#else
    vec3 normal = normalize(normal);
#endif
    float nDotV = max(1e-5, dot(normal, eyedir));

    const float roughness = 0.f;

    vec3 base_colour = vec3(0.1, 0.2, 1.0);

    vec3 diffuse_colour = base_colour.rgb;
    vec3 specular_colour = vec3(1, 1, 1);

    //float alpha = min(1, 1 - pow(0.8, depth_factor*depth));

    float fresnel = min(1.f, max(0.f, 1.f - nDotV * 1.3f));

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
