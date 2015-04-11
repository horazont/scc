#version 330 core

in NDData {
    vec3 normal;
} nd_in;

out vec4 color;

void main() {
    color = vec4(normalize(nd_in.normal) * 0.5 + 0.5, 1);
}
