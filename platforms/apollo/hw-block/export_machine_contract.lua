local function script_directory()
    local source = debug.getinfo(1, "S").source:sub(2)
    return source:match("(.*/)") or "./"
end

local function parse_arguments(arguments)
    local values = {
        contract_dir = script_directory();
        out_dir = nil;
    }
    local index = 1
    while index <= #arguments do
        local argument = arguments[index]
        if argument == "--contract-dir" then
            index = index + 1
            values.contract_dir = assert(arguments[index], "--contract-dir requires a value")
        elseif argument == "--out-dir" then
            index = index + 1
            values.out_dir = assert(arguments[index], "--out-dir requires a value")
        else
            error("unknown argument: "..argument)
        end
        index = index + 1
    end
    assert(values.out_dir ~= nil, "--out-dir is required")
    return values
end

local arguments = parse_arguments(arg)
local contract_module = dofile(script_directory().."machine_contract.lua")
local contract = contract_module.load(arguments.contract_dir, false)
contract_module.emit(contract, arguments.out_dir)
