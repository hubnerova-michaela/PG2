#include "CupcakeGame.hpp"
#include "camera.hpp"
#include "AudioEngine.hpp"
#include "ParticleSystem.hpp"
#include "PhysicsSystem.hpp"
#include <glm/glm.hpp>
#include <iostream>
#include <algorithm>

CupcakeGame::CupcakeGame() {
    // Constructor - basic initialization done via member initializers
}

CupcakeGame::~CupcakeGame() {
    // Destructor - cleanup any game-specific resources
}

void CupcakeGame::initialize() {
    // Setup initial road segments along -Z
    gameState.roadSegments.clear();
    for (int i = 0; i < gameState.roadSegmentCount; ++i) {
        gameState.roadSegments.push_back(glm::vec3(0.0f, 0.0f, -static_cast<float>(i) * gameState.roadSegmentLength));
    }

    // Pre-spawn initial houses alternating left/right
    gameState.houses.clear();
    float z = -10.0f;
    bool left = true;
    for (int i = 0; i < 14; ++i) {
        House h;
        h.id = gameState.nextHouseId++;
        h.position = glm::vec3(left ? -gameState.houseOffsetX : gameState.houseOffsetX, 1.0f, z);
        h.halfExtents = glm::vec3(1.0f, 2.0f, 1.0f);
        h.requesting = false;
        gameState.houses.push_back(h);
        z -= gameState.houseSpacing;
        left = !left;
    }
}

void CupcakeGame::handleMouseClick(Camera* camera) {
    if (!camera) return;
    
    Projectile p;
    p.alive = true;
    p.life = 0.0f;
    p.pos = camera->Position + camera->Front * 0.8f + glm::vec3(0.0f, -0.2f, 0.0f); // slight basket offset
    glm::vec3 forward = glm::normalize(glm::vec3(camera->Front.x, camera->Front.y + 0.05f, camera->Front.z)); // tiny upward bias
    p.vel = forward * 14.0f; // throw speed
    gameState.projectiles.push_back(p);
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

    // Auto-forward bike movement along camera forward projected to XZ
    glm::vec3 fwd = camera->Front;
    glm::vec3 fwdXZ = glm::normalize(glm::vec3(fwd.x, 0.0f, fwd.z));
    glm::vec3 desiredMovement = fwdXZ * gameState.speed * deltaTime;

    if (physicsSystem) {
        glm::vec3 actualMovement = physicsSystem->moveCamera(*camera, desiredMovement);
        camera->Position += actualMovement;
    }

    // Pacing: increase speed slightly over time
    gameState.pacingTimer += deltaTime;
    if (gameState.pacingTimer >= gameState.pacingStep) {
        gameState.pacingTimer = 0.0f;
        gameState.speed = glm::min(gameState.speed + 0.5f, gameState.maxSpeed);
        gameState.requestInterval = glm::max(gameState.requestInterval - 0.2f, gameState.minInterval);
    }
}

void CupcakeGame::updateRequests(float deltaTime, Camera* camera) {
    if (!camera) return;

    // Request spawning and timeout
    if (gameState.requestingHouseId < 0) {
        gameState.requestTimer += deltaTime;
        if (gameState.requestTimer >= gameState.requestInterval && !gameState.houses.empty()) {
            // Pick a house ahead of camera within ~60 units
            int pick = -1;
            for (size_t i = 0; i < gameState.houses.size(); ++i) {
                const auto& h = gameState.houses[i];
                if (h.position.z < camera->Position.z - 2.0f && h.position.z > camera->Position.z - 60.0f) {
                    pick = static_cast<int>(i);
                    break;
                }
            }
            if (pick >= 0) {
                gameState.houses[pick].requesting = true;
                gameState.requestingHouseId = gameState.houses[pick].id;
                gameState.requestTimeLeft = gameState.requestTimeout;
                gameState.requestTimer = 0.0f;
            }
        }
    } else {
        gameState.requestTimeLeft -= deltaTime;
        if (gameState.requestTimeLeft <= 0.0f) {
            // Missed
            gameState.happiness = glm::max(0, gameState.happiness - 5);
            // clear request
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
            
            // Start quake sound loop if available
            if (audioEngine && audioEngine->isInitialized() && !quakeSoundPlaying && camera) {
                glm::vec3 fwdXZ = glm::normalize(glm::vec3(camera->Front.x, 0.0f, camera->Front.z));
                float dist = 8.0f + 22.0f * gameState.unirand(gameState.rng); // 8..30m
                float side = (gameState.unirand(gameState.rng) < 0.5f ? -1.0f : 1.0f) * (2.0f + 6.0f * gameState.unirand(gameState.rng));
                glm::vec3 right = glm::normalize(glm::cross(fwdXZ, glm::vec3(0,1,0)));
                gameState.quakeEpicenter = camera->Position + fwdXZ * dist + right * side;
                gameState.quakeEpicenter.y = 0.0f;
                
                // Try to start quake sound
                quakeSoundHandle = audioEngine->playLoop3D("resources/audio/052256_cracking-earthquake-cracking-soil-cracking-stone-86770.wav", gameState.quakeEpicenter);
                if (quakeSoundHandle != 0) {
                    quakeSoundPlaying = true;
                }
            }
        }
    } else {
        // Update quake sound position
        if (audioEngine && audioEngine->isInitialized() && quakeSoundPlaying && quakeSoundHandle != 0) {
            audioEngine->setSoundPosition(quakeSoundHandle, gameState.quakeEpicenter);
        }

        gameState.quakeTimeLeft -= deltaTime;
        if (gameState.quakeTimeLeft <= 0.0f) {
            gameState.quakeActive = false;
            gameState.quakeTimeLeft = 0.0f;
            // Stop quake loop sound
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
        if (!p.alive) continue;
        p.life += deltaTime;
        if (p.life > p.maxLife) { p.alive = false; continue; }
        // simple gravity
        p.vel.y += -9.81f * deltaTime * 0.6f;
        p.pos += p.vel * deltaTime;

        // hit test vs houses (AABB)
        for (auto& h : gameState.houses) {
            glm::vec3 minB = h.position - h.halfExtents;
            glm::vec3 maxB = h.position + h.halfExtents;
            glm::vec3 c = glm::clamp(p.pos, minB, maxB);
            float d2 = glm::dot(p.pos - c, p.pos - c);
            if (d2 <= p.radius * p.radius) {
                // Resolve outcome
                if (h.requesting) {
                    gameState.money += 5;
                    gameState.happiness = glm::min(100, gameState.happiness + 3);
                    h.requesting = false;
                    gameState.requestingHouseId = -1;
                    gameState.requestTimer = 0.0f;
                    // SFX success
                } else {
                    gameState.money = std::max(0, gameState.money - 2);
                    // SFX wrong
                }
                p.alive = false;
                break;
            }
        }
    }
    
    // Remove dead projectiles
    gameState.projectiles.erase(
        std::remove_if(gameState.projectiles.begin(), gameState.projectiles.end(),
            [](const Projectile& pr){ return !pr.alive; }),
        gameState.projectiles.end()
    );
}

void CupcakeGame::updateHouses(Camera* camera, PhysicsSystem* physicsSystem) {
    if (!camera) return;

    // Recycle road segments when behind camera
    for (auto& seg : gameState.roadSegments) {
        if (seg.z > camera->Position.z + gameState.roadSegmentLength) {
            // move far ahead
            float minZ = seg.z;
            for (auto& s2 : gameState.roadSegments) minZ = std::min(minZ, s2.z);
            seg.z = minZ - gameState.roadSegmentLength;
        }
    }

    // Cull far behind houses
    gameState.houses.erase(
        std::remove_if(gameState.houses.begin(), gameState.houses.end(),
            [&](const House& h){ return h.position.z > camera->Position.z + 5.0f; }),
        gameState.houses.end()
    );

    // House collision -> Game Over
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

    // Determine farthest Z of existing houses; generate forward to maintain ~150 units ahead
    float farthestZ = camera->Position.z;
    for (auto& h : gameState.houses) farthestZ = std::min(farthestZ, h.position.z);

    auto spawnRowAt = [&](float zrow) {
        // Three spots: Left, Right, and Empty possibility per side
        float pEmpty = 0.35f;     // 35% empty
        float pInactive = 0.60f;  // 60% inactive
        bool canMakeActive = (gameState.requestingHouseId < 0);

        auto spawnSide = [&](bool leftSide){
            float r = gameState.unirand(gameState.rng);
            if (r < pEmpty) return; // empty

            House h;
            h.id = gameState.nextHouseId++;
            float x = leftSide ? -gameState.houseOffsetX : gameState.houseOffsetX;
            h.position = glm::vec3(x, 1.0f, zrow);
            h.halfExtents = glm::vec3(1.0f, 2.0f, 1.0f);
            h.requesting = false;

            if (canMakeActive) {
                float r2 = gameState.unirand(gameState.rng);
                if (r2 > (pEmpty + pInactive)) {
                    // Make this the active request house now
                    h.requesting = true;
                    gameState.requestingHouseId = h.id;
                    gameState.requestTimeLeft = gameState.requestTimeout;
                    gameState.requestTimer = 0.0f;
                    canMakeActive = false;
                }
            }

            gameState.houses.push_back(h);
        };

        // Spawn left and right
        spawnSide(true);
        spawnSide(false);
    };

    while (camera->Position.z - farthestZ < 150.0f) {
        farthestZ -= gameState.houseSpacing;
        spawnRowAt(farthestZ);
    }
}
