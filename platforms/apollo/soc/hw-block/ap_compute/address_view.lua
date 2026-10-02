local address_view = {}

function address_view.enable_ap_router(ctx, platform, hw)
    local options = ctx.options
    local AP_ADDRESS = hw.AP_ADDRESS
    local AP_SIZE = hw.AP_SIZE

    print("Apollo QVP AP router enabled...")

    local hipc_shared_base, hipc_shared_size =
        ctx.modules.si_cl1.hipc_shared_window()

    -- AP logical view
    platform.ap_router = {
        moduletype = "router";
        log_level = 0;
    }

    platform.ap_hipc_alias = {
        moduletype = "addrtr";
        mapped_base_addr = AP_ADDRESS.hipc_alias;
        target_socket = {
            address = hipc_shared_base;
            size = hipc_shared_size;
            bind = "&ap_router.initiator_socket";
            relative_addresses = false;
            priority = 0;
        };
        initiator_socket = {bind = "&ap_router.target_socket"};
        log_level = 0;
    }

    platform.ap_to_system_rse_carveout_bridge = {
        moduletype = "addrtr";
        mapped_base_addr = AP_ADDRESS.mhu_pointer_access;
        target_socket = {
            address = AP_ADDRESS.mhu_pointer_access;
            size = AP_SIZE.mhu_pointer_access;
            bind = "&ap_router.initiator_socket";
            relative_addresses = false;
            priority = 0;
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        log_level = 0;
    }

    platform.ap_to_system_pfdi_monitor_mhu_bridge = {
        moduletype = "addrtr";
        mapped_base_addr = AP_ADDRESS.host_pfdi_monitor_mhu_pbx;
        target_socket = {
            address = AP_ADDRESS.pfdi_monitor_mhu_pbx;
            size = AP_SIZE.pfdi_monitor_mhu;
            bind = "&ap_router.initiator_socket";
            relative_addresses = false;
            priority = 0;
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        log_level = 0;
    }

    platform.system_to_ap_flash_bridge = {
        moduletype = "addrtr";
        mapped_base_addr = AP_ADDRESS.flash;
        target_socket = {
            address = AP_ADDRESS.flash;
            size = AP_SIZE.flash;
            bind = "&system_router.initiator_socket";
            relative_addresses = false;
        };
        initiator_socket = {bind = "&ap_router.target_socket"};
        log_level = 0;
    }

    local smd_fmu_aliases = {
        { local_base = AP_ADDRESS.fmu_cl0;
          physical_base = AP_ADDRESS.host_fmu_cl0 };
        { local_base = AP_ADDRESS.fmu_cl1;
          physical_base = AP_ADDRESS.host_fmu_cl1 };
        { local_base = AP_ADDRESS.fmu_cl2;
          physical_base = AP_ADDRESS.host_fmu_cl2 };
        { local_base = AP_ADDRESS.fmu_cl3;
          physical_base = AP_ADDRESS.host_fmu_cl3 };
    }
    for i, route in ipairs(smd_fmu_aliases) do
        platform["smd_ap_cl" .. (i - 1) .. "_ni710ae_fmu_alias"] = {
            moduletype = "addrtr";
            mapped_base_addr = route.local_base;
            target_socket = {
                address = route.physical_base;
                size = AP_SIZE.fmu_modeled;
                bind = "&smd_router.initiator_socket";
                relative_addresses = false;
            };
            initiator_socket = {bind = "&ap_router.target_socket"};
            log_level = 0;
        }
    end

    -- Host-to-AP translation and AP masters
    local function bind_ap_target(target)
        if target ~= nil then
            target.bind = "&ap_router.initiator_socket"
            target.priority = target.priority or 0
        end
    end

    local function bind_ap_socket(device, socket_name)
        if device ~= nil then
            bind_ap_target(device[socket_name])
        end
    end

    if platform.host_ap_atu ~= nil and
       platform.host_ap_atu.translation_socket ~= nil then
        platform.host_ap_atu.translation_socket.bind =
            "&ap_router.initiator_socket"
        platform.host_ap_atu.translation_socket.priority = 10
    end

    if platform.ap_global_peripheral_initiator ~= nil then
        platform.ap_global_peripheral_initiator.global_initiator = {
            bind = "&ap_router.target_socket";
        }
    end
    if platform.ap_bl2_reset_loader ~= nil then
        platform.ap_bl2_reset_loader.initiator_socket = {
            bind = "&ap_router.target_socket";
        }
    end
    if platform.ap_gpex_0 ~= nil then
        if options.smmu_backend == "systemc-mmu720ae" then
            platform.ap_gpex_0.bus_master = {
                bind = "&ap_smmu_lti00.upstream_socket";
            }
        else
            platform.ap_gpex_0.bus_master = {
                bind = "&ap_router.target_socket";
            }
        end
        bind_ap_target(platform.ap_gpex_0.pio_iface)
        bind_ap_target(platform.ap_gpex_0.mmio_iface)
        bind_ap_target(platform.ap_gpex_0.ecam_iface)
        bind_ap_target(platform.ap_gpex_0.mmio_iface_high)
    end
    if platform.ap_pcie_epc ~= nil then
        bind_ap_target(platform.ap_pcie_epc.regs)
        bind_ap_target(platform.ap_pcie_epc.outbound)
        platform.ap_pcie_epc.local_master = {bind = "&ap_router.target_socket"}
    end

    -- Memory and interrupt controller windows
    bind_ap_socket(platform.host_ap_shared_sram, "target_socket")
    bind_ap_socket(platform.host_ap_mhu_ns_shared_sram, "target_socket")
    bind_ap_socket(platform.host_ap_peripheral_ns_sram_tail, "target_socket")
    bind_ap_socket(platform.host_ap_bl2_header_sram, "target_socket")
    bind_ap_socket(platform.host_ap_flash, "target_socket")
    bind_ap_socket(platform.host_ap_trusted_nvctr, "target_socket")
    bind_ap_socket(platform.host_ap_dram1, "target_socket")
    bind_ap_socket(platform.host_ap_dram2, "target_socket")
    bind_ap_socket(platform.host_ap_cper, "target_socket")
    bind_ap_socket(platform.host_ap_ffa_mm_comm_buffer, "target_socket")
    bind_ap_socket(platform.host_ap_spmc_sdram, "target_socket")
    bind_ap_socket(platform.ap_gic, "dist_iface")
    if platform.ap_gic ~= nil then
        for i = 0, 15 do
            bind_ap_socket(platform.ap_gic, "redist_iface_" .. i)
        end
    end
    bind_ap_socket(platform.ap_gic_its, "mem")
    if options.smmu_backend == "systemc-mmu720ae" then
        bind_ap_socket(platform.ap_smmu_0, "target_socket")
    else
        bind_ap_socket(platform.ap_smmu_0, "mem")
    end
    if options.smmu_backend == "systemc-mmu720ae" and platform.ap_smmu_0 ~= nil then
        platform.ap_smmu_0.dma = {
            bind = "&ap_router.target_socket";
        }
        platform.ap_smmu_lti00.downstream_socket = {
            bind = "&ap_router.target_socket";
        }
    end

    -- RoS and AP peripherals
    ctx.ros.bind_ap_view_targets(platform, bind_ap_target)
    bind_ap_socket(platform.ap_watchdog_0, "control")
    bind_ap_socket(platform.ap_watchdog_0, "refresh")
    bind_ap_socket(platform.ap_secure_uart, "target_socket")
    bind_ap_socket(platform.ap_primary_uart, "target_socket")
    bind_ap_socket(platform.ap_timer_mem, "mem")
    bind_ap_socket(platform.ap_secure_wdog, "control")
    bind_ap_socket(platform.ap_secure_wdog, "refresh")
    bind_ap_socket(platform.ap_sid, "target_socket")
    bind_ap_socket(platform.ap_rgic2lgic_messreg, "target_socket")
    bind_ap_socket(platform.ap_cl0_ni710ae_fmu, "target_socket")
    bind_ap_socket(platform.ap_cl1_ni710ae_fmu, "target_socket")
    bind_ap_socket(platform.ap_cl2_ni710ae_fmu, "target_socket")
    bind_ap_socket(platform.ap_cl3_ni710ae_fmu, "target_socket")
    for cluster = 0, 3 do
        bind_ap_socket(platform["ap_sbist_cluster" .. cluster], "target_socket")
    end
    -- AP CPU initiator view
    for i = 0, 15 do
        local cpu = platform["ap_cpu_" .. i]
        if cpu ~= nil then
            cpu.mem = {bind = "&ap_router.target_socket"}
        end
    end
end

return address_view
