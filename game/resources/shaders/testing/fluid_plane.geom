#version 330 core

layout(lines_adjacency) in;
layout(triangle_strip, max_vertices=4) out;

layout(std140) uniform MatrixBlock {
   layout(row_major) mat4 proj;
   layout(row_major) mat4 view;
   layout(row_major) mat4 model;
   layout(row_major) mat3 normal;
} mats;

in FluidVData {
    vec3 world;
    float eye_z;
    vec3 normal;
    vec3 tangent;
    vec2 tc0;
    vec4 fluiddata;
} fluid_vertex[4];

out FluidFData {
    vec3 world;
    float eye_z;
    vec3 normal;
    vec3 tangent;
    vec2 tc0;
    vec4 fluiddata;
} fluid;

const float vis_threshold = 1e-6;

void main()
{
    // input vertices are in the order (0, 0), (0, 1), (1, 1), (1, 0)
    // let’s make a triangle strip

    float count_height = 0;
    for (int i = 0; i < 4; i++) {
        float fluid_height = fluid_vertex[i].fluiddata.g;
        if (fluid_height >= vis_threshold) {
            count_height += 1;
        }
    }

    if (count_height == 0) {
        // do not generate vertices
        return;
    }

    if (abs(fluid_vertex[1].world.z - fluid_vertex[3].world.z) >
            abs(fluid_vertex[0].world.z - fluid_vertex[2].world.z))
    {
        fluid.world = fluid_vertex[1].world;
        fluid.normal = fluid_vertex[1].normal;
        fluid.tangent = fluid_vertex[1].tangent;
        fluid.tc0 = fluid_vertex[1].tc0;
        fluid.fluiddata = fluid_vertex[1].fluiddata;
        gl_Position = gl_in[1].gl_Position;
        EmitVertex();

        fluid.world = fluid_vertex[0].world;
        fluid.normal = fluid_vertex[0].normal;
        fluid.tangent = fluid_vertex[0].tangent;
        fluid.tc0 = fluid_vertex[0].tc0;
        fluid.fluiddata = fluid_vertex[0].fluiddata;
        gl_Position = gl_in[0].gl_Position;
        EmitVertex();

        fluid.world = fluid_vertex[2].world;
        fluid.normal = fluid_vertex[2].normal;
        fluid.tangent = fluid_vertex[2].tangent;
        fluid.tc0 = fluid_vertex[2].tc0;
        fluid.fluiddata = fluid_vertex[2].fluiddata;
        gl_Position = gl_in[2].gl_Position;
        EmitVertex();

        fluid.world = fluid_vertex[3].world;
        fluid.normal = fluid_vertex[3].normal;
        fluid.tangent = fluid_vertex[3].tangent;
        fluid.tc0 = fluid_vertex[3].tc0;
        fluid.fluiddata = fluid_vertex[3].fluiddata;
        gl_Position = gl_in[3].gl_Position;
        EmitVertex();

        EndPrimitive();
    } else {
        fluid.world = fluid_vertex[0].world;
        fluid.normal = fluid_vertex[0].normal;
        fluid.tangent = fluid_vertex[0].tangent;
        fluid.tc0 = fluid_vertex[0].tc0;
        fluid.fluiddata = fluid_vertex[0].fluiddata;
        gl_Position = gl_in[0].gl_Position;
        EmitVertex();

        fluid.world = fluid_vertex[3].world;
        fluid.normal = fluid_vertex[3].normal;
        fluid.tangent = fluid_vertex[3].tangent;
        fluid.tc0 = fluid_vertex[3].tc0;
        fluid.fluiddata = fluid_vertex[3].fluiddata;
        gl_Position = gl_in[3].gl_Position;
        EmitVertex();

        fluid.world = fluid_vertex[1].world;
        fluid.normal = fluid_vertex[1].normal;
        fluid.tangent = fluid_vertex[1].tangent;
        fluid.tc0 = fluid_vertex[1].tc0;
        fluid.fluiddata = fluid_vertex[1].fluiddata;
        fluid.normal = fluid_vertex[1].normal;
        gl_Position = gl_in[1].gl_Position;
        EmitVertex();

        fluid.world = fluid_vertex[2].world;
        fluid.normal = fluid_vertex[2].normal;
        fluid.tangent = fluid_vertex[2].tangent;
        fluid.tc0 = fluid_vertex[2].tc0;
        fluid.fluiddata = fluid_vertex[2].fluiddata;
        gl_Position = gl_in[2].gl_Position;
        EmitVertex();

        EndPrimitive();
    }

}
