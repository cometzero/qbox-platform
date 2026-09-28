macro(gs_add_test test)
    set(test_target "apollo-${test}")
    add_executable(${test_target} ${test}.cc)
    target_link_libraries(${test_target} PRIVATE gtest gmock uart-pl011 uart-ibex char_backend_socket char_backend_file char_backend_stdio ${TARGET_LIBS})
    if (WIN32)
        target_link_libraries(${test_target} PRIVATE ws2_32)
    endif()
    add_test(NAME ${test_target} COMMAND ${test_target})
    set_tests_properties(${test_target} PROPERTIES
        TIMEOUT 30 LABELS "qbox-platform-uart")
endmacro()
gs_add_test(file-backend-test)
gs_add_test(pl011-aperture-tests)
add_executable(apollo-pl011-reset-tests
    "${QBOX_CORE_SOURCE_DIR}/tests/components/uart/pl011-reset-tests.cc")
target_link_libraries(apollo-pl011-reset-tests PRIVATE
    gtest gmock uart-pl011 char_backend_file ${TARGET_LIBS})
add_test(NAME apollo-pl011-reset-tests COMMAND apollo-pl011-reset-tests)
set_tests_properties(apollo-pl011-reset-tests PROPERTIES
    TIMEOUT 30 LABELS "qbox-platform-uart")
gs_add_test(uart-biflow-stdio-test)
gs_add_test(uart-biflow-backend-socket-test)
gs_add_test(uart-ibex-biflow-stdio-test)
