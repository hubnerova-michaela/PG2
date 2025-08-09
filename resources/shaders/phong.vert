#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 uProj_m = mat4(1.0);
uniform mat4 uM_m = mat4(1.0);
uniform mat4 uV_m = mat4(1.0);
uniform mat3 uNormal_m = mat3(1.0); // Normal matrix for transforming normals

out vec3 FragPos;      // Fragment position in world space
out vec3 Normal;       // Normal in world space
out vec2 TexCoords;    // Texture coordinates

void main()
{
    // Calculate fragment position in world space
    FragPos = vec3(uM_m * vec4(aPos, 1.0));
    
    // Transform normal to world space using normal matrix
    Normal = normalize(uNormal_m * aNormal);
    
    // Pass through texture coordinates
    TexCoords = aTexCoords; // Pass texture coordinates to fragment shader
    
    // Calculate final vertex position
    gl_Position = uProj_m * uV_m * uM_m * vec4(aPos, 1.0);
}
