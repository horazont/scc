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

out vec4 color;
in vec2 coord;

uniform sampler2D depth;
uniform vec2 viewport_size;
uniform float zfar;
uniform float znear;

uniform vec2 pmin;
uniform vec2 pmax;

void main()
{
    float d = texture2D(depth, coord).r;
    /* vec4 ss_template = mats.proj*vec4(0, 0, 10.0, 1); */

    vec4 clipspace = vec4(
                coord * 2.f - 1.f,
                d*2.f-1.f,
                1.0);

    /* color = vec4(screen_space.z, screen_space.z, screen_space.z, 0.99); */

    vec4 unprojected = inv_mats.proj * clipspace;
    /* unprojected /= unprojected.w; */

    vec4 world = inv_mats.view * unprojected;
    world /= world.w;

    /* vec2 dy = normalize(vec2(0, pmax.y));
    vec2 dx = normalize(vec2(pmax.x, 0));*/

    if (d >= 0.99999) {
        discard;
    }

    mat4 m = inv_mats.proj;
    color = vec4(m[0].xyz / 2 + 0.5, 1);

}
