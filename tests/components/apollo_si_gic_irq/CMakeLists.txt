add_executable(
    apollo_si_gic_irq-tests
    apollo_si_gic_irq-tests.cc
    ../apollo_si_canonical_gic/canonical_gic_integration.cc
)
target_compile_definitions(
    apollo_si_gic_irq-tests
    PRIVATE APOLLO_PLATFORM_DIR="${CMAKE_SOURCE_DIR}/platforms/apollo"
)
target_link_libraries(
    apollo_si_gic_irq-tests
    PRIVATE
        arm_gicv3
        cpu_arm_cortexR82
        gicx00_multiview
        gs_memory
        gtest
        lua
        router
)
add_test(
    NAME apollo_si_gic_irq-tests
    COMMAND apollo_si_gic_irq-tests
)
set_tests_properties(
    apollo_si_gic_irq-tests
    PROPERTIES TIMEOUT 120
)
