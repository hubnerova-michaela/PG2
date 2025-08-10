-- This script controls how houses are generated along the road.

-- A table to hold the available house models. We'll populate this from C++.
house_models = {}

-- A function to pick a random model from the list.
function get_random_house_model()
    local index = math.random(1, #house_models)
    return house_models[index]
end

-- This is the main function our C++ code will call.
-- It receives the Z position where new houses should be spawned.
function generate_houses_at_z(z_position)
    -- Tweakable parameters for generation
    local house_side_offset = 10.0
    local empty_plot_probability = 0.2 -- 20% chance of an empty plot

    -- --- Left side of the road ---
    if math.random() > empty_plot_probability then
        -- This function, 'add_house', is a C++ function we expose to Lua.
        add_house({
            model = get_random_house_model(),
            position = { x = -house_side_offset, y = 0.0, z = z_position }
        })
    end

    -- --- Right side of the road ---
    if math.random() > empty_plot_probability then
        add_house({
            model = get_random_house_model(),
            position = { x = house_side_offset, y = 0.0, z = z_position }
        })
    end
end