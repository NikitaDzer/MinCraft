#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;

    vec2 origin_pos;
} ubo;


layout(location = 0) in uint in_vertex_data;
layout(location = 1) in uint in_tex_data;

layout(location = 0) out vec3 frag_tex_coord;

void main() {
    float x = float( in_vertex_data & 0x000007FF );
    float y = float( ( in_vertex_data & 0x003FF800 ) >> 11 );
    float z = float( in_vertex_data >> 22 );
    x += ( ubo.origin_pos.x ) * 16;
    y += ( ubo.origin_pos.y ) * 16;

    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(x, y, z, 1.0);

    float u = float( in_tex_data & 0x000001FF );
    float v = float( ( in_tex_data & 0x0003FE00 ) >> 9 );
    float id = float( in_tex_data >> 18 ) - 1;

    frag_tex_coord = vec3( u, v, id );
}
