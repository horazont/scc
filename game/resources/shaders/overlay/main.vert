#version 330 core

in vec2 position;
out vec2 coord;

void main()
{
    gl_Position = vec4(position * 2 - 1, 0, 1);
    coord = position;
}
