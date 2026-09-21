add_executable(apollo_si_stub-tests apollo_si_stub-tests.cc)
target_link_libraries(apollo_si_stub-tests PRIVATE gtest gmock apollo_si_stub ${TARGET_LIBS})
add_test(NAME apollo_si_stub-tests COMMAND apollo_si_stub-tests)
set_tests_properties(apollo_si_stub-tests PROPERTIES TIMEOUT 20)
