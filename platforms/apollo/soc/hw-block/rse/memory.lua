local RSE_ITCM_SECURE_BASE = 0x10000000
local RSE_ITCM_SIZE = 0x00008000
local RSE_ITCM_NON_SECURE_BASE = 0x00000000
local RSE_ITCM_CPU0_SECURE_BASE = 0x1A000000
local RSE_ITCM_CPU0_NON_SECURE_BASE = 0x0A000000
local RSE_DTCM_SECURE_BASE = 0x30000000
local RSE_DTCM_SIZE = 0x00008000
local RSE_DTCM_NON_SECURE_BASE = 0x20000000
local RSE_DTCM_CPU0_SECURE_BASE = 0x34000000
local RSE_DTCM_CPU0_NON_SECURE_BASE = 0x24000000
local RSE_VM0_SECURE_BASE = 0x31000000
local RSE_TRAM_SECURE_BASE = 0x5015D000
local RSE_REGISTER_WINDOW_SIZE = 0x00001000

local module = {}

function module.create(ctx)
    local rse_dtcm_dmi = ctx.options.rse_dtcm_dmi
    local rse_itcm_dmi = ctx.options.rse_itcm_dmi
    local rse_split_cpu0_dtcm_alias = ctx.options.rse_split_cpu0_dtcm_alias
    local rse_split_cpu0_itcm_alias = ctx.options.rse_split_cpu0_itcm_alias
    local rse_vm_dmi = ctx.options.rse_vm_dmi
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

    local rse_vmaddrwidth = ctx.options.rse_vmaddrwidth
    local RSE_VM_SIZE = 2 ^ rse_vmaddrwidth
    local RSE_VM1_SECURE_BASE = RSE_VM0_SECURE_BASE + RSE_VM_SIZE

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.rse_itcm == nil,
            "duplicate object: platform.rse_itcm")
        platform.rse_itcm = {
            moduletype = "gs_memory";
            shared_memory = true;
            shared_memory_prefix = "rse_itcm_";
            dmi_allow = rse_itcm_dmi;
            target_socket = {
                address = RSE_ITCM_SECURE_BASE;
                size = RSE_ITCM_SIZE;
                bind = "&rse_router.initiator_socket";
                aliases = rse_tcm_aliases(
                    rse_split_cpu0_itcm_alias,
                    RSE_ITCM_NON_SECURE_BASE,
                    RSE_ITCM_CPU0_SECURE_BASE,
                    RSE_ITCM_CPU0_NON_SECURE_BASE,
                    RSE_ITCM_SIZE);
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
                    address = RSE_ITCM_CPU0_SECURE_BASE;
                    size = RSE_ITCM_SIZE;
                    bind = "&rse_router.initiator_socket";
                    aliases = {
                        cpu0_ns = {
                            address = RSE_ITCM_CPU0_NON_SECURE_BASE;
                            size = RSE_ITCM_SIZE;
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
                address = RSE_DTCM_SECURE_BASE;
                size = RSE_DTCM_SIZE;
                bind = "&rse_router.initiator_socket";
                aliases = rse_tcm_aliases(
                    rse_split_cpu0_dtcm_alias,
                    RSE_DTCM_NON_SECURE_BASE,
                    RSE_DTCM_CPU0_SECURE_BASE,
                    RSE_DTCM_CPU0_NON_SECURE_BASE,
                    RSE_DTCM_SIZE);
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
                    address = RSE_DTCM_CPU0_SECURE_BASE;
                    size = RSE_DTCM_SIZE;
                    bind = "&rse_router.initiator_socket";
                    aliases = {
                        cpu0_ns = {
                            address = RSE_DTCM_CPU0_NON_SECURE_BASE;
                            size = RSE_DTCM_SIZE;
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
                address = RSE_VM0_SECURE_BASE;
                size = RSE_VM_SIZE;
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
                address = RSE_VM1_SECURE_BASE;
                size = RSE_VM_SIZE;
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
                address = RSE_TRAM_SECURE_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
                bind = "&rse_router.initiator_socket";
            };
            init_mem = true;
            log_level = 0;
        }
    end

    return block
end

return module
