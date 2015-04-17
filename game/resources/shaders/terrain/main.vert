#version 330 core

layout(std140) uniform MatrixBlock {
   layout(row_major) mat4 proj;
   layout(row_major) mat4 view;
   layout(row_major) mat4 model;
   layout(row_major) mat3 normal;
} mats;

uniform float chunk_size;
uniform vec2 chunk_translation;
uniform vec2 heightmap_base;
uniform sampler2D heightmap;
uniform sampler2D normalt;
uniform vec3 lod_viewpoint;

uniform float zoffset;

const float heightmap_factor = 0.03076923076923077;
const float grid_size = 64;
const float scale_to_radius = 1.984375;

in vec2 position;

out TerrainData {
    vec3 world;
    vec2 tc0;
    vec3 normal;
};

vec2 morph_vertex(vec2 grid_pos, vec2 vertex, float morph_k)
{
   vec2 frac_part = fract(grid_pos.xy * grid_size * 0.5) * 2.0 / grid_size;
   if (grid_pos.x == 1.0) { frac_part.x = 0; }
   if (grid_pos.y == 1.0) { frac_part.y = 0; }
   return vertex.xy - frac_part * chunk_size * morph_k;
}

float morph_k(vec3 viewpoint, vec2 world_pos)
{
   float dist = length(viewpoint - vec3(world_pos, 0)) - chunk_size*scale_to_radius;
   float normdist = dist/(scale_to_radius*chunk_size);
   return clamp((normdist - 0.6) * 2.5, 0, 1);
}

void main() {
   vec2 model_vertex = position * chunk_size + chunk_translation;
   vec2 morphed = morph_vertex(position, model_vertex, morph_k(lod_viewpoint, model_vertex));
   vec2 morphed_object = (morphed - chunk_translation) / chunk_size;
   tc0 = morphed.xy / 5.0;

   vec2 lookup_coord = heightmap_base + morphed_object.xy * heightmap_factor;
   float height = textureLod(heightmap, lookup_coord, 0).r;
   normal = textureLod(normalt, lookup_coord, 0).xyz;

   world = vec3(morphed, height);

   float dist = length(lod_viewpoint - world);

   vec4 final_position = mats.proj * mats.view * mats.model * vec4(
               /* morphed, height+zoffset*dist*0.0001, 1.f); */
               morphed, height, 1.f);

   final_position.z -= zoffset*dist*0.0001;
   gl_Position = final_position;
}
