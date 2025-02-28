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
#include <opencv2/opencv.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

// Vertex structure definition
struct vertex {
    glm::vec3 position;
};

// Global variables
bool g_vSync = true;
std::string g_windowTitle = "Graphics Application";
int g_windowWidth = 800;
int g_windowHeight = 600;

// GL objects and shader program
GLuint shader_prog_ID = 0;
GLuint VBO_ID = 0;
GLuint VAO_ID = 0;
GLfloat r = 1.0f, g = 0.0f, b = 0.0f, a = 1.0f;

// Triangle vertices
std::vector<vertex> triangle_vertices = {
    {{0.0f,  0.5f,  0.0f}},
    {{0.5f, -0.5f,  0.0f}},
    {{-0.5f, -0.5f,  0.0f}}
};

// Callback functions
void error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    // Toggle VSync with F12
    if (key == GLFW_KEY_F12 && action == GLFW_PRESS) {
        g_vSync = !g_vSync;
        glfwSwapInterval(g_vSync ? 1 : 0);
        std::cout << "VSync: " << (g_vSync ? "ON" : "OFF") << std::endl;
    }

    // Change triangle color with different keys
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        switch (key) {
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
        case GLFW_KEY_A:
            a = (a > 0.5f) ? 0.5f : 1.0f;
            std::cout << "Alpha: " << a << std::endl;
            break;
        }
    }
}

void fbsize_callback(GLFWwindow* window, int width, int height) {
    g_windowWidth = width;
    g_windowHeight = height;
    glViewport(0, 0, width, height);
    std::cout << "Window resized to: " << width << "x" << height << std::endl;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        // Change color on left click
        r = static_cast<float>(rand()) / RAND_MAX;
        g = static_cast<float>(rand()) / RAND_MAX;
        b = static_cast<float>(rand()) / RAND_MAX;
        std::cout << "Random color: R=" << r << " G=" << g << " B=" << b << std::endl;
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    // Just for demonstration purposes
    // std::cout << "Cursor position: " << xpos << ", " << ypos << std::endl;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    // Adjust alpha with scroll
    a += static_cast<float>(yoffset) * 0.1f;
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    std::cout << "Alpha adjusted to: " << a << std::endl;
}

// Debug output callback
void APIENTRY gl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar* message, const void* userParam) {
    std::string sourceStr;
    switch (source) {
    case GL_DEBUG_SOURCE_API: sourceStr = "API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM: sourceStr = "Window System"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceStr = "Shader Compiler"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY: sourceStr = "Third Party"; break;
    case GL_DEBUG_SOURCE_APPLICATION: sourceStr = "Application"; break;
    case GL_DEBUG_SOURCE_OTHER: sourceStr = "Other"; break;
    }

    std::string typeStr;
    switch (type) {
    case GL_DEBUG_TYPE_ERROR: typeStr = "Error"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "Deprecated Behavior"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: typeStr = "Undefined Behavior"; break;
    case GL_DEBUG_TYPE_PORTABILITY: typeStr = "Portability"; break;
    case GL_DEBUG_TYPE_PERFORMANCE: typeStr = "Performance"; break;
    case GL_DEBUG_TYPE_MARKER: typeStr = "Marker"; break;
    case GL_DEBUG_TYPE_PUSH_GROUP: typeStr = "Push Group"; break;
    case GL_DEBUG_TYPE_POP_GROUP: typeStr = "Pop Group"; break;
    case GL_DEBUG_TYPE_OTHER: typeStr = "Other"; break;
    }

    std::string severityStr;
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH: severityStr = "High"; break;
    case GL_DEBUG_SEVERITY_MEDIUM: severityStr = "Medium"; break;
    case GL_DEBUG_SEVERITY_LOW: severityStr = "Low"; break;
    case GL_DEBUG_SEVERITY_NOTIFICATION: return; // Ignore notifications
    default: severityStr = "Unknown"; break;
    }

    std::cerr << "OpenGL Debug - " << sourceStr << ", " << typeStr << ", " << severityStr << ": " << message << std::endl;
}

// Function to check OpenGL errors
void checkGLError(const std::string& location) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::string errorString;
        switch (error) {
        case GL_INVALID_ENUM: errorString = "GL_INVALID_ENUM"; break;
        case GL_INVALID_VALUE: errorString = "GL_INVALID_VALUE"; break;
        case GL_INVALID_OPERATION: errorString = "GL_INVALID_OPERATION"; break;
        case GL_STACK_OVERFLOW: errorString = "GL_STACK_OVERFLOW"; break;
        case GL_STACK_UNDERFLOW: errorString = "GL_STACK_UNDERFLOW"; break;
        case GL_OUT_OF_MEMORY: errorString = "GL_OUT_OF_MEMORY"; break;
        default: errorString = "Unknown error"; break;
        }
        std::cerr << "OpenGL Error at " << location << ": " << errorString << " (" << error << ")" << std::endl;
    }
}

// Function to initialize assets and shaders
void init_assets() {
    //
    // Initialize pipeline: compile, link and use shaders
    //

    // SHADERS - define & compile & link
    const char* vertex_shader =
        "#version 460 core\n"
        "in vec3 attribute_Position;"
        "void main() {"
        "  gl_Position = vec4(attribute_Position, 1.0);"
        "}";

    const char* fragment_shader =
        "#version 460 core\n"
        "uniform vec4 uniform_Color;"
        "out vec4 FragColor;"
        "void main() {"
        "  FragColor = uniform_Color;"
        "}";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertex_shader, NULL);
    glCompileShader(vs);

    // Check for compilation errors
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vs, 512, NULL, infoLog);
        std::cerr << "Vertex shader compilation failed: " << infoLog << std::endl;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragment_shader, NULL);
    glCompileShader(fs);

    // Check for compilation errors
    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fs, 512, NULL, infoLog);
        std::cerr << "Fragment shader compilation failed: " << infoLog << std::endl;
    }

    shader_prog_ID = glCreateProgram();
    glAttachShader(shader_prog_ID, fs);
    glAttachShader(shader_prog_ID, vs);
    glLinkProgram(shader_prog_ID);

    // Check for linking errors
    glGetProgramiv(shader_prog_ID, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shader_prog_ID, 512, NULL, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
    }

    // Now we can delete shader parts
    glDetachShader(shader_prog_ID, fs);
    glDetachShader(shader_prog_ID, vs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // 
    // Create and load data into GPU
    //

    // Create VAO
    glGenVertexArrays(1, &VAO_ID);
    glBindVertexArray(VAO_ID);

    // Create and fill VBO
    glGenBuffers(1, &VBO_ID);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_ID);
    glBufferData(GL_ARRAY_BUFFER, triangle_vertices.size() * sizeof(vertex), triangle_vertices.data(), GL_STATIC_DRAW);

    // Set up vertex attributes
    GLint position_attrib_location = glGetAttribLocation(shader_prog_ID, "attribute_Position");
    glEnableVertexAttribArray(position_attrib_location);
    glVertexAttribPointer(position_attrib_location, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, position));

    // Unbind VAO to prevent accidental modifications
    glBindVertexArray(0);
}

// Function to load settings from JSON
nlohmann::json loadSettings() {
    try {
        std::ifstream settingsFile("app_settings.json");
        if (!settingsFile.is_open()) {
            throw std::runtime_error("Could not open app_settings.json");
        }
        return nlohmann::json::parse(settingsFile);
    }
    catch (const std::exception& e) {
        std::cerr << "Error loading settings: " << e.what() << std::endl;
        std::cerr << "Using default settings instead." << std::endl;

        nlohmann::json defaultSettings;
        defaultSettings["appname"] = "Graphics Application";
        defaultSettings["default_resolution"]["x"] = 800;
        defaultSettings["default_resolution"]["y"] = 600;
        return defaultSettings;
    }
}

int main() {
    try {
        // Load settings from JSON
        nlohmann::json settings = loadSettings();

        // Get window resolution from settings
        if (settings["default_resolution"]["x"].is_number_integer() &&
            settings["default_resolution"]["y"].is_number_integer()) {
            g_windowWidth = settings["default_resolution"]["x"].get<int>();
            g_windowHeight = settings["default_resolution"]["y"].get<int>();
        }

        // Get application name
        if (settings["appname"].is_string()) {
            g_windowTitle = settings["appname"].get<std::string>();
        }

        std::cout << "Application: " << g_windowTitle << std::endl;
        std::cout << "Initial resolution: " << g_windowWidth << "x" << g_windowHeight << std::endl;

        // Check if resources directory exists
        if (!std::filesystem::exists("resources")) {
            throw std::runtime_error("Directory 'resources' not found. Various media files are expected to be there.");
        }

        // Initialize GLFW
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        // Set error callback
        glfwSetErrorCallback(error_callback);

        // Set OpenGL version and profile
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

        // Create window
        GLFWwindow* window = glfwCreateWindow(g_windowWidth, g_windowHeight, g_windowTitle.c_str(), NULL, NULL);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        // Make the window's context current
        glfwMakeContextCurrent(window);

        // Set VSync
        glfwSwapInterval(g_vSync ? 1 : 0);

        // Initialize GLEW
        GLenum err = glewInit();
        if (err != GLEW_OK) {
            throw std::runtime_error(std::string("Failed to initialize GLEW: ") +
                reinterpret_cast<const char*>(glewGetErrorString(err)));
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
        if (profile == GL_CONTEXT_CORE_PROFILE_BIT) {
            std::cout << "OpenGL Profile: Core" << std::endl;
        }
        else {
            std::cout << "OpenGL Profile: Not Core!" << std::endl;
        }

        // Check if requested version was obtained
        if (major < 4 || (major == 4 && minor < 6)) {
            std::cerr << "Warning: OpenGL 4.6 was requested but " << major << "." << minor << " was obtained." << std::endl;
        }

        // Check debug output extension
        if (GLEW_ARB_debug_output) {
            std::cout << "Debug output extension is supported." << std::endl;
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(gl_debug_callback, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        }
        else {
            std::cout << "Debug output extension is not supported." << std::endl;
        }

        // Set up callbacks
        glfwSetKeyCallback(window, key_callback);
        glfwSetFramebufferSizeCallback(window, fbsize_callback);
        glfwSetMouseButtonCallback(window, mouse_button_callback);
        glfwSetCursorPosCallback(window, cursor_position_callback);
        glfwSetScrollCallback(window, scroll_callback);

        // Initialize OpenGL assets
        init_assets();

        // OpenCV test (just to verify it works)
        cv::Mat image = cv::imread("resources/lightbulb.jpg");
        if (!image.empty()) {
            std::cout << "Image loaded successfully. Size: " << image.cols << "x" << image.rows << std::endl;
        }
        else {
            std::cerr << "Failed to load image." << std::endl;
        }

        // Enable alpha blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Variables for FPS calculation
        auto lastTime = std::chrono::high_resolution_clock::now();
        int frameCount = 0;
        float fps = 0.0f;

        // Get uniform location for color
        glUseProgram(shader_prog_ID);
        GLint uniform_color_location = glGetUniformLocation(shader_prog_ID, "uniform_Color");
        if (uniform_color_location == -1) {
            std::cerr << "Uniform location is not found in active shader program." << std::endl;
        }

        // Print controls info
        std::cout << "\nControls:\n";
        std::cout << "R, G, B keys - Toggle red, green, blue components\n";
        std::cout << "Left mouse click - Set random color\n";
        std::cout << "Mouse wheel - Adjust alpha/transparency\n";
        std::cout << "F12 - Toggle VSync\n";
        std::cout << "ESC - Exit\n\n";

        // Main rendering loop
        while (!glfwWindowShouldClose(window)) {
            // Calculate FPS
            auto currentTime = std::chrono::high_resolution_clock::now();
            frameCount++;

            // Update FPS once per second
            float timeDelta = std::chrono::duration<float>(currentTime - lastTime).count();
            if (timeDelta >= 1.0f) {
                fps = frameCount / timeDelta;
                frameCount = 0;
                lastTime = currentTime;

                // Update window title with FPS and VSync status
                std::string title = g_windowTitle + " | FPS: " + std::to_string(static_cast<int>(fps)) +
                    " | VSync: " + (g_vSync ? "ON" : "OFF");
                glfwSetWindowTitle(window, title.c_str());
            }

            // Clear the screen
            glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            // Render triangle
            glUseProgram(shader_prog_ID);
            glUniform4f(uniform_color_location, r, g, b, a);
            glBindVertexArray(VAO_ID);
            glDrawArrays(GL_TRIANGLES, 0, triangle_vertices.size());
            glBindVertexArray(0);

            // Check for errors
            checkGLError("Main Loop");

            // Swap front and back buffers
            glfwSwapBuffers(window);

            // Poll for and process events
            glfwPollEvents();
        }

        // Clean up GL resources
        glDeleteProgram(shader_prog_ID);
        glDeleteBuffers(1, &VBO_ID);
        glDeleteVertexArrays(1, &VAO_ID);

        // Clean up
        glfwDestroyWindow(window);
        glfwTerminate();

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        glfwTerminate();
        return -1;
    }
}