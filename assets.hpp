#pragma once

#include <glm/glm.hpp>
#include <string>

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

// Transparent object structure for managing objects with alpha values
struct TransparentObject {
    std::string name;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    glm::vec4 color; // RGBA with alpha channel
    float distance; // Distance from camera for depth sorting
    
    TransparentObject(const std::string& n, const glm::vec3& pos, const glm::vec3& rot, 
                     const glm::vec3& sc, const glm::vec4& col) 
        : name(n), position(pos), rotation(rot), scale(sc), color(col), distance(0.0f) {}
};

