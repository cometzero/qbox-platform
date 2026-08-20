add_executable(apollo_cpu_ras-tests apollo_cpu_ras-tests.cc)
target_link_libraries(apollo_cpu_ras-tests PRIVATE gtest apollo_cpu_ras ${TARGET_LIBS})
add_test(NAME apollo_cpu_ras-tests COMMAND apollo_cpu_ras-tests)
set_tests_properties(apollo_cpu_ras-tests PROPERTIES TIMEOUT 20)
