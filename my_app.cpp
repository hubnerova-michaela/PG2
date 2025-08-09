//
// myapp.cpp: library demonstrator
//

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <chrono>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

#include "ShaderProgram.hpp"
#include "Model.hpp"
#include "camera.hpp"
#include "assets.hpp"
#include "lighting.hpp"
#include "ParticleSystem.hpp"
#include "PhysicsSystem.hpp"
#include "AudioEngine.hpp"
#include "particles.cpp"
#include "TextureLoader.hpp"
#include "CupcakeGame.hpp"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

// Vertex structure definition
struct vertex
{
    glm::vec3 position;
};

// Forward declarations
void saveSettings();

// Global variables
bool g_vSync = true;
std::string g_windowTitle = "Graphics Application";
int g_windowWidth = 800;
int g_windowHeight = 600;
bool g_antialisingEnabled = false;
int g_antialiasingLevel = 4;

// GL objects and shader program
std::unique_ptr<ShaderProgram> my_shader;
std::unique_ptr<ShaderProgram> particle_shader;
std::unique_ptr<ShaderProgram> road_shader;
std::unique_ptr<LightingSystem> lightingSystem;
std::unique_ptr<ParticleSystem> particleSystem;
std::unique_ptr<PhysicsSystem> physicsSystem;
std::unique_ptr<AudioEngine> audioEngine;

// Quake audio state
static unsigned int g_quakeSoundHandle = 0;
static bool g_quakeSoundPlaying = false;

// Debug world bounds state
static glm::vec3 g_worldMin(-100.0f, -5.0f, -100.0f);
static glm::vec3 g_worldMax(100.0f, 100.0f, 100.0f);

// Audio handles / flags
// (removed duplicate quake variables; using the ones defined above)
std::unordered_map<std::string, std::unique_ptr<Model>> scene;
GLfloat r = 1.0f, g = 0.0f, b = 0.0f, a = 1.0f;

// Road texture resources
static GLuint g_roadTex = 0;
static GLint g_roadTexSamplerLoc = -1;
static GLint g_roadTilingLoc = -1;

// Transparent objects management
std::vector<TransparentObject> transparentObjects;

// ---------- Cupcake Delivery Game: Core State and Systems ----------
// Game instance
std::unique_ptr<CupcakeGame> cupcakeGame;

// Animation control
bool g_animationEnabled = true;
unsigned int g_triangleSoundHandle = 0; // Handle for triangle audio

// Transformation matrices
glm::mat4 projectionMatrix{1.0f};
glm::mat4 viewMatrix{1.0f};
float fov = 60.0f; // Field of view in degrees

// Camera system
std::unique_ptr<Camera> camera;
bool firstMouse = true;
double lastX = 400, lastY = 300; // Initial mouse position (center of 800x600 window)
// Camera is always enabled for free look in game mode
bool cameraEnabled = true;

// Triangle vertices
std::vector<vertex> triangle_vertices = {
    {{0.0f, 0.5f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}},
    {{-0.5f, -0.5f, 0.0f}}};

// Callback functions
void error_callback(int error, const char *description)
{
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    // Delegate particle system keys
    particles_key_callback(window, key, scancode, action, mods, particleSystem.get(), physicsSystem.get());

    // Toggle VSync with F12
    if (key == GLFW_KEY_F12 && action == GLFW_PRESS)
    {
        g_vSync = !g_vSync;
        glfwSwapInterval(g_vSync ? 1 : 0);
        std::cout << "VSync: " << (g_vSync ? "ON" : "OFF") << std::endl;
        
        // Save the new setting
        saveSettings();
    }

    // Toggle antialiasing with F11
    if (key == GLFW_KEY_F11 && action == GLFW_PRESS)
    {
        g_antialisingEnabled = !g_antialisingEnabled;
        std::cout << "Antialiasing toggle requested: " << (g_antialisingEnabled ? "ON" : "OFF") << std::endl;
        std::cout << "Level: " << g_antialiasingLevel << "x" << std::endl;
        std::cout << "Note: Antialiasing changes require application restart to take effect." << std::endl;
        
        // Save the new setting
        saveSettings();
    } // Toggle animation with SPACE
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
        g_animationEnabled = !g_animationEnabled;
        std::cout << "Animation: " << (g_animationEnabled ? "ON" : "OFF") << std::endl;
    }

    // Camera control is always enabled; ignore C key toggling
    if (key == GLFW_KEY_C && action == GLFW_PRESS)
    {
        // no-op
    }

    // Reset camera speed with V key
    if (key == GLFW_KEY_V && action == GLFW_PRESS && camera)
    {
        camera->MovementSpeed = 2.5f;
        std::cout << "Camera speed reset to: " << camera->MovementSpeed << std::endl;
    }

    // Toggle spot light with L key
    if (key == GLFW_KEY_L && action == GLFW_PRESS && lightingSystem)
    {
        // Toggle spot light intensity
        static bool spotLightOn = true;
        spotLightOn = !spotLightOn;
        if (spotLightOn) {
            lightingSystem->spotLight.diffuse = glm::vec3(1.0f, 1.0f, 1.0f);
            lightingSystem->spotLight.specular = glm::vec3(1.0f, 1.0f, 1.0f);
        } else {
            lightingSystem->spotLight.diffuse = glm::vec3(0.0f, 0.0f, 0.0f);
            lightingSystem->spotLight.specular = glm::vec3(0.0f, 0.0f, 0.0f);
        }
        std::cout << "Spot light (headlight): " << (spotLightOn ? "ON" : "OFF") << std::endl;
    }

    // Change triangle color with different keys
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        switch (key)
        {
        case GLFW_KEY_R:
            r = (r > 0.5f) ? 0.0f : 1.0f;
            std::cout << "Red: " << r << std::endl;
            break;
        case GLFW_KEY_G:
            g = (g > 0.5f) ? 0.0f : 1.0f;
            std::cout << "Green: " << g << std::endl;
            break;
        case GLFW_KEY_B:
            b = (b > 0.5f) ? 0.0f : 1.0f;
            std::cout << "Blue: " << b << std::endl;
            break;
        case GLFW_KEY_T: // Changed from A to T for Transparency
            a = (a > 0.5f) ? 0.5f : 1.0f;
            std::cout << "Alpha: " << a << std::endl;
            break;
        case GLFW_KEY_EQUAL: // + key (usually Shift + =)
        case GLFW_KEY_KP_ADD: // Numpad +
            if (mods & GLFW_MOD_SHIFT || key == GLFW_KEY_KP_ADD)
            {
                if (g_antialiasingLevel < 8)
                {
                    g_antialiasingLevel *= 2; // 2, 4, 8
                    if (g_antialiasingLevel > 8) g_antialiasingLevel = 8;
                    std::cout << "Antialiasing level: " << g_antialiasingLevel << "x (restart required)" << std::endl;
                    saveSettings();
                }
            }
            break;
        case GLFW_KEY_MINUS: // - key
        case GLFW_KEY_KP_SUBTRACT: // Numpad -
            if (g_antialiasingLevel > 2)
            {
                g_antialiasingLevel /= 2; // 8, 4, 2
                if (g_antialiasingLevel < 2) g_antialiasingLevel = 2;
                std::cout << "Antialiasing level: " << g_antialiasingLevel << "x (restart required)" << std::endl;
                saveSettings();
            }
            break;
        }
    }

    // Toggle gameplay active/pause (for testing) with F5
    if (key == GLFW_KEY_F5 && action == GLFW_PRESS) {
        if (cupcakeGame) {
            cupcakeGame->getGameState().active = !cupcakeGame->getGameState().active;
            std::cout << "Cupcake game: " << (cupcakeGame->getGameState().active ? "ACTIVE" : "PAUSED") << std::endl;
        }
    }
}

void fbsize_callback(GLFWwindow *window, int width, int height)
{
    g_windowWidth = width;
    g_windowHeight = height;

    // Set viewport
    glViewport(0, 0, width, height);

    // Update projection matrix
    if (height <= 0)
        height = 1; // avoid division by 0
    float ratio = static_cast<float>(width) / height;

    projectionMatrix = glm::perspective(
        glm::radians(fov), // Field of view
        ratio,             // Aspect ratio
        0.1f,              // Near clipping plane
        20000.0f           // Far clipping plane
    );

    // Set projection matrix uniform if shader is active
    if (my_shader)
    {
        my_shader->activate();
        my_shader->setUniform("uProj_m", projectionMatrix);
        my_shader->deactivate();
    }

    std::cout << "Window resized to: " << width << "x" << height << std::endl;
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // Handle cupcake projectile via game class
        if (cupcakeGame) {
            cupcakeGame->handleMouseClick(camera.get());
        }
    }
}

void cursor_position_callback(GLFWwindow *window, double xpos, double ypos)
{
    if (!cameraEnabled || !camera)
        return;

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
        return;
    }

    double xoffset = xpos - lastX;
    double yoffset = lastY - ypos; // Reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera->ProcessMouseMovement(static_cast<GLfloat>(xoffset), static_cast<GLfloat>(yoffset));
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    if (cameraEnabled)
    {
        // Adjust camera movement speed with scroll when camera is enabled
        if (camera)
        {
            // Use multiplicative scaling instead of additive for more predictable behavior
            float speedMultiplier = 1.0f + static_cast<float>(yoffset) * 0.1f;
            camera->MovementSpeed *= speedMultiplier;
            camera->MovementSpeed = glm::clamp(camera->MovementSpeed, 0.5f, 15.0f);
            std::cout << "Camera speed: " << camera->MovementSpeed << std::endl;
        }
    }
    else
    {
        // Adjust alpha with scroll when camera is disabled
        a += static_cast<float>(yoffset) * 0.1f;
        if (a < 0.0f)
            a = 0.0f;
        if (a > 1.0f)
            a = 1.0f;
        std::cout << "Alpha adjusted to: " << a << std::endl;
    }
}

// Debug output callback
void APIENTRY gl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                GLsizei length, const GLchar *message, const void *userParam)
{
    std::string sourceStr;
    switch (source)
    {
    case GL_DEBUG_SOURCE_API:
        sourceStr = "API";
        break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        sourceStr = "Window System";
        break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        sourceStr = "Shader Compiler";
        break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        sourceStr = "Third Party";
        break;
    case GL_DEBUG_SOURCE_APPLICATION:
        sourceStr = "Application";
        break;
    case GL_DEBUG_SOURCE_OTHER:
        sourceStr = "Other";
        break;
    }

    std::string typeStr;
    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR:
        typeStr = "Error";
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        typeStr = "Deprecated Behavior";
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        typeStr = "Undefined Behavior";
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        typeStr = "Portability";
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        typeStr = "Performance";
        break;
    case GL_DEBUG_TYPE_MARKER:
        typeStr = "Marker";
        break;
    case GL_DEBUG_TYPE_PUSH_GROUP:
        typeStr = "Push Group";
        break;
    case GL_DEBUG_TYPE_POP_GROUP:
        typeStr = "Pop Group";
        break;
    case GL_DEBUG_TYPE_OTHER:
        typeStr = "Other";
        break;
    }

    std::string severityStr;
    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:
        severityStr = "High";
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        severityStr = "Medium";
        break;
    case GL_DEBUG_SEVERITY_LOW:
        severityStr = "Low";
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        return; // Ignore notifications
    default:
        severityStr = "Unknown";
        break;
    }

    std::cerr << "OpenGL Debug - " << sourceStr << ", " << typeStr << ", " << severityStr << ": " << message << std::endl;
}

// Function to check OpenGL errors
void checkGLError(const std::string &location)
{
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::string errorString;
        switch (error)
        {
        case GL_INVALID_ENUM:
            errorString = "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            errorString = "GL_INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            errorString = "GL_INVALID_OPERATION";
            break;
        case GL_STACK_OVERFLOW:
            errorString = "GL_STACK_OVERFLOW";
            break;
        case GL_STACK_UNDERFLOW:
            errorString = "GL_STACK_UNDERFLOW";
            break;
        case GL_OUT_OF_MEMORY:
            errorString = "GL_OUT_OF_MEMORY";
            break;
        default:
            errorString = "Unknown error";
            break;
        }
        std::cerr << "OpenGL Error at " << location << ": " << errorString << " (" << error << ")" << std::endl;
    }
}

// Function to initialize assets and shaders
void init_assets()
{
    // Notice: code massively simplified - all moved to specific classes
    // shader: load, compile, link, initialize params - using lighting shaders
    my_shader = std::make_unique<ShaderProgram>("resources/shaders/phong.vert", "resources/shaders/phong.frag");
    
    // Initialize particle shader
    particle_shader = std::make_unique<ShaderProgram>("resources/shaders/particle.vert", "resources/shaders/particle.frag");

    // Road shader: reuse basic textured shader files (create if you have them). For now, use "basic" which supports color only; we will bind texture manually.
    // If "basic" doesn’t support textures, this will still draw; texture use added via fixed pipeline bindings.
    road_shader = std::make_unique<ShaderProgram>("resources/shaders/basic.vert", "resources/shaders/basic.frag");
    if (!road_shader) {
        throw std::runtime_error("Failed to create road shader");
    }

    // Initialize lighting system
    lightingSystem = std::make_unique<LightingSystem>();
    
    // Initialize physics system
    physicsSystem = std::make_unique<PhysicsSystem>();
    
    // Set up world bounds (large enough for our scene)
    // Extend world bounds in -Z to reduce constant boundary collisions during auto-forward motion
    g_worldMin = glm::vec3(-100.0f, -5.0f, -300.0f);
    g_worldMax = glm::vec3(100.0f, 100.0f, 100.0f);
    physicsSystem->setWorldBounds(g_worldMin, g_worldMax);
    
    // Add some collision objects for testing
    physicsSystem->addCollisionObject(CollisionObject(CollisionType::BOX, glm::vec3(5.0f, 1.0f, -5.0f), glm::vec3(2.0f, 2.0f, 2.0f)));
    physicsSystem->addCollisionObject(CollisionObject(CollisionType::SPHERE, glm::vec3(-5.0f, 2.0f, -8.0f), glm::vec3(1.5f, 0.0f, 0.0f)));
    physicsSystem->addCollisionObject(CollisionObject(CollisionType::PLANE, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f))); // Ground plane
    
    // Set up collision callbacks
    // Throttle wall hit logging to avoid spam when sliding along boundaries
    physicsSystem->setWallHitCallback([](const glm::vec3& hitPoint) {
        static auto lastPrint = std::chrono::high_resolution_clock::now();
        static glm::vec3 lastPos(FLT_MAX);
        auto now = std::chrono::high_resolution_clock::now();
        float secs = std::chrono::duration<float>(now - lastPrint).count();

        // Print if sufficient time has passed or we hit a noticeably different spot
        if (secs > 0.5f || glm::length(hitPoint - lastPos) > 0.75f) {
            std::cout << "Wall hit at: (" << hitPoint.x << ", " << hitPoint.y << ", " << hitPoint.z << ")" << std::endl;
            lastPrint = now;
            lastPos = hitPoint;
        }
    });
    
    physicsSystem->setObjectHitCallback([](const glm::vec3& hitPoint) {
        std::cout << "Object hit at: (" << hitPoint.x << ", " << hitPoint.y << ", " << hitPoint.z << ")" << std::endl;
    });
    
    particleSystem = std::make_unique<ParticleSystem>(*particle_shader, 1000);
    particleSystem->setEmitterPosition(glm::vec3(0.0f, 10.0f, -5.0f));

    audioEngine = std::make_unique<AudioEngine>();

    // Initialize cupcake game
    cupcakeGame = std::make_unique<CupcakeGame>();
    cupcakeGame->initialize();

    // Load road texture (JPEG): resources/textures/asphalt.jpg using inline OpenCV helpers
    {
        const std::filesystem::path texPath = "resources/textures/asphalt.jpg";
        try {
            g_roadTex = TextureLoader::textureInit(texPath);
        } catch (const std::exception& e) {
            std::cerr << "Failed to load texture '" << texPath.string() << "': " << e.what() << " - using procedural fallback\n";
            g_roadTex = 0;
        }

        // Fallback: create a small checker texture to avoid null deref later
        if (g_roadTex == 0) {
            const int W = 64, H = 64;
            std::vector<unsigned char> pixels(W * H * 3);
            for (int y = 0; y < H; ++y) {
                for (int x = 0; x < W; ++x) {
                    bool c = ((x / 8) ^ (y / 8)) & 1;
                    int idx = (y * W + x) * 3;
                    pixels[idx+0] = c ? 90 : 40;
                    pixels[idx+1] = c ? 90 : 40;
                    pixels[idx+2] = c ? 90 : 40;
                }
            }
            glGenTextures(1, &g_roadTex);
            glBindTexture(GL_TEXTURE_2D, g_roadTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, W, H, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
    if (audioEngine->init()) {
        std::cout << "Audio engine initialized successfully" << std::endl;
        glm::vec3 quadPosition(0.0f, 0.0f, -3.0f);
        std::cout << "Attempting to load audio file: resources/audio/818034__boatlanman__slow-ethereal-piano-loop-80bpm.wav" << std::endl;
        if (audioEngine->playLoop3D("resources/audio/818034__boatlanman__slow-ethereal-piano-loop-80bpm.wav", quadPosition, &g_triangleSoundHandle)) {
            std::cout << "Triangle audio started successfully at position: " << quadPosition.x << ", " << quadPosition.y << ", " << quadPosition.z << std::endl;
            std::cout << "Triangle sound handle: " << g_triangleSoundHandle << std::endl;
        }
    } else {
        std::cout << "Failed to initialize audio engine" << std::endl;
    }

    // model: load model files to test enhanced OBJ loader
    scene["triangle"] = std::make_unique<Model>("resources/objects/triangle.obj", *my_shader);
    scene["quad"] = std::make_unique<Model>("resources/objects/quad.obj", *my_shader);
    scene["simple"] = std::make_unique<Model>("resources/objects/simple_triangle.obj", *my_shader);

    // Initialize transparent objects - creating 3 semi-transparent objects with different alpha values
    transparentObjects.clear();
    transparentObjects.emplace_back("transparent_triangle1", glm::vec3(-2.0f, 1.5f, -4.0f), 
                                   glm::vec3(0.0f), glm::vec3(0.8f), 
                                   glm::vec4(1.0f, 0.2f, 0.2f, 0.7f)); // Semi-transparent red
    
    transparentObjects.emplace_back("transparent_triangle2", glm::vec3(0.0f, 1.5f, -5.0f), 
                                   glm::vec3(0.0f), glm::vec3(0.8f), 
                                   glm::vec4(0.2f, 1.0f, 0.2f, 0.5f)); // Semi-transparent green
    
    transparentObjects.emplace_back("transparent_triangle3", glm::vec3(2.0f, 1.5f, -6.0f), 
                                   glm::vec3(0.0f), glm::vec3(0.8f), 
                                   glm::vec4(0.2f, 0.2f, 1.0f, 0.3f)); // Semi-transparent blue

    // Set up projection matrix
    if (g_windowHeight <= 0)
        g_windowHeight = 1; // avoid division by 0
    float ratio = static_cast<float>(g_windowWidth) / g_windowHeight;

    projectionMatrix = glm::perspective(
        glm::radians(fov), // Field of view
        ratio,             // Aspect ratio
        0.1f,              // Near clipping plane
        20000.0f           // Far clipping plane
    );                     // Set up view matrix - position camera to better see the scene
    viewMatrix = glm::lookAt(
        glm::vec3(0.0f, 2.0f, 5.0f),  // Camera position (eye)
        glm::vec3(0.0f, 0.0f, -2.0f), // Look at point (center)
        glm::vec3(0.0f, 1.0f, 0.0f)   // Up vector
    );

    // Initialize camera at the same position as the static view
    camera = std::make_unique<Camera>(glm::vec3(0.0f, 2.0f, 5.0f));
    camera->MovementSpeed = 2.5f;
    camera->MouseSensitivity = 0.1f;
    // Always capture cursor for freelook
    if (GLFWwindow* current = glfwGetCurrentContext()) {
        glfwSetInputMode(current, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
    }

    // Always capture the cursor for free look
    GLFWwindow* current = glfwGetCurrentContext();
    if (current) {
        glfwSetInputMode(current, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
    }

    // Setup initial road segments along -Z
    cupcakeGame->getGameState().roadSegments.clear();
    for (int i = 0; i < cupcakeGame->getGameState().roadSegmentCount; ++i) {
        cupcakeGame->getGameState().roadSegments.push_back(glm::vec3(0.0f, 0.0f, -static_cast<float>(i) * cupcakeGame->getGameState().roadSegmentLength));
    }

    // Pre-spawn initial houses alternating left/right
    cupcakeGame->getGameState().houses.clear();
    float z = -10.0f;
    bool left = true;
    for (int i = 0; i < 14; ++i) {
        House h;
        h.id = cupcakeGame->getGameState().nextHouseId++;
        h.position = glm::vec3(left ? -cupcakeGame->getGameState().houseOffsetX : cupcakeGame->getGameState().houseOffsetX, 1.0f, z);
        h.halfExtents = glm::vec3(1.0f, 2.0f, 1.0f);
        h.requesting = false;
        cupcakeGame->getGameState().houses.push_back(h);
        z -= cupcakeGame->getGameState().houseSpacing;
        left = !left;
    }

    // Set initial uniforms
    my_shader->activate();
    my_shader->setUniform("uProj_m", projectionMatrix);
    my_shader->setUniform("uV_m", viewMatrix);
    my_shader->deactivate();
}

// Function to load settings from JSON
nlohmann::json loadSettings()
{
    try
    {
        std::ifstream settingsFile("app_settings.json");
        if (!settingsFile.is_open())
        {
            throw std::runtime_error("Could not open app_settings.json");
        }
        return nlohmann::json::parse(settingsFile);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error loading settings: " << e.what() << std::endl;
        std::cerr << "Using default settings instead." << std::endl;

        nlohmann::json defaultSettings;
        defaultSettings["appname"] = "Graphics Application";
        defaultSettings["default_resolution"]["x"] = 800;
        defaultSettings["default_resolution"]["y"] = 600;
        defaultSettings["antialiasing"]["enabled"] = false;
        defaultSettings["antialiasing"]["level"] = 4;
        return defaultSettings;
    }
}

// Function to validate and apply antialiasing settings
void validateAntialiasingSettings(const nlohmann::json& settings)
{
    bool hasAA = settings.contains("antialiasing");
    if (!hasAA)
    {
        std::cout << "No antialiasing settings found, using default: disabled" << std::endl;
        g_antialisingEnabled = false;
        g_antialiasingLevel = 4;
        return;
    }

    const auto& aaSettings = settings["antialiasing"];
    
    // Get enabled status
    if (aaSettings.contains("enabled") && aaSettings["enabled"].is_boolean())
    {
        g_antialisingEnabled = aaSettings["enabled"].get<bool>();
    }
    else
    {
        std::cerr << "Warning: Malformed antialiasing 'enabled' setting, using default: false" << std::endl;
        g_antialisingEnabled = false;
    }
    
    // Get level
    if (aaSettings.contains("level") && aaSettings["level"].is_number_integer())
    {
        g_antialiasingLevel = aaSettings["level"].get<int>();
    }
    else
    {
        std::cerr << "Warning: Malformed antialiasing 'level' setting, using default: 4" << std::endl;
        g_antialiasingLevel = 4;
    }
    
    // Validate settings logic
    if (g_antialisingEnabled && g_antialiasingLevel <= 1)
    {
        std::cerr << "Warning: Antialiasing is enabled but level is " << g_antialiasingLevel 
                  << " (should be > 1). Disabling antialiasing." << std::endl;
        g_antialisingEnabled = false;
    }
    
    if (g_antialiasingLevel > 8)
    {
        std::cerr << "Warning: Antialiasing level " << g_antialiasingLevel 
                  << " is too high (max recommended: 8). Clamping to 8." << std::endl;
        g_antialiasingLevel = 8;
    }
    
    std::cout << "Antialiasing: " << (g_antialisingEnabled ? "enabled" : "disabled")
              << ", Level: " << g_antialiasingLevel << std::endl;
}

// Function to save current settings to JSON
void saveSettings()
{
    try
    {
        nlohmann::json settings;
        settings["appname"] = g_windowTitle;
        settings["default_resolution"]["x"] = g_windowWidth;
        settings["default_resolution"]["y"] = g_windowHeight;
        settings["vsync_enabled"] = g_vSync;
        settings["debug_mode"] = true;
        settings["antialiasing"]["enabled"] = g_antialisingEnabled;
        settings["antialiasing"]["level"] = g_antialiasingLevel;

        std::ofstream settingsFile("app_settings.json");
        if (settingsFile.is_open())
        {
            settingsFile << settings.dump(2) << std::endl;
            settingsFile.close();
            std::cout << "Settings saved to app_settings.json" << std::endl;
        }
        else
        {
            std::cerr << "Warning: Could not save settings to app_settings.json" << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error saving settings: " << e.what() << std::endl;
    }
}

int main()
{
    try
    {
        // Load settings from JSON
        nlohmann::json settings = loadSettings();

        // Validate and apply antialiasing settings
        validateAntialiasingSettings(settings);

        // Get window resolution from settings
        if (settings["default_resolution"]["x"].is_number_integer() &&
            settings["default_resolution"]["y"].is_number_integer())
        {
            g_windowWidth = settings["default_resolution"]["x"].get<int>();
            g_windowHeight = settings["default_resolution"]["y"].get<int>();
        }

        // Get application name
        if (settings["appname"].is_string())
        {
            g_windowTitle = settings["appname"].get<std::string>();
        }

        // Get VSync setting
        if (settings.contains("vsync_enabled") && settings["vsync_enabled"].is_boolean())
        {
            g_vSync = settings["vsync_enabled"].get<bool>();
        }

        std::cout << "Application: " << g_windowTitle << std::endl;
        std::cout << "Initial resolution: " << g_windowWidth << "x" << g_windowHeight << std::endl;

        // Check if resources directory exists
        if (!std::filesystem::exists("resources"))
        {
            throw std::runtime_error("Directory 'resources' not found. Various media files are expected to be there.");
        }

        // Initialize GLFW
        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        // Set error callback
        glfwSetErrorCallback(error_callback);

        // Set OpenGL version and profile
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

        // Set antialiasing hints if enabled
        if (g_antialisingEnabled)
        {
            glfwWindowHint(GLFW_SAMPLES, g_antialiasingLevel);
            std::cout << "Setting up multisampling with " << g_antialiasingLevel << " samples" << std::endl;
        }

        // Create window
        GLFWwindow *window = glfwCreateWindow(g_windowWidth, g_windowHeight, g_windowTitle.c_str(), NULL, NULL);
        if (!window)
        {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        // Make the window's context current
        glfwMakeContextCurrent(window);

        // Set VSync
        glfwSwapInterval(g_vSync ? 1 : 0);

        // Initialize GLEW
        GLenum err = glewInit();
        if (err != GLEW_OK)
        {
            throw std::runtime_error(std::string("Failed to initialize GLEW: ") +
                                     reinterpret_cast<const char *>(glewGetErrorString(err)));
        }

        // Print OpenGL and GLFW versions
        std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
        std::cout << "GLFW Version: " << glfwGetVersionString() << std::endl;
        std::cout << "GLEW Version: " << glewGetString(GLEW_VERSION) << std::endl;
        std::cout << "GLM Version: " << GLM_VERSION_MAJOR << "." << GLM_VERSION_MINOR << "." << GLM_VERSION_PATCH << std::endl;

        // Verify OpenGL version and profile
        int major, minor, profile;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);

        std::cout << "OpenGL Context Version: " << major << "." << minor << std::endl;
        if (profile == GL_CONTEXT_CORE_PROFILE_BIT)
        {
            std::cout << "OpenGL Profile: Core" << std::endl;
        }
        else
        {
            std::cout << "OpenGL Profile: Not Core!" << std::endl;
        }

        // Check if requested version was obtained
        if (major < 4 || (major == 4 && minor < 6))
        {
            std::cerr << "Warning: OpenGL 4.6 was requested but " << major << "." << minor << " was obtained." << std::endl;
        }

        // Check debug output extension
        if (GLEW_ARB_debug_output)
        {
            std::cout << "Debug output extension is supported." << std::endl;
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(gl_debug_callback, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        }
        else
        {
            std::cout << "Debug output extension is not supported." << std::endl;
        }

        // Set up callbacks
        glfwSetKeyCallback(window, key_callback);
        glfwSetFramebufferSizeCallback(window, fbsize_callback);
        glfwSetMouseButtonCallback(window, mouse_button_callback);
        glfwSetCursorPosCallback(window, cursor_position_callback);
        glfwSetScrollCallback(window, scroll_callback); // Initialize OpenGL assets
        init_assets();

        // Enable depth testing
        glEnable(GL_DEPTH_TEST);

        // Enable alpha blending for transparency
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Enable multisampling if antialiasing is enabled
        if (g_antialisingEnabled)
        {
            glEnable(GL_MULTISAMPLE);
            
            // Verify multisampling is available
            GLint samples;
            glGetIntegerv(GL_SAMPLES, &samples);
            std::cout << "Multisampling enabled with " << samples << " samples" << std::endl;
            
            if (samples == 0)
            {
                std::cerr << "Warning: Multisampling requested but not available!" << std::endl;
            }
        }
        else
        {
            std::cout << "Antialiasing disabled" << std::endl;
        } // Variables for FPS calculation
        auto lastTime = std::chrono::high_resolution_clock::now();
        int frameCount = 0;
        float fps = 0.0f; // Print controls info
        std::cout << "\nControls:\n";
        std::cout << "R, G, B keys - Toggle red, green, blue components\n";
        std::cout << "T key - Toggle alpha/transparency\n";
        std::cout << "SPACE - Toggle animation on/off\n";
        std::cout << "C - Toggle camera control mode\n";
        std::cout << "L - Toggle spot light (headlight) on/off\n";
        std::cout << "Left mouse click - Set random color\n";
        std::cout << "Mouse wheel - Adjust alpha/transparency (or camera speed when camera mode is on)\n";
        std::cout << "+/- keys - Increase/decrease antialiasing level (2x, 4x, 8x)\n";
        std::cout << "F11 - Toggle antialiasing (requires restart)\n";
        std::cout << "F12 - Toggle VSync\n";
        std::cout << "ESC - Exit\n\n";
        std::cout << "Particle System Controls:\n";
        std::cout << "P - Toggle point smoothing\n";
        std::cout << "N - Emit particle burst\n";
        std::cout << "M - Reset particle system\n";
        std::cout << "Numpad +/- - Adjust point size\n\n";
        std::cout << "Camera Controls (when enabled with C key):\n";
        std::cout << "WASD - Move forward/back/left/right (with collision detection)\n";
        std::cout << "Q/E - Move down/up\n";
        std::cout << "Mouse - Look around (cursor will be captured)\n";
        std::cout << "Mouse wheel - Adjust movement speed\n";
        std::cout << "V - Reset camera speed to default\n\n";

        std::cout << "Physics Features:\n";
        std::cout << "- Collision detection with walls and objects\n";
        std::cout << "- Wall sliding when hitting obstacles\n";
        std::cout << "- Height map walking simulation\n";
        std::cout << "- World boundary constraints\n\n";

        std::cout << "Transformation Demo:\n";
        std::cout << "- Left object: Simple Y-axis rotation\n";
        std::cout << "- Center object: Translation (up/down) + Scaling\n";
        std::cout << "- Right object: Multi-axis rotation\n\n";

        std::cout << "Transparency Demo:\n";
        std::cout << "- 3 semi-transparent objects with different alpha values\n";
        std::cout << "- Red (alpha 0.7), Green (alpha 0.5), Blue (alpha 0.3)\n";
        std::cout << "- Objects are depth-sorted for correct transparency rendering\n\n";

        std::cout << "Lighting Demo:\n";
        std::cout << "- Directional light: Animated sun with color changes\n";
        std::cout << "- Point lights: 3 colored lights (red, green, blue) with motion\n";
        std::cout << "- Spot light: Camera-attached headlight (toggle with L key)\n";
        std::cout << "- Phong lighting model with ambient, diffuse, and specular components\n\n";
        
        std::cout << "Particle System Demo:\n";
        std::cout << "- Real-time particle emission and physics simulation\n";
        std::cout << "- Gravity, ground collision, and bounce effects\n";
        std::cout << "- Dynamic particle lifecycle with fade-out\n";
        std::cout << "- Emitter follows camera position\n";
        std::cout << "- Point-based rendering with size variation\n\n";
        
        std::cout << "3D Spatial Audio Demo:\n";
        std::cout << "- Triangle emits looping 3D spatial audio\n";
        std::cout << "- Audio position follows triangle object\n";
        std::cout << "- Listener position and orientation follow camera\n";
        std::cout << "- Move camera around to hear directional audio effects\n";
        std::cout << "- Audio will be louder/quieter based on distance to triangle\n";
        std::cout << "- Sound will pan left/right based on triangle's relative position\n\n"; // Variables for time-based animation
        auto startTime = std::chrono::high_resolution_clock::now();

        // Main rendering loop
        while (!glfwWindowShouldClose(window))
        {
            // Calculate FPS and time
            auto currentTime = std::chrono::high_resolution_clock::now();
            frameCount++;

            // Calculate elapsed time for animations
            float elapsedTime = std::chrono::duration<float>(currentTime - startTime).count(); // Update FPS once per second
            float timeDelta = std::chrono::duration<float>(currentTime - lastTime).count();
            if (timeDelta >= 1.0f)
            {
                fps = frameCount / timeDelta;
                frameCount = 0;
                lastTime = currentTime;

                // Update window title with FPS, VSync and Antialiasing status
                std::string title = g_windowTitle + " | FPS: " + std::to_string(static_cast<int>(fps)) +
                                    " | VSync: " + (g_vSync ? "ON" : "OFF") +
                                    " | AA: " + (g_antialisingEnabled ? ("ON(" + std::to_string(g_antialiasingLevel) + "x)") : "OFF");
                glfwSetWindowTitle(window, title.c_str());
            } // Process camera input and update view matrix
            if (camera && cupcakeGame && cupcakeGame->getGameState().active)
            {
                // Calculate delta time for smooth camera movement
                static auto lastFrameTime = currentTime;
                float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
                lastFrameTime = currentTime;

                // Update cupcake game
                cupcakeGame->update(deltaTime, camera.get(), audioEngine.get(), particleSystem.get(), physicsSystem.get());

                // Update view matrix from camera, with quake shake if active
                glm::mat4 V = camera->GetViewMatrix();
                if (cupcakeGame->getGameState().quakeActive) {
                    // Stronger shake that scales with proximity to epicenter
                    float dist = glm::length((camera ? camera->Position : glm::vec3(0.0f)) - cupcakeGame->getGameState().quakeEpicenter);
                    float k = 10.0f; // tighter falloff
                    float falloff = 1.0f / (1.0f + dist / k);
                    float t = elapsedTime * 22.0f;
                    float amp = cupcakeGame->getGameState().quakeAmplitude * 1.8f; // increase overall intensity
                    float ax = sin(t * 1.9f) * amp * falloff;
                    float ay = cos(t * 1.4f) * amp * 0.8f * falloff;
                    V = glm::translate(V, glm::vec3(ax, ay, 0.0f));
                }
                viewMatrix = V;
            }

            // Update 3D audio listener position based on camera
            audioEngine->setListener(camera->Position, camera->Front, camera->Up);

            // Clear the screen
            // Dynamic background: light blue normally, smoothly blend to gray during earthquake and back after it ends
            {
                glm::vec3 normalSky = glm::vec3(0.55f, 0.70f, 0.95f); // light blue
                glm::vec3 quakeSky  = glm::vec3(0.55f, 0.55f, 0.55f); // gray

                // Persistent blend factor that eases in/out across frames
                static float quakeBlend = 0.0f;
                // Compute target based on quake state
                float target = cupcakeGame->getGameState().quakeActive ? 1.0f : 0.0f;
                // Ease rate (seconds^-1) scaled by frame time; faster when activating, slower when deactivating
                float easeInRate = 2.5f;
                float easeOutRate = 1.5f;

                // Approximate dt via FPS estimate or last timeDelta; we have timeDelta above calculated once per second for FPS.
                static auto lastBG = std::chrono::high_resolution_clock::now();
                auto nowBG = std::chrono::high_resolution_clock::now();
                float dtBG = std::chrono::duration<float>(nowBG - lastBG).count();
                lastBG = nowBG;

                float rate = (target > quakeBlend) ? easeInRate : easeOutRate;
                quakeBlend += (target - quakeBlend) * glm::clamp(rate * dtBG, 0.0f, 1.0f);
                quakeBlend = glm::clamp(quakeBlend, 0.0f, 1.0f);

                glm::vec3 bg = glm::mix(normalSky, quakeSky, quakeBlend);
                glClearColor(bg.r, bg.g, bg.b, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }

            // Update lighting system
            if (lightingSystem) {
                // Update animated lights
                lightingSystem->updateLights(elapsedTime);
                
                // Update spot light to follow camera (headlight effect)
                if (camera) {
                    lightingSystem->spotLight.position = camera->Position;
                    lightingSystem->spotLight.direction = camera->Front;
                } else {
                    // Static camera fallback
                    lightingSystem->spotLight.position = glm::vec3(0.0f, 2.0f, 5.0f);
                    lightingSystem->spotLight.direction = glm::vec3(0.0f, 0.0f, -1.0f);
                }
                
                // Get current camera position for lighting calculations
                glm::vec3 viewPos = camera ? camera->Position : glm::vec3(0.0f, 2.0f, 5.0f);
                
                // Setup lighting uniforms
                lightingSystem->setupLightUniforms(*my_shader, viewPos);
            }

            // STEP 1: Render opaque objects first
            my_shader->activate();

            // Set view matrix (updated each frame for camera movement)
            my_shader->setUniform("uV_m", viewMatrix); 

            // Triangle: Very subtle rotation around Y-axis (left object)
            if (scene.find("triangle") != scene.end())
            {
                glm::vec3 position(-1.5f, 0.0f, -3.0f);
                glm::vec3 rotation(0.0f, g_animationEnabled ? elapsedTime * 8.0f : 0.0f, 0.0f); // Very slow Y rotation
                glm::vec3 scale(1.0f);                                                          // No scaling
                
                // Update triangle audio position to match visual position
                if (audioEngine && audioEngine->isInitialized() && g_triangleSoundHandle != 0) {
                    audioEngine->setSoundPosition(g_triangleSoundHandle, position);
                }
                
                scene.at("triangle")->draw(position, rotation, scale);
            }

            // Draw road segments with road texture (if available)
            if (scene.find("quad") != scene.end()) {
                bool skippedFirst = false;
                // Activate road shader (uses simple MVP; our Model::draw sets matrices via my_shader, so bind manually)
                if (!road_shader) {
                    std::cerr << "Road shader is null, skipping road rendering" << std::endl;
                } else {
                    road_shader->activate();
                }
                // Set shared matrices
                if (road_shader) {
                    road_shader->setUniform("uV_m", viewMatrix);
                    road_shader->setUniform("uProj_m", projectionMatrix);
                    road_shader->setUniform("uniform_Color", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
                }

                // Bind texture unit 0 and set texture uniforms
                if (g_roadTex != 0) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, g_roadTex);
                    if (road_shader) {
                        road_shader->setUniform("textureSampler", 0);
                        road_shader->setUniform("useTexture", true);
                    }
                } else {
                    if (road_shader) {
                        road_shader->setUniform("useTexture", false);
                    }
                }

                for (const auto& seg : cupcakeGame->getGameState().roadSegments) {
                    if (!skippedFirst) { skippedFirst = true; continue; } // skip initial rectangle
                    glm::vec3 pos(seg.x, -0.01f, seg.z);
                    glm::vec3 rot(0.0f);
                    glm::vec3 scl(3.2f, 1.0f, cupcakeGame->getGameState().roadSegmentLength * 0.50f);

                    // Manually draw a textured strip using immediate mode fallback (two triangles) if Model doesn't support UVs.
                    // Build a simple VBO-less draw call using glBegin/glEnd is not available in core profile; instead render the quad model for geometry
                    // and rely on shader sampling with generated UVs from world position.

                    // Reuse the existing model draw for transforms but switch shader: call triangle/quad model with our road_shader active
                    // If Model::draw always uses its own shader, we approximate by drawing colored and letting texture bind affect fragment shader if it samples.
                    scene.at("quad")->draw(pos, rot, scl);
                }

                // Unbind texture
                if (g_roadTex != 0) {
                    glBindTexture(GL_TEXTURE_2D, 0);
                }
                if (road_shader) {
                    road_shader->deactivate();
                }
            }

            // Draw houses as scaled triangles/quads placeholders (use triangle model scaled as a box stand-in)
            for (const auto& h : cupcakeGame->getGameState().houses) {
                if (scene.find("triangle") != scene.end()) {
                    glm::vec3 pos = h.position;
                    glm::vec3 rot(0.0f);
                    glm::vec3 scl(1.5f, 2.5f, 1.0f);
                    scene.at("triangle")->draw(pos, rot, scl);
                }
            }

            // Place particle effects near the "active" house as a big green semi-transparent aura
            if (cupcakeGame->getGameState().requestingHouseId >= 0 && particleSystem) {
                for (const auto& h : cupcakeGame->getGameState().houses) {
                    if (h.id == cupcakeGame->getGameState().requestingHouseId) {
                        // Position aura slightly in front of the house toward the camera and above the roof line
                        glm::vec3 toCamera = glm::normalize((camera ? camera->Position : glm::vec3(0.0f)) - h.position);
                        glm::vec3 inFront = h.position + toCamera * 1.2f + glm::vec3(0.0f, h.indicatorHeight * 0.6f, 0.0f);
                        particleSystem->setEmitterPosition(inFront);
                        // Emit more particles to make the aura dense without spreading too far
                        for (int i = 0; i < 3; ++i) {
                            particleSystem->emit(6);
                        }
                        break;
                    }
                }
            }

            // Quad: Subtle translation up/down (center object)
            if (scene.find("quad") != scene.end())
            {
                glm::vec3 position(0.0f, g_animationEnabled ? sin(elapsedTime * 0.8f) * 0.15f : 0.0f, -3.0f);
                glm::vec3 rotation(0.0f);                                                            // No rotation
                glm::vec3 scale(g_animationEnabled ? 1.0f + 0.05f * sin(elapsedTime * 1.2f) : 1.0f); // Very subtle scale
                scene.at("quad")->draw(position, rotation, scale);
            }

            // Simple triangle: Subtle combined rotation (right object)
            if (scene.find("simple") != scene.end())
            {
                glm::vec3 position(1.5f, 0.0f, -3.0f);
                glm::vec3 rotation(g_animationEnabled ? elapsedTime * 6.0f : 0.0f,
                                   g_animationEnabled ? elapsedTime * 10.0f : 0.0f, 0.0f);
                glm::vec3 scale(0.8f); // Smaller scale
                scene.at("simple")->draw(position, rotation, scale);
            }

            // STEP 2: Render transparent objects (back-to-front order)
            if (!transparentObjects.empty())
            {
                // Calculate distances from camera and sort
                glm::vec3 cameraPos = cameraEnabled && camera ? camera->Position : glm::vec3(0.0f, 2.0f, 5.0f);
                
                for (auto& obj : transparentObjects)
                {
                    // Apply animation to positions
                    glm::vec3 animatedPos = obj.position;
                    if (g_animationEnabled)
                    {
                        // Different animation patterns for each transparent object
                        if (obj.name == "transparent_triangle1")
                        {
                            animatedPos.y += sin(elapsedTime * 0.5f) * 0.3f;
                        }
                        else if (obj.name == "transparent_triangle2")
                        {
                            animatedPos.x += cos(elapsedTime * 0.7f) * 0.2f;
                        }
                        else if (obj.name == "transparent_triangle3")
                        {
                            animatedPos.z += sin(elapsedTime * 0.6f) * 0.1f;
                        }
                    }
                    
                    obj.distance = glm::length(cameraPos - animatedPos);
                }
                
                // Sort by distance (farthest first for correct alpha blending)
                std::sort(transparentObjects.begin(), transparentObjects.end(),
                         [](const TransparentObject& a, const TransparentObject& b) {
                             return a.distance > b.distance;
                         });
                
                // Render transparent objects back-to-front
                for (const auto& obj : transparentObjects)
                {
                    // Apply animation to positions (same logic as above for consistency)
                    glm::vec3 animatedPos = obj.position;
                    glm::vec3 animatedRot = obj.rotation;
                    if (g_animationEnabled)
                    {
                        if (obj.name == "transparent_triangle1")
                        {
                            animatedPos.y += sin(elapsedTime * 0.5f) * 0.3f;
                            animatedRot.y = elapsedTime * 15.0f;
                        }
                        else if (obj.name == "transparent_triangle2")
                        {
                            animatedPos.x += cos(elapsedTime * 0.7f) * 0.2f;
                            animatedRot.x = elapsedTime * 20.0f;
                        }
                        else if (obj.name == "transparent_triangle3")
                        {
                            animatedPos.z += sin(elapsedTime * 0.6f) * 0.1f;
                            animatedRot.z = elapsedTime * 12.0f;
                        }
                    }
                    
                    // Use triangle model for transparent objects (could be any model)
                    // Note: This will use the same color as the main objects (r,g,b,a)
                    if (scene.find("triangle") != scene.end())
                    {
                        scene.at("triangle")->draw(animatedPos, animatedRot, obj.scale);
                    }
                }
            }

            my_shader->deactivate();
            
            // Update and render particle system
            if (particleSystem && g_animationEnabled) {
                // Calculate delta time for particle physics
                static auto lastParticleTime = currentTime;
                float particleDeltaTime = std::chrono::duration<float>(currentTime - lastParticleTime).count();
                lastParticleTime = currentTime;

                // Update particle system faster to look like an aura
                particleSystem->update(particleDeltaTime * 1.5f);

                // IMPORTANT: Do NOT bind particle emitter to camera anymore.
                // Emitter position is set when a house is active in the indicator block.
                // If no active house, move emitter far away to effectively hide particles.
                if (cupcakeGame->getGameState().requestingHouseId < 0) {
                    particleSystem->setEmitterPosition(glm::vec3(0.0f, -10000.0f, 0.0f));
                }

                // Render particles
                // Provide time to particle shader for subtle flicker
                if (particle_shader) {
                    particle_shader->activate();
                    particle_shader->setUniform("uTime", elapsedTime);
                    particle_shader->deactivate();
                }
                particleSystem->draw(viewMatrix, projectionMatrix);
            }

            // HUD / Overlays
            // Simple "GAME OVER" overlay when gameplay is inactive
            if (!cupcakeGame->getGameState().active) {
                // Draw a centered translucent panel using scissor without altering global background color intent
                glEnable(GL_SCISSOR_TEST);
                int rw = static_cast<int>(g_windowWidth * 0.65f);
                int rh = static_cast<int>(g_windowHeight * 0.28f);
                int rx = (g_windowWidth - rw) / 2;
                int ry = (g_windowHeight - rh) / 2;
                glScissor(rx, ry, rw, rh);

                // Choose panel color based on current sky color so it’s not pure black
                // Recompute the sky color similarly to the clear step to pick a suitable panel tint
                glm::vec3 normalSky = glm::vec3(0.55f, 0.70f, 0.95f); // light blue
                glm::vec3 quakeSky  = glm::vec3(0.55f, 0.55f, 0.55f); // gray
                // We don’t have access to the persistent quakeBlend here; approximate via darker overlay that works on both
                glm::vec3 panel = glm::mix(quakeSky, normalSky, 0.25f) * 0.6f; // soft bluish-gray
                glClearColor(panel.r, panel.g, panel.b, 0.55f); // semi-transparent, not pure black
                glClear(GL_COLOR_BUFFER_BIT);
                glDisable(GL_SCISSOR_TEST);

                // Also set window title to reflect game over status prominently
                std::string title = g_windowTitle + " | GAME OVER";
                glfwSetWindowTitle(window, title.c_str());
            }

            // Check for errors
            checkGLError("Main Loop");

            // Swap front and back buffers
            glfwSwapBuffers(window);

            // Poll for and process events
            glfwPollEvents();
        }

        // Clean up GL resources
        my_shader->clear();
        if (particle_shader) {
            particle_shader->clear();
        }

        // Clean up
        glfwDestroyWindow(window);
        glfwTerminate();

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        glfwTerminate();
        return -1;
    }
}