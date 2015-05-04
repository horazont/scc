#version 330 core

in vec2 tc0;

in vec4 fluiddata;

out vec4 colour;

void main()
{
    /*vec4 fluiddata = texture2D(fluidmap, tc0);*/

    /*colour = vec4(fluiddata.rg / 5.f, 0.f, 1.f);*/

    if (fluiddata.g < 1e-6) {
        discard;
    }

    colour = vec4(0.5, 0.5, 0.5, 0.5);
}
