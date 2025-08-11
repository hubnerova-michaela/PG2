#pragma once

#include <vector>
#include <string>
#include <memory>
#include <random>
#include <glm/glm.hpp>
#include "HouseGenerator.hpp"
#include "Projectile.hpp" // Make sure you have a header for your Projectile class

// Forward declarations
class Camera;
class AudioEngine;
class ParticleSystem;
class PhysicsSystem;

// Forward declare sol::state to avoid including Sol2 in header
namespace sol {
    class state;
}

// Defines a single house in the game world
struct House {
    int id = -1;
    glm::vec3 position{0.0f};
    std::string modelName;
    glm::vec3 halfExtents{1.0f};
    float indicatorHeight = 7.0f;
    bool requesting = false;
    bool delivered = false;
    float deliveryEffectTimer = 0.0f;
};

// Holds all the dynamic data for the game
struct GameState {
    bool active = true;
    int money = 100;
    int happiness = 100;
    float speed = 8.0f;
    float maxSpeed = 30.0f;
    int nextHouseId = 0;
    int requestingHouseId = -1;
    float requestTimeout = 10.0f;
    float requestTimeLeft = 0.0f;
    float requestInterval = 5.0f;
    float minInterval = 2.0f;
    float pacingTimer = 0.0f;
    float pacingStep = 10.0f;

    std::vector<glm::vec3> roadSegments;
    int roadSegmentCount = 30;
    float roadSegmentLength = 10.0f;
    float roadSegmentWidth = 10.0f;
    float houseOffsetX = 12.0f;
    float houseSpacing = 18.0f;
    
    std::vector<House> houses;
    std::vector<std::unique_ptr<Projectile>> projectiles;

    // Earthquake event data
    bool quakeActive = false;
    float quakeTimer = 0.0f;
    float quakeCooldown = 30.0f;
    float quakeTimeLeft = 0.0f;
    float quakeDuration = 12.0f;
    float quakeAmplitude = 0.05f;
    glm::vec3 quakeEpicenter{0.0f};
    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> unirand{0.0f, 1.0f};
};

class CupcakeGame {
public:
    CupcakeGame();
    ~CupcakeGame();

    void initialize();
    void update(float deltaTime, Camera* camera, AudioEngine* audioEngine, ParticleSystem* particleSystem, PhysicsSystem* physicsSystem);
    void handleMouseClick(Camera* camera);
    GameState& getGameState() { return gameState; }
    glm::vec3 getHouseExtents(const std::string& modelName);
    float getIndicatorHeight(const std::string& modelName);

private:
    // Main update sub-routines
    void updateMovement(float deltaTime, Camera* camera, PhysicsSystem* physicsSystem);
    void updateProjectiles(float deltaTime);
    void updateEarthquake(float deltaTime, Camera* camera, AudioEngine* audioEngine, ParticleSystem* particleSystem = nullptr);
    void updateHouses(Camera* camera, PhysicsSystem* physicsSystem);
    
    // House generation methods, now part of this class
    void spawnNewHouses(float zPosition, PhysicsSystem* physicsSystem);
    std::string getRandomHouseModel();

    GameState gameState;
    std::unique_ptr<HouseGenerator> houseGenerator;

    // RNG and properties for house generation
    std::mt19937 rng;
    std::uniform_real_distribution<float> unirand;
    std::vector<std::string> houseModels;
    float emptyPlotProbability = 0.2f;

    // Sound handles
    unsigned int quakeSoundHandle = 0;
    bool quakeSoundPlaying = false;
    
    PhysicsSystem* cachedPhysicsSystem;
};