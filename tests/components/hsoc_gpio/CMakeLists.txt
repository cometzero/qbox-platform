add_executable(hsoc_gpio-tests hsoc_gpio-tests.cc)
target_include_directories(
    hsoc_gpio-tests
    PRIVATE ${QBOX_CORE_SOURCE_DIR}/tests/qbox/include
)
target_link_libraries(
    hsoc_gpio-tests
    PRIVATE hsoc_gpio ${TARGET_LIBS}
)
add_test(NAME hsoc_gpio-tests COMMAND hsoc_gpio-tests)
set_tests_properties(hsoc_gpio-tests PROPERTIES TIMEOUT 20)
