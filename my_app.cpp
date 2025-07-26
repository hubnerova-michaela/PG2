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
std::unordered_map<std::string, std::unique_ptr<Model>> scene;
GLfloat r = 1.0f, g = 0.0f, b = 0.0f, a = 1.0f;

// Transparent objects management
std::vector<TransparentObject> transparentObjects;

// Animation control
bool g_animationEnabled = true;

// Transformation matrices
glm::mat4 projectionMatrix{1.0f};
glm::mat4 viewMatrix{1.0f};
float fov = 60.0f; // Field of view in degrees

// Camera system
std::unique_ptr<Camera> camera;
bool firstMouse = true;
double lastX = 400, lastY = 300; // Initial mouse position (center of 800x600 window)
bool cameraEnabled = false;      // Toggle camera control mode

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

    // Toggle camera control with C
    if (key == GLFW_KEY_C && action == GLFW_PRESS)
    {
        cameraEnabled = !cameraEnabled;
        if (cameraEnabled)
        {
            // Capture cursor for camera movement
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true; // Reset mouse to avoid sudden camera jump
        }
        else
        {
            // Release cursor
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        std::cout << "Camera control: " << (cameraEnabled ? "ON (WASD + mouse)" : "OFF") << std::endl;
    }

    // Reset camera speed with V key
    if (key == GLFW_KEY_V && action == GLFW_PRESS && camera)
    {
        camera->MovementSpeed = 2.5f;
        std::cout << "Camera speed reset to: " << camera->MovementSpeed << std::endl;
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
        // Change color on left click
        r = static_cast<float>(rand()) / RAND_MAX;
        g = static_cast<float>(rand()) / RAND_MAX;
        b = static_cast<float>(rand()) / RAND_MAX;
        std::cout << "Random color: R=" << r << " G=" << g << " B=" << b << std::endl;
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
    // Notice: code massively simplified - all moved to specific classes    // shader: load, compile, link, initialize params
    my_shader = std::make_unique<ShaderProgram>("resources/shaders/basic.vert", "resources/shaders/basic.frag");

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
        std::cout << "Left mouse click - Set random color\n";
        std::cout << "Mouse wheel - Adjust alpha/transparency (or camera speed when camera mode is on)\n";
        std::cout << "+/- keys - Increase/decrease antialiasing level (2x, 4x, 8x)\n";
        std::cout << "F11 - Toggle antialiasing (requires restart)\n";
        std::cout << "F12 - Toggle VSync\n";
        std::cout << "ESC - Exit\n\n";
        std::cout << "Camera Controls (when enabled with C key):\n";
        std::cout << "WASD - Move forward/back/left/right\n";
        std::cout << "Q/E - Move down/up\n";
        std::cout << "Mouse - Look around (cursor will be captured)\n";
        std::cout << "Mouse wheel - Adjust movement speed\n";
        std::cout << "V - Reset camera speed to default\n\n";

        std::cout << "Transformation Demo:\n";
        std::cout << "- Left object: Simple Y-axis rotation\n";
        std::cout << "- Center object: Translation (up/down) + Scaling\n";
        std::cout << "- Right object: Multi-axis rotation\n\n";

        std::cout << "Transparency Demo:\n";
        std::cout << "- 3 semi-transparent objects with different alpha values\n";
        std::cout << "- Red (alpha 0.7), Green (alpha 0.5), Blue (alpha 0.3)\n";
        std::cout << "- Objects are depth-sorted for correct transparency rendering\n\n"; // Variables for time-based animation
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
            if (cameraEnabled && camera)
            {
                // Calculate delta time for smooth camera movement
                static auto lastFrameTime = currentTime;
                float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
                lastFrameTime = currentTime;

                // Process camera movement input
                glm::vec3 movement = camera->ProcessInput(window, deltaTime);
                if (glm::length(movement) > 0.001f) // Only print if there's significant movement
                {
                    static int debugCounter = 0;
                    if (++debugCounter % 60 == 0) // Print debug info once per second at 60fps
                    {
                        std::cout << "Camera - Speed: " << camera->MovementSpeed
                                  << ", Movement: (" << movement.x << ", " << movement.y << ", " << movement.z << ")"
                                  << ", DeltaTime: " << deltaTime << std::endl;
                    }
                }
                camera->Position += movement;

                // Update view matrix from camera
                viewMatrix = camera->GetViewMatrix();
            }

            // Clear the screen
            glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

            // STEP 1: Render opaque objects first
            my_shader->activate();
            my_shader->setUniform("uniform_Color", glm::vec4(r, g, b, a));

            // Set view matrix (updated each frame for camera movement)
            my_shader->setUniform("uV_m", viewMatrix); 

            // Triangle: Very subtle rotation around Y-axis (left object)
            if (scene.find("triangle") != scene.end())
            {
                glm::vec3 position(-1.5f, 0.0f, -3.0f);
                glm::vec3 rotation(0.0f, g_animationEnabled ? elapsedTime * 8.0f : 0.0f, 0.0f); // Very slow Y rotation
                glm::vec3 scale(1.0f);                                                          // No scaling
                scene.at("triangle")->draw(position, rotation, scale);
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
                    
                    my_shader->setUniform("uniform_Color", obj.color);
                    
                    // Use triangle model for transparent objects (could be any model)
                    if (scene.find("triangle") != scene.end())
                    {
                        scene.at("triangle")->draw(animatedPos, animatedRot, obj.scale);
                    }
                }
            }

            my_shader->deactivate();

            // Check for errors
            checkGLError("Main Loop");

            // Swap front and back buffers
            glfwSwapBuffers(window);

            // Poll for and process events
            glfwPollEvents();
        }

        // Clean up GL resources
        my_shader->clear();

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