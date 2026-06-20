local fabric = {}

function fabric.create()
    return {
        moduletype = "Container";
        quantum_ns = 10000000;

        -- Root fabric
        host_router = {
            moduletype = "router";
            log_level = 0;
        },

        keep_alive_0 = {
            moduletype = "keep_alive";
        },

    }
end

return fabric
