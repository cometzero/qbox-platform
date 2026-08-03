add_executable(
    apollo_si_single_gic_config-tests
    apollo_si_single_gic_config-tests.cc
)
target_compile_definitions(
    apollo_si_single_gic_config-tests
    PRIVATE APOLLO_HW_BLOCK_DIR="${CMAKE_SOURCE_DIR}/platforms/apollo/hw-block"
)
target_link_libraries(
    apollo_si_single_gic_config-tests
    PRIVATE gtest lua
)
add_test(
    NAME apollo_si_single_gic_config-tests
    COMMAND apollo_si_single_gic_config-tests
)
set_tests_properties(
    apollo_si_single_gic_config-tests
    PROPERTIES TIMEOUT 20
)
