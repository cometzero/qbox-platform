-- SPDX-License-Identifier: BSD-3-Clause
-- Direct SI CL1 boot of the unmodified integrated Zephyr binary.
-- The loader replaces firmware placement/reset release; AP/RSE/SCP are absent.
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
        input_frequency_hz = 125000000;
        reported_frequency_hz = 125000000;
        integer_increment = 1;
        construction_priority = -298;
    };
}
local hw = ctx.hardware.si_cl1
-- Keep the integrated board UART, timer, mailboxes and shared-memory map.
hw.interconnect.define(ctx, platform)
platform.si_cl1_hipc_bridge = nil
local instance = hw.cpu.define_qemu_instance(ctx, platform)
hw.memory.define(ctx, platform)
hw.system.define_interrupt_controller(ctx, platform, instance)
hw.io_peri.define(ctx, platform)
hw.mailbox.define(ctx, platform)
hw.cpu.define(ctx, platform, instance)
-- POST_KERNEL PFDI discovery needs replies before the shell can start.
-- Use the existing functional SCMI responder instead of an absent SCP peer.
local pfdi = platform.si_cl1_pfdi_mhu_pbx
pfdi.protocol = "scmi"
pfdi.pair = "zephyr_direct_pfdi"
pfdi.requester_hold_enable = false
for i = 0,3 do
    pfdi["requester_hold_" .. i] = nil
end
platform.si_cl1_pfdi_reply_mhu_mbx.pair = "zephyr_direct_pfdi"
-- The AP peer is offline; preserve the addressable HIPC buffer without
-- claiming an RPMsg handshake or synthesizing successful peer responses.
platform.si_cl1_hipc_shmem = {
    moduletype = "gs_memory";
    target_socket = {address = 0xe0130000; size = 0x80000;
                     bind = "&si_cl1_router.initiator_socket"};
}
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
    cpu.cntfrq_hz = 125000000
    -- Integrated Zephyr DTS assigns CNTP to PPI29 and CNTV to PPI27.
    cpu.irq_timer_phys_out = {bind = "&si_cl1_gic.ppi_in_cpu_" .. i .. "_29"}
    cpu.irq_timer_sec_out = {bind = "&si_cl1_gic.ppi_in_cpu_" .. i .. "_20"}
end
dofile(dir .. "vp/si_cl1/qemu.lua").configure(ctx, platform)
platform.si_cl1_qemu_inst.managed_start_in_reset_release = false
dofile(dir .. "vp/si_cl1/console.lua").create(ctx, runtime).define(ctx, platform)
platform.si_cl1_uart.backend_socket = {bind = "&si_cl1_console_file.biflow_socket"}
dofile(dir .. "vp/si_cl1/boot.lua").create(ctx, runtime).define_loader(
    ctx, platform, runtime.si_cl1_image)
