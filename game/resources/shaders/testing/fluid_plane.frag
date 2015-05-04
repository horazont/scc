#version 330 core

in vec2 tc0;

uniform sampler2D fluidmap;

out vec4 colour;

void main()
{
    vec4 fluiddata = texture2D(fluidmap, tc0);

    colour = vec4(fluiddata.rg, 0.f, 1.f);
}
