add_executable(zena_reset_ctrl-tests zena_reset_ctrl-tests.cc)
target_link_libraries(zena_reset_ctrl-tests PRIVATE gtest gmock zena_reset_ctrl ${TARGET_LIBS})
add_test(NAME zena_reset_ctrl-tests COMMAND zena_reset_ctrl-tests)
set_tests_properties(zena_reset_ctrl-tests PROPERTIES TIMEOUT 20)
