include("${QBOX_CORE_SOURCE_DIR}/tests/cmake/QBoxSelectedTests.cmake")
set(_qbox_core_tests_root "${QBOX_CORE_SOURCE_DIR}/tests")

# Keep test dependencies and helper headers local to the selected QBox tests.
set(TARGET_LIBS qbox gtest gmock)
set(QBOX_TEST_DIR "${QBOX_CORE_SOURCE_DIR}/tests/qbox")
include_directories("${QBOX_TEST_DIR}/include")

qbox_add_selected_test_directories(
    "${_qbox_core_tests_root}"
    "${CMAKE_CURRENT_BINARY_DIR}"
    qbox_core_test_targets
    ${QBOX_CORE_TEST_DIRS}
)
add_custom_target(qbox_core_unit_tests DEPENDS ${qbox_core_test_targets})
