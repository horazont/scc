#version 330 core

layout(std140) uniform MatrixBlock {
   layout(row_major) mat4 proj;
   layout(row_major) mat4 view;
   layout(row_major) mat4 model;
   layout(row_major) mat3 normal;
} mats;


out FluidVData {
    vec3 world;
    vec3 normal;
    vec3 tangent;
    vec2 tc0;
    vec4 fluiddata;
} fluid_vertex;

in vec3 position;

uniform sampler2D fluidmap;

uniform float width;
uniform float height;

const float vis_threshold = 1e-6;

// we have to calculate the normals here

vec2 fetch_abs_height(vec2 tc0, vec2 offs)
{
    vec2 height_data = texture2D(fluidmap, tc0 + offs).rg;
    return vec2(height_data.r + height_data.g, height_data.g);
}

void main()
{
    vec4 world = mats.model * vec4(position, 1.f);

    float stepx = 1. / width;
    float stepy = 1. / height;

    vec2 tc0 = vec2(world.x / width, world.y / height);
    fluid_vertex.tc0 = tc0;

    vec4 my_fluiddata = texture2D(fluidmap, fluid_vertex.tc0);
    fluid_vertex.fluiddata = my_fluiddata;

    vec2 neigh_fluid_hd[4];

    neigh_fluid_hd[0] = fetch_abs_height(tc0, vec2(0, -stepy));
    neigh_fluid_hd[1] = fetch_abs_height(tc0, vec2(0, stepy));
    neigh_fluid_hd[2] = fetch_abs_height(tc0, vec2(-stepx, 0));
    neigh_fluid_hd[3] = fetch_abs_height(tc0, vec2(stepx, 0));

    if (my_fluiddata.g <= vis_threshold) {
        float sum_height = 0.f;
        float ctr_height = 0.f;
        for (int i = 0; i < 4; i++) {
            if (neigh_fluid_hd[i].y > vis_threshold) {
                sum_height += neigh_fluid_hd[i].x;
                ctr_height += 1.f;
            }
        }
        if (ctr_height > 0) {
            world.z = sum_height / ctr_height;
        } else {
            world.z = -1;
        }
    } else {
        world.z = my_fluiddata.r + my_fluiddata.g;
    }

    for (int i = 0; i < 4; i++) {
        if (neigh_fluid_hd[i].y <= vis_threshold) {
            neigh_fluid_hd[i].x = world.z;
        }
    }

    float tx_z = neigh_fluid_hd[3].x - neigh_fluid_hd[2].x;
    float ty_z = neigh_fluid_hd[1].x - neigh_fluid_hd[0].x;

    vec3 tx = normalize(vec3(2, 0, tx_z));
    vec3 ty = normalize(vec3(0, 2, ty_z));

    fluid_vertex.tangent = tx;
    fluid_vertex.normal = normalize(cross(tx, ty));
    fluid_vertex.world = world.xyz;

    gl_Position = mats.proj * mats.view * world;
}
