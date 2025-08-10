#include "lighting.hpp"
#include "ShaderProgram.hpp"
#include <iostream>

LightingSystem::LightingSystem() {
    setupDefaultLights();
}

void LightingSystem::setupDefaultLights() {    
    dirLight = DirectionalLight(
        glm::vec3(-0.3f, -0.8f, -0.2f),  // Direction (from top-right, simulating afternoon sun)
        glm::vec3(0.2f, 0.2f, 0.15f),    // Ambient (warm ambient light)
        glm::vec3(1.2f, 1.1f, 0.9f),     // Diffuse (bright warm sunlight)
        glm::vec3(1.3f, 1.2f, 1.0f)      // Specular (very bright sun highlights)
    );
    
    // Setup spot light (camera headlight)
    spotLight = SpotLight(
        glm::vec3(0.0f, 0.0f, 0.0f),     // Position (will be updated to camera pos)
        glm::vec3(0.0f, 0.0f, -1.0f),    // Direction (will be updated to camera front)
        12.5f, 22.5f,                    // Inner and outer cone angles
        1.0f, 0.09f, 0.032f,             // Attenuation
        glm::vec3(0.0f, 0.0f, 0.0f),     // Ambient (no ambient for spotlight)
        glm::vec3(1.0f, 1.0f, 1.0f),     // Diffuse (white)
        glm::vec3(1.0f, 1.0f, 1.0f)      // Specular (white)
    );
}

void LightingSystem::updateLights(float time) {
    // Animate directional light (sun moving across the sky)
    float sunRotation = time * 0.15f; // Slow rotation across sky
    dirLight.direction = glm::normalize(glm::vec3(
        sin(sunRotation) * 0.8f,       // Move horizontally across sky
        -0.6f - 0.3f * cos(sunRotation), // Vary height (higher at noon)
        cos(sunRotation) * 0.4f - 0.3f // Move forward/back slightly
    ));
    
    // Change sun color based on time (sunrise/sunset effect)
    float sunColorFactor = (sin(time * 0.08f) + 1.0f) * 0.5f; // 0 to 1, slower cycle
    dirLight.diffuse = glm::mix(
        glm::vec3(1.5f, 0.6f, 0.3f),  // Sunset/sunrise orange-red
        glm::vec3(1.3f, 1.2f, 1.0f)   // Noon bright white-yellow
    , sunColorFactor);
    
    // Adjust specular to match
    dirLight.specular = dirLight.diffuse * 1.1f;
    float intensity1 = (sin(time * 2.0f) + 1.0f) * 0.5f + 0.3f;  // 0.3 to 1.3
    float intensity2 = (cos(time * 1.5f) + 1.0f) * 0.5f + 0.3f;  // 0.3 to 1.3
    float intensity3 = (sin(time * 2.5f + 1.0f) + 1.0f) * 0.5f + 0.3f;  // 0.3 to 1.3
    
    pointLights[0].diffuse = glm::vec3(1.0f, 0.2f, 0.2f) * intensity1;
    pointLights[1].diffuse = glm::vec3(0.2f, 1.0f, 0.2f) * intensity2;
    pointLights[2].diffuse = glm::vec3(0.2f, 0.2f, 1.0f) * intensity3;
}

void LightingSystem::setupLightUniforms(ShaderProgram& shader, const glm::vec3& viewPos) {
    shader.activate();
    
    shader.setUniform("viewPos", viewPos);
    
    try {
        setupMaterial(shader);
        setupDirectionalLight(shader);
        setupPointLights(shader);
        setupSpotLight(shader);
    } catch (const std::exception& e) {
        std::cerr << "Error setting up lighting uniforms: " << e.what() << std::endl;
    }
    
    shader.deactivate();
}

void LightingSystem::setupMaterial(ShaderProgram& shader) {
    shader.setUniform("material.ambient", material.ambient);
    shader.setUniform("material.diffuse", material.diffuse);
    shader.setUniform("material.specular", material.specular);
    shader.setUniform("material.shininess", material.shininess);
    shader.setUniform("material.emission", material.emission);
}

void LightingSystem::setupDirectionalLight(ShaderProgram& shader) {
    shader.setUniform("dirLight.direction", dirLight.direction);
    shader.setUniform("dirLight.ambient", dirLight.ambient);
    shader.setUniform("dirLight.diffuse", dirLight.diffuse);
    shader.setUniform("dirLight.specular", dirLight.specular);
}

void LightingSystem::setupPointLights(ShaderProgram& shader) {
    static const std::vector<std::string> uniformNames = {
        "pointLights[0]", "pointLights[1]", "pointLights[2]"
    };
    
    for (int i = 0; i < 3; i++) {
        const std::string& base = uniformNames[i];
        
        shader.setUniform(base + ".position", pointLights[i].position);
        shader.setUniform(base + ".constant", pointLights[i].constant);
        shader.setUniform(base + ".linear", pointLights[i].linear);
        shader.setUniform(base + ".quadratic", pointLights[i].quadratic);
        shader.setUniform(base + ".ambient", pointLights[i].ambient);
        shader.setUniform(base + ".diffuse", pointLights[i].diffuse);
        shader.setUniform(base + ".specular", pointLights[i].specular);
    }
}

void LightingSystem::setupSpotLight(ShaderProgram& shader) {
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
