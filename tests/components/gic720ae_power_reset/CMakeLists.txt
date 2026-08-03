add_executable(
    gic720ae_power_reset-tests
    gic720ae_power_reset-tests.cc
)
target_link_libraries(
    gic720ae_power_reset-tests
    PRIVATE
        arm_gicv3
        cpu_arm_cortexR82
        gic720ae_messreg
        gic720ae_power_bridge
        gicx00_multiview
        gs_memory
        gtest
        lua
        qemu_device_cold_reset
        reset_fanout
        router
)
target_compile_definitions(
    gic720ae_power_reset-tests
    PRIVATE APOLLO_PLATFORM_DIR="${CMAKE_SOURCE_DIR}/platforms/apollo"
)
add_test(
    NAME gic720ae_power_reset-tests
    COMMAND gic720ae_power_reset-tests
)
set_tests_properties(
    gic720ae_power_reset-tests
    PROPERTIES TIMEOUT 120
)
