#include "Projectile.hpp"
#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Projectile::Projectile(glm::vec3 pos, glm::vec3 vel, float r) 
    : position(pos), velocity(vel), radius(r), life(5.0f), alive(true), VAO(0), VBO(0), EBO(0), indexCount(0) {
    createSphere();
}

Projectile::~Projectile() {
    if (VAO != 0) glDeleteVertexArrays(1, &VAO);
    if (VBO != 0) glDeleteBuffers(1, &VBO);
    if (EBO != 0) glDeleteBuffers(1, &EBO);
}

void Projectile::createSphere(int segments) {
    std::vector<float> vertices;
    indices.clear();
    
    // Generate sphere vertices
    for (int lat = 0; lat <= segments; ++lat) {
        float theta = lat * M_PI / segments;
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);
        
        for (int lon = 0; lon <= segments; ++lon) {
            float phi = lon * 2 * M_PI / segments;
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);
            
            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;
            
            // Position
            vertices.push_back(x * radius);
            vertices.push_back(y * radius);
            vertices.push_back(z * radius);
            
            // Normal (same as position for unit sphere)
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            
            // Texture coordinates
            vertices.push_back(1.0f - (float)lon / segments);
            vertices.push_back((float)lat / segments);
        }
    }
    
    // Generate indices
    for (int lat = 0; lat < segments; ++lat) {
        for (int lon = 0; lon < segments; ++lon) {
            int first = lat * (segments + 1) + lon;
            int second = first + segments + 1;
            
            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);
            
            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }
    
    indexCount = indices.size();
    
    // Create OpenGL objects
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    
    glBindVertexArray(VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
    
    // Position attribute (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Normal attribute (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Texture coordinate attribute (location 2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
}

void Projectile::update(float deltaTime) {
    if (!alive) return;
    
    // Apply gravity
    velocity.y -= 9.81f * deltaTime;
    
    // Update position
    position += velocity * deltaTime;
    
    // Update life
    life -= deltaTime;
    if (life <= 0.0f) {
        alive = false;
    }
    
    // Check if hit ground (simple collision)
    if (position.y <= 0.0f) {
        alive = false;
    }
}

void Projectile::draw(ShaderProgram& shader, const glm::mat4& view, const glm::mat4& projection) {
    if (!alive) return;
    
    shader.activate();
    
    // Create model matrix
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    
    // Set uniforms
    shader.setUniform("uM_m", model);
    shader.setUniform("uV_m", view);
    shader.setUniform("uP_m", projection);
    
    // Calculate and set normal matrix
    glm::mat3 normal_matrix = glm::mat3(glm::transpose(glm::inverse(model)));
    shader.setUniform("uNormal_m", normal_matrix);
    
    // Set material properties for a pinkish cupcake color
    shader.setUniform("material_ambient", glm::vec3(0.2f, 0.1f, 0.1f));
    shader.setUniform("material_diffuse", glm::vec3(0.8f, 0.4f, 0.5f)); // Pinkish color
    shader.setUniform("material_specular", glm::vec3(0.3f, 0.3f, 0.3f));
    shader.setUniform("material_shininess", 32.0f);
    shader.setUniform("material_hasTexture", false);
    
    // Draw the sphere
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    shader.deactivate();
}
