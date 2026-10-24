add_executable(zena_ni710ae_fmu-tests zena_ni710ae_fmu-tests.cc)
target_link_libraries(zena_ni710ae_fmu-tests PRIVATE gtest zena_ni710ae_fmu ${TARGET_LIBS})
add_test(NAME zena_ni710ae_fmu-tests COMMAND zena_ni710ae_fmu-tests)
set_tests_properties(zena_ni710ae_fmu-tests PROPERTIES TIMEOUT 20)
