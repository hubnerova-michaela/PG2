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
#include "TextureLoader.hpp"

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
        std::vector<OBJMeshData> mesh_datas;
        if (!loadOBJMeshes(filename.string().c_str(), mesh_datas)) {
            std::cerr << "Failed to load OBJ file: " << filename << std::endl;
            throw std::runtime_error("OBJ loading failed for " + filename.string());
        }
        name = filename.stem().string();
        for (const auto& mesh_data : mesh_datas) {
            std::vector<Vertex> vertices;
            for (size_t i = 0; i < mesh_data.vertices.size(); ++i) {
                Vertex v;
                v.Position = mesh_data.vertices[i];
                v.Normal = (i < mesh_data.normals.size()) ? mesh_data.normals[i] : glm::vec3(0,0,1);
                v.TexCoords = (i < mesh_data.uvs.size()) ? mesh_data.uvs[i] : glm::vec2(0,0);
                vertices.push_back(v);
            }
            Mesh mesh(GL_TRIANGLES, shader, vertices, mesh_data.indices, glm::vec3(0.0f), glm::vec3(0.0f), 0);
            mesh.diffuse_material = glm::vec4(mesh_data.diffuse_color, 1.0f);
            meshes.push_back(std::move(mesh));
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
