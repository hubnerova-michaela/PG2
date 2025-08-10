#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "ShaderProgram.hpp"

class Projectile {
public:
    glm::vec3 position;
    glm::vec3 velocity;
    float radius;
    float life;
    bool alive;
    
    // Rendering data
    GLuint VAO, VBO, EBO;
    std::vector<GLuint> indices;
    int indexCount;

    Projectile(glm::vec3 pos, glm::vec3 vel, float r = 0.1f);
    ~Projectile();
    
    void update(float deltaTime);
    void draw(ShaderProgram& shader, const glm::mat4& view, const glm::mat4& projection);
    
private:
    void createSphere(int segments = 16);
};
