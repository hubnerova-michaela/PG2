#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "assets.hpp"
#include "Mesh.hpp"
#include "ShaderProgram.hpp"
#include "OBJloader.hpp"

class Model
{
public:
    std::vector<Mesh> meshes;
    std::string name;

    // original position
    glm::vec3 origin{};
    glm::vec3 orientation{};            // rotation by x,y,z axis, in radians
    glm::vec3 scale{1.0f, 1.0f, 1.0f};  // default scale to 1.0
    glm::mat4 local_model_matrix{1.0f}; // for complex transformations (identity matrix)

    ShaderProgram &shader;

    // Constructor
    Model(const std::filesystem::path &filename, ShaderProgram &shader) : shader(shader)
    {
        // Load OBJ file using the OBJ loader
        std::vector<glm::vec3> obj_vertices;
        std::vector<glm::vec2> obj_uvs;
        std::vector<glm::vec3> obj_normals;

        if (!loadOBJ(filename.string().c_str(), obj_vertices, obj_uvs, obj_normals))
        {
            std::cerr << "Failed to load OBJ file: " << filename << std::endl;
            throw std::runtime_error("OBJ loading failed for " + filename.string());
        }

        std::cout << "DEBUG Model '" << filename.filename().string() << "': loaded " 
                  << obj_vertices.size() << " vertices, " 
                  << obj_uvs.size() << " UVs, " 
                  << obj_normals.size() << " normals" << std::endl;

        // Check if we have any geometry at all
        if (obj_vertices.empty()) {
            std::cerr << "ERROR: No vertices loaded for " << filename << std::endl;
            throw std::runtime_error("No vertices in OBJ file: " + filename.string());
        }

        // Calculate bounding box to check model scale
        glm::vec3 minBounds(FLT_MAX), maxBounds(-FLT_MAX);
        for (const auto& vertex : obj_vertices) {
            minBounds = glm::min(minBounds, vertex);
            maxBounds = glm::max(maxBounds, vertex);
        }
        glm::vec3 size = maxBounds - minBounds;
        std::cout << "DEBUG Model bounds: min(" << minBounds.x << "," << minBounds.y << "," << minBounds.z 
                  << ") max(" << maxBounds.x << "," << maxBounds.y << "," << maxBounds.z 
                  << ") size(" << size.x << "," << size.y << "," << size.z << ")" << std::endl;

        // Convert to our vertex format and create indices
        std::vector<Vertex> vertices;
        std::vector<GLuint> indices;

        // Since the OBJ loader already unwraps the indices, we can directly convert
        for (size_t i = 0; i < obj_vertices.size(); ++i)
        {
            Vertex vertex;
            vertex.Position = obj_vertices[i];

            // Handle cases where we might have fewer normals or UVs than vertices
            if (i < obj_normals.size())
            {
                vertex.Normal = obj_normals[i];
            }
            else
            {
                vertex.Normal = glm::vec3(0.0f, 0.0f, 1.0f); // Default normal
            }

            if (i < obj_uvs.size())
            {
                vertex.TexCoords = obj_uvs[i];
            }
            else
            {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f); // Default UV
            }

            vertices.push_back(vertex);
            indices.push_back(static_cast<GLuint>(i)); // Simple sequential indices
        }

        // Create mesh and add to model
        name = filename.stem().string();
        meshes.emplace_back(GL_TRIANGLES, shader, vertices, indices, glm::vec3(0.0f), glm::vec3(0.0f), 0);
        
        // Set reasonable default material properties for visibility
        if (!meshes.empty()) {
            // Use different materials for different house types for variety
            if (name == "house") {
                // Brown wood material
                meshes[0].ambient_material = glm::vec4(0.15f, 0.07f, 0.02f, 1.0f);   // Dark brown ambient
                meshes[0].diffuse_material = glm::vec4(0.73f, 0.32f, 0.07f, 1.0f);   // Beech wood diffuse
                meshes[0].specular_material = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);     // Gray specular
                meshes[0].reflectivity = 0.5f; 
            } else if (name == "Bambo_House") {
                // Green bamboo material
                meshes[0].ambient_material = glm::vec4(0.05f, 0.15f, 0.05f, 1.0f);   // Dark green ambient
                meshes[0].diffuse_material = glm::vec4(0.3f, 0.7f, 0.2f, 1.0f);     // Green diffuse
                meshes[0].specular_material = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);    // Low specular
                meshes[0].reflectivity = 0.3f; 
            } else if (name == "Cyprys_House") {
                // Gray stone material
                meshes[0].ambient_material = glm::vec4(0.1f, 0.1f, 0.12f, 1.0f);    // Dark gray ambient
                meshes[0].diffuse_material = glm::vec4(0.6f, 0.6f, 0.7f, 1.0f);     // Light gray diffuse
                meshes[0].specular_material = glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);    // Gray specular
                meshes[0].reflectivity = 0.4f; 
            } else if (name == "Building,") {
                // Red brick material
                meshes[0].ambient_material = glm::vec4(0.15f, 0.05f, 0.05f, 1.0f);  // Dark red ambient
                meshes[0].diffuse_material = glm::vec4(0.8f, 0.3f, 0.2f, 1.0f);     // Red brick diffuse
                meshes[0].specular_material = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);    // Low specular
                meshes[0].reflectivity = 0.2f; 
            } else {
                // Default material
                meshes[0].ambient_material = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);   // Dark gray ambient
                meshes[0].diffuse_material = glm::vec4(0.8f, 0.6f, 0.4f, 1.0f);   // Light brown diffuse
                meshes[0].specular_material = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);  // Gray specular
                meshes[0].reflectivity = 0.5f; 
            }
        }
    }

    // Move constructor
    Model(Model &&other) noexcept
        : meshes(std::move(other.meshes)), name(std::move(other.name)), origin(other.origin), orientation(other.orientation), shader(other.shader) {}

    // Move assignment operator
    Model &operator=(Model &&other) noexcept
    {
        if (this != &other)
        {
            meshes = std::move(other.meshes);
            name = std::move(other.name);
            origin = other.origin;
            orientation = other.orientation;
            // shader reference stays the same
        }
        return *this;
    }

    // Delete copy constructor and copy assignment
    Model(const Model &) = delete;
    Model &operator=(const Model &) = delete;

    // update position etc. based on running time
    void update(const float delta_t)
    {
        // origin += glm::vec3(3,0,0) * delta_t; // s = s0 + v*dt
    }
    void draw(glm::vec3 const &offset = glm::vec3(0.0),
              glm::vec3 const &rotation = glm::vec3(0.0f),
              glm::vec3 const &scale_change = glm::vec3(1.0f))
    {
        // compute complete transformation
        glm::mat4 t = glm::translate(glm::mat4(1.0f), origin);
        glm::mat4 rx = glm::rotate(glm::mat4(1.0f), orientation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 ry = glm::rotate(glm::mat4(1.0f), orientation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 rz = glm::rotate(glm::mat4(1.0f), orientation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);

        glm::mat4 m_off = glm::translate(glm::mat4(1.0f), offset);
        glm::mat4 m_rx = glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 m_ry = glm::rotate(glm::mat4(1.0f), rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 m_rz = glm::rotate(glm::mat4(1.0f), rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 m_s = glm::scale(glm::mat4(1.0f), scale_change);

        glm::mat4 model_matrix = local_model_matrix * s * rz * ry * rx * t * m_s * m_rz * m_ry * m_rx * m_off;

        // call draw() on mesh (all meshes)
        for (auto &mesh : meshes)
        {
            mesh.draw(model_matrix);
        }
    }

    void draw(glm::mat4 const &model_matrix)
    {
        for (auto &mesh : meshes)
        {
            mesh.draw(local_model_matrix * model_matrix);
        }
    }
};
