-- Shared SI architectural INTID, QEMU socket index and View1/View2 contracts.
local interrupts = {}
local SI_CONST = {}
SI_CONST.GIC_SPI_BASE_INTID = 32
SI_CONST.GIC_SPI_LIMIT_INTID = 992
SI_CONST.SI_PE_COUNT = 5
SI_CONST.SI_IRQ = {
    sgi_directed = 5;
    sgi_broadcast = 6;
    hypervisor_timer = 19;
    physical_timer = 20;
    virtual_timer = 27;
    secure_timer = 29;
    cl0_system_timer = 34;
    cl0_watchdog_ws0 = 37;
    cl1_uart = 39;
    cl0_uart = 40;
    cl0_vmcu_uart = 41; -- QVP extension, reserved in RD-Aspen.
    cl0_vmcu_gpio = 42; -- QVP extension, reserved in RD-Aspen.
    cl1_hipc_pbx = 72;
    cl1_hipc_mbx = 73;
    cl1_pfdi = 82;
    cl0_ap_ns_mhu_pbx = 96;
    cl0_ap_ns_mhu_mbx = 97;
    cl0_ap_scmi_mhu_pbx = 98;
    cl0_ap_scmi_mhu_mbx = 99;
    cl0_ap_pfdi_mhu_pbx = 102;
    cl0_ap_pfdi_mhu_mbx = 103;
    cl0_rse_mhu = 105;
    cl0_cl1_mhu = 107;
    cl0_fmu_critical = 128;
    cl0_fmu_noncritical = 129;
    cl0_ap_ns_watchdog_ws1 = 321;
    cl0_ap_ras_cluster0 = 325;
    cl0_ap_ras_cluster1 = 327;
    cl0_ap_ras_cluster2 = 329;
    cl0_ap_ras_cluster3 = 331;
}
local VALID_TRIGGER = {edge = true; level = true}
local VALID_POLARITY = {
    high = true; low = true; positive = true; negative = true; none = true
}
local VALID_TARGET_SEMANTICS = {
    shared = true; per_cpu = true; directed = true; broadcast = true
}

local function irq_route_definition(
    name, source, kind, intid, owner_view, target_semantics, target_pes)
    local private_interrupt = kind == "SGI" or kind == "PPI"
    return {
        name = name;
        source = source;
        controller = owner_view == "View1" and
            "si_cl0_gic" or "si_cl1_gic";
        kind = kind;
        architectural_intid = intid;
        socket_class = kind == "SGI" and "sgi" or
            (kind == "PPI" and "ppi" or "normal_spi");
        socket_index = private_interrupt and intid or
            intid - SI_CONST.GIC_SPI_BASE_INTID;
        trigger = kind == "SGI" and "edge" or "level";
        polarity = kind == "SGI" and "none" or
            (kind == "PPI" and "low" or "high");
        owner_view = owner_view;
        target_semantics = target_semantics;
        target_pes = target_pes;
    }
end

local SI_ACTIVE_ROUTES = {
    irq_route_definition(
        "si_cl0_ap_ns_watchdog_ws1", "ap_watchdog_0.ws1", "SPI", SI_CONST.SI_IRQ.cl0_ap_ns_watchdog_ws1,
        "View1", "shared", {0});
    irq_route_definition(
        "si_sgi_directed", "si_cpu.icc_sgi1r_el1", "SGI", SI_CONST.SI_IRQ.sgi_directed,
        "View1", "directed", {4});
    irq_route_definition(
        "si_sgi_broadcast", "si_cpu.icc_sgi1r_el1_irm", "SGI", SI_CONST.SI_IRQ.sgi_broadcast,
        "View1", "broadcast", {1, 2, 3, 4});
    irq_route_definition(
        "si_physical_timer", "si_cpu.generic_timer_phys", "PPI", SI_CONST.SI_IRQ.physical_timer,
        "View1", "per_cpu", {0, 1, 2, 3, 4});
    irq_route_definition(
        "si_hypervisor_timer", "si_cpu.generic_timer_hyp", "PPI", SI_CONST.SI_IRQ.hypervisor_timer,
        "View1", "per_cpu", {0, 1, 2, 3, 4});
    irq_route_definition(
        "si_virtual_timer", "si_cpu.generic_timer_virt", "PPI", SI_CONST.SI_IRQ.virtual_timer,
        "View1", "per_cpu", {0, 1, 2, 3, 4});
    irq_route_definition(
        "si_secure_timer", "si_cpu.generic_timer_sec", "PPI", SI_CONST.SI_IRQ.secure_timer,
        "View1", "per_cpu", {0, 1, 2, 3, 4});
    irq_route_definition(
        "si_cl0_system_timer", "si_cl0_timer_cntbase.irq", "SPI", SI_CONST.SI_IRQ.cl0_system_timer,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_watchdog_ws0", "si_cl0_watchdog.ws0", "SPI", SI_CONST.SI_IRQ.cl0_watchdog_ws0,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl1_uart", "si_cl1_uart.irq", "SPI", SI_CONST.SI_IRQ.cl1_uart,
        "View2", "shared", {1, 2, 3, 4});
    irq_route_definition(
        "si_cl0_uart", "si_cl0_uart.irq", "SPI", SI_CONST.SI_IRQ.cl0_uart,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_vmcu_uart", "si_cl0_vmcu_uart.irq", "SPI", SI_CONST.SI_IRQ.cl0_vmcu_uart,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_vmcu_gpio", "si_cl0_vmcu_gpio.irq", "SPI", SI_CONST.SI_IRQ.cl0_vmcu_gpio,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl1_hipc_pbx", "si_cl1_hipc_mhu_pbx.irq", "SPI", SI_CONST.SI_IRQ.cl1_hipc_pbx,
        "View2", "shared", {1, 2, 3, 4});
    irq_route_definition(
        "si_cl1_hipc_mbx", "si_cl1_hipc_mhu_mbx.irq", "SPI", SI_CONST.SI_IRQ.cl1_hipc_mbx,
        "View2", "shared", {1, 2, 3, 4});
    irq_route_definition(
        "si_cl1_pfdi", "si_cl1_pfdi_mhu_pbx.irq", "SPI", SI_CONST.SI_IRQ.cl1_pfdi,
        "View2", "shared", {1, 2, 3, 4});
    irq_route_definition(
        "si_cl0_ap_ns_mhu_pbx", "si_cl0_ap_ns_mhu_pbx.irq", "SPI", SI_CONST.SI_IRQ.cl0_ap_ns_mhu_pbx,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_ns_mhu_mbx", "si_cl0_ap_ns_mhu_mbx.irq", "SPI", SI_CONST.SI_IRQ.cl0_ap_ns_mhu_mbx,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_scmi_mhu_pbx", "si_cl0_ap_scmi_mhu_pbx.irq", "SPI", SI_CONST.SI_IRQ.cl0_ap_scmi_mhu_pbx,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_scmi_mhu_mbx", "si_cl0_ap_scmi_mhu_mbx.irq", "SPI", SI_CONST.SI_IRQ.cl0_ap_scmi_mhu_mbx,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_pfdi_mhu_pbx", "si_cl0_ap_pfdi_monitor_mhu_pbx.irq",
        "SPI", SI_CONST.SI_IRQ.cl0_ap_pfdi_mhu_pbx, "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_pfdi_mhu_mbx", "si_cl0_ap_pfdi_monitor_mhu_mbx.irq",
        "SPI", SI_CONST.SI_IRQ.cl0_ap_pfdi_mhu_mbx, "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_rse_mhu", "si_cl0_rse_mhu_mbx.irq", "SPI", SI_CONST.SI_IRQ.cl0_rse_mhu,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_cl1_mhu", "si_cl0_cl1_mhu_mbx.irq", "SPI", SI_CONST.SI_IRQ.cl0_cl1_mhu,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_fmu_critical", "si_cl0_fmu.critical_irq", "SPI", SI_CONST.SI_IRQ.cl0_fmu_critical,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_fmu_noncritical", "si_cl0_fmu.non_critical_irq", "SPI", SI_CONST.SI_IRQ.cl0_fmu_noncritical,
        "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_ras_cluster0", "si_cl0_ap_ras_cluster0.cluster_irq", "SPI",
        SI_CONST.SI_IRQ.cl0_ap_ras_cluster0, "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_ras_cluster1", "si_cl0_ap_ras_cluster1.cluster_irq", "SPI",
        SI_CONST.SI_IRQ.cl0_ap_ras_cluster1, "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_ras_cluster2", "si_cl0_ap_ras_cluster2.cluster_irq", "SPI",
        SI_CONST.SI_IRQ.cl0_ap_ras_cluster2, "View1", "shared", {0});
    irq_route_definition(
        "si_cl0_ap_ras_cluster3", "si_cl0_ap_ras_cluster3.cluster_irq", "SPI",
        SI_CONST.SI_IRQ.cl0_ap_ras_cluster3, "View1", "shared", {0});
}

function interrupts.validate_irq_routes(routes)
    assert(type(routes) == "table", "SI IRQ routes must be a table")
    local names = {}
    local bindings = {}
    local owners = {}
    for _, route in ipairs(routes) do
        assert(
            type(route.name) == "string" and route.name ~= "",
            "SI IRQ route requires a name")
        assert(names[route.name] == nil, "duplicate SI IRQ route: " .. route.name)
        names[route.name] = true
        assert(
            type(route.source) == "string" and route.source ~= "",
            "invalid SI IRQ source: " .. route.name)
        assert(
            type(route.controller) == "string" and
                route.controller ~= "",
            "invalid SI IRQ controller: " .. route.name)
        assert(
            route.owner_view == "View1" or route.owner_view == "View2",
            "invalid SI IRQ owner view: " .. route.name)
        assert(
            VALID_TRIGGER[route.trigger] == true,
            "invalid SI IRQ trigger: " .. route.name)
        assert(
            VALID_POLARITY[route.polarity] == true,
            "invalid SI IRQ polarity: " .. route.name)
        assert(
            VALID_TARGET_SEMANTICS[route.target_semantics] == true,
            "invalid SI IRQ target semantics: " .. route.name)
        assert(
            type(route.target_pes) == "table" and #route.target_pes > 0,
            "invalid SI IRQ targets: " .. route.name)
        local targets = {}
        for _, pe in ipairs(route.target_pes) do
            assert(
                type(pe) == "number" and pe >= 0 and pe < SI_CONST.SI_PE_COUNT and
                    pe % 1 == 0 and targets[pe] == nil,
                "invalid SI IRQ target PE: " .. route.name)
            targets[pe] = true
        end
        if route.kind == "SPI" then
            assert(
                route.socket_class == "normal_spi" and
                    type(route.architectural_intid) == "number" and
                    route.architectural_intid >= SI_CONST.GIC_SPI_BASE_INTID and
                    route.architectural_intid < SI_CONST.GIC_SPI_LIMIT_INTID,
                "invalid normal SPI route: " .. route.name)
            assert(
                route.socket_index ==
                    route.architectural_intid - SI_CONST.GIC_SPI_BASE_INTID,
                "normal SPI socket must equal architectural INTID - 32: " ..
                    route.name)
        elseif route.kind == "PPI" then
            assert(
                route.socket_class == "ppi" and
                    route.architectural_intid >= 16 and
                    route.architectural_intid < SI_CONST.GIC_SPI_BASE_INTID and
                    route.socket_index == route.architectural_intid and
                    route.target_semantics == "per_cpu",
                "invalid PPI route: " .. route.name)
        elseif route.kind == "SGI" then
            assert(
                route.socket_class == "sgi" and
                    route.architectural_intid >= 0 and
                    route.architectural_intid < 16 and
                    route.socket_index == route.architectural_intid and
                    (route.target_semantics == "directed" or
                        route.target_semantics == "broadcast"),
                "invalid SGI route: " .. route.name)
        else
            error(
                "ESPI/EPPI and unknown classes cannot use normal SI routes: " ..
                    route.name)
        end
        local binding = route.source .. "|" .. route.controller .. "|" ..
            route.socket_index
        assert(bindings[binding] == nil, "duplicate SI IRQ binding: " .. route.name)
        bindings[binding] = true
        local owner = route.controller .. "|" .. route.owner_view .. "|" ..
            route.architectural_intid
        assert(owners[owner] == nil, "duplicate SI IRQ owner: " .. route.name)
        owners[owner] = true
    end
    return names
end

function interrupts.irq_route(routes, name)
    interrupts.validate_irq_routes(routes)
    for _, route in ipairs(routes) do
        if route.name == name then
            return route
        end
    end
    error("missing active SI IRQ route: " .. name)
end

function interrupts.active_irq_routes()
    return SI_ACTIVE_ROUTES
end

local function active_route(name)
    return interrupts.irq_route(SI_ACTIVE_ROUTES, name)
end

function interrupts.ppi_index(ctx, name)
    local route = active_route(name)
    assert(
        route.kind == "PPI" and route.socket_class == "ppi",
        "active SI route is not a PPI: " .. name)
    return route.socket_index
end

function interrupts.spi_target(ctx, name, expected_view)
    local route = active_route(name)
    assert(
        route.kind == "SPI" and route.socket_class == "normal_spi",
        "active SI route is not a normal SPI: " .. name)
    assert(
        route.owner_view == expected_view,
        "active SI route owner mismatch: " .. name)
    return "&" .. route.controller .. ".spi_in_" .. route.socket_index
end

return interrupts
