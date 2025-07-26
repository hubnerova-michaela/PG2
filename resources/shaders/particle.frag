#version 460 core

out vec4 FragColor;

uniform vec4 particleColor = vec4(1.0, 0.5, 0.2, 1.0);

void main()
{
    // Create circular particle shape
    vec2 coord = gl_PointCoord - vec2(0.5);
    float distance = length(coord);
    
    if (distance > 0.5) {
        discard;
    }
    
    // Fade towards edges
    float alpha = 1.0 - (distance * 2.0);
    alpha = alpha * alpha; // Square for smoother falloff
    
    FragColor = vec4(particleColor.rgb, particleColor.a * alpha);
}
