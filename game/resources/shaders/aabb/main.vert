#version 330 core

layout(std140) uniform MatrixBlock {
   layout(row_major) mat4 proj;
   layout(row_major) mat4 view;
   layout(row_major) mat4 model;
   layout(row_major) mat3 normal;
} mats;

in vec4 position_t;

out float t;

void main() {
    t = position_t.w;
    vec3 position = position_t.xyz;
    gl_Position = mats.proj * mats.view * vec4(position, 1.f);
}
