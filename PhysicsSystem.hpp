#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <functional>

// Forward declarations
class Camera;
class ParticleSystem;

// Collision types
enum class CollisionType {
    SPHERE,
    BOX,
    PLANE
};

// Collision object
struct CollisionObject {
    CollisionType type;
    glm::vec3 position;
    glm::vec3 size; // For box: width, height, depth; For sphere: radius in x component
    bool isStatic;
    
    CollisionObject(CollisionType t, const glm::vec3& pos, const glm::vec3& sz, bool stat = true)
        : type(t), position(pos), size(sz), isStatic(stat) {}
};

// Physics world boundaries
struct WorldBounds {
    glm::vec3 min;
    glm::vec3 max;
    
    WorldBounds(const glm::vec3& minBounds, const glm::vec3& maxBounds)
        : min(minBounds), max(maxBounds) {}
};

class PhysicsSystem {
private:
    std::vector<CollisionObject> collisionObjects;
    WorldBounds worldBounds;
    
    // Collision callbacks
    std::function<void(const glm::vec3&)> wallHitCallback;
    std::function<void(const glm::vec3&)> objectHitCallback;
    
public:
    PhysicsSystem();
    
    // World bounds management
    void setWorldBounds(const glm::vec3& min, const glm::vec3& max);
    bool isInsideWorld(const glm::vec3& position) const;
    glm::vec3 constrainToWorld(const glm::vec3& position) const;
    
    // Collision objects
    void addCollisionObject(const CollisionObject& obj);
    void clearCollisionObjects();
    
    // Collision detection
    bool checkCollision(const glm::vec3& position, float radius = 0.5f) const;
    bool checkSphereCollision(const glm::vec3& center, float radius, const CollisionObject& obj) const;
    bool checkBoxCollision(const glm::vec3& center, float radius, const CollisionObject& obj) const;
    
    // Camera movement with collision
    glm::vec3 moveCamera(Camera& camera, const glm::vec3& movement) const;
    
    // Player collision detection
    bool checkPlayerCollision(const glm::vec3& playerPos, float playerRadius) const;
    
    // Projectile collision
    bool checkProjectileHit(const glm::vec3& projectilePos, float projectileRadius, glm::vec3& hitPoint) const;
    
    // Height map walking (simplified)
    float getHeightAtPosition(const glm::vec3& position) const;
    glm::vec3 adjustForHeightMap(const glm::vec3& position) const;
    
    // Wall sliding calculation
    glm::vec3 calculateWallSliding(const glm::vec3& desiredMovement, const glm::vec3& position, float radius) const;
    
    // Callbacks
    void setWallHitCallback(std::function<void(const glm::vec3&)> callback);
    void setObjectHitCallback(std::function<void(const glm::vec3&)> callback);

    void remove_collision_object(const glm::vec3& position);
    
private:
    glm::vec3 getNormalAtCollision(const glm::vec3& position, float radius) const;
};
