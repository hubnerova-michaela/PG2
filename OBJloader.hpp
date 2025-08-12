#pragma once
#ifndef OBJloader_H
#define OBJloader_H

#include <vector>
#include <string>
#include <unordered_map>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>

struct OBJMeshData
{
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::vector<GLuint> indices;
    std::string material_name;
    glm::vec3 diffuse_color;
};

bool loadOBJMeshes(
    const char *path,
    std::vector<OBJMeshData> &out_meshes,
    std::string *out_texture_path = nullptr);

bool loadOBJ(const char *path, std::vector<glm::vec3> &out_vertices,
             std::vector<glm::vec2> &out_uvs, std::vector<glm::vec3> &out_normals);

std::vector<std::string> split(const std::string &s, char delimiter);

#endif
