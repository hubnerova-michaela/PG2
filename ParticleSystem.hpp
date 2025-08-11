#pragma once

#include <vector>
#include <random>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>
#include "ShaderProgram.hpp"
#include "TextureLoader.hpp"
#include "assets.hpp"

// Particle types for different effects
enum class ParticleType {
    GLOW,    // Original green glow particles
    SMOKE    // Smoke particles with texture
};

// Simple particle structure for our system
struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec4 color;
    float life;        // remaining life time
    float lifetime;    // total lifetime
    float size;
    ParticleType type; // Type of particle
    
    Particle() : position(0.0f), velocity(0.0f), color(1.0f), life(0.0f), lifetime(0.0f), size(1.0f), type(ParticleType::GLOW) {}
};

class ParticleSystem {
private:
    std::vector<Particle> particles;
    GLuint VAO, VBO;
    ShaderProgram& shader;
    std::mt19937 generator;
    std::uniform_real_distribution<float> dis;
    
    size_t maxParticles;
    glm::vec3 emitterPosition;
    float emissionRate;
    float lastEmissionTime;
    
    // Texture support
    GLuint smokeTexture;
    bool smokeTextureLoaded;
    
    // Current particle type for new emissions
    ParticleType currentParticleType;
    
    // Physics constants
    const float GRAVITY = -9.8f;
    const float BOUNCE_DAMPING = 0.7f;
    const float GROUND_LEVEL = 0.0f;
    
public:
    ParticleSystem(ShaderProgram& shaderProgram, size_t maxParticles = 1000);
    ~ParticleSystem();
    
    void setEmitterPosition(const glm::vec3& position);
    void setParticleType(ParticleType type); // New method to set particle type
    void update(float deltaTime);
    void draw(const glm::mat4& view, const glm::mat4& projection);
    void emit(int count = 1);
    void emitSmoke(int count = 1); // New method specifically for smoke
    void reset();
    
    // Collision detection methods
    bool checkCollisionWithBox(const glm::vec3& boxMin, const glm::vec3& boxMax);
    bool checkCollisionWithSphere(const glm::vec3& center, float radius);
    
private:
    void setupBuffers();
    void updateBuffers();
    Particle createParticle();
    Particle createSmokeParticle(); // New method for creating smoke particles
    void respawnParticle(Particle& particle);
    void loadSmokeTexture(); // New method to load smoke texture
};
