local RSE_WDOG_NS_WS0_IRQ = 1
local RSE_WDOG_NS_WS1_IRQ = 0
local RSE_WATCHDOG_CLOCK_HZ = 32000000
local RSE_WDOG_NON_SECURE_CONTROL_BASE = 0x48040000
local RSE_REGISTER_WINDOW_SIZE = 0x00001000
local RSE_WDOG_NON_SECURE_REFRESH_BASE = 0x48041000
local RSE_WDOG_SECURE_CONTROL_BASE = 0x58040000
local RSE_WDOG_SECURE_REFRESH_BASE = 0x58041000
local RSE_SYSCTRL_SECURE_BASE = 0x58021000
local RSE_INTEG_LAYER_SECURE_BASE = 0x58100000
local RSE_TIMER0_IRQ = 3
local RSE_TIMER1_IRQ = 4
local RSE_TIMER2_IRQ = 5
local RSE_TIMER3_IRQ = 27
local RSE_MHU0_SENDER_SECURE_BASE = 0x50160000
local RSE_LOCAL_MHU_FRAME_SIZE = 0x00010000
local RSE_MHU0_RECEIVER_SECURE_BASE = 0x50170000
local RSE_CMU_MHU0_RECEIVER_IRQ = 41
local RSE_MHU2_SENDER_SECURE_BASE = 0x501A0000
local RSE_MHU2_RECEIVER_SECURE_BASE = 0x501B0000
local RSE_CMU_MHU2_RECEIVER_IRQ = 45

local module = {}

function module.create(ctx, runtime)
    local rse_cpuwait = ctx.options.rse_cpuwait
    local rse_dma_boot_en = ctx.options.rse_dma_boot_en
    local rse_lsc_input_hz = ctx.options.rse_lsc_input_hz
    local rse_reset_syndrome = ctx.options.rse_reset_syndrome
    local rse_smd_counter_mirror = ctx.options.rse_smd_counter_mirror
    local sysctrl_trace = ctx.options.sysctrl_trace
    local sysctrl_trace_limit = ctx.options.sysctrl_trace_limit
    local RSE_RUNTIME_ADDRESS = runtime.addresses

    local mhu_trace = ctx.options.mhu_trace
    local mhu_trace_limit = ctx.options.mhu_trace_limit

    local rse_flash_backend = ctx.options.rse_flash_backend
    local rse_local_boot_flash = ctx.options.rse_local_boot_flash

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.rse_watchdog_ns == nil,
            "duplicate object: platform.rse_watchdog_ns")
        platform.rse_watchdog_ns = {
            moduletype = "zena_watchdog";
            clock_frequency = RSE_WATCHDOG_CLOCK_HZ;
            control = {
                address = RSE_WDOG_NON_SECURE_CONTROL_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
                bind = "&rse_router.initiator_socket";
            };
            refresh = {
                address = RSE_WDOG_NON_SECURE_REFRESH_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
                bind = "&rse_router.initiator_socket";
            };
            ws0 = {bind = "&rse_cpu_pass.target_signal_socket_" .. RSE_WDOG_NS_WS0_IRQ};
            ws1 = {bind = "&rse_cpu_pass.target_signal_socket_" .. RSE_WDOG_NS_WS1_IRQ};
            log_level = 0;
        }

        assert(
            platform.rse_watchdog_s == nil,
            "duplicate object: platform.rse_watchdog_s")
        platform.rse_watchdog_s = {
            moduletype = "zena_watchdog";
            clock_frequency = RSE_WATCHDOG_CLOCK_HZ;
            control = {
                address = RSE_WDOG_SECURE_CONTROL_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
                bind = "&rse_router.initiator_socket";
            };
            refresh = {
                address = RSE_WDOG_SECURE_REFRESH_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
                bind = "&rse_router.initiator_socket";
            };
            log_level = 0;
        }
        assert(
            platform.rse_sysctrl == nil,
            "duplicate object: platform.rse_sysctrl")
        platform.rse_sysctrl = {
            moduletype = "rse_sysctrl";
            trace = sysctrl_trace;
            trace_limit = sysctrl_trace_limit;
            reset_syndrome = rse_reset_syndrome;
            cpuwait = rse_cpuwait;
            dma_boot_en = rse_dma_boot_en;
            dma_boot_addr = RSE_RUNTIME_ADDRESS.dma_boot;
            target_socket = {
                address = RSE_SYSCTRL_SECURE_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
                bind = "&rse_router.initiator_socket";
            };
            system_reset = {bind = "&apollo_system_reset_fanout.reset_in"};
            log_level = 0;
        }

        assert(
            platform.rse_integ_layer_regs == nil,
            "duplicate object: platform.rse_integ_layer_regs")
        platform.rse_integ_layer_regs = {
            moduletype = "gs_memory";
            target_socket = {
                address = RSE_INTEG_LAYER_SECURE_BASE;
                size = RSE_REGISTER_WINDOW_SIZE;
                bind = "&rse_router.initiator_socket";
            };
            init_mem = true;
            log_level = 0;
        }
    end

    function block.define_local(ctx, platform)
        assert(
            platform.rse_lsc_clock == nil,
            "duplicate object: platform.rse_lsc_clock")
        platform.rse_lsc_clock = {
            moduletype = "qemu_clock_source";
            args = {ctx.instances.rse_local};
            frequency_hz = rse_lsc_input_hz;
        }

        assert(
            platform.rse_local_system_counter == nil,
            "duplicate object: platform.rse_local_system_counter")
        if not rse_smd_counter_mirror then
            platform.rse_local_system_counter = {
                moduletype = "arm_system_counter";
                input_frequency_hz = rse_lsc_input_hz;
                integer_increment = 1;
                reported_frequency_hz = rse_lsc_input_hz;
                enabled = true;
                initial_count = 0;
            }
        end

        assert(
            platform.rse_lsc_counter == nil,
            "duplicate object: platform.rse_lsc_counter")
        platform.rse_lsc_counter = {
            moduletype = "qemu_sse_counter_mirror";
            dylib_path = "qemu_sse_counter";
            args = {
                ctx.instances.rse_local;
                "&rse_lsc_clock";
                rse_smd_counter_mirror and
                    "&platform.css_system_counter" or
                    "&rse_local_system_counter";
            };
        }

        assert(
            platform.rse_timer_0 == nil,
            "duplicate object: platform.rse_timer_0")
        platform.rse_timer_0 = {
            moduletype = "qemu_sse_timer";
            args = {ctx.instances.rse_local, "&rse_lsc_counter"};
            irq = {bind = "&cpu_0.cpu.nvic.irq_in_" .. RSE_TIMER0_IRQ};
        }

        assert(
            platform.rse_timer_1 == nil,
            "duplicate object: platform.rse_timer_1")
        platform.rse_timer_1 = {
            moduletype = "qemu_sse_timer";
            args = {ctx.instances.rse_local, "&rse_lsc_counter"};
            irq = {bind = "&cpu_0.cpu.nvic.irq_in_" .. RSE_TIMER1_IRQ};
        }

        assert(
            platform.rse_timer_2 == nil,
            "duplicate object: platform.rse_timer_2")
        platform.rse_timer_2 = {
            moduletype = "qemu_sse_timer";
            args = {ctx.instances.rse_local, "&rse_lsc_counter"};
            irq = {bind = "&cpu_0.cpu.nvic.irq_in_" .. RSE_TIMER2_IRQ};
        }

        assert(
            platform.rse_timer_3 == nil,
            "duplicate object: platform.rse_timer_3")
        platform.rse_timer_3 = {
            moduletype = "qemu_sse_timer";
            args = {ctx.instances.rse_local, "&rse_lsc_counter"};
            irq = {bind = "&cpu_0.cpu.nvic.irq_in_" .. RSE_TIMER3_IRQ};
        }

    end

    function block.define_mailbox(ctx, platform)
        assert(
            platform.rse_mhu0_sender_s == nil,
            "duplicate object: platform.rse_mhu0_sender_s")
        platform.rse_mhu0_sender_s = {
            moduletype = "mhu320ae";
            frame = "pbx";
            pair = "rse_ap_monitor_local";
            protocol = "doorbell";
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            target_socket = {
                address = RSE_MHU0_SENDER_SECURE_BASE;
                size = RSE_LOCAL_MHU_FRAME_SIZE;
                bind = "&rse_router.initiator_socket";
            };
            initiator_socket = {bind = "&system_router.target_socket"};
            log_level = 0;
        }

        assert(
            platform.rse_mhu0_receiver_s == nil,
            "duplicate object: platform.rse_mhu0_receiver_s")
        platform.rse_mhu0_receiver_s = {
            moduletype = "mhu320ae";
            frame = "mbx";
            pair = "rse_ap_monitor_local";
            protocol = "doorbell";
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            target_socket = {
                address = RSE_MHU0_RECEIVER_SECURE_BASE;
                size = RSE_LOCAL_MHU_FRAME_SIZE;
                bind = "&rse_router.initiator_socket";
            };
            initiator_socket = {bind = "&system_router.target_socket"};
            irq = {bind = "&rse_cpu_pass.target_signal_socket_" ..
                RSE_CMU_MHU0_RECEIVER_IRQ};
            log_level = 0;
        }

        assert(
            platform.rse_mhu2_sender_s == nil,
            "duplicate object: platform.rse_mhu2_sender_s")
        platform.rse_mhu2_sender_s = {
            moduletype = "mhu320ae";
            frame = "pbx";
            pair = "rse_to_ap_s";
            protocol = "doorbell-bridge";
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            target_socket = {
                address = RSE_MHU2_SENDER_SECURE_BASE;
                size = RSE_LOCAL_MHU_FRAME_SIZE;
                bind = "&rse_router.initiator_socket";
            };
            initiator_socket = {bind = "&system_router.target_socket"};
            log_level = 0;
        }

        assert(
            platform.rse_mhu2_receiver_s == nil,
            "duplicate object: platform.rse_mhu2_receiver_s")
        platform.rse_mhu2_receiver_s = {
            moduletype = "mhu320ae";
            frame = "mbx";
            pair = "ap_s_to_rse";
            protocol = "doorbell-bridge";
            doorbell_commit_on_notify = true;
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            target_socket = {
                address = RSE_MHU2_RECEIVER_SECURE_BASE;
                size = RSE_LOCAL_MHU_FRAME_SIZE;
                bind = "&rse_router.initiator_socket";
            };
            initiator_socket = {bind = "&system_router.target_socket"};
            irq = {bind = "&rse_cpu_pass.target_signal_socket_" ..
                RSE_CMU_MHU2_RECEIVER_IRQ};
            log_level = 0;
        }
    end

    block.define_reset = module.define_reset
    return block
end


    function module.define_reset(ctx, platform)
        local rse_local_boot_flash = ctx.options.rse_local_boot_flash
        local rse_flash_backend = ctx.options.rse_flash_backend
        local rse_smd_counter_mirror = ctx.options.rse_smd_counter_mirror
        local rse_sys_rss_reset_targets = {
            "&rse_cpu_pass.cpu_0.cpu.reset";
            "&rse_cpu_pass.rse_nvic_cold_reset.reset";
            "&rse_cpu_pass.rse_timer_0.reset";
            "&rse_cpu_pass.rse_timer_1.reset";
            "&rse_cpu_pass.rse_timer_2.reset";
            "&rse_gpio_0_cold_reset.reset";
            "&rse_gpio_0.reset";
            "&rse_gpio_1_cold_reset.reset";
            "&rse_gpio_1.reset";
        }
        if rse_local_boot_flash and rse_flash_backend == "qemu-cfi-local" then
            rse_sys_rss_reset_targets[#rse_sys_rss_reset_targets + 1] =
                "&rse_cpu_pass.rse_pflash_cold_reset.reset"
        end

        local rse_aon_reset_targets = {
            "&rse_cpu_pass.rse_timer_3.reset";
        }
        if not rse_smd_counter_mirror then
            rse_aon_reset_targets[#rse_aon_reset_targets + 1] =
                "&rse_cpu_pass.rse_local_system_counter.reset"
        end
        assert(
            platform.rse_sys_rss_reset_fanout == nil,
            "duplicate object: platform.rse_sys_rss_reset_fanout")
        platform.rse_sys_rss_reset_fanout = {
            moduletype = "reset_fanout";
            reset_out = {bind = table.concat(rse_sys_rss_reset_targets, ";")};
            log_level = 0;
        }

        assert(
            platform.rse_aon_reset_fanout == nil,
            "duplicate object: platform.rse_aon_reset_fanout")
        platform.rse_aon_reset_fanout = {
            moduletype = "reset_fanout";
            reset_out = {bind = table.concat(rse_aon_reset_targets, ";")};
            log_level = 0;
        }
    end



return module
