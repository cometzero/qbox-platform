add_executable(host_gtimer-tests host_gtimer-tests.cc)
target_link_libraries(host_gtimer-tests PRIVATE
    arm_system_counter
    gtest
    gmock
    host_gtimer
    ${TARGET_LIBS}
)
add_test(NAME host_gtimer-tests COMMAND host_gtimer-tests)
set_tests_properties(host_gtimer-tests PROPERTIES TIMEOUT 20)

add_executable(host_gtimer-irq-tests host_gtimer-irq-tests.cc)
target_link_libraries(host_gtimer-irq-tests PRIVATE
    arm_system_counter
    gtest
    gmock
    host_gtimer
    ${TARGET_LIBS}
)
add_test(NAME host_gtimer-irq-tests COMMAND host_gtimer-irq-tests)
set_tests_properties(host_gtimer-irq-tests PROPERTIES TIMEOUT 20)
