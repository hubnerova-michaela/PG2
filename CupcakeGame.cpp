#include "CupcakeGame.hpp"
#include "camera.hpp"
#include "AudioEngine.hpp"
#include "ParticleSystem.hpp"
#include "PhysicsSystem.hpp"
#include <glm/glm.hpp>
#include <iostream>
#include <algorithm>

CupcakeGame::CupcakeGame() {
    houseGenerator = std::make_unique<HouseGenerator>();
}

CupcakeGame::~CupcakeGame() {
}

void CupcakeGame::initialize() {
    gameState.roadSegments.clear();
    for (int i = 0; i < gameState.roadSegmentCount; ++i) {
        gameState.roadSegments.push_back(glm::vec3(0.0f, 0.0f, -static_cast<float>(i) * gameState.roadSegmentLength));
    }

    gameState.houses.clear();
    float z = -15.0f;
    
    for (int i = 0; i < 20; ++i) {
        std::vector<House> newHouses = houseGenerator->generateHouseRow(
            z, gameState.nextHouseId, gameState.houseOffsetX, 
            gameState.requestingHouseId < 0
        );
        
        for (auto& house : newHouses) {
            if (house.requesting) {
                gameState.requestingHouseId = house.id;
                gameState.requestTimeLeft = gameState.requestTimeout;
            }
            gameState.houses.push_back(house);
        }
        
        z -= gameState.houseSpacing;
    }
    
    std::cout << "Initialized with " << gameState.houses.size() << " houses" << std::endl;
}

void CupcakeGame::handleMouseClick(Camera* camera) {
    if (!camera) return;
    
    glm::vec3 pos = camera->Position + camera->Front * 2.5f + glm::vec3(0.0f, -0.1f, 0.0f); // spawn 2.5 units ahead
    glm::vec3 forward = glm::normalize(glm::vec3(camera->Front.x, camera->Front.y + 0.1f, camera->Front.z)); // slight upward bias for arc
    glm::vec3 vel = forward * 16.0f; // increased throw speed
    
    auto projectile = std::make_unique<Projectile>(pos, vel, 0.2f);
    gameState.projectiles.push_back(std::move(projectile));
    
    std::cout << "Cupcake projectile launched from position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
}

void CupcakeGame::update(float deltaTime, Camera* camera, AudioEngine* audioEngine, 
                        ParticleSystem* particleSystem, PhysicsSystem* physicsSystem) {
    if (!gameState.active) return;

    updateMovement(deltaTime, camera, physicsSystem);
    updateRequests(deltaTime, camera);
    updateEarthquake(deltaTime, camera, audioEngine);
    updateProjectiles(deltaTime);
    updateHouses(camera, physicsSystem);
}

void CupcakeGame::updateMovement(float deltaTime, Camera* camera, PhysicsSystem* physicsSystem) {
    if (!camera || !gameState.active) return;

    glm::vec3 fwd = camera->Front;
    glm::vec3 fwdXZ = glm::normalize(glm::vec3(fwd.x, 0.0f, fwd.z));
    glm::vec3 desiredMovement = fwdXZ * gameState.speed * deltaTime;

    if (physicsSystem) {
        glm::vec3 actualMovement = physicsSystem->moveCamera(*camera, desiredMovement);
        camera->Position += actualMovement;
        
        if (camera->Position.y < 1.5f) {
            camera->Position.y = 1.5f;
        } else if (camera->Position.y > 4.0f) {
            camera->Position.y = 4.0f;
        }
        
        if (std::abs(camera->Position.x) > gameState.houseOffsetX - 2.0f) {
            float sign = camera->Position.x > 0.0f ? 1.0f : -1.0f;
            camera->Position.x = sign * (gameState.houseOffsetX - 2.0f);
        }
    }

    gameState.pacingTimer += deltaTime;
    if (gameState.pacingTimer >= gameState.pacingStep) {
        gameState.pacingTimer = 0.0f;
        gameState.speed = glm::min(gameState.speed + 0.3f, gameState.maxSpeed);
        gameState.requestInterval = glm::max(gameState.requestInterval - 0.15f, gameState.minInterval);
        std::cout << "Speed increased to: " << gameState.speed << ", Request interval: " << gameState.requestInterval << std::endl;
    }
}

void CupcakeGame::updateRequests(float deltaTime, Camera* camera) {
    if (!camera) return;

    if (gameState.requestingHouseId < 0) {
        gameState.requestTimer += deltaTime;
        if (gameState.requestTimer >= gameState.requestInterval && !gameState.houses.empty()) {
            int pick = -1;
            for (size_t i = 0; i < gameState.houses.size(); ++i) {
                const auto& h = gameState.houses[i];
                if (h.position.z < camera->Position.z - 5.0f && h.position.z > camera->Position.z - 80.0f) {
                    pick = static_cast<int>(i);
                    break;
                }
            }
            if (pick >= 0) {
                gameState.houses[pick].requesting = true;
                gameState.requestingHouseId = gameState.houses[pick].id;
                gameState.requestTimeLeft = gameState.requestTimeout;
                gameState.requestTimer = 0.0f;
                std::cout << "New delivery request at house " << gameState.requestingHouseId 
                         << " (pos: " << gameState.houses[pick].position.x << ", " 
                         << gameState.houses[pick].position.z << ")" << std::endl;
            }
        }
    } else {
        gameState.requestTimeLeft -= deltaTime;
        if (gameState.requestTimeLeft <= 0.0f) {
            // Missed delivery
            gameState.happiness = glm::max(0, gameState.happiness - 8);
            std::cout << "Missed delivery! Happiness: " << gameState.happiness << std::endl;
            
            for (auto& h : gameState.houses) {
                if (h.id == gameState.requestingHouseId) { 
                    h.requesting = false; 
                    break; 
                }
            }
            gameState.requestingHouseId = -1;
            gameState.requestTimer = 0.0f;
        }
    }
}

void CupcakeGame::updateEarthquake(float deltaTime, Camera* camera, AudioEngine* audioEngine) {
    if (!gameState.quakeActive) {
        gameState.quakeTimer += deltaTime;
        if (gameState.quakeTimer >= gameState.quakeCooldown) {
            gameState.quakeActive = true;
            gameState.quakeDuration = glm::max(gameState.quakeDuration, 8.0f);
            gameState.quakeTimeLeft = gameState.quakeDuration;
            gameState.quakeTimer = 0.0f;
            
            if (audioEngine && audioEngine->isInitialized() && !quakeSoundPlaying && camera) {
                glm::vec3 fwdXZ = glm::normalize(glm::vec3(camera->Front.x, 0.0f, camera->Front.z));
                float dist = 8.0f + 22.0f * gameState.unirand(gameState.rng); // 8..30m
                float side = (gameState.unirand(gameState.rng) < 0.5f ? -1.0f : 1.0f) * (2.0f + 6.0f * gameState.unirand(gameState.rng));
                glm::vec3 right = glm::normalize(glm::cross(fwdXZ, glm::vec3(0,1,0)));
                gameState.quakeEpicenter = camera->Position + fwdXZ * dist + right * side;
                gameState.quakeEpicenter.y = 0.0f;
                
                quakeSoundHandle = audioEngine->playLoop3D("resources/audio/052256_cracking-earthquake-cracking-soil-cracking-stone-86770.wav", gameState.quakeEpicenter);
                if (quakeSoundHandle != 0) {
                    quakeSoundPlaying = true;
                }
            }
        }
    } else {
        if (audioEngine && audioEngine->isInitialized() && quakeSoundPlaying && quakeSoundHandle != 0) {
            audioEngine->setSoundPosition(quakeSoundHandle, gameState.quakeEpicenter);
        }

        gameState.quakeTimeLeft -= deltaTime;
        if (gameState.quakeTimeLeft <= 0.0f) {
            gameState.quakeActive = false;
            gameState.quakeTimeLeft = 0.0f;
            if (audioEngine && quakeSoundPlaying && quakeSoundHandle != 0) {
                audioEngine->stopSound(quakeSoundHandle);
                quakeSoundHandle = 0;
                quakeSoundPlaying = false;
            }
        }
    }
}

void CupcakeGame::updateProjectiles(float deltaTime) {
    for (auto& p : gameState.projectiles) {
        if (!p || !p->alive) continue;
        
        p->update(deltaTime);

        for (auto& h : gameState.houses) {
            glm::vec3 minB = h.position - h.halfExtents;
            glm::vec3 maxB = h.position + h.halfExtents;
            glm::vec3 c = glm::clamp(p->position, minB, maxB);
            float d2 = glm::dot(p->position - c, p->position - c);
            if (d2 <= p->radius * p->radius) {
                if (h.requesting) {
                    gameState.money += 8;
                    gameState.happiness = glm::min(100, gameState.happiness + 5);
                    h.requesting = false;
                    gameState.requestingHouseId = -1;
                    gameState.requestTimer = 0.0f;
                    std::cout << "Successful delivery! Money: " << gameState.money 
                             << ", Happiness: " << gameState.happiness << std::endl;
                } else {
                    gameState.money = std::max(0, gameState.money - 3);
                    gameState.happiness = glm::max(0, gameState.happiness - 2);
                    std::cout << "Wrong house! Money: " << gameState.money 
                             << ", Happiness: " << gameState.happiness << std::endl;
                }
                p->alive = false;
                break;
            }
        }
    }
    
    gameState.projectiles.erase(
        std::remove_if(gameState.projectiles.begin(), gameState.projectiles.end(),
            [](const std::unique_ptr<Projectile>& pr){ return !pr || !pr->alive; }),
        gameState.projectiles.end()
    );
}

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
            [&](const House& h){ return h.position.z > camera->Position.z + 5.0f; }),
        gameState.houses.end()
    );

    if (gameState.active) {
        glm::vec3 camPos = camera->Position;
        float camRadius = 0.5f;
        for (const auto& h : gameState.houses) {
            glm::vec3 minB = h.position - (h.halfExtents + glm::vec3(camRadius));
            glm::vec3 maxB = h.position + (h.halfExtents + glm::vec3(camRadius));
            glm::vec3 c = glm::clamp(camPos, minB, maxB);
            float d2 = glm::dot(camPos - c, camPos - c);
            if (d2 <= camRadius * camRadius) {
                std::cout << "Game Over! Crashed into house at (" << h.position.x << ", " << h.position.y << ", " << h.position.z << ")" << std::endl;
                gameState.active = false;
                gameState.speed = 0.0f;
                break;
            }
        }
    }

    spawnHouses(camera, physicsSystem);
}

void CupcakeGame::spawnHouses(Camera* camera, PhysicsSystem* physicsSystem) {
    if (!camera) return;

    float farthestZ = camera->Position.z;
    for (const auto& h : gameState.houses) {
        farthestZ = std::min(farthestZ, h.position.z);
    }

    while (camera->Position.z - farthestZ < 200.0f) {
        farthestZ -= gameState.houseSpacing;
        
        std::vector<House> newHouses = houseGenerator->generateHouseRow(
            farthestZ, gameState.nextHouseId, gameState.houseOffsetX,
            gameState.requestingHouseId < 0
        );
        
        for (auto& house : newHouses) {
            if (house.requesting && gameState.requestingHouseId < 0) {
                gameState.requestingHouseId = house.id;
                gameState.requestTimeLeft = gameState.requestTimeout;
                std::cout << "Generated new requesting house at z=" << house.position.z << std::endl;
            }
            gameState.houses.push_back(house);
        }
        
        if (physicsSystem) {
            for (const auto& house : newHouses) {
                physicsSystem->addCollisionObject(CollisionObject(
                    CollisionType::BOX, house.position, house.halfExtents
                ));
            }
        }
    }
}
