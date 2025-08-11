#include "ParticleSystem.hpp"
#include <iostream>
#include <algorithm>

ParticleSystem::ParticleSystem(ShaderProgram& shaderProgram, size_t maxParticles)
    : shader(shaderProgram), maxParticles(maxParticles), emitterPosition(0.0f, 10.0f, 0.0f), 
      emissionRate(50.0f), lastEmissionTime(0.0f), generator(std::random_device{}()), dis(0.0f, 1.0f),
      smokeTexture(0), smokeTextureLoaded(false), currentParticleType(ParticleType::GLOW) {
    
    particles.resize(maxParticles);
    setupBuffers();
    loadSmokeTexture();
    
    // Initialize all particles as inactive
    for (auto& particle : particles) {
        particle.life = 0.0f;
    }
}

ParticleSystem::~ParticleSystem() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    if (smokeTextureLoaded && smokeTexture != 0) {
        glDeleteTextures(1, &smokeTexture);
    }
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

void ParticleSystem::setParticleType(ParticleType type) {
    currentParticleType = type;
}

void ParticleSystem::loadSmokeTexture() {
    try {
        smokeTexture = TextureLoader::textureInit("resources/textures/smoke1.png");
        smokeTextureLoaded = true;
        std::cout << "Loaded smoke texture successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load smoke texture: " << e.what() << std::endl;
        smokeTextureLoaded = false;
        smokeTexture = 0;
    }
}

void ParticleSystem::update(float deltaTime) {
    // Update existing particles
    for (auto& particle : particles) {
        if (particle.life > 0.0f) {
            // Update physics
            particle.life -= deltaTime;
            particle.position += particle.velocity * deltaTime;
            
            // Apply different physics based on particle type
            if (particle.type == ParticleType::SMOKE) {
                // Smoke particles rise and drift with reduced gravity
                particle.velocity.y += -GRAVITY * 0.1f * deltaTime; // Much lighter than normal gravity
                
                // Add some horizontal drift for realistic smoke movement
                particle.velocity.x += (dis(generator) - 0.5f) * 0.5f * deltaTime;
                particle.velocity.z += (dis(generator) - 0.5f) * 0.5f * deltaTime;
                
                // Apply air resistance to slow down over time
                particle.velocity *= 0.98f;
            } else {
                // Normal gravity for glow particles
                particle.velocity.y += GRAVITY * deltaTime;
            }
            
            // Ground collision (only for glow particles)
            if (particle.type == ParticleType::GLOW && particle.position.y <= GROUND_LEVEL) {
                particle.position.y = GROUND_LEVEL;
                particle.velocity.y = -particle.velocity.y * BOUNCE_DAMPING;
                
                // Add some friction
                particle.velocity.x *= 0.9f;
                particle.velocity.z *= 0.9f;
            }
            
            // Update color based on life (fade out)
            float lifeRatio = particle.life / particle.lifetime;
            particle.color.a = lifeRatio;
            
            // Size based on life - smoke particles grow over time, glow particles shrink
            if (particle.type == ParticleType::SMOKE) {
                particle.size = 2.0f + (1.0f - lifeRatio) * 4.0f; // Smoke grows as it dissipates
            } else {
                particle.size = 1.0f + (1.0f - lifeRatio) * 2.0f;
            }
        }
    }
}

void ParticleSystem::draw(const glm::mat4& view, const glm::mat4& projection) {
    shader.activate();
    
    // Set uniforms
    shader.setUniform("uV_m", view);
    shader.setUniform("uProj_m", projection);
    
    // Enable point size variation in vertex shader
    glEnable(GL_PROGRAM_POINT_SIZE);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glBindVertexArray(VAO);
    
    // Count particles by type and prepare position data
    std::vector<glm::vec3> glowPositions;
    std::vector<glm::vec3> smokePositions;
    
    for (const auto& particle : particles) {
        if (particle.life > 0.0f) {
            if (particle.type == ParticleType::GLOW) {
                glowPositions.push_back(particle.position);
            } else if (particle.type == ParticleType::SMOKE) {
                smokePositions.push_back(particle.position);
            }
        }
    }
    
    // Draw glow particles first (no texture)
    if (!glowPositions.empty()) {
        shader.setUniform("useTexture", 0);
        shader.setUniform("uPointSize", 10.0f);
        shader.setUniform("particleColor", glm::vec4(0.15f, 0.95f, 0.2f, 0.6f)); // Green glow
        
        // Update buffer with glow positions
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, maxParticles * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW); // Re-allocate buffer
        glBufferSubData(GL_ARRAY_BUFFER, 0, glowPositions.size() * sizeof(glm::vec3), glowPositions.data());
        
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(glowPositions.size()));
    }
    
    // Draw smoke particles with texture
    if (!smokePositions.empty() && smokeTextureLoaded) {
        shader.setUniform("useTexture", 1);
        shader.setUniform("uPointSize", 200.0f); // Larger point size for smoke
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, smokeTexture);
        shader.setUniform("particleTexture", 0);
        shader.setUniform("particleColor", glm::vec4(0.6f, 0.6f, 0.6f, 0.7f)); // Gray smoke
        
        // Update buffer with smoke positions
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, maxParticles * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW); // Re-allocate buffer
        glBufferSubData(GL_ARRAY_BUFFER, 0, smokePositions.size() * sizeof(glm::vec3), smokePositions.data());
        
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(smokePositions.size()));
    }
    
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    shader.deactivate();
}

void ParticleSystem::updateBuffers() {
    // This function is no longer needed as buffer updates are handled in draw()
}

void ParticleSystem::emit(int count) {
    for (int i = 0; i < count; ++i) {
        // Find inactive particle
        auto it = std::find_if(particles.begin(), particles.end(), 
                              [](const Particle& p) { return p.life <= 0.0f; });
        
        if (it != particles.end()) {
            if (currentParticleType == ParticleType::SMOKE) {
                *it = createSmokeParticle();
            } else {
                *it = createParticle();
            }
        }
    }
}

void ParticleSystem::emitSmoke(int count) {
    ParticleType previousType = currentParticleType;
    setParticleType(ParticleType::SMOKE);
    emit(count);
    setParticleType(previousType);
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
    particle.type = ParticleType::GLOW;
    
    return particle;
}

Particle ParticleSystem::createSmokeParticle() {
    Particle particle;
    
    // Spawn around the emitter with some randomness
    particle.position = emitterPosition + glm::vec3(
        (dis(generator) - 0.5f) * 4.0f,  // Wider spread for smoke
        dis(generator) * 0.5f,            // Start near ground level
        (dis(generator) - 0.5f) * 4.0f
    );
    
    // Smoke particles rise with some random horizontal movement
    particle.velocity = glm::vec3(
        (dis(generator) - 0.5f) * 2.0f,  // Some horizontal drift
        2.0f + dis(generator) * 3.0f,    // Upward movement
        (dis(generator) - 0.5f) * 2.0f   // Some horizontal drift
    );
    
    particle.lifetime = 4.0f + dis(generator) * 3.0f; // Longer lifetime for smoke
    particle.life = particle.lifetime;
    
    // Grayish smoke color with transparency
    particle.color = glm::vec4(
        0.5f + dis(generator) * 0.3f,    // Gray tones
        0.5f + dis(generator) * 0.3f,
        0.5f + dis(generator) * 0.3f,
        0.8f - dis(generator) * 0.3f     // Semi-transparent
    );
    
    particle.size = 2.0f + dis(generator) * 2.0f;
    particle.type = ParticleType::SMOKE;
    
    return particle;
}

void ParticleSystem::respawnParticle(Particle& particle) {
    if (currentParticleType == ParticleType::SMOKE) {
        particle = createSmokeParticle();
    } else {
        particle = createParticle();
    }
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
