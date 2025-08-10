#pragma once

#include <vector>
#include <random>
#include <string>
#include <glm/glm.hpp>

struct House {
    int id;
    glm::vec3 position;
    glm::vec3 halfExtents; // AABB half sizes
    bool requesting{false};
    float indicatorHeight{2.5f};
    std::string modelName; // Which house model to use
};

class HouseGenerator {
public:
    HouseGenerator();
    ~HouseGenerator() = default;

    // Generate a random house at the specified position
    House generateHouse(int id, const glm::vec3& position, bool canBeRequesting = false);
    
    // Generate houses for a row (left and right sides)
    std::vector<House> generateHouseRow(float zPosition, int& nextHouseId, 
                                      float houseOffsetX, bool canMakeActive = false);
    
    // Get all available house model names
    const std::vector<std::string>& getAvailableHouseModels() const { return houseModels; }
    
    // Configuration
    void setEmptyProbability(float prob) { emptyProbability = prob; }
    void setActiveProbability(float prob) { activeProbability = prob; }
    
private:
    std::mt19937 rng;
    std::uniform_real_distribution<float> uniformDist;
    
    // Available house models (expanded from the original 2 to 4)
    std::vector<std::string> houseModels;
    
    // Generation probabilities
    float emptyProbability{0.35f};    // 35% chance for empty spot
    float activeProbability{0.10f};   // 10% chance for active request house
    
    // House variations
    std::vector<glm::vec3> houseScales;
    std::vector<glm::vec3> houseExtents;
    
    std::string getRandomHouseModel();
    glm::vec3 getHouseExtents(const std::string& modelName);
    glm::vec3 getHouseScale(const std::string& modelName);
};
