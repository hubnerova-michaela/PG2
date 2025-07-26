#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Material properties for Phong lighting
struct Material {
    glm::vec3 ambient;   // Ambient color
    glm::vec3 diffuse;   // Diffuse color
    glm::vec3 specular;  // Specular color
    float shininess;     // Shininess exponent
    
    // Default constructor with reasonable values
    Material() 
        : ambient(0.2f, 0.2f, 0.2f)
        , diffuse(0.8f, 0.8f, 0.8f)
        , specular(1.0f, 1.0f, 1.0f)
        , shininess(32.0f) {}
        
    Material(glm::vec3 amb, glm::vec3 diff, glm::vec3 spec, float shin)
        : ambient(amb), diffuse(diff), specular(spec), shininess(shin) {}
};

// Directional light (like the sun)
struct DirectionalLight {
    glm::vec3 direction;  // Direction of the light
    
    glm::vec3 ambient;    // Ambient intensity
    glm::vec3 diffuse;    // Diffuse intensity
    glm::vec3 specular;   // Specular intensity
    
    // Default constructor - sun-like light
    DirectionalLight()
        : direction(-0.2f, -1.0f, -0.3f)
        , ambient(0.05f, 0.05f, 0.05f)
        , diffuse(0.4f, 0.4f, 0.4f)
        , specular(0.5f, 0.5f, 0.5f) {}
        
    DirectionalLight(glm::vec3 dir, glm::vec3 amb, glm::vec3 diff, glm::vec3 spec)
        : direction(dir), ambient(amb), diffuse(diff), specular(spec) {}
};

// Point light
struct PointLight {
    glm::vec3 position;   // Position of the light
    
    // Attenuation parameters
    float constant;       // Constant attenuation
    float linear;         // Linear attenuation
    float quadratic;      // Quadratic attenuation
    
    glm::vec3 ambient;    // Ambient intensity
    glm::vec3 diffuse;    // Diffuse intensity
    glm::vec3 specular;   // Specular intensity
    
    // Default constructor
    PointLight()
        : position(0.0f, 0.0f, 0.0f)
        , constant(1.0f)
        , linear(0.09f)
        , quadratic(0.032f)
        , ambient(0.05f, 0.05f, 0.05f)
        , diffuse(0.8f, 0.8f, 0.8f)
        , specular(1.0f, 1.0f, 1.0f) {}
        
    PointLight(glm::vec3 pos, float c, float l, float q, glm::vec3 amb, glm::vec3 diff, glm::vec3 spec)
        : position(pos), constant(c), linear(l), quadratic(q)
        , ambient(amb), diffuse(diff), specular(spec) {}
};

// Spot light (like a flashlight)
struct SpotLight {
    glm::vec3 position;   // Position of the light
    glm::vec3 direction;  // Direction of the light cone
    float cutOff;         // Inner cone angle (cosine)
    float outerCutOff;    // Outer cone angle (cosine)
    
    // Attenuation parameters
    float constant;       // Constant attenuation
    float linear;         // Linear attenuation
    float quadratic;      // Quadratic attenuation
    
    glm::vec3 ambient;    // Ambient intensity
    glm::vec3 diffuse;    // Diffuse intensity
    glm::vec3 specular;   // Specular intensity
    
    // Default constructor - flashlight-like
    SpotLight()
        : position(0.0f, 0.0f, 0.0f)
        , direction(0.0f, 0.0f, -1.0f)
        , cutOff(glm::cos(glm::radians(12.5f)))
        , outerCutOff(glm::cos(glm::radians(15.0f)))
        , constant(1.0f)
        , linear(0.09f)
        , quadratic(0.032f)
        , ambient(0.0f, 0.0f, 0.0f)
        , diffuse(1.0f, 1.0f, 1.0f)
        , specular(1.0f, 1.0f, 1.0f) {}
        
    SpotLight(glm::vec3 pos, glm::vec3 dir, float inner, float outer,
              float c, float l, float q, glm::vec3 amb, glm::vec3 diff, glm::vec3 spec)
        : position(pos), direction(dir)
        , cutOff(glm::cos(glm::radians(inner)))
        , outerCutOff(glm::cos(glm::radians(outer)))
        , constant(c), linear(l), quadratic(q)
        , ambient(amb), diffuse(diff), specular(spec) {}
};

// Lighting system class to manage all lights
class LightingSystem {
public:
    Material material;
    DirectionalLight dirLight;
    PointLight pointLights[3];
    SpotLight spotLight;
    
    LightingSystem();
    void setupDefaultLights();
    void updateLights(float time);
    void setupLightUniforms(class ShaderProgram& shader, const glm::vec3& viewPos);
    
private:
    void setupDirectionalLight(ShaderProgram& shader);
    void setupPointLights(ShaderProgram& shader);
    void setupSpotLight(ShaderProgram& shader);
    void setupMaterial(ShaderProgram& shader);
};
