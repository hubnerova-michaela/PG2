#define GLM_ENABLE_EXPERIMENTAL
#include "CupcakeGame.hpp"
#include "camera.hpp"
#include "AudioEngine.hpp"
#include "ParticleSystem.hpp"
#include "PhysicsSystem.hpp"

#include <iostream>
#include <algorithm>
#include <glm/gtx/norm.hpp>

CupcakeGame::CupcakeGame()
    : rng(std::random_device{}()), unirand(0.0f, 1.0f)
{
    this->house_generator = std::make_unique<HouseGenerator>();
    this->house_models = {"bambo_house", "cyprys_house", "building"};
    this->cached_physics_system = nullptr;
}

CupcakeGame::~CupcakeGame() {}

void CupcakeGame::initialize()
{
    this->game_state = GameState();

    game_state.money = 50;
    game_state.happiness = 50;

    for (int i = 0; i < game_state.road_segment_count; ++i)
    {
        game_state.road_segments.push_back(glm::vec3(0.0f, 0.0f, -static_cast<float>(i) * game_state.road_segment_length));
    }
}

void CupcakeGame::restart_game()
{
    game_state = GameState();
    game_state.money = 50;
    game_state.happiness = 50;
    game_state.active = true;

    // Odstraneni projektilu
    game_state.projectiles.clear();

    // Odstraneni domu
    game_state.houses.clear();

    // Vyresetovani silnice
    game_state.road_segments.clear();
    for (int i = 0; i < game_state.road_segment_count; ++i)
    {
        game_state.road_segments.push_back(glm::vec3(0.0f, 0.0f, -static_cast<float>(i) * game_state.road_segment_length));
    }

    // Vyresetovani nacachovaneho fyzikalniho systemu
    if (cached_physics_system)
    {
        cached_physics_system->clearCollisionObjects();
    }

    std::cout << "Hra restartovana!" << std::endl;
}

glm::vec3 CupcakeGame::calculate_movement(float delta, Camera *camera, PhysicsSystem *physics_system)
{
    if (!camera || !game_state.active)
    {
        return glm::vec3(0.0f);
    }

    float moveSpeed = game_state.speed * delta;
    glm::vec3 world_movement = glm::vec3(0.0f, 0.0f, moveSpeed);

    return world_movement;
}

void CupcakeGame::update(float delta, Camera *camera, AudioEngine *audio_engine,
                         ParticleSystem *particle_system, PhysicsSystem *physics_system)
{
    if (!game_state.active)
        return;

    // stav hry
    if (game_state.money <= 0 || game_state.happiness <= 0)
    {
        game_state.active = false;
        std::cout << "Game Over! ";
        if (game_state.money <= 0)
            std::cout << "Dosly penize!";
        if (game_state.happiness <= 0)
            std::cout << "Doslo spokojenost!";
        std::cout << std::endl;
        return;
    }

    glm::vec3 world_movement = calculate_movement(delta, camera, physics_system);

    for (auto &house : game_state.houses)
    {
        house.position += world_movement;
    }
    for (auto &seg : game_state.road_segments)
    {
        seg += world_movement;
    }
    for (auto &projectile : game_state.projectiles)
    {
        if (projectile)
        {
            projectile->position += world_movement;
        }
    }
    if (game_state.quake_active)
    {
        game_state.quake_epicenter += world_movement;
    }

    house_generator->updateRequests(delta, this->game_state, camera);
    update_earthquake(delta, camera, audio_engine, particle_system);
    update_projectiles(delta);
    update_houses(camera, physics_system);

    if (particle_system)
    {
        for (auto &house : game_state.houses)
        {
            if (house.delivery_effect_timer > 0.0f)
            {
                house.delivery_effect_timer -= delta;
                glm::vec3 emitterPos = house.position + glm::vec3(0.0f, house.indicator_height * 0.7f, 0.0f);
                particle_system->set_emitter_position(emitterPos);
                particle_system->emit(2);
                if (house.delivery_effect_timer <= 0.0f)
                {
                    house.delivered = false;
                }
            }
        }
    }
}

void CupcakeGame::update_houses(Camera *camera, PhysicsSystem *physics_system)
{
    if (!camera)
        return;

    this->cached_physics_system = physics_system;

    const float road_recycle_z = camera->Position.z + 20.0f;

    for (auto &seg : game_state.road_segments)
    {
        if (seg.z > road_recycle_z)
        {
            float minZ = 0.0f;
            for (const auto &s2 : game_state.road_segments)
            {
                minZ = std::min(minZ, s2.z);
            }
            seg.z = minZ - game_state.road_segment_length;
        }
    }

    const float house_removal_z = camera->Position.z + 30.0f;

    auto removal_begin_it = std::remove_if(
        game_state.houses.begin(), game_state.houses.end(),
        [&](const House &h)
        {
            return h.position.z > house_removal_z;
        });

    if (physics_system)
    {
        for (auto it = removal_begin_it; it != game_state.houses.end(); ++it)
        {
            physics_system->remove_collision_object(it->position);
        }
    }

    game_state.houses.erase(removal_begin_it, game_state.houses.end());
}

void CupcakeGame::handle_mouse_click(Camera *camera)
{
    if (!camera || !game_state.active)
        return;

    if (game_state.money < 10)
    {
        std::cout << "Nedostatek penez pro vystrelebi cupcaku!" << std::endl;
        return;
    }

    // Cena za vystrel
    game_state.money -= 10;
    glm::vec3 projectile_position = camera->Position + camera->Front * 2.0f;
    glm::vec3 projectile_velocity = camera->Front * 50.0f;
    auto projectile = std::make_unique<Projectile>(projectile_position, projectile_velocity, 0.3f);
    game_state.projectiles.push_back(std::move(projectile));
}

void CupcakeGame::update_projectiles(float deltaTime)
{
    for (auto &projectile : game_state.projectiles)
    {
        if (!projectile || !projectile->alive)
            continue;

        projectile->update(deltaTime);

        for (auto &house : game_state.houses)
        {
            if (house.delivered)
                continue;

            // AABB detekce kolize
            glm::vec3 boxMin = house.position - house.half_extents;
            glm::vec3 boxMax = house.position + house.half_extents;

            glm::vec3 closestPoint = glm::clamp(projectile->position, boxMin, boxMax);

            float distanceSq = glm::distance2(projectile->position, closestPoint);
            if (distanceSq < (projectile->radius * projectile->radius))
            {

                projectile->alive = false; // neaktivni cupcake

                if (house.requesting)
                {                           // uspesna dodavka
                    game_state.money += 20; // +20 penez (-10 naklad = +10 zisk)
                    game_state.happiness = std::min(100, game_state.happiness + 10);

                    house.requesting = false;
                    house.delivered = true;
                    house.delivery_effect_timer = 2.5f; // particle effect casovac

                    game_state.requesting_house_id = -1;
                    game_state.request_time_left = 0.0f;
                }
                else
                { // spatna dodavka
                    game_state.happiness = std::max(0, game_state.happiness - 10);
                }
                break;
            }
        }
    }

    // odstraneni neaktivnich projektilu
    game_state.projectiles.erase(
        std::remove_if(game_state.projectiles.begin(), game_state.projectiles.end(),
                       [](const std::unique_ptr<Projectile> &p)
                       {
                           return !p || !p->alive;
                       }),
        game_state.projectiles.end());
}

glm::vec3 CupcakeGame::get_house_extents(const std::string &model_name)
{
    if (model_name == "bambo_house")
        return glm::vec3(4.0f, 6.0f, 4.0f);
    if (model_name == "cyprys_house")
        return glm::vec3(4.5f, 7.0f, 4.5f);
    if (model_name == "building")
        return glm::vec3(6.0f, 9.0f, 6.0f);
    return glm::vec3(3.5f, 5.0f, 3.5f);
}

float CupcakeGame::get_indicator_height(const std::string &modelName)
{
    if (modelName == "bambo_house")
        return 8.0f;
    if (modelName == "cyprys_house")
        return 9.0f;
    if (modelName == "building")
        return 12.0f;
    return 7.0f;
}

void CupcakeGame::update_movement(float delta, Camera *camera, PhysicsSystem *physics_system)
{
    if (!camera || !game_state.active)
        return;
    float moveSpeed = game_state.speed * delta;
    glm::vec3 movement = glm::vec3(0.0f, 0.0f, -moveSpeed);
    if (physics_system)
    {
        glm::vec3 actualMovement = physics_system->moveCamera(*camera, movement);
        camera->Position += actualMovement;
    }
    else
    {
        camera->Position += movement;
    }
}

void CupcakeGame::update_earthquake(float delta, Camera *camera, AudioEngine *audio_engine, ParticleSystem *particle_system)
{
    if (!camera)
        return;
    if (game_state.quake_cooldown > 0.0f)
    {
        game_state.quake_cooldown -= delta;
    }
    if (!game_state.quake_active && game_state.quake_cooldown <= 0.0f)
    {
        game_state.quake_active = true;
        game_state.quake_time_left = game_state.quake_duration;
        game_state.quake_epicenter = camera->Position + glm::vec3(
                                                            (unirand(rng) - 0.5f) * 50.0f, 0.0f, (unirand(rng) - 0.5f) * 50.0f);
        if (audio_engine && !quake_sound_playing)
        {
            if (audio_engine->playLoop3D("resources/audio/052256_cracking-earthquake-cracking-soil-cracking-stone-86770.wav", game_state.quake_epicenter, &quake_sound_handle))
            {
                quake_sound_playing = true;
            }
        }
    }
    if (game_state.quake_active)
    {
        game_state.quake_time_left -= delta;
        if (game_state.quake_time_left <= 0.0f)
        {
            game_state.quake_active = false;
            game_state.quake_cooldown = 25.0f;
            if (audio_engine && quake_sound_playing)
            {
                audio_engine->stop_sound(quake_sound_handle);
                quake_sound_playing = false;
            }
        }
        else
        {
            float distance = glm::length(camera->Position - game_state.quake_epicenter);
            float intensity = std::max(0.0f, 1.0f - distance / 100.0f);
            float shake_x = (unirand(rng) - 0.5f) * game_state.quake_amplitude * intensity;
            float shake_y = (unirand(rng) - 0.5f) * game_state.quake_amplitude * intensity;
            float shake_z = (unirand(rng) - 0.5f) * game_state.quake_amplitude * intensity;
            camera->Position += glm::vec3(shake_x, shake_y, shake_z);

            if (particle_system)
            {
                for (const auto &segment : game_state.road_segments)
                {
                    float halfWidth = game_state.road_segment_width * 0.5f;
                    float halfLength = game_state.road_segment_length * 0.5f;

                    glm::vec3 pos = glm::vec3(
                        (unirand(rng) - 0.5f) * game_state.road_segment_width,
                        0.1f,
                        segment.z + (unirand(rng) - 0.5f) * game_state.road_segment_length);

                    particle_system->set_emitter_position(pos);
                    particle_system->emit_smoke(5);
                }
            }
        }
    }
}