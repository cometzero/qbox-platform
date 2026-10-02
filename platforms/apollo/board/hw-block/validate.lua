-- Check populated board I2C buses, including device-specific address aliases.
local validate = {}

function validate.i2c_bus(platform, bus_name)
    assert(platform[bus_name], "missing board I2C bus: " .. bus_name)
    local target = "&" .. bus_name .. ".initiator_socket"
    local owners = {}
    for name, device in pairs(platform) do
        if type(device) == "table" and device.i2c_socket and
           device.i2c_socket.bind == target then
            local address = device.address
            -- TPS6594 pages decode at the base address and four aliases.
            local span = device.moduletype == "tps6594" and 5 or 1
            assert(type(address) == "number" and address % 1 == 0 and
                   address >= 0 and address + span - 1 <= 0x7f,
                   "invalid I2C address range: " .. name)
            for alias = address, address + span - 1 do
                assert(owners[alias] == nil,
                       "I2C address collision on " .. bus_name .. " at " ..
                       string.format("0x%02x", alias) .. ": " .. name ..
                       " and " .. tostring(owners[alias]))
                owners[alias] = name
            end
        end
    end
end

return validate
