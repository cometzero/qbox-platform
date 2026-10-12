-- SPDX-License-Identifier: BSD-3-Clause
-- AP-only firmware substitutes and Linux loader; no SI/RSE firmware executes.
return function(ctx)
    local options = ctx.options
    local boot_stub_address = 0x000f0000 -- AP shared SRAM, above SDS and BL2 work areas.
    local common = dofile(ctx.apollo_dir.."apollo-qvp-common.lua")
    assert(options.enable_ap_cpus, "Linux profile requires QBOX_RDASPEN_ENABLE_AP_CPUS=true")
    assert(not ctx.config.runtime_injection.enabled,
           "Full-system runtime injection is not supported by the Linux profile")

    local platform = {
        moduletype = "Container";
        quantum_ns = 10000000;
        ap_router = {moduletype = "router"; log_level = 0};
        keep_alive_0 = {moduletype = "keep_alive"};
        css_system_counter = {
            moduletype = "arm_system_counter";
            input_frequency_hz = 125000000;
            integer_increment = 1;
            reported_frequency_hz = 125000000;
            construction_priority = -298;
        };
    }
    common.define_ap(ctx, platform)
    dofile(ctx.apollo_dir.."vp/debug/qmp.lua")(platform, false)
    if ctx.config.monitor.enabled then
        platform.qbox_monitor = {
            moduletype = "monitor";
            bind_address = "127.0.0.1";
            server_port = ctx.config.monitor.port;
            runtime_mutation = false;
            use_html_presentation = false;
        }
    end
    platform.host_smd_gpio = {
        moduletype = "qemu_pl061";
        args = {"&platform.ap_qemu_inst"};
        mem = {address = 0x40750000; size = 0x10000;
               bind = "&ap_router.initiator_socket"};
        irq = {bind = "&ap_gic.spi_in_193"};
    }
    common.connect_board(ctx, platform)

    dofile(ctx.apollo_dir.."vp/backends/audio.lua").connect(platform)

    -- Retain the AP hardware definitions; replace only their firmware/domain edges.
    -- These objects implement firmware boot, whole-platform reset or SI fault routing.
    for _, name in ipairs({
        "ap_reset_gpio", "ap_cold_reset_fanout", "apollo_system_reset_fanout",
        "ap_bl2_reset_loader", "host_ap_flash", "ap_ns_watchdog_ws1_fanout",
    }) do
        platform[name] = nil
    end
    for cluster=0,3 do
        platform["ap_cl"..cluster.."_ni710ae_fmu"] = nil
        platform["ap_sbist_cluster"..cluster] = nil
    end
    platform.ap_watchdog_0.ws1 = {bind = "&ap_gic.spi_in_51"}
    platform.ap_secure_wdog.ws1 = nil
    platform.ap_secure_console_file.write_file = options.primary_console_log..".secure"
    platform.ap_secure_console_file.read_file = "/dev/null"
    platform.ap_secure_console_file.poll_read = false
    platform.ap_qemu_inst.managed_start_in_reset_release = false
    -- There is no secure firmware to assign interrupt groups in this profile.
    platform.ap_gic.has_security_extensions = false
    for i=0,options.AP_NUM_CPUS-1 do
        local cpu = platform["ap_cpu_"..i]
        cpu.start_in_reset = false
        cpu.rvbar = boot_stub_address
        cpu.psci_conduit = "smc"
        cpu.linux_smc_stub_address = 0x0f000000
        cpu.ras_uncontainable_interrupt = nil
    end

    -- Keep DT-described slots readable; DeviceID zero means no virtio device.
    local first_empty = ctx.getenv_or("QBOX_RDASPEN_ROOTFS", "") == "" and 0 or 1
    for i=first_empty,3 do
        local mem = platform["ap_virtioblk_"..i].mem
        platform["ap_virtioblk_"..i] = {
            moduletype = "gs_memory";
            target_socket = {address = mem.address; size = mem.size;
                             bind = mem.bind};
            init_mem = true;
            read_only = true;
            dmi_allow = false;
            load = {data = {0x74726976, 2, 0, 0}; offset = 0};
            log_level = 0;
        }
    end

    -- AP and RoS use one local decode in the firmware-free profile. No ATU setup
    -- or host physical-domain routing is needed. Rewrite only socket bindings.
    local function bind_local(value)
        for key, item in pairs(value) do
            if type(item) == "table" then
                bind_local(item)
            elseif key == "bind" and type(item) == "string" then
                value[key] = item:gsub("&system_router%.", "&ap_router.")
                                 :gsub("&host_router%.", "&ap_router.")
            end
        end
    end
    bind_local(platform)
    if options.smmu_backend == "systemc-mmu720ae" then
        platform.ap_gpex_0.bus_master = {bind = "&ap_smmu_lti00.upstream_socket"}
    end

    local function required(name)
        local path = ctx.getenv_or(name, "")
        assert(path ~= "", "Missing Linux boot artifact: "..name)
        return path
    end
    platform.linux_loader = {
        moduletype = "loader";
        initiator_socket = {bind = "&ap_router.target_socket"};
        {bin_file = required("QBOX_LINUX_BOOT_STUB"); address = boot_stub_address};
        {bin_file = required("QBOX_LINUX_KERNEL"); address = 0x80200000};
        {bin_file = required("QBOX_LINUX_DTB"); address = 0x88000000};
    }
    local initrd = ctx.getenv_or("QBOX_LINUX_INITRD", "")
    if initrd ~= "" then
        table.insert(platform.linux_loader, {bin_file = initrd; address = 0x90000000})
    end
    dofile(ctx.apollo_dir.."vp/mocks/domains.lua").define(ctx, platform)
    print("Apollo QVP Linux-only profile: SystemC domain mocks enabled")
    return platform
end
