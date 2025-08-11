#define GLM_ENABLE_EXPERIMENTAL
#include "CupcakeGame.hpp"
#include "camera.hpp"
#include "AudioEngine.hpp"
#include "ParticleSystem.hpp"
#include "PhysicsSystem.hpp"

#include <iostream>
#include <algorithm>
#include <glm/gtx/norm.hpp>

CupcakeGame::CupcakeGame()
    : rng(std::random_device{}()), unirand(0.0f, 1.0f) {
    this->houseGenerator = std::make_unique<HouseGenerator>();
    this->houseModels = { "bambo_house", "cyprys_house", "building" };
    this->cachedPhysicsSystem = nullptr;
}

CupcakeGame::~CupcakeGame() {}

void CupcakeGame::initialize() {
    this->gameState = GameState();

    gameState.money = 50;
    gameState.happiness = 50;

    for (int i = 0; i < gameState.roadSegmentCount; ++i) {
        gameState.roadSegments.push_back(glm::vec3(0.0f, 0.0f, -static_cast<float>(i) * gameState.roadSegmentLength));
    }
}

void CupcakeGame::update(float deltaTime, Camera* camera, AudioEngine* audioEngine,
    ParticleSystem* particleSystem, PhysicsSystem* physicsSystem) {
    if (!gameState.active) return;
    updateMovement(deltaTime, camera, physicsSystem);
    houseGenerator->updateRequests(deltaTime, this->gameState, camera);
    updateEarthquake(deltaTime, camera, audioEngine, particleSystem);
    updateProjectiles(deltaTime);
    updateHouses(camera, physicsSystem);

    if (particleSystem) {
        for (auto& house : gameState.houses) {
            if (house.deliveryEffectTimer > 0.0f) {
                house.deliveryEffectTimer -= deltaTime;
                glm::vec3 emitterPos = house.position + glm::vec3(0.0f, house.indicatorHeight * 0.7f, 0.0f);
                particleSystem->setEmitterPosition(emitterPos);
                particleSystem->emit(2);
                if (house.deliveryEffectTimer <= 0.0f) {
                    house.delivered = false;
                }
            }
        }
    }
}

void CupcakeGame::updateHouses(Camera* camera, PhysicsSystem* physicsSystem) {
    if (!camera) return;

    this->cachedPhysicsSystem = physicsSystem;

    for (auto& seg : gameState.roadSegments) {
        if (seg.z > camera->Position.z + gameState.roadSegmentLength) {
            float minZ = seg.z;
            for (auto& s2 : gameState.roadSegments) {
                minZ = std::min(minZ, s2.z);
            }
            seg.z = minZ - gameState.roadSegmentLength;
        }
    }

    gameState.houses.erase(
        std::remove_if(gameState.houses.begin(), gameState.houses.end(),
            [&](const House& h) {
                if (h.position.z > camera->Position.z + gameState.roadSegmentLength) {
                    if (physicsSystem) {
                        physicsSystem->removeCollisionObject(h.position);
                    }
                    return true;
                }
                return false;
            }),
        gameState.houses.end()
    );
}

void CupcakeGame::handleMouseClick(Camera* camera) {
    if (!camera || !gameState.active) return;
    gameState.money -= 10;
    glm::vec3 projectilePos = camera->Position + camera->Front * 2.0f;
    glm::vec3 projectileVel = camera->Front * 50.0f;
    auto projectile = std::make_unique<Projectile>(projectilePos, projectileVel, 0.3f);
    gameState.projectiles.push_back(std::move(projectile));
}

void CupcakeGame::updateProjectiles(float deltaTime) {
    for (auto& projectile : gameState.projectiles) {
        if (!projectile || !projectile->alive) continue;
        projectile->update(deltaTime);
        for (auto& house : gameState.houses) {
            if (house.delivered) continue;
            glm::vec3 boxMin = house.position - house.halfExtents;
            glm::vec3 boxMax = house.position + house.halfExtents;
            glm::vec3 closestPoint = glm::clamp(projectile->position, boxMin, boxMax);
            float distanceSq = glm::distance2(projectile->position, closestPoint);
            if (distanceSq < (projectile->radius * projectile->radius)) {
                projectile->alive = false;
                if (house.requesting) {
                    gameState.money += 20;
                    gameState.happiness = std::min(100, gameState.happiness + 10);
                    house.requesting = false;
                    gameState.requestingHouseId = -1;
                    gameState.requestTimeLeft = 0.0f;
                    house.delivered = true;
                    house.deliveryEffectTimer = 2.5f;
                }
                else {
                    gameState.happiness = std::max(0, gameState.happiness - 10);
                }
                break;
            }
        }
    }
    gameState.projectiles.erase(
        std::remove_if(gameState.projectiles.begin(), gameState.projectiles.end(),
            [](const std::unique_ptr<Projectile>& p) {
                return !p || !p->alive;
            }),
        gameState.projectiles.end()
    );
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

void CupcakeGame::updateMovement(float deltaTime, Camera* camera, PhysicsSystem* physicsSystem) {
    if (!camera || !gameState.active) return;
    float moveSpeed = gameState.speed * deltaTime;
    glm::vec3 movement = glm::vec3(0.0f, 0.0f, -moveSpeed);
    if (physicsSystem) {
        glm::vec3 actualMovement = physicsSystem->moveCamera(*camera, movement);
        camera->Position += actualMovement;
    }
    else {
        camera->Position += movement;
    }
}

void CupcakeGame::updateEarthquake(float deltaTime, Camera* camera, AudioEngine* audioEngine, ParticleSystem* particleSystem) {
    if (!camera) return;
    if (gameState.quakeCooldown > 0.0f) {
        gameState.quakeCooldown -= deltaTime;
    }
    if (!gameState.quakeActive && gameState.quakeCooldown <= 0.0f) {
        gameState.quakeActive = true;
        gameState.quakeTimeLeft = gameState.quakeDuration;
        gameState.quakeEpicenter = camera->Position + glm::vec3(
            (unirand(rng) - 0.5f) * 50.0f, 0.0f, (unirand(rng) - 0.5f) * 50.0f
        );
        if (audioEngine && !quakeSoundPlaying) {
            if (audioEngine->playLoop3D("resources/audio/052256_cracking-earthquake-cracking-soil-cracking-stone-86770.wav", gameState.quakeEpicenter, &quakeSoundHandle)) {
                quakeSoundPlaying = true;
            }
        }
    }
    if (gameState.quakeActive) {
        gameState.quakeTimeLeft -= deltaTime;
        if (gameState.quakeTimeLeft <= 0.0f) {
            gameState.quakeActive = false;
            gameState.quakeCooldown = 25.0f;
            if (audioEngine && quakeSoundPlaying) {
                audioEngine->stopSound(quakeSoundHandle);
                quakeSoundPlaying = false;
            }
        }
        else {
            float distance = glm::length(camera->Position - gameState.quakeEpicenter);
            float intensity = std::max(0.0f, 1.0f - distance / 100.0f);
            float shakeX = (unirand(rng) - 0.5f) * gameState.quakeAmplitude * intensity;
            float shakeY = (unirand(rng) - 0.5f) * gameState.quakeAmplitude * intensity;
            float shakeZ = (unirand(rng) - 0.5f) * gameState.quakeAmplitude * intensity;
            camera->Position += glm::vec3(shakeX, shakeY, shakeZ);

            if (particleSystem) {
                for (const auto& segment : gameState.roadSegments) {
                    float halfWidth = gameState.roadSegmentWidth * 0.5f;
                    float halfLength = gameState.roadSegmentLength * 0.5f;
                    
                    glm::vec3 pos = glm::vec3(
                        (unirand(rng) - 0.5f) * gameState.roadSegmentWidth,
                        0.1f,
                        segment.z + (unirand(rng) - 0.5f) * gameState.roadSegmentLength
                    );

                    particleSystem->setEmitterPosition(pos);
                    particleSystem->emitSmoke(5);
                }
            }
        }
    }
}