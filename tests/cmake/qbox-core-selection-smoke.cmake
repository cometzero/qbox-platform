cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

get_filename_component(platform_source "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if(NOT QBOX_CORE_SOURCE_DIR)
    get_filename_component(QBOX_CORE_SOURCE_DIR "${platform_source}/../qbox" ABSOLUTE)
endif()
set(fixture_source "${CMAKE_CURRENT_LIST_DIR}/qbox-core-selection")
set(smoke_root "${CMAKE_CURRENT_BINARY_DIR}/qbox-core-selection-smoke")

foreach(smoke_case IN ITEMS valid empty duplicate traversal missing overlap absolute)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -S "${fixture_source}"
            -B "${smoke_root}/${smoke_case}"
            "-DQBOX_CORE_SOURCE_DIR=${QBOX_CORE_SOURCE_DIR}"
            "-DQBOX_SMOKE_CASE=${smoke_case}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(smoke_case STREQUAL "valid" OR smoke_case STREQUAL "empty")
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "${smoke_case} selection failed:\n${output}${error}")
        endif()
    elseif(result EQUAL 0)
        message(FATAL_ERROR "${smoke_case} selection unexpectedly succeeded")
    endif()
endforeach()

message(STATUS "QBox core selection smoke test passed")
