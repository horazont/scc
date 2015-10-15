#version 330 core

layout(lines_adjacency) in;
layout(triangle_strip, max_vertices=6) out;

in TerrainData {
    vec3 world;
    vec2 tc0;
    vec2 global_lookup;
    vec2 local_lookup;
    vec3 normal;
} terrain_vertex[4];

out TerrainData {
    vec3 world;
    vec2 tc0;
    vec2 global_lookup;
    vec2 local_lookup;
    vec3 normal;
} terrain;

void main()
{
    if (abs(terrain_vertex[1].world.z - terrain_vertex[3].world.z) >
            abs(terrain_vertex[0].world.z - terrain_vertex[2].world.z))
    {
        terrain.world = terrain_vertex[1].world;
        terrain.tc0 = terrain_vertex[1].tc0;
        terrain.normal = terrain_vertex[1].normal;
        terrain.global_lookup = terrain_vertex[1].global_lookup;
        terrain.local_lookup = terrain_vertex[1].local_lookup;
        gl_Position = gl_in[1].gl_Position;
        EmitVertex();

        terrain.world = terrain_vertex[0].world;
        terrain.tc0 = terrain_vertex[0].tc0;
        terrain.normal = terrain_vertex[0].normal;
        terrain.global_lookup = terrain_vertex[0].global_lookup;
        terrain.local_lookup = terrain_vertex[0].local_lookup;
        gl_Position = gl_in[0].gl_Position;
        EmitVertex();

        terrain.world = terrain_vertex[2].world;
        terrain.tc0 = terrain_vertex[2].tc0;
        terrain.normal = terrain_vertex[2].normal;
        terrain.global_lookup = terrain_vertex[2].global_lookup;
        terrain.local_lookup = terrain_vertex[2].local_lookup;
        gl_Position = gl_in[2].gl_Position;
        EmitVertex();

        terrain.world = terrain_vertex[3].world;
        terrain.tc0 = terrain_vertex[3].tc0;
        terrain.normal = terrain_vertex[3].normal;
        terrain.global_lookup = terrain_vertex[3].global_lookup;
        terrain.local_lookup = terrain_vertex[3].local_lookup;
        gl_Position = gl_in[3].gl_Position;
        EmitVertex();

        EndPrimitive();
    } else {
        terrain.world = terrain_vertex[0].world;
        terrain.tc0 = terrain_vertex[0].tc0;
        terrain.normal = terrain_vertex[0].normal;
        terrain.global_lookup = terrain_vertex[0].global_lookup;
        terrain.local_lookup = terrain_vertex[0].local_lookup;
        gl_Position = gl_in[0].gl_Position;
        EmitVertex();

        terrain.world = terrain_vertex[3].world;
        terrain.tc0 = terrain_vertex[3].tc0;
        terrain.normal = terrain_vertex[3].normal;
        terrain.global_lookup = terrain_vertex[3].global_lookup;
        terrain.local_lookup = terrain_vertex[3].local_lookup;
        gl_Position = gl_in[3].gl_Position;
        EmitVertex();

        terrain.world = terrain_vertex[1].world;
        terrain.tc0 = terrain_vertex[1].tc0;
        terrain.normal = terrain_vertex[1].normal;
        terrain.global_lookup = terrain_vertex[1].global_lookup;
        terrain.local_lookup = terrain_vertex[1].local_lookup;
        gl_Position = gl_in[1].gl_Position;
        EmitVertex();

        terrain.world = terrain_vertex[2].world;
        terrain.tc0 = terrain_vertex[2].tc0;
        terrain.normal = terrain_vertex[2].normal;
        terrain.global_lookup = terrain_vertex[2].global_lookup;
        terrain.local_lookup = terrain_vertex[2].local_lookup;
        gl_Position = gl_in[2].gl_Position;
        EmitVertex();

        EndPrimitive();
    }
}
