#version 330 core

layout(std140) uniform MatrixBlock {
   layout(row_major) mat4 proj;
   layout(row_major) mat4 view;
   layout(row_major) mat4 model;
   layout(row_major) mat3 normal;
} mats;

in vec3 position;

out vec2 tc0;

uniform float width;
uniform float height;

void main()
{
    tc0 = vec2(position.x / width, position.y / height);
    gl_Position = mats.proj * mats.view * vec4(position, 1.f);
}
