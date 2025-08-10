#include "Projectile.hpp"
#include <iostream>

Projectile::Projectile(glm::vec3 pos, glm::vec3 vel, float r)
    : position(pos), velocity(vel), radius(r), life(5.0f), alive(true) {
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

void Projectile::draw(Model* cupcakeModel) {
    if (!alive || !cupcakeModel) return;
    
    // Create model matrix for this projectile
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, position);
    
    // Rotate cupcake to face upwards (rotate 90 degrees around X-axis)
    modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    
    // Make projectiles 50% smaller
    modelMatrix = glm::scale(modelMatrix, glm::vec3(radius * 1.0f)); // Use radius as base scale (50% smaller than default)
    
    // The cupcake model will handle its own drawing with the current shader
    cupcakeModel->draw(modelMatrix);
}
