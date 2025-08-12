#pragma once

#include <vector>
#include <string>
#include <memory>
#include <random>
#include <glm/glm.hpp>
#include "HouseGenerator.hpp"
#include "Projectile.hpp"

class Camera;
class AudioEngine;
class ParticleSystem;
class PhysicsSystem;

struct House
{
   int id = -1;
   glm::vec3 position{0.0f};
   std::string modelName;
   glm::vec3 half_extents{1.0f};
   float indicator_height = 7.0f;
   bool requesting = false;
   bool delivered = false;
   float delivery_effect_timer = 0.0f;
};

struct GameState
{
   bool active = true;
   int money = 100;
   int happiness = 100;
   float speed = 8.0f;
   float max_speed = 30.0f;
   int next_house_id = 0;
   int requesting_house_id = -1;
   float request_timeout = 10.0f;
   float request_time_left = 0.0f;
   float request_interval = 5.0f;
   float min_interval = 2.0f;
   float pacing_timer = 0.0f;
   float pacing_step = 10.0f;

   std::vector<glm::vec3> road_segments;
   int road_segment_count = 30;
   float road_segment_length = 10.0f;
   float road_segment_width = 10.0f;
   float house_offset_x = 12.0f;
   float house_spacing = 18.0f;

   std::vector<House> houses;
   std::vector<std::unique_ptr<Projectile>> projectiles;

   bool quake_active = false;
   float quake_timer = 0.0f;
   float quake_cooldown = 30.0f;
   float quake_time_left = 0.0f;
   float quake_duration = 12.0f;
   float quake_amplitude = 0.05f;
   glm::vec3 quake_epicenter{0.0f};
   glm::vec3 quake_relative_offset{0.0f}; // Offset relative to camera for consistent audio
   std::mt19937 rng{std::random_device{}()};
   std::uniform_real_distribution<float> unirand{0.0f, 1.0f};
};

class CupcakeGame
{
public:
   CupcakeGame();
   ~CupcakeGame();

   void initialize();
   glm::vec3 calculate_movement(float delta, Camera *camera, PhysicsSystem *physics_system);
   void update(float delta, Camera *camera, AudioEngine *audio_engine, ParticleSystem *particle_system, PhysicsSystem *physics_system);
   void handle_mouse_click(Camera *camera);
   GameState &get_game_state() { return game_state; }
   glm::vec3 get_house_extents(const std::string &model_name);
   float get_indicator_height(const std::string &model_name);

   bool is_game_over() const { return !game_state.active && (game_state.money <= 0 || game_state.happiness <= 0); }
   void restart_game();

private:
   void update_movement(float delta, Camera *camera, PhysicsSystem *physics_system);
   void update_projectiles(float delta);
   void update_earthquake(float delta, Camera *camera, AudioEngine *audio_engine, ParticleSystem *particle_system = nullptr);
   void update_houses(Camera *camera, PhysicsSystem *physics_system);

   GameState game_state;
   std::unique_ptr<HouseGenerator> house_generator;

   std::mt19937 rng;
   std::uniform_real_distribution<float> unirand;
   std::vector<std::string> house_models;
   float empty_plot_probability = 0.2f;

   unsigned int quake_sound_handle = 0;
   bool quake_sound_playing = false;

   PhysicsSystem *cached_physics_system;
};