#version 460 core

layout (location = 0) in vec3 aPos;

uniform mat4 uProj_m;
uniform mat4 uV_m;
uniform float uPointSize = 10.0; // Base point size

void main()
{
    gl_Position = uProj_m * uV_m * vec4(aPos, 1.0);
    gl_PointSize = uPointSize; // Use uniform point size, can be modified per draw call
}
