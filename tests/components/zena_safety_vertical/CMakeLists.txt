add_executable(zena_safety_vertical-tests zena_safety_vertical-tests.cc)
target_link_libraries(zena_safety_vertical-tests PRIVATE gtest gmock host_ni710ae_nci zena_fmu zena_ssu zena_reset_ctrl ${TARGET_LIBS})
add_test(NAME zena_safety_vertical-tests COMMAND zena_safety_vertical-tests)
set_tests_properties(zena_safety_vertical-tests PROPERTIES TIMEOUT 20)
