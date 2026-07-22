add_executable(rse_ppc_filter-tests rse_ppc_filter-tests.cc)
target_link_libraries(rse_ppc_filter-tests PRIVATE gtest gmock rse_ppc_filter ${TARGET_LIBS})
add_test(NAME rse_ppc_filter-tests COMMAND rse_ppc_filter-tests)
set_tests_properties(rse_ppc_filter-tests PROPERTIES TIMEOUT 20)
