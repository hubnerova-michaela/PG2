#include "PhysicsSystem.hpp"
#include "camera.hpp"
#include <algorithm>
#include <iostream>

PhysicsSystem::PhysicsSystem() : worldBounds(glm::vec3(-50.0f, -10.0f, -50.0f), glm::vec3(50.0f, 50.0f, 50.0f)) {
    // Default world bounds - can be changed later
}

void PhysicsSystem::setWorldBounds(const glm::vec3& min, const glm::vec3& max) {
    worldBounds.min = min;
    worldBounds.max = max;
}

bool PhysicsSystem::isInsideWorld(const glm::vec3& position) const {
    return position.x >= worldBounds.min.x && position.x <= worldBounds.max.x &&
           position.y >= worldBounds.min.y && position.y <= worldBounds.max.y &&
           position.z >= worldBounds.min.z && position.z <= worldBounds.max.z;
}

glm::vec3 PhysicsSystem::constrainToWorld(const glm::vec3& position) const {
    return glm::vec3(
        glm::clamp(position.x, worldBounds.min.x, worldBounds.max.x),
        glm::clamp(position.y, worldBounds.min.y, worldBounds.max.y),
        glm::clamp(position.z, worldBounds.min.z, worldBounds.max.z)
    );
}

void PhysicsSystem::addCollisionObject(const CollisionObject& obj) {
    collisionObjects.push_back(obj);
}

void PhysicsSystem::clearCollisionObjects() {
    collisionObjects.clear();
}

bool PhysicsSystem::checkCollision(const glm::vec3& position, float radius) const {
    // Check world bounds first
    if (!isInsideWorld(position)) {
        return true;
    }
    
    // Check collision objects
    for (const auto& obj : collisionObjects) {
        switch (obj.type) {
            case CollisionType::SPHERE:
                if (checkSphereCollision(position, radius, obj)) {
                    return true;
                }
                break;
            case CollisionType::BOX:
                if (checkBoxCollision(position, radius, obj)) {
                    return true;
                }
                break;
            case CollisionType::PLANE:
                // Simple plane collision (floor)
                if (position.y - radius <= obj.position.y) {
                    return true;
                }
                break;
        }
    }
    return false;
}

bool PhysicsSystem::checkSphereCollision(const glm::vec3& center, float radius, const CollisionObject& obj) const {
    float distance = glm::length(center - obj.position);
    return distance <= (radius + obj.size.x); // obj.size.x is the sphere radius
}

bool PhysicsSystem::checkBoxCollision(const glm::vec3& center, float radius, const CollisionObject& obj) const {
    // Calculate closest point on the box to the sphere center
    glm::vec3 boxMin = obj.position - obj.size * 0.5f;
    glm::vec3 boxMax = obj.position + obj.size * 0.5f;
    
    glm::vec3 closestPoint = glm::clamp(center, boxMin, boxMax);
    
    // Check if the distance from sphere center to closest point is less than radius
    float distance = glm::length(center - closestPoint);
    return distance <= radius;
}

glm::vec3 PhysicsSystem::moveCamera(Camera& camera, const glm::vec3& movement) const {
    glm::vec3 newPosition = camera.Position + movement;
    
    // Check if the new position would cause a collision
    if (checkCollision(newPosition, 0.5f)) {
        // Try sliding along walls
        glm::vec3 slidingMovement = calculateWallSliding(movement, camera.Position, 0.5f);
        newPosition = camera.Position + slidingMovement;
        
        // Final check - if still colliding, don't move
        if (checkCollision(newPosition, 0.5f)) {
            return glm::vec3(0.0f); // No movement
        }
        
        // Trigger wall hit callback
        if (wallHitCallback) {
            wallHitCallback(newPosition);
        }
        
        return slidingMovement;
    }
    
    // Check world bounds
    newPosition = constrainToWorld(newPosition);
    
    // Adjust for height map
    newPosition = adjustForHeightMap(newPosition);
    
    return newPosition - camera.Position;
}

bool PhysicsSystem::checkPlayerCollision(const glm::vec3& playerPos, float playerRadius) const {
    return checkCollision(playerPos, playerRadius);
}

bool PhysicsSystem::checkProjectileHit(const glm::vec3& projectilePos, float projectileRadius, glm::vec3& hitPoint) const {
    for (const auto& obj : collisionObjects) {
        bool hit = false;
        
        switch (obj.type) {
            case CollisionType::SPHERE:
                hit = checkSphereCollision(projectilePos, projectileRadius, obj);
                break;
            case CollisionType::BOX:
                hit = checkBoxCollision(projectilePos, projectileRadius, obj);
                break;
            case CollisionType::PLANE:
                hit = (projectilePos.y - projectileRadius <= obj.position.y);
                break;
        }
        
        if (hit) {
            hitPoint = obj.position;
            if (objectHitCallback) {
                objectHitCallback(hitPoint);
            }
            return true;
        }
    }
    return false;
}

float PhysicsSystem::getHeightAtPosition(const glm::vec3& position) const {
    // Simple height map implementation - can be extended with actual height map data
    // For now, create some simple hills using sine waves
    float height = 0.0f;
    
    // Add some terrain variation
    height += 2.0f * sin(position.x * 0.1f) * cos(position.z * 0.1f);
    height += 1.0f * sin(position.x * 0.3f) * sin(position.z * 0.2f);
    
    return std::max(height, 0.0f); // Don't go below ground level
}

glm::vec3 PhysicsSystem::adjustForHeightMap(const glm::vec3& position) const {
    float terrainHeight = getHeightAtPosition(position);
    glm::vec3 adjustedPosition = position;
    
    // Keep camera above terrain with some offset
    if (adjustedPosition.y < terrainHeight + 1.5f) {
        adjustedPosition.y = terrainHeight + 1.5f;
    }
    
    return adjustedPosition;
}

glm::vec3 PhysicsSystem::calculateWallSliding(const glm::vec3& desiredMovement, const glm::vec3& position, float radius) const {
    // Get the normal of the collision surface
    glm::vec3 normal = getNormalAtCollision(position + desiredMovement, radius);
    
    // Project the movement vector onto the plane perpendicular to the normal
    // Sliding vector = movement - (movement · normal) * normal
    float dot = glm::dot(desiredMovement, normal);
    glm::vec3 slidingMovement = desiredMovement - dot * normal;
    
    // Scale down the sliding movement to prevent getting stuck
    slidingMovement *= 0.8f;
    
    return slidingMovement;
}

glm::vec3 PhysicsSystem::getNormalAtCollision(const glm::vec3& position, float radius) const {
    // Simple normal calculation - in a real implementation, this would be more sophisticated
    
    // Check each direction to find the collision normal
    const float epsilon = 0.01f;
    glm::vec3 normals[] = {
        glm::vec3(1.0f, 0.0f, 0.0f),   // Right
        glm::vec3(-1.0f, 0.0f, 0.0f),  // Left
        glm::vec3(0.0f, 1.0f, 0.0f),   // Up
        glm::vec3(0.0f, -1.0f, 0.0f),  // Down
        glm::vec3(0.0f, 0.0f, 1.0f),   // Forward
        glm::vec3(0.0f, 0.0f, -1.0f)   // Backward
    };
    
    for (const auto& normal : normals) {
        glm::vec3 testPos = position + normal * epsilon;
        if (!checkCollision(testPos, radius)) {
            return -normal; // Return inward normal
        }
    }
    
    // Default to up normal if no clear direction is found
    return glm::vec3(0.0f, 1.0f, 0.0f);
}

void PhysicsSystem::setWallHitCallback(std::function<void(const glm::vec3&)> callback) {
    wallHitCallback = callback;
}

void PhysicsSystem::setObjectHitCallback(std::function<void(const glm::vec3&)> callback) {
    objectHitCallback = callback;
}
