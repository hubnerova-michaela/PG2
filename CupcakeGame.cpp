#include "CupcakeGame.hpp"
#include "camera.hpp"
#include "AudioEngine.hpp"
#include "ParticleSystem.hpp"
#include "PhysicsSystem.hpp"
#include <iostream>
#include <algorithm>

// --- Class Implementation ---

CupcakeGame::CupcakeGame() 
    : rng(std::random_device{}()), unirand(0.0f, 1.0f), emptyPlotProbability(0.2f) {
    this->houseGenerator = std::make_unique<HouseGenerator>();
    this->houseModels = {"house", "bambo_house", "cyprys_house", "building"};
}

CupcakeGame::~CupcakeGame() {}

void CupcakeGame::initialize() {
    this->gameState = GameState(); 

    gameState.roadSegments.clear();
    for (int i = 0; i < gameState.roadSegmentCount; ++i) {
        gameState.roadSegments.push_back(glm::vec3(0.0f, 0.0f, -static_cast<float>(i) * gameState.roadSegmentLength));
    }

    gameState.houses.clear();
    float z = -15.0f;
    for (int i = 0; i < 20; ++i) {
        spawnNewHouses(z, nullptr);
        z -= gameState.houseSpacing;
    }
    
    std::cout << "Initialized with " << gameState.houses.size() << " houses" << std::endl;
}

void CupcakeGame::update(float deltaTime, Camera* camera, AudioEngine* audioEngine, 
                       ParticleSystem* particleSystem, PhysicsSystem* physicsSystem) {
    if (!gameState.active) return;

    updateMovement(deltaTime, camera, physicsSystem);
    houseGenerator->updateRequests(deltaTime, this->gameState, camera);
    updateEarthquake(deltaTime, camera, audioEngine);
    updateProjectiles(deltaTime);
    updateHouses(camera, physicsSystem);
}

// All other member functions from your original file go here...
// (handleMouseClick, updateMovement, updateProjectiles, updateEarthquake, etc.)
// The following are the helper methods that have been moved here.

void CupcakeGame::updateHouses(Camera* camera, PhysicsSystem* physicsSystem) {
    if (!camera) return;

    for (auto& seg : gameState.roadSegments) {
        if (seg.z > camera->Position.z + gameState.roadSegmentLength) {
            float minZ = seg.z;
            for (auto& s2 : gameState.roadSegments) minZ = std::min(minZ, s2.z);
            seg.z = minZ - gameState.roadSegmentLength;
        }
    }

    gameState.houses.erase(
        std::remove_if(gameState.houses.begin(), gameState.houses.end(),
            [&](const House& h){ 
                if (h.position.z > camera->Position.z + 15.0f) {
                    if (physicsSystem) {
                        // This line was causing an error, see explanation below
                        physicsSystem->removeCollisionObject(h.position);
                    }
                    return true;
                }
                return false; 
            }),
        gameState.houses.end()
    );

    if (gameState.active) {
       // ... your collision logic
    }

    float farthestZ = camera->Position.z;
    for (const auto& h : gameState.houses) {
        farthestZ = std::min(farthestZ, h.position.z);
    }
    if (camera->Position.z - farthestZ < 200.0f) {
        spawnNewHouses(farthestZ - gameState.houseSpacing, physicsSystem);
    }
}

void CupcakeGame::spawnNewHouses(float zPosition, PhysicsSystem* physicsSystem) {
    if (this->unirand(this->rng) > this->emptyPlotProbability) {
        House house;
        house.id = gameState.nextHouseId++;
        house.position = glm::vec3(-gameState.houseOffsetX, 0.0f, zPosition);
        house.modelName = getRandomHouseModel();
        house.halfExtents = getHouseExtents(house.modelName);
        house.indicatorHeight = getIndicatorHeight(house.modelName);
        
        gameState.houses.push_back(house);
        if (physicsSystem) {
            physicsSystem->addCollisionObject({CollisionType::BOX, house.position, house.halfExtents});
        }
    }
    
    if (this->unirand(this->rng) > this->emptyPlotProbability) {
        House house;
        house.id = gameState.nextHouseId++;
        house.position = glm::vec3(gameState.houseOffsetX, 0.0f, zPosition);
        house.modelName = getRandomHouseModel();
        house.halfExtents = getHouseExtents(house.modelName);
        house.indicatorHeight = getIndicatorHeight(house.modelName);
        
        gameState.houses.push_back(house);
        if (physicsSystem) {
            physicsSystem->addCollisionObject({CollisionType::BOX, house.position, house.halfExtents});
        }
    }
}

std::string CupcakeGame::getRandomHouseModel() {
    if (this->houseModels.empty()) return "house";
    std::uniform_int_distribution<size_t> modelDist(0, this->houseModels.size() - 1);
    return this->houseModels[modelDist(this->rng)];
}

glm::vec3 CupcakeGame::getHouseExtents(const std::string& modelName) {
    if (modelName == "bambo_house") return glm::vec3(4.0f, 6.0f, 4.0f);
    if (modelName == "cyprys_house") return glm::vec3(4.5f, 7.0f, 4.5f);
    if (modelName == "building") return glm::vec3(6.0f, 9.0f, 6.0f);
    return glm::vec3(3.5f, 5.0f, 3.5f);
}

float CupcakeGame::getIndicatorHeight(const std::string& modelName) {
    if (modelName == "bambo_house") return 8.0f;
    if (modelName == "cyprys_house") return 9.0f;
    if (modelName == "building") return 12.0f;
    return 7.0f;
}

void CupcakeGame::handleMouseClick(Camera* camera) {
    if (!camera || !gameState.active) return;
    
    // Create a projectile in the direction the camera is facing
    glm::vec3 projectilePos = camera->Position + camera->Front * 2.0f;
    glm::vec3 projectileVel = camera->Front * 15.0f; // Adjust speed as needed
    
    auto projectile = std::make_unique<Projectile>(projectilePos, projectileVel, 0.2f);
    gameState.projectiles.push_back(std::move(projectile));
}

void CupcakeGame::updateProjectiles(float deltaTime) {
    // Update all projectiles
    for (auto& projectile : gameState.projectiles) {
        if (projectile && projectile->alive) {
            projectile->update(deltaTime);
        }
    }
    
    // Remove dead projectiles
    gameState.projectiles.erase(
        std::remove_if(gameState.projectiles.begin(), gameState.projectiles.end(),
            [](const std::unique_ptr<Projectile>& p) {
                return !p || !p->alive;
            }),
        gameState.projectiles.end()
    );
}

void CupcakeGame::updateMovement(float deltaTime, Camera* camera, PhysicsSystem* physicsSystem) {
    if (!camera || !gameState.active) return;
    
    // Simple forward movement for the game
    float moveSpeed = gameState.speed * deltaTime;
    glm::vec3 movement = glm::vec3(0.0f, 0.0f, -moveSpeed);
    
    if (physicsSystem) {
        // Use physics system for movement with collision detection
        glm::vec3 actualMovement = physicsSystem->moveCamera(*camera, movement);
        camera->Position += actualMovement;
    } else {
        // Simple movement without collision
        camera->Position += movement;
    }
}

void CupcakeGame::updateEarthquake(float deltaTime, Camera* camera, AudioEngine* audioEngine) {
    if (!camera) return;
    
    // Update earthquake cooldown
    if (gameState.quakeCooldown > 0.0f) {
        gameState.quakeCooldown -= deltaTime;
    }
    
    // Start earthquake if cooldown is finished and not already active
    if (!gameState.quakeActive && gameState.quakeCooldown <= 0.0f) {
        gameState.quakeActive = true;
        gameState.quakeTimeLeft = gameState.quakeDuration;
        gameState.quakeEpicenter = camera->Position + glm::vec3(
            (gameState.unirand(gameState.rng) - 0.5f) * 50.0f,
            0.0f,
            (gameState.unirand(gameState.rng) - 0.5f) * 50.0f
        );
        
        // Play earthquake sound
        if (audioEngine && !quakeSoundPlaying) {
            if (audioEngine->playLoop3D("resources/audio/052256_cracking-earthquake-cracking-soil-cracking-stone-86770.wav", gameState.quakeEpicenter, &quakeSoundHandle)) {
                quakeSoundPlaying = true;
            }
        }
    }
    
    // Update active earthquake
    if (gameState.quakeActive) {
        gameState.quakeTimeLeft -= deltaTime;
        
        if (gameState.quakeTimeLeft <= 0.0f) {
            // End earthquake
            gameState.quakeActive = false;
            gameState.quakeCooldown = 25.0f; // Reset cooldown
            
            // Stop earthquake sound
            if (audioEngine && quakeSoundPlaying) {
                audioEngine->stopSound(quakeSoundHandle);
                quakeSoundPlaying = false;
            }
        } else {
            // Apply earthquake effect to camera
            float distance = glm::length(camera->Position - gameState.quakeEpicenter);
            float intensity = std::max(0.0f, 1.0f - distance / 100.0f); // Falloff over distance
            
            float shakeX = (gameState.unirand(gameState.rng) - 0.5f) * gameState.quakeAmplitude * intensity;
            float shakeY = (gameState.unirand(gameState.rng) - 0.5f) * gameState.quakeAmplitude * intensity;
            float shakeZ = (gameState.unirand(gameState.rng) - 0.5f) * gameState.quakeAmplitude * intensity;
            
            camera->Position += glm::vec3(shakeX, shakeY, shakeZ);
        }
    }
}