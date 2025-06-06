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
    glm::vec3 orientation{};  //rotation by x,y,z axis, in radians
    glm::vec3 scale{1.0f, 1.0f, 1.0f};  // default scale to 1.0
    glm::mat4 local_model_matrix{1.0f}; //for complex transformations (identity matrix)
    
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
            return;
        }

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
    }    void draw(glm::vec3 const &offset = glm::vec3(0.0), 
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
