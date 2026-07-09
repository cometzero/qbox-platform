add_executable(ras_ffh_stub-tests ras_ffh_stub-tests.cc)
target_link_libraries(ras_ffh_stub-tests PRIVATE gtest gmock ras_ffh_stub ${TARGET_LIBS})
add_test(NAME ras_ffh_stub-tests COMMAND ras_ffh_stub-tests)
set_tests_properties(ras_ffh_stub-tests PROPERTIES TIMEOUT 20)
