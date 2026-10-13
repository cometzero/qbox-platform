local AP_FMU_CL0_BASE = 0x1D000000
local AP_REGISTER_WINDOW_SIZE = 0x00010000
local AP_FMU_CL1_BASE = 0x1D100000
local AP_FMU_CL2_BASE = 0x1D200000
local AP_FMU_CL3_BASE = 0x1D300000
local AP_SBIST_CLUSTER0_BASE = 0x140100000
local AP_SI_CL0_FMU_BASE = 0x2A510000
local AP_SBIST_SIZE = 0x00100000

local safety = {}

function safety.define(ctx, platform)
    local options = ctx.options
    assert(platform.ap_cl0_ni710ae_fmu == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_cl0_ni710ae_fmu = {
            moduletype = "zena_ni710ae_fmu";
            node_index = 0xb;
            parent_critical_record = 3;
            parent_non_critical_record = 1;
            target_socket = {
                address = AP_FMU_CL0_BASE;
                size = AP_REGISTER_WINDOW_SIZE;
                bind = "&system_router.initiator_socket";
            };
            fault_socket = {bind = "&si_cl0_router.target_socket"};
            log_level = 0;
        }
    end

    assert(platform.ap_cl1_ni710ae_fmu == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_cl1_ni710ae_fmu = {
            moduletype = "zena_ni710ae_fmu";
            node_index = 0xb;
            parent_critical_record = 18;
            parent_non_critical_record = 16;
            target_socket = {
                address = AP_FMU_CL1_BASE;
                size = AP_REGISTER_WINDOW_SIZE;
                bind = "&system_router.initiator_socket";
            };
            fault_socket = {bind = "&si_cl0_router.target_socket"};
            log_level = 0;
        }
    end

    assert(platform.ap_cl2_ni710ae_fmu == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_cl2_ni710ae_fmu = {
            moduletype = "zena_ni710ae_fmu";
            node_index = 0xb;
            parent_critical_record = 33;
            parent_non_critical_record = 31;
            target_socket = {
                address = AP_FMU_CL2_BASE;
                size = AP_REGISTER_WINDOW_SIZE;
                bind = "&system_router.initiator_socket";
            };
            fault_socket = {bind = "&si_cl0_router.target_socket"};
            log_level = 0;
        }
    end

    assert(platform.ap_cl3_ni710ae_fmu == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_cl3_ni710ae_fmu = {
            moduletype = "zena_ni710ae_fmu";
            node_index = 0xb;
            parent_critical_record = 48;
            parent_non_critical_record = 46;
            target_socket = {
                address = AP_FMU_CL3_BASE;
                size = AP_REGISTER_WINDOW_SIZE;
                bind = "&system_router.initiator_socket";
            };
            fault_socket = {bind = "&si_cl0_router.target_socket"};
            log_level = 0;
        }
    end

    if options.enable_ap_cpus then
        for cluster = 0, 3 do
            assert(platform["ap_sbist_cluster" .. cluster] == nil, "duplicate Apollo object")
            platform["ap_sbist_cluster" .. cluster] = {
                moduletype = "apollo_sbist";
                cpu_base = cluster * 4;
                fmu_base = AP_SI_CL0_FMU_BASE;
                target_socket = {
                    address = AP_SBIST_CLUSTER0_BASE + (cluster * 0x04000000);
                    size = AP_SBIST_SIZE;
                    bind = "&system_router.initiator_socket";
                    priority = 0;
                };
                fmu_initiator = {bind = "&si_cl0_router.target_socket"};
                log_level = 0;
            }
        end
    end
end

return safety
