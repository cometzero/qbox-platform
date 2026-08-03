add_executable(
    apollo_si_single_instance-tests
    apollo_si_single_instance-tests.cc
)
target_compile_definitions(
    apollo_si_single_instance-tests
    PRIVATE APOLLO_HW_BLOCK_DIR="${CMAKE_SOURCE_DIR}/platforms/apollo/hw-block"
)
target_link_libraries(
    apollo_si_single_instance-tests
    PRIVATE gtest lua
)
add_test(
    NAME apollo_si_single_instance-tests
    COMMAND apollo_si_single_instance-tests
)
set_tests_properties(
    apollo_si_single_instance-tests
    PROPERTIES TIMEOUT 20
)
