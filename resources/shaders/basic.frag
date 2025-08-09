#version 460 core

in vec3 Normal;
in vec2 TexCoord;

uniform vec4 uniform_Color;
uniform sampler2D textureSampler;
uniform bool useTexture = false;
out vec4 FragColor;

void main()
{
    if (useTexture) {
        vec4 texColor = texture(textureSampler, TexCoord);
        FragColor = texColor * uniform_Color;
    } else {
        FragColor = uniform_Color;
    }
}
