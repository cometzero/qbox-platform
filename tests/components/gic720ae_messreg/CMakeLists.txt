add_executable(gic720ae_messreg-tests gic720ae_messreg-tests.cc)
target_link_libraries(gic720ae_messreg-tests PRIVATE gtest gmock gic720ae_messreg ${TARGET_LIBS})
add_test(NAME gic720ae_messreg-tests COMMAND gic720ae_messreg-tests)
set_tests_properties(gic720ae_messreg-tests PROPERTIES TIMEOUT 20)
