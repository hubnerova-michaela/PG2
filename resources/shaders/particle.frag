#version 460 core

out vec4 FragColor;

// Green glow with semi-transparency; can be overridden from CPU via uniform
uniform vec4 particleColor = vec4(0.15, 0.95, 0.2, 0.6);
// Subtle animated flicker; time provided by CPU or derived via gl_FragCoord
uniform float uTime = 0.0;
// Texture for particles (0 = no texture, 1 = use texture)
uniform int useTexture = 0;
uniform sampler2D particleTexture;

float smoothGlow(float r, float rInner, float rOuter)
{
    // Core: inside rInner nearly full intensity
    float core = 1.0 - smoothstep(rInner * 0.9, rInner, r);
    // Halo: smooth falloff between inner and outer
    float halo = 1.0 - smoothstep(rInner, rOuter, r);
    // Combine with emphasis on halo for glow
    return clamp(core * 0.75 + halo, 0.0, 1.0);
}

void main()
{
    // Check if we should use texture
    if (useTexture == 1) {
        // Sample the texture
        vec4 texColor = texture(particleTexture, gl_PointCoord);
        
        // Apply particle color tint and fade based on life
        vec4 finalColor = texColor * particleColor;
        
        // Discard transparent pixels
        if (finalColor.a < 0.01) discard;
        
        FragColor = finalColor;
    } else {
        // Original glow effect for non-textured particles
        // Normalized point sprite coords [-0.5..0.5]
        vec2 coord = gl_PointCoord - vec2(0.5);
        float r = length(coord);

        // Soft circle cutoff a bit past 0.5 to allow feathering
        if (r > 0.55) discard;

        // Inner/outer radii define bright core and glow halo
        float rInner = 0.18;
        float rOuter = 0.52;

        // Subtle flicker using a trig pattern; stable across fragment with slight variance
        float flicker = 0.92 + 0.08 * sin(uTime * 6.3 + coord.x * 12.0 + coord.y * 9.0);

        // Glow intensity
        float glow = smoothGlow(r, rInner, rOuter) * flicker;

        // Additional edge feathering for better blending
        float edge = 1.0 - smoothstep(0.50, 0.55, r);
        glow *= edge;

        // Color grading: brighten center slightly and tint towards lime for vividness
        vec3 base = particleColor.rgb;
        vec3 vivid = mix(base, vec3(0.3, 1.0, 0.35), 0.35);
        vec3 color = mix(base, vivid, clamp(glow * 1.2, 0.0, 1.0));

        // Alpha ramps with glow; clamp for semi-transparency
        float alpha = particleColor.a * clamp(glow, 0.0, 1.0);

        FragColor = vec4(color, alpha);
    }
}
