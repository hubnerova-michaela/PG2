#include <string>
#include <GL/glew.h>
#include <glm/glm.hpp>

#include "OBJloader.hpp"

#define MAX_LINE_SIZE 255

bool loadOBJ(const char *path, std::vector<glm::vec3> &out_vertices, std::vector<glm::vec2> &out_uvs, std::vector<glm::vec3> &out_normals)
{
	std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;
	std::vector<glm::vec3> temp_vertices;
	std::vector<glm::vec2> temp_uvs;
	std::vector<glm::vec3> temp_normals;

	out_vertices.clear();
	out_uvs.clear();
	out_normals.clear();

	FILE *file;
	fopen_s(&file, path, "r");
	if (file == NULL)
	{
		printf("Impossible to open the file !\n");
		return false;
	}
	while (1)
	{

		char lineHeader[MAX_LINE_SIZE] = {0}; // Initialize to ensure null termination
		int res = fscanf_s(file, "%254s", lineHeader, (unsigned)_countof(lineHeader));
		if (res == EOF)
		{
			break;
		}

		if (strcmp(lineHeader, "v") == 0)
		{
			glm::vec3 vertex;
			fscanf_s(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
			temp_vertices.push_back(vertex);
		}
		else if (strcmp(lineHeader, "vt") == 0)
		{
			glm::vec2 uv;
			fscanf_s(file, "%f %f\n", &uv.y, &uv.x);
			temp_uvs.push_back(uv);
		}
		else if (strcmp(lineHeader, "vn") == 0)
		{
			glm::vec3 normal;
			fscanf_s(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
			temp_normals.push_back(normal);
		}
		else if (strcmp(lineHeader, "f") == 0)
		{
			// Read the entire face line
			char faceLine[MAX_LINE_SIZE];
			fgets(faceLine, MAX_LINE_SIZE, file);

			// Parse face line to handle various formats and polygon triangulation
			std::vector<unsigned int> faceVertices, faceUVs, faceNormals;
			
			// Parse each vertex in the face
			char* context = nullptr;
			char* token = strtok_s(faceLine, " \t\n", &context);
			while (token != nullptr) {
				unsigned int v = 0, vt = 0, vn = 0;
				
				// Try different face formats
				if (sscanf_s(token, "%d/%d/%d", &v, &vt, &vn) == 3) {
					// v/vt/vn format (vertex/texture/normal)
					faceVertices.push_back(v);
					faceUVs.push_back(vt);
					faceNormals.push_back(vn);
				}
				else if (sscanf_s(token, "%d//%d", &v, &vn) == 2) {
					// v//vn format (vertex//normal, no texture)
					faceVertices.push_back(v);
					faceUVs.push_back(0); // No UV, will be handled later
					faceNormals.push_back(vn);
				}
				else if (sscanf_s(token, "%d/%d", &v, &vt) == 2) {
					// v/vt format (vertex/texture, no normal)
					faceVertices.push_back(v);
					faceUVs.push_back(vt);
					faceNormals.push_back(0); // No normal, will be calculated later
				}
				else if (sscanf_s(token, "%d", &v) == 1) {
					// v format (vertex only)
					faceVertices.push_back(v);
					faceUVs.push_back(0);
					faceNormals.push_back(0);
				}
				
				token = strtok_s(nullptr, " \t\n", &context);
			}
			
			// Triangulate the polygon (fan triangulation from first vertex)
			if (faceVertices.size() >= 3) {
				for (size_t i = 1; i < faceVertices.size() - 1; ++i) {
					// Triangle: v0, vi, vi+1
					vertexIndices.push_back(faceVertices[0]);
					vertexIndices.push_back(faceVertices[i]);
					vertexIndices.push_back(faceVertices[i + 1]);
					
					uvIndices.push_back(faceUVs[0]);
					uvIndices.push_back(faceUVs[i]);
					uvIndices.push_back(faceUVs[i + 1]);
					
					normalIndices.push_back(faceNormals[0]);
					normalIndices.push_back(faceNormals[i]);
					normalIndices.push_back(faceNormals[i + 1]);
				}
			}
		}
	}

	// Handle missing texture coordinates by providing defaults
	bool hasTexCoords = !temp_uvs.empty();
	if (!hasTexCoords)
	{
		// Create default UV coordinates for each vertex
		for (size_t i = 0; i < temp_vertices.size(); ++i)
		{
			temp_uvs.push_back(glm::vec2(0.0f, 0.0f));
		}
		// Fill uvIndices to match vertexIndices
		for (size_t i = 0; i < vertexIndices.size(); ++i)
		{
			uvIndices.push_back(vertexIndices[i]);
		}
	}

	// Handle missing normals by calculating them
	bool hasNormals = !temp_normals.empty();
	if (!hasNormals)
	{
		printf("No normals found in OBJ file. Calculating face normals...\n");

		// Calculate face normals for each triangle
		for (size_t i = 0; i < vertexIndices.size(); i += 3)
		{
			glm::vec3 v0 = temp_vertices[vertexIndices[i] - 1];
			glm::vec3 v1 = temp_vertices[vertexIndices[i + 1] - 1];
			glm::vec3 v2 = temp_vertices[vertexIndices[i + 2] - 1];

			glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
			temp_normals.push_back(normal);
		}
		// Create normal indices (one normal per triangle, shared by all vertices)
		for (size_t i = 0; i < vertexIndices.size(); i += 3)
		{
			unsigned int normalIdx = static_cast<unsigned int>((i / 3) + 1); // OBJ indices are 1-based
			normalIndices.push_back(normalIdx);
			normalIndices.push_back(normalIdx);
			normalIndices.push_back(normalIdx);
		}
	}

	// unroll from indirect to direct vertex specification
	// sometimes not necessary, definitely not optimal

	for (unsigned int u = 0; u < vertexIndices.size(); u++)
	{
		unsigned int vertexIndex = vertexIndices[u];
		glm::vec3 vertex = temp_vertices[vertexIndex - 1];
		out_vertices.push_back(vertex);
	}
	for (unsigned int u = 0; u < uvIndices.size(); u++)
	{
		unsigned int uvIndex = uvIndices[u];
		if (uvIndex > 0 && uvIndex <= temp_uvs.size())
		{
			glm::vec2 uv = temp_uvs[uvIndex - 1];
			out_uvs.push_back(uv);
		}
		else
		{
			// Fallback to default UV if index is invalid
			out_uvs.push_back(glm::vec2(0.0f, 0.0f));
		}
	}
	for (unsigned int u = 0; u < normalIndices.size(); u++)
	{
		unsigned int normalIndex = normalIndices[u];
		if (normalIndex > 0 && normalIndex <= temp_normals.size())
		{
			glm::vec3 normal = temp_normals[normalIndex - 1];
			out_normals.push_back(normal);
		}
		else
		{
			// Fallback to default normal if index is invalid
			out_normals.push_back(glm::vec3(0.0f, 0.0f, 1.0f));
		}
	}

	fclose(file);
	return true;
}
