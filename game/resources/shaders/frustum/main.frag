#version 330 core

out vec4 colour;

in vec3 world;

void main() {
    colour = vec4(world / 5.f + 0.5, 1);
}
