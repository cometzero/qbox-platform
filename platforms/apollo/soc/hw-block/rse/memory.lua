local module = {}

function module.create(ctx, hardware, _runtime, _interrupts)
    local rse_dtcm_dmi = ctx.options.rse_dtcm_dmi
    local rse_itcm_dmi = ctx.options.rse_itcm_dmi
    local rse_split_cpu0_dtcm_alias = ctx.options.rse_split_cpu0_dtcm_alias
    local rse_split_cpu0_itcm_alias = ctx.options.rse_split_cpu0_itcm_alias
    local rse_vm_dmi = ctx.options.rse_vm_dmi
    local RSE_ADDRESS = hardware.RSE_ADDRESS
    local RSE_SIZE = hardware.RSE_SIZE
    local function rse_tcm_aliases(
        split_cpu0_alias, ns_address, cpu0_s_address, cpu0_ns_address, size)
        local aliases = {
            ns = {address = ns_address; size = size;};
        }

        if not split_cpu0_alias then
            aliases.cpu0_s = {address = cpu0_s_address; size = size;}
            aliases.cpu0_ns = {address = cpu0_ns_address; size = size;}
        end

        return aliases
    end

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.rse_rom == nil,
            "duplicate object: platform.rse_rom")
        platform.rse_rom = {
            moduletype = "gs_memory";
            read_only = true;
            shared_memory = true;
            shared_memory_prefix = "rse_rom_";
            target_socket = {
                address = RSE_ADDRESS.rom_secure;
                size = RSE_SIZE.rom;
                bind = "&rse_router.initiator_socket";
            };
            log_level = 0;
        }

        assert(
            platform.rse_itcm == nil,
            "duplicate object: platform.rse_itcm")
        platform.rse_itcm = {
            moduletype = "gs_memory";
            shared_memory = true;
            shared_memory_prefix = "rse_itcm_";
            dmi_allow = rse_itcm_dmi;
            target_socket = {
                address = RSE_ADDRESS.itcm_secure;
                size = RSE_SIZE.itcm;
                bind = "&rse_router.initiator_socket";
                aliases = rse_tcm_aliases(
                    rse_split_cpu0_itcm_alias,
                    RSE_ADDRESS.itcm_non_secure,
                    RSE_ADDRESS.itcm_cpu0_secure,
                    RSE_ADDRESS.itcm_cpu0_non_secure,
                    RSE_SIZE.itcm);
            };
            init_mem = true;
            log_level = 0;
        }

        assert(
            platform.rse_itcm_cpu0 == nil,
            "duplicate object: platform.rse_itcm_cpu0")
        if rse_split_cpu0_itcm_alias then
            platform.rse_itcm_cpu0 = {
                moduletype = "gs_memory";
                shared_memory = true;
                shared_memory_prefix = "rse_itcm_cpu0_";
                dmi_allow = rse_itcm_dmi;
                target_socket = {
                    address = RSE_ADDRESS.itcm_cpu0_secure;
                    size = RSE_SIZE.itcm;
                    bind = "&rse_router.initiator_socket";
                    aliases = {
                        cpu0_ns = {
                            address = RSE_ADDRESS.itcm_cpu0_non_secure;
                            size = RSE_SIZE.itcm;
                        };
                    };
                };
                init_mem = true;
                log_level = 0;
            }
        end

        assert(
            platform.rse_dtcm == nil,
            "duplicate object: platform.rse_dtcm")
        platform.rse_dtcm = {
            moduletype = "gs_memory";
            shared_memory = true;
            shared_memory_prefix = "rse_dtcm_";
            dmi_allow = rse_dtcm_dmi;
            target_socket = {
                address = RSE_ADDRESS.dtcm_secure;
                size = RSE_SIZE.dtcm;
                bind = "&rse_router.initiator_socket";
                aliases = rse_tcm_aliases(
                    rse_split_cpu0_dtcm_alias,
                    RSE_ADDRESS.dtcm_non_secure,
                    RSE_ADDRESS.dtcm_cpu0_secure,
                    RSE_ADDRESS.dtcm_cpu0_non_secure,
                    RSE_SIZE.dtcm);
            };
            init_mem = true;
            log_level = 0;
        }

        assert(
            platform.rse_dtcm_cpu0 == nil,
            "duplicate object: platform.rse_dtcm_cpu0")
        if rse_split_cpu0_dtcm_alias then
            platform.rse_dtcm_cpu0 = {
                moduletype = "gs_memory";
                shared_memory = true;
                shared_memory_prefix = "rse_dtcm_cpu0_";
                dmi_allow = rse_dtcm_dmi;
                target_socket = {
                    address = RSE_ADDRESS.dtcm_cpu0_secure;
                    size = RSE_SIZE.dtcm;
                    bind = "&rse_router.initiator_socket";
                    aliases = {
                        cpu0_ns = {
                            address = RSE_ADDRESS.dtcm_cpu0_non_secure;
                            size = RSE_SIZE.dtcm;
                        };
                    };
                };
                init_mem = true;
                log_level = 0;
            }
        end

        assert(
            platform.rse_vm0 == nil,
            "duplicate object: platform.rse_vm0")
        platform.rse_vm0 = {
            moduletype = "gs_memory";
            shared_memory = true;
            shared_memory_prefix = "rse_vm0_";
            dmi_allow = rse_vm_dmi;
            target_socket = {
                address = RSE_ADDRESS.vm0_secure;
                size = RSE_SIZE.vm;
                bind = "&rse_router.initiator_socket";
            };
            init_mem = true;
            log_level = 0;
        }

        assert(
            platform.rse_vm1 == nil,
            "duplicate object: platform.rse_vm1")
        platform.rse_vm1 = {
            moduletype = "gs_memory";
            shared_memory = true;
            shared_memory_prefix = "rse_vm1_";
            dmi_allow = rse_vm_dmi;
            target_socket = {
                address = RSE_ADDRESS.vm1_secure;
                size = RSE_SIZE.vm;
                bind = "&rse_router.initiator_socket";
            };
            init_mem = true;
            log_level = 0;
        }
        assert(
            platform.rse_tram == nil,
            "duplicate object: platform.rse_tram")
        platform.rse_tram = {
            moduletype = "gs_memory";
            target_socket = {
                address = RSE_ADDRESS.tram_secure;
                size = RSE_SIZE.register_window;
                bind = "&rse_router.initiator_socket";
            };
            init_mem = true;
            log_level = 0;
        }
    end

    return block
end

return module
