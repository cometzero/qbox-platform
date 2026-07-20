add_executable(zena_watchdog-tests zena_watchdog-tests.cc)
target_link_libraries(zena_watchdog-tests PRIVATE gtest gmock zena_watchdog ${TARGET_LIBS})
add_test(NAME zena_watchdog-tests COMMAND zena_watchdog-tests)
set_tests_properties(zena_watchdog-tests PROPERTIES TIMEOUT 20)
