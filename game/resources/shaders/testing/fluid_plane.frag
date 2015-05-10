#version 330 core


in FluidFData {
    vec2 tc0;
    vec4 fluiddata;
} fluid;

out vec4 colour;

uniform sampler2D fluidmap;
uniform sampler2D waves;

uniform float t;

const vec2 winddir = vec2(1, 0);

const float wave_tc_factor = 64.f;
const float fluiddata_tc_factor = 1080.f/2.f;
const float flow_factor = 8.f;


/*vec2 sample_normal(
        const vec2 wavecoord,
        const vec2 tilecoord,
        const vec2 tileoffset,
        const vec2 timeoffset)
{
    vec2 flowdir  = texture2D(fluidmap, floor(tilecoord+tileoffset)/fluiddata_tc_factor).ba * flow_factor;
    return normalize(flowdir) / 2.0 + 0.5;
}*/

vec2 sample_normal(
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
        flowdir = flowdir + winddir * (1-flowintensity);
        flowintensity = 1.f;
    } else if (flowintensity > 10.f) {
        flowdir /= flowintensity;
        flowdir *= 10.f;
        flowintensity = 10.f;
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

    return texture2D(waves, rotmat*wavecoord-timeoffset).rg;
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

    vec2 normal_tile_A = sample_normal(wavecoord, flow_tex_coord, vec2(0, 0), toffset);
    vec2 normal_tile_B = sample_normal(wavecoord, flow_tex_coord, vec2(0.5, 0), toffset);

    vec2 normal_AB = ff.x * normal_tile_A + (1-ff.x) * normal_tile_B;

    vec2 normal_tile_C = sample_normal(wavecoord, flow_tex_coord, vec2(0, 0.5), toffset);
    vec2 normal_tile_D = sample_normal(wavecoord, flow_tex_coord, vec2(0.5, 0.5), toffset);

    vec2 normal_CD = ff.x * normal_tile_C + (1-ff.x) * normal_tile_D;

    vec2 partial_normal = ff.y * normal_AB + (1-ff.y) * normal_CD;

    /*partial_normal = (partial_normal - 0.5) / (ffscale.y * ffscale.x);*/

    /*partial_normal = (partial_normal - 0.5) * 2.f;

    vec3 final_normal = vec3(partial_normal, sqrt(1-partial_normal.x*partial_normal.x - partial_normal.y*partial_normal.y));*/

    /*colour = vec4(fluiddata.rg / 5.f, 0.f, 1.f);*/


    /*colour = vec4(floor(flow_tex_coord+vec2(-0.5f, -0.5f))/fluiddata_tc_factor*1080.f/10.f, 0.f, 1.f);*/
    colour = vec4(partial_normal, 0.5, 1.f);
}
