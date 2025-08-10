#define GLM_ENABLE_EXPERIMENTAL
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
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

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
#include "HouseGenerator.hpp"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

struct vertex
{
    glm::vec3 position;
};

static GLuint g_roadVAO = 0;
static GLuint g_roadVBO = 0;
static GLuint g_roadEBO = 0;

void initRoadGeometry() {
    float roadVertices[] = {
        -1.0f, 0.0f, -1.0f,  0.0f, 0.0f,  // bottom left
         1.0f, 0.0f, -1.0f,  2.0f, 0.0f,  // bottom right (increased U coord for tiling)
         1.0f, 0.0f,  1.0f,  2.0f, 2.0f,  // top right
        -1.0f, 0.0f,  1.0f,  0.0f, 2.0f   // top left
    };
    
    unsigned int roadIndices[] = {
        0, 1, 2,  // first triangle
        0, 2, 3   // second triangle
    };
    
    glGenVertexArrays(1, &g_roadVAO);
    glGenBuffers(1, &g_roadVBO);
    glGenBuffers(1, &g_roadEBO);
    
    glBindVertexArray(g_roadVAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, g_roadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(roadVertices), roadVertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_roadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(roadIndices), roadIndices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
}

void cleanupRoadGeometry() {
    if (g_roadVAO != 0) {
        glDeleteVertexArrays(1, &g_roadVAO);
        g_roadVAO = 0;
    }
    if (g_roadVBO != 0) {
        glDeleteBuffers(1, &g_roadVBO);
        g_roadVBO = 0;
    }
    if (g_roadEBO != 0) {
        glDeleteBuffers(1, &g_roadEBO);
        g_roadEBO = 0;
    }
}

void saveSettings();
void checkGLError(const std::string &location);

bool g_vSync = true;
std::string g_windowTitle = "Cupcagame";
int g_windowWidth = 800;
int g_windowHeight = 600;
bool g_antialisingEnabled = false;
int g_antialiasingLevel = 4;

std::unique_ptr<ShaderProgram> my_shader;
std::unique_ptr<ShaderProgram> particle_shader;
std::unique_ptr<ShaderProgram> road_shader;
std::unique_ptr<LightingSystem> lightingSystem;
std::unique_ptr<ParticleSystem> particleSystem;
std::unique_ptr<PhysicsSystem> physicsSystem;
std::unique_ptr<AudioEngine> audioEngine;

static unsigned int g_quakeSoundHandle = 0;
static bool g_quakeSoundPlaying = false;
static unsigned int g_triangleSoundHandle = 0;

static glm::vec3 g_worldMin(-100.0f, -5.0f, -100.0f);
static glm::vec3 g_worldMax(100.0f, 100.0f, 100.0f);

std::unordered_map<std::string, std::unique_ptr<Model>> scene;
GLfloat r = 1.0f, g = 0.0f, b = 0.0f, a = 1.0f;

static GLuint g_roadTex = 0;
static GLint g_roadTexSamplerLoc = -1;
static GLint g_roadTilingLoc = -1;

std::vector<TransparentObject> transparentObjects;

std::unique_ptr<CupcakeGame> cupcakeGame;

bool g_animationEnabled = true;

glm::mat4 projectionMatrix{1.0f};
glm::mat4 viewMatrix{1.0f};
float fov = 60.0f;

std::unique_ptr<Camera> camera;
bool firstMouse = true;
double lastX = 400, lastY = 300;
bool g_bikeSteeringMode = true; // Enable bike steering by default

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

    particles_key_callback(window, key, scancode, action, mods, particleSystem.get(), physicsSystem.get());

    if (key == GLFW_KEY_F10 && action == GLFW_PRESS)
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
        
        saveSettings();
    }
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
        g_animationEnabled = !g_animationEnabled;
        std::cout << "Animation: " << (g_animationEnabled ? "ON" : "OFF") << std::endl;
    }

    if (key == GLFW_KEY_L && action == GLFW_PRESS && lightingSystem)
    {
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

    if (key == GLFW_KEY_F5 && action == GLFW_PRESS) {
        if (cupcakeGame) {
            cupcakeGame->getGameState().active = !cupcakeGame->getGameState().active;
            std::cout << "Cupcake game: " << (cupcakeGame->getGameState().active ? "ACTIVE" : "PAUSED") << std::endl;
        }
    }
    
    // Toggle bike steering mode with B key
    if (key == GLFW_KEY_B && action == GLFW_PRESS) {
        g_bikeSteeringMode = !g_bikeSteeringMode;
        std::cout << "Bike steering mode: " << (g_bikeSteeringMode ? "ON" : "OFF") << std::endl;
        std::cout << "Move mouse left/right to steer the bike" << std::endl;
    }
}

void fbsize_callback(GLFWwindow *window, int width, int height)
{
    g_windowWidth = width;
    g_windowHeight = height;

    glViewport(0, 0, width, height);

    if (height <= 0)
        height = 1; // avoid division by 0
    float ratio = static_cast<float>(width) / height;

    projectionMatrix = glm::perspective(
        glm::radians(fov), // Field of view
        ratio,             // Aspect ratio
        0.1f,              // Near clipping plane
        20000.0f           // Far clipping plane
    );

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
        if (cupcakeGame) {
            cupcakeGame->handleMouseClick(camera.get());
        }
    }
}

void cursor_position_callback(GLFWwindow *window, double xpos, double ypos)
{
    if (!camera)
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

    // Handle camera look direction (unchanged)
    camera->ProcessMouseMovement(static_cast<GLfloat>(xoffset), static_cast<GLfloat>(yoffset));
    
    // Add lateral movement for "bike steering"
    if (cupcakeGame && cupcakeGame->getGameState().active) {
        const float steeringSensitivity = 0.01f; // Adjust this value to change steering responsiveness
        const float maxSteeringSpeed = 15.0f;    // Maximum lateral movement speed
        
        // Calculate lateral movement based on horizontal mouse movement
        float lateralMovement = static_cast<float>(xoffset) * steeringSensitivity;
        lateralMovement = glm::clamp(lateralMovement, -maxSteeringSpeed, maxSteeringSpeed);
        
        // Apply the movement to the camera's right direction (X-axis)
        glm::vec3 rightVector = camera->Right;
        glm::vec3 movement = rightVector * lateralMovement;
        
        // Apply the movement (this will be processed by physics system if available)
        camera->Position += movement;
    }
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
        if (camera)
        {
            // Use multiplicative scaling instead of additive for more predictable behavior
            float speedMultiplier = 1.0f + static_cast<float>(yoffset) * 0.1f;
            camera->MovementSpeed *= speedMultiplier;
            camera->MovementSpeed = glm::clamp(camera->MovementSpeed, 0.5f, 15.0f);
            std::cout << "Camera speed: " << camera->MovementSpeed << std::endl;
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

void init_assets()
{
    my_shader = std::make_unique<ShaderProgram>("resources/shaders/phong.vert", "resources/shaders/phong.frag");
    
    particle_shader = std::make_unique<ShaderProgram>("resources/shaders/particle.vert", "resources/shaders/particle.frag");

    road_shader = std::make_unique<ShaderProgram>("resources/shaders/basic.vert", "resources/shaders/basic.frag");

    lightingSystem = std::make_unique<LightingSystem>();
    
    physicsSystem = std::make_unique<PhysicsSystem>();
    
    g_worldMin = glm::vec3(-100.0f, -5.0f, -300.0f);
    g_worldMax = glm::vec3(100.0f, 100.0f, 100.0f);
    physicsSystem->setWorldBounds(g_worldMin, g_worldMax);
    
    physicsSystem->setWallHitCallback([&](const glm::vec3& hitPoint) {
        if (cupcakeGame && !cupcakeGame->getGameState().active) return;
         static auto lastPrint = std::chrono::high_resolution_clock::now();
         static glm::vec3 lastPos(FLT_MAX);
         auto now = std::chrono::high_resolution_clock::now();
         float secs = std::chrono::duration<float>(now - lastPrint).count();

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

    cupcakeGame = std::make_unique<CupcakeGame>();
    cupcakeGame->initialize();

    initRoadGeometry();

    {
        const std::filesystem::path texPath = "resources/textures/asphalt.jpg";
        try {
            g_roadTex = TextureLoader::textureInit(texPath);
        } catch (const std::exception& e) {
            std::cerr << "Failed to load texture '" << texPath.string() << "': " << e.what() << " - using procedural fallback\n";
            g_roadTex = 0;
        }

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

    std::cout << "Loading basic models..." << std::endl;
    std::vector<std::pair<std::string, std::string>> models = {
        {"cupcake", "resources/objects/12188_Cupcake_v1_L3.obj"},
        {"house", "resources/objects/house.obj"},
        {"bambo_house", "resources/objects/Bambo_House.obj"},
        {"cyprys_house", "resources/objects/Cyprys_House.obj"},
        {"building", "resources/objects/Building,.obj"}
    };
    
    for (const auto& [name, path] : models) {
        try {
            auto start = std::chrono::high_resolution_clock::now();
            std::cout << "Loading " << path << "..." << std::endl;
            
            auto model = std::make_unique<Model>(path, *my_shader);
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            std::cout << "✓ Loaded " << name << " (" << model->meshes.size() 
                      << " meshes) in " << duration.count() << "ms" << std::endl;
            scene[name] = std::move(model);
        } catch (const std::exception& e) {
            std::cerr << "✗ FAILED to load " << name << " from " << path << ": " << e.what() << std::endl;
        }
    }
    
    std::cout << "Scene now contains " << scene.size() << " models:" << std::endl;
    for (const auto& pair : scene) {
        std::cout << "  - " << pair.first << std::endl;
    }

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
    if (GLFWwindow* current = glfwGetCurrentContext()) {
        glfwSetInputMode(current, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
    }

    GLFWwindow* current = glfwGetCurrentContext();
    if (current) {
        glfwSetInputMode(current, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
    }

    // setup initial game state
    cupcakeGame->getGameState().roadSegments.clear();
    for (int i = 0; i < cupcakeGame->getGameState().roadSegmentCount; ++i) {
        cupcakeGame->getGameState().roadSegments.push_back(glm::vec3(0.0f, 0.0f, -static_cast<float>(i) * cupcakeGame->getGameState().roadSegmentLength));
    }

    my_shader->activate();
    my_shader->setUniform("uProj_m", projectionMatrix);
    my_shader->setUniform("uV_m", viewMatrix);
    my_shader->deactivate();
}

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
    
    if (aaSettings.contains("enabled") && aaSettings["enabled"].is_boolean())
    {
        g_antialisingEnabled = aaSettings["enabled"].get<bool>();
    }
    else
    {
        std::cerr << "Warning: Malformed antialiasing 'enabled' setting, using default: false" << std::endl;
        g_antialisingEnabled = false;
    }
    
    if (aaSettings.contains("level") && aaSettings["level"].is_number_integer())
    {
        g_antialiasingLevel = aaSettings["level"].get<int>();
    }
    else
    {
        std::cerr << "Warning: Malformed antialiasing 'level' setting, using default: 4" << std::endl;
        g_antialiasingLevel = 4;
    }
    
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
        nlohmann::json settings = loadSettings();

        validateAntialiasingSettings(settings);

        if (settings["default_resolution"]["x"].is_number_integer() &&
            settings["default_resolution"]["y"].is_number_integer())
        {
            g_windowWidth = settings["default_resolution"]["x"].get<int>();
            g_windowHeight = settings["default_resolution"]["y"].get<int>();
        }

        if (settings["appname"].is_string())
        {
            g_windowTitle = settings["appname"].get<std::string>();
        }

        if (settings.contains("vsync_enabled") && settings["vsync_enabled"].is_boolean())
        {
            g_vSync = settings["vsync_enabled"].get<bool>();
        }

        std::cout << "Application: " << g_windowTitle << std::endl;
        std::cout << "Initial resolution: " << g_windowWidth << "x" << g_windowHeight << std::endl;

        if (!std::filesystem::exists("resources"))
        {
            throw std::runtime_error("Directory 'resources' not found. Various media files are expected to be there.");
        }

        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        glfwSetErrorCallback(error_callback);

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

        if (g_antialisingEnabled)
        {
            glfwWindowHint(GLFW_SAMPLES, g_antialiasingLevel);
            std::cout << "Setting up multisampling with " << g_antialiasingLevel << " samples" << std::endl;
        }

        GLFWwindow *window = glfwCreateWindow(g_windowWidth, g_windowHeight, g_windowTitle.c_str(), NULL, NULL);
        if (!window)
        {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwMakeContextCurrent(window);

        glfwSwapInterval(g_vSync ? 1 : 0);

        GLenum err = glewInit();
        if (err != GLEW_OK)
        {
            throw std::runtime_error(std::string("Failed to initialize GLEW: ") +
                                     reinterpret_cast<const char *>(glewGetErrorString(err)));
        }

        std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
        std::cout << "GLFW Version: " << glfwGetVersionString() << std::endl;
        std::cout << "GLEW Version: " << glewGetString(GLEW_VERSION) << std::endl;
        std::cout << "GLM Version: " << GLM_VERSION_MAJOR << "." << GLM_VERSION_MINOR << "." << GLM_VERSION_PATCH << std::endl;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        
        ImGui::StyleColorsDark();
        
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 460 core");
        
        std::cout << "ImGui initialized successfully" << std::endl;

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

        if (major < 4 || (major == 4 && minor < 6))
        {
            std::cerr << "Warning: OpenGL 4.6 was requested but " << major << "." << minor << " was obtained." << std::endl;
        }

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

        glfwSetKeyCallback(window, key_callback);
        glfwSetFramebufferSizeCallback(window, fbsize_callback);
        glfwSetMouseButtonCallback(window, mouse_button_callback);
        glfwSetCursorPosCallback(window, cursor_position_callback);
        glfwSetScrollCallback(window, scroll_callback); // Initialize OpenGL assets

        // loading screen
        {
            std::string loadingTitle = g_windowTitle + " | Loading...";
            glfwSetWindowTitle(window, loadingTitle.c_str());

            glClearColor(0.04f, 0.05f, 0.08f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
            ImGui::SetNextWindowPos(ImVec2(g_windowWidth * 0.5f, g_windowHeight * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(400.0f, 150.0f), ImGuiCond_Always);
            ImGui::Begin("Loading", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            
            ImGui::Text("Loading Game Assets...");
            ImGui::Separator();
            ImGui::Text("Please wait while we load:");
            ImGui::BulletText("3D Models and Textures");
            ImGui::BulletText("Audio Files");
            ImGui::BulletText("Game Systems");
            ImGui::Separator();
            ImGui::Text("This may take a few moments...");
            
            ImGui::End();
            
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        init_assets();

        glEnable(GL_DEPTH_TEST);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (g_antialisingEnabled)
        {
            glEnable(GL_MULTISAMPLE);
            
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
        }
        auto lastTime = std::chrono::high_resolution_clock::now();
        int frameCount = 0;
        float fps = 0.0f;
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
        std::cout << "- Sound will pan left/right based on triangle's relative position\n\n";
        
        std::cout << "🧁 CUPCAGAME:\n";
        std::cout << "- OBJECTIVE: Deliver cupcakes to houses with glowing indicators\n";
        std::cout << "- CONTROLS: Use mouse to look around, left click to throw cupcakes\n";
        std::cout << "- SCORING: +8 money and +5 happiness for correct deliveries\n";
        std::cout << "- PENALTIES: -3 money and -2 happiness for wrong house deliveries\n";
        std::cout << "- WARNING: -8 happiness for missed deliveries (timeout)\n";
        std::cout << "- COLLISION: Avoid crashing into houses - it's game over!\n";
        std::cout << "- SPEED: Game speed increases over time for more challenge\n";
        std::cout << "- F5: Pause/Resume game\n\n"; // Variables for time-based animation
        auto startTime = std::chrono::high_resolution_clock::now();

        while (!glfwWindowShouldClose(window))
        {
            auto currentTime = std::chrono::high_resolution_clock::now();
            frameCount++;

            float elapsedTime = std::chrono::duration<float>(currentTime - startTime).count(); // Update FPS once per second
            float timeDelta = std::chrono::duration<float>(currentTime - lastTime).count();
            if (timeDelta >= 1.0f)
            {
                fps = frameCount / timeDelta;
                frameCount = 0;
                lastTime = currentTime;

                int houseCount = cupcakeGame ? static_cast<int>(cupcakeGame->getGameState().houses.size()) : 0;
                int money = cupcakeGame ? cupcakeGame->getGameState().money : 0;
                int happiness = cupcakeGame ? cupcakeGame->getGameState().happiness : 0;
                bool gameActive = cupcakeGame ? cupcakeGame->getGameState().active : true;
                float speed = cupcakeGame ? cupcakeGame->getGameState().speed : 0.0f;
                
                std::string title = g_windowTitle + " | FPS: " + std::to_string(static_cast<int>(fps)) +
                                    " | VSync: " + (g_vSync ? "ON" : "OFF") +
                                    " | AA: " + (g_antialisingEnabled ? ("ON(" + std::to_string(g_antialiasingLevel) + "x)") : "OFF") +
                                    " | 🧁 $" + std::to_string(money) + 
                                    " | 😊" + std::to_string(happiness) + "%" +
                                    " | Speed:" + std::to_string(static_cast<int>(speed)) + 
                                    " | Houses:" + std::to_string(houseCount) +
                                    (gameActive ? "" : " | 💥 GAME OVER");
                glfwSetWindowTitle(window, title.c_str());
            }
            if (camera && cupcakeGame && cupcakeGame->getGameState().active)
            {
                static auto lastFrameTime = currentTime;
                float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
                lastFrameTime = currentTime;

                cupcakeGame->update(deltaTime, camera.get(), audioEngine.get(), particleSystem.get(), physicsSystem.get());

                // update view matrix with earthquake shake if active
                glm::mat4 V = camera->GetViewMatrix();
                if (cupcakeGame->getGameState().quakeActive) {
                    // stronger shake effect during earthquake, based on the epicenter
                    float dist = glm::length((camera ? camera->Position : glm::vec3(0.0f)) - cupcakeGame->getGameState().quakeEpicenter);
                    float k = 10.0f;
                    float falloff = 1.0f / (1.0f + dist / k);
                    float t = elapsedTime * 22.0f;
                    float amp = cupcakeGame->getGameState().quakeAmplitude * 1.8f;
                    float ax = sin(t * 1.9f) * amp * falloff;
                    float ay = cos(t * 1.4f) * amp * 0.8f * falloff;
                    V = glm::translate(V, glm::vec3(ax, ay, 0.0f));
                }
                viewMatrix = V;
            }

            audioEngine->setListener(camera->Position, camera->Front, camera->Up);

            {
                glm::vec3 normalSky = glm::vec3(0.55f, 0.70f, 0.95f);
                glm::vec3 quakeSky  = glm::vec3(0.55f, 0.55f, 0.55f);

                static float quakeBlend = 0.0f;
                float target = cupcakeGame->getGameState().quakeActive ? 1.0f : 0.0f;
                float easeInRate = 2.5f;
                float easeOutRate = 1.5f;

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

            static float lastLightingUpdate = 0.0f;
            const float lightingUpdateInterval = 1.0f / 30.0f;
            
            if (lightingSystem && (elapsedTime - lastLightingUpdate) >= lightingUpdateInterval) {
                lightingSystem->updateLights(elapsedTime);
                
                if (camera) {
                    lightingSystem->spotLight.position = camera->Position;
                    lightingSystem->spotLight.direction = camera->Front;
                } else {
                    lightingSystem->spotLight.position = glm::vec3(0.0f, 2.0f, 5.0f);
                    lightingSystem->spotLight.direction = glm::vec3(0.0f, 0.0f, -1.0f);
                }
                
                glm::vec3 viewPos = camera ? camera->Position : glm::vec3(0.0f, 2.0f, 5.0f);
                
                lightingSystem->setupLightUniforms(*my_shader, viewPos);
                
                lastLightingUpdate = elapsedTime;
            }

            my_shader->activate();

            my_shader->setUniform("uV_m", viewMatrix); 

            
            if (scene.find("cupcake") != scene.end())
            {
                glm::vec3 position(-1.5f, 0.0f, -3.0f);
                glm::vec3 rotation(0.0f, g_animationEnabled ? elapsedTime * 8.0f : 0.0f, 0.0f);
                glm::vec3 scale(0.15f, 0.15f, 0.15f); // Smaller scale for cupcake
                
                if (audioEngine && audioEngine->isInitialized() && g_triangleSoundHandle != 0) {
                    audioEngine->setSoundPosition(g_triangleSoundHandle, position);
                }
                
                scene.at("cupcake")->draw(position, rotation, scale);
            }

            if (road_shader && g_roadVAO != 0) {
                road_shader->activate();
                
                road_shader->setUniform("uV_m", viewMatrix);
                road_shader->setUniform("uProj_m", projectionMatrix);
                road_shader->setUniform("uniform_Color", glm::vec4(0.9f, 0.9f, 0.9f, 1.0f)); // Slightly gray for asphalt

                if (g_roadTex != 0) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, g_roadTex);
                    road_shader->setUniform("textureSampler", 0);
                    road_shader->setUniform("useTexture", true);
                } else {
                    road_shader->setUniform("useTexture", false);
                }

                
                glBindVertexArray(g_roadVAO);

                for (const auto& seg : cupcakeGame->getGameState().roadSegments) {
                    glm::vec3 pos(seg.x, -0.02f, seg.z); // Slightly below ground to avoid z-fighting
                    
                    float roadWidth = cupcakeGame->getGameState().roadSegmentWidth;
                    float roadLength = cupcakeGame->getGameState().roadSegmentLength;
                    glm::vec3 scl(roadWidth, 1.0f, roadLength);

                    glm::mat4 model = glm::mat4(1.0f);
                    model = glm::translate(model, pos);
                    model = glm::scale(model, scl);
                    
                    road_shader->setUniform("uM_m", model);
                    
                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                }

                glBindVertexArray(0);
                if (g_roadTex != 0) {
                    glBindTexture(GL_TEXTURE_2D, 0);
                }
                road_shader->deactivate();
            }

            const float cameraZ = camera ? camera->Position.z : 0.0f;
            const float cullingDistance = 120.0f; // render houses within range
            
            for (const auto& h : cupcakeGame->getGameState().houses) {
            if (std::abs(h.position.z - cameraZ) > cullingDistance) {
                continue;
            }
            
            std::string modelName = h.modelName;
            if (modelName.empty()) {
                modelName = "house";
            }
            
            if (scene.find(modelName) != scene.end()) {
                glm::vec3 pos = h.position;
                glm::vec3 rot(0.0f, h.id * 23.0f * (3.14159f / 180.0f), 0.0f);
                
                glm::vec3 scl;
                if (modelName == "bambo_house") {
                    scl = glm::vec3(0.35f, 0.35f, 0.35f);
                } else if (modelName == "cyprys_house") {
                    scl = glm::vec3(0.4f, 0.4f, 0.4f);
                } else if (modelName == "building") {
                    scl = glm::vec3(0.25f, 0.25f, 0.25f);
                } else { // house.obj
                    scl = glm::vec3(0.018f, 0.018f, 0.018f);
                }
                
                scene.at(modelName)->draw(pos, rot, scl);
            }
        
            if (h.requesting) {
                if (scene.find("cupcake") != scene.end()) {
                    glm::vec3 indicatorPos = h.position;
                    indicatorPos.y += h.indicatorHeight;
                    glm::vec3 rot(0.0f, elapsedTime * 60.0f, 0.0f);
                    glm::vec3 scl(0.12f, 0.12f, 0.12f);
                    scene.at("cupcake")->draw(indicatorPos, rot, scl);
                }
            }
        }

        if (scene.find("cupcake") != scene.end()) {
            for (const auto& projectile : cupcakeGame->getGameState().projectiles) {
                if (projectile && projectile->alive) {
                    projectile->draw(scene.at("cupcake").get());
                }
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
                        // Reduced particle emission for better performance
                        particleSystem->emit(4); // Reduced from 18 to 4 particles per frame
                        break;
                    }
                }
            }
            else if (scene.find("house") != scene.end())
            {
                glm::vec3 position(0.0f, g_animationEnabled ? sin(elapsedTime * 0.8f) * 0.15f : 0.0f, -3.0f);
                glm::vec3 rotation(0.0f);
                glm::vec3 scale(0.02f * (g_animationEnabled ? 1.0f + 0.05f * sin(elapsedTime * 1.2f) : 1.0f));
                scene.at("house")->draw(position, rotation, scale);
            }

            if (scene.find("building") != scene.end())
            {
                glm::vec3 position(1.5f, 0.0f, -3.0f);
                glm::vec3 rotation(g_animationEnabled ? elapsedTime * 6.0f : 0.0f,
                                   g_animationEnabled ? elapsedTime * 10.0f : 0.0f, 0.0f);
                glm::vec3 scale(0.03f); // Much smaller scale for building
                scene.at("building")->draw(position, rotation, scale);
            }

            // render sun
            if (scene.find("sphere") != scene.end() && lightingSystem)
            {
                glm::vec3 sunDirection = -lightingSystem->dirLight.direction;
                
                float sunDistance = 100.0f;
                glm::vec3 cameraPos = camera ? camera->Position : glm::vec3(0.0f, 2.0f, 5.0f);
                glm::vec3 sunPosition = cameraPos + sunDirection * sunDistance;
                
                sunPosition.y = glm::max(sunPosition.y, 20.0f);
                
                glm::vec3 sunRotation(0.0f, g_animationEnabled ? elapsedTime * 30.0f : 0.0f, 0.0f); // rotation for glowing
                glm::vec3 sunScale(3.0f);
                
                my_shader->setUniform("material.emission", glm::vec3(2.0f, 1.5f, 0.5f)); // Bright yellow-orange emission
                
                scene.at("sphere")->draw(sunPosition, sunRotation, sunScale);
                
                my_shader->setUniform("material.emission", glm::vec3(0.0f, 0.0f, 0.0f));
            }

            my_shader->deactivate();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
            if (!cupcakeGame->getGameState().active) {
                ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
                ImGui::SetNextWindowSize(ImVec2(static_cast<float>(g_windowWidth), static_cast<float>(g_windowHeight)));
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.7f)); // Semi-transparent black
                ImGui::Begin("GameOverOverlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
                
                float windowWidth = ImGui::GetWindowSize().x;
                float windowHeight = ImGui::GetWindowSize().y;
                
                ImVec2 titleSize = ImGui::CalcTextSize("GAME OVER");
                ImGui::SetCursorPos(ImVec2((windowWidth - titleSize.x) * 0.5f, windowHeight * 0.3f));
                
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f)); // Red text
                ImGui::Text("GAME OVER");
                ImGui::PopStyleColor();
                
                ImGui::SetCursorPos(ImVec2((windowWidth - 350) * 0.5f, windowHeight * 0.45f));
                ImGui::BeginChild("GameStats", ImVec2(350.0f, 180.0f), true);
                ImGui::Text("Final Statistics:");
                ImGui::Separator();
                ImGui::Text("Money Earned: $%d", cupcakeGame->getGameState().money);
                ImGui::Text("Happiness: %d%%", cupcakeGame->getGameState().happiness);
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "Controls:");
                ImGui::Text("Press F5 to restart");
                ImGui::Text("Press ESC to quit");
                ImGui::EndChild();
                
                ImGui::End();
                ImGui::PopStyleColor();
                
                std::string title = g_windowTitle + " | GAME OVER";
                glfwSetWindowTitle(window, title.c_str());
            }
            
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            if (!cupcakeGame->getGameState().active) {
                glEnable(GL_SCISSOR_TEST);
                int rw = static_cast<int>(g_windowWidth * 0.65f);
                int rh = static_cast<int>(g_windowHeight * 0.28f);
                int rx = (g_windowWidth - rw) / 2;
                int ry = (g_windowHeight - rh) / 2;
                glScissor(rx, ry, rw, rh);

                glm::vec3 normalSky = glm::vec3(0.55f, 0.70f, 0.95f);
                glm::vec3 quakeSky  = glm::vec3(0.55f, 0.55f, 0.55f);
                glm::vec3 panel = glm::mix(quakeSky, normalSky, 0.25f) * 0.6f;
                glClearColor(panel.r, panel.g, panel.b, 0.55f);
                glClear(GL_COLOR_BUFFER_BIT);
                glDisable(GL_SCISSOR_TEST);

                std::string title = g_windowTitle + " | GAME OVER";
                glfwSetWindowTitle(window, title.c_str());
            }

            checkGLError("Main Loop");

            glfwSwapBuffers(window);

            glfwPollEvents();
        }

        my_shader->clear();
        if (particle_shader) {
            particle_shader->clear();
        }
        cleanupRoadGeometry();

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

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