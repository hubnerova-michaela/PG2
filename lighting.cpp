#include "lighting.hpp"
#include "ShaderProgram.hpp"
#include <iostream>

LightingSystem::LightingSystem()
{
    setupDefaultLights();
}

void LightingSystem::setupDefaultLights()
{
    dirLight = DirectionalLight(
        glm::vec3(-0.3f, -0.8f, -0.2f), // Direction (from top-right, simulating afternoon sun)
        glm::vec3(0.2f, 0.2f, 0.15f),   // Ambient (warm ambient light)
        glm::vec3(1.2f, 1.1f, 0.9f),    // Diffuse (bright warm sunlight)
        glm::vec3(1.3f, 1.2f, 1.0f)     // Specular (very bright sun highlights)
    );

    // Setup bike lights (red side lights)
    // Left bike light
    pointLights[0] = PointLight(
        glm::vec3(-2.0f, 0.5f, 0.0f), // Position (left side of camera)
        1.0f, 0.14f, 0.07f,           // Attenuation (medium range)
        glm::vec3(0.1f, 0.0f, 0.0f),  // Ambient (dim red)
        glm::vec3(1.0f, 0.0f, 0.0f),  // Diffuse (bright red)
        glm::vec3(1.0f, 0.2f, 0.2f)   // Specular (red with slight pink)
    );

    // Right bike light
    pointLights[1] = PointLight(
        glm::vec3(2.0f, 0.5f, 0.0f), // Position (right side of camera)
        1.0f, 0.14f, 0.07f,          // Attenuation (medium range)
        glm::vec3(0.1f, 0.0f, 0.0f), // Ambient (dim red)
        glm::vec3(1.0f, 0.0f, 0.0f), // Diffuse (bright red)
        glm::vec3(1.0f, 0.2f, 0.2f)  // Specular (red with slight pink)
    );

    // Third point light (unused/disabled)
    pointLights[2] = PointLight(
        glm::vec3(0.0f, 0.0f, 0.0f), // Position (disabled)
        1.0f, 1.0f, 1.0f,            // High attenuation (very short range)
        glm::vec3(0.0f, 0.0f, 0.0f), // Ambient (off)
        glm::vec3(0.0f, 0.0f, 0.0f), // Diffuse (off)
        glm::vec3(0.0f, 0.0f, 0.0f)  // Specular (off)
    );

    // Setup spot light (camera headlight)
    spotLight = SpotLight(
        glm::vec3(0.0f, 0.0f, 0.0f),  // Position (will be updated to camera pos)
        glm::vec3(0.0f, 0.0f, -1.0f), // Direction (will be updated to camera front)
        12.5f, 22.5f,                 // Inner and outer cone angles
        1.0f, 0.09f, 0.032f,          // Attenuation
        glm::vec3(0.0f, 0.0f, 0.0f),  // Ambient (no ambient for spotLight)
        glm::vec3(1.0f, 1.0f, 1.0f),  // Diffuse (white)
        glm::vec3(1.0f, 1.0f, 1.0f)   // Specular (white)
    );
}

void LightingSystem::updateLights(float time)
{
    // Animate directional light (sun moving across the sky)
    float sunRotation = time * 0.15f; // Slow rotation across sky
    dirLight.direction = glm::normalize(glm::vec3(
        sin(sunRotation) * 0.8f,         // Move horizontally across sky
        -0.6f - 0.3f * cos(sunRotation), // Vary height (higher at noon)
        cos(sunRotation) * 0.4f - 0.3f   // Move forward/back slightly
        ));

    // Change sun color based on time (sunrise/sunset effect)
    float sunColorFactor = (sin(time * 0.08f) + 1.0f) * 0.5f; // 0 to 1, slower cycle
    dirLight.diffuse = glm::mix(
        glm::vec3(1.5f, 0.6f, 0.3f), // Sunset/sunrise orange-red
        glm::vec3(1.3f, 1.2f, 1.0f)  // Noon bright white-yellow
        ,
        sunColorFactor);

    // Adjust specular to match
    dirLight.specular = dirLight.diffuse * 1.1f;

    // Note: Bike light positions will be updated in main loop relative to camera position
    // The bike lights (pointLights[0] and pointLights[1]) maintain their red color and don't animate
}

void LightingSystem::updateBikeLights(const glm::vec3 &cameraPos, const glm::vec3 &cameraRight)
{
    // Position bike lights on the sides of the camera/player
    float sideOffset = 2.0f;     // Distance from center to each side
    float heightOffset = 0.5f;   // Height above camera center
    float forwardOffset = -0.5f; // Slightly behind camera center

    // Left bike light
    pointLights[0].position = cameraPos +
                              (-cameraRight * sideOffset) +
                              glm::vec3(0.0f, heightOffset, forwardOffset);

    // Right bike light
    pointLights[1].position = cameraPos +
                              (cameraRight * sideOffset) +
                              glm::vec3(0.0f, heightOffset, forwardOffset);
}

void LightingSystem::setupLightUniforms(ShaderProgram &shader, const glm::vec3 &viewPos)
{
    shader.activate();

    shader.setUniform("viewPos", viewPos);

    try
    {
        setupMaterial(shader);
        setupDirectionalLight(shader);
        setupPointLights(shader);
        setupSpotLight(shader);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error setting up lighting uniforms: " << e.what() << std::endl;
    }

    shader.deactivate();
}

void LightingSystem::setupMaterial(ShaderProgram &shader)
{
    shader.setUniform("material.ambient", material.ambient);
    shader.setUniform("material.diffuse", material.diffuse);
    shader.setUniform("material.specular", material.specular);
    shader.setUniform("material.shininess", material.shininess);
    shader.setUniform("material.emission", material.emission);
}

void LightingSystem::setupDirectionalLight(ShaderProgram &shader)
{
    shader.setUniform("dirLight.direction", dirLight.direction);
    shader.setUniform("dirLight.ambient", dirLight.ambient);
    shader.setUniform("dirLight.diffuse", dirLight.diffuse);
    shader.setUniform("dirLight.specular", dirLight.specular);
}

void LightingSystem::setupPointLights(ShaderProgram &shader)
{
    static const std::vector<std::string> uniformNames = {
        "pointLights[0]", "pointLights[1]", "pointLights[2]"};

    for (int i = 0; i < 3; i++)
    {
        const std::string &base = uniformNames[i];

        shader.setUniform(base + ".position", pointLights[i].position);
        shader.setUniform(base + ".constant", pointLights[i].constant);
        shader.setUniform(base + ".linear", pointLights[i].linear);
        shader.setUniform(base + ".quadratic", pointLights[i].quadratic);
        shader.setUniform(base + ".ambient", pointLights[i].ambient);
        shader.setUniform(base + ".diffuse", pointLights[i].diffuse);
        shader.setUniform(base + ".specular", pointLights[i].specular);
    }
}

void LightingSystem::setupSpotLight(ShaderProgram &shader)
{
    shader.setUniform("spotLight.position", spotLight.position);
    shader.setUniform("spotLight.direction", spotLight.direction);
    shader.setUniform("spotLight.cutOff", spotLight.cutOff);
    shader.setUniform("spotLight.outerCutOff", spotLight.outerCutOff);
    shader.setUniform("spotLight.constant", spotLight.constant);
    shader.setUniform("spotLight.linear", spotLight.linear);
    shader.setUniform("spotLight.quadratic", spotLight.quadratic);
    shader.setUniform("spotLight.ambient", spotLight.ambient);
    shader.setUniform("spotLight.diffuse", spotLight.diffuse);
    shader.setUniform("spotLight.specular", spotLight.specular);
}
