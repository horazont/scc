#version 330 core

uniform float normal_length;

layout(triangles) in;
layout(line_strip, max_vertices=6) out;

{% include ":/shaders/lib/matrix_block.glsl" %}

in TerrainData {
    vec3 world;
    vec2 tc0;
    vec3 normal;
} terrain_in[3];

out NDData {
    vec3 normal;
} nd_out;

uniform sampler2D normalt;

void main() {
   for (int i = 0; i < 3; i++) {
       vec3 normal = terrain_in[i].normal;
       gl_Position = gl_in[i].gl_Position;
       nd_out.normal = normal;
       EmitVertex();
       gl_Position = gl_in[i].gl_Position +
           mats.proj * mats.view * vec4(normal * normal_length, 0.f);
       nd_out.normal = normal;
       EmitVertex();
       EndPrimitive();
   }

   /* for (int i = 0; i < gl_in.length(); i++) {
      gl_Position = gl_in[i].gl_Position;
      nd_out.normal = terrain_in[i].normal;
      /* nd_out[i].data_texcoord = terrain_in[i].data_texcoord;
      nd_out[i].tc0 = terrain_in[i].tc0;
      EmitVertex();
   } */
}
