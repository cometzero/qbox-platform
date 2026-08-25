add_executable(signal_or-tests signal_or-tests.cc)
target_link_libraries(signal_or-tests PRIVATE gtest gmock signal_or ${TARGET_LIBS})
add_test(NAME signal_or-tests COMMAND signal_or-tests)
set_tests_properties(signal_or-tests PROPERTIES TIMEOUT 20)
