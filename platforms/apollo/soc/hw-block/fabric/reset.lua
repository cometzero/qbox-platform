-- SPDX-License-Identifier: BSD-3-Clause
-- Hardware reset fanout. Keep acknowledgement sources before AP release.
return function(options)
    local AP_CORES_PER_CLUSTER = 4
    function options.ap_cpu_reset_bind_targets(count)
        local targets = {}
        for i=0,(count-1) do
            targets[#targets + 1] = "&ap_cpu_"..i..".reset"
        end
        return table.concat(targets, ";")
    end

    function options.ap_cold_reset_bind_targets()
        local targets = {
            -- Clear both WS1 sources before releasing the asynchronous AP reset.
            -- WS1 falling acknowledges reset to the host controller; it must not
            -- remain asserted while reset_gpio waits for its QEMU reset handshake.
            "&ap_watchdog_0.reset";
            "&ap_secure_wdog.reset";
            "&ap_secure_uart.reset";
            "&ap_primary_uart.reset";
            "&dma350_0.reset";
            "&dma350_1.reset";
            "&pinctrl_peri0.reset";
            "&pinctrl_peri1.reset";
            "&host_ap_bl2_header_sram.reset";
            "&ap_reset_gpio.reset_in";
            "&host_ap_si_ns_scmi_mhu_pbx.reset";
            "&host_ap_si_ns_scmi_mhu_mbx.reset";
            "&host_ap_si_scmi_mhu_pbx.reset";
            "&host_ap_si_scmi_mhu_mbx.reset";
            "&host_ap_si_cl1_mhu_pbx.reset";
            "&host_ap_si_cl1_mhu_mbx.reset";
            "&host_ap_si_pfdi_monitor_mhu_pbx.reset";
            "&host_ap_si_pfdi_monitor_mhu_mbx.reset";
            "&host_ap_rse_mhu_pbx.reset";
            "&host_ap_rse_mhu_mbx.reset";
        }

        if options.smmu_backend == "systemc-mmu720ae" then
            targets[#targets + 1] = "&ap_smmu_0.reset"
        end

        for i=0,5 do
            targets[#targets + 1] = "&ap_dw_i2c_"..i..".reset"
        end
        for i=0,3 do
            targets[#targets + 1] = "&ap_dw_ssi_"..i..".reset"
            targets[#targets + 1] = "&ap_dw_uart_"..i..".reset"
        end
        for i=0,1 do
            targets[#targets + 1] = "&ap_dw_i2s_"..i..".reset"
        end


        return table.concat(targets, ";")
    end

    function options.ap_system_reset_bind_targets()
        local targets = {}
        for cpu=0,(options.AP_NUM_CPUS-1) do
            local cluster = math.floor(cpu / AP_CORES_PER_CLUSTER)
            local core = cpu % AP_CORES_PER_CLUSTER
            targets[#targets + 1] =
                "&si_cl0_ap_cluster"..cluster.."_core"..core.."_ppu.reset"
        end
        targets[#targets + 1] = options.ap_cold_reset_bind_targets()

        return table.concat(targets, ";")
    end

    function options.apollo_system_reset_bind_targets()
        local targets = {
            options.ap_system_reset_bind_targets();
            "&rse_host_uart0_s.reset";
            "&rse_sysctrl.reset";
            "&rse_watchdog_ns.reset";
            "&rse_watchdog_s.reset";
            "&rse_mhu0_sender_s.reset";
            "&rse_mhu0_receiver_s.reset";
            "&rse_mhu2_sender_s.reset";
            "&rse_mhu2_receiver_s.reset";
            "&host_rse_si_mhu_pbx.reset";
            "&host_rse_si_mhu_mbx.reset";
            "&host_smd_gpio_cold_reset.reset";
            "&host_smd_gpio.reset";
            "&si_cl0_watchdog.reset";
        }

        if options.rse_local_crypto then
            targets[#targets + 1] = "&rse_cpu_pass.rse_kmu_regs.reset"
            targets[#targets + 1] = "&rse_cpu_pass.rse_cc3xx.reset"
        else
            targets[#targets + 1] = "&rse_kmu_regs.reset"
            targets[#targets + 1] = "&rse_cc3xx.reset"
        end

        targets[#targets + 1] = "&rse_cpu_pass.cpu_0.accel_reset"
        targets[#targets + 1] = "&rse_sys_rss_reset_fanout.reset_in"

        targets[#targets + 1] = "&host_si_cl0_clus_ppu.reset"
        targets[#targets + 1] = "&host_si_cl0_core0_ppu.reset"
        targets[#targets + 1] = "&si_cl0_ni710ae_primary_nci.reset"
        targets[#targets + 1] = "&si_cl0_ni710ae_secondary_nci.reset"
        targets[#targets + 1] = "&si_cl0_ni710ae_mhu_nci.reset"
        targets[#targets + 1] = "&si_cl0_qemu_inst.reset"
        for _, frame in ipairs({
            "si_cl0_ap_ns_mhu_pbx";
            "si_cl0_ap_ns_mhu_mbx";
            "si_cl0_ap_scmi_mhu_pbx";
            "si_cl0_ap_scmi_mhu_mbx";
            "si_cl0_ap_pfdi_monitor_mhu_pbx";
            "si_cl0_ap_pfdi_monitor_mhu_mbx";
            "si_cl0_rse_mhu_pbx";
            "si_cl0_rse_mhu_mbx";
        }) do
            targets[#targets + 1] = "&"..frame..".reset"
        end

        targets[#targets + 1] = "&host_si_cl1_clus_ppu.reset"
        targets[#targets + 1] = "&si_cl1_cluster_ppu.reset"
        for cpu=0,3 do
            targets[#targets + 1] = "&si_cl1_core"..cpu.."_ppu.reset"
        end
        targets[#targets + 1] = "&si_cl1_qemu_inst.reset"
        for _, frame in ipairs({
            "si_cl1_hipc_mhu_pbx";
            "si_cl1_hipc_mhu_mbx";
            "si_cl1_pfdi_mhu_pbx";
            "si_cl1_pfdi_reply_mhu_mbx";
        }) do
            targets[#targets + 1] = "&"..frame..".reset"
        end
        targets[#targets + 1] = "&si_cl0_pfdi_mhu_pbx.reset"
        targets[#targets + 1] = "&si_cl0_pfdi_mhu_mbx.reset"
        targets[#targets + 1] = "&si_cl0_uart.reset"
        targets[#targets + 1] = "&si_cl1_uart.reset"
        targets[#targets + 1] = "&si_gic_multiview.reset"
        targets[#targets + 1] = "&ap_rgic2lgic_messreg.reset"



        return table.concat(targets, ";")
    end

end
