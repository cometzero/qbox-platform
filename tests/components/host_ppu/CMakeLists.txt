add_executable(host_ppu-tests host_ppu-tests.cc)
target_link_libraries(host_ppu-tests PRIVATE gtest gmock host_ppu ${TARGET_LIBS})
add_test(
    NAME host_ppu-tests
    COMMAND host_ppu-tests
        --gtest_filter=-HostPpuTest.PowerOnTransitionSignalsLoadBeforeResetRelease:HostPpuTest.DynamicSuspendWakeAndClusterHandshake
)
add_test(
    NAME host_ppu-signal-tests
    COMMAND host_ppu-tests
        --gtest_filter=HostPpuTest.PowerOnTransitionSignalsLoadBeforeResetRelease
)
add_test(
    NAME host_ppu-idle-tests
    COMMAND host_ppu-tests
        --gtest_filter=HostPpuTest.DynamicSuspendWakeAndClusterHandshake
)
set_tests_properties(host_ppu-tests host_ppu-signal-tests host_ppu-idle-tests
    PROPERTIES TIMEOUT 20)
