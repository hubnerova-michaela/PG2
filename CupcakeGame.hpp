#pragma once

#include <vector>
#include <random>
#include <glm/glm.hpp>

// Forward declarations
class AudioEngine;
class ParticleSystem;
class PhysicsSystem;
class Camera;

struct House {
    int id;
    glm::vec3 position;
    glm::vec3 halfExtents; // AABB half sizes
    bool requesting{false};
    float indicatorHeight{2.5f};
};

struct Projectile {
    glm::vec3 pos;
    glm::vec3 vel;
    float radius{0.2f};
    float life{0.0f};
    float maxLife{5.0f};
    bool alive{false};
};

enum class DeliveryOutcome { None, Success, Wrong, Missed };

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
    int roadSegmentCount{8};

    std::vector<House> houses;
    float houseSpacing{12.0f};
    float houseOffsetX{6.0f};
    int nextHouseId{1};

    std::vector<Projectile> projectiles;

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
