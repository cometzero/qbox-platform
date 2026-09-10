add_executable(dw-apb-i2s-tests dw-apb-i2s-tests.cc)
target_link_libraries(dw-apb-i2s-tests PRIVATE
    gtest gmock dw-apb-i2s ${TARGET_LIBS})
add_test(NAME dw-apb-i2s-tests COMMAND dw-apb-i2s-tests)
set_tests_properties(dw-apb-i2s-tests PROPERTIES
    LABELS qbox-platform-dw-apb-i2s
    TIMEOUT 5)
