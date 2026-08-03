add_executable(
    apollo_si_five_pe_topology-tests
    apollo_si_five_pe_topology-tests.cc
)
target_compile_definitions(
    apollo_si_five_pe_topology-tests
    PRIVATE APOLLO_HW_BLOCK_DIR="${CMAKE_SOURCE_DIR}/platforms/apollo/hw-block"
)
target_link_libraries(
    apollo_si_five_pe_topology-tests
    PRIVATE gtest lua
)
add_test(
    NAME apollo_si_five_pe_topology-tests
    COMMAND apollo_si_five_pe_topology-tests
)
set_tests_properties(
    apollo_si_five_pe_topology-tests
    PROPERTIES TIMEOUT 20
)
