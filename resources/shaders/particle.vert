#version 460 core

layout (location = 0) in vec3 aPos;

uniform mat4 uProj_m;
uniform mat4 uV_m;

void main()
{
    gl_Position = uProj_m * uV_m * vec4(aPos, 1.0);
    gl_PointSize = 5.0; // Fixed point size for now
}
