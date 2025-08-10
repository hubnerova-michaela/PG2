#define GLM_ENABLE_EXPERIMENTAL
#include "CupcakeGame.hpp"
#include "camera.hpp"
#include "AudioEngine.hpp"
#include "ParticleSystem.hpp"
#include "PhysicsSystem.hpp"

// Include the Sol2 header for Lua binding
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <iostream>
#include <algorithm>
#include <glm/gtx/norm.hpp>

// --- Class Implementation ---

CupcakeGame::CupcakeGame() 
    : rng(std::random_device{}()), unirand(0.0f, 1.0f) {
    this->houseGenerator = std::make_unique<HouseGenerator>();
    this->houseModels = {"house", "bambo_house", "cyprys_house", "building"};
    this->cachedPhysicsSystem = nullptr;

    // --- Initialize Lua and Create Bindings ---
    try {
        // Create Lua state
        lua = std::make_unique<sol::state>();
        
        // 1. Open standard Lua libraries needed for the script (base utils, math functions)
        lua->open_libraries(sol::lib::base, sol::lib::math);

        // 2. Expose the glm::vec3 type to Lua so it can be used for positions.
        // We define how to create it and what properties (x, y, z) it has.
        lua->new_usertype<glm::vec3>("vec3",
            sol::constructors<glm::vec3(), glm::vec3(float, float, float)>(),
            "x", &glm::vec3::x,
            "y", &glm::vec3::y,
            "z", &glm::vec3::z
        );

        // 3. Create the 'add_house' function that the Lua script can call.
        // This is the bridge from Lua back into our C++ engine.
        // It takes a Lua table as an argument, which we parse into house properties.
        lua->set_function("add_house", [&](sol::table args) {
            std::cout << "add_house called from Lua" << std::endl;
            
            try {
                House house;
                house.id = gameState.nextHouseId++;
                
                // Extract model name
                sol::object modelObj = args["model"];
                if (modelObj.is<std::string>()) {
                    house.modelName = modelObj.as<std::string>();
                    std::cout << "Model name: " << house.modelName << std::endl;
                } else {
                    std::cerr << "ERROR: Model name is not a string!" << std::endl;
                    return;
                }
                
                // Extract position
                sol::object posObj = args["position"];
                if (posObj.is<glm::vec3>()) {
                    house.position = posObj.as<glm::vec3>();
                    std::cout << "Position (vec3): (" << house.position.x << ", " << house.position.y << ", " << house.position.z << ")" << std::endl;
                } else {
                    std::cerr << "ERROR: Position is not a vec3!" << std::endl;
                    std::cerr << "Position type ID: " << static_cast<int>(posObj.get_type()) << std::endl;
                    return;
                }

                // Get properties based on the model name
                house.halfExtents = getHouseExtents(house.modelName);
                house.indicatorHeight = getIndicatorHeight(house.modelName);
                
                gameState.houses.push_back(house);
                std::cout << "House added successfully. Total houses: " << gameState.houses.size() << std::endl;
                
                // If the physics system is available, add a collision object for the new house.
                if (this->cachedPhysicsSystem) {
                     this->cachedPhysicsSystem->addCollisionObject({CollisionType::BOX, house.position, house.halfExtents});
                     std::cout << "Collision object added for house" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "ERROR in add_house: " << e.what() << std::endl;
            }
        });

        // 4. Load and execute the Lua script file.
        std::cout << "Attempting to load Lua script: resources/scripts/generation.lua" << std::endl;
        lua->script_file("resources/scripts/generation.lua");
        
        // 5. Pass the C++ list of available house models into the Lua global state.
        // Now the Lua script can access this list.
        (*lua)["house_models"] = this->houseModels;
        std::cout << "Successfully loaded 'generation.lua' script." << std::endl;
        
        // Test if the function exists
        sol::function test_func = (*lua)["generate_houses_at_z"];
        if (test_func.valid()) {
            std::cout << "Function 'generate_houses_at_z' found in Lua script." << std::endl;
        } else {
            std::cout << "WARNING: Function 'generate_houses_at_z' NOT found in Lua script." << std::endl;
        }

    } catch (const sol::error& e) {
        // If the script has an error or can't be found, print it to the console.
        std::cerr << "LUA SCRIPTING ERROR: " << e.what() << std::endl;
        std::cerr << "Falling back to C++ house generation." << std::endl;
        lua.reset(); // Clear the Lua state to indicate it's not working
    } catch (const std::exception& e) {
        std::cerr << "SOL2 LIBRARY ERROR: " << e.what() << std::endl;
        std::cerr << "Sol2 library not properly linked. Falling back to C++ house generation." << std::endl;
        lua.reset();
    }
}

CupcakeGame::~CupcakeGame() {}

void CupcakeGame::initialize() {
    this->gameState = GameState(); 

    gameState.money = 50;
    gameState.happiness = 50;

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
    
    std::cout << "Initialized with " << gameState.houses.size() << " houses." << std::endl;
}

void CupcakeGame::update(float deltaTime, Camera* camera, AudioEngine* audioEngine, 
                       ParticleSystem* particleSystem, PhysicsSystem* physicsSystem) {
    if (!gameState.active) return;

    updateMovement(deltaTime, camera, physicsSystem);
    houseGenerator->updateRequests(deltaTime, this->gameState, camera);
    updateEarthquake(deltaTime, camera, audioEngine);
    updateProjectiles(deltaTime);
    updateHouses(camera, physicsSystem);

    // Handle post-delivery particle effects for successful deliveries
    if (particleSystem) {
        for (auto& house : gameState.houses) {
            if (house.deliveryEffectTimer > 0.0f) {
                house.deliveryEffectTimer -= deltaTime;
                
                // Set emitter position above the house
                glm::vec3 emitterPos = house.position + glm::vec3(0.0f, house.indicatorHeight * 0.7f, 0.0f);
                particleSystem->setEmitterPosition(emitterPos);
                
                // Emit particles
                particleSystem->emit(2); // Emit 2 particles per frame
                
                if (house.deliveryEffectTimer <= 0.0f) {
                    house.delivered = false;
                }
            }
        }
    }
}

void CupcakeGame::updateHouses(Camera* camera, PhysicsSystem* physicsSystem) {
    if (!camera) return;

    // Cache the physics system pointer so our Lua callback can use it.
    this->cachedPhysicsSystem = physicsSystem;

    // Recycle road segments
    for (auto& seg : gameState.roadSegments) {
        if (seg.z > camera->Position.z + gameState.roadSegmentLength) {
            float minZ = seg.z;
            for (auto& s2 : gameState.roadSegments) minZ = std::min(minZ, s2.z);
            seg.z = minZ - gameState.roadSegmentLength;
        }
    }

    // Despawn houses that are behind the camera
    gameState.houses.erase(
        std::remove_if(gameState.houses.begin(), gameState.houses.end(),
            [&](const House& h){ 
                if (h.position.z > camera->Position.z + 15.0f) {
                    if (physicsSystem) {
                        physicsSystem->removeCollisionObject(h.position);
                    }
                    return true;
                }
                return false; 
            }),
        gameState.houses.end()
    );

    // Check if we need to spawn new houses ahead of the player
    float farthestZ = camera->Position.z;
    for (const auto& h : gameState.houses) {
        farthestZ = std::min(farthestZ, h.position.z);
    }
    if (camera->Position.z - farthestZ < 200.0f) {
        spawnNewHouses(farthestZ - gameState.houseSpacing, physicsSystem);
    }
}

void CupcakeGame::spawnNewHouses(float zPosition, PhysicsSystem* physicsSystem) {
    std::cout << "spawnNewHouses called with zPosition: " << zPosition << std::endl;
    
    // For debugging - let's always use C++ fallback for now
    std::cout << "Using C++ fallback for house generation." << std::endl;
    const float houseOffsetX = 10.0f; // Match the Lua script's house_side_offset
    const float emptyPlotProbability = 0.2f;
    
    // Left side of the road
    if (unirand(rng) > emptyPlotProbability) {
        House house;
        house.id = gameState.nextHouseId++;
        house.modelName = getRandomHouseModel();
        house.position = glm::vec3(-houseOffsetX, 0.0f, zPosition);
        house.halfExtents = getHouseExtents(house.modelName);
        house.indicatorHeight = getIndicatorHeight(house.modelName);
        
        gameState.houses.push_back(house);
        std::cout << "C++ Left house added: " << house.modelName << " at (" 
                  << house.position.x << ", " << house.position.y << ", " << house.position.z 
                  << "). Total houses: " << gameState.houses.size() << std::endl;
        
        if (physicsSystem) {
            physicsSystem->addCollisionObject({CollisionType::BOX, house.position, house.halfExtents});
        }
    }
    
    // Right side of the road
    if (unirand(rng) > emptyPlotProbability) {
        House house;
        house.id = gameState.nextHouseId++;
        house.modelName = getRandomHouseModel();
        house.position = glm::vec3(houseOffsetX, 0.0f, zPosition);
        house.halfExtents = getHouseExtents(house.modelName);
        house.indicatorHeight = getIndicatorHeight(house.modelName);
        
        gameState.houses.push_back(house);
        std::cout << "C++ Right house added: " << house.modelName << " at (" 
                  << house.position.x << ", " << house.position.y << ", " << house.position.z 
                  << "). Total houses: " << gameState.houses.size() << std::endl;
        
        if (physicsSystem) {
            physicsSystem->addCollisionObject({CollisionType::BOX, house.position, house.halfExtents});
        }
    }
    
    /*
    // Try to use Lua script first if available
    if (lua) {
        try {
            sol::function generate_func = (*lua)["generate_houses_at_z"];
            if (generate_func.valid()) {
                std::cout << "Calling Lua generate_houses_at_z function..." << std::endl;
                // Execute the Lua function, passing the Z position.
                generate_func(zPosition); 
                std::cout << "Lua function executed. Current house count: " << gameState.houses.size() << std::endl;
                return; // Success, exit early
            } else {
                std::cerr << "LUA SCRIPTING ERROR: Could not find function 'generate_houses_at_z' in script." << std::endl;
            }
        } catch(const sol::error& e) {
            std::cerr << "LUA SCRIPTING ERROR during execution: " << e.what() << std::endl;
        }
    }
    
    // Fallback to C++ implementation if Lua is not available or fails
    std::cout << "Using C++ fallback for house generation." << std::endl;
    const float houseOffsetX = 10.0f; // Match the Lua script's house_side_offset
    const float emptyPlotProbability = 0.2f;
    
    // Left side of the road
    if (unirand(rng) > emptyPlotProbability) {
        House house;
        house.id = gameState.nextHouseId++;
        house.modelName = getRandomHouseModel();
        house.position = glm::vec3(-houseOffsetX, 0.0f, zPosition);
        house.halfExtents = getHouseExtents(house.modelName);
        house.indicatorHeight = getIndicatorHeight(house.modelName);
        
        gameState.houses.push_back(house);
        
        if (physicsSystem) {
            physicsSystem->addCollisionObject({CollisionType::BOX, house.position, house.halfExtents});
        }
    }
    
    // Right side of the road
    if (unirand(rng) > emptyPlotProbability) {
        House house;
        house.id = gameState.nextHouseId++;
        house.modelName = getRandomHouseModel();
        house.position = glm::vec3(houseOffsetX, 0.0f, zPosition);
        house.halfExtents = getHouseExtents(house.modelName);
        house.indicatorHeight = getIndicatorHeight(house.modelName);
        
        gameState.houses.push_back(house);
        
        if (physicsSystem) {
            physicsSystem->addCollisionObject({CollisionType::BOX, house.position, house.halfExtents});
        }
    }
    */
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
                } else {
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

// --- Helper Functions and Unchanged Logic ---

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
    } else {
        camera->Position += movement;
    }
}

void CupcakeGame::updateEarthquake(float deltaTime, Camera* camera, AudioEngine* audioEngine) {
    // This function remains unchanged.
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
        } else {
            float distance = glm::length(camera->Position - gameState.quakeEpicenter);
            float intensity = std::max(0.0f, 1.0f - distance / 100.0f);
            
            float shakeX = (unirand(rng) - 0.5f) * gameState.quakeAmplitude * intensity;
            float shakeY = (unirand(rng) - 0.5f) * gameState.quakeAmplitude * intensity;
            float shakeZ = (unirand(rng) - 0.5f) * gameState.quakeAmplitude * intensity;
            
            camera->Position += glm::vec3(shakeX, shakeY, shakeZ);
        }
    }
}
