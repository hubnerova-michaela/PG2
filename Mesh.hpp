#pragma once

#include <string>
#include <vector>
#include <iostream>

#include <GL/glew.h>
#include <glm/glm.hpp> 
#include <glm/ext.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "assets.hpp"
#include "ShaderProgram.hpp"
                         
class Mesh {
public:
    // mesh data
    glm::vec3 origin{};
    glm::vec3 orientation{};
    
    GLuint texture_id{0}; // texture id=0  means no texture
    GLenum primitive_type = GL_POINT;    ShaderProgram& shader;
    
    // mesh material
    glm::vec4 ambient_material{1.0f}; //white, non-transparent 
    glm::vec4 diffuse_material{1.0f}; //white, non-transparent 
    glm::vec4 specular_material{1.0f}; //white, non-transparent
    float reflectivity{1.0f}; 
      // indirect (indexed) draw 
	Mesh(GLenum primitive_type, ShaderProgram& shader, std::vector<Vertex> const & vertices, std::vector<GLuint> const & indices, glm::vec3 const & origin, glm::vec3 const & orientation, GLuint const texture_id = 0):
        primitive_type(primitive_type),
        shader(shader),
        vertices(vertices),
        indices(indices),
        origin(origin),
        orientation(orientation),
        texture_id(texture_id)    {
        // Create and initialize VAO, VBO, EBO using DSA
        glCreateVertexArrays(1, &VAO);
        glCreateBuffers(1, &VBO);
        glCreateBuffers(1, &EBO);
        
        // Upload vertex data
        glNamedBufferData(VBO, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
        
        // Upload index data
        glNamedBufferData(EBO, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
        
        // Bind VBO and EBO to VAO
        glVertexArrayVertexBuffer(VAO, 0, VBO, 0, sizeof(Vertex));
        glVertexArrayElementBuffer(VAO, EBO);
        
        // Configure vertex attributes
        // Position attribute (location = 0)
        glEnableVertexArrayAttrib(VAO, 0);
        glVertexArrayAttribFormat(VAO, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, Position));
        glVertexArrayAttribBinding(VAO, 0, 0);
        
        // Normal attribute (location = 1)
        glEnableVertexArrayAttrib(VAO, 1);
        glVertexArrayAttribFormat(VAO, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, Normal));
        glVertexArrayAttribBinding(VAO, 1, 0);
        
        // Texture coordinates attribute (location = 2)
        glEnableVertexArrayAttrib(VAO, 2);
        glVertexArrayAttribFormat(VAO, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, TexCoords));        glVertexArrayAttribBinding(VAO, 2, 0);
    }

    // Move constructor
    Mesh(Mesh&& other) noexcept 
        : origin(other.origin)
        , orientation(other.orientation)
        , texture_id(other.texture_id)
        , primitive_type(other.primitive_type)
        , shader(other.shader)
        , ambient_material(other.ambient_material)
        , diffuse_material(other.diffuse_material)
        , specular_material(other.specular_material)
        , reflectivity(other.reflectivity)
        , VAO(other.VAO)
        , VBO(other.VBO)
        , EBO(other.EBO)
        , vertices(std::move(other.vertices))
        , indices(std::move(other.indices)) {
        // Reset other's OpenGL handles to prevent double deletion
        other.VAO = 0;
        other.VBO = 0;
        other.EBO = 0;
    }

    // Move assignment operator
    Mesh& operator=(Mesh&& other) noexcept {
        if (this != &other) {
            // Clean up existing resources
            clear();
            
            // Move data from other
            origin = other.origin;
            orientation = other.orientation;
            texture_id = other.texture_id;
            primitive_type = other.primitive_type;
            ambient_material = other.ambient_material;
            diffuse_material = other.diffuse_material;
            specular_material = other.specular_material;
            reflectivity = other.reflectivity;
            VAO = other.VAO;
            VBO = other.VBO;
            EBO = other.EBO;
            vertices = std::move(other.vertices);
            indices = std::move(other.indices);
            
            // Reset other's OpenGL handles
            other.VAO = 0;
            other.VBO = 0;
            other.EBO = 0;
        }
        return *this;
    }

    // Delete copy constructor and copy assignment
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

      void draw(glm::vec3 const & offset, glm::vec3 const & rotation ) {
 		if (VAO == 0) {
			std::cerr << "VAO not initialized!\n";
			return;
		}
 
        shader.activate();
        
        // Create transformation matrix using the offset
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, origin + offset);
        // TODO: Add rotation support: model = glm::rotate(model, rotation...);
        
        // Set model matrix uniform
        shader.setUniform("model", model);
        
        // for future use: set uniform variables: position, textures, etc...  
        //set texture id etc...
        //if (texture_id > 0) {
        //    ...
        //}
        
        //TODO: draw mesh: bind vertex array object, draw all elements with selected primitive type 
        glBindVertexArray(VAO);
        glDrawElements(primitive_type, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        
        shader.deactivate(); 
    }

	void clear(void) {
        texture_id = 0;
        primitive_type = GL_POINT;
        origin = glm::vec3(0.0f);
        orientation = glm::vec3(0.0f);
        
        // Delete all OpenGL allocations
        if (VBO != 0) {
            glDeleteBuffers(1, &VBO);
            VBO = 0;
        }
        if (EBO != 0) {
            glDeleteBuffers(1, &EBO);
            EBO = 0;
        }
        if (VAO != 0) {
            glDeleteVertexArrays(1, &VAO);
            VAO = 0;
        }
        
        vertices.clear();
        indices.clear();
    };

private:
    // OpenGL buffer IDs
    // ID = 0 is reserved (i.e. uninitalized)
    unsigned int VAO{0}, VBO{0}, EBO{0};
    
    // mesh data storage
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
};
  


