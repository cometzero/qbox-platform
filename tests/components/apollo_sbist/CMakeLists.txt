add_executable(apollo_sbist-tests apollo_sbist-tests.cc)
target_link_libraries(apollo_sbist-tests PRIVATE gtest gmock apollo_sbist ${TARGET_LIBS})
add_test(NAME apollo_sbist-tests COMMAND apollo_sbist-tests)
set_tests_properties(apollo_sbist-tests PROPERTIES TIMEOUT 20)
