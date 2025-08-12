#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

// Forward declarations
struct ma_engine_wrapper;
struct ma_sound_wrapper;

class AudioEngine
{
private:
    std::unique_ptr<ma_engine_wrapper> engine;
    std::unordered_map<unsigned int, std::unique_ptr<ma_sound_wrapper>> activeSounds;
    unsigned int nextHandle;

public:
    AudioEngine();
    ~AudioEngine();

    bool init();
    void shutdown();

    // 3D audio functions
    void setListener(const glm::vec3 &position, const glm::vec3 &forward, const glm::vec3 &up);
    bool playLoop3D(const std::string &filepath, const glm::vec3 &position, unsigned int *handle = nullptr);
    void setSoundPosition(unsigned int handle, const glm::vec3 &position);
    void setSoundVolume(unsigned int handle, float volume);
    void playOneShot3D(const std::string &filepath, const glm::vec3 &position);
    void stop_sound(unsigned int handle);

    // Utility
    bool isInitialized() const;
};
