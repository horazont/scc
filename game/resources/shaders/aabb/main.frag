#version 330 core

out vec4 colour;
in float t;

const vec3 colour_a = vec3(1, 1, 0);
const vec3 colour_b = vec3(0, 0, 0);

void main() {
    colour = vec4(mix(colour_a, colour_b, round(fract(t * 10))), 1);
}
