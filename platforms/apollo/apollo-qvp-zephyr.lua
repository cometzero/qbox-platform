-- SPDX-License-Identifier: BSD-3-Clause
-- Standalone SI CL1: no RSE/SCP/PPU or AP handoff is required.
local dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
local ctx = dofile(dir .. "apollo-qvp-common.lua").load(dir)
local runtime = ctx.config.si_cl1
runtime.entry = assert(tonumber(os.getenv("QBOX_ZEPHYR_ENTRY")), "QBOX_ZEPHYR_ENTRY required")
platform = {
    moduletype = "Container";
    quantum_ns = 100000;
    keep_alive_0 = {moduletype = "keep_alive"};
    css_system_counter = {
        moduletype = "arm_system_counter";
        input_frequency_hz = 100000000;
        reported_frequency_hz = 100000000;
        integer_increment = 1;
        construction_priority = -298;
    };
}
local hw = ctx.hardware.si_cl1
-- Compose only the devices used by the standalone board.
hw.interconnect.define(ctx, platform)
platform.si_cl1_hipc_bridge = nil
local instance = hw.cpu.define_qemu_instance(ctx, platform)
hw.memory.define(ctx, platform)
hw.system.define_interrupt_controller(ctx, platform, instance)
hw.io_peri.define(ctx, platform)
hw.cpu.define(ctx, platform, instance)
platform.si_cl1_scmi_shmem = nil
for name, address in pairs({bench_sram = 0x40000000; bench_nc_sram = 0x48000000}) do
    platform[name] = {
        moduletype = "gs_memory";
        target_socket = {address = address; size = 0x800000;
                         bind = "&si_cl1_router.initiator_socket"};
    }
end
local gic = platform.si_cl1_gic
gic.has_multi_view = false
gic.has_security_extensions = false
gic.dist_iface = {address = 0x30200000; size = 0x10000;
                  bind = "&si_cl1_router.initiator_socket"}
for i = 0,3 do
    gic["redist_iface_" .. i] = {address = 0x30260000 + i * 0x20000;
        size = 0x20000; bind = "&si_cl1_router.initiator_socket"}
    local cpu = platform["si_cl1_cpu_" .. i]
    cpu.rvbar = runtime.entry
    cpu.start_in_reset = false
    cpu.cntfrq_hz = 100000000
    -- Standalone reference board assigns CNTP to PPI29 and CNTV to PPI27.
    cpu.irq_timer_phys_out = {bind = "&si_cl1_gic.ppi_in_cpu_" .. i .. "_29"}
    cpu.irq_timer_sec_out = {bind = "&si_cl1_gic.ppi_in_cpu_" .. i .. "_20"}
end
dofile(dir .. "vp/si_cl1/qemu.lua").configure(ctx, platform)
platform.si_cl1_qemu_inst.managed_start_in_reset_release = false
dofile(dir .. "vp/si_cl1/console.lua").create(ctx, runtime).define(ctx, platform)
platform.si_cl1_uart.backend_socket = {bind = "&si_cl1_console_file.biflow_socket"}
dofile(dir .. "vp/si_cl1/boot.lua").create(ctx, runtime).define_loader(
    ctx, platform, runtime.si_cl1_image)
