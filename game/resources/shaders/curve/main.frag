#version 330 core

out vec4 colour;
in float t;

void main() {
    colour = vec4(t, t, t, 1);
}
