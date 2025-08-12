#include "AudioEngine.hpp"
#include "miniaudio.h"
#include <iostream>
#include <memory>

// PIMPL wrapper for ma_engine
struct ma_engine_wrapper
{
    ma_engine engine;
    bool initialized;

    ma_engine_wrapper() : initialized(false)
    {
        // Initialize with default config
        ma_engine_config config = ma_engine_config_init();
        config.listenerCount = 1;
        config.channels = 2; // Stereo output
        config.sampleRate = 48000;

        if (ma_engine_init(&config, &engine) == MA_SUCCESS)
        {
            initialized = true;
        }
        else
        {
            std::cerr << "Failed to initialize audio engine" << std::endl;
        }
    }

    ~ma_engine_wrapper()
    {
        if (initialized)
        {
            ma_engine_uninit(&engine);
        }
    }

    ma_engine *get() { return &engine; }
    bool isInitialized() const { return initialized; }
};

// PIMPL wrapper for ma_sound
struct ma_sound_wrapper
{
    ma_sound sound;
    bool initialized;

    ma_sound_wrapper() : initialized(false)
    {
        // Initialize with null sound
        memset(&sound, 0, sizeof(sound));
    }

    ~ma_sound_wrapper()
    {
        if (initialized)
        {
            ma_sound_uninit(&sound);
        }
    }

    ma_sound *get() { return &sound; }
    void setInitialized(bool state) { initialized = state; }
    bool isInitialized() const { return initialized; }
};

AudioEngine::AudioEngine() : engine(nullptr), nextHandle(1)
{
}

AudioEngine::~AudioEngine()
{
    shutdown();
}

bool AudioEngine::init()
{
    try
    {
        engine = std::make_unique<ma_engine_wrapper>();
        return engine && engine->isInitialized();
    }
    catch (...)
    {
        std::cerr << "Exception during audio engine initialization" << std::endl;
        return false;
    }
}

void AudioEngine::shutdown()
{
    activeSounds.clear();
    engine.reset();
}

void AudioEngine::setListener(const glm::vec3 &position, const glm::vec3 &forward, const glm::vec3 &up)
{
    if (!engine || !engine->isInitialized())
    {
        return;
    }
    glm::vec3 miniaudio_forward = -forward; // Invert forward vector for miniaudio compatibility

    ma_engine_listener_set_position(engine->get(), 0, position.x, position.y, position.z);

    ma_engine_listener_set_direction(engine->get(), 0, miniaudio_forward.x, miniaudio_forward.y, miniaudio_forward.z);

    ma_engine_listener_set_world_up(engine->get(), 0, up.x, up.y, up.z);
}

bool AudioEngine::playLoop3D(const std::string &filepath, const glm::vec3 &position, unsigned int *handle)
{
    if (!engine || !engine->isInitialized())
    {
        std::cerr << "Audio engine not initialized" << std::endl;
        return false;
    }

    std::cout << "Attempting to load: " << filepath << std::endl;

    auto sound = std::make_unique<ma_sound_wrapper>();

    // Initialize sound from file with 3D spatialization (enabled by default)
    ma_uint32 flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_ASYNC;

    ma_result result = ma_sound_init_from_file(engine->get(), filepath.c_str(), flags, nullptr, nullptr, sound->get());
    if (result != MA_SUCCESS)
    {
        std::cerr << "Failed to load sound: " << filepath << " (error code: " << result << ")" << std::endl;
        return false;
    }

    sound->setInitialized(true);
    std::cout << "Sound loaded successfully" << std::endl;

    // Set initial position
    ma_sound_set_position(sound->get(), position.x, position.y, position.z);
    std::cout << "Sound position set to: " << position.x << ", " << position.y << ", " << position.z << std::endl;

    // Set to loop
    ma_sound_set_looping(sound->get(), MA_TRUE);
    std::cout << "Sound set to looping" << std::endl;

    // Start playing
    result = ma_sound_start(sound->get());
    if (result != MA_SUCCESS)
    {
        std::cerr << "Failed to start sound: " << filepath << " (error code: " << result << ")" << std::endl;
        return false;
    }

    std::cout << "Sound started playing" << std::endl;

    // Store the sound with a unique handle
    unsigned int currentHandle = nextHandle++;
    activeSounds[currentHandle] = std::move(sound);

    // Return handle if requested
    if (handle)
    {
        *handle = currentHandle;
    }

    std::cout << "Sound registered with handle: " << currentHandle << std::endl;
    return true;
}

void AudioEngine::setSoundPosition(unsigned int handle, const glm::vec3 &position)
{
    auto it = activeSounds.find(handle);
    if (it != activeSounds.end() && it->second)
    {
        ma_sound_set_position(it->second->get(), position.x, position.y, position.z);
    }
}

void AudioEngine::setSoundVolume(unsigned int handle, float volume)
{
    auto it = activeSounds.find(handle);
    if (it != activeSounds.end() && it->second)
    {
        ma_sound_set_volume(it->second->get(), volume);
    }
}

void AudioEngine::stop_sound(unsigned int handle)
{
    auto it = activeSounds.find(handle);
    if (it != activeSounds.end())
    {
        if (it->second && it->second->isInitialized())
        {
            ma_sound_stop(it->second->get());
        }
        activeSounds.erase(it);
    }
}

void AudioEngine::playOneShot3D(const std::string &filepath, const glm::vec3 &position)
{
    if (!engine || !engine->isInitialized())
    {
        return;
    }

    // For one-shot sounds, we use ma_engine_play_sound which handles cleanup automatically
    ma_engine_play_sound(engine->get(), filepath.c_str(), nullptr);
}

bool AudioEngine::isInitialized() const
{
    return engine && engine->isInitialized();
}
