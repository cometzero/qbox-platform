-- Shared AP hardware geometry retained during per-file constant migration.
-- Single-owner Address/Size/IRQ values live in their consuming Lua modules.
local map = {}

function map.create(options)
    local AP_ADDRESS = {
        shared_sram = 0x00000000;
        flash = 0x38000000;
        fmu_cl0 = 0x1D000000;
        fmu_cl1 = 0x1D100000;
        fmu_cl2 = 0x1D200000;
        fmu_cl3 = 0x1D300000;
        gic_redist = 0x20880000;
    }
    local AP_SIZE = {
        flash = 0x08000000;
        uart = 0x00010000;
        gic_dist = 0x00010000;
        gic_redist = 0x00040000;
        register_window = 0x00010000;
    }
    local AP_IRQ = {
        smmu_event = 65;
    }
    local AP_HW = {
        max_cpus = 16;
        gpex_requester_id = 0x40;
        smmu_fault_bank_count = 1;
        smmu_fault_record_count = 2;
        smmu_pamax = 52;
        smmu_sid_size = 32;
        smmu_tbu_count = 1;
        smmu_iidr = 0x720AE000;
        gic_redist_regions = 16;
        gic_active_redist_regions = options.AP_GIC_NUM_CPUS;
        gic_view0_redist_regions = 16;
        gic_revision = 4;
        gic_num_spi = 960;
        gic_vpeid_bits = 16;
        gic_its_svpet = 1;
        gic_its_cte_size = 2;
        arch_timer_frequency_hz = 125000000;
        arch_timer_frame_count = 2;
        fmu_bank_count = 5;
        fmu_record_count = 384;
        uart_revision = 3;
        sid_system_id = 0x0047773d;
        sid_soc_id = 0x00000000;
        sid_chip_id = 0x00000000;
        sid_pidr4 = 0x00000004;
        sid_pidr0 = 0x0000003d;
        sid_pidr1 = 0x000000b7;
        sid_pidr2 = 0x0000000b;
        sid_pidr3 = 0x00000000;
        sid_cidr0 = 0x0000000d;
        sid_cidr1 = 0x000000f0;
        sid_cidr2 = 0x00000005;
        sid_cidr3 = 0x000000b1;
    }
    return { AP_ADDRESS = AP_ADDRESS; AP_SIZE = AP_SIZE; AP_IRQ = AP_IRQ; AP_HW = AP_HW; }
end

return map
