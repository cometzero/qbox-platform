-- Apollo FVP primary-compute Linux direct-boot configuration for QBox.

function top()
    local str = debug.getinfo(2, "S").source:sub(2)
    if str:match("(.*/)")
    then
        return str:match("(.*/)")
    else
        return "./"
    end
end

print("Apollo FVP QBox config running...")

INITIAL_DDR_SPACE = 0x80000000

_KERNEL64_LOAD_ADDR = 0x80080000
_DTB_LOAD_ADDR      = 0x8fc00000
_INITRAMFS_LOAD_ADDR = 0x94000000

dofile(top().."../../../fw/arm64_bootloader.lua")

local function getenv_or(name, default)
    local value = os.getenv(name)
    if value == nil or value == "" then
        return default
    end
    return value
end

local function getenv_number_or(name, default)
    local value = tonumber(getenv_or(name, default))
    assert(value ~= nil, name.." must be numeric")
    return value
end

local function getenv_bool_or(name, default)
    local value = os.getenv(name)
    if value == nil or value == "" then
        return default
    end
    value = string.lower(value)
    return value == "1" or value == "true" or value == "yes" or value == "on"
end

local function repeat_value(value, count)
    local values = {}
    for _=1,count do
        values[#values + 1] = value
    end
    return values
end

local function mp_affinity(cpu_index)
    local cluster = math.floor(cpu_index / 4)
    local core = cpu_index % 4
    return (cluster * 0x10000) + (core * 0x100)
end

local root = top().."../../../../../"
local kernel_image = getenv_or(
    "QBOX_APOLLO_KERNEL",
    root.."build/tmp_baremetal/deploy/images/apollo-fvp/Image")
local dtb_image = getenv_or(
    "QBOX_APOLLO_DTB",
    root.."build/qbox-apollo-fvp/apollo-fvp-direct.dtb")
local initramfs_image = os.getenv("QBOX_APOLLO_INITRAMFS")
local disk_image = os.getenv("QBOX_APOLLO_ROOTFS")
local extra_disk_images = {
    getenv_or("QBOX_APOLLO_EXTRA_BLK1", root.."build/qbox-apollo-fvp/apollo-extra-blk1.raw"),
    getenv_or("QBOX_APOLLO_EXTRA_BLK2", root.."build/qbox-apollo-fvp/apollo-extra-blk2.raw"),
    getenv_or("QBOX_APOLLO_EXTRA_BLK3", root.."build/qbox-apollo-fvp/apollo-extra-blk3.raw"),
}
local netdev = getenv_or("QBOX_APOLLO_NETDEV", "type=user,hostfwd=tcp::2222-:22")
local pcie_irq_test_enabled =
    getenv_bool_or("QBOX_APOLLO_PCIE_IRQ_TEST", false)
local fault_event_test_enabled =
    getenv_bool_or("QBOX_APOLLO_FAULT_EVENT_TEST", false)

if ACCEL == nil then
    ACCEL = getenv_or("QBOX_APOLLO_ACCEL", "tcg")
end

local ARM_NUM_CPUS = getenv_number_or("QBOX_APOLLO_NUM_CPUS", "16")
assert(ARM_NUM_CPUS >= 1 and ARM_NUM_CPUS <= 16, "QBOX_APOLLO_NUM_CPUS must be 1..16")
local pc_trace = getenv_bool_or("QBOX_APOLLO_PC_TRACE", false)
local pc_trace_file = getenv_or(
    "QBOX_APOLLO_PC_TRACE_FILE",
    root.."build/qbox-apollo-fvp/cpu-pc-trace.log")
local pc_trace_interval = getenv_number_or("QBOX_APOLLO_PC_TRACE_INTERVAL", "1")
local pc_trace_limit = getenv_number_or("QBOX_APOLLO_PC_TRACE_LIMIT", "4096")
local exception_trace = getenv_bool_or("QBOX_APOLLO_EXCEPTION_TRACE", false)
local gdb_port_base = getenv_number_or("QBOX_APOLLO_GDB_PORT_BASE", "0")
assert(gdb_port_base >= 0 and gdb_port_base <= 65535, "QBOX_APOLLO_GDB_PORT_BASE must be 0..65535")
if gdb_port_base ~= 0 then
    assert(false, "QBOX_APOLLO_GDB_PORT_BASE is unsupported; use QBOX_APOLLO_GDB_CPU_INDEX and QBOX_APOLLO_GDB_PORT")
end
local gdb_cpu_index = getenv_number_or("QBOX_APOLLO_GDB_CPU_INDEX", "-1")
local gdb_port = getenv_number_or("QBOX_APOLLO_GDB_PORT", "0")
assert(gdb_cpu_index >= -1 and gdb_cpu_index < ARM_NUM_CPUS, "QBOX_APOLLO_GDB_CPU_INDEX must be -1 or in CPU range")
assert(gdb_port >= 0 and gdb_port <= 65535, "QBOX_APOLLO_GDB_PORT must be 0..65535")
if gdb_port == 0 then
    assert(gdb_cpu_index == -1, "QBOX_APOLLO_GDB_PORT must be nonzero when QBOX_APOLLO_GDB_CPU_INDEX is set")
else
    assert(gdb_cpu_index >= 0, "QBOX_APOLLO_GDB_CPU_INDEX must be set when QBOX_APOLLO_GDB_PORT is nonzero")
end
local GIC_REDIST_BASE = 0x20880000
local GIC_REDIST_SIZE = 0x40000
local ARCH_TIMER_VIRT_IRQ = 16 + 11
local ARCH_TIMER_S_EL1_IRQ = 16 + 13
local ARCH_TIMER_NS_EL1_IRQ = 16 + 14
local ARCH_TIMER_NS_EL2_IRQ = 16 + 10

platform = {

    moduletype="Container";

    quantum_ns = 10000000;

    -- Fabric
    router = {
        moduletype="router";
        log_level=0;
    },

    keep_alive_0 = {
        moduletype = "keep_alive";
    },

    -- Primary-compute memory map
    ram_0 = {
        moduletype="gs_memory";
        target_socket = {
            address = INITIAL_DDR_SPACE;
            size = 0x7f000000;
            bind = "&router.initiator_socket"
        };
        log_level=0,
    };

    ram_1 = {
        moduletype="gs_memory";
        target_socket = {
            address = 0x20000000000;
            size = 0x80000000;
            bind = "&router.initiator_socket"
        };
        log_level=0,
    };

    sram_0 = {
        moduletype="gs_memory";
        target_socket = {
            address = 0x00180000;
            size = 0x00001000;
            bind = "&router.initiator_socket"
        };
        log_level=0,
    };

    si_cl1_rproc_rsctbl_0 = {
        moduletype="gs_memory";
        target_socket = {
            address = 0x00100000;
            size = 0x00020000;
            bind = "&router.initiator_socket"
        };
        log_level=0,
    };

    si_cl1_vdev0vring0_0 = {
        moduletype="gs_memory";
        target_socket = {
            address = 0x00120000;
            size = 0x00020000;
            bind = "&router.initiator_socket"
        };
        log_level=0,
    };

    si_cl1_vdev0vring1_0 = {
        moduletype="gs_memory";
        target_socket = {
            address = 0x00140000;
            size = 0x00020000;
            bind = "&router.initiator_socket"
        };
        log_level=0,
    };

    si_cl1_vdev0buffer_0 = {
        moduletype="gs_memory";
        target_socket = {
            address = 0x00160000;
            size = 0x00020000;
            bind = "&router.initiator_socket"
        };
        log_level=0,
    };

    ras_buffer_0 = {
        moduletype="gs_memory";
        target_socket = {
            address = 0xffa00000;
            size = 0x00100000;
            bind = "&router.initiator_socket"
        };
        log_level=0,
    };

    -- AP CPU backend
    qemu_inst_mgr = {
        moduletype = "QemuInstanceManager";
        construction_priority = -300;
    },

    qemu_inst = {
        moduletype="QemuInstance";
        args = {"&platform.qemu_inst_mgr", "AARCH64"};
        accel = ACCEL,
        qemu_args = getenv_or("QBOX_APOLLO_QEMU_ARGS", ""),
        tcg_mode = getenv_or("QBOX_APOLLO_TCG_MODE", "MULTI"),
        sync_policy = getenv_or("QBOX_APOLLO_SYNC_POLICY", "multithread-unconstrained"),
        construction_priority = -299
    },

    -- Interrupt controller
    gic_0 = {
        moduletype = "arm_gicv3",
        args = {"&platform.qemu_inst"},
        dist_iface = {
            address = 0x20800000,
            size = 0x10000,
            bind = "&router.initiator_socket"
        };
        num_cpus = ARM_NUM_CPUS,
        redist_region = repeat_value(1, ARM_NUM_CPUS);
        has_lpi = true;
        revision = 4;
        num_spi = 960;
        has_gicv4_1 = true;
        has_direct_lpi = true;
        has_rvpeid = true;
        has_vpend_valid_dirty = true;
        vpeid_bits = 16;
    };

    its_0 = {
        moduletype = "arm_gicv3_its",
        args = {"&platform.qemu_inst", "&platform.gic_0"};
        has_gicv4_1 = true;
        gicv4_1_svpet = 1;
        gicv4_1_cte_size = 8;
        mem = {
            address = 0x20840000,
            size = 0x40000,
            bind = "&router.initiator_socket"
        };
    };

    smmu_0 = {
        moduletype = "smmuv3";
        pamax = 48;
        sidsize = 8;
        ato = false;
        num_tbu = 1;
        iidr = 0x720AE000;
        target_socket = {
            address = 0x1c0000000;
            size = 0x08000000;
            bind = "&router.initiator_socket";
        };
        dma = {bind = "&router.target_socket"};
        irq_eventq = {
            bind = fault_event_test_enabled and
                "&smmu_event_fanout.signal_in" or
                "&gic_0.spi_in_65";
        };
    };

    smmu_event_fanout = fault_event_test_enabled and {
        moduletype = "signal_fanout";
        signal_out = {
            bind = "&gic_0.spi_in_65;&smmu_fault_observer.fault_in";
        };
    } or nil;

    smmu_fault_observer = fault_event_test_enabled and {
        moduletype = "zena_fmu";
        bank_count = 1;
        record_count = 2;
        enforce_sys_key = false;
        fault_input_enabled = true;
        fault_input_record = 1;
        fault_source = "smmu_0.irq_eventq";
        fault_id = "smmuv3-eventq";
        fault_sink = "gic_0.spi_in_65";
        event_log = getenv_or("QBOX_APOLLO_FAULT_EVENT_LOG", "");
        log_level = 0;
    } or nil;

    smmu_lti00 = {
        moduletype = "smmuv3_tbu";
        args = {"&platform.smmu_0"};
        topology_id = 0x40;
        upstream_socket = {};
        downstream_socket = {bind = "&router.target_socket"};
    };

    gpex_0 = {
        moduletype = "qemu_gpex";
        args = {"&platform.qemu_inst"};
        request_origin_id = 0x1100;
        request_domain_id = 1;
        requester_id = 0x40;
        bus_master = {bind = "&smmu_lti00.upstream_socket"};
        pio_iface = {
            address = 0x60200000;
            size = 0x00100000;
            bind = "&router.initiator_socket";
        };
        mmio_iface = {
            address = 0x60300000;
            size = 0x1fd00000;
            bind = "&router.initiator_socket";
        };
        ecam_iface = {
            address = 0x43b50000;
            size = 0x10000000;
            bind = "&router.initiator_socket";
        };
        mmio_iface_high = {
            address = 0x400000000;
            size = 0x200000000;
            bind = "&router.initiator_socket";
        };
        irq_out_0 = {bind = "&gic_0.spi_in_300"};
        irq_out_1 = {bind = "&gic_0.spi_in_301"};
        irq_out_2 = {bind = "&gic_0.spi_in_302"};
        irq_out_3 = {bind = "&gic_0.spi_in_303"};
    };

    pcie_irq_test_endpoint = pcie_irq_test_enabled and {
        moduletype = "virtio_net_pci";
        args = {"&platform.qemu_inst", "&platform.gpex_0"};
        addr = "01.0";
        mac = "52:54:00:12:34:56";
        netdev_str = "type=user";
    } or nil;

    -- RoS peripherals visible to Linux
    virtioblk_1 = {
        moduletype = "virtio_mmio_blk",
        args = {"&platform.qemu_inst"};
        mem = {
            address = 0x30030000,
            size = 0x10000,
            bind = "&router.initiator_socket",
            mirror_4k_aperture = true
        },
        irq_out = {bind = "&gic_0.spi_in_258"},
        blkdev_str = "file="..extra_disk_images[1]..",format=raw,if=none,cache=writeback"
    };

    virtioblk_2 = {
        moduletype = "virtio_mmio_blk",
        args = {"&platform.qemu_inst"};
        mem = {
            address = 0x30040000,
            size = 0x10000,
            bind = "&router.initiator_socket",
            mirror_4k_aperture = true
        },
        irq_out = {bind = "&gic_0.spi_in_259"},
        blkdev_str = "file="..extra_disk_images[2]..",format=raw,if=none,cache=writeback"
    };

    virtioblk_3 = {
        moduletype = "virtio_mmio_blk",
        args = {"&platform.qemu_inst"};
        mem = {
            address = 0x30050000,
            size = 0x10000,
            bind = "&router.initiator_socket",
            mirror_4k_aperture = true
        },
        irq_out = {bind = "&gic_0.spi_in_260"},
        blkdev_str = "file="..extra_disk_images[3]..",format=raw,if=none,cache=writeback"
    };

    virtionet0_0 = {
        moduletype = "virtio_mmio_net",
        args = {"&platform.qemu_inst"};
        mem = {
            address = 0x30060000,
            size = 0x10000,
            bind = "&router.initiator_socket",
            mirror_4k_aperture = true
        },
        irq_out = {bind = "&gic_0.spi_in_261"},
        netdev_str = netdev
    };

    virtiorng_0 = {
        moduletype = "virtio_mmio_rng",
        args = {"&platform.qemu_inst"};
        mem = {
            address = 0x30080000,
            size = 0x10000,
            bind = "&router.initiator_socket",
            mirror_4k_aperture = true
        },
        irq_out = {bind = "&gic_0.spi_in_263"}
    };

    rtc_0 = {
        moduletype = "pl031",
        args = {"&platform.qemu_inst"};
        mem = {
            address = 0x300d0000,
            size = 0x1000,
            bind = "&router.initiator_socket"
        },
        irq_out = {bind = "&gic_0.spi_in_268"}
    };

    watchdog_0 = {
        moduletype = "sbsa_gwdt",
        args = {"&platform.qemu_inst"};
        refresh_mem = {
            address = 0x1a420000,
            size = 0x1000,
            bind = "&router.initiator_socket"
        },
        control_mem = {
            address = 0x1a430000,
            size = 0x1000,
            bind = "&router.initiator_socket"
        },
        irq_out = {bind = "&gic_0.spi_in_50"}
    };

    -- Console
    charbackend_stdio_0 = {
        moduletype = "char_backend_stdio";
        read_write = true;
    };

    pl011_uart_0 = {
        moduletype = "Pl011",
        dylib_path = "uart-pl011",
        target_socket = {
            address = 0x1a400000,
            size = 0x10000,
            bind = "&router.initiator_socket"
        },
        irq = {bind = "&gic_0.spi_in_52"},
        backend_socket = {bind = "&charbackend_stdio_0.biflow_socket"},
    };

    global_peripheral_initiator_arm_0 = {
        moduletype = "global_peripheral_initiator",
        args = {"&platform.qemu_inst", "&platform.cpu_0"},
        global_initiator = {bind = "&router.target_socket"},
    };

    -- Catch-all decode
    fallback_0 = {
        moduletype="gs_memory";
        target_socket = {
            address = 0x0;
            size = 0x800000000,
            bind = "&router.initiator_socket",
            priority = 1
        },
        dmi_allow = false,
        log_level = 0,
    };

    -- Boot artifacts
    load = {
        moduletype = "loader",
        initiator_socket = {bind = "&router.target_socket"};
        { bin_file = kernel_image, address = _KERNEL64_LOAD_ADDR };
        { bin_file = dtb_image, address = _DTB_LOAD_ADDR };
        { data = _bootloader_aarch64, address = INITIAL_DDR_SPACE };
    };
};

for i=0,(ARM_NUM_CPUS-1) do
    platform.gic_0["redist_iface_"..i] = {
        address = GIC_REDIST_BASE + (i * GIC_REDIST_SIZE),
        size = GIC_REDIST_SIZE,
        bind = "&router.initiator_socket"
    };
end

if initramfs_image ~= nil and initramfs_image ~= "" then
    table.insert(platform.load, {
        bin_file = initramfs_image,
        address = _INITRAMFS_LOAD_ADDR
    })
end

if disk_image ~= nil and disk_image ~= "" then
    platform.virtioblk_0 = {
        moduletype = "virtio_mmio_blk",
        args = {"&platform.qemu_inst"};
        mem = {
            address = 0x30020000,
            size = 0x10000,
            bind = "&router.initiator_socket",
            mirror_4k_aperture = true
        },
        irq_out = {bind = "&gic_0.spi_in_257"},
        blkdev_str = "file="..disk_image..",format=raw,if=none,cache=writeback"
    };
end

print("kernel is loaded at: 0x"..string.format("%x", _KERNEL64_LOAD_ADDR));
print("dtb is loaded at:    0x"..string.format("%x", _DTB_LOAD_ADDR));
if initramfs_image ~= nil and initramfs_image ~= "" then
    print("initramfs is loaded at: 0x"..string.format("%x", _INITRAMFS_LOAD_ADDR));
end
print("kernel image: "..kernel_image);
print("dtb image:    "..dtb_image);
if initramfs_image ~= nil and initramfs_image ~= "" then
    print("initramfs:    "..initramfs_image);
end
if disk_image ~= nil and disk_image ~= "" then
    print("disk image:   "..disk_image);
end
for i=1,#extra_disk_images do
    print("extra disk "..i..": "..extra_disk_images[i]);
end
print("ap cpus:      "..tostring(ARM_NUM_CPUS));
print("PC trace:     "..tostring(pc_trace));
if pc_trace then
    print("PC trace log: "..pc_trace_file);
end
if gdb_port ~= 0 then
    print("GDB CPU:      "..tostring(gdb_cpu_index));
    print("GDB port:     "..tostring(gdb_port));
else
    print("GDB port:     disabled");
end
print("accel:        "..ACCEL);

local psci_conduit = "smc";
if ACCEL == "kvm" then
    psci_conduit = "hvc";
end
print("PSCI conduit: "..psci_conduit)

-- CPU cluster and GIC CPU interfaces
for i=0,(ARM_NUM_CPUS-1) do
    local cpu = {
        moduletype = "cpu_arm_cortexA720AE";
        args = {"&platform.qemu_inst"};
        mem = {bind = "&router.target_socket"};
        has_el3 = false;
        has_el2 = false;
        irq_timer_phys_out = {
            bind = "&gic_0.ppi_in_cpu_"..i.."_"..ARCH_TIMER_NS_EL1_IRQ
        },
        irq_timer_virt_out = {
            bind = "&gic_0.ppi_in_cpu_"..i.."_"..ARCH_TIMER_VIRT_IRQ
        },
        irq_timer_hyp_out = {
            bind = "&gic_0.ppi_in_cpu_"..i.."_"..ARCH_TIMER_NS_EL2_IRQ
        },
        irq_timer_sec_out = {
            bind = "&gic_0.ppi_in_cpu_"..i.."_"..ARCH_TIMER_S_EL1_IRQ
        },
        gicv3_maintenance_interrupt = {
            bind = "&gic_0.ppi_in_cpu_"..i.."_25"
        },
        pmu_interrupt = {bind = "&gic_0.ppi_in_cpu_"..i.."_23"},
        psci_conduit = psci_conduit,
        mp_affinity = mp_affinity(i);
        start_powered_off = true;
        rvbar = INITIAL_DDR_SPACE;
        construction_priority = -200 + i;
        trace_pc = pc_trace;
        trace_pc_file = pc_trace_file;
        trace_pc_interval = pc_trace_interval;
        trace_pc_limit = pc_trace_limit;
        trace_exception_state = exception_trace;
    };
    if i == 0 then
        cpu["start_powered_off"] = false;
    end
    if gdb_port ~= 0 and i == gdb_cpu_index then
        cpu["gdb_port"] = gdb_port;
    end
    platform["cpu_"..tostring(i)] = cpu;

    platform["gic_0"]["irq_out_"..i] = {bind = "&cpu_"..i..".irq_in"}
    platform["gic_0"]["fiq_out_"..i] = {bind = "&cpu_"..i..".fiq_in"}
    platform["gic_0"]["virq_out_"..i] = {bind = "&cpu_"..i..".virq_in"}
    platform["gic_0"]["vfiq_out_"..i] = {bind = "&cpu_"..i..".vfiq_in"}
end
