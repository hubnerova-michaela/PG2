#include "HouseGenerator.hpp"
#include "CupcakeGame.hpp" // Includes GameState and Camera definitions
#include "camera.hpp"
#include <iostream>
#include <vector>

HouseGenerator::HouseGenerator()
    : rng(std::random_device{}()), uniformDist(0.0f, 1.0f), requestTimer(0.0f)
{
}

void HouseGenerator::updateRequests(float deltaTime, GameState &gameState, const Camera *camera)
{
    if (!camera)
        return;

    if (gameState.requesting_house_id < 0)
    {
        this->requestTimer += deltaTime;
        if (this->requestTimer >= gameState.request_interval && !gameState.houses.empty())
        {
            std::vector<int> potentialIndices;
            for (size_t i = 0; i < gameState.houses.size(); ++i)
            {
                const auto &h = gameState.houses[i];
                if (h.position.z < camera->Position.z - 20.0f && h.position.z > camera->Position.z - 100.0f)
                {
                    potentialIndices.push_back(static_cast<int>(i));
                }
            }

            if (!potentialIndices.empty())
            {
                std::uniform_int_distribution<size_t> dist(0, potentialIndices.size() - 1);
                int pickedHouseIndex = potentialIndices[dist(this->rng)];

                gameState.houses[pickedHouseIndex].requesting = true;
                gameState.requesting_house_id = gameState.houses[pickedHouseIndex].id;
                gameState.request_time_left = gameState.request_timeout;
                this->requestTimer = 0.0f;
                std::cout << "REQUEST HANDLER: New delivery request at house " << gameState.requesting_house_id << std::endl;
            }
        }
    }
    else
    {
        gameState.request_time_left -= deltaTime;
        if (gameState.request_time_left <= 0.0f)
        {
            gameState.happiness = glm::max(0, gameState.happiness - 8);
            std::cout << "REQUEST HANDLER: Missed delivery! Happiness: " << gameState.happiness << "%" << std::endl;

            for (auto &h : gameState.houses)
            {
                if (h.id == gameState.requesting_house_id)
                {
                    h.requesting = false;
                    break;
                }
            }
            gameState.requesting_house_id = -1;
            this->requestTimer = 0.0f;
        }
    }
}