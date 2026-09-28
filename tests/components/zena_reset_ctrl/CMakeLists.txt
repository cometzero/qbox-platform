add_executable(zena_reset_ctrl-tests zena_reset_ctrl-tests.cc)
target_link_libraries(zena_reset_ctrl-tests PRIVATE gtest gmock zena_reset_ctrl zena_watchdog reset_fanout ${TARGET_LIBS})
add_test(NAME zena_reset_ctrl-tests COMMAND zena_reset_ctrl-tests)
set_tests_properties(zena_reset_ctrl-tests PROPERTIES TIMEOUT 20)

add_executable(ap_watchdog_power_sequence-tests ap_watchdog_power_sequence-tests.cc)
target_link_libraries(ap_watchdog_power_sequence-tests PRIVATE gtest gmock
    host_ppu zena_reset_ctrl zena_watchdog reset_fanout signal_fanout ${TARGET_LIBS})
add_test(NAME ap_watchdog_power_sequence-tests COMMAND ap_watchdog_power_sequence-tests)
set_tests_properties(ap_watchdog_power_sequence-tests PROPERTIES TIMEOUT 20)
