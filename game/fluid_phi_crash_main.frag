#version 330 core

in vec2 tc0;
in vec4 fluiddata;

out vec4 colour;

uniform float t;

const float wave_tc_factor = 64.f;

void main()
{
    vec2 toffset = vec2(t*0.08, 0);
    vec2 wavecoord = tc0 * wave_tc_factor;

    colour = vec4(wavecoord, 0.f, 1.f);
}
