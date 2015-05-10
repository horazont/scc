#version 330 core

layout(std140) uniform MatrixBlock {
   layout(row_major) mat4 proj;
   layout(row_major) mat4 view;
   layout(row_major) mat4 model;
   layout(row_major) mat3 normal;
} mats;


out FluidVData {
    vec2 tc0;
    vec4 fluiddata;
} fluid_vertex;

in vec3 position;

uniform sampler2D fluidmap;

uniform float width;
uniform float height;

void main()
{
    vec4 world = mats.model * vec4(position, 1.f);
    fluid_vertex.tc0 = vec2(world.x / width, world.y / height);

    vec4 my_fluiddata = texture2D(fluidmap, fluid_vertex.tc0);
    fluid_vertex.fluiddata = my_fluiddata;

    gl_Position = vec4(world.xy, my_fluiddata.r + my_fluiddata.g, world.w);
}
