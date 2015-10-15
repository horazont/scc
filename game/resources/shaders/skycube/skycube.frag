#version 330 core fragment

uniform samplerCube skycube;

in vec3 coord;

out vec4 colour;

void main()
{
    // colour = vec4(texture(skycube, coord).rgb, 1.f);
    // colour = vec4(1, 0, 0, 1);
    colour = vec4(texture(skycube, coord).rgb, 1.f);
}
