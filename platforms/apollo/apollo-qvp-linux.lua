-- SPDX-License-Identifier: BSD-3-Clause
-- AP-only profile: direct Linux or optional standalone U-Boot/UKIBoot first boot.
-- RSE, SCP, Zephyr and TF-A remain absent; whole-platform EFI reset is not wired.
local apollo_dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
local common = dofile(apollo_dir.."apollo-qvp-common.lua")
local ctx = common.load(apollo_dir)
assert(enable_ap_cpus, "Linux profile requires QBOX_RDASPEN_ENABLE_AP_CPUS=true")
assert(not ctx.config.runtime_injection.enabled,
       "Full-system runtime injection is not supported by the Linux profile")

platform = {
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
platform.host_smd_gpio = {
    moduletype = "qemu_pl061";
    args = {"&platform.ap_qemu_inst"};
    mem = {address = 0x40750000; size = 0x10000;
           bind = "&ap_router.initiator_socket"};
    irq = {bind = "&ap_gic.spi_in_193"};
}
common.connect_board(ctx, platform)

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
platform.ap_secure_console_file.write_file = primary_console_log..".secure"
platform.ap_secure_console_file.read_file = "/dev/null"
platform.ap_secure_console_file.poll_read = false
platform.ap_qemu_inst.managed_start_in_reset_release = false
-- There is no secure firmware to assign interrupt groups in this profile.
platform.ap_gic.has_security_extensions = false
for i=0,AP_NUM_CPUS-1 do
    local cpu = platform["ap_cpu_"..i]
    cpu.start_in_reset = false
    cpu.rvbar = 0x80000000
    cpu.psci_conduit = "smc"
    cpu.linux_smc_stub_address = 0x0f000000
    cpu.ras_uncontainable_interrupt = nil
end

-- One WIC disk for either product or BSP. BSP still uses initramfs as root.
for i=1,3 do platform["ap_virtioblk_"..i] = nil end
if ctx.getenv_or("QBOX_RDASPEN_ROOTFS", "") == "" then
    platform.ap_virtioblk_0 = nil
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
if smmu_backend == "systemc-mmu720ae" then
    platform.ap_gpex_0.bus_master = {bind = "&ap_smmu_lti00.upstream_socket"}
end

local function required(name)
    local path = ctx.getenv_or(name, "")
    assert(path ~= "", "Missing Linux boot artifact: "..name)
    return path
end
local firmware = ctx.getenv_or("QBOX_LINUX_FIRMWARE", "false") == "true"
platform.linux_loader = {
    moduletype = "loader";
    initiator_socket = {bind = "&ap_router.target_socket"};
    {bin_file = required("QBOX_LINUX_BOOT_STUB"); address = 0x80000000};
    {bin_file = required("QBOX_LINUX_KERNEL"); address = firmware and 0x80080000 or 0x80200000};
    {bin_file = required("QBOX_LINUX_DTB"); address = 0x88000000};
}
local initrd = ctx.getenv_or("QBOX_LINUX_INITRD", "")
if initrd ~= "" then
    assert(not firmware, "EFI firmware mode takes its initrd from the on-disk UKI")
    table.insert(platform.linux_loader, {bin_file = initrd; address = 0x90000000})
end
dofile(apollo_dir.."linux-boot/domains.lua").define(ctx, platform)
print("Apollo QVP Linux-only profile: SystemC domain mocks enabled")
