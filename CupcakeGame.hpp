#pragma once

#include <vector>
#include <random>
#include <memory>
#include <glm/glm.hpp>
#include "HouseGenerator.hpp"
#include "Projectile.hpp"

// Forward declarations
class AudioEngine;
class ParticleSystem;
class PhysicsSystem;
class Camera;

struct GameState {
    // Quake epicenter and proximity-based intensity
    glm::vec3 quakeEpicenter{0.0f, 0.0f, -50.0f};
    // Score
    int money{0};
    int happiness{50}; // start neutral, clamp [0,100]

    // Movement
    float baseSpeed{6.0f};
    float speed{6.0f};
    float maxSpeed{14.0f};

    // Requests
    float requestInterval{4.0f};
    float minInterval{2.0f};
    float maxInterval{6.0f};

    // Projectiles using new Projectile class
    std::vector<std::unique_ptr<Projectile>> projectiles;
    float requestTimer{0.0f};
    float requestTimeout{5.0f};
    float requestTimeLeft{0.0f};
    int requestingHouseId{-1};

    // Earthquake
    bool quakeActive{false};
    float quakeCooldown{12.0f};
    float quakeTimer{0.0f};
    float quakeDuration{2.5f};
    float quakeTimeLeft{0.0f};
    float quakeAmplitude{0.12f}; // increased base amplitude

    // Pacing
    float pacingTimer{0.0f};
    float pacingStep{10.0f};

    // RNG
    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> unirand{0.0f,1.0f};

    // Systems data
    std::vector<glm::vec3> roadSegments; // centers along -Z
    float roadSegmentLength{20.0f};
    float roadSegmentWidth{8.0f}; // Width of each road segment
    int roadSegmentCount{8}; // Increased for better coverage

    std::vector<House> houses;
    float houseSpacing{15.0f}; // Increased spacing for better gameplay
    float houseOffsetX{8.0f}; // Increased offset for wider road
    int nextHouseId{1};

    bool active{true};
};

class CupcakeGame {
public:
    CupcakeGame();
    ~CupcakeGame();

    void initialize();
    void update(float deltaTime, Camera* camera, AudioEngine* audioEngine, 
               ParticleSystem* particleSystem, PhysicsSystem* physicsSystem);
    void handleMouseClick(Camera* camera);
    
    GameState& getGameState() { return gameState; }
    const GameState& getGameState() const { return gameState; }

private:
    GameState gameState;
    std::unique_ptr<HouseGenerator> houseGenerator;
    
    // Audio state
    unsigned int quakeSoundHandle = 0;
    bool quakeSoundPlaying = false;
    
    void updateMovement(float deltaTime, Camera* camera, PhysicsSystem* physicsSystem);
    void updateRequests(float deltaTime, Camera* camera);
    void updateEarthquake(float deltaTime, Camera* camera, AudioEngine* audioEngine);
    void updateProjectiles(float deltaTime);
    void updateHouses(Camera* camera, PhysicsSystem* physicsSystem);
    void spawnHouses(Camera* camera, PhysicsSystem* physicsSystem);
};
