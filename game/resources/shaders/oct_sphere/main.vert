#version 330

layout(std140) uniform MatrixBlock {
   layout(row_major) mat4 proj;
   layout(row_major) mat4 view;
   layout(row_major) mat4 model;
   layout(row_major) mat3 normal;
} mats;

in vec3 position;

void main() {
    gl_Position = mats.proj * mats.view * mats.model * vec4(position, 1.f);
}
