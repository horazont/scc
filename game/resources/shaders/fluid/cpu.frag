#version 330 core fragment

out vec4 colour;

in float depth;
in vec3 normal;

const float depth_factor = 0.5f;

void main() {
    colour = vec4(normal, 1.f);
}
