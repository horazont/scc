#version 330 core

layout(lines_adjacency) in;
layout(triangle_strip, max_vertices=8) out;

layout(std140) uniform MatrixBlock {
   layout(row_major) mat4 proj;
   layout(row_major) mat4 view;
   layout(row_major) mat4 model;
   layout(row_major) mat3 normal;
} mats;

in FluidVData {
    vec2 tc0;
    vec4 fluiddata;
} fluid_vertex[4];

out FluidFData {
    vec2 tc0;
    vec4 fluiddata;
} fluid;

const float vis_threshold = 1e-6;

vec4 transform(int index, float avg_height)
{
    vec4 world = gl_in[index].gl_Position;
    if (fluid_vertex[index].fluiddata.g < vis_threshold) {
        world.z = avg_height;
    }
    return mats.proj * mats.view * world;
}

void main()
{
    // input vertices are in the order (0, 0), (0, 1), (1, 1), (1, 0)
    // let’s make a triangle strip

    float sum_height = 0;
    float count_height = 0;
    for (int i = 0; i < 4; i++) {
        float fluid_height = fluid_vertex[i].fluiddata.g;
        if (fluid_height >= vis_threshold) {
            sum_height += gl_in[i].gl_Position.z;
            count_height += 1;
        }
    }

    if (count_height == 0) {
        // do not generate vertices
        return;
    }

    float avg_height = sum_height / count_height;

    fluid.tc0 = fluid_vertex[1].tc0;
    fluid.fluiddata = fluid_vertex[1].fluiddata;
    gl_Position = transform(1, avg_height);
    EmitVertex();

    fluid.tc0 = fluid_vertex[0].tc0;
    fluid.fluiddata = fluid_vertex[0].fluiddata;
    gl_Position = transform(0, avg_height);
    EmitVertex();

    fluid.tc0 = fluid_vertex[2].tc0;
    fluid.fluiddata = fluid_vertex[2].fluiddata;
    gl_Position = transform(2, avg_height);
    EmitVertex();

    fluid.tc0 = fluid_vertex[3].tc0;
    fluid.fluiddata = fluid_vertex[3].fluiddata;
    gl_Position = transform(3, avg_height);
    EmitVertex();
    EndPrimitive();

}
