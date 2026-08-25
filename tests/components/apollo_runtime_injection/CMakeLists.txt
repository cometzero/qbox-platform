add_executable(
    apollo_runtime_injection-tests
    apollo_runtime_injection-tests.cc
)
target_include_directories(
    apollo_runtime_injection-tests
    PRIVATE ${QBOX_CORE_SOURCE_DIR}/tests/qbox/include
)
target_link_libraries(
    apollo_runtime_injection-tests
    PRIVATE apollo_runtime_injection ${TARGET_LIBS}
)
add_test(
    NAME apollo_runtime_injection-tests
    COMMAND apollo_runtime_injection-tests
)
set_tests_properties(apollo_runtime_injection-tests PROPERTIES TIMEOUT 30)
