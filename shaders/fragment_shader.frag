#version 450

layout(location = 0) in vec3 fragTexCoord;
layout(binding = 1) uniform sampler2DArray texSamplerArray;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture( texSamplerArray, fragTexCoord);
}
