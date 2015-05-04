#version 330 core

layout(std140) uniform MatrixBlock {
   layout(row_major) mat4 proj;
   layout(row_major) mat4 view;
   layout(row_major) mat4 model;
   layout(row_major) mat3 normal;
} mats;

in vec3 position;

out vec2 tc0;
out vec4 fluiddata;

uniform sampler2D fluidmap;

uniform float width;
uniform float height;

void main()
{
    vec4 world = mats.model * vec4(position, 1.f);
    tc0 = vec2(world.x / width, world.y / height);

    vec4 my_fluiddata = texture2D(fluidmap, tc0);
    fluiddata = my_fluiddata;

    gl_Position = mats.proj * mats.view * vec4(world.xy, my_fluiddata.r+my_fluiddata.g, 1.f);
}
