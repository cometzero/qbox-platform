local fabric = {}

function fabric.create(ctx)
    assert(ctx.machine_contract.topology.machine == "apollo-qvp")
    return {
        moduletype = "Container";
        quantum_ns = 10000000;

        -- Root fabric
        system_router = {
            moduletype = "router";
            log_level = 0;
        },

        keep_alive_0 = {
            moduletype = "keep_alive";
        },

    }
end

return fabric
