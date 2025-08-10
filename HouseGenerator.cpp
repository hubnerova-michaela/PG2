#include "HouseGenerator.hpp"
#include <algorithm>
#include <iostream>

HouseGenerator::HouseGenerator() 
    : rng(std::random_device{}()), uniformDist(0.0f, 1.0f) {
    
    // Define available house models - using all 4 house types
    houseModels = {
        "house",
        "bambo_house",
        "cyprys_house", 
        "building"
    };
    
    // Define different scales for variety
    houseScales = {
        glm::vec3(0.8f, 0.8f, 0.8f),  // Small
        glm::vec3(1.0f, 1.0f, 1.0f),  // Normal
        glm::vec3(1.2f, 1.0f, 1.2f),  // Wide
        glm::vec3(1.0f, 1.3f, 1.0f)   // Tall
    };
    
    // Define corresponding extents for collision detection
    houseExtents = {
        glm::vec3(0.8f, 1.6f, 0.8f),  // Small house extents
        glm::vec3(1.0f, 2.0f, 1.0f),  // Normal house extents
        glm::vec3(1.4f, 2.0f, 1.4f),  // Wide house extents
        glm::vec3(1.0f, 2.6f, 1.0f)   // Tall house extents
    };
}

House HouseGenerator::generateHouse(int id, const glm::vec3& position, bool canBeRequesting) {
    House house;
    house.id = id;
    house.position = position;
    house.modelName = getRandomHouseModel();
    house.halfExtents = getHouseExtents(house.modelName);
    house.requesting = false;
    
    // Randomly decide if this house should be requesting (if allowed)
    if (canBeRequesting && uniformDist(rng) < activeProbability) {
        house.requesting = true;
    }
    
    return house;
}

std::vector<House> HouseGenerator::generateHouseRow(float zPosition, int& nextHouseId, 
                                                   float houseOffsetX, bool canMakeActive) {
    std::vector<House> houses;
    bool madeActive = false;
    
    // Generate left side house
    if (uniformDist(rng) > emptyProbability) {
        glm::vec3 leftPos(-houseOffsetX, 1.0f, zPosition);
        bool canBeActive = canMakeActive && !madeActive;
        House leftHouse = generateHouse(nextHouseId++, leftPos, canBeActive);
        
        if (leftHouse.requesting) {
            madeActive = true;
        }
        
        houses.push_back(leftHouse);
    }
    
    // Generate right side house
    if (uniformDist(rng) > emptyProbability) {
        glm::vec3 rightPos(houseOffsetX, 1.0f, zPosition);
        bool canBeActive = canMakeActive && !madeActive;
        House rightHouse = generateHouse(nextHouseId++, rightPos, canBeActive);
        
        houses.push_back(rightHouse);
    }
    
    return houses;
}

std::string HouseGenerator::getRandomHouseModel() {
    if (houseModels.empty()) {
        return "house"; // fallback
    }
    
    std::uniform_int_distribution<size_t> modelDist(0, houseModels.size() - 1);
    return houseModels[modelDist(rng)];
}

glm::vec3 HouseGenerator::getHouseExtents(const std::string& modelName) {
    // Return appropriate extents based on model type
    if (modelName == "bambo_house") {
        return glm::vec3(1.2f, 2.2f, 1.2f); // Slightly larger
    } else if (modelName == "cyprys_house") {
        return glm::vec3(1.1f, 2.5f, 1.1f); // Taller
    } else if (modelName == "building") {
        return glm::vec3(1.5f, 3.0f, 1.5f); // Much larger
    } else { // "house" or default
        return glm::vec3(1.0f, 2.0f, 1.0f); // Standard size
    }
}

glm::vec3 HouseGenerator::getHouseScale(const std::string& modelName) {
    // Return appropriate scale based on model type
    if (modelName == "bambo_house") {
        return glm::vec3(1.1f, 1.1f, 1.1f);
    } else if (modelName == "cyprys_house") {
        return glm::vec3(1.0f, 1.2f, 1.0f);
    } else if (modelName == "building") {
        return glm::vec3(1.3f, 1.4f, 1.3f);
    } else { // "house" or default
        return glm::vec3(1.0f, 1.0f, 1.0f);
    }
}
