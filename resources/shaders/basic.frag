#version 460 core

in vec3 Normal;
in vec2 TexCoord;

uniform vec4 uniform_Color;
out vec4 FragColor;

void main()
{
    FragColor = uniform_Color;
}
