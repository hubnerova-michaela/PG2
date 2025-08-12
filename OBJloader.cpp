#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <array>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "OBJloader.hpp"
#include <vector>
#include <iostream>
#include <iterator>
#define MAX_LINE_SIZE 255

// Helper function to split a string by a delimiter
std::vector<std::string> split(const std::string &s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

// Helper: parse MTL file for all diffuse colors
static std::unordered_map<std::string, glm::vec3> parseMTLColors(const std::string &mtlPath)
{
    std::unordered_map<std::string, glm::vec3> colors;
    std::ifstream mtlFile(mtlPath);
    if (!mtlFile.is_open())
        return colors;
    std::string line, current;
    while (std::getline(mtlFile, line))
    {
        std::istringstream iss(line);
        std::string key;
        iss >> key;
        if (key == "newmtl")
        {
            iss >> current;
        }
        else if (key == "Kd" && !current.empty())
        {
            float r, g, b;
            iss >> r >> g >> b;
            colors[current] = glm::vec3(r, g, b);
        }
    }
    return colors;
}

bool loadOBJMeshes(const char *path, std::vector<OBJMeshData> &out_meshes, std::string *out_texture_path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return false;
    std::string objDir, mtlFileName;
    {
        std::string objPath(path);
        size_t lastSlash = objPath.find_last_of("/\\");
        objDir = (lastSlash != std::string::npos) ? objPath.substr(0, lastSlash + 1) : "";
    }
    std::unordered_map<std::string, glm::vec3> mtlColors;
    std::string currentMaterial;
    std::vector<glm::vec3> temp_vertices;
    std::vector<glm::vec2> temp_uvs;
    std::vector<glm::vec3> temp_normals;
    struct Face
    {
        std::vector<unsigned int> v, vt, vn;
        std::string material;
    };
    std::vector<Face> faces;
    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string key;
        iss >> key;
        if (key == "mtllib")
        {
            iss >> mtlFileName;
            mtlFileName = objDir + mtlFileName;
            mtlColors = parseMTLColors(mtlFileName);
        }
        else if (key == "v")
        {
            glm::vec3 v;
            iss >> v.x >> v.y >> v.z;
            temp_vertices.push_back(v);
        }
        else if (key == "vt")
        {
            glm::vec2 uv;
            iss >> uv.x >> uv.y;
            temp_uvs.push_back(uv);
        }
        else if (key == "vn")
        {
            glm::vec3 n;
            iss >> n.x >> n.y >> n.z;
            temp_normals.push_back(n);
        }
        else if (key == "usemtl")
        {
            iss >> currentMaterial;
        }
        else if (key == "f")
        {
            Face face;
            face.material = currentMaterial;
            std::string vert;
            while (iss >> vert)
            {
                unsigned int v = 0, vt = 0, vn = 0;
                if (sscanf_s(vert.c_str(), "%d/%d/%d", &v, &vt, &vn) == 3)
                {
                    face.v.push_back(v);
                    face.vt.push_back(vt);
                    face.vn.push_back(vn);
                }
                else if (sscanf_s(vert.c_str(), "%d//%d", &v, &vn) == 2)
                {
                    face.v.push_back(v);
                    face.vt.push_back(0);
                    face.vn.push_back(vn);
                }
                else if (sscanf_s(vert.c_str(), "%d/%d", &v, &vt) == 2)
                {
                    face.v.push_back(v);
                    face.vt.push_back(vt);
                    face.vn.push_back(0);
                }
                else if (sscanf_s(vert.c_str(), "%d", &v) == 1)
                {
                    face.v.push_back(v);
                    face.vt.push_back(0);
                    face.vn.push_back(0);
                }
            }
            faces.push_back(face);
        }
    }
    std::unordered_map<std::string, std::vector<Face>> material_faces;
    for (const auto &f : faces)
        material_faces[f.material].push_back(f);
    for (const auto &[mat, fs] : material_faces)
    {
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec2> uvs;
        std::vector<glm::vec3> normals;
        std::vector<GLuint> indices;
        std::unordered_map<std::string, GLuint> vert_map;
        GLuint idx = 0;
        for (const auto &face : fs)
        {
            size_t n = face.v.size();
            for (size_t i = 1; i + 1 < n; ++i)
            {
                std::array<size_t, 3> tri = {0, i, i + 1};
                for (size_t tri_idx = 0; tri_idx < 3; ++tri_idx)
                {
                    size_t k = tri[tri_idx];
                    std::string key = std::to_string(face.v[k]) + "/" + std::to_string(face.vt[k]) + "/" + std::to_string(face.vn[k]);
                    auto it = vert_map.find(key);
                    if (it == vert_map.end())
                    {
                        vertices.push_back(temp_vertices[face.v[k] - 1]);
                        if (face.vt[k] > 0 && face.vt[k] <= temp_uvs.size())
                            uvs.push_back(temp_uvs[face.vt[k] - 1]);
                        else
                            uvs.push_back(glm::vec2(0.0f, 0.0f));
                        if (face.vn[k] > 0 && face.vn[k] <= temp_normals.size())
                            normals.push_back(temp_normals[face.vn[k] - 1]);
                        else
                            normals.push_back(glm::vec3(0.0f, 0.0f, 1.0f));
                        vert_map[key] = idx;
                        indices.push_back(idx++);
                    }
                    else
                    {
                        indices.push_back(it->second);
                    }
                }
            }
        }
        OBJMeshData mesh;
        mesh.vertices = std::move(vertices);
        mesh.uvs = std::move(uvs);
        mesh.normals = std::move(normals);
        mesh.indices = std::move(indices);
        mesh.material_name = mat;
        mesh.diffuse_color = mtlColors.count(mat) ? mtlColors[mat] : glm::vec3(1.0f);
        out_meshes.push_back(std::move(mesh));
    }
    return true;
}

bool loadOBJ(const char *path, std::vector<glm::vec3> &out_vertices,
             std::vector<glm::vec2> &out_uvs, std::vector<glm::vec3> &out_normals)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "Impossible to open the file: " << path << std::endl;
        return false;
    }

    std::vector<glm::vec3> temp_vertices;
    std::vector<glm::vec2> temp_uvs;
    std::vector<glm::vec3> temp_normals;
    std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        std::stringstream ss(line);
        std::string lineHeader;
        ss >> lineHeader;

        if (lineHeader == "v")
        {
            glm::vec3 vertex;
            ss >> vertex.x >> vertex.y >> vertex.z;
            temp_vertices.push_back(vertex);
        }
        else if (lineHeader == "vt")
        {
            glm::vec2 uv;
            ss >> uv.x >> uv.y;
            // uv.y = 1.0f - uv.y;
            temp_uvs.push_back(uv);
        }
        else if (lineHeader == "vn")
        {
            glm::vec3 normal;
            ss >> normal.x >> normal.y >> normal.z;
            temp_normals.push_back(normal);
        }
        else if (lineHeader == "f")
        {
            std::string vertex_str;
            std::vector<std::string> face_vertices;
            while (ss >> vertex_str)
            {
                face_vertices.push_back(vertex_str);
            }

            auto processVertex = [&](const std::string &vertexData)
            {
                std::vector<std::string> tokens = split(vertexData, '/');
                if (tokens.size() >= 1 && !tokens[0].empty())
                    vertexIndices.push_back(std::stoul(tokens[0]));
                if (tokens.size() >= 2 && !tokens[1].empty())
                    uvIndices.push_back(std::stoul(tokens[1]));
                if (tokens.size() >= 3 && !tokens[2].empty())
                    normalIndices.push_back(std::stoul(tokens[2]));
            };

            for (size_t i = 1; i + 1 < face_vertices.size(); ++i)
            {
                processVertex(face_vertices[0]);
                processVertex(face_vertices[i]);
                processVertex(face_vertices[i + 1]);
            }
        }
    }

    out_vertices.clear();
    out_uvs.clear();
    out_normals.clear();

    out_vertices.reserve(vertexIndices.size());
    out_uvs.reserve(uvIndices.size());
    out_normals.reserve(normalIndices.size());

    // De-index the data
    for (unsigned int vertexIndex : vertexIndices)
    {
        out_vertices.push_back(temp_vertices[vertexIndex - 1]);
    }

    for (unsigned int uvIndex : uvIndices)
    {
        out_uvs.push_back(temp_uvs[uvIndex - 1]);
    }

    for (unsigned int normalIndex : normalIndices)
    {
        out_normals.push_back(temp_normals[normalIndex - 1]);
    }

    return true;
}