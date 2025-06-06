#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <iostream>
#include <glm/glm.hpp> 

#include "assets.hpp"
#include "Mesh.hpp"
#include "ShaderProgram.hpp"
#include "OBJloader.hpp"

class Model {
public:
    std::vector<Mesh> meshes;
    std::string name;
    glm::vec3 origin{};
    glm::vec3 orientation{};
    ShaderProgram& shader;

    // Constructor
    Model(const std::filesystem::path & filename, ShaderProgram& shader) : shader(shader) {
        // Load OBJ file using the OBJ loader
        std::vector<glm::vec3> obj_vertices;
        std::vector<glm::vec2> obj_uvs;
        std::vector<glm::vec3> obj_normals;
        
        if (!loadOBJ(filename.string().c_str(), obj_vertices, obj_uvs, obj_normals)) {
            std::cerr << "Failed to load OBJ file: " << filename << std::endl;
            return;
        }
        
        // Convert to our vertex format and create indices
        std::vector<Vertex> vertices;
        std::vector<GLuint> indices;
        
        // Since the OBJ loader already unwraps the indices, we can directly convert
        for (size_t i = 0; i < obj_vertices.size(); ++i) {
            Vertex vertex;
            vertex.Position = obj_vertices[i];
            
            // Handle cases where we might have fewer normals or UVs than vertices
            if (i < obj_normals.size()) {
                vertex.Normal = obj_normals[i];
            } else {
                vertex.Normal = glm::vec3(0.0f, 0.0f, 1.0f); // Default normal
            }
            
            if (i < obj_uvs.size()) {
                vertex.TexCoords = obj_uvs[i];
            } else {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f); // Default UV
            }
            
            vertices.push_back(vertex);
            indices.push_back(static_cast<GLuint>(i)); // Simple sequential indices
        }
        
        // Create mesh and add to model
        name = filename.stem().string();
        meshes.emplace_back(GL_TRIANGLES, shader, vertices, indices,                           glm::vec3(0.0f), glm::vec3(0.0f), 0);
    }

    // Move constructor
    Model(Model&& other) noexcept 
        : meshes(std::move(other.meshes))
        , name(std::move(other.name))
        , origin(other.origin)
        , orientation(other.orientation)
        , shader(other.shader) {}

    // Move assignment operator
    Model& operator=(Model&& other) noexcept {
        if (this != &other) {
            meshes = std::move(other.meshes);
            name = std::move(other.name);
            origin = other.origin;
            orientation = other.orientation;
            // shader reference stays the same
        }
        return *this;
    }

    // Delete copy constructor and copy assignment
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // update position etc. based on running time
    void update(const float delta_t) {
        // origin += glm::vec3(3,0,0) * delta_t; // s = s0 + v*dt
    }
      void draw(glm::vec3 const & offset = glm::vec3(0.0), glm::vec3 const & rotation = glm::vec3(0.0f)) {
        // call draw() on mesh (all meshes)
        for (auto& mesh : meshes) {
            mesh.draw(origin+offset, orientation+rotation);
        }
    }
};

