#version 330 core

layout(std140) uniform MatrixBlock {
   layout(row_major) mat4 proj;
   layout(row_major) mat4 view;
   layout(row_major) mat4 model;
   layout(row_major) mat3 normal;
} mats;

in vec3 position;

out vec3 world;

void main() {
    world = position;
    gl_Position = mats.proj * mats.view * vec4(position, 1.f);
}
