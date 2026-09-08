add_executable(hsoc_pinctrl-tests hsoc_pinctrl-tests.cc)
target_include_directories(
    hsoc_pinctrl-tests
    PRIVATE ${QBOX_CORE_SOURCE_DIR}/tests/qbox/include
)
target_link_libraries(
    hsoc_pinctrl-tests
    PRIVATE hsoc_pinctrl ${TARGET_LIBS}
)
add_test(NAME hsoc_pinctrl-tests COMMAND hsoc_pinctrl-tests)
set_tests_properties(hsoc_pinctrl-tests PROPERTIES TIMEOUT 20)
