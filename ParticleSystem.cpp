#include "ParticleSystem.hpp"
#include <iostream>
#include <algorithm>

ParticleSystem::ParticleSystem(ShaderProgram& shaderProgram, size_t maxParticles)
    : shader(shaderProgram), maxParticles(maxParticles), emitterPosition(0.0f, 10.0f, 0.0f), 
      emissionRate(50.0f), lastEmissionTime(0.0f), generator(std::random_device{}()), dis(0.0f, 1.0f) {
    
    particles.resize(maxParticles);
    setupBuffers();
    
    // Initialize all particles as inactive
    for (auto& particle : particles) {
        particle.life = 0.0f;
    }
}

ParticleSystem::~ParticleSystem() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void ParticleSystem::setupBuffers() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    
    // Create buffer for particle positions (will be updated dynamically)
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, maxParticles * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
}

void ParticleSystem::setEmitterPosition(const glm::vec3& position) {
    emitterPosition = position;
}

void ParticleSystem::update(float deltaTime) {
    // Update existing particles
    for (auto& particle : particles) {
        if (particle.life > 0.0f) {
            // Update physics
            particle.life -= deltaTime;
            particle.position += particle.velocity * deltaTime;
            
            // Apply gravity
            particle.velocity.y += GRAVITY * deltaTime;
            
            // Ground collision
            if (particle.position.y <= GROUND_LEVEL) {
                particle.position.y = GROUND_LEVEL;
                particle.velocity.y = -particle.velocity.y * BOUNCE_DAMPING;
                
                // Add some friction
                particle.velocity.x *= 0.9f;
                particle.velocity.z *= 0.9f;
            }
            
            // Update color based on life (fade out)
            float lifeRatio = particle.life / particle.lifetime;
            particle.color.a = lifeRatio;
            
            // Size based on life
            particle.size = 1.0f + (1.0f - lifeRatio) * 2.0f;
        }
    }
    
    // Emit new particles
    lastEmissionTime += deltaTime;
    if (lastEmissionTime >= 1.0f / emissionRate) {
        emit(1);
        lastEmissionTime = 0.0f;
    }
    
    updateBuffers();
}

void ParticleSystem::draw(const glm::mat4& view, const glm::mat4& projection) {
    shader.activate();
    
    // Set uniforms
    shader.setUniform("uV_m", view);
    shader.setUniform("uProj_m", projection);
    
    // Enable point size variation in vertex shader
    glEnable(GL_PROGRAM_POINT_SIZE);
    
    glBindVertexArray(VAO);
    
    // Count active particles
    int activeParticles = 0;
    for (const auto& particle : particles) {
        if (particle.life > 0.0f) {
            activeParticles++;
        }
    }
    
    if (activeParticles > 0) {
        glDrawArrays(GL_POINTS, 0, activeParticles);
    }
    
    glDisable(GL_PROGRAM_POINT_SIZE);
    glBindVertexArray(0);
    shader.deactivate();
}

void ParticleSystem::updateBuffers() {
    std::vector<glm::vec3> positions;
    
    // Collect positions of active particles
    for (const auto& particle : particles) {
        if (particle.life > 0.0f) {
            positions.push_back(particle.position);
        }
    }
    
    if (!positions.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, positions.size() * sizeof(glm::vec3), positions.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void ParticleSystem::emit(int count) {
    for (int i = 0; i < count; ++i) {
        // Find inactive particle
        auto it = std::find_if(particles.begin(), particles.end(), 
                              [](const Particle& p) { return p.life <= 0.0f; });
        
        if (it != particles.end()) {
            respawnParticle(*it);
        }
    }
}

Particle ParticleSystem::createParticle() {
    Particle particle;
    
    particle.position = emitterPosition + glm::vec3(
        (dis(generator) - 0.5f) * 2.0f,
        (dis(generator) - 0.5f) * 1.0f,
        (dis(generator) - 0.5f) * 2.0f
    );
    
    // Random spherical velocity
    particle.velocity = glm::sphericalRand(5.0f + dis(generator) * 10.0f);
    
    particle.lifetime = 2.0f + dis(generator) * 3.0f;
    particle.life = particle.lifetime;
    
    // Random color with full alpha
    particle.color = glm::vec4(
        0.5f + dis(generator) * 0.5f,
        0.3f + dis(generator) * 0.4f,
        0.1f + dis(generator) * 0.3f,
        1.0f
    );
    
    particle.size = 1.0f;
    
    return particle;
}

void ParticleSystem::respawnParticle(Particle& particle) {
    particle = createParticle();
}

void ParticleSystem::reset() {
    for (auto& particle : particles) {
        particle.life = 0.0f;
    }
}

bool ParticleSystem::checkCollisionWithBox(const glm::vec3& boxMin, const glm::vec3& boxMax) {
    for (const auto& particle : particles) {
        if (particle.life > 0.0f) {
            if (particle.position.x >= boxMin.x && particle.position.x <= boxMax.x &&
                particle.position.y >= boxMin.y && particle.position.y <= boxMax.y &&
                particle.position.z >= boxMin.z && particle.position.z <= boxMax.z) {
                return true;
            }
        }
    }
    return false;
}

bool ParticleSystem::checkCollisionWithSphere(const glm::vec3& center, float radius) {
    for (const auto& particle : particles) {
        if (particle.life > 0.0f) {
            float distance = glm::length(particle.position - center);
            if (distance <= radius) {
                return true;
            }
        }
    }
    return false;
}
