add_executable(gicx00_multiview-tests gicx00_multiview-tests.cc)
target_link_libraries(gicx00_multiview-tests PRIVATE gtest gmock gicx00_multiview zena_device_fmu zena_fmu ${TARGET_LIBS})
# Pre-elaboration unit cases destroy their modules. Run the simulated source
# path in a fresh process so no pending kernel callbacks refer to those modules.
add_test(NAME gicx00_multiview-tests COMMAND gicx00_multiview-tests
    --gtest_filter=-Gicx00MultiviewTest.SgiDedSourceReachesLeafAndSystemFmuWithHardwareOrigin)
add_test(NAME gicx00_multiview-fmu-source COMMAND gicx00_multiview-tests
    --gtest_filter=Gicx00MultiviewTest.SgiDedSourceReachesLeafAndSystemFmuWithHardwareOrigin)
set_tests_properties(gicx00_multiview-fmu-source PROPERTIES TIMEOUT 20)
set_tests_properties(gicx00_multiview-tests PROPERTIES TIMEOUT 20)
