#pragma once

#include <random>

// Forward declarations to avoid including full headers here
struct GameState;
class Camera;

class HouseGenerator {
public:
    HouseGenerator();
    void updateRequests(float deltaTime, GameState& gameState, const Camera* camera);

private:
    std::mt19937 rng;
    std::uniform_real_distribution<float> uniformDist;
    float requestTimer; // Timer for controlling request frequency
};