local machine_contract = {}

local contract_files = {
    topology = "topology.lua";
    address_map = "address_map.lua";
    transaction_routes = "transaction_routes.lua";
    signal_routes = "signal_routes.lua";
    boot_control = "boot_control.lua";
    software_contract = "software_contract.lua";
}

local function join_path(directory, name)
    if directory:sub(-1) == "/" then
        return directory..name
    end
    return directory.."/"..name
end

local function indexed_by_name(values, label)
    local indexed = {}
    for _, value in ipairs(values) do
        assert(type(value.name) == "string" and value.name ~= "", label.." requires name")
        assert(indexed[value.name] == nil, "duplicate "..label..": "..value.name)
        indexed[value.name] = value
    end
    return indexed
end

local function validate_references(contract)
    local domains = indexed_by_name(contract.topology.domains, "domain")
    local views = indexed_by_name(contract.topology.views, "view")
    local routers = indexed_by_name(contract.topology.routers, "router")
    local bridges = indexed_by_name(contract.topology.bridges, "bridge")
    local initiators = indexed_by_name(contract.transaction_routes.initiators, "initiator")

    for _, view in ipairs(contract.topology.views) do
        assert(domains[view.domain] ~= nil, "view references missing domain: "..view.name)
    end
    for _, router in ipairs(contract.topology.routers) do
        assert(views[router.view] ~= nil, "router references missing view: "..router.name)
    end
    for _, bridge in ipairs(contract.topology.bridges) do
        assert(views[bridge.from] ~= nil, "bridge references missing source view: "..bridge.name)
        assert(views[bridge.to] ~= nil, "bridge references missing target view: "..bridge.name)
    end
    for _, range in ipairs(contract.address_map.ranges) do
        assert(views[range.view] ~= nil, "range references missing view: "..range.name)
        assert(type(range.base) == "number" and range.base >= 0, "invalid range base: "..range.name)
        assert(type(range.size) == "number" and range.size > 0, "invalid range size: "..range.name)
        if range.bridge ~= nil then
            assert(bridges[range.bridge] ~= nil, "range references missing bridge: "..range.name)
        end
    end
    for _, route in ipairs(contract.transaction_routes.routes) do
        assert(initiators[route.initiator] ~= nil, "route references missing initiator: "..route.name)
        assert(views[route.from] ~= nil, "route references missing source view: "..route.name)
        assert(views[route.to] ~= nil, "route references missing target view: "..route.name)
        if route.bridge ~= nil then
            assert(bridges[route.bridge] ~= nil, "route references missing bridge: "..route.name)
        end
        if route.target:sub(-7) == "_router" then
            assert(routers[route.target] ~= nil, "route references missing router: "..route.name)
        end
    end
end

local function json_escape(value)
    local escaped = value:gsub("\\", "\\\\")
    escaped = escaped:gsub('"', '\\"')
    escaped = escaped:gsub("\b", "\\b")
    escaped = escaped:gsub("\f", "\\f")
    escaped = escaped:gsub("\n", "\\n")
    escaped = escaped:gsub("\r", "\\r")
    escaped = escaped:gsub("\t", "\\t")
    return escaped
end

local function array_length(value)
    local length = 0
    for key, _ in pairs(value) do
        if type(key) ~= "number" or key < 1 or key % 1 ~= 0 then
            return nil
        end
        if key > length then
            length = key
        end
    end
    for index=1,length do
        if value[index] == nil then
            return nil
        end
    end
    return length
end

local function encode_json(value, indent)
    local value_type = type(value)
    if value_type == "nil" then
        return "null"
    end
    if value_type == "boolean" then
        return tostring(value)
    end
    if value_type == "number" then
        if math.type ~= nil and math.type(value) == "integer" then
            return string.format("%d", value)
        end
        if value % 1 == 0 then
            return string.format("%.0f", value)
        end
        return tostring(value)
    end
    if value_type == "string" then
        return '"'..json_escape(value)..'"'
    end
    assert(value_type == "table", "unsupported JSON type: "..value_type)

    local next_indent = indent.."  "
    local length = array_length(value)
    if length ~= nil then
        if length == 0 then
            return "[]"
        end
        local items = {}
        for index=1,length do
            items[#items + 1] = next_indent..encode_json(value[index], next_indent)
        end
        return "[\n"..table.concat(items, ",\n").."\n"..indent.."]"
    end

    local keys = {}
    for key, _ in pairs(value) do
        assert(type(key) == "string", "JSON object key must be a string")
        keys[#keys + 1] = key
    end
    table.sort(keys)
    if #keys == 0 then
        return "{}"
    end
    local items = {}
    for _, key in ipairs(keys) do
        items[#items + 1] = next_indent..'"'..json_escape(key)..'": '..encode_json(value[key], next_indent)
    end
    return "{\n"..table.concat(items, ",\n").."\n"..indent.."}"
end

local function write_json(path, value)
    local handle, error_message = io.open(path, "w")
    assert(handle ~= nil, error_message)
    handle:write(encode_json(value, ""), "\n")
    handle:close()
end

function machine_contract.load(directory, validate)
    local contract = {}
    for key, name in pairs(contract_files) do
        contract[key] = dofile(join_path(directory, name))
    end
    if validate ~= false then
        machine_contract.assert_valid(contract)
    end
    return contract
end

function machine_contract.assert_valid(contract)
    assert(contract.topology.schema_version == 1, "unsupported topology schema")
    assert(contract.address_map.schema_version == 1, "unsupported address schema")
    assert(contract.transaction_routes.schema_version == 1, "unsupported transaction schema")
    assert(contract.signal_routes.schema_version == 1, "unsupported signal schema")
    assert(contract.boot_control.schema_version == 1, "unsupported boot schema")
    assert(contract.software_contract.schema_version == 1, "unsupported software schema")
    validate_references(contract)
end

function machine_contract.range(contract, name)
    for _, range in ipairs(contract.address_map.ranges) do
        if range.name == name then
            return range
        end
    end
    error("missing address range: "..name)
end

function machine_contract.emit(contract, directory)
    write_json(join_path(directory, "topology.json"), contract.topology)
    write_json(join_path(directory, "address-routes.json"), contract.address_map)
    write_json(join_path(directory, "transaction-routes.json"), contract.transaction_routes)
    write_json(join_path(directory, "irq-routes.json"), {
        schema_version = contract.signal_routes.schema_version;
        routes = contract.signal_routes.irq_routes;
    })
    write_json(join_path(directory, "reset-routes.json"), {
        schema_version = contract.signal_routes.schema_version;
        fault_routes = contract.signal_routes.fault_routes;
        reset_routes = contract.signal_routes.reset_routes;
    })
    write_json(join_path(directory, "boot-routes.json"), contract.boot_control)
    write_json(join_path(directory, "software-routes.json"), contract.software_contract)
end

return machine_contract
