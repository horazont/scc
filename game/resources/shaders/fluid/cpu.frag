#version 330 core fragment

out vec4 colour;

in float depth;

const float depth_factor = 0.5f;

void main() {
    colour = vec4(0.05, 0.1, 0.2, min(1, 1 - pow(0.8, depth)));
}
