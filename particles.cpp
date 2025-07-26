//
// Enhanced particle system with collision detection and physics
// This integrates with the main application's physics system
//

#include "ParticleSystem.hpp"
#include "PhysicsSystem.hpp"
#include <iostream>

// Legacy particle system functions for compatibility
// These are now integrated into the main application

// Particle key callbacks for integration with main app
void particles_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods,
                           ParticleSystem* particleSystem, PhysicsSystem* physicsSystem)
{
    static bool aa = false;
    static GLfloat point_size = 1.0f;

    if ((action == GLFW_PRESS) || (action == GLFW_REPEAT))
    {
        switch (key) {
        case GLFW_KEY_P:  // Toggle point smoothing
            std::cout << "Point AA switch" << std::endl;
            if (aa)
                glEnable(GL_POINT_SMOOTH);
            else
                glDisable(GL_POINT_SMOOTH);
            aa = !aa;
            break;
        case GLFW_KEY_KP_ADD:
            point_size += 1.0f;
            glPointSize(point_size);
            std::cout << "Point size: " << point_size << std::endl;
            break;
        case GLFW_KEY_KP_SUBTRACT:
            if (point_size > 1.0f)
                point_size -= 1.0f;
            glPointSize(point_size);
            std::cout << "Point size: " << point_size << std::endl;
            break;
        case GLFW_KEY_N: // New: emit burst of particles
            if (particleSystem) {
                particleSystem->emit(50);
                std::cout << "Particle burst emitted!" << std::endl;
            }
            break;
        case GLFW_KEY_M: // New: reset particle system
            if (particleSystem) {
                particleSystem->reset();
                std::cout << "Particle system reset!" << std::endl;
            }
            break;
        default:
            break;
        }
    }
}

// Utility function for random values
float random(float min, float max)
{
    return min + (float)rand() / ((float)RAND_MAX / (max - min));
}

