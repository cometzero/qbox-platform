add_executable(zena_leaf_fmu-tests zena_leaf_fmu-tests.cc)
target_link_libraries(zena_leaf_fmu-tests PRIVATE gtest gmock zena_device_fmu zena_fmu zena_ni710ae_fmu ${TARGET_LIBS})
add_test(NAME zena_leaf_fmu-tests COMMAND zena_leaf_fmu-tests)
set_tests_properties(zena_leaf_fmu-tests PROPERTIES TIMEOUT 20)
