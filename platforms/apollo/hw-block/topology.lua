local SI_SINGLE_GIC_ENV = "QBOX_APOLLO_FULL_SI_SINGLE_GIC"

local function strict_bool_env(name, default)
    local value = os.getenv(name)
    if value == nil or value == "" then
        return default
    end
    if value == "true" or value == "1" or value == "yes" then
        return true
    end
    if value == "false" or value == "0" or value == "no" then
        return false
    end
    error(name.." must be true, false, 1, 0, yes, or no")
end

local si_single_gic = strict_bool_env(SI_SINGLE_GIC_ENV, false)

local function single_si_value(suffix, default)
    return os.getenv("QBOX_APOLLO_FULL_SI_"..suffix) or
        os.getenv("QBOX_APOLLO_FULL_SI_CL0_"..suffix) or
        default
end

local si_qemu_instances
if si_single_gic then
    si_qemu_instances = {
        {
            name = "si_qemu_inst"; domain = "si"; architecture = "AARCH64";
            cpu = "cortex-r82";
            acceleration = single_si_value("ACCEL", "tcg");
            tcg_mode = single_si_value("TCG_MODE", "MULTI");
            sync_policy = single_si_value(
                "SYNC_POLICY",
                "multithread-quantum");
            ram_owner = "systemc";
        };
    }
else
    si_qemu_instances = {
        {
            name = "si_cl0_qemu_inst"; domain = "si_cl0";
            architecture = "AARCH64"; cpu = "cortex-r82";
            acceleration = "tcg"; tcg_mode = "MULTI";
            sync_policy = "multithread-quantum"; ram_owner = "systemc";
        };
        {
            name = "si_cl1_qemu_inst"; domain = "si_cl1";
            architecture = "AARCH64"; cpu = "cortex-r82";
            acceleration = "tcg"; tcg_mode = "MULTI";
            sync_policy = "multithread-quantum"; ram_owner = "systemc";
            scope = "fvp_cfg2_extension";
        };
    }
end

local si_pe_instance = si_single_gic and "si_qemu_inst" or nil
local safety_island_gics
if si_single_gic then
    safety_island_gics = {
        {
            name = "si_cl0_gic"; qemu_instance = "si_qemu_inst";
            redistributor_regions = {1, 4};
            cpu_interfaces = 5; normal_spi_count = 960;
            state_owner = "qemu"; canonical = true;
        };
    }
else
    safety_island_gics = {
        {
            name = "si_cl0_gic"; qemu_instance = "si_cl0_qemu_inst";
            redistributor_regions = {1};
            cpu_interfaces = 1; normal_spi_count = 384;
            state_owner = "qemu";
        };
        {
            name = "si_cl1_gic"; qemu_instance = "si_cl1_qemu_inst";
            redistributor_regions = {1, 1, 1, 1};
            cpu_interfaces = 4; normal_spi_count = 128;
            state_owner = "qemu"; scope = "fvp_cfg2_extension";
        };
    }
end

local safety_island_pes = {
    {
        pe = 0; name = "si_cl0_cpu_0"; cluster = "si_cl0";
        qemu_instance = si_pe_instance or "si_cl0_qemu_inst";
        mp_affinity = 0x00000; affinity = "0.0.0.0";
        image = "CL0"; image_loader = "si_cl0_loader";
        router = "si_cl0_ni710ae_primary_nci.protected_target_socket";
        reset = si_single_gic and
            "si_cl0_cpu_0_reset" or "si_cl0_qemu_inst";
    };
}
for cpu=0,3 do
    safety_island_pes[#safety_island_pes + 1] = {
        pe = cpu + 1; name = "si_cl1_cpu_"..cpu; cluster = "si_cl1";
        qemu_instance = si_pe_instance or "si_cl1_qemu_inst";
        mp_affinity = 0x10000 + (cpu * 0x100);
        affinity = "0.1."..cpu..".0";
        image = "CL1"; image_loader = "si_cl1_loader";
        router = "si_cl1_router.target_socket";
        reset = si_single_gic and
            "si_cl1_cpu_"..cpu.."_reset" or "si_cl1_qemu_inst";
    }
end

local safety_island_reset_targets = {}
local reset_target_seen = {}
local function add_reset_target(target)
    if not reset_target_seen[target] then
        safety_island_reset_targets[#safety_island_reset_targets + 1] = target
        reset_target_seen[target] = true
    end
end

add_reset_target("&si_gic_multiview.reset")
if si_single_gic then
    add_reset_target("&si_gic_power_bridge.reset")
    add_reset_target("&si_gic_reset.reset")
    for _, pe in ipairs(safety_island_pes) do
        add_reset_target("&"..pe.reset..".reset")
    end
else
    for _, instance in ipairs(si_qemu_instances) do
        add_reset_target("&"..instance.name..".reset")
    end
end
add_reset_target("&ap_rgic2lgic_messreg.reset")

local safety_island_trace_targets = {}
for _, instance in ipairs(si_qemu_instances) do
    safety_island_trace_targets[#safety_island_trace_targets + 1] = {
        kind = "qemu_instance";
        name = instance.name;
        domain = instance.domain;
    }
end
for _, pe in ipairs(safety_island_pes) do
    safety_island_trace_targets[#safety_island_trace_targets + 1] = {
        kind = "pe";
        name = pe.name;
        qemu_instance = pe.qemu_instance;
        pe = pe.pe;
        affinity = pe.affinity;
        image = pe.image;
    }
end
for _, gic in ipairs(safety_island_gics) do
    safety_island_trace_targets[#safety_island_trace_targets + 1] = {
        kind = "gic";
        name = gic.name;
        qemu_instance = gic.qemu_instance;
        cpu_interfaces = gic.cpu_interfaces;
    }
end

local safety_island_contract = {
    env_var = SI_SINGLE_GIC_ENV;
    mode = si_single_gic and "single" or "split";
    enabled = si_single_gic;
    qemu_instances = si_qemu_instances;
    pes = safety_island_pes;
    gics = safety_island_gics;
    reset_targets = safety_island_reset_targets;
    trace_targets = safety_island_trace_targets;
    rollback_command =
        "python3 scripts/run/run_qbox_apollo_fvp_full.py --si-split-gic";
}

local topology = {
    schema_version = 1;
    machine = "apollo-qvp";
    variant = "cfg2";
    si_gic_mode = si_single_gic and "single" or "split";
    scopes = {
        { name = "zena_css_architecture"; source = "doc/arm_zena_css_dev_guide/02-block-diagram-for-zena-css.md" };
        { name = "rd_aspen_cfg2"; source = "arm-zena-css/documentation/images/aspen_high_level_arch.png" };
        { name = "fvp_cfg2_extension"; source = "doc/arm_zena_css_dev_guide/08-fixed-virtual-platform.md" };
        { name = "qvp_functional_abstraction"; source = "doc/apollo-qvp-machine-architecture-ko.md" };
    };
    domains = {
        { name = "system"; owner = "rse"; scope = "zena_css_architecture" };
        { name = "ap"; owner = "ap"; scope = "zena_css_architecture" };
        { name = "smd"; owner = "si_cl0"; scope = "zena_css_architecture" };
        { name = "rse"; owner = "rse"; scope = "zena_css_architecture" };
        { name = "si_cl0"; owner = "si_cl0"; scope = "zena_css_architecture" };
        { name = "si_cl1"; owner = "si_cl1"; scope = "fvp_cfg2_extension" };
    };
    views = {
        { name = "system"; domain = "system"; width = 52; scope = "zena_css_architecture" };
        { name = "ap"; domain = "ap"; width = 52; scope = "zena_css_architecture" };
        { name = "smd"; domain = "smd"; width = 52; scope = "zena_css_architecture" };
        { name = "rse"; domain = "rse"; width = 32; scope = "zena_css_architecture" };
        { name = "si_cl0"; domain = "si_cl0"; width = 40; scope = "zena_css_architecture" };
        { name = "si_cl1"; domain = "si_cl1"; width = 40; scope = "fvp_cfg2_extension" };
    };
    routers = {
        { name = "system_router"; view = "system"; cci_path = "platform.system_router"; scope = "zena_css_architecture" };
        { name = "ap_router"; view = "ap"; cci_path = "platform.ap_router"; scope = "zena_css_architecture" };
        { name = "smd_router"; view = "smd"; cci_path = "platform.smd_router"; scope = "zena_css_architecture" };
        { name = "rse_router"; view = "rse"; cci_path = "platform.rse_router"; scope = "zena_css_architecture" };
        { name = "si_cl0_router"; view = "si_cl0"; cci_path = "platform.si_cl0_router"; scope = "zena_css_architecture" };
        { name = "si_cl1_router"; view = "si_cl1"; cci_path = "platform.si_cl1_router"; scope = "fvp_cfg2_extension" };
    };
    bridges = {
        {
            name = "ap_to_smd_atu_apu"; from = "ap"; to = "smd";
            kind = "atu_apu"; owner = "rse"; width = 52;
            reset_policy = "rse_only"; scope = "zena_css_architecture";
        };
        {
            name = "system_to_smd_nci"; from = "system"; to = "smd";
            kind = "nci_decode"; owner = "rse"; width = 52;
            reset_policy = "static_high_nibble"; scope = "zena_css_architecture";
        };
        {
            name = "rse_to_system_atu_apu"; from = "rse"; to = "system";
            kind = "atu_apu"; owner = "rse"; width = 32;
            reset_policy = "rse_only"; scope = "zena_css_architecture";
        };
        {
            name = "si_cl0_to_system_atu_apu"; from = "si_cl0"; to = "system";
            kind = "atu_apu"; owner = "rse"; width = 40;
            reset_policy = "rse_only"; scope = "zena_css_architecture";
        };
        {
            name = "si_cl0_ni710ae_primary_nci"; from = "si_cl0"; to = "si_cl0";
            kind = "apu"; owner = "si_cl0"; width = 40;
            reset_policy = "owner_only"; scope = "zena_css_architecture";
        };
        {
            name = "si_cl0_to_rse_shared"; from = "si_cl0"; to = "system";
            kind = "static_window"; owner = "rse"; width = 40;
            reset_policy = "static_allow_list"; scope = "zena_css_architecture";
        };
        {
            name = "si_cl0_to_si_cl1_scmi_apu"; from = "si_cl0"; to = "si_cl1";
            kind = "apu"; owner = "si_cl0"; width = 40;
            reset_policy = "static_allow_list"; scope = "fvp_cfg2_extension";
        };
        {
            name = "si_cl0_to_ap_hipc"; from = "si_cl0"; to = "ap";
            kind = "atu_apu"; owner = "rse"; width = 40;
            reset_policy = "deny_until_rse_programmed"; scope = "fvp_cfg2_extension";
        };
        {
            name = "si_cl1_to_ap_hipc"; from = "si_cl1"; to = "ap";
            kind = "static_window"; owner = "si_cl1"; width = 40;
            reset_policy = "static_allow_list"; scope = "fvp_cfg2_extension";
        };
    };
    qemu_instances = {
        {
            name = "ap_qemu_inst"; domain = "ap"; architecture = "AARCH64";
            cpu = "cortex-a720ae"; acceleration = "tcg"; tcg_mode = "MULTI";
            sync_policy = "multithread-freerunning"; ram_owner = "systemc";
        };
        {
            name = "rse_qemu_inst"; domain = "rse"; architecture = "ARM";
            cpu = "cortex-m55"; acceleration = "tcg"; tcg_mode = "MULTI";
            sync_policy = "multithread-freerunning"; ram_owner = "systemc";
        };
    };
    safety_island_pes = safety_island_pes;
    safety_island_gics = safety_island_gics;
    safety_island_contract = safety_island_contract;
    validation = {
        topology_frozen = true;
        required_socket_cardinality = 1;
        forbid_runtime_priority_mutation = true;
        forbid_broad_passthrough = true;
        migration_phase = "A4_policy_routing";
        compatibility_debt = {};
    };
}

for _, instance in ipairs(si_qemu_instances) do
    table.insert(topology.qemu_instances, #topology.qemu_instances, instance)
end

return topology
