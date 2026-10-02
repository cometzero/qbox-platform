-- SPDX-License-Identifier: BSD-3-Clause
-- SMD defines shared control, translation, timers and domain mailboxes.
local module = {}

function module.create(ctx)
    local hw = dofile(ctx.apollo_dir.."soc/hw-block/system_mgmt/constants.lua")(ctx)
    local system_mgmt = {}
    system_mgmt.ownership = {
        reset = {
            "reset_gpio";
            "reset_fanout";
            "zena_reset_ctrl";
            "host_ppu";
            "mhu320ae";
        };
        shared_memory = {
            "host_rse_si_ssram";
            "host_smd_shared_sram";
        };
        messaging = {
            "host_ap_rse_mhu_pbx";
            "host_ap_rse_mhu_mbx";
            "host_rse_si_mhu_pbx";
            "host_rse_si_mhu_mbx";
            "host_ap_si_pfdi_monitor_mhu_pbx";
            "host_ap_si_pfdi_monitor_mhu_mbx";
            "si_cl1_pfdi_mhu_pbx";
        };
        translation = {
            "rse_atu_regs";
            "host_si_atu";
            "host_ap_atu";
            "host_smdexp2smd_atu";
        };
        safety_control = {
            "host_smcf_mgi";
            "si_cl0_ssu";
            "si_cl0_fmu";
            "host_scr";
            "host_system_pll";
            "host_gtimer";
            "host_css_counters_timers";
            "host_css_counters_timers_read";
            "host_css_counters_timers_sync";
        };
        peripherals = {
            "host_smd_gpio";
        };
    }

    function system_mgmt.define(ctx, platform)
        dofile(ctx.apollo_dir.."soc/hw-block/system_mgmt/power.lua")(ctx, platform, hw)
        dofile(ctx.apollo_dir.."soc/hw-block/system_mgmt/si_mailbox.lua")(ctx, platform, hw)
        dofile(ctx.apollo_dir.."soc/hw-block/system_mgmt/si_sram.lua")(ctx, platform, hw)
        dofile(ctx.apollo_dir.."soc/hw-block/system_mgmt/ap_atu.lua")(ctx, platform, hw)
        dofile(ctx.apollo_dir.."soc/hw-block/system_mgmt/ap_si_mailbox.lua")(ctx, platform, hw)
        dofile(ctx.apollo_dir.."soc/hw-block/system_mgmt/io_reset.lua")(ctx, platform, hw)
        dofile(ctx.apollo_dir.."soc/hw-block/system_mgmt/timer.lua")(ctx, platform, hw)
        dofile(ctx.apollo_dir.."soc/hw-block/system_mgmt/shared_sram.lua")(ctx, platform, hw)
        dofile(ctx.apollo_dir.."soc/hw-block/system_mgmt/ap_rse_mailbox.lua")(ctx, platform, hw)
    end
    return system_mgmt
end

return module
