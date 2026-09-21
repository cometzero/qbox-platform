add_executable(apollo_linux_stub-tests apollo_linux_stub-tests.cc)
target_link_libraries(apollo_linux_stub-tests PRIVATE gtest gmock apollo_linux_stub ${TARGET_LIBS})
add_test(NAME apollo_linux_stub-tests COMMAND apollo_linux_stub-tests)
set_tests_properties(apollo_linux_stub-tests PROPERTIES TIMEOUT 20)
