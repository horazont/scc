#version 330 core

out vec4 color;
in vec2 tc0;
in vec2 data_texcoord;

uniform sampler2D normalt;
uniform sampler2D grass;

void main() {
   color = vec4(texture2D(normalt, data_texcoord).rgb * 0.5 + 0.5, 1.0);
}
