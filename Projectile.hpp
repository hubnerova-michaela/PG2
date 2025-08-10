#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "ShaderProgram.hpp"
#include "Model.hpp"
#include <memory>

class Projectile {
public:
    glm::vec3 position;
    glm::vec3 velocity;
    float radius;
    float life;
    bool alive;

    Projectile(glm::vec3 pos, glm::vec3 vel, float r = 0.1f);
    ~Projectile() = default;
    
    void update(float deltaTime);
    void draw(Model* cupcakeModel);
};
