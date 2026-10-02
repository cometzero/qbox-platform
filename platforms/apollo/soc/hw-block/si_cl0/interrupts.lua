local module = {}

function module.create(ctx, hardware, runtime, _interrupts)
    local SI_CONST = hardware
    local si_gic_trace = runtime.si_gic_trace
    local si_gic_trace_limit = runtime.si_gic_trace_limit

    local block = {}

    function block.define_multiview(ctx, platform)
        assert(
            platform.si_gic_multiview == nil,
            "duplicate object: platform.si_gic_multiview")
        platform.si_gic_multiview = {
            moduletype = "gicx00_multiview";
            trace = si_gic_trace;
            trace_limit = si_gic_trace_limit;
            backend_dist_base = SI_CONST.SI_GIC_BACKEND_DIST_BASE;
            backend_redist_base = SI_CONST.SI_GIC_BACKEND_REDIST_BASE;
            backend_redist_stride = SI_CONST.SI_GICR_SIZE;
            backend_redist_count = SI_CONST.SI_GIC_BACKEND_REDIST_COUNT;
            view_redist_stride = SI_CONST.SI_GICR_SIZE;
            view1_redist_first = SI_CONST.SI_GIC_VIEW1_REDIST_FIRST;
            view2_redist_first = SI_CONST.SI_GIC_VIEW2_REDIST_FIRST;
            spi_count = SI_CONST.SI_GIC_SPI_COUNT;
            view0_dist_cfgid = {
                address = SI_CONST.SI_CL0_GICD_VIEW0_BASE + SI_CONST.SI_GIC_DIST_CFGID_OFFSET;
                size = SI_CONST.SI_GIC_DIST_CFGID_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 0;
            };
            view0_dist_iviewr = {
                address = SI_CONST.SI_CL0_GICD_VIEW0_BASE + SI_CONST.SI_GIC_DIST_IVIEWR_OFFSET;
                size = SI_CONST.SI_GIC_DIST_IVIEWR_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 0;
            };
        }

        for i = 1,4 do
            platform.si_gic_multiview["view0_redist_" .. i] = {
                address = SI_CONST.SI_CL0_GICR_VIEW0_BASES[i + 1];
                size = SI_CONST.SI_GICR_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 20;
            }
        end

        platform.si_gic_multiview.view0_redist_0_pwrr = {
            address = SI_CONST.SI_CL0_GICR_VIEW0_BASES[1] + SI_CONST.SI_GIC_REDIST_PWRR_OFFSET;
            size = SI_CONST.SI_GIC_REDIST_REGISTER_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        }
        platform.si_gic_multiview.view0_redist_0_viewr = {
            address = SI_CONST.SI_CL0_GICR_VIEW0_BASES[1] + SI_CONST.SI_GIC_REDIST_VIEWR_OFFSET;
            size = SI_CONST.SI_GIC_REDIST_REGISTER_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        }
        platform.si_gic_multiview.view0_redist_0_flushr = {
            address = SI_CONST.SI_CL0_GICR_VIEW0_BASES[1] + SI_CONST.SI_GIC_REDIST_FLUSHR_OFFSET;
            size = SI_CONST.SI_GIC_REDIST_REGISTER_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        }
    end

    return block
end

return module
