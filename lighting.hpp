#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Material
{
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float shininess;
    glm::vec3 emission;

    Material()
        : ambient(0.2f, 0.2f, 0.2f), diffuse(0.8f, 0.8f, 0.8f), specular(1.0f, 1.0f, 1.0f), shininess(32.0f), emission(0.0f, 0.0f, 0.0f) {}

    Material(glm::vec3 amb, glm::vec3 diff, glm::vec3 spec, float shin)
        : ambient(amb), diffuse(diff), specular(spec), shininess(shin), emission(0.0f, 0.0f, 0.0f) {}
};

struct DirectionalLight
{
    glm::vec3 direction;

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    DirectionalLight()
        : direction(-0.2f, -1.0f, -0.3f), ambient(0.05f, 0.05f, 0.05f), diffuse(0.4f, 0.4f, 0.4f), specular(0.5f, 0.5f, 0.5f) {}

    DirectionalLight(glm::vec3 dir, glm::vec3 amb, glm::vec3 diff, glm::vec3 spec)
        : direction(dir), ambient(amb), diffuse(diff), specular(spec) {}
};

struct PointLight
{
    glm::vec3 position;

    float constant;
    float linear;
    float quadratic;

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    PointLight()
        : position(0.0f, 0.0f, 0.0f), constant(1.0f), linear(0.09f), quadratic(0.032f), ambient(0.05f, 0.05f, 0.05f), diffuse(0.8f, 0.8f, 0.8f), specular(1.0f, 1.0f, 1.0f) {}

    PointLight(glm::vec3 pos, float c, float l, float q, glm::vec3 amb, glm::vec3 diff, glm::vec3 spec)
        : position(pos), constant(c), linear(l), quadratic(q), ambient(amb), diffuse(diff), specular(spec) {}
};

struct SpotLight
{
    glm::vec3 position;
    glm::vec3 direction;
    float cutOff;
    float outerCutOff;

    float constant;
    float linear;
    float quadratic;

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    SpotLight()
        : position(0.0f, 0.0f, 0.0f), direction(0.0f, 0.0f, -1.0f), cutOff(glm::cos(glm::radians(12.5f))), outerCutOff(glm::cos(glm::radians(15.0f))), constant(1.0f), linear(0.09f), quadratic(0.032f), ambient(0.0f, 0.0f, 0.0f), diffuse(1.0f, 1.0f, 1.0f), specular(1.0f, 1.0f, 1.0f) {}

    SpotLight(glm::vec3 pos, glm::vec3 dir, float inner, float outer,
              float c, float l, float q, glm::vec3 amb, glm::vec3 diff, glm::vec3 spec)
        : position(pos), direction(dir), cutOff(glm::cos(glm::radians(inner))), outerCutOff(glm::cos(glm::radians(outer))), constant(c), linear(l), quadratic(q), ambient(amb), diffuse(diff), specular(spec) {}
};

class LightingSystem
{
public:
    Material material;
    DirectionalLight dirLight;
    PointLight pointLights[3];
    SpotLight spotLight;

    LightingSystem();
    void setupDefaultLights();
    void updateLights(float time);
    void updateBikeLights(const glm::vec3 &cameraPos, const glm::vec3 &cameraRight);
    void setupLightUniforms(class ShaderProgram &shader, const glm::vec3 &viewPos);

private:
    void setupDirectionalLight(ShaderProgram &shader);
    void setupPointLights(ShaderProgram &shader);
    void setupSpotLight(ShaderProgram &shader);
    void setupMaterial(ShaderProgram &shader);
};
