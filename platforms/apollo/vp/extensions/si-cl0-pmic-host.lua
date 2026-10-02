-- QVP-only polling PMIC transport; these addresses are not physical SI assignments.
local module = {}

function module.create(ctx, _hardware, _runtime, _interrupts)
    local PMIC_I2C_BASE = 0x2A800000
    local PMIC_GPIO_BASE = 0x2A810000
    local PMIC_MMIO_SIZE = 0x00010000

    local block = {}

    function block.define(ctx, platform)
        -- SCP initializes the board rails before power-feature initialization.
        -- The boot transport polls the controller. Independent PMIC fault inputs
        -- are exposed for inspection without an AP or SI GIC interrupt route.
        assert(
            platform.si_cl0_dw_i2c_0 == nil,
            "duplicate object: platform.si_cl0_dw_i2c_0")
        platform.si_cl0_dw_i2c_0 = {
            moduletype = "dw_apb_i2c";
            dylib_path = "dw-apb-i2c";
            transfer_latency = "10 us";
            target_socket = {
                address = PMIC_I2C_BASE;
                size = PMIC_MMIO_SIZE;
                bind = "&si_cl0_router.initiator_socket";
            };
        }
        assert(
            platform.si_cl0_pmic_gpio == nil,
            "duplicate object: platform.si_cl0_pmic_gpio")
        platform.si_cl0_pmic_gpio = {
            moduletype = "qemu_pl061";
            args = {"&platform.si_cl0_qemu_inst"};
            init_inputs = 0x01;
            pullups = 0x01;
            pulldowns = 0;
            mem = {
                address = PMIC_GPIO_BASE;
                size = PMIC_MMIO_SIZE;
                bind = "&si_cl0_router.initiator_socket";
            };
        }
        assert(
            platform.si_cl0_pmic_gpio_cold_reset == nil,
            "duplicate object: platform.si_cl0_pmic_gpio_cold_reset")
        platform.si_cl0_pmic_gpio_cold_reset = {
            moduletype = "qemu_device_cold_reset";
            args = {"&platform.si_cl0_pmic_gpio"};
        }
    end

    return block
end

return module
