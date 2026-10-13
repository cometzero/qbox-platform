local RSE_PROVISIONING_OFFSET = 0x00020000
local RSE_ROM_SECURE_BASE = 0x11000000

-- RSE firmware image loading and execution diagnostics.
local module = {}

function module.create(ctx)
    local AP_BL2_ELF = ctx.options.AP_BL2_ELF
    local AP_NUM_CPUS = ctx.options.AP_NUM_CPUS
    local ap_flash = ctx.options.ap_flash
    local primary_console_log = ctx.options.primary_console_log
    local provisioning_bundle = ctx.options.provisioning_bundle
    local rse_flash = ctx.options.rse_flash
    local rse_flash_backend = ctx.options.rse_flash_backend
    local rse_log = ctx.options.rse_log
    local rse_lsc_input_hz = ctx.options.rse_lsc_input_hz
    local rse_otp = ctx.options.rse_otp
    local rse_rom = ctx.options.rse_rom
    local rse_smd_counter_mirror = ctx.options.rse_smd_counter_mirror
    local rse_vmaddrwidth = ctx.options.rse_vmaddrwidth
    local secure_console_log = ctx.options.secure_console_log

    local RSE_VM_SIZE = 2 ^ rse_vmaddrwidth

    local block = {}

    function block.load_images(ctx, platform)
        platform.rse_rom.load = {bin_file = rse_rom; offset = 0;}
        platform.rse_vm1.load = {bin_file = provisioning_bundle; offset = RSE_PROVISIONING_OFFSET;}
        platform.rse_otp_wrapper.load = {bin_file = rse_otp; offset = 0;}
    end

    function block.report(ctx, platform)
        print("rse rom:      " .. rse_rom)
        print("rse flash:    " .. rse_flash)
        print("rse flash backend: " .. rse_flash_backend)
        print("rse otp:      " .. rse_otp)
        print("ap flash:     " .. ap_flash)
        print("ap bl2 elf:   " .. AP_BL2_ELF)
        print("provisioning: " .. provisioning_bundle)
        print("rse log:      " .. rse_log)
        print("secure log:   " .. secure_console_log)
        print("primary log:  " .. primary_console_log)
        print("ap cpus:      " .. tostring(AP_NUM_CPUS))
        print("rse rom base: 0x" .. string.format("%x", RSE_ROM_SECURE_BASE))
        print("rse vmaddrwidth: " .. tostring(rse_vmaddrwidth))
        print("rse vm size:  0x" .. string.format("%x", RSE_VM_SIZE))
        print("rse SMD counter mirror: " .. tostring(rse_smd_counter_mirror))
        print("rse LSC input Hz: " .. tostring(rse_lsc_input_hz))
    end

    return block
end

return module
