-- This script controls how houses are generated along the road.

-- A table to hold the available house models. We'll populate this from C++.
house_models = {}

-- A function to pick a random model from the list.
function get_random_house_model()
    if #house_models == 0 then
        print("WARNING: house_models table is empty!")
        return "house" -- fallback
    end
    local index = math.random(1, #house_models)
    return house_models[index]
end

-- This is the main function our C++ code will call.
-- It receives the Z position where new houses should be spawned.
function generate_houses_at_z(z_position)
    print("Lua generate_houses_at_z called with z_position: " .. z_position)
    print("Available house models count: " .. #house_models)
    
    -- Tweakable parameters for generation
    local house_side_offset = 10.0
    local empty_plot_probability = 0.2 -- 20% chance of an empty plot

    -- --- Left side of the road ---
    local left_roll = math.random()
    print("Left side roll: " .. left_roll .. " (threshold: " .. empty_plot_probability .. ")")
    if left_roll > empty_plot_probability then
        local model = get_random_house_model()
        print("Creating left house with model: " .. model)
        -- This function, 'add_house', is a C++ function we expose to Lua.
        add_house({
            model = model,
            position = vec3(-house_side_offset, 0.0, z_position)
        })
    else
        print("Left plot empty")
    end

    -- --- Right side of the road ---
    local right_roll = math.random()
    print("Right side roll: " .. right_roll .. " (threshold: " .. empty_plot_probability .. ")")
    if right_roll > empty_plot_probability then
        local model = get_random_house_model()
        print("Creating right house with model: " .. model)
        add_house({
            model = model,
            position = vec3(house_side_offset, 0.0, z_position)
        })
    else
        print("Right plot empty")
    end
    
    print("generate_houses_at_z completed")
end