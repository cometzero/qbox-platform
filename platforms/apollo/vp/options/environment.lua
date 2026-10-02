-- SPDX-License-Identifier: BSD-3-Clause
-- Pure host option readers; no values are exported to the Lua global table.
return function(options)
    local AP_CORES_PER_CLUSTER = 4
    function options.getenv_or(name, default)
        local value = os.getenv(name)
        if value == nil or value == "" then
            return default
        end
        return value
    end

    function options.getenv_number_or(name, default)
        local value = tonumber(options.getenv_or(name, default))
        assert(value ~= nil, name.." must be numeric")
        return value
    end

    function options.getenv_bool_or(name, default)
        local value = options.getenv_or(name, default and "true" or "false")
        return value == "true" or value == "1" or value == "yes"
    end

    function options.repeat_value(value, count)
        local values = {}
        for _=1,count do
            values[#values + 1] = value
        end
        return values
    end

    function options.mp_affinity(cpu_index)
        local cluster = math.floor(cpu_index / AP_CORES_PER_CLUSTER)
        local core = cpu_index % AP_CORES_PER_CLUSTER
        return (cluster * 0x10000) + (core * 0x100)
    end

    function options.ap_cpu_index(cluster, core)
        return cluster * AP_CORES_PER_CLUSTER + core
    end

end
