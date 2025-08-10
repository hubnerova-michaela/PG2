#include "lighting.hpp"
#include "ShaderProgram.hpp"
#include <iostream>

LightingSystem::LightingSystem() {
    setupDefaultLights();
}

void LightingSystem::setupDefaultLights() {
    // Setup material with nice default values
    material = Material(
        glm::vec3(0.1f, 0.1f, 0.1f),   // Ambient
        glm::vec3(0.6f, 0.6f, 0.6f),   // Diffuse  
        glm::vec3(0.8f, 0.8f, 0.8f),   // Specular
        64.0f                          // Shininess
    );
    
    // Setup directional light (sun)
    dirLight = DirectionalLight(
        glm::vec3(-0.2f, -1.0f, -0.3f),  // Direction (from top-left)
        glm::vec3(0.15f, 0.15f, 0.15f),  // Ambient (soft)
        glm::vec3(0.8f, 0.8f, 0.7f),     // Diffuse (warm sunlight)
        glm::vec3(1.0f, 1.0f, 0.9f)      // Specular (bright)
    );
    
    // Setup point lights with different colors and positions
    // Light 1: Red light
    pointLights[0] = PointLight(
        glm::vec3(2.0f, 1.0f, -1.0f),    // Position
        1.0f, 0.09f, 0.032f,             // Attenuation
        glm::vec3(0.05f, 0.0f, 0.0f),    // Ambient (dark red)
        glm::vec3(1.0f, 0.2f, 0.2f),     // Diffuse (bright red)
        glm::vec3(1.0f, 0.5f, 0.5f)      // Specular (red-white)
    );
    
    // Light 2: Green light
    pointLights[1] = PointLight(
        glm::vec3(-2.0f, 1.0f, -1.0f),   // Position
        1.0f, 0.09f, 0.032f,             // Attenuation
        glm::vec3(0.0f, 0.05f, 0.0f),    // Ambient (dark green)
        glm::vec3(0.2f, 1.0f, 0.2f),     // Diffuse (bright green)
        glm::vec3(0.5f, 1.0f, 0.5f)      // Specular (green-white)
    );
    
    // Light 3: Blue light
    pointLights[2] = PointLight(
        glm::vec3(0.0f, 2.0f, -2.0f),    // Position
        1.0f, 0.09f, 0.032f,             // Attenuation
        glm::vec3(0.0f, 0.0f, 0.05f),    // Ambient (dark blue)
        glm::vec3(0.2f, 0.2f, 1.0f),     // Diffuse (bright blue)
        glm::vec3(0.5f, 0.5f, 1.0f)      // Specular (blue-white)
    );
    
    // Setup spot light (camera headlight)
    spotLight = SpotLight(
        glm::vec3(0.0f, 0.0f, 0.0f),     // Position (will be updated to camera pos)
        glm::vec3(0.0f, 0.0f, -1.0f),    // Direction (will be updated to camera front)
        12.5f, 17.5f,                    // Inner and outer cone angles
        1.0f, 0.09f, 0.032f,             // Attenuation
        glm::vec3(0.0f, 0.0f, 0.0f),     // Ambient (no ambient for spotlight)
        glm::vec3(1.0f, 1.0f, 1.0f),     // Diffuse (white)
        glm::vec3(1.0f, 1.0f, 1.0f)      // Specular (white)
    );
}

void LightingSystem::updateLights(float time) {
    // Animate directional light (rotating sun)
    float sunRotation = time * 0.2f; // Slow rotation
    dirLight.direction = glm::vec3(
        sin(sunRotation) * 0.5f,
        -1.0f,
        cos(sunRotation) * 0.5f - 0.3f
    );
    
    // Change sun color based on time (sunrise/sunset effect)
    float sunColorFactor = (sin(time * 0.1f) + 1.0f) * 0.5f; // 0 to 1
    dirLight.diffuse = glm::mix(
        glm::vec3(1.0f, 0.4f, 0.2f),  // Sunset orange
        glm::vec3(1.0f, 1.0f, 0.9f)   // Noon white
    , sunColorFactor);
    
    // Animate point lights
    // Red light: circular motion
    pointLights[0].position = glm::vec3(
        2.0f * cos(time * 0.8f),
        1.0f + 0.5f * sin(time * 1.2f),
        -1.0f + sin(time * 0.8f)
    );
    
    // Green light: up-down motion
    pointLights[1].position = glm::vec3(
        -2.0f,
        1.0f + sin(time * 1.5f),
        -1.0f
    );
    
    // Blue light: figure-8 motion
    pointLights[2].position = glm::vec3(
        sin(time * 0.6f) * 1.5f,
        2.0f + cos(time * 1.2f) * 0.5f,
        -2.0f + cos(time * 0.6f) * 0.8f
    );
    
    // Animate point light intensities
    float intensity1 = (sin(time * 2.0f) + 1.0f) * 0.5f + 0.3f;  // 0.3 to 1.3
    float intensity2 = (cos(time * 1.5f) + 1.0f) * 0.5f + 0.3f;  // 0.3 to 1.3
    float intensity3 = (sin(time * 2.5f + 1.0f) + 1.0f) * 0.5f + 0.3f;  // 0.3 to 1.3
    
    pointLights[0].diffuse = glm::vec3(1.0f, 0.2f, 0.2f) * intensity1;
    pointLights[1].diffuse = glm::vec3(0.2f, 1.0f, 0.2f) * intensity2;
    pointLights[2].diffuse = glm::vec3(0.2f, 0.2f, 1.0f) * intensity3;
}

void LightingSystem::setupLightUniforms(ShaderProgram& shader, const glm::vec3& viewPos) {
    shader.activate();
    
    // Set view position
    shader.setUniform("viewPos", viewPos);
    
    // Setup all light types with error checking
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
}

void LightingSystem::setupDirectionalLight(ShaderProgram& shader) {
    shader.setUniform("dirLight.direction", dirLight.direction);
    shader.setUniform("dirLight.ambient", dirLight.ambient);
    shader.setUniform("dirLight.diffuse", dirLight.diffuse);
    shader.setUniform("dirLight.specular", dirLight.specular);
}

void LightingSystem::setupPointLights(ShaderProgram& shader) {
    // Pre-build uniform names to avoid string concatenation in loop
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
