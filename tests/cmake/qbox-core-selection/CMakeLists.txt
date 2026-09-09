cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(qbox_core_selection_smoke LANGUAGES CXX)

if(NOT QBOX_CORE_SOURCE_DIR OR NOT QBOX_SMOKE_CASE)
    message(FATAL_ERROR "QBOX_CORE_SOURCE_DIR and QBOX_SMOKE_CASE are required")
endif()

set(fake_core "${CMAKE_CURRENT_BINARY_DIR}/fake-qbox")
foreach(test_dir IN ITEMS alpha alpha/nested beta)
    file(MAKE_DIRECTORY "${fake_core}/tests/${test_dir}")
endforeach()
file(WRITE "${fake_core}/tests/empty.cc" "int main() { return 0; }\n")
file(WRITE "${fake_core}/tests/alpha/CMakeLists.txt"
    "add_executable(alpha-test ../empty.cc)\nadd_subdirectory(nested)\n")
file(WRITE "${fake_core}/tests/alpha/nested/CMakeLists.txt"
    "add_executable(nested-test ../../empty.cc)\n")
file(WRITE "${fake_core}/tests/beta/CMakeLists.txt"
    "add_executable(beta-test ../empty.cc)\n")

if(QBOX_SMOKE_CASE STREQUAL "valid")
    set(QBOX_CORE_TEST_DIRS "alpha;beta")
elseif(QBOX_SMOKE_CASE STREQUAL "empty")
    set(QBOX_CORE_TEST_DIRS "")
elseif(QBOX_SMOKE_CASE STREQUAL "duplicate")
    set(QBOX_CORE_TEST_DIRS "alpha;alpha")
elseif(QBOX_SMOKE_CASE STREQUAL "traversal")
    set(QBOX_CORE_TEST_DIRS "../alpha")
elseif(QBOX_SMOKE_CASE STREQUAL "missing")
    set(QBOX_CORE_TEST_DIRS "missing")
elseif(QBOX_SMOKE_CASE STREQUAL "overlap")
    set(QBOX_CORE_TEST_DIRS "alpha;alpha/nested")
elseif(QBOX_SMOKE_CASE STREQUAL "absolute")
    set(QBOX_CORE_TEST_DIRS "${fake_core}/tests/alpha")
else()
    message(FATAL_ERROR "Unknown smoke case: ${QBOX_SMOKE_CASE}")
endif()

include("${QBOX_CORE_SOURCE_DIR}/tests/cmake/QBoxSelectedTests.cmake")
qbox_add_selected_test_directories(
    "${fake_core}/tests"
    "${CMAKE_CURRENT_BINARY_DIR}"
    qbox_core_test_targets
    ${QBOX_CORE_TEST_DIRS}
)
add_custom_target(qbox_core_unit_tests DEPENDS ${qbox_core_test_targets})
if(NOT TARGET qbox_core_unit_tests)
    message(FATAL_ERROR "qbox_core_unit_tests was not created")
endif()

qbox_collect_test_executables("${CMAKE_CURRENT_BINARY_DIR}" selected_targets)
list(SORT selected_targets)
if(QBOX_SMOKE_CASE STREQUAL "valid")
    if(NOT selected_targets STREQUAL "alpha-test;beta-test;nested-test")
        message(FATAL_ERROR "Unexpected selected targets: ${selected_targets}")
    endif()
elseif(QBOX_SMOKE_CASE STREQUAL "empty" AND selected_targets)
    message(FATAL_ERROR "Empty selection created targets: ${selected_targets}")
endif()
