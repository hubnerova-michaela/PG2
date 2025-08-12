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

static GLuint g_road_vao = 0;
static GLuint g_road_vbo = 0;
static GLuint g_road_ebo = 0;

// const std::vector<std::string> HOUSE_MODELS = { "bambo_house", "cyprys_house", "building" };
const std::vector<std::string> HOUSE_MODELS = {"bambo_house"};

void spawn_new_houses(CupcakeGame &game, PhysicsSystem &physics, float zPosition)
{
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    const float house_side_offset = 10.0f;
    const float empty_plot_probability = 0.2f;

    // leva strana
    if (dist(rng) > empty_plot_probability)
    {
        House house;
        house.id = game.get_game_state().next_house_id++;
        house.modelName = HOUSE_MODELS[rng() % HOUSE_MODELS.size()]; // nahodny dum
        house.position = glm::vec3(-house_side_offset - 10.0f, 0.0f, zPosition);
        house.half_extents = game.get_house_extents(house.modelName);
        house.indicator_height = game.get_indicator_height(house.modelName);

        game.get_game_state().houses.push_back(house);
        physics.addCollisionObject({CollisionType::BOX, house.position, house.half_extents});
    }

    // prava strana
    if (dist(rng) > empty_plot_probability)
    {
        House house;
        house.id = game.get_game_state().next_house_id++;
        house.modelName = HOUSE_MODELS[rng() % HOUSE_MODELS.size()]; // nahodny dum
        house.position = glm::vec3(house_side_offset, 0.0f, zPosition);
        house.half_extents = game.get_house_extents(house.modelName);
        house.indicator_height = game.get_indicator_height(house.modelName);

        game.get_game_state().houses.push_back(house);
        physics.addCollisionObject({CollisionType::BOX, house.position, house.half_extents});
    }
}

void initRoadGeometry()
{
    float road_vertices[] = {
        -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, // bottom left
        1.0f, 0.0f, -1.0f, 2.0f, 0.0f,  // bottom right (increased U coord for tiling)
        1.0f, 0.0f, 1.0f, 2.0f, 2.0f,   // top right
        -1.0f, 0.0f, 1.0f, 0.0f, 2.0f   // top left
    };

    unsigned int road_indices[] = {
        0, 1, 2, // prvni trojuhelnik
        0, 2, 3  // druhy trojuhelnik
    };

    glGenVertexArrays(1, &g_road_vao);
    glGenBuffers(1, &g_road_vbo);
    glGenBuffers(1, &g_road_ebo);

    glBindVertexArray(g_road_vao);

    glBindBuffer(GL_ARRAY_BUFFER, g_road_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(road_vertices), road_vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_road_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(road_indices), road_indices, GL_STATIC_DRAW);

    // pozice
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    // textura
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void cleanupRoadGeometry()
{
    if (g_road_vao != 0)
    {
        glDeleteVertexArrays(1, &g_road_vao);
        g_road_vao = 0;
    }
    if (g_road_vbo != 0)
    {
        glDeleteBuffers(1, &g_road_vbo);
        g_road_vbo = 0;
    }
    if (g_road_ebo != 0)
    {
        glDeleteBuffers(1, &g_road_ebo);
        g_road_ebo = 0;
    }
}

// KONFIGURACE

void saveSettings();

bool g_vSync = true;
std::string g_windowTitle = "Cupcagame";
int g_window_width = 800;
int g_window_height = 600;
bool g_aa_enabled = false;
int g_aa_level = 4;

// INCLUDY

std::unique_ptr<ShaderProgram> phong_shader;
std::unique_ptr<ShaderProgram> particle_shader;
std::unique_ptr<ShaderProgram> road_shader;
std::unique_ptr<LightingSystem> lightning_system;
std::unique_ptr<ParticleSystem> particle_system;
std::unique_ptr<PhysicsSystem> physics_system;
std::unique_ptr<AudioEngine> audio_engine;

static unsigned int g_quake_sound_handle = 0;
static bool g_quake_sound_playing = false;
static unsigned int g_ambient_sound_handle = 0;

// VELIKOST MAPY
static glm::vec3 g_world_min(-100.0f, -5.0f, -100.0f);
static glm::vec3 g_world_max(100.0f, 100.0f, 100.0f);

std::unordered_map<std::string, std::unique_ptr<Model>> scene;
GLfloat r = 1.0f, g = 0.0f, b = 0.0f, a = 1.0f;

static GLuint g_roadTex = 0;
static GLint g_roadTexSamplerLoc = -1;
static GLint g_roadTilingLoc = -1;

std::vector<TransparentObject> transparentObjects;

std::unique_ptr<CupcakeGame> cupcagame;

bool g_animationEnabled = true;

glm::mat4 pm{1.0f};
glm::mat4 vm{1.0f};
float fov = 60.0f;

std::unique_ptr<Camera> camera;
bool firstMouse = true;
double lastX = 400, lastY = 300;

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

    particles_key_callback(window, key, scancode, action, mods, particle_system.get(), physics_system.get());

    if (key == GLFW_KEY_F10 && action == GLFW_PRESS)
    {
        g_vSync = !g_vSync;
        glfwSwapInterval(g_vSync ? 1 : 0);
        std::cout << "VSync: " << (g_vSync ? "ON" : "OFF") << std::endl;

        saveSettings();
    }

    if (key == GLFW_KEY_F11 && action == GLFW_PRESS)
    {
        g_aa_enabled = !g_aa_enabled;
        std::cout << "AA switch: " << (g_aa_enabled ? "ON" : "OFF") << std::endl;
        std::cout << "Level: " << g_aa_level << "x" << std::endl;

        saveSettings();
    }

    if (key == GLFW_KEY_L && action == GLFW_PRESS && lightning_system)
    {
        static bool spotLightOn = true;
        spotLightOn = !spotLightOn;
        if (spotLightOn)
        {
            lightning_system->spotLight.diffuse = glm::vec3(1.0f, 1.0f, 1.0f);
            lightning_system->spotLight.specular = glm::vec3(1.0f, 1.0f, 1.0f);
        }
        else
        {
            lightning_system->spotLight.diffuse = glm::vec3(0.0f, 0.0f, 0.0f);
            lightning_system->spotLight.specular = glm::vec3(0.0f, 0.0f, 0.0f);
        }
        std::cout << "Svetlo: " << (spotLightOn ? "ON" : "OFF") << std::endl;
    }

    if (key == GLFW_KEY_F5 && action == GLFW_PRESS)
    {
        if (cupcagame)
        {
            if (cupcagame->is_game_over())
            {
                // Restart game if it's game over
                cupcagame->restart_game();

                // Also respawn houses
                cupcagame->get_game_state().houses.clear();
                if (physics_system)
                {
                    physics_system->clearCollisionObjects();
                }

                float z = -15.0f;
                for (int i = 0; i < 20; ++i)
                {
                    spawn_new_houses(*cupcagame, *physics_system, z);
                    z -= cupcagame->get_game_state().house_spacing;
                }

                std::cout << "Hra restartovana!" << std::endl;
            }
            else
            {
                // Normal pause/unpause
                cupcagame->get_game_state().active = !cupcagame->get_game_state().active;
                std::cout << "Cupcagame: " << (cupcagame->get_game_state().active ? "Aktivni" : "Pauznuty") << std::endl;
            }
        }
    }
}

void window_resize_callback(GLFWwindow *window, int width, int height)
{
    g_window_width = width;
    g_window_height = height;

    glViewport(0, 0, width, height);

    if (height <= 0)
        height = 1;
    float ratio = static_cast<float>(width) / height;

    pm = glm::perspective(
        glm::radians(fov), // Field of view
        ratio,             // Aspect ratio
        0.1f,              // Near clipping plane
        20000.0f           // Far clipping plane
    );

    if (phong_shader)
    {
        phong_shader->activate();
        phong_shader->setUniform("uProj_m", pm);
        phong_shader->deactivate();
    }

    std::cout << "Window resized to: " << width << "x" << height << std::endl;
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        if (cupcagame)
        {
            cupcagame->handle_mouse_click(camera.get());
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
    double yoffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    camera->ProcessMouseMovement(static_cast<GLfloat>(xoffset), static_cast<GLfloat>(yoffset));

    if (cupcagame && cupcagame->get_game_state().active)
    {
        const float steeringSensitivity = 0.01f;
        const float maxSteeringSpeed = 15.0f;

        float lateralMovement = static_cast<float>(xoffset) * steeringSensitivity;
        lateralMovement = glm::clamp(lateralMovement, -maxSteeringSpeed, maxSteeringSpeed);

        glm::vec3 rightVector = camera->Right;
        glm::vec3 movement = rightVector * lateralMovement;

        camera->Position += movement;
    }
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    if (camera)
    {
        float speedMultiplier = 1.0f + static_cast<float>(yoffset) * 0.1f;
        camera->MovementSpeed *= speedMultiplier;
        camera->MovementSpeed = glm::clamp(camera->MovementSpeed, 0.5f, 15.0f);
        std::cout << "Camera speed: " << camera->MovementSpeed << std::endl;
    }
}

void init_assets()
{
    phong_shader = std::make_unique<ShaderProgram>("resources/shaders/phong.vert", "resources/shaders/phong.frag");
    particle_shader = std::make_unique<ShaderProgram>("resources/shaders/particle.vert", "resources/shaders/particle.frag");
    road_shader = std::make_unique<ShaderProgram>("resources/shaders/basic.vert", "resources/shaders/basic.frag");

    lightning_system = std::make_unique<LightingSystem>();
    physics_system = std::make_unique<PhysicsSystem>();

    g_world_min = glm::vec3(-100.0f, -5.0f, -300.0f);
    g_world_max = glm::vec3(100.0f, 100.0f, 100.0f);
    physics_system->setWorldBounds(g_world_min, g_world_max);

    physics_system->setWallHitCallback([&](const glm::vec3 &hitPoint)
                                       {
            if (cupcagame && !cupcagame->get_game_state().active) return;
            static auto lastPrint = std::chrono::high_resolution_clock::now();
            static glm::vec3 lastPos(FLT_MAX);
            auto now = std::chrono::high_resolution_clock::now();
            float secs = std::chrono::duration<float>(now - lastPrint).count();

            if (secs > 0.5f || glm::length(hitPoint - lastPos) > 0.75f) {
                std::cout << "Wall hit at: (" << hitPoint.x << ", " << hitPoint.y << ", " << hitPoint.z << ")" << std::endl;
                lastPrint = now;
                lastPos = hitPoint;
            } });

    physics_system->setObjectHitCallback([](const glm::vec3 &hitPoint)
                                         { std::cout << "Object hit at: (" << hitPoint.x << ", " << hitPoint.y << ", " << hitPoint.z << ")" << std::endl; });

    particle_system = std::make_unique<ParticleSystem>(*particle_shader, 1000);
    particle_system->set_emitter_position(glm::vec3(0.0f, 10.0f, -5.0f));

    audio_engine = std::make_unique<AudioEngine>();

    cupcagame = std::make_unique<CupcakeGame>();
    cupcagame->initialize();

    initRoadGeometry();

    {
        const std::filesystem::path texPath = "resources/textures/asphalt.jpg";
        try
        {
            g_roadTex = TextureLoader::textureInit(texPath);
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error("Nepodarilo se nacist texturu '" + texPath.string() + "': " + e.what());
            g_roadTex = 0;
        }
    }

    if (audio_engine->init())
    {
        std::cout << "Audio engine inicializovan" << std::endl;
        glm::vec3 quadPosition(0.0f, 0.0f, -3.0f);
        if (audio_engine->playLoop3D("resources/audio/818034__boatlanman__slow-ethereal-piano-loop-80bpm.wav", quadPosition, &g_ambient_sound_handle))
        {
            std::cout << "Hudba zacala na: " << quadPosition.x << ", " << quadPosition.y << ", " << quadPosition.z << std::endl;
        }
    }
    else
    {
        std::cout << "Nepodarilo se nacist audio engine" << std::endl;
    }

    std::cout << "Nacitani modelu..." << std::endl;
    std::vector<std::pair<std::string, std::string>> models = {
        {"cupcake", "resources/objects/12188_Cupcake_v1_L3.obj"},
        {"bambo_house", "resources/objects/Bambo_House.obj"},
        {"cyprys_house", "resources/objects/Cyprys_House.obj"},
        {"building", "resources/objects/Building,.obj"}};

    for (const auto &[name, path] : models)
    {
        try
        {
            auto start = std::chrono::high_resolution_clock::now();
            std::cout << "Nacitani " << path << "..." << std::endl;

            auto model = std::make_unique<Model>(path, *phong_shader);

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            std::cout << "Nacetlo se " << name << " (" << model->meshes.size()
                      << " meshu) v " << duration.count() << "ms" << std::endl;
            scene[name] = std::move(model);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Nepodarilo se nacist" << name << " z " << path << ": " << e.what() << std::endl;
        }
    }

    std::cout << "Scena obsahuje " << scene.size() << " modely:" << std::endl;
    for (const auto &pair : scene)
    {
        std::cout << "  - " << pair.first << std::endl;
    }

    if (g_window_height <= 0)
        g_window_height = 1; // avoid division by 0
    float ratio = static_cast<float>(g_window_width) / g_window_height;

    pm = glm::perspective(
        glm::radians(fov), // field of view
        ratio,             // aspect ratio
        0.1f,              // near clipping plane
        20000.0f           // far clipping plane
    );
    vm = glm::lookAt(
        glm::vec3(0.0f, 2.0f, 5.0f),  // pozice kamery
        glm::vec3(0.0f, 0.0f, -2.0f), // stred
        glm::vec3(0.0f, 1.0f, 0.0f)   // up vektor
    );

    // inicializace kamery na stejne pozici jako viewMatrix
    camera = std::make_unique<Camera>(glm::vec3(0.0f, 2.0f, 5.0f));
    camera->MovementSpeed = 2.5f;
    camera->MouseSensitivity = 0.1f;
    if (GLFWwindow *current = glfwGetCurrentContext())
    {
        glfwSetInputMode(current, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
    }

    GLFWwindow *current = glfwGetCurrentContext();
    if (current)
    {
        glfwSetInputMode(current, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
    }

    cupcagame->get_game_state().road_segments.clear();
    for (int i = 0; i < cupcagame->get_game_state().road_segment_count; ++i)
    {
        cupcagame->get_game_state().road_segments.push_back(glm::vec3(0.0f, 0.0f, -static_cast<float>(i) * cupcagame->get_game_state().road_segment_length));
    }

    phong_shader->activate();
    phong_shader->setUniform("uProj_m", pm);
    phong_shader->setUniform("uV_m", vm);
    phong_shader->deactivate();
}

nlohmann::json load_settings()
{
    try
    {
        std::ifstream settingsFile("app_settings.json");
        if (!settingsFile.is_open())
        {
            throw std::runtime_error("Nepodarilo se nacist app_settings.json");
        }
        return nlohmann::json::parse(settingsFile);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Chyba pri nacitani: " << e.what() << std::endl;
        std::cerr << "Pouziti vychozich nastaveni..." << std::endl;

        nlohmann::json default_settings;
        default_settings["appname"] = "Cupcagame";
        default_settings["default_resolution"]["x"] = 800;
        default_settings["default_resolution"]["y"] = 600;
        default_settings["antialiasing"]["enabled"] = false;
        default_settings["antialiasing"]["level"] = 4;
        return default_settings;
    }
}

void validate_aa_settings(const nlohmann::json &settings)
{
    bool has_aa = settings.contains("antialiasing");
    if (!has_aa)
    {
        std::cout << "Nenalezena nastaveni AA, pouziva se vychozi: vypnuto" << std::endl;
        g_aa_enabled = false;
        g_aa_level = 4;
        return;
    }

    const auto &aaSettings = settings["antialiasing"];

    if (aaSettings.contains("enabled") && aaSettings["enabled"].is_boolean())
    {
        g_aa_enabled = aaSettings["enabled"].get<bool>();
    }
    else
    {
        std::cerr << "Varovani: Chybne nastaveni AA 'enabled', pouziva se vychozi: false" << std::endl;
        g_aa_enabled = false;
    }

    if (aaSettings.contains("level") && aaSettings["level"].is_number_integer())
    {
        g_aa_level = aaSettings["level"].get<int>();
    }
    else
    {
        std::cerr << "Varovani: Chybne nastaveni AA 'level', pouziva se vychozi: 4" << std::endl;
        g_aa_level = 4;
    }

    if (g_aa_enabled && g_aa_level <= 1)
    {
        std::cerr << "Varovani: AA je zapnut, ale uroven je " << g_aa_level
                  << " (ma byt > 1). AA bude vypnut." << std::endl;
        g_aa_enabled = false;
    }

    if (g_aa_level > 8)
    {
        std::cerr << "Varovani: Uroven AA " << g_aa_level
                  << " je prilis vysoka (maximalne doporuceno: 8). Nastavuji na 8." << std::endl;
        g_aa_level = 8;
    }

    std::cout << "AA: " << (g_aa_enabled ? "zapnuto" : "vypnuto")
              << ", Uroven: " << g_aa_level << std::endl;
}

void saveSettings()
{
    try
    {
        nlohmann::json settings;
        settings["appname"] = g_windowTitle;
        settings["default_resolution"]["x"] = g_window_width;
        settings["default_resolution"]["y"] = g_window_height;
        settings["vsync_enabled"] = g_vSync;
        settings["debug_mode"] = true;
        settings["antialiasing"]["enabled"] = g_aa_enabled;
        settings["antialiasing"]["level"] = g_aa_level;

        std::ofstream settingsFile("app_settings.json");
        if (settingsFile.is_open())
        {
            settingsFile << settings.dump(2) << std::endl;
            settingsFile.close();
            std::cout << "Nastaveni ulozeno do app_settings.json" << std::endl;
        }
        else
        {
            std::cerr << "Nepodarilo se ulozit nastaveni do app_settings.json" << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Chyba pri ukladani nastaveni: " << e.what() << std::endl;
    }
}

int main()
{
    try
    {
        nlohmann::json settings = load_settings();

        validate_aa_settings(settings);

        if (settings["default_resolution"]["x"].is_number_integer() &&
            settings["default_resolution"]["y"].is_number_integer())
        {
            g_window_width = settings["default_resolution"]["x"].get<int>();
            g_window_height = settings["default_resolution"]["y"].get<int>();
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
        std::cout << "Initial resolution: " << g_window_width << "x" << g_window_height << std::endl;

        if (!glfwInit())
        {
            throw std::runtime_error("Nepodarilo se nacist GLFW");
        }

        glfwSetErrorCallback(error_callback);

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

        if (g_aa_enabled)
        {
            glfwWindowHint(GLFW_SAMPLES, g_aa_level);
            std::cout << "Multisampling s " << g_aa_level << " samply" << std::endl;
        }

        GLFWwindow *window = glfwCreateWindow(g_window_width, g_window_height, g_windowTitle.c_str(), NULL, NULL);
        if (!window)
        {
            glfwTerminate();
            throw std::runtime_error("Chyba pri vytvareni GLFW okna");
        }

        glfwMakeContextCurrent(window);

        glfwSwapInterval(g_vSync ? 1 : 0);

        GLenum err = glewInit();
        if (err != GLEW_OK)
        {
            throw std::runtime_error(std::string("chyba pri inicializaci GLEW: ") +
                                     reinterpret_cast<const char *>(glewGetErrorString(err)));
        }

        std::cout << "OpenGL verze: " << glGetString(GL_VERSION) << std::endl;
        std::cout << "GLFW verze: " << glfwGetVersionString() << std::endl;
        std::cout << "GLEW verze: " << glewGetString(GLEW_VERSION) << std::endl;
        std::cout << "GLM verze: " << GLM_VERSION_MAJOR << "." << GLM_VERSION_MINOR << "." << GLM_VERSION_PATCH << std::endl;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 460 core");

        std::cout << "ImGui inicializovano" << std::endl;

        int major, minor, profile;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);

        std::cout << "OpenGL Context verze: " << major << "." << minor << std::endl;
        if (profile == GL_CONTEXT_CORE_PROFILE_BIT)
        {
            std::cout << "OpenGL profil: Core" << std::endl;
        }
        else
        {
            std::cout << "OpenGL profil: neni Core!!!" << std::endl;
        }

        if (GLEW_ARB_debug_output)
        {
            std::cout << "Debug rozsireni povoleno." << std::endl;
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        }
        else
        {
            std::cout << "Debug rozsireni nepovoleno." << std::endl;
        }

        glfwSetKeyCallback(window, key_callback);
        glfwSetFramebufferSizeCallback(window, window_resize_callback);
        glfwSetMouseButtonCallback(window, mouse_button_callback);
        glfwSetCursorPosCallback(window, cursor_position_callback);
        glfwSetScrollCallback(window, scroll_callback);

        // loading screen
        {
            std::string loadingTitle = g_windowTitle + " | Loading...";
            glfwSetWindowTitle(window, loadingTitle.c_str());

            glClearColor(0.04f, 0.05f, 0.08f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::SetNextWindowPos(ImVec2(g_window_width * 0.5f, g_window_height * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(600.0f, 150.0f), ImGuiCond_Always);
            ImGui::Begin("Nacitani", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

            ImGui::Text("Nacitani Cupcagame...");
            ImGui::Separator();
            ImGui::Text("A jak hrat? Strilej cupcaky na domy, ktere je chteji...");
            ImGui::Text("Mas dva ukazatele: penize a spokojenost.");
            ImGui::Text("Kdyz budes strilet cupcaky na vsechny domy, dojdou ti penize.");
            ImGui::Text("Kdyz nebudes strilet cupcaky na domy, ktere je chteji, dojde ti spokojenost.");
            ImGui::Separator();
            ImGui::Text("A nezapomen na to, ze cupcake je nejlepsi!");

            ImGui::End();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        init_assets();

        {
            float z = -15.0f;
            for (int i = 0; i < 20; ++i)
            {
                spawn_new_houses(*cupcagame, *physics_system, z);
                z -= cupcagame->get_game_state().house_spacing;
            }
        }

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        auto lastTime = std::chrono::high_resolution_clock::now();
        int frameCount = 0;
        float fps = 0.0f;
        auto startTime = std::chrono::high_resolution_clock::now();

        while (!glfwWindowShouldClose(window))
        {
            auto currentTime = std::chrono::high_resolution_clock::now();
            frameCount++;

            // aktualizace FPS za 100 ms
            float elapsedTime = std::chrono::duration<float>(currentTime - startTime).count();
            float timeDelta = std::chrono::duration<float>(currentTime - lastTime).count();
            if (timeDelta >= 0.1f)
            {
                fps = frameCount / timeDelta;
                frameCount = 0;
                lastTime = currentTime;

                int houseCount = cupcagame ? static_cast<int>(cupcagame->get_game_state().houses.size()) : 0;
                int money = cupcagame ? cupcagame->get_game_state().money : 0;
                int happiness = cupcagame ? cupcagame->get_game_state().happiness : 0;
                bool gameActive = cupcagame ? cupcagame->get_game_state().active : true;
                float speed = cupcagame ? cupcagame->get_game_state().speed : 0.0f;

                std::string title = g_windowTitle + " | FPS: " + std::to_string(static_cast<int>(fps)) +
                                    " | VSync: " + (g_vSync ? "ON" : "OFF") +
                                    " | AA: " + (g_aa_enabled ? ("ON(" + std::to_string(g_aa_level) + "x)") : "OFF") +
                                    " | 🧁 $" + std::to_string(money) +
                                    " | 😊" + std::to_string(happiness) + "%" +
                                    " | Speed:" + std::to_string(static_cast<int>(speed)) +
                                    " | Houses:" + std::to_string(houseCount) +
                                    (gameActive ? "" : " | 💥 GAME OVER");
                glfwSetWindowTitle(window, title.c_str());
            }
            if (camera && cupcagame && cupcagame->get_game_state().active)
            {
                static auto lastFrameTime = currentTime;
                float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
                lastFrameTime = currentTime;

                cupcagame->update(deltaTime, camera.get(), audio_engine.get(), particle_system.get(), physics_system.get());

                {
                    float farthestZ = camera->Position.z;
                    for (const auto &h : cupcagame->get_game_state().houses)
                    {
                        farthestZ = std::min(farthestZ, h.position.z);
                    }

                    // generovani novych domu, pokud je kamera blizko k nejzazsimu domu
                    if (camera->Position.z - farthestZ < 100.0f)
                    {
                        float newZ = farthestZ - cupcagame->get_game_state().house_spacing;
                        spawn_new_houses(*cupcagame, *physics_system, newZ);
                    }
                }

                // aktualizace view matice se zemetresenim
                glm::mat4 V = camera->GetViewMatrix();
                if (cupcagame->get_game_state().quake_active)
                {
                    // efekt zemetreseni, zameren podle epicentra zemetreseni
                    float dist = glm::length((camera ? camera->Position : glm::vec3(0.0f)) - cupcagame->get_game_state().quake_epicenter);
                    float k = 10.0f;
                    float falloff = 1.0f / (1.0f + dist / k);
                    float t = elapsedTime * 22.0f;
                    float amp = cupcagame->get_game_state().quake_amplitude * 1.8f;
                    float ax = sin(t * 1.9f) * amp * falloff;
                    float ay = cos(t * 1.4f) * amp * 0.8f * falloff;
                    V = glm::translate(V, glm::vec3(ax, ay, 0.0f));
                }
                vm = V;
            }

            audio_engine->setListener(camera->Position, camera->Front, camera->Up);

            {
                glm::vec3 normal_sky_color = glm::vec3(0.55f, 0.70f, 0.95f); // modra
                glm::vec3 quake_sky_color = glm::vec3(0.55f, 0.55f, 0.55f);  // seda

                static float quakeBlend = 0.0f;
                float target = cupcagame->get_game_state().quake_active ? 1.0f : 0.0f;
                float easeInRate = 2.5f;
                float easeOutRate = 1.5f;

                static auto lastBG = std::chrono::high_resolution_clock::now();
                auto nowBG = std::chrono::high_resolution_clock::now();
                float dtBG = std::chrono::duration<float>(nowBG - lastBG).count();
                lastBG = nowBG;

                float rate = (target > quakeBlend) ? easeInRate : easeOutRate;
                quakeBlend += (target - quakeBlend) * glm::clamp(rate * dtBG, 0.0f, 1.0f);
                quakeBlend = glm::clamp(quakeBlend, 0.0f, 1.0f);

                glm::vec3 bg = glm::mix(normal_sky_color, quake_sky_color, quakeBlend);
                glClearColor(bg.r, bg.g, bg.b, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }

            static float lastLightingUpdate = 0.0f;
            const float lightingUpdateInterval = 1.0f / 30.0f;

            if (lightning_system && (elapsedTime - lastLightingUpdate) >= lightingUpdateInterval)
            {
                lightning_system->updateLights(elapsedTime);

                if (camera)
                {
                    lightning_system->spotLight.position = camera->Position;
                    lightning_system->spotLight.direction = camera->Front;
                }
                else
                {
                    lightning_system->spotLight.position = glm::vec3(0.0f, 2.0f, 5.0f);
                    lightning_system->spotLight.direction = glm::vec3(0.0f, 0.0f, -1.0f);
                }

                glm::vec3 viewPos = camera ? camera->Position : glm::vec3(0.0f, 2.0f, 5.0f);

                lightning_system->setupLightUniforms(*phong_shader, viewPos);

                lastLightingUpdate = elapsedTime;
            }

            phong_shader->activate();

            phong_shader->setUniform("uV_m", vm);

            if (road_shader && g_road_vao != 0)
            {
                road_shader->activate();

                road_shader->setUniform("uV_m", vm);
                road_shader->setUniform("uProj_m", pm);
                road_shader->setUniform("uniform_Color", glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));

                if (g_roadTex != 0)
                {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, g_roadTex);
                    road_shader->setUniform("textureSampler", 0);
                    road_shader->setUniform("useTexture", true);
                }
                else
                {
                    road_shader->setUniform("useTexture", false);
                }

                glBindVertexArray(g_road_vao);

                for (const auto &seg : cupcagame->get_game_state().road_segments)
                {
                    // segment silnice pod kamerou (z-fighting)
                    glm::vec3 pos(seg.x, -0.02f, seg.z);

                    float roadWidth = cupcagame->get_game_state().road_segment_width;
                    float roadLength = cupcagame->get_game_state().road_segment_length;
                    glm::vec3 scl(roadWidth, 1.0f, roadLength);

                    glm::mat4 model = glm::mat4(1.0f);
                    model = glm::translate(model, pos);
                    model = glm::scale(model, scl);

                    road_shader->setUniform("uM_m", model);

                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                }

                glBindVertexArray(0);
                if (g_roadTex != 0)
                {
                    glBindTexture(GL_TEXTURE_2D, 0);
                }
                road_shader->deactivate();
            }

            const float cameraZ = camera ? camera->Position.z : 0.0f;
            // renderovani domu ve scene
            const float cullingDistance = 120.0f;

            for (const auto &h : cupcagame->get_game_state().houses)
            {
                // vyber modelu podle typu modelu
                std::string model_name = h.modelName;
                if (model_name.empty())
                {
                    model_name = "bambo_house"; // vychoze bambo_house
                }

                if (scene.find(model_name) != scene.end())
                {
                    glm::vec3 pos = h.position;
                    glm::vec3 rot(0.0f);
                    glm::vec3 scl(1.0f);

                    if (model_name == "bambo_house")
                    {
                        scl = glm::vec3(2.0f);
                    }
                    else if (model_name == "cyprys_house")
                    {
                        scl = glm::vec3(2.5f);
                    }
                    else if (model_name == "building")
                    {
                        scl = glm::vec3(1.5f);
                    }

                    scene.at(model_name)->draw(pos, rot, scl);
                }

                if (h.requesting && scene.find("cupcake") != scene.end())
                {
                    glm::vec3 indicator_pos = h.position + glm::vec3(0.0f, h.indicator_height, 0.0f);

                    indicator_pos.y += sin(elapsedTime * 2.5f) * 0.7f;

                    float x_offset = h.half_extents.x + 3.0f;

                    if (h.position.x < 0) // dum je nalevo
                    {
                        indicator_pos.x += x_offset;
                    }
                    else // dum je napravo
                    {
                        indicator_pos.x -= x_offset;
                    }

                    glm::vec3 indicator_scale(0.2f);

                    glm::mat4 cupcake_model_matrix = glm::mat4(1.0f);
                    cupcake_model_matrix = glm::translate(cupcake_model_matrix, indicator_pos);
                    cupcake_model_matrix = glm::rotate(cupcake_model_matrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                    cupcake_model_matrix = glm::scale(cupcake_model_matrix, indicator_scale);

                    phong_shader->setUniform("material.diffuse", glm::vec3(1.0f, 0.95f, 0.2f));
                    phong_shader->setUniform("material.emission", glm::vec3(2.5f, 2.3f, 0.5f));

                    scene.at("cupcake")->draw(cupcake_model_matrix);

                    phong_shader->setUniform("material.emission", glm::vec3(0.0f, 0.0f, 0.0f));
                }
            }

            if (scene.find("cupcake") != scene.end())
            {
                for (const auto &projectile : cupcagame->get_game_state().projectiles)
                {
                    if (projectile && projectile->alive)
                    {
                        projectile->draw(scene.at("cupcake").get());
                    }
                }
            }

            // slunce
            if (scene.find("sphere") != scene.end() && lightning_system)
            {
                glm::vec3 sunDirection = -lightning_system->dirLight.direction;

                float sunDistance = 100.0f;
                glm::vec3 cameraPos = camera ? camera->Position : glm::vec3(0.0f, 2.0f, 5.0f);
                glm::vec3 sunPosition = cameraPos + sunDirection * sunDistance;

                sunPosition.y = glm::max(sunPosition.y, 20.0f);

                // rotace pro efekt
                glm::vec3 sunRotation(0.0f, elapsedTime * 30.0f, 0.0f);
                glm::vec3 sunScale(20.0f);

                phong_shader->setUniform("material.emission", glm::vec3(2.0f, 1.5f, 0.5f));

                scene.at("sphere")->draw(sunPosition, sunRotation, sunScale);

                phong_shader->setUniform("material.emission", glm::vec3(0.0f, 0.0f, 0.0f));
            }

            phong_shader->deactivate();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            if (cupcagame && cupcagame->get_game_state().active)
            {
                ImGui::SetNextWindowPos(ImVec2(g_window_width - 220.0f, 10.0f), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(210.0f, 90.0f), ImGuiCond_Always);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.15f, 0.6f));
                ImGui::Begin("Stav hry", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

                ImGui::Text("Penize: $%d", cupcagame->get_game_state().money);
                ImGui::Separator();
                ImGui::Text("Spokojenost: %d%%", cupcagame->get_game_state().happiness);
                float happiness_fraction = static_cast<float>(cupcagame->get_game_state().happiness) / 100.0f;
                ImGui::ProgressBar(happiness_fraction, ImVec2(-1.0f, 0.0f), "");

                ImGui::End();
                ImGui::PopStyleColor();
            }

            if (!cupcagame->get_game_state().active)
            {
                ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
                ImGui::SetNextWindowSize(ImVec2(static_cast<float>(g_window_width), static_cast<float>(g_window_height)));
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.7f));
                ImGui::Begin("GameOverOverlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);

                float window_width = ImGui::GetWindowSize().x;
                float window_height = ImGui::GetWindowSize().y;

                bool is_game_over = cupcagame->is_game_over();
                const char *title_text = is_game_over ? "GAME OVER" : "PAUZA";

                ImVec2 title_size = ImGui::CalcTextSize(title_text);
                ImGui::SetCursorPos(ImVec2((window_width - title_size.x) * 0.5f, window_height * 0.3f));

                ImVec4 title_color = is_game_over ? ImVec4(1.0f, 0.2f, 0.2f, 1.0f) : ImVec4(1.0f, 1.0f, 0.2f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, title_color);
                ImGui::Text("%s", title_text);
                ImGui::PopStyleColor();

                float content_start_y = window_height * 0.45f;
                ImGui::SetCursorPos(ImVec2((window_width - 350) * 0.5f, content_start_y));

                ImDrawList *draw_list = ImGui::GetWindowDrawList();
                ImVec2 content_pos = ImGui::GetCursorScreenPos();
                ImVec2 content_size(350.0f, 200.0f);

                ImGui::SetCursorPos(ImVec2((window_width - 330) * 0.5f, content_start_y + 10.0f));

                if (is_game_over)
                {
                    ImGui::Text("Finalni statistiky:");
                    ImGui::Separator();

                    if (cupcagame->get_game_state().money <= 0)
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Dosly ti penize!");
                        ImGui::Text("Strilel jsi prilis mnoho cupcaku...");
                    }
                    if (cupcagame->get_game_state().happiness <= 0)
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Doslo ti spokojenost!");
                        ImGui::Text("Neuspokojil jsi zakazniky...");
                    }
                    ImGui::Separator();
                }
                else
                {
                    ImGui::Text("Hra je pozastavena");
                    ImGui::Separator();
                }

                ImGui::Text("Penize: $%d", cupcagame->get_game_state().money);
                ImGui::Text("Spokojenost: %d%%", cupcagame->get_game_state().happiness);
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "Ovladani:");

                if (is_game_over)
                {
                    ImGui::Text("F5 - Restart hry");
                }
                else
                {
                    ImGui::Text("F5 - Pokracovat");
                }
                ImGui::Text("ESC - Konec hry");

                ImGui::End();
                ImGui::PopStyleColor();

                std::string title = g_windowTitle + (is_game_over ? " | GAME OVER" : " | PAUSED");
                glfwSetWindowTitle(window, title.c_str());
            }

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            if (!cupcagame->get_game_state().active)
            {
                glEnable(GL_SCISSOR_TEST);
                int rw = static_cast<int>(g_window_width * 0.65f);
                int rh = static_cast<int>(g_window_height * 0.28f);
                int rx = (g_window_width - rw) / 2;
                int ry = (g_window_height - rh) / 2;
                glScissor(rx, ry, rw, rh);

                glm::vec3 normal_sky_color = glm::vec3(0.55f, 0.70f, 0.95f);
                glm::vec3 quake_sky_color = glm::vec3(0.55f, 0.55f, 0.55f);
                glm::vec3 panel = glm::mix(quake_sky_color, normal_sky_color, 0.25f) * 0.6f;
                glClearColor(panel.r, panel.g, panel.b, 0.55f);
                glClear(GL_COLOR_BUFFER_BIT);
                glDisable(GL_SCISSOR_TEST);

                std::string title = g_windowTitle + " | GAME OVER";
                glfwSetWindowTitle(window, title.c_str());
            }

            glfwSwapBuffers(window);

            glfwPollEvents();
        }

        phong_shader->clear();
        if (particle_shader)
        {
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