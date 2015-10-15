#version 330 core


vec3 fluidatten(float depth)
{
    const float depth_factor = 0.5f;
    const vec3 depth_attenuation = vec3(0.7, 0.8, 0.9);
    return pow(depth_attenuation, vec3(depth_factor*depth));
}
